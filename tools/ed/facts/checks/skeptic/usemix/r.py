from lib import *
import glob,re,collections
# branch-aware: is there a path where fn93 ran, then ed11(0,0,0) in the same call?
use=set(s16(r,38) for r in U)-{0}
hits=[];n=0
for i in sorted(use):
    ps=glob.glob('build/ed-db/scripts/L*/%d.lua'%i)
    t=open(ps[0],encoding='utf8',errors='replace').read().splitlines()
    lab={l.strip()[2:-2]:k for k,l in enumerate(t) if l.startswith('::')}
    seen=set();st=[(0,False)];found=False
    while st:
        k,f=st.pop()
        if (k,f) in seen or k>=len(t): continue
        seen.add((k,f))
        l=t[k]
        m=re.match(r'if .* then goto (\w+) end',l)
        if m:
            st.append((lab[m.group(1)],f)); st.append((k+1,f)); continue
        m=re.match(r'goto (\w+)',l)
        if m: st.append((lab[m.group(1)],f)); continue
        if 'ed9(' in l and False: continue
        nf=f or 'fn93(' in l
        if 'ed11(0, 0, 0)' in l and f: found=True
        st.append((k+1,nf))
    n+=1
    if found: hits.append(i)
print(n,'use scripts; paths with fn93 before ed11(0,0,0):',hits)
# flags 0x20 kinds and all flag values
print('0x20',[k for k,r in enumerate(U) if struct.unpack('>I',r[4:8])[0]&0x20],[name(k)[1] for k,r in enumerate(U) if struct.unpack('>I',r[4:8])[0]&0x20])
print('slot5 alive',[ (i in nodes(savestate.State('saves/slot5.state'))) for i in (66,105,106)])
