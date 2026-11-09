#!/bin/sh
# item_sweep.sh [SAVE] [FIRST] [LAST] [BATCH]: every item type FIRST..LAST (0..243) spawned for both players
# (ED_ITEM_SPAWN, hle.c item_test_spawn) and picked up, BATCH types a run (30), each run from SAVE (saves/slot1.state)
# so one find's script can't spoil the rest. Player 1 presses B by ED_INPUT_SCRIPT, player 2 by ED_PAD2_SCRIPT: a type
# every 300 frames, player 1's at its start, player 2's 150 after. Then tools/ed/item_sweep_report.py reads the runs.
#   sh tools/ed/item_sweep.sh saves/slot1.state 0 243 30
# ED_RUN_FRAMES closes every EternalDarkness.exe: don't run it with the game open.
cd "$(dirname "$0")/../.." || exit 1
SAVE=${1:-saves/slot1.state}; FIRST=${2:-0}; LAST=${3:-243}; BATCH=${4:-30}
START=${SWEEP_START:-2150}; EVERY=300
STAMP=$(date +%Y%m%d-%H%M%S); LIST="logs/item-sweep-$STAMP.txt"; : > "$LIST"
from=$FIRST
while [ "$from" -le "$LAST" ]; do
    to=$((from + BATCH - 1)); [ "$to" -gt "$LAST" ] && to=$LAST
    P1=""; P2=""; i=0
    while [ $((from + i)) -le "$to" ]; do
        s=$((START + i * EVERY)); t=$((s + 150))
        P1="$P1$((s+30)),$((s+38)),0x0200;$((s+90)),$((s+98)),0x0200;$((s+130)),$((s+138)),0x0200;"
        P2="$P2$((t+30)),$((t+38)),b;$((t+90)),$((t+98)),b;$((t+130)),$((t+138)),b;"
        i=$((i + 1))
    done
    end=$((START + i * EVERY + 30))
    R=$(ED_ITEM_SPAWN="$START,$from,$to,$EVERY,3" ED_INPUT_SCRIPT="$P1" ED_PAD2_SCRIPT="$P2" ED_RUN_FRAMES=$end \
        ED_RUN_SECONDS=$((i * 6 + 90)) ED_RUN_SHOTS_MS=500 sh tools/ed/run.sh "$SAVE" "items-$from-$to" 2>&1 | head -1)
    echo "$from $to $START $EVERY $R" >> "$LIST"
    echo "[sweep] types $from-$to: $R"
    from=$((to + 1))
done
python -I tools/ed/item_sweep_report.py "$LIST"
