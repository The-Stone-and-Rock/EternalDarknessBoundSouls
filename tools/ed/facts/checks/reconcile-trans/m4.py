import sys,struct,glob,os
sys.path.insert(0,'tools/ed')
import savestate,gametext
for path in sorted(glob.glob('saves/*.state')):
    b=os.path.basename(path)
    if not b.startswith(('slot','coop')): continue
    s=savestate.State(path)
    node=s.u32(s.r13-21492);obj=s.u32(node+52);t=s.u32(obj+648);n=s.u16(obj+726)
    nb=s.u16(s.u32(obj+60)+8)
    row=[]
    for mid in (2,25,103,104):
        if mid>=n or not s.u32(t+4*mid): row.append((mid,None));continue
        d=s.u32(s.u32(t+4*mid)+4)
        fr=s.u32(d+8); sz=s.u32(d+288)
        x=bytes(gametext.bpe_expand(s.bytes(s.u32(d+280),s.u32(d+284)),0,s.u32(d+284)))
        head=struct.unpack('>4I',x[:16])
        lasts=[];spans=[]
        for i in range(nb):
            tr=head[2]+16*i
            c1,_,p1,c2,_,p2=struct.unpack('>HHIHHI',x[tr:tr+16])
            for c,p in ((c1,p1),(c2,p2)):
                if c:
                    k0=struct.unpack('>I',x[p:p+4])[0]; kl=struct.unpack('>I',x[p+12*(c-1):p+12*(c-1)+4])[0]
                    lasts.append(kl>>16); spans.append(((kl>>2)-(k0>>2))/65536)
        c,_,p,_,_,_=struct.unpack('>HHIHHI',x[head[2]:head[2]+16])
        keys=[struct.unpack('>Hxxhhh',x[p+12*i:p+12*i+10]) for i in range(c)]
        row.append((mid,'fr',fr,'sz',sz==len(x),'lastkey',min(lasts),max(lasts),'upd',min(spans),max(spans),'k0',keys[0],'kL',keys[-1],'zr',min(k[3] for k in keys),max(k[3] for k in keys)))
    has99=99<n and bool(s.u32(t+4*99))
    ch=s.u32(obj+64)
    print(b,'nb',nb,'model',s.u32(obj+0) if 0 else '','99',has99,'state',s.u32(node+20),'mot',s.u32(s.u32(ch+184)),'fl',hex(s.u32(ch+244)),'+124',hex(s.u32(s.u32(node+40)+124)))
    for r in row: print('  ',r)
