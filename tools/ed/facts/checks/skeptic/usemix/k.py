from lib import *
import re,glob,collections
print(D.glob(r'chrs'))
b=D.read(D.glob(r'ed_chrs')[0])
print(len(b),b[:32].hex(), (len(b)-0x20)/224)
C={}
for i in range((len(b)-0x20)//224):
    r=b[0x20+224*i:0x20+224*(i+1)]
    C[struct.unpack('>I',r[4:8])[0]]=struct.unpack('>I',r[136:140])[0]
print('chrs',len(C),'sample',{k:C[k] for k in (25,39,137,10,11) if k in C})
def tcls(t):
    r=S[t][0x10:] # see below
    return r
print('S rec size',set(len(x) for x in S), S[1][:32].hex())
# find data offset: InvS 332 bytes with data at +0x60 per report; test with live memory
st=states()
s=st['coop-apart']
N=nodes(s)
for iid in (9,10,12,13):
    o=s.u32(N[iid]+44); t0=s.u32(o); tm=s.u32(o+8)
    mem=s.bytes(t0,64)
    f=S[tm]
    at=f.find(mem[:32])
    print(iid,'tmpl',tm,'mem',mem[:32].hex(),'at in file',at, 'w16',hex(struct.unpack('>I',mem[16:20])[0]),'w20',hex(struct.unpack('>I',mem[20:24])[0]),'w24',hex(struct.unpack('>I',mem[24:28])[0]),'h28',hex(struct.unpack('>H',mem[28:30])[0]))
