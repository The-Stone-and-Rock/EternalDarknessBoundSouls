import sys; sys.path.insert(0,'tools/ed/facts/checks/args')
from _r import *
x=room(153); print(len(x),[hex(v) for v in header(153)])
print(x[:64].hex())
for tag in (2154,645,849,851,850):
    b=struct.pack('>I',tag)
    print(tag,[hex(m.start()) for m in re.finditer(re.escape(b),x)][:20], [hex(m.start()) for m in re.finditer(re.escape(struct.pack('>H',tag)),x)][:10])
