"""Writes runtime/host/sanity_doors.inc: for Mods' Sanity Effects, the doors out of each room, from the kit's own door table, and
each room's effect list from its file (+128, the count a door reads at 0x80153A00). Run again after `ed build`:

    python -I tools/ed/facts/sanity_doors.py

A door that tests a flag comes last for its room, as it may be locked. See docs/game/systems/sanity.md."""
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import disc  # noqa: E402
import gametext  # noqa: E402
import roomfile  # noqa: E402

OUT = os.path.join(disc.ROOT, 'runtime', 'host', 'sanity_doors.inc')


def room_list(d, path):
    """The room's effect list: count at +128, the list 0x78 bytes and its own padded length before the cameras' eyes. Checked
    against live memory in 12 rooms. -1 is an empty place in it (65535 in the file)."""
    b = d.read(path)
    first = struct.unpack('>I', b[4:8])[0]
    csize = struct.unpack('>I', b[12:16])[0]
    sec = struct.unpack('>I', b[16:20])[0]
    x = bytes(gametext.bpe_expand(b, first, min(first + csize, len(b))))
    n = struct.unpack('>H', x[128:130])[0]
    if not n:
        return []
    cameras = struct.unpack('>H', x[162:164])[0]
    hits = roomfile._find_eyes(x, cameras, sec or len(x))
    if len(hits) != 1:
        raise ValueError('%s: %d places for its cameras' % (path, len(hits)))
    at = hits[0] - 0x78 - ((2 * n + 3) & ~3)
    return list(struct.unpack('>%dH' % n, x[at:at + 2 * n]))


def main():
    d = disc.Disc()
    lists = {}
    for path in d.glob(r'^Rooms/rmed\d+\.bpe$'):
        lists[int(re.search(r'rmed(\d+)', path).group(1))] = room_list(d, path)
    has = {k: bool(v) for k, v in lists.items()}
    table = json.load(open(os.path.join(disc.DB, 'facts', 'doors_table.json'), encoding='utf8'))
    rows = set()
    for r in table:
        if r.get('source') == 'none_found' and r['level'].isdigit() and r.get('entry_in_to_room'):
            # No region known to run it, so -1: a door into this room is allowed, but can't be picked to drive.
            rows.add((int(r['level']), -1, 1, r['to_room'], r['script'], 0))
            continue
        if r.get('source') != 'region' or not r.get('entry_in_to_room') or r.get('flags'):
            continue
        if not r['level'].isdigit():
            continue
        script = os.path.join(disc.DB, 'scripts', 'L' + r['level'], '%d.lua' % r['script'])
        if not os.path.exists(script):
            continue
        text = open(script, encoding='latin1').read()
        gated = 1 if re.search(r'\bfn30\(|\bed60\(', text) else 0
        for f in r['from']:
            if isinstance(f.get('room'), int) and f.get('room') != r['to_room']:
                rows.add((int(r['level']), f['room'], gated, r['to_room'], r['script'], len(r.get('asks_in_script') or [])))
    with open(OUT, 'w', encoding='utf8', newline='\n') as f:
        f.write('// Written by tools/ed/facts/sanity_doors.py, from the kit\'s door table. Don\'t edit by hand.\n')
        f.write('// {level, from room, tests a flag, to room, its script, words it asks}\n')
        f.write('static const SanityDoor kSanityDoors[] = {\n')
        for row in sorted(rows):
            f.write('    {%d, %d, %d, %d, %d, %d},\n' % row)
        f.write('};\n\n// Rooms with an effect list in their file.\nstatic const u16 kSanityListRooms[] = {\n')
        listed = sorted(k for k, v in has.items() if v)
        for i in range(0, len(listed), 16):
            f.write('    ' + ', '.join(str(x) for x in listed[i:i + 16]) + ',\n')
        f.write('};\n\n// {room, an effect its list names}\nstatic const SanityNamed kSanityRoomNames[] = {\n')
        for room in sorted(lists):
            for effect in sorted(set(lists[room])):
                if effect != 65535:
                    f.write('    {%d, %d},\n' % (room, effect))
        f.write('};\n')
    print('%d doors, %d of %d rooms have a list: %s' % (len(rows), len(listed), len(has), OUT))
    near = [r for r in sorted(rows) if r[1] in (166, 138, 216) and r[0] == 2]
    print('from slot 2:', [(r[1], r[3], 'gated' if r[2] else 'open', r[4], 'list' if has.get(r[3]) else 'no list') for r in near])


if __name__ == '__main__':
    main()
