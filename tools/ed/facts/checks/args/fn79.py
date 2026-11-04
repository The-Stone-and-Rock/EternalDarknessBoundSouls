import re,glob,collections,os
c=collections.Counter();ex={}
for f in glob.glob('build/ed-db/scripts/L*/*.lua'):
    t=open(f,encoding='utf8',errors='replace').read()
    for m in re.finditer(r'(?<![A-Za-z0-9_])fn79\(([^()\n]*)\)',t):
        a=[x.strip() for x in m.group(1).split(',')]
        c[a[1]]+=1; ex.setdefault(a[1],[]).append(('L'+f.split('L')[-1].replace(chr(92),'/'),m.group(1)))
for k,v in c.most_common():
    print(k,v,ex[k][:3])
