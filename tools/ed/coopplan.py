"""coopplan: one chapter's co-op check, as steps, from the kit's own tables (build/ed-db). No save state: the chapter is
reached by Jump to Game (tools/ed/jtg.sh), and every step is placed in frames after the frame both players are first
in control ("base"), so one plan fits any build. coopcheck.py renders it into ED_INPUT_SCRIPT, ED_PAD2_SCRIPT, ED_PUT
and (fallback only) ED_RUN_SCRIPT, and coopjudge.py checks it.

Nothing here is per room or chapter: the start room, its ways out, its examines, pickups and enemies all come from the
tables `ed build` made (doors_table, examines_table, rooms.json, placed_things), the same for every chapter.

A step is a dict:
  kind      walk | tome | examine | pickup | door | fight
  actor     p1 | p2 (walk: both)
  at, len   frames after base
  room      the room it is planned for (a step whose room isn't the one in play is BLOCKED, not failed)
  ...       per kind: region, script, centre; door: to_room, entry, camera, transition, motion, presses
and the inputs it needs, as offsets: p1 [(from, to, buttons, sx, sy)], p2 [(from, to, name)], puts [(at, who, x, y, z)],
scripts [(at, script)] (only for --door-mode script).
"""
import json
import math
import os
import re

import disc

FACTS = os.path.join(disc.DB, 'facts')

# Jump to Game's order (frontend.cpp kCharacters, hle.c kPlayerModel): name, level, the room the chapter starts in
# (tools/ed/sweep.py START_ROOM, from the chapter-start saves), the character's model (= its moves file charNNNN.cpk).
# Jump to Game's Peter starts level 10 and Edward level 9.
CHAPTERS = {1: ('Pius', 1, 341, 0), 2: ('Ellia', 2, 166, 70), 3: ('Anthony', 3, 150, 85), 4: ('Karim', 4, 338, 150),
            5: ('Max', 5, 249, 18), 6: ('Lindsey', 6, 303, 73), 7: ('Paul', 7, 103, 74), 8: ('Roberto', 8, 82, 76),
            9: ('Peter', 10, 27, 75), 10: ('Edward', 9, 237, 69), 11: ('Michael', 11, 50, 81), 12: ('Alex', 12, 100, 9)}
PLAYER_MODELS = {v[3] for v in CHAPTERS.values()}

EXIT_PROMPTS = {1, 3, 4, 10}        # OPEN DOOR, CLIMB UP, CLIMB DOWN, ENTER SECRET PASSAGE (coop_prompt_is_exit)
WORDS = re.compile(r'(?:^|[^a-z0-9_])(ed11|ed12|ed97)[(]', re.M)  # a script that opens words (chapters.py)

# Frames. A tick of the game is a frame (docs/game/systems/doors.md).
SETTLE = 120          # after base, before the first step
WALK = 360            # both walk, the four ways and back
TOME = 420            # one player's Tome open, a page, closed; the other walks
INTERACT = 900        # one examine or pickup start to finish, and the settle after (sweep.py STEP)
PRESS_GAP = 130       # between B presses in a close-up (sweep.py)
DOOR_BASE = 760       # put, a step, the presses, 62 ticks of fade, the load, about 2.1 s of arrival
WALK_AFTER = 300      # both walk after a door, cameras and the floor watched
FIGHT = 1200          # lock on, attack, the finisher's B
OTHER_GAP = 350       # the other player stands at least this far from any way out


def _load(name):
    path = os.path.join(FACTS, name) if not name.endswith('rooms.json') else os.path.join(disc.DB, name)
    if not os.path.exists(path):
        raise SystemExit('%s is missing: run `python -I tools/ed/ed.py build` once' % path)
    return json.load(open(path, encoding='utf8'))


_cache = {}


def table(name):
    if name not in _cache:
        _cache[name] = _load(name)
    return _cache[name]


def rooms():
    return table('rooms.json')


# ---- geometry -------------------------------------------------------------------------------------------------------

def region(room, name):
    """(lo, hi, shape) of a region by name, from rooms.json. Spheres as their box. Planes (shape 2) aren't zones a
    second player crosses (docs/game/systems/prompts.md), so they're left out (None)."""
    for r in rooms().get(str(room), {}).get('regions', []):
        if r['name'] != name:
            continue
        if r.get('shape') == 2:
            return None
        if 'min' in r:
            return list(r['min']), list(r['max']), r.get('shape')
        if 'pos' in r and 'radius' in r:
            p, rad = r['pos'], r['radius']
            return [p[0] - rad, p[1] - rad, p[2] - rad], [p[0] + rad, p[1] + rad, p[2] + rad], r.get('shape')
    return None


def centre(lo, hi):
    return [(a + b) / 2 for a, b in zip(lo, hi)]


def xy_dist_to_box(p, lo, hi):
    dx = max(lo[0] - p[0], 0, p[0] - hi[0])
    dy = max(lo[1] - p[1], 0, p[1] - hi[1])
    return math.hypot(dx, dy)


def markers(room, prefix=''):
    return [m for m in rooms().get(str(room), {}).get('markers', []) if m['name'].startswith(prefix)]


def marker(room, name):
    for m in markers(room):
        if m['name'] == name:
            return m
    return None


def has_gravity_marker(room):
    """A door's motion plays toward the nearest marker with flag 4; 85 rooms have none, and there it plays nothing."""
    return any(m.get('flags', 0) & 4 for m in markers(room))


def spot_in(lo, hi, toward, floor_z=None):
    """Inside a region, 40 in from its edge nearest `toward` (a sphere's or a pit's middle may be a hole: sweep.py),
    on its floor: ED_PUT drops it to the floor under it by the game's own collision."""
    c = [min(max(toward[k], lo[k] + 40), hi[k] - 40) if hi[k] - lo[k] > 80 else (lo[k] + hi[k]) / 2 for k in (0, 1)]
    z = lo[2] + 5 if floor_z is None else max(lo[2], floor_z) + 5
    return [c[0], c[1], z]


# ---- the chapter's tables -----------------------------------------------------------------------------------------

def _script_has_words(level, script):
    path = os.path.join(disc.DB, 'scripts', 'L%02d' % level, '%d.lua' % script)
    return os.path.exists(path) and WORDS.search(open(path, encoding='utf8', errors='replace').read()) is not None


def region_any(room, name):
    """A region of any shape: (lo, hi, shape), a plane as None for lo and hi (its place isn't read: sweep.py)."""
    for r in rooms().get(str(room), {}).get('regions', []):
        if r['name'] == name:
            if r.get('shape') == 2:
                return None, None, 2
            return region(room, name)
    return None


def doors_out(level, room, planes=True):
    """Every way out of a room that a region in it starts, the plainest first: an exit prompt or a zone exit, no words,
    fewest asks. From doors_table (what `ed door ROOM` prints), the level's own scripts and level 00's. A script that tests
    where the player came from (fn25) has several rooms: all are kept in to_rooms, and to_room is the first. A plane only
    the game's own player crosses (prompts.md), and its place isn't read, so a plane's door can only be run by script."""
    by = {}
    for d in table('doors_table.json'):
        if d['level'] not in ('%02d' % level, '00') or d.get('source') != 'region' or not isinstance(d.get('to_room'), int):
            continue
        if d.get('entry_in_to_room') is False or d['to_room'] == room:
            continue
        for f in d.get('from', []):
            if f.get('room') != room or f.get('field') not in ('enter', 'cross_a', 'cross_b'):
                continue
            box = region_any(room, f['region'])
            if box is None or (box[2] == 2 and not planes):
                continue
            k = (d['script'], f['region'])
            dest = dict(to_room=d['to_room'], entry=d.get('entry'), entry_hash=d.get('entry_hash'), camera=d.get('camera'),
                        camera_hash=d.get('camera_hash'), transition=d.get('transition'), motion=d.get('motion'),
                        after_script=d.get('after_script'))
            if k in by:
                if all(x['to_room'] != dest['to_room'] or x['entry'] != dest['entry'] for x in by[k]['to']):
                    by[k]['to'].append(dest)
                continue
            asks = d.get('asks_in_script') or []
            kind = 'plane' if box[2] == 2 else 'zone' if not asks else 'exit' if set(asks) <= EXIT_PROMPTS else 'asks'
            by[k] = dict(script=d['script'], script_level=int(d['level']), region=f['region'], lo=box[0], hi=box[1],
                         asks=asks, door_kind=kind, words=_script_has_words(int(d['level']), d['script']), to=[dest])
    rank = {'exit': 0, 'zone': 1, 'asks': 2, 'plane': 3}
    out = sorted(by.values(), key=lambda d: (rank[d['door_kind']], 1 if d['words'] else 0, len(d['asks']), len(d['to']), d['script']))
    return out


def destination(door, came_from=None):
    """Where a door is expected to go: back to where the player came from if the script has that room (its fn25 test),
    else its first."""
    for t in door['to']:
        if t['to_room'] == came_from:
            return t
    return door['to'][0]


def exit_boxes(level, room):
    return [(d['lo'], d['hi']) for d in doors_out(level, room) if d['lo'] is not None]


def interactions(level, room):
    """Examines and pickups started by walking into a region of the room (examines_table, as sweep_list.py), less any
    whose region overlaps a way out (walking onto it would take the door)."""
    exits = exit_boxes(level, room)
    out, seen = [], set()
    for e in table('examines_table.json'):
        if e['level'] != '%02d' % level:
            continue
        for f in e['from']:
            if f['room'] != room or f['field'] != 'enter':
                continue
            box = region(room, f['region'])
            if box is None:
                continue
            lo, hi = box[0], box[1]
            if (e['script'], f['region']) in seen:
                continue  # the same look listed twice
            seen.add((e['script'], f['region']))
            if any(lo[0] - 50 < h[0] and hi[0] + 50 > l[0] and lo[1] - 50 < h[1] and hi[1] + 50 > l[1] for l, h in exits):
                continue
            out.append(dict(kind='pickup' if e['native'] == 'ed221' else 'examine', native=e['native'], script=e['script'],
                            region=f['region'], lo=lo, hi=hi, words=e.get('label') or e.get('words')))
    # ed219 close-up DETAIL words RETURN: 4 presses; ed220 words: 2; ed221 close-up DETAIL words PICK UP find: 5 (sweep.py)
    for it in out:
        it['presses'] = {'ed219': 4, 'ed220': 2, 'ed221': 5}.get(it['native'], 4)
    out.sort(key=lambda it: (it['kind'] != 'pickup', it['script']))
    return out


def enemies(level, room):
    """Characters the level places in the room that aren't a playable model (placed_things section 0). Which of them
    can be fought isn't in the tables: inferred, so a fight that finds no one is SKIP, not FAIL."""
    return [t for t in table('placed_things.json').get(str(level), [])
            if t.get('section') == 0 and t.get('room') == room and t.get('chr') not in PLAYER_MODELS]


def aside(room, spot, avoid):
    """Where the other player stands: a marker or region middle of the room at least 300 from the actor's spot and as far
    as can be from every way out (they walk back and forth there, and must not take a door)."""
    cands = [m['pos'] for m in markers(room) if m['name'].startswith(('EntryMkr', 'GravityMkr', 'Puzzle_Mkr'))]
    for r in rooms().get(str(room), {}).get('regions', []):
        if 'min' in r and r.get('shape') != 2:
            cands.append(centre(r['min'], r['max']))
    best = None
    for c in cands:
        if math.hypot(c[0] - spot[0], c[1] - spot[1]) < 300:
            continue
        clear = min([xy_dist_to_box(c, lo, hi) for lo, hi in avoid] or [9999])
        if best is None or clear > best[0]:
            best = (clear, c)
    if best is None:
        return [spot[0] + 300, spot[1], spot[2]], 0
    return [best[1][0], best[1][1], best[1][2]], best[0]


# ---- inputs -----------------------------------------------------------------------------------------------------

B1, A1, START1, R1 = 0x0200, 0x0100, 0x1000, 0x0020
DIRS1 = ((0, 100), (100, 0), (0, -100), (-100, 0))
DIRS2 = ('lup', 'lright', 'ldown', 'lleft')
BACK2 = {'lup': 'ldown', 'ldown': 'lup', 'lleft': 'lright', 'lright': 'lleft'}


class Inputs:
    def __init__(self):
        self.p1, self.p2, self.puts, self.scripts = [], [], [], []

    def press(self, who, at, length, what):
        if who == 'p1':
            b, x, y = what if isinstance(what, tuple) else (what, 0, 0)
            self.p1.append((at, at + length, b, x, y))
        else:
            self.p2.append((at, at + length, what))

    def put(self, at, who, xyz):
        self.puts.append((at, 1 if who == 'p1' else 2, round(xyz[0]), round(xyz[1]), round(xyz[2])))

    def walk(self, who, at, frames, burst=45, rest=15):
        """Back and forth the four ways, so a wall stops only one and they end near where they began."""
        t, k = at, 0
        while t + 2 * burst + rest <= at + frames:
            if who == 'p1':
                x, y = DIRS1[k % 4]
                self.press('p1', t, burst, (0, x, y))
                self.press('p1', t + burst + rest, burst, (0, -x, -y))
            else:
                d = DIRS2[k % 4]
                self.press('p2', t, burst, d)
                self.press('p2', t + burst + rest, burst, BACK2[d])
            t += 2 * (burst + rest)
            k += 1

    def step_in_place(self, who, at):
        """Player 1's zones are looked at only as they move (player 2's each frame by the host): a small step, and back."""
        if who == 'p1':
            self.press('p1', at, 12, (0, 0, 40))
            self.press('p1', at + 14, 12, (0, 0, -40))
        else:
            self.press('p2', at, 8, 'lup')
            self.press('p2', at + 10, 8, 'ldown')

    def b(self, who, at):
        self.press(who, at, 5, B1 if who == 'p1' else 'b')


# ---- the plan -----------------------------------------------------------------------------------------------------

def other(p):
    return 'p2' if p == 'p1' else 'p1'


def build(chapter, quick=False, fight=None, door_mode='real', solo=False, max_each=3):
    """The steps for one chapter. quick: a walk, both Tomes, an examine and a pickup each where the first two rooms have
    them, a door by each. Full: up to max_each examines and pickups a player a room, and a fight by each where the tables
    place someone. solo: player 1's steps alone (mod_coop 0): the game's own one-player run, the reference."""
    name, level, start_room, model = CHAPTERS[chapter]
    fight = (not quick) if fight is None else fight
    steps, inp = [], Inputs()
    at = SETTLE
    actors = ('p1',) if solo else ('p1', 'p2')
    notes = []

    def add(step, length):
        nonlocal at
        step.update(at=at, len=length)
        steps.append(step)
        at += length

    # 1. both walk from where Jump to Game put them
    for p in actors:
        inp.walk(p, at, WALK)
    add(dict(kind='walk', actor='both', room=start_room), WALK)

    # 2. each player's Tome, the other walking
    for p in actors:
        inp.press(p, at, 4, START1 if p == 'p1' else 'start')
        inp.press(p, at + 110, 4, R1 if p == 'p1' else 'r')        # a page
        inp.press(p, at + 220, 4, START1 if p == 'p1' else 'start')  # closed
        if not solo:
            inp.walk(other(p), at + 20, TOME - 40)
        add(dict(kind='tome', actor=p, room=start_room), TOME)

    def interact(room, p, it):
        avoid = exit_boxes(level, room)
        toward = aside(room, centre(it['lo'], it['hi']), avoid)[0]
        spot = spot_in(it['lo'], it['hi'], toward, toward[2])
        far, clear = aside(room, spot, avoid)
        inp.put(at, p, spot)
        if not solo:
            inp.put(at, other(p), far)
        inp.step_in_place(p, at + 20)
        presses = [60 + PRESS_GAP * k for k in range(it['presses'])]
        for d in presses:
            inp.b(p, at + d)
        if not solo:
            inp.walk(other(p), at + 40, 660)
        # off the spot once done, so a press after can't open it again
        inp.put(at + presses[-1] + 120, p, [far[0] + 60, far[1] + 60, far[2]])
        add(dict(kind=it['kind'], actor=p, room=room, region=it['region'], script=it['script'], native=it['native'],
                 words=it['words'], spot=spot, other_at=far, other_clear=round(clear), presses=len(presses)), INTERACT)

    def interactions_in(room, order):
        items = interactions(level, room)
        picks = [i for i in items if i['kind'] == 'pickup']
        looks = [i for i in items if i['kind'] == 'examine']
        mine = {p: [] for p in order}
        done = lambda p, kind: quick and any(s['kind'] == kind and s['actor'] == p for s in steps)
        # Round by round, the players in turn, so a room's pickups are shared out between them (a pickup goes once).
        for _ in range(1 if quick else max_each):
            for p in order:
                if picks and not done(p, 'pickup'):
                    mine[p].append(picks.pop(0))
                if looks and not done(p, 'examine'):
                    mine[p].append(looks[0] if quick or len(looks) == 1 else looks.pop(0))  # an examine can be done again
        for p in order:
            for it in mine[p]:
                interact(room, p, it)

    def take_door(room, p, d, came_from):
        dest = destination(d, came_from)
        avoid = exit_boxes(level, room)
        mode = 'script' if d['door_kind'] == 'plane' else door_mode
        presses = 0 if d['door_kind'] in ('zone', 'plane') else 1 if d['door_kind'] == 'exit' else min(4, len(d['asks']) + 1)
        spot = None
        if mode == 'script':
            # The fallback: the door's own script from the host, twice for its zone and its press (chapters.py). It runs
            # as player 1 whoever is the actor, and isn't a real zone or press: COOP_PROMPT_RUNS and COOP_ZONE_RUNS paths
            # aren't exercised.
            inp.scripts.append((at + 40, d['script']))
            if d['asks']:
                inp.scripts.append((at + 100, d['script']))
            if p == 'p2':
                notes.append('room %d: door %d run by script, as player 1: player 2 owning a door is not tested by it' % (room, d['script']))
        else:
            mk = (markers(room, 'EntryMkr') or [{'pos': centre(d['lo'], d['hi'])}])[0]['pos']
            spot = spot_in(d['lo'], d['hi'], mk, mk[2])
            inp.put(at, p, spot)
            inp.step_in_place(p, at + 20)
            for k in range(presses):
                inp.b(p, at + 80 + PRESS_GAP * k)
        actor_at = spot or (markers(room, 'EntryMkr') or [{'pos': [0, 0, 0]}])[0]['pos']
        far, clear = aside(room, actor_at, avoid)
        if not solo:
            inp.put(at, other(p), far)
            inp.walk(other(p), at + 40, 200 + PRESS_GAP * presses)
        length = DOOR_BASE + PRESS_GAP * presses
        for q in actors:
            inp.walk(q, at + length, WALK_AFTER)
        add(dict(kind='door', actor=p, room=room, region=d['region'], script=d['script'], script_level=d['script_level'],
                 to_room=dest['to_room'], to_rooms=sorted({t['to_room'] for t in d['to']}), to=d['to'],
                 entry=dest['entry'], camera=dest['camera'], camera_hash=dest['camera_hash'], transition=dest['transition'],
                 motion=dest['motion'], door_kind=d['door_kind'], asks=d['asks'], presses=presses, mode=mode, spot=spot,
                 other_at=far, other_clear=round(clear),
                 motion_out=bool(dest['motion']) and has_gravity_marker(room),
                 motion_in=bool(dest['motion']) and has_gravity_marker(dest['to_room']),
                 walk_after=length), length + WALK_AFTER)
        return dest['to_room']

    def fight_in(room, p):
        foes = enemies(level, room)
        if not foes:
            return False
        f = foes[0]
        mk = (markers(room, 'EntryMkr') or [{'pos': [f['pos'][0] + 200, f['pos'][1], f['pos'][2]]}])[0]['pos']
        v = [mk[0] - f['pos'][0], mk[1] - f['pos'][1]]
        n = math.hypot(*v) or 1.0
        spot = [f['pos'][0] + v[0] / n * 140, f['pos'][1] + v[1] / n * 140, f['pos'][2]]
        far, _ = aside(room, spot, exit_boxes(level, room))
        inp.put(at, p, spot)
        if not solo:
            inp.put(at, other(p), far)
            inp.walk(other(p), at + 40, FIGHT - 80)
        # R held (lock on), A every 36 frames, B every 90 for FINISH HIM (button 2). Player 1's R is the digital bit only:
        # the input script has no analog trigger (hle.c input_script_apply).
        inp.press(p, at + 20, FIGHT - 200, R1 if p == 'p1' else 'r')
        for k in range(0, FIGHT - 260, 36):
            inp.press(p, at + 30 + k, 4, A1 if p == 'p1' else 'a')
        for k in range(0, FIGHT - 200, 90):
            inp.b(p, at + 50 + k)
        add(dict(kind='fight', actor=p, room=room, chr=f.get('chr'), foe_at=f['pos'], spot=spot), FIGHT)
        return True

    route = [start_room]
    interactions_in(start_room, actors)
    # The first door: the plainest whose room has a way out of its own, so the second player has a door to take.
    outs = sorted(doors_out(level, start_room), key=lambda d: not doors_out(level, destination(d)['to_room']))
    if outs:
        second = take_door(start_room, 'p1', outs[0], None)
        route.append(second)
        interactions_in(second, tuple(reversed(actors)))  # player 2 first here: a room's only pickup is theirs
        if fight:
            for p in actors:
                fight_in(second, p) or fight_in(start_room, p)
        outs2 = doors_out(level, second)
        back = [d for d in outs2 if any(t['to_room'] == start_room for t in d['to'])]
        if outs2:
            route.append(take_door(second, actors[-1], (back or outs2)[0], start_room))
        else:
            notes.append('room %d has no way out in the tables: no door by %s' % (second, actors[-1]))
    else:
        notes.append('room %d has no way out in the tables: no doors' % start_room)
        if fight:
            for p in actors:
                fight_in(start_room, p)
    for p in actors:
        for kind in ('examine', 'pickup', 'door', 'fight') if fight else ('examine', 'pickup', 'door'):
            if not any(s['kind'] == kind and s['actor'] == p for s in steps):
                notes.append("no %s by %s: the route's rooms have none in the tables" % (kind, p))

    return dict(chapter=chapter, name=name, level=level, start_room=start_room, model=model, quick=quick, solo=solo,
                door_mode=door_mode, route=route, steps=steps, end=at + 60, notes=notes,
                inputs=dict(p1=sorted(inp.p1), p2=sorted(inp.p2), puts=sorted(inp.puts), scripts=sorted(inp.scripts)))


# ---- rendering to the game's variables ----------------------------------------------------------------------------

P1_TEXT_MAX = 4096 - 300   # hle.c INPUT_SCRIPT_MAX_TEXT, less room for jtg.sh's own menu presses
P1_EVENTS_MAX = 256 - 16   # INPUT_SCRIPT_MAX_EVENTS
PUT_TEXT_MAX = 16384
SCRIPT_TEXT_MAX = 8192


def render(plan, base, first_row):
    """The environment for a launch whose players are in control at frame `base`. ED_INPUT_SCRIPT, ED_PAD2_SCRIPT and
    ED_PUT count the game's frames (timebase / 675000); ED_RUN_SCRIPT counts from the first frame the recorder wrote."""
    i = plan['inputs']
    p1 = ';'.join('%d,%d,%s,%d,%d' % (base + a, base + b, ('0x%X' % btn) if btn else '0', x, y) for a, b, btn, x, y in i['p1'])
    p2 = ';'.join('%d,%d,%s' % (base + a, base + b, what) for a, b, what in i['p2'])
    puts = ';'.join('%d,%d,%d,%d,%d' % (base + a, who, x, y, z) for a, who, x, y, z in i['puts'])
    scripts = ','.join('%d:%d' % (base + a - first_row, s) for a, s in i['scripts'])
    problems = []
    if len(p1) > P1_TEXT_MAX or len(i['p1']) > P1_EVENTS_MAX:
        problems.append('player 1\'s presses are %d characters, %d events: over what hle.c reads (%d, %d)' % (
            len(p1), len(i['p1']), P1_TEXT_MAX, P1_EVENTS_MAX))
    if len(puts) > PUT_TEXT_MAX:
        problems.append('ED_PUT is %d characters, over %d' % (len(puts), PUT_TEXT_MAX))
    if len(scripts) > SCRIPT_TEXT_MAX:
        problems.append('ED_RUN_SCRIPT is %d characters, over %d' % (len(scripts), SCRIPT_TEXT_MAX))
    env = dict(ED_INPUT_SCRIPT=p1, ED_PAD2_SCRIPT=p2, ED_PUT=puts)
    if scripts:
        env['ED_RUN_SCRIPT'] = scripts
    return env, problems


def describe(plan, base=None):
    lines = ['chapter %d %s (level %d), start room %d, route %s%s' % (
        plan['chapter'], plan['name'], plan['level'], plan['start_room'], ' -> '.join(str(r) for r in plan['route']),
        ', solo' if plan['solo'] else '')]
    for s in plan['steps']:
        extra = ''
        if s['kind'] == 'door':
            extra = 'L%02d/%d %s -> room %d%s at %s, camera %s, transition %s (motion %s), %s, %d press%s%s' % (
                s['script_level'], s['script'], s['region'], s['to_room'],
                '' if len(s['to_rooms']) == 1 else ' (or %s)' % '/'.join(str(r) for r in s['to_rooms'] if r != s['to_room']),
                s['entry'], s['camera'], s['transition'], s['motion'],
                s['door_kind'], s['presses'], '' if s['presses'] == 1 else 'es', ', by script' if s['mode'] == 'script' else '')
        elif s['kind'] in ('examine', 'pickup'):
            extra = '%s script %d %s (%s), %d presses' % (s['region'], s['script'], s['words'] or '', s['native'], s['presses'])
        elif s['kind'] == 'fight':
            extra = 'character %s placed at %s' % (s['chr'], s['foe_at'])
        lines.append('  +%-5d %-5d %-7s %-4s room %-4d %s' % (s['at'], s['len'], s['kind'], s['actor'], s['room'], extra))
    lines.append('  ends at +%d; %d presses for player 1, %d for player 2, %d puts, %d scripts' % (
        plan['end'], len(plan['inputs']['p1']), len(plan['inputs']['p2']), len(plan['inputs']['puts']), len(plan['inputs']['scripts'])))
    lines += ['  note: ' + n for n in plan['notes']]
    return lines
