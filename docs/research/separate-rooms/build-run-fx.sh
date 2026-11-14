#!/bin/sh
# Effects per room (ED_FX_ROOMS; ED_FX_FACING): fresh Jump-to-Game states made on this build first, then
#  A  Karim (ch 4, room 338, the altar flame under the dome): player 1 down the ladder (script 132, to room 5), player 2 stays.
#  C  Karim together, player 2 seeing the altar from the side (their own camera): each half's cards face its camera.
#  E  Chapter 1: player 1 in 183's pit, player 2 through the door to 180 (180 entered apart: its braziers must burn).
# Each with the fix on (1) and off (0). No ed test, no ed chapters (old saves).
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_ROOM_LOG=1 ED_RUN_SHOTS_MS=250 ED_RUN_SHOTS_W=${ED_RUN_SHOTS_W:-1920}
rm -f saves/fx-ch4.state saves/rooms-ch1-183.state
JTG_SLOT=1 ED_SAVE_STATE=2700:saves/fx-ch4.state ED_RUN_FRAMES=2800 sh tools/ed/jtg.sh 4 fx-s0-ch4 </dev/null
JTG_SLOT=1 ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 \
  ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 fx-s0-183 </dev/null
[ -f saves/fx-ch4.state ] && [ -f saves/rooms-ch1-183.state ] || { echo "no fresh state"; exit 1; }
S4=$(ls -d logs/run-*-jtg-fx-s0-ch4 | tail -1)/settings.ini; S1=$(ls -d logs/run-*-jtg-fx-s0-183 | tail -1)/settings.ini
export ED_COOP_STAY=1 ED_COOP_LOG=1
for F in ${FX_RUNS:-1 0}; do
  ED_SETTINGS="$S4" ED_FX_ROOMS=$F ED_RUN_SCRIPT=60:132,120:132 ED_RUN_FRAMES=3400 sh tools/ed/run.sh saves/fx-ch4.state fx-a-$F </dev/null
  ED_SETTINGS="$S4" ED_FX_FACING=$F ED_PUT="2730,2,1300,750,10" ED_PAD2_SCRIPT="2760,2785,ldown" ED_RUN_FRAMES=2950 sh tools/ed/run.sh saves/fx-ch4.state fx-c-side-$F </dev/null
  ED_SETTINGS="$S1" ED_FX_ROOMS=$F ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3760,3770,b" \
    sh tools/ed/run.sh saves/rooms-ch1-183.state fx-e-180-$F </dev/null
done
echo RUNS DONE
