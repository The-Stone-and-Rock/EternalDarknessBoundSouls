"""coopcheck: the co-op gate. Each chapter from Jump to Game, no save state, one launch a chapter: both players walk, open
and close their Tomes, examine and pick up, take a door each (the game's own door, by its real zone and prompt), and fight
where someone is placed, each as the actor while the other walks. Then the game's rules are checked on the recording, for
both players, and a table says what passed.

  python -I tools/ed/coopcheck.py [CHAPTERS] [options]      CHAPTERS: 1 4 7, 1-12 (default all 12)
    --quick              a walk, both Tomes, one examine and one pickup each, a door each (about 2 minutes a chapter)
    --fight / --no-fight a finisher by each where the tables place someone (default: on, off with --quick)
    --door-mode script   the door's own script from the host (ED_RUN_SCRIPT), the fallback for a door the real way can't take
    --p1 N --p2 N        Jump to Game characters 1-12 (JTG_P1, JTG_P2; 0, the default for player 1, is the chapter's own;
                         player 2 defaults to the next chapter's, so the 12 chapters have the 12 characters as player 2).
    --user               the user's own settings.ini characters and forms, as they play
    --solo               player 1 alone, co-op off (JTG_COOP=0: jtg.sh must honour it), the one-player game, stored as the
                         reference the co-op run is held to
    --plan               print each chapter's plan and stop (no game)
    --judge RUN          judge a run again (its coopcheck.json, or the plan-free rules on any old run)
    --last               judge the newest coopcheck run of each chapter again, with no game
    --sheets             a picture sheet for each failed step (ed sheet), in Screenshots for Claude/coopcheck
    --no-retime          don't launch again when the run's base is off the plan's
    --recording-patch    print the hle.c recorder additions this reads when they are there

Needs tools/ed/jtg.sh: `sh tools/ed/jtg.sh CHAPTER NAME`, cold boot -> title -> Jump to Game -> chapter, with no save
state. It keeps absolute ED_INPUT_SCRIPT / ED_PAD2_SCRIPT / ED_PUT after its own menu presses, passes ED_RUN_SCRIPT and the
rest through, closes at ED_RUN_FRAMES (absolute), records as run.sh does under logs/run-*-jtg-NAME, writes its own
settings copy (the user's settings.ini, co-op on, JTG_P1 / JTG_P2 characters) and prints the run folder (again on each
intro retry, and last).

Timing: the presses count the game's frames, and when Jump to Game hands over isn't known before a launch. The frame both
players are first in control, per chapter, characters and build, is kept in build/coopcheck/bases.json. With none, a
calibration launch finds it (the game is closed as soon as both are in control). A run whose base is more than 30 frames
off is launched once more with its own.

Limits, said in every report: ED_INPUT_SCRIPT goes round controls.cpp, so player 1's held-over and Tome button handling
isn't the user's; a plane only the game's player crosses is taken by script; nobody should hold a controller (the script
pad joins as controller 3).
"""
import glob
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coopjudge  # noqa: E402
import coopplan  # noqa: E402
import disc  # noqa: E402
import recording  # noqa: E402

ROOT = disc.ROOT
OUT = os.path.join(ROOT, 'build', 'coopcheck')
PICS = os.path.join(ROOT, 'Screenshots for Claude', 'coopcheck')
JTG = os.path.join(ROOT, 'tools', 'ed', 'jtg.sh')
EXE = os.path.join(ROOT, 'build-win', 'EternalDarkness.exe')
RETIME = 30
FPS = 60.0

COLUMNS = [('walk', 'both'), ('tome', 'p1'), ('tome', 'p2'), ('examine', 'p1'), ('examine', 'p2'), ('pickup', 'p1'),
           ('pickup', 'p2'), ('door', 'p1'), ('door', 'p2'), ('fight', 'p1'), ('fight', 'p2')]
HEAD = ['walk', 'tome1', 'tome2', 'exam1', 'exam2', 'pick1', 'pick2', 'door1', 'door2', 'fin1', 'fin2']


def sh():
    return shutil.which('sh') or r'C:\Program Files\Git\bin\sh.exe'


def chapters_from(args):
    out = []
    for a in args:
        if re.fullmatch(r'\d+-\d+', a):
            x, y = map(int, a.split('-'))
            out += list(range(x, y + 1))
        elif a.isdigit():
            out.append(int(a))
    return [c for c in out if c in coopplan.CHAPTERS] or list(range(1, 13))


def flag(argv, name, default=None):
    if name in argv:
        i = argv.index(name)
        return argv[i + 1] if i + 1 < len(argv) else default
    return default


# ---- settings, timing, references ----------------------------------------------------------------------------------

def settings_hash():
    """The user's settings.ini, which jtg.sh copies for each run (co-op on, JTG_P1, JTG_P2): its options and forms."""
    path = os.path.join(ROOT, 'settings.ini')
    return hashlib.sha1(open(path, 'rb').read()).hexdigest()[:10] if os.path.exists(path) else '-'


def build_stamp():
    parts = []
    for p in (EXE, disc.DOL):
        parts.append('%d' % os.path.getmtime(p) if os.path.exists(p) else '-')
    return '.'.join(parts)


def store(name):
    path = os.path.join(OUT, name)
    return json.load(open(path)) if os.path.exists(path) else {}


def keep(name, data):
    os.makedirs(OUT, exist_ok=True)
    json.dump(data, open(os.path.join(OUT, name), 'w'), indent=1)


def base_key(chapter, p1, p2, solo):
    return '%d|%s|%s|%s|%s|%s' % (chapter, p1, p2, int(solo), settings_hash(), build_stamp())


def ref_key(chapter, model):
    return '%d|%d' % (chapter, model)


# ---- launching ------------------------------------------------------------------------------------------------

def taskkill():
    subprocess.run(['taskkill', '/F', '/IM', 'EternalDarkness.exe'], capture_output=True)


def game_running():
    out = subprocess.run(['tasklist'], capture_output=True, text=True, errors='replace').stdout
    return 'eternaldarkness.exe' in out.lower()


def launch(chapter, name, env, until=None, seconds=600):
    """jtg.sh with this environment. until(rows) -> True closes the game early (calibration). A recording that stops
    growing for 25 s with the game still up is a freeze: closed, and said so. Returns (run folder, stdout, froze)."""
    if not os.path.exists(JTG):
        raise SystemExit('tools/ed/jtg.sh is not there yet: it takes the launcher to Jump to Game and chapter N. coopcheck needs it.')
    for _ in range(30):  # jtg.sh refuses while a copy is up: the last one may still be closing
        if not game_running():
            break
        time.sleep(1.0)
    full = dict(os.environ)
    full.update(env)
    proc = subprocess.Popen([sh(), JTG, str(chapter), name], cwd=ROOT, env=full, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace')
    out, run, froze = [], [None], [False]

    def read():
        for line in proc.stdout:
            out.append(line)
            m = re.match(r'\s*(logs[/\\]run-\S+)\s*$', line)
            if m and not m.group(1).endswith('-introfail'):
                run[0] = os.path.join(ROOT, m.group(1))  # the latest: jtg.sh starts again when the intro skip misses
    reader = threading.Thread(target=read, daemon=True)
    reader.start()
    started, size, grew = time.time(), -1, time.time()
    while proc.poll() is None:
        time.sleep(1.0)
        if run[0] is None:
            if time.time() - started > 120:
                taskkill()
            continue
        if not os.path.isdir(run[0]):
            continue  # renamed -introfail: the next attempt's line comes
        tsv = os.path.join(run[0], 'both.tsv')
        now = os.path.getsize(tsv) if os.path.exists(tsv) else -1
        if now != size:
            size, grew = now, time.time()
        elif now > 0 and time.time() - grew > 25:
            froze[0] = True
            taskkill()
        if until and now > 2000:
            try:
                if until(recording.load(run[0])):
                    taskkill()
            except SystemExit:
                pass
        if time.time() - started > seconds:
            taskkill()
    reader.join(5)
    if run[0] is None:
        runs = sorted(glob.glob(os.path.join(ROOT, 'logs', 'run-*-' + name)), key=os.path.getmtime)
        run[0] = runs[-1] if runs else None
    return run[0], ''.join(out), froze[0]


def common_env(chars):
    """What every launch has: the characters (jtg.sh's JTG_P1, JTG_P2, JTG_COOP), full stats so a fight doesn't end the
    run, and the camera stack's log for a camera finding."""
    env = dict(ED_CHEAT='2', ED_RUN_SHOTS_MS='500', ED_RUN_SHOTS_W='960', ED_CAMSTACK_LOG='1')
    env.update(chars)
    return env


def calibrate(chapter, chars, solo):
    """One launch to find the chapter's base: no presses, closed as soon as both are in control."""
    name = 'coopcheck-ch%02d-base' % chapter
    env = common_env(chars)
    env.update(ED_RUN_FRAMES='999999999', ED_RUN_SECONDS='300', ED_RUN_SHOTS_MS='5000')

    def ready(rows):
        st = coopjudge.playable(rows, solo)
        return st is not None and rows[-1]['frame'] > st['frame'] + 40
    run, out, froze = launch(chapter, name, env, until=ready, seconds=320)
    if not run or not os.path.exists(os.path.join(run, 'both.tsv')):
        return None, 'no recording (%s)' % (out.strip().split('\n')[-1][:200] if out.strip() else 'no output')
    rows = recording.load(run)
    st = coopjudge.playable(rows, solo)
    if st is None:
        return None, 'never in control (%s, last frame %s)' % (os.path.basename(run), rows[-1]['frame'] if rows else '-')
    return dict(base=st['frame'], first=rows[0]['frame'], room=st['room'], run=os.path.basename(run)), None


# ---- one chapter --------------------------------------------------------------------------------------------------

def fit(chapter, opts):
    """The fullest plan whose presses hle.c can read in one launch."""
    for each in ((1,) if opts['quick'] else (3, 2, 1)):
        plan = coopplan.build(chapter, quick=opts['quick'], fight=opts['fight'], door_mode=opts['door_mode'], solo=opts['solo'], max_each=each)
        env, problems = coopplan.render(plan, 100000, 15)
        if not problems:
            return plan
    plan = coopplan.build(chapter, quick=True, fight=opts['fight'], door_mode=opts['door_mode'], solo=opts['solo'])
    plan['notes'].append('cut to the quick plan: the full one has more presses than hle.c reads in one launch')
    return plan


def run_chapter(chapter, opts):
    name, level, start_room, model = coopplan.CHAPTERS[chapter]
    if opts['user']:
        p1 = p2 = None  # the user's settings.ini as it is
        chars = {}
    else:
        p1 = opts['p1']
        p2 = opts['p2'] if opts['p2'] is not None else chapter % 12 + 1
        chars = dict(JTG_P1=str(p1), JTG_P2=str(p2))
    if opts['solo']:
        chars['JTG_COOP'] = '0'
    plan = fit(chapter, opts)
    plan.update(p1_character=p1, p2_character=None if opts['solo'] else p2)
    bases = store('bases.json')
    key = base_key(chapter, p1, p2, opts['solo'])
    timing = bases.get(key)
    if timing is None:
        print('  finding when Jump to Game hands chapter %d over (one launch, no presses)...' % chapter, flush=True)
        timing, err = calibrate(chapter, chars, opts['solo'])
        if timing is None:
            return dict(chapter=chapter, plan=plan, error='calibration: ' + err)
        bases[key] = timing
        keep('bases.json', bases)
    tries = 0
    while True:
        tries += 1
        env, problems = coopplan.render(plan, timing['base'], timing['first'])
        if problems:
            return dict(chapter=chapter, plan=plan, error='; '.join(problems))
        env.update(common_env(chars))
        end = timing['base'] + plan['end']
        env.update(ED_RUN_FRAMES=str(end), ED_RUN_SECONDS=str(int((end - timing['first']) / FPS * 2.5) + 90))
        label = 'coopcheck-ch%02d-%s%s%s' % (chapter, name, '-quick' if plan['quick'] else '', '-solo' if plan['solo'] else '')
        run, out, froze = launch(chapter, label, env, seconds=int(env['ED_RUN_SECONDS']) + 60)
        if not run or not os.path.exists(os.path.join(run, 'both.tsv')):
            return dict(chapter=chapter, plan=plan, error='no recording: %s' % (out.strip().split('\n')[-1][:200] if out.strip() else 'jtg.sh printed nothing'))
        json.dump(dict(plan=plan, timing=timing, env=env, froze=froze), open(os.path.join(run, 'coopcheck.json'), 'w'), indent=1)
        rows = recording.load(run)
        if opts['solo'] and rows and any(r['p2m'] >= 0 for r in rows[-60:]):
            return dict(chapter=chapter, plan=plan, error='jtg.sh ran co-op: --solo needs it to honour JTG_COOP=0 (mod_coop = 0)')
        result = judge_run(run, plan, timing)
        result['froze'] = froze
        got = result.get('base')
        if got is not None and abs(got - timing['base']) > RETIME and not opts['no_retime'] and tries == 1:
            print('  in control at frame %d, the plan was for %d: launching again on its own timing' % (got, timing['base']), flush=True)
            timing = dict(timing, base=got, first=result['first'])
            bases[key] = timing
            keep('bases.json', bases)
            continue
        if got is not None and abs(got - timing['base']) <= RETIME:
            bases[key] = dict(timing, base=got, first=result['first'])
            keep('bases.json', bases)
        if opts['solo']:
            keep_reference(chapter, model if not p1 else coopplan.CHAPTERS[p1][3], result)
        return result


def judge_run(run, plan, timing=None):
    refs = store('reference.json')
    p1 = plan.get('p1_character') or 0
    model = plan['model'] if not p1 else coopplan.CHAPTERS[p1][3]
    ref = None if plan['solo'] else refs.get(ref_key(plan['chapter'], model))
    # The plan's frames go on from the run's own base when it's near the planned one (a few frames of drift), else from
    # the planned one, where the presses really were.
    start, verdicts, findings, lacks = coopjudge.judge(run, plan, ref, base=None)
    rows = recording.load(run)
    if start is not None and timing and abs(start['frame'] - timing['base']) > RETIME:
        start2, verdicts, findings, lacks = coopjudge.judge(run, plan, ref, base=timing['base'])
    return dict(chapter=plan['chapter'], plan=plan, run=run, base=start['frame'] if start else None, first=rows[0]['frame'] if rows else None,
                verdicts=verdicts, findings=findings, lacks=lacks)


def keep_reference(chapter, model, result):
    """The one-player game's answers, for the co-op run to be held to: the camera each door arrives on, the finisher."""
    refs = store('reference.json')
    ref = refs.setdefault(ref_key(chapter, model), {'doors': {}})
    for v in result.get('verdicts', []):
        if v.step['kind'] == 'door' and v.status == 'PASS' and getattr(v, 'result_room', None) is not None:
            cam = [t for n, ok, t in v.checks if n == 'arrival camera']
            if cam:
                ref['doors']['%d->%d' % (v.step['script'], v.result_room)] = {'camera': cam[0].split(' on ', 1)[1].split(';')[0]}
        if v.step['kind'] == 'fight' and getattr(v, 'finisher', None):
            ref['finisher'] = v.finisher
    refs[ref_key(chapter, model)] = ref
    keep('reference.json', refs)


# ---- reports ------------------------------------------------------------------------------------------------------

def cell(verdicts, kind, actor):
    vs = [v for v in verdicts if v.step['kind'] == kind and v.step['actor'] == actor]
    if not vs:
        return '-'
    order = ['FAIL', 'BLOCKED', 'SKIP', 'PASS']
    worst = min((v.status for v in vs), key=order.index)
    return {'FAIL': 'FAIL', 'BLOCKED': 'BLK', 'SKIP': 'skip', 'PASS': 'ok'}[worst] + ('' if len(vs) == 1 else '%d' % len(vs))


def report(result, out):
    plan = result['plan']
    out.append('=== chapter %d %s, level %d, player 1 %s, player 2 %s%s' % (
        plan['chapter'], plan['name'], plan['level'], coopplan_name(plan.get('p1_character')), coopplan_name(plan.get('p2_character')) if not plan['solo'] else '-',
        ', solo' if plan['solo'] else ''))
    if result.get('error'):
        out.append('    ERROR ' + result['error'])
        return
    out.append('    run %s, in control at frame %s' % (os.path.relpath(result['run'], ROOT), result['base']))
    for v in result['verdicts']:
        s = v.step
        what = s['kind'] + (' ' + s.get('region', '') if s.get('region') else '') + (' -> %s' % s.get('to_room') if s['kind'] == 'door' else '')
        out.append('    %-7s %-4s +%-5d %-38s %s%s' % (v.status, s['actor'], s['at'], what[:38], v.why, ''))
        for n, ok, t in v.checks:
            if ok is False:
                out.append('             FAIL %-18s %s' % (n, t))
            elif ok is None:
                out.append('             info %-18s %s' % (n, t))
    for f in result['findings']:
        out.append('    %-17s %s %s-%s %s' % (f['rule'], f.get('who', '').upper(), f.get('from', ''), f.get('to', ''), f['text']))
    if result.get('froze'):
        out.append('    the recording stopped growing for 25 s with the game up: closed as a freeze')
    if result.get('lacks'):
        out.append('    not checked, the recorder lacks %s (--recording-patch)' % ', '.join(result['lacks']))
    for n in result['plan'].get('notes', []):
        out.append('    note: ' + n)


def coopplan_name(n):
    if n is None:
        return "the user's"
    return 'the chapter\'s own' if n == 0 else coopplan.CHAPTERS[n][0]


def table(results):
    lines = ['chapter            ' + ' '.join('%-6s' % h for h in HEAD) + ' rules']
    for r in results:
        plan = r['plan']
        if r.get('error'):
            lines.append('%2d %-15s %s' % (plan['chapter'], plan['name'], 'ERROR ' + r['error'][:90]))
            continue
        cells = [cell(r['verdicts'], k, a) for k, a in COLUMNS]
        bad = [f for f in r['findings'] if f['rule'] not in ('TOME_UNASKED', 'PICTURE', 'BOUNCE')]
        rules = 'ok' if not bad else ','.join(sorted({f['rule'] for f in bad}))
        lines.append('%2d %-15s %s %s' % (plan['chapter'], plan['name'], ' '.join('%-6s' % c for c in cells), rules))
    return lines


def sheets(result):
    """A captioned picture sheet for each failed step (ed sheet), copied to Screenshots for Claude/coopcheck."""
    os.makedirs(PICS, exist_ok=True)
    base = result['base']
    for v in result['verdicts']:
        if v.status != 'FAIL':
            continue
        s = base + v.step['at']
        picks = [s + 30, s + v.step['len'] // 3, s + 2 * v.step['len'] // 3, s + v.step['len'] - 20]
        made = subprocess.run([sys.executable, '-I', os.path.join(ROOT, 'tools', 'ed', 'ed.py'), 'sheet', os.path.basename(result['run'])] + [str(x) for x in picks],
                              cwd=ROOT, capture_output=True, text=True, errors='replace').stdout.strip().split('\n')[-1].strip()
        if os.path.exists(made):
            shutil.copy(made, os.path.join(PICS, 'ch%02d-%s-%s-%d.jpg' % (result['plan']['chapter'], v.step['kind'], v.step['actor'], v.step['at'])))


def recording_patch():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'coopcheck-recording.diff')
    print(open(path, encoding='utf8').read() if os.path.exists(path) else 'tools/ed/coopcheck-recording.diff is missing')


def main(argv):
    if '--recording-patch' in argv:
        recording_patch()
        return 0
    if '--judge' in argv:
        run = recording.find_run(flag(argv, '--judge'))
        meta = os.path.join(run, 'coopcheck.json')
        out = []
        if os.path.exists(meta):
            m = json.load(open(meta))
            result = judge_run(run, m['plan'], m.get('timing'))
            report(result, out)
            out += [''] + table([result])
        else:
            start, findings = coopjudge.any_run(run)
            out.append('%s: no coopcheck plan, the plan-free rules from frame %s' % (os.path.basename(run), start and start['frame']))
            for f in findings:
                out.append('    %-17s %s %s-%s %s' % (f['rule'], f.get('who', '').upper(), f.get('from', ''), f.get('to', ''), f['text']))
        print('\n'.join(out))
        return 0
    opts = dict(quick='--quick' in argv, fight=True if '--fight' in argv else False if '--no-fight' in argv else None,
                door_mode=flag(argv, '--door-mode', 'real'), solo='--solo' in argv, no_retime='--no-retime' in argv,
                p1=int(flag(argv, '--p1', 0)), p2=int(flag(argv, '--p2')) if flag(argv, '--p2') else None, user='--user' in argv)
    skip = {flag(argv, n) for n in ('--door-mode', '--p1', '--p2', '--judge')}
    chapters = chapters_from([a for a in argv if not a.startswith('--') and a not in skip])
    if '--plan' in argv:
        for c in chapters:
            plan = fit(c, opts)
            print('\n'.join(coopplan.describe(plan)))
            env, problems = coopplan.render(plan, 1000, 15)
            print('  player 1 %d characters, player 2 %d, puts %d%s' % (len(env['ED_INPUT_SCRIPT']), len(env['ED_PAD2_SCRIPT']), len(env['ED_PUT']),
                                                                       '; ' + '; '.join(problems) if problems else ''))
        return 0
    results = []
    if '--last' in argv:
        for c in chapters:
            runs = sorted(glob.glob(os.path.join(ROOT, 'logs', 'run-*-coopcheck-ch%02d-*' % c)), key=os.path.getmtime)
            runs = [r for r in runs if os.path.exists(os.path.join(r, 'coopcheck.json'))]
            if runs:
                m = json.load(open(os.path.join(runs[-1], 'coopcheck.json')))
                results.append(judge_run(runs[-1], m['plan'], m.get('timing')))
    else:
        for c in chapters:
            print('chapter %d %s...' % (c, coopplan.CHAPTERS[c][0]), flush=True)
            t = time.time()
            r = run_chapter(c, opts)
            r['seconds'] = time.time() - t
            results.append(r)
            one = []
            report(r, one)
            print('\n'.join(one), flush=True)
            if '--sheets' in argv and not r.get('error'):
                sheets(r)
    out = []
    for r in results:
        report(r, out)
    out += ['', 'Each chapter from Jump to Game, no save state. ok, FAIL, skip (the test couldn\'t: no floor, no one to fight),',
            'BLK (an earlier step left them elsewhere), - (the tables have none on the route). "rules": the whole run.'] + table(results)
    out.append('Not covered: player 1\'s presses go round controls.cpp (the user\'s held buttons and Tome handling), doors by '
               'script are player 1\'s whoever is the actor, and a forced Tome (the help pages) is only flagged.')
    print('\n'.join(out[-(len(results) + 6):]) if '--last' not in argv else '\n'.join(out))
    os.makedirs(OUT, exist_ok=True)
    stamp = time.strftime('%Y%m%d-%H%M%S')
    open(os.path.join(OUT, 'summary-%s.txt' % stamp), 'w', encoding='utf8').write('\n'.join(out) + '\n')
    fails = sum(1 for r in results if r.get('error') or any(v.status == 'FAIL' for v in r['verdicts']))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
