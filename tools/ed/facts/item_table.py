"""The game's item table: item type id -> label, words, scripts and every other field, from the disc only.
Run from anywhere: python -I tools/ed/facts/item_table.py   (writes item_table.json beside this file, prints checks)

Sources (nothing is hand typed):
  InvU.bin          pack of 244: entry N is item type N. {1, 0xFB90, 16, 88} then the 88-byte record the game copies
                    (0x8015062C, called by 0x801506C8 with the type in r4; the type is kept at info+12).
  UBootPak.bin      section 4: the text pack the game keeps at r13-26492. A record's +76 is the group, +77/+78/+79 entries.
                    English only. 0x800218EC to 0x80021A00 load UBootPkF/G/I/S by language: same 749 keys and labels, other words.
  InvS.bin          pack of 55 kinds (equip behaviour). A thing's kind is its ed_chrs.dat record's +136 (0x80031D80).
  ai/ed_chrs.dat    32-byte header, then 224-byte records: +4 id, +64 class (4 is a pickup, 0x80031C50), +136 InvS kind.
  Npcs*.npc         the placed things: table 3 (44-byte records, loader 0x80041AFC) and table 5 (60-byte, 0x80041ED4).
  build/ed-db/scripts  scripts decompiled from the disc: ed3 spawns, fn92 tests and Mix scripts (kit build step).
Saves are only read, for the checks, and a missing one is skipped.
"""
import glob
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'ed'))
import disc  # noqa: E402
import gametext  # noqa: E402
import savestate  # noqa: E402

OUT = os.path.join(ROOT, 'build', 'ed-db', 'facts', 'item_table.json')
OICW_MODE = 16  # fire_mode 16: 0x80150A70 to 0x80150A80
DB = os.path.join(ROOT, 'build', 'ed-db')
NONE16 = 0xFFFF


def pack_entries(blob):
    """A 0x6B5 pack: [(offset, size)]."""
    count, magic = struct.unpack('>II', blob[:8])
    if magic != 0x6B5:
        raise ValueError('not a pack')
    return [struct.unpack('>II', blob[8 + i * 8:16 + i * 8]) for i in range(count)]


def sub0(blob, off):
    """Part 0 of a pack entry (0xFB90): what 0x801DD03C(pack, i, 0) returns."""
    n, magic, o, s = struct.unpack('>IIII', blob[off:off + 16])
    if magic != 0xFB90:
        raise ValueError('not a 0xFB90 entry at %#x' % off)
    return blob[off + o:off + o + s], o, s


def read_record(r):
    """The 88 bytes of InvU.bin, every field. Names are only where the code was read: see item-ids.md."""
    u32 = lambda a: struct.unpack_from('>I', r, a)[0]
    s16 = lambda a: struct.unpack_from('>h', r, a)[0]
    u16 = lambda a: struct.unpack_from('>H', r, a)[0]
    return {
        'fire_mode': u32(0),            # 0x80150814, set 0x80150A2C. Tome action 4 (Mode) cycles it through InvS +20. 16: second ammo pool
        'flags': u32(4),                # 0x801507FC; 0x20: the Use script runs after Examine (0x80104170)
        'word8': u32(8),
        'rotation_12': [round(struct.unpack_from('>f', r, a)[0], 5) for a in (12, 16, 20, 24)],
        'word28': u32(28),              # 0x8015088C, set 0x80150898
        'script_after_find': s16(36),   # 0x80150868, run by 0x800D9B64
        'script_use': s16(38),          # 0x80150850, Tome action 0
        'script_after_examine': s16(40),  # 0x8015085C, read at 0x80104114
        'script_mix': s16(42),          # 0x80150874, Tome action 3
        'short44_46_48': [s16(44), s16(46), s16(48)],  # 0x80150B74
        'tome_50': s16(50),             # 0x801507B4
        'tome_52': s16(52),             # 0x801507C0
        'shorts54_66': [s16(a) for a in range(54, 68, 2)],
        'count': u16(68),               # 0x801508FC when fire_mode isn't 16
        'capacity': u16(70),            # 0x801508B0 when fire_mode isn't 16
        'capacity_mode16': u16(72),     # 0x801508B0 when fire_mode is 16
        'count_mode16': r[74],          # 0x801508FC when fire_mode is 16 (a byte: lbz at 0x80150930)
        'byte75': r[75],                # 0x801509E8
        'text_group': r[76],            # 0x80150B5C
        'text_pickup': r[77],           # 0x80150B2C, set 0x80150AFC
        'text_description': r[78],      # 0x80150B38, set 0x80150B20
        'text_name': r[79],             # 0x80150B44, set 0x80150B14
        'text_enchanted': r[80],        # 0x80150B50: entry in group 0, 0x801C6FB8
        'byte81': r[81],                # 0x80150A20, set 0x80150AC8
        'byte82': r[82],                # 0x80150880, set 0x80150AA4
        'byte83': r[83],                # 0x80150964
        'byte84': r[84],                # 0x801508A4
        'byte85': r[85],                # 0x80150B68
        'bytes86_87': r[86:88].hex(),
    }


def read_chrs(d):
    b = d.read('ai/ed_chrs.dat')
    out = {}
    for i in range((len(b) - 32) // 224):
        r = b[32 + i * 224:32 + (i + 1) * 224]
        out[struct.unpack('>i', r[4:8])[0]] = {
            'class': struct.unpack('>h', r[64:66])[0],
            'subclass': struct.unpack('>h', r[66:68])[0],
            'inv_kind': struct.unpack('>h', r[136:138])[0],
        }
    return out


def read_inv_kinds(d):
    b = d.read('InvS.bin')
    out = []
    for off, _ in pack_entries(b):
        p, _, _ = sub0(b, off)
        w = struct.unpack('>9I', p[:36])
        out.append({
            'inv_e': w[0],            # +0: turned into a pointer by 0x801504DC
            'word16': w[4],           # 0x801507F0
            'fire_modes': w[5],       # 0x80150808: the fire_mode bits Tome action 4 can cycle to (0x8010461C). Non-zero on 4 guns only
            'word24': w[6],
            'mask': w[7] >> 16,       # 0x8015119C (u16 at +28): ed140 needs weapon and ammo to share a bit. The disc's value:
                                      # 0x80150A2C rewrites it in memory when the mode changes, see OICW_MODE
            'low28': w[7] & 0xFFFF,
            'byte32': w[8] >> 24,     # 0x80151178
            'rest32': w[8] & 0xFFFFFF,
        })
    return out


def read_placed(d, chrs):
    """type id -> where the area files put it."""
    placed = {}

    def add(t, row, chr_id):
        if chr_id in chrs and chrs[chr_id]['class'] == 4 and 0 <= t < 0x7FFF:
            placed.setdefault(t, []).append(row)

    for path in d.glob(r'^Npcs\d+\.npc$'):
        b = d.read(path)
        head = struct.unpack('>11I', b[:44])
        at = 44
        tables = {}
        for t in range(5):
            tables[t + 1] = (at, head[1 + t * 2], head[2 + t * 2])
            at += head[2 + t * 2]
        at, count, size = tables[3]
        for i in range(count):
            r = b[at + i * 44:at + (i + 1) * 44]
            x, y, z, room = struct.unpack('>4h', r[:8])
            model, typ, chr_id = struct.unpack('>HHH', r[28:34])
            tag = struct.unpack('>H', r[40:42])[0]
            add(typ, {'file': path, 'table': 3, 'index': i, 'room': room, 'pos': [x, y, z], 'chr': chr_id, 'model': model,
                      'mask': r[11], 'tag': None if tag == NONE16 else tag, 'flags': struct.unpack('>H', r[38:40])[0]},
                chr_id)
        at, count, size = tables[5]
        for i in range(count):
            r = b[at + i * 60:at + (i + 1) * 60]
            x, y, z, room = struct.unpack('>4h', r[:8])
            tag = struct.unpack('>H', r[56:58])[0]
            for k, which in ((32, 1), (40, 2), (48, 3)):  # by the word r13-26320 points at, 0x80041F88: 1, 2 or 3 (0, or 4 and up, picks none)
                model, chr_id, flags, typ = struct.unpack('>4H', r[k:k + 8])
                add(typ, {'file': path, 'table': 5, 'index': i, 'room': room, 'pos': [x, y, z], 'chr': chr_id, 'model': model,
                          'mask': r[11], 'tag': None if tag == NONE16 else tag, 'flags': flags, 'set': which}, chr_id)
    return placed


ED3 = re.compile(r'ed3\((-?\d+), (-?\d+), (-?\d+), (-?\d+), (-?\d+), (-?\d+), (-?\d+),')
FN92 = re.compile(r'local L(\d+) = fn92\(')
FN92B = re.compile(r'^L(\d+) = fn92\(')
CMP = re.compile(r'if L(\d+) (?:~=|==) (\d+) then')


def read_scripts(chrs):
    """From the decompiled scripts: ed3 with a literal type, and types a script tests with fn92."""
    spawns, tests, by_id = {}, {}, {}
    for path in glob.glob(os.path.join(DB, 'scripts', 'L*', '*.lua')):
        level = os.path.basename(os.path.dirname(path))[1:]
        sid = int(os.path.basename(path)[:-4])
        text = open(path, encoding='utf8', errors='replace').read()
        for m in ED3.finditer(text):
            chr_id, model, room, typ, _, _, tag = (int(x) for x in m.groups())
            if chr_id in chrs and chrs[chr_id]['class'] == 4 and typ >= 0:
                spawns.setdefault(typ, set()).add((sid, chr_id, model, room, tag))
        if 'fn92(' in text:
            locals_ = {int(x) for x in FN92.findall(text)}
            found = set()
            for line in text.splitlines():
                m = FN92B.match(line)
                if m:
                    locals_.add(int(m.group(1)))
                m = CMP.match(line)
                if m and int(m.group(1)) in locals_:
                    found.add(int(m.group(2)))
            by_id.setdefault(sid, set()).update(found)
            for t in found:
                tests.setdefault(t, set()).add(sid)
    return spawns, tests, by_id


def read_code_tests():
    """Where the game's code asks 0x80151184 (a thing's type) and compares it with a number."""
    tests = {}
    path = os.path.join(DB, 'disasm.txt')
    if not os.path.exists(path):
        return tests
    lines = open(path).read().splitlines()
    for i, line in enumerate(lines):
        if not line.endswith('bl      0x80151184'):
            continue
        at = i + 1
        while at < len(lines):
            m = re.match(r'(80[0-9A-F]{6}): cmpwi\s+r3, (-?\d+)', lines[at])
            if m:
                tests.setdefault(int(m.group(2)), []).append(line[:8])
                at += 1
            elif re.match(r'80[0-9A-F]{6}: bc\s', lines[at]):
                at += 1
            else:
                break
    return tests


def build():
    d = disc.Disc()
    u = d.read('InvU.bin')
    boot = d.read('UBootPak.bin')
    o, s = gametext.pack_table(boot)[4]
    text = gametext.read_groups(boot[o:o + s])
    chrs = read_chrs(d)
    kinds = read_inv_kinds(d)
    placed = read_placed(d, chrs)
    spawns, script_tests, mix_tests = read_scripts(chrs)
    code_tests = read_code_tests()

    def words(g, e):
        return text.get((g, e), ('', '', ''))

    rows = []
    used = set()
    for tid, (off, size) in enumerate(pack_entries(u)):
        r, part_off, part_size = sub0(u, off)
        f = read_record(r)
        g = f['text_group']
        name, desc, pick = words(g, f['text_name']), words(g, f['text_description']), words(g, f['text_pickup'])
        used.update((g, e) for e in (f['text_name'], f['text_description'], f['text_pickup']))
        base = re.sub(r'_(INV|DESC|PICKUP)$', '', name[1])
        odd = []
        if not name[1].endswith('_INV'):
            odd.append('name entry is ' + name[1])
        if not desc[1].endswith('_DESC'):
            odd.append('description entry is ' + desc[1])
        if not pick[1].endswith('_PICKUP'):
            odd.append('pickup entry is ' + pick[1])
        where = placed.get(tid, [])
        made = sorted(spawns.get(tid, ()))
        inv = sorted({chrs[w['chr']]['inv_kind'] for w in where} | {chrs[m[1]]['inv_kind'] for m in made})
        row = {
            'id': tid,
            'label': base,
            'name': name[2],
            'description': desc[2],
            'pickup_words': pick[2],
            'kind': name[0],
            'text': {'group': g, 'group_label': name[0], 'pickup': f['text_pickup'], 'description': f['text_description'],
                     'name': f['text_name'], 'labels': [pick[1], desc[1], name[1]]},
            'placeholder': f['text_pickup'] == f['text_description'] == f['text_name'],
            'label_notes': odd,
            'use_script': f['script_use'] or None,
            'after_find_script': f['script_after_find'] or None,
            'after_examine_script': f['script_after_examine'] or None,
            'mix_script': f['script_mix'] or None,
            'mix_script_tests_types': sorted(mix_tests.get(f['script_mix'], ())) if f['script_mix'] else [],
            'count': f['count'],
            'capacity': f['capacity'],
            'fire_mode': f['fire_mode'],
            'inv_kinds': inv,
            'reload_masks': [],
            'reload_partners': None,
            'tags': sorted({w['tag'] for w in where if w['tag'] is not None} | {m[4] for m in made if m[4] >= 0}),
            'placed': where,
            'spawned_by': [{'script': m[0], 'chr': m[1], 'model': m[2], 'room': m[3], 'tag': m[4]} for m in made],
            'scripts_testing_type': sorted(script_tests.get(tid, ())),
            'code_testing_type': sorted(code_tests.get(tid, ())),
            'record': f,
            'record_file_offset': off + part_off,
        }
        rows.append(row)

    # ed140 (0x800168AC): weapon (group 3) with ammo (group 4) when their masks share a bit. The mask is not fixed: 0x80150A2C
    # writes InvS +28 through 0x801511B8, 0x400 in fire mode 16 and 0x100 otherwise (if it has a 0x500 bit), so a gun that can
    # reach mode 16 has both. Partners are possible pairs only: ed140 also wants the weapon equipped and not full (ed140_results).
    # None: no kind known for the row (no placing or literal ed3 found), so no mask to compare. []: masks known, no partner.
    for a in rows:
        if a['text']['group'] in (3, 4) and a['inv_kinds']:
            mask = 0
            for k in a['inv_kinds']:
                mask |= kinds[k]['mask']
                if kinds[k]['fire_modes'] & OICW_MODE:
                    mask |= 0x400
            a['reload_masks'] = [1 << b for b in range(16) if mask >> b & 1]
    for a in rows:
        if a['text']['group'] in (3, 4) and a['inv_kinds']:
            want = 4 if a['text']['group'] == 3 else 3
            a['reload_partners'] = [b['id'] for b in rows if b['text']['group'] == want and set(b['reload_masks']) & set(a['reload_masks'])]

    unused_labels = sorted('%d,%d %s' % (k[0], k[1], v[1]) for k, v in text.items()
                           if v[1].endswith('_INV') and k not in used)
    return {
        'numbering': {
            'id': 'item type: InvU.bin entry, info+12, 0x80151184(info), fn92(handle) in scripts. The same in every chapter.',
            'handle': 'a thing in this play: node+16, carried list entry+8, Tome slot+40, 0x80335748 word 0. Changes with the chapter and the order things were made.',
            'tag': 'a script number: node+36 then +24, ed3 argument 7, Npcs record. fn78(2, tag), fn78(4, tag), fn78(5, tag) find a thing by it. Not unique: 481 is on the Scramasax and the Broken Blue Urn.',
            'chr': 'ed_chrs.dat record: ed3 argument 1, Npcs record. Its +136 is the inv_kind.',
        },
        'notes': [
            'Starting values: scripts and native code change a live record (fn162, ed168, 0x8006C514, 0x800B8E0C).',
            'Words are the English pack; the other language packs have the same group and entry numbers.',
            'fire_mode is the record +0 the Tome Mode action cycles. reload_partners: possible pairs only, see ed140_results.',
            'tags: only Npcs tables 3 and 5 and literal ed3 calls. Scripts also search tags from Npcs tables 1 (+34), 2 (+98) and 4 (+46).',
        ],
        'tome_actions': [{'action': a, 'label': words(14, a)[1], 'words': words(14, a)[2], 'code': c} for a, c in (
            (0, 'runs record +38 (0x80104330, 0x801043D0)'), (1, 'calls 0x800C9FD4'), (2, 'always offered; shows +78, then runs +40'),
            (3, 'runs +42 of the first item chosen (0x80104294 to 0x801042A0)'), (4, 'cycles +0 through InvS +20 (0x801045EC to 0x8010463C)'))],
        'ed140_results': {str(n): (words(0, e)[1] if e is not None else w) for n, e, w in (
            (-1, None, 'no such thing'), (1, None, 'can reload'), (2, 31, ''), (3, 32, ''), (4, 33, ''), (5, 34, ''), (6, 35, ''))},
        'count': len(rows),
        'rows': rows,
        'inv_kinds': kinds,
        'item_labels_with_no_row': unused_labels,
    }


# ---- checks: read only, a missing state is skipped


def carried(s, who=1):
    head = s.sda(-21468)
    nodes = {}
    n = s.u32(head + 4)
    while n and len(nodes) < 2000:
        nodes[s.u32(n + 16)] = n
        n = s.u32(n + 4)
    out = []
    lst = s.u32(nodes[who] + 48)
    e = s.u32(lst + 4)
    while e and len(out) < 64:
        h = s.u32(e + 8)
        n = nodes[h]
        info = s.u32(n + 44)
        stats = s.u32(n + 36)
        # A thing can have type -1 and no usable record (13 things in 12 states, record 0 or 0x81FF0000): 0x8015062C makes none.
        out.append({'handle': h, 'type': s.s32(info + 12), 'tag': s.s32(stats + 24), 'record': s.u32(info + 4), 'kind': s.s32(info + 8)})
        e = s.u32(e + 4)
    return out


def table_in_memory(s, rows, u):
    base = s.sda(-23412)
    bad = 0
    for i, (off, _) in enumerate(pack_entries(u)):
        p = s.u32(base + 8 + i * 8)
        if s.bytes(p + 16, 88) != u[off + 16:off + 104]:
            bad += 1
    return s.bytes(base, 8).hex(), bad


def checks(result):
    rows = result['rows']
    u = disc.Disc().read('InvU.bin')
    print('table: %d rows, %d placeholders, %d with a Use script, %d with a Mix script' % (
        len(rows), sum(r['placeholder'] for r in rows), sum(1 for r in rows if r['use_script']), sum(1 for r in rows if r['mix_script'])))
    for tid, name, use in ((21, 'Bronze Necklace', 1510), (31, 'Blue Urn', 1429), (150, 'Talisman', 1874), (24, 'Strange Necklace', 2243),
                           (84, 'Tulwar', None), (82, 'Chakrams', None), (30, 'Tome of Eternal Darkness', None)):
        r = rows[tid]
        ok = r['name'] == name and (use is None or r['use_script'] == use)
        print('  %-4s id %3d %-26s label %-14s use %s count %d' % ('ok' if ok else 'BAD', tid, r['name'], r['label'], r['use_script'], r['count']))
    sets = sorted({w['set'] for r in rows for w in r['placed'] if 'set' in w})
    print('  %-4s table 5 sets used: %s' % ('ok' if sets == [1, 2, 3] else 'BAD', sets))
    for tid, want in ((137, [135, 136]), (136, [137]), (135, [137]), (100, [88]), (84, [])):
        print('  %-4s id %3d %-20s reload_partners %s masks %s' % ('ok' if rows[tid]['reload_partners'] == want else 'BAD', tid, rows[tid]['name'], rows[tid]['reload_partners'], rows[tid]['reload_masks']))
    print('  group 3/4 rows with no known kind (reload_partners null): %s' % [r['id'] for r in rows if r['text']['group'] in (3, 4) and r['reload_partners'] is None])
    print('  fire modes on kinds: %s' % {i: hex(k['fire_modes']) for i, k in enumerate(result['inv_kinds']) if k['fire_modes']})
    print('  Tome actions: %s' % [a['label'] for a in result['tome_actions']])
    print('  ed140 results: %s' % result['ed140_results'])
    expected = [
        ('saves/slot1.state', {62: 31}),
        ('saves/slot3.state', {}),
        ('build/runs/s1.state', {9: 84, 10: 150, 11: 82}),
        ('logs/saved-changes/working-01b3d85-before-tome-copy/saves/slot10.state', {102: 21}),
    ]
    for path, want in expected:
        full = os.path.join(ROOT, path)
        if not os.path.exists(full):
            print('  (no %s)' % path)
            continue
        s = savestate.State(full)
        magic, bad = table_in_memory(s, rows, u)
        print(' %s: room %d, InvU in memory %s, %d of 244 records differ from the disc' % (path, s.sda(-23276), magic, bad))
        for c in carried(s):
            if not 0 <= c['type'] < len(rows):
                print('   handle %3d type %3d: no record' % (c['handle'], c['type']))
                continue
            r = rows[c['type']]
            live = s.bytes(c['record'], 88)
            file = u[r['record_file_offset']:r['record_file_offset'] + 88]
            diff = [i for i in range(88) if live[i] != file[i]]
            note = '' if c['handle'] not in want else (' KNOWN ok' if want[c['handle']] == c['type'] else ' KNOWN BAD')
            print('   handle %3d type %3d tag %5d kind %2d %-26s live bytes differing from table: %s%s' % (
                c['handle'], c['type'], c['tag'], c['kind'], r['name'], diff or 'none', note))


def main():
    result = build()
    with open(OUT, 'w', encoding='utf8') as f:
        json.dump(result, f, indent=1, ensure_ascii=False)
    print('wrote', OUT)
    checks(result)


if __name__ == '__main__':
    main()
