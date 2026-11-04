"""python -I tools/ed/facts/usemix_saves.py [STATE...] : every save's carried items and equipment, named through InvU.bin (default saves/*.state and build/runs/*.state).
Checks the id -> kind -> name chain. Only reads the states. A state with no player (the title screen) is listed and skipped."""
import sys,json,struct,glob
sys.path.insert(0,'tools/ed/facts'); sys.path.insert(0,'tools/ed')
import savestate,usemix_inv
R=usemix_inv.load()
T=json.load(open('build/ed-db/text.json',encoding='utf8'))['boot']
def name(k):
    r=R[k]; return T.get('%d,%d'%(r[76],r[79]),['','','?'])[2]
def inmem(a): return 0x80000000<=a<0x81800000
for f in sys.argv[1:] or sorted(glob.glob('saves/*.state'))+sorted(glob.glob('build/runs/*.state')):
    s=savestate.State(f)
    head=s.sda(-21468)
    if not inmem(head) or not inmem(s.sda(-21492)):
        print('==',f,'no player, skipped'); continue
    nodes={};n=s.u32(head+4);c=0
    while 0x80000000<=n<0x81800000 and c<4096:
        nodes[s.u32(n+16)]=n; n=s.u32(n+4); c+=1
    p=s.sda(-21492)
    print('==',f,'room',s.sda(-23276),'player id',s.sda(-21496),'record',[s.u32(0x80335748+i*4) for i in range(6)],'page state',s.u32(0x80625420-24492),'tome-mode',s.s32(0x80625420-24244),'left script',s.s32(0x80625420-26324),'mix script',s.u32(0x80625420-24536))
    l=s.u32(p+48); e=s.u32(l+4) if inmem(l) else 0; ids=[]
    while inmem(e) and len(ids)<64:
        ids.append(s.u32(e+8)); e=s.u32(e+4)
    for i in ids:
        nd=nodes.get(i)
        if not nd: print('  ',i,'no node'); continue
        o=s.u32(nd+44)
        k=s.u32(o+12); ins=s.u32(o+4)
        use,mix=struct.unpack('>h',s.bytes(ins+38,2))[0],struct.unpack('>h',s.bytes(ins+42,2))[0]
        same=s.bytes(ins+36,8)==R[k][36:44] and s.bytes(ins+76,4)==R[k][76:80]
        print('   id',i,'kind',k,name(k),'tmpl',s.u32(o+8),'use',use,'mix',mix,'count',struct.unpack('>H',s.bytes(ins+68,2))[0],'matchesInvU',same)
    # equipment
    a=0x80588DF0
    for j in range(20):
        hid=s.s32(a)
        if hid>0:
            cnt=s.u32(a+388)
            ents=[tuple(s.u32(a+4+12*x+y) for y in (0,4,8)) for x in range(cnt)]
            print('   equip of id',hid,'count',cnt,'entries(id,hands,word)',ents,'+392',hex(s.u32(a+392)),'+396',hex(s.u32(a+396)))
        a+=408
