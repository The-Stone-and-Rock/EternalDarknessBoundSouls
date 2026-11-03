#!/bin/sh
# launcher.sh [NAME]: the game from its launcher with two Tomes on, recorded under logs/ with every log on, for tracing a chapter from Jump to Game.
#   sh tools/coop-test/launcher.sh paul-solo
cd "$(dirname "$0")/../.." || exit 1
NAME=${1:-launcher}
R="logs/run-$(date +%Y%m%d-%H%M%S)-$NAME"; mkdir -p "$R"; echo "$R"
ED_COOP_TOME_COPY=2 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_COOP_CAST_LOG=1 ED_COOP_PAD_LOG=1 ED_COOP_TRACE=1 ED_ANIM_TRACE=1 \
    build-win/EternalDarkness.exe generated/main.dol --iso dump/GEDP01.iso \
    --max-blocks 1000000000000000 2>&1 | python -u -I tools/coop-test/pagelog.py "$R" 20000
