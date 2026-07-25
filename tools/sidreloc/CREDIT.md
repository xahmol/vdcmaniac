# Credit

`sidreloc` -- relocating SID player code (zero-page + address references)
to a target load address and zero-page window.

Written by Linus Åkesson.

- https://www.linusakesson.net/software/sidreloc/
- Source: https://hd0.linusakesson.net/files/sidreloc-1.0.tgz

Unmodified from the 1.0 tarball (`cpu.c`, `solver.c`, `sidreloc.c`, `reloc.h`,
`Makefile`, `README`, `COPYING`, `sidreloc.1`). Licensed under the terms in
`COPYING` (MIT).

Used in this project to relocate `Maniac.sid` (Paul Kleimeyer, 1983, Access
Software -- https://csdb.dk/release/?id=238071 /
https://hvsc.csdb.dk/MUSICIANS/K/Kleimeyer_Paul/Maniac.sid) from its native
`$7580` load address to `$2080` (page `$20`, matching this project's memory
map -- see `include/defines.h`'s `SIDINIT`/`SIDPLAY`), with its zero-page
usage (`$fb`/`$fc` natively) moved to `$80`/`$81` via `-z 80-df`, clear of
both Oscar64's own zero page (`$f7`-`$ff`) and Krill's loader zero page
(`$e0`-`$f5`). Conversion command:

```
sidreloc -v -p 20 -z 80-df Maniac.sid maniac_reloc.sid
```

Verified via the tool's own PAL-emulated playback check: 0% bad pitches,
0% bad pulse widths, both before and after relocation.
