#!/bin/sh
# The change freeze, Phase 1 (design-change-freeze.md P1.1-P1.3, log only): measure what the stayer loses at the other's door.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_CHANGE_TIME=1 DOL_GX_TIME=1 ED_RUN_SHOTS_MS=250
rm -f saves/rooms-ch1-183.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 freeze-s0-state </dev/null
SET=$(ls -d logs/run-*-jtg-freeze-s0-state | tail -1)/settings.ini; export ED_SETTINGS="$SET"; ST=saves/rooms-ch1-183.state
# M1: player 1 leaves 183 for 180 (a split), player 2 stays and turns their camera.
ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,1250,1650,185" ED_RUN_SCRIPT=110:119,170:119 ED_PAD2_SCRIPT="3700,4100,rright" sh tools/ed/run.sh $ST freeze-m1-p1-leaves </dev/null
# M3: as M1 with player 2 among the zombies swinging (B) and a quick spell (up) through the countdown, with the hold rows.
ED_COOP_ENEMY_LOG=1 ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,1250,1650,185" ED_RUN_SCRIPT=110:119,170:119 ED_PAD2_SCRIPT="3700,3705,b;3740,3745,b;3770,3775,b;3790,3795,up;3810,3815,b;3850,3855,b" sh tools/ed/run.sh $ST freeze-m3-p1-leaves-fight </dev/null
# M2: player 2 leaves (their door, B at 3760), player 1 walks through the ticket.
ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3760,3770,b" ED_INPUT_SCRIPT="3700,4100,0,0,127" sh tools/ed/run.sh $ST freeze-m2-p2-leaves </dev/null
unset ED_SETTINGS
# M5: apart -> apart: player 1 341 -> 343 (101), then on by 105 while player 2 stays in 341 and walks.
JTG_SLOT=1 ED_RUN_SCRIPT=1950:101,2010:101,2400:105,2460:105 JTG_P2_SCRIPT="450,700,lleft" JTG_FRAMES=1000 sh tools/ed/jtg.sh 1 freeze-m5-apart-apart </dev/null
# M8: solo, the same doors.
JTG_SLOT=1 JTG_SET="mod_coop=0" ED_RUN_SCRIPT=1950:101,2010:101,2400:105,2460:105 JTG_FRAMES=1000 sh tools/ed/jtg.sh 1 freeze-m8-solo </dev/null
echo RUNS DONE
