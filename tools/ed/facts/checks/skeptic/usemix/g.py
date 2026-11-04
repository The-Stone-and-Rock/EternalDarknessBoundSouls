from lib import *
import re,glob,os,collections
SC={}
for p in glob.glob('build/ed-db/scripts/L*/*.lua'):
    lv=os.path.basename(os.path.dirname(p)); i=int(os.path.basename(p)[:-4])
    SC.setdefault(i,{})[lv]=open(p,encoding='utf8',errors='replace').read()
print('scripts',len(SC))
use=collections.defaultdict(list);mix=collections.defaultdict(list);chk=collections.defaultdict(list);pick=collections.defaultdict(list)
for k,r in enumerate(U):
    for d,o in ((pick,36),(use,38),(chk,40),(mix,42)):
        v=s16(r,o)
        if v: d[v].append(k)
print('use scripts',len(use),'mix',len(mix),dict(mix),'chk',dict(chk))
missing=[v for v in list(use)+list(mix)+list(chk)+list(pick) if v not in SC]
print('referenced but absent',sorted(set(missing)))
st={'ed20':0,'pTrigger':0,'both':0,'neither':0}
pt=[];ne=[]
for v in use:
    t=' '.join(SC.get(v,{}).values())
    a='ed20(' in t;b='pTrigger' in t
    st['ed20']+=a;st['pTrigger']+=b;st['both']+=a and b;st['neither']+=(not a and not b)
    if b: pt.append(v)
    if not a and not b: ne.append(v)
print(st,'use scripts with pTrigger',pt,'with neither',ne[:50])
for v in pt[:6]:
    for lv,t in SC[v].items():
        print(v,lv,[l for l in t.splitlines() if 'pTrigger' in l][:4]);break
