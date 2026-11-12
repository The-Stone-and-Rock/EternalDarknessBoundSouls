# R6 readers, round 2 (2026-11-11 night): the open questions of enemy-readers.md

Four readers on the game's code and the host (HEAD 2886af7), read only. Facts carry EVIDENCE; [inf] marks inferences.

######## read:enemy-attack
READER REPORT: how room 183's zombies attack, and who takes the hit. Read only: no file changed, nothing run or built. hle.c means runtime/host/hle.c. Facts have evidence; [inf] marks an inference.

## (1) The enemies of room 183 and their behaviour
- **What is placed.** Room 183 has four chr 24 / model 8 things with spawn40 = 4: placed_things level 1, section 0, index 5-8, at (-128,833,3), (880,789,4), (1564,96,178) and (364,-863,165). Room 180 has one, at (-65,502,0). Evidence: build/ed-db/facts/placed_things.json. `ed room 183` lists no NPCs.
- **spawn40 is the node type.** It is the spawn record's +40, the node type (build/ed-research/arguments.md:296; the field is read at tools/ed/facts/checks/args/things_build.py:16).
- **Behaviour by node type.** The table built at 0x80030C28 (each entry is `0x801F5A9C(fn, type, r13-30728)`) gives:
  - type 4 = 0x8003B470 (0x80030C80-0x80030C90). The zombies run this one.
  - type 2 = 0x80058DD8 (the player)
  - type 18 = 0x80060370 (the grabber)
  - type 19 = 0x80060EC8 (the one held)
  - type 37 = 0x8003E468
- **Ids in the R6 base state** (logs/run-20261111-1944*):
  - Player 1 is id 1, node 0x81127F3C.
  - Player 2 is id **59**, node 0x81128CD4 ('[coop] player 2 0x81128CD4' and 'damage node 81128CD4 id 59').
  - The zombies are ids 40, 41, 42 and 48.
  - Ids differ per save (133 and 143 in older logs), so R6 must read node+16 and never hard-code an id.

## (2) The hit: the sender and its 'to'
- **The swing does not send 39.** It sends event 11, from the zombie to the victim's own id.
- **Attack start, 0x80039840.** It reads r29 = stats+20 (0x800398AC) and r28 = its own id (0x800398BC). It then makes a block:
  - kind via 0x8019B6FC
  - +156 = its own id (0x8019B73C at 0x80039C4C)
  - **+152 = stats+20, the quarry** (0x8019B744 at 0x80039C58)
  - +124 = 1 (0x80039C78), unless the effect test 0x8006A448(0x50000) answers yes
  - +168 = 0x100A84 (0x80039C88)
  - the move-key callbacks 0x800383B8 and 0x80039E40 (0x80121C08 at 0x80039DA8/DC0)
- **The hit test chain.** Key callback 0x80039E40 calls 0x80038860(node, block) (0x80039EAC). That function takes from = +156 and victim = 0x801F5F88(+152), and dispatches on the +168 bits; bit 0x4 calls 0x80036094 (0x800388E4).
- **First test, the quarry: no room test.** 0x80036094 tests the +152 victim by distance < reach and facing only (0x800365F4-0x8003665C), then calls 0x80035C58.
- **Second test, a sweep of the room.** If +124 bit 0 is set, it walks every node (0x800366F0-0x80036838). A candidate must be in the attacker's room by +592 (0x80036758-0x8003676C) and answer event 59 with 1 (0x80036780). It must also be within reach and facing. The candidate's id is then written to +152 (0x800367EC) and 0x80035C58 is called.
- **The send.** 0x80035C58 sends event 11 with from = 0x801F62C8(attacker) (r15, 0x80035C7C) and to = 0x801F62C8(victim node) (r23, 0x80035CFC) at 0x80036058, 0x80036000, 0x80035F78 and 0x80035D54. The sibling tests 0x80035740 (0x8003589C) and 0x80035974 (0x80035B34) do the same. Both test the attacker's room against the victim's (0x80035798, 0x800359D4).
- **'to' is never 0x801F62B8 and never re-addressed.** It is the quarry stats+20 (first test) or the swept candidate's own id.
- **The logs agree.**
  - 'event 11 from 40 to 1 lr 80036060 (80036660 800388E8 80039EB0)' is the quarry path.
  - '(80036800 ...)' is the sweep path.
  - 'event 11 from 41 to 40' is a zombie sweeping another zombie (run r6-base-c2).
- **The victim's side.**
  - The player's event-11 case (0x80059C44-0x80059C68) calls 0x8006255C(block).
  - That function reads to = +152 (0x80062584) and from = +156 (0x800625FC).
  - **NEW GATE: the victim's room against r13-23276** (0x80062594-0x800625B8). A mismatch, with the room not -1, branches to 0x80063798: no 53 and no 39.
  - Otherwise it sends 53 (0x8006367C) and **39 from +156 to +152** (0x80063754-0x80063764).
  - The 39 then reaches 0x8005A604, then 0x80061B9C (the first readers' gate 0x80061BD0), then 0x80061E90.
  - 0x8006255C has 39 bl sites (ed fn), so every victim kind has these two gates.
- **The 39 senders that belong to this enemy:**
  - **0x80063764**: inside 0x8006255C, run by the victim. This is the melee's damage.
  - **0x8003A9F4**: in 0x8003A8AC, which is the type-4 behaviour's case for **event 203** (0x8003B888-0x8003B8A0). It is not the attack. In state 10 it sends a sanity hit of 50 (0x8019B7D4(2) at 0x8003A9D4; amount 50 at 0x8003A974) to **0x801F62B8 = r13-21496** (0x8003A9A8). Event 203 comes from 0x8006255C at 0x80063564, attacker to victim, so it fires when something strikes the zombie [inf: a reaction to being struck in state 10].
  - **0x8003B9C8**: the type-4 case for event 58, forwarded as 39 to the 58's to (0x801F539C).
  - **0x800606B0**: the same forward in the grabber.
  - **0x80060B80, 0x80060BC0, 0x80060BF8**: the grabber's bites, in (3).
  - None of the other 'li r3,39' sites is in the type-4, type-18 or type-19 code.

## (3) The grab
- **Yes, this behaviour has one.** The attack chooser 0x80039468 returns kind 6 from the creature table: 0x8006F3D8(info+144) +44/+52/+60 shares against rand%100 (0x80039644-0x80039684).
- **Kind 6 in 0x80039840.**
  - Distance 100-123 (0x80039998-0x800399B0) and facing.
  - Move 88/94 (0x80039A7C).
  - **Event 32 from the zombie to stats+20** (0x80039A88-0x80039A98).
  - info+140 +20 := that id (0x80039AC0).
  - The zombie's own node type := 18 (0x801F64A8 is `stw 12`; 0x80039AD4-0x80039ADC).
- **The victim's side.** The player's event 32 (0x80059CD8) calls 0x8005FF34: move 35, then the node type := 19 (0x80060050-0x80060054). There is no room read in 0x8005FF34-0x80060090.
- **The bites, in the grabber's event-3 tick** (0x80060AB4-0x80060C04):
  - every 8th frame when the one held is player kind (+158 == 1)
  - event 39 from the zombie to **info+140 +20, the id taken at the grab** (0x80060B40, 0x80060B70)
  - health 1, plus magick (+148 == 2) or sanity (+148 == 3)
  - no room test and no 0x801F62B8
- **The held one's taker.** Type 19's case 39 (0x80061888, 0x80061B50) goes to 0x80060188, which has no room gate and does not call 0x8006255C or 0x80061B9C. **So a grab of player 2 apart would land.**
- **Other hits on the one held stay gated.** A second zombie's event 11 to them goes through 0x8006255C (0x8006129C).
- **Seen in logs:** grab bites appear only in ch11 ('event 39 from 95 to 1 lr 80060B84', 100 lines, run-20261106-071357-c11tome). Every hit by a ch1 zombie in the logs is the 11 → 0x8006255C chain (lr 80036060). Whether chr 24's table ever picks kind 6 is open.

## (4) How a same-room hit reaches player 2 today
- **Nothing re-addresses it.** coop_send_event (hle.c:20944-21001) changes 'to' only for event 204 (20948-20956). It answers 0 to event 59/239 between the two players (20975-20985) and to a held player's 59 (20990-21000). No other `gpr[5] =` in hle.c touches a hit.
- **The path:**
  1. The quarry (or coop_enemy_target, 24114) puts player 2's id in stats+20.
  2. The block's +152 holds that id, and event 11 goes to player 2's own id.
  3. Player 2's node runs 0x80058DD8 in coop_events (21614-21682: their state words through coop_events_words 21506-21524 and hle_guest_call). The marker '0BADF010' in the chain is that guest call [inf].
  4. 0x8006255C's gate passes, then 39 goes to player 2's id, then 0x80061B9C runs on player 2's node.
- **Log proof** (run-20261111-194313-jtg-r6-base-state, frame 2674): 'event 11 from 48 to 59 lr 80036060', then '53/39 from 48 to 59 lr 80063768 (80059C6C 0BADF010 ...)', then 'damage node 81128CD4 id 59 amount 7 now 75'. ch1 runs c1z6 and c1z7 show the same with zombie 42.
- **The grep given in the task is broken.** `[^\n]` in ERE excludes the letters `\` and `n`, so it cuts every line at 'eve'. Fixed with `grep -rh '\[hurt\]' | tr -d '\r' | sort | uniq -c`. Totals: about 78k lines are player swings (lr 800C3D08); zombie lines are the chain above; grab bites are ch11 only.

## (5) Apart: player 2 in room B (183), player 1 in room A (180, in play)
- **Proof:** run-20261111-194435-r6-base-c1-p1away. both.tsv from frame 3897 shows room 180, p1room 180, p2room 183. Zombie 40 sends **'event 11 from 40 to 59 lr 80036060 (80036660 ...)' 15 times (frames 4129-5051)**: the quarry path, to player 2's id. **No 53, 39 or damage line follows, and p2hp stays 68.** The hit is dropped in player 2's 0x8006255C at 0x800625A4 [inf: the only exit before 0x8006367C/0x80063764 on this path; the node is found and the room is 183, not 180].
- **If that gate passed:** 39 from 40 to 59, then 0x80061B9C runs on **player 2's node** and stops at its second gate (0x80061BD0) for the same reason.
- **Verdict: R6 does not re-address anything.** It only brackets the room word. Set r13-23276 := the victim's +592 around the victim's 0x8006255C. Doing it around player 2's event 11 inside coop_events, or as an intercept on 0x8006255C, also covers the nested 39 → 0x80061B9C [inf: the 39 is delivered immediately, inside 0x8006255C; both log lines carry the same frame].
- **Condition for the bracket:** the victim's room is resident (room_slot_of) **and** equals the attacker's +592 (block +156 → 0x801F5F88 → 0x801F662C).

## Hazards
- **Two victim-room gates per hit.** 0x800625A4 (event 11) and 0x80061BD0 (event 39); the death gate 0x8006383C makes three. The first readers named only 0x80061B9C.
- **The quarry test has no room test.** 0x80036094's first test (0x800365F4-0x8003665C) and 0x80035C58 do not compare rooms. A stale cross-room quarry in reach by coordinates gets an 11. With player 1 as that victim in room A (in play), today's gate passes, so a zombie of B could hurt player 1 [inf: needs overlapping coordinates between quarry runs]. That is why the bracket must require attacker room == victim room.
- **The first test skips event 59.** So a held player (coop_player_held refuses 59) who is the zombie's quarry is still hit. Only the sweep asks 59.
- **Event 203's sanity hit goes to r13-21496, not to the striker** (0x8003A9A8). If player 2 strikes a zombie in state 10 outside a who[] swap, player 1 loses 50 sanity [inf]. This one would need re-addressing to the 203's from.
- **The extras inside 0x8006255C key on 0x801F62B8.** These are 0x800627A8 (attacker == the player) and 0x80062998 (victim == the player → 0x8006EDE0). Like 0x80061E90's extras, they treat player 2 as 'not the player' unless who[] is swapped.
- **Grab bites skip every gate** (0x80060188), and coop_enemy_target cannot retarget apart (hle.c:24072-24073). So a grab started on player 2 apart lands in full. The grab start's own room test is only the quarry's.
- **The 0x8006255C bracket also tags the room for its effects.** 0x80119C34 at 0x80062AE8 and the sends of 86/130/230/50/203 get room B: right for a hit in B [inf].

## Open
- Does chr 24's creature table (0x8006F3D8(info+144) +44/+52/+60) ever give kind 6 in ch1? Read the record, or run with ED_GRAB_LOG=1 in room 183.
- Confirm that the 39 is delivered nested inside 0x8006255C (an hurt_chain depth >3, or a notify on 0x8005A604 while inside 0x8006255C), so one bracket covers both gates.
- Is the claim in docs/coop-next-session.md:34 ('same-room hits while apart DO land') wrong? Every recorded p2hp drop in R2/R3 is at room 183 with player 1 back. R3 t1-stay both.tsv frame 4870 shows p1room 183, p2room 183. No both.tsv has player 2 losing health while apart, and the c1 run shows the hits dropped. The claim looks wrong [inf].

Files: runtime/host/hle.c (20944-21001, 21506-21524, 21614-21682, 21892, 24061-24120); build/ed-db/disasm.txt; build/ed-db/facts/placed_things.json; build/ed-research/arguments.md:296; logs/run-20261111-194435-r6-base-c1-p1away/page-0001.log; logs/run-20261111-194313-jtg-r6-base-state/page-0001.log; logs/run-20261111-195135-r6-base2-c2-p2away/both.tsv.

######## read:enemy-life
R6 READER: which room-in-play tests (r13-23276) break an enemy of the OTHER resident room. Read only, HEAD 2886af7. hle.c = runtime/host/hle.c. Facts have evidence. [inf] marks an inference.

## 0. A correction to enemy-readers.md: event 8 is DEATH, not "wake"
- 0x80038E54 (event 8's handler; called from 0x8003B8BC (state 0), 0x8003CC90 (state 33) and 0x800A30D8) is the death. The normal path plays move 24 (0x80122920(obj,24,32,10) at 0x80039150) and sets health to 0 (0x800354F8(node,0,0,1) at 0x8003917C). It then posts event 17 for later (0x801F58AC at 0x800391BC; 120 frames in rooms 409/251, otherwise 1000: -31472(r2)=120.0, -31468(r2)=1000.0 read from generated/main.dol) and sets state 8. The other path plays move 43, sets health to 1 (0x8003907C) and type 28 (0x8003911C).
- Senders of event 8: the meter setter 0x80061E90 at 0x800621B8 when health reaches 0 or less, and the hit taker 0x80061B9C at 0x80061C90. Both send it synchronously, from the attacker, inside the hit.
- What the readers called "destroyed" means this. 0x801DCB58(2,node) queues op 2, which disables the behaviour record (arguments.md:269). 0x801F64A8(node,0) with 0x801F6490(node,1) is a pending TYPE change to 0. The flush (0x801F51DC-0x801F525C) sets node+8 := 0, and type 0 has no handler (0x801F52D4-DC: beq, no call). So the node is never removed, but it becomes inert for good: no death move, it never gets another event. Nothing turns it back [inf].
- How messages are delivered. 0x801F59B0 delivers synchronously, and so does 0x801F5620's room broadcast: its delay -22248(r2) is 0.0, read from the DOL. A zero delay is not queued (0x801F50A4). 0x801F57C0 and 0x801F58AC with a delay queue the message, and the pump 0x801F53E8 delivers it from the main loop at 0x800084D4. That is in the frame, after notify_behaviour_pass has ended any R4 bracket (hle.c:13848-13869), so the room word is player 1's.
- How the pending changes are flushed. Every delivery (0x801F5000) flushes them: event 2 to the old state, then event 1 to the new state (0x801F514C-0x801F51C4). A type change sends event 1 by 0x801F59B0 (0x801F525C). So "event 1 in state N" is the entry into state N. The room test at 0x8003B580 is only in state 0's event 1.

## (1) When events 1 and 8 reach an enemy, and whether they fire while player 2 is apart
- Event 1 (state 0) is sent once, when the node is created: 0x801F5D18(node) = 0x801F59B0(1,id,id) has 49 bl sites, each right after the room stamp 0x801F64C8. It is also sent on a type change into this type (0x801F525C).
  - The level's placings spawn at level start: 0x80020328 -> 0x8004222C at 0x80020424 -> 0x800418F4/0x80041AA4/0x80041AFC/0x80041ED4. Each one ends 0x801F64C8(rec+6), then 0x801F5D18 (e.g. 0x80041C44-0x80041C54), then 0x801DCB58(1). Then 62 goes out to r13-23276 (0x800422E0). The room word then is the level's start room [inf: both players are together then].
  - Runtime spawns: ed3 0x8000E4A4 (0x8000E724), sanity effects 0x8006DAC0, summons 0x801C3A88/0x801C6CC8, the enemy kinds' own spawns 0x8008C5A0/0x80099918/0x800A14A8/0x800A2408, and others.
  - The room test at 0x8003B580 runs only when info+4's byte 9 has bit 0x2 (0x8003B570-7C). If the room is in play the enemy sends itself event 8 (it dies at once); if not, event 57 makes it inert (0x8003B60C-0x8003B690). Bit 0x2 also forbids the move-43 path in the death (0x80038FA0-B8). So it is a "placed dead" kind of flag [inf]. Without bit 0x2 the enemy goes to state 33 (bit 0x8) or state 1, with no room test (0x8003B5BC-0x8003B600).
  - Apart: a script of player 2's room that spawns such a thing is fine inside the host's brackets (turn 20613, use 17837, timers 18433). Started from the frame (the game's own script tick, or the zone scan of player 2's object), it would get 57 [inf].
- Event 8 (death) fires when the enemy is killed by a hit (synchronous, inside 0x80061B9C/0x80061E90). Also: event 11's room-39 rule (0x8003BAA4), a corpse spawn (0x800A14EC after 0x800A14A8), spells (0x801D01BC/0x801D43B4/0x801D453C), and EDSpecialRegionCheck 0x80009790. That last one is a script native: it sends 8 to characters inside a region of the current slot's zones whose object +592 == r13-23276 (0x800098D0-0x80009904).
  - Not sent by a door, by arrival, by a timer or by coming near. "Waking" is the quarry, by +592 (enemy-readers).
  - Apart, today: player 2's hit never lands (0x80061B9C's gate), so no 8.
  - Hazard: if R6 lifts only that gate's compare, the death runs with player 1's word, 0x80038ED4 fails, and the zombie is made inert standing, with no fall. A room-word bracket over the whole of 0x80061B9C keeps 8, 11 and 11->30 inside it: 0x80061F9C, 0x800621B8 and 0x800636F4 are all 0x801F59B0.
- Player 2's door alone (R4 bracket). notify_ticket_run (hle.c:13836) sets r13-23276 := their old room; 0x80154C94 then writes the new room. Inside the bracket the broadcasts are synchronous: 62 to the new room's nodes (0x80152774-80, room r13-23276 = B) and 61 to the old room's (0x801520E8-F4, room r13-23288 = their old room). No event 1 or 8 is sent at arrival, so arrival is safe for 0x8003B580/0x80038ED4. Event 62's helpers see B: 0x800648BC, 0x80066044, 0x800BAF78, 0x800C7CDC, 0x801C5674 (call sites 0x8003B73C-0x8003B844).
- HAZARD, new: event 61 to the room being left reaches its enemies even when the other player stays there. That happens for player 1 leaving player 2's room too: 0x80152614 is player 1's own change.
  - In state 0 the enemy drops its route and is unregistered (0x800BB0C0, 0x8003A2E8, 0x80064A7C, 0x800E8464 at 0x8003B850-84).
  - In state 33, when 0x8003A1D0(id) == 4, 61 sends 57: inert (0x8003CB54-0x8003CB7C).

## (2) The 29 lwz -23276(r13) sites in 0x80030000-0x8003FFFF (grep '^8003....: lwz r., -23276(r13)'): what each does when the enemy's room != the room in play
1. 0x80030140, the spawner behaviour 0x8002FC94 (record info+124, its room at +28), state 11, event 3: WAIT, goes to state 38. The same test, just below the range in the same function: 0x8002FD68 (state 38 resumes to state 1 only when equal), 0x8002FDD0/0x8002FE0C (state 1), 0x8002FE8C (state 12), 0x8002FFD4 (state 10). Its spawn also needs 0x8002F5D0: the point off-screen, or unseen by a ray through the CURRENT slot's section (0x801551A0(2) at 0x8002F654). So it uses player 1's camera and room [inf on the camera].
2. 0x80030454, 0x800302DC (6 callers in the hit effects, 0x80062D68 and others): sound tag only (0x8019F104 r10).
3. 0x80030BA4, 0x80030B68 = ed203 count_in_zone (script native, called from 0x80009A1C): SKIP, others' room not counted. Used by L01/2116.
4. 0x80033020, 0x80032E8C (the player's code, from 0x800C0394/0x800C3900): SKIP, a downed candidate (r30 8/31) of another room is ignored [inf: the finisher/stomp pick]. Player-side.
5. 0x800332EC, 0x800330E0 (the finisher look): SKIP. Already in enemy-readers.
6. 0x80038DF4, 0x80038DB4 (from the chase tick 0x8003AFF8/0x8003B0C0 and 0x8003EE14): a room-96 constant (returns 2). Constant only.
7. 0x80038ED4, the death 0x80038E54: DESTROY (inert, no death move).
8. 0x80038F7C, same function: a room-208 constant (kind 1 there gets no move-43 path).
9. 0x800390C4, same function: sound 75 tag.
10. 0x80039180, same function: room 409/251 constant (the event-17 delay).
11. 0x8003932C, 0x8003928C (state 0 event 9, 0x8003B8D8): room-251 constant delay (event 24 posted by 0x801F58AC).
12. 0x8003A4F0, 0x8003A464 (state 0 event 30, 0x8003BC08): SKIP the whole event. It only sets flag 0x4, then returns 0. Lost: the four 0x8014D3D0 effects, 0x800391EC, 0x80063B50 and 0x80039F3C. Event 30 comes from event 11's 0x8006255C (0x800636F4) [inf: dismemberment].
13. 0x8003A558, same function: sound tag.
14. 0x8003B2F8, the sight 0x8003B1C8: sound tag.
15. 0x8003B580, state 0 event 1 with bit 0x2 set: 57, DESTROY.
16. 0x8003B700, state 0 event 201: sound 497 tag.
17. 0x8003B9F0, state 0 event 11: room-39 constant (event 246).
18. 0x8003BA74, state 0 event 11: room-39 constant (sends itself 8 when +148 == 2).
19. 0x8003C0C4, state 21 event 3 (TICK): room-183 constant. In room 183 the speed passed to 0x800BC4F0 is 0.02 (-31340(r2)), otherwise 0x80038D44(node). With player 1 in 180, room 183's zombies in state 21 move at the other speed (shown in the test room) [inf: what state 21 is].
20. 0x8003C380, state 21 event 2 (exit): room-83 constant.
21. 0x8003C8F4, state 47 event 3: sound tag.
22. 0x8003CBE4, state 33 event 8: DESTROY (the same as 7).
23. 0x8003D95C, state 9 (down) event 24: the room argument of 0x800C90E8(node,100,0,room,1). That function counts characters within 100 units in THAT room (0x800C91FC-0x800C9214), so it counts room A's characters at room B's coordinates.
24. 0x8003D97C, same: WAIT. It never gets up (0x8003D9B0-0x8003DA1C: move 41, health 10, state 33). It posts 24 again after 300 frames (0x801F58AC at 0x8003DA54; -31320(r2)=300.0), delivered by the pump with player 1's room word, so it waits for as long as the players are apart.
25. 0x8003E51C, the sibling behaviour 0x8003E468 (reached by table), state 0 event 1: the start move 79 is skipped. State 1 anyway.
26. 0x8003E5B4, 0x8003E468 event 201: sound tag.
27. 0x8003E71C, 0x8003E468 event 24: room argument to 0x800C90E8 (reach 50).
28. 0x8003E74C, same: WAIT. Rising (move 41, byte 9 |= 0x8, type 4) is skipped, and 24 is posted again after 60 frames (0x801F57C0 at 0x8003E850; -31308(r2)=60.0).
29. 0x8003E800, same: r13-23276 passed to 0x80007364, which is a bare blr. Nothing.

Summary: DESTROY at 7, 15 and 22. WAIT at 1 (and the 5 sites near it), 24 and 28. SKIP at 3, 4, 5 and 12. Room-number constants at 6, 8, 10, 11, 17-20. Sound tag only at 2, 9, 13, 14, 16, 21 and 26. The two count arguments at 23 and 27. 25 loses only a motion; 29 does nothing.
- Sound tags: a track's +132 = r13-23276 (sound-readers.md:125). So B's enemy sounds are tagged A, and R5's per-room gain/fade (hle.c 14027-14163) treats them as player 1's [inf].

## (3) Do the per-frame enemy ticks test the room in play?
- The ticks do not destroy or stop an enemy. Event 3 is sent per frame by the holder step 0x800276C4 -> 0x801F5D48 (0x80027784; skipped while busy 0x80155738 / held 0x801F68D4).
- Room-word reads on event 3:
  - The sight's sound tag (14).
  - State 21's room-183 constant (19) and state 47's sound tag (21).
  - The chase tick 0x8003AD74 (state 3, 0x8003C5A4) -> 0x80038DB4, the room-96 constant (6).
  - The attack start 0x80039840 -> 0x8019B418 -> 0x8019B5F0 stamps hit block +198 := r13-23276 (0x8019B6AC). It is read only by a printf: 0x8019B2B0 -> 0x80200C0C.
- State 1's 0x80039718 and 0x8003A738 read none directly. The scan: scratchpad reach.py, 3 calls deep. Its paths through 0x800BB728 and 0x8002FC70 are false hits, because both are functions of 2 and a few instructions.
- On the state-3 tick, 0x800BBCC8 needs the CURRENT slot's record to be loaded (0x80155130(2), +0x8143/+0x8142 at 0x800BBCF0-0x800BBD18). The route stamp at route +808 := r13-23276 (0x800BAF20) has no reader (grep ', 808(' in 0x800B: the store only).
- The AI point graph is the things copy (r13-25060). It is filled only for the room in play, by the table-reached 0x800BB730 (0x800BB914) and at the change. So other-room routing (target 550 units or more away) uses room A's points [inf, open].
- Not on the tick but per frame: the step's character filter 0x801308F8 (enemy-readers).

## (4) What R6 should do
- Intercept the one handler call, 0x801F5294(node, state, msg, answer): every delivery reaches it, synchronous, queued and broadcast, and the flush's events 2/1 go through it again. It is the bctrl at 0x801F52F4 to table 0x806068A8[type*16]. Install check: 0x9421FFF0 at +0, 0x8005000C at +0xC, 0x4E800421 at +0x60.
- Rule: R = coop_object_room(cpu, r3) (hle.c:13799). If coop_stay(), g_stay_bracket == 0, no change of player 1's is running, R != -1, R != r13-23276, and room_slot_of(R) >= 0 with its record loaded (+0x8142/+0x8143, as 0x80123F70 tests):
  - swap r13-23276 := R and the slot table +0/+4 to R's slot, as coop_stay_room_in does (14238-14256, but for R rather than player 2's room);
  - hle_guest_call the function (the coop_as_player2 pattern, 21368-21385);
  - put the words back.
  The nested call passes through because R == the word by then. Nested deliveries to nodes of A switch back to A by the same rule. This covers the tick (3), death (8), 11->30, 24, 17, 1, 57, 61/62 and the pump's delayed messages.
- With N=2, a node of a resident room that is not in play is in player 2's room. So also swap who[] (coop_be_player2's words, hle.c:12291) for those nodes, except player 2's own node, which coop_events handles (21614). The reason: B's attack senders aim at 0x801F62B8() (e.g. 0x8003A9A8) and would hit player 1; the sight's "is it the player" check is the same (0x8003B260). With who[] = player 2 in the bracket, coop_sighting's swap is not needed for B's enemies.
- 0x80061B9C then passes for any victim in a resident room, so add the hits-reader's check that the attacker's room equals the victim's room.
- Cost: one guest call per delivery per node of B: about one event-3 tick per frame per enemy, plus events [inf: tens of µs each].
- The step's collision filter (0x8013085C) needs its own bracket on the node's step 0x801FA44C. The dispatcher does not cover it.

## Hazards
- Lifting a gate without the word turns deaths into inert standing zombies (7, 22).
- Event 61 at either player's door strips the staying player's room's enemies (state 33: removed).
- The spawner and down/rise polls of B wait forever while apart (1, 24, 28). The spawner's visibility test uses player 1's camera and section.
- The rise count 0x800C90E8 counts the room argument's characters, and "main" is r13-21484 (0x800C9114, 0x800C9260).
- The things copy (point graph) is A's only.
- No flag exists today for "a change of player 1's is running" (when the slot table is mid-change) [inf].

## Open
- The meaning of info+4 byte 9 bit 0x2: the only setter found is 0x8006644C in 0x800661EC. Settle with ed dis 0x800661EC 200, or a probe at 0x8003B578.
- Why 0x8003A1D0(id) == 4 in state 33's event 61 (the 36-byte table 0x8030AB10).
- Whether B's enemies route on A's points: ed dis 0x800BB730 / 0x800BC1EC.
- The host recursion depth of hle_guest_call in nested deliveries.
- Which script spawns of player 2's room run unbracketed.

Scratch scripts (not project files): C:\Users\stevi\AppData\Local\Temp\claude\9282620f7b4851ea\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\reach.py, C:\Users\stevi\AppData\Local\Temp\claude\9282620f7b4851ea\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\lab.py, C:\Users\stevi\AppData\Local\Temp\claude\9282620f7b4851ea\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\ctx.txt

######## read:player-hits
READER REPORT: how a player's attack lands, and where the room in play stops it for player 2 in another room (B). I only read code and logs: no file changed, no build, no run. "frame" means outside the host's brackets, where r13-23276 = player 1's room A. "turn" means coop_player_turn.

## Corrections to enemy-readers.md
- **C1. 0x800C3ACC is not the melee hit.** It is the player's every-frame tick: event 3 → 0x80058ED0 bl 0x800C3E1C → 0x800C3E4C bl 0x800C3ACC.
  - It updates meter 3 (0x80034E20/0x80034F7C/0x800354F8(node,3,..)) and sends event 39 every frame to (info+140)->+16 with data = &float ratio (0x800C3CE8-0x800C3D04). The data is not a hit block.
  - Logs (logs/run-20261106-0*hurt*/*.log): 38629 lines "event 39 from 1 to 5 ... lr 800C3D08 (800C3E50 80058ED4 801F52F8)", and thousands more "from 59/143/133/89 to 0".
  - So the question "where is (r31+140)->+16 filled for player 2" does not matter for hits.
- **C2. 0x8005BA70 and 0x8005BCA4 are not player attacks.** Their only callers are two enemy behaviours, by event:
  - kind 5, 0x8005E4E0 (registered at 0x80030C58-0x80030C68 via 0x801F5A9C): event 223 → 0x8005BA70 (0x8005E740-58); events 241/245 → 0x8005BCA4 (0x8005E6F0-0x8005E734).
  - kind 50, 0x800A2B94: 223 at 0x800A34B0-C8; 241 at 0x800A3488-A4.
  - Event 223 is posted to the attacker itself, with data = victim id, by kind 5's area sweep 0x8005BFCC (0x8005C6E4-F8, delayed send 0x801F57C0).
    - The sweep walks every node, keeps a candidate only if its own room == r13-23276 (0x8005C180-88), then event 59 (0x8005C1EC), distance and ray.
    - 0x8005BA70 then sends 39 from itself to the victim (0x8005BBC4-0x8005BC84).
  - 241/245 only make an effect plus sound 649 tagged with r13-23276 (0x8005BE74).
  - [inf] Kinds 5 and 50 are aligned creatures (actor+148: 1/2/4) with a blast attack. Which creatures they are is open.
- **C3. Event 201 goes to the attacker, not the victim.** 0x80061E90 sends it from the victim (r27 = 0x801F62C8(node)) to the attacker (r28 = block+156 via 0x8019B734): 0x80061F08-1C, 0x800620AC-C0. It is sent only when the victim is r13-21496 (0x80061F20-30).

## Facts: the hit block and the sends
- **Block layout:** +152 = victim id (set 0x8019B744, read 0x8019B72C), +156 = attacker id (set 0x8019B73C, read 0x8019B734), +198 = r13-23276 stamped at allocation (0x8019B6AC-B0). +198 is read only by the debug dump 0x8019B230 (0x8019B2B0).
- **Message layout:** +0 event, +4 from, +8 to, +28 data (0x801F5384/0x801F53AC).
- **0x801F59B0 is a synchronous send.** It goes to 0x801F5000, which delivers at once unless the message time is in the future (0x801F509C-0x801F510C).
- **0x801F57C0 is a delayed post** (time = now + f1, 0x801F57F8-0x801F5800). The queue is drained in the main loop at 0x800084D4 (bl 0x801F53E8), in the frame.

## (1) Which attack goes through what
- **One shared resolver for melee and guns: 0x80038860(object, block).**
  - It finds the attacker and victim nodes from the block (+156/+152; 0x80038878-0x800388AC).
  - It picks a hit test by block flags (0x8019B80C):
    - 0x4 / 0x2000 / 0x4000 → 0x80036094 (the swing)
    - 0x2 → 0x80035B5C → 0x80035974
    - 0x10 → 0x80036BC8
    - 0x8 → 0x80036D98
    - 0x10000 → 0x800371F4
    - anything else → 0x800358C4 → 0x80035740
  - It has 14 callers, among them the player key callbacks below and 0x801F6DEC.
- **Melee swing (state 6).** The entry 0x800C06BC:
  - allocates a block (0x800C08BC bl 0x8019B418);
  - copies the given block into it (0x8019B5C0: 208 bytes, so +152 comes along);
  - sets attacker := the node's id (0x800C08D8);
  - registers key callback 0x800BFB44 four times (0x800C0C34-0x800C0C88, lis 0x800C / -1212).
  - 0x800BFB44 calls 0x80038860 (0x800BFB88).
- **Gun, state 96.** Entry 0x800BD62C registers fire callback 0x800BCE44 (0x800BD964-78, 0x800BDB4C-60). It checks 0x800BCEA4 (ammo/fire test), then 0x80038860 (0x800BCE78).
- **State 108.** Entry 0x800C0DD4 registers 0x800C1228 (0x800C0FA4-B8), which calls 0x800E68A4: it spawns a projectile node (0x8003121C at 0x800E69D8). The spawn record's room comes from 0x80118CAC at 0x800E69C4 [inf: the shooter object's +592].
- **State 85** (0x800C2A5C) and **state 87** (0x80055868) were not settled. 87 makes no send itself.
- **No room test on the chosen victim.** 0x80035C58 tests the pre-chosen victim (block+152) with a segment/body test (0x8012F28C, 0x801389DC, 0x8013C688, 0x8013CBEC). It reads no 0x801F662C and no r13-23276. On a hit it sends event 11 synchronously from the attacker to the victim (0x80035D54, 0x80035F78, 0x80036000, 0x80036058).
  - 0x8013CBEC filters its shape table (0x805852F0) by entry+0 == r13-23276 (0x8013CD18-20): in the frame it uses room A's blockers [inf].
- **Victim searches filter by the attacker's room (+592), never by r13-23276:**
  - 0x80036094's walk (0x80036754-6C), then event 59, reach 0x801F8914, facing 0x80124CEC, victim := candidate (0x800367EC), then 0x80035C58;
  - 0x80035740 (0x80035798);
  - 0x80035974 (0x800359D4);
  - 0x800371F4 (0x800374C4-CC).
  - The walk in 0x80036094 is enabled only by block flag bit 1, or when the attacker id == 0x801F62B8 (r13-21496) with no victim (0x8003669C-0x800366C8). [inf] In the frame that is player 1's id, so player 2's untargeted 0x2000/0x4000-kind attacks never search.
- **The victim's side has two gates, both against the room in play:**
  1. Event 11 → the victim's handler (player 0x80059C44-68; kind 4 0x8003BA94/0x8003BAE8/0x8003BB98) → **0x8006255C(block)**. It compares the room of block+152's node with r13-23276 (0x80062584-0x800625B8; exit 0x80063798). This is a second gate the first readers did not record.
  2. Event 39 from attacker to victim (0x80063754-64) → 0x80061B9C, gate at 0x80061BD0.
  - Death: 53 (0x8006366C-7C) → 0x800637B8, gate at 0x8006383C.
- **Spells.**
  - The contact callback 0x801CB580 (table-reached; the host hooks it for its hurt log at hle.c:27277) sends 59 from caster to victim (0x801CB650-60).
  - 0x801CB858 builds the block (attacker = caster at 0x801CB898, victim at 0x801CB8A8) and POSTS 237 (0x801CBB18-2C, delayed send 0x801F57C0).
  - The victim's 237 is re-sent as event 11 (player 0x80059BA4-DC, kind 4 0x8003B940-78) → 0x8006255C (gate) → 39 → 0x80061B9C (gate).
  - There is no room test of the victim on the spell side. The spell's sound is tagged with r13-23276 (0x801CB7D0).
  - 0x801C5994 steps a spell only if 0x80155450(spell+8 room) (hle.c:12770-12774). Under N=2 a resident room B passes.
- **The finisher.** Its stab callbacks send 47 (synchronous) and 48 (synchronous at 0x80032CF0; also delayed at 0x80032C80) from the finisher to the victim id info+140→+28 (0x80032C1C, 0x80032CB0). Kind 4 turns 48 into 0x8006255C (0x8003DCA0-B8, gate) and 47 into a state change (0x8003D878).
- **Other area hits filter the candidate's room against r13-23276:**
  - 0x80149F54 / 0x8014A450 / 0x8014AB30 (0x8014A1C4, 0x8014A844, 0x8014AF44), then a synchronous 11 with attacker 0;
  - 0x80178428 (0x80178508), sends 39.
  - 0x800C6394 instead filters by the attacker's +592 (0x800C63D0, 0x800C6418-20), then 39.

## (2) Turn or frame, for player 2
- **Swing and shot entries run in their turn.** The turn runs COOP_CONTROLLER (hle.c:20724) after coop_stay_room_in (20613) and the who-swap (20603-20609). The A press sends event 40 synchronously (0x80045E98-9C), so the state entry and the block copy (+152 included) run with room B and player 2's words.
- **Where +152 comes from:** the TARGET verb (R, 0x800BF4D8 → 0x8019B744), which also runs in the turn and is +592-filtered.
  - Exception: state 40's tick re-posts event 40 with a delay (0x800552AC-C0), so that start happens in the frame, from the queue.
- **The hits themselves run in the frame.** Key callbacks fire from the animation advance of the character step: 0x801FA44C → 0x801FA4C0 bl 0x80123698 → 0x80123224 → 0x80120668 → 0x8012088C → 0x80121A1C (bctrl).
  - That happens in the behaviour pass with room A and player 1's who-words.
  - coop_character_step wraps only 0x80133434, not the anim advance.
  - The host already says player 2's moves play outside their turn (hle.c:12841, coop_move_sound).
  - coop_events (21614-21682) does not cover these: it wraps the player behaviour 0x80058DD8, not the character step.
- **Spell steps** run inside coop_spells_update's coop_be_player2 (hle.c:12790) with room A. Their 237 is delivered from the queue in the frame.
- **Result today:** player 2 apart in room B hits an enemy of room B; the swing test passes (attacker room == victim room). The victim's 11 reaches 0x8006255C, and B != A at 0x800625AC, so nothing happens. A spell's 237 → 11 dies the same way.

## (3) The finisher's look
- 0x800330E0 runs only from state 1's event-3 tick: 0x80058C38-44 bl 0x800C030C → 0x800C03D0 bl 0x800330E0(node, attack block).
- For player 2's node that is the frame's behaviour pass, under coop_events with their CoopPlayerState in but no room word (the 0x80058DD8 call at hle.c:21658).
- So it keeps bodies whose room == A (0x800332E8-F0), states 8/9 (0x80033324-38), then event 193, distance 0x8016D848, cone 0x801F8BC8.
- Player 2 apart is offered room A's bodies (measured from room-B coordinates) and none of their own.
- Fix site: coop_stay_room_in round coop_events' 0x80058DD8 call for player 2 when coop_stay_apart (or round 0x800330E0 alone).

## (4) A bracket r13-23276 := victim's room for one call
It must cover **0x8006255C** as well as 0x80061B9C and 0x800637B8: melee, finisher, spells and area hits reach 0x8006255C first.

**Inside 0x80061B9C / 0x80061E90** (bl closure to depth 8):
- Only the gate (0x80061BD0) and the block stamp in 0x8019B418 (0x8019B6AC: a debug field).

**Synchronous sends from 0x80061B9C / 0x80061E90:**
- **Event 8 to the victim** (victim to itself at 0x80061C90; attacker to victim at 0x800621B8). Kind 4's handler 0x80038E54 destroys the node when its room != r13-23276 (0x80038ED4-E8, 0x801DCB58), as does kind 4's other event-8 handler (0x8003CBE4-0x8003CBF8).
  - So lifting the gate without setting the room word would destroy B's enemies. The bracket must cover this.
  - 0x80038E54 also reads the room word: room 208 rule (0x80038F7C), sound tag (0x800390C4), rooms 409/251 (0x80039180), and a sound and a moves-stream stamp through its callees (see "Common to 8/11").
  - Player's event 8: room-96 rule (0x80059F90).
- **Event 11 to the victim's stats+36 id** (0x80061F48-9C) → that node's 11 handler → 0x8006255C (see "Inside 0x8006255C"). [inf: a protector]
- **Event 201 to the attacker:** kind 4's 0x8003B694 has no room read.
- **Event 53 to the victim** → 0x800637B8.
- **A chained 39** with the sub-block 0x8019B724 (0x80062254-64) → 0x80061B9C again (re-entry is fine under the same bracket).

**Inside 0x800637B8:**
- The gate (0x8006383C). Nothing else to depth 4. The death move 0x80122920 fires anim key callbacks by bctrl; those were not followed.

**Inside 0x8006255C:**
- The gate (0x800625A4).
- Sound tags: 0x800302DC (0x80030454), 0x8019DA18 → 0x801A0C90 (0x801A0CC0).
- Effect stamps: 0x8014699C → 0x801432BC sets +4900 := room (0x80143304); 0x801D6D30 sets +52 := room (0x801D6D58).
- 0x801D12B0 passes the room to 0x800CB774 (0x801D1364).
- 0x801F9FD0 counts nodes in r13-23276 (0x801F9FF8, 0x801F56B0).
- Kind 4's own 11 case has a room-39 rule (0x8003B9F0-F8).
- All of these then act on the victim's room, which is what we want.

**Common to the 8 and 11 handlers:**
- 0x801F9FD0 (the node count above).
- Sound tag via 0x80064C84 → 0x801A0B48 (0x801A0B88).
- Moves-stream stamp via 0x80064F1C → 0x80145BB0 → 0x801432BC (0x80143304).

**Do we need to swap the slot table?**
- **No:** none of these calls reads 0x801551A0(2), 0x80155234(2) or 0x80155130 through bl.
- **Two current-slot lookups:**
  - 0x80125A70 → 0x80155544(room) looks at table +0 → 2 and +4 → 3, else -1 (0x80155544-0x801555F8). It answers either resident slot without a swap.
  - 0x800324B8 → 0x800323A8 → 0x80123F70 goes by +592, except for r13-26524's object.
- **[inf]** It holds as long as the who-words are not set to player 2 for the call. Callbacks reached by bctrl (stat-change callbacks, the death move's keys) were not checked.

**Gate rule for the bracket:** apply it only when the victim's room is resident (room_slot_of, hle.c:13803) and the attacker's +592 (block+156 → 0x801F5F88 → +592) == the victim's.
- For a projectile or spell, take the room at +8 / the node's room.

**Other places:**
- The attacker-side gates and filters (0x8005BA70 0x8005BAA8, 0x8005BCA4 0x8005BCCC, 0x8005BFCC 0x8005C180, 0x80149F54-family, 0x80178428) need the attacker's room in. That is a separate bracket on those behaviours' steps, for the enemy lens.
- Player 2's melee needs none of them.

## Hazards
- Bracketing 0x80061B9C alone misses 0x8006255C (the first gate for swings, spells, finishers and area 11s) and the event-8 destroy (0x80038ED4).
- With the gate lifted but no room word, an enemy of room B is destroyed by its own wake event.
- Swing internals that read "the player" get player 1 in the frame:
  - the walk's enable test (0x800366A0);
  - 0x8013C688's player-object exclusion (0x8013C6B8, 0x8013C6E8);
  - 0x80061E90's extras (help flags, 201) [inf].
- Blockers in 0x8013CBEC are room-A's in the frame [inf].

## Open
- Which creatures kinds 5 and 50 are.
- States 85 and 87 (0x800C2A5C, 0x80055868): their hit path.
- Which hit test the guns' block flags pick in 0x80038860.
- Whether 0x80122920 / anim-key callbacks under 0x800637B8 read the current slot.
- Whether 0x801CB580 is called inside the spell step (that is, under coop_be_player2) [inf: yes].
- Not run: ED_HURT_LOG with player 2 apart, swinging and casting, to confirm the refusal at lr 0x800625B8, and that the 11 comes from 0x80035C58 / 0x80059BE0.

Scratch scripts I wrote (helpers, not deliverables):
- C:\Users\stevi\AppData\Local\Temp\claude\9282620f7b4851ea\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\closure.py
- C:\Users\stevi\AppData\Local\Temp\claude\9282620f7b4851ea\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\fnsum.py
- C:\Users\stevi\AppData\Local\Temp\claude\9282620f7b4851ea\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\encl.py

######## read:quarry-sight
READER: quarry, chase and sight with the players apart, and the host's view test. Nothing was run and no file was changed. HEAD 2886af7. hle.c = runtime/host/hle.c.

## (1) 0x800324B8(node, other): it returns 0 or 1, never an id
- **Every exit returns 0 or 1.** The 1s are at 0x80032774, 0x80032824, 0x80032A5C and 0x80032AA4 (`li r3,1`). The 0s are at 0x800327C8 and 0x80032AB8. The id is only stored (0x801F654C = `stw r4,20(stats)`). So in the sight test, `quarry == r13-21496` (0x8003B25C-64) means "found someone" only because player 1's id is 1.
- **The `other != 0` path (0x80032644-0x80032778) has NO room test.**
  - It refuses only in these cases:
    - this node is the player (0x80032658);
    - `other`'s flags have 0x80 or 0x8000 set (0x8003260C, 0x80032668);
    - the team tests fail;
    - event 59 to `other` does not answer 1 (0x800326B0-C0);
    - the line of sight fails, when stats+12 bit 2 is set (0x80032720-4C).
  - Then it sets stats+12 |= 1 (locked, 0x80032758) and stats+20 := other's id (0x80032770).
  - Callers that pass `other`: event 101 in 0x8003B470 (0x8003B904-24, other = 0x801F5F88(from)), and 0x8005D0BC, 0x8005EA10, 0x80060014, 0x800606E4, 0x80061290, 0x8006461C, 0x80074C90, 0x800902B0, 0x80094028, 0x800A5814, 0x8000E3A4, 0x8002D778/0x8002DEEC, 0x80062898 (none of these were read).
- **The three room tests of the `other == 0` path are confirmed:**
  - (i) The locked target (stats+12 bit 0, 0x800327D8) is kept only if 0x801F5F88(old) is found, 0x801F662C(it) == r29 (0x800327F4-FC), and event 59 answers 1 (0x80032810-20). Otherwise bit 0 is cleared (0x8003282C-38).
  - (ii) A candidate needs 0x801F662C(candidate) == r29 (0x800328A8, 0x800328C8-CC).
  - (iii) The old target is kept only if it is found, in the same room (0x80032A68-7C) and answers 59. Otherwise stats+20 := 0 (0x80032AAC-B4) and it returns 0.
  - Any candidate the walk finds beats a kept old target: the tail runs only when the walk found nobody (0x80032A3C-44).
- **Corrections to enemy-readers.md:**
  - A candidate's flag 0x80 must be CLEAR (`rlwinm. r24,r30,0,24,24` at 0x800328AC; `bne skip` at 0x800328D0-D4), not set. 0x8000 must be clear too (0x800328D8).
  - When 0x800C8E10(node) == 0, it returns 0 WITHOUT clearing the target (0x800327BC-CC: `bne` continues).
  - An enemy whose own flags have 0x1000 set takes only r13-21496 as a candidate (0x8003283C, 0x8003293C-50). In the frame that is player 1 only.
  - The walk has no distance cap. It takes the nearest (unsigned compare 0x800329C4, r19 starts at -1) within the cone 0x801F8BC8 (half-angle from +330, which is 150 at spawn, 0x8003091C-38).

## (2) The chase tick's mask (0x8003ADFC) and its period
- **The function is 0x8003AD74(node, object, …, r8 → r25, r9 → r24).** Its callers are 0x8003C5A4 (in 0x8003B470) and 0x8003EF3C.
- **r25** = r13-22216 (the game frame counter, +1 per frame by 0x801DC9F8, called from the main loop at 0x8001871C) + (s16) info+156, a per-enemy phase (0x8003B504-10; 0x8003E9BC-C8).
- **r24** = (s8) record+353 (0x8003B50C; 0x8003E9C4). It is set at spawn (0x80030954-64 and 0x80051134-44) from the table at 0x80620470 (r2-32752; r2 = 0x80628460), which holds {07, 0F, 1F, 3F} (read from generated/main.dol).
  - The index r13-26412 goes round 0..3, one step per spawn (0x80030968-84, 0x80051148-64). It is reset to 0 at 0x80021560, 0x80021C3C and 0x8002204C.
- **What the mask does:**
  - The quarry runs only when (frame + phase) & mask == 0 (0x8003ADFC-0x8003AE0C).
  - In between, stats+20 is read straight off: 0 → give up (0x8003AE1C → 0x8003B100). Otherwise 0x801F5F88(id) (0x8003AE20) gives the target, and the enemy walks or attacks toward 0x801F8D40's position, with no room test.
- **Result:** depending on spawn order, an enemy re-checks every 8, 16, 32 or 64 frames. So it chases a target that has left its room for up to 7, 15, 31 or 63 frames. The same mask shape is at 0x8003A828 (quarry at 0x8003A84C).

## (3) 0x8003AA30 sends no event 58
- **It sends event 237 to itself** (from = to = its own id, 0x8003AB24-34, delayed post 0x801F57C0).
  - Only when flag 0x20000 is set (0x8003AA60).
  - Every 236 + (rand & 127) frames (countdown at +346, 0x8003AA68-8C and 0x8003AB38-4C).
  - Plus an effect, 0x8014699C. It has nothing to do with sight.
- **The real 'seen' sanity hit is 0x800CA370**, the first call in 0x8003B1C8 (0x8003B1F8). It calls 0x800CA4E4(node, 8, 23, amount).
  - It sends event 58 from the enemy TO r13-21496, the player's id, not stats+20 (0x800CA504 → r27; 0x800CA69C-A8).
  - Gates:
    - every 32nd frame (0x800CA410-18);
    - 'has seen' (bit 8) clear (0x800CA3C4);
    - stats+20 is 0 or equal to r13-21496 (0x800CA3D8-0x800CA408);
    - r13-26336 != 0 (0x800CA458);
    - 0x80155738() == 0 and 0x8004303C() == 0;
    - 0x800C8E10 != 0;
    - its state is not 8, 9 or 31.
  - Its target is 0x801F5F88(r13-21496), i.e. r13-21492 directly, and that node's info+159 must be 1 (0x800CA550).
  - Then the cone 0x801F8BC8 (0x800CA5B0) and a line of sight through 0x80123F70(the ENEMY's object) — its own room's record by +592 — by 0x801388CC/0x801390F0 (0x800CA5C0-40).
  - Then flags |= 8 and sound 23.
  - **There is NO room test.**
- **Other kinds call it directly.** 0x800CA370 has 12 callers (0x8003E9D8, 0x8005E5D4, 0x8007226C, …) and 0x800968D4 calls 0x800CA4E4 directly. coop_sighting swaps players only round 0x8003B1C8.
- **Another 58 with no sight or room test:** state 37 (info+159 == 0x25) sends 58 to r13-21496 every 128 frames (0x8003B374-0x8003B428).
- **Correction: 0x8003B1C8's own quarry call is acquisition only.** It runs when r13-26336 == 0 AND bit 8 is clear AND stats+20 == 0 (each test is `bc 4,2` = branch if not equal, to skip: 0x8003B234, 0x8003B23C, 0x8003B24C).
- **Correction to the sight-target hazard.** That hazard said that run as player 2 the look "can never mark seen", so the game's own 58 to player 2 cannot come from coop_sighting. That is wrong. Under coop_sighting (stats+20 == 133), 0x800CA370 reads r27 = 133 = stats+20 and sends the game's own 58 to player 2.

## (4) Each player's room-mates, without a host retarget
- **Enemy in room A with stats+20 = 133:**
  - 0x801F5F88(133): 133 != r13-21496 (1), so it walks the list by node+16 (0x801F5FC4-E4) and finds player 2's node. 0x801F662C of it is their object +592 = B.
  - At its next quarry run, the walk may take player 1 (same room) → stats+20 := 1. If not, (i) and (iii) fail on the room → stats+20 := 0.
  - Until that run, it chases toward B's coordinates for up to 63 frames.
  - HAZARD: coop_sighting (hle.c:21470-21474) swaps with no room test. So 0x800CA370 tests player 2's room-B position through room A's collision, which can give player 2 a false cross-room sanity hit.
- **Enemy in room B with stats+20 = 1:**
  - 0x801F5F88(1) returns r13-21492 directly (0x801F5FA8-B4), i.e. room A → cleared at its next run.
  - HAZARD: while stats+20 is 0 or 1, 0x800CA370 in the frame tests player 1's room-A position from room B, through B's collision. The game itself can give player 1 a cross-room sanity hit.
- **Does the walk take player 2 on its own? Yes [inf on two words].**
  - Player 2's kind is 1, the same as player 1's: logs `[coop] player 2 0x81128CD4: … type 1 model 70 class 1` beside player 1's `class 1`. 'class' is actor+158 (hle.c:6781). Kind 1 passes 0x800328E0-F0.
  - Player 2 is in the enemy's room.
  - [inf] Flags 0x80 and 0x8000 clear and the team word (+148) the same as player 1's: player 2 is the same type 1. Not read.
  - Enemies with the 0x1000 flag never take player 2 in the frame.
  - Event 59 to player 2 from an enemy:
    - coop_send_event (20944-21001) zeroes it only between the two players (20978-20985) or for a held player (20990-21000). Otherwise it passes through.
    - coop_events (21614, branch 21627-21665) then runs 0x80058DD8 in player 2's words. The player's 59 case 0x8005A610 answers 1, except when:
      - r13-29252 == 0 (0x8005A618-24);
      - [0x80307170+8] == 13 and 0x800AA3E0() (0x8005A63C-54);
      - 0x801D620C(r30) == 4 and 0x80038AFC(sender) (0x8005A66C-94).
  - So an enemy in room B takes player 2 (stats+20 := 133) whenever its quarry runs with other == 0: the acquisition in 0x8003B1C8 (when r13-26336 == 0 and stats+20 == 0), the chase tick, or 0x8003A84C. The chase then finds player 2 by id.
- **HAZARD: in the frame the enemy gets marked 'seen' with no hit.** The acquisition compare is result 1 == r13-21496 1, so an enemy whose walk took player 2 gets bit 8 and sound 23 (0x8003B25C-80). Bit 8 then blocks its 0x800CA370 (0x800CA3C4).
- **HAZARD: the host's +12 write corrupts the quarry's flags.** COOP_TARGET_A = +12 (21354) is the quarry's flags word (0x801F6448 `lwz 12` / 0x801F65D4 `stw 12`; bit 0 = locked, bit 2 = line of sight), not a second id. When +12 happened to be 1 (locked), coop_enemy_target 24115-24116 writes 133 = 0x85 (locked + line of sight + 0x80) on a retarget, and 1 back. Drop that write.

## (5) Where the host needs a room test
1. **coop_in_view (21959-21981):** first condition `seer+592 == them+592` (COOP_OBJECT_ROOM, 15932). This covers both callers.
   - camera_collide (6263) takes object = r13 + g_camera_player_offset, i.e. r13-26524 (find_player 6571-6583).
   - Its world is 0x80123F70(that object). For the player's object that returns the CURRENT slot whatever +592 says (0x80123F94-A0), and the offset used is the player's +612 (6279).
   - So for a seer in room B it tests room A's mesh with player 1's offset: WRONG. It needs a variant that takes the seer's object (a non-player → 0x80123F70 by +592 → its own room; its own +612). [inf] It is right only inside player 2's bracketed turn.
2. **coop_seen_shared (22023):** the room test at 22051, before coop_in_view.
3. **coop_seen_later (22069):** at 22083, `objects[k]+592 == object+592`.
4. **coop_sighting (21473-21474):** add 'enemy room == player 2's room' to the swap condition. Passing through on a stale 133 is safe, because in the frame 0x800CA370's r29 is 0 and no 58 is sent.
5. **coop_enemy_target (24061-24121):**
   - Replace the both-in-its-room gate (24071-24073) with an in-room flag per player.
   - Call coop_seen_later (24102) always.
   - Retarget, including the held override (24107-24111), only among players in the enemy's room. Set stats+20 to the in-room player, or 0, when it points at a player in another room.
   - Remove the +12 write (24115-24116).
6. **The game's own cross-room sight (no host code has it today):**
   - An intercept on 0x800CA4E4 (2 callers): return 0 when 0x801F662C(node) != 0x801F662C(0x801F5F88(r13-21496)). This covers both players, since coop_sighting swaps r13-21496.
   - Or drop the event 58 in coop_events (before 21682) for player 2 and in player 1's path.
7. **One choke point for every quarry path:** all four keep/lock paths ask event 59 first (0x800326B0, 0x80032810, 0x800329DC, 0x80032A90). An answer of 0 in coop_send_event (near 20986) when the target is a player and the sender's node has a room != the player's room would make every path room-correct, including the `other` path with no room test (event 101 etc.). Skip this test when the sender has no node or its room is -1: hazards (0x8014512C) and damage fields (0x801CB658) ask 59 too.

## Open
- **Player 2's flags (0x80/0x8000) and team word (+148), against player 1's:** settle with one probe. ED_COOP_ENEMY_LOG=1 already prints the class.
- **Who sends event 101** (0x8002D480, 0x80018A8C, 0x8004000C, 0x800DC74C, 0x801A3254) and what the other `other != 0` callers do.
- **What r13-26336 is.** It gates the acquisition (== 0) against 0x800CA370's 58 (!= 0). Writers: 0x8000BB10, 0x800216DC, 0x80040868.
- **The other event-58 senders:** 0x80018A7C, 0x800211A8, 0x800767D0, 0x800B3968, 0x800B43B4, 0x800CCC24.
- **Whether 0x80123F70(enemy object) finds slot B's record** for the line-of-sight ray while slot B is resident (no probe has checked it).

