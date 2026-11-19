# 22 Walk on Ceiling for player 2 alone, and where to test it (read 2026-11-18, nothing run)

Read from the disassembly (`ed dis`), the disc's rooms (`ed room`, `ed door`, `ed script`) and `runtime/host/hle.c`.
Read with `plan.md` and `return-spec.md` beside it. Every statement is marked READ (from the code or the disc) or
GUESS. `hle.c` was being edited while this was read: its line numbers are "about", the function names are exact.

Corrections to what was noted before:
- The ticket is written by the `bl` at 0x8009B808 (lr 0x8009B80C). VISetBlack(1) is the `bl 0x801E4610` at
  **0x8009B810** (lr 0x8009B814), and VISetBlack(0) the one at 0x8009B6AC (lr 0x8009B6B0). READ.
- The rules 0x8006EB6C answer **yes to 22 at any sanity level** (its case is `li r30, 1` at 0x8006EBE8). The level 3
  test is in the door's own ask, 0x80068770. READ.
- An event "for later" (0x801F57C0) counts in **game frames**, not seconds: its due time is f1 + 0x801F5350(), which
  is the word r13-22216 as a float, and 0x801DC9F8 adds 1 to that word each frame. READ. 22's own 81 is sent with
  f1 = 50.0 (r2-28584 = 0x806214B8). `sanity_later` (about line 28033) takes f1 for seconds when it is under 20 and
  for 1 frame otherwise: Head's 81 (1.0) came 61 frames late in `logs/run-20261118-184436-jtg-sanity2-head`, and
  22's would come after 1 frame, not 50.
- 22's record flags are 0x102 (0x80068684, table 0x80243EB0[22] -> 0x800686B8 `li r3, 258`): door-started (2), not
  kept out of the roll (8 clear), no enemies moved away (0x10 clear), not ended by event 82 (0x20 clear). READ.

## 1. The sequence

One tick of an effect's timer is one update of the tracker (the record's +20 gains r13-25484 = 0x10000 each update,
0x80069A24; the steps compare its high half). Player 2's update runs once a frame, so a tick is a frame. READ.

| Step | Address | Runs in | Acts on, reads / writes |
|---|---|---|---|
| The door asks for 22 by itself | 0x8004305C(next room) at 0x80152640 -> 0x80068770(holder, room) at 0x80043144 | the runner, before the load: player 2's door bracket | True when: holder+21 (level) == 3; the room is in list 1 (0x800689D0(room, 1) gives its index 0-9: 30, 105, 151, 161, 167, 243, 248, 253, 255, 257, table 0x80243D00); bit `1 << index` of the data's +28 is clear (once a room, until the set is zeroed); and, for room 255 alone, flag 883 clear. No chance is rolled. Only reached when nothing blocks: 0x80043218(0) is 0 (no effect running that is in table 0x8023EEFC) and 12's ask 0x80068868 said no. READ |
| ...then | 0x8004314C-0x800431B0 | the same | gate 0x8006DD70(0); event 82 to the tracker (drops what waits); the data's +56 = 22; event 83 -> 0x8006A10C: rules 0x8006EB6C(22, holder+21, holder+56) and holder+4 == 0, then armed (stage 5). The tracker is found by the CURRENT PLAYER's info+140 -> +68 (0x801F62B8 = r13-21496). READ |
| The place is kept | 0x8006BE04(player node, tracker node) at 0x800431C8 | the same | the data's +72 record: room r13-23276, place, angle, four stats of the current player; cameras 0x801EF2EC(0x803196C8); r13-25496 = 1. READ |
| The door starts it | 0x80043380(22, room, marker, camera) at 0x80152748, case 0x8004345C | the runner, after the load | list 1 again (r28), the room's own list not asked (r29 = 0); 0x8009E738(22) = 0, gate 0x8006DD70(0); 0x8006C008(holder, room, marker, camera): 0x80319578 zeroed, holder+56's things stamped with the room, the data's +76 room, +88 marker, +92 camera; event 84 at once. READ |
| Setup | 0x8009B520(holder) (start table 0x80243D8C[22]) | event 84's handler, in the bracket | The record's runner (+28) = 0x8006BBC4 (the looping stepper), its counter (+8, s16) = 0, timer 0. Step slots of the holder: [0] = 0x8009B760 at 0 ticks, [1][2][3] = 0x8009B640 at 30 ticks each, loop from 1 to 3 (holder+106 = 1, +107 = 4). Name "Ins: Walk On Ceiling" to holder+24. The holder and the record only. READ |
| Step 0 | 0x8009B760(holder), their first update after the door | `sanity_p2_step` (their update) | 0x800689D0(data+76, 1) and the gate 0x8006DD70(0) again; fails -> 0x8009B844. Else: the ticket 0x80155770(data+76 + 1000, marker data+88, camera data+92, script 0, flags 0, countdown 3, fade count 2, transition 0; no callback; +24 = 1) at 0x8009B808; VISetBlack(1) at 0x8009B810; the data's +28 gets the room's bit; 0x801F6828(tracker node, 0) (the tracker's "held while busy" bit, node+28's 8, cleared); 0x80199EA0(0) (play hold, busy 2). The tracker node is found through holder+56's character's link (0x8006A4EC). READ |
| The tick | 0x80155830 | the frame, outside any turn | counts +0 down 3, 2, 1; at the second tick the fade starts (0x801E7C88, +4 reached 0); at 0, transition 0 is not 17: the RUNNER 0x80152614(room + 1000, marker, camera, 0, 0) at 0x801558F4. READ |
| The runner | 0x80152614 | `notify_ticket_run`'s bracket, as player 2's change (section 3) | 0x8004305C(room + 1000): nothing waits (the record's stage is 4, not 5), 22 running makes 0x80043218 skip the asks: returns -1. The load 0x80152298; r13-26524's object (theirs in the bracket) stamped with the room (ticket +24) and put at the marker; 0x80043380(-1, ...) does nothing; the camera data+92. READ |
| Steps 1-3, looping | 0x8009B640(holder), every 30 ticks | their update | At its first call (counter 0): VISetBlack(0) at 0x8009B6AC, 0x80199EA0(1) (let go), 0x801F6828(tracker node, 1). Counter + 1. At 50: the three steps emptied, event 81 for later (50 frames) to holder+56's character's link, 0x801F6828(tracker node, 0), 0x80199EA0(0) (held again), returns 1. Acts on holder+56's character only. READ |
| The early end | state 42's event 3, 0x8006D61C-0x8006D694 | their update | Only for 22: when the counter is over 1 (60 ticks in), the SHOWN prompt record 0x80335AE0 has button 2 (B; its +4, 0x801179AC) and id 1, 3 or 4 (OPEN DOOR, CLIMB UP, CLIMB DOWN; its +8, 0x801179CC), and pad 0's pressed word has B (0x8013DC24(512, 0, 0): 0x805880D0 +4): event 81 at once. Pressing a way out of the ceiling room ends it. READ |
| It ends itself | 0x8009B844 | their update | prints "Ins_SwitchToCeilRoom_Action() Cancelling upside down ceiling room %d" (0x802452BC), empties the four steps, the runner = 0x8006BAB0 (one shot). No ticket, no black. READ |
| The return | event 81 -> 0x8006C850 -> flash -> 0x8006BFAC: ticket (data+72, transition 17), callback 0x8006B45C | their update, then the frame | as return-spec.md section 1, already bracketed (`g_sanity_p2_return`, `sanity_p2_return_open`). READ |

Timeline in frames from step 0 (T): T ticket, black, hold; T+3 the ceiling room loads; T+30 un-black, let go;
T+60 the early end becomes possible; T+1500 held, 81 sent for 50 frames on; T+1550 the flash; about T+1560 the return
ticket; T+1561 back in the room before the door, at the kept place. READ (counts), GUESS (the load takes no ticks).

The gate 0x8006DD70(0), asked three times, refuses when: chapter word 0x80307178 == 5 with flag 883 set; 0x80157364()
or r13-25456 (0x8007AA64) not 0; the CURRENT PLAYER's info+158 == 1 with +159 of 3, 4 or 6; the scene hold r13-25696
(0x8004303C) not 0; a spell being cast r13-22336 (0x801C5260); r13-23276 in list 5 (5, 39, 83, 157, 185, 191, 213,
292). READ. r13-25696 is one word for both players: player 1's scene at step 0 cancels player 2's 22 (0x8009B844).

What these reach that is NOT in `SanityOwn` (`sanity_set_io`, `kSanityWords`):

| Where | What | Decision |
|---|---|---|
| r13-21880 (0x8061FEA8) | the game's kept black flag; read and written only inside 0x801E4610 | not a set word: section 4 |
| the tracker node's +28, bit 8 | "held while busy", cleared and set by 0x801F6828 from both steps | leave: one node for both sets; while player 2's 22 has it clear, player 1's tracker runs through a busy. GUESS that this is harmless |
| 0x80335AE0 (+4, +8) | the shown prompt record (slot 6 of `PROMPT_SLOTS` 0x80335A38) | in their update it is player 1's: section 3, item 6 |
| 0x805880D0 (+4) | pad 0's pressed buttons | theirs in their update: it is inside `CoopPlayerState.input` (0x805880A0, 816 bytes). READ |
| 0x8058AFFC, r13-23392, r13-23344 bit 4, 0x800443E4's locks | the ticket | the game's one ticket: section 3 |
| the data's +28 | the ten rooms' "done" bits | in the set (`ext`), but player 2's set is copied from player 1's when made and `memset(s->ext + 12, 0, 16)` (about line 28327) stops at +27: clear +28..+31 too |
| r13-25484 | the tick's size, 0x10000, written once at 0x80068378 | read only: skip |

Everything else they touch is the holder, the table, the data (`ext`), 0x80319578, 0x803196C8 and r13-25496: in the
set already. READ.

## 2. The room + 1000

- **It is a real room file.** `ed room 1105` reads `Rooms/rmed1105.bpe` from the disc; the ten exist (1030, 1105,
  1151, 1161, 1167, 1243, 1248, 1253, 1255, 1257: `build/ed-db/lights/`). The name's format is "%s%04d.bpe"
  (0x80253028). Each is its own geometry with its own cameras, the same marker and camera NAMES as its room (so the
  door's marker and camera hashes land), and the room's door regions with the same scripts: 1105's Region01 runs
  L07/1249 to room 109, 1257's Region02 runs L05/912 to room 256. READ. A player can walk out of a ceiling room by
  its door. GUESS: the picture is upside down by the room's build, not by the camera.
- **The loader takes no notice of the number.** 0x80152298 -> 0x80154C94(room, ...) keeps it as a 32-bit word
  (r13-23276), as an s16 in the slot record (+0x8140, 0x80154DE4) and in the read cache (0x8058AF60, 24 bytes a
  room, searched by number at 0x80151CE0). No flag, no branch on 1000. READ. One test on size: r13-23284 is set to
  the new room only when it is under 0x801DC1C8(r13-23328) (0x80154CE8-0x80154CFC), so a ceiling room is never the
  "last real room". READ (the test), GUESS (what r13-23284 is for).

The host, by number: no table in `hle.c` or the other host files is indexed by room, and no test compares a room with
a size, except these. READ (greps for arrays, bitsets, `room <`, narrow casts).

| Where | What it does with a room of 1000 or more | Wrong? |
|---|---|---|
| `sanity_room_list` (about line 27693) | `room >= 512u`: returns, nothing logged | no: solo's log only |
| `room_slot_put_back` (about 16178), `room_slots_log_table` | the slot record's room as an s16 | no: 1257 fits |
| `SanityDoor`, `kSanityDoors` (`sanity_doors.inc`) | s16; it lists doors OUT of 1255 | no: solo's door tool |
| `coop_object_room`, `room_slot_of`, `coop_stay_apart`, `coop_room_in`, `room_slots_return`, `notify_room_slots_tail`, `effect_draw_by_room`, the lights' grid (about 6563), the sound tracks' rooms | compare rooms for equal, s32 | no |

The host, by meaning (all READ in the host code; what follows from them is GUESS until run):

| Where | What happens with player 2 in room N + 1000 |
|---|---|
| The two slots | Their slot holds N + 1000 in place of N. With player 1 in N, the two are "apart" (`coop_stay_apart`: the numbers differ): each half draws its own room, and neither sees the other. With both in N at the ticket, N + 1000 is unpacked into the other slot and N stays player 1's (the bracket's own rule). |
| `sanity_p2_step`, "came into another room" (about line 28415) | After their own change `g_sanity_p2_door_began` is false (the tracker was at 42 when the bracket opened: `sanity_p2_door`, about 28264), so the next update sends event 82 and prints "ended as a door ends one". For 22 the game ignores it (0x8006E00C returns 0: flag 0x20 clear). A false line, no harm. The same when they walk out by the ceiling room's door. |
| A door out of the ceiling room | An ordinary door of player 2's from N + 1000. The effect stays on until the count of 50, then "returns" them to data+72 (section 3, item 6 is why the game's own end at that press is missed). |
| `tome_trip_note`, a save, a save state, a death, with player 2 in N + 1000 | not checked |

## 3. The effect's own ticket, written in their update

What happens today, in order (READ unless marked):

1. 22 cannot start for player 2 at all: `sanity_rules` answers 0 to anything outside `kSanityP2` while
   `g_sanity_p2_in` is set, and that is set in their update AND in their door bracket (`sanity_p2_door`). The door's
   own event 83 is refused there.
2. With 22 allowed: step 0 runs inside `sanity_p2_step` with `coop_be_player2` in, so r13-21496 is player 2's id.
   `split_ticket_ask` (the intercept on 0x80155770) refuses only while another ticket counts with a window on
   (`split_counting`); otherwise it passes. `notify_room_ticket` then sets `g_coop_room_p2` (r13-21496 == their
   id), `g_fade_mover = 2`, and `g_sanity_p2_return` stays false (lr is 0x8009B80C, not 0x8006BFF8).
   `split_ticket_open` opens player 1's stayer window.
3. `stayer_lock` (the intercept on 0x800443E4, lr 0x801557F8) takes the ticket for one written with player 1's input
   copy in (`g_coop_turn` is false in the sanity step) and sets only the Start lock. Their own copy gets no binding
   lock. They are under the play hold a few instructions later, so nothing shows. The return ticket of 32 and 17
   already goes this way.
4. Busy: bit 4 stays in the game's word, bit 2 (the hold) goes to `g_sanity_p2_busy`. As built.
5. The three frames of the countdown: `sanity_p2_step`'s guard (about line 28307) has `g_stay_bracket`,
   `g_sanity_p2_door`, `g_sanity_p2_return`: none is set, so their update runs on. The stepper only counts (1, 2,
   3 of 30). Harmless here.
6. The tick calls the runner; `notify_ticket_run` -> `stay_bracket_open` opens (stay mode, `g_coop_room_p2`, no
   bracket on, they have a room): player 2 the player, their room word, their cameras, their sanity set
   (`sanity_p2_door(true)`). The load, the placing and the camera are theirs. `coop_stay_change_end` closes it in
   the same frame. **The existing bracket takes it: nothing to add for the change itself.**
7. VISetBlack(1) blanks the whole window for 30 frames (section 4).

What must change is in "What to build". The one real fault in this part is the refusal: if player 1's door is
counting when step 0 runs, the ticket is refused, and 0x8009B760 still blacks the screen, holds them, marks the room
done and counts 1500 ticks to a "return" from a room they never left. A guard on a counting ticket stops that, and it
is what the game does alone (a ticket's hold stops the tracker's update: its node has bit 8 of +28 set). The same
guard keeps their return ticket (0x8006BFAC) from being refused.

## 4. VISetBlack(1)

- 0x801E4610(on): old = r13-21880; VISetBlack(on) (0x8020C648); r13-21880 = on; returns old. 36 callers. READ.
- `notify_VISetBlack` (about line 3861, a notify on VISetBlack) calls `dol_platform_set_xfb_black`, which ends in
  `aurora::vi::set_xfb_black` (`.deps/GXRuntime/graphics/aurora/lib/dolphin/vi/vi.cpp`); `end_frame`
  (`aurora.cpp`, `viBlack`) then presents black for the whole window. **Both halves.** READ. Player 1's own 22 does
  this in co-op today.
- The smallest change that keeps it to their half: an intercept on **0x801E4610** (it has no dispatch entry today:
  add it to `kOwn[]` in `sanity_tool_install`). When `g_sanity_p2_in` and lr is 0x8009B814 (on) or 0x8009B6B0 (off):
  set or clear a new `g_sanity_p2_black`, answer r13-21880 unchanged, and do not call the game's. Nothing else reads
  r13-21880, so nothing is left behind. Any other call passes through.
- Drawing it: in `sanity_overlay`, in player 2's part, when `g_sanity_p2_black`: the two half-height black rectangles
  the TV Video screen already draws there (0x8019CFD4(5), then 0x8019C8FC twice: about lines 28078-28088), no
  words. That is the black half already seen for TV Video.
- Also clear `g_sanity_p2_black` in `sanity_p2_step` whenever their tracker is not at 42 with effect 22, so a 22 that
  ends another way cannot leave their half black.
- Or skip it for them (the intercept alone, no drawing): they then see the door's fade and the cut. GUESS that the
  black only hides the first half second in the new room.

## 5. Where to test

The doors into the ten rooms (READ, `ed door`):

| Level (Jump to Game) | Doors |
|---|---|
| 3 Anthony (3) | 152 -> 151 (L03/896), 153 -> 151 (L03/1139) |
| 5 Max (5) | 256 -> 257 (L05/899); 255 -> 253 (L05/283), 258 -> 253 (L05/640); 252, 253, 254, 259 -> 255 (L05/580, 563, 648, 649, 62) |
| 6 Lindsey (6) | 306, 322 -> 161 (L06/1486, 1477); 144, 296, 298 -> 167 (L06/1024, 1474, 1027) |
| 7 Paul (7) | 109 -> 105 (L07/1341) |
| 9 Edward (10) | 241, 242, 245, 247 -> 243 (L09/885, 237, 996, 909); 247 -> 248 (L09/910) |
| 11 Michael (11) | 32, 58 -> 30 (L11/384, 527: CLIMB DOWN, CLIMB UP) |

The earlier note is right, and levels 0/12, 1, 2, 4, 8, 10 have none. READ.

From each chapter's start (`ed level`), with no story progress. The number is the Jump to Game chapter. Chains and
scripts READ; "against it" is GUESS where it says what a run would do:

| Jump to Game | Chain | Doors | Against it |
|---|---|---|---|
| 5 Max, start 249 | 249 -> 252 (L05/561) -> 256 (L05/63) -> 257 (L05/899) | 3, all OPEN DOOR, each script a bare prompt then `go_to_room`, no flag | one door more than Edward |
| 10 Edward (level 9), start 237 | 237 -> 245 (L09/737) -> 243 (L09/996) | 2, OPEN DOOR | both scripts test the vampire (`ed189`, flag 505) and go on through `run_again` (fn34), not tried for a door of player 2's; the chapter turns every effect off once flag 518 is set (L09/2026 sets it) |
| 7 Paul, start 103 | 103 -> 123 (L07/1212) -> 109 (L07/1146, walk in) -> 105 (L07/1341) | 3 | 123's walk-in region (x 66..354, y 4131..4638, z -311..0) does not hold its entry marker (-105, 4376, -35), and another floor may lie over it (a camera region there starts at z 62): where to put player 2 is a guess. Room 123 also has the body's scene region (BodyCineRgn00, L07/2138) |
| 5 Max, by 255 | 249 -> 252 -> 255 (L05/580) | 2 | 580's first time is a scene (stats to full, event 82 to the tracker, script 1070) unless flag 294 or 128 is set; 255 also wants flag 883 clear |
| 6 Lindsey, start 303 | 303 -> 143 -> 301 -> 172 -> 302 -> 144 -> 167 | 6, walk in | far |
| 3 Anthony, 11 Michael | 150 and 50 are not next to a room that has such a door (not traced further) | 3 or more | far |

**The one to try: chapter 5, Max, to room 257.** In all three rooms the door's region holds an entry marker, so the
marker's own place is a flat spot inside the region. READ (`ed room 249`, `252`, `256 --level 5`):

| Door | Region | Put player 2 at |
|---|---|---|
| 249 -> 252 | Region01, box -410..-214, -990..-790, 0..168 | -321, -887, 3 (EntryMkr01) |
| 252 -> 256 | Region01, sphere at 1617, 1834, 541, radius 163 | 1629, 1840, 553 (EntryMkr02) |
| 256 -> 257 | Region02, box -550..-355, 249..521, 0..203 | -472, 388, 0 (EntryMkr02) |

No `ED_RUN_SCRIPT`, no `ED_SET_FLAG`, no key, no Tome: in stay mode a script run by `ED_RUN_SCRIPT` is player 1's
door and leaves player 2 behind (`test_run_script` calls it with player 1 current), so player 2 takes each door by
their own press: `ED_PUT` (who 2), a nudge of the stick, then B, as `logs/run-20261117-124127-f2-p2scene-save` did.
The room they arrive in is 1257: a small room, one door region (back to 256), no arrival script. READ.

## 6. The chapter's rules, and forcing

- **0x8006EB6C(22, level, character)** in chapter 5: yes. Its only refusals for 22 are the ones for every effect:
  the room in play in list 5, chapters 13, 15, 16, chapter 9 with flag 518. READ.
- **The door's own ask** needs level 3 in THEIR holder: `sanity_level` writes holder+21 from their own meter in their
  update, and their set is the one in memory in the bracket. `ED_COOP_P2_SANITY=10` gives level 3 (10 of 75 in the
  Head run). READ. Room 257 is index 9 of list 1 and its bit is clear on a first visit.
- **Flag 883** turns every door effect away in chapter 5 while set (the gate 0x8006DD70). L05/1568 (the letter in
  room 252) sets it, L05/1360 to 1362 (an essence taken) clear it. GUESS: clear at the chapter's start. The test
  never goes near the letter.
- **So no forcing is needed, and none is right.** `ED_SANITY_P2_TEST=F,22` would arm it by event 83, and then
  `sanity_door_rules`' forced branch (about line 27544) starts the forced effect at ANY door of player 2's: for 22
  that breaks the user's rule (the step then cancels itself at 0x8009B844, as "ended after 4 frames" in sanity.md).
  With that branch closed to 22, the game's own case at 0x8004345C starts it only at a door into its ten rooms.
- With 22 in `kSanityP2` the game's roll can also pick it for player 2 at level 3 (flag 8 is clear). It then waits
  for such a door, and keeps their other rolls out until the roll's timer drops it at 8000. That is the game's own
  behaviour (sanity.md: it waited 13,521 frames in the user's solo run). READ.

## What to build

1. **`kSanityP2`** (about line 27589): add 22. Without it the door's own event 83 is refused by `sanity_rules`.
2. **`sanity_door_rules`** (about line 27544): the forced branch never for 22 (`&& g_sanity_p2_forced != 22`). The
   game's own rule at 0x8004345C decides.
3. **`sanity_p2_step`'s guard** (about line 28307): no update while a ticket counts:
   `(s32)mem_read32(cpu, COOP_ROOM_TICKET) > 0`. Their step 0 and their return then never write a ticket that
   `split_ticket_ask` refuses, and their timers stand still for a door's second as the game's do.
4. **The black, their half alone**: the intercept on 0x801E4610 for lr 0x8009B814 and 0x8009B6B0 under
   `g_sanity_p2_in`, `g_sanity_p2_black`, the two rectangles in `sanity_overlay`, the clear in `sanity_p2_step`
   (section 4).
5. **`sanity_p2_door(false)`** (about line 28264): keep `g_sanity_p2_door_began` true when the tracker was already at
   42 and the effect running is 22, so "came into another room" sends no 82 after their own change. No harm today;
   it stops a false log line and keeps 22 safe if the 82 is ever given teeth.
6. **The early end** (0x8006D61C): around `SANITY_UPDATE` in `sanity_p2_step`, their own prompt slots in
   (`g_coop_prompt_slots` over `PROMPT_SLOTS` 0x80335A38, 196 bytes, and back, by `coop_bytes` as `coop_p2_own_cancel` does), so
   the test reads THEIR shown prompt with their B. Today it reads player 1's prompt with player 2's B: their press on
   the ceiling room's door does not end the effect (they walk out and it runs on), and their B anywhere ends it
   while player 1 stands at a door. Not checked: whether player 2's slot 6 is filled as the game fills it.
7. **`sanity_later`** (about line 28033): frames = f1, at least 1 (not seconds). 22's 81 then comes 50 frames after
   the hold, and Head's 81 one frame after it is sent.
8. **Their set when made** (about line 28327): zero `ext` +28..+31 too (the ten rooms' bits are player 1's).
9. Optional: `stayer_lock` (about line 30185) passes a ticket written under `g_sanity_p2_in`, so the binding lock
   lands in their own input copy.
10. Not for this step: player 1's own 22 blacks both halves (the same intercept, the other side); the tracker
    node's bit 8; the menu's Ignore starting 22 at any door in solo.

Nothing to add for the change itself, the return, or the set's bytes: the runner's bracket, `sanity_p2_return_open`
and `SanityOwn` cover them.

## The test

One run before the build, as the route's check: it reads the control frame, shows the three doors, and at the third
shows the door's ask refused (no effect starts). The same run after the build is the test. Frames in `JTG_PUT` and
`JTG_P2_SCRIPT` count from the base; chapter 5's base is not measured (2300 by default): if the report says LATE,
give `JTG_BASE=` the control frame + 30 and run again.

```sh
ED_RUN_AWAY=1 ED_RUN_SECONDS=240 ED_RUN_SHOTS_MS=300 ED_RUN_SHOTS_W=1280 \
ED_COOP_P2_SANITY=10 \
JTG_SET="mod_p1_character=0;mod_p2_character=2;mod_p1_costume=0;mod_p2_costume=0" \
JTG_PUT="60,2,-321,-887,3;400,2,1629,1840,553;740,2,-472,388,0" \
JTG_P2_SCRIPT="62,68,lup;120,126,b;402,408,lup;460,466,b;742,748,lup;800,806,b" \
JTG_FRAMES=2900 \
sh tools/ed/jtg.sh 5 sanity2-ceiling
```

No `ED_SANITY_P2_TEST`. If a door does not open, read its frames in `jtg-report.txt` and move that door's put and
presses (transition 9 is 62 ticks from the press to the change; 280 frames are left between a press and the next
put). Stay mode is on by default in a co-op run ("[rooms] separate rooms on for this run").

What to read after the build (the frames are GUESS, base + about):

| When | Log | Picture |
|---|---|---|
| +870..930 | `[stay] player 2's change to room 257 runs as theirs`; `[sanity2] a door of player 2's ... (state 1, effect 0`; then `player 2's door is over: their set comes out (state 42, effect 22` | their half: room 257 |
| a frame or two on | `[coop] a room change of player 2's`; no `[split] a second room ticket refused`; three frames on `[stay] player 2's change to room 1257 runs as theirs (from room 257`; `[rooms] unpack asked: room 1257` | their half black, player 1's half Max's bedroom and never black |
| +30 frames | (the new line for the black going off) | their half: the room upside down; they can walk |
| +1500 frames | `tracker sends itself event 81 for later (50.000` | they stand held |
| +1560 frames | `player 2's own effect asks for its return to room 256`; `[stay] player 2's change to room 256 runs as theirs (from room 1257`; `player 2's return is done`; `tracker: state 1, effect 0` | the flash in their half, then room 256 at -472, 388, where they pressed the door |

The return is to room 256, not 257: 0x8006BE04 keeps r13-23276 and their place BEFORE the load, at the door into
257. READ.

Pass: all of the above, player 1 untouched in room 249 all through (place, busy 0, no black), no "refused", no
stuck hold on player 2 after the return, and no "ended as a door ends one" line. A second run with a B press in the
ceiling room (`JTG_P2_SCRIPT` + `;1100,1106,b`, they arrive inside 1257's door region) checks item 6: the effect
must end at that press, with the return to 256.
