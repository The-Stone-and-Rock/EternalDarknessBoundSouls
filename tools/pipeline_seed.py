"""The shipped pipeline list (res/initial_pipeline_cache.db): made from a player's own list, and checked.

  python -I tools/pipeline_seed.py make [SRC_DB] [OUT_DB]
      SRC_DB: a pipeline_cache.db (default: %APPDATA%/Eternal Darkness/pipeline_cache.db). It is copied to a temp
      folder first (with its -wal), and only the copy is opened, so the player's own file is never touched.
      OUT_DB: default res/initial_pipeline_cache.db. Only rows at the renderer's current versions go in.
  python -I tools/pipeline_seed.py check [DB] [--commit COMMIT]
      Fails (exit 1) when a row is not at the current version of its type, or there are no GXCore rows.
      With --commit, the versions are those of an exe built from COMMIT (its GXRuntime pin and its patch), not the
      working tree's .deps: tools/win/make-win-package.sh runs it so on the seed it packs.

At each start the renderer merges the seed into the player's list and then deletes every row older than the current
versions (.deps/GXRuntime/graphics/aurora/lib/gfx/pipeline_cache.cpp, seed_pipeline_cache and
prune_old_pipeline_cache_versions). A seed at old versions does nothing at all, so each version bump needs a new seed.
The versions are read from the renderer's source, never typed here.
The list keeps the rows of both anti-aliasing settings (1x and 4x MSAA). At start the renderer builds only the rows
of its own sample count (and the 1x rows of its offscreen passes); the others stay in the player's list unbuilt.
"""
import os
import re
import shutil
import sqlite3
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIB = os.path.join(ROOT, ".deps", "GXRuntime", "graphics", "aurora", "lib")
SCHEMA = 1  # pipeline_cache.cpp PipelineCacheSchema
# ShaderType (gfx/common.hpp) and where each type's version is.
TYPES = {
    0: ("Clear", "gfx/clear.hpp", "ClearPipelineConfigVersion"),
    1: ("GX", "gx/pipeline.hpp", "GXPipelineConfigVersion"),
    2: ("Rml", "rmlui/pipeline.hpp", "RmlPipelineConfigVersion"),
    3: ("GXCore", "gfx/gxcore_draw.hpp", "GXCorePipelineConfigVersion"),
}


def git(*args):
    r = subprocess.run(["git", *args], capture_output=True, text=True, encoding="utf-8")
    return r.stdout if r.returncode == 0 else None


def versions(commit=None):
    """The renderer's versions: from the working tree's .deps/GXRuntime, or (commit) as the exe built from that
    commit has them: the GXRuntime pin in that commit's tools/setup-deps.sh, with that commit's patch on top
    (patches/gxruntime/eternal-darkness.patch), whatever state the working tree's .deps is in."""
    deps = os.path.join(ROOT, ".deps", "GXRuntime")
    patch = pin = None
    if commit is not None:
        setup = git("-C", ROOT, "show", f"{commit}:tools/setup-deps.sh")
        m = re.search(r"^pin GXRuntime \S+ ([0-9a-f]+)", setup or "", re.M)
        if not m:
            sys.exit(f"pipeline_seed: no GXRuntime pin in {commit}:tools/setup-deps.sh")
        pin = m.group(1)
        patch = git("-C", ROOT, "show", f"{commit}:patches/gxruntime/eternal-darkness.patch") or ""
    out = {}
    for t, (name, header, const) in TYPES.items():
        rx = r"constexpr\s+uint32_t\s+" + const + r"\s*=\s*(\d+)\s*;"
        value = None
        if commit is not None:
            # The patch's own line for the constant wins (the last added one in that header's part of the patch).
            part = re.search(r"^diff --git a/graphics/aurora/lib/" + re.escape(header) + r" .*?(?=^diff --git |\Z)",
                             patch, re.M | re.S)
            added = re.findall(r"^\+\s*" + rx, part.group(0), re.M) if part else []
            if added:
                value = int(added[-1])
            else:
                text = git("-C", deps, "show", f"{pin}:graphics/aurora/lib/{header}")
                if text is None:
                    sys.exit(f"pipeline_seed: can't read {header} at GXRuntime {pin}: is .deps/GXRuntime there "
                             f"(sh tools/setup-deps.sh) with that commit fetched?")
                m = re.search(rx, text)
                value = int(m.group(1)) if m else None
            where = f"GXRuntime {pin} with {commit}'s patch"
        else:
            path = os.path.join(LIB, header)
            if not os.path.exists(path):
                sys.exit(f"pipeline_seed: {path} is missing: fetch .deps (sh tools/setup-deps.sh) first")
            with open(path, encoding="utf-8") as f:
                m = re.search(rx, f.read())
            value = int(m.group(1)) if m else None
            where = ".deps/GXRuntime as it is now"
        if value is None:
            sys.exit(f"pipeline_seed: {const} not found in {header} ({where})")
        out[t] = value
    return out


def counts(db):
    return db.execute("SELECT type, config_version, COUNT(*) FROM pipeline_cache GROUP BY 1, 2 ORDER BY 1, 2").fetchall()


def make(src, out):
    want = versions()
    tmp = tempfile.mkdtemp(prefix="pipeline_seed_")
    try:
        copy = os.path.join(tmp, "pipeline_cache.db")
        shutil.copyfile(src, copy)
        for tail in ("-wal",):  # (the -shm is rebuilt from the wal)
            if os.path.exists(src + tail):
                shutil.copyfile(src + tail, copy + tail)
        s = sqlite3.connect(copy)
        if s.execute("SELECT value FROM aurora_schema").fetchone() != (SCHEMA,):
            sys.exit(f"pipeline_seed: {src} is not pipeline cache schema {SCHEMA}")
        print("source rows (type, version, count):", counts(s))
        new = out + ".new"
        if os.path.exists(new):
            os.remove(new)
        d = sqlite3.connect(new)
        # The renderer's own table and index (pipeline_cache.cpp), so the seed is read as a player's list is.
        d.executescript(
            "CREATE TABLE aurora_schema(value INTEGER);"
            f"INSERT INTO aurora_schema VALUES ({SCHEMA});"
            "CREATE TABLE pipeline_cache (type INTEGER NOT NULL, hash INTEGER NOT NULL, config_version INTEGER NOT NULL,"
            " config_size INTEGER NOT NULL, config BLOB NOT NULL, first_frame_used INTEGER NOT NULL,"
            " PRIMARY KEY (type, hash));"
            "CREATE INDEX pipeline_cache_load_order_idx ON pipeline_cache(type, config_version, first_frame_used);")
        rows = 0
        for t, v in want.items():
            for row in s.execute("SELECT type, hash, config_version, config_size, config, first_frame_used FROM "
                                 "pipeline_cache WHERE type = ? AND config_version = ? AND config_size = length(config)",
                                 (t, v)):
                d.execute("INSERT OR IGNORE INTO pipeline_cache VALUES (?, ?, ?, ?, ?, ?)", row)
                rows += 1
        d.commit()
        d.execute("VACUUM")
        d.close()
        s.close()
        os.replace(new, out)
        print(f"wrote {out}: {rows} rows at", {TYPES[t][0]: v for t, v in want.items()})
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return check(out)


def check(path, commit=None):
    want = versions(commit)
    db = sqlite3.connect(f"file:{path}?mode=ro&immutable=1", uri=True)
    bad = [(t, v, n) for t, v, n in counts(db) if want.get(t) != v]
    gxcore = sum(n for t, v, n in counts(db) if t == 3 and v == want[3])
    db.close()
    if bad or gxcore == 0:
        for t, v, n in bad:
            name = TYPES.get(t, (f"type {t}",))[0]
            print(f"pipeline_seed: {n} {name} rows at version {v}, the renderer wants {want.get(t)}", file=sys.stderr)
        if gxcore == 0:
            print(f"pipeline_seed: no GXCore rows at version {want[3]}", file=sys.stderr)
        print(f"pipeline_seed: {path} is stale: make it again (python -I tools/pipeline_seed.py make)", file=sys.stderr)
        return 1
    print(f"pipeline_seed: {path} is current ({gxcore} GXCore rows at version {want[3]})")
    return 0


def main(argv):
    if len(argv) < 2 or argv[1] not in ("make", "check"):
        print(__doc__)
        return 2
    seed = os.path.join(ROOT, "res", "initial_pipeline_cache.db")
    if argv[1] == "check":
        args = argv[2:]
        commit = None
        if "--commit" in args:
            i = args.index("--commit")
            if i + 1 >= len(args):
                print(__doc__)
                return 2
            commit = args[i + 1]
            del args[i:i + 2]
        return check(args[0] if args else seed, commit)
    src = argv[2] if len(argv) > 2 else os.path.join(os.environ.get("APPDATA", ""), "Eternal Darkness",
                                                     "pipeline_cache.db")
    return make(src, argv[3] if len(argv) > 3 else seed)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
