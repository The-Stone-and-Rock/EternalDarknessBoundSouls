import sys,re,json,os,struct,collections
sys.path.insert(0,'tools/ed'); sys.path.insert(0,'tools/ed/facts/checks/args')
import disc
from _npc2 import parse,D as DISC
dol=disc.Dol()
rooms=json.load(open('build/ed-db/rooms.json'))
text=json.load(open('build/ed-db/text.json'))
# chapter -> model: jump table 0x8023BC2C of 0x80007C8C
lines=open('build/ed-db/disasm.txt').read().splitlines()
idx={l[:8]:i for i,l in enumerate(lines) if len(l)>8 and l[8]==':'}
model={}
for c in range(17):
    t=dol.word(0x8023BC2C+4*c)
    m=re.search(r'li\s+r3, (\d+)',lines[idx['%08X'%t]])
    model[c]=int(m.group(1))
start={c:dol.word(0x8023D738+4*c) for c in range(18)}
who={0:'Alex in the mansion, while flag 787 is off',1:'Pious',2:'Ellia',3:'Anthony',4:'Karim',5:'Maximillian',6:'Lindsey',7:'Paul Luther',8:'Roberto',9:'Edward',10:'Peter',11:'not named: the disc gives only the page label MAJOR',12:'Alex in the mansion, once flag 787 is on',13:'Alex by the model table. Reached only by ed87(12,13,1) in script 1010',16:'chapter 16 starts in room 96: scripts, words and U_Level are from level 00, see note'}
notes={-1:'Boot pack: 0x800182C4 loads /levels/level-1/ScrLvl-1.gpk through 0x80164278. Scripts 1500 and 1600 only. No rooms.',
0:'0x8003F4F4 (scripts) and 0x80040598 (room words) swap 0 for 12 when flag 787 is on. 0x80040598 sets flag 787 when chapter is 0 and room is 100.',
12:'Never a chapter number: ed87 maps level 12 to 0. Reached through flag 787, or directly at 0x800203F8 for any chapter except 13 and 15 while flag 787 is on.',
14:'U_Level14.bin only. ed87 maps level 14 to 0. No caller of it found.',
15:'U_Level15.bin only. ed87 can store 15, but no script calls ed87 with 15. Not seen in play.',
16:'Chapter 16: 0x8003F4F4, 0x80040598 and 0x800407FC turn 16 into 0 (then 12 if flag 787 is on), and 0x80042268 does for the main Npcs load, but 0x800ADF44 and 0x800B8098 pass the chapter word raw, so those two read Npcs16.npc. Files in Level16/: ScrLvl16 and the words load only through argument 19 (digits 1 and 6), the chapter 0 entry of table 0x802441F4 in 0x8006C708, as far as read. prologue.aud and cin0069 loaders not traced. All 299 of its scripts are byte identical to Level09 scripts of the same id. Its words are a subset of room09 (70 of 70 entries equal).',
17:'Cinematics and insanity audio only: cin0074 to cin0077, ins1 to ins4. No scripts.'}
# 0x8006C708's table at 0x802441F4: which level's scripts and words it loads for a chapter
remap={}
for c in range(11):
    t=dol.word(0x802441F4+4*c)
    m=re.search(r'li\s+r30, (\d+)',lines[idx['%08X'%t]])
    remap[c]=int(m.group(1)) if m else c
# chapter page labels: an item whose use script calls ed87(n,n,0)
items=json.load(open('build/ed-db/facts/item_table.json'))['rows']
page={}
for it in items:
    u=it.get('use_script')
    if not u: continue
    for lv in ('00','01'):
        p=os.path.join('build/ed-db/scripts','L'+lv,'%d.lua'%u)
        if os.path.exists(p):
            m=re.search(r'ed87\((\d+), (\d+), 0\)',open(p,encoding='utf8',errors='replace').read())
            if m and m.group(1)==m.group(2): page[int(m.group(2))]=it.get('label')
            break
print('chapter page labels',page)
print('0x8006C708 remap',remap)
root='build/ed-db/scripts'
def body(lv,s):
    p=os.path.join(root,'L'+lv,'%d.lua'%s)
    return open(p,encoding='utf8',errors='replace').read() if os.path.exists(p) else None
def dests(lv,s,seen):
    out=set()
    if s in seen: return out
    seen.add(s)
    b=body(lv,s)
    if b is None: return out
    for m in re.finditer(r'(?<![A-Za-z0-9_])fn66\((\d+),',b): out.add(int(m.group(1)))
    for m in re.finditer(r'(?<![A-Za-z0-9_])fn73\((\d+)\)',b): out|=dests(lv,int(m.group(1)),seen)
    return out
levels={}
placed={}
for p in DISC.glob(r'^Npcs\d+\.npc$'):
    n=int(re.search(r'Npcs(\d+)',p).group(1))
    b,secs=parse(n)
    rs=collections.Counter()
    for c,sz,at in secs:
        st=sz//c if c else 0
        for i in range(c):
            rs[struct.unpack('>h',b[at+i*st+6:at+i*st+8])[0]]+=1
    placed[n]=rs
allfiles=DISC.glob(r'^Levels/Level[^/]+/')
for name in ['-1']+['%02d'%i for i in range(18)]:
    files=[f.split('/')[-1] for f in allfiles if f.startswith('Levels/Level%s/'%name)]
    n=int(name)
    sc=os.path.join(root,'L'+name)
    ent={'level':name,'disc_files':len(files),'has':{'scripts':any(f.startswith('ScrLvl') for f in files),'u_level':any(f.startswith('U_Level') for f in files),'room_text':any(f.startswith('URmTxt') for f in files),'npcs':('Npcs%d.npc'%n in DISC.files)},
         'script_count':len(os.listdir(sc)) if os.path.isdir(sc) else 0,'room_text_entries':len(text.get('room'+name,{})),
         'audio_and_cinematics':sorted(f for f in files if f.endswith('.aud') or f.startswith('cin'))[:12]}
    if 0<=n<=16: ent['model_0x80007C8C']=model[n]
    if 0<=n<=17: ent['start_room_0x8023D738']=start[n]
    if n in who: ent['who']=who[n]
    if n in page: ent['chapter_page_label']=page[n]
    if n in remap and remap[n]!=n: ent['remap_0x8006C708_loads']=remap[n]
    if n in notes: ent['note']=notes[n]
    ent['placed_rooms_Npcs']=sorted(placed.get(n,{}))
    # door walk
    if ent['has']['scripts'] and name not in ('-1','16'):
        seeds=set(placed.get(n,{}))|({start[n]} if n in start and str(start.get(n)) in rooms else set())
        seeds-={5,83}
        seen=set(seeds);q=list(seeds);edges={}
        while q:
            r=q.pop()
            v=rooms.get(str(r))
            if not v: continue
            for g in v['regions']:
                for s in (g.get(k,-1) for k in ('enter','leave','stay','other_enter','other_leave','other_stay','cross_a','cross_b')):
                    if s<=0: continue
                    for d in dests(name,s,set()):
                        if d not in seen and str(d) in rooms:
                            seen.add(d);q.append(d)
        ent['rooms_by_door_walk_from_those']=sorted(seen)
    levels[name]=ent
out={'_how':'Rooms: placed_rooms_Npcs is every room named by the Npcs<level>.npc placement file (+6 of each record, read by 0x800417D8 and neighbours). rooms_by_door_walk starts from those rooms and the start room (without 5 and 83, which every level names), and follows fn66(room,...) in the region scripts of that room in that level, plus fn73(n) one chain. Model is the jump table of 0x80007C8C. start_room is the word table 0x8023D738 read by ed87 (0x800154FC). who comes from how often names appear in the level room text (args/names.py), so it is inferred; chapter_page_label is the item whose use script calls ed87 for that level, and is a label, not always the person (level 4 JOSEPH, level 8 ABDUL). Model is what the table says: the host can override it with a chosen character (player_model in runtime/host/hle.c). See arguments.md.','chapter_read_at':'0x80307178 (0x80307170 +8)','levels':levels}
json.dump(out,open('build/ed-db/facts/levels.json','w'),indent=1)
for k,v in levels.items():
    pass
    #print(k,v.get('who'),'model',v.get('model_0x80007C8C'),'start',v.get('start_room_0x8023D738'),'scripts',v['script_count'],'text',v['room_text_entries'],'files',v['disc_files'],v['has'])
# reverse
rev=collections.defaultdict(list)
for k,v in levels.items():
    for r in v.get('rooms_by_door_walk_from_those',v['placed_rooms_Npcs']): rev[r].append(k)
all_r=set(int(x) for x in rooms)
print('rooms with no level',sorted(all_r-set(rev)))
print('rooms in many',{r:l for r,l in rev.items() if len(l)>1})
