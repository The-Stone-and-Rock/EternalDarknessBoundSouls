"""python -I tools/ed/facts/checks/reconcile-usemix/refs.py HEX... : lis+addi/ori references to an address, from the whole disassembly."""
import re,sys
want={int(x,16) for x in sys.argv[1:]}
hi={}
for l in open('build/ed-db/disasm.txt'):
    try: a=int(l[:8],16)
    except: continue
    t=' '.join(l[10:].split())
    m=re.match(r'lis (r\d+), (-?\d+)',t)
    if m: hi[m.group(1)]=(a,(int(m.group(2))&0xffff)<<16); continue
    m=re.match(r'(addi|ori) (r\d+), (r\d+), (-?\w+)',t)
    if m and m.group(3) in hi and a-hi[m.group(3)][0]<64:
        v=int(m.group(4),0)
        v=(hi[m.group(3)][1]+v)&0xffffffff
        if v in want: print('%08X %s  -> %08X'%(a,t,v))
