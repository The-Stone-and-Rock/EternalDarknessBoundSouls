# Refuting design-tome-room.md against the host and the goal (2026-11-14)

Read only: nothing edited, built or run. Sources: runtime/host/hle.c and frontend.cpp (working tree on 3542bf0),
runtime/host/main.c and hle.h, `ed dis` / `ed fn` / `ed door`, build/ed-db/scripts, and the logs named. Tags on each
point: **[real]** = wrong or missing as written, with the evidence; **[maybe]** = a risk not settled by what was read.
Inside each point, claims are tagged [read] (address, file:line or log line given) or [inferred].

Words as in the design: T the taker (room RT, place PT), O the other (RO, PO), K the finder. Room 5 the Tome room.

---------------------------------------------------------------------------------------------------------------------
## A. What was checked and holds

- `coop_stay()` is cached once and needs ED_COOP_STAY and two slots (hle.c:14328-14333) [read]. The frontend sets the
  two switches only while co-op is on (frontend.cpp:482-503) [read]. So pieces behind `coop_stay()` never reach solo
  [inferred].
- `notify_room_ticket` (11424-11456): the asker by identity or player 2's way out, `g_coop_p1_tickets`, `g_fade_mover`,
  then `split_ticket_open` at its end [read]. `notify_ticket_run` opens the bracket only with `g_coop_room_p2`
  (14405-14407) [read]. `coop_stay_apart` (14346-14352), `coop_stay_stayer`/`coop_stay_spared` (14359-14368), the tail
  (15062-15087) and `room_slots_return` (15092-15119) are as F8-F12 say [read].
- `r4_fade_quad` returns at once when `g_fade_mover == 0` (25904-25905): the fade is drawn in both halves [read].
- The bring code in `coop_follow` (22461-22621) is as F10 says: player 2 on the arrival spot and player 1 beside when
  `g_coop_entered_p2`, the facing copied, the room stamped with 0x801F64C8, look and find reset [read].
- The writer 0x80155770 [read `ed dis 0x80155770`]: r3 room (+8), r4 entry (+28), r5 camera (+32), r6 script (+12), r7
  flags (+16), r8 countdown (+0, at least 1), r9 delay (+4), r10 transition (+20); the caller's r1+8/+12/+16 go to
  +36/+40/+24. Then 0x80117260(0x10000,1), the busy set 0x801556CC(4,1), the lock 0x800443E4, the cast end 0x801C5118.
- The runner's flags bit 1 calls 0x8016490C(0) in place of the marker, and leaves the heading at 16(r1) unset; both
  facing calls read it (0x80152680-0x80152734) [read]. Event 61 goes to r13-23288 at 0x801520E0-0x801520F4 [read].
- A host guest call runs through the dispatcher (main.c:57-100 calls `dolrecomp_call`, which asks the HLE hooks first:
  hle.h:8-12) [read]. So P7's call of 0x80155770 does re-enter the writer's intercept, and its notify runs on
  passthrough (hle.c:5638-5652) [read].
- 0x80134D48(object) walks the current slot's zones (0x80155234(2)) and calls 0x80134684 for each, with the sound and
  ambience calls 0x800445C4 / 0x80044538 on enter and leave [read `ed dis 0x80134D48`, `0x80134CA4`]. F28 holds, and
  docs/coop-failed-attempts.md:466 (sound zones) is also right: it does both [inferred from the two reads].
- Room 5's only ways out are 1413's chapter-13 branch and 1599's returns [read `ed door 5 --level 4`].
- `ed284`/`screen_effect_on` is in the seven door scripts and 2675 only [read: grep of build/ed-db/scripts].

---------------------------------------------------------------------------------------------------------------------
## B. Points

### 1. [real] At the way back, the other slot's record is dead: every first return is a fresh load
- The film's end reloads the room in play through 0x80152298 with flags 0x44 (`[room] change 0x00000096 0x00000044`,
  logs/run-20261113-181338-jtg-stayer-c3-153/page-0001.log:2572) [read].
- The loader does no load when the wanted room is the current slot's (0x80154D98-0x80154DB8 → 0x80154E1C) [read]. But
  after the load, 0x801523A8-0x80152404 marks the OTHER slot's record dead (room -1, loaded 0). It is skipped only when
  the other index is -1. Bit 0x40 skips only 0x80152410-0x8015242C, so the host's tail at 0x80152430 still runs [read
  `ed dis 0x80152298`].
- `notify_room_slots_tail` puts back only the room in play and the stayer's room. The stayer is player 2, who is in
  room 5 by then (15070-15075) [read]. So RO (4.1), RT (4.2) or RT=RO (4.3) is dead from the film's end on [inferred].
- Wrong as written:
  - §4.1 step 8 ("RO is live in the other slot, so a swap and no unpack");
  - §4.3 ("O goes first ... into RT's live record: a swap and no load");
  - P7 ("finds RO live in the other slot");
  - reader 4 §3 and §7;
  - run 3's pass line `[rooms] a return into room 211 ... kept`. It will log "nobody stayed in it: loaded fresh"
    (15101-15103) [read].
- Not a crash. The dead-record rule makes it a fresh load, the solo re-entry [read 15094-15100]. T's own return in 4.3
  is still "already in the other slot", because O's change puts that room back at its tail [inferred].
- In c3-153 the other slot was already empty, so no log has shown this yet [read: lines 2573-2577].

### 2. [real] Player 2's door does not run in player 2's turn: F3's "asker's copy" and P9's premise are wrong
- "Their B with a prompt: its script runs at player 1's controller (coop_use_start)" (hle.c:21946-21947) [read].
- `coop_use_start` (18862-18905) swaps these and nothing else [read]:
  - the identity;
  - the room (`coop_stay_room_in`);
  - the kinds;
  - the script words;
  - the zones;
  - the cameras.
- The controller block (r13-25680..-25632, `coop_state` 12354-12362) is swapped only in `hle_coop_controller`
  (21839-21841, 22068-22077) [read].
- Player 2's script timers also resume through `coop_use_start` (21482-21491) [read].
- In logs/run-20261106-085728-jtg-dc1-c4p2climb, both 132's press (page-0001.log:441-445) and the run that wrote its
  ticket to room 5 (604-621) were "script 132 runs for player 2" at lr 0x7FFF0070 [read]. That run is from before the
  stay. The press code at 21935-21975 is the same under the stay, outside another player's door window [read].
- So `ed284(1)` and the ticket both use player 1's copy of r13-25680 [inferred]. P2's test therefore sees the word the
  door set, and 2675's `ed284(0)`, run as player 1, clears that same copy [inferred].
- P9's "When T is player 2, their copy keeps the 1" and §4.2 step 1's "the script runs in player 2's turn ... the
  dissolve is player 2's word" do not hold. P9 is not needed for that reason [inferred].
- Side effect, not read: ed285 scales the strength by the player's distance to the marker (reader 1 §3). For a door of
  player 2's, "the player" at the draw is player 1, in another room [maybe].

### 3. [maybe] The trip's mark is a word the game also sets outside the seven doors
- The game's own code calls 0x80044200 with r3 1 at 0x8001E4F0 (`li r3,1` at 0x8001E4EC), and also from 0x8001FE28
  [read: disasm.txt]. What these are was not read.
- A ticket written while one of them has r13-25680 on would be marked a trip. P6 would then keep the next ticket out of
  that room as "the way back", and a normal door would send O home [inferred].
- A tighter mark, engine-generic: the target room's script (record +136, F16) is 2675, or the target is a room 1599
  tests. Both can be read at the ticket [inferred].

### 4. [real] P6's install worry is moot
- `split_ticket_ask` is put on the writer whenever `coop_stay()` and the code checks pass (hle.c:31852-31870)
  [read]. `g_split_ready` does not depend on ED_SPLIT_HOLD. `split_switch()` only changes `split_ticket_open`'s answer
  (25953-25956) [read].
- So "the intercept must be in ... also with ED_SPLIT_HOLD=0" is already true. The trip branch must come before its
  `split_counting` return (26001-26003) [read].

### 5. [maybe] P7's call from inside the intercept: the one passthrough flag
- The dispatcher clears `g_hle_passthrough` before each intercept and reads it after (hle.c:5638-5647) [read]. It is
  one global.
- P6/P7 as written "void the writer (passthrough = false, return)" and then write O's ticket from the same intercept.
  The re-entered intercept passes through on the guard, which leaves the flag true [read 26000]. If it isn't set false
  again after the guest call, the recompiled writer then also runs 1599's ticket over O's. There is one record at
  0x8058AFFC [inferred].
- `g_hle_tail_call` has the same problem [inferred].

### 6. [real] T's ticket wipes O's come-into mark
- `notify_room_ticket` sets `g_follow_fresh = 0`, `g_coop_arrival_scene = 0` and `g_coop_p1_own_camera = 0` at every
  ticket (hle.c:11448-11450) [read].
- P7 marks O come-into (`g_follow_fresh = 2`) at the end of O's change. P8 writes T's ticket "at the first frame after
  O's change with busy 4 clear". The arrival already cleared every busy bit inside O's change (0x80152184) [read
  reader 4 §2].
- So T's ticket can land before O's first look with zones on, which comes after the 10-tick fade-in, and clear the mark.
  O's first look then comes into what they stand in at PO and runs its enter script (doors.md:77) [inferred].
- P8 must wait for O's first look, or re-arm the mark after its own ticket.
- For player 1, 0x80134D48 is not touched by the ticket [read A]. It must run while RO is the current slot, which holds
  at the behaviour pass after their change [inferred].

### 7. [real] When player 2 is T, the one left unbrought is T, not O
- P2 makes the trip change player 1's, so the game always moves player 1 into room 5 [read 11436-11437 plus P2]. When T
  is player 2, O (player 1) is in room 5 at once, and it is T who waits for P5's bring.
- P5 postpones the bring while player 2 casts or has their copy Tome open. Whether the copy honours the door's
  `deny_menu(200)` is open (§11.13).
- P6 ("state 1 with O not yet brought"), P7 ("If O is not in the trip room ... skip to P8") and §5.4 all assume the
  unbrought one is O. If T = player 2 is still in RT at the pickup (K is then player 1, and 1599 plays fine in room 5):
  - P7 sends player 1 to RO;
  - P8 then replays the kept ticket as player 2's, from RT to RT;
  - that is an R4 change into their own room, which the design does not cover [inferred].

### 8. [real] A trip that does not begin can leave player 2 alone in room 5, and the game's return then moves player 1
- P2 refuses the trip under these conditions, and today's R4 then takes player 2 alone into room 5 [read 14405-14416]:
  - `cutscene_playing`, which is global (3869-3875): player 1's in-world scene in RO is enough;
  - `g_tome_copy_turn`;
  - `ED_TOME_TRIP=0`.
- F18 is right only for Karim. 1599's branches for Anthony, Lindsey, Paul, Roberto, Peter and the Major have no room
  test (reader 1 §4.2, reader 2 §3) [read there].
- So player 2's pickup alone plays the film, and 1599's return `fn66` is player 1's: player 1 is taken from RO to RT,
  and player 2 is left in room 5. P6's fallback covers only "both in room 5" [inferred].
- The way in must be held, or the bring forced, whenever P2 refuses. Letting player 2 go alone is the failure.

### 9. [real] The no-record fallback has no test and no end
- P6's fallback "state 0 with both in room 5 after a pickup" needs to know the trip room without a record. The design
  gives no test (point 3's mark would serve).
- It then "set[s] state 1 with no record". P5 sets state 2 "if both are now in the trip room" (`g_tome_trip.room`
  unset). No step returns this path to state 0, because P8's "state 0" is on the record path. A later ticket out of
  the room could then be kept as a way back [inferred].

### 10. [real] A state loaded in the same process keeps a live trip record
- `g_tome_trip` is host memory. The design says a state "loaded during the trip has no record". That is true for a
  fresh process only.
- Loading a state while a trip is on keeps the live record, and P5/P6 act on it in a world where it is false. The
  host already has a state-load hook (`hle_coop_prompt_load` → `tw_state_loaded`, 21517-21528) [read], so the record
  needs clearing there [inferred].
- The kit loads states (run.sh saves/slot7.state).

### 11. [real] The trip's countdown is not fx-a-1's
- P2 refuses the split window, so `split_ticket_open` takes its "holds as today" path (25957-25969) [read]. fx-a-1 had
  the window (`[split] ... player 2 stays in room 338`, `[stayer] player 2 stays: their presses go on`, reader 3 §6)
  [read there].
- Apart, the door's event 102 does not reach RO, because it is sent by room (`notify_change_hold_event`, 28243-28260)
  [read]. But busy 4 is one global word, and RO's busy asks are no longer answered "not busy" [inferred].
- So §4.1 step 3's "Player 2 goes on in RO as under the stay ... as in fx-a-1" is wrong. It is harmless, since they are
  about to be taken.

### 12. [real] fn66's sound cut runs after the writer, not before
- In fn66, the let-go 0x801F6828 runs before the writer (0x8016679C-0x801667A4). The sound cut 0x801A05EC(0,1,0) runs
  after it returns (0x801668EC-0x801668F8) [read `ed dis 0x80166780`].
- P6 says both have run before the writer. With the writer voided, the cut still runs as fn66 goes on [inferred].
  O's host-written ticket has neither [read: P7 calls only the writer].

### 13. [maybe] O's change drops T's music and fade
- 1599 state 1 starts `stream_play(81,...)` and the white fade before its `fn66` [read there: reader 1 §4.2].
- O's change (wanted ≠ room in play) runs 0x801A0EAC at 0x80152340, which releases every stream-backed track
  [read `ed dis 0x80152298`; SOUND_STREAM_DROP comment in hle.c]. Whether R5's walk keeps a stream begun for T in
  room 5 was not read.
- O's arrival also runs the 10-tick fade-in (0x801521A8) on the one global fade, under 1599's white. The design's §7
  notes the picture half of this.

### 14. [maybe] T's camera follow-ups when T is player 2
- In the six non-Karim chapters, 868 runs `run_next(531 | 1061 | 1161)`, which set Camera01 (reader 1 §4.3) [read
  there]. 868 runs inside T's R4 bracket. Whether the queued script then runs as player 2's or as player 1's from the
  game's table is §11.2's question.
- §11.2 asks it for 1349 only, and F22 sets the camera ones aside. Run as player 1's, they would cut O's camera in RO
  [inferred].

### 15. [maybe] RO after the host bring
- After P5 moves O (or, in 4.2, T) by hand, the room left stays resident and live until the film's end (point 1),
  with nobody in it. Sound (R5's kept tracks), effects, fog and lights are kept per room by the players' rooms
  [read there: reader 3 §2.8].
- What each does with a resident room nobody is in was not read. The bring from before the stay never had one.
  Neither had the stay: `coop_stay_apart` turns false only with both in one room [read 14346-14352].

### 16. [maybe] What player 1 is doing in RO when player 2's trip drags them (4.2)
- **Player 1's own door counting.** T's `fn66` is passed over (17437-17444) [read]. The door script has already run
  the dissolve, `stream_fade(0,90)`, the Tome sound 33 and `deny_menu`. Only 2675 fades that sound out (reader 2 §2)
  [read there]. This is today's behaviour too, but the trip makes it reachable.
- **Player 1 in a find, words or close-up.**
  - Words and close-ups are closed, because `g_coop_room_p2` is false (11327-11358) [read].
  - A find (busy 32): fn66's own gate 0x800440D0 (0x80166788) may refuse the trip [not read].
- **A death in RO during the countdown.** Game over comes at least 120 frames after the death move
  (b1b2-ticket-death-readers.md §0) [read there]. It can land while both are in room 5 or in the film.

### 17. [maybe] O's camera hash for player 2
- P3 reads it "at the end of player 2's own turn" from r13-26864 and the eye records. The kit's reader (18610-18617)
  reads only the game's own records [read].
- Whether r13-26864 is part of player 2's swapped camera set was not shown.

### 18. [maybe] P7/P8's player-2 wrap is not a player 2 door
- A real door of player 2's has their room in (`coop_stay_room_in` in `coop_use_start`, 18874) [read]. `coop_be_player2`
  alone leaves r13-23276 as player 1's room [read 12426-12447].
- The busy set's event 102 then goes to player 1's room. In P8 that is RO, where O is back. The split window keeps it
  when it opens. Not read for a window refused by a cutscene at the ticket (P7 runs inside mode 6's end).

---------------------------------------------------------------------------------------------------------------------
## C. The user's goal

| The goal | As designed |
|---|---|
| Both get the dissolve and go to the Tome room as before separate rooms | Yes when P2 marks the trip [inferred from point 2]. Not when P2 refuses (point 8), or when the bring waits for player 2 (point 7) |
| The world changes properly | The game's own scripts in room 5 with both there [read there]. Karim's 1349 for T = player 2 is still open (§11.2); in Karim's chapter only "together" can happen (F6), so 1349 reads room 338 either way [inferred] |
| Both can cast | Yes: the find goes into player 1's list, and "has the Tome" is answered from either list (23855-23885) [read] |
| Each back in their own room | Yes by the design's order, every first return being a fresh load (point 1) [inferred] |
| Each back at their own spot | O: yes, if the mark survives (point 6). T: no by default. ED_TOME_BACK=marker puts T at the door's marker, 133 units outside the region in 303 (F21). The user's words ask for "the position they were at" [read there] |
| Solo untouched | Yes, behind `coop_stay()` (A) [read] |
