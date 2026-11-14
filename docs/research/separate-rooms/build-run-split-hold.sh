#!/bin/sh
# The stayer's room plays on through the other's door countdown (design-split-hold.md sections 14, 18, 19).
# Fresh states made on this build first. Each case is run with the rule on; K1 and K3 also with ED_SPLIT_HOLD=0.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_CHANGE_TIME=1 ED_RUN_SHOTS_MS=250
L="ED_COOP_ENEMY_LOG=1 ED_CHANGE_HOLD=1 ED_HURT_LOG=1 ED_COOP_LOG=1"
rm -f saves/rooms-ch1-183.state saves/rooms-ch1-344.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 split-s0-183 </dev/null
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105 ED_SAVE_STATE=2650:saves/rooms-ch1-344.state ED_RUN_FRAMES=2750 sh tools/ed/jtg.sh 1 split-s0-344 </dev/null
SET=$(ls -d logs/run-*-jtg-split-s0-183 | tail -1)/settings.ini; export ED_SETTINGS="$SET"; ST=saves/rooms-ch1-183.state
P2FIGHT="3700,3900,r;3790,3795,a;3805,3810,a;3820,3825,a;3835,3840,a;3850,3855,a;3865,3870,a;3800,3805,up"
PUT1="3680,1,1305,1508,185;3680,2,1110,1170,185"
# K3: apart (the user's report): player 1 to 180, then 180's ladder 28 while player 2 stays in 183 with the zombies.
env $L ED_RUN_FRAMES=4700 ED_PUT="$PUT1" ED_RUN_SCRIPT=110:119,170:119,420:28,480:28 ED_PAD2_SCRIPT="4100,4500,r;4120,4125,a;4150,4155,a;4180,4185,a" sh tools/ed/run.sh $ST split-k3-apart-on </dev/null
env $L ED_SPLIT_HOLD=0 ED_RUN_FRAMES=4700 ED_PUT="$PUT1" ED_RUN_SCRIPT=110:119,170:119,420:28,480:28 ED_PAD2_SCRIPT="4100,4500,r;4120,4125,a;4150,4155,a;4180,4185,a" sh tools/ed/run.sh $ST split-k3-apart-off </dev/null
# K1: shared room, player 1 leaves 183, player 2 among the zombies attacking. K1-off the same with the rule off.
env $L ED_RUN_FRAMES=4300 ED_PUT="$PUT1" ED_RUN_SCRIPT=110:119,170:119 ED_PAD2_SCRIPT="$P2FIGHT" sh tools/ed/run.sh $ST split-k1-p1-leaves </dev/null
env $L ED_SPLIT_HOLD=0 ED_RUN_FRAMES=4300 ED_PUT="$PUT1" ED_RUN_SCRIPT=110:119,170:119 ED_PAD2_SCRIPT="$P2FIGHT" sh tools/ed/run.sh $ST split-k1-off </dev/null
# K1k: attacking from 3690 for a kill inside the window.
env $L ED_RUN_FRAMES=4300 ED_PUT="$PUT1" ED_RUN_SCRIPT=110:119,170:119 ED_PAD2_SCRIPT="3690,3900,r;3700,3705,a;3720,3725,a;3740,3745,a;3760,3765,a;3780,3785,a;3800,3805,a;3820,3825,a;3840,3845,a;3860,3865,a" sh tools/ed/run.sh $ST split-k1k-kill </dev/null
# K1b: player 1 walks to the door through the zombies and presses B; creatures kept on the mover (testing switch).
env $L ED_SPLIT_TARGET_MOVER=1 ED_RUN_FRAMES=4300 ED_PUT="3680,1,360,3090,185;3680,2,1305,1508,185" ED_INPUT_SCRIPT="3700,3760,0,0,127;3790,3800,b" sh tools/ed/run.sh $ST split-k1b-mover-hit </dev/null
# K2: player 2 leaves (B at the door), player 1 stays walking. K2b: creatures kept on player 2, the mover.
env $L ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3760,3770,b" ED_INPUT_SCRIPT="3700,4100,0,0,127" sh tools/ed/run.sh $ST split-k2-p2-leaves </dev/null
env $L ED_SPLIT_TARGET_MOVER=1 ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3700,3760,up;3790,3800,b" sh tools/ed/run.sh $ST split-k2b-mover-hit </dev/null
# K4: a ladder split in 344. K5: follow mode (nothing new). K6: solo (nothing new).
env $L ED_RUN_FRAMES=3000 ED_RUN_SCRIPT=110:111,170:111 sh tools/ed/run.sh saves/rooms-ch1-344.state split-k4-ladder </dev/null
env -u ED_COOP_STAY $L ED_RUN_FRAMES=4300 ED_PUT="$PUT1" ED_RUN_SCRIPT=110:119,170:119 sh tools/ed/run.sh $ST split-k5-follow </dev/null
unset ED_SETTINGS
JTG_SLOT=1 JTG_SET="mod_coop=0" env $L ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29,3820:119,3880:119 ED_RUN_FRAMES=4300 sh tools/ed/jtg.sh 1 split-k6-solo </dev/null
echo RUNS DONE
