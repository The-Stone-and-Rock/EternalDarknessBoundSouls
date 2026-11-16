#!/usr/bin/env python3
"""Which game addresses the host hooks, and with what: a map from game address to host function, read from
runtime/host/hle.c without running it. Run from the repo root: python -I tools/ed/facts/hooks.py
Reads only: runtime/host/hle.c, generated/game_data.h, generated/sdk_symbols.inc.
Writes only: build/ed-db/facts/hooks.json and prints counts and anything it couldn't resolve.

A hook is an entry of the dispatch table (hle_dispatch_entry(address)): an "intercept" replaces the game function, a
"notify" runs beside it. They are installed in these ways (docs/game/research/kit-groundwork.md, the hook-map section):
  - the name tables kHandlers, kAddrHandlers and kNotify (by SDK symbol name);
  - X->intercept / X->notify = fn, X from hle_dispatch_entry(ARG) earlier in the same function, ARG a literal, a
    #define, an SDK symbol, or a row of a local table (T[i].field, T[i]);
  - hle_dispatch_entry(ARG)->intercept = fn inline;
  - a few through a local variable or a code scan at install time: LOCAL below, keyed by host function, so a moved
    line still matches and a new one is reported, not guessed.
What this can't know: whether an install's checks pass on a given run (prologue words, ED_ switches, the PAL disc),
and which run-time lr filters inside a handler narrow it (coop_interface_ran's kRan list and the like). Each row
carries the installing function, its #if stack and the ED_ switches that function reads, as a hint."""
import collections
import json
import os
import re

DB = 'build/ed-db'
SRC = 'runtime/host/hle.c'

# Installs whose address the parser can't evaluate: a local variable or a code scan. (host, kind): [(address, note)].
# From the reader's hand check of each site, 2026-11-16. A site missing here is reported as unresolved.
LOCAL = {
    ('hle_xfb_reader_notify', 'notify'): [(0x801E9920, 'PAL; USA 0x801F50EC')],
    ('notify_book_load', 'notify'): [(0x8002089C, 'bl target of 0x80116CE8+16, PAL only')],
    ('hle_book_menu_open', 'intercept'): [(0x80111078, 'PAL only')],
    ('hle_book_menu_frame', 'intercept'): [(0x80111234, 'PAL only')],
    ('notify_options_menu', 'notify'): [(0x80116470, 'PAL only')],
    ('hle_camera_fade', 'intercept'): [(0x8011CA04, 'PAL; USA 0x801237F0')],
    ('hle_draw_characters', 'intercept'): [(0x8002790C, 'code scan (find_show_player), replicated on the dol')],
    ('notify_character_bones', 'notify'): [(0x8011D394, 'code scan (find_skinning)')],
    ('hle_skin', 'intercept'): [(0x8011D5BC, 'code scan (find_skinning)')],
    ('notify_bone_matrix', 'notify'): [(0x80120E2C, 'code scan (find_bone_matrix)')],
    ('notify_perspective', 'notify'): [('C_MTXPerspective', 'SDK symbol')],
    ('notify_look_at', 'notify'): [('C_MTXLookAt', 'SDK symbol')],
    ('fog_texmtx_load', 'notify'): [('GXLoadTexMtxImm', 'SDK symbol')],
    ('coop_interface_player', 'intercept'): [(0x801F62B0, 'COOP_PLAYER_NODE_FN'), (0x801F62B8, 'COOP_PLAYER_ID_FN')],
    ('tw_retrace_wait', 'intercept'): [('VIWaitForRetrace', 'SDK symbol')],
    ('notify_item_let_go', 'notify'): [(0x80150FA0, 'i == 0'), (0x800C9DD4, 'i == 1')],
}
# install_msl_memory_hle's scan of 0x80003000..0x80005000 for the MSL memory functions, replicated on the dol.
SCAN = {'hle_msl_memcpy': 0x800031EC, 'hle_msl_memset': 0x80003100, 'hle_msl_copy_forward': 0x8000326C}


def strip(line):
    return re.sub(r'//.*', '', re.sub(r'"(\\.|[^"\\])*"', '""', line))


def main():
    src = open(SRC, encoding='utf8', errors='replace').read().split('\n')
    text = '\n'.join(src)

    # The #if stack at each line, and the #defines (later ones win, as the compiler sees the last before use; the few
    # redefined names are reported).
    defs, redefined, stack, pp_at = {}, collections.defaultdict(list), [], []
    for i, l in enumerate(src, 1):
        s = l.strip()
        if re.match(r'#\s*(if|ifdef|ifndef)\b', s):
            stack.append(s)
        elif re.match(r'#\s*(else|elif)\b', s) and stack:
            stack[-1] = stack[-1] + ' [%s]' % s
        elif re.match(r'#\s*endif\b', s) and stack:
            stack.pop()
        pp_at.append(' && '.join(stack))
        m = re.match(r'\s*#define\s+([A-Za-z_]\w*)\s+(.+?)\s*(//.*)?$', l)
        if m:
            redefined[m.group(1)].append(i)
            defs[m.group(1)] = m.group(2)
    for line in open('generated/game_data.h', encoding='utf8', errors='replace'):
        m = re.match(r'\s*#define\s+(\w+)\s+(\S+)', line)
        if m:
            defs.setdefault(m.group(1), m.group(2))
    syms = {}
    for line in open('generated/sdk_symbols.inc', encoding='utf8', errors='replace'):
        m = re.match(r'\s*\{\s*0x([0-9A-Fa-f]+)u,\s*"([^"]+)"', line)
        if m:
            syms.setdefault(m.group(2), int(m.group(1), 16))

    def ev(expr, depth=0):
        """A C constant: hex or decimal, #defines, sdk_symbol_address("x"), + - * and parentheses. None if not one."""
        if depth > 8:
            return None
        e = re.sub(r'sdk_symbol_address\("([^"]+)"\)', lambda m: hex(syms[m.group(1)]) if m.group(1) in syms else 'UNK', expr.strip())

        def name(m):
            n = m.group(0)
            if n in defs:
                v = ev(defs[n], depth + 1)
                return hex(v) if v is not None else 'UNK'
            return n
        e = re.sub(r'\b(?!0x)[A-Za-z_]\w*\b', name, e)
        e = re.sub(r'(0x[0-9A-Fa-f]+|\d+)[uU]\b', r'\1', e)
        if not re.fullmatch(r'[0-9A-Fa-fx\s+\-()*]+', e):
            return None
        try:
            return eval(e, {'__builtins__': {}}) & 0xFFFFFFFF
        except Exception:
            return None

    # The function each line is in (top-level braces).
    fn_at, cur, depth = [], None, 0
    for l in src:
        if depth == 0:
            m = re.match(r'^(?:static\s+)?(?:inline\s+)?[\w\s\*]+?\b(\w+)\s*\([^;]*\)\s*\{', l)
            if m:
                cur = m.group(1)
        fn_at.append(cur)
        code = strip(l)
        depth += code.count('{') - code.count('}')
        if depth == 0 and '}' in code:
            cur = None
    switches = collections.defaultdict(set)
    for i, l in enumerate(src):
        for m in re.finditer(r'getenv\("(ED_\w+)"\)', l):
            if fn_at[i]:
                switches[fn_at[i]].add(m.group(1))

    rows, unresolved, not_on_disc, handled = [], [], [], set()
    # The name tables' installer skips some names unless a switch is on (strcmp(kX[i].name, "...") in its loop).
    init = [i for i, f in enumerate(fn_at) if f == 'initialize_hle_dispatch']
    init_text = '\n'.join(src[i] for i in init)
    gated_names = set(re.findall(r'strcmp\(k\w+\[i\]\.name,\s*"(\w+)"\)', init_text))
    gate_switch = sorted(set(re.findall(r'getenv\("(ED_\w+)"\)', init_text)))
    DEBUG_GUARD = re.compile(r'getenv\("ED_\w*(LOG|TRACE|PROBE|WATCH|DUMP)\w*"\)|\b(coop_trace|fin_trace|change_time|r6_log|hurt_log)\(')

    def guard_text(line):
        """The install line and up to 4 lines before it in the same function: where its if-condition is."""
        lines = [line - 1]
        for k in range(line - 2, max(-1, line - 6), -1):
            if fn_at[k] != fn_at[line - 1]:
                break
            lines.insert(0, k)
        return ' '.join(src[k].split('//')[0] for k in lines)

    def add(addr, host, kind, line, how, when=None):
        f = fn_at[line - 1]
        g = guard_text(line)
        rows.append(dict(address='0x%08X' % addr, host=host, kind=kind, line=line, installed_by=f, how=how,
                         pp=pp_at[line - 1], switches=sorted(switches.get(f, ())),
                         needs_empty=sorted(set(re.findall(r'(intercept|notify)\s*==\s*NULL', g))),
                         debug=bool(DEBUG_GUARD.search(g)) or bool(re.search(r'trace|probe|_log\b|watch', host)),
                         **({'when': when} if when else {})))
        handled.add(line)

    # 1. The three name tables.
    for tname, kind, by_name_first in (('kHandlers', 'intercept', True), ('kAddrHandlers', 'intercept', False), ('kNotify', 'notify', False)):
        start = next(i for i, l in enumerate(src) if re.search(r'\b%s\[\]\s*=\s*\{' % tname, l))
        for j in range(start + 1, len(src)):
            if src[j].strip().startswith('};'):
                break
            if by_name_first:
                m = re.match(r'\s*\{\s*"([^"]+)",\s*(\w+)\s*\}', src[j])
                sym, fn, lit = (m.group(1), m.group(2), None) if m else (None, None, None)
            else:
                m = re.match(r'\s*\{\s*(0u|0x[0-9A-Fa-f]+u?),\s*(\w+),\s*"([^"]+)"\s*\}', src[j])
                sym, fn, lit = (m.group(3), m.group(2), m.group(1)) if m else (None, None, None)
            if not m:
                continue
            addr = ev(lit) if lit and lit != '0u' else syms.get(sym)
            if addr:
                add(addr, fn, kind, j + 1, '%s, by name %s' % (tname, sym))
                rows[-1]['installed_by'] = 'initialize_hle_dispatch'
                rows[-1]['switches'] = gate_switch if sym in gated_names else []
                if sym in gated_names:
                    rows[-1]['when'] = 'only with %s=1 (off by default)' % ' '.join(gate_switch)
            else:  # the name table installs nothing for a symbol this disc doesn't have
                not_on_disc.append(dict(line=j + 1, host=fn, kind=kind, symbol=sym, table=tname))

    # A local table T's rows as {field: expr}, from the last `T[...] = {` before a line; fields from its struct.
    def table_rows(tname, line):
        best = None
        for m in re.finditer(r'\b%s\s*(?:\[[^\]]*\]\s*)+=\s*\{' % re.escape(tname), text):
            if text.count('\n', 0, m.start()) + 1 <= line:
                best = m
        if not best:
            return None
        head = re.sub(r'//[^\n]*', '', text[max(0, best.start() - 2000):best.start()])
        sm = re.search(r'struct\s*\{([^{}]*)\}\s*$', head)
        fields = []
        if sm:
            for decl in sm.group(1).split(';'):
                decl = decl.strip()
                if not decl:
                    continue
                fp = re.search(r'\(\s*\*\s*(\w+)\s*\)', decl)  # a function pointer: void (*fn)(CPUState*)
                if fp:
                    fields.append(fp.group(1))
                else:
                    fields += [re.sub(r'.*?(\w+)\s*$', r'\1', part.strip()) for part in decl.split(',')]
        i, d = best.end() - 1, 0
        for j in range(i, len(text)):
            if text[j] == '{':
                d += 1
            elif text[j] == '}':
                d -= 1
                if d == 0:
                    body = re.sub(r'//[^\n]*', '', text[i + 1:j])
                    break
        items, cur_item, d = [], '', 0
        for ch in body:
            if ch in '({[':
                d += 1
            if ch in ')}]':
                d -= 1
            if ch == ',' and d == 0:
                items.append(cur_item.strip())
                cur_item = ''
            else:
                cur_item += ch
        if cur_item.strip():
            items.append(cur_item.strip())
        out = []
        for it in items:
            if it.startswith('{'):
                parts, cur_p, d = [], '', 0
                for ch in it[1:-1]:
                    if ch in '({[':
                        d += 1
                    if ch in ')}]':
                        d -= 1
                    if ch == ',' and d == 0:
                        parts.append(cur_p.strip())
                        cur_p = ''
                    else:
                        cur_p += ch
                parts.append(cur_p.strip())
                out.append(dict(zip(fields, parts)) if fields and len(fields) >= len(parts) else {'_%d' % k: p for k, p in enumerate(parts)})
            else:
                out.append({'_': it})
        return out

    def add_hosts(addr, host, kind, line, how):
        """A host written `cond ? a : b` is a row per non-NULL branch, each with its condition."""
        m = re.fullmatch(r'(.+?)\s*\?\s*(\w+)\s*:\s*(\w+)', host.strip())
        if not m:
            add(addr, host, kind, line, how)
            return
        for h, when in ((m.group(2), 'when %s' % m.group(1).strip()), (m.group(3), 'when not %s' % m.group(1).strip())):
            if h != 'NULL':
                add(addr, h, kind, line, how, when)

    # 2. Every other assignment.
    asg = re.compile(r'(\w+)->(intercept|notify)\s*=\s*([^=;][^;]*);')
    inline = re.compile(r'hle_dispatch_entry\((.+?)\)->(intercept|notify)\s*=\s*([^;]+);')
    local_lines = collections.defaultdict(list)
    for i, l in enumerate(src, 1):
        code = l.split('//')[0]  # strings kept: sdk_symbol_address("OSSendMessage") is an address
        found = [(m.group(1), m.group(2), m.group(3).strip(), True) for m in inline.finditer(code)]
        if not found:
            found = [(m.group(1), m.group(2), m.group(3).strip(), False) for m in asg.finditer(code)]
        for var, kind, host, is_inline in found:
            if re.search(r'\bk(Handlers|AddrHandlers|Notify)\[i\]', host):
                handled.add(i)  # the name tables' own loop, read above
                continue
            arg = var if is_inline else None
            if arg is None:
                decl = re.compile(r'\b%s\s*=\s*(?:[^;]*\?\s*)?hle_dispatch_entry\((.*?)\)\s*(?::\s*NULL\s*)?;' % re.escape(var))
                for k in range(i - 1, max(0, i - 160), -1):
                    if fn_at[k - 1] != fn_at[i - 1]:
                        break
                    dm = decl.search(src[k - 1].split('//')[0])
                    if dm:
                        arg = dm.group(1)
                        break
            local = LOCAL.get((host, kind))
            if local:
                local_lines[(host, kind)].append(i)
                for a, note in local:
                    a = syms.get(a) if isinstance(a, str) else a
                    if a:
                        add(a, host, kind, i, 'local site: %s' % note)
                continue
            if host in SCAN:
                add(SCAN[host], host, kind, i, 'code scan of 0x80003000..0x80005000, replicated on the dol')
                continue
            if arg is None:
                unresolved.append(dict(line=i, host=host, kind=kind, why='no hle_dispatch_entry(...) found for %s' % var))
                continue
            v = ev(arg)
            if v is not None:
                add_hosts(v, host, kind, i, 'hle_dispatch_entry(%s)' % arg.strip())
                continue
            tm = re.match(r'(\w+)\[i\](?:\.(\w+)|\[0\])?$', arg.strip())
            if tm and tm.group(1) == 'found':  # install_msl_memory_hle's scan results
                for h, a in SCAN.items():
                    add(a, h, kind, i, 'code scan of 0x80003000..0x80005000, replicated on the dol')
                continue
            if tm:
                trows = table_rows(tm.group(1), i)
                if trows is None:
                    unresolved.append(dict(line=i, host=host, kind=kind, why='table %s not found' % tm.group(1)))
                    continue
                hm = re.match(r'(\w+)\[i\](?:\.(\w+))?$', host)
                for k, row in enumerate(trows):
                    addr_expr = row.get(tm.group(2)) if tm.group(2) else (row.get('_') or next(iter(row.values())))
                    if hm and hm.group(1) == tm.group(1):
                        h = row.get(hm.group(2)) if hm.group(2) else None
                    elif hm:  # a parallel array of handlers
                        other = table_rows(hm.group(1), i) or []
                        h = other[k].get('_') if k < len(other) else None
                    else:
                        h = host
                    if 'notify' in row and row['notify'] in ('true', 'false') and (row['notify'] == 'true') != (kind == 'notify'):
                        continue  # the row's own flag picks intercept or notify
                    a = ev(addr_expr) if addr_expr else None
                    if a is None or not h:
                        unresolved.append(dict(line=i, host=h or host, kind=kind, why='table %s row %d: %s' % (tm.group(1), k, addr_expr)))
                        continue
                    add_hosts(a, h, kind, i, 'table %s row %d' % (tm.group(1), k))
                continue
            unresolved.append(dict(line=i, host=host, kind=kind, why='address %s not a constant' % arg.strip()))

    # Nothing silently dropped: every assignment line must have made a row or an unresolved entry, and each LOCAL
    # site must match exactly one line.
    seen_unresolved = {u['line'] for u in unresolved}
    for i, l in enumerate(src, 1):
        if re.search(r'->\s*(intercept|notify)\s*=(?!=)', l.split('//')[0]) and i not in handled and i not in seen_unresolved:
            unresolved.append(dict(line=i, host=l.strip()[:80], kind='?', why='an install line the parser did not read'))
    for key in LOCAL:
        n = len(local_lines.get(key, []))
        if n != 1:
            unresolved.append(dict(line=(local_lines.get(key) or [0])[0], host=key[0], kind=key[1],
                                   why='LOCAL site matched %d install lines, not 1: check its address by hand' % n))

    # Two installs on one slot (an address's intercept, or its notify): one holds it on a run. An install that needs
    # the slot empty (== NULL in its if) loses to one that doesn't; two that both check: whichever runs first.
    by_slot = collections.defaultdict(list)
    for r in rows:
        by_slot[(r['address'], r['kind'])].append(r)
    for (addr, kind), group in by_slot.items():
        hosts = {r['host'] for r in group}
        if len(hosts) < 2:
            continue
        # A log or trace install (off unless its switch is set) doesn't take the slot from a real hook.
        unguarded = {r['host'] for r in group if kind not in r['needs_empty'] and not r['debug']}
        for r in group:
            others = sorted(hosts - {r['host']})
            if kind in r['needs_empty'] and unguarded - {r['host']}:
                r['slot'] = 'loses the slot to %s, which installs without checking it' % ' '.join(sorted(unguarded - {r['host']}))
            elif kind not in r['needs_empty'] and len(unguarded) > 1:
                r['slot'] = 'shares it with %s, neither checking: the later install wins' % ' '.join(others)
            else:
                r['slot'] = 'shares it with %s: whichever installs first holds it' % ' '.join(others)
    # A notify that needs the intercept empty, on an address whose intercept is taken by an install that doesn't check.
    for r in rows:
        other = 'intercept' if r['kind'] == 'notify' else 'notify'
        if other in r['needs_empty']:
            takers = sorted({x['host'] for x in by_slot.get((r['address'], other), [])
                             if r['kind'] not in x['needs_empty'] and not x['debug']})
            if takers:
                r.setdefault('slot', 'installs only while the %s is empty, and %s takes it' % (other, ' '.join(takers)))

    # Return-address filters: a handler that acts only for calls from one place (cpu->lr == X). The row is keyed by the
    # game function holding X, so `ed fn` on the caller can say a call it makes is handled.
    import sys
    sys.path.insert(0, 'tools/ed')
    import know  # noqa: E402
    code = know.Code()
    # Function starts: called (bl) or a script function, plus a prologue right after a function's end (a function only
    # reached through a pointer or the dispatch table has no bl caller).
    starts = set(code.starts)
    for k in range(1, len(code.ops)):
        prev, op = code.ops[k - 1], code.ops[k]
        if re.match(r'(blr|b |bctr|rfi|\.long)', prev) and re.match(r'(stwu\s+r1, -|mflr\s+r0)', op):
            starts.add(code.addrs[k])
    starts = sorted(starts)
    import bisect
    lr_rows, bad_lr = collections.defaultdict(list), []
    LR = re.compile(r'\blr\s*(==|!=)\s*((?:[A-Za-z_]\w*|0x[0-9A-Fa-f]+u?)(?:\s*[+-]\s*(?:0x[0-9A-Fa-f]+|\d+)u?)?)')
    for i, l in enumerate(src, 1):
        for m in LR.finditer(l.split('//')[0]):
            v = ev(m.group(2))
            if v is None or not 0x80003000 <= v < 0x80300000:
                continue
            k = bisect.bisect_right(starts, v) - 1
            caller = starts[k] if k >= 0 else None
            before = code.ops[code.index[v - 4]] if v - 4 in code.index else ''
            is_return = bool(re.match(r'(bl|bctrl|blrl|bla)\b', before))
            g = guard_text(i)
            row = dict(lr='0x%08X' % v, test=m.group(1), host=fn_at[i - 1], line=i, name=m.group(2),
                       debug=bool(DEBUG_GUARD.search(g)), after=before)
            if not is_return:
                row['not_a_return_address'] = True  # the instruction before it isn't a call: this test can never match
                bad_lr.append(row)
            lr_rows['0x%08X' % caller if caller is not None else '?'].append(row)

    # An intercept that passes every call through but one: `g_hle_passthrough = true;` then `if (cpu->lr != X ...) return`
    # (or `if (cpu->lr != X) { g_hle_passthrough = true; return; }`) at its top. It covers only the call returning to X.
    starts_at = {}
    for i, f in enumerate(fn_at):
        if f and f not in starts_at:
            starts_at[f] = i
    for r in rows:
        i = starts_at.get(r['host'])
        if i is None or r['kind'] != 'intercept':
            continue
        top = ' '.join(src[k].split('//')[0] for k in range(i, min(i + 7, len(src))))
        m = re.search(r'if \((cpu->lr != [^)]*)\)\s*(?:\{\s*g_hle_passthrough = true;\s*)?return;', top)
        if m and 'g_hle_passthrough = true' in top:
            vals = [ev(x) for x in re.findall(r'cpu->lr != ([A-Za-z_]\w*|0x[0-9A-Fa-f]+u?)', m.group(1))]
            if vals and all(v is not None for v in vals):
                r['when'] = 'only for the call%s returning to %s (passes every other call through)' % (
                    's' if len(vals) > 1 else '', ', '.join('0x%08X' % v for v in vals))

    by_addr = collections.defaultdict(list)
    for r in rows:
        by_addr[r['address']].append(r)
    out = dict(
        _how='tools/ed/facts/hooks.py: read from runtime/host/hle.c without running it. An install whose checks fail on a '
             'run (prologue words, ED_ switches, the disc) is still listed; see installed_by, pp and switches.',
        counts=dict(rows=len(rows), addresses=len(by_addr), intercept=sum(r['kind'] == 'intercept' for r in rows),
                    notify=sum(r['kind'] == 'notify' for r in rows), unresolved=len(unresolved)),
        hooks=dict(sorted(by_addr.items())), unresolved=unresolved, not_on_this_disc=not_on_disc,
        by_return_address=dict(sorted(lr_rows.items())), return_address_checks_that_cannot_match=bad_lr,
        redefined={k: v for k, v in redefined.items() if len(v) > 1})
    json.dump(out, open(os.path.join(DB, 'facts', 'hooks.json'), 'w', encoding='utf8'), indent=1)
    print('hooks: %(rows)d rows on %(addresses)d game addresses (%(intercept)d intercept, %(notify)d notify), '
          '%(unresolved)d unresolved' % out['counts'] + '; %d name-table rows name a symbol this disc lacks' % len(not_on_disc))
    for u in unresolved:
        print('  unresolved: hle.c:%(line)d %(kind)s %(host)s (%(why)s)' % u)
    for r in bad_lr:
        print('  CAN\'T MATCH: hle.c:%d %s tests lr %s %s, but the instruction before %s is "%s", not a call' % (
            r['line'], r['host'], r['test'], r['name'], r['lr'], r['after']))


if __name__ == '__main__':
    main()
