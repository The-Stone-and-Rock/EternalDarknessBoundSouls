"""Every record of a room file, read the way the game lays them out. The tables were checked field by field against live
memory in the saves: cameras in 12 saves, regions in 10, markers in 12, no differences (docs/game/research).

An earlier version looked for names in the tail by pattern. That lost 989 of the 2,177 cameras, 414 regions and 147 markers.

Header words used: +162 camera count, +164 and +172 the eye and target arrays' build-time addresses (little endian, so their
difference is a size), +176 region count, +212 marker count. The pointers beside the counts are stale tool addresses, so
each array is found as the one place its count of records fits.
"""
import re
import struct

NAME = re.compile(rb'[\x20-\x7e]{2,31}\0')
REGION_NAME = re.compile(rb'[A-Za-z0-9_][A-Za-z0-9_ .\-]{0,30}\0')


def u16(x, o):
    return struct.unpack_from('>H', x, o)[0]


def s16(x, o):
    return struct.unpack_from('>h', x, o)[0]


def u32(x, o):
    return struct.unpack_from('>I', x, o)[0]


def f32(x, o):
    return struct.unpack_from('>f', x, o)[0]


def le32(x, o):
    return struct.unpack_from('<I', x, o)[0]


# Cameras: 120 byte entries, the eyes, each eye's keys, then the targets and their keys.


def _chain(x, s, n):
    """Bytes from s to the end of these n entries' keys, by the sizes in the entries."""
    t = n * 120
    for k in range(n):
        e = s + k * 120
        if e + 120 > len(x):
            return -1
        if u16(x, e + 96):
            t += u32(x, e + 100)
        if u16(x, e + 104):
            t += u32(x, e + 108)
        if u16(x, e + 112):
            t += u32(x, e + 116)
    return t


def _find_eyes(x, count, lim):
    if not count:
        return []
    want = le32(x, 172) - le32(x, 164)
    hits = set()
    for m in NAME.finditer(x, 0, lim):
        e0 = m.start() - 56
        if e0 < 0 or e0 + 120 > len(x):
            continue
        # Each position in the array, on all four byte offsets: a hash ending in a printable byte runs into the name.
        for k in [i * 120 + d for i in range(count) for d in range(0, 4)]:
            s = e0 - k
            if s < 0 or s % 4 or s in hits:
                continue
            if _chain(x, s, count) == want and all(u32(x, s + j * 120 + 52) and NAME.match(x, s + j * 120 + 56) and 0.5 < f32(x, s + j * 120 + 36) < 180 for j in range(count)):
                hits.add(s)
    return sorted(hits)


def _walk_cameras(x, start, n, cur, named):
    rows = []
    for k in range(n):
        e = start + k * 120
        n96, n104, n112 = u16(x, e + 96), u16(x, e + 104), u16(x, e + 112)
        r = {'pos': list(struct.unpack_from('>3h', x, e)), 'axis': [round(f32(x, e + 8 + 4 * i), 4) for i in range(3)], 'angle': round(f32(x, e + 20), 4),
             'far': round(f32(x, e + 32), 1), 'fov': round(f32(x, e + 36), 2), 'path': n104 > 0}
        if named:
            r['name'] = x[e + 56:e + 88].split(b'\0')[0].decode('latin1')
            r['hash'] = u32(x, e + 52)
        if n96:
            cur += u32(x, e + 100)
        if n104:
            r['keys'] = [list(struct.unpack_from('>I3h', x, cur + j * 12)) for j in range(n104)]
            cur += u32(x, e + 108)
        if n112:
            cur += u32(x, e + 116)
        rows.append(r)
    return rows, cur


def cameras(x, sec1):
    """Each camera: its eye's fields and, on a moving one, keys [frame, x, y, z] for the eye and the target. pos is only the
    first key on a moving camera: a moving camera's eye is never just there. 814 of the 2,177 move."""
    count = u16(x, 162)
    if not count:
        return []
    # Only the first section: room 337 has a second place these records fit, past it.
    hits = _find_eyes(x, count, sec1 or len(x))
    if len(hits) != 1:
        raise ValueError('camera array found %d times' % len(hits))
    eyes, cur = _walk_cameras(x, hits[0], count, hits[0] + count * 120, True)
    targets, _ = _walk_cameras(x, cur, count, cur + count * 120, False)
    for e, t in zip(eyes, targets):
        e['target_pos'] = t['pos']
        if 'keys' in t:
            e['target_keys'] = t['keys']
            e['target_path'] = True
    return eyes


# Regions: 116 byte records. +108 is the shape: 0 sphere, 1 box, 2 plane.


def _region_valid(x, at, count):
    if at < 0 or at + 116 * count > len(x):
        return False
    for i in range(count):
        a = at + 116 * i
        if x[a:a + 4] != b'\0\0\0\0' or not REGION_NAME.match(x, a + 4):
            return False
        name = x[a + 4:a + 40].split(b'\0')[0]
        if len(name) < 3 or not name[:1].isalpha() or x[a + 4 + len(name):a + 40].strip(b'\0') or b'Mkr' in name:
            return False
        if s16(x, a + 108) not in (0, 1, 2) or x[a + 104:a + 108] != b'\0\0\0\0':
            return False
    return True


def region_fields(get):
    """get(offset) -> signed short. The fields the game's scan, 0x80134758, reads by shape. Works on a file or on live memory."""
    shape = get(108)
    if shape == 0:
        s = dict(enter=get(58), leave=get(60), stay=get(62), other_enter=get(64), other_leave=get(66), other_stay=get(68))
    elif shape == 1:
        s = dict(enter=get(60), leave=get(62), stay=get(64), other_enter=get(66), other_leave=get(68), other_stay=get(70))
    else:
        s = dict(cross_a=get(100), cross_b=get(102))
    s['shape'] = shape
    s['p110'] = get(110)
    return s


def regions(x):
    count = u16(x, 176)
    if not count:
        return []
    places = [m.start() - 4 for m in REGION_NAME.finditer(x) if _region_valid(x, m.start() - 4, count)]
    if not places:
        raise ValueError('region list not found')
    out = []
    for i in range(count):
        a = places[0] + 116 * i
        r = region_fields(lambda o: s16(x, a + o))
        r['name'] = x[a + 4:a + 40].split(b'\0')[0].decode('latin1')
        r['hash'] = u32(x, a + 44)
        if r['shape'] == 1:
            r['min'] = [s16(x, a + 48 + 2 * k) for k in range(3)]
            r['max'] = [s16(x, a + 54 + 2 * k) for k in range(3)]
        elif r['shape'] == 0:
            # 0x8019C0A0: the radius is an s32 at +48, and the centre three s16 at +52.
            r['radius'] = struct.unpack_from('>i', x, a + 48)[0]
            r['pos'] = [s16(x, a + 52 + 2 * k) for k in range(3)]
        scripts = [r.get(k, 0) for k in ('enter', 'leave', 'cross_a', 'cross_b', 'stay')]
        real = [s for s in scripts if s > 0]
        r['script'] = real[0] if real else None
        out.append(r)
    return out


# Markers: 76 byte records. +0 name, +40 hash, +44 place, +56 axis, +68 angle, +72 flags.


def _good_marker_name(x, a):
    f = x[a:a + 40]
    e = f.find(b'\0')
    return e >= 3 and all(32 <= c < 127 for c in f[:e]) and not f[e:].strip(b'\0') and (65 <= f[0] <= 90 or 97 <= f[0] <= 122)


def markers(x):
    n = u16(x, 212)
    if not n:
        return []
    alpha = set(range(65, 91)) | set(range(97, 123))
    for p in range(0, len(x) - 76 * n, 4):
        if x[p] in alpha and x[p + 76 * (n - 1)] in alpha and all(_good_marker_name(x, p + 76 * i) for i in range(n)):
            out = []
            for i in range(n):
                a = p + 76 * i
                out.append({'name': x[a:a + 40].split(b'\0')[0].decode(), 'hash': u32(x, a + 40), 'pos': [s16(x, a + 44 + 2 * k) for k in range(3)],
                            'axis': [round(f32(x, a + 56 + 4 * k), 4) for k in range(3)], 'angle': round(f32(x, a + 68), 4), 'flags': u32(x, a + 72)})
            return out
    raise ValueError('marker table not found')
