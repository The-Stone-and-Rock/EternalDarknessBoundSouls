import sys,glob,json,collections
sys.path.insert(0,'tools/ed');import savestate
P=json.load(open('build/ed-db/facts/placed_things.json'))
def live(s):
    h=s.sda(-21468); n=s.u32(h+4); d={}
    while n:
        i=s.u32(n+36)
        if i:
            o=s.u32(n+52)
            d.setdefault(s.s32(i+24),[]).append((s.s32(n+8),s.s32(n+16),s.s32(o+592) if o else None))
        n=s.u32(n+4)
    return d
for f in sorted(glob.glob('saves/*.state')):
    s=savestate.State(f)
    if not s.sda(-21468): continue
    ch=s.s32(0x80307178)
    d=live(s)
    rows=[r for r in P.get(str(ch),[]) if r['section']==0 and r.get('type')==44]
    print(f[6:],'ch',ch,'byte6425',s.bytes(0x80307170+6425,1).hex(),'byte6424',s.bytes(0x80307170+6424,1).hex(),'type44 records',len(rows),'live',sum(r['tag'] in d for r in rows),'nodes type 44',sum(1 for v in d.values() for t in v if t[0]==44))
c=collections.Counter()
for L,rows in P.items():
    for r in rows:
        if r['section']==0: c[(int(L),r.get('type'))]+=1
print(sorted(k for k in c if k[1]==44), c.most_common(5))
