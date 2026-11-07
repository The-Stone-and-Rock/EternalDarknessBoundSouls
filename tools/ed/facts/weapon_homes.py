"""Each weapon's home character, and each home's skeleton, from the disc: what the finisher (FINISH HIM, move 65)
plays with a weapon in another character's hands (runtime/host/hle.c, fin_home_*).

Run from anywhere: python -I tools/ed/facts/weapon_homes.py [--c]   (--c: also print the C tables for hle.c)

How each entry is found (nothing hand typed):
  weapons        InvU.bin rows (item_table.build) whose +54 names a seven of parts (the weapon's moves, 0x800CBE7C).
  where it is    Npcs<level>.npc placements (item_table.read_placed), else the decompiled scripts that give it
                 (ed3 spawns, build/ed-db/scripts/L<level>/<script>.lua). Placed levels win over given ones; among
                 several, the lowest level (the earliest chapter).
  whose level    0x80007C8C(chapter): the chapter's playable, a jump table of `li r3, N; blr` read from main.dol.
  move 65        /Chars/char<home>.cpk: the parts list (first 1 KB, +16 count, +20 where: 16 bytes each, +8 the id),
                 each part's moves at its +24 (+20 how many), 296 bytes each, the move's number at +6 (0x8012484C).
  skeleton       the home's body, in their first chapter's U_LevelNN.bin (Anthony: /U_Tony0.bin, his whole one), the
                 body list's entry by model number (as the host borrows bodies, borrow_load); each bone 80 bytes from
                 the body's +36, its name's hash at +64, the count at +8. A move's tracks go by these bone numbers.
Levels are unpacked with the game's own decompressor (ppcemu.Unpacker, about a minute each, in parallel) and kept
in build/ed-db/unpacked/ (derived from the disc: never committed).
"""
import concurrent.futures
import glob
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'ed'))
sys.path.insert(0, HERE)
import disc  # noqa: E402

DB = os.path.join(ROOT, 'build', 'ed-db')
CACHE = os.path.join(DB, 'unpacked')
CHAPTER_CHARACTER = 0x80007C8C
FINISH = 65
NAMES = {9: 'Alex', 0: 'Pius', 70: 'Ellia', 85: 'Anthony', 150: 'Karim', 18: 'Max', 73: 'Lindsey', 74: 'Paul',
         76: 'Roberto', 69: 'Edward', 75: 'Peter', 81: 'Michael'}  # labels for the print only


def chapter_models(dol):
    """0x80007C8C: cmplwi r3,16; bgt default; lis r4,H; slwi; addi r3,r4,L; lwzx; mtctr; bctr. Each case li r3,N."""
    w = dol.word
    assert w(CHAPTER_CHARACTER) == 0x28030010, 'not the chapter character function'
    hi, lo = w(CHAPTER_CHARACTER + 8) & 0xFFFF, w(CHAPTER_CHARACTER + 16) & 0xFFFF
    table = ((hi << 16) + (lo - 0x10000 if lo & 0x8000 else lo)) & 0xFFFFFFFF
    out = []
    for c in range(17):
        case = w(table + c * 4)
        li = w(case)
        assert li >> 16 == 0x3860, 'case %d is not li r3' % c
        out.append(li & 0xFFFF)
    return out, table


def file_moves(d, model):
    """part id -> set of move numbers, from the character's file."""
    try:
        b = d.read('Chars/char%04d.cpk' % model)
    except KeyError:
        return {}
    n, at = struct.unpack('>II', b[16:24])
    out = {}
    for i in range(n):
        off, size, pid = struct.unpack('>IIH', b[at + i * 16:at + i * 16 + 10])
        p = b[off:off + size]
        count, where = struct.unpack('>H', p[20:22])[0], struct.unpack('>I', p[24:28])[0]
        out[pid] = {struct.unpack('>H', p[where + k * 296 + 6:where + k * 296 + 8])[0]: struct.unpack('>H', p[where + k * 296 + 4:where + k * 296 + 6])[0]
                    for k in range(count)}  # move number -> its track count (+4)
    return out


def _unpack(job):
    name, out = job
    import ppcemu
    d = disc.Disc()
    u = ppcemu.Unpacker(os.path.join(DB, 'disasm.txt'))
    x = u.unpack(d.read(name))
    open(out + '.part', 'wb').write(x)
    os.replace(out + '.part', out)
    return name, len(x)


def unpacked(names):
    os.makedirs(CACHE, exist_ok=True)
    jobs = [(n, os.path.join(CACHE, os.path.basename(n) + '.raw')) for n in names]
    todo = [j for j in jobs if not os.path.exists(j[1])]
    if todo:
        print('unpacking', len(todo), 'files with the game\'s decompressor (about a minute each) ...', flush=True)
        with concurrent.futures.ProcessPoolExecutor(max_workers=min(len(todo), 12)) as ex:
            for name, size in ex.map(_unpack, todo):
                print('  ', name, size, 'bytes', flush=True)
    return {n: open(o, 'rb').read() for n, o in jobs}


def body_list(x, is_pack):
    """The list of bodies: a level's pack's first entry (+4 the pack, its entry 0); Anthony's file is the list."""
    if is_pack:
        return 4 + struct.unpack('>I', x[12:16])[0]
    return 4


def bones_of(x, lst, model):
    count, magic = struct.unpack('>II', x[lst:lst + 8])
    if magic != 0x6B5 or model >= count:
        return None
    off, size = struct.unpack('>II', x[lst + 8 + model * 8:lst + 16 + model * 8])
    if off == 0 or size == 0:
        return None
    a = lst + off
    n = struct.unpack('>H', x[a + 8:a + 10])[0]
    bl = struct.unpack('>I', x[a + 36:a + 40])[0]
    bl = a + bl if bl < 0x80000000 else None
    if bl is None or n == 0 or n > 127:
        return None
    return [struct.unpack('>I', x[bl + i * 80 + 64:bl + i * 80 + 68])[0] for i in range(n)]


def bone_turns(x, lst, model, with_world=False):
    """Each bone's turn from its parent as made (a quaternion x, y, z, w, as the game's rotation keys: s16 / 16384).
    A bone's +12 matrix takes the model into the bone (its rows are the bone's axes); its turn from the parent is
    R(bone) R(parent)^T, the root's R(bone). The rows are made unit first (some dummy bones are scaled)."""
    count, magic = struct.unpack('>II', x[lst:lst + 8])
    if magic != 0x6B5 or model >= count:
        return None
    off = struct.unpack('>I', x[lst + 8 + model * 8:lst + 12 + model * 8])[0]
    a = lst + off
    n = struct.unpack('>H', x[a + 8:a + 10])[0]
    bl = a + struct.unpack('>I', x[a + 36:a + 40])[0]
    rows, parents = [], []
    for i in range(n):
        m = struct.unpack('>12f', x[bl + i * 80 + 12:bl + i * 80 + 60])
        R = []
        for r in range(3):
            v = [m[r * 4 + c] for c in range(3)]
            s = math.sqrt(sum(c * c for c in v)) or 1.0
            R.append([c / s for c in v])
        rows.append(R)
        parents.append(struct.unpack('>H', x[bl + i * 80 + 76:bl + i * 80 + 78])[0])
    out = []
    for i in range(n):
        R = rows[i]
        if parents[i] < n:
            P = rows[parents[i]]
            R = [[sum(R[r][k] * P[c][k] for k in range(3)) for c in range(3)] for r in range(3)]
        out.append(quat(R))
    if with_world:
        return out, [quat(rows[i]) for i in range(n)], parents
    return out


def quat(R):
    t = R[0][0] + R[1][1] + R[2][2]
    if t > 0:
        s = math.sqrt(t + 1.0) * 2
        q = [(R[2][1] - R[1][2]) / s, (R[0][2] - R[2][0]) / s, (R[1][0] - R[0][1]) / s, 0.25 * s]
    elif R[0][0] > R[1][1] and R[0][0] > R[2][2]:
        s = math.sqrt(1.0 + R[0][0] - R[1][1] - R[2][2]) * 2
        q = [0.25 * s, (R[0][1] + R[1][0]) / s, (R[0][2] + R[2][0]) / s, (R[2][1] - R[1][2]) / s]
    elif R[1][1] > R[2][2]:
        s = math.sqrt(1.0 + R[1][1] - R[0][0] - R[2][2]) * 2
        q = [(R[0][1] + R[1][0]) / s, 0.25 * s, (R[1][2] + R[2][1]) / s, (R[0][2] - R[2][0]) / s]
    else:
        s = math.sqrt(1.0 + R[2][2] - R[0][0] - R[1][1]) * 2
        q = [(R[0][2] + R[2][0]) / s, (R[1][2] + R[2][1]) / s, 0.25 * s, (R[1][0] - R[0][1]) / s]
    n = math.sqrt(sum(c * c for c in q)) or 1.0
    return [max(-32767, min(32767, int(round(c / n * 16384)))) for c in q]


def script_levels():
    out = {}
    for path in glob.glob(os.path.join(DB, 'scripts', 'L*', '*.lua')):
        lv = os.path.basename(os.path.dirname(path))[1:]
        if lv.lstrip('-').isdigit() and int(lv) >= 0:
            out.setdefault(int(os.path.basename(path)[:-4]), set()).add(int(lv))
    return out


def main(argv):
    import contextlib
    import io
    import item_table
    d, dol = disc.Disc(), disc.Dol()
    models, table = chapter_models(dol)
    print('0x80007C8C chapter -> playable (jump table 0x%08X):' % table, ' '.join('%d:%d' % (c, m) for c, m in enumerate(models)))
    with contextlib.redirect_stdout(io.StringIO()):
        rows = item_table.build()['rows']
    levels_of = script_levels()
    weapons = []
    for r in rows:
        seven = r['record']['shorts54_66']
        if not seven or seven[0] <= 0 or r['kind'] != 'WEAPONS':
            continue
        placed = sorted({int(p['file'][4:-4]) for p in r['placed']})
        given = sorted({lv for s in r['spawned_by'] for lv in levels_of.get(s['script'], ())})
        where, how = (placed, 'placed in Npcs%s.npc' % ','.join(map(str, placed))) if placed else \
                     (given, 'given by script %s in level %s' % (','.join(str(s['script']) for s in r['spawned_by']), ','.join(map(str, given)))) if given else ([], 'nowhere')
        home = models[where[0]] if where and where[0] < len(models) else None
        weapons.append({'id': r['id'], 'name': r['name'], 'seven': seven[0], 'home': home, 'level': where[0] if where else None, 'how': how})
    homes = sorted({w['home'] for w in weapons if w['home'] is not None})
    moves = {m: file_moves(d, m) for m in sorted(set(homes) | set(models[:12]))}
    print('\nweapon -> home (the character of the first level that has it), and the home file\'s parts with move 65 in the weapon\'s seven:')
    for w in weapons:
        h = w['home']
        have = [p for p in range(w['seven'], w['seven'] + 7) if FINISH in moves.get(h, {}).get(p, ())] if h is not None else []
        w['has65'] = have
        print('  %3d %-18s seven %3d  home %-4s %-8s level %-4s %s; move 65 in parts %s' % (
            w['id'], w['name'], w['seven'], h if h is not None else '-', NAMES.get(h, ''), w['level'] if w['level'] is not None else '-',
            w['how'], have or 'none'))
    # Skeletons: the first chapter (0 to 11) that is theirs.
    sources = {}
    for m in sorted(set(homes) | set(m for m in models[:12])):
        if m == 85:
            sources[m] = ('U_Tony0.bin', False)
            continue
        c = next(c for c in range(12) if models[c] == m)
        sources[m] = ('Levels/Level%02d/U_Level%02d.bin' % (c, c), True)
    raws = unpacked(sorted({s[0] for s in sources.values()}))
    skel, turns, worlds, parents_of = {}, {}, {}, {}
    print('\nskeletons (bone count, from):')
    for m, (name, is_pack) in sorted(sources.items()):
        x = raws[name]
        b = bones_of(x, body_list(x, is_pack), m)
        skel[m] = b
        turns[m], worlds[m], parents_of[m] = bone_turns(x, body_list(x, is_pack), m, True) if b else (None, None, None)
        tracks = sorted({t for part in moves.get(m, {}).values() for i, t in part.items() if i == FINISH})
        print('  %3d %-8s %s bones, %s; move 65 has %s tracks (a bone each and the root)' % (
            m, NAMES.get(m, ''), len(b) if b else 'NO', name, tracks or '-'))
    if '--c' in argv:
        print('\n// --- C tables (tools/ed/facts/weapon_homes.py --c) ---')
        print('static const struct { u16 item, home; } kFinHome[] = {')
        print('    ' + ', '.join('{%d, %d}' % (w['id'], w['home']) for w in weapons if w['home'] is not None and w['has65']) + '};')
        for m in sorted(skel):
            if skel[m]:
                print('static const u32 kFinBones%d[%d] = {' % (m, len(skel[m])))
                for i in range(0, len(skel[m]), 8):
                    print('    ' + ', '.join('0x%08Xu' % v for v in skel[m][i:i + 8]) + ',')
                print('};')
        print('static const struct { int model; u32 count; const u32* names; } kFinSkeletons[] = {')
        print('    ' + ', '.join('{%d, %d, kFinBones%d}' % (m, len(skel[m]), m) for m in sorted(skel) if skel[m]) + '};')
    if '--turns' in argv:
        # Each bone's turn from its parent as made (bone_turns), for wpn_bind: a name match whose turns differ a lot
        # (Ellia's side braid against Alex's ponytail, 54-61 degrees) isn't played from the home's track.
        print('\n// --- C tables (tools/ed/facts/weapon_homes.py --turns) ---')
        for m in sorted(turns):
            if turns[m]:
                print('static const s16 kFinTurns%d[%d][4] = {' % (m, len(turns[m])))
                for i in range(0, len(turns[m]), 6):
                    print('    ' + ', '.join('{%d, %d, %d, %d}' % tuple(q) for q in turns[m][i:i + 6]) + ',')
                print('};')
        # Each bone's place as made, turned (the +12 matrix's rows made unit, as a quaternion) and its parent: what a
        # reload's move from the gun's home is turned by to sit on another's bones (fin_fit).
        for m in sorted(turns):
            if turns[m]:
                print('static const s16 kFinWorld%d[%d][4] = {' % (m, len(worlds[m])))
                for i in range(0, len(worlds[m]), 6):
                    print('    ' + ', '.join('{%d, %d, %d, %d}' % tuple(q) for q in worlds[m][i:i + 6]) + ',')
                print('};')
                print('static const u16 kFinParent%d[%d] = {%s};' % (m, len(parents_of[m]), ', '.join(str(v) for v in parents_of[m])))
        print('static const struct { int model; u32 count; const s16 (*turns)[4]; const s16 (*world)[4]; const u16* parent; } kFinTurnsOf[] = {')
        print('    ' + ', '.join('{%d, %d, kFinTurns%d, kFinWorld%d, kFinParent%d}' % (m, len(turns[m]), m, m, m) for m in sorted(turns) if turns[m]) + '};')
        # The name matches whose turns differ by more than 45 degrees, for each pair the user plays (a check).
        def angle(a, b):
            d = abs(sum(a[k] * b[k] for k in range(4))) / (16384.0 * 16384.0)
            return math.degrees(2 * math.acos(min(1.0, d)))
        for holder, home in ((70, 9), (70, 18), (85, 75), (74, 75), (0, 18), (75, 150)):
            if not (skel.get(holder) and skel.get(home)):
                continue
            far = []
            for j, name in enumerate(skel[holder]):
                if name in skel[home]:
                    k = skel[home].index(name)
                    a = angle(turns[holder][j], turns[home][k])
                    if a > 45:
                        far.append('%d->%d %.0f' % (j, k, a))
            print('// %s with %s\'s moves: name matches turned over 45 degrees: %s' % (
                NAMES.get(holder), NAMES.get(home), ', '.join(far) or 'none'))


if __name__ == '__main__':
    main(sys.argv[1:])
