"""Builds build/ed-db from the disc, main.dol and the recompiler's output. Never committed: it holds the game's data.
Run through `python -I tools/ed/ed.py build`. Takes a few minutes the first time, for the rooms."""
import glob
import json
import multiprocessing
import os
import re
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disc
import gametext
import lua40
import ppcemu
import roomfile

DB = disc.DB
LEVEL_RE = re.compile(r'Levels/Level(-?\d+)/ScrLvl')
# Everything named in a room's tail: a kind, then a number.
RECORD_KINDS = re.compile(rb'(?<![A-Za-z0-9_])([A-Z][A-Za-z]{2,}[A-Za-z0-9_]{0,24})\0')


def build_disasm():
    out = os.path.join(DB, 'disasm.txt')
    pat = re.compile(r'^\s+// (80[0-9A-F]{6}: .*)$')
    lines = {}
    for path in glob.glob(os.path.join(disc.ROOT, 'generated', 'chunks', '*.c')):
        for line in open(path, encoding='utf8', errors='replace'):
            m = pat.match(line)
            if m:
                lines[m.group(1)[:8]] = m.group(1)
    with open(out, 'w') as f:
        for k in sorted(lines):
            f.write(lines[k] + '\n')
    return len(lines)


def build_calls():
    """Who calls what, from the disassembly: callee -> caller function starts."""
    syms = sorted(disc.symbols())
    import bisect
    calls = {}
    for line in open(os.path.join(DB, 'disasm.txt')):
        m = re.match(r'(80[0-9A-F]{6}): bl\s+0x([0-9A-F]{8})', line)
        if m:
            calls.setdefault(m.group(2), []).append(m.group(1))
    json.dump(calls, open(os.path.join(DB, 'calls.json'), 'w'))
    return len(calls)


def build_natives(dol):
    syms = disc.symbols()
    t = dol.native_table()
    with open(os.path.join(DB, 'natives.tsv'), 'w') as f:
        for name, (fn, at) in sorted(t.items()):
            f.write('%s\t%08X\t%s\t%08X\n' % (name, fn, syms.get(fn, ''), at))
    return t


def build_scripts(d):
    """Every script of every level, decompiled; and an index of which globals each one uses."""
    index = {}
    for path in d.glob(r'ScrLvl.*\.gpk'):
        level = LEVEL_RE.search(path).group(1)
        outdir = os.path.join(DB, 'scripts', 'L' + level)
        os.makedirs(outdir, exist_ok=True)
        for sid, data in lua40.pack_entries(d.read(path)).items():
            f = lua40.parse(data)
            text = lua40.decompile(f)
            with open(os.path.join(outdir, '%d.lua' % sid), 'w', encoding='utf8') as o:
                o.write(text + '\n')
            index.setdefault(str(sid), {})[level] = {'globals': lua40.globals_called(f), 'size': len(data)}
    json.dump(index, open(os.path.join(DB, 'scripts.json'), 'w'))
    return sum(len(v) for v in index.values())


def text_rows(blob):
    return {'%d,%d' % k: list(v) for k, v in gametext.read_groups(blob).items()}


def build_text(d, unpacker):
    out = {}
    b = d.read('UBootPak.bin')
    secs = gametext.pack_table(b)
    out['boot'] = text_rows(b[secs[4][0]:secs[4][0] + secs[4][1]])
    out['prompts'] = text_rows(b[secs[6][0]:secs[6][0] + secs[6][1]])
    for path in d.glob(r'Levels/Level[^/]+/URmTxt'):
        level = re.search(r'Level(-?\d+)/', path).group(1)
        try:
            out['room' + level] = text_rows(unpacker.unpack(d.read(path))[4:])
        except (ValueError, RuntimeError, struct.error, IndexError) as e:
            out['room' + level] = {}
            print('  text of level', level, 'not read:', e)
    json.dump(out, open(os.path.join(DB, 'text.json'), 'w'))
    items = {}
    for key, row in out['boot'].items():
        group, label, text = row
        m = re.match(r'(.+)_(PICKUP|INV|DESC)$', label)
        if m:
            items.setdefault(m.group(1), {'group': group})[m.group(2).lower()] = text
            items[m.group(1)][m.group(2).lower() + '_at'] = key
    json.dump(items, open(os.path.join(DB, 'items.json'), 'w'), indent=0)
    return sum(len(v) for v in out.values()), len(items)


def one_room(args):
    path, offset, size = args
    with open(disc.ISO, 'rb') as f:
        f.seek(offset)
        b = f.read(size)
    errors = []

    def read(what, fn):
        try:
            return fn()
        except Exception as e:  # a table that doesn't read must not stop the rest
            errors.append('%s: %r' % (what, e))
            return []
    try:
        first = struct.unpack('>I', b[4:8])[0]
        csize = struct.unpack('>I', b[12:16])[0]
        sec1 = struct.unpack('>I', b[16:20])[0]
        x = bytes(gametext.bpe_expand(b, first, min(first + csize, len(b))))
    except Exception as e:
        return path, {'error': repr(e), 'cameras': [], 'regions': [], 'markers': []}
    room = {'cameras': read('cameras', lambda: roomfile.cameras(x, sec1)), 'regions': read('regions', lambda: roomfile.regions(x)),
            'markers': read('markers', lambda: roomfile.markers(x)), 'bytes': len(x)}
    if errors:
        room['error'] = '; '.join(errors)
    return path, room


def build_rooms(d, workers=None):
    jobs = [(p, d.files[p][0], d.files[p][1]) for p in d.glob(r'^Rooms/rmed\d+\.bpe$')]
    rooms = {}
    t = time.time()
    with multiprocessing.Pool(workers or max(1, (os.cpu_count() or 2) - 1)) as pool:
        for k, (path, room) in enumerate(pool.imap_unordered(one_room, jobs), 1):
            rooms[str(int(re.search(r'rmed(\d+)', path).group(1)))] = room
            if k % 40 == 0:
                print('  %d of %d rooms, %ds' % (k, len(jobs), time.time() - t), flush=True)
    # Moving cameras' keys are big, and only the recording's camera check wants them.
    paths = {}
    for rid, room in rooms.items():
        for c in room['cameras']:
            k = {x: c.pop(x) for x in ('keys', 'target_keys') if x in c}
            if k:
                paths.setdefault(rid, {})[str(c['hash'])] = k
    json.dump(paths, open(os.path.join(DB, 'camera_paths.json'), 'w'))
    rooms = {k: rooms[k] for k in sorted(rooms, key=int)}
    json.dump(rooms, open(os.path.join(DB, 'rooms.json'), 'w'))
    return rooms


# The tables the kit reads, each from a verified generator in tools/ed/facts. Order matters: later ones read earlier ones.
FACTS = ['facts/verbs_regions.py', 'facts/checks/args/cam_all.py', 'facts/checks/args/things_build.py',
         'facts/item_table.py', 'facts/checks/args/levels_build.py', 'facts/mix_table.py', 'facts/transitions.py', 'facts/verbs.py', 'facts/completeness_checks.py']


def build_facts():
    os.makedirs(os.path.join(DB, 'facts'), exist_ok=True)
    here = os.path.dirname(os.path.abspath(__file__))
    for rel in FACTS:
        t = time.time()
        r = subprocess.run([sys.executable, '-I', os.path.join(here, *rel.split('/'))], cwd=disc.ROOT, capture_output=True, text=True, errors='replace')
        with open(os.path.join(DB, 'facts', os.path.basename(rel) + '.out'), 'w', encoding='utf8') as f:
            f.write(r.stdout + r.stderr)
        print('  %-36s %s, %ds' % (rel, 'ok' if r.returncode == 0 else 'FAILED, see build/ed-db/facts/%s.out' % os.path.basename(rel), time.time() - t), flush=True)
        if r.returncode:
            raise SystemExit(r.stderr[-1500:])


def build(steps=None):
    os.makedirs(DB, exist_ok=True)
    d = disc.Disc()
    dol = disc.Dol()
    want = lambda s: steps is None or s in steps
    if want('disasm') or not os.path.exists(os.path.join(DB, 'disasm.txt')):
        print('disassembly:', build_disasm(), 'instructions', flush=True)
        print('call graph:', build_calls(), 'functions called', flush=True)
    if want('natives'):
        print('script functions:', len(build_natives(dol)), flush=True)
    if want('scripts'):
        print('scripts:', build_scripts(d), flush=True)
    if want('text'):
        print('text entries, items:', build_text(d, ppcemu.Unpacker(os.path.join(DB, 'disasm.txt'))), flush=True)
    if want('rooms'):
        rooms = build_rooms(d)
        print('rooms:', len(rooms), 'cameras:', sum(len(r['cameras']) for r in rooms.values()), 'regions:',
              sum(len(r['regions']) for r in rooms.values()), 'markers:', sum(len(r['markers']) for r in rooms.values()),
              'errors:', sum(1 for r in rooms.values() if 'error' in r), flush=True)
    if want('facts'):
        print('facts:', flush=True)
        build_facts()


if __name__ == '__main__':
    build(sys.argv[1:] or None)
