# Refuter: design-fall.md (F1 own floor, G apart rescue, the checks) against the current implementation (2026-11-12)

Lens: conflict with what is built: hooks on the same addresses, double runs, passthrough, save/load, room-change edges,
failed attempts. Read only: runtime/host/hle.c (working tree, 28,968 lines), runtime/host/main.c, build/ed-db/disasm.txt,
generated/main.dol, `ed dis/fn/script`, build/ed-db/scripts, design-fall.md and its three readers, doors-readers.md,
stay-readers.md, docs/coop-failed-attempts.md. Nothing edited, built or launched. [read] = seen at the address, file:line or
log given; [inferred] = reasoned from those.

Verdict: **needs changes.** F1's hook site, its gates' reads and its install check match the code. But F1 trusts the
player's +592, and the engine leaves that word stale on some doors (1). F1 also changes which zone list the player's step
scans, not only the floor (2). The test hole skips the prompt-word save that coop_character_step has (3). G needs resets
it doesn't state (4). The rest are smaller.

---------------------------------------------------------------------------------------------------------------
## What holds (checked, no conflict)

- **The hook site is free, and the check words are right.**
  - No intercept or notify sits on 0x80123F70. hle.c names it only in find_player (6606, 6619), which reads its code,
    and in the guest-call defines COOP_PLACE_INFO (6911) and COOP_CHARACTER_INFO (12005) [read: grep].
  - The first word is 0x9421FFE0. +0x24 (0x80123F94) is 0x800D9864, `lwz r0,-26524(r13)` [read: dol bytes]. find_player
    checks the same load at +0x18+12 (hle.c:6606-6611) [read].
- **The getter is pure.** 0x80123F70 calls only 0x801551A0 and 0x80155130 and stores nothing outside its frame
  [read 0x80123F70-0x8012402C]. So answering in place of it (set r3, then hle_return) drops no side effect.
  - 0x80155130(2) is the record at `0x8058AFC0 + 12 + 4*table[+0]`. (3) gives 0 when +4 == -1. 0x801551A0 adds the
    +0x8142/+0x8143 nonzero test [read 0x80155130-0x801551E8].
- **No clash with the lights hook.** The lights hook on 0x80155130 (lights_record_for_actor, hle.c ~24398) acts only at
  lr 0x8002798C with r3 == 2. The getter's own call has lr 0x80123FB0 [read]. If F1 reads the table in C, as the design
  says, neither hook sees the other.
- **Host calls reach F1.** hle_guest_call runs `fn` through dolrecomp_call (main.c:61-100). r6_hit_room already relies
  on its own guest call re-entering its intercept and passing through ("The nested dispatch passed itself through",
  hle.c ~24740) [read]. So camera_collide_object (6309) and the seven COOP_CHARACTER_INFO / COOP_PLACE_INFO calls
  (6938, 13960, 16661, 18059, 18631, 20920, 26118, 26896) get F1's answer [inferred from the dispatch path].
- **The R4 runner is inside gate 5.** ROOM_CHANGE_FROM/TO is 0x80152298..0x80152814, which covers 0x80152614 to its end
  (hle.c ~17306-17320) [read]. So inside the R4 runner F1 passes through. After the runner, until the pass closes the
  bracket, the design's "F1 gives their old room" holds [inferred].
- **The stamp comes before the case logic.** In the load 0x80154C94, the mover's +592 stamp (0x80154D2C-0x80154D30)
  runs before the swap and unpack cases (0x80154D64+) [read]. So inside a normal change the mover's +592 already names
  the new room, and gate 5 is a safety net rather than a need. This case does not hold for the doors in problem 1.
- **0x80130EF0's f1 is 10.0.** r2-22244 = 0x80622D7C holds the float 10.0 [read: dol]. 0x801526C8 loads it for the
  arrival [read]. Design section 7's open item is closed: G rays 10 units along gravity, and on a miss it places at the
  point itself (0x80130FC0-0x80130FD4) [read].
- **The fall-speed reset is what the design says.** 0x80118A6C is `+40 := +36` [read 0x80118A6C-0x80118A74].

---------------------------------------------------------------------------------------------------------------
## Problems

### 1. MAJOR: F1 trusts "the player's" +592, which the game never reads for the player and leaves stale on some doors

- **The engine's stamp gate.** The load stamps r13-26524's object +592 only when both of these hold [read
  0x80154D00-0x80154D30]:
  - `r13-26280 == 0`;
  - the ticket's +24 (0x8058B014) is nonzero.
- **Where the gate closes.**
  - go_to_room's optional 7th argument, default 1 (`ed fn fn66`), is 0 in nine calls, which skip the stamp
    [read: build/ed-db/scripts]. They are L00/1564 (rooms 80, 86), L06/1838 (313), L09 and L16/1183
    (ServantAttackRm), and L11/1521 and 1805 (219). doors-readers.md:108 already records this.
  - 0x800A2834 sets r13-26280 := 1 before a change, and it is cleared at 0x801525B8, 0x801527FC and 0x801558D8 [read].
- **The failure.** Apart, player 2 stays in A and player 1 takes one of those doors to C. Player 1's +592 stays A.
  - coop_stay_apart compares player 2's room with the ROOM WORD (C), not with player 1's +592 (hle.c:13869-13874)
    [read]. So it says apart, and notify_room_slots_tail puts A's record back, live (hle.c ~14380-14388) [read].
  - F1's gates then all pass: the object is r13-26524 and player 1's; cur (C) != +592 (A); oth holds A and is live.
  - F1 answers A's record. Player 1 is stepped, dropped and zone-scanned on room A's floor at C's coordinates
    [inferred]. That is the very fall F1 is meant to stop, made by F1, for player 1.
  - Before F1, the player path gave C, which was right [read 0x80123F94-0x80123FA0].
- **The root of it.** The design says "acts only when a player's room is resident and not current, which means apart"
  (3.1). That is not true when the player's +592 is stale.
- **Fix.** Add a gate: the two players' +592 differ.
  - With the stale stamp both name A, so F1 passes through.
  - Don't use r6_apart for this: it is false inside the R4 bracket (g_stay_bracket), and that would drop the H1b cover.
  - Also log any frame where the player's +592 differs from the room word while the room word's slot is current
    (`[floor] the player's room word and +592 differ`). The probe then shows the stale case instead of being fooled by it.

### 2. MAJOR: F1 also changes the zone list the step scans for "the player", so in any C1 window the stayer's own room's zone scripts run as the player with the other room in play

- **What the step scans.** The step scans `0x80134A44(obj, info+180, info+176, ...)` with `info` = the getter's answer.
  For "the player" the scan always runs (0x801335A8-0x801335C4) [read]. The scan runs zone scripts: doors (go_to_room),
  camera regions, words.
- **What F1 changes.** With F1, a stayer who is "the player" in a C1 window is scanned against their OWN room's zones,
  as the player. The room word and table are still the other room's (no room in).
  - Region02 in 182 (script 124 → go_to_room 181) could then start with r13-23276 = 181: "same room", r13-26508 := 1
    (0x80152334-0x80152360) [read the gate; inferred the outcome].
  - A camera region's script would set the game's camera while player 1 is in 181 [inferred].
- **The stayer's room's zone state is player 1's, and stale.** The player's in-zone state in 182's record is what
  player 1 left when they walked out through Region02. The put-back restores the record as it was (room_slot_put_back
  writes only +0x8140..+0x8143, hle.c ~14374) [read]. The first scan of player 2 as the player can therefore fire
  LEAVE events for zones player 1 stood in [inferred].
- **Where the design goes too far.**
  - "coop_character_step (ED_COOP_P2_STEP) becomes safe apart" (section 5) does not follow. Its own comment says that
    with it "neither player can open one yet ... upset by the second scan" (hle.c ~24945-24948) [read]. F1 leaves that
    scan in place and points it at another list.
  - "F1 closes every C1 window" (3.2) is true for the floor only.
- **Fix.**
  - Say that F1 closes the floor, and keep the windows themselves out: no stayer as the player without their room in.
    Every host window either has room-in or is in the list in fall-host-readers.md 3.
  - Keep the "[floor] ... given" log as the finder.
  - Don't present ED_COOP_P2_STEP as a hole that F1 makes safe.

### 3. MAJOR (test validity): the check holes run the player's step without the save that coop_character_step makes, and 6.2(a) runs P2_STEP from frame 0

- **What ED_FALL_HOLE doesn't save.** It guest-calls 0x80133434 with player 2 as the player. The step's scan then
  writes the prompt and its state words (r13-24040 .. -21980), which are player 1's.
  - coop_character_step saves and restores exactly these (coop_bytes of 2060 bytes at r13-24040, coop_state, and
    coop_words_back), because "finding nothing would clear them (player 1 could not open a door)" (hle.c ~24960-24975)
    [read].
  - The hole does none of this. In 6.2(b) and 6.3's second hole, player 1's prompt is wiped for 20 frames, and zone
    enter and leave events fire as described in problem 2 [inferred].
  - Fix: copy coop_character_step's save and restore round the hole's call.
- **6.2(a) doesn't wait for the layout.** ED_COOP_P2_STEP=1 is not gated on time. It steps player 2 as the player from
  the first frame, together in 182 too (hle.c ~24950: only `off`, g_coop_step_running, g_coop_p2_state and the object)
  [read]. With it, doors are known to break (same comment).
  - So the run may not reach the apart layout it means to test, and a failure there says nothing about F1.
  - Fix: drop (a), or limit P2_STEP to a frame window for the test.
- **The hole's call stack.** It is the step's frames on top of a hle_guest_call frame, which makes gate 5's walk read
  stale memory (problem 5).

### 4. MAJOR (G): G's "last still place" needs explicit resets, or an arrival or a load fires G and puts the player at the old room's coordinates

- **What the design says.** G keeps a per-player last still place and fires at `z < last - 2000` (4.1). It never says
  when that place is cleared.
- **What coop_follow already does.** Its single `stood_z` is NAN until set, and is cleared to NAN whenever the rooms
  differ (hle.c:21374, 21383, 21419) [read]. So it never compares across a room change.
- **G's cases.** G is per player, and the player who moves keeps a value from the room they left:
  - **A change while apart.** The mover arrives in a room of its own coordinates. Floors differ by room: 182 stands at
    78-109, 181 at 0 (fall-rooms-readers.md 1-3). If the old room's z was 2000 or more above the new floor, G fires on
    the first apart frame before a still frame. It then places them, inside their NEW room's bracket, at the OLD room's
    coordinates [inferred].
  - **Apart, then together, then apart again.** Stale values carry over.
  - **A state load mid-run.** sweep_put sees the frame move back and starts its puts over (hle.c ~15024-15030), and
    lights has lights_state_load. G needs the same [read: those patterns].
  - **A start value of 0 rather than NAN.** It fires at once for any floor below -2000.
- **Fix.** Make it unset (NAN) at start. Reset it when that player's +592 changes, when r6_apart turns false, and on a
  state load (frame moving back). Set it only from a still frame in the current room.
  - Also return in g_tome_copy_turn: the copy runs on `cpu->ram = copy`, and the pad read's helpers there would touch
    the copy.

### 5. MINOR: gate 5's stack walk follows a stale back chain under host guest calls

- **How a guest call starts.** hle_guest_call sets `r1 := (r1 - 0x800) & ~15` and `lr := 0x0BADF010` and writes no
  back chain (main.c:78-81) [read].
- **What the walk reads.** For a call from camera_collide_object, COOP_CHARACTER_INFO or the test hole,
  coop_camera_at_room_change (hle.c ~17309) follows `*(r1)` into old memory below the stack, for up to 24 links.
  - An old return address into 0x80152298..0x80152814 left there by an earlier change reads as "in the change". F1
    then passes through for that call without a word [inferred].
  - The opposite mistake can't happen (the walk only ever adds a pass-through).
- **Fix.** Stop the walk at the sentinel frame: when lr or a back link is 0x0BADF010, or the link leaves the range
  between r1 and the stack's top. Or use a host flag set at notify_room_change and cleared after the runner.

### 6. MINOR: the interactions table leaves out brackets where player 1 is "the player" with player 2's room current

- **What the design says.** r6_hit_room is "the only behaviour change inside an existing bracket" (section 5).
- **Two more brackets.** coop_events (hle.c:21905) and coop_grab_events (22197) put player 2's room in while apart and
  leave r13-26524 as player 1's [read]. A getter call there with player 1's object now gets player 1's room.
- None of the 17 getter call sites is in the route-job code (0x800B9-0x800BB) [read: disasm], so R6b is unaffected.
- **Fix.** List them in the table, and look out for their lrs in the probe. The change is probably harmless [inferred].

### 7. MINOR: gate 3's "player 1's object" by r6_player1_node can name a story character

- **The helper.** r6_player1_node returns r13-21492 whenever it is not player 2's node (hle.c ~24329) [read].
- **The switch.** Event 120's handover makes another object the player (0x80059B1C) [read in design].
- **Fix.** Compare with g_coop_p1_node's object, or with g_coop_turn_p1's in the turn, rather than with whatever node
  is current.

### 8. MINOR: gate order and how the Tome copy is excluded

- **Gate order.** Gate 1 reads r13-26524 before gate 2's sda_ready. Put a static "installed and co-op on" byte first;
  it costs less than a memory read.
- **The Tome copy.** F1 checks `!g_tome_copy_turn` itself, but the project's pattern for co-op hooks in the copy's turn
  is the kOff list in tome_copy_hook_off (hle.c ~27060-27072). Add F1 there too, so that "[tome-copy] stood aside"
  lists it.

### 9. MINOR (test plan 6.3): the put lands inside the door sphere, and the timing is tight

- **The put is inside Region01.** (-2420, 1068) is 248 from 181's Region01 centre (-2668, 1068), and the radius is 261
  [read: fall-rooms-readers.md 3]. So the put is inside the door's sphere, and the door back can fire at the put
  (4500), not on the walk.
- **The put may land in 182.** It needs player 2 already in 181 at 4500.
  - The door waits for 2033 (about 230 frames after about 3765).
  - Then come the walk (4050-4400) and the 62-tick ticket.
  - If player 2 isn't through by 4500, the put runs in 182, where x -2420 has no floor, and sweep_put says "not put"
    (hle.c ~15046) [read the branch; inferred the timing].
- **Fix.** Put them outside the sphere, at about (-2350, 1068). Time the put from the `[stay]` or "player 2 ... room 181"
  line, not from a fixed frame.

---------------------------------------------------------------------------------------------------------------
## Not refuted (outside this lens or held)

- C3, 181's bodies and the room-50 constant keyed on r13-23276, stays every frame in the normal pass, not only in
  windows. F1 doesn't reach it. G would catch a resulting fall of more than 2000. The design says so.
- Every save state carries F1's answers, because F1 keeps no state. Only G and the detector keep state (problem 4).
- The failed attempts have no entry about the getter or ED_COOP_P2_STEP. Lines 468, 527 and 576 (the look line's
  +612/+628, ED_PUT's 50-units-low floor before the fix, room 50 falls) don't contradict the design.
