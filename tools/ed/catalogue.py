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

DB = disc.DB
HERE = os.path.dirname(os.path.abspath(__file__))

CALL = re.compile(r'\b(fn\d+|ed\d+|ED\w+|StopAudioStream)\(')
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


def calls_in(line):
    """Each call on a line: (name, [args]), nested calls included, parentheses balanced."""
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


def room_states(lines):
    """For each line, the rooms it can run in, from the script's own tests of the room in play (fn25, or a variable set
    from it), followed over its jumps: ('in', rooms) or ('out', rooms), or None for a line no path reaches. A test of
    anything else, or a compound test, leaves the rooms as they were, so the answer is never too narrow."""
    alias = {m.group(1) for l in lines for m in [re.match(r'^\s*(?:local\s+)?(\w+)\s*=\s*fn25\(\)\s*$', l)] if m}
    room_word = r'(?:fn25\(\)|%s)' % '|'.join(map(re.escape, sorted(alias))) if alias else r'fn25\(\)'
    test = re.compile(r'^\s*%s\s*(==|~=)\s*(\d+)\s*$' % room_word)
    labels = {m.group(1): i for i, l in enumerate(lines) for m in [LABEL.match(l)] if m}

    def apply(state, op, n):
        kind, s = state
        if op == '==':
            s2 = (s & {n}) if kind == 'in' else (frozenset() if n in s else frozenset({n}))
            return ('in', s2)
        return ('in', s - {n}) if kind == 'in' else ('out', s | {n})

    def join(a, b):
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

    def edges(i):
        l = lines[i]
        m = IF_GOTO.match(l)
        if m:
            t = test.match(m.group(1))
            yes = (t.group(1), int(t.group(2))) if t else None
            no = (NEGATE[t.group(1)], int(t.group(2))) if t else None
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


class Reader:
    def __init__(self, data=None):
        self.d = data or Data()

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
            extra = ', unless %s is carried' % self.item(args[4]) if name == 'ed221' and len(args) > 4 else ''
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
            return 'cinematic %s%s' % (args[0] if args else '?', (' (level %s room %s, then script %s)' % (c['level'], c['room'], c['end_script'])) if c else '')
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
        if name == 'fn36':
            return 'waits %s frames' % (args[0] if args else '?')
        if name == 'fn1':
            return 'its state to %s' % (args[0] if args else '?')
        if name == 'fn25':
            return 'asks the room in play'
        p = d.prims.get(name)
        return '%s(%s)' % (p['short'] if p else name, ', '.join(args))

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
            _seen = (_seen or set()) | {str(sid)}
            for r in list(rows):
                if r['call'] == 'fn73' and r['args'] and re.fullmatch(r'\d+', r['args'][0]) and r['args'][0] not in _seen:
                    sub = self.read(r['args'][0], level, follow=False, _seen=_seen) or []
                    for s in sub:
                        s['via'] = r['args'][0]
                    rows += sub
        return rows


FIELDS = ('enter', 'leave', 'stay', 'other_enter', 'other_leave', 'other_stay', 'cross_a', 'cross_b')
OUT = os.path.join(disc.ROOT, 'docs', 'game', 'catalogue')
CANT_KNOW = ('What this can\'t know: story gates beyond the literal tests shown (a flag set elsewhere, a variable from '
             'another script), timing (what runs a frame later, what waits), anything only seen on screen, and calls '
             'whose argument is a variable. The tests shown are the nearest ones in the script, not the whole path.')


def level_rooms(d, level):
    """A level's rooms: its Npcs placements, the door walk from them, and its start room (facts/levels.json)."""
    v = d.levels.get(level, {})
    rooms = set(v.get('placed_rooms_Npcs') or []) | set(v.get('rooms_by_door_walk_from_those') or [])
    if v.get('start_room_0x8023D738') is not None:
        rooms.add(v['start_room_0x8023D738'])
    return sorted(r for r in rooms if str(r) in d.rooms)


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
    return [x for x in rows if x.get('via') or not x['line'] or x.get('rooms') is None and False or
            (x.get('rooms') is not None and runs_in((x['rooms'][0], frozenset(x['rooms'][1])), rid))]


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
    return str(s).replace('|', '\\|').replace('\n', ' ')


def generate(only_level=None, out=OUT):
    r = Reader()
    d = r.d
    os.makedirs(out, exist_ok=True)
    levels = [l for l in sorted(d.levels, key=lambda x: int(x)) if level_rooms(d, l) and os.path.isdir(os.path.join(DB, 'scripts', 'L' + l))]
    placed = set()
    index = []
    tsv_head = ['level', 'room', 'script', 'from', 'line', 'kind', 'call', 'what', 'if', 'via']
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
        n_rows = n_scripts = 0
        body = []
        for rid in rooms:
            sources = room_sources(d, rid)
            room = d.rooms[str(rid)]
            doors, cam_only, sections = [], [], []
            for sid, where in sources.items():
                rows = r.read(sid, level) if level in d.scripts.get(sid, {}) else None
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
                if named and all(x['kind'] == 'camera' for x in named) and all('arrival' not in w and 'leave' not in w for w in where):
                    cam_only.append((sid, where, sorted({x['what'] for x in named})))
                    continue
                for x in rows:
                    if x['kind'] == 'door' and x['call'] == 'fn66':
                        doors.append('%s (script %s%s)' % (x['args'][0] if x['args'] else '?', sid, (', if ' + ' and '.join(x['guard'])) if x['guard'] else ''))
                    tsv.append('\t'.join(md(v) for v in (level, rid, sid, '; '.join(where), x['line'], x['kind'], x['call'] or '',
                                                          x['what'], ' and '.join(x['guard']), x.get('via', ''))))
                n_rows += len(rows)
                part = ['### Script %s (%s)' % (sid, '; '.join(where)), '']
                tests = [x for x in rows if x['kind'] == 'room_test']
                shown = shown_rows(rows)
                if tests:
                    part += ['Picks by the room in play (fn25), %d test%s%s.' % (
                        len(tests), '' if len(tests) == 1 else 's', '' if not dropped else
                        '; %d rows of other rooms\' branches are left out (followed over the script\'s jumps)' % dropped), '']
                if not shown:
                    part.append('No other call the catalogue names.')
                else:
                    part += ['| line | kind | what | if |', '|---|---|---|---|']
                    for x in shown:
                        part.append('| %s | %s | %s%s | %s |' % (x['line'] or '', x['kind'], md(x['what']),
                                                                (' (via %s)' % x['via']) if x.get('via') else '', md(' and '.join(x['guard']))))
                sections.append('\n'.join(part) + '\n')
            head = ['## Room %d' % rid, '']
            head.append('Its own scripts: arrival %s, leave %s. Regions: %d.' % (
                room.get('arrival_script') or 'none', room.get('leave_script') or 'none', len(room['regions'])))
            if doors:
                head.append('Doors out (fn66): %s.' % '; '.join(sorted(set(doors))))
            if cam_only:
                head.append('Camera-only scripts (%d, not listed below): %s.' % (len(cam_only), '; '.join(
                    '%s from %s: %s' % (sid, ', '.join(w.split(' ')[0] for w in where), ', '.join(c.replace('camera ', '') for c in cams))
                    for sid, where, cams in cam_only)))
            body.append('\n'.join(head) + '\n\n' + '\n'.join(sections))
        lines.append('Rooms %d, scripts read %d, rows %d. The TSV beside this file has every row.' % (len(rooms), n_scripts, n_rows))
        lines.append('')
        lines.append('Rooms: ' + ', '.join('[%d](#room-%d)' % (x, x) for x in rooms))
        lines.append('')
        open(os.path.join(out, 'level-%s.md' % level), 'w', encoding='utf8', newline='\n').write('\n'.join(lines) + '\n' + '\n'.join(body))
        open(os.path.join(out, 'level-%s.tsv' % level), 'w', encoding='utf8', newline='\n').write('\n'.join(tsv) + '\n')
        index.append((level, who, len(rooms), n_scripts, n_rows))
        print('level %s: %d rooms, %d scripts, %d rows' % (level, len(rooms), n_scripts, n_rows), flush=True)
    if only_level is not None:
        return
    # Rooms no level reaches: their scripts, and the levels that hold each.
    unplaced = sorted(int(x) for x in d.rooms if int(x) not in placed)
    up = ['# Rooms in no level', '', 'Rooms the catalogue can\'t give a level: no level\'s Npcs placements, door walk or '
          'start room names them (facts/levels.json), so which level\'s copy of a script runs there is not known here. '
          'Each is reached some other way (a cinematic, a film, a script\'s own room change) or not at all.', '']
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
          '| level | who | rooms | scripts | rows | page |', '|---|---|---|---|---|---|']
    for level, who, nr, ns, nrow in index:
        ix.append('| %s | %s | %d | %d | %d | [level-%s.md](level-%s.md), [tsv](level-%s.tsv) |' % (level, md(who), nr, ns, nrow, level, level, level))
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
