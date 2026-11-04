"""python -I tools/ed/facts/checks/reconcile-usemix/scripts.py : script-side counts behind use-mix.md, from build/ed-db/scripts and mix_table.json."""
import glob,json,os,re,collections
d=json.load(open('build/ed-db/facts/mix_table.json'))
K=d['kinds_InvU']
S=collections.defaultdict(dict)
for f in glob.glob('build/ed-db/scripts/L*/*.lua'):
    lvl=os.path.basename(os.path.dirname(f));sid=int(os.path.basename(f)[:-4])
    S[sid][lvl]=open(f,encoding='utf8',errors='replace').read()
def has(sid,pat): return any(re.search(pat,t) for t in S.get(sid,{}).values())
use=sorted({k['use_script'] for k in K if k['use_script']})
cnt=collections.Counter();other=[]
for s in use:
    a=has(s,r'ed20\(0\)');b=has(s,r'fn85\(pTrigger\)');c=has(s,r'ed158\(');e=has(s,r'ed20\(')
    if not s in S: other.append((s,'no script')); continue
    cnt['ed20(0)' if a else 'no ed20(0)']+=1
    if not a: other.append((s,'fn85(pTrigger)' if b else '','ed158' if c else '',[k['kind'] for k in K if k['use_script']==s],sorted(S[s])))
print('distinct Use scripts',len(use),dict(cnt)); print(' without ed20(0):',other)
print('Use scripts with both ed20(0) and fn85(pTrigger):',[s for s in use if has(s,r'ed20\(0\)') and has(s,r'fn85\(pTrigger\)')])
mix=collections.defaultdict(list)
for k in K:
    if k['mix_script']: mix[k['mix_script']].append(k['kind'])
print('kinds with a mix script',sum(len(v) for v in mix.values()),{s:len(v) for s,v in mix.items()})
for s in (1582,2013,2221):
    print(s,'levels',sorted(S[s]),[(k,K[k]['text_group'],K[k]['name']) for k in mix[s]])
print('text groups of ammo-script kinds',collections.Counter(K[k]['text_group'] for s in (1582,2013,2221) for k in mix[s]))
print('fn92 users',sorted(s for s in S if has(s,r'fn92\(')))
print('ed140 users',sorted(s for s in S if has(s,r'ed140\(')))
print('ed167 users',sorted((s,sorted(S[s])) for s in S if has(s,r'ed167\(')))
print('pickup scripts of ed167 users',[(k['kind'],k['name'],k['pickup_script']) for k in K if k['pickup_script'] in (1978,483,448)])
print('kinds named Dual',[(k['kind'],k['name'],k['pickup_script'],k['mix_script']) for k in K if k['name'] and 'Dual' in k['name']])
print('fn162 values for which=1',sorted({(s,m) for s in S for t in S[s].values() for m in re.findall(r'fn162\(\w+, 1, (-?\w+)\)',t)}))
print('fn162 which=0 value 864',sorted({(l,s) for s in S for l,t in S[s].items() if re.search(r'fn162\(\w+, 0, 864\)',t)}))
print('fn162 which counts',collections.Counter(m for s in S for t in S[s].values() for m in re.findall(r'fn162\(\w+, (\d+),',t)))
print('renames',sorted({(l,s) for s in S for l,t in S[s].items() if re.search(r'fn162\(\w+, [34],',t)}))
print('ed9 users',len([s for s in S if has(s,r'\bed9\(')]))
onrec={k[f] for k in K for f in ('pickup_script','use_script','check_script','mix_script')}
print('ed9 users not on a record',sorted(s for s in S if has(s,r'\bed9\(') and s not in onrec))
print('ed9 3-arg forms',sorted({m for s in S for t in S[s].values() for m in re.findall(r'\bed9\(([^)]*,[^)]*)\)',t)}))
c=collections.Counter(m for s in S for t in S[s].values() for m in re.findall(r'ed11\((\d+, \d+), 0\)',t))
print('ed11 refusals with group,entry,0 in Use scripts:',collections.Counter(m for s in use for t in S.get(s,{}).values() for m in re.findall(r'ed11\((\d+, \d+), 0\)',t)).most_common(8))
print('Use scripts with ed11(0, 0, 0)',len([s for s in use if has(s,r'ed11\(0, 0, 0\)')]))
print('occurrences ed11(0, 0, 0) all scripts',sum(len(re.findall(r'ed11\(0, 0, 0\)',t)) for v in S.values() for t in v.values()))
for sid in (1588,1589,2514):
    print(sid,'named elsewhere',sorted({(l,s) for s in S for l,t in S[s].items() if s!=sid and re.search(r'\b%d\b'%sid,t)})[:6],'on record',sid in onrec)
print('flag 601/602 writers',sorted({(l,s,m) for s in S for l,t in S[s].items() for m in re.findall(r'fn29\((60[12]), \d\)',t)}))
print('kind 82 Chakrams',K[82]); print('kind 91',K[91]['mix_script'],K[91]['name'],'kind 243',K[243]['mix_script'])
print('count +0 == 16 kinds',[k['kind'] for k in K if k['mode']==16])
print('flag 1 kinds',[k['kind'] for k in K if k['flags']&1],'flag 0x20',[k['kind'] for k in K if k['flags']&0x20],'other flags',[(k['kind'],hex(k['flags'])) for k in K if k['flags'] & ~0x21])
print('check script kinds',[(k['kind'],k['check_script']) for k in K if k['check_script']])
print('names on two+ kinds',{n:v for n,v in collections.Counter(k['name'] for k in K).items() if v>1 and n})
rooms=json.load(open('build/ed-db/rooms.json',encoding='utf8'))
rk=set(map(str,rooms.keys())) if isinstance(rooms,dict) else set()
lits=collections.defaultdict(set)
for s in S:
    for l,t in S[s].items():
        for m in re.findall(r'fn25\(\) (?:~=|==|<=|>=|<|>) (-?\d+)',t): lits[m].add('%s/%d'%(l,s))
print('fn25 literals',len(lits),'not a room key:',{k:sorted(v)[:3] for k,v in lits.items() if k not in rk},'(rooms.json has %d keys)'%len(rk))
print('ed9 first arg differs from the script:',sorted({(l,s,m) for s in S for l,t in S[s].items() for m in re.findall(r'\bed9\((\d+)',t) if int(m)!=s})[:12])
print('ed3 kind literal for 209-213:',{k:sorted({'%s/%d'%(l,s) for s in S for l,t in S[s].items() if re.search(r'ed3\(-?\d+, -?\w+, -?\d+, %d\b'%k,t)}) for k in (209,210,211,212,213,35,122)})
print('kinds 209-213 pickup/mix',[(k['kind'],k['pickup_script'],k['mix_script'],k['use_script']) for k in K if 209<=k['kind']<=213])
print('placed via Npcs tables 3,5 of 209..213,35,122: see mix_table.json pieces.*.kind_sources (all empty except 213?)')
