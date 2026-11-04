import sys,struct,re,collections,json
sys.path.insert(0,'tools/ed')
import disc,gametext,savestate
d=disc.Disc()
paths=d.glob(r'Rooms/.*\.bpe')
print(len(paths))
def expand(path):
    r=d.read(path)
    first=struct.unpack('>I',r[4:8])[0];csize=struct.unpack('>I',r[12:16])[0];sec1=struct.unpack('>I',r[16:20])[0]
    return bytes(gametext.bpe_expand(r,first,min(first+csize,len(r)))),sec1
# test whether +216 is an offset to the record table
ok=bad=zero=0;bads=[]
sample=None
res={}
for p in paths:
    rid=int(p[-8:-4])
    x,sec1=expand(p)
    n=struct.unpack('>H',x[212:214])[0]
    off=struct.unpack('>I',x[216:220])[0]
    if n==0: zero+=1; res[rid]=[]; continue
    good=off and off+76*n<=len(x) and all(32<=x[off+76*i]<127 for i in range(n))
    if good: ok+=1
    else: bad+=1; bads.append((rid,n,hex(off),len(x)))
    if good:
        res[rid]=[(x[off+76*i:off+76*i+40].split(b'\0')[0].decode('latin1'),struct.unpack('>I',x[off+76*i+40:off+76*i+44])[0],struct.unpack('>I',x[off+76*i+72:off+76*i+76])[0]) for i in range(n)]
print('rooms',len(paths),'ok',ok,'bad',bad,bads[:10],'zero',zero)
json.dump(res,open('tools/ed/facts/checks/reconcile-trans/mk.json','w'))
# compare with S2's parse
try:
    mk=json.load(open('build/ed-db/facts/skeptic/trans/markers2.json'))
    diff=[k for k,v in mk.items() if [r['name'] for r in v['recs']]!=[a[0] for a in res.get(int(k),[])]]
    print('differs from skeptic parse',diff[:20],len(diff))
except Exception as e: print(e)
hasf4={k for k,v in res.items() if any(a[2]&4 for a in v)}
grav={k for k,v in res.items() if any(a[0].startswith('GravityMkr') for a in v)}
print('rooms',len(res),'with flag4',len(hasf4),'without',len(res)-len(hasf4),sorted(set(res)-hasf4))
print('no GravityMkr',len(res)-len(grav),'grav without flag4',sorted(grav-hasf4),'flag4 without grav',sorted(hasf4-grav))
gt=sum(1 for v in res.values() for a in v if a[0].startswith('GravityMkr'))
g4=sum(1 for v in res.values() for a in v if a[0].startswith('GravityMkr') and a[2]&4)
print('Gravity',gt,'flag4',g4,'others flag4',[(k,a[0],hex(a[2])) for k,v in res.items() for a in v if a[2]&4 and not a[0].startswith('GravityMkr')])
print('grav lacking',[(k,a[0],hex(a[2])) for k,v in res.items() for a in v if a[0].startswith('GravityMkr') and not a[2]&4])
rj=json.load(open('build/ed-db/rooms.json'))
short=[(k,len(rj[str(k)]['markers']),len(v)) for k,v in res.items() if str(k) in rj and len(rj[str(k)]['markers'])!=len(v)]
print('rooms.json != table',len(short),short[:40])
print('rooms in rooms.json',len(rj),'rooms on disc',len(res))
for sl in ['slot1','slot2','slot3','slot4','slot5','slot6','slot7','slot8','slot9','slot10']:
    s=savestate.State('saves/%s.state'%sl)
    cur=s.s32(s.r13-23276); room=s.u32(0x8058AFC0+12); t=room+160; n=s.u16(t+52); rec=s.u32(t+56)
    v=res[cur]; bad=sum(1 for i in range(n) if s.cstr(rec+76*i,40)!=v[i][0] or s.u32(rec+76*i+40)!=v[i][1] or s.u32(rec+76*i+72)!=v[i][2])
    print(sl,'room',cur,'records',n,'disc',len(v),'differ',bad)
