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

## Parked (do not pick up unprompted)

- [ ] **SID NTSC/PAL tempo mismatch.** `Maniac.sid` was composed for NTSC
  (~60Hz) and plays back at PAL's 50Hz — a ~17% tempo slowdown. Root cause
  is confirmed (not an IRQ-timing issue); the fix itself (call `SIDPLAY`
  at the tune's native rate, e.g. 6 times per 5 real frames) was never
  implemented. Explicitly parked by request ("Park SID for now. First fix
  images."). A separate CIA1-retiming attempt was tried and fully
  reverted after live IRQ-storm failures — see memory
  `vdcmaniac_sid_cia1_retime` for the resume checklist and root-cause
  findings before attempting again.

## Unresolved, low priority

- [ ] **Windows VICE "Failed to mute device #11" report.** Surfaced when
  Krill's loader activates on the user's own Windows VICE build. Almost
  certainly a local VICE drive-sound/TDE config issue (this project's
  build only ever attaches a d81 at unit 8, never 11), not a bug in this
  repo — not yet confirmed either way.

## Staged, not yet used (future modes/features, not currently planned work)

- `include/vdc_win.c`, `vdc_menu.c`, `vdc_softscroll.c`,
  `vdc_textscroller.c` are present but not `#include`-d from `main.c` —
  windowed UI, a popup/menu system, soft-scrolling, and a text scroller,
  staged for whenever a mode needs them.

## Explicitly out of scope (won't do)

- **VDC-IM960 (960x540 mono).** Code kept (`mono_im960_demo()`) but not
  called from `main()` — dropped because it never rendered correctly live
  without an RGBtoHDMI device, matching Tokra's own readme caveat.
- **The original demo's two extra 80-column text modes** ("larger screen
  area") were never ported and aren't tracked as planned work.
