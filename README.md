# vdcmaniac

A modern Commodore 128 VDC (8563/8568, 80-column chip) demo, written from
scratch in C with the [Oscar64](https://github.com/drmortalwombat/oscar64)
cross-compiler.

## Inspiration

This project is inspired by **VDC Mode Mania**, a 2012 demo by **Tokra**
and **Mike** of **Akronyme Analogiker**, originally released at the
Connected 9 party (Sep 2, 2012), later extended twice (an 800x600 mode in
2015, a 960x540 mode in 2023). VDC Mode Mania was a slideshow showcasing the
C128's rare, little-used 64KB-VDC-RAM graphics modes -- interlaced and
non-interlaced colour and monochrome modes at resolutions far beyond the
VIC-II's usual 320x200, from 480x252 colour up to 960x540 monochrome -- with
converted picture assets and BASIC-based viewers. The original demo, its
BASIC source, its image converters, and its own README are kept for
reference under `original/v12/`.

`vdcmaniac` is **not a port** of that BASIC codebase. It's a from-scratch
reimplementation in C, targeting the same family of rare VDC modes but built
around this project's own tooling and techniques:

- **Oscar64**, a modern C cross-compiler for 6502 targets, instead of
  interpreted BASIC.
- **[Krill's fastloader](https://csdb.dk/release/?id=226124)**, with
  on-the-fly TSCrunch decompression, for asset loading -- see
  `krill_manual.md`.
- A **custom raster-IRQ trampoline**, installed directly on the CPU's
  hardware interrupt vector (not the KERNAL soft vector, which this
  project's own boot sequence makes unreliable), driving per-scanline VDC
  colour effects and, going forward, synced SID music playback -- see
  `raster_lib_manual.md`.
- A **from-scratch VDC mode/register library** (`include/vdc_core.c`) and
  its own picture converters (`tools/vdc_convert.py`) rather than the
  original's PC-side C converters -- see `vdc_reference_manual.md`.

The set of VDC modes being brought back is the same one VDC Mode Mania
demonstrated (see `original/v12/readme.txt` for the full list and Tokra's
own notes on real-hardware/monitor compatibility per mode); the techniques
driving them are new.

**On pictures**: some sections currently still load Tokra's own original
converted images (kept, credited, as a working baseline while each mode is
brought up) or early own conversions of placeholder source images. These
are being replaced with this project's own artwork over time as each
section's conversion pipeline (`tools/vdc_convert.py`) matures -- not yet
finished across every mode.

## Contents

- [Inspiration](#inspiration)
- [Building from source](#building-from-source)
- Further documentation:
  [`ARCHITECTURE.md`](ARCHITECTURE.md) (code layout),
  [`vdc_reference_manual.md`](vdc_reference_manual.md) (VDC registers/modes),
  [`raster_lib_manual.md`](raster_lib_manual.md) (raster bar/IRQ library),
  [`krill_manual.md`](krill_manual.md) (fastloader integration)

## Building from source

### Prerequisites

| Tool | Purpose | Install |
|---|---|---|
| [oscar64](https://github.com/drmortalwombat/oscar64) | C compiler targeting C128 | Build from source or download release |
| `c1541` | Disk image creation (part of VICE) | `sudo apt install vice` |
| `wput` | FTP upload to Ultimate II+ | `sudo apt install wput` |
| `pandoc` + `texlive-xetex` | Regenerate README.pdf from README.md | `sudo apt install pandoc texlive-xetex` (optional) |
| `python3` + Pillow | Regenerate converted picture assets | `pip install Pillow` (optional -- build warns and skips if missing) |

The compiler is expected at `/home/xahmol/oscar64/bin/oscar64`. Edit the `CC` variable in the Makefile if yours is installed elsewhere.

### Deployment setup (.env)

Create a `.env` file in the project root to configure deployment to your Ultimate II+. This file is gitignored and will never be committed:

```ini
ULTIP1 = 192.168.1.xx       # IP of your primary Ultimate II+
# ULTIP2 = 192.168.1.yy    # optional second machine
```

The Makefile constructs the FTP path as `ftp://$(ULTIP1)/usb1/temp/`. Override `ULTUSB ?= usb1` in `.env` if your USB port is numbered differently.

### Make targets

| Target | Description |
|---|---|
| `make` / `make all` | Build the program, boot sector and the Krill-fastloader D81 disk image (see `krill_manual.md`) |
| `make krill` | Same D81 build, invokable on its own |
| `make clean` | Remove all build artefacts |
| `make vice` | Launch VICE x128 with the D81 |
| `make deploy` | Upload build to primary Ultimate II+ (requires `ULTIP1` in `.env`) |
| `make deploy2` | Upload to second Ultimate II+ (requires `ULTIP2` in `.env`) |
| `make docs` | Regenerate `README.pdf` from `README.md` (requires pandoc) |
