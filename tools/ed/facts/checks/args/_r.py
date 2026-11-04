import sys,struct,re,json,os; sys.path.insert(0,'tools/ed')
import disc,gametext
_d=disc.Disc()
CACHE='tools/ed/facts/checks/args/_rooms'
def room(n):
    os.makedirs(CACHE,exist_ok=True)
    c='%s/%04d.bin'%(CACHE,n)
    if os.path.exists(c): return open(c,'rb').read()
    b=_d.read('Rooms/rmed%04d.bpe'%n)
    first=struct.unpack('>I',b[4:8])[0]; csize=struct.unpack('>I',b[12:16])[0]
    x=gametext.bpe_expand(b,first,min(first+csize,len(b)))
    open(c,'wb').write(x)
    return x
def header(n):
    b=_d.read('Rooms/rmed%04d.bpe'%n)
    return struct.unpack('>8I',b[:32])
