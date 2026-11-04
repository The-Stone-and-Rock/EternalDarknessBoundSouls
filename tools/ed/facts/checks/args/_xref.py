import sys,re
# usage: _xref.py ADDR [ADDR...]: every lis/addi or lis/lwz pair in disasm.txt that forms an address within [ADDR, ADDR+span)
span=0x60
targets=[int(a,16) for a in sys.argv[1:]]
lines=open('build/ed-db/disasm.txt').read().splitlines()
lis={}
res={}
for i,l in enumerate(lines):
    m=re.match(r'([0-9A-F]{8}): lis\s+r(\d+), (-?\d+)',l)
    if m:
        lis[int(m.group(2))]=(i,int(m.group(3))&0xffff); continue
    m=re.match(r'([0-9A-F]{8}): (\w+\.?)\s+r(\d+), r(\d+), (-?\d+)',l)
    if m and m.group(2) in('addi',):
        rd,ra,imm=int(m.group(3)),int(m.group(4)),int(m.group(5))
        if ra in lis and i-lis[ra][0]<40:
            a=((lis[ra][1]<<16)+imm)&0xffffffff
            for t in targets:
                if t<=a<t+span: res.setdefault(t,[]).append((m.group(1),hex(a)))
        continue
    m=re.match(r'([0-9A-F]{8}): (\w+)\s+[rf](\d+), (-?\d+)\(r(\d+)\)',l)
    if m:
        ra,imm=int(m.group(5)),int(m.group(4))
        if ra in lis and i-lis[ra][0]<40:
            a=((lis[ra][1]<<16)+imm)&0xffffffff
            for t in targets:
                if t<=a<t+span: res.setdefault(t,[]).append((m.group(1),m.group(2),hex(a)))
for t,v in res.items():
    print(hex(t),len(v))
    for x in v: print(' ',x)
