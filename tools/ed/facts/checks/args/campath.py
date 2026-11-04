import sys,struct,re,json; sys.path.insert(0,'tools/ed/facts/checks/args')
from _r import room
x=room(147)
for m in re.finditer(rb'Camera01\0',x):
    i=m.start(); e=i-56
    print('elem file offset',hex(e),'pos',struct.unpack('>3h',x[e:e+6]),'hash',hex(struct.unpack('>I',x[e+52:e+56])[0]),'+96..',x[e+96:e+120].hex())
    n=struct.unpack('>H',x[e+104:e+106])[0]; p=struct.unpack('>I',x[e+108:e+112])[0]
    print('n',n,'p',hex(p),'x[p]:',x[p:p+24].hex() if p<len(x) else None, 'x[e+0xB40]',x[e+0xB40:e+0xB40+24].hex())
    break
