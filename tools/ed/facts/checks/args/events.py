import re,sys,json,collections
lines=open('build/ed-db/disasm.txt').read().splitlines()
idx={l[:8]:i for i,l in enumerate(lines) if len(l)>8 and l[8]==':'}
sys.path.insert(0,'tools/ed')
import savestate
s=savestate.State('saves/slot1.state')
H={t:s.u32(0x806068A8+t*16) for t in range(0,100) if s.u32(0x806068A8+t*16)}
ev=collections.defaultdict(list)
for t,a in sorted(H.items()):
    i=idx['%08X'%a]
    # function: to next stwu r1 at function start
    j=i+1
    while j<len(lines) and not lines[j][10:].startswith('stwu     r1,'):
        j+=1
    body=lines[i:j]
    reg=None
    for k,l in enumerate(body):
        if 'bl      0x801F5384' in l:
            m=re.search(r'or\s+r(\d+), r3, r3',body[k+1])
            if m: reg=m.group(1)
            else:
                reg='3'
            break
    if reg is None: 
        print('type',t,hex(a),'no 801F5384 in body, len',len(body)); continue
    found=[]
    for k,l in enumerate(body):
        m=re.search(r'cmpwi\s+r%s, (-?\d+)'%reg,l)
        if m:
            n=int(m.group(1)); 
            # first call after compare
            c=None
            for q in range(k+1,min(k+14,len(body))):
                mm=re.search(r'bl\s+0x([0-9A-F]+)',body[q])
                if mm:c=mm.group(1);break
            found.append(n); ev[n].append((t,l[:8],c))
    print('type',t,hex(a),'len',len(body),'events',sorted(set(found)))
print()
for n in sorted(ev):
    print(n,ev[n][:14])
