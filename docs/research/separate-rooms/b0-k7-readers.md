# B0: K7's rest (the ten room-change sites, kind 50's filter, the tracker's room)

For design-split-hold.md §3.3, §3.8, §3.12 and §13 B0. Read only: no code changed, nothing built, the game not run.
Tools: `ed fn`, `ed dis`, `ed uses`, build/ed-db/disasm.txt and calls.json, generated/main.dol (tables read from it).
[read] = seen at the address given. [inferred] = reasoned, not seen.

## 0. The answer

- **Only one of the ten sites is a node kind's: 0x8009B760 is kind 32's** (Walk on Ceiling, sanity effect 22).
- **0x800C04EC is kind 2's** (the player's death state). Kind 2 can't go into K7: it is the stayer.
- The other eight are not owned by any node. They belong to a script function (ed145) or to game modes 1, 4, 5, 6 and 9.
- **New kind for K7: 28.** Its event 67 reaches 0x800A2548, the ticket writer shared with kinds 50 and 82. This is
  transitions.md's "0x80065FF0 (not traced)".
- **None of the ten paths has a busy ask (bl 0x80155738) or a held ask (bl 0x801F68D4) of its own.** I checked each
  function on each path against the 35 busy-ask sites and the 12 held-ask sites [read].
  - Kind 32's and kind 2's sites are held only through the node gates: the update at 0x8000749C and the motion advance.
  - The script and game-mode sites have no gate at all.
- **0x800A2C20 is kind 50's own copy of the player's busy filter.** While the game is busy, 0x800CA964 swallows events
  8, 39, 133 and the rest. An answer of 0 lets a death through to state 8. State 8's motion end then calls 0x800A2548.
  So 0x800A2C24 must keep the real word.
- **The kind-32 tracker has no room.** It is made with no object (+52 is 0), so coop_object_room gives -1.
- **Final K7:** kinds 28, 32, 39, 50, 82. Sites (lr): 0x800E891C, 0x800A29B8, 0x800A2C24. Kind 56 is optional (§6).
- **Two design points:**
  - G1's refusal loses ed145's return: the request dispatcher clears the request whatever happens.
  - G2 can hold the door's countdown for a cinematic whose end writes its own ticket, which G1 then refuses.

## 1. Game modes and the request dispatcher (needed by several sites)

**The mode table** is at 0x80238810: an enter and a frame function for each mode [read dol]. The mode word is r13-26188,
set by 0x80018808 [read].

| Mode | Enter / frame | What it is |
|---|---|---|
| 0 | 0x8001FFB8 / 0x80020144 | title or menus [inferred] |
| 1 | 0x800202FC / 0x80020328 | level start: loads the chapter's scripts (0x8003F4F4) and its room. Set from title code 0x8001C768/0x8001C86C and mode 5's 0x80021E90 [read] |
| 2 | 0x80008234 / 0x800083C8 | play (TW_WORLD_FRAME) |
| 3 | 0x80111078 / 0x80111234 | the Tome |
| 4 | 0x8004F5C0 / 0x8004FB20 | a movie. Started by 0x8004F4D0, which ed83 play_fmv calls. Steps 0x800EB6C0 until done [read] |
| 5 | 0x80022350 / 0x800224B8 | game end and menus: 0x80022328(n) sets it (game over at 0x80058950, ed89's case 10) [read] |
| 6 | 0x80050174 / 0x80050A00 | an in-engine cinematic. Started by 0x8004F374, which ed55 play_cinematic calls. Also ticks the ticket, at 0x80050A90 [read] |
| 9 | 0x80020494 / 0x80020558 | load a saved game (0x800AE61C) [read the calls; the name is inferred] |
| 7, 8, 10-13 | 0x80112D98 / ... | Tome-like screens (use-mix.md) |

**The request dispatcher, 0x800B7208(mask)** [read 0x800B7208-0x800B72F8]:
- Pending request: r13-25172. Its delay: r13-25176. Both are set by 0x800B71E8.
- When the request & mask is non-zero and the delay has run out, it branches on the request:
  - 1 or 16: 0x800ADA24 (a ticket);
  - 32 or 64: 0x800AD6F4 (a direct load). 16 and 32 also call 0x80153B2C(2, 0);
  - 4: 0x800B7100 (ed88);
  - 8: ed89's path;
  - 2: 0x801A172C.
- **Then it clears both words, whatever the branch did** (0x800B72F0-0x800B72F8).
- Callers: the world frame twice (0x800083F4, right after the ticket tick at 0x800083EC, and 0x800085B0), and modes 0, 4
  and 6, all with mask 127. The Tome calls it with masks 4 and 2 [read].
- The ten setter sites pass 0, 1, 2, 4, 8 or 32. Nothing sets 16 or 64 [read the li before each bl 0x800B71E8].

## 2. The four ticket writers

### 2.1 0x800ADA24 (lr 0x800ADABC): ed145, the scripted scene's return

- **Path:** a script calls ed145 (0x8001545C). It stores the script in r13-25180 and sets request 1 (0x800154E0). The
  next 0x800B7208(127) runs 0x800ADA24 at 0x800B7290. That writes the ticket at 0x800ADAB8: transition 17, flags 68,
  callback 0x800AD8A0 [read; agrees with ed145's kit entry].
- **Owner:** no node kind. It is the script function ed145. 16 scripts call it: L00/129 (the piano, levels 0, 5, 9, 12,
  13 and 16), 1560, 1925-1927, 2305, 2318, 788, L11/1521 [read `ed uses ed145`]. It is the end of a scene that ed144
  kept (0x800AD588 into 0x80326540).
- **Gate:** none [read]. 0x8001545C, 0x800B7208 and 0x800ADA24 hold no ask.
- **The player:** 0x800ADA24 tests the current player 0x801F62B0 for non-zero, then reads the kept room at
  0x80326540+16. Before the ticket, if that room isn't r13-23276, it writes r13-24248 := 0 [read 0x800ADA3C-0x800ADA7C].
  - The callback 0x800AD8A0 restores the kept state; I did not read its target.
  - The direct form 0x800AD6F4 (§3.1) writes the kept place, heading and room onto 0x801F62B0's object.
  - So whoever is current when it runs gets the kept place: player 1 in solo. In player 2's turn it would be player 2,
    with player 1's kept place, if ed144 ran in player 1's turn [inferred].
- **For the design:** G1 refuses this ticket, but 0x800B7208 has already cleared request 1. The scene's return is lost,
  and the player is left in the scene's room and camera [inferred from the clear].
  - Better than G1 here: under split_counting, skip 0x800B7208 whole while r13-25172 & mask & (1|16|32|64) != 0.
    The request and its delay then survive to the firing.
  - One intercept at the dispatcher's entry covers both the world frame's calls.

### 2.2 0x8012D1A0 (lr 0x8012D2F0): the cinematic's end

- **Path:** mode 6's frame 0x80050A00 calls 0x8012A8A4 at 0x80050AC0. That calls 0x8012D1A0 at 0x8012ABF0 [read].
- The ticket at 0x8012D2EC is written only when the cinematic record's +120 (a room) is non-zero and bit 0 of
  0x803162C0+456 is clear. It uses transition 17, flags 32, entry -2 and callback 0x80152488 [read 0x8012D25C-0x8012D2EC].
- **Owner:** game mode 6, not a node.
- Mode 6 is started by 0x8004F374. Its callers [read]:
  - ed55 (0x8001265C at 0x800126F4);
  - **kind 2's event 242** (0x8005A850);
  - kind 2's state 8 event 17 in chapter 13 (0x80058920);
  - **kind 32's effect 12**: callback 0x8009B9DC, registered at 0x8009B91C by its start 0x8009B8E4 (start table entry 12);
  - the cinematic chain in 0x80025FF0 (0x80027060);
  - mode 1 (0x8002037C);
  - 0x80051788 and 0x80051874.
- Event 242 is not in the busy filter's list (hold-readers.md 4.3), so a busy game passes it to kind 2's handler as
  in solo [read].
- **Gate:** none on the path [read].
- **For the design:** suppose a cinematic starts inside the window (a woken script, or a 242). G2 holds the door's
  countdown for it. At its end it writes its own ticket while the door's still counts, and G1 refuses it. The scene's
  room change is then lost [inferred]. G1 needs a rule for lr 0x8012D2F0: let it replace the door's ticket, or end the
  window first.

### 2.3 0x800C04EC (lr 0x800C0598): a death in room 255

- **Path:** kind 2's handler 0x80058DD8 calls state 8's function 0x8005855C at 0x8005B938, behind `cmpwi r20, 8` [read].
  - At event 6 (motion end), if 0x8009E218() != 0, it calls 0x800C04EC(0, id) at 0x8005876C [read 0x80058750-0x8005876C].
  - 0x8009E218 finds the tracker (kind 32) and tests its current effect (0x80068D48) against a table at 0x80245470
    [read 0x8009E218-0x8009E2A0]. So this is "the player dies while an effect of that set is on". That the set is the
    fake-death effects is inferred.
- Inside 0x800C04EC [read 0x800C0500-0x800C05D4]:
  - in room 255, chapter 5, with flag 883 on: ed85-like 0x80145760(20), then a ticket to room 252 (transition 0,
    script 2779, countdown 0 raised to 1);
  - otherwise: event 81 to the tracker, which ends the effect.
- **Owner:** kind 2, the player. It must not go into K7.
- **Gate:** none inside. Event 6 is not in 0x800CA964's list. The motion end that sends it needs the motion advance,
  which is gated at 0x800271F0/0x80027298 (busy and the held bit) [read].
- **The player:**
  - Player 1 or player 2: whichever node dies runs it, with its own id in r4.
  - The room test reads the room in play (r13-23276). The effect test reads the one tracker, which watches one
    character (§5) [read].
  - Under A1/A3 the stayer's motion runs, so a stayer dying in the window can reach this. G1 then refuses the ticket.
    That is B2's question (deaths), and one more reason to read it.

### 2.4 0x8009B760 (lr 0x8009B80C): Walk on Ceiling, kind 32

- **Start:** the sanity start table at 0x80243D8C holds 0x8009B520 at entry 22 (0x80243DE4) [read dol]. Effect 22 is
  Walk on Ceiling (sanity.md).
  - 0x8009B520 sets the effect record's stepper to 0x8006BBC4: 0x80068F48 writes +28, at 0x8009B554.
  - It registers the callback 0x8009B760 (0x8006AF5C at 0x8009B5BC, slot +64) [read].
- **Run:** kind 32's handler 0x8006CEAC, in state 1, on event 3 (the frame), calls 0x8006A2B8 at 0x8006D358.
  - That calls the record's +28 by bctrl at 0x8006A2FC.
  - 0x8006BBC4 calls the +64 callback by bctrl at 0x8006BC3C, when the node's state matches the stored +96 [read].
  - 0x8009B760 tests 0x8006DD70, the scene hold [read 0x8009B7B4].
  - It then writes the ticket at 0x8009B808: room = record +76 + 1000, transition 0, countdown 3, delay 2.
- The sanity return 0x8006BFAC is kind 32's too: 0x8006C850 registers its callback 0x8006B438 (-19400 at 0x8006CD34),
  and kind 32 calls 0x8006C850 at 0x8006D5D0 [read].
- **Owner:** kind 32.
- **Gate:** only the node update gate. Event 3 is sent by 0x801F5D48 in 0x80007370, behind the busy ask at 0x8000749C
  and the held ask at 0x800074AC [read]. Nothing inside asks.
- **The player:** the tracker watches one character, the one made at chapter load (§5). In co-op that is player 1
  [inferred: 0x800074E8 makes the chapter's character; player 2 is the host's copy].

## 3. The six direct loads (0x80152298)

| Load (site) | Function's callers, up | Owner | Gate | Mark |
|---|---|---|---|---|
| 0x800AD6F4 (0x800AD7C8, flags 68) | 0x800B7208 on request 32 (0x800B72B4) or 64 (0x800B7298). Request 32's setters: 0x80027084 (0x80025FF0's tail after op 23, the cinematic's finish), 0x8004F890 and 0x8004FACC (0x8004F850, called by the enter of mode 4 at 0x8004F67C and of mode 6 at 0x800501A0), 0x8004FF6C (0x8004FC3C, the movie's end). Nothing sets 64 | modes 4 and 6 (a movie or cinematic going back to what ed144 kept) | none | [read] |
| 0x80157458 (0x8015758C, current room, 68) | 0x80025FF0's op 23 (0x80026484, 0x800264A4, 0x800264C4). Op 23 is queued by 0x8005092C at 0x800509E8, which mode 6's frame (0x80050E0C) and 0x8012C7A0 call (the latter from the cinematic end 0x8012A8A4). 0x80025FF0 runs in the world frame (0x80008548), mode 1, mode 6, the Tome and 0x800B8468 | mode 6: the cinematic's teardown, then the room reloaded | none | [read] |
| 0x800ADADC (0x800ADC24) | mode 1's frame 0x80020328 (0x8002041C); 0x800ADDCC (0x800ADF00), which mode 5's case function 0x80021648 calls (0x80022248, ed89's load back from 0x80307184) and mode 9's 0x800AE61C (0x800AE780) | modes 1, 5 and 9 | none | [read] |
| 0x800AE61C (0x800AE768, flags 92) | mode 9's frame 0x80020558 (0x800206D4) | mode 9 (load a game) | none | [read] |
| 0x8004FC3C (0x8004FDAC, current room, 68, when 0x803162C0+482 is set) | mode 4's frame 0x8004FB20 (0x8004FBA0, 0x8004FBD0) when the movie step 0x800EB6C0 says done | mode 4 (a movie's end) | none | [read] |
| 0x80020328 (0x80020358, current room, 0) | it is mode 1's frame (0 bl callers; mode table) | mode 1 | none | [read] |

- **Who starts mode 4** (0x8004F4D0's 13 sites) [read the sites; owners from the kind table]:
  - ed83 (0x8001263C);
  - kind 2's event 242 (0x8005A838);
  - kind 56 (0x800A6A9C, after its event 141 path runs script 829);
  - the timer 0x8007E124 (0x8007E2F0, below);
  - the Tome (0x80111030);
  - the chains in 0x80025FF0, 0x8004F850 and 0x8004FC3C;
  - title code 0x8001B924, and 0x80051788/0x80051874.
- **0x8007E124's timer** (record 0x80323920 +20):
  - It counts down only while not busy: its busy ask is at 0x8007E140 [read].
  - At 0 it starts movie 64. Then, at -120, it calls 0x80022328(3), the game over.
  - If a ticket counts (0x80155818 > 0 at 0x8007E29C), or the current player 0x801F62B0 is in state 8, it sets the
    count to 1 and waits [read 0x8007E29C-0x8007E2BC].
  - So it can't start the movie inside a countdown, whatever the busy answer. Keeping the word there (§3.3) is still
    right.
- **The player:** the loads take the room in play (r13-23276) or the kept room, and so both players. 0x800AD6F4 writes
  the current player's object (0x801F62B0 → 0x801F633C): player 1 in solo, and the current one in co-op [read 0x800AD718-0x800AD78C].
- **In the window:** none of these is called from a node in play mode. A load can land in the window only in two cases:
  - a mode 4 or 6 begun inside it, which G2's cutscene_playing wait covers (g_movie_file covers mode 4);
  - a cinematic's finish (op 23), read by the world frame's 0x80025FF0.
  G5's report is enough for these [inferred].

## 4. 0x800A2C20, read as 0x800A29B4 was (kind 50)

- **The head of kind 50's handler** (0x800A2B94) [read 0x800A2C14-0x800A2C48]:
  - After its getters: `if (0x80155738() != 0 && 0x800CA964(node, msg, answer) != 0) return 1`.
  - It is the same busy filter the player's handler uses at 0x80058E80. While busy, it swallows 8, 9, 11, 24-30, 32,
    39, 50, 53, 55, 57, 59, 60, 63, 67, 101, 103, 107, 125, 133 and 202 (hold-readers.md 4.3).
- **Where kind 50 calls 0x800A2548**, then the ticket at 0x800A281C [read the compares before 0x800A3268, 0x800A32A8,
  0x800A3B48]:
  - event 250 (0x800A3230);
  - event 135 when 0x8006A540(...) is 0 (0x800A3274);
  - **state 8 (`cmpwi r20, 8` at 0x800A3AD0), event 6**: the death's motion end.
- 250 and 135 are not on the filter's list, so they reach the handler busy or not, as in solo [read].
- **The path an answer of 0 opens:**
  1. Event 8 (a death) or 39 (a hit) reaches kind 50's state handlers.
  2. State 8 begins.
  3. If the motion runs (A1 frees it when +592 is the stayer's room), event 6 arrives.
  4. 0x800A2548 writes a ticket when the linked object's room isn't the room in play (transitions.md).
  5. G1 refuses it, and the return to the body is lost [inferred].
- **Event 3**, the frame, is where 0x800A298C runs (0x800A2D04). That is the timer path already read: busy ask
  0x800A29B4, hold 0x8004303C, 0x8006A540(23), one off stat 1 of 0x801F6250's character, then 135 to r13-26424.
- **So both kind 50 sites keep the real word (lr 0x800A29B8, 0x800A2C24), and kind 50 stays off A1 and A2.**
- **The player:** 0x800A298C takes the stat from 0x801F6250() (r13-21484's character) and sends 135 to r13-26424. Which
  player those are in co-op wasn't read. 0x800A2548 moves the room in play, and so both players [inferred].

## 5. The kind-32 tracker's room: none (-1)

- **Made by 0x800CBC38(character node)** [read 0x800CBC38-0x800CBCD4]. It looks for the first kind-32 node
  (0x801F8FD8(first, 32)). If there is none, it calls 0x8003460C(32).
  - Data +56 := the character's id (0x801F62C8, stored at 0x800CBC9C).
  - The character's +140 record, at +68 := the tracker's id.
  - Then event 21 is queued.
- 0x800CBC38 is called once, at 0x80007B78 in 0x800074E8. That function makes the chapter's character (r31, from
  0x8003121C at 0x8000784C) [read].
- **0x8003460C, kind 32's case** (0x800349E8-0x80034A34) [read]:
  - It takes a node from 0x801F5E40, which sets +56 := -1, inits +16/+20/+24/+8/+32, and sets +28 |= 2 (0x801F5C3C).
  - Then +56 := 4, kind +8 := 32, data +40 := a new block. That's all.
  - **No object: +52 is never written.**
- Both of the pool allocator's branches clear the block (bl 0x80003100(p, 0, size) at 0x801717C4 and 0x80171804) [read],
  so +52 is 0.
- The only setter of +52 is 0x801F6544. Its four callers are spawn paths with an object: 0x80030870 (room spawns),
  0x80051068, 0x800E68A4 and 0x801F9E98 [read the sites]. None is on 0x800CBC38's path [inferred].
- **So 0x801F633C(tracker) is 0, and coop_object_room (hle.c:13986) returns -1.**
  - The tracker is roomless. A1 never frees it.
  - K7 matters for it only through S2 (it still gets the door's 102).
  - Its update stands in the window only if its +28 bit 8 is set. I did not find who would set it on the tracker:
    0x801F6828(node, 1) sets it, and no call with the tracker was found [not read].
- **The player:** one tracker watches the chapter's first character: player 1 [inferred, §2.4].

## 6. Final K7

| Kind | Why (path to a ticket, a load or a player's motion) | Mark |
|---|---|---|
| 28 | event 67 → 0x80065B10 (0x80066AD8, `cmpwi r26, 67` at 0x80066AB8) → 0x800A2548 at 0x80065FF0 when 0x800BC6B8() == 1 and a byte == 3 → ticket 0x800A281C. Kind 28 is the type a creature takes on its other death path (0x8003911C, enemy-readers-2.md:110). Its handler has no busy ask, so A2 must not free messages to it | [read the chain; event 67's sender and the meaning not read] |
| 32 | 0x8009B808 (§2.4), 0x8006BFF4 (sanity return), and mode 6 through effect 12's 0x8009B9DC | [read] |
| 39 | capture → room 83 (design §16 B1) | [read, design] |
| 50 | 135, 250 and state 8's event 6 → 0x800A2548; the timer → 135 (§4) | [read] |
| 82 | state 8's event 6 (`cmpwi r22, 8` at 0x800A46F0, `cmpwi r27, 6` at 0x800A4738) → 0x800A2548 at 0x800A475C | [read] |
| 56 (optional) | event 141's path starts movie 8-16 (0x800A6A9C): a mode change whose end can load the room (0x8004FC3C). It is not a ticket. It seems to be a boss's ending [inferred] | [read the call] |

- **Not K7:** kind 2 (0x800C04EC, §2.3) is the stayer. Guard it through G1 and B2.
- **Busy-ask sites that must keep the real word (lr):**
  - 0x800E891C (kind 39);
  - 0x800A29B8 and 0x800A2C24 (kind 50);
  - 0x80058E84 for the mover (K1).
- **The A1/A2 sites must also refuse K7 kinds' nodes:** 0x800074A0 (r28), 0x800271F4, 0x8002729C, 0x80027768 and
  0x800277F8 (r31), and 0x801F5420 for messages to a K7 node.
- The other sites in §3.3 keep the word as the design has them. 0x8007E144 is safe either way (§3).

## 7. Still open

- Who sends event 67 to kind 28, and what kind 28 is (§6).
- Whether anything sets the tracker's +28 bit 8 (§5).
- 0x800AD8A0's restore target, and whether 0x8012D1A0's record +120 is ever non-zero for a scene a script can start in
  play (transitions.md's open point).
- For G1: hold request 1 at 0x800B7208 instead of refusing its ticket (§2.1). Give lr 0x8012D2F0 a rule (§2.2).
