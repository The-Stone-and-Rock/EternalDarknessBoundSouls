import sys,struct,re,json,collections
sys.path.insert(0,'tools/ed');sys.path.insert(0,'tools/ed/facts/checks/args')
import disc,gametext,cam_all
D=disc.Disc()
LE=lambda x,o:struct.unpack('<I',x[o:o+4])[0]
BE=lambda x,o:struct.unpack('>I',x[o:o+4])[0]
rooms=[147,1,2,3,5,175,20,74,189,1030]
for n in rooms:
    p='Rooms/rmed%04d.bpe'%n
    if p not in D.files:
        print(n,'no file');continue
    b=D.read(p)
    first,csize,sec1=BE(b,4),BE(b,12),BE(b,16)
    x=gametext.bpe_expand(b,first,min(first+csize,len(b)))
    c=cam_all.U16(x,162)
    h=cam_all.find_array(x,c,sec1 or len(x))
    print(n,'count',c,'hits',h[:4],'LE164',hex(LE(x,164)),'LE172',hex(LE(x,172)),'hdrwords',[hex(BE(b,o)) for o in (8,12,16,20,24,28)],'len',hex(len(x)))
    if len(h)>=1:
        print('   K',[hex((LE(x,164)&0xFFFFFF)-e) for e in h[:4]])
    print('   ptr fields',[(hex(o),hex(LE(x,o))) for o in (0xc,0x2c,0x34,0x3c,0x50,0x60,0x68,0x70,0x78,0xa4,0xac,0xb4,0xbc,0xd8,0xfc,0x104,0x11c)])
