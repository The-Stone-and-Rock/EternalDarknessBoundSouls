"""Reads a run's both.tsv (the game's ED_BOTH recording): both players, every frame. Checks the user's whole-game rules
on it, and tells what each player was doing, side by side."""
import csv
import glob
import json
import math
import os

import disc

STICK = 40  # a stick pushed this far means they asked to walk
BOOK_EYE = (200.0, 1100.0, 100.0)
INT = {'ticket', 'fade', 'p1anim', 'p2anim', 'camidx', 'p1carry', 'p2carry', 'frame', 'ms', 'room', 'p1st', 'p1hp', 'p1m', 'p2st', 'p2hp', 'p2m', 'p1sx', 'p1sy', 'p2sx', 'p2sy', 'script', 'mode', 'mode2',
       'screen', 'owner', 'shares', 'copy', 'copyp1', 'lent', 'cutscene', 'menu'}
HEX = {'camhash', 'p1b', 'p2b', 'busy', 'hold', 'focus', 'focus2', 'window', 'who', 'ctl', 'entry'}


def find_run(name=None):
    """A run folder: a path, a part of its name, or the newest."""
    if name and os.path.isdir(name):
        return name
    if name and os.path.isdir(os.path.join(disc.ROOT, name)):
        return os.path.join(disc.ROOT, name)
    runs = sorted(glob.glob(os.path.join(disc.ROOT, 'logs', 'run-*')), key=os.path.getmtime, reverse=True)
    for r in runs:
        if os.path.exists(os.path.join(r, 'both.tsv')) and (not name or name in os.path.basename(r)):
            return r
    raise SystemExit('no run with a recording' + (' matching ' + name if name else '') + ' (runs need tools/ed/run.sh)')


def load(run):
    rows = []
    if not os.path.exists(os.path.join(run, 'both.tsv')):
        raise SystemExit('%s has no both.tsv: it was not made with tools/ed/run.sh. Its page-*.log files are all there is: grep them.' % run)
    with open(os.path.join(run, 'both.tsv'), newline='') as f:
        r = csv.reader(f, delimiter='\t')
        head = next(r)
        for line in r:
            if len(line) != len(head):
                continue  # the row a closing game cut short
            row = {}
            for k, v in zip(head, line):
                # The Windows runtime prints a bad float as -nan(ind): float() would stop the whole run's reading.
                row[k] = int(v) if k in INT else int(v, 16) if k in HEX else (float('nan') if 'nan' in v else float(v))
            rows.append(row)
    return rows


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def pos(r, p):
    return (r[p + 'x'], r[p + 'y'], r[p + 'z'])


def stick(r, p):
    return max(abs(r[p + 'sx']), abs(r[p + 'sy']))


def has_player2(r):
    return r['p2m'] >= 0


def eye(r, half):
    """What each half looks from. In the player's own close-up the host lends the game's camera to player 2."""
    if half == 'p1':
        if r['lent'] and (r['p1ex'] or r['p1ey']):
            return (r['p1ex'], r['p1ey'], r['p1ez'])
        return (r['ex'], r['ey'], r['ez'])
    if r['lent']:
        return (r['ex'], r['ey'], r['ez'])
    return (r['p2ex'], r['p2ey'], r['p2ez'])


def tome_of(r, p):
    """Whether this player has a Tome up, so their stick isn't for walking."""
    if r['copy'] and (r['copyp1'] == (p == 'p1')):
        return True
    if r['shares'] and r['owner'] == (0 if p == 'p1' else 1):
        return True
    return False


def own_close_up(r, p):
    """Whether this player's own close-up or words are up: they stand, and that's right."""
    if p == 'p2':
        return bool(r['screen'])
    return bool(r['busy'] or r['window']) and not r['screen']


def other_busy(r, p):
    """Whether the other player's action is up: the rule is that this player keeps playing."""
    other = 'p1' if p == 'p2' else 'p2'
    return own_close_up(r, other) or tome_of(r, other)


def activity(r, p):
    if r['cutscene']:
        return 'cutscene'
    st = r.get(p + 'st')
    if st == 121:
        return 'door motion %s' % r.get(p + 'anim', '?')
    if st in (80, 81):
        return 'arrives, motion %s' % r.get(p + 'anim', '?')
    if r['menu'] and not r['shares'] and not r['copy']:
        return 'menu'
    if tome_of(r, p):
        return 'Tome'
    if own_close_up(r, p):
        return 'close-up'
    return None


def check(rows, min_frames=30):
    """The user's rules for the whole game, with no per-room knowledge. Returns findings, worst first."""
    out = []
    if not rows:
        return [{'rule': 'NO_DATA', 'text': 'no rows recorded'}]
    # Asked to walk, and doesn't move. A wall does this too, so it's a finding only when someone's action is up; alone
    # it's listed as a maybe.
    for p in ('p1', 'p2'):
        start = None
        for i, r in enumerate(rows + [None]):
            pushing = r is not None and stick(r, p) >= STICK and activity(r, p) is None and (p == 'p1' or has_player2(r))
            if pushing and start is None:
                start = i
            elif not pushing and start is not None:
                a, b = rows[start], rows[i - 1]
                n = b['frame'] - a['frame']
                moved = dist(pos(a, p), pos(b, p))
                if n >= min_frames and moved < 5:
                    other = [x for x in rows[start:i] if other_busy(x, p)]
                    rule = 'FROZEN' if other else 'STUCK?'
                    out.append({'rule': rule, 'who': p, 'from': a['frame'], 'to': b['frame'], 'text':
                                "%s pushed the stick for %d frames and moved %.1f at %.0f %.0f %.0f%s" % (
                                    p.upper(), n, moved, *pos(a, p),
                                    ', with the other player\'s action up (script %d, busy 0x%X, window 0x%X)' % (a['script'], a['busy'], a['window'])
                                    if other else ' with nothing up: a wall, or a real stop')})
                start = None
    # A place changing by itself: sent somewhere, or falling.
    for p in ('p1', 'p2'):
        for a, b in zip(rows, rows[1:]):
            if (p == 'p2' and not (has_player2(a) and has_player2(b))) or a['room'] != b['room'] or b['frame'] - a['frame'] > 3:
                continue
            if a['cutscene'] or b['cutscene']:
                continue
            d = dist(pos(a, p), pos(b, p))
            if d > 150 and any(pos(a, p)) and any(pos(b, p)):
                out.append({'rule': 'SENT', 'who': p, 'from': a['frame'], 'to': b['frame'], 'text': '%s moved %.0f units in %d frames with no room change: %s to %s' % (
                    p.upper(), d, b['frame'] - a['frame'], tuple(round(x) for x in pos(a, p)), tuple(round(x) for x in pos(b, p)))})
        i = 0
        while i + 30 < len(rows):
            a, b = rows[i], rows[i + 30]
            if a['room'] == b['room'] and (p == 'p1' or has_player2(a)) and a[p + 'z'] - b[p + 'z'] > 250 and not a['cutscene']:
                out.append({'rule': 'FELL', 'who': p, 'from': a['frame'], 'to': b['frame'], 'text': '%s dropped %.0f units in 30 frames' % (p.upper(), a[p + 'z'] - b[p + 'z'])})
                i += 120
            i += 1
    # Time passing with no frames: a freeze the watchdog doesn't count.
    for a, b in zip(rows, rows[1:]):
        if b['ms'] - a['ms'] > 2500:
            out.append({'rule': 'STALL', 'from': a['frame'], 'to': b['frame'], 'text': 'no frame for %.1f s (room %d, script %d, Tome %d/%d)' % (
                (b['ms'] - a['ms']) / 1000, a['room'], a['script'], a['shares'], a['copy'])})
    out += check_cameras(rows)
    order = {'FROZEN': 0, 'STALL': 1, 'SENT': 2, 'FELL': 3, 'CAMERA': 4, 'BOUNCE': 5, 'STUCK?': 6}
    out.sort(key=lambda f: (order.get(f['rule'], 9), f.get('from', 0)))
    return out


def room_cameras():
    path = os.path.join(disc.DB, 'rooms.json')
    return json.load(open(path)) if os.path.exists(path) else {}


_paths = None


def _to_segment(a, b, p):
    ab = [y - x for x, y in zip(a, b)]
    n = sum(x * x for x in ab)
    s = 0.0 if not n else max(0.0, min(1.0, sum((q - x) * y for x, y, q in zip(a, ab, p)) / n))
    return dist([x + s * y for x, y in zip(a, ab)], p)


def camera_paths():
    global _paths
    if _paths is None:
        path = os.path.join(disc.DB, 'camera_paths.json')
        _paths = json.load(open(path)) if os.path.exists(path) else {}
    return _paths


def nearest_camera(rooms, room, at):
    """The room's camera nearest this eye. A moving camera (814 of the 2,177) counts anywhere along its keys."""
    best = None
    moving = camera_paths().get(str(room), {})
    for c in rooms.get(str(room), {}).get('cameras', []):
        d = dist(c['pos'], at)
        keys = moving.get(str(c['hash']), {}).get('keys', [])
        for a, b in zip(keys, keys[1:]):
            d = min(d, _to_segment(a[1:], b[1:], at))
        if best is None or d < best[0]:
            best = (d, c['name'])
    return best


def check_cameras(rows):
    """Each half's eye. A cut to one of the room's own cameras is the game working; a jump to anywhere else, or an eye
    going back and forth, is not."""
    out = []
    rooms = room_cameras()
    for half in ('p1', 'p2'):
        last_flip = []
        was = None
        for a, b in zip(rows, rows[1:]):
            if a['room'] != b['room'] or a['cutscene'] or b['cutscene'] or (half == 'p2' and not has_player2(a)):
                was = None
                continue
            if tome_of(a, half) or tome_of(b, half) or b['frame'] - a['frame'] > 3:
                continue
            # The Tome's own book camera (eye 200, 1100, 100, FOV 45) shows for a frame or two as a Tome opens. Only that frame's
            # eye is skipped: mode2 stays 3 all the time a Tome is up, and the other player plays on through it.
            if dist(eye(a, half), BOOK_EYE) < 5 or dist(eye(b, half), BOOK_EYE) < 5:
                was = None
                continue
            if half == 'p1' and (a['menu'] or b['menu']) and not (a['shares'] or a['copy']):
                was = None
                continue
            ea, eb = eye(a, half), eye(b, half)
            if not any(ea) or not any(eb):
                continue
            d = dist(ea, eb)
            if d > 120:
                near = nearest_camera(rooms, b['room'], eb)
                if near is None or near[0] > 60:
                    out.append({'rule': 'CAMERA', 'who': half, 'from': a['frame'], 'to': b['frame'], 'text':
                                "%s's eye jumped %.0f units to %s, %s" % (
                                    half.upper(), d, tuple(round(x) for x in eb),
                                    'not near any camera of room %d (nearest %s, %.0f away)' % (b['room'], near[1], near[0]) if near
                                    else 'room %d has no cameras listed' % b['room'])})
            # back and forth
            if was is not None and d > 1.5:
                v = tuple(y - x for x, y in zip(ea, eb))
                if sum(x * y for x, y in zip(v, was)) < 0:
                    last_flip = [f for f in last_flip if b['frame'] - f < 60] + [b['frame']]
                    if len(last_flip) >= 6:
                        out.append({'rule': 'BOUNCE', 'who': half, 'from': last_flip[0], 'to': b['frame'], 'text':
                                    "%s's eye went back and forth %d times in a second" % (half.upper(), len(last_flip))})
                        last_flip = []
            if d > 1.5:
                was = tuple(y - x for x, y in zip(ea, eb))
    # One finding for each burst
    merged = []
    for f in out:
        if merged and merged[-1]['rule'] == f['rule'] and merged[-1]['who'] == f['who'] and f['from'] - merged[-1]['to'] < 60:
            merged[-1]['to'] = f['to']
            merged[-1]['n'] = merged[-1].get('n', 1) + 1
        else:
            merged.append(f)
    return merged


def status(r, a, b, p):
    """Words for one player over a span."""
    what = activity(b, p)
    if what:
        return what
    if p == 'p2' and not has_player2(b):
        return 'none'
    moved = dist(pos(a, p), pos(b, p))
    push = stick(b, p) >= STICK
    if moved > 3:
        return 'walks'
    return 'PUSHES, STILL' if push else 'stands'


def game_camera(rooms, r):
    """The room camera entry on the game's current eye record, by hash: set by fn8, fn10 or a room change. It's player 1's, or
    player 2's while lent. Blank at index 8 and during a load: `index N` then, as the record has no room camera on it."""
    h = r.get('camhash')
    if h is None:
        return None
    if not h:
        return 'index %s' % r.get('camidx')
    for c in rooms.get(str(r['room']), {}).get('cameras', []):
        if c['hash'] == h:
            return c['name']
    return '0x%08X' % h


def camera_name(rooms, r, half):
    e = eye(r, half)
    if not any(e):
        return '-'
    near = nearest_camera(rooms, r['room'], e)
    if near and near[0] < 40:
        return near[1]
    return 'free %d,%d,%d' % tuple(round(x) for x in e)


def timeline(rows, start=None, end=None, step=15):
    """Both players as spans: one line each time what either is doing, the room, the script or a camera changes."""
    rooms = room_cameras()
    picked = [r for r in rows if (start is None or r['frame'] >= start) and (end is None or r['frame'] <= end)]
    lines = []
    last = None
    since = None
    for i in range(0, len(picked) - 1, step):
        a, b = picked[i], picked[min(i + step, len(picked) - 1)]
        state = (b['room'], status(b, a, b, 'p1'), status(b, a, b, 'p2'), camera_name(rooms, b, 'p1'), camera_name(rooms, b, 'p2'),
                 b['script'], bool(b['window']), bool(b['busy']), bool(b['screen']), game_camera(rooms, b), (b['p1carry'], b['p2carry']) if 'p1carry' in b else None)
        if state != last:
            if last is not None:
                lines.append(_line(since, a, last))
            last, since = state, a['frame']
    if last is not None:
        lines.append(_line(since, picked[-1], last))
    return lines


def _line(since, r, s):
    return '%6d-%-6d room %-4d P1 %-20s P2 %-20s cam1 %-16s cam2 %-16s script %-5d%s%s%s' % (
        since, r['frame'], s[0], s[1], s[2], s[3][:16], s[4][:16], s[5], ' words' if s[6] else '', ' busy' if s[7] else '', ' p2-look' if s[8] else '') + (' game-cam ' + s[9] if s[9] else '') + (' carry %s/%s' % s[10] if s[10] else '')


def at_frame(rows, frame):
    best = min(rows, key=lambda r: abs(r['frame'] - frame))
    rooms = room_cameras()
    lines = ['frame %d  room %d  entry 0x%08X  script %d  who 0x%X  ctl 0x%X' % (best['frame'], best['room'], best['entry'], best['script'], best['who'], best['ctl'])]
    for p, name in (('p1', 'PLAYER 1'), ('p2', 'PLAYER 2')):
        e = eye(best, p)
        lines.append('%s  model %d  at %.0f %.0f %.0f  state %d  motion %s  health %d  stick %d %d  buttons 0x%X  doing: %s  camera %s (eye %.0f %.0f %.0f)' % (
            name, best[p + 'm'], *pos(best, p), best[p + 'st'], best.get(p + 'anim', '?'), best[p + 'hp'], best[p + 'sx'], best[p + 'sy'], best[p + 'b'],
            activity(best, p) or 'free', camera_name(rooms, best, p), *e))
    lines.append('busy 0x%X hold 0x%X focus 0x%X/0x%X window 0x%X  mode %d/%d  their look %d  Tome owner %d shares %d copy %d(p1 %d)  lent %d  cutscene %d menu %d' % (
        best['busy'], best['hold'], best['focus'], best['focus2'], best['window'], best['mode'], best['mode2'], best['screen'], best['owner'], best['shares'], best['copy'], best['copyp1'], best['lent'], best['cutscene'], best['menu']))
    if 'camhash' in best:
        lines.append('the game\'s camera records: index %s, %s' % (best.get('camidx'), game_camera(rooms, best)))
    if 'ticket' in best:
        lines.append('room change countdown %s, fade %s%s' % (best.get('ticket'), best.get('fade'), ', carried %s and %s' % (best['p1carry'], best['p2carry']) if 'p1carry' in best else ''))
    return lines, best


def shots(run):
    """The pictures run.sh took, by time: [(ms, path, left change, right change)]."""
    path = os.path.join(run, 'shots', 'shots.csv')
    if not os.path.exists(path):
        return []
    out = []
    for row in csv.DictReader(open(path)):
        out.append((int(row['ms']), os.path.join(run, 'shots', row['file']), float(row['left']), float(row['right'])))
    return out


def shot_at(run, rows, frame):
    s = shots(run)
    if not s:
        return None
    r = min(rows, key=lambda x: abs(x['frame'] - frame))
    best = min(s, key=lambda x: abs(x[0] - r['ms']))
    return best[1], best[0] - r['ms']


def check_pictures(run, rows, seconds=2.0):
    """A half whose picture stops while its player or camera moves. The watchdog counts frames, so it never sees this."""
    s = shots(run)
    out = []
    if len(s) < 4 or not rows:
        return out
    ms = [r['ms'] for r in rows]
    import bisect
    for side, idx, p in (('left', 2, 'p1'), ('right', 3, 'p2')):
        start = None
        for i, x in enumerate(s + [None]):
            still = x is not None and x[idx] < 0.005  # a walking player far off is still 0.06: a still picture is 0
            if still and start is None:
                start = i
            elif not still and start is not None:
                a, b = s[start], s[i - 1]
                if (b[0] - a[0]) / 1000 >= seconds:
                    ra = rows[min(bisect.bisect_left(ms, a[0]), len(rows) - 1)]
                    rb = rows[min(bisect.bisect_left(ms, b[0]), len(rows) - 1)]
                    if rb['frame'] - ra['frame'] > 30 and not rb['shares'] and not rb['menu'] and not rb['cutscene']:
                        moved = dist(pos(ra, p), pos(rb, p)) + dist(eye(ra, p), eye(rb, p))
                        if moved > 10:
                            out.append({'rule': 'PICTURE', 'who': p, 'from': ra['frame'], 'to': rb['frame'], 'text':
                                        "%s half's picture stood still %.1f s while the game went on, and they moved %.0f" % (side, (b[0] - a[0]) / 1000, moved)})
                start = None
    return out
