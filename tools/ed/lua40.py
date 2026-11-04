"""Lua 4.0 bytecode, as the game's script packs (ScrLvlNN.gpk) hold it: parse and decompile to readable text.

The output is Lua with `goto`s where control flow joins: it is for reading, not for compiling.
Instruction layout is standard 4.0: op 6 bits, then U 26 bits, or B 9 bits and A 17 bits.
"""
import struct

MAXARG_S = (1 << 25) - 1
MULT_RET = 255
OPS = ['END', 'RETURN', 'CALL', 'TAILCALL', 'PUSHNIL', 'POP', 'PUSHINT', 'PUSHSTRING', 'PUSHNUM', 'PUSHNEGNUM',
       'PUSHUPVALUE', 'GETLOCAL', 'GETGLOBAL', 'GETTABLE', 'GETDOTTED', 'GETINDEXED', 'PUSHSELF', 'CREATETABLE',
       'SETLOCAL', 'SETGLOBAL', 'SETTABLE', 'SETLIST', 'SETMAP', 'ADD', 'ADDI', 'SUB', 'MULT', 'DIV', 'POW', 'CONCAT',
       'MINUS', 'NOT', 'JMPNE', 'JMPEQ', 'JMPLT', 'JMPLE', 'JMPGT', 'JMPGE', 'JMPT', 'JMPF', 'JMPONT', 'JMPONF', 'JMP',
       'PUSHNILJMP', 'FORPREP', 'FORLOOP', 'LFORPREP', 'LFORLOOP', 'CLOSURE']
CMP = {'JMPNE': '~=', 'JMPEQ': '==', 'JMPLT': '<', 'JMPLE': '<=', 'JMPGT': '>', 'JMPGE': '>='}
ARITH = {'ADD': '+', 'SUB': '-', 'MULT': '*', 'DIV': '/', 'POW': '^'}


class Fn:
    __slots__ = ('params', 'vararg', 'maxstack', 'kstr', 'knum', 'kproto', 'code', 'locals')


class Reader:
    def __init__(self, data, at=0):
        self.d, self.p = data, at

    def int(self):
        v = struct.unpack_from('<I', self.d, self.p)[0]
        self.p += 4
        return v

    def byte(self):
        v = self.d[self.p]
        self.p += 1
        return v

    def string(self):
        n = self.int()
        if n == 0:
            return None
        s = self.d[self.p:self.p + n - 1].decode('latin1')
        self.p += n
        return s

    def num(self):
        v = struct.unpack_from('<d', self.d, self.p)[0]
        self.p += 8
        return v


def parse(data):
    if data[:5] != b'\x1bLua\x40':
        raise ValueError('not a Lua 4.0 chunk')
    if data[5:13] != b'\x01\x04\x04\x04\x20\x06\x09\x08':
        raise ValueError('unexpected chunk layout %s' % data[5:13].hex())
    r = Reader(data, 21)  # the header, then an 8-byte test number
    return _function(r)


def _function(r):
    f = Fn()
    r.string()  # source
    r.int()  # line defined
    f.params = r.int()
    f.vararg = r.byte()
    f.maxstack = r.int()
    f.locals = []
    for _ in range(r.int()):
        f.locals.append((r.string(), r.int(), r.int()))
    for _ in range(r.int()):
        r.int()  # line info
    f.kstr = [r.string() for _ in range(r.int())]
    f.knum = [r.num() for _ in range(r.int())]
    f.kproto = [_function(r) for _ in range(r.int())]
    f.code = [r.int() for _ in range(r.int())]
    return f


def fields(w):
    op = w & 63
    u = w >> 6
    return OPS[op] if op < len(OPS) else 'OP%d' % op, u, u - MAXARG_S, (w >> 6) & 0x1FF, w >> 15


def globals_called(f, out=None):
    """Every global read in a function and its nested functions, with how many times."""
    out = {} if out is None else out
    for w in f.code:
        name, u, _, _, _ = fields(w)
        if name == 'GETGLOBAL':
            out[f.kstr[u]] = out.get(f.kstr[u], 0) + 1
    for p in f.kproto:
        globals_called(p, out)
    return out


def _quote(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'


def _num(v):
    return str(int(v)) if v == int(v) and abs(v) < 1e15 else repr(v)


class _Item:
    __slots__ = ('text', 'pc', 'is_local')

    def __init__(self, text, pc):
        self.text, self.pc, self.is_local = text, pc, False


def _run(f, flagged, indent, namer, emit_on):
    """One pass over the code. The first pass (emit_on False) finds which stack slots are locals, as stripped
    chunks don't say; the second writes the text."""
    lines = []
    stack = [_Item('p%d' % i, -1) for i in range(f.params)]
    for it in stack:
        it.is_local = True
    pad = '  ' * indent
    targets = set()
    for pc, w in enumerate(f.code):
        name, u, s, b, a = fields(w)
        if name.startswith('JMP') or name in ('PUSHNILJMP', 'FORPREP', 'FORLOOP', 'LFORPREP', 'LFORLOOP'):
            targets.add(pc + 1 + s)
    used = set()

    def out(t):
        lines.append(pad + t)

    def pop():
        if not stack:
            return _Item('?', -1)
        return stack.pop()

    def text(it):
        return it.text

    def push(t, pc):
        slot = len(stack)
        it = _Item(t, pc)
        if pc in flagged:
            out('local L%d = %s' % (slot, t))
            it.text = 'L%d' % slot
            it.is_local = True
        stack.append(it)

    pc = 0
    n = len(f.code)
    while pc < n:
        w = f.code[pc]
        name, u, s, b, a = fields(w)
        if pc in targets:
            out('::L%d::' % pc)
        if name == 'END':
            pass
        elif name == 'RETURN':
            vals = [text(i) for i in stack[u:]]
            for i in stack[u:]:
                used.add(i.pc)
            out('return ' + ', '.join(vals))
            del stack[u:]
        elif name in ('CALL', 'TAILCALL'):
            args = stack[a + 1:]
            fn = stack[a] if a < len(stack) else _Item('?', -1)
            call = '%s(%s)' % (fn.text, ', '.join(i.text for i in args))
            del stack[a:]
            if name == 'TAILCALL':
                out('return ' + call)
            elif b == 0:
                out(call)
            elif b == MULT_RET:
                push(call, pc)
            else:
                push(call, pc)
                for _ in range(b - 1):
                    push('nil --[[extra result of %s]]' % fn.text, -pc - 1)
        elif name == 'PUSHNIL':
            for k in range(u):
                push('nil', -pc - 100 - k if k else pc)
        elif name == 'POP':
            for _ in range(u):
                it = pop()
                if not it.is_local:
                    used.add(it.pc)
                    if '(' in it.text:
                        out(it.text)
        elif name == 'PUSHINT':
            push(str(s), pc)
        elif name == 'PUSHSTRING':
            push(_quote(f.kstr[u]), pc)
        elif name == 'PUSHNUM':
            push(_num(f.knum[u]), pc)
        elif name == 'PUSHNEGNUM':
            push('-' + _num(f.knum[u]), pc)
        elif name == 'PUSHUPVALUE':
            push('upvalue%d' % u, pc)
        elif name == 'GETLOCAL':
            if u < len(stack):
                used.add(stack[u].pc)
                push(stack[u].text if stack[u].is_local else 'L%d' % u, pc)
            else:
                push('L%d' % u, pc)
        elif name == 'GETGLOBAL':
            push(namer(f.kstr[u]), pc)
        elif name == 'GETTABLE':
            k, t = pop(), pop()
            push('%s[%s]' % (t.text, k.text), pc)
        elif name == 'GETDOTTED':
            t = pop()
            push('%s.%s' % (t.text, f.kstr[u]), pc)
        elif name == 'GETINDEXED':
            t = pop()
            key = stack[u].text if u < len(stack) else 'L%d' % u
            push('%s[%s]' % (t.text, key), pc)
        elif name == 'PUSHSELF':
            t = pop()
            push('%s.%s' % (t.text, f.kstr[u]), pc)
            push(t.text, pc)
        elif name == 'CREATETABLE':
            push('{}', pc)
        elif name == 'SETLOCAL':
            v = pop()
            if u < len(stack):
                used.add(stack[u].pc)
                out('%s = %s' % (stack[u].text if stack[u].is_local else 'L%d' % u, v.text))
            else:
                out('L%d = %s' % (u, v.text))
        elif name == 'SETGLOBAL':
            v = pop()
            out('%s = %s' % (f.kstr[u], v.text))
        elif name == 'SETTABLE':
            v = pop()
            at = len(stack) - a  # A: where the table is, from the top with the value gone
            if 0 <= at and at + 1 < len(stack) + 1 and at + 1 <= len(stack):
                t = stack[at - 1] if at >= 1 else _Item('?', -1)
                k = stack[at] if at < len(stack) else _Item('?', -1)
                out('%s[%s] = %s' % (t.text, k.text, v.text))
            else:
                out('?[?] = %s' % v.text)
            for _ in range(max(0, b)):
                if stack and b > 0:
                    pop()
            # B counts the value too, which has gone
            if b > 0 and False:
                pass
        elif name == 'SETLIST':
            vals = [pop() for _ in range(b)][::-1]
            t = pop()
            push('{%s}' % ', '.join(i.text for i in vals), pc)
        elif name == 'SETMAP':
            pairs = []
            for _ in range(u):
                v, k = pop(), pop()
                pairs.append('[%s]=%s' % (k.text, v.text))
            pop()
            push('{%s}' % ', '.join(pairs[::-1]), pc)
        elif name in ARITH:
            y, x = pop(), pop()
            push('(%s %s %s)' % (x.text, ARITH[name], y.text), pc)
        elif name == 'ADDI':
            x = pop()
            push('(%s + %d)' % (x.text, s), pc)
        elif name == 'CONCAT':
            vals = [pop() for _ in range(u)][::-1]
            push('(%s)' % ' .. '.join(i.text for i in vals), pc)
        elif name == 'MINUS':
            push('(-%s)' % pop().text, pc)
        elif name == 'NOT':
            push('(not %s)' % pop().text, pc)
        elif name in CMP:
            y, x = pop(), pop()
            out('if %s %s %s then goto L%d end' % (x.text, CMP[name], y.text, pc + 1 + s))
        elif name == 'JMPT':
            out('if %s then goto L%d end' % (pop().text, pc + 1 + s))
        elif name == 'JMPF':
            out('if not %s then goto L%d end' % (pop().text, pc + 1 + s))
        elif name == 'JMPONT':
            x = pop()
            out('if %s then goto L%d end --[[keeps the value]]' % (x.text, pc + 1 + s))
        elif name == 'JMPONF':
            x = pop()
            out('if not %s then goto L%d end --[[keeps the value]]' % (x.text, pc + 1 + s))
        elif name == 'JMP':
            out('goto L%d' % (pc + 1 + s))
        elif name == 'PUSHNILJMP':
            push('nil', pc)
            pc += 1
        elif name == 'FORPREP':
            out('-- for loop, ends at L%d' % (pc + 1 + s))
        elif name == 'FORLOOP':
            out('-- next of for loop, back to L%d' % (pc + 1 + s))
        elif name == 'LFORPREP':
            out('-- for-in loop, ends at L%d' % (pc + 1 + s))
            push('k', pc)
            push('v', -pc - 1)
        elif name == 'LFORLOOP':
            out('-- next of for-in, back to L%d' % (pc + 1 + s))
        elif name == 'CLOSURE':
            for _ in range(b):
                pop()
            sub = decompile(f.kproto[a], indent + 1, namer)
            args = ', '.join('p%d' % i for i in range(f.kproto[a].params))
            push('function(%s)\n%s\n%send' % (args, sub, pad), pc)
        else:
            out('-- ' + name)
        pc += 1
    if pc in targets:
        out('::L%d::' % pc)
    return lines, used, stack


def decompile(f, indent=0, namer=lambda n: n):
    # Slots still on the stack at the end, or read by GETLOCAL, were locals.
    flagged = set()
    for _ in range(3):
        _, used, stack = _run(f, flagged, indent, namer, False)
        new = {p for p in used if p >= 0} | {i.pc for i in stack if i.pc >= 0}
        if new <= flagged:
            break
        flagged |= new
    lines, _, _ = _run(f, flagged, indent, namer, True)
    return '\n'.join(lines)


def pack_entries(data):
    """A script pack: u32 count, u32 0x6B5, then {offset, size} for each script id."""
    count, magic = struct.unpack('>II', data[:8])
    if magic != 0x6B5:
        raise ValueError('not a script pack')
    out = {}
    for i in range(count):
        off, size = struct.unpack('>II', data[8 + i * 8:16 + i * 8])
        if size:
            out[i] = data[off:off + size]
    return out
