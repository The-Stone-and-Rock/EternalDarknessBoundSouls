import sys,struct,re;sys.path.insert(0,'tools/ed');import disc,gametext,savestate
D=disc.Disc()
ps=D.glob(r'^Rooms/rmed0*147\.bpe$'); print(ps)
b=D.read(ps[0])
print(b[:32].hex())
first,csize,sec1=struct.unpack('>I',b[4:8])[0],struct.unpack('>I',b[12:16])[0],struct.unpack('>I',b[16:20])[0]
x=gametext.bpe_expand(b,first,min(first+csize,len(b)))
print(len(x),first,csize,sec1)
s=savestate.State('saves/slot1.state')
rs=s.u32(0x80590000-20544+s.u32(0x80590000-20544)*4+12)
print('room struct',hex(rs),'count',s.u16(rs+162),'eyes',hex(s.u32(rs+164)),'targets',hex(s.u32(rs+172)))
print('live hdr',s.bytes(rs,32).hex())
print('file hdr',x[:32].hex())
e0=s.u32(rs+164);t0=s.u32(rs+172)
print('rel',hex(e0-rs),hex(t0-rs))
print('file hdr162',x[162:164].hex(),x[164:168].hex(),x[172:176].hex())
print(s.cstr(e0+56))
print('file at off',x[e0-rs+56:e0-rs+70] if e0-rs<len(x) else None)
n=s.u16(rs+162)
for i in range(3):
    e=e0+i*120
    print(i,s.cstr(e+56),'n96',s.u16(e+96),hex(s.u32(e+100)),'n104',s.u16(e+104),hex(s.u32(e+108)),'n112',s.u16(e+112),hex(s.u32(e+116)))
    fe=e-rs
    print('  file',x[fe+56:fe+66],struct.unpack('>H',x[fe+96:fe+98]),hex(struct.unpack('>I',x[fe+100:fe+104])[0]),struct.unpack('>H',x[fe+104:fe+106]),hex(struct.unpack('>I',x[fe+108:fe+112])[0]),struct.unpack('>H',x[fe+112:fe+114]),hex(struct.unpack('>I',x[fe+116:fe+120])[0]))
