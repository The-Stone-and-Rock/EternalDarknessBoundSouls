import sys,struct,re;sys.path.insert(0,'tools/ed');import disc,gametext,savestate
D=disc.Disc()
b=D.read('Rooms/rmed0147.bpe')
first,csize,sec1=struct.unpack('>I',b[4:8])[0],struct.unpack('>I',b[12:16])[0],struct.unpack('>I',b[16:20])[0]
x=gametext.bpe_expand(b,first,min(first+csize,len(b)))
i=x.find(b'\x21\xB0\x00\x00') 
names=[m.start() for m in re.finditer(rb'Camera01\0',x)]
print('Camera01 at',names,[hex(n-56) for n in names])
s=savestate.State('saves/slot1.state')
rs=s.u32(0x80590000-20544+s.u32(0x80590000-20544)*4+12)
e0=s.u32(rs+164)
live=s.bytes(e0,120)
print(live[:60].hex())
j=x.find(live[:56]); print('live entry in x',j,hex(j))
e=j
print('rs in x?',x.find(s.bytes(rs,12)))
print([hex(v) for v in struct.unpack('>I',x[e+100:e+104])+struct.unpack('>I',x[e+108:e+112])+struct.unpack('>I',x[e+116:e+120])])
print('hdr',x[160:176].hex(), 'live',s.bytes(rs+160,16).hex())
# How far apart in live vs file
print('rel live',hex(e0-rs),'file',hex(j))
# search for count word positions: what's before eye array in file
print('before',x[j-16:j].hex())
for o in range(0,0x400,4):
    v=struct.unpack('>I',x[o:o+4])[0]
    w=s.u32(rs+o)
    if w and v!=w and 0x80000000<=w<0x81800000 and o<300:
        print(hex(o),'file',hex(v),'live',hex(w),'liverel',hex(w-rs))
