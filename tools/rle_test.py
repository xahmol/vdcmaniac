#!/usr/bin/env python3
# RLE compression: Phase 0 test-first validation (see
# /home/xahmol/.claude/plans/rle-vdc-blockfill-compression.md's own Phase 0).
# Scratch/throwaway test script, not wired into the real build -- confirms
# the encoding scheme itself is correct before any C-side decoder exists.
#
# Format: PackBits-style, one control byte per unit.
#   control 0x00-0x7F (0-127): literal run, (control+1) literal bytes follow
#                              (1-128 bytes).
#   control 0x80-0xFF (128-255): repeat run, (control-127) copies of the
#                                single byte that follows (1-128 copies).
# Runs longer than 128 bytes are split across multiple consecutive control
# units by the encoder -- the decoder handles this naturally, one unit at a
# time, with no special-casing needed. This also means an original run
# longer than 255 bytes (vdc_block_fill()'s own per-call length cap) decodes
# via multiple chained repeat units, exactly matching how the real C-side
# decoder will need to chain multiple vdc_block_fill() calls for such a run.

MIN_RUN = 7  # matches the measured vdc_block_fill() break-even (not the
             # disk-space-optimal min-run of ~3), so RLE can never make
             # runtime *worse*, only better or neutral.


def rle_encode(data: bytes) -> bytes:
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        # Measure the run length of identical bytes starting at i.
        run_len = 1
        while i + run_len < n and data[i + run_len] == data[i] and run_len < 128:
            run_len += 1

        if run_len >= MIN_RUN:
            out.append(0x80 + (run_len - 1))
            out.append(data[i])
            i += run_len
        else:
            # Accumulate a literal run until we hit a run of MIN_RUN or more,
            # or the 128-byte literal cap.
            lit_start = i
            i += 1
            while i < n:
                # Look ahead for a qualifying repeat run starting here.
                j = i
                look_run = 1
                while j + look_run < n and data[j + look_run] == data[j] and look_run < 128:
                    look_run += 1
                if look_run >= MIN_RUN:
                    break
                if i - lit_start >= 128:
                    break
                i += 1
            lit_len = i - lit_start
            out.append(lit_len - 1)
            out.extend(data[lit_start:i])
    return bytes(out)


def rle_decode(data: bytes) -> bytes:
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        control = data[i]
        i += 1
        if control <= 0x7F:
            count = control + 1
            out.extend(data[i:i + count])
            i += count
        else:
            count = control - 127
            value = data[i]
            i += 1
            out.extend(bytes([value]) * count)
    return bytes(out)


def main():
    import os

    base = os.path.join(os.path.dirname(__file__), "..", "assets")
    buckets = {
        "dithered bitmap (ours)": "vdcfli.bit",
        "colour/attribute (ours)": "vdcfli.col",
        "mono bitmap (ours)": "vdcimono.top",
        "Tokra reference (dithered bitmap)": "vdcfli-orig.bit",
    }

    all_ok = True
    for label, fname in buckets.items():
        path = os.path.join(base, fname)
        with open(path, "rb") as f:
            data = f.read()

        encoded = rle_encode(data)
        decoded = rle_decode(encoded)

        ok = decoded == data
        ratio = len(encoded) / len(data)
        status = "OK" if ok else "MISMATCH"
        print(f"{label:38s} {fname:20s} raw={len(data):6d} rle={len(encoded):6d} ratio={ratio:.3f}  {status}")

        if not ok:
            all_ok = False
            for idx in range(min(len(data), len(decoded))):
                if data[idx] != decoded[idx]:
                    print(f"  first mismatch at offset {idx}: expected {data[idx]:#04x}, got {decoded[idx]:#04x}")
                    break
            else:
                print(f"  length mismatch: expected {len(data)}, got {len(decoded)}")

    print()
    print("ALL ROUND-TRIPS PASSED" if all_ok else "FAILURES FOUND")
    return 0 if all_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
