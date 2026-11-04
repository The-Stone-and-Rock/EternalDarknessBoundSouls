import re,glob,collections,sys,os
names=sys.argv[1:]
res={n:collections.Counter() for n in names}
ex={n:{} for n in names}
for f in glob.glob('build/ed-db/scripts/L*/*.lua'):
    t=open(f,encoding='utf8',errors='replace').read()
    for n in names:
        for m in re.finditer(r'(?<![A-Za-z0-9_])'+n+r'\(([^()\n]*)\)',t):
            a=[x.strip() for x in m.group(1).split(',')]
            key=tuple(a)
            res[n][key]+=1
            ex[n].setdefault(key,f[-12:])
for n in names:
    print('==',n,sum(res[n].values()),'calls',len(res[n]),'distinct')
    for k,v in res[n].most_common(int(os.environ.get('TOP','30'))):
        print('  ',v,k,ex[n][k])
