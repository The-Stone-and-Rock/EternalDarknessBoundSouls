from lib import *
import glob,re,collections
use=set(s16(r,38) for r in U)-{0}
sc=collections.defaultdict(dict)
for p in glob.glob('build/ed-db/scripts/L*/*.lua'):
    i=int(os.path.basename(p)[:-4]); sc[i]=open(p,encoding='utf8',errors='replace').read()
c=collections.Counter()
bad=[]
for i in use:
    t=sc[i]
    m=re.findall(r'ed11\((\d+), (\d+), (\w+)\)',t)
    if ('0','0','0') in m: c['has ed11(0,0,0)']+=1
    # line-order test
    ls=t.splitlines()
    seen93=False
    for l in ls:
        if l.startswith('::') or l.startswith('goto') : pass
        if 'fn93(' in l: seen93=True
        if 'ed11(0, 0, 0)' in l and seen93:
            bad.append(i);break
print(c,len(use),'ed11(0,0,0) after a fn93 line in file:',bad)
for i in bad[:4]:
    print('=====',i); print(sc[i][:700])
