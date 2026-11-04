# Helpers for reading the game's disassembly (research only).
import json,os,re,sys,bisect
ROOT=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..','..'))
sys.path.insert(0,os.path.join(ROOT,'tools','ed'))
DIS=os.path.join(ROOT,'build','ed-db','disasm.txt')
_lines=[l for l in open(DIS,encoding='utf8').read().split('\n') if len(l)>9 and l[8]==':']
ADDR=[int(l[:8],16) for l in _lines]
TXT=[l[10:].strip() for l in _lines]
IDX={a:i for i,a in enumerate(ADDR)}
CALLS=json.load(open(os.path.join(ROOT,'build','ed-db','calls.json')))
NAT={}
for l in open(os.path.join(ROOT,'build','ed-db','natives.tsv'),encoding='utf8'):
    p=l.rstrip('\n').split('\t')
    if len(p)>=2: NAT.setdefault(int(p[1],16),p[0])
def fnstart(a):
    i=IDX[a]
    while i>0:
        t=TXT[i]
        if re.match(r'stwu\s+r1, -\d+\(r1\)',t):
            # previous must be a terminator-ish: blr, b, or padding
            return ADDR[i]
        if t.startswith('blr') :
            return ADDR[i+1]
        i-=1
    return None
def callers(a):
    return [int(x,16) for x in CALLS.get('%08X'%a,[])]
def callerfns(a):
    return sorted(set(fnstart(x) for x in callers(a)))
def show(a,n=40):
    i=IDX[a]
    for k in range(i,min(i+n,len(ADDR))):
        print('%08X: %s'%(ADDR[k],TXT[k]))
def fnrange(a):
    """a to its last blr before the next function start (approx)."""
    i=IDX[a]+1
    while i<len(ADDR):
        t=TXT[i]
        if re.match(r'stwu\s+r1, -\d+\(r1\)',t): return ADDR[IDX[a]],ADDR[i-1]
        i+=1
    return a,ADDR[-1]
def addis(hi,lo):
    return ((hi&0xffff)<<16)+lo & 0xffffffff
def nm(a):
    if a in NAT: return NAT[a]
    return ''
def up(a,depth=4,seen=None,ind=0):
    seen=seen if seen is not None else set()
    s=fnstart(a) if a in IDX else a
    print(' '*ind+'%08X %s'%(s,nm(s)))
    if s in seen or depth==0: return
    seen.add(s)
    for c in callerfns(s):
        up(c,depth-1,seen,ind+2)

import struct as _s
def _dol():
    import disc
    return disc.Dol()
def dataptrs(target, D=None):
    """Words in the dol's data sections equal to target, and lis+addi pairs in code that build it."""
    D=D or _dol()
    out=[]
    pat=_s.pack('>I',target)
    o=0
    while True:
        o=D.data.find(pat,o)
        if o<0: break
        a=D.addr(o)
        if a and o%4==0: out.append(a)
        o+=4
    return out
def codeptrs(target):
    hi=(target>>16)&0xffff; lo=target&0xffff
    if lo>=0x8000: hi=(hi+1)&0xffff
    los=lo if lo<0x8000 else lo-0x10000
    his=hi if hi<0x8000 else hi-0x10000
    res=[]
    for i,t in enumerate(TXT):
        if t.startswith('addi') and t.endswith(', %d'%los):
            for k in range(max(0,i-12),i):
                if TXT[k].startswith('lis') and TXT[k].endswith(', %d'%his):
                    res.append(ADDR[i]); break
        if t.startswith('ori') and False: pass
    return res

def compact(a,n=None,end=None):
    """Only the calls, branches on constants and li: a quick shape of a function."""
    s=IDX[a]
    if end is None: 
        end=IDX[fnrange(a)[1]] if n is None else s+n
    else: end=IDX[end]
    for k in range(s,end+1):
        t=TXT[k]
        if t.startswith('bl ') or t.startswith('cmp') or t.startswith('bctr') or t.startswith('li ') or t.startswith('blr') or t.startswith('b ') or 'rlwinm.' in t or re.match(r'(lwz|lha|lbz|stw|stb|sth)\S* +r\d+, -?\d+\(r(13)\)',t):
            print('%08X %s'%(ADDR[k],t))
