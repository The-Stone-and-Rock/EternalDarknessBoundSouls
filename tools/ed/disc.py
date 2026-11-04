"""Read the game's own files: the disc (GameCube FST) and main.dol. Standard library only."""
import mmap
import os
import re
import struct

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
ISO = os.path.join(ROOT, 'dump', 'GEDP01.iso')
DOL = os.path.join(ROOT, 'generated', 'main.dol')
DB = os.path.join(ROOT, 'build', 'ed-db')  # derived from the user's disc: never committed


class Disc:
    def __init__(self, path=ISO):
        self._f = open(path, 'rb')
        self.m = mmap.mmap(self._f.fileno(), 0, access=mmap.ACCESS_READ)
        fst, size = struct.unpack('>II', self.m[0x424:0x42C])
        d = self.m[fst:fst + size]
        n = struct.unpack('>I', d[8:12])[0]
        names = n * 12

        def name(off):
            return d[names + off:d.find(b'\0', names + off)].decode('latin1')

        self.files = {}
        stack = [(n, '')]
        for i in range(1, n):
            while i >= stack[-1][0]:
                stack.pop()
            w, a, b = struct.unpack('>III', d[i * 12:i * 12 + 12])
            p = stack[-1][1] + name(w & 0xFFFFFF)
            if w >> 24:
                stack.append((b, p + '/'))
            else:
                self.files[p] = (a, b)

    def read(self, path):
        a, b = self.files[path]
        return bytes(self.m[a:a + b])

    def glob(self, pattern):
        r = re.compile(pattern)
        return sorted(p for p in self.files if r.search(p))


class Dol:
    def __init__(self, path=DOL):
        self.data = open(path, 'rb').read()
        offs = struct.unpack('>18I', self.data[0:72])
        addrs = struct.unpack('>18I', self.data[72:144])
        sizes = struct.unpack('>18I', self.data[144:216])
        self.sections = [(o, a, s, i) for i, (o, a, s) in enumerate(zip(offs, addrs, sizes)) if s]
        self.text = [(o, a, s) for o, a, s, i in self.sections if i < 7]

    def offset(self, addr):
        for o, a, s, _ in self.sections:
            if a <= addr < a + s:
                return o + addr - a
        return None

    def addr(self, off):
        for o, a, s, _ in self.sections:
            if o <= off < o + s:
                return a + off - o
        return None

    def word(self, addr):
        o = self.offset(addr)
        return None if o is None else struct.unpack('>I', self.data[o:o + 4])[0]

    def in_text(self, addr):
        return any(a <= addr < a + s for o, a, s in self.text)

    def cstr(self, addr, limit=200):
        o = self.offset(addr)
        if o is None:
            return None
        e = self.data.find(b'\0', o, o + limit)
        if e <= o:
            return None
        s = self.data[o:e]
        if not all(32 <= c < 127 for c in s):
            return None
        return s.decode()

    def native_table(self):
        """The game's table of script-callable functions: {name pointer, function pointer} pairs."""
        out = {}
        for o, a, s, i in self.sections:
            if i < 7:
                continue
            for off in range(o, o + s - 7, 4):
                np, fp = struct.unpack('>II', self.data[off:off + 8])
                if not (0x80000000 <= np < 0x80700000 and self.in_text(fp)):
                    continue
                nm = self.cstr(np, 64)
                if nm and re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', nm) and nm not in out:
                    out[nm] = (fp, self.addr(off))
        return out


def symbols():
    """Decomp names (config/GEDE01/symbols.txt, USA addresses) carried to this disc by generated/symbols.tsv.
    Returns {address on this disc: name}."""
    out = {}
    path = os.path.join(ROOT, 'generated', 'symbols.tsv')
    if os.path.exists(path):
        for line in open(path):
            p = line.rstrip('\n').split('\t')
            if len(p) == 3:
                out[int(p[2], 16)] = p[0]
    return out
