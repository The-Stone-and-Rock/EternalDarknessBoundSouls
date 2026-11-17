#!/bin/sh
# L06's traps (ed156) run from other_enter for player 2: the test runs for docs/research/kit-bugs/plans.md section 3
# (read it first; its "Fix plan" and "Test plan (the skeptic's corrected version)" are what this script follows).
#
# THE BUG (plans.md 3, read in the code, the run-time parts inferred). L06/2105 is room 168's TrapRgn04 enter script AND its
# other_enter script. When player 2's body crosses the box it runs twice:
#   (1) player 2's look (coop_prompt_look -> COOP_SCAN, inside their turn) runs the enter field as player 2, with their room
#       in play: right;
#   (2) the game's own step for player 2's object (0x80133434 -> zone scan 0x80134A44, r27 = 1 "other") runs the other_enter
#       field at lr 0x80134B38 outside any bracket, so coop_run_script lets it run as player 1, with player 1's room in play
#       and player 1's script states.
#   Run (2) fires the trap again (sound 273, event 154 to 396-399), and its ed156 (0x800849AC switches on the room in play:
#   313, 233, 168, 306, 308) writes player 1's room's record, or nothing for any other room (139). About 40 frames later the
#   game's timer runs player 1's copy of 2105 again (lr 0x80163E5C, state 1: look_at / face_morph on player 1).
# THE PLANNED FIX (plans.md 3, STEP 1): in coop_run_script, before the g_coop_p1_ran_script block, pass over a zone run at
#   lr 0x80134B38 of player 2's body (r27 == 1, r29 == player 2's object) when their look scanned in the last 2 frames
#   (g_coop_p2_look_frame) and the zone's player field holds the same script (coop_zone_other_dup). Logged as
#   "[coop] zone script N (player 2's body, other field) passed over: ...".
# STEP 0 of the plan is a LOG-ONLY build first (no change in behaviour): a notify on 0x800849AC (or ed156's own handler
#   0x80015AA8 if that address has no dispatch entry) printing "[coop] ed156 record: room in play R (...; the player's id I,
#   turn T, look L, frame F)". Without it runs a and b have no evidence of the wrong record before the fix.
#
# WHAT TO RUN, IN THIS ORDER (sh -n only checks this file; every step below starts the game):
#   1. On the STEP 0 build (log only):  sh docs/research/kit-bugs/build-run-traps.sh probe s0 t a b solo
#   2. On the STEP 1 build (the fix):   sh docs/research/kit-bugs/build-run-traps.sh probe s0 t a b solo
#   probe and s0 MUST be run again on each build: a state is made on the current build and thrown away when the code
#   changes (test_save_state's comment, hle.c:19897), and control's frame F is measured on it. t, a and b refuse a state
#   older than build-win/EternalDarkness.exe; s0 and solo refuse an F (logs/trap-ch6.env) older than it.
#   No ed test, no ed chapters, no older state. Run from Git Bash, one game at a time, never with the user's game open:
#   t, a and b go through tools/ed/run.sh, whose ED_RUN_FRAMES closes EVERY EternalDarkness.exe by name; probe, s0 and
#   solo go through tools/ed/jtg.sh with JTG_SLOT=1, which runs and closes only its copy build-win/EDrun1.exe.
#   No steps given = all six, in that order. If probe or s0 fails (no control frame, or s0's checks below not met), the
#   steps after it are not run; a failed precondition in t, a, b or solo is printed and listed at the end.
#
# THE STEPS. Chapter 6 (Lindsey, level 06, start room 303), JTG_P1=6 JTG_P2=2 JTG_SLOT=1, ED_RUN_AWAY=1 for every run
# (jtg.sh and run.sh set ED_COOP_LOG, ED_COOP_TRACE and ED_ROOM_LOG themselves). TrapRgn04 (room 168): box x -375..-77,
# y -1136..-944, z -8..175; every put is at its middle (-226,-1040,10) and followed by a 6-frame stick step (a put alone
# does not prove the zone fires: docs/coop-failed-attempts.md:527, 566).
#   probe  a cold Jump to Game in follow mode (as s0: ED_ROOM_SLOTS=2, ED_COOP_STAY unset; a launch naming neither switch
#          would get both, frontend.cpp rooms_follow_coop), no presses, JTG_FRAMES=1500 (base 2300, closes at 3800).
#          Reads "control at frame F, room R" from jtg-report.txt (jtg_report.py --wait as the fallback), requires R = 303,
#          and keeps F in logs/trap-ch6.env. (The Tome runs measured chapter 6's control at 1486-1507 in room 303,
#          logs/run-*-jtg-tome-s0-ch6, with both switches on; the probe measures it again on this build.)
#   s0     follow mode (ED_ROOM_SLOTS=2, ED_COOP_STAY and ED_COOP_STAY_ASKED unset), cold Jump to Game: JTG_BASE=F+30,
#          ED_RUN_SCRIPT F+60:1467 (L06/1467: room_queue(168) + go_to_room(168, EntryMkr01); `ed door 139` says it asks
#          nothing, so one run is the door), the state saved at S=F+400 with both idle, closed at S+100. The state is
#          deleted first. CHECKED (s0 fails, and keeps no S, unless all hold): "running script 1467", "[room] change
#          0x000000A8" (168), NO "player 2 stays in room" line, the "saving a state" line and the file, and both.tsv's last row at or before
#          S with p1room = p2room = 168 (EntryMkr01 is at -70,-2916; the places are printed). If player 2 is not in 168 the
#          follow did not bring them: raise S (S0_AFTER, default 400) and run s0 again.
#   t      together, follow mode, from the state: player 2 put in at S+60, stick up S+62..S+68, closed at S+400.
#   a      apart, no record: ED_COOP_STAY=1, ED_RUN_SCRIPT 60:1465 (from the load; player 1 to 139, EntryMkr02), player 2
#          stays in 168; put at S+400, step S+402..S+408, closed at S+700.
#   b      apart, the wrong record: as a plus 400:1468 (player 1 on to 313); put at S+800, step S+802..S+808, closed S+1100.
#   solo   co-op off (JTG_SET=mod_coop=0), cold Jump to Game: F+60:1467, player 1 put in at F+400, stick up F+402..F+408
#          (ED_INPUT_SCRIPT buttons 0, stick 0,127), closed at F+700. F is co-op's control frame; solo's is not measured,
#          so the 1467 and room checks below say whether F+60 came after solo's control.
#   ED_RUN_SCRIPT's delays count from the first pad read after the boot (hle.c test_run_script), and run.sh's load is at
#   block 0, before it: in t, a and b they count from the load. ED_PUT, ED_PAD2_SCRIPT, ED_INPUT_SCRIPT, ED_SAVE_STATE and
#   ED_RUN_FRAMES are absolute frames (the state keeps its frame, so S+N is N frames after the load).
#   1465 and 1468 also ask nothing (`ed door 168`, `ed door 139`): one run each. Their entry markers are outside every
#   region of the room they land in (139's EntryMkr02 -1320,218; 313's EntryMkr02 3200,-94; `ed room 139`, `ed room 313`).
#
# THE PRECONDITIONS EACH STEP CHECKS (printed as "PRECONDITION NOT MET"; the trap lines mean nothing without them):
#   t      "[sweep] player 2 put at", no "player 2 stays in room" line (other [stay] lines can print in follow mode),
#          both players in 168 at the step's first frame (both.tsv's p1room/p2room), and player 2 inside TrapRgn04's x/y in
#          room 168 in some both.tsv row from the put to the step's end.
#   a      "running script 1465", "[room] change 0x0000008B", "[stay] player 2 stays in room 168 ... went to room 139",
#          player 1 in 139 and player 2 in 168 at the step's first frame, then the put and the box as in t.
#   b      as a, plus "running script 1468", "[room] change 0x00000139", "... went to room 313", and player 1 in 313 at the
#          step's first frame.
#   solo   "running script 1467", "[room] change 0x000000A8", "[sweep] player 1 put at", player 1 in 168 at the step's
#          first frame and inside the box after the put.
#
# WHAT EACH STEP'S OUTPUT MEANS (grep of page-*.log, not *.log: EVENTS.log repeats the lines). Room numbers in [room]
# lines are hex (168 = 0x000000A8, 139 = 0x0000008B, 313 = 0x00000139); [stay], the trace and both.tsv print them in
# decimal. Each step prints every evidence line (none cut), the 2105 runs counted by lr and who, the ed156 records counted
# by room, turn and id, and any saves/reports/hang-*.txt newer than the run (there must be none).
#   Before the fix (STEP 0 build):
#   - t, a, b: "[sweep] player 2 put at ...", then around the step two "[trace] script run r3 2105 r4 <zone, decimal>
#     lr 0x80134B38 who <A>" / "... who <B>" lines: A = player 2's id (their look), B = player 1's id (the other_enter run).
#     About 40 frames later "[trace] script run r3 2105 ... lr 0x80163E5C who <B>" (the timer runs player 1's copy).
#   - "[coop] ed156 record:" lines: t two with "room in play 168"; a one "room in play 168" (turn 1, id A) and one
#     "room in play 139" (turn 0, id B: nothing written, 139 is not in the switch); b "room in play 168" and
#     "room in play 313" (313's record written, 0x80323920+296).
#   - a and b: "[stay] player 2 stays in room 168 (slot N) while player 1 went to room 139" (b also "... room 313"; the
#     line prints once per room).
#   - solo: one "script run r3 2105 ... lr 0x80134B38 who <player 1>", one "ed156 record: room in play 168", one timer run
#     about 40 frames later.
#   - Flag 555 (set by L06/1783, ButtonRgn01) does NOT hide the 2105 lines: `ed script 2105 --level 6` tests its state
#     and plays sound 273 before it reads flag 555, which only skips the events and ed156 (goto L79 still sets state 1
#     and waits 40). Its sign is 2105 trace lines with no "[coop] ed156 record" line on a build with the STEP 0 notify.
#     No 2105 line at all means the put or the step missed the box, or the game's scan did not run.
#   After the fix (STEP 1 build, s0 made again):
#   - t, a, b: one "lr 0x80134B38 who <A>" run of 2105 per crossing; the who <B> trace is followed at once by "[coop] zone
#     script 2105 (player 2's body, other field) passed over: their look runs it as theirs (zone ..., room in play
#     168/139/313, theirs 168, ...)".
#   - exactly one "ed156 record: room in play 168" per crossing (turn 1, id A in a and b); no "room in play 139" or
#     "room in play 313"; no "lr 0x80163E5C who <B>" run of 2105.
#   - solo: as before the fix (no "passed over").
#   - every run: no hang report.
#
# NOT REACHED BY THIS SCRIPT (left out, not guessed):
#   - What the duplicate trap does on screen and in sound (sound 273 twice, the vents 396-399 sent event 154 twice): the
#     runs are muted (ED_RUN_AWAY) and shots/ can only be looked at by hand; nothing here grades them.
#   - What the gas ramp looks like in either half (the world animator 0x80082090 plays only the room in play's record;
#     the fix leaves that alone, plans.md 3 "Left as it is").
#   - Whether player 1's script-state copy at 1 changes their next crossing (no run here crosses twice as player 1).
#   - An "ASSERT" grep: plans.md's list has it, but no hle.c line prints it (it is aurora's macro), so it is left out;
#     a halt shows as a saves/reports/hang-*.txt, which every step checks.
# Still inferred (a run shows them; the checks above catch the last three): that the game's step scans player 2's
# object (if not, t shows only the who <A> line and the bug is not there), that flag 555 is 0 at chapter 6's start, that
# 0x800849AC can take a notify (else 0x80015AA8, same words), that 1465/1468 land cleanly under the stay with 168 kept in
# its slot, that S+400 is long enough for the follow to bring player 2, and that solo reaches control by F+60.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_RUN_AWAY=1 ED_RUN_SHOTS_MS=250
export JTG_P1=6 JTG_P2=2 JTG_SLOT=1
STATE=saves/trap-ch6-168.state
ENVF=logs/trap-ch6.env          # F (control), S (the state's frame), SET (the state run's settings.ini)
TRAP="-226,-1040,10"            # TrapRgn04's middle, room 168
# The evidence lines. Where each comes from in runtime/host/hle.c today:
#   "[trace] script run r3 2105 "   trace_look's "[trace] %s r3 %d r4 %d lr 0x%08X who %u" (11768) with "script run" (20664)
#   "[sweep] player"                ED_PUT's "[sweep] player %d put at" / "not put" (17030, 17022)
#   "[stay] player 2 stays"         23820
#   "[room] change 0x"              room_log's "[room] %s 0x%08X" (11463) with "change" (11627); the room is hex
#   "running script"                test_run_script's "[test] frame %llu: running script %d" (19986)
#   "ed156 record"                  ONLY AFTER plans.md's STEP 0 (the log-only build) and on the fix's build
#   "zone script 2105"              ONLY AFTER THE FIX (STEP 1's "[coop] zone script %d (player 2's body, other field)
#                                   passed over")
KEYS='\[trace\] script run r3 2105 \|\[sweep\] player\|\[stay\] player 2 stays\|\[room\] change 0x\|running script\|ed156 record\|zone script 2105'

last() { ls -d logs/run-*-"$1" 2>/dev/null | tail -1; }
# The run's folder only if this run made it: the newest one, and not the one that was newest before the run started (a
# run that never started, "EDrun1.exe is already running" or a refusal, leaves the old folder newest: never read it).
fresh() {  # folder name, the newest folder before the run
    R=$(last "$1")
    if [ -z "$R" ] || [ "$R" = "$2" ]; then R=; return 1; fi
}
has() { grep -q "$1" "$R"/page-*.log 2>/dev/null; }
miss() { echo "[traps] $N: PRECONDITION NOT MET: $*"; MISS="$MISS $N"; }
# both.tsv's last row at or before a frame: frame, room in play, p1room, p2room, then both players' x y (decimal rooms).
row_at() {  # frame
    awk -F'\t' -v f="$1" 'NR == 1 { for (i = 1; i <= NF; i++) c[$i] = i; next }
        $c["frame"] + 0 <= f + 0 { r = $c["frame"] " " $c["room"] " " $c["p1room"] " " $c["p2room"] " P1 " int($c["p1x"]) " " int($c["p1y"]) " P2 " int($c["p2x"]) " " int($c["p2y"]) }
        END { print r }' "$R/both.tsv" 2>/dev/null
}
rooms_at() {  # frame, player 1's room, player 2's room ("-" for any): a miss unless both.tsv says so
    W=$(row_at "$1"); echo "[traps] $N: both.tsv at frame $1: frame room p1room p2room = $W"
    set -- "$1" "$2" "$3" $W
    [ -n "$4" ] || { miss "no both.tsv row at or before frame $1"; return 1; }
    [ "$2" = - ] || [ "$6" = "$2" ] || miss "player 1 in room $6 at frame $1, not $2"
    [ "$3" = - ] || [ "$7" = "$3" ] || miss "player 2 in room $7 at frame $1, not $3"
}
in_trap() {  # player (1 or 2), first frame, last frame: a miss unless some both.tsv row has them in 168 inside TrapRgn04's x/y
    awk -F'\t' -v w="$1" -v a="$2" -v b="$3" 'NR == 1 { for (i = 1; i <= NF; i++) c[$i] = i; next }
        { f = $c["frame"] + 0; x = $c["p" w "x"] + 0; y = $c["p" w "y"] + 0
          if (f >= a + 0 && f <= b + 0 && $c["p" w "room"] + 0 == 168 && x >= -375 && x <= -77 && y >= -1136 && y <= -944) n++ }
        END { exit n ? 0 : 1 }' "$R/both.tsv" 2>/dev/null ||
        miss "player $1 never inside TrapRgn04 (room 168, x -375..-77, y -1136..-944) in both.tsv frames $2..$3"
}
look() {  # run folder name, the newest before it: every evidence line, the counts, hang reports made since it started
    N=$1
    fresh "$1" "$2" || { echo "[traps] $N: no new run folder (newest is $(last "$1"))"; MISS="$MISS $N"; return 1; }
    echo "[traps] $N: $R"
    grep -h "$KEYS" "$R"/page-*.log 2>/dev/null
    echo "[traps] $N: runs of 2105 by lr and who (one 0x80134B38 who <A> per crossing after the fix):"
    grep -h '\[trace\] script run r3 2105 ' "$R"/page-*.log 2>/dev/null |
        sed -n 's/.* lr \(0x[0-9A-Fa-f]*\) who \([0-9]*\) .*/  lr \1 who \2/p' | sort | uniq -c
    echo "[traps] $N: ed156 records by room in play, turn and id (only on the STEP 0 and STEP 1 builds):"
    grep -h 'ed156 record: room in play' "$R"/page-*.log 2>/dev/null |
        sed -n "s/.*room in play \([0-9-]*\).*the player's id \([0-9]*\), turn \([0-9-]*\).*/  room \1 turn \3 id \2/p" | sort | uniq -c
    echo "[traps] $N: passed over (only after the fix): $(grep -h 'zone script 2105' "$R"/page-*.log 2>/dev/null | wc -l)"
    T5=$(grep -h '\[trace\] script run r3 2105 ' "$R"/page-*.log 2>/dev/null | wc -l)
    E6=$(grep -h 'ed156 record: room in play' "$R"/page-*.log 2>/dev/null | wc -l)
    if [ "$T5" -eq 0 ]; then
        echo "[traps] $N: no 2105 run at all: the put or the step missed the box, or the game's scan did not run"
    elif [ "$E6" -eq 0 ]; then
        echo "[traps] $N: 2105 ran $T5 times with no ed156 record: flag 555 is on (2105 skips ed156), or this build has no STEP 0 notify"
    fi
    H=$(find saves/reports -maxdepth 1 -name 'hang-*.txt' -newer "$R/env.txt" 2>/dev/null)
    if [ -n "$H" ]; then echo "[traps] $N: HANG REPORT $H"; else echo "[traps] $N: no hang report"; fi
}
put_and_step() {  # player, put frame, step's last frame
    has "\[sweep\] player $1 put at" || miss "no '[sweep] player $1 put at' line (the put did not land)"
    in_trap "$1" "$2" "$3"
}
need_state() {  # the fresh state and its env, made on this build (s0 writes S only when its checks held)
    [ -f "$ENVF" ] && . "./$ENVF"
    [ -n "$S" ] && [ -n "$SET" ] && [ -f "$SET" ] && [ -f "$STATE" ] || { echo "[traps] run s0 first (no state, or no S in $ENVF: s0 not passed)"; return 1; }
    if [ build-win/EternalDarkness.exe -nt "$STATE" ]; then
        echo "[traps] $STATE is older than build-win/EternalDarkness.exe: run probe and s0 again on this build"; return 1
    fi
}
need_f() {  # control's frame, measured on this build
    [ -f "$ENVF" ] && . "./$ENVF"
    [ -n "$F" ] || { echo "[traps] run probe first (no control frame in $ENVF)"; return 1; }
    if [ build-win/EternalDarkness.exe -nt "$ENVF" ]; then
        echo "[traps] $ENVF's F=$F is older than build-win/EternalDarkness.exe: run probe again on this build"; return 1
    fi
}

run_one() {
F=; S=; SET=; N=$1
case "$1" in
probe)  # chapter 6's control frame, from jtg_report.py's "control at frame F, room R:" in the run's jtg-report.txt
    B=$(last jtg-trap-probe)
    env -u ED_COOP_STAY -u ED_COOP_STAY_ASKED ED_ROOM_SLOTS=2 JTG_FRAMES=1500 sh tools/ed/jtg.sh 6 trap-probe </dev/null
    fresh jtg-trap-probe "$B" || { echo "[traps] probe: no new run folder"; return 1; }
    F=$(sed -n 's/^control at frame \([0-9]*\), room .*/\1/p' "$R/jtg-report.txt" 2>/dev/null | head -1)
    ROOM=$(sed -n 's/^control at frame [0-9]*, room \([0-9]*\):.*/\1/p' "$R/jtg-report.txt" 2>/dev/null | head -1)
    [ -n "$F" ] || F=$(python -I tools/ed/jtg_report.py --wait "$R" 30)
    [ -n "$F" ] || { echo "[traps] probe: no control frame in $R"; return 1; }
    echo "[traps] probe: $R"
    grep -h '^control at frame' "$R/jtg-report.txt" 2>/dev/null
    # (jtg.sh warns that a loaded machine can land in the wrong chapter: chapter 6 starts in room 303.)
    [ -z "$ROOM" ] || [ "$ROOM" = 303 ] || { echo "[traps] probe: control came in room $ROOM, not chapter 6's 303: F not kept"; return 1; }
    rm -f "$ENVF"; printf 'F=%s\n' "$F" > "$ENVF"
    echo "[traps] chapter 6: control at frame $F (kept in $ENVF)" ;;
s0)     # follow mode: player 1 and the bring into 168, the state saved with both idle
    need_f || return 1
    S=$((F + ${S0_AFTER:-400}))
    rm -f "$STATE"; printf 'F=%s\n' "$F" > "$ENVF"   # (no S until this s0 passes: t, a and b refuse)
    B=$(last jtg-trap-s0)
    env -u ED_COOP_STAY -u ED_COOP_STAY_ASKED ED_ROOM_SLOTS=2 JTG_BASE=$((F + 30)) ED_RUN_SCRIPT="$((F + 60)):1467" \
        ED_SAVE_STATE="$S:$STATE" ED_RUN_FRAMES=$((S + 100)) sh tools/ed/jtg.sh 6 trap-s0 </dev/null
    fresh jtg-trap-s0 "$B" || { echo "[traps] s0: no new run folder"; return 1; }
    echo "[traps] s0: $R (state $STATE at frame $S)"
    grep -h 'running script\|\[room\] change 0x\|player 2 stays in room\|saving a state' "$R"/page-*.log 2>/dev/null
    MISS0=$MISS
    [ -f "$STATE" ] || miss "no fresh state $STATE"
    has "saving a state to $STATE" || miss "no 'saving a state to $STATE' line"
    has 'running script 1467' || miss "no 'running script 1467' (F+60 came before the game would run it)"
    has '\[room\] change 0x000000A8' || miss "no '[room] change 0x000000A8' (168)"
    has 'player 2 stays in room' && miss "a '[stay] ... player 2 stays in room' line: the run was apart, not follow mode"
    rooms_at "$S" 168 168
    if [ "$MISS" != "$MISS0" ]; then echo "[traps] s0: not passed: no S kept (raise S0_AFTER if player 2 was not brought)"; return 1; fi
    printf "F=%s\nS=%s\nSET='%s'\n" "$F" "$S" "$R/settings.ini" > "$ENVF"
    echo "[traps] s0: passed (F, S and SET kept in $ENVF)" ;;
t)      # together, follow mode
    need_state || return 1
    B=$(last trap-t)
    env -u ED_COOP_STAY -u ED_COOP_STAY_ASKED ED_SETTINGS="$SET" ED_ROOM_SLOTS=2 ED_PUT="$((S + 60)),2,$TRAP" \
        ED_PAD2_SCRIPT="$((S + 62)),$((S + 68)),lup" ED_RUN_FRAMES=$((S + 400)) \
        sh tools/ed/run.sh "$STATE" trap-t </dev/null
    look trap-t "$B" || return 1
    has 'player 2 stays in room' && miss "a '[stay] ... player 2 stays in room' line: the run was apart, not follow mode"
    rooms_at "$((S + 62))" 168 168
    put_and_step 2 "$((S + 60))" "$((S + 68))" ;;
a)      # apart: player 1 in 139 (no record), player 2 into the trap in 168
    need_state || return 1
    B=$(last trap-a)
    ED_SETTINGS="$SET" ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_RUN_SCRIPT="60:1465" ED_PUT="$((S + 400)),2,$TRAP" \
        ED_PAD2_SCRIPT="$((S + 402)),$((S + 408)),lup" ED_RUN_FRAMES=$((S + 700)) \
        sh tools/ed/run.sh "$STATE" trap-a </dev/null
    look trap-a "$B" || return 1
    has 'running script 1465' || miss "no 'running script 1465'"
    has '\[room\] change 0x0000008B' || miss "no '[room] change 0x0000008B' (139)"
    has 'player 2 stays in room 168 .*went to room 139' || miss "no '[stay] player 2 stays in room 168 ... went to room 139'"
    rooms_at "$((S + 402))" 139 168
    put_and_step 2 "$((S + 400))" "$((S + 408))" ;;
b)      # apart: player 1 on to 313 (the wrong record), player 2 into the trap in 168
    need_state || return 1
    B=$(last trap-b)
    ED_SETTINGS="$SET" ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_RUN_SCRIPT="60:1465,400:1468" ED_PUT="$((S + 800)),2,$TRAP" \
        ED_PAD2_SCRIPT="$((S + 802)),$((S + 808)),lup" ED_RUN_FRAMES=$((S + 1100)) \
        sh tools/ed/run.sh "$STATE" trap-b </dev/null
    look trap-b "$B" || return 1
    has 'running script 1465' || miss "no 'running script 1465'"
    has 'running script 1468' || miss "no 'running script 1468'"
    has '\[room\] change 0x0000008B' || miss "no '[room] change 0x0000008B' (139)"
    has '\[room\] change 0x00000139' || miss "no '[room] change 0x00000139' (313)"
    has 'player 2 stays in room 168 .*went to room 139' || miss "no '[stay] player 2 stays in room 168 ... went to room 139'"
    has 'player 2 stays in room 168 .*went to room 313' || miss "no '[stay] player 2 stays in room 168 ... went to room 313'"
    rooms_at "$((S + 802))" 313 168
    put_and_step 2 "$((S + 800))" "$((S + 808))" ;;
solo)   # co-op off, a cold Jump to Game: player 1 into 168 and into the trap
    need_f || return 1
    B=$(last jtg-trap-solo)
    JTG_SET="mod_coop=0" JTG_BASE=$((F + 30)) ED_RUN_SCRIPT="$((F + 60)):1467" ED_PUT="$((F + 400)),1,$TRAP" \
        ED_INPUT_SCRIPT="$((F + 402)),$((F + 408)),0,0,127" ED_RUN_FRAMES=$((F + 700)) \
        sh tools/ed/jtg.sh 6 trap-solo </dev/null
    look jtg-trap-solo "$B" || return 1
    has 'running script 1467' || miss "no 'running script 1467' (F+60 came before solo's control)"
    has '\[room\] change 0x000000A8' || miss "no '[room] change 0x000000A8' (168)"
    rooms_at "$((F + 402))" 168 -
    put_and_step 1 "$((F + 400))" "$((F + 408))" ;;
*)  echo "[traps] no run named $1 (probe s0 t a b solo)" ;;
esac
}

MISS=
if [ $# -eq 0 ]; then set -- probe s0 t a b solo; fi
for r in "$@"; do
    if ! run_one "$r"; then
        case "$r" in probe|s0)
            echo "[traps] $r failed: the steps after it are not run"
            case " $MISS " in *" $r "*) ;; *) MISS="$MISS $r" ;; esac
            break ;;
        esac
    fi
done
if [ -n "$MISS" ]; then echo "[traps] NOT MET in:$MISS (their trap lines are not evidence)"; fi
echo RUNS DONE
