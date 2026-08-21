# TODO

Status tracker for work remaining before release. Completed work isn't
re-listed here — see `git log`, `ARCHITECTURE.md`, and per-topic memory
records for history. Items below are grouped by whether they block release.

## Before release

- [ ] **Joystick input not independently hardware-tested.** Wired
  demo-wide (`joy_poll()`, port 2) and structurally identical to the
  proven keyboard path, but never exercised with real joystick hardware
  or VICE joystick emulation — only the keyboard-side code path has been
  exercised live.
- [ ] **`fli_color_demo()` phantom-keypress risk, flagged not resolved.**
  Its SEI-held loop uses `keyb_poll()`/CIA1 matrix scan instead of
  `vdcwin_checkch()`; `joy_poll()` shares CIA1 `$dc00`/Port A with the
  keyboard-matrix row-select output, so holding a joystick direction/fire
  while a scan is mid-flight could in principle register as a phantom
  keypress. Mitigated (joystick only trusted as fallback when
  `keyb_key == 0`) but not live-stress-tested.
- [ ] **Live playtest still owed for Plasma effect / Colour rotation
  effect.** VDC-SCROLL (VDC-VSCROLL + VDC-PANORAMA + VDC-PANORAMA 2D) is
  now real-hardware-confirmed end to end. The main menu's other six
  entries (VDC-FLI, VDC-IFLI, VDC-mono, VDC-SCROLL, VDC Spectrum, End
  demo) are all individually live-confirmed correct; these two weren't
  touched by the recent restructuring and haven't been independently
  re-checked since.

## Before release (continued)

- [x] **SID playback — resolved, real-hardware-confirmed.** The resident
  tune ("Maniac" by Antti Hannula/Flex, a PAL-native/VBI-tempo composition
  — see `defines.h`) needs no rate correction at all
  (`SID_TUNE_USES_CIA_SPEED=0`), so `raster_irq_playframe()`'s own rate
  accumulator (`include/vdc_raster.c`, generalized for any future
  CIA-timer tune via `SID_TUNE_USES_CIA_SPEED`/`SID_TUNE_IS_NTSC` in
  `defines.h`) is currently inert by design — exactly one `SIDPLAY`
  call/IRQ, no correction needed. Confirmed live on real C128 hardware:
  music plays correctly through every section, including the FLI/MONO
  sections' own SEI-held raster loops (`sid_play_frame_foreground()`
  fallback, `banking.c`). NTSC playback tempo remains unverified (no NTSC
  hardware available to this project) — revisit only if that hardware
  becomes available.

## Staged, not yet used (future modes/features, not currently planned work)

- `include/vdc_menu.c` (a popup/menu-system library) is present but not
  `#include`-d from `main.c` — this project's own `main_menu()` is
  hand-rolled directly in `main.c` instead. `vdc_win.c`, `vdc_softscroll.c`,
  and `vdc_textscroller.c` are now all wired in and actively used
  (`vdcwin_checkch()` demo-wide, `credits_screen()`'s own panner/scroller).

## Colour-cell attribute byte convention

Every colour-cell showcase mode (VDC-FLI, VDC-HFLI, VDC-IHFLI, VDC-ITFLI,
VDC Spectrum) packs its per-cell attribute bytes as
`(background<<4)|foreground` — background in the high nibble, foreground
in the low nibble. This is the VDC's own real hardware convention for its
bitmap attribute plane, distinct from `VDCR_COLOR`/register 26 (a
separate, single global register with its own, different format).
Confirmed by live testing across real hardware, VICE, and z64k on all
five modes. If a future colour-cell mode is ever added, use this
convention by default — see project memory
`vdcmaniac_attribute_byte_nibble_order.md` for the full diagnostic
writeup and a methodology note on why a purely self-consistent
encode/decode test cannot validate a hardware-interface convention like
this one.

## Future modes: BASIC8/iPaint dropped, VDC Spectrum done

- **BASIC 8 / iPaint picture format support — not planned.** Real,
  implementable spec is documented (18-byte `BRUS` header, RLE scheme,
  mode 0/1/2 geometry, odd/even-interlace-field colour cells — see project
  memory `vdcmaniac_basic8_ipaint_spectrum_formats.md` for the full
  reference if revisited). Not pursued as a showcase mode because it adds
  no capability this project doesn't already have: its one genuinely new
  technique (shared background, independently-blended foreground per
  interlace field, for shades beyond the native 16-colour palette) is
  already `tools/vdc_convert.py`'s own default for VDC-IFLI
  (`convert_colour_cells_paired()`), and BASIC8 mode 1's own geometry
  matches VDC-IHFLI at a lower resolution (640x400 vs 640x480). If ever
  revisited, treat it as a lightweight "decode a real unconverted iPaint
  file" curiosity, not a peer of the other photo modes.
- **VDC Spectrum — implemented.** `spectrum_demo()` (`src/main.c`) and
  `convert_spectrum()` (`tools/vdc_convert.py`), menu key `'7'`. Decodes
  real ZX Spectrum `.scr` screen dumps (6912 bytes: 6144 bitmap + 768
  attribute, the standard well-documented format) into vdcmaniac's own VDC
  output, TSCrunch-compressed and loaded via `krill_loadcompd()` the same
  as every other picture section. Reuses `VDC_HIRES_640x200_Color_PAL`'s
  own already-proven timing unchanged — no new `vdc_modes[]` row, no new
  horizontal-timing register values (confirmed via disassembly that
  Tokra's own "VDC SpectruMania" reference does the same: zero writes to
  any VDC timing register, only bitmap/attribute addressing). The
  Spectrum's 256x192 picture is pixel-doubled to 512 VDC pixels wide,
  centred with an 8-char border each side and one blank top char row, with
  a direct dim/bright Spectrum-to-VDC palette mapping (`SPECTRUM_TO_VDC`
  table) and flat 8x8 attribute cells — a colour granularity this project
  didn't have before (existing range was 8x1 FLI up to 8x3 ITFLI). Needed
  `#pragma heapsize(0)` in `main.c` once total code/data growth hit
  Oscar64's "error 3034: Cannot place heap section" (documented fix, see
  `oscar64manual.md`'s own gotcha entry — nothing in this project uses the
  heap). Three real demoscene graphics-competition entries used (not
  Tokra's own bundled game screenshots, which remain licensing-uncertain
  and unused): "np" (prof4d, DiHalt Lite 2015), "Prisoner of Time" (PheeL,
  Chaos Constructions 2001), "Cursed Eighth" (Piesiu, Chaos Constructions
  2010), sourced via zxart.ee's public API and credited by scener/party in
  `defines.h` per the demoscene's own reuse-with-credit norm (not a formal
  CC licence like the rest of the photo roster). Live-confirmed working
  end to end.
- **VDC Spectrum's own two other display modes** (256x192 "double-pixel-
  width" via an unproven hardware pixel-doubling trick, and the 512x384
  4-in-1 multisync mode) were NOT pursued — the implemented mode
  ("standard-pixel-width", storing literal doubled data) was deliberately
  chosen as the low-risk option needing no new/unproven VDC timing.
  Revisit only if there's a specific reason to want the other two modes'
  own tradeoffs.

## Explicitly out of scope (won't do)

- **VDC-IM960 (960x540 mono).** Code kept (`mono_im960_demo()`) but not
  called from `main()` — dropped because it never rendered correctly live
  without an RGBtoHDMI device, matching Tokra's own readme caveat.
- **The original demo's two extra 80-column text modes** ("larger screen
  area") were never ported and aren't tracked as planned work.
