"""`ed check TARGET [options]`: one script or region checked in play, solo, together and apart, both players recorded,
and the runs compared. What docs/research/kit-bugs/build-run-*.sh did by hand for each bug, from the kit's own tables.

TARGET   L09/1951            a script: the room and region whose own field (enter, cross, stay) runs it
         273:SphereRegion01  a room and a region (give --level when the room is in more than one level)
Options  --modes solo,together,apart,p1   which runs (default solo,together,apart; p1: together, player 1 in it)
         --press a|b|none    the press in the region, 60 frames after the put (default b: the game's action button)
         --flag N [--flag N] flags set before the door in (ED_SET_FLAG), for a script that waits on them
         --wait N            frames after the press before the run closes (default 1200)
         --hero              ED_COOP_HERO_ID=1 (the hero gates open for player 2)
         --env K=V           any other switch for every run (repeatable)
         --dry               write the plan and print it, run nothing

The route: Jump to Game to the level's chapter (coopplan.CHAPTERS); its control frame F from a probe run (kept per
build in build/check/probe-chN.txt); a door script into the room run at F+60 (ED_RUN_SCRIPT, from the doors table:
`ed door`), after the flags at F+30; a state saved there in follow mode for the co-op runs; for apart, a door out of the
room run for player 1 (ED_COOP_STAY=1 keeps player 2). The region's player is put in its middle (ED_PUT), a short step
for the zone look, then the press. Every run: ED_RUN_AWAY=1, pictures 4 a second, both.tsv, the coop logs.

The report (build/check/NAME/report.md, printed): for each run, every run of the target script: who ran it (player 1 /
player 2 by r13-21496), from where, its calls and where it stopped (`ed stops`); the [coop]/[hero]/[stay]/[sound] lines;
hang reports before and after; then the comparison: the furthest line the script reached solo against each co-op run,
and any co-op run where it ran as the wrong player or stopped sooner. A contact sheet of each run around the press.
"""
import glob
import json
import os
import re
import subprocess
import sys
import time

import coopplan
import disc
import stops

ROOT = disc.ROOT
OUT = os.path.join(ROOT, 'build', 'check')
SH = r'C:\Program Files\Git\bin\sh.exe' if os.name == 'nt' else 'sh'
EXE = os.path.join(ROOT, 'build-win', 'EternalDarkness.exe')
LOG_LINES = re.compile(r'^\[(coop|hero|stay|sound|tome|title|plaques|room)\]')
# Every-frame or start-up chatter, left out of the report (the run's page logs keep it all).
NOISE = re.compile(r'^\[coop\] player [12] 0x|pad: buttons|PADRead|the player object|^\[room\] (later|request|ticket)|first turn ran|step runs')


def die(msg):
    raise SystemExit('ed check: ' + msg)


def chapter_of(level):
    for ch, v in coopplan.CHAPTERS.items():
        if v[1] == level:
            return ch, v[2]
    die('level %02d is no Jump to Game chapter (levels 1-12 are; the mansion, 00, needs a new game)' % level)


def regions_running(level, script):
    """(room, region, field) for each region whose own field runs the script in that level's rooms."""
    out = []
    rooms = coopplan.rooms()
    levels = json.load(open(os.path.join(disc.DB, 'facts', 'levels.json'), encoding='utf8'))
    lv = levels.get('%02d' % level) if isinstance(levels, dict) else None
    in_level = set(str(r) for r in (lv.get('rooms', []) if isinstance(lv, dict) else []))
    for rid, room in rooms.items():
        if in_level and rid not in in_level:
            continue
        for r in room.get('regions', []):
            for f in ('enter', 'cross_a', 'cross_b', 'stay'):
                if r.get(f) == script:
                    out.append((int(rid), r['name'], f))
    return out


def door_in(level, room):
    """A door script that takes the player into the room (the plainest: no asks, one destination)."""
    best = None
    for d in coopplan.table('doors_table.json'):
        if d.get('to_room') != room or d.get('level') not in ('%02d' % level, '00') or d.get('entry_in_to_room') is False:
            continue
        key = (0 if d['level'] == '%02d' % level else 1, len(d.get('asks_in_script') or []), d['script'])
        if best is None or key < best[0]:
            best = (key, d)
    return best[1] if best else None


def door_out(level, room):
    for d in coopplan.doors_out(level, room):
        if d['door_kind'] in ('exit', 'zone', 'asks') and d['to'][0]['to_room'] != room:
            return d
    return None


def newest(pattern):
    found = sorted(glob.glob(os.path.join(ROOT, 'logs', pattern)))
    return found[-1] if found else None


def hangs():
    return len(glob.glob(os.path.join(ROOT, 'saves', 'reports', 'hang-*.txt')))


def run_sh(script, args, env, label):
    """Runs tools/ed/<script> with the env and returns the new run folder (or None)."""
    before = set(glob.glob(os.path.join(ROOT, 'logs', 'run-*')))
    e = dict(os.environ)
    for k in ('ED_COOP_STAY', 'ED_QUICKLOAD', 'ED_TITLE_STATE'):
        e.pop(k, None)
    e.update({k: str(v) for k, v in env.items() if v is not None})
    print('[check] %s: %s %s' % (label, script, ' '.join('%s=%s' % kv for kv in sorted(env.items()) if kv[0] != 'ED_RUN_AWAY')))
    sys.stdout.flush()
    subprocess.run([SH, os.path.join('tools', 'ed', script)] + args, cwd=ROOT, env=e, stdin=subprocess.DEVNULL,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    made = sorted(set(glob.glob(os.path.join(ROOT, 'logs', 'run-*'))) - before)
    made = [m for m in made if not m.endswith('introfail')]
    return made[-1] if made else None


def control_frame(run):
    try:
        text = open(os.path.join(run, 'jtg-report.txt'), encoding='utf8', errors='replace').read()
    except OSError:
        return None
    m = re.search(r'control at frame (\d+), room (\S+?):', text)
    return (int(m.group(1)), int(m.group(2), 0)) if m else None


def probe(ch):
    """The chapter's control frame on this build (a cold Jump to Game, no presses), kept per build."""
    os.makedirs(OUT, exist_ok=True)
    keep = os.path.join(OUT, 'probe-ch%d.txt' % ch)
    if os.path.exists(keep) and os.path.getmtime(keep) > os.path.getmtime(EXE):
        return int(open(keep).read().split()[0])
    run = run_sh('jtg.sh', [str(ch), 'check-probe'], dict(ED_RUN_AWAY=1, JTG_SLOT=1, JTG_FRAMES=300), 'probe chapter %d' % ch)
    cf = control_frame(run) if run else None
    if cf is None:
        die('no control frame from the probe (%s)' % run)
    open(keep, 'w').write('%d %d\n' % cf)
    return cf[0]


def lines_of(run):
    out = []
    for p in sorted(glob.glob(os.path.join(run, 'page-*.log'))):
        for line in open(p, encoding='utf8', errors='replace'):
            if LOG_LINES.match(line) and not NOISE.search(line):
                out.append(line.rstrip())
    return out


def script_runs(run, script, level):
    """Each traced run of the script: who, from where, calls, the line it stopped at."""
    out = []
    for r in stops.runs_of(run):
        if r['script'] != script:
            continue
        w = stops.where_stopped(script, '%02d' % level, r['calls'])
        last = None
        if w is not None:
            matched = w[0]
            last = (matched[-1] + 1) if matched else 0
        out.append(dict(frame=r['frame'], who=r['who'], lr=stops.LR_NAMES.get(r['lr'], r['lr']), calls=len(r['calls']),
                        last=last, full=len(r['calls']) >= 40))
    return out


def cmd_check(argv):
    if not argv or argv[0] in ('-h', '--help'):
        raise SystemExit(__doc__)
    opts = dict(modes='solo,together,apart', press='b', wait=1200, level=None, dry=False, hero=False)
    flags, extra = [], {}
    target = None
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ('--modes', '--press', '--wait', '--level'):
            opts[a[2:]] = argv[i + 1]
            i += 2
        elif a == '--flag':
            flags.append(int(argv[i + 1]))
            i += 2
        elif a == '--env':
            k, _, v = argv[i + 1].partition('=')
            extra[k] = v
            i += 2
        elif a in ('--dry', '--hero'):
            opts[a[2:]] = True
            i += 1
        else:
            target = a
            i += 1
    if target is None:
        die('no target')
    # The target: a script (Lnn/ID) or ROOM:REGION.
    m = re.match(r'^L(\d+)/(\d+)$', target)
    if m:
        level, script = int(m.group(1)), int(m.group(2))
        places = regions_running(level, script)
        if not places:
            die('L%02d/%d is no region\'s own script in level %02d\'s rooms (ed catalogue --script %s)' % (level, script, level, target))
        room, region, field = places[0]
        if len(places) > 1:
            print('[check] %s runs from %d regions; checking the first: %s' % (target, len(places),
                  ', '.join('%d:%s (%s)' % p for p in places[:6])))
    else:
        m = re.match(r'^(\d+):(\w+)$', target)
        if not m:
            die('TARGET is Lnn/ID or ROOM:REGION')
        room, region = int(m.group(1)), m.group(2)
        reg = next((r for r in coopplan.rooms().get(str(room), {}).get('regions', []) if r['name'] == region), None)
        if reg is None:
            die('room %d has no region %s' % (room, region))
        field = next((f for f in ('enter', 'cross_a', 'cross_b', 'stay') if reg.get(f, -1) > 0), 'enter')
        script = reg.get(field, -1)
        level = int(opts['level']) if opts['level'] else None
        if level is None:
            die('give --level for ROOM:REGION')
    ch, start_room = chapter_of(level)
    box = coopplan.region(room, region)
    if box is None:
        die('%d:%s is a plane: only the game\'s own player crosses it, it can\'t be put into' % (room, region))
    lo, hi, shape = box
    spot = coopplan.centre(lo, hi)
    spot[2] = lo[2] + 2 if shape != 0 else spot[2]
    put = ','.join(str(int(round(v))) for v in spot)
    din = door_in(level, room) if room != start_room else None
    if room != start_room and din is None:
        die('no door script into room %d in level %02d\'s tables (ed door %d)' % (room, level, room))
    dout = door_out(level, room)
    name = re.sub(r'[^A-Za-z0-9]+', '-', target).strip('-')
    work = os.path.join(OUT, name)
    os.makedirs(work, exist_ok=True)
    modes = [x for x in opts['modes'].split(',') if x]
    press_p1 = {'a': '0x0100', 'b': '0x0200'}.get(opts['press'])
    press_p2 = opts['press'] if opts['press'] in ('a', 'b') else None
    wait = int(opts['wait'])
    plan = ['# ed check %s (%s)' % (target, time.strftime('%Y-%m-%d %H:%M')),
            'script L%02d/%d, from %d:%s (%s); chapter %d (%s), start room %d' % (level, script, room, region, field, ch,
                                                                                  coopplan.CHAPTERS[ch][0], start_room),
            'put at %s; press %s; flags %s' % (put, opts['press'], flags or 'none'),
            'door in: %s' % ('none (the start room)' if din is None else 'L%s/%d (from %s) to %s' % (
                din['level'], din['script'], ', '.join('%d:%s' % (f['room'], f['region']) for f in din.get('from', [])[:2]) or '?',
                din.get('entry'))),
            'door out (apart): %s' % ('none found: apart is skipped' if dout is None else 'L%02d/%d (%s) to room %d' % (
                dout['script_level'], dout['script'], dout['region'], dout['to'][0]['to_room']))]
    # The flags the script tests before its first state step (fn30(N) ~= 1 / == 1): what it may wait on.
    path = os.path.join(disc.DB, 'scripts', 'L%02d' % level, '%d.lua' % script)
    if os.path.exists(path):
        head = open(path, encoding='utf8', errors='replace').read().split('fn1(', 1)[0]
        tested = sorted(set(int(n) for n in re.findall(r'fn30\((\d+)\)', head)))
        missing = [n for n in tested if n not in flags]
        if tested:
            plan.append('flags the script tests first (it may need each on or off; read its top lines): %s%s' % (
                ', '.join(map(str, tested)), ' (--flag N sets one on)' if missing else ''))
    print('\n'.join(plan))
    if opts['dry']:
        return
    common = dict(ED_RUN_AWAY=1, ED_ROOM_SLOTS=2, ED_ROOM_LOG=1, ED_COOP_LOG=1, ED_RUN_SHOTS_MS=250, JTG_SLOT=1)
    if opts['hero']:
        common['ED_COOP_HERO_ID'] = 1
    common.update(extra)
    h0 = hangs()
    F = probe(ch)
    S = F + 900
    flag_env = ','.join('%d:%d' % (F + 30 + k * 2, f) for k, f in enumerate(flags)) or None
    door_env = '%d:%d' % (F + 60, din['script']) if din else None
    runs = {}
    if 'solo' in modes:
        P = S
        env = dict(common, JTG_SET='mod_coop=0', ED_SET_FLAG=flag_env, ED_RUN_SCRIPT=door_env, ED_PUT='%d,1,%s' % (P, put),
                   ED_INPUT_SCRIPT='%d,%d,0,0,127' % (P + 2, P + 8) + (';%d,%d,%s' % (P + 60, P + 66, press_p1) if press_p1 else ''),
                   ED_RUN_FRAMES=P + 60 + wait)
        runs['solo'] = (run_sh('jtg.sh', [str(ch), 'check-%s-solo' % name], env, 'solo'), P)
    coop_modes = [m for m in modes if m in ('together', 'apart', 'p1')]
    state = os.path.join(ROOT, 'saves', 'check-%s.state' % name)
    if coop_modes:
        if os.path.exists(state):
            os.remove(state)
        env = dict(common, ED_SET_FLAG=flag_env, ED_RUN_SCRIPT=door_env, ED_SAVE_STATE='%d:%s' % (S, state), ED_RUN_FRAMES=S + 60)
        srun = run_sh('jtg.sh', [str(ch), 'check-%s-state' % name], env, 'state (follow mode)')
        if not os.path.exists(state) or srun is None:
            die('no state saved (%s)' % srun)
        settings = os.path.join(srun, 'settings.ini')
        for mode in coop_modes:
            if mode == 'apart' and dout is None:
                print('[check] apart skipped: no door out of room %d' % room)
                continue
            P = S + (900 if mode == 'apart' else 300)
            who = 1 if mode == 'p1' else 2
            env = dict(common, ED_SETTINGS=settings, ED_SET_FLAG=','.join('20:%d' % f for f in flags) or None,
                       ED_PUT='%d,%d,%s' % (P, who, put), ED_RUN_FRAMES=P + 60 + wait,
                       ED_RUN_SECONDS=(P + 60 + wait - S) // 20 + 120)
            if mode == 'apart':
                env.update(ED_COOP_STAY=1, ED_RUN_SCRIPT='100:%d' % dout['script'])
            if who == 2:
                env['ED_PAD2_SCRIPT'] = '%d,%d,lup' % (P + 2, P + 8) + (';%d,%d,%s' % (P + 60, P + 66, press_p2) if press_p2 else '')
            else:
                env['ED_INPUT_SCRIPT'] = '%d,%d,0,0,127' % (P + 2, P + 8) + (';%d,%d,%s' % (P + 60, P + 66, press_p1) if press_p1 else '')
            runs[mode] = (run_sh('run.sh', [state, 'check-%s-%s' % (name, mode)], env, mode), P)
    # The report.
    rep = plan + ['']
    reach = {}
    for mode, (run, P) in runs.items():
        rep.append('## %s: %s' % (mode, os.path.basename(run) if run else 'NO RUN'))
        if run is None:
            continue
        rs = script_runs(run, script, level)
        if not rs:
            rep.append('  the script never ran (not in the region? a flag it waits on? `ed catalogue --script L%02d/%d`)' % (level, script))
        for r in rs:
            whose = 'player 1' if r['who'] == 1 else 'player 2 (id %d)' % r['who']
            rep.append('  frame %d: for %s, from %s, %d calls, last at line %s%s' % (
                r['frame'], whose, r['lr'], r['calls'], r['last'], ' (40 logged: went on past)' if r['full'] else ''))
        lines = lines_of(run)
        rep.append('  log lines (%d): %s' % (len(lines), '' if lines else 'none'))
        rep.extend('    ' + l for l in lines[:40])
        if len(lines) > 40:
            rep.append("    ... %d more in the run's page logs" % (len(lines) - 40))
        reach[mode] = max([r['last'] or 0 for r in rs] + [0])
        try:
            sheet = subprocess.run([sys.executable, '-I', os.path.join('tools', 'ed', 'ed.py'), 'sheet', run] +
                                   [str(P + d) for d in (0, 60, 120, 300, 600, wait)], cwd=ROOT, capture_output=True,
                                   text=True).stdout.strip().splitlines()
            if sheet:
                rep.append('  pictures: %s' % sheet[-1])
        except OSError:
            pass
    rep.append('')
    rep.append('## Compared')
    base = reach.get('solo')
    for mode, r in reach.items():
        if mode == 'solo':
            continue
        if base is None:
            rep.append('  %s: reached line %d (no solo run to compare)' % (mode, r))
        elif r < base:
            rep.append('  %s: STOPPED SOONER: line %d against solo\'s %d' % (mode, r, base))
        else:
            rep.append('  %s: as far as solo (line %d)' % (mode, r))
    h1 = hangs()
    rep.append('  hang reports: %d before, %d after%s' % (h0, h1, '  <-- NEW' if h1 > h0 else ''))
    text = '\n'.join(rep) + '\n'
    open(os.path.join(work, 'report.md'), 'w', encoding='utf8').write(text)
    print(text)
    print('[check] report: %s' % os.path.join(work, 'report.md').replace('\\', '/'))
