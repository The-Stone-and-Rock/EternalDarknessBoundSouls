"""Reads an item sweep (tools/ed/item_sweep.sh) and says, for each item type and each player, whether the find went as
a normal player's: the item made (its own model or a stand-in), the finder's close-up in their own half and not the
other's, the other player told (the notice), the carried count up by one, the room unchanged, nobody held.
    python -I tools/ed/item_sweep_report.py logs/item-sweep-STAMP.txt
Writes logs/item-sweep-STAMP-report.md beside the list and prints the failures.
"""
import glob
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SPAWN = re.compile(r'\[items\] for player (\d): \[items\] type (\d+) \((.*?)\)(?: on the ground .*?, (its own|stand-in) model (-?\d+)|: (.*))')
NOTICE = re.compile(r'\[coop\] notice for player (\d): (.*)')
SPAN = re.compile(r'^\s*(\d+)-(\d+)\s+room (\d+)\s+P1 (.+?)\s{2,}P2 (.+?)\s{2,}')
CARRY = re.compile(r'carry (\d+)/(\d+)')


def timeline(run, start, end):
    out = subprocess.run([sys.executable, '-I', os.path.join(ROOT, 'tools', 'ed', 'ed.py'), 'timeline', run, str(start), str(end)],
                         capture_output=True, text=True, cwd=ROOT).stdout
    spans = []
    for line in out.splitlines():
        m = SPAN.match(line)
        if m:
            c = CARRY.search(line)
            spans.append((int(m[1]), int(m[2]), int(m[3]), m[4].strip(), m[5].strip(), int(c[1]) if c else None))
    return spans


def window(spans, a, b):
    s = [x for x in spans if x[1] > a and x[0] < b]
    return {
        'p1_close': any('close-up' in x[3] for x in s), 'p2_close': any('close-up' in x[4] for x in s),
        'rooms': {x[2] for x in s}, 'carry': [x[5] for x in s if x[5] is not None],
        'long': any(('close-up' in x[3] or 'close-up' in x[4]) and x[1] - x[0] > 260 for x in s),
    }


def main():
    lst = sys.argv[1]
    rows, fails = [], []
    for line in open(lst, encoding='utf-8'):
        first, last, start, every, run = line.split()[:5]
        first, last, start, every = int(first), int(last), int(start), int(every)
        text = ''.join(open(p, encoding='utf-8', errors='replace').read() for p in sorted(glob.glob(os.path.join(ROOT, run, 'page-*.log'))))
        spans = timeline(run, start, start + (last - first + 1) * every + 30)
        base_room = spans[0][2] if spans else None
        # Each log line belongs to the last spawn before it.
        made, notices, current = {}, {}, None
        for l in text.splitlines():
            m = SPAWN.search(l)
            if m:
                current = (int(m[2]), int(m[1]))
                made[current] = (m[3], m[4] or 'not made: ' + (m[6] or '?'), m[5])
                continue
            m = NOTICE.search(l)
            if m and current:
                notices.setdefault(current, []).append((int(m[1]), m[2]))
        for t in range(first, last + 1):
            s = start + (t - first) * every
            for p in (1, 2):
                a, b = (s, s + 150) if p == 1 else (s + 150, s + 300)
                w = window(spans, a, b)
                late = window(spans, a + 25, b)  # the other's last close-up can run a few frames into this window
                name, how, model = made.get((t, p), ('?', 'not spawned', None))
                told = [n for who, n in notices.get((t, p), []) if who != p]
                carry = w['carry']
                up = carry and carry[-1] - carry[0] if carry else 0
                problems = []
                if not how.startswith(('its own', 'stand-in')):
                    problems.append(how)
                else:
                    if not (w['p1_close'] if p == 1 else w['p2_close']):
                        problems.append('no close-up in their half')
                    if (late['p2_close'] if p == 1 else late['p1_close']):
                        problems.append("close-up in the other's half")
                    if not told:
                        problems.append('other player not told')
                    if up != 1:
                        problems.append(f'carried count +{up}')
                if base_room is not None and w['rooms'] - {base_room}:
                    problems.append(f"room changed to {sorted(w['rooms'] - {base_room})}")
                if w['long']:
                    problems.append('close-up over 260 frames (held?)')
                row = (t, name, p, how + ('' if model is None else f' {model}'), '; '.join(problems) or 'ok', told[0] if told else '', run)
                rows.append(row)
                if problems:
                    fails.append(row)
    out = lst[:-4] + '-report.md'
    with open(out, 'w', encoding='utf-8') as f:
        f.write('| type | item | player | model | result | notice | run |\n|---|---|---|---|---|---|---|\n')
        for r in rows:
            f.write('| %d | %s | %d | %s | %s | %s | %s |\n' % r)
    ok = len(rows) - len(fails)
    print(f'{ok} of {len(rows)} finds as a normal player\'s; {len(fails)} not. Report: {out}')
    for r in fails:
        print(f'  type {r[0]} {r[1]}, player {r[2]} ({r[3]}): {r[4]}')


if __name__ == '__main__':
    main()
