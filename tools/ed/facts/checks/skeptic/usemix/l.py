from lib import *
import re,glob,collections
b=D.read('ai/ed_chrs.dat')
C={}
for i in range((len(b)-0x20)//224):
    r=b[0x20+224*i:0x20+224*(i+1)]
    C[struct.unpack('>I',r[4:8])[0]]=struct.unpack('>H',r[136:138])[0]
def TF(t):
    d=S[t][0x60:]
    return dict(flags=struct.unpack('>I',d[16:20])[0],modes=struct.unpack('>I',d[20:24])[0],w24=struct.unpack('>I',d[24:28])[0],cls=struct.unpack('>H',d[28:30])[0])
# verify InvS vs memory
st=states()
bad=0;tot=0
ties=collections.defaultdict(set) # kind->set((tmpl,src))
for n,s in st.items():
    N=nodes(s)
    for iid,nd in N.items():
        o=s.u32(nd+44)
        if not 0x80000000<=o<0x81800000: continue
        rec=s.u32(o+4);kind=s.u32(o+12);tm=s.u32(o+8);t0=s.u32(o)
        if not 0x80000000<=rec<0x81800000 or not 0<kind<244 or not 0<=tm<55 or not 0x80000000<=t0<0x81800000: continue
        m=s.bytes(t0,32); f=S[tm][0x60:0x60+32]
        tot+=1
        if m[8:]!=f[8:]:
            bad+=1; print('tmpl mismatch',n,iid,kind,tm,m.hex(),f.hex())
        ties[kind].add((tm,'live:'+n))
print('tmpl vs mem',tot,'bad',bad)
for p in glob.glob('build/ed-db/scripts/L*/*.lua'):
    t=open(p,encoding='utf8',errors='replace').read()
    for m in re.finditer(r'ed3\((\d+), *[^,]+, *[^,]+, *(\d+),',t):
        ch=int(m.group(1));k=int(m.group(2))
        if ch in C and k<244: ties[k].add((C[ch],'ed3:'+os.path.basename(p)+':chr%d'%ch))
pl=json.load(open('tools/ed/facts/checks/skeptic/usemix/placed.json'))
for k,v in pl.items():
    for e in v:
        if int(k)>=0 and int(k)<244 and e[3] in C: ties[int(k)].add((C[e[3]],'npc:'+e[0]+':chr%d'%e[3]))
weap=[k for k,r in enumerate(U) if r[76]==3 and s16(r,42)]
ammo=[k for k,r in enumerate(U) if r[76]==4 and s16(r,42)]
print('all kinds group 3 count',sum(1 for r in U if r[76]==3),'group 4',sum(1 for r in U if r[76]==4))
def show(k):
    r=U[k]
    tp=collections.defaultdict(list)
    for t,src in ties.get(k,()): tp[t].append(src)
    out={t:(hex(TF(t)['cls']),sorted(set(x.split(':')[0]+(':'+x.split(':')[1] if x.startswith('npc') else '') for x in v))) for t,v in tp.items()}
    return '%3d %-28s grp %d mix %4d count %d cap %d  %s'%(k,name(k)[2][:28],r[76],s16(r,42),s16(r,68),s16(r,70),out)
print('--weapons');[print(show(k)) for k in weap]
print('--ammo');[print(show(k)) for k in ammo]
print('--weapon/ammo with no mix')
for k,r in enumerate(U):
    if r[76] in(3,4) and not s16(r,42): print(show(k))
print('templates with cls');
for t in range(55):
    f=TF(t)
    if f['cls']: print(t,{a:hex(b) for a,b in f.items()},[c for c,v in C.items() if v==t])
