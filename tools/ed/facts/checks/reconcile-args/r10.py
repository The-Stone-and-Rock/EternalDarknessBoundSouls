import sys,json,collections,struct
sys.path.insert(0,'tools/ed');sys.path.insert(0,'tools/ed/facts/checks/args')
W=json.load(open('build/ed-db/facts/cameras_all.json'))
print('target fov values',collections.Counter(c['fov'] for v in W.values() for c in v['targets']).most_common(4))
print('eye fov 0',sum(1 for v in W.values() for c in v['eyes'] if c['fov']==0))
import savestate
s=savestate.State('saves/slot1.state')
a=0x80590000-20544
rs=s.u32(a+s.u32(a)*4+12); t=s.u32(rs+172)
print('live target 0 name/hash',s.bytes(t+52,16).hex(),'fov',s.f32(t+36))
