#!/usr/bin/env python3
"""Every player action: counts and asker sites from the disc's scripts and the game's code.
Run from the repo root: python -I tools/ed/facts/verbs.py
Writes build/ed-db/facts/verbs.json and build/ed-db/facts/verbs.md. Nothing here runs the game."""
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import h
from h import ADDR, IDX, TXT, callers, fnstart

ROOT = h.ROOT
DB = os.path.join(ROOT, 'build', 'ed-db')
D = h._dol()
text = json.load(open(os.path.join(DB, 'text.json'), encoding='utf8'))


def clean(words):
    w = re.sub(r'\\[is][0-9.]+', ' ', words).replace('\\n', ' ')
    return re.sub(r'\s+', ' ', w).strip()


# ---- 1. the scripts
if not os.path.exists(os.path.join(ROOT, 'build', 'ed-db', 'facts', 'regions.json')):
    sys.exit('Run python -I tools/ed/facts/verbs_regions.py first: this needs regions.json')
REGIONS = json.load(open(os.path.join(ROOT, 'build', 'ed-db', 'facts', 'regions.json')))
SUMMARY = json.load(open(os.path.join(ROOT, 'build', 'ed-db', 'facts', 'regions_summary.json')))
PLACED = {}
for _room, _v in REGIONS.items():
    for _r in _v['regions']:
        for _f in ('enter', 'leave', 'stay', 'other_enter', 'other_leave', 'other_stay', 'cross_a', 'cross_b'):
            if _r.get(_f, 0) > 0:
                PLACED.setdefault(_r[_f], {}).setdefault(_f, 0)
                PLACED[_r[_f]][_f] += 1


def states_by_line(body):
    """Each line's state: inside an `if fn0() ~= N then goto L end` up to its label."""
    lines = body.split('\n')
    st = [None] * len(lines)
    cur = end = None
    for i, line in enumerate(lines):
        m = re.match(r'if fn0\(\) ~= (\d+) then goto (L\d+) end', line)
        if m:
            cur, end = int(m.group(1)), m.group(2)
        elif end and line.strip() == '::%s::' % end:
            cur = end = None
        st[i] = cur
    return lines, st


def door_file(pid, body, sid, name):
    d = door.setdefault(pid, {'files': 0, 'no_fn66': [], 'fn66_with_the_ask': [], 'press_word_holds_fn66': 0, 'other': [], 'simple': 0, 'placed': {}, 'not_placed': []})
    lines, st = states_by_line(body)
    d['files'] += 1
    for f in PLACED.get(sid, {}):
        d['placed'][f] = d['placed'].get(f, 0) + 1
    if sid not in PLACED:
        d['not_placed'].append(name)
    tok = 'ed301(2, %d)' % pid
    asks = [i for i, line in enumerate(lines) if tok in line]
    goes = [i for i, line in enumerate(lines) if 'fn66(' in line]
    if not goes:
        d['no_fn66'].append(name)
        return
    if any(st[a] == st[g] and g > a for a in asks for g in goes):
        d['fn66_with_the_ask'].append(name)
    a = asks[0]
    words = []
    for i in range(a + 1, len(lines)):
        if st[i] != st[a]:
            break
        m = re.search(r'\bfn1\((\d+)\)', lines[i])
        if m:
            words.append(int(m.group(1)))
    where = {st[g] for g in goes}
    if where & set(words):
        d['press_word_holds_fn66'] += 1
    else:
        d['other'].append(name)
    if st[a] == 0 and words == [1] and where == {1}:
        d['simple'] += 1


ask = {}          # id -> calls, files, ids
door = {}         # id -> how its files reach a room change; see door_file
word100 = {'test': 0, 'set_themselves': 0, 'no_setter': []}
ed52 = {'calls': 0, 'files': 0, 'with_prompt_19': 0, 'without_prompt_19': 0}
fn25_values = set()
fn35_files = 0
neg_entry = {'ed219': 0, 'ed220': 0}
go_room = {}      # id -> files that also call fn66
exam = {'ed219': [], 'ed220': [], 'ed221': []}
nonliteral = 0
files = sorted(glob.glob(os.path.join(DB, 'scripts', 'L*', '*.lua')))
for path in files:
    body = open(path, encoding='utf8', errors='replace').read()
    level, sid = os.path.basename(os.path.dirname(path)), os.path.basename(path)[:-4]
    calls = re.findall(r'\bed301\((\d+), (\d+)\)', body)
    for pid in (1, 3, 4, 10):
        if 'ed301(2, %d)' % pid in body:
            door_file(pid, body, int(sid), '%s/%s' % (level, sid))
    if re.search(r'fn0\(\) (~=|==) 100\b', body):
        word100['test'] += 1
        if 'fn1(100)' in body:
            word100['set_themselves'] += 1
        else:
            word100['no_setter'].append('%s/%s' % (level, sid))
    n52 = len(re.findall(r'\bed52\(', body))
    if n52:
        ed52['calls'] += n52
        ed52['files'] += 1
        ed52['with_prompt_19' if 'ed301(2, 19)' in body else 'without_prompt_19'] += 1
    fn25_values.update(int(v) for v in re.findall(r'fn25\(\) (?:~=|==) (\d+)', body))
    if re.search(r'\bfn35\(', body):
        fn35_files += 1
    for nm in neg_entry:
        neg_entry[nm] += len(re.findall(r'\b%s\(\s*[^,()]+,\s*-\d' % nm, body))
    if len(re.findall(r'\bed301\(', body)) != len(calls):
        nonliteral += 1
    for button, pid in calls:
        pid = int(pid)
        a = ask.setdefault(pid, {'calls': 0, 'files': set(), 'ids': set(), 'buttons': set()})
        a['calls'] += 1
        a['files'].add('%s/%s' % (level, sid))
        a['ids'].add(int(sid))
        a['buttons'].add(int(button))
        if 'fn66(' in body:
            go_room.setdefault(pid, set()).add('%s/%s' % (level, sid))
    for name in exam:
        for m in re.finditer(r'\b%s\(([^()\n]*)\)' % name, body):
            exam[name].append(('%s/%s' % (level, sid), [x.strip() for x in m.group(1).split(',')]))

exam_stats = {}
for name, rows in exam.items():
    s = {'calls': len(rows), 'files': len({f for f, _ in rows}), 'ids': len({f.split('/')[1] for f, _ in rows})}
    if name == 'ed219':
        s['arg4_flags'] = {}
        for _, a in rows:
            s['arg4_flags'][a[3] if len(a) > 3 else '?'] = s['arg4_flags'].get(a[3] if len(a) > 3 else '?', 0) + 1
    if name == 'ed221':
        s['arg4_flags'] = {}
        s['arg5_nonzero'] = s['arg5_zero'] = 0
        s['arg6'] = {}
        for _, a in rows:
            if len(a) != 6:
                continue
            s['arg4_flags'][a[3]] = s['arg4_flags'].get(a[3], 0) + 1
            if a[4] == '0':
                s['arg5_zero'] += 1
            else:
                s['arg5_nonzero'] += 1
            s['arg6'][a[5]] = s['arg6'].get(a[5], 0) + 1
    exam_stats[name] = s


# ---- 2. the code's own asks of 0x80117404
def regs_before(c, back=18):
    r = {}
    i = IDX[c]
    for k in range(i - back, i):
        t = TXT[k]
        m = re.match(r'li\s+(r\d+), (-?\d+)$', t)
        if m:
            r[m.group(1)] = int(m.group(2))
            continue
        m = re.match(r'(?:lwz|lwzx|or|add|lha|lhz)\S*\s+(r\d+),', t)
        if m:
            r[m.group(1)] = None
    return r


FUNCTION_NOTE = {
    0x8000A424: 'ed301',
    0x80017154: 'ed220',
    0x80017324: 'ed219',
    0x80017604: 'ed221',
    0x80024D78: 'the close-up stepper',
    0x80053E4C: 'kind 2, state 56',
    0x800566AC: 'kind 2, state 52',
    0x8005713C: 'kind 2, state 67',
    0x80057790: 'kind 2, state 69',
    0x80058DD8: 'kind 2 event 147',
    0x80060EC8: 'kind 19 behaviour, its tick (event 3)',
    0x8007E41C: 'interface ed56(9,10) tick',
    0x8007E508: 'ed68(0) at 0x8007A9AC, and 0x80083598; which n is not settled',
    0x8007E9BC: 'interface ed56(1) tick',
    0x8007F254: 'interface ed56(8): called at 0x8007B504, rows 2 and 3 of its table',
    0x8007F610: 'interface ed56(11) tick',
    0x800810D0: 'interface ed56(3) tick',
    0x800C030C: "a thing's own, kind 2 state 1 event 3",
    0x801EF074: 'camera pan/tilt, from 0x800083C8',
}
native = []
for c in callers(0x80117404):
    f = fnstart(c)
    if f == 0x80117404:
        continue  # its own move from slot 2 to slot 4
    r = regs_before(c)
    row = {'site': '0x%08X' % c, 'function': '0x%08X' % f, 'where': FUNCTION_NOTE.get(f, ''),
           'button': r.get('r3'), 'id': r.get('r4'), 'script': r.get('r5'), 'arg4': r.get('r6'),
           'priority': r.get('r7'), 'slot': r.get('r8'), 'flag': r.get('r9')}
    native.append(row)
# 0x800812A0's id is in r31: 57 at 0x800810E4, 25 at 0x80081250.
extra_ids = {0x800812A0: [57, 25]}
native_by_id = {}
for row in native:
    ids = [row['id']] if row['id'] is not None else extra_ids.get(int(row['site'], 16), [])
    for i in ids:
        native_by_id.setdefault(i, []).append(row)
# ed219, 220, 221 ask 6, ed301 asks whatever it was given: not a fixed id
for row in native:
    if row['function'] == '0x8000A424':
        native_by_id.get(None, [])
if None in native_by_id:
    del native_by_id[None]


# ---- 3. ed301's own filters: jump tables 0x8023BF58 (id-6) and 0x8023BE68 (id)
FILTER_A = {i + 6: D.word(0x8023BF58 + 4 * i) == 0x8000A4E8 for i in range(0x36)}  # refused while 0x8006DB04(0x4000)
FILTER_B = {i: D.word(0x8023BE68 + 4 * i) == 0x8000A524 for i in range(0x3C)}      # refused while actor +158==1, +159==6
# Read at 0x80006F10..0x80006F64: the ids a stick press acts on, with the player's control channels held
STICK = {6, 7, 11, 13, 19, 26, 34, 58}


# ---- 4. the words of group 1 and 2: shown by 0x801DB374 directly, not by a prompt
def word_sites(group, entry):
    out = []
    for c in callers(0x801DB374):
        i = IDX[c]
        a = b = None
        for k in range(i - 1, max(i - 12, 0), -1):
            t = TXT[k]
            m = re.match(r'li\s+r4, (-?\d+)$', t)
            if m and a is None:
                a = int(m.group(1))
            m = re.match(r'li\s+r5, (-?\d+)$', t)
            if m and b is None:
                b = int(m.group(1))
            if t.startswith('bl '):
                break
        if a == group and b == entry:
            out.append('0x%08X' % c)
    return out


# ---- 5. the input bindings
PAD = [(0x1, 'D-left'), (0x2, 'D-right'), (0x4, 'D-down'), (0x8, 'D-up'), (0x10, 'Z'), (0x20, 'R-digital'), (0x40, 'L-digital'),
       (0x100, 'A'), (0x200, 'B'), (0x400, 'X'), (0x800, 'Y'), (0x1000, 'Start'), (0x10000, 'stick-x-'), (0x20000, 'stick-x+'),
       (0x40000, 'stick-y-'), (0x80000, 'stick-y+'), (0x100000, 'C-x-'), (0x200000, 'C-x+'), (0x400000, 'C-y-'), (0x800000, 'C-y+'),
       (0x1000000, 'L-trigger'), (0x2000000, 'R-trigger'), (0x4000000, 'L-full'), (0x8000000, 'R-full'), (0x10000000, 'D-x'),
       (0x20000000, 'D-y')]


def padname(m):
    return '+'.join(n for b, n in PAD if m & b) or hex(m)


def table(addr, count):
    rows = []
    for i in range(count):
        w = [D.word(addr + 32 * i + 4 * k) for k in range(5)]
        rows.append({'at': '0x%08X' % (addr + 32 * i), 'mask': '0x%08X' % w[0], 'pad': padname(w[0]),
                     'when': 'held' if w[1] else 'pressed', 'handler': '0x%08X' % w[2], 'r4': '0x%X' % w[3],
                     'r3_range': [(w[4] >> 16) if (w[4] >> 16) < 0x8000 else (w[4] >> 16) - 0x10000,
                                  (w[4] & 0xFFFF) if (w[4] & 0xFFFF) < 0x8000 else (w[4] & 0xFFFF) - 0x10000]})
    return rows


BINDINGS = {
    'in play: prompts, Tome, quick spells (0x80006988, channel r13-26224, run by 0x80006A94)': table(0x8023B9B8, 11),
    'walking control, into 0x805D79A0 (channel r13-23060, block r13-25660)': table(0x8023AF40, 9),
    'while R is held, into 0x805D7AC0 (channel r13-23056, block r13-25660)': table(0x8023AE60, 7),
    'triggers (0x80199C3C, channel r13-23052)': table(0x802549E4, 3),
    'stick for 0x80045BC4 (channel r13-25648)': table(0x8023F1A8, 2),
    'words open (0x80024310)': table(0x8023E458, 3),
    'a find open (0x80024528)': table(0x8023E4B8, 2),
    'a thing shown (0x80024A14)': table(0x8023E510, 3),
    'Options page (0x80101600)': table(0x8024EDD8, 5),
    'Map page (0x80111B40)': table(0x80252778 + 96, 11),
    'interface ed56(11) (0x8007AB04)': table(0x80244740, 5),
    'interface ed56(9, 10) first (0x8007AC94)': table(0x802447E0, 3),
    'interface ed56(9, 10) second': table(0x80244840, 2),
    'interface ed56(8) (0x8007B478)': table(0x80244880, 3),
    'interface ed56(7) (0x8007B50C)': table(0x802448E0, 2),
    'interface ed56(0) (0x8007B5A4)': table(0x80244920, 4),
    'interface ed56(1) (0x8007B834)': table(0x802449A0, 4),
    'interface ed56(2) (0x8007B90C)': table(0x80244A20, 4),
    'interface ed56(3) (0x8007B964)': table(0x80244AA0, 3),
    'interface ed56(4) (0x8007BA50)': table(0x80244B00, 4),
    'camera pan and tilt (0x801EEF18)': table(0x80301348, 3) + table(0x803013A8, 3),
}


# ---- 6. what pressing each does, and its chain. Hand-written from what was read; see verbs.md for how to check.
SCRIPT_RUN = '0x80006EBC -> 0x801642D4(script, 0, 0): the script again, at the word it left (fn0)'
ZONE_CHAIN = ['0x80134A44 scan', '0x80134758 picks enter/leave/stay', '0x801642D4 region script', '0x8000A424 ed301', '0x80117404', '0x80117628 picks the prompt',
              'B: 0x80006EBC', '0x801642D4']
DOOR = 'The script again, at the word it set: fn66 go_to_room 0x801666A4 (usually word 1; counts in the doors section)'
PRESS = {
    1: (DOOR, ZONE_CHAIN + ['0x801666A4 fn66']),
    3: (DOOR + '. 2 of the files (L00/776, L11/776) use ed68, ed83, ed55, ed89 instead', ZONE_CHAIN + ['0x801666A4 fn66']),
    4: (DOOR, ZONE_CHAIN + ['0x801666A4 fn66']),
    6: ('From ed219/220/221 (script -2): 0x80006EBC -> 0x80024D78 steps the close-up. From ed301: the script again',
        ['0x80134A44 scan', '0x80017604 ed221 / 0x80017324 ed219 / 0x80017154 ed220', '0x800170D8', '0x800172C8', '0x800174A0', '0x803090C4 record',
         '0x80117404', 'B: 0x80006EBC', '0x80307108 looks', '0x80024D78', '0x80024310 words']),
    7: ('Script -2: the close-up\'s last step, 0x80024D78 -> 0x801642D4(the look\'s script). From ed301: the script again. A stick press also ends it: 0x80024B24(1), which for a look of kind 2 or 3 sets its script to word 100 and runs it; from ed301, word 100 and a run (0x80006F80)',
        ['0x80024D78', '0x80025114 / 0x800254F8 last step', '0x801EEA9C', '0x80043C88(1)', '0x8002576C', '0x801642D4']),
    10: (DOOR, ZONE_CHAIN + ['0x801666A4 fn66']),
    19: ('Three askers. Thing in the world, script 2246: fn79(player, 71, 0), then 0x800C362C. From a close-up (script -2, kind 3): 0x80025408 -> 0x801F9DE8(thing, player, 30) and 0x80024528. From ed301: the script again. The find also has ways in with no prompt 19 (ed52)',
         ['0x800C030C', '0x800DA688', '0x80117404(2, 19, 2246, 0, 49, 1, 0)', 'script 2246', '0x801F57C0 event 71', '0x800C362C',
          'model bit 0x4000: 0x80024A14, 0x801F59B0 event 71 to the thing, 0x800D8634 state 35', 'else +56 of 2: 0x80024528']),
    20: ('Nothing: button 6 has no binding. It is the stick: event 44, 0x80060DC4 adds to actor +140 -> +357 when a stick sector changes, at 10 event 126 lets go. Read for state 56',
         ['0x80053E4C state 56', '0x8005713C state 67', '0x80057790 state 69', '0x80060EC8 kind 19', '0x80045BC4 event 44', '0x80060DC4']),
    21: ("The weapon's script, 0x80150874, or 1582 if that's 0. Script 1582: ed82, ed20, ed140, ed9, ed130, ed80. Z runs the weapon's script with no prompt and no 1582 (0x800C2CA4)",
         ['0x800C030C', '0x800C05F0', '0x80117404(2, 21, script, 0, 50, 3, 0)', 'B: 0x80006EBC', '0x801642D4', 'ed80 0x80012CE0 event 155', '0x80079798 state 77']),
    22: ('Script 1902: fn79(player, 147, 0, 1). Event 147 with data: 0x800BF720 gives who, and if they answer 1, state 71 (0x800527A8). From ed301 in L00/583: the script again',
         ['0x80091F0C event 147 data 0 to the player', '0x80058DD8 event 147', '0x80117404(2, 22, 1902, 0, 48, 3, 0)', 'B: script 1902', '0x80058DD8 event 147 data 1', '0x800BF720', 'state 71 0x800527A8']),
    23: ('Script 2244: fn79(player, 202, 0). Event 202: 0x800C30CC, one path sets state 30', ['0x800C030C', '0x800330E0', '0x80117404(2, 23, 2244, 0, 49, 3, 2)', 'script 2244', 'event 202', '0x800C30CC']),
    24: ('Nothing: button 6. In state 52 event 44 adds through 0x80060DC4; at 8, event 112, state 1', ['event 107', '0x80074CBC state 52', '0x800566AC', '0x8005680C']),
    25: ('The interface\'s own bindings (0x80244AA0: 0x80080D70, 0x80080C68)', ['ed56(3)', '0x8007B964', '0x800810D0 tick', '0x80117404(2, r31, 0, 0, 50, 0, 0)']),
    34: ('Script -2: next step of the close-up, its words (0x80024310(element, -2, 0)). From ed301: the script again', ['0x80024D78', '0x80025050 / 0x80025344', '0x801DB374', '0x80024310']),
    43: ('Script 2250: fn79(player, 194, 0). Event 194: 0x800C38B8, state 84. Only in level 5', ['0x800C030C', '0x80032E8C', '0x80117404(2, 43, 2250, 0, 49, 5, 0)', 'script 2250', '0x800C38B8']),
    47: ('The interface\'s own bindings', ['ed56(9, 10)', '0x8007E41C tick', '0x80117404(2, 47, 0, 0, 50, 0, 0)']),
    49: ('Nothing runs: script 0. Its stick binding is 0x801EF110; that it pans is from the name', ['0x800083C8', '0x801EF074', '0x801EEF18']),
    50: ('Button 6: not pressed. ed56(1)\'s own bindings (0x802449A0)', ['ed56(1)', '0x8007B834', '0x8007E9BC tick', '0x80117404(6, 50, 0, 0, 50, 0, 0)']),
    53: ('Button 6: not pressed. Which interface it belongs to is not settled', ['ed68(0) 0x8007A9AC / 0x80083598', '0x8007E508', '0x80117404(6, 53, 0, 0, 50, 0, 0)']),
    54: ('Nothing runs: script 0. Its stick binding is 0x801EF110; that it tilts is from the name', ['0x800083C8', '0x801EF074', '0x801EEF18']),
    55: ('Button 6: not pressed. ed56(8)\'s own bindings (table 0x80244880; rows 2 and 3 are 0x8007F254)', ['ed56(8) 0x8007B478', '0x8007F254', '0x80117404(6, 55, 0, 0, 50, 0, 0)']),
    57: ('The interface\'s own bindings (0x80244AA0)', ['ed56(3)', '0x800810D0', '0x80117404(2, r31, 0, 0, 50, 0, 0)']),
    60: ('The interface\'s own bindings', ['ed56(9, 10)', '0x8007E41C tick', '0x80117404(2, 60, 0, 0, 50, 0, 0)']),
    62: ('The interface\'s own bindings', ['ed56(9, 10)', '0x8007E41C tick', '0x80117404(2, 62, 0, 0, 50, 0, 0)']),
}

rows = []
prompts = text['prompts']
for pid in range(63):
    entry = prompts.get('0,%d' % pid)
    section, label, words = entry
    a = ask.get(pid)
    how = []
    if a:
        how.append('ed301(2, %d) 0x8000A424' % pid)
    if pid == 6:
        how += ['ed219 0x80017324', 'ed220 0x80017154', 'ed221 0x80017604']
    natives_here = native_by_id.get(pid, [])
    for n in natives_here:
        if n['function'] in ('0x80017154', '0x80017324', '0x80017604'):
            continue
        how.append('%s in %s (%s) button %s script %s prio %s slot %s flag %s' % (
            n['site'], n['function'], n['where'] or '-', n['button'], n['script'] if n['script'] is not None else 'from the weapon', n['priority'], n['slot'] if n['slot'] is not None else '2 or 4', n['flag']))
    press = PRESS.get(pid)
    if press is None and a:
        press = (SCRIPT_RUN, ZONE_CHAIN)
    asked = bool(how)
    LIVE = {6: 'saves 2, 5, 7, 8', 19: 'save 4'}
    if not asked:
        mark = 'proved: nothing asks, by scan'
    elif pid in LIVE:
        mark = 'ask proved (live, %s); press read' % LIVE[pid]
    elif a:
        mark = 'ask proved (scripts and ed301 agree); press read'
    else:
        mark = 'ask read; press read'
    if pid in (49, 54):
        mark = 'ask read; press inferred'
    row = {
        'id': pid, 'label': label, 'words': clean(words), 'raw_words': words, 'asked': asked, 'how_asked': how,
        'script_calls': a['calls'] if a else 0, 'script_files': len(a['files']) if a else 0, 'script_ids': len(a['ids']) if a else 0,
        'buttons_in_scripts': sorted(a['buttons']) if a else [],
        'files_with_fn66': len(go_room.get(pid, ())),
        'sample': sorted(a['files'])[:6] if a else [],
        'press_runs': press[0] if press else None,
        'chain': press[1] if press else [],
        'refused_while_0x4000': FILTER_A.get(pid, False),
        'refused_while_state_1_6': FILTER_B.get(pid, False),
        'stick_acts': pid in STICK,
        'mark': mark,
    }
    if pid == 6:
        row['examine_natives'] = exam_stats
    rows.append(row)

never = [r['id'] for r in rows if not r['asked']]

group1 = {k: [v[1], clean(v[2]), word_sites(int(k.split(',')[0]), int(k.split(',')[1]))] for k, v in prompts.items() if k.startswith(('1,', '2,'))}

out = {
    'source': 'tools/ed/facts/verbs.py, from build/ed-db and generated/main.dol. Nothing here came from a running game. Save state checks are in verbs.md.',
    'scripts_scanned': len(files),
    'ed301_with_a_number_not_written_out': nonliteral,
    'prompts': rows,
    'never_asked': [{'id': i, 'label': rows[i]['label']} for i in never],
    'native_asks': native,
    'door_stats': door,
    'word_100': word100,
    'ed52': ed52,
    'fn25_compared_with': sorted(fn25_values),
    'files_with_fn35': fn35_files,
    'regions_summary': SUMMARY,
    'examine_natives': exam_stats,
    'tome_hint_words_groups_1_2': group1,
    'bindings': BINDINGS,
}
json.dump(out, open(os.path.join(ROOT, 'build', 'ed-db', 'facts', 'verbs.json'), 'w', encoding='utf8'), indent=1, ensure_ascii=False)


# ---- verbs.md: the table is made here, the rest is in verbs_text.md
def how_short(r):
    if not r['how_asked']:
        return 'nothing found'
    return '<br>'.join(r['how_asked'])


lines = ['| id | label | words | how it is asked | script calls / files / distinct ids | what pressing it runs | filters | marks |', '|---|---|---|---|---|---|---|---|']
for r in rows:
    f = []
    if r['refused_while_0x4000']:
        f.append('A')
    if r['refused_while_state_1_6']:
        f.append('B')
    if r['stick_acts']:
        f.append('S')
    counts = '%d / %d / %d' % (r['script_calls'], r['script_files'], r['script_ids'])
    if r['files_with_fn66']:
        counts += ' (%d with fn66)' % r['files_with_fn66']
    lines.append('| %d | %s | %s | %s | %s | %s | %s | %s |' % (r['id'], r['label'], r['words'].replace('|', '/'), how_short(r), counts, r['press_runs'] or '-', ' '.join(f), r['mark']))
TABLE = '\n'.join(lines)

blines = []
for name, tab in BINDINGS.items():
    blines.append('**%s**\n' % name)
    blines.append('| at | pad | when | handler | r4 | r3 from, to |\n|---|---|---|---|---|---|')
    for b in tab:
        blines.append('| %s | %s | %s | %s | %s | %s, %s |' % (b['at'], b['pad'], b['when'], b['handler'], b['r4'], b['r3_range'][0], b['r3_range'][1]))
    blines.append('')
BIND = '\n'.join(blines)

nlines = ['| site | function | where | button | id | script | arg 4 | priority | slot | flag |', '|---|---|---|---|---|---|---|---|---|---|']
for n in native:
    nlines.append('| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |' % (n['site'], n['function'], n['where'], n['button'], n['id'] if n['id'] is not None else 'r31 or from ed301', n['script'] if n['script'] is not None else '-', n['arg4'] if n['arg4'] is not None else '-', n['priority'], n['slot'] if n['slot'] is not None else '2 or 4', n['flag']))
NATIVE = '\n'.join(nlines)

ex = exam_stats
EXAM = 'ed219: %d calls in %d files, %d ids. ed220: %d calls in %d files, %d ids. ed221: %d calls in %d files, %d ids. ed221 fifth argument: %d with a thing, %d without. ed221 sixth: %s. ed221 fourth: %s. ed219 fourth: %s.' % (
    ex['ed219']['calls'], ex['ed219']['files'], ex['ed219']['ids'], ex['ed220']['calls'], ex['ed220']['files'], ex['ed220']['ids'], ex['ed221']['calls'], ex['ed221']['files'], ex['ed221']['ids'],
    ex['ed221']['arg5_nonzero'], ex['ed221']['arg5_zero'], ex['ed221']['arg6'], ex['ed221']['arg4_flags'], ex['ed219']['arg4_flags'])
NEVER = ', '.join('%d %s' % (i, rows[i]['label']) for i in never)

dl = ['| id | label | files | no `fn66` | `fn66` in the same state as the ask | press word holds `fn66` | other | simple | placed as a region\'s |', '|---|---|---|---|---|---|---|---|---|']
for pid in sorted(door):
    d = door[pid]
    dl.append('| %d | %s | %d | %d%s | %d | %d | %d%s | %d | %s |' % (
        pid, rows[pid]['label'], d['files'], len(d['no_fn66']), ' (%s)' % ', '.join(d['no_fn66']) if d['no_fn66'] else '', len(d['fn66_with_the_ask']), d['press_word_holds_fn66'],
        len(d['other']), ' (%s)' % ', '.join(d['other']) if d['other'] else '', d['simple'], ', '.join('%s %d' % kv for kv in sorted(d['placed'].items())) or '-'))
DOORS = '\n'.join(dl)
T = SUMMARY['totals']
REG = '%d rooms, %d regions: %d boxes, %d spheres, %d planes. %d enter scripts, %d leave, %d stay, %d enter and %d leave for other nodes, %d planes with a script.' % (
    T['rooms'], T['regions'], T['shape1'], T['shape0'], T['shape2'], T['enter'], T['leave'], T['stay'], T['other_enter'], T['other_leave'], T['plane_with_a_script'])
ROOMSJSON = '`rooms.json` once lacked 367 planes and 47 spheres or boxes, and most leave scripts, from a name filter. Fixed: tools/ed/roomfile.py reads the whole list, and `ed room` prints every script'
W = '%d files test word 100; %d set it themselves; %d have no setter (%s).' % (word100['test'], word100['set_themselves'], len(word100['no_setter']), ', '.join(word100['no_setter']))
ED52 = '`ed52` has %d calls in %d files; %d of the files never ask 19.' % (ed52['calls'], ed52['files'], ed52['without_prompt_19'])
rooms_on_disc = {int(k) for k in REGIONS}
not_rooms = sorted(fn25_values - rooms_on_disc)
FN25 = '%d distinct values, %d of them rooms on the disc (not: %s)' % (len(fn25_values), len(fn25_values) - len(not_rooms), ', '.join(map(str, not_rooms)) or 'none')
NEG = 'ed219 %d calls and ed220 %d with a negative second argument' % (neg_entry['ed219'], neg_entry['ed220'])
src = open(os.path.join(HERE, 'verbs_text.md'), encoding='utf8').read()
for k, v in (('{{DOORS}}', DOORS), ('{{REGIONS}}', REG), ('{{ROOMSJSON}}', ROOMSJSON), ('{{WORD100}}', W), ('{{ED52}}', ED52), ('{{FN25}}', FN25), ('{{NEG}}', NEG), ('{{FN35}}', str(fn35_files)),
             ('{{ROOMS_LIVE}}', '147 in slot 1, 21 in slot 3, 153 in slot 6, 155 in slot 7, 115 in slot 10')):
    src = src.replace(k, v)
doc = src.replace('{{TABLE}}', TABLE).replace('{{BINDINGS}}', BIND).replace('{{NATIVE}}', NATIVE).replace('{{EXAM}}', EXAM).replace('{{NEVER}}', NEVER).replace('{{N_SCRIPTS}}', str(len(files))).replace('{{NONLITERAL}}', str(nonliteral))
open(os.path.join(ROOT, 'build', 'ed-db', 'facts', 'verbs.md'), 'w', encoding='utf8').write(doc)
print('scripts', len(files), 'nonliteral', nonliteral)
print('never asked', len(never), NEVER)
print(EXAM)
print('doors', {k: {a: (b if not isinstance(b, list) else len(b)) for a, b in v.items()} for k, v in door.items()})
print(W)
print(ED52, FN25, NEG, 'fn35', fn35_files)
left = re.findall(r'\{\{[A-Z0-9_]+\}\}', doc)
if left:
    sys.exit('placeholders left: %s' % left)
