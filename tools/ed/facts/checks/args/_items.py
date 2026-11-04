import sys,struct,json; sys.path.insert(0,'tools/ed')
import savestate
s=savestate.State(sys.argv[1])
h=s.sda(-21468)
n=s.u32(h+4)
while n:
    d36=s.u32(n+36); n44=s.u32(n+44)
    if n44:
        d=s.u32(n44+4)
        print('id',s.s32(n+16),'type',s.s32(n+8),'room',s.s32(s.u32(n+52)+592) if s.u32(n+52) else None,'tag',s.s32(d36+24) if d36 else None,'| n44',hex(n44),'[0]',hex(s.u32(n44)),'[8]',hex(s.u32(n44+8)),'[12]',s.s32(n44+12),'[16]',hex(s.u32(n44+16)),'[20]',hex(s.u32(n44+20)),'[24]',hex(s.u32(n44+24)),'| def',hex(d),'+76/77',s.bytes(d+76,1)[0],s.bytes(d+77,1)[0],'+78..85',list(s.bytes(d+78,8)), 'def[0]', hex(s.u32(d)), s.cstr(d,16))
    n=s.u32(n+4)
