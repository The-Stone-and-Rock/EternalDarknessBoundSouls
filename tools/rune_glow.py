"""Smooth glowing edges for the spell runes on the floor (the user: the original glyphs' edges look blocky and streaky).

Each floor glyph draws two pack textures: the rune (textures/GED/runes, 64x64 in the game) and a glow mask round it
(textures/GED/shadows and transparency, 64x64). The pack's masks are noisy and streaked. This rebuilds each rune's mask
as a smooth halo: the mask blurred (its shape kept, the streaks gone), with a soft wide bloom from the rune itself
added, so the edges glow and fade smoothly.

    python -I tools/rune_glow.py pair        # find each rune's mask by shape; writes build/rune_glow/pairs.txt + sheet
    python -I tools/rune_glow.py apply       # rebuild the paired masks into textures-spellfx/
Writes to textures-spellfx/ (same path as in textures/), which the game uses only while Options > Graphics >
Remastered spells is on; textures/ keeps the originals and is never written.

Needs build/texconv/texconv.exe (tools/texpack_mips.py has it). No numpy: images are worked at 128x128.
"""
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEXCONV = os.path.join(ROOT, 'build', 'texconv', 'texconv.exe')
RUNES = os.path.join(ROOT, 'textures', 'GED', 'runes')
MASKS = os.path.join(ROOT, 'textures', 'GED', 'shadows and transparency')
OUT = os.path.join(ROOT, 'build', 'rune_glow')
FX = os.path.join(ROOT, 'textures-spellfx')  # the remastered spells' textures (texpack.cpp, spell_fx)


def to_rgba(paths, size, outdir):
    """texconv the DDS files to uncompressed RGBA at size x size; returns {path: bytes (size*size*4)}."""
    os.makedirs(outdir, exist_ok=True)
    flist = os.path.join(outdir, 'list.txt')
    with open(flist, 'w', encoding='utf-8') as f:
        f.write('\n'.join(paths))
    subprocess.run([TEXCONV, '-nologo', '-y', '-m', '1', '-f', 'R8G8B8A8_UNORM', '-w', str(size), '-h', str(size), '-o', outdir,
                    '-flist', flist], capture_output=True)
    res = {}
    for p in paths:
        q = os.path.join(outdir, os.path.basename(p))
        try:
            d = open(q, 'rb').read()
        except OSError:
            continue
        off = 148 if d[84:88] == b'DX10' else 128
        res[p] = d[off:off + size * size * 4]
    return res


def lum(px, size):
    return [((px[i * 4] + px[i * 4 + 1] + px[i * 4 + 2]) / 3.0) * (px[i * 4 + 3] / 255.0) for i in range(size * size)]


def blur(v, size, radius):
    k = [math.exp(-(x * x) / (2.0 * (radius / 2.0) ** 2)) for x in range(-radius, radius + 1)]
    s = sum(k)
    k = [x / s for x in k]
    tmp = [0.0] * (size * size)
    for y in range(size):
        row = y * size
        for x in range(size):
            a = 0.0
            for j, w in enumerate(k):
                xx = min(size - 1, max(0, x + j - radius))
                a += v[row + xx] * w
            tmp[row + x] = a
    out = [0.0] * (size * size)
    for y in range(size):
        for x in range(size):
            a = 0.0
            for j, w in enumerate(k):
                yy = min(size - 1, max(0, y + j - radius))
                a += tmp[yy * size + x] * w
            out[y * size + x] = a
    return out


def corr(a, b):
    n = len(a)
    ma, mb = sum(a) / n, sum(b) / n
    sa = math.sqrt(sum((x - ma) ** 2 for x in a)) or 1.0
    sb = math.sqrt(sum((x - mb) ** 2 for x in b)) or 1.0
    return sum((x - ma) * (y - mb) for x, y in zip(a, b)) / (sa * sb)


def pair():
    runes = sorted(os.path.join(RUNES, f) for f in os.listdir(RUNES) if f.startswith('tex1_64x64_') and f.endswith('.dds'))
    masks = sorted(os.path.join(MASKS, f) for f in os.listdir(MASKS) if f.startswith('tex1_64x64_') and f.endswith('.dds'))
    tmp = tempfile.mkdtemp(prefix='runeglow-')
    S = 24
    rr = to_rgba(runes, S, os.path.join(tmp, 'r'))
    mm = to_rgba(masks, S, os.path.join(tmp, 'm'))
    mvec = {p: lum(px, S) for p, px in mm.items()}
    lines = []
    for r, px in rr.items():
        v = blur(lum(px, S), S, 2)
        if max(v) < 20:
            continue
        best = sorted(((corr(v, mv), m) for m, mv in mvec.items()), reverse=True)[:2]
        lines.append('%.3f %.3f\t%s\t%s' % (best[0][0], best[1][0], os.path.basename(r), os.path.basename(best[0][1])))
    os.makedirs(OUT, exist_ok=True)
    lines.sort(reverse=True)
    with open(os.path.join(OUT, 'pairs.txt'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    shutil.rmtree(tmp, ignore_errors=True)
    print('\n'.join(lines[:60]))
    print(len(lines), 'runes paired; build/rune_glow/pairs.txt')


def write_dds_rgba(path, px, size):
    hdr = bytearray(128)
    hdr[0:4] = b'DDS '
    struct.pack_into('<7I', hdr, 4, 124, 0x1007, size, size, size * 4, 0, 1)
    struct.pack_into('<2I', hdr, 76, 32, 0x41)          # pixel format: size, RGB | alpha
    struct.pack_into('<5I', hdr, 88, 32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000)
    struct.pack_into('<I', hdr, 108, 0x1000)
    with open(path, 'wb') as f:
        f.write(hdr)
        f.write(bytes(px))


def apply(min_score=0.6, install=True):
    pairs = [l.split('\t') for l in open(os.path.join(OUT, 'pairs.txt'), encoding='utf-8').read().splitlines() if l]
    pairs = [(float(p[0].split()[0]), p[1], p[2]) for p in pairs]
    chosen = {}
    for score, rune, mask in pairs:
        if score >= min_score and mask not in chosen:
            chosen[mask] = rune
    S = 128
    tmp = tempfile.mkdtemp(prefix='runeglow-')
    # From the original masks (kept in the backup once replaced), so a second run starts again from them.
    src = {m: os.path.join(MASKS, m) for m in chosen}  # the originals
    mpaths = list(src.values())
    rpaths = [os.path.join(RUNES, r) for r in chosen.values()]
    mm = to_rgba(mpaths, S, os.path.join(tmp, 'm'))
    rr = to_rgba(rpaths, S, os.path.join(tmp, 'r'))
    work = os.path.join(tmp, 'w')
    os.makedirs(work, exist_ok=True)
    done = []
    for mask, rune in chosen.items():
        mp, rp = os.path.join(MASKS, mask), os.path.join(RUNES, rune)
        if src[mask] not in mm or rp not in rr:
            continue
        m = mm[src[mask]]
        r = lum(rr[rp], S)
        ml = lum(m, S)
        soft = blur(ml, S, 7)            # the mask's own shape, its streaks gone
        bloom = blur(r, S, 22)           # a wide soft bloom from the rune itself: the aura
        top = max(max(soft), 1.0)
        topb = max(max(bloom), 1.0)
        # colour: the mask's average hue (masks are mostly white/grey: kept as they are)
        px = bytearray(S * S * 4)
        for i in range(S * S):
            a = 0.8 * (soft[i] / top) + 0.9 * (bloom[i] / topb)
            a = 1.0 - math.exp(-2.2 * a)                       # a soft shoulder: bright core, long smooth falloff
            v = int(max(0.0, min(255.0, 255.0 * a)))
            px[i * 4:i * 4 + 4] = bytes((v, v, v, v))
        raw = os.path.join(work, mask)
        write_dds_rgba(raw, px, S)
        done.append((mp, raw))
    out = os.path.join(tmp, 'bc7')
    os.makedirs(out, exist_ok=True)
    flist = os.path.join(tmp, 'w.txt')
    with open(flist, 'w', encoding='utf-8') as f:
        f.write('\n'.join(r for _, r in done))
    subprocess.run([TEXCONV, '-nologo', '-y', '-m', '0', '-f', 'BC7_UNORM', '-w', '256', '-h', '256', '-o', out, '-flist', flist],
                   capture_output=True)
    if not install:  # previews: build/rune_glow/*.png, the pack untouched
        os.makedirs(OUT, exist_ok=True)
        subprocess.run([TEXCONV, '-nologo', '-y', '-ft', 'png', '-f', 'R8G8B8A8_UNORM', '-m', '1', '-o', OUT, '-flist', flist], capture_output=True)
        shutil.rmtree(tmp, ignore_errors=True)
        print(len(done), 'previews in build/rune_glow/')
        return
    n = 0
    for mp, raw in done:
        made = os.path.join(out, os.path.basename(raw))
        if not os.path.exists(made):
            continue
        dst = os.path.join(FX, os.path.relpath(mp, os.path.join(ROOT, 'textures')))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(made, dst)
        n += 1
    shutil.rmtree(tmp, ignore_errors=True)
    print(n, 'glow masks rebuilt into textures-spellfx/')


if __name__ == '__main__':
    {"pair": pair, "apply": apply, "preview": lambda: apply(install=False)}[sys.argv[1]]()
