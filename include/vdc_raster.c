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
//
// Increments sid_expected_framecount (banking.h) here -- this is this
// section's own per-frame boundary, matching raster_bar_end()'s own
// fallback play call at the other end of the same frame. See
// sid_play_frame_foreground()'s comment (banking.c) for the full
// mechanism.
{
    sid_expected_framecount++;
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
// Attention point: takes/returns `line` by value, not by pointer. This loop
// is cycle-critical (raster_waitline()'s sub-line timing assumes a fixed
// cycle count between "target reached" and "colour written"), and a pointer
// parameter adds an extra memory indirection on every access inside the
// loop -- enough extra cycles per line, accumulated over a long segment
// (e.g. title_screen()'s 60-line segment), to visibly destabilize the
// timing. Keep this by-value.
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
//
// sid_play_frame_foreground() (banking.c) is called here, still under SEI,
// right after the frame's own vblank wait. Attention point: every
// Mechanism-1 section (this function is their shared per-frame sync point)
// holds interrupts disabled for most of each frame, starving Krill's
// interrupt-driven music of any reliable chance to fire on schedule (most
// noticeable in the FLI/MONO showcases, whose raster sweeps span most/all
// of the visible area) -- this one manual play call per frame, at a
// consistent frame-synced point, is what keeps music timing correct
// through those sections instead.
{
    vdc_wait_vblank();
    sid_play_frame_foreground();
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
// Synchronizes CIA2 Timer A/B to the VDC's own raster beam position (via
// $D600 bit 5) and arms them for raster_waitline()'s own cycle-exact
// sub-line timing. Leaves interrupts disabled (SEI, no matching CLI) --
// raster_bar_end() is what re-enables them once the sweep this starts is
// complete.
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
// seen in "Risen from Oblivion VDC v2" (Crest/Oxyron, 2006,
// https://csdb.dk/release/?id=44983), which measures the same ratio the
// same way -- CIA timer as a free-running cycle counter, VDC status
// register as the synchronization edge -- this is not a transplant of any
// external code):
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
// raster_music_irq_stop() has torn it down. Attention point: Oscar64's
// C128E startup (crt.c, part of the toolchain, not this project) banks out
// KERNAL ROM as literally the first instruction of the compiled program,
// before main() ever runs and long before this project's own
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

// Switch code generation to low memory (common-RAM, $0000-$1fff -- bnk_init()
// sets xmmu.rcr=0x06, 8KB common at the bottom of memory, banking.c) for
// raster_irq_playframe() below -- see its own comment for why this is
// required, not optional.
#pragma code(bcode1)
#pragma data(bdata1)
#pragma bss(bbss1)

__interrupt void raster_irq_playframe()
// Play one SID music frame (SIDPLAY, defines.h) -- Bank 1 + I/O, matching
// sid_music_init()'s own bank-switch (banking.c). Assumes the tune was
// already initialized via sid_music_init().
//
// Called from krill_interrupt (krill.c), once per VIC raster interrupt
// (every frame, ~50/60Hz) -- zero, one, or two SIDPLAY calls happen per
// IRQ, per the rate accumulator below, so the tune's own tempo stays
// correct even when it doesn't match the host's frame rate. NOT from
// foreground code any more. This is what makes music keep playing *during*
// an active krill_loadcompd() call
// (the whole point of using Krill in the first place: it keeps interrupts
// enabled throughout a background load), not just during sections that
// happen to have their own per-frame foreground loop. __interrupt (not
// plain void) is required, matching raster_irq_tick()/raster_irq_worker()'s
// own reasoning above: any function reachable from a hardware-interrupt
// context that does ordinary C computation (the `char old = mmu.cr` local
// here counts) needs Oscar64's own zero-page save/restore prologue/epilogue
// to protect the shared compiler-register scratch space from whatever
// foreground code this interrupt happens to preempt mid-computation.
//
// No sei/cli here (unlike this function's earlier, foreground-callable
// version) -- the hardware already set the CPU's I-flag on IRQ entry, and
// stays set until krill_interrupt's own RTI, so no nested interrupt can
// land mid-call regardless. Deliberately minimal for the same reason
// krill_interrupt's own callers care about timing: this runs inside
// Krill's own interrupt window during an active load, so it should stay as
// fast as the tune's own play routine allows, not add unnecessary overhead
// on top.
//
// Attention point: MUST live in low/common memory ($0000-$1fff, the bcode1
// segment banking.c's krill_init()/sid_music_init()/etc. already use), NOT
// the main program's ordinary code segment. BNK_1_IO (0x7e) switches to
// bank 1 -- a genuinely different physical 64KB RAM chip, not just a
// different ROM/IO overlay within the same bank (contrast
// raster_music_irq_start()'s BMK_0_IO, which stays within bank 0 and is
// therefore safe from ordinary code). Common RAM (bnk_init(),
// xmmu.rcr=0x06) is the only address range guaranteed identical across
// both banks; anything outside it -- like this function's own remaining
// instructions, if placed in the main program's ordinary (bank-0-only)
// code segment -- physically vanishes the instant mmu.cr switches banks,
// replaced by whatever's actually sitting in bank 1's own copy of that
// address range. The CPU keeps fetching from the same PC regardless, so it
// would silently execute that garbage instead of this function's own
// JSR/bank-restore/RTS, crashing into the KERNAL's BRK handler.
{
    // Attention point: deliberately unconditional, with no awareness of
    // sid_play_frame_foreground() at all. Coordination between this
    // function and that one is entirely sid_play_frame_foreground()'s own
    // responsibility (comparing sid_music_framecount against
    // sid_expected_framecount, a self-correcting comparison that stays
    // right regardless of what else is or isn't running) -- see that
    // function's own comment (banking.c). Do not introduce a shared
    // boolean "did someone already play this frame" flag between the two:
    // it can be left stale by contexts (like an active krill load) that
    // never touch it, and a self-correcting framecount comparison already
    // covers the same coordination without that failure mode.

    // Rate accumulator: decide how many SIDPLAY calls are due this IRQ,
    // from common-RAM state only, BEFORE the bank switch below -- keeps the
    // KERNAL-banked-out window exactly as short as it needs to be. See
    // banking.h's own comment on sid_rate_accum/sid_rate_inc and
    // sid_music_init() (banking.c) for how sid_rate_inc is chosen from the
    // tune's own PSID-header-derived tempo properties (defines.h) vs. the
    // host's detected video standard. plays is 1 on most IRQs (both
    // matched-standard and no-adjustment-needed cases collapse sid_rate_inc
    // to 0, which never trips either branch below -- identical to this
    // function's pre-accumulator behaviour); 2 when the tune's native rate
    // is faster than the host's frame rate and enough debt has accrued to
    // catch up; 0 when it's slower and enough debt has accrued to hold
    // back.
    char plays = 1;
    sid_rate_accum += sid_rate_inc;
    if (sid_rate_accum >= SID_RATE_SCALE)
    {
        sid_rate_accum -= SID_RATE_SCALE;
        plays = 2;
    }
    else if (sid_rate_accum <= -SID_RATE_SCALE)
    {
        sid_rate_accum += SID_RATE_SCALE;
        plays = 0;
    }

    if (plays == 0)
        return;

    char old = mmu.cr;
    // Defensive sei, no matching cli: hardware sets I=1 on IRQ entry, but
    // that's only a safe assumption if NOTHING earlier in the chain up to
    // here has already done its own cli before reaching this point (the
    // KERNAL's own hardware-IRQ dispatcher, at $ff17, runs before $314 is
    // ever reached, and may re-enable interrupts as part of its own
    // register-save convention -- not confirmed either way, so don't rely
    // on it). Leaving interrupts disabled afterward is fine: the eventual
    // RTI (via krill_interrupt's own jmp $ff33, reached through
    // sid_music_interrupt's chain) restores the real saved flags anyway,
    // same as it always would.
    __asm { sei }
    mmu.cr = BNK_1_IO;
    while (plays > 0)
    {
        // Restart the tune every SID_RESTART_FRAMES play calls instead of
        // playing one more frame -- Maniac.sid's own composed length/loop
        // point was never measured, and if its own internal loop-back jump
        // is one of the handful of addresses sidreloc's relocation left
        // "status undetermined" (see project memory), it may not loop on
        // its own at all. Restarting from our own side sidesteps needing to
        // know either way -- see banking.h's own comment on
        // SID_RESTART_FRAMES for the exact threshold (a rough guess, not
        // measured against this tune specifically). Never do a restart and
        // a second play in the same IRQ: drop any remaining due play and
        // zero the accumulator, both to cap worst-case handler length and
        // to avoid carrying a stale debt across the tune's own restart.
        if (++sid_music_framecount >= SID_RESTART_FRAMES)
        {
            sid_music_framecount = 0;
            // Resync sid_expected_framecount to match -- see
            // sid_play_frame_foreground()'s own comment (banking.c): left
            // un-resynced, expected would stay permanently offset ahead of
            // actual by ~SID_RESTART_FRAMES forever after this point,
            // making its own catch-up check ("am I behind schedule")
            // permanently true and firing on every call from then on, in
            // every section, not just genuinely SEI-starved ones.
            sid_expected_framecount = 0;
            sid_resetsid();
            __asm
            {
                lda #$00
                jsr SIDINIT
            }
            sid_rate_accum = 0;
            break;
        }
        else
        {
            __asm { jsr SIDPLAY }
        }
        plays--;
    }
    mmu.cr = old;
}

// Back to the main program's ordinary code segment for everything else in
// this file.
#pragma code(code)
#pragma data(data)
#pragma bss(bss)

__interrupt void raster_irq_tick()
// Runs once per raster_irq.linespertick VDC rasterlines: writes that many
// colour bytes from raster_irq.colortable to the VDC colour register
// (selecting it once, then relying on the VDC keeping that register
// selected across repeated data writes -- confirmed via reverse-engineering
// a reference demo's own raster bar routine, see rfo-vdc-calibration-notes.md).
// When the table is exhausted, plays one SID frame and restarts from the
// top for the next frame's redraw.
//
// __interrupt: required per oscar64manual.md's "Interrupt handlers" note --
// any __hwinterrupt-reachable function that does ordinary C computation
// needs it, since Oscar64 automatically excludes every function reachable
// from a __hwinterrupt/__interrupt root (propagated transitively through
// the whole call chain) from its normal call-graph-based local-variable/
// scratch reuse analysis. Attention point: this handler is deliberately
// paced to consume close to its entire own reload period doing VDC-ready
// busy-waits (see the loop below), leaving foreground code almost no CPU
// time regardless of raster_irq.linespertick (which scales the work done
// per call and the reload period together, so the starvation ratio doesn't
// change). Keypress detection does not work reliably while this mechanism
// is active, from either foreground code or from inside this function (see
// memory: mono_colorize_keypress_bug) -- callers must use
// raster_irq.framecount for a fixed-duration exit instead of waiting for a
// key while this is active.
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

            // Attention point: keypress detection does not work reliably
            // under this mechanism, from either foreground code or from
            // here (once per frame, where CPU time is guaranteed) -- see
            // memory: mono_colorize_keypress_bug. Callers must not wait for
            // a keypress while this is active at all -- use
            // raster_irq.framecount (below) to run for a fixed duration
            // instead, then call raster_music_irq_stop() and check for a
            // keypress afterwards via the normal vdcwin_checkch() path,
            // once KERNAL banking is back.
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
// raster_irq_entry() (below). Attention point: __interrupt gives this
// function its own zero-page save/restore prologue/epilogue, protecting
// the shared compiler-register scratch space ("accu"/"tmp"/"ip"/"addr"
// etc.) that this code's own expression evaluation and 16-bit arithmetic
// (the cia1.ta assignment included) needs, from whatever foreground C
// code this interrupt happens to preempt mid-computation. Being
// __hwinterrupt does NOT provide this protection -- __hwinterrupt only
// saves CPU registers -- so nothing that touches shared zero-page scratch
// can safely live directly in a __hwinterrupt function's own body; it has
// to be isolated in a proper __interrupt worker like this one instead.
//
// raster_irq.active is checked with a single exit point (if/no-else),
// never an early `return` -- the same epilogue-skipping hazard
// oscar64manual.md's "Interrupt handlers" note describes for
// __hwinterrupt/__interrupt functions generally. Don't reintroduce one.
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