# TODO

Status tracker for work remaining before release. Completed work isn't
re-listed here — see `git log`, `ARCHITECTURE.md`, and per-topic memory
records for history. Items below are grouped by whether they block release.

## Before release

- [ ] **Full live playtest of all 6 modes / 18 photos.** Automated
  verification (VICE keyboard-feed + `display_get()` screenshots) covered
  FLI byte-exact and confirmed the menu renders; the other 5 modes follow
  the same proven wiring pattern but haven't each been individually
  live-confirmed end to end by eye. Photo fixes since (Kelly Lee
  Owens/Maupi crops, poppy/lavender field swaps, ITFLI rose->Utrecht,
  FLI colour-destination bug) suggest this is substantially advanced —
  needs one clean pass through the whole menu to confirm nothing's left.
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

## Explicitly out of scope (won't do)

- **VDC-IM960 (960x540 mono).** Code kept (`mono_im960_demo()`) but not
  called from `main()` — dropped because it never rendered correctly live
  without an RGBtoHDMI device, matching Tokra's own readme caveat.
- **The original demo's two extra 80-column text modes** ("larger screen
  area") were never ported and aren't tracked as planned work.
