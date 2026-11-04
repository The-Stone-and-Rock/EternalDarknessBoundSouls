import sys,glob;sys.path.insert(0,'tools/ed');import savestate
for f in sorted(glob.glob('saves/*.state')):
    s=savestate.State(f)
    if not s.sda(-21468) or s.s32(s.r13-23276)<0: continue
    v=[s.f32(0x803012E0+o) for o in (0,4,8)]
    shown=[s.f32(0x806037D8+o) for o in (0,4,8)]
    c=s.s32(s.r13-26864); tb=[s.f32(0x80603178+c*136+o) for o in (0,4,8)]
    # node type 0 with tag
    h=s.sda(-21468); n=s.u32(h+4); t0=0; ex=None; total=0
    while n:
        total+=1
        if s.s32(n+8)==0:
            t0+=1
            d=s.u32(n+36)
            if d and not ex: ex=(s.s32(n+16),s.s32(d+24))
        n=s.u32(n+4)
    st=[s.s32(0x80603178+c*136+32),s.s32(0x80602B18+c*136+32)]
    print(f[6:12],'cam',c,'view-shown',round(max(abs(a-b) for a,b in zip(v,shown)),2),'view-TB',round(max(abs(a-b) for a,b in zip(v,tb)),2),'eye/target +32',st,'+36',s.s32(0x80603178+c*136+36),'nodes',total,'type0',t0,'first tagged type0 (handle,tag)',ex)
