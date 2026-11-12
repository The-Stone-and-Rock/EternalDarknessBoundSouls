#!/bin/sh
cd "/e/Eternal Darkness Recomp Fork" || exit 1
S="C:/Users/stevi/AppData/Local/Temp/claude/8a0b49d5cd35decf/73f71524-55f9-4337-bea8-8369f9fa7835/scratchpad/rooms"
cmd //c "$(cygpath -w tools/ed/build-win.bat)" > "$S/build-r5.log" 2>&1 || { echo BUILD FAILED; exit 1; }
grep -q "FAILED" "$S/build-r5.log" && { echo BUILD FAILED; exit 1; }
export ED_ROOM_SLOTS=2 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_SHOW_TRACE=1 ED_FIN_LOG=1 ED_HEAP_CHECK=2000 ED_RUN_SHOTS_MS=200
rm -f saves/rooms-ch1-183.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 rooms-r5-state </dev/null
ls -la saves/rooms-ch1-183.state || exit 2
export ED_COOP_STAY=1
ST=saves/rooms-ch1-183.state
# s1: player 2's door 183 -> 180 at 3760 while player 1 stays in 183; walks; the way back at 4300.
ED_RUN_FRAMES=4900 ED_RUN_SECONDS=140 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3760,3770,b;4100,4200,lup;4300,4310,b;4500,4600,lup" ED_INPUT_SCRIPT="3900,4000,0,0,127;4500,4600,0,0,-127,0" sh tools/ed/run.sh $ST rooms-r5-s1-p2door </dev/null
# s2: player 1's door 119 (183 -> 180) at ~3760 while player 2 stays in 183; player 1 back through 29 at ~4300.
ED_RUN_FRAMES=4900 ED_RUN_SECONDS=140 ED_PUT="3680,2,1305,1508,185" ED_RUN_SCRIPT=110:119,170:119,650:29,710:29 ED_PAD2_SCRIPT="3900,4000,lup;4500,4600,lleft" ED_INPUT_SCRIPT="4100,4200,0,0,127" sh tools/ed/run.sh $ST rooms-r5-s2-p1door </dev/null
# s3: player 2 opens their Tome (start) and navigates (right, right, left), then closes (start); player 1 idle in 183.
ED_RUN_FRAMES=4600 ED_RUN_SECONDS=120 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3760,3765,start;3900,3905,right;4000,4005,right;4100,4105,left;4200,4205,start" sh tools/ed/run.sh $ST rooms-r5-s3-tome </dev/null
echo RUNS DONE
