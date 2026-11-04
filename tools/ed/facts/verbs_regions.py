#!/usr/bin/env python3
"""Every region of every room, read the way the game reads them: the room file's own header gives the count at +176 and
a list of 116-byte records, and the three shapes' script fields are the ones 0x80134758 reads.
Run from the repo root: python -I tools/ed/facts/verbs_regions.py
Writes build/ed-db/facts/regions.json. Prints the totals, how far rooms.json differs, and a check against every save state.
Nothing here runs the game; saves/ is only read."""
import collections
import glob
import json
import multiprocessing
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'ed'))
import disc
import gametext
import savestate

NAME = re.compile(rb'[A-Za-z0-9_][A-Za-z0-9_ .\-]{0,30}\0')


def s16(b, a):
    return struct.unpack('>h', b[a:a + 2])[0]


def fields(get):
    """get(offset) -> signed short. The fields 0x80134758 reads, by shape (+108)."""
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


def valid(x, at, count):
    """Are `count` records of 116 bytes at `at` all named regions of a known shape, with a clear +104 on disc?"""
    if at < 0 or at + 116 * count > len(x):
        return False
    for i in range(count):
        a = at + 116 * i
        if x[a:a + 4] != b'\0\0\0\0' or not NAME.match(x, a + 4):
            return False
        # A real name field: starts with a letter, three characters or more, zeros after it up to +40, not a marker.
        name = x[a + 4:a + 40].split(b'\0')[0]
        if len(name) < 3 or not name[:1].isalpha() or x[a + 4 + len(name):a + 40].strip(b'\0') or b'Mkr' in name:
            return False
        if s16(x, a + 108) not in (0, 1, 2) or x[a + 104:a + 108] != b'\0\0\0\0':
            return False
    return True


def job(args):
    path, off, size = args
    with open(disc.ISO, 'rb') as f:
        f.seek(off)
        b = f.read(size)
    first, csize = struct.unpack('>I', b[4:8])[0], struct.unpack('>I', b[12:16])[0]
    x = gametext.bpe_expand(b, first, min(first + csize, len(b)))
    count = struct.unpack('>H', x[176:178])[0]
    # The list's pointer at +180 is a stale tool address, so the offset is found: the one place `count` records in a row fit.
    places = []
    if count:
        for m in NAME.finditer(x):
            at = m.start() - 4
            if valid(x, at, count):
                places.append(at)
    out = {'count': count, 'places': len(places), 'regions': []}
    if places:
        at = places[0]
        for i in range(count):
            a = at + 116 * i
            r = fields(lambda o: s16(x, a + o))
            r['name'] = x[a + 4:a + 40].split(b'\0')[0].decode('latin1')
            r['hash'] = struct.unpack('>I', x[a + 44:a + 48])[0]
            out['regions'].append(r)
    return int(re.search(r'rmed(\d+)', path).group(1)), out


def live_check(rooms):
    """Each save's live list against the file's list, name by name and script by script."""
    lines = []
    for path in sorted(glob.glob(os.path.join(ROOT, 'saves', 'slot*.state'))):
        S = savestate.State(path)

        def ls16(a):
            v = S.u16(a)
            return v - 65536 if v >= 32768 else v
        room = S.s32(0x80625420 - 23276)
        r = S.u32(0x8058AFC0 + (S.u32(0x8058AFC0) << 2) + 12)
        count, lst = S.u16(r + 176), S.u32(r + 180)
        file_rows = rooms[str(room)]['regions']
        bad = 0
        for i in range(count):
            a = lst + 116 * i
            live = fields(lambda o: ls16(a + o))
            live['name'] = S.cstr(a + 4)
            f = file_rows[i] if i < len(file_rows) else {}
            for k, v in live.items():
                if f.get(k) != v:
                    bad += 1
        lines.append('%s room %d: %d live regions, %d in the file, %d fields differ' % (os.path.basename(path), room, count, len(file_rows), bad))
    return lines


def main():
    d = disc.Disc()
    jobs = [(p, d.files[p][0], d.files[p][1]) for p in d.glob(r'^Rooms/rmed\d+\.bpe$')]
    with multiprocessing.Pool(max(1, (os.cpu_count() or 2) - 1)) as pool:
        res = dict(pool.imap_unordered(job, jobs))
    rooms = {str(k): v for k, v in sorted(res.items())}
    json.dump(rooms, open(os.path.join(ROOT, 'build', 'ed-db', 'facts', 'regions.json'), 'w'), indent=0)
    t = collections.Counter()
    stays = []
    planes_scripts = collections.Counter()
    for rid, v in rooms.items():
        t['rooms'] += 1
        t['regions'] += v['count']
        if v['count'] and v['places'] != 1:
            t['rooms_not_exactly_one_place'] += 1
            print('room', rid, 'count', v['count'], 'places', v['places'])
        for r in v['regions']:
            t['shape%d' % r['shape']] += 1
            if r['shape'] == 2:
                if r['cross_a'] > 0 or r['cross_b'] > 0:
                    t['plane_with_a_script'] += 1
                for k in ('cross_a', 'cross_b'):
                    if r[k] > 0:
                        planes_scripts[r[k]] += 1
                continue
            for k in ('enter', 'leave', 'stay', 'other_enter', 'other_leave', 'other_stay'):
                if r[k] > 0:
                    t[k] += 1
            if r['stay'] > 0:
                stays.append((rid, r['name'], r['enter'], r['leave'], r['stay']))
    print(dict(t))
    print('stay scripts:', stays)
    print('commonest plane scripts:', planes_scripts.most_common(8))

    # rooms.json against this
    db = json.load(open(os.path.join(ROOT, 'build', 'ed-db', 'rooms.json')))
    missing = collections.Counter()
    wrong = []
    for rid, v in rooms.items():
        have = collections.defaultdict(list)
        for x in db.get(rid, {}).get('regions', []):
            have[x['name']].append(x)
        for r in v['regions']:
            if not have.get(r['name']):
                missing['plane' if r['shape'] == 2 else 'sphere_or_box'] += 1
                continue
            if r['shape'] == 2:
                continue
            x = have[r['name']].pop(0)
            for k in ('enter', 'leave', 'stay', 'other_enter', 'other_leave', 'other_stay'):
                if x.get(k) != r[k]:
                    wrong.append((rid, r['name'], k, r[k], x.get(k)))
    print('rooms.json lacks:', dict(missing))
    print('rooms.json fields that differ from this list:', len(wrong), wrong[:12])
    json.dump({'lacks': dict(missing), 'wrong': wrong, 'leave_hidden': 0, 'totals': dict(t), 'stay': stays},
              open(os.path.join(ROOT, 'build', 'ed-db', 'facts', 'regions_summary.json'), 'w'), indent=0)
    for line in live_check(rooms):
        print(line)


if __name__ == '__main__':
    main()
