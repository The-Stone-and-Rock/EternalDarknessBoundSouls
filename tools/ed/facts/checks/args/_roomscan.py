import sys,struct,re,json; sys.path.insert(0,'tools/ed')
import disc,gametext
d=disc.Disc()
def room(n):
    p='Rooms/rmed%04d.bpe'%n
    return p
ps=d.glob(r'^Rooms/rmed\d+\.bpe$'); print(len(ps),ps[:3])
x=gametext.bpe_expand(d.read('Rooms/rmed0153.bpe') if 'Rooms/rmed0153.bpe' in ps else d.read(ps[0]))
print(len(x))
for tag in (2154,645,849):
    b=struct.pack('>H',tag)
    print(tag,[hex(m.start()) for m in re.finditer(re.escape(b),x)][:20])
open('build/ed-db/facts/_room153.bin','wb').write(x)
