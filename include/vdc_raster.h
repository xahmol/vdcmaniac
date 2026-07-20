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

#ifndef VDC_RASTER_H
#define VDC_RASTER_H

void raster_waitline(char rasterline);
void raster_synch();
void raster_time();

extern char rastertime;

// Higher-level busy-wait bar-drawing primitives, built on raster_synch()/
// raster_waitline() above. Bracket one synced sweep per frame with
// raster_bar_begin()/raster_bar_end(); in between, draw any mix of single
// fixed-colour lines (raster_bar_line()) and palette-driven runs
// (raster_bar_segment()) -- see raster_lib_manual.md for worked examples
// (raster_place_test(), raster_bar(), title_screen() in src/main.c).
void raster_bar_begin();
void raster_bar_line(char line, char color);
char raster_bar_segment(char line, const char *colors, unsigned char count);
void raster_bar_end();

// Advances pos by *direction (flipping it at low/high) -- a reusable
// position updater for a bar that bounces between two rasterlines.
char raster_bar_bounce(char pos, char low, char high, signed char *direction);

// Declarative multi-bar drawing: describe several independent bars (static
// colours and/or palette-driven runs, contiguous or not) as data and draw
// them all in one synced sweep via raster_bar_draw_list(), instead of
// hand-sequencing raster_bar_begin()/line()/segment()/end() calls -- use
// the latter directly when the layout is computed at runtime rather than
// fixed.
enum RasterBarKind
{
    RASTERBAR_LINE,   // single fixed colour -- color/startline apply, length ignored
    RASTERBAR_SEGMENT // palette-driven run -- colors/length/startline apply
};

struct RasterBarDef
{
    char startline;      // rasterline to begin this bar at
    unsigned char length; // number of lines (RASTERBAR_SEGMENT only)
    char kind;            // enum RasterBarKind
    char color;           // fixed colour (RASTERBAR_LINE only)
    const char *colors;   // palette, length entries (RASTERBAR_SEGMENT only)
};

void raster_bar_draw_list(const struct RasterBarDef *bars, unsigned char count);

// Measure the actual CPU cycles per VDC rasterline on this specific machine
// (varies slightly by 8563/8568 revision and PAL/NTSC) and update
// raster_timer_reload / raster_cycles_per_line_x1000 from the result.
// Call once at startup, after vdc_init(), before relying on raster_synch().
void raster_calibrate();

extern char raster_timer_reload;             // calibrated Timer A reload for raster_synch()
extern unsigned raster_cycles_per_line_x1000; // measured cycles/line, fixed-point x1000 (e.g. 63056 = 63.056)

// CIA1-driven raster colour + music IRQ: an interrupt-based alternative to
// raster_synch()/raster_waitline()'s CIA2 busy-wait. The CPU is free to do
// other work between colour writes instead of blocking for a whole effect.
// Installs directly on the hardware IRQ vector ($fffe/$ffff) while banking
// out KERNAL/BASIC/character ROM (I/O stays visible) -- NOT chained through
// the KERNAL's $314 soft vector, which was confirmed (live, in VICE) to not
// reliably hold a valid address in this project's boot context. Supersedes
// banking.c's sid_startmusic() while active -- do not run both at once,
// they both drive CIA1. While active:
//   - KERNAL calls (GETIN/vdcwin_checkch(), file loading, etc.) will not
//     work -- use keyb_poll()/key_pressed() (<c64/keyboard.h>) for input.
//   - Don't touch any VDC register ($d600/$d601, or the vdc_prints()/
//     vdc_write() family that use them) from foreground code: the IRQ also
//     writes them, and racing it can leave the VDC's addressing in a state
//     where a foreground "wait for ready" spins forever.
//
// colortable/tablelength: a per-line ink/paper byte (VDCR_COLOR format,
// background in bits 0-3, foreground in bits 4-7) for each rasterline of
// the effect, replayed once per frame. linespertick: how many table
// entries (rasterlines) get written per interrupt call -- the interrupt
// entry/exit cost is roughly fixed per call, so batching several lines
// per tick matters a lot for how much spare CPU time is left over; start
// around 4 and adjust based on what's actually needed/measured.
// musicenabled: pass non-zero only once a tune has already been loaded and
// initialized the same way sid_startmusic() expects (init call to $2000 in
// Bank 1) -- with nothing loaded there, calling into $2003 would jump into
// garbage memory. Pass 0 to drive the colour table alone.
void raster_music_irq_start(const char *colortable, unsigned tablelength, char linespertick, char musicenabled);
void raster_music_irq_stop();

#pragma compile("vdc_raster.c")

#endif