"""Feather the hard, cut-out edges of spell effect sprites (streaks, sparkles, glows, flipbook flames) in the HD pack.

The pack's spell sprites are upscales with hard alpha cut-outs and dark outlines, which read as blocky stickers (the
user's "weird blocky texture on the edges"). Each sprite here keeps its colour; its alpha is blurred a little (a soft
falloff instead of a cut) and pixels that are dark but opaque go see-through, so a glow has no black ring.

    python -I tools/vfx_soften.py FILELIST          # pack-relative paths (GED/...), one a line
    python -I tools/vfx_soften.py --beam FILELIST   # light shafts' strips: a soft core across
Writes to textures-spellfx/ (same path as in textures/), which the game uses only while Options > Graphics >
Remastered spells is on; textures/ keeps the originals and is never written.
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
PACK = os.path.join(ROOT, 'textures')
FX = os.path.join(ROOT, 'textures-spellfx')  # the remastered spells' textures (texpack.cpp, spell_fx)


def dims(path):
    h = open(path, 'rb').read(32)
    return struct.unpack_from('<I', h, 16)[0], struct.unpack_from('<I', h, 12)[0]  # w, h


def blur1(v, w, h, r, horizontal):
    if r <= 0:
        return v[:]
    k = [math.exp(-(x * x) / (2.0 * (r / 2.0) ** 2)) for x in range(-r, r + 1)]
    s = sum(k)
    k = [x / s for x in k]
    out = [0.0] * (w * h)
    for y in range(h):
        for x in range(w):
            a = 0.0
            for j, kk in enumerate(k):
                if horizontal:
                    xx = min(w - 1, max(0, x + j - r))
                    a += v[y * w + xx] * kk
                else:
                    yy = min(h - 1, max(0, y + j - r))
                    a += v[yy * w + x] * kk
            out[y * w + x] = a
    return out


def soften(px, w, h):
    n = w * h
    alpha = [px[i * 4 + 3] / 255.0 for i in range(n)]
    r = max(1, int(round(min(w, h) * 0.02)))
    a = blur1(blur1(alpha, w, h, r, True), w, h, r, False)
    out = bytearray(px)
    for i in range(n):
        lum = max(px[i * 4], px[i * 4 + 1], px[i * 4 + 2]) / 255.0
        dark = min(1.0, lum / 0.18)                      # near-black: see-through
        na = min(alpha[i], a[i]) * 0.35 + a[i] * 0.65    # the cut softened into a falloff
        out[i * 4 + 3] = int(max(0.0, min(255.0, 255.0 * na * dark)))
    return out


def beam(px, w, h):
    """A light shaft's strip: a bright core fading smoothly to nothing at both sides (the strips were even across, so
    the beam had hard straight edges), so it reads as light."""
    out = bytearray(px)
    prof = [math.exp(-((x + 0.5 - w / 2.0) / (w * 0.24)) ** 2) for x in range(w)]
    for y in range(h):
        for x in range(w):
            i = (y * w + x) * 4
            k = prof[x]
            # Drawn added (the colour is the light; alpha is 255 throughout): the colour itself fades to nothing at
            # the sides, the core a little brighter. Its sides were still 36 of 255 and cut off straight.
            for c in range(3):
                out[i + c] = min(255, int(px[i + c] * k * 1.25))
            out[i + 3] = int(px[i + 3] * k)
    return out


def main():
    mode = soften
    args = sys.argv[1:]
    if args and args[0] == '--beam':
        mode = beam
        args = args[1:]
    if args and args[0] == '--clear':  # testing: see-through, to find what a sprite draws
        mode = lambda px, w, h: bytes(b if i % 4 != 3 else 0 for i, b in enumerate(px))
        args = args[1:]
    rels = [l.strip() for l in open(args[0], encoding='utf-8') if l.strip()]
    tmp = tempfile.mkdtemp(prefix='vfxsoft-')
    done = 0
    for rel in rels:
        path = os.path.join(PACK, rel.replace('\\', os.sep))
        if not os.path.exists(path):
            print('missing', rel)
            continue
        src = path  # the original
        w, h = dims(src)
        raw_dir = os.path.join(tmp, 'raw')
        os.makedirs(raw_dir, exist_ok=True)
        subprocess.run([TEXCONV, '-nologo', '-y', '-m', '1', '-f', 'R8G8B8A8_UNORM', '-o', raw_dir, src], capture_output=True)
        raw = os.path.join(raw_dir, os.path.basename(src))
        d = open(raw, 'rb').read()
        off = 148 if d[84:88] == b'DX10' else 128
        px = mode(d[off:off + w * h * 4], w, h)
        with open(raw, 'wb') as f:
            f.write(d[:off])
            f.write(px)
        out_dir = os.path.join(tmp, 'bc7')
        os.makedirs(out_dir, exist_ok=True)
        subprocess.run([TEXCONV, '-nologo', '-y', '-m', '0', '-f', 'BC7_UNORM', '-o', out_dir, raw], capture_output=True)
        made = os.path.join(out_dir, os.path.basename(raw))
        if not os.path.exists(made):
            print('failed', rel)
            continue
        dst = os.path.join(FX, os.path.relpath(path, PACK))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(made, dst)
        done += 1
        print('softened', rel, '%dx%d' % (w, h))
    shutil.rmtree(tmp, ignore_errors=True)
    print(done, 'sprites softened into textures-spellfx/')


if __name__ == '__main__':
    main()
