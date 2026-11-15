#!/bin/sh
# The Tome room with the players apart (design-tome-room.md §10; tome-trip-impl-notes.md says what was built): every run
# from a FRESH Jump-to-Game state made here on the current build (the user's rule: no old saves, no `ed test`, no
# `ed chapters`), with the window off the visible desktop and the sound muted (ED_RUN_AWAY=1), the stay on, every log.
# Built already (the scratchpad build.bat). Run from Git Bash: sh docs/research/separate-rooms/build-run-tome.sh [RUN...]
#   RUN: s4 s6 s8 s11 (the fresh states) 1 2 3 4 5a 5b 6a 6b 6c solo follow off; none given: all, in that order.
# Frames: a state saved at S keeps its guest clock, so ED_PUT / ED_INPUT_SCRIPT / ED_PAD2_SCRIPT here are absolute (S+N).
# ED_RUN_SCRIPT=DELAY:SCRIPT counts DELAY from the first pad read after the load (as build-run-lights2.sh S4 uses it), so
# its delays are small. Each door script twice, 60 frames apart (walking into its zone, then pressing its prompt).
# Read each run's `python -I tools/ed/ed.py timeline RUN` and EVENTS.log first (pagelog.py now lists [tome-trip] lines),
# then `grep -h "\[tome-trip\]" RUN/page-*.log`. A run fails on any "[tome-trip] WARNING", a saves/reports/hang-*.txt, a
# player in the wrong room at the end of both.tsv, or a place more than 4 units off the noted one.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_RUN_AWAY=1 ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_COOP_LOG=1 ED_ROOM_LOG=1 ED_RUN_SHOTS_MS=250
LONG=9000   # frames after S each Tome run lasts: the find, the film (its length not read), the way back, a while after
SECS=600

last_run() { ls -d logs/run-*-"$1" 2>/dev/null | tail -1; }
# "control at frame F, room R: ..." from a run's jtg-report.txt -> "F R" (R decimal), or nothing.
control_of() {
    python -I - "$1/jtg-report.txt" <<'EOF'
import re, sys
try:
    text = open(sys.argv[1], encoding='utf8', errors='replace').read()
except OSError:
    sys.exit(1)
m = re.search(r'control at frame (\d+), room (\S+?):', text)
if not m:
    sys.exit(1)
print(m.group(1), int(m.group(2), 0))
EOF
}
# A's presses: n presses of player 1's A (pad 0x0100) or player 2's a, `gap` frames apart from `at`.
# The Tome's pickup: B on its EXAMINE (1413), then A on Yes; alternated, so a missed one is taken by the next pair.
p1_presses() { at=$1; n=$2; gap=$3; out=; i=0; while [ $i -lt $n ]; do if [ $((i % 2)) = 0 ]; then k=0x0200; else k=0x0100; fi; out="$out${out:+;}$at,$((at+6)),$k"; at=$((at+gap)); i=$((i+1)); done; printf '%s' "$out"; }
p2_presses() { at=$1; n=$2; gap=$3; out=; i=0; while [ $i -lt $n ]; do if [ $((i % 2)) = 0 ]; then k=b; else k=a; fi; out="$out${out:+;}$at,$((at+6)),$k"; at=$((at+gap)); i=$((i+1)); done; printf '%s' "$out"; }

# ---- The fresh states. Step 0 (design §10): a no-press run for the chapter's control frame and start room; then, when the
# start room is not the chapter's Tome door room, its door script (in follow mode, ED_COOP_STAY unset, so the bring takes
# player 2 along, as build-run-lights2.sh S4 does); then the state, both players idle, 90 frames after control or arrival.
# Writes saves/tome-chN.state and logs/tome-chN.env (S, the settings to load it with).
fresh() {  # chapter, the Tome door room, the door script into that room (or -)
    ch=$1; want=$2; into=$3
    rm -f "saves/tome-ch$ch.state" "logs/tome-ch$ch.env"
    JTG_SLOT=1 JTG_FRAMES=1500 sh tools/ed/jtg.sh "$ch" "tome-s0-ch$ch" </dev/null
    R0=$(last_run "jtg-tome-s0-ch$ch")
    CR=$(control_of "$R0") || { echo "[tome] chapter $ch: no control in $R0"; return 1; }
    F=${CR% *}; ROOM=${CR#* }
    echo "[tome] chapter $ch: control at frame $F in room $ROOM (the door room is $want)"
    if [ "$ROOM" = "$want" ] || [ "$into" = - ]; then
        S=$((F + 90))
        JTG_SLOT=1 ED_SAVE_STATE="$S:saves/tome-ch$ch.state" ED_RUN_FRAMES=$((S + 60)) sh tools/ed/jtg.sh "$ch" "tome-state-ch$ch" </dev/null
    else
        S=$((F + 900))  # the door script's dissolve or motion, its countdown and the arrival, then a rest
        JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT="$((F + 60)):$into,$((F + 120)):$into" ED_SAVE_STATE="$S:saves/tome-ch$ch.state" \
            ED_RUN_FRAMES=$((S + 60)) sh tools/ed/jtg.sh "$ch" "tome-state-ch$ch" </dev/null
    fi
    RS=$(last_run "jtg-tome-state-ch$ch")
    [ -f "saves/tome-ch$ch.state" ] || { echo "[tome] chapter $ch: no state saved ($RS)"; return 1; }
    printf 'S=%s\nSET=%s\n' "$S" "$RS/settings.ini" > "logs/tome-ch$ch.env"
    grep -h "^\[rooms\] \|^\[room\] change" "$RS"/page-*.log 2>/dev/null | tail -3
    echo "[tome] chapter $ch: saves/tome-ch$ch.state at frame $S (check the last both.tsv row of $RS: both in room $want)"
}
load_env() {  # chapter -> S, SET
    [ -f "logs/tome-ch$1.env" ] || { echo "[tome] make the chapter $1 state first (RUN s$1)"; return 1; }
    . "logs/tome-ch$1.env"
}
from_state() {  # chapter, name, then the env assignments for the run
    ch=$1; name=$2; shift 2
    env ED_SETTINGS="$SET" ED_RUN_FRAMES=$((S + LONG)) ED_RUN_SECONDS=$SECS "$@" sh tools/ed/run.sh "saves/tome-ch$ch.state" "$name" </dev/null
    R=$(last_run "$name")
    echo "[tome] $name: $R"
    grep -h "\[tome-trip\]" "$R"/page-*.log 2>/dev/null
    ls saves/reports/hang-*.txt 2>/dev/null | tail -1
}

# Room 5 (every chapter's Tome room): CameraRgn02 (min -1370 -139 284, max -1203 151 534) runs 1413, the Tome's EXAMINE then
# PICK UP. 852 needs about 150 frames after the arrival (design §10 run 1). Put at T5, A from T5+200, 14 presses 45 apart
# (EXAMINE, Yes, the words).
TOME_AT="-1290,0,300"

run_one() {
case "$1" in
s4) fresh 4 338 - ;;                # Karim: room 338 (its only way out is the ladder L04/132, Region01)
s5) # Max: room 258 (his desk, TomeRegion L05/2434); 930 is 253's SECRET PASSAGE into it, open with flag 184 (found).
    export ED_SET_FLAG=30:184; fresh 5 258 930; unset ED_SET_FLAG ;;
m1) # Max (ch 5) together in 258, the Tome's after-find script 1599 started (film 114, world changes, 255's summon scene,
    # back to 258 at MaxSpawnMkr01, the letter). Pass (2026-11-15, the user): both see the film; both go to 255 and back;
    # each back where they stood; the letter and magick_give as player 1's; flags 321, 944, 490 on.
    load_env 5 || return
    from_state 5 tome-m1-c5-together ED_RUN_SCRIPT="300:1599" ;;
m2) # Max apart: player 2 out to 253 first (640: 258's SECRET PASSAGE, Region08), then 1599. Pass: player 2 brought to 255
    # with player 1, then back in 253 at their spot; player 1 back in 258 at theirs; the letter.
    load_env 5 || return
    # (Region08: x -188..97, y 392..697; player 2 put in it presses B on its SECRET PASSAGE, twice as the doors' runs do.)
    # (-60,600 has furniture at height 205, above the box's 171: 2026-11-15's first try never took the passage.)
    from_state 5 tome-m2-c5-apart ED_PUT="$((S + 30)),2,-120,470,10" ED_PAD2_SCRIPT="$((S + 120)),$((S + 126)),b;$((S + 180)),$((S + 186)),b" \
        ED_RUN_SCRIPT="1300:1599" ;;
s6) fresh 6 303 626 ;;              # Lindsey: room 303 (BoxRegion02, L06/335, the unprompted zone exit); 626 from 143
s8) fresh 8 204 249 ;;              # Roberto: room 204 (Region02 L08/327 to 5, Region01 L08/329 to 211); 249 from 211
s11) fresh 11 43 1036 ;;            # Michael: room 43 (Region02 L11/358 to 5, BoxRegion02 L11/360 to 60); 1036 from 60
1)  # Ch 4, together in 338: player 1 takes the ladder (132), player 1 takes the Tome. A player 1 (exact, the user's
    # decision), B player 2 (exact). Pass: "[tome-trip] player 1's door from room 338 to room 5: both go"; no
    # "[split] ... stays in room 338"; "both in room 5"; "the way back kept: room 338 ... A player 1 (exact), B player 2
    # (exact)"; "loaded fresh" at B's change and "kept" at A's; each "back in room 338" within 4 units; 868 then 1349 as
    # player 1 (flag 522, zombies 174-178).
    load_env 4 || return
    T5=$((S + 600))
    from_state 4 tome-1-c4-p1door-p1tome ED_RUN_SCRIPT="60:132,120:132" ED_PUT="$T5,1,$TOME_AT" \
        ED_INPUT_SCRIPT="$(p1_presses $((T5 + 200)) 14 45)" ;;
2)  # Ch 4, together: player 2 takes the ladder (put into Region01, a), player 2 takes the Tome. Pass: the trip names
    # player 2; player 1 moved by the game's change, player 2 on the arrival spot; "kept ... A player 1 (exact), B player 2
    # (exact)"; 1599 as player 2's use or player 1's (say which: F18); 1349 as player 1.
    load_env 4 || return
    T5=$((S + 700))
    from_state 4 tome-2-c4-p2door-p2tome ED_PUT="$((S + 30)),2,-800,-80,10;$T5,2,$TOME_AT" \
        ED_PAD2_SCRIPT="$((S + 120)),$((S + 126)),a;$(p2_presses $((T5 + 200)) 14 45)" ;;
3)  # Ch 8, apart: player 2 below in 211 (329: Region01, a), then player 1 takes 327 (twice) to room 5 and takes the Tome.
    # Pass: the trip from 204; "player 2 brought from room 211 (event 61 to it: sent"; the way back: player 2 to 211 at the
    # noted spot ("loaded fresh"), then player 1 to 204 at their noted spot by the kept ticket (868); then player 2 climbs
    # back (249: SphereRegion04, a) and "[stay] player 2's change ... runs as theirs"; player 2 opens their Tome (Start) and
    # closes it (B).
    load_env 8 || return
    T5=$((S + 1200)); BACK=$((S + 7000))
    from_state 8 tome-3-c8-apart-p1door ED_RUN_SCRIPT="600:327,660:327" ED_PUT="$((S + 30)),2,-150,675,0;$T5,1,$TOME_AT;$BACK,2,-19,-1307,0" \
        ED_INPUT_SCRIPT="$(p1_presses $((T5 + 200)) 14 45)" \
        ED_PAD2_SCRIPT="$((S + 120)),$((S + 126)),a;$((BACK + 90)),$((BACK + 96)),a;$((BACK + 500)),$((BACK + 506)),start;$((BACK + 700)),$((BACK + 706)),b" ;;
4)  # Ch 8, apart, the mirror: player 1 to 211 (329 twice), player 2 takes the Tome door (Region02, a) and the Tome.
    # Pass: player 1 back in 211 at the noted spot (fresh load), then player 2 by an R4 bracket ("[stay] player 2's change
    # to room 204 runs as theirs") at their noted spot with 868's fade in their half; pictures: the dissolve in both halves.
    load_env 8 || return
    T5=$((S + 1300))
    from_state 8 tome-4-c8-apart-p2door ED_RUN_SCRIPT="60:329,120:329" ED_PUT="$((S + 600)),2,257,-580,0;$T5,2,$TOME_AT" \
        ED_PAD2_SCRIPT="$((S + 700)),$((S + 706)),a;$(p2_presses $((T5 + 200)) 14 45)" ;;
5a) # Ch 11, apart: player 2 to 60 (360: BoxRegion02, a), player 1 takes 358 (twice) to room 5 (the 62-tick fade, 868 ->
    # 1161) and the Tome.
    load_env 11 || return
    T5=$((S + 1300))
    from_state 11 tome-5a-c11-apart-p1door ED_RUN_SCRIPT="600:358,660:358" ED_PUT="$((S + 30)),2,0,-1750,0;$T5,1,$TOME_AT" \
        ED_PAD2_SCRIPT="$((S + 120)),$((S + 126)),a" ED_INPUT_SCRIPT="$(p1_presses $((T5 + 200)) 14 45)" ;;
5b) # Ch 11, apart, the other order: player 1 to 60 (360 twice), player 2 takes 358 (Region02, a) and the Tome.
    load_env 11 || return
    T5=$((S + 1400))
    from_state 11 tome-5b-c11-apart-p2door ED_RUN_SCRIPT="60:360,120:360" ED_PUT="$((S + 600)),2,-85,1200,0;$T5,2,$TOME_AT" \
        ED_PAD2_SCRIPT="$((S + 700)),$((S + 706)),a;$(p2_presses $((T5 + 200)) 14 45)" ;;
6a) # Ch 6, together in 303: the UNPROMPTED zone exit (335 on entering BoxRegion02: player 1 put into it). The exact
    # return puts player 1 back INSIDE BoxRegion02 with the Tome carried, where 335 would ticket them to room 143 at once.
    # Pass: "player 1 back in room 303" within 4 units of the noted spot; "[tome-trip] player 1's change to room 303 runs (at
    # a saved place, ... come into)"; "[coop] player 1, brought along, stands inside the way back (script 335) as come into"
    # or "... its room change passed over"; NO "[room] ticket" to room 143 after it; both in 303 at the end of both.tsv.
    load_env 6 || return
    T5=$((S + 700))
    # (The Tome's after-find script 1599 is started directly: test presses don't move the Tome's EXAMINE words, 2026-11-14.)
    from_state 6 tome-6a-c6-zone-exact ED_PUT="$((S + 30)),1,6,-1597,0" ED_RUN_SCRIPT="1000:1599" ;;
6b) # Ch 6, apart: player 2 to 59 first (334: BoxRegion01, entered), then player 1 into BoxRegion02 (335). Pass as 6a, and
    # player 2 back in 59 at their noted spot.
    load_env 6 || return
    T5=$((S + 1100))
    from_state 6 tome-6b-c6-zone-apart ED_PUT="$((S + 30)),2,0,1620,0;$((S + 500)),1,6,-1597,0" ED_RUN_SCRIPT="1500:1599" ;;
6c) # Ch 6, as 6a with ED_TOME_BACK=marker: the taker back at EntryMkr_TomeReturn (-6, -1425, 7), outside the region: the
    # game's own spot, to compare.
    load_env 6 || return
    T5=$((S + 700))
    from_state 6 tome-6c-c6-zone-marker ED_TOME_BACK=marker ED_PUT="$((S + 30)),1,6,-1597,0" ED_RUN_SCRIPT="1000:1599" ;;
z4) # Ch 4 as run 1, the Tome's after-find script 1599 started directly (test presses don't move its EXAMINE words). Pass:
    # both back in 338 at their noted spots; "the taker stood in the way in's zone ... marker is in it: exact kept".
    load_env 4 || return
    from_state 4 tome-z4 ED_RUN_SCRIPT="60:132,120:132,1000:1599" ;;
z8a) # Ch 8 apart as run 3 (player 2 down in 211 first, player 1 takes 327), 1599 started. Pass: player 2 back in 211,
    # player 1 back in 204, each at their noted spot.
    load_env 8 || return
    from_state 8 tome-z8a ED_PUT="$((S + 30)),2,-150,675,6" ED_PAD2_SCRIPT="$((S + 120)),$((S + 126)),b;$((S + 180)),$((S + 186)),b" \
        ED_RUN_SCRIPT="700:327,760:327,1700:1599" ;;
z8b) # Ch 8 apart as run 4 (player 1 down in 211 first, player 2 takes 327 by B), 1599 started. Pass: player 1 back in 211,
    # player 2 back in 204, each at their noted spot.
    load_env 8 || return
    from_state 8 tome-z8b ED_PUT="$((S + 600)),2,257,-580,6" ED_PAD2_SCRIPT="$((S + 700)),$((S + 706)),b;$((S + 760)),$((S + 766)),b" \
        ED_RUN_SCRIPT="60:329,120:329,1800:1599" ;;
solo)  # Ch 4 solo (co-op off), from a cold Jump to Game (a co-op state played alone parks player 2): the ladder, the Tome,
    # the film, the way back. Pass: no [tome-trip], [stay] or [split] line; the same room changes and scripts as solo
    # before the build.
    load_env 4 || return
    F=$((S - 90))
    JTG_SLOT=1 JTG_SET="mod_coop=0" JTG_BASE=$((F + 30)) JTG_FRAMES=$LONG ED_RUN_SECONDS=$SECS \
        ED_RUN_SCRIPT="$((F + 60)):132,$((F + 120)):132" JTG_PUT="570,1,$TOME_AT" JTG_P1_SCRIPT="$(p1_presses 770 14 45)" \
        sh tools/ed/jtg.sh 4 tome-solo-c4 </dev/null
    R=$(last_run jtg-tome-solo-c4); echo "[tome] solo: $R"; grep -ch "\[tome-trip\]\|\[stay\]\|\[split\]" "$R"/page-*.log ;;
follow)  # Ch 4, follow mode (ED_COOP_STAY unset): as run 1. Pass: no [tome-trip] line; 2026-11-08's sequence (the bring).
    load_env 4 || return
    T5=$((S + 600))
    from_state 4 tome-follow-c4 env -u ED_COOP_STAY ED_RUN_SCRIPT="60:132,120:132" ED_PUT="$T5,1,$TOME_AT" \
        ED_INPUT_SCRIPT="$(p1_presses $((T5 + 200)) 14 45)" ;;
off)  # Ch 4, ED_TOME_TRIP=0 with the stay: today's behaviour (fx-a-1), player 1 alone to room 5. No [tome-trip] line.
    load_env 4 || return
    T5=$((S + 600))
    from_state 4 tome-off-c4 ED_TOME_TRIP=0 ED_RUN_SCRIPT="60:132,120:132" ED_PUT="$T5,1,$TOME_AT" \
        ED_INPUT_SCRIPT="$(p1_presses $((T5 + 200)) 14 45)" ;;
*) echo "[tome] no run named $1" ;;
esac
}

if [ $# -eq 0 ]; then set -- s4 s6 s8 s11 1 2 3 4 5a 5b 6a 6b 6c solo follow off; fi
for r in "$@"; do run_one "$r"; done
echo RUNS DONE
