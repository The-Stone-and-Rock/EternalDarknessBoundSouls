"""The game's text: byte pair encoded records in UBootPak.bin (items, spells, prompts, ...) and URmTxtNN.cmp (rooms).

A pack is u32 count, u32 0x6B5, then {offset, size}. A text pack's groups are u32 count, u32 0xFB90, {offset, size}, and
each entry is Philip Gage's Byte Pair Encoding: a pair table, a big-endian u16 size, the data; several blocks in a row.
"""
import re
import struct


def bpe_expand(data, at, end):
    """Expand BPE blocks from data[at:end]; returns the bytes."""
    out = bytearray()
    while at < end:
        left = list(range(256))
        right = [0] * 256
        c = 0
        while c < 256:
            count = data[at]
            at += 1
            if count > 127:
                c += count - 127
                count = 0
            if c == 256:
                break
            for _ in range(count + 1):
                left[c] = data[at]
                at += 1
                if c != left[c]:
                    right[c] = data[at]
                    at += 1
                c += 1
        size = data[at] << 8 | data[at + 1]
        at += 2
        stop = at + size
        if stop > end:
            raise ValueError('block runs past its entry')
        stack = []
        while True:
            if stack:
                b = stack.pop()
            elif at < stop:
                b = data[at]
                at += 1
            else:
                break
            if b == left[b]:
                out.append(b)
            else:
                stack.append(right[b])
                stack.append(left[b])
                if len(stack) > 4096:  # a pair table that loops: this isn't byte pair data
                    raise ValueError('pair table loops')
    return bytes(out)


def pack_table(blob):
    count, magic = struct.unpack('>II', blob[:8])
    if magic != 0x6B5:
        raise ValueError('not a pack')
    return [struct.unpack('>II', blob[8 + i * 8:16 + i * 8]) for i in range(count)]


_CTRL = re.compile(r'\\a.')


def clean(s):
    return _CTRL.sub('', s).replace('~p', '<player>').replace('\n', ' ').strip()


def parse_entry(raw):
    """A text record: floats and sizes, then c\\0 GROUP\\0 ...\\0 LABEL\\0 ...\\0 TEXT."""
    m = re.search(rb'[cl]\0([A-Za-z0-9_ ]*)\0+', raw)  # c: one size of window, l: another
    if not m:
        return '', '', ''
    group = m.group(1).decode('latin1')
    parts = [p for p in re.split(rb'\0+', raw[m.end():]) if p]
    if not parts:
        return group, '', ''
    label = parts[0].decode('latin1')
    text = ' '.join(p.decode('latin1') for p in parts[1:])
    return group, label, clean(text)


def read_groups(blob):
    """Every {group, index} of a text pack: returns {(group, index): (group label, label, text)}."""
    out = {}
    table = pack_table(blob)
    groups = []
    base = 8 + len(table) * 8
    for g, (off, _) in enumerate(table):
        if off + 8 > len(blob):
            continue
        n, magic = struct.unpack('>II', blob[off:off + 8])
        if magic == 0xFB90:
            groups.append((g, off, n))
            base = max(base, off + 8 + n * 8)
    # Entries' offsets count from their group's own table, and each starts with a u32 before its pair table.
    for g, off, n in groups:
        for i in range(n):
            eo, es = struct.unpack('>II', blob[off + 8 + i * 8:off + 16 + i * 8])
            if not es:
                continue
            at = off + eo
            try:
                raw = bpe_expand(blob, at + 4, at + es)
            except (IndexError, ValueError):
                continue
            out[(g, i)] = parse_entry(raw)
    return out
