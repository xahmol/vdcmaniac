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

#ifndef VDC_TEXTSCROLLERL_H
#define VDC_TEXTSCROLLERL_H

#include "vdc_win.h"

// Function prototypes
void txtscr_bigfont_init(struct TXTSCRBigFont *settings, char scr, char *sp, char width, char ch_width, char ch_height, char ch_num, char *color);
void txtscr_bigfont_printchar(struct TXTSCRBigFont *settings, char ch, char x, char y, char w);
void txtscr_scroller_init(struct TXTSCRScrollText *settings, struct TXTSCRBigFont *bigfont, char *textscr, char xs, char ys, char xw, char border);
void txtscr_scroll_do(struct TXTSCRScrollText *settings);

// Global vars
struct TXTSCRBigFont {
    char num;
    char cr;
    unsigned line_incr;
    char width;
    char height;
    char *address[91];
    char color[8];
};
struct TXTSCRScrollText {
    char *textscr;
    struct TXTSCRBigFont *bigfont;
    struct VDCWin win;
    unsigned count_char;
    char count_col;
    char count_softx;
};

// Cupid PETSCII font scroller: per-character-cell horizontal scroll (via
// vdcwin_scroll_left(), VDC hardware block-copy -- no wide virtual buffer,
// no pixel-fine VDCR_HSCROLL) with a subtle per-letter vertical sine
// bounce. Glyph data/geometry/credit: see vdc_textscroller.c's own header
// comment above the font tables.
#define CUPID_FONT_H 5 // glyph height in text rows
// Scroll/render window height: CUPID_FONT_H + 2*max|cupid_sin_row|
// clearance. Was 9 (for the original +/-2 sine amplitude); cupid_sin_row
// was later capped to +/-1 (live feedback: the bounce was too strong) but
// this wasn't shrunk to match until also live-diagnosed as unnecessary
// per-frame VDC bus work. 7 is the exact fit for +/-1.
#define CUPID_BAND_H 7
#define CUPID_BASE_OFFSET 1 // row within the band the glyph's own row 0 sits at when row_offset==0
// Total per-frame steps txtscr_cupid_render_letter_step() needs to render
// one whole letter (CUPID_BAND_H blank-row steps + CUPID_FONT_H glyph-row
// steps) -- see that function's own comment.
#define CUPID_RENDER_STEPS (CUPID_BAND_H + CUPID_FONT_H)
void txtscr_cupid_init(struct TXTSCRCupidScroll *settings, const char *textscr, char xs, char ys, char xw);
void txtscr_cupid_scroll_do(struct TXTSCRCupidScroll *settings);

struct TXTSCRCupidScroll {
    const char *textscr;  // message, NUL-terminated, loops back to the start
    struct VDCWin win;    // scroll window; height is fixed at CUPID_BAND_HEIGHT
    unsigned count_char;  // index into textscr of the letter now entering
    char count_col;       // column within that letter's glyph (0..width-1)
    char letter_phase;    // sine phase -- advances once per new letter, not per frame/column
    signed char row_offset; // this letter's sampled sine row offset, reused for all its columns
};

// Pre-rendered variant, for use with vdc_softscroll.c instead of the
// per-column live-blit pair above -- see vdc_textscroller.c's own comment
// above these two functions for why (the live-blit path's per-frame
// hardware block-copy was diagnosed as the actual source of raster
// instability this session; this path does all its VDC-bitmap-shaped work
// once, up front, and lets vdc_softscroll's cheap register-only panning
// handle the steady-state per-frame motion).
unsigned txtscr_cupid_measure(const char *text);
void txtscr_cupid_render(char cr, char *dest, const char *text, unsigned width, char total_height, char band_row, unsigned start_col);
unsigned char txtscr_cupid_letter_width(unsigned char ch);
void txtscr_cupid_render_letter_step(char cr, char *dest, unsigned width, char total_height, char band_row, unsigned col, unsigned char ch, unsigned char letter_phase, unsigned char step);

#pragma compile("vdc_textscroller.c")

#endif