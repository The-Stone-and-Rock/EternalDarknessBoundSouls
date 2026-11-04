# Checks args/cameras_all.json against live rooms in every save, and fn8's literal hashes against it.
# Run: python -I tools/ed/facts/checks/args/cam_check.py
import sys,glob,json,struct,re,collections
sys.path.insert(0,'tools/ed'); sys.path.insert(0,'tools/ed/facts/checks/args')
import savestate
from calls import calls
W=json.load(open('build/ed-db/facts/cameras_all.json'))
kit=json.load(open('build/ed-db/rooms.json'))
done=set()
for f in sorted(glob.glob('saves/*.state')):
    s=savestate.State(f)
    if not s.sda(-21468) or str(s.s32(s.r13-23276)) not in W: continue
    base=0x8058AFC0
    i=s.u32(base); rs=s.u32(base+i*4+12)
    room=s.s32(s.r13-23276)
    n=s.u16(rs+162); eye=s.u32(rs+164); tgt=s.u32(rs+172)
    row=W[str(room)]
    bad=0
    for k in range(n):
        e=eye+k*120; t=tgt+k*120; a=row['eyes'][k]; b=row['targets'][k]
        ok=(s.u32(e+52)==a['hash'] and s.u16(e+104)==a['n104'] and s.u16(e+112)==a['n112'] and list(struct.unpack('>3h',s.bytes(e,6)))==list(a['pos'])
            and s.u16(t+104)==b['n104'] and s.u16(t+112)==b['n112'] and list(struct.unpack('>3h',s.bytes(t,6)))==list(b['pos']))
        if a['n104']:
            p=s.u32(e+108); ok=ok and s.u32(p)==0 and list(struct.unpack('>3h',s.bytes(p+4,6)))==list(a['pos'])
            q=p+(a['n104']-1)*12; ok=ok and s.u32(q)==a['last_key'][0] and list(struct.unpack('>3h',s.bytes(q+4,6)))==list(a['last_key'][1:])
        if a['n112']:
            p=s.u32(e+116); ok=ok and s.u32(p+(a['n112']-1)*20)==a['rot_frames'][2]
        bad+=not ok
    print(f[6:],'room',room,'live count',n,'file count',row['count'],'cameras disagreeing',bad)
    done.add(room)
print('rooms checked live',sorted(done))
hs=collections.defaultdict(set)
for r,v in W.items():
    for c in v['eyes']: hs[c['hash']].add(int(r))
kh=set(c['hash'] for v in kit.values() for c in v['cameras'])
tot=lit=onwalk=inkit=0; missing=collections.Counter()
for rel,a in calls('fn8'):
    tot+=1
    if not a or not a[0].isdigit(): continue
    lit+=1; h=int(a[0])
    onwalk+=h in hs; inkit+=h in kh
    if h not in hs: missing[h]+=1
print('fn8 calls',tot,'literal hash',lit,'on the walk',onwalk,'in the kit',inkit,'literal hash on no room walk',sum(missing.values()),len(missing),missing.most_common(6))
