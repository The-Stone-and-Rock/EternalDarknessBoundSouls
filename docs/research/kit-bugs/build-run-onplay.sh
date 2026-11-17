#!/bin/sh
# ed177's play-start script in co-op ("onplay"; docs/research/kit-bugs/plans.md section 5, the skeptic's corrected fix
# and test plans, which this follows).
#
# THE BUG. ed177 (on_play_script, 0x8000F9C4) stores one script id in r13-24248, with no owner. Mode 2's enter
# (0x80008234) runs it once at 0x8000827C (lr 0x80008280) when play resumes after the real Tome, a film or a cinematic.
# hle.c's coop_run_script has no branch for lr 0x80008280, so it always runs as player 1, with player 1's zones and room.
# Here: L03 room 150's MuffledVoicesRgn01 enter (script 933) does "if in_zone: ed177(933), stream_play 40", the re-arm
# inside the zone test. Stored by player 2 (in the zone) and run as player 1 (outside it), it does nothing and does not
# re-arm, so the second Tome close runs nothing at all. Cosmetic (sound and streams only), but real.
# THE PLANNED FIX (plans.md section 5, "Fix plan"): a notify on 0x8000F9C4 records whether player 2 called ed177
# (g_play_script_p2); a PLAY_START_RAN 0x80008280 block in coop_run_script, after its trace, runs the stored script as
# player 2's (coop_be_player2, coop_zones_swap, coop_stay_room_in, a guest call, then undone) when player 2 stored it.
# Solo and player 1's stores stay native.
#
# WHAT IS TESTED. Chapter 3 (Anthony, level 03) starts in room 150 (0x96 in [room] lines, which print hex): control at
# frame 2114-2124 in 20+ earlier runs, P1 (-753,-30,2), P2 (-753,120,0), both outside MuffledVoicesRgn01 (box x -301..475,
# y 155..1048, z 0..288; enter 933, leave 934). No door script and no flag (the reader's draft used door 898 and flag 77:
# dropped, the skeptic's notes). One fresh state per build, at frame S (default 2300, after control), then:
#   p2in  (the bug)     player 2 put in the zone at S+100 with a short stick step, player 1 stays outside; player 1's
#                       real Tome opened and closed twice (Start at S+T, S+T+300, S+T+600, S+T+900; T = ONPLAY_TOME_AT,
#                       default 900).
#   p1in  (control)     the same with player 1 in the zone and player 2 outside. Unchanged by the fix.
#   solo  (control)     co-op off (mod_coop=0), player 1 in the zone. A COLD Jump to Game (a co-op state played alone parks
#                       player 2, as build-run-tome.sh's solo says), base S (or ONPLAY_SOLO_S). Unchanged by the fix.
# Every run: ED_RUN_AWAY=1 (window away, muted), follow mode (ED_ROOM_SLOTS=2, ED_COOP_STAY unset), ED_MODE_LOG=1;
# jtg.sh and run.sh already set ED_COOP_LOG, ED_COOP_TRACE and ED_ROOM_LOG. Runs close T+1200 frames after S (2100 by
# default: 294 frames after the last close press, whatever T is).
#
# STEPS, IN ORDER (run from Git Bash; never with the user's game open: the runs close the exe by name):
#   BEFORE the fix, on today's build:   sh docs/research/kit-bugs/build-run-onplay.sh state p2in p1in solo
#   AFTER the fix, once rebuilt:        ONPLAY_TAG=after sh docs/research/kit-bugs/build-run-onplay.sh state p2in p1in solo
#   (none given: state p2in p1in solo.) `state` must come first on each build: p2in/p1in refuse a state older than
#   build-win/EternalDarkness.exe, or more than 12 hours old (a fresh state per session, not one left from another day).
#   ONPLAY_TAG (default before) goes into every run's name. Each step refuses to read a run directory older than itself.
# Knobs: ONPLAY_S (the state's frame, default 2300); ONPLAY_ZONE (the put, default 300,400,-150: the ray starts at z 150,
# below the z 300 surface an old put hit; if the [sweep] line's Z is outside 0..288, use 365,180,-150, where player 2
# stood in the zone in logs/run-20261106-142801-jtg-rega5-c3a-m5 at 7977, though only 25 units inside the box's y 155
# edge: the "player N at frame" lines then show whether the stick step walked them out); ONPLAY_TOME_AT (T, the first
# Start, default 900 after S; the run's length follows it); ONPLAY_CLOSE (the close press, default 0x1000 Start,
# inferred to close the Tome; if "[mode] 3" stays open, use 0x0200, B); ONPLAY_SOLO_S (solo's base, default the state's
# S: solo checks its own control frame and says LATE with the value to use).
#
# WHAT EACH STEP PRINTS AND WHAT IT MEANS.
#   state: "saving a state" and the control frame. FAILS (and says so) if control came later than S-30 (then set
#     ONPLAY_S to the control frame + 90 and run state again) or not in room 150. Then "player 1/2 at frame S: ...,
#     room R (OUTSIDE ...)" from both.tsv: both in room 150 and both OUTSIDE the zone's box. No "script run" line for 933
#     should be listed (nobody in the zone yet).
#   p2in / p1in / solo: the evidence lines, in order:
#     "[sweep] player N put at X Y Z" with a note whether the place is inside the zone's box ("not put: no floor" too).
#     "[onplay] player N at frame F: X Y Z (inside/OUTSIDE ...)": where the zone's player stood (both.tsv) after the stick
#       step (S+150) and as the Tome first opens (S+T). OUTSIDE on either: another ONPLAY_ZONE; the rest can't be read.
#     "[trace] script run r3 933 ... lr L who W ..." (every run of 933, and 934's, the leave), each followed by 933's own
#       "[script] calls" lines, in 933's order: 0x8016A7DC (fn23, the zone test), then on its true path 0x8000F9C4 (ed177,
#       the re-arm), 0x80169128 (fn63), 0x80165020 (fn126, the stream), 0x8016B784 (fn36, the wait). Calls come from
#       g_script_watch (40 calls after any traced script, hle.c:20665) and carry no script id, so a calls line is taken as
#       933's only while it is the next one in that order right after the 933 line; the first that is not ends 933's list
#       (other scripts' calls, e.g. the untraced zone scripts', are not listed under it). L: 0x80134B38 = the zone's enter
#       (W = whoever walked in: 1 is player 1, player 2's id is another number, e.g. 76; read it from this line);
#       0x80163E5C = 933 run again by its wait(500), about 650 frames on (logs/run-20261106-052058-jtg-rega3-c3b-r3: 6606,
#       then 7257, each with the whole true path, re-arm included); 0x80008280 = mode 2's enter (the play-start run);
#       0x0BADF010 = a guest call. A 934 line at lr 0x80134B38 means the zone's player walked out.
#     "[mode] M (asked A) at frame F": 3 = the real Tome up, 2 = play. They print in solo too (read: the game's pad read
#       calls coop_pad_read whatever mod_coop says, hle.c:4377, and the [mode] print at hle.c:32355 comes before its co-op
#       tests; logs/run-20261103-121532-s1-solo has them). In p1in/solo a note is printed if the first "[mode] 3" came
#       before 933's lr 0x80163E5C line (the Tome may have opened inside 933's wait: raise ONPLAY_TOME_AT). In p2in it is
#       only a notice: player 2's wait run may never come (rega5 has none in ~1700 frames after 7977, player 2 gone at 8000).
#     (only after the fix) "[coop] on_play_script left by player N" and "[coop] play start's script 933 (left by player 2)
#       ran as player 2's"; "[coop] on_play_script (0x8000F9C4) not hooked" means the fix's hook did not install.
#     The counts (always printed, the line cap of 200 never cuts them): 933's zone entries by player 1 and by player 2;
#       934 leaves; Tome opens and closes; wait runs (lr 0x80163E5C), play-start runs (lr 0x80008280) and guest runs
#       (lr 0x0BADF010), each with how many of them re-armed (a 0x8000F9C4 call in 933's own list). Then FAILED lines for a
#       precondition that did not hold (the wrong player entered, the zone left, fewer than 2 opens and closes), and the
#       hang reports new since the step began.
#   BEFORE the fix:
#     p2in: "script run r3 933 ... lr 0x80134B38 who <P2ID>" then 933's whole true path; after the first "[mode] 2
#       (asked 2)" that follows "[mode] 3": "script run r3 933 r4 0 lr 0x80008280 who 1" followed only by 0x8016A7DC;
#       after the second close, NO lr 0x80008280 line (not re-armed). Counts: 1 play-start run, 0 re-armed. A wait run of
#       player 2's entry, if one comes, runs as player 1 (natively: hle.c has no player 2 branch for lr 0x80163E5C but the
#       stay-room script's, hle.c:20557), shows only 0x8016A7DC and re-arms nothing.
#     p1in and solo: "lr 0x80008280 who 1" after BOTH closes, each followed by 933's whole true path. Counts: 2 play-start
#       runs, 2 re-armed. Player 1 stays in the zone, so wait runs (lr 0x80163E5C, who 1) also come every ~650 frames,
#       each re-arming too: the second play-start run alone does not show the first one re-armed; its "re-armed" count does.
#     The same shape is in logs/run-20261106-142801-jtg-rega5-c3a-m5, frames 7977 and 9671.
#   AFTER the fix:
#     p2in: "on_play_script left by player 2" at the zone entry; at each close "lr 0x80008280 who 1" (with no calls of its
#       own: the guest call's trace comes next), then "script run r3 933 r4 0 lr 0x0BADF010 who <P2ID>" with 933's whole
#       true path, then "play start's script 933 (left by player 2) ran as player 2's". Counts: 2 play-start runs, 2 guest
#       runs, 2 guest re-arms.
#     p1in: "on_play_script left by player 1", no "play start's script"; otherwise as before. solo: neither line; as before.
#   Every run: no new hang report (saves/reports/hang-*.txt), and `ed check`'s own summary (printed by jtg.sh / run.sh)
#   clean. (The plan's ASSERT grep is left out: no line in runtime/host/hle.c prints ASSERT.)
#
# NOT SCRIPTED (left out rather than guessed):
#   - Whether the restore is audible (stream 40 stopped by the Tome, put back after): ED_RUN_AWAY mutes the game and no
#     log reads the stream records at 0x805DB5C0; ED_STREAM_LOG's disc reads can't tell a restart from a stream that never
#     stopped. The test reads who runs 933 and what it calls, not the sound.
#   - Room 83's pools (1188/1189): no door script leads there (`ed door 83`: only game code, transition kind 39).
#   - L03 room 152's 2211 -> 2465 DARK_AREA words run as player 2 at mode 2's enter (the fix's untested risk): no way into
#     room 152 or press schedule for its Region04 has been read for these tools.
#   - The players apart (one word for two rooms, the room-change clears): not in the corrected test plan.
# LEFT FOR A RUN (fixing them needs a run to know):
#   - Whether 933's wait runs (lr 0x80163E5C) go on while the Tome is up (mode 3), and so where they fall against the
#     closes, and whether player 2's entry gets one at all. The script prints and counts them; it does not predict them.
#   - A stray call of another script's that is exactly the next one in 933's order right after a 933 line would still be
#     listed under 933 (the calls lines carry no script id). Rare; nothing short of a run shows whether it happens.
#   - Whether solo's control test (jtg_report.py's, without its player 2 terms) picks the real control frame [inferred].
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_RUN_AWAY=1 ED_ROOM_SLOTS=2 ED_MODE_LOG=1 ED_COOP_LOG=1 ED_ROOM_LOG=1 ED_RUN_SHOTS_MS=250
unset ED_COOP_STAY   # follow mode
TAG=${ONPLAY_TAG:-before}
ST=saves/onplay-ch3.state
ENVF=logs/onplay-ch3.env
SECS=600
ZONE=${ONPLAY_ZONE:-300,400,-150}   # in MuffledVoicesRgn01 (x -301..475, y 155..1048, z 0..288)
T=${ONPLAY_TOME_AT:-900}
FR=$((T + 1200))   # the run's length after S: the last close press is at T+906
CLOSE=${ONPLAY_CLOSE:-0x1000}
# Player 1's real Tome, open and close twice, frames relative to $1 (0 for jtg.sh's relative lists, S for absolute ones).
tome() { b=$1; printf '%s' "$((b+T)),$((b+T+6)),0x1000;$((b+T+300)),$((b+T+306)),$CLOSE;$((b+T+600)),$((b+T+606)),0x1000;$((b+T+900)),$((b+T+906)),$CLOSE"; }

last_run() { ls -d logs/run-*-"$1" 2>/dev/null | tail -1; }
# The newest run named $1, only if it is not $2 (the newest before the step): never an older run's evidence.
new_run() { nr=$(last_run "$1"); [ -n "$nr" ] && [ "$nr" != "$2" ] && echo "$nr"; }
# A solo run's control frame and room -> "F R", or nothing. jtg_report.py's own test needs player 2 (its "control at
# frame" line says "control never came" in solo runs, e.g. logs/run-20261115-182018-jtg-page-solo), so this is its
# in_control without the player 2 terms [inferred to pick the same frame].
solo_control() {
    python -I - "$1" <<'EOF'
import os, sys
sys.path.insert(0, os.path.join('tools', 'ed'))
import jtg_report as j
for r in j.rows(sys.argv[1]):
    if (j.num(r['room']) >= 0 and j.num(r['busy']) == 0 and j.num(r['hold']) == 0 and j.num(r['cutscene']) == 0
            and j.num(r['menu']) == 0 and j.num(r['fade']) == 0 and j.num(r['p1st']) >= 0):
        print(r['frame'], int(j.num(r['room'])))
        sys.exit(0)
sys.exit(1)
EOF
}
# Where player $2 stood in run $1 at each later frame (the first both.tsv row at or after it), against the zone's box.
stood() {
    python -I - "$@" <<'EOF'
import os, sys
sys.path.insert(0, os.path.join('tools', 'ed'))
import jtg_report as j
run, who, frames = sys.argv[1], sys.argv[2], [int(f) for f in sys.argv[3:]]
rs = j.rows(run)
if not rs:
    print('[onplay] no both.tsv rows in', run)
    sys.exit(1)
for want in frames:
    r = next((r for r in rs if j.num(r['frame']) >= want), None)
    if r is None:
        print('[onplay] player %s at frame %d: no row (the run ended first)' % (who, want))
        continue
    x, y, z = j.place(r, 'p' + who)
    inside = -301 <= x <= 475 and 155 <= y <= 1048 and 0 <= z <= 288
    print("[onplay] player %s at frame %s: %.0f %.0f %.0f, room %s (%s MuffledVoicesRgn01's box)" % (
        who, r['frame'], x, y, z, r['room'], 'inside' if inside else 'OUTSIDE'))
EOF
}
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
hangs() { ls saves/reports/hang-*.txt 2>/dev/null; }
new_hangs() {  # the list taken before the step
    n=$(hangs | grep -Fxv "$1")
    if [ -n "$n" ]; then echo "[onplay] NEW hang reports:"; echo "$n"; else echo "[onplay] no new hang report"; fi
}

# The evidence of one run. Grep strings, all in runtime/host/hle.c today unless marked:
#   "[sweep] player" (hle.c:17030 put, 17022 not put; the put line gives the landed place, z = the floor hit + 2),
#   "script run" (hle.c:20664, printed by trace_look's "[trace] %s r3 %d r4 %d lr 0x%08X who %u" at hle.c:11768, so
#   $5 is the script and $9 the lr, $11 who), "[script] calls 0x" (hle.c:11804, $3 the function), "[mode] " (hle.c:32361,
#   $2 the mode), "on_play_script" and "play start's script" (only after the fix).
# 933's own calls, in its order (`ed script 933 --level 3`): fn23 0x8016A7DC, then on its true path ed177 0x8000F9C4,
# fn63 0x80169128, fn126 0x80165020, fn36 0x8016B784 (the order logs/run-20261106-052058-jtg-rega3-c3b-r3 shows at 6606).
# Arguments: the run, the step (p2in, p1in or solo), the zone's player (1 or 2), the base frame (S).
evidence() {
    R=$1
    stood "$R" "$3" $(($4 + 150)) $(($4 + T))
    grep -h -e '\[sweep\] player' -e 'script run' -e '\[script\] calls 0x' -e '\[mode\] ' \
        -e 'on_play_script' -e "play start's script" "$R"/page-*.log 2>/dev/null |
    awk -v step="$2" -v zw="$3" -v q="'" '
function out(s) { if (++lines <= 200) print s; else cut++ }
BEGIN { split("0x8016A7DC 0x8000F9C4 0x80169128 0x80165020 0x8016B784", own, " ") }
/\[sweep\] player [12] put at / {
    out($0)
    if ($6 + 0 >= -301 && $6 + 0 <= 475 && $7 + 0 >= 155 && $7 + 0 <= 1048 && $8 + 0 >= 0 && $8 + 0 <= 288)
        out("    (inside MuffledVoicesRgn01" q "s box)")
    else out("    (OUTSIDE MuffledVoicesRgn01" q "s box: try ONPLAY_ZONE=365,180,-150)")
    if ($3 == zw) put++
    next }
/\[sweep\] player [12] not put/ { out($0); next }
/\[trace\] script run r3 / {
    seq = 0
    if ($5 != "933" && $5 != "934") next
    out($0)
    if ($5 == "934") { if ($9 == "0x80134B38") { left++; if (closes < 2) left_early++ }; next }
    lr = $9; seq = 1
    if (lr == "0x80134B38") { if ($11 == "1") enter1++; else enter2++ }
    else if (lr == "0x80163E5C") waits++
    else if (lr == "0x80008280") start++
    else if (lr == "0x0BADF010") guest++
    next }
/\[script\] calls 0x/ {
    if (seq < 1 || seq > 5 || $3 != own[seq]) { seq = 0; next }
    out("    " $0)
    if (seq == 2) {
        if (lr == "0x80163E5C") waits_rearm++
        else if (lr == "0x80008280") start_rearm++
        else if (lr == "0x0BADF010") guest_rearm++
    }
    seq++
    next }
/\[mode\] / {
    out($0)
    if ($2 == "3" && !up) {
        up = 1; opens++
        if (opens == 1 && !waits) {
            if (step == "p2in") out("    (notice: no lr 0x80163E5C run of 933 before the Tome. Player 2" q "s may never come (rega5 had none): not by itself a reason to raise ONPLAY_TOME_AT)")
            else out("    (the Tome opened before 933" q "s lr 0x80163E5C line, maybe inside its wait(500): raise ONPLAY_TOME_AT)")
        }
    } else if ($2 == "2" && up) { up = 0; closes++ }
    next }
/on_play_script|play start/ { out($0); next }
END {
    if (cut) print "[onplay] (" cut " more lines cut: the page logs have them)"
    printf "[onplay] 933 zone entries: by player 1 %d, by player 2 %d; 934 leaves %d; Tome opens %d, closes %d\n", enter1, enter2, left, opens, closes
    printf "[onplay] 933 runs (re-armed): wait lr 0x80163E5C %d (%d); play start lr 0x80008280 %d (%d); guest lr 0x0BADF010 %d (%d)\n", waits, waits_rearm, start, start_rearm, guest, guest_rearm
    if (!put) print "[onplay] FAILED: player " zw " was never put (no [sweep] put line for them)"
    if (zw == 2 && (enter2 < 1 || enter1 > 0)) print "[onplay] FAILED: 933 should be entered by player 2 only: the rest cannot be read"
    if (zw == 1 && (enter1 < 1 || enter2 > 0)) print "[onplay] FAILED: 933 should be entered by player 1 only: the rest cannot be read"
    if (left_early) print "[onplay] FAILED: the zone was left (934) before the second close: the play-start counts cannot be read (another ONPLAY_ZONE)"
    if (opens < 2 || closes < 2) print "[onplay] FAILED: the Tome opened " opens + 0 " and closed " closes + 0 " times, 2 and 2 expected (if [mode] 3 stays up: ONPLAY_CLOSE=0x0200)"
}'
}

from_state() {  # step, the zone's player (1 or 2), then the env assignments for the run
    step=$1; zw=$2; shift 2
    name="onplay-$step-$TAG"
    [ -f "$ENVF" ] && [ -f "$ST" ] || { echo "[onplay] make the state first (step state)"; return 1; }
    if [ build-win/EternalDarkness.exe -nt "$ST" ]; then echo "[onplay] $ST is older than this build: run step state first"; return 1; fi
    if [ -n "$(find "$ST" -mmin +720 2>/dev/null)" ]; then
        echo "[onplay] $ST is over 12 hours old ($(date -r "$ST" '+%Y-%m-%d %H:%M')): run step state again for a fresh one"; return 1
    fi
    . "./$ENVF"
    HB=$(hangs)
    B=$(last_run "$name")
    env ED_SETTINGS="$SET" ED_RUN_FRAMES=$((S + FR)) ED_RUN_SECONDS=$SECS "$@" sh tools/ed/run.sh "$ST" "$name" </dev/null
    R=$(new_run "$name" "$B") || { echo "[onplay] $name: no new run directory (run.sh did not start?)"; new_hangs "$HB"; return 1; }
    echo "[onplay] $name: $R (state frame $S, run closes at $((S + FR)))"
    evidence "$R" "$step" "$zw" "$S"
    new_hangs "$HB"
}

run_one() {
case "$1" in
state) # A fresh state on this build: a cold Jump to Game to chapter 3, saved at S with both players idle in room 150.
    S=${ONPLAY_S:-2300}
    rm -f "$ST" "$ENVF"
    HB=$(hangs)
    B=$(last_run "jtg-onplay-state-$TAG")
    JTG_SLOT=1 ED_SAVE_STATE="$S:$ST" ED_RUN_FRAMES=$((S + 60)) ED_RUN_SECONDS=$SECS sh tools/ed/jtg.sh 3 "onplay-state-$TAG" </dev/null
    RS=$(new_run "jtg-onplay-state-$TAG" "$B") || { echo "[onplay] state: no new run directory (jtg.sh did not start?)"; rm -f "$ST"; new_hangs "$HB"; return 1; }
    echo "[onplay] state: $RS"
    grep -h -e 'saving a state' "$RS"/page-*.log 2>/dev/null
    # ("script run": hle.c:20664; " r3 933 " picks script 933 out of trace_look's "r3 %d", hle.c:11768: expected none.)
    grep -h -e 'script run' "$RS"/page-*.log 2>/dev/null | grep ' r3 933 '
    new_hangs "$HB"
    [ -f "$ST" ] || { echo "[onplay] no state saved ($RS)"; return 1; }
    CR=$(control_of "$RS") || { echo "[onplay] no control frame in $RS/jtg-report.txt"; rm -f "$ST"; return 1; }
    F=${CR% *}; ROOM=${CR#* }
    echo "[onplay] control at frame $F in room $ROOM; state at frame $S"
    if [ $((F + 30)) -gt "$S" ]; then echo "[onplay] LATE: control after S-30; run again with ONPLAY_S=$((F + 90))"; rm -f "$ST"; return 1; fi
    if [ "$ROOM" != 150 ]; then echo "[onplay] control in room $ROOM, not 150: the wrong chapter?"; rm -f "$ST"; return 1; fi
    printf 'S=%s\nSET=%s\n' "$S" "$RS/settings.ini" > "$ENVF"
    # Both players in room 150 at S and both outside the zone (the state's precondition; "no row" if the run ended first).
    stood "$RS" 1 "$S"; stood "$RS" 2 "$S"
    ;;
p2in) # The bug: player 2 into the zone (a put, then 12 frames of stick up), player 1 outside; player 1's real Tome twice.
    [ -f "$ENVF" ] && . "./$ENVF"
    from_state p2in 2 ED_PUT="$((S + 100)),2,$ZONE" ED_PAD2_SCRIPT="$((S + 130)),$((S + 142)),lup" \
        ED_INPUT_SCRIPT="$(tome "$S")" ;;
p1in) # Control: player 1 into the zone (a put, then 12 frames of stick up), player 2 outside; player 1's real Tome twice.
    [ -f "$ENVF" ] && . "./$ENVF"
    from_state p1in 1 ED_PUT="$((S + 100)),1,$ZONE" \
        ED_INPUT_SCRIPT="$((S + 130)),$((S + 142)),0,0,127;$(tome "$S")" ;;
solo) # Control, co-op off: a cold Jump to Game (base SB), player 1 into the zone, the real Tome twice.
    S=${ONPLAY_S:-2300}
    [ -f "$ENVF" ] && . "./$ENVF"
    SB=${ONPLAY_SOLO_S:-$S}
    HB=$(hangs)
    B=$(last_run "jtg-onplay-solo-$TAG")
    JTG_SLOT=1 JTG_SET="mod_coop=0" JTG_BASE=$SB JTG_FRAMES=$FR ED_RUN_SECONDS=$SECS \
        JTG_PUT="100,1,$ZONE" JTG_P1_SCRIPT="130,142,0,0,127;$(tome 0)" sh tools/ed/jtg.sh 3 "onplay-solo-$TAG" </dev/null
    R=$(new_run "jtg-onplay-solo-$TAG" "$B") || { echo "[onplay] solo: no new run directory (jtg.sh did not start?)"; new_hangs "$HB"; return 1; }
    echo "[onplay] solo: $R (base $SB, run closes at $((SB + FR)))"
    # Solo's own control: jtg-report.txt's "control at frame" needs player 2, so it says "control never came" here.
    if CR=$(solo_control "$R"); then
        F=${CR% *}; ROOM=${CR#* }
        echo "[onplay] solo: player 1 in control at frame $F in room $ROOM (base $SB)"
        if [ $((F + 30)) -gt "$SB" ]; then echo "[onplay] solo FAILED: LATE, control after base-30 (the put, step and presses may be lost); run solo again with ONPLAY_SOLO_S=$((F + 90))"; fi
        if [ "$ROOM" != 150 ]; then echo "[onplay] solo FAILED: control in room $ROOM, not 150"; fi
    else
        echo "[onplay] solo FAILED: no player 1 control frame in $R/both.tsv"
    fi
    evidence "$R" solo 1 "$SB"
    new_hangs "$HB" ;;
*)  echo "[onplay] no step named $1" ;;
esac
}

if [ $# -eq 0 ]; then set -- state p2in p1in solo; fi
for r in "$@"; do run_one "$r"; done
echo RUNS DONE
