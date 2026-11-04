from lib import *
st=states()
tot=0;match=0
mism=[]
ALL={}
for n,s in st.items():
    pn=s.sda(-21492)
    if not 0x80000000<=pn<0x81800000: print('==',n,'no player');continue
    print('==',n,'room',s.sda(-23276),'pid',s.sda(-21496),'pnode',hex(pn),'pagestate',s.sda(-24492),'mix',s.sda(-24536),'rec',[s.u32(0x80335748+4*i) for i in range(6)], 'mode', s.s32(s.r13-24244))
    N=nodes(s)
    ids=carried_ids(s,pn)
    print(' raw',ids[:4])
    for tag,iid in ids[1:]:
        nd=N.get(iid)
        if not nd: print('  no node',iid,hex(tag));continue
        obj=s.u32(nd+44)
        if not obj: print('  noobj',iid);continue
        kind=s.u32(obj+12); tmpl=s.u32(obj+8); rec=s.u32(obj+4)
        if not 0<=kind<len(U): print('  ?',iid,hex(obj),kind);continue
        mem=s.bytes(rec,88)
        f=U[kind]
        same_sc=mem[36:44]==f[36:44]
        same_tx=mem[76:80]==f[76:80]
        tot+=1
        good=same_sc and same_tx
        match+=good
        diffs=[i for i in range(88) if mem[i]!=f[i]]
        print('  id',iid,'kind',kind,'tmpl',tmpl,name(kind)[1] if name(kind) else None,'+36..',struct.unpack('>4H',mem[36:44]),'file',struct.unpack('>4H',f[36:44]),'match',good,'diffs',diffs,'hdlr',hex(tag))
        ALL.setdefault(kind,set()).add((n,iid,tmpl))
        if not good: mism.append((n,iid,kind,struct.unpack('>4H',mem[36:44]),struct.unpack('>4H',f[36:44]),mem[76:80].hex(),f[76:80].hex()))
print(tot,match,mism)
