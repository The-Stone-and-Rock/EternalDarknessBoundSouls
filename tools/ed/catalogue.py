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
            return 'spawns chr %s model %s in room %s, tag %s' % tuple((args + ['?'] * 7)[k] for k in (0, 1, 2, 6))
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
        rows, others = [], collections.Counter()
        for i, l in enumerate(lines):
            in_test = bool(IF_GOTO.match(l))
            for name, args in calls_in(l):
                kind = KIND_OF.get(name)
                if kind is None:
                    p = self.d.prims.get(name)
                    if p and p['system'] not in ('math', 'debug', 'script'):
                        others[p['short']] += 1  # summed up in one row at the end
                    continue
                if in_test and kind not in ('flag', 'room_test', 'state', 'item'):
                    continue  # a call made only to test its answer is in the guard, not an action
                rows.append(dict(kind=kind, call=name, args=args, what=self.describe(name, args, level),
                                 line=i + 1, test=in_test, guard=g[i]))
        if others:
            rows.append(dict(kind='other', call=None, args=[], line=0, test=False, guard=[],
                             what='also: ' + ', '.join('%s%s' % (k, ' x%d' % v if v > 1 else '') for k, v in sorted(others.items()))))
        if follow:
            _seen = (_seen or set()) | {str(sid)}
            for r in list(rows):
                if r['call'] == 'fn73' and r['args'] and re.fullmatch(r'\d+', r['args'][0]) and r['args'][0] not in _seen:
                    sub = self.read(r['args'][0], level, follow=False, _seen=_seen) or []
                    for s in sub:
                        s['via'] = r['args'][0]
                    rows += sub
        return rows


def cmd_catalogue(argv):
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
        print('L%s/%s: %d rows' % (level, sid, len(rows)))
        for x in rows:
            print('  %4d %-10s %s%s%s' % (x['line'], x['kind'], x['what'], ('   [if %s]' % ' and '.join(x['guard'])) if x['guard'] else '',
                                          ('   (via %s)' % x['via']) if x.get('via') else ''))
        return
    raise SystemExit('ed catalogue --script L00/1816 (the generator for docs/game/catalogue comes next)')
