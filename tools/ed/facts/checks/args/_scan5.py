import sys,struct,re; sys.path.insert(0,'tools/ed')
import disc
d=disc.Disc()
u=open('tools/ed/facts/checks/args/_U3.bin','rb').read()
pairs=[(2154,225),(2156,225),(2155,225),(2028,71),(645,40),(849,None),(851,None),(882,None),(883,None),(340,None),(282,None)]
for name,x in (('U3',u),('gpk',d.read('Levels/Level03/ScrLvl03.gpk'))):
    print(name,len(x))
    for t,i in pairs:
        H=[m.start() for m in re.finditer(re.escape(struct.pack('>H',t)),x)]
        I=[m.start() for m in re.finditer(re.escape(struct.pack('>I',t)),x)]
        D=[m.start() for m in re.finditer(re.escape(struct.pack('>d',float(t))),x)]
        DL=[m.start() for m in re.finditer(re.escape(struct.pack('<d',float(t))),x)]
        print(t,i,'H',len(H),'I',[hex(a) for a in I][:8],'dbl',[hex(a) for a in D][:8],[hex(a) for a in DL][:8])
