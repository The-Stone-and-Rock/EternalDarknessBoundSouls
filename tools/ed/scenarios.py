"""`ed test`: the regression scenarios in tools/ed/scenarios. Each replays real presses with the game as the user plays it
(co-op, the separate rooms, the other player staying at a door) and checks the rules. A fix isn't done until these pass:
the user shouldn't be the one to find it broke something.

**Every scenario starts from Jump to Game, on today's exe (the user, 2026-11-18).** A save state made by an older build
holds that build's host data and, since the Solo / Co-op page, a state under 56 MB plays SOLO: from 2026-11-17 every
scenario ran without player 2 and nobody saw. So a scenario names a START (STARTS below: a chapter, the door scripts to
its room), and the driver makes that start's state itself: a cold boot through the title and Jump to Game
(tools/ed/jtg.sh), the door scripts, a state saved with both players standing (ED_SAVE_STATE). The state is kept as
saves/test-START.state and made again whenever build-win/EternalDarkness.exe is newer than it. Never point a scenario at
a save of the user's.

A scenario is JSON:
  name, what it proves, start (a name in STARTS), base (the frame its numbers were written against: every frame in p1,
  p2, frames, expect and the env's puts is moved by the fresh state's frame + 40 - base), put (p1 / p2: x, y, z in the
  start's room, where each stands 20 frames into the run), mode ("follow": player 2 is brought through a door, the
  kit's "together"; default: as played, they stay), skip (why it cannot run yet: reported, never counted as passed),
  p1 (ED_INPUT_SCRIPT presses), p2 (ED_PAD2_SCRIPT presses), frames (when to close),
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
    flicker   needs env ED_RUN_PROBE: player `view` (0 or 1), the picture's one-frame spikes of over `threshold` (0.25; the fault was 0.6 to 0.8, and moments between frames differ from frames by about 0.13) at most `spikes` (0), its largest step at most `max_step`, and no shadows dropping out for a frame

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
EXE = os.path.join(disc.ROOT, 'build-win', 'EternalDarkness.exe')
KEEP = os.path.join(disc.ROOT, 'build', 'test')

# Where scenarios start: a chapter of Jump to Game (1 Pius ... 12 Alex End, as tools/ed/jtg.sh counts), then the door
# scripts that take the players to the room (`ed door ROOM`; a prompted door twice). `room` is checked on the state.
STARTS = {
    # Alex's last chapter (Jump to Game's twelfth): the Tome, every rune and every spell are hers from its first frame, so
    # nothing has to be played or spawned first (the user, 2026-11-18: "Alex is the only chapter that already has the tome
    # and spells"). Anthony's chapter from its start does not reach the bookshelf room 147 by a door script alone: its plane's
    # script 892 sends him to the Tome's room 5 until the room's own Tome (tag 733) is his, a story of several scenes.
    'alex-100': {'chapter': 12, 'scripts': [], 'room': 100},
    'karim-338': {'chapter': 4, 'scripts': [], 'room': 338},       # Karim's start room
    # Max's chapter from its start, his bedroom: a close camera, three plain doors to a Walk on Ceiling room (249 -> 252 ->
    # 256 -> 257). The place to test sanity effects (the user, 2026-11-18: Alex's room 100 has its camera too far away).
    'max-249': {'chapter': 5, 'scripts': [], 'room': 249},
}
# The players of every test, whatever the user's settings.ini has: player 1 the chapter's own character, player 2 Ellia,
# no other forms. A scenario that tests a form or a change of characters asks for it itself (ED_COSTUME, ED_TEST_*).
SETTINGS = 'mod_p1_character=0;mod_p2_character=2;mod_p1_costume=0;mod_p2_costume=0'
SETTLE = 40  # frames from the state's own to the scenario's base: the puts land at 20


def exe_stamp():
    return int(os.path.getmtime(EXE))


def sh(script, args, env):
    """Runs tools/ed/<script>; its first output line is the run's folder."""
    e = dict(os.environ)
    for k in ('ED_QUICKLOAD', 'ED_TITLE_STATE', 'ED_COOP_STAY', 'ED_ROOM_SLOTS', 'ED_SETTINGS', 'ED_PUT', 'ED_RUN_SCRIPT',
              'ED_INPUT_SCRIPT', 'ED_PAD2_SCRIPT', 'ED_SAVE_STATE', 'ED_RUN_FRAMES'):
        e.pop(k, None)
    e.setdefault('ED_RUN_AWAY', '1')  # the window off the desktop and no sound, unless the caller says ED_RUN_AWAY=
    if e.get('ED_RUN_AWAY') == '':
        del e['ED_RUN_AWAY']
    e.update({k: str(v) for k, v in env.items()})
    # A test run closes EVERY EternalDarkness.exe by name when it ends: never start one over a game the user has open.
    listed = subprocess.run(['tasklist'], capture_output=True, text=True, errors='replace').stdout or ''
    if 'eternaldarkness.exe' in listed.lower():
        raise SystemExit('the game is open (EternalDarkness.exe): close it first, a test run would close it by name')
    r = subprocess.run(['sh', 'tools/ed/' + script] + args, cwd=disc.ROOT, env=e, capture_output=True, text=True, errors='replace',
                       stdin=subprocess.DEVNULL)
    runs = [l.strip() for l in (r.stdout or '').split('\n') if l.strip().startswith('logs/run-') and not l.strip().endswith('introfail')]
    return (os.path.join(disc.ROOT, runs[-1]) if runs else ''), r.stdout


def control_frame(chapter):
    """The frame both players get control in a chapter of Jump to Game on this exe (one short run, kept per build)."""
    os.makedirs(KEEP, exist_ok=True)
    keep = os.path.join(KEEP, 'control-ch%d.txt' % chapter)
    if os.path.exists(keep):
        stamp, frame = open(keep).read().split()[:2]
        if int(stamp) == exe_stamp():
            return int(frame)
    print('    [start] chapter %d: when do both players get control? (one short run)' % chapter, flush=True)
    run, out = sh('jtg.sh', [str(chapter), 'test-control-ch%d' % chapter], dict(JTG_SET=SETTINGS, JTG_FRAMES=240, ED_RUN_SECONDS=200))
    m = None
    if run and os.path.exists(os.path.join(run, 'jtg-report.txt')):
        m = re.search(r'control at frame (\d+)', open(os.path.join(run, 'jtg-report.txt'), encoding='utf8', errors='replace').read())
    if m is None:
        raise SystemExit('chapter %d: no control from Jump to Game (%s)' % (chapter, run or out[-400:]))
    open(keep, 'w').write('%d %s\n' % (exe_stamp(), m.group(1)))
    return int(m.group(1))


def fresh_state(name):
    """The start's state, made from Jump to Game by this exe: {'path', 'frame', 'settings'}."""
    if name not in STARTS:
        raise SystemExit('no start %s (tools/ed/scenarios.py STARTS)' % name)
    st = STARTS[name]
    path = os.path.join('saves', 'test-%s.state' % name)
    full = os.path.join(disc.ROOT, path)
    meta_path = os.path.join(disc.ROOT, 'saves', 'test-%s.json' % name)
    settings = os.path.join('saves', 'test-%s.settings.ini' % name)
    if os.path.exists(full) and os.path.exists(meta_path):
        meta = json.load(open(meta_path))
        if meta.get('exe') == exe_stamp():
            return meta
    why = ''
    for attempt in range(3):
        F = control_frame(st['chapter'])
        at = F + 60
        env = dict(JTG_SET=SETTINGS, ED_RUN_SECONDS=300)
        if st.get('tome'):
            # The Tome, as the story gives it later in the chapter: spawned at player 1's feet (type 30), picked up and
            # its words closed with B. Doors that test for it (L03/892 sends Anthony to the Tome's room without it) then
            # go where they go for the rest of the chapter.
            env['ED_ITEM_SPAWN'] = '%d,30,30,300,1' % at
            env['ED_INPUT_SCRIPT'] = ';'.join('%d,%d,0x0200' % (at + d, at + d + 8) for d in (30, 90, 130, 170, 210, 250))
            at += 420
        doors = ','.join('%d:%d' % (at + 300 * i, script) for i, script in enumerate(st['scripts']))
        S = at + 300 * len(st['scripts']) + 240
        for old in (full, meta_path):
            if os.path.exists(old):
                os.remove(old)
        print('    [start] %s: a fresh state from Jump to Game, chapter %d%s%s, saved at frame %d%s' % (
            name, st['chapter'], ', the Tome picked up' if st.get('tome') else '', ', door script %s' % doors if doors else '', S,
            ' (try %d)' % (attempt + 1) if attempt else ''), flush=True)
        env.update(ED_SAVE_STATE='%d:%s' % (S, path), ED_RUN_FRAMES=S + 90)
        if doors:
            env['ED_RUN_SCRIPT'] = doors
        run, out = sh('jtg.sh', [str(st['chapter']), 'test-state-%s' % name], env)
        if not run or not os.path.exists(full):
            why = 'no state was saved (%s)' % (run or out[-300:])
            continue
        log = log_text(run)
        report = open(os.path.join(run, 'jtg-report.txt'), encoding='utf8', errors='replace').read() if os.path.exists(os.path.join(run, 'jtg-report.txt')) else ''
        got = re.search(r'control at frame (\d+)', report)
        rows = recording.load(run)
        if got is None or abs(int(got.group(1)) - F) > 45:
            # (The intro's skip sometimes takes much longer: every press of this run then came at the wrong moment.)
            why = 'control came at frame %s, not about %d (%s)' % (got.group(1) if got else 'none', F, run)
            os.remove(os.path.join(KEEP, 'control-ch%d.txt' % st['chapter'])) if got and attempt == 1 else None
            continue
        if 'memory 56 MB' not in log:
            why = 'the run was not co-op with the separate rooms (%s)' % run
            continue
        if not rows or rows[-1].get('room') != st['room']:
            why = 'the state is in room %s, wanted %s (%s)' % (rows[-1].get('room') if rows else None, st['room'], run)
            continue
        m = re.search(r'\[test\] frame (\d+): saving a state', log)
        with open(os.path.join(run, 'settings.ini')) as f, open(os.path.join(disc.ROOT, settings), 'w') as g:
            g.write(f.read())
        meta = {'exe': exe_stamp(), 'path': path, 'frame': int(m.group(1)) if m else S, 'settings': settings, 'run': os.path.relpath(run, disc.ROOT)}
        json.dump(meta, open(meta_path, 'w'))
        return meta
    if os.path.exists(full):
        os.remove(full)
    raise SystemExit('%s: no good state after 3 tries: %s' % (name, why))


def moved(text, shift, frames, sep=';'):
    """A list of presses or puts with its first `frames` fields moved by shift."""
    out = []
    for item in text.split(sep):
        if not item.strip():
            continue
        f = item.split(',')
        for i in range(min(frames, len(f))):
            f[i] = str(int(f[i]) + shift)
        out.append(','.join(f))
    return sep.join(out)


def load_all():
    out = {}
    for path in sorted(glob.glob(os.path.join(HERE, '*.json'))):
        s = json.load(open(path, encoding='utf8'))
        out[s['name']] = s
    return out


def run_game(s):
    name = 'test-' + s['name']
    st = fresh_state(s['start'])
    shift = st['frame'] + SETTLE - s['base']
    # As the user plays (play.bat): two room slots and the other player staying at a door. "follow": the slots alone,
    # so player 2 is brought through (the kit's "together").
    env = {'ED_RUN_FRAMES': s['frames'] + shift, 'ED_RUN_SECONDS': s.get('seconds', 240), 'ED_SETTINGS': st['settings'], 'ED_ROOM_SLOTS': 2}
    if s.get('mode') != 'follow':
        env['ED_COOP_STAY'] = 1
    env['ED_INPUT_SCRIPT'] = moved(s.get('p1', ''), shift, 2)
    env['ED_PAD2_SCRIPT'] = moved(s.get('p2', ''), shift, 2)
    env['ED_RUN_SCRIPT'] = s.get('run_script', '')  # (its delays count from the load)
    env['ED_PUT'] = ';'.join('%d,%d,%g,%g,%g' % ((st['frame'] + 20, who) + tuple(s['put'][key]))
                             for who, key in ((1, 'p1'), (2, 'p2')) if key in s.get('put', {}))
    for k, v in s.get('env', {}).items():
        env[k] = moved(v, shift, 1) if k in ('ED_COOP_P2_PUT', 'ED_QUICK_SET', 'ED_SANITY_P2_TEST') else v  # (those start with a frame)
    # run.sh's first line is this run's folder. A name search could find an older run when this one never started, and pass it.
    run, out = sh('run.sh', [st['path'], name], env)
    if not run or not os.path.exists(os.path.join(run, 'both.tsv')) or os.path.getsize(os.path.join(run, 'both.tsv')) < 2000:
        return None, out
    open(os.path.join(run, 'test-shift.txt'), 'w').write('%d\n' % shift)
    return run, out


def shift_of(run):
    try:
        return int(open(os.path.join(run, 'test-shift.txt')).read().split()[0])
    except (OSError, ValueError, IndexError):
        return 0


def log_text(run):
    text = []
    for path in sorted(glob.glob(os.path.join(run, 'page-*.log'))):
        text.append(open(path, encoding='utf8', errors='replace').read())
    return '\n'.join(text)


def span(rows, c, shift=0):
    if 'during' in c:
        d = c['during']
        test = {'window': lambda r: r['window'], 'busy': lambda r: r['busy'], 'screen': lambda r: r['screen'],
                'words_p1': lambda r: r['window'] and not r['screen'], 'words_p2': lambda r: r['window'] and r['screen']}[d]
        return [r for r in rows if test(r)]
    return [r for r in rows if c.get('from', 0) + shift <= r['frame'] <= c.get('to', 1 << 60) + shift]


def walked(rows, who):
    return sum(recording.dist(recording.pos(a, who), recording.pos(b, who)) for a, b in zip(rows, rows[1:]) if b['frame'] - a['frame'] < 4)


def judge(s, run, rows, shift=0):
    fails = []
    log = log_text(run)
    rooms = recording.room_cameras()
    for c in s.get('expect', []):
        kind = c['check']
        if kind == 'log' and not re.search(c['pattern'], log):
            fails.append('never logged: %s' % c['pattern'])
        elif kind == 'flicker':
            import gfxcheck
            probe = os.path.join(run, 'probe.txt')
            if not os.path.exists(probe):
                # (Written only for split views: a run that played solo has none. It crashed the whole suite before.)
                fails.append('player %d: the run wrote no probe.txt (no split screen: did it play solo?)' % (c['view'] + 1))
                continue
            v = gfxcheck.summary(probe, c.get('threshold', 0.25), skip=SETTLE + 50 if shift else 0).get('views', {}).get(c['view'])
            if v is None:
                fails.append('player %d: no probe of their view' % (c['view'] + 1))
            else:
                if len(v['col']['spikes']) > c.get('spikes', 0):
                    fails.append('player %d: %d one-frame spikes: %s' % (c['view'] + 1, len(v['col']['spikes']), v['col']['spikes'][:5]))
                if 'max_step' in c and v['col']['max'] > c['max_step']:
                    fails.append('player %d: picture jumped by %.2f (at most %.2f)' % (c['view'] + 1, v['col']['max'], c['max_step']))
                if v['tile_drops']:
                    fails.append('player %d: shadows dropped out for a frame at %s' % (c['view'] + 1, v['tile_drops'][:5]))
        elif kind == 'no_log':
            m = re.search(c['pattern'], log)
            if m:
                fails.append('logged: %s' % m.group(0)[:100])
        elif kind == 'count':
            n = len(re.findall(c['pattern'], log))
            if n != c['n']:
                fails.append('%s logged %d times, wanted %d' % (c['pattern'], n, c['n']))
        elif kind == 'moved':
            part = span(rows, c, shift)
            d = walked(part, c['who'])
            if not part:
                fails.append('%s: that moment never came (%s)' % (c['who'], c.get('during', c.get('from'))))
            elif d < c['min']:
                fails.append('%s walked %.0f during %s, wanted at least %d (frames %d to %d)' % (c['who'], d, c.get('during', 'the span'), c['min'], part[0]['frame'], part[-1]['frame']))
        elif kind == 'stayed':
            part = span(rows, c, shift)
            d = walked(part, c['who'])
            if d > c['max']:
                fails.append('%s walked %.0f, wanted at most %d' % (c['who'], d, c['max']))
        elif kind == 'camera':
            r = min(rows, key=lambda x: abs(x['frame'] - (c['frame'] + shift)))
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
    bad = skipped = 0
    for k in names:
        s = all_s.get(k)
        if s is None:
            raise SystemExit('no scenario %s (--list)' % k)
        print('=== %s: %s' % (k, s.get('proves', '')), flush=True)
        if s.get('skip'):
            print('    SKIPPED (not run, not passed): %s' % s['skip'], flush=True)
            skipped += 1
            continue
        if rejudge:
            run = recording.find_run('test-' + k)
        else:
            run, _ = run_game(s)
        if run is None:
            print('    FAIL the game didn\'t start or wrote no recording')
            print('    FAILED', flush=True)
            bad += 1
            continue
        shift = shift_of(run)
        rows = recording.load(run)
        fails = judge(s, run, rows, shift)
        if rows:
            print('    %s: %d frames, to frame %d (the scenario\'s numbers moved by %+d)' % (os.path.basename(run), len(rows), rows[-1]['frame'], shift))
        if rows and rows[-1]['frame'] < s['frames'] + shift - 30:
            fails.insert(0, 'stopped at frame %d, before %d: a freeze?' % (rows[-1]['frame'], s['frames'] + shift))
        for f in fails:
            print('    FAIL ' + f)
        print('    %s' % ('FAILED' if fails else 'passed'), flush=True)
        bad += bool(fails)
    ran = len(names) - skipped
    print('%d of %d passed%s' % (ran - bad, ran, ', %d skipped (not run)' % skipped if skipped else ''))
    return bad
