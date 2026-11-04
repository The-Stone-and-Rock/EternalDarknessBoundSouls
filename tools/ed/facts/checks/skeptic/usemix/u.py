from lib import *
import re
for k in (160,161,154,60,67,165,167,176): 
    r=U[k];print(k,name(k),'grp',r[76],'desc',r[78],'chk',s16(r,40),'use',s16(r,38))
print(T.get('13,33')[1],T.get('13,36')[1])
hi={}
R=[]
for line in open('build/ed-db/disasm.txt'):
    if not re.match(r'[0-9A-F]{8}:',line): continue
    a=line[:8];t=line[10:].strip()
    m=re.match(r'lis\s+r(\d+), (-?\d+)',t)
    if m: hi[m.group(1)]=(int(m.group(2))<<16)&0xffffffff; continue
    m=re.match(r'addi\s+r(\d+), r(\d+), (-?\d+)',t)
    if m and m.group(2) in hi:
        hi[m.group(1)]=(hi[m.group(2)]+int(m.group(3)))&0xffffffff
        continue
    m=re.search(r'(-?\d+)\(r(\d+)\)',t)
    if m and m.group(2) in hi:
        v=(hi[m.group(2)]+int(m.group(1)))&0xffffffff
        if v in (0x80316490,0x8031649A): R.append((a,t,hex(v)))
print(R[:40],len(R))
