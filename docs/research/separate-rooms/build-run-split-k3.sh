#!/bin/sh
# design-split-hold.md K3-off: measure the apart shape before any change (today's build). Player 1 by 119 to 180, then the
# ladder 28 in 180 (to 344) while player 2 stays in 183 with the zombies, attacking. Read room 183's zombie rows through
# player 1's ladder countdown: step 0.0 (held) or moving.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_CHANGE_TIME=1 ED_RUN_SHOTS_MS=250
rm -f saves/rooms-ch1-183.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 split-s0-183 </dev/null
SET=$(ls -d logs/run-*-jtg-split-s0-183 | tail -1)/settings.ini; export ED_SETTINGS="$SET"
env ED_COOP_ENEMY_LOG=1 ED_CHANGE_HOLD=1 ED_HURT_LOG=1 ED_COOP_LOG=1 ED_RUN_FRAMES=4700 ED_PUT="3680,1,1305,1508,185;3680,2,1110,1170,185" \
  ED_RUN_SCRIPT=110:119,170:119,420:28,480:28 ED_PAD2_SCRIPT="4100,4500,r;4300,4305,a;4330,4335,a" \
  sh tools/ed/run.sh saves/rooms-ch1-183.state split-k3-apart-today </dev/null
echo RUNS DONE
