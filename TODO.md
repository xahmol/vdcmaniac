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

- [ ] **SID rate-accumulator fix: real PAL hardware pass still outstanding
  (only real hardware this project can currently test on).** Implemented
  and live-verified in VICE this session (see plan
  `want-to-revisit-timning-zany-patterson.md`): `raster_irq_playframe()`
  (`include/vdc_raster.c`) now calls `SIDPLAY` zero, one, or two times per
  IRQ via a signed fixed-point rate accumulator, generalized for any
  combination of tune-native-standard × machine-standard (per-tune
  properties `SID_TUNE_USES_CIA_SPEED`/`SID_TUNE_IS_NTSC` in
  `include/defines.h`, derived from `Maniac.sid`'s actual PSID header
  "speed" field, fetched from HVSC and decoded this session — confirmed
  CIA-timer tempo, not vsync).
  **PAL path (the real-hardware-testable case): fully verified.**
  Live-measured in VICE via the binary monitor: `sid_rate_inc` correctly
  resolves to `1935`, `sid_music_framecount` grew at ~59.9/61.2 calls/sec
  (target NTSC 59.826Hz) versus the old 50.1Hz PAL rate; demo boots through
  diagnostic screen → logo → title → main menu → a VDC-FLI picture load
  with no hang/crash, picture rendered correctly, music kept advancing
  through the `krill_loadcompd()` load. Still needs one real `make deploy`
  pass on actual C128 hardware to close this out (VICE-only so far).
  **NTSC path: code-verified correct, tempo unverifiable in this
  environment.** `sid_rate_inc` correctly resolves to `0` (matched
  standard, no correction needed) on both WSLg x128 and native Windows
  VICE, confirmed via direct register inspection — the logic is right.
  Actual playback tempo couldn't be confirmed by ear: both VICE builds,
  tested independently (ruling out host performance — native Windows
  VICE used only 50% of one core, not saturated), paced the emulation at
  ~51 calls/sec instead of NTSC's ~59.8, despite the running program
  correctly detecting/reporting NTSC on-screen. This looks like a VICE
  quirk in how `-model ntsc` (command-line) drives internal timing, not a
  vdcmaniac bug -- but since no NTSC hardware is available to this project,
  it can't be fully closed out beyond "code and PAL behaviour are both
  correct, NTSC audio timing accepted as unverifiable for now." Revisit
  only if NTSC hardware becomes available, or if the VICE quirk is worth
  chasing on its own for unrelated reasons.

## Resolved this session

- [x] **Windows VICE "Failed to mute device #11" report — root cause
  found, not what it looked like.** Not an audio-device issue: Krill's
  loader needs True Drive Emulation to run its drive-side install code
  (`M-E`/`M-R` commands), and this Windows VICE install had TDE off by
  default, causing Krill's install to hang silently (confirmed live via
  `-binarymonitor`: `VDriveCommand: Warning - M-E 020a (+14) (needs TDE)`
  in the log, matching a hang at the "loading assets" screen). Fixed by
  launching with `-drive8truedrive` (or enabling True Drive Emulation for
  drive 8 in VICE's own settings permanently). Not a bug in this repo.

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
