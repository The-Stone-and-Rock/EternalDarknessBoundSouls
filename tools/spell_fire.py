"""The spell beams as columns of fire (the user's reference: a column of flame rising from each rune, as wide as the
rune, round, in the alignment's colour).

The beam rising at each rune is drawn from 32 strips (textures/GED/shadows and transparency/tex1_16x64_*, list in
build/rune_glow/beams.txt), added to the scene and coloured by the game per alignment (they're grey). Each strip keeps
the original's rise and fade along its length (its brightness down the middle, row by row), so the game's own timing
stays; across it, it becomes a round column of flame: a hot core, flame tongues rising and breaking up toward the tip,
different in every strip so it flickers.

    python -I tools/spell_fire.py            # writes the 32 strips into textures-spellfx/
Writes to textures-spellfx/ (same path as in textures/), which the game uses only while Options > Graphics >
Remastered spells is on; textures/ keeps the originals and is never written.
"""
import math
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEXCONV = os.path.join(ROOT, 'build', 'texconv', 'texconv.exe')
PACK = os.path.join(ROOT, 'textures', 'GED', 'shadows and transparency')
FX = os.path.join(ROOT, 'textures-spellfx', 'GED', 'shadows and transparency')  # used only with Remastered spells on
LIST = os.path.join(ROOT, 'build', 'rune_glow', 'beams.txt')
W, H = 64, 256

# Value noise on a lattice, smooth, tiling across x.
import random
_rng = random.Random(1906)
LAT = [[_rng.random() for _ in range(64)] for _ in range(256)]


def vnoise(x, y):
    xi, yi = int(math.floor(x)), int(math.floor(y))
    fx, fy = x - xi, y - yi
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    def L(i, j):
        return LAT[j % 256][i % 64]
    a = L(xi, yi) + (L(xi + 1, yi) - L(xi, yi)) * fx
    b = L(xi, yi + 1) + (L(xi + 1, yi + 1) - L(xi, yi + 1)) * fx
    return a + (b - a) * fy


def fbm(x, y):
    return 0.55 * vnoise(x, y) + 0.3 * vnoise(x * 2.03 + 17.1, y * 2.01) + 0.15 * vnoise(x * 4.1 + 3.7, y * 3.9 + 9.2)


def read_rgba(path, tmp):
    subprocess.run([TEXCONV, '-nologo', '-y', '-m', '1', '-f', 'R8G8B8A8_UNORM', '-o', tmp, path], capture_output=True)
    d = open(os.path.join(tmp, os.path.basename(path)), 'rb').read()
    off = 148 if d[84:88] == b'DX10' else 128
    return d, off


def fire(envelope, t):
    """One strip: envelope[y] the original's brightness (0-1) along it, row 0 its base; t its own time."""
    px = bytearray(W * H * 4)
    for y in range(H):
        e = envelope[y]
        if e <= 0.004:
            continue
        rise = y / float(H)                       # 0 at the base, 1 at the tip
        for x in range(W):
            u = (x + 0.5) / W * 2.0 - 1.0         # -1..1 across
            r = abs(u)
            body = math.sqrt(max(0.0, 1.0 - (r / 0.96) ** 2))          # a round column, its whole width
            if body <= 0.0:
                continue
            # Flame tongues: noise rising up the strip, the column narrowing and breaking up toward its tip.
            n = fbm(x / 7.0 + t * 3.1, y / 9.0 - t * 5.3)      # rounder: no vertical streaks
            wisp = fbm(x / 4.5 - t * 2.0 + 40.0, y / 5.5 - t * 7.9)
            edge = r + (n - 0.5) * 0.9 + rise * 0.35
            tongues = max(0.0, min(1.0, (1.0 - edge) * 2.2))
            heat = tongues * (0.65 + 0.7 * wisp) * body
            core = math.exp(-(u / 0.28) ** 2) * (1.0 - rise * 0.6)       # white-hot middle
            v = e * min(1.0, 0.85 * heat + 0.35 * core * tongues)   # no brighter than the original: the game's tint stays red
            c = int(max(0.0, min(255.0, 255.0 * v)))
            i = (y * W + x) * 4
            px[i:i + 4] = bytes((c, c, c, c))
    return px


def main():
    names = [l.strip() for l in open(LIST, encoding='utf-8') if l.strip()]
    tmp = tempfile.mkdtemp(prefix='spellfire-')
    raw, out = os.path.join(tmp, 'raw'), os.path.join(tmp, 'bc7')
    os.makedirs(raw)
    os.makedirs(out)
    os.makedirs(FX, exist_ok=True)
    done = 0
    for k, name in enumerate(names):
        path = os.path.join(PACK, name)
        d, off = read_rgba(path, raw)  # the original
        # The original's brightness along it: the middle third's average, row by row.
        env = []
        for y in range(H):
            s = 0
            for x in range(W // 3, 2 * W // 3):
                s += d[off + (y * W + x) * 4]
            env.append(s / float(W // 3) / 255.0)
        top = max(env) or 1.0
        env = [min(1.0, v / top * top) for v in env]   # its own shape and strength
        px = fire(env, k * 0.173)
        dst = os.path.join(raw, name)
        with open(dst, 'wb') as f:
            f.write(d[:off])
            f.write(px)
        subprocess.run([TEXCONV, '-nologo', '-y', '-m', '0', '-f', 'BC7_UNORM', '-o', out, dst], capture_output=True)
        made = os.path.join(out, name)
        if os.path.exists(made):
            shutil.copy2(made, os.path.join(FX, name))
            done += 1
        if k == 0:
            subprocess.run([TEXCONV, '-nologo', '-y', '-ft', 'png', '-f', 'R8G8B8A8_UNORM', '-m', '1', '-o',
                            os.path.join(ROOT, 'build', 'rune_glow'), dst], capture_output=True)
    shutil.rmtree(tmp, ignore_errors=True)
    print(done, 'fire strips into textures-spellfx/; a preview in build/rune_glow/')


if __name__ == '__main__':
    main()
