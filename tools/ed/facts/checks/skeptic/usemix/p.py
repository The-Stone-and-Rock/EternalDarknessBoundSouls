from lib import *
import glob
def TF(t):
    d=S[t][0x60:]
    return dict(set=struct.unpack('>i',d[0:4])[0],flags=struct.unpack('>I',d[16:20])[0],modes=struct.unpack('>I',d[20:24])[0],w24=struct.unpack('>I',d[24:28])[0],cls=struct.unpack('>H',d[28:30])[0])
print('tmpl 10',TF(10),'51',TF(51),'0',TF(0),'17',TF(17))
print('InvE',[ (i,e[:40].hex()) for i,e in enumerate(E)])
paths=sorted(glob.glob('saves/*.state'))+sorted(glob.glob('build/runs/*.state'))
for p in paths:
    try: s=savestate.State(p)
    except Exception as e: print(p,e);continue
    if s.sda(-21492)<0x80000000: continue
    N=nodes(s)
    print('==',p,'room',s.s32(s.r13-23276),'pid',s.sda(-21496),'ctrl',hex(s.sda(-21484)))
    for tag,iid in carried_ids(s,s.sda(-21492))[1:]:
        nd=N.get(iid)
        if nd:
            o=s.u32(nd+44);t0=s.u32(o);print('   ',iid,'kind',s.u32(o+12),name(s.u32(o+12))[2][:24],'t',s.u32(o+8),'setptr',hex(s.u32(t0)),'mask',s.u32(s.u32(t0)),'w24',s.u32(t0+24), 'l20',s.u32(o+16),s.u32(o+20),s.u32(o+24))
    a=0x80588DF0
    for j in range(20):
        hid=s.s32(a)
        if hid:
            cnt=s.u32(a+388)
            print('   eq#%d holder %d count %d ents %s +392 %#x +396 %#x +400 %#x +404 %#x'%(j,hid,cnt,[tuple(s.u32(a+4+12*x+y) for y in(0,4,8)) for x in range(min(cnt,4))],s.u32(a+392),s.u32(a+396),s.u32(a+400),s.u32(a+404)))
        a+=408
