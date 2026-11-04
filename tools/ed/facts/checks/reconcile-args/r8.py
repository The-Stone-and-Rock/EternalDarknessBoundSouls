import sys,re,glob,collections;sys.path.insert(0,'tools/ed');sys.path.insert(0,'tools/ed/facts/checks/args')
import disc
from calls import calls
D=disc.Dol()
c=collections.Counter();ex={}
for r,a in calls('ed87'):
    k=tuple(a); c[k]+=1; ex.setdefault(k,[]).append(r)
for k,v in sorted(c.items(),key=lambda kv:str(kv[0])):
    print(k,v,ex[k][:3])
s=open('build/ed-db/scripts/L-1/1600.lua',encoding='utf8',errors='replace').read()
print([l for l in s.split('\n') if 'gInsEffectID' in l or 'fn79' in l or 'ed61' in l][:6])
for n in ('2246','2244','1056'):
    fs=glob.glob('build/ed-db/scripts/L*/%s.lua'%n)
    print(n,fs[:2],[l for l in open(fs[0],encoding='utf8',errors='replace').read().split('\n') if 'fn79' in l][:3])
