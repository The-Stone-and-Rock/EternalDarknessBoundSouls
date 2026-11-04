from lib import *
import glob,re,collections
pl=json.load(open('tools/ed/facts/checks/skeptic/usemix/placed.json'))
lit=collections.defaultdict(set)
for p in glob.glob('build/ed-db/scripts/L*/*.lua'):
    t=open(p,encoding='utf8',errors='replace').read()
    lv=os.path.basename(os.path.dirname(p));i=os.path.basename(p)[:-4]
    for m in re.finditer(r'ed3\(([^,]+), *[^,]+, *[^,]+, *(\d+),',t):
        lit[int(m.group(2))].add(lv+'/'+i)
for k in (35,188,36,37,121,122,123,128,129,130,131,159,186,187,214,217,236,237,238,209,210,211,212,213):
    print(k,name(k)[2][:30],'placed',[(e[0],e[2]) for e in pl.get(str(k),[])][:4],'ed3',sorted(lit.get(k,[]))[:6])
# ed3 with non-literal kind count
c=0
for p in glob.glob('build/ed-db/scripts/L*/*.lua'):
    t=open(p,encoding='utf8',errors='replace').read()
    for m in re.finditer(r'ed3\(([^,]+), *[^,]+, *[^,]+, *([A-Za-z_]\w*),',t):
        if m.group(2) not in('NONE',): c+=1; print('var kind',os.path.basename(os.path.dirname(p)),os.path.basename(p),m.group(0))
print('non-literal kind ed3',c)
s=states()
for n in ('coop-apart','slot10','slot8','slot5'):
    st=s[n];tb=st.sda(-26528)
    print(n,'flag894',(st.u32(tb+4+((894>>3)&~3))>>(894&31))&1,'601',(st.u32(tb+4+((601>>3)&~3))>>(601&31))&1)
