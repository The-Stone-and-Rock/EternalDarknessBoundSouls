"""Reconcile scan for item-ids.md: walks every readable .state under the repo, read only. Writes scan.json beside itself.
python -I tools/ed/facts/checks/reconcile-items/scan.py"""
import sys,struct,json,os,glob,collections,hashlib
HERE=os.path.dirname(os.path.abspath(__file__))
ROOT=os.path.abspath(os.path.join(HERE,'..','..','..'))
sys.path.insert(0,os.path.join(ROOT,'tools','ed'))
import savestate,disc
os.chdir(ROOT)
u=disc.Disc().read('InvU.bin')
def rec(i):
    off=struct.unpack('>I',u[8+i*8:12+i*8])[0];return u[off+16:off+104]
RECS=[rec(i) for i in range(244)]
paths=sorted(glob.glob('**/*.state',recursive=True))
R=dict(files=len(paths),unread=[],nolist=[],withlist=0,unique=set())
things=typed=neg=0
textdiff=collections.Counter();bytediff=collections.Counter();handles=collections.defaultdict(set);tagtypes=collections.defaultdict(set)
handletypes=collections.defaultdict(set);setword=collections.Counter();negs=[];tablebad=0;tablechecked=0
lists=listbad=listent=0;nonplayerlists=0;order_bad=0;room_mismatch=0;room_checked=0
slots=slotok=0;slotbad=[];samples={}
modes=collections.Counter();maskdiff=collections.Counter()
for p in paths:
    try: s=savestate.State(p)
    except Exception as e:
        R['unread'].append(p);continue
    head=s.sda(-21468)
    if not (0x80000000<=head<0x81800000) or not s.u32(head+4):
        R['nolist'].append(p);continue
    R['withlist']+=1
    R['unique'].add(hashlib.md5(bytes(s.mem)).hexdigest())
    pw=s.sda(-26320)
    try: setword[s.u32(pw)]+=1
    except Exception: setword['bad']+=1
    base=s.sda(-23412);tablechecked+=1
    for i in range(244):
        q=s.u32(base+8+i*8)
        if s.bytes(q+16,88)!=RECS[i]: tablebad+=1;break
    n=s.u32(head+4);nodes={};order=[]
    while n and len(nodes)<3000:
        h=s.u32(n+16);nodes[h]=n;order.append(h);n=s.u32(n+4)
    if any(order[i]<=order[i+1] for i in range(len(order)-1)): order_bad+=1
    room=s.sda(-23276)
    if 1 in nodes:
        o=s.u32(nodes[1]+52)
        if o:
            room_checked+=1
            if s.s32(o+592)!=s.s32(s.r13-23276): room_mismatch+=1
    for h,n in nodes.items():
        info=s.u32(n+44)
        if info:
            things+=1
            ty=s.s32(info+12);kind=s.s32(info+8)
            st=s.u32(n+36);tag=s.s32(st+24) if st else None
            if ty<0 or ty>=244:
                neg+=1;negs.append((p,h,ty,kind,hex(s.u32(info+4))));continue
            typed+=1
            r=s.u32(info+4);live=s.bytes(r,88);disk=RECS[ty]
            for i in range(88):
                if live[i]!=disk[i]:
                    bytediff[(i)]+=1
                    if 76<=i<=79: textdiff[(ty,i,live[i],disk[i])]+=1
            handles[ty].add(h);handletypes[h].add(ty);tagtypes[tag].add(ty)
            m=struct.unpack('>I',live[:4])[0]
            if m: modes[(ty,m)]+=1
            invp=s.u32(info);mask=s.u16(invp+28)
            maskdiff[(kind,mask)]+=1
        lp=s.u32(n+48)
        if lp:
            lists+=1
            if h!=1: nonplayerlists+=1
            cnt=s.u32(lp+8);e=s.u32(lp+4);w=0;prev=lp
            while e and w<5000:
                if s.u32(e)!=prev or s.u32(e+8) not in nodes: listbad+=1
                prev=e;e=s.u32(e+4);w+=1;listent+=1
            if w!=cnt or s.u32(lp)!=0: listbad+=1
    for lb in (264,792):
        for i in range(12):
            a=0x80334388+lb+i*44
            info=s.u32(a+36);h=s.u32(a+40)
            if info and 0x80000000<=info<0x81800000:
                slots+=1
                if h in nodes and s.u32(nodes[h]+44)==info: slotok+=1
                else: slotbad.append((p,lb,i,h,hex(info)))
out=dict(files=R['files'],unread=R['unread'],nolist=len(R['nolist']),withlist=R['withlist'],unique_with_list=len(R['unique']),
 things=things,typed=typed,neg=neg,negs=negs,textdiff=sorted([list(k)+[v] for k,v in textdiff.items()]),bytediff=sorted(bytediff.items()),
 handles={k:sorted(v) for k,v in handles.items()},handletypes_multi=sum(1 for v in handletypes.values() if len(v)>1),handles_total=len(handletypes),
 tagtypes={str(k):sorted(v) for k,v in tagtypes.items()},setword={str(k):v for k,v in setword.items()},tablebad=tablebad,tablechecked=tablechecked,
 lists=lists,nonplayerlists=nonplayerlists,listent=listent,listbad=listbad,order_bad=order_bad,room_checked=room_checked,room_mismatch=room_mismatch,
 slots=slots,slotok=slotok,slotbad=slotbad,modes=sorted([list(k)+[v] for k,v in modes.items()]),kindmask=sorted([list(k)+[v] for k,v in maskdiff.items()]))
json.dump(out,open(os.path.join(HERE,'scan.json'),'w'))
for k in ('files','unread','nolist','withlist','unique_with_list','things','typed','neg','textdiff','tablebad','tablechecked','lists','nonplayerlists','listent','listbad','order_bad','room_checked','room_mismatch','slots','slotok','setword','handletypes_multi','handles_total','modes'):
    print(k,out[k])
print('negs',negs);print('slotbad',len(slotbad),collections.Counter(x[0] for x in slotbad))
print('kindmask',out['kindmask'])
