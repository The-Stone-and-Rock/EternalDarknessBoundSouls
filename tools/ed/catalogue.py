"""`ed catalogue`: every level, room and region with what it runs, generated from the disc data and the kit's readers.

This file: the per-script reader (what one level's copy of a script does, call by call, with the tests that guard each
action), the room and level structure, and the writer of docs/game/catalogue/. Everything comes from build/ed-db and
tools/ed; nothing from a running game, so story gates beyond the literal tests, timing, and what's only visible on
screen are not here, and a call whose argument is a variable shows the variable."""
import collections
import json
import os
import re

import disc
import patterns

DB = disc.DB
PATTERN_NAME = {p['key']: p['name'] for p in patterns.PATTERNS}
HERE = os.path.dirname(os.path.abspath(__file__))

def _call_names():
    """Every script function name in tools/ed/primitives.tsv (fnN, edN and the game's own names such as BKSpecialText),
    longest first; one- and two-letter names (H, V) are script variables as often as functions and are left out."""
    names = set()
    for line in open(os.path.join(HERE, 'primitives.tsv'), encoding='utf8'):
        if not line.startswith('#') and line.strip():
            n = line.split('\t')[0]
            if len(n) > 2:
                names.add(n)
    return sorted(names, key=len, reverse=True)


CALL = re.compile(r'\b(fn\d+|ed\d+|%s)\(' % '|'.join(re.escape(n) for n in _call_names() if not re.fullmatch(r'(fn|ed)\d+', n)))
IF_GOTO = re.compile(r'^\s*if (.+) then goto (L\d+) end\s*$')
GOTO = re.compile(r'^\s*goto (L\d+)\s*$')
LABEL = re.compile(r'^\s*::(L\d+)::\s*$')
NEGATE = {'~=': '==', '==': '~=', '<': '>=', '>=': '<', '>': '<=', '<=': '>'}

# The calls the catalogue names, by kind. Anything else a script calls is listed under "other" with its short name.
KINDS = {
    'prompt': {'ed301'}, 'words': {'ed11', 'ed12', 'ed13', 'ed97'}, 'examine': {'ed219', 'ed220', 'ed221'},
    'camera': {'fn8', 'fn10', 'fn134'}, 'door': {'fn66', 'fn71', 'ed29', 'ed162', 'ed163'},
    'flag': {'fn29', 'fn30', 'fn31'}, 'item': {'ed52', 'fn86', 'fn93', 'ed133', 'ed138', 'ed139', 'ed51', 'fn121', 'fn162'},
    'spawn': {'ed3'}, 'cinematic': {'ed55'}, 'film': {'ed83'}, 'input_mode': {'ed56', 'ed68'},
    'subtitle': {'EDStartSubtitleForEdward', 'EDStartSubtitlesForAlexClockPuzzle', 'EDStartSubtitlesForGhostMaid',
                 'EDStartSubtitlesForInsanityPhoneCall', 'EDStartSubtitlesForMikeDestroyThis', 'EDCleanUpSubtitles'},
    'hold': {'fn21', 'fn139', 'fn111', 'fn207', 'fn22', 'fn91', 'ed217', 'ed265', 'ed277'},
    'chapter': {'ed87', 'ed88', 'ed89', 'ed144', 'ed145', 'ed9', 'ed159'},
    'chain': {'fn73', 'fn36', 'ed177'}, 'state': {'fn1', 'fn35'}, 'room_test': {'fn25'}, 'sound': {'fn151', 'fn58'},
}
KIND_OF = {name: kind for kind, names in KINDS.items() for name in names}


class Data:
    """Everything the reader looks things up in, loaded once."""

    def __init__(self):
        j = lambda *p: json.load(open(os.path.join(DB, *p), encoding='utf8'))
        self.scripts = j('scripts.json')
        self.rooms = j('rooms.json')
        self.text = j('text.json')
        self.prims = {}
        for line in open(os.path.join(HERE, 'primitives.tsv'), encoding='utf8'):
            if not line.startswith('#') and line.strip():
                p = line.rstrip('\n').split('\t')
                self.prims[p[0]] = {'short': p[1], 'system': p[2]}
        import gameinfo
        self.hashes = gameinfo.hash_names()
        self.prompts = {p['id']: p['label'] for p in j('facts', 'verbs.json')['prompts']}
        self.items = {r['id']: r for r in j('facts', 'item_table.json')['rows']}
        cin = j('facts', 'cinematics.json')
        self.cinematics, self.films, self.modes = cin['cinematics'], cin['films'], cin['input_modes']
        self.levels = j('facts', 'levels.json')['levels']
        self._bodies = {}

    def body(self, sid, level):
        key = (str(sid), level)
        if key not in self._bodies:
            path = os.path.join(DB, 'scripts', 'L%s' % level, '%s.lua' % sid)
            self._bodies[key] = open(path, encoding='utf8').read() if os.path.exists(path) else None
        return self._bodies[key]

    def name(self, h):
        try:
            return self.hashes.get(int(h), {}).get('name') or str(h)
        except ValueError:
            return h

    def words(self, table, group, entry):
        row = self.text.get(table, {}).get('%s,%s' % (group, entry))
        return (row[1], re.sub(r'\\[a-z]\d*(\.\d+)?|\\n', ' ', row[2]).strip()) if row else None


def split_args(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


FOR_CALL = re.compile(r'^\s*(?:L\d+|\d+)\(-?\d+, -?\d+, (fn\d+|ed\d+|ED\w+|BK\w+)(?:, |\))')


def calls_in(line):
    """Each call on a line: (name, [args]), nested calls included, parentheses balanced. A call right after a for loop
    comes out of the decompiler as `L18(7, 1, fn91, 1)` (the loop's counter, limit and step left on its stack): it's
    read as fn91(1)."""
    m = FOR_CALL.match(line)
    if m:
        inner = line[line.index('(') + 1:line.rindex(')')]
        parts = split_args(inner)
        return [(m.group(1), parts[3:])] + calls_in(', '.join(parts[3:]))
    out = []
    for m in CALL.finditer(line):
        depth, j = 0, m.end() - 1
        for j in range(m.end() - 1, len(line)):
            if line[j] == '(':
                depth += 1
            elif line[j] == ')':
                depth -= 1
                if depth == 0:
                    break
        out.append((m.group(1), split_args(line[m.end():j])))
    return out


def negate(cond):
    cond = cond.strip()
    if cond.startswith('not ') and '(' not in cond[4:5] and ' and ' not in cond and ' or ' not in cond:
        return cond[4:].strip()
    m = re.fullmatch(r'(.+?)\s*(~=|==|<=|>=|<|>)\s*(.+)', cond)
    return '%s %s %s' % (m.group(1), NEGATE[m.group(2)], m.group(3)) if m else 'not (%s)' % cond.strip()


def guards(lines):
    """For each line: the nearest tests that decide whether it runs. Within its block (since the last label), each
    `if C then goto` passed means not C; the block's label is reached when one of the tests that jump to it holds (or
    from the line before it, when that isn't a jump). One level only: a test further up is not followed."""
    jumps_to = collections.defaultdict(list)
    for i, l in enumerate(lines):
        m = IF_GOTO.match(l)
        if m:
            jumps_to[m.group(2)].append(m.group(1).strip())
        m = GOTO.match(l)
        if m:
            jumps_to[m.group(1)].append(None)  # an unconditional jump from elsewhere
    out, block, entry = [], [], []
    for i, l in enumerate(lines):
        m = LABEL.match(l)
        if m:
            prev = lines[i - 1] if i else ''
            falls_in = i == 0 or not (GOTO.match(prev) or prev.strip() == 'return' or prev.strip().startswith('return '))
            conds = jumps_to.get(m.group(1), [])
            entry = [] if falls_in or None in conds else ([' or '.join('(%s)' % c for c in conds)] if len(conds) > 1 else conds)
            block = []
        out.append(entry + block)
        m = IF_GOTO.match(l)
        if m:
            block = block + [negate(m.group(1))]
    return out


RETURN = re.compile(r'^\s*return\b')


def join_states(a, b):
    """The rooms either of two room states allows."""
    if a is None:
        return b
    if b is None:
        return a
    (ka, sa), (kb, sb) = a, b
    if ka == kb == 'in':
        return ('in', sa | sb)
    if ka == kb == 'out':
        return ('out', sa & sb)
    i, o = (sa, sb) if ka == 'in' else (sb, sa)
    return ('out', o - i)


ROOM_TEST = re.compile(r'^\s*(fn25\(\)|\w+)\s*(==|~=)\s*(\d+)\s*$')


def room_test(cond, holding):
    """(op, room) when a test is `fn25() == N` / `~= N`, or the same of a variable that holds the room there."""
    t = ROOM_TEST.match(cond)
    if not t or (t.group(1) != 'fn25()' and t.group(1) not in holding):
        return None
    return t.group(2), int(t.group(3))


def room_holders(lines):
    """For each line, the variables that hold the room in play there: from `X = fn25()` to X's next assignment, in the
    file's order (a jump back over another assignment isn't followed; such a test then isn't read as a room test,
    which only widens the answer)."""
    holds, current = [], set()
    for l in lines:
        m = re.match(r'^\s*(?:local\s+)?(\w+)\s*=(?!=)\s*(.+?)\s*$', l)
        if m:
            current = (current | {m.group(1)}) if m.group(2) == 'fn25()' else (current - {m.group(1)})
        holds.append(set(current))
    return holds


def room_states(lines):
    """For each line, the rooms it can run in, from the script's own tests of the room in play (fn25, or a variable set
    from it), followed over its jumps: ('in', rooms) or ('out', rooms), or None for a line no path reaches. A test of
    anything else, or a compound test, leaves the rooms as they were, so the answer is never too narrow."""
    holds = room_holders(lines)
    labels = {m.group(1): i for i, l in enumerate(lines) for m in [LABEL.match(l)] if m}

    def apply(state, op, n):
        kind, s = state
        if op == '==':
            s2 = (s & {n}) if kind == 'in' else (frozenset() if n in s else frozenset({n}))
            return ('in', s2)
        return ('in', s - {n}) if kind == 'in' else ('out', s | {n})

    join = join_states

    def edges(i):
        l = lines[i]
        m = IF_GOTO.match(l)
        if m:
            t = room_test(m.group(1), holds[i])
            yes = (t[0], t[1]) if t else None
            no = (NEGATE[t[0]], t[1]) if t else None
            out = [(labels.get(m.group(2)), yes), (i + 1, no)]
        elif GOTO.match(l):
            out = [(labels.get(GOTO.match(l).group(1)), None)]
        elif RETURN.match(l):
            out = []
        else:
            out = [(i + 1, None)]
        return [(j, c) for j, c in out if j is not None and j < len(lines)]

    state = [None] * len(lines)
    if lines:
        state[0] = ('out', frozenset())
    work = [0] if lines else []
    while work:
        i = work.pop()
        for j, cond in edges(i):
            s = state[i] if cond is None else apply(state[i], *cond)
            if s[0] == 'in' and not s[1]:
                continue  # this edge can't be taken in any room
            new = join(state[j], s)
            if new != state[j]:
                state[j] = new
                work.append(j)
    return state


def runs_in(state, rid):
    if state is None:
        return False
    kind, s = state
    return rid in s if kind == 'in' else rid not in s


def short(what):
    return re.split(r'[.:;(]', what)[0].strip()


class Reader:
    def __init__(self, data=None):
        self.d = data or Data()
        self._needs, self._coop = None, {}

    def coop(self, name):
        """The per-player words a script function reaches (`ed needs`), each with how it's covered for player 2:
        hooked (a live host hook on the script function itself, or, for a word it branches on, on the function holding
        that compare; for one it only reads, on a function it's read in), keyed (a table looked up by an id the caller
        passes, right for either player when the id is), per run (written for every script run, whoever's), helper (the
        kit names a host helper that puts player 2's in: whether it's in force for this run is not known here), or one per
        game (player_data.tsv says none / one per game / see a failed attempt). picks: branched on within one call of the
        script function. flagged: picks, and hooked / keyed / per run don't apply."""
        if name in self._coop:
            return self._coop[name]
        n = self.needs()
        out = []
        if name in n.natives():
            out = self._coop_rows(n.native(name)[1], [n.natives()[name]])
        self._coop[name] = out
        return out

    def needs(self):
        import know
        if self._needs is None:
            self._needs = know.Needs()
        return self._needs

    def _coop_rows(self, found, roots):
        import know
        live = lambda a: any(know.live(h) for h in know.hooks().get(a, []))
        hosts = lambda a: sorted({h['host'] for h in know.hooks().get(a, []) if know.live(h)})
        out = []
        for (where, what, helper), row in sorted(found.items(), key=lambda kv: (kv[1]['depth'], kv[0][0])):
            picks = row['cmp'] is not None and row['cmp'] <= 1
            at = roots + (row.get('cmp_in') or [] if picks else row['in'])
            hooked = sorted({h for a in at if live(a) for h in hosts(a)})
            h = helper.strip().lower()
            status = ('hooked' if hooked else 'keyed' if h.startswith('keyed') else 'per run' if h.startswith('per run')
                      else 'one per game' if h.startswith(('none', 'one per game', '(see')) else 'helper')
            out.append(dict(word=short(what), where=where, helper=helper, how=sorted(row['how']), depth=row['depth'],
                            picks=picks, status=status, hooked_by=hooked,
                            flagged=picks and status not in ('hooked', 'keyed', 'per run')))
        return out

    def coop_row(self, x):
        """A row's co-op words: its call's, and for ed56/ed68 with a literal mode, also those of that input mode's own
        case, handlers and tick (facts/cinematics.json), which run later and aren't reached by direct calls."""
        if not x.get('call'):
            return []
        base = self.coop(x['call'])
        if x['call'] in ('ed56', 'ed68') and x['args'] and re.fullmatch(r'\d+', x['args'][0]) and int(x['args'][0]) < len(self.d.modes):
            key = ('mode', int(x['args'][0]))
            if key not in self._coop:
                m = self.d.modes[key[1]]
                addrs = [int(a, 16) for a in [m.get('case'), m.get('tick')] + m.get('handlers', []) + m.get('also', []) if a]
                found = self.needs().reach(addrs)[1] if addrs else {}
                rows = self._coop_rows(found, [])
                for w in rows:
                    # While a mode is player 2's, coop_input_mode_start keeps their close-up and pad with them (the pad
                    # routing reads their copy): the pad words in the mode's own code are theirs then.
                    if w['word'] in ('raw and processed pads, the input channels', 'the pad routine\'s state', 'control blocks'):
                        w['status'], w['flagged'] = 'routed', False
                    w['word'] = w['word'] + ' (in mode %d\'s own code)' % key[1]
                self._coop[key] = rows
            return base + self._coop[key]
        return base

    def describe(self, name, args, level):
        """One call, in words. Literal numbers are named where the kit can name them."""
        d = self.d
        num = lambda k: int(args[k]) if len(args) > k and re.fullmatch(r'-?\d+', args[k]) else None
        if name == 'ed301':
            p = num(1)
            return 'prompt %s (button %s)' % (d.prompts.get(p, args[1] if len(args) > 1 else '?'), args[0] if args else '?')
        if name in ('ed11', 'ed13'):
            w = d.words('boot', args[0], args[1]) if len(args) > 1 else None
            return 'words %s "%s"' % (w[0], w[1][:90]) if w else 'words %s' % ', '.join(args[:2])
        if name == 'ed12':
            w = d.words('room' + level, args[0], args[1]) if len(args) > 1 else None
            return 'room words %s "%s"' % (w[0], w[1][:90]) if w else 'room words %s' % ', '.join(args[:2])
        if name == 'ed97':
            return 'a person\'s words %s' % ', '.join(args[:3])
        if name in ('ed219', 'ed220', 'ed221'):
            w = d.words('room' + level, args[0], args[1]) if len(args) > 1 else None
            cam = d.name(args[2]) if name != 'ed220' and len(args) > 2 else None
            text = ('%s "%s"' % (w[0], w[1][:80])) if w else ('words by a variable (%s, %s)' % (args[0], args[1]) if len(args) > 1 else '')
            extra = ', unless %s is carried' % self.tagged(args[4]) if name == 'ed221' and len(args) > 4 else ''
            return 'EXAMINE: %s%s%s' % (text, (', close-up camera %s' % cam) if cam else '', extra)
        if name in ('fn8', 'fn10'):
            return 'camera %s%s' % (d.name(args[0]) if args else '?', ' (its path)' if name == 'fn10' else '')
        if name == 'fn134':
            return 'camera %s (key %s)' % ('saved' if args and args[0] == '1' else 'put back', args[1] if len(args) > 1 else '?')
        if name == 'fn66':
            room = num(0)
            after = ', then script %s' % args[4] if len(args) > 4 and args[4] not in ('0', '-1') else ''
            return 'door to room %s at %s, camera %s, transition %s%s' % (
                args[0] if args else '?', d.name(args[1]) if len(args) > 1 else '?', d.name(args[2]) if len(args) > 2 else '?',
                args[3] if len(args) > 3 else '?', after)
        if name == 'fn71':
            return 'room %s queued (loaded ahead)' % (args[0] if args else '?')
        if name == 'ed29':
            return 'back to the saved room'
        if name in ('ed162', 'ed163'):
            return 'teleport (%s)' % ('arrive' if name == 'ed162' else 'pick %s' % (args[0] if args else ''))
        if name == 'fn29':
            return 'flag %s %s' % (args[0] if args else '?', 'off' if len(args) > 1 and args[1] == '0' else 'on' if len(args) > 1 and args[1] == '1' else 'set to %s' % (args[1] if len(args) > 1 else '?'))
        if name == 'fn30':
            return 'asks flag %s' % (args[0] if args else '?')
        if name == 'fn31':
            return 'flips flag %s' % (args[0] if args else '?')
        if name == 'ed52':
            return 'finds %s%s' % (self.item(args[0]) if args else '?', (', then script %s' % args[1]) if len(args) > 1 and args[1] != '0' else '')
        if name == 'fn86':
            return 'gives %s to %s' % (self.item(args[1]) if len(args) > 1 else '?', {'-2': 'the player'}.get(args[0], args[0]) if args else '?')
        if name == 'fn93':
            return 'takes %s' % (args[0] if args else '?')
        if name == 'ed133':
            return '%s %s' % ('equips' if len(args) > 1 and args[1] != '0' else 'unequips', self.item(args[0]) if args else '?')
        if name == 'ed3':
            a = args + ['?'] * 7
            return 'spawns chr %s model %s in %s%s' % (a[0], a[1], 'the room in play' if a[2] == '-1' else 'room %s' % a[2],
                                                      '' if a[6] in ('-1', '?') else ', tag %s' % a[6])
        if name == 'ed55':
            n = num(0)
            c = d.cinematics[n] if n is not None and 0 <= n < len(d.cinematics) else None
            if not c:
                return 'cinematic %s' % (args[0] if args else '?')
            bits = ['level %s' % c['level'] if c['level'] is not None else None,
                    'room %s' % c['room'] if c['room'] is not None else None,
                    'queues room %s' % c['queued_room'] if c['room'] is None and c['queued_room'] is not None else None,
                    'then script %s' % c['end_script'] if c['end_script'] else None]
            return 'cinematic %s (%s)' % (args[0], ', '.join(b for b in bits if b))
        if name == 'ed83':
            n = num(0)
            f = d.films[n] if n is not None and 0 <= n < len(d.films) else None
            return 'film %s%s' % (args[0] if args else '?', (' (%s, then script %s)' % (f['avi'], f['end_script'])) if f else '')
        if name in ('ed56', 'ed68'):
            n = num(0)
            m = d.modes[n]['name'] if n is not None and 0 <= n < len(d.modes) else args[0] if args else '?'
            return 'input mode %s: %s %s' % (args[0] if args else '?', m, 'starts' if name == 'ed56' else 'ends')
        if name == 'fn73':
            return 'runs script %s next' % (args[0] if args else '?')
        other = ('script %s' % args[1]) if len(args) > 1 and name in ('fn1', 'fn35', 'fn36') else None
        if name == 'fn36':
            return '%s %s frames' % ((other + ' waits') if other else 'waits', args[0] if args else '?')
        if name == 'fn1':
            return '%s state to %s' % ((other + '\'s') if other else 'its', args[0] if args else '?')
        if name == 'fn35':
            return '%s state kept %s' % ((other + '\'s') if other else 'its', 'on' if args and args[0] != '0' else 'off')
        if name == 'fn25':
            return 'asks the room in play'
        p = d.prims.get(name)
        return '%s(%s)' % (p['short'] if p else name, ', '.join(args))

    def tagged(self, arg):
        """A placed thing by its tag (ed221's 5th argument; fn78(5, tag)): the items that carry that tag."""
        if not re.fullmatch(r'\d+', str(arg)):
            return 'thing tagged %s' % arg
        names = sorted({r['name'] or r['label'] for r in self.d.items.values() if int(arg) in (r.get('tags') or [])})
        return 'the thing tagged %s%s' % (arg, (' (%s)' % ', '.join(names)) if names else '')

    def item(self, arg):
        if re.fullmatch(r'\d+', str(arg)) and int(arg) in self.d.items:
            r = self.d.items[int(arg)]
            return '%s (%s)' % (r['name'] or r['label'], arg)
        return 'thing %s' % arg

    def read(self, sid, level, follow=True, _seen=None):
        """One level's copy of a script: a row per call worth naming, in order, with the tests guarding it.
        follow: also read the scripts it runs next (fn73) once, marked with via."""
        body = self.d.body(sid, level)
        if body is None:
            return None
        lines = body.split('\n')
        named = lambda m: self.d.hashes[int(m.group(0))]['name'] if int(m.group(0)) in self.d.hashes else m.group(0)
        g = [[re.sub(r'\b\d{6,}\b', named, c) for c in x] for x in guards(lines)]  # region and camera hashes by name
        rooms_at = room_states(lines)
        rows = []
        for i, l in enumerate(lines):
            in_test = bool(IF_GOTO.match(l))
            for name, args in calls_in(l):
                kind = KIND_OF.get(name)
                if kind is None:
                    p = self.d.prims.get(name)
                    if p and p['system'] not in ('math', 'debug', 'script'):
                        st = rooms_at[i]  # a row of its own, summed up in one line when shown
                        rows.append(dict(kind='other', call=name, args=args, what=p['short'], line=i + 1, test=in_test,
                                         guard=g[i], rooms=None if st is None else (st[0], sorted(st[1]))))
                    continue
                if in_test and kind not in ('flag', 'room_test', 'state', 'item'):
                    continue  # a call made only to test its answer is in the guard, not an action
                st = rooms_at[i]
                rows.append(dict(kind=kind, call=name, args=args, what=self.describe(name, args, level),
                                 line=i + 1, test=in_test, guard=g[i],
                                 rooms=None if st is None else (st[0], sorted(st[1]))))
        if follow:
            # Each script run next once, whatever number of fn73 calls name it; its rows can run in the rooms where any
            # of those calls can (joined), and nowhere else.
            targets = collections.OrderedDict()
            for r in rows:
                if r['call'] == 'fn73' and r['args'] and re.fullmatch(r'\d+', r['args'][0]) and r['args'][0] != str(sid):
                    st = None if r['rooms'] is None else (r['rooms'][0], frozenset(r['rooms'][1]))
                    targets[r['args'][0]] = join_states(targets.get(r['args'][0]), st) if r['args'][0] in targets else st
            for target, st in targets.items():
                sub = self.read(target, level, follow=False) or []
                for s in sub:
                    s['via'] = target
                    s['via_rooms'] = None if st is None else (st[0], sorted(st[1]))
                rows += sub
        return rows


FIELDS = ('enter', 'leave', 'stay', 'other_enter', 'other_leave', 'other_stay', 'cross_a', 'cross_b')
OUT = os.path.join(disc.ROOT, 'docs', 'game', 'catalogue')
CANT_KNOW = ('What this can\'t know: story gates beyond the literal tests shown (a flag set elsewhere, a variable from '
             'another script), timing (what runs a frame later, what waits), anything only seen on screen, and calls '
             'whose argument is a variable. The tests shown are the nearest ones in the script, not the whole path.')


# Which level's scripts a level runs: chapter 16's scripts, words and U_Level are level 00's (0x8003F4F4 turns 16 into
# 0; facts/levels.json, level 16's note), whatever build/ed-db/scripts/L16 holds.
SCRIPT_PACK = {'16': '00'}


def level_rooms(d, level):
    """A level's rooms: its Npcs placements, the door walk from them, and its start room (facts/levels.json). Level 12
    is the mansion once flag 787 is on: it has no Npcs file of its own, so level 00's rooms are its rooms too."""
    v = d.levels.get(level, {})
    rooms = set(v.get('placed_rooms_Npcs') or []) | set(v.get('rooms_by_door_walk_from_those') or [])
    if v.get('start_room_0x8023D738') is not None:
        rooms.add(v['start_room_0x8023D738'])
    if level == '12':
        rooms |= set(level_rooms(d, '00'))
    return sorted(r for r in rooms if str(r) in d.rooms)


CAMERA_LIKE = ('camera', 'zones')  # what a camera-only script may also call: camera moves and zone tests


def room_sources(d, rid):
    """{script id: [where it runs from]} for one room: its own arrival and leave scripts and each region's fields."""
    room = d.rooms[str(rid)]
    out = collections.OrderedDict()
    for what in ('arrival_script', 'leave_script'):
        if room.get(what):
            out.setdefault(str(room[what]), []).append('the room\'s %s' % what.split('_')[0])
    for reg in room['regions']:
        for f in FIELDS:
            if reg.get(f, -1) > 0:
                out.setdefault(str(reg[f]), []).append('%s %s' % (reg['name'], f))
    return out


ROOM_IS = re.compile(r'fn25\(\)\s*==\s*(\d+)')
ROOM_NOT = re.compile(r'fn25\(\)\s*~=\s*(\d+)')


def for_room(rows, rid):
    """A shared script's rows that can run in room rid, by the script's own tests of the room in play followed over
    its jumps (room_states). Rows with no line (the summed-up others) and rows from a script run next are kept."""
    def ok(state):
        return state is not None and runs_in((state[0], frozenset(state[1])), rid)
    # A row of a script run next runs where the fn73 calling it can (via_rooms) and where its own tests allow.
    return [x for x in rows if (not x.get('via') or ok(x.get('via_rooms'))) and (not x['line'] or ok(x.get('rooms')))]


class Units:
    """Pattern units: (pattern, unit id) -> the covers its rows had and where they are. A unit is uncovered if any of
    its rows is; else 'bracket' if any is (covered only when player 2's run goes through their bracket); else covered
    if any is; else n/a."""

    def __init__(self):
        self.u = collections.OrderedDict()

    def add(self, key, uid, cover, where):
        e = self.u.setdefault((key, uid), {'covers': set(), 'where': [], 'n': 0, 'unc': 0})
        e['covers'].add(cover)
        e['n'] += 1
        if cover == 'uncovered':
            e['unc'] += 1
            if len(e['where']) < 12:
                e['where'].append(where)  # the uncovered places, the ones to look at

    @staticmethod
    def resolve(covers):
        for c in ('uncovered', 'bracket', 'covered'):
            if c in covers:
                return c
        return 'n/a'

    @staticmethod
    def label(uid):
        if uid[0] == 'script':
            return 'script L%s/%s' % (uid[1], uid[2])
        return '%s(%s)' % (uid[1], ', '.join(uid[2])) if uid[2] or uid[1].startswith(('fn', 'ed')) else uid[1]

    def summary(self):
        out = collections.OrderedDict((p['key'], {'units': 0, 'covered': 0, 'bracket': 0, 'uncovered': 0, 'n/a': 0, 'top_uncovered': []})
                                      for p in patterns.PATTERNS)
        unc = collections.defaultdict(list)
        for (key, uid), e in self.u.items():
            c = self.resolve(e['covers'])
            out[key]['units'] += 1
            out[key][c] += 1
            if c == 'uncovered':
                unc[key].append((e['unc'], e['n'], self.label(uid), e['where']))
        for key, items in unc.items():
            items.sort(key=lambda t: (-t[0], t[2]))
            out[key]['top_uncovered'] = ['%s: uncovered at %d of its %d place%s, e.g. %s' % (lab, u, n, '' if n == 1 else 's', '; '.join(w[:2]))
                                         for u, n, lab, w in items]
        return out


def shown_rows(rows):
    """Rows as a page shows them: room tests left out (they're counted above the table) and the other calls summed
    in one 'also' line per script (and per script run next)."""
    out, others = [], collections.OrderedDict()
    for x in rows:
        if x['kind'] == 'room_test':
            continue
        if x['kind'] == 'other':
            others.setdefault(x.get('via'), collections.Counter())[x['what']] += 1
            continue
        out.append(x)
    for via, c in others.items():
        out.append(dict(kind='other', call=None, args=[], line=0, guard=[], via=via,
                        what='also: ' + ', '.join('%s%s' % (k, ' x%d' % v if v > 1 else '') for k, v in sorted(c.items()))))
    return out


def md(s):
    """Text safe in a Markdown table cell: | escaped, < and > as entities (the game's <player> would vanish as a tag)."""
    return str(s).replace('|', '\\|').replace('\n', ' ').replace('<', '&lt;').replace('>', '&gt;')


def tsv_cell(s):
    return str(s).replace('\t', ' ').replace('\n', ' ')


def generate(only_level=None, out=OUT):
    r = Reader()
    d = r.d
    os.makedirs(out, exist_ok=True)
    levels = [l for l in sorted(d.levels, key=lambda x: int(x)) if level_rooms(d, l) and os.path.isdir(os.path.join(DB, 'scripts', 'L' + l))]
    placed = set()
    index = []
    tsv_head = ['level', 'room', 'script', 'from', 'line', 'kind', 'call', 'what', 'if', 'via', 'coop_words', 'coop_flagged', 'patterns']
    all_flags = collections.Counter()
    all_units = Units()
    import herogates
    hero_gates = collections.defaultdict(list)  # (level, script) -> the bytecode's hero-gate compares
    for g in herogates.all_gates():
        if not g['path']:
            hero_gates[(g['level'], g['script'])].append(g)
    for level in levels:
        rooms = level_rooms(d, level)
        placed |= set(rooms)
        if only_level is not None and level != only_level:
            continue
        who = d.levels[level].get('who') or ''
        lines = ['# Level %s: %s' % (level, who), '',
                 'Generated by `python -I tools/ed/ed.py catalogue` from build/ed-db (the disc and the kit\'s readers); '
                 'don\'t edit by hand. %s' % CANT_KNOW, '']
        tsv = ['\t'.join(tsv_head)]
        n_rows = n_scripts = n_flag_rows = 0
        level_flags = collections.Counter()
        level_units = Units()
        body = []
        pack = SCRIPT_PACK.get(level, level)
        not_here = []
        for rid in rooms:
            sources = room_sources(d, rid)
            room = d.rooms[str(rid)]
            doors, cam_only, sections = [], [], []
            present = sum(1 for sid in sources if pack in d.scripts.get(sid, {}))
            if sources and not present:
                not_here.append(rid)  # named for this level on paper (a door walk), but none of its scripts is here
                continue
            for sid, where in sources.items():
                rows = r.read(sid, pack) if pack in d.scripts.get(sid, {}) else None
                if rows is None:
                    elsewhere = sorted(d.scripts.get(sid, {}))
                    sections.append('### Script %s (%s)\n\nNot in this level\'s scripts, so it runs nothing here%s.\n' % (
                        sid, '; '.join(where), (' (it is in %s)' % ' '.join('L' + x for x in elsewhere)) if elsewhere else ''))
                    continue
                n_scripts += 1
                total = len(rows)
                rows = for_room(rows, rid)
                dropped = total - len(rows)
                named = [x for x in rows if x['kind'] != 'other']
                camera_only = bool(named) and all(x['kind'] == 'camera' for x in named) and all(
                    d.prims.get(x['call'], {}).get('system') in CAMERA_LIKE for x in rows if x['kind'] == 'other') and all(
                    not w.startswith('the room\'s ') for w in where)
                script_words, script_flags, script_patterns = collections.OrderedDict(), collections.OrderedDict(), collections.OrderedDict()
                cam_ok = patterns.keep_camera(rows)
                gates = hero_gates.get((pack, int(sid)), [])
                if gates:  # a script-level pattern: the script itself tests the player's handle against 1 (its bytecode)
                    for g in gates:
                        # covered by ED_COOP_HERO_ID's chunk patch when it turns the player away; a two-path compare is left
                        cover = 'bracket' if g['turns_away'] else 'uncovered'
                        here = 'L%s/%s room %d pc %d (+0x%X%s)' % (pack, sid, rid, g['pc'], g['offset'], '' if g['turns_away'] else ', two paths')
                        level_units.add('hero', ('script', pack, sid), cover, here)
                        all_units.add('hero', ('script', pack, sid), cover, here)
                        script_patterns.setdefault('hero', set()).add(cover)
                for x in rows:
                    if x['kind'] == 'door' and x['call'] == 'fn66':
                        doors.append('%s (script %s%s)' % (x['args'][0] if x['args'] else '?', x.get('via') or sid,
                                                            (', if ' + ' and '.join(x['guard'])) if x['guard'] else ''))
                    co = r.coop_row(x)
                    x['coop_flagged'] = sorted({w['word'] for w in co if w['flagged']})
                    for w in co:
                        script_words.setdefault(w['word'], w['status'])
                        if w['flagged']:
                            script_flags.setdefault(w['word'], set()).add(x['what'] if x['kind'] != 'other' else x['what'])
                    if x['coop_flagged']:
                        n_flag_rows += 1
                        for w in x['coop_flagged']:
                            level_flags[re.sub(r" \(in mode \d+'s own code\)", " (in an input mode's own code)", w)] += 1
                    x['patterns'] = [t for t in patterns.for_row(x, co, where, d.prims) if t[0] != 'camera' or cam_ok]
                    here = 'L%s/%s room %d line %s%s' % (pack, sid, rid, x['line'], (' (via %s)' % x['via']) if x.get('via') else '')
                    for key, cover, unit in x['patterns']:
                        uid = ('script', pack, sid) if unit == ('script',) else unit
                        level_units.add(key, uid, cover, here)
                        all_units.add(key, uid, cover, here)
                        sc = script_patterns.setdefault(key, set())
                        sc.add(cover)
                    tsv.append('\t'.join(tsv_cell(v) for v in (level, rid, sid, '; '.join(where), x['line'], x['kind'], x['call'] or '',
                                                                x['what'], ' and '.join(x['guard']), x.get('via', ''),
                                                                '; '.join('%s [%s%s]' % (w['word'], w['status'], ', picks' if w['picks'] else '') for w in co),
                                                                '; '.join(x['coop_flagged']),
                                                                '; '.join('%s (%s)' % (k, c) for k, c, _ in x['patterns']))))
                n_rows += len(rows)
                if camera_only:  # its rows are in the TSV; the page sums it up in the room's head
                    cam_only.append((sid, where, sorted({x['what'] for x in named})))
                    continue
                part = ['### Script %s (%s)' % (sid, '; '.join(where)), '']
                tests = [x for x in rows if x['kind'] == 'room_test']
                shown = shown_rows(rows)
                if tests:
                    part += ['Picks by the room in play (fn25), %d test%s%s.' % (
                        len(tests), '' if len(tests) == 1 else 's', '' if not dropped else
                        '; %d rows of other rooms\' branches are left out (followed over the script\'s jumps)' % dropped), '']
                if script_flags:
                    part += ['Co-op, flagged (a per-player word branched on within one call, with no live host hook where '
                             'it\'s read): %s.' % '; '.join('**%s** (%s, by %s)' % (w, script_words[w], ', '.join(sorted(
                                 {short(c) for c in calls}))[:160]) for w, calls in script_flags.items()), '']
                if script_patterns:
                    part += ['Co-op patterns (`ed patterns`): %s.' % ', '.join(
                        '%s (%s)' % (PATTERN_NAME[k], Units.resolve(c)) for k, c in script_patterns.items()), '']
                if script_words:
                    part += ['Co-op, reaches: %s.' % ', '.join('%s [%s]' % (w, s) for w, s in script_words.items()), '']
                if not shown:
                    part.append('No other call the catalogue names.')
                else:
                    part += ['| line | kind | what | if | co-op flagged |', '|---|---|---|---|---|']
                    for x in shown:
                        flagged = x.get('coop_flagged') or sorted({w['word'] for o in rows if o['kind'] == 'other' and o.get('via') == x.get('via')
                                                                   for w in r.coop_row(o) if w['flagged']}) if x['kind'] == 'other' else x.get('coop_flagged', [])
                        part.append('| %s | %s | %s%s | %s | %s |' % (x['line'] or '', x['kind'], md(x['what']),
                                                                     (' (via %s)' % x['via']) if x.get('via') else '', md(' and '.join(x['guard'])),
                                                                     md(', '.join(flagged))))
                sections.append('\n'.join(part) + '\n')
            head = ['## Room %d' % rid, '']
            head.append('Its own scripts: arrival %s, leave %s. Regions: %d.' % (
                room.get('arrival_script') or 'none', room.get('leave_script') or 'none', len(room['regions'])))
            if doors:
                head.append('Doors out (fn66): %s.' % '; '.join(sorted(set(doors))))
            if cam_only:
                head.append('Camera-only scripts (%d: only camera moves and zone tests; their rows are in the TSV): %s.' % (len(cam_only), '; '.join(
                    '%s from %s: %s' % (sid, ', '.join(w.split(' ')[0] for w in where), ', '.join(c.replace('camera ', '') for c in cams))
                    for sid, where, cams in cam_only)))
            body.append('\n'.join(head) + '\n\n' + '\n'.join(sections))
        lines.append('Rooms %d, scripts read %d, rows %d. The TSV beside this file has every row.' % (len(rooms), n_scripts, n_rows))
        lines.append('')
        if pack != level:
            lines.append('This level runs level %s\'s scripts (facts/levels.json: 0x8003F4F4 turns %s into %s), so those are '
                         'the copies read here.' % (pack, int(level), int(pack)))
            lines.append('')
        if not_here:
            lines.append('Rooms this level reaches on paper (its door walk) but whose scripts are none of them in this level, so '
                         'nothing of theirs runs here and they have no section: %s. A door to them may stand behind a test of '
                         'another chapter (ed143).' % ', '.join(map(str, not_here)))
            lines.append('')
        lines.append('Co-op: %d rows flagged (a call that branches on a per-player word within one call, with no live host '
                     'hook where that word is read; `ed needs` and tools/ed/player_data.tsv for the words). Most often: %s.' % (
                         n_flag_rows, ', '.join('%s %d' % (w, c) for w, c in level_flags.most_common(8)) or 'none'))
        lines.append('A flag is a place to look, not a bug found: the host can still cover it another way (a helper around the '
                     'whole script run, a swap around the caller), and a word that is one per game may be fine for both.')
        lines.append('')
        all_flags.update(level_flags)
        summ = level_units.summary()
        lines.append('Co-op patterns (`ed patterns`; counted per script or per distinct call: units, of them uncovered): %s.' % ', '.join(
            '%s %d (%d)' % (PATTERN_NAME[k], s['units'], s['uncovered']) for k, s in summ.items() if s['units']))
        lines.append('')
        lines.append('Rooms: ' + ', '.join('[%d](#room-%d)' % (x, x) for x in rooms if x not in not_here))
        lines.append('')
        open(os.path.join(out, 'level-%s.md' % level), 'w', encoding='utf8', newline='\n').write('\n'.join(lines) + '\n' + '\n'.join(body))
        open(os.path.join(out, 'level-%s.tsv' % level), 'w', encoding='utf8', newline='\n').write('\n'.join(tsv) + '\n')
        index.append((level, who, len(rooms), n_scripts, n_rows, n_flag_rows))
        print('level %s: %d rooms, %d scripts, %d rows' % (level, len(rooms), n_scripts, n_rows), flush=True)
    if only_level is not None:
        return
    # Rooms no level reaches: their scripts, and the levels that hold each.
    unplaced = sorted(int(x) for x in d.rooms if int(x) not in placed)
    up = ['# Rooms in no level', '', 'Rooms the catalogue can\'t give a level: no level with scripts names them in its Npcs '
          'placements, door walk or start room (facts/levels.json; room 0 is only the start word of level 17, which has no '
          'scripts), so which level\'s copy of a script runs there is not known here. Each is reached some other way (a '
          'cinematic, a film, a script\'s own room change) or not at all.', '']
    for rid in unplaced:
        src = room_sources(d, rid)
        up.append('## Room %d' % rid)
        up.append('')
        up.append('Regions %d; arrival %s, leave %s.' % (len(d.rooms[str(rid)]['regions']), d.rooms[str(rid)].get('arrival_script') or 'none',
                                                       d.rooms[str(rid)].get('leave_script') or 'none'))
        for sid, where in src.items():
            up.append('- script %s (%s): in %s' % (sid, '; '.join(where), ' '.join('L' + x for x in sorted(d.scripts.get(sid, {}))) or 'no level'))
        cins = [c['n'] for c in d.cinematics if c['room'] == rid]
        if cins:
            up.append('- cinematics that play here: %s' % ' '.join(map(str, cins)))
        up.append('')
    open(os.path.join(out, 'unplaced.md'), 'w', encoding='utf8', newline='\n').write('\n'.join(up) + '\n')
    ix = ['# The catalogue: every level, room and region with what it runs', '',
          'Generated by `python -I tools/ed/ed.py catalogue` (tools/ed/catalogue.py) from build/ed-db: the disc and the kit\'s '
          'readers, no game run. A level\'s page lists each room\'s own scripts, its doors, and every region\'s scripts call by '
          'call with the tests that guard each call; camera-only scripts are summed up per room. The TSV beside each page has '
          'every row. %s' % CANT_KNOW, '',
          '| level | who | rooms | scripts | rows | co-op flagged | page |', '|---|---|---|---|---|---|---|']
    for level, who, nr, ns, nrow, nflag in index:
        ix.append('| %s | %s | %d | %d | %d | %d | [level-%s.md](level-%s.md), [tsv](level-%s.tsv) |' % (
            level, md(who), nr, ns, nrow, nflag, level, level, level))
    summ = all_units.summary()
    json.dump(summ, open(os.path.join(out, 'patterns.json'), 'w', encoding='utf8', newline='\n'), indent=1)
    ix += ['', '## The known co-op patterns', '',
           'Each row of the catalogue is tagged with the known patterns it shows (TSV column `patterns`). They are counted '
           'in units, per script (a level\'s copy) or per distinct call (the call and its literal arguments, over every '
           'level), so a long script or a repeated call counts once. A unit is covered (a fix the kit can see applies), '
           'uncovered (none it can see), or n/a (whether it\'s covered is a run-time fact). The fix is what already exists '
           'for that pattern in runtime/host/hle.c, with the commit that added it. `ed patterns KEY` lists the uncovered '
           'units with where each is met; patterns.json beside this file has them all.', '',
           '| pattern | counted per | units | covered | bracket | uncovered | n/a | what it is | fix in hle.c | how far the check can be trusted |',
           '|---|---|---|---|---|---|---|---|---|---|']
    for p in patterns.PATTERNS:
        s = summ[p['key']]
        ix.append('| %s | %s | %d | %d | %d | %d | %d | %s | %s | %s |' % (
            p['name'], p['unit'], s['units'], s['covered'], s['bracket'], s['uncovered'], s['n/a'], md(p['what']),
            md(patterns.fix_text(p)), md(p['trust'])))
    ix += ['', '"bracket": covered only when player 2\'s run goes through the bracket around their own runs (coop_use_start: '
           'their player, room, zones\' words and script words in), as a region\'s own enter/leave/stay script does for '
           'whoever crosses it; "uncovered" for those words means the run is outside it: a region\'s other_* script, or a '
           'script run next.']
    ix += ['', 'Run-time only, no check from the disc: %s.' % patterns.RUN_TIME_ONLY, '']
    ix += ['', 'Co-op flagged rows by word, over every level: %s.' % ', '.join('%s %d' % (w, c) for w, c in all_flags.most_common()),
           'A row is flagged when its call branches on a per-player word within one call and no live host hook sits on the '
           'function holding that compare. It\'s a place to look, not a bug found.', '',
           'Statuses in the co-op columns: **hooked** (a live host hook on the script function or the function holding the '
           'compare: logs, traces, hooks that lose their slot and hooks that act for one caller only don\'t count), '
           '**keyed** (a table looked up by an id the caller passes), **per run** (written for every script run, '
           'whoever\'s), **routed** (an input mode\'s pad while the mode is player 2\'s: coop_input_mode_start), **helper** '
           '(tools/ed/player_data.tsv names a host helper that puts player 2\'s in), **one per game** (no copy of player '
           '2\'s).', '',
           'The two biggest, the room in play and the script words, are put in for player 2 by the brackets around their '
           'own runs (coop_use_start, coop_run_script\'s first block, coop_stay_room_in with coop_states_swap). A flag there '
           'matters where a run happens outside those brackets: a region\'s other_enter script, a script queued for a later '
           'frame, a timer, or a room\'s arrival script run for player 2 with player 1 still the character. Whether a '
           'bracket is in force on a given run is a run-time fact the catalogue can\'t see.']
    ix += ['', 'Rooms in no level (%d): [unplaced.md](unplaced.md).' % len(unplaced), '',
           'Levels with no rooms or scripts of their own here: %s (see `ed level`).' % ', '.join(
               l for l in sorted(d.levels, key=int) if l not in levels)]
    open(os.path.join(out, 'index.md'), 'w', encoding='utf8', newline='\n').write('\n'.join(ix) + '\n')
    print('index: %d levels; %d rooms in no level' % (len(index), len(unplaced)))


def cmd_catalogue(argv):
    if not argv or argv[0] == '--level' and '--script' not in argv:
        return generate(argv[1].zfill(2) if argv else None)
    """ed catalogue --script L00/1816 | --script ID --level N: what one script does, call by call, with its guards."""
    if '--script' in argv:
        i = argv.index('--script')
        sid = argv[i + 1]
        level = argv[argv.index('--level') + 1].zfill(2) if '--level' in argv else None
        m = re.fullmatch(r'[Ll](-?\d+)/(\d+)', sid)
        if m:
            level, sid = m.group(1).zfill(2) if not m.group(1).startswith('-') else m.group(1), m.group(2)
        r = Reader()
        levels = sorted(r.d.scripts.get(sid, {}))
        if level is None:
            if not levels:
                raise SystemExit('script %s is in no level' % sid)
            level = levels[0]
        rows = r.read(sid, level)
        if rows is None:
            raise SystemExit('script %s is not in level %s: it is in %s' % (sid, level, ' '.join(levels)))
        room = int(argv[argv.index('--room') + 1]) if '--room' in argv else None
        if room is not None:
            rows = for_room(rows, room)
        tests = sum(1 for x in rows if x['kind'] == 'room_test')
        print('L%s/%s%s: %d rows%s' % (level, sid, (' in room %d' % room) if room is not None else '', len(rows),
                                      ('; picks by the room in play, %d tests' % tests) if tests else ''))
        for x in shown_rows(rows):
            print('  %4s %-10s %s%s%s' % (x['line'] or '', x['kind'], x['what'], ('   [if %s]' % ' and '.join(x['guard'])) if x['guard'] else '',
                                          ('   (via %s)' % x['via']) if x.get('via') else ''))
        return
    raise SystemExit('ed catalogue [--level N] (writes docs/game/catalogue/), or ed catalogue --script L00/ID [--room N]')
