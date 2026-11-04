from lib import *
d=Dol.data
for o,a,s,i in Dol.sections:
    b=d[o:o+s]
    for off in range(0,s-3,4):
        w=struct.unpack('>I',b[off:off+4])[0]
        if w in(0x80008234,):
            print('word at',hex(a+off),'sec',i,'->',hex(w),[hex(struct.unpack('>I',b[off+4*k:off+4*k+4])[0]) for k in range(-3,4)])
