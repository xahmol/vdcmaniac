#!/usr/bin/env python3
"""
VDC picture converter for vdcmodemania-oscar64.

Converts a source picture (any format Pillow can read) into the raw VDC
bitmap(+attribute) byte layout this project's bnk_load()/bnk_cpytovdc()
pipeline expects, for two modes:

  fli   -- 480x252, 8x1 colour cells, non-interlace (VDC_HIRES_480x252_Color_PAL)
  imono -- 720x700, monochrome, interlace (VDC_HIRES_720x700_Mono_PAL)

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

    Colour byte nibble order matches this project's documented VDCR_COLOR
    convention (foreground in bits 4-7, background in bits 0-3 --
    include/vdc_raster.h) for consistency with the rest of the codebase.
    Tokra's own vdc_quant.c packs the opposite way (background high,
    foreground low) -- if VICE shows swapped colours, flip the packing line
    marked below rather than anything else in this function.
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
            colour[y * cells_per_row + (x0 >> 3)] = (best_fore << 4) | best_back  # see docstring note
        err_curr, err_next = err_next, [[0, 0, 0] for _ in range(width + 2)]
        if y % 20 == 0 or y == height - 1:
            print(f"FLI: line {y + 1}/{height}", file=sys.stderr)
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


def fit_to_size(img, width, height):
    """Center-crop img to the target aspect ratio, then resize to exact
    (width, height) -- so any source photo can be dropped in as-is rather
    than requiring it to be pre-cropped to the exact VDC resolution."""
    src_w, src_h = img.size
    target_ratio = width / height
    src_ratio = src_w / src_h
    if src_ratio > target_ratio:
        # Source is wider than target -- crop left/right.
        new_w = round(src_h * target_ratio)
        left = (src_w - new_w) // 2
        img = img.crop((left, 0, left + new_w, src_h))
    elif src_ratio < target_ratio:
        # Source is taller than target -- crop top/bottom.
        new_h = round(src_w / target_ratio)
        top = (src_h - new_h) // 2
        img = img.crop((0, top, src_w, top + new_h))
    return img.resize((width, height), Image.LANCZOS)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--mode", choices=["fli", "imono"], required=True)
    ap.add_argument("--input", required=True, help="Source image (any Pillow-readable format, any size/aspect)")
    ap.add_argument("--out-prefix", required=True, help="Output path prefix; writes <prefix>.bit/.col or <prefix>.bit")
    args = ap.parse_args()

    img = Image.open(args.input)

    if args.mode == "fli":
        width, height = 480, 252
        img = fit_to_size(img, width, height)
        bitmap, colour = convert_fli(img, width, height)
        with open(args.out_prefix + ".bit", "wb") as f:
            f.write(HEADER + bitmap)
        with open(args.out_prefix + ".col", "wb") as f:
            f.write(HEADER + colour)
        print(f"wrote {args.out_prefix}.bit ({len(bitmap)} bytes) and .col ({len(colour)} bytes)")
    else:
        width, height = 720, 700
        img = fit_to_size(img, width, height)
        bitmap = convert_imono(img, width, height)
        # Split top/bottom, same convention as vdce-scrtit.top/.bot: the full
        # 63000-byte bitmap doesn't fit in the 32KB CPU RAM staging area
        # (MEM_SCREEN..MEM_CHARSET) used to load a picture into VDC memory in
        # one piece, so it's loaded (and generated) in two halves instead.
        bytes_per_row = width // 8
        half_row = height // 2
        split = bytes_per_row * half_row
        top, bot = bitmap[:split], bitmap[split:]
        with open(args.out_prefix + ".top", "wb") as f:
            f.write(HEADER + top)
        with open(args.out_prefix + ".bot", "wb") as f:
            f.write(HEADER + bot)
        print(f"wrote {args.out_prefix}.top ({len(top)} bytes) and .bot ({len(bot)} bytes)")


if __name__ == "__main__":
    main()
