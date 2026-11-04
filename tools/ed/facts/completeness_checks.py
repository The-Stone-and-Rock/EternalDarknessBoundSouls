#!/usr/bin/env python3
"""The three further checks in completeness.md. Run from the repo root: python -I tools/ed/facts/completeness_checks.py
Reads the disc, build/ed-db and build/ed-db/facts's JSON. Never writes outside build/ed-db/facts.
Writes doors_table.json, examines_table.json, item_script_levels.json, who_table.json and prints every count quoted."""
import collections
import json
import os
import re
import sys

sys.path.insert(0, 'tools/ed')
sys.path.insert(0, 'tools/ed/facts')
import transitions as T  # noqa: E402

R = 'build/ed-db/facts/'
ARG = re.compile(r'\b(ed219|ed220|ed221)\(([^()]*)\)')
FN66 = re.compile(r'\bfn66\(([^()]*)\)')
ASK = re.compile(r'\bed301\((-?\d+), (-?\d+)\)')
FIELDS = ('enter', 'leave', 'stay', 'other_enter', 'other_leave', 'other_stay', 'cross_a', 'cross_b')


def jl(p):
    return json.load(open(p, encoding='utf8'))


def level_rooms():
    out = {}
    for k, v in jl(R + 'levels.json')['levels'].items():
        s = set(v.get('placed_rooms_Npcs') or []) | set(v.get('rooms_by_door_walk_from_those') or [])
        if v.get('start_room_0x8023D738') is not None:
            s.add(v['start_room_0x8023D738'])
        out[k] = s
    return out


def region_index():
    out = collections.defaultdict(list)
    for rid, v in jl(R + 'regions.json').items():
        for r in v['regions']:
            for f in FIELDS:
                if r.get(f, -1) > 0:
                    out[r[f]].append({'room': int(rid), 'region': r['name'], 'field': f})
    return out


def placed_by(sid, lv, regions, rooms):
    """Regions naming this script. Those in the level's own rooms if any are, else all, marked."""
    all_ = regions.get(sid, [])
    mine = [x for x in all_ if x['room'] in rooms.get(lv, ())]
    return (mine, True) if mine else (all_, False)


def named_by(bodies, items):
    """Who else names a script id as a whole-number argument, per level. Text search: a place to look, not proof of a call."""
    pat = re.compile(r'\b[a-zA-Z_][a-zA-Z_0-9]*\(([^()]*)\)')
    out = collections.defaultdict(set)
    for (lv, sid), body in bodies.items():
        for m in pat.finditer(body):
            for a in m.group(1).split(','):
                a = a.strip()
                if a.isdigit() and int(a) > 0:
                    out[(lv, int(a))].add(sid)
    return out


def check_doors(bodies, regions, rooms):
    tables = T.marker_tables()
    cams = jl(R + 'cameras_all.json')
    tr = jl(R + 'transitions.json')['transitions']
    marker_name, cam_name = {}, {}
    for rid, recs in tables.items():
        for name, h, _ in recs:
            marker_name[h] = name
    cam_room = {}
    for rid, v in cams.items():
        cam_room[int(rid)] = {e['hash'] for e in v['eyes']}
        for e in v['eyes']:
            cam_name[e['hash']] = e['name']
    names = named_by(bodies, None)
    on_item = collections.defaultdict(list)
    for it in jl(R + 'item_table.json')['rows']:
        for fld in ('use_script', 'mix_script', 'after_find_script', 'after_examine_script'):
            if it[fld]:
                on_item[it[fld]].append({'type_id': it['id'], 'label': it['label'], 'field': fld})
    print('room 64 markers', tables.get(64))
    rows = []
    for (lv, sid), body in sorted(bodies.items()):
        for m in FN66.finditer(body):
            a = [x.strip() for x in m.group(1).split(',')]
            lit = [int(x) if re.fullmatch(r'-?\d+', x) else x for x in a]
            before = ASK.findall(body[:m.start()])
            asks = sorted({int(p) for _, p in ASK.findall(body)})
            frm, own = placed_by(sid, lv, regions, rooms)
            via = []
            if not frm:
                for other in sorted(names.get((lv, sid), ())):
                    if other != sid:
                        f2, own2 = placed_by(other, lv, regions, rooms)
                        via.append({'script': other, 'regions': f2[:4]})
            to, ent, cam, t = lit[0], lit[1], lit[2], lit[3]
            te = tr.get(str(t)) if isinstance(t, int) else None
            rows.append({'level': lv, 'script': sid, 'from': frm, 'from_in_level_rooms': own, 'named_by': via, 'on_item_record': on_item.get(sid, []),
                         'source': 'region' if frm else 'item_record' if on_item.get(sid) else 'named_by_script' if via else 'none_found', 'asks_in_script': asks,
                         'ask_before': int(before[-1][1]) if before else None, 'to_room': to, 'entry_hash': ent if isinstance(ent, int) else None,
                         'entry': marker_name.get(ent, ent if isinstance(ent, str) else None),
                         'entry_in_to_room': (isinstance(ent, int) and isinstance(to, int) and ent in {h for _, h, _ in tables.get(to, [])}),
                         'camera_hash': cam if isinstance(cam, int) else None, 'camera': cam_name.get(cam, cam if isinstance(cam, str) else None),
                         'camera_in_to_room': (isinstance(cam, int) and isinstance(to, int) and cam in cam_room.get(to, ())),
                         'transition': t, 'motion': te and te['motion'], 'fade_out_ticks': te and te['fade_out_ticks'],
                         'ticks_until_room_change': te and te['ticks_until_room_change'], 'after_script': lit[4], 'flags': lit[5],
                         'arg7': lit[6] if len(lit) > 6 else None})
    json.dump(rows, open(R + 'doors_table.json', 'w', encoding='utf8'), indent=0)
    n = len(rows)
    print('\nDOORS raw fn66', n, 'with literal room', sum(isinstance(r['to_room'], int) for r in rows))
    print(' placed by a region, in that level\'s rooms', sum(1 for r in rows if r['from'] and r['from_in_level_rooms']))
    print(' placed by a region, but not one in that level\'s known rooms', sum(1 for r in rows if r['from'] and not r['from_in_level_rooms']))
    print(' by source column', collections.Counter(r['source'] for r in rows))
    nowhere = sorted({(r['level'], r['script']) for r in rows if r['source'] == 'none_found'})
    print('  those scripts', nowhere[:40])
    lit = [r for r in rows if isinstance(r['to_room'], int)]
    badE = [r for r in lit if not r['entry_in_to_room']]
    badC = [r for r in lit if not r['camera_in_to_room']]
    print(' entry hash is a marker in the target room', len(lit) - len(badE), 'of', len(lit))
    print(' camera hash is a camera in the target room', len(lit) - len(badC), 'of', len(lit))
    print('  entry misses', collections.Counter((r['to_room'], r['entry'], r['entry_hash']) for r in badE).most_common(25))
    print('  camera misses', collections.Counter((r['to_room'], r['camera'], r['camera_hash']) for r in badC).most_common(25))
    print('  entry misses, transition', collections.Counter(r['transition'] for r in badE))
    # the usage column, by whole-script ask, not text position
    by = collections.defaultdict(lambda: collections.Counter())
    distinct = collections.defaultdict(set)
    for r in rows:
        k = (r['to_room'], r['entry_hash'], r['camera_hash'], r['transition'], r['after_script'], r['flags'], r['arg7'])
        distinct[r['transition']].add(k)
        by[r['transition']][tuple(r['asks_in_script'])] += 1
    for t in (9, 12, 15, 6, 1):
        print(' transition', t, 'raw', sum(by[t].values()), 'by whole-script asks', by[t].most_common(6))
    # from a door: the rows a person means by "a door"
    door = [r for r in rows if r['transition'] == 9]
    print(' transition 9: from a region', sum(1 for r in door if r['from']), 'enter-field', sum(1 for r in door if any(x['field'] == 'enter' for x in r['from'])), 'no region', sum(1 for r in door if not r['from']))
    print(' transition 9: scripts with several from rooms', sum(1 for r in door if len({x['room'] for x in r['from']}) > 1))
    # room pairs that have a door one way only, within a level's rows
    pairs = {(x['room'], r['to_room'], r['level']) for r in door if isinstance(r['to_room'], int) for x in r['from']}
    back = sum(1 for a, b, l in pairs if (b, a, l) in pairs)
    print(' transition 9 room pairs (from, to, level)', len(pairs), 'with a transition 9 the other way', back)
    return rows


def check_examines(bodies, regions, rooms):
    text = jl('build/ed-db/text.json')
    placed = jl(R + 'placed_things.json')
    items = jl(R + 'item_table.json')['rows']
    spawn = collections.defaultdict(list)
    for r in items:
        for s in r['spawned_by']:
            spawn[s['tag']].append(r['id'])
    rows = []
    for (lv, sid), body in sorted(bodies.items()):
        for m in ARG.finditer(body):
            a = [x.strip() for x in m.group(2).split(',')]
            lit = [int(x) if re.fullmatch(r'-?\d+', x) else x for x in a]
            frm, own = placed_by(sid, lv, regions, rooms)
            tx = text.get('room' + lv, {})
            g, e = lit[0], lit[1]
            w = tx.get('%s,%s' % (g, e)) if isinstance(g, int) and isinstance(e, int) else None
            row = {'level': lv, 'script': sid, 'native': m.group(1), 'group': g, 'entry': e, 'label': w and w[1], 'words': w and w[2], 'args': lit, 'from': frm}
            if m.group(1) == 'ed221' and len(lit) >= 6:
                tag = lit[4]
                num = {'00': '0', '12': '0', '16': '16'}.get(lv, str(int(lv)))
                recs = [p for p in placed.get(num, []) if p.get('tag') == tag]
                rms = {x['room'] for x in frm}
                inroom = [p for p in recs if p['room'] in rms]
                use = inroom or recs
                types = sorted({p['type_id'] for p in use if p.get('type_id') is not None} | {x['type_id'] for p in use for x in p.get('by_alignment', []) if x.get('type_id') is not None})
                row.update({'tag': tag, 'fallback_entry': lit[5], 'n': lit[3], 'camera_hash': lit[2], 'placed_records': len(recs), 'in_script_room': len(inroom),
                            'type_ids': types, 'item_names': [items[t]['name'] for t in types if 0 <= t < len(items)],
                            'spawned_types_with_tag': sorted(set(spawn.get(tag, [])))[:6]})
            rows.append(row)
    json.dump(rows, open(R + 'examines_table.json', 'w', encoding='utf8'), indent=0)
    c = collections.Counter(r['native'] for r in rows)
    print('\nEXAMINES', c, 'words found', sum(1 for r in rows if r['words']), 'of', len(rows), 'with a region', sum(1 for r in rows if r['from']))
    missing = [(r['level'], r['script'], r['native'], r['group'], r['entry']) for r in rows if not r['words'] and isinstance(r['entry'], int) and r['entry'] >= 0]
    print(' no words at that group,entry, entry >= 0', len(missing), missing[:20])
    print(' entry below 0 or variable', sum(1 for r in rows if not isinstance(r['entry'], int) or r['entry'] < 0))
    e221 = [r for r in rows if r['native'] == 'ed221']
    print(' ed221', len(e221), 'tag placed', sum(1 for r in e221 if r['placed_records']), 'in the script\'s room', sum(1 for r in e221 if r['in_script_room']),
          'resolves to an item type', sum(1 for r in e221 if r['type_ids']), 'one type', sum(1 for r in e221 if len(r['type_ids']) == 1))
    print(' ed221 placed record is not a type-holding section (no type id)', [(r['level'], r['script'], r['tag']) for r in e221 if r['placed_records'] and not r['type_ids']][:25])
    print(' ed221 with no placed record', [(r['level'], r['script'], r['tag'], r['spawned_types_with_tag']) for r in e221 if not r['placed_records']][:15])
    return rows


def check_item_levels(bodies):
    items = jl(R + 'item_table.json')['rows']
    have = collections.defaultdict(set)
    for lv, sid in bodies:
        have[sid].add(lv)
    # A chapter's Npcs file serves which script packs: 0 and 16 turn into 0, or 12 with flag 787 (arguments.md section 5). L13 and the others their own.
    serves = {'0': {'00', '12', '16'}, '16': {'00', '12', '09', '16'}}
    rows, gaps, field_totals = [], [], collections.Counter()
    for r in items:
        row = {'id': r['id'], 'name': r['name'], 'label': r['label'], 'scripts': {}, 'placed_levels': sorted({re.search(r'\d+', p['file']).group(0) for p in r['placed']})}
        sp = {s['script'] for s in r['spawned_by']}
        row['spawned_levels'] = sorted({lv for s in sp for lv in have.get(s, ())})
        for f in ('use_script', 'mix_script', 'after_find_script', 'after_examine_script'):
            sid = r[f]
            if not sid:
                continue
            row['scripts'][f] = {'id': sid, 'levels': sorted(have.get(sid, ()))}
            field_totals[f] += 1
            source = set()
            for n in row['placed_levels']:
                source |= serves.get(n, {'%02d' % int(n)})
            source |= set(row['spawned_levels'])
            if not have.get(sid):
                gaps.append((r['id'], r['name'], f, sid, 'in no level', sorted(source)))
            elif source and not (source & have[sid]):
                gaps.append((r['id'], r['name'], f, sid, 'levels ' + ','.join(sorted(have[sid])), sorted(source)))
            elif source and (source - have[sid]):
                row.setdefault('script_missing_in_a_level_it_appears', {})[f] = sorted(source - have[sid])
        rows.append(row)
    json.dump(rows, open(R + 'item_script_levels.json', 'w', encoding='utf8'), indent=0)
    print('\nITEM SCRIPTS by field', field_totals)
    print(' script exists in no level, or never in a level the item appears in:', len(gaps))
    for g in gaps:
        print('  ', g)
    partial = [(x['id'], x['name'], x['script_missing_in_a_level_it_appears']) for x in rows if x.get('script_missing_in_a_level_it_appears')]
    print(' script missing from SOME level the item appears in:', len(partial))
    for p in partial[:60]:
        print('  ', p)
    src = sum(1 for x in rows if x['scripts'] and (x['placed_levels'] or x['spawned_levels']))
    print(' rows with a script and a known source', src, 'rows with a script', sum(1 for x in rows if x['scripts']))
    return rows


# who: which of "the player" words each game function reads
LINE = re.compile(r'^([0-9A-F]{8}): (.*)$')
WORDS = {'-21496(r13)': 'current_id', '-21492(r13)': 'current_node', '-21488(r13)': 'main_id', '-21484(r13)': 'main_node', '-26524(r13)': 'current_model'}
GETTERS = {0x801F62B8: 'current_id', 0x801F62B0: 'current_node', 0x801F6258: 'main_id', 0x801F6250: 'main_node'}


def load_dis():
    d = {}
    for ln in open('build/ed-db/disasm.txt', encoding='utf8', errors='replace'):
        m = LINE.match(ln)
        if m:
            d[int(m.group(1), 16)] = m.group(2).strip()
    return d


def walk(dis, start, limit=6000):
    seen, todo, calls, jump = set(), [start], [], False
    while todo and len(seen) < limit:
        a = todo.pop()
        while a in dis and a not in seen:
            seen.add(a)
            t = dis[a]
            op = t.split()[0]
            if op == 'blr':
                break
            if op == 'bctr':
                jump = True
                break
            m = re.match(r'^bl\s+0x([0-9A-F]+)', t)
            if m:
                calls.append((a, int(m.group(1), 16)))
            elif op == 'b':
                x = re.search(r'0x([0-9A-F]+)', t)
                if x:
                    a = int(x.group(1), 16)
                    continue
                break
            elif op == 'bc':
                x = re.search(r'0x([0-9A-F]+)$', t)
                if x:
                    todo.append(int(x.group(1), 16))
                if t.startswith('bc    20'):
                    break
            a += 4
    return seen, calls, jump


def reads(dis, seen, calls):
    out = collections.defaultdict(list)
    for a in sorted(seen):
        t = dis[a]
        for k, n in WORDS.items():
            if k in t:
                out[n].append('%08X' % a)
    for a, c in calls:
        if c in GETTERS:
            out[GETTERS[c]].append('%08X' % a)
    return out


def check_who():
    dis = load_dis()
    nat = {}
    for ln in open('build/ed-db/natives.tsv', encoding='utf8'):
        p = ln.rstrip('\n').split('\t')
        nat[p[0]] = int(p[1], 16)
    F = [('Tome Use, A', 0x801043A8), ('Tome Equip, A', 0x801043F0), ('Tome Check, A', 0x801044C0), ('Tome Mix first, A', 0x80104580), ('Tome Mix second, A', 0x8010422C),
         ('Tome Mode, A', 0x801045EC), ('Tome list builder', 0x80101DC0), ('Tome action allowed', 0x80102C28), ('equip toggle', 0x800C9FD4), ('find carried ammo', 0x801F9B18),
         ('fn92 item type', nat.get('fn92')), ('fn93 take item', nat.get('fn93')), ('fn162 item field set', nat.get('fn162')), ('ed20 chosen item', nat.get('ed20')), ('ed140 reload check', nat.get('ed140')),
         ('ed131 ammo merge', nat.get('ed131')), ('ed158 item charges', nat.get('ed158')), ('ed52 find', nat.get('ed52')), ('ed219 examine', nat.get('ed219')), ('ed220 examine words', nat.get('ed220')),
         ('ed221 examine + pick up', nat.get('ed221')), ('ed301 prompt', nat.get('ed301')), ('prompt press', 0x80006EBC), ('prompt ask', 0x80117404), ('close-up stepper', 0x80024D78),
         ('the find', 0x80024528), ('pick up: make it a kind', 0x801F9DE8), ('world pickup, event 71', 0x800C362C), ('asks pick up/reload/autopsy/finish', 0x800C030C),
         ('Z reload', 0x800C2CA4), ('reload prompt test', 0x800C05F0), ('load all', 0x8007A1DC), ('load one', 0x8007A09C), ('fn66 door', nat.get('fn66')), ('ticket writer', 0x80155770), ('ticket tick', 0x80155830),
         ('arrival', 0x80152614), ('room load', 0x80152298), ('closes the wait', 0x80151FDC), ('door motion', 0x801669AC), ('fn78 find handle', nat.get('fn78')), ('fn25 room', nat.get('fn25'))]
    rows = []
    for name, addr in F:
        if addr is None or addr not in dis:
            rows.append({'what': name, 'addr': addr, 'error': 'address not in disassembly'})
            continue
        seen, calls, jump = walk(dis, addr)
        d = reads(dis, seen, calls)
        one = collections.defaultdict(list)
        for a, c in calls:
            if c in GETTERS:
                continue
            s2, c2, _ = walk(dis, c, 1500)
            for k, v in reads(dis, s2, c2).items():
                one[k].append('%08X' % c)
        rows.append({'what': name, 'addr': '%08X' % addr, 'instructions': len(seen), 'calls': len(calls), 'has_jump_table': jump,
                     'direct': {k: v for k, v in sorted(d.items())}, 'one_call_down': {k: sorted(set(v)) for k, v in sorted(one.items())}})
    json.dump(rows, open(R + 'who_table.json', 'w', encoding='utf8'), indent=1)
    print('\nWHO: direct reads, then callees that read directly')
    for r in rows:
        if 'error' in r:
            print(' ', r['what'], r['error'])
            continue
        print(' %-38s %s%s direct=%s one_down=%s' % (r['what'], r['addr'], ' JT' if r['has_jump_table'] else '', {k: len(v) for k, v in r['direct'].items()}, {k: v[:3] for k, v in r['one_call_down'].items()}))
    return rows


def main():
    bodies = {(lv, sid): body for lv, sid, body in T.scripts()}
    regions = region_index()
    rooms = level_rooms()
    check_item_levels(bodies)
    check_examines(bodies, regions, rooms)
    check_who()
    check_doors(bodies, regions, rooms)


if __name__ == '__main__':
    main()
