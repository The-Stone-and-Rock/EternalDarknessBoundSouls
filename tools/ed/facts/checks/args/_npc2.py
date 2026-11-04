import sys,struct,re,json; sys.path.insert(0,'tools/ed')
import disc
D=disc.Disc()
def parse(n):
    b=D.read('Npcs%d.npc'%n)
    h=struct.unpack('>11I',b[:44])
    assert h[0]==0x2c
    at=0x2c
    secs=[]
    for c,sz in ((h[1],h[2]),(h[3],h[4]),(h[5],h[6]),(h[7],h[8]),(h[9],h[10])):
        secs.append((c,sz,at)); at+=sz
    return b,secs
if __name__=='__main__':
    b,secs=parse(int(sys.argv[1]))
    print(secs,len(b))
    c,sz,at=secs[2]
    for i in range(c):
        r=b[at+i*44:at+(i+1)*44]
        print(i,'pos',struct.unpack('>3h',r[:6]),'room',struct.unpack('>h',r[6:8])[0],'+8',r[8:12].hex(),'axis',[round(v,2) for v in struct.unpack('>3f',r[12:24])],'ang',round(struct.unpack('>f',r[24:28])[0],2),'+28',struct.unpack('>6H',r[28:40]),'tag',struct.unpack('>H',r[40:42])[0],'+42',r[42:44].hex())
    for name,idx,st in (('S1',1,104),('S0',0,40),('S3',3,52),('S4',4,60)):
        c,sz,at=secs[idx]; print(name,c,sz/ c if c else 0)
        for i in range(min(c,6)):
            r=b[at+i*st:at+(i+1)*st]; print(' ',i,struct.unpack('>4h',r[:8]),r[8:].hex())
