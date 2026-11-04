import sys; sys.path.insert(0,'tools/ed')
import disc,ppcemu,struct,re
d=disc.Disc()
b=d.read('Levels/Level03/U_Level03.bin')
print(len(b),b[:16])
u=ppcemu.Unpacker('build/ed-db/disasm.txt').unpack(b)
print(len(u),u[:32].hex())
open('tools/ed/facts/checks/args/_U3.bin','wb').write(u)
for tag in (2154,645,2028,2156):
    print(tag,[hex(m.start()) for m in re.finditer(re.escape(struct.pack('>I',tag)),u)][:10],[hex(m.start()) for m in re.finditer(re.escape(struct.pack('>H',tag)),u)][:10])
