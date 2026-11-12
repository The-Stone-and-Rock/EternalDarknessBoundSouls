#!/bin/sh
cd "/e/Eternal Darkness Recomp Fork" || exit 1
S="C:/Users/stevi/AppData/Local/Temp/claude/8a0b49d5cd35decf/73f71524-55f9-4337-bea8-8369f9fa7835/scratchpad/rooms"
cmd //c "$(cygpath -w tools/ed/build-win.bat)" > "$S/build-r4.log" 2>&1 || { echo BUILD FAILED; exit 1; }
grep -q "FAILED" "$S/build-r4.log" && { echo BUILD FAILED; exit 1; }
export ED_ROOM_SLOTS=2 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_SHOW_TRACE=1 ED_FIN_LOG=1 ED_HEAP_CHECK=2000 ED_RUN_SHOTS_MS=200
rm -f saves/rooms-ch1-183.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 rooms-r4-state </dev/null
ls -la saves/rooms-ch1-183.state || exit 2
export ED_COOP_STAY=1
ST=saves/rooms-ch1-183.state
# t1: player 2 at the door's region presses OPEN DOOR (119) at 3760: their change; player 1 walks in 183 (3900-4000), player 2 walks in 180
# (4100-4200); player 2 presses B again at 4300 (the way back, 29, if they stand in its zone); both walk after.
ED_RUN_FRAMES=4900 ED_RUN_SECONDS=140 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3760,3770,b;4100,4200,lup;4300,4310,b;4500,4600,lup" ED_INPUT_SCRIPT="3900,4000,0,0,127;4500,4600,0,0,-127,0" sh tools/ed/run.sh $ST rooms-r4-t1-p2door </dev/null
# t2: player 2's door (3760) to 180, then player 1's door 119 at ~4150 into 180 where player 2 is: the same room again.
ED_RUN_FRAMES=4900 ED_RUN_SECONDS=140 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_RUN_SCRIPT=500:119,560:119 ED_PAD2_SCRIPT="3760,3770,b;4100,4200,lup;4500,4600,lleft" ED_INPUT_SCRIPT="3900,4000,0,0,127;4500,4600,0,0,-127,0" sh tools/ed/run.sh $ST rooms-r4-t2-p2door-p1follows </dev/null
echo RUNS DONE
