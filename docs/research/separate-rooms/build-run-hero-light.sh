#!/bin/sh
# Player 2 lit as a hero (lights_hero_budget): beside 341's lamp at 880,-11 (amber), off (ED_P2_HERO_LIGHT=0) then on.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_COOP_LOG=1 ED_RUN_SHOTS_MS=200
for mode in off on; do
  if [ $mode = off ]; then export ED_P2_HERO_LIGHT=0; else unset ED_P2_HERO_LIGHT; fi
  # together in 341, both by the lamp
  JTG_SLOT=1 JTG_PUT="200,1,760,90,0;200,2,760,-110,0" JTG_P1_SCRIPT="300,330,0,127,0" JTG_P2_SCRIPT="300,330,lright" JTG_FRAMES=600 sh tools/ed/jtg.sh 1 hero-$mode-together </dev/null
  # apart: player 1 to 343, player 2 by the lamp in 341
  JTG_SLOT=1 ED_RUN_SCRIPT=1950:101,2010:101 JTG_PUT="400,2,760,-110,0" JTG_P2_SCRIPT="500,530,lright" JTG_FRAMES=800 sh tools/ed/jtg.sh 1 hero-$mode-apart </dev/null
done
unset ED_P2_HERO_LIGHT
JTG_SLOT=1 JTG_SET="mod_coop=0" JTG_PUT="200,1,760,90,0" JTG_FRAMES=500 sh tools/ed/jtg.sh 1 hero-solo </dev/null
echo RUNS DONE
