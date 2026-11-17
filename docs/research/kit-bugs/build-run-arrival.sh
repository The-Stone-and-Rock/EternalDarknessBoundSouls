#!/bin/sh
# ed113 / ed248 in player 2's own room arrival (docs/research/kit-bugs/plans.md, section 2: the bug, the fix, the
# corrected test plan and the skeptic's notes; this script follows the corrected plan).
#
# THE BUG. With the players apart (ED_COOP_STAY=1, ED_ROOM_SLOTS=2), the arrival script queued by player 2's own room
# change (notify_script_later, inside the R4 bracket) runs a frame later in coop_run_script's first block (hle.c
# 20554-20577). That block puts player 2's room in (coop_room_in) but leaves player 1 as the player and the main
# character (no coop_be_player2). ed113 (0x80011150 -> 0x801D6364, rooms 90 and 202) then reads player 1's main node
# (r13-21484 through 0x801F6250), and ed248 (0x800DC46C) player 1's main id (r13-21488 through 0x801F6258).
# THE PLANNED FIX (plans.md section 2, "Fix plan (the skeptic's corrected version)"): in that block only, when player 2
# is listed, not the player already, has a valid object and stands in the script's room (coop_object_room == want),
# coop_be_player2(on) before coop_room_in and coop_be_player2(off) after coop_stay_room_out; its [stay] line gains
# ", as player N's (main node 0x........; frame N)".
#
# WHAT IS TESTED. Chapter 12 (Alex End, level 12), start room 100 (0x64), JTG_BASE=2255 (as docs/research/build-run-pages.sh).
# Player 2 is put into room 100's BoxRegion02 (min [1595,-4885,-94] max [1790,-3876,919]) at +30 (ARRIVAL_X, -4380: about
# 100 from both x edges and 500 from both y edges), with a short stick step after it; the box runs L12/635 (fn71(90),
# fn66(90, EntryMkr02, Camera02, 3, 0, 0)), a walk-in door with no prompt.
# Player 1 has no presses and stays in room 100. Room 90's (0x5A) arrival is L12/2477 (0x9AD), whose ed113(90) is
# unconditional. The trace reads the main node ed113 uses: 0x801F6300 (lwz r3,40(r3)) called from 0x801D63BC, so its
# chain line has "<- 0x801D63C0" and its r3 is the main node.
#
# NO STATE. Every run here is a COLD Jump to Game (tools/ed/jtg.sh) on the build as it is now: the game boots from
# nothing, so each run is its own fresh start on the current build and no state step is needed (none is made or loaded,
# so ED_SETTINGS is jtg.sh's own copy of settings.ini in each run folder). ED_RUN_AWAY=1 for every run. Slot 2
# (build-win/EDrun2.exe) unless ARRIVAL_SLOT=N. Never `ed test`, never `ed chapters`.
#
# Run from Git Bash: sh docs/research/kit-bugs/build-run-arrival.sh [STEP...]
#   STEP: probe a near flat solo; none given: probe a.
#   ARRIVAL_BUILD=before|after names the runs (default "now"): logs/run-*-jtg-arrival-STEP-BUILD.
# Order:
#   BEFORE the fix (the build as it is):  ARRIVAL_BUILD=before sh <this> probe a      (then "near" only if a says so)
#   build WITH the fix, then on that build: ARRIVAL_BUILD=after sh <this> probe a solo  (then "near" only if a says so)
#   ("flat" only if near still has no "<- 0x801D63C0" line.)
#
# The steps and what their output means:
#   probe  Co-op apart, no trace. jtg-report's control frame (LATE means raise ARRIVAL_BASE and run again), the put
#          ("[sweep] player 2 put at" or "not put: no floor under": then ARRIVAL_X=1620, still in the box), player 2's
#          change ("[stay] player 2's change to room 90 runs as theirs"), "[room] change 0x0000005A", 2477 queued
#          inside the bracket ("[room] later 0x000009AD ... calls: 0x80153B6C") and the first block's run ("[stay] room
#          90's own script 2477 (queued by player 2's arrival) ran with room 90 current (frame N)"). N is the arrival;
#          the script prints it as "ARRIVAL_AT=N" for near and flat.
#   a      The same run traced: ED_TRACE_FN=0x801D6364,0x801F6300 ED_TRACE_CHAIN=1 from BASE+PUT_AT+10 (after the put;
#          the getter is hot and chain mode prints at most 1024 distinct chains). Adds "[chain] 0x801D6364
#          r3=0x0000005A" (ed113(90) called) and "[chain] 0x801F6300 r3=0xNNNNNNNN ... <- 0x801D63C0" (the node ed113
#          read), and compares it with "[coop] player 2's first turn ran (player 1's node 0xAAAAAAAA, player 2's
#          0xBBBBBBBB)". The comparison counts only if the [stay] own-script line is there (the arrival ran in the first
#          block); the script says so when it is not, and prints both frames (the node line's and the [stay] line's).
#          BEFORE the fix: the node is player 1's (0xAAAAAAAA) and the [stay] line has no "as player": that is the bug.
#          AFTER the fix: the node is player 2's (0xBBBBBBBB), the [stay] line reads "... ran with room 90 current, as
#          player 2's (main node 0xBBBBBBBB; ...)", no "[stay] WARNING", no new saves/reports/hang-*.txt, and both.tsv's
#          last row (printed as "places") has p1room 100 and p2room 90. (jtg-report.txt has one room column only, the
#          game's current room, so it cannot show the two players' rooms.)
#          If the ed113 line shows but no "<- 0x801D63C0" line does, the chain cap filled first: run "near".
#   near   As a, the trace started 30 frames before the arrival: ARRIVAL_AT=N (the frame of the [stay] line from probe or
#          a; frames drift a few from run to run, ARRIVAL_EARLY=K starts it K before instead of 30), closed 120 after.
#   flat   The plan's fallback when even near's chain cap fills: the same two functions traced WITHOUT chain mode (every
#          call printed, "[fn] NAME lr=0x... r3=0x..."), from ARRIVAL_AT-ARRIVAL_EARLY and closed 30 after ARRIVAL_AT
#          (0x801F6300 is hot, so the window is kept short). The node is the r3 of the "lr=0x801D63C0" line. (No
#          ED_TRACE_R3: the node is r3 itself, no field of it is needed.)
#   solo   Co-op off (JTG_SET=mod_coop=0, no ED_COOP_STAY, no ED_ROOM_SLOTS), player 1 put into the same box with the
#          same trace. Expect: "[sweep] player 1 put at", no "[stay] room 90's own script" line, "[room] change
#          0x0000005A" (player 1 took the door; the script says so if it is missing), p1room 90 in the places, and the
#          ed113 chain lines (the node is player 1's: there is no player 2). Needed after the fix; before it, it is only
#          a baseline.
# Every step also prints the newest saves/reports/hang-*.txt if it is new, the head and tail of jtg-report.txt (control
# frame, LATE) and both.tsv's last row's p1room/p2room. A run whose folder is not new (jtg.sh stopped before making one,
# or every try was an *-introfail) is reported and its step stops, so an older run's evidence is never printed as this one's.
#
# ONLY A RUN CAN SHOW (not fixable here):
#   - Where the stick step takes player 2: lup is stick up, relative to player 2's camera then (which one is not known;
#     room 100's Camera05, the nearest to the box, looks toward +x and -y), not world +y, and 1692 is only about 100 from the box's x edges. The put is
#     already inside the box, so its enter should fire; if the [room] change line is missing, try ARRIVAL_X=1620.
#   - Whether tracing the hot getter 0x801F6300 from BASE+PUT_AT+10 slows the game enough to shift the frames or run into
#     ED_RUN_SECONDS=600 (compare a's frames with probe's), and whether flat's every-call log is bearable in its window.
# Grep strings are from runtime/host/hle.c: "[room] change"/"[room] later" are room_log(cpu, "change"/"later") (hle.c
# 11463, "[room] %s 0x%08X ...", rooms in hex), "[sweep] player 2 put at"/"not put: no floor under" are 17030/17022
# ("player %d"), "[chain] ... <-" is 5613 ("[fn] %s lr=0x%08X r3=0x%08X" 5628, the name 0x%08X when
# sdk_symbol_name has none),"[stay] player 2's change to room %d runs as theirs" 14780, "[stay] room
# %d's own script %d (queued by player 2's arrival) ran with room %d current" 20569, "[stay] WARNING" 14808, "[coop]
# player 2's first turn %s" 23363. ", as player" on the [stay] line exists ONLY AFTER THE FIX. (No "ASSERT" grep: that
# string is not in hle.c; a crash or hang shows as a new hang report and in `ed check`, which jtg.sh runs.)
#
# NOT SCRIPTED HERE (left out on purpose, as plans.md says):
#   - ed248: no cheap case. L08/2079 is 11 doors from chapter 8's start and needs a state made mid-script; L12/2777's
#     ed248 sits behind previous_room==265. The fix writes -21488 in the same swap, so ed248 is covered by reading the
#     code only.
#   - Room 81's body (L00/1816, level 0, a new game): scripted runs never got player 2 into 81 before player 1
#     (logs/run-20261115-2126*..2132*-jtg-room81-body*), so after the fix this is a check in the user's own play: look
#     in logs/play.log for "[stay] room 81's own script 1816 ... as player 2's" (only after the fix) and that the hanged
#     body is still hidden.
#   - What ed113/ed248 change on screen (player 2 casting the matching spell in room 90 or 202), an arrival camera
#     (L12/2777's IntroCam01) in player 2's set, and scripts that wait inside an arrival (1861, 2777): not scripted.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_RUN_AWAY=1 ED_COOP_LOG=1 ED_ROOM_LOG=1 ED_RUN_SHOTS_MS=250
SLOT=${ARRIVAL_SLOT:-2}
BASE=${ARRIVAL_BASE:-2255}   # chapter 12's base (build-run-pages.sh)
FR=600                       # the put at +30, the transition-3 fade, the arrival a frame after it, then a rest
X=${ARRIVAL_X:-1692}         # inside BoxRegion02 (1595..1790); 1620 if there is no floor under 1692
PUT_AT=30
BUILD=${ARRIVAL_BUILD:-now}
UNSET=
STAY="ED_ROOM_SLOTS=2 ED_COOP_STAY=1"   # apart
CHAIN="ED_TRACE_FN=0x801D6364,0x801F6300 ED_TRACE_CHAIN=1"
FLAT="ED_TRACE_FN=0x801D6364,0x801F6300"          # no chain mode: every call printed ("[fn] NAME lr=... r3=...")
TRACE_AFTER_PUT=$((BASE + PUT_AT + 10))           # a's and solo's trace start: after the put, inside the stick step
EARLY=${ARRIVAL_EARLY:-30}                        # near's and flat's trace start, this many frames before ARRIVAL_AT
# The short stick step after the put, so the zone scan sees the player inside the box. lup / stick_y 127 is stick up,
# relative to the camera (not world +y); where it takes them is only shown by a run (see the header).
P2_STEP="$((PUT_AT + 6)),$((PUT_AT + 14)),lup"
P1_STEP="$((PUT_AT + 6)),$((PUT_AT + 14)),0,0,127"

last_run() { ls -d logs/run-*-jtg-"$1" 2>/dev/null | tail -1; }
newest_hang() { ls saves/reports/hang-*.txt 2>/dev/null | tail -1; }
logs() { cat "$R"/page-*.log 2>/dev/null; }   # pagelog.py names them page-NNNN.log, so the glob is in order

go() {  # step name, then the env assignments for the run
    name="arrival-$1-$BUILD"; shift
    HANG0=$(newest_hang)
    R0=$(last_run "$name")   # an older run of the same name, which must not be taken for this one
    # (env's -u options must come before its assignments: UNSET holds them, set by solo.)
    env $UNSET JTG_SLOT=$SLOT JTG_BASE=$BASE JTG_FRAMES=$FR ED_RUN_SECONDS=600 "$@" sh tools/ed/jtg.sh 12 "$name" </dev/null
    rc=$?
    R=$(last_run "$name")
    echo "[arrival] $name: $R (jtg.sh exit $rc)"
    [ -n "$R" ] && [ "$R" != "$R0" ] || { echo "[arrival] no NEW run folder for $name (jtg.sh stopped first, or every try was an introfail)"; R=; return 1; }
    [ $rc = 0 ] || { echo "[arrival] jtg.sh failed for $name: its evidence is not read"; return 1; }
}

stay_frame() {  # the frame of the first block's [stay] line for 2477 (before the fix "(frame N)", after it "; frame N)")
    logs | grep "own script 2477 (queued by player 2's arrival) ran with room 90 current" | head -1 |
        sed -n 's/.*frame \([0-9]*\)).*/\1/p'
}

evidence() {  # what every step prints from $R
    echo "-- the nodes"
    logs | grep "\[coop\] player 2's first turn" | head -1
    echo "-- the put"
    logs | grep "\[sweep\] player [12] put at\|\[sweep\] player [12] not put: no floor under" | head -4
    [ -n "$(logs | grep "\[sweep\] player [12] put at" | head -1)" ] ||
        echo "[arrival] nobody was put (no \"[sweep] player N put at\"): the rest of this run is not the door's; try ARRIVAL_X=1620"
    echo "-- the change into room 90 (0x5A), the queue, the first block's run (first 3 of each)"
    logs | grep "\[stay\] player 2's change to room 90 runs as theirs" | head -3
    logs | grep "\[room\] change 0x0000005A" | head -3
    logs | grep "\[room\] later 0x000009AD .*calls: 0x80153B6C" | head -3
    logs | grep "own script 2477 (queued by player 2's arrival) ran with room 90 current" | head -3
    echo "-- the [stay] line as player 2's (only after the fix)"
    logs | grep "own script 2477 (queued by player 2's arrival) ran with room 90 current, as player" | head -2
    echo "-- [stay] WARNING lines: $(logs | grep -c "\[stay\] WARNING") (the first 3)"
    logs | grep "\[stay\] WARNING" | head -3
    N=$(stay_frame)
    [ -n "$N" ] && echo "[arrival] the arrival's frame: ARRIVAL_AT=$N"
    H=$(newest_hang)
    if [ -n "$H" ] && [ "$H" != "$HANG0" ]; then echo "-- NEW hang report: $H"; fi
    echo "-- jtg-report.txt (control frame, LATE; its room column is the game's current room only)"
    head -4 "$R/jtg-report.txt" 2>/dev/null
    tail -8 "$R/jtg-report.txt" 2>/dev/null
    echo "-- places: both.tsv's last row (frame, p1room, p2room)"
    awk -F'\t' 'NR == 1 { for (i = 1; i <= NF; i++) c[$i] = i; next }
        { l = "frame " $c["frame"] ": p1room " $c["p1room"] ", p2room " $c["p2room"] } END { print l }' "$R/both.tsv" 2>/dev/null
}

traced() {  # $1 chain or flat: the ed113 lines and the comparison with the nodes
    echo "-- ed113(90) and the main node it read"
    if [ "$1" = flat ]; then
        CALL="\[fn\] [^ ]* lr=0x[0-9A-F]* r3=0x0000005A "
        NODE="\[fn\] [^ ]* lr=0x801D63C0 r3=0x[0-9A-F]*"
        PICK='s/.* lr=0x801D63C0 r3=\(0x[0-9A-F]*\).*/\1/p'
        AT=                     # the [fn] lines carry no frame
    else
        CALL="\[chain\] 0x801D6364 r3=0x0000005A"
        NODE="\[chain\] 0x801F6300 r3=0x[0-9A-F]* frame=[0-9]* <- 0x801D63C0"
        PICK='s/.*\[chain\] 0x801F6300 r3=\(0x[0-9A-F]*\) .*/\1/p'
        AT=$(logs | grep "$NODE" | head -1 | sed -n 's/.* frame=\([0-9]*\) .*/\1/p')
    fi
    logs | grep "$CALL" | head -2
    logs | grep "$NODE" | head -2
    NODES=$(logs | grep "\[coop\] player 2's first turn" | head -1 |
        sed -n "s/.*player 1's node \(0x[0-9A-F]*\), player 2's \(0x[0-9A-F]*\)).*/\1 \2/p")
    READ=$(logs | grep "$NODE" | head -1 | sed -n "$PICK")
    N1=${NODES% *}; N2=${NODES#* }
    N=$(stay_frame)
    if [ -n "$NODES" ] && [ -z "$N" ]; then
        echo "[arrival] co-op, but no [stay] own-script line for 2477: the arrival never ran in the first block, so any node below is not the bug's"
    fi
    [ -n "$AT" ] && [ -n "$N" ] && [ "$AT" != "$N" ] &&
        echo "[arrival] the node line is at frame $AT, the [stay] line at $N: not the same run of 2477, read with care"
    if [ -z "$READ" ]; then
        if [ -z "$(logs | grep "$CALL" | head -1)" ]; then
            echo "[arrival] ed113(90) itself not seen: the trace started after it (ARRIVAL_EARLY larger), or the arrival never ran"
        elif [ "$1" = flat ]; then
            echo "[arrival] ed113(90) seen but no lr=0x801D63C0 line: the node read was not traced"
        else
            echo "[arrival] ed113(90) seen but no <- 0x801D63C0 line: the chain cap filled; run near (then flat) with ARRIVAL_AT=the [stay] line's frame"
        fi
    elif [ -z "$NODES" ]; then
        echo "[arrival] ed113 read node $READ (no first-turn line to compare: solo, or co-op never started)"
    elif [ "$READ" = "$N1" ]; then
        echo "[arrival] ed113 read node $READ = player 1's: the bug (expected before the fix)"
    elif [ "$READ" = "$N2" ]; then
        echo "[arrival] ed113 read node $READ = player 2's: the fix holds (expected after the fix)"
    else
        echo "[arrival] ed113 read node $READ: neither player 1's $N1 nor player 2's $N2"
    fi
}

run_one() {
case "$1" in
probe) # Co-op apart, untraced: the control frame, the put, the change, the queue and the arrival's frame.
    go probe $STAY JTG_PUT="$PUT_AT,2,$X,-4380,400" JTG_P2_SCRIPT="$P2_STEP" || return
    evidence ;;
a)     # Co-op apart, traced from after the put.
    go a $STAY $CHAIN ED_TRACE_FROM=$TRACE_AFTER_PUT JTG_PUT="$PUT_AT,2,$X,-4380,400" JTG_P2_SCRIPT="$P2_STEP" || return
    evidence; traced chain ;;
near|flat)  # As a, the trace from just before the arrival (ARRIVAL_AT=N from probe's or a's [stay] line); flat unchained.
    [ -n "$ARRIVAL_AT" ] || { echo "[arrival] $1 needs ARRIVAL_AT=the frame of the [stay] room 90 line"; return 1; }
    FROM=$((ARRIVAL_AT - EARLY))
    [ $FROM -ge $TRACE_AFTER_PUT ] ||
        echo "[arrival] the trace starts at $FROM, before the put's +10 ($TRACE_AFTER_PUT): ARRIVAL_AT or ARRIVAL_EARLY is off"
    if [ "$1" = near ]; then T=$CHAIN; END=$((ARRIVAL_AT + 120)); else T=$FLAT; END=$((ARRIVAL_AT + 30)); fi
    go "$1" $STAY $T ED_TRACE_FROM=$FROM ED_RUN_FRAMES=$END \
        JTG_PUT="$PUT_AT,2,$X,-4380,400" JTG_P2_SCRIPT="$P2_STEP" || return
    evidence; if [ "$1" = near ]; then traced chain; else traced flat; fi ;;
solo)  # Co-op off: player 1 takes the same door, traced the same way. No [stay] line expected.
    # (-u is only defensive: STAY is never exported, and with mod_coop=0 the frontend unsets both anyway.)
    UNSET="-u ED_COOP_STAY -u ED_ROOM_SLOTS"
    go solo JTG_SET="mod_coop=0" $CHAIN ED_TRACE_FROM=$TRACE_AFTER_PUT \
        JTG_PUT="$PUT_AT,1,$X,-4380,400" JTG_P1_SCRIPT="$P1_STEP"; ok=$?
    UNSET=
    [ $ok = 0 ] || return
    evidence; traced chain
    [ -n "$(logs | grep "\[sweep\] player 1 put at" | head -1)" ] || echo "[arrival] solo: player 1 was not put: nothing below is the door's"
    [ -n "$(logs | grep "\[room\] change 0x0000005A" | head -1)" ] || echo "[arrival] solo: no change to room 90: player 1 never took the door"
    echo "-- [stay] room 90 own-script lines in the solo run (expect 0)"
    logs | grep -c "\[stay\] room 90's own script" ;;
*)     echo "[arrival] no step named $1" ;;
esac
}

if [ $# -eq 0 ]; then set -- probe a; fi
for s in "$@"; do run_one "$s"; done
echo RUNS DONE
