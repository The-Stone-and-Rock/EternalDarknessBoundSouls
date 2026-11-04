import sys,struct; sys.path.insert(0,'tools/ed')
import savestate
s=savestate.State(sys.argv[1])
p=s.sda(-23484); print('r13-23484',hex(p),[hex(s.u32(p+o)) for o in range(0,64,4)])
cnt=s.u32(p+12); arr=s.u32(p+16)
print('n',cnt,hex(arr))
for i in range(min(cnt,int(sys.argv[2]) if len(sys.argv)>2 else 20)):
    a=arr+i*104
    b=s.bytes(a,104)
    print(i,'pos',struct.unpack('>3h',b[:6]),'+6',struct.unpack('>h',b[6:8])[0],'+8..',b[8:60].hex(),'+60 v3',struct.unpack('>3f',b[60:72]),'a',struct.unpack('>f',b[72:76])[0],'76..',struct.unpack('>14H',b[76:104]))
