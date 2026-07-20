# VDC Raster Bar Library Manual

Reference for `vdc_raster.h`/`vdc_raster.c` — Xander Mol's C128/VDC raster
timing and bar-effect library for Oscar64. Covers calibration, the two
independent bar-drawing mechanisms (CIA2 busy-wait and CIA1 interrupt-driven),
and how to combine multiple bars in one sweep.

The VDC (8563/8568) has no raster IRQ and no register exposing the current
rasterline, unlike the VIC-II — everything here works around that using a
CIA timer as a stand-in, calibrated at runtime since the true cycles-per-line
ratio is a non-integer that varies slightly by machine (see `raster_calibrate()`
below).

---

## Calibration

```c
void raster_calibrate();

extern char raster_timer_reload;
extern unsigned raster_cycles_per_line_x1000;
```

Call once at startup, right after `vdc_init()`, before using anything else
in this library. Measures actual CPU cycles per VDC rasterline on the
running machine (varies by 8563 vs 8568 and PAL vs NTSC) by timing 64 VDC
VBlank-to-VBlank periods against a free-running CIA1 timer, then dividing
by the rasterlines/frame of whatever VDC mode is currently active.

- `raster_timer_reload` — the measured value rounded to the nearest integer
  cycle count. This is what `raster_synch()` (below) uses as its CIA2 Timer
  A reload.
- `raster_cycles_per_line_x1000` — the same measurement as a fixed-point
  value with 3 decimal digits (e.g. `62264` = 62.264 cycles/line), used by
  the IRQ-driven path (below) to compute a cycle-exact reload rather than
  just the rounded integer.

Both have sane hardcoded defaults (`62` / `63056`) in case `raster_calibrate()`
is never called, but real hardware/emulator sessions should always call it —
see `raster_place_test()` in `src/main.c` for a tool that lets you eyeball
whether the calibrated value holds a bar steady at a given line.

**Interlaced modes** (LACE register bits 0-1 = `11`, e.g.
`VDC_HIRES_720x700_Mono_PAL`): the VBlank status bit this function times
against toggles once per *field*, not once per full frame, so
`raster_calibrate()` halves its line-count formula for those modes to match
— confirmed live: without this, a genuinely interlaced mode calibrated to
exactly half the correct cycles/line value (e.g. measured 31.4 instead of
the expected ~63), which silently broke `raster_music_irq_start()`'s timing
for that mode (the picture displayed fine, since VDC addressing doesn't
depend on this value, but the colour gradient wouldn't advance visibly).
Non-interlaced modes are unaffected.

---

## Mechanism 1: CIA2 busy-wait bars

The CPU blocks for the whole effect. Simple, and fine for short decorative
bars, but nothing else runs while it's active — use Mechanism 2 (below) for
anything that needs to color a whole picture or share the CPU with other
work (like music).

### Low-level primitives

```c
void raster_synch();
void raster_waitline(char rasterline);
```

`raster_synch()` arms CIA2 Timer A (reload = `raster_timer_reload`) cascaded
into Timer B, synced to the next VBlank, and leaves interrupts disabled
(`SEI`) for the caller to manage. `raster_waitline(line)` busy-waits until
Timer B's low byte reaches `line`, with a small NOP-ladder for sub-line
precision. These are the foundation everything else in this section is
built on; call them directly only if you need something the higher-level
API below doesn't cover.

### High-level bar-drawing primitives

```c
void raster_bar_begin();
void raster_bar_line(char line, char color);
char raster_bar_segment(char line, const char *colors, unsigned char count);
void raster_bar_end();
```

Bracket one synced sweep per frame with `raster_bar_begin()`/
`raster_bar_end()`. In between, draw any mix of:
- `raster_bar_line(line, color)` — a single fixed-colour line (a "cap"
  between animated sections, or a lone static bar).
- `line = raster_bar_segment(line, colors, count)` — `count` lines read from
  `colors[]`, one per rasterline, starting at `line` and decrementing it
  each call; returns the line one past the last one drawn, ready to feed
  into a following call in the same sweep.

**Why `raster_bar_segment()` takes/returns `line` by value, not by
pointer:** an earlier version used `char *line` so multiple calls could
share a running position more conveniently. That's wrong for this code —
it's cycle-critical (`raster_waitline()`'s sub-line NOP-ladder assumes a
fixed cycle count between "target reached" and "colour written"), and a
pointer parameter adds a memory indirection on every loop iteration. Over a
60-line segment that was enough extra overhead per line to visibly
destabilize `title_screen()`'s picture (confirmed live, in VICE) —
mechanically working but no longer cycle-accurate. Don't reintroduce a
pointer here, or in any new bar-drawing primitive that sits inside a
`raster_bar_begin()`/`raster_bar_end()` sweep.

**Combining static and animated bars, and multiple bars per sweep:** both
of the above can be called any number of times, in any mix, within one
`raster_bar_begin()`/`raster_bar_end()` bracket — `title_screen()` in
`src/main.c` does exactly this (two static cap lines, a 60-line animated
segment, three more static cap lines, all in one sweep). Bars don't need to
be contiguous either: since each call waits for an explicit target line,
any gap between two bars is simply busy-waited through. The only
requirement is that lines are listed in descending order within a sweep,
matching a single top-to-bottom pass.

```c
void raster_bar_end();
```
Waits out the rest of the frame and re-enables interrupts (`raster_synch()`,
via `raster_bar_begin()`, leaves them disabled for the sweep's duration).

### Declarative multi-bar drawing

```c
enum RasterBarKind { RASTERBAR_LINE, RASTERBAR_SEGMENT };

struct RasterBarDef
{
    char startline;
    unsigned char length;  // RASTERBAR_SEGMENT only
    char kind;             // enum RasterBarKind
    char color;            // RASTERBAR_LINE only
    const char *colors;    // RASTERBAR_SEGMENT only, `length` entries
};

void raster_bar_draw_list(const struct RasterBarDef *bars, unsigned char count);
```

A convenience wrapper for when a bar layout is more natural to express as
data than as a sequence of calls — e.g. a table of bars built from a level
or scene definition. Draws every entry in one synced sweep (calls
`raster_bar_begin()`/`raster_bar_end()` itself). Use `raster_bar_line()`/
`raster_bar_segment()` directly instead when the layout has to be computed
at runtime (per-frame animation, conditional bars, etc.) — `title_screen()`
and `raster_bar()` both do that, since their bar positions/palettes change
every frame.

### Position-animation helper

```c
char raster_bar_bounce(char pos, char low, char high, signed char *direction);
```

Advances `pos` by `*direction`, flipping `*direction` when `pos` reaches
`low` or `high`. A small reusable helper for a bar that bounces back and
forth between two rasterlines, one step per frame — see `raster_bar()` in
`src/main.c`.

### Worked examples (`src/main.c`)

- `raster_place_test()` — a single animated segment (a 16-shade gradient)
  at an interactively movable position (cursor keys). The simplest use of
  `raster_bar_segment()`.
- `raster_bar()` — a bouncing bar between two fixed lines, driven by
  `raster_bar_bounce()`.
- `title_screen()` — a fixed-position, six-part bar: static cap lines
  around a 60-line segment whose palette start rotates one step each
  frame (the `rastercolors[start..]` pointer offset), all as one sweep.

---

## Mechanism 2: CIA1 interrupt-driven colouring + music

```c
void raster_music_irq_start(const char *colortable, unsigned tablelength,
                             char linespertick, char musicenabled);
void raster_music_irq_stop();
```

A real hardware interrupt (installed directly on `$FFFE`/`$FFFF`, not
chained through the KERNAL's `$314` vector — that was tried and found
unreliable in this project's boot context, see the comments in
`vdc_raster.c`) that walks a per-line colour table automatically, frame
after frame, freeing the CPU to do anything else between colour writes —
including, once per full table pass, playing one SID music frame. Use this
instead of Mechanism 1 for anything spanning most/all of a picture, or
anything that needs to run alongside other CPU work.

- `colortable`/`tablelength`: one `VDCR_COLOR`-format byte (background in
  bits 0-3, foreground in bits 4-7) per rasterline, replayed once per
  frame from the top.
- `linespertick`: how many table entries get written per interrupt call.
  Interrupt entry/exit cost is roughly fixed per call regardless of how
  much work it does, so batching several lines per tick matters a lot for
  how much spare CPU time is left over between calls — start around 4 and
  measure from there.
- `musicenabled`: non-zero only once a tune is already loaded and
  initialized the same way `banking.c`'s `sid_startmusic()` expects (init
  call to `$2000` in Bank 1) — with nothing loaded there, this would jump
  into garbage memory.

See `mono_colorize_demo()` in `src/main.c` for a complete worked example.

### Gotchas (all confirmed the hard way, live in VICE)

- **KERNAL/BASIC/character ROM is banked out while this is active** (I/O
  stays visible). Any KERNAL call — `vdcwin_checkch()`/`GETIN`, file
  loading, etc. — will not work until `raster_music_irq_stop()` restores
  normal banking. Use `keyb_poll()`/`key_pressed()` (`<c64/keyboard.h>`,
  direct keyboard matrix scan) for input while it's running instead.
- **Never touch a VDC register from foreground code while this is active** —
  no `vdc_prints()`, `vdc_write()`, or anything else touching `$D600`/
  `$D601`. The interrupt also writes them in the background; racing it can
  leave the VDC's addressing in a state where a foreground "wait for
  ready" spins forever. (This is exactly how an early version of this
  library's own diagnostic test hung — a busy foreground print loop
  fighting the interrupt over the same registers, not a flaw in the
  mechanism itself.)
- **Don't run this at the same time as a Mechanism 1 (busy-wait) bar over
  the same visual region** — not a hardware conflict (they use different
  CIA chips), but they'll fight over the same VDC colour register and
  produce visual nonsense.
- `raster_music_irq_stop()` doesn't restore CIA1 Timer A's original
  jiffy-clock reload (the 6526 has no way to read back a previous latch,
  only the live countdown) — it just stops the timer. Fine for a demo
  transitioning between effects; a following `vdc_exit()`/reset or the
  next `sid_startmusic()` call reprograms it anyway.

---

## Which mechanism do I use?

| Situation | Use |
|---|---|
| A short decorative bar, blocking the CPU is fine | Mechanism 1 (`raster_bar_*`) |
| Multiple/combined static + animated bars in a fixed layout | Mechanism 1, `raster_bar_draw_list()` |
| Same, but layout changes per frame | Mechanism 1, `raster_bar_line()`/`raster_bar_segment()` directly |
| Colouring most/all of a picture, line by line | Mechanism 2 (`raster_music_irq_start()`) |
| Needs the CPU free for other work (scroller, game logic, etc.) | Mechanism 2 |
| Needs music running independently of the main loop | Mechanism 2 (`musicenabled`) |
