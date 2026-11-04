import sys,struct,json,glob,os
sys.path.insert(0,'tools/ed')
import disc,savestate
D=disc.Disc()
Dol=disc.Dol()
def tbl(name):
    b=D.read(name); n=struct.unpack('>I',b[:4])[0]
    return [b[struct.unpack('>I',b[8+i*8:12+i*8])[0]:][:struct.unpack('>I',b[12+i*8:16+i*8])[0]] for i in range(n)]
U=[r[16:] for r in tbl('InvU.bin')]
S=tbl('InvS.bin')
E=tbl('InvE.bin')
T=json.load(open('build/ed-db/text.json',encoding='utf8'))['boot']
def tx(g,e):
    v=T.get('%d,%d'%(g,e)); return v
def h(r,o): return struct.unpack('>H',r[o:o+2])[0]
def s16(r,o): return struct.unpack('>h',r[o:o+2])[0]
def name(k):
    r=U[k]; return tx(r[76],r[79])
def states():
    out={}
    for p in sorted(glob.glob('saves/*.state')):
        n=os.path.basename(p)[:-6]
        try: out[n]=savestate.State(p)
        except Exception as e: print('bad',n,e)
    return out
def carried(s,node=None):
    node=node or s.sda(-21492)
    o=[]
    it=s.u32(node+48) if node else 0
    c=0
    while it and c<200:
        o.append(it); c+=1
        it=s.u32(it+4)
    return o
def nodes(s):
    head=s.sda(-21468); out={}
    if not 0x80000000<=head<0x81800000: return out
    n=s.u32(head+4); c=0
    while 0x80000000<=n<0x81800000 and c<8192:
        out[s.u32(n+16)]=n; n=s.u32(n+4); c+=1
    return out
def carried_ids(s,node):
    l=s.u32(node+48)
    ids=[]
    c=0
    while l and c<200:
        ids.append((s.u32(l),s.u32(l+8)))
        l=s.u32(l+4); c+=1
    return ids
