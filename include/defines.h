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

-   Tokra: For the optimal VDC registry settings for 80x50 and 80x70 textmodes,
    and for the VDC-FLI (480x252, 8x1 colour cells) and VDC-IMONO (720x700,
    interlace monochrome) register settings, from "VDC Mode Mania":

    https://github.com/xahmol/vdcmodemania-oscar64/tree/main/original/v12

-   Michael Kircher: dithering/colour-cell technique (per-8-pixel-cell
    brute-force background/foreground search with Floyd-Steinberg error
    diffusion) studied from his 2011 vdc_quant.c/hfli_quant.c converters,
    bundled unmodified in this repo at original/v12/converters/source/ for
    reference. tools/vdc_convert.py is an independent reimplementation of
    the same published technique (those files carry no license permitting
    reuse of the code itself).

-   "Aegina sunset" photograph by Jebulon, CC0 1.0 Universal (public domain
    dedication), used as the source picture for the VDC-FLI demo section:

    https://commons.wikimedia.org/wiki/File:Aegina_sunset.jpg

-   "Eurasian Eagle Owl" photograph by Peter K Burian, CC BY-SA 4.0, used as
    the source picture for the VDC-IMONO demo section:

    https://commons.wikimedia.org/wiki/File:Eurasian_Eagle_Owl.jpg

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

#ifndef __DEFINES_H_
#define __DEFINES_H_

// Memory map bank 1
#define MEM_SID 0x2000
#define MEM_SCREEN 0x4000
#define MEM_CHARSET 0xC000

// "Maniac" by Paul Kleimeyer, 1983, Access Software Inc. --
// https://csdb.dk/release/?id=238071 /
// https://hvsc.csdb.dk/MUSICIANS/K/Kleimeyer_Paul/Maniac.sid -- relocated
// from its native $7580 (init)/$7587 (play) to this project's target load
// address via tools/sidreloc (Linus Akesson, MIT license --
// https://www.linusakesson.net/software/sidreloc/, vendored at
// tools/sidreloc/, see tools/sidreloc/CREDIT.md):
//
//   tools/sidreloc/sidreloc -v -p 20 -z 80-df Maniac.sid maniac_reloc.sid
//
// Result: init $2080, play $2087, zero page $fb/$fc -> $80/$81 (clear of
// both Oscar64's own default zeropage, $f7-$ff, and Krill's loader,
// $e0-$f5). sidreloc's own emulated verification (~33 simulated minutes of
// playback): 0% bad pitches, 0% bad pulse widths. assets/music.prg is
// maniac_reloc.sid's PSID payload with the PSID header stripped (payload
// already starts with its own 2-byte $2080 load-address prefix, i.e. it's
// already a valid PRG -- no repacking needed).
//
// HISTORY: this exact tune was tried once before this session and
// abandoned after a crash, then hand-patched (12 unrelocated
// absolute-address references found and fixed) with the crash still
// persisting -- at the time this looked like a structural relocation
// problem sidreloc's own byte-level scan couldn't fully resolve (likely
// data-table pointers indistinguishable from code), so the project moved
// to "Faded" (GoatTracker, compiled straight from source, needing no
// relocation at all) instead, which worked. That diagnosis turned out to
// be wrong: the actual crash, found once "Faded" was wired in and *still*
// crashed the same way, was raster_irq_playframe() (vdc_raster.c) living
// in the main program's ordinary (bank-0-only) code segment instead of the
// low/common-RAM segment banking.c's krill_init()/sid_music_init()/etc.
// use -- BNK_1_IO switches to a genuinely different physical 64KB RAM bank,
// outside the 8KB common-RAM window bnk_init() sets up (xmmu.rcr=0x06,
// banking.c), so a function's own remaining code physically vanishes the
// instant it switches banks mid-execution. Fixed by moving
// raster_irq_playframe() into the bcode1/bdata1/bbss1 segment (see its own
// comment in vdc_raster.c) plus a defensive `sei` in sid_music_init() --
// both entirely unrelated to which tune was loaded. With that infrastructure
// bug fixed, Maniac.sid (this file, unpatched beyond sidreloc's own
// automated relocation) plays correctly, confirming the original crash was
// never actually a relocation problem.
#define SIDINIT 0x2080
#define SIDPLAY 0x2087

// References to steering chars
#define CH_CURS_UP 145    // Petscii control code for Cursor Up
#define CH_CURS_DOWN 17   // Petscii control code for Cursor Down
#define CH_CURS_LEFT 157  // Petscii control code for Cursor Left
#define CH_CURS_RIGHT 29  // Petscii control code for Cursor Right
#define CH_PI 222         // Petscii control code for Pi
#define CH_HOME 19        // Petscii control code for Home (HOME)
#define CH_CLEAR 147      // Petscii control code for Clear (SHIFT+HOME)
#define CH_DEL 20         // Petscii control code for delete (DEL)
#define CH_INS 148        // Petscii control code for Insert (SHIFT+DEL)
#define CH_ENTER 13       // Petscii control code for enter (RETURN)
#define CH_STOP 3         // Petscii control code for stop (STOP)
#define CH_RUN 10         // Petscii control code for run (SHIFT+STOP)
#define CH_TAB 9          // Petscii control code for tab (TAB)
#define CH_LIRA 92        // Petscii control code for pound sign
#define CH_ESC 27         // Petscii control code for escape (ESC)
#define CH_FONT_LOWER 14  // Petscii control code for lower case font
#define CH_FONT_UPPER 142 // Petscii control code for upper case font
#define CH_ARROW_UP 94    // Petscii control code for arrow up
#define CH_ARROW_LEFT 95  // Petscii control code for arrow left
#define CH_SPACE 32       // Petscii code and Screencode for space
#define CH_MINUS 45       // Screencode for minus
#define CH_BLACK 144      // Petscii control code for black           CTRL-1
#define CH_WHITE 5        // Petscii control code for white           CTRL-2
#define CH_DRED 28        // Petscii control code for dark red        CTRL-3
#define CH_LCYAN 159      // Petscii control code for light cyan      CTRL-4
#define CH_LPURPLE 156    // Petscii control code for light purple    CTRL-5
#define CH_DGREEN 30      // Petscii control code for dark green      CTRL-6
#define CH_DBLUE 31       // Petscii control code for dark blue       CTRL-7
#define CH_LYELLOW 158    // Petscii control code for light yellow    CTRL-8
#define CH_RVSON 18       // Petscii control code for RVS ON          CTRL-9
#define CH_RVSOFF 146     // Petscii control code for RVS OFF         CTRL-0
#define CH_DPURPLE 129    // Petscii control code for dark purple     C=-1
#define CH_DYELLOW 149    // Petscii control code for dark yellow     C=-2
#define CH_LRED 150       // Petscii control code for light red       C=-3
#define CH_DCYAN 151      // Petscii control code for dark cyan       C=-4
#define CH_DGREY 152      // Petscii control code for dark grey       C=-5
#define CH_LGREEN 153     // Petscii control code for light green     C=-6
#define CH_LBLUE 154      // Petscii control code for light blue      C=-7
#define CH_LGREY 155      // Petscii control code for light grey      C=-8

#endif // __DEFINES_H_