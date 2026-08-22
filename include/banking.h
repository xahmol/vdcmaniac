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

#ifndef BANKING_H
#define BANKING_H

// Needed so banking.c's own pragma-compiled body (below) can see SIDINIT --
// #pragma compile() only sees a header's own local preprocessor state, not
// whatever a sibling header included earlier in some other file's include
// chain (confirmed empirically: krill.h's own KRILL_LOADRAW/KRILL_LOADCOMPD
// work in krill.c because they're #defined in krill.h itself, but SIDINIT
// living only in defines.h was invisible to banking.c until this include
// was added).
#include "defines.h"

// Defines for using Oscar64 fast load functions
#define FLOSSIEC_MAXFILES 10 // Maximum files of assets to map for fast loading

// Defines for MMU modes, MMU $FF00 configuration values
#define BNK_DEFAULT 0x0e
#define BNK_CHARROM 0x01
#define BNK_0_FULL 0x3f
#define BNK_1_FULL 0x7f
#define BMK_0_IO 0x3e
#define BNK_1_IO 0x7e

// Defines for scroll directions
#define SCROLL_LEFT 0x01
#define SCROLL_RIGHT 0x02
#define SCROLL_DOWN 0x04
#define SCROLL_UP 0x08

// Function Prototypes

// Not in overlay
void load_overlay(const char *fname);
void bnk_init();
void bnk_exit();
char getcurrentdevice();

// In overlay
__noinline char bnk_readb(char cr, volatile char *p);
__noinline unsigned bnk_readw(char cr, volatile unsigned *p);
__noinline unsigned long bnk_readl(char cr, volatile unsigned long *p);
__noinline void bnk_writeb(char cr, volatile char *p, char b);
__noinline void bnk_writew(char cr, volatile unsigned *p, unsigned w);
__noinline void bnk_writel(char cr, volatile unsigned long *p, unsigned long l);
__noinline void bnk_memcpy(char dcr, volatile char *dp, char scr, volatile char *sp, unsigned size);
__noinline void bnk_memset(char cr, volatile char *p, char val, unsigned size);
__noinline void bnk_cpytovdc(unsigned vdcdest, char scr, volatile char *sp, unsigned size);
__noinline void bnk_cpyfromvdc(char dcr, volatile char *dp, unsigned vdcsrc, unsigned size);
__noinline void bnk_redef_charset(unsigned vdcdest, char scr, volatile char *sp, unsigned size);
__noinline bool bnk_load(char device, char bank, const char *start, const char *fname);
__noinline bool bnk_save(char device, char bank, const char *start, const char *end, const char *fname);
__noinline void sid_music_init(char is_ntsc);
__noinline void sid_resetsid();
__noinline void sid_play_frame_foreground();

// How many "logical frames" a fallback-using caller has entered so far --
// incremented once per frame boundary by each such caller (raster_bar_begin(),
// fli_color_demo()'s own loop top), compared against sid_music_framecount
// (actual plays so far, from EITHER path) by sid_play_frame_foreground()'s
// own fallback to decide whether the interrupt-driven path has fallen
// behind. See that function's own comment (banking.c) for the full
// mechanism, including why a self-correcting counter comparison is used
// here rather than a boolean flag. Not volatile, matching
// sid_music_framecount's own existing precedent (also written from both
// interrupt and foreground context without volatile) -- 6502 has no
// interrupt re-entrancy to race against once one starts.
extern unsigned sid_expected_framecount;

// Frame counter incremented once per actual SIDPLAY call inside
// raster_irq_playframe() (vdc_raster.c) -- used there to restart the tune
// (re-run SIDINIT) after SID_RESTART_FRAMES calls, since Maniac.sid's own
// composed length/loop point isn't known precisely and this project has no
// per-tune "has the song ended" telemetry. Counts play calls, not IRQs --
// correct even now that the rate accumulator below can call SIDPLAY zero,
// one, or two times per IRQ. Declared here (not local to
// raster_irq_playframe()) so it lives in banking.c's existing
// bcode1/bdata1/bbss1 segment next to the rest of the SID state, matching
// this project's own "anything touched from interrupt context must live in
// common RAM" rule.
extern unsigned sid_music_framecount;
#define SID_RESTART_FRAMES 15000 // ~4-5 minutes depending on tune/machine standard match. Attention point: not measured against the tune's actual composed length -- a safe margin, not a precise one; narrow it if the real length is ever determined

// Rate accumulator state for retiming SID playback to the tune's own native
// rate when it doesn't match the host machine's VIC-frame rate (defines.h's
// SID_TUNE_USES_CIA_SPEED/SID_TUNE_IS_NTSC, set once by sid_music_init()
// below; consumed every IRQ by raster_irq_playframe(), vdc_raster.c). Must
// live in common RAM (bbss1) for the same reason sid_music_framecount does
// -- raster_irq_playframe() runs partly with mmu.cr=BNK_1_IO, a genuinely
// different physical RAM bank. sid_rate_accum is signed: it holds a
// positive debt when the tune's native rate is faster than the host's
// frame rate (extra SIDPLAY calls needed to catch up) or a negative debt
// when slower (calls need to be skipped to hold the tune back) -- see
// vdc_raster.c's raster_irq_playframe() for the consuming logic.
extern int sid_rate_accum;
extern int sid_rate_inc;
#define SID_RATE_SCALE 10000
#define SID_RATE_DELTA_NTSC_ON_PAL 1935   // NTSC-tempo tune (59.826Hz) on a PAL machine (50.1245Hz): ratio 1.19354, error -0.03%
#define SID_RATE_DELTA_PAL_ON_NTSC (-1622) // PAL-tempo tune (50.1245Hz) on an NTSC machine (59.826Hz): ratio 0.83784, error +0.02%

// Fastload
__noinline bool fastload_mapdir(const char * fnames);
__noinline bool fastload_load(char bank, const char *start, char fnumber);

// Global variables
extern char bootdevice;

#pragma compile("banking.c")

#endif