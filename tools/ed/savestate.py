"""Read a save state's guest memory (saves/slotN.state), with no game running. EDQSTATE: header, then tagged sections;
MEM1 is {size, page size}, then {page index, page bytes}... ending in 0xFFFFFFFF. See runtime/host/quickstate.c."""
import os
import struct
import sys

import disc

sys.path.insert(0, os.path.join(disc.ROOT, 'tools'))


class State:
    def __init__(self, path):
        f = open(path, 'rb').read()
        if f[:8] != b'EDQSTATE':
            raise ValueError('not a save state: %s' % path)
        count = struct.unpack('<I', f[12:16])[0]
        at = 24
        self.sections = {}
        for _ in range(count):
            size = struct.unpack('<Q', f[at + 8:at + 16])[0]
            self.sections[f[at:at + 4].decode()] = (at + 16, size)
            at += 16 + size
        o, _ = self.sections['MEM1']
        size, page = struct.unpack('<II', f[o:o + 8])
        at = o + 8
        self.mem = bytearray(size)
        while True:
            i = struct.unpack('<I', f[at:at + 4])[0]
            at += 4
            if i == 0xFFFFFFFF:
                break
            n = min(page, size - i * page)
            self.mem[i * page:i * page + n] = f[at:at + n]
            at += n
        self.r13 = r13()

    def bytes(self, addr, n):
        o = addr - 0x80000000
        return bytes(self.mem[o:o + n])

    def u32(self, addr):
        return struct.unpack('>I', self.bytes(addr, 4))[0]

    def s32(self, addr):
        return struct.unpack('>i', self.bytes(addr, 4))[0]

    def u16(self, addr):
        return struct.unpack('>H', self.bytes(addr, 2))[0]

    def f32(self, addr):
        return struct.unpack('>f', self.bytes(addr, 4))[0]

    def sda(self, off):
        """The word at r13 + off (off is usually negative)."""
        return self.u32(self.r13 + off)

    def cstr(self, addr, limit=128):
        b = self.bytes(addr, limit)
        return b.split(b'\0')[0].decode('latin1')


def r13():
    """r13 on this disc, from main.dol's __init_registers: lis r13 and ori/addi r13 at its start."""
    d = disc.Dol()
    w = d.word(d.data and struct.unpack('>I', d.data[0xE0:0xE4])[0])
    entry = struct.unpack('>I', d.data[0xE0:0xE4])[0]
    li = w & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    a = entry + li
    hi = val = None
    for i in range(32):
        x = d.word(a + i * 4)
        op, rt, ra, imm = x >> 26, (x >> 21) & 31, (x >> 16) & 31, x & 0xFFFF
        if rt == 13 and op == 15:
            hi = imm << 16
        elif rt == 13 and op == 24 and hi is not None:
            return hi | imm
        elif rt == 13 and op == 14 and hi is not None:
            return (hi + (imm - 0x10000 if imm & 0x8000 else imm)) & 0xFFFFFFFF
    raise ValueError('r13 not found')
