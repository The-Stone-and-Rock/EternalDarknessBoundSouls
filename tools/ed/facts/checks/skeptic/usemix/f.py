from lib import *
st=states()
for n,s in st.items():
    t=s.sda(-26528)
    if not 0x80000000<=t<0x81800000: print(n,'no flag table',hex(t));continue
    def fl(i):
        return (s.u32(t+4+((i>>3)&~3))>>(i&31))&1
    print(n,'room',s.sda(-23276),'flags 599..603',[fl(i) for i in range(599,604)], 'r13-24244',s.s32(s.r13-24244),'r13-24492',s.sda(-24492))
