#!/usr/bin/env python3
"""The game's cinematics (ed55), films (ed83) and input modes (ed56), named. Run from the repo root:
python -I tools/ed/facts/cinematics.py
Reads only: the dol (the two record tables and the stream-name table), the disc's file list, build/ed-db/scripts.
Writes only: build/ed-db/facts/cinematics.json and prints counts.

Every field below was read from the code that uses it (2026-11-16, a reader per table and a refuter each; the
instruction addresses are in docs/game/research/kit-groundwork.md). A field nobody reads is left out, not guessed."""
import collections
import glob
import json
import os
import re
import struct
import sys

sys.path.insert(0, 'tools/ed')
import disc  # noqa: E402

DB = 'build/ed-db'
CIN_TABLE, CIN_COUNT, CIN_SIZE = 0x80242150, 173, 32   # ed55(n): 0x8004F374(n, -1, 0, 0)
FMV_TABLE, FMV_COUNT, FMV_SIZE = 0x80241B38, 65, 24    # ed83(n): 0x8004F4D0(2, n, 1, -1, 0)
AUDIO_TABLE, AUDIO_COUNT = 0x8024C8E0, 166              # 0x800E2628(i): 32-byte stream names
NO_AUDIO = (-1, 60)                                     # 60 is "/levels/level00/no_audio.aud"
NEXT_KIND = {-1: None, 0: 'cinematic', 1: 'film'}

# The input modes, ed56(n) through the jump table 0x80244CAC (0x8007AA6C). Names are from the scripts that start
# each and their rooms and words (a pattern, not a name in the game); handlers and room reads were read from the code.
INPUT_MODES = {
    0: dict(name='the staff puzzle', rooms=[43], case=0x8007B5A4, bindings=0x80244920,
            handlers=[0x8007DAEC, 0x8007DE4C, 0x8007DFC4], also=[0x8007E508],
            room_reads='the case and 0x8007E508 (MOVE STAFF EXIT, from ed68(0) and the room updater 0x80083464)'),
    1: dict(name='the safe dial', rooms=[80], case=0x8007B834, bindings=0x802449A0,
            handlers=[0x8007EA00, 0x8007ECF4], tick=0x8007E9BC, room_reads='the case (finds tag 173 in the room in play)'),
    2: dict(name='piano and organ keys', rooms=[81, 123, 27, 345], case=0x8007B90C, bindings=0x80244A20,
            handlers=[0x80080814], room_reads='0x80080814 (room 81 picks the piano set)'),
    3: dict(name='the mantelpiece tiles', rooms=[253], case=0x8007B964, bindings=0x80244AA0,
            handlers=[0x80080D70, 0x80080C68], tick=0x800810D0, room_reads='the case and 0x80080D70'),
    4: dict(name='the mirror cranks', rooms=[89], case=0x8007BA50, bindings=0x80244B00,
            handlers=[0x800813B8, 0x800812C0, 0x80081F40, 0x80081FE8], room_reads='the case (tags 2265-2267, 10001)'),
    5: dict(name='the bomb countdown', rooms=[219], case=0x8007B548, bindings=None, handlers=[], tick=0x8007E124,
            room_reads='none found'),
    6: dict(name='nothing (the case is the exit)', rooms=[], case=0x8007BD30, bindings=None, handlers=[], room_reads='none'),
    7: dict(name='B and Start only (the opening dream)', rooms=[96], case=0x8007B50C, bindings=0x802448E0,
            handlers=[0x80006EBC, 0x80006CDC], tick=0x8007E38C, room_reads='only 0x80006CDC, the in-play Start'),
    8: dict(name='the clock dials', rooms=[78, 238], case=0x8007B478, bindings=0x80244880,
            handlers=[0x8007F140, 0x8007F254], room_reads='the case, 0x8007F140 and 0x8007F254 (room 78 picks the clock)'),
    # One case for 9 and 10: bindings 0x802447E0 in every chapter but 13, 0x80244840 in chapter 13 (0x8007ACF4);
    # the tick 0x8007E41C (0x8007ADDC-0x8007ADE8). L09/L12/L13 2782 starts 10 when flag 659 is on, else 9.
    9: dict(name='scribing a rune', rooms=[37] + list(range(261, 270)) + list(range(284, 292)), case=0x8007AC94,
            bindings=0x802447E0, bindings_chapter13=0x80244840, handlers=[0x80080034, 0x80080308, 0x80080794],
            tick=0x8007E41C, room_reads='the case and 0x80080034 (rooms 261-291 pick the rune set)'),
    10: dict(name='scribing a rune (as 9; only ed68 differs)', rooms=[37] + list(range(261, 270)) + list(range(284, 292)),
             case=0x8007AC94, bindings=0x802447E0, bindings_chapter13=0x80244840,
             handlers=[0x80080034, 0x80080308, 0x80080794], tick=0x8007E41C, room_reads='as 9'),
    11: dict(name='the candles', rooms=[71, 217, 218], case=0x8007AB04, bindings=0x80244740,
             handlers=[0x8007FB1C, 0x8007FF3C], tick=0x8007F610, room_reads='the case, 0x8007FB1C, 0x8007FF3C (218 / 71 pick the set)'),
}


def s16(b, o):
    return struct.unpack_from('>h', b, o)[0]


def s32(b, o):
    return struct.unpack_from('>i', b, o)[0]


def none_if(v, *nil):
    return None if v in (nil or (-1,)) else v


def first_arg(body, start):
    """The first top-level argument of the call whose '(' is at body[start], parentheses balanced."""
    depth, arg = 0, ''
    for ch in body[start:]:
        if ch == '(':
            depth += 1
            if depth == 1:
                continue
        elif ch == ')':
            depth -= 1
            if depth == 0:
                break
        elif ch == ',' and depth == 1:
            break
        arg += ch
    return arg.strip()


def values_of(arg, body):
    """The numbers an argument can be: a literal, or VAR / (VAR + k) / (VAR - k) with VAR only ever set to literals in the
    script. None when it can't be told from the script alone."""
    if re.fullmatch(r'-?\d+', arg):
        return {int(arg)}
    m = re.fullmatch(r'\(*\s*([A-Za-z_]\w*)\s*(?:([+-])\s*(\d+))?\s*\)*', arg)
    if not m:
        return None
    sets = re.findall(r'(?<![\w.])%s\s*=(?!=)\s*([^\n;]+)' % re.escape(m.group(1)), body)  # assignments, not ==
    if not sets or not all(re.fullmatch(r'-?\d+', s.strip()) for s in sets):
        return None
    k = int(m.group(3) or 0) * (-1 if m.group(2) == '-' else 1)
    return {int(s) + k for s in sets}


def callers(name):
    """{n: [level/script, ...]} for every call name(n) in the scripts; a call by a variable is listed under each value
    the script can give it, marked; [script, argument] for those that can't be told."""
    out, unknown = collections.defaultdict(list), []
    call = re.compile(r'\b%s\(' % name)
    for path in sorted(glob.glob(os.path.join(DB, 'scripts', 'L*', '*.lua'))):
        level = os.path.basename(os.path.dirname(path))
        sid = os.path.splitext(os.path.basename(path))[0]
        body = open(path, encoding='utf8', errors='replace').read()
        where = '%s/%s' % (level, sid)
        for m in call.finditer(body):
            arg = first_arg(body, m.end() - 1)
            vals = values_of(arg, body)
            if vals is None:
                unknown.append([where, arg])
                continue
            tag = where if re.fullmatch(r'-?\d+', arg) else '%s (by a variable: %s)' % (where, arg)
            for v in sorted(vals):
                if tag not in out[v]:
                    out[v].append(tag)
    return out, unknown


def main():
    dol, iso = disc.Dol(), disc.Disc()
    files = {p.lower(): p for p in iso.files}  # the game asks in lower case; the disc's own spelling is kept

    def on_disc(path):
        return files.get(path.lower()) if path else None

    audio = [dol.cstr(AUDIO_TABLE + 32 * i, 32) for i in range(AUDIO_COUNT)]

    def stream(i):
        return None if i in NO_AUDIO or not 0 <= i < AUDIO_COUNT else audio[i]

    cin_calls, cin_var = callers('ed55')
    fmv_calls, fmv_var = callers('ed83')
    mode_calls, mode_var = callers('ed56')

    o = dol.offset(CIN_TABLE)
    cinematics = []
    for n in range(CIN_COUNT):
        b = dol.data[o + n * CIN_SIZE:o + (n + 1) * CIN_SIZE]
        level = struct.unpack_from('>b', b, 16)[0]
        spool, pack = s16(b, 0), b[17]
        spool_path = 'levels/level%02d/cin%04d' % (level, spool) if level >= 0 else None
        pack_path = 'levels/level%02d/cin%04d.bin' % (level, pack) if level >= 0 else None
        kind = NEXT_KIND.get(s16(b, 24), s16(b, 24))
        cinematics.append(dict(
            n=n, id=s32(b, 4), level=none_if(level), room=none_if(s16(b, 8)), chapter=none_if(s16(b, 22)),
            start_script=none_if(s16(b, 12)), end_script=none_if(s16(b, 14)),
            audio=stream(s16(b, 10)), audio_index=s16(b, 10),
            spool_file=on_disc(spool_path), pack_file=on_disc(pack_path),
            missing=[p for p in (spool_path, pack_path) if p and not on_disc(p)],
            on_disc=bool(on_disc(spool_path) and on_disc(pack_path)),
            alignment=b[18],  # 1, 2 or 3; 0 is every alignment
            ending_gate=struct.unpack_from('>b', b, 19)[0], seen_flag=none_if(s16(b, 20)),
            next=None if kind is None else dict(kind=kind, n=s16(b, 26)), queued_room=none_if(s16(b, 28)),
            scripts=cin_calls.get(n, [])))

    o = dol.offset(FMV_TABLE)
    films = []
    for n in range(FMV_COUNT):
        b = dol.data[o + n * FMV_SIZE:o + (n + 1) * FMV_SIZE]
        avi, aud = s32(b, 0), s16(b, 12)
        kind = NEXT_KIND.get(s16(b, 16), s16(b, 16))
        avi_path = 'fmv/fmv%04d.avi' % avi
        # 0x800EB564 starts the sound only if the file opens: -1, 60 and a number with no file (50) give no sound of the
        # film's own. 60 is the "no sound" number on purpose (the end tests it with -1 to leave the sound playing), so
        # it isn't listed as a missing file.
        aud_path = None if aud in NO_AUDIO else 'fmv/fmv%04d.aud' % aud
        films.append(dict(
            n=n, id=s32(b, 4), avi=on_disc(avi_path) or avi_path, aud=on_disc(aud_path), aud_number=aud,
            missing=[p for p in (avi_path, aud_path) if p and not on_disc(p)],
            on_disc=bool(on_disc(avi_path)),
            alignment=s16(b, 8),  # 1, 2 or 3; -1 is any alignment (the low byte 255 picks flag block 0)
            end_script=none_if(s16(b, 10)), seen_flag=none_if(s16(b, 14)),
            next=None if kind is None else dict(kind=kind, n=s16(b, 18)), queued_room=none_if(s16(b, 20)),
            scripts=fmv_calls.get(n, [])))

    modes = []
    for n in range(12):
        m = dict(INPUT_MODES[n])
        m = {k: ('0x%08X' % v if isinstance(v, int) and k in ('case', 'bindings', 'bindings_chapter13', 'tick') else v)
             for k, v in m.items()}
        for k in ('handlers', 'also'):
            if k in m:
                m[k] = ['0x%08X' % a for a in m[k]]
        m['n'] = n
        m['scripts'] = mode_calls.get(n, [])
        modes.append(m)

    out = dict(
        _how='tools/ed/facts/cinematics.py: the dol tables 0x%08X (ed55, %d x %d bytes), 0x%08X (ed83, %d x %d), the '
             'stream names 0x%08X, and literal ed55/ed83/ed56 calls in the scripts. Fields read from their code; see '
             'docs/game/research/kit-groundwork.md.' % (CIN_TABLE, CIN_COUNT, CIN_SIZE, FMV_TABLE, FMV_COUNT, FMV_SIZE, AUDIO_TABLE),
        calls_not_told=dict(ed55=cin_var, ed83=fmv_var, ed56=mode_var),
        cinematics=cinematics, films=films, input_modes=modes)
    os.makedirs(os.path.join(DB, 'facts'), exist_ok=True)
    json.dump(out, open(os.path.join(DB, 'facts', 'cinematics.json'), 'w', encoding='utf8'), indent=1)
    by_var = sum(1 for c in cinematics + films for s in c['scripts'] if 'by a variable' in s)
    print('cinematics %d (%d with both files on the disc, %d played by a script), films %d (%d with the video on the '
          'disc, %d played by a script), input modes 12; %d script entries by a variable the script sets; calls whose '
          'number the script alone can\'t tell: ed55 %d, ed83 %d, ed56 %d' % (
              len(cinematics), sum(c['on_disc'] for c in cinematics), sum(1 for c in cinematics if c['scripts']),
              len(films), sum(f['on_disc'] for f in films), sum(1 for f in films if f['scripts']), by_var,
              len(cin_var), len(fmv_var), len(mode_var)))
    for name, rows in out['calls_not_told'].items():
        for where, arg in rows:
            print('  %s(%s) in %s: the number is not told by the script' % (name, arg, where))


if __name__ == '__main__':
    main()
