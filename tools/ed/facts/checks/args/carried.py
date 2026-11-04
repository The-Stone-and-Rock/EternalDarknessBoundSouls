import sys,struct,json; sys.path.insert(0,'tools/ed')
import savestate
s=savestate.State(sys.argv[1])
head=s.sda(-21468)
cur=s.sda(-21484); print('r13-21484 cur',hex(cur),'-21488',hex(s.sda(-21488)),'-21496 who',s.sda(-21496),'-21492',hex(s.sda(-21492)))
def thing(id):
    n=s.u32(head+4)
    while n:
        if s.s32(n+16)==id: return n
        n=s.u32(n+4)
n=s.u32(head+4)
while n:
    if s.s32(n+16)==1 or s.s32(n+8)==2:
        print('player thing',hex(n),'id',s.s32(n+16),'type',s.s32(n+8),'n48',hex(s.u32(n+48)))
    n=s.u32(n+4)
m=s.u32(cur+48)
print('carried head',hex(m))
i=0
while m and i<80:
    w=[s.u32(m+o) for o in (0,4,8,12,16,20,24)]
    print(i,hex(m),[hex(x) for x in w], end=' ')
    t=thing(s.s32(m+8))
    if t:
        n44=s.u32(t+44)
        print('thing type',s.s32(t+8),'n44',hex(n44),'kind',s.s32(n44+12) if n44 else None,end='')
    print()
    m=s.u32(m+4); i+=1
