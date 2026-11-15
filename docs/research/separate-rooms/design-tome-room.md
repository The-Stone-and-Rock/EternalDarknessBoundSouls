# Design: the Tome room with the players apart (2026-11-14, revised after the two refutations)

The feature, in the user's words (2026-11-14): players apart; either takes the way into the Tome room; "each player will get
this dissolve transition to the tome room, each player goes to the tome room as it did before the separate room feature";
either picks up the Tome; "after the flashback scenes that play, each player returns to the position they were at when the
transition to the tome room occurred". World flags and magick as the game sets them. Co-op only (ED_ROOM_SLOTS=2,
ED_COOP_STAY=1); solo untouched; every chapter, every character, both players; the game's own functions.

Design only. Nothing was edited, built or run. Built on the four readers (tome-room-game-transition-readers.md,
tome-room-game-scenes-readers.md, tome-room-host-today-readers.md, tome-room-return-readers.md), the two refutations
(refute-tome-room-game.md, refute-tome-room-host.md, both checked point by point in §12), runtime/host/hle.c and main.c on
3542bf0 plus the working tree, and `ed` reads named below. Tags: **[read]** = seen at the address, script line, file:line or
log line given; **[read there]** = stated by the named reader or doc; **[inferred]** = worked out, not seen.

Words: **T** the taker (took the way in, from room **RT**, place **PT**); **O** the other (room **RO**, place **PO**); **K**
whoever picks up the Tome. The Tome room is room 5 (the game's one Tome room: `ed door 5` lists its only ways in, the seven
chapter doors, and chapter 13's film, §12 G3) [read]. On the way back: **A** is the player who takes the game's own return
ticket (the one with 868), **B** the other, who goes back by a ticket the host writes (§3 P7).

What changed from the first version, in short:
- The trip is marked by the ticket's room (r3 == 5), not by the dissolve word (§12 G2, G11, H3).
- Player 2's door runs at player 1's controller, so the dissolve word is player 1's copy either way; P9 is dropped (§12 H2).
- Every first return is a fresh load: the film's end kills the other slot's record (§12 G1, H1).
- The host's tickets are written from a per-frame step, not from inside the writer's intercept (§12 H5, G12).
- The return ticket with 868 goes to **player 1 whenever player 1 goes back to RT** (always in Karim's chapter), so 1349's
  scene and zombies run as player 1's, as in solo (§12 H14, design §11.2 of the first version).
- The come-into mark waits to be used before the next ticket clears it (§12 H6).
- No refusals at the way in (cutscene, Tome copy turn); the bring has no cutscene gate (§12 H8, G6).
- A fallback with a test and an end, a clear at state load (§12 H9, H10).
- Saved-place arrivals use flags bit 0 (value 1), the bit 0x80152680 tests (`rlwinm. r0,r25,0,31,31`) [read].

---------------------------------------------------------------------------------------------------------------------------
## 1. The approach in one paragraph

A room ticket into room 5 written from another room is **the Tome trip**. That one change is run the way co-op ran every door
before separate rooms: as player 1's change, with no stayer window and no R4 bracket, the fade in both halves, and then the
existing bring in `coop_follow` puts player 2 in room 5 (beside player 1, or on the arrival spot with player 1 beside when
player 2 took the door). At the change the host notes each player's room, place, facing and camera. In room 5 the game runs
untouched with both players there: 2675, the Tome, the find, 1599's film. When the game writes its way back out of room 5
(1599's `fn66`), the host keeps that ticket and voids the writer. On the next frames, from the behaviour pass, it first writes
B's ticket with the game's own writer (0x80155770): B's noted room, the game's "arrive at a saved place" (flags bit 0, slot 0
of 0x805CC2BC lent for the change), B's camera, no script. When B's change is over and B's first look has used the come-into
mark, it writes the kept ticket as A's (A = player 1 when player 1 goes back to RT, otherwise player 2). The game then takes A
back to RT at its return marker (or A's own spot, see §5) and runs 868 (and Karim's 1349) as A, in RT. Every piece is behind
co-op on and the stay.

---------------------------------------------------------------------------------------------------------------------------
## 2. The facts it rests on

### The way in
- F1. The only literal `fn66(5, ...)` calls are the seven chapter doors L03/892 (room 150), L04/132 (338), L06/335 (303),
  L07/1111 (115), L08/327 (204), L10/1215 (17) and L11/358 (43). Each plays the dissolve (`ed284(1)`, `ed285(marker, radius,
  strength)`), waits 120 ticks, then calls `fn66(5, EntryMkr01, Camera02, 2|3, 1161, 0)` [read `ed door 5`; reader 1 §2;
  refute-game §17]. Transition 2 is 12 ticks, 3 is 62 [read `ed door 5`].
- F2. Chapter 13 reaches room 5 by a film, not a ticket: L13/1828 in room 64 saves the place and camera (`ed164`, `fn134(1,5)`)
  and plays film 134, whose row loads room 5 directly at mode 6's start (0x800503B4-0x800503D0 → 0x801525D4 → 0x80152298)
  [read build/ed-db/scripts/L13/1828.lua; `ed dis 0x800503A0`; `ed dis 0x801525D4`]. Its way back is 1413's `fn66(64,
  EntryMkr01, FightCam04, 2, 1828, 0)`, and 1828 then restores the one saved place [read `ed door 5 --level 13`; the script].
- F3. `ed284(on)` reaches 0x80044200, which writes r13-25680 := on, and r13-25684 := 2 on "on" [read `ed dis 0x80044200`].
  0x80044254 counts r13-25684 down each frame while r13-25680 is 0 [read `ed dis 0x80044254`].
- F4. A door script of player 2's runs at player 1's controller: their press and their script timers both start it through
  `coop_use_start` (hle.c:21946-21947, 21482-21491), which swaps the identity, room, kinds, script words, zones and cameras,
  not the controller block r13-25680..-25632 (hle.c:18862-18905) [read]. That block is swapped only for player 2's turn
  (hle.c:21839-21841, 22068-22077) and their words' answers (16654-16831, 20730) [read]. So `ed284` and the ticket of any door
  see player 1's copy [inferred]. Logs: run-20261106-085728-jtg-dc1-c4p2climb, 132 "runs for player 2" at lr 0x7FFF0070
  [read there: refute-host §B2].
- F5. The door script tests "the player is the chapter's own model" and "the Tome is not carried". The host answers both for
  either player (`hle_object_model` hle.c:11214-11236; `coop_script_item` hle.c:23885-23921) [read there: readers 1, 2].
- F6. In L04/132, room 5 is chosen when the model is 150 and neither 733 nor 831 is carried; otherwise room 339 [read
  `ed script 132 --level 4`, refute-game §17]. Room 338's only way out is that ladder [read `ed door 338 --level 4`]. So in
  Karim's chapter the players are always together in 338 at the trip [inferred].

### Today's host at a change under the stay
- F7. `coop_stay()` is cached once and needs ED_COOP_STAY and two slots (hle.c:14328-14333); the frontend sets both only with
  co-op on (frontend.cpp:482-503) [read there: refute-host §A].
- F8. `notify_room_ticket` (hle.c:11424-11456) sets `g_coop_entered_p2` by identity (r13-21496 == player 2's +16) or player
  2's way out, `g_coop_room_p2 = g_coop_entered_p2`, counts player 1's tickets, sets `g_fade_mover` under the stay, clears
  `g_coop_arrival_scene`, `g_coop_p1_own_camera` and **`g_follow_fresh`**, then calls `split_ticket_open` [read].
- F9. `notify_ticket_run` (0x80152614's entry) opens the R4 bracket only when `g_coop_room_p2` (hle.c:14405); the bracket
  swaps player 2's identity, sets r13-23276 to their room and puts their camera set in; it does not swap the script states
  (hle.c:14395-14430; `coop_states_swap` callers 17502, 18876, 19476, 30358) [read]. `coop_stay_change_end` closes it at the
  behaviour pass (`notify_behaviour_pass`, lr 0x80008400 / 0x80050A9C, hle.c:14432-14494) [read].
- F10. `coop_follow` (hle.c:22461) returns at once while `split_on` (22492); the stay branch (22532-22540) leaves player 2 where
  they are while `coop_stay_apart`; otherwise the bring (22541-22621): player 2 beside the arrival spot, or on it with player 1
  beside when `g_coop_entered_p2`; facing copied; room stamped with 0x801F64C8; `g_follow_fresh` = 1 or 2 with
  `g_follow_fresh_until` = frame + 3600; look and find reset; a door motion seen out. The cutscene test is on the fall rescue
  only (22511), not on the bring [read].
- F11. The strips spare `coop_stay_stayer()` while `coop_stay_apart` (hle.c:14346-14368). The tail
  (`notify_room_slots_tail`, 15062-15087) puts back only the record of the room in play and the stayer's room [read].
- F12. Every 0x80152298 marks the other slot's record dead at 0x801523A8-0x80152404 when table +4 is not -1; flags 0x40 skips
  only the room-script start (0x80152408-0x8015242C) [read `ed dis 0x801523A8`]. `room_slots_return` (15092-15119) then reports
  a return into a dead record as "nobody stayed in it: loaded fresh", and swaps the slot indices when player 2 is apart in the
  slot the engine would unpack into (only with `!g_coop_room_p2`) [read].
- F13. `coop_script_go_to_room` (the intercept on fn66, 0x801666A4, hle.c:17400) passes over a fn66 in the other's door window
  (M13 not built) and one while another change is on its way (R4) [read hle.c:17421-17445].
- F14. `split_ticket_ask` is the writer's intercept whenever `coop_stay()` and the code checks pass (hle.c:31855-31870); it
  does not depend on ED_SPLIT_HOLD; `notify_room_ticket` is its notify and runs on passthrough [read]. A notify's guest calls
  do not change whether the recompiled function runs (hle.c:5641-5666) [read].
- F15. `r4_fade_quad` marks the fade out of the stayer's half only while `g_fade_mover != 0` (hle.c:25904-25905) [read there:
  refute-host §A].
- F16. `coop_follow_look` (hle.c:21003-21030) is the brought one's first look with the zones on: the game's look, with what a
  way back asks for passed over (`g_follow_refusing`), then `g_follow_fresh = 0` [read].

### In the Tome room
- F17. 2675 spawns the Tome (733) only when none is in the room; its start at the tail is skipped with flags 0x40, so the
  film's 0x44 reload does not restart it [read there: refute-game §17].
- F18. 2026-11-08 follow mode, both in room 5, player 2 took the Tome: the find went into player 1's list, 1599 ran as player
  1 and its ticket took player 1 to room 150, and the bring put player 2 beside [read there: reader 2 §2;
  logs/run-20261108-165807-p2-tome-moves page-0001.log:3079, 3245, 3259-3260]. Since steps C and F, a find of player 2's runs
  its after-script as their use (hle.c:19338-19355) and a cinematic end of a script marked theirs runs as their use
  (19518-19535) [read], so with K = player 2, 1599 and its ticket are now likely player 2's [inferred; no run since 11-11 shows
  it: refute-game §5].
- F19. 1599's Karim branch tests `room() == 5` [read `ed script 1599 --level 4` line 26]; the other chapters' branches have
  no room test [read there: readers 1, 2]. Both players are in room 5 at the pickup by this design, so either way it holds.
- F20. The film's end reloads the room in play with flags 0x44 (0x80157584 → 0x80152298), finds it in the current slot and
  does not unpack (0x80154D64-0x80154DB8) [read there: refute-game §1, §17].

### The way back
- F21. 1599 state 1: `fade_set(white)`, `stream_play(81, ...)`, `fn66(RT, EntryMkr_TomeReturn, Camera01|Camera04, 0, 868,
  0)`; RT is always the room of that chapter's door (150, 338, 303, 115, 204, 17, 43) [read `ed script 1599 --level 4` lines
  19-22, 34-37; `ed door 5`]. Its ticket's flags are 0 [inferred from fn66's `or r7,r23,r23`, 0x801668D4].
- F22. fn66 writes tickets at 0x801668E8 (lr 0x801668EC) and 0x80166930 (lr 0x80166934); the let-go 0x801F6828 is before the
  writer (0x801667A4), the sound cut 0x801A05EC(0,1,0) after it (0x801668EC-0x801668F8) [read `ed dis 0x80166780`, `0x801668D0`;
  grep `bl 0x80155770` in disasm.txt].
- F23. EntryMkr_TomeReturn sits on the marker the door's swirl centres on (`ed285`) in all seven rooms; in 303 it is about
  133 units past BoxRegion02's edge [read there: refute-game §17; reader 1 §6].
- F24. 868: `fade_set(white)`, `fade(0,0,0,0,40)`, then by model `run_next` 1349 (150), 1599 (18), 531 (74), 1061 (73), 1161
  (81) [read `ed script 868`]. 531, 1061 and 1161 only set Camera01 and ease, no waits [read `ed script 531|1061|1161`].
  `run_next` stores the id at r13-23220 and 0x801642D4 runs it at 0x80164470-0x80164484 as the current script returns, so
  inside the same call [read `ed fn run_next`; `ed dis 0x80164440`]. The runner calls 0x801642D4(script) at 0x801527F4 [read].
  So 868 and its follow-up start inside the change that carries it [inferred]. 1349 (Karim, `if room == 338`) plays a scene,
  sets flag 522, wakes zombies 174-178 against "the player", reruns 132 [read there: readers 1, 2]; its later states resume
  from the timers of the table they were started in [inferred from F9: the bracket does not swap script states].
- F25. "Arrive at a saved place": flags bit 0 (value 1) makes the runner call 0x8016490C(0) in place of the marker lookup
  (0x80152680-0x80152690); the heading at 16(r1) is left unset [read `ed dis 0x80152680`; refute-host §A]. Slot 0 of
  0x805CC2BC (three s16) is what `fn104(0, thing)` writes, in 152 calls over 131 scripts [read there: refute-game §12].
- F26. The writer 0x80155770: r3 room, r4 entry, r5 camera, r6 script, r7 flags, r8 countdown (at least 1), r9 delay, r10
  transition; the caller's r1+8/+12/+16 go to ticket +36/+40/+24, the 7th (+24) must be 1 for the +592 stamp (0x80154D20)
  [read there: refute-host §A, refute-game §17]. `hle_guest_call` sets r3-r10 and r1 := (r1 - 0x800) & ~15 (main.c:72-82)
  [read], so the stack words go at that r1 + 8, +12, +16.
- F27. 0x801F5620(61, 0, 0, room) with f1 = -22248(r2) is the arrival's "left the room" event to r13-23288's room
  (0x801520E0-0x801520F4) [read there: refute-game §17].
- F28. Player 2's camera set (the records 0x80602B18.., the stack, depth r13-21700 and index r13-26864) is held in
  `g_coop_cameras.p2` outside their turn, in memory in their turn or while lent (`coop_camera_set_io`, hle.c:16207-16225, 16258,
  16267) [read]. The hash: entry `0x80603178 + index*136 + 124`, hash at the entry's +52 (hle.c:18610-18617) [read].
- F29. The stay's sound rule at a change: `coop_sound_stream_drop` stops only the direct streams of rooms nobody is in, keeping
  the other player's room (hle.c:14734-14752); each stream start is noted with the room in play (14627-14636) [read].

### Magick and the world
- F30. Casting asks 0x80109A68: yes when the caster's list holds type 30 or 120 (0x80109B10, 0x80109B1C) [read there:
  refute-game §17]. The host answers "has the Tome" from either list (`coop_has_tome`, hle.c:23855) [read there: refute-host §C].
  Nothing in room 5's pickup writes the spell record [read there: reader 2 §5].

---------------------------------------------------------------------------------------------------------------------------
## 3. Pieces of host code

All in runtime/host/hle.c. Every piece is gated on **`tome_trip_on()`** = `g_settings.mod_coop && g_coop_p2 != 0 &&
sda_ready(cpu) && coop_stay() && tome_trip_switch()`. `coop_stay()` needs co-op on and two slots (F7), so solo, follow mode
and one slot never reach any of it [inferred from F7]. No new install: every hook below is an existing intercept or notify
(the writer's `split_ticket_ask`, `notify_room_ticket`, `notify_room_change`, `notify_ticket_run`, `notify_behaviour_pass`,
`coop_follow`, `coop_script_go_to_room`, `tw_state_loaded`), each already installed only with its own address checks.

### P1. The trip record (new, beside the stay's state, after `coop_stay_spared`, ~hle.c:14370)
```
#define TOME_ROOM 5   // the game's one Tome room (`ed door 5`)
static struct {
    int state;            // 0 off, 1 going in, 2 in the room, 3 kept (way back held), 4 B going back, 5 A going back
    int taker;            // 1 or 2
    unsigned tickets;     // g_coop_tickets at the trip's ticket
    struct { bool noted; s32 room; float place[3]; u32 facing[4]; float heading; u32 camera; } p[2];  // at the change
    u32 regs[8], stack[3];  // the kept way back (r3-r10, r1+8..+16)
    int a, b;             // who takes the kept ticket (A), who gets the host's (B; 0 = nobody)
    bool b_exact, a_exact;  // placed by saved place (flags | 1) rather than the marker
    bool slot0_lent; u8 slot0[6];   // 0x805CC2BC slot 0, lent for one change
    bool guard;           // our own writes to 0x80155770 pass through
    unsigned long long frame;  // the frame the current step began (for the timeouts)
} g_tome_trip;
```
- `tome_trip_switch()`: ED_TOME_TRIP cached, default 1. 0 = today's behaviour exactly (apart, player 2 can then go alone
  into room 5: a known failure, kept only to compare).
- ED_TOME_BACK=marker|exact (default marker): T's place on the way back (§5, a decision for the user).

### P2. The trip begins: `notify_room_ticket` (hle.c:11424)
- Where: right after `g_coop_room_p2 = g_coop_entered_p2;` (11437) and again after the `g_fade_mover` line (11441).
- Condition: `tome_trip_on()`, state 0, **r3 == TOME_ROOM**, and the asker's room is not TOME_ROOM (the asker: player 2 when
  `g_coop_entered_p2`, else r13-23276). No refusal for a cutscene or the Tome copy's turn (§12 H8); each is logged as a WARNING
  on the trip line. The dissolve word r13-25680 is logged, not tested (§12 G2, H3).
- Record: state 1, taker = `g_coop_entered_p2 ? 2 : 1`, tickets.
- Make it a change of the old kind:
  - `g_coop_room_p2 = false` before line 11438: no R4 bracket (F9), player 1's ticket counted, the slot swap in
    `room_slots_return` allowed (F12), the sound rule's mover 1 (F29). The game moves player 1.
  - `g_coop_entered_p2` left as computed, so the bring puts player 2 on the arrival spot when they asked (F10), and the arrival
    scene is theirs as before separate rooms (hle.c:19428-19447) [read].
  - `g_fade_mover = 0` after 11441: the fade in both halves (F15).
- `split_ticket_open` (hle.c:25935): `else if (g_tome_trip.state == 1 && tickets match) no = "the Tome room: both go";` before
  the cutscene test. No stayer window; the stayer is held through the 12 or 62 ticks "as today" (§12 H11).
- Log: `[tome-trip] player %d's door from room %d to room %d: both go there (dissolve word %d%s; frame %llu)`.

### P2b. A trip that could not be asked for: `coop_script_go_to_room` (hle.c:17400)
- At each of its three pass-overs (the stayer's way out, M13, R4's one-at-a-time; 17406-17445), when `tome_trip_on()` and
  r3 == TOME_ROOM: call 0x80044200(0) (the door's dissolve off, which nothing else would turn off: 2675 never runs) and print
  `[tome-trip] WARNING: player %d's way into room 5 passed over (%s): the dissolve put off; take the door again`.
- This is the case of both doors within about two seconds (§12 G13, H16).

### P3. Both players noted: `notify_room_change` (hle.c:11361), at the trip's change only
- At 0x80152298's entry, before `room_slots_return`, while state 1, r3 == TOME_ROOM and `!p[*].noted`. For each player:
  - room = their object's +592; place = +0..+11; facing words +44..+59; heading = `2*atan2(+52, +56)` (as hle.c:22533 logs);
  - camera hash by F28's recipe: player 1 from memory; player 2 from `g_coop_cameras.p2` (the index at
    `COOP_CAMERA_BYTES + 4`, the record bytes at offset `0x80603178 - 0x80602B18 + index*136 + 124`, then the entry's +52 in
    guest memory), or from memory when `g_coop_cameras.lent` [inferred from F28: the set's layout is read, the read-back not].
- Also here for chapter 13's film way in, when P12 is built (§3 P12).
- Log: `[tome-trip] player %d noted in room %d at %.0f %.0f %.0f facing %.2f camera 0x%08X`.

### P4. The change itself: no new code on the change path
- T is player 1: a change of player 1's under the stay: `room_slots_return` evicts RT, player 2 is spared, RO's record is
  put back at the tail and their hold let go (F11, F12) [inferred from the reads; the run fx-a-1 shows this change alone].
- T is player 2: the game moves player 1 from RO to EntryMkr01 and sends 61 to RO (F27). `room_slots_return` swaps the indices
  (player 2 apart in RT in the other slot), so RO is evicted; player 2 is spared in RT, and RT is put back [inferred].
- Pre-stay arrival pieces for "player 2's door moved player 1", gated off under the stay today, open for the trip's change
  (`tome_trip_change()` = state 1 and the change not yet over):
  - `coop_p1_unplaced` (hle.c:20990: `|| coop_stay()` becomes `|| (coop_stay() && !tome_trip_change())`);
  - the arrival scene's `g_coop_p1_own_camera` (hle.c:17995: `&& !coop_stay()` becomes `&& (!coop_stay() ||
    tome_trip_change())`).
  These are the only `coop_stay()` gates on `g_coop_entered_p2`'s readers (grep of g_coop_entered_p2: 11436, 17995, 19377,
  19432-19447, 20990, 22555-22570) [read]. That they suffice is inferred; this route has not been played since the stay came
  in (§11).

### P5. The bring: `coop_follow` (hle.c:22532)
- The stay branch's test becomes `coop_stay_apart(cpu) && !tome_trip_brings(cpu)`. `tome_trip_brings` = state 1, the trip's
  change over (player 1's room is TOME_ROOM, busy 4 clear, ticket countdown out), player 2 not casting
  (`split_stayer_casting`), player 2's copy Tome closed (`!g_tome_copy_open`). **No cutscene gate**: the pre-stay bring had
  none (F10), and Anthony's arrival scene starts at once (§12 G6). Then the existing bring runs as written.
- `coop_stay_apart` itself is left alone, so the strips and player 2's turn treat them as apart until the bring [inferred].
- After the bring line, for the room player 2 left (call it L):
  - if no player is in L now and L is not the room the game's change left (r13-23288 at the change, which the game already
    sent its 61), broadcast `0x801F5620(61, 0, 0, L)` with f1 = the float at r2-22248 (F27) [inferred: what the game's arrival
    does for the room it moves player 1 out of];
  - stop L's direct streams: `coop_streams_stop_except(cpu, TOME_ROOM)` (F29) [inferred: nobody hears L now].
- State 2. Log: `[tome-trip] both in room 5: player 2 brought from room %d (event 61 to it: %s; %d streams stopped; frame %llu)`.
- If the bring is still waiting when the way back is kept (player 2 casting or in their copy Tome the whole time): P6 treats
  player 2 as not on the trip (they stay where they are).

### P6. The keep: the writer's intercept `split_ticket_ask` (hle.c:25999), a branch before its `split_counting` test
- If `g_tome_trip.guard`: passthrough (our own tickets).
- Condition: `tome_trip_on()`, lr is fn66's (0x801668EC or 0x80166934, F22), the asker's room is TOME_ROOM (the asker by
  identity: player 2's object +592 when r13-21496 == player 2's +16, else r13-23276), r3 != TOME_ROOM.
  - The running script (r13-23228) is one of room 5's ways out (1599, or 1413 in chapter 13: `ed door 5`) [read]: **keep it**.
  - Any other (a door script of O's, started before the trip, writing from room 5: §12 G13): void it and print
    `[tome-trip] WARNING: a ticket out of room 5 by script %d (room %d) refused`.
  - A ticket by lr 0x8012D2F0 (the film's own) or any other lr: passed through and printed as a WARNING [§12 G8: not seen for
    rows 112-120].
- Keep r3-r10 and the caller's r1+8, +12, +16 (F26). Void the writer (`g_hle_passthrough = false; g_hle_tail_call = false;`)
  as G1 does. fn66 goes on to its sound cut (F22); the stay's sound rule takes it with the last change's mover [inferred].
- Who goes where (the record, or a record made now when there is none):
  - With a record (state 2, or state 1 with player 2 never brought):
    - On the trip: each player whose room now is TOME_ROOM.
    - A = player 1 if player 1 is on the trip and their noted room is RT (the kept r3); else player 2 if on the trip; else
      nobody (impossible: the asker is in room 5).
    - B = the other player, if on the trip; else 0 (they stay where they are).
    - A's place: the marker (the kept entry) when A is T; A's noted place (`a_exact`) when A is O (together, T = player 2).
    - B's place: their noted place (`b_exact`), except B = T (together, T = player 1 never; T = player 2 with A = player 1):
      then the kept entry and camera, unless ED_TOME_BACK=exact.
  - **Fallback, no record** (a state loaded in room 5, a trip begun with ED_TOME_TRIP=0, §12 H9): note each player in
    TOME_ROOM as "noted in RT at the kept entry" and each player elsewhere as not on the trip. Then the same rules: both in
    room 5 → A = player 1, B = player 2, both at the game's spot, as co-op did before separate rooms; one in room 5 → A =
    that one, B = 0. Print `[tome-trip] NO RECORD: %s`.
- State 3, `g_tome_trip.frame` = now. Log: `[tome-trip] the way back kept: room %d entry 0x%08X camera 0x%08X transition %d
  script %d (script running %d, lr 0x%08X); A player %d (%s), B player %d (%s) (frame %llu)`.

### P7. B goes back first: `tome_trip_step`, called from `notify_behaviour_pass` (hle.c:14491) after `coop_stay_change_end`
- Runs once a frame, outside any intercept (§12 H5). In state 3, when busy 4 is clear and no ticket is counting
  (`COOP_ROOM_TICKET` +0 <= 0), and B != 0:
  - Arguments: room = B's noted room; entry = 0 (exact) or the kept entry; camera = B's noted hash (exact) or the kept
    camera; script 0; flags 1 (exact) or 0; countdown 1; delay 0; transition 0; stack words 0, 0, 1 at the guest-call stack
    +8..+16 (F26).
  - When B is player 2: `coop_be_player2(cpu, true, was)` and `coop_stay_room_in(cpu, keep)` around the call, as a use of
    theirs runs (hle.c:18872-18874), so the ticket is theirs (R4 bracket at the runner, F9) and the busy set's event 102 goes to
    the room they are in [inferred: §12 H18].
  - guard on, `hle_guest_call(cpu, 0x80155770, regs, 8, 0.0, &ignore)`, guard off. Its notify runs as for any ticket.
  - State 4.
- Slot 0 (exact only): lent at `notify_ticket_run` (0x80152614's entry) of that change, not at the write (§12 G12): save its 6
  bytes, write B's noted place as three s16 cut toward zero (as fn104 cuts), `slot0_lent`.
- At that change's end (the behaviour pass after it: `coop_stay_change_end` for player 2, the same pass for player 1):
  - Face B with `0x80124BE4(object, heading)` (as hle.c:9652 does) when exact (F25: the runner's heading is unset).
  - Mark what B stands in as come into: `g_follow_fresh = B`, `g_follow_fresh_until = frame + 3600` (the bring's own rule,
    F10, F16) [inferred: doors.md:77].
  - Slot 0 put back.
  - Log: `[tome-trip] player %d back in room %d at %.0f %.0f %.0f facing %.2f (noted %.0f %.0f %.0f %.2f) (frame %llu)`.
- B's room is always loaded fresh: the film's end left its record dead (F12, F20; §12 G1). `room_slots_return` prints
  "nobody stayed in it: loaded fresh".

### P8. Then A takes the game's own way back (the same step)
- In state 4 (or state 3 when B = 0), when B's change is over, busy 4 is clear, no ticket counting, and **B's come-into mark is
  used** (`g_follow_fresh == 0`; `notify_room_ticket` would clear it, F8 and §12 H6), or 120 frames have passed (then a
  WARNING):
  - The kept registers and stack words; with `a_exact` (A = O, or ED_TOME_BACK=exact): flags | 1, slot 0 lent at the runner
    as in P7, facing at the end.
  - Wrapped as player 2's (P7's wrap) when A is player 2.
  - guard on, the guest call, guard off. State 5.
- That change:
  - A = player 1, B = player 2 elsewhere: `room_slots_return` sees player 2 apart in the other slot and swaps; RT is unpacked
    into room 5's slot [inferred from F12].
  - A = player 1, B = player 2 in RT (together): RT is in the other slot, put back at B's tail (a player is in it): "kept";
    no load [inferred].
  - A = player 2 (T = player 2, apart): an R4 bracket; RT is unpacked into the former other slot, room 5's [inferred from F9].
  - The game places A, runs 868 as A inside the change (F24): its white fade-out in A's half, its camera follow-up in A's set;
    in Karim's chapter A is player 1 (F6), so 1349 and its later states run as player 1, in 338, as in solo.
- At its end (the behaviour pass after): A's come-into mark (`g_follow_fresh = A`) when A was placed by saved place; state 0;
  the record cleared. Log `[tome-trip] over: player 1 in room %d at ..., player 2 in room %d at ... (frame %llu)`.

### P10. The clear at state load: `tw_state_loaded` (hle.c:13566)
- `memset(&g_tome_trip, 0, sizeof g_tome_trip)` (§12 H10). A slot 0 that was lent is not put back (the state has its own).

### P11. The trip's timeouts
- In `tome_trip_step`: state 1 with no change after 600 frames (the ticket lost), or state 3/4 with nothing to write after
  600 frames: print `[tome-trip] WARNING: stuck in state %d: cleared`, put slot 0 back, clear the record.

### P12. Chapter 13 (later; the user's call, §11)
- Way in: `notify_room_change` with r3 == TOME_ROOM, lr 0x801525FC (0x801525D4's) and the room in play not TOME_ROOM, state 0:
  begin the trip as P2 does (no ticket: the change is player 1's already), note both (P3), state 1.
- The rest as P5-P8: the kept ticket is 1413's `fn66(64, ..., 1828, 0)`, and 1828 restores the saved place for A as the game
  does (F2) [inferred]. Who ran 1828 (and whose place `ed164` saved) when player 2 walked into its region is not read.

---------------------------------------------------------------------------------------------------------------------------
## 4. The sequences

Frames are the change's own; "the bring" is `coop_follow` in the same or the next frame [inferred].

### 4.1 Player 1 takes the way in, player 2 elsewhere (RT ≠ RO)
1. Player 1 in RT walks into the door region; the door script plays the dissolve, waits 120 ticks, calls `fn66(5, ...)`.
2. Ticket: P2 marks the trip. No window; the fade covers both halves; player 2 is held through the countdown in RO.
3. Change: P3 notes both. RT is evicted, room 5 unpacks there, player 2 is spared and RO put back, player 1 at EntryMkr01.
4. The bring: player 2 beside player 1 in room 5; RO gets 61 and its streams stop. State 2.
5. K takes the Tome. 1599 runs (as player 1's, or as player 2's use when K is player 2: F18), the film plays full screen, the
   film's end reloads room 5 (no unpack) and marks RO's record dead (F12, F20).
6. 1599 state 1: white, `fn66(RT, EntryMkr_TomeReturn, ...)`. P6 keeps it: A = player 1 (noted in RT), B = player 2 (exact).
7. P7: player 2's ticket to RO (R4 bracket): RO loaded fresh into the other slot; player 2 placed at PO, faced, marked.
   Player 1 waits in room 5 for those frames (§7).
8. Player 2's first look uses the mark. P8: the kept ticket as player 1's: RT unpacked into room 5's slot; player 1 at the
   marker; 868 runs.

### 4.2 Player 2 takes the way in, player 1 elsewhere (RT ≠ RO)
1. Player 2's door script runs at player 1's controller (F4); the dissolve word is player 1's copy.
2. Ticket: P2 makes it a change of player 1's, `g_coop_entered_p2` kept.
3. Change: P3 notes both. The game moves player 1 from RO to room 5 (RO gets 61); RO is evicted; player 2 is spared in RT.
4. The bring with `g_coop_entered_p2`: player 2 on the arrival spot, player 1 beside; RT gets 61 (P5). State 2.
5. As 4.1 steps 5-6, with A = player 2 (player 1's noted room is RO, not RT), B = player 1 (exact).
6. P7: player 1's ticket to RO (no wrap): RO loaded fresh into the other slot; room 5 stays, player 2 in it.
7. P8: the kept ticket wrapped as player 2's: an R4 bracket; RT unpacked into room 5's slot; player 2 at the marker; 868 as
   player 2 (white fade-out in their half, Camera01 in their set; no 1349 outside Karim's chapter).

### 4.3 Both in the same room at the way in (RT == RO; Karim's chapter always)
- The way in: 4.1 or 4.2 with RO = RT. Room 5 unpacks into the other slot; RT keeps its slot and is put back at the tail,
  because player 2, the stayer, is still in it [inferred from F11]. With T = player 1, the game's arrival already sent 61
  to RT; P5 skips its own 61 then (L == r13-23288 at the change).
- The way back: A = player 1 always (player 1's noted room is RT).
  - T = player 1: B = player 2 to RT at PO (fresh load), then player 1 by the kept ticket to the marker ("kept").
  - T = player 2: B = player 2 = T to RT at the marker (fresh load), then player 1 by the kept ticket at PO (flags | 1),
    "kept".
  - 868 and Karim's 1349 run as player 1 in 338, as in solo [inferred from F24, F6].

---------------------------------------------------------------------------------------------------------------------------
## 5. Back to their own room and spot

1. **O** goes back by the game's own saved-place arrival (F25): their noted room, place, facing and camera. Only slot 0 is
   lent for the one change. The room is loaded fresh, as solo does at every return (F12) [read there: reader 4 §6].
2. **T** goes back to RT at EntryMkr_TomeReturn by default: the spot the game made for coming back through that door (F23).
3. **T exactly at PT** (`ED_TOME_BACK=exact`): the same means as O. The user's words ask for this ("the position they were
   at"). Not the default yet, because:
   - in chapters 3 and 6 the way in is an unprompted zone exit, so PT lies inside the exit; the come-into mark should keep
     it from firing (F16) but this is not tried [inferred];
   - in 303 the game put its marker outside BoxRegion02; the marker and PT may differ by up to the region's size (338's
     Region01 is 377 by 434 units) [read there: reader 1 §6].
   **For the user to choose** (§11).
4. **A player never brought** (P5 still waiting at the keep): they stay where they are; only those in room 5 go back.
5. **RO is RT's neighbour** (O below a ladder in 211 while T is in 204): nothing special.
6. **RO is room 5**: can't happen; the only ways in are the trip's doors [read `ed door 5`].

---------------------------------------------------------------------------------------------------------------------------
## 6. World flags and magick for both

- The game sets everything itself in room 5, with both players there: 852 sets flags 614 and 679; 1599 clears both and sends
  event 82; Peter's event 57s go to things placed in room 27 by "any room" lookup [read there: reader 2 §6; refute-game §17].
- Both players' doors take the "Tome carried" branch afterwards, and both can cast (F5, F30). **Nothing to add for magick.**
- Karim's world change (868 → 1349: flag 522, zombies 174-178 against "the player") runs as player 1's in 338, with A =
  player 1 always there (§4.3). That is the game's own path, and pre-stay co-op's.
- The other chapters' 868 follow-ups are cameras only, run inside A's change in A's set (F24).

---------------------------------------------------------------------------------------------------------------------------
## 7. The dissolve and the fades for both

- The way in: the door's `ed284`/`ed285` effect is drawn while player 1's copy of r13-25680 is on, outside any turn (F3, F4).
  For either player's door it is player 1's copy that the door turned on and 2675 turns off [inferred]. With `g_fade_mover =
  0` the change's fade is in both halves (F15). Open: `ed285` scales the strength by the main player's distance to the marker
  (fog-host-readers.md:106, read there); for player 2's door the main player is player 1 in RO, so the dissolve may be faint
  or full in both halves. Check in run 4's pictures.
- The way back: 1599's white covers the frame. B's change is an ordinary R4 or player-1 change, so its fade is marked out of
  A's half (`g_fade_mover` = B). A's half may show room 5 for B's change and the wait for B's look (about 10-30 frames)
  before A's own change [inferred]. To judge in the pictures; if it shows, the fix to try is `g_fade_mover = 0` for B's change
  (then A's half shows B's 10-tick fade-in instead).
- The music: 1599's stream 81 is noted with room 5 (F29). B's change keeps room 5's streams (A is there); A's change keeps B's
  room's and so may stop stream 81 [inferred]. Whether solo keeps 81 through its own change was not read. Check: `[sound]`
  lines in run 1 against solo run 6; if 81 stops, re-note its start with RT at the keep.

---------------------------------------------------------------------------------------------------------------------------
## 8. What it must not break

- **Solo:** every piece behind `tome_trip_on()` → `coop_stay()` → co-op on (F7). Check: run 6.
- **Follow mode (ED_COOP_STAY unset):** `coop_stay()` false; today's bring does the Tome room as on 2026-11-08 (F18). Run 7.
- **The stayer's door work** (R4, split window, stayer presses, R4b/R4c, effects, fog, lights, sound): only the trip's change
  skips them. B's and A's changes back are ordinary changes. Check: run 3's last steps.
- **The Tome copy:** the bring waits while player 2's copy is open (P5). Check: run 3, player 2 opens their Tome after.
- **Saves:** no format change. A state loaded during the trip clears the record (P10) and the fallback takes those in room 5
  back to the game's spot (P6). A host state taken in room 5 loses "each to their own room" (§11).
- **The dead-record rule** (`room_slots_return`): unchanged; the returns rely on it (F12).
- **1599's other branches** (Max 258/255, Edward in place, chapter 13 to room 64): no ticket into room 5 and no keep (P6 needs
  the asker in room 5), so they run as today [inferred]. Max: see §11.
- **Karim's ladder after the return:** 1349 reruns 132, which goes to 339 for either player (F6).

---------------------------------------------------------------------------------------------------------------------------
## 9. Switches and log lines

- `ED_TOME_TRIP` (default 1 with the stay; 0 = today, no `[tome-trip]` line). `ED_TOME_BACK=marker|exact` (default marker).
- `[tome-trip]` lines are always printed (rare events): begun; each noted; both in room 5 (61, streams); kept (with A and B);
  each back (noted and actual); over; NO RECORD; and a WARNING for: a way in passed over (P2b); a refused ticket out of room 5
  (P6); a ticket by another lr out of room 5; a trip begun in a cutscene or the copy's turn; B's mark not used in 120 frames;
  a camera hash not found (0x801EE998 gives -1); a player placed more than 4 units from the noted spot; stuck (P11).
- EVENTS.log and `ed timeline` must list `[tome-trip]` lines [inferred: how EVENTS.log picks its lines was not read].

---------------------------------------------------------------------------------------------------------------------------
## 10. The runs that would prove it

Fresh Jump-to-Game only (`sh tools/ed/jtg.sh CHAPTER NAME`), **with `ED_RUN_AWAY=1` on every run: the window off the visible
desktop and the sound muted (jtg.sh:170-171, tools/ed/away.ps1), as the user asked, until they ask to see it.** Never
`ed test`, `ed chapters` or an old save until the user's desktop is free. Add `ED_COOP_STAY=1 ED_ROOM_SLOTS=2 ED_COOP_LOG=1
ED_ROOM_LOG=1 ED_RUN_SHOTS_MS=250`. `ED_RUN_SCRIPT=DELAY:SCRIPT` runs as player 1 (hle.c:18736-18770) [read there]. Player
2's door: `ED_PUT="frame,2,x,y,z"` into the region plus `ED_PAD2_SCRIPT="start,end,a"`. Step 0, once per chapter: a no-press
run, to read "control at frame F" and the start room from jtg-report.txt (chapters 8 and 11 not read yet).

1. **Ch 4, together, player 1 takes the ladder, player 1 takes the Tome.** ED_RUN_SCRIPT=F+60:132,F+120:132. In room 5:
   ED_PUT player 1 into CameraRgn02 (-1290, 0, 300), wait 150 frames for 852, then A for EXAMINE, Yes and the words.
   Pass: `[tome-trip] player 1's door ... both go`; no `[split] ... stays in room 338`; `player 2 brought to player 1's room
   0x00000005`; both.tsv 5/5 before the pickup; the Tome spawned once; the film; `the way back kept: room 338 entry
   0x37A365CD ... A player 1 (marker), B player 2 (exact)`; `[rooms] ... room 338 ... loaded fresh` at B's change and `kept` at
   A's; player 2 within 4 units and 0.1 rad of the noted spot; player 1 at (-950, -194, 1); 868 then 1349 as player 1, flag
   522, zombies 174-178 with a quarry; no hang report, no WARNING; pictures: the dissolve in both halves, white, then each half
   fading out; `[sound]` lines for stream 81.
2. **Ch 4, together, player 2 takes the ladder, player 2 takes the Tome** (ED_PUT 2 into Region01 (-940, -80, 0), A).
   Pass: the trip names player 2; player 1 moved by the game's change, player 2 on the arrival spot; the find in player 1's
   list; 1599 as player 2's use or player 1's (log which: F18); kept with `A player 1 (exact), B player 2 (marker)`; player 2
   placed at the marker first, then player 1 at their noted spot; 1349 as player 1.
3. **Ch 8, apart, player 1 takes the Tome door, player 2 below in 211.** Both to room 204 by the door scripts (from step 0).
   Player 2 takes L08/329 (ED_PUT 2 into Region01 (-150, 675, 0), A) to 211; then player 1 takes L08/327 (ED_RUN_SCRIPT 327
   twice). Pass: the trip from 204; player 2 brought from 211 with `event 61 to it`; the way back: player 2 to 211 at the
   noted spot, `loaded fresh`; player 1 to 204 at (256, -503, 6) with Camera04; then player 2 climbs back (L08/249, A) and
   `[stay] player 2's change ... runs as theirs`; player 2 opens their Tome and casts (`HAS_TOME`).
4. **Ch 8, apart, player 2 takes the Tome door, player 1 in 211** (player 1 by ED_RUN_SCRIPT 329 twice; player 2 into Region02
   (257, -580, 0), A). Pass: the mirror: player 1 back in 211 at the noted spot (fresh load), then player 2 by an R4 bracket at
   the marker in 204 with 868's fade in their half; pictures of the dissolve in both halves (§7).
5. **Ch 11, apart, either order** (the Major: L11/360 to 60, L11/358 to 5). Pass as 3/4, with the 62-tick fade and 868 → 1161.
6. **Solo, ch 4 (co-op off):** ladder, Tome, film, way back. Pass: no `[tome-trip]`, `[stay]`, `[split]` line; the same room
   changes and scripts as a solo run from before the build.
7. **Follow mode, ch 4 (ED_COOP_STAY unset):** as run 1. Pass: no `[tome-trip]` line, and 2026-11-08's sequence.
8. **ED_TOME_TRIP=0, ch 4, with the stay:** today's fx-a-1 behaviour, player 1 alone to room 5.

A run fails on any WARNING, any hang report, a player in the wrong room in both.tsv at the end, or a place more than 4 units
off. Read `ed timeline` and EVENTS.log first. Then, when the user asks to see it, show the pictures and give the exact presses
for both controllers.

---------------------------------------------------------------------------------------------------------------------------
## 11. Not read, open, and for the user

For the user to decide:
1. **T's spot on the way back**: the door's return marker (the game's spot, default) or exactly where T stood (the user's
   words). Both are built; one switch.
2. **Max's chapter** (1599 in 258 → 255 → 258, no Tome room, no dissolve): today player 1 (or whoever reads) goes alone and
   back; player 2 stays. Should the same "both go, each back" apply there? (Not in this build.)
3. **Chapter 13** (Alex, a film into room 5, the game itself saves and restores one place): build P12 so both go, or leave
   it to the game (one player goes)?
4. **The mansion Tome** (L00/1571: films 122 then 1, which loads room 174, a chapter start): outside this design; the stay
   leaves player 2 behind at chapter starts too [inferred]. A separate item.

Not read:
5. Player 2's find of the Tome since steps C/F: whether 1599 and its ticket are player 2's (F18). Run 2 shows it.
6. What 0x801EE764 does with camera index -1 (a noted hash not found in the room).
7. Whether `0x801F5620`'s 61 must be sent with the left room in play.
8. Who ran L13/1828 (and whose place `ed164` saved) when player 2 walked into its region (P12).
9. Whether player 2's copy Tome honours `deny_menu` (r13-26232) during the door's hold.
10. The 10-30 frames of room 5 in A's half between B's change and A's (§7), and stream 81 (§7).
11. `ed285`'s strength for player 2's door (§7).
12. The pre-stay gates in P4 are all of them: the route "player 2's door moved player 1" has not been played since the stay
    came in (6b2a584 onward).
13. A death in RO during the countdown (game over at least 120 frames after the death move, b1b2-ticket-death-readers.md §0):
    it can land in room 5 or in the film. Not handled.
14. What 0x8006E2F0 tests in fn66's gate 0x800440D0 (a refusal of the asker's own fn66) [read the gate's shape, not the test].
15. Player 1 in their own find (busy 32) when player 2's trip takes them: a prompted door of player 2's waits for player 1's
    look or find (`coop_player1_mid_use`, hle.c:21965) [read]; an unprompted zone exit (chapters 3 and 6) does not wait
    [not read].
16. EntryMkr_TomeReturn's place in 150 (a plane) and in 303 (outside the region).

---------------------------------------------------------------------------------------------------------------------------
## 12. Refutations, checked

Each point of refute-tome-room-game.md (G1-G16) and refute-tome-room-host.md (H1-H18), checked against the code here.

### [real] points: fixed in the design
- **G1 / H1, the other slot is dead at the way back.** Checked: 0x801523A8-0x80152404 marks it dead at every 0x80152298
  unless table +4 is -1, and flags 0x40 skips only 0x80152408-0x8015242C [read `ed dis 0x801523A8`]; the tail puts back only
  the room in play and the stayer's (hle.c:15067-15078) [read]. Both in room 5 at the film's end, so RO (or RT) is dead.
  Changed: P7, P8, §4 and runs 1, 3 now expect "loaded fresh" at B's change and "kept" only at A's in the together case.
- **G2, the dissolve word re-trips; H2, player 2's door runs at player 1's controller.** Checked H2: the press and the timers
  start player 2's scripts through `coop_use_start` (hle.c:21946-21947, 21482-21491), which does not swap the controller
  block (18862-18905); the block is swapped only in their turn and their words' answers [read]. So G2's premise (player 2's
  copy keeps the 1) does not hold, but the mark is still loose (G11). Changed: P2 tests r3 == 5 and only logs the word; P9
  (the per-player word) is dropped; §4.2 step 1 and §7 rewritten.
- **G3, chapter 13's way in.** Checked: L13/1828, film 134, mode 6's direct load 0x801525D4 → 0x80152298 [read]. Changed: F2,
  P12 (later, the user's call), §11.3.
- **G4 / H12, fn66's sound cut is after the writer.** Checked: let-go 0x801667A4 before, writer 0x801668E8, cut 0x801668F8
  after [read]. Changed: P6's wording; the cut still runs at the keep.
- **H4, the intercept is already installed with the stay.** Checked hle.c:31855-31870: `split_ticket_ask` goes in with
  `coop_stay()` and the code checks, not with ED_SPLIT_HOLD [read]. Changed: P6 needs no install; its branch is before
  `split_counting`.
- **H6, the next ticket clears B's come-into mark.** Checked: `g_follow_fresh = 0` at every ticket (hle.c:11449) [read].
  Changed: P8 waits for `g_follow_fresh == 0` (B's first look used it, F16) or 120 frames; B's mark is the bring's own
  `g_follow_fresh = B` with its until, for player 1 too (in place of 0x80134D48).
- **H7, with T = player 2 the unbrought one is T.** Checked: the trip is player 1's change, so player 1 is in room 5 at once
  and player 2 is the one P5 brings, whoever took the door [read hle.c:11436-11441 with P2]. Changed: P5 and P6 speak of "player
  2 never brought", and P6 sends back only those in room 5.
- **H8, a refused trip leaves player 2 alone in room 5.** Checked: today's R4 takes player 2 alone (hle.c:14405-14416), and
  only Karim's branch tests the room (F19). Changed: P2 refuses nothing (WARNING lines instead); the fallback in P6 covers one
  player in room 5; P2b covers a way in passed over.
- **H9, the fallback had no test and no end.** Changed: P6's test is lr + asker's room + the running script; the fallback
  builds a record and ends like any trip; P11's timeouts.
- **H10, a state loaded in the same process keeps the record.** Checked `hle_coop_prompt_load` → `tw_state_loaded`
  (hle.c:21515-21525, 13566) [read]. Changed: P10.
- **H11, the countdown is not fx-a-1's.** Checked: with the window refused, `split_ticket_open` returns on its "holds as
  today" path (hle.c:25957-25969) [read]. Changed: §4.1 step 2: the other player is held through the countdown.

### [maybe] points: what was found, what changed
- **G5, 1599 as player 2's when K is player 2.** Found: the after-script and cinematic-end routes of player 2's scripts run
  as their use (hle.c:19338-19355, 19518-19535) [read]; no run shows a player-2 Tome find since 11-11. Changed: P6's keep does
  not depend on the asker (the asker's room by identity), and run 2 logs which it was.
- **G6, Anthony's arrival scene and P5's cutscene gate.** Found: the pre-stay bring has no cutscene gate (hle.c:22511 is the
  fall rescue only) [read]. Changed: P5 has no cutscene gate.
- **G7, Max's two-room Tome scene.** Found as stated (no room 5, no dissolve; under the stay the reader goes alone). Not
  built; for the user (§11.2).
- **G8, the film's own ticket.** Found: fn66's two writer calls are the only fn66 lrs (0x801668EC, 0x80166934) [read grep].
  Changed: P6 keeps only fn66's tickets by room 5's ways-out scripts; any other lr is printed.
- **G9, row +28 preloads RT's file.** No change: B's fresh load may read from disc; that is time, not a fault [inferred].
- **G10, player 2's dissolve lasts two frames.** Found: its premise (player 1's copy is 0) is wrong by H2: the door sets
  player 1's copy [inferred from F4]. The strength by the main player's distance stays open (§7, §11.11).
- **G11 / H3, game code turns the effect on too.** Closed by P2's r3 == 5 test.
- **G12, slot 0 is scratch between the write and the read.** Changed: slot 0 is lent at `notify_ticket_run` of that change
  (the runner reads it at 0x80152688 in the same call) and put back at its end.
- **G13, O's own door in flight.** Found: T's fn66 is passed over by `coop_script_go_to_room` (hle.c:17421-17445) [read], and
  state 2 of a door of player 2's resumes from the timers through `coop_use_start` with p2 (hle.c:21482-21491) [read], so its
  ticket is theirs. Changed: P2b (the dissolve put off, WARNING); P6 refuses a door script's ticket out of room 5.
- **G14, saves.** No format change; P10 and the fallback. A host state taken in room 5 sends those in it to the game's spot
  (§8). The game's own saving in room 5 was not found (`ed how save`: rooms 93 and 274) [read there].
- **G15, the mansion Tome.** Outside this design; §11.4.
- **G16, the open items the game answers.** r13-26280 is 0 at B's change unless 0x800A2548's ticket was the last; entry 0
  with flags 1 matters only to a sanity effect that uses it [read there: refute-game §16]. Taken; removed from §11.
- **H5, the one passthrough flag.** Found: the dispatcher clears `g_hle_passthrough` before each intercept and reads it after
  (hle.c:5641-5648) [read]. Changed: the host's tickets are written from `notify_behaviour_pass`, a notify, whose guest calls
  do not decide whether its own function runs (5652-5666) [read]; the keep only voids.
- **H13, B's change and T's music.** Found: the stay's stream drop keeps the other player's room's streams and notes each
  start with the room in play (hle.c:14627-14636, 14734-14752) [read]. Changed: §7 names the check and the fix to try.
- **H14, T's camera follow-ups when T is player 2.** Found: 868 is run by the runner at 0x801527F4 and its `run_next` inside
  the same 0x801642D4 call (0x80164470-0x80164484) [read], so inside the R4 bracket; 531, 1061 and 1161 have no waits [read].
  So they set player 2's set [inferred]. Changed: A = player 1 whenever player 1 goes back to RT, so 1349 (with waits) is
  always player 1's.
- **H15, the room left behind after the bring.** Found: nobody is in it until the film's end kills its record (G1).
  Changed: P5 sends it 61 and stops its direct streams. Effects, fog and lights of a resident room nobody is in were not
  read (§11).
- **H16, player 1 in RO when player 2's trip takes them.** Found: a prompted door of player 2's waits for player 1's look or
  find (hle.c:21965) [read]; words and close-ups are closed by `coop_room_change_words` (`g_coop_room_p2` false) [read
  11328-11358]. Changed: P2b; the rest is §11.13-15.
- **H17, player 2's camera hash.** Found: their set, with r13-26864, is in `g_coop_cameras.p2` outside their turn
  (hle.c:16207-16225, 16258) [read]. Changed: P3 reads it from there at the change; no per-frame noting.
- **H18, the player-2 wrap is not a door of theirs.** Found: `coop_be_player2` leaves r13-23276 player 1's (hle.c:12426-12447)
  [read there]. Changed: P7 and P8 wrap with `coop_stay_room_in` too, as a use of theirs does (hle.c:18874) [read].
