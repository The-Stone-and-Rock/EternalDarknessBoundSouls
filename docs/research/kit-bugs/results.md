# The kit's likely co-op bugs: what the runs showed (2026-11-17)

Runs from fresh Jump-to-Game states on the build of the day, ED_RUN_AWAY=1, both players recorded (both.tsv, shots
every 250 ms), read with the scripts beside this file and `python -I tools/ed/ed.py stops RUN`. The fixes are in
runtime/host/hle.c, **uncommitted, for the user to review**. Plans: plans.md.

## Before the fixes (build 2026-11-17 01:03, HEAD 57bea79 plus controls.cpp's test edits)

1. **The duck (fn151)**: not reached. Edward's lever L09/1951 ran for player 2 (who 161) when they entered
   SphereRegion01 and stopped at its 4th test, `if L0 ~= 1` (L0 = fn78(1, 0), the player's handle): a **hero gate**,
   only the chapter's character (handle 1) gets the lever. logs/run-20261117-010550-duck-apart,
   logs/run-20261117-010635-duck-together; `ed stops 010635-duck-together 1951 --level 09`. 73 script files have such a
   gate (35 reached from rooms in the catalogue; the pattern "only for the chapter's character"). The duck's fix can't
   be shown with this lever until that gate is dealt with.
2. **The arrival scripts (ed113 / ed248)**: shown. Player 2 into room 90 apart (chapter 12): room 90's own script 2477
   ran with room 90 current and its ed113 read player 1's node 0x81127F3C (logs/run-20261117-011013-jtg-arrival-a-before,
   `[chain] 0x801F6300 r3=0x81127F3C frame=2356`).
3. **The L06 traps (ed156)**: shown, worse than read. In solo, 2105 runs twice from TrapRgn04 (and twice from its timer).
   In co-op with player 2 standing in it, the game's own zone look runs 2105 as player 1 (who 1, lr 0x80134B38) on
   190 to 273 frames, beside player 2's own one or two runs (who 239): t 273, a 190, b 201
   (logs/run-20261117-01*-trap-*; traps-before in the session's scratch). Every run plays the trap and writes the record
   of the room in play.
4. **The L04 elevators**: not reached; the test's route was wrong, not the game. Door 132 sends the chapter's character
   (model 150, what hle_object_model tells scripts) to room 5 unless thing 733 or 831 is carried, and the Tome trip took
   both there (logs/run-20261117-011659-elev-apart). The test needs 733 or 831 carried first. The traps show the same
   mechanism (the game's zone look running other_* fields for player 2's body), and the fix is one for both.
5. **ed177's play-start scripts**: shown. Player 2 in MuffledVoicesRgn01 (L03/933, room 150), then the Tome: the stored
   script ran from play mode's start (lr 0x80008280) twice, as the game's, though player 2 left it
   (logs/run-20261117-012026-onplay-p2in-before).

## The fixes (runtime/host/hle.c, uncommitted)

- The duck: coop_sound_change_act tests lr 0x80164DD8 (the return), not 0x80164DD4 (the call).
- The arrival scripts: coop_run_script's first block makes player 2 the player (coop_be_player2) around their own
  room's queued arrival script, when they stand in that room; its [stay] line now says "as player N's (main node ...)".
- The traps and elevators: coop_scan_player1 passes over the game's own step's zone look (lr 0x801335C8) for player 2's
  object; their own look runs their regions as theirs; monsters keep theirs. ED_COOP_P2_OTHER_ZONES=1: the old route.
  Logged under ED_COOP_LOG: "[coop] player 2's object passed over by the game's zone look for others".
- ed177: a notify on 0x8000F9C4 (coop_play_script_left) keeps who stored it; coop_run_script runs the play-start script
  (lr 0x80008280) as player 2's when they did. Logged: "[coop] on_play_script left by player N",
  "[coop] play start's script S (left by player 2) ran as player 2's".

## After the fixes (build 2026-11-17 01:2x with the four fixes; fresh states made on it)

2. **Arrival scripts: fixed.** "[stay] room 90's own script 2477 (queued by player 2's arrival) ran with room 90 current,
   as player 2's (main node 0x8112A084; frame 2350)"; its ed113 read player 2's node 0x8112A084
   (logs/run-20261117-012659-jtg-arrival-a-after).
3. **Traps: fixed.** "[coop] player 2's object passed over by the game's zone look for others" once per room. Runs of 2105
   as player 1 from the zone, with player 2 standing in the trap: t 273 -> 1, a 190 -> 0, b 201 -> 1; player 2's own run
   once per crossing (who 239) and its follow-up (lr 0x7FFF0070). The one run as player 1 left in t and b comes about 200
   frames after the crossing: likely a monster of room 168 crossing the trap (monsters keep the game's zone look and run
   as the game's, who 1), not proven (the trace doesn't name the object). Solo unchanged: 2 zone runs and 2 timer runs,
   as before the fix (logs/run-20261117-01*-trap-*). No hang report in any.
4. **Elevators**: not run (the route needs thing 733 or 831 carried); the same fix as the traps.
5. **ed177: fixed.** "[coop] on_play_script left by player 2" for player 2's zone entry and "left by player 1" for
   player 1's (and solo's run unchanged); the play-start runs of 933 then go through as player 2's (guest calls, lr
   0x0BADF010, 4 in p2in). No hang report (logs/run-20261117-01*-onplay-*).
1. **The duck: not shown yet** (the lever's hero gate stops player 2 before fn151); waits for the hero-gate fix.
