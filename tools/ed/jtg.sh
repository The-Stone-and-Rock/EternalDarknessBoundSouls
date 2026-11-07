#!/bin/sh
# jtg.sh CHAPTER NAME: co-op from a cold boot -> the game's title -> Jump to Game -> CHAPTER, with NO save state anywhere
# (no ED_QUICKLOAD, no saves/title.state: the launcher is passed over with ED_NO_LAUNCHER and the intro movies are
# tapped through by ED_SKIP_INTRO, so the game runs every frame from its own boot). Recorded under logs/run-*-jtg-NAME/
# as tools/ed/run.sh does (both.tsv, shots/, page-*.log), then `ed check`, and tools/ed/jtg_report.py prints the
# frame both players got control and both players' places over time.
#   sh tools/ed/jtg.sh 1 pius-walk
#   JTG_P2_SCRIPT="0,180,lup" JTG_FRAMES=400 sh tools/ed/jtg.sh 1 p2-walk
#   JTG_P1=7 JTG_P2=3 JTG_PUT="60,2,-100,700,0" sh tools/ed/jtg.sh 1 put-test
#
# CHAPTER is the Jump to Game grid, left to right, top to bottom (as the game shows it, which is also the order of
# settings' mod_p1_character / mod_p2_character): 1 Pius, 2 Ellia, 3 Anthony, 4 Karim, 5 Max, 6 Lindsey, 7 Paul,
# 8 Roberto, 9 Peter, 10 Edward, 11 Michael, 12 Alex End.
#
# Settings: a copy of settings.ini in the run folder (ED_SETTINGS), with mod_coop = 1 and the user's own characters,
# forms and options, unless JTG_P1=N / JTG_P2=N (1-12) choose the characters. The user's settings.ini is not touched.
#
# The presses that get there (player 1's pad, ED_INPUT_SCRIPT, absolute guest frames = timebase / 675000, 60 a second;
# the guest clock follows the wall clock, so loads shift things by a few frames from run to run):
#   ~455  the intro is over (ED_SKIP_INTRO stops tapping A: "[input] opening movie over (/UMemcardText.bin) at frame 452-462"
#         in 6 of 7 runs); the title menu fades in by ~530 with Start Game lit, and stays (no attract loop by 1500)
#   700, 740, 780  Down x3 (the menu WRAPS: 5 rows; 6 Downs land on Load Game, logs/run-20261105-194551-jtg-probe2)
#   840   A: the 12-chapter grid fades in by ~910 with Pius lit
#   940.. D-pad Down per grid row, then Right per column, 30 frames apart (none for chapter 1; chapter 7 checked: Paul lit)
#   1100  A on the chapter. Chapters 2-12 then ask "CHOOSE YOUR FATE" (A at 1200 takes the lit Chattur'gha), and all ask
#         "Do you want to set the Eternal Mode on?" with No lit (the card has a finished game): A on No at 1200 (Pius) or
#         1300 (the others). JTG_ETERNAL=0 leaves that press out, for a card where it is not asked.
#   then hle.c skip_card_inserted presses A through the memory card screen itself, and the chapter loads.
#   JTG_TITLE_AT=N moves the whole schedule (default 700; everything above is relative to it).
#
# MEASURED 2026-11-05 with this schedule, the user's settings (P1 mod_p1_character 4 -> model 9 wearing "Chandra's
# ghost's other body", P2 mod_p2_character 2 -> model 70 wearing "Ellia's other body"; health 100 / 75):
#   Chapter 1 (Pius, level 0): room 341 (0x155) loads at ~1260; its intro (script 478, IntroCam01/02 and a fly-over,
#   player 1 "close-up", busy 0x22) runs to ~1865; CONTROL (first frame in a room with busy/hold/cutscene/menu/fade 0,
#   script 349) at frame 1868 (logs/run-20261105-195157-jtg-pius-stuck) and 1875 (logs/run-20261105-195957-jtg-pius-dirs2);
#   1766 with the chapter's A 100 frames earlier (logs/run-20261105-194838-jtg-probe4). Room 341, on the dais:
#   player 1 at (-47, 832, 15), player 2 at (139, 833, 26) (z settles from 24). Player 1's half on Camera01, player 2's on
#   IntroCam02 (eye -726,-295,1143), which it then never leaves (see the report of these runs).
#   Chapter 7 (Paul): room 103, control at 1552 (logs/run-20261105-200236-jtg-grid-paul2), P1 (-47, 401, 0), P2 (139, 410, 0).
# JTG_BASE is the frame every relative time below counts from: default 1900 for chapter 1, 1600 for chapter 7, 2300 for
# the others (not measured). Leave ~30 frames after control. The report says "LATE" when control came after the base:
# the presses before it were then lost, so raise JTG_BASE and run again. For another chapter, run it once with no presses,
# read "control at frame F" (also printed live as "[jtg] both players in control at frame F"), and use JTG_BASE=F+30.
# So "player 2 pushes the stick up for 3 s, half a second after the base" in chapter 1 is JTG_P2_SCRIPT="30,210,lup"
# (absolute 1930-2110), and player 1 the same is JTG_P1_SCRIPT="30,210,0,0,127".
#
# Relative presses (frames counted from JTG_BASE; turned into absolute frames and passed on):
#   JTG_P1_SCRIPT="start,end,buttons[,stick_x,stick_y[,analog_a]];..."  -> ED_INPUT_SCRIPT (player 1, GameCube pad:
#       buttons 0x0100 A, 0x0200 B, 0x0400 X, 0x0800 Y, 0x0010 Z, 0x0020 R, 0x0040 L, 0x1000 Start, 0x0001 left,
#       0x0002 right, 0x0004 down, 0x0008 up; stick -127..127, +y is up). Buttons 0 with a stick is fine.
#   JTG_P2_SCRIPT="start,end,input;..."  -> ED_PAD2_SCRIPT (player 2: a b x y z start l r up down left right, the left
#       stick lup ldown lleft lright, their camera rup rdown rleft rright).
#   JTG_PUT="frame,who,x,y,z;..."  -> ED_PUT (who 1 or 2, put on the floor under x y in the room they are in).
#   JTG_FRAMES=N closes the game N frames after JTG_BASE (default 600). ED_RUN_FRAMES=N (absolute) wins if set.
# Absolute ED_INPUT_SCRIPT / ED_PAD2_SCRIPT / ED_PUT given as well are kept (added after the relative ones); any other
# env (ED_RUN_SCRIPT, ED_COOP_P2_PUT, ED_ANIM_TRACE, ED_CAM_LOG...) passes straight through to the game.
# Those are NOT shifted: ED_COOP_P2_PUT=frame,... is an absolute frame; ED_RUN_SCRIPT=DELAY:SCRIPT counts DELAY from the
# first pad read after boot (hle.c test_run_script), i.e. about the absolute frame, so give it a delay past control.
# Reading a run: both.tsv's p1sx/p1sy (and so `ed timeline`'s "PUSHES, STILL") do NOT show ED_INPUT_SCRIPT's stick:
# player 1 pushing a wall shows as "stands". Player 2's scripted stick is recorded. jtg-report.txt in the run has the
# places; `python -I tools/ed/ed.py timeline RUN`, `ed see FRAME RUN` and shots/ (shots.csv: wall ms -> both.tsv ms) the rest.
#
# A REAL CONTROLLER PLUGGED IN: player 1's scripted presses here go through ED_INPUT_SCRIPT, which is added to player 1's
# pad inside the game's own PADRead (hle.c input_script_apply), so it works with real controllers connected; what the
# real pad does is OR'd in, so nobody should touch it. Do NOT use ED_PAD_SCRIPT for player 1 here: that one is an SDL
# virtual pad, which joins after the real ones (as controller 2 with one plugged in, so it became PLAYER 2 in
# logs/run-20261104-151159-jump-title2; controller 3 with two). Player 2's ED_PAD2_SCRIPT is mixed into player 2's read
# (controls.cpp script_player2) whatever is plugged in; with a real second pad, its stick at rest does not cancel it.
#
# ED_RUN_FRAMES closes EVERY EternalDarkness.exe by name: run one at a time, never with the user's game open.
cd "$(dirname "$0")/../.." || exit 1
CH=${1:?chapter 1-12}; NAME=${2:-jtg}
case "$CH" in [1-9]|1[0-2]) ;; *) echo "chapter must be 1-12"; exit 1 ;; esac
# JTG_SLOT=N (1-9): run beside other slots. The exe is copied to build-win/EDrunN.exe (next to its DLLs) and only that
# name is closed (JTG_EXE=build-win/var-NAME.exe runs a tools/ed/variant.sh build instead), so several runs (and a rebuild of EternalDarkness.exe) can go at once. Without it: one at a time.
# JTG_SLOT=auto waits for a free one of slots 1..JTG_SLOTS (default 3: more slows each game below 60) and frees it after.
if [ "$JTG_SLOT" = auto ]; then
    mkdir -p build; n=0
    while :; do
        for k in $(seq 1 "${JTG_SLOTS:-3}"); do
            if mkdir "build/jtg-slot-$k.lock" 2>/dev/null; then JTG_SLOT=$k; break 2; fi
        done
        n=$((n+1)); [ $n -gt 3600 ] && { echo "[jtg] no free slot for an hour"; exit 1; }
        sleep 1
    done
    trap 'rm -rf "build/jtg-slot-$JTG_SLOT.lock"' EXIT
    echo "[jtg] slot $JTG_SLOT"
fi
if [ -n "$JTG_SLOT" ]; then
    EXE_NAME="EDrun$JTG_SLOT.exe"
    cp -f "${JTG_EXE:-build-win/EternalDarkness.exe}" "build-win/$EXE_NAME" || { echo "could not copy the exe for slot $JTG_SLOT"; exit 1; }
    export ED_NO_BOOT_STATE=1
else
    EXE_NAME=EternalDarkness.exe
fi
PROC=${EXE_NAME%.exe}
if tasklist 2>/dev/null | grep -qi "$EXE_NAME"; then
    echo "$EXE_NAME is already running: close it first (this run closes every copy by name)"; exit 1
fi

# The menu presses.
T=${JTG_TITLE_AT:-700}
MENU="$T,$((T+6)),0x0004;$((T+40)),$((T+46)),0x0004;$((T+80)),$((T+86)),0x0004;$((T+140)),$((T+146)),0x0100"
# JTG_SLOW=1: a slower grid for a loaded machine (several games at once, 6-40 fps). The stock 30-frame, 6-frame-long
# grid presses were lost there and the run landed in the wrong chapter (room 166/150/338/103) or a menu
# (2026-11-06, round 3); a first grid press at T+330, 60 frames apart, 14 frames long worked 6 of 6. It moves control
# about 300 frames later, so the default bases move with it.
if [ "${JTG_SLOW:-0}" = 1 ]; then GRID_AT=330; GAP=60; HOLD=14; PICK=660; QAT=780; QGAP=150; LATER=300
else GRID_AT=240; GAP=30; HOLD=6; PICK=400; QAT=500; QGAP=100; LATER=0; fi
ROW=$(( (CH - 1) / 4 )); COL=$(( (CH - 1) % 4 )); AT=$((T+GRID_AT)); i=0
while [ $i -lt $ROW ]; do MENU="$MENU;$AT,$((AT+HOLD)),0x0004"; AT=$((AT+GAP)); i=$((i+1)); done
i=0
while [ $i -lt $COL ]; do MENU="$MENU;$AT,$((AT+HOLD)),0x0002"; AT=$((AT+GAP)); i=$((i+1)); done
MENU="$MENU;$((T+PICK)),$((T+PICK+6)),0x0100"
# Chapters 2-12 first ask "CHOOSE YOUR FATE" (the alignment: Chattur'gha lit first), then the Eternal Mode question.
# Pius's chapter has no such question (Pius picks the artifact in the game).
Q=$((T+QAT))
if [ "$CH" != 1 ]; then MENU="$MENU;$Q,$((Q+6)),0x0100"; Q=$((Q+QGAP)); fi
[ "${JTG_ETERNAL:-1}" = 0 ] || MENU="$MENU;$Q,$((Q+6)),0x0100"

case "$CH" in 1) BASE=${JTG_BASE:-$((1900+LATER))} ;; 7) BASE=${JTG_BASE:-$((1600+LATER))} ;; *) BASE=${JTG_BASE:-$((2300+LATER))} ;; esac
# Relative lists -> absolute: the first field of each item gets BASE added (and the second too for presses).
shift_list() {  # $1 list, $2 how many leading fields are frames
    printf '%s' "$1" | tr ';' '\n' | awk -F, -v b="$BASE" -v n="$2" 'NF { for (i = 1; i <= n; i++) $i += b; OFS = ","; $1 = $1; print }' | paste -sd ';' -
}
P1=$( [ -n "$JTG_P1_SCRIPT" ] && shift_list "$JTG_P1_SCRIPT" 2 )
P2=$( [ -n "$JTG_P2_SCRIPT" ] && shift_list "$JTG_P2_SCRIPT" 2 )
PUT=$( [ -n "$JTG_PUT" ] && shift_list "$JTG_PUT" 1 )
IN="$MENU${P1:+;$P1}${ED_INPUT_SCRIPT:+;$ED_INPUT_SCRIPT}"
PAD2="$P2${ED_PAD2_SCRIPT:+${P2:+;}$ED_PAD2_SCRIPT}"
PUTS="$PUT${ED_PUT:+${PUT:+;}$ED_PUT}"
STOP=${ED_RUN_FRAMES:-$((BASE + ${JTG_FRAMES:-600}))}
unset ED_QUICKLOAD ED_TITLE_STATE ED_PAD_SCRIPT

# ED_SKIP_INTRO taps A until it sees the intro's files go by. About 1 run in 6 it misses them, a press of ours ends the
# taps instead, and the taps have already started a NEW game on the title's Start Game (logs/run-20261105-195336-jtg-pius-dirs).
# That attempt is closed, kept as *-introfail, and the game started again (JTG_TRIES, default 3).
TRY=1
while :; do
    R="logs/run-$(date +%Y%m%d-%H%M%S)-jtg-$NAME"; mkdir -p "$R/shots"; echo "$R"
    # Settings: the user's, with co-op on and the characters chosen.
    sed -e 's/^mod_coop *=.*/mod_coop = 1/' \
        ${JTG_P1:+-e "s/^mod_p1_character *=.*/mod_p1_character = $JTG_P1/"} \
        ${JTG_P2:+-e "s/^mod_p2_character *=.*/mod_p2_character = $JTG_P2/"} settings.ini > "$R/settings.ini"
    grep -q '^mod_coop' "$R/settings.ini" || echo 'mod_coop = 1' >> "$R/settings.ini"
    # JTG_SET="key=value;key=value": any other setting for this run (mod_p1_costume=0 for no form, mod_coop=0 for solo).
    if [ -n "$JTG_SET" ]; then
        printf '%s
' "$JTG_SET" | tr ';' '
' | while IFS='=' read -r k v; do
            [ -n "$k" ] || continue
            if grep -q "^$k *=" "$R/settings.ini"; then sed -i "s/^$k *=.*/$k = $v/" "$R/settings.ini"; else echo "$k = $v" >> "$R/settings.ini"; fi
        done
    fi
    {
        grep -E '^mod_(coop|p1_character|p2_character|p1_costume|p2_costume) ' "$R/settings.ini" | tr '\n' ' '; echo
        echo "chapter $CH, base frame $BASE, closes at frame $STOP, attempt $TRY"
        echo "ED_INPUT_SCRIPT=$IN"; echo "ED_PAD2_SCRIPT=$PAD2"; echo "ED_PUT=$PUTS"
    } | tee "$R/jtg.txt"
    powershell -NoProfile -ExecutionPolicy Bypass -File tools/ed/shots.ps1 -proc "$PROC" -dir "$R/shots" -everyMs "${ED_RUN_SHOTS_MS:-250}" -width "${ED_RUN_SHOTS_W:-960}" >/dev/null 2>&1 &
    ED_SETTINGS="$R/settings.ini" ED_NO_LAUNCHER=1 ED_SKIP_INTRO=1 ED_INPUT_SCRIPT="$IN" ED_PAD2_SCRIPT="$PAD2" ED_PUT="$PUTS" \
    ED_BOTH="$R/both.tsv" ED_DETAIL_LOG=1 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_COOP_CAST_LOG=1 ED_COOP_PAD_LOG=1 ED_COOP_TRACE=1 \
        "build-win/$EXE_NAME" generated/main.dol --iso dump/GEDP01.iso --no-launcher \
        --max-blocks 1000000000000000 2>&1 | python -u -I tools/coop-test/pagelog.py "$R" 20000 &
    GAME=$!
    if python -I tools/ed/jtg_report.py --intro "$R" 120; then break; fi
    taskkill //F //IM "$EXE_NAME" >/dev/null 2>&1
    wait $GAME 2>/dev/null; wait 2>/dev/null
    mv "$R" "$R-introfail"; echo "[jtg] the intro skip missed (kept as $R-introfail): starting again"
    TRY=$((TRY + 1))
    [ $TRY -le "${JTG_TRIES:-3}" ] || { echo "[jtg] gave up after ${JTG_TRIES:-3} tries"; exit 1; }
done
# Both players in control: said as soon as it happens, so a watcher knows where the base stands.
( F=$(python -I tools/ed/jtg_report.py --wait "$R" "${ED_RUN_SECONDS:-300}") && echo "[jtg] both players in control at frame $F (base $BASE)" ) &
python -I tools/ed/wait_frame.py "$R/both.tsv" "$STOP" "${ED_RUN_SECONDS:-300}"
taskkill //F //IM "$EXE_NAME" >/dev/null 2>&1
wait $GAME 2>/dev/null
wait 2>/dev/null
python -I tools/ed/ed.py check "$R"
python -I tools/ed/jtg_report.py "$R" "$BASE" "${JTG_EVERY:-30}" | tee "$R/jtg-report.txt"
echo "$R"
