#!/usr/bin/env python3
"""
VDC picture converter for vdcmodemania-oscar64.

Converts a source picture (any format Pillow can read) into the raw VDC
bitmap(+attribute) byte layout this project's bnk_load()/bnk_cpytovdc()
pipeline expects, for these modes:

  fli         -- 480x252, 8x1 colour cells, non-interlace (VDC_HIRES_480x252_Color_PAL)
  hfli        -- 640x400, 8x2 colour cells, non-interlace (VDC_HIRES_640x400_HFLI_PAL)
  ihfli       -- 640x480, 8x2 colour cells, interlace, even/odd field split (VDC_HIRES_640x480_IHFLI_NTSC)
  itfli       -- 640x576, 8x3 colour cells, interlace, even/odd field split (VDC_HIRES_640x576_ITFLI_PAL)
  imono       -- 720x700, monochrome, interlace, even/odd field split (VDC_HIRES_720x700_Mono_PAL)
  im800       -- 800x600, monochrome, interlace, even/odd field split (VDC_HIRES_800x600_IM800_PAL)
  titlescreen -- 640x400, monochrome, interlace, even/odd field split (VDC_HIRES_640x400_Mono_PAL),
                 title_screen()'s own picture (vdce-scrtit.eve/.odd)

hfli/ihfli/itfli share one generalized colour-cell converter
(convert_colour_cells(), cell_height 2 or 3) with fli (cell_height 1) --
same per-cell brute-force (back,fore) search and Floyd-Steinberg diffusion,
just evaluated/committed across cell_height rows at a time instead of 1.
hfli is non-interlace (single field, convert_colour_cells() directly);
ihfli/itfli are genuinely interlaced and go through
convert_split_colour_cells()/convert_colour_cells_paired() instead (see
below) -- their colour cells are chosen JOINTLY across both fields, not
independently.

**Interlace colour-blend technique (default for ihfli/itfli)**:
convert_colour_cells_paired() picks, per cell, between each field's own
independent best fit and a shared-background/per-field-best-foreground
alternative -- when the two fields' best foregrounds differ,
holding the background steady and alternating just the foreground by
field parity is the same technique the C128 "BASIC 8"/iPaint picture
format uses to fake shades beyond the native 16-colour VDC palette via
real interlace persistence. Identified and confirmed against real
disk-bundled iPaint sample pictures' own colour-index bytes (statistics
only, no copyrighted picture content reproduced), cross-checked against
GoDot's (github.com/godot64/GoDot) own iPaint saver/loader source
(savers/s_IPaint.a, savers/s_Basic8Mode1.a, loaders/l_Basic8Select.a,
commit 712196d) -- see defines.h's own credit block and
convert_colour_cells_paired()'s own docstring for the full writeup.
Always falls back to the old independent result when that's genuinely
better, so this only ever extends quality, never regresses it.

Every genuinely interlaced mode here (ihfli/itfli/imono/im800/titlescreen --
anything with LACE=3 in its vdc_core.c mode table row) splits its source
image into "even"/"odd" fields by row PARITY (even source rows -> even
field, odd source rows -> odd field), never by physical top-half/bottom-half
position -- confirmed directly against Tokra/Kircher's own original,
proven-working converters (original/v12/converters/source/ivdc_quant.c,
i3vdc_quant.c; original/v12/converters/vdcimono.bas, vdcim800.bas line 11).
An earlier version of this file used a physical-half split for
ihfli/itfli/imono/im800, and called the two fields "top"/"bottom"
throughout (title_screen's own mode already correctly used even/odd
content, just under the same misleading top/bottom naming) -- live-testing
on real photos (2026-07-26) showed exactly the symptom a physical-half
split produces: the picture's physical top half displaying, then being
replaced by its bottom half, not a single combined interlaced image. Fixed,
and renamed throughout (files, variables, comments) from top/bottom to
even/odd to describe what these actually are and avoid re-confusing the
two conventions again. See deinterlace_fields()'s own comment for the
mechanics.

Dithering technique (per-cell brute-force background/foreground colour pair
search with Floyd-Steinberg error diffusion, serpentine scan) is studied from
Michael Kircher's 2011 vdc_quant.c/hfli_quant.c, bundled unmodified in this
repo at original/v12/converters/source/ for reference -- those files carry
no license or copyright notice permitting reuse, so this is an independent
reimplementation of the same published technique, not a copy of that code.

Output files are prefixed with an arbitrary 2-byte load-address header
($00 $80) to match this project's existing asset convention -- bnk_load()
(include/banking.c) sets the KERNAL LOAD secondary address to 0, which makes
LOAD use the caller-supplied destination address and simply skip the file's
first 2 bytes, so their value doesn't matter.
"""

import argparse
import sys

from PIL import Image

# Standard C64/128 16-colour palette, VDC colour index order (0=black..15=white).
VDC_PALETTE = [
    (0, 0, 0),
    (85, 85, 85),
    (0, 0, 170),
    (85, 85, 255),
    (0, 170, 0),
    (85, 255, 85),
    (0, 170, 170),
    (85, 255, 255),
    (170, 0, 0),
    (255, 85, 85),
    (170, 0, 170),
    (255, 85, 255),
    (170, 85, 0),
    (255, 255, 85),
    (170, 170, 170),
    (255, 255, 255),
]

R_WEIGHT = 299
G_WEIGHT = 587
B_WEIGHT = 114

HEADER = bytes([0x00, 0x80])


def rgb_dist(c1, c2):
    dr = c1[0] - c2[0]
    dg = c1[1] - c2[1]
    db = c1[2] - c2[2]
    return R_WEIGHT * dr * dr + G_WEIGHT * dg * dg + B_WEIGHT * db * db


def clamp(v):
    return 0 if v < 0 else 255 if v > 255 else v


def quant_cell(src_row_getter, x0, y0, dx, back, fore, err_curr, err_next, commit):
    """Dither one 8-pixel-wide cell against a fixed (back, fore) colour pair.

    err_curr/err_next are per-column [r,g,b] accumulators for the *whole
    row* (sized width+2, with a 1-pixel padding lane on each side so
    diffusion can spill past column 0/width-1) -- indexed by true global
    column position (x0+x+1), not a position local to this cell. Returns
    (cost, byte).
    """
    logical = (back, fore)
    cost = 0.0
    byte = 0
    xs = range(0, 8) if dx == 1 else range(7, -1, -1)
    for x in xs:
        col = x0 + x
        r, g, b = src_row_getter(col, y0)
        r = clamp(r + ((err_curr[col + 1][0] + 8) >> 4))
        g = clamp(g + ((err_curr[col + 1][1] + 8) >> 4))
        b = clamp(b + ((err_curr[col + 1][2] + 8) >> 4))
        best_i, best_d = -1, None
        for i, ci in enumerate(logical):
            d = rgb_dist((r, g, b), VDC_PALETTE[ci])
            if best_d is None or d < best_d:
                best_d, best_i = d, i
        cost += best_d
        byte |= best_i << (7 - x)
        pr, pg, pb = VDC_PALETTE[logical[best_i]]
        dr, dg, db = r - pr, g - pg, b - pb
        if commit:
            i0 = col + 1 + dx
            i1 = col + 1
            i2 = col + 1 - dx
            err_curr[i0][0] += 7 * dr
            err_curr[i0][1] += 7 * dg
            err_curr[i0][2] += 7 * db
            err_next[i0][0] += dr
            err_next[i0][1] += dg
            err_next[i0][2] += db
            err_next[i1][0] += 5 * dr
            err_next[i1][1] += 5 * dg
            err_next[i1][2] += 5 * db
            err_next[i2][0] += 3 * dr
            err_next[i2][1] += 3 * dg
            err_next[i2][2] += 3 * db
    return cost, byte


def convert_fli(img, width, height):
    """480x252, 8x1 cells, non-interlace. Returns (bitmap_bytes, colour_bytes).

    Colour byte nibble order: (background<<4)|foreground -- background
    high, foreground low. This is the VDC's own real hardware convention
    for its bitmap attribute plane, not the (foreground<<4)|background
    convention register 26 (VDCR_COLOR, a separate, single global
    register) uses -- confirmed by live testing across real hardware,
    VICE, and z64k. Every colour-cell mode in this project
    (convert_colour_cells()/convert_colour_cells_paired()/
    convert_spectrum()) uses the same convention -- see project memory
    vdcmaniac_attribute_byte_nibble_order.md for the full diagnostic
    history if this class of bug ever needs revisiting for a new mode.
    """
    pixels = img.convert("RGB").load()

    def get(x, y):
        return pixels[x, y]

    cells_per_row = width // 8
    bitmap = bytearray(cells_per_row * height)
    colour = bytearray(cells_per_row * height)

    err_curr = [[0, 0, 0] for _ in range(width + 2)]
    err_next = [[0, 0, 0] for _ in range(width + 2)]

    for y in range(height):
        for c in range(len(err_next)):
            err_next[c] = [0, 0, 0]
        # Serpentine per-line direction, matching vdc_quant.c's quant_line().
        if y & 1:
            xs = range(0, width, 8)
            dx = 1
        else:
            xs = range(width - 8, -8, -8)
            dx = -1
        for x0 in xs:
            best_back = best_fore = -1
            best_cost = None
            for back in range(15):
                for fore in range(back + 1, 16):
                    cost, _ = quant_cell(get, x0, y, dx, back, fore, err_curr, err_next, commit=False)
                    if best_cost is None or cost < best_cost:
                        best_cost, best_back, best_fore = cost, back, fore
            _, byte = quant_cell(get, x0, y, dx, best_back, best_fore, err_curr, err_next, commit=True)
            bitmap[y * cells_per_row + (x0 >> 3)] = byte
            colour[y * cells_per_row + (x0 >> 3)] = (best_back << 4) | best_fore  # see docstring
        err_curr, err_next = err_next, [[0, 0, 0] for _ in range(width + 2)]
        if y % 20 == 0 or y == height - 1:
            print(f"FLI: line {y + 1}/{height}", file=sys.stderr)
    return bytes(bitmap), bytes(colour)


def convert_colour_cells(img, width, height, cell_height):
    """Generalized colour-cell converter: 8-pixel-wide x cell_height-row-tall
    cells, one shared (back,fore) colour pair per cell, cell_height bitmap
    bytes (one per row) but only one colour byte per cell. cell_height=1
    reduces to exactly convert_fli()'s own algorithm (verified byte-identical
    against it); cell_height=2/3 serve hfli/ihfli/itfli.

    Row 0 of each cell is exact: it's evaluated and committed against the
    real, continuously-chained err_curr/err_next state, identical to
    convert_fli()'s single-row treatment, so already-decided neighbour cells
    in the same row are always accounted for correctly. Rows 1..cell_height-1
    (only relevant when cell_height>1) use a small cell-local diffusion chain
    seeded from zero instead of the real row-wide state, because the real
    state for those rows depends on neighbour cells in row 0 that haven't
    been decided yet at this point in the scan -- this is a deliberate,
    documented approximation (misses the 3/16 and 1/16 diagonal spread from
    not-yet-decided neighbours for those deeper rows only), not a bug.
    """
    pixels = img.convert("RGB").load()

    def get(x, y):
        return pixels[x, y]

    cells_per_row = width // 8
    cell_rows = height // cell_height
    bitmap = bytearray(cells_per_row * height)
    colour = bytearray(cells_per_row * cell_rows)

    err_curr = [[0, 0, 0] for _ in range(width + 2)]
    err_next = [[0, 0, 0] for _ in range(width + 2)]

    def local_chain_cost(back, fore, x0, y0, dx):
        """Sum of quant_cell() cost for rows 1..cell_height-1 of this cell,
        chained through a small zero-seeded local buffer (10 = 8 + 1-pixel
        padding on each side, matching quant_cell()'s col+1+-dx indexing when
        called with a local x0 of 0)."""
        if cell_height == 1:
            return 0.0
        get_local = lambda lx, ly: get(x0 + lx, ly)
        local_curr = [[0, 0, 0] for _ in range(10)]
        local_next = [[0, 0, 0] for _ in range(10)]
        quant_cell(get_local, 0, y0, dx, back, fore, local_curr, local_next, commit=True)
        local_curr, local_next = local_next, [[0, 0, 0] for _ in range(10)]
        total = 0.0
        for r in range(1, cell_height):
            cost, _ = quant_cell(get_local, 0, y0 + r, dx, back, fore, local_curr, local_next, commit=False)
            total += cost
            if r < cell_height - 1:
                quant_cell(get_local, 0, y0 + r, dx, back, fore, local_curr, local_next, commit=True)
                local_curr, local_next = local_next, [[0, 0, 0] for _ in range(10)]
        return total

    def commit_local_rows(back, fore, x0, y0, dx):
        """Re-run the winning pair's local chain with commit=True and write
        the resulting bytes for rows 1..cell_height-1 into bitmap."""
        get_local = lambda lx, ly: get(x0 + lx, ly)
        local_curr = [[0, 0, 0] for _ in range(10)]
        local_next = [[0, 0, 0] for _ in range(10)]
        quant_cell(get_local, 0, y0, dx, back, fore, local_curr, local_next, commit=True)
        local_curr, local_next = local_next, [[0, 0, 0] for _ in range(10)]
        for r in range(1, cell_height):
            _, byte = quant_cell(get_local, 0, y0 + r, dx, back, fore, local_curr, local_next, commit=True)
            bitmap[(y0 + r) * cells_per_row + (x0 >> 3)] = byte
            if r < cell_height - 1:
                local_curr, local_next = local_next, [[0, 0, 0] for _ in range(10)]

    for cr in range(cell_rows):
        y0 = cr * cell_height
        for c in range(len(err_next)):
            err_next[c] = [0, 0, 0]
        if cr & 1:
            xs = range(0, width, 8)
            dx = 1
        else:
            xs = range(width - 8, -8, -8)
            dx = -1
        for x0 in xs:
            best_back = best_fore = -1
            best_cost = None
            for back in range(15):
                for fore in range(back + 1, 16):
                    cost, _ = quant_cell(get, x0, y0, dx, back, fore, err_curr, err_next, commit=False)
                    cost += local_chain_cost(back, fore, x0, y0, dx)
                    if best_cost is None or cost < best_cost:
                        best_cost, best_back, best_fore = cost, back, fore
            _, byte0 = quant_cell(get, x0, y0, dx, best_back, best_fore, err_curr, err_next, commit=True)
            bitmap[y0 * cells_per_row + (x0 >> 3)] = byte0
            colour[cr * cells_per_row + (x0 >> 3)] = (best_back << 4) | best_fore  # see convert_fli()'s docstring
            if cell_height > 1:
                commit_local_rows(best_back, best_fore, x0, y0, dx)
        err_curr, err_next = err_next, [[0, 0, 0] for _ in range(width + 2)]
        if cr % 10 == 0 or cr == cell_rows - 1:
            print(f"colour-cells: cell-row {cr + 1}/{cell_rows}", file=sys.stderr)
    return bytes(bitmap), bytes(colour)


def convert_imono(img, width, height):
    """720x700 monochrome, plain Floyd-Steinberg 1-bit dither. Returns bitmap_bytes."""
    gray = img.convert("L")
    px = gray.load()
    err_curr = [0] * (width + 2)
    err_next = [0] * (width + 2)
    bytes_per_row = width // 8
    bitmap = bytearray(bytes_per_row * height)

    for y in range(height):
        for c in range(len(err_next)):
            err_next[c] = 0
        if y & 1:
            xs = range(width)
            dx = 1
        else:
            xs = range(width - 1, -1, -1)
            dx = -1
        row_bits = [0] * width
        for x in xs:
            val = clamp(px[x, y] + ((err_curr[x + 1] + 8) >> 4))
            bit = 1 if val >= 128 else 0
            row_bits[x] = bit
            err = val - (255 if bit else 0)
            i0, i1, i2 = x + 1 + dx, x + 1, x + 1 - dx
            if 0 <= i0 < len(err_curr):
                err_curr[i0] += (7 * err) // 16
            if 0 <= i1 < len(err_next):
                err_next[i1] += (5 * err) // 16
            if 0 <= i2 < len(err_next):
                err_next[i2] += (3 * err) // 16
            err_next[x + 1] += err // 16
        for bx in range(bytes_per_row):
            byte = 0
            for b in range(8):
                byte |= row_bits[bx * 8 + b] << (7 - b)
            bitmap[y * bytes_per_row + bx] = byte
        err_curr, err_next = err_next, [0] * (width + 2)
        if y % 50 == 0 or y == height - 1:
            print(f"IMONO: line {y + 1}/{height}", file=sys.stderr)
    return bytes(bitmap)


def fit_to_size(img, width, height, crop_top=None, crop_left=None):
    """Crop img to the target aspect ratio, then resize to exact (width,
    height) -- so any source photo can be dropped in as-is rather than
    requiring it to be pre-cropped to the exact VDC resolution. Defaults to
    a center crop; pass crop_top/crop_left (source-pixel offsets) to anchor
    the crop window elsewhere instead -- e.g. a portrait photo where the
    subject's face/ears sit above center, so a blind center crop cuts them
    off (found live with im8002_kellyleeowens.jpg/im8003_cat.jpg -- see
    those --crop-top call sites in main() below)."""
    src_w, src_h = img.size
    target_ratio = width / height
    src_ratio = src_w / src_h
    if src_ratio > target_ratio:
        # Source is wider than target -- crop left/right.
        new_w = round(src_h * target_ratio)
        left = crop_left if crop_left is not None else (src_w - new_w) // 2
        img = img.crop((left, 0, left + new_w, src_h))
    elif src_ratio < target_ratio:
        # Source is taller than target -- crop top/bottom.
        new_h = round(src_w / target_ratio)
        top = crop_top if crop_top is not None else (src_h - new_h) // 2
        img = img.crop((0, top, src_w, top + new_h))
    return img.resize((width, height), Image.LANCZOS)


def deinterlace_fields(img, width, height):
    """Split img into (even_rows_image, odd_rows_image), each width x
    (height//2) -- row 0,2,4... go to the first, 1,3,5... to the second.
    Confirmed against Tokra/Kircher's own original, proven-working
    converters (original/v12/converters/source/ivdc_quant.c, lines
    192-222; i3vdc_quant.c, same structure; converters/vdcimono.bas and
    vdcim800.bas line 11: `if y/2=int(y/2) then print#3 else print#4`) --
    every one of them splits genuinely interlaced VDC modes by row PARITY
    into "top"/"bot", never by physical top-half/bottom-half position. An
    earlier version of this function (and the imono/im800 mode branches
    below) used a physical-half crop instead -- live-testing on real photos
    (2026-07-26) showed this as the picture's physical top half displayed,
    then replaced by its bottom half, rather than a single interlaced
    image; the fix is this function, confirmed directly against the
    original reference source above rather than by further guessing."""
    half = height // 2
    even_img = Image.new(img.mode, (width, half))
    odd_img = Image.new(img.mode, (width, half))
    src = img.load()
    even_px = even_img.load()
    odd_px = odd_img.load()
    for y in range(half):
        for x in range(width):
            even_px[x, y] = src[x, 2 * y]
            odd_px[x, y] = src[x, 2 * y + 1]
    return even_img, odd_img


def convert_colour_cells_paired(even_img, odd_img, width, height, cell_height):
    """Joint even/odd colour-cell converter for genuinely interlaced modes
    (ihfli/itfli) -- the DEFAULT for these modes now, replacing two fully
    independent convert_colour_cells() passes.

    At every cell this evaluates two strategies and keeps whichever gives
    lower total (even+odd) dithering error:
      - independent: each field freely picks its own best (back,fore) pair
        (today's old behaviour -- the two fields may end up with different
        backgrounds too).
      - shared-bg blend: one background shared by both fields, each field
        picks its own best foreground given that fixed background. When
        the two fields' best foregrounds differ, this reproduces the real,
        confirmed C128 "BASIC 8"/iPaint colour-cell technique (credited in
        defines.h -- GoDot's own saver/loader source, github.com/godot64/
        GoDot): hold the background steady and alternate ONE foreground
        pair by field parity, so real VDC interlace persistence blends
        the two into a shade the native 16-colour palette doesn't have on
        its own -- confirmed against real disk-bundled iPaint sample
        pictures via colour-index statistics (44% of all differing cells
        in one real sample reused a single repeated foreground
        substitution with an unchanged background -- not noise). No
        copyrighted image content was rendered/viewed to derive this,
        only decoded colour-index bytes.

    Falls back to independent whenever it's genuinely better (flat
    regions where both already agree, or areas where the two fields'
    true content really does differ) -- this never produces worse output
    than the old always-independent behaviour, only extends it.

    Roughly 3x convert_colour_cells()'s own per-cell search cost (adds a
    16-background x 2-field foreground search on top of the existing
    120-pair x 2-field independent search) -- an offline asset-build
    tool, so the extra runtime is an acceptable trade for the improved
    output.
    """
    pixels_e = even_img.convert("RGB").load()
    pixels_o = odd_img.convert("RGB").load()

    def get_e(x, y):
        return pixels_e[x, y]

    def get_o(x, y):
        return pixels_o[x, y]

    cells_per_row = width // 8
    cell_rows = height // cell_height
    bitmap_e = bytearray(cells_per_row * height)
    bitmap_o = bytearray(cells_per_row * height)
    colour_e = bytearray(cells_per_row * cell_rows)
    colour_o = bytearray(cells_per_row * cell_rows)

    err_curr_e = [[0, 0, 0] for _ in range(width + 2)]
    err_next_e = [[0, 0, 0] for _ in range(width + 2)]
    err_curr_o = [[0, 0, 0] for _ in range(width + 2)]
    err_next_o = [[0, 0, 0] for _ in range(width + 2)]

    def local_chain_cost(get, back, fore, x0, y0, dx):
        # Same deliberate zero-seeded-local-buffer approximation as
        # convert_colour_cells()'s own local_chain_cost -- see its
        # docstring for why rows 1..cell_height-1 can't use the real
        # row-wide diffusion state.
        if cell_height == 1:
            return 0.0
        get_local = lambda lx, ly: get(x0 + lx, ly)
        local_curr = [[0, 0, 0] for _ in range(10)]
        local_next = [[0, 0, 0] for _ in range(10)]
        quant_cell(get_local, 0, y0, dx, back, fore, local_curr, local_next, commit=True)
        local_curr, local_next = local_next, [[0, 0, 0] for _ in range(10)]
        total = 0.0
        for r in range(1, cell_height):
            cost, _ = quant_cell(get_local, 0, y0 + r, dx, back, fore, local_curr, local_next, commit=False)
            total += cost
            if r < cell_height - 1:
                quant_cell(get_local, 0, y0 + r, dx, back, fore, local_curr, local_next, commit=True)
                local_curr, local_next = local_next, [[0, 0, 0] for _ in range(10)]
        return total

    def commit_local_rows(get, back, fore, x0, y0, dx, bitmap):
        get_local = lambda lx, ly: get(x0 + lx, ly)
        local_curr = [[0, 0, 0] for _ in range(10)]
        local_next = [[0, 0, 0] for _ in range(10)]
        quant_cell(get_local, 0, y0, dx, back, fore, local_curr, local_next, commit=True)
        local_curr, local_next = local_next, [[0, 0, 0] for _ in range(10)]
        for r in range(1, cell_height):
            _, byte = quant_cell(get_local, 0, y0 + r, dx, back, fore, local_curr, local_next, commit=True)
            bitmap[(y0 + r) * cells_per_row + (x0 >> 3)] = byte
            if r < cell_height - 1:
                local_curr, local_next = local_next, [[0, 0, 0] for _ in range(10)]

    def best_independent(get, err_curr, err_next, x0, y0, dx):
        best_cost, best_back, best_fore = None, -1, -1
        for back in range(15):
            for fore in range(back + 1, 16):
                cost, _ = quant_cell(get, x0, y0, dx, back, fore, err_curr, err_next, commit=False)
                cost += local_chain_cost(get, back, fore, x0, y0, dx)
                if best_cost is None or cost < best_cost:
                    best_cost, best_back, best_fore = cost, back, fore
        return best_cost, best_back, best_fore

    def best_fore_given_bg(get, err_curr, err_next, bg, x0, y0, dx):
        best_cost, best_fore = None, -1
        for fore in range(16):
            if fore == bg:
                continue
            cost, _ = quant_cell(get, x0, y0, dx, bg, fore, err_curr, err_next, commit=False)
            cost += local_chain_cost(get, bg, fore, x0, y0, dx)
            if best_cost is None or cost < best_cost:
                best_cost, best_fore = cost, fore
        return best_cost, best_fore

    for cr in range(cell_rows):
        y0 = cr * cell_height
        for c in range(len(err_next_e)):
            err_next_e[c] = [0, 0, 0]
            err_next_o[c] = [0, 0, 0]
        if cr & 1:
            xs = range(0, width, 8)
            dx = 1
        else:
            xs = range(width - 8, -8, -8)
            dx = -1
        for x0 in xs:
            cost_e, back_e_ind, fore_e_ind = best_independent(get_e, err_curr_e, err_next_e, x0, y0, dx)
            cost_o, back_o_ind, fore_o_ind = best_independent(get_o, err_curr_o, err_next_o, x0, y0, dx)
            independent_cost = cost_e + cost_o

            best_shared = None  # (cost, bg, fore_e, fore_o)
            for bg in range(16):
                be_cost, be_fore = best_fore_given_bg(get_e, err_curr_e, err_next_e, bg, x0, y0, dx)
                bo_cost, bo_fore = best_fore_given_bg(get_o, err_curr_o, err_next_o, bg, x0, y0, dx)
                total = be_cost + bo_cost
                if best_shared is None or total < best_shared[0]:
                    best_shared = (total, bg, be_fore, bo_fore)

            if best_shared[0] < independent_cost:
                _, bg, fore_e, fore_o = best_shared
                back_e, back_o = bg, bg
            else:
                back_e, fore_e = back_e_ind, fore_e_ind
                back_o, fore_o = back_o_ind, fore_o_ind

            _, byte_e0 = quant_cell(get_e, x0, y0, dx, back_e, fore_e, err_curr_e, err_next_e, commit=True)
            bitmap_e[y0 * cells_per_row + (x0 >> 3)] = byte_e0
            colour_e[cr * cells_per_row + (x0 >> 3)] = (back_e << 4) | fore_e  # see convert_fli()'s docstring
            if cell_height > 1:
                commit_local_rows(get_e, back_e, fore_e, x0, y0, dx, bitmap_e)

            _, byte_o0 = quant_cell(get_o, x0, y0, dx, back_o, fore_o, err_curr_o, err_next_o, commit=True)
            bitmap_o[y0 * cells_per_row + (x0 >> 3)] = byte_o0
            colour_o[cr * cells_per_row + (x0 >> 3)] = (back_o << 4) | fore_o  # see convert_fli()'s docstring
            if cell_height > 1:
                commit_local_rows(get_o, back_o, fore_o, x0, y0, dx, bitmap_o)

        err_curr_e, err_next_e = err_next_e, [[0, 0, 0] for _ in range(width + 2)]
        err_curr_o, err_next_o = err_next_o, [[0, 0, 0] for _ in range(width + 2)]
        if cr % 10 == 0 or cr == cell_rows - 1:
            print(f"colour-cells (blend-aware): cell-row {cr + 1}/{cell_rows}", file=sys.stderr)

    return bytes(bitmap_e), bytes(colour_e), bytes(bitmap_o), bytes(colour_o)


def convert_split_colour_cells(img, width, height, cell_height):
    """ihfli/itfli's interlaced modes: deinterlace into even/odd-row fields
    *before* conversion (see deinterlace_fields()'s comment for why this
    must happen before, not after, cell-based colour conversion -- each
    field's own colour cells pair same-parity source rows, e.g. source
    rows 0 and 2 share one colour byte in the even field, not source rows 0
    and 1), then run the two fields through convert_colour_cells_paired()'s
    joint blend-aware search (see its own docstring) rather than two fully
    independent conversions -- this is the default for both interlaced
    colour modes now, not an opt-in."""
    even_img, odd_img = deinterlace_fields(img, width, height)
    half = height // 2
    print("colour-cells: joint even/odd blend-aware conversion", file=sys.stderr)
    return convert_colour_cells_paired(even_img, odd_img, width, half, cell_height)


# ZX Spectrum 15-colour palette (INK/PAPER 3-bit value, plus BRIGHT) to
# VDC's own 16-native-colour indices (vdc_core.h's VDC_BLACK..VDC_WHITE
# enum) -- direct dim/bright pairing, no attempt at closer RGB matching
# since both palettes are small, fixed, named colour sets with an obvious
# 1:1 correspondence (black/blue/red/magenta/green/cyan/yellow/white,
# dim<->dark, bright<->light). Index = (bright << 3) | colour3bit.
SPECTRUM_TO_VDC = [
    0, 2, 8, 10, 4, 6, 12, 14,  # dim: black,blue,red,magenta,green,cyan,yellow,white(->lgrey)
    0, 3, 9, 11, 5, 7, 13, 15,  # bright: black(unchanged),blue,red,magenta,green,cyan,yellow,white
]


def convert_spectrum(scr_bytes):
    """Decode a real ZX Spectrum .scr (exactly 6912 bytes: 6144-byte
    bitmap + 768-byte attribute plane, the standard raw SCREEN$ memory
    dump -- see justsolve.archiveteam.org/wiki/SCR_(ZX_Spectrum)) into
    vdcmaniac's own VDC output.

    Deliberately reuses VDC_HIRES_640x200_Color_PAL's already-proven
    timing completely unchanged -- no new vdc_modes[] row, no new
    horizontal-timing register values. Confirmed this is the same
    category of choice Tokra's own reference implementation makes (see
    defines.h's credit block): disassembling his scr-copy.bin ("VDC
    SpectruMania", tokra.de/c128/vdcspectrumania.zip) shows zero writes to
    any of the VDC's horizontal/vertical timing registers (0-9) anywhere
    -- it runs entirely inside whatever VDC mode was already active,
    manipulating only bitmap/attribute *addressing* (registers 18/19/25/
    28/31) -- not a genuinely narrower display, just clever content
    placement within a fixed-width canvas. No code from that release is
    used here, only the disassembly-verified approach. This function does
    the same:
    the Spectrum's 256x192 picture is pixel-doubled to 512 VDC pixels
    wide (matching the readme's own "standard-pixel-width-mode", which
    stores literal doubled bytes rather than relying on the OTHER,
    unproven "double-pixel-width-mode" hardware trick), centred with an
    8-VDC-char blank border on each side (640 - 512 = 128px = 16 chars,
    8 each side), and one blank top VDC character row (200 - 192 = 8
    lines = exactly 1 char row) so every Spectrum attribute cell lands on
    a whole VDC character row -- no fractional-row splitting anywhere.

    Returns (bitmap_bytes, colour_bytes), each already the full 640x200/
    80x25 canvas size (cells outside the centred picture are left 0x00 --
    black bitmap bits under a black-on-black colour byte, i.e. a plain
    black border).
    """
    if len(scr_bytes) != 6912:
        print(f"warning: expected exactly 6912-byte .scr, got {len(scr_bytes)}", file=sys.stderr)
    bitmap_src = scr_bytes[:6144]
    attr_src = scr_bytes[6144:6912]

    # Un-interleave the Spectrum's own classic non-linear scanline
    # addressing into a clean row-major 32-bytes/row x 192-row buffer.
    # addr = third*2048 + line*256 + charrow*32 + x  (third=Y>>6,
    # charrow=(Y>>3)&7, line=Y&7) -- the well-documented public SCREEN$
    # layout, see this function's own docstring for the format reference.
    rows = [None] * 192
    for y in range(192):
        third = y >> 6
        charrow = (y >> 3) & 7
        line = y & 7
        base = third * 2048 + line * 256 + charrow * 32
        rows[y] = bitmap_src[base:base + 32]

    cells_per_row = 80  # 640 / 8
    bitmap = bytearray(cells_per_row * 200)
    colour = bytearray(cells_per_row * 25)

    left_char = 8  # (80 - 64) // 2 -- 64 VDC char columns = 512 doubled px
    top_row = 1    # one blank VDC char row of top margin

    for y in range(192):
        vdc_y = top_row * 8 + y
        src_row = rows[y]
        for sx in range(32):  # 32 source bytes = 256 source pixels
            srcbyte = src_row[sx]
            outbits = 0
            for b in range(8):
                bit = (srcbyte >> (7 - b)) & 1
                outbits = (outbits << 2) | (0b11 if bit else 0b00)
            vdc_col0 = left_char + sx * 2
            bitmap[vdc_y * cells_per_row + vdc_col0] = (outbits >> 8) & 0xFF
            bitmap[vdc_y * cells_per_row + vdc_col0 + 1] = outbits & 0xFF

    for cy in range(24):  # 192 / 8
        vdc_cy = top_row + cy
        attr_row = attr_src[cy * 32:(cy + 1) * 32]
        for sx in range(32):
            a = attr_row[sx]
            bright = (a >> 6) & 1
            paper = (a >> 3) & 7
            ink = a & 7
            fg = SPECTRUM_TO_VDC[(bright << 3) | ink]
            bg = SPECTRUM_TO_VDC[(bright << 3) | paper]
            # (background<<4)|foreground -- see convert_fli()'s own
            # docstring for the full hardware-convention explanation.
            cb = (bg << 4) | fg
            vdc_col0 = left_char + sx * 2
            colour[vdc_cy * cells_per_row + vdc_col0] = cb
            colour[vdc_cy * cells_per_row + vdc_col0 + 1] = cb

    return bytes(bitmap), bytes(colour)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--mode",
        choices=["fli", "hfli", "ihfli", "itfli", "imono", "im800", "titlescreen", "vscroll", "spectrum"],
        required=True,
    )
    ap.add_argument("--input", required=True, help="Source image (any Pillow-readable format, any size/aspect)")
    ap.add_argument("--out-prefix", required=True, help="Output path prefix; writes <prefix>.bit/.col or <prefix>.bit")
    ap.add_argument("--crop-top", type=int, default=None, help="Override fit_to_size()'s center crop with this source-pixel top offset (for portrait photos where the subject sits above center)")
    ap.add_argument("--crop-left", type=int, default=None, help="Trim this many source pixels off the LEFT edge before fit_to_size() runs -- for vscroll mode, whose own source is proportionally narrower than the target (so fit_to_size takes its top/bottom-crop branch and always keeps the full source width); shifts the visible frame right, e.g. away from a decorative border pillar.")
    args = ap.parse_args()

    if args.mode == "spectrum":
        # Raw .scr byte dump, not a Pillow-openable image -- handled
        # entirely separately, before the Image.open() every other mode
        # needs.
        scr_bytes = open(args.input, "rb").read()
        bitmap, colour = convert_spectrum(scr_bytes)
        with open(args.out_prefix + ".bit", "wb") as f:
            f.write(HEADER + bitmap)
        with open(args.out_prefix + ".col", "wb") as f:
            f.write(HEADER + colour)
        print(f"wrote {args.out_prefix}.bit ({len(bitmap)} bytes) and .col ({len(colour)} bytes)")
        return

    img = Image.open(args.input)
    if args.crop_left:
        img = img.crop((args.crop_left, 0, img.width, img.height))

    if args.mode == "fli":
        width, height = 480, 252
        img = fit_to_size(img, width, height)
        bitmap, colour = convert_fli(img, width, height)
        with open(args.out_prefix + ".bit", "wb") as f:
            f.write(HEADER + bitmap)
        with open(args.out_prefix + ".col", "wb") as f:
            f.write(HEADER + colour)
        print(f"wrote {args.out_prefix}.bit ({len(bitmap)} bytes) and .col ({len(colour)} bytes)")
    elif args.mode == "hfli":
        width, height = 640, 400
        img = fit_to_size(img, width, height)
        bitmap, colour = convert_colour_cells(img, width, height, cell_height=2)
        with open(args.out_prefix + ".bit", "wb") as f:
            f.write(HEADER + bitmap)
        with open(args.out_prefix + ".col", "wb") as f:
            f.write(HEADER + colour)
        print(f"wrote {args.out_prefix}.bit ({len(bitmap)} bytes) and .col ({len(colour)} bytes)")
    elif args.mode == "ihfli":
        # 640x480, cell_height=2, even/odd-row field split (see
        # deinterlace_fields()'s comment -- confirmed against Tokra/
        # Kircher's own original ivdc_quant.c, not a physical-half split).
        width, height = 640, 480
        img = fit_to_size(img, width, height)
        be, ce, bo, co = convert_split_colour_cells(img, width, height, cell_height=2)
        for suffix, data in ((".be", be), (".ce", ce), (".bo", bo), (".co", co)):
            with open(args.out_prefix + suffix, "wb") as f:
                f.write(HEADER + data)
        print(f"wrote {args.out_prefix}.be/.ce/.bo/.co ({len(be)}/{len(ce)}/{len(bo)}/{len(co)} bytes)")
    elif args.mode == "itfli":
        # 640x576, cell_height=3, even/odd-row field split (confirmed
        # against Tokra/Kircher's own original i3vdc_quant.c).
        width, height = 640, 576
        img = fit_to_size(img, width, height)
        be, ce, bo, co = convert_split_colour_cells(img, width, height, cell_height=3)
        for suffix, data in ((".be", be), (".ce", ce), (".bo", bo), (".co", co)):
            with open(args.out_prefix + suffix, "wb") as f:
                f.write(HEADER + data)
        print(f"wrote {args.out_prefix}.be/.ce/.bo/.co ({len(be)}/{len(ce)}/{len(bo)}/{len(co)} bytes)")
    elif args.mode == "im800":
        # 800x600 monochrome. Convert the full image once (correct spatial
        # adjacency for error diffusion, matching titlescreen's own
        # technique below), then split the resulting bitmap by row PARITY
        # into even/odd fields -- confirmed against Tokra's original
        # converters/vdcim800.bas line 11: `if y/2=int(y/2) then print#3
        # else print#4` (even rows to .eve, odd rows to .odd), not a
        # physical-half split.
        width, height = 800, 600
        img = fit_to_size(img, width, height, crop_top=args.crop_top)
        bitmap = convert_imono(img, width, height)
        bytes_per_row = width // 8
        even = bytearray()
        odd = bytearray()
        for y in range(height):
            row = bitmap[y * bytes_per_row:(y + 1) * bytes_per_row]
            (even if y % 2 == 0 else odd).extend(row)
        with open(args.out_prefix + ".eve", "wb") as f:
            f.write(HEADER + bytes(even))
        with open(args.out_prefix + ".odd", "wb") as f:
            f.write(HEADER + bytes(odd))
        print(f"wrote {args.out_prefix}.eve ({len(even)} bytes) and .odd ({len(odd)} bytes)")
    elif args.mode == "vscroll":
        # VDC-VSCROLL (src/main.c vscroll_demo()): stored bitmap TALLER
        # than the 640x200 mode it's actually displayed through (see
        # VDC_HIRES_640x200_Mono_VSCROLL's own comment, vdc_core.c) --
        # was named/attempted as a WIDER-than-display horizontal pan
        # first (working title "panorama"), abandoned after the CSIZE/
        # ROWINC mechanism it needed proved unstable; renamed once the
        # effect settled on vertical scrolling instead, since "panorama"
        # no longer fit. Non-interlaced, so unlike imono/im800 above,
        # there's no INTERLACE reason to split fields -- split into
        # top/mid/bottom PHYSICAL thirds instead purely for
        # krill_loadcompd() staging-size reasons (live-diagnosed,
        # 2026-08-18): its in-place decompression writes directly into
        # the destination as it goes, and any single Bank-1 staging call
        # (MEM_SCREEN=$4000) spanning across $b000 runs straight through
        # Oscar64's own C runtime stack ($b000-$be99 in this build's own
        # .map), corrupting live return addresses mid-decompress. Each
        # chunk here stays at or under ~20800 bytes, comfortably clear of
        # that boundary. Pass --crop-top 0 for source images where the
        # composition's dramatic focal point sits at the very top (e.g.
        # Kinryuzan Temple's giant lantern) -- the default centre crop
        # would otherwise eat into it instead of less interesting content
        # further down.
        #
        # height=798 (live-tuned, twice): first raised from an initial
        # 600 to fit both the lantern AND the gate/crowd below it in the
        # same 640:height crop window, then raised again to 798 -- this
        # mode's own near-ceiling (65536 VDC RAM bytes / 80 bytes-per-row
        # = 819 rows max at this width, no charset/attribute overhead
        # since char_std=0; 798 leaves a small safety margin and stays a
        # clean multiple of 3 for the chunk split below). Split into
        # THREE equal thirds, not two halves -- each krill_loadcompd()
        # chunk must stay under the ~28672-byte safe-chunk ceiling
        # documented above, and half of 798 rows' worth would exceed it.
        width, height = 640, 798
        img = fit_to_size(img, width, height, crop_top=args.crop_top)
        bitmap = convert_imono(img, width, height)
        third = len(bitmap) // 3
        top, mid, bot = bitmap[:third], bitmap[third:2 * third], bitmap[2 * third:]
        with open(args.out_prefix + ".top", "wb") as f:
            f.write(HEADER + top)
        with open(args.out_prefix + ".mid", "wb") as f:
            f.write(HEADER + mid)
        with open(args.out_prefix + ".bot", "wb") as f:
            f.write(HEADER + bot)
        print(f"wrote {args.out_prefix}.top/.mid/.bot ({len(top)}/{len(mid)}/{len(bot)} bytes)")
    elif args.mode == "imono":
        width, height = 720, 700
        img = fit_to_size(img, width, height, crop_top=args.crop_top)
        bitmap = convert_imono(img, width, height)
        # Split even/odd by row PARITY (even rows -> even field, odd rows ->
        # odd field), NOT physical half -- confirmed against Tokra's
        # original converters/vdcimono.bas line 11: `if y/2=int(y/2) then
        # print#3 else print#4`. An earlier version of this branch used a
        # physical top-half/bottom-half split instead, reasoning (wrongly)
        # that the ~1980-byte VDC address gap between the two fields
        # (0x0000 vs 0x82c8, reverse-engineered from Tokra's BASIC loader,
        # mono_hires_xl_demo()/src/main.c) meant the source data itself
        # didn't need pre-interleaving -- live-testing on a real photo
        # (2026-07-26) showed this as the picture's physical top half
        # displayed, then replaced by its bottom half, not a combined
        # interlaced image. The address gap is real and unrelated to this;
        # the *content* assigned to each address still has to be
        # deinterlaced first, same as title_screen()'s own working
        # mode below (which has no address gap at all, just this same
        # even/odd split, and was never wrong).
        bytes_per_row = width // 8
        even = bytearray()
        odd = bytearray()
        for y in range(height):
            row = bitmap[y * bytes_per_row:(y + 1) * bytes_per_row]
            (even if y % 2 == 0 else odd).extend(row)
        with open(args.out_prefix + ".eve", "wb") as f:
            f.write(HEADER + bytes(even))
        with open(args.out_prefix + ".odd", "wb") as f:
            f.write(HEADER + bytes(odd))
        print(f"wrote {args.out_prefix}.eve ({len(even)} bytes) and .odd ({len(odd)} bytes)")
    else:
        width, height = 640, 400
        img = fit_to_size(img, width, height)
        bitmap = convert_imono(img, width, height)
        # Split by EVEN/ODD row (interlace field), NOT physical half --
        # same convention as VDC-IMONO/VDC-IM800 above, and confirmed the
        # hard way originally: title_screen() (src/main.c) copies both
        # fields to VDC memory back-to-back with NO address gap (one
        # bnk_cpytovdc() call over the whole concatenated buffer) into a
        # genuinely interlaced mode (VDC_HIRES_640x400_Mono_PAL, LACE=3) --
        # decoding the original, confirmed-working vdce-scrtit.eve/.odd with
        # a naive physical-half split showed the whole picture duplicated
        # top and bottom (each half independently spans the full image,
        # just at half the row density) -- the unambiguous signature of
        # even/odd interlace field data, not a physical-half split. Without
        # an address gap to do the interleaving in hardware (unlike IMONO),
        # the source data has to be pre-interleaved instead.
        bytes_per_row = width // 8
        even = bytearray()
        odd = bytearray()
        for y in range(height):
            row = bitmap[y * bytes_per_row:(y + 1) * bytes_per_row]
            (even if y % 2 == 0 else odd).extend(row)
        with open(args.out_prefix + ".eve", "wb") as f:
            f.write(HEADER + bytes(even))
        with open(args.out_prefix + ".odd", "wb") as f:
            f.write(HEADER + bytes(odd))
        print(f"wrote {args.out_prefix}.eve ({len(even)} bytes) and .odd ({len(odd)} bytes)")


if __name__ == "__main__":
    main()
