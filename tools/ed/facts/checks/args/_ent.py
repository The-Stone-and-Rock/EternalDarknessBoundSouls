import sys,struct; sys.path.insert(0,'tools/ed')
import savestate
s=savestate.State(sys.argv[1])
h=s.sda(-21468)
n=s.u32(h+4)
while n:
    d36=s.u32(n+36)
    if d36:
        print('id',s.s32(n+16),'type',s.s32(n+8),'n44',hex(s.u32(n+44)),'n48',hex(s.u32(n+48)),'n52',hex(s.u32(n+52)),'|',' '.join('%08x'%s.u32(d36+o) for o in range(0,56,4)), '| name?', s.bytes(d36,4))
    n=s.u32(n+4)
