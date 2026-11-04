#!/usr/bin/env python3
"""ed: the whole game for Claude, from the disc and the code. Run with `python -I tools/ed/ed.py COMMAND`, from the repo.

Start with these, before writing anything:
  how THING             what the game does for a normal player with it: words, where, prompt, script, camera, door
  needs ADDR|fnN        the one-player data a game function reaches, and the host helper that puts player 2's in
  host WORD...          what this project has already built for it: functions, docs, commits
  keep                  what the user already has working as they like it: extend these, never rebuild them

The game, not a room:
  build [step]          (once, and after the disc or dol change) make build/ed-db
  flicker [RUN]         each player's shadows and picture, frame by frame, from a run with ED_RUN_PROBE=1
  lamps [RUN...]        the lamps each room was seen to have, from a run with AURORA_SHADOW_LOG
  graphics [SAVE] [OPT] every graphics option at every value: what it did to the picture, frame rate and log
  system [NAME]         a system's page: what it is everywhere, every way in, every rule. No NAME lists them
  script ID [--level N] a script, readable: functions named, words and cameras filled in. No level: which levels have it
  room N [--level N]    a room's cameras, regions with what each runs, doors, entries and markers doors use
  camera NAME|HASH [--room N]  where a camera is. With a room: its keys, what names it, from which regions
  where ROOM X Y Z      which regions a place is inside, what they run, and the room's nearest cameras
  tag N                 a placed thing by its tag: which levels, rooms and items, and which scripts look for it
  door [ROOM]           every way in and out of a room: from where, prompt, entry, camera, what its transition does
  transition [N]        what each door transition number does: motion, fade, ticks, what it looks like
  item TEXT|ID          an item by name, label or type id: words, Use/Check/Mix scripts, ammo, where placed, its examine
  mix [TEXT]            every Mix: pieces, ammo classes, and what the game says when it refuses
  verb [NAME]           everything a player can do (use, equip, check, examine, pick up, door...): the function to call,
                        arguments, gates, which player's data it reads, what the host has
  examine ROOM|TEXT     every Examine: words, camera, what it offers
  level [N]             each level: who, start room, model, scripts
  who [ADDR|WORD]       which player pair each function reads (current or main)
  carried SAVE          who carries what in a save: handles, type ids, names
  text TEXT             any of the game's words: which entry, and which scripts open them
  prompts [TEXT]        every prompt: who asks for it, what pressing it runs
  uses NAME [--level N] the scripts that call a function
  fn NAME|ADDR          all that's known of a function: script name, callers, hle.c lines, docs, commits
  dis ADDR [COUNT]      the game's code at an address

A run, both players at once (a run comes from tools/ed/run.sh):
  check [RUN]           the whole-game rules on a run: who froze, camera jumps, falls, stalls, still pictures
  timeline [RUN] [FROM TO]  both players side by side, a line each time anything changes
  see FRAME [RUN]       one frame: both players' full state, and its picture's path
  sheet RUN F1 F2 ...   a contact sheet of frames, captioned with both players, to look at in one read
  diff RUN_A RUN_B      what changed between two runs of the same thing

Guarding fixes:
  test [NAME] [--list]  the regression scenarios (tools/ed/scenarios): every system, each player. A fix isn't done until this passes
"""
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disc
import gameinfo
import recording
from gameinfo import cmd_carried, cmd_door, cmd_examine, cmd_item, cmd_level, cmd_mix, cmd_prompts, cmd_tag, cmd_transition, cmd_verb, cmd_where, cmd_who

DB = disc.DB
HERE = os.path.dirname(os.path.abspath(__file__))
SYSTEMS = os.path.join(disc.ROOT, 'docs', 'game', 'systems')


def need(name):
    path = os.path.join(DB, name)
    if not os.path.exists(path):
        raise SystemExit('run `python -I tools/ed/ed.py build` first (no %s)' % name)
    return path


def jload(name):
    return json.load(open(need(name), encoding='utf8'))


def prims():
    out = {}
    for line in open(os.path.join(HERE, 'primitives.tsv'), encoding='utf8'):
        if line.startswith('#') or not line.strip():
            continue
        p = line.rstrip('\n').split('\t')
        out[p[0]] = {'short': p[1], 'system': p[2], 'sure': p[3], 'why': p[4], 'args': p[5] if len(p) > 5 else ''}
    return out


def natives():
    out = {}
    for line in open(need('natives.tsv')):
        p = line.rstrip('\n').split('\t')
        out[p[0]] = {'addr': p[1], 'symbol': p[2]}
    return out


def hash_names():
    return gameinfo.hash_names()


def args_flag(argv, name, default=None):
    if name in argv:
        i = argv.index(name)
        v = argv[i + 1]
        del argv[i:i + 2]
        return v
    return default


def script_levels(sid):
    return sorted(jload('scripts.json').get(str(sid), {}), key=int)


def read_script(sid, level):
    path = os.path.join(DB, 'scripts', 'L%s' % level, '%d.lua' % sid)
    return open(path, encoding='utf8').read() if os.path.exists(path) else None


def annotate(text, level, P, H, T):
    """Functions by name, hashes by name, and the words a words call opens."""
    room = T.get('room' + str(level), {})
    boot = T['boot']

    def fn_name(m):
        p = P.get(m.group(0))
        return '%s[%s]' % (p['short'], m.group(0)) if p else m.group(0)

    out = []
    for line in text.split('\n'):
        note = []
        for m in re.finditer(r'\b(\d{7,10})\b', line):
            h = H.get(int(m.group(1)))
            if h:
                note.append('%s=%s' % (m.group(1), h['name']))
        m = re.search(r'\bed11\((\d+), (\d+)', line)
        if m:
            row = boot.get('%s,%s' % (m.group(1), m.group(2)))
            if row:
                note.append('"%s"' % row[2][:110])
        m = re.search(r'\bed(?:12|219|220|221)\((\d+), (\d+)', line)
        if m:
            row = room.get('%s,%s' % (m.group(1), m.group(2)))
            if row:
                note.append('%s "%s"' % (row[1], row[2][:110]))
        line = re.sub(r'\b(fn|ed)\d+\b', fn_name, line)
        out.append(line + ('    -- ' + '; '.join(note) if note else ''))
    return '\n'.join(out)


def cmd_script(argv):
    level = args_flag(argv, '--level')
    sid = int(argv[0])
    levels = script_levels(sid)
    if not levels:
        return print('script %d is in no level' % sid)
    print('script %d is in levels: %s' % (sid, ' '.join(levels)))
    bodies = {}
    for lv in levels:
        bodies.setdefault(read_script(sid, lv), []).append(lv)
    print('%d different version(s)%s' % (len(bodies), ': the same everywhere, so this script is the whole game\'s' if len(bodies) == 1 and len(levels) > 1 else ''))
    P, H, T = prims(), hash_names(), jload('text.json')
    for body, lvs in bodies.items():
        if level and level not in lvs and level.zfill(2) not in lvs:
            continue
        print('--- levels %s' % ' '.join(lvs))
        print(annotate(body.rstrip(), lvs[0], P, H, T))


def cmd_room(argv):
    level = args_flag(argv, '--level')
    if not argv or not argv[0].isdigit():
        raise SystemExit('ed room N [--level N]')
    rid = argv[0]
    r = jload('rooms.json').get(str(int(rid)))
    if not r:
        return print('no room', rid)
    print('room %s (Rooms/rmed%04d.bpe)' % (rid, int(rid)))
    print('CAMERAS (all of them, from the room\'s own array. A moving camera is only at this place on its first frame)')
    for c in r['cameras']:
        print('  %-14s hash %-10d at %6d %6d %6d  looks at %6d %6d %6d  fov %.1f%s' % (c['name'], c['hash'], *c['pos'], *c.get('target_pos', (0, 0, 0)), c['fov'], ('  (moves on a path)' if c.get('path') else '') + ('  (its target moves)' if c.get('target_path') else '')))
    index = jload('scripts.json')
    H = hash_names()
    print('REGIONS (enter/leave/stay: what runs; other: for anyone but player 1; a plane runs by which way it\'s crossed)')
    SH = {0: 'sphere', 1: 'box', 2: 'plane'}
    for x in r['regions']:
        runs = ['%s %d' % (k, x[k]) for k in ('enter', 'leave', 'stay', 'other_enter', 'other_leave', 'other_stay', 'cross_a', 'cross_b') if x.get(k, -1) > 0]
        extra = ''
        for k in ('enter', 'leave', 'stay', 'cross_a', 'cross_b'):
            sid = x.get(k, -1)
            if sid <= 0:
                continue
            levels = sorted(index.get(str(sid), {}), key=int)
            lv = level.zfill(2) if level and level.zfill(2) in levels else (levels[0] if levels else None)
            body = read_script(sid, lv) if lv else None
            if body:
                cams = sorted({H[int(m)]['name'] for m in re.findall(r'\bfn(?:8|10)\((\d+)', body) if int(m) in H})
                door = re.findall(r'\bfn66\((\d+), (\d+), (\d+), (\d+)', body)
                if re.search(r'\bfn66\([A-Za-z]', body):
                    extra += '  %s door to a variable room' % k
                if cams:
                    extra += '  %s cameras %s' % (k, ','.join(cams))
                if door:
                    extra += '  %s door to %s' % (k, ','.join(sorted({'%s/%s transition %s' % (d[0], H.get(int(d[1]), {}).get('name', d[1]), d[3]) for d in door})[:3]))
                if re.search(r'\b(ed219|ed220|ed221)\(', body):
                    extra += '  %s examine' % k
        where = 'min %s max %s' % (x['min'], x['max']) if 'min' in x else ('at %s radius %s' % (x['pos'], x.get('radius')) if 'pos' in x else 'where: not read')
        print('  %-20s %-6s +110 %-3s %-30s %s%s' % (x['name'], SH.get(x.get('shape'), '?'), x.get('p110'), ' '.join(runs) or '-', where, extra))
    kinds = {}
    landed = {d['entry_hash'] for d in gameinfo.fact('doors_table.json') if d['to_room'] == int(rid)}
    for m in r['markers']:
        kinds.setdefault(re.sub(r'\d+.*$', '', m['name']), []).append(m)
    print('MARKERS')
    for k, v in sorted(kinds.items()):
        if k.startswith(('Entry', 'Axis', 'Look', 'Exit', 'Gravity')):
            for m in v:
                print('  %-16s hash %-10d at %s  flags 0x%X' % (m['name'], m['hash'], m['pos'], m.get('flags', 0)))
        else:
            print('  %s: %d' % (k, len(v)))
            for m in v:
                if m['hash'] in landed:
                    print('  %-16s hash %-10d at %s  a door lands here' % (m['name'], m['hash'], m['pos']))


def cmd_camera(argv):
    room = args_flag(argv, '--room')
    if not argv:
        raise SystemExit('ed camera NAME|HASH [--room N]: where it is. With a room: its keys and the scripts that name it')
    q = argv[0].strip()
    rooms = jload('rooms.json')
    paths = recording.camera_paths()
    want = int(q, 0) if re.fullmatch(r'(0x)?[0-9a-fA-F]+', q) and (q.startswith('0x') or q.isdigit()) else None
    n = 0
    for rid, r in sorted(rooms.items(), key=lambda kv: int(kv[0])):
        if room and rid != str(int(room)):
            continue
        for c in r['cameras']:
            if (want is not None and c['hash'] == want) or (want is None and c['name'].strip().lower() == q.lower()):
                n += 1
                print('room %-5s %-12s hash %-10d at %6d %6d %6d  looks at %6d %6d %6d  fov %.1f%s%s' % (rid, c['name'], c['hash'], *c['pos'], *c.get('target_pos', (0, 0, 0)), c['fov'], '  moves' if c.get('path') else '', '  target moves' if c.get('target_path') else ''))
                if room:
                    k = paths.get(rid, {}).get(str(c['hash']), {})
                    for key, what in (('keys', 'eye'), ('target_keys', 'target')):
                        rows = k.get(key, [])
                        if rows:
                            step = max(1, len(rows) // 12)
                            print('  %s, %d keys, frame x y z: %s' % (what, len(rows), '  '.join('%d: %d %d %d' % tuple(x) for x in rows[::step])))
                    hits = _scripts_with(r'\b%d\b' % c['hash'])
                    calls = sorted({m for h in hits for m in re.findall(r'\b(\w+)\([^()\n]*\b%d\b' % c['hash'], all_scripts()[h])})
                    print('  named by: %s' % (' '.join(calls) or 'nothing'))
                    ids = {x.get(f, -1) for x in r['regions'] for f in ('enter', 'leave', 'stay', 'cross_a', 'cross_b', 'other_enter', 'other_leave', 'other_stay')}
                    local = sorted({int(h.split('/')[1]) for h in hits} & ids)
                    print('  run from this room\'s own regions and naming it: %s' % (' '.join(str(x) for x in local) or 'none'))
                    print('  all scripts naming this hash, which a room\'s name can share with other rooms: %s%s' % (' '.join(hits[:30]) or 'none', ' +%d more' % (len(hits) - 30) if len(hits) > 30 else ''))
    if not n:
        print('no camera %s%s' % (q, ' in room ' + room if room else ''))



_bodies = None


def all_scripts():
    global _bodies
    if _bodies is None:
        _bodies = {}
        root = os.path.join(DB, 'scripts')
        for lv in sorted(os.listdir(root)):
            for f in os.listdir(os.path.join(root, lv)):
                _bodies['%s/%s' % (lv, f[:-4])] = open(os.path.join(root, lv, f), encoding='utf8').read()
    return _bodies


def _scripts_with(pattern):
    r = re.compile(pattern)
    return [k for k, v in all_scripts().items() if r.search(v)]


def cmd_text(argv):
    q = ' '.join(argv).lower()
    if not q.strip():
        raise SystemExit('ed text WORDS: any of the game\'s words, by words or label. NPC talk (ed97) is in another pack, not read yet')
    T = jload('text.json')
    n = 0
    for pack, rows in sorted(T.items()):
        for key, (group, label, text) in rows.items():
            if q in text.lower() or q in label.lower():
                n += 1
                g, e = key.split(',')
                if pack == 'prompts':
                    how = 'a prompt: `ed prompts %s`' % label if g == '0' else 'a hint made by the Tome\'s pages: docs/game/research/verbs.md'
                else:
                    call = 'ed11' if pack == 'boot' else 'ed(?:12|219|220|221)'
                    hits = _scripts_with(r'\b%s\(%s, %s\b' % (call, g, e))
                    if pack != 'boot':
                        hits = [h for h in hits if h.startswith('L' + pack[4:] + '/')]
                    how = ' '.join(hits[:10]) + (' +%d more' % (len(hits) - 10) if len(hits) > 10 else '')
                    if not hits:
                        how = 'an item row names it: `ed item %s`' % label.rsplit('_', 1)[0] if pack == 'boot' and label.endswith(('_PICKUP', '_DESC', '_INV')) else 'none found (may be by a variable)'
                print('%s %s %s | %s\n   %s\n   opened by: %s' % (pack, key, group, label, text[:300], how))
    if not n:
        print('No words match. NPC talk (ed97, r13-26480) is in another pack, not read yet.')


def cmd_uses(argv):
    level = args_flag(argv, '--level')
    if not argv:
        raise SystemExit('ed uses NAME [--level N]: NAME is fn8, ed221, or a short name such as camera_set')
    name = argv[0]
    names = [name]
    if name not in natives():
        names = [k for k, v in prims().items() if v['short'] == name]
        if not names:
            return print('No function named %s: `ed fn NAME` or tools/ed/primitives.tsv.' % name)
    for n in names:
        hits = _scripts_with(r'\b%s\(' % re.escape(n))
        if level:
            hits = [h for h in hits if h.split('/')[0] == 'L' + level.zfill(2) or h.split('/')[0] == 'L' + level]
        print('%d scripts call %s%s%s' % (len(hits), n, ' (%s)' % name if n != name else '', ' in level %s' % level if level else ''))
        print(' '.join(hits[:200]))


def grep_lines(path, pattern, limit=12):
    out = []
    r = re.compile(pattern, re.I)
    for n, line in enumerate(open(path, encoding='utf8', errors='replace'), 1):
        if r.search(line):
            out.append('%s:%d: %s' % (os.path.relpath(path, disc.ROOT).replace('\\', '/'), n, line.strip()[:170]))
            if len(out) >= limit:
                break
    return out


def cmd_fn(argv):
    import know
    if not argv:
        raise SystemExit('ed fn NAME|ADDR: all that\'s known of a function')
    N = natives()
    P = prims()
    at, name = know.resolve(argv[0])
    addr = '%08X' % at
    syms = disc.symbols()
    print('function 0x%s  decomp name: %s' % (addr, syms.get(int(addr, 16), '(none)')))
    if name:
        p = P.get(name)
        print('script name: %s%s' % (name, '  = %s (%s, %s): %s' % (p['short'], p['system'], p['sure'], p['why']) if p else '  (not in tools/ed/primitives.tsv yet)'))
        print('script files calling it, over all levels: %d' % len(_scripts_with(r'\b%s\(' % re.escape(name))))
    calls = jload('calls.json').get(addr, [])
    print('bl sites in the game\'s code: %d%s %s' % (len(calls), ' (0 is normal for a script function or a switch case: they\'re reached by a table)' if not calls else ':', ' '.join(calls[:14])))
    pat = '0x%s' % addr
    print('--- host code')
    for path in ('runtime/host/hle.c', 'runtime/host/camera.c', 'runtime/host/controls.cpp'):
        for line in grep_lines(os.path.join(disc.ROOT, path), pat, 10):
            print(line)
    print('--- docs')
    for root, _, files in os.walk(os.path.join(disc.ROOT, 'docs')):
        for f in files:
            if f.endswith('.md'):
                for line in grep_lines(os.path.join(root, f), pat, 4):
                    print(line)
    print('--- commits')
    r = subprocess.run(['git', '-C', disc.ROOT, 'log', '--oneline', '-i', '--grep', pat], capture_output=True, text=True)
    print(r.stdout.strip()[:700] or '(none name it)')


def cmd_dis(argv):
    import know
    if not argv:
        raise SystemExit('ed dis ADDR|NAME [COUNT]')
    a = '%08X' % know.resolve(argv[0])[0]
    n = int(argv[1]) if len(argv) > 1 else 40
    if n < 1:
        raise SystemExit('ed dis ADDR [COUNT]: COUNT above 0')
    syms = disc.symbols()
    show = 0
    for line in open(need('disasm.txt')):
        if show == 0 and line[:8] >= a:
            show = n
        if show:
            at = int(line[:8], 16)
            if at in syms:
                print('--- %s' % syms[at])
            print(line.rstrip())
            show -= 1
            if not show:
                break


def cmd_system(argv):
    if not argv:
        print('The game has one of each of these. Read its page before changing anything that touches it:')
        for f in sorted(os.listdir(SYSTEMS)):
            first = open(os.path.join(SYSTEMS, f), encoding='utf8').readline().strip('# \n')
            print('  %-14s %s' % (f[:-3], first))
        return
    path = os.path.join(SYSTEMS, argv[0] + '.md')
    if not os.path.exists(path):
        raise SystemExit('no page %s. `ed system` lists them' % argv[0])
    print(open(path, encoding='utf8').read())


def cmd_check(argv):
    run = recording.find_run(argv[0] if argv else None)
    rows = recording.load(run)
    print('%s: %d frames (%s to %s)' % (os.path.basename(run), len(rows), rows[0]['frame'] if rows else '-', rows[-1]['frame'] if rows else '-'))
    found = recording.check(rows) + recording.check_pictures(run, rows)
    hard = [f for f in found if f['rule'] != 'STUCK?']
    for f in found:
        print('%-7s %s %s  %s' % (f['rule'], str(f.get('from', '')), str(f.get('to', '')), f['text']))
    print('%d finding(s), %d maybe. Look with: ed see FRAME %s' % (len(hard), len(found) - len(hard), os.path.basename(run)))
    return hard


def cmd_timeline(argv):
    run = None
    nums = []
    for a in argv:
        if a.isdigit():
            nums.append(int(a))
        else:
            run = a
    run = recording.find_run(run)
    rows = recording.load(run)
    print(os.path.basename(run))
    for line in recording.timeline(rows, nums[0] if nums else None, nums[1] if len(nums) > 1 else None):
        print(line)


def cmd_see(argv):
    frame = int(argv[0])
    run = recording.find_run(argv[1] if len(argv) > 1 else None)
    rows = recording.load(run)
    lines, best = recording.at_frame(rows, frame)
    print('\n'.join(lines))
    if best['script'] > 0:
        print('script %d: `ed script %d`' % (best['script'], best['script']))
    pic = recording.shot_at(run, rows, frame)
    if pic:
        print('picture: %s (%+d ms from that frame). Left half is player 1, right is player 2.' % (pic[0].replace('\\', '/'), pic[1]))


def cmd_sheet(argv):
    run = recording.find_run(argv[0])
    rows = recording.load(run)
    lines = []
    frames = [int(a) for a in argv[1:]]
    rooms = recording.room_cameras()
    for f in frames:
        pic = recording.shot_at(run, rows, f)
        if not pic:
            raise SystemExit('this run has no pictures')
        r = min(rows, key=lambda x: abs(x['frame'] - f))
        cap = 'F%d  P1 %.0f,%.0f %s cam %s | P2 %.0f,%.0f %s cam %s | script %d%s' % (
            r['frame'], r['p1x'], r['p1y'], recording.activity(r, 'p1') or 'free', recording.camera_name(rooms, r, 'p1'),
            r['p2x'], r['p2y'], recording.activity(r, 'p2') or 'free', recording.camera_name(rooms, r, 'p2'), r['script'], ' words' if r['window'] else '')
        lines.append('%s\t%s' % (os.path.abspath(pic[0]), cap))
    listing = os.path.join(run, 'sheet.txt')
    open(listing, 'w', encoding='utf8').write('\n'.join(lines))
    out = os.path.abspath(os.path.join(run, 'sheet-%d-%d.jpg' % (frames[0], frames[-1])))
    subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', os.path.join(HERE, 'sheet.ps1'), '-list', os.path.abspath(listing), '-out', out], check=True, capture_output=True)
    print(out.replace('\\', '/'))


def cmd_diff(argv):
    a, b = recording.find_run(argv[0]), recording.find_run(argv[1])
    ra, rb = recording.load(a), recording.load(b)
    fa = {(f['rule'], f.get('who')) for f in recording.check(ra)}
    fb = recording.check(rb)
    print('%s: %d frames, %d findings\n%s: %d frames, %d findings' % (os.path.basename(a), len(ra), len(recording.check(ra)), os.path.basename(b), len(rb), len(fb)))
    new = [f for f in fb if (f['rule'], f.get('who')) not in fa and f['rule'] != 'STUCK?']
    print('New in the second:')
    for f in new:
        print('  %-7s %s %s  %s' % (f['rule'], f.get('from', ''), f.get('to', ''), f['text']))
    if not new:
        print('  none')


def cmd_test(argv):
    import scenarios
    return scenarios.main(argv)


def cmd_flicker(argv):
    import gfxcheck
    gfxcheck.cmd_flicker(argv)


def cmd_lamps(argv):
    import gfxcheck
    gfxcheck.cmd_lamps(argv)


def cmd_graphics(argv):
    import gfxcheck
    gfxcheck.cmd_graphics(argv)


def cmd_how(argv):
    import know
    know.cmd_how(argv)


def cmd_needs(argv):
    import know
    know.cmd_needs(argv)


def cmd_host(argv):
    import know
    know.cmd_host(argv)


def cmd_keep(argv):
    import know
    know.cmd_keep(argv)


def main():
    # -I ignores PYTHONIOENCODING, and the game's words and some pages aren't in the console's code page.
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding='utf8', errors='replace')
        except AttributeError:
            pass
    argv = sys.argv[1:]
    if not argv or argv[0] in ('-h', '--help', 'help'):
        return print(__doc__)
    cmd, rest = argv[0], argv[1:]
    if cmd == 'build':
        import build_db
        return build_db.build(rest or None)
    fn = globals().get('cmd_' + cmd)
    if not fn:
        raise SystemExit('no command %s\n%s' % (cmd, __doc__))
    try:
        result = fn(rest)
    except (ValueError, IndexError, KeyError) as e:
        line = next((l.strip() for l in __doc__.split('\n') if l.strip().startswith(cmd + ' ')), cmd)
        raise SystemExit('ed %s: bad or missing argument (%s). Usage: %s' % (cmd, e, line))
    if cmd == 'test' and result:
        sys.exit(1)


if __name__ == '__main__':
    try:
        main()
        sys.stdout.flush()
    except (BrokenPipeError, OSError) as e:  # piped to head: Windows gives errno 22
        if not isinstance(e, BrokenPipeError) and e.errno != 22:
            raise
        os.dup2(os.open(os.devnull, os.O_WRONLY), sys.stdout.fileno())
