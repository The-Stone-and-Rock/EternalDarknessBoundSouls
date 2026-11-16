"""`ed how`, `ed needs`, `ed host`: use what the game and the host already have, instead of building it again."""
import bisect
import json
import os
import re
import subprocess

import disc

DB = disc.DB
HERE = os.path.dirname(os.path.abspath(__file__))
LOADS = {'lwz', 'lhz', 'lha', 'lbz', 'lfs', 'lfd', 'lwzu', 'lhzu', 'lhau', 'lbzu', 'lfsu', 'lfdu', 'psq_l', 'psq_lu'}
STORES = {'stw', 'sth', 'stb', 'stfs', 'stfd', 'stwu', 'sthu', 'stbu', 'stfsu', 'stfdu', 'psq_st', 'psq_stu'}


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


_HOOKS = None


def hooks():
    """{game address: [hook rows]} from build/ed-db/facts/hooks.json (tools/ed/facts/hooks.py), or {} before it's built."""
    global _HOOKS
    if _HOOKS is None:
        path = os.path.join(DB, 'facts', 'hooks.json')
        _HOOKS = {int(a, 16): rows for a, rows in json.load(open(path, encoding='utf8'))['hooks'].items()} if os.path.exists(path) else {}
    return _HOOKS


def live(r):
    """A hook that does co-op or game work on a normal run: not a log or trace, not one that loses its slot."""
    # 'only with' a switch off by default, 'only for the call returning to X' (it passes the others through)
    return not r.get('debug') and not r.get('slot', '').startswith(('loses', 'installs only while')) and not r.get('when', '').startswith('only')


def hook_lines(addr):
    """A line per host hook on a game address: kind, host function, hle.c line, the ED_ switches its installer reads,
    and why it may not be in effect (a log or trace, a slot another install takes, a switch that's off by default)."""
    out = []
    for r in hooks().get(addr, []):
        notes = [n for n in ('debug: a log or trace' if r.get('debug') else '', r.get('slot', ''), r.get('when', '')) if n]
        out.append('  %-9s %s  (hle.c:%d, installed by %s%s%s)%s' % (
            r['kind'], r['host'], r['line'], r['installed_by'] or '?', (', ' + r['pp']) if r['pp'] else '',
            (', reads ' + ' '.join(r['switches'])) if 0 < len(r['switches']) <= 4 else
            (', which reads %d ED_ switches' % len(r['switches'])) if r['switches'] else '',
            ('  [%s]' % '; '.join(notes)) if notes else ''))
    return out


def lr_lines(addr):
    """Calls this function makes that a host handler picks out by their return address (cpu->lr == X)."""
    path = os.path.join(DB, 'facts', 'hooks.json')
    rows = json.load(open(path, encoding='utf8')).get('by_return_address', {}).get('0x%08X' % addr, []) if os.path.exists(path) else []
    # == and != are both a handler singling out that call (!= is usually "return early for every other call").
    return ['  the call returning to %s: %s checks for it (lr %s, hle.c:%d, %s)%s' % (
        r['lr'], r['host'], r['test'], r['line'], r['name'],
        '  [debug: a log or trace]' if r.get('debug') else '') +
        ('  [CAN\'T MATCH: the instruction before it is not a call]' if r.get('not_a_return_address') else '') for r in rows]


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


GPR = re.compile(r'r\d+$')
# Instructions whose first operand is read, not written (or isn't a register): they leave what a register holds alone.
KEEPS = STORES | {'stmw', 'stwx', 'stbx', 'sthx', 'stfsx', 'stfdx', 'psq_stx', 'stwcx',
                  'cmpw', 'cmplw', 'cmpwi', 'cmplwi', 'cmp', 'cmpl', 'mtlr', 'mtctr', 'mtspr', 'mtcrf', 'mtcr', 'mtxer',
                  'mtmsr', 'mtsr', 'mtsrr', 'mtsprg', 'tw', 'twi', 'dcbf', 'dcbst', 'dcbi', 'dcbz', 'dcbt', 'icbi',
                  'b', 'bc', 'bl', 'bla', 'blr', 'bctr', 'bctrl', 'blrl', 'bclr'}
UPDATES = {'lwzu', 'lhzu', 'lhau', 'lbzu', 'lfsu', 'lfdu', 'psq_lu', 'stwu', 'sthu', 'stbu', 'stfsu', 'stfdu', 'psq_stu'}
CALLS = {'bl', 'bla', 'bctrl', 'blrl'}
ENDS = {'b', 'blr', 'bctr', 'rfi'}  # no fall-through: the next instruction in address order is another path
VOLATILE = ['r0'] + ['r%d' % n for n in range(3, 13)]
DOT_TESTS = {'rlwinm', 'andi', 'andis', 'or', 'and', 'extsh', 'extsb', 'srawi', 'addic', 'neg', 'add', 'subf'}


def _compared(body, idx, reg, ahead=6):
    """Is the value just loaded into reg branched on within the next few instructions of the same path: a cmp of it, or
    a dot-form (rlwinm. andi. or. extsh. ...) made from it, through mr / extsh / extsb / rlwinm copies? A call ends what
    r0 and r3-r12 hold (the callee's return value is not the word), and an unconditional branch or a gap in addresses
    ends the path."""
    held = {reg}
    prev = body[idx][0]
    for a, op in body[idx + 1:idx + 1 + ahead]:
        if a != prev + 4:
            return False
        prev = a
        p = op.replace(',', ' ').split()
        if not p:
            continue
        name = p[0].rstrip('.')
        dot = p[0].endswith('.')
        srcs = p[2:] if len(p) > 2 else []
        if name.startswith('cmp'):
            if any(x in held for x in p[1:]):
                # Against 0 or -1 (cmpwi/cmplwi rX, 0 / -1, the game's "none" mark): a there-or-not test, not a pick
                # between values.
                return 'zero' if name in ('cmpwi', 'cmplwi') and re.fullmatch(r'(0x)?0+|-1|0x[Ff]{4}', p[-1]) else True
        elif dot and name in DOT_TESTS and any(x in held for x in srcs):
            return True  # sets cr0 from the value: a test of it
        elif name in ('subfic', 'xori') and len(p) > 3 and p[2] in held and re.fullmatch(r'-?(0x)?[0-9A-Fa-f]+', p[3]):
            return True  # the compiler's branchless "is it N" (0x8007F254: subfic r3,r6,78 picks the clock by room 78)
        elif name in CALLS:
            held -= set(VOLATILE)
        elif name in ENDS:
            return False
        elif name in ('extsh', 'extsb', 'rlwinm') and srcs and srcs[0] in held:
            held.add(p[1])
        elif name == 'or' and len(p) > 3 and p[2] == p[3] and p[2] in held:  # mr
            held.add(p[1])
        elif len(p) > 1 and p[1] in held and not name.startswith('st'):
            held.discard(p[1])  # overwritten
        if not held:
            return False
    return False


def _abs_hits(at, abst, how, a, hits):
    for start, size, what, helper in abst:
        if start <= at < start + size:
            hits.append((how, '0x%08X' % at, what, helper, a))


def scan(code, addr, r13t, abst, callt):
    """What one function touches directly, and what it calls.

    A register holds a known address after lis (hi: the high half only) and lis+addi/ori, and through more addi steps
    (value). Any other write to it, and a call (r0, r3-r12), forgets it, so a stale base can't name a word. The walk
    is in address order, not by path: a base set on one branch can still reach a use on another."""
    hits, callees = [], []
    hi, value = {}, {}

    def forget(reg):
        hi.pop(reg, None)
        value.pop(reg, None)

    body = list(code.body(addr))
    for idx, (a, op) in enumerate(body):
        p = op.replace(',', ' ').split()
        name = p[0].rstrip('.')
        dst = p[1] if len(p) > 1 and GPR.match(p[1]) else None
        if name == 'lis':
            forget(dst)
            hi[dst] = (int(p[2], 0) << 16) & 0xFFFFFFFF
        elif name in ('addi', 'ori') and len(p) > 3 and (p[2] in value or p[2] in hi):
            base = value[p[2]] if p[2] in value else hi[p[2]]
            imm = int(p[3], 0)
            at = (base + imm) & 0xFFFFFFFF if name == 'addi' else base | (imm & 0xFFFF)
            forget(dst)
            value[dst] = at
            _abs_hits(at, abst, 'addr', a, hits)  # a base address taken, read through an index later
        elif name == 'add' and len(p) > 3 and ((p[2] in value or p[2] in hi) != (p[3] in value or p[3] in hi)):
            # a table's address plus an index (lis; addi; add rD, base, rIndex): rD stays inside that table, the index
            # unknown, so an access through it is counted as the table's (at the base plus its offset).
            src = p[2] if (p[2] in value or p[2] in hi) else p[3]
            src_hi, src_value = hi.get(src), value.get(src)
            forget(dst)
            if src_hi is not None:
                hi[dst] = src_hi
            if src_value is not None:
                value[dst] = src_value
        elif name == 'or' and len(p) > 3 and p[2] == p[3]:  # mr: the copy holds what the source held
            src_hi, src_value = hi.get(p[2]), value.get(p[2])
            forget(dst)
            if src_hi is not None:
                hi[dst] = src_hi
            if src_value is not None:
                value[dst] = src_value
        elif name in LOADS or name in STORES:
            m = re.match(r'(-?\d+)\((r\d+)\)', p[2]) if len(p) > 2 else None
            if m:
                off, base = int(m.group(1)), m.group(2)
                how = 'write' if name in STORES else 'read'
                if how == 'read' and dst:
                    c = _compared(body, idx, dst)
                    if c == 'zero':
                        how = 'zero-test'  # a there-or-not test (cmp against 0): it branches, but doesn't pick a value
                    elif c:
                        how = 'compare'  # the function picks by the value: a branch on it, not just passing it on
                if base == 'r13' and off in r13t:
                    hits.append((how, 'r13%+d' % off, *r13t[off], a))
                else:
                    at = None
                    if base in value:
                        at = (value[base] + off) & 0xFFFFFFFF
                    elif base in hi:
                        at = (hi[base] + off) & 0xFFFFFFFF
                    if at is not None:
                        _abs_hits(at, abst, how, a, hits)
                    if name in UPDATES:  # the base moves to the address just used
                        forget(base)
                        if at is not None:
                            value[base] = at
            if name in LOADS and dst:
                forget(dst)
        elif name == 'bl':
            callees.append(int(p[1], 0))
            t = p[1][2:].upper()
            if t in callt:
                hits.append(('call', '0x' + t, *callt[t], a))
            for reg in VOLATILE:
                forget(reg)
        elif name in CALLS:  # bctrl, blrl, bla: a call through a pointer, not followed, but it still ends r0, r3-r12
            for reg in VOLATILE:
                forget(reg)
        elif name == 'lmw' and dst:
            for n in range(int(dst[1:]), 32):
                forget('r%d' % n)
        elif dst and name not in KEEPS:
            forget(dst)
    return hits, callees


class Needs:
    """`ed needs` as something to call: the one-player words a function or a script reaches, each with its helper.
    One disassembly load and one scan per function for any number of questions (the catalogue asks thousands).

    A found row is keyed (where, what, helper) and holds: how (read / write / compare / addr / call / is), depth (the
    shallowest it was seen at), cmp (the shallowest compare, or None), offs (offsets inside a table), in (up to 3
    functions it was seen in)."""

    def __init__(self):
        self.code = Code()
        self.r13t, self.abst, self.callt = _player_table()
        self._scans, self._natives, self._native_reach = {}, None, {}

    def scan(self, f):
        if f not in self._scans:
            self._scans[f] = scan(self.code, f, self.r13t, self.abst, self.callt)
        return self._scans[f]

    def reach(self, roots, depth=3, cap=1500):
        """Walk direct calls from the roots, breadth first, `depth` calls deep, at most `cap` functions."""
        seen, queue, found = {}, [], {}
        for a in roots:
            if a not in seen:
                seen[a] = 0
                queue.append(a)
        for a in roots:
            if '%08X' % a in self.callt:
                what, helper = self.callt['%08X' % a]
                found[('0x%08X' % a, what, helper)] = {'how': {'is'}, 'in': [a], 'depth': 0, 'offs': set(), 'cmp': None, 'cmp_in': []}
        while queue and len(seen) < cap:
            f = queue.pop(0)
            hits, callees = self.scan(f)
            for how, where, what, helper, at in hits:
                off = None
                if how != 'call' and where.startswith('0x'):  # a table's word: one row per table, with the offsets seen
                    at_word = int(where, 16)
                    for start, size, w, h in self.abst:
                        if w == what and start <= at_word < start + size:
                            where, off = '0x%08X' % start, at_word - start
                            break
                key = (where, what, helper)
                row = found.setdefault(key, {'how': set(), 'in': [], 'depth': seen[f], 'offs': set(), 'cmp': None, 'cmp_in': []})
                row['how'].add(how)
                if how == 'compare' and (row['cmp'] is None or seen[f] < row['cmp']):
                    row['cmp'] = seen[f]  # the shallowest depth it's branched on, which may be deeper than where it's first seen
                if how == 'compare' and f not in row['cmp_in'] and len(row['cmp_in']) < 6:
                    row['cmp_in'].append(f)  # the functions that branch on it: where a hook would have to sit
                if off is not None:
                    row['offs'].add(off)
                if len(row['in']) < 3 and f not in row['in']:
                    row['in'].append(f)
            if seen[f] < depth:
                for c in callees:
                    if c not in seen:
                        seen[c] = seen[f] + 1
                        queue.append(c)
        return seen, found

    def natives(self):
        """{script function name: address} from natives.tsv."""
        if self._natives is None:
            self._natives = {l.split('\t')[0]: int(l.split('\t')[1], 16) for l in open(os.path.join(DB, 'natives.tsv'))}
        return self._natives

    def native(self, name, depth=3):
        """What one script function reaches (cached): (seen, found)."""
        if name not in self._native_reach:
            self._native_reach[name] = self.reach([self.natives()[name]], depth)
        return self._native_reach[name]

    def script(self, globals_, depth=3):
        """What a script reaches through the script functions it calls (its `globals` in scripts.json, one level's
        copy): the union of each function's own walk, so each walk is done once for the whole game.
        Returns {key: row} with row['via'], the script functions that reach it."""
        merged = {}
        for name in sorted(set(globals_) & set(self.natives())):
            for key, row in self.native(name, depth)[1].items():
                m = merged.get(key)
                if m is None:
                    m = merged[key] = {'how': set(), 'in': [], 'depth': row['depth'], 'offs': set(), 'cmp': None, 'cmp_in': [], 'via': []}
                m['how'] |= row['how']
                for f in row.get('cmp_in', []):
                    if f not in m['cmp_in'] and len(m['cmp_in']) < 6:
                        m['cmp_in'].append(f)
                m['offs'] |= row['offs']
                m['depth'] = min(m['depth'], row['depth'])
                if row['cmp'] is not None and (m['cmp'] is None or row['cmp'] < m['cmp']):
                    m['cmp'] = row['cmp']
                for f in row['in']:
                    if len(m['in']) < 3 and f not in m['in']:
                        m['in'].append(f)
                m['via'].append(name)
        return merged


def cmd_needs(argv):
    if not argv:
        raise SystemExit('ed needs ADDR|fnN [--depth N], or ed needs --script ID|L00/ID [--level N]')
    depth = 3
    extra_roots = []
    level = None
    if '--level' in argv:
        i = argv.index('--level')
        level = argv[i + 1]
        del argv[i:i + 2]
    if '--script' in argv:
        i = argv.index('--script')
        sid = argv[i + 1]
        del argv[i:i + 2]
        m = re.fullmatch(r'[Ll](-?\d+)/(\d+)', sid)  # L00/1816: the level's own copy
        if m:
            level, sid = m.group(1), m.group(2)
        per_level = json.load(open(os.path.join(DB, 'scripts.json'))).get(sid, {})
        if not per_level:
            raise SystemExit('script %s is in no level (`ed script %s`)' % (sid, sid))
        if level is not None:
            key = level if level in per_level else level.zfill(2)
            if key not in per_level:
                raise SystemExit('script %s is not in level %s: it is in %s' % (sid, level, ' '.join(sorted(per_level))))
            per_level = {key: per_level[key]}
        names = set()
        for row in per_level.values():
            names |= set(row['globals'])
        table = {l.split('\t')[0]: int(l.split('\t')[1], 16) for l in open(os.path.join(DB, 'natives.tsv'))}
        extra_roots = sorted(table[n] for n in names if n in table)
        if not extra_roots:
            raise SystemExit('script %s calls no known function' % sid)
        where = 'level %s' % ' '.join(sorted(per_level)) if len(per_level) == 1 else \
            'levels %s, merged (one level: --level N or L00/%s)' % (' '.join(sorted(per_level)), sid)
        print('script %s (%s) calls %d functions: %s' % (sid, where, len(extra_roots), ' '.join(sorted(n for n in names if n in table))))
    elif level is not None:
        raise SystemExit('--level goes with --script: ed needs --script ID --level N')
    if '--depth' in argv:
        i = argv.index('--depth')
        depth = int(argv[i + 1])
        del argv[i:i + 2]
    given_addr = bool(argv)
    if extra_roots and not argv:
        argv = ['%08X' % extra_roots[0]]
    addr, name = resolve(argv[0])
    q = name or argv[0]
    natives = {name: 1} if name else {}
    syms = disc.symbols()
    seen, found = Needs().reach([addr] + [a for a in extra_roots if a != addr], depth)
    if extra_roots and not given_addr:
        subject = 'script %s\'s %d functions' % (sid, len(extra_roots))
    elif extra_roots:
        subject = '0x%08X %s and script %s\'s %d functions' % (addr, q if q in natives else syms.get(addr, ''), sid, len(extra_roots))
    elif q in natives:
        subject = '0x%08X %s' % (addr, q)
    else:
        usa = syms.get(addr, '')
        # Decomp names carry the USA disc's address: fn_8008210C is this disc's 0x8007F254, the same function.
        subject = '0x%08X %s' % (addr, '%s (decomp name: its number is the USA address)' % usa if usa.startswith('fn_') else usa)
    print('%s: looked through %d functions, %d calls deep' % (subject.rstrip(), len(seen), depth))
    for line in hook_lines(addr) if not extra_roots or given_addr else []:
        print('hooked by the host:' + line)
    hooked_seen = sorted(a for a in seen if any(live(r) for r in hooks().get(a, [])))
    if hooked_seen:
        print('Of the functions looked through, %d have a live host hook (marked * below; logs, traces and hooks that lose '
              'their slot don\'t count; `ed fn ADDR` for each): %s' % (
            len(hooked_seen), ' '.join('%08X' % a for a in hooked_seen[:12]) + (' ...' if len(hooked_seen) > 12 else '')))
    if not found:
        print('None of the one-player data listed in tools/ed/player_data.tsv was found.')
        print('That list is what is known so far: a pointer the game passes around is not seen. Test it for player 2 before relying on this.')
        return
    print('It reaches this one-player data. To call it for player 2, the helper on the right must have their data in first.')
    print('This follows direct calls only: a script it runs (`ed needs --script ID`), a table, and work it leaves for later in the frame aren\'t followed.')
    print('"compare": a function branches on the word (it picks by it: the likely co-op bug when the players are apart);'
          ' "zero-test": it only asks whether the word is 0, -1 (none) or there; "read": it only loads it, maybe to pass it on. Depth 0 '
          'is the function itself.')
    picks = sorted({(row['cmp'], where, what) for (where, what, helper), row in found.items()
                    if row.get('cmp') is not None and row['cmp'] <= 1})
    if picks:
        print('Picks by, within one call: %s' % '; '.join('%s (%s, depth %d)' % (where, re.split(r'[.:;(]', what)[0].strip(), d)
                                                        for d, where, what in picks))
        import patterns
        hit = patterns.for_words(found)
        for p in patterns.PATTERNS:
            if p['key'] in hit:
                print('Pattern "%s" (%s): fix %s' % (p['name'], ', '.join(sorted(set(hit[p['key']]))), patterns.fix_text(p)))
    groups = {}
    for (where, what, helper), row in found.items():
        groups.setdefault(helper, []).append((where, what, row))
    for helper, rows in sorted(groups.items()):
        print('\n  %s' % helper)
        for where, what, row in sorted(rows, key=lambda x: x[2]['depth']):
            offs = sorted(row['offs'])
            at = '' if not offs or offs == [0] else ' at +%s' % ' +'.join('%d' % o for o in offs[:8]) + (' ...' if len(offs) > 8 else '')
            print('    %-12s %-5s %-48s depth %d, in %s%s' % (where, '/'.join(sorted(row['how'])), what[:48], row['depth'], ' '.join('%08X%s' % (x, '*' if any(live(r) for r in hooks().get(x, [])) else '') for x in row['in']), at))


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
