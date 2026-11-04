"""`ed chapters`: every room of a chapter, from the save at the chapter's start (saves/slot1 to slot12), through the game's own door
scripts, as a brief check of each room. A room that shows errors is run again for longer.

  ed chapters N [--name NAME]          chapter by save slot, 1 to 12: every room its scripts have a door to
  ed chapters N --room R [--long]      one room
  ed chapters --compare NAME_A NAME_B  each chapter's sheets, one above the other

What's checked in a room, from the kit's own tables and the run's logs:
  rooms     the door's script took both players there (doors_table)
  start     each player arrived near the door's entry marker (rooms.json), and together
  camera    what `ed check` says about both cameras, and the door's own camera against the one the game then shows
  shadows   a shadow is cast from where each player really stands, every frame looked at: the renderer's [shadow] lines against both.tsv
  others    how many others cast (enemies, people, bodies)
  lamps     the renderer's lamps for the room against the room's light grid on the disc (lights.py)
  flicker   one-frame jumps in either player's picture, from inside the renderer (ED_RUN_PROBE)
  errors    WebGPU errors, freezes and crashes in the run
  Tomes     both opened and closed in the chapter's first room, then both walk

Door scripts run from the host: `ED_RUN_SCRIPT` isn't a real zone or press, and a door's flags aren't set as play would set them.
A door that doesn't work this way is logged for the user to look at, as DOOR, not taken for a fault in the game. Only the pictures
say whether a room looks right: the sheet is there to be read, and the user's eyes outrank all of it.
"""
import bisect
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import time

import gfxcheck
import lights
import recording

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, 'Screenshots for Claude', 'chapters')

# Save slot: name, level. Slots 9 and 10 are the way round Jump to Game has them, and 12 is Alex's end (level 12's files).
SLOTS = {1: ('Pius', 1), 2: ('Ellia', 2), 3: ('Anthony', 3), 4: ('Karim', 4), 5: ('Max', 5), 6: ('Lindsey', 6), 7: ('Paul', 7), 8: ('Roberto', 8),
         9: ('Peter', 10), 10: ('Edward', 9), 11: ('Michael', 11), 12: ('Alex', 12)}

ROOM_FRAMES = 420      # a room's brief check, arrival to the next door
LONG_FRAMES = 1500     # and one that showed errors
SETTLE = 170           # a door's arrival: 62 ticks of fade, then in control about 2 s on
ASK_GAP = 60           # a prompted door: its script runs twice, as walking in and as pressing
TOMES = 800            # the first room, frames for both Tomes before the first door


def rooms_of_level(level):
    """Each room the level's scripts have a door to, and the doors to try, the plainest first."""
    table = json.load(open(os.path.join(ROOT, 'build', 'ed-db', 'facts', 'doors_table.json')))
    rooms = {}
    for d in table:
        if d['level'] != '%02d' % level or not isinstance(d['to_room'], int) or d['source'] == 'item_record' or d.get('entry_in_to_room') is False:
            continue
        rooms.setdefault(d['to_room'], []).append(d)
    # A door whose script opens words waits on an answer: run from the host they stay up, over every room after it. Those last.
    def has_words(d):
        path = os.path.join(ROOT, 'build', 'ed-db', 'scripts', 'L%02d' % level, '%d.lua' % d['script'])
        return os.path.exists(path) and WORDS.search(open(path, encoding='utf8', errors='replace').read()) is not None
    for doors in rooms.values():
        doors.sort(key=lambda d: (1 if has_words(d) else 0, len(d.get('asks_in_script') or []), 1 if d.get('after_script') else 0, d['script']))
    return rooms


def first_frame(slot):
    """The frame a save starts on: ED_INPUT_SCRIPT counts in the game's own frames."""
    cache = os.path.join(OUT, 'frames.json')
    known = json.load(open(cache)) if os.path.exists(cache) else {}
    stamp = '%d:%d' % (slot, os.path.getmtime(os.path.join(ROOT, 'saves', 'slot%d.state' % slot)))
    if stamp in known:
        return known[stamp]
    env = dict(os.environ, ED_RUN_FRAMES='999999999', ED_RUN_SECONDS='6', ED_RUN_SHOTS_MS='5000')
    sh = shutil.which('sh') or r'C:\Program Files\Git\bin\sh.exe'
    out = subprocess.run([sh, 'tools/ed/run.sh', 'saves/slot%d.state' % slot, 'frame-of-slot%d' % slot], cwd=ROOT, env=env, capture_output=True, text=True, errors='replace').stdout
    rows = recording.load(os.path.join(ROOT, out.split('\n')[0].strip()))
    known[stamp] = int(rows[0]['frame'])
    os.makedirs(OUT, exist_ok=True)
    json.dump(known, open(cache, 'w'))
    return known[stamp]


def plan(slot, rooms, frames, tomes=True):
    """The run: both Tomes in the first room, then each room's door in turn, with both players stepping about in it."""
    at = 60
    start = first_frame(slot)
    p1, p2, scripts, visits = [], [], [], []

    def press1(offset, length, text):
        p1.append('%d,%d,%s' % (start + offset, start + offset + length, text))

    def press2(offset, length, what):
        p2.append('%d,%d,%s' % (start + offset, start + offset + length, what))

    if tomes:
        press1(200, 4, '0x1000,0,0,0')
        press2(320, 4, 'start')
        press2(420, 4, 'r')
        press2(520, 4, 'start')
        press1(620, 4, '0x1000,0,0,0')
        press1(680, 60, '0x0000,100,0,0')
        press2(680, 60, 'lleft')
        at = TOMES
    for room, door in rooms:
        scripts.append('%d:%d' % (at, door['script']))
        if door.get('asks_in_script'):
            scripts.append('%d:%d' % (at + ASK_GAP, door['script']))
        visits.append(dict(room=room, door=door, offset=at))
        # A step each way, a beat after they arrive, so a shadow has to go with them. A long check walks them about more.
        steps = 1 if frames <= ROOM_FRAMES else max(1, frames // 300)
        # The game's help the first time something's seen waits for A on the Tome, and hides that half from then on.
        for tap in range(3):
            press1(ASK_GAP + at + SETTLE - 30 + tap * 12, 4, '0x0100,0,0,0')
        for s in range(steps):
            f = at + ASK_GAP + SETTLE + 40 + s * 280
            x, y = ((0, 127), (127, 0), (0, -127), (-127, 0))[s % 4]
            press1(f, 50, '0x0000,%d,%d,0' % (x, y))
            press2(f, 50, ('lleft', 'ldown', 'lright', 'lup')[s % 4])
        at += frames
    return start, at, p1, p2, scripts, visits


def execute(slot, label, last, seconds, p1, p2, scripts):
    env = dict(os.environ)
    env.update(ED_INPUT_SCRIPT=';'.join(p1), ED_PAD2_SCRIPT=';'.join(p2), ED_RUN_SCRIPT=','.join(scripts), ED_RUN_PROBE='1', AURORA_SHADOW_LOG='2',
               ED_RUN_SHOTS_W='1920', ED_RUN_SHOTS_MS='500', ED_RUN_FRAMES=str(last), ED_RUN_SECONDS=str(seconds))
    sh = shutil.which('sh') or r'C:\Program Files\Git\bin\sh.exe'
    out = subprocess.run([sh, 'tools/ed/run.sh', 'saves/slot%d.state' % slot, label], cwd=ROOT, env=env, capture_output=True, text=True, errors='replace').stdout
    return os.path.join(ROOT, out.split('\n')[0].strip())


WORDS = re.compile(r'(?:^|[^a-z0-9_])(ed11|ed12|ed97)[(]', re.M)  # the script opens words: boot, room or a person's
SHADOW = re.compile(r'\[shadow\] frame (\d+) caster (\d+) of (\d+) stands (-?\d+) (-?\d+) (-?\d+) radius (\d+) sets (\d+) nearest lamps (-?\d+) (-?\d+)')
LAMP = re.compile(r'\[shadow-lamp\] (\d+) at (-?\d+) (-?\d+) (-?\d+) colour (\d+) (\d+) (\d+) power (\d+)')
GRID = re.compile(r'\[room-lights\] grid (0x[0-9A-F]+): (\d+) lamps')
SYNC = re.compile(r'\[sync\] game frame (\d+)')


def read_log(run):
    """What the renderer said, in order: its casters by frame, and the lamps sent for each room in turn."""
    casters, lamp_lists, errors, pairs = {}, [], [], []
    game = None
    for path in sorted(glob.glob(os.path.join(run, 'page-*.log'))):
        for line in open(path, errors='replace'):
            m = SYNC.search(line)
            if m:
                game = int(m.group(1))
                continue
            m = SHADOW.search(line)
            if m:
                # By the game's frame: the renderer's count runs on its own, and drifts with a Tome up or a room loading.
                if game is not None:
                    # The window's last frame only: that's one look at who's there, not everyone again and again.
                    window = casters.setdefault(game, {})
                    window.setdefault(int(m.group(1)), []).append((int(m.group(4)), int(m.group(5)), int(m.group(9)), int(m.group(10))))
                    if not pairs or pairs[-1][1] != game:
                        pairs.append((int(m.group(1)), game))
                continue
            if GRID.search(line):
                lamp_lists.append([])
                continue
            m = LAMP.search(line)
            if m and lamp_lists:
                lamp_lists[-1].append(tuple(int(x) for x in m.groups()[1:4]) + (int(m.group(8)),))
            if re.search(r'WebGPU error|\[wgpu\].*error|\[hang\]|exception  *: (?!none)', line):
                errors.append(line.strip()[:160])
    return casters, lamp_lists, errors, pairs


def check_room(run, rows, room, door, offset, frames, start, casters, spikes, rule_findings, known_lamps, pairs):
    """Findings for one visit, and the frame to look at."""
    out = []
    within = [r for r in rows if start + offset - 5 <= r['frame'] < start + offset + frames]
    here = [r for r in within if r['room'] == room]
    if not here:
        seen = sorted({r['room'] for r in within})
        out.append('DOOR script %d to room %d didn\'t get there: rooms seen in its time %s' % (door['script'], room, seen))
        return out, None
    arrive = here[0]['frame']
    settled = [r for r in here if r['frame'] >= arrive + SETTLE]
    if not settled:
        out.append('DOOR script %d: room %d left again within %d frames' % (door['script'], room, len(here)))
        return out, here[-1]['frame']
    # A room's first scene plays on arrival, with the players put aside and its own cameras: what's judged is what comes after,
    # and the second half's camera during it goes down for the user (docs/coop-todo.md has one such).
    free = [r for r in settled if recording.activity(r, 'p1') not in ('close-up', 'Tome', 'cutscene') and recording.activity(r, 'p2') not in ('close-up', 'Tome', 'cutscene')]
    if len(free) < len(settled):
        cameras = recording.room_cameras()
        during = [r for r in settled if r not in free][0]
        out.append('SCENE %d frames held on arrival, P1 on %s and P2 on %s' % (len(settled) - len(free), recording.camera_name(cameras, during, 'p1'), recording.camera_name(cameras, during, 'p2')))
    if not free:
        return out, dict(look=settled[len(settled) // 2]['frame'], others=0, lamps=known_lamps)
    settled = free
    first = settled[0]

    # Where they began.
    entry = None
    rooms_json = json.load(open(os.path.join(ROOT, 'build', 'ed-db', 'rooms.json')))
    for m in rooms_json.get(str(room), {}).get('markers', []):
        if m['name'] == door.get('entry'):
            entry = m['pos']
    if entry is not None:
        for who in ('p1', 'p2'):
            d = recording.dist(recording.pos(first, who), entry)
            if d > 500:
                out.append('START %s arrived %.0f from %s at %s, at (%.0f,%.0f,%.0f)' % (who, d, door.get('entry'), [round(x) for x in entry], *recording.pos(first, who)))
    apart = recording.dist(recording.pos(first, 'p1'), recording.pos(first, 'p2'))
    if apart > 500:
        out.append('START the players arrived %.0f apart' % apart)

    # The camera the door names, against the one each half is on as they arrive.
    named = door.get('camera')
    if named and door.get('camera_in_to_room') is not False:
        cameras = recording.room_cameras()
        on = [recording.camera_name(cameras, first, who) for who in ('p1', 'p2')]
        if named not in on:
            out.append('CAMERA the door names %s, and they arrive on %s and %s' % (named, on[0], on[1]))

    # A shadow from where each one is.
    looked = covered1 = covered2 = 0
    others = {}
    churn = {}
    for r in settled:
        if r['frame'] % 20 >= 2:  # the [sync] lines, each 20 frames; what's logged after one is that frame's, or the next's
            continue
        window = casters.get(r['frame'] - r['frame'] % 20)
        if not window:
            continue
        cast = window[max(window)]
        looked += 1
        near = lambda who: any(((c[0] - r[who + 'x']) ** 2 + (c[1] - r[who + 'y']) ** 2) < 130 ** 2 for c in cast)
        covered1 += near('p1')
        covered2 += near('p2')
        n = sum(1 for c in cast if ((c[0] - r['p1x']) ** 2 + (c[1] - r['p1y']) ** 2) >= 130 ** 2 and ((c[0] - r['p2x']) ** 2 + (c[1] - r['p2y']) ** 2) >= 130 ** 2)
        others[n] = others.get(n, 0) + 1
    if looked == 0:
        out.append('SHADOW no shadow lines at all in the room')
    else:
        if covered1 < looked * 0.95:
            out.append('SHADOW player 1 cast from where they stand in only %d of %d frames looked at' % (covered1, looked))
        if covered2 < looked * 0.95:
            out.append('SHADOW player 2 cast from where they stand in only %d of %d frames looked at' % (covered2, looked))
    busiest = max(others, key=others.get) if others else 0

    # Flicker, once settled.
    for view, frame, kind, d1, d2 in spikes:
        game = to_game(pairs, frame)
        if arrive + SETTLE <= game < here[-1]['frame'] - 30 and d1 > 0.25 and d2 > 0.25:
            out.append('FLICKER player %d frame %d, %.2f then %.2f' % (view + 1, game, d1, d2))

    # And what `ed check` found.
    for frame, text in rule_findings:
        if arrive + SETTLE <= frame < start + offset + frames:
            out.append('RULE ' + text)
    return out, dict(look=settled[len(settled) // 2]['frame'], others=busiest, lamps=known_lamps)


def to_game(pairs, frame):
    """A renderer's frame as the game's, from the pairs read in the log."""
    i = bisect.bisect_right(pairs, (frame, 10 ** 12)) - 1
    if i < 0:
        return -1
    return pairs[i][1] + frame - pairs[i][0]


def run_rules(run, rows):
    """`ed check`'s findings, with the frames they name. STUCK? may just be a wall, as its own words say, and the players are put down anywhere here."""
    found = []
    for f in list(recording.check(rows)) + list(recording.check_cameras(rows)):
        if f.get('rule') != 'STUCK?':
            found.append((f.get('from', -1), '%s %s' % (f.get('rule'), f.get('text'))))
    return found


def tour(slot, name, only=None, long=False, after=None, part=1):
    title, level = SLOTS[slot]
    rooms = rooms_of_level(level)
    if only is not None:
        rooms = {only: rooms.get(only) or []}
        if not rooms[only]:
            raise SystemExit('no door to room %d in level %d' % (only, level))
    order = [(room, doors[0]) for room, doors in sorted(rooms.items())]
    # Rooms whose every door opens words aren't gone to with the others: they'd leave them up. They're named for the user.
    worded = [room for room, door in order if os.path.exists(os.path.join(ROOT, 'build', 'ed-db', 'scripts', 'L%02d' % level, '%d.lua' % door['script'])) and WORDS.search(open(os.path.join(ROOT, 'build', 'ed-db', 'scripts', 'L%02d' % level, '%d.lua' % door['script']), encoding='utf8', errors='replace').read()) is not None]
    if only is None:
        order = [o for o in order if o[0] not in worded]
    # After a freeze, the rooms it left unseen, from the start again.
    if after is not None:
        order = [o for o in order if o[0] in after]
    tail = '%s%s' % ('-room%d' % only if only else '', '-part%d' % part if part > 1 else '')
    frames = LONG_FRAMES if long else ROOM_FRAMES
    start, last, p1, p2, scripts, visits = plan(slot, order, frames, tomes=only is None and after is None)
    label = '%s-ch%02d-%s%s' % (name, slot, title, tail)
    # Doors can run long: a scene on arrival holds the next one back.
    began = time.time()
    run = execute(slot, label, start + last, 90 + last // 20, p1, p2, scripts)
    rows = recording.load(run)
    casters, lamp_lists, errors, pairs = read_log(run)
    probe = os.path.join(run, 'probe.txt')
    spikes = []
    if os.path.exists(probe):
        s = gfxcheck.summary(probe, 0.1)
        for view, v in s['views'].items():
            for frame, kind, d1, d2 in v['col']['spikes']:
                spikes.append((view, frame, kind, d1, d2))
    rule_findings = run_rules(run, rows)

    report = ['Chapter slot %d, %s: level %d, %d rooms with a door. %s' % (slot, title, level, len(order), os.path.relpath(run, ROOT)),
              'Frames %d to %d. Rooms seen in order: %s' % (rows[0]['frame'], rows[-1]['frame'], seen_rooms(rows))]
    log = '\n'.join(open(p, errors='replace').read() for p in sorted(glob.glob(os.path.join(run, 'page-*.log'))))
    if only is None and worded and after is None:
        report.append('DOOR not gone to, as each door to it opens words that wait for an answer: rooms %s' % worded)
    if only is None and after is None:
        tome = "player 2's Tome opens in the copy, beside player 1's" in log
        report.append('Tomes: %s' % ('both opened, side by side' if tome else 'TOMES the log never says both were open'))
    if errors:
        report.append('ERRORS %d, first: %s' % (len(errors), errors[0]))
    hangs = [p for p in glob.glob(os.path.join(ROOT, 'saves', 'reports', 'hang-*')) if os.path.getmtime(p) > began]
    # Where it stopped: what's left gets a run of its own, so one bad door doesn't cost the chapter.
    unseen = []
    if hangs:
        report.append('FREEZE %s, at frame %d, room %d' % (', '.join(os.path.basename(p) for p in hangs), rows[-1]['frame'], rows[-1]['room']))
        if only is None:
            unseen = [v['room'] for v in visits if start + v['offset'] > rows[-1]['frame']]
            visits = [v for v in visits if v['room'] not in unseen]
            report.append('Left for the next part: %s' % unseen)

    looks, bad = ([start + 480] if only is None and after is None else []), []
    for v in visits:
        found, info = check_room(run, rows, v['room'], v['door'], v['offset'], frames, start, casters, spikes, rule_findings, None, pairs)
        extra = ''
        if isinstance(info, dict):
            looks.append(info['look'])
            extra = ', others casting %d' % info['others']
        elif info:
            looks.append(info)
        line = '  room %-4d door script %-5d %s%s' % (v['room'], v['door']['script'], 'ok' if not found else '%d finding(s)' % len(found), extra)
        report.append(line)
        report += ['      ' + f for f in found[:8]]
        # A door that doesn't go isn't the room's fault: it's logged, for the user, and there's nothing there to look at for longer.
        if any(not f.startswith(('DOOR', 'SCENE', 'CAMERA')) for f in found):
            bad.append(v['room'])

    # The lamps sent against the disc.
    report += lights.compare(lamp_lists)

    dest = os.path.join(OUT, name)
    os.makedirs(dest, exist_ok=True)
    sheets = []
    for i in range(0, len(looks), 12):
        chunk = looks[i:i + 12]
        if len(chunk) == 1:
            chunk = chunk + [chunk[0] + 120]  # the sheet wants two
        got = subprocess.run([sys.executable, '-I', os.path.join(ROOT, 'tools', 'ed', 'ed.py'), 'sheet', run] + [str(f) for f in chunk], cwd=ROOT, capture_output=True, text=True, errors='replace').stdout.strip().split('\n')[-1]
        if not os.path.exists(got):
            report.append('No sheet for frames %s: %s' % (chunk, got))
        else:
            to = os.path.join(dest, '%02d-%s%s-%d.jpg' % (slot, title, tail, i // 12 + 1))
            shutil.copy(got, to)
            sheets.append(os.path.relpath(to, ROOT))
    report.append('Sheets: ' + ', '.join(sheets))
    text = '\n'.join(report)
    print(text)
    sys.stdout.flush()
    with open(os.path.join(dest, '%02d-%s%s.txt' % (slot, title, tail)), 'w', encoding='utf8') as f:
        f.write(text + '\n')
    if unseen and part < 6:
        print('\nFroze: going on with the %d rooms left' % len(unseen))
        sys.stdout.flush()
        bad += tour(slot, name, None, False, unseen, part + 1)
    return bad


def seen_rooms(rows):
    out = []
    for r in rows:
        if r['room'] > 0 and (not out or out[-1] != r['room']):
            out.append(r['room'])
    return out


def compare(a, b):
    dest = os.path.join(OUT, '%s-vs-%s' % (a, b))
    os.makedirs(dest, exist_ok=True)
    for path in sorted(glob.glob(os.path.join(OUT, a, '*.jpg'))):
        other = os.path.join(OUT, b, os.path.basename(path))
        if os.path.exists(other):
            to = os.path.join(dest, os.path.basename(path))
            subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', path, '-i', other, '-filter_complex', 'vstack', '-frames:v', '1', to])
            print(to)


def main(argv):
    if '--compare' in argv:
        i = argv.index('--compare')
        return compare(argv[i + 1], argv[i + 2])
    name = time.strftime('%H%M')
    if '--name' in argv:
        i = argv.index('--name')
        name = argv[i + 1]
        del argv[i:i + 2]
    only = None
    if '--room' in argv:
        i = argv.index('--room')
        only = int(argv[i + 1])
        del argv[i:i + 2]
    long = '--long' in argv
    if long:
        argv.remove('--long')
    if not argv or not argv[0].isdigit() or int(argv[0]) not in SLOTS:
        raise SystemExit('ed chapters N [--name NAME] [--room R] [--long]. N is a save slot: ' + ', '.join('%d %s' % (k, v[0]) for k, v in SLOTS.items()))
    slot = int(argv[0])
    bad = tour(slot, name, only, long)
    # The brief check found something: each such room again, for longer.
    if only is None and bad:
        print('\nExtended check of %d room(s) with findings: %s' % (len(bad), bad))
        sys.stdout.flush()
        for room in bad:
            tour(slot, name, room, True)
