"""`ed test`: the regression scenarios in tools/ed/scenarios. Each replays real presses on a save, with the game as the user
plays it, and checks the rules. A fix isn't done until these pass: the user shouldn't be the one to find it broke something.

A scenario is JSON:
  name, what it proves, save, p1 (ED_INPUT_SCRIPT presses), p2 (ED_PAD2_SCRIPT presses), frames (when to close),
  env (more variables for the run), run_script (ED_RUN_SCRIPT, DELAY:SCRIPT,...: a script run as a zone would, twice for its zone and its prompt's press)
  expect: list of checks, each {"check": ..., ...}:
    log       pattern must appear in the run's log
    no_log    pattern must not
    count     pattern must appear exactly n times (a rune given once, a find not run twice)
    moved     who walked at least min units from frame `from` to `to`, or while `during` is true: window, busy, screen, words_p1, words_p2
    stayed    who stayed within max units over the same
    camera    who's camera is the room's camera `name` at `frame` (from the room's table on the disc)
    field     who's field (p1hp, p1m...) reads `value` on the run's last frame
    plays     who was at some point in player state `state` with motion `anim`
    room      the run ends in room `room`; with who: both, player 2 ended within `within` units of player 1 (default 400)
    rules     no findings from `ed check`, other than those in `allow`

Limits, as docs/coop-failed-attempts.md records: ED_INPUT_SCRIPT goes round controls.cpp, so player 1's held-over and Tome
button handling isn't tested here. ED_RUN_SCRIPT calls the script from the host, whose return address is neither a zone's nor a
prompt's, so host code that branches on those (COOP_PROMPT_RUNS, COOP_ZONE_RUNS) isn't tested by it, and the player needn't be near. Live controllers mix into a run, so nobody should hold one during `ed test`.
"""
import glob
import json
import os
import re
import subprocess
import sys

import disc
import recording

HERE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'scenarios')


def load_all():
    out = {}
    for path in sorted(glob.glob(os.path.join(HERE, '*.json'))):
        s = json.load(open(path, encoding='utf8'))
        out[s['name']] = s
    return out


def run_game(s):
    name = 'test-' + s['name']
    env = dict(os.environ, ED_RUN_FRAMES=str(s['frames']), ED_RUN_SECONDS=str(s.get('seconds', 240)))
    env['ED_INPUT_SCRIPT'] = s.get('p1', '')
    env['ED_PAD2_SCRIPT'] = s.get('p2', '')
    env['ED_RUN_SCRIPT'] = s.get('run_script', '')
    env.update(s.get('env', {}))
    r = subprocess.run(['sh', 'tools/ed/run.sh', s['save'], name], cwd=disc.ROOT, env=env, capture_output=True, text=True, errors='replace')
    # run.sh's first line is this run's folder. A name search could find an older run when this one never started, and pass it.
    lines = (r.stdout or '').strip().split('\n')
    run = os.path.join(disc.ROOT, lines[0].strip()) if lines and lines[0].strip() else ''
    if not run or not os.path.exists(os.path.join(run, 'both.tsv')) or os.path.getsize(os.path.join(run, 'both.tsv')) < 2000:
        return None, r.stdout
    return run, r.stdout


def log_text(run):
    text = []
    for path in sorted(glob.glob(os.path.join(run, 'page-*.log'))):
        text.append(open(path, encoding='utf8', errors='replace').read())
    return '\n'.join(text)


def span(rows, c):
    if 'during' in c:
        d = c['during']
        test = {'window': lambda r: r['window'], 'busy': lambda r: r['busy'], 'screen': lambda r: r['screen'],
                'words_p1': lambda r: r['window'] and not r['screen'], 'words_p2': lambda r: r['window'] and r['screen']}[d]
        return [r for r in rows if test(r)]
    return [r for r in rows if c.get('from', 0) <= r['frame'] <= c.get('to', 1 << 60)]


def walked(rows, who):
    return sum(recording.dist(recording.pos(a, who), recording.pos(b, who)) for a, b in zip(rows, rows[1:]) if b['frame'] - a['frame'] < 4)


def judge(s, run, rows):
    fails = []
    log = log_text(run)
    rooms = recording.room_cameras()
    for c in s.get('expect', []):
        kind = c['check']
        if kind == 'log' and not re.search(c['pattern'], log):
            fails.append('never logged: %s' % c['pattern'])
        elif kind == 'no_log':
            m = re.search(c['pattern'], log)
            if m:
                fails.append('logged: %s' % m.group(0)[:100])
        elif kind == 'count':
            n = len(re.findall(c['pattern'], log))
            if n != c['n']:
                fails.append('%s logged %d times, wanted %d' % (c['pattern'], n, c['n']))
        elif kind == 'moved':
            part = span(rows, c)
            d = walked(part, c['who'])
            if not part:
                fails.append('%s: that moment never came (%s)' % (c['who'], c.get('during', c.get('from'))))
            elif d < c['min']:
                fails.append('%s walked %.0f during %s, wanted at least %d (frames %d to %d)' % (c['who'], d, c.get('during', 'the span'), c['min'], part[0]['frame'], part[-1]['frame']))
        elif kind == 'stayed':
            part = span(rows, c)
            d = walked(part, c['who'])
            if d > c['max']:
                fails.append('%s walked %.0f, wanted at most %d' % (c['who'], d, c['max']))
        elif kind == 'camera':
            r = min(rows, key=lambda x: abs(x['frame'] - c['frame']))
            have = recording.camera_name(rooms, r, c['who'])
            if have != c['name']:
                fails.append('%s\'s camera at frame %d is %s, wanted %s' % (c['who'], r['frame'], have, c['name']))
        elif kind == 'field':
            last = rows[-1] if rows else {}
            if last.get(c['field']) != c['value']:
                fails.append('%s is %s at the end, wanted %s' % (c['field'], last.get(c['field']), c['value']))
        elif kind == 'plays':
            st, an = ('%sst' % c['who'], '%sanim' % c['who'])
            if not any(r.get(st) == c['state'] and r.get(an) == c['anim'] for r in rows):
                fails.append('%s was never in state %s with motion %s' % (c['who'], c['state'], c['anim']))
        elif kind == 'room':
            last = rows[-1] if rows else {}
            if last.get('room') != c['room']:
                fails.append('ended in room %s, wanted %s' % (last.get('room'), c['room']))
            elif c.get('who') == 'both':
                # The room's own arrival places them 150 or 75 units from player 1, or on their spot.
                d = recording.dist(recording.pos(last, 'p1'), recording.pos(last, 'p2'))
                if not any(recording.pos(last, 'p2')) or d > c.get('within', 400):
                    fails.append('player 2 is %.0f units from player 1 at the end, wanted within %d' % (d, c.get('within', 400)))
        elif kind == 'rules':
            allow = set(c.get('allow', [])) | {'STUCK?'}
            for f in recording.check(rows) + recording.check_pictures(run, rows):
                if f['rule'] not in allow:
                    fails.append('%s %s-%s %s' % (f['rule'], f.get('from', ''), f.get('to', ''), f['text']))
    return fails


def main(argv):
    all_s = load_all()
    if '--list' in argv:
        for k, s in all_s.items():
            print('%-26s %s' % (k, s.get('proves', '')))
        return 0
    rejudge = '--last' in argv  # judge the newest run of each again, with no game
    names = [a for a in argv if not a.startswith('--')] or list(all_s)
    bad = 0
    for k in names:
        s = all_s.get(k)
        if s is None:
            raise SystemExit('no scenario %s (--list)' % k)
        print('=== %s: %s' % (k, s.get('proves', '')), flush=True)
        if rejudge:
            run = recording.find_run('test-' + k)
        else:
            run, _ = run_game(s)
        if run is None:
            print('    FAIL the game didn\'t start or wrote no recording')
            print('    FAILED', flush=True)
            bad += 1
            continue
        rows = recording.load(run)
        fails = judge(s, run, rows)
        if rows:
            print('    %s: %d frames, to frame %d' % (os.path.basename(run), len(rows), rows[-1]['frame']))
        if rows and rows[-1]['frame'] < s['frames'] - 30:
            fails.insert(0, 'stopped at frame %d, before %d: a freeze?' % (rows[-1]['frame'], s['frames']))
        for f in fails:
            print('    FAIL ' + f)
        print('    %s' % ('FAILED' if fails else 'passed'), flush=True)
        bad += bool(fails)
    print('%d of %d passed' % (len(names) - bad, len(names)))
    return bad
