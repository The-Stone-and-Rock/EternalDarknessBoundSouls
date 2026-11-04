> Kept as the evidence behind `docs/game/systems/`. Reports were checked by skeptics before the kit was built. Where a report says the kit is wrong, the kit has since been corrected. `tools/ed/facts` holds the scripts it cites, and `ed build facts` remakes the tables.

> **Watched since: `transitions-watched.md`.** A tick is one frame. The motion is still playing at the room change on 9, 12 and 15. The arrival map and state 80 and 81 are proved there. "Not settled" below is as written before those runs.

# Door transitions

Reconciled against three skeptic reviews. Everything in the main sections survived them or was settled by looking. What failed is under **Not established** at the end.

Each statement is marked:

- **[proved]**: checked against live memory, or data and code agree.
- **[read]**: from the code alone. Not watched in a run.
- **[inferred]**: from how scripts use it.

All commands run from the repo root.

- `python -I tools/ed/facts/transitions.py` reprints every count below and rewrites `transitions.json`. It reads the disc, `build/ed-db`, `regions.json` and the saves.
- `python -I tools/ed/ed.py dis ADDR COUNT` shows any address quoted here.

"Tick" means one call of `0x80155830`, once per game loop (`0x800083EC`, `0x80050A90`) **[read]**. Seconds per tick weren't measured.

## The answer

The transition number packs two things into one branch of `fn66`'s jump table: **how long the fade to black is** and **which motion the player starts**.

| n | Motion | Fade (ticks) | Ticks to room change | Script calls, raw / distinct | Usage |
|---|---|---|---|---|---|
| 0 | none | none | 1 | 143 / 27 | 23 of 27 have no prompt. Entries include 7 `EntryMkr_TomeReturn`, regions include 6 `MirrorRgn` |
| 1 | none | 5 | 7 | 2 / 2 | Both follow EXAMINE. `L04/143` runs from `ElevatorRgn01` in room 190, `L04/146` from `SKPlane02` and `03` in room 189 |
| 2 | none | 10 | 12 | 422 / 253 | 205 of 253 have no prompt. 42 follow OPEN_DOOR |
| 3 | none | 60 | 62 | 86 / 63 | 53 of 63 have no prompt |
| 4 | 2 | 5 | 7 | 0 | Never used by a script |
| 5 | 2 | 10 | 12 | 0 | Never used by a script |
| 6 | 2 | 60 | 62 | 2 / 2 | Both follow EXAMINE, both `JumpOverRgn`: `L04/117`, `L04/51` |
| 7 | 25 | 5 | 7 | 0 | Never used by a script |
| 8 | 25 | 10 | 12 | 0 | Never used by a script |
| **9** | **25** | **60** | **62** | 333 / 271 | **Door: all 271 follow OPEN_DOOR** |
| 10 | 103 | 5 | 7 | 0 | Never used by a script |
| 11 | 103 | 10 | 12 | 0 | Never used by a script |
| **12** | **103** | **60** | **62** | 46 / 43 | **Climb up: all 43 follow CLIMB_UP** |
| 13 | 104 | 5 | 7 | 0 | Never used by a script |
| 14 | 104 | 10 | 12 | 0 | Never used by a script |
| **15** | **104** | **60** | **62** | 49 / 48 | **Climb down: 45 follow CLIMB_DOWN, 2 TEXT, 1 EXAMINE** |
| 16 | none | 62, after 128 ticks | 190 | 0 | Only game code asks for it: `0x800E8E14`, to room 83 |
| 17 | none | `0x801E7C88(black, 0)` | 1 | 0 | Game code only, five sites. Loads the room, doesn't place the player |
| other | none | none | 1 | 0 | Above 16 or negative: the default branch |

- **Motion, fade, countdown: [proved].** The 18 words at `0x80253F90` are identical in the DOL and all 12 saves, and word 17 is text, so the table has 17 entries. `jump_table` in the JSON decodes each branch from `dis 0x801667C8 60`.
- **Default branch: [read].** `0x801667A8` is `cmplwi r25,16` then `bgt`, with `r22=-1` from `0x80166794`.
- **Transition 16's 62: [read].** `fn66` sets countdown 190, delay 128 and skips its own fade (`0x80166934`). The tick starts it when the delay reaches 0, with whatever countdown remains: 190 - 128 = 62 (`0x80155864` to `0x8015587C`).
- **Transition 17: [read].** All five sites pass countdown 1 and delay 1, so the tick calls `0x801E7C88(black, 0)` (`ticket_sites` in the JSON).
- **Script-call counts and the usage column: [inferred] for meaning, [proved] for the counts.**
  - Raw calls total 1083, distinct 709, across 658 per-level scripts and 531 ids.
  - Name strings in the packs give 658 chunks with `fn66`, so the decompiler lost none.
  - Level -1 has no `fn66` or `ed29`. Every transition argument is a literal.
  - "Prompt" is the last `ed301` earlier in the same file. It is a text position, not flow analysis.
- **The rule runs one way only [proved].** OPEN_DOOR also precedes 42 transition 2, 5 transition 3 and 2 transition 0 calls. A prompt doesn't fix the transition.

**Timing [read]:** countdown is fade + 2 (`0x8016692C`), or 1 with no fade (`0x801668E4`). The writer makes any countdown of 0 or less into 1 (`0x80155794`). The tick decrements first and acts at 0 (`0x80155854`, `0x8015588C`).

**Fade length is in ticks [read]:** `0x801E7DA4` takes 1 off `r13-21784` per call (`0x801E7DDC`, `0x801E7EA8`). It is called in the same two loops as the tick (`0x80008560`, `0x80050BA4`).

**Fade colour [proved]:** black `0x000000FF`. `fn66` uses `r2-22112`, transition 16 uses `r2-22236`. `r2` is `0x80628460` (`0x80005424`), and `r2-22112` holds `0xFF` in all 12 saves.

## What fn66 does when called [read]

From `0x801666A4`:

1. **Reads its arguments only if there are 6 or 7** (`0x801666C8`). Otherwise it reads none and uses what `r21` to `r28` hold. Every script call passes 6 or 7 **[proved]**, so this only matters to host code calling it.
2. **Checks it may go** (`0x800440D0`). If that returns 0 nothing happens. No script reads the return value.
3. **Lets the player move while busy:** `0x801F6828(player, 0)` clears bit `0x8` of node +28.
   - While busy, a node with that bit set gets `0x80123B70`, which doesn't advance its motion, instead of `0x80123698` (`0x800271F0` to `0x8002721C`).
   - `coop_door_motion`'s comment describes the same thing: "the motion stood still through the fade".
4. **Starts the motion** (`0x801669AC(m)`), which sends event 253 to the player node.
5. **Writes the ticket** (`0x80155770`).
6. **Starts the fade** (`0x801E7C88(black, fade)`), except transition 16.
7. **Fades out sound** (`0x801A05EC`), length capped at 254 (`0x80166950`). A cut passes 0.

### The ticket at 0x8058AFFC

| Offset | Holds |
|---|---|
| +0 | Countdown |
| +4 | Ticks before the fade starts (only game code and transition 16 use it) |
| +8 | Room |
| +12 | 5th argument, script |
| +16 | 6th argument, flags |
| +20 | Transition |
| +24 | 7th argument (1 if absent) |
| +28 | Entry hash |
| +32 | Camera hash |
| +36, +40 | Callback and its argument (`fn66` passes 0) |

**Layout [proved]:** stores at `0x801557AC` to `0x801557DC`, and each save's words equal one script's exact call on every field:

| Save | Room | Transition | Script with that exact call |
|---|---|---|---|
| slot1, slot2 | 147 | 3 | `L03/892` |
| slot3 | 21 | 15 | `L03/1097` |
| slot4 | 152 | 2 | `L03/2300` |
| slot5 | 151 | 9 | `L03/896` |
| slot6 | 153 | 9 | `L03/675` |
| slot7, slot8 | 155 | 9 | `L03/1107` |
| slot9, slot10, coop-apart, coop-recover | 0 | 0 | Blank, with +24 = 1. Matches nothing |

**How long it stays [read]:** the tick never clears it, so a used ticket remains. `0x80155740` zeroes all 44 bytes (called from `0x801540AC`), and `0x801522E4` resets +24 to 1. Slot9 and slot10 were in a room with a different previous room and still hold a blank ticket **[proved]**, so a blank ticket doesn't mean no room change has happened. What cleared them wasn't traced.

The writer also sets `r13-23392` to 1, calls `0x80117260(0x10000, 1)`, and sets busy reason 4 through `0x801556CC` **[read]**.

## The motion on the player

Player event handler `0x80058DD8`, event 253 at `0x80059188` **[read]**.

**How the event gets there:** `0x801F5000` tries the current state, then state 0 (`0x801F510C`). State 121 doesn't test 253. States 80 and 81 do: they stop the motion and re-send (`0x8005B10C`, `0x8005B2B0`).

1. **Nearest marker with flag 4** (`0x80151B90`: smallest distance over records with +72 & 4, no range limit). **None: no motion, no state change** (`0x800591BC`).
2. **Motion 25 only:** if the model has no 25 (`0x80123544`), it uses 99 (`0x80059218`).
3. **Motion 2:** `0x80122E44(obj, 2, flags 5)`, turning to that marker. Bit 1 of the flags is looping (`0x801238FC`).
4. **Others:** `0x80122920(obj, m, flags 0x100, r6=5)`. `r6` is stored at channel +250 (`0x80123170`) and compared by `0x80123070`. Turns only if off by more than a threshold (`0x80059274`).
5. **If the play call returned nothing, nothing more happens** (`0x80059290`). Otherwise it registers two callbacks and sets **state 121**:
   - `0x8012206C` registers `0x801F8FA4` as the end callback (channel +176, run through `0x801DCAE4` at `0x80123804`). It sends **event 6** to the node.
   - `0x80122088` registers the same function for replacement (channel +168, called at `0x801230EC`). It sends **event 7**.

**State 121 [read]** (`0x8005AE90`):

- **Event 6:** state 1, always.
- **Event 7:** state 1 only if `0x80032AD0` returns 0. It returns 1 when node +32 is set and +20 != +24, or node +33 is set and +8 != +12. +20 is the node's state word. The meaning of the other fields wasn't traced, so "a change is pending" is a reading.
- **Eleven events return 1:** 52, 32, 107, 153, 44, 105, 125, 175, 40, 147, 43.
- **Everything else returns 0** (`0x8005B964`) and falls back to state 0. That is how event 238 is answered in state 121.

**Busy doesn't swallow these [proved by decoding the branch tree].** While busy, `0x80058E80` to `0x80058EB4` offers every event to `0x800CA964`.

- Events 6, 7, 61, 62, 191, 238 and 253 reach `li r3,0` at `0x800CAAF0`: not consumed.
- It consumes 10, 24, 25, 30, 32, 50, 53, 55, 57, 60, 63, 67, 101, 103, 107, 125, 133, 202 always, and 8, 11, 39, 59 unless the room is 208 or 64 with conditions.

### Flag 4 [proved]

Read from each room's own table: count at +212, 76-byte records, flags at +72. The table matches live memory with 0 differing records in all 12 saves. `marker_table_vs_live` in the JSON shows it.

- **429 of 432 `GravityMkr` have flag 4.** The other three: room 165 `0x10`, 258 `0x2`, 302 `0x10`.
- **Four other markers have flag 4:** `SpellMkr01` in 91 and 272, `SanityMkr04` in 99 and 278. All four rooms also have a `GravityMkr`.
- **85 of 297 rooms have no flag 4 marker.** 82 have no `GravityMkr`; add 165, 258, 302. Listed in `rooms_without_flag4_marker`. By step 1 above **[read]**, a transition with a motion started there plays nothing.
- **Motion scripts placed in such rooms: 15 ids, 19 per-level scripts** (`motion_scripts_in_a_room_without_flag4`):

| Script | Transition | Room without flag 4 | Also placed in a room with it |
|---|---|---|---|
| `L05/421` | 9 | 251 (`BoxRegion01`) | none |
| `L03/674`, `675` | 9 | 1151 | 151 |
| `L05/278`, `282`, `283`, `284`, `343` | 9 | 1255 | 255 |
| `L05/563` | 9 | 1253 | 253 |
| `L07/1249` | 9 | 1105 | 105 |
| `L09` and `L16` `277`, `333`, `348`, `579` | 9 | 1243 | 243 |
| `L11/339` | 12 | 1030 | 30 |

Whether any of them runs in those rooms wasn't tested: no literal `fn66` names a room of 1000 or above, and none names 251 **[proved]**. The only site found building a 1000+ room is `0x8009B7C8` (`object +76 + 1000`) **[read]**, and who runs it wasn't traced. Rooms 1000 and up: 1030, 1105, 1151, 1161, 1167, 1243, 1248, 1253, 1255, 1257.

### How long each motion runs [proved]

This is the new result, and it decides what arrival does.

**One update advances a track by 1.0 in 16.16, or 4 units of the key's u16 time.** Evidence from slot1's playing track (`live_track_vs_keys`):

- Keys `0x00100001` and `0x00E00001` give a segment length of `0x340000`, exactly `(b>>2) - (a>>2)` (`0x80121A04`).
- Track time `0x370000` equals `(a>>2)` + elapsed `0x330000`.
- The rate is `0x10000` at channel +200 and object +600, in every save. `0x801202F8` adds rate × direction per update.
- All lists of all tracks in these motions end on the same key.
- So the frame word at motion +8 is **2 updates per frame**, not 1.

Length in updates, first key to last, for player 1 (`player_motions_by_save`):

| Motion | slots 1 to 10 (frame word) | coop-apart, coop-recover (frame word) |
|---|---|---|
| 2 (looped) | 52 (28) | 48 (26) |
| 25 | **106** (55) | **104** (54) |
| 103 | **80** (42) | **82** (43) |
| 104 | **60** (32) | **82** (43) |

Each unpacked motion matches the game's size at +288 in all 12 saves. Motion 99 isn't loaded in any of them.

**Whether the motion is still playing at the room change [inferred]:**

- The count of 62 ticks is shorter than motions 25 and 103, so if the player's motion updates once per tick they are **still playing when the room changes**, and event 238 finds them.
- `coop_follow`'s comment agrees: a door opener is still in state 121 at the room change and goes on to "see the motion out".
- Motion 104 at 60 updates is too close to call. It could end around the room change.
- Once per tick isn't traced: `0x800271B4`, which makes the call, is registered by address and its caller wasn't followed.

### The raw key data, per save [proved as numbers]

The third short of the first key list, which the earlier report called root height. The label isn't proved.

| Motion | slots 1, 2, 9, 10 | slot 3, 5, 6 | slot 4, 7, 8 | coop saves |
|---|---|---|---|---|
| 25, first to last (range) | 2537 to 2260 | 2514 to 2239 | 2514 to 2239 | 2240 to 2046 (2046 to 2269) |
| 103 | 2335 to 2491 (2335 to 2512) | same | same | 2240 to 2233 (2174 to 2252) |
| 104 | 2335 to 1289 (1289 to 2465) | same | same | 2240 to 1352 (1206 to 2240) |

103 and 104 start on the same first key in all 12 saves. Motion 2's first and last keys match in all 12.

Standing player 1 in all 12 saves: state 1, motion 15. Flags `0x21` in 11, `0x25` in slot9. `player_data +124` is -1 in all 12.

## Arrival

From `0x80155830`, `0x80152298`, `0x80152614` **[read]**.

1. **At countdown 0** the tick calls `0x80152614(room, entry, camera, script, flags)`. For transition 17 it calls only `0x80152298(room, flags)`, then sends event 62 unless `r13-26508` is set. Then any callback.
2. **The room loads.** `0x80154C94` sets `r13-23276` and keeps the old room in `r13-23288`.
3. **`0x80151FDC` closes the wait.** It clears busy and re-holds the player. When `r13-26508` is 0 it sends event 61 to the player unless `0x80157364` is set (`0x801520DC`), then to nodes in the **old** room (`0x801520F4`).
4. **It starts a 10 tick fade to transparent** (`0x801521A8`). The transition has no effect on this, and `r2-16856` is 0 in all 12 saves **[proved]**. A later `0x801E7C88` overwrites it: see the 5th argument.
5. **The player is placed**, unless `r13-26280` is set. `0x80155348(entry, 2)` finds the marker by hash at +40, position at +44, heading from +56 and +68 (`0x80151914`). `0x80130EF0(player, pos, 10.0)` places them.
6. **Event 238 asks which motion to finish** (`0x8005A8DC`, state 0 fallback). It has two branches:

**First, player data +124 (`0x8005A8E4`).** Player data is `*(node +40)`. If not -1, it stops the motion, plays `0x80118FB4`'s motion with flags `0x10031`, returns +124 (0 becomes -1), and resets +124 to -1. **Native `ed196` sets it.**

| Script | Line | Effect |
|---|---|---|
| `L01/122` | `fn66(180, ..., 3, 1162, 0)` then `ed196(25)` | A transition with no leaving motion still arrives on 25 |
| `L01/27` | `fn66(182, ..., 9, 0, 0)` then `ed196(0)` | A door arrives with no motion |
| `L05/2718` | `ed196(0)` at line 40, then `fn66(259, ..., 0, 1167, 0)` or `fn66(259, ..., 0, 1162, 0)` | A cut's arrival still goes through the +124 branch: stops the motion, plays `0x80118FB4`'s |

Those three are all of them: 3 decompiled, 3 name strings in the packs **[proved]**. The +124 write is `0x800170B8`. It is reset at `0x80058F48` and `0x8005A928`.

**Otherwise, by the motion current at that moment (`0x801222F0`), not the one the door started:**

| Motion playing | Plays on arrival |
|---|---|
| 25 | 25 |
| 99 | 99 |
| 103 | 104 |
| 104 | 103 |
| Anything else | None |

7. **If there is one:** heading is the marker's + pi (`r2-22240`, pi in all 12 saves **[proved]**). Event 191 plays it with flags `0x10002`, and state becomes 80 (state 1 if the play returns 0). Bit `0x2` sets `r26=-1` at `0x8012326C`, passed on as a direction, and flips the time compares at `0x80124304`, so it reads as backwards play.
8. **Then:** event 6 in state 80 plays motion 2 at +128 plus the current heading, state 81. Event 6 in 81: state 1. Event 7 in 80 and 81 has the same `0x80032AD0` gate (`0x8005B0AC`, `0x8005B250`), so this chain is the event 6 path.
9. **If there isn't one:** heading is the marker's, no motion.
10. **Something comes along:** `0x8004305C` before the load reads what is linked at the player's data, and `0x80043380` then calls `0x8006C008` on a kind 32 object and sends event 84 (`0x80043688`, `0x800436A4`). What the object is wasn't traced.
11. **Event 62** to nodes in the new room (`0x80152780`), unless `r13-26508`.
12. **Camera:** `0x801EE998(hash, 2, 0)`, `0x801EE764` on the record at `0x80603288`, then `0x801ECCE0`, `0x801EEB58(2)`, `0x801EEA9C`.
13. **Script:** the 5th argument, `0x801642D4(script, 0, 0)` (`0x801527E0`). Then `r13-26280` goes to 0.

## The other arguments

### 5th, script after arrival

Run by `0x801642D4` if above 0 **[read]**. 72 distinct values: 69 positive ids, plus 0, -1 (`L06/1635`) and a variable (`L4`, script 858) **[proved]**.

| Script | Raw calls | What it is |
|---|---|---|
| 0 | 326 | None |
| 1161 | 242 | `fn4(0)`, `fn8(Camera01, 0, 2, 0, 0)`, `fn3(20)` |
| 2004 | 126 | Levels 09 and 16, a long script. Not read through |
| 868 | 97 | `fn209(255,255,255,255)`, `fn112(0,0,0,0,40)`, then a script by player model |
| 1828 | 39 | Levels 07 and 13, branches on `fn127()` |
| 1162 to 1170 | 83 | Camera scripts. No 1166 in use |
| 1061 | 18 | Camera script |

- **26 ids hold only `fn2`, `fn3`, `fn4`, `fn8` lines** **[proved]**, 1161 among them. `fn4` writes a float to +56 of the camera record `0x80603288` (`0x80168504`, `0x801EB97C`), the record `0x801EE764` sets, so **1161 is camera-only** **[read]**.
- **Camera in those scripts against the 3rd argument:** raw calls 356 same, 21 different. Distinct calls 275 and 20. Counting only ids present in the caller's own level, raw 354 and 21 **[proved]**.
- **46 raw calls name a script id that doesn't exist in the caller's own level**, for example `L00/1413` to 1828 **[proved]**. What the game does then wasn't traced.
- **The 5th script can replace the arrival fade [read].**
  - `fn112(r,g,b,a,n)` reaches `0x801E7C88` at `0x8016BD30`, and `fn209` sets the fade colour through `0x801E7D24`.
  - Three scripts call `fn112` before any `fn1` or `fn36`: 868 at line 4 with `(0,0,0,0,40)`, `L02/424` at line 7 with `(0,0,0,255,1)`, `L11/1805` at line 8 with `(0,0,0,255,1)`. For those, 10 ticks to transparent doesn't hold. The last two sit behind `if` checks.
  - Six more call it later: 889, 1387, 1390, 1391, 1521, 1599 (`arg5_scripts_with_fn112`).

### 6th, flags

Bit tests **[read]** at `0x80152670` (mask 2) and `0x80152680` (mask 1). Counts **[proved]**: 1072 use 0, 10 use 2, 1 uses 1.

| Bit | Effect | Used by |
|---|---|---|
| 1 | Arrive at saved place slot 0 (`0x8016490C(0)`, table `0x805CC2BC`, 6 bytes a slot) instead of the marker. `fn104(slot, object)` writes it. Every one of the 152 `fn104` calls names slot 0 | `L06/1838`, with transition 0, script 2252 |
| 2 | `0x80043BE8(0)`: `r13-26276` goes to 0, and `r13-26272` too when `0x801F62B8()` equals `0x801F6258()` | `L09`, `L16` scripts 1183, 2090 |
| Others | Passed to `0x80152298`. No script sets them | Game code: 4, 32, 68, 76, 92, 128 |

- **Bit 1 leaves the heading unset [read].** `0x80152690` jumps past `0x80151914`, so `16(r1)` is never written, yet `0x80152730` (or `0x80152704` with pi added) passes it to `0x80124BE4`. `L06/1838` arrives with whatever is on the stack, unless script 2252 corrects it. 2252 calls `ed181`, which wasn't read.
- **Bit 2 isn't one skipped update [read].** Both words are 1 in all 12 saves **[proved]**, and stay 0 until written. `ed293` (`0x80009C58`) writes them: `L09/2090` and `L16/2090` call `ed293(1)` at line 105. 1183 has no `ed293` of its own.
- **What bit 2 skips [read]:** `0x80007460` to `0x80007490` runs on if `r13-26272` is set and this is `0x801F6258()`'s node, or if `r13-26276` is set and it isn't. Otherwise it skips `0x80045BC4` and `0x801F5D48`. `0x801F5D48` sends events 3 and 4. An hle.c comment calls `0x80045BC4` the player controller, which isn't checked further here.

### 7th, optional

1 if absent (`0x801666B8`). Nine calls pass 0: `L00/1564` twice, `L06/1838`, `L09` and `L16` 1183 twice each, `L11/1521`, `L11/1805` **[proved]**. All nine name their own script as the 5th.

**Effect [read]** at `0x80154D20`: when not 0 and `r13-26280` is 0, it calls `0x80118CB4` (object +592), `0x80043E78`, and `0x80043F14` for one node. That the second is "what they hold" isn't proved.

## Every other way a room changes

**10 sites call the ticket writer [proved]:** two in `fn66`, one for `ed29`, **seven in game code**. `grep -E 'bl +0x80155770' build/ed-db/disasm.txt`. Arguments **[read]**, in `ticket_sites`:

| Function | Site | Transition | Countdown, delay | Notes |
|---|---|---|---|---|
| `0x800E8C9C` | `0x800E8D1C` | 17 | 1, 1 | `ed29` (`0x8000F70C`), one script id, 1023, in 15 levels **[proved]**. Also game code at `0x800692CC` when the room is 83. Room from data +92, callback `0x800E8B48` |
| `0x800E8E14` | `0x800E8FE4` | 16 | 190, 128 | To room 83. In kind 39's handler `0x800E860C` |
| `0x8006BFAC` | `0x8006BFF4` | 17 | 1, 1 | Callback `0x8006B45C`. Called from `0x8006B438`. Purpose not traced |
| `0x8009B760` | `0x8009B808` | 0 | 3, 2 | Room = object +76 + 1000. No `bl` calls it. Purpose not traced |
| `0x800A2548` | `0x800A281C` | 17 | 1, 1 | Flags 4. Only if a linked object's room isn't the current room. Callers: `0x800A3268`, `0x800A32A8`, `0x800A3B48` (kind 50), `0x800A475C` (kind 82), `0x80065FF0` (not traced) |
| `0x800ADA24` | `0x800ADAB8` | 17 | 1, 1 | Flags 68, callback `0x800AD8A0` |
| `0x800C04EC` | `0x800C0594` | 0 | 0 made 1, 0 | Room 252, script 2779, when the room is 255. Called at `0x8005876C` |
| `0x8012D1A0` | `0x8012D2EC` | 17 | 1, 1 | Flags 32, room = record +120, callback `0x80152488`, entry -2. Called from `0x8012A8A4` |

Direct calls, no ticket **[read]**:

| Where | Calls | Notes |
|---|---|---|
| `0x801525D4` | `0x80152298` and `0x80152488` | From `0x80050174`. Flags 128 |
| `0x800ADDCC` | `0x80152614` | Entry and camera hashed from strings |
| `0x800AD6F4` | `0x80152298(room, 68)` | |
| `0x80110DA8` | `0x80152298(current, 76)` | The Tome giving the room back (`tw_keep` comment, hle.c) |
| `0x80157458` | `0x80152298(current, 68)` | |
| `0x800ADADC`, `0x800AE61C`, `0x8004FC3C`, `0x80020328` | `0x80152298` | Purposes not traced |

- **Entry -2** means the first marker with flag `0x10`, or the first marker if none has it (`0x80151AC8` to `0x80151B00`) **[read]**.
- **`0x80152488`** is the callback form of arrival. It starts its script through `0x80163C6C` at `0x801525B0` **[read]**.

**Teleports [proved]:** the TELEPORT prompt is asked 0 times.

- 20 regions named `TeleportRgn` or `TeleporterRgn` run 858 or 1010, which use plain `fn66`.
- The 4 `TeleportRegion` in room 83 run 381, with no `fn66`.
- 3 regions named `Teleport*` run 2778, with no `fn66`.

## Things the kit has wrong

| Where | Says | Actually | Status |
|---|---|---|---|
| `rooms.json`, `ed room` | Marker lists | **Short in 31 rooms, one extra in room 70.** 26769 markers against 26916. Room 93 has 70 of 106, room 274 has 59 of 95. `build_db.py`'s `RECORD_KINDS` drops a name when the byte before it is ASCII. `rooms_json_marker_differences` lists them | [proved] |
| `rooms.json` | Region scripts | Lacks 414 regions, see `verbs_regions.py` and `regions_summary.json`. This report now uses `regions.json` | [proved] |
| `primitives.tsv` `fn41` | `go_room_a` | Sets bits on records matched by name hash at +2 of 272-byte records. slot3 has `RI_FIRE01.IFL`, `RI_OPASFIRE01.IFL`. "Texture list" rests on those names | [proved] for the code and names |
| `primitives.tsv` `fn71` | `music` | `0x80155600` stores the room in table `0x8058AF60`, then `0x80154EC4(0)`. 703 of 1056 literal `fn66` calls have `fn71` of the same room. "Preload" is a reading | [read], count [proved] |
| `primitives.tsv` `fn104` | `sound_at` | Saves an object's place to a slot and returns it (`0x8016AD10` to `0x8016AD70`) | [read] |
| `primitives.tsv` | Nothing | `ed196` and `fn4` aren't listed | [proved] |
| Research brief | `0x80152658` as a function | Mid-`0x80152614`, after `bl 0x80152298`. Nothing branches to it | [proved] |
| Research brief | `ed85` as a room change | `0x80145760` calls `0x801413E8` and `0x8019DB90(536, 100)`. "Effect object" is a reading | [read] |
| Research brief | `0x801EC1C8` as arrival | Follows a run of record +68's script in `0x801EBF4C`, on a record compared against the camera block | [read] |

`0x80134B38` is the return address of a script run (`0x80134B34`). It compares the room before and after and stops scanning if it changed **[read]**. hle.c calls it `ARRIVAL_ZONE_RAN`.

The earlier version of `transitions.py` tested the name `GravityMkr`, used `rooms.json`, and flagged a script only if every room using it lacked one. All three are fixed, and the numbers above come from the corrected script.

## For two players [read, none of it run]

From `runtime/host/hle.c`:

- **`coop_door_motion`** hooks `0x801669AC`. When player 2 opened the door it clears their `0x8` bit as `fn66` does for player 1, and plays the motion on them.
- **`coop_follow`** moves player 2 to player 1's room. If they opened the door they take the arrival spot. It sets state 80 on them if they are in 121.
- **It sends no arrival events.** No event 238 or 191 appears in hle.c. So player 2 doesn't get the map, the backwards motion, or `ed196`'s override.
- **It does give them a heading.** It copies player 1's object +44 to +59, and `0x80124BE4` writes the heading to object +44. So player 2 faces wherever player 1 faced when `coop_follow` ran. Whether that is after `0x80152614`'s heading wasn't proved.
- **It watches object +592.** A `fn66` with 7th argument 0 doesn't write +592, so on those nine calls `coop_follow` won't see a change from that word.
- **Event 61 reaches nodes in the old room,** where player 2 still is by code order. In state 121 it falls back to state 0's `0x80129878(obj, 1)` (`0x800590D8`). In state 80 it stops the motion (`0x8005B0D8`). The comment above `coop_follow`'s state 80 says being told the event stopped them mid motion. That the broadcast reaches them wasn't run.
- **Event 62 goes to nodes in the new room,** so by code order player 2 misses it unless `coop_follow` already moved them.
- **The game moves something else along itself:** `0x8004305C` and `0x80043380`. Whether that could serve player 2 hasn't been checked.

## Not established

These failed review or could not be settled. Don't use them as fact.

**Stated earlier and wrong:**

| Earlier statement | What is wrong |
|---|---|
| State 121 goes to state 1 on event 6 or 7 | Event 7 is conditional on `0x80032AD0` (`0x8005AEE8`) |
| State 121 swallows most other events | It returns 1 for 11 events. Others fall back to state 0 |
| `r6=5` is a blend | It is stored at channel +250 and compared. No sign it is a blend. The blend length comes from motion byte +12 through channel +257 |
| 427 of 430 `GravityMkr`, 83 rooms | 429 of 432, and the test is the flag: 85 rooms. Caused by `rooms.json`'s gaps and a test on the name |
| Only door script `L05/421` is affected by a missing marker | 15 ids, because of an `all()` in the script |
| Fade-in is always 10 ticks whatever happens | True of the game's own call, but the 5th script can replace it |
| Event 238 maps by the leaving motion | It checks +124 first and then reads the motion current at that moment |
| Game code has eight ticket sites | Seven |
| 275 and 20 calls name the same or another camera | Those are distinct calls, set beside raw counts. Raw is 356 and 21 |
| Motion numbers as general facts about players | They differ by model and save. The coop saves' 103 doesn't rise, and 104 isn't a plain fall |
| `coop_follow` means player 2 misses the heading change | They miss the event, but get player 1's facing copied |

**Not settled:**

- **Whether scripts have only two routes to a room change.** Only `fn66` and `ed29` reach the ticket writer, `0x80152298`, `0x80152614` or `0x80152488` by `bl` and tail branches, per the skeptics' separate call graphs. That doesn't follow events or handlers registered by address. There is a live candidate: `fn79(handle, 135, 0)` in `L09/1010`, `L12/1010`, `L13/1010`, `L12/2778`, `L12/2792`. The handle is `fn78(1, 0)`, "the player as scripts see them". Kind 50's handler answers event 135 with `0x800A2548`, which writes a ticket if a linked object's room differs. `L12/2778` calls the same handle `SummonedMonster` and runs from three `Teleport*` regions. Whether the player handle is ever a kind 50 node wasn't proved.
- **Whether a script can start the cinematic that ends in `0x8012D1A0`.** The record holding +120 wasn't traced to its source, and unpacking a `cin` file passed 180 seconds.
- **Whether arriving through a door plays the motion backwards facing the door.** The code reads that way, but it wasn't watched. It needs motion 25 still current at the room change, which needs one update per tick.
- **What climbing looks like on arrival, and "rises to standing".** Motion 104 at 60 updates may already have ended at 62 ticks, and then the map returns -1 on whatever state 1 plays.
- **That the first motion track is the root, or its third short the height.** No code read shows it.
- **That motion 2 is a walk, or 25 a door opening, on screen.** Usage only.
- **What rooms 1000 and up are for,** and who runs `0x8009B760`.
- **Seconds per tick.**
- **Motions' update rate per tick:** `0x800271B4`'s caller wasn't followed.
- **What motion events do,** and **what motion flag 0x100 does** beyond `0x80123F38` and `0x80124448`.
- **Door sound.** Motion 25 has no events. 24 of 271 distinct door calls sit in scripts with `fn47`. A normal door's sound wasn't traced.
- **What motion 99 is.** Not loaded for any saved player. Only player 1 was read in each save.
- **Why blank tickets exist in slot9, slot10 and the coop saves:** `0x801540AC`'s caller wasn't followed.
- **`fn41` as a texture list, `fn71` as a preload, and what `0x801413E8` makes:** the code is read, these names are readings.

## Provenance of this revision

The numbers above come from a run of the corrected script. `tools/ed/facts/checks/reconcile-trans/run.txt` holds its output, and `transitions.before.*` beside it hold the earlier versions. Nothing under `saves/` was written, and the game wasn't run.
