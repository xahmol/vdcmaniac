# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

An Oscar64/C remake of "VDC Mode Mania" — a Commodore 128 demo/tool that shows
off the VDC (8563, 80-column chip)'s various bitmap and text display modes.
The original (BASIC, by an unknown author) ships in `original/v12/` for
reference: detokenised source, disk images, and the image-format converters
it used. This repo is a from-scratch C128/Oscar64 rewrite, still early —
`src/main.c` currently drives three effects (a VDC raster-bar test, a hires
plasma, and a color-cycling rotate) plus a title screen, with more modes to
come as image assets under `assets/vdcmodemania/` get wired in.

## Build commands

```
make            # build vdcmaniac.prg, boot sector, and a D81 disk image in build/standard/
make clean      # remove everything in build/
make vice       # run the D81 in the x128 emulator
make deploy     # wput the build to a primary Ultimate II+ over FTP (needs ULTIP1 in .env)
make deploy2    # same, to a second Ultimate II+ (needs ULTIP2 in .env)
make docs       # regenerate README.pdf from README.md (requires pandoc)
```

Toolchain: Oscar64 at `/home/xahmol/oscar64/bin/oscar64` (edit `CC` in the
`Makefile` if installed elsewhere), `c1541` (VICE) for disk images, `wput` for
deployment. Deployment IPs go in a gitignored `.env` (`ULTIP1`/`ULTIP2`) — see
README "Building from source". There is no automated test suite; verification
is visual, via `make vice` or real hardware.

The Makefile builds two D81 variants: `standard` (`all`/`vice-stnd`, plain
`bnk_load()`-based asset loading) and `krill` (`vice`, Krill fastloader --
see `krill_manual.md`), the latter being the one actually meant to be
tested/deployed going forward. The `flossiec`-fastload variant and `d64`/
`d71` disk images have been dropped entirely (asset sizes need D81's larger
capacity; `flossiec` never had a D81 variant) — don't reintroduce them
without being asked. Update the `MAIN_SRCS` list in the Makefile whenever
`src/main.c` starts `#include`-ing another `include/*.h` (it's a plain file
list, not auto-derived from `#pragma compile` chains, so make won't know to
rebuild otherwise).

## Architecture

- `src/main.c` is the demo driver: `main()` calls `bnk_init()` and
  `vdc_init()` once, then runs each effect in sequence
  (`raster_place_test()` → `title_screen()` → `plasma_demo()` →
  `rotate_demo()`), and `vdc_exit()` at the end. Each effect function owns
  its own VDC mode switch, screen data, and cleanup.
- `raster_bar()`/`raster_place_test()` are the VDC equivalent of a C64 raster
  bar, built on `include/vdc_raster.c`'s `raster_synch()`/`raster_waitline()`
  — this is the same CIA2-timer-based busy-wait synchronization technique
  prototyped in the separate `VDCRasterExperiment` repo (plain CC65
  assembly), now reimplemented as an Oscar64 C/inline-asm library function
  so any effect can call it.
- `plasma_demo()`/`init_plasma()` render a classic sine-table plasma directly
  into VDC hires bitmap memory (`vdc_state.bitmap`), recalculating per-pixel
  color from two independently-scrolling `colormap0`/`colormap1` sine
  offsets each frame.
- `rotate_demo()`/`init_rotate()`/`rotup()`/`rotdown()` cycle a fixed VDC
  bitmap image's color/attribute table to fake movement/rainbow effects
  without touching pixel data.
- `title_screen()` loads two prerendered hires screen halves
  (`vdce-scrtit.top`/`.bot`, built with the author's own VDC Screen Editor)
  into Bank 1 RAM via `bnk_load()`, blits them to VDC memory with
  `bnk_cpytovdc()`, and layers a raster-bar effect underneath while waiting
  for a keypress.
- VDC/bank helper libraries live in `include/` and follow the conventions in
  `~/.claude/vdclib_c128.md` (`vdc_core.h`+`banking.h` — the banking-aware
  pair, since this project uses Bank 1 for screen/charset data): `vdc_core.c`
  for mode/register/bitmap handling, `banking.c` for MMU bank access and
  Kernal file I/O, `vdc_raster.c` for the raster-sync helpers above.
- `include/vdc_win.c`, `vdc_menu.c`, `vdc_softscroll.c`,
  `vdc_textscroller.c`, and `krill.c`/`krill.h` are present but **not yet
  `#include`-d from `main.c`** — they're staged for upcoming modes (windowed
  UI, a fast KRILL-based loader, soft-scrolling, a text scroller) rather than
  dead code. Don't assume they're wired in without checking `main.c`'s
  `#include` list first.
- `src/main.-vdctestold.c` is a superseded earlier version of `main.c`, kept
  for reference; it is not part of the build (`MAINSRC` in the `Makefile`
  points only at `src/main.c`).
- `assets/vdcmodemania/` holds the original demo's per-image bitmap/color
  data (`.bb`/`.bt`/`.cb`/`.ct` and `.bit`/`.col` pairs, one per showcased
  picture) carried over from `original/v12/sd2iec-version/` — these are the
  modes still to be ported into `main.c`.

## Related reference material

Per the user's global instructions, consult in priority order: Oscar64
reference (`oscar64manual.md` in this repo / `~/.claude/oscar64.md`), the
general C64/C128 demo-coding and hardware references, and the VDC library
reference (`~/.claude/vdclib_c128.md`, canonical source
`/home/xahmol/VDCScreenEditor2/vdclib_manual.md`) for the full `vdc_core`/
`vdc_win`/`banking` API used throughout `include/`.
