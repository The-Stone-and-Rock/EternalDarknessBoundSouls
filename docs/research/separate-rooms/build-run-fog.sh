#!/bin/sh
# The room mist per room, players apart (design-fog.md, section 8): Part A (ED_FOG_ROOMS, ED_FOG_LISTS, ED_FOG_KEEP) and
# Part B (ED_FOG_VIEWS=1, the renderer's depth per half). One fresh Jump-to-Game state, made first on this build, for all.
# No 'ed test' and no 'ed chapters' here: both load old slot saves (saves/slot1.state and the scenarios'), and old states
# are not to be run (the user, 2026-11-13). The 'ed chapters' block at the end runs only with CHAPTERS=1.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
ED="python -I tools/ed/ed.py"
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_RUN_SHOTS_MS=250 ED_FOG_LOG=1
L="ED_COOP_LOG=1 ED_STAYER_LOG=1"
rm -f saves/rooms-ch1-183.state saves/fog-apart-off.state
mkdir -p build && touch build/fog-runs.stamp   # (the summary at the end: only this script's runs)
# S0 the fresh state: Jump to Game, chapter 1, the doors to 183 (as build-run-stayer-presses.sh), saved at 3650.
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 \
  ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 fog-s0-183 </dev/null
[ -f saves/rooms-ch1-183.state ] || { echo "no fresh state: saves/rooms-ch1-183.state was not written"; exit 1; }
SET=$(ls -d logs/run-*-jtg-fog-s0-183 | tail -1)/settings.ini; export ED_SETTINGS="$SET"; ST=saves/rooms-ch1-183.state
P1GO="ED_RUN_SCRIPT=110:119,170:119"     # player 1's door 183 -> 180, ticket ~3822
D2="3760,3770,b"                          # player 2's OPEN DOOR at 183's door to 180
for F in 0 1 2; do
  # F1: player 1 in 183's pit, player 2 to 180 (183 in play, 180 is room1).
  env $L ED_FOG_ROOMS=$F ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" \
    ED_PAD2_SCRIPT="$D2" sh tools/ed/run.sh $ST fog-f1-p1-haze-$F </dev/null
  # F2: player 2 in 183's pit, player 1 to 180 (180 in play, 183 is room1).
  env $L $P1GO ED_FOG_ROOMS=$F ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1250,1650,185" \
    sh tools/ed/run.sh $ST fog-f2-p2-haze-$F </dev/null
done
# F3 together in 183's pit; F4 follow into 180: on and off must match (no scope); Part B changes player 2's half only.
# (F3-r0: ED_FOG_ROOMS=0, the off run F3-v0 is compared with. Together, A4 still clears the room in play's list once after
# the state load, section 2, made again as its own: one 'cleared' line, nothing else.)
env $L ED_FOG_ROOMS=0 ED_FOG_VIEWS=0 ED_RUN_FRAMES=4100 ED_PUT="3680,1,1305,1508,185;3680,2,1250,1650,185" sh tools/ed/run.sh $ST fog-f3-together-183-r0 </dev/null
for V in 0 1; do
  env $L ED_FOG_VIEWS=$V ED_RUN_FRAMES=4100 ED_PUT="3680,1,1305,1508,185;3680,2,1250,1650,185" sh tools/ed/run.sh $ST fog-f3-together-183-v$V </dev/null
done
env -u ED_COOP_STAY $L $P1GO ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1508,185" sh tools/ed/run.sh $ST fog-f4-follow-180 </dev/null
# F6 a state saved apart with the fix off (lists baked outside a scope), then loaded with it on. Pass (review fix 4): one
# 'cleared' line per resident slot after the load (section 3's 180 in its scope, section 2's 183: a load makes both lists'
# verdicts unknown), then none; 180's half then equal to F1-1's within 3.
env $L ED_FOG_ROOMS=0 ED_RUN_FRAMES=4060 ED_SAVE_STATE=4000:saves/fog-apart-off.state \
  ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" sh tools/ed/run.sh $ST fog-f6a-save-off </dev/null
env $L ED_FOG_ROOMS=1 ED_RUN_FRAMES=4300 sh tools/ed/run.sh saves/fog-apart-off.state fog-f6b-load-on </dev/null
# F7 the order turned: player 2 into 180 first, player 1 follows (together in 180), player 1 back to 183 (apart again).
for F in 0 1; do
  env $L ED_FOG_ROOMS=$F ED_RUN_FRAMES=4600 ED_PUT="3680,1,1110,1170,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" \
    ED_RUN_SCRIPT=250:119,310:119,600:29,660:29 sh tools/ed/run.sh $ST fog-f7-order-$F </dev/null
done
# F8 the stayer's mist across the other's door (A8): player 2 still in 183's pit, player 1 leaves; shots every 50 ms.
for K in 0 1; do
  env $L $P1GO ED_FOG_KEEP=$K ED_RUN_SHOTS_MS=50 ED_RUN_FRAMES=3950 ED_PUT="3680,1,1110,1170,185;3680,2,1250,1650,185" \
    sh tools/ed/run.sh $ST fog-f8-keep-$K </dev/null
done
# F9 player 2's examine close-up while apart (lent cameras): player 1's half (180) must not change with it (Part B).
for V in 0 1; do
  env $L $P1GO ED_FOG_VIEWS=$V ED_RUN_SHOTS_MS=50 ED_RUN_FRAMES=4100 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1508,185" \
    ED_PAD2_SCRIPT="3900,3906,b;4000,4006,b" sh tools/ed/run.sh $ST fog-f9-lent-v$V </dev/null
done
# Stayer S3 (player 2's copy Tome while player 1 leaves) and S8 (player 1's while player 2 leaves), from
# build-run-stayer-presses.sh, with both parts on: no scope may open and no view depth be sent during the copy turn (the
# section-3 count is 0 then, no '[fog] views: ... sent' line inside it).
LS="ED_COOP_LOG=1 ED_COOP_ENEMY_LOG=1 ED_CHANGE_HOLD=1 ED_STAYER_LOG=1 ED_COOP_TRACE=1 ED_COOP_CAST_TRACE=1"
env $LS $P1GO ED_FOG_ROOMS=1 ED_FOG_VIEWS=1 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1000,185" \
  ED_PAD2_SCRIPT="3830,3836,start;3860,3866,right;3880,3886,b;3900,3906,b;3940,3946,b" sh tools/ed/run.sh $ST fog-s3-p2-start </dev/null
env $LS ED_FOG_ROOMS=1 ED_FOG_VIEWS=1 ED_RUN_FRAMES=4200 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" \
  ED_INPUT_SCRIPT="3770,3776,0x1000;3790,3796,0x0002;3805,3811,0x0200;3830,3836,0x0200;3870,3876,0x0200" sh tools/ed/run.sh $ST fog-s8-p1-start </dev/null
# F5 solo, unchanged (no [fog] line); F5b solo reference: the hero where F2's player 2 stands (183's pit).
unset ED_SETTINGS
JTG_SLOT=1 JTG_SET="mod_coop=0" env $L ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29,3820:119,3880:119 \
  ED_RUN_FRAMES=4300 sh tools/ed/jtg.sh 1 fog-f5-solo </dev/null
JTG_SLOT=1 JTG_SET="mod_coop=0" env $L ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 \
  ED_PUT="3680,1,1250,1650,185" ED_RUN_FRAMES=4000 sh tools/ed/jtg.sh 1 fog-f5b-solo-183-pit </dev/null
# Part B is a graphics change: chapter 1's rooms with ED_FOG_VIEWS=0 and 1, their sheets one above the other. 'ed
# chapters' runs from saves/slot1.state, made without the two room slots, which does not load with ED_ROOM_SLOTS set
# (quickstate.c); and without the slots and the stay nothing of the mist is installed. So this is the check that both
# parts are inert there (the two sets of sheets must be equal); Part B's own pictures are F3-v0/v1 and F9-v0/v1 above.
if [ "${CHAPTERS:-0}" = 1 ]; then
  env -u ED_ROOM_SLOTS -u ED_COOP_STAY -u ED_FOG_VIEWS $ED chapters 1 --name fog-views0 </dev/null
  env -u ED_ROOM_SLOTS -u ED_COOP_STAY ED_FOG_VIEWS=1 $ED chapters 1 --name fog-views1 </dev/null
  $ED chapters --compare fog-views0 fog-views1
fi
# Each run: its timeline, its check, its [fog] lines (host and renderer) and a sheet of the settled frames.
for R in $(find logs -maxdepth 1 -type d -name 'run-*-fog-*' -newer build/fog-runs.stamp | sort); do
  echo "=== $R"
  $ED timeline "$R" | tail -40
  $ED check "$R"
  grep -h "\[fog" "$R"/page-*.log "$R"/*.log 2>/dev/null | sort -u | head -80
  # (F5 solo: with the switches on the install lines print; any other [fog] line in a solo run is a failure.)
  case "$R" in *fog-f5*) echo "solo [fog] lines besides the install: $(grep -h "\[fog" "$R"/page-*.log "$R"/*.log 2>/dev/null | sort -u |     grep -cvE "the room mist per room|views: each half|not taken|NOT installed|not watched|has an intercept")";; esac
  echo "cleared lines: $(grep -h "\[fog\] list: .*cleared" "$R"/page-*.log "$R"/*.log 2>/dev/null | sort -u | wc -l)"
  $ED sheet "$R" 3950 4050 4150 4250
done
echo RUNS DONE
