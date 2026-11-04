"""Tome slots: each block is 264 bytes = 6 slots of 44 (clears at 0x80102880 to 0x801028CC). Counts link rate per block, all states."""
import sys,glob,os,collections,json
HERE=os.path.dirname(os.path.abspath(__file__));ROOT=os.path.abspath(os.path.join(HERE,'..','..','..'))
sys.path.insert(0,os.path.join(ROOT,'tools','ed'));os.chdir(ROOT)
import savestate
c=collections.Counter();bad=[]
for p in sorted(glob.glob('**/*.state',recursive=True)):
    s=savestate.State(p);head=s.sda(-21468)
    if not (0x80000000<=head<0x81800000) or not s.u32(head+4): continue
    n=s.u32(head+4);nodes={}
    while n and len(nodes)<3000: nodes[s.u32(n+16)]=n;n=s.u32(n+4)
    for lb in (0,264,528,792,1056):
        for i in range(6):
            a=0x80334388+lb+i*44;info=s.u32(a+36);h=s.u32(a+40)
            if info and 0x80000000<=info<0x81800000:
                c[lb,'all']+=1
                if h in nodes and s.u32(nodes[h]+44)==info: c[lb,'ok']+=1
                else: bad.append((p,lb,i,h,hex(info),h in nodes))
print(sorted(c.items()));print(len(bad))
for b in bad[:40]: print(b)
