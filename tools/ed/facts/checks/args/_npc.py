import sys,struct; sys.path.insert(0,'tools/ed')
import disc
D=disc.Disc()
b=D.read('Npcs3.npc'); print(len(b),b[:8])
print([hex(x) for x in struct.unpack('>20I',b[:80])])
for tag in (2154,2156,2155,2028,645,481,849):
    import re
    print(tag,[hex(m.start()) for m in re.finditer(re.escape(struct.pack('>H',tag)),b)][:8])
