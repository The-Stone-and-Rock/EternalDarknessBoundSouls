# Blood Bath (effect 11) for player 2 alone: the read (from the disassembly 2026-11-18, nothing run)

Read with `return-spec.md` beside it. Every line is READ (seen in `ed dis`, the DOL's data, or hle.c) unless it says
GUESS. hle.c was being edited while this was read: its line numbers are as of the reading, the function names hold.

## 0. The answer in short

- **Today player 2's Blood Bath ends in a real game over for both players.** The effect's last step sends event 8 (the
  death) inside player 2's update, where the game takes it as the fake one. But the death move's end (event 6) comes
  later, in the frame's node update, with PLAYER 1's sanity set in memory: the game's test `0x8009E218` then says "no
  effect", and the death goes on to event 17, the whole-screen fade and `0x80022328(3)`.
- **The test that decides fake or real is `0x8009E218()`, not `0x800C04EC`.** It reads the tracker IN MEMORY (first
  node of kind 32, its holder's running effect), not the dying character's link. `0x800C04EC` only sends the 81, and it
  sends it through the DYING character's own link (info+140 -> +68), at once, with `0x801F59B0`.
- **The host has no co-op death code.** No hook on `0x8009E218`, `0x800C04EC`, `0x8005855C` or the game over. A death
  of either player is the game's own, for both.
- **Two intercepts make it work:** `0x8009E218` answered from player 2's own kept set when player 2's character asks,
  and `0x800C04EC` queueing the 81 in `g_sanity_p2_later` when the id is player 2's. Section 6.
- Two corrections to `return-spec.md`: the body parts take the BODY's own room (object +592), not r13-23276; the
  death code runs in the frame's node update through `coop_events`, not in player 2's turn.

## 1. The effect itself

Confirmed: start table `0x80243DB8` = `0x80243D8C[11]` = `0x80086A4C`; the name it copies is "Insanity: Blood Bath"
(`0x80244F8C`).

### 1.1 The start, 0x80086A4C(holder)

| Address | What | Reads / writes |
|---|---|---|
| 0x80086A68 | `0x8006BDA0(holder, 11, &i)`: effect 11's record and its row i | holder+4.. |
| 0x80086A80 | `0x80068F48(record, 0x8006BBC4)`: record+28 := the looping step runner | the table |
| 0x80086A98-B8 | row bytes: +106 := 0 (loop start), +107 := 4 (steps), +104 := 4 | holder + i*44 |
| 0x80086ABC-E0 | `0x8006AF5C(holder, 11, 0x80086BA0, holder, 100 + rand & 15)`: step slot 0 | holder + i*44 + 64/80/96 |
| 0x80086AF0-B2C | three more: slots 1, 2, 3, each `300 + rand & 31` ticks | the same |
| 0x80086B38-5C | the order of the parts: screen+5952 := 0, +5956 := 2, +5960 := 3, +5964 := 1; screen+6016 := 0 (the count) | data+348's object (0x80317DF0) |
| 0x80086B6C | +104 := 0 (the first slot is next) | |
| 0x80086B70 | `0x800F5918(holder+24, "Insanity: Blood Bath")` | holder+24 |
| 0x80086B74-88 | record+24 \|= 0x10; data+32 &= ~3 | the table; data |

- No timer but the four slots, no sound, no stream, no camera, no play hold (`0x80199EA0` is not called), no r13 word.
- The runner `0x8006BBC4` calls a slot when the record's tick count (record+20's upper half, +1 each update:
  r13-25484 is the constant 0x10000, `0x80068378`) equals the slot's ticks, then zeroes the count and moves on. So:
  part 0 at 100-115 updates, then each next part 300-331 updates after the one before. **The death comes about
  1000 to 1108 updates of player 2's tracker after the start.**

### 1.2 The step, 0x80086BA0(holder), called four times

| Address | What | Acts on |
|---|---|---|
| 0x80086BC4 | `0x8019B418()`: an event data block (freed at the end, `0x8019B4C4`) | the pool r13-23044 |
| 0x80086BE8-FC | node := `0x801F5F88(holder+56)`; none: return 0 | **holder+56, never r13-26524** |
| 0x80086C48-5C | part := screen[5952 + 4 * screen[6016]]: 0, then 2, then 3, then 1 | the screen object |
| 0x80086C60-84 | `0x801F9FD0(object, part, &vel, 0x2000)`: the part falls off (1.3) | the body |
| 0x80086C88-A4 | data+148 := the part's id, or 0 | data |
| 0x80086CA8-D68 | blood: `0x80118804(object, 20, part, -1, &out, 1)`, then `0x8014699C(object, pos, vel, 16, 4, 0x80301214, 3)` with a random velocity (8 - rand & 15 each way, up 8.0) | a world effect in the room |
| 0x80086D6C-88 | data block +136 := 11, **event 53 at once** to holder+56 (the character's flinch, animation 11) | `0x801F59B0(53, 0, id, block)` |
| parts 2, 3 (0x80086DA0-E38) | if the part was made: hands := `0x80151500(id, 0)`; `0x80150F48(hands, 1 or 2, 0)` -> **data+156 (part 2) or data+160 (part 3)**; not -1: `0x80150FA0(hands, item)` lets go of it. Blood `0x8014610C`. **data+32 \|= 0x00101200. `0x80117260(256, 1)`: bit 256 of r13-24048** | the character's own hands record; the prompt word |
| part 1, the last (0x80086E3C-FA4) | link := `0x80033850(node)` (info+140); data+32 \|= 3; from := link+68. If made: slot 4's item -> **data+164**, let go. 16 blood sprays (`0x80124AD4`, `0x8014699C`). Row bytes +106 := 0, +107 := 0, +104 := 4 and slot 0 := no function: **the steps are over before the send.** Then **`0x801F59B0(8, from, holder+56, 0)`: the death, at once** | |
| part 0 | blood `0x8014610C` only | |
| 0x80086FC4-F0 | screen[6016] += 1 (back to 0 at 16) | |

- No sound call, no stream, no camera, no hold in the step. The player walks about while falling apart.
- data+32's bits 0x200, 0x1000, 0x100000 (parts 2, 3) and 1, 2 (the death) are the game's "no arms" rules. They are
  asked with `0x8006DB04(mask)` of the tracker IN MEMORY: 0x200 in the player's handler at events 15, 40, 69, 70;
  0x100000 at events 155, 177, 198 (the reload), 208; 0x1000 at `0x801043F4`; 1 at `0x80006D8C` and `0x80113EC0`;
  2 at `0x801C42A4`. What each forbids: GUESS (attacks, reload, items, the pause, a cast).

### 1.3 A part, 0x801F9FD0 -> 0x801F9E98

| Address | What |
|---|---|
| 0x801F9FF8 | r13-23276 is read for a node count (`0x801F56B0(-1, room)`) **whose answer is thrown away** (r3 overwritten at 0x801FA000) |
| 0x801FA004-3C | made only if `0x8012166C` / `0x80121574` and `0x80119944(object)` say yes (room for one more: GUESS) |
| 0x801279AC | the part's object: **its room +592 := the body's +592** (0x80127A2C), its +660, +44..+56 from the body too |
| 0x801F9EF8-F14 | a node of kind 3 (handler `0x801FA07C`) |
| 0x801F9F18-38 | type 5 (`0x80117C5C`); **type 6 when the body's id is the current player's** (`0x801F62B8`, r13-21496) |
| 0x801F9F40 | event 1 to itself at once (`0x801F5D18`) |
| 0x80127968 -> 0x801259F4 | **the part hidden on the body**: `0x801258BC(object, part, 0)` |

The part's own behaviour, `0x801FA07C`:
- Event 1 with flag 0x2000 (Blood Bath's): event 17 to itself for 320.0 later (`0x801F57C0`), state 11. At 17: a fade
  (`0x80125BB8`), then event 57 for 52.0 later. 57: `0x801DCB58(2, node)`, the part is freed. So **each part removes
  itself**, about 372 of `0x801F57C0`'s time units after it falls (the unit: GUESS, the same as the death's 120).
- **Event 229** (and 61, 89, 28): a part of type 6 hides (`0x8011FC8C`) and sends itself 57 at once.
- Shakespeare's head is 0x24000: no 0x2000, so it never times out. Blood Bath's parts have no 0x4000: not pickable.

### 1.4 Words outside `SanityOwn` that the start and the step touch

| Word | Where | Decision |
|---|---|---|
| r13-24048 bit 256 (prompts off) | written `0x80086E34`, parts 2 and 3; cleared by the return's callback `0x8006B578` | per player in the host: section 5, build step 4 |
| r13-21496 (the current player's id) | read `0x801F9F24` for the part's type 6 | right in their update (`coop_be_player2`) |
| r13-23276 | read `0x801F9FF8`, unused | nothing |
| r13-26840 (event serial), r13-23044, r13-21504 (pools), the random state `0x800F4474` | | shared, fine |
| 0x80239464 (12 bytes of 0.0), 0x80239470 (16 offsets), 0x80301214, 0x80244F8C | read only | nothing |

Everything else it writes is in the set already: the holder's rows (+64..+107), the record (the table), data+32,
+148, +156, +160, +164 (`SANITY_EXT`), screen+5952..+6016 (`SANITY_SCREEN`, 6024 bytes). **Nothing to add to
`SanityOwn` or `kSanityWords`.**

## 2. The death

### 2.1 The path in the game

| Step | Address | What |
|---|---|---|
| Event 8 arrives | `0x801F59B0` -> `0x801F5000` -> `0x801F5294` -> kind 2's handler `0x80058DD8` | synchronous. A state that does not answer falls back to state 0's handlers (0x801F50EC-0x801F511C) |
| Busy filter | `0x80058E80`: `0x80155738()` not 0 -> `0x800CA964` | **event 8 is swallowed while the game is busy** (0x800CA9C0 -> 0x800CAA94 -> answer 1; rooms 208 and 64 differ). 85, 6, 17, 229 pass |
| State 0, event 8 | `0x80059F64` | `0x8009E218()` true: **event 85 to itself at once**, no death. False: room 96 nothing; else move 24 with flag 32, things dropped, state 8 |
| State 56 (a grab), event 8 | `0x80053F68` | 126 to the grabber, then the same test and 85 |
| Event 85 | `0x8005A0CC` | `0x80122920(object, 24, 0, 10)` (move 24); its end sends 6 and 7 (`0x8012206C` / `0x80122088` with `0x801F8FA4`, id << 8 \| event); state 8 |
| State change | `0x801F514C-C8` | in the same dispatch: event 2 to the old state, event 1 to state 8 |
| State 8, event 1 | `0x800585A4` | `0x80018808(2)`, `0x801E4610(0)`, `0x801172B0()`, **`0x80117260(64, 1)`: bit 64 of r13-24048**, the busy ask, then the test again. True: pending 17s dropped (`0x801F4E48(id, id, 8, 17, 0)`), **music `0x801A16A0(50, 50, 5)`, `0x801A19D0(60)`, `0x801A16A0(100, 100, 30)`**. False: the real one's, `0x801A19D0(600)` |
| Move 24 ends | the frame's node update `0x8002721C` / `0x800273DC` -> `0x80123698` -> `0x801F8FA4` -> `0x801F59B0(6, id, id, 0)` | not in a turn |
| State 8, event 6 | `0x80058750` | **the test again.** True: `0x800C04EC(0, id)` at `0x8005876C`, then state 1. False: models 85, 119-122 get up with health refilled; **everyone else: event 17 for 120.0 later and a 120-tick whole-screen fade (`0x801E7C88`)** |
| State 8, event 17 | `0x8005889C` | fade, `0x80042974(0, 0)`, **`0x80022328(3)`: the game over** |
| State 8, event 229 | `0x800586E8` | test true: state 1 and `0x80124788(object)` (the death move let go of) |
| State 8, event 2 | `0x8005895C` | `0x80117260(64, 0)`, pending 17s dropped |

### 2.2 The test, 0x8009E218() (no arguments)

- Tracker := first node of kind 32 (`0x801F8FD8`); holder := its info+56; number := `0x80068D48(holder)` (the holder's
  +4 record at stage 4, and data+32's 0x10000 clear; else -1).
- Yes for effects **4, 9, 11, 16, 17, 18, 19, 22, 29, 32, 33, 34, 35, 36** (jump table `0x80245470`, index number - 4);
  for 13 unless data+32 has bit 4.
- Yes too when data+32 has 0x20000 (`0x8006DB04(0x20000)`), or in room 255 of chapter 5 with flag 883.
- **It never looks at who is dying.** Nine callers: `0x80059F6C`, `0x800585F0`, `0x800586F0`, `0x8005872C`,
  `0x80058740`, `0x80058758`, `0x80053F84` (all the player's handler), `0x801CF394` (a thing tied to an owner in state
  8), `0x8019E37C` (sound 28's priority).

### 2.3 0x800C04EC(r3 = 0, r4 = the dying character's id), whole

| Address | What |
|---|---|
| 0x800C0500-30 | room r13-23276 == 255, chapter 5, flag 883: `0x80145760(20, 0, 0)` and a ticket to room 252, script 2779 |
| 0x800C059C-BC | otherwise: node of the id, its info, **info+140: the dying character's own link**; none: nothing |
| 0x800C05C4-D4 | **`0x801F59B0(81, id, link+68, 0)`: at once, not for later** |
| 0x800C05DC | answers 1 always |

- It makes no decision. With link+68 == 0 the 81 goes to id 0: `0x801F5000` finds no node and answers 8 (0x801F5020-60).
  **Nothing happens, and the caller puts the character in state 1.** No real death comes from here.
- `split_send` / `sanity_later` never see it: they hang on `0x801F57C0`.

### 2.4 In which context each piece runs for player 2

| Piece | Context | Set in memory | Link |
|---|---|---|---|
| Event 8, the 85, state 8's event 1 | inside `sanity_p2_step` (hle.c 28279, from `coop_pad_read` 34947), through `coop_events` (25780) nested | player 2's | the tracker's id |
| Event 6, `0x800C04EC`, state 8's event 2 | the frame's node update, through `coop_events`: player 2's controller state, kinds and room are in; **player 1 is still the current player** | **player 1's** | 0 |
| Event 17 and the game over | the game's event queue, the same | player 1's | 0 |

- `coop_player_turn` (24532) is the controller's turn (the pad to `0x80045BC4`). The death code does not run there.
- hle.c's own note agrees: a move's end (event 6) of player 2's comes outside their turn (25047-25049, the reload).
- **What co-op does with the fake death today:** event 6's test reads player 1's set. Player 1 has no such effect:
  real death, event 17, the fade over both halves, game over for both. No revive, nothing hidden.
- The mirror, also today: player 1 has one of the fourteen running and player 2 is killed by an enemy: the test says
  yes, player 2 plays the death move, the 81 goes to id 0, and player 2 stands up in state 1 with health 0 (what
  follows: GUESS).

## 3. What must be true for the 81 to reach player 2's set

1. At event 6 the test must answer for PLAYER 2's set. It does not: section 2.4.
2. The 81 must go to the tracker while player 2's set is in. It is sent at once, outside their update, to id 0.

Both are host work: section 6, steps 1 and 2. `docs/coop-failed-attempts.md` has nothing on a player's death or a
game over (searched: death, dies, dead, game over, revive; the only hits are two notes on player 2 hurt 75 to 0 by a
hazard, fixed in `coop_hazard_touch`).

## 4. After the 81

| Step | Address | What | For player 2 |
|---|---|---|---|
| 81 handled | `0x8006C850(holder, 0)` (state 42, `0x8006D5C0`) | 11 has no case of its own (`0x8006CC08`: number - 13 out of range) | in their update, from the queue |
| | `0x8006CC98-AC` | **229 at once to holder+56** | their character is in state 1 by then: state 8's `0x80124788` does not run (build step 2 calls it) |
| | `0x8006CCB0-B4` | **`0x8009EA64(r13-23276)`: 229 to every kind-3 node whose object's room is that room.** Parts of type 6 free themselves | their update has their room in (`coop_stay_room_in`): their parts |
| | `0x8006CCC4-D4` | `0x80199EA0(0)`, `0x80117260(256, 1)`, the flash `0x8006B270` and `0x8006B438` registered | as for 32 and 17 |
| The return | ticket, transition 17, callback `0x8006B45C` -> `0x8006BF00` -> `0x8006B7B8(holder, 0)` | the bracket already built (`sanity_p2_return_open`) | |
| | `0x8006B7FC-824` | place data+116..124 (`0x80118264`), angle data+128, `0x801C5580(id, 0)` | |
| | `0x8006B910-4C` | **`0x801258BC(object, 0..3, 1)`: the four parts shown on the body again** | the body is whole |
| | `0x8006B950-A48` | data+160, +156, +164, each if not -1 and its node is found: **`0x80150D9C(hands, id, slot 2 / 1 / 4, type, 0)`: back in the hands** | by id: see risks |
| | `0x8006BA4C-98` | four stats from data+96..102 (`0x800354F8(node, 0 / 2 / 3 / 1, value, 0)`): health too | a real hit that killed them under an effect is undone, as in solo |
| | `0x8006B4E8-B580` | `0x80199EA0(1)`, data+32 \|= 0x10000, `0x80117260(256, 0)` | |

- **The body is put back together by the return** (the four `0x801258BC`).
- **The parts are cleaned up twice over:** by the 229 at the 81 (type 6, in the room in play), and by their own clock.
- **Which room the parts get:** the body's own (+592), whatever r13-23276 holds. Player 2 apart: player 2's room.
- Type 6 needs player 2 to be the current player when the part is made: true in their update.
- The 229 sweep takes every type-6 part in that room: with both players in one room and both in Blood Bath, one's
  81 frees the other's parts too (they come back on the body only at their own return). Rare.
- data+156 / +160 / +164 are written only by the step (no other writer between 0x80068000-0x80070000 and
  0x80084000-0x800A0000). The return reads them after EVERY effect. Player 2's set is copied from player 1's when
  made: start them at -1 (build step 5).

## 5. What shows on player 1's half or acts on player 1

| What | Where | Effect on player 1 |
|---|---|---|
| r13-26524 (the current player) | not read by the start, the step or the return for 11 (only effect 23's branch, `0x8006B4B8`) | none |
| The camera | none in 11; the flash's `0x801EEE90` and the return's cameras are the ones 32 and 17 already use | none new |
| **r13-24048 bit 256** | set at part 2, about 400 updates in, until the return's callback: 600 updates and more | **the prompt chooser shows nothing while any bit is set (`0x801176B4`): player 1 has no prompts all that time**, unless the host keeps the bit player 2's |
| **r13-24048 bit 64** | state 8's event 1 to event 2 (the length of move 24) | the same |
| Whole-screen fade | only the REAL death's (`0x801E7C88` at `0x80058890`, `0x8005893C`) | none once step 1 is built |
| `0x801E4610(0)` (un-black) | state 8's event 1 | none, unless player 1's own black is on (22) |
| The music | state 8's event 1: down to 50 and back over 60 | both hear it |
| Sound 536 and the flash | the 81, as 32 and 17 | as built |
| Blood and the parts | world effects and objects of player 2's room | player 1 sees them in a shared room: right |
| `0x801172B0()` | state 8's event 1, inside player 2's update with player 1's prompt slots in | player 1's shown prompt cleared for a frame (GUESS) |
| A FINISH HIM on player 2's body | player 1's look for bodies in state 8 asks event 193; kind 2's handler has no case for 193 | passed over (GUESS: the default answer) |

## 6. What to build

1. **Intercept `0x8009E218` (new `sanity_fake_death`, in `kOwn[]`, hle.c 29033; first word 0x9421FFF0).** No
   arguments, answer in r3. Find the asker's node by the return address:

   | lr | Asker's node |
   |---|---|
   | 0x80059F70 | gpr[31] |
   | 0x800585F4, 0x800586F4, 0x80058730, 0x80058744, 0x8005875C | gpr[27] |
   | 0x80053F88 | gpr[24] |
   | 0x801CF398 | gpr[30] |
   | any other (0x8019E380) | pass through |

   When co-op is on, `g_sanity_p2.made`, the asker is `g_coop_p2` and `!g_sanity_p2_in && !g_sanity_p2_door`: answer
   from `g_sanity_p2` with `sanity_own_running(cpu, &g_sanity_p2, &data)` (27960) and
   `flags = sanity_own_read(cpu, &g_sanity_p2, data + 32, 4)`: 1 when the number is one of 4, 9, 11, 16, 17, 18, 19,
   22, 29, 32, 33, 34, 35, 36, or 13 with `!(flags & 4)`, or `flags & 0x20000`, or room 255 / chapter 5
   (`0x80307178`) / flag 883 (`0x801DC22C(r13-26528, 883)`). Else 0. Everything else passes through.
   Optional mirror: the asker is player 1's node while player 2's set is in: the same from `g_sanity_p1_kept`
   (update, pickup) or `g_sanity_p1_door` (door).
2. **Intercept `0x800C04EC` (new `sanity_death_ends`; first word 0x9421FFE0; its one caller is `0x8005876C`).** When
   gpr[4] is player 2's id (`mem_read32(g_coop_p2 + 16)`), `g_sanity_p2.made`, their set is not in, and it is not the
   room 255 case:
   - queue `{event 81, data 0, due = frame + 1}` in `g_sanity_p2_later` (as `sanity_later` fills it; `sanity_p2_step`
     gives it as `0x801F59B0(81, id, id, 0)` with everything of theirs in);
   - call `0x80124788(mem_read32(g_coop_p2 + 52))`: what state 8's own 229 does when the 81 is answered at once;
   - take bit 64 out of the host's copy of their prompt bits (step 4);
   - answer 1, no pass through.
   With their set in (`g_sanity_p2_in`), or another id: pass through.
3. **Add 11 to `kSanityP2` (27589).** Nothing to add to `SanityOwn`.
4. **Bits 64 and 256 of r13-24048 kept player 2's, as `g_sanity_p2_busy` keeps the busy bits (28529).** In
   `sanity_p2_step`: before the update `word |= g_sanity_p2_prompts`; after it
   `g_sanity_p2_prompts = now & 0x140 & (~was | g_sanity_p2_prompts)` and the word gets player 1's own two bits back.
   The same in the return's bracket (`sanity_p2_return_open` in, `sanity_p2_door(false)` out: the callback clears 256
   there). This also frees player 1's prompts between the 81 and the return of 32 and 17.
5. **Player 2's set starts with data+156, +160, +164 at -1 and +148 at 0**: where the set is made in
   `sanity_p2_step` (`s->ext + 156`, 12 bytes of 0xFF; `s->ext + 148`, 4 of 0).
6. **The death must not be swallowed.** The step has cleared its slots before it sends event 8; if the busy filter
   (`0x80058E80`) or a state that ignores 8 (78, 82, 127) eats it, the effect never ends. In `coop_send_event`
   (25046): event 8 to player 2's id with `g_sanity_p2_in` sets a one-shot flag; `coop_busy_ask_answer` (23237)
   answers 0 once at lr 0x80058E84 while it is set. Fallback: in `sanity_p2_step`, effect 11 running with row byte
   +107 == 0 and player 2 not in state 8 for 120 updates: queue the 81.
7. Optional, for the rules to bind player 2 (no attack, no reload with no arms): `0x8006DB04(mask)` answered from
   `g_sanity_p2`'s data+32 when player 2's handler asks (`g_coop_turn || g_coop_events_own`). Without it nothing
   breaks: player 2 can still swing with no arms.

Effect 23 (Zombie) dies by the same event 8: steps 1, 2 and 6 serve it too.

## 7. Not checked

- Player 2's held things are host copies answered by id (`coop_rehold` 33005, `hle_find_by_id`, `g_coop_copies`).
  The step's `0x80150FA0` and the return's `0x801F5F88` + `0x80150D9C` go by id: whether each finds the copy inside
  the update and the return's bracket was not traced. `coop_rehold` puts a listed thing back in an empty hand once a
  second, up to three tries: it may fight the step if the hands record still lists the item after the let go.
- `coop_events` nested inside `sanity_p2_step` swaps player 2's controller state a second time (25795 has no
  `!g_sanity_p2_in`): what the death's handlers write there is dropped at the update's end. Effect 17's event 53 has
  run this way without harm.
- While the game is busy a node with bit 8 of +28 does not advance its move (`0x80123B70`, transitions.md:72): the
  death move, and so event 6, waits out a hold of player 1's that the host does not answer "not busy". A delay only.
- What event 82 (a door) does to a Blood Bath in progress: not read.
- States 78, 82 and 127 of the player (they do not take event 8): which they are.
- The time unit of `0x801F57C0`'s wait (the parts' 320 and 52, the death's 120).

## 8. Test

Forced, as `return-spec.md` section 5: `ED_SANITY_P2_TEST="F,11"`, `ED_COOP_P2_PUT` into room 100's box region, a
door of player 2's. Pass:
- about 1100 frames on, `[sanity2] the kept event 81 given to player 2's tracker`, then the return's two lines;
- player 2 whole, at the kept place, holding what they held, health as before;
- no fade on player 1's half, no room 69, player 1 walking and offered prompts throughout;
- no kind-3 node left in the effect's room.
Then the mirror: player 1 forced into 32, player 2 killed by an enemy: the real death, as the game's.
