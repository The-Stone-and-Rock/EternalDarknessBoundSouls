"""Writes runtime/host/item_spawn.inc: each item type's world model (chr, model) for the test spawner
(ED_ITEM_SPAWN in hle.c), from build/ed-db/facts/item_table.json: the first place the game puts one
(Npcs table 3 or 5) or makes one (an ed3 call). Items the game never places have model -1.
    python -I tools/ed/facts/item_spawn_table.py
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
rows = json.load(open(os.path.join(ROOT, 'build', 'ed-db', 'facts', 'item_table.json'), encoding='utf-8'))['rows']
out = ['// Written by tools/ed/facts/item_spawn_table.py: {type, chr, model, name}. model -1: never placed in the world.']
known = 0
for r in rows:
    src = (r.get('placed') or []) + (r.get('spawned_by') or [])
    chr_, model = (src[0]['chr'], src[0]['model']) if src else (25, -1)
    known += model >= 0
    name = ''.join(c for c in (r.get('name') or r.get('label') or '') if c.isprintable() and c not in '"\\')
    out.append('{%d, %d, %d, "%s"},' % (r['id'], chr_, model, name))
with open(os.path.join(ROOT, 'runtime', 'host', 'item_spawn.inc'), 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(out) + '\n')
# The item models each level places (NpcsN.npc is level N): loaded there, so a stand-in for one from elsewhere.
pairs = sorted({(int(p['file'][4:-4]), p['chr'], p['model']) for r in rows for p in (r.get('placed') or [])
                if p['file'].startswith('Npcs') and p['file'][4:-4].isdigit()})
with open(os.path.join(ROOT, 'runtime', 'host', 'item_spawn_levels.inc'), 'w', encoding='utf-8', newline='\n') as f:
    f.write('// Written by tools/ed/facts/item_spawn_table.py: {level, chr, model} for each item model a level places.\n')
    f.write('\n'.join('{%d, %d, %d},' % p for p in pairs) + '\n')
print(f'{len(rows)} items, {known} with a world model; {len(pairs)} level models')
