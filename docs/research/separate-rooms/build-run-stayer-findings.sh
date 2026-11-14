#!/bin/sh
# The stayer-presses findings F2 (player 2's arrival cancels player 1's close-up) and F3 (the door's fade in a stayer's copy
# Tome): design-stayer-findings.md revision 2, F2.6 and F3.5. Fresh states first. Read `ed timeline`, EVENTS.log and
# page-0001.log of each run first; measure each half's luma from shots/*.jpg for the F3 lines.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_CHANGE_TIME=1 ED_RUN_SHOTS_MS=250
L="ED_COOP_LOG=1 ED_COOP_ENEMY_LOG=1 ED_CHANGE_HOLD=1 ED_STAYER_LOG=1 ED_COOP_TRACE=1 ED_COOP_CAST_TRACE=1"
# --- fresh state 1: chapter 1, room 183 (as build-run-stayer-presses.sh makes it) ---
rm -f saves/rooms-ch1-183.state saves/rooms-ch1-344.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 stayer-s0-183 </dev/null
SET1=$(ls -d logs/run-*-jtg-stayer-s0-183 | tail -1)/settings.ini
# --- fresh state 2: chapter 3, room 153, ED_CHEAT=1 and the Tome (stayer-test-setups.md §1b). Check before use: `[room] ticket`
#     near 2812, both players in room 153 by ~2875, and T0 (both.tsv's first row). The ch3 frames below take T0 = 3050. ---
rm -f saves/rooms-ch3-153.state
JTG_SLOT=1 JTG_P1=1 JTG_P2=2 env -u ED_COOP_STAY ED_CHEAT=1 ED_ITEM_SPAWN=2400,30,30,300,1 \
  JTG_P1_SCRIPT="130,138,0x0200;190,198,0x0200;230,238,0x0200;270,278,0x0200" \
  ED_RUN_SCRIPT=2750:1106,2810:1106 ED_SAVE_STATE=3050:saves/rooms-ch3-153.state ED_RUN_FRAMES=3150 \
  sh tools/ed/jtg.sh 3 stayer-c3-153 </dev/null
SET3=$(ls -d logs/run-*-jtg-stayer-c3-153 | tail -1)/settings.ini

# ===== chapter 1, room 183 =====
export ED_SETTINGS="$SET1"; ST=saves/rooms-ch1-183.state
P1GO="ED_RUN_SCRIPT=110:119,170:119"                       # player 1's door (183 -> 180), ticket ~3822
D2="3760,3770,b"                                            # player 2's OPEN DOOR, ticket 3760
# --- F2: player 1 stays in a close-up through player 2's door ---
# S9 (as is): F2a's line (slot, look 0 or 1), F2b's traces, R4b's `keeps player 1's busy 2`, the carried-over line, no WARNING.
env $L ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185;3772,1,1305,1508,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3700,3714,0,0,127;3775,3789,0,0,127;3800,3806,0x0200" sh tools/ed/run.sh $ST stayer-s9-p1-zone </dev/null
# S9w: S9, then its words to the end (DETAIL opens them, RETURN closes them, the game's pop). `ed see 4100`: free, busy 0.
env $L ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185;3772,1,1305,1508,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3700,3714,0,0,127;3775,3789,0,0,127;3800,3806,0x0200;3850,3856,0x0200;3900,3906,0x0200;3950,3956,0x0200;4000,4006,0x0200" sh tools/ed/run.sh $ST stayer-s9w-p1-words </dev/null
# S9z: the zone EXAMINE ready, no B, at the arrival: F2a's line with kind 1; the B at 3900 opens the close-up.
env $L ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185;3772,1,1305,1508,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3700,3714,0,0,127;3775,3789,0,0,127;3900,3906,0x0200;3950,3956,0x0200" sh tools/ed/run.sh $ST stayer-s9z-p1-zone-ready </dev/null
# S9z-off: today's code: the always-printed WARNING, no F2a line, the B at 3900 does nothing (the dead EXAMINE).
env $L ED_STAYER_PRESSES=0 ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185;3772,1,1305,1508,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3700,3714,0,0,127;3775,3789,0,0,127;3900,3906,0x0200;3950,3956,0x0200" sh tools/ed/run.sh $ST stayer-s9z-off </dev/null
# S11d (apart, player 1 stays in 183): S11c's line, EXAMINE inside player 2's ladder window (~4170-4232), DETAIL after the arrival.
env $L ED_COOP_P1_QUICK=00010504 ED_RUN_FRAMES=4800 ED_PUT="3680,1,1110,1170,185;3680,2,360,3090,185;4090,1,1305,1508,185;4100,2,-900,78,-150;4130,2,-1023,76,-150" ED_PAD2_SCRIPT="3760,3770,b;4104,4110,lup;4134,4138,lup;4170,4176,b" ED_INPUT_SCRIPT="4185,4191,0x0200;4260,4266,0x0200;4300,4306,0x0200;4350,4356,0x0200" sh tools/ed/run.sh $ST stayer-s11d-apart-p1-closeup </dev/null
# S6r (RETURN up at the arrival, stage 3): counts only if the `[stayer] frame 3822` line's `shown` is a -2 slot (RETURN).
env $L ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3765,3771,0x0200;3775,3781,0x0200;3800,3806,0x0200;3900,3906,0x0200" sh tools/ed/run.sh $ST stayer-s6r-p1-return </dev/null
# --- F3: the door's fade in a stayer's copy Tome (luma of each half) ---
# S3: F3a's line ~3830; the right half (the copy) at its undimmed level at shots 000012-000014; `copy drew the fade quad 0 times`.
env $L $P1GO ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1000,185" ED_PAD2_SCRIPT="3830,3836,start;3860,3866,right;3880,3886,b;3900,3906,b;3940,3946,b" sh tools/ed/run.sh $ST stayer-s3-p2-start </dev/null
# S3h: the copy held across the firing (~3884): the Tome half clear 3884-3990; F3h's line once the fade-in is over.
env $L $P1GO ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1000,185" ED_PAD2_SCRIPT="3830,3836,start;3860,3866,right;3990,3996,b;4010,4016,b" sh tools/ed/run.sh $ST stayer-s3h-p2-held </dev/null
# S3t: player 2's Start in the fade-in tail: F3a's line naming `the fade-in` (counts only if `player 2's Tome opens` ~3886).
env $L $P1GO ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1000,185" ED_PAD2_SCRIPT="3886,3892,start;3950,3956,b" sh tools/ed/run.sh $ST stayer-s3t-p2-tail </dev/null
# S8: F3a's line for player 1 ~3770; the left half at its undimmed level at 000007-000009.
env $L ED_RUN_FRAMES=4200 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3770,3776,0x1000;3790,3796,0x0002;3805,3811,0x0200;3830,3836,0x0200;3870,3876,0x0200" sh tools/ed/run.sh $ST stayer-s8-p1-start </dev/null
# S8h: player 1's copy across player 2's firing (~3824): the left half clear across it, then room 183.
env $L ED_RUN_FRAMES=4200 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3770,3776,0x1000;3790,3796,0x0002;3900,3906,0x0200;3930,3936,0x0200" sh tools/ed/run.sh $ST stayer-s8h-p1-held </dev/null
# S8t (F3f): `[stayer] player 1's Start in player 2's fade-in` and F3a's line naming `the fade-in`; the left half never dark.
env $L ED_RUN_FRAMES=4200 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3826,3832,0x1000;3900,3906,0x0200" sh tools/ed/run.sh $ST stayer-s8t-p1-tail </dev/null
# S3m (F3e): no F3a line; each Start read in the countdown prints F3e's refusal; no copy; player 2's half fades with their door.
env $L ED_RUN_FRAMES=4200 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3760,3770,b;3775,3781,start;3800,3806,start;3860,3866,b" sh tools/ed/run.sh $ST stayer-s3m-mover-copy </dev/null
# S3m-tail (F3e, review fix): player 2's Start in their own fade-in after the arrival (~3823-3832): the refusal line, no copy,
#   no `[tome-copy] player 2's copy opened in a fade`. Counts only if the Start falls before the fade's last step.
env $L ED_RUN_FRAMES=4200 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3760,3770,b;3826,3832,start" sh tools/ed/run.sh $ST stayer-s3m-tail-mover </dev/null
# S6f (F3d): S6b with the presses moved so the find (busy 16) is up at ~3818-3822: `bits 0x10` and F3d's line; the left half
#   never dark at 3818-3836. (The timing is a guess from S6b: if the frame lines show no busy 16 at 3818, move the B's.)
env $L ED_RUN_FRAMES=4400 ED_PUT="3680,1,360,340,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3790,3796,0x0200;3812,3818,0x0200;3900,3906,0x0200" sh tools/ed/run.sh $ST stayer-s6f-p1-find-fade </dev/null
# --- then: the keep, the rejoin, the apart mirror ---
# S6c (player 1's B before player 2's ticket): `player 1's look or find is up` keeps player 2's B; after RETURN the door runs.
env $L ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3740,3746,0x0200;3800,3806,0x0200;3880,3886,0x0200;3930,3936,0x0200" sh tools/ed/run.sh $ST stayer-s6c-p1-exam-before </dev/null
# S11r (rejoin): player 2 to 180 by the door (ticket 3760), then back through 180's Region02 (script 29, sphere (-2,-662)
#   r 152; they arrive at EntryMkr02 (0,-580), inside it: put outside, step, put inside, step, B) while player 1 EXAMINEs in
#   183 and is at DETAIL at the rejoin's firing. The frames are a guess: find player 2's `[room] ticket` and move player 1's
#   B (EXAMINE into the window; DETAIL/RETURN after the arrival) by the same amount.
env $L ED_RUN_FRAMES=4500 ED_PUT="3680,1,1110,1170,185;3680,2,360,3090,185;3900,1,1305,1508,185;3910,2,-2,-900,-150;3940,2,-2,-662,-150" ED_PAD2_SCRIPT="$D2;3914,3920,lup;3944,3948,lup;3970,3976,b;4000,4006,b" ED_INPUT_SCRIPT="3990,3996,0x0200;4100,4106,0x0200;4150,4156,0x0200;4200,4206,0x0200" sh tools/ed/run.sh $ST stayer-s11r-rejoin </dev/null
# S11s (apart, player 2 stays in 183): S3 in the apart shape.
env $L ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4700 ED_PUT="3680,1,1110,1170,185;4100,2,1305,1508,185" ED_RUN_SCRIPT=110:119,170:119,420:28,480:28 ED_PAD2_SCRIPT="4140,4146,start;4170,4176,right;4250,4256,b" sh tools/ed/run.sh $ST stayer-s11s-apart-p2-start </dev/null
# --- unchanged: S6 (words at the arrival), the mirror S4/S1/S5b, the mover's own S5/S10/S11 ---
env $L ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3770,3776,0x0200;3790,3796,0x0200;3830,3836,0x0200;3880,3886,0x0200" sh tools/ed/run.sh $ST stayer-s6-p1-examine </dev/null
env $L $P1GO ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1000,185;3845,2,1305,1508,185" ED_PAD2_SCRIPT="3700,3714,lup;3850,3864,lup;3870,3876,b" sh tools/ed/run.sh $ST stayer-s4-p2-zone </dev/null
env $L $P1GO ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1508,185" ED_PAD2_SCRIPT="3828,3834,b;3850,3856,b;3900,3906,b;3950,3956,b" sh tools/ed/run.sh $ST stayer-s1-p2-examine </dev/null
env $L $P1GO ED_RUN_FRAMES=4400 ED_PUT="3680,1,1110,1170,185;3680,2,360,340,185" ED_PAD2_SCRIPT="3830,3836,b;3860,3866,b;3990,3996,b" sh tools/ed/run.sh $ST stayer-s5b-p2-pickup </dev/null
env $L $P1GO ED_RUN_FRAMES=4400 ED_PUT="3680,1,1110,1170,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3840,3846,b" sh tools/ed/run.sh $ST stayer-s5-p2-exit </dev/null
env $L ED_RUN_FRAMES=4400 ED_PUT="3680,1,250,3050,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3775,3781,0x0200" sh tools/ed/run.sh $ST stayer-s10-p1-exit </dev/null
env $L ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4700 ED_PUT="3680,1,1110,1170,185;4100,2,1305,1508,185" ED_RUN_SCRIPT=110:119,170:119,420:28,480:28 ED_PAD2_SCRIPT="4140,4146,b;4160,4166,up;4240,4246,b" sh tools/ed/run.sh $ST stayer-s11-apart-p2 </dev/null
# Not runnable from these states (no fresh state or line for them yet; the doc's open items):
#   S15e: script 132's ladder (level 04) with player 1's EXAMINE after player 2's B and before the ticket (impl D, G).
#   S5d: the lend route at the last count (S5c's lever line moved to ticket + 61; impl F, observation only).
#   Arrival scene (goal 1.6): a door into a room with an arrival cinematic, the stayer's copy open at the firing.
#   S10c: named in the design's run list beside S10, but no line for it exists in any build-run script or setup doc.
#   R4c with a zone (review-safety 5): player 1 in a fn22 scene (look kind 3) while standing in an EXAMINE zone at player
#     2's arrival: F2a now keeps the kind 3 and bit 2. Needs a room with a scene inside a zone; none is set up here.

# ===== chapter 3, room 153 (ED_CHEAT=1 again on every run: cheat_apply only puts back what it changed) =====
export ED_SETTINGS="$SET3"; ST=saves/rooms-ch3-153.state
L3="$L ED_COOP_CAST_LOG=1 ED_CHEAT=1"
P1GO3="ED_RUN_SCRIPT=410:1139,470:1139"   # player 1's door 153 -> 151: ticket ~3522 (after the load's TransformCam01 scene, busy 0x22 to 3330)
DOOR="-60,-400,-100"                       # inside Region02
FOUNT="1150,-100,0"                        # inside CameraRgn03 (FOUNTAIN)
D23="3460,3470,b"                          # player 2's own OPEN DOOR: ticket ~3460
# c3-p1-summon-zone (a refused window, goal 1.4): counts only if the ticket's line says the window is refused for the summon.
env $L3 ED_COOP_P1_QUICK=00020821 ED_RUN_FRAMES=3900 ED_PUT="3380,1,$FOUNT;3380,2,$DOOR" ED_PAD2_SCRIPT="$D23" ED_INPUT_SCRIPT="3440,3446,0x0008;3600,3606,0x0200" sh tools/ed/run.sh $ST stayer-c3-p1-summon-zone </dev/null
# c3-s3m-cast (F3e in a refused window, review fix): the same refused window, player 2's Start twice in their own countdown:
#   F3e's refusal for each Start read, no copy, player 2's half fades with their door and comes in with their new room.
env $L3 ED_COOP_P1_QUICK=00020821 ED_RUN_FRAMES=3900 ED_PUT="3380,1,$FOUNT;3380,2,$DOOR" ED_PAD2_SCRIPT="$D23;3480,3486,start;3500,3506,start" ED_INPUT_SCRIPT="3440,3446,0x0008" sh tools/ed/run.sh $ST stayer-c3-s3m-cast </dev/null
# c3-p1-exam-in: S9 in room 153, the words seen through.
env $L3 ED_RUN_FRAMES=3900 ED_PUT="3380,1,$FOUNT;3380,2,$DOOR" ED_PAD2_SCRIPT="$D23" ED_INPUT_SCRIPT="3480,3486,0x0200;3550,3556,0x0200;3600,3606,0x0200;3650,3656,0x0200" sh tools/ed/run.sh $ST stayer-c3-p1-exam-in </dev/null
# c3-p1-exam-before: S6c in room 153 (the keep), with DETAIL/RETURN.
env $L3 ED_RUN_FRAMES=3900 ED_PUT="3380,1,$FOUNT;3380,2,$DOOR" ED_PAD2_SCRIPT="$D23" ED_INPUT_SCRIPT="3440,3446,0x0200;3500,3506,0x0200;3580,3586,0x0200;3630,3636,0x0200" sh tools/ed/run.sh $ST stayer-c3-p1-exam-before </dev/null
# c3-p2-start: F3a's line; a cast from the copy if the page's B casts; the Tome half clear.
env $L3 $P1GO3 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4000 ED_PUT="3380,1,$DOOR;3380,2,$FOUNT" ED_PAD2_SCRIPT="3530,3536,start;3560,3566,right;3580,3586,b;3600,3606,b;3640,3646,b" sh tools/ed/run.sh $ST stayer-c3-p2-start </dev/null
# c3-p1-start: player 1's copy in player 2's door (S8's presses moved by the ticket, ~3160).
env $L3 ED_RUN_FRAMES=3900 ED_PUT="3380,1,$FOUNT;3380,2,$DOOR" ED_PAD2_SCRIPT="$D23" ED_INPUT_SCRIPT="3470,3476,0x1000;3490,3496,0x0002;3505,3511,0x0200;3530,3536,0x0200;3570,3576,0x0200" sh tools/ed/run.sh $ST stayer-c3-p1-start </dev/null
# Regression of the casts beside F2/F3: casts or keeps as before, no F2/F3 lines.
env $L3 $P1GO3 ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=3900 ED_PUT="3380,1,$DOOR;3380,2,$FOUNT" ED_PAD2_SCRIPT="3540,3546,up" sh tools/ed/run.sh $ST stayer-c3-p2-quick </dev/null
env $L3 $P1GO3 ED_COOP_P2_QUICK=00010812 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=3900 ED_PUT="3380,1,$DOOR;3380,2,$FOUNT" ED_PAD2_SCRIPT="3540,3546,up" sh tools/ed/run.sh $ST stayer-c3-p2-810 </dev/null
env $L3 $P1GO3 ED_COOP_P2_QUICK=00020821 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4000 ED_PUT="3380,1,$DOOR;3380,2,$FOUNT" ED_PAD2_SCRIPT="3540,3546,up" sh tools/ed/run.sh $ST stayer-c3-p2-summon </dev/null
env $L3 ED_COOP_P1_QUICK=00010504 ED_RUN_FRAMES=3800 ED_PUT="3380,1,$FOUNT;3380,2,$DOOR" ED_PAD2_SCRIPT="$D23" ED_INPUT_SCRIPT="3475,3481,0x0008" sh tools/ed/run.sh $ST stayer-c3-p1-quick </dev/null

# ===== off, follow, solo (chapter 1): no F2a/F2b/carried lines, no F3 lines but F3g's; the WARNING may print =====
export ED_SETTINGS="$SET1"; ST=saves/rooms-ch1-183.state
env $L ED_SPLIT_HOLD=0 ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185;3772,1,1305,1508,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3700,3714,0,0,127;3775,3789,0,0,127;3800,3806,0x0200" sh tools/ed/run.sh $ST stayer-s9-split-off </dev/null
env $L ED_SPLIT_HOLD=0 $P1GO ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1000,185" ED_PAD2_SCRIPT="3830,3836,start;3860,3866,right;3880,3886,b;3900,3906,b;3940,3946,b" sh tools/ed/run.sh $ST stayer-s3-split-off </dev/null
env $L ED_STAYER_PRESSES=0 $P1GO ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4200 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1508,185" ED_PAD2_SCRIPT="3835,3841,b;3840,3846,up" sh tools/ed/run.sh $ST stayer-s12-off </dev/null
env -u ED_COOP_STAY $L $P1GO ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1508,185" ED_PAD2_SCRIPT="3835,3841,b" sh tools/ed/run.sh $ST stayer-s13-follow </dev/null
unset ED_SETTINGS
JTG_SLOT=1 JTG_SET="mod_coop=0" env $L ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29,3820:119,3880:119 ED_RUN_FRAMES=4300 sh tools/ed/jtg.sh 1 stayer-s14-solo </dev/null
# No `ed test` here: its scenarios load old slot states (2026-11-09/10), which the user forbids for checks (fresh
# Jump-to-Game states only, memory save-states-fresh-only). It comes back once its scenarios start from fresh states.
echo RUNS DONE
