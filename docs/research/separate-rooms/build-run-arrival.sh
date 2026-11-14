#!/bin/sh
# The arrival camera's checks (design-arrival-camera.md section 9), chained so the game opens five times, not ten.
# Built already (cmake --build build-win). Run from Git Bash: sh docs/research/separate-rooms/build-run-arrival.sh
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_SHOW_TRACE=1 ED_FIN_LOG=1 ED_HEAP_CHECK=2000 ED_RUN_SHOTS_MS=200
rm -f saves/rooms-ch1-183.state saves/rooms-ch1-341-apart.state
# S0: the together state in 183 made WITHOUT the stay (player 2 brought through 101, 105, 111, 29); also the no-stay check.
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 cam-s0-state </dev/null
ls -la saves/rooms-ch1-183.state || exit 2
SET=$(ls -d logs/run-*-jtg-cam-s0-state | tail -1)/settings.ini
export ED_COOP_STAY=1 ED_SETTINGS="$SET"
ST=saves/rooms-ch1-183.state
# A (C1 + C2 + case D + C3 in one run), frames absolute, ED_RUN_SCRIPT delays from the load (~3650):
#  3760 player 2's door 183 -> 180 (C1), 4300 their way back into 183 (C1 back, rejoin);
#  ~4750 player 1's door 119 183 -> 180, player 2 stays in 183 (C2); ~5150 player 1's door 29 back into 183, where player 2
#  is (case D, H8); 5440 player 2 put at 183's door, 5520 their door 183 -> 180 (C1 again), ~5850 player 1 follows by 119
#  into 180 where player 2 is (C3, H8). Walks between so both halves move.
ED_RUN_FRAMES=6400 ED_RUN_SECONDS=220 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185;5440,2,360,3090,185" \
  ED_RUN_SCRIPT=1100:119,1160:119,1500:29,1560:29,2200:119,2260:119 \
  ED_PAD2_SCRIPT="3760,3770,b;4100,4200,lup;4300,4310,b;4500,4600,lup;5520,5530,b;5650,5750,lup;6000,6100,lleft" \
  ED_INPUT_SCRIPT="3900,4000,0,0,127;4500,4600,0,0,-127,0;5300,5380,0,127,0" sh tools/ed/run.sh $ST cam-a-doors </dev/null
# D (J1): Jump to Game with the stay: player 2 stays bare in 341 while player 1 goes 341 -> 342 -> 343 (the JTG route's
# doors as in the earlier runs) and comes back into 341; the apart state saved on the way.
unset ED_SETTINGS
# 101 run from 341 takes player 1 to 343; 9 (342's CLIMB UP, run twice as a prompted door) brings them back into 341.
JTG_SLOT=1 ED_RUN_SCRIPT=1950:101,2010:101,2600:9,2660:9 ED_SAVE_STATE=2300:saves/rooms-ch1-341-apart.state ED_RUN_FRAMES=3300 sh tools/ed/jtg.sh 1 cam-d-jtg-stay </dev/null
ls -la saves/rooms-ch1-341-apart.state || exit 3
# L + F: load the apart state (player 2's set remade from player 1's while apart, then cut at their first own camera: H9,
# H10), then player 2's own door 341 -> 342 through Region01 (103: EXAMINE then DETAIL, the user's stuck spot of play 1).
ED_SETTINGS=$(ls -d logs/run-*-jtg-cam-d-jtg-stay | tail -1)/settings.ini ED_RUN_FRAMES=3500 ED_RUN_SECONDS=120 \
  ED_PUT="2400,2,-167,-4,0" ED_PAD2_SCRIPT="2480,2490,b;2620,2630,b;3000,3100,lup" ED_INPUT_SCRIPT="2700,2800,0,0,127" \
  sh tools/ed/run.sh saves/rooms-ch1-341-apart.state cam-lf-load-p2door </dev/null
# J2: solo Jump to Game (co-op off): the switches must be off for the run.
JTG_SLOT=1 JTG_SET="mod_coop=0" ED_RUN_FRAMES=2600 sh tools/ed/jtg.sh 1 cam-j2-solo </dev/null
echo RUNS DONE
# Round 2, after the put fix (camera_collide_object): build-run-arrival-2.sh
