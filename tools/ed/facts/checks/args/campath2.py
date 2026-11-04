import sys,struct,re,json; sys.path.insert(0,'tools/ed/facts/checks/args'); sys.path.insert(0,'tools/ed')
from _r import room
import savestate
s=savestate.State('saves/slot1.state')
x=room(147)
M=0x807fda40-0xC3380
print('mem base guess',hex(M),'match first 64',s.bytes(M,64)==x[:64],'file[:32]',x[:32].hex(),'mem[:32]',s.bytes(M,32).hex())
tbl=0x8058AFC0
i=s.u32(0x80590000-20544)
print('cur idx',i)
r=s.u32(0x80590000-20544+4*i+12); print('room struct',hex(r))
print('count u16@162',s.u16(r+162),'eye@164',hex(s.u32(r+164)),'@168',hex(s.u32(r+168)),'tgt@172',hex(s.u32(r+172)),'@176',hex(s.u32(r+176)),'@180',hex(s.u32(r+180)))
print('file-rel: r-M',hex(r-M))
print('words mem@r+160',[hex(s.u32(r+160+o)) for o in range(0,32,4)])
print('words file@160',[hex(struct.unpack('>I',x[160+o:160+o+4])[0]) for o in range(0,32,4)])
