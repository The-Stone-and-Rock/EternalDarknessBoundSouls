# Readers: what holds the room at a door (event 102, node +28 bit 8, the busy word)

The reading for the change freeze's Phase 1b (design-change-freeze.md section 5: F5, F2', F6, F4). The question: in a split
(both players in one room, one takes a door, the other stays), what makes the stayer's room stand still for the ~62-frame
countdown, who sets it, what reads it, who clears it, and why the solo game does it. Sources: the disassembly
(`build/ed-db/disasm.txt`, `ed dis`), `runtime/host/hle.c` (uncommitted, 29,549 lines), and the Phase 1 runs
`logs/run-20261112-1801*-freeze-*` (M3 = `-180114-freeze-m3-p1-leaves-fight`, the only one with `[change-hold]` rows).
No code was changed, nothing was built, the game was not run.
[read] = seen at the address, line or log row given. [inferred] = reasoned, not seen.

## 0. In short

1. **Nothing sets the hold bit at the ticket. Node +28 bit 8 is a standing flag ("this node stops while the game is busy"),
   set when a node is made and left set. The hold is `busy word != 0 && +28 bit 8`. The real setter at the ticket is the
   busy word: `0x801556CC(4, 1)` at `0x801557EC`, inside the ticket writer 0x80155770.** [read code; log M3]
   - Every creature row in M3 shows `hold 1` on the first row after the ticket (frame 3822), including zombie 33 in room 180,
     a room no 102 was sent to in the run's window. The probe on 0x801F6828 saw no call on any creature in the window.
     After the change the zombies still show `hold 1` and move (busy 0): step 3.5, 4.4, ... per 3 frames from 3887
     [log M3 page-0001.log 866-880, 1686-1701, 1784, 2258].
   - None of the 12 game sites that answer event 102 calls 0x801F6828 or writes +28 (section 3) [read].
   - The host's comments that "event 102 sets the receivers' +28's 8" (hle.c:11327-11329, 14529-14531, 25385) are wrong
     about the mechanism. What 102 does is different (section 3).
2. **The per-ticket writes, in order** [read]:
   - fn66 (0x801666A4) clears THE ASKER's bit: `0x801F6828(0x801F62B0(), 0)` at 0x801667A4, after 0x800440D0 accepts and
     before the ticket. This is before `notify_room_ticket` (on 0x80155770's entry) opens the probe window, which is why M3
     logged no fn66 call. Player 2's own door motion does the same for player 2 (coop_door_motion, hle.c:21767-21774).
   - 0x80155770 writes the ticket, then busy |= 4 (0x801557E4-0x801557EC).
   - The busy set broadcasts event 102 from the asker's id (r13-21496) to the asker's room (0x801556FC-0x80155720).
3. **What reads bit 8**: only 0x801F68D4 (`return +28 & 8`), which has 12 callers. Seven of them gate on the busy word first
   (`0x80155738() != 0 && 0x801F68D4(node)`): the per-frame update events 3 and 4 (0x800074AC, 0x80027774, 0x80027804), the
   motion advance (0x80027200), a motion/physics step (0x800272A8), and the delayed-message queue (0x801F5438, 0x801F55D4).
   The rest are in section 4 [read].
4. **The stayer is not held by this.** The host's intercept on 0x801F68D4 (coop_node_held_ask, hle.c:19727-19747) answers
   "not held" for the player who did not ask, while coop_room_change_on. M1 and M3 log it at lr 0x80027778 (the update
   events), M2 at lr 0x80027204 (the motion advance) [log]. The stayers in M1-M3 had the stick at 0, so their 0.0 step is
   not evidence of a hold [log both.tsv p1sx/p2sx].
   - **But the busy word still reaches the stayer through the game's direct busy asks.** The worst is the player's own
     handler: while busy, 0x80058E80 sends the message to the player's busy filter 0x800CA964, which **swallows hits
     (event 39) and about 20 other events**. So the stayer cannot be hurt during the other's countdown, and those events
     are dropped, not postponed [read; inferred that it applies to the stayer, since the word is one].
5. **The chase cut.** In ten creature kinds, event 102 in state 3 (the chase) stops the motion (0x80124788) and requests
   state 1 (0x801F64A0(node, 1), 0x801F6488(node, 1)). In kind 19, the player while held by an enemy, 102 lets go:
   event 38 to the grabber and to itself [read]. Even with the hold lifted (F5), every chaser in the stayer's room is still
   cut once at the ticket [inferred].
6. **Correction to "apart -> apart doesn't do it".** Since the hold is the one global busy word and a standing bit, the
   stayer's room's creatures are held during the other's countdown **apart too**. Only the chase cut (102) is limited to
   the asker's room. Nothing in R6c (hle.c:25080-25150) or the update path tests the room [read]. That apart creatures
   freeze is [inferred], strongly, but **not measured**: M5 ran without `ED_CHANGE_HOLD=1` and had no creature rows. This
   matches the user's literal example: player 1 fighting in one room, player 2 entering another, player 1's screen pauses.
7. **Clearing.** The busy word is cleared at the arrival, all bits: `0x801556CC(-1, 0)` at 0x80152184, with no 102 because the
   result is 0. The asker's bit is set again at once: `0x801F6828(r13-21492, 1)` at 0x80152190 (M3: `hold bit := 1 on node 1
   (P1) from lr 0x80152194`). Creatures' bits are never cleared. The host clears the stayer's bit at the tail
   (hle.c:14529-14540; M3 3883, lr 0x0BADF010), and it then stays 0 [read; log].
8. **The game's reason, in solo.** While the hero is busy (a door, words, an examine, a find, a scene), the world stands:
   - nodes marked holdable get no update events and their motions do not advance;
   - delayed messages to them wait;
   - the hero's handler swallows hits;
   - a chaser gives up the chase (102: back to state 1) and looks again once the world goes on.

   At a door this means nothing can reach or hurt the hero walking through, and on arrival the creatures they left start
   over. It is "the world stops for the one player", and in co-op the stayer pays for it [read mechanism; the intent is
   inferred].

## 1. The pieces, address by address

### 1.1 The busy word r13-23344 and its set 0x801556CC(bits, on)

- 0x801556CC [read 0x801556CC-0x80155734]:
  - `on ? word | bits : word & ~bits` is stored at r13-23344.
  - If the result is non-zero, `0x801F5620(102, 0x801F62B8() = r13-21496 (the current id), 0, 0x801F662C(0x801F5F88(id))
    = that node's object +592, f1 = r2-22248)`.
  - So **every** busy change that leaves a non-zero word broadcasts 102, not only the door's.
  - A clear to 0 broadcasts nothing.
- 0x80155738 = `return r13-23344` (35 callers, section 4.2) [read]. The host intercepts it as coop_busy_ask
  (hle.c:19704-19718). That intercept answers 0 only for close-ups, words, finds and player 2's holds. It never does so
  for a room change [read].
- The bits, from the host [read hle.c:13034, 11334-11341, 19290-19299]:
  - 1: a script's busy_set, fn91(1). A door that asks first leaves it on.
  - 2: the hold or close-up.
  - 4: the room change.
  - 8: words.
  - 16: a find.
  - 32: a scene.
- coop_room_change_on (hle.c:11334-11341) requires bit 4 set, nothing but 4|2|1, ticket +0 > 0, no cutscene and no game
  menu.
- Measured in M3's countdown: busy 0x4 on every frame 3822-3882, then 0x0 from 3886 [log both.tsv col 44].

### 1.2 The broadcast 0x801F5620(event, sender id, r5, room) and the delivery

- 0x801F5620 walks the node list 0x801F5C20()+4 [read 0x801F5648-0x801F5694]. It skips the node whose +16 is the sender.
  It delivers to nodes whose 0x801F662C (object +592; -1 when the node has no object) is the room or -1, through
  0x801F57C0(event, sender, receiver id, r5).
- 0x801F57C0 builds a message {+0 event, +4 sender, +8 receiver, +12 -1, +16 -1, +20 time = now (0x801F5350) + f1, +32 seq,
  +36 arg} and calls 0x801F5000 [read 0x801F57C0-0x801F5838].
- 0x801F5000 (the deliver) [read 0x801F5000-0x801F51D8]:
  - If the time is still ahead, the message is queued (0x801F5310).
  - Otherwise the type's handler `0x806068A8[node +8 * 16]` is called (0x801F5294 -> bctrl 0x801F52F4) with the state
    node +20.
  - If that handler does not take it (and the event is not 3), the handler is called again with state 0, the type's "any
    state" code (0x801F50F4-0x801F5118).
  - Then the state flush runs while +32 (a change is asked) is set (0x801F514C-0x801F51D4). If +20 == +24, +32 := 0.
    Otherwise event 2 goes to the old state, +20 := +24, and event 1 goes to the new state.
- So +20 = state now, +24 = state asked, +32 = the "asked" byte. The `[change-hold]` "state" column is +24 (hle.c:12318
  COOP_NODE_STATE 24) [read].
- 102 from the busy set is delivered at once (time = now + the r2 constant) [inferred: the constant is not read; M3's rows
  show the effects in the same frame].
- Type handlers are registered by 0x801F5A9C(fn, type) at 0x80030C40-0x80031208 [read]:
  - type 2 (player) 0x80058DD8;
  - type 4 (zombie) 0x8003B470;
  - type 19 (player while held by an enemy) 0x80060EC8;
  - type 29 (script node) 0x801F759C;
  - type 91 (the players' child node: node 9 under player 1, 60 under player 2) 0x800DAD74.

### 1.3 The hold bit: 0x801F6828(node, on) and 0x801F68D4(node)

- 0x801F6828 [read 0x801F6828-0x801F68D0]:
  - It recurses into the node's children first (0x80151500(id, 0) list; its own call at 0x801F687C, lr 0x801F6880).
  - Then, if on and the node's kind (0x801F62D0) is not 21, it does +28 |= 8. If off, +28 &= ~8.
  - **It is the only `ori 0x8` / `stw +28` pair in the node module** [read: grep of `ori ..., 0x0008` followed by
    `stw ..., 28(`; the only other +28 writers in 0x801F4000-0x801F8000 are 0x801F5C54, which ORs in 2, and 0x801F655C, a
    whole-word store with 3 callers on type-29 records].
- 0x801F68D4 = `return node +28 & 8` [read].
- The host's intercept on it: coop_node_held_ask (hle.c:19727-19747, installed at 28398).

## 2. Who sets bit 8 (the 89 callers of 0x801F6828), and what that means

- **At the door (per ticket):** fn66's clear of the asker at 0x801667A4 (r4 0) [read]. The asker's re-hold at the arrival,
  0x80152190 (r4 1, on r13-21492) [read; log M3 3883].
- **At creation (standing):** the room's placing function 0x8003121C sets it on what it makes:
  - case 1: 0x8003132C, after the actor fields, 0x80033850, +180;
  - the case at 0x80031C48, after allocating the state block from r13-25892;
  - class 3's 0x80031ED4, unless +40 is 80 [read 0x80031220-0x80031EE0].
- The same pattern (allocate a state block, 0x801F64C0, then `0x801F6828(node, 1)`) appears at 0x800348D0, 0x80034930 and
  0x800349DC [read].
- More `r4 = 1` sites: 0x8002A3A0, 0x8002A7E8, 0x8002AF4C, 0x8002B4EC, 0x8002B770, 0x800526EC, 0x800550BC, 0x8005A8C0,
  0x8006B4F8-0x8006E200 (several), 0x8008551C, 0x8008553C, 0x8009B6C0, 0x8009CE58, 0x8009D080, 0x800C99D4, 0x800CBD0C,
  0x800CBD0C's neighbour 0x800CA28C (the argument passed, after 0x8003460C(36, ...) makes a type-36 node), 0x800D0FCC,
  0x800D3E64, 0x800D6668, 0x800DE82C, 0x801D4850, 0x801D5BA4, 0x801D5BF4 [read the list; owners not read one by one].
- **Script and loop sites:**
  - 0x8000A32C: a script function, hold := arg on a node whose object is in the room in play, with actor +158 == 2 and
    +159 == 6.
  - 0x80046738: a loop setting the arg on every child of the player node.
  - 0x8016CEB4, 0x801D5D44, 0x801F9AE8: register arguments [read].
- **Which site made the M3 zombies holdable is not pinned** [not read]. That they were holdable before the ticket and stayed
  holdable after it is [log].
- **So "lift the hold" cannot mean "clear the creature's bit".** That would last past the change, for every later busy
  (words, examines), in solo-like moments too. The hold has to be answered at the ask (0x801F68D4 or 0x80155738), as
  coop_node_held_ask already does for the other player [inferred from the above].

## 3. What event 102 does, by receiver

Found by `cmpwi rN, 102` in the handlers' ranges, then the state compare that branches to it [read each site].
Switch-table cases would not show up this way (a risk, small: the types below use if-chains).

| Receiver (type, handler) | Where | What 102 does |
|---|---|---|
| zombie (4, 0x8003B470) | state 3 (0x8003C550-0x8003C55C -> 0x8003C5B0) | 0x80124788(object) stops the motion; 0x801F64A0(node, 1) (+24 := 1), 0x801F6488(node, 1) (+32 := 1); answers 1 |
| type 37 (0x8003E468) | state 3 -> 0x8003EF48 | the same three calls |
| type 5 (0x8005E4E0) | state 3 -> 0x8005F2FC | the same |
| type 6 (0x800881C8) | state 3 -> 0x80089280 | the same |
| type 45 (0x800913B4) | state 3 -> 0x80091968 | the same |
| type 44 (0x80094530) | state 3 -> 0x80094F24 | the same |
| type 56 (0x800A5B88) | state 3 -> 0x800A718C | the same |
| type 63 (0x800C6588) | state 3 -> 0x800C6E70 | the same |
| type 74 (0x800E0408) | state 3 -> 0x800E0F68 | the same |
| type 89 (0x800E44E0) | state 3 -> 0x800E4D80 | the same |
| type 48 (0x800DCAE8) | state 3 -> 0x800DD060 | an event-1/3 branch of its own (not traced) |
| type 9 (0x8008E414) | state 18 -> 0x8008F720 | a branch on r25 == 1 that sends 57 (not traced) |
| type 28 (0x800661EC) | the event switch at 0x800662EC | 0x8013D6CC(1, 2, 15) (not traced) |
| **type 19, the player held by an enemy (0x80060EC8)** | after the 57 case, 0x80061148 | **event 38 to the holder (+20) and to itself: let go**. The host passes over a 102 whose sender is the other player, unless busy has bit 4 (coop_grab_events hle.c:22296-22307): **so the other's door lets a grabbed stayer go** (F6) |
| **the player (2, 0x80058DD8)** | no `cmpwi r18, 102` among the 74 events its dispatcher compares (0x80057F68-0x8005B970) | nothing found. The busy filter 0x800CA964 does not take 102 (its list is in 4.3), so it reaches the state handlers, none of which compares it [read the compare list; the per-state functions it calls were not each read] |
| type 91, the players' child (0x800DAD74) | no compare | nothing found |
| others | 0x8015BBAC (not a node handler), 0x800ACA64 (a range test) | not node effects |

- **The target is kept.** The chase cut changes no target: M3's `after` column (+COOP_TARGET_B) stays 1/0 through the
  countdown [log 866-1599].
- **Entering state 1 goes through the game's flush:** event 2 to state 3, then event 1 to state 1. The zombie's state 1 has
  no event-1 case, so its default code runs (0x8003B560-0x8003B604) [read]:
  - 0x800648BC(id);
  - if its info +9 has 0x2: event 8 to itself (its room is the room in play, r13-23276) or event 57 (not);
  - if +9 has 0x8: state 33;
  - else state 1.
- **Timers:** 102 itself sets none. The delayed-message queue extends a held node's pending messages while busy (4.1).
- **Animation:** 0x80124788 stops the motion. A creature in any other state (a swing, a hit reaction, rising) is not cut by
  102: it is frozen mid-pose by the motion gate (4.1) and goes on where it stopped once busy is 0 [read gates; inferred
  for a mid-swing].
- **Who receives it:**
  - In a split, the stayer, every creature and object node of the room, the leaver's child node (the sender is skipped,
    not its children: M3 node 9 is in the room), and every node with no object (room -1) [read filter; log "19 nodes told,
    player 1 not, player 2 told"].
  - Apart, the mover's room and the roomless nodes only.
- **Seen on the way (outside the door item):** player 2's own close-up in their copy broadcast 102 three times, at frames
  3700, 3740 and 3770, `from player 2's busy set (busy 0x2) to room 183: 19 nodes told, player 1 told` [log 253, 448, 600].
  Their busy bit is in their copy ("the game's word kept 0"), but the set's broadcast still goes out [log]. So each
  examine of player 2's cuts any chase in the shared room, aimed at either player [inferred from section 3's table]. This
  goes to the both-at-once items, not this one.

## 4. What reads the hold

### 4.1 The twelve callers of 0x801F68D4 (all go through coop_node_held_ask) [read each]

| Site | Gate | Held means |
|---|---|---|
| 0x800074AC in the holder update 0x80007370 | `0x80155738() && held` (0x8000749C) | 0x801F5D48 is skipped: no **event 3 or 4** (the per-frame update) for the node |
| 0x80027774 (holder callback) | `busy && held` (0x80027764) | the same: no 0x801F5D48 |
| 0x80027804 (holder callback) | `busy && held` (0x800277F4) | the same |
| 0x80027200 in 0x800271B4 (the motion step) | `busy && held` (0x800271F0) | 0x80123B70 (pose, no advance) in place of 0x80123698 (advance) |
| 0x800272A8 in 0x8002723C | `busy && held` (0x80027298) | 0x80126514 and 0x8011DFD0 skipped |
| 0x801F5438 in the delayed queue's tick 0x801F53E8 | `busy && held` (0x801F541C) | the message's +20 time += r2-18680 each tick: **it waits** (event 75 waits whatever the receiver) |
| 0x801F55D4 in 0x801F5590 | held (no busy test) | the same postponement, in a second walker |
| 0x8002C7E4 (type 36's handler 0x8002C794) | none | kept in r26 (use not traced) |
| 0x8006705C (in type 31's range) | none | passed as the argument to 0x8017B984 |
| 0x80117E04 | none | 0x80117E58(object, ...) only when held |
| 0x80144688, 0x80144924 (the moves streamer's step) | 0x80144924: held -> 0x801748D8(+136, 0, 8) | not traced |

**The hold of the M3 zombies is exactly rows 1-6:** no update events, posed motion, waiting messages. In state 1 with
busy 4 they stepped 0.0 for 62 frames, and with busy 0 they walked while still marked [log].

### 4.2 The game's direct busy asks (0x80155738, 35 callers): these do not look at the bit

- **Node-gated** (with 0x801F68D4 after): 0x8000749C, 0x800271F0, 0x80027298, 0x80027764, 0x800277F4, 0x801F541C.
- **The player's own:**
  - 0x80058E80, the player handler's busy filter (4.3);
  - 0x800585E0;
  - 0x8005D2E0 (busy -> answers 0; not traced);
  - 0x80061750 and 0x80061A8C, type 19's tick. At 0x80061A8C, if the held one is the current id while busy, it sends
    102 to itself: the solo let-go when the hero becomes busy while held;
  - 0x800644A4, a script function: if the target is the current player, it saves busy, clears it, sends event 8, and
    sets it again, which broadcasts 102.
- **Creatures, hazards, effects asking for themselves** (each "busy -> stop or skip"): 0x8007E140, 0x8008106C (saves and
  clears it), 0x800820B8, 0x800846C8, 0x80086160, 0x80087D90, 0x8008D36C, 0x8008F478 (type 9), 0x800A29B4/0x800A2C20
  (type 50), 0x800CA41C (every 32nd frame), 0x800DFBF0 (type 80), 0x800E4F94 (type 89), 0x800E8918 (type 39).
- **Systems:**
  - 0x80144B98 and 0x80145A0C, the moves streamer;
  - 0x80165570, the script system;
  - 0x8017BF14, 0x80182A60 and 0x80182B18, record steppers;
  - 0x801F22A0, 0x801F248C and 0x801F2514 [read the sites; each body not traced].
- These stop with ANY busy bit, in every room, and know nothing of who is busy [read]. A fix through 0x801F68D4 alone (F5)
  leaves them all stopped. That is the same in solo, where the hero is the only player, but in co-op it is the stayer's
  world too [inferred].

### 4.3 The player's busy filter: why a stayer cannot be hurt during the other's countdown

- 0x80058DD8 (type 2), at 0x80058E80 [read 0x80058E80-0x80058EB4]: `if (0x80155738() != 0)`, then 0x800C3D28(id, msg) and
  0x800CA964(node, msg, answer). A non-zero answer means taken (handler returns 1).
- 0x800C3D28 [read 0x800C3D28-0x800C3DC4]: an event 39 (an enemy's hit) whose attack record has 0x2 and a positive amount
  is answered by **event 201 to the attacker**.
- 0x800CA964 [read 0x800CA964-0x800CAB08] takes 8, 9, 11, 24, 25(?), 30, 32, **39**, 50, 53, 55, 57, 59, 60, 63, 67, 101,
  103, 107, 125, 133 and 202. Rooms 208 and 64 have exceptions for 39 (a type-40 attacker) and 59.
  - The exact ranges were read from the compare chain. The values between 24 and 30 are inferred from its blt/bgt order.
- **102 is not in the list.**
- The busy ask is coop_busy_ask. It does not answer 0 for a room change, so during the other's ticket the stayer's hits
  are taken and dropped [inferred: path read, no run]. In solo this is the hero's protection while walking through a door.
- During player 2's change, player 1's turn runs with the word set to 0 by coop_hold_open (room_turn, hle.c:20322-20345).
  Events from the world update outside that turn, where enemy hits come from, see 4 [read; inferred for the hit's origin].

### 4.4 The host's own readers of the ticket, for the stayer (current line numbers)

| Host line | Who | What waits |
|---|---|---|
| coop_node_held_ask 19727-19747 | both | **not held** for the player who did not ask (answers 0 at every 0x801F68D4 above); logged as "goes on through ... not held" |
| coop_quick_cast_ask 19752-19765 (on 0x800069DC) | player 1, in player 2's change | the quick spell is refused (to 0x80006A80), the press not kept |
| coop_quick_cast(wait) 12463, called at 21070 with `room_other` | player 2, in player 1's change | the quick spell waits, the press not kept |
| 20163-20168 | player 1, in player 2's change | the zone look is refused (answer 0) |
| 20978-20982 | player 2, in player 1's change | the zones are not looked at, g_coop_prompt := -1, so **B starts nothing** |
| 20849-20850 | player 2 | their turn runs through player 1's change (room_other); the game's busy word 4 is in memory during it |
| room_turn 20322-20345 | player 1, in player 2's change | a turn of their own with coop_hold_open (busy 0, the hold channels and words) |
| 26870 (Tome copy mode 2), 27093 | player 2, in player 1's change | **Start opens no Tome**; the press is not kept |
| coop_script_go_to_room 16748-16755 | either | the stayer's own door is passed over while one counts (one ticket record) |
| coop_grab_events 22296-22307 | either, grabbed | the other's ticket 102 (busy 4) **is let through on purpose**, so it lets go (follow-mode reasoning) |
| notify_room_slots_tail 14529-14540 | the stayer | 0x801F6828(stayer, 0) at the tail: the stayer's bit stays 0 after |
| stay_busy_clear 25060-25076 | player 1, in player 2's change | the arrival's all-bits clear keeps player 1's scene or close-up bits (written, with no 102) |

## 5. When the hold ends

1. ticket +0 reaches 0 in 0x80155830 (M3: frame 3882). From then on coop_room_change_on is false
   (`(s32)ticket > 0`, hle.c:11338), so the host's "not held" answer stops for the rest of that frame. The main thread then
   blocks in the load (0x80152398), so nothing of the stayer's runs meanwhile [read; code-readers §2].
2. The arrival 0x80151FDC [read in change-freeze-code-readers.md §2, checked at 0x80152184/0x80152190; log M3 3883]:
   - event 61 directly to the mover;
   - **event 61 broadcast to every node of the room LEFT**, `0x801F5620(61, 0, 0, r13-23288)` at 0x801520DC-0x801520F4. In a
     split that is **the stayer's room**:
     - the zombie drops its route and its id tables (enemy-readers.md: 0x8003B850-0x8003B884);
     - the stayer's motion in states 80/81, a cast or a finisher is cut (stay-readers.md).

     This is not the hold, but it lands at the same moment. No host filter for this broadcast was found
     (grep hle.c for 0x801520DC/0x801520F4 or "event 61": comments only, 14006, 22033, 25019) [read];
   - busy := 0, all bits (0x80152184; no 102);
   - the mover's bit set again (0x80152190).
3. The tail: the host's let-go of the stayer's bit (lr 0x0BADF010). The tail runs in the same frame as the arrival (M3
   3883) [log].
4. The world update that follows sees busy 0. The zombies get events 3 and 4 again, and the chaser cut to state 1 looks for
   a quarry. M3: zombie 40 is in state 3 after player 2 by frame 3884, and zombie 41 by 3896 [log 1661, 1784].

## 6. Both players, both directions, and the stayer's states

| Case | 102 reaches | Stayer's node | Stayer's room's creatures | Stayer's presses |
|---|---|---|---|---|
| S1 split, player 1 leaves (M3) | room 183: player 2, 18 more nodes [log 854] | bit 1 (standing), answered "not held" [log] | held (busy 4 and bit 1) for 62 frames [log]; chasers cut [read] | quick spell waits, B and zones off, Start no Tome [read] |
| S2 split, player 2 leaves (M2) | player 2's room = player 1's: player 1 and the room [read; M2 ran without the rows] | "not held" [log M2 3760, lr 0x80027204]; their turn with the hold open (room_turn) | held, cut [inferred, same mechanism] | quick spell refused, zone look refused; Start: the game opens no Tome in player 2's change (comment 27088-27092) |
| A1/A2 apart -> apart | the mover's room only (0x801F5668) [read] | "not held" (coop_node_held_ask has no room test) [read] | **held, but not cut** [inferred: the busy word and the standing bit; not measured] | the same waits (they key on coop_room_change_other, which has no room test) [read] |
| solo | the hero's room | the hero is the asker (bit cleared by fn66) | held and cut, as designed | - |
| follow mode (no ED_COOP_STAY) | as S1/S2 | not held, brought along | held, then left | as today |

The stayer's states during the other's countdown (S1/S2; apart is the same except the 102 rows):

- **Fighting:**
  - Their own motion advances, since the host says not held.
  - Their hit on a zombie is delivered at once, unless it is a delayed message to a held zombie, which waits (4.1) [read
    queue; whether hits are delayed was not read].
  - A zombie's swing in progress is frozen mid-pose. A zombie's hit on the stayer is swallowed by 0x800CA964 [inferred].
  - A chasing zombie is cut to state 1 in a split, not apart.
- **Casting:**
  - A quick cast is refused for both players [read].
  - A cast already under way runs in the stayer's own handler, which is not held.
  - Spell or effect nodes with no object (room -1) receive 102 in either case. If they are marked holdable, they freeze
    [inferred; which spell nodes carry bit 8 was not read: the 0x801D4850/0x801D5BA4/0x801D5BF4 sites are candidates].
- **In a close-up or words:**
  - Player 2's are in their copy. The game's word has only 4, so nothing of theirs is held [read 15809, the "kept 0" lines].
  - Player 1's set bits in the game's word (8/16/2). Then coop_room_change_on is false: no "not held" answer, and no
    room_turn. They stand anyway in their words. The arrival keeps their bits (stay_busy_clear) [read].
- **In their Tome:**
  - Player 2's copy opened before the ticket stays whole. Start during the countdown is refused [read comment 27089-27091].
  - Player 1's own Tome is the game's menu, so coop_room_change_on is false and the menu stops the world as in solo
    [read 11338].
- **Grabbed:**
  - The other's ticket 102 reaches kind 19, which lets go (event 38 to both), because the host passes busy-4 102s through.
    In a split, the grabbed stayer is freed by the other's door (F6).
  - Apart, 102 does not reach them, so they stay held. But the holder's update is held by busy too [inferred].
- **Dead or dying:** their node is not held, so their death motion runs. What a game-over script does when it fires inside
  the other's countdown was not read.

## 7. What this means for Phase 1b (readings for the design, not a design)

- **F5 at 0x801F68D4 (coop_node_held_ask) is the right ask for rows 1-6 of 4.1**, the game's own gate, and it changes no
  bit. But the rule "a node in the stayer's room, in a split" misses the apart case (item 6 of section 0). The reading
  suggests a single rule for both: **a node is held only if its room is the asker's room and no player stays in it**. The
  stayer's room is never held, whether it is the asker's (split) or another (apart). The asker's room with nobody left in
  it is held as in solo. Roomless nodes (-1) need their own answer: whose effect is it? [open].
- **The stayer's handler must see "not busy" for the other's ticket, or the freed creatures hit an invulnerable stayer.**
  The ask is 0x80155738 from lr 0x80058E84, with the node in r31. Answering `busy & ~4` there, for the non-asker player
  while coop_room_change_other, would keep their hits and the filtered events. The same question applies at 0x800585E0
  and 0x8005D2E0 [inferred].
  - This is the counterpart of coop_node_held_ask for the player's own handler. It must not touch the asker, who is
    protected in their door motion as in solo.
- **The chase cut.** With F5 alone, every chaser in the stayer's room is cut at the ticket, stops, and looks again 8-64
  frames later (enemy-readers: the look's period). That is a visible hitch at the parting.
  - Built for two would be: the busy set's 102 for bit 4 not delivered to the stayer's room in a split, at 0x801F5620 from
    lr 0x80155724 when bit 4 is the new bit.
  - F2' then turns engaged creatures from the leaver to the stayer.
  - The design's §7 rejected answering 102 "as sent" because it would skip "the 102 handlers' let-go". The reading shows
    that let-go is this chase cut itself, plus kind 19's release (which F6 wants to skip anyway) and type 28's
    0x8013D6CC(1, 2, 15), which is untraced. The per-kind effects are in section 3.
- **F6 (grabbed stayer):** confirmed as the only player-side 102 effect found. The host's exception for busy-4 102s
  (22296-22307: `& COOP_BUSY_ROOM) == 0u`) is what lets it through.
- **Event 61 to the room left** reaches the stayer and their creatures at every split arrival. It is not part of the hold,
  but it is the next hitch at the same frame. No host filter was found. doors-readers.md:26 already asked for one ("keep
  the direct 61, withhold the broadcast when someone stays").
- **The direct busy asks of 4.2** stop hazards, spawners, scripts, record steppers and the moves streamer everywhere for
  any busy bit. Whether any of them is in the stayer's view at a door is for a run to show.
- **Clearing a bit is not a fix:** it lasts (section 2), and the host's own tail let-go already leaves the stayer
  unholdable after every split. A later rejoin keeps that until the next arrival of theirs or a game set (14531: "player 1
  coming back into their room held them again") [read the comment; not checked in a run].

## 8. What a run would settle (none were made)

- **H1 (the apart hold):** M5 again with `ED_CHANGE_HOLD=1 ED_COOP_ENEMY_LOG=1`, the stayer standing among the zombies of
  their own room while the other takes a door apart. Expected from this reading: the stayer's room's creatures `hold 1`,
  step 0.0 through the countdown, and no `event 102 ... to room <stayer's>`.
- **H2 (the stayer's hits):** M3 with a zombie engaged and swinging at the stayer through the countdown, with ED_HURT_LOG.
  Expected: no health change for the stayer before the arrival.
- **H3 (the chase cut):** a zombie in state 3 after the stayer when the other leaves (split). Expected: state 1 in the first
  row, and state 3 again only after busy 0.
- **H4 (roomless nodes):** the `[change-hold]` roster leaves out room -1 nodes (hle.c:25478-25480). One row each for them
  (type, +28 & 8) would show which effects and spells freeze.
- **H5 (the bit's maker):** a one-run notify on 0x801F6828 from the level load would name the zombie's creation site
  (section 2). It is not needed for the design.

## 9. Facts read for this report

| Fact | Where | Mark |
|---|---|---|
| busy set: word op, then 102 from r13-21496 to that node's room when non-zero | 0x801556CC-0x80155734 | [read] |
| the ticket writer sets busy 4 after writing the ticket | 0x801557E4-0x801557EC | [read] |
| fn66 clears the asker's hold before the ticket | 0x8016679C-0x801667A4 | [read] |
| the broadcast's filter (room or -1, not the sender) and its per-node send | 0x801F5648-0x801F5694, 0x801F57C0-0x801F5838 | [read] |
| deliver: queue if ahead, the type handler at 0x806068A8, the state-0 fallback, the state flush (events 2, 1) | 0x801F5000-0x801F51D8, 0x801F5294-0x801F5308 | [read] |
| type registrations (2: 0x80058DD8, 4: 0x8003B470, 19: 0x80060EC8, 91: 0x800DAD74, ...) | 0x80030C40-0x80031208 | [read] |
| 0x801F6828 recursion, kind-21 skip, +28 |= 8 / &= ~8; the only such writer | 0x801F6828-0x801F68D0, grep | [read] |
| 0x801F68D4 = +28 & 8; 12 callers and their gates | disasm, the sites in 4.1 | [read] |
| node-update skip (no 0x801F5D48: events 3, 4) while busy and held | 0x8000749C-0x800074CC, 0x80027764-0x80027784, 0x800277F4-0x80027814 | [read] |
| motion pose in place of advance while busy and held | 0x800271F0-0x8002721C | [read] |
| delayed messages postponed for held receivers while busy | 0x801F541C-0x801F5460, 0x801F55D4-0x801F55F4 | [read] |
| creatures' 102 = chase cut in state 3 (10 kinds); kind 19 lets go; type 28 0x8013D6CC | section 3 sites | [read] |
| the player's dispatcher compares no 102; busy filter list | 0x80057F68-0x8005B970 (`cmpwi r18`), 0x800CA964-0x800CAB08, 0x800C3D28 | [read] |
| the player's busy ask before the filter | 0x80058E80-0x80058EB4 | [read] |
| 35 callers of 0x80155738 | disasm grep | [read] |
| arrival: busy -1,0 then re-hold of r13-21492 | 0x80152184, 0x80152190; M3 3883 | [read], [log] |
| bit 8 set on all M3 zombies at the first row and after the change; moving with it set once busy 0 | M3 page-0001.log 866-880, 1686-1701, 1784-2258 | [log] |
| no 0x801F6828 call on any creature in the window; only P1 at 0x80152194 and the host's P2 let-go | M3 page-0001.log 1616, 1620 | [log] |
| busy 0x4 frames 3822-3882, 0 from 3886; stayers' sticks 0 in M1-M3 | both.tsv cols 44, 20-24 | [log] |
| the host answers the stayer "not held" (lr 0x80027778 / 0x80027204) | M1 3822, M2 3760, M3 3821 | [log] |
| player 2's close-up broadcast 102 to the shared room three times | M3 page-0001.log 253, 448, 600 | [log] |
| host waits: quick spells, zones/B, Start, the second ticket, the grab exception, the tail let-go, stay_busy_clear | hle.c:19752-19765, 12463/21070, 20163, 20978-20982, 26870, 27093, 16748, 22296-22307, 14529-14540, 25060-25076 | [read] |
| R6c changes the room and "the player" for a creature's events, not the busy word or the gate | hle.c:25080-25150 | [read] |
