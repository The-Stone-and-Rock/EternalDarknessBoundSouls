import sys,struct,glob,json; sys.path.insert(0,'tools/ed')
import savestate
for f in sorted(glob.glob('saves/*.state')):
    if 'boot' in f or 'title' in f: continue
    s=savestate.State(f)
    cam=s.s32(s.r13-26864)
    TB=0x80603178+cam*136; TA=0x80602B18+cam*136
    e=s.u32(TB+124); t=s.u32(TA+124)
    if not e: print(f,cam,'no elem'); continue
    print(f[6:],'room',s.s32(s.r13-23276),'cam idx',cam,s.cstr(e+56),'hash',hex(s.u32(e+52)),'file-pos',(s.u16(e),),struct.unpack('>3h',s.bytes(e,6)),'n96',s.u16(e+96),'n104',s.u16(e+104),'n112',s.u16(e+112),
      '| eye now',[round(s.f32(TB+o),1) for o in (0,4,8)],'mode+36',s.s32(TB+36),'step108',hex(s.u32(TB+108)),'param+120',round(s.f32(TB+120),2),
      '| tgt now',[round(s.f32(TA+o),1) for o in (0,4,8)],'tgt elem pos',struct.unpack('>3h',s.bytes(t,6)) if t else None,'step',hex(s.u32(TA+108)),'+104',hex(s.u32(TA+104)),'param',round(s.f32(TA+120),2),
      '| view',[round(s.f32(0x803012E0+o),1) for o in (0,4,8)])
