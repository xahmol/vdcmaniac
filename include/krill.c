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

#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <petscii.h>
#include <c64/kernalio.h>
#include <c64/cia.h>
#include <c64/vic.h>
#include <c128/vdc.h>
#include <c128/mmu.h>
#include "c128/vdc.h"
#include "banking.h"
#include "peekpoke.h"
#include "vdc_core.h"
#include "krill.h"

#pragma code(code)
#pragma data(data)
#pragma bss(bss)

char krill_install()
// Install Krill code on drive, return error or succes code
{
    __asm
    {
        jsr KRILL_INSTALL
        bcs krill_install_error
        lda #$00
krill_install_error:
        sta accu
    }
}

void krill_loadcode()
// Load and initialise Krill's loader code
{
    printf("loading krill's loader: installer.\n");
    load_overlay("install-c128");
    printf("loading krill's loader: resident code.\n");
    load_overlay("loader-c128");
}

char krill_load(char cr, const unsigned start, const char *fname)
// Load a raw (uncompressed) file with Krill's loader
{
    krillvar.oldcr = mmu.cr;
    krillvar.cr = cr;
    krillvar.error = 0;
    krillzp.loadaddr = start;
    strcpy(krillvar.filename,fname);
    krill_load_core();
    return krillvar.error;
}

char krill_loadcompd(char cr, const unsigned start, const char *fname)
// Load and depack a TSCrunch-compressed file with Krill's loader. The depack
// destination is taken from the asset itself: TSCrunch bakes the original
// file's PRG load address into the compressed stream, and Krill's decompressor
// restores the data there. So build each asset TSCrunched at its intended
// runtime address (idi8blogo.scrn is a $5800 PRG; idi8blkr therefore depacks
// to $5800), exactly as the raw-load path and the rest of this demo treat
// their assets.
//
// `start` is the address the asset is EXPECTED to depack to -- it must match
// the asset's own baked-in load address. It is passed for documentation /
// call-site clarity only; it is NOT used to relocate (see below).
//
// --- Two bugs were fixed here; both had been mis-analysed as a zero-page
//     initialization problem. The actual findings (static analysis against
//     krill/loader/src/decompress/tsdecomp.s and the build's own linker map,
//     July 2026): ---
//
// 1. decdest ($e4/$e5) IS initialized on the loadcompd path -- the earlier
//    "decdest is never written with MEM_DECOMP_TO_API off" conclusion was
//    wrong. It missed that tsdecomp.s aliases `tsput = decdestlo` (its line
//    25) and, in its INPLACE init loop (the `.else` branch, ~line 86:
//    `lda (tsget),y / sta tsput,y` for y=0..3), reads the first bytes of the
//    compressed stream -- which are TSCrunch's baked-in depack address -- and
//    writes them into decdest. The earlier grep only searched resident.s and
//    only for the symbol name "decdestlo", so it never saw these writes under
//    the alias "tsput" in tsdecomp.s.
//
// 2. THE CRASH. The previous "fix" called with carry SET and offset = start.
//    In offset mode the decompressor does tsput = bakedDepackAddr + offset
//    (tsdecomp.s LOADCOMPD_TO block, ~lines 92-101). For idi8blkr that is
//    $5800 (baked) + $5800 (offset) = $B000 -- and the Oscar64 C runtime
//    STACK for this build lives at $B000-$BFA4 (verified in the build's .map:
//    `b000 - bfa4 : STACK, stack`). Decompressing ~4 KB onto the C stack
//    corrupts every saved return address, so the return from the loader lands
//    on garbage, executes a $00/BRK, and drops into the KERNAL BREAK handler
//    around $C25E-$C365 -- exactly the observed crash. (This is why removing
//    krill_interrupt's KERNAL chain never helped: the crash is stack
//    corruption from a doubled destination address, not a zero-page/IRQ race.
//    The $e4 watchpoint hit was real but incidental to this crash.)
//
// Fix: call with carry CLEAR (krill_loadcompd_core below), which routes
// through resident.s's "depack to the address stored in the file" path.
// openfile zeroes loadaddroffs on that path, and the decompressor sets
// decdest from the stream header, so the asset lands at exactly its baked
// address ($5800) -- no doubling, no stack collision.
{
    (void)start; // destination comes from the asset's own header; see above
    krillvar.oldcr = mmu.cr;
    krillvar.cr = cr;
    krillvar.error = 0;
    strcpy(krillvar.filename,fname);
    krill_loadcompd_core();
    return krillvar.error;
}

// Now switch code generation to low region
#pragma code(bcode1)
#pragma data(bdata1)
#pragma bss(bbss1)

volatile struct KRILLVARS krillvar;

__asm krill_interrupt
// Krill IRQ handler, installed at the KERNAL soft vector $314 by
// krill_init() for the whole program run (not torn down until
// krill_done() at the very end of main()).
//
// HISTORY (asset-loading-roadmap.md Phase 4): this was stripped down to a
// bare `jmp $ff33` earlier in that phase's investigation, on the theory
// that the real KERNAL chain below was vestigial (this project's own music,
// raster_music_irq_start(), uses the direct hardware vector $fffe/$ffff,
// never $314) and possibly responsible for zero-page corruption seen during
// loadcompd debugging. That corruption turned out to be an unrelated bug
// (a doubled decompression destination address landing on the C stack --
// see krill_loadcompd()'s comment), so the removal never helped, and it
// broke two things at once that only showed up later:
//
// 1. An IRQ storm: nothing acknowledged VIC-II's $D019 raster-interrupt
//    flag, so the VIC's /IRQ line stayed asserted and the CPU re-took the
//    interrupt immediately on every RTI -- confirmed live via VICE's binary
//    monitor, single-stepping showed zero mainline instructions executing,
//    ever, in a tight $FF17->$0314->krill_interrupt->$FF33->RTI->$FF17 loop.
// 2. Once the storm was papered over by masking `vic.intr_enable=0` in
//    krill_init() (a dead end also tried and reverted), the demo hung
//    forever at every "press a key" prompt instead: vdcwin_checkch() reads
//    via KERNAL GETIN ($FFE4), which only sees keys the KERNAL's own
//    keyboard-scan routine has placed in its buffer -- and that scan is
//    exactly what `jsr $c024` below performs. With it removed (and/or the
//    interrupt masked off entirely), that buffer is never filled again for
//    the rest of the program.
//
// Root cause of both: `jsr $c024` (the real C128 KERNAL jiffy/keyboard-scan
// routine) was doing its own correct $D019 acknowledgment as part of normal
// system housekeeping -- a plain `lda $d019` does NOT clear the flag (that
// C64/C128 gotcha applies to CIA's ICR, not VIC-II's $D019, which clears
// only on a WRITE of a 1 bit); only $c024 was ever actually clearing it.
// Fix: restore the original chain unmodified (verified against this file's
// own pre-Phase-4 git history) and stop masking the VIC raster interrupt.
{
    jsr $c024
    bcc krillirq
    jsr $f5f8
krillirq:
    jmp $ff33
}

void krill_init()
// Initialise Krill's loader
{
    // cia1.icr = 0x7f disables CIA #1's own interrupt sources -- this is
    // NOT the C128's system-jiffy IRQ source (that's the VIC-II
    // raster-compare interrupt, per c128_reference.md), so it doesn't touch
    // that path; it's just resetting CIA #1 to a known state before Krill's
    // loader takes over cia2.pra for the IEC bus below. The VIC raster
    // interrupt itself is left enabled and unmasked -- see krill_interrupt's
    // comment: it must keep firing normally (KERNAL keyboard-scan/jiffy
    // clock depend on it), and $c024 in that handler acknowledges $D019
    // correctly on every entry.
    cia1.icr =0x7f;
    cia2.pra = 2;

    krillvar.error = krill_install();
    if (krillvar.error)
    {
        printf("error in installing. error code: %u.\n",krillvar.error);
        exit(1);
    }

    // Set new IRQ vector to Krill handler
	__asm
		{
		sei									
        lda #<krill_interrupt					
		sta $314																
        lda #>krill_interrupt					
		sta $315
		cli
		}
}

void krill_done()
// Disable Krill IRQ handler
{
    __asm 
    {
        sei								
        lda #$65				
		sta $314							
        lda #$fa
		sta $315
		cli
        jsr KRILL_UNINSTA
    }
}

void krill_load_core()
// Load a raw file with Krill's loader
{
    char filelo = (unsigned)krillvar.filename;
    char filehi = ((unsigned)krillvar.filename) >> 8;
    char error = 0;
    mmu.cr = krillvar.cr;
    __asm
        {
        ldx filelo
        ldy filehi
        sec
        jsr KRILL_LOADRAW
        bcs krill_load_error
        lda #$00
krill_load_error:
        sta error
         }
    krillvar.error = error;
    mmu.cr = krillvar.oldcr;
}

void krill_loadcompd_core()
// Load and depack a compressed file with Krill's loader. Carry is CLEAR
// before the call: per loader.inc's loadcompd contract, c=0 means "depack to
// the address stored in the file". TSCrunch bakes the asset's intended
// runtime address into the compressed stream, and the decompressor restores
// decdest ($e4/$e5) from it (tsdecomp.s INPLACE init loop). openfile also
// zeroes loadaddroffs on this path, so there is no offset added -- the asset
// lands at exactly its baked address. See krill_loadcompd()'s comment for the
// full history: the previous carry-SET-with-offset call doubled the address
// ($5800 + $5800 = $B000) straight onto the Oscar64 C stack and crashed.
//
// Not wrapped in SEI/CLI (would defeat the point of using Krill -- background
// loading so IRQ-driven music keeps playing). The earlier crash was stack
// corruption from the doubled destination, not an IRQ/zero-page race, so no
// interrupt masking is needed here.
{
    char filelo = (unsigned)krillvar.filename;
    char filehi = ((unsigned)krillvar.filename) >> 8;
    char error = 0;
    mmu.cr = krillvar.cr;
    __asm
        {
        ldx filelo
        ldy filehi
        clc
        jsr KRILL_LOADCOMPD
        bcs krill_loadcompd_error
        lda #$00
krill_loadcompd_error:
        sta error
         }
    krillvar.error = error;
    mmu.cr = krillvar.oldcr;
}

#pragma code(code)
#pragma data(data)
#pragma bss(bss)