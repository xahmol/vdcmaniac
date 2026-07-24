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
char krill_loadcompd(char cr, const char *fname);

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
//   $e2-$e3 loadaddroffslo/hi  -- loadcompd: destination OFFSET added to the
//                                 file's own embedded depack address (LOAD_TO_API)
// Not contiguous with the old raw-only ($f5-$fc) layout -- loadaddroffslo/hi
// did not exist there, so endaddrlo/hi immediately followed loadaddrhi. That
// endaddr field is dropped here: END_ADDRESS_API is disabled in our config
// (never populated by the loader) and nothing in this codebase read it.
struct KRILLZP
{
    volatile word loadaddr;             // Load address $e0-$e1
    volatile word loadaddroffs;         // Load address offset $e2-$e3
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