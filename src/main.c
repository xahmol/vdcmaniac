#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <petscii.h>
#include <conio.h>
#include <c128/vdc.h>
#include <c128/mmu.h>
#include <c64/cia.h>
#include <c64/keyboard.h>
#include "defines.h"
#include "banking.h"
#include "vdc_core.h"
#include "vdc_win.h"
#include "vdc_raster.h"

// Buffer for attribute screen calculations
char Screen[4000];

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

	vdc_set_mode(mode);
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
	} while (!vdcwin_checkch());
}

// Color rotate demo routines
void init_rotate(char mode)
// Init ghires for color rotate
{
	unsigned dp;
	char pattern[8] = {0x00, 0xd4, 0xaa, 0xd4, 0xaa, 0xd4, 0xaa, 0xff};
	char y, line;

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

	} while (!vdcwin_checkch());
}

// Raster
void raster_place_test()
{
	static const char gradient16[16] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
	char rasterline = 100;
	char line;
	char keypress = 0;

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

		// Check keys
		keypress = vdcwin_checkch();
		if (keypress == CH_CURS_DOWN && rasterline > 16)
		{
			rasterline--;
		}
		if (keypress == CH_CURS_UP && rasterline < 255)
		{
			rasterline++;
		}

	} while (keypress != CH_ESC && keypress != CH_STOP);

	vdc_cls();
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

	// Load screen
	bnk_load(bootdevice, 1, (char *)MEM_SCREEN, "vdce-scrtit.top");
	bnk_load(bootdevice, 1, (char *)MEM_SCREEN + 16000, "vdce-scrtit.bot");

	// Init proper hires mode
	// Must match the resolution the vdce-scrtit.top/.bot assets were
	// exported for: 640x400 mono (32000 bytes total, 16000/half). An
	// in-session experiment switched this to VDC_HIRES_640x480_Mono_NTSC
	// (with the load offset bumped to 19200 to match that taller mode's
	// framebuffer) without regenerating the assets for the new height --
	// that left a stale/garbage gap between the top and bottom halves in
	// VRAM (19200-16000 unfilled bytes) and reinterpreted 400-line bitmap
	// data as a 480-line layout (VDC bitmap memory is organized in
	// per-character-row blocks, so the row layout differs by height),
	// which is what caused the corruption/instability seen in VICE.
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

		raster_bar_begin();
		raster_bar_line(182, VDC_DRED);
		raster_bar_line(181, 16 * VDC_LYELLOW + VDC_DGREY);
		line = 176;
		line = raster_bar_segment(line, &rastercolors[start], 60);
		raster_bar_line(114, 16 * VDC_LCYAN + VDC_DGREY);
		raster_bar_line(85, VDC_DRED);
		raster_bar_line(84, 16 * VDC_WHITE + VDC_BLACK);
		raster_bar_end();
	} while (!vdcwin_checkch());
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
{
	static char colortable[400];
	unsigned i;
	unsigned dp;
	char pattern[8] = {0xff, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xff};
	char y, line;

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

	// linespertick=4 as a starting point (see the plan's cycle-budget
	// note); the tick offset between vblank-end and row 0 of the bitmap
	// isn't calibrated yet -- watch the top of the picture in VICE and
	// adjust raster_music_irq_start()'s internals if it doesn't line up.
	raster_music_irq_start(colortable, 400, 4, 0);

	// KERNAL/BASIC ROM is banked out while the IRQ is active (see
	// raster_music_irq_start()), so GETIN-based vdcwin_checkch() can't be
	// used here -- direct keyboard matrix scan instead.
	do
	{
		keyb_poll();
	} while (keyb_key == KSCAN_MAX);

	raster_music_irq_stop();

	vdc_cls();
}

void fli_geometry_test()
// TEMPORARY diagnostic -- fills VDC_HIRES_480x252_Color_PAL's bitmap/
// attribute planes with a trivial synthetic pattern (not loaded from disk)
// to check whether the mode's geometry/addressing is right, independent of
// tools/vdc_convert.py's output. Remove once fli_color_demo() is confirmed
// correct.
{
	unsigned addr;
	char row, col;

	vdc_init(VDC_HIRES_480x252_Color_PAL, 1);
	if (!vdc_state.bitmap)
	{
		return;
	}

	// Bitmap: solid white (all bits set) so only colour drives the pattern.
	vdc_mem_addr(vdc_state.base_text);
	for (addr = 0; addr < 15120; addr++)
	{
		vdc_write(0xff);
	}

	// Attribute: a distinct colour per 8-pixel column within each row, and
	// a distinct colour per row, so both axes are checkable at a glance.
	vdc_mem_addr(vdc_state.base_attr);
	for (row = 0; row < 252; row++)
	{
		for (col = 0; col < 60; col++)
		{
			vdc_write(((row & 0xf) << 4) | (col % 15) | 1);
		}
	}

	while (vdcwin_checkch())
	{
	}
	while (!vdcwin_checkch())
	{
	}
	vdc_cls();
}

void fli_color_demo()
// Showcases VDC-FLI (480x252, 8x1 colour cells, non-interlace) -- the
// simplest of Tokra's colour modes to add ("VDC Mode Mania", see
// original/v12/ and the credits in defines.h), since it needs no genuine
// interlaced dual-field encoding: just a static bitmap plane plus a static
// attribute plane, the same mechanism plasma_demo()/rotate_demo() already
// use for VDC_HIRES_640x200_Color_PAL (procedural content there; a loaded
// picture here). Picture converted from a CC0 photograph by
// tools/vdc_convert.py -- see defines.h for the source/license.
{
	bnk_load(bootdevice, 1, (char *)MEM_SCREEN, "vdcfli.bit");
	bnk_load(bootdevice, 1, (char *)MEM_SCREEN + 15120, "vdcfli.col");

	vdc_init(VDC_HIRES_480x252_Color_PAL, 1);
	if (!vdc_state.bitmap)
	{
		return;
	}

	bnk_cpytovdc(vdc_state.base_text, BNK_1_FULL, (char *)MEM_SCREEN, 15120);
	bnk_cpytovdc(vdc_state.base_attr, BNK_1_FULL, (char *)MEM_SCREEN + 15120, 15120);

	while (vdcwin_checkch())
	{
	}

	while (!vdcwin_checkch())
	{
	}

	vdc_cls();
}

void mono_hires_xl_demo()
// Showcases VDC-IMONO (720x700, interlace monochrome -- see original/v12/)
// combined with the CIA1 raster+music IRQ colourizer
// (raster_music_irq_start()): the mechanism doesn't hardcode any particular
// resolution (see mono_colorize_demo()), so this is mostly a matter of
// pointing it at a bigger picture and a taller colour table. Picture
// converted from a CC-BY-SA photograph by tools/vdc_convert.py -- see
// defines.h for the source/license.
//
// The 720x700 bitmap is 63000 bytes -- bigger than the 32KB CPU RAM staging
// area (MEM_SCREEN..MEM_CHARSET, see defines.h) used to load a picture into
// VDC memory in one piece, so unlike title_screen()'s top+bot (which both
// fit in that 32KB together), each half here is loaded and copied to VDC
// memory before the next half is loaded into the same buffer.
{
	static char colortable[700];
	unsigned i;

	bnk_load(bootdevice, 1, (char *)MEM_SCREEN, "vdcimono.top");

	vdc_init(VDC_HIRES_720x700_Mono_PAL, 1);
	if (!vdc_state.bitmap)
	{
		return;
	}

	// Recalibrate for this mode -- see title_screen()/mono_colorize_demo()'s
	// comments: calibration is mode-specific and must be redone after every
	// vdc_init() switch that a raster-timed effect depends on.
	raster_calibrate();

	bnk_cpytovdc(vdc_state.base_text, BNK_1_FULL, (char *)MEM_SCREEN, 31500);

	bnk_load(bootdevice, 1, (char *)MEM_SCREEN, "vdcimono.bot");
	bnk_cpytovdc(vdc_state.base_text + 31500, BNK_1_FULL, (char *)MEM_SCREEN, 31500);

	for (i = 0; i < 700; i++)
	{
		colortable[i] = VDC_BLACK | 16 * rasterbar[i % 13];
	}

	while (vdcwin_checkch())
	{
	}

	// linespertick=4 as a starting point, same as mono_colorize_demo() --
	// this mode's interlace flag (LACE=3) driving a 700-line table is new
	// territory for this mechanism this session (previously only proven at
	// 400 lines, progressive-looking geometry) -- watch for the picture
	// wrapping/shifting in VICE and adjust tablelength/linespertick if 700
	// doesn't match what the hardware is actually scanning per real frame.
	raster_music_irq_start(colortable, 700, 4, 0);

	// KERNAL/BASIC ROM is banked out while the IRQ is active (see
	// raster_music_irq_start()), so GETIN-based vdcwin_checkch() can't be
	// used here -- direct keyboard matrix scan instead.
	do
	{
		keyb_poll();
	} while (keyb_key == KSCAN_MAX);

	raster_music_irq_stop();

	vdc_cls();
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

	vdc_init(VDC_TEXT_80x25_PAL, 1);

	// This demo's hires effects (title_screen(), mono_colorize_demo(),
	// plasma_demo(), rotate_demo()) all use bitmap modes whose framebuffer
	// alone exceeds the VDC's base 16KB (640x400 mono is 32000 bytes by
	// itself) -- they need the extended 64KB VDC memory to exist at all.
	// vdc_set_mode() silently no-ops when a mode needs more memory than is
	// present, so without this check the demo would just show a blank/
	// stuck screen on a 16KB VDC instead of a clear reason why. memsize is
	// populated by vdc_detect_mem_size(), called from the vdc_init() above.
	if (vdc_state.memsize != 64)
	{
		vdc_prints(5, 5, "This demo requires a VDC with 64 KB RAM.");
		vdc_prints(5, 6, "Your VDC only has 16 KB -- exiting.");
		vdc_prints(5, 8, "Press a key to continue.");
		while (!vdcwin_checkch())
		{
		}
		vdc_exit();
		return 0;
	}

	raster_calibrate();

	raster_place_test();

	title_screen();

	mono_colorize_demo();

	fli_color_demo();

	mono_hires_xl_demo();

	plasma_demo(VDC_HIRES_640x200_Color_PAL);

	rotate_demo(VDC_HIRES_640x200_Color_PAL);

	vdc_exit();

	return 0;
}
