"""`ed herogates [--c] [--all]`: the hero gates in the game's scripts, found in their Lua 4.0 bytecode.

A hero gate keeps the player's handle (fn78(1, 0): kind 1 gives r13-21496, the current player's id) and tests it
against 1, the chapter's character, turning anyone else away. Run as player 2's, the handle is their id (161 in
Edward's chapter) and the script turns them away (Edward's lever L09/1951: `ed stops 010635-duck-together 1951`).

The host's fix (hle.c, ED_COOP_HERO_ID) rewrites the gate's own `PUSHINT 1` in the chunk the game is about to run, to
player 2's id in their runs and back to 1 in player 1's: only that compare changes; fn78, every native, the queues and
the r13-21496 / -21488 compares still see player 2's real id, as they do today. This command finds each gate's byte
offset in its chunk, from build/ed-db's script packs, and prints the host's table (--c).

A gate is GETLOCAL k (k holding fn78(1, 0)'s result, tracked straight through the code: an initialised local, or a
SETLOCAL right after the call; any other SETLOCAL k forgets it), then PUSHINT 1, then JMPNE / JMPEQ. It is kept
("turns away") when the jump on a mismatch goes to the end (END / RETURN, or a JMP to one): a compare with two paths
(both doing something) is listed with --all but left out of the table, for someone to decide on."""
import os
import struct

import disc
import lua40

DB = disc.DB


def _end_at(code, pc, depth=0):
    if not 0 <= pc < len(code) or depth > 4:
        return False
    name, u, s, _, _ = lua40.fields(code[pc])
    if name in ('END', 'RETURN'):
        return True
    if name == 'JMP':
        return _end_at(code, pc + 1 + s, depth + 1)
    return False


def gates_of(f, out, path=()):
    code = f.code
    holds = set()
    for pc, w in enumerate(code):
        name, u, s, b, a = lua40.fields(w)
        if name == 'GETGLOBAL' and f.kstr[u] == 'fn78' and pc + 3 < len(code):
            n1, _, s1, _, _ = lua40.fields(code[pc + 1])
            n2, _, s2, _, _ = lua40.fields(code[pc + 2])
            n3, u3, _, b3, a3 = lua40.fields(code[pc + 3])
            if n1 == 'PUSHINT' and s1 == 1 and n2 == 'PUSHINT' and s2 == 0 and n3 == 'CALL':
                slot = a3  # CALL A B: the function's stack position, where the result lands
                n4, u4, _, _, _ = lua40.fields(code[pc + 4]) if pc + 4 < len(code) else ('', 0, 0, 0, 0)
                holds.add(u4 if n4 == 'SETLOCAL' else slot)
        elif name == 'SETLOCAL' and pc > 0 and lua40.fields(code[pc - 1])[0] != 'CALL':
            holds.discard(u)
        elif name == 'GETLOCAL' and u in holds and pc + 2 < len(code):
            n1, _, s1, _, _ = lua40.fields(code[pc + 1])
            n2, _, s2, _, _ = lua40.fields(code[pc + 2])
            if n1 == 'PUSHINT' and s1 == 1 and n2 in ('JMPNE', 'JMPEQ'):
                # JMPNE jumps when they differ (the mismatch: someone else); JMPEQ jumps when it's the hero, so the
                # mismatch falls through to pc + 3.
                miss = pc + 3 + s2 if n2 == 'JMPNE' else pc + 3
                out.append(dict(path=path, pc=pc + 1, offset=f.code_at + 4 * (pc + 1), slot=u, jump=n2,
                                turns_away=_end_at(code, miss)))
    for i, p in enumerate(f.kproto):
        gates_of(p, out, path + (i,))
    return out


def all_gates():
    d = disc.Disc()
    rows = []
    for path in d.glob(r'^Levels/Level-?\d+/ScrLvl-?\d+\.gpk$'):
        level = os.path.basename(path)[6:-4]
        if level.startswith('-'):
            continue  # the boot pack: no rooms
        for sid, data in sorted(lua40.pack_entries(d.read(path)).items()):
            try:
                f = lua40.parse(data)
            except ValueError:
                continue
            for g in gates_of(f, []):
                word = struct.unpack_from('<I', data, g['offset'])[0]
                g.update(level=level, script=sid, word=word, size=len(data))
                rows.append(g)
    return rows


def cmd_herogates(argv):
    rows = all_gates()
    keep = [r for r in rows if r['turns_away'] and not r['path']]
    if '--c' in argv:
        print('// tools/ed/herogates.py (`ed herogates --c`): each hero gate\'s PUSHINT 1 in its script\'s chunk, by level and')
        print('// script; the byte offset from the chunk\'s start, the handle\'s local slot, and the jump. Regenerate after the disc or')
        print('// the kit\'s Lua reader changes.')
        print('static const struct { s8 level; s16 script; u16 offset; u8 slot; u8 jmpeq; } kHeroGates[] = {')
        for r in keep:
            print('    {%d, %d, 0x%X, %d, %d},' % (int(r['level']), r['script'], r['offset'], r['slot'], r['jump'] == 'JMPEQ'))
        print('};')
        return
    files = sorted({(r['level'], r['script']) for r in rows})
    print('%d hero-gate compares in %d script files; %d turn the player away (in the host\'s table), %d have two paths%s' % (
        len(rows), len(files), len(keep), sum(1 for r in rows if not r['turns_away']), '' if '--all' in argv else ' (--all lists them)'))
    for r in rows:
        if not r['turns_away'] and '--all' not in argv:
            continue
        print('  L%s/%d  pc %d at +0x%X  slot %d  %s  %s%s' % (r['level'], r['script'], r['pc'], r['offset'], r['slot'], r['jump'],
                                                          'turns away' if r['turns_away'] else 'two paths',
                                                          ' (in a nested function %s)' % (r['path'],) if r['path'] else ''))
