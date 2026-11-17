"""The known co-op patterns as checks: what each looks like in a catalogue row or an `ed needs` answer, the fix that
already exists for it in runtime/host/hle.c (its function, line and commit), and how far a check from the disc data
can be trusted. `ed patterns` lists them with their counts in the catalogue.

From docs/coop-failed-attempts.md, docs/coop-todo.md, docs/coop-next-session.md ("The pattern behind many of these
bugs") and the kit-mapping prompt; checked against hle.c and git on 2026-11-16, then reviewed. A pattern here is where
a co-op bug of a known kind can be, not a bug.

Each pattern is counted in units: per script (a level's copy of one script) or per distinct call (the call and its
literal arguments), not per row, so a long script or a call repeated in a loop counts once. Each unit is covered (a
fix the kit can see applies), uncovered (no fix it can see), or n/a (whether it's covered is a run-time fact)."""
import functools
import os
import re
import subprocess

import disc

HLE = os.path.join(disc.ROOT, 'runtime', 'host', 'hle.c')

ROOM_WORDS = ('the room in play', 'the room slot table')
IDENTITY_WORDS = ("the player's object", 'current', 'main', 'asks for the character info', "reads the zones' inside words")
STATE_WORDS = ('the script words',)
LOOK_WORDS = ('the look being set up', 'the two looks on offer')
SUBTITLES = {'EDStartSubtitleForEdward', 'EDStartSubtitlesForAlexClockPuzzle', 'EDStartSubtitlesForGhostMaid',
             'EDStartSubtitlesForInsanityPhoneCall', 'EDStartSubtitlesForMikeDestroyThis', 'EDCleanUpSubtitles'}
ONE_GLOBAL_CALLS = {'ed56', 'ed68', 'fn111', 'fn207', 'fn21', 'fn139', 'fn91', 'ed219', 'ed220', 'ed221', 'ed301',
                    'fn113', 'fn211', 'ed277', 'fn26'} | SUBTITLES
FILLS_CALLS = {'ed88': 'the state keep', 'ed301': 'the six prompts', 'fn54': 'the sound streams', 'fn55': 'the sound streams',
               'fn114': 'the sound streams', 'fn126': 'the sound streams', 'fn148': 'the sound streams'}
CAMERA_CALLS = {'fn8', 'fn10', 'fn134'}
SCENE_KINDS = ('hold', 'input_mode', 'cinematic', 'film')
MAKER_SYSTEMS = ('objects', 'magick', 'sound', 'audio', 'enemies')
# A maker makes something (a lookup such as fn78 handle or ed24 nearest_class2 reads the room but makes nothing).
MAKER_NAMES = r'(?i)spawn|make|create|effect|explosion|flash|spark|flame|summon|emit|particle|projectile|Create'
REGION_FIELDS = ('enter', 'leave', 'stay', 'other_enter', 'other_leave', 'other_stay', 'cross_a', 'cross_b')
OWN_FIELDS = ('enter', 'leave', 'stay', 'cross_a', 'cross_b')  # run for the one who crosses; other_* for anyone else

# A fix: (symbol, commit, note). commit None: the first commit with the symbol (git log -S), right only when that's the
# commit that added this fix; give it when the symbol is older than the fix (rune_flame, coop_prompt_look).
PATTERNS = [
    dict(key='room', name='asks the room in play', unit='call / script',
         what='A call picks by the room in play (r13-23276) or the slot table within one call, or a region\'s script tests '
              'fn25 itself. Run for player 2 apart, it answers for player 1\'s room.',
         fix=[('coop_stay_room_in', None, None), ('coop_room_in', None, None), ('coop_dial_run', None, 'the clock')],
         example='the clock dials (L00/1551, ed56(8)): coop_dial_run runs the step and end with player 2\'s room in',
         trust='High for a script\'s own fn25 tests and for a compare within one call; covered means a live hook on the '
               'function holding the compare, or a room\'s arrival script (coop_run_script puts the room in).'),
    dict(key='identity', name='who "the player" is', unit='call',
         what='A call reads the one player\'s object or node (r13-26524, -21492, -21496, -21488, -21484) or the zones\' '
              'inside words within one call: it acts on player 1 unless player 2\'s are put in.',
         fix=[('coop_be_player2', None, None), ('coop_zones_swap', None, 'the zones\' inside words'),
              ('hle_object_model', None, 'what a chapter script is told the character is')],
         example='docs/coop-next-session.md "The pattern behind many of these bugs"',
         trust='Medium: a read within one call (not only a compare: these words are read and acted on); a pointer passed '
               'on further is not followed.'),
    dict(key='global', name='one global per game', unit='call',
         what='A call uses something the game keeps one of: the busy word, the input mode, the show words (fn111), the '
              'holds, the zones-on word, the looks on offer, the six prompts, the subtitles.',
         fix=[('coop_no_pause', None, 'the busy word'), ('coop_busy_ask', None, 'the busy word'),
              ('split_busy_allows', None, 'the busy word, the stayer\'s room'), ('coop_input_mode_start', None, 'the input mode'),
              ('coop_show_asked', None, 'the show words'), ('coop_hold_open', None, 'the holds'),
              ('g_coop_prompt_slots', None, 'player 2\'s six prompts'), ('coop_prompt_look', 'c3619bf', 'the six prompts'),
              ('coop_prompt_player1s', None, 'player 1\'s prompts in player 2\'s close-up'),
              ('coop_scan_player1', None, 'the zones-on word, the prompts')],
         example='subtitles have no fix yet (0x80308DE8 is one record; the keep fix only stops it being overwritten)',
         trust='High that the call uses it; uncovered when a word it reaches is one per game (no copy of player 2\'s) or '
               'it\'s a subtitle; whether two players really collide on it is a run-time fact.'),
    dict(key='state', name='one script state for both', unit='script',
         what='A script run from a region either player can enter keeps a state between runs (fn1 to a step above 0, or '
              'fn35): the second player meets the first one\'s step.',
         fix=[('coop_states_swap', None, None), ('StayerAgain', None, 'the stayer\'s door script')],
         example='the door script L01/119 (coop-failed-attempts.md)',
         trust='High that the script keeps a state; whether both players are in it at once is a run-time fact (n/a).'),
    dict(key='fills', name='a table player 2\'s things fill', unit='call',
         what='A call adds to a table of fixed size or no bound that solo never fills: the state keep (ed88), the six '
              'prompts, the sound streams.',
         fix=[('keep_filter', None, 'the keep'), ('keep_request_hold', None, 'the keep')],
         example='the Chapter Page keep overflow (74a7989): solo\'s keep already fills to 0x80308D88 in chapter 12',
         trust='Low: whether the table overflows is a run-time fact (n/a); this lists the calls that add to one.'),
    dict(key='stamped', name='stamped with the room in play when made', unit='call',
         what='A thing is made "in the room in play": ed3 with room -1, or a maker (objects, magick, sound) that reads the '
              'room in play within one call. Made for player 2 apart, it belongs to player 1\'s room.',
         fix=[('flame_keep', 'b42fb68', 'the rune sparks: coop_room_in around rune_flame in runes_frame')],
         example='player 2\'s rune sparks apart (coop-failed-attempts.md: probe a maker\'s calls before hooking it)',
         trust='High for ed3\'s room -1; a maker deeper than one call is not seen. Covered in a room\'s arrival script '
               '(its room is put in), uncovered elsewhere unless hooked.'),
    dict(key='later', name='queued for a later frame', unit='script',
         what='Work runs later than the call that asked for it, outside the bracket that made it player 2\'s: a wait '
              '(fn36), a cinematic\'s or film\'s end script (ed55, ed83), a play-start script (ed177), a script left for '
              'the Tome\'s end (ed9). (A script run next with fn73 is not later: it runs inside the same run, 0x80164488.)',
         fix=[('coop_story_ticket', None, 'a film\'s or cinematic\'s next script'), ('g_story_bring', None, 'the bring'),
              ('coop_script_timers', None, 'a script\'s wait'), ('coop_p2_script_mark', None, 'a script player 2 started'),
              ('g_stay_room_script', None, 'a room\'s script queued by player 2\'s change')],
         example='the dream left player 2 behind (coop_story_ticket); the hanged body in room 81 (g_stay_room_script)',
         trust='Partly: ed55 / ed83 end scripts are covered (coop_story_ticket), fn36 by the script timers, ed177 and ed9 '
               'have no fix the kit sees; when they run and as whose is a run-time fact.'),
    dict(key='change', name='run inside player 2\'s change', unit='script',
         what='A room\'s arrival or leave script runs as part of a room change, and a door\'s after-script after it: in '
              'player 2\'s change it must run as theirs, with their room in.',
         fix=[('notify_ticket_run', None, 'the R4 bracket'), ('g_stay_room_script', None, 'the arrival script queued')],
         example='the R4 bracket (notify_ticket_run, 9957a4f); room 81\'s body (f8e5d82)',
         trust='Covered for the script\'s own rows and the scripts it runs next with fn73 (they run inside the same run, '
               '0x80164488); a wait (fn36) resumes later through the script timers (coop_script_timers); a door\'s '
               'after-script is n/a (it\'s not followed here).'),
    dict(key='hero', name='only for the chapter\'s character (a hero gate)', unit='script',
         what='A script keeps the player\'s handle (fn78(1, 0), r13-21496) and tests it against 1, the chapter\'s character: '
              'run as player 2\'s (their handle, e.g. 161), it turns them away. 44 of the 73 also pass that handle on '
              '(holds, reloads, walks, look-ats).',
         fix=[('coop_be_player2', None, 'makes player 2 the player for their runs, so the handle is theirs: the gate then fails')],
         example='Edward\'s lever L09/1951 (room 273): player 2 is turned away at `if L0 ~= 1` (logs/run-20261117-010635-duck-together, '
                 '`ed stops`)',
         trust='High: the test is in the script. No fix yet: the handle has to mean player 2\'s body inside their runs (an '
               'identity change, docs/research/kit-bugs/plans.md).'),
    dict(key='camera', name='drawn from one camera', unit='script',
         what='A script selects, saves or restores the camera set (fn8, fn10, fn134) and also holds play, plays a scene or '
              'starts an input mode: the set has one owner.',
         fix=[('coop_cameras_in', None, 'the camera set\'s owner'), ('coop_cameras_lend', None, 'lent to player 2'),
              ('fx_facing_begin', None, 'effects face each half\'s camera')],
         example='room 183\'s scenes (coop-todo.md: one owner for the camera set); smoke over the braziers (3542bf0)',
         trust='High for script cameras; who owns the set at that moment is a run-time fact (n/a). Camera reads at draw '
               'time go through callbacks a static walk can\'t follow.'),
]
RUN_TIME_ONLY = ('held buttons reaching the world as new presses (coop-failed-attempts.md: the stayer\'s kept exit B); '
                 'host state a save state doesn\'t carry (states saved apart: coop_state_loaded); one render resource per '
                 'frame (the fade quad: g_fade_mover; the shared vertex buffer\'s flip, r13-21824)')
BY_KEY = {p['key']: p for p in PATTERNS}


@functools.lru_cache(None)
def fix_where(symbol, commit=None):
    """(hle.c line of its definition, or of its first mention marked so; the commit; that commit's subject)."""
    text = open(HLE, encoding='utf8', errors='replace').read().split('\n')
    line, how = None, ''
    defn = re.compile(r'^(static\s+)?[\w\s\*]*\b%s\b\s*(\([^;]*\)\s*\{|\s*\[[^;]*;|\s*=)|^\}\s*%s\s*;' % (re.escape(symbol), re.escape(symbol)))
    for i, l in enumerate(text, 1):
        if defn.match(l):
            line = i
            break
    if line is None:
        for i, l in enumerate(text, 1):
            if re.search(r'\b%s\b' % re.escape(symbol), l):
                line, how = i, ' (first mention)'
                break
    if commit is None:
        r = subprocess.run(['git', '-C', disc.ROOT, 'log', '-S', symbol, '--reverse', '--format=%h', '--', 'runtime/host/hle.c'],
                           capture_output=True, text=True, errors='replace')
        commit = (r.stdout.strip().split('\n') or [''])[0]
    r = subprocess.run(['git', '-C', disc.ROOT, 'log', '-1', '--format=%s', commit], capture_output=True, text=True, errors='replace')
    return ('%s%s' % (line, how)) if line else '?', commit, r.stdout.strip()


def fix_text(p):
    return ', '.join('%s (hle.c:%s, %s%s)' % (s, fix_where(s, c)[0], fix_where(s, c)[1] or 'no commit found',
                                              ('; ' + note) if note else '') for s, c, note in p['fix'])


def _base(word):
    return word.split(' (in mode')[0]


def for_row(x, words, where, prims):
    """The patterns a catalogue row shows: [(key, cover, unit)] with cover 'covered' / 'uncovered' / 'n/a' and unit the
    thing counted (('call', name, args) or ('script',)). x the row, words its co-op words (Reader.coop_row), where its
    sources, prims the primitives table."""
    out = []
    name = x.get('call') or ''
    args = tuple(x.get('args') or [])
    call_unit = ('call', name, args)
    # A script run next with fn73 runs inside the same run (0x80164470 reads r13-23220 as the script returns and runs it
    # at once, 0x80164488; hle.c's coop_run_script: "a script it ran on to with fn73, run inside it"), so its rows (via)
    # keep the bracket of the script that ran it on.
    arrival_row = any(w.startswith('the room\'s arrival') for w in where)
    fields = [w.rsplit(' ', 1)[-1] for w in where if not w.startswith('the room\'s ')]
    region_script = any(f in REGION_FIELDS for f in fields)
    own_field = any(f in OWN_FIELDS for f in fields) or any(w.startswith('the room\'s ') for w in where)
    near = [w for w in words if w['depth'] <= 1]

    def bracketed(hooked):
        """The cover of a word the bracket around player 2's own runs puts in (the room, the player, the zones'
        words, the script words): covered by a live hook or in a room's arrival script (coop_run_script puts the
        room in); 'bracket' when the script runs from a region's own fields (player 2's run goes through their turn:
        coop_use_start); uncovered when it runs only from other_* fields (for anyone but the one who crosses)."""
        if hooked or arrival_row:
            return 'covered'
        if not own_field:
            return 'uncovered'
        return 'bracket'

    p = prims.get(name, {})
    maker = p.get('system') in MAKER_SYSTEMS and re.search(MAKER_NAMES, p.get('short', '') + ' ' + name)
    stamped = name == 'ed3' and len(args) > 2 and args[2] == '-1' or (
        maker and any(_base(w['word']) in ROOM_WORDS for w in near))
    ident = [w for w in near if _base(w['word']) in IDENTITY_WORDS]
    if ident:
        out.append(('identity', bracketed(all(w['status'] in ('hooked', 'keyed', 'per run') for w in ident)), call_unit))
    if stamped:
        out.append(('stamped', bracketed(any(w['status'] == 'hooked' for w in near if _base(w['word']) in ROOM_WORDS)), call_unit))
    else:
        rooms = [w for w in words if w['picks'] and _base(w['word']) in ROOM_WORDS]
        if rooms:
            out.append(('room', bracketed(not any(w['flagged'] for w in rooms)), call_unit))
        elif x['kind'] == 'room_test' and region_script:
            out.append(('room', bracketed(False), ('script',)))
        elif x['kind'] == 'room_test':
            out.append(('room', 'covered' if arrival_row else bracketed(False), ('script',)))
    if name in ONE_GLOBAL_CALLS or any(w['picks'] and _base(w['word']) in LOOK_WORDS for w in words):
        # Each global has its fix (the busy word, the input mode, the show words, the holds, the prompts, the zones-on
        # word: see the fix list) but two: the looks on offer (one copy, never swapped) and the subtitles (no fix).
        look = name in ('ed219', 'ed220', 'ed221') or any(_base(w['word']) in LOOK_WORDS and w['status'] == 'one per game' for w in near)
        unit = ('call', name, args[:1]) if name in ('ed56', 'ed68') else ('call', name, ())
        out.append(('global', 'uncovered' if look or name in SUBTITLES else 'covered', unit))
    if name in ('fn1', 'fn35') and region_script and (name == 'fn35' or (args and args[0] not in ('0',))):
        out.append(('state', 'n/a', ('script',)))
    if name in FILLS_CALLS:
        out.append(('fills', 'n/a', ('call', FILLS_CALLS[name], ())))
    if name in ('fn36', 'ed9', 'ed55', 'ed83') or (name == 'ed177' and args and args[0] not in ('-1', '0')):
        # ed177(-1) only clears the word; ed177(script) leaves a script for play mode's next start (0x80008234), which
        # hle.c doesn't handle (no r13-24248): run as whoever is the player then.
        out.append(('later', 'covered' if name in ('ed55', 'ed83', 'fn36') else 'uncovered', ('script',)))
    if any(w.startswith('the room\'s ') for w in where):
        out.append(('change', 'covered', ('script',)))  # a script it runs next with fn73 runs inside the same run
    if name == 'fn66' and len(args) > 4 and args[4] not in ('0', '-1', 'nil'):
        out.append(('change', 'n/a', ('call', 'door after-script', (args[4],))))
    if name in CAMERA_CALLS:
        out.append(('camera', 'n/a', ('script',)))  # kept only for scripts that also hold, play a scene or a mode
    return out


HERO_VAR = re.compile(r'(?:local\s+)?(\w+)\s*=\s*(?:handle\[fn78\]|fn78)\(1, 0\)')


def hero_gate(body):
    """The line numbers (1-based) where a script tests the player's handle against 1, or []."""
    if not body:
        return []
    names = set(HERO_VAR.findall(body))
    out = []
    for i, line in enumerate(body.split('\n'), 1):
        if re.search(r'\bif (?:%s) (?:~=|==) 1 then' % '|'.join(map(re.escape, names)), line) if names else False:
            out.append(i)
        elif re.search(r'\bif (?:handle\[fn78\]|fn78)\(1, 0\) (?:~=|==) 1 then', line):
            out.append(i)
    return out


def keep_camera(rows):
    """A script's camera tags count only when it also holds play, plays a scene or starts an input mode."""
    return any(r['kind'] in SCENE_KINDS for r in rows)


def for_words(found):
    """The word-based patterns an `ed needs` answer shows: {key: [words]} over its found rows (within one call)."""
    out = {}
    for (where, what, helper), row in found.items():
        word = re.split(r'[.:;(]', what)[0].strip()
        picks = row.get('cmp') is not None and row['cmp'] <= 1
        if word in IDENTITY_WORDS and row['depth'] <= 1:
            out.setdefault('identity', []).append(word)
        if not picks:
            continue
        for key, words in (('room', ROOM_WORDS), ('state', STATE_WORDS), ('global', LOOK_WORDS)):
            if word in words:
                out.setdefault(key, []).append(word)
    return out


def cmd_patterns(argv):
    """ed patterns [KEY]: each known co-op pattern, its fix in hle.c, how far the check can be trusted, its catalogue
    counts (units, covered, uncovered, n/a) and the uncovered units most often met."""
    import json
    path = os.path.join(disc.ROOT, 'docs', 'game', 'catalogue', 'patterns.json')
    counts = json.load(open(path, encoding='utf8')) if os.path.exists(path) else {}
    for p in PATTERNS:
        if argv and p['key'] not in argv:
            continue
        c = counts.get(p['key'], {})
        print('%s: %s (counted per %s)' % (p['key'], p['name'], p['unit']))
        print('  %s' % p['what'])
        print('  fix: %s' % fix_text(p))
        print('  e.g. %s' % p['example'])
        print('  trust: %s' % p['trust'])
        if c:
            print('  catalogue: %d units: %d covered, %d bracket (covered when player 2\'s run is in their bracket), %d '
                  'uncovered, %d n/a' % (c['units'], c['covered'], c.get('bracket', 0), c['uncovered'], c['n/a']))
            for u in c.get('top_uncovered', [])[:8 if not argv else 40]:
                print('    uncovered: %s' % u)
        else:
            print('  catalogue: not counted yet (`ed catalogue`)')
    print('run-time only, no check from the disc: %s' % RUN_TIME_ONLY)
