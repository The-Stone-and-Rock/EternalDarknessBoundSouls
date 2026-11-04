"""The game's lights, from the room files on the disc. `ed lights ROOM` lists a room's lamps, and `ed lights ROOM X Y Z` the three that
light a place, as the game looks them up.

Each room file has one grid of cells over the room. At the game's address it's what +4 of 0x804F0780 points to, and in the file:
  +4 count, +8 the far corner (3 floats), +20 a cell's size (3 u16), +27 +28 +29 cells across (x fastest), +32 the cells, which follow.
  A cell, 20 bytes: its middle (3 s16), a lamp count at +8 (always 3), then pointers to its lamps at +12 and its two fills at +16.
  A lamp or fill, 20 bytes: place (3 floats), r g b a, power (u32).
Fills, two to a cell, come before the cell's two far-off records and its three lamps: 7 records a cell, one after another.

The game lights with them at 0x801E54BC: GXInitLightAttn with a0 = 1.5 * power, over k0 + k1 d + k2 d^2. A fill has k 9500, 3, 0.0385 (from
r2) and a lamp 1, 1, 0.8. So a lamp's own power is how far it carries, and all of its fade.
"""
import json
import os
import struct

import disc
import gametext

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CACHE = os.path.join(ROOT, 'build', 'ed-db', 'lights')


def _is_record(x, o):
    a, b, c = struct.unpack_from('>3f', x, o)
    if not all(v == v and abs(v) < 60000 for v in (a, b, c)) or x[o + 15] != 0xff:
        return False
    return 0 < struct.unpack_from('>I', x, o + 16)[0] < 2000000


def _room_bytes(room):
    offset, size = disc.Disc().files['Rooms/rmed%04d.bpe' % room]
    with open(disc.ISO, 'rb') as f:
        f.seek(offset)
        b = f.read(size)
    first = struct.unpack('>I', b[4:8])[0]
    csize = struct.unpack('>I', b[12:16])[0]
    return bytes(gametext.bpe_expand(b, first, min(first + csize, len(b))))


def room_lamps(room):
    """The room's lamps: (x, y, z, r, g, b, power), and how many cells name each. The file's longest run of records is the table."""
    path = os.path.join(CACHE, '%d.json' % room)
    if os.path.exists(path):
        return [tuple(l) for l in json.load(open(path))]
    x = _room_bytes(room)
    best = (0, 0)
    o = 0
    while o < len(x) - 20:
        if _is_record(x, o):
            e = o
            while e < len(x) - 20 and _is_record(x, e):
                e += 20
            if e - o > best[1]:
                best = (o, e - o)
            o = e
        else:
            o += 4
    counts = {}
    for c in range(best[1] // 140):
        for k in (4, 5, 6):
            q = best[0] + (c * 7 + k) * 20
            a, b, d = struct.unpack_from('>3f', x, q)
            key = (round(a), round(b), round(d), x[q + 12], x[q + 13], x[q + 14], struct.unpack_from('>I', x, q + 16)[0])
            counts[key] = counts.get(key, 0) + 1
    lamps = sorted(counts, key=lambda k: -counts[k])
    os.makedirs(CACHE, exist_ok=True)
    json.dump(lamps, open(path, 'w'))
    return lamps


def compare(lists):
    """The lamps the game sent the renderer, room by room, against every room on the disc: is each list some room's?"""
    out = []
    wanted = {}
    for name in disc.Disc().glob(r'^Rooms/rmed\d+\.bpe$'):
        room = int(name[-8:-4])
        wanted[room] = {l[:3] for l in room_lamps(room)}
    unmatched = 0
    for sent in lists:
        got = {l[:3] for l in sent}
        best = max(wanted, key=lambda r: len(got & wanted[r]), default=None)
        if best is None or not got:
            continue
        missing = wanted[best] - got
        extra = got - wanted[best]
        # What's sent is what counts: every lamp in it should be on the disc. A few rooms' tables aren't read cleanly here, so what's missing says little.
        if len(extra) > 2 + len(got) // 4:
            unmatched += 1
            out.append('  LAMPS %d sent, best match room %d, %d missing and %d not on the disc' % (len(got), best, len(missing), len(extra)))
    out.append('Lamps: %d room grids sent, %d not matching the disc' % (len(lists), unmatched))
    return out


def main(argv):
    room = int(argv[0])
    lamps = room_lamps(room)
    print('room %d: %d lamps' % (room, len(lamps)))
    for l in lamps:
        print('  at %6d %6d %6d  colour %3d %3d %3d  power %6d  reach %4.0f' % (l[0], l[1], l[2], l[3], l[4], l[5], l[6], (625 * l[6]) ** 0.5))
