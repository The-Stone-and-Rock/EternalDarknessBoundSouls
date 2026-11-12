#!/bin/sh
# (The build: cmd //c "$(cygpath -w tools/ed/build-win.bat)". Run from a state whose making run shows hold 0x0.)
# R6 checks on the R6 build, from saves/rooms-ch1-183.state (a host-only change: the state loads).
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_COOP_ENEMY_LOG=1 ED_HURT_LOG=1 ED_SHOW_TRACE=1 ED_HEAP_CHECK=2000 ED_FIN_LOG=1 ED_RUN_SHOTS_MS=250
ST=saves/rooms-ch1-183.state
# c1 player 1 to 180; player 2 stands still among room 183's zombies: they hit player 2.
ED_RUN_FRAMES=5400 ED_RUN_SECONDS=160 ED_PUT="3680,2,1305,1508,185" ED_RUN_SCRIPT=110:119,170:119 sh tools/ed/run.sh $ST r6b-c1-p1away </dev/null
# c2 player 2 to 180; player 1 stays among them: player 1 hit, player 2 untouched (as base2-c2).
ED_RUN_FRAMES=5200 ED_RUN_SECONDS=150 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3760,3770,b;4100,4200,lup;4400,4500,ldown" ED_INPUT_SCRIPT="3900,4000,0,0,127" sh tools/ed/run.sh $ST r6b-c2-p2away </dev/null
# c3 player 1 to 180; player 2 waits for the zombies, then aims (R) and swings (A).
ED_RUN_FRAMES=5800 ED_RUN_SECONDS=170 ED_PUT="3680,2,1305,1508,185" ED_RUN_SCRIPT=110:119,170:119 ED_PAD2_SCRIPT="4150,5700,r;4200,4210,a;4320,4330,a;4440,4450,a;4560,4570,a;4680,4690,a;4800,4810,a;4920,4930,a;5040,5050,a;5160,5170,a;5280,5290,a;5400,5410,a;5520,5530,a" sh tools/ed/run.sh $ST r6b-c3-p2swings </dev/null
# c4 together, no presses (as base2-c4).
ED_RUN_FRAMES=5200 ED_RUN_SECONDS=150 ED_PUT="3680,1,1305,1508,185;3680,2,1250,1650,185" sh tools/ed/run.sh $ST r6b-c4-together </dev/null
# c5 together, player 2 swings at the zombies (the same-room path, unchanged).
ED_RUN_FRAMES=5800 ED_RUN_SECONDS=170 ED_PUT="3680,1,1250,1650,185;3680,2,1305,1508,185" ED_PAD2_SCRIPT="4150,5700,r;4200,4210,a;4320,4330,a;4440,4450,a;4560,4570,a;4680,4690,a;4800,4810,a;4920,4930,a;5040,5050,a;5160,5170,a;5280,5290,a;5400,5410,a;5520,5530,a" sh tools/ed/run.sh $ST r6b-c5-together-p2swings </dev/null
# c1n: player 1 to 180; then player 2 put on the floor near zombies 39/40 (about 300 units) and stands: they hit player 2.
ED_RUN_FRAMES=5400 ED_RUN_SECONDS=160 ED_PUT="3680,2,760,1050,20" ED_RUN_SCRIPT=110:119,170:119 sh tools/ed/run.sh $ST r6c-c1n-p1away-near </dev/null
# c3n: the same, and player 2 aims (R) and swings (A) at them.
ED_RUN_FRAMES=5800 ED_RUN_SECONDS=170 ED_PUT="3680,2,760,1050,20" ED_RUN_SCRIPT=110:119,170:119 ED_PAD2_SCRIPT="4050,5700,r;4100,4110,a;4220,4230,a;4340,4350,a;4460,4470,a;4580,4590,a;4700,4710,a;4820,4830,a;4940,4950,a;5060,5070,a;5180,5190,a;5300,5310,a;5420,5430,a;5540,5550,a" sh tools/ed/run.sh $ST r6c-c3n-p2swings-near </dev/null
echo RUNS DONE
