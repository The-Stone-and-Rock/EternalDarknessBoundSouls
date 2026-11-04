import json,collections
P=json.load(open('build/ed-db/facts/placed_things.json'))
for L in ('3',):
    for r in P[L]:
        if r['section']==0 and r.get('type')==44:
            print(r)
c=collections.Counter()
for L,rows in P.items():
    for r in rows:
        if 'mask' in r: c[(int(L),r['section'],r['mask'])]+=1
print(sorted(c.items()))
