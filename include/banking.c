/*
Oscar64 VDC function library

Written in 2024 by Xander Mol

https://github.com/xahmol/Oscar64Test

https://www.idreamtin8bits.com/

Code and resources from others used:

-   Oscar64 cross compiler

	https://github.com/drmortalwombat/oscar64

	Many thanks also to https://github.com/drmortalwombat to provide extrordinary support and tips for making this and adapting Oscar64 to my needs faster than I could ask it.

-   Screens used in the demo made with my own VDC Screen Editor.

	https://github.com/xahmol/VDCScreenEdit

-   Commodore logo charset created using CharPad Pro.

	https://subchristsoftware.itch.io/c64-pro-editions

-   C128 Programmers Reference Guide: For the basic VDC register routines and VDC code inspiration

	http://www.zimmers.net/anonftp/pub/cbm/manuals/c128/C128_Programmers_Reference_Guide.pdf

-   Tokra: For the optimal VDC registry settings for 80x50 and 80x70 textmodes

-   Scott Hutter - VDC Core functions inspiration:

	https://github.com/Commodore64128/vdc_gui/blob/master/src/vdc_core.c

	(used as starting point)

-   Scott Robison for teaching me how o create a C128 disk boot sector

-   Francesco Sblendorio - Screen Utility: used for inspiration:

	https://github.com/xlar54/ultimateii-dos-lib/blob/master/src/samples/screen_utility.c

-   DevDef: Commodore 128 Assembly - Part 3: The 80-column (8563) chip

	https://devdef.blogspot.com/2018/03/commodore-128-assembly-part-3-80-column.html

-   Tips and Tricks for C128: VDC

	http://commodore128.mirkosoft.sk/vdc.html

-   Steve Goldsmith - C3L Commodore 128 CP/M C Library

	https://github.com/sgjava/c3l

	(Used for inspiration and for the text wrap and random sentence generator functions)

-   Bart van Leeuwen: For inspiration and advice while coding. Also for providing the excellent Device Manager ROM to make testing on real hardware very easy

-   Original windowing system code on Commodore 128 by unknown author.

-   Tested using real hardware (C128D and C128DCR) plus VICE.

The code can be used freely as long as you retain a notice describing original source and author.

THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL, BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!
*/

#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <petscii.h>
#include <c64/kernalio.h>
#if defined(FLOSSIEC)
#include <c64/flossiec.h>
#endif
#include <c128/vdc.h>
#include <c128/mmu.h>
#include "c128/vdc.h"
#include "banking.h"
#include "peekpoke.h"
#include "vdc_core.h"
#include "vdc_raster.h"

// Section and region for low memory area overlay
#pragma overlay(vdcelmc, 1)
#pragma section(bcode1, 0)
#pragma section(bdata1, 0)
#pragma section(bbss1, 0)
#pragma region(bank1, 0x1300, 0x1b00, , 1, { bcode1, bdata1, bbss1  } )

#pragma code(code)
#pragma data(data)
#pragma bss(bss)

char bootdevice;

#if defined(FLOSSIEC)
struct floss_blk blks[FLOSSIEC_MAXFILES];
#endif

char getcurrentdevice()
// Return last used device number for IO operations. Default on 8 if still zero.
{
	// Feading zeropage address containg current device number ($BA)
	char curunit = *(char *)0xba;

	// Default on 8 if still zero
	if (!curunit)
	{
		curunit = 8;
	}
	return curunit;
}

void load_overlay(const char *fname)
// Loading an overlay file
{
	krnio_setbnk(0, 0);
	krnio_setnam(fname);
	printf("loading: %s\n", fname);
	if (!krnio_load(1, bootdevice, 1))
	{
		printf("loading overlay file failed.\n");
		printf("status: %d\n", krnio_pstatus[1]);
		exit(1);
	}
}

void bnk_init()
// Initialise banking functions in low memory
{
	// Get device ID used to load the program
	bootdevice = getcurrentdevice();
	printf("bootdevice: %u\n", bootdevice);

	// Set 8Kb shared memory size
	// So set MMU Ram Configuration Register at:
	// - bit 0-1:   %10 for 8 KB common RAM
	// - bit 2-3:   %01 for bootom of RAM bank 0 is common
	// - bit 7:     $0 for VIC RAM in bank 0
	xmmu.rcr = 0x06;

	// Load overlay in low memory
	printf("loading low memory code.\n");
	load_overlay("vdcelmc");

#if defined(FLOSSIEC)
	// Initialize fast load drive code
	printf("initialize fast load drive code.\n");
	flossiec_init(bootdevice);
#endif
}

void bnk_exit()
// Retsore to default situtation for shared memory
{
	// Set 8Kb shared memory size
	// So set MMU Ram Configuration Register at:
	// - bit 0-1:   %00 for 1 KB common RAM
	// - bit 2-3:   %01 for bootom of RAM bank 0 is common
	// - bit 7:     $0 for VIC RAM in bank 0
	xmmu.rcr = 0x04;

#if defined(FLOSSIEC)
	flossiec_shutdown();
#endif
}

#if defined(FLOSSIEC)
bool fastload_mapdir(const char *fnames)
// Map assets to fastload via a comma separated list of filenames.
// File must be on the disk present at the same IEC devide ID as the bootdevice
{
	if(!flossiec_mapdir(fnames, blks))
	{
		printf("Mapping assets for fastloading failed!\n");
		exit(1);
	}

	// Debug
	//for(char i=0;i<7;i++)
	//{
	//	printf("file number %u: track %3u sector %3u\n",i,blks[i].track,blks[i].sector);
	//}
}
#endif

// Now switch code generation to low region
#pragma code(bcode1)
#pragma data(bdata1)
#pragma bss(bbss1)

char bnk_readb(char cr, volatile char *p)
// Function to read a byte from given address with specified banking config register value
{
	char old = mmu.cr;
	mmu.cr = cr;
	char c = *p;
	mmu.cr = old;
	return c;
}

unsigned bnk_readw(char cr, volatile unsigned *p)
// Function to read a word from given address with specified banking config register value
{
	char old = mmu.cr;
	mmu.cr = cr;
	unsigned w = *p;
	mmu.cr = old;
	return w;
}

void bnk_writeb(char cr, volatile char *p, char b)
// Function to write a byte to given address with specified banking config register value
{
	char old = mmu.cr;
	mmu.cr = cr;
	*p = b;
	mmu.cr = old;
}

void bnk_writew(char cr, volatile unsigned *p, unsigned w)
// Function to write a word to given address with specified banking config register value
{
	char old = mmu.cr;
	mmu.cr = cr;
	*p = w;
	mmu.cr = old;
}

void bnk_memcpy(char dcr, volatile char *dp, char scr, volatile char *sp, unsigned size)
// Menory copy of size bytes from source bank/address to destination source/address
{
	char old = mmu.cr;
	while (size > 0)
	{
		mmu.cr = scr;
		char c = *sp++;
		mmu.cr = dcr;
		*dp++ = c;
		size--;
	}
	mmu.cr = old;
}

void bnk_memset(char cr, volatile char *p, char val, unsigned size)
// Fill memory from bank/address with value, size is bytes to fill
{
	char old = mmu.cr;
	mmu.cr = cr;
	while (size > 0)
	{
		*p++ = val;
		size--;
	}
	mmu.cr = old;
}

void bnk_cpytovdc(unsigned vdcdest, char scr, volatile char *sp, unsigned size)
// Menory copy of size bytes from source bank/address to destination VDC address
{
	char old = mmu.cr;
	mmu.cr = BNK_DEFAULT;
	vdc_mem_addr(vdcdest);

	while (size > 0)
	{
		mmu.cr = scr;
		char c = *sp++;
		mmu.cr = BNK_DEFAULT;
		vdc_write(c);
		size--;
	}
	mmu.cr = old;
}

void bnk_cpyfromvdc(char dcr, volatile char *dp, unsigned vdcsrc, unsigned size)
// Menory copy of size bytes from source VDC address to destination bank/address
{
	char old = mmu.cr;
	while (size > 0)
	{
		mmu.cr = BNK_DEFAULT;
		char c = vdc_mem_read_at(vdcsrc++);
		mmu.cr = dcr;
		*dp++ = c;
		mmu.cr = BNK_DEFAULT;
		size--;
	}
	mmu.cr = old;
}

void bnk_redef_charset(unsigned vdcdest, char scr, volatile char *sp, unsigned size)
// Function to copy charset definition from normal memory to VDC
// Input: Source normal memory address and bank config where charset defintion resides,
//		  Destination address in VDC memory,
//		  Numbers of characters to redefine.
// Takes charset definition of 8 bytes per character as input.
// Destination address should be the location pointed as character definition address
{
	char old = mmu.cr;
	mmu.cr = BNK_DEFAULT;
	vdc_mem_addr(vdcdest);

	while (size > 0)
	{
		// Copy charset data per char
		for (char i = 0; i < 8; i++)
		{
			mmu.cr = scr;
			char c = *sp++;
			mmu.cr = BNK_DEFAULT;
			vdc_write(c);
		}
		// Add 8 byte zero padding needed for charsets of 8 bytes high
		for (char i = 0; i < 8; i++)
		{
			vdc_write(0);
		}
		size--;
	}
	mmu.cr = old;
}

// Saved $314/$315 -- whatever krill_init() (krill.c) already installed
// there (krill_interrupt's own address) by the time sid_music_init() below
// runs. sid_music_interrupt's own trailing jmp is self-modified from this
// every time it fires, so it chains to krill_interrupt correctly regardless
// of exactly where the linker placed it. Matches Oscar64Test's own
// sid_irq[2] (banking.c there) byte-for-byte in spirit.
char sid_krill_irq_saved[2];

// See its own comment in banking.h.
unsigned sid_music_framecount;

__asm sid_music_interrupt
// SID play + Krill chain trampoline. Installed at $314 by sid_music_init()
// below, AFTER krill_init() has already installed krill_interrupt there --
// this REPLACES krill_interrupt as the direct $314 target; krill_interrupt
// itself is reached only via the jmp at the end, never modified.
//
// Plays one SID frame via an ordinary `jsr raster_irq_playframe`
// (vdc_raster.c) -- ordinary JSR/RTS, fully unwound before this trampoline
// continues -- then jmps (not jsrs: adds no stack depth at all) to
// krill_interrupt's own address, self-modified into the placeholder below
// from sid_krill_irq_saved.
//
// This exact structure matters: an earlier attempt called
// raster_irq_playframe() with a nested jsr *from inside* krill_interrupt
// itself (one more call depth on top of a handler already reached several
// JSRs deep through the KERNAL's own dispatch chain) and crashed
// immediately, every time, regardless of which SID tune was used --
// something about the added stack depth conflicts with Krill's own
// protocol timing (see krill_interrupt's own comment, krill.c). Chaining
// this way instead -- play, fully return, *then* jmp onward -- means
// krill_interrupt always runs at its original, unmodified call depth,
// exactly as if this trampoline didn't exist. This is the same mechanism
// Oscar64Test's own sid_interrupt/sid_startmusic() (banking.c there)
// already proved works correctly alongside Krill -- reused rather than
// reinvented, once the nested-jsr attempt here confirmed why it's built
// this way.
{
    jsr raster_irq_playframe

    lda sid_krill_irq_saved
    sta sid_krill_chain + 1
    lda sid_krill_irq_saved + 1
    sta sid_krill_chain + 2
sid_krill_chain:
    jmp $ff33
}

void sid_music_init()
// Initialise SID music: bank to where the tune lives (matches
// raster_irq_playframe()'s own bank-switch, defines.h's SIDINIT/SIDPLAY),
// reset the SID chip, call the tune's init entry point (song 0), then chain
// sid_music_interrupt in ahead of krill_interrupt at $314 -- see that
// function's own comment for the full trampoline mechanism and why it's
// structured this way. Call once, after krill_init() has already installed
// krill_interrupt (krill.c) -- this saves whatever's there at that point
// and chains onward to it, so ordering matters.
//
// sei/cli bracket the bank-switch below for the same reason
// raster_irq_playframe() (vdc_raster.c) does: BNK_1_IO banks out KERNAL ROM
// at $C000-$FFFF, where the hardware IRQ vector's real target lives -- an
// interrupt landing mid-call without this would fetch that vector from
// bank-1 RAM garbage instead of ROM and crash. Confirmed live: this
// function originally shipped without the bracketing and crashed on the
// very first run (screen corruption, PC in the weeds) -- a single missed
// spot, not a new failure mode; raster_irq_playframe() already had it from
// the start per the Phase 5 safety assessment.
{
	char old = mmu.cr;
	__asm { sei }
	mmu.cr = BNK_1_IO;
	sid_resetsid();
	__asm
	{
		lda #$00
		jsr SIDINIT
	}
	mmu.cr = old;
	sid_music_framecount = 0;

	// Save krill_interrupt's own address (already installed by krill_init()
	// by this point), then chain sid_music_interrupt in ahead of it.
	__asm
	{
		lda $314
		sta sid_krill_irq_saved
		lda #<sid_music_interrupt
		sta $314
		lda $315
		sta sid_krill_irq_saved + 1
		lda #>sid_music_interrupt
		sta $315
	}
	__asm { cli }
}

void sid_resetsid()
// Resets SID and silences presently played notes
{
	// Reset SID
	__asm
	{
		ldx #$18
        lda #$00
rst1:    
        sta $d400,x
        dex
        bpl rst1
 
        lda #$08
        sta $d404
        sta $d40b
        sta $d412 
        ldx #$03
rst2:       
        bit $d011
        bpl *-3
        bit $d011
        bmi *-3
        dex
        bpl rst2
 
        lda #$00
        sta $d404
        sta $d40b
        sta $d412
        lda #$00
        sta $d418
	}
}

bool bnk_load(char device, char bank, const char *start, const char *fname)
{
	krnio_setbnk(bank, 0);
	krnio_setnam(fname);
	__asm
	{
		lda	#1
		ldx	device
		ldy #0		
		jsr	$ffba // setlfs
		
		lda #0
		ldx start
		ldy start+1
		jsr	$FFD5 // load

		lda #0
		bcs W1
		lda #1
	W1: sta accu
	}
}
#pragma native(bnk_load)

bool bnk_save(char device, char bank, const char *start, const char *end, const char *fname)
{
	krnio_setbnk(bank, 0);
	krnio_setnam(fname);
	return krnio_save(device, start, end);
}

// Fast load helper routines
#if defined(FLOSSIEC)
bool fastload_load(char bank, const char *start, char fnumber)
// Fastload a file
{
	char* dp = (char*)start;
	char old = mmu.cr;

	fastmode(0);

	if (!flossiec_open(blks[fnumber].track, blks[fnumber].sector))
	{
		return 0;
	}

	// Skip first two bytes that are the load address
	flossiec_get();
	flossiec_get();

	mmu.cr = bank;
	while (!flossiec_eof())
	{
		*dp++ = flossiec_get();
	}
	mmu.cr = old;

	flossiec_close();

	fastmode(1);

	return (dp == start);
}
#endif

#pragma code(code)
#pragma data(data)
#pragma bss(bss)