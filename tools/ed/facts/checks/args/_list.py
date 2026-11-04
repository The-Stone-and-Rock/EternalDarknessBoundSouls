import sys; sys.path.insert(0,'tools/ed')
import savestate
s=savestate.State(sys.argv[1])
print('r13',hex(s.r13))
h=s.sda(-21468); print('head',hex(h),'world-state? fn25',s.sda(-23276) , 'r13-23228', hex(s.sda(-23228)), 'cur', hex(s.sda(-21484)))
n=s.u32(h+4); i=0
while n and i<400:
    d36=s.u32(n+36); d52=s.u32(n+52)
    print(i,hex(n),'id',s.s32(n+16),'[8]',s.s32(n+8),'[20]',s.s32(n+20),'[24]',s.s32(n+24),'d36',hex(d36),'d36+24',s.s32(d36+24) if d36 else None,'d52',hex(d52),'d52.592',s.s32(d52+592) if d52 else None)
    n=s.u32(n+4); i+=1
