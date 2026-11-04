import sys,glob; sys.path.insert(0,'tools/ed')
import savestate
for f in sorted(glob.glob('saves/*.state')):
    s=savestate.State(f)
    try:
        h=s.sda(-21468); n=s.u32(h+4); out=[]
        while n:
            if s.s32(n+8)==2:
                o=s.u32(n+52)
                out.append((s.s32(n+16),'model',s.s32(o+580) if o else None,'room',s.s32(o+592) if o else None,'n40+148',s.s32(s.u32(n+40)+148) if s.u32(n+40) else None))
            n=s.u32(n+4)
        print(f[6:],'chapter',s.s32(0x80307178),'fn25',s.s32(s.r13-23276),'align',s.s32(s.u32(s.r13-26320)) if s.u32(s.r13-26320) else None,'player r13-26524',hex(s.sda(-26524)),out)
    except Exception as e: print(f,'err',e)
