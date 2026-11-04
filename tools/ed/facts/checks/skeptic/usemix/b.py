import sys,struct,json
sys.path.insert(0,'tools/ed')
import disc,savestate
D=disc.Disc()
U=D.read('InvU.bin')
n=struct.unpack('>I',U[0:4])[0]
ids=[]
for i in range(n):
    off,size=struct.unpack('>II',U[8+i*8:16+i*8])
    h=struct.unpack('>4I',U[off:off+16])
    ids.append(h)
print(ids[:6],ids[-3:])
print('hdr[0]==i+1',all(h[0]==i+1 for i,h in enumerate(ids)))
print('seq',[ (i,h[0]) for i,h in enumerate(ids) if h[0]!=i+1][:20])
print(set(h[2:] for h in ids))
