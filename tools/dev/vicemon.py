#!/usr/bin/env python3
import socket, struct, sys, time

STX = 0x02
API = 0x02

class ViceMon:
    def __init__(self, host="127.0.0.1", port=6502):
        self.sock = socket.create_connection((host, port), timeout=10)
        self.sock.settimeout(5)
        self.reqid = 1
        self.buf = b""

    def _read_exact(self, n):
        while len(self.buf) < n:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise EOFError("connection closed")
            self.buf += chunk
        data = self.buf[:n]
        self.buf = self.buf[n:]
        return data

    def read_response(self):
        # find STX
        hdr = self._read_exact(11)
        assert hdr[0] == STX, f"bad stx {hdr!r}"
        api = hdr[1]
        length = struct.unpack("<I", hdr[2:6])[0]
        resp_type = hdr[6]
        err = hdr[7]
        reqid = struct.unpack("<I", hdr[8:12-1])[0] if False else struct.unpack("<I", hdr[7:11])[0]
        # NOTE: header is byte0 STX,1 api,2-5 length,6 resp_type,7 err,8-11 reqid = 12 bytes total not 11
        raise RuntimeError("shouldn't reach")

    def recv_response(self):
        hdr = self._read_exact(12)
        assert hdr[0] == STX, f"bad stx {hdr!r}"
        length = struct.unpack("<I", hdr[2:6])[0]
        resp_type = hdr[6]
        err = hdr[7]
        reqid = struct.unpack("<I", hdr[8:12])[0]
        body = self._read_exact(length)
        return {"type": resp_type, "err": err, "reqid": reqid, "body": body}

    def send_cmd(self, cmd, body=b""):
        rid = self.reqid
        self.reqid += 1
        header = bytes([STX, API]) + struct.pack("<I", len(body)) + struct.pack("<I", rid) + bytes([cmd])
        self.sock.sendall(header + body)
        return rid

    def wait_for(self, rid, max_events=50):
        """Read responses until we get the one matching rid (collecting async ones)."""
        events = []
        for _ in range(max_events):
            r = self.recv_response()
            if r["reqid"] == rid:
                return r, events
            else:
                events.append(r)
        raise TimeoutError("did not get matching response")

    def ping(self):
        rid = self.send_cmd(0x81)
        r, ev = self.wait_for(rid)
        return r, ev

    def checkpoint_set(self, start, end, stop=True, enabled=True, op=0x04, temp=False):
        body = struct.pack("<HHBBBB", start, end, 1 if stop else 0, 1 if enabled else 0, op, 1 if temp else 0)
        rid = self.send_cmd(0x12, body)
        r, ev = self.wait_for(rid)
        return r, ev

    def checkpoint_delete(self, num):
        body = struct.pack("<I", num)
        rid = self.send_cmd(0x13, body)
        r, ev = self.wait_for(rid)
        return r, ev

    def exit_monitor(self):
        rid = self.send_cmd(0xaa)
        r, ev = self.wait_for(rid)
        return r, ev

    def registers_get(self, memspace=0):
        rid = self.send_cmd(0x31, bytes([memspace]))
        r, ev = self.wait_for(rid)
        return r, ev

    def mem_get(self, start, end, memspace=0, side_effects=0, bank=0):
        # NOTE: (start, end) is INCLUSIVE on both ends per VICE's own binary
        # monitor protocol -- requesting end=start+N returns N+1 bytes, not
        # N. The response body also has its own 2-byte little-endian
        # "length of data that follows" prefix, which is NOT part of the
        # memory content -- callers must skip body[0:2]. Both of these bit
        # a manual chunked-read helper written directly against this method
        # during vdcmaniac's photo-pipeline verification (2026-07-26):
        # naively concatenating multiple mem_get() calls' raw bodies without
        # accounting for either produced a spurious few-byte artifact at
        # every chunk boundary, which briefly looked exactly like a genuine
        # picture-data corruption bug before being traced back here. Use
        # read_mem_bytes() below for a chunked read that already handles
        # both of these correctly, rather than reimplementing chunking
        # against this raw method again.
        body = struct.pack("<BHHBH", side_effects, start, end, memspace, bank)
        rid = self.send_cmd(0x01, body)
        r, ev = self.wait_for(rid)
        return r, ev

    def read_mem_bytes(self, start, n, memspace=0, side_effects=0, bank=0, chunk_size=2000):
        """Read exactly n contiguous bytes starting at `start`, chunked to
        avoid whatever response-size limit the binary monitor enforces.
        Correctly strips the 2-byte length prefix and accounts for the
        inclusive (start,end) range -- see mem_get()'s own comment for why
        naive chunking here is a trap."""
        out = bytearray()
        off = start
        remaining = n
        while remaining > 0:
            c = min(chunk_size, remaining)
            r, ev = self.mem_get(off, off + c - 1, memspace=memspace, side_effects=side_effects, bank=bank)
            data = r["body"][2:2 + c]
            out.extend(data)
            off += c
            remaining -= c
        return bytes(out)

    def display_get(self, use_vic_ii=0, image_format=0):
        """MON_CMD_DISPLAY_GET (0x84) -- captures the emulator's current
        rendered frame as an indexed-8bpp image (image_format=0). Returns
        (width, height, raw_index_bytes) -- raw_index_bytes is width*height
        palette-index bytes, row-major. For a genuinely interlaced VDC mode
        (LACE=3), the returned height reflects a single field (half the
        mode's full vertical resolution), not the combined frame -- observed
        directly this way for VDC-IHFLI (640x480 mode captured at 240 lines)
        during vdcmaniac's interlace-alignment investigation (2026-07-26).
        Response body layout: [info_len:4][info: dbg_w,dbg_h,off_x,off_y,
        inner_w,inner_h,bpp (2 bytes each except bpp=1, 13 bytes total)]
        [inner_w*inner_h index bytes] -- no separate length prefix on the
        image data itself, unlike mem_get()'s response."""
        rid = self.send_cmd(0x84, bytes([use_vic_ii, image_format]))
        r, ev = self.wait_for(rid)
        data = r["body"]
        info_len = struct.unpack("<I", data[0:4])[0]
        info = data[4:4 + info_len]
        w = int.from_bytes(info[8:10], "little")
        h = int.from_bytes(info[10:12], "little")
        img = data[4 + info_len:4 + info_len + w * h]
        return w, h, img

    def registers_available(self, memspace=0):
        rid = self.send_cmd(0x83, bytes([memspace]))
        r, ev = self.wait_for(rid)
        return r, ev

    def quit_vice(self):
        rid = self.send_cmd(0xbb)
        r, ev = self.wait_for(rid)
        return r, ev

    def reset(self, mode=0):
        rid = self.send_cmd(0xcc, bytes([mode]))
        r, ev = self.wait_for(rid)
        return r, ev


def parse_registers(body):
    # body: 2 bytes count, then array of items: 1 byte item-len, 1 byte reg id, 2 bytes value (item-len usually 3)
    count = struct.unpack("<H", body[0:2])[0]
    pos = 2
    regs = {}
    for _ in range(count):
        item_len = body[pos]
        reg_id = body[pos+1]
        val_bytes = body[pos+2:pos+1+item_len]
        val = int.from_bytes(val_bytes, "little")
        regs[reg_id] = val
        pos += 1 + item_len
    return regs


if __name__ == "__main__":
    m = ViceMon()
    r, ev = m.ping()
    print("ping resp:", r, "events:", ev)
