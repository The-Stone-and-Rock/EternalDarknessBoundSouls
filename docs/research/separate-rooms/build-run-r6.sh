#!/bin/sh
# R6 checks: the state remade from Jump to Game on the build (the R4 recipe), then three runs with the enemy log on.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
S="C:/Users/stevi/AppData/Local/Temp/claude/8a0b49d5cd35decf/73f71524-55f9-4337-bea8-8369f9fa7835/scratchpad/rooms"
cmd //c "$(cygpath -w tools/ed/build-win.bat)" > "$S/build-r6.log" 2>&1 || { echo BUILD FAILED; exit 1; }
grep -q "FAILED" "$S/build-r6.log" && { echo BUILD FAILED; exit 1; }
export ED_ROOM_SLOTS=2 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_COOP_ENEMY_LOG=1 ED_SHOW_TRACE=1 ED_FIN_LOG=1 ED_HEAP_CHECK=2000 ED_RUN_SHOTS_MS=250
rm -f saves/rooms-ch1-183.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 rooms-r6-state </dev/null
ls -la saves/rooms-ch1-183.state || exit 2
export ED_COOP_STAY=1
ST=saves/rooms-ch1-183.state
# c1: player 1 to 180 (119 twice) while player 2 stays in 183 among its zombies for 1200 frames: the zombies go after player 2
# (room 183) and may hurt them; none goes after player 1 (room 180); player 1's health never moves.
ED_RUN_FRAMES=5200 ED_RUN_SECONDS=150 ED_PUT="3680,2,1305,1508,185" ED_RUN_SCRIPT=110:119,170:119 ED_PAD2_SCRIPT="3900,4000,lup;4200,4300,ldown" sh tools/ed/run.sh $ST rooms-r6-c1-p1away </dev/null
# c2: player 2's door to 180 while player 1 stays in 183 among its zombies: they go after player 1 only; player 2's health never moves.
ED_RUN_FRAMES=5200 ED_RUN_SECONDS=150 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3760,3770,b;4100,4200,lup;4400,4500,ldown" ED_INPUT_SCRIPT="3900,4000,0,0,127" sh tools/ed/run.sh $ST rooms-r6-c2-p2away </dev/null
# c3: player 1 to 180; player 2 in 183 aims (R) and swings (A) at the zombies: a zombie's health changes ([coop] N's health a -> b).
ED_RUN_FRAMES=5200 ED_RUN_SECONDS=150 ED_PUT="3680,2,1305,1508,185" ED_RUN_SCRIPT=110:119,170:119 ED_PAD2_SCRIPT="3900,4700,r;3950,3960,a;4100,4110,a;4250,4260,a;4400,4410,a;4550,4560,a" sh tools/ed/run.sh $ST rooms-r6-c3-p2swings </dev/null
echo RUNS DONE
