# TODO

v1.0.0 has shipped, real-hardware-verified end to end. Nothing is
currently open — see `git log`, `ARCHITECTURE.md`, `vdc_reference_manual.md`
and per-topic memory records for implementation history. This file now
just tracks explicit "won't do" decisions, so they aren't re-litigated.

## Explicitly out of scope (won't do)

- **SID playback tempo correction for a PAL/NTSC mismatch.** Attempted
  twice (a reverted CIA1 Timer B retime, and a dormant rate-accumulator
  mechanism in `raster_irq_playframe()`/`sid_music_init()`, removed
  2026-08-30) -- real-hardware/VICE feedback (lemon64 forum, user "DDT")
  found the corrected playback audibly worse (frequent glitches) than
  simply letting the current PAL-composed, VBI-tempo tune play ~19% fast
  on NTSC. `raster_irq_playframe()` now always does exactly one SIDPLAY
  call per IRQ, unconditionally. Don't reintroduce without a specific
  request.
- **VDC-IM960 (960x540 mono).** Code kept (`mono_im960_demo()`) but not
  called from `main()` — dropped because it never rendered correctly live
  without an RGBtoHDMI device, matching Tokra's own readme caveat.
- **The original demo's two extra 80-column text modes** ("larger screen
  area") were never ported and aren't tracked as planned work.
- **BASIC 8 / iPaint picture format support.** Real, implementable spec
  was researched (see project memory
  `vdcmaniac_basic8_ipaint_spectrum_formats.md`), but adds no capability
  this project doesn't already have — its one genuinely new technique
  (odd/even-interlace-field colour blending) is already
  `tools/vdc_convert.py`'s own default for VDC-IFLI
  (`convert_colour_cells_paired()`), and BASIC8 mode 1's own geometry
  matches VDC-IHFLI at a lower resolution.
