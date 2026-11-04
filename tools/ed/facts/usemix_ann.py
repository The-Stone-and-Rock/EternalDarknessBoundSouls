"""Annotated disassembly: python -I tools/ed/facts/usemix_ann.py START END  (branch targets marked, lis+addi resolved, natives named)"""
import re,sys,csv
L=open('build/ed-db/disasm.txt').read().splitlines()
D={}
for l in L:
    try: D[int(l[:8],16)]=l[10:].strip()
    except: pass
names={}
for row in open('build/ed-db/natives.tsv'):
    p=row.rstrip('\n').split('\t')
    if len(p)>1 and p[1]: names[int(p[1],16)]=p[0]
EX={0x801642D4:'RUN_SCRIPT',0x80110478:'spellrec_get',0x801F5F88:'node_by_id',0x801F6398:'node_obj',0x8013DB3C:'alloc?',0x80003100:'memset',
}
names.update(EX)
s,e=int(sys.argv[1],16),int(sys.argv[2],16)
hi={}
tg=set()
for a in range(s,e,4):
    m=re.search(r'0x(80[0-9A-Fa-f]{6})$',D.get(a,''))
    if m and D[a].startswith('bc') : tg.add(int(m.group(1),16))
    if m and D[a].startswith('b '): tg.add(int(m.group(1),16))
for a in range(s,e,4):
    t=D.get(a,'')
    if not t: continue
    note=''
    m=re.match(r'lis\s+(r\d+), (-?\d+)',t)
    if m: hi[m.group(1)]=(int(m.group(2))&0xffff)<<16
    m=re.match(r'addi\s+(r\d+), (r\d+), (-?\d+)',t)
    if m and m.group(2) in hi:
        v=(hi[m.group(2)]+int(m.group(3)))&0xffffffff; note='=%08X'%v
        hi[m.group(1)]=v
    m=re.search(r'(l|st)\w+\s+\S+, (-?\d+)\((r\d+)\)',t)
    if m and m.group(3)=='r13':
        note='sda %08X'%((0x80625420+int(m.group(2)))&0xffffffff)
    elif m and m.group(3)=='r2':
        pass
    elif m and m.group(3) in hi:
        note='@%08X'%((hi[m.group(3)]+int(m.group(2)))&0xffffffff)
    m=re.match(r'bl\s+0x(\w+)',t)
    if m:
        n=names.get(int(m.group(1),16))
        if n: note=n
        for r in ('r3','r4','r5','r6','r7','r8','r9','r10','r11','r12','r0'): hi.pop(r,None)
    t=re.sub(r'\s+',' ',t)
    print(('>' if a in tg else ' ')+'%08X %-34s %s'%(a,t,note))
