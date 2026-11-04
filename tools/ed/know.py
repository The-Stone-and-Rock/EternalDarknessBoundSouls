"""`ed how`, `ed needs`, `ed host`: use what the game and the host already have, instead of building it again."""
import bisect
import json
import os
import re
import subprocess

import disc

DB = disc.DB
HERE = os.path.dirname(os.path.abspath(__file__))
LOADS = {'lwz', 'lhz', 'lha', 'lbz', 'lfs', 'lfd', 'lwzu', 'lhzu', 'lbzu'}
STORES = {'stw', 'sth', 'stb', 'stfs', 'stfd', 'stwu', 'sthu', 'stbu'}


def _player_table():
    r13, abss, calls = {}, [], {}
    for line in open(os.path.join(HERE, 'player_data.tsv'), encoding='utf8'):
        if line.startswith('#') or not line.strip():
            continue
        kind, at, size, what, helper = line.rstrip('\n').split('\t')
        if kind == 'r13':
            for off in range(0, max(int(size), 1), 4):
                r13[int(at) + off] = (what, helper)
        elif kind == 'abs':
            abss.append((int(at, 16), int(size), what, helper))
        else:
            calls[at[2:].upper()] = (what, helper)
    return r13, abss, calls


class Code:
    def __init__(self):
        self.addrs, self.ops = [], []
        for line in open(os.path.join(DB, 'disasm.txt')):
            self.addrs.append(int(line[:8], 16))
            self.ops.append(line[10:].strip())
        self.index = {a: i for i, a in enumerate(self.addrs)}
        # True starts only: called, or a script function. Decomp symbols can sit in mid-function.
        self.starts = set()
        for op in self.ops:
            m = re.match(r'bl\s+0x([0-9A-F]{8})', op)
            if m:
                self.starts.add(int(m.group(1), 16))
        for line in open(os.path.join(DB, 'natives.tsv')):
            self.starts.add(int(line.split('\t')[1], 16))

    def body(self, addr):
        """A function's instructions in address order, by following its branches until every path ends. A jump table isn't followed."""
        seen = set()
        todo = [addr]
        while todo:
            a = todo.pop()
            while a in self.index and a not in seen:
                seen.add(a)
                op = self.ops[self.index[a]]
                name = op.split()[0].rstrip('+-.') if op else ''
                target = re.search(r'0x([0-9A-F]{8})$', op)
                if name in ('blr', 'bctr', 'rfi'):
                    break
                if name == 'b':
                    t = int(target.group(1), 16) if target else None
                    if t is None or t in self.starts:  # a tail call
                        break
                    a = t
                    continue
                if name == 'bc' and target:
                    todo.append(int(target.group(1), 16))
                a += 4
        for a in sorted(seen):
            yield a, self.ops[self.index[a]]


def resolve(q):
    """A script name, a short name, or a code address: to an address and its script name. Anything else is an error, not an empty answer."""
    natives = {l.split('\t')[0]: l.split('\t')[1] for l in open(os.path.join(DB, 'natives.tsv'))}
    if q in natives:
        return int(natives[q], 16), q
    short = {}
    for line in open(os.path.join(HERE, 'primitives.tsv'), encoding='utf8'):
        if not line.startswith('#') and line.strip():
            c = line.split('\t')
            short.setdefault(c[1], []).append(c[0])
    if q in short:
        if len(short[q]) > 1:
            print('%s is the short name of %s: taking %s' % (q, ' and '.join(short[q]), short[q][0]))
        n = short[q][0]
        return int(natives[n], 16), n
    if not re.fullmatch(r'(0x)?[0-9a-fA-F]{8}', q):
        raise SystemExit('%s is no script function, short name or address. `ed fn NAME`, tools/ed/primitives.tsv' % q)
    addr = int(q, 16)
    first = open(os.path.join(DB, 'disasm.txt')).readline()
    with open(os.path.join(DB, 'disasm.txt')) as f:
        have = any(line.startswith('%08X' % addr) for line in f)
    if not have:
        raise SystemExit('0x%08X is not in the game\'s code' % addr)
    for n, a in natives.items():
        if int(a, 16) == addr:
            return addr, n
    return addr, None


def scan(code, addr, r13t, abst, callt):
    """What one function touches directly, and what it calls."""
    hits, callees = [], []
    hi, value = {}, {}
    for a, op in code.body(addr):
        p = op.replace(',', ' ').split()
        name = p[0].rstrip('.')
        if name == 'lis':
            hi[p[1]] = (int(p[2], 0) << 16) & 0xFFFFFFFF
            value.pop(p[1], None)
        elif name == 'addi' and p[2] in hi:
            value[p[1]] = (hi[p[2]] + int(p[3], 0)) & 0xFFFFFFFF
            for start, size, what, helper in abst:
                if start <= value[p[1]] < start + size:  # a base address taken, read through an index later
                    hits.append(('addr', '0x%08X' % value[p[1]], what, helper, a))
        elif name == 'addi' and p[2] in value:
            value[p[1]] = (value[p[2]] + int(p[3], 0)) & 0xFFFFFFFF
        elif name in LOADS or name in STORES:
            m = re.match(r'(-?\d+)\((r\d+)\)', p[2]) if len(p) > 2 else None
            if m:
                off, base = int(m.group(1)), m.group(2)
                how = 'write' if name in STORES else 'read'
                if base == 'r13' and off in r13t:
                    hits.append((how, 'r13%+d' % off, *r13t[off], a))
                else:
                    at = None
                    if base in value:
                        at = (value[base] + off) & 0xFFFFFFFF
                    elif base in hi:
                        at = (hi[base] + off) & 0xFFFFFFFF
                    if at is not None:
                        for start, size, what, helper in abst:
                            if start <= at < start + size:
                                hits.append((how, '0x%08X' % at, what, helper, a))
        elif name == 'bl':
            callees.append(int(p[1], 0))
            t = p[1][2:].upper()
            if t in callt:
                hits.append(('call', '0x' + t, *callt[t], a))
    return hits, callees


def cmd_needs(argv):
    if not argv:
        raise SystemExit('ed needs ADDR|fnN [--depth N], or ed needs --script ID')
    depth = 3
    extra_roots = []
    if '--script' in argv:
        i = argv.index('--script')
        sid = argv[i + 1]
        del argv[i:i + 2]
        names = set()
        for row in json.load(open(os.path.join(DB, 'scripts.json'))).get(sid, {}).values():
            names |= set(row['globals'])
        table = {l.split('\t')[0]: int(l.split('\t')[1], 16) for l in open(os.path.join(DB, 'natives.tsv'))}
        extra_roots = sorted(table[n] for n in names if n in table)
        if not extra_roots:
            raise SystemExit('script %s calls no known function' % sid)
        print('script %s calls %d functions: %s' % (sid, len(extra_roots), ' '.join(sorted(n for n in names if n in table))))
    if '--depth' in argv:
        i = argv.index('--depth')
        depth = int(argv[i + 1])
        del argv[i:i + 2]
    if extra_roots and not argv:
        argv = ['%08X' % extra_roots[0]]
    addr, name = resolve(argv[0])
    q = name or argv[0]
    natives = {name: 1} if name else {}
    code = Code()
    r13t, abst, callt = _player_table()
    syms = disc.symbols()
    seen = {addr: 0}
    queue = [addr]
    for a in extra_roots:
        if a not in seen:
            seen[a] = 0
            queue.append(a)
    found = {}
    if '%08X' % addr in callt:
        what, helper = callt['%08X' % addr]
        found[('0x%08X' % addr, what, helper)] = {'how': {'is'}, 'in': [addr], 'depth': 0}
    while queue and len(seen) < 1500:
        f = queue.pop(0)
        hits, callees = scan(code, f, r13t, abst, callt)
        for how, where, what, helper, at in hits:
            key = (where, what, helper)
            row = found.setdefault(key, {'how': set(), 'in': [], 'depth': seen[f]})
            row['how'].add(how)
            if len(row['in']) < 3 and f not in row['in']:
                row['in'].append(f)
        if seen[f] < depth:
            for c in callees:
                if c not in seen:
                    seen[c] = seen[f] + 1
                    queue.append(c)
    print('0x%08X %s: looked through %d functions, %d calls deep' % (addr, q if q in natives else syms.get(addr, ''), len(seen), depth))
    if not found:
        print('None of the one-player data listed in tools/ed/player_data.tsv was found.')
        print('That list is what is known so far: a pointer the game passes around is not seen. Test it for player 2 before relying on this.')
        return
    print('It reaches this one-player data. To call it for player 2, the helper on the right must have their data in first.')
    print('This follows direct calls only: a script it runs (`ed needs --script ID`), a table, and work it leaves for later in the frame aren\'t followed.')
    groups = {}
    for (where, what, helper), row in found.items():
        groups.setdefault(helper, []).append((where, what, row))
    for helper, rows in sorted(groups.items()):
        print('\n  %s' % helper)
        for where, what, row in sorted(rows, key=lambda x: x[2]['depth']):
            print('    %-12s %-5s %-48s depth %d, in %s' % (where, '/'.join(sorted(row['how'])), what[:48], row['depth'], ' '.join('%08X' % x for x in row['in'])))


DEFINITION = re.compile(r'^(?:static |extern "C" |inline )*[A-Za-z_][\w \*]*?\b([a-z_][a-z0-9_]+)\([^;{}]*\)\s*\{', re.M)


def _functions(path):
    """Each function in a host file with the comment above it: over several lines, and a body on one line."""
    text = open(path, encoding='utf8', errors='replace').read()
    lines = text.split('\n')
    out = []
    for m in DEFINITION.finditer(text):
        line = m.group(0)
        if line.startswith(('if', 'for', 'while', 'switch', 'else', 'return', 'do')):
            continue
        i = text.count('\n', 0, m.start())
        j = i - 1
        comment = []
        # A comment block can sit above #defines that belong to the function.
        while j >= 0 and (lines[j].startswith('//') or lines[j].startswith('#define') or (not lines[j].strip() and j > 0 and lines[j - 1].startswith('#define'))):
            if lines[j].startswith('//'):
                comment.insert(0, lines[j][2:].strip())
            j -= 1
        out.append((i + 1, m.group(1), ' '.join(comment)))
    return out


def cmd_host(argv):
    if not argv:
        raise SystemExit('ed host WORD...: what the host already has for this')
    words = [w.lower() for w in argv]
    print('What this project has already built for: %s' % ' '.join(argv))
    same = {'examine': ('examine', 'close_up', 'close-up', 'close up', 'look_prompt'), 'pickup': ('pick', 'find', 'found', 'take'), 'door': ('door', 'arriv', 'follow'),
            'camera': ('camera', 'view'), 'tome': ('tome',), 'item': ('item', 'carr'), 'words': ('words', 'window', 'message')}
    n = 0
    for path in ('runtime/host/hle.c', 'runtime/host/camera.c', 'runtime/host/controls.cpp', 'runtime/host/frontend.cpp'):
        full = os.path.join(disc.ROOT, path)
        for line, name, comment in _functions(full):
            if name in argv:
                n += 1
                print('%s:%d %s\n    %s' % (path, line, name, comment[:260] or '(no comment)'))
                continue
            blob = ' ' + re.sub(r'[^a-z0-9]+', ' ', (name + ' ' + comment).lower()) + ' '
            if all(any((' ' + re.sub(r'[^a-z0-9]+', ' ', x).strip()) in blob for x in same.get(w, (w,))) for w in words):
                n += 1
                print('%s:%d %s\n    %s' % (path, line, name, comment[:260] or '(no comment)'))
    print('%d function(s).' % n)
    print('--- docs')
    r = re.compile('|'.join(re.escape(w) for w in words), re.I)
    for root, _, files in os.walk(os.path.join(disc.ROOT, 'docs')):
        for f in files:
            if not f.endswith('.md'):
                continue
            path = os.path.join(root, f)
            for k, text in enumerate(open(path, encoding='utf8', errors='replace'), 1):
                if all(w in text.lower() for w in words):
                    print('%s:%d %s' % (os.path.relpath(path, disc.ROOT).replace('\\', '/'), k, text.strip()[:200]))
    print('--- commits')
    cmd = ['git', '-C', disc.ROOT, 'log', '--oneline', '-i', '--all-match', '-n', '12'] + [x for w in words for x in ('--grep', w)]
    print(subprocess.run(cmd, capture_output=True, text=True).stdout.strip() or '(none)')


def gameinfo_hashes():
    import gameinfo
    return gameinfo.hash_names()


def _script_text(level, sid):
    path = os.path.join(DB, 'scripts', 'L' + level, '%d.lua' % sid)
    return open(path, encoding='utf8').read() if os.path.exists(path) else ''


def summary(body, level, text, hashes, prompts):
    """What a script does, as a normal player meets it."""
    out = []
    room = text.get('room' + level, {})
    for m in re.finditer(r'\bed301\((\d+), (\d+)\)', body):
        row = prompts.get('0,%s' % m.group(2))
        out.append('prompt %s (button %s)' % (row[1] if row else m.group(2), m.group(1)))
    for m in re.finditer(r'\bed11\((\d+), (\d+), (\w+)', body):
        row = text['boot'].get('%s,%s' % (m.group(1), m.group(2)))
        out.append('words %s: "%s"' % (row[1] if row else '', (row[2] if row else '')[:90]))
    for m in re.finditer(r'\bed12\((\d+), (\d+), (\w+)', body):
        row = room.get('%s,%s' % (m.group(1), m.group(2)))
        out.append('room words %s: "%s"' % (row[1] if row else '', (row[2] if row else '')[:90]))
    for m in re.finditer(r'\bed(219|220|221)\((\d+), (\d+)(?:, (\d+))?', body):
        row = room.get('%s,%s' % (m.group(2), m.group(3)))
        cam = hashes.get(int(m.group(4)), {}).get('name', m.group(4)) if m.group(4) else 'none'
        out.append('EXAMINE prompt (ed%s), close-up camera %s, words %s: "%s"' % (m.group(1), cam, row[1] if row else '', (row[2] if row else '')[:80]))
    for m in re.finditer(r'\bfn8\((\d+)', body):
        out.append('camera ' + hashes.get(int(m.group(1)), {}).get('name', m.group(1)))
    for m in re.finditer(r'\bfn66\((\d+), (\d+), (\d+), (\d+)', body):
        out.append('door to room %s at %s, transition %s' % (m.group(1), hashes.get(int(m.group(2)), {}).get('name', m.group(2)), m.group(4)))
    if re.search(r'\bfn79\(fn78\(1, 0\), 71', body) or re.search(r', 71, 0\)', body):
        out.append('event 71: the pick up (0x80024528)')
    if re.search(r'\bfn21\(0\)', body):
        out.append('holds play (fn21)')
    for m in re.finditer(r'\bfn29\((\d+), 1\)', body):
        out.append('flag %s on' % m.group(1))
    for m in re.finditer(r'\bfn30\((\d+)\)', body):
        out.append('asks flag %s' % m.group(1))
    seen, keep = set(), []
    for x in out:
        if x not in seen:
            seen.add(x)
            keep.append(x)
    return keep


def cmd_how(argv):
    if not argv:
        raise SystemExit('ed how THING: what the game does for a normal player with it')
    level = None
    if '--level' in argv:
        i = argv.index('--level')
        level = argv[i + 1].zfill(2) if i + 1 < len(argv) else None
        del argv[i:i + 2]
    q = ' '.join(argv).lower()
    words = q.split()
    text = json.load(open(os.path.join(DB, 'text.json'), encoding='utf8'))
    items = json.load(open(os.path.join(DB, 'items.json'), encoding='utf8'))
    rooms = json.load(open(os.path.join(DB, 'rooms.json'), encoding='utf8'))
    index = json.load(open(os.path.join(DB, 'scripts.json')))
    prompts = text.get('prompts', {})
    hashes = gameinfo_hashes()
    print('How the game does "%s" for a normal player (from the disc).\n' % q)
    import gameinfo
    route = gameinfo.route(q)
    if route:
        print('START HERE: ' + route + '\n')
    head = gameinfo.how_lines(q)
    for line in head:
        print(line)
    if head:
        print()
    # The words, everywhere
    scripts = {}
    print('WORDS')
    calls = {}
    root = os.path.join(DB, 'scripts')
    bodies = {}
    for lv in os.listdir(root):
        for f in os.listdir(os.path.join(root, lv)):
            bodies[(lv[1:], int(f[:-4]))] = open(os.path.join(root, lv, f), encoding='utf8').read()
    for (lv, sid), body in bodies.items():
        for m in re.finditer(r'\bed11\((\d+), (\d+)', body):
            calls.setdefault(('boot', m.group(1) + ',' + m.group(2)), set()).add((lv, sid))
        for m in re.finditer(r'\bed(?:12|219|220|221)\((\d+), (\d+)', body):
            calls.setdefault(('room' + lv, m.group(1) + ',' + m.group(2)), set()).add((lv, sid))
    n = 0
    for pack, rows in sorted(text.items()):
        if pack == 'prompts' or (level and pack.startswith('room') and pack != 'room' + level):
            continue
        for key, (group, label, words_text) in rows.items():
            blob = words_text.lower() + ' ' + label.lower().replace('_', ' ')
            if all(w in blob for w in words):
                n += 1
                by = calls.get((pack, key), set())
                for s in by:
                    scripts[s] = 'opens these words'
                if n <= 25:
                    print('  %-7s %-6s %-22s "%s"%s' % (pack, key, label[:22], words_text[:120], ('  <- ' + ' '.join('L%s/%d' % s for s in sorted(by)[:5])) if by else ''))
    if not n:
        print('  none')
    elif n > 25:
        print('  +%d more words: `ed text %s`, or --level N' % (n - 25, q))
    # Regions named for it, and the scripts they run
    print('\nWHERE IT IS (regions named for it, in the rooms)')
    n = 0
    regions = {}
    squeezed = q.replace(' ', '')
    for rid, r in sorted(rooms.items(), key=lambda kv: int(kv[0])):
        for x in r['regions']:
            nm = x['name'].lower()
            if squeezed in nm or any(len(w) > 3 and w in nm for w in words):
                n += 1
                runs = {k: x[k] for k in ('enter', 'leave', 'stay', 'cross_a', 'cross_b', 'other_enter', 'other_leave', 'other_stay') if x.get(k, -1) > 0}
                for sid in runs.values():
                    regions.setdefault(sid, []).append((rid, x['name']))
                if n <= 25:
                    print('  room %-5s %-24s %s' % (rid, x['name'], ' '.join('%s %d' % kv for kv in runs.items()) or 'runs nothing'))
    if n > 25:
        print('  +%d more regions' % (n - 25))
    if not n:
        print('  no region is named for it. A region\'s name isn\'t always what it holds: `ed item`, `ed verb`, `ed examine`, `ed door ROOM`')
    for sid in regions:
        if sid:
            for lv in index.get(str(sid), {}):
                scripts.setdefault((lv, sid), 'runs in a region named for it')
    print('\nWHAT THE GAME DOES (each script, in the words of its calls; `ed script ID` for all of it)')
    done = set()
    for (lv, sid), why in sorted(scripts.items(), key=lambda kv: (kv[0][1], kv[0][0])):
        if level and lv != level:
            continue
        body = bodies.get((lv, sid), '')
        if (sid, body) in done:
            continue
        done.add((sid, body))
        if len(done) > 30:
            continue
        print('  script %d (level %s), %s' % (sid, lv, why))
        lines = summary(body, lv, text, hashes, prompts)[:14]
        for line in lines:
            print('     ' + line)
        if not lines:
            print('     (no prompt, words or door by itself: `ed script %d`)' % sid)
    if len(done) > 30:
        print('  +%d more scripts: --level N, or `ed text`, `ed uses`' % (len(done) - 30))
    print('\nNext: `ed needs fnN` shows what a call needs swapped for player 2; `ed host WORD` what the host already has.')


def cmd_keep(argv):
    # (keep.md is the user's list)
    print(open(os.path.join(disc.ROOT, 'docs', 'game', 'keep.md'), encoding='utf8').read())
