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
        body = struct.pack("<BHHBH", side_effects, start, end, memspace, bank)
        rid = self.send_cmd(0x01, body)
        r, ev = self.wait_for(rid)
        return r, ev

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
