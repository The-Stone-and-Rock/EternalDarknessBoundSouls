# R6b readers (2026-11-11 night): the enemies' point graph and routes for the room player 2 stays in

Read only. Facts carry EVIDENCE; [inf] marks inferences.

######## read:graph
READER REPORT: the AI point graph (r13-25060 / r13-25064). I changed no files and ran nothing. Addresses come from `ed dis` and build/ed-db/disasm.txt.

## Main finding
Swapping the graph round B's enemy's behaviour call is NOT enough. The zombie does not search the graph itself. It posts a job to the background thread, and that thread searches later, with whatever words are current at that time. That time is outside every host bracket, so the words are room A's.

- **The post:** zombie 0x8003B470 calls 0x800BBDF4 (path status) at 0x8003C568, then 0x800BBCC8 (post) at 0x8003C584, then the chase 0x8003AD74 at 0x8003C5A4.
- **0x800BBCC8(node, ai):**
  - It checks that the current slot's record is loaded: 0x80155130(2), +0x8142/+0x8143 at 0x800BBCEC-0x800BBD18.
  - It checks that the thread is on: 0x800BB728 returns r13-25048.
  - It sends the node's handle (0x801F62C8) to queue 0x805B2AFC without blocking (OSSendMessage 0x8020190C at 0x800BBD88, r5=0).
  - It marks the job pending: +20 bit 1 of the 24-byte record 0x80327B80 + 24*ai(+176), at 0x800BBDC8.
- **The thread:** 0x800BB730 is the body of the background thread (thread 0x805BBB60, priority 31, per docs/coop-two-tomes-design.md:79). It blocks in OSReceiveMessage 0x802019D4 at 0x800BB794 on 0x805B2AFC. It runs only when the game thread sleeps:
  - the frame's retrace wait (hle.c 14350);
  - a room change's OSReceiveMessage (0x80152298);
  - the Tome's way in (0x800BBC08).
- **The job (0x800BB810-0x800BBB88), in order:**
  - 0x800BA530(0) at 0x800BB914.
  - Start and end points are chosen at 0x800BB95C, 0x800BB984 and 0x800BB9A8 through 0x800B9EE4, mode 194 or 66 (chosen by r13-29556). Those modes include bit 0x40, a line-of-sight test that reads the current slot's collision: 0x801551A0(2) at 0x800BA1E8 and 0x800BA354.
  - A* 0x800BB1AC at 0x800BB9C0.
  - Smoothing 0x800BACEC at 0x800BB9DC. It reads r13-23276 (== 120, at 0x800BADB0) and the current slot's collision (0x801551A0(2) at 0x800BADFC).
  - The 400-byte result 0x803279F0 is copied into the 824-byte path record 0x80327D60 + 824*ai(+172), at 0x800BBAC4 / 0x800BBAF0.
- **Consequence today [inf]:** a job from B's zombie runs with A's words. 0x800BA530(0) sees r13-25064 == r13-23276 == A and does nothing. The path is then built from A's points and A's collision. That fits B's zombies standing still beyond about 550 units.
- **The stored path:** it is a list of POINTERS to 56-byte graph entries. 0x800BACEC copies entry pointers from the A* list, and the consumers dereference them: entry +0 points to the record's s16 x/y/z, read by 0x800BAAA8 (called at 0x800BC14C). So a stored path is valid only while its buffer is not rebuilt.

## (1) 0x800BA530(flag), read whole (0x800BA530-0x800BA668)
- **Setup:** r31 = 0x80155234(2), the current slot's record +160 (0x800BA544). 0x80155234 calls 0x80155130(2), tests +0x8142/+0x8143 and returns record+160 or 0.
  - There is no null check on r31 before 0x800BA584. If the current slot is not loaded, it reads from 0+100 [inf crash risk].
- **The flag:** flag != 0 sets r13-25064 := 0 (0x800BA54C-0x800BA55C).
- **The test:** if r13-25064 == r13-23276, it returns at once (0x800BA560-0x800BA56C).
- **Otherwise:**
  - r13-25064 := r13-23276 (0x800BA570).
  - memset(r13-25060, 0, 28000) (0x800BA574-0x800BA580).
- **Loop 1 (0x800BA584-0x800BA5BC):** for i < (u16)rec160[+96]:
  - buf[i*56 + 0] = rec160[+100] + 12*i (a pointer into the slot's 12-byte list);
  - buf[i*56 + 54] = (u8)i.
- **Loop 2 (0x800BA5C0-0x800BA654), the neighbours:** for each entry e while buf[e]+0 != 0, let src = buf[e]+0. For k < src+6 (u8, the link count):
  - id = src+8 [k], a u32 array;
  - p = 0x800B9EE4(id, 0, sp+8, 1, 0, 0, 0). Flag 1 means "find by pointer": it scans the buffer for the entry whose +0 == id (0x800BA120-0x800BA148) and returns buf + e*56;
  - if id != 0, p != 0 and k < 8: buf[e + 4 + 4k] = p. So +4..+32 hold up to 8 neighbour ENTRY pointers;
  - then buf[e+53] = k, the full link count (it can be more than 8).
- **Other fields:** +36..+52 are the A* fields, written by the search, not here. +52 bit 0 is set by 0x800BB5FC and cleared by 0x800BB5EC/0x800BB60C; bit 1 means "open" (0x800BB41C). The open list is sorted on +48 (0x800BB44C) [inf].
- **Other writes:** none. No other r13 word and no global is written. 0x800B9EE4 in flag-1 mode only stores -1 and then the index into sp+8, and reads only r13-25060.
- **Return:** nothing meaningful. r3 is r13-23276 on the early exit, else 0 from the last lwzx. No caller uses r3:
  - 0x80020364: li r3, 0
  - 0x80152084: or r3, r29
  - 0x800BB918: lfs
  - 0x80152520 / 0x80152754: the next call is 0x800428D0, which calls 0x80039FF4 [I did not check whether 0x80039FF4 reads r3].

## (2) Every reader
Allocation: 0x800BA4BC (play start) does r13-25060 := 0x801F31D4(28000, tag 421), r13-25056 := 8000-byte pool (tag 427), r13-25052 := 0 (0x800BA4E0-0x800BA51C).

**r13-25064** is read and written only in 0x800BA530 (0x800BA55C, 0x800BA560, 0x800BA570).

**r13-25060:**

| Function | What it reads from the graph | Room reads (r13-23276, current slot, 0x8058AFC0) |
|---|---|---|
| 0x800B9EE4, the search | 10 reads, 0x800B9F90-0x800BA3BC: walks entries until +0 == 0 or 500; by pointer, by index (+54), random (0x800F4474), random in range (scratch 0x80327928, 50 pointers), nearest | Modes with bit 0x40 read the current slot via 0x801551A0(2) at 0x800BA1E8 / 0x800BA354. It never reads r13-23276 or 0x8058AFC0 directly. |
| 0x800BA530 | builds the buffer, see (1) | r13-23276 and 0x80155234(2) |
| 0x800BA66C, debug draw | 0x800BA850 / 0x800BA86C: +0, +4..+32, +53; draws with 0x801DC4EC / 0x800EDC60. Its only bl site is 0x800ED7E4 (table-reached). | none |
| 0x800BB60C | 0x800BB628..0x800BB6E8: clears +52 bit 0 on all 500 entries, then empties the open list (0x800BB4D4 / 0x800BB40C) | none |

- **Correction to stay-readers.md:** 0x800BB60C is not run every frame. Its only bl sites are 0x800BB1CC, 0x800BB25C and 0x800BB350, all inside the A* 0x800BB1AC.
- **Indirect users, through entry pointers:** the A* (0x800BB1AC and 0x800BB374..0x800BB5E0, pool r13-25056 and open-list head r13-25052) and the smoothing 0x800BACEC. The pool and open list are emptied at the A*'s start and end (0x800BB1CC / 0x800BB350 call 0x800BB60C), so they never carry state across jobs [inf].
- **Callers of 0x800B9EE4 in the game thread** (the ones a behaviour bracket would cover):
  - the 11 script verbs (stay-readers.md 216);
  - 0x800BC1EC at 0x800BC354 (mode 8, random in range: a wander point);
  - effects 0x8014A6BC at 0x8014A9D4 (mode 8; it reads r13-23276 straight after, at 0x8014A9D8).

## (3) Callers of 0x800BA530

| Site | Function | Flag | When |
|---|---|---|---|
| 0x80020360 | 0x80020328 (table-reached) | 1 (forced) | right after 0x80152298 at 0x80020358 (play start / load) |
| 0x80152080 | 0x80151FDC, the arrival | 0 | called from 0x801523A4 inside 0x80152298 (step 8 has already set r13-23276 to the new room) |
| 0x8015251C | 0x80152488 | 0 | its only caller is 0x80152600, inside 0x80152614; followed by 0x800428D0 / 0x800428F0 |
| 0x80152750 | 0x80152614, the ticket runner | 0 | followed by 0x800428D0 and 0x800428F0 (0x800428F0 calls 0x800647C8, the thread resume [inf]) |
| 0x800BB914 | the background thread's job | 0 | asynchronous, see above |

- 0x80152614's callers are 0x801558F4 (the ticket tick 0x80155830, run at 0x800083EC) and 0x800AE000.
- **R4 hazard, today [inf]:** player 2's change runs 0x80152614 with r13-23276 = B's room (hle.c 13844-13847). So 0x80152080 rebuilds THE ONLY buffer to B's new room. Its stored A paths now point at entries that hold B's data. After coop_stay_change_end (hle.c 13853-13867) the first thread job rebuilds it back to A at 0x800BB914.

## (4) A second buffer for room B
**Is 0x800BA530(0) safe inside a B bracket?** Yes. It reads only r13-23276, r13-25064, r13-25060, and the current slot through 0x80155234(2) → 0x80155130(2), which reads the table at 0x8058AFC0. It reads nothing else of the room. The flag-1 link lookup reads only the buffer.
- Pass flag 0 with the B pair's r13-25064 set to -1 first, or pass flag 1 (it zeroes r13-25064 itself).
- B's slot must be loaded (+0x8142/+0x8143), because nothing guards a null r31.

**Can 0x800BB914 run inside such a bracket?** Only if the game thread sleeps inside it:
- OSSendMessage at 0x800BBD88 does not block, and the thread has priority 31, so a post does not switch threads [inf: I did not read the game thread's priority].
- A behaviour-call bracket does not sleep [inf].
- If it did, the job would see r13-25064 == r13-23276 == B and run an A job on B's graph and collision. It would not rebuild the wrong buffer.
- If only r13-23276 and the slot table were swapped, without the pair, it would rebuild A's buffer with B's points.
- In the R4 change bracket the game thread does sleep (0x80042910, 0x80152298 step 9), but the thread is paused first: message -1717 sets r13-25048 := 0, drops the queued jobs (0x800BB7B4-0x800BB7C8) and acks. While paused it skips jobs (0x800BB804-0x800BB80C). It resumes at 0x80152758, which is still inside the bracket [inf: no further sleep before the 0x800083FC end].

**Words to swap.** The bracket round B's behaviour covers only the game-thread readers in (2). The path must also be made in B, so the job needs its own bracket:
- **Where:** on 0x800BA530 entry with lr == 0x800BB918. The handle is in r26 and the node in r24 (both live: 0x800BA530 saves only r27-r31, and 0x800BB7B4/0x800BB820 set them).
- **When:** if coop_object_room(node) is B and B is resident and loaded.
- **What, until the job's end:** swap in r13-23276, ROOM_SLOT_TABLE +0/+4, r13-25060 and r13-25064. The job's end is the OSDisableInterrupts 0x8020102C at 0x800BB9E0, after 0x800BACEC has filled 0x803279F0. The 824-byte copy after it needs no room words.
- **Same four words in the change bracket:** notify_ticket_run / coop_stay_change_end, so B's change rebuilds B's buffer and A's stays A's.
- **Not swapped:** r13-25056 / r13-25052 (the per-job workspace), 0x80327928 and 0x803279F0 (scratch), and the path records 0x80327D60 / 0x80327B80 (one per enemy).

**Invariant:** outside every bracket, r13-25060 is A's buffer and r13-25064 is A's room or 0. The B buffer's room word is set to -1 whenever B's slot is replaced, or when the players meet in one room.

######## read:routes
READER REPORT: an enemy's route (READ ONLY, HEAD 2886af7 plus the working tree). Addresses come from `ed dis` and `ed fn`. [inf] marks an inference.

## Main finding
The route is not computed in the enemy's tick. A background OS thread computes it.
- 0x800BB730 is that thread's body. It is created at 0x80154060-0x80154084: entry lis -32756/addi -18640 = 0x800BB730, stack 8192, priority r8 = 31 (the lowest), via 0x80203D84 [inf: OSCreateThread by its argument shape].
- It runs only when the higher-priority threads block, which is the main thread's frame wait. So it always runs with player 1's words: r13-23276 = A and the slot table current = A.
- So no bracket around 0x8003AD74 or 0x8003B470 can make it route in B.

## (1) The functions, and where the route lives
**Storage: two static pools of 20 entries each.** 0x800BB168 clears them: 16480 = 20x824 and 480 = 20x24.
- **The route record**, 0x80327D60 + i*824:
  - +0..+399 is list 0 and +400..+799 is list 1. Each list holds 100 words, null-terminated.
  - +800 and +804 are the two lists' cursors.
  - +808 is the room stamp, written as r13-23276 at 0x800BAF20. Nothing reads it.
  - +812 is the owner id, +816 the allocation time (r13-22216).
  - +820 holds flags: bit 0x1 in use, bit 0x4 no path.
- **The route state**, 0x80327B80 + i*24:
  - +0 is the mode: -1 empty, -2 requested, 0 or 1 = the list in use.
  - +4 is the owner id, +8 the allocation time, +12 the request time.
  - +16 = 1 means a new route is ready.
  - +20 holds flags: 0x1 in use, 0x2 request pending, 0x4 no path.
- **The enemy's indices**: info+172 is the record index and info+176 the state index. info = *(0x801F6300(node)+140), as at 0x800BAB1C-20. Both are set to the same index at 0x800BAFD4/0x800BAFDC.
- info+359 (a byte) is a re-route counter. In 0x800BAC78(node,10), called at 0x8003ADC4: when it reaches 10 or more, the cursor is reset by 0x800BABD4.

**A route stores POINTERS, two levels deep.**
- A route entry is a pointer to a 56-byte graph node inside the r13-25060 buffer. The A* writes them into scratch 0x803279F0 by walking the +36 parent chain (0x800BB23C-0x800BB258). The worker then copies them into the record (0x800BBAA4-0x800BBAF0).
- The graph node's +0 is a pointer to a 12-byte point in the current slot's record list: 0x80155234(2) = record+160; its list at +100, count at +96 (0x800BA584-0x800BA5B4).
- Other graph node fields: +4..+35 up to 8 neighbour pointers, +53 neighbour count, +54 index, +36 parent, +40/+44/+48 the A* g/h/f values, +52 A* flags.
- A point is halfwords x,y,z at +0, +2, +4. 0x800BAAA8 converts them to floats.
- Use: 0x800BC0B0 reads route[mode*400 + cursor*4] (0x800BC130), then lwz 0(r3) (0x800BC13C), then 0x800BAAA8.

**Each function:**
- **0x800BAF78(node, info)** allocates a BLANK record and computes nothing.
  - If info+172 != -1 it returns the existing index.
  - Otherwise it takes the first record whose +820 bit 0 is clear and calls 0x800BAED4. That sets +820 = 1, +816 = the time, +812 = id, +808 = r13-23276, clears +0..+799, and sets the state entry's +0 = -1, +20 = 1, +16 = 0.
- **0x800BB0C0(node, info)** drops the route. With both indices in 0..19, it clears the 24-byte state entry and the 824-byte record, then sets +176 = +172 = -1 (0x800BB100-0x800BB148).
- **0x800BBCC8(node, info)** is the route REQUEST.
  - It needs the current slot's record loaded: 0x80155130(2), +0x8143/+0x8142 (0x800BBCEC-0x800BBD18).
  - It needs the worker running: 0x800BB728() != 0.
  - If +176 < 0 and +172 < 0, it ALLOCATES on demand with 0x800BAF78 (0x800BBD30-0x800BBD54).
  - If bit 0x2 is clear, it posts the id to the queue 0x805B2AFC with 0x8020190C (no wait). On success, mode -1 becomes -2, bit 0x2 is set and +12 = the time.
- **0x800BBDF4(node, info)** adopts a ready route (+16 == 1).
  - If the mode is below 0: mode := 0 and cursor := 0.
  - Otherwise the mode flips and 0x800BAB10 places the new cursor at the old current node's index in the new list, or at 0.
  - It then clears +12, bit 0x2 and +16.
- **0x800BC044(node, info)** returns state +20 & 0x4, the "no path" flag.
- **0x800BC0B0(node, info, out)** writes the current waypoint (floats) to out and returns 1. It returns 0 when there is no route, the mode is not 0 or 1, the entry is null, or the node's +0 is null.
- **0x800BC174(node, info)** advances the cursor: cursor++.
- **0x800BB728()** returns r13-25048, the worker's "enabled" flag. Message -6666 sets it to 1 (0x800BB7E4-0x800BB7FC). Message -1717 clears it and acknowledges on 0x805B2B5C (0x800BB79C-0x800BB7E0). 0x800BBC08 sends -1717 with 0x80201AB0 and waits for the acknowledgement.
- **0x800BB730** is the worker loop. For each id received (0x802019D4 at 0x800BB794):
  - node = 0x801F5F88(id) in r24. Start = the node object's position.
  - The goal is the position of the node's target (node+36 -> +20, through 0x801F6308/0x801F63BC). With no target, 0x801F8D40 supplies it.
  - **0x800BA530(0)** at 0x800BB914 rebuilds the graph.
  - **0x800B9EE4** at 0x800BB95C/0x800BB984/0x800BB9A8 finds the nearest graph nodes.
  - **0x800BB1AC(start, goal)** at 0x800BB9C0 is the A*. Its pool is r13-25056 and its open list r13-25052.
  - **0x800BACEC** at 0x800BB9DC smooths the path.
  - It then copies the result into the list not in use: mode 0 goes to +400, mode 1, -1 or -2 go to +0. It sets +16 = 1. If the A* failed it sets bit 0x4 (0x800BBAF4-0x800BBB3C); otherwise it clears it (0x800BBB40-7C).

## (2) The chase tick 0x8003AD74
State 3 of 0x8003B470 runs three calls each tick:
1. 0x800BBDF4, the adopt (0x8003C568).
2. If 0x801F63BC(r19) != 0, 0x800BBCC8, the request (0x8003C584).
3. 0x8003AD74 (0x8003C5A4).

The sibling behaviour has the same three: 0x8003EF00, 0x8003EF1C, 0x8003EF3C. So a route is requested on demand, every tick, while the enemy has a target. It is not only built at event 62.

Inside 0x8003AD74:
- **Inside the attack reach** (from 0x800C92D8): the attack, 0x80039840.
- **Under 550** (0x8003AF08) with 0x8003AD50 true: walks straight (0x80122BB0/0x80122D6C). 0x8003AD50 is a ray through 0x80123F70(obj). That function picks the slot whose record room == obj+592 (0x80123FD4-0x80123FE8), so this test is already room-correct for B.
- **Otherwise, the route:**
  - 0x800BC044 != 0 (no path): 0x80124788, then pending state 1 (0x8003B020-0x8003B04C).
  - 0x800BC0B0 == 0 (no waypoint yet, mode -2): state 1 (0x8003B0E0).
  - Otherwise it walks to the waypoint. Under 80 units (cmplwi 0x50 at 0x8003B078), 0x800BC174 advances the cursor.
- So the first tick after a request always drops to state 1, and the adopt picks the route up on a later state-3 tick.

## (3) After event 61
- State 0's event 61 (0x8003B850-0x8003B87C) calls 0x800BB0C0, 0x8003A2E8(id), 0x80064A7C(id) and 0x800E8464.
- The enemy is NOT stuck. On the next state-3 tick 0x800BBCC8 sees +172/+176 < 0, allocates (0x800BBD4C-54) and requests again. Event 62 is not needed.
- A game hazard [inf]: if a request is in flight when the route is dropped, the worker still indexes with +176 = -1. It writes 0x80327B80-24 +16 at 0x800BBA74. The owner check at 0x800BBA2C/0x800BBA4C only calls 0x800BB020, which does nothing.
- **0x8030AB10**: 20 entries of 36 bytes. +0 is the id, +4..+16 four effect ids, +20..+32 four effect handles. Only 0x8003Axxx code addresses it (address scan). 0x8003A2E8 kills each live effect with 0x8014D698(h,1) when 0x8014F8A8(h) still matches, then clears the entry.
- 0x8003A1D0(id) returns 4 if the id is registered there. Its callers are 0x80038F60 (death), 0x8003B2C0 (sight), 0x8003CB30/0x8003CB60 (state 33's 61), 0x8003CDC8, 0x8003CF18, 0x8003CF54, 0x80051FC0 and 0x800520D0.
- These are the effects of event 30 [inf: dismemberment]. The table is not read by the chase and is not a count of attackers.
- **0x80316978**: entries of 272 bytes, id at +248. 0x800648BC(id) registers an enemy only when info+159 == 3, info+148 == 2, the node's room (0x801F662C) == r13-23276 (0x8006490C), and *(info+4) is empty. It then stores the entry's address in *(info+4). 0x80064A7C clears both.
- No reader was found outside 0x80064xxx, and 0x8003AD74 calls neither table. Its purpose is open.

## (4) Room, slot and player reads
- **0x800BBCC8**: the current slot's record loaded (passes in the frame, since A is loaded).
- **0x800BAED4**: +808 := r13-23276 (no reader).
- **In the worker:**
  - 0x800BA530: 0x80155234(2), the current slot's record, and r13-25064 vs r13-23276 (0x800BA560-6C). It rebuilds into r13-25060 when they differ.
  - 0x800B9EE4: rays through 0x801551A0(2), the current slot's section (0x800BA1E8, 0x800BA354).
  - 0x800BACEC: r13-23276 == 120 (0x800BADB0) and 0x801551A0(2) (0x800BADFC).
- So the worker today builds A's graph and tests B's coordinates against A's nodes and A's collision.
- With A's collision, no node passes the nearest-node test; the A* is skipped and r29 stays 0 (0x800BB764, 0x800BB9AC). Bit 0x4 is set, so 0x800BC044 sends the enemy to state 1 every tick. That fits r6b-c1 ("chose player 2 but stood", docs/coop-level3-design.md:2492-2493) [inf].
- **Player words**: r13-21492/-21496 are used only inside 0x801F5F88's id shortcut, which is harmless; player 2 is found by the list at +16. r13-26524 appears only in 0x80123F70's shortcut. 0x800BC044, 0x800BC0B0, 0x800BC174, 0x800BAF78 and 0x800BB0C0 read no room, slot or player word.

## (5) Recommendation: the smallest bracket, inside the worker thread
1. **Open** at the entry of 0x800BA530 when LR == 0x800BB918. r24 = the node and r26 = its id; r24 is not reassigned between 0x800BB824 and 0x800BB914.
   - R = coop_object_room(r24).
   - Open only if r6_apart (hle.c:24110), R != r13-23276, and r6_room_loaded(R) (hle.c ~24130).
   - Swap the room word and slot table +0/+4 with coop_room_in(cpu, R, keep) (hle.c:14244).
   - Save r13-25060/-25064, then set r13-25060 := B's own 28000-byte buffer and r13-25064 := the room that buffer was built for (a host variable; -1 forces one build from B's record).
   - Clear MSR[EE]. interrupt.c:525-528 delivers the retrace only with EE set, so otherwise the main thread could preempt the worker and run a frame with B in play.
2. **Close** at the entry of 0x8020102C when LR == 0x800BB9E4. This is reached on every path after 0x800BA530, including the "no start or goal node" branch at 0x800BB9AC.
   - Store r13-25064 into the host variable.
   - Restore r13-25060/-25064 and call coop_stay_room_out (hle.c:14262).
   - Put EE back before the game's call runs, so its OSRestoreInterrupts at 0x800BBB84 stays correct [inf: 0x8020102C/0x80201054 = OSDisableInterrupts/OSRestoreInterrupts, by their use].
3. **A second graph buffer is mandatory.** Routes hold pointers into the buffer. Rebuilding the shared one for B would point all of A's enemies' routes at B's points, and A and B would rebuild it in turn.
   - Allocate it once from the main thread the way 0x800BA4C8 does: 0x801F31D4(28000, 1, file, line) [inf].
   - When B's room leaves its slot, zero the buffer and set its room to -1. Then 0x800BC0B0 sees a node with +0 == 0, returns 0, the enemy goes to state 1 (safe), and the next request rebuilds.
4. **No main-thread bracket is needed for routing.**
   - The request passes with A's slot.
   - The direct test under 550 is room-correct (0x80123F70).
   - Walking already works at close range (r6c-c1n hits, coop-level3-design.md:2487-2488).
   - Event 62's 0x800BAF78 does not need re-running for B's enemies after player 1 leaves, because 0x800BBCC8 allocates on demand.
   - A stale bit 0x4 from earlier failed searches on A's graph clears at the next good search (0x800BBB40-7C).
5. **Alternative** if hooking 0x8020102C is unwanted: hook 0x800BBCC8 for B's nodes and run the worker's steps (0x800BA530, 0x800B9EE4 x2, 0x800BB1AC, 0x800BACEC, the copy, +16 = 1) synchronously in a B bracket on the main thread. Do it only while the worker is idle, because the A* pool and the 0x803279F0 scratch are shared. It is more code.

Files: runtime/host/hle.c (13808, 14244-14266, 24110-24135, 4679-4686), runtime/host/interrupt.c (525-528), docs/coop-level3-design.md (2492-2499), build/ed-db/disasm.txt.

######## read:memory
READER REPORT: guest memory for a second 28000-byte AI point graph (R6b). Nothing was changed, run or built.

(1) How the game allocates r13-25060
- One allocation site and one writer. 0x800BA4BC does `li r3,28000; li r4,1; r5=0x8024B3B0 (file string); li r6,421; bl 0x801F31D4` and then `stw r3,-25060(r13)` at 0x800BA4E0. It zeroes the block with memset 0x80003100 (28000 bytes).
- The same function allocates a second block of 8000 bytes (line 427) into r13-25056 (0x800BA508) and sets r13-25052 = 0 (0x800BA51C).
- `grep '-25060(r13)'` in build/ed-db/disasm.txt finds no other stw. No load of the pointer is followed by a free call. The only bl after a load is the memset at 0x800BA580.
- 0x801F31D4 is the game's heap allocator (size, flags, file, line), with 92 call sites (`ed fn`). docs/research/separate-rooms/readers.md:42-43 and refute.md:11,55 treat it as the OS heap allocator.
- So the buffer is on the heap, not static. Its address depends on the run and cannot be derived from code. [inf] It lies inside the game's 24 MB (the game's OS sees only 24 MB, per .deps/GXRuntime/src/core/cpu.c:19-21). The host could log it with mem_read32(r13-25060).
- Caller: 0x80021BD8 is the only call to 0x800BA4BC, inside 0x80021648. That function's prologue (stwu) is at 0x80021648, which is FIN_PLAY_START (hle.c:21736). So the allocation happens at play start, and there is no "already allocated" check.
- Layout, from 0x800BA530:
  - At most 500 entries of 56 bytes (cmpwi 500 at 0x800BA87C and 0x800BA9B0).
  - +0: a pointer into the current slot record's +100 list (12-byte source points; 0x800BA59C, stride at 0x800BA5A0).
  - +4..+32: up to 8 neighbour pointers from 0x800B9EE4 (0x800BA620). 0x800B9EE4 returns buffer + idx*56 (0x800B9FD4-0x800B9FDC), so these are absolute pointers into the same buffer.
  - +52: search flags (0x800BB424 sets bit 2).
  - +53: neighbour count (0x800BA644).
  - +54: index (0x800BA5AC).
- Consequence: a copy of room A's graph cannot be relocated with memcpy. The second graph must be built in place: r13-25060 points at the new buffer, B is current (coop_room_in), and r13-25064 is set to B's buffer key (or the call 0x800BA530(1) is used, which zeroes r13-25064 at 0x800BA55C).
- The 8000-byte r13-25056 block is a pool of 500 × 16-byte search nodes (0x800BB3D8: +12 bit 0 = in use, +8 = graph entry). r13-25052 is the open-list head (0x800BB41C-0x800BB4D0).
  - [inf] Each search stays inside one call of 0x800BB1AC. That function calls the reset 0x800BB60C at its start (0x800BB1CC) and at its exits (0x800BB25C, 0x800BB350). The reset clears all graph entries' search bytes and the pool (0x800BB708 bl 0x800BB40C, 0x800BB70C bl 0x800BB4D4).
  - So the pool can stay shared. Only r13-25060 and r13-25064 need swapping, and only around a whole call.

(2) Guest memory layout under ED_ROOM_SLOTS=2
- RAM size is 24 MB (game) + 24 MB (host) + 8 MB with ED_ROOM_SLOTS = 56 MB, ending at 0x83800000 (.deps/GXRuntime/src/core/cpu.c:18-31; "56 MB" also in quickstate.c:436 and frontend.cpp:472).
- Every 0x82/0x83 constant in hle.c was listed with `grep -oE 0x8[23]......`. Apart from hashes and opcodes (hle.c:8163-8275 hash tables; 26932 and 27222 instruction words), the host uses:

| Range | Use | Evidence |
|---|---|---|
| 0x81800000-0x81C00000 | P2 moves slots 0-3 (moves_slot_table = SCRATCH_RAM + k*1 MB) | hle.c:7410, 7450, 7472-7475 |
| 0x81C00000 | SCRATCH_BODY | 7411 |
| from 0x81D00000 | COSTUME_MOVES_SLOT 5 | 7460 |
| 0x81D90000-0x81E00000 | SCRATCH_TRACKS | 7476-7477 |
| 0x81E00000 | SCRATCH_BODY2 | |
| 0x81F00000 / 0x81F80000 | SCRATCH_COSTUME / SCRATCH_COSTUME2 | |
| 0x81FF0000 + 32×88 + 32×4 | SCRATCH_KINDS | 12016-12017 |
| 0x81FFF000 | SCRATCH_HUD | |
| 0x82000000 | SCRATCH_END | 7411-7416 |
| 0x82000000-0x822D8020 | SCRATCH_TOME + TW_TOME_MEMORY | 13141-13142; given to the game at 13452 |
| 0x82300000 | SPELL_COLOUR, one word | 9797, 9814 |
| 0x82400000-0x82500000 | SCRATCH_FINISH, moves slot 6 | 7465 |
| 0x82500000-0x82502000 | FIN_SCRATCH, 2 × 0x1000 | 8651-8652 |
| 0x82510000-0x82600000 | WPN_SCRATCH, 4 × 60 × 0x1000 | 8864-8867, 9335 |
| 0x82600000-0x82800000 | SCRATCH_WPN_P1, host slots 7 and 8 | 7469 |
| 0x82800000-0x82EEA500 | DETAIL_POOL_POINTS / MEMORY / LISTS | 17368-17374; end value 0x82EEA500 confirmed by comment 8734 |
| 0x82F00000-0x83000000 | RETARGET_KEYS, 2 × 0x80000 | 8734-8735 |
| 0x83000000-0x832C8020 | ROOM_SLOT1, ROOM_SLOT_BYTES 2916384 | 6096, 13471; written at 13556 |
| 0x83300000-0x83351CA0 | ROOM_AREA1, ROOM_AREA_BYTES 335008 | 13470, 13472; written at 13575 |
| 0x837FFFFC | Tome-copy canary, last 4 bytes of RAM, test mode only and only in a copy | hle.c:14422 |

- Free gaps: 0x822D8020-0x82300000, 0x82300004-0x82400000, 0x82502000-0x82510000, 0x82EEA500-0x82F00000, 0x832C8020-0x83300000, and 0x83351CA0-0x837FFFFC.
- Recommended range: 0x83400000-0x83408000 (32 KB). Why it is unused:
  - No host constant or computed base lands in 0x83351CA0-0x837FFFF8. The only end-of-RAM use is the canary at 14422.
  - runtime/host/*.c|cpp outside hle.c has no 0x82/0x83 constants.
  - The game never allocates there, because its OS heap is 24 MB.
  - The game writes slot 1 and area 1 only within their sizes (the layout hooks at 13543-13580 give them fixed bases).
  - [inf] This holds unless a third slot is ever laid out.
  - The range exists only with ED_ROOM_SLOTS, which is the only mode R6b runs in. Guard it like 27285: `cpu->ram_size >= X + 0x8000 - GC_RAM_BASE`.
- Fallback if it must also exist without the switch: 0x82380000-0x82388000, between SPELL_COLOUR's word and SCRATCH_FINISH.
- The 8000-byte pool needs no copy (see (1)).

(3) Save states
- quickstate.c:384 saves MEM1 as the whole cpu->ram_size (56 MB with the switch). A buffer at 0x83400000 is therefore saved and loaded byte for byte, together with the slot records its +0 pointers point into.
- A state of another size is refused under ED_ROOM_SLOTS (quickstate.c:436-440).
- What is not saved is host statics, such as "B's graph built for room N".
  - Existing pattern: the blob table at quickstate.c:109-125 (COFM hle_costume_save/load hle.c:11679-11705, COKN 12151-12163), plus hle_state_loaded / coop_state_loaded (hle.c:27885-27908).
  - Simpler option: keep the key word in guest RAM next to the buffer (e.g. at 0x83400000+28000), mirroring r13-25064. It then rides the state, and nothing needs refilling after a load.
  - Use a sentinel the zeroed RAM of a fresh run cannot fake, e.g. store room+1 or a magic. The game's own "none" value is 0 (0x800BA558-0x800BA55C).
- [inf] r13-25060 itself is in the SDA, so it is restored too. A swap left in place at save time would restore swapped. Swap only inside a call, as coop_room_in does.

(4) Host patterns to imitate
- Swapping a game pointer word around one call: coop_room_in / coop_stay_room_out (hle.c:14242-14266), as used by r6_hit_room (24149-24196: keep[], g_r6_depth, hle_guest_call). R6b can add r13-25060 and r13-25064 to that keep/put-back.
- A host-owned scratch the game is pointed at:
  - tw_room_buffer (hle.c:13447-13455): returns SCRATCH_TOME from the game's buffer getter.
  - SCRATCH_KINDS (12000-12040, copy and repoint at 12085-12136): a host copy of a game record in the scratch, with the game's pointer (record+4) set to it.
- A fixed host address with a size guard: SPELL_COLOUR (9797-9815) and the room slots (13471-13472, install guard 27285).
- Building the graph with the game's own code: call 0x800BA530 via hle_guest_call, with B current (coop_room_in), r13-25060 set to the host buffer and r13-25064 set to B's key, then restore both.
- Rebuild when B's slot is reloaded or its room changes. [inf] The +0 pointers point into the slot record, so they go stale when the slot is unpacked again; key on g_room_slot_room (hle.c:13489).
