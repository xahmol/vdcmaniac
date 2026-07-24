# Krill Fast Loader Integration Manual

Reference for how `vdcmodemania-oscar64` uses Krill's Loader (v194,
[csdb.dk/release/?id=226124](https://csdb.dk/release/?id=226124), by Krill
of Plush, assisted by Bitbreaker of Nuance and Performers) as a drop-in
fast-loading replacement for the KERNAL's own `LOAD` — a `-dKRILL` build
variant, gated so the standard `bnk_load()`-based build is completely
unaffected. Covers the integration architecture, the C API, the build
pipeline, and the one hard-won gotcha (`cia_init()`) that cost a real
debugging session to find.

---

## Why Krill, and why in original form

The KERNAL's own serial-bus `LOAD` is slow — Krill's Loader replaces it
with custom drive-side code (uploaded once, at install time, over the IEC
bus) that speaks a much faster protocol. Krill's source is ~14,800 lines
of 6502 assembly across drive-side and computer-side halves, built with
its own `ca65`/`cl65` toolchain — genuinely impossible to port through
Oscar64 (its inline `__asm{}` has no macros, no `.proc`/`.export` scoping,
no conditional assembly, all of which Krill's source leans on heavily) and
not attempted. Instead, this project follows the same pattern as the
sibling `Oscar64Test` repo: **use Krill in its original assembled form**,
built once via its own Makefile into two small `.prg` files, loaded by the
demo like any other asset and called into via a handful of fixed machine
code entry points. Oscar64 never compiles a single line of Krill's own
source — it only ever calls pre-assembled code.

---

## Architecture

Three pieces, all in `include/krill.c`/`krill.h`:

1. **`install-c128.prg`** (loads to `$A000`) — a one-time bootstrap that,
   when run, uploads Krill's real drive-side code to the disk drive over
   the IEC bus. Only needed transiently during install; the C128-side
   memory it occupies (`$A000`–`$BC74`) is free to be reused (e.g. as
   picture-loading staging space) immediately after `krill_init()`
   returns.
2. **`loader-c128.prg`** (loads to `$0B00`) — the small resident
   "computer-side" loader that stays resident for Krill's entire active
   session, providing the actual fast-load entry point (`KRILL_LOADRAW`,
   `$0B00`) and an uninstall entry point (`KRILL_UNINSTA`, `$0E91`).
3. **A thin Oscar64 C wrapper** (`include/krill.c`/`.h`) that loads those
   two `.prg` files via the existing `load_overlay()` helper (the same one
   `bnk_init()` uses for `vdcelmc`), and calls into their fixed entry
   points with `__asm { jsr $a000 }`-style stubs.

### C API

```c
#define KRILL_INSTALL 0xa000
#define KRILL_LOADRAW 0x0b00
#define KRILL_UNINSTA 0x0e91
#define KRILL_ZPSTART 0xf5

void krill_loadcode();
void krill_init();
char krill_load(char cr, const unsigned start, const char *fname);
void krill_done();
```

- **`krill_loadcode()`** — loads `install-c128.prg` and `loader-c128.prg`
  from disk into their fixed addresses (via `load_overlay()`, ordinary
  KERNAL `LOAD` — this one call is *not* itself fast-loaded, since Krill
  isn't installed yet). Call once, early, before `krill_init()`.
- **`krill_init()`** — runs the install bootstrap (`krill_install()`,
  `jsr $a000`), which uploads Krill's real drive-side code to the disk
  drive, then redirects the KERNAL IRQ vector (`$314`/`$315`) to
  `krill_interrupt`, a small internal handler Krill's own protocol needs.
  Call once, after `krill_loadcode()`.
- **`krill_load(cr, start, fname)`** — loads one file, fast, into MMU bank
  config `cr` at address `start` — the direct replacement for
  `bnk_load(device, bank, start, fname)`. Internally switches `mmu.cr` to
  `cr` only for the duration of the actual transfer, restoring whatever it
  was before immediately after (`krill_load_core()`, `krill.c`) — an
  ordinary temporary bank switch, not a session-wide ROM banking change
  the way Mechanism 2's `raster_music_irq_start()` does. Returns non-zero
  on error (same convention as `bnk_load()`'s return, just inverted —
  check the return value and bail out, matching every call site in
  `src/main.c`). Call as many times as needed, for as many assets as
  needed, between `krill_init()` and `krill_done()`.
- **`krill_done()`** — restores `$314`/`$315` to the standard KERNAL IRQ
  vector via a hardcoded literal (`$FA65`) — not a saved "old value" (none
  is ever captured; `krill_init()` overwrites the vector unconditionally
  too) — and uninstalls Krill's drive-side code (`jsr $0e91`). Call once,
  when no more Krill loads are needed for the rest of the program.

### Lifecycle: install once, load many, done once

`main()` brackets the **entire program run**, not each individual load:

```c
cia_init();
bnk_init();

#if defined(KRILL)
    krill_loadcode();
#endif

vdc_init(VDC_TEXT_80x25_PAL, 1);

#if defined(KRILL)
    krill_init();
#endif

    /* ... every demo section's own #if defined(KRILL) krill_load(...) ... */

#if defined(KRILL)
    krill_done();
#endif

vdc_exit();
```

Every individual asset load site in `src/main.c` looks like this — the
`#else` branch (unconditionally compiled into the standard, non-`KRILL`
build) is the original `bnk_load()` call, completely unchanged:

```c
#if defined(KRILL)
    if (krill_load(BNK_1_IO, MEM_SCREEN, "vdce-scrtit.top"))
    {
        printf("krill load failed: vdce-scrtit.top\n");
        exit(1);
    }
#else
    bnk_load(bootdevice, 1, (char *)MEM_SCREEN, "vdce-scrtit.top");
#endif
```

`BNK_1_IO` (`0x7e`, `banking.h`) is Krill's own MMU bank-config parameter
for the same staging-buffer destinations (`MEM_SCREEN`, `MEM_SID`, etc.)
this project already uses for `bnk_load()`/`bnk_cpytovdc()` — same
addresses, different bank-selection representation (`krill_load()` takes a
raw `mmu.cr` value directly; `bnk_load()` takes a KERNAL-`SETBNK`-style
bank number).

**Why install/done bracket the whole run, not each section**: an earlier,
narrower version of this integration (the Phase 1 proof-of-concept)
installed and tore down Krill tightly around just one section's own loads.
That was fine for proving the mechanism worked at all, but doesn't match
how a real multi-section demo should use it — see the `cia_init()` gotcha
below for what actually went wrong when this was widened to cover every
section without also fixing a hidden per-load "safety net" call that
turned out to conflict with Krill's own state.

---

## The one hard gotcha: never call `cia_init()` while Krill is installed

Oscar64's own `cia_init()` (`c64/cia.c`, part of the standard library, not
this project's code) unconditionally sets `cia2.pra = 0x07`. `krill_init()`
sets `cia2.pra = 2` as part of its own install — these are the IEC bus
control lines (ATN/CLK/DATA) Krill's loader protocol depends on for its
*entire* active session, not just around one individual load. Calling
`cia_init()` at any point between `krill_init()` and `krill_done()`
silently overwrites that back to `0x07`, and once more than one section
had loaded an asset via Krill this caused a real hang partway through the
demo (confirmed live in VICE: hung after a section's assets had loaded,
before that section's picture displayed).

This actually happened here: an earlier version of this integration added
a `cia_init()` call after every individual `krill_load()`, following a
plan document's own (plausible-sounding, but never actually tested)
recommendation — "a cheap, standing safety net against leftover CIA1
interrupt-pending state." It sounded reasonable because this project
*does* call `cia_init()` once at program start for a real, separate
reason (leftover CIA1 interrupt-pending state from the boot-sector/disk
process corrupting `$314`/`$315` before `raster_music_irq_start()` could
save them) — but that startup call runs *before* Krill is ever installed,
so it doesn't touch Krill's runtime state at all. The per-load version was
different in kind, not just redundant.

**The fix, and the rule going forward**: `cia_init()` runs exactly once,
at the very top of `main()`, before `bnk_init()`/`krill_loadcode()`/
`krill_init()` ever run — and never again anywhere else in the program,
matching `Oscar64Test`'s own proven reference usage exactly (it doesn't
call `cia_init()` again either, not even after its own `krill_done()`). If
a genuine CIA-state safety net is ever needed *during* an active Krill
session, it would need to be something that doesn't touch `cia2.pra` at
all — not a blanket `cia_init()` call. See memory:
`krill_cia_init_conflict` for the full writeup.

---

## Build pipeline

Krill's own source (`krill/loader/`, `krill/shared/`) is checked directly
into this repo (copied from the sibling `Oscar64Test` repo, which has the
same checkout already proven working), built via its own `ca65`/`cl65`
Makefile — a genuinely separate toolchain step from Oscar64, sequenced by
this project's own Makefile:

```make
loader-c128.prg:
	@$(MKDIR) build/krill 2>$(NULLDEV) ; true
	cd krill/loader/; $(DEL) build/*.* 2>$(NULLDEV)
	cd krill/loader/; make PLATFORM=c128 prg INSTALL=A000 RESIDENT=0b00 ZP=f5 PROJECT=
	cd krill/loader/; $(RMDIR) build/intermediate 2>$(NULLDEV)
	cd krill/loader/; $(DEL) build/transient*.* 2>$(NULLDEV)
	cp krill/loader/build/*.prg build/krill
```

`$(MAIN).prg`'s own rule compiles **two** variants from the same
`src/main.c` in one `make` invocation — the standard build and, with
`-dKRILL` added, the Krill build:

```make
$(CC) $(CFLAGS) -dKRILL -n -o=build/krill/$(MAIN).prg $(MAINSRC)
$(CC) $(CFLAGS) -n -o=build/standard/$(MAIN).prg $(MAINSRC)
```

`vdcelmc.prg` (the low-memory banking overlay `bnk_init()` loads) doesn't
need any special handling for the Krill build — Oscar64's `#pragma
overlay(vdcelmc, 1)` mechanism (`banking.c`) emits it automatically
alongside whichever `$(MAIN).prg` variant is being compiled, landing in
the matching `build/{standard,krill}/` directory as a side effect of the
same compile invocation.

### Make targets

| Target | Builds |
|---|---|
| `make krill` | Full bootable `build/krill/vdcexp-krill.d81` — the `-dKRILL` binary, `install-c128.prg`/`loader-c128.prg`, and every asset the demo needs (every non-Krill-converted section still needs its own assets too, since only *some* call sites are on the fast-load path is not the case here — all of them are, but the disk image still needs every file regardless of loader). |
| `make d81` / `make all` | Standard (non-Krill) build only — unaffected by any of the above. |
| `make vice` | Runs the **Krill** build by default (`x128 build/krill/vdcexp-krill.d81`) — this is the variant actually meant to be tested/deployed going forward. |
| `make vice-stnd` | Runs the plain `bnk_load()`-based build instead, for comparison. |

---

## Adding a new Krill-loaded asset

1. Find (or add) the `bnk_load()` call site in `src/main.c`.
2. Wrap it:
   ```c
   #if defined(KRILL)
       if (krill_load(BNK_1_IO, dest, "filename"))
       {
           printf("krill load failed: filename\n");
           exit(1);
       }
   #else
       bnk_load(bootdevice, 1, (char *)dest, "filename");
   #endif
   ```
3. Do **not** add a `cia_init()` call anywhere near it (see the gotcha
   above).
4. Rebuild both `make d81` and `make krill`, confirm both compile clean.
5. Test live in VICE (`make vice` for the Krill build) — confirm the
   asset loads and displays identically to the standard build, *and* that
   every later section still behaves normally (the real test of Krill
   coexisting correctly with the rest of the program, not just of the one
   load succeeding).

## Debugging a Krill-specific problem

- **Hang after a load, before the picture displays**: almost certainly the
  `cia_init()` conflict — search for any `cia_init()` call between
  `krill_init()` and `krill_done()` and remove it.
- **Load reports failure (`krill_load()` returns non-zero)**: check the
  filename is present on the built disk image (`make krill`'s own file
  list, `Makefile`'s `ASSETS` variable) — Krill's loader still needs the
  file to actually exist on disk, same as `bnk_load()` would.
- **Works in the standard build, not the Krill build, no error reported**:
  compare register/memory state around the affected section the same way
  `vdc_reference_manual.md`'s "boot-baseline register leaks" section
  recommends for VDC bugs — a live comparison between the two build
  variants at the same point is far more reliable than guessing.
