from lib import *
st=states()
import collections
for n,s in st.items():
    N=nodes(s)
    if not N: continue
    tot=m=0; bad=[]; kinds=collections.Counter()
    for iid,nd in N.items():
        obj=s.u32(nd+44)
        if not 0x80000000<=obj<0x81800000: continue
        rec=s.u32(obj+4); kind=s.u32(obj+12); tmpl=s.u32(obj+8); t0=s.u32(obj)
        if not 0x80000000<=rec<0x81800000 or not 0<kind<244 or not 0<=tmpl<55 or not 0x80000000<=t0<0x81800000: continue
        mem=s.bytes(rec,88); f=U[kind]
        d=[i for i in range(88) if mem[i]!=f[i]]
        tot+=1
        g=not [i for i in d if 36<=i<44 or 76<=i<80]
        m+=g
        if not g: bad.append((iid,kind,name(kind)[1],d[:8]))
        kinds[kind]+=1
        if iid in (65,66,67,105): print('   ',n,'node',iid,'kind',kind,name(kind)[1],'tmpl',tmpl)
    print(n,'item-shaped nodes',tot,'match',m,'bad',bad[:8])
