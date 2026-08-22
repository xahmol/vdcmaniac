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

-   Font: "Small Round PETSCII Font" by Cupid (https://csdb.dk/release/?id=188169).
    Glyph data (screen-code + colour arrays) and the letter_start[]/letter_width[]/
    letter_idx() lookup scheme, plus the sin_row[] sine-bounce table and its
    "resample only at a glyph's first column" technique, ported from the
    author's own UltimateDemo2026 project (src/scroller.c), adapted here from a
    per-frame full-band VIC-II redraw to a per-character-column VDC append
    (see txtscr_cupid_scroll_do() below) -- real, stock-speed C128 VDC access
    is far more expensive per byte than VIC-II's directly-mapped screen RAM,
    so this version only ever touches the one new column being scrolled in,
    never redraws the whole band.

The code can be used freely as long as you retain a notice describing original source and author.

THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL, BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!
*/

#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <petscii.h>
#include <c128/vdc.h>
#include "vdc_core.h"
#include "banking.h"
#include "vdc_win.h"
#include "vdc_textscroller.h"

// Big-font scroller (txtscr_bigfont_*/txtscr_scroller_*): a simple
// character-by-character scrolling text banner using a large custom
// font stored as raw screen-code tiles. Not currently called from
// main.c -- superseded by the Cupid font scroller below -- kept for
// possible future use.

void txtscr_bigfont_init(struct TXTSCRBigFont *settings, char scr, char *sp, char width, char ch_width, char ch_height, char ch_num, char *color)
// Initialise big font
{
    char i;

    settings->num = ch_num;
    settings->cr = scr;
    settings->line_incr = width;
    settings->width = ch_width;
    settings->height = ch_height;

    for (i = 0; i < ch_num; i++)
    {
        settings->address[i] = sp + ((i / (width / ch_width)) * (width * ch_height)) + ((i % (width / ch_width)) * ch_width);
    }
    for (i = 0; i < ch_height; i++)
    {
        settings->color[i] = *(color + i);
    }
}

void txtscr_bigfont_printchar(struct TXTSCRBigFont *settings, char ch, char x, char y, char col)
// Print a big font char
{
    char *address = settings->address[ch - 32];
    char line;

    for (line = 0; line < settings->height; line++)
    {
        vdc_printc(x, y + line, bnk_readb(settings->cr, address + col), settings->color[line]);
        address += settings->line_incr;
    }
}

void txtscr_scroller_init(struct TXTSCRScrollText *settings, struct TXTSCRBigFont *bigfont, char *textscr, char xs, char ys, char xw, char border)
// Initialise a big-font scroll window (position/width/border) bound to
// a bigfont and a NUL-terminated source text.
{
    settings->textscr = textscr;
    settings->bigfont = bigfont;
    settings->count_char = 0;
    settings->count_col = 0;
    settings->count_softx = 0;
    vdcwin_init(&settings->win, xs, ys, xw, settings->bigfont->height);
    vdcwin_border_clear(&settings->win, WIN_BOR_ALL);
}

void txtscr_scroll_do(struct TXTSCRScrollText *settings)
// One scroll step: shifts the window one column left, then prints the
// next column of the current (or next, on wrap) character from
// settings->textscr at the newly-revealed edge.
{
    char y, ch;

    // Copy scroll lines to left
    vdcwin_scroll_left(&settings->win, 1);

    // Get char to print
    do
    {
        ch = *(settings->textscr + settings->count_char);

        // Has last char been arrived at?
        if (!ch)
        {
            settings->count_char = 0;
        }
    } while (!ch);

    // Print next column of char
    txtscr_bigfont_printchar(settings->bigfont, ch, settings->win.sx + settings->win.wx - 1, settings->win.sy, settings->count_col);
    settings->count_col++;
    if (settings->count_col > settings->bigfont->width - 1)
    {
        settings->count_col = 0;
        settings->count_char++;
    }
}

// =================================================================
// Cupid PETSCII font scroller -- see this file's own credit block
// above for provenance. Glyph tables/geometry/lookup functions below
// are a direct port of UltimateDemo2026/src/scroller.c's font data;
// txtscr_cupid_init()/txtscr_cupid_scroll_do() at the bottom are new,
// written for this project's per-character-column scroll mechanism
// (see vdc_textscroller.h's struct comment for why: no wide virtual
// buffer, no per-frame full-band redraw).
// =================================================================

// ---------------------------------------------------------------
// Font -- lowercase a-n: screen_001 rows 0-4 (petscii-cupid.petmate)
// ---------------------------------------------------------------
static const unsigned char cupid_lc_an_ch[5][40] = {
    {32,32,32,72,32,32,32,32,32,32,32,71,32,32,32,85,70,32,32,32,72,32,32,87,32,32,84,32,32,71,32,32,32,32,32,32,32,32,32,32},
    {85,68,73,107,68,73,85,68,73,85,68,115,85,68,73,107,70,85,68,73,107,68,73,89,32,73,71,32,78,66,112,68,114,68,73,112,68,73,32,32},
    {85,68,115,93,32,72,71,32,32,71,32,72,107,70,75,71,32,71,32,72,71,32,72,72,32,72,107,60,32,93,71,32,71,32,72,71,32,72,32,32},
    {74,70,125,109,70,75,74,70,75,74,70,75,74,70,75,74,32,74,70,115,66,32,66,113,32,72,66,32,77,72,109,32,125,32,125,109,32,125,32,32},
    {32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,70,75,32,32,32,32,70,75,32,32,32,32,32,32,32,32,32,32,32,32,32,32}
};
static const unsigned char cupid_lc_an_co[5][40] = {
    {14,14,14,13,13,14,14,14,14,14,15,13,14,14,14,13,14,14,14,14,15,15,14,14,14,14,14,15,14,14,15,14,14,14,14,14,14,14,14,14},
    {15,13,14,15,13,14,15,13,14,13,14,14,15,13,14,13,14,15,13,14,13,14,14,14,15,13,13,13,14,13,15,13,14,15,14,15,13,14,14,14},
    {5,13,13,5,14,13,15,15,14,15,15,13,15,13,14,15,15,15,15,13,15,15,13,13,15,14,15,13,15,15,5,15,13,15,13,5,15,13,15,14},
    {5,5,15,5,5,15,5,15,13,5,5,15,5,5,15,5,15,5,5,15,5,15,15,15,15,13,5,5,5,5,5,15,15,15,15,5,15,15,15,14},
    {14,14,14,14,14,14,14,14,14,14,14,15,15,14,14,15,15,15,5,15,15,15,15,15,5,15,15,15,15,15,15,15,14,14,14,14,14,14,14,14}
};

// ---------------------------------------------------------------
// Font -- lowercase o-z: screen_001 rows 5-9 (petscii-cupid.petmate)
// ---------------------------------------------------------------
static const unsigned char cupid_lc_oz_ch[5][40] = {
    {32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,72,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32},
    {85,68,73,85,68,73,85,68,73,85,68,73,85,68,73,107,68,32,85,32,73,84,89,85,32,85,32,73,77,32,47,85,32,73,112,68,110,32,32,32},
    {71,32,72,71,32,72,71,32,72,71,32,32,74,68,73,71,32,72,71,32,72,71,72,71,32,71,32,72,32,86,32,71,32,72,85,68,75,32,32,32},
    {74,70,75,107,70,75,74,70,115,113,32,32,74,70,75,74,70,75,74,70,75,74,75,74,70,113,70,75,47,32,77,74,70,115,109,70,67,32,32,32},
    {32,32,32,72,32,32,32,32,71,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,70,75,32,32,32,32,32,32}
};
static const unsigned char cupid_lc_oz_co[5][40] = {
    {14,14,14,15,15,14,14,14,14,14,14,14,14,15,14,14,14,14,15,15,15,14,15,15,15,15,14,14,15,15,15,15,15,15,15,14,14,14,14,14},
    {15,13,14,15,13,14,15,13,14,13,14,13,13,14,13,13,14,15,13,15,14,13,14,13,15,14,15,14,14,15,14,13,15,14,15,13,14,14,14,14},
    {5,15,13,15,15,13,15,15,13,15,15,15,13,15,15,15,15,13,15,15,14,15,13,15,15,13,15,13,15,13,15,15,15,14,15,15,13,14,14,14},
    {5,5,15,5,15,15,5,5,15,5,15,15,5,5,15,5,5,15,5,15,13,5,15,5,5,15,5,15,5,15,15,5,15,13,5,5,15,15,14,14},
    {14,15,15,5,14,15,15,14,5,14,15,14,14,14,14,14,14,14,15,15,15,15,14,14,14,14,14,14,14,14,14,14,5,15,14,14,14,14,14,14}
};

// ---------------------------------------------------------------
// Font -- uppercase A-M: screen_001 rows 20-24 (petscii-cupid.petmate)
// ---------------------------------------------------------------
static const unsigned char cupid_am_ch[5][40] = {
    {85,68,73,112,68,73,85,68,73,112,68,73,85,68,110,85,68,110,85,68,73,114,32,114,32,114,32,32,32,110,114,32,47,114,32,32,85,114,73,32},
    {107,70,115,107,70,115,71,32,32,72,32,72,107,68,32,107,68,32,71,68,110,107,68,115,32,93,32,112,32,72,107,60,32,93,32,32,71,72,72,32},
    {71,32,72,71,32,72,71,32,32,72,32,72,71,32,32,71,32,32,71,32,72,72,32,71,32,72,32,71,32,72,72,32,77,71,32,32,71,93,72,32},
    {109,32,125,109,70,75,74,70,75,109,70,75,74,70,125,113,32,32,74,70,75,113,32,113,32,113,32,74,70,75,113,32,39,109,70,125,74,32,75,32},
    {32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32}
};
static const unsigned char cupid_am_co[5][40] = {
    {15,13,14,15,13,14,15,13,14,15,13,14,15,13,14,15,13,14,15,13,14,15,13,14,13,14,14,13,13,14,15,13,14,14,13,13,13,13,14,13},
    {15,13,13,15,13,13,15,15,15,15,15,13,15,13,15,5,15,15,15,15,15,15,13,13,13,13,13,13,13,13,15,14,13,13,13,15,15,13,13,13},
    {5,15,13,5,15,13,5,15,13,5,15,13,5,15,13,5,15,5,5,15,13,5,15,13,13,15,13,5,15,13,5,13,13,15,15,13,5,15,15,13},
    {5,15,15,5,5,15,5,5,15,5,5,15,5,5,15,5,5,5,5,5,15,5,15,15,13,5,13,5,5,15,5,13,5,5,5,15,5,13,15,13},
    {14,15,15,14,14,14,15,15,13,13,13,13,13,13,13,14,14,14,14,14,15,14,15,15,15,13,13,14,14,15,15,13,13,14,13,13,14,14,13,14}
};

// ---------------------------------------------------------------
// Font -- uppercase N-Z: screen_002 rows 0-4 (petscii-cupid.petmate)
// ---------------------------------------------------------------
static const unsigned char cupid_nz_ch[5][40] = {
    {114,32,114,85,68,73,85,68,73,85,68,73,85,68,73,85,68,73,112,114,110,85,32,73,116,89,85,32,73,32,32,32,71,32,71,67,68,110,32,32},
    {71,77,72,71,32,72,71,32,72,71,32,72,71,32,72,74,68,73,32,93,32,71,32,72,84,89,71,72,72,77,32,47,74,70,115,32,78,32,32,32},
    {71,32,72,71,32,72,107,70,75,71,32,72,107,70,75,71,32,72,32,93,32,71,32,72,71,72,71,93,72,32,86,32,71,32,72,78,32,72,32,32},
    {113,32,113,74,70,75,93,32,32,74,70,77,93,32,77,74,70,75,32,113,32,74,70,75,74,75,74,113,75,47,32,77,109,70,75,109,70,125,32,32},
    {32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32}
};
static const unsigned char cupid_nz_co[5][40] = {
    {15,13,14,15,13,14,13,13,14,15,13,14,13,13,14,15,13,14,15,13,14,15,13,14,13,14,13,13,14,15,13,14,15,13,14,15,13,14,13,13},
    {15,13,13,15,13,13,15,15,13,15,13,13,15,15,13,15,13,14,5,15,15,15,15,15,15,13,15,13,13,15,13,14,15,13,13,15,13,14,13,13},
    {5,15,13,5,15,13,5,15,15,5,15,13,5,15,15,5,15,13,5,15,5,5,15,13,5,15,5,15,15,15,13,13,5,15,13,5,15,13,13,13},
    {5,15,15,5,5,15,5,5,5,5,5,15,5,5,5,5,5,15,5,5,5,5,5,15,5,13,5,5,15,5,15,13,5,5,15,5,5,15,13,13},
    {14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,15,14,14,14,14,14,5,5,5,14,5,5,5,14,14,5,14}
};

// ---------------------------------------------------------------
// Font -- digits 0-9 + symbols : . ! ; , / ' " : screen_001 rows 10-14
// ---------------------------------------------------------------
static const unsigned char cupid_dig_ch[5][40] = {
    {85,68,73,73,85,68,73,85,68,73,85,32,73,112,68,110,85,68,73,112,68,110,85,68,73,85,68,73,32,32,89,32,32,32,32,32,66,66,66,32},
    {71,78,72,72,85,70,75,32,70,115,71,32,72,74,68,73,107,68,73,32,85,125,107,70,115,71,32,72,81,32,72,81,32,32,32,47,72,72,71,32},
    {71,32,72,72,71,32,32,32,32,72,74,70,115,71,32,72,71,32,72,32,71,32,71,32,72,74,70,115,81,32,93,66,66,32,78,32,32,32,32,32},
    {74,70,75,113,74,70,125,109,70,75,32,32,71,74,70,75,74,70,75,32,84,32,74,70,75,74,70,75,32,81,81,72,72,47,32,32,32,32,32,32},
    {32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,75,75,32,32,32,32,32,32,32},
};
static const unsigned char cupid_dig_co[5][40] = {
    {15,13,14,14,15,13,14,13,14,14,13,15,14,15,13,14,15,13,14,15,13,14,15,13,14,13,14,14,15,15,14,14,15,15,15,15,14,14,14,14},
    {15,15,14,13,15,13,14,15,15,13,15,15,13,15,13,14,15,13,14,15,15,13,15,13,14,15,15,13,13,15,13,13,15,14,15,13,13,13,13,14},
    {5,15,13,15,5,15,15,15,15,15,5,5,15,5,15,13,5,15,13,15,15,14,5,15,13,5,15,15,15,15,15,15,13,15,15,15,15,14,14,14},
    {5,5,15,5,5,5,15,5,5,15,15,15,5,5,5,15,5,5,15,15,5,14,5,5,15,5,5,15,15,15,5,5,15,5,14,14,14,14,14,14},
    {14,15,15,14,14,14,15,15,15,15,14,14,15,15,15,14,14,14,14,14,15,14,15,15,15,15,15,14,14,15,15,5,5,14,14,14,14,14,14,14},
};

// ---------------------------------------------------------------
// Font -- symbols $ & % - + ? = # ( < ) > [ @ ] : screen_001 rows 15-19
// ---------------------------------------------------------------
static const unsigned char cupid_sym_ch[5][40] = {
    {85,91,73,85,68,73,32,32,32,32,32,32,32,32,32,32,112,68,73,32,32,32,32,71,71,85,68,32,47,68,73,77,32,112,64,32,32,32,64,110},
    {74,91,73,107,70,115,87,32,47,32,32,32,32,32,72,32,32,85,75,70,67,68,32,91,91,71,32,60,32,32,72,32,62,93,32,85,68,73,32,66},
    {32,66,72,71,77,72,32,78,32,32,70,67,68,70,91,68,32,71,32,70,67,68,32,91,91,71,32,32,77,32,72,47,32,93,32,71,74,75,32,66},
    {74,91,75,74,70,91,47,32,87,32,32,32,32,32,71,32,32,81,32,32,32,32,32,72,72,74,70,32,32,70,75,32,32,109,64,74,70,75,64,125},
    {32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32},
};
static const unsigned char cupid_sym_co[5][40] = {
    {15,13,14,13,14,14,14,15,15,14,14,14,14,15,14,15,13,14,14,14,14,14,14,13,14,13,14,15,14,13,14,14,14,13,14,14,14,14,13,14},
    {15,13,14,15,15,13,13,15,14,14,14,14,15,15,14,15,15,15,13,15,13,14,15,15,13,15,15,13,15,15,13,14,13,15,15,13,13,14,15,13},
    {15,15,13,5,15,13,15,13,15,15,15,13,14,15,13,15,15,5,14,15,13,14,14,15,15,5,15,14,15,15,15,15,14,5,15,15,13,15,15,15},
    {5,5,15,5,15,15,15,14,15,15,14,15,14,15,15,15,14,5,14,14,14,14,14,5,5,5,15,15,14,5,5,14,14,5,15,5,5,15,15,5},
    {14,14,14,14,15,15,14,14,15,15,15,14,14,14,15,14,14,15,13,14,14,14,14,15,14,13,13,13,13,13,14,14,14,14,14,14,14,13,15,14},
};

// Letter geometry (indices 0-25 upper A-Z, 26-51 lower a-z, 52 space,
// 53-62 digits, 63-70 symbols : . ! ; , / ' ", 71-85 symbols $ & % - + ? = # ( < ) > [ @ ]).
static const unsigned char cupid_letter_start[86] = {
    0, 3, 6, 9,12,15,18,21,25,27,30,33,36,
    0, 3, 6, 9,12,15,18,21,24,26,29,32,35,
    0, 3, 6, 9,12,15,17,20,23,24,26,29,30,35,
    0, 3, 6, 9,12,15,18,21,23,28,31,34,
    0,
    0, 3, 4, 7,10,13,16,19,22,25,
    28,29,30,31,32,33,36,37,
    0, 3, 6, 9,13,16,19,23,25,27,29,31,33,35,38
};
static const unsigned char cupid_letter_width[86] = {
    3,3,3,3,3,3,3,4,2,3,3,3,3,
    3,3,3,3,3,3,3,3,2,3,3,3,3,
    3,3,3,3,3,2,3,3,1,2,3,1,5,3,
    3,3,3,3,3,3,3,2,5,3,3,3,
    3,
    3,1,3,3,3,3,3,3,3,3,
    1,1,1,1,1,3,1,2,
    3,3,3,4,3,3,4,2,2,2,2,2,2,3,2
};

// Sine table for the scroller wave: +/-1 row, 64 entries (subtle wave).
// Gentler than UltimateDemo2026's own +/-2 source table, same wave shape.
static const signed char cupid_sin_row[64] = {
     0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0, 0, 1,
     0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0, 0, 1
};

static unsigned char cupid_font_ch(unsigned char li, unsigned char ci, unsigned char ri)
// Screen-code glyph pixel for letter index li, glyph column ci, glyph
// row ri -- looks up the correct one of the six per-case/digit/symbol
// tables above by li's range.
{
    unsigned char sc = (unsigned char)(cupid_letter_start[li] + ci);
    if (li < 13) return cupid_am_ch[ri][sc];
    if (li < 26) return cupid_nz_ch[ri][sc];
    if (li < 40) return cupid_lc_an_ch[ri][sc];
    if (li < 52) return cupid_lc_oz_ch[ri][sc];
    if (li < 71) return cupid_dig_ch[ri][sc];
    return cupid_sym_ch[ri][sc];
}

static unsigned char cupid_font_co(unsigned char li, unsigned char ci, unsigned char ri)
// Colour byte for the same glyph pixel cupid_font_ch() looks up.
{
    unsigned char sc = (unsigned char)(cupid_letter_start[li] + ci);
    if (li < 13) return cupid_am_co[ri][sc];
    if (li < 26) return cupid_nz_co[ri][sc];
    if (li < 40) return cupid_lc_an_co[ri][sc];
    if (li < 52) return cupid_lc_oz_co[ri][sc];
    if (li < 71) return cupid_dig_co[ri][sc];
    return cupid_sym_co[ri][sc];
}

// letter_idx: 0-25 uppercase, 26-51 lowercase, 52 space/other, 53-62
// digits, 63+ symbols. Relies on <petscii.h>'s charmap swap (already
// included by this file): source uppercase A-Z compiles to PETSCII
// 0x61-0x7A (97-122), source lowercase a-z compiles to PETSCII 0x41-0x5A
// (65-90) -- so ordinary mixed-case C source text (e.g. "Hello World")
// routes correctly with no special-casing needed at the call site.
static unsigned char cupid_letter_idx(unsigned char c)
{
    if (c >= 97 && c <= 122) return (unsigned char)(c - 97);
    if (c >= 65 && c <= 90) return (unsigned char)(c - 65 + 26);
    if (c >= 48 && c <= 57) return (unsigned char)(c - 48 + 53);
    switch (c)
    {
    case 58: return 63;  // :
    case 46: return 64;  // .
    case 33: return 65;  // !
    case 59: return 66;  // ;
    case 44: return 67;  // ,
    case 47: return 68;  // /
    case 39: return 69;  // '
    case 34: return 70;  // "
    case 36: return 71;  // $
    case 38: return 72;  // &
    case 37: return 73;  // %
    case 45: return 74;  // -
    case 43: return 75;  // +
    case 63: return 76;  // ?
    case 61: return 77;  // =
    case 35: return 78;  // #
    case 40: return 79;  // (
    case 60: return 80;  // <
    case 41: return 81;  // )
    case 62: return 82;  // >
    case 91: return 83;  // [
    case 64: return 84;  // @
    case 93: return 85;  // ]
    }
    return 52;
}

void txtscr_cupid_init(struct TXTSCRCupidScroll *settings, const char *textscr, char xs, char ys, char xw)
// Initialise a Cupid-font per-column live-blit scroll window bound to a
// NUL-terminated, looping source text. Not currently called from
// main.c -- superseded by the pre-rendered variant further down this
// file -- kept for reference. xs/ys/xw: screen position and width
// (columns) of the scroll window; height is always CUPID_BAND_H.
{
    settings->textscr = textscr;
    settings->count_char = 0;
    settings->count_col = 0;
    settings->letter_phase = 0;
    settings->row_offset = 0;
    vdcwin_init(&settings->win, xs, ys, xw, CUPID_BAND_H);
}

void txtscr_cupid_scroll_do(struct TXTSCRCupidScroll *settings)
// One scroll step: shift the window one character-column left (VDC
// hardware block-copy via vdcwin_scroll_left(), same primitive
// vdcwin_viewportscroll() itself already uses elsewhere in this
// library), then compute and write the single new column revealed at
// the right edge -- the only VDC memory this step ever touches, whether
// that column is glyph pixels, inter-letter blank space, or the
// blank/clearance rows above and below the current glyph.
{
    unsigned char ch, li, row;
    char x, y0, fr;

    vdcwin_scroll_left(&settings->win, 1);

    do
    {
        ch = (unsigned char)settings->textscr[settings->count_char];
        if (!ch)
        {
            settings->count_char = 0;
        }
    } while (!ch);

    li = cupid_letter_idx(ch);

    if (settings->count_col == 0)
    {
        // New letter starting: sample this letter's own sine row offset
        // once, reuse it for every column of this letter -- the whole
        // glyph rides one shift, never distorts internally.
        settings->row_offset = cupid_sin_row[settings->letter_phase & 63];
        settings->letter_phase++;
    }

    x = settings->win.sx + settings->win.wx - 1;
    y0 = settings->win.sy;

    for (row = 0; row < CUPID_BAND_H; row++)
    {
        if (li != 52 && row >= CUPID_BASE_OFFSET + settings->row_offset &&
            row < CUPID_BASE_OFFSET + settings->row_offset + CUPID_FONT_H)
        {
            fr = row - (CUPID_BASE_OFFSET + settings->row_offset);
            vdc_printc(x, y0 + row, cupid_font_ch(li, settings->count_col, fr), cupid_font_co(li, settings->count_col, fr));
        }
        else
        {
            vdc_printc(x, y0 + row, 32, VDC_BLACK);
        }
    }

    settings->count_col++;
    if (settings->count_col >= cupid_letter_width[li])
    {
        settings->count_col = 0;
        settings->count_char++;
    }
}

// =================================================================
// Cupid pre-rendered scroller -- for use with vdc_softscroll.c instead
// of the per-column live-blit approach above (txtscr_cupid_scroll_do(),
// not currently called anywhere; kept for reference). Attention point:
// txtscr_cupid_scroll_do()'s per-frame vdcwin_scroll_left() (hardware
// block-copy, 2 operations per row of the scroll window) is expensive
// enough to be the dominant cost of a per-frame text scroll -- not the
// sine calculation, which is a single array lookup and negligible. The
// functions below avoid it architecturally: render the whole message
// once, up front (a one-time cost with no per-frame budget to blow),
// then let vdc_softscroll's own VDCR_HSCROLL/display-address register
// panning (single synchronous register writes, no block-copy) handle
// the actual per-frame motion. Prefer this pre-rendered path for any
// new per-frame text scroll.
// =================================================================

unsigned txtscr_cupid_measure(const char *text)
// Total column width `text` needs when rendered in the Cupid font --
// call this first to size the vdc_softscroll virtual buffer/width.
{
    unsigned width = 0;
    unsigned char ch, li;

    while ((ch = (unsigned char)*text++) != 0)
    {
        li = cupid_letter_idx(ch);
        width += cupid_letter_width[li];
    }
    return width;
}

void txtscr_cupid_render(char cr, char *dest, const char *text, unsigned width, char total_height, char band_row, unsigned start_col)
// Renders `text` into a flat [width*total_height screen codes][48-byte
// pad][width*total_height attribute bytes] buffer at `dest` in bank `cr`
// -- exactly the layout vdc_softscroll_init()'s own `source` parameter
// expects (see vdc_softscroll.c). `width` must be >= txtscr_cupid_
// measure(text)'s own return value.
//
// Attention point: total_height MUST match the visible mode's own row
// count (e.g. 25 for VDC_TEXT_80x25_PAL), not just CUPID_BAND_H --
// vdc_softscroll_init() remaps the *entire* screen's addressing to this
// buffer, so every row the display actually scans needs real (even if
// blank) content; leaving rows unwritten reads back as whatever garbage
// was already sitting in that VDC memory. band_row is which row of the
// full buffer the glyph band's own row 0 sits at (CUPID_BAND_H rows tall
// from there); every other row is flat blank space.
//
// start_col: normally 0 (blank+render the whole buffer, the original
// behaviour). Pass a nonzero value to preserve columns [0, start_col) of
// every row untouched and only blank+render from there on -- used by
// credits_screen()'s chunk-transition rebase (main.c) to append new text
// right after content it has deliberately preserved (the currently-visible
// window, copied there by the caller beforehand) instead of erasing it,
// so a transition can extend the buffer without a visible reset/blank.
//
// The sine row-offset is sampled once per letter here, at render time,
// not per frame -- baked into the buffer's row placement, matching every
// other raster/colour effect in this project's own "no per-frame work
// that doesn't need to be per-frame" discipline.
{
    unsigned col = start_col;
    unsigned char ch, li, ci, row, idx;
    unsigned char letter_phase = 0;
    signed char row_offset = 0;
    unsigned vdcsize = width * (unsigned)total_height;

    // Blank from start_col on (cheap relative to the one-time cost of this
    // whole function; simplest way to guarantee no row is ever left
    // unwritten regardless of message length/column count). Columns before
    // start_col are the caller's own preserved content -- left alone.
    for (row = 0; row < total_height; row++)
    {
        for (col = start_col; col < width; col++)
        {
            bnk_writeb(cr, (volatile char *)(dest + (unsigned)row * width + col), 32);
            bnk_writeb(cr, (volatile char *)(dest + vdcsize + 48 + (unsigned)row * width + col), VDC_BLACK);
        }
    }

    col = start_col;
    while (col < width)
    {
        ch = (unsigned char)*text;
        if (!ch)
        {
            return; // remainder already blanked above
        }
        text++;
        li = cupid_letter_idx(ch);
        row_offset = cupid_sin_row[letter_phase & 63];
        letter_phase++;

        for (ci = 0; ci < cupid_letter_width[li] && col < width; ci++, col++)
        {
            if (li == 52)
            {
                continue; // space: already blank from the pre-fill above
            }
            for (row = 0; row < CUPID_FONT_H; row++)
            {
                idx = (unsigned char)(band_row + CUPID_BASE_OFFSET + row_offset + row);
                bnk_writeb(cr, (volatile char *)(dest + (unsigned)idx * width + col), cupid_font_ch(li, ci, row));
                bnk_writeb(cr, (volatile char *)(dest + vdcsize + 48 + (unsigned)idx * width + col), cupid_font_co(li, ci, row));
            }
        }
    }
}

unsigned char txtscr_cupid_letter_width(unsigned char ch)
// Glyph width (columns) for `ch`, without touching the buffer -- lets a
// caller check room (col + width <= buffer width) before starting
// txtscr_cupid_render_letter_step()'s per-row spread below.
{
    return cupid_letter_width[cupid_letter_idx(ch)];
}

void txtscr_cupid_render_letter_step(char cr, char *dest, unsigned width, char total_height, char band_row, unsigned col, unsigned char ch, unsigned char letter_phase, unsigned char step)
// Renders ONE ROW of a single letter (`ch`) at buffer column `col`, in the
// same [width*total_height screen codes][48-byte pad][width*total_height
// attribute bytes] buffer layout txtscr_cupid_render() itself uses. Caller
// drives `step` from 0 to CUPID_RENDER_STEPS-1 (vdc_textscroller.h) across
// that many separate frames to render one whole letter -- step 0..
// CUPID_BAND_H-1 blanks band row `step` across this letter's own width;
// step CUPID_BAND_H..CUPID_RENDER_STEPS-1 overlays glyph row
// (step-CUPID_BAND_H) (skipped for a space, cupid_letter_idx()==52 --
// already blank from the earlier blank steps). Column bounds
// (col+width<=width) must already be checked by the caller via
// txtscr_cupid_letter_width() before starting a letter's steps.
//
// Attention point: exists so credits_screen() (main.c) can extend its
// scroll buffer one ROW at a time, not one whole letter at a time -- a
// single whole-letter render (up to CUPID_BAND_H*2 blank writes plus
// CUPID_FONT_H*2 glyph writes, ~70 bnk_writeb() calls worst case) costs
// more than this VDC revision's entire ~48-line VBLANK window, so doing
// it in one frame risks raster jitter. One row here (at most
// BAND_LETTER_MAX_W columns x 2 planes = 10 bnk_writeb() calls) is a
// small fraction of that, safe to spread across several frames instead.
//
// Only ever touches CUPID_BAND_H rows starting at band_row -- every row
// outside that band was already blanked once, across the buffer's full
// width, when the buffer was first set up, and nothing else ever writes
// there.
{
    unsigned char li = cupid_letter_idx(ch);
    unsigned char lw = cupid_letter_width[li];
    unsigned char ci, idx;
    signed char row_offset;
    unsigned vdcsize = width * (unsigned)total_height;
    unsigned c;

    if (step < CUPID_BAND_H)
    {
        idx = band_row + step;
        for (c = col; c < col + lw; c++)
        {
            bnk_writeb(cr, (volatile char *)(dest + (unsigned)idx * width + c), 32);
            bnk_writeb(cr, (volatile char *)(dest + vdcsize + 48 + (unsigned)idx * width + c), VDC_BLACK);
        }
    }
    else if (li != 52) // not a space -- overlay the glyph (space: already blank from the steps above)
    {
        unsigned char row = step - CUPID_BAND_H;
        row_offset = cupid_sin_row[letter_phase & 63];
        idx = (unsigned char)(band_row + CUPID_BASE_OFFSET + row_offset + row);
        for (ci = 0; ci < lw; ci++)
        {
            bnk_writeb(cr, (volatile char *)(dest + (unsigned)idx * width + col + ci), cupid_font_ch(li, ci, row));
            bnk_writeb(cr, (volatile char *)(dest + vdcsize + 48 + (unsigned)idx * width + col + ci), cupid_font_co(li, ci, row));
        }
    }
}