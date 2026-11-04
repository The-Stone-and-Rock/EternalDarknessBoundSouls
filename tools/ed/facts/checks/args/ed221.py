import sys,json,re,collections
sys.path.insert(0,'tools/ed/facts/checks/args'); sys.path.insert(0,'tools/ed/facts/checks/args')
from calls import calls
T=json.load(open('build/ed-db/facts/_tags.json')) if 0 else None
text=json.load(open('build/ed-db/text.json'))
def words(l,g,e):
    v=text.get('room'+l,{}).get('%s,%s'%(g,e))
    return (v[1],v[2][:70]) if v else None
rows=list(calls('ed221'))
print(len(rows))
ns=collections.Counter(a[3] for r,a in rows); print('n',ns)
ys=[(r,a) for r,a in rows if a[5]!='-1']
for r,a in ys:
    l=r.split('/')[0][1:]
    print(r,a,'\n   X present ->',words(l,a[0],a[1]),'\n   X gone ->',words(l,a[0],a[5]) if a[5].lstrip('-').isdigit() else a[5])
print(collections.Counter(a[3] for r,a in calls('ed219')))
print(sum(1 for _ in calls('ed220')),collections.Counter(len(a) for r,a in calls('ed220')))
