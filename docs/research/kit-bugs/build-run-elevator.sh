#!/bin/sh
# L04's elevator (rooms 189/190, chapter 4, Karim) and the zone scripts that sit only in the zones' fields for anyone
# but the player: docs/research/kit-bugs/plans.md section 4 (its corrected test plan and the skeptic's notes are what
# this follows). Nothing here has been run; `sh -n` only. What only a run can settle is under LEFT FOR A RUN below.
#
# THE BUG. The game's own step (0x80133434, bl 0x80134A44 at 0x801335C4) scans player 2's object as "anyone else"
# (bit 1 of the zones' +104), so player 2 fires the other_* fields: 190 ElevatorRgn02 other_enter 2726 / other_leave
# 2301, 189 ElevatorRgn01 other_leave 2301, 189 MonsterCheckRgn00 other_enter 2738 (2726 also on 189's SKPlane02/03
# cross_b). They run with player 1 as the player and player 1's room in play: 2726 clears player 1's prompt slots 2/4
# and sets 143 (190) or 146 (189) to state 0 without re-asking; 2301/2738 set it to 0 and run it again. So
#   (a) together in 190, player 2 beside the lift (ElevatorRgn02) knocks out player 1's 143 EXAMINE: no ride;
#   (b) together in 189, player 2 in and out of MonsterCheckRgn00 / ElevatorRgn01 restarts 146;
#   (c) apart, player 2 in 189's MonsterCheckRgn00 runs 2738 with room 190 in play: 146 inside room 190, whose Yes
#       goes to 190/EntryMkr04, not down to 189.
# THE PLANNED FIX (plans.md section 4, "Fix plan (the skeptic's corrected version)"): in runtime/host/hle.c's
# coop_scan_player1, after `if (g_coop_p1_own_look) return;`, pass over player 2's object when the scan's caller is the
# step (lr COOP_STEP_SCAN_RAN 0x801335C8) and player 2 is the player (g_coop_p2_is_player), with mod_coop and g_coop_p2
# set; it prints "[coop] player 2's object passed over by the game's zone look for others (zones 0x...)" once per zones
# list under ED_COOP_LOG. ED_COOP_P2_OTHER_ZONES=1 (testing) keeps the old route on the fixed build.
#
# HOW. Run from Git Bash: sh docs/research/kit-bugs/build-run-elevator.sh [STEP...]
#   STEP: state ref lift mon189 apart solo; none given: all, in that order.
#   1. BEFORE the fix, on today's build: `state` first (a fresh chapter-4 state made on this build), then ref, lift,
#      mon189, apart, solo. Read ref's timing first (below) and set ELEV_SHIFT if it is late.
#   2. AFTER the fix is built: `state` again (the old state is thrown away: a fresh state on that build), then ref, lift,
#      mon189, apart, solo. The steps refuse to run from a state made on another build (build-win/EternalDarkness.exe's
#      time differs from the one logs/elev-ch4.env recorded).
#   3. Optional, on the fixed build: ELEV_OLD=1 (ED_COOP_P2_OTHER_ZONES=1, a name that exists only after the fix)
#      ELEV_TAG=old sh ... lift mon189 apart, which should look like step 1's runs again. Without ELEV_OLD the script
#      unsets ED_COOP_P2_OTHER_ZONES, so a stray export can't turn the old route on.
#   ELEV_TAG=before / after names the runs: logs/run-*-elev-STEP-TAG for ref, lift, mon189 and apart (run.sh),
#   logs/run-*-jtg-elev-solo-TAG for solo and logs/run-*-jtg-elev-state for the state (jtg.sh; the state has no tag).
#   ELEV_SHIFT=N moves every put and press, the frame budgets and the checks' frames (not the flag or the door scripts)
#   N frames later; ELEV_S=N saves the state at frame N (default 2450; the later steps take S from logs/elev-ch4.env, not
#   from ELEV_S); ELEV_SLOT picks the jtg.sh slot for the state and solo (default 1).
#
# CONVENTIONS (tools/ed/jtg.sh's header, tools/ed/run.sh, docs/research/separate-rooms/build-run-tome.sh / -fx.sh):
#   - ED_RUN_AWAY=1 for every run (the window off the desktop, muted). No `ed test`, no `ed chapters`, no old saves.
#     run.sh closes EVERY EternalDarkness.exe by name: one run at a time, never with the game open.
#   - The state: a cold Jump to Game (jtg.sh 4, JTG_P1=4 Karim, JTG_P2=2 Ellia, model 70, so 132 sends her to 339), in
#     follow mode (ED_ROOM_SLOTS=2, ED_COOP_STAY unset), both idle in room 338, saved at frame S = ELEV_S. Chapter 4's
#     control was measured at 2204 / 2233 (logs/run-20261115-175417-jtg-tome-s0-ch4, -175541-jtg-tome-state-ch4), so no
#     probe run; the step checks that control came at least 30 frames before S, in room 338.
#   - Runs from the state: tools/ed/run.sh with ED_SETTINGS=<the state run>/settings.ini. A state keeps its guest clock,
#     so ED_PUT / ED_INPUT_SCRIPT / ED_PAD2_SCRIPT are absolute: every frame below is relative to S (plans.md's frames
#     relative to the base) and is shifted to S + ELEV_SHIFT + n. ED_SET_FLAG and ED_RUN_SCRIPT count DELAY from the
#     first pad read after the load, so they are given as small delays (20, 50, 110), as plans.md's B+20/B+50/B+110.
#   - Follow mode = ED_ROOM_SLOTS=2 with ED_COOP_STAY unset; apart = ED_ROOM_SLOTS=2 and ED_COOP_STAY=1 (the skeptic:
#     with both unset the run takes the one-slot route and 144 brings player 2 along, never apart).
#   - A short stick step after every ED_PUT into a zone (the look runs as they move): player 1 0,0,40, player 2 lup.
#   - run.sh / jtg.sh set ED_COOP_TRACE=1, ED_ROOM_LOG=1 and ED_COOP_LOG=1 and run `ed check` on each run themselves.
#   - Solo is a cold Jump to Game (JTG_SET mod_coop=0), not from the state: a co-op state played alone parks player 2
#     (build-run-tome.sh's solo). Its puts and presses count from JTG_BASE = 2300 + ELEV_SHIFT (jtg.sh turns them into
#     absolute frames); its flag and door scripts are given as plans.md's B+20 / B+50 / B+110, but hle.c's
#     test_set_flag / test_run_script count those from the first pad read after boot (sda_ready), not frame 0, so they
#     land some frames after absolute B+n (13 in logs/run-20261115-151539-jtg-tome-state-ch5: ED_RUN_SCRIPT 1561 ran at
#     1574). Harmless with the puts at +700; the [test] lines print the real frames.
#   - The checks below the evidence print "[elev] WARNING: ..." when a step's precondition did not hold (the rooms the
#     players are in when a put lands, the arrival before the puts, flag 91 set before the lift, player 2 inside or
#     outside the zones after a put). A run with a WARNING says nothing about the bug: fix the timing and run it again.
#
# WHAT EACH STEP PRINTS (only page-*.log is grepped; the hle.c string each grep matches is noted at the greps below;
# room numbers in [room] lines are hex: 189 = 0xBD, 190 = 0xBE, 338 = 0x152, 339 = 0x153; [stay] lines are decimal):
#   Every step prints its evidence lines (all of them are kept in the run's elev-evidence.txt; the first 150 are
#   printed), then its checks. "[sweep] player N put at X Y Z" shows each put landed ("not put: no floor under" if one
#   didn't: the step's later reading is then void).
#   THE "PASSED OVER" LINE, after the fix: the planned fix prints it once each time the zones list it skips changes (a
#   static said_zones = r4, so once per room in a row, again on a return), and in every co-op step, not only lift: in
#   ref, lift, mon189 and apart expect one or more (338's list, then 190's or 189's), in solo none. Before the fix, none
#   (the string doesn't exist). Its count is not a pass or fail; only lift's, mon189's and apart's other lines are.
#   state  "saving a state to saves/elev-ch4.state", the control frame and room (338), and both players in room 338
#          at S (both.tsv; the step refuses the state otherwise). No "[coop] the zones' look not found" (if it prints,
#          the hook isn't installed and the fix does nothing).
#   ref    (the control, follow mode) player 1 to 190 by 144, put on the lift at +700, B at +950, A x3. Before AND
#          after: "[room] change 0x000000BE" by about +300..+450 (the step warns if it is later than +600 + ELEV_SHIFT
#          and names the ELEV_SHIFT to use: then run every step again); both players in 190 at +700; flag 91 set before
#          +700; "[trace] script run r3 143 ... lr 0x80134B38" after the push (player 1's enter), "[trace] script run r3
#          143 ... lr 0x8000704C" at about +950 (player 1's B), then "[room] change 0x000000BD" at about +1400..+1700
#          (printed again as "after player 1's B"). No 2726 / 2301 line. Player 2 stays at (400,-500) (CameraRgn01
#          only). After the fix also the "passed over" lines (above): not a failure.
#   lift   ref plus player 2 put at (150,0) at +850 (ElevatorRgn02, outside ElevatorRgn01). The step checks player 2 is
#          inside ElevatorRgn02 and outside ElevatorRgn01 at +860 and +950. BEFORE: "[trace] script run r3 2726 ... lr
#          0x80134B38" at about +850, no 143 at lr 0x8000704C after the B at +950, no change to 0xBD by the end: the
#          bug (a). AFTER: no 2726, the "passed over" lines, and ref's 143 run and change to 0xBD. If BEFORE shows no
#          2726 with the place check passed, the gate didn't make the scan see player 2 (cannot know before a run).
#   mon189 (log lines only, follow mode) both to 189 by 150 (the step checks both in 189 at +700); player 1 on the
#          lift; player 2 in and out at (150,-2300) (inside MonsterCheckRgn00 and ElevatorRgn01, outside CameraRgn03's
#          xy). The step checks player 2 inside both spheres at +860 / +1060 and outside both at +960 / +1160. BEFORE:
#          "script run r3 2738 ... lr 0x80134B38" at about +850 and +1050, "script run r3 2301 ... lr 0x80134B38" at
#          about +950 and +1150; a "script run r3 146 ... lr 0x80164488" after each only if player 1's own bit is set
#          there (inferred). AFTER: none of them, only the "passed over" lines. (ED_PUT casts down from z + 300, so a
#          put at z 0 can't land on CameraRgn03's raised floor at 553..745; p2z is printed only to show the floor.)
#   apart  (stay mode) player 1 to 190 by 144 alone; player 2 by their own doors 338 -> 339 -> 190 ("[stay] player
#          2's change to room 339 runs as theirs", "... to room 190 ...", then their 143 ride: "... to room 189 ...",
#          "[stay] player 2's change done: they are in room 189" by about +1900); the step checks player 2 in 338 at
#          +400, in 339 at +800, both in 190 at +1150, player 1 in 190 and player 2 in 189 at +2100 and +2350; if one
#          fails, read the [stay] frames and move the later items. Player 1 back on 190's lift at +2100; player 2 into
#          189's MonsterCheckRgn00 at +2350; player 1's B at +2450, A x3. The lines from +2350 on are printed again
#          ("from player 2's step in"), since 0xBE and 0xBD changes also come earlier (player 1's arrival at about
#          +290, player 2's own changes). BEFORE: "script run r3 2738 ... lr 0x80134B38" at about +2350, then "script
#          run r3 146 ... lr 0x80164488" (146 run in room 190), player 1's B running "script run r3 146 ... lr
#          0x8000704C", then "[room] change 0x000000BE" or a refused same-room change (cannot know which): bug (c).
#          AFTER: no 2738, "script run r3 143 ... lr 0x8000704C", "[room] change 0x000000BD" after +2450, and the last
#          both.tsv row with p1room = p2room = 189.
#   solo   (co-op off, cold Jump to Game) ref's presses. Before and after: the same 143 runs and change to 0xBD as ref,
#          and no "passed over" line. The step warns if jtg-report.txt says LATE (control after JTG_BASE).
#   Every step: no "ASSERT" / "Assertion (" line, no new saves/reports/hang-*.txt (printed as "NEW hang report" if one
#   appears) and `ed check` clean (run.sh / jtg.sh print it; it reads both.tsv and shots/ only, not the page logs).
#
# LEFT FOR A RUN (cannot know without one):
#   - apart loads the follow-mode state in stay mode (ED_COOP_STAY=1). build-run-tome.sh only did the reverse (stay
#     states loaded in follow), so this way round is untried. The +400 check catches it: player 2 in 190 there means
#     144 brought them along and stay mode didn't take; then apart needs its own state made with ED_COOP_STAY=1.
#   - Whether the game's "ASSERT(0)" / "Assertion (%s) failed in ..." text (main.dol's strings) reaches page-*.log: it
#     would come through hle_OSReport (hle.c, fputs to stdout) and run.sh / jtg.sh's 2>&1 into pagelog.py, if those
#     strings are printed by OSReport at all (inferred). No hle.c line prints ASSERT; `ed check` doesn't look for it.
#   - The rest of plans.md's "What only a run or a play can show" (the scan gate +584 <= 2, whether a put alone is
#     seen, the monster handles, the A count of the words, fn66 from 190 to 190 in case c).
#
# LEFT OUT (not reachable by this script, or not this script's):
#   - The optional L06 trap follow-up (plans.md risk 1: player 2 tripping an L06 TrapRgn together and apart after the
#     fix): its own run, with bug 3's script (build-run-traps.sh).
#   - Whether the prompts are drawn, whether Yes is lit first, and whether player 2's ride holds player 1 through its
#     cutscene: only the run's shots/ and `python -I tools/ed/ed.py timeline RUN` show those (look by eye).
#   - The probe run of the reader's first plan: control is measured; the state step checks it.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_RUN_AWAY=1 ED_RUN_SHOTS_MS=250
SLOT=${ELEV_SLOT:-1}
SHIFT=${ELEV_SHIFT:-0}
SAVE_AT=${ELEV_S:-2450}
ST=saves/elev-ch4.state
ENVF=logs/elev-ch4.env
SECS=900
OLD=${ELEV_OLD:+ED_COOP_P2_OTHER_ZONES=1}   # only after the fix: the name does not exist in hle.c today
[ -n "$ELEV_OLD" ] || unset ED_COOP_P2_OTHER_ZONES
TAG=${ELEV_TAG:+-$ELEV_TAG}

# The common pieces (plans.md section 4): delays from the first pad read after the load.
FLAG="ED_SET_FLAG=20:91"              # 143 asks only with flag 91 (the shrine risen); 2360 is then empty
TO190="ED_RUN_SCRIPT=50:144,110:144"  # 340's ladder script 144: the prompt, then fn66 to 190/EntryMkr02
TO189="ED_RUN_SCRIPT=50:150,110:150"  # 195's door script 150: the prompt, then fn66 to 189/EntryMkr02

# The evidence, as grep -E patterns, each with the hle.c string it matches:
#   hle.c:11768 "[trace] %s r3 %d r4 %d lr 0x%08X who %u ..." with what = "script run" (hle.c:20664, under ED_COOP_TRACE)
ZONES="\[trace\] script run r3 (2726|2738|2301|143|146|132|34) r4"
#   hle.c:11463 "[room] %s 0x%08X 0x%08X (frame %llu) ..." with what = "change" (hle.c:11627, under ED_ROOM_LOG)
ROOMS="\[room\] change 0x(000000B[DE]|0000015[23]) "
#   hle.c:14780 "[stay] player 2's change to room %d runs as theirs ..." and hle.c:14846 "[stay] player 2's change done:
#   they are in room %d ..."
STAYS="\[stay\] player 2's change"
#   hle.c:19886 "[test] frame %llu: setting flag %d (%s)" and hle.c:19986 "[test] frame %llu: running script %d"
TESTS="\[test\] frame [0-9]+: (setting flag 91|running script (144|150))"
#   hle.c:32955 "[coop] the zones' look not found: player 1 walking can take player 2's prompts"
HOOK="the zones' look not found"
#   ONLY AFTER THE FIX: "[coop] player 2's object passed over by the game's zone look for others (zones 0x%08X): ..."
PASSED="passed over by the game's zone look for others"
#   hle.c:17022 "[sweep] player %d not put: no floor under %.0f %.0f %.0f (frame %llu)" and hle.c:17030 "[sweep] player
#   %d put at %.0f %.0f %.0f (frame %llu)" (sweep_put, ED_PUT)
SWEEP="\[sweep\] player [12] (not put: no floor under|put at) "
#   NOT an hle.c string: the game's own text in generated/main.dol ("ASSERT(0)", "Assertion (%s) failed in \"%s\" on
#   line %d"), which would reach page-*.log through hle_OSReport (hle.c, fputs to stdout) and the 2>&1 into pagelog.py
#   if the game prints it by OSReport (inferred; LEFT FOR A RUN in the header). plans.md's "Every run: no ASSERT".
ASSERTS="ASSERT|Assertion \("

last_run() { ls -d logs/run-*-"$1" 2>/dev/null | tail -1; }
# A both.tsv column's value pair at the first row at or after an absolute frame: "p1room p2room" or "p2x p2y p2z".
rooms_at() {  # run, frame
    awk -F'\t' -v f="$2" 'NR == 1 { for (i = 1; i <= NF; i++) c[$i] = i; next }
        $c["frame"] + 0 >= f + 0 { print $c["p1room"], $c["p2room"]; exit }' "$1/both.tsv" 2>/dev/null
}
p2_at() {  # run, frame
    awk -F'\t' -v f="$2" 'NR == 1 { for (i = 1; i <= NF; i++) c[$i] = i; next }
        $c["frame"] + 0 >= f + 0 { print $c["p2x"], $c["p2y"], $c["p2z"]; exit }' "$1/both.tsv" 2>/dev/null
}
# A precondition: the players' rooms (decimal, both.tsv) at +N from a base; "-" = that player's room not checked.
want_rooms() {  # run, base, n, p1room, p2room, what a miss means
    got=$(rooms_at "$1" $(($2 + $3)))
    if [ -z "$got" ]; then echo "[elev] WARNING: no both.tsv row at +$3 (frame $(($2 + $3))): the run ended early"; return 1; fi
    p1=${got% *}; p2=${got#* }
    if { [ "$4" != - ] && [ "$p1" != "$4" ]; } || { [ "$5" != - ] && [ "$p2" != "$5" ]; }; then
        echo "[elev] WARNING: at +$3 player 1 is in room $p1 and player 2 in $p2, not $4 / $5: $6"; return 1
    fi
    echo "[elev] at +$3 player 1 is in room $p1 and player 2 in $p2, as wanted ($4 / $5)"
}
# The page-log lines matching a pattern whose last "frame N" is at or after an absolute frame (each hle.c line used
# here ends its frame as "frame N" or "(frame N)"; [test] lines start with it, and have no other).
lines_from() {  # run, pattern, frame
    grep -hE "$2" "$1"/page-*.log 2>/dev/null | awk -v f="$3" '{ n = split($0, w, "frame "); if (n > 1 && w[n] + 0 >= f + 0) print }'
}
frame_of() {  # run, pattern, frame: the first such line's frame, or nothing
    lines_from "$1" "$2" "$3" | head -1 | awk '{ n = split($0, w, "frame "); print w[n] + 0 }'
}
# The arrival the puts at +700 wait for: plans.md moves every put and press by the difference when the change comes
# after +600. The door scripts aren't shifted, so the change's frame from the base doesn't move with ELEV_SHIFT.
arrived_by() {  # run, base (S, or solo's B; no shift), hex room (000000BE), room
    A=$(frame_of "$1" "\[room\] change 0x$3 " "$2")
    if [ -z "$A" ]; then echo "[elev] WARNING: no \"[room] change 0x$3\" after the base: the puts at +700 land in the wrong room"; return 1; fi
    late=$((A - $2 - 600))
    if [ "$late" -gt "$SHIFT" ]; then
        echo "[elev] WARNING: the change to room $4 came at +$((A - $2)), after +600 + ELEV_SHIFT ($SHIFT): run every step again with ELEV_SHIFT=$late"
        return 1
    fi
    echo "[elev] the change to room $4 came at +$((A - $2)) (by +600 + ELEV_SHIFT $SHIFT: in time for the puts)"
}
# 143 asks only with flag 91: its [test] line must say "(set)" and come before the puts at +700.
flag_before() {  # run, absolute frame of the first put
    L=$(grep -hE "\[test\] frame [0-9]+: setting flag 91 " "$1"/page-*.log 2>/dev/null | head -1)
    Ff=$(printf '%s' "$L" | sed -n 's/^.*\[test\] frame \([0-9][0-9]*\):.*$/\1/p')
    case "$L" in
    *"(set)"*) if [ "$Ff" -lt "$2" ]; then echo "[elev] flag 91 set at frame $Ff, before the puts ($2)"; return 0; fi
               echo "[elev] WARNING: flag 91 set at frame $Ff, not before the puts ($2): 143 may not ask" ;;
    "")        echo "[elev] WARNING: no \"setting flag 91\" line: 143 won't ask" ;;
    *)         echo "[elev] WARNING: flag 91 not set: $L" ;;
    esac
    return 1
}
# Player 2 inside ElevatorRgn02 and outside ElevatorRgn01 (ed room 190: boxes [-208,-193,-534]..[178,185,215] and
# [-156,-139,-534]..[127,131,215]) at +N.
p2_lift_at() {  # run, base, n
    printf '%s\n' "$(p2_at "$1" $(($2 + $3)))" | awk -v n="$3" '
        NF < 3 { print "[elev] WARNING: no both.tsv row at +" n; exit }
        { x = $1 + 0; y = $2 + 0; z = $3 + 0
          in2 = x >= -208 && x <= 178 && y >= -193 && y <= 185 && z >= -534 && z <= 215
          in1 = x >= -156 && x <= 127 && y >= -139 && y <= 131 && z >= -534 && z <= 215
          if (in2 && !in1) print "[elev] at +" n " player 2 is at " $0 ": in ElevatorRgn02, outside ElevatorRgn01, as wanted"
          else print "[elev] WARNING: at +" n " player 2 is at " $0 ", not in ElevatorRgn02 alone: the put or the stick step missed it" }'
}
# Player 2 inside (want 1) or outside (want 0) both of 189's spheres (ed room 189: MonsterCheckRgn00 [33,-2240,47]
# r230, ElevatorRgn01 [33,-2347,47] r222; the distance in 3D) at +N.
p2_spheres_at() {  # run, base, n, want
    printf '%s\n' "$(p2_at "$1" $(($2 + $3)))" | awk -v n="$3" -v want="$4" '
        NF < 3 { print "[elev] WARNING: no both.tsv row at +" n; exit }
        { x = $1 + 0; y = $2 + 0; z = $3 + 0
          m = (x - 33) ^ 2 + (y + 2240) ^ 2 + (z - 47) ^ 2 <= 230 ^ 2
          e = (x - 33) ^ 2 + (y + 2347) ^ 2 + (z - 47) ^ 2 <= 222 ^ 2
          what = want == 1 ? "inside both spheres" : "outside both spheres"
          if ((want == 1 && m && e) || (want == 0 && !m && !e)) print "[elev] at +" n " player 2 is at " $0 ": " what ", as wanted"
          else print "[elev] WARNING: at +" n " player 2 is at " $0 ", not " what " (MonsterCheckRgn00 " m ", ElevatorRgn01 " e "): the put or the stick step missed" }'
}
# The lines that read the bug, from a frame on (player 1's B, or player 2's step in), printed again by themselves.
after() {  # run, frame, what it is
    echo "[elev] from $3 (frame $2): the 143 / 146 / 2726 / 2738 / 2301 runs and the room changes"
    lines_from "$1" "$ZONES|$ROOMS" "$2" | cut -c1-200 | head -40
}
newest_hang() { ls saves/reports/hang-*.txt 2>/dev/null | tail -1; }
# "control at frame F, room R: ..." from a run's jtg-report.txt (tools/ed/jtg_report.py) -> "F R" (R decimal), or nothing.
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
# A relative list ("frame,..." items, `;` between) -> absolute: the first $2 fields get S + ELEV_SHIFT added.
abs_list() {
    printf '%s' "$1" | tr ';' '\n' | awk -F, -v b="$((S + SHIFT))" -v n="$2" 'NF { for (i = 1; i <= n; i++) $i += b; OFS = ","; $1 = $1; print }' | paste -sd ';' -
}
# both.tsv rows at (the first row at or after) each absolute frame given, then its last row: the players' rooms and places.
places() {  # run, frames...
    R=$1; shift
    awk -F'\t' -v want="$*" '
        NR == 1 { for (i = 1; i <= NF; i++) c[$i] = i; n = split(want, w, " "); k = 1
                  print "[elev] both.tsv: frame p1room p2room p1x p1y p1z p2x p2y p2z"; next }
        { row = $c["frame"] " " $c["p1room"] " " $c["p2room"] " " $c["p1x"] " " $c["p1y"] " " $c["p1z"] " " $c["p2x"] " " $c["p2y"] " " $c["p2z"]
          while (k <= n && $c["frame"] + 0 >= w[k] + 0) { print "[elev]   at " w[k] ": " row; k++ } }
        END { print "[elev]   last: " row }' "$R/both.tsv" 2>/dev/null
}
# The evidence lines of a run (all of them into the run's elev-evidence.txt, the first 150 printed), then its new hang
# report if any.
evidence() {  # run, hang report before it
    R=$1
    echo "[elev] $R"
    grep -hE -e "$ZONES" -e "$ROOMS" -e "$STAYS" -e "$TESTS" -e "$HOOK" -e "$PASSED" -e "$SWEEP" -e "$ASSERTS" \
        "$R"/page-*.log 2>/dev/null | cut -c1-260 > "$R/elev-evidence.txt"
    n=$(($(wc -l < "$R/elev-evidence.txt")))
    head -150 "$R/elev-evidence.txt"
    [ "$n" -gt 150 ] && echo "[elev] ... and $((n - 150)) more lines: $R/elev-evidence.txt"
    A=$(grep -chE "$ASSERTS" "$R"/page-*.log 2>/dev/null | awk '{ s += $1 } END { print s + 0 }')
    [ "$A" -gt 0 ] && echo "[elev] WARNING: $A ASSERT / Assertion line(s) in the page logs"
    H=$(newest_hang)
    [ -n "$H" ] && [ "$H" != "$2" ] && echo "[elev] NEW hang report: $H"
    return 0
}
load_env() {
    [ -f "$ENVF" ] && [ -f "$ST" ] || { echo "[elev] make the state first (STEP state)"; return 1; }
    . "./$ENVF"
    NOW=$(stat -c %Y build-win/EternalDarkness.exe 2>/dev/null)
    [ "$NOW" = "$BUILT" ] || { echo "[elev] the build changed since the state was made: run STEP state again on this build"; return 1; }
}
from_state() {  # name, then the env assignments for the run
    name="elev-$1$TAG"; shift
    H0=$(newest_hang)
    R0=$(last_run "$name")
    env ED_SETTINGS="$SET" ED_RUN_FRAMES="$FR" ED_RUN_SECONDS=$SECS $OLD "$@" sh tools/ed/run.sh "$ST" "$name" </dev/null
    R=$(last_run "$name")
    if [ -z "$R" ] || [ "$R" = "$R0" ]; then echo "[elev] no new run folder for $name (the newest is still ${R0:-none})"; return 1; fi
    evidence "$R" "$H0"
}

# ---- The state: a cold Jump to Game, follow mode, both idle in room 338, saved at S. Writes $ST and $ENVF (S, the
# settings to load it with, the build's time).
make_state() {
    rm -f "$ST" "$ENVF"
    H0=$(newest_hang)
    RS0=$(last_run jtg-elev-state)
    env -u ED_COOP_STAY JTG_SLOT=$SLOT JTG_P1=4 JTG_P2=2 ED_ROOM_SLOTS=2 ED_RUN_SECONDS=$SECS \
        ED_SAVE_STATE="$SAVE_AT:$ST" ED_RUN_FRAMES=$((SAVE_AT + 60)) sh tools/ed/jtg.sh 4 elev-state </dev/null
    RS=$(last_run jtg-elev-state)
    if [ -z "$RS" ] || [ "$RS" = "$RS0" ]; then echo "[elev] no new run folder for the state"; rm -f "$ST"; return 1; fi
    #   hle.c:19921 "[test] frame %llu: saving a state to %s"; hle.c:11463/11627 "[room] %s 0x%08X" with "change"
    # (the last 5: the boot's room changes come first; the hook line is grepped by itself, since it prints at boot)
    grep -hE -e "\[test\] frame [0-9]+: saving a state to" -e "\[room\] change 0x" "$RS"/page-*.log 2>/dev/null | cut -c1-200 | tail -5
    grep -hE -e "$HOOK" -e "$ASSERTS" "$RS"/page-*.log 2>/dev/null | cut -c1-200 | sed 's/^/[elev] WARNING: /' | head -10
    H=$(newest_hang); [ -n "$H" ] && [ "$H" != "$H0" ] && echo "[elev] NEW hang report: $H"
    [ -f "$ST" ] || { echo "[elev] no state saved ($RS)"; return 1; }
    CR=$(control_of "$RS") || { echo "[elev] no control in $RS"; rm -f "$ST"; return 1; }
    F=${CR% *}; ROOM=${CR#* }
    echo "[elev] control at frame $F in room $ROOM (want 338, at least 30 frames before $SAVE_AT)"
    if [ "$ROOM" != 338 ] || [ $((F + 30)) -gt "$SAVE_AT" ]; then
        echo "[elev] the state is not idle in room 338: raise ELEV_S and run STEP state again"; rm -f "$ST"; return 1
    fi
    places "$RS" "$SAVE_AT"
    want_rooms "$RS" 0 "$SAVE_AT" 338 338 "both must be in 338 when the state is saved" || { rm -f "$ST"; return 1; }
    printf 'S=%s\nSET=%s\nBUILT=%s\n' "$SAVE_AT" "$RS/settings.ini" "$(stat -c %Y build-win/EternalDarkness.exe 2>/dev/null)" > "$ENVF"
    echo "[elev] $ST at frame $SAVE_AT (from $RS)"
}

# Relative lists (frames from S, as plans.md's from the base).
P1_RIDE="720,728,0,0,40;950,956,0x0200;1050,1056,0x0100;1110,1116,0x0100;1170,1176,0x0100"  # push, B on 143, A x3
PUT_RIDE="700,1,0,0,0;700,2,400,-500,0"   # player 1 in ElevatorRgn01/02 and Region01; player 2 in CameraRgn01 only

run_one() {
case "$1" in
state)  make_state ;;
ref)    # The control (follow mode): player 1 rides 190 -> 189 with player 2 off the lift.
        load_env || return
        FR=$((S + SHIFT + 1900))
        from_state ref env -u ED_COOP_STAY ED_ROOM_SLOTS=2 "$FLAG" "$TO190" \
            ED_PUT="$(abs_list "$PUT_RIDE" 1)" ED_INPUT_SCRIPT="$(abs_list "$P1_RIDE" 2)" \
            ED_PAD2_SCRIPT="$(abs_list "720,724,lup" 2)" || return
        places "$R" $((S + SHIFT + 700)) $((S + SHIFT + 950))
        arrived_by "$R" "$S" 000000BE 190
        flag_before "$R" $((S + SHIFT + 700))
        want_rooms "$R" $((S + SHIFT)) 700 190 190 "the puts landed in another room (follow mode: both should be in 190)"
        after "$R" $((S + SHIFT + 950)) "player 1's B at +950" ;;
lift)   # Bug (a): ref plus player 2 put into ElevatorRgn02 (150,0) at +850, a stick step, then player 1's B at +950.
        load_env || return
        FR=$((S + SHIFT + 1900))
        from_state lift env -u ED_COOP_STAY ED_ROOM_SLOTS=2 "$FLAG" "$TO190" \
            ED_PUT="$(abs_list "$PUT_RIDE;850,2,150,0,0" 1)" ED_INPUT_SCRIPT="$(abs_list "$P1_RIDE" 2)" \
            ED_PAD2_SCRIPT="$(abs_list "720,724,lup;852,856,lup" 2)" || return
        places "$R" $((S + SHIFT + 700)) $((S + SHIFT + 860)) $((S + SHIFT + 950))
        arrived_by "$R" "$S" 000000BE 190
        flag_before "$R" $((S + SHIFT + 700))
        want_rooms "$R" $((S + SHIFT)) 700 190 190 "the puts landed in another room (follow mode: both should be in 190)"
        p2_lift_at "$R" $((S + SHIFT)) 860
        p2_lift_at "$R" $((S + SHIFT)) 950
        after "$R" $((S + SHIFT + 850)) "player 2's put at +850" ;;
mon189) # Bug (b), log lines only (follow mode): both to 189; player 2 in and out of MonsterCheckRgn00 / ElevatorRgn01.
        load_env || return
        FR=$((S + SHIFT + 1300))
        from_state mon189 env -u ED_COOP_STAY ED_ROOM_SLOTS=2 "$TO189" \
            ED_PUT="$(abs_list "700,1,33,-2347,0;700,2,33,-1800,0;850,2,150,-2300,0;950,2,33,-1800,0;1050,2,150,-2300,0;1150,2,33,-1800,0" 1)" \
            ED_INPUT_SCRIPT="$(abs_list "720,728,0,0,40" 2)" \
            ED_PAD2_SCRIPT="$(abs_list "720,724,lup;852,856,lup;952,956,lup;1052,1056,lup;1152,1156,lup" 2)" || return
        places "$R" $((S + SHIFT + 700)) $((S + SHIFT + 860)) $((S + SHIFT + 960)) $((S + SHIFT + 1060)) $((S + SHIFT + 1160))
        arrived_by "$R" "$S" 000000BD 189
        want_rooms "$R" $((S + SHIFT)) 700 189 189 "the puts landed in another room (follow mode: both should be in 189)"
        p2_spheres_at "$R" $((S + SHIFT)) 860 1
        p2_spheres_at "$R" $((S + SHIFT)) 960 0
        p2_spheres_at "$R" $((S + SHIFT)) 1060 1
        p2_spheres_at "$R" $((S + SHIFT)) 1160 0
        after "$R" $((S + SHIFT + 850)) "player 2's first step in at +850" ;;
apart)  # Bug (c), stay mode: player 1 to 190 alone by 144; player 2 by their own doors 338 (132) -> 339 (34) -> 190,
        # then down to 189 on 143 (B, A x3); player 1 back on 190's lift; player 2 into 189's MonsterCheckRgn00 at
        # +2350; player 1's B at +2450, A x3.
        load_env || return
        FR=$((S + SHIFT + 3300))
        from_state apart ED_ROOM_SLOTS=2 ED_COOP_STAY=1 "$FLAG" "$TO190" \
            ED_PUT="$(abs_list "400,2,-940,-85,0;800,2,-248,1377,0;1150,1,300,-400,0;1150,2,0,0,0;2100,1,0,0,0;2250,2,33,-1800,0;2350,2,150,-2300,0" 1)" \
            ED_PAD2_SCRIPT="$(abs_list "420,428,lup;500,506,b;820,828,lup;900,906,b;1170,1176,lup;1250,1256,b;1350,1356,a;1410,1416,a;1470,1476,a;2252,2256,lup;2352,2356,lup" 2)" \
            ED_INPUT_SCRIPT="$(abs_list "1152,1156,0,0,40;2120,2128,0,0,40;2450,2456,0x0200;2550,2556,0x0100;2610,2616,0x0100;2670,2676,0x0100" 2)" || return
        places "$R" $((S + SHIFT + 400)) $((S + SHIFT + 800)) $((S + SHIFT + 1150)) $((S + SHIFT + 1900)) $((S + SHIFT + 2100)) \
            $((S + SHIFT + 2360)) $((S + SHIFT + 2450))
        arrived_by "$R" "$S" 000000BE 190
        flag_before "$R" $((S + SHIFT + 1150))
        want_rooms "$R" $((S + SHIFT)) 400 - 338 "player 2 isn't in 338 for their door (in 190: 144 brought them along, stay mode didn't take from the follow-mode state)"
        want_rooms "$R" $((S + SHIFT)) 800 - 339 "player 2's door 132 (B at +500) didn't take them to 339 in time"
        want_rooms "$R" $((S + SHIFT)) 1150 190 190 "player 2's door 34 (B at +900) didn't take them to 190 in time"
        want_rooms "$R" $((S + SHIFT)) 2100 190 189 "player 2's 143 ride (B at +1250) didn't take them down to 189 in time"
        want_rooms "$R" $((S + SHIFT)) 2350 190 189 "the players aren't apart in 190 / 189 for player 2's step in"
        p2_spheres_at "$R" $((S + SHIFT)) 2360 1
        after "$R" $((S + SHIFT + 2350)) "player 2's step in at +2350" ;;
solo)   # Co-op off, a cold Jump to Game (a co-op state played alone parks player 2): ref's put and presses.
        # jtg.sh's chapter-4 base. ED_SET_FLAG / ED_RUN_SCRIPT count from the first pad read after boot (sda_ready),
        # so they land some frames after absolute B+20 / B+50 / B+110 (the header's CONVENTIONS); the [test] lines say.
        B=2300
        H0=$(newest_hang)
        R0=$(last_run "jtg-elev-solo$TAG")
        env -u ED_COOP_STAY JTG_SLOT=$SLOT JTG_SET="mod_coop=0" JTG_P1=4 JTG_P2=2 ED_ROOM_SLOTS=2 ED_RUN_SECONDS=$SECS \
            JTG_BASE=$((B + SHIFT)) JTG_FRAMES=1900 ED_SET_FLAG="$((B + 20)):91" ED_RUN_SCRIPT="$((B + 50)):144,$((B + 110)):144" \
            JTG_PUT="700,1,0,0,0" JTG_P1_SCRIPT="$P1_RIDE" sh tools/ed/jtg.sh 4 "elev-solo$TAG" </dev/null
        R=$(last_run "jtg-elev-solo$TAG")
        if [ -z "$R" ] || [ "$R" = "$R0" ]; then echo "[elev] no new run folder for elev-solo$TAG"; return 1; fi
        evidence "$R" "$H0"
        #   tools/ed/jtg_report.py:108 "base frame %d: control came %+d frames from it" with "  << LATE: ..." when late
        grep -hE "^(control at frame|control never came|base frame)" "$R/jtg-report.txt" 2>/dev/null
        if grep -q "<< LATE" "$R/jtg-report.txt" 2>/dev/null; then
            echo "[elev] WARNING: jtg-report.txt says LATE: control came after JTG_BASE $((B + SHIFT)) and presses were lost; raise ELEV_SHIFT"
        elif ! grep -q "^control at frame" "$R/jtg-report.txt" 2>/dev/null; then
            echo "[elev] WARNING: no control line in $R/jtg-report.txt"
        fi
        places "$R" $((B + SHIFT + 700)) $((B + SHIFT + 950))
        arrived_by "$R" "$B" 000000BE 190
        flag_before "$R" $((B + SHIFT + 700))
        want_rooms "$R" $((B + SHIFT)) 700 190 - "player 1's put landed in another room"
        after "$R" $((B + SHIFT + 950)) "player 1's B at +950" ;;
*)      echo "[elev] no step named $1" ;;
esac
}

if [ $# -eq 0 ]; then set -- state ref lift mon189 apart solo; fi
for r in "$@"; do run_one "$r"; done
echo RUNS DONE
