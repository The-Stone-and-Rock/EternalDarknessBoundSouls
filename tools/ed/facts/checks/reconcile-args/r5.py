import sys,struct,re
sys.path.insert(0,'tools/ed');sys.path.insert(0,'tools/ed/facts/checks/args')
import disc,gametext,cam_all as c
D=disc.Disc()
b=D.read('Rooms/rmed0175.bpe')
first,csize,sec1=c.U32(b,4),c.U32(b,12),c.U32(b,16)
x=gametext.bpe_expand(b,first,min(first+csize,len(b)))
for e in range(885376-56-240,885376-56+120*10,120):
    print(e,x[e+52:e+80],c.U16(x,e+96),c.U16(x,e+104),c.U16(x,e+112),round(c.F32(x,e+36),1),c.U32(x,e+100),c.U32(x,e+108),c.U32(x,e+116),struct.unpack('>3h',x[e:e+6]))
