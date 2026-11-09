"""Add mip chains to the HD pack's BC7 textures that have none, keeping each full-size image byte for byte.

    python -I tools/texpack_mips.py [--limit N] [--dry]

Why: 7402 of the pack's 8591 DDS files are BC7 with only their full-size level. Far away they are sampled at
full size, so they shimmer, cost bandwidth, and Texture filtering can't apply (gxcore_draw.cpp with_texture_mips
skips a texture with one level). See docs/graphics-review.md, item 9.

How: texconv (build/texconv/texconv.exe, Microsoft DirectXTex) makes the full chain; then the new file's level 0
is replaced by the original's, so up close nothing changes. Each original is copied first to
textures-backup-nomips/ (same relative path); to undo, copy that folder back over textures/.
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PACK = os.path.join(ROOT, "textures")
BACKUP = os.path.join(ROOT, "textures-backup-nomips")
TEXCONV = os.path.join(ROOT, "build", "texconv", "texconv.exe")
HEADER = 148  # "DDS " + 124-byte header + 20-byte DX10 header
FORMATS = {98: "BC7_UNORM", 99: "BC7_UNORM_SRGB"}


def info(path):
    """(width, height, mips, dxgi) for a single-level 2D BC7 DDS, else None."""
    with open(path, "rb") as f:
        h = f.read(HEADER)
    if len(h) < HEADER or h[:4] != b"DDS " or h[84:88] != b"DX10":
        return None
    flags, height, width = struct.unpack_from("<3I", h, 8)
    mips = struct.unpack_from("<I", h, 28)[0] if flags & 0x20000 else 1
    dxgi, dim, _misc, array = struct.unpack_from("<4I", h, 128)
    if dxgi not in FORMATS or dim != 3 or array != 1 or max(mips, 1) > 1 or min(width, height) < 8:
        return None
    return width, height, dxgi


def level0_bytes(w, h):
    return max(1, (w + 3) // 4) * max(1, (h + 3) // 4) * 16


def main():
    limit = int(sys.argv[sys.argv.index("--limit") + 1]) if "--limit" in sys.argv else None
    dry = "--dry" in sys.argv
    todo = {}
    for d, _, files in os.walk(PACK):
        for name in files:
            if name.lower().endswith(".dds"):
                p = os.path.join(d, name)
                i = info(p)
                if i:
                    todo.setdefault((d, i[2]), []).append((p, i))
    total = sum(len(v) for v in todo.values())
    print(f"{total} textures without mips", flush=True)
    if dry:
        return
    done = failed = 0
    tmp = tempfile.mkdtemp(prefix="texmips-")
    try:
        for (d, dxgi), items in todo.items():
            if limit is not None and done >= limit:
                break
            if limit is not None:
                items = items[: limit - done]
            for start in range(0, len(items), 200):
                batch = items[start : start + 200]
                out = os.path.join(tmp, "out")
                shutil.rmtree(out, ignore_errors=True)
                os.makedirs(out)
                flist = os.path.join(tmp, "list.txt")
                with open(flist, "w", encoding="utf-8") as f:
                    f.write("\n".join(p for p, _ in batch))
                r = subprocess.run([TEXCONV, "-nologo", "-y", "-m", "0", "-f", FORMATS[dxgi], "-o", out, "-flist", flist],
                                   capture_output=True, text=True)
                for p, (w, h, _) in batch:
                    made = os.path.join(out, os.path.basename(p))
                    try:
                        with open(p, "rb") as f:
                            orig = f.read()
                        with open(made, "rb") as f:
                            new = bytearray(f.read())
                        n0 = level0_bytes(w, h)
                        ok = (new[:4] == b"DDS " and struct.unpack_from("<2I", new, 12) == (h, w)
                              and struct.unpack_from("<I", new, 128)[0] == dxgi
                              and struct.unpack_from("<I", new, 28)[0] > 1
                              and len(orig) >= HEADER + n0 and len(new) > HEADER + n0)
                        if not ok:
                            raise ValueError("unexpected texconv output")
                        new[HEADER : HEADER + n0] = orig[HEADER : HEADER + n0]  # the full-size image, as it was
                        keep = os.path.join(BACKUP, os.path.relpath(p, PACK))
                        os.makedirs(os.path.dirname(keep), exist_ok=True)
                        if not os.path.exists(keep):
                            shutil.copy2(p, keep)
                        with open(p + ".tmp", "wb") as f:
                            f.write(new)
                        os.replace(p + ".tmp", p)
                        done += 1
                    except Exception as e:  # noqa: BLE001 - report and go on; the original is untouched
                        failed += 1
                        print(f"skip {os.path.relpath(p, PACK)}: {e} {r.stderr.strip()[:200]}", flush=True)
                print(f"{done}/{total} done, {failed} skipped", flush=True)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
