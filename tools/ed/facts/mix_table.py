"""THE MIX TABLE, from the disc. python -I tools/ed/facts/mix_table.py  (run from the repo root; writes build/ed-db/facts/mix_table.json)

There is no recipe table in the game. Mix runs the FIRST picked item's script (item record +42) and that script decides.
So: InvU.bin gives every item kind its scripts and words, the script packs give the recipes (each mix script is run here
for every pair of kinds it names), InvS.bin + InvE.bin + ai/ed_chrs.dat give the ammo classes and hands.

Reads only the disc (dump/GEDP01.iso). Save states are read only for one extra column: kinds seen live with a template.
"""
import glob
import json
import os
import re
import struct
import sys

ROOT = os.getcwd()
sys.path.insert(0, os.path.join(ROOT, 'tools', 'ed'))
import disc      # noqa: E402
import gametext  # noqa: E402
import lua40     # noqa: E402

D = disc.Disc()


def table(name):
    b = D.read(name)
    n = struct.unpack('>I', b[:4])[0]
    return [b[o:o + s] for o, s in (struct.unpack('>II', b[8 + i * 8:16 + i * 8]) for i in range(n))]


boot = D.read('UBootPak.bin')
secs = gametext.pack_table(boot)
TEXT = gametext.read_groups(boot[secs[4][0]:secs[4][0] + secs[4][1]])  # (group, entry) -> (group name, label, words)


def words(g, i):
    r = TEXT.get((g, i))
    return {'label': r[1], 'words': r[2]} if r else None


# Every kind: InvU.bin, record = 16 byte chunk header + the 88 bytes that 0x8015062C copies to the object's +4.
KINDS = []
for k, raw in enumerate(table('InvU.bin')):
    r = raw[16:]
    assert len(r) == 88
    g = r[76]
    KINDS.append({
        'kind': k,
        'name': (words(g, r[79]) or {}).get('words'),
        'label': (words(g, r[79]) or {}).get('label'),
        'text_group': g, 'text_pickup': r[77], 'text_desc': r[78], 'text_inv': r[79],
        'mode': struct.unpack('>I', r[0:4])[0],
        'flags': struct.unpack('>I', r[4:8])[0],
        'pickup_script': struct.unpack('>h', r[36:38])[0],   # +36, 0x80150868: run at 0x800D9BA4 with pTarget = the item's id
        'use_script': struct.unpack('>h', r[38:40])[0],      # +38, 0x80150850: Use
        'check_script': struct.unpack('>h', r[40:42])[0],    # +40, 0x8015085C: A while reading its Check words
        'mix_script': struct.unpack('>h', r[42:44])[0],      # +42, 0x80150874: Mix, on the first picked; RELOAD prompt on a held weapon
        'count': struct.unpack('>H', r[68:70])[0],           # +68, 0x801508FC: loaded, or ammo in the box, or charges
        'count_max': struct.unpack('>H', r[70:72])[0],       # +70, 0x801508B0: clip size
    })

# Templates: InvS.bin, 55 of them; object +0 is one, chosen by the thing's definition (ai/ed_chrs.dat +136), not by the kind.
HANDS = [[w for w in struct.unpack('>32I', r[16:144]) if w] for r in table('InvE.bin')]
TEMPLATES = []
for i, r in enumerate(table('InvS.bin')):
    w = struct.unpack('>9I', r[0x60:0x84])
    TEMPLATES.append({'template': i, 'hands_set': w[0], 'hands': HANDS[w[0]] if w[0] < len(HANDS) else None, 'flags': w[4],
                      'modes': w[5], 'third_hand_word': w[6], 'ammo_class': w[7] >> 16})
chrs = D.read('ai/ed_chrs.dat')
DEFS = {}
for i in range((len(chrs) - 32) // 224):
    r = chrs[32 + i * 224:32 + (i + 1) * 224]
    DEFS[struct.unpack('>i', r[4:8])[0]] = struct.unpack('>h', r[136:138])[0]

# Every script on the disc, decompiled, level by level.
SCRIPTS = {}
for path in D.glob(r'ScrLvl.*\.gpk'):
    level = re.search(r'Level(-?\d+)/ScrLvl', path).group(1)
    for sid, data in lua40.pack_entries(D.read(path)).items():
        SCRIPTS.setdefault(sid, {})[level] = lua40.decompile(lua40.parse(data))


def simulate(text, state, a, b):
    """Walk a decompiled script's statements with fn0()=state, fn92(first)=a, fn92(second)=b. Returns what it would call."""
    lines = [l.strip() for l in text.splitlines() if l.strip()]
    labels = {l[2:-2]: i for i, l in enumerate(lines) if l.startswith('::')}
    env, out = {}, {'next_state': None, 'removes': [], 'closes_tome_with': None, 'words': [], 'spawns': [], 'runs': []}
    i, steps = 0, 0

    def val(e):
        e = e.strip()
        if re.fullmatch(r'-?\d+', e):
            return int(e)
        if e == 'fn0()':
            return state
        return env.get(e)

    while i < len(lines) and steps < 5000:
        steps += 1
        l = lines[i]
        i += 1
        m = re.fullmatch(r'if (.+?) ~= (.+?) then goto (L\d+) end', l)
        if m:
            if val(m.group(1)) != val(m.group(2)):
                i = labels[m.group(3)]
            continue
        m = re.fullmatch(r'goto (L\d+)', l)
        if m:
            i = labels[m.group(1)]
            continue
        if l.startswith('::'):
            continue
        m = re.fullmatch(r'(?:local )?(L\d+) = (.+)', l)
        if m:
            name, e = m.groups()
            if e == 'ed20(0)':
                env[name] = 'first'
            elif e == 'ed20(1)':
                env[name] = 'second'
            elif re.fullmatch(r'fn92\(L\d+\)', e):
                who = env.get(e[5:-1])
                env[name] = a if who == 'first' else b if who == 'second' else None
            elif re.fullmatch(r'-?\d+', e):
                env[name] = int(e)
            else:
                env[name] = None
            n = re.match(r'ed3\((-?\d+), (-?\d+), (-?\d+), (-?\d+)', e)
            if n:
                out['spawns'].append([int(x) for x in n.groups()])
            continue
        m = re.fullmatch(r'fn1\((\d+)\)', l)
        if m:
            out['next_state'] = int(m.group(1))
            continue
        m = re.fullmatch(r'fn93\((L\d+)\)', l)
        if m:
            out['removes'].append(env.get(m.group(1)))
            continue
        m = re.fullmatch(r'ed9\((\d+)\)', l)
        if m:
            out['closes_tome_with'] = int(m.group(1))
            continue
        m = re.fullmatch(r'ed11\((\d+), (\d+), (\d+)\)', l)
        if m:
            out['words'].append([int(x) for x in m.groups()])
            continue
        n = re.match(r'ed3\((-?\d+), (-?\d+), (-?\d+), (-?\d+)', l)
        if n:
            out['spawns'].append([int(x) for x in n.groups()])
            continue
        m = re.fullmatch(r'fn73\((\d+)\)', l)
        if m:
            out['runs'].append(int(m.group(1)))
    return out


def recipes(sid):
    """All pairs of kinds a two-part script names, played from state 0 through the states it asks for."""
    versions = SCRIPTS[sid]
    text = next(iter(versions.values()))
    named = sorted(set(int(x) for x in re.findall(r'if L\d+ ~= (\d+) then goto', text)))
    named = [k for k in named if k > 3]  # the small ones compared here are states and fn0 counters, never kinds
    out = []
    for a in named:
        for b in named:
            s0 = simulate(text, 0, a, b)
            row = {'first': a, 'second': b, 'removes_both': sorted(str(x) for x in s0['removes']) == ['first', 'second'],
                   'closes_tome_with': s0['closes_tome_with'], 'makes': [], 'words': s0['words'], 'runs': s0['runs']}
            state, seen = s0['next_state'], set()
            while state and state not in seen:
                seen.add(state)
                s = simulate(text, state, a, b)
                row['makes'] += [{'def': d, 'model': m, 'kind': k} for d, m, _, k in s['spawns']]
                state = s['next_state']
            out.append(row)
    return named, out


def nm(k):
    return KINDS[k]['name'] if 0 <= k < len(KINDS) else None


ED3_KIND = re.compile(r'ed3\((-?\d+), -?\w+, -?\d+, (\d+)')
SPAWNS = {}
for sid, v in SCRIPTS.items():
    for level, text in v.items():
        for _, k in ED3_KIND.findall(text):
            SPAWNS.setdefault(int(k), set()).add('L%s/%d' % (level, sid))


MIX_SCRIPTS = sorted(set(k['mix_script'] for k in KINDS if k['mix_script']))
AMMO_SCRIPTS = [s for s in MIX_SCRIPTS if any('ed140(' in t for t in SCRIPTS.get(s, {}).values())]
TWO_READS = sorted(s for s, v in SCRIPTS.items() if any('ed20(1)' in t for t in v.values()))
PIECES = {}
for sid in TWO_READS:
    if sid in AMMO_SCRIPTS:
        continue
    named, rows = recipes(sid)
    good = [r for r in rows if r['makes'] or r['removes_both']]
    PIECES[sid] = {
        'levels': sorted(SCRIPTS[sid]),
        'kinds_with_this_as_mix_script': [k['kind'] for k in KINDS if k['mix_script'] == sid],
        'reachable_by_mix_action': any(k['mix_script'] == sid for k in KINDS),
        'kinds_named': {str(k): nm(k) for k in named},
        'pairs_that_work': [dict(r, first_name=nm(r['first']), second_name=nm(r['second']),
                                 makes=[dict(m, name=nm(m['kind'])) for m in r['makes']]) for r in good],
        'pairs_that_fail_words': sorted(set(tuple(w) for r in rows if not (r['makes'] or r['removes_both']) for w in r['words'])),
    }

# Where each kind a piece script names comes from. Filled after PLACED is read; empty means no literal ed3 and no table 3 or 5 placing.
# Which template each weapon or ammo kind has: ed3(def, model, -1, kind, ...) literals in scripts, and any save that has one.
KIND_TEMPLATE = {}
for sid, v in SCRIPTS.items():
    for t in v.values():
        for d, k in re.findall(r'ed3\((-?\d+), -?\d+, -?\d+, (-?\d+)', t):
            d, k = int(d), int(k)
            if d in DEFS and 0 <= k < len(KINDS) and 0 <= DEFS[d] < len(TEMPLATES):
                KIND_TEMPLATE.setdefault(k, {}).setdefault(DEFS[d], []).append('script %d def %d' % (sid, d))

# Things placed by the disc: NpcsN.npc starts with 0x2C and five {count, bytes}. Table 3 (44 byte records, loader 0x80041AFC)
# has room +6, kind +30, def +32. Table 5 (60 byte records, loader 0x80041ED4) has room +6 and three {model, def, flags, kind}
# sets at +32, +40, +48. Tables 1, 2 and 4 are NOT read here, so "no placement" only speaks for tables 3 and 5.
PLACED = {}
for path in D.glob(r'^Npcs\d+\.npc$'):
    b = D.read(path)
    head = struct.unpack('>11I', b[:44])
    at, tabs = 44, {}
    for i in range(5):
        tabs[i + 1] = (at, head[1 + i * 2], head[2 + i * 2])
        at += head[2 + i * 2]
    at3, n3, size3 = tabs[3]
    at5, n5, size5 = tabs[5]
    assert size3 == n3 * 44 and size5 == n5 * 60, path
    found = []
    for i in range(n3):
        r = b[at3 + i * 44:at3 + (i + 1) * 44]
        room = struct.unpack('>h', r[6:8])[0]
        kind, d = struct.unpack('>HH', r[30:34])
        found.append((kind, d, room, 3))
    for i in range(n5):
        r = b[at5 + i * 60:at5 + (i + 1) * 60]
        room = struct.unpack('>h', r[6:8])[0]
        for o in (32, 40, 48):
            _, d, _, kind = struct.unpack('>4H', r[o:o + 8])
            found.append((kind, d, room, 5))
    for kind, d, room, table in found:
        if kind < len(KINDS):
            PLACED.setdefault(kind, []).append({'file': path, 'table': table, 'room': room, 'def': d})
            if d in DEFS and 0 <= DEFS[d] < len(TEMPLATES):
                KIND_TEMPLATE.setdefault(kind, {}).setdefault(DEFS[d], []).append('%s table %d def %d' % (path, table, d))

try:
    import savestate
    for f in sorted(glob.glob(os.path.join(ROOT, 'saves', '*.state'))) + sorted(glob.glob(os.path.join(ROOT, 'build', 'runs', '*.state'))):
        s = savestate.State(f)
        head = s.sda(-21468)
        if not 0x80000000 <= head < 0x81800000:  # the title screen has no thing list
            continue
        n, c = s.u32(head + 4), 0
        while 0x80000000 <= n < 0x81800000 and c < 4096:
            o = s.u32(n + 44)
            if 0x80000000 <= o < 0x81800000 and 0x80000000 <= s.u32(o + 4) < 0x81800000:
                k, t = s.u32(o + 12), s.u32(o + 8)
                if k < len(KINDS) and t < len(TEMPLATES):
                    KIND_TEMPLATE.setdefault(k, {}).setdefault(t, []).append('save ' + os.path.basename(f))
            n, c = s.u32(n + 4), c + 1
except Exception as e:  # the saves are an extra, the disc is the answer
    print('saves not read:', e)

WEAPONS, AMMO = [], []
for k in KINDS:
    if k['text_group'] not in (3, 4):
        continue
    tmpl = KIND_TEMPLATE.get(k['kind'], {})
    row = {'kind': k['kind'], 'name': k['name'], 'mix_script': k['mix_script'], 'count': k['count'], 'count_max': k['count_max'], 'mode': k['mode'],
           'templates': [dict(TEMPLATES[t], seen_by=sorted(set(w))[:3]) for t, w in sorted(tmpl.items())]}
    (WEAPONS if k['text_group'] == 3 else AMMO).append(row)

for sid, p in PIECES.items():
    p['kind_sources'] = {str(k): {'ed3_literal': sorted(SPAWNS.get(k, [])), 'npcs_tables_3_5': PLACED.get(k, [])[:8]} for k in map(int, p['kinds_named'])}

classes = {}
for side, rows in (('weapons', WEAPONS), ('ammo', AMMO)):
    for r in rows:
        for t in r['templates']:
            if t['ammo_class']:
                classes.setdefault('0x%X' % t['ammo_class'], {'weapons': [], 'ammo': []})[side].append([r['kind'], r['name']])

json.dump({
    'how': 'Mix runs record +42 of the FIRST picked item (stored in r13-24536 on A at the menu for ANY action, 0x80104340/0x8010436C, run at 0x801042A0). There is no table: piece scripts compare fn92 kinds, ammo scripts call ed140, which compares template classes.',
    'ids_warning': 'kind is fixed; an id (handle) is good for one play only. Labels are not unique to a kind: key on the kind.',
    'mix_scripts_on_item_records': MIX_SCRIPTS,
    'ammo_scripts': AMMO_SCRIPTS,
    'scripts_that_read_a_second_item': TWO_READS,
    'pieces': {str(k): v for k, v in PIECES.items()},
    'ammo_by_class': classes,
    'ammo_codes_from_ed140': {'1': 'ok', '2': 'boot 0,31 MIX_ERR_NO_WEAPON_OR_AMMO', '3': 'boot 0,32 MIX_ERR_WEAPON_NOT_EQUIPPED', '4': 'boot 0,33 MIX_ERR_AMMO_NO_MATCH',
                              '5': 'boot 0,34 MIX_ERR_WEAPON_FULLY_LOADED', '6': 'boot 0,35 MIX_ERR_NOT_RIGHT_MODE', '-1': 'an id with no node: ed130 shows nothing'},
    'weapons': WEAPONS,
    'ammo': AMMO,
    'hands_sets_InvE': HANDS,
    'templates_InvS': TEMPLATES,
    'kinds_InvU': KINDS,
}, open(os.path.join(ROOT, 'build', 'ed-db', 'facts', 'mix_table.json'), 'w'), indent=1)

for sid, p in PIECES.items():
    print(sid, 'levels', ','.join(p['levels']), 'mix action:', p['reachable_by_mix_action'], p['kinds_with_this_as_mix_script'])
    for r in p['pairs_that_work']:
        print('   ', r['first'], r['first_name'], '+', r['second'], r['second_name'], '->', [(m['kind'], m['name']) for m in r['makes']], 'removes both' if r['removes_both'] else '')
    print('    sources:', {k: (v['ed3_literal'][:3], [(x['file'], x['table'], x['room']) for x in v['npcs_tables_3_5'][:2]]) for k, v in p['kind_sources'].items()})
print('ammo scripts', AMMO_SCRIPTS)
for c, v in sorted(classes.items()):
    print(c, 'weapons', v['weapons'], 'ammo', v['ammo'])
two = {r['kind']: sorted(set(x['ammo_class'] for x in r['templates'])) for r in WEAPONS + AMMO if len(set(x['ammo_class'] for x in r['templates'])) > 1}
print('kinds with two classes:', two)
print('no template known:', [(r['kind'], r['name']) for r in WEAPONS + AMMO if not r['templates']])
