import re,glob,collections,os
pat=re.compile(r'ed221\s*\(([^()]*(?:\([^()]*\)[^()]*)*)\)')
c=collections.Counter(); rows=[]
for f in glob.glob('build/ed-db/scripts/L*/*.lua'):
    t=open(f,encoding='utf8',errors='replace').read()
    for m in pat.finditer(t):
        a=[x.strip() for x in m.group(1).split(',')]
        rows.append((os.path.relpath(f,'build/ed-db/scripts'),a))
print(len(rows))
xc=collections.Counter(r[1][4] if len(r[1])>4 else '?' for r in rows)
yc=collections.Counter(r[1][5] if len(r[1])>5 else '?' for r in rows)
nc=collections.Counter(r[1][3] if len(r[1])>3 else '?' for r in rows)
print('X',xc.most_common(80))
print('Y',yc.most_common(30))
print('n',nc.most_common(30))
for r in rows[:15]: print(r)
