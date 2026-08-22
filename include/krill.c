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
// Attention point: krill_loadcompd_core() below calls Krill's loader with
// carry CLEAR, per loader.inc's own loadcompd contract -- c=0 means "depack
// to the address stored in the file" (decdest is set from the compressed
// stream's own baked address; openfile zeroes loadaddroffs on this path, so
// no offset is added). Calling with carry SET instead switches to OFFSET
// mode, where the decompressor computes tsput = bakedDepackAddr + `start`
// -- if `start` equals the asset's own baked address (the natural thing to
// pass), that DOUBLES the destination address rather than confirming it.
// For an asset baked at $5800 that lands at $B000, inside this build's own
// Oscar64 C runtime stack ($B000-$BFA4 per the build's .map) -- decompressing
// onto the live C stack corrupts every saved return address. Always use the
// carry-CLEAR path for TSCrunch assets baked at their real runtime address.
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
// Attention point: `jsr $c024` (the real C128 KERNAL jiffy/keyboard-scan
// routine) must run on every entry, unmodified, for two independent
// reasons -- removing or masking it breaks both at once. First, $c024 is
// what actually acknowledges VIC-II's $D019 raster-interrupt flag: a plain
// `lda $d019` does NOT clear it (that C64/C128 gotcha applies to CIA's
// ICR, not VIC-II's $D019, which clears only on a WRITE of a 1 bit) --
// without that acknowledgment the VIC's /IRQ line stays asserted and the
// CPU re-takes the interrupt immediately on every RTI (an IRQ storm, zero
// mainline code ever runs). Second, $c024 is also what refills the
// KERNAL's own keyboard buffer -- `vdcwin_checkch()` reads via KERNAL
// GETIN ($FFE4), which only sees keys that scan has placed in the buffer.
//
// Attention point: don't nest additional calls (e.g. a `jsr` to play SID
// music) directly inside this handler -- it's already reached several
// JSRs deep through the KERNAL's own dispatch chain, and the added stack
// depth conflicts with Krill's own protocol timing. SID playback is
// instead chained in *ahead* of this handler via a separate trampoline
// (sid_music_interrupt, banking.c) that REPLACES this at $314, plays one
// SID frame via an ordinary JSR/RTS (fully unwound, no added depth here),
// then JMPs (not JSRs -- no stack growth) to this function's own address,
// saved at install time -- so krill_interrupt itself always runs at its
// original, unmodified call depth. Matches Oscar64Test's own
// sid_interrupt/sid_startmusic() pattern (banking.c there).
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
// lands at exactly its baked address. See krill_loadcompd()'s own comment
// for why carry SET (offset mode) must be avoided here.
//
// Not wrapped in SEI/CLI (would defeat the point of using Krill -- background
// loading so IRQ-driven music keeps playing). No interrupt masking is needed
// for this call.
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