"""python -I tools/ed/facts/checks/reconcile-usemix/equip.py : equipment records, the InvU table in memory, urn nodes, r2 doubles. Reads states only."""
import sys,struct,glob
sys.path.insert(0,'tools/ed/facts/checks/reconcile-usemix');sys.path.insert(0,'tools/ed');sys.path.insert(0,'tools/ed/facts')
import savestate,disc
from live import nodes,carried,inmem,name
import usemix_inv
U=usemix_inv.load()
for f in ['saves/slot1.state','saves/slot3.state','saves/slot7.state','saves/slot10.state','saves/coop-apart.state','build/runs/s1.state']:
    s=savestate.State(f);N=nodes(s)
    a=0x80588DF0
    print(f,'room',s.sda(-23276))
    for j in range(20):
        h=s.s32(a)
        if h>0:
            c=s.u32(a+388)
            print('   holder',h,'count',c,[tuple(s.u32(a+4+12*x+y) for y in (0,4,8)) for x in range(c)],'+392',s.u32(a+392),'+396',s.u32(a+396),'+400 %08X +404 %08X'%(s.u32(a+400),s.u32(a+404)))
        a+=408
    t=s.sda(-23412)
    bad=sum(1 for k in range(244) if s.bytes(s.u32(t+8+k*8)+16,88)!=U[k])  # in memory the offsets are fixed up to pointers
    print('   r13-23412 %08X'%t,'head',struct.unpack('>II',s.bytes(t,8)),'records unlike the file:',bad)
    for i in (9,10,11):
        nd=N.get(i)
        if nd and f.endswith('s1.state'):
            o=s.u32(nd+44);print('   id',i,'kind',s.u32(o+12),'template',s.u32(o+8),'t+24',s.u32(s.u32(o)+24),'class',s.u16(s.u32(o)+28),'count',s.u16(s.u32(o+4)+68),'cap',s.u16(s.u32(o+4)+70),'+20,+24',s.u32(o+20),s.u32(o+24))
for f in ['saves/slot1.state','saves/slot2.state','saves/slot3.state','saves/slot4.state','saves/slot5.state','saves/slot9.state']:
    s=savestate.State(f);N=nodes(s);p=s.sda(-21492);c=carried(s,p)
    r={}
    for i in (65,66,67,105,106,11,12,13):
        nd=N.get(i)
        r[i]=(s.u32(s.u32(nd+44)+12) if nd and inmem(s.u32(nd+44)) else None, i in c)
    print(f,'nodes (kind, carried):',r)
s=savestate.State('saves/slot1.state')
r2=0x80628460
print('doubles',{o:struct.unpack('>d',s.bytes(r2+o,8))[0] for o in (-32520,-32512,-32504,-32496,-32488,-32656,-32584)})
for f,ids in [('saves/slot1.state',(1498,1582,2013,2221,1429,1359)),('build/runs/s1.state',(1498,1582,2013,2221,1874,1359)),('saves/coop-apart.state',(2013,1582,2324,1498))]:
    s=savestate.State(f);t=s.sda(-23212)
    print(f,'script sizes in the loaded table r13-23212',{i:s.u32(t+(2*i+2)*4+4) for i in ids})
s=savestate.State('saves/coop-apart.state');N=nodes(s)
for i in range(9,18):
    o=s.u32(N[i]+44);r=s.u32(o+4);print('coop-apart id',i,'kind',s.u32(o+12),'use',s.u16(r+38),'count',s.u16(r+68))
