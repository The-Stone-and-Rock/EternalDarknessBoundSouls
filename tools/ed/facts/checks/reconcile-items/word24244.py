"""Value of the word ed82 tests (r13-24244), with the Tome state word (r13-24492), over every state. Read only."""
import sys,glob,os,collections
HERE=os.path.dirname(os.path.abspath(__file__));ROOT=os.path.abspath(os.path.join(HERE,'..','..','..'))
sys.path.insert(0,os.path.join(ROOT,'tools','ed'));os.chdir(ROOT)
import savestate
c=collections.Counter();ex={}
for p in sorted(glob.glob('**/*.state',recursive=True)):
    s=savestate.State(p);v=s.sda(-24244);t=s.sda(-24492);k=(v,t);c[k]+=1;ex.setdefault(k,p)
print(sorted(c.items()))
for k,p in sorted(ex.items()): print(k,p)
s=savestate.State('saves/slot10.state') if os.path.exists('saves/slot10.state') else None
if s:
    head=s.sda(-21468);n=s.u32(head+4);out=[]
    while n:
        st=s.u32(n+36);info=s.u32(n+44)
        if st and info and s.s32(st+24) in (678,733): out.append((s.u32(n+16),s.s32(info+12),s.s32(st+24)))
        n=s.u32(n+4)
    print('slot10 tags 678/733',out)
