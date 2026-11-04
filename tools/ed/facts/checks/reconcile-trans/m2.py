import sys,struct,json
sys.path.insert(0,'tools/ed')
import disc,gametext,savestate
d=disc.Disc()
def expand(path):
    r=d.read(path)
    first=struct.unpack('>I',r[4:8])[0];csize=struct.unpack('>I',r[12:16])[0];sec1=struct.unpack('>I',r[16:20])[0]
    return bytes(gametext.bpe_expand(r,first,min(first+csize,len(r)))),sec1
mk=json.load(open('build/ed-db/facts/skeptic/trans/markers2.json'))
for rid in (147,21,155):
    x,sec1=expand('Rooms/rmed%04d.bpe'%rid)
    c=mk[str(rid)]['cands']
    print(rid,'n',mk[str(rid)]['n'],'cand',c,hex(c[0]),'len',hex(len(x)),'sec1',hex(sec1))
    print(' hdr',[hex(struct.unpack('>I',x[i:i+4])[0]) for i in range(160,260,4)])
    # search for the offset as a big endian word in header
    o=struct.pack('>I',c[0])
    print(' occurrences of offset as word',[i for i in range(0,len(x)-3) if x[i:i+4]==o][:5])
    s=savestate.State('saves/slot1.state')
    if rid==147:
        room=s.u32(0x8058AFC0+12)
        print(' live room',hex(room),'rec',hex(s.u32(room+160+56)),'diff',hex(s.u32(room+160+56)-room),'words',[hex(s.u32(room+i)) for i in range(160,260,4)])
