from lib import *
import re
for k,r in enumerate(U):
    t=tx(r[76],r[79])
    lab=t[1] if t else ''
    txt=t[2] if t else ''
    if re.search(r'TULWAR|CHAKRAM|TALISMAN|NECKLACE|BRONZE|BLUE_URN',lab+txt.upper()) or k in (21,31,82,83,84,150):
        print(k,lab,repr(txt[:40]),'use',s16(r,38),'pick',s16(r,36),'chk',s16(r,40),'mix',s16(r,42),'cnt',r[68],r[69],'pickword',tx(r[76],r[77]) and tx(r[76],r[77])[2][:50],'|desc',tx(r[76],r[78]) and tx(r[76],r[78])[2][:60])
# runtime table vs file
st=states()
for n,s in st.items():
    tb=s.sda(-23412)
    if not 0x80000000<=tb<0x81800000: print(n,'no table');continue
    cnt=s.u32(tb); magic=s.u32(tb+4)
    bad=[]
    for k in range(cnt):
        p=s.u32(tb+8+k*8)
        if p==0: bad.append((k,'null'));continue
        # offset fixed up to ptr?
        if s.bytes(p+16,88)!=U[k]: bad.append((k,[i for i in range(88) if s.bytes(p+16,88)[i]!=U[k][i]][:6]))
    print(n,'count',cnt,hex(magic),'mismatches',bad[:10])
