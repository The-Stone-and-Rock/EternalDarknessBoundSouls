from lib import *
import re,glob,os
for k in list(range(118,125))+[35,36,37,188,128,129,130,131,159,186,187,214,217,236,237,238,162,163]+list(range(200,214)):
    r=U[k]; print(k,name(k)[1:], 'pick',s16(r,36),'use',s16(r,38),'chk',s16(r,40),'mix',s16(r,42),'flags',hex(struct.unpack('>I',r[4:8])[0]),'txt',r[76],r[79])
targets={1588,1589,2514}
for k,r in enumerate(U):
    for o in (36,38,40,42):
        if s16(r,o) in targets: print('REC',k,o,s16(r,o))
# labels with INV having no kind
used={(r[76],r[79]) for r in U}
labs=[(k,v[1]) for k,v in T.items() if v[1].endswith('_INV') and tuple(map(int,k.split(','))) not in used]
print(len(labs),labs)
for w in ('BLANKONETHIRD','BLANKTWOTHIRD','BLANKPAGE','GRN_SHARD2','GRN_SHARD3','GRN_URN_FIX'):
    print(w,[ (k,v[1],v[2][:60]) for k,v in T.items() if w in v[1]])
pat=re.compile(r'\b(1588|1589|2514)\b')
for p in glob.glob('build/ed-db/scripts/L*/*.lua'):
    t=open(p,encoding='utf8',errors='replace').read()
    for l in t.splitlines():
        if pat.search(l): print(p[20:],l)
pat=re.compile(r'[(, ](209|210|211|212)[,) ]')
c=0
for p in glob.glob('build/ed-db/scripts/L*/*.lua'):
    t=open(p,encoding='utf8',errors='replace').read()
    for l in t.splitlines():
        if pat.search(l) and re.match(r'.*\bed3\(',l): print(p[20:],l)
