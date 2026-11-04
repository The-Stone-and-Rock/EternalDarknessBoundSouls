import sys,struct,re,json,glob,os; sys.path.insert(0,'tools/ed/facts/checks/args'); sys.path.insert(0,'tools/ed')
from _npc2 import parse,D
levels=[int(re.search(r'Npcs(\d+)',p).group(1)) for p in D.glob(r'^Npcs\d+\.npc$')]
print(sorted(levels))
T={}
rooms={}
for L in sorted(levels):
    b,secs=parse(L)
    rooms[L]={}
    for si,(c,sz,at) in enumerate(secs):
        st=sz//c if c else 0
        for i in range(c):
            r=b[at+i*st:at+(i+1)*st]
            room=struct.unpack('>h',r[6:8])[0]
            rooms[L].setdefault(room,[0,0,0,0,0])[si]+=1
            if si==2:
                model,item,cls,a34,a36,typ,tag=struct.unpack('>7H',r[28:42])
                T.setdefault(L,{}).setdefault(tag,[]).append(dict(room=room,pos=struct.unpack('>3h',r[:6]),item=item,model=model,cls=cls,a34=a34,type=typ,mask=r[11]))
            if si==4:
                vals=struct.unpack('>12H',r[32:56]); tag=struct.unpack('>H',r[56:58])[0]
                T.setdefault(L,{}).setdefault(tag,[]).append(dict(room=room,pos=struct.unpack('>3h',r[:6]),per_alignment=[dict(model=vals[i],cls=vals[i+1],type=vals[i+2],item=vals[i+3]) for i in (0,4,8)],mask=r[11]))
            if si in(0,):
                pass
    print('L',L,'counts',[s[0] for s in secs],'rooms',sorted(rooms[L]))
# ed221 cross check
pat=re.compile(r'ed221\s*\(([^()]*)\)')
ok=bad=0; miss=[]
for f in glob.glob('build/ed-db/scripts/L*/*.lua'):
    lv=os.path.basename(os.path.dirname(f))[1:]
    t=open(f,encoding='utf8',errors='replace').read()
    for m in pat.finditer(t):
        a=[x.strip() for x in m.group(1).split(',')]
        try: X=int(a[4])
        except: continue
        if lv.lstrip('-').isdigit() and int(lv) in T and X in T[int(lv)]:
            ok+=1
        else:
            bad+=1; miss.append((lv,os.path.basename(f),X))
print('ed221 X found in same-level pickups',ok,'not',bad)
print(miss[:80])
json.dump({str(k):{str(t):v for t,v in vv.items()} for k,vv in T.items()},open('build/ed-db/facts/_tags.json','w'))
print(T[2].get(2029),T[3].get(2154))
