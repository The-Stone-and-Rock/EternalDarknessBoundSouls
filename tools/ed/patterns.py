"""The known co-op patterns as checks: what each looks like in a catalogue row or an `ed needs` answer, the fix that
already exists for it in runtime/host/hle.c (its function, line and the commit that added that function), and how
far a check from the disc data can be trusted. `ed patterns` lists them with their counts in the catalogue.

From docs/coop-failed-attempts.md, docs/coop-todo.md, docs/coop-next-session.md ("The pattern behind many of these
bugs") and the kit-mapping prompt; checked against hle.c and git on 2026-11-16. A pattern here is where a co-op bug
of a known kind can be, not a bug: the column says whether a hook covers it where the kit can see one."""
import functools
import os
import re
import subprocess

import disc

HLE = os.path.join(disc.ROOT, 'runtime', 'host', 'hle.c')

# Words of tools/ed/player_data.tsv, by their 'what' text's start (the catalogue's short word).
ROOM_WORDS = ('the room in play', 'the room slot table')
IDENTITY_WORDS = ("the player's object", 'current', 'main')
STATE_WORDS = ('the script words',)
LOOK_WORDS = ('the look being set up', 'the two looks on offer')
ONE_GLOBAL_CALLS = {'ed56', 'ed68', 'fn111', 'fn207', 'fn21', 'fn139', 'fn91', 'ed219', 'ed220', 'ed221', 'ed301',
                    'fn113', 'fn211', 'ed277', 'EDStartSubtitleForEdward', 'EDStartSubtitlesForAlexClockPuzzle',
                    'EDStartSubtitlesForGhostMaid', 'EDStartSubtitlesForInsanityPhoneCall',
                    'EDStartSubtitlesForMikeDestroyThis', 'EDCleanUpSubtitles', 'fn26'}
FILLS_CALLS = {'ed88', 'ed301', 'fn54', 'fn55', 'fn114', 'fn126', 'fn148'}
QUEUED_CALLS = {'fn73', 'fn36', 'ed177', 'ed9', 'ed55', 'ed83'}
CAMERA_CALLS = {'fn8', 'fn10', 'fn134'}

PATTERNS = [
    dict(key='room', name='asks the room in play',
         what='A call or a script picks by the room in play (r13-23276, fn25) or the slot table. Run for player 2 apart, '
              'it answers for player 1\'s room.',
         fix=['coop_stay_room_in', 'coop_room_in', 'coop_dial_run'],
         example='the clock dials (L00/1551, ed56(8)): coop_dial_run runs the step and end with player 2\'s room in',
         trust='High for a script\'s own fn25 tests; for a call, the compare must be within one call (`ed needs`).'),
    dict(key='identity', name='who "the player" is',
         what='A call picks by the one player\'s object or node (r13-26524, -21492, -21496, -21488, -21484).',
         fix=['coop_be_player2'],
         example='docs/coop-next-session.md "The pattern behind many of these bugs"',
         trust='High where `ed needs` sees the compare; a pointer passed on is not followed.'),
    dict(key='global', name='one global per game',
         what='A call uses something the game keeps one of: the busy word, the input mode, the show words (fn111), '
              'the holds, the zones-on word, the looks on offer, the six prompts, the subtitles.',
         fix=['coop_no_pause', 'coop_busy_ask', 'coop_input_mode_start', 'coop_show_asked', 'coop_hold_open',
              'coop_prompt_player1s'],
         example='subtitles have no fix yet (0x80308DE8 is one record; the keep fix only stops it being overwritten)',
         trust='High that the call uses it; whether two players really collide on it is a run-time fact.'),
    dict(key='state', name='one script state for both',
         what='A script with a state machine (fn1 / fn0, the script words at 0x805CAA40) runs from a region both players '
              'can enter: the second player meets the first one\'s step.',
         fix=['coop_states_swap', 'StayerAgain'],
         example='the door script L01/119 (coop-failed-attempts.md)',
         trust='High: the script and its region are on the disc.'),
    dict(key='fills', name='a table player 2\'s things fill',
         what='A call adds to a table of fixed size or no bound that solo never fills: the state keep (ed88), the six '
              'prompts, the sound tracks.',
         fix=['keep_filter', 'keep_request_hold'],
         example='the Chapter Page keep overflow (74a7989): solo\'s keep already fills to 0x80308D88 in chapter 12',
         trust='Low: whether the table overflows is a run-time fact; this only lists the calls that add to one.'),
    dict(key='stamped', name='stamped with the room in play when made',
         what='A thing is made "in the room in play" (ed3 with room -1, effects): made for player 2 apart, it belongs '
              'to player 1\'s room.',
         fix=['rune_flame'],
         example='player 2\'s rune sparks apart (coop-failed-attempts.md: probe a maker\'s calls before hooking it)',
         trust='High for ed3\'s room -1; effects made deeper in a call are only seen through `ed needs`.'),
    dict(key='later', name='queued for a later frame',
         what='Work runs later than the call that asked for it, outside the bracket that made it player 2\'s: a script '
              'run next (fn73), a wait, a cinematic\'s or film\'s end script, a room\'s arrival script.',
         fix=['coop_story_ticket', 'g_stay_room_script'],
         example='the dream left player 2 behind (film and cinematic next scripts: coop_story_ticket); the hanged body '
                 'in room 81 (the arrival script queued by player 2\'s change: notify_script_later, coop_run_script)',
         trust='Partly: the queueing call is on the disc; when it runs and in whose context is a run-time fact.'),
    dict(key='change', name='run inside player 2\'s change',
         what='A room\'s arrival or leave script, or a door\'s after-script, runs as part of a room change: in player 2\'s '
              'change it must run as theirs, with their room in.',
         fix=['notify_ticket_run', 'g_stay_room_script'],
         example='the R4 bracket (notify_ticket_run, 9957a4f); room 81\'s body (f8e5d82)',
         trust='Static: it flags "runs on arrival"; whether the bracket is in force is a run-time fact.'),
    dict(key='camera', name='drawn from one camera',
         what='A script selects, saves or restores the camera set (fn8, fn10, fn134), or a look opens a close-up camera: '
              'the set has one owner.',
         fix=['coop_cameras_in', 'coop_cameras_lend', 'fx_facing_begin'],
         example='room 183\'s scenes (coop-todo.md: one owner for the camera set); smoke over the braziers (3542bf0)',
         trust='High for script cameras; camera reads at draw time go through callbacks a static walk can\'t follow.'),
]
RUN_TIME_ONLY = ('held buttons reaching the world as new presses (coop-failed-attempts.md: the stayer\'s kept exit B); '
                 'host state a save state doesn\'t carry (states saved apart, coop_state_loaded); one render resource '
                 'per frame (the shared vertex buffer, the fade quad)')


@functools.lru_cache(None)
def fix_where(symbol):
    """(hle.c line of its definition, the commit that added it, that commit's subject) for a fix's function or global."""
    text = open(HLE, encoding='utf8', errors='replace').read().split('\n')
    line = None
    for i, l in enumerate(text, 1):
        if re.match(r'^(static\s+)?[\w\s\*]*\b%s\b\s*(\([^;]*\)\s*\{|\s*=|\s*\[|;)' % re.escape(symbol), l) and '{' in l or \
                re.match(r'^(static\s+)?[\w\s\*]+\b%s\b(\s*=|\s*\[|;)' % re.escape(symbol), l):
            line = i
            break
    if line is None:
        for i, l in enumerate(text, 1):
            if re.search(r'\b%s\b' % re.escape(symbol), l):
                line = i
                break
    r = subprocess.run(['git', '-C', disc.ROOT, 'log', '-S', symbol, '--reverse', '--format=%h %s', '--', 'runtime/host/hle.c'],
                       capture_output=True, text=True, errors='replace')
    first = (r.stdout.strip().split('\n') or [''])[0]
    commit, _, subject = first.partition(' ')
    return line, commit, subject


def for_row(x, words, where):
    """The patterns a catalogue row shows: x the row, words its co-op words (Reader.coop_row), where its sources."""
    out = []
    picks = {w['word'].split(' (in mode')[0] for w in words if w['picks']}
    flagged = {w['word'].split(' (in mode')[0] for w in words if w['flagged']}
    name = x.get('call') or ''
    if picks & set(ROOM_WORDS) or x['kind'] == 'room_test':
        out.append(('room', bool(flagged & set(ROOM_WORDS))))
    if picks & set(IDENTITY_WORDS):
        out.append(('identity', bool(flagged & set(IDENTITY_WORDS))))
    if name in ONE_GLOBAL_CALLS or picks & set(LOOK_WORDS):
        out.append(('global', bool(flagged & set(LOOK_WORDS)) or name.startswith('EDStartSubtitle')))
    if x['kind'] == 'state' and any(' ' in w and not w.startswith('the room\'s') for w in where):
        out.append(('state', bool(flagged & set(STATE_WORDS))))
    if name in FILLS_CALLS:
        out.append(('fills', False))
    if name == 'ed3' and len(x.get('args') or []) > 2 and x['args'][2] == '-1':
        out.append(('stamped', bool(flagged & set(ROOM_WORDS))))
    if name in QUEUED_CALLS or (name == 'fn66' and len(x.get('args') or []) > 4 and x['args'][4] not in ('0', '-1')):
        out.append(('later', False))
    if any(w.startswith('the room\'s ') for w in where):
        out.append(('change', bool(flagged & set(ROOM_WORDS))))
    if name in CAMERA_CALLS or (name in ('ed219', 'ed221') and len(x.get('args') or []) > 2):
        out.append(('camera', False))
    return out


def for_words(found):
    """The word-based patterns an `ed needs` answer shows: {key: [words]} over its found rows."""
    out = {}
    for (where, what, helper), row in found.items():
        word = re.split(r'[.:;(]', what)[0].strip()
        picks = row.get('cmp') is not None and row['cmp'] <= 1
        if not picks:
            continue
        for key, words in (('room', ROOM_WORDS), ('identity', IDENTITY_WORDS), ('state', STATE_WORDS), ('global', LOOK_WORDS)):
            if word in words:
                out.setdefault(key, []).append(word)
    return out


def fix_text(p):
    bits = []
    for s in p['fix']:
        line, commit, _ = fix_where(s)
        bits.append('%s (hle.c:%s, %s)' % (s, line or '?', commit or 'no commit found'))
    return ', '.join(bits)


def cmd_patterns(argv):
    """ed patterns: each known co-op pattern, its fix in hle.c, how far the check can be trusted, its catalogue counts."""
    import csv
    import glob
    counts = {p['key']: [0, 0] for p in PATTERNS}
    for f in glob.glob(os.path.join(disc.ROOT, 'docs', 'game', 'catalogue', 'level-*.tsv')):
        for r in csv.DictReader(open(f, encoding='utf8'), delimiter='\t'):
            for tag in (r.get('patterns') or '').split('; '):
                if not tag:
                    continue
                key, _, flag = tag.partition(' ')
                if key in counts:
                    counts[key][0] += 1
                    counts[key][1] += flag == '(uncovered)'
    for p in PATTERNS:
        n, u = counts[p['key']]
        print('%s: %s' % (p['key'], p['name']))
        print('  %s' % p['what'])
        print('  fix: %s' % fix_text(p))
        print('  e.g. %s' % p['example'])
        print('  trust: %s' % p['trust'])
        print('  catalogue: %d rows, %d of them flagged uncovered' % (n, u))
    print('run-time only, no check from the disc: %s' % RUN_TIME_ONLY)
