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

**On pictures**: every showcase section uses this project's own sourced
artwork, converted via `tools/vdc_convert.py` -- public-domain paintings/
prints and CC-licensed photographs, each with a full attribution/licence
note in `include/defines.h`'s own credit block. None of Tokra's original
converted images are used.

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
| `java` (JRE 8+) | Run [z64k](https://www.z64k.com/) (`make z64k`) | `sudo apt install default-jre` (optional) |
| `pandoc` + `texlive-xetex` | Regenerate README.pdf from README.md | `sudo apt install pandoc texlive-xetex` (optional) |
| `python3` + Pillow | Regenerate converted picture assets | `pip install Pillow` (optional -- build warns and skips if missing) |

The compiler is expected at `/home/xahmol/oscar64/bin/oscar64`. Edit the `CC` variable in the Makefile if yours is installed elsewhere.

### Deployment setup (.env)

Create a `.env` file in the project root to configure deployment to your Ultimate II+. This file is gitignored and will never be committed:

```ini
ULTIP1 = 192.168.1.xx       # IP of your primary Ultimate II+
# ULTIP2 = 192.168.1.yy    # optional second machine
# ULTIP3 = 192.168.1.zz    # optional real-hardware test machine (make deploy3)
```

The Makefile constructs the FTP path for `deploy`/`deploy2` as
`ftp://$(ULTIP1)/usb1/temp/`. Override `ULTUSB ?= usb1` in `.env` if your
USB port is numbered differently. `deploy3` uses its own dedicated path
(`ULTPATH3`, default `/USB1/idi8b/dev/`) instead -- override that in
`.env` too if you want it elsewhere.

### Make targets

| Target | Description |
|---|---|
| `make` / `make all` | Build the program, boot sector and the Krill-fastloader D81 disk image (see `krill_manual.md`) |
| `make krill` | Same D81 build, invokable on its own |
| `make clean` | Remove all build artefacts |
| `make vice` | Launch VICE x128 with the D81 -- **needs True Drive Emulation enabled** (VICE Settings -> Drive, or `x128 -drive8truedrive`); Krill's loader runs drive-side install code (`M-E`/`M-R`) that silently hangs at the "loading assets" screen without it |
| `make z64k` | Launch [z64k](https://www.z64k.com/) with the D81 (downloads `Z64K.jar` on first use; Java 8+ required) -- a second, independently-implemented C128/VDC emulator, useful when VICE doesn't reproduce a real-hardware issue |
| `make deploy` | Upload build to primary Ultimate II+ (requires `ULTIP1` in `.env`) |
| `make deploy2` | Upload to second Ultimate II+ (requires `ULTIP2` in `.env`) |
| `make deploy3` | Upload build to a real-hardware test machine (requires `ULTIP3` in `.env`) |
| `make docs` | Regenerate `README.pdf` from `README.md` (requires pandoc) |

## License

This project's own code is licensed under the [GNU General Public
License v3.0](LICENSE). Krill's fastloader (`krill/`) and TSCrunch
(`krill/loader/tools/tscrunch/`) are vendored third-party components
under their own, separate, permissive attribution-required licenses --
both are credited in-demo (the end-credits scroller) and in this
README's own Inspiration section, per their own license terms. Every
picture/artwork/music asset has its own individual attribution and
licence noted in `include/defines.h`'s credit block -- see "On
pictures" above.
