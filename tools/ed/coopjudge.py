"""coopjudge: the game's rules for two players, checked on one run's both.tsv and logs. Used by coopcheck.py.

Each plan step (coopplan.py) is judged in its own window of frames, shifted to the run's own base (the frame both players
were first in control), and the whole run is checked for what must never happen whatever the step. Every check reads
only the recording and the logs, and the kit's tables for the rooms: nothing is per room, chapter or character.

Statuses: PASS; FAIL; SKIP (the test couldn't do it: no floor at a spot, no one to fight); BLOCKED (an earlier step left
the players somewhere else, so this one can't be judged).

Columns this reads that both.tsv has today: see recording.py. Columns it uses when the recorder has them (the additions
coopcheck.py --recording-patch prints): p1room p2room (each player's own room), tkroom tkentry tkby (the ticket: room,
entry, and who was current as it was written), p1prompt p2prompt, v2ex..v2tz v2on (player 2's half as drawn), curp
mainp use (who the game's current and main pairs name, and whose use is running), p1sb p1ssx p1ssy (player 1's
scripted pad, which p1b/p1sx/p1sy miss). Without them those checks say so.
"""
import bisect
import glob
import math
import os
import re

import coopplan
import recording as R

STICK_MIN_WALK = 120      # a walk step: each player's path at least this
OTHER_MIN_WALK = 60       # the other player keeps walking through someone's action
VIEW_DEG = 50.0           # own player inside this cone of their own half's camera (runs: median 9-17, 99% under 55)
LOST_FRAMES = 60          # out of their own view this long, free, is a finding
ARRIVE_ENTRY = 200        # the door's taker lands within this of the door's entry marker
FOLLOW_NEAR = 400         # the follower lands within this of the taker (coop_place: 150, 75, or their own spot)
SAME_FLOOR = 150          # and this high
FELL_DROP = 250           # a drop of more than this after arrival is a fall
CAMERA_NEAR = 60          # an eye this close to a room camera (or its path) is on it (recording.check_cameras)
SETTLED = 0.8             # fraction of a window a camera rule must hold for

# ---- the log, with frames ---------------------------------------------------------------------------------------

FRAME = re.compile(r'(?:\(frame (\d+)\)|frame[= ](\d+)|at frame (\d+))')


def log_lines(run):
    """Every log line with the game frame it was written at: the last frame any line before it named. Lines name frames
    often ([input] script frame=, [coop] ... (frame N), [trace] ... frame N)."""
    out, frame = [], 0
    for path in sorted(glob.glob(os.path.join(run, 'page-*.log'))):
        for line in open(path, encoding='utf8', errors='replace'):
            m = FRAME.search(line)
            if m:
                frame = int(next(g for g in m.groups() if g))
            out.append((frame, line.rstrip('\n')))
    return out


def lines_in(log, a, b, pattern):
    rx = re.compile(pattern)
    return [(f, l, m) for f, l in log if a <= f <= b for m in [rx.search(l)] if m]


# ---- the recording ----------------------------------------------------------------------------------------------

def playable(rows, solo=False, hold=30):
    """The first frame both players are in control, held for `hold` frames: in a room with nothing busy, as
    tools/ed/jtg_report.py has it (no scene, Tome, close-up hold, words or fade: a chapter's opening fly-over is player 1's
    "close-up", busy 0x22, with them in state 1), and both standing (state 1)."""
    run = 0
    for i, r in enumerate(rows):
        ok = (r['room'] >= 0 and r['p1st'] == 1 and not r['cutscene'] and not r['menu'] and not r['busy'] and not r['hold']
              and not r['fade'] and not r['window'] and (solo or (r['p2m'] >= 0 and r['p2st'] == 1)))
        run = run + 1 if ok else 0
        if run >= hold:
            return rows[i - hold + 1]
    return None


def window(rows, a, b):
    frames = [r['frame'] for r in rows]
    return rows[bisect.bisect_left(frames, a):bisect.bisect_right(frames, b)]


def at(rows, frame):
    frames = [r['frame'] for r in rows]
    i = min(bisect.bisect_left(frames, frame), len(rows) - 1)
    return rows[i] if rows else None


def walked(rows, p):
    """Path length, leaving out puts and room changes (a step over 40 in a frame)."""
    total = 0.0
    for a, b in zip(rows, rows[1:]):
        if b['frame'] - a['frame'] < 4 and a['room'] == b['room']:
            d = R.dist(R.pos(a, p), R.pos(b, p))
            if d < 40:
                total += d
    return total


def view(r, h):
    """(eye, target) of each half as drawn. Player 2's half: the drawn view when the recorder has it (v2*), else their own
    Original camera (p2e*, p2t*), else the game's: camera.c draws player 2's half from the game's camera until their own
    has stepped (camera_split_update, `own ? ... : eye`), and the recorder writes 0 for theirs then."""
    if h == 'p1':
        if r['lent'] and (r['p1ex'] or r['p1ey']):
            return (r['p1ex'], r['p1ey'], r['p1ez']), (r['p1tx'], r['p1ty'], r['p1tz'])
        return (r['ex'], r['ey'], r['ez']), (r['tx'], r['ty'], r['tz'])
    if r.get('v2on'):
        return (r['v2ex'], r['v2ey'], r['v2ez']), (r['v2tx'], r['v2ty'], r['v2tz'])
    if r['lent'] or not (r['p2ex'] or r['p2ey'] or r['p2ez']):
        return (r['ex'], r['ey'], r['ez']), (r['tx'], r['ty'], r['tz'])
    return (r['p2ex'], r['p2ey'], r['p2ez']), (r['p2tx'], r['p2ty'], r['p2tz'])


def off_axis(r, h, lift=60.0):
    """Degrees between the half's line of sight and its own player (their chest, `lift` up). None with no camera."""
    e, t = view(r, h)
    p = R.pos(r, h)
    f = [b - a for a, b in zip(e, t)]
    v = [p[0] - e[0], p[1] - e[1], p[2] + lift - e[2]]
    nf, nv = math.sqrt(sum(x * x for x in f)), math.sqrt(sum(x * x for x in v))
    if nf < 1e-3 or nv < 1e-3 or not any(e) or any(x != x for x in e + t):
        return None
    c = sum(a * b for a, b in zip(f, v)) / nf / nv
    return math.degrees(math.acos(max(-1.0, min(1.0, c))))


def in_view(r, h):
    a = off_axis(r, h)
    return a is not None and a < VIEW_DEG


def on_room_camera(rooms, r, h):
    e, _ = view(r, h)
    if not any(e):
        return False
    near = R.nearest_camera(rooms, r['room'], e)
    return near is not None and near[0] < CAMERA_NEAR


def camera_label(rooms, r, h):
    e, _ = view(r, h)
    if not any(e):
        return '-'
    near = R.nearest_camera(rooms, r['room'], e)
    return near[1] if near and near[0] < 40 else 'free %d,%d,%d' % tuple(round(x) for x in e)


def busy(r, p):
    """Anything of this player's up, or a scene, or a door: their camera is the game's business then."""
    return bool(r['cutscene'] or R.activity(r, p) or r[p + 'st'] in (121, 80, 81) or (p == 'p1' and r['menu']))


def fraction(rows, test):
    rows = list(rows)
    return sum(1 for r in rows if test(r)) / len(rows) if rows else 0.0


# ---- the moves files --------------------------------------------------------------------------------------------

_moves = {}


def moves_of(model):
    """Every move number in a character's moves file (chars/charNNNN.cpk), as tools/ed/facts/checks/moves_of.py reads them.
    None if the disc can't be read."""
    if model in _moves:
        return _moves[model]
    try:
        import struct
        import disc
        b = disc.Disc().read('Chars/char%04d.cpk' % int(model))
        u32 = lambda o: struct.unpack_from('>I', b, o)[0]
        u16 = lambda o: struct.unpack_from('>H', b, o)[0]
        n, where, out = u32(16), u32(20), set()
        if len(b) >= 64 and n <= 64:
            for k in range(n):
                part = u32(where + k * 16)
                for i in range(u16(part + 20)):
                    out.add(u16(part + u32(part + 24) + i * 296 + 6))
        _moves[model] = out
    except Exception:
        _moves[model] = None
    return _moves[model]


# ---- one step ---------------------------------------------------------------------------------------------------

class Verdict:
    def __init__(self, step):
        self.step, self.checks, self.status, self.why = step, [], None, ''

    def check(self, name, ok, text):
        self.checks.append((name, bool(ok), text))
        return ok

    def info(self, name, text):
        self.checks.append((name, None, text))

    def done(self, status=None, why=''):
        if status:
            self.status, self.why = status, why
        elif self.status is None:
            self.status = 'FAIL' if any(ok is False for _, ok, _ in self.checks) else 'PASS'
        return self


def other(p):
    return 'p2' if p == 'p1' else 'p1'


def judge_walk(v, rows, s, e, plan):
    for p in ('p1',) if plan['solo'] else ('p1', 'p2'):
        d = walked(window(rows, s, e), p)
        v.check('walks', d >= STICK_MIN_WALK, '%s walked %.0f from where Jump to Game put them (at least %d)' % (p.upper(), d, STICK_MIN_WALK))


def tome_up(r, p):
    return R.tome_of(r, p) or (p == 'p1' and r['menu'] and not r['copy'])


def camera_back(v, rows, rooms, after, p, label, both=True):
    """After `label` ends at frame `after`: each half on one of the room's cameras, with its own player in it, for most of
    the next 30..150 frames they're free."""
    for h in ((p, other(p)) if both else (p,)):
        part = [r for r in window(rows, after + 30, after + 150) if not busy(r, h)]
        if not part:
            v.info('camera', '%s had no free frames after %s to look at' % (h.upper(), label))
            continue
        seen = fraction(part, lambda r: in_view(r, h))
        on = fraction(part, lambda r: on_room_camera(rooms, r, h))
        worst = max(part, key=lambda r: off_axis(r, h) or 0)
        v.check('camera %s' % h, seen >= SETTLED,
                "%s in their own view %.0f%% of the frames after %s (worst %.0f deg at frame %d, camera %s)" % (
                    h.upper(), seen * 100, label, off_axis(worst, h) or -1, worst['frame'], camera_label(rooms, worst, h)))
        v.check('room camera %s' % h, on >= SETTLED, "%s's eye on one of room %d's cameras %.0f%% of the frames after %s" % (
            h.upper(), part[0]['room'], on * 100, label))


def judge_tome(v, rows, log, rooms, s, e, plan):
    p = v.step['actor']
    win = window(rows, s, e)
    up = [r for r in win if tome_up(r, p)]
    if not v.check('opens', len(up) >= 20, '%s\'s Tome up for %d frames (Start at +0)' % (p.upper(), len(up))):
        return
    v.check('closes', not any(tome_up(r, p) for r in window(rows, e - 120, e)), '%s\'s Tome closed by the end' % p.upper())
    first, last = up[0]['frame'], up[-1]['frame']
    if not plan['solo']:
        o = other(p)
        d = walked(window(rows, first, last), o)
        v.check('other walks', d >= OTHER_MIN_WALK, '%s walked %.0f while %s\'s Tome was up (no pause)' % (o.upper(), d, p.upper()))
        v.check('one Tome', not any(tome_up(r, o) for r in window(rows, first, last)), 'only %s\'s Tome opened' % p.upper())
    after = window(rows, last + 60, last + 150)
    eight = [r for r in after if r['camidx'] == 8 and not r['menu']]
    v.check('record 8', not eight, "the game's camera off the Tome's record 8 after it closed%s" % (
        '' if not eight else ' (on it at frame %d)' % eight[0]['frame']))
    camera_back(v, rows, rooms, last, p, 'the Tome closed', both=not plan['solo'])


def judge_interact(v, rows, log, rooms, s, e, plan):
    p, step = v.step['actor'], v.step
    o = other(p)
    if lines_in(log, s - 2, s + 5, r'\[sweep\] player %d not put' % (1 if p == 'p1' else 2)):
        return v.done('SKIP', 'no floor under %s for %s' % (step['spot'], p.upper()))
    win = window(rows, s, e)
    own = [r for r in win if R.activity(r, p) == 'close-up']
    v.check('close-up', len(own) >= 10, '%s had their own close-up for %d frames (%s, script %d)' % (p.upper(), len(own), step['region'], step['script']))
    if not plan['solo']:
        theirs = [r for r in win if R.activity(r, o) == 'close-up']
        v.check('only theirs', not theirs, '%s had no close-up of their own meanwhile%s' % (o.upper(), '' if not theirs else ' (%d frames from %d)' % (len(theirs), theirs[0]['frame'])))
        d = walked(window(rows, s + 50, s + 600), o)
        v.check('other walks', d >= OTHER_MIN_WALK, '%s walked %.0f through it (no pause)' % (o.upper(), d))
    words = [l for _, l, _ in lines_in(log, s, e, r'\[trace\] words: (?!\\i)')]
    v.check('words', words, 'its words were shown%s' % (': ' + words[0].split('words: ', 1)[1][:60] if words else ''))
    end = window(rows, e - 60, e)
    v.check('back', end and all(R.activity(r, 'p1') is None and R.activity(r, 'p2') is None and not r['menu'] and not r['window'] for r in end),
            'both back with nothing up at the end')
    if own:
        ten = [r for r in window(rows, own[-1]['frame'] + 60, own[-1]['frame'] + 150) if r['camidx'] in (8, 10) and not any(busy(r, q) for q in ('p1', 'p2'))]
        v.check('record 10', not ten, "the game's camera off the close-up's record 10 after RETURN%s" % ('' if not ten else ' (on %d at frame %d)' % (ten[0]['camidx'], ten[0]['frame'])))
        camera_back(v, rows, rooms, own[-1]['frame'], p, 'the close-up', both=not plan['solo'])
    if step['kind'] == 'pickup':
        n = 1 if p == 'p1' else 2
        found = lines_in(log, s, e, r'\[trace\] words: .*has (?:picked up|found) a')
        v.check('found', found, '%s\'s find words (has picked up / has found)' % p.upper())
        carried = lambda r: max(r.get('p1carry', -1), 0) + max(r.get('p2carry', -1), 0)
        a, b = at(rows, s + 10), at(rows, e - 20)
        if a and b and 'p1carry' in a:
            v.check('carried', carried(b) > carried(a), 'what both carry went %d to %d' % (carried(a), carried(b)))
        drawn = lines_in(log, s, e, r'is drawn for player (\d) alone')
        if drawn:
            v.check('drawn', all(int(m.group(1)) == n for _, _, m in drawn), 'the find drawn for player %s alone (finder %d)' % (
                '/'.join(m.group(1) for _, _, m in drawn), n))
        if not plan['solo']:
            notice = lines_in(log, s, e, r'\[coop\] notice for player (\d)')
            v.check('notice', any(int(m.group(1)) == 3 - n for _, _, m in notice), 'the other player told by a notice%s' % (
                '' if notice else ' (none logged)'))
        if found and n == 1:
            v.info('name', found[0][1].split('words: ', 1)[1][:80])


def judge_door(v, rows, log, rooms, s, e, plan, ref):
    p, step = v.step['actor'], v.step
    o = other(p)
    win = window(rows, s, e)
    if step['mode'] == 'real' and lines_in(log, s - 2, s + 5, r'\[sweep\] player %d not put' % (1 if p == 'p1' else 2)):
        return v.done('SKIP', 'no floor in the door region %s for %s' % (step['region'], p.upper()))
    change = None
    for a, b in zip(win, win[1:]):
        if a['room'] == step['room'] and b['room'] in step['to_rooms']:
            change = b
            break
    tickets = lines_in(log, s, e, r'\[room\] ticket 0x([0-9A-F]+) 0x([0-9A-F]+)')
    if not change:
        why = 'no room change to %s' % '/'.join(str(x) for x in step['to_rooms'])
        if tickets:
            why += ', though a ticket for room %d was written' % int(tickets[0][2].group(1), 16)
        elif step['mode'] == 'real':
            n = 1 if p == 'p1' else 2
            shown = lines_in(log, s, e, r"player 2's prompt (\d+)") if n == 2 else lines_in(log, s, e, r'\[trace\] words: \\i21.*(OPEN DOOR|CLIMB|ENTER|EXAMINE)')
            if win and p + 'prompt' in win[0]:
                ids = sorted({int(r[p + 'prompt']) for r in win if r[p + 'prompt'] > 0})
                shown = shown or any(i in step['asks'] for i in ids)
                why += ' (prompts %s had up: %s)' % (p.upper(), ids or 'none')
            why += '; its prompt %s' % ('showed, B pressed, and nothing came of it' if shown else 'never showed for %s' % p.upper()) if step['presses'] else '; walking into the zone did nothing'
        v.check('room', False, why)
        return
    tc = change['frame']
    dest = next((t for t in step['to'] if t['to_room'] == change['room']), step['to'][0])
    v.check('room', True, 'room %d to %d at frame %d (+%d)' % (step['room'], change['room'], tc, tc - s))
    if 'tkby' in change:
        by = at(rows, tc)['tkby']
        v.check('owner', by == (1 if p == 'p1' else 2), 'the ticket was written as player %s (the taker is %s)' % (by, p.upper()))
    # The taker's motion out, and in (event 238 plays it on arrival, from what's playing then)
    if step['motion_out'] and step['mode'] == 'real':
        out = [r for r in window(rows, s, tc) if r[p + 'st'] == 121]
        v.check('motion out', any(r[p + 'anim'] == dest['motion'] for r in out), '%s went out on the door\'s motion %s%s' % (
            p.upper(), dest['motion'], '' if out else ' (never in state 121)'))
    if step['motion_in'] and step['mode'] == 'real':
        arrive = [r for r in window(rows, tc, tc + 240) if r[p + 'st'] == 80]
        v.check('motion in', arrive, '%s arrived on the arrival motion (state 80)' % p.upper())
    # Places: the taker at the door's entry marker, the follower beside them, on the floor, in the same room
    mk = coopplan.marker(change['room'], dest['entry']) if dest.get('entry') else None
    arr = [r for r in window(rows, tc, tc + 90) if r['room'] == change['room'] and any(R.pos(r, p))]
    if mk and arr:
        d = min(math.hypot(r[p + 'x'] - mk['pos'][0], r[p + 'y'] - mk['pos'][1]) for r in arr)
        v.check('taker at entry', d <= ARRIVE_ENTRY, '%s arrived %.0f from %s (at most %d)' % (p.upper(), d, dest['entry'], ARRIVE_ENTRY))
    if not plan['solo']:
        r30 = at(rows, tc + 30)
        if r30:
            d = R.dist(R.pos(r30, p), R.pos(r30, o))
            dz = abs(r30[p + 'z'] - r30[o + 'z'])
            v.check('follower beside', 20 <= d <= FOLLOW_NEAR and dz <= SAME_FLOOR,
                    '%s followed to %.0f from %s, %.0f higher or lower (beside: 20 to %d, within %d)' % (o.upper(), d, p.upper(), dz, FOLLOW_NEAR, SAME_FLOOR))
            if 'p2room' in r30:
                v.check('same room', r30['p1room'] == r30['p2room'], 'both players\' own rooms %d and %d' % (r30['p1room'], r30['p2room']))
        later = window(rows, tc + 30, s + step['walk_after'] + 200)
        if later:
            z0 = later[0][o + 'z']
            low = min(r[o + 'z'] for r in later)
            v.check('follower on a floor', z0 - low <= FELL_DROP, '%s stayed on a floor after arriving (dropped %.0f)' % (o.upper(), z0 - low))
        v.check('follower stands', not any(r[o + 'st'] == 121 for r in window(rows, s, tc)), '%s didn\'t take a door motion of their own' % o.upper())
    # Control back, then both cameras on the new room's with their own player in view
    ctrl = next((r for r in window(rows, tc, tc + 500) if r['p1st'] == 1 and (plan['solo'] or r['p2st'] == 1) and not r['cutscene']), None)
    if not v.check('control', ctrl, 'both in control within 500 frames of arriving%s' % ('' if not ctrl else ' (+%d)' % (ctrl['frame'] - tc))):
        return
    camera_back(v, rows, rooms, ctrl['frame'], p, 'the door', both=not plan['solo'])
    first = at(rows, ctrl['frame'] + 20)
    have = camera_label(rooms, first, p)
    v.info('arrival camera', "%s's half on %s; the door names %s" % (p.upper(), have, dest.get('camera')))
    key = '%d->%d' % (step['script'], change['room'])
    if ref and key in ref.get('doors', {}):
        want = ref['doors'][key].get('camera')
        v.check('as solo', have == want, "%s's arrival camera %s, the one-player game's %s" % (p.upper(), have, want))
    # Both walk on in the new room
    walk = window(rows, s + step['walk_after'], e)
    for q in (p,) if plan['solo'] else (p, o):
        d = walked(walk, q)
        v.check('walks after', d >= OTHER_MIN_WALK, '%s walked %.0f in the new room' % (q.upper(), d))
    if any(r['room'] != change['room'] for r in window(rows, tc + 5, e)):
        v.check('stays', False, 'the room changed again before the step ended (sent back?)')
    v.result_room = change['room']


def judge_fight(v, rows, log, rooms, s, e, plan, ref):
    p = v.step['actor']
    o = other(p)
    win = window(rows, s, e)
    fin = [r for r in win if r[p + 'st'] == 30]
    asked = lines_in(log, s, e, r'FINISH HIM') + (lines_in(log, s, e, r"player 2's prompt 23\b") if p == 'p2' else [])
    if not fin:
        if asked:
            v.check('finisher', False, 'FINISH HIM was asked and B pressed, and %s never went into state 30' % p.upper())
            return
        return v.done('SKIP', 'no finisher: nobody downed in reach (character %s placed at %s)' % (v.step['chr'], v.step['foe_at']))
    model = fin[0][p + 'm']
    anims = sorted({r[p + 'anim'] for r in fin})
    idle = {r[p + 'anim'] for r in win if r[p + 'st'] == 1}
    v.check('finisher', len(fin) >= 20, '%s in state 30 for %d frames, motion %s' % (p.upper(), len(fin), anims))
    v.check('its motion', any(a not in idle for a in anims), 'the finisher plays a motion that isn\'t their standing or walking one')
    moves = moves_of(model)
    if moves is not None:
        v.check('own moves', all(a in moves for a in anims), 'motion %s is in model %d\'s own moves file (char%04d.cpk)' % (anims, model, model))
    want = (ref or {}).get('finisher')
    if want:
        v.check('as solo', set(anims) & set(want), 'the one-player game finishes with %s, %s played %s' % (want, p.upper(), anims))
    if not plan['solo']:
        d = walked(window(rows, fin[0]['frame'], fin[-1]['frame']), o)
        v.check('other walks', d >= OTHER_MIN_WALK or fin[-1]['frame'] - fin[0]['frame'] < 40, '%s walked %.0f through the finisher' % (o.upper(), d))
    back = next((r for r in window(rows, fin[-1]['frame'], fin[-1]['frame'] + 300) if r[p + 'st'] == 1), None)
    v.check('back', back, '%s back in control after it' % p.upper())
    if back:
        camera_back(v, rows, rooms, back['frame'], p, 'the finisher', both=False)
    v.finisher = anims


# ---- the whole run ----------------------------------------------------------------------------------------------

def global_rules(rows, log, rooms, base, end, plan, run):
    """What must never happen, whatever the step: recording.check's rules (freezes, stalls, sends, falls, camera jumps,
    bounces) without the puts' own jumps, and the new ones: a player out of their own half's view, a player with no body,
    the players in two rooms, the game's player left as player 2."""
    out = []
    part = window(rows, base, end)
    puts = [f for f, _, _ in lines_in(log, base, end + 5, r'\[sweep\] player \d put at')]
    changes = [b['frame'] for a, b in zip(part, part[1:]) if a['room'] != b['room']]
    near = lambda f, lst, before, after: any(-before <= f - q <= after for q in lst)
    if part and 'p1ssx' in part[0]:
        # Player 1's scripted stick: the recorder reads the pad before hle.c adds ED_INPUT_SCRIPT (both_record is called
        # before input_script_apply), so p1sx/p1sy are 0 in a scripted run and recording.check can't see them push.
        part = [dict(r, p1sx=r['p1ssx'] or r['p1sx'], p1sy=r['p1ssy'] or r['p1sy']) for r in part]
    for f in R.check(part):
        if f['rule'] == 'STUCK?':
            continue
        if f['rule'] in ('SENT', 'CAMERA', 'BOUNCE') and near(f.get('from', 0), puts, 3, 180):
            continue
        out.append(f)
    if os.path.isdir(os.path.join(run, 'shots')):
        out += R.check_pictures(run, part)
    for h in ('p1',) if plan['solo'] else ('p1', 'p2'):
        start = None
        for r in part + [None]:
            # Only their own action, a scene or a door excuses it: the other's close-up or Tome doesn't (each half is its own).
            lost = (r is not None and not busy(r, h) and not r[other(h) + 'st'] in (121, 80, 81) and not near(r['frame'], puts, 0, 120)
                    and not near(r['frame'], changes, 30, 150) and (h == 'p1' or r['p2m'] >= 0) and off_axis(r, h) is not None and not in_view(r, h))
            if lost and start is None:
                start = r
            elif not lost and start is not None:
                if last['frame'] - start['frame'] >= LOST_FRAMES:
                    out.append({'rule': 'LOST', 'who': h, 'from': start['frame'], 'to': last['frame'], 'text':
                                "%s out of their own half's view for %d frames (%.0f deg off, room %d, camera %s%s)" % (
                                    h.upper(), last['frame'] - start['frame'], off_axis(start, h) or -1, start['room'],
                                    camera_label(rooms, start, h), ", player 2's own camera never stepped: the game's (player 1's) drawn"
                                    if h == 'p2' and not (start['p2ex'] or start['p2ey']) and 'v2on' not in start else '')})
                start = None
            last = r
    if not plan['solo']:
        gone = [r for r in part if r['p2m'] < 0]
        if gone:
            out.append({'rule': 'NO_P2', 'who': 'p2', 'from': gone[0]['frame'], 'to': gone[-1]['frame'], 'text': 'player 2 had no body for %d frames' % len(gone)})
        if part and 'p2room' in part[0]:
            apart = [r for r in part if r['p1room'] != r['p2room'] and not near(r['frame'], changes, 10, 120) and not r['cutscene']]
            if len(apart) > 30:
                out.append({'rule': 'TWO_ROOMS', 'from': apart[0]['frame'], 'to': apart[-1]['frame'], 'text':
                            'the players in rooms %d and %d for %d frames' % (apart[0]['p1room'], apart[0]['p2room'], len(apart))})
        if part and 'curp' in part[0]:
            leak = [r for r in part if (r['curp'] == 2 or r['mainp'] == 2) and r['use'] != 2 and not r['screen'] and not R.tome_of(r, 'p2')]
            if len(leak) > 10:
                out.append({'rule': 'P2_LEFT_AS_PLAYER', 'from': leak[0]['frame'], 'to': leak[-1]['frame'], 'text':
                            "the game's %s pair named player 2 for %d frames with nothing of theirs running" % (
                                'current' if leak[0]['curp'] == 2 else 'main', len(leak))})
        asked = {(s['actor'], s['at'] + base) for s in plan['steps'] if s['kind'] == 'tome'}
        for p in ('p1', 'p2') if plan.get('steps') else ():
            start = None
            for r in part + [None]:
                on = r is not None and tome_up(r, p)
                if on and start is None:
                    start = r
                elif not on and start is not None:
                    if not any(q == p and abs(start['frame'] - f) < 60 for q, f in asked) and last['frame'] - start['frame'] > 20:
                        out.append({'rule': 'TOME_UNASKED', 'who': p, 'from': start['frame'], 'to': last['frame'], 'text':
                                    "%s's Tome opened with no Start pressed (%d frames): a forced Tome pauses play" % (p.upper(), last['frame'] - start['frame'])})
                    start = None
                last = r
    if not plan['solo']:
        out += same_place_same_camera(part, rooms, puts)
    hangs = []
    if rows:
        closed = os.path.getmtime(os.path.join(run, 'both.tsv'))
        opened = closed - (rows[-1]['ms'] - rows[0]['ms']) / 1000.0
        hangs = [h for h in glob.glob(os.path.join(R.disc.ROOT, 'saves', 'reports', 'hang-*.txt')) if opened - 5 <= os.path.getmtime(h) <= closed + 60]
    for h in hangs:
        out.append({'rule': 'HANG', 'text': 'the watchdog wrote %s' % os.path.relpath(h, R.disc.ROOT)})
    order = {'HANG': 0, 'FROZEN': 1, 'STALL': 2, 'NO_P2': 3, 'TWO_ROOMS': 4, 'P2_LEFT_AS_PLAYER': 5, 'LOST': 6, 'CAMERA_APART': 7, 'CAMERA': 7, 'SENT': 8,
             'FELL': 9, 'TOME_UNASKED': 10, 'BOUNCE': 11, 'PICTURE': 12}
    out.sort(key=lambda f: (order.get(f['rule'], 20), f.get('from', 0)))
    return out


def regions_at(rooms, room, p):
    """The names of the room's box and sphere regions a place is inside: where the game's zone scripts pick cameras."""
    out = []
    for g in rooms.get(str(room), {}).get('regions', []):
        if 'min' in g and g.get('shape') != 2:
            lo, hi = g['min'], g['max']
        elif 'pos' in g and 'radius' in g and g.get('shape') == 0:
            lo = [g['pos'][k] - g['radius'] for k in range(3)]
            hi = [g['pos'][k] + g['radius'] for k in range(3)]
        else:
            continue
        if all(lo[k] - 1 <= p[k] <= hi[k] + 1 for k in range(2)) and lo[2] - 60 <= p[2] <= hi[2] + 60:
            out.append(g['name'])
    return tuple(sorted(out))


def same_place_same_camera(part, rooms, puts, near=320.0, frames=120):
    """The game picks its camera by where the player stands (zone scripts, fn8): two players standing together, inside
    the same regions, each on the original camera, should be seen from the same room camera. One half kept on another
    (an opening's IntroCam, the last room's, the other player's close-up) is that half's camera not following its player."""
    out, start, last = [], None, None
    for r in part + [None]:
        differ = False
        if r is not None and not busy(r, 'p1') and not busy(r, 'p2') and r['p2m'] >= 0 and not any(-3 <= r['frame'] - q <= 150 for q in puts):
            if R.dist(R.pos(r, 'p1'), R.pos(r, 'p2')) < near and regions_at(rooms, r['room'], R.pos(r, 'p1')) == regions_at(rooms, r['room'], R.pos(r, 'p2')):
                a, b = camera_label(rooms, r, 'p1'), camera_label(rooms, r, 'p2')
                differ = a != b and not (a.startswith('free') and b.startswith('free'))
        if differ and start is None:
            start = r
        elif not differ and start is not None:
            if last['frame'] - start['frame'] >= frames:
                out.append({'rule': 'CAMERA_APART', 'who': 'p2', 'from': start['frame'], 'to': last['frame'], 'text':
                            'standing together in the same regions of room %d, P1 seen from %s and P2 from %s for %d frames' % (
                                start['room'], camera_label(rooms, start, 'p1'), camera_label(rooms, start, 'p2'), last['frame'] - start['frame'])})
            start = None
        last = r
    return out


def judge(run, plan, ref=None, base=None):
    """All of one run: (base row, step verdicts, global findings, the recording's lacks)."""
    rows = R.load(run)
    log = log_lines(run)
    rooms = R.room_cameras()
    start = playable(rows, plan['solo']) if rows else None
    if start is None:
        return None, [], [{'rule': 'NEVER_PLAYABLE', 'text': 'both players were never in control%s' % (
            ' (last frame %d, room %d)' % (rows[-1]['frame'], rows[-1]['room']) if rows else ', and nothing was recorded')}], []
    base = start['frame'] if base is None else base
    verdicts, room_now = [], start['room']
    if start['room'] != plan['start_room']:
        verdicts.append(Verdict(dict(kind='start', actor='both', at=0, len=0, room=start['room'])).done(
            'FAIL', 'Jump to Game started in room %d, the plan is for %d' % (start['room'], plan['start_room'])))
    last_frame = rows[-1]['frame']
    for step in plan['steps']:
        v = Verdict(step)
        s, e = base + step['at'], base + step['at'] + step['len']
        verdicts.append(v)
        if e > last_frame + 30:
            v.done('BLOCKED', 'the run ended at frame %d, before this step\'s end (%d): a freeze or a crash?' % (last_frame, e))
            continue
        here = at(rows, s)
        if here['room'] != step['room']:
            v.done('BLOCKED', 'planned for room %d, the players were in %d' % (step['room'], here['room']))
            continue
        k = step['kind']
        if k == 'walk':
            judge_walk(v, rows, s, e, plan)
        elif k == 'tome':
            judge_tome(v, rows, log, rooms, s, e, plan)
        elif k in ('examine', 'pickup'):
            judge_interact(v, rows, log, rooms, s, e, plan)
        elif k == 'door':
            judge_door(v, rows, log, rooms, s, e, plan, ref)
        elif k == 'fight':
            judge_fight(v, rows, log, rooms, s, e, plan, ref)
        v.done()
    findings = global_rules(rows, log, rooms, base, min(last_frame, base + plan['end']), plan, run)
    have = set(rows[0]) if rows else set()
    lacks = [c for c in ('p1room', 'tkby', 'v2on', 'curp', 'p1prompt', 'p1ssx') if c not in have]
    return start, verdicts, findings, lacks


def any_run(run):
    """The rules that need no plan, on any recording (an old run.sh or sweep run): from the first playable frame on."""
    rows = R.load(run)
    start = playable(rows, False) or playable(rows, True)
    if start is None:
        return None, []
    plan = dict(solo=start['p2m'] < 0, steps=[], end=rows[-1]['frame'] - start['frame'])
    return start, global_rules(rows, log_lines(run), R.room_cameras(), start['frame'], rows[-1]['frame'], plan, run)
