#!/bin/sh
# variant.sh NAME PATCH: an exe with PATCH (a unified diff against the working tree's runtime/host/*) applied, built as
# build-win/var-NAME.exe, without leaving the change in the tree. Builds take turns (a lock in build/variant.lock), so
# several agents can each make their own and run it at once:  JTG_EXE=build-win/var-NAME.exe JTG_SLOT=N sh tools/ed/jtg.sh ...
#   sh tools/ed/variant.sh p2door /path/to/p2door.diff
# PATCH "-" builds the tree as it is (a baseline exe of its own).
cd "$(dirname "$0")/../.." || exit 1
NAME=${1:?a name}; PATCH=${2:?a patch file or -}
LOCK=build/variant.lock; mkdir -p build
n=0
until mkdir "$LOCK" 2>/dev/null; do
    n=$((n+1)); [ $n -gt 900 ] && { echo "[variant] lock held for 15 min: $LOCK (remove it if no build is running)"; exit 1; }
    sleep 1
done
B=build/variant-backup; rm -rf "$B"; mkdir -p "$B"; cp -r runtime/host "$B/"
# Always put the tree back, however this ends: piped into `grep -m8` or `head`, the closed pipe killed it before its
# restore once (2026-11-05) and left a broken patch in runtime/host for every other agent's build.
RESTORED=0
cleanup() {
    [ "$RESTORED" = 1 ] || restore >/dev/null 2>&1 || echo "[variant] COULD NOT PUT THE TREE BACK" >&2
    rm -rf "$LOCK"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM PIPE
# Put the tree back, checked: a copy refused (Permission denied while an editor or scan holds the file) once left a
# patch in the tree for the next agent's build. Retry until every file matches the backup.
restore() {
    for t in 1 2 3 4 5 6 7 8 9 10; do
        cp -r "$B/host/." runtime/host/ 2>/dev/null
        diff -rq "$B/host" runtime/host >/dev/null 2>&1 && { RESTORED=1; return 0; }
        sleep 2
    done
    echo "[variant] COULD NOT PUT THE TREE BACK: runtime/host differs from $B/host; fix it before any other build"; exit 2
}
if [ "$PATCH" != - ]; then
    if ! git apply --whitespace=nowarn "$PATCH"; then
        echo "[variant] the patch does not apply to the tree as it is now"; restore; exit 1
    fi
fi
OUT=$(cmd //c "$(cygpath -w tools/ed/build-win.bat)" 2>&1)
STATUS=$?
if [ $STATUS -ne 0 ] || printf '%s' "$OUT" | grep -q "FAILED:"; then
    printf '%s\n' "$OUT" | grep -E "error|Error" | head -30
    echo "[variant] build failed"; restore; cmd //c "$(cygpath -w tools/ed/build-win.bat)" >/dev/null 2>&1; exit 1
fi
cp -f build-win/EternalDarkness.exe "build-win/var-$NAME.exe"
restore
# Put the tree's own exe back, so build-win/EternalDarkness.exe stays the tree's.
cmd //c "$(cygpath -w tools/ed/build-win.bat)" >/dev/null 2>&1
echo "[variant] built build-win/var-$NAME.exe"
