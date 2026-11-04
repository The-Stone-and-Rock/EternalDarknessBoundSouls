import sys,struct,re
sys.path.insert(0,'tools/ed');sys.path.insert(0,'tools/ed/facts/checks/args')
import disc,gametext,cam_all as c
D=disc.Disc()
for n in (175,26,46,209,337,1255):
    b=D.read('Rooms/rmed%04d.bpe'%n)
    first,csize,sec1=c.U32(b,4),c.U32(b,12),c.U32(b,16)
    x=gametext.bpe_expand(b,first,min(first+csize,len(b)))
    cnt=c.U16(x,162)
    h=c.find_array(x,cnt,sec1 or len(x))
    print(n,'count',cnt,'want',c.LE(x,172)-c.LE(x,164),'next',c.LE(x,180)-c.LE(x,172) if c.LE(x,180) else 0)
    for s in h[:8]:
        names=[x[s+j*120+56:s+j*120+80].split(b'\0')[0] for j in range(cnt)]
        print('  ',s,names,[round(c.F32(x,s+j*120+36),1) for j in range(cnt)],'tchain',c.chain(x,s+c.LE(x,172)-c.LE(x,164),cnt))
    if n==175:
        for m in re.finditer(rb'(?i)cam[A-Za-z0-9_]*\0',x[:sec1]):
            e=m.start()
            print('   name at',e,x[max(0,e-16):e+24])
            if e>0 and sum(1 for _ in [0])>40:break
