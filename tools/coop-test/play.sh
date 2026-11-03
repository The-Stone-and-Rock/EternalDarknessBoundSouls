#!/bin/sh
# play.sh [STATE] [NAME]: the game on a quick state with every log on, recorded under logs/
# (page-NNNN.log, INDEX.txt, EVENTS.log). Each player's presses are logged with their frame, and [trace] lines show how their actions run.
#   sh tools/coop-test/play.sh saves/slot7.state tome-test
cd "$(dirname "$0")/../.." || exit 1
STATE=${1:-saves/slot7.state}; NAME=${2:-coop}
R="logs/run-$(date +%Y%m%d-%H%M%S)-$NAME"; mkdir -p "$R"; echo "$R"
ED_QUICKLOAD="$STATE" ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_COOP_CAST_LOG=1 ED_COOP_PAD_LOG=1 ED_COOP_TRACE=1 \
    build-win/EternalDarkness.exe generated/main.dol --iso dump/GEDP01.iso --no-launcher \
    --max-blocks 1000000000000000 2>&1 | python -u -I tools/coop-test/pagelog.py "$R" 20000
