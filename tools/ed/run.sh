#!/bin/sh
# run.sh SAVE NAME: the game exactly as the user plays it (their settings.ini), with every log, the both-player
# recording (both.tsv) and pictures (shots/), under logs/run-*-NAME/. `ed check` reads it when it's closed.
#   sh tools/ed/run.sh saves/slot1.state urn
# SAVE as `launcher`: from the launcher, for Jump to Game and what loads in, with no save to load.
#   sh tools/ed/run.sh launcher kariem
# ED_RUN_PROBE=1 measures each player's view of each moment drawn, for `ed flicker RUN`.
# ED_RUN_SHOTS_W=N takes the pictures N wide (960), to look at the graphics closely. ED_RUN_FRAMES=N closes the game at guest frame N. Test presses come through the environment as usual
# (ED_INPUT_SCRIPT, ED_PAD2_SCRIPT); the user's own controllers work the same.
cd "$(dirname "$0")/../.." || exit 1
STATE=${1:?a save state}; NAME=${2:-run}
R="logs/run-$(date +%Y%m%d-%H%M%S)-$NAME"; mkdir -p "$R/shots"; echo "$R"
powershell -NoProfile -ExecutionPolicy Bypass -File tools/ed/shots.ps1 -dir "$R/shots" -everyMs "${ED_RUN_SHOTS_MS:-250}" -width "${ED_RUN_SHOTS_W:-960}" >/dev/null 2>&1 &
if [ -n "$ED_RUN_PROBE" ]; then export AURORA_SHADOW_PROBE="$R/probe.txt"; fi
if [ "$STATE" = launcher ]; then SKIP=; else SKIP=--no-launcher; export ED_QUICKLOAD="$STATE"; fi
ED_BOTH="$R/both.tsv" ED_DETAIL_LOG=1 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_DTK_LOG=1 ED_COOP_CAST_LOG=1 ED_COOP_PAD_LOG=1 ED_COOP_TRACE=1 \
    build-win/EternalDarkness.exe generated/main.dol --iso dump/GEDP01.iso $SKIP \
    --max-blocks 1000000000000000 2>&1 | python -u -I tools/coop-test/pagelog.py "$R" 20000 &
GAME=$!
if [ -n "$ED_RUN_FRAMES" ]; then
    python -I tools/ed/wait_frame.py "$R/both.tsv" "$ED_RUN_FRAMES" "${ED_RUN_SECONDS:-300}"
    taskkill //F //IM EternalDarkness.exe >/dev/null 2>&1
fi
wait $GAME 2>/dev/null
wait 2>/dev/null
python -I tools/ed/ed.py check "$R"
