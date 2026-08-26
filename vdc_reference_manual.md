# VDC Reference Manual

A from-first-principles reference for the Commodore 128's VDC (8563/8568,
80-column) chip, written up from what this project (`vdcmodemania-oscar64`)
actually learned building it — every gotcha here was hit and diagnosed
live, not copied from a datasheet. Three parts:

1. **Registers** — the full register set and what each one actually does.
2. **Modes** — every entry in this project's own `vdc_modes[]` table
   (`include/vdc_core.c`), what it's for, and why its particular register
   values are what they are.
3. **Advanced techniques** — things that aren't obvious from the register
   table alone: why register write ORDER (not just final value) matters
   — the single most expensive lesson this project has learned — plus
   interlace, the per-frame CSIZE trick, charset swapping, the bitmap
   attribute plane's own byte format, extended memory, hardware block
   copy, and the register-leak/stale-state bug classes that cost the
   most debugging time otherwise.

Companion documents, not duplicated here: `raster_lib_manual.md` (the two
raster-bar mechanisms in detail), `oscar64manual.md` (compiler-level
Oscar64 notes), `ARCHITECTURE.md` (this project's own code layout).

---

## Part 1: Registers

The VDC is accessed through exactly two memory locations, `$D600`
(index/status) and `$D601` (data) — every one of its 37 internal registers
is reached through this same two-step protocol:

```
select register:  STA $D600      ; register number 0-36 (bits 0-5)
wait for ready:    LDA $D600
                   BPL wait       ; bit 7 = 0 while busy, 1 when ready
read/write data:   LDA/STA $D601
```

`include/vdc_core.c`'s `vdc_reg_read()`/`vdc_reg_write()` wrap exactly this;
everything else in this project goes through them (or the higher-level
helpers built on top) rather than poking `$D600`/`$D601` directly.

### Full register table

| # | Hex | `VDCR_*` enum | Default | Function |
|---|-----|---|---|---|
| 0 | $00 | `HTOTAL` | $7E (126) | Total horizontal character positions − 1 |
| 1 | $01 | `HDISPLAY` | $50 (80) | Active horizontal character positions |
| 2 | $02 | `HSYNC` | $66 (102) | Horizontal sync position |
| 3 | $03 | `SYNCSIZE` | $49 | Horiz sync width (bits 3:0) + vert sync width (bits 7:4) |
| 4 | $04 | `VTOTAL` | $20/$27 | Total screen rows − 1 (NTSC=32, PAL=39) |
| 5 | $05 | `VADJUST` | $00 | Vertical fine adjustment (bits 4:0; extra scan lines beyond whole character rows) |
| 6 | $06 | `VDISPLAY` | $19 (25) | Active screen rows |
| 7 | $07 | `VSYNC` | $1D/$20 | Vertical sync position (NTSC=29, PAL=32) |
| 8 | $08 | `LACE` | $00 | Interlace mode (bits 1:0: `00`=non-interlaced, `01`=interlaced sync only, `11`=interlaced sync+video) |
| 9 | $09 | `CSIZE` | $07 | Scan lines per character − 1 (default 7 → 8 lines/row) |
| 10 | $0A | `CURSOR_START` | $60 | Cursor mode (bits 7:5) + cursor top scan line (bits 4:0) |
| 11 | $0B | `CURSOR_END` | $07 | Cursor bottom scan line (bits 4:0) |
| 12 | $0C | `DISP_ADDRH` | $00 | Screen (bitmap/text) memory start address, high byte |
| 13 | $0D | `DISP_ADDRL` | $00 | Screen memory start address, low byte |
| 14 | $0E | `CURSOR_ADDRH` | — | Cursor address, high byte |
| 15 | $0F | `CURSOR_ADDRL` | — | Cursor address, low byte |
| 16 | $10 | `LPEN_Y` | — | Light pen vertical position (read-only) |
| 17 | $11 | `LPEN_X` | — | Light pen horizontal position (read-only) |
| 18 | $12 | `ADDRH` | — | Current memory address, high byte (auto-increments on reg 31 access) |
| 19 | $13 | `ADDRL` | — | Current memory address, low byte |
| 20 | $14 | `ATTR_ADDRH` | $08 | Attribute memory start address, high byte |
| 21 | $15 | `ATTR_ADDRL` | $00 | Attribute memory start address, low byte |
| 22 | $16 | `CWIDTH` | $78 | Character width: horiz active pixels (bits 3:0) + total pixels−1 (bits 7:4) |
| 23 | $17 | `CHEIGHT` | $08 | Character vertical size: active scan lines (bits 4:0) |
| 24 | $18 | `VSCROLL` | $20 | Vertical smooth scroll (bits 4:0) + blink rate (bit 5) + reverse mode (bit 6) + copy/fill select (bit 7) |
| 25 | $19 | `HSCROLL` | $47/$4F | Horiz scroll (bits 3:0) + pixel double (bit 4) + semigraphic (bit 5) + attributes enable (bit 6) + bitmap/graphics mode enable (bit 7) |
| 26 | $1A | `COLOR` | $00 | Background colour (bits 3:0) + foreground colour when attributes off (bits 7:4) |
| 27 | $1B | `ROWINC` | $00 | Address increment per character row (added on top of `HDISPLAY` for the next row's start) |
| 28 | $1C | `CHAR_ADDRH` | $20 | Character set start address (bits 7:5) + memory type (bit 4: 0=4416 chips, 1=4164 chips) |
| 29 | $1D | `UNDERLINE` | $07 | Underline scan line position (bits 4:0) |
| 30 | $1E | `DSIZE` | — | Block copy/fill byte count — **writing this register initiates the operation** |
| 31 | $1F | `DATA` | — | Memory read/write gateway to VDC's own private RAM |
| 32 | $20 | `BLOCK_ADDRH` | — | Block copy source address, high byte |
| 33 | $21 | `BLOCK_ADDRL` | — | Block copy source address, low byte |
| 34 | $22 | `HSTART` | $7D | Horizontal blanking start position (screen on/off is implemented by moving this) |
| 35 | $23 | `HEND` | $64 | Horizontal blanking end position |
| 36 | $24 | `REFRESH` | $75 | Memory refresh cycles per scan line (bits 3:0; default 5) |

### Register groups, in practice

**Horizontal timing** — `HTOTAL`, `HDISPLAY`, `HSYNC`, `SYNCSIZE`,
`HSTART`, `HEND`. `HTOTAL` sets the overall line length; `HDISPLAY` how
many of those character positions are actually visible; `HSYNC`/`SYNCSIZE`
position the sync pulse. `HSTART`/`HEND` are the ones this project actually
manipulates at runtime: `vdc_disable_display()`/`vdc_enable_display()`
blank the screen by pushing `HSTART` off past the visible window (`0x80`)
and restoring it, rather than touching `HDISPLAY` — see "Blanking the
display" below.

**Vertical timing** — `VTOTAL`, `VADJUST`, `VDISPLAY`, `VSYNC`. `VTOTAL` is
in whole character rows; `VADJUST` adds a few extra raw scan lines on top
for fine-tuning frame height below one full character row's granularity.
`VSYNC` positions the vertical sync pulse — treated by this project, until
this session, as pure monitor-centering with no content implications. **That
assumption turned out to be wrong for interlaced modes** — see "Interlace"
below.

**Character geometry** — `CSIZE` (scan lines per character row),
`CWIDTH`/`CHEIGHT` (used for the hardware character generator, not bitmap
modes), `ROWINC`. `CSIZE`'s low 5 bits are the only meaningful ones on real
hardware — reading back a value with garbage in the upper 3 bits is normal
(see "Narrow-field readback noise" below), not a fault.

**Addressing** — `DISP_ADDRH/L` (where the bitmap or text screen starts in
VDC RAM), `ATTR_ADDRH/L` (where the attribute/colour plane starts),
`CHAR_ADDRH` (character set location, bits 7:5 only — **8KB granularity**,
see "Charset swapping" below), `ADDRH/L` (the general-purpose VDC memory
pointer used for `DATA`-register reads/writes), `BLOCK_ADDRH/L` (block-copy
source, separate from `ADDRH/L` because the destination address for a block
copy is set through the normal `ADDRH/L`/`DATA` path while the source has
its own dedicated pair — see "Hardware block copy" below).

**Mode/display control** — `LACE` (interlace), `HSCROLL` (packs the
bitmap/attribute enable bits alongside the actual horizontal-scroll pixel
offset — see the HSCROLL leak story in "Boot-baseline register leaks"),
`VSCROLL` (vertical scroll, plus the copy/fill select bit used by block
operations), `COLOR` (the single global ink/paper register non-attribute
text and bitmap modes rely on — see `raster_lib_manual.md` for how the
raster-bar library exploits this).

**Block copy/fill** — `DSIZE`, `DATA`, `BLOCK_ADDRH/L`, plus `VSCROLL` bit
7 as the copy/fill mode select. See "Hardware block copy/fill" below.

---

## Part 2: Modes — `vdc_modes[22]`

Every mode this project supports is one row of `struct VDCModeSet
vdc_modes[22]` in `include/vdc_core.c`, selected by the `enum VDCMode`
index and applied via `vdc_init(mode, extmem)` → `vdc_set_mode(mode)`.

### The struct

```c
struct VDCModeSet
{
    unsigned width;
    unsigned height;
    char bitmap;       // 1 = bitmap mode, 0 = text mode
    char colorlines;    // non-zero = attribute mode on (per-cell colour RAM)
    char extmem;        // 1 = requires the full 64KB VDC
    unsigned base_text;  // DISP_ADDR this mode normally uses
    unsigned base_attr;  // ATTR_ADDR this mode normally uses
    unsigned swap_text;   // secondary text buffer, for double-buffered effects
    unsigned swap_attr;   // secondary attribute buffer
    unsigned char_std;    // charset address (standard/uppercase)
    unsigned char_alt;    // charset address (alternate/lowercase) -- see caveat below
    unsigned extended;     // spare region, used ad hoc by individual effects
    char regset[33];        // {register, value, register, value, ..., 255}
};
```

`vdc_set_mode()` applies a row in this order: disable display → set
`DISP_ADDR`/`ATTR_ADDR` from `base_text`/`base_attr` → set charset address
+ restore charsets from ROM (skipped if `char_std==0`) → walk `regset[]`
writing each pair until the `255` terminator → OR in the bitmap/attribute
enable bits onto whatever `HSCROLL` value the regset left → re-enable
display. **Register pairs in `regset[]` are applied *after*
`base_text`/`base_attr`**, so a row that also lists `DISP_ADDRH`/`ADDRL` in
its `regset[]` has that value win over `base_text` — several modes below
rely on exactly this to give the register a different value than what
`base_text` is used for elsewhere (asset-copy destinations, mainly).

### Mode table

Interlace column is the actual `LACE` register value written by that row
(masked to its meaningful low 2 bits: `00`=off, `11`=on) — not a
description of whether the mode "needs" interlace. Several plain text/hires
modes turn it on for more visible scan lines within a given `VTOTAL`, not
because they're doing anything picture-format-special with it.

| Enum | Res | Bitmap | Attr | Interlace (`LACE`) | extmem | Used by |
|---|---|---|---|---|---|---|
| `VDC_TEXT_80x25_PAL` | 80×25 text | no | yes | off | no | Default/baseline mode |
| `VDC_TEXT_80x50_PAL` | 80×50 text | no | yes | **on** | no | — |
| `VDC_TEXT_80x70_PAL` | 80×70 text | no | yes | **on** | yes | — |
| `VDC_TEXT_80x25_NTSC` | 80×25 text | no | yes | off | no | — |
| `VDC_TEXT_80x50_NTSC` | 80×50 text | no | yes | **on** | no | — |
| `VDC_TEXT_80x60_NTSC` | 80×60 text | no | yes | **on** | yes | — |
| `VDC_HIRES_640x200_Color_PAL` | 640×200 | yes | yes | off | yes | `plasma_demo()`, `rotate_demo()` |
| `VDC_HIRES_640x200_Mono_PAL` | 640×200 | yes | no | off | yes | — |
| `VDC_HIRES_640x400_Color_PAL` | 640×400 | yes | yes | **on** | yes | — |
| `VDC_HIRES_640x400_Mono_PAL` | 640×400 | yes | no | **on** | yes | `title_screen()` |
| `VDC_HIRES_640x480_Mono_NTSC` | 640×480 | yes | no | **on** | yes | — |
| `VDC_HIRES_480x252_Color_PAL` | 480×252, 8×1 cells | yes | yes | off (explicit `0x00`) | yes | `fli_color_demo()` (VDC-FLI) |
| `VDC_HIRES_720x700_Mono_PAL` | 720×700 | yes | no | **on** | yes | `mono_hires_xl_demo()` (VDC-IMONO) |
| `VDC_HIRES_640x480_IHFLI_NTSC` | 640×480, 8×2 cells | yes | yes | **on** | yes | `fli_ihfli_demo()` (VDC-IHFLI) |
| `VDC_HIRES_640x576_ITFLI_PAL` | 640×576, 8×3 cells | yes | yes | **on** | yes | `fli_itfli_demo()` (VDC-ITFLI) |
| `VDC_HIRES_640x400_HFLI_PAL` | 640×400, 8×2 cells | yes | yes | *not set — inherited* | yes | `fli_hfli_demo()` (VDC-HFLI) |
| `VDC_HIRES_800x600_IM800_PAL` | 800×600 | yes | no | **on** | yes | `mono_im800_demo()` (VDC-IM800) |
| `VDC_HIRES_960x540_IM960_PAL` | 960×540 | yes | no | **on** | yes | `mono_im960_demo()` (VDC-IM960, needs RGBtoHDMI hardware — not run in the demo) |
| `VDC_TEXT_80x25_Mono_PAL` | 80×25 text | no | no | off | no | `idi8b_logo_demo()` |
| `VDC_HIRES_640x200_Mono_VSCROLL` | 640×200 window (stored 640×798) | yes | no | off | yes | `vscroll_demo()` (VDC-VSCROLL) |
| `VDC_HIRES_640x200_Mono_PANORAMA_R27` | 640×200 window (stored 1608×200) | yes | no | off | yes | `panorama_demo()` (VDC-PANORAMA) |
| `VDC_HIRES_640x200_Mono_PANORAMA2D` | 640×200 window (stored 904×426) | yes | no | off | yes | `panorama2d_demo()` (VDC-PANORAMA 2D) |

`VDC_HIRES_640x400_HFLI_PAL`'s missing `LACE` entry is a real gap by this
manual's own "boot-baseline register leaks" definition below — `LACE` isn't
one of the registers `vdc_reset_boot_registers()` covers, so this mode's
actual interlace state depends entirely on whichever mode ran immediately
before it. It's live-confirmed working regardless, meaning either every
mode that can precede it happens to leave `LACE` compatible, or the
picture tolerates either state — not confirmed which, and worth
revisiting if a future mode reordering ever breaks it.

The eight "Tokra VDC Mode Mania" hires modes (VDC-FLI through VDC-IM960)
are all reconstructed directly from Tokra's original BASIC source
(`vdcmodemania.bas`, part of the original release on CSDb —
https://csdb.dk/release/?id=234174) — the sections below note what's
genuinely non-obvious about each family, not a field-by-field walkthrough
(the source comments on each row in `vdc_core.c` have that level of detail
already).

### Text modes

The six `VDC_TEXT_*` rows are all straightforward attribute-mode text
(`colorlines` non-zero), differing only in row count (25/50/70) and PAL/NTSC
timing. 80×70 and 80×60(NTSC) need `extmem=1` — more than 16KB of VDC RAM
to hold a screen that size at 8 bytes/attribute-cell.

### Plain hires bitmap modes

`VDC_HIRES_640x200_*`, `640x400_*`, `640x480_Mono_NTSC` — ordinary bitmap
modes, colour (attribute on) or mono (attribute off) variants, no
interlace tricks, no per-frame register games. `640x200_Color_PAL` is the
one with the HSCROLL leak history (see "Boot-baseline register leaks").
`640x400_Mono_PAL` is `title_screen()`'s mode — see "Interlace, method 2"
below for why its picture format isn't what you'd naively expect.

### VDC-FLI (480×252, 8×1 colour cells)

The odd one out: **`CSIZE` is not set anywhere in this mode's `regset[]`**,
on purpose. Tokra's own BASIC toggles it every single frame — `$E0` (char
height 1) the instant the VDC's vblank status bit clears, `$E7` (char
height 8) the instant it sets again — because the real 8563/8568 hardware
needs the char-height register toggled every frame to hold this
resolution's internal addressing stable; a static value (either one) works
for a few lines then visibly drifts/tiles. `fli_color_demo()` in
`src/main.c` replicates this per-frame toggle. `DISP_ADDR`/`ATTR_ADDR` are
also set to values that don't match where the picture data is actually
copied (`0xf808`/`0x3fc4` vs `0x0000`/`0x4000`) — disassembly of Tokra's own
loader confirmed the picture bytes go to the plain addresses via the
general-purpose `ADDRH/L` pointer, and the `DISP_ADDR`/`ATTR_ADDR` pair
instead seeds an internal VDC addressing counter that the per-frame CSIZE
toggle depends on.

### VDC-IMONO / IHFLI / ITFLI / IM800 / IM960 (interlaced)

All five genuinely interlaced (`LACE=3`) modes. Two completely different
techniques are used across this project for getting a full-height picture
into an interlaced mode from two half-height source files — see "Interlace"
below, it matters which one a given mode's own demo function uses.
IHFLI/ITFLI additionally needed `VSYNC` set explicitly (see "VSYNC and
interlace field alignment") — found only via a live register-dump
comparison against Tokra's own working demo, not from the BASIC source
(neither program ever set it, so both were just running on leftover
values — one leftover happened to work, one didn't).

### VDC-HFLI (640×400, 8×2 cells, non-interlace)

The simplest of the colour "cell" modes — one static bitmap+colour plane,
no dual-field split, no per-frame register toggling. `DISP_ADDR`/`ATTR_ADDR`
aren't set by this row at all (Tokra's own table omits them too), so
`base_text`/`base_attr` (via `vdc_set_disp_address()`) are the *only* thing
that sets those registers for this mode — they have to exactly match where
the picture is actually loaded, unlike IHFLI/ITFLI where the two are
allowed to diverge.

### VDC-SCROLL family (VSCROLL / PANORAMA / PANORAMA2D) — panning beyond the 640×200 window

Three modes, three demo functions (`vscroll_demo()`, `panorama_demo()`,
`panorama2d_demo()`), one shared idea: the visible window stays a plain
640×200 bitmap, but the *stored* bitmap is taller (VSCROLL: 640×798),
wider (PANORAMA: 1608×200), or both (PANORAMA2D: 904×426), and a scripted
waypoint bounce/tour pans a 640×200 "window" through it. None of the
three modes' own `vdc_modes[]` rows differ from each other's timing —
same non-interlaced 640×200 family every other plain hires mono mode
uses (`colorlines=0`, no charset/attribute overhead). What differs is
entirely in how each demo function drives `DISP_ADDR`/`ROWINC`/`HSCROLL`
at runtime — see "Register write ORDER matters" immediately below, the
single most important lesson this family produced.

---

## Part 3: Advanced techniques

### Register write ORDER matters — not just the final values

**Attention point, worth leading with rather than burying as a
footnote.** On real 8563/8568 silicon, several registers' effects
depend on what OTHER registers already held at the moment they're
written — not just on their own final value. Writing one out of order
can leave the chip in a corrupted internal addressing state that
*persists even after every register is subsequently set to its
"correct" value* — this is not documented in the C128 Programmer's
Reference Guide, so it can't be caught by reading it more carefully; it
has to be designed around explicitly.

**R27 (`ROWINC`) and `DISP_ADDR`**: `vdc_set_mode()` applies a mode row
in a fixed order — `DISP_ADDR`/`ATTR_ADDR` are set from
`base_text`/`base_attr` FIRST, then `regset[]` is walked (see Part 2's
"The struct" above). If a mode's own `regset[]` row bakes in a `ROWINC`
write, that write lands while `DISP_ADDR` still holds `base_text`'s
value — not necessarily the real, final offset a caller wants to pan
to. Writing a nonzero `ROWINC` against a not-yet-final `DISP_ADDR`
corrupts internal addressing state that then persists regardless of
what both registers are set to afterward. **Rule**: never bake `ROWINC`
into a mode's own `regset[]` row — write it via an explicit
`vdc_reg_write(VDCR_ROWINC, value)` call, strictly AFTER
`vdc_set_disp_address()` has already set the real, final offset. Applied
this way, R27 behaves exactly as the C128 Programmer's Reference Guide
describes ("increments the address of the bit-mapped data from one
scan line to the next").

**`DISP_ADDR` and `HSCROLL`**: whenever both are written in the same
step (a byte-boundary crossing during horizontal panning, or any frame
where a combined row+column `DISP_ADDR` changes), the `DISP_ADDR` write
must be framed by `vdc_wait_no_vblank()` → `vdc_set_disp_address()` →
`vdc_wait_vblank()` before `HSCROLL` is written — the same pair
`vdc_softscroll_right()`/`vdc_softscroll_left()`
(`include/vdc_softscroll.c`) use around a byte crossing. Skipping this
framing on any step that touches both registers together produces
jarring, jumpy motion.

**The general rule**: when a register's own hardware effect depends on
another register's CURRENT value (`ROWINC` vs `DISP_ADDR`, `HSCROLL` vs
`DISP_ADDR`, and by extension any future register pairing not yet hit),
write the dependency FIRST, frame it with an explicit vblank wait to
confirm it has taken effect, THEN write the dependent register — every
time a step touches both, not only when a crossing/change seems likely.
When factoring register-write code into a shared helper, bake this
ordering into the helper itself, unconditionally, rather than leaving
it to each caller to apply conditionally — and re-test live on real
hardware after any such refactor, since this class of bug can look
identical to "the technique doesn't work" while actually being a pure
sequencing issue. See project memory
`vdcmaniac_r27_real_hardware_quirk_found.md` for the full technical
history.

### Interlace: two different ways to reassemble a split picture

Every interlaced mode in this project loads its picture as two half-height
files (`.top`/`.bot`, or similarly named), because a full interlaced
bitmap is too big for the 32KB CPU staging buffer to hold in one piece.
**How those two halves need to be arranged is not the same across every
mode** — this was the single most expensive lesson of this whole project,
first discovered fixing the new title screen picture.

**Method 1 — address-gap (VDC-IMONO, IHFLI, ITFLI, IM800, IM960).** Each
half is a perfectly ordinary top-to-bottom slice of the picture (rows
0..N/2−1 in one file, N/2..N−1 in the other) — but they're copied to VDC
memory at two addresses with a *specific, non-obvious gap* between them,
not simply back-to-back. VDC-IMONO's bottom half goes to `0x82c8`, not
`base_text+31500` (`0x7b0c`) — a ~1980-byte gap, reverse-engineered from
disassembling Tokra's own loader. The VDC's own interlace scan-out
hardware reassembles the two ordinary-order halves into a correct full
frame *because* of that specific gap; get the gap wrong and the picture
tears. Every one of IMONO/IHFLI/ITFLI/IM800/IM960's demo functions
(`src/main.c`) has its bottom-field address as a literal comment
explaining where it came from — always disassembly of Tokra's own loader,
never guessed.

**Method 2 — source-interleaved (title_screen(), `VDC_HIRES_640x400_Mono_PAL`).**
The two halves are copied to VDC memory *back-to-back with no gap at all*
(one `bnk_cpytovdc()` call over the whole concatenated buffer) — but the
source *data itself* has to be pre-interleaved by even/odd row, not split
by physical half. `.top` = rows 0, 2, 4, …; `.bot` = rows 1, 3, 5, ….
Decoding the old (confirmed-working) title screen picture with a naive
physical-half split showed the whole picture duplicated, squashed to half
height, in *both* halves — the unambiguous signature of interlace field
data being read as if it were sequential rows. `tools/vdc_convert.py`'s
`titlescreen` mode does this interleaving explicitly:

```python
for y in range(height):
    row = bitmap[y * bytes_per_row:(y + 1) * bytes_per_row]
    (top if y % 2 == 0 else bot).extend(row)
```

**Which method does a new interlaced mode need?** Check how its demo
function places the two halves: same contiguous placement as `base_text`
with no special second address → Method 2 (interleave the source); a
distinct, gapped second address (a literal in the demo function, not
`base_text+halfsize`) → Method 1 (keep the source in plain row order,
trust the gap). Don't assume — verify by decoding the actual bytes both
ways and looking for the "duplicated, squashed" tell if a new mode's
picture looks wrong.

### VSYNC and interlace field alignment

Register 7 (`VSYNC`) was assumed, for most of this project, to be pure
monitor-centering — moves the whole picture up/down on a real screen, no
content implications. **That's only true for non-interlaced modes.** For
IHFLI, a live VDC register-dump comparison (VICE monitor `io d600`, this
project's own build vs Tokra's original, both paused at the same picture)
found every geometry register matching *exactly* except `VSYNC` — and the
picture was visibly torn with the wrong value, clean with the right one.
Neither program ever explicitly sets `VSYNC` for this mode (confirmed
against Tokra's own BASIC `DATA` statements), so both readings were just
"whatever was left over" from each program's own prior state — the
original demo's leftover happened to be a working value, this project's
leftover wasn't.

The value that worked, `0x81`, turned out to equal `VTOTAL(0x84) −
VADJUST(3)` exactly for that mode. ITFLI's own `VSYNC` (`0x63`) was set
using the same relationship (`VTOTAL(0x68) − VADJUST(5) = 0x63`),
extrapolated rather than independently confirmed the same way.

That extrapolation turned out to be unreliable: a real-hardware report
from Tokra (RGBtoHDMI interlace fields swapped for the mono modes,
2026-08-24) led to a live `io d600` comparison against his own
`vdcmodemania.bas` DATA statements, which found `VTOTAL − VADJUST`
undershooting Tokra's actual value by exactly 1 for both VDC-IMONO
(`0x64` vs the real `0x65`) and VDC-IM800 (`0x56` vs the real `0x57`) —
IHFLI's own `0x81` matched exactly, and ITFLI's extrapolated `0x63` also
checked out against his own live register dump, so the formula isn't
wrong everywhere, just not reliable enough to trust without confirming
each mode individually. Both wrong values are now fixed to Tokra's own
literal constants. **Takeaway for any new interlaced mode**: don't leave
`VSYNC` unset/inherited, and don't trust `VTOTAL − VADJUST` on its own —
transcribe the source demo's own literal value if known, and confirm live
either way.

Even a correct transcribed value isn't universally correct across every
monitor, per Tokra's own note: "interlace is super fiddly, some displays
may actually need the +1 value" — his original demo lets the user nudge
VSYNC ±1 live with the cursor keys for exactly this reason. `vdcmaniac`
now offers the same adjustment (see `vsync_nudge` in `vdc_core.c`/
`main.c`'s interlaced-mode sections) rather than relying on one hardcoded
constant being right for every display.

### Boot-baseline register leaks

The single most common bug class in this whole project. Many VDC
registers are **not** part of every mode's own `regset[]` row — either
because no mode needs anything but the KERNAL's own boot-time default, or
because only *one* mode (the first to ever need something different) sets
it explicitly. Without a reset mechanism, whichever mode last touched such
a register leaks its value forward into every later mode that doesn't
override it — including modes that ran long before the "leaking" mode was
even added to the codebase, which is exactly what made this bug class so
disruptive to track down (a change to mode A silently breaking mode C,
with no direct connection between them in the source).

Three concrete, live-confirmed incidents:
- **HSCROLL**: `VDC_HIRES_640x200_Color_PAL`'s row initially omitted
  `HSCROLL` entirely, then a fix attempt set it to `0x00` on an unverified
  assumption ("no scroll offset must be correct") — actually wrong; a
  register-dump comparison against a known-working build showed the real
  value needed is `0x07` (matching Tokra's own convention seen on several
  other modes). The `0x00` "fix" was itself the bug.
- **HEND**: `mono_im800_demo()`'s mode explicitly sets `HEND=0x6a` for its
  own wide display. Nothing reset it afterward, so it leaked into
  `plasma_demo()`/`rotate_demo()` (which run immediately after it in
  `main()`), which needed the boot default `0x64` instead. This was the
  long-parked "isolated shifted fragment" bug — see memory:
  `rotate_demo_shift_bug` for the full diagnostic history.
- **REFRESH** (register 36): IHFLI/ITFLI both explicitly set it (`0x02`,
  matching Tokra's own DATA statements), but IMONO/IM800 don't — and
  neither does Tokra's own code for those two modes. A live `io d600`
  comparison against Tokra's build (2026-08-24) found it deviating
  between the two programs, purely because each program's own execution
  history left a different value sitting there. Added to the capture/
  restore set below.

**This project's fix**: `vdc_reset_boot_registers()` (`vdc_core.c`)
captures a set of registers once, on the very first `vdc_init()` call
(before anything has touched them), and restores that captured baseline at
the start of *every* `vdc_init()` and at `vdc_exit()` — so no mode
transition can inherit a leaked value regardless of what ran before it.
Currently covers `HDISPLAY`, `HSYNC`, `SYNCSIZE`, `HSCROLL`, `VSCROLL`,
`HEND`, `DISP_ADDRH/L`, `ATTR_ADDRH/L`, `REFRESH` (plus `HSTART`, handled
the same way but restored specifically by `vdc_enable_display()` since
it's also the screen-blanking register). **If a new mode ever explicitly
sets a register that isn't in this list**, and a later mode doesn't set its own value for
that register either, expect exactly this bug — add the register to
`vdc_reset_boot_registers()`'s capture/restore pair (bundled in `struct
VDCBootBaseline`) rather than hand-fixing it in the affected mode's own
row.

**How to actually find one of these, fast**: don't reason about it
statically — dump live VDC registers (`io d600` in VICE's monitor) at the
same point in both a known-working build and the broken one, and diff.
This found all three incidents above in one pass each, after static analysis
alone (register-table comparisons against BASIC source, exhaustive
`Screen[]`/VDC-memory content scans) had failed to find the
`rotate_demo_shift_bug` for an entire prior session.

### Stale `vdc_state` mid-`vdc_init()` — a sibling bug class

A close relative of the register-leak class above, but about the C-side
`vdc_state` struct instead of VDC registers: `vdc_init(mode, extmem)`
calls `vdc_detect_mem_size()` (which probes for 16 vs 64KB VDC RAM)
BEFORE `vdc_set_mode(mode)` — and `vdc_set_mode()` is what actually
updates `vdc_state.width`/`.height`/`.bitmap` to describe the mode being
switched TO. **Attention point**: anything that runs in that window and
reads those fields gets the OUTGOING mode's values, not the new mode's.

The display is disabled for this entire stretch of `vdc_init()`
(`vdc_disable_display()`, called before `vdc_detect_mem_size()` runs),
so nothing in this window is ever visible regardless of what it does —
but code here can still cost real time. `vdc_detect_mem_size()` itself
does no clearing of its own (a `vdc_cls()` there would be redundant
regardless: `vdc_set_mode()`, called immediately afterward, already
does its own correctly-sized `vdc_cls()` for any `!bitmap` (text)
destination, and every bitmap-mode destination's own caller overwrites
the whole screen itself — `bnk_cpytovdc()`/`vdc_wipe_transition()` —
before ever re-enabling the display).

**Rule**: anything added to the pre-`vdc_set_mode()` stretch of
`vdc_init()` must not depend on `vdc_state`'s mode-describing fields —
they're stale there by construction — and should avoid work sized to
mode geometry, since it's invisible (display disabled) but still costs
real time proportional to whatever stale size it reads. Prefer
operations that don't need current-mode geometry in that window; if one
genuinely does, move it to after `vdc_set_mode()` runs instead.

### Narrow-field readback noise

Several registers have fewer meaningful bits than a full byte —
`VADJUST`/`CSIZE` (5 bits), `LACE` (2 bits) among them. **Reading one back
after writing it can show garbage in the unused upper bits** — this is
normal hardware behaviour, not corruption. Confirmed live: writing
`CSIZE=0x03` and reading it back as `0xe3` (low 5 bits still correctly
`0x03`) during the IHFLI diagnostic session. Always mask to the documented
bit width before comparing a readback against an expected value.

### Charset swapping (lowercase text in non-attribute mode)

The KERNAL's own SHIFT+COMMODORE 80-column toggle swaps between two ROM
charset images: the standard set (`$D000`, uppercase + graphics) and the
alternate set (`$D800`, uppercase + lowercase). `vdc_set_mode()` always
selects `char_std` via `vdc_set_charset_address()` — it never
automatically shows the alternate set for you.

**In attribute-mode text**, each character cell's own colour-RAM byte can
individually request the alternate charset (`VDC_A_ALTCHAR`, bit 7 of the
attribute byte) — `vdc_altchar()` sets this per subsequent `vdc_prints()`
call.

**In non-attribute-mode text, there is no per-character attribute RAM at
all**, so that bit is never consulted — the screen is stuck on whichever
charset `char_std` currently points to, full stop. Two further subtleties
that make "just point somewhere else" not work:

1. `vdc_set_charset_address()` masks the target address with `0xE0` (top 3
   bits of `CHAR_ADDRH` only) — **8KB granularity**. `char_std` (`0x2000`)
   and `char_alt` (`0x3000`), only 4KB apart, land in the *same* selectable
   8KB region — the register value is identical either way, so "just
   select `char_alt` instead" is a no-op.
2. Even with perfect addressing, the VDC's own character lookup formula is
   `charset_base + screencode*16`, and screen codes only range 0–255 (4096
   bytes) — `char_alt` at `base+4096` is structurally unreachable through
   normal character rendering, by hardware design, regardless of any
   register setting.

**The actual fix**: overwrite `char_std`'s own *contents* with the
alternate ROM image, exactly like the KERNAL's own toggle does — don't try
to redirect which address is selected.

```c
bnk_redef_charset(vdc_state.char_std, BNK_CHARROM, (char *)0xd800, 256);
```

`bnk_redef_charset()` (`include/banking.c`) copies `size` characters × 16
bytes (8 real ROM bytes + 8 zero padding, since VDC character cells are 16
bytes but the C64/128 charset ROM is 8 bytes/char) from CPU-bank-selected
ROM into VDC memory. `vdc_restore_charsets()` calls this with `char_std` as
destination and `$D000` as source, `size=512` in one call — which actually
copies *both* the standard (`$D000`–`$D7FF`) and alternate (`$D800`–`$DFFF`)
images sequentially, landing at `char_std` and `char_std+0x1000`
respectively — explaining why `char_alt` is conventionally
`char_std+0x1000` in this project's mode table, even though (per point 2
above) that second copy is unreachable via normal rendering and exists
only as an artifact of how `vdc_restore_charsets()` happens to work.
`idi8b_logo_demo()` in `src/main.c` is the worked example of the real fix.

### The bitmap attribute plane's own byte format

Every colour-cell mode this project has (VDC-FLI, VDC-HFLI, VDC-IHFLI,
VDC-ITFLI, VDC Spectrum) packs its per-cell attribute byte as
`(background<<4)|foreground` — background in the HIGH nibble, foreground
in the LOW nibble. This is the real hardware convention for the VDC's
own bitmap ATTRIBUTE PLANE specifically. **Attention point**: it is
**not** the same convention register 26 (`VDCR_COLOR`) uses — that
single, global register packs the opposite way (foreground high,
background low) — and it is also the opposite of what the C128
Programmer's Reference Guide's own text-mode attribute-byte diagram
might suggest by analogy (that diagram describes bits 7:4 as
charset-select/reverse/underline/flash flags for TEXT mode, not a
colour nibble at all — bitmap mode has no character to underline or
flash, so the hardware repurposes those upper bits as the background
colour instead). When verifying a colour-cell convention like this one,
trust real hardware (or an independently-implemented emulator) over a
self-consistent encode/decode round-trip test — the latter looks clean
regardless of whether the convention it uses actually matches the
hardware.

**Attention point for asset regeneration**: VDC-FLI uniquely loads its
bitmap and colour data to two different destination offsets in the same
load pass (`MEM_SCREEN` and `MEM_SCREEN+15120`, not `MEM_SCREEN` for
both). When regenerating its assets, use the correct destination for
each file rather than reusing a blanket "bake `MEM_SCREEN`" helper
across every colour-cell mode.

### Extended (64KB) memory

`vdc_detect_mem_size()` probes for 64KB by writing to `$1FFF` and `$9FFF`
and checking whether they alias (16KB VDC) or stay independent (64KB).
`vdc_set_extended_memsize()` sets register 28 bit 4 to switch the VDC's
internal DRAM refresh timing to match 64Kx1 chips instead of the default
16Kx4 assumption — **this is a real hardware-configuration bit, matching
what chips are actually populated on the board, not just an "unlock more
address space" flag** (per `c128_reference.md`: "changing to 1 for 64Kx1
chips requires chip replacement" on hardware that's actually wired for the
smaller chips — on a genuine 64KB-VDC C128 this is safe, it's just not a
purely software toggle in spirit). It also does its own internal
`vdc_wipe_mem()` + `vdc_restore_charsets()` while the display is disabled,
to avoid visible artifacts from the timing change. Any mode whose row has
`extmem=1` triggers this automatically via `vdc_init()`'s second parameter.

### Hardware block copy/fill

`vdc_block_fill(address, value, length)` and
`vdc_block_copy_page()`/`vdc_block_copy()` use the VDC's own internal
block-transfer hardware — writing `DSIZE` (register 30) *initiates* a
transfer of that many bytes, using whatever's already been set up in
`ADDRH/L` (destination) and, for copies, `BLOCK_ADDRH/L` (source) plus
`VSCROLL` bit 7 (the copy/fill mode select bit — set for copy, cleared for
fill). Fixed overhead per call is roughly 6-7 VDC-ready-wait transactions
regardless of run length (`length` is a `char`, capped at 255 — longer
runs need chained calls, see `vdc_block_copy()`'s page-chunking loop) — the
practical break-even for using this over a plain byte-copy loop is around
7 identical/copyable bytes; shorter runs are faster as literal copies.
`vdc_wipe_mem()` (fills all 64KB to zero) and every `vdc_wipe_transition()`
screen wipe in this project go through `vdc_block_fill()` for exactly this
speed reason. **`vdc_wipe_mem()` has no concept of "current mode" — it
just blasts the entire 64KB address range**, which includes whatever
charset is currently loaded at `char_std`. Confirmed live: an isolated RLE
C-side decoder test called it directly (as a "clean slate" before writing
its own test data) and it wiped the active mode's character set along with
everything else — since nothing else in the demo happened to reload the
charset until the *next* section's own `vdc_init()`, every `vdc_prints()`
call in between (including the test's own pass/fail message) rendered as
blank space. Use `vdc_wipe_transition()` (which disables the display
first, and is meant to be visible as a section-transition flash) instead
of calling `vdc_wipe_mem()` bare, unless you're certain nothing currently
on screen — including the charset — needs to survive.

### Blanking the display

`vdc_disable_display()`/`vdc_enable_display()` blank the screen by moving
`HSTART` (register 34) off past the visible window (`0x80` = disabled)
and back to its real value, rather than touching `HDISPLAY` or any other
timing register — cheaper and less disruptive than reprogramming the whole
mode. `vdc_enable_display()` restores the *captured boot-time* `HSTART`
value (see "Boot-baseline register leaks"), not a hardcoded literal, so a
mode with its own genuinely different intended `HSTART` (VDC-IM960's
`0x06`, needed for its own horizontal positioning) has to explicitly
re-poke it *after* `vdc_init()` returns — `vdc_set_mode()`'s own final
`vdc_enable_display()` call would otherwise silently overwrite it with the
boot baseline.

### Raster bars

Covered in full in `raster_lib_manual.md` — summary only here. The VDC has
**no raster IRQ and no register exposing the current scanline**, unlike
the VIC-II, so both of this project's raster-bar mechanisms fake one:

- **Mechanism 1** (`raster_bar_*()`): CIA2 Timer A/B cascaded and
  calibrated at runtime (`raster_calibrate()`), busy-waited against with a
  NOP-ladder for sub-line precision. Blocks the CPU for the whole effect;
  simple, fine for short decorative bars.
- **Mechanism 2** (`raster_music_irq_start()`): a genuine CIA1 hardware
  IRQ installed directly on `$FFFE`/`$FFFF`, re-armed with a calibrated
  reload on every firing — frees the CPU between colour writes, but banks
  out KERNAL/BASIC/character ROM while active and (for reasons never fully
  root-caused, see memory: `mono_colorize_keypress_bug`) cannot reliably
  detect a keypress while running.

Both mechanisms exploit the same register-24/26 (`COLOR`) behaviour:
in a non-attribute-mode screen, writing `COLOR` mid-frame changes the
*background* nibble for every subsequent scanline until the next write —
in attribute mode, only the background nibble is ever affected regardless
(each character's own colour-RAM entry always overrides the foreground),
which is what makes "raster bar behind fixed-colour text" trivial in
attribute mode and what required the "cap line" bookend pattern
(`raster_bar_line()` calls bracketing an animated segment) in
`idi8b_logo_demo()`'s non-attribute mode.

### Fast loading: Krill

Not a VDC technique specifically, but relevant to any VDC demo built
around loading large picture assets from disk: `vdcmodemania-oscar64`
integrates Krill's Loader (v194, [csdb.dk/release/?id=226124](https://csdb.dk/release/?id=226124),
by Krill of Plush) in its original 6502-assembly form, built via a
separate `ca65`/`cl65` toolchain step and called into via a thin Oscar64 C
wrapper (`include/krill.c`/`.h`) — never compiled through Oscar64 itself.
Gated behind a `-dKRILL` build variant (`make krill`) so the standard
`bnk_load()`-based build is unaffected. One hard-won gotcha: **never call
`cia_init()` while Krill is installed** — Oscar64's `cia_init()`
unconditionally sets `cia2.pra = 0x07`, directly conflicting with
`krill_init()`'s own `cia2.pra = 2` (the IEC bus line state Krill's loader
protocol needs for its whole active session) — see memory:
`krill_cia_init_conflict`.

---

## Quick-reference checklist for a new mode

1. Find the source demo's register table (Tokra's BASIC `DATA` statements,
   or equivalent) and transcribe every register it sets — don't assume
   defaults are fine for anything the source explicitly sets, even if the
   value looks unusual (see `VDC-FLI`'s per-frame CSIZE toggle).
2. If the mode is interlaced (`LACE=3`), check `VSYNC` explicitly — don't
   leave it to inherit from a prior mode. Try `VTOTAL − VADJUST` if the
   source doesn't set it either, and confirm with a live register-dump
   comparison.
3. If the mode is interlaced and needs a two-file picture split, figure
   out *which* interlace method (address-gap vs source-interleaved) by
   checking how the demo function places the two halves — a distinct
   gapped second address means address-gap (keep source in plain row
   order); contiguous placement means source-interleaved (split the
   source by even/odd row, not by physical half).
4. If the mode sets any register outside the "always covered" set
   (`HDISPLAY`/`HSYNC`/`SYNCSIZE`/`HSCROLL`/`VSCROLL`/`HEND`/`DISP_ADDR`/
   `ATTR_ADDR`/`HSTART`) that a later mode might not also set, add it to
   `vdc_reset_boot_registers()`'s capture/restore list rather than hoping
   nothing downstream depends on the boot default.
5. If a register's effect depends on another register's current value
   (`ROWINC` vs `DISP_ADDR`, `HSCROLL` vs `DISP_ADDR` — see "Register
   write ORDER matters"), write the dependency first, then the dependent
   register — every time, not just when a caller judges it's needed.
   Never bake such a register into a mode's own `regset[]` row if
   `vdc_set_mode()`'s own DISP_ADDR-then-regset sequencing would apply it
   before the real value is set.
6. If the mode uses per-cell colour attributes, use
   `(background<<4)|foreground` for the attribute plane byte — see "The
   bitmap attribute plane's own byte format" — and don't trust a
   self-consistent encode/decode round-trip test as proof; verify against
   real hardware or an independent reference instead.
7. Test live, and if anything looks torn/shifted/wrong, reach for a VICE
   monitor `io d600` register dump compared against a known-working
   reference *before* re-deriving values from first principles again —
   it's found the actual root cause faster than static analysis every
   single time this project has tried both.
