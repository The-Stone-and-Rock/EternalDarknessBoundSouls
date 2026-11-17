"""`ed stops RUN [SCRIPT|--p2]`: every script run in a run, who ran it (player 1 or player 2), the calls it made, and
where it stopped: the calls matched against the script's own lines, and the test right after the last one. A script
that runs for player 2 and stops early at a test (a gate) is the first thing to look at in a co-op bug.

Reads a run folder's page-*.log: the "[trace] script run r3 S r4 Z lr L who W ctl C" lines (coop_run_script's
trace_look, ED_COOP_TRACE: jtg.sh and run.sh set it) and the "[script] calls 0xADDR (N args)" lines after each (up to
40 per traced run, hle.c g_script_watch). who is r13-21496 (the player then), ctl r13-21488 (the main character).
Player 1's id is 1 (the chapter's character); any other who is "player 2 (id W)" or another character.

The match walks the script's lines in file order, taking each logged call where it next appears: right for the guard
at the top of a script (the usual stop), approximate after a jump. It says so when a call isn't found."""
import collections
import glob
import os
import re

import disc

DB = disc.DB
TRACE = re.compile(r'^\[trace\] script run r3 (-?\d+) r4 (-?\d+) lr (0x[0-9A-Fa-f]+) who (\d+) ctl (\d+).*?frame (\d+)')
CALL = re.compile(r'^\[script\] calls (0x[0-9A-Fa-f]+) \((\d+) args\)')
LR_NAMES = {'0x80134B38': 'a zone (enter/leave/stay or other_*)', '0x80163E5C': 'a timer or a queued run (waits)',
            '0x8000704C': 'a prompt press', '0x0BADF010': 'a host guest call', '0x80008280': 'play mode\'s start (ed177)',
            '0x800082AC': 'the Tome\'s end (ed9)', '0x80164488': 'run next (fn73)'}


def natives_by_addr():
    out = {}
    for line in open(os.path.join(DB, 'natives.tsv')):
        p = line.rstrip('\n').split('\t')
        out['0x' + p[1].upper()] = p[0]
    return out


def player1_id(runs):
    """Player 1's id: 1, the chapter's character (proved in all 10 saves, docs/game/research/verbs.md:15). (Guessing it
    from the runs fails when player 2's chain makes most of them: their main character is theirs then.)"""
    return 1


def runs_of(run):
    names = natives_by_addr()
    out, cur = [], None
    for path in sorted(glob.glob(os.path.join(run, 'page-*.log'))):
        for line in open(path, encoding='utf8', errors='replace'):
            m = TRACE.match(line)
            if m:
                cur = dict(script=int(m.group(1)), zone=int(m.group(2)), lr=m.group(3).upper().replace('0X', '0x'),
                           who=int(m.group(4)), ctl=int(m.group(5)), frame=int(m.group(6)), calls=[])
                out.append(cur)
                continue
            m = CALL.match(line)
            if m and cur is not None and len(cur['calls']) < 40:
                addr = '0x' + m.group(1)[2:].upper()
                cur['calls'].append(names.get(addr, addr))
    return out


def level_of(run):
    """The level whose scripts the run played: from both.tsv's room and the catalogue's level rooms, else None."""
    return None


def where_stopped(sid, level, calls):
    """(matched lines, the line index after the last match, lines) for one run, or None without the script's text."""
    paths = [os.path.join(DB, 'scripts', 'L%s' % level, '%d.lua' % sid)] if level else sorted(
        glob.glob(os.path.join(DB, 'scripts', 'L*', '%d.lua' % sid)))
    for path in paths:
        if not os.path.exists(path):
            continue
        lines = open(path, encoding='utf8', errors='replace').read().split('\n')
        at, matched, missing = 0, [], []
        for name in calls:
            pat = re.compile(r'\b%s\(' % re.escape(name))
            j = next((k for k in range(at, len(lines)) if pat.search(lines[k])), None)
            if j is None:
                missing.append(name)
                continue
            # a line can make several calls (handle[fn78](1, 0) twice on one line is one line): stay on it while it has more
            matched.append(j)
            at = j if len(pat.findall(lines[j])) > matched.count(j) else j + 1
        return matched, at, lines, missing, os.path.relpath(path, DB)
    return None


def cmd_stops(argv):
    if not argv:
        raise SystemExit('ed stops RUN [SCRIPT] [--p2] [--level N]: the script runs of a run, who ran each, where each stopped')
    level = None
    if '--level' in argv:
        i = argv.index('--level')
        level = argv[i + 1].zfill(2)
        del argv[i:i + 2]
    only_p2 = '--p2' in argv
    argv = [a for a in argv if a != '--p2']
    run = argv[0] if os.path.isdir(argv[0]) else (sorted(glob.glob(os.path.join(disc.ROOT, 'logs', 'run-*' + argv[0] + '*'))) or [argv[0]])[-1]
    want = int(argv[1]) if len(argv) > 1 else None
    runs = runs_of(run)
    p1 = player1_id(runs)
    print('%s: %d traced script runs; player 1 is id %s (the chapter\'s character), any other who is player 2 or '
          'another character' % (os.path.basename(run), len(runs), p1))
    shown = 0
    for r in runs:
        if want is not None and r['script'] != want:
            continue
        whose = 'player 1' if r['who'] == p1 else 'player 2 (id %d)' % r['who']
        if only_p2 and r['who'] == p1:
            continue
        shown += 1
        print('\nframe %d: script %d for %s (main %d), from %s, %d calls logged' % (
            r['frame'], r['script'], whose, r['ctl'], LR_NAMES.get(r['lr'], 'lr ' + r['lr']), len(r['calls'])))
        print('  calls: %s' % (' '.join(r['calls']) or '(none logged)'))
        w = where_stopped(r['script'], level, r['calls'])
        if w is None:
            print('  (no script text found)')
            continue
        matched, at, lines, missing, rel = w
        if missing:
            print('  (%s: calls not found in file order: %s; the match is approximate here)' % (rel, ' '.join(missing)))
        if len(r['calls']) >= 40:
            print('  (40 calls logged: the log stops there, so the run went on past what is shown)')
            continue
        last = matched[-1] if matched else -1
        print('  %s, the last call at line %d: %s' % (rel, last + 1, lines[last].strip() if last >= 0 else '(none)'))
        nxt = [(k, lines[k].strip()) for k in range(last + 1, min(len(lines), last + 6)) if lines[k].strip()]
        test = next(((k, t) for k, t in nxt if t.startswith('if ')), None)
        if test:
            print('  it stopped at or after line %d: %s' % (test[0] + 1, test[1]))
            if re.search(r'\b(\w+) (~=|==) 1 then', test[1]) and any(re.search(r'\b%s\s*=\s*(?:handle\[fn78\]|fn78)\(1, 0\)' % re.escape(
                    re.search(r'\b(\w+) (~=|==) 1 then', test[1]).group(1)), l) for l in lines):
                print('  ^ a hero gate: the player\'s handle (fn78(1, 0)) must be 1, the chapter\'s character. For player 2 it is %d.' % r['who'])
        for k, t in nxt[:4]:
            print('    %4d  %s' % (k + 1, t))
    if not shown:
        print('(no traced run matches)')
