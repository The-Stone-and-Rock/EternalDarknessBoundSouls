"""python -I tools/ed/facts/checks/reconcile-usemix/scanwords.py HEX... : every place in main.dol holding that word, with its neighbours."""
import sys,struct
sys.path.insert(0,'tools/ed')
import disc
d=disc.Dol()
for x in sys.argv[1:]:
    w=int(x,16); b=struct.pack('>I',w); i=d.data.find(b)
    while i>=0:
        a=d.addr(i)
        if a and i%4==0:
            print(x,'at %08X'%a,' '.join('%08X'%(d.word(a+4*k) or 0) for k in range(-4,5)))
        i=d.data.find(b,i+1)
