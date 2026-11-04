# Facts drawn from args/cameras_all.json. Run: python -I tools/ed/facts/checks/args/cam_stats.py
import json,re,collections
W=json.load(open('build/ed-db/facts/cameras_all.json'))
kit=json.load(open('build/ed-db/rooms.json'))
same=diff=0
for v in W.values():
    for a,b in zip(v['eyes'],v['targets']):
        if a['n104']==b['n104']: same+=1
        else: diff+=1
print('eye and target key counts equal',same,'differ',diff)
print('room 147 Camera01',[ (c['pos'],c['first_key'],c['last_key'],c['frames']) for c in W['147']['eyes'] if c['name']=='Camera01'])
print('eye last key differs from pos',sum(1 for v in W.values() for c in v['eyes'] if c['n104'] and tuple(c['last_key'][1:])!=tuple(c['pos'])),'of',sum(1 for v in W.values() for c in v['eyes'] if c['n104']))
miss=[];ok=0
for r,v in W.items():
    kh={c['hash'] for c in kit.get(r,{'cameras':[]})['cameras']}
    for c in v['eyes']:
        if c['hash'] in kh or not re.fullmatch(r'Camera\d+[A-Za-z]*',c['name']): continue
        miss.append((int(r),c['name'],c['hash']))
print('Camera## eyes not in the kit',len(miss),'hash low byte is a letter, digit or _',sum(1 for m in miss if re.match(rb'[A-Za-z0-9_]',bytes([m[2]&0xff]))),miss[:3],[m for m in miss if m[0]==24][:3])
print('room 147 eyes on disc',W['147']['count'],'in kit',len(kit['147']['cameras']))
tot=sum(v['count'] for v in W.values()); mv=sum(1 for v in W.values() for c in v['eyes'] if c['n104'])
print('eyes',tot,'moving',mv,round(100*mv/tot,1),'percent. rooms',len(W),'rooms with no camera',[r for r,v in W.items() if v['count']==0])
names=collections.Counter(re.sub(r'\d+.*','',c['name']) for v in W.values() for c in v['eyes'])
print(names.most_common(12))
