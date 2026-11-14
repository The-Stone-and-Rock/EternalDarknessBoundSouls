#!/bin/sh
# Lights per room, stage 2 (design-lights.md H and R1: two lamp sets, one shadow and haze Frame per view, a camera per view).
# Apart cases twice with the same presses, ED_LIGHTS_OFF=1 (today) then on. AURORA_SHADOW_LOG for [shadow-view] and
# [lights] lines, the probe for `ed flicker`. Built already. Run from Git Bash.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_SHOW_TRACE=1 ED_RUN_SHOTS_MS=200 AURORA_SHADOW_LOG=30
# S1: Jump to Game, chapter 1: player 1 by 101 to 343, player 2 stays in 341; both walk.
for mode in off on; do
  if [ $mode = off ]; then export ED_LIGHTS_OFF=1; else unset ED_LIGHTS_OFF; fi
  JTG_SLOT=1 AURORA_SHADOW_PROBE=logs/probe-lights2-$mode-341.txt ED_RUN_SCRIPT=1950:101,2010:101 JTG_PUT="400,2,-245,-90,16" \
    JTG_P2_SCRIPT="500,620,lleft;700,820,lright" JTG_P1_SCRIPT="500,620,0,-127,0" JTG_FRAMES=1000 \
    sh tools/ed/jtg.sh 1 lights2-$mode-341-343 </dev/null
done
unset ED_LIGHTS_OFF
# S2: together in 341 (nothing new may run). S3: solo.
JTG_SLOT=1 AURORA_SHADOW_PROBE=logs/probe-lights2-together.txt JTG_P1_SCRIPT="100,280,0,0,127" JTG_P2_SCRIPT="100,280,lup" JTG_FRAMES=500 sh tools/ed/jtg.sh 1 lights2-together </dev/null
JTG_SLOT=1 JTG_SET="mod_coop=0" AURORA_SHADOW_PROBE=logs/probe-lights2-solo.txt JTG_P1_SCRIPT="100,280,0,0,127" JTG_FRAMES=500 sh tools/ed/jtg.sh 1 lights2-solo </dev/null
# S4: room 183's zombies apart (player 1 by 119 to 180, player 2 among them), then player 2's own door 183 -> 180 (put at
# it, B) into player 1's room: together again (set B off, per view off). Off then on.
rm -f saves/rooms-ch1-183.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 lights2-s0-state </dev/null
SET=$(ls -d logs/run-*-jtg-lights2-s0-state | tail -1)/settings.ini
for mode in off on; do
  if [ $mode = off ]; then export ED_LIGHTS_OFF=1; else unset ED_LIGHTS_OFF; fi
  ED_SETTINGS="$SET" AURORA_SHADOW_PROBE=logs/probe-lights2-$mode-183.txt ED_RUN_FRAMES=5000 ED_RUN_SECONDS=160 \
    ED_PUT="3680,1,1305,1508,185;3680,2,1250,1650,185;4500,2,360,3090,185" ED_RUN_SCRIPT=110:119,170:119 \
    ED_PAD2_SCRIPT="4000,4150,lup;4300,4450,ldown;4560,4570,b" sh tools/ed/run.sh saves/rooms-ch1-183.state lights2-$mode-183-zombies-door </dev/null
done
unset ED_LIGHTS_OFF
echo RUNS DONE
