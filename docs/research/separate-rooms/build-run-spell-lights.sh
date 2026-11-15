#!/bin/sh
# Lights per room P7 (design-lights.md): a rune's light only in the half of its caster's room while the players are apart.
# Fresh Jump-to-Game state on the current build (chapter 3, room 153, the cheat's magick and a Tome, as
# build-run-stayer-findings.sh makes it), the window off the desktop and muted (ED_RUN_AWAY=1), Remastered spells forced
# on (ED_RUNES_3D=1: the rune models and their lights). Pass: the `[lights] rune N's light (caster's room R): views V` lines,
# V 2 for player 2's cast in 153 with player 1 in 151 (run a), V 1 for player 1's in 153 with player 2 in 151 (run b);
# together (run c) no such line (views 3); solo (run d) no [lights] rune line. No hang report.
# Run from Git Bash: sh docs/research/separate-rooms/build-run-spell-lights.sh [s3] [a] [b] [c] [d]; none given: all.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_RUN_AWAY=1 ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_RUN_SHOTS_MS=250
L3="ED_COOP_CAST_LOG=1 ED_CHEAT=1 ED_RUNES_3D=1 ED_COOP_SPELL_LIST=1"
ST=saves/spell-lights-ch3-153.state
last_run() { ls -d logs/run-*-"$1" 2>/dev/null | tail -1; }
DOOR="-60,-400,-100"   # inside Region02 (1139, 153 -> 151)
FOUNT="1150,-100,0"    # inside CameraRgn03, away from the door
show() { R=$(last_run "$1"); echo "[spell-lights] $1: $R"; grep -h "\[lights\] rune\|\[lights\] apart\|cast by\|steps in its own room\|screen flash\|\[runes\] model\|\[test\] player 1\|\[coop\] spells at" "$R"/page-*.log 2>/dev/null | grep -v "spells at frame [0-9]*: *$" | head -16; ls saves/reports/hang-*.txt 2>/dev/null | tail -1; }
run_one() {
case "$1" in
s3) rm -f "$ST"
    JTG_SLOT=1 JTG_P1=1 JTG_P2=2 env -u ED_COOP_STAY ED_CHEAT=1 ED_ITEM_SPAWN=2400,30,30,300,1 \
      JTG_P1_SCRIPT="130,138,0x0200;190,198,0x0200;230,238,0x0200;270,278,0x0200" \
      ED_RUN_SCRIPT=2750:1106,2810:1106 ED_SAVE_STATE=3050:$ST ED_RUN_FRAMES=3150 \
      sh tools/ed/jtg.sh 3 spell-lights-c3-153 </dev/null
    R=$(last_run jtg-spell-lights-c3-153); echo "SET=$R/settings.ini" > logs/spell-lights-ch3.env
    [ -f "$ST" ] && echo "[spell-lights] $ST from $R" || echo "[spell-lights] no state ($R)" ;;
a)  . logs/spell-lights-ch3.env
    env ED_SETTINGS="$SET" $L3 ED_RUN_SCRIPT=410:1139,470:1139 ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4200 \
      ED_PUT="3380,1,$DOOR;3380,2,$FOUNT" ED_PAD2_SCRIPT="3700,3706,up" sh tools/ed/run.sh $ST spell-lights-a-p2-apart </dev/null
    show spell-lights-a-p2-apart ;;
b)  . logs/spell-lights-ch3.env
    env ED_SETTINGS="$SET" $L3 ED_COOP_P1_QUICK=00010504 ED_RUN_FRAMES=4200 ED_PUT="3380,1,$FOUNT;3380,2,$DOOR" \
      ED_PAD2_SCRIPT="3460,3470,b" ED_INPUT_SCRIPT="3700,3706,0x0008" sh tools/ed/run.sh $ST spell-lights-b-p1-apart </dev/null
    show spell-lights-b-p1-apart ;;
c)  . logs/spell-lights-ch3.env
    env ED_SETTINGS="$SET" $L3 ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=3900 ED_PUT="3380,1,$FOUNT;3380,2,1250,-100,0" \
      ED_PAD2_SCRIPT="3540,3546,up" sh tools/ed/run.sh $ST spell-lights-c-together </dev/null
    show spell-lights-c-together ;;
d)  # Solo casts with the game's own Y slot (ED_QUICK_SET: slot 0; ED_COOP_P1_QUICK is co-op only). JTG_P1_SCRIPT is relative
    # to JTG_BASE (2300 for chapter 3): Y at 3300 and 3450, after the arrival in 153 (~2875).
    JTG_SLOT=1 JTG_SET="mod_coop=0" env $L3 ED_ITEM_SPAWN=2400,30,30,300,1 ED_QUICK_SET=3200,0,00010504 \
      JTG_P1_SCRIPT="130,138,0x0200;190,198,0x0200;230,238,0x0200;270,278,0x0200;1000,1006,0x0800;1150,1156,0x0800" \
      ED_RUN_SCRIPT=2750:1106,2810:1106 ED_RUN_FRAMES=3700 sh tools/ed/jtg.sh 3 spell-lights-d-solo </dev/null
    show jtg-spell-lights-d-solo ;;
*) echo "[spell-lights] no run named $1" ;;
esac
}
if [ $# -eq 0 ]; then set -- s3 a b c d; fi
for r in "$@"; do run_one "$r"; done
echo RUNS DONE
