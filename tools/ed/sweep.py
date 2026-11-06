"""sweep.py LEVEL SLOT ROOM [ROOM...]: the co-op sweep (python -I tools/ed/sweep.py 1 1 341 342). In each room, each examine and pickup is done start to finish by player 1,
then by player 2, with real presses, while the other walks back and forth. Then each is checked from the both-player recording:
who had the close-up and words, that the other kept moving, the cameras, and that both are back at the base line before the next.
settings.ini isn't touched: a copy (build/sweep/sweep.ini) has co-op on, the save's characters (SWEEP_P1, SWEEP_P2: the Mods
choices, 1 and 10 by default, as the saves have them), full stats (ED_CHEAT=2) and the original camera on both.
SWEEP_FIRST=2: player 2 does each first, as a pickup can only be taken once.
Output: build/sweep/LLEVEL-rROOM.txt and a sheet a step in Screenshots for Claude/sweep/.
Placing: ED_PUT puts each on the floor under the spot (the game's collision), a step in so player 1's zones look. Rooms of
many floors (183) aren't placed right yet: say so, don't take them for the game."""
import json, os, shutil, subprocess, sys

sys.path.insert(0, 'tools/ed')
import chapters, recording  # noqa: E402

ROOT = os.getcwd()
level, slot, rooms = int(sys.argv[1]), int(sys.argv[2]), [int(x) for x in sys.argv[3:]]
plan = json.loads(subprocess.run([sys.executable, '-I', 'tools/ed/sweep_list.py', str(level)], capture_output=True, text=True).stdout)
OUT = os.path.join('build', 'sweep')
PICS = os.path.join('Screenshots for Claude', 'sweep')
os.makedirs(OUT, exist_ok=True)
os.makedirs(PICS, exist_ok=True)
INI = os.path.join(ROOT, OUT, 'sweep.ini')
_text = open('settings.ini', encoding='utf-8', errors='replace').read()
for _k, _v in (('mod_coop', 1), ('mod_p1_character', os.environ.get('SWEEP_P1', '1')), ('mod_p2_character', os.environ.get('SWEEP_P2', '10')),
               ('mod_p1_costume', 0), ('mod_p2_costume', 0), ('mod_p1_stats', 0), ('mod_p2_stats', 0), ('camera', 0), ('mod_p2_camera', 2)):
    import re
    _text = re.sub(r'(?m)^%s = .*$' % _k, '%s = %s' % (_k, _v), _text)
open(INI, 'w', encoding='utf-8').write(_text)

START_ROOM = {1: 341, 2: 166, 3: 150, 4: 338, 5: 249, 6: 303, 7: 103, 8: 82, 9: 237, 10: 27, 11: 50, 12: 100}
ROOMS = json.load(open('build/ed-db/rooms.json'))
ARRIVE = 900     # a door's press, the walk through it, its arrival and the room settling
STEP = int(os.environ.get('SWEEP_STEP', '900'))  # one player's interaction, start to finish, and the settle after
B1, A1 = '0x0200,0,0,0', '0x0100,0,0,0'


def run_room(room):
    start = chapters.first_frame(slot)
    info = plan['rooms'][str(room)]
    p1, p2, puts, scripts, steps = [], [], [], [], []
    at = 120
    entries = [m['pos'] for m in ROOMS[str(room)]['markers'] if m['name'].startswith('EntryMkr')]
    if room != START_ROOM.get(level):
        # As the chapter tour takes a door: a door that asks (OPEN DOOR) runs its script again as the press, then the walk
        # through it and the arrival.
        scripts.append('%d:%d' % (at, info['door']))
        scripts.append('%d:%d' % (at + chapters.ASK_GAP, info['door']))
        at += ARRIVE
    for it in info['interactions']:
        centre = list(it['centre'])
        # The other player stands by the room's nearest entry marker at least 300 from the spot.
        far = [e for e in entries if ((e[0] - centre[0]) ** 2 + (e[1] - centre[1]) ** 2) ** 0.5 >= 300] or entries or [[centre[0] + 260, centre[1], centre[2]]]
        aside = min(far, key=lambda e: (e[0] - centre[0]) ** 2 + (e[1] - centre[1]) ** 2)
        # The actor inside the spot, 40 in from its edge nearest that marker (a spot's middle may be a hole, the pit's), on its floor.
        c = [min(max(aside[k], it['min'][k] + 40), it['max'][k] - 40) if it['max'][k] - it['min'][k] > 80 else centre[k] for k in (0, 1)]
        # On the floor: a sphere's height is its middle, which can be under the floor (room 183): never below the room's entry
        # marker by it, where a player stands.
        c.append(max(it['min'][2], aside[2]) + 5)
        for who in ((2, 1) if os.environ.get('SWEEP_FIRST') == '2' else (1, 2)):  # SWEEP_FIRST=2: player 2 takes things first
            other = 3 - who
            f = start + at
            # Both put apart, the actor on the spot (it asks for its prompt as they come into it), the other a way off.
            puts.append('%d,%d,%.0f,%.0f,%.0f' % (f, who, c[0], c[1], c[2]))
            puts.append('%d,%d,%.0f,%.0f,%.0f' % (f, other, aside[0], aside[1], aside[2]))
            # B presses, by what the examine is (the kit's verbs): ed219 close-up, DETAIL, words, RETURN; ed220 words alone; ed221
            # close-up, DETAIL, words, PICK UP, then the find's words.
            count = {'ed219': 4, 'ed220': 2, 'ed221': 5}.get(it['native'], 4)
            count += int(os.environ.get('SWEEP_MORE', '0'))  # SWEEP_MORE=N: N more presses, where enemies interrupt them
            presses = [60 + 130 * k for k in range(count)]
            # A step in place first: player 1's zones are looked at only as they move (player 2's each frame, by the host).
            if who == 1:
                p1.append('%d,%d,0x0000,0,40,0' % (f + 20, f + 32))
                p1.append('%d,%d,0x0000,0,-40,0' % (f + 34, f + 46))
            else:
                p2.append('%d,%d,lup' % (f + 20, f + 28))
                p2.append('%d,%d,ldown' % (f + 30, f + 38))
            for k, d in enumerate(presses):
                if who == 1:
                    p1.append('%d,%d,%s' % (f + d, f + d + 5, B1))
                else:
                    p2.append('%d,%d,b' % (f + d, f + d + 5))
            # The other walks in bursts the whole time, round the four ways so a wall stops only one.
            for k in range(6):
                g = f + 40 + k * 110
                if other == 1:
                    x, y = ((0, 100), (100, 0), (0, -100), (-100, 0))[k % 4]
                    p1.append('%d,%d,0x0000,%d,%d,0' % (g, g + 50, x, y))
                else:
                    p2.append('%d,%d,%s' % (g, g + 50, ('lup', 'lright', 'ldown', 'lleft')[k % 4]))
            # Off the spot once done, so a press after can't open it again.
            puts.append('%d,%d,%.0f,%.0f,%.0f' % (f + presses[-1] + 120, who, aside[0] + 60, aside[1] + 60, aside[2]))
            steps.append(dict(region=it['region'], kind=it['kind'], script=it['script'], who=who, start=f, end=f + STEP))
            at += STEP
    p1.sort(key=lambda s: int(s.split(',')[0]))
    p2.sort(key=lambda s: int(s.split(',')[0]))
    puts.sort(key=lambda s: int(s.split(',')[0]))
    env = dict(os.environ, ED_SETTINGS=INI, ED_CHEAT='2', ED_INPUT_SCRIPT=';'.join(p1),
               ED_PAD2_SCRIPT=';'.join(p2), ED_PUT=';'.join(puts), ED_RUN_SCRIPT=','.join(scripts), ED_RUN_FRAMES=str(start + at + 60),
               ED_RUN_SECONDS=str(at // 50 + 60), ED_RUN_SHOTS_MS='500', ED_RUN_SHOTS_W='1280')
    if not scripts:
        env.pop('ED_RUN_SCRIPT')
    sh = shutil.which('sh') or r'C:\Program Files\Git\bin\sh.exe'
    out = subprocess.run([sh, 'tools/ed/run.sh', 'saves/slot%d.state' % slot, 'sweep-L%02d-r%d%s' % (level, room, '-p2first' if os.environ.get('SWEEP_FIRST') == '2' else '')], env=env, capture_output=True, text=True, errors='replace').stdout
    run = out.split('\n')[0].strip()
    return run, steps


def judge(run, steps, room):
    rows = recording.load(os.path.join(ROOT, run))
    log = ''
    for p in sorted(os.listdir(run)):
        if p.startswith('page-'):
            log += open(os.path.join(run, p), encoding='utf-8', errors='replace').read()
    lines = []
    findings = recording.check(rows)
    for s in steps:
        win = [r for r in rows if s['start'] <= r['frame'] < s['end']]
        a, o = 'p%d' % s['who'], 'p%d' % (3 - s['who'])
        if not win:
            lines.append('%s by %s: NO FRAMES' % (s['region'], a.upper()))
            continue
        own = [r for r in win if recording.activity(r, a) == 'close-up']
        other_up = [r for r in win if recording.activity(r, o) not in (None,)]
        # How far the other walked while the actor's action was up (the recording's stick is the game's pad, not theirs).
        moved = 0.0
        for x, y in zip(win, win[1:]):
            d = recording.dist(recording.pos(x, o), recording.pos(y, o))
            if s['start'] + 50 <= x['frame'] <= s['start'] + 600 and d < 40:
                moved += d
        end = [r for r in win if r['frame'] >= s['end'] - 60]
        base = all(recording.activity(r, 'p1') is None and recording.activity(r, 'p2') is None and not r['menu'] and not r['window'] for r in end)
        words = [l for l in log.split('\n') if '[trace] words:' in l]
        f = [x for x in findings if s['start'] <= x.get('from', 0) < s['end']]
        verdict = []
        if not own:
            verdict.append('the actor never had a close-up')
        if other_up:
            verdict.append('the other player had %s for %d frames' % (recording.activity(other_up[0], o), len(other_up)))
        if moved < 60:
            verdict.append('the other player moved only %.0f' % moved)
        if not base:
            verdict.append('not back at the base line at the end')
        puts = [int(l.split('(frame ')[1].rstrip(')')) for l in log.splitlines() if '[sweep] player' in l and '(frame ' in l]
        f = [x for x in f if not (x['rule'] in ('SENT', 'CAMERA') and any(-3 <= x['from'] - q <= (180 if x['rule'] == 'CAMERA' else 3) or abs(x['to'] - q) <= 3 for q in puts))]
        for x in f:
            verdict.append('%s: %s' % (x['rule'], x['text'][:120]))
        lines.append('%-22s %-7s by %s  close-up %3d frames, other moved %5.0f  %s' % (s['region'], s['kind'], a.upper(), len(own), moved,
                                                                                       'OK' if not verdict else 'PROBLEM: ' + '; '.join(verdict)))
        picks = [s['start'] + 30, s['start'] + 260, s['start'] + 420, s['start'] + 640, s['end'] - 20]
        made = subprocess.run([sys.executable, '-I', 'tools/ed/ed.py', 'sheet', os.path.basename(run)] + [str(x) for x in picks],
                              capture_output=True, text=True, errors='replace').stdout.strip().split('\n')[-1].strip()
        if os.path.exists(made):
            shutil.copy(made, os.path.join(PICS, 'L%02d-r%d-%s-%s%s.jpg' % (level, room, s['region'], a, '-p2first' if os.environ.get('SWEEP_FIRST') == '2' else '')))
    return lines


for room in rooms:
    run, steps = run_room(room)
    lines = ['room %d  run %s' % (room, os.path.basename(run))] + judge(run, steps, room)
    open(os.path.join(OUT, 'L%02d-r%d%s.txt' % (level, room, '-p2first' if os.environ.get('SWEEP_FIRST') == '2' else '')), 'w').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))
