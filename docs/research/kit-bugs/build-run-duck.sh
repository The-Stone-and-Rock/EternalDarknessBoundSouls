#!/bin/sh
# fn151's music duck by player 2 apart (docs/research/kit-bugs/plans.md, section 1: the corrected test plan and the
# skeptic's notes there are what this follows). Syntax-checked only; nothing here has been run.
#
# THE BUG. coop_sound_change_act (runtime/host/hle.c:15084) passes over fn151's duck when player 2 is in another room,
# but it tests `cpu->lr == 0x80164DD4u`, the address of fn151's `bl 0x8019E068` itself. The intercept sees the return
# address 0x80164DD8, so the test never matches: player 2's fn151(1) apart runs 0x8019E068(0, 25, 10), which ducks the
# one global sound group 0, and player 1's music and effects drop to 25 for player 2's scene.
# THE PLANNED FIX (not made here; never edit runtime/host/* from this script): hle.c:15084 0x80164DD4u -> 0x80164DD8u, and
# the comment at hle.c:15045 the same. Gating is unchanged (coop_stay(), g_coop_p2, sda_ready, r4 == 25,
# coop_music_theirs, coop_rooms_apart), so solo, follow mode and players together are untouched.
#
# WHAT IT TESTS. Chapter 10 (Edward, level 09; Jump to Game grid 10). Control comes in room 237 (0xED): the past chapter-10
# jtg reports (logs/run-20261106-002427-jtg-ver-p2exit-ch10 and the others) all say "control at frame 1546-1595, room
# 237", and tools/ed/coopplan.py CHAPTERS[10] and tools/ed/sweep.py START_ROOM[9] agree. (plans.md's "238" is the room of a
# hang, from docs/chapter-tour-review.md:21.) Room 273 (0x111), SphereRegion01 (centre 2145 1211 -186, radius 124; enter
# runs L09/1951). With flags 413 and 172 on, 1951's state 0 shows PULL_LEVER (button 2: b for player 2); the press runs
# states 3-7 (waits 75, 60, 200, 90, 30) and state 8 calls fn151(1), about 455 frames after the press (inferred; loads
# shift it). Door 754 (room 279's BoxRegion05, run here straight from 237) takes both to 273 (follow mode); door 732 is
# the way out to room 279 (0x117). Neither door asks anything (`ed door 273`: "asks nothing"; `ed script 754/732 --level
# 9` is only room_queue and go_to_room), so each runs ONCE: tools/ed/chapters.py:108 runs a door a second time only when it
# asks, and a second run here would land inside the first one's change and could queue a second change.
# Room 273's arrival script L09/1626 makes the force field (barrier_make, ShldMkr08/05/03/06/07/02) only when flag 413 is
# on AT THE ARRIVAL, and 1951's state 6 removes it. So the state step sets 413 at F+30, before 754 at F+60, as
# tools/ed/jtg.sh's header says for ED_SET_FLAG ("a door's flag, before its ED_RUN_SCRIPT"); plans.md doesn't cover this.
#
# STEPS (names as arguments; none given: probe state apart together static, in that order):
#   probe     Chapter 10 from a cold Jump to Game with no presses: the control frame F and its room (read by python from the
#             run's jtg-report.txt, "control at frame F, room R"). Expect room 237. It closes at jtg.sh's default base for
#             chapter 10 (2300) + JTG_FRAMES=300, well past the control seen before; it has no presses, so a "LATE" in its
#             report means nothing. The state step uses this F only when the probe is newer than
#             build-win/EternalDarkness.exe; DUCK_F=N overrides it (by hand: nothing checks N).
#   state     A FRESH state on the current build: follow mode (ED_ROOM_SLOTS=2, ED_COOP_STAY unset), flag 413 at F+30, door
#             754 once at F+60, saved at S=F+900 (the door's dissolve, countdown and arrival, then a rest) to
#             saves/duck-ch10-273.state; S, F and the run's settings.ini go to logs/duck-ch10.env. Prints the flag, the
#             754 run, the [room] change lines (0x00000111 = 273 is the arrival) and the save line; warns when 413 was not
#             set before 754 ran; then the last both.tsv row's p1room / p2room (decimal): both must be 273, or the state is
#             removed and the step fails.
#   apart     From the state, the stay on (ED_ROOM_SLOTS=2, ED_COOP_STAY=1), ED_SETTINGS=<the state run>/settings.ini:
#             flags 413 (on already from the state; set again for the log line) and 172 set, player 1 out by 732 (once,
#             100 frames after the load) to room 279, player 2 stays; PUT=S+900 (DUCK_PUT=N moves it) player 2 put in the
#             sphere, PUT+2..PUT+8 a short stick step (lup), PUT+60 b on PULL_LEVER; it closes 1440 frames after the press.
#             Warns when player 1's change to 279 did not come before PUT.
#   together  The same with no 732: player 1 stays in 273; PUT=S+400. The negative check.
#   static    No game: hle.c's fn151 test and hooks.py's return-address check.
# Each game step refuses to read a run unless a new run dir appeared (never an older run's logs), and warns when ED_PUT
# put nobody or 1951 never ran for player 2.
#
# ORDER. Before the fix, on the current build: probe state apart together static. Then make the fix, rebuild, and run
# all five again: the state step refuses a probe older than the build and makes a new state on the new build (apart and
# together refuse a state older than build-win/EternalDarkness.exe).
#
# ONLY A RUN CAN SHOW: whether 413 set before 754 changes anything else on the way (other readers of 413 in level 09
# were not looked up), whether 754 run from 237 lands both in 273, and the rest of plans.md's "Not knowable" list.
#
# WHAT EACH STEP'S OUTPUT MEANS (every grep string below is an fprintf of runtime/host/hle.c today, except where a
# comment names its source or says "only after the fix"):
#   "[test] frame N: setting flag 413 (set)" / "... flag 172 (set)" (hle.c:19886): the flags went on.
#   "[test] frame N: running script 732" once (hle.c:19986), then "[room] change 0x00000117 ..." (room_log, hle.c:11463,
#     rooms in hex): player 1 went to 279. Apart only; it must come before PUT (the step warns), or run again with
#     DUCK_PUT later.
#   "[sweep] player 2 put at 2145 1211 Z (frame PUT)" (hle.c:17030). "[sweep] player 2 not put: no floor under ..."
#     (hle.c:17022) means change z.
#   "[coop] script 1951 runs for player 2 (frame ~PUT+60)" (hle.c:20137's "script %d runs for player %d") and then several
#     "[coop] script 1951 goes on as player 2's (lr 0x...)" (hle.c:20772), one per resumed wait, the last about
#     press+455: these prove state 8 (the duck) ran. None: the script stopped at state 0 (its `L0 ~= 1`, player 2's id)
#     and the scripted test can't reach the duck; then only the user's play of the clock (below) tests it.
#   "[sound] a duck (fn151) by player 2 apart: passed over, player 1's music stays (frame N)" (hle.c:15086), counted:
#     apart before the fix: 0 although the 1951 lines show state 8 ran. That is the bug (no log records each 0x8019E068
#       call, so the duck going through shows only as this missing line).
#     apart after the fix: exactly 1, N about press+455.
#     together, before and after: 0.
#   The newest saves/reports/hang-*.txt and the count of them, before and after each run: no new one. (The plan's ASSERT
#     grep is left out: no ASSERT string is in hle.c.)
#   static: before the fix, hle.c:15084 shows 0x80164DD4u and hooks.py prints "  CAN'T MATCH: hle.c:15084 ... tests lr
#     == 0x80164DD4u ..."; after the fix, 0x80164DD8u at 15084 and no CAN'T MATCH line for 15084.
#
# NOT REACHABLE BY SCRIPT, so left out:
#   - Hearing the duck: ED_RUN_AWAY=1 mutes the window, and no log records each 0x8019E068 call.
#   - The fallback, the user's own play of the grandfather clock (L00/L12 1551, room 78, the third try, AlexClockTry >= 2;
#     no door into room 78 is known in level 12). The user, from cmd: `set ED_COOP_LOG=1`, then
#     `play-rooms-log.bat duck-clock` (it gives ED_ROOM_SLOTS=2 and ED_COOP_STAY=1, not ED_COOP_LOG); player 2 in the
#     study, player 1 in the main hall; grep that run's page-*.log for "a duck (fn151)" and "script 1551". Before the
#     fix: no line, and player 1's music drops in player 2's clock scene; after: one line and the music holds.
#   - L04/1381 is no alternative: it picks by the room in play (fn25() == 193), which apart is player 1's.
#
# Run from Git Bash: sh docs/research/kit-bugs/build-run-duck.sh [STEP...]. Never with the user's game open: run.sh's
# ED_RUN_FRAMES closes every EternalDarkness.exe by name. Never `ed test` or `ed chapters` (old saves).
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_RUN_AWAY=1 ED_ROOM_SLOTS=2 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_RUN_SHOTS_MS=250
unset ED_COOP_STAY    # tested with getenv != NULL (hle.c:14688): only unsetting turns it off; each step sets its own
ST=saves/duck-ch10-273.state
ENVF=logs/duck-ch10.env

last_run() { ls -d logs/run-*-"$1" 2>/dev/null | tail -1; }
# After a run: R = the newest run dir named $1, refused when it is still $2 (the newest before the run), so a run that
# made no dir never has an older run's logs read as its own.
new_run() {
    R=$(last_run "$1")
    [ -n "$R" ] && [ "$R" != "$2" ] || { echo "[duck] no new $1 run (the newest is still ${2:-none})"; return 1; }
}
# The frame of the first page-*.log line in run $1 matching $2 ("[test] frame N: ...", "... (frame N) ..."), or nothing.
frame_of() { grep -h "$2" "$1"/page-*.log 2>/dev/null | head -1 | sed -n 's/.*frame \([0-9][0-9]*\).*/\1/p'; }
# run.sh's wall-clock cap for frames $1 to $2: a third of real speed (20 frames a second) plus two minutes for the load.
secs() { echo $((($2 - $1) / 20 + 120)); }
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
# The last both.tsv row's frame, p1room and p2room (decimal), by the header's names. With $2 and $3: a warning, and
# status 1, unless player 1 is in room $2 and player 2 in room $3 there (also when there is no both.tsv).
rooms_at_end() {
    awk -F'\t' -v w1="$2" -v w2="$3" 'NR == 1 { for (i = 1; i <= NF; i++) c[$i] = i; next }
        { f = $c["frame"]; a = $c["p1room"]; b = $c["p2room"] }
        END { printf "[duck] last both.tsv row: frame %s, p1room %s, p2room %s\n", f, a, b
              if (w1 != "" && (a != w1 || b != w2)) { printf "[duck] WARNING: expected p1room %s, p2room %s\n", w1, w2; exit 1 } }' \
        "$1/both.tsv" 2>/dev/null || { [ -f "$1/both.tsv" ] || echo "[duck] WARNING: no $1/both.tsv"; return 1; }
}
hangs() { n=$(ls saves/reports/hang-*.txt 2>/dev/null | wc -l); echo "[duck] hang reports: $n, newest: $(ls saves/reports/hang-*.txt 2>/dev/null | tail -1)"; }
load_env() {
    [ -f "$ST" ] && [ -f "$ENVF" ] || { echo "[duck] run the state step first"; return 1; }
    [ build-win/EternalDarkness.exe -nt "$ST" ] && { echo "[duck] the build is newer than $ST: run the state step again"; return 1; }
    . "./$ENVF"
}
# The evidence lines of an apart or together run $1; $2 $3 are the rooms player 1 and player 2 should end in.
evidence() {
    R=$1
    # Not cut by head: the 1951 lines (before the fix, the only proof that state 8 ran) come late, and under ED_COOP_LOG
    # every [sound] line would come before them (46 in logs/run-20261111-182644-rooms-r5-s1-p2door), so those go apart.
    grep -h "setting flag\|running script 732\|\[room\] change\|\[sweep\] player 2\|script 1951 runs for player 2\|script 1951 goes on as player 2's" \
        "$R"/page-*.log 2>/dev/null
    echo "[duck] [sound] lines: $(grep -h "\[sound\] " "$R"/page-*.log 2>/dev/null | wc -l); the first 20:"
    grep -h "\[sound\] " "$R"/page-*.log 2>/dev/null | head -20
    grep -h "a duck (fn151)" "$R"/page-*.log 2>/dev/null
    # 0 before the fix (apart and together); after the fix 1 apart, 0 together.
    echo "[duck] duck pass-over lines: $(grep -h "a duck (fn151) by player 2 apart: passed over" "$R"/page-*.log 2>/dev/null | wc -l)"
    grep -q "\[sweep\] player 2 put at" "$R"/page-*.log 2>/dev/null || echo "[duck] WARNING: ED_PUT put nobody: no '[sweep] player 2 put at' line"
    grep -q "script 1951 runs for player 2" "$R"/page-*.log 2>/dev/null ||
        echo "[duck] WARNING: 1951 never ran for player 2 (not in the sphere, no prompt, or the press missed it): no duck to test"
    echo "[duck] 1951 resumes for player 2: $(grep -h "script 1951 goes on as player 2's" "$R"/page-*.log 2>/dev/null | wc -l)"
    rooms_at_end "$R" "$2" "$3"
    hangs
}

run_one() {
case "$1" in
probe)  # Chapter 10, no presses: the control frame. Closes at the default base 2300 + 300 (control came at 1546-1595).
        B=$(last_run jtg-duck-probe)
        JTG_SLOT=1 JTG_FRAMES=300 env -u ED_COOP_STAY sh tools/ed/jtg.sh 10 duck-probe </dev/null
        new_run jtg-duck-probe "$B" || return 1
        echo "[duck] probe: $R"
        CR=$(control_of "$R") || { echo "[duck] no control in $R (later than frame 2600? raise JTG_FRAMES)"; return 1; }
        echo "[duck] control at frame ${CR% *} in room ${CR#* } (expect 237)"
        [ "${CR#* }" = 237 ] || echo "[duck] WARNING: control not in room 237" ;;
state)  # A fresh state on this build: flag 413, then both players to room 273 by door 754 (follow mode), saved at F+900.
        if [ -n "$DUCK_F" ]; then F=$DUCK_F    # by hand: nothing checks that it is this build's
        else
            P=$(last_run jtg-duck-probe)
            [ -n "$P" ] && [ ! build-win/EternalDarkness.exe -nt "$P/jtg-report.txt" ] ||
                { echo "[duck] no probe newer than the build (newest: ${P:-none}): run the probe step first (or set DUCK_F)"; return 1; }
            CR=$(control_of "$P") || { echo "[duck] no control in $P: run the probe step again (or set DUCK_F)"; return 1; }
            F=${CR% *}
        fi
        S=$((F + 900))
        rm -f "$ST" "$ENVF"
        hangs
        B=$(last_run jtg-duck-state)
        # 413 at F+30, before 754: the arrival script L09/1626 makes the force field only when 413 is on at the arrival.
        # 754 asks nothing, so it runs once.
        JTG_SLOT=1 env -u ED_COOP_STAY ED_SET_FLAG="$((F + 30)):413" ED_RUN_SCRIPT="$((F + 60)):754" ED_SAVE_STATE="$S:$ST" \
            ED_RUN_FRAMES=$((S + 60)) sh tools/ed/jtg.sh 10 duck-state </dev/null
        new_run jtg-duck-state "$B" || return 1
        RS=$R; echo "[duck] state: $RS (F $F, S $S)"
        # The start, room 237 (0x000000ED), makes no change line; 0x00000111 = room 273 is 754's arrival.
        grep -h "setting flag\|running script 754\|\[room\] change\|saving a state" "$RS"/page-*.log 2>/dev/null
        FF=$(frame_of "$RS" "setting flag 413 (set)"); FD=$(frame_of "$RS" "running script 754")
        [ -n "$FF" ] && [ -n "$FD" ] && [ "$FF" -lt "$FD" ] ||
            echo "[duck] WARNING: flag 413 (frame ${FF:-none}) was not set before 754 ran (frame ${FD:-none}): no force field at the arrival"
        [ -f "$ST" ] || { echo "[duck] no state saved ($RS)"; return 1; }
        rooms_at_end "$RS" 273 273 || { rm -f "$ST"; echo "[duck] both players must be in 273: $ST removed"; return 1; }
        printf 'F=%s\nS=%s\nSET=%s\n' "$F" "$S" "$RS/settings.ini" > "$ENVF"
        hangs ;;
apart)  # Player 1 out to 279 (732 once), player 2 stays: put in the sphere at PUT, a step, b at PUT+60.
        load_env || return
        PUT=${DUCK_PUT:-$((S + 900))}; PRESS=$((PUT + 60)); END=$((PRESS + 1440))
        hangs
        B=$(last_run duck-apart)
        ED_SETTINGS="$SET" ED_COOP_STAY=1 ED_SET_FLAG="20:413,22:172" ED_RUN_SCRIPT="100:732" \
            ED_PUT="$PUT,2,2145,1211,-184" ED_PAD2_SCRIPT="$((PUT + 2)),$((PUT + 8)),lup;$PRESS,$((PRESS + 6)),b" \
            ED_RUN_FRAMES=$END ED_RUN_SECONDS=$(secs "$S" "$END") sh tools/ed/run.sh "$ST" duck-apart </dev/null
        new_run duck-apart "$B" || return 1
        echo "[duck] apart: $R (S $S, put $PUT, press $PRESS, the duck about $((PRESS + 455)))"
        FC=$(frame_of "$R" "\[room\] change 0x00000117")
        [ -n "$FC" ] && [ "$FC" -lt "$PUT" ] ||
            echo "[duck] WARNING: player 1's change to 279 (frame ${FC:-none}) did not come before the put ($PUT): set DUCK_PUT later and run again"
        evidence "$R" 279 273 ;;
together) # The same with player 1 left in 273: no pass-over line (not apart), before or after the fix.
        load_env || return
        PUT=$((S + 400)); PRESS=$((PUT + 60)); END=$((PRESS + 1440))
        hangs
        B=$(last_run duck-together)
        ED_SETTINGS="$SET" ED_COOP_STAY=1 ED_SET_FLAG="20:413,22:172" \
            ED_PUT="$PUT,2,2145,1211,-184" ED_PAD2_SCRIPT="$((PUT + 2)),$((PUT + 8)),lup;$PRESS,$((PRESS + 6)),b" \
            ED_RUN_FRAMES=$END ED_RUN_SECONDS=$(secs "$S" "$END") sh tools/ed/run.sh "$ST" duck-together </dev/null
        new_run duck-together "$B" || return 1
        echo "[duck] together: $R (S $S, put $PUT, press $PRESS, the duck about $((PRESS + 455)))"
        evidence "$R" 273 273 ;;
static) # No game. 0x80164DD4u is in hle.c today; 0x80164DD8u only after the fix.
        grep -n "0x80164DD4u\|0x80164DD8u" runtime/host/hle.c
        # "CAN'T MATCH" is hooks.py's own output (tools/ed/facts/hooks.py:427), not hle.c's; it also writes
        # build/ed-db/facts/hooks.json (build output). Before the fix: the hle.c:15084 line; after: none for 15084.
        python -I tools/ed/facts/hooks.py | grep "CAN'T MATCH" ;;
*)      echo "[duck] no step named $1" ;;
esac
}

if [ $# -eq 0 ]; then set -- probe state apart together static; fi
for r in "$@"; do run_one "$r"; done
echo RUNS DONE
