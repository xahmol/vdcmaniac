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
#include <c128/vdc.h>
#include "vdc_core.h"
#include "banking.h"
#include "peekpoke.h"

char linebuffer[81];
struct VDCStatus vdc_state;
unsigned multab[72];

// Registers no vdc_modes[] row (and no other code path) ever resets to a
// known baseline -- either because no mode's row lists them at all (VSCROLL:
// vdc_block_fill()/vdc_block_copy_page(), used by every vdc_wipe_mem() call
// among others, only ever touch its bit 7 "COPY" flag, preserving whatever
// its vertical-scroll-amount bits happen to be -- confirmed live: a later
// section came out shifted, tracked down to this register never having been
// given a baseline anywhere in the codebase), or because only the one mode
// that first changes them away from the KERNAL's boot-time default ever
// touches them again (HDISPLAY/HSYNC/SYNCSIZE/HSCROLL/DISP_ADDR/ATTR_ADDR:
// VDC-FLI/VDC-IMONO's rows set these to their own narrow/bitmap-specific
// values; VDC_TEXT_80x25_PAL's row lists none of them). Without resetting
// all of these to a known baseline, whichever effect last touched one leaks
// forward into every later mode that doesn't happen to override it itself
// -- including straight through vdc_exit() into BASIC. Captured once, on
// the very first vdc_init() call, before anything (including that first
// call's own mode) has touched them -- restored by every later vdc_init()
// call and by vdc_exit().
// HSTART (register 34): vdc_disable_display()/vdc_enable_display() use this
// to blank/unblank the screen by pushing the visible window off-screen and
// back -- previously two bare hardcoded literals (0x80 disabled, 0x7d
// enabled), with no link to any mode's own timing and no boot-time capture
// at all. Captured/used the same way as the registers above so "enabled"
// always restores the real baseline instead of an assumed-universal literal.
struct VDCBootBaseline
{
    char captured;
    char hdisplay;
    char hsync;
    char syncsize;
    char hscroll;
    char vscroll;
    char hend;
    char dispaddrh;
    char dispaddrl;
    char attraddrh;
    char attraddrl;
    char hstart;
};
static struct VDCBootBaseline vdc_boot;

// VDC mode settings. Credits to Tokra.
struct VDCModeSet vdc_modes[19] =
    {
        // VDC_TEXT_80x25_PAL: standard 80x25 text mode, PAL -- this
        // project's default/baseline mode (main() switches here first,
        // and vdc_exit() returns to it).
        {80, 25, 0, 8, 0, 0x0000, 0x0800, 0x1000, 0x1800, 0x2000, 0x3000, 0x4000, {VDCR_HTOTAL, 0x7f, VDCR_VTOTAL, 0x26, VDCR_VADJUST, 0xe0, VDCR_VDISPLAY, 0x19, VDCR_VSYNC, 0x20, VDCR_LACE, 0xfc, VDCR_CSIZE, 0xe7, VDCR_REFRESH, 0x7e, 255}},
        // VDC_TEXT_80x50_PAL: 80x50 text mode, PAL (double-height rows via
        // a shorter char cell).
        {80, 50, 0, 8, 0, 0x0000, 0x1000, 0x4000, 0x5000, 0x2000, 0x3000, 0x6000, {VDCR_HTOTAL, 0x7f, VDCR_VTOTAL, 0x4d, VDCR_VADJUST, 0x00, VDCR_VDISPLAY, 0x32, VDCR_VSYNC, 0x40, VDCR_LACE, 0x03, VDCR_CSIZE, 0x07, VDCR_REFRESH, 0x00, 255}},
        // VDC_TEXT_80x70_PAL: 80x70 text mode, PAL -- extmem=1 (needs the
        // full 64KB VDC).
        {80, 70, 0, 8, 1, 0x0000, 0x1800, 0x6000, 0x7800, 0x4000, 0x5000, 0x9000, {VDCR_HTOTAL, 0x7f, VDCR_VTOTAL, 0x4d, VDCR_VADJUST, 0x00, VDCR_VDISPLAY, 0x46, VDCR_VSYNC, 0x48, VDCR_LACE, 0x03, VDCR_CSIZE, 0x07, VDCR_REFRESH, 0x00, 255}},
        // VDC_TEXT_80x25_NTSC: 80x25 text mode, NTSC timing.
        {80, 25, 0, 8, 0, 0x0000, 0x0800, 0x1000, 0x1800, 0x2000, 0x3000, 0x4000, {VDCR_HTOTAL, 0x7e, VDCR_VTOTAL, 0x20, VDCR_VADJUST, 0xe0, VDCR_VDISPLAY, 0x19, VDCR_VSYNC, 0x1d, VDCR_LACE, 0xfc, VDCR_CSIZE, 0xe7, VDCR_REFRESH, 0xf5, 255}},
        // VDC_TEXT_80x50_NTSC: 80x50 text mode, NTSC timing.
        {80, 50, 0, 8, 0, 0x0000, 0x1000, 0x4000, 0x5000, 0x2000, 0x3000, 0x6000, {VDCR_HTOTAL, 0x7e, VDCR_VTOTAL, 0x41, VDCR_VADJUST, 0x00, VDCR_VDISPLAY, 0x32, VDCR_VSYNC, 0x3b, VDCR_LACE, 0x03, VDCR_CSIZE, 0x07, VDCR_REFRESH, 0x00, 255}},
        // VDC_TEXT_80x60_NTSC: 80x60 text mode, NTSC timing -- extmem=1.
        {80, 60, 0, 8, 1, 0x0000, 0x1800, 0x6000, 0x7800, 0x4000, 0x5000, 0x9000, {VDCR_HTOTAL, 0x7e, VDCR_VTOTAL, 0x41, VDCR_VADJUST, 0x00, VDCR_VDISPLAY, 0x3c, VDCR_VSYNC, 0x3d, VDCR_LACE, 0x03, VDCR_CSIZE, 0x07, VDCR_REFRESH, 0x00, 255}},
        // VDC_HIRES_640x200_Color_PAL / VDC_HIRES_640x200_Mono_PAL (next
        // two rows): 640x200 bitmap, PAL, colour (attribute mode on) and
        // mono variants -- used by plasma_demo()/rotate_demo(). HSCROLL
        // (register 25) explicitly set to 0x07 below on both these rows --
        // previously omitted (leaving it inherited from whichever mode ran
        // before) or wrongly set to 0x00 (an unverified assumption that
        // briefly caused plasma_demo()/rotate_demo()'s "isolated shifted
        // fragment" bug, see memory: rotate_demo_shift_bug). 0x07 matches
        // Tokra's own convention seen on several other modes' HSCROLL
        // values (0x87/0xc7 etc., always low-nibble 7), needed to
        // correctly align this bitmap mode's scan start to its
        // character-cell-based addressing. Live-confirmed working.
        {640, 200, 1, 8, 1, 0x0000, 0x4000, 0x4800, 0x8800, 0x9000, 0xa000, 0xb000, {VDCR_HTOTAL, 0x7f, VDCR_VTOTAL, 0x26, VDCR_VADJUST, 0xe0, VDCR_VDISPLAY, 0x19, VDCR_VSYNC, 0x20, VDCR_LACE, 0xfc, VDCR_CSIZE, 0xe7, VDCR_REFRESH, 0x7e, VDCR_HSCROLL, 0x07, 255}},
        {640, 200, 1, 0, 1, 0x0000, 0x4000, 0x4800, 0x8800, 0x9000, 0xa000, 0xb000, {VDCR_HTOTAL, 0x7f, VDCR_VTOTAL, 0x26, VDCR_VADJUST, 0xe0, VDCR_VDISPLAY, 0x19, VDCR_VSYNC, 0x20, VDCR_LACE, 0xfc, VDCR_CSIZE, 0xe7, VDCR_REFRESH, 0x7e, VDCR_HSCROLL, 0x07, 255}},
        // VDC_HIRES_640x400_Color_PAL: 640x400 bitmap, PAL, colour
        // (attribute mode on).
        {640, 400, 1, 8, 1, 0x0000, 0x8000, 0x9000, 0xd000, 0xe000, 0xf000, 0x0000, {VDCR_HTOTAL, 0x7f, VDCR_VTOTAL, 0x4d, VDCR_VADJUST, 0x00, VDCR_VDISPLAY, 0x32, VDCR_VSYNC, 0x40, VDCR_LACE, 0x03, VDCR_CSIZE, 0x07, VDCR_REFRESH, 0x00, 255}},
        // VDC_HIRES_640x400_Mono_PAL: 640x400 bitmap, PAL, monochrome --
        // title_screen()'s own mode.
        {640, 400, 1, 0, 1, 0x0000, 0x0000, 0x8000, 0x0000, 0x9000, 0xa000, 0xb000, {VDCR_HTOTAL, 0x7f, VDCR_VTOTAL, 0x4d, VDCR_VADJUST, 0x00, VDCR_VDISPLAY, 0x31, VDCR_VSYNC, 0x40, VDCR_LACE, 0x03, VDCR_CSIZE, 0x07, VDCR_REFRESH, 0x00, 255}},
        // VDC_HIRES_640x480_Mono_NTSC: 640x480 bitmap, NTSC, monochrome,
        // interlace.
        {640, 480, 1, 0, 1, 0x0000, 0x0000, 0x9600, 0x0000, 0xa000, 0xb000, 0xc000, {VDCR_HTOTAL, 0x7e, VDCR_SYNCSIZE, 0x89, VDCR_VTOTAL, 0x84, VDCR_VADJUST, 0x03, VDCR_VDISPLAY, 0x84, VDCR_LACE, 0x03, VDCR_CSIZE, 0x03, VDCR_REFRESH, 0x02, 255}},
        // VDC-FLI: 480x252, non-interlace, 8x1 colour cells. Timing values
        // from Tokra's "VDC Mode Mania" (original/v12/source/vdcmodemania.bas
        // line 78), minus its DISP_ADDR/ATTR_ADDR/CHAR_ADDR register pairs --
        // this project sets those from base_text/base_attr via
        // vdc_set_disp_address() instead (see vdc_set_mode()); char_std/
        // char_alt below are left 0x0000, which makes vdc_set_mode() skip
        // charset setup for this mode entirely (no room/need for one).
        // HSCROLL (register 25) restored from the same source table: Tokra's
        // 0xc7 (bits 7/6 = bitmap/attribute enable, already applied
        // generically by vdc_set_mode() regardless of what's written here;
        // bits 0-3 = horizontal smooth-scroll amount = 7). This row omitted
        // it entirely at first -- leaving the scroll nibble inherited from
        // whichever mode ran before -- which is the likely cause of this
        // mode's picture looking subtly misaligned/"unrecognisable" rather
        // than outright broken.
        // "8x1" in the mode's name means 8 pixels wide by 1 scanline tall
        // per colour cell --
        // true per-scanline colour resolution -- so the bitmap is plain
        // row-major (no 8-line interleaving), matching what
        // tools/vdc_convert.py already produces. LACE explicitly 0x00
        // (rather than omitted) so a previously active interlaced mode
        // can't leave stale interlace state behind.
        //
        // CSIZE (register 9) is deliberately NOT set here -- Tokra's own
        // DATA statement for this mode omits it too, with the comment
        // "register 9 set by assembler loop". Disassembling that routine
        // (sys4864 in vdcmodemania.bas, POKEd from DATA at line 4) shows
        // it's not a static value at all: every frame, while idle, it
        // writes CSIZE=$e0 (char height 1) right as the VDC's status bit 5
        // (see raster_synch()'s identical wait pattern) clears (start of
        // active display), then writes CSIZE=$e7 (char height 8) right as
        // bit 5 sets again (start of the next vblank) -- i.e. the real
        // 8563/8568 hardware needs char-height toggled every single frame
        // to hold this resolution's addressing stable; a static value
        // (either one) works for a few lines then drifts, which is exactly
        // the tiled/repeating corruption seen when this was first tried as
        // a fixed 0x00 or 0xe7 in this table. See fli_color_demo() in
        // main.c for the per-frame toggle that replaces the missing entry.
        //
        // DISP_ADDR/ATTR_ADDR (registers 12/13/20/21) restored to Tokra's
        // exact values (248,8 / 63,196 = 0xf808/0x3fc4) below, instead of
        // this project's usual base_text/base_attr convention (which would
        // give 0x0000/0x4000 via vdc_set_disp_address() -- what this row
        // originally left in place by omitting these registers entirely).
        // Disassembling Tokra's file loader (gosub9999, DATA at
        // vdcmodemania.bas line 51) shows the picture bytes themselves are
        // copied to plain 0x0000/0x4000 via a *different* register pair
        // (18/19, the generic VDC address pointer) -- so 0xf808/0x3fc4 are
        // NOT "where the picture is", they're a separate value the mode
        // apparently needs for its own sake. Given this mode's per-frame
        // CSIZE toggle (see fli_color_demo()) is documented as resetting an
        // "internal VDC addressing counter" every vblank, DISP_ADDR/
        // ATTR_ADDR most likely seed that counter. Live-confirmed working.
        {480, 252, 1, 8, 1, 0x0000, 0x4000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, {VDCR_HTOTAL, 0x7e, VDCR_HDISPLAY, 0x3c, VDCR_HSYNC, 0x5c, VDCR_VTOTAL, 0xff, VDCR_VDISPLAY, 0xfe, VDCR_VSYNC, 0x02, VDCR_LACE, 0x00, VDCR_REFRESH, 0x00, VDCR_HSCROLL, 0xc7, VDCR_DISP_ADDRH, 248, VDCR_DISP_ADDRL, 8, VDCR_ATTR_ADDRH, 63, VDCR_ATTR_ADDRL, 196, 255}},
        // VDC-IMONO: 720x700, interlace, monochrome. Timing values from
        // Tokra's original demo (vdcmodemania.bas line 110), same
        // DISP_ADDR/ATTR_ADDR/CHAR_ADDR omission as above. Bitmap alone is
        // 720/8*700 = 63000 bytes -- only ~2.5KB spare in the 64KB VDC, so
        // char_std/char_alt are left 0x0000 (no room for a charset copy);
        // vdc_set_mode() skips charset setup for any mode with char_std==0,
        // which this row's zeros trigger. HSCROLL (register 25) restored
        // from the same source table -- Tokra's 0x87 (bits 7/6 bitmap/
        // attribute enable, already applied generically by vdc_set_mode();
        // bits 0-3 = horizontal smooth-scroll amount = 7, same nibble as
        // VDC-FLI above) -- previously omitted here too.
        {720, 700, 1, 0, 1, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, {VDCR_HTOTAL, 0x7f, VDCR_HDISPLAY, 0x5a, VDCR_HSYNC, 0x6b, VDCR_SYNCSIZE, 0x89, VDCR_VTOTAL, 0x6a, VDCR_VADJUST, 0x06, VDCR_VDISPLAY, 0x6a, VDCR_VSYNC, 0x65, VDCR_LACE, 0x03, VDCR_CSIZE, 0x06, VDCR_HSCROLL, 0x87, 255}},
        // VDC-IHFLI: 640x480, interlace, 8x2 colour cells, near-NTSC (Tokra's
        // original/v12/source/vdcmodemania.bas line 42). Genuinely
        // interlaced dual-field encoding: separate top/bottom bitmap AND
        // separate top/bottom colour(attribute) data, four files per
        // picture. DISP_ADDR/ATTR_ADDR are Tokra's own literal values
        // (0x5280/0x0000); base_text/base_attr below are where this
        // project's own bnk_cpytovdc() calls target instead (top
        // bitmap/colour fields) -- bottom-field addresses (0x5780 bitmap,
        // 0x0230 colour) are literals in the demo function, same
        // convention as VDC-IMONO's 0x82c8. char_std/char_alt left 0x0000
        // (no charset room -- picture data alone is 57600 of 65536 bytes).
        // VDCR_VSYNC=0x81 found via live register comparison against
        // Tokra's original demo: every other geometry register already
        // matched exactly, VSYNC was the one difference (ours read a
        // leftover boot-baseline value, neither program had ever
        // explicitly set it for this mode) -- 0x81 = VTOTAL(0x84) -
        // VADJUST(3), confirmed live. Live-confirmed working.
        {640, 480, 1, 8, 1, 0xaa00, 0x2b70, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, {VDCR_HTOTAL, 0x7e, VDCR_SYNCSIZE, 0x89, VDCR_VTOTAL, 0x84, VDCR_VADJUST, 0x03, VDCR_VDISPLAY, 0x84, VDCR_VSYNC, 0x81, VDCR_LACE, 0x03, VDCR_CSIZE, 0x03, VDCR_DISP_ADDRH, 0x52, VDCR_DISP_ADDRL, 0x80, VDCR_ATTR_ADDRH, 0x00, VDCR_ATTR_ADDRL, 0x00, VDCR_HSCROLL, 0xc7, VDCR_CHAR_ADDRH, 0xff, VDCR_REFRESH, 0x02, 255}},
        // VDC-ITFLI: 640x576, interlace, 8x3 colour cells, near-PAL (Tokra's
        // vdcmodemania.bas line 64) -- same dual-field structure as
        // VDC-IHFLI above, just PAL-tuned timing and taller (8x3 cells).
        // DISP_ADDR/ATTR_ADDR are Tokra's own literals (0x4010/0x0000);
        // bottom-field literals (0x4100 bitmap, 0x0000 colour) belong in
        // the demo function. Tightest fit of the whole set: picture data
        // alone is 61440 of 65536 bytes, ~4KB spare -- no charset room
        // either. VDCR_VSYNC=0x63 extrapolated using the same VTOTAL-
        // VADJUST relationship that matched IHFLI's live-confirmed value:
        // VTOTAL(0x68=104) - VADJUST(5) = 99 = 0x63. Live-confirmed working.
        {640, 576, 1, 8, 1, 0xa280, 0x2080, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, {VDCR_HTOTAL, 0x7f, VDCR_HSYNC, 0x68, VDCR_SYNCSIZE, 0x89, VDCR_VTOTAL, 0x68, VDCR_VADJUST, 0x05, VDCR_VDISPLAY, 0x68, VDCR_VSYNC, 0x63, VDCR_LACE, 0x03, VDCR_CSIZE, 0x05, VDCR_DISP_ADDRH, 0x40, VDCR_DISP_ADDRL, 0x10, VDCR_ATTR_ADDRH, 0x00, VDCR_ATTR_ADDRL, 0x00, VDCR_HSCROLL, 0xc7, VDCR_CHAR_ADDRH, 0xff, VDCR_REFRESH, 0x02, 255}},
        // VDC-HFLI: 640x400, non-interlace, 8x2 colour cells (Tokra's
        // vdcmodemania.bas line 89). Simplest of the three remaining colour
        // modes -- single static bitmap+colour plane, no dual-field
        // encoding, no per-frame CSIZE toggle (CSIZE=1 is set directly in
        // this row, unlike VDC-FLI). DISP_ADDR/ATTR_ADDR are NOT set by
        // this row (Tokra's own table omits them too) -- unlike
        // VDC-IHFLI/ITFLI above, base_text/base_attr here MUST exactly
        // match what the picture needs (0x0000 bitmap/0x8000 colour), since
        // vdc_set_disp_address() (driven by these two fields) is the only
        // thing that sets those registers for this mode. Live-confirmed
        // working.
        {640, 400, 1, 8, 1, 0x0000, 0x8000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, {VDCR_HTOTAL, 0x7e, VDCR_VTOTAL, 0xd4, VDCR_VDISPLAY, 0xc9, VDCR_VSYNC, 0xc9, VDCR_CSIZE, 0x01, VDCR_ATTR_ADDRH, 0x80, VDCR_HSCROLL, 0xc7, VDCR_CHAR_ADDRH, 0xff, VDCR_REFRESH, 0x00, 255}},
        // VDC-IM800: 800x600, interlace, monochrome (Tokra's vdcmodemania.bas
        // line 158). Tokra's own readme note: needs a monitor that can
        // squeeze the image on a real C128 -- a Commodore 1901 "cannot
        // squeeze horizontally, so you will miss the left and right edges."
        // No colour plane (colorlines=0) -- base_attr unused. base_text =
        // top bitmap field (0x0000); bottom field (0x7e2c) is a literal in
        // the demo function, same convention as VDC-IMONO. COLOR (register
        // 26) = 32 here is just Tokra's initial ink/paper default -- the
        // original lets the user retune it live with cursor keys, optional
        // for a first pass. Live-confirmed working.
        {800, 600, 1, 0, 1, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, {VDCR_HTOTAL, 0x7f, VDCR_HDISPLAY, 0x64, VDCR_HSYNC, 0x70, VDCR_SYNCSIZE, 0x89, VDCR_VTOTAL, 0x5c, VDCR_VADJUST, 0x06, VDCR_VDISPLAY, 0x5c, VDCR_VSYNC, 0x57, VDCR_LACE, 0x03, VDCR_CSIZE, 0x06, VDCR_HSCROLL, 0x87, VDCR_COLOR, 0x20, VDCR_CHAR_ADDRH, 0xff, VDCR_HEND, 0x6a, 255}},
        // VDC-IM960: 960x540, interlace, monochrome (Tokra's vdcmodemania.bas
        // line 181) -- Tokra's own readme note: "specifically designed for
        // the RGBtoHDMI-device. It will probably not work otherwise" --
        // include for completeness, but don't expect this one to render
        // correctly in VICE; treat a working real-hardware/RGBtoHDMI result
        // as the actual bar for success, same spirit as other
        // real-hardware-only caveats already documented in this project.
        // Field order is reversed vs every other interlaced mode here:
        // Tokra's own "top" field (.bt) goes to the HIGHER address
        // (0x8160), "bottom" (.bb) to the LOWER (0x0000) -- base_text is
        // set to 0x0000 anyway (this mode's own row doesn't set DISP_ADDR,
        // so base_text must be wherever the frame should actually start;
        // 0x0000 matches every other mode's convention) -- the 0x8160
        // field is a literal in the demo function. Tightest fit of the
        // entire project: picture data alone is 64800 of 65536 bytes, 736
        // spare. HSTART (register 34) = 6 here is Tokra's own value, but
        // vdc_set_mode()'s final vdc_enable_display() call overwrites
        // HSTART with the captured boot baseline *after* this row's regset
        // loop runs -- the demo function must re-poke VDCR_HSTART=6 after
        // vdc_init() returns, or this mode's own intended value never
        // actually takes effect. VADJUST/CSIZE=226 look unusual (only their
        // low 5 bits are meaningful on real VDC hardware) -- transcribed
        // faithfully from Tokra's DATA statement, not yet confirmed live.
        {960, 540, 1, 0, 1, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, {VDCR_HTOTAL, 0x7f, VDCR_HDISPLAY, 0x78, VDCR_HSYNC, 0x7b, VDCR_SYNCSIZE, 0x89, VDCR_VTOTAL, 0xb8, VDCR_VADJUST, 0xe2, VDCR_VDISPLAY, 0xb8, VDCR_VSYNC, 0xb5, VDCR_LACE, 0x03, VDCR_CSIZE, 0xe2, VDCR_HSCROLL, 0x87, VDCR_COLOR, 0xf0, VDCR_CHAR_ADDRH, 0x3f, VDCR_HSTART, 0x06, VDCR_HEND, 0x7e, 255}},
        // VDC_TEXT_80x25_Mono_PAL: identical register row to
        // VDC_TEXT_80x25_PAL above -- only colorlines differs (0, not 8),
        // which disables the VDC's extended-attribute mode entirely. Needed
        // for idi8b_logo_demo(): a raster bar can only change *foreground*
        // colour when attribute mode is off (in attribute mode, each
        // character's own colour-RAM entry overrides the raster's
        // foreground nibble -- only the background nibble still applies
        // globally). Confirmed via live testing earlier this session.
        {80, 25, 0, 0, 0, 0x0000, 0x0800, 0x1000, 0x1800, 0x2000, 0x3000, 0x4000, {VDCR_HTOTAL, 0x7f, VDCR_VTOTAL, 0x26, VDCR_VADJUST, 0xe0, VDCR_VDISPLAY, 0x19, VDCR_VSYNC, 0x20, VDCR_LACE, 0xfc, VDCR_CSIZE, 0xe7, VDCR_REFRESH, 0x7e, 255}}};

char screen_width()
// Return screenwidth 40 or 80
{
    if (*(volatile char *)0xd7 >= 128)
    {
        return 80;
    }
    else
    {
        return 40;
    }
}

void screen_setmode(char mode)
// Set mode of text screen: inpout 40 or 80 for repp. VIC-II or VDC mode
{
    if (mode != screen_width())
    {
        __asm
        {
            jsr 0xff5f
        }
    }
}

void fastmode(char set)
// Set (1) or disable (0) fast 2 MHz mode. Set blanks VIC.
{
    if (set)
    {
        POKE(0xd011, PEEK(0xd011) & (~(1 << 4))); // Disable the 5th bit of the SCROLY register to blank VIC screen
        POKE(0xd011, PEEK(0xd011) & (~(1 << 7))); // Disable the 8th bit of the SCROLY register to avoid accidentally setting raster interrupt to high
        POKE(0xd030, 1);                          // Set C128 speed to FAST (2 Mhz)
    }
    else
    {
        POKE(0xd030, 0);                          // Switch to 1Mhz mode
        POKE(0xd011, PEEK(0xd011) | (1 << 4));    // Enable the 5th bit of the SCROLY register to blank VIC screen
        POKE(0xd011, PEEK(0xd011) & (~(1 << 7))); // Disable the 8th bit of the SCROLY register to avoid accidentally setting raster interrupt to high
    }
}

char pet2screen(char p)
// Convert Petscii values to screen code values
{
    if (p < 32)
        p = p + 128;
    else if (p > 63 && p < 96)
        p = p - 64;
    else if (p > 95 && p < 128)
        p = p - 32;
    else if (p > 127 && p < 160)
        p = p + 64;
    else if (p > 159 && p < 192)
        p = p - 64;
    else if (p > 191 && p < 255)
        p = p - 128;
    else if (p == 255)
        p = 94;

    return p;
}

void vdc_set_disp_address(unsigned text, unsigned attr)
// Function to set the VDC display addresses for text and attributes
{
    // Text
    vdc_reg_write(VDCR_DISP_ADDRH, text >> 8); // Set high byte of text address
    vdc_reg_write(VDCR_DISP_ADDRL, text);      // Set low byte of text address
    // Attribute
    vdc_reg_write(VDCR_ATTR_ADDRH, attr >> 8); // Set high byte of attribute address
    vdc_reg_write(VDCR_ATTR_ADDRL, attr);      // Set low byte of attribute address
}

void vdc_set_charset_address(unsigned address)
// Function to set the VDC display addresses for text and attributes
{
    vdc_reg_write(VDCR_CHAR_ADDRH, ((vdc_reg_read(VDCR_CHAR_ADDRH) & 0x10) | ((address >> 8) & 0xe0)));
}

void vdc_set_multab()
// Set multiplication table for screen width
{
    unsigned val = 0;
    for (char index = 0; index < vdc_state.height; index++)
    {
        multab[index] = val;
        val += vdc_state.width + vdc_state.disp_skip;
    }
}

char vdc_set_mode(char mode)
// Function to set one of the preset VDC modes. Returns 1=succes, 0=fail.
{
    char index = 0;

    // Check if extended memory is required and available. If not, return with code 0.
    if (vdc_modes[mode].extmem && vdc_state.memsize == 16)
    {
        return 0;
    }

    // Set screen state
    vdc_state.mode = mode;
    vdc_state.width = vdc_modes[mode].width;
    vdc_state.height = vdc_modes[mode].height;
    vdc_state.bitmap = vdc_modes[mode].bitmap;
    vdc_state.colorlines = vdc_modes[mode].colorlines;
    vdc_state.base_text = vdc_modes[mode].base_text;
    vdc_state.base_attr = vdc_modes[mode].base_attr;
    vdc_state.swap_text = vdc_modes[mode].swap_text;
    vdc_state.swap_attr = vdc_modes[mode].swap_attr;
    vdc_state.char_std = vdc_modes[mode].char_std;
    vdc_state.char_alt = vdc_modes[mode].char_alt;
    vdc_state.extended = vdc_modes[mode].extended;
    vdc_state.dispaddr_offset = 0;
    vdc_state.disp_skip = 0;

    // Set multiplication table for screen width
    if (!vdc_state.bitmap)
    {
        vdc_set_multab();
    }

    // Set VDC addresses
    vdc_disable_display();
    vdc_set_disp_address(vdc_modes[mode].base_text, vdc_modes[mode].base_attr);
    // Reverted: this used to key off "!vdc_state.bitmap" to skip charset
    // setup for any bitmap mode, on the assumption bitmap modes never render
    // text glyphs. That's too broad -- several pre-existing bitmap modes
    // (e.g. title_screen()'s VDC_HIRES_640x400_Mono_PAL, char_std=0xe000) DO
    // reserve/populate a real charset area and relied on this call to do it;
    // skipping it left stale/uninitialized VDC state behind and destabilized
    // those modes' raster bars (confirmed live: title_screen()'s bars no
    // longer held steady). char_std==0 is what actually distinguishes the
    // two new modes that truly have no charset room (VDC_HIRES_480x252_Color_PAL,
    // VDC_HIRES_720x700_Mono_PAL, both 0x0000) from every mode that has one.
    if (vdc_modes[mode].char_std != 0)
    {
        vdc_set_charset_address(vdc_modes[mode].char_std);
        vdc_restore_charsets();
    }

    index = 0;
    do
    {
        vdc_reg_write(vdc_modes[mode].regset[index++], vdc_modes[mode].regset[index++]);
    } while (vdc_modes[mode].regset[index] != 255);

    // Check if extended memory is required and not yet set. If so, set.
    if (vdc_modes[mode].extmem && !vdc_state.memextended)
    {
        vdc_set_extended_memsize();
    }

    // Set bitmap and color status
    index = vdc_reg_read(VDCR_HSCROLL);
    if (vdc_state.bitmap)
    {
        index |= VDC_BITMAP;
        vdc_state.charwidth = vdc_state.width / 8;
        vdc_state.charheight = vdc_state.height / 8;
    }
    else
    {
        index &= ~VDC_BITMAP;
    }

    if (vdc_state.colorlines)
    {
        index |= VDC_ATTRB;
    }
    else
    {
        index &= ~VDC_ATTRB;
    }

    vdc_reg_write(VDCR_HSCROLL, index);

    // VDC revision fix (Tokra's "fix for old vdc-revision",
    // original/v12/source/vdcmodemania.bas line 9020, run after applying
    // every mode's own register table, same as here): bits 0-2 of $D600 --
    // read directly, no register select needed, these are status bits, not
    // the data-ready bit 7 that vdc_read()/vdc_write() poll -- identify the
    // VDC chip revision (0 = "version 0", matching this project's own
    // c128_reference.md note that version 0 needs different reg 25
    // initialization than version 1). On a version-0 chip, force HSCROLL's
    // low 3 bits to 0 regardless of what the mode's own table just set,
    // same masking Tokra applies universally to every mode.
    if ((vdc.addr & 0x07) == 0)
    {
        vdc_reg_write(VDCR_HSCROLL, vdc_reg_read(VDCR_HSCROLL) & 0xf8);
    }

    if (!vdc_state.bitmap)
    {
        vdc_cls();
    }
    vdc_enable_display();
    return 1;
}

static void vdc_reset_boot_registers()
// Resets HDISPLAY/HSYNC/SYNCSIZE/HSCROLL/VSCROLL/HEND/DISP_ADDR/ATTR_ADDR
// back to their KERNAL boot-time baseline -- see struct VDCBootBaseline's
// comment above. Called at the start of every vdc_init() (before that
// call's own vdc_set_mode() applies its regset[] overrides on top) and by
// vdc_exit(), so no transition -- including the final return to BASIC --
// can inherit a leaked value.
//
// HEND added here after a live register-dump comparison against a known-
// working build found VDC_HIRES_640x200_Color_PAL's plasma_demo()/
// rotate_demo() reading HEND=0x6a (leaked from mono_im800_demo()'s own
// explicit VDCR_HEND=0x6a, needed for its 100-column-wide display) instead
// of the boot default 0x64 -- this was the "isolated shifted fragment" bug
// (see memory: rotate_demo_shift_bug), same class of leaked-register issue
// HDISPLAY/HSYNC/SYNCSIZE were already added here to prevent, just missed
// for HEND until IM800 (the first mode to ever touch it) existed.
{
    if (!vdc_boot.captured)
    {
        return;
    }

    vdc_reg_write(VDCR_HDISPLAY, vdc_boot.hdisplay);
    vdc_reg_write(VDCR_HSYNC, vdc_boot.hsync);
    vdc_reg_write(VDCR_SYNCSIZE, vdc_boot.syncsize);
    vdc_reg_write(VDCR_HSCROLL, vdc_boot.hscroll);
    vdc_reg_write(VDCR_VSCROLL, vdc_boot.vscroll);
    vdc_reg_write(VDCR_HEND, vdc_boot.hend);
    vdc_reg_write(VDCR_DISP_ADDRH, vdc_boot.dispaddrh);
    vdc_reg_write(VDCR_DISP_ADDRL, vdc_boot.dispaddrl);
    vdc_reg_write(VDCR_ATTR_ADDRH, vdc_boot.attraddrh);
    vdc_reg_write(VDCR_ATTR_ADDRL, vdc_boot.attraddrl);
}

void vdc_init(char mode, char extmem)
// Initialize VDC screen
{
    // Capture boot-time register values once, before this (or any) call
    // touches anything -- see struct VDCBootBaseline's comment above. On
    // every later call, reset to that captured baseline instead (first
    // call has nothing to reset yet -- it just captured the baseline
    // itself).
    if (!vdc_boot.captured)
    {
        vdc_boot.hdisplay = vdc_reg_read(VDCR_HDISPLAY);
        vdc_boot.hsync = vdc_reg_read(VDCR_HSYNC);
        vdc_boot.syncsize = vdc_reg_read(VDCR_SYNCSIZE);
        vdc_boot.hscroll = vdc_reg_read(VDCR_HSCROLL);
        vdc_boot.vscroll = vdc_reg_read(VDCR_VSCROLL);
        vdc_boot.hend = vdc_reg_read(VDCR_HEND);
        vdc_boot.dispaddrh = vdc_reg_read(VDCR_DISP_ADDRH);
        vdc_boot.dispaddrl = vdc_reg_read(VDCR_DISP_ADDRL);
        vdc_boot.attraddrh = vdc_reg_read(VDCR_ATTR_ADDRH);
        vdc_boot.attraddrl = vdc_reg_read(VDCR_ATTR_ADDRL);
        vdc_boot.hstart = vdc_reg_read(VDCR_HSTART);
        vdc_boot.captured = 1;
    }
    else
    {
        vdc_reset_boot_registers();
    }

    // Init screen colors
    vdc_bgcolor(VDC_BLACK);
    vdc_fgcolor(VDC_LYELLOW);
    vdc_state.text_attr = VDC_LYELLOW + VDC_A_ALTCHAR;

    // Detect VDC memsize and set to extended if 64 KB
    vdc_detect_mem_size();

    // Give message if 40 column screen is active and wait on key before switching to 80
    if (screen_width() == 40)
    {
        printf("switch to 80 column screen\nand press key.\n");
        getch();
        clrscr();
        screen_setmode(80);
    }

    // Set 2 Mhz mode
    fastmode(1);

    // Set screen mode
    vdc_set_mode(mode);

    // If 64 KB VDC and extmem is requested, enable it.
    if (vdc_state.memsize == 64 && extmem)
    {
        vdc_set_extended_memsize();
    }
}

void vdc_exit()
// Return to normal state of VDC
{
    fastmode(0);              // Disable fast mode
    vdc_reset_boot_registers(); // See its own comment above
    vdc_set_mode(VDC_TEXT_80x25_PAL); // Set default mode
    vdc_cls();                        // Clear screen
    bnk_exit();                       // Reset shared memory to default
}

unsigned vdc_coords(unsigned x, unsigned y)
// Function returns a VDC memory address for given x,y coords. To be added to base address for text or attributes.
{
    if (vdc_state.bitmap)
    {
        return (y * vdc_state.width) + (x / 8);
    }
    else
    {
        return multab[y] + x;
    }
}

void vdc_restore_charsets()
// Restore charsets from ROM
{
    bnk_redef_charset(vdc_state.char_std, BNK_CHARROM, (char *)0xd000, 512);
}

void vdc_detect_mem_size()
// Function to detect VDC memory size. Returns size in KB in glbal variable.
{
    // Reading register 28, safeguarding value, setting bit 4 and storing back to register 28
    char ramtype = vdc_reg_read(VDCR_CHAR_ADDRH);
    vdc_reg_write(VDCR_CHAR_ADDRH, ramtype | 0x10);

    // Writing a $00 value to VDC $1fff
    vdc_mem_write_at(0x1fff, 0x00);

    // Writing a $ff value to VDC $9fff
    vdc_mem_write_at(0x9fff, 0xff);

    // Reading back value of VDC $1fff and comparing value with $00 to see if 64KB address could be read
    // If value has remained 0, then 64 KB VDC mem is detected, else 16.
    vdc_state.memsize = (vdc_mem_read_at(0x1fff) == 0x00) ? 64 : 16;

    // Restore bit 4 of register 28
    vdc_reg_write(VDCR_CHAR_ADDRH, ramtype);

    // Do a clear screen
    vdc_cls();
}

void vdc_disable_display()
// Function to disable VDC display
{
    vdc_reg_write(VDCR_HSTART, 0x80);
}

void vdc_enable_display()
// Function to enable VDC display
{
    // Use the captured boot-time baseline (see struct VDCBootBaseline's
    // comment) instead of a bare literal -- falls back to the old
    // hardcoded 0x7d only if this somehow runs before vdc_init() ever has
    // (shouldn't happen in practice: vdc_init() captures HSTART before
    // vdc_set_mode() ever calls this for the first time).
    vdc_reg_write(VDCR_HSTART, vdc_boot.captured ? vdc_boot.hstart : 0x7d);
}

void vdc_block_fill(unsigned address, char value, char length)
// Function to flll VDC area with blockfill
// Input:   address =   start address
//          value   =   value to fill area with
//          length  =   number of positions to fill, zero based, max 255
{
    vdc_mem_addr(address);                                          // Set VDC address
    vdc_write(value);                                               // Write value to data register
    vdc_reg_write(VDCR_VSCROLL, vdc_reg_read(VDCR_VSCROLL) & 0x7f); // Clear copy bit (bit 7) of register 24
    vdc_reg_write(VDCR_DSIZE, length);                              // Set block copy length
}

void vdc_block_copy_page(unsigned dest, unsigned src, char length)
// Function to copy maximum a page within VDC memory using fast block copy
// Input: Destination (dest) amd source (src) addresses, length max 255 zero based
{
    // Set base addresses
    vdc_mem_addr(dest);                                             // Set VDC destination address
    vdc_reg_write(VDCR_VSCROLL, vdc_reg_read(VDCR_VSCROLL) | 0x80); // Set copy bit (bit 7) of registerv 24
    vdc_reg_write(VDCR_BLOCK_ADDRH, src >> 8);                      // Set high byte of source address
    vdc_reg_write(VDCR_BLOCK_ADDRL, src);                           // Set low byte of source address
    vdc_reg(VDCR_DATA);                                             // Write to VDC

    // Set length
    vdc_reg_write(VDCR_DSIZE, length); // Set length in register 30
}

void vdc_block_copy(unsigned dest, unsigned src, unsigned length)
// Function to copy multiple pages within VDC memory using fast block copy
// Input: Destination (dest) amd source (src) addresses, length zero based
{
    char pages = length / 256;    // Calculate number of pages
    char lastpage = length % 256; // Calculate length left after doing all full pages

    // Copy full pages
    for (char page = 0; page < pages; page++)
    {
        vdc_block_copy_page(dest, src, 255);
        dest += 256;
        src += 256;
    }

    // Copy length left
    vdc_block_copy_page(dest, src, lastpage);
}

void vdc_scroll_copy(unsigned dest, unsigned src, char lines, char length)
// Function to copy a window of lines by length within VDC memory to another location
// Source address is address of upper left corner
{
    for (char line = 0; line < lines; line++)
    {
        vdc_block_copy_page(dest, src, length);
        src += vdc_state.width;
        dest += vdc_state.width;
    }
}

void vdc_wipe_mem()
// Function to wipe VDC memory to avoid visible screen corruption on VDC mem lauout change
{
    unsigned address = 0;

    for (char x = 0; x < 255; x++)
    {
        vdc_block_fill(address, 0, 255);
        address += 256;
    }
    vdc_block_fill(address, 0, 255);
}

void vdc_wipe_transition()
// Wipes VDC memory as a deliberately visible black transition between
// sections. vdc_wipe_mem() on its own, called with the display still
// enabled, shows the raw block-fill mechanics in whatever the *current*
// mode's geometry happens to be while it runs (VDC_HIRES_720x700_Mono_PAL
// in particular reads that as a fast-moving fine texture, not a clean
// flash to black) -- easy to miss or misread as still more corruption
// rather than a deliberate transition. Hiding it behind a disabled display
// (the same "avoid artifacts" reasoning vdc_set_extended_memsize() already
// uses for its own internal wipe) and then holding the resulting blank
// screen for a fixed, perceptible pause once display comes back gives an
// unambiguous flash-to-black instead.
{
    char i;

    vdc_disable_display();
    vdc_wipe_mem();
    vdc_enable_display();

    // ~15 frames (~0.3s at 50Hz PAL) -- long enough to register as a
    // deliberate pause, short enough not to feel like a stall.
    for (i = 0; i < 15; i++)
    {
        vdc_pass_vblank();
    }
}

void vdc_set_extended_memsize()
// Function to set VDC in 64k memory configuration
{
    // Check if 64 KB VDC, return if 16. Also return if already set.
    if (vdc_state.memsize == 16 || vdc_state.memextended)
    {
        return;
    }

    vdc_disable_display();                                                // Disable display to not show artifacts
    vdc_wipe_mem();                                                       // Wipe memory to avoid artifacts
    vdc_reg_write(VDCR_CHAR_ADDRH, vdc_reg_read(VDCR_CHAR_ADDRH) | 0x10); // Setting memory mode to 64KB by setting bit 4 of register 28
    vdc_restore_charsets();                                               // Restore charsets from ROM
    if (!vdc_state.bitmap)
    {
        vdc_cls(); // CLear VDC screen with spaces in color ywllow
    }
    vdc_enable_display();      // Enable display again
    vdc_state.memextended = 1; // Set state flag
}

void vdc_set_default_memsize()
// Function to set VDC in default memory configuration
{
    // Check if already in default mode
    if (!vdc_state.memextended)
    {
        return;
    }

    vdc_disable_display();                                                // Disable display to not show artifacts
    vdc_wipe_mem();                                                       // Wipe memory to avoid artifacts
    vdc_reg_write(VDCR_CHAR_ADDRH, vdc_reg_read(VDCR_CHAR_ADDRH) & 0xef); // Setting memory mode to 64KB by clearing bit 4 of register 28
    vdc_restore_charsets();                                               // Restore charsets from ROM
    if (!vdc_state.bitmap)
    {
        vdc_cls(); // CLear VDC screen with spaces in color ywllow
    }
    vdc_enable_display();      // Enable display again
    vdc_state.memextended = 0; // Set state flag
}

void vdc_bgcolor(char color)
// Set VDC background color
{
    vdc_reg_write(VDCR_COLOR, (vdc_reg_read(VDCR_COLOR) & 0xf0) + color);
}

void vdc_fgcolor(char color)
// Set VDC foreground color
{
    vdc_reg_write(VDCR_COLOR, (vdc_reg_read(VDCR_COLOR) & 0x0f) + (color * 16));
}

void vdc_textcolor(char color)
// Set default text attributes
{
    // Set color while retaining bits 4-7
    vdc_state.text_attr = (vdc_state.text_attr & 0xf0) + color;
}

void vdc_altchar(char set)
// Clear (set=0) or set (set=1) alternate charset mode
{
    vdc_state.text_attr = (set) ? (vdc_state.text_attr | VDC_A_ALTCHAR) : (vdc_state.text_attr & ~VDC_A_ALTCHAR);
}

void vdc_blink(char set)
// Clear (set=0) or set (set=1) blink mode
{
    vdc_state.text_attr = (set) ? (vdc_state.text_attr | VDC_A_BLINK) : (vdc_state.text_attr & ~VDC_A_BLINK);
}

void vdc_underline(char set)
// Clear (set=0) or set (set=1) underline mode
{
    vdc_state.text_attr = (set) ? (vdc_state.text_attr | VDC_A_UNDERLINE) : (vdc_state.text_attr & ~VDC_A_UNDERLINE);
}

void vdc_reverse(char set)
// Clear (set=0) or set (set=1) reverse mode
{
    vdc_state.text_attr = (set) ? (vdc_state.text_attr | VDC_A_REVERSE) : (vdc_state.text_attr & ~VDC_A_REVERSE);
}

void vdc_printc(char x, char y, char val, char attr)
// Function to plot a char at a given coordinate
{
    unsigned address = vdc_coords(x, y);
    vdc_mem_write_at(address + vdc_state.base_text, val);
    vdc_mem_write_at(address + vdc_state.base_attr, attr);
}

void vdc_prints_attr(char x, char y, const char *string, char attr)
// Function to plot a string at a given coordinate with given attributes
{
    unsigned address = vdc_coords(x, y);
    char len = strlen(string);

    // Print text
    vdc_mem_addr(address + vdc_state.base_text);
    for (char i = 0; i < len; i++)
    {
        vdc_write(pet2screen(string[i]));
    }

    // Color
    vdc_block_fill(address + vdc_state.base_attr, attr, len - 1);
}

void vdc_prints(char x, char y, const char *string)
// Function to plot a string at a given coordinate with active attributes
{
    vdc_prints_attr(x, y, string, vdc_state.text_attr);
}

void vdc_hchar(unsigned x, unsigned y, char val, char attr, char length)
// Function to plot horizontal line using block copy
{
    unsigned address = vdc_coords(x, y);
    vdc_block_fill(address + vdc_state.base_text, val, length - 1);  // Text
    vdc_block_fill(address + vdc_state.base_attr, attr, length - 1); // Attributes
}

void vdc_vchar(unsigned x, unsigned y, char val, char attr, char length)
// Function to plot vertical line from top to bottom
{
    for (char i = y; i < y + length; i++)
    {
        vdc_printc(x, i, val, attr);
    }
}

void vdc_clear(unsigned x, unsigned y, char val, char length, unsigned lines)
// Function to clear VDC area with given value and attribute
{
    for (unsigned i = y; i < y + lines; i++)
    {
        vdc_hchar(x, i, val, vdc_state.text_attr, length);
    }
}

void vdc_cls()
// Function to clear VDC screen with given value and attribute
{
    vdc_clear(0, 0, C_SPACE, vdc_state.width, vdc_state.height);
}

void vdc_hires_colorarea(char xc, char yc, char xw, char yw, char fg, char bg)
{
    unsigned address;
    char charwidth = vdc_state.width / 8;
    char y;
    char color = bg + (16 * fg);

    address = vdc_state.base_attr + (yc * charwidth) + xc;

    for (y = 0; y < yw; y++)
    {
        vdc_block_fill(address, color, xw);
        address += charwidth;
    }
}

static __native inline void vdc_wait_vblank()
// Function to wait until VDC VBLANK
{
    // Loop while bit 5 is on
    do
    {
    } while (!(vdc.addr & 0x20));
}

static __native inline void vdc_wait_no_vblank()
// Function to wait until after VDC VBLANK
{
    // Loop while bit 5 is on
    do
    {
    } while (vdc.addr & 0x20);
}

static __native inline void vdc_pass_vblank()
// Function to include a delay to wait for first scanlines passing
{
    vdc_wait_vblank();
    vdc_wait_no_vblank();
}