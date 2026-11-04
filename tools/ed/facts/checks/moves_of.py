"""Which moves a character's file has, by part: python -I tools/ed/facts/checks/moves_of.py CHARACTER [CHARACTER...]
A file's first 1 KB lists its parts (+16 how many, +20 where: 16 bytes each, +0 offset, +4 size, +8 the part's id). A part
holds its moves at its own +24 (how many at +20), 296 bytes each, the move's number at +6 (0x8012484C reads them)."""
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
import disc

d = disc.Disc()
u32 = lambda b, o: struct.unpack_from('>I', b, o)[0]
u16 = lambda b, o: struct.unpack_from('>H', b, o)[0]
for m in sys.argv[1:]:
    b = d.read('Chars/char%04d.cpk' % int(m))
    n, where = u32(b, 16), u32(b, 20)
    print('char %s: %d bytes, %d parts' % (m, len(b), n))
    if len(b) < 64 or n > 64:
        print('  no parts: it stands in for another file')
        continue
    for k in range(n):
        at, pid = u32(b, where + k * 16), u16(b, where + k * 16 + 8)
        c, lst = u16(b, at + 20), u32(b, at + 24)
        print('  part %-3d %3d moves: %s' % (pid, c, ' '.join(str(x) for x in sorted(u16(b, at + lst + i * 296 + 6) for i in range(c)))))
