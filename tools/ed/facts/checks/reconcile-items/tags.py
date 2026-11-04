"""Which source each fn78(2|4, literal) tag has: Npcs tables 1 (+34), 2 (+98), 3 (+40), 4 (+46), 5 (+56), literal ed3 arg 7, live thing. Read only."""
import sys,struct,re,glob,os,json,collections
HERE=os.path.dirname(os.path.abspath(__file__));ROOT=os.path.abspath(os.path.join(HERE,'..','..','..'))
sys.path.insert(0,os.path.join(ROOT,'tools','ed'));os.chdir(ROOT)
import disc
d=disc.Disc()
OFF={1:(40,34),2:(104,98),3:(44,40),4:(52,46),5:(60,56)}
src=collections.defaultdict(set);sizes=collections.Counter()
for p in d.glob(r'^Npcs\d+\.npc$'):
    b=d.read(p);h=struct.unpack('>11I',b[:44]);at=44
    for t in range(1,6):
        n,sz=h[t*2-1],h[t*2];st,o=OFF[t]
        assert n*st==sz,(p,t,n,sz)
        for i in range(n):
            v=struct.unpack('>H',b[at+i*st+o:at+i*st+o+2])[0]
            if v!=0xFFFF: src[v].add('t%d'%t)
        at+=sz
    assert at==len(b)
want=collections.Counter()
ED3=re.compile(r'ed3\((?:[^,]+,){6} *(-?\d+),');F=re.compile(r'fn78\(([24]), (\d+)\)')
for f in glob.glob('build/ed-db/scripts/L*/*.lua'):
    t=open(f,encoding='utf8',errors='replace').read()
    for m in ED3.finditer(t):
        if int(m.group(1))>=0: src[int(m.group(1))].add('ed3')
    for m in F.finditer(t): want[int(m.group(2))]+=1
cov=collections.Counter()
nosrc=[]
for v,c in want.items():
    s=src.get(v)
    if not s: nosrc.append(v);continue
    for k in s: cov[k]+=1
print('distinct fn78 2/4 literals',len(want),'by source (a tag can be in several)',sorted(cov.items()))
only1=[v for v in want if src.get(v) and src[v]<= {'t1'}]
only124=[v for v in want if src.get(v) and src[v]<={'t1','t2','t4'}]
print('only in table 1',len(only1),'only in 1/2/4',len(only124))
print('no source',sorted(nosrc))
