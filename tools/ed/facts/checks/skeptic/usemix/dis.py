import sys,re
# python -I dis.py START END : plain disasm range with r13 and lis/addi resolved by a simple tracker; independent of usemix_ann
s=int(sys.argv[1],16);e=int(sys.argv[2],16)
hi={}
R13=0x80625420;R2=0x80628460
for line in open('build/ed-db/disasm.txt'):
    a=int(line[:8],16) if re.match(r'[0-9A-F]{8}:',line) else None
    if a is None or a<s: continue
    if a>=e: break
    t=line[10:].strip()
    x=''
    m=re.match(r'lis\s+r(\d+), (-?\d+)',t)
    if m: hi[m.group(1)]=(int(m.group(2))<<16)&0xffffffff
    m=re.match(r'(addi|ori)\s+r(\d+), r(\d+), (-?\d+)',t)
    if m and m.group(3) in hi:
        v=(hi[m.group(3)]+int(m.group(4)))&0xffffffff; hi[m.group(2)]=v; x=' ; =%08X'%v
    m=re.search(r'(-?\d+)\(r13\)',t)
    if m: x=' ; sda %08X (r13%+d)'%(R13+int(m.group(1)),int(m.group(1)))
    m=re.search(r'(-?\d+)\(r2\)',t)
    if m: x=' ; r2 %08X'%(R2+int(m.group(1)))
    m=re.search(r'(-?\d+)\(r(\d+)\)',t)
    if m and m.group(2) in hi and not x and m.group(2) not in('1','13','2'):
        x=' ; mem %08X'%((hi[m.group(2)]+int(m.group(1)))&0xffffffff)
    print(line[:8],t,x)
