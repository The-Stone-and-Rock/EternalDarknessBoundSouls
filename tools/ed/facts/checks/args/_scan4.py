import sys; sys.path.insert(0,'tools/ed/facts/checks/args')
from _r import *
x=room(153)
ms=[m.start() for m in re.finditer(rb'SpawnMkr\d+',x)]
print(len(ms),hex(ms[0]),hex(ms[-1]), ms[1]-ms[0], ms[2]-ms[1])
def hexd(a,n):
    for i in range(a,a+n,16):
        ln=x[i:i+16]; print('%06x'%i,ln.hex(' '),''.join(chr(c) if 32<=c<127 else '.' for c in ln))
hexd(ms[3]-32,160)
print()
# tag appears within spawn-marker region?
lo,hi=ms[0]-200,ms[-1]+400
for tag in (2154,645,849,851,850,882,883,340,282):
    print(tag,[(hex(m.start())) for m in re.finditer(re.escape(struct.pack('>H',tag)),x) if lo<m.start()<hi])
print(hex(lo),hex(hi),hex(header(153)[4]))
