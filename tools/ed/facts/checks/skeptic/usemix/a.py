import sys,struct,json
sys.path.insert(0,'tools/ed')
import disc,savestate
D=disc.Disc()
print([p for p in D.glob(r'Inv')])
U=D.read(D.glob(r'InvU')[0])
print(len(U),struct.unpack('>8I',U[:32]))
n=struct.unpack('>I',U[0:4])[0]
print('n',n)
recs=[]
for i in range(n):
    off,size=struct.unpack('>II',U[8+i*8:16+i*8])
    recs.append((off,size))
print(recs[:5],set(s for o,s in recs))
# check next file offset
print(U[recs[0][0]:recs[0][0]+16].hex())
print(U[recs[0][0]+16:recs[0][0]+16+88].hex())
print(len(U), recs[-1][0]+recs[-1][1])
