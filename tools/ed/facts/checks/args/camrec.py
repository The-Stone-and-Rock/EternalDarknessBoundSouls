import sys,struct; sys.path.insert(0,'tools/ed')
import savestate
s=savestate.State(sys.argv[1])
# room struct from TB[2]+124 array walk: find by r13-?; use eye pointer
e=s.u32(0x80603288+124); t=s.u32(0x80602C28+124)
print('eye elem',hex(e),'target elem',hex(t))
def dump(a):
    b=s.bytes(a,120)
    print(' pos',struct.unpack('>3h',b[:6]),'+6',b[6:8].hex(),'axis',struct.unpack('>3f',b[8:20]),'angle',struct.unpack('>f',b[20:24])[0],'+24',b[24:32].hex(),'far',struct.unpack('>f',b[32:36])[0],'fov',struct.unpack('>f',b[36:40])[0],'+40',b[40:52].hex(),'hash',hex(struct.unpack('>I',b[52:56])[0]),'name',b[56:96].split(b'\0')[0],'+96',b[96:120].hex())
dump(e);dump(t)
print(s.cstr(e+56),s.cstr(t+56))
def keys(a):
    n=s.u16(a+104); p=s.u32(a+108); m=s.u16(a+112); q=s.u32(a+116); n2=s.u16(a+96); p2=s.u32(a+100)
    print('n96',n2,hex(p2),'n104',n,hex(p),'n112',m,hex(q))
    for i in list(range(min(n,5)))+[n-1]:
        if n: print(' k',i,s.u32(p+i*12),struct.unpack('>3h',s.bytes(p+i*12+4,6)),s.bytes(p+i*12+10,2).hex())
    for i in list(range(min(m,5)))+[m-1]:
        if m: print(' r',i,s.bytes(q+i*16,16).hex(),'|',s.u32(q+i*16),struct.unpack('>3f',s.bytes(q+i*16+4,12)))
keys(e);keys(t)
