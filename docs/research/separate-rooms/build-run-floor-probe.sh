#!/bin/sh
# The floor probe (design-fall.md section 6, log only). P1: a real fall the probe must name: player 2 apart in 341 put at
# the hole's edge (Region01, the room's own way down to 342) slides to its bottom; "[floor] falling" expected, no "under".
# P2: doors apart in 183/180 with walking and the zombies: no [floor] line expected (the probe must stay quiet in play).
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_SHOW_TRACE=1 ED_RUN_SHOTS_MS=200
JTG_SLOT=1 ED_RUN_SCRIPT=1950:101,2010:101 JTG_PUT="400,2,-167,-4,0" JTG_P2_SCRIPT="600,700,lup" JTG_FRAMES=900 \
  sh tools/ed/jtg.sh 1 floor-p1-hole </dev/null
rm -f saves/rooms-ch1-183.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 floor-s0-state </dev/null
SET=$(ls -d logs/run-*-jtg-floor-s0-state | tail -1)/settings.ini
ED_SETTINGS="$SET" ED_RUN_FRAMES=5400 ED_RUN_SECONDS=180 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" \
  ED_RUN_SCRIPT=1100:119,1160:119,1500:29,1560:29 \
  ED_PAD2_SCRIPT="3760,3770,b;4100,4200,lup;4300,4310,b;4500,4600,lup;5000,5150,lleft" \
  ED_INPUT_SCRIPT="3900,4000,0,0,127;4500,4600,0,0,-127,0;4900,5000,0,127,0" sh tools/ed/run.sh saves/rooms-ch1-183.state floor-p2-doors </dev/null
echo RUNS DONE
