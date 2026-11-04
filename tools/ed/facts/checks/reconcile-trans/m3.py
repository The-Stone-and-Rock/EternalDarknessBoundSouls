import sys,struct,json,re,time,glob,os,collections
sys.path.insert(0,'tools/ed')
import disc,gametext,savestate
d=disc.Disc()
AL=set(range(65,91))|set(range(97,123))
def expand(path):
    r=d.read(path)
    first=struct.unpack('>I',r[4:8])[0];csize=struct.unpack('>I',r[12:16])[0]
    return bytes(gametext.bpe_expand(r,first,min(first+csize,len(r))))
def goodname(x,a):
    f=x[a:a+40]
    e=f.find(b'\0')
    if e<3: return False
    if any(not 32<=c<127 for c in f[:e]): return False
    if f[e:].strip(b'\0'): return False
    return f[0] in AL
def table(x,n):
    out=[]
    end=len(x)-76*n
    for p in range(0,end,4):
        if x[p] in AL and x[p+76*(n-1)] in AL and goodname(x,p):
            if all(goodname(x,p+76*i) for i in range(1,n)):
                out.append(p)
    return out
t=time.time()
res={};fails=[];multi=[]
for p in d.glob(r'^Rooms/rmed\d+\.bpe$'):
    rid=int(re.search(r'rmed(\d+)',p).group(1))
    x=expand(p)
    n=struct.unpack('>H',x[212:214])[0]
    c=table(x,n) if n else []
    if n and not c: fails.append((rid,n)); continue
    if len(c)>1: multi.append((rid,n,c,[x[q:q+20].split(b'\0')[0] for q in c]))
    if c:
        q=c[0]
        res[rid]=[(x[q+76*i:q+76*i+40].split(b'\0')[0].decode(),struct.unpack('>I',x[q+76*i+40:q+76*i+44])[0],struct.unpack('>I',x[q+76*i+72:q+76*i+76])[0]) for i in range(n)]
    else: res[rid]=[]
print('secs',time.time()-t,'rooms',len(res),'fails',fails,'multi',multi[:10],len(multi))
json.dump({str(k):v for k,v in res.items()},open('tools/ed/facts/checks/reconcile-trans/mk.json','w'))
sk=json.load(open('build/ed-db/facts/skeptic/trans/markers2.json'))
print('differs from S2 parse',[k for k,v in sk.items() if [r['name'] for r in v['recs']]!=[a[0] for a in res.get(int(k),[])]])
hasf4={k for k,v in res.items() if any(a[2]&4 for a in v)}
grav={k for k,v in res.items() if any(a[0].startswith('GravityMkr') for a in v)}
print('with flag4',len(hasf4),'without',len(res)-len(hasf4),sorted(set(res)-hasf4))
print('no GravityMkr',len(res)-len(grav),'grav without flag4',sorted(grav-hasf4),'flag4 without grav',sorted(hasf4-grav))
gt=sum(1 for v in res.values() for a in v if a[0].startswith('GravityMkr'))
g4=sum(1 for v in res.values() for a in v if a[0].startswith('GravityMkr') and a[2]&4)
print('Gravity',gt,'flag4',g4,'others flag4',[(k,a[0],hex(a[2])) for k,v in res.items() for a in v if a[2]&4 and not a[0].startswith('GravityMkr')])
print('grav lacking',[(k,a[0],hex(a[2])) for k,v in res.items() for a in v if a[0].startswith('GravityMkr') and not a[2]&4])
print('Entry flags',collections.Counter(hex(a[2]) for v in res.values() for a in v if a[0].startswith('EntryMkr')))
print('flag0x10 by name',collections.Counter(re.sub(r'\d+.*$','',a[0]) for v in res.values() for a in v if a[2]&0x10).most_common(8))
rj=json.load(open('build/ed-db/rooms.json'))
short=[(k,len(rj[str(k)]['markers']),len(v)) for k,v in res.items() if len(rj[str(k)]['markers'])!=len(v)]
print('rooms.json != table',len(short),short[:40])
print('total rooms.json markers',sum(len(r['markers']) for r in rj.values()),'table',sum(len(v) for v in res.values()))
for path in sorted(glob.glob('saves/*.state')):
    if not os.path.basename(path).startswith(('slot','coop')): continue
    s=savestate.State(path)
    cur=s.s32(s.r13-23276); room=s.u32(0x8058AFC0+(s.u32(0x8058AFC0)<<2)+12); t=room+160; n=s.u16(t+52); rec=s.u32(t+56)
    v=res.get(cur,[]); bad=sum(1 for i in range(min(n,len(v))) if s.cstr(rec+76*i,40)!=v[i][0] or s.u32(rec+76*i+40)!=v[i][1] or s.u32(rec+76*i+72)!=v[i][2])
    print(os.path.basename(path),'room',cur,'records',n,'disc',len(v),'differ',bad)
