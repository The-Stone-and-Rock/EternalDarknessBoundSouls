#!/bin/sh
# Round 2 of the arrival camera's checks, on the build with the put fix (ED_PUT finds the floor of the put player's own
# room: camera_collide_object). States remade on this build.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_SHOW_TRACE=1 ED_FIN_LOG=1 ED_HEAP_CHECK=2000 ED_RUN_SHOTS_MS=200
rm -f saves/rooms-ch1-183.state saves/rooms-ch1-341-apart.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 cam2-s0-state </dev/null
ls -la saves/rooms-ch1-183.state || exit 2
export ED_COOP_STAY=1 ED_SETTINGS=$(ls -d logs/run-*-jtg-cam2-s0-state | tail -1)/settings.ini
# C3 (R4 t2's recipe): player 2's door 183 -> 180 at 3760, then player 1's door 119 at ~4150 into 180 where player 2 is.
ED_RUN_FRAMES=4900 ED_RUN_SECONDS=140 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_RUN_SCRIPT=500:119,560:119 ED_PAD2_SCRIPT="3760,3770,b;4100,4200,lup;4500,4600,lleft" ED_INPUT_SCRIPT="3900,4000,0,0,127;4500,4600,0,0,-127,0" sh tools/ed/run.sh saves/rooms-ch1-183.state cam2-c3-p2door-p1follows </dev/null
unset ED_SETTINGS
JTG_SLOT=1 ED_RUN_SCRIPT=1950:101,2010:101,2600:9,2660:9 ED_SAVE_STATE=2300:saves/rooms-ch1-341-apart.state ED_RUN_FRAMES=3300 sh tools/ed/jtg.sh 1 cam2-d-jtg-stay </dev/null
ls -la saves/rooms-ch1-341-apart.state || exit 3
ED_SETTINGS=$(ls -d logs/run-*-jtg-cam2-d-jtg-stay | tail -1)/settings.ini ED_RUN_FRAMES=3500 ED_RUN_SECONDS=120 \
  ED_PUT="2400,2,-167,-4,0" ED_PAD2_SCRIPT="2480,2490,b;2620,2630,b;3000,3100,lup" ED_INPUT_SCRIPT="2700,2800,0,0,127" \
  sh tools/ed/run.sh saves/rooms-ch1-341-apart.state cam2-lf-load-p2door </dev/null
echo RUNS DONE
