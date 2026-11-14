#!/bin/sh
# The stayer keeps every action through the other's door countdown (design-stayer-presses.md). Fresh states first.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_CHANGE_TIME=1 ED_RUN_SHOTS_MS=250
L="ED_COOP_LOG=1 ED_COOP_ENEMY_LOG=1 ED_CHANGE_HOLD=1 ED_STAYER_LOG=1 ED_COOP_TRACE=1 ED_COOP_CAST_TRACE=1"
rm -f saves/rooms-ch1-183.state saves/rooms-ch1-344.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 stayer-s0-183 </dev/null
SET=$(ls -d logs/run-*-jtg-stayer-s0-183 | tail -1)/settings.ini; export ED_SETTINGS="$SET"; ST=saves/rooms-ch1-183.state
P1GO="ED_RUN_SCRIPT=110:119,170:119"                       # player 1's door (183 -> 180), ticket ~3822
D2="3760,3770,b"                                            # player 2's OPEN DOOR, ticket 3760
# --- player 2 stays (player 1's door), split ---
# S1 examine and words across the firing: EXAMINE ~3830, DETAIL ~3850 (words open in the window), RETURN ~3950.
env $L $P1GO ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1508,185" ED_PAD2_SCRIPT="3828,3834,b;3850,3856,b;3900,3906,b;3950,3956,b" sh tools/ed/run.sh $ST stayer-s1-p2-examine </dev/null
# S2 quick spell; S2b the same spell begun before the ticket (L3: a record already under way).
env $L $P1GO ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4200 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1000,185" ED_PAD2_SCRIPT="3840,3846,up" sh tools/ed/run.sh $ST stayer-s2-p2-quick </dev/null
env $L $P1GO ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4200 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1000,185" ED_PAD2_SCRIPT="3810,3816,up" sh tools/ed/run.sh $ST stayer-s2b-p2-cast-before </dev/null
# S3 Start: their copy Tome, a page, a spell cast from it, then out.
env $L $P1GO ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1000,185" ED_PAD2_SCRIPT="3830,3836,start;3860,3866,right;3880,3886,b;3900,3906,b;3940,3946,b" sh tools/ed/run.sh $ST stayer-s3-p2-start </dev/null
# S4 zone look: put outside CameraRgn05, step, put inside in the window, step (failed-attempts 566), then B.
env $L $P1GO ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1000,185;3845,2,1305,1508,185" ED_PAD2_SCRIPT="3700,3714,lup;3850,3864,lup;3870,3876,b" sh tools/ed/run.sh $ST stayer-s4-p2-zone </dev/null
# S5 exit kept: both at the 183 -> 180 door; player 1 leaves by script, player 2's B on OPEN DOOR in the window.
env $L $P1GO ED_RUN_FRAMES=4400 ED_PUT="3680,1,1110,1170,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3840,3846,b" sh tools/ed/run.sh $ST stayer-s5-p2-exit </dev/null
# S5b pick up in the window: PICK UP, the found words, close.
env $L $P1GO ED_RUN_FRAMES=4400 ED_PUT="3680,1,1110,1170,185;3680,2,360,340,185" ED_PAD2_SCRIPT="3830,3836,b;3860,3866,b;3990,3996,b" sh tools/ed/run.sh $ST stayer-s5b-p2-pickup </dev/null
# S5c a use by player 2 in the window (B6): a script prompt that is not an exit (the lever of `ed test`'s lever-* room).
# S7b a 0x810 spell by player 2 in the window (pick the spell word from docs/game/systems/magick).
# --- player 1 stays (player 2's door), split ---
# S6 examine; S6w words across player 2's firing (EXAMINE 3770, DETAIL 3790, RETURN 3880).
env $L ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3770,3776,0x0200;3790,3796,0x0200;3830,3836,0x0200;3880,3886,0x0200" sh tools/ed/run.sh $ST stayer-s6-p1-examine </dev/null
# S6b player 1's pickup across the firing (P4's no-hold find).
env $L ED_RUN_FRAMES=4400 ED_PUT="3680,1,360,340,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3770,3776,0x0200;3800,3806,0x0200;3900,3906,0x0200" sh tools/ed/run.sh $ST stayer-s6b-p1-pickup </dev/null
env $L ED_COOP_P1_QUICK=00010504 ED_RUN_FRAMES=4100 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3775,3781,0x0008" sh tools/ed/run.sh $ST stayer-s7-p1-quick </dev/null
# S8 Start: their copy, a page, a cast from it, a Mix if the chapter has one, close; shots of both halves.
env $L ED_RUN_FRAMES=4200 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3770,3776,0x1000;3790,3796,0x0002;3805,3811,0x0200;3830,3836,0x0200;3870,3876,0x0200" sh tools/ed/run.sh $ST stayer-s8-p1-start </dev/null
env $L ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185;3772,1,1305,1508,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3700,3714,0,0,127;3775,3789,0,0,127;3800,3806,0x0200" sh tools/ed/run.sh $ST stayer-s9-p1-zone </dev/null
env $L ED_RUN_FRAMES=4400 ED_PUT="3680,1,250,3050,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3775,3781,0x0200" sh tools/ed/run.sh $ST stayer-s10-p1-exit </dev/null
# --- apart ---
# S11 player 2 stays: player 1 to 180, then 180's ladder 28 (ticket ~4132) while player 2 in 183 examines and casts.
env $L ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4700 ED_PUT="3680,1,1110,1170,185;4100,2,1305,1508,185" ED_RUN_SCRIPT=110:119,170:119,420:28,480:28 ED_PAD2_SCRIPT="4140,4146,b;4160,4166,up;4240,4246,b" sh tools/ed/run.sh $ST stayer-s11-apart-p2 </dev/null
# S11b player 1 stays: player 2 to 180 by the door, then 180's ladder by B; player 1 in 183 examines, casts, Start, picks up
#   (180's ladder place from `ed room 180` first).
#   180's Region03 (enter 28, transition 12, the 344 way out) is a sphere at (-1075, 78, 8) r 109: player 2 is put in it
#   at 4100 and steps (a teleport alone does not come into a zone). Player 1 is put at 183's EXAMINE at 4090.
env $L ED_COOP_P1_QUICK=00010504 ED_RUN_FRAMES=4700 ED_PUT="3680,1,1110,1170,185;3680,2,360,3090,185;4090,1,1305,1508,185;4100,2,-1075,78,8" ED_PAD2_SCRIPT="$D2;4102,4110,lup" ED_INPUT_SCRIPT="4112,4118,0x0200;4130,4136,0x0200;4160,4166,0x0008;4200,4206,0x0200;4240,4246,0x0200" sh tools/ed/run.sh $ST stayer-s11b-apart-p1 </dev/null
# --- doors that hold play through their ticket, one per direction (refute-impl 2.3, 2.8) ---
# S15/S16 script 132's ladder (level 04): the mover's hold spans the ticket (g_split.word0 & 2); the stayer examines and
#   opens words in the window. S17 L01/103: the stayer's own ladder Yes in the other's window (M13, or today's printed loss).
# --- the 12-frame exit and player 2's walk-in door ---
# S18 player 1 walks into a zone exit (transition 2) while player 2 presses B 4 frames after its ticket.
# S19 player 2 walks into a way out in their turn (L1's g_coop_turn case; stayer_cast_end with player 1 charging a cast).
# --- off, follow, solo ---
env $L ED_STAYER_PRESSES=0 $P1GO ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4200 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1508,185" ED_PAD2_SCRIPT="3835,3841,b;3840,3846,up" sh tools/ed/run.sh $ST stayer-s12-off </dev/null
env -u ED_COOP_STAY $L $P1GO ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1508,185" ED_PAD2_SCRIPT="3835,3841,b" sh tools/ed/run.sh $ST stayer-s13-follow </dev/null
unset ED_SETTINGS
JTG_SLOT=1 JTG_SET="mod_coop=0" env $L ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29,3820:119,3880:119 ED_RUN_FRAMES=4300 sh tools/ed/jtg.sh 1 stayer-s14-solo </dev/null
echo RUNS DONE
