#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <petscii.h>
#include <conio.h>
#include <c128/vdc.h>
#include <c128/mmu.h>
#include <c64/cia.h>
#include <c64/vic.h>
#include <c64/keyboard.h>
#include <c64/joystick.h>
#include "defines.h"
#include "banking.h"
#include "vdc_core.h"
#include "vdc_win.h"
#include "vdc_raster.h"
#include "peekpoke.h"
#include "krill.h"

// Buffer for attribute screen calculations
char Screen[4000];

// Cached PAL/NTSC state -- set once by system_diagnostic_screen() (the
// first thing main() runs), reused by vdc_row_to_rasterline() so later
// callers don't need to re-run detect_ntsc()'s own one-VIC-frame timing
// measurement.
char g_is_ntsc;

// Raster colour values
char rasterbar[13] = {VDC_BLACK, VDC_DGREY, VDC_LGREY, VDC_WHITE, VDC_DCYAN, VDC_LBLUE, VDC_DBLUE, VDC_LBLUE, VDC_DCYAN, VDC_WHITE, VDC_LGREY, VDC_DGREY, VDC_BLACK};

// Plasma demo variables
static const char sintab[] = {
	128, 131, 134, 137, 140, 144, 147, 150, 153, 156, 159, 162, 165, 168, 171, 174, 177, 179, 182, 185, 188, 191, 193, 196, 199, 201, 204, 206, 209, 211, 213, 216, 218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 237, 239, 240, 241, 243, 244, 245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254, 254, 255, 255, 255,
	255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251, 250, 250, 249, 248, 246, 245, 244, 243, 241, 240, 239, 237, 235, 234, 232, 230, 228, 226, 224, 222, 220, 218, 216, 213, 211, 209, 206, 204, 201, 199, 196, 193, 191, 188, 185, 182, 179, 177, 174, 171, 168, 165, 162, 159, 156, 153, 150, 147, 144, 140, 137, 134, 131,
	128, 125, 122, 119, 116, 112, 109, 106, 103, 100, 97, 94, 91, 88, 85, 82, 79, 77, 74, 71, 68, 65, 63, 60, 57, 55, 52, 50, 47, 45, 43, 40, 38, 36, 34, 32, 30, 28, 26, 24, 22, 21, 19, 17, 16, 15, 13, 12, 11, 10, 8, 7, 6, 6, 5, 4, 3, 3, 2, 2, 2, 1, 1, 1,
	1, 1, 1, 1, 2, 2, 2, 3, 3, 4, 5, 6, 6, 7, 8, 10, 11, 12, 13, 15, 16, 17, 19, 21, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 43, 45, 47, 50, 52, 55, 57, 60, 63, 65, 68, 71, 74, 77, 79, 82, 85, 88, 91, 94, 97, 100, 103, 106, 109, 112, 116, 119, 122, 125};
char colormap0[256], colormap1[256];
char colors0[] = {VDC_BLACK, VDC_DBLUE, VDC_LBLUE, VDC_WHITE, VDC_LGREEN, VDC_DGREEN, VDC_BLACK};
char colors1[] = {VDC_BLACK, VDC_DYELLOW, VDC_LYELLOW, VDC_WHITE, VDC_LGREY, VDC_DGREY, VDC_BLACK};
unsigned c1A, c1B, c2A, c2B, c3A, c3B;
int d1A, d1B, d2A, d2B, d3A, d3B;

// Color rotate demo variables
char colors[] = {
	VDC_BLACK | 16 * VDC_BLACK,
	VDC_BLACK | 16 * VDC_DGREY,
	VDC_DGREY | 16 * VDC_DGREY,
	VDC_DGREY | 16 * VDC_LGREY,
	VDC_LGREY | 16 * VDC_LGREY,
	VDC_LGREY | 16 * VDC_WHITE,
	VDC_WHITE | 16 * VDC_WHITE,
	VDC_WHITE | 16 * VDC_DCYAN,
	VDC_DCYAN | 16 * VDC_DCYAN,
	VDC_LBLUE | 16 * VDC_DCYAN,
	VDC_LBLUE | 16 * VDC_LBLUE,
	VDC_DBLUE | 16 * VDC_LBLUE,
	VDC_DBLUE | 16 * VDC_DBLUE,
	VDC_BLACK | 16 * VDC_DBLUE,
	VDC_BLACK | 16 * VDC_BLACK,
	VDC_BLACK | 16 * VDC_BLACK,
	VDC_BLACK | 16 * VDC_BLACK,
	VDC_BLACK | 16 * VDC_DRED,
	VDC_DRED | 16 * VDC_DRED,
	VDC_DRED | 16 * VDC_DYELLOW,
	VDC_DYELLOW | 16 * VDC_DYELLOW,
	VDC_DYELLOW | 16 * VDC_LYELLOW,
	VDC_LYELLOW | 16 * VDC_LYELLOW,
	VDC_LYELLOW | 16 * VDC_DCYAN,
	VDC_DCYAN | 16 * VDC_DCYAN,
	VDC_LGREEN | 16 * VDC_DCYAN,
	VDC_LGREEN | 16 * VDC_LGREEN,
	VDC_DGREEN | 16 * VDC_LGREEN,
	VDC_DGREEN | 16 * VDC_DGREEN,
	VDC_BLACK | 16 * VDC_DGREEN,
	VDC_BLACK | 16 * VDC_BLACK,
	VDC_BLACK | 16 * VDC_BLACK,
};

// Plasma demo routines
void init_plasma(char mode)
// Init plasma hires screen
{
	unsigned dp;
	char pattern[8] = {0x01, 0x38, 0x7c, 0x7c, 0x7c, 0x38, 0x01, 0x83};
	char y, line;

	// No wipe here -- the previous section's own exit loop already calls
	// vdc_wipe_transition() right when it detects the keypress that ends it
	// (see plasma_demo()/rotate_demo()/etc.'s own exit loops), so the wipe
	// happens right as the user acts, not buried inside this function's own
	// setup where bnk_load()/etc. can delay or mask it. Matters more here
	// than usual that SOMETHING wipes before this mode's own content loads:
	// VDC-IMONO's 63000-byte bitmap (base_text=0x0000) overlaps this mode's
	// own base_text/base_attr addresses, and only the bitmap plane below
	// gets overwritten before the main loop starts -- the attribute plane
	// is filled dynamically by doplasma0()/doplasma1(), so without a prior
	// wipe this would show IMONO's leftover bitmap bytes as noise for the
	// first frame or two.

	// Full vdc_init() (memsize re-detect, fastmode, extended-memory check),
	// not a bare vdc_set_mode() -- every other section in this file
	// transitions this way (see title_screen()/mono_colorize_demo()/
	// fli_color_demo()/mono_hires_xl_demo()/init_rotate()); this was the one
	// exception, relying on whatever the previous effect already set up.
	// That went unnoticed as long as this ran right after mono_colorize_demo()
	// (an ordinary non-interlaced mode), but the two new modes immediately
	// before this one now push far more extreme register values (VDC-IMONO:
	// interlace, VTOTAL=0x6a), so this no longer transitions reliably without
	// the full re-init.
	vdc_init(mode, 1);
	if (!vdc_state.bitmap)
	{
		return;
	}

	dp = vdc_state.base_text;

	for (y = 0; y < vdc_state.charheight; y++)
	{
		for (line = 0; line < 8; line++)
		{
			vdc_block_fill(dp, pattern[line], vdc_state.charwidth);
			dp += vdc_state.charwidth;
		}
	}

	for (int i = 0; i < 256; i++)
	{
		colormap0[i] = colors0[i / 37];
		colormap1[i] = colors1[i / 37] << 4;
	}

	memset(Screen, 0, 4000);
}

inline void doplasma(unsigned scrn)
// Perform plasma calculations
{
	char xbuf0[80], xbuf1[80];
	char ybuf0[50], ybuf1[50];

	char c2a = c2A >> 8;
	char c2b = c2B >> 8;
	char c1a = c1A >> 8;
	char c1b = c1B >> 8;

	for (char i = 0; i < vdc_state.charheight; i++)
	{
		ybuf0[i] = sintab[(c1a + c2a) & 0xff] + sintab[c1b];
		c1a += 13;
		c1b -= 5;
	}

	for (char i = 0; i < vdc_state.charwidth; i++)
	{
		xbuf0[i] = sintab[(c2a + c1b) & 0xff] + sintab[c2b];
		c2a += 11;
		c2b -= 7;
	}

	c2a = c2B >> 8;
	c2b = c3A >> 8;
	c1a = c1B >> 8;
	c1b = c3B >> 8;

	for (char i = 0; i < vdc_state.charheight; i++)
	{
		ybuf1[i] = sintab[(c1b + c2a) & 0xff] + sintab[c1a];
		c1a += 4;
		c1b -= 6;
	}

	for (char i = 0; i < vdc_state.charwidth; i++)
	{
		xbuf1[i] = sintab[(c2b + c1a) & 0xff] + sintab[c2a];
		c2a += 7;
		c2b -= 9;
	}

#pragma unroll(full)
	for (char k = 0; k < 5; k++)
	{
		char tbuf0[5], tbuf1[5];
#pragma unroll(full)
		for (char i = 0; i < 4; i++)
		{
			tbuf0[i] = ybuf0[5 * k + i + 1] - ybuf0[5 * k + i];
			tbuf1[i] = ybuf1[5 * k + i + 1] - ybuf1[5 * k + i];
		}

		for (signed char i = vdc_state.charwidth - 1; i >= 0; i--)
		{
			char t = xbuf0[i] + ybuf0[5 * k];
			char u = xbuf1[i] + ybuf1[5 * k];

#pragma unroll(full)
			for (char j = 0; j < 5; j++)
			{
				Screen[(vdc_state.charwidth * j) + (5 * vdc_state.charwidth * k) + i] = colormap0[t] | colormap1[u];
				t += tbuf0[j];
				u += tbuf1[j];
			}
		}
	}

	c1A += 8 * ((int)sintab[d1A] - 128);
	c1B += 16 * ((int)sintab[d1B] - 128);
	c2A += 8 * ((int)sintab[d2A] - 128);
	c2B += 16 * ((int)sintab[d2B] - 128);
	c3A += 6 * ((int)sintab[d3A] - 128);
	c3B += 12 * ((int)sintab[d3B] - 128);

	d1A += 3;
	d1B += rand() & 3;
	d2A += 5;
	d2B += rand() & 3;
	d3A += 2;
	d3B += rand() & 3;

	vdc_mem_addr(scrn);
	for (int pos = 0; pos < (vdc_state.charwidth * vdc_state.charheight); pos++)
	{
		vdc_write(Screen[pos]);
	}
}

void doplasma0(void)
// Calculate for first screen frame
{
	doplasma(vdc_state.base_attr);
}

void doplasma1(void)
// Calculate for second screen frame
{
	doplasma(vdc_state.swap_attr);
}

void setattraddress(unsigned address)
// Set attribute address
{
	// Attribute
	vdc_reg_write(VDCR_ATTR_ADDRH, address >> 8); // Set high byte of attribute address
	vdc_reg_write(VDCR_ATTR_ADDRL, address);	  // Set low byte of attribute address
}

void plasma_demo(char mode)
// Plasma demo main loop
{
	// Init
	init_plasma(mode);
	setattraddress(vdc_state.swap_attr);

	// Ensure no longer keypress is detected
	while (vdcwin_checkch())
	{
		;
	}

	do
	{
		doplasma0();
		vdc_wait_vblank();
		setattraddress(vdc_state.base_attr);
		doplasma1();
		vdc_wait_vblank();
		setattraddress(vdc_state.swap_attr);
		joy_poll(0);
	} while (!vdcwin_checkch() && !joyb[0]);

	// Wipe right as the keypress that ends this section is detected -- see
	// init_plasma()'s comment on why this moved here instead of living in
	// the next section's own setup.
	vdc_wipe_transition();
}

// Color rotate demo routines
void init_rotate(char mode)
// Init ghires for color rotate
{
	unsigned dp;
	char pattern[8] = {0x00, 0xd4, 0xaa, 0xd4, 0xaa, 0xd4, 0xaa, 0xff};
	char y, line;

	// No wipe here -- see init_plasma()'s comment on why this now happens
	// in the previous section's own exit loop instead.
	vdc_init(mode, 0);
	if (!vdc_state.bitmap)
	{
		return;
	}

	dp = vdc_state.base_text;

	for (y = 0; y < vdc_state.charheight; y++)
	{
		for (line = 0; line < 8; line++)
		{
			vdc_block_fill(dp, pattern[line], vdc_state.charwidth);
			dp += vdc_state.charwidth;
		}
	}

	memset(Screen, 0, 4000);
}

void rotup(char x)
// Rotate up
{
	char *dp = Screen + x;

	for (char i = 0; i < vdc_state.charheight - 1; i++)
	{
		dp[0] = dp[vdc_state.charwidth];
		dp += vdc_state.charwidth;
	}
	dp[0] = 0;
}

void rotdown(char x)
// Rotate down
{
	char *dp = Screen + (vdc_state.charwidth * (vdc_state.charheight - 1)) + x;
	for (char i = 0; i < vdc_state.charheight - 1; i++)
	{
		dp -= vdc_state.charwidth;
		dp[vdc_state.charwidth] = dp[0];
	}
	dp[0] = 0;
}

inline void rotleft(char y)
// Rotate left
{
	char *dp = Screen + vdc_state.charwidth * y;
	for (char i = 0; i < vdc_state.charwidth - 1; i++)
		dp[i] = dp[i + 1];
	dp[vdc_state.charwidth - 1] = 0;
}

inline void rotright(char y)
// Rotate right
{
	char *dp = Screen + vdc_state.charwidth * y;
	for (char i = vdc_state.charwidth - 1; i > 0; i--)
		dp[i] = dp[i - 1];
	dp[0] = 0;
}

void rotate_demo(char mode)
{
	// Init
	init_rotate(mode);

	char cy[40], cx[12];
	for (char i = 0; i < 40; i++)
		cy[i] = 0;
	for (char i = 0; i < 12; i++)
		cx[i] = 0;

	char ch = 0;

	// Ensure no longer keypress is detected
	while (vdcwin_checkch())
	{
		;
	}

	do
	{

		for (char i = 0; i < 40; i++)
		{
			cy[i] += i + 1;
			if (cy[i] >= vdc_state.charheight - 1)
			{
				cy[i] -= vdc_state.charheight - 1;
				rotdown(39 - i);
				rotup(40 + i);
			}
		}

#pragma unroll(full)
		for (signed char i = 11; i >= 0; i--)
		{
			cx[i] += i + 1;
			if (cx[i] >= 7)
			{
				cx[i] -= 7;
				rotleft(11 - i);
				rotright(13 + i);
			}
		}

		for (char i = 0; i < 40; i++)
			Screen[vdc_state.charwidth * 12 + i] = colors[((ch + i) >> 2) & 31];
		ch++;

		vdc_mem_addr(vdc_state.base_attr);
		for (int pos = 0; pos < 2000; pos++)
		{
			vdc_write(Screen[pos]);
		}

		joy_poll(0);
	} while (!vdcwin_checkch() && !joyb[0]);

	// Wipe right as the keypress that ends this (last) section is detected
	// -- see init_plasma()'s comment. Nothing visual follows this before
	// vdc_exit() returns to BASIC, but this keeps the convention consistent
	// and leaves a clean VDC state either way.
	vdc_wipe_transition();
}

// Raster
void raster_place_test()
{
	static const char gradient16[16] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
	char rasterline = 100;
	char line;
	char keypress = 0;

	// This function used to inherit whatever mode the caller (main_menu())
	// left the VDC in, which was always VDC_TEXT_80x25_PAL (attribute mode
	// ON, so vdc_prints()'s text came out in vdc_state.text_attr's per-
	// character colour) -- fine until main_menu() switched to
	// VDC_TEXT_80x25_Mono_PAL for its own raster-driven header/highlight.
	// Inheriting mono mode here left every vdc_prints() call using
	// whatever the last raster_bar_*() write happened to leave in the
	// single global VDCR_COLOR register instead (often black foreground),
	// hence the black-ink regression. Set the mode explicitly instead of
	// relying on inherited state, same as every other demo section does.
	vdc_init(VDC_TEXT_80x25_PAL, 1);
	raster_calibrate();

	// Clear first -- entered from the menu (or looped back to from a
	// previous ESC/STOP), whose own text would otherwise still show through
	// in every column this function doesn't explicitly overwrite (it only
	// ever writes columns 0-1 for the line-number gutter plus a handful of
	// short strings, never a full-width vdc_cls()-equivalent of its own).
	vdc_cls();

	// Print line numbers and instructions
	for (char i = 0; i < 25; i++)
	{
		sprintf(linebuffer, "%2u", i);
		vdc_prints(0, i, linebuffer);
	}

	vdc_prints(5, 0, "Rasterline placement test.");
	sprintf(linebuffer, "Measured: VDC rasterline takes %2u.%03u cycles (timer reload %2u).",
			raster_cycles_per_line_x1000 / 1000, raster_cycles_per_line_x1000 % 1000, raster_timer_reload);
	vdc_prints(5, 1, linebuffer);
	vdc_prints(5, 2, "Move bar with CURSOR UP and CURSOR DOWN.");
	vdc_prints(5, 3, "Press ESC to exit.");
	vdc_prints(5, 5, "Present rasterline:");

	// Ensure that no keypress is still in buffer
	while (vdcwin_checkch())
	{
		;
	}

	do
	{
		// Print present raster line
		sprintf(linebuffer, "%3u", rasterline);
		vdc_prints(25, 5, linebuffer);

		// Draw raster line
		line = rasterline;
		raster_bar_begin();
		line = raster_bar_segment(line, gradient16, 16);
		raster_bar_end();

		// Check keys -- joystick up/down mirrors the cursor keys (no
		// debounce needed here, unlike the main menu: holding a direction
		// to scrub quickly through the range is desirable, and there's no
		// "wrong item selected" failure mode with a single movable bar).
		joy_poll(0);
		keypress = vdcwin_checkch();
		if ((keypress == CH_CURS_DOWN || joyy[0] == 1) && rasterline > 16)
		{
			rasterline--;
		}
		if ((keypress == CH_CURS_UP || joyy[0] == -1) && rasterline < 255)
		{
			rasterline++;
		}

	} while (keypress != CH_ESC && keypress != CH_STOP);

	// Wipe right as the keypress that ends this section is detected -- see
	// init_plasma()'s comment. Supersedes the plain vdc_cls() this used to
	// end with (a visible transition into title_screen(), not just a clear).
	vdc_wipe_transition();
}

void vdc_header_bar(const char *subtitle)
// Two-row reverse-video header bar, matching UltimateDemo2026's
// screen_init()/header_line() convention (see that project's src/screen.c):
// row 0 is the fixed project title+tagline (left) plus the site credit
// (right), row 1 the screen-specific subtitle (centred). Both rows are
// full-width solid-colour bars (vdc_hchar() fills both the text plane,
// with C_SPACE, and the attribute plane, with the reverse-video colour, in
// one call -- same primitive vdc_menu.c's own dormant menu_placeheader()/
// menu_placebar() already use). Called at the top of every menu/info/
// diagnostic text screen, right after vdc_cls() -- body text starts at
// row 3 or later, same as before this convention existed. Title text is
// 50 chars, credit is 17, right-aligned at column 61-77 -- an ~10-column
// gap between them at 80 columns, checked when the title was lengthened
// from plain "VDC Maniac" to include the tagline (title_screen()'s own
// "Experiments with C128's greatest asset").
{
	static const char title[] = "VDC Maniac: Experiments with C128's greatest asset";
	static const char credit[] = "IDreamIn8Bits.com";
	char x;

	vdc_hchar(0, 0, C_SPACE, VDC_DGREEN | VDC_A_REVERSE | VDC_A_ALTCHAR, 80);
	vdc_prints_attr(1, 0, title, VDC_DGREEN | VDC_A_REVERSE | VDC_A_ALTCHAR);
	x = (char)(78 - strlen(credit));
	vdc_prints_attr(x, 0, credit, VDC_DGREEN | VDC_A_REVERSE | VDC_A_ALTCHAR);

	vdc_hchar(0, 1, C_SPACE, VDC_LGREEN | VDC_A_REVERSE | VDC_A_ALTCHAR, 80);
	x = (char)((80 - strlen(subtitle)) / 2);
	vdc_prints_attr(x, 1, subtitle, VDC_LGREEN | VDC_A_REVERSE | VDC_A_ALTCHAR);
}

void diag_line(char row, const char *label, const char *badge, char badgecolor, const char *detail)
// One row of a UltimateDemo2026-style "label : [BADGE] detail" diagnostic
// line (see that project's screen_result()) -- label in the ambient body
// colour, badge in badgecolor (reverse-free, just a coloured tag), detail
// alongside it. Uses its own local buffer for the label, never the shared
// global `linebuffer` -- callers commonly pass a `linebuffer`-built string
// as `detail` itself, and formatting the label into the same shared buffer
// here would clobber that string before it gets printed.
{
	char label_buf[24];
	sprintf(label_buf, "%-20s:", label);
	vdc_prints(5, row, label_buf);
	vdc_prints_attr(26, row, badge, badgecolor | VDC_A_ALTCHAR);
	vdc_prints(33, row, detail);
}

void vdc_mode_info_screen(const char *modename, const char *line1, const char *line2, const char *line3, const char *line4)
// Shows a text-mode "specs" screen for the upcoming VDC Mode Mania mode --
// same idea as Tokra's own original BASIC demo, which prints a screen like
// this (mode name, resolution, colour depth, interlace, refresh rate)
// before loading each mode's picture (see original/v12/source/
// vdcmodemania.bas, e.g. line 45 for VDC-IHFLI). Called at the very start
// of each of the 7 VDC-Mode-Mania-recreation demo functions, before their
// own bnk_load() calls -- so this text is what's actually on screen while
// the (comparatively slow) disk load happens, instead of a blank/wiped
// screen with nothing to look at. line1-4 are optional (pass 0/NULL for
// unused ones); modename and line1 are expected to always be given.
{
	vdc_init(VDC_TEXT_80x25_PAL, 0);
	vdc_cls();
	vdc_header_bar(modename);
	if (line1)
	{
		vdc_prints(5, 5, line1);
	}
	if (line2)
	{
		vdc_prints(5, 6, line2);
	}
	if (line3)
	{
		vdc_prints(5, 7, line3);
	}
	if (line4)
	{
		vdc_prints(5, 8, line4);
	}
	vdc_prints(5, 11, "Loading...");
}

void raster_bar(char upper, char lower, char length)
{
	char count;
	char position = upper;
	char line;
	signed char direction = -1;

	char fgcolor = VDC_WHITE * 16;

	for (count = 0; count < length; count++)
	{
		rasterbar[count] += fgcolor;
	}

	// Ensure that no keypress is still in buffer
	while (vdcwin_checkch())
	{
		;
	}

	do
	{
		line = position;
		raster_bar_begin();
		line = raster_bar_segment(line, rasterbar, length);
		raster_bar_end();

		position = raster_bar_bounce(position, upper, lower, &direction);

	} while (!vdcwin_checkch());
}

// void raster_hiressplit()
//{
//
//	// Ensure that no keypress is still in buffer
//	while (vdcwin_checkch())
//	{
//		;
//	}
//
//	do
//	{
//		__asm
//		{
//			lda #$20
//		rhs_w1:
//			bit $d600
//			beq rhs_w1
//
//		rhs_w2:
//			bit $d600
//			bne rhs_w2
//
//			lda #$0d
//			ldx #$04
//			stx $d600
//		rhs_w3:
//			bit $d600
//			bpl rhs_w3
//			sta $d601
//
//			lda #$0a
//			ldx #$06
//			stx $d600
//		rhs_w4:
//			bit $d600
//			bpl rhs_w4
//			sta $d601
//
//			lda #$05
//			ldx #$07
//			stx $d600
//		rhs_w5:
//			bit $d600
//			bpl rhs_w5
//			sta $d601
//
//			lda #$48
//			ldx #$12
//			stx $d600
//		rhs_w6:
//			bit $d600
//			bpl rhs_w6
//			sta $d601
//
//			lda #$20
//		rhs_w7:
//			bit $d600
//			beq rhs_w7
//
//		rhs_w8:
//			bit $d600
//			bne rhs_w8
//
//			lda #$0c
//			ldx #$04
//			stx $d600
//		rhs_w9:
//			bit $d600
//			bpl rhs_w9
//			sta $d601
//
//			lda #$0a
//			ldx #$06
//			stx $d600
//		rhs_w10:
//			bit $d600
//			bpl rhs_w10
//			sta $d601
//
//			lda #$40
//			ldx #$07
//			stx $d600
//		rhs_w11:
//			bit $d600
//			bpl rhs_w11
//			sta $d601
//
//			lda #$00
//			ldx #$12
//			stx $d600
//		rhs_w12:
//			bit $d600
//			bpl rhs_w12
//			sta $d601
//		};
//		//vdc_wait_vblank();					// Wait for VBLANK=1
//		//vdc_wait_no_vblank();				// Wait for VBLANK=0 to display first half frame
//		//vdc_reg_write(VDCR_VTOTAL,0x13);	// Vertical total to 19*8 lines
//		//vdc_reg_write(VDCR_VTOTAL,0x0d);	// Vertical displayed to 13 line for second half frame
//		//vdc_reg_write(VDCR_VSYNC,0x40);		// Turn off vertical synch at position 16
//		//vdc_reg_write(VDCR_ADDRH,0x48);		// First screen for first frame
//		//vdc_wait_vblank();					// Wait for VBLANK=1
//		//vdc_wait_no_vblank();				// Wait for VBLANK=0 to display second half frame
//		//vdc_reg_write(VDCR_VTOTAL,0x13);	// Vertical total to 19*8 lines
//		//vdc_reg_write(VDCR_VTOTAL,0x0c);	// Vertical displayed to 13 line for second half frame
//		//vdc_reg_write(VDCR_VSYNC,0x10);		// Turn on vertical synch at position 16
//		//vdc_reg_write(VDCR_ADDRH,0x48);		// Second screen for first frame
//	} while (!vdcwin_checkch());
// }

void title_screen()
{
	char color[16] = {
			VDC_DGREY | 16 * VDC_DPURPLE,
			VDC_DGREY | 16 * VDC_LPURPLE,
			VDC_DGREY | 16 * VDC_LBLUE,
			VDC_DGREY | 16 * VDC_DBLUE,
			VDC_DGREY | 16 * VDC_LBLUE,
			VDC_DGREY | 16 * VDC_DCYAN,
			VDC_DGREY | 16 * VDC_LCYAN,
			VDC_DGREY | 16 * VDC_DCYAN,
			VDC_DGREY | 16 * VDC_LGREEN,
			VDC_DGREY | 16 * VDC_DGREEN,
			VDC_DGREY | 16 * VDC_LGREEN,
			VDC_DGREY | 16 * VDC_DYELLOW,			
			VDC_DGREY | 16 * VDC_LYELLOW,
			VDC_DGREY | 16 * VDC_DYELLOW,
			VDC_DGREY | 16 * VDC_LRED,
			VDC_DGREY | 16 * VDC_DRED};
	char rastercolors[76];
	char start = 0;
	char i;
	char line,count;

	// Load screen -- asset-loading-roadmap.md Phase 1 proof, now folded into
	// Phase 2's full rollout: krill_loadcode()/krill_init()/krill_done() are
	// installed/torn down once in main() (see its own comment) rather than
	// per-function -- every load call site in this file is just the load
	// itself (see idi8b_logo_demo()'s comment on this pattern).
	// krill-loader-integration.md's plan recommended a cia_init() call
	// after every Krill load as a "free" safety net -- tried, and it isn't
	// free: cia_init() (c64/cia.c) unconditionally sets cia2.pra = 0x07,
	// directly overwriting krill_init()'s own cia2.pra = 2 (the IEC bus
	// control lines Krill's loader protocol depends on for its whole active
	// session, install to done) -- confirmed live as the cause of a hang
	// partway through the demo once more than one section had loaded via
	// Krill. Oscar64Test's own reference usage never calls cia_init()
	// during an active Krill session either (only once, before
	// krill_loadcode()/krill_init() even run) -- removed here to match.
	// TSCrunch-compressed via krill_loadcompd() -- see Makefile's
	// KRILL_COMPRESSED_ASSETS comment for how titleevk/titleodk were derived
	// from vdce-scrtit.eve/.odd (same content, re-baked destination header).
	// Named even/odd, not top/bottom: these are interlace fields (even
	// source rows / odd source rows), not a physical top-half/bottom-half
	// split -- see tools/vdc_convert.py's own comment on this mode.
	if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, "titleevk"))
	{
		printf("krill loadcompd failed: titleevk\n");
		exit(1);
	}
	if (krill_loadcompd(BNK_1_IO, MEM_SCREEN + 16000, "titleodk"))
	{
		printf("krill loadcompd failed: titleodk\n");
		exit(1);
	}

	// Init proper hires mode
	// Must match the resolution the vdce-scrtit.eve/.odd assets were
	// exported for: 640x400 mono (32000 bytes total, 16000/half). An
	// in-session experiment switched this to VDC_HIRES_640x480_Mono_NTSC
	// (with the load offset bumped to 19200 to match that taller mode's
	// framebuffer) without regenerating the assets for the new height --
	// that left a stale/garbage gap between the top and bottom halves in
	// VRAM (19200-16000 unfilled bytes) and reinterpreted 400-line bitmap
	// data as a 480-line layout (VDC bitmap memory is organized in
	// per-character-row blocks, so the row layout differs by height),
	// which is what caused the corruption/instability seen in VICE.
	// No wipe here -- raster_place_test()'s own exit loop already wiped
	// right as it detected the keypress that ends it (see init_plasma()'s
	// comment for why this convention moved there).
	vdc_init(VDC_HIRES_640x400_Mono_PAL, 1);

	// Deliberately NOT recalibrating here: this function's bar-position
	// constants (182, 181, 176..117, 114, 85, 84 below) were hand-tuned
	// against the static default raster_timer_reload (62, see
	// vdc_raster.c), long before raster_calibrate() existed -- not against
	// a live measurement. A recalibration pass was tried here and made the
	// bars unstable, most likely because this mode's interlace flag (the
	// "1" above) throws off raster_calibrate()'s VTOTAL/CSIZE-based
	// lines-per-frame math, producing a value that no longer matches what
	// these constants assume. main()'s one-time text-mode calibration
	// (62.218, confirmed via raster_place_test()) stays close enough to
	// the hand-tuned default not to matter here.

	// Copy data to VDC
	bnk_cpytovdc(vdc_state.base_text, BNK_1_FULL, (char *)MEM_SCREEN, 0x8000);

	// Ensure that no keypress is still in buffer
	while (vdcwin_checkch())
	{
		;
	}

	count=0;
	for (i = 0; i < 76; i++)
	{
		rastercolors[i] = color[count];
		count++;
		count &= 0x0f;
	}

	do
	{
		start++;
		start &= 0xf;

		// Bar positions retuned for the new "VDC Maniac" picture (the old
		// values -- 182/181/176/114/85/84 -- were hand-tuned against the
		// old "VDC Experience" picture's own content boundaries, which sit
		// at different rows). Derived by measuring per-row pixel density in
		// the new vdce-scrtit.eve/.odd to find the actual near-blank gaps
		// between the top art / "VDC Maniac" text / "Experiments with..."
		// text / bottom art, then mapping row -> raster-line using the old
		// picture's own two known-good anchor points (182 <-> its top gap's
		// centre row, 85 <-> its bottom gap's centre row) as a linear
		// calibration, applied to the new picture's analogous rows -- then
		// the top line nudged down 4 more (188, not 192) after a live
		// check in VICE confirmed everything else. Live-confirmed working.
		raster_bar_begin();
		raster_bar_line(188, VDC_DRED);
		raster_bar_line(187, 16 * VDC_LYELLOW + VDC_DGREY);
		line = 186;
		line = raster_bar_segment(line, &rastercolors[start], 60);
		raster_bar_line(120, 16 * VDC_LCYAN + VDC_DGREY);
		raster_bar_line(89, VDC_DRED);
		raster_bar_line(88, 16 * VDC_WHITE + VDC_BLACK);
		raster_bar_end();
		joy_poll(0);
	} while (!vdcwin_checkch() && !joyb[0]);

	// Wipe right as the keypress that ends this section is detected -- see
	// init_plasma()'s comment.
	vdc_wipe_transition();
}

void idi8b_logo_demo()
// Showcases the idreamtin8bits.com logo (PETSCII text screen, standard
// charset, exported from VDC Screen Editor as monochrome white -- see
// assets/idi8blogo.scrn, copied verbatim from
// idreamtin8bits-astro/src/assets/idi8b-80.scrn.prg) with a single animated
// Mechanism-1 raster bar "behind" the logo: recolours only the background
// nibble, foreground fixed white, so the logo stays legible against a
// changing background.
//
// THIRD RESET (2026-07-22): two bars -> one bar -> added raster_calibrate()
// -> dropped raster_bar_bounce() for a fixed position -- ALL still
// unstable (see git history for each attempt). Dropping in the bar code
// from raster_place_test() ("section 1") UNCHANGED, in VDC_TEXT_80x25_PAL
// (attribute mode on, the mode that function itself is proven stable in)
// FIXED it -- confirmed live. That isolated the untested VDC_TEXT_80x25_
// Mono_PAL (attribute mode off) combination as the actual variable, not
// bar count/calibration/bounce.
//
// FOURTH STEP (confirmed stable live): kept this exact bar code, switched
// only the mode back to VDC_TEXT_80x25_Mono_PAL -- stable. So Mono_PAL was
// never the culprit; the earlier unstable versions must have been broken
// by something else that's no longer present.
//
// FIFTH STEP: stable, but the logo text outside the bar's own lines was
// showing black ink. Root cause: in this non-attribute mode there's no
// per-character colour RAM, so $1A/VDCR_COLOR is a single GLOBAL register
// -- raster_bar_end() never resets it, so whatever the bar's *last* write
// was (gradient16's final entry, 0 = black-on-black) keeps applying to
// every scanline the bar itself doesn't touch, above and below it, same
// as how title_screen()'s own "cap line" raster_bar_line() calls bookend
// its segments for the same reason. Bookend the segment with explicit
// white-ink raster_bar_line() calls instead.
//
// SIXTH STEP: moved to be the second section in main(), right after
// raster_place_test() (previously ran after title_screen()) -- runs in the
// same VDC_TEXT_80x25_* family raster_place_test() itself uses, so no mode
// detour through a bitmap mode in between. Added a "PRESENTS...." line
// below the logo, and centred the whole block (logo + line) both
// horizontally and vertically on the 80x25 screen: idi8blogo.scrn's own
// exported content is top-left anchored (rows 1-12, cols 1-54 -- computed
// by inspecting the raw file), so instead of dumping it 1:1 via a single
// bnk_cpytovdc() like before, only that content's bounding box is copied,
// row by row, into a computed centred destination position.
//
// SEVENTH STEP: the "presents...." text was written UPPERCASE in the C
// source on a theory (uppercase source -> case-inverted lowercase glyph,
// derived from cross-referencing idi8blogo.scrn's own raw codes against
// pet2screen()'s ASCII mapping) that turned out wrong live -- came out
// uppercase on screen, not lowercase. Flipped to a plain lowercase source
// string instead, empirically matching what's actually wanted, rather
// than re-deriving the theory further.
{
	// NINTH STEP: single bouncing bar -> two mirrored bars ("Fire" always
	// physically upper, "Ice" always physically lower, both centred on
	// CENTER and driven by one shared bounce value t). They meet/overlap
	// ("collide") near t==0 and separate toward the screen edges near
	// t==AMPLITUDE. Which one is rendered "in front of" the logo (solid
	// flash, wins any overlap) vs "behind" it (white ink, logo stays
	// legible) is not fixed to either bar -- fire_is_front toggles every
	// time they fully collide, so the two swap roles pass to pass. Built as
	// a single merged per-frame colour buffer (resolving overlap by however
	// fire_is_front says to) fed into the existing raster_bar_segment(), so
	// this needed zero changes to vdc_raster.c -- see git history for the
	// design note (plan "want-to-revisit-timning-zany-patterson.md" at the
	// time of writing).
	static const char fire[16] = {
		VDC_WHITE, VDC_WHITE, VDC_LYELLOW, VDC_LYELLOW, VDC_DYELLOW, VDC_DYELLOW,
		VDC_LRED, VDC_LRED, VDC_LRED, VDC_DRED, VDC_DRED, VDC_DRED,
		VDC_BLACK, VDC_BLACK, VDC_BLACK, VDC_BLACK};
	static const char ice[16] = {
		VDC_WHITE, VDC_WHITE, VDC_LCYAN, VDC_LCYAN, VDC_DCYAN, VDC_DCYAN,
		VDC_LBLUE, VDC_LBLUE, VDC_LBLUE, VDC_DBLUE, VDC_DBLUE, VDC_DBLUE,
		VDC_BLACK, VDC_BLACK, VDC_BLACK, VDC_BLACK};
	enum
	{
		DEFAULTCOLOR = (VDC_WHITE * 16) | VDC_BLACK,
		BARLEN = 16,
		CENTER = 158,
		AMPLITUDE = 80,
		CYCLE_FRAMES = 6,
		BARBUFLEN = 2 * AMPLITUDE + BARLEN,
		// Bounding box of idi8blogo.scrn's own logo content (rows/cols
		// 0-indexed), found by inspecting the raw exported screen codes --
		// the file itself is a full top-left-anchored 80x25 screen, not
		// pre-centred.
		SRC_MINROW = 1,
		SRC_MAXROW = 12,
		SRC_MINCOL = 1,
		CONTENTWIDTH = 54,
		CONTENTHEIGHT = SRC_MAXROW - SRC_MINROW + 1, // 12
		// Centred destination: (80-54)/2 = 13 columns each side; block
		// height = logo (12 rows) + 1 blank gap row + 1 text row = 14,
		// (25-14)/2 = 5 rows above.
		DESTCOL = (80 - CONTENTWIDTH) / 2,
		DESTROW = (25 - (CONTENTHEIGHT + 2)) / 2,
		ROWSHIFT = DESTROW - SRC_MINROW,
		PRESENTSROW = DESTROW + CONTENTHEIGHT + 1
	};
	static const char presentstext[] = "presents....";
	static char barcolors[BARBUFLEN];
	char t = AMPLITUDE;
	signed char direction = -1;
	char fire_is_front = 1;
	char cycle_offset = 0;
	char cycle_counter = 0;
	char upperTop, lowerTop, upperBottom, lowerBottom;
	char count, i;
	char inUpper, inLower;
	char idx;
	char line;
	char r;

	// No wipe here -- see init_plasma()'s comment on why this now happens
	// in the previous section's own exit loop instead (title_screen()'s).
	vdc_init(VDC_TEXT_80x25_Mono_PAL, 0);

	// Recalibrate for this mode -- this function now runs right after
	// raster_place_test() (VDC_TEXT_80x25_PAL), switching to a genuinely
	// different mode of its own (VDC_TEXT_80x25_Mono_PAL, colorlines
	// differs). Matches mono_colorize_demo()/mono_hires_xl_demo()'s
	// established pattern of recalibrating fresh after their own
	// vdc_init().
	raster_calibrate();

	// Real fix for the earlier uppercase-logo bug (kept from before): see
	// git history / memory (vdc_charset_selection_no_attribute_mode) for
	// why vdc_set_charset_address(char_alt) can't work here -- overwrite
	// char_std's own contents with the alternate/lowercase ROM image
	// directly instead, exactly like the KERNAL's own SHIFT+COMMODORE
	// 80-column toggle does.
	bnk_redef_charset(vdc_state.char_std, BNK_CHARROM, (char *)0xd800, 256);

	// idi8b-80.scrn.prg is VDC Screen Editor's own .scrn format: [2000
	// screen codes][48-byte signature][2000 attribute bytes] (see
	// VDCScreenEditor2/ARCHITECTURE.md's "Screen Map Format"). Only the
	// screen codes are needed here -- attribute mode is off in this mode,
	// so the attribute bytes (all-white anyway, per the file's own name)
	// would never be read. Loaded into the CPU staging buffer only --
	// copied to VDC memory below, row by row, at the centred position,
	// not with a single 1:1 bnk_cpytovdc() like before.
	// asset-loading-roadmap.md Phase 2: full Krill rollout, following
	// title_screen()'s Phase 1 proof -- krill_loadcode()/krill_init()/
	// krill_done() are now installed/torn down once in main() instead of
	// per-function (see main()'s own comment), so every load call site
	// here is just the load itself -- see title_screen()'s comment for why
	// there's no cia_init() call after it (the plan's own "safety net"
	// recommendation, disproven live: it conflicts with Krill's own
	// cia2.pra usage).
	// asset-loading-roadmap.md Phase 4 rollout: TSCrunch-compressed via
	// krill_loadcompd() instead of the raw krill_load() above -- idi8bcmp is
	// idi8blogo.scrn's payload, re-baked to load at MEM_SCREEN ($4000, this
	// function's actual destination -- the checked-in asset's own header was
	// a placeholder $8000) and compressed (see Makefile's
	// KRILL_COMPRESSED_ASSETS comment for the exact conversion steps).
	if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, "idi8bcmp"))
	{
		printf("krill loadcompd failed: idi8bcmp\n");
		exit(1);
	}

	vdc_cls();
	for (r = SRC_MINROW; r <= SRC_MAXROW; r++)
	{
		bnk_cpytovdc(vdc_coords(DESTCOL, r + ROWSHIFT) + vdc_state.base_text,
					 BNK_1_FULL,
					 (char *)MEM_SCREEN + (unsigned)r * 80 + SRC_MINCOL,
					 CONTENTWIDTH);
	}
	vdc_prints((80 - sizeof(presentstext) + 1) / 2, PRESENTSROW, presentstext);

	// EIGHTH STEP (kept from the single-bar version): ceiling is 255, not
	// just a chosen bound -- raster_waitline() compares its line argument
	// against $dd06, CIA2 Timer B's LOW byte only, an 8-bit register -- so
	// `line` parameters are `char` throughout this whole raster library.
	// Floor of 60 likewise carried over unchanged. CENTER/AMPLITUDE above
	// were picked so upperTop's max (238) and lowerBottom's min (63) both
	// stay safely inside this proven 60-255 range. $1a/VDCR_COLOR is never
	// reset between frames (confirmed by the single-bar version's own
	// EIGHTH STEP investigation) -- one priming write here, before the loop
	// starts, covers the very first frame; every frame's own trailing
	// raster_bar_line(line, DEFAULTCOLOR) call below (right after the merged
	// segment) carries DEFAULTCOLOR forward through vblank into the next
	// frame's top portion, all the way up to wherever that frame's own
	// segment starts.
	raster_bar_begin();
	raster_bar_line(255, DEFAULTCOLOR);
	raster_bar_end();

	while (vdcwin_checkch())
	{
	}

	do
	{
		t = raster_bar_bounce(t, 0, AMPLITUDE, &direction);
		if (t == 0)
		{
			fire_is_front = !fire_is_front;
		}

		upperTop = CENTER + t;
		lowerTop = CENTER - t;
		upperBottom = upperTop - (BARLEN - 1);
		lowerBottom = lowerTop - (BARLEN - 1);
		count = upperTop - lowerBottom + 1;

		if (++cycle_counter >= CYCLE_FRAMES)
		{
			cycle_counter = 0;
			cycle_offset = (cycle_offset + 1) % BARLEN;
		}

		for (i = 0; i < count; i++)
		{
			line = upperTop - i;
			inUpper = (line >= upperBottom);
			inLower = (line <= lowerTop && line >= lowerBottom);

			if (inUpper && (fire_is_front || !inLower))
			{
				idx = fire[(upperTop - line + cycle_offset) % BARLEN];
				barcolors[i] = fire_is_front ? (idx * 16) | idx : (VDC_WHITE * 16) | idx;
			}
			else if (inLower)
			{
				idx = ice[(lowerTop - line + cycle_offset) % BARLEN];
				barcolors[i] = fire_is_front ? (VDC_WHITE * 16) | idx : (idx * 16) | idx;
			}
			else
			{
				barcolors[i] = DEFAULTCOLOR;
			}
		}

		raster_bar_begin();
		line = raster_bar_segment(upperTop, barcolors, count);
		raster_bar_line(line, DEFAULTCOLOR);
		raster_bar_end();
		joy_poll(0);
	} while (!vdcwin_checkch() && !joyb[0]);

	// Wipe right as the keypress that ends this section is detected -- see
	// init_plasma()'s comment.
	vdc_wipe_transition();
}

void mono_colorize_demo()
// Demonstrates the CIA1 raster+music IRQ (raster_music_irq_start()):
// colours a monochrome hires picture with a per-line ink/paper gradient,
// via a real interrupt rather than raster_synch()/raster_waitline()'s
// busy-wait, so the CPU is free between colour writes. No real picture
// asset is wired into this project yet (see ARCHITECTURE.md), so a plain
// filled placeholder bitmap stands in for one -- the point here is proving
// the per-line colouring mechanism, not the picture content. No SID tune
// is loaded either, so music is left disabled (see raster_music_irq_start()).
//
// Runs for a fixed duration (DEMOFRAMES below) rather than waiting for a
// keypress -- see raster_music_irq_start()'s header comment: no keypress
// detection was ever found to work reliably while this mechanism is
// active, from foreground code or from inside the ISR (see memory:
// mono_colorize_keypress_bug). Any leftover buffered keypress is drained
// via vdcwin_checkch() only after the effect stops and KERNAL banking is
// back, matching mono_hires_xl_demo()'s own approach of not fighting this
// mechanism's limitation.
{
	enum
	{
		DEMOFRAMES = 300 // ~6 seconds at 50Hz PAL
	};
	static char colortable[400];
	unsigned i;
	unsigned dp;
	char pattern[8] = {0xff, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xff};
	char y, line;

	// No wipe here -- see init_plasma()'s comment on why this now happens
	// in the previous section's own exit loop instead (title_screen()'s,
	// since this function is unused/dead code -- see main(), never
	// re-enabled after Mechanism 2 was dropped for it).
	vdc_init(VDC_HIRES_640x400_Mono_PAL, 0);
	if (!vdc_state.bitmap)
	{
		return;
	}

	// Recalibrate for this mode -- see the comment in title_screen(); this
	// mode's VTOTAL/CSIZE (and PAL clock) differ from both text mode and
	// title_screen()'s NTSC hires mode, so raster_music_irq_start()'s reload
	// math (which reads raster_cycles_per_line_x1000) needs a fresh value.
	raster_calibrate();

	// Placeholder bitmap: a repeated box pattern.
	dp = vdc_state.base_text;
	for (y = 0; y < vdc_state.charheight; y++)
	{
		for (line = 0; line < 8; line++)
		{
			vdc_block_fill(dp, pattern[line], vdc_state.charwidth);
			dp += vdc_state.charwidth;
		}
	}

	// Per-line colour gradient across the picture height, reusing the
	// existing rasterbar[] palette.
	for (i = 0; i < 400; i++)
	{
		colortable[i] = VDC_BLACK | 16 * rasterbar[i % 13];
	}

	while (vdcwin_checkch())
	{
	}

	// One throwaway poll first, still under normal KERNAL banking (before
	// raster_music_irq_start() below ever touches CIA1/keyboard) --
	// keyb_poll()'s own keyb_matrix[] (its press/release transition
	// history) is a global that starts BSS-zero-initialized -- all zeros --
	// but the correct "nothing pressed" baseline for this matrix convention
	// is all-ones (rows float high, pulled low only when a key closes the
	// circuit). This is the very first place in the whole program that
	// calls keyb_poll(), so its first-ever call would otherwise compute
	// every column's transition against that wrong all-zero baseline and
	// never detect anything. One discarded call here primes keyb_matrix[]
	// with real hardware state before the mechanism below starts relying
	// on it every frame.
	keyb_poll();

	// linespertick=25 (bumped from an initial 4 while chasing what turned
	// out to be an unrelated bug -- not reverted, no reason to).
	raster_music_irq_start(colortable, 400, 25, 0);

	// Run for a fixed number of frames -- see this function's top comment
	// and raster_music_irq_start()'s header comment for why this doesn't
	// wait for a keypress instead. raster_irq.framecount is declared
	// volatile (vdc_raster.c), so this bare spin loop is safe -- unlike the
	// old keyb_key-based loop this replaces, which needed an explicit
	// volatile cast at the use site because keyb_key itself isn't declared
	// volatile in Oscar64's own <c64/keyboard.h>.
	while (raster_irq.framecount < DEMOFRAMES)
	{
	}

	raster_music_irq_stop();

	// Drain any keypress that arrived while the effect was running -- KERNAL
	// banking (and therefore vdcwin_checkch()) is only usable again now that
	// raster_music_irq_stop() has restored it. Doesn't try to detect one
	// during the effect itself; matches mono_hires_xl_demo()'s own approach
	// of not fighting this mechanism's keypress limitation.
	while (vdcwin_checkch())
	{
	}

	// Wipe right as the buffered keypress is drained above -- see
	// init_plasma()'s comment.
	vdc_wipe_transition();
}

void fli_color_demo()
// Showcases VDC-FLI (480x252, 8x1 colour cells, non-interlace) -- the
// simplest of Tokra's colour modes to add ("VDC Mode Mania", see
// original/v12/ and the credits in defines.h), since it needs no genuine
// interlaced dual-field encoding: just a static bitmap plane plus a static
// attribute plane, the same mechanism plasma_demo()/rotate_demo() already
// use for VDC_HIRES_640x200_Color_PAL (procedural content there; a loaded
// picture here). Cycles through 3 real photos (tools/vdc_convert.py output,
// see defines.h for sources/licenses), advancing on keypress.
{
	static const char *descr[3] = {
		"Van Gogh, Wheat Field with Cypresses",
		"Van Gogh, Starry Night Over the Rhone",
		"Van Gogh, Vase with Irises",
	};
	static const char *bitnames[3] = {"fli1bitk", "fli2bitk", "fli3bitk"};
	static const char *colnames[3] = {"fli1colk", "fli2colk", "fli3colk"};
	char pic;

	// HDISPLAY/HSYNC (registers 1/2) are not part of any other mode's
	// vdc_modes[] row -- every 640-pixel-wide mode has always relied on
	// these staying whatever the KERNAL's own 80-column init left them at,
	// since nothing in this project touched them before this mode existed.
	// VDC-FLI is the first mode to genuinely need different values (480
	// pixels wide = 60 displayed characters, not 80), so save the incoming
	// values here and restore them before returning -- otherwise every
	// later mode that doesn't set these registers itself (all of them
	// except VDC-IMONO) inherits this mode's narrower timing and comes out
	// horizontally corrupted. See mono_hires_xl_demo() for the same pattern.
	char old_hdisplay = vdc_reg_read(VDCR_HDISPLAY);
	char old_hsync = vdc_reg_read(VDCR_HSYNC);

	for (pic = 0; pic < 3; pic++)
	{
		// Wipe first every pass (harmless on pic==0, coming from the text-mode
		// menu; clears the *previous* picture's bitmap on later passes before
		// vdc_mode_info_screen() below draws text over it -- see the 3-picture
		// loop's own comment convention, mono_hires_xl_demo()/mono_im800_demo()
		// use the same pattern).
		vdc_wipe_transition();

		vdc_mode_info_screen("VDC-FLI", "480 x 252 pixels, non-interlace", "colour resolution: 8x1", descr[pic], 0);

		// TSCrunch-compressed via krill_loadcompd() -- see Makefile's
		// KRILL_COMPRESSED_ASSETS comment.
		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, bitnames[pic]))
		{
			printf("krill loadcompd failed: %s\n", bitnames[pic]);
			exit(1);
		}
		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN + 15120, colnames[pic]))
		{
			printf("krill loadcompd failed: %s\n", colnames[pic]);
			exit(1);
		}

		// Wipe here too -- the info screen (text mode) needs to be cleared
		// before switching to this mode's own bitmap geometry, otherwise it
		// would briefly show reinterpreted under VDC-FLI's bitmap/attribute
		// layout before bnk_cpytovdc() below overwrites it.
		vdc_wipe_transition();

		vdc_init(VDC_HIRES_480x252_Color_PAL, 1);
		if (!vdc_state.bitmap)
		{
			return;
		}

		// CSIZE (register 9) is deliberately absent from this mode's
		// vdc_modes[] row (see its comment in vdc_core.c) -- vdc_set_mode()'s
		// regset loop therefore leaves it at whatever the *previous* mode
		// set it to (height 8 for every mode that ran before this one), not
		// the height-1 this mode needs. The per-frame toggle below corrects
		// that once it starts, but vdc_init() already re-enabled the
		// display, and there's a real gap between here and the toggle
		// loop's first pass (both bnk_cpytovdc() calls, plus the
		// keypress-drain wait below) where the picture would otherwise show
		// at 8x the intended height. Force it to height 1 now so that gap
		// is never visibly wrong.
		vdc_reg_write(VDCR_CSIZE, 0xe0);

		bnk_cpytovdc(vdc_state.base_text, BNK_1_FULL, (char *)MEM_SCREEN, 15120);
		bnk_cpytovdc(vdc_state.base_attr, BNK_1_FULL, (char *)MEM_SCREEN + 15120, 15120);

		while (vdcwin_checkch())
		{
		}

		// VDC-FLI's per-frame CSIZE (register 9) toggle -- see the long
		// comment on this mode's vdc_modes[] row in vdc_core.c for why this
		// is here instead of a static register value: reverse-engineered
		// from Tokra's sys4864 (vdcmodemania.bas).
		//
		// Exit check is a direct CIA1 keyboard-matrix poll (keyb_poll(),
		// called with interrupts still disabled from the asm block above),
		// not vdcwin_checkch()/KERNAL GETIN -- matching Tokra's own
		// sys4864, which reads $dc00/$dc01 directly inside this same
		// SEI-held loop rather than re-enabling interrupts and going
		// through the KERNAL. This loop holds SEI for nearly the entire VDC
		// frame (fw1 waits out the active display period, fw2 waits out the
		// following vblank), leaving only a brief CLI window each pass --
		// not enough of a guarantee that the KERNAL's own keyboard-scan IRQ
		// gets to run and populate GETIN's buffer, which is what going
		// through vdcwin_checkch() here depended on and is the likely cause
		// of this section previously appearing to hang (frozen in the
		// pre-toggle CSIZE state above, never reaching a completed pass).
		// keyb_poll() only touches CIA1 registers directly, so it's safe to
		// call with interrupts still off.
		do
		{
			__asm
			{
				sei
				ldx #9
				lda #$20
				ldy #$e0
			fw1:
				bit $d600
				bne fw1
				stx $d600
				sty $d601
				ldy #$e7
			fw2:
				bit $d600
				beq fw2
				sty $d601
			}
			keyb_poll();
			__asm { cli }
			// joy_poll() only after keyb_poll() returns, and only trusted
			// as a fallback when keyb_poll() found no real key -- CIA1
			// $dc00/Port A is shared between joystick port 2 input and
			// this project's own keyboard-matrix row-select output, and
			// this is the one call site running a hand-rolled matrix scan
			// back-to-back with a raw joystick read in the same SEI-held
			// pass, with no other frame's scanning in between to dilute a
			// held joystick direction/fire being misread as a phantom
			// keyboard-matrix keypress on whichever row that bit's wire
			// crosses (a generic, well-known port-2 hardware quirk, not
			// introduced by this change). Preferring a real keyb_key
			// result when one exists sidesteps needing to touch
			// keyb_poll() itself at all.
			if (keyb_key == 0)
			{
				joy_poll(0);
			}
		} while (keyb_key == 0 && !joyb[0]);
	}

	// Wipe right as the keypress that ends this section is detected -- see
	// init_plasma()'s comment. Supersedes the plain vdc_cls() this used to
	// end with.
	vdc_wipe_transition();

	// Restore HDISPLAY/HSYNC (see the comment above) so the next mode --
	// which won't set these registers itself -- doesn't inherit VDC-FLI's
	// narrower 60-character timing.
	vdc_reg_write(VDCR_HDISPLAY, old_hdisplay);
	vdc_reg_write(VDCR_HSYNC, old_hsync);
}

void fli_ihfli_demo()
// Showcases VDC-IHFLI (640x480, interlace, 8x2 colour cells, near-NTSC --
// see original/v12/ and vdc_modes[]'s own row comment in vdc_core.c).
// Genuinely interlaced dual-field encoding: separate top/bottom bitmap AND
// separate top/bottom colour(attribute) planes, four files loaded one at a
// time into the 32KB CPU staging buffer (each field individually fits;
// no pair fits together, unlike VDC-FLI's single bitmap+colour load) --
// same sequential load-then-copy pattern as VDC-IMONO's top+bottom split,
// just with twice as many fields. Cycles through 3 real photos (see
// defines.h for sources/licenses). No HDISPLAY/HSYNC/
// SYNCSIZE save-restore needed here (unlike VDC-FLI/VDC-IMONO): this mode
// is 640 pixels/80 columns wide like the KERNAL default, and every
// vdc_init() call now resets those three registers to the boot baseline
// before applying its own mode (see vdc_reset_boot_registers() in
// vdc_core.c) -- the next mode's own vdc_init() call handles it
// automatically regardless of what this one leaves behind.
// Live-confirmed working.
{
	static const char *descr[3] = {
		"Passiflora caerulea macro",
		"Sunflower",
		"Keel-billed toucan, Belize",
	};
	// Named even/odd, not top/bottom: these are interlace fields (even
	// source rows / odd source rows), not a physical top-half/bottom-half
	// split -- see tools/vdc_convert.py's deinterlace_fields() comment.
	static const char *ce_names[3] = {"ihfli1cek", "ihfli2cek", "ihfli3cek"};
	static const char *co_names[3] = {"ihfli1cok", "ihfli2cok", "ihfli3cok"};
	static const char *be_names[3] = {"ihfli1bek", "ihfli2bek", "ihfli3bek"};
	static const char *bo_names[3] = {"ihfli1bok", "ihfli2bok", "ihfli3bok"};
	char pic;

	for (pic = 0; pic < 3; pic++)
	{
		vdc_wipe_transition();

		vdc_mode_info_screen("VDC-IHFLI", "640 x 480 pixels (interlace)", "colour resolution: 8x2", descr[pic], 0);

		// TSCrunch-compressed via krill_loadcompd() -- see Makefile's
		// KRILL_COMPRESSED_ASSETS comment.
		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, ce_names[pic]))
		{
			printf("krill loadcompd failed: %s\n", ce_names[pic]);
			exit(1);
		}

		vdc_wipe_transition();

		vdc_init(VDC_HIRES_640x480_IHFLI_NTSC, 1);
		if (!vdc_state.bitmap)
		{
			return;
		}

		bnk_cpytovdc(vdc_state.base_attr, BNK_1_FULL, (char *)MEM_SCREEN, 9600);

		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, co_names[pic]))
		{
			printf("krill loadcompd failed: %s\n", co_names[pic]);
			exit(1);
		}
		bnk_cpytovdc(0x0230, BNK_1_FULL, (char *)MEM_SCREEN, 9600);

		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, be_names[pic]))
		{
			printf("krill loadcompd failed: %s\n", be_names[pic]);
			exit(1);
		}
		bnk_cpytovdc(vdc_state.base_text, BNK_1_FULL, (char *)MEM_SCREEN, 19200);

		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, bo_names[pic]))
		{
			printf("krill loadcompd failed: %s\n", bo_names[pic]);
			exit(1);
		}
		bnk_cpytovdc(0x5780, BNK_1_FULL, (char *)MEM_SCREEN, 19200);

		while (vdcwin_checkch())
		{
		}

		do
		{
			joy_poll(0);
		} while (!vdcwin_checkch() && !joyb[0]);
	}

	// Wipe right as the keypress that ends this section is detected -- see
	// init_plasma()'s comment.
	vdc_wipe_transition();
}

void fli_itfli_demo()
// Showcases VDC-ITFLI (640x576, interlace, 8x3 colour cells, near-PAL --
// see original/v12/). Same dual-field structure as VDC-IHFLI above, just
// PAL-tuned timing and taller (8x3 cells) -- tightest fit of the whole
// set (picture data alone is 61440 of 65536 bytes). Cycles through 3 real
// photos (see defines.h for sources/licenses). No HDISPLAY/HSYNC/SYNCSIZE
// save-restore needed, same reasoning as fli_ihfli_demo().
{
	static const char *descr[3] = {
		"Tutankhamun funerary mask",
		"Hyacinth macaw, the Pantanal, Brazil",
		"Utrecht cityscape",
	};
	// Named even/odd, not top/bottom -- see fli_ihfli_demo()'s own comment.
	static const char *ce_names[3] = {"itfli1cek", "itfli2cek", "itfli3cek"};
	static const char *co_names[3] = {"itfli1cok", "itfli2cok", "itfli3cok"};
	static const char *be_names[3] = {"itfli1bek", "itfli2bek", "itfli3bek"};
	static const char *bo_names[3] = {"itfli1bok", "itfli2bok", "itfli3bok"};
	char pic;

	for (pic = 0; pic < 3; pic++)
	{
		vdc_wipe_transition();

		vdc_mode_info_screen("VDC-ITFLI", "640 x 576 pixels (interlace)", "colour resolution: 8x3", descr[pic], 0);

		// TSCrunch-compressed via krill_loadcompd() -- see Makefile's
		// KRILL_COMPRESSED_ASSETS comment.
		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, ce_names[pic]))
		{
			printf("krill loadcompd failed: %s\n", ce_names[pic]);
			exit(1);
		}

		vdc_wipe_transition();

		vdc_init(VDC_HIRES_640x576_ITFLI_PAL, 1);
		if (!vdc_state.bitmap)
		{
			return;
		}

		bnk_cpytovdc(vdc_state.base_attr, BNK_1_FULL, (char *)MEM_SCREEN, 7680);

		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, co_names[pic]))
		{
			printf("krill loadcompd failed: %s\n", co_names[pic]);
			exit(1);
		}
		bnk_cpytovdc(0x0000, BNK_1_FULL, (char *)MEM_SCREEN, 7680);

		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, be_names[pic]))
		{
			printf("krill loadcompd failed: %s\n", be_names[pic]);
			exit(1);
		}
		bnk_cpytovdc(vdc_state.base_text, BNK_1_FULL, (char *)MEM_SCREEN, 23040);

		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, bo_names[pic]))
		{
			printf("krill loadcompd failed: %s\n", bo_names[pic]);
			exit(1);
		}
		bnk_cpytovdc(0x4100, BNK_1_FULL, (char *)MEM_SCREEN, 23040);

		while (vdcwin_checkch())
		{
		}

		do
		{
			joy_poll(0);
		} while (!vdcwin_checkch() && !joyb[0]);
	}

	// Wipe right as the keypress that ends this section is detected -- see
	// init_plasma()'s comment.
	vdc_wipe_transition();
}

void fli_hfli_demo()
// Showcases VDC-HFLI (640x400, non-interlace, 8x2 colour cells -- see
// original/v12/). Simplest of the three remaining colour modes -- single
// static bitmap+colour plane, no dual-field encoding, no per-frame CSIZE
// toggle (CSIZE=1 is set directly in this mode's own vdc_modes[] row,
// unlike VDC-FLI). Cycles through 3 real photos (tools/vdc_convert.py
// output, see defines.h for sources/licenses), advancing on keypress.
{
	static const char *descr[3] = {
		"Hokusai, The Great Wave off Kanagawa",
		"Hokusai, Fine Wind, Clear Morning",
		"Hokusai, Ejiri in Suruga Province",
	};
	static const char *bitnames[3] = {"hfli1btk", "hfli2btk", "hfli3btk"};
	static const char *colnames[3] = {"hfli1clk", "hfli2clk", "hfli3clk"};
	char pic;

	for (pic = 0; pic < 3; pic++)
	{
		vdc_wipe_transition();

		vdc_mode_info_screen("VDC-HFLI", "640 x 400 pixels, non-interlace", "colour resolution: 8x2", descr[pic], 0);

		// TSCrunch-compressed via krill_loadcompd() -- see Makefile's
		// KRILL_COMPRESSED_ASSETS comment.
		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, bitnames[pic]))
		{
			printf("krill loadcompd failed: %s\n", bitnames[pic]);
			exit(1);
		}

		vdc_wipe_transition();

		vdc_init(VDC_HIRES_640x400_HFLI_PAL, 1);
		if (!vdc_state.bitmap)
		{
			return;
		}

		bnk_cpytovdc(vdc_state.base_text, BNK_1_FULL, (char *)MEM_SCREEN, 32000);

		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, colnames[pic]))
		{
			printf("krill loadcompd failed: %s\n", colnames[pic]);
			exit(1);
		}
		bnk_cpytovdc(vdc_state.base_attr, BNK_1_FULL, (char *)MEM_SCREEN, 16000);

		while (vdcwin_checkch())
		{
		}

		do
		{
			joy_poll(0);
		} while (!vdcwin_checkch() && !joyb[0]);
	}

	// Wipe right as the keypress that ends this section is detected -- see
	// init_plasma()'s comment.
	vdc_wipe_transition();
}

// Shared by mono_hires_xl_demo()/mono_im800_demo(): both are non-attribute
// bitmap modes (colorlines=0), so the *entire* picture's "1" bits are
// coloured by one single register (VDCR_COLOR high nibble, vdc_fgcolor()) --
// same mechanism idi8b_logo_demo()'s raster bar exploits, just static here
// instead of animated. Per-picture raster colour bands were tried first and
// dropped (2026-07-26): unstable in VICE and not the effect actually
// wanted. This replaces them with a manual colour cycle instead.
static const char mono_cycle_colors[15] = {
	VDC_WHITE, VDC_LGREY, VDC_DGREY, VDC_LYELLOW, VDC_DYELLOW,
	VDC_LRED, VDC_DRED, VDC_LPURPLE, VDC_DPURPLE, VDC_LBLUE,
	VDC_DBLUE, VDC_LCYAN, VDC_DCYAN, VDC_LGREEN, VDC_DGREEN,
};

char mono_color_cycle_wait()
// Waits for a keypress; '+' (or '=', accepted as an alias since it shares
// the same physical key on most keyboards and is easier to hit than a
// shifted '+' through VICE's own keyboard mapping) and '-' cycle the
// picture's foreground colour instead of ending the wait. Starts at white
// every time a new picture is shown; VDC_BLACK is deliberately excluded
// from the cycle entirely (a black "1" plane would make the whole picture
// invisible against the black background). Returns the first non-cycling
// key pressed -- the caller's own "advance to next picture" signal.
{
	char key;
	char idx = 0; // mono_cycle_colors[0] == VDC_WHITE

	vdc_fgcolor(mono_cycle_colors[idx]);
	for (;;)
	{
		do
		{
			joy_poll(0);
			key = vdcwin_checkch();
		} while (key == 0 && !joyb[0]);
		if (key == 0)
		{
			key = CH_ENTER; // joystick fire behaves like "any other key" -- advance
		}

		if (key == '+' || key == '=')
		{
			idx = (idx + 1) % 15;
			vdc_fgcolor(mono_cycle_colors[idx]);
		}
		else if (key == '-')
		{
			idx = (idx == 0) ? 14 : idx - 1;
			vdc_fgcolor(mono_cycle_colors[idx]);
		}
		else
		{
			return key;
		}
	}
}

void mono_hires_xl_demo()
// Showcases VDC-IMONO (720x700, interlace monochrome -- see original/v12/).
// Mechanism 2 (raster_music_irq_start()'s CIA1 hardware IRQ) was dropped
// from this demo entirely, see mono_colorize_demo()'s comment in main():
// even a fixed-duration timer exit didn't work reliably, it banks out
// KERNAL/BASIC/char ROM for its whole active duration, which is
// what made keyb_poll()-based keypress detection unreliable here (see
// memory: mono_colorize_keypress_bug -- extensively diagnosed, root cause
// never found). The proven-reliable vdcwin_checkch() (KERNAL GETIN) works
// exactly like it does in title_screen() -- this sidesteps that bug
// entirely rather than continuing to chase it. Picture converted from a
// CC-BY-SA photograph by tools/vdc_convert.py -- see defines.h for the
// source/license.
//
// The 720x700 bitmap is 63000 bytes -- bigger than the 32KB CPU RAM staging
// area (MEM_SCREEN..MEM_CHARSET, see defines.h) used to load a picture into
// VDC memory in one piece, so unlike title_screen()'s even/odd fields
// (which both fit in that 32KB together), each field here is loaded and
// copied to VDC memory before the next field is loaded into the same
// buffer.
{
	static const char *descr[3] = {
		"Strasbourg Cathedral, exterior",
		"Zebra with herd",
		"Berber woman, traditional dress",
	};
	// Named even/odd, not top/bottom: these are interlace fields (even
	// source rows / odd source rows), not a physical top-half/bottom-half
	// split -- see tools/vdc_convert.py's deinterlace_fields() comment.
	static const char *ev_names[3] = {"imono1evk", "imono2evk", "imono3evk"};
	static const char *od_names[3] = {"imono1odk", "imono2odk", "imono3odk"};
	char pic;

	// HDISPLAY/HSYNC/SYNCSIZE (registers 1/2/3) aren't part of any other
	// mode's vdc_modes[] row (see the identical comment in fli_color_demo());
	// VDC-IMONO is the other mode that genuinely needs different values (720
	// pixels wide = 90 displayed characters). Save/restore so the next mode
	// doesn't inherit this one's timing.
	char old_hdisplay = vdc_reg_read(VDCR_HDISPLAY);
	char old_hsync = vdc_reg_read(VDCR_HSYNC);
	char old_syncsize = vdc_reg_read(VDCR_SYNCSIZE);

	for (pic = 0; pic < 3; pic++)
	{
		vdc_wipe_transition();

		vdc_mode_info_screen("VDC-IMONO", "720 x 700 pixels (interlace)", "monochrome", descr[pic], 0);

		// TSCrunch-compressed via krill_loadcompd() -- see Makefile's
		// KRILL_COMPRESSED_ASSETS comment.
		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, ev_names[pic]))
		{
			printf("krill loadcompd failed: %s\n", ev_names[pic]);
			exit(1);
		}

		// Wipe here -- see the identical comment in fli_color_demo(): the
		// info screen (text mode) needs wiping away before switching to
		// this mode's own geometry.
		vdc_wipe_transition();

		vdc_init(VDC_HIRES_720x700_Mono_PAL, 1);
		if (!vdc_state.bitmap)
		{
			return;
		}

		bnk_cpytovdc(vdc_state.base_text, BNK_1_FULL, (char *)MEM_SCREEN, 31500);

		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, od_names[pic]))
		{
			printf("krill loadcompd failed: %s\n", od_names[pic]);
			exit(1);
		}

		// Odd field goes to VDC address 0x82c8, *not* base_text+31500
		// (0x7b0c) -- disassembling Tokra's own file loader (gosub9999,
		// vdcmodemania.bas line 5004: "v=dec("82c8"):n$=f$+".bb"") shows a
		// genuine ~1980-byte gap between the two fields, not a simple back-to-
		// back split. tools/vdc_convert.py's own deinterlace_fields() confirms
		// this is an even/odd row split, not a physical top/bottom-half one.
		// 0x82c8 leaves the same ~2.5KB headroom this mode's char_std=0
		// comment already assumed (63000 bytes across the two fields either
		// way), just not contiguous.
		bnk_cpytovdc(0x82c8, BNK_1_FULL, (char *)MEM_SCREEN, 31500);

		while (vdcwin_checkch())
		{
		}

		// See mono_color_cycle_wait()'s own comment: +/-/= cycle the
		// picture's colour, any other key advances to the next picture.
		mono_color_cycle_wait();
	}

	// Wipe right as the keypress that ends this section is detected -- see
	// init_plasma()'s comment. Supersedes the plain vdc_cls() this used to
	// end with.
	vdc_wipe_transition();

	// Restore HDISPLAY/HSYNC/SYNCSIZE (see the comment above) so the next
	// mode doesn't inherit VDC-IMONO's 90-character timing.
	vdc_reg_write(VDCR_HDISPLAY, old_hdisplay);
	vdc_reg_write(VDCR_HSYNC, old_hsync);
	vdc_reg_write(VDCR_SYNCSIZE, old_syncsize);
}

void mono_im800_demo()
// Showcases VDC-IM800 (800x600, interlace, monochrome -- see original/v12/).
// Tokra's own readme note: needs a monitor that can squeeze the image on
// real hardware -- a Commodore 1901 "cannot squeeze horizontally, so you
// will miss the left and right edges." No colour plane (colorlines=0), so
// only two fields to load (even/odd bitmap), same sequential
// load-then-copy pattern as VDC-IMONO. This mode is 800 pixels/100 columns
// wide, wider than the KERNAL default -- HDISPLAY/HSYNC/SYNCSIZE saved and
// restored explicitly here, same pattern as fli_color_demo()/
// mono_hires_xl_demo(), since the *next* mode needs the standard 80-column
// baseline back and this one's row doesn't match it.
//
// 3rd picture is Maupi, the author's own cat (not Commons-sourced, see
// defines.h).
{
	static const char *descr[3] = {
		"Portrait of a woman",
		"Kelly Lee Owens, studio portrait",
		"Maupi, the author's own cat",
	};
	// Named even/odd, not top/bottom -- see mono_hires_xl_demo()'s own
	// comment (same interlace-field convention, no address gap concern
	// here since 0x7e2c below is unconditional either way).
	static const char *ev_names[3] = {"im8001evk", "im8002evk", "im8003evk"};
	static const char *od_names[3] = {"im8001odk", "im8002odk", "im8003odk"};
	char pic;

	char old_hdisplay = vdc_reg_read(VDCR_HDISPLAY);
	char old_hsync = vdc_reg_read(VDCR_HSYNC);
	char old_syncsize = vdc_reg_read(VDCR_SYNCSIZE);

	for (pic = 0; pic < 3; pic++)
	{
		vdc_wipe_transition();

		vdc_mode_info_screen("VDC-IM800", "800 x 600 pixels (interlace)", "monochrome", descr[pic], 0);

		// TSCrunch-compressed via krill_loadcompd() -- see Makefile's
		// KRILL_COMPRESSED_ASSETS comment.
		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, ev_names[pic]))
		{
			printf("krill loadcompd failed: %s\n", ev_names[pic]);
			exit(1);
		}

		vdc_wipe_transition();

		vdc_init(VDC_HIRES_800x600_IM800_PAL, 1);
		if (!vdc_state.bitmap)
		{
			return;
		}

		bnk_cpytovdc(vdc_state.base_text, BNK_1_FULL, (char *)MEM_SCREEN, 30000);

		if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, od_names[pic]))
		{
			printf("krill loadcompd failed: %s\n", od_names[pic]);
			exit(1);
		}
		bnk_cpytovdc(0x7e2c, BNK_1_FULL, (char *)MEM_SCREEN, 30000);

		while (vdcwin_checkch())
		{
		}

		// See mono_color_cycle_wait()'s own comment: +/-/= cycle the
		// picture's colour, any other key advances to the next picture.
		mono_color_cycle_wait();
	}

	// Wipe right as the keypress that ends this section is detected -- see
	// init_plasma()'s comment.
	vdc_wipe_transition();

	// Restore HDISPLAY/HSYNC/SYNCSIZE (see the comment above) so the next
	// mode doesn't inherit VDC-IM800's 100-character timing.
	vdc_reg_write(VDCR_HDISPLAY, old_hdisplay);
	vdc_reg_write(VDCR_HSYNC, old_hsync);
	vdc_reg_write(VDCR_SYNCSIZE, old_syncsize);
}

void mono_im960_demo()
// Showcases VDC-IM960 (960x540, interlace, monochrome -- see
// original/v12/). Tokra's own readme note: "specifically designed for the
// RGBtoHDMI-device. It will probably not work otherwise" -- included for
// completeness, but don't expect this one to render correctly in VICE;
// treat a working real-hardware/RGBtoHDMI result as the actual bar for
// success. Tightest fit of the entire project: picture data alone is
// 64800 of 65536 bytes, 736 spare -- and Tokra's own field-address
// convention is reversed here vs every other interlaced mode (the "top"
// field goes to the HIGHER address, 0x8160; "bottom" to the LOWER, 0x0000)
// -- see this mode's vdc_modes[] row comment in vdc_core.c. Picture:
// Tokra's own "cat" (temporary, see fli_ihfli_demo()'s comment on
// licensing). This mode is 960 pixels/120 columns wide -- HDISPLAY/HSYNC/
// SYNCSIZE saved/restored, same reasoning as mono_im800_demo().
{
	vdc_mode_info_screen("VDC-IM960", "960 x 540 pixels (interlace)", "monochrome", "Designed for RGBtoHDMI device", "Refresh rate about 28 Hz");

	// asset-loading-roadmap.md Phase 2 Krill rollout -- see idi8b_logo_demo()'s
	// comment on this pattern.
	if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, "im960btk"))
	{
		printf("krill loadcompd failed: im960btk\n");
		exit(1);
	}

	char old_hdisplay = vdc_reg_read(VDCR_HDISPLAY);
	char old_hsync = vdc_reg_read(VDCR_HSYNC);
	char old_syncsize = vdc_reg_read(VDCR_SYNCSIZE);

	vdc_wipe_transition();

	vdc_init(VDC_HIRES_960x540_IM960_PAL, 1);
	if (!vdc_state.bitmap)
	{
		return;
	}

	// HSTART (register 34): this mode's own vdc_modes[] row sets it to 6
	// (Tokra's own value), via the regset[] loop inside vdc_set_mode() --
	// but vdc_set_mode()'s own final step, vdc_enable_display(), always
	// overwrites HSTART with the captured boot baseline (see
	// vdc_boot_hstart in vdc_core.c) *after* the regset loop runs. Without
	// this re-poke, this mode's own intended HSTART value would never
	// actually take effect. See this mode's vdc_modes[] row comment for
	// the full explanation.
	vdc_reg_write(VDCR_HSTART, 0x06);

	bnk_cpytovdc(0x8160, BNK_1_FULL, (char *)MEM_SCREEN, 32400);

	if (krill_loadcompd(BNK_1_IO, MEM_SCREEN, "im960bbk"))
	{
		printf("krill loadcompd failed: im960bbk\n");
		exit(1);
	}
	bnk_cpytovdc(vdc_state.base_text, BNK_1_FULL, (char *)MEM_SCREEN, 32400);

	while (vdcwin_checkch())
	{
	}

	while (!vdcwin_checkch())
	{
	}

	// Wipe right as the keypress that ends this section is detected -- see
	// init_plasma()'s comment.
	vdc_wipe_transition();

	// Restore HDISPLAY/HSYNC/SYNCSIZE (see the comment above) so the next
	// mode doesn't inherit VDC-IM960's 120-character timing.
	vdc_reg_write(VDCR_HDISPLAY, old_hdisplay);
	vdc_reg_write(VDCR_HSYNC, old_hsync);
	vdc_reg_write(VDCR_SYNCSIZE, old_syncsize);
}

void demo_end_screen(const char *message)
// Ends the program without ever returning to BASIC. Call after vdc_exit()
// (and krill_done(), if KRILL); prints message centred, waits for a
// keypress or joystick fire, then triggers a full machine reset instead of
// `return`-ing from main().
//
// Oscar64's own docs (oscar64.md, "Limits and Errors") document a known
// limitation: "Basic zero page variables not restored on stop/restore" --
// any Oscar64 program that uses zero page leaves BASIC's own zero-page
// state corrupted on return, and a clean READY prompt is not guaranteed.
// Jumping through the hardware RESET vector ($FFFC, same entry point the
// physical RESET button/power-on uses -- see ~/.claude/c128_reference.md's
// KERNAL jump table) is the standard demo-scene way to do this in software
// instead of requiring the user to actually press the button. `krill_done()`/
// `vdc_exit()` (called before this, at every call site) already restore
// $314/$315 and default MMU banking, so KERNAL/BASIC ROM is banked in here
// exactly as it would be for a real RESET.
//
// Live-tested in VICE and confirmed live by the user (2026-08-17): the
// jump correctly resets the machine, and since the disk/D81 is still
// mounted with its boot sector set to autostart, the KERNAL cold-start
// this triggers re-runs that autoboot, which reloads and restarts the
// whole demo -- press-key-to-reset therefore loops the demo end-to-end,
// same as many boot-sector-loaded C64/C128 demos are designed to do. No
// visible BASIC banner/READY prompt is expected or seen, because the
// autoboot takes over before KERNAL would ever get there.
{
    char col = (char)((80 - strlen(message)) / 2);
    vdc_prints(col, 12, message);
    vdc_prints(24, 14, "Press a key or fire to reset the machine");

    do
    {
        joy_poll(0);
    } while (!vdcwin_checkch() && !joyb[0]);

    __asm
    {
        sei
        jmp ($fffc)
    }
}

// Main menu
//
// Replaces the old flat, non-interactive sequence of demo-section calls in
// main() with a number-key-driven menu: all 6 real VDC hires-mode
// showcases, the 2 procedural effects (plasma/colour rotation), and the
// raster placement diagnostic, selectable in any order, any number of
// times, until ESC/STOP. Every entry's function is already fully
// self-contained (own vdc_init(), own exit-on-keypress loop, own
// vdc_wipe_transition() before returning) -- the menu only needs to call
// each one and redraw itself afterwards.
//
// plasma_demo()/rotate_demo() take a char mode parameter (both currently
// called with VDC_HIRES_640x200_Color_PAL from the old main()) -- wrapped
// in these two no-arg thunks so every entry fits the same menu_fn
// signature.
void menu_plasma_demo()
{
	plasma_demo(VDC_HIRES_640x200_Color_PAL);
}

void menu_rotate_demo()
{
	rotate_demo(VDC_HIRES_640x200_Color_PAL);
}

typedef void (*menu_fn)();
typedef struct
{
	char key;
	const char *label;
	menu_fn fn;
} menu_entry;

static const menu_entry menu_entries[9] = {
	{'1', "VDC-FLI      (480x252, colour 8x1 cells)", fli_color_demo},
	{'2', "VDC-HFLI     (640x400, colour 8x2 cells)", fli_hfli_demo},
	{'3', "VDC-IHFLI    (640x480, interlace, colour 8x2)", fli_ihfli_demo},
	{'4', "VDC-ITFLI    (640x576, interlace, colour 8x3)", fli_itfli_demo},
	{'5', "VDC-IMONO    (720x700, interlace mono)", mono_hires_xl_demo},
	{'6', "VDC-IM800    (800x600, interlace mono)", mono_im800_demo},
	{'7', "Plasma effect", menu_plasma_demo},
	{'8', "Colour rotation effect", menu_rotate_demo},
	{'9', "Raster bar placement test", raster_place_test},
};

#define MENU_REPEAT_DELAY 15 // frames held before auto-repeat kicks in (~0.3s @ 50Hz)
#define MENU_REPEAT_RATE 5    // frames between repeats once repeating (~10/s)
#define MENU_GLIDE_STEP 3     // rasterlines/frame the highlight glides toward its target (~1 row/frame at 8 lines/row, i.e. a quick but visible slide, not a snap)
#define MENU_ITEM_NUDGE 1     // small live-tuning correction for the item highlight vs its text (independent of the header's own row-1 fix) -- 2->1, shifts all rasters back down one line, per live feedback
#define HEADER_NUDGE 1        // small live-tuning correction for the header band vs its text, on top of the whole-row row-1 shift -- 2->1, same reason as MENU_ITEM_NUDGE above

char raster_bar_flat(char line, char color, char count)
// Fills `count` consecutive rasterlines with one repeated colour -- the
// flat-fill complement to raster_bar_segment()'s per-line palette walk,
// built from the same raster_bar_line() primitive. Must be called between
// raster_bar_begin()/raster_bar_end(), same as raster_bar_segment().
{
	while (count--)
	{
		raster_bar_line(line, color);
		line--;
	}
	return line;
}

// PAL: empirically calibrated live against raster_place_test() -- rasterline
// 153 landed on text row 13, i.e. TOP_LINE(13) = 255-8*13 = 151, within this
// project's usual +/-2-line live-tuning margin. NTSC: not yet independently
// calibrated -- placeholder uses the same offset until verified live in an
// NTSC VICE session (see eager-sniffing-feather.md); PAL and NTSC share the
// same 8-scanlines-per-text-row hardware constant (CSIZE doesn't change with
// video standard), so only this reference offset can differ between them.
#define PAL_ROW0_RASTERLINE 255
#define NTSC_ROW0_RASTERLINE 255

char vdc_row_to_rasterline(char row)
// Converts an 80x25 text row (0-24) to the VDC rasterline value
// raster_waitline()/raster_bar_*() expect, using g_is_ntsc (cached by
// system_diagnostic_screen(), the first thing main() runs) to pick the
// right reference offset. Row 0's own top-of-row value is the constant
// itself; every later row is 8 rasterlines further down (hardware CSIZE=8,
// same for every VDC_TEXT_80x25_* mode this project uses).
{
	return (g_is_ntsc ? NTSC_ROW0_RASTERLINE : PAL_ROW0_RASTERLINE) - 8 * row;
}

void main_menu()
// Cursor-key/joystick-driven main menu with a raster-bar highlight (full
// foreground+background gradient) behind the selected item. Runs in
// VDC_TEXT_80x25_Mono_PAL (attribute mode OFF) specifically so the raster
// sweep can drive BOTH colour nibbles at once -- in attribute mode (used
// by vdc_header_bar()/system_diagnostic_screen()/vdc_mode_info_screen())
// a raster bar can only ever change the background nibble, since each
// character's own attribute-RAM entry overrides the foreground (confirmed
// live, see the VDC_TEXT_80x25_Mono_PAL mode-table comment in
// vdc_core.c). This screen therefore draws its own header directly
// instead of calling vdc_header_bar(), and does not share code with the
// other two screens' attribute-mode header -- see
// eager-sniffing-feather.md for the full writeup of this trade-off.
//
// Readable-raster redesign (2026-08-17, per explicit user feedback: the
// original raster-gradient text wasn't clearly readable against a raster-
// gradient background). Settled shape: header is a subtle 3-tone text
// gradient on a flat background with a slow pulsing accent line at its
// very top and very bottom pixel line; unselected items are plain flat
// colour (an earlier same-session attempt gave every unselected item this
// same subtle treatment and had to be reverted for a cycle-budget desync
// bug -- see git history); the SELECTED item alone gets the header's same
// subtle treatment (warm text gradient, pulsing top/bottom line) instead
// of a plain flat highlight, so it's still clearly marked as selected
// without the harsh two-nibble gradient the original design had. Selection
// still glides smoothly between rows (`highlight_top` animation,
// unchanged) rather than snapping.
//
// Digit keys ('1'-'9') still select directly, unchanged. Cursor keys or
// joystick move the highlighted row (shared edge-detect + delay-then-
// repeat state machine so holding either doesn't double-advance); Return
// or joystick fire (edge-detected against `prevjoyb` -- a level check
// alone would immediately re-trigger a select the instant the menu is
// re-entered while fire is still held) confirms. ESC/STOP semantics
// unchanged -- still ends main_menu() entirely.
{
	// Every array below is a small (8-entry) compile-time constant, one
	// entry per rasterline of a single row, read with plain sequential
	// indexing -- exactly title_screen()'s own proven pattern, and nothing
	// like the previous attempt's per-line phase/modulo lookup that caused
	// a cycle-budget desync (see git history). The only thing that changes
	// per frame is which *pre-existing* table entry a couple of scalar
	// locals point at (pulse_highlight below) --
	// an array read, not a computation, so it costs the same one
	// instruction whether the sweep is running or not.
	//
	// Header text: 2-tone gradient (top to bottom), on a flat green
	// background -- title darker green, subtitle lighter, like before.
	// The two rows deliberately use different tone pairs now (live-tuned
	// per row, not copy-pasted): title (darker DGREEN background) uses
	// bright white/light-cyan; subtitle (lighter LGREEN background) uses
	// dark-grey/dark-cyan instead -- against the lighter background the
	// darker pair reads better. (Earlier single-pair attempts -- dark-grey
	// for both, then light-grey for both -- were too low-contrast on one
	// row or the other; per-row tone pairs is what actually worked.) No
	// pulsing accent here (tried, per live user feedback it read as
	// distracting rather than subtle -- unlike the selected item below,
	// this is on screen constantly, not just while attention is on one
	// row) -- flat header, full 8-line table read directly for both rows.
	static const char title_combined[8] = {
		16 * VDC_WHITE + VDC_DGREEN, 16 * VDC_WHITE + VDC_DGREEN, 16 * VDC_WHITE + VDC_DGREEN, 16 * VDC_WHITE + VDC_DGREEN,
		16 * VDC_LCYAN + VDC_DGREEN, 16 * VDC_LCYAN + VDC_DGREEN, 16 * VDC_LCYAN + VDC_DGREEN, 16 * VDC_LCYAN + VDC_DGREEN,
	};
	static const char subtitle_combined[8] = {
		16 * VDC_DGREY + VDC_LGREEN, 16 * VDC_DGREY + VDC_LGREEN, 16 * VDC_DGREY + VDC_LGREEN, 16 * VDC_DGREY + VDC_LGREEN,
		16 * VDC_DCYAN + VDC_LGREEN, 16 * VDC_DCYAN + VDC_LGREEN, 16 * VDC_DCYAN + VDC_LGREEN, 16 * VDC_DCYAN + VDC_LGREEN,
	};
	// Selection (CORRECTED 2026-08-17 -- the subtle pulsing-edge/gradient
	// treatment below belongs to the SELECTED item only, not to every
	// unselected one; an earlier pass in this same session had that
	// backwards): warm yellow -> orange(-ish) -> red text gradient (top to
	// bottom across its own 8 lines -- dyellow/lred are the closest this
	// palette has to "orange"; LRED not DRED at the bottom -- DRED-on-DBLUE
	// was live-tested too low-contrast to read comfortably, exactly the
	// problem this redesign exists to avoid), flat dark-blue background,
	// with the same subtle pulsing-line treatment as the header at its own
	// top and bottom pixel line. highlight_combined's index 0 and index 7
	// are never read (same reasoning as title_combined/subtitle_combined)
	// -- those two lines are drawn live instead, pulsing.
	static const char highlight_combined[8] = {
		16 * VDC_LYELLOW + VDC_DBLUE, 16 * VDC_LYELLOW + VDC_DBLUE,
		16 * VDC_DYELLOW + VDC_DBLUE, 16 * VDC_DYELLOW + VDC_DBLUE,
		16 * VDC_LRED + VDC_DBLUE, 16 * VDC_LRED + VDC_DBLUE,
		16 * VDC_LRED + VDC_DBLUE, 16 * VDC_LRED + VDC_DBLUE,
	};
	static const char pulse_table[8] = {
		VDC_DBLUE, VDC_DBLUE, VDC_LBLUE, VDC_WHITE, VDC_WHITE, VDC_LBLUE, VDC_DBLUE, VDC_DBLUE,
	};
	// Unselected items: back to plain flat colour, same as the original
	// (proven-safe) design -- the subtle effect above is for the one
	// selected row only.
	static const char floor_color = 16 * VDC_LYELLOW + VDC_BLACK;
	static const char gap_color = 16 * VDC_BLACK + VDC_BLACK;

	enum
	{
		PULSE_FRAMES = 6 // frames between pulse-phase steps -- ~1s per full breathe cycle at 8 steps and 50Hz
	};

	char key, i;
	char selected;
	signed char dir;
	char holdframes;
	char prevjoyb;
	char firepressed;
	char line;
	char target_top, highlight_top;
	char before_count, after_count;
	char items_top, items_bottom;
	char pulse_phase, pulse_counter;
	char pulse_highlight;

	// Set once, not per redraw pass -- retains the last-chosen item across
	// a dispatched section returning here, instead of always resetting to
	// the top of the list.
	selected = 0;

	for (;;)
	{
		vdc_init(VDC_TEXT_80x25_Mono_PAL, 1);
		// Recalibrate -- this mode's colorlines differ from whatever mode
		// last ran (e.g. VDC_TEXT_80x25_PAL for the diagnostic/info
		// screens, or a hires mode for a just-finished demo section),
		// matching idi8b_logo_demo()/mono_hires_xl_demo()'s own established
		// pattern of recalibrating fresh after every genuinely different
		// vdc_init().
		raster_calibrate();
		// Attribute mode off means no per-character ALTCHAR bit -- without
		// this, mixed-case text falls back to the uppercase-only charset
		// (see idi8b_logo_demo()'s own identical fix and its comment /
		// memory: vdc_charset_selection_no_attribute_mode).
		bnk_redef_charset(vdc_state.char_std, BNK_CHARROM, (char *)0xd800, 256);
		vdc_cls();

		// Row 1/2, not 0/1 -- row 0's own rasterline (255) is the very
		// first line reachable after raster_synch()'s own VBLANK sync
		// point (0 lines elapsed), with no room to nudge it any earlier;
		// row 1 (247) has a full row's worth of margin instead, and there
		// was no shortage of blank screen to give up row 0 to get it.
		vdc_prints(1, 1, "VDC Maniac: Experiments with C128's greatest asset");
		vdc_prints(61, 1, "IDreamIn8Bits.com");
		i = (char)((80 - strlen("Main menu")) / 2);
		vdc_prints(i, 2, "Main menu");

		for (i = 0; i < 9; i++)
		{
			sprintf(linebuffer, "%c) %s", menu_entries[i].key, menu_entries[i].label);
			vdc_prints(7, 5 + i, linebuffer);
		}
		vdc_prints(5, 16, "Cursor/joystick + RETURN/fire, or a number key. ESC/STOP to end.");

		holdframes = 0;
		prevjoyb = 0;
		// Item rows (text rows 5-13) as a single contiguous rasterline
		// span, top to bottom -- vdc_row_to_rasterline() gives the TOP
		// line of each row directly, so items_bottom is just the last
		// row's top minus 7 (its own 8 lines). No more manual gap-length
		// arithmetic (the old GAP_TRIM hack): the gap fill's length is
		// simply "whatever's left" between the header and items_top.
		// MENU_ITEM_NUDGE: small live-tuned correction (independent of the
		// header's own row-1 fix above, which is a whole-row change) --
		// the highlight sat 1-2 lines too low against the item text.
		items_top = vdc_row_to_rasterline(5) + MENU_ITEM_NUDGE;
		items_bottom = vdc_row_to_rasterline(13) - 7 + MENU_ITEM_NUDGE;
		// No animation on the very first draw -- start already at the
		// target so the highlight doesn't visibly slide in from row 0.
		highlight_top = items_top;

		pulse_phase = 0;
		pulse_counter = 0;

		while (vdcwin_checkch())
		{
		}

		key = 0;
		for (;;)
		{
			// One synced sweep per frame, top to bottom: header (rows
			// 0-1, own gradient), blank gap, the 9 menu-item rows' worth
			// of rasterlines (floor colour throughout, except an 8-line
			// window at `highlight_top` -- the glow), then the
			// instruction rows. `highlight_top` is animated toward
			// `target_top` a few lines per frame (see below) instead of
			// jumping straight there, so moving the selection glides
			// smoothly instead of snapping.
			target_top = vdc_row_to_rasterline(5 + selected) + MENU_ITEM_NUDGE;
			if (highlight_top < target_top)
			{
				highlight_top += MENU_GLIDE_STEP;
				if (highlight_top > target_top)
				{
					highlight_top = target_top;
				}
			}
			else if (highlight_top > target_top)
			{
				highlight_top -= MENU_GLIDE_STEP;
				if (highlight_top < target_top)
				{
					highlight_top = target_top;
				}
			}

			// Pulse phase: advances once every PULSE_FRAMES frames, well
			// outside the sweep below -- this and the table read right
			// after it are the *only* per-frame colour computation
			// anywhere in this function; everything the sweep itself
			// touches is a pre-existing constant table or a plain scalar.
			if (++pulse_counter >= PULSE_FRAMES)
			{
				pulse_counter = 0;
				pulse_phase = (pulse_phase + 1) % 8;
			}
			pulse_highlight = pulse_table[pulse_phase];

			raster_bar_begin();
			// Header: both rows read straight from their own flat 8-line
			// constant table, no live pulse.
			line = vdc_row_to_rasterline(1) + HEADER_NUDGE;
			line = raster_bar_segment(line, title_combined, 8);
			line = raster_bar_segment(line, subtitle_combined, 8);
			// True gap between the header and the item area (gap_color --
			// black on black, nothing is printed there so it doesn't
			// matter that this isn't an item colour).
			line = raster_bar_flat(line, gap_color, line - items_top);
			// Unselected item rows above the (possibly still-animating)
			// highlight -- floor_color, not gap_color, since these DO have
			// visible text (yellow-on-black) that gap_color's black-on-
			// black foreground would otherwise hide. Last of these lines
			// is instead drawn with the highlight's own (non-pulsing) blue
			// background -- the selected item's background starting one
			// pixel line earlier than its text/pulse actually does, per
			// live feedback -- skipped entirely when the highlight is
			// already at the very top row (before_count==0, nothing above
			// it to borrow a line from).
			before_count = line - highlight_top;
			if (before_count > 0)
			{
				line = raster_bar_flat(line, floor_color, before_count - 1);
				raster_bar_line(line, (VDC_LYELLOW * 16) | VDC_DBLUE);
				line--;
			}
			// The SELECTED item only: pulsing top line, warm gradient body,
			// pulsing bottom line -- same pattern as the header block
			// above, just for this one 8-line row instead of two rows.
			raster_bar_line(line, (VDC_LYELLOW * 16) | pulse_highlight);
			line--;
			line = raster_bar_segment(line, &highlight_combined[1], 6);
			raster_bar_line(line, (VDC_LRED * 16) | pulse_highlight);
			line--;
			// Symmetric one-line extension below the highlight (see above),
			// then the remaining unselected item rows down to the last item
			// row (+1: items_bottom is the last line to actually colour,
			// inclusive, not "one past" like every other target in this
			// sweep).
			after_count = line - items_bottom + 1;
			if (after_count > 0)
			{
				raster_bar_line(line, (VDC_LYELLOW * 16) | VDC_DBLUE);
				line--;
				line = raster_bar_flat(line, floor_color, after_count - 1);
			}
			line = raster_bar_flat(line, floor_color, 24);
			raster_bar_end();

			joy_poll(0);
			key = vdcwin_checkch();

			dir = 0;
			if (key == CH_CURS_UP || joyy[0] == -1)
			{
				dir = -1;
			}
			else if (key == CH_CURS_DOWN || joyy[0] == 1)
			{
				dir = 1;
			}

			if (dir != 0)
			{
				if (holdframes == 0 || holdframes > MENU_REPEAT_DELAY)
				{
					selected = (selected + 9 + dir) % 9;
					holdframes = (holdframes == 0) ? 1 : (MENU_REPEAT_DELAY - MENU_REPEAT_RATE + 1);
				}
				else
				{
					holdframes++;
				}
			}
			else
			{
				holdframes = 0;
			}

			firepressed = joyb[0] && !prevjoyb;
			prevjoyb = joyb[0];

			if (key == CH_ENTER || firepressed)
			{
				key = CH_ENTER;
				break;
			}
			if (key == CH_ESC || key == CH_STOP)
			{
				break;
			}
			if (key >= '1' && key <= '9')
			{
				selected = key - '1';
				key = CH_ENTER;
				break;
			}
		}

		if (key == CH_ESC || key == CH_STOP)
		{
			vdc_wipe_transition();
			return;
		}

		menu_entries[selected].fn();
	}
}

// System diagnostics (PAL/NTSC detection, VDC RAM/revision, raster
// calibration) -- shown once at boot, before the demo proper starts.

unsigned vic_rasterline()
// Full 9-bit VIC-II raster line (ctrl1 bit 7 = MSB, raster = low 8 bits,
// c64/vic.h) -- vic.raster alone aliases at physical line 256 (wraps back
// to 0), which would make a naive "wait for raster==0" false-trigger a
// full frame early: both PAL (312 lines) and NTSC (263 lines) exceed 255.
{
	return ((unsigned)(vic.ctrl1 & 0x80) << 1) | vic.raster;
}

char detect_ntsc()
// Detects PAL vs NTSC by measuring how many CIA1 cycles elapse during one
// full VIC-II frame (raster line 0 -> away from 0 -> back to 0), using the
// VIC's own hardware raster counter directly -- not any KERNAL/ROM-
// provided flag, since this project boots via a raw boot sector (not a
// normal KERNAL cold start) and this session already established KERNAL/
// ROM soft state can't be trusted after that (see krill_interrupt's own
// $314/$315 comment). VIC-II keeps generating its own raster timing even
// while blanked in 2MHz fast mode (confirmed this session -- it's exactly
// what keeps krill_interrupt's jsr $c024 jiffy/keyboard-scan working), so
// this works regardless of fastmode(). PAL is 312 lines * 63 cycles/line =
// 19656 cycles/frame; NTSC is 263 lines * 65 cycles/line = 17095 -- a wide
// enough gap (>2500 cycles) that a simple midpoint threshold is reliable
// even with a few cycles of measurement slop. Returns 1 if NTSC, 0 if PAL.
{
	unsigned elapsed;

	while (vic_rasterline() != 0)
	{
	}
	cia1.cra = 0x00;
	cia1.ta = 0xffff;
	cia1.cra = 0x11; // start, force load, continuous
	while (vic_rasterline() == 0)
	{
	}
	while (vic_rasterline() != 0)
	{
	}
	elapsed = 0xffff - cia1.ta;
	cia1.cra = 0x00; // stop -- leaves CIA1 idle for raster_calibrate()/etc. below

	return elapsed < 18375; // midpoint of 19656 (PAL) and 17095 (NTSC)
}

char detect_vdc_revision()
// Reads the VDC chip version directly from $D600's status byte, bits 2:0
// (0=original 8563, nonzero=later revision/8568) -- per
// ~/.claude/c128_reference.md's own documented VDC status register layout.
// A raw memory read, no register-select needed (unlike every other VDC
// register access in this file, via vdc_reg_read()): $D600 always returns
// this status byte on a plain read, regardless of which internal register
// was last selected by a write.
{
	return PEEK(0xd600) & 0x07;
}

void system_diagnostic_screen()
// Shows VDC RAM size, VDC chip revision, detected video standard, and
// raster calibration timing before the demo proper starts -- confirms the
// hardware was detected correctly, and gives the mandatory 64KB VDC RAM
// check (every hires mode in this demo needs it) a proper diagnostic
// screen instead of just a bare error message. Per explicit request: wait
// for a key to proceed, THEN exit if VDC RAM isn't 64KB -- the diagnostic
// info is worth seeing either way, not just on the failure path.
{
	char is_ntsc = detect_ntsc();
	char vdc_rev = detect_vdc_revision();
	g_is_ntsc = is_ntsc; // cached for vdc_row_to_rasterline()

	// Calibrate now, in this text mode, before showing the numbers below --
	// same call site raster_bar_*() users elsewhere already depend on this
	// running from (main(), before the first hires mode).
	raster_calibrate();

	vdc_cls();
	vdc_header_bar("System diagnostics");

	sprintf(linebuffer, "%u KB", vdc_state.memsize);
	diag_line(5, "VDC RAM", vdc_state.memsize == 64 ? "[ OK ]" : "[FAIL]",
			  vdc_state.memsize == 64 ? VDC_LGREEN : VDC_LRED, linebuffer);

	sprintf(linebuffer, "%u (%s)", vdc_rev,
			vdc_rev == 0 ? "original 8563" : "later revision / 8568");
	diag_line(6, "VDC chip revision", "[INFO]", VDC_LCYAN, linebuffer);

	diag_line(7, "Video standard", "[INFO]", VDC_LCYAN, is_ntsc ? "NTSC" : "PAL");

	sprintf(linebuffer, "%2u.%03u cycles/line (timer reload %2u)",
			raster_cycles_per_line_x1000 / 1000, raster_cycles_per_line_x1000 % 1000, raster_timer_reload);
	diag_line(8, "Raster calibration", "[INFO]", VDC_LCYAN, linebuffer);

	vdc_prints(5, 11, "Press a key to continue.");

	while (vdcwin_checkch())
	{
	}
	do
	{
		joy_poll(0);
	} while (!vdcwin_checkch() && !joyb[0]);

	if (vdc_state.memsize != 64)
	{
		vdc_prints(5, 13, "This demo requires a VDC with 64 KB RAM.");
		vdc_prints(5, 14, "Your VDC only has 16 KB -- exiting.");
		vdc_prints(5, 16, "Press a key to continue.");
		do
		{
			joy_poll(0);
		} while (!vdcwin_checkch() && !joyb[0]);
		krill_done();
		vdc_exit();
		demo_end_screen("This demo requires a VDC with 64 KB RAM.");
	}
}

// Main routine
int main(void)
{
	char pattern[2][8] = {{0x00, 0xd4, 0xaa, 0xd4, 0xaa, 0xd4, 0xaa, 0xff}, {0x01, 0x38, 0x7c, 0x7c, 0x7c, 0x38, 0x01, 0x83}};
	char y, line;
	unsigned screen1, screen2;

	// Init
	// cia_init() must run first: resets both CIA chips to a known-clean
	// state and explicitly acks their interrupt-control registers. Without
	// it, whatever CIA1 interrupt-pending state the boot-sector/disk-load
	// process left behind stuck around, and later corrupted $314/$315 by
	// the time raster_music_irq_start() tried to save it (confirmed live
	// in VICE: it read back as $ffff, garbage). Oscar64Test's main.c calls
	// this first for the same reason and its IRQ-driven music works there.
	cia_init();

	bnk_init();

	// asset-loading-roadmap.md Phase 2: install Krill's loader once, here,
	// for the whole program run -- matches Oscar64Test's own proven
	// sequence (krill_loadcode() right after bnk_init(), krill_init() right
	// after the first vdc_init()) rather than installing/tearing down per
	// function the way Phase 1's title_screen()-only proof did. Every demo
	// function's own load call (see idi8b_logo_demo()'s comment) is just
	// that -- no per-load cia_init() (the plan's own
	// recommended "safety net", which turned out to actively conflict with
	// Krill's own cia2.pra usage and hang the demo partway through once
	// live-tested; see title_screen()'s comment for the full explanation).
	// krill_done() below, at the very end, is the one teardown for the
	// whole run -- cia_init() itself only ever runs once, at the very top
	// of main(), before any of this.
	krill_loadcode();

	vdc_init(VDC_TEXT_80x25_PAL, 1);

	krill_init();

	// This demo's hires effects (title_screen(), mono_colorize_demo(),
	// plasma_demo(), rotate_demo()) all use bitmap modes whose framebuffer
	// alone exceeds the VDC's base 16KB (640x400 mono is 32000 bytes by
	// itself) -- they need the extended 64KB VDC memory to exist at all.
	// vdc_set_mode() silently no-ops when a mode needs more memory than is
	// present, so without this check the demo would just show a blank/
	// stuck screen on a 16KB VDC instead of a clear reason why. memsize is
	// populated by vdc_detect_mem_size(), called from the vdc_init() above.
	// system_diagnostic_screen() shows this (plus PAL/NTSC, VDC revision,
	// raster calibration) and performs the actual 64KB exit itself -- see
	// its own comment.
	system_diagnostic_screen();

	// Intro: black screen, black border, "loading assets" message, then
	// start the music -- SID load+init moved here (from its old position
	// right after the 64KB check) so it happens right after this message
	// is on screen, matching the demo spec's own "give message... start
	// music" ordering. VDC_TEXT_80x25_PAL is the attribute-mode text mode
	// (colorlines=8, unlike VDC_TEXT_80x25_Mono_PAL) -- vdc_fgcolor() only
	// sets the physical border nibble here; per-character text colour
	// comes from attribute RAM (vdc_prints_attr()/vdc_state.text_attr)
	// independently, so blacking the border doesn't also black the text
	// (see idi8b_logo_demo()'s own mode-table comment on this exact
	// attribute-mode-vs-not distinction).
	vdc_init(VDC_TEXT_80x25_PAL, 0);
	vdc_bgcolor(VDC_BLACK);
	vdc_fgcolor(VDC_BLACK);
	vdc_cls();
	vdc_prints_attr(20, 12, "Demo starting.... loading assets", VDC_LGREEN | VDC_A_ALTCHAR);

	// SID music (Phase 5): loaded once, here, for the whole program run --
	// "Faded" (GoatTracker), compiled straight from source to SIDINIT/
	// SIDPLAY -- see defines.h for the full history and why. sid_music_init()
	// (banking.c) below runs the tune's own init once; playback itself is
	// driven by krill_interrupt (krill.c) calling raster_irq_playframe()
	// (vdc_raster.c) once per VIC raster interrupt from here on -- not a
	// foreground call, so music keeps playing through every subsequent
	// krill_loadcompd() picture load too, not just this one section.
	if (krill_loadcompd(BNK_1_IO, SIDINIT, "musick"))
	{
		printf("krill loadcompd failed: musick\n");
		exit(1);
	}
	sid_music_init(g_is_ntsc);

	idi8b_logo_demo();

	title_screen();

	// Dropped for good (2026-07-22): mono_colorize_demo() is the last
	// remaining caller of Mechanism 2 (raster_music_irq_start()). Its
	// keypress-detection loop never worked (root cause never found, see
	// memory: mono_colorize_keypress_bug); replacing that with a fixed
	// frame-count timer (raster_irq.framecount) didn't work either -- live
	// testing showed it still never proceeds, so something about this
	// mechanism is more broken than just "can't detect a keypress".
	// mono_hires_xl_demo() already proved the better path forward: use
	// Mechanism 1 (raster_bar_*()) instead, which doesn't have any of these
	// problems. Not calling mono_colorize_demo() here; left intact
	// (unused) in case Mechanism 2 gets revisited later.
	// mono_colorize_demo();

	// Dropped from the demo run (2026-07-22): confirmed live in VICE that
	// VDC-IM960 doesn't render correctly here -- matches Tokra's own readme
	// note ("specifically designed for the RGBtoHDMI-device. It will
	// probably not work otherwise"). Function left intact in the codebase
	// (unused) for real-hardware/RGBtoHDMI testing later; not on the menu
	// since nothing else in this file depends on it running.
	// mono_im960_demo();

	// Every hires-mode showcase, both procedural effects, and the raster
	// placement diagnostic are now selectable from the menu (any order, any
	// number of times) instead of being called directly in a fixed sequence
	// -- see main_menu()/menu_entries[] above.
	main_menu();

	// One-time teardown for the whole run -- see the krill_loadcode()/
	// krill_init() comment near the top of main().
	krill_done();

	vdc_exit();

	demo_end_screen("VDC Maniac -- demo finished");

	return 0; // unreachable -- demo_end_screen() loops forever
}
