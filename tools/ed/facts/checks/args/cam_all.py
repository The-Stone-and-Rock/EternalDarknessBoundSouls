# Every room's whole camera array, read by walking the arrays the way the game lays them out.
#   Eye array: n entries of 120 bytes, n is the u16 at file +162 (the game reads it at 0x801EE818).
#   Right after it, each eye's keys in order. Entry +100, +108, +116 are the byte sizes of its
#   +96, +104, +112 arrays in the file, and pointers once loaded. Then the target array, then its keys.
# Writes args/cameras_all.json and prints every check. Run: python -I tools/ed/facts/checks/args/cam_all.py
import sys,struct,re,json,multiprocessing,collections
sys.path.insert(0,'tools/ed')
import disc,gametext
U16=lambda x,o:struct.unpack('>H',x[o:o+2])[0]
U32=lambda x,o:struct.unpack('>I',x[o:o+4])[0]
F32=lambda x,o:struct.unpack('>f',x[o:o+4])[0]
LE=lambda x,o:struct.unpack('<I',x[o:o+4])[0]
NAME=re.compile(rb'[\x20-\x7e]{2,31}\0')
def chain(x,s,n):
    """Bytes from s to the end of these n entries' keys, using the sizes in the entries."""
    t=n*120
    for k in range(n):
        e=s+k*120
        if e+120>len(x): return -1
        if U16(x,e+96): t+=U32(x,e+100)
        if U16(x,e+104): t+=U32(x,e+108)
        if U16(x,e+112): t+=U32(x,e+116)
    return t
def find_array(x,count,lim):
    """The eye array. The file's +164 and +172 still hold the array's and the target array's build-time
    addresses (little endian), so their difference is a size to test against: count entries plus their keys.
    Candidates are named entries; each is tried at each position in the array, on all four byte offsets
    because a hash ending in a printable byte runs into the name."""
    if not count: return []
    want=LE(x,172)-LE(x,164)
    hits=set()
    for m in NAME.finditer(x,0,lim):
        e0=m.start()-56
        if e0<0 or e0+120>len(x): continue
        for k in [i*120+d for i in range(count) for d in range(0,4)]:
            s=e0-k
            if s<0 or s%4 or s in hits: continue
            if chain(x,s,count)==want and all(U32(x,s+j*120+52) and NAME.match(x,s+j*120+56) and 0.5<F32(x,s+j*120+36)<180 for j in range(count)):
                hits.add(s)
    return sorted(hits)
def keys(x,at,n,stride,fmt):
    out=[]
    for j in range(n):
        out.append(struct.unpack(fmt,x[at+j*stride:at+j*stride+struct.calcsize(fmt)]))
    return out
def walk(x,start,n,cur,named):
    rows=[]
    for k in range(n):
        e=start+k*120
        n96,n104,n112=U16(x,e+96),U16(x,e+104),U16(x,e+112)
        o96,o104,o112=U32(x,e+100),U32(x,e+108),U32(x,e+116)
        r=dict(pos=struct.unpack('>3h',x[e:e+6]),fov=round(F32(x,e+36),2),far=round(F32(x,e+32),1),n96=n96,n104=n104,n112=n112,size96=o96,size104=o104,size112=o112)
        if named:
            r['name']=x[e+56:e+88].split(b'\0')[0].decode('latin1'); r['hash']=U32(x,e+52)
        if n96: cur+=o96
        if n104:
            ks=keys(x,cur,n104,12,'>I3h')
            r['first_key']=ks[0]; r['last_key']=ks[-1]; r['frames']=ks[-1][0]
            cur+=o104
        if n112:
            ks=keys(x,cur,n112,20,'>I4f')
            r['rot_frames']=(ks[0][0],ks[1][0] if n112>1 else None,ks[-1][0])
            cur+=o112
        rows.append(r)
    return rows,cur
def one(a):
    path,off,size=a
    with open(disc.ISO,'rb') as fh:
        fh.seek(off); b=fh.read(size)
    first,csize,sec1=U32(b,4),U32(b,12),U32(b,16)
    x=gametext.bpe_expand(b,first,min(first+csize,len(b)))
    n=int(re.search(r'rmed(\d+)',path).group(1))
    count=U16(x,162)
    lim=sec1 or len(x)
    hits=find_array(x,count,lim)
    res=dict(room=n,count=count,hits=len(hits))
    if count==0 or len(hits)!=1:
        res['eyes']=[];res['targets']=[]
        return res
    e0=hits[0]
    eyes,cur=walk(x,e0,count,e0+count*120,True)
    res['eye_at']=e0;res['target_at']=cur
    targets,end=walk(x,cur,count,cur+count*120,False)
    res['end']=end
    # +180 is the next array's build-time address when it is not 0
    nxt=LE(x,180)
    res['end_agrees']=(end-cur==nxt-LE(x,172)) if nxt else None
    res['eyes']=eyes;res['targets']=targets
    return res
if __name__=='__main__':
    d=disc.Disc()
    jobs=[(p,d.files[p][0],d.files[p][1]) for p in d.glob(r'^Rooms/rmed\d+\.bpe$')]
    res={}
    with multiprocessing.Pool(8) as pool:
        for r in pool.imap_unordered(one,jobs): res[r['room']]=r
    kit=json.load(open('build/ed-db/rooms.json'))
    print('room files',len(res),'count in header total',sum(r['count'] for r in res.values()))
    print('rooms where the array was not found once',[(k,r['count'],r['hits']) for k,r in sorted(res.items()) if r['count'] and r['hits']!=1])
    print('target keys end at the next array (+180): yes',sum(1 for r in res.values() if r.get('end_agrees') is True),'no',[k for k,r in sorted(res.items()) if r.get('end_agrees') is False],'+180 empty',sum(1 for r in res.values() if r['eyes'] and r.get('end_agrees') is None))
    EY=[c for r in res.values() for c in r['eyes']]; TG=[c for r in res.values() for c in r['targets']]
    print('eyes',len(EY),'eyes with +104 keys',sum(1 for c in EY if c['n104']),'with +96',sum(1 for c in EY if c['n96']),'with +112',sum(1 for c in EY if c['n112']))
    print('targets',len(TG),'targets with +104 keys',sum(1 for c in TG if c['n104']),'with +112',sum(1 for c in TG if c['n112']))
    M=[c for c in EY+TG if c['n104']]
    print('first key is frame 0 at pos',sum(1 for c in M if c['first_key'][0]==0 and tuple(c['first_key'][1:])==tuple(c['pos'])),'of',len(M),[(c.get('name'),c['pos'],c['first_key']) for c in M if not(c['first_key'][0]==0 and tuple(c['first_key'][1:])==tuple(c['pos']))][:8])
    print('size104 minus n104*12',collections.Counter(c['size104']-c['n104']*12 for c in M).most_common(5))
    R=[c for c in EY+TG if c['n112']]
    print('size112 minus n112*20',collections.Counter(c['size112']-c['n112']*20 for c in R).most_common(5))
    print('n104 equals n112',sum(1 for c in EY+TG if c['n104']==c['n112']),'of',len(EY+TG),'n112 only',sum(1 for c in EY+TG if c['n112'] and not c['n104']),'n104 only',sum(1 for c in EY+TG if c['n104'] and not c['n112']))
    print('rot frame steps',collections.Counter(c['rot_frames'][1]-c['rot_frames'][0] for c in R if c['rot_frames'][1] is not None).most_common(5))
    # against the kit
    in_kit=not_in=0; flagged=flagged_still=0; missing=collections.Counter(); kit_unmatched=[]
    for k,r in res.items():
        km={c['hash']:c for c in kit.get(str(k),{'cameras':[]})['cameras']}
        for c in r['eyes']:
            q=km.get(c['hash'])
            c['in_kit']=bool(q); c['kit_path']=q['path'] if q else None
            if q:
                in_kit+=1
                if q['path']:
                    flagged+=1; flagged_still+=not(c['n96'] or c['n104'])
            else:
                not_in+=1; missing[re.sub(r'\d+','',c['name'])]+=1
        hs={c['hash'] for c in r['eyes']}
        for q in kit.get(str(k),{'cameras':[]})['cameras']:
            if q['hash'] not in hs: kit_unmatched.append((k,q['name'],q['hash']))
    print('kit lists',sum(len(v['cameras']) for v in kit.values()),'of these on the walk',in_kit,'eyes the kit lacks',not_in,missing.most_common(12))
    print('kit entries not on the walk',kit_unmatched[:10],len(kit_unmatched))
    print('kit path flag',flagged,'of which still',flagged_still)
    print('moving among eyes the kit lists',sum(1 for r in res.values() for c in r['eyes'] if c.get('in_kit') and (c['n96'] or c['n104'])))
    json.dump({str(k):dict(count=r['count'],eyes=r['eyes'],targets=r['targets']) for k,r in sorted(res.items())},open('build/ed-db/facts/cameras_all.json','w'))
