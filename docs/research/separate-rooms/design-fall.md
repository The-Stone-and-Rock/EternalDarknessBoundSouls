# Design (final): a player stood, placed or frozen on the wrong room's floor while apart (the fall in room 182)

The bug (the user, 2026-11-12, separate rooms on). In Pius's chapter, player 1 went from the statue room 182 into 181.
Player 2, still in 182 and about to follow, fell through the floor and could not move after. The log is lost. Scripted
attempts did not reproduce it.

This is the final design after two refutations (refute-fall-impl.md, refute-fall-goal.md). Section 9 lists each point
and what I found when I checked it against the code. It builds on fall-ground-readers.md, fall-host-readers.md and
fall-rooms-readers.md. Nothing was edited, built or launched; this file is the only one written.

Marks: [read] = seen at the address, hle.c line (working tree, 28,968 lines; numbers drift) or log given. [inferred] =
reasoned, not seen.

## 1. In short

- **The cause is not found.** No path in the default build (Tome copy mode 2, no ED_COOP_P2_STEP) is shown to reach any
  of the three shapes below in the user's layout. This design is **hardening plus a probe**, not "the class closed".
- **Three shapes fit "fell through the floor and could not move":**
  - **C1, the wrong room's floor.** The game's collision getter 0x80123F70(object) gives "the player" (r13-26524's
    object) the CURRENT slot's record, whatever the object's +592 says. Every other object gets its own room's record
    [read 0x80123F94-0x80123FA0, 0x80123FA4-0x80123FFC]. So a stayer made "the player" while the other room is current
    is stepped, dropped and zone-scanned against the other room. In 182's north half, 181 has no floor (y > 2023) or one
    78-109 lower [read: fall-rooms-readers.md 2-3].
    - A body that goes under its own floor either falls for ever (sweeps 172914, 173348: to -3405 and -7597) or comes
      to rest on geometry below and can still walk there (160254: rest at z -704.8 from frame 2532, moved on the stick
      at 3006-3107) [read both.tsv].
  - **C2, a dead record.** The stayer's slot record not live: the getter answers 0, the step stores its FROM, the body
    neither moves nor falls [read 0x80133488-0x8013349C], and their half draws no room [read: fall-ground-readers.md 5].
    They stand frozen in a void. This fits "could not move" at least as well as C1.
  - **C3, the room in play's bodies.** The body test in the stayer's own step takes the bodies of the room in play
    (r13-23276) [read 0x801308E0-0x80130950], at the stayer's coordinates. It is argued not to press a body down in the
    user's direction; the reverse is open.
- **What gets built (one build, all co-op-apart only):**
  - **F1** (section 3): the getter answers a player's own resident room, live, when that player is "the player" while
    the other room is current. It fixes the **floor** in any such window. It does not make such windows safe: the zone
    scan in the same step uses the same answer (section 3.3).
  - **G** (section 4): a through-the-floor guard for either player while apart. It detects a body that went down through
    a surface of its own room, using the game's own collision against their own room, and puts them back on that surface
    with the game's arrival placer.
  - **D** (section 5): the dead-record detector and repair. It puts the stayer's record back if it stays dead for 30
    frames, and lets coop_follow bring them if that cannot be done.
  - **The probe** (section 6): `[floor]` lines printed without a switch, rate-limited, and the play log kept across
    sessions. The user's next separate-rooms play names the window if it happens again.
- **The checks** (section 7) first reproduce each shape on purpose with test switches, both players, as a hard gate. Only
  then are the fix-on runs read. Then come play layouts for every kind of frame in 182/181, the roles exchanged, one
  other room pair, together and solo.

## 2. The causes, ranked

### C1: the stayer is "the player" while the other room's slot is current

- **What it needs.** `r13-26524 == the stayer's object` while table 0x8058AFC0 +0 is the other room's slot, during the
  step 0x80133434. Every mover reaches that step: the behaviour pass 0x801FA494, root motion 0x801241B4, the push list
  0x8004582C [read: the three `bl 0x80133434` in disasm.txt]. Gravity 0x8013332C runs inside the step (0x801334E4) and
  asks the getter itself (0x80133384) [read].
- **How much of it is needed.** One wrong step drops a grounded body about 1 unit, and the next own-room step catches it
  [fall-ground-readers.md 2.4]. A fall needs many wrong steps in a row (the sweeps were stepped against the wrong room
  every frame), a direct write under the floor, or a large push-out [inferred from 160254 and the sweeps].
- **The windows known to reach it:**
  - `ED_COOP_P2_STEP=1`: player 2's every step with who swapped and no room in, from their first turn on
    (hle.c:25118-25121 gates on `g_coop_p2_state`) [read]. Off unless set; nothing the user runs sets it [read: play.bat,
    play-rooms-log.bat].
  - The old Tome routes (`ED_COOP_TOME_COPY=0/1`): coop_tome_player(true) held across up to COOP_START_WAIT pad reads
    [read: fall-host-readers.md 3]. Mode 2, the default, does not take them.
  - One-call windows that hold coop_be_player2 or coop_tome_player round a game call. The equip set and the hazard reach
    no mover, six direct-call levels deep [read: a call-graph walk; table calls not followed].
  - H1b (the R4 bracket outliving its pass) and H2 (an unpack into the stayer's slot) [read: no apart run has more
    "unpack asked" lines than changes].
  - Event 120's handover (0x80059B1C) makes another object the player [read]; a story switch, not in 182/181
    [inferred].
- **For player 1 as "the player" with player 2's room current:** r6_hit_room, coop_events (hle.c:21905) and
  coop_grab_events (hle.c:22197) [read]. They are one-call brackets that run player 2's work; none is a step of player
  1's [inferred].
- **Verdict.** It gives the right outcome. No default-build window is shown to give it many steps in a row.

### C2: the stayer's record dead (frozen, their room not drawn)

- **The repair today.** The put-back runs at every pass through the change's tail: notify_room_slots_tail on
  0x80154B54 at lr 0x80152434 (hle.c:14387-14389, install 28075-28077) [read].
  - The Tome's give-back (0x80110E88) and the reload (0x801558A8) call the same runner 0x80152298, whose tail is
    0x80152430 `bl 0x80154B54` [read: disasm.txt]. So those re-runs get the put-back too. The goal refuter's "nothing
    rescues it" is true only for a mark made outside that tail [inferred: no other writer was searched for].
- **What keeps a freeze for good.** coop_stay_apart keys on `g_room_slot_room` (room_slot_of, hle.c:13861-13874). That
  word is learned only from a loaded record (room_slots_frame, hle.c:13665-13668) and is cleared only by an unpack ask
  (hle.c:13640) [read]. So a record dead with no unpack keeps "apart" true. coop_follow then returns at its apart branch
  (hle.c:21413-21421) and never brings them [read].
- **Verdict.** It fits the words "could not move" well. It has no known trigger in the recordings: a scan of every page
  log with a `[stay] player 2 stays` line found only door-runner and jump-to-game changes [read: refute-fall-goal.md P4].

### C3: the room in play's bodies in the stayer's own step

- Player 2's step collides with the bodies of the room in play at player 2's coordinates (node room == r13-23276,
  object +596 bit 0x80, node flag 0x4, shape 0x80130504), and with the room-50 floor constant (0x80133CC0) [read].
- **The user's direction** (player 2 in 182 at z 78-109, 181 in play at z 0): 181's bodies lie below the stayer. A push
  is unlikely to point down [inferred].
- **The reverse** (player 2 in 181 at z 0, 182 in play): player 1 on the dais at z 109 and the 182 creature at
  (-767, 1212, 111) lie over 181's teleport region (-760, 1064) r 219, where player 2 walks. Those bodies are above the
  stayer. 0x80130504's shape is not read, so a downward push is not ruled out [inferred].
- **Verdict.** A real artifact across rooms (invisible walls). As a fall it is open until the probe in 7.8 has run.

### C4: the Tome copy's return (play-r5c's lift)

- Player 2, idle and apart, rose 1,228 units after the copy closed, then landed on 341's own floor [read:
  logs/run-20261111-191306-play-r5c 6890-6930]. Low for a fall-through; an open oddity.

### Ruled out [read]

- Standing or walking at z 0 and z 78 in 182 while player 1 changes, and walking through their own ticket at z 78
  (logs/run-20261112-033642, -033738).
- The change frame: the put-back runs before the behaviour pass (lr 0x80152434).
- fn71 at the change only touches the ARAM cache (0x80155600-0x8015568C).
- The doors that skip the +592 stamp (section 3.1, gate 4) are in L00, L06, L09, L11 and L16, not in Pius's level L01
  [read: refute-fall-impl.md 1, build/ed-db/scripts].

## 3. F1: a player's floor is their own room's (intercept on 0x80123F70; 0x8012AB2C on the USA build)

### 3.1 The gates, cheapest first

1. A static byte `f1_on`, set at install only when co-op is on and both room switches are set. It is checked first.
   (Since 5eb040d the switches are the run's own decision: rooms_follow_coop unsets ED_ROOM_SLOTS and ED_COOP_STAY
   whenever co-op is off, before the game starts [read frontend.cpp:478-503].) Then `g_settings.mod_coop`, `sda_ready`,
   and `!g_tome_copy_turn`.
   - F1 also goes in tome_copy_hook_off's kOff list (hle.c:27068-27075), so "[tome-copy] stood aside" names it.
2. `object == r13-26524`. Otherwise pass through: the game's non-player path already answers by +592.
3. The object is a player's.
   - Player 2: `g_coop_p2 + 52`.
   - Player 1: in the turn, `g_coop_turn_p1`'s object. Otherwise `g_coop_p1_node`'s object, which coop_be_player2 sets
     at every swap (hle.c:12342) [read]. Only while that is 0 is r13-21492's node used.
   - Any other object (an event-120 character) passes through.
4. **The two players' +592 differ.** This is the guard against the stamp the engine skips:
   - The load stamps r13-26524's object +592 only if r13-26280 == 0 and the ticket's +24 (0x8058B014) is nonzero
     [read 0x80154D00-0x80154D30]. Nine fn66 calls pass 0 as the 7th argument; 0x800A2834 sets r13-26280 := 1 [read:
     refute-fall-impl.md 1].
   - On such a door, player 1's +592 stays the room they left. If player 2 stays there, both name it, and this gate
     passes through: the game's player path then gives the current slot, which is right for player 1.
   - r6_apart is not used here: it is false inside the R4 bracket (g_stay_bracket) [read hle.c:24339], which would drop
     the H1b cover.
5. `cur = record of table +0`, `oth = record of table +4` (0 when +4 == -1), read in C as 0x80155130 reads them [read
   0x80155130-0x8015519C]. Reading them directly does not reach the lights hook on 0x80155130.
   - If cur's +0x8140 == object+592: pass through. Their room is current, and the game's answer is theirs, even a 0 for
     a dead record.
   - Else, if oth's +0x8140 == object+592 and +0x8142 and +0x8143 are set (the test at 0x80123FB8-0x80123FE0): oth is
     the candidate.
   - Anything else passes through.
6. Not inside the room change. F1 has its own walk, not coop_camera_at_room_change's:
   - lr in 0x80152298..0x80152814, or a back-chain return address in it;
   - the walk stops when a saved lr is GUEST_CALL_SENTINEL 0x0BADF010 (hle_guest_call writes no back chain:
     main.c:80-81) [read], when a link does not go up the stack, or after 24 links.
   - It is evaluated only when 1-5 hold, which is rare.

Then F1 answers `oth`. That is 0x801551A0(3)'s answer, the game's own non-player path for n = 3 [read
0x80123FE4-0x80123FEC]. With `ED_COOP_OWN_FLOOR=0` it passes through and logs "would give".

**The getter is pure.** It calls only 0x801551A0 and 0x80155130 and writes nothing outside its frame [read: refute-fall-impl.md].
So answering in its place (r3, hle_return) drops nothing.

### 3.2 Install

- Do it beside R6c and the lights (hle.c ~28075-28095), inside `room_slots() == 2 && coop_stay()`.
- Check the first word 0x9421FFE0 and `lwz r0,-26524(r13)` at +0x24 (as find_player does, hle.c:6606-6611), and that the
  dispatch entry is free.
- Otherwise print `[coop] own floor: 0x80123F70 not as expected: not installed`.

### 3.3 What F1 changes besides the floor (the zone scan)

- The step's first getter answer (lr 0x8013346C, kept in r28) feeds both the swept collision test (0x801334BC into
  0x80133A44) and the zone scan `0x80134A44(obj, info+180, info+176)` (0x801335B4-0x801335C4) [read]. For the player the
  scan always runs (0x801335A8-0x801335B0) [read].
- So in a C1 window with F1 on, the stayer is scanned **as the player against their own room's zones while the other
  room's word and table are current**. Without F1, they are scanned against the other room's zones at foreign
  coordinates. Both are wrong.
  - With F1 on, a door of their own room could start go_to_room with r13-23276 = the other room. The gate at
    0x80152334-0x80152360 then takes it as "the same room" [read by the refuter; outcome inferred].
  - The stayer's record holds player 1's stale in-zone state from when player 1 walked out. The put-back writes only
    +0x8140..+0x8143 (hle.c:14374-14383) [read], so LEAVE events can fire [inferred].
- **So F1 fixes the floor only.** The windows themselves must stay closed: no stayer as the player without their room in.
  ED_COOP_P2_STEP is not made safe by F1, and it stays a test switch.
- **Considered, not built: F1b**, skipping the player's zone scan in a step where F1 changed the answer. 0x80134A44's
  side effects are not read, so it is not built ("read the gates before building").

### 3.4 What F1 leaves

- **Host direct writes** that place a body by another room's floor or ignore rooms:
  - coop_follow's bring and rescue, coop_park (hle.c:7232-7247: beside player 1, rooms not checked);
  - ED_COOP_P2_PUT (hle.c:21357-21364);
  - sweep_put (ED_PUT, fixed today to camera_collide_object).
- **The readers of the current section that take no object:** the arrival placer 0x80130EF0 (runner only), and 0x80134FB8
  with its callers 0x80051DC8, 0x8007076C, 0x80135258 and 0x801ECEC8, which are not classified [read: disasm.txt]. Also
  the effects and nine script natives [read: enemy-readers.md d].
- C2 and C3.

### 3.5 Interactions

| Piece | With F1 |
|---|---|
| R1 two slots | Reads the table and the records only. Writes nothing. |
| R2 stayer, strips, put-back | F1 needs the stayer's record live. Dead, it passes through (0: frozen, as today), and D deals with it. |
| R4 player 2's change | Inside the runner (0x80152298..0x80152814 covers both runners, hle.c:17306-17307) F1 passes through. It acts only in frames outside the runner while the bracket is still open (H1b), and there it gives player 2 the room they are in. |
| R4b/R4c | No collision read. |
| R5 sound, R3 picture | None. |
| R6 r6_hit_room, coop_events (21905), coop_grab_events (22197) | Player 1 is the player with player 2's room current. A getter call for player 1's object there now gets player 1's room. The hit's line test passes the hit node's own object (0x800324C8-0x80032738), so hits do not change [read: refute-fall-goal.md]. Watch for these lrs in the probe. |
| R6b route job | No getter call site in 0x800B9-0x800BB [read: refute-fall-impl.md 6]. Protects player 1 only if a job's bracket leaks. |
| R6c creatures | The bracket's player is in the bracket's room: no-op. |
| Lights G1-G3 | Their hook on 0x80155130 acts at lr 0x8002798C only. F1 reads the records itself. No contact. |
| Arrival camera tags | The tag at coop_stay_change_end asks after be_player2(false): non-player path, unchanged. A host zone read with player 2 the player and no room in now gets their own list (fewer false "new zones"). |
| camera_collide, coop_place, coop_floor | For whoever is the player, the answer is now their own resident room in a window. Outside windows, unchanged. |
| ED_COOP_P2_STEP | The floor is right; the zone scan is not (3.3). Not "safe". |
| Tome copy | kOff: stands aside. |
| Solo, together | Not installed solo. Together, gate 4 or 5 passes through. |

## 4. G: through their own floor while apart, put back on it (either player)

The game has no fall recovery [read: fall-ground-readers.md 2.4]. The together rescue's rule (2000 below the last still
z) would not fire on the one recorded fall: each slide re-latches "still" under the floor (rows 2409-2413, 2445-2450,
2471-2477, 2498-2504: |dx| < 1, dy 0, dz -0.6/-0.7), and the body comes to rest 728.5 below its start [read: 160254
both.tsv]. G therefore does not use a still-z history. It asks the game's collision of the body's own room whether the
body went down through one of that room's surfaces.

1. **Where it runs.** At the pad read, beside coop_follow, outside player 2's turn (`!g_coop_turn`), for each player, only
   while r6_apart.
   - It returns in g_tome_copy_turn, cutscene_playing, g_stay_bracket, inside the change (F1's walk), and when the
     player's room record is not live.
   - For player 1 it also returns unless table +0's record is player 1's room. At the pad read it is, outside brackets
     [inferred].
   - For player 2 the non-player path already answers by their +592.
2. **What it keeps.** Per player, a ring of the last 32 pad-read places, each with its +592. The ring is emptied:
   - at start;
   - when that player's +592 changes;
   - when r6_apart turns false;
   - on a state load (the frame moving back, as sweep_put does at hle.c:15020-15025, and from hle_coop_state_load).
3. **The test.** Each pad read:
   - Take the highest ring place within 60 units in x,y of now, in the same room. If there is none, or it is less than
     40 above now: nothing.
   - Else cast camera_collide_object(cpu, object, (x, y, z_high + 40), (x, y, z + 20), 0, hit) at the body's current
     x,y. This is the game's world, segment and static test (hle.c:6293-6330) on the room the getter gives that object.
   - A hit means a surface of their own room lies over them at their own x,y, and less than 32 frames ago they were
     above it. They went through it.
   - G fires when the hit holds on two tests four frames apart. This skips a one-frame clip at a ledge's edge.
   - A drop off a ledge does not fire: at the current x,y past the edge, the ledge is not overhead [inferred]. A floor
     that is a dynamic collider (a scripted collapse) does not fire: the static test does not see it [read: the
     test is 0x8013085C's static path; inferred that collapses are dynamic].
4. **What it does.**
   - Bracket: for player 2, coop_stay_room_in and coop_be_player2, as the R4 arrival has them. For player 1, nothing:
     their room is current.
   - Call `0x80130EF0(object, (x, y, hit.z + 5), f1 = 10.0)`. f1 is the arrival's r2-22244 = 0x80622D7C = 10.0 [read:
     refute-fall-impl.md, dol]. The placer rays 10 units along gravity against the current section and places at the
     hit, else at the point [read 0x80130F84-0x80130FD4]. So the body is placed on the surface it went through.
   - Call `0x80118A6C(object)`: the fall speed reset (+40 := +36) [read 0x80118A6C-0x80118A74].
   - Close the bracket. Empty that player's ring.
5. **Log** (no switch): `[floor] player N went through room R's floor at x y (z, was above at z_high): put back on it at
   z' by the game's arrival placer (frame F)`.
6. **Limits.**
   - Three fires in 300 frames for the same player: G stops for that player and logs `[floor] player N keeps going
     through room R's floor: G stopped (a cause still acts)`. That keeps it out of a loop.
   - Off switch: `ED_COOP_FALL_RESCUE=0`.
7. **What G does not catch.** A direct write under the floor at a new x,y, which is 160254's shape (frame 2401: dx -134.3,
   dy -683.1, dz -72.7 in one frame). The ring has no place above the floor there. Such writes are fixed at their
   source; each host placement already casts for a floor.
8. **Together unchanged.** coop_follow's same-room rescue is left as it is. It has the same blind spot (it re-latches on
   slides), and that is recorded as open, not changed.
   - Its `stood_z` is a plain static, so it starts at 0.0, not NAN (hle.c:21379) [read]. Before the first still frame,
     it fires only below z -2000.

## 5. D: the dead record, detected and repaired

- **Where.** room_slots_frame, once a frame (hle.c:13654). Only while coop_stay_apart. After R4 the room in play is
  player 1's again, so the slot that can die unseen is player 2's, the non-current one [read: the R4 comment at
  hle.c:13876-13881].
- **The detector.** s = room_slot_of(player 2's room). The record's word reads room -1, loaded 0, and +0x8144 == 0 (the
  shape the tail leaves, which room_slot_put_back tests at hle.c:14374-14380): count frames, else reset.
- **The repair.**
  - At 30 frames: `room_slot_put_back(cpu, s, "player 2's room dead 30 frames while they stay")`, under its own three
    gates. This is at the draw, not the pass. That is allowed here because a player is in the room (the comment at
    hle.c:13671-13672 forbids it only for a room nobody is in). The next frame's pass sees it live.
  - At 120 frames still not live (the word has another shape, so the put-back refused): log, and set
    `g_room_slot_room[s] = -1`. coop_stay_apart then turns false, and the next pad read's coop_follow brings them beside
    player 1.
  - A record that is dead before an unpack does not reach either step: the unpack ask clears g_room_slot_room first
    (hle.c:13640) [read]. This relies on the ask coming within 30 frames of the mark [inferred].
- **Log** (no switch): `[floor] player 2's room R has no live record (slot S, word 0x...) for 30 frames: frozen and not
  drawn; put back` / `...: forgotten, brought to player 1`.
- **Off switch:** `ED_COOP_DEAD_REPAIR=0` keeps the detector line only.

## 6. The probe (reaches a normal play)

- **Unconditional, rate-limited lines.** They are rare, so they need no ED_COOP_LOG. play.bat sets none [read
  play.bat:13-17].
  - F1: `[floor] player N asked the floor as the player with room X current; their room Y's given (lr, caller's lr,
    turn=, tome_p2=, bracket=, r6c=, frame)`, once per (player, lr) per 600 frames. A count goes in the 120-frame
    `[stay]` line.
  - The stale stamp: `[floor] player 1's +592 says room A while the room in play is C (a door that skipped the stamp)`.
    It is printed when the object is player 1's, the room word's slot is current, and +592 != the room word. Once per
    change.
  - G's and D's lines (sections 4 and 5).
- **Keep the logs.** play.bat keeps one previous log only (`move /y logs\play.log logs\play-prev.log`, line 16) [read].
  That is how the bug's log was lost: logs/play.log of Nov 12 03:19 is a later co-op-off session [fall-host-readers.md 6].
  - Change: rotate five (play-prev4 → play-prev5, ... play.log → play-prev.log).
  - Tell the user that play-rooms-log.bat records everything for a session they expect to test (run.sh: both.tsv,
    pictures) [read play-rooms-log.bat].

## 7. The checks, one build

Switches:
- `ED_COOP_OWN_FLOOR=0`: F1 logs "would give" only.
- `ED_COOP_FALL_RESCUE=0`: G off.
- `ED_COOP_DEAD_REPAIR=0`: D's repair off.
- `ED_FALL_HOLE="from,to,P[;...]"` (testing only). Inside coop_character_step (the step 0x80133434, which every mover and
  gravity go through), for frames in [from, to), while r6_apart, for player P's object:
  - **P = 2:** the existing ED_COOP_P2_STEP path, with the window in place of the switch. Who is swapped, the 2060
    prompt bytes at r13-24040 and coop_state are saved and put back by coop_words_back, and no room is put in
    (hle.c:25124-25156) [read].
  - **P = 1:** the same save and restore round a guest call of the step for player 1's object, with player 2's room put
    in by coop_room_in and taken out by coop_stay_room_out. Player 1 is already the player.
  - Either way the hole spans **every** step of the window, as C1's real shape and the sweeps that fell do. It is not
    one step per pad read.
  - Log once per window: `[floor] test hole: player P stepped with room R current, frames from..to`.
- `ED_FALL_DEAD=frame` (testing only). At that frame, while apart, write player 2's slot record word to the tail's dead
  shape (room -1, loaded 0, +0x8143 kept) and log it.

Rules: states made fresh on the build (Jump to Game, then doors or ED_PUT), game-window pictures only, `ed test` after
the change, nothing committed.

**Hard gate:** a fix-on run is read only after its fix-off run has shown the shape: p2z or p1z leaving its floor and
going on down, or the frozen body. If a fix-off run does not show it, stop and report. The hole is then not C1's shape,
and no fix-on result means anything.

### 7.1 State S182, both players together in 182 (fresh)

```
cd "/e/Eternal Darkness Recomp Fork"
export ED_ROOM_SLOTS=2 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_SHOW_TRACE=1 ED_RUN_SHOTS_MS=200
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:27,3210:27 \
  ED_SAVE_STATE=3650:saves/rooms-ch1-182b.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 fall-s182 </dev/null
export ED_COOP_STAY=1 ED_SETTINGS=$(ls -d logs/run-*-jtg-fall-s182 | tail -1)/settings.ini
```
Check: the last row's room is 182 for both.

### 7.2 H-2: player 2 stays in 182 where 181 has no floor (C1, player 2)

- Player 2 is put at EntryMkr02 (-769, 2229, 30) and grounds to 78.4. That is outside Region02 (y 2303..2525) and every
  CameraRgn (y max 2185), and 181 has no floor under it [read: fall-rooms-readers.md 2-3].
- Player 1 takes 124 at 3700 and is in 181 from about 3765.

```
for FIX in 0 1; do
  env ED_COOP_OWN_FLOOR=$FIX ED_COOP_FALL_RESCUE=0 ED_FALL_HOLE="4200,4260,2" ED_RUN_FRAMES=4600 ED_RUN_SECONDS=150 \
    ED_PUT="3650,2,-769,2229,30" ED_RUN_SCRIPT=3700:124 \
    sh tools/ed/run.sh saves/rooms-ch1-182b.state fall-h2-fix$FIX </dev/null
done
```

| | Fix off (must show the fall) | Fix on |
|---|---|---|
| both.tsv | p2room 182, p2z leaves 78.x at 4200 and goes on down | p2z 78.x every row |
| page log | `[floor] ... would give room 182's` with lr 0x8013346C / in 0x8013332C; the hole line | `... room 182's given`, same lrs; zone events from the scan are expected (3.3) and logged |
| player 1 | prompt and doors as before the window (the saved bytes) | the same |

### 7.3 H-1: player 1 the stayer (C1, player 1), two layouts

- **H-1a, the same table as H-2.** Player 1 is in 181 at its entry (-2147, 1061, 0). That is west of 182's whole
  footprint (x -1762..290), so 182 has no floor there [read: fall-ground-readers.md 2.6; inferred no floor]. Run
  `ED_FALL_HOLE="4200,4260,1"` with the rest as in 7.2.
- **H-1b, the roles exchanged by R4.** Player 2 goes 182 → 181 after player 1, then 181 → 182 by Region01 (126), so that
  player 1 stays in 181 through a change of player 2's. Two passes:
  - Pass 1 finds frame F2, where the log says `[stay] player 2's change to room 181 runs as theirs`, and checks that
    p2room is 181 at F2 + 200.
  - Pass 2 puts player 2 at (-2350, 1068, 0) at F2 + 200. That is 318 from Region01's centre (-2668, 1068), outside its
    r 261 [read: fall-rooms-readers.md 3]. Then a 60-frame walk -x on the pad 2 stick into Region01.
  - Precondition: `[stay] player 2's change to room 182 runs as theirs ... player 1 stays in room 181`, then `[stay]
    player 2 stays in room 182`.
  - Then hole `F3,F3+60,1`, where F3 is 200 frames after that line, and `F3+300,F3+360,2`. Both directions in one run.
  - Stick directions are taken from 033738's both.tsv: there p2sy -127 moved player 2 to lower y, 4.4 a frame.
- **Expected.** Fix off: p1z leaves 0.0 in the first window and goes down; p2z leaves 78.x in the second. Fix on: both
  hold, with `given` lines for both players.

### 7.4 G

- Repeat 7.2 and 7.3a with `ED_COOP_OWN_FLOOR=0` and G on.
- **Expected.** Within about 4-8 frames of the first drop under the floor: `[floor] player N went through room R's floor
  ... put back on it`. Then pNz is back at 78.x (or 0.0), and stick input afterwards moves pNx/pNy with pNz held.
- The hole is still open after the put, so a second fall follows. Three fires end in `G stopped`. That is the loop guard,
  and the line must appear.
- **A direct-write case, to record G's limit.** `ED_PUT="4200,2,-769,2229,-60"` (under 182's floor, as 160254) with G
  on: no G line expected (section 4.7). Record what happens; it is not a failure of G.

### 7.5 D (C2)

- `ED_FALL_DEAD=4200` in the 7.2 layout, without a hole.
  - `ED_COOP_DEAD_REPAIR=0`: p2x/p2y/p2z frozen under stick input, and no 182 in player 2's half (picture). This
    reproduces "could not move".
  - Repair on: the put back line at about 4230, player 2 walks and their half shows 182 again.
- **Play check P-T.** In the 7.2 layout with no switches: player 1 opens and closes the game's Tome in 181 (pad 1
  `start`, then B) while player 2 waits at 182's door end. Expected: no D line. If a D line appears, that is a real
  trigger: record it in coop-failed-attempts.md or the readers.

### 7.6 Play layouts, fix on, F1 log-only once, then on (the probe; every kind of frame in 182/181)

Pass for each: p2z never under 70 at y > 600 while p2room is 182, and the same for player 1 when they are the one in
182. No G or D line. In the log-only run, any `would give` line names a window: that is C1's missing piece.

- **D1, the user's layout.**
  - Both players are put in the north corridor: player 1 at (-770, 2000, 78), player 2 at (-770, 1880, 78).
  - Player 1 walks into Region02 on the stick (`ED_INPUT_SCRIPT`, +y). Player 2 follows about 100 behind, on the
    stick, through the 62-tick ticket, the change and 2033.
  - Player 2 then walks out of the sphere and back in.
- **D2, roles exchanged.** The same, with player 2 walking into Region02 first and player 1 about 100 behind at z 78
  (an R4 change of player 2's; player 1 stays in 182 with 182 in play).
- **D3, the second time.** D1 with flag 546 on (after a first pass and 126 back): then-script 1161, no scene.
- **D4, player 1's button scene.** After D1, player 1 walks into 181's SphereRegion01 (-1692, 402) and takes 290
  (prompt, cutscene, Camera03). Player 2 waits at 182's door end, then on the dais (-790, 1100, 109).
- **D5, the teleport with a stayer.** After D4, player 1 takes 127 (SphereRegion02, to 184): a second change with
  player 2 still in 182. Expected `[rooms]` lines: 184 into player 1's old slot (room_slots_return's swap), with 182
  kept.
- **D6, back into the stayer's room.** After D1, player 1 takes 126 back into 182: a return into the stayer's slot.
  Expected: "already in the other slot", together again, and G's ring emptied.
- **D7, player 2's own close-up apart.** After D1, player 2 walks into Region03 (-790, 1234) r 312: script 1298's words
  or examine (Camera13), on the dais above 181's teleport region.
- **D8, a fight in 182 apart.** After D1, player 2 swings (B) at the dais and walks the room. 182 has 36 SpawnMkrs [read:
  fall-rooms-readers.md 6]. If no creature comes, record that.
- **D9, the Tome copy apart.** After D1, player 2 opens the copy (`start`), equips (A), and closes it (B), as in
  play-r5c 3600-3680. Once at the door end and once on the dais. Watch for C4's lift.
- **D10, another room pair.** 341/343, where the tool fall happened (the cam2-lf-load-p2door route from
  build-run-arrival-2.sh with the ED_PUT fix), and D1's shape there.

### 7.7 Together and solo

- **Together.** The arrival-2 together route (101/105/111/29 with ED_COOP_STAY=1, both through every door), and 7.1's
  route to 182, then 124 with player 2 brought along. Expected:
  - no `[floor]` line;
  - `ed check` findings as on the build before;
  - coop_follow's rescue untouched.
- **Solo.**
  - jtg with co-op off: `[rooms] separate rooms off for this run: co-op is off`, no `[coop] own floor` line of any kind,
    no `[floor]` line.
  - The same 182 → 181 door played solo (ED_RUN_SCRIPT=3700:124 from a solo state): no `[floor]` line.
- **Lights and camera.** Re-run cam2-c3-p2door-p1follows and cam2-lf-load-p2door from build-run-arrival-2.sh: the same
  `[stay]` camera lines and no new ARRIVAL finding.
- **Last.** `ed test`.

### 7.8 C3 probe (log only, no fix)

- Apart, every 600 frames, list each node with object +596 bit 0x80 and node flag 0x4: id, room (0x801F662C), place.
- Run it in D1 (the user's direction) and in D2's mirror: player 2 in 181 walking through SphereRegion02 (-760, 1064)
  while player 1 stands on 182's dais at z 109.
- If a body of the room in play lies over the stayer and p2z drops there, build F2 (section 8). Until then F2 is not
  ruled out.

## 8. F2 (follow-up, not in this build): the room word for the stayer's own step

- **Build it only if 7.8 shows a need.**
- **The fix.** r13-23276 := their room round the stayer's own step in the pass.
- **What it needs.**
  - A restore at the step's return; the dispatch has no notify for that today [read: enemy-readers.md hazards].
  - The zone scan inside it would start a zone script with their word but the table still player 1's. So F2 must put
    the table in too, or keep the scan out.

## 9. Refuted or corrected (each point checked against the code)

### refute-fall-impl.md

| # | Point | Found | Change |
|---|---|---|---|
| 1 | F1 trusts a stale +592 | **Holds.** The stamp gate is at 0x80154D0C-0x80154D28 (r13-26280, 0x8058AFFC+24) [read]. coop_stay_apart compares player 2's room with the room word (hle.c:13869-13874) [read]. Also found: in that case the host's own views disagree. coop_stay_apart says apart, r6_apart (both +592) says together (24338-24342), and coop_follow takes its together branch by +592 (21380) [read]. | Gate 4 (+592s differ) and the stale-stamp probe line. The host's disagreement is listed in Open. |
| 2 | F1 changes the zone list | **Holds.** One r28 feeds both the collision and the scan (0x8013346C, 0x801334BC, 0x801335B4) [read]. | 3.3: F1 is floor-only. P2_STEP is not "safe". F1b considered, not built. |
| 3 | Holes skip the prompt save; P2_STEP from frame 0 | **Holds**, with a correction: P2_STEP starts at player 2's first turn (g_coop_p2_state), not at frame 0 (hle.c:25118-25121) [read]. | ED_FALL_HOLE now runs inside coop_character_step with its save and restore, in a frame window. 6.2(a) is dropped. |
| 4 | G's resets unstated | **Holds.** | Ring emptied at start, on a +592 change, on together, and on a load. Returns in the copy turn. Its place comes from the surface hit, not a stale place. |
| 5 | Stale back chain under guest calls | **Holds**: r1 := (r1-0x800)&~15, lr := 0x0BADF010, no back chain (main.c:80-81) [read]. | F1's own walk stops at the sentinel and at a link that does not go up. The shared coop_camera_at_room_change keeps its walk (Open). |
| 6 | coop_events / coop_grab_events left out | **Holds** (21905, 22197) [read]. | Listed in 2 and 3.5. |
| 7 | r6_player1_node can name a story character | **Holds** (24329-24335) [read]. | Gate 3 uses g_coop_turn_p1 / g_coop_p1_node, set at every swap (12342). |
| 8 | Gate order; kOff | **Holds** (kOff at 27068-27075) [read]. | Static byte first; F1 added to kOff. |
| 9 | 6.3's put inside Region01, timing | **Holds** (248 < 261). | (-2350, 1068), 318 from the centre. The put is timed from pass 1's log line. |

### refute-fall-goal.md

| # | Point | Found | Change |
|---|---|---|---|
| P1 | G never fires on 160254 and would put back under the floor | **Holds** [read both.tsv 2401-2536, 3006-3107]. "Falls for ever" was wrong for that run: it rests at -704.8 and still walks. | G rebuilt as a through-the-floor test on their own room's collision, placing on the surface hit (section 4). The together rescue's blind spot is recorded, not changed (together unchanged). |
| P2 | One wrong step per pad read may sink no one | **Holds** (fall-ground-readers.md 2.4). | The hole spans every step of its window, for both players; "fix off reproduces" is a hard gate. |
| P3 | No check reproduces the user's bug; the probe won't reach a play log | **Holds** (play.bat:13-17) [read]. | Called hardening plus probe. Unconditional `[floor]` lines, log rotation, what F1 leaves (3.4). |
| P4 | C2 has no check and no repair | **Holds in part.** The put-back does run on the Tome's and the reload's re-runs, which go through the same tail (0x80110E88 and 0x801558A8 → 0x80152298 → 0x80152430) [read]. A mark outside that tail is still unguarded, and the freeze is for good (g_room_slot_room never cleared) [read 13640, 13665]. | D (section 5), ED_FALL_DEAD, play check P-T. |
| P5 | Frames and directions not covered | **Holds.** | D2 and D4-D10. |
| P6 | C3 argued one way only | **Holds** (0x80130504 unread). | 7.8 runs the reverse layout. F2 stays open. |
| P7 | The R4 row contradicts the window | **Holds** (hle.c:17306-17307) [read]. | 3.5's R4 row corrected. |
| P8 | Install by getenv carries F1 solo | **Refuted in substance.** Since 5eb040d, rooms_follow_coop unsets both switches whenever co-op is off, before the game starts. The env is the run's decision (frontend.cpp:478-503) [read]. | Kept as a belt: the static byte also needs g_settings.mod_coop. The solo check adds a played 182 → 181 door. |
| P9 | 6.3 setup | Same as impl 9. | As above. |

### My own corrections to the first design

- coop_follow's `stood_z` starts at 0.0, not NAN (a static float, hle.c:21379) [read].
- 0x80130EF0's f1 is 10.0 (0x80622D7C), closed by the refuter [read there].
- "F1 closes the whole class at its one door" is withdrawn: it covers the floor in windows only (3.3, 3.4).

## 10. Open

- **The cause.** Which window, dead record or push dropped the user's player 2 is still unknown. The probe in a normal
  play names it.
- **The stale stamp in co-op apart** (a no-stamp door taken by player 1 while player 2 stays). coop_stay_apart,
  r6_apart and coop_follow disagree there, and coop_follow's together branch would compare positions across two rooms'
  coordinates. None of those doors is in L01. A host fix (stamping the mover with the game's 0x80118CB4 at the tail when
  the game skipped it, apart only) needs the reason the game skips it read first.
- **coop_camera_at_room_change's walk** reads stale chains under guest calls. It is shared by coop_events,
  coop_grab_events and others, so it is not changed here.
- **The zone scan in a C1 window** (F1b): 0x80134A44's side effects are unread.
- **C3:** which nodes carry flag 0x4, and 0x80130504's shape.
- **C4:** the lift after the copy.
- **The no-object current-section readers:** 0x80134FB8's four callers.
- **The together rescue's slide blind spot** (coop_follow).
- **D's 30-frame wait** relies on the unpack ask following a game-made mark quickly; that is not measured.
