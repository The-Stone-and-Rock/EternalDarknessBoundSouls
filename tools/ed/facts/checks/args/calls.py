import re,glob,collections,os,json,sys
def calls(name):
    pat=re.compile(r'(?<![A-Za-z0-9_])'+name+r'\(')
    for f in glob.glob('build/ed-db/scripts/L*/*.lua'):
        t=open(f,encoding='utf8',errors='replace').read()
        rel=f.replace(chr(92),'/').split('scripts/')[-1]
        for m in pat.finditer(t):
            i=m.end();d=1;args=[];cur=''
            while i<len(t) and d>0:
                c=t[i]
                if c=='(':d+=1
                elif c==')':
                    d-=1
                    if d==0:break
                if c==',' and d==1:
                    args.append(cur.strip());cur=''
                else:cur+=c
                i+=1
            if cur.strip():args.append(cur.strip())
            yield rel,args
if __name__=='__main__':
    idx=int(os.environ.get('IDX','0'));top=int(os.environ.get('TOP','40'))
    for n in sys.argv[1:]:
        c=collections.Counter();ex={};na=collections.Counter()
        for rel,a in calls(n):
            k=a[idx] if len(a)>idx else '-'
            c[k]+=1;ex.setdefault(k,[]).append((rel,','.join(a)));na[len(a)]+=1
        print('==',n,sum(c.values()),'argcounts',dict(na))
        for k,v in c.most_common(top):
            print(' ',k,v,ex[k][:2])
