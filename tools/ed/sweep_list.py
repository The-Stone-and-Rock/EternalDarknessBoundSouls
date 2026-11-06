"""sweep_list.py LEVEL: every examine and pickup region of a chapter, from the kit's tables, with where to stand.
Prints JSON: rooms reached by doors, and for each room its interactions (kind, script, region, centre, size, words)."""
import json, os, sys

sys.path.insert(0, 'tools/ed')
import chapters  # noqa: E402

level = int(sys.argv[1])
rooms_doors = chapters.rooms_of_level(level)
rooms_json = json.load(open('build/ed-db/rooms.json'))
examines = json.load(open('build/ed-db/facts/examines_table.json'))
out = {'level': level, 'rooms': {}}
for room in sorted(rooms_doors):
    regions = {r['name']: r for r in rooms_json.get(str(room), {}).get('regions', [])}
    items = []
    for e in examines:
        if e['level'] != '%02d' % level:
            continue
        for f in e['from']:
            if f['room'] != room or f['field'] != 'enter':
                continue
            r = regions.get(f['region'])
            if r is None:
                continue
            if 'min' in r:
                lo, hi = r['min'], r['max']
            elif 'pos' in r:
                lo = [r['pos'][0] - r['radius'], r['pos'][1] - r['radius'], r['pos'][2]]
                hi = [r['pos'][0] + r['radius'], r['pos'][1] + r['radius'], r['pos'][2]]
            else:
                continue
            centre = [(a + b) / 2 for a, b in zip(lo, hi)]
            items.append({'kind': 'pickup' if e['native'] == 'ed221' else 'examine', 'native': e['native'], 'script': e['script'],
                          'region': f['region'], 'shape': r['shape'], 'min': lo, 'max': hi, 'centre': centre, 'words': e.get('label') or e.get('words')})
    out['rooms'][room] = {'door': rooms_doors[room][0]['script'], 'interactions': items}
print(json.dumps(out, indent=1))
