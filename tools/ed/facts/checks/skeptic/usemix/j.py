from lib import *
import re,glob,collections
print(D.glob(r'[Nn]pcs')[:30])
def npcs(p):
    b=D.read(p)
    if b[:4]==b'': return None
    return b
tot=collections.defaultdict(list)
for p in D.glob(r'Npcs\d+\.npc$'):
    b=D.read(p)
    h=struct.unpack('>I',b[:4])[0]
    pairs=struct.unpack('>10I',b[4:44])
    tabs={}
    at=0x2C
    for i in range(5):
        c,sz=pairs[i*2],pairs[i*2+1]
        tabs[i+1]=(at,c,sz)
        at+=sz
    # table 3
    a,c,sz=tabs[3]
    assert sz==c*44 or c==0,(p,tabs)
    for i in range(c):
        r=b[a+44*i:a+44*i+44]
        room=s16(r,6);typ=s16(r,30);chr_=s16(r,32);model=s16(r,28)
        tot[typ].append((p,3,room,chr_,model))
    a,c,sz=tabs[5]
    assert sz==c*60 or c==0,(p,tabs)
    for i in range(c):
        r=b[a+60*i:a+60*i+60]
        room=s16(r,6)
        for o in (32,40,48):
            model,ch,fl,typ=struct.unpack('>4h',r[o:o+8])
            tot[typ].append((p,5,room,ch,model,o))
    print(p,hex(h),{k:(v[1],v[2]) for k,v in tabs.items()},len(b),at)
for k in (209,210,211,212,121,122,35,188,36,37,87,85,139,135,136,95,229,243,91,233,137,138,84,82,150,84,83,109):
    print(k,name(k)[2],'placed',len(tot.get(k,[])),tot.get(k,[])[:5])
json.dump({str(k):v for k,v in tot.items()},open('tools/ed/facts/checks/skeptic/usemix/placed.json','w'))
