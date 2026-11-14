#!/bin/sh
# Lights per room, stage 1 (design-lights.md G1-G3: the game's own lighting of player 2's room by that room). Each apart
# case runs twice with the same presses, ED_LIGHTS_OFF=1 (today's lighting) then on, so the halves compare picture to
# picture. Built already. Run from Git Bash: sh docs/research/separate-rooms/build-run-lights1.sh
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_SHOW_TRACE=1 ED_RUN_SHOTS_MS=200
# L1: Jump to Game, chapter 1: player 1 by 101 (run from 341 it takes them to 343; 103, the ladder, run by the host only opens
# its EXAMINE close-up), player 2 stays in 341 (blue-grey lamps); both walk.
for mode in off on; do
  if [ $mode = off ]; then export ED_LIGHTS_OFF=1; else unset ED_LIGHTS_OFF; fi
  JTG_SLOT=1 ED_RUN_SCRIPT=1950:101,2010:101 JTG_PUT="400,2,-245,-90,16" \
    JTG_P2_SCRIPT="500,620,lleft;700,820,lright" JTG_P1_SCRIPT="500,620,0,-127,0" JTG_FRAMES=1000 \
    sh tools/ed/jtg.sh 1 lights1-$mode-341-343 </dev/null
done
unset ED_LIGHTS_OFF
# L2: together in 341, no door (nothing new may run): both walk.
JTG_SLOT=1 JTG_P1_SCRIPT="100,280,0,0,127" JTG_P2_SCRIPT="100,280,lup" JTG_FRAMES=500 sh tools/ed/jtg.sh 1 lights1-together </dev/null
# L3: solo.
JTG_SLOT=1 JTG_SET="mod_coop=0" JTG_P1_SCRIPT="100,280,0,0,127" JTG_FRAMES=500 sh tools/ed/jtg.sh 1 lights1-solo </dev/null
# L4: room 183's zombies apart: the state in 183 (made without the stay), player 1 by 119 to 180, player 2 stays among the
# zombies, off then on.
rm -f saves/rooms-ch1-183.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 lights1-s0-state </dev/null
SET=$(ls -d logs/run-*-jtg-lights1-s0-state | tail -1)/settings.ini
for mode in off on; do
  if [ $mode = off ]; then export ED_LIGHTS_OFF=1; else unset ED_LIGHTS_OFF; fi
  ED_SETTINGS="$SET" ED_RUN_FRAMES=4700 ED_RUN_SECONDS=140 ED_PUT="3680,1,1305,1508,185;3680,2,1250,1650,185" ED_RUN_SCRIPT=110:119,170:119 \
    ED_PAD2_SCRIPT="4000,4150,lup;4300,4450,ldown" sh tools/ed/run.sh saves/rooms-ch1-183.state lights1-$mode-183-zombies </dev/null
done
unset ED_LIGHTS_OFF
echo RUNS DONE
