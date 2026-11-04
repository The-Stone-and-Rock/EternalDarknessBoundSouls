import sys; sys.path.insert(0,'tools/ed')
import savestate,collections
s=savestate.State(sys.argv[1])
base=0x806068A8
out=[]
for t in range(0,100):
    w=[s.u32(base+t*16+o) for o in (0,4,8,12)]
    if w[0]: out.append((t,[hex(x) for x in w]))
for x in out: print(x)
