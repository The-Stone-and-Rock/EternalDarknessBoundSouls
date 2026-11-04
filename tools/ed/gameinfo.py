"""The whole game's tables, from build/ed-db/facts (made by `ed build facts` from tools/ed/facts). Items and their ids, Use and
Mix, every verb, door transitions, every door and examine, levels, and which player each function reads. The evidence for all
of it is in docs/game/research."""
import json
import os
import re

import disc

DB = disc.DB
FACTS = os.path.join(DB, 'facts')
HERE = os.path.dirname(os.path.abspath(__file__))
_cache = {}
# The codes some names hold unexpanded. The meanings are inferred from labels (ULY, XEL, MANT), and only help searching.
ALIGN = {'~c': "chattur'gha", '~u': 'ulyaoth', '~x': "xel'lotath", '~m': 'mantorok'}


def fact(name):
    if name not in _cache:
        path = os.path.join(FACTS, name)
        if not os.path.exists(path):
            raise SystemExit('run `python -I tools/ed/ed.py build facts` first (no %s)' % name)
        _cache[name] = json.load(open(path, encoding='utf8'))
    return _cache[name]


def db(name):
    if name not in _cache:
        _cache[name] = json.load(open(os.path.join(DB, name), encoding='utf8'))
    return _cache[name]


def hash_names():
    """Hash -> every name it has, by room. Six marker hashes are shared by two different names."""
    if 'hash_index' not in _cache:
        out = {}
        for rid, r in sorted(db('rooms.json').items(), key=lambda kv: int(kv[0])):
            for kind in ('cameras', 'regions', 'markers'):
                for x in r[kind]:
                    row = out.setdefault(x['hash'], {'names': {}, 'kind': kind, 'rooms': []})
                    row['names'].setdefault(x['name'].strip(), []).append(int(rid))
                    row['rooms'].append(int(rid))
        for row in out.values():
            row['name'] = ' or '.join(sorted(row['names']))
        _cache['hash_index'] = out
    return _cache['hash_index']


def hash_name(h, room=None):
    row = hash_names().get(h)
    if not row:
        return str(h)
    if room is not None:
        for name, rooms in row['names'].items():
            if room in rooms:
                return name
    return row['name']


def show(v):
    return '-' if v in (None, 0, -1, '', []) else str(v)


def boot_words(group, entry):
    row = db('text.json')['boot'].get('%s,%s' % (group, entry))
    return (row[1], row[2]) if row else ('', '')


def body(level, sid):
    path = os.path.join(DB, 'scripts', 'L' + level, '%s.lua' % sid)
    return open(path, encoding='utf8').read() if os.path.exists(path) else ''


def more(rows, n):
    return ['+%d more' % (len(rows) - n)] if len(rows) > n else []


# ---- items


def item_blob(r):
    raw = ' '.join(str(r.get(k) or '') for k in ('name', 'label', 'pickup_words')).lower()
    blob = raw
    for code, name in ALIGN.items():
        blob = blob.replace(code, name)
    return (raw + ' ' + blob + ' ' + blob.replace("'", ''))


def item_rows(q):
    rows = fact('item_table.json')['rows']
    if re.fullmatch(r'\d+', q):
        i = int(q)
        return [rows[i]] if i < len(rows) else []
    q = q.lower()
    return [r for r in rows if q in item_blob(r) or q.replace("'", '') in item_blob(r)]


def script_levels_of(row):
    t = {r['id']: r for r in fact('item_script_levels.json')}
    return t.get(row['id'], {}).get('scripts', {})


def examines_of(row):
    """The ed221 calls offering this item. A script is often in levels that don't place it: those are left out, and counted."""
    tags = set(row.get('tags') or [])
    real, copies = [], []
    for e in fact('examines_table.json'):
        if e['native'] == 'ed221' and len(e['args']) > 4 and e['args'][4] in tags:
            if row['id'] in (e.get('type_ids') or []) or row['id'] in (e.get('spawned_types_with_tag') or []):
                real.append(e)
            else:
                copies.append(e)
    return real, copies


def says_of(row):
    """The words a Use or Mix script can put up: ed11(group, entry, ...), refusals among them."""
    out = []
    for key in ('use_script', 'mix_script'):
        sid = row.get(key)
        if not sid:
            continue
        for level, _ in sorted(db('scripts.json').get(str(sid), {}).items()):
            text = body(level, sid)
            for g, e in re.findall(r'\bed11\((\d+), (\d+)', text):
                label, words = boot_words(g, e)
                line = 'script %s: boot %s,%s %s "%s"' % (sid, g, e, label, words[:80])
                if line not in out:
                    out.append(line)
            room = db('text.json').get('room' + level, {})
            for g, e in re.findall(r'\bed12\((\d+), (\d+)', text):
                row = room.get('%s,%s' % (g, e))
                line = 'script %s: room text %s,%s %s "%s"' % (sid, g, e, row[1] if row else '', row[2][:80] if row else '')
                if line not in out:
                    out.append(line)
            break
    return out


def print_item(r):
    t = r.get('text') or {}
    print('%d  %s  (%s, %s)%s' % (r['id'], r.get('name') if len(r.get('name') or '') < 70 else (r['name'][:70] + '...'), r.get('label'), r.get('kind'), '  same text entry for all three words' if r.get('placeholder') else ''))
    print('   words   pickup %s,%s "%s"' % (t.get('group'), t.get('pickup'), (r.get('pickup_words') or '')[:90]))
    print('           check  %s,%s "%s"' % (t.get('group'), t.get('description'), (r.get('description') or '')[:90]))
    scr = script_levels_of(r)
    for key, what in (('use_script', 'USE'), ('after_examine_script', 'CHECK'), ('mix_script', 'MIX'), ('after_find_script', 'AFTER FIND')):
        v = r.get(key)
        if v:
            lv = scr.get(key, {}).get('levels')
            print('   %-7s script %d%s' % (what, v, '  only in levels ' + ' '.join(lv) + ': silently nothing in others' if lv else ''))
    for line in says_of(r):
        print('   says    ' + line)
    rec = r.get('record') or {}
    fire = r.get('fire_mode')
    if r.get('count') or r.get('capacity') or rec.get('count_mode16') or rec.get('capacity_mode16'):
        line = 'count %s, capacity %s' % (r.get('count'), r.get('capacity'))
        if rec.get('count_mode16') or rec.get('capacity_mode16'):
            line += '; in mode 16 (+74, +72): count %s, capacity %s%s' % (rec.get('count_mode16'), rec.get('capacity_mode16'), ', the one in use: this row starts in mode 16' if fire == 16 else '')
        print('   count   ' + line)
    kinds = fact('item_table.json')['inv_kinds']
    modes = 0
    for k in r.get('inv_kinds') or []:
        x = kinds.get(str(k)) if isinstance(kinds, dict) else (kinds[k] if k < len(kinds) else None)
        if x:
            modes |= x.get('fire_modes') or 0
    if modes:
        print('   mode    starts at %s, template modes 0x%X%s' % (fire, modes, ', MODE offered' if bin(modes).count('1') > 1 else ', one mode, so MODE is not offered'))
    if rec.get('flags'):
        print('   flags   0x%X' % rec['flags'])
    if r.get('reload_partners'):
        rows = fact('item_table.json')['rows']
        print('   reload  ammo class %s, fits %s' % (' '.join('0x%X' % m for m in r.get('reload_masks') or []),
                                                     ', '.join('%d %s' % (x, rows[x]['name']) for x in r['reload_partners'])))
    elif r.get('mix_script') and r.get('reload_partners') is None:
        print('   reload  class unknown: no placing or literal ed3 ties this row to a template')
    placed = r.get('placed') or []
    seen = {}
    for p in placed:
        seen.setdefault((p['file'], p['table'], p['index']), []).append(p)
    for group in seen.values():
        p = group[0]
        sets = ['set %s model %s' % (x.get('set'), x['model']) for x in group if x.get('set') is not None]
        print('   placed  %s room %s at %s, tag %s, model %s, chr %s%s' % (p['file'], p['room'], p['pos'], p['tag'] if p.get('tag') is not None else '-', p['model'], p['chr'], ', by alignment (1 Chattur\'gha, 2 Ulyaoth, 3 Xel\'lotath): ' + ', '.join(sets) if sets else ''))
    if not placed and not r.get('spawned_by'):
        print('   placed  no placing or literal ed3 found')
    for s in r.get('spawned_by') or []:
        print('   spawned script %s, chr %s, model %s, tag %s' % (s.get('script'), s.get('chr'), s.get('model'), s.get('tag')) if isinstance(s, dict) else '   spawned %s' % s)
    real, copies = examines_of(r)
    for e in real:
        rooms = ', '.join('room %s %s' % (f['room'], f['region']) for f in e['from'][:3])
        a = e['args']
        cam = hash_name(a[2]) if isinstance(a[2], int) else a[2]
        print('   examine L%s/%d ed221(%s, %s, %s, %s, %s, %s) from %s' % (e['level'], e['script'], a[0], a[1], cam, a[3], a[4], a[5], rooms or 'no region found'))
    if copies:
        print('   (%d more ed221 with this tag are copies of a script in levels that don\'t place it: nothing asks there)' % len(copies))
    for s in r.get('scripts_testing_type') or []:
        print('   tested  %s' % (s if isinstance(s, str) else json.dumps(s)[:120]))


def cmd_item(argv):
    if not argv:
        raise SystemExit('ed item TEXT|TYPE_ID: by name, label or type id. `ed item --list` for all 244.')
    if argv[0] == '--list':
        for r in fact('item_table.json')['rows']:
            print('%3d  %-34s use %-5s check %-5s mix %-5s find %-5s %s' % (r['id'], (r.get('name') or '')[:34], show(r.get('use_script')), show(r.get('after_examine_script')),
                                                                         show(r.get('mix_script')), show(r.get('after_find_script')), r.get('label')))
        return
    q = ' '.join(argv)
    rows = item_rows(q)
    print('Type id is the row in InvU.bin, the same in every play. A number in a log or ed20 is a handle, which lasts one play: `ed carried SAVE HANDLE`.' if rows else '')
    for r in rows:
        print_item(r)
    names = {r.get('name') for r in rows}
    if len(rows) > 1 and len(names) == 1:
        print('%d rows share this name: key on the type id.' % len(rows))
    if rows:
        print('These are starting values: scripts rewrite Use, Check and words with fn162. A placed thing by its tag: `ed tag N`.')
    print('%d item(s), of 244.' % len(rows) if rows else 'No such item. Ids are 0 to 243. Names with a ~ code can be found by label or the god\'s name.')


def cmd_mix(argv):
    m = fact('mix_table.json')
    rows = fact('item_table.json')['rows']
    q = ' '.join(argv).lower()
    print('There is no recipe table. Mix runs the FIRST pick\'s +42 script, which compares fn92 types (pieces) or calls ed140 (ammo). The two items are ed20(0) and ed20(1). A script missing from the loaded level does nothing, silently.\n')
    print('PIECES (a pair is listed only where its first pick has this script)')
    shown = False
    for sid, s in sorted(m['pieces'].items(), key=lambda kv: int(kv[0])):
        lines = []
        for p in s.get('pairs_that_work', []):
            if rows[p['first']].get('mix_script') != int(sid):
                continue
            made = ', '.join('%s %s' % (x['kind'], x['name']) for x in p.get('makes', [])) or ('runs ' + ', '.join(str(x) for x in p.get('runs', [])) if p.get('runs') else 'nothing recognised')
            lines.append('  %3d %-26s + %3d %-26s -> %s' % (p['first'], p['first_name'][:26], p['second'], p['second_name'][:26], made))
        blob = (' '.join(lines) + ' ' + sid + ' ' + ' '.join(s.get('kinds_named', {}).values())).lower()
        if q and q not in blob:
            continue
        shown = True
        print('script %s, level %s%s' % (sid, ' '.join(s['levels']), '' if s.get('reachable_by_mix_action', True) else '  (no item\'s +42 names it, so nothing is known to run it)'))
        print('\n'.join(lines) if lines else '  no pairs recognised: `ed script %s`' % sid)
        for w in s.get('pairs_that_fail_words', []):
            label, words = boot_words(w[0], w[1])
            print('  a wrong pair says: boot %s,%s %s "%s"' % (w[0], w[1], label, words[:70]))
        for k, src in sorted(s.get('kind_sources', {}).items(), key=lambda kv: int(kv[0])):
            if not src.get('ed3_literal') and not src.get('npcs_tables_3_5'):
                print('  no placing or literal ed3 found for type %s %s' % (k, rows[int(k)]['name']))
    if q and not shown:
        print('  none match')
    print('\nAMMO: fits when the classes share a bit (template +28). Scripts 1582, 2013, 2221 call ed140(first pick, second pick), and the Assault Rifle takes 0x400 too: Mode 16 rewrites its class')
    for cls, v in sorted(m['ammo_by_class'].items(), key=lambda kv: int(kv[0], 16)):
        weapons = list(v['weapons']) + ([[137, 'Assault Rifle (in mode 16)']] if cls == '0x400' else [])
        line = '  %-7s %-52s %s' % (cls, ', '.join('%d %s' % (a, b) for a, b in weapons)[:52], ', '.join('%d %s' % (a, b) for a, b in v['ammo']) or 'no ammo kind tied')
        if not q or q in line.lower():
            print(line)
    unknown = [r for r in rows if r.get('mix_script') and r.get('reload_partners') is None and r.get('text', {}).get('group') in (3, 4)]
    if unknown and not q:
        print('  class unknown, no placing or literal ed3: ' + ', '.join('%d %s' % (r['id'], r['name']) for r in unknown))
    print('\nWhat ed140 returns, and the words ed130 shows for it')
    for code, entry in ((1, None), (2, 31), (3, 32), (4, 33), (5, 34), (6, 35)):
        if entry is None:
            print('  1  fine: the script goes on to ed9')
        else:
            label, words = boot_words(0, entry)
            print('  %d  boot 0,%d %s "%s"' % (code, entry, label, words[:90]))
    print('  -1 a handle has no node: no words')


# ---- verbs


def verbs():
    out = []
    for line in open(os.path.join(HERE, 'verbs.tsv'), encoding='utf8'):
        if line.startswith('#') or not line.strip():
            continue
        p = line.rstrip('\n').split('\t')
        p += [''] * (11 - len(p))
        out.append(dict(zip(('name', 'words', 'system', 'starts', 'call', 'args', 'gates', 'does', 'host', 'sure', 'who'), p)))
    return out


def who_rows():
    return {r['addr']: r for r in fact('who_table.json')}


PAIR = {'current_id': 'current', 'current_node': 'current', 'main_id': 'main', 'main_node': 'main'}


def who_line(addr, W):
    r = W.get(addr)
    if not r:
        return None
    direct = sorted({PAIR.get(k, k) for k in r.get('direct', {})})
    down = sorted({PAIR.get(k, k) for k in r.get('one_call_down', {})} - set(direct))
    return '%s %s: reads %s%s%s' % (addr, r['what'], ' and '.join(direct) or 'neither directly', ', and through a call ' + ' and '.join(down) if down else '',
                                    ' (has a jump table: cases may be missed)' if r.get('has_jump_table') else '')


def who_live(addr):
    direct, down = who_scan(int(addr, 16))
    return '%s: reads %s%s (scanned now: a table or a pointer isn\'t followed)' % (addr, ' and '.join(sorted(direct)) or 'neither directly', ', and through a call ' + ' and '.join(sorted(down)) if down else '')


def cmd_verb(argv):
    q = ' '.join(argv).lower()
    if q in ('prompts', 'prompt'):
        return cmd_prompts([])
    V = verbs()
    if not q:
        print('Everything a player can do, and the function behind it. `ed verb NAME` for one. `ed verb prompts` for all 63 prompts.\n')
        for v in V:
            print('  %-14s %-11s %s' % (v['name'], v['system'], v['call'][:88]))
        return
    W = who_rows()
    hits = [v for v in V if q == v['name'].lower() or q in [w.strip() for w in v['words'].lower().split(',')]] or [v for v in V if q in v['name'].lower()]
    if not hits:
        print('No verb named that. As a prompt:')
        return cmd_prompts([q])
    for v in hits:
        print('%s  (%s, %s)' % (v['name'], v['system'], v['sure']))
        print('  starts   ' + v['starts'])
        print('  call     ' + v['call'])
        print('  args     ' + v['args'])
        print('  gates    ' + v['gates'])
        print('  does     ' + v['does'])
        print('  host     ' + v['host'])
        lines = []
        for a in v['who'].split(','):
            if a:
                lines.append(who_line(a, W) or who_live(a))
        if lines:
            print('  who      "current" is r13-21496/-21492, "main" r13-21488/-21484. Moving only one pair breaks these silently:')
            for x in lines:
                print('           ' + x)
        page = 'docs/game/systems/%s.md' % v['system']
        print('  page     ' + (page if os.path.exists(os.path.join(disc.ROOT, page)) else 'docs/game/research/verbs.md'))
        print()


def cmd_prompts(argv):
    q = ' '.join(argv).lower()
    v = fact('verbs.json')
    print('Every prompt. 0x80117404 is asked from 29 sites: scripts reach it only through ed301, ed219, ed220, ed221. Button 2 is B. Button 6 has no binding: only shown, never pressed.')
    print('ed301 calls counts calls in all levels\' scripts. EXAMINE and PICK UP are also asked by the 624 ed219, ed220 and ed221 calls: `ed examine`.\n')
    print('id  label                  words                    ed301 calls, files, ids')
    exact = [p for p in v['prompts'] if q and (q == str(p['id']) or q == p['label'].lower() or q == p['words'].lower())]
    shown = 0
    for p in v['prompts']:
        full = ' '.join(str(x) for x in (p['id'], p['label'], p['words'], p.get('press_runs') or '', ' '.join(p.get('how_asked', [])))).lower()
        if exact and p not in exact:
            continue
        if q and q not in full:
            continue
        if not q and not p.get('asked'):
            continue
        shown += 1
        print('%3d %-22s %-24s %-4s %-4s %-4s %s' % (p['id'], p['label'][:22], p['words'][:24], p.get('script_calls', 0), p.get('script_files', 0), p.get('script_ids', 0), (p.get('press_runs') or '-')[:100]))
        if q:
            for h in p.get('how_asked', []):
                print('      asked: ' + h)
            print('      runs:  ' + (p.get('press_runs') or '-'))
            if p.get('sample'):
                print('      e.g.:  ' + ' '.join(p['sample']))
    if q and not shown:
        print('None match.')
    if not q:
        print('\nWords on the disc that nothing asks for: ' + ', '.join(p['label'] for p in v['prompts'] if not p.get('asked')))


# ---- doors


def watched():
    path = os.path.join(HERE, 'transitions_watched.json')
    return json.load(open(path, encoding='utf8')) if os.path.exists(path) else {}


def cmd_transition(argv):
    t = fact('transitions.json')['transitions']
    W = watched()
    t = dict(t)
    t.setdefault('17', {'motion': None, 'fade_out_ticks': None, 'ticks_until_room_change': 1, 'used_by_scripts': 0})
    keys = sorted((int(k) for k in t if re.fullmatch(r'\d+', k)))
    if argv:
        if not re.fullmatch(r'-?\d+', argv[0]):
            raise SystemExit('ed transition [N]')
        n = int(argv[0])
        r = t.get(str(n))
        if not r:
            return print('no transition %d: above 16 or negative takes the default branch, no motion, no fade, 1 tick' % n)
        print(json.dumps(r, indent=1))
        if str(n) in W:
            print('Watched: ' + W[str(n)])
        return
    print('fn66\'s 4th argument picks a fade to black and a motion on the player. A tick is one frame, 1/60 s. Runs took 2 or 3 frames more than the ticks, as a ticket starts after the frame it was written.\n')
    print('  n  motion  fade  ticks to room  script calls  what it is')
    for k in keys:
        r = t[str(k)]
        print('%3d  %-6s  %-5s %-13s  %-12s  %s' % (k, r.get('motion') if r.get('motion') is not None else '-', r.get('fade_out_ticks') if r.get('fade_out_ticks') is not None else '-',
                                                    r.get('ticks_until_room_change'), r.get('used_by_scripts', 0), W.get(str(k)) or r.get('meaning_by_usage') or ''))
    print('\nArrival: 10 tick fade in. The motion playing at that moment picks what plays: 25 gives 25, 99 gives 99, 103 gives 104, 104 gives 103. ed196 overrides it (3 scripts). Then the camera, then the script (5th argument).')
    print('Motion needs a marker with flag 4 in the room (85 rooms have none). Motion lengths differ by model. Evidence: docs/game/research/transitions.md, transitions-watched.md')


ASKS = {1: 'OPEN DOOR', 3: 'CLIMB UP', 4: 'CLIMB DOWN', 6: 'EXAMINE', 7: 'RETURN', 10: 'SECRET PASSAGE', 19: 'PICK UP', 34: 'DETAIL'}


def key_of(d):
    return (d['script'], str(d['to_room']), d['entry_hash'], d['camera_hash'], d['transition'], d['after_script'], d['flags'])


def merged_rows(rows):
    """Each distinct call once, over levels: `from` joined, and the label from a level this room belongs to where there is one."""
    out = {}
    for d in sorted(rows, key=lambda d: not d.get('from_in_level_rooms')):
        k = key_of(d)
        if k not in out:
            out[k] = dict(d, levels=[d['level']], **{'from': list(d['from'])})
        else:
            out[k]['levels'].append(d['level'])
            for f in d['from']:
                if f not in out[k]['from']:
                    out[k]['from'].append(f)
    return list(out.values())


def room_tests(d):
    """The rooms this call's script compares fn25 with: the script may act differently by room. Which branch needs reading."""
    return sorted({int(x) for x in re.findall(r'fn25\(\) [~=]= (\d+)', body(d['level'], d['script']))})


def variable_rooms(d):
    """A variable first argument: the literals it's given in this script, or for a global in any script of the level."""
    name = str(d['to_room'])
    if re.fullmatch(r'L\d+', name):
        return sorted({int(x) for x in re.findall(r'\b%s = (\d+)' % name, body(d['level'], d['script']))})
    out = set()
    root = os.path.join(DB, 'scripts', 'L' + d['level'])
    key = 'glob:' + d['level'] + name
    if key not in _cache:
        for f in os.listdir(root):
            out |= {int(x) for x in re.findall(r'\b%s = (\d+)' % name, open(os.path.join(root, f), encoding='utf8').read())}
        _cache[key] = sorted(out)
    return _cache[key]


def door_row(d, room=None):
    if d['source'] == 'item_record':
        where = 'item ' + ', '.join('%d %s' % (x['type_id'], x['label']) for x in d['on_item_record'][:2])
    else:
        local = [f for f in d['from'] if f['room'] == room] or d['from']
        where = ', '.join('%s %s' % (f['room'], f['region']) for f in local[:2]) or 'no region found'
    chain = [ASKS.get(a, str(a)) for a in d.get('asks_in_script') or []]
    t = d['transition']
    warn = []
    if d['entry_in_to_room'] is False and isinstance(d['to_room'], int):
        warn.append('entry not in that room')
    if d['camera_in_to_room'] is False and isinstance(d['to_room'], int):
        warn.append('camera not in that room')
    if isinstance(d['to_room'], int) and str(d['to_room']) not in db('rooms.json'):
        warn.append('no such room on the disc')
    tests = room_tests(d)
    gate = None
    return 'L%s/%-5d %-30s -> room %-5s at %-13s camera %-11s transition %-2s (%s), then %s, asks %s%s%s' % (
        d['level'], d['script'], where[:30], d['to_room'], d['entry'] or d['entry_hash'], d['camera'] or d['camera_hash'], t,
        'motion %s, %s ticks' % (d['motion'], d['ticks_until_room_change']) if d['motion'] else '%s ticks' % d['ticks_until_room_change'], show(d['after_script']),
        ' then '.join(chain) or 'nothing', ', script tests rooms %s' % ' '.join(str(x) for x in tests[:6]) if tests else '', '  (' + ', '.join(warn) + ')' if warn else '')


def cmd_door(argv):
    level = None
    if '--level' in argv:
        i = argv.index('--level')
        if i + 1 >= len(argv):
            raise SystemExit('ed door [ROOM] [--level N]')
        level = argv[i + 1].zfill(2)
        del argv[i:i + 2]
    rows = fact('doors_table.json')
    if level:
        rows = [r for r in rows if r['level'] == level]
    if argv and not re.fullmatch(r'\d+', argv[0]):
        raise SystemExit('ed door [ROOM] [--level N]')
    if not argv:
        if level:
            print('%d room changes in level %s:' % (len(rows), level))
            for d in merged_rows(rows):
                print('  ' + door_row(d))
            return
        print('%d room changes in the scripts, all through fn66(room, entry, camera, transition, script, flags). `ed transition` says what each number does. `ed door ROOM`, `ed door --level N`.' % len(rows))
        print('Not listed: ed29, script 1023, in 15 levels, transition 17; and 7 game code sites. `ed system doors`.\n')
        return cmd_transition([])
    room = int(argv[0])
    if str(room) not in db('rooms.json'):
        print('Room %d has no file on the disc.' % room)
    picked = merged_rows(rows)
    here = {}
    for x in db('rooms.json').get(str(room), {}).get('regions', []):
        for k in ('enter', 'leave', 'stay', 'cross_a', 'cross_b', 'other_enter', 'other_leave', 'other_stay'):
            if x.get(k, -1) > 0:
                here.setdefault(x[k], []).append(x['name'])
    print('WAYS OUT of room %d (every door whose script a region here runs. A script testing fn25 may do this only in some rooms: `ed script ID`)' % room)
    unknown = 0
    for d in picked:
        if d['script'] in here or any(f['room'] == room for f in d['from']):
            if not any(f['room'] == room for f in d['from']):
                d['from'] = [{'room': room, 'region': n, 'field': ''} for n in here[d['script']]] + d['from']
            print('  ' + door_row(d, room))
        elif not d['from']:
            if room in room_tests(d):
                print('  ' + door_row(d, room) + '  (no region: its script tests this room)')
            else:
                unknown += 1
    print('WAYS IN to room %d' % room)
    for d in picked:
        if d['to_room'] == room:
            print('  ' + door_row(d, room))
    for d in picked:
        if not isinstance(d['to_room'], int) and room in variable_rooms(d):
            print('  L%s/%-5d variable room %s, which can be %d: transition %s, from %s' % (d['level'], d['script'], d['to_room'], room, d['transition'], ', '.join('%s %s' % (f['room'], f['region']) for f in d['from'][:2]) or 'no region found'))
    print('(%d more calls start from no known room: `ed door --level N` lists a level\'s. A call the game\'s code makes, or ed29, isn\'t here.)' % unknown)


def variable_words(r):
    """An examine whose group or entry is a local: the words of the values the script gives it."""
    text = body(r['level'], r['script'])
    room = db('text.json').get('room' + r['level'], {})

    def values(v):
        return [v] if isinstance(v, int) else [int(x) for x in re.findall(r'\b%s = (\d+)' % re.escape(str(v)), text)]
    found = []
    for g in values(r.get('group')):
        for e in values(r.get('entry')):
            row = room.get('%s,%s' % (g, e))
            if row and row[2] not in found:
                found.append(row[2])
    if not found:
        return '(group or entry is a variable: `ed script %d`)' % r['script']
    return 'by variable, one of: ' + ' | '.join('"%s"' % w[:70] for w in found[:4]) + (' +%d more' % (len(found) - 4) if len(found) > 4 else '')


def cmd_examine(argv):
    rows = fact('examines_table.json')
    if not argv:
        import collections
        c = collections.Counter(r['native'] for r in rows)
        print('%d examines in the scripts: %s. `ed examine ROOM` or `ed examine WORD`.' % (len(rows), dict(c)))
        print('ed220(group, entry) words only. ed219(group, entry, camera, n) close-up. ed221(group, entry, camera, n, tag, entry once taken) close-up and PICK UP.')
        print('n: bit 1 blend into the close-up, bit 2 (ed221) fall back to words only, bit 4 0x80043C88 on opening and closing.')
        return
    q = ' '.join(argv)
    if re.fullmatch(r'\d+', q):
        if q not in db('rooms.json'):
            print('Room %s has no file on the disc.' % q)
        rows = [r for r in rows if any(f['room'] == int(q) for f in r['from'])]
    else:
        ql = q.lower()
        rows = [r for r in rows if ql in (r.get('label') or '').lower() or ql in (r.get('words') or '').lower() or any(ql in f['region'].lower() for f in r['from']) or any(ql in n.lower() for n in r.get('item_names') or [])]
    merged = {}
    for r in rows:
        k = (r['script'], json.dumps(r['args']))
        m = merged.setdefault(k, {'row': r, 'levels': [], 'from': []})
        m['levels'].append(r['level'])
        for f in r['from']:
            if f not in m['from']:
                m['from'].append(f)
        # Prefer the copy in a level that places the thing
        if r.get('item_names') and not m['row'].get('item_names'):
            m['row'] = r
    for m in merged.values():
        r = m['row']
        a = r['args']
        cam = hash_name(a[2]) if len(a) > 2 and isinstance(a[2], int) else (a[2] if len(a) > 2 else '-')
        thing = ''
        if r['native'] == 'ed221' and len(a) > 4:
            names = r.get('item_names') or []
            spawned = r.get('spawned_types_with_tag') or []
            items = fact('item_table.json')['rows']
            offer = ', '.join('%s %s' % (i, n) for i, n in zip(r.get('type_ids') or [], names)) or ', '.join('%s %s, spawned by a script' % (i, items[i]['name']) for i in spawned if isinstance(i, int)) or 'nothing placed in these levels'
            thing = '  offers %s (tag %s)' % (offer, a[4])
            if len(a) > 5 and a[5] != -1:
                thing += ', once taken entry %s' % a[5]
        where = ', '.join('%s %s' % (f['room'], f['region']) for f in m['from'][:3]) or 'no region found'
        if r.get('entry') == -1:
            words = '(entry -1: no words, straight to RETURN)'
        elif not isinstance(r.get('entry'), int) or not isinstance(r.get('group'), int):
            words = variable_words(r)
        else:
            words = '"%s"' % (r.get('words') or '')[:130]
        print('L%s/%-5d %-5s %s, camera %s, n %s  %s%s\n    %s' % (','.join(m['levels']), r['script'], r['native'], where, cam, a[3] if len(a) > 3 else '-', r.get('label') or '', thing, words))
    print('%d.' % len(merged))


# ---- levels, who, saves


def cmd_level(argv):
    L = fact('levels.json')['levels']
    if not argv:
        print('Level is the chapter word 0x80307178. "who" is from name counts in the level\'s words, so it is inferred; model is the game\'s table, which player_model() can override. Level 16\'s scripts are copies of level 09\'s, loaded only through argument 19: `ed system levels`.')
        for k in sorted(L, key=lambda x: int(x)):
            v = L[k]
            print('  %3s  %-58s page %-9s model %-4s start room %-5s %d scripts' % (k, (v.get('who') or '')[:58], v.get('chapter_page_label') or '-', v.get('model_0x80007C8C', '-'), v.get('start_room_0x8023D738', '-'), v.get('script_count') or 0))
        return
    v = L.get(argv[0].zfill(2)) or L.get(argv[0])
    print(json.dumps(v, indent=0)[:2500] if v else 'no level %s' % argv[0])


PAIR_READS = {-21496: 'current', -21492: 'current', -21488: 'main', -21484: 'main'}
PAIR_CALLS = {0x801F62B8: 'current', 0x801F62B0: 'current', 0x801F6258: 'main', 0x801F6250: 'main'}


def who_scan(addr):
    """Which pair a function reads directly and through one call, from its disassembly. Direct branches only."""
    import know
    code = know.Code()

    def one(a):
        pairs, callees = set(), []
        for _, op in code.body(a):
            m = re.match(r'(?:lwz|stw)\s+r\d+, (-\d+)\(r13\)', op)
            if m and int(m.group(1)) in PAIR_READS:
                pairs.add(PAIR_READS[int(m.group(1))])
            m = re.match(r'bl\s+0x([0-9A-F]{8})', op)
            if m:
                t = int(m.group(1), 16)
                if t in PAIR_CALLS:
                    pairs.add(PAIR_CALLS[t])
                else:
                    callees.append(t)
        return pairs, callees
    direct, callees = one(addr)
    down = set()
    for c in callees:
        down |= one(c)[0]
    return direct, down - direct


def cmd_who(argv):
    W = who_rows()
    print('The game has two player pairs. Current: r13-21496 id, -21492 node. Main: r13-21488 id, -21484 node. Every save holds the same in both, so only the code tells them apart.')
    print('Moving only one pair breaks Use and Mix with no error: the Tome lists current\'s items, and fn93 takes from main\'s.')
    print('This list is the functions studied so far. The two main getters alone have 104 call sites: name any address.\n')
    if not argv:
        for addr in W:
            print(who_line(addr, W))
        return
    q = argv[0]
    hits = [a for a, r in W.items() if re.search(r'\b%s\b' % re.escape(q), r['what'], re.I)]
    if hits and not re.fullmatch(r'(0x)?[0-9a-fA-F]{8}', q):
        for a in hits:
            print(who_line(a, W))
        return
    import know
    addr = '%08X' % know.resolve(q)[0]
    print(who_line(addr, W) if addr in W else who_live(addr))


def _nodes(S):
    head = S.u32(S.r13 - 21468)
    nodes = {}
    if head:
        node = S.u32(head + 4)
        while node:
            nodes[S.s32(node + 16)] = node
            node = S.u32(node + 4)
    return nodes


def _held(S):
    """Handle -> (holder, hand mask), from equipment at 0x80588DF0: 20 records of 408 bytes, entries at +4, 12 bytes each."""
    held = {}
    for i in range(20):
        at = 0x80588DF0 + i * 408
        holder, count = S.s32(at), S.s32(at + 388)
        if holder <= 0 or count <= 0 or count > 32:
            continue
        for k in range(count):
            e = at + 4 + 12 * k
            held[S.s32(e)] = (holder, S.s32(e + 4))
    return held


def describe_thing(S, h, n, rows, held):
    info = S.u32(n + 44)
    tid = S.s32(info + 12) if info else -1
    d = S.u32(n + 36)
    tag = S.s32(d + 24) if d else 0
    rec = S.u32(info + 4) if info else 0
    name = rows[tid]['name'] if 0 <= tid < len(rows) else 'not an item'
    count = ''
    if rec and 0x80000000 <= rec < 0x81800000:
        c, m = S.u16(rec + 68), S.u16(rec + 70)
        count = ', count %d of %d' % (c, m) if c or m else ''
    hold = ', IN HAND of %d, mask %d' % held[h] if h in held else ''
    return 'handle %-4d type %-4d %-30s tag %-5d node kind %d%s%s' % (h, tid, name[:30], tag, S.s32(n + 8), count, hold)


def cmd_carried(argv):
    import savestate
    if not argv:
        raise SystemExit('ed carried SAVE [HANDLE]: who carries what, and what\'s in hand. With a handle: any thing, carried or not')
    path = argv[0]
    for base in (path, os.path.join(disc.ROOT, path), os.path.join(disc.ROOT, 'saves', path), os.path.join(disc.ROOT, 'saves', path + '.state')):
        if os.path.exists(base):
            path = base
            break
    else:
        raise SystemExit('no save %s: saves/ has them, as saves/slot1.state or slot1' % path)
    S = savestate.State(path)
    rows = fact('item_table.json')['rows']
    print('%s: chapter %d, room %d. Saves get re-saved: this is now, not what a log of it saw. Handles last this play only; type ids are always the same.' % (os.path.basename(path), S.s32(0x80307178), S.s32(S.r13 - 23276)))
    print('current handle %d, main handle %d. Tome record word 0: %d, word 1: %d' % (S.s32(S.r13 - 21496), S.s32(S.r13 - 21488), S.s32(0x80335748), S.s32(0x8033574C)))
    nodes = _nodes(S)
    if not nodes:
        return print('no thing list in this save')
    held = _held(S)
    if len(argv) > 1:
        h = int(argv[1])
        print(describe_thing(S, h, nodes[h], rows, held) if h in nodes else 'handle %d: no node in this save' % h)
        return
    print('(node kind: 2 a player, 30 a thing that can be found. 0 and 91 are seen on carried things too; what those mean isn\'t worked out.)')
    for h, n in sorted(nodes.items()):
        if S.s32(n + 8) != 2:
            continue
        carry = S.u32(n + 48)
        if not carry:
            continue
        print('player handle %d, model %s, carries %d:' % (h, S.u16(S.u32(n + 40) + 154) if S.u32(n + 40) else '?', S.s32(carry + 8)))
        e = S.u32(carry + 4)
        k = 0
        while e and k < 64:
            hh = S.s32(e + 8)
            print('   ' + (describe_thing(S, hh, nodes[hh], rows, held) if hh in nodes else 'handle %d: no node' % hh))
            e = S.u32(e + 4)
            k += 1


def cmd_tag(argv):
    if not argv or not argv[0].isdigit():
        raise SystemExit('ed tag N: a placed thing by its tag')
    tag = int(argv[0])
    print('Tag %d: a number from a placing, on node +36 then +24, not unique. fn78(2, tag), fn78(4, tag), ed221\'s 5th argument.' % tag)
    n = 0
    for level, rows in sorted(fact('placed_things.json').items(), key=lambda kv: int(kv[0])):
        for r in rows:
            if r.get('tag') == tag:
                n += 1
                type_id = r.get('type_id')
                sets = r.get('by_alignment') or []
                items = fact('item_table.json')['rows']
                what = '%s %s' % (type_id, items[type_id]['name']) if isinstance(type_id, int) and type_id < len(items) else ', '.join('set %s: %s' % (s['align'], s.get('type_id')) for s in sets) or 'not an item: chr %s model %s' % (r.get('chr'), r.get('model'))
                print('  Npcs%s section %s, room %s at %s: %s' % (level, r['section'], r['room'], r['pos'], what))
    if not n:
        print('  in no placing file')
    root = os.path.join(DB, 'scripts')
    pat = re.compile(r'\b(?:fn78\([245], %d\)|ed3\([^()\n]*, %d, [^()\n]*\)|ed221\([^()\n]*, %d, -?\d+\))' % (tag, tag, tag))
    hits = []
    for lv in sorted(os.listdir(root)):
        for f in os.listdir(os.path.join(root, lv)):
            if pat.search(open(os.path.join(root, lv, f), encoding='utf8').read()):
                hits.append('%s/%s' % (lv, f[:-4]))
    print('  scripts naming it: ' + (' '.join(hits[:30]) or 'none') + (' +%d more' % (len(hits) - 30) if len(hits) > 30 else ''))


def cmd_where(argv):
    if len(argv) < 4:
        raise SystemExit('ed where ROOM X Y Z: which regions a place is inside, and the nearest cameras')
    room = db('rooms.json').get(str(int(argv[0])))
    if not room:
        return print('Room %s has no file on the disc.' % argv[0])
    x, y, z = [float(a) for a in argv[1:4]]
    print('Inside at %g %g %g in room %s. Spheres and boxes only: where a plane lies isn\'t read. Which wins when regions overlap: the last to run, not worked out.' % (x, y, z, argv[0]))
    n = 0
    for r in room['regions']:
        inside = False
        if r.get('shape') == 1:
            inside = all(a <= v <= b for a, v, b in zip(r['min'], (x, y, z), r['max']))
        elif r.get('shape') == 0:
            inside = sum((a - b) ** 2 for a, b in zip(r['pos'], (x, y, z))) <= r['radius'] ** 2
        if inside:
            n += 1
            print('  %-22s +110 %-3s %s' % (r['name'], r.get('p110'), ' '.join('%s %d' % (k, r[k]) for k in ('enter', 'leave', 'stay') if r.get(k, -1) > 0) or 'runs nothing'))
    if not n:
        print('  in no sphere or box')
    near = sorted(room['cameras'], key=lambda c: sum((a - b) ** 2 for a, b in zip(c['pos'], (x, y, z))))[:3]
    print('Nearest cameras by first place: ' + ', '.join(c['name'] for c in near) + '. What should be on comes from the regions\' scripts: `ed script ID`.')
    marks = sorted(room['markers'], key=lambda m: sum((a - b) ** 2 for a, b in zip(m['pos'], (x, y, z))))[:3]
    print('Nearest markers: ' + ', '.join(m['name'] for m in marks))


ROUTES = {
    'door': '`ed system doors`, `ed verb door`, `ed door ROOM`, `ed transition`', 'doors': '`ed system doors`, `ed door ROOM`, `ed transition`', 'transition': '`ed transition`, `ed system doors`',
    'ladder': '`ed system doors`, `ed transition 12`, `ed transition 15`', 'room': '`ed room N`, `ed door ROOM`, `ed where ROOM X Y Z`', 'arrival': '`ed system doors`',
    'camera': '`ed system camera`, `ed room N`, `ed camera NAME --room N`, `ed where ROOM X Y Z`', 'tome': '`ed system tome`, `ed verb tome`, `ed system use-mix`, `ed keep`',
    'menu': '`ed system tome`, `ed system use-mix`', 'inventory': '`ed system use-mix`, `ed verb use`', 'examine': '`ed system examine`, `ed verb examine`, `ed examine ROOM`',
    'check': '`ed verb check`, `ed system use-mix`', 'mix': '`ed system use-mix`, `ed verb mix`, `ed mix`', 'use': '`ed system use-mix`, `ed verb use`, `ed item ID`', 'equip': '`ed verb equip`, `ed system use-mix`',
    'mode': '`ed verb mode`', 'reload': '`ed verb reload`, `ed system use-mix`, `ed mix`', 'pick up': '`ed system pickups`, `ed verb pick up`, `ed item ID`', 'pickup': '`ed system pickups`, `ed verb pick up`',
    'find': '`ed system pickups`, `ed verb find`', 'item': '`ed system items`, `ed item TEXT|ID`, `ed item --list`, `ed carried SAVE`', 'items': '`ed system items`, `ed item --list`, `ed carried SAVE`',
    'location': '`ed system location`, `ed where ROOM X Y Z`, `ed room N`', 'where': '`ed where ROOM X Y Z`', 'prompt': '`ed system prompts`, `ed prompts`', 'level': '`ed system levels`, `ed level`', 'chapter': '`ed system levels`, `ed level`',
    'words': '`ed system words`, `ed text WORDS`', 'player': '`ed system players`, `ed who`, `ed needs ADDR`', 'players': '`ed system players`, `ed who`', 'talk': '`ed verb talk`', 'attack': '`ed verb attack`', 'cast': '`ed verb cast`, `ed host cast`', 'spell': '`ed verb cast`',
}


def route(q):
    return ROUTES.get(q.lower().strip())


def how_lines(q):
    """For `ed how`: the rows of the whole-game tables that match, so how starts from them."""
    words = q.lower().split()
    out = []
    rows = [r for r in fact('item_table.json')['rows'] if all(w in ((r.get('name') or '') + ' ' + (r.get('label') or '')).lower() for w in words)]
    for r in rows[:6]:
        scr = [('USE', 'use_script'), ('CHECK', 'after_examine_script'), ('MIX', 'mix_script'), ('AFTER FIND', 'after_find_script')]
        s = ', '.join('%s script %d' % (a, r[b]) for a, b in scr if r.get(b)) or 'no item scripts'
        rooms = ', '.join('room %s tag %s' % (p['room'], p['tag']) for p in (r.get('placed') or [])[:3])
        out.append('ITEM type %d %s: %s%s. `ed item %d`' % (r['id'], (r.get('name') or '')[:60], s, '; placed ' + rooms if rooms else '', r['id']))
        for line in says_of(r)[:4]:
            out.append('  SAYS ' + line)
        for e in examines_of(r)[0][:3]:
            a = e['args']
            rooms = ', '.join('room %s %s' % (f['room'], f['region']) for f in e['from'][:2])
            out.append('  EXAMINE L%s/%d ed221(%s, %s, %s, %s, %s, %s) from %s: EXAMINE, DETAIL, words, PICK UP. Steps: `ed system examine`' % (e['level'], e['script'], a[0], a[1], hash_name(a[2]) if isinstance(a[2], int) else a[2], a[3], a[4], a[5], rooms))
    for v in verbs():
        if q.lower() == v['name'].lower() or q.lower() in [w.strip() for w in v['words'].lower().split(',')]:
            out.append('VERB %s: call %s. `ed verb %s`' % (v['name'], v['call'], v['name'].lower()))
    return out
