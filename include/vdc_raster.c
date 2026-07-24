#include <stdlib.h>
#include <petscii.h>
#include <c64/cia.h>
#include <c128/vdc.h>
#include <c128/mmu.h>
#include "vdc_core.h"
#include "vdc_raster.h"
#include "banking.h"

// Sane defaults in case raster_calibrate() is never called.
__zeropage char raster_timer_reload = 62;
unsigned raster_cycles_per_line_x1000 = 63056;

// raster_waitline()/raster_synch() (below): C translation of the CIA-timer
// VDC raster sync routine from "64'er Sonderheft 95", "VDC-Intromaker:
// Perfektes Rasterzeilen-Timing" (p.45) -- see the credits at the top of
// vdc_raster.h. Adapted: C function boundaries/calling convention instead
// of the article's plain 6502 subroutine labels; register/variable choices
// otherwise unchanged (same CIA2 Timer A/B chaining, same $D600 bit 5 sync
// point, same self-modifying NOP-jump-table for sub-line precision).
void raster_waitline(char rasterline)
{
    __asm
    {
        lda rasterline

    rw_wait1:
        cmp $dd06
        bne rw_wait1

        lsr
        lsr
        lsr
        lsr
        sta rw_waitjump+1

    rw_waitjump:
        bne rw_lineend
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
    rw_lineend:

    };
}

void raster_bar_begin()
// Call once per frame before any raster_bar_line()/raster_bar_segment()
// calls: selects the VDC colour register and starts the CIA2 raster sync
// for this synced sweep. Wraps the vdc.addr = VDCR_COLOR + raster_synch()
// pair previously duplicated at each bar-drawing call site.
{
    vdc.addr = VDCR_COLOR;
    raster_synch();
}

void raster_bar_line(char line, char color)
// Waits for `line` then writes a single fixed colour to the VDC colour
// register. The "cap line" pattern used for one-off colour changes between
// animated segments (e.g. title_screen()'s border lines around its
// animated rainbow run). Must be called between raster_bar_begin() and
// raster_bar_end(), with VDCR_COLOR already selected.
{
    raster_waitline(line);
    vdc.data = color;
}

char raster_bar_segment(char line, const char *colors, unsigned char count)
// Writes `count` colours from the colors[] table, one per rasterline,
// starting at `line` and decrementing it each time -- the "count lines from
// a palette" loop shared by every bar effect that walks a gradient or
// rotating palette rather than a single fixed colour. Returns the line one
// past the last line drawn, ready to pass into a following
// raster_bar_line()/raster_bar_segment() call in the same sweep (matches
// title_screen()'s multi-segment layout).
//
// Takes/returns `line` by value, not by pointer: this loop is cycle-critical
// (raster_waitline()'s sub-line timing assumes a fixed cycle count between
// "target reached" and "colour written"), and a pointer parameter adds an
// extra memory indirection on every access inside the loop -- enough extra
// cycles per line, accumulated over a long segment, to visibly destabilize
// the timing. Confirmed live: an earlier pointer-based version of this
// function caused exactly that in title_screen()'s 60-line segment.
{
    unsigned char i;

    for (i = 0; i < count; i++)
    {
        raster_bar_line(line, colors[i]);
        line--;
    }

    return line;
}

void raster_bar_end()
// Call once per frame after the last raster_bar_line()/raster_bar_segment()
// call: waits out the rest of the frame and re-enables interrupts. Wraps
// the vdc_wait_vblank() + inline cli pair previously duplicated at each
// bar-drawing call site. raster_synch() (called via raster_bar_begin())
// leaves interrupts disabled (SEI) for the duration of the sweep -- this is
// what re-enables them afterward.
{
    vdc_wait_vblank();
    __asm { cli }
}

char raster_bar_bounce(char pos, char low, char high, signed char *direction)
// Advances pos by *direction, flipping *direction when pos reaches low or
// high. Generalizes the bounce logic used by a bouncing bar effect (moves
// a bar's position back and forth between two rasterlines, one step per
// frame) into a reusable helper independent of any specific effect.
{
    pos += *direction;
    if (pos == low || pos == high)
    {
        *direction = -*direction;
    }
    return pos;
}

void raster_bar_draw_list(const struct RasterBarDef *bars, unsigned char count)
// Draws `count` independently-defined bars -- any mix of RASTERBAR_LINE
// (single fixed colour) and RASTERBAR_SEGMENT (palette-driven run) -- in
// one synced sweep, wrapping raster_bar_begin()/raster_bar_end() itself.
// Lets several bars be declared as data instead of writing out the
// sequence of raster_bar_line()/raster_bar_segment() calls by hand; use
// that lower-level pair directly instead when the bar layout is computed
// at runtime rather than fixed.
//
// Bars may be non-contiguous: raster_waitline() (via raster_bar_line()/
// raster_bar_segment()) waits for an explicit target line each time, so a
// gap between two bars is simply busy-waited through. List bars in
// descending rasterline order -- each bar's startline should be at or
// below the previous bar's final line -- matching a single top-to-bottom
// sweep; the list isn't sorted for you.
{
    unsigned char i;
    char line;

    raster_bar_begin();

    for (i = 0; i < count; i++)
    {
        line = bars[i].startline;
        if (bars[i].kind == RASTERBAR_SEGMENT)
        {
            raster_bar_segment(line, bars[i].colors, bars[i].length);
        }
        else
        {
            raster_bar_line(line, bars[i].color);
        }
    }

    raster_bar_end();
}

void raster_synch()
{
    __asm 
    {
        sei
        ldx #$7F
        stx $dd0d
        ldx raster_timer_reload
        stx $dd04
        ldx #$FF
        stx $dd06
        lda #$00
        sta $dd05
        sta $dd07
        lda #$20
    rs_wait1:
        bit $d600
        bne rs_wait1
        ldx #9
        nop
    rs_wait2:
        dex
        bne rs_wait2
        lda #$11
        sta $dd0e
        lda #$51
        sta $dd0f
    };
}

void raster_calibrate()
// Measure actual CPU cycles per VDC rasterline on this specific machine.
//
// Technique (independently reimplemented from first principles after
// reverse-engineering the idea from a VDC-timing "system analysis" screen
// seen in a 2006 C128 demo, which measures the same ratio the same way --
// CIA timer as a free-running cycle counter, VDC status register as the
// synchronization edge -- this is not a transplant of any external code):
//
// CIA1 Timer A/B are chained (B counts A's underflows), both started at
// $FFFF, and left running freely across 64 consecutive VDC VBlank pulses
// (a long sample window averages out jitter). Stopping the timers and
// reading how far they've counted down gives the total elapsed CPU cycles
// for those 64 frames. Dividing that by 64 and by the rasterlines/frame of
// whatever VDC mode is currently active (read back live from the VDC's own
// registers, so this works for PAL/NTSC and any character height) gives
// cycles-per-rasterline as a fixed-point value with 3 decimal digits.
//
// raster_synch()'s CIA2 Timer A reload assumes this value is a whole
// number of cycles; it isn't (typically ~63.0x), so a couple of cycles of
// drift accumulate for every extra rasterline a synced effect runs across.
// Calibrating rather than hardcoding at least removes the machine-to-machine
// guesswork of raster_place_test()-style hand-tuning.
{
    char i;
    char vtotal, csize;
    unsigned long elapsed;
    unsigned long linesperframe;
    unsigned long cpl_x1000;
    char underflows;

    __asm { sei }

    cia1.cra = 0x00;
    cia1.crb = 0x00;
    cia1.ta = 0xffff;
    cia1.tb = 0xffff;
    cia1.cra = 0x11; // start, force load, continuous
    cia1.crb = 0x51; // start, force load, continuous, count Timer A underflows

    for (i = 0; i < 64; i++)
    {
        vdc_wait_vblank();
        vdc_wait_no_vblank();
    }

    cia1.cra = 0x00;
    cia1.crb = 0x00;

    __asm { cli }

    // Timer B only ever loses a handful of counts across 64 frames, so its
    // high byte never changes -- only the low byte needs to be compared.
    underflows = 0xff - (char)(cia1.tb & 0xff);
    elapsed = (unsigned long)underflows * 65536UL + (0xffffUL - cia1.ta);

    // Rasterlines per frame for the currently active mode: VDC register 4
    // (vertical total, in character rows) and register 9 bits 0-4
    // (character height - 1) fully determine it: (VTOTAL+1) * (CHEIGHT+1).
    vtotal = vdc_reg_read(VDCR_VTOTAL);
    csize = vdc_reg_read(VDCR_CSIZE);
    linesperframe = (unsigned long)(vtotal + 1) * (unsigned long)((csize & 0x1f) + 1);

    cpl_x1000 = (elapsed * 1000UL) / (64UL * linesperframe);

    raster_cycles_per_line_x1000 = (unsigned)cpl_x1000;
    raster_timer_reload = (char)((cpl_x1000 + 500UL) / 1000UL);
}

// --- CIA1-driven raster colour + music IRQ ---
// Fields bundled into struct RasterIRQState -- see its own comment in
// vdc_raster.h. `active` (BSS, zero-initialized at startup): guards
// raster_irq_entry() against running its real logic -- and, critically,
// rearming CIA1's timer -- if it's ever reached before
// raster_music_irq_start() has genuinely installed it, or after
// raster_music_irq_stop() has torn it down. Confirmed live in VICE:
// Oscar64's C128E startup (crt.c, part of the toolchain, not this project)
// banks out KERNAL ROM as literally the first instruction of the compiled
// program, before main() ever runs and long before this project's own
// cia_init()/raster_music_irq_start() calls -- if the KERNAL's own
// still-active jiffy-clock interrupt fires during that window, it lands on
// whatever happens to be sitting at $fffe in the now-exposed, uninitialized
// RAM there. That's a one-off event outside this program's control, but
// raster_irq_entry() itself rearms CIA1's timer on every call, so a single
// spurious hit was turning into a *permanent* hang -- the handler kept
// re-triggering itself forever regardless of what caused the first one.
// Checking this flag breaks that self-sustaining loop: a spurious/premature
// call just acks CIA1 and returns without touching anything else.
struct RasterIRQState raster_irq;

void raster_irq_playframe()
// Play one SID music frame. Same $2003 call and Bank 1 + I/O convention as
// banking.c's sid_startmusic()/sid_interrupt() -- assumes music was already
// initialized the same way (init call to $2000 in Bank 1).
{
    char old = mmu.cr;
    mmu.cr = BNK_1_IO;
    __asm { jsr $2003 }
    mmu.cr = old;
}

__interrupt void raster_irq_tick()
// Runs once per raster_irq.linespertick VDC rasterlines: writes that many
// colour bytes from raster_irq.colortable to the VDC colour register
// (selecting it once, then relying on the VDC keeping that register
// selected across repeated data writes -- confirmed via reverse-engineering
// a reference demo's own raster bar routine, see rfo-vdc-calibration-notes.md).
// When the table is exhausted, plays one SID frame and restarts from the
// top for the next frame's redraw.
//
// __interrupt: kept for correctness (any __hwinterrupt-reachable function
// that does ordinary C computation should be __interrupt, per
// oscar64manual.md's "Interrupt handlers" note), but NOT the fix for a
// zero-page scratch corruption theory once suspected here -- checked
// directly against the compiler's generated assembly and disproved:
// Oscar64 automatically excludes every function reachable from a
// __hwinterrupt/__interrupt root (propagated transitively through the whole
// call chain) from its normal call-graph-based local-variable/scratch reuse
// analysis, specifically to prevent this class of bug. Separately, this
// handler is deliberately paced to consume close to its entire own reload
// period doing VDC-ready busy-waits (see the loop below), leaving
// foreground code almost no CPU time regardless of raster_irq.linespertick
// (which scales the work done per call and the reload period together, so
// the starvation ratio doesn't change) -- keypress polling was tried both
// from foreground code and from inside this function and neither reliably
// detected anything (root cause never found, see memory:
// mono_colorize_keypress_bug); callers must use raster_irq.framecount for a
// fixed-duration exit instead of waiting for a key while this is active.
{
    char n;

    // Prepare the reload for the *next* tick first: a fixed NOP pad can't
    // track a per-call-varying correction, so the fractional remainder is
    // instead diffused across ticks by occasionally arming reload+1.
    raster_irq.fracaccum_x1000 += raster_irq.fracpertick_x1000;
    if (raster_irq.fracaccum_x1000 >= 1000)
    {
        raster_irq.fracaccum_x1000 -= 1000;
        raster_irq.next_reload = raster_irq.reload + 1;
    }
    else
    {
        raster_irq.next_reload = raster_irq.reload;
    }

    vdc.addr = VDCR_COLOR;
    for (n = 0; n < raster_irq.linespertick; n++)
    {
        while (!(vdc.addr & 0x80))
        {
        }
        vdc.data = raster_irq.colortable[raster_irq.pos];
        raster_irq.pos++;
        if (raster_irq.pos >= raster_irq.tablelength)
        {
            raster_irq.pos = 0;

            // Keypress detection under this mechanism was tried both from
            // foreground code and from here (once per frame, where CPU time
            // is guaranteed) -- neither ever reliably detected a real
            // keypress, root cause never found despite extensive live
            // diagnosis (see memory: mono_colorize_keypress_bug). Given
            // that, callers must not wait for a keypress while this is
            // active at all -- use raster_irq.framecount (below) to run for
            // a fixed duration instead, then call raster_music_irq_stop()
            // and check for a keypress afterwards via the normal
            // vdcwin_checkch() path, once KERNAL banking is back.
            raster_irq.framecount++;

            if (raster_irq.musicenabled)
            {
                raster_irq_playframe();
            }
            return;
        }
    }
}

__interrupt void raster_irq_worker(void)
// All of this handler's real C-level logic lives here, not in
// raster_irq_entry() (below) -- __interrupt gives this function its own
// zero-page save/restore prologue/epilogue, protecting the shared
// compiler-register scratch space ("accu"/"tmp"/"ip"/"addr" etc.) that
// this code's own expression evaluation and 16-bit arithmetic (the cia1.ta
// assignment included) needs, from whatever foreground C code this
// interrupt happens to preempt mid-computation. raster_irq_entry() being
// __hwinterrupt does NOT provide this protection -- __hwinterrupt only
// saves CPU registers -- so nothing that touches shared zero-page scratch
// can safely live directly in that function's own body; it has to be
// isolated in a proper __interrupt worker like this one instead. See
// raster_irq_tick()'s comment for the live-confirmed failure this fixes
// (previously, only raster_irq_tick() itself had this attribute, but
// this active-check and the CIA1 rearm, sitting directly in
// raster_irq_entry()'s own body, were still unprotected and still
// corrupted foreground code on every firing).
//
// raster_irq.active is checked with a single exit point (if/no-else),
// never an early `return` -- the same class of epilogue-skipping bug this
// whole restructure fixes applies here too (oscar64manual.md's "Interrupt
// handlers" note): an early `return` was tried first and, on its own,
// also broke things (confirmed live) before this deeper zero-page issue
// was found underneath it. Don't reintroduce one.
{
    if (raster_irq.active)
    {
        raster_irq_tick(); // also computes raster_irq.next_reload for below

        cia1.ta = raster_irq.next_reload;
        cia1.cra = 0x19; // one-shot, force-load, start
    }
}

__hwinterrupt void raster_irq_entry(void)
// Direct hardware IRQ vector target ($fffe/$ffff) -- NOT chained through the
// KERNAL's $314 soft vector. Confirmed live in VICE (with the user's help
// interactively breaking into the monitor) that $314/$315 do not reliably
// hold a valid KERNAL continuation address in this project's boot context
// (it boots via a raw machine-code boot sector, never through a normal
// BASIC cold-start) -- one run read back as $ffff, and chaining to that
// sent the CPU into the weeds (crashed into BASIC's "?SYNTAX ERROR"/"BREAK"
// error paths). Rather than depend on that vector's contents, this installs
// directly on the CPU's own hardware vector while ROM is banked out (see
// raster_music_irq_start()), so there's nothing to save or chain to --
// __hwinterrupt generates the full register save/restore + RTI itself,
// correct for a function that IS the hardware entry point.
//
// Deliberately minimal: an inline-asm ack (pure asm, no C expression
// evaluation, so no zero-page usage at all) plus a single no-argument,
// no-return-value call into raster_irq_worker() (needs no zero-page
// scratch for the call mechanism itself, just a plain JSR/RTS). All real
// logic belongs in that __interrupt-attributed worker, never directly
// here -- see its comment for why.
//
// LDA $dc0d acks CIA1's ICR (it's read-to-clear; without reading it the
// chip's IRQ line stays asserted and this handler re-enters itself
// immediately instead of waiting for the next real Timer A underflow --
// confirmed via Oscar64's own rirq_isr_kernal_io in c64/rasterirq.c, which
// always reads $dc0d on its CIA1 path for the same reason).
{
    __asm { lda $dc0d }

    raster_irq_worker();
}

void raster_music_irq_start(const char *colortable, unsigned tablelength, char linespertick, char musicenabled)
// Installs the CIA1 raster+music IRQ directly on the hardware vector. Call
// once vdc_init()/raster_calibrate() have run and the colour table + VDC
// mode are ready. Supersedes sid_startmusic() while active, and banks out
// the KERNAL/BASIC/character ROM for the duration (I/O stays visible) --
// KERNAL calls (GETIN, file loading, etc.) will not work until
// raster_music_irq_stop() restores normal banking. Do not wait for a
// keypress while this is active (see raster_irq.framecount's comment) --
// run it for a fixed number of frames instead, then stop it and check for a
// keypress via vdcwin_checkch() afterwards.
{
    unsigned long truereload_x1000;

    raster_irq.colortable = colortable;
    raster_irq.tablelength = tablelength;
    raster_irq.pos = 0;
    raster_irq.linespertick = linespertick;
    raster_irq.musicenabled = musicenabled;
    raster_irq.framecount = 0;

    // Split the true (fractional) reload for this many lines into an
    // integer part (armed every tick) and a x1000-scaled remainder (diffused
    // across ticks by raster_irq_tick() -- see its comment).
    truereload_x1000 = (unsigned long)raster_cycles_per_line_x1000 * linespertick;
    raster_irq.reload = (unsigned)(truereload_x1000 / 1000UL);
    raster_irq.fracpertick_x1000 = (unsigned)(truereload_x1000 % 1000UL);
    raster_irq.fracaccum_x1000 = 0;
    raster_irq.next_reload = raster_irq.reload;

    __asm { sei }

    raster_irq.oldmmu = mmu.cr;
    mmu.cr = BMK_0_IO; // bank 0, full RAM (no KERNAL/BASIC/char ROM), I/O stays visible

    cia1.cra = 0x00; // stop Timer A while reprogramming it
    cia1.ta = raster_irq.reload;

    *(void (**)(void))0xfffe = raster_irq_entry; // install directly on the hardware vector

    cia1.icr = 0x81; // enable CIA1 Timer A interrupts
    cia1.cra = 0x19; // one-shot, force-load, start

    raster_irq.active = 1; // before cli: must be set before interrupts can fire

    __asm { cli }
}

void raster_music_irq_stop()
// Stops the CIA1 raster+music IRQ and restores normal ROM banking -- which
// also restores the real KERNAL hardware vector automatically (it's ROM
// underneath our RAM writes to $fffe/$ffff, unaffected by them; switching
// ROM back in just makes it visible again). Does not attempt to restore
// CIA1 Timer A's original jiffy-clock reload value (the 6526 doesn't expose
// a way to read back the previous latch, only the live countdown) -- a
// following vdc_exit()/reset (or the next effect calling
// raster_music_irq_start() or sid_startmusic() again) reprograms it anyway.
{
    raster_irq.active = 0; // first: any in-flight/pending interrupt now bails out safely

    __asm { sei }

    cia1.cra = 0x00; // stop Timer A
    cia1.icr = 0x7f; // disable + ack all CIA1 interrupt sources

    mmu.cr = raster_irq.oldmmu; // restores real KERNAL ROM + its real vector

    __asm { cli }
}