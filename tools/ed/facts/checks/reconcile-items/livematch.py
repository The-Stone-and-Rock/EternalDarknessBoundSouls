"""How many live things match a table 3 or 5 (type, tag), and the Scramasax's object room by slot. Read only."""
import sys,glob,os,json
HERE=os.path.dirname(os.path.abspath(__file__));ROOT=os.path.abspath(os.path.join(HERE,'..','..','..'))
sys.path.insert(0,os.path.join(ROOT,'tools','ed'));os.chdir(ROOT)
import savestate
rows=json.load(open('build/ed-db/facts/item_table.json',encoding='utf8'))['rows']
pairs={(r['id'],65535 if w['tag'] is None else w['tag']) for r in rows for w in r['placed']}
hit=tot=0
for p in sorted(glob.glob('**/*.state',recursive=True)):
    s=savestate.State(p);head=s.sda(-21468)
    if not (0x80000000<=head<0x81800000) or not s.u32(head+4): continue
    n=s.u32(head+4);c=0
    while n and c<3000:
        c+=1;info=s.u32(n+44);st=s.u32(n+36)
        if info:
            t=s.s32(info+12)
            if 0<=t<244:
                tot+=1;tag=s.s32(st+24) if st else None
                if (t,tag) in pairs: hit+=1
        n=s.u32(n+4)
print('live matched',hit,'of',tot,'unmatched',tot-hit)
for slot in (1,2,3,5,7):
    s=savestate.State('saves/slot%d.state'%slot);head=s.sda(-21468);n=s.u32(head+4)
    while n:
        info=s.u32(n+44);st=s.u32(n+36)
        if info and st and s.s32(st+24)==481:
            o=s.u32(n+52);print('slot',slot,'room',s.s32(s.r13-23276),'handle',s.u32(n+16),'type',s.s32(info+12),'obj',hex(o),'objroom',s.s32(o+592) if o else None)
        n=s.u32(n+4)
for p,w in (('saves/slot2.state',62),('saves/slot5.state',105),('saves/slot5.state',66)):
    s=savestate.State(p);print(p,'word0',s.u32(0x80335748))
