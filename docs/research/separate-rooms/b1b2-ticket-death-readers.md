# B1 and B2: the ticket getter's callers, and deaths in the countdown (readers)

For design-split-hold.md §13 (B1, B2). Read only: no code changed, nothing built, the game not run.
[read] = seen at the address given (build/ed-db/disasm.txt, `ed dis`, `ed fn`, `ed script`). [inferred] = reasoned.
Owner kinds come from the type table 0x80030C40-0x80031208 (0x801F5A9C(fn, type)), decoded again here [read].
"The current player" is r13-21492 (node, 0x801F62B0) and r13-21496 (id, 0x801F62B8); "the main player" is r13-21484
(0x801F6250) and r13-21488 (0x801F6258) [read 0x801F6250-0x801F62BC]. Player 1 holds them unless a host swap puts
player 2's in.

## 0. In short

- The getter 0x80155818 returns the ticket's +0, the countdown [read 0x80155818-0x80155824]. The tick only counts it
  down and never clears it, so "a ticket counts" means +0 > 0 [read 0x80155844-0x80155858; transitions.md].
- Three of the eight callers sit in node kinds that A1 frees: 0x80064644 (creature kinds 4, 5, 6, 7, 18, 28, and 19),
  0x8008D380 (kinds 9 and 73) and 0x800E0664 (kind 74). In each, "a ticket counts" picks the game's own "not now"
  branch: no scene, no grab, no death. Nothing there writes a ticket, sets busy or starts a scene while counting.
- The other five are not node controllers: two world-frame room effects (rooms 190 and 189), the chapter-11 bomb clock,
  an ambient effect process, and a holder callback. A1 does not change them.
- What solo never meets: a killing blow landing while a ticket counts. Kinds 9 and 74 then refuse to die and stay at 1
  health. The room-41 fight scene (script 1150) is not started by that hit.
- New for G2: three callers act on "countdown == 1" exactly. If G2's wait holds +0 at 1, they run every waiting frame.
  The room 190/189 lamps then stay off, and a room's ambient effect process is freed.
- K7: kind 73 reaches fn66 through script 1183 (ServantMkr). Its gate is 0x8008D340, which asks the real busy word
  (0x8008D36C, kept) and the ticket. Add kind 73 to K7.
- Deaths reach no busy set, no fn22 and no ticket in their first frames, with three exceptions. A zombie's death runs
  its own death script at once. A linked node's death asks mode 2. A player's death in chapter 5 room 255 can write a
  ticket at the death move's end.
- Game over: player state 8. At the end of the death move it posts event 17 for 120 frames later, with a 120-tick fade.
  Event 17 then sets game mode 5 (0x80022328(3)). That is at least 120 frames after the move ends, so outside a 62-frame
  window.

## 1. B1: the eight callers of 0x80155818

| Site | Function | Owner | When a ticket counts | Mark |
|---|---|---|---|---|
| 0x80064644 | 0x80064548 | event 234 in kinds 4 (0x8003BE10), 5 (0x8005EC70), 18 (0x8006054C), 19 (0x80060FE8), 28 (0x80066948), 7 (0x80072420), 6 (0x80088400) | skips script 1150 (room 41 only) | [read] |
| 0x8007D1E0 | 0x8007D168 | world frame: 0x800083C8 -> 0x8007CC78 (0x80008478) -> 0x8007CE6C | at +0 == 1: stops room 190's seven effects | [read] |
| 0x8007D49C | 0x8007D464 | the same frame (0x8007CE70) | at +0 == 1: stops room 189's seven effects | [read] |
| 0x8007E29C | 0x8007E124 | timer callback (registered 0x8007B598) of ed56(5), the bomb clock | the clock's expiry waits a frame | [read] |
| 0x8008D380 | 0x8008D340 | "may act" gate for kinds 9 and 73 (six sites) | answers no | [read] |
| 0x800E0664 | 0x800E0408 | kind 74, event 8 (its death) | refuses the death: health 1 | [read] |
| 0x801423E4 | 0x80142328 | a holder callback (+152), installed by 0x80141FBC and at 0x801CD8C8 | skips one value copy | [read] |
| 0x8014E0EC | 0x8014E0C0 | effect process made at arrival (0x80151FDC -> 0x80044748 -> 0x8014DEDC -> 0x8014E030) | at +0 == 1: frees itself | [read] |

### 1.1 0x80064644: event 234 (a creature's attack connected)

- The hit system 0x80198068 sends 234 from the record's +8 to its owner +4 [read 0x80198284, 0x801982EC]. Arg 0 is
  sent on a contact; arg -1 is sent in room 41 only.
- 0x80064548(node, message) returns at once unless arg != -1 [read 0x800645A0-0x800645B0, 0x80064628].
- If the room word r13-23276 is 41 and +0 == 0, it runs script 1150 with the creature's id [read 0x80064634-0x8006465C].
- Then, in either case, it ORs 0x400000 into the creature's data +0, stores the victim's id at +192, and calls
  0x801F654C(+36 node, 0) [read 0x80064660-0x80064678]. If 0x800AA48C (chapter 13's r13-25292, else 0) is set and is
  not the victim, it sends 144 to it [read 0x800AA48C, 0x8006467C-0x8006469C].
- Script 1150 (level 11 only): for tags 2170/2171 and flag 16 clear, it runs hold(1), player_on(0),
  player_on_other(0), hold_scene(1), busy_set(0, 2) and camera FightCam [read `ed script 1150`]. Room 41 holds both tags
  (Npcs11) [read `ed tag 2170`].
- Under A1: a creature in room 41 that hits the stayer during a countdown takes the skip. The fight scene starts on the
  first hit after the firing, because flag 16 is still clear [inferred]. This is benign: it is what G2 wants.
- In solo the creatures stand through a countdown, so a 234 with a ticket live is rare [inferred]. Kind 19 (the held
  player) stays off A1.
- Players: the victim stored at +192 can be either player. Script 1150's holds act on both players [read script].

### 1.2 0x8007D1E0 and 0x8007D49C: rooms 190 and 189

- 0x8007CC78 is called every world frame at 0x80008478, with no busy test before it [read 0x80008474-0x80008478].
- 0x8007D168 (room 190): at +0 == 1 it calls 0x8007D5FC(0) [read 0x8007D1E4-0x8007D1F4]. That frees seven effect slots
  at 0x80323B04+1132 (0x8014D698) and zeroes byte +1167 [read 0x8007D5FC-0x8007D6A8]. Otherwise it remakes missing
  effects (0x8007D6C0 -> 0x8014D4C4, with sound 466 at the node, 0x8007D590). It chooses them by story flags 88-91 and by
  which region of the current slot holds the player (0x80155130(2), +104 & 1) [read].
- 0x8007D464 (room 189) does the same, with all seven [read 0x8007D49C-0x8007D564].
- In any room that is neither 190 nor 189, both stop all seven [read 0x8007D440-0x8007D44C, 0x8007D568-0x8007D574].
- Not a node, so A1 does not touch it. It reads the room word r13-23276, not a player.
- For G2: if the wait holds +0 at 1, the lamps stay off for the whole wait. They are remade on the first frame with
  +0 != 1 and the room word still 190/189 [inferred].

### 1.3 0x8007E29C: the bomb clock (ed56(5), L11/1883)

- ed56(5) makes the text box r13-25452 ("--:--:--", 0x801DA2BC) and the timer r13-25456 with callback 0x8007E124 [read
  0x8007B548-0x8007B59C]. L11/1883 is the only script that calls ed56(5) [read the ed56 uses].
- 0x8007E124 counts 0x80323920+20 down, once per call, but only when not busy (0x8007E140 -> 0x8007E288). It plays
  sound 480 every 10, sound 481 every 60 under 3600, and a stream per whole minute (0x801A4884) [read 0x8007E14C-0x8007E284].
- At +20 == 0: if +0 > 0, or the current player's state (0x801F62B0 -> +20) is 8, it sets +20 := 1 and tries again
  next frame [read 0x8007E29C-0x8007E2C0].
- Otherwise it frees the box, calls 0x800A50D4(0) and plays FMV 64 (0x8004F4D0(2, 64, 1, 0, 0), ed83's engine). It then
  calls 0x8013D6CC(1, 0, 60), (1, 1, 120) and 0x8013DB1C [read 0x8007E2C4-0x8007E318].
- The ticket test is on both paths: the busy path joins at 0x8007E288 [read]. (refute-split-hold-goal.md:71 says busy
  path only.)
- Not a node. The busy ask 0x8007E140 keeps the real word (§3.3).
- Players: only the current player's death holds the expiry. Player 2 dying does not, and FMV 64 then plays for both
  [read 0x8007E2A8; effect inferred].

### 1.4 0x8008D380: 0x8008D340, "may act" for kinds 9 and 73

- It returns 1 only when all of these hold [read 0x8008D340-0x8008D3F0]:
  - its own state is not 8;
  - the current player's state is not 8;
  - no cast is up, or 0x801CDB2C(r13-21496) passes;
  - the busy word is 0 (0x8008D36C, the real word);
  - mode asked r13-26188 == 2;
  - +0 <= 0;
  - the room is not 83.
- Its callers [read]:
  - Kind 9's event 8, the death (0x8008E8AC -> 0x80089EDC, site 0x80089F4C). On "no" it sets its own health to 1
    (0x800354F8(node, 0, 1, 0)) and does not die. On "yes" it builds a hit on the player id r13-21496 of -50/-75/-100
    by difficulty r13-26372, sends 39 to the player and takes state 102 [read 0x80089F58-0x8008A00C].
  - Kind 9's states at 0x8008F4E8 and 0x8008FB48: the branch (0x800DC63C(1), 129 to itself) is skipped on "no" [read].
  - Kind 73's 0x8008D648 (from 0x8008DB40). On "no" it sends itself 23 (cancel, then 0x801F57C0 delayed) [read
    0x8008D71C-0x8008D74C, 0x8008D7F0-0x8008D820]. On "yes" it calls 0x8008CCD8 or 0x8008D140.
  - 0x8008CCD8 ends with script 1183, whatever the gate answers: the result at 0x8008D118 is unused [read
    0x8008D114-0x8008D128]. Script 1183 (L09, L16) is the servant's grab: fn22(1, 0), then fn66(L0, ServantMkr, ServantCam,
    3, 1183, 2, 0) [read `ed script 1183`].
- So kind 73 reaches a room ticket. It is gated by the real busy word and the ticket, so it cannot start in the window
  [read]. It still belongs in K7 by the design's rule.
- Players: kind 9's death hits r13-21496, player 1 outside host swaps, whoever killed it [read 0x80089EFC, 0x80089FE0].
- Under A1: a kind 9 the stayer kills in the window refuses to die and stays at 1 health. It dies on the next blow after
  the firing [inferred].

### 1.5 0x800E0664: kind 74's death (chr 132, L7 room 104)

- Event 8 with +0 > 0 goes to 0x800E0870: health := 1 (0x800354F8(self, 0, 1, 0)) and 0x8003505C(r23, 1, 1) [read].
- The same refusal happens when the player id's node (0x801F62B8) is missing, or is not a hero (+159 != 1, 0x80033968)
  [read 0x800E0670-0x800E069C, 0x800E0848].
- Otherwise it plays move 24 (and on a linked +92 node too) and sets health 0 (0x800354F8(self, 0, 0, 1)). Then:
  - if +140 -> +334 == 0: event 25 delayed 240.0, state 9;
  - else: music_set(7, 5), event 17 delayed 330.0, event 249 (arg 74), state 8 [read 0x800E06A0-0x800E07A8; r2
    constants from generated/main.dol].
- Kind 74's case 39 calls the taker 0x80061B9C only when r13-21496's node is a hero [read 0x800E0620-0x800E0650].
- Under A1: the stayer's killing blow in the window leaves kind 74 at 1 health. Solo never meets this [inferred].

### 1.6 0x801423E4: 0x80142328, a holder callback

- 0x80141FBC stores 0x80142328 at +152 of a holder record, with two sibling methods at +144 and +148 [read
  0x80141FBC-0x80141FFC]. Its callers: kind 7 (0x800727F0 -> 0x8014D154), creature effect makers (0x800CDF6C,
  0x800D2280, 0x800D7328) and 0x801CE5B8 [read]. 0x801CD8C8 also sets it with 0x8014F86C [read].
- For each pair of the two holders' handles (+136), unless room 83 or +0 > 0, it copies a byte read with 0x80174930
  onto the other with 0x8017949C [read 0x801423D8-0x80142454]. What the byte is: not read.
- Who runs +152, and whether that is gated by busy: not read.
- Under A1, if it runs in a freed node's step, the copy waits until the firing. That could show as one attached part
  standing still for the window [inferred].

### 1.7 0x8014E0EC: 0x8014E0C0, a room's ambient effect process

- Made at arrival: 0x80151FDC (0x80152118) -> 0x80044748 -> 0x8014DEDC -> 0x8014E030. It goes in the process pool
  (0x801432BC) with type byte +4904 = 132 and room +4900 [read 0x8014E030-0x8014E06C].
- Each step: if its room is not the room word, or +0 == 1, it frees itself (0x8014334C) [read 0x8014E0DC-0x8014E118].
  Otherwise, on a random timer (rand & 0x7FF + 60), it starts an effect at a random point of its list (0x8014DC8C)
  [read 0x8014E16C-0x8014E1F8].
- The runner 0x80144B34 (world frame 0x80008488) runs type 0x84 through busy (bit 8 is clear) [read 0x80144B60-0x80144BAC].
  Not a node.
- In a split, the shared room's process is freed at +0 == 1 and is only remade by an arrival. The stayer's room keeps
  no ambient effects until someone enters it again [inferred]. That is true today, without A1. A G2 wait at 1 frees it
  too.

## 2. B2: deaths

### 2.1 Who sends event 8

- The meter setter 0x80061E90: when health (index 0) drops to <= 0 and the node is not marked deathless
  (0x801F6920, +36 & 0x800000), it sends 8 from the attacker to the victim's own id [read 0x80062180-0x800621B8].
  It is synchronous, inside the hit. A value above 0 sends 53 (hurt) instead [read 0x8006214C-0x8006215C].
- The taker 0x80061B9C, at 0x80061C88-0x80061C90: an instant 8, from the victim to itself. It needs hit flag bit 0, the
  victim to be r13-21496 (player 1 outside swaps), and 0x8006A3A8(0x801F8FD8(node list, 32), 0x10000) [read; "the kind-32 sanity tracker" inferred].
- Both are behind the taker's room test: the victim's +592 must be r13-23276 or -1 [read 0x80061BCC-0x80061BE0].
- While busy, the player filter 0x800CA964 swallows 8 (transitions.md). Under A3 the stayer's 8 gets through. A player
  death during a countdown is therefore new.

### 2.2 A player's death (event 8 to player 1's or player 2's node)

Frame 0, state 0's event 8 (0x80059F64) [read 0x80059F64-0x8005A0C8]:
- If 0x8009E218() is set, it sends 85 to itself and does not die. 0x8009E218 is set when the node found by 0x801F8FD8(list, 32) (the kind-32 tracker [inferred]) has an effect in
  one of a set (jump table 0x80245470), when 0x8006DB04(0x20000) holds, or in chapter 5 room 255 with flag 883 [read
  0x8009E218-0x8009E304]. "A fake death" is [inferred].
- In room 96 nothing happens [read 0x80059F90-0x80059F98].
- Otherwise it plays move 24 with callbacks for events 6 and 7 (0x801F8FA4). It may drop what it holds (0x80046170,
  0x80046254, 0x80038A04) and takes state 8. If chapter 13's r13-25292 is set, it sends 195 (arg 10005) to it.
- States 78 and 82 swallow 8 and state 127 sets health 1 instead; state 121 (the door motion) does not take it
  [read 0x8005ADD0, 0x8005B570, 0x8005B724].

State 8 (0x8005B924 -> 0x8005855C) [read 0x8005855C-0x80058958]:
- Event 1 (the same frame): 0x80018808(2) (mode asked := 2), 0x801E4610(0), 0x80129878, 0x801295F4, 0x801172B0,
  0x80117260(64, 1) (prompt flag 64), the busy ask (answer unused, 0x800585E0) and 0x80121B98(-1). It also fades the
  music (0x801A16A0, 0x801A19D0(600)), except for models 85 and 119-122, and when 0x8009E218 is set.
- Event 6 (move 24 ends):
  - If 0x8009E218 is set: 0x800C04EC(0, id). In room 255, chapter 5, flag 883, that writes a ticket to room 252
    (script 2779). Then state 1.
  - Else if the object's model (+580) is 85 or 119-122: health refilled, sound 541, move 41, state 1. 85 is chapter 3's
    hero (kChapterModel, hle.c:7432).
  - Else: event 17 posted for 120.0 frames later, and a 120-tick fade to 0x000000FF (0x801E7C88).
- Event 17:
  - In chapter 13 (0x80307178 == 13), if r13-25292's node is not in state 8: a 1-tick fade, then play_cinematic
    (0x8004F374) of an entry chosen by r13-26320.
  - Else: a 120-tick fade, 0x80042974(0, 0) (fn22 off: busy_set(32, 0), play hold off, zones on), then 0x80022328(3)
    (r13-25992 := 3, mode asked := 5).

What decides the game over: event 6 in state 8, falling past the 0x8009E218 test and the model test, then event 17 [read].
It comes at the end of move 24 plus 120 frames. Move 24's length: not read. Mode 5 leading to room 69 and script 1500
is from the earlier sighting (coop-no-pause.md:75), not read here.

In the first ~62 frames, then:
- no busy set, no fn22, no play_cinematic, no fn66, no 0x80152298;
- prompt flag 64 is set and mode 2 is asked in frame 0;
- the only ticket is 0x800C04EC's, and only if move 24 ends inside the window in chapter 5 room 255. G1 would refuse it,
  leaving the player standing in 255 [inferred].
- The busy_set(32, 0) of event 17 comes later (at least 120 frames after event 6), outside the window. If a ticket were
  live then, its 102 broadcast would follow, because bit 4 stays set (§1.1 of the design) [inferred].

Player 1 against player 2:
- Event 8 goes to the victim's own id, so it reaches either node [read 0x800621A8].
- 0x8009E218 reads the shared kind-32 tracker and the room word [read]. The model is the dying object's own [read].
- 0x80018808(2), 0x80117260(64, 1), the fades and 0x80022328(3) are global. A game over from either player's death ends
  it for both (enemy-readers.md (b)).
- The bomb clock (§1.3) and 0x8008D340 (§1.4) look only at the current player's state 8.

### 2.3 A zombie's death (kind 4; event 8 -> 0x80038E54; state 8)

0x80038E54, in the hit, frame 0 [read 0x80038E54-0x800391E8]:
- If the zombie's +592 is not the room word: op 2 and type 0, so it stays inert and standing. In the apart shape this
  bites unless the host brackets the room word (enemy-readers-2.md §0).
- If flag 0x80 is set: 0x800391EC.
- The rise path (move 43, health 1): 0x80064C10, 0x80064C84, 0x80064F1C, 0x800650D8, the death script (below), then
  type 28.
- The normal path: move 24, health 0, event 17 posted for 1000 frames later (120 in rooms 409 and 251), state 8.

State 8, event 1 (0x8003DFB0-0x8003E00C), the same frame [read]:
- 0x80118BEC(obj, 192), 0x800BC6B8, 0x800CA6F4(node, 1, 0).
- 0x800DE760(node, 2 * 0x800C7FC8(sub-kind), 0). If the actor record (+140) holds a linked id at +108, it:
  - sends that node event 8;
  - sets 0x01000000 on it (0x801F65C4);
  - asks mode 2 (0x80018808(2)) and calls 0x801E4610(0);
  - sets the hold bit (0x801F6828);
  - clears +108.
  [read 0x800DE760-0x800DE868]. What links there (a possession, a rider): not read.
- 0x800C8154(node): the death script.
- 0x801F9770.

What a zombie death sends to scripts:
- Its own death script: the actor's +140 record +328, if not 0 or -1. It runs once, guarded by node flag 0x20, through
  0x801642D4(script, zombie id, 0), synchronously, in frame 0 [read 0x800C8154-0x800C8200].
- No story flag set and no counter written on the death path itself. A bl walk 4 deep from 0x80038E54, 0x800DE760 and
  the state-8 callees found no flag set, busy set, fn22, play_cinematic, fn66, ticket or room load. The only reaches
  were 0x800C8154's script run and 0x800DE760's mode ask [read, scratch reach.py]. Events and table calls were not
  followed.
- So any flag, counter, drop or scene after a kill comes from that death script, or from a room script that polls on its
  own. Which placings carry a +328 script: not read.
- Later events, all after the window: 17 at 1000 (or 120) frames gives sound 0x800CD4B0 and type 21. Event 61 (the
  arrival's, to the old room) gives 57 (inert) [read 0x8003E058-0x8003E178].

## 3. What this means for the design

1. B1 needs no new guard. Every getter caller in an A1-freed node takes the game's own "not now" branch while +0 > 0.
2. Add kind 73 to K7: it reaches fn66 through script 1183 (§1.4).
3. G2 should not hold +0 at exactly 1. Three readers treat 1 as "the change is next" and stop or free room effects
   (§1.2, §1.7). Hold it at 2 or more, or let it reach 1 only on the frame before the firing [inferred].
4. Deaths in the window: watch for a zombie's death script (§2.3) and a linked node's mode ask (0x800DE760). G1 and G2
   cover what a death script can start: a ticket, or a scene that turns coop_room_change_on off.
5. A stayer's death in the window is new (A3), but its game over comes later than the window. Only chapter 5 room 255's
   ticket can fall inside, and G1 refusing it strands the player [inferred, rare].
6. Kinds 9 and 74 stay at 1 health when killed inside the window. To check in K1k if those rooms are tested.

## 4. Facts

| Fact | Where | Mark |
|---|---|---|
| getter returns ticket +0 | 0x80155818-0x80155824 | [read] |
| type table | 0x80030C40-0x80031208 | [read, decoded] |
| 234 sender, args 0 / -1 (room 41) | 0x80198284, 0x801982DC-0x801982F8 | [read] |
| room 41 and ticket 0 -> script 1150 | 0x80064634-0x8006465C | [read] |
| world frame calls 0x8007CC78, no busy test | 0x80008474-0x80008478 | [read] |
| rooms 190/189: +0 == 1 stops seven effects | 0x8007D1E0-0x8007D1F4, 0x8007D49C-0x8007D4B0, 0x8007D5FC | [read] |
| bomb clock: ed56(5), the waits, FMV 64 | 0x8007B548-0x8007B59C, 0x8007E124-0x8007E36C | [read] |
| 0x8008D340's conditions | 0x8008D340-0x8008D3F0 | [read] |
| kind 9's death: refuse, or a 39 to r13-21496 | 0x8008E8AC-0x8008E8D0, 0x80089EDC-0x8008A00C | [read] |
| kind 73 -> 0x8008CCD8 -> script 1183 -> fn66 | 0x8008D648, 0x8008D114-0x8008D128, `ed script 1183` | [read] |
| kind 74's death refused while counting | 0x800E065C-0x800E0898 | [read] |
| ambient process freed at +0 == 1 | 0x8014E0C0-0x8014E118 | [read] |
| health <= 0 -> event 8 to the victim | 0x80062180-0x800621B8 | [read] |
| player state 0's event 8, state 8 | 0x80059F64-0x8005A0C8, 0x8005B924, 0x8005855C-0x80058958 | [read] |
| 120.0 at r2-30752 and r2-31472; 1000.0 at r2-31468; 240.0 and 330.0 at r2-26348 and r2-26344 | generated/main.dol | [read] |
| 0x80022328(3): r13-25992 := 3, mode asked := 5 | 0x80022328-0x8002234C, 0x80018808 | [read] |
| zombie death; death script once | 0x80038E54-0x800391E8, 0x8003DFB0-0x8003E00C, 0x800C8154-0x800C8200 | [read] |
| linked node's 8 and mode ask | 0x800DE760-0x800DE868 | [read] |
