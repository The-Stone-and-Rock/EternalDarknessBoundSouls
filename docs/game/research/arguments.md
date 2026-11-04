> Kept as the evidence behind `docs/game/systems/`. Reports were checked by skeptics before the kit was built. Where a report says the kit is wrong, the kit has since been corrected. `tools/ed/facts` holds the scripts it cites, and `ed build facts` remakes the tables.

# Unknown arguments: what survived checking

Three skeptics checked the first version of this report, and every disagreement was then settled by looking at the code and data again. What failed is in "Not established" at the end.

Run commands from the repo root. Helpers are in `tools/ed/facts/checks/args/`, and the reconcile's own checks are in `tools/ed/facts/checks/reconcile-args/`.

**Marks**

- **proved:** checked against live memory in a save, or data and code agree.
- **read:** from the code alone, nothing run.
- **inferred:** from usage.
- **(R)** means no skeptic was asked about it. It was re-read in the reconcile by one reader only.

**Short forms:** `TA[n]` is `0x80602B18 + n*136`, `TB[n]` is `0x80603178 + n*136`. A handle is a thing's id, node `+16`. See `docs/game/research/item-ids.md` for handles against type ids.

## 1. ed219, ed220, ed221

| Call | Mode written to `0x803090C4` | What the player gets | Mark |
|---|---|---|---|
| `ed220(group, entry)` | 1 | Words only, no close-up | read |
| `ed219(group, entry, camera hash, n)` | 2 | Close-up, Details, words, Return | read |
| `ed221(group, entry, camera hash, n, X, Y)` | 3, or 1 or 2 once X is gone | Close-up, Details, words, **Pick Up** | read |

**X is a thing's tag, Y is the fallback entry.** They are not flags or scripts.

| Call | The tag is | Mark |
|---|---|---|
| `ed221(69, 3, Camera09, 0, 2029, -1)`, script 1263 | Level 2, room 217, type id 21 Bronze Necklace, model 150, at -302, -608, 169 | proved: disc record, item table, and region SphereRegion01 in room 217 |
| `ed221(123, 3, hash, 4, 270, -1)`, script 2698 | Level 6, room 172, type id 21. Tag 270 is also a section 1 record in room 303, so the room filter picks | proved: disc record, region NecklaceRegion01 in room 172 |
| `ed221(96, 3, Camera04, 3, 633, -1)`, script 1293 | Level 3, room 147, type id 31 Blue Urn, at -901, 116, 70 | proved: slot 1 handle 62 has tag 633, room 147, type id 31 |

- **Calls checked, proved:** 118 of 124 calls name a tag in a placement file. For 96 unique (script, X) pairs, 87 have the record in the room of the script's region, and none disagree. `python -I tools/ed/facts/checks/args/things_build.py` prints the count.
- **The other 6 calls, proved:** 974, 720 (2 calls) and 2057 (2 calls) are `ed3` spawn tags, in scripts 2531, 424 and 2026. Tag 2042 (L08 script 2182) appears nowhere else, so it is not found.

**How X and Y work** (`0x800174A0`, read unless marked):

1. `0x801F6160(X, r13-23276)` walks every thing and wants node `+36`, then `+24`, equal to X, and object `+592` equal to the room. It returns the handle, or -1. Room -1 means any room. A thing with node `+52` of 0 skips the room test (`0x801F6198`).
2. Handle of 0 or below (`0x800174EC`): mode becomes 1 if `n & 2`, else 2, and the entry becomes Y.
3. Two more triggers take the same fallback and clear `+28`: the thing is in the player's carried list (`0x801F9228`), or its node type is 30 with node `+20` of 36 (`0x80017514`).
4. Otherwise mode stays 3 and `+28` holds the handle. Mode also stays 3 with the original entry if `0x801F5F88` returns 0 or the node's `+48` list is empty (`0x800174F8`, `0x80017530`).
5. An entry below 0 means no prompt at all (`0x800176BC`).

**Y is proved.** There are 116 calls with Y of -1 and 8 with Y of 0 or more. `python -I tools/ed/facts/checks/args/ed221.py` prints the words as pairs: SPICE_JAR then PANTRY_CONTENTS, SPELL_DESK then TABLE, RECOVER_SPELL then CORPSE, SHEET_MUSIC then BOOKSHELVES, MAGICK_RECOVER then ALTAR, BIND_SPELL then BONE_SHRINE.

**n is bits** (read):

| Bit | Effect | Evidence |
|---|---|---|
| 1 | Blend into the close-up instead of cutting: passed as `c` to `0x801EE8E0`. If clear, closing snaps back with `0x801EEA9C` | `0x80024FD0`, `0x80025304`, `0x80025138`, `0x80025530` |
| 2 | ed221 only: the fallback is words with no close-up, mode 1 rather than 2 | `0x80017558`, `0x800175A8` |
| 4 | `0x80043C88(0)` on opening, `0x80043C88(1)` on closing | `0x80024F58`, `0x8002528C`, `0x80025150`, `0x80025548` |

Literal n values (proved from the scripts):

- ed221: 1 (72 calls), 4 (27), 0 (12), 3 (4), 6 (4), and `LIGHT_AFTER_EXAMINE_TWEEN` (5).
- ed219: 0, 1, 4, and a variable in L05 script 2039. No script passes 2 to ed219.

**What bit 4 reaches** (read):

- `0x80043C88` stores its argument + 1 at `r13-25688`. `0x80043CCC`, called at `0x80008214`, then runs `0x80043D70(argument)` once.
- `0x80043D70` works on the handle at `r13-21488` (`0x801F6258`), not on the examining player. It stores `r13-26272`, then calls `0x801C588C` and `0x801D5D60`.
- With 0, `0x80043DE8` calls `0x801258BC(object, 15, 0)` on each thing in the list from `0x80151500(handle)`. With 1, `0x80043FD0` does the same with 1, and more.
- **For co-op, proved in every save with a list:** `r13-21488` and `r13-21496` both hold 1, so nothing in the saves separates them.

**The game changes n itself** (read):

- Mode 2: if `0x801F0638` or then `0x801F077C` returns nonzero, it sets 4 and clears 1 (`0x80024EF0`, `0x80024F18`, `0x80024F30`).
- Mode 3: it tests only `0x801F0638` (`0x8002524C`).
- A nonzero result from `0x800255A4` clears 1 (`0x80024F98`, `0x800252CC`).
- What those three functions test is not established.

**The record, 32 bytes at `0x803090C4`** (read, R). It is copied to look 0 at `0x80307108` or look 1 at `0x80307128`.

| Offset | What | Written at |
|---|---|---|
| +0 | Mode. 0 when finished | `0x8001769C` and others |
| +4 | Script that asked, `r13-23228`. Run again when the look ends (`0x8002558C`) | `0x80017104` |
| +8 | Press counter, moved by `0x80024D78` | `0x800170FC` |
| +12, +16 | Group, entry | `0x8001711C`, `0x8001713C` |
| +20 | Camera hash | `0x800172F4` |
| +24 | n | `0x8001730C` |
| +28 | Handle of the thing to pick up | `0x800174E8` |

Other reads (read, R):

- **Which look:** `0x80117404(2, 6, -2, script, 50, 2, 0)` returns a prompt slot. Slot 2 copies to look 0, slot 4 to look 1, and 3 or 5 and up copy nothing (`0x800176E8`).
- **Gates:** ed219, ed220 and ed221 do nothing if `0x8006DB04(0x4000)` is set (`0x80017374`, `0x800171A4`, `0x80017668`). ed221 also needs bytes `+158` and `+159` of the player's node `+40` to be 1 (`0x80017674`).
- **`0x8006DB04(mask)`** is word `+32` of (`+196` of the type 32 thing's `0x8006A4A8` result), ANDed with mask. `0x8006C750` sets `0x4000`. That is the one site found in `0x8006A000` to `0x8006F000`; the rest of the code was not searched.

**Presses** (`0x80024D78`, read; prompt numbers from `python -I tools/ed/ed.py prompts`):

| Count | Mode 2 | Mode 3 |
|---|---|---|
| 0 | Push camera `0x801ECD68`, camera 10, Details (34). Entry below 0: Return (7), jump to count 3 | Same open, Details (34) |
| 1 | Words through `0x801DB374(r13-26496, group, entry)` | Same words |
| 2 | Return (7) `0x800250F8` | **Pick Up (19)** `0x800253EC` |
| 3 | Close `0x80025114`: pop camera `0x801ECCEC`, `0x80199EA0(1)`, mode 0, re-run script | Pick up `0x80025408`, below |
| 4 | | Close `0x800254F8`: pop camera, mode 0, re-run script. No `0x80199EA0(1)` |

Mode 1 opens words and clears the mode at once (`0x80024DF4`). ed219 with entry -1 (83 calls) goes straight to Return (`0x80025004`). ed221 falling back to mode 2 ends with Return.

**The pick up at count 3** (read):

- `0x801F9DE8(thing, player, 30)` does nothing if the thing's node type is already 30. Otherwise, if its node type isn't 0, it asks `0x801F59B0(240, player, handle, 30)`, and a nonzero answer refuses.
- If not refused it sets node `+12` = 30 and `+33` = 1, so the node type changes on the next flush (`0x801F51DC`).
- Then come the thing's own words from `r13-26492`, and `0x80024528`.
- Hard-coded case at `0x80025468`: type id 31 (Blue Urn) with flag 266 off calls `0x801A4884(12, 100, 6, 1, 0, 5, 0, 0)`.

## 2. Cameras

**The records, proved.** `TA` is the look-at target and `TB` is the eye, 12 of each, 136 bytes (loop `0x801EB9FC`, `0x801EBA3C`).

- `TB[n] +116` points at `TA[n]` in all 12 saves checked.
- Slot 1: view words 0 to 2 at `0x803012E0` equal `TB[2]` (470, -145, 359), and words 3 to 5 equal `TA[2]` (-112, 85, 283). The view's forward vector is target minus eye, normalised.
- `0x806037D8` is the shown eye. It equals the view with 0.0 difference in all 12 saves. `TB[current]` equals it only once settled: 20.93 off in slot 5, and 1,032 and 1,245 off in slots 10 and 9, which use camera 8. Check: `python -I tools/ed/facts/checks/reconcile-args/r9.py`.

| Index | Use | Mark |
|---|---|---|
| 2 | Region cameras: every fn8 and fn10 writes `TB[2]` (`0x80166D7C`) | read, R |
| 8 | Current in slots 9 and 10 | proved |
| 10 | Examine close-up (`0x80024E94`) | read |
| Current | `r13-26864`, set by `0x801EEB58(n)` | proved in saves, setter read |

Record fields:

| Offset | Field | Mark |
|---|---|---|
| +0, +4, +8 | Position, floats | proved |
| +12, +16 | Frame on a timed path, camera's index in the room. Written only when a timed path is set (`0x801EE4CC`, `0x801EE524`). Slot 1 reads 150 and 3 with index 0 current, so they can be stale | read, R |
| +20, +24, +28 | Current key, direction ±1, step per frame | read, R (`0x801EE4B8`, `0x801EE4FC`) |
| +32 | State: 3 on setting, 5 waiting to play, 1 playing (`0x801EC990`), 4 fixed or ended (`0x801EE730`, `0x801ECC74`). **2 (`0x801EC1E0`) means an eye in state 3 has arrived**, and fn11's script runs then. Most saves show eye 2, target 3; slot 5, still settling, shows eye 3 | proved for 2 and 3, the rest read |
| +36 | How the eye picks its place on its path, b below | proved for 2 and 0 |
| +40, +44 | Follow offsets. Defaults set by `0x801EB7A0`. fn6 and fn7 set them, guarded on `r13-26524` not 0 | read, R |
| +48, +52 | Blend divisor, FOV | proved (FOV 65.25, divisor 20 in slot 1) |
| +56 | Follow value, fn4 sets it (`0x801EB97C`) | read, R |
| +64 | On the shown records: 1 means snap this frame (`0x801EE8B4`, `0x801EBA58`) | read |
| +68 | Script to run on arrival, fn11 sets it | read, R |
| +104 | Who it follows, 0 meaning `r13-26524` (`0x801EC7A4`) | read; slot 10 has target `+104` = player object |
| +108 | Per-frame step function | proved (`0x801EC9AC` in slot 1) |
| +112 | Shown records: destination record (`0x801EBF64`) | read, R |
| +116 | Eye records: paired target | proved |
| +120 | Path position, a float frame | proved |
| +124 | The room's camera entry | proved (slot 1: `0x807FDA40`, Camera01) |
| +128 | Called once when a timed path may start: `0x801EC97C` | read, R |

**`fn8(hash, a, b, c, d)`** goes `0x801EE8E0`, then `0x801EE764`. Uses across 2,801 calls (proved, `IDX=1 python -I tools/ed/facts/checks/args/calls.py fn8` and neighbours):

- `0,2,1,0`: 821
- `0,0,0,0`: 770
- `0,2,0,0`: 535
- `0,0,1,0`: 511
- `1,0,1,0`: 116
- `1,0,0,0`: 43
- 5 with variables: `L1,L2,1,0` four times in script 1594, `0,L3,0,0` once in L04 script 2125

| Arg | 0 | Not 0 | Mark |
|---|---|---|---|
| a | Target stays on its path or point | Target follows: `+104` = `r13-26524`, `+108` = `0x801EC704` (`0x801EE694`, `0x801EE73C`) | read |
| b | Eye takes the **nearest** key to the follow point (`0x801EDA40`, `0x801EDB18`). Any value but 1 or 2 means this | 1: **farthest** (`0x801ED0FC`, no literal use, no live case). 2: **same path position as the target** (`0x801ED6F4`) | 2 and 0 proved: eye and target have the same `+120` in every save with a moving room camera (`python -I tools/ed/facts/checks/args/cam_live.py`). 1 read |
| c | **Cut.** Sets `0x806037D8 +64` (`0x801EE8A4`) | **Blend** | read |
| d | Nothing | `0x801EB7A0(record, 0)` on both records (`0x801EE838`). This restores settings `+36, +40, +44, +48, +52, +56, +60, +68` and keeps position and `+104`. Only `0x801EB7A0(record, nonzero)` clears and sets `+104` to the player, and fn8 never passes that. Literal d is 0 in every call | read; d's literal use proved |

Other natives (read, R):

| Native | Does | Evidence |
|---|---|---|
| `fn2(n)` | Blend divisor of `0x806037D8` and `TA[2]` | `0x801683C4`, `0x80168428` |
| `fn3(n)` | Blend divisor of `TB[2]` and the shown target | `0x80168450` |
| `fn4(x)` | `TB[2] +56`. Only ever called with 0 | `0x80168504` |
| `fn6`, `fn7` | `+40` and `+44` of `TB[2]` | `0x8016869C` |
| `fn10(hash, reverse, c)` | Timed play from first key, or last if reverse. Passes r8 = 1 and r9 = reverse to `0x801EE8E0` | `0x80166D94`, `0x801EE498` |
| `fn11(script)` | `TB[2] +68`. Runs once the eye and target are both in state 4 or 5, or the eye is in state 3. Cleared when a new path camera is set | `0x801EC1B0`, `0x801EC1E4` |

`fn174(handle)` (read by all three skeptics): handle to node to object, then `0x801EB778(2, object)` writes `+104` on `TA[2]` and `TB[2]`. **It names who the camera follows.** fn8 with a nonzero overwrites the target's `+104`.

**Stack** (read, R): `0x801ECD68` pushes the camera (shown records' divisors and destinations, current index) to 20-byte slots at `0x806038E8`, slot `r13-21700`, and `0x801ECCEC` pops it. Examine pushes at `0x80024E90` and pops at `0x8002511C` and `0x80025514`.

**Blending** (read, R): `0x801EBF4C` moves each value by (value - destination) / divisor per frame. "Arrived" is `0x801EC108`, where each position difference over divisor truncates to 0.

**Paths in the room file, proved.** `python -I tools/ed/facts/checks/args/cam_all.py` walks every room's array; the laid-out sizes below are all it assumes. It writes `args/cameras_all.json` (`count`, `eyes`, `targets`, per room).

- **Eye array:** n entries of 120 bytes, n the u16 at file `+162`. Eye keys follow it, then the target array, then target keys.
- **The walk fits exactly.** File `+164` and `+172` keep build-time addresses (little endian), and their difference equals the walked size in all 297 rooms. On the 288 rooms with an address at `+180`, the walk ends exactly there. 0 disagree.
- **Live, proved:** `python -I tools/ed/facts/checks/args/cam_check.py` compares hash, positions, key counts, first and last keys to live memory in 12 saves across rooms 21, 100, 115, 147, 151, 152, 153, 155. **0 cameras disagree.**
- **Target entries** have no name and no hash (live, slot 1) and FOV 0 (2,177 of 2,177 in the file).

| Entry offset | Field | Mark |
|---|---|---|
| +0 | x, y, z as s16: **the first key only** on a moving camera | proved: frame 0 equals pos in 1,628 of 1,628 moving eyes and targets |
| +8, +20 | Axis, angle | read, R; see `+112` |
| +32, +36 | Far, FOV | proved |
| +52, +56 | Hash, name | proved |
| +96, +100 | Count, then byte size. No camera on the disc uses these | proved: 0 of 2,177 eyes, 0 targets |
| +104, +108 | Count, then 12-byte keys, u32 frame then s16 x, y, z. File `+108` is the byte size, (n+1)*12, a pointer once loaded | proved: 1,628 of 1,628 |
| +112, +116 | Count, then **20-byte** keys, u32 frame then 4 floats. File `+116` is (n+1)*20. Frame step is 5 in 796 of the 807 arrays. In room 147 Camera01 the first key's floats are the axis times 4, then the angle, so they look like rotation | stride proved, meaning inferred |

**How many cameras, proved:** `python -I tools/ed/facts/checks/args/cam_stats.py`

- 2,177 eyes in 297 rooms. **814 move (37.4 percent)**, 1,363 are still.
- Eye and target have equal key counts in 2,154 entries and differ in 23.
- A moving eye's last key is never its `pos` (814 of 814). Room 147 Camera01 goes from 470, -145, 359 at frame 0 to -91, -63, 570 at frame 395.

**The kit's cameras are wrong in three ways, proved** (`tools/ed/build_db.py`, not edited, it isn't mine):

| Fault | Where | Effect |
|---|---|---|
| The lookbehind on `RECORD_KINDS` rejects a name whose hash ends in a letter, digit or `_` | Line 21 | 124 `Camera##` entries lost, all 124 have such a byte. Room 24 Camera11, hash 0x217C304D |
| The camera test accepts only `Camera##`, `Cam_`, `CAM`, `Finishcam` | Line 128 | IntroCam, PuzzleCam, RuneCam and others lost. In total **989 of 2,177 eyes are not in `rooms.json`**. Room 147 has 24, the kit lists 5 |
| `path` reads `u32(x, i - 88)`, which is entry `-32` | Line 132 | 30 flagged, 8 of them still. Right test: u16 at entry `+104`, name `+48`. 619 of the eyes it lists move |

**fn8 hashes, proved:** of 2,774 calls with a literal hash, all 2,774 are in `cameras_all.json`, and only 1,740 are in `rooms.json`. Also, `pos` is not where a moving eye is: slot 2 Camera01's eye is at 243.5, -34.6, 1150.1 against `pos` 470, -145, 359.

## 3. Natives

**fn25 is the current room, proved.** It returns `r13-23276` (`0x8016A9D4`).

- It equals the player object's `+592` in all play saves, across rooms 147, 21, 152, 151, 153, 155, 115, 100 (`python -I tools/ed/facts/checks/args/models.py`).
- In script 1429, 153 and 155 are rooms. Branch 153 tests CameraRgn03, and branch 155 tests PlateRgn and UrnExamineRgn00 to 03, which `rooms.json` has in those rooms and room 147 doesn't.
- Of 160 distinct values compared in scripts, 159 are rooms in `rooms.json`.
- **Not a room file:** 403, in L09 and L16 script 61. No `rmed` file exists for 403 or 408.
- Writers: 15 by `grep -- "stw .*-23276(r13)" build/ed-db/disasm.txt`, including `0x800157E0`, which stores the start room from `0x8023D738`.

**fn78(kind, id)**, switch at `0x80165D34` (read). Use counts are proved. All 3,765 calls have a literal kind.

| Kind | Returns | Uses |
|---|---|---|
| 1 | The controlled thing, `r13-21496`. id ignored | 1,394 |
| 2 | Thing with tag `id` in the current room | 1,847 |
| 3 | Thing of **node type** `id`. The loop has no break, so the last in the list | 218 |
| 4 | Thing with tag `id` in any room | 181 |
| 5 | Thing with tag `id` in the controlled thing's carried list (`0x801F60BC`) | 125 |
| 6 | Handle (`+16`, `0x801F62C8`) of the node passed as userdata | 0 |
| Other, or a result of 0 or below | nil (`0x80165E10`) | |

None of these checks node type, so a type 0 thing with a tag is still found.

**fn79(thing, event, delay, extra)** at `0x80165E5C` (read). 599 calls have 3 arguments, 539 have 4, and event is literal in all 1,138.

- It calls `0x801F57C0(event, 0, thing, extra, delay)`. Extra defaults to 0, and thing 0 does nothing.
- **Time:** delay + `r13-22216`. The clock ticks at `0x8001871C` once per main loop, unless `r13-26240` is 4, and also inside two wait loops (`0x801A3174`, `0x801A5034`). That one tick is one frame is inferred.
- **A delay of 0 is not queued.** `0x801F5000` delivers inside the call. Only a later time goes to `0x801F5310` (`0x801F50A4`).
- **Delivery** calls `0x806068A8[node type * 16]` (`0x801F52E0`) with node `+20` as the state. If unhandled it calls again with state 0 (`0x801F5108`).
- **Then** pending state and type changes (node `+32`, `+33`) are flushed with events 2 and 1 (`0x801F514C` to `0x801F5260`).

`python -I tools/ed/facts/checks/args/handlers.py saves/slot1.state` lists the table. `args/events.txt`, from `args/events.py`, lists events per type by pattern, so treat it as a place to look.

Events are read at the addresses given. These confirm where the code goes, not its meaning.

| Event | Uses (proved) | Where read |
|---|---|---|
| 154 | 306 | Type 29: stores extra at node `+36`'s `+52`. Then state 14 sends 18 and state 13 sends 19, unless flag 555 is on (`0x801F7A88`, `0x801F7CFC`) |
| 57 | 243 | See below |
| 82 | 198 | Type 32: `0x8006E00C` (`0x8006D564`), with a second compare at `0x8006D3FC`. On 39 not read |
| 18 | 135 | Type 29 in state 14: `0x801F7020` (`0x801F7A30`) |
| 6 | 37 | Type 29, state 0 block: runs the script at (node `+36`) `+28` (`0x801F7724`, `0x801F6308`, `0x801F63F0`) |
| 213 | 33 | Type 79: extra is a handle, stored at `+40`, joins to it (`0x800DEA80`). One skeptic did not re-read this |
| 19 | 19 | Type 29 in state 13: `0x801F6BB4` (`0x801F7C44`) |
| 81 | 16 | Type 32: `0x8006C850` (`0x8006D5C0`). Script 1600 sends it when `ed61() == 12`, in a global it names `gInsEffectID` |
| 71 | 15 | Player: `0x800C362C` (`0x8005A7D8`), script 2246. Type 30: `0x800D87F0` |
| 202 | 15 | Player: `0x800C30CC` (`0x8005A7BC`), script 2244 |
| 16 | 12 | Type 79: extra is a handle (`0x800DE9F4`). One skeptic did not re-read this |

**Event 57 and its queue, read, with the node part proved.** Its queue reader was found.

- `0x801DCB58(op, payload)` queues. `0x80025FF0` reads, called from `0x80008548`, `0x80020430`, `0x80050ABC`, `0x800B8578`, `0x8011177C`. It walks with `0x801DCCC4`, `0x801DCCCC`, `0x801DCCD4`, `0x801DCCDC` and switches on op through table `0x8023E660`.
- **Op 1 and op 29** (`0x80026080`): build a record with `0x8014FD08` and store the thing at its `+56`. Op 1 is queued at spawn (`0x800418D4`).
- **Op 2** (`0x80026274`): `0x8014ED1C(thing)` finds that record, then `0x8014FF5C` zeroes `+24` to `+32` and sets `+68` bit 2. `0x801F6960` can cancel a pending op 2.
- **Op 28** (`0x80026F48`): `0x800339A4`, then `0x801F5EC4`, the actual delete. `0x801F5EC4` is also called at `0x8002719C` and `0x8006DAE0`.
- **Event 57:** type 29 does only op 2 (`0x801F7750`). Types 10 and 30 also write node `+12` = 0 and `+33` = 1 (`0x800291F8`, `0x800D8710`), so the node type becomes 0 at `0x801F51DC`. Handlers 9, 25, 36 and 49 compare 57 with no `0x801DCB58` within 40 lines, not traced.
- **So 57 does not remove the node, proved.** Type 0 nodes stay listed with handle and tag. Saves hold 23 to 67 of them, for example coop-apart handle 162, tag 2184 (`python -I tools/ed/facts/checks/reconcile-args/r9.py`). `0x806068A8[0]` is 0.

**Alignment**

| Statement | Mark |
|---|---|
| `ed17()` returns `*r13-26320` (`0x80010438`), set by `0x8003F4BC` as `0x8023B97C + 12*row + 4`. Table rows are `[0,0,0]`, `[3,1,2]`, `[1,2,3]`, `[2,3,1]`, so it returns the row | read, table proved from the dol |
| Every save with a player reads 1. The title reads 0. No save holds 2 or 3 | proved |
| Scripts compare it only with 1, 2, 3 (143, 143, 140 compares) | proved |
| `ed15(handle, value)` (`0x800102E0`) stores node `+40` `+148` (`0x80032128`), and if the handle is the controlled thing, calls `0x8003F4BC(value)` (`0x800103B4`). Its one script call is in L01 script 655 | read |
| **1 is Chattur'gha, 2 Ulyaoth, 3 Xel'lotath.** Script 655 sets L0 to 1, 2 or 3 by region, shows `ed12(36, L0-1)`, and calls `ed15(player, L0)`. Room 01 text 36,0 is CHATT_ARTIFACT, 36,1 ULY_ARTIFACT, 36,2 XEL_ARTIFACT, each asking whether Pious should claim it | read: script and text agree, not seen live |
| `0x80041D2C` picks a section 3 record's set by this value | read |

**`ed16(handle)`** returns node `+40` `+148` (`0x800104F4`). Proved: on players it equals alignment (`0x80031300` copies it at spawn), and every type 2 node reads 1. On others it is that thing's own field: in saves with alignment 1, type 4 nodes read 4 and 3, type 37 nodes 3, 2 and 4, type 6 nodes 2 and 3, and type 88 nodes 2 and 3. Other writers read: `0x800315B4`, `0x8003165C`, `0x800319C8`.

**The rest:**

| Native | Does | Mark and evidence |
|---|---|---|
| `fn71(room)` | Queues a room, not music. `0x80155600` looks in the 24-byte table at `0x8058AF60` and takes a slot if absent, then calls `0x80154EC4` | proved: live slots hold rooms (slot 3: 147, 21; slot 10: 115), and 252 of 253 literal values are rooms (408 is not). That it loads the next room is inferred |
| `fn73(n)` | Sets `r13-23220`; script n runs straight after this one ends | read, R (`0x80168ED0`, `0x80164470`) |
| `fn34()` | The same with the current script: it **runs itself again**, it doesn't stop. Scripts use `fn1(n); fn34()` to step at once | read by all three (`0x80168E48`, `0x80164470`) |
| `fn76(room)` | Counts things in that room with node `+56` = 1 for which `0x801F59B0(59, handle, handle, 0)` gives 1 | read, R (`0x8016CB40`, `0x80043740`, `0x800437CC`) |
| `ed143()` | Chapter from `0x80307178`. 12, 14 and any above 16 read as 0 | read, R (`0x80015808`, table `0x8023C410`, `0x800158B8`) |
| `fn104(slot, handle)` | Writes the thing's position as s16 at `0x805CC2BC + slot*6` and returns it as a pointer | read; slot is 0 in 152 of 152 calls; script 1429 passes it to `ed3` and `fn148` |

**ed3:** arguments 1 to 8 are read in order (`0x8000E4FC` onward) into a spawn record: 1 chr `+16`, 2 model `+20`, 4 type id `+24`, 5 `+28`, 6 `+32`, 7 tag `+36`, 8 `+40` (read, R). Proved on one example: `ed3(25, 56, -1, 32, -1, -1, 481, 30, 0, L1, 4)` in script 1429 gives slot 3 handle 83, type id 32, tag 481, node type 30 (`python -I tools/ed/facts/checks/args/_items.py saves/slot3.state`). Arguments 3 and 9 to 11 are not traced.

## 4. Placed things: the file the kit is missing

`Npcs<level>.npc`, in the disc's root, for levels 0 to 11, 13 and 16. Proved:

- The header is 11 words: size 0x2C, then count and bytes for each of 5 sections. Sizes sum to the file on all 14 files.
- Names: `NPCS` at `r13-30468`, format `%s%d.NPC` at `0x8023EC90`, loaded by `0x8003F354`.
- All records start with s16 x, y, z, room.

| Section | Size | Loader | Fields (spawn record offsets) | Tag at |
|---|---|---|---|---|
| 0 | 40 | `0x8004193C` | Model `+24`, chr `+26`, spawn `+40` from `+32`. Mask: low byte of `+36` | +34 |
| 1 | 104 | `0x800417D8` | Chr `+76`, model `+78`, spawn `+40` from `+80` | +98 |
| 2 | 44 | `0x80041B2C` | Pickups: model `+28`, **type id `+30`**, chr `+32`, spawn `+40` from `+38`. Mask: byte `+11` | +40 |
| 3 | 52 | `0x80041CA0` | Model, chr, spawn `+40`, one set per alignment, at `+24`, `+30`, `+36` | +46 |
| 4 | 60 | `0x80041F04` | Model, chr, spawn `+40`, **type id**, one set per alignment, at `+32`, `+40`, `+48` | +56 |

Only sections 2 and 4 hold a type id. Sections 0, 1 and 3 store nothing at spawn `+24`.

**Parsed:** `build/ed-db/facts/placed_things.json`, rebuilt with clear key names: `spawn40`, `type_id`, `chr`, `tag`, `mask`.

**Live check, proved:** `python -I tools/ed/facts/checks/args/things_build.py` prints tagged records live in the same room:

- coop-apart 105 of 105
- slot 10 42 of 43
- slot 1 39 of 48
- slot 3 37 of 48

Section 2 type id equals the live type id on 37 of 37.

**The misses**, proved as listed:

- Slot 3's 11 are 9 section 0 records with spawn `+40` of 44 and urns 633 and 632.
- Slot 1 still holds 633 and 632, and 633 is carried at handle 62. That the urn is used up in script 1429 is inferred.
- A further type 44 record, 340, in room 152, is live in slots 1 to 4 and 9. Why the others are absent is not established.

**Spawning is gated** (read, with values proved). A record spawns only if its mask ANDs with a value from `0x800421CC`:

- That value is `0x80308A89`'s bit count mapped 0 to 1, 1 to 2, 2 or 3 to 4, 4 or more to 1.
- The byte is `0x0E` in every play save, so the gate is 4 there.
- Masks are 7 everywhere except two section 3 records in level 4, masks 4 and 6, so the gate doesn't explain the misses above.
- `npcfile.npc` (72 bytes) is a different format, and its name is not in `main.dol`. Not opened further.

## 5. Levels

**Level number is the chapter number** at `0x80307178`, which equals the loaded level at `r13-30472` in every save (proved). Script packs in memory agree: chapter 3 saves hold Level03, slot 10 holds Level07, and coop-apart holds Level12 with flag 787 on.

**Columns:**

- **Model:** the table `0x8023BC2C`, read by `0x80007C8C` (read).
- **Start room:** `0x8023D738`, stored into `r13-23276` by ed87 at `0x800157E0` (read, table from the dol).
- **Who:** inferred from name counts in the level's room text (`python -I tools/ed/facts/checks/args/names.py`), and `ed166` model tests by level agree.
- **Page label:** proved. It is the item whose use script calls `ed87(n, n, 0)`.

Data is in `build/ed-db/facts/levels.json`, rebuilt by `python -I tools/ed/facts/checks/args/levels_build.py`.

| Level | Who | Page label | Model | Start room | Files |
|---|---|---|---|---|---|
| -1 | Boot pack, not a level. Scripts 1500, 1600. `0x800182C4` | | | | Scripts |
| 00 | Alex in the mansion, flag 787 off | | 9 | 69 | All |
| 01 | Pious | | 0 | 341 | All |
| 02 | Ellia | ELLIA | 70 | 166 | All |
| 03 | Anthony | ANTHONY | 85 | 150 | All |
| 04 | Karim | JOSEPH | 150 | 338 | All |
| 05 | Maximillian | MAX | 18 | 249 | All |
| 06 | Lindsey | LINDSEY | 73 | 303 | All |
| 07 | Paul Luther | PAUL | 74 | 103 | All |
| 08 | Roberto | ABDUL | 76 | 82 | All |
| 09 | Edward | EDWARD | 69 | 237 | All |
| 10 | Peter | PETER | 75 | 27 | All |
| 11 | Not named by the disc's text, scripts or file names | MAJOR | 81 | 50 | All |
| 12 | Alex in the mansion, once flag 787 is on | | 9 | 100 | No Npcs |
| 13 | Alex by the model table | | 9 | 64 | All |
| 14 | `ed87` maps it to 0. No caller found | | 9 | 279 | `U_Level14.bin` only |
| 15 | No script asks for it | | 75 | 39 | `U_Level15.bin` only |
| 16 | See below | | 9 | 96 | No `U_Level` |
| 17 | Cinematics and insanity audio only | | | 0 | No scripts |

Notes:

- **Labels are not always people** (proved): level 4 is JOSEPH and level 8 is ABDUL, where the text names Karim and Roberto. MAJOR is item 208, whose use script 1581 calls `ed87(11, 11, 0)`. So level 11's label is tied to the data, but MAJOR need not be the person. `main.dol` has a debug string naming Edwards, not tied to any level.
- **The model column is the game's table, not what these saves show.** Chapter 3 and 7 saves have model 0 (proved). `player_model()` in `runtime/host/hle.c` overrides it with a chosen character (read). That this is why is inferred; slot 10's note in `docs/coop-next-session.md` says character 1 was chosen.
- **Level 12, proved:** `0x8003F4F4` (scripts) and `0x80040598` (words) turn 0 into 12 when flag 787 is on (`0x8003F544`, `0x8004066C`). coop-apart has it on and holds Level12's chunks. `0x80040650` sets 787 when chapter is 0 and room is 100, and script 1020 also sets it (read). `0x800203F8` also loads 12 for any chapter except 13 and 15 when 787 is on (read; how `0x80020328` is reached was not traced).
- **Level 13:** `ed87(12, 13, 1)` in script 1010 (in L09, L12, L13) is the only script call that targets 13 (proved). L13's pack also holds chapter page scripts with `ed87(2..11)`.
- **`ed87(a, level, c)`** at `0x800154FC` (read): a goes to `0x80307170`, its old value to `+4`, level to `+8`. Scripts 2028 and 1572 to 1581 call it for chapters 1 to 11 (proved). Script 1911 calls `ed87(0, 16, 0)`.

**Chapter 16** (read; no save is in chapter 16):

- `0x8001C768` writes chapter 16 and room 96.
- Scripts, words and U_Level: 16 becomes 0 (`0x8003F534`, `0x8004065C`, `0x800407FC`), then 12 if flag 787 is on.
- Npcs: the main load turns 16 into 0 (`0x80042268`). `0x800ADF44` and `0x800B8098` pass the chapter word raw, so those two read `Npcs16.npc`, 27 records in rooms 5, 39, 83. Those two run from `0x800ADDCC` (called at `0x80022248`, `0x800AE780`) and `0x800ADD78`. Whether they run during chapter 16 is not established.

**Level 16's own files**

- All 299 Lua scripts are byte-identical to Level 09's of the same id. L09 has 434 (proved). Its words are a subset of room09: 70 of 70 entries equal (proved).
- `ScrLvl16` and its words can load only through argument 19 (read). Of the callers of `0x8003F4F4` and `0x80040598`, the arguments are a chapter word, literal 0 or 12, ed241's argument, `0x80040750`'s, or `r30` at `0x8006C80C`. 16 becomes 0 in both functions, so only 19 spells it (`0x8003F590`, `0x80040684`).
- ed241's only argument is the global `ED_LEVEL_ALEX_END`, with no assignment in any script and no string in `main.dol`.
- `0x80040750`'s argument comes through `0x8003FC78` from a chapter word (`0x800203BC`, `0x800ADE58`) or from `+22` of table `0x80242150` (`0x8012AA44`). The 30 rows read hold no 19. The table's length was not established.
- **The 19 is table `0x802441F4`'s chapter 0 entry, in `0x8006C708`** (read, table from the dol). It is called at `0x8004371C` for a type 32 thing after `0x8006A32C(..., 13)`. It loads another chapter's scripts and words: 0 loads 19, 5 loads 0, 7 loads 10, 9 loads 5, 10 loads 7, the others their own. It also sets `0x4000`, the flag that gates examine. `0x8006CE7C` and `0x8006CE84` reload the chapter's own.

**Rooms:** `placed_rooms_Npcs` is firm. `rooms_by_door_walk_from_those` is an upper bound, since shared scripts name rooms of other levels. Rooms 5 and 83 are left out as seeds. 26 rooms have no level by either method, printed by `levels_build.py`.

## 6. What the task, docs and kit had wrong

| Said | Actually | Mark |
|---|---|---|
| slot 3 is Karim's chapter | Chapter 3, room 21, player model 0, carrying Broken Blue Urn, Tome, Scramasax, Message Scroll | proved, `args/models.py` |
| slot 7 is Paul's chapter | Chapter 3, room 155. slot 10 is chapter 7, room 115 | proved |
| Why | The slots were re-saved. Files are dated 2026-11-03, while `docs/coop-next-session.md` notes slot 3 as Karim's on 11-02 23:16 and says the user re-saves. The doc is stale, not necessarily wrong when written | proved |
| slot 1 player 2 is model 69 | One type 2 node, handle 1, model 0. slots 9 and 10 have a model 69 | proved |
| Bronze Necklace 102, Tulwar 9, item 62 are item ids | They are not type ids (Necklace 21, Tulwar 84, Chakrams 82, Talisman 150). The item script's value goes through `0x801F5F88`, a handle lookup (`0x80015CA8`, read). Proved for 62 only: slot 1 handle 62 is type id 31, Blue Urn. Handles change between plays: handle 9 is a Message Scroll in slot 1, type 43 in slot 9, type 86 in slot 10. No current save holds 102, 9, 10, 11 as those items | proved for 62, read for the rest |
| `primitives.tsv`: fn25 `used_thing`, fn34 `stop_self`, fn71 `music`, fn104 `sound_at` | Current room, re-run script, queue a room, position pointer | see section 3 |
| `primitives.tsv`: ed16 `character` | A thing's own field at node `+40` `+148`, alignment only on players | proved |
| `docs/game/systems/items.md` line 12: fn25 "what a thing had used on it" | Current room | proved |
| `docs/game/systems/camera.md` line 10: `0x80602B18` "the records that follow someone" | Only the target half of the records | proved |
| `rooms.json` `pos` and `path`, and its camera list | See section 2 | proved |
| `ed timeline` printing `free x,y,z` for an eye on a rail | Comes from that `pos` | inferred, not re-run |

## Not established

**Claims that failed**

| The first version said | Why it failed | What stands instead |
|---|---|---|
| fn8's d clears both records | `0x801EB7A0(record, 0)` skips the memset | Section 2 |
| "Most cameras move", 618 of 1,186 | Figures did not reproduce, and the kit list is about half the game. Replaced by a full walk | 814 of 2,177 |
| Event 57's queue reader was not found | It is `0x80025FF0` | Section 3 |
| ed16 is a thing's copy of alignment | True on players only. Other nodes read 2, 3, 4 with alignment 1 | Section 3 |
| 13 is the last level | One skeptic confirmed, two found nothing but one call and the act numbers. Nothing read shows what follows it | Section 5 keeps the call only |
| Chapter 16 runs Level 00's files | True for scripts, words and U_Level. Two Npcs loaders pass 16 raw | Section 5 |
| Level 16's files load only through argument 19 | `Npcs16.npc` loads with chapter 16. prologue.aud and cin0069 loaders not traced | Section 5, for scripts and words only |
| `LIGHT_AFTER_EXAMINE_TWEEN` is 0 | No script assigns it, its string is not in `main.dol`, no host code names it, and `lua_tonumber` turns a non-number into a constant (`0x80163558`). So 0 is expected, and **never seen run** | Still only an expectation. `ED_LEVEL_ALEX_END` is the same case |

**Meanings that nothing shows**

- That n's bit 4 hides the player. What's read is in section 1: it toggles slot 15 on a list of things, plus `0x801C588C` and `0x801D5D60`.
- What `0x801F0638`, `0x801F077C` and `0x800255A4` test. The first version called them "crosses the camera's line" and "flags other characters", with no reading behind it.
- What `0x801A4884(12, 100, 6, 1, 0, 5, 0, 0)` does for the Blue Urn.
- What reads the 20-byte camera keys at entry `+112`. `0x801ECD18` and `0x801ECD88`, which use a stride of 20, are the camera stack, not this.
- `fn4`'s `+56`.
- What `0x80308A89` counts, and what sets it beyond `0x80026568` and `0x8004F91C`.
- What the neighbour values in alignment table rows are for.
- Meanings of events 202, 186, 135, 250, 147, 125, 194, and the unread list 172, 146, 218, 210, 164, 245, 170, 148, 149, 197, 24, 52, 105, 190, 50, 17, 182, 174, 244, 227, 141. Usage counts are in `IDX=1 TOP=60 python -I tools/ed/facts/checks/args/calls.py fn79`.
- Which effects use `0x8006C708`, and what effect 13 is.
- That chapter 16 is the opening. `prologue.aud` being in Level16's folder is all there is.

**Things not found or not traced**

- Tag 2042, in L08 script 2182.
- Why 9 of Level 3's type 44 records, and 1 of Level 7's, are not live in any save.
- 4 cases where `fn25() ~= N` is followed by regions from another room (scripts 1436 with 187, 1412 with 32).
- Who, if anyone, enters levels 14 and 15.
- `0x801CFC14`, `0x800CDAEC`, and other users of the look record beyond `0x800259E4`, `0x80025A9C`.
- b = 1 on a live camera, alignment 2 or 3 live, and chapter 16 live: no save has them.
