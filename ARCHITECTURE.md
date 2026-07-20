# Architecture

This documents the actual state of the repository as of the last work session
(git log tops out at 2024-07-23; there are also uncommitted local changes —
see "Uncommitted work in progress" below). It's a snapshot for picking the
project back up, not a description of a finished product — **this is an
early, unfinished prototype**, not a working slideshow yet.

## What this project is

An Oscar64/C rewrite, for the Commodore 128 VDC (8563/8568, 80-column chip),
of **VDC Mode Mania** — a 2012 BASIC demo/tool by Tokra and Mike (group
"Akronyme Analogiker") that shows off the VDC's unusual bitmap resolutions
(640x480, 640x576, 800x600, 960x540, etc. — see `original/v12/readme.txt`).
The original ships in full under `original/v12/` (detokenised BASIC source,
disk images, PC-side image converters) for reference. Xander Mol was thanked
in the original's credits and is now rewriting it from scratch in C using his
own `vdc_core`/`vdc_win`/`banking` Oscar64 library (the same library suite
used in `VDCScreenEditor2`).

## What actually runs today

`src/main.c`'s `main()` is a linear, hardcoded sequence — there is no menu,
no slideshow driver, no image loop yet:

```
bnk_init()
vdc_init(VDC_TEXT_80x25_PAL, 1)
raster_place_test()   // interactive raster-line calibration tool
title_screen()        // one prerendered hires title image + raster bars
plasma_demo(...)      // one hires plasma effect
rotate_demo(...)      // one hires image with color-cycle "rotation"
vdc_exit()
```

This reads like a developer test harness that runs each effect once in a
fixed order, not the intended final program (the original's actual purpose —
paging through dozens of converted images across many VDC modes — isn't
implemented; see "Assets vs. code" below).

### `raster_place_test()` — the calibration tool, not a real effect

This is the first thing that runs. It draws a 16-line color raster bar and
lets you nudge it up/down with cursor keys while it prints the live
rasterline number, until ESC/STOP. It exists **to hand-tune the VDC
raster-sync constants against real hardware/VICE by eye** — see the next
section, this is exactly what was being tuned when the repo was last
touched. It is not part of any real effect; nothing calls it except `main()`.

### `title_screen()`

Loads two prerendered hires screen halves (`vdce-scrtit.top`/`.bot`, made
with the author's own VDC Screen Editor) into Bank 1 RAM via `bnk_load()`,
blits them into VDC bitmap memory with `bnk_cpytovdc()`, then loops a
16-color rainbow raster-bar effect underneath (built directly on
`raster_synch()`/`raster_waitline()` from `vdc_raster.c`) until a keypress.

### `plasma_demo()` / `init_plasma()`

Classic two-layer sine-table plasma rendered into VDC hires bitmap memory,
double-buffered via the attribute address swap (`vdc_state.swap_attr` /
`base_attr`) so one buffer is written while the other displays.

### `rotate_demo()` / `init_rotate()` / `rotup()`/`rotdown()`/`rotleft()`/`rotright()`

Cycles a fixed VDC hires image's attribute/color table row-by-row and
column-by-column to fake movement without touching pixel data, using a
software `Screen[4000]` shadow buffer that gets pushed to VDC attribute RAM
each frame.

### `raster_bar()` — dead code

Defined at `src/main.c:431` but **never called** — `title_screen()` has its
own inline copy of the same rainbow-raster-bar logic instead of calling this
function. Likely an unfinished refactor (extract the shared logic, then
switch both callers over); don't be surprised it does nothing right now.

## Uncommitted work in progress

`git status` shows local, uncommitted edits to `include/vdc_core.c`,
`include/vdc_core.h`, `include/vdc_raster.c`, and `src/main.c` (on top of the
last commit, `e56776f`, 2024-07-23). This is almost certainly exactly where
work was interrupted:

- **`include/vdc_raster.c`**: the CIA2 timer constants in `raster_synch()`
  were being hand-tuned — `ldx #63` → `ldx #62` (CIA2 Timer A reload value)
  and the post-sync delay-loop counter `ldx #7` → `ldx #9`. These are the
  exact constants `raster_place_test()` exists to help you find by eye. If
  you pick this back up, run `raster_place_test()` first and check the bar
  sits where expected before touching anything else.
- **`include/vdc_core.c` / `.h`**: a new VDC mode was added,
  `VDC_HIRES_640x480_Mono_NTSC` (11th entry in `vdc_modes[]`, was 10), plus a
  one-register tweak to the existing `640x400 non-interlace` mode's
  `VDCR_VDISPLAY` value (`0x32` → `0x31`). `title_screen()` was switched over
  from `VDC_HIRES_640x400_Mono_PAL` to this new NTSC mode mid-edit.
- **`src/main.c`**: `title_screen()` was being reworked in lockstep with the
  mode change above — rainbow palette widened from 8 to 16 colors, the
  top/bottom screen-half split offset changed from 16000 to 19200 bytes (to
  match the new mode's taller resolution), and the raster line numbers for
  the last two bar segments shifted by one (84/83 → 85/84).

None of this is committed. Treat it as the actual "current state" rather
than what's in the last commit — it doesn't look finished (mid-tuning, not
mid-crash), so it's plausible the numbers just weren't verified on real
hardware yet before the session ended.

## Library layer: wired in vs. staged only

`src/main.c` currently `#include`s only: `defines.h`, `banking.h`,
`vdc_core.h`, `vdc_win.h`, `vdc_raster.h`. Everything else under `include/`
is present but **not referenced from anywhere in `src/`**:

| File | Status | Purpose (if/when wired in) |
|---|---|---|
| `vdc_menu.c/h` | staged, unused | Pulldown menu system — would be the natural driver for a real image-selection menu |
| `vdc_softscroll.c/h` | staged, unused | Sub-character pixel scrolling |
| `vdc_textscroller.c/h` | staged, unused | Text scroller |
| `krill.c/h` | staged, unused | KRILL fastloader integration — matches the commented-out `krill`/`flossiec` build variants in the `Makefile` |
| `peekpoke.h` | used transitively | Included by `vdc_core.c` and `banking.c` |

`vdc_win.c/h` *is* included but nothing in `main.c` currently calls any
`vdcwin_*` window/viewport function directly — only `vdcwin_checkch()` for
non-blocking keypress polling. The windowing layer isn't driving anything
yet.

`src/main.-vdctestold.c` is a superseded earlier version of `main.c`, kept
around for reference; it is not part of the build (`MAINSRC` in the
`Makefile` points only at `src/main.c`).

## Assets vs. code

`assets/vdce-scrtit.top`/`.bot` (the title screen) are the **only** asset
files actually loaded by any code path, via `title_screen()`.

`assets/vdcmodemania/` holds ~25 converted images carried over from
`original/v12/sd2iec-version/`, in two families by filename suffix, and
**none of them are referenced anywhere in `src/` or `include/`** (checked via
grep) — this is the bulk of the porting work still ahead, matching the
original's actual "slideshow across many VDC modes" concept:

- `NAME.bit` + `NAME.col` — single bitmap+color pair, for the modes that fit
  in one contiguous block (the color interlace modes, per the original's
  8x2/8x3-color-resolution descriptions).
- `NAME.bb`/`NAME.bt` (+ optional `NAME.cb`/`NAME.ct`) — bitmap split into
  top/bottom halves (`.b`/`.t`) with optional separate color halves
  (`.cb`/`.ct` — absent for the monochrome high-res modes, which need no
  per-cell color data), for the taller resolutions that don't fit in one
  VDC memory window.
- `settings.ihf` (`"129"`) / `settings.itf` (`"98"`) — small plain-PETSCII
  parameter files carried over unchanged from the original; not read by any
  current code, meaning unchecked.

## Build & tooling state

See `CLAUDE.md` for day-to-day build commands and `Makefile`
targets. As of this session the Makefile/`.gitignore`/`.vscode` were just
brought up to the same conventions used in `VDCScreenEditor2` /
`UltimateDemo2026` (`.env`-based deploy IPs, `check-deploy`, `MAIN_SRCS`
dependency tracking, a `docs`/pandoc pipeline) — those files are staged as
untracked/modified changes alongside this doc and haven't been committed
either. The Makefile's `krill`/`flossiec`/`d64`/`d71` alternate build targets
remain commented out and untouched; only the `standard` D81 build path is
active, matching that none of the krill/menu/scroller library code is wired
into `main.c` yet.

## Suggested pick-up order

1. Decide on and re-verify the `raster_synch()` CIA timer constants and the
   new `VDC_HIRES_640x480_Mono_NTSC` mode/`title_screen()` changes above —
   they're mid-edit, not known-good.
2. Extract `title_screen()`'s inline rainbow-bar loop into a call to the
   existing (currently dead) `raster_bar()`, or delete `raster_bar()` if the
   inline version is preferred.
3. Design the actual slideshow driver (likely built on `vdc_menu.h`, which is
   staged but unused) that iterates `assets/vdcmodemania/*` and picks the
   right VDC mode per image family.
