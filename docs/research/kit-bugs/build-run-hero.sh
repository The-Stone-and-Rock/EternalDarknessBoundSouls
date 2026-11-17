#!/bin/sh
# The hero-gate patch (ED_COOP_HERO_ID=1, hle.c coop_hero_chunk) for player 1: Edward's lever L09/1951, room 273, as in
# build-run-duck.sh (read its header for the room, the sphere, the flags and the doors). The duck runs showed the gate
# opening for player 2 ("[hero] script 1951: gate +0x29F opens for player 2 (id 161; ...)"); these show player 1 is
# untouched by it.
#
# STEPS (none given: solo p1after):
#   solo     Co-op off (JTG_SET=mod_coop=0), a cold Jump to Game to chapter 10 with ED_COOP_HERO_ID=1: flags 413 and 172 at
#            F+30, door 754 at F+60, player 1 put in the sphere at F+900, a short stick step, B (0x0200) at F+960, closed
#            1440 frames after the press. F is logs/duck-ch10.env's (co-op's control frame).
#            Expect: no "[hero]" line (no player 2: the gate stays 1); 1951 runs for player 1 and goes past state 0 (the
#            lever scene: `ed stops RUN 1951 --level 09` shows calls past the gate); no new hang report.
#   p1after  Co-op, players together (the duck state, ED_COOP_STAY=1): player 2 into the sphere at PUT=S+400 (the gate
#            patched to 161 at that run's load), out again at PUT+150 (their start, -2562 1357), player 1 into the sphere
#            at PUT+200 and B at PUT+260. Expect: "[hero] ... opens for player 2" once, then at player 1's load either
#            "[hero] ... back to the chapter's character (id 1)" (the chunk kept) or nothing (a fresh chunk each run);
#            either way 1951 runs for player 1 (who 1) and goes past state 0, and the scene is player 1's.
# Run from Git Bash: sh docs/research/kit-bugs/build-run-hero.sh [STEP...]. Never with the user's game open.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_RUN_AWAY=1 ED_ROOM_SLOTS=2 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_RUN_SHOTS_MS=250 ED_COOP_HERO_ID=1
unset ED_COOP_STAY
ST=saves/duck-ch10-273.state
. ./logs/duck-ch10.env || exit 1
SPHERE=2145,1211,-184

last_run() { ls -d logs/run-*-"$1" 2>/dev/null | tail -1; }
new_run() {
    R=$(last_run "$1")
    [ -n "$R" ] && [ "$R" != "$2" ] || { echo "[hero] no new $1 run (the newest is still ${2:-none})"; return 1; }
}
hangs() { echo "[hero] hang reports: $(ls saves/reports/hang-*.txt 2>/dev/null | wc -l)"; }
evidence() {
    grep -h "setting flag\|running script 754\|\[room\] change 0x00000111\|\[sweep\] player\|\[hero\]\|script 1951\|scene 1951" \
        "$1"/page-*.log 2>/dev/null | head -40
    echo "[hero] [hero] lines: $(grep -h "^\[hero\]" "$1"/page-*.log 2>/dev/null | wc -l)"
    python -I tools/ed/ed.py stops "$1" 1951 --level 09 | head -60
    hangs
}

run_one() {
case "$1" in
solo)
    hangs
    B=$(last_run jtg-hero-solo)
    PUT=$((F + 900)); PRESS=$((F + 960))
    JTG_SLOT=1 JTG_SET="mod_coop=0" JTG_BASE=$((F + 30)) ED_SET_FLAG="$((F + 30)):413,$((F + 32)):172" \
        ED_RUN_SCRIPT="$((F + 60)):754" ED_PUT="$PUT,1,$SPHERE" \
        ED_INPUT_SCRIPT="$((PUT + 2)),$((PUT + 8)),0,0,127;$PRESS,$((PRESS + 6)),0x0200" \
        ED_RUN_FRAMES=$((PRESS + 1440)) sh tools/ed/jtg.sh 10 hero-solo </dev/null
    new_run jtg-hero-solo "$B" || return 1
    echo "[hero] solo: $R (put $PUT, press $PRESS)"
    evidence "$R" ;;
p1after)
    [ build-win/EternalDarkness.exe -nt "$ST" ] && { echo "[hero] the build is newer than $ST: run build-run-duck.sh state"; return 1; }
    hangs
    B=$(last_run hero-p1after)
    PUT=$((S + 400)); P1=$((PUT + 200)); PRESS=$((P1 + 60)); END=$((PRESS + 1440))
    ED_SETTINGS="$SET" ED_COOP_STAY=1 ED_SET_FLAG="20:413,22:172" \
        ED_PUT="$PUT,2,$SPHERE;$((PUT + 150)),2,-2562,1357,-696;$P1,1,$SPHERE" \
        ED_PAD2_SCRIPT="$((PUT + 2)),$((PUT + 8)),lup" \
        ED_INPUT_SCRIPT="$((P1 + 2)),$((P1 + 8)),0,0,127;$PRESS,$((PRESS + 6)),0x0200" \
        ED_RUN_FRAMES=$END ED_RUN_SECONDS=$(((END - S) / 20 + 120)) sh tools/ed/run.sh "$ST" hero-p1after </dev/null
    new_run hero-p1after "$B" || return 1
    echo "[hero] p1after: $R (player 2 in $PUT, out $((PUT + 150)), player 1 in $P1, press $PRESS)"
    evidence "$R" ;;
*)  echo "[hero] no step named $1" ;;
esac
}

if [ $# -eq 0 ]; then set -- solo p1after; fi
for r in "$@"; do run_one "$r"; done
echo RUNS DONE
