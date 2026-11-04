import sys,struct; sys.path.insert(0,'tools/ed')
import savestate
s=savestate.State(sys.argv[1])
print('fn25',s.sda(-23276),'cam idx r13-26864',s.s32(s.r13-26864),'r13-26872',s.s32(s.r13-26872),'stack r13-21700',s.s32(s.r13-21700))
for name,base in (('TA',0x80602B18),('TB',0x80603178)):
    for i in range(12):
        a=base+i*136
        fl=[round(s.f32(a+o),2) for o in (0,4,8)]
        print(name,i,hex(a),'pos',fl,'+12',hex(s.u32(a+12)),'+16',hex(s.u32(a+16)),'48',round(s.f32(a+48),3),'52',round(s.f32(a+52),3),'56',round(s.f32(a+56),3),'60',round(s.f32(a+60),3),'64',s.s32(a+64),'68',s.s32(a+68),'72..84',[round(s.f32(a+o),2) for o in(72,76,80)],'84',hex(s.u32(a+84)),'88',hex(s.u32(a+88)),'92',hex(s.u32(a+92)),'96',hex(s.u32(a+96)),'100',hex(s.u32(a+100)),'104',hex(s.u32(a+104)),'108',hex(s.u32(a+108)),'112',hex(s.u32(a+112)),'116',hex(s.u32(a+116)),'120',hex(s.u32(a+120)),'124',hex(s.u32(a+124)),'128',hex(s.u32(a+128)),'132',hex(s.u32(a+132)))
for nm,a in (('cur0 806037D8',0x806037D8),('cur1 80603860',0x80603860)):
    print(nm,[round(s.f32(a+o),2) for o in (0,4,8)],'48',round(s.f32(a+48),3),'52',round(s.f32(a+52),3),'64',s.s32(a+64),'108',hex(s.u32(a+108)),'112',hex(s.u32(a+112)))
print('view 803012E0',[round(s.f32(0x803012E0+4*i),2) for i in range(13)])
