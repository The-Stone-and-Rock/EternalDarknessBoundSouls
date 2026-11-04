from lib import *
import glob
paths=sorted(glob.glob('saves/*.state'))+sorted(glob.glob('build/runs/*.state'))+sorted(glob.glob('logs/saved-changes/*/saves/*.state'))
print(len(paths))
for p in paths:
    try: s=savestate.State(p)
    except Exception as e: continue
    N=nodes(s)
    for iid,nd in N.items():
        o=s.u32(nd+44)
        if not 0x80000000<=o<0x81800000: continue
        rec=s.u32(o+4);kind=s.u32(o+12);tm=s.u32(o+8)
        if not 0x80000000<=rec<0x81800000 or not 0<kind<244 or not 0<=tm<55: continue
        a,b=s.u32(o+20),s.u32(o+24)
        if a or b: print(p,'id',iid,'kind',kind,name(kind)[2],'+20',a,'+24',b)
