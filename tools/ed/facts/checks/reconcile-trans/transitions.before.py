#!/usr/bin/env python3
"""Every count in transitions.md, and transitions.json. Run from the repo root: python -I tools/ed/facts/transitions.py
Reads only: build/ed-db (scripts, rooms.json, disasm.txt), the disc, and saves/slot*.state (read, never written).
Writes only: build/ed-db/facts/transitions.json and prints the same numbers."""
import collections
import json
import os
import re
import struct
import sys

sys.path.insert(0, 'tools/ed')
import build_db  # noqa: E402
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
CAM_SCRIPTS = re.compile(r'\bfn8\((\d+), (\d+), (\d+), (\d+), (\d+)\)')


MOTIONS = {2: 'walk cycle: read as looped (flags 5) with a heading; "walk" is inferred', 25: 'door motion, inferred from 271/271 OPEN_DOOR. If the model has no 25: 99',
           103: 'climb-up motion, inferred from 43/43 CLIMB_UP', 104: 'climb-down motion, inferred from 45/48 CLIMB_DOWN'}
USAGE = {0: 'cut: Tome returns, mirrors, scripted moves (23 of 27 no prompt)', 1: 'elevators (EXAMINE prompt, ElevatorRgn): 2 calls', 2: 'quick exit: walk-in zones (205 of 253 no prompt); 42 OPEN_DOOR without a motion',
         3: 'slow fade with no motion (53 of 63 no prompt)', 6: 'walk while fading: JumpOverRgn only, 2 calls', 9: 'open a door: 271 of 271 OPEN_DOOR',
         12: 'climb up: 43 of 43 CLIMB_UP', 15: 'climb down: 45 of 48 CLIMB_DOWN'}
ARGUMENTS = {
    '1 room': 'ticket +8',
    '2 entry marker hash': 'ticket +28. Found by 0x80155348(hash, 2): +40 of the 76-byte marker records. Placed there and turned to its heading',
    '3 camera hash': 'ticket +32. 0x801EE998(hash, 2, 0) gives the index, 0x801EE764 sets it on the record at 0x80603288',
    '4 transition': 'ticket +20. See transitions',
    '5 script': 'ticket +12. Run by 0x801642D4(script, 0, 0) at the end of 0x80152614, after placing and the camera. 0 or below: none',
    '6 flags': 'ticket +16. 1: arrive at saved place slot 0 (0x8016490C(0), set by fn104(0, object)) not the marker. 2: 0x80043BE8(0): r13-26276 goes to 0, and r13-26272 too when the player node is r13-21488; 0x80007460 skips a controller update on 0. Also passed to 0x80152298 as its flags',
    '7 optional, default 1': 'ticket +24. Not 0: 0x80154C94 writes the new room to the player object +592 and what they hold (0x80043E78). 0: not written',
    'return': '0x800440D0: 0 means refused (0x8006E2F0 true) and nothing happens; no script reads it',
}
ABOVE_16 = '17: no motion, no fade, ticket +20 = 17, so the tick calls 0x80152298(room, arg 6) with no placement. 18 and up: as 0. No script uses any of these. 17 is what the game code itself asks for at five sites'
ARRIVAL = ['0x80155830 counts ticket +0 down once a tick; at 0 it calls 0x80152614(room, entry, camera, script, flags), or 0x80152298 for transition 17',
           '0x80152298 loads the room. Its 0x80151FDC clears busy (0x801556CC(-1, 0)), holds the player again (0x801F6828(player, 1)) and fades in over 10 frames (0x801E7C88 with transparent, r2-16856)',
           '0x80152658 is the instruction after that load returns, not a function: nothing branches to it. If r13-26280 is 0: place the player',
           'Event 238 to the player returns the motion to finish: 25->25, 99->99, 103->104, 104->103, others -1 (0x8005A8DC)',
           'If not -1: heading is marker heading + pi, event 191 plays that motion with flags 0x10002 (flag 0x2 = backwards, 0x80123264), state 80. On its end: motion 2, state 81. On that end: state 1',
           'If -1: heading is the marker heading, no motion',
           '0x80043380 calls 0x8006C008 on a kind 32 object, for what 0x8004305C found at player data +140 +68 (not traced), then event 62, camera, then the 5th argument script']
OTHER_ROUTES = {
    'ed29()': '0x800E8C9C: ticket, transition 17, to the room kept in +92 of the kind-39 data, callback 0x800E8B48 puts them back at +32. 15 per-level scripts, one id: 1023',
    '0x800E8E14': 'To room 83, transition 16, entry EntryMkr01, camera IntroCam01 or Camera01. Keeps room, place and heading first. From the kind 39 handler 0x800E860C',
    '0x8006BFAC': 'Ticket, transition 17, callback 0x8006B45C. Kind 32 handler. Purpose not traced',
    '0x8009B760': 'Ticket, transition 0, room = object +76 + 1000, count 3, delay 2. Purpose not traced',
    '0x800A2548': 'Ticket, transition 17, flags 4, sets r13-26280 so the player is not placed, callback 0x800A24E8. Kinds 28, 50, 82',
    '0x800ADA24': 'Ticket, transition 17, flags 68, callback 0x800AD8A0, from 0x800B7208 (a delayed request word r13-25172)',
    '0x800AD6F4': 'Direct 0x80152298(room, 68), from 0x800B7208',
    '0x800C04EC': 'Ticket, transition 0, to room 252, script 2779, from room 255 when flag 883 is on (0x801DC22C, as fn30), at the end of player state 8',
    '0x8012D1A0': 'Cinematic end: ticket, transition 17, flags 32, room = cinematic +120, callback 0x80152488, entry -2 (first marker with flag 0x10)',
    '0x80152488': 'The arrival for callback use: as 0x80152614 but with the script started later by 0x80163C6C(script, -1, 1)',
    '0x801525D4': 'Immediate 0x80152298 + 0x80152488, from 0x80050174 only: room 96 or a room in r3, flags 128',
    '0x800ADDCC': 'Direct 0x80152614, entry and camera from strings, from 0x80022248 and 0x800AE780',
    '0x800ADADC, 0x800AE61C, 0x8004FC3C, 0x80020328': 'Direct 0x80152298 callers, purposes not traced',
    '0x80110DA8': 'Direct 0x80152298(current room, 76): the Tome giving the room back (comment at tw_keep in hle.c)',
    '0x80157458': 'Direct 0x80152298(current room, 68), from 0x80025FF0',
}
NOT_ROOM_CHANGES = {
    'fn41': 'Sets a mode on named texture lists: hash of the 272-byte record at +2 in the room (RI_FIRE01.IFL in slot3), value through 0x801661FC. primitives.tsv calls it go_room_a: wrong',
    'ed85': '0x80145760: makes an effect object (0x801413E8) and sound 536 at 100. Not a room change. 1 fn66 call has it on the line before',
    'fn71': '0x80155600: asks the loader to get a room ready (table 0x8058AF60). primitives.tsv calls it music: wrong. 703 of 1056 literal fn66 calls have fn71 of the same room in the script',
    'fn104': 'Writes an object\'s place as 3 shorts to slot N at 0x805CC2BC. primitives.tsv calls it sound_at: wrong',
    'TeleportRegion (room 83)': 'Script 381 has no fn66',
    'coop_position_copy': 'Hook on 0x80118274 for the detail share. Nothing to do with doors, though doors.md lists it',
    '0x801EC1C8': 'Inside the camera code: at a camera\'s end it runs the script in record +68. Not a door arrival',
    '0x80134B38': 'Return address of a zone\'s script run. It compares the room before and after and stops scanning if it changed',
}
NOT_FOUND = ['What motions 25, 103 and 104 look like on screen: not watched. Stairs or ladder: not determined',
             'Seconds per tick: the tick is once per game loop, the frame rate was not measured',
             'What each motion event does (values 20, 56, 30, 31)',
             'What sound doors make: 24 of 271 door scripts call fn47, motion 25 has no events. Not traced further',
             'What flag 0x100 on a motion does beyond 0x80123F38 and 0x80124448']


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
        out[n] = {'branch': '0x%08X' % at, 'motion': motion, 'fade_frames': fade if fade != -1 else None, 'fade_delay': delay,
                  'countdown': 1 if fade == -1 else fade + 2}
    return out


def scripts():
    for lv in sorted(os.listdir(ROOT)):
        for f in sorted(os.listdir(os.path.join(ROOT, lv))):
            if f.endswith('.lua'):
                yield lv[1:], int(f[:-4]), open(os.path.join(ROOT, lv, f), encoding='utf8').read()


def all_calls(rooms):
    regions = collections.defaultdict(list)
    names = {}
    for rid, r in rooms.items():
        for x in r['regions']:
            for key in ('script', 'script2'):
                if x.get(key):
                    regions[int(x[key])].append((int(rid), x['name']))
        for m in r['markers'] + r['cameras']:
            names[m['hash']] = m['name']
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
                         'preload_any': bool(re.search(r'\bfn71\(', body)),
                         'sounds': re.findall(r'\bfn47\((\d+)', body)})
    return rows, bodies, names


def marker_flags():
    """Marker name prefix -> flags at name+72 (the word 0x80151BDC tests), across every room on the disc."""
    d = disc.Disc()
    out = collections.Counter()
    rooms_scanned = 0
    for path in d.glob(r'Rooms/.*\.bpe'):
        b = d.read(path)
        first = struct.unpack('>I', b[4:8])[0]
        csize = struct.unpack('>I', b[12:16])[0]
        sec1 = struct.unpack('>I', b[16:20])[0]
        x = bytes(gametext.bpe_expand(b, first, min(first + csize, len(b))))
        x = x[:sec1] if sec1 else x
        rooms_scanned += 1
        for m in build_db.RECORD_KINDS.finditer(x):
            name = m.group(1).decode()
            if re.search(r'Mkr|Marker', name) and m.start() + 76 <= len(x):
                out[(re.sub(r'\d+.*$', '', name), hex(struct.unpack_from('>I', x, m.start() + 72)[0]))] += 1
    return rooms_scanned, out


def main():
    rooms = json.load(open(os.path.join(DB, 'rooms.json')))
    rows, bodies, names = all_calls(rooms)
    table = jump_table()
    gravity = {int(k) for k, r in rooms.items() if any(m['name'].startswith('GravityMkr') for m in r['markers'])}
    print('jump table:')
    for n, t in table.items():
        print(' ', n, t)
    print('raw fn66 calls', len(rows), 'scripts (level,id)', len({(r['level'], r['script']) for r in rows}), 'ids', len({r['script'] for r in rows}))
    print('arg counts', collections.Counter(len(r['args']) for r in rows))
    uniq = {}
    for r in rows:
        uniq.setdefault((r['script'], tuple(r['args'])), r)
    print('distinct (script, args)', len(uniq))
    by = {}
    for t in sorted({int(r['args'][3]) for r in rows}):
        rs = [r for r in uniq.values() if int(r['args'][3]) == t]
        raw = [r for r in rows if int(r['args'][3]) == t]
        motion = table[t]['motion'] if t in table else None
        placed = [r for r in rs if r['regions']]
        no_gravity = [r for r in placed if motion is not None and all(room not in gravity for room, _ in r['regions'])]
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
                 'motion_but_source_room_has_no_GravityMkr': ['L%s/%d %s' % (r['level'], r['script'], r['regions'][:2]) for r in no_gravity],
                 'examples': sorted({'L%s/%d' % (r['level'], r['script']) for r in rs})[:8]}
        print('--- transition', t, json.dumps(by[t]))
    # Which transition each prompt leads to.
    cross = collections.defaultdict(collections.Counter)
    for r in uniq.values():
        cross[PROMPTS.get(r['prompt_before'], r['prompt_before'])][int(r['args'][3])] += 1
    print('prompt -> transition:', {str(k): dict(v) for k, v in cross.items()})
    # 5th argument.
    arg5 = collections.Counter(r['args'][4] for r in rows)
    print('arg5 distinct', len(arg5), arg5.most_common(12))
    cam_script = {}
    for (lv, sid), body in bodies.items():
        lines = [l for l in body.strip().split('\n') if l.strip()]
        cam = CAM_SCRIPTS.findall(body)
        if cam and len(lines) <= 4:
            cam_script[sid] = int(cam[0][0])
    same = diff = 0
    samples = []
    for r in uniq.values():
        s = r['args'][4]
        if s.isdigit() and int(s) in cam_script and r['args'][2].isdigit():
            if int(r['args'][2]) == cam_script[int(s)]:
                same += 1
            else:
                diff += 1
                samples.append((r['level'], r['script'], names.get(int(r['args'][2])), names.get(cam_script[int(s)])))
    print('camera-setting 5th scripts:', sorted(cam_script), 'same camera as arg3', same, 'different', diff, samples[:6])
    arg5_used = sorted({int(s) for s in arg5 if re.fullmatch(r'\d+', s)} - {0})
    print('5th-arg script ids', len(arg5_used), 'of which short camera scripts', len([s for s in arg5_used if s in cam_script]))
    # 6th and 7th.
    print('arg6', collections.Counter(r['args'][5] for r in rows))
    print('arg6 nonzero', [('L%s/%d' % (r['level'], r['script']), r['args']) for r in rows if r['args'][5] != '0'])
    print('7 args', [('L%s/%d' % (r['level'], r['script']), r['args']) for r in rows if len(r['args']) == 7])
    # Teleport named regions and their scripts.
    tele = {}
    for rid, r in rooms.items():
        for g in r['regions']:
            if re.search('tele', g['name'], re.I):
                sid = g.get('script')
                tele.setdefault(g['name'].rstrip('0123456789'), []).append((int(rid), sid))
    tele_fn66 = collections.Counter()
    for name, v in tele.items():
        for rid, sid in v:
            hit = any(k[1] == sid and 'fn66(' in body for k, body in bodies.items()) if sid else False
            tele_fn66[(name, hit)] += 1
    print('teleport regions (name, script has fn66):', dict(tele_fn66))
    prompt_counts = collections.Counter()
    for (lv, sid), body in bodies.items():
        for b, p in PROMPT.findall(body):
            prompt_counts[int(p)] += 1
    print('ed301 asks of OPEN_DOOR, TELEPORT, CLIMB_UP, CLIMB_DOWN, CLIMB_OVER:', [prompt_counts[k] for k in (1, 2, 3, 4, 5)])
    # Other natives.
    uses = collections.Counter()
    for (lv, sid), body in bodies.items():
        for name in ('ed29(', 'fn41(', 'ed85('):
            if name in body:
                uses[name] += 1
    print('per-level scripts using', dict(uses))
    # Marker flags.
    scanned, flags = marker_flags()
    print('marker flags over', scanned, 'rooms:')
    summary = collections.defaultdict(dict)
    for (name, fl), n in sorted(flags.items()):
        summary[name][fl] = n
    for name in ('GravityMkr', 'EntryMkr', 'SpawnMkr', 'AxisMkr', 'LookMkr'):
        print(' ', name, summary.get(name))
    only4 = {name: v for name, v in summary.items() if any(int(k, 16) & 4 for k in v)}
    print(' every marker name with flag 4 set:', only4)
    only10 = {name: v for name, v in summary.items() if any(int(k, 16) & 0x10 for k in v)}
    print(' every marker name with flag 0x10 set:', only10)
    print('rooms with no GravityMkr', len(rooms) - len(gravity), sorted(int(k) for k in rooms if int(k) not in gravity))
    # Save states: the last room ticket is left in the record.
    live = {}
    for slot in ('slot1', 'slot2', 'slot3', 'slot7', 'slot10'):
        s = savestate.State('saves/%s.state' % slot)
        rec = {name: s.u32(0x8058AFFC + off) for name, off in (('countdown', 0), ('delay', 4), ('room', 8), ('script', 12), ('flags', 16), ('transition', 20),
               ('arg7', 24), ('entry', 28), ('camera', 32), ('callback', 36), ('callback_arg', 40))}
        rec['room_now'] = s.s32(s.r13 - 23276)
        match = ['L%s/%d' % (r['level'], r['script']) for r in rows if r['args'][0] == str(rec['room']) and r['args'][1] == str(rec['entry']) and
                 r['args'][2] == str(rec['camera']) and r['args'][3] == str(rec['transition'])]
        rec['scripts_with_this_exact_call'] = match[:6]
        rec['entry_name'] = names.get(rec['entry'])
        rec['camera_name'] = names.get(rec['camera'])
        node = s.u32(s.r13 - 21492)
        obj = s.u32(node + 52)
        chan = s.u32(obj + 64)
        rec['player_state'] = s.u32(node + 20)
        rec['player_motion'] = s.u32(s.u32(chan + 184))
        rec['player_motion_flags'] = hex(s.u32(chan + 244))
        t, n = s.u32(obj + 648), s.u16(obj + 726)
        rec['player_motions_loaded'] = {i: s.u32(s.u32(s.u32(t + 4 * i) + 4) + 8) for i in range(n) if i in (1, 2, 3, 15, 25, 99, 103, 104) and s.u32(t + 4 * i)}
        live[slot] = rec
        print(slot, rec)
    # Motion data, unpacked as the game does (0x8012E748 -> 0x801559D8, BPE) from slot1's player.
    s = savestate.State('saves/slot1.state')
    obj = s.u32(s.r13 - 26524)
    t = s.u32(obj + 648)
    motions = {}
    for mid in (2, 25, 103, 104):
        d = s.u32(s.u32(t + 4 * mid) + 4)
        frames, nev = s.u32(d + 8), s.u32(d + 272)
        raw_len = s.u32(d + 288)
        x = bytes(gametext.bpe_expand(s.bytes(s.u32(d + 280), s.u32(d + 284)), 0, s.u32(d + 284)))
        head = struct.unpack('>4I', x[:16])
        events = [struct.unpack('>HH', x[head[0] + 12 * i:head[0] + 12 * i + 4])[0:1] + struct.unpack('>H', x[head[0] + 12 * i + 10:head[0] + 12 * i + 12]) for i in range(nev)]
        tr = head[2]
        count, _, ptr, _, _, _ = struct.unpack('>HHIHHI', x[tr:tr + 16])
        keys = [struct.unpack('>Hxxhhh', x[ptr + 12 * i:ptr + 12 * i + 10]) for i in range(count)]
        motions[mid] = {'frames': frames, 'unpacked_bytes_match_game': raw_len == len(x), 'events_time_value': events,
                        'root_first': keys[0], 'root_last': keys[-1], 'root_z_min': min(k[3] for k in keys), 'root_z_max': max(k[3] for k in keys)}
        print('motion', mid, motions[mid])
    rot103 = motions[103]['root_first'] == motions[104]['root_first']
    print('103 and 104 start on the same root key:', rot103)
    meaning = {}
    for n, t in table.items():
        m = t['motion']
        meaning[n] = {'motion': m, 'motion_means': MOTIONS.get(m), 'fade_out_frames': t['fade_frames'] if n != 16 else 62, 'fade_starts_after': t['fade_delay'],
                      'ticks_until_room_change': t['countdown'], 'source': 'read from code (0x801666A4 jump table at 0x80253F90)',
                      'used_by_scripts': by.get(n, {}).get('raw_calls', 0), 'meaning_by_usage': USAGE.get(n)}
    json.dump({'function': 'fn66 = 0x801666A4', 'arguments': ARGUMENTS, 'transitions': meaning, 'above_16': ABOVE_16, 'arrival': ARRIVAL,
               'other_room_changes': OTHER_ROUTES, 'not_room_changes': NOT_ROOM_CHANGES, 'not_found': NOT_FOUND,
               'jump_table': table, 'counts_by_transition': by, 'prompt_to_transition': {str(k): dict(v) for k, v in cross.items()},
               'arg5_raw_counts': arg5.most_common(), 'camera_only_scripts': cam_script, 'camera_script_vs_arg3': {'same': same, 'different': diff},
               'rooms_without_GravityMkr': sorted(int(k) for k in rooms if int(k) not in gravity),
               'marker_flags': {k: v for k, v in summary.items() if k in ('GravityMkr', 'EntryMkr', 'SpawnMkr', 'AxisMkr', 'LookMkr', 'EntryMkr_TomeReturn', 'SanityMkr', 'SpellMkr')},
               'save_state_tickets': live, 'player_motions_slot1': motions}, open('build/ed-db/facts/transitions.json', 'w'), indent=1, default=str)


if __name__ == '__main__':
    main()
