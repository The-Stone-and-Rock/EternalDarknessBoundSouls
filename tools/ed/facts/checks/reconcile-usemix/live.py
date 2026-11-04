"""python -I tools/ed/facts/checks/reconcile-usemix/live.py : live memory checks behind use-mix.md. Reads states, never writes them.
Walks every state in saves/, build/runs/ and logs/saved-changes/working-01b3d85-before-tome-copy/saves (with reports/)."""
import glob,json,os,struct,sys,collections
sys.path.insert(0,'tools/ed'); sys.path.insert(0,'tools/ed/facts')
import savestate,usemix_inv
R=usemix_inv.load()
T=json.load(open('build/ed-db/text.json',encoding='utf8'))['boot']
def name(k):
    r=R[k]; return T.get('%d,%d'%(r[76],r[79]),['','','?'])[2]
def inmem(a): return 0x80000000<=a<0x81800000
def nodes(s):
    h=s.sda(-21468)
    if not inmem(h): return None
    out={};n=s.u32(h+4);c=0
    while inmem(n) and c<4096:
        out[s.u32(n+16)]=n;n=s.u32(n+4);c+=1
    return out
def carried(s,node):
    l=s.u32(node+48)
    if not inmem(l): return []
    e=s.u32(l+4);ids=[]
    while inmem(e) and len(ids)<64:
        ids.append(s.u32(e+8));e=s.u32(e+4)
    return ids
def flag(s,n):
    t=s.sda(-26528)
    return (s.u32(t+4+(n>>3&~3))>>(n&31))&1 if inmem(t) else None
def states():
    fs=sorted(glob.glob('saves/*.state'))+sorted(glob.glob('build/runs/*.state'))+sorted(glob.glob('logs/saved-changes/working-01b3d85-before-tome-copy/saves/*.state'))+sorted(glob.glob('logs/saved-changes/working-01b3d85-before-tome-copy/saves/reports/*.state'))
    for f in fs:
        try: s=savestate.State(f)
        except Exception as e: print('skip',f,e); continue
        N=nodes(s)
        if N is None: continue
        yield f,s,N
if __name__=='__main__':
    want=sys.argv[1] if len(sys.argv)>1 else 'all'
    tot=0;same=0;diffbytes=collections.Counter();table_ok=collections.Counter();byid=collections.defaultdict(set);tml=0;tbad=0;nodecount=0;dual=0
    roomid=collections.defaultdict(lambda:collections.defaultdict(set))
    for f,s,N in states():
        short=f.replace('logs/saved-changes/working-01b3d85-before-tome-copy/saves','LOGS')
        tab=s.sda(-23412)
        ok=None
        if inmem(tab):
            good=0
            for k in range(244):
                # memory table is the file loaded whole
                pass
        p=s.sda(-21492)
        ids=carried(s,p) if inmem(p) else []
        row=[]
        for i in ids:
            nd=N.get(i)
            if not nd: row.append((i,'nonode'));continue
            o=s.u32(nd+44);k=s.u32(o+12);ins=s.u32(o+4)
            if k>=244: row.append((i,'k?'));continue
            tot+=1
            b=s.bytes(ins,88)
            for x in range(88):
                if b[x]!=R[k][x]: diffbytes[x]+=1
            row.append((i,k,name(k)[:18]))
            roomid[s.sda(-23276)][i].add(k)
        for i,nd in N.items():
            o=s.u32(nd+44)
            if not inmem(o) or not inmem(s.u32(o+4)): continue
            k=s.u32(o+12)
            if k>=244: continue
            nodecount+=1
            if s.u32(o+20) or s.u32(o+24): dual+=1
        print(short,'room',s.sda(-23276),'f601/602',flag(s,601),flag(s,602),'7178',s.u32(0x80307178),'pg',s.u32(0x80625420-24492),'rec',[s.u32(0x80335748+4*i) for i in range(2)],'mix',s.u32(0x80625420-24536),'r13-24244',s.s32(0x80625420-24244))
        if want=='items': print('   ',row)
    print('carried records',tot,'bytes that ever differ from InvU:',sorted(diffbytes.items()))
    print('item nodes',nodecount,'with +20/+24 set',dual)
    print('same room, id with more than one kind:')
    for r,d in sorted(roomid.items()):
        x={i:sorted(v) for i,v in d.items() if len(v)>1}
        if x: print(' room',r,x)
