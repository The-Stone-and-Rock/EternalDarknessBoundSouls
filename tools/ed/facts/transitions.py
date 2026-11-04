#!/usr/bin/env python3
"""Every count in transitions.md, and transitions.json. Run from the repo root: python -I tools/ed/facts/transitions.py
Reads only: build/ed-db (scripts, rooms.json, disasm.txt), build/ed-db/facts/regions.json (every region, from
verbs_regions.py), the disc, and saves/slot*.state and saves/coop-*.state (read, never written).
Writes only: build/ed-db/facts/transitions.json and prints the same numbers.
Markers are read from each room's own table (count at +212, 76-byte records), not from rooms.json, which is short of
markers in 32 rooms. Regions come from regions.json, not rooms.json, which lacks 414 of them."""
import collections
import glob
import json
import multiprocessing
import os
import re
import struct
import sys

sys.path.insert(0, 'tools/ed')
import disc  # noqa: E402
import gametext  # noqa: E402
import savestate  # noqa: E402

DB = 'build/ed-db'
ROOT = os.path.join(DB, 'scripts')
CALL = re.compile(r'\bfn66\(([^()]*)\)')
PROMPT = re.compile(r'\bed301\((-?\d+), (-?\d+)\)')
JUMP_TABLE = 0x80253F90  # 0x801667B0: lis r3,-32731; 0x801667B8: addi r3,r3,16272
PROMPTS = {0: 'INVALID_BUTTON', 1: 'OPEN_DOOR', 2: 'TELEPORT', 3: 'CLIMB_UP', 4: 'CLIMB_DOWN', 5: 'CLIMB_OVER', 6: 'EXAMINE', 7: 'RETURN',
           10: 'ENTER_SECRET_PASSAGE', 11: 'PULL_LEVER', 16: 'OPEN_FUSEBOX', 17: 'TRIP_BREAKER', 18: 'OPEN_DRAWER', 19: 'PICK_UP', 22: 'TALK',
           27: 'USE_STAFF', 33: 'LOOK_AROUND', 34: 'TEXT', 35: 'ADJUST_MIRROR', 36: 'ANSWER_PHONE', 38: 'SEARCH'}
REGION_FIELDS = ('enter', 'leave', 'stay', 'other_enter', 'other_leave', 'other_stay', 'cross_a', 'cross_b')
CAM_LINE = re.compile(r'^fn8\((\d+), (-?\d+), (-?\d+), (-?\d+), (-?\d+)\)$')
CAM_ONLY_LINE = re.compile(r'^fn(2|3|4|8)\(')

MOTIONS = {2: 'looped (flags 5) with a heading. What it looks like is not established',
           25: 'door motion: inferred only from 271/271 OPEN_DOOR. If the model has no 25: 99',
           103: 'climb-up motion: inferred only from 43/43 CLIMB_UP',
           104: 'climb-down motion: inferred only from 45/48 CLIMB_DOWN'}
USAGE = {0: '23 of 27 distinct calls have no ed301 prompt before them', 1: '2 calls after EXAMINE; L04/143 runs from ElevatorRgn01, and both from SKPlane regions',
         2: '205 of 253 distinct calls have no prompt before them; 42 follow OPEN_DOOR', 3: '53 of 63 distinct calls have no prompt before them',
         6: '2 calls, both JumpOverRgn scripts', 9: '271 of 271 distinct calls follow OPEN_DOOR', 12: '43 of 43 follow CLIMB_UP', 15: '45 of 48 follow CLIMB_DOWN (2 TEXT, 1 EXAMINE)'}
ARGUMENTS = {
    '1 room': 'ticket +8',
    '2 entry marker hash': 'ticket +28. Found by 0x80155348(hash, 2): +40 of the 76-byte marker records. Placed there and turned to its heading',
    '3 camera hash': 'ticket +32. 0x801EE998(hash, 2, 0) gives the index, 0x801EE764 sets it on the record at 0x80603288',
    '4 transition': 'ticket +20. See transitions',
    '5 script': 'ticket +12. Run by 0x801642D4(script, 0, 0) at the end of 0x80152614, after placing and the camera. 0 or below: none',
    '6 flags': "ticket +16. Bit tests. Bit 1 (0x80152680): arrive at saved place slot 0 (0x8016490C(0), set by fn104(0, object)) not the marker; this path skips 0x80151914, so the heading word 16(r1) is never written. Bit 2 (0x80152670): 0x80043BE8(0): r13-26276 goes to 0, and r13-26272 too when 0x801F62B8() equals 0x801F6258(); they stay 0 until something writes them (ed293 does); 1 in all 12 saves; while they forbid it, 0x80007460 skips 0x80045BC4 and 0x801F5D48. Also passed to 0x80152298 as its flags",
    '7 optional, default 1': 'ticket +24. Not 0: 0x80154C94 writes the new room to the player object +592 (0x80118CB4), calls 0x80043E78, and calls 0x80043F14 for that one node. 0: none of the three. All also need r13-26280 = 0',
    'argument count': '0x801666C8: 6 or 7 are read. Fewer than 6 or more than 7 reads none and uses whatever r21-r28 hold. Every script call passes 6 or 7',
    'return': '0x800440D0: 0 means refused (0x8006E2F0 true) and nothing happens; no script reads it',
}
ABOVE_16 = 'cmplwi r25,16 then bgt at 0x801667A8, so above 16 and negative values take the default: no motion, no fade (r22 = -1), countdown 1. Ticket +20 keeps the number, and the tick treats only 17 differently: 0x80152298(room, arg 6) with no placement. No script passes any of these to fn66. Game code passes 17 at five sites with countdown 1 and delay 1, so the tick calls 0x801E7C88(black, 0) first (0x8015587C)'
ARRIVAL = ['0x80155830 counts ticket +0 down once a tick; at 0 it calls 0x80152614(room, entry, camera, script, flags), or 0x80152298 for transition 17',
           '0x80152298 loads the room. Its 0x80151FDC clears busy (0x801556CC(-1, 0)), holds the player again (0x801F6828(player, 1)) and calls 0x801E7C88(transparent from r2-16856, 10) at 0x801521A8, whatever the transition. A later 0x801E7C88 overwrites that: fn112 reaches it at 0x8016BD30, and script 868, run as the 5th argument, does so on its 4th line',
           '0x80152658 is the instruction after that load returns, not a function: nothing branches to it. If r13-26280 is 0: place the player',
           "Event 238 to the player returns the motion to finish. First player data (*(node +40)) +124: if not -1, stop the motion (0x80124788), play 0x80118FB4's motion with flags 0x10031, return +124 with 0 turned into -1, and reset +124 to -1. Native ed196 writes +124. Otherwise by the motion current at that moment (0x801222F0): 25->25, 99->99, 103->104, 104->103, others -1 (0x8005A8DC)",
           'If not -1: heading is marker heading + pi, event 191 plays that motion with flags 0x10002 (bit 0x2 sets r26 = -1 at 0x8012326C), state 80 (state 1 if the play returns 0). Event 6 there: motion 2 at +128 + current heading, state 81. Event 6 there: state 1. Event 7 in 80 and 81 is gated by 0x80032AD0',
           'If -1: heading is the marker heading, no motion. With flag bit 1 the heading is the unwritten stack word 16(r1) in both cases',
           '0x80043380 calls 0x8006C008 on a kind 32 object, for what 0x8004305C found at player data +140 +68 (not traced), then event 62, camera, then the 5th argument script']
OTHER_ROUTES = {
    'ed29()': "0x800E8C9C: ticket, transition 17, to the room kept in +92 of the node's data, callback 0x800E8B48. Scripts: one id, 1023, in 15 levels. Game code also calls 0x800E8C9C at 0x800692CC when the room is 83",
    '0x800E8E14': 'To room 83, transition 16, countdown 190, delay 128, entry EntryMkr01, camera IntroCam01 or Camera01. In the kind 39 handler 0x800E860C',
    '0x8006BFAC': 'Ticket 0x8006BFF4, transition 17, callback 0x8006B45C. Room is node data +72. Purpose not traced',
    '0x8009B760': 'Ticket 0x8009B808, transition 0, room = object +76 + 1000, countdown 3, delay 2. The only site found that builds a room 1000 or above. Who runs it is not traced',
    '0x800A2548': "Ticket 0x800A281C, transition 17, flags 4, callback 0x800A24E8, only if a linked object's room is not the current room. Called at 0x800A3268, 0x800A32A8, 0x800A3B48 (kind 50's handler 0x800A2B94; 0x800A32A8 on event 135), 0x800A475C (kind 82's 0x800A3F44), 0x80065FF0 (in 0x80065B10, not traced)",
    '0x800ADA24': 'Ticket 0x800ADAB8, transition 17, flags 68, callback 0x800AD8A0, from 0x800B7208',
    '0x800AD6F4': 'Direct 0x80152298(room, 68), from 0x800B7208',
    '0x800C04EC': 'Ticket 0x800C0594, transition 0, r8 = 0 (the writer makes that 1), to room 252, script 2779, when the room is 255. Called at 0x8005876C',
    '0x8012D1A0': 'Ticket 0x8012D2EC, transition 17, flags 32, room = record +120, callback 0x80152488, entry -2 (first marker with flag 0x10). Called at 0x8012ABF0 in 0x8012A8A4, which 0x80050A00 calls',
    '0x80152488': 'The arrival for callback use: as 0x80152614 but with the script started later by 0x80163C6C(script, -1, 1)',
    '0x801525D4': 'Immediate 0x80152298 + 0x80152488, from 0x80050174 only: room 96 or a room in r3, flags 128',
    '0x800ADDCC': 'Direct 0x80152614, entry and camera from strings, from 0x80022248 and 0x800AE780',
    '0x800ADADC, 0x800AE61C, 0x8004FC3C, 0x80020328': 'Direct 0x80152298 callers, purposes not traced',
    '0x80110DA8': 'Direct 0x80152298(current room, 76): the Tome giving the room back (comment at tw_keep in hle.c)',
    '0x80157458': 'Direct 0x80152298(current room, 68), from 0x80025FF0',
}
NOT_ROOM_CHANGES = {
    'fn41': 'Sets bits on named records: hash of a name at +2 of 272-byte room records (RI_FIRE01.IFL in slot3), through 0x801661FC. primitives.tsv calls it go_room_a: wrong. "Texture list" rests on those .IFL names',
    'ed85': '0x80145760: calls 0x801413E8 and sound 536 at 100 (0x8019DB90). Not a room change. 0x801413E8 is not traced, so "effect object" is a reading',
    'fn71': '0x80155600: stores the room in table 0x8058AF60 and calls 0x80154EC4(0). primitives.tsv calls it music: wrong. 703 of 1056 literal fn66 calls have fn71 of the same room in the script. "Preload" is a reading of that table',
    'fn104': "Writes an object's place as 3 shorts to slot N at 0x805CC2BC and returns it. Every script call names slot 0. primitives.tsv calls it sound_at: wrong",
    'ed196': 'Not in primitives.tsv. 0x80017018 writes its argument to player data +124, read first by event 238',
    'TeleportRegion (room 83)': 'Script 381 has no fn66',
    'coop_position_copy': 'Hook on 0x80118274 for the detail share. Nothing to do with doors, though doors.md lists it',
    '0x801EC1C8': "After 0x801642D4 ran a camera record's +68 script. Not a door arrival",
    '0x80134B38': "Return address of a zone's script run. It compares the room before and after and stops scanning if it changed",
}
NOT_FOUND = ['What motions 25, 103 and 104 look like on screen: not watched. Stairs or ladder: not determined',
             'Seconds per tick: the tick is once per game loop, the frame rate was not measured',
             'Whether the motion update 0x80123698 runs exactly once per tick for the player: 0x800271B4 is registered by address and its caller was not traced',
             'What each motion event does',
             'What sound doors make: 24 of 271 door scripts call fn47, motion 25 has no events. Not traced further',
             'What flag 0x100 on a motion does beyond 0x80123F38 and 0x80124448',
             'Whether the first motion track is the root and its third short the height',
             'Whether a script can cause a room change other than through fn66 and ed29: fn79 event 135 is not ruled out']


def jump_table():
    """Each transition's branch in fn66, decoded from the DOL's jump table and the disassembly."""
    d = disc.Dol()
    lines = {}
    order = []
    for l in open(os.path.join(DB, 'disasm.txt')):
        m = re.match(r'([0-9A-F]{8}): (\S+)\s*(.*)', l)
        if m and 0x801667C0 <= int(m.group(1), 16) <= 0x801668B4:
            a = int(m.group(1), 16)
            lines[a] = (m.group(2), m.group(3))
            order.append(a)
    out = {}
    for n in range(17):
        at = d.word(JUMP_TABLE + 4 * n)
        motion, fade, delay = None, -1, 0
        a = at
        while True:
            op, arg = lines[a]
            if op == 'li' and arg.startswith('r3,'):
                motion = int(arg.split(',')[1])
            if op == 'li' and arg.startswith('r22,'):
                fade = int(arg.split(',')[1])
            if op == 'li' and arg.startswith('r21,'):
                delay = int(arg.split(',')[1])
            if op == 'b' or a + 4 >= 0x801668B0:
                break
            a += 4
        out[n] = {'branch': '0x%08X' % at, 'motion': motion, 'fade_ticks': fade if fade != -1 else None, 'fade_delay': delay,
                  'countdown': 1 if fade == -1 else fade + 2}
    return out


def scripts():
    for lv in sorted(os.listdir(ROOT)):
        for f in sorted(os.listdir(os.path.join(ROOT, lv))):
            if f.endswith('.lua'):
                yield lv[1:], int(f[:-4]), open(os.path.join(ROOT, lv, f), encoding='utf8').read()


def region_map():
    """Script id -> [(room, region name)], every script field of every region, from the room files (verbs_regions.py)."""
    out = collections.defaultdict(list)
    for rid, v in json.load(open('build/ed-db/facts/regions.json')).items():
        for r in v['regions']:
            for key in REGION_FIELDS:
                if r.get(key, -1) > 0 and (int(rid), r['name']) not in out[r[key]]:
                    out[r[key]].append((int(rid), r['name']))
    return out


def all_calls(rooms, tables, regions):
    names = {}
    for r in rooms.values():
        for m in r['cameras']:
            names[m['hash']] = m['name']
    for recs in tables.values():
        for name, h, _ in recs:
            names[h] = name
    rows = []
    bodies = {}
    for lv, sid, body in scripts():
        bodies[(lv, sid)] = body
        for m in CALL.finditer(body):
            a = [x.strip() for x in m.group(1).split(',')]
            before = PROMPT.findall(body[:m.start()])
            tail = body[:m.start()].rstrip().split('\n')[-1]
            rows.append({'level': lv, 'script': sid, 'args': a,
                         'prompt_before': int(before[-1][1]) if before else None,
                         'regions': regions.get(sid, []),
                         'entry': names.get(int(a[1])) if a[1].isdigit() else a[1],
                         'camera': names.get(int(a[2])) if a[2].isdigit() else a[2],
                         'ed85_just_before': tail.startswith('ed85('),
                         'preload_same_room': bool(re.search(r'\bfn71\(%s\)' % re.escape(a[0]), body)),
                         'sounds': re.findall(r'\bfn47\((\d+)', body)})
    return rows, bodies, names


def _good_name(x, a):
    f = x[a:a + 40]
    e = f.find(b'\0')
    return e >= 3 and all(32 <= c < 127 for c in f[:e]) and not f[e:].strip(b'\0') and (65 <= f[0] <= 90 or 97 <= f[0] <= 122)


def _room_markers(args):
    """One room's marker table: count at +212. The pointer beside it is a stale tool address, so the offset is found as
    the one place `count` names sit 76 bytes apart. Checked against live memory in live_marker_check."""
    path, off, size = args
    with open(disc.ISO, 'rb') as f:
        f.seek(off)
        b = f.read(size)
    first, csize = struct.unpack('>I', b[4:8])[0], struct.unpack('>I', b[12:16])[0]
    x = bytes(gametext.bpe_expand(b, first, min(first + csize, len(b))))
    n = struct.unpack('>H', x[212:214])[0]
    rid = int(re.search(r'rmed(\d+)', path).group(1))
    if not n:
        return rid, 1, []
    places = []
    alpha = set(range(65, 91)) | set(range(97, 123))
    for p in range(0, len(x) - 76 * n, 4):
        if x[p] in alpha and x[p + 76 * (n - 1)] in alpha and all(_good_name(x, p + 76 * i) for i in range(n)):
            places.append(p)
    recs = []
    if places:
        q = places[0]
        for i in range(n):
            a = q + 76 * i
            recs.append((x[a:a + 40].split(b'\0')[0].decode(), struct.unpack_from('>I', x, a + 40)[0], struct.unpack_from('>I', x, a + 72)[0]))
    return rid, len(places), recs


def marker_tables():
    d = disc.Disc()
    jobs = [(p, d.files[p][0], d.files[p][1]) for p in d.glob(r'^Rooms/rmed\d+\.bpe$')]
    with multiprocessing.Pool(max(1, (os.cpu_count() or 2) - 1)) as pool:
        res = pool.map(_room_markers, jobs)
    bad = [(r, c) for r, c, _ in res if c != 1]
    print('rooms read', len(res), 'without exactly one marker table:', bad)
    return {r: recs for r, _, recs in res}


def saves():
    for path in sorted(glob.glob('saves/*.state')):
        name = os.path.basename(path)[:-6]
        if name.startswith(('slot', 'coop-')):
            yield name, savestate.State(path)


def live_marker_check(tables):
    out = {}
    for name, s in saves():
        cur = s.s32(s.r13 - 23276)
        room = s.u32(0x8058AFC0 + (s.u32(0x8058AFC0) << 2) + 12)
        n, rec = s.u16(room + 160 + 52), s.u32(room + 160 + 56)
        v = tables[cur]
        bad = sum(1 for i in range(n) if i >= len(v) or s.cstr(rec + 76 * i, 40) != v[i][0] or s.u32(rec + 76 * i + 40) != v[i][1] or s.u32(rec + 76 * i + 72) != v[i][2])
        out[name] = {'room': cur, 'live_records': n, 'disc_records': len(v), 'differ': bad}
    return out


def ticket_sites():
    """Every call of the ticket writer. Walking back from the call, the first write of each register decides: a plain li
    gives its value, anything else is None. A call in between clobbers what is left (r3 to r10 are not kept across it)."""
    lines = [l.rstrip() for l in open(os.path.join(DB, 'disasm.txt'), errors='replace')]
    want = ('r3', 'r6', 'r7', 'r8', 'r9', 'r10')
    out = {}
    for i, l in enumerate(lines):
        if re.match(r'[0-9A-F]{8}: bl\s+0x80155770$', l):
            vals = {}
            for k in range(i - 1, max(0, i - 40), -1):
                m = re.match(r'[0-9A-F]{8}: (\S+)\s*(.*)$', lines[k])
                op, arg = m.group(1), m.group(2)
                if op == 'bl' or op == 'stwu':
                    break
                if op.startswith(('stw', 'stb', 'sth', 'stf', 'cmp', 'bc', 'b')) and op != 'bl':
                    continue
                dest = arg.split(',')[0].strip()
                if dest in want and dest not in vals:
                    vals[dest] = int(arg.split(',')[1]) if op == 'li' else None
            out['0x' + l[:8]] = {'room_r3': vals.get('r3'), 'script_r6': vals.get('r6'), 'flags_r7': vals.get('r7'), 'countdown_r8': vals.get('r8'),
                                 'delay_r9': vals.get('r9'), 'transition_r10': vals.get('r10')}
    return out


def motion_lengths(s):
    """The motions the door code plays, for the save's player 1. Length in updates: last key word >> 2 less first key
    word >> 2, over 0x10000 (the rate in chan +200, from obj +600), over every list of every track. Keys unpacked as the
    game does: 0x8012E748 -> 0x801559D8, BPE."""
    node = s.u32(s.r13 - 21492)
    obj = s.u32(node + 52)
    t, n, bones = s.u32(obj + 648), s.u16(obj + 726), s.u16(s.u32(obj + 60) + 8)
    out = {}
    for mid in (2, 25, 103, 104):
        d = s.u32(s.u32(t + 4 * mid) + 4)
        x = bytes(gametext.bpe_expand(s.bytes(s.u32(d + 280), s.u32(d + 284)), 0, s.u32(d + 284)))
        head = struct.unpack('>4I', x[:16])
        spans, lasts = [], []
        for i in range(bones):
            c1, _, p1, c2, _, p2 = struct.unpack_from('>HHIHHI', x, head[2] + 16 * i)
            for c, p in ((c1, p1), (c2, p2)):
                if c:
                    k0, kl = struct.unpack_from('>I', x, p)[0], struct.unpack_from('>I', x, p + 12 * (c - 1))[0]
                    lasts.append(kl >> 16)
                    spans.append(((kl >> 2) - (k0 >> 2)) // 0x10000)
        c = struct.unpack_from('>H', x, head[2])[0]
        p = struct.unpack_from('>I', x, head[2] + 4)[0]
        keys = [struct.unpack_from('>Hxxhhh', x, p + 12 * i) for i in range(c)]
        out[mid] = {'frames_word': s.u32(d + 8), 'events': s.u32(d + 272), 'unpacked_matches_game_size': s.u32(d + 288) == len(x),
                    'last_key_range': [min(lasts), max(lasts)], 'updates_range': [min(spans), max(spans)],
                    'first_list_third_short': {'first': keys[0][3], 'last': keys[-1][3], 'min': min(k[3] for k in keys), 'max': max(k[3] for k in keys)},
                    'first_list_first_key': keys[0]}
    return out, {'motion_99_loaded': 99 < n and bool(s.u32(t + 4 * 99)), 'motion_table_count': n, 'bones': bones}


def live_key_check(s):
    """A playing track: its live time against its key words, to prove the key-time unit and the rate."""
    node = s.u32(s.r13 - 21492)
    obj = s.u32(node + 52)
    a = s.u32(obj + 352) + 4
    ka, kb = s.u32(a + 20), s.u32(a + 24)
    return {'track_time': hex(s.u32(a)), 'segment_elapsed': hex(s.u32(a + 12)), 'segment_length': hex(s.u32(a + 16)),
            'key_a_word': hex(s.u32(ka)), 'key_b_word': hex(s.u32(kb)), 'key_diff_shifted': hex((s.u32(kb) >> 2) - (s.u32(ka) >> 2)),
            'time_equals_key_a_plus_elapsed': s.u32(a) == (s.u32(ka) >> 2) + s.u32(a + 12), 'rate_chan200': hex(s.u32(s.u32(obj + 64) + 200)),
            'rate_obj600': hex(s.u32(obj + 600))}


def main():
    rooms = json.load(open(os.path.join(DB, 'rooms.json')))
    tables = marker_tables()
    regions = region_map()
    rows, bodies, names = all_calls(rooms, tables, regions)
    table = jump_table()
    flag4_rooms = {r for r, v in tables.items() if any(x[2] & 4 for x in v)}
    gravity = {r for r, v in tables.items() if any(x[0].startswith('GravityMkr') for x in v)}
    print('jump table:')
    for n, t in table.items():
        print(' ', n, t)
    print('raw fn66 calls', len(rows), 'scripts (level,id)', len({(r['level'], r['script']) for r in rows}), 'ids', len({r['script'] for r in rows}))
    print('levels with fn66', sorted({r['level'] for r in rows}), 'levels with scripts', sorted({k[0] for k in bodies}))
    print('arg counts', collections.Counter(len(r['args']) for r in rows))
    print('transition argument always literal', all(re.fullmatch(r'-?\d+', r['args'][3]) for r in rows))
    uniq = {}
    for r in rows:
        uniq.setdefault((r['script'], tuple(r['args'])), r)
    print('distinct (script, args)', len(uniq))
    by = {}
    placed_no_flag4 = {}
    for t in sorted({int(r['args'][3]) for r in rows}):
        rs = [r for r in uniq.values() if int(r['args'][3]) == t]
        raw = [r for r in rows if int(r['args'][3]) == t]
        motion = table[t]['motion'] if t in table else None
        no_flag = sorted({('L%s/%d' % (r['level'], r['script'])) for r in raw if motion is not None and any(room not in flag4_rooms for room, _ in r['regions'])})
        for r in raw:
            if motion is not None:
                lacking = [(room, n) for room, n in r['regions'] if room not in flag4_rooms]
                if lacking:
                    placed_no_flag4['L%s/%d' % (r['level'], r['script'])] = {'transition': t, 'rooms_without_flag4': sorted(set(lacking)),
                                                                             'rooms_with_flag4': sorted({room for room, _ in r['regions'] if room in flag4_rooms})}
        by[t] = {'raw_calls': len(raw), 'distinct_script_calls': len(rs),
                 'distinct_targets': len({(r['args'][0], r['args'][1], r['args'][2], r['args'][4]) for r in rs}),
                 'prompt_before': collections.Counter(PROMPTS.get(r['prompt_before'], r['prompt_before']) for r in rs).most_common(),
                 'region_kinds': collections.Counter(re.sub(r'\d+.*', '', n) for r in rs for _, n in r['regions']).most_common(),
                 'calls_not_in_any_region_script': sum(1 for r in rs if not r['regions']),
                 'arg5': collections.Counter(r['args'][4] for r in rs).most_common(),
                 'arg6': collections.Counter(r['args'][5] for r in rs).most_common(),
                 'entry_kinds': collections.Counter(re.sub(r'\d+.*', '', str(r['entry'])) for r in rs).most_common(),
                 'ed85_just_before': sum(1 for r in rs if r['ed85_just_before']),
                 'preload_same_room': sum(1 for r in rs if r['preload_same_room']),
                 'scripts_with_fn47_sound': sum(1 for r in rs if r['sounds']),
                 'motion_scripts_in_a_room_without_flag4': no_flag,
                 'examples': sorted({'L%s/%d' % (r['level'], r['script']) for r in rs})[:8]}
        print('--- transition', t, json.dumps(by[t]))
    # Which transition each prompt leads to.
    cross = collections.defaultdict(collections.Counter)
    for r in uniq.values():
        cross[PROMPTS.get(r['prompt_before'], r['prompt_before'])][int(r['args'][3])] += 1
    print('prompt -> transition:', {str(k): dict(v) for k, v in cross.items()})
    # Marker flags, from each room's own table.
    live_markers = live_marker_check(tables)
    print('marker table against live memory:', live_markers)
    summary = collections.defaultdict(collections.Counter)
    for v in tables.values():
        for name, _, fl in v:
            summary[re.sub(r'\d+.*$', '', name)][hex(fl)] += 1
    gravity_total = sum(summary['GravityMkr'].values())
    gravity_flag4 = sum(c for k, c in summary['GravityMkr'].items() if int(k, 16) & 4)
    rooms_short = {str(r): {'rooms_json': len(rooms[str(r)]['markers']), 'table': len(v)} for r, v in tables.items() if len(rooms[str(r)]['markers']) != len(v)}
    print('rooms', len(tables), 'markers', sum(len(v) for v in tables.values()), 'rooms.json markers', sum(len(r['markers']) for r in rooms.values()), 'rooms where they differ', len(rooms_short), rooms_short)
    print('GravityMkr', gravity_total, 'with flag 4', gravity_flag4, summary['GravityMkr'])
    print('GravityMkr without flag 4', [(r, x[0], hex(x[2])) for r, v in tables.items() for x in v if x[0].startswith('GravityMkr') and not x[2] & 4])
    print('flag 4 not on a GravityMkr', [(r, x[0], hex(x[2])) for r, v in tables.items() for x in v if x[2] & 4 and not x[0].startswith('GravityMkr')])
    print('EntryMkr flags', summary['EntryMkr'])
    print('rooms with no GravityMkr', len(tables) - len(gravity), 'rooms with no flag 4 marker', len(tables) - len(flag4_rooms), sorted(set(tables) - flag4_rooms))
    print('motion scripts placed in a room with no flag 4 marker', len(placed_no_flag4), json.dumps(placed_no_flag4))
    high = sorted(r for r in tables if r >= 1000)
    print('literal fn66 calls naming room 251', sum(1 for r in rows if r['args'][0] == '251'), '; L05/421 names room', [r['args'][0] for r in rows if r['script'] == 421 and r['level'] == '05'])
    print('rooms 1000 and up', high, 'literal fn66 targets among them', sorted({r['args'][0] for r in rows if r['args'][0].isdigit() and int(r['args'][0]) >= 1000}))
    # 5th argument.
    arg5 = collections.Counter(r['args'][4] for r in rows)
    print('arg5 distinct', len(arg5), arg5.most_common(12))
    arg5_used = sorted({int(s) for s in arg5 if re.fullmatch(r'\d+', s)} - {0})
    print('arg5 values not a positive literal', [k for k in arg5 if not re.fullmatch(r'[1-9]\d*', k)], 'positive ids', len(arg5_used))
    cam_script = {}
    for (lv, sid), body in bodies.items():
        lines = [l.strip() for l in body.split('\n') if l.strip()]
        cams = [CAM_LINE.match(l) for l in lines]
        if all(CAM_ONLY_LINE.match(l) for l in lines) and any(cams):
            cam_script[sid] = int([m for m in cams if m][0].group(1))
    cam5 = [s for s in arg5_used if s in cam_script]
    print('5th-arg ids whose every line is fn2, fn3, fn4 or fn8 with a literal fn8:', len(cam5), cam5)
    same_raw = sum(1 for r in rows if r['args'][4].isdigit() and int(r['args'][4]) in cam_script and r['args'][2].isdigit() and int(r['args'][2]) == cam_script[int(r['args'][4])])
    diff_raw = sum(1 for r in rows if r['args'][4].isdigit() and int(r['args'][4]) in cam_script and r['args'][2].isdigit() and int(r['args'][2]) != cam_script[int(r['args'][4])])
    same_uniq = sum(1 for r in uniq.values() if r['args'][4].isdigit() and int(r['args'][4]) in cam_script and r['args'][2].isdigit() and int(r['args'][2]) == cam_script[int(r['args'][4])])
    diff_uniq = sum(1 for r in uniq.values() if r['args'][4].isdigit() and int(r['args'][4]) in cam_script and r['args'][2].isdigit() and int(r['args'][2]) != cam_script[int(r['args'][4])])
    in_own_level = [r for r in rows if r['args'][4].isdigit() and int(r['args'][4]) > 0 and (r['level'], int(r['args'][4])) not in bodies]
    own_cam = [r for r in rows if r['args'][4].isdigit() and int(r['args'][4]) in cam_script and (r['level'], int(r['args'][4])) in bodies]
    own_same = sum(1 for r in own_cam if r['args'][2].isdigit() and int(r['args'][2]) == cam_script[int(r['args'][4])])
    own_diff = sum(1 for r in own_cam if r['args'][2].isdigit() and int(r['args'][2]) != cam_script[int(r['args'][4])])
    print('same camera as arg3: raw', same_raw, 'different', diff_raw, '| distinct calls same', same_uniq, 'different', diff_uniq, '| raw counting only ids in the caller\'s level same', own_same, 'different', own_diff)
    print('raw calls whose 5th script id is not in the caller\'s own level:', len(in_own_level), [('L%s/%d' % (r['level'], r['script']), r['args'][4]) for r in in_own_level][:8])
    # fn112 in 5th-argument scripts: does the arrival's 10 tick fade get overwritten?
    fades = {}
    for s in arg5_used:
        for (lv, sid), body in bodies.items():
            if sid == s and 'fn112(' in body:
                lines = [l.strip() for l in body.split('\n')]
                at = next(i for i, l in enumerate(lines) if l.startswith('fn112('))
                wait = next((i for i, l in enumerate(lines) if re.match(r'(fn36|fn1)\(', l)), None)
                fades[s] = {'level': lv, 'first_fn112_line': at + 1, 'call': lines[at], 'first_fn36_or_fn1_line': None if wait is None else wait + 1, 'raw_calls_naming_it': arg5[str(s)]}
                break
    print('5th-arg scripts with fn112:', json.dumps(fades))
    # 6th and 7th.
    print('arg6', collections.Counter(r['args'][5] for r in rows))
    print('arg6 nonzero', [('L%s/%d' % (r['level'], r['script']), r['args']) for r in rows if r['args'][5] != '0'])
    print('7 args', [('L%s/%d' % (r['level'], r['script']), r['args']) for r in rows if len(r['args']) == 7])
    # Teleport named regions and their scripts.
    tele = collections.Counter()
    for rid, v in json.load(open('build/ed-db/facts/regions.json')).items():
        for g in v['regions']:
            if re.search('tele', g['name'], re.I):
                for key in REGION_FIELDS:
                    sid = g.get(key, -1)
                    if sid > 0:
                        hit = any(k[1] == sid and 'fn66(' in body for k, body in bodies.items())
                        tele[(g['name'].rstrip('0123456789'), int(rid) if int(rid) == 83 else 'other', sid, hit)] += 1
    print('teleport regions (name, room 83 or other, script, script has fn66):', sorted(tele.items(), key=str))
    prompt_counts = collections.Counter()
    for (lv, sid), body in bodies.items():
        for b, p in PROMPT.findall(body):
            prompt_counts[int(p)] += 1
    print('ed301 asks of OPEN_DOOR, TELEPORT, CLIMB_UP, CLIMB_DOWN, CLIMB_OVER:', [prompt_counts[k] for k in (1, 2, 3, 4, 5)])
    # Other natives.
    uses = collections.Counter()
    where = collections.defaultdict(list)
    for (lv, sid), body in bodies.items():
        for name in ('ed29(', 'fn41(', 'ed85(', 'ed196(', 'ed293('):
            if name in body:
                uses[name] += 1
                if name in ('ed196(', 'ed293(', 'ed29('):
                    where[name].append('L%s/%d' % (lv, sid))
    print('per-level scripts using', dict(uses), dict(where))
    print('fn79 event 135', sorted({'L%s/%d' % k for k, body in bodies.items() if re.search(r'fn79\([^,]*, 135\b', body)}))
    # Bytecode: each script is a Lua chunk in the level's pack, so a global's name appears once per chunk that uses it.
    d = disc.Disc()
    raw = collections.Counter()
    for path in d.glob(r'Levels/Level-?\d+/ScrLvl.*\.gpk'):
        b = d.read(path)
        for name in (b'fn66', b'ed29', b'ed196', b'ed293'):
            raw[name.decode()] += len(re.findall(re.escape(name) + rb'\0', b))
    print('name strings in the script packs (one per chunk):', dict(raw))
    # Ticket sites.
    sites = ticket_sites()
    print('ticket writer call sites', len(sites), json.dumps(sites))
    # Save states: what is left in the ticket, and what the player is doing.
    live = {}
    motions_by_save = {}
    extra = {}
    for slot, s in saves():
        w = [s.u32(0x8058AFFC + 4 * i) for i in range(11)]
        rec = dict(zip(('countdown', 'delay', 'room', 'script', 'flags', 'transition', 'arg7', 'entry', 'camera', 'callback', 'callback_arg'), w))
        rec['room_now'] = s.s32(s.r13 - 23276)
        rec['room_before'] = s.s32(s.r13 - 23288)
        full = []
        for r in rows:
            a = r['args']
            if a[0] == str(rec['room']) and a[1] == str(rec['entry']) and a[2] == str(rec['camera']) and a[3] == str(rec['transition']) and a[4] == str(rec['script']) and a[5] == str(rec['flags']) and (a[6] if len(a) > 6 else '1') == str(rec['arg7']):
                full.append('L%s/%d' % (r['level'], r['script']))
        rec['scripts_with_this_exact_call'] = sorted(set(full))[:6]
        rec['entry_name'] = names.get(rec['entry'])
        rec['camera_name'] = names.get(rec['camera'])
        node = s.u32(s.r13 - 21492)
        obj = s.u32(node + 52)
        chan = s.u32(obj + 64)
        rec['player_state'] = s.u32(node + 20)
        rec['player_motion'] = s.u32(s.u32(chan + 184))
        rec['player_motion_flags'] = hex(s.u32(chan + 244))
        rec['player_data_124'] = s.s32(s.u32(s.u32(node + 40)) + 124)
        rec['r13_26272'], rec['r13_26276'], rec['r13_26280'], rec['busy_r13_23344'] = s.u32(s.r13 - 26272), s.u32(s.r13 - 26276), s.u32(s.r13 - 26280), s.u32(s.r13 - 23344)
        rec['fade_black_r2_22112'] = hex(s.u32(0x80628460 - 22112))
        rec['arrival_colour_r2_16856'] = hex(s.u32(0x80628460 - 16856))
        rec['pi_r2_22240'] = s.f32(0x80628460 - 22240)
        rec['jump_table_words_match_dol'] = all(s.u32(JUMP_TABLE + 4 * i) == disc.Dol().word(JUMP_TABLE + 4 * i) for i in range(18))
        live[slot] = rec
        motions_by_save[slot], extra[slot] = motion_lengths(s)
        print(slot, rec)
        print('  motions', motions_by_save[slot], extra[slot])
        if slot in ('slot1', 'slot3', 'slot7'):
            print('  playing track', live_key_check(s))
    key_check = {slot: live_key_check(s) for slot, s in saves()}
    meaning = {}
    for n, t in table.items():
        m = t['motion']
        meaning[n] = {'motion': m, 'motion_means': MOTIONS.get(m), 'fade_out_ticks': t['fade_ticks'] if n != 16 else 62, 'fade_starts_after': t['fade_delay'],
                      'ticks_until_room_change': t['countdown'], 'source': 'read from code (0x801666A4 jump table at 0x80253F90)',
                      'used_by_scripts': by.get(n, {}).get('raw_calls', 0), 'meaning_by_usage': USAGE.get(n)}
    json.dump({'function': 'fn66 = 0x801666A4', 'arguments': ARGUMENTS, 'transitions': meaning, 'above_16': ABOVE_16, 'arrival': ARRIVAL,
               'other_room_changes': OTHER_ROUTES, 'ticket_sites': sites, 'not_room_changes': NOT_ROOM_CHANGES, 'not_found': NOT_FOUND,
               'jump_table': table, 'counts_by_transition': by, 'prompt_to_transition': {str(k): dict(v) for k, v in cross.items()},
               'arg5_raw_counts': arg5.most_common(), 'camera_only_scripts': {str(k): v for k, v in cam_script.items() if k in cam5},
               'camera_script_vs_arg3': {'raw_same': same_raw, 'raw_different': diff_raw, 'distinct_same': same_uniq, 'distinct_different': diff_uniq},
               'arg5_scripts_with_fn112': fades, 'rooms_without_GravityMkr': sorted(set(tables) - gravity), 'rooms_without_flag4_marker': sorted(set(tables) - flag4_rooms),
               'motion_scripts_in_a_room_without_flag4': placed_no_flag4,
               'marker_counts': {'GravityMkr': gravity_total, 'GravityMkr_with_flag4': gravity_flag4},
               'marker_flags': {k: dict(summary[k]) for k in ('GravityMkr', 'EntryMkr', 'SpawnMkr', 'AxisMkr', 'LookMkr', 'EntryMkr_TomeReturn', 'SanityMkr', 'SpellMkr')},
               'marker_table_vs_live': live_markers, 'rooms_json_marker_differences': rooms_short,
               'save_state_tickets': live, 'player_motions_by_save': motions_by_save, 'player_motion_table_by_save': extra, 'live_track_vs_keys': key_check},
              open('build/ed-db/facts/transitions.json', 'w'), indent=1, default=str)


if __name__ == '__main__':
    main()
