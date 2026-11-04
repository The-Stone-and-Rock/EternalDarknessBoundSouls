"""Runs the game's own *SK_ASC* decompressor (0x801412F8) from its disassembly, translated to Python, so the unpacking
can't differ from what the game does. Only the integer instructions that code uses are handled; anything else raises.
The disassembly is build/ed-db/disasm.txt (from the recompiler's comments in generated/chunks)."""
import re

M = 0xFFFFFFFF
UNPACK = 0x801412F8
CODE_LO, CODE_HI = 0x8013E000, 0x80141388
READ_CB, WRITE_CB, MEMSET, MEMCMP, STOP = 0xFFFF0000, 0xFFFF0010, 0x80003100, 0x800F56F4, 0xFFFFFFF0
WORK, STACK = 0x100000, 0x3F0000


def _mask(mb, me):
    a = M >> mb
    b = (M << (31 - me)) & M
    return (a & b) if mb <= me else (a | b)


def _mem(a):
    m = re.match(r'(-?\d+)\(r(\d+)\)', a)
    d, b = int(m.group(1)), int(m.group(2))
    return '%d' % d if b == 0 else '(r[%d]+%d)&M' % (b, d)


def _sx(e):
    return '((%s)^0x80000000)-0x80000000' % e


def _translate(op, a, addr, nxt):
    """Python lines for one instruction, and the next-pc expression if it ends a block."""
    R = lambda s: 'r[%s]' % s[1:]
    I = lambda s: int(s, 0)
    rec = op.endswith('.')
    o = op.rstrip('.')
    L = []
    end = None
    if o == 'lwz':
        L.append("e=%s;%s=fb(mem[e:e+4],'big')" % (_mem(a[1]), R(a[0])))
    elif o == 'lhz':
        L.append('e=%s;%s=mem[e]<<8|mem[e+1]' % (_mem(a[1]), R(a[0])))
    elif o == 'stw':
        L.append("e=%s;mem[e:e+4]=%s.to_bytes(4,'big')" % (_mem(a[1]), R(a[0])))
    elif o == 'sth':
        L.append("e=%s;mem[e:e+2]=(%s&0xFFFF).to_bytes(2,'big')" % (_mem(a[1]), R(a[0])))
    elif o == 'stwu':
        b = re.match(r'-?\d+\(r(\d+)\)', a[1]).group(1)
        L.append("e=%s;mem[e:e+4]=%s.to_bytes(4,'big');r[%s]=e" % (_mem(a[1]), R(a[0]), b))
    elif o == 'lhzx':
        L.append('e=(%s+%s)&M;%s=mem[e]<<8|mem[e+1]' % (R(a[1]), R(a[2]), R(a[0])))
    elif o == 'lbzx':
        L.append('%s=mem[(%s+%s)&M]' % (R(a[0]), R(a[1]), R(a[2])))
    elif o == 'stbx':
        L.append('mem[(%s+%s)&M]=%s&255' % (R(a[1]), R(a[2]), R(a[0])))
    elif o == 'sthx':
        L.append("e=(%s+%s)&M;mem[e:e+2]=(%s&0xFFFF).to_bytes(2,'big')" % (R(a[1]), R(a[2]), R(a[0])))
    elif o == 'stmw':
        L.append("e=%s\nfor i in range(%s,32):\n  mem[e:e+4]=r[i].to_bytes(4,'big');e+=4" % (_mem(a[1]), a[0][1:]))
    elif o == 'lmw':
        L.append("e=%s\nfor i in range(%s,32):\n  r[i]=fb(mem[e:e+4],'big');e+=4" % (_mem(a[1]), a[0][1:]))
    elif o == 'li':
        L.append('%s=%d' % (R(a[0]), I(a[1]) & M))
    elif o == 'lis':
        L.append('%s=%d' % (R(a[0]), (I(a[1]) << 16) & M))
    elif o == 'addi':
        L.append('%s=(%s+%d)&M' % (R(a[0]), R(a[1]), I(a[2])))
    elif o == 'addis':
        L.append('%s=(%s+%d)&M' % (R(a[0]), R(a[1]), I(a[2]) << 16))
    elif o == 'addic':
        L.append('t=%s+(%d&M);s.ca=t>>32;%s=t&M' % (R(a[1]), I(a[2]), R(a[0])))
    elif o == 'lbz':
        L.append('e=%s;%s=mem[e]' % (_mem(a[1]), R(a[0])))
    elif o == 'lha':
        L.append('e=%s;x=mem[e]<<8|mem[e+1];%s=(x-65536 if x&0x8000 else x)&M' % (_mem(a[1]), R(a[0])))
    elif o == 'extsb':
        L.append('x=%s&255;%s=(x-256 if x&128 else x)&M' % (R(a[1]), R(a[0])))
    elif o == 'extsh':
        L.append('x=%s&65535;%s=(x-65536 if x&32768 else x)&M' % (R(a[1]), R(a[0])))
    elif o == 'neg':
        L.append('%s=(-%s)&M' % (R(a[0]), R(a[1])))
    elif o == 'mulli':
        L.append('%s=((%s)*%d)&M' % (R(a[0]), _sx(R(a[1])), I(a[2])))
    elif o == 'mulhw':
        L.append('%s=(((%s)*(%s))>>32)&M' % (R(a[0]), _sx(R(a[1])), _sx(R(a[2]))))
    elif o == 'add':
        L.append('%s=(%s+%s)&M' % (R(a[0]), R(a[1]), R(a[2])))
    elif o == 'addze':
        L.append('t=%s+s.ca;s.ca=t>>32;%s=t&M' % (R(a[1]), R(a[0])))
    elif o == 'subf':
        L.append('%s=(%s-%s)&M' % (R(a[0]), R(a[2]), R(a[1])))
    elif o == 'subfc':
        L.append('t=(%s^M)+%s+1;s.ca=t>>32;%s=t&M' % (R(a[1]), R(a[2]), R(a[0])))
    elif o == 'subfe':
        L.append('t=(%s^M)+%s+s.ca;s.ca=t>>32;%s=t&M' % (R(a[1]), R(a[2]), R(a[0])))
    elif o == 'mullw':
        L.append('%s=((%s)*(%s))&M' % (R(a[0]), _sx(R(a[1])), _sx(R(a[2]))))
    elif o == 'divw':
        L.append('x=%s;y=%s;q=(abs(x)//abs(y)) if y else 0;%s=(q if (x<0)==(y<0) else -q)&M' % (_sx(R(a[1])), _sx(R(a[2])), R(a[0])))
    elif o == 'or':
        L.append('%s=%s|%s' % (R(a[0]), R(a[1]), R(a[2])))
    elif o == 'and':
        L.append('%s=%s&%s' % (R(a[0]), R(a[1]), R(a[2])))
    elif o == 'andc':
        L.append('%s=%s&(%s^M)' % (R(a[0]), R(a[1]), R(a[2])))
    elif o == 'xor':
        L.append('%s=%s^%s' % (R(a[0]), R(a[1]), R(a[2])))
    elif o == 'ori':
        L.append('%s=%s|%d' % (R(a[0]), R(a[1]), I(a[2])))
    elif o == 'oris':
        L.append('%s=%s|%d' % (R(a[0]), R(a[1]), I(a[2]) << 16))
    elif o == 'xori':
        L.append('%s=%s^%d' % (R(a[0]), R(a[1]), I(a[2])))
    elif o == 'andi':
        L.append('%s=%s&%d' % (R(a[0]), R(a[1]), I(a[2])))
    elif o == 'rlwinm':
        sh, mb, me = I(a[2]), I(a[3]), I(a[4])
        v = R(a[1])
        rot = v if sh == 0 else '((%s<<%d)|(%s>>%d))' % (v, sh, v, 32 - sh)
        L.append('%s=%s&%d' % (R(a[0]), rot, _mask(mb, me)))
    elif o == 'srawi':
        sh = I(a[2])
        L.append('x=%s;t=x>>%d;s.ca=1 if x<0 and (x&%d) else 0;%s=t&M' % (_sx(R(a[1])), sh, (1 << sh) - 1, R(a[0])))
    elif o in ('cmpw', 'cmpwi', 'cmplwi', 'cmplw'):
        f = 0
        if a[0].startswith('cr'):
            f, a = int(a[0][2:]), a[1:]
        y = R(a[1]) if a[1].startswith('r') else '%d' % (I(a[1]) & M)
        x = R(a[0])
        if o in ('cmpw', 'cmpwi'):
            x, y = _sx(x), _sx(y)
        L.append('x=%s;y=%s;c[%d]=4 if x<y else 2 if x>y else 1' % (x, y, f))
    elif o == 'mflr':
        L.append('%s=s.lr' % R(a[0]))
    elif o == 'mtlr':
        L.append('s.lr=%s' % R(a[0]))
    elif o == 'mtctr':
        L.append('s.ctr=%s' % R(a[0]))
    elif o == 'blr':
        end = 's.lr'
    elif o == 'bctrl':
        L.append('s.lr=%d' % nxt)
        end = 's.ctr'
    elif o == 'b':
        end = '%d' % I(a[0])
    elif o == 'bl':
        L.append('s.lr=%d' % nxt)
        end = '%d' % I(a[0])
    elif o == 'bclr':
        bo, bi = I(a[0]), I(a[1])
        if bo & 4 and not bo & 16:
            end = 's.lr if ((c[%d]>>%d)&1)==%d else %d' % (bi >> 2, 2 - (bi & 3), (bo >> 3) & 1, nxt)
        else:
            raise RuntimeError('bclr form at %08X' % addr)
    elif o == 'bc':
        bo, bi, tgt = I(a[0]), I(a[1]), I(a[2])
        tests = []
        if not bo & 4:  # counts down (bdnz and friends)
            L.append('s.ctr=(s.ctr-1)&M')
            tests.append('s.ctr==0' if bo & 2 else 's.ctr!=0')
        if not bo & 16:
            if bi & 3 == 3:
                raise RuntimeError('summary overflow branch at %08X' % addr)
            tests.append('((c[%d]>>%d)&1)==%d' % (bi >> 2, 2 - (bi & 3), (bo >> 3) & 1))
        end = '%d if %s else %d' % (tgt, ' and '.join(tests), nxt) if tests else '%d' % tgt
    else:
        raise RuntimeError('instruction not handled at %08X: %s %s' % (addr, op, a))
    if rec:
        L.append('x=%s;c[0]=4 if x<0 else 2 if x>0 else 1' % _sx(R(a[0])))
    return L, end


class Unpacker:
    def __init__(self, disasm_path):
        code = {}
        for line in open(disasm_path):
            a = int(line[:8], 16)
            if CODE_LO <= a < CODE_HI:
                p = line[10:].strip().replace(',', ' ').split()
                code[a] = (p[0], p[1:])
        starts = {UNPACK}
        for a, (op, args) in code.items():
            if op in ('b', 'bl'):
                starts.add(int(args[0], 0))
                starts.add(a + 4)
            elif op == 'bc':
                starts.add(int(args[2], 0))
                starts.add(a + 4)
            elif op in ('blr', 'bctrl', 'bclr'):
                starts.add(a + 4)
        src = ['def _make(s,mem,r,c,fb,M):', ' t={}']
        for b in sorted(x for x in starts if x in code):
            src.append(' def f():')
            src.append('  global_ = 0')
            a = b
            while a in code:
                op, args = code[a]
                try:
                    lines, end = _translate(op, args, a, a + 4)
                except RuntimeError as e:
                    # never reached in the unpacker's path; raises only if it is
                    lines, end = ['raise RuntimeError(%r)' % str(e)], '0'
                for l in lines:
                    for ll in l.split('\n'):
                        src.append('  ' + ll)
                a += 4
                if end is not None:
                    src.append('  return ' + end)
                    break
                if a in starts:
                    src.append('  return %d' % a)
                    break
            else:
                src.append('  raise RuntimeError("fell off the code")')
            src.append(' t[%d]=f' % b)
        src.append(' return t')
        self.mem = bytearray(0x400000)
        self.r = [0] * 32
        self.c = [0] * 8
        self.lr = self.ctr = self.ca = 0
        env = {}
        exec('\n'.join(src), env)
        self.blocks = env['_make'](self, self.mem, self.r, self.c, int.from_bytes, M)

    def unpack(self, blob):
        if blob[:8] != b'*SK_ASC*':
            return blob
        mem, r = self.mem, self.r
        pos = [0]
        out = bytearray()

        def ret():
            return self.lr

        def memset():
            a, v, n = r[3], r[4] & 255, r[5]
            mem[a:a + n] = bytes([v]) * n
            return self.lr

        def read():
            n = r[4]
            mem[r[3]:r[3] + n] = blob[pos[0]:pos[0] + n].ljust(n, b'\0')
            pos[0] += n
            return self.lr

        def write():
            out.extend(mem[r[3]:r[3] + r[4]])
            return self.lr

        def memcmp():
            # Its only call here checks the file's magic against the string in the game's data.
            r[3] = 0 if bytes(mem[r[4]:r[4] + r[5]]) == b'*SK_ASC*' else 1
            return self.lr

        blocks = self.blocks
        blocks[MEMCMP] = memcmp
        blocks[MEMSET] = memset
        blocks[READ_CB] = read
        blocks[WRITE_CB] = write
        r[1] = STACK
        r[3], r[4], r[5], r[6], r[7] = len(blob), READ_CB, WRITE_CB, 0, WORK
        self.lr = STOP
        pc = UNPACK
        while pc != STOP:
            pc = blocks[pc]()
        return bytes(out)
