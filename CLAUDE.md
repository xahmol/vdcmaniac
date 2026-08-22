# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

An Oscar64/C remake of "VDC Mode Mania" (Tokra/Mike, Akronyme Analogiker,
2012) — a Commodore 128 demo showcasing the VDC (8563, 80-column chip)'s
rare 64KB-VDC-RAM bitmap modes, from 480x252 colour up to 720x700
monochrome. The original BASIC version (detokenised source, disk images,
its own image converters) is available from CSDb:
https://csdb.dk/release/?id=234174 — kept locally under `original/` for
reference only, not tracked in this repo (gitignored) — this is a
from-scratch C rewrite: its own menu-driven demo
driver, its own VDC mode/register library, its own picture converter, its
own asset loader, and its own sourced/credited artwork throughout (see
`include/defines.h`'s own credit block — none of Tokra's original
converted images are used).

Menu-driven: `main_menu()` presents 8 selectable sections (any order, any
number of times) — three grouped real-photo VDC colour-mode showcases
(VDC-FLI: FLI+HFLI non-interlace; VDC-IFLI: IHFLI+ITFLI interlace;
VDC-mono: IMONO+IM800, each entry cycling every photo from both of its
component modes), a real-ZX-Spectrum-picture showcase (VDC Spectrum), two
procedural effects (plasma, colour rotation), a scripted-scroll trio
(VDC-SCROLL: vertical, then horizontal, then a combined 4-corner
diagonal tour — see `menu_scroll_family()`), and "End demo + credits".
The raster-bar placement diagnostic (`raster_place_test()`) was retired
(2026-08-21) to reclaim code-size budget for VDC-SCROLL's third section
— its code is still in the repo, unused, but it's no longer reachable
from anywhere in the running demo. Before the menu, `main()` runs a
fixed intro sequence once: system diagnostics (PAL/NTSC, VDC revision,
64KB check), an idi8b logo section, and a title screen.

## Build commands

```
make            # build vdcmaniac.prg, boot sector, and the krill D81 disk image in build/krill/
make clean      # remove everything in build/
make vice       # run the D81 in the x128 emulator
make z64k       # run the D81 in z64k (independent C128/VDC emulator, downloads Z64K.jar on first use)
make deploy     # wput the build to a primary Ultimate II+ over FTP (needs ULTIP1 in .env)
make deploy2    # same, to a second Ultimate II+ (needs ULTIP2 in .env)
make deploy3    # same, to a real-hardware test machine (needs ULTIP3 in .env)
make docs       # regenerate README.pdf from README.md (requires pandoc)
```

Toolchain: Oscar64 at `/home/xahmol/oscar64/bin/oscar64` (edit `CC` in the
`Makefile` if installed elsewhere), `c1541` (VICE) for disk images, `wput` for
deployment. Deployment IPs go in a gitignored `.env` (`ULTIP1`/`ULTIP2`/`ULTIP3`)
— see README "Building from source". There is no automated test suite;
verification is visual, via `make vice`, `make z64k`, or real hardware. VICE
has repeatedly not reproduced real-hardware-only VDC timing bugs this
project has hit (see `~/.claude/projects/-home-xahmol-git-vdcmaniac/memory/`
for specifics) — z64k, an independently-implemented emulator, is a useful
second data point when VICE says "fine" but real hardware doesn't.

The Makefile builds a single D81 variant: `krill` (`all`/`vice`, Krill
fastloader -- see `krill_manual.md`). The plain `bnk_load()`-based "standard"
build was dropped entirely (2026-07-26) — every asset load in `src/main.c`
goes through `krill_load()`/`krill_loadcompd()` unconditionally now, so
there's no `#if defined(KRILL)` branching left to build twice. The
`flossiec`-fastload variant and `d64`/`d71` disk images have also been
dropped entirely (asset sizes need D81's larger capacity; `flossiec` never
had a D81 variant) — don't reintroduce any of these without being asked.
Update the `MAIN_SRCS` list in the Makefile whenever
`src/main.c` starts `#include`-ing another `include/*.h` (it's a plain file
list, not auto-derived from `#pragma compile` chains, so make won't know to
rebuild otherwise).

## Architecture

- `src/main.c` is the whole demo: `main()` runs `cia_init()`/`bnk_init()`/
  `krill_loadcode()`/`krill_init()` once, a fixed intro sequence
  (`system_diagnostic_screen()` → `idi8b_logo_demo()` → `title_screen()`),
  loads the SID tune, then hands off to `main_menu()` (loops until
  ESC/STOP or "End demo + credits" is chosen) and `menu_end_demo()` →
  `demo_end_screen()` (never returns). Each section function owns its own
  VDC mode switch, asset load, and cleanup; `menu_entries[]` (near
  `main_menu()`) is the single source of truth for what's selectable and
  in what order it's listed.
- Picture showcase sections (`fli_color_demo()`, `fli_hfli_demo()`,
  `fli_ihfli_demo()`, `fli_itfli_demo()`, `mono_hires_xl_demo()`
  (VDC-IMONO), `mono_im800_demo()`, `spectrum_demo()`) all follow the same
  shape: blank → show an info/credit screen (`vdc_mode_info_screen()`) →
  `krill_loadcompd()` the picture into Bank 1 staging → blank →
  `vdc_init()` the mode → `bnk_cpytovdc()` the picture into VDC memory →
  enable display → wait for a keypress, cycling 3 photos per section (see
  `include/defines.h`'s credit block for sources/licences).
  `menu_fli_family()`/`menu_ifli_family()`/`menu_mono_family()` (just
  above `menu_entries[]`) are thin wrappers chaining two of these
  section functions back-to-back under one menu row each, keeping the
  FLI- and mono-family showcases within the raster-highlight sweep's own
  row budget; a dedicated BASIC8/iPaint showcase mode was considered and
  dropped instead (see TODO.md — its one genuinely new technique,
  odd/even-field colour blending, is now `tools/vdc_convert.py`'s own
  default for VDC-IFLI). `mono_im960_demo()`
  and `mono_colorize_demo()` exist but are intentionally not called
  anywhere (IM960 doesn't render correctly without RGBtoHDMI hardware;
  colorize's own keypress-detection mechanism was never fixed) — left in
  place for possible future real-hardware use, not menu-wired.
- `spectrum_demo()`/`convert_spectrum()` (VDC Spectrum): decodes real ZX
  Spectrum `.scr` screen dumps into flat 8x8 colour-attribute cells,
  reusing `VDC_HIRES_640x200_Color_PAL`'s own timing unchanged (pixel-
  doubled and centred, not a new `vdc_modes[]` row) — see `defines.h`'s
  credit block for the Tokra/VDC-SpectruMania reference this was verified
  against, and the three real demoscene picture credits.
- `plasma_demo()`/`init_plasma()` render a classic sine-table plasma
  directly into VDC hires bitmap memory, recalculating per-pixel colour
  from two independently-scrolling sine offsets each frame.
  `rotate_demo()`/`init_rotate()`/`rotup()`/`rotdown()` cycle a fixed VDC
  bitmap's colour/attribute table for movement/rainbow effects without
  touching pixel data.
- `menu_scroll_family()` (VDC-SCROLL) chains three scripted-scroll
  sections under one menu row: `vscroll_demo()` steps a tall (640x798)
  monochrome bitmap through the 640x200 display window a whole
  8-scanline row at a time via `DISP_ADDR` (deliberately no `VDCR_VSCROLL`
  sub-pixel smoothing — that was tried extensively and reliably tore on
  real hardware/z64k regardless of write timing; see project memory
  `vdcmaniac_vscroll_dispaddr_latch_lag.md` if revisiting); `panorama_demo()`
  pans a bitmap wider than the display using VDC register 27
  (`VDCR_ROWINC`) for per-scanline addressing plus `VDCR_HSCROLL` for
  sub-byte motion; `panorama2d_demo()` combines both into a diagonal
  4-corner tour of a bitmap both wider and taller than the display. The
  R27 mechanism only works if `VDCR_ROWINC` is written via an explicit
  call strictly *after* `DISP_ADDR` already holds its real value, never
  baked into a mode's own `vdc_modes[]` regset row — see project memory
  `vdcmaniac_r27_real_hardware_quirk_found.md` for the full technical
  history of this ordering requirement. `load_chunk_to_vdc()`/`krill_load_or_die()`
  (defined just above `vscroll_demo()`) are shared asset-loading helpers
  used project-wide, not just by this family.
- `raster_bar()`/`main_menu()`'s own highlight sweep are built on
  `include/vdc_raster.c`'s `raster_synch()`/
  `raster_waitline()` — CIA2-timer-based cycle-exact raster-position
  sync, reused via `raster_bar_begin()`/`raster_bar_line()`/
  `raster_bar_segment()`/`raster_bar_end()` (which also holds interrupts
  disabled for the sweep and provides a manual SID-fallback play call,
  since Krill's own interrupt-driven playback gets starved while SEI is
  held).
- `title_screen()` loads two prerendered hires screen halves into Bank 1
  via `krill_loadcompd()`, blits them to VDC memory, and layers a
  raster-bar effect underneath while waiting for a keypress.
  `idi8b_logo_demo()` shows the demo's own logo with a bouncing dual-bar
  raster effect, and is also where SID playback actually starts
  (`sid_music_init()`, right after this section's own `raster_calibrate()`
  — see that call site's own comment for why the ordering matters).
  `credits_screen()` (reached via `menu_end_demo()`) runs an endless
  scrolling-text + colour-cycling-bars sequence until ESC/STOP.
- VDC/bank helper libraries live in `include/` and follow the conventions
  in `~/.claude/vdclib_c128.md` (`vdc_core.h`+`banking.h` — the
  banking-aware pair, since this project uses Bank 1 for screen/charset
  data): `vdc_core.c` for mode/register/bitmap handling, `banking.c` for
  MMU bank access, Kernal file I/O, and the SID-playback fallback
  mechanism, `vdc_raster.c` for the raster-sync helpers above,
  `vdc_win.c` for the VDCWin windowed-text API (`vdcwin_checkch()` is used
  everywhere as the keyboard-check function; its popup/window-drawing
  functions aren't currently called by any section), `vdc_softscroll.c`
  for `softscroll_pan_pre()`/`softscroll_pan_post()`/
  `softscroll_buffer_shift_chunk()` (used by `credits_screen()`'s own
  horizontal panner), `vdc_textscroller.c` for the Cupid-font background
  text-stream renderer (also `credits_screen()`), `krill.c`/`krill.h` for
  the fastloader integration every asset load goes through. `vdc_menu.c`
  is present in `include/` but not `#include`-d from `main.c` at all —
  this project's own `main_menu()` is hand-rolled in `main.c`, not built
  on that library.
- `src/main.-vdctestold.c` is a superseded earlier version of `main.c`,
  kept for reference; not part of the build (`MAINSRC` in the `Makefile`
  points only at `src/main.c`).
- `assets/vdcmodemania/` (gitignored, not tracked) held the ORIGINAL
  demo's own per-image bitmap/colour data, carried over from its
  sd2iec-version release for reference only — no longer used by anything
  (every section now loads its own converted assets, built from
  `assets/source/*.jpg` via `tools/vdc_convert.py`, TSCrunch-compressed
  per `krill_manual.md`); see the original release on CSDb
  (https://csdb.dk/release/?id=234174) if this data is ever needed again.

## Related reference material

Per the user's global instructions, consult in priority order: Oscar64
reference (`oscar64manual.md` in this repo / `~/.claude/oscar64.md`), the
general C64/C128 demo-coding and hardware references, and the VDC library
reference (`~/.claude/vdclib_c128.md`, canonical source
`/home/xahmol/VDCScreenEditor2/vdclib_manual.md`) for the full `vdc_core`/
`vdc_win`/`banking` API used throughout `include/`.
