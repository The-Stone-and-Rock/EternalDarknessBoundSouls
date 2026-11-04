import sys,struct,re,json; sys.path.insert(0,'tools/ed'); sys.path.insert(0,'tools/ed/facts/checks/args')
from _npc2 import parse,D as DISC
def H(r,o): return struct.unpack('>H',r[o:o+2])[0]
def S(r,o): return struct.unpack('>h',r[o:o+2])[0]
out={}
for p in sorted(DISC.glob(r'^Npcs\d+\.npc$'),key=lambda p:int(re.search(r'(\d+)',p).group(1))):
    n=int(re.search(r'Npcs(\d+)',p).group(1))
    b,secs=parse(n)
    rows=[]
    for si,(c,sz,at) in enumerate(secs):
        st=sz//c if c else 0
        for i in range(c):
            r=b[at+i*st:at+(i+1)*st]
            o={'section':si,'index':i,'pos':struct.unpack('>3h',r[:6]),'room':S(r,6)}
            if si==0:
                o.update(model=H(r,24),chr=H(r,26),rec28=S(r,28),rec30=S(r,30),spawn40=H(r,32),tag=H(r,34),mask=r[37])
            elif si==1:
                o.update(chr=H(r,76),model=H(r,78),spawn40=H(r,80),rec84=S(r,84),tag=H(r,98),rec100=H(r,100))
            elif si==2:
                o.update(model=H(r,28),type_id=H(r,30),chr=H(r,32),rec34=S(r,34),spawn40=H(r,38),tag=H(r,40),mask=r[11])
            elif si==3:
                o.update(by_alignment=[dict(align=a,model=H(r,24+6*k),chr=H(r,26+6*k),spawn40=H(r,28+6*k)) for k,a in enumerate((1,2,3))],tag=H(r,46),mask=H(r,48))
            elif si==4:
                o.update(by_alignment=[dict(align=a,model=H(r,32+8*k),chr=H(r,34+8*k),spawn40=H(r,36+8*k),type_id=H(r,38+8*k)) for k,a in enumerate((1,2,3))],tag=H(r,56),mask=r[11])
            rows.append(o)
    out[str(n)]=rows
json.dump(out,open('build/ed-db/facts/placed_things.json','w'))
print({k:len(v) for k,v in out.items()})
# live check: each save against its chapter's file
sys.path.insert(0,'tools/ed'); import savestate,glob
for f in sorted(glob.glob('saves/*.state')):
    st=savestate.State(f)
    if not st.sda(-21468) or st.s32(st.r13-23276)<0: continue
    ch=str(st.s32(0x80307178))
    if ch not in out: continue
    n=st.u32(st.sda(-21468)+4); live={}
    while n:
        d=st.u32(n+36)
        if d:
            o=st.u32(n+52)
            live.setdefault(st.s32(d+24),[]).append((st.s32(n+8),st.s32(o+592) if o else None,st.s32(n+16)))
        n=st.u32(n+4)
    ok=0;miss=[]
    for r in out[ch]:
        t=r['tag']
        if t in (65535,0): continue
        if t in live and any(x[1]==r['room'] for x in live[t]): ok+=1
        else: miss.append((r['section'],t,r['room'],r.get('spawn40')))
    print(f[6:],'chapter',ch,'tagged records live in the same room',ok,'not',len(miss),miss[:12])
# every ed221 X anywhere
sys.path.insert(0,'tools/ed/facts/checks/args')
from calls import calls
tags={}
for L,rows in out.items():
    for r in rows: tags.setdefault(r['tag'],[]).append((L,r['room'],r['section'],r.get('type_id')))
xs=[(rel,a[4]) for rel,a in calls('ed221')]
found=[(rel,x,tags[int(x)]) for rel,x in xs if x.isdigit() and int(x) in tags]
nf=[(rel,x) for rel,x in xs if not(x.isdigit() and int(x) in tags)]
print(len(xs),len(found),nf)
same=[(rel,x) for rel,x,t in found if any(int(rel[1:3].replace('-',''))==int(l[0]) if rel[1:3].lstrip('-').isdigit() else False for l in t)]
print('same level',len(same))
