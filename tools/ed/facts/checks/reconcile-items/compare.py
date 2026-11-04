"""Compares item_table.json with the copy from before the reconcile fixes."""
import json
import os
HERE = os.path.dirname(os.path.abspath(__file__))
a = json.load(open(os.path.join(HERE, 'item_table.before.json'), encoding='utf8'))
b = json.load(open(os.path.join(HERE, '..', 'item_table.json'), encoding='utf8'))
ch = []
for x, y in zip(a['rows'], b['rows']):
    if x['reload_partners'] != y['reload_partners']:
        ch.append((y['id'], y['name'], x['reload_partners'], y['reload_partners']))
print('rows whose reload_partners changed', len(ch))
print([c for c in ch if c[3] is not None])
print('to null', [c[0] for c in ch if c[3] is None])
print('placed rows with set 3 now', sum(1 for r in b['rows'] for w in r['placed'] if w.get('set') == 3),
      'with set 4 before', sum(1 for r in a['rows'] for w in r['placed'] if w.get('set') == 4),
      'with set 4 now', sum(1 for r in b['rows'] for w in r['placed'] if w.get('set') == 4))
print('grip left in json:', 'grip' in json.dumps(b['rows'][21]['record']) or 'grip' in b['rows'][21])
print([(r['id'], r['name'], [w.get('set') for w in r['placed']], [w['room'] for w in r['placed']]) for r in b['rows'] if any('set' in w for w in r['placed'])])
print(b['tome_actions'][2])
same = sum(1 for x, y in zip(a['rows'], b['rows']) if {k: v for k, v in x.items() if k not in ('grip', 'fire_mode', 'reload_partners', 'reload_masks', 'record', 'placed')} ==
           {k: v for k, v in y.items() if k not in ('grip', 'fire_mode', 'reload_partners', 'reload_masks', 'record', 'placed')})
print('rows otherwise unchanged', same, 'of', len(b['rows']))
print('group 3/4 no kind', [(r['id'], r['name']) for r in b['rows'] if r['text']['group'] in (3, 4) and r['reload_partners'] is None])
