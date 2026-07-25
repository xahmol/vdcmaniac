/*
Oscar64 Krill's loader function library

Written in 2024 by Xander Mol

https://github.com/xahmol/Oscar64Test

https://www.idreamtin8bits.com/

Code and resources from others used:

-   Oscar64 cross compiler

    https://github.com/drmortalwombat/oscar64

    Many thanks also to https://github.com/drmortalwombat to provide extrordinary support and tips for making this and adapting Oscar64 to my needs faster than I could ask it.

-   Krill's Loader, Repository Version 194, by Krill / Plush.

    https://csdb.dk/release/?id=226124

-   Tested using real hardware (C128D and C128DCR) plus VICE.

The code can be used freely as long as you retain a notice describing original source and author.

THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL, BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!
*/

#ifndef KRILL_H
#define KRILL_H

// Defines
#define KRILL_INSTALL 0xa000
#define KRILL_LOADRAW 0x0b00
#define KRILL_LOADCOMPD 0x0b14
#define KRILL_UNINSTA 0x0ef3
#define KRILL_ZPSTART 0xe0

// Function prototypes
void krill_loadcode();
char krill_load(char cr, const unsigned start, const char *fname);
char krill_loadcompd(char cr, const unsigned start, const char *fname);

// In low memory
__noinline void krill_init();
__noinline void krill_done();
__noinline void krill_load_core();
__noinline void krill_loadcompd_core();

// Globals
//
// Zero-page layout for the compressed-capable loader build (LOAD_COMPD_API=1,
// ZP=e0, krill-config/loaderconfig.inc), per krill/loader/build/loadersymbols-c128.inc:
//   $e0-$e1 loadaddrlo/hi      -- loadraw: absolute destination (LOAD_TO_API)
//   $e2-$e3 loadaddroffslo/hi  -- loadcompd: destination OFFSET added to
//                                 decdestlo/hi below (LOAD_TO_API)
//   $e4-$e5 decdestlo/hi       -- decompression write pointer ("tsput" in
//                                 krill/loader/src/decompress/tsdecomp.s).
//                                 NOT in loadersymbols-c128.inc -- resident.s
//                                 only wires this up via .exportzp when
//                                 MEM_DECOMP_TO_API is on (our config has it
//                                 off, only LOAD_COMPD_API); with it off,
//                                 resident.s falls back to a bare
//                                 `decdestlo: .res 1` / `decdesthi: .res 1`
//                                 (line ~79 of resident.s).
//                                 CORRECTION (July 2026): an earlier note here
//                                 claimed "nothing ever writes decdest on the
//                                 loadcompd path". That was WRONG -- it only
//                                 grepped resident.s, and only for the symbol
//                                 "decdestlo". tsdecomp.s ALIASES this cell as
//                                 `tsput = decdestlo` (its line 25) and DOES
//                                 write it: the INPLACE init loop (the .else
//                                 branch, ~line 86: `lda (tsget),y / sta
//                                 tsput,y` for y=0..3) copies the compressed
//                                 stream's first bytes -- TSCrunch's baked-in
//                                 depack address -- into decdest. So on a
//                                 carry-clear ("use file address") load,
//                                 decdest is correctly set from the asset.
//                                 Address empirically confirmed at $e4/$e5 via the
//                                 linker's own verbose map file (krill/loader
//                                 build's intermediate/loader-c128.map, kept
//                                 out of a normal build -- rebuild manually
//                                 with `make PLATFORM=c128 prg ...` directly
//                                 in krill/loader/ without the cleanup step
//                                 our own Makefile does, if this ever needs
//                                 re-checking against a future Krill version):
//                                 endaddrlo/hi confirmed at $e8/$e9,
//                                 BLOCKDESTLO at $ea, consistent with
//                                 decdestlo/hi + 2 unnamed TSCrunch dummy
//                                 bytes filling exactly $e4-$e7.
//                                 History of the bug fixed here (see the full
//                                 write-up on krill_loadcompd() in krill.c):
//                                 the code briefly used LOAD_TO_API offset mode
//                                 (carry set) with offset = the full target
//                                 address, on the mistaken belief that decdest
//                                 was uninitialized. Because decdest IS set
//                                 from the asset's baked address (see the
//                                 CORRECTION above), offset mode DOUBLED the
//                                 address: $5800 (baked) + $5800 (offset) =
//                                 $B000, which is the Oscar64 C stack for this
//                                 build (map: `b000 - bfa4 : STACK`). That
//                                 depacked ~4 KB onto the stack and crashed
//                                 into the KERNAL BREAK handler. Correct fix:
//                                 call with carry CLEAR so the asset lands at
//                                 its own baked address, no offset, no doubling.
// Not contiguous with the old raw-only ($f5-$fc) layout -- loadaddroffslo/hi
// and decdestlo/hi did not exist there, so endaddrlo/hi immediately followed
// loadaddrhi. That endaddr field is dropped here: END_ADDRESS_API is disabled
// in our config (never populated by the loader) and nothing in this codebase
// read it.
struct KRILLZP
{
    volatile word loadaddr;             // Load address $e0-$e1
    volatile word loadaddroffs;         // Load address offset $e2-$e3
    volatile word decdest;              // Decompression dest pointer $e4-$e5 -- see comment above
};
#define krillzp (*((struct KRILLZP *)KRILL_ZPSTART))

struct KRILLVARS
{
    char filename[16];
    unsigned loadaddr;
    char cr;
    char oldcr;
    char error;
};
extern volatile struct KRILLVARS krillvar;

#pragma compile("krill.c")

#endif