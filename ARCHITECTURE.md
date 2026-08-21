# Architecture

This documents the current, working state of the repository. For
day-to-day build commands and `Makefile` targets, see `CLAUDE.md`.

## What this project is

An Oscar64/C rewrite, for the Commodore 128 VDC (8563/8568, 80-column
chip), of **VDC Mode Mania** — a 2012 BASIC demo/tool by Tokra and Mike
(group "Akronyme Analogiker") that shows off the VDC's unusual bitmap
resolutions (480x252 up to 720x700, see `original/v12/readme.txt`). The
original ships in full under `original/v12/` (detokenised BASIC source,
disk images, PC-side image converters) for reference only — no code or
converted images from it are used. This is a from-scratch rewrite: its
own menu-driven demo, its own `vdc_core`/`vdc_win`/`banking` Oscar64
library (the same library suite used in `VDCScreenEditor2`), its own
picture converter (`tools/vdc_convert.py`), its own asset loader (Krill's
fastloader), and its own sourced/credited artwork (`include/defines.h`'s
credit block).

## What actually runs today

`main()` (`src/main.c`) runs a fixed intro sequence once, then hands off
to a menu that loops until the user ends the demo:

```
cia_init() / bnk_init() / krill_loadcode() / krill_init()
system_diagnostic_screen()   // PAL/NTSC, VDC revision, 64KB check
idi8b_logo_demo()            // logo + bouncing dual-bar raster effect; SID playback starts here
title_screen()                // prerendered hires title image + raster bars
r27_scroll_test_demo()        // TEMPORARY test scaffold, see its own section below
main_menu()                   // loops until ESC/STOP or "End demo + credits"
menu_end_demo() -> demo_end_screen()   // never returns
```

`main_menu()` presents 11 selectable sections, any order, any number of
times (`menu_entries[]` is the single source of truth for the list):

| Key | Section | Function |
|---|---|---|
| 1 | VDC-FLI (480x252, colour 8x1 cells) | `fli_color_demo()` |
| 2 | VDC-HFLI (640x400, colour 8x2 cells) | `fli_hfli_demo()` |
| 3 | VDC-IHFLI (640x480, interlace, colour 8x2) | `fli_ihfli_demo()` |
| 4 | VDC-ITFLI (640x576, interlace, colour 8x3) | `fli_itfli_demo()` |
| 5 | VDC-IMONO (720x700, interlace mono) | `mono_hires_xl_demo()` |
| 6 | VDC-IM800 (800x600, interlace mono) | `mono_im800_demo()` |
| 7 | Plasma effect | `menu_plasma_demo()` -> `plasma_demo()` |
| 8 | Colour rotation effect | `menu_rotate_demo()` -> `rotate_demo()` |
| 9 | Raster bar placement test | `raster_place_test()` |
| 0 | VDC-VSCROLL (640x200 window, scripted hires scroll) | `vscroll_demo()` |
| E | End demo + credits | `menu_end_demo()` -> `credits_screen()` |

### Picture showcase sections (keys 1-6)

`fli_color_demo()`, `fli_hfli_demo()`, `fli_ihfli_demo()`,
`fli_itfli_demo()`, `mono_hires_xl_demo()`, `mono_im800_demo()` all follow
the same shape: blank -> `vdc_mode_info_screen()` (text-mode info/credit
screen) -> `krill_loadcompd()` the picture into Bank 1 staging -> blank ->
`vdc_init()` the mode -> `bnk_cpytovdc()` into VDC memory -> enable
display -> wait for a keypress, cycling 3 real photos per section. Source
images live in `assets/source/*.jpg`, converted via
`tools/vdc_convert.py`, TSCrunch-compressed per `krill_manual.md`. Full
attribution/licence for every photo is in `include/defines.h`'s own
credit block.

`mono_im960_demo()` (VDC-IM960, 960x540) and `mono_colorize_demo()` exist
in the codebase but are never called — IM960 doesn't render correctly
without RGBtoHDMI hardware (matches Tokra's own readme caveat), and
colorize's own keypress-detection mechanism was never made reliable. Both
kept for possible future real-hardware work, not menu-wired.

### `raster_place_test()` — the raster-sync calibration tool

A 16-line colour raster bar, nudgeable up/down with cursor keys while it
prints the live rasterline number, until ESC/STOP. Built directly on
`raster_synch()`/`raster_waitline()` (`vdc_raster.c`) — this is the tool
used to hand-verify the CIA2-timer raster-sync constants
(`raster_timer_reload`, calibrated automatically at runtime by
`raster_calibrate()`) actually line up on real hardware.

### `title_screen()`

Loads two prerendered hires screen halves (`vdce-scrtit.top`/`.bot`, made
with the author's own VDC Screen Editor) into Bank 1 via
`krill_loadcompd()`, blits them into VDC bitmap memory with
`bnk_cpytovdc()`, then layers a raster-bar effect underneath (built on
`raster_bar()`) until a keypress.

### `plasma_demo()` / `init_plasma()`

Classic two-layer sine-table plasma rendered into VDC hires bitmap
memory, recalculating per-pixel colour from two independently-scrolling
`colormap0`/`colormap1` sine offsets each frame, double-buffered via the
attribute-address swap (`vdc_state.swap_attr`/`base_attr`).

### `rotate_demo()` / `init_rotate()` / `rotup()`/`rotdown()`

Cycles a fixed VDC hires image's attribute/colour table to fake
movement/rainbow effects without touching pixel data, via a software
`Screen[4000]` shadow buffer pushed to VDC attribute RAM each frame.

### `vscroll_demo()` (VDC-VSCROLL)

Steps a tall (640x798) monochrome bitmap through the 640x200 display
window a whole 8-scanline row at a time via `DISP_ADDR`, on a
scripted waypoint bounce (`PanWaypoint`). Deliberately no `VDCR_VSCROLL`
sub-pixel smoothing — a combined `DISP_ADDR`+`VSCROLL` smooth scroll (the
standard technique for this effect) was tried extensively and reliably
tore on real hardware and z64k regardless of write order or timing
relative to vblank; see project memory
(`vdcmaniac_vscroll_dispaddr_latch_lag.md`) before revisiting.

### `r27_scroll_test_demo()` — temporary test scaffold

**Not menu-wired** — called directly from `main()`, right before
`main_menu()`. VDC-PANORAMA attempt 3, Phase 0 only: proves whether R27
(`VDCR_ROWINC`) gives correct per-scanline addressing for a bitmap wider
than the 640px display (live-confirmed: yes). No motion yet, just a
static barcode test pattern with a keypress to continue into the normal
menu. See the plan file
(`~/.claude/plans/want-to-revisit-timning-zany-patterson.md`) for the
phased build this is one step of, and project memory
(`vdcmaniac_r27_phase0_confirmed.md`) for the finding itself.

### `credits_screen()`

Reached via `menu_end_demo()`. Endless scrolling-text stream (Cupid font,
`vdc_textscroller.c`) plus colour-cycling background bars, using
`vdc_softscroll.c`'s `softscroll_pan_pre()`/`softscroll_pan_post()`/
`softscroll_buffer_shift_chunk()` for its own horizontal panning, until
ESC/STOP.

### `raster_bar()` and friends

`raster_bar_begin()`/`raster_bar_line()`/`raster_bar_segment()`/
`raster_bar_end()` are the shared raster-bar mechanism used by
`title_screen()`, `idi8b_logo_demo()`, and `main_menu()`'s own selection
highlight — all built on `raster_synch()`/`raster_waitline()`
(CIA2-timer-based cycle-exact raster-position sync). `raster_bar_begin()`
holds interrupts disabled for the sweep; `raster_bar_end()` waits out the
rest of the frame, makes one manual SID-fallback play call
(`sid_play_frame_foreground()`, `banking.c` — Krill's own interrupt-driven
playback gets starved while SEI is held), then re-enables interrupts.

## Library layer: wired in vs. staged only

`src/main.c` `#include`s: `defines.h`, `banking.h`, `vdc_core.h`,
`vdc_win.h`, `vdc_raster.h`, `peekpoke.h`, `vdc_textscroller.h`,
`vdc_softscroll.h`, `krill.h`. Every one of these is actively used
somewhere in the current demo.

| File | Status | Purpose |
|---|---|---|
| `vdc_menu.c/h` | staged, unused | Pulldown menu system — `main_menu()` is hand-rolled in `main.c` instead, doesn't use this |
| `vdc_win.c/h` | wired in | `vdcwin_checkch()` used demo-wide for keyboard polling; its popup/window/viewport functions aren't called by any current section |
| `vdc_softscroll.c/h` | wired in | `credits_screen()`'s own horizontal panner |
| `vdc_textscroller.c/h` | wired in | `credits_screen()`'s own Cupid-font background text stream |
| `krill.c/h` | wired in | every asset load in the demo goes through `krill_load()`/`krill_loadcompd()` |
| `peekpoke.h` | used transitively | included by `vdc_core.c`/`banking.c` |

`src/main.-vdctestold.c` is a superseded earlier version of `main.c`, kept
for reference; not part of the build (`MAINSRC` in the `Makefile` points
only at `src/main.c`).

## Assets vs. code

Every showcase section's real picture asset is TSCrunch-compressed and
loaded via `krill_loadcompd()` (see `Makefile`'s `KRILL_COMPRESSED_ASSETS`
list) — no raw/uncompressed picture files ship in the D81. Source images
live in `assets/source/*.jpg` (public-domain paintings/prints and
CC-licensed photographs, one set per showcased picture — see
`include/defines.h`'s own credit block for full attribution), converted
via `tools/vdc_convert.py`.

`assets/vdcmodemania/` holds the ORIGINAL demo's own per-image bitmap/
colour data, carried over from `original/v12/sd2iec-version/` for
reference only — **not referenced anywhere in `src/` or `include/`**
(checked via grep). Every section now uses this project's own converted
assets instead.

## Build & tooling state

See `CLAUDE.md` for day-to-day build commands and `Makefile` targets. The
Makefile builds a single D81 variant (`krill`, Krill fastloader) — the
plain `bnk_load()`-based "standard" build and the `flossiec`/`d64`/`d71`
alternate targets have all been dropped entirely (asset sizes need D81's
larger capacity). Deployment to up to three Ultimate II+/64 devices is
supported (`make deploy`/`deploy2`/`deploy3`, IPs in a gitignored `.env`)
— `deploy3` is this project's own dedicated real-hardware test machine.
`make vice` (VICE x128) and `make z64k` ([z64k](https://www.z64k.com/), a
second independent VDC implementation) are both available for
emulator-side verification, though VICE has repeatedly not reproduced
real-hardware-only VDC timing bugs this project has hit — real hardware
or z64k are the more trustworthy verification paths for anything
timing-sensitive.
