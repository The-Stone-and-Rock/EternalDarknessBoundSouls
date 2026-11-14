# Fall readers: rooms 182 and 181, and the recordings (2026-11-12)

The bug: with separate rooms on, in Pius's chapter, player 1 took the statue room's (182) walk-in door to room 181 and
player 2, still in 182 and about to follow, fell through the floor and could not move. The log was lost. This reader
covers the two rooms' places and floors, what player 1's change runs for player 2, the two scripted runs, and a scan of
every recording for a sudden z change while the players are apart. Nothing was edited, built or launched.

Marks: [read] = seen in `ed` output, a log, a both.tsv row or a hle.c line. [inferred] = reasoned from those, not seen.

Tools: `ed room 182/181`, `ed door 182/181`, `ed script 124/1161/205/2033/2273/2679/126/1298`, `ed where 181 X Y Z`,
`ed tag`, `ed check`. Throwaway scripts in the scratchpad read every `logs/run-202611*/both.tsv` (5,393 runs): a
floor-height map per room from every row with state 1, and a scan for a z change of more than 50 in 6 frames while
p1room != p2room.

## 1. In short

- **Room 182 stands at three heights** [read, recordings]: z 0 in the south (y -500..400), a ramp to z 78 (y 400..600),
  z 78 (y 600..900), the statue's dais at z 104..109 (y 900..1400, the statue's Region03 at -790,1234), and z 78 again
  along the whole north corridor to the door (y 1500..2571). Region02, the door to 181, is a sphere at (-762, 2414, 80),
  radius 111, and EntryMkr02 is at (-769, 2229, 81). **A player "about to follow" stands at z 78.**
- **Room 181 is in its own coordinates and its floor is about z 0** [read: markers and regions at z 0..6; the rows seen
  there are all at z 0]. Its regions span x -2668..563 and y -6..2023 [read]. **181 has no floor at 182's door end**
  (y 2023..2525): a player who stood at (-709, 2289, 81) while the room in play was 181 fell for ever [read:
  logs/run-20261105-172914-sweep-L01-r182 both.tsv 4765-4888, z 81 to -3405].
- So **one frame against room 181's collision sinks player 2 for good almost anywhere in 182's north half.** Where 181 has a
  floor (about z 0) they land 78 to 109 below 182's floor. Where it has none (the door end, y over about 2023), there is
  nothing at all. A player under 182's floor does not come back: put at (-769, 1506, -7) under the dais edge (floor about
  100), player 1 fell to z -7597 and on [read: logs/run-20261105-173348-sweep-L01-r182 4450-4565]. **While the players are
  apart, the host has no rescue.** coop_follow's "fell out of the room" check runs only when both objects' +592 match
  [read hle.c:21341-21378]. Apart, it returns at 21380 with `stood_z = NAN`. That fits "could not move after".
- **In 182's south half (z 0) a frame against 181's collision would not show**, wherever 181 also has a floor at z 0. The
  first scripted run kept player 2 there.
- **No scripted run had the user's layout**: both players at the north end, player 1 walking into Region02 through the zone
  scan, player 2 a few steps behind at z 78, moving. What each run did and did not cover is in section 6.
- **Player 1's change sends nothing into 182** [read]. Script 124 queues 181 in the ARAM cache (fn71) and calls go_to_room.
  205 or 1161 is a camera in 181. 2273 drives 181's teleport things (tags 2236 and 2190, both in 181). 2033 is 181's
  first-visit camera scene (cutscene fn22(1,1), flag 546). Nothing touches the statue (script 1298, flags 608 and 84).
- **The scan of all apart recordings found no fall through a floor** except the known ED_PUT tool fall. One unexplained
  move is worth a look: in the user's play-r5c (Pius's chapter, rooms 341 and 342), player 2 was lifted 1,228 units in 17
  frames, idle, as their Tome closed in the Tome copy while apart. They then sank and landed on 341's stairs at z 192
  (section 7).

## 2. Room 182, the statue room

Regions and markers [read `ed room 182`]:

| What | Where | Runs |
|---|---|---|
| Region01 (box) | min [-876,-572,-15] max [-645,-463,176] | 122: door to 180 / EntryMkr01, transition 3, asks OPEN DOOR, then 1162 |
| **Region02 (sphere)** | **at [-762, 2414, 80] r 111** | **124: door to 181 / EntryMkr01, transition 3 (62 ticks), then 205 (flag 546 off) or 1161 (flag 546 on); asks nothing (walk-in)** |
| Region03 (sphere) | at [-790, 1234, 0] r 312 | enter 1298 (Camera13, the statue's words or examine), leave 311 |
| SKPlane01 (plane) | not read | cross_a 1298 |
| CameraRgn02/04/05/07/09/11/12/13/15 | boxes over x -1762..290, y -556..2185, z -21..170 | cameras only |
| EntryMkr01 | (-758, -499, 3) | arrival from 180 |
| **EntryMkr02** | **(-769, 2229, 81)** | **arrival from 181 (script 126)** |
| AxisMkr01/02 | (-779, 3657, 0) / (-779, -1221, 0) | the room's axis, y -1221..3657 |
| GravityMkr01 | (-752, -576, 0) | |

Script 124 [read `ed script 124`]:
```
room_queue[fn71](181)
if flag(546) ~= 0 then go_to_room(181, EntryMkr01, Camera01, 3, 1161, 0)
else go_to_room(181, EntryMkr01, Camera01, 3, 205, 0)
```
fn71 is the ARAM room-file cache queue (0x80155600, table 0x8058AF60). It does not put a room into a slot [read
readers.md:48, 468].

The statue: script 1298 (Region03 enter / SKPlane01 cross) shows the words "Pious, you must prove your worth by
destroying this statue" once (flag 608). After that, with flag 84 = 1, it sets Camera13 and offers the examine [read
`ed script 1298`]. No level-1 script sets flag 84 [read: grep of build/ed-db/scripts/L01]. The game's code does
[inferred]. No script that runs at the 182 to 181 change touches it.

### 182's floor, from every recording

Each cell is 100 x 100 units. The values are the lowest and highest z of a player in state 1, over 27 runs [read: the
scratchpad's floor map]. The very negative lows (-8909, -1177393, ...) are the ED_PUT tool falls of section 8, not the
floor.

```
 y \ x   -1000    -900     -800     -700     -600
 2500      .       .       78       .        .     <- Region02 (y 2303..2525)
 2400      .       .       78       .        .
 2300      .       .       78       .        .
 2200      .       .       78..81   .        .     <- EntryMkr02 (y 2229)
 1600-2100 .       78      78       78       .     the north corridor
 1500      .       78..102 ..102    78..104  .     the dais's north edge (the sweep put at -769,1506 fell from here)
 900-1400  109     104..109  109    107..109 109   the dais (the statue at -790,1234)
 600-800   78      78      78       78       .
 500       .       ..53    ..54     0..53    .     the ramp
 -500..400 0       0       0        0        0     the south half, to the door to 180
```

## 3. Room 181, the button room

[read `ed room 181`, `ed door 181`, `ed tag 2236/2190`]

| What | Where | Runs |
|---|---|---|
| Region01 (sphere) | (-2668, 1068, 0) r 261 | 126: door back to 182 / EntryMkr02 |
| SphereRegion02 (sphere) | (-760, 1064, 0) r 219 | 127: the teleport to 184 / EntryMkr01 |
| SphereRegion01 | (-1692, 402, 0) r 224 | 290: the button scene (prompt, cutscene, Camera03) |
| CameraRgn03/09/10/11/12/13 | boxes over x -2382..563, y -6..2023, z -31..112 | 607, 611, 2679, 2033 |
| **EntryMkr01** | **(-2147, 1061, 3)** | **arrival from 182** |
| GravityMkr02/03 | (-1731, 405, 6) / (-755, 1060, 0) | |
| AxisMkr01/02 | (-3522, 1060, 0) / (783, 1060, 0) | the room's axis, x -3522..783 |
| things 2236, 2190 (chr 122) | (-756, 1064, 27) / (-760, 1068, 9) | the teleport's parts (scripts 2273, 290) |

**Same coordinate space? No.** Each room has its own coordinates. They are joined only by the doors: 182's Region02 at
(-762, 2414) leads to 181's EntryMkr01 at (-2147, 1061), and 181's Region01 at (-2668, 1068) leads to 182's EntryMkr02 at
(-769, 2229) [read]. The overlap of the two rooms in raw coordinates means nothing to the game.

**181's floor at 182's places** [`ed where 181 ...` read; floor heights inferred unless marked]:

| A place in 182 (z = 182's floor) | In 181 at the same x, y | If player 2 is grounded once against 181 |
|---|---|---|
| Region02 (-762, 2414, 80), the door end y 2023..2525 | in no region; outside 181's regions (y max 2023) | **no floor**: they fall for ever. **[read]** for (-709, 2289): together sweep 172914 fell from z 81. Also (-704, 2295) in sweep 173348, p2 at z -207 with room 181. |
| EntryMkr02 (-769, 2229, 78) | in no region | no floor [inferred, same as the row above] |
| North corridor (-770, 1800, 78) | CameraRgn10 (x -1155..-365, y 1299..2015) | 181's floor about z 0 [inferred]: they land 78 below 182's floor and fall for ever there |
| The dais / statue (-790, 1234, 109) | **181's SphereRegion02, the teleport** (radius 219 round -760,1064) | about z 0 [inferred]: they land 109 below the dais |
| Middle (-770, 700, 78) | in no region (the strip x -797..-708 between CameraRgn11 and 13) | unknown: a gap or a floor not under a camera box |
| South (-622, 133, 0) | CameraRgn13 | about z 0, the same as 182's: **it would not show** |
| South door side (-758, -349, 0) | in no region (181's y min -6) | no floor [inferred] |

The reverse, a player in 181 grounded once against 182's collision [inferred]:
- At 181's entry (-2147, 1061) and its whole west part (x < -1762, 182's west edge): no 182 floor. They fall.
- At the teleport (-760, 1064): 182's dais is at z 109, above them. They would be lifted onto it, then drop back to 181's
  z 0. That would look like a hop, not a fall.

## 4. What player 1's change 182 to 181 runs, for player 2 in 182

The sequence from logs/run-20261112-033642-p1-to-181/page-0001.log [read]. The p2-follows run is the same, three frames
later.

| Frame | What |
|---|---|
| 4697 | `[test] running script 124`. **The test ran the door script directly, with player 1 at 182's SOUTH door (-758,-499), not by walking into Region02.** `the other goes on through it`. `player 2 goes on through player 1's room change: not held (lr 0x80117E08)` |
| 4701-4760 | ticket 61 to 0. Player 2 stands at (-758,-349,0), idle (anim 15) |
| 4761 | `[room] change 0xB5`. Before: table +0 1 +4 0, slot 1 = 182 loaded. `the load's strip passes over player 2's node`. Slot swap, a fresh load: **table +0 still 1 while the room in play is 181**. 181 is unpacked into slot 0 |
| 4763 | `the arrival's walk passes over player 2's node`. `slot 1's record put back: room 182, loaded (a player is in it)`: **182's record was dead from the change until here**. `player 2's hold from the ticket let go`. After the tail: table +0 0 (181) +4 1 (182). Scripts 205 (Camera05), 2273 (181's teleport textures and glow) and 2033 (`scene 2033 begins as player 1's`, `player 1's scene holds them alone`) |
| 4764 | `[stay] player 2 stays in room 182 (slot 1) at -758 -349 -0; the room in play is 181 (slot 0)`. Both rooms drawn |
| 4767 | flag 546 on (2033's first visit) |
| 4772 | player 2 starts walking (p2anim 2), still z -0.0 |

hle.c's own comment says the put-back runs right after the tail, before "the behaviour pass, the scripts and the things
tick of this frame" [read hle.c:14355-14357]. So by its design no step of player 2's sees the dead record. No row shows
otherwise: p2z is -0.0 at every frame from 4697 to 5726 [read both.tsv].

Script 2033 [read]: on the first visit (flag 546 off) it runs `cutscene(1,1)`, eases the camera and plays Camera04's path,
and ends at state 1. Player 1's scene lasts about 230 frames: script 2273 runs from 4763 to 5002 in the p1-to-181 run, and
until 4990 in the p2-follows run. Player 2's own door waits for it: `player 2's zone 9 asked for a room while player 1's
scene, words or close-up is up: it waits for its end` (frames 4846 and 4974 in p2-follows) [read]. With flag 546 on (a
later visit) the door's then-script is 1161 and 2033 only sets Camera01: no scene, so no wait [read].

## 5. Player 1 as the stayer (player 2 takes 182's door)

The geometry is the same: player 1 at z 78 or 109 in 182's north half is in the same danger from a frame against 181.

The one run with player 1 staying is p2-follows after frame 5052, with player 1 staying in 181 at (-2140, 1061, 0). That
spot is outside 182's footprint, so a frame against 182 there would have dropped them. p1z is 0.0 from 5052 to 5635 [read
both.tsv]: no such frame happened through player 2's bracketed change (`player 2's change to room 181 runs as theirs`,
`before the change: table +0 0 +4 1; room in play 182 old 182`, `player 1's hold from the ticket let go`, frame 5052).

No run has player 1 staying in 182 while player 2 goes to 181 [read: the pair table in section 7 lists 182 apart only in
the two scripted runs, as 181/182].

## 6. The two scripted runs, frame by frame

**logs/run-20261112-033642-p1-to-181**, 1,085 rows, frames 4501-5726. `ed check`: 0 findings [read].
- Player 1 starts at 182's EntryMkr01 (-758,-499,0). The door runs as `[test] running script 124` at 4697: the north
  Region02 is never walked into.
- Player 2 is at (-758,-349,0) in the south half, idle through the ticket and the change. From 4772 they walk
  (-758,-343) to (-955,-6) to (-944,-268) to (-580,-255) to (-622,133). Every row has p2z -0.0.
- That covers player 2 idle at z 0 through the change, and walking at z 0 after it. It includes points with no 181 floor
  under them (y < -6), which would have shown a frame against 181. It does not cover the north half (z 78 or 109), or player
  2 moving during the ticket.

**logs/run-20261112-033738-p2-follows-181**, 990 rows, frames 4501-5635. `ed check`: one SENT at 4701-4704, the tool's put
[read].
- `[sweep] player 2 put at -769 2229 30` at frame 4701, during player 1's ticket. The row shows them at 78.4 from 4704.
- Player 2 stands idle at (-769, 2229, 78.4) through the change frames 4764-4766: the record put-back and the hold let go
  [read page log 204-229]. p2z stays 78.4.
- From about 4805 they walk to y 2571 and back to y 2443, at z 78.2-78.4. They pass through Region02 at 4846 and 4974, and
  the door waits for player 1's scene. At 4990 `player 2's way out goes`. The ticket runs 4991-5052, they arrive at 181's
  (-2147, 1061, 0) at 5054 and walk at z 0.0.
- That covers player 2 idle at z 78 through player 1's change, which a frame against 181 would have sunk (no 181 floor at
  y 2229). It also covers walking at z 78 at y 2229..2571 while apart, and the door waiting for player 1's scene.

**Neither run had** [read: the rows above]:
1. Player 1 walking into Region02 through the zone scan. Both runs started the door from the host at 182's south door.
2. Player 2 moving (walking or running toward the door) during player 1's ticket and change frames. Player 2 was idle from
   4697/4701 until after the tail in both runs, and they are "not held" by player 1's ticket.
3. The two players close together. Player 2 was 150 units from player 1 (p1-to-181) or 2,700 units (p2-follows).
4. Player 2 inside the Region02 sphere when player 1's ticket starts or when the tail lets them go.
5. A visit with flag 546 on (door then-script 1161, no 2033 scene).
6. Anything of player 2's own (an attack on the statue, the Tome, an item, examine) while apart in 182.
7. Enemies. 182 has 36 SpawnMkr [read]. Whether any were live in those states was not read.

## 7. The scan of every apart recording

Every run with p1room and p2room columns and at least one apart row: 57 runs [read]. The apart pairs: 180/183 in the
R3-R6 tests; 341/342/343/344 in the user's plays (play-r5b, play-r5c, rooms-play x2) and jtg-coop-gate; **181/182 only in
the two scripted runs**. A z change of more than 50 in 6 frames, same room, while apart [read]:

| Run | Who | Frames | z | Rooms p1/p2 | What it is |
|---|---|---|---|---|---|
| run-20261112-160254-cam-lf-load-p2door | p2 | 2400 to 2401 | 23.7 to -49.0, then falls on (-116 at 2420) | 343/341 | the known ED_PUT tool fall (put by room 343's floor while in 341). Fixed |
| run-20261112-161027-cam2-lf-load-p2door | p2 | 2395 to 2401 | 23.7 to -63.7 | 343/341 | the same tool fall |
| run-20261111-221737-r4b-c7-apart-finish | p2 | 7401-7435 | 75 to 81 to 0.0 | 180/183 | **not a bug**: state 7, anim 11 (a hit), knocked off a ledge in 183, landing on 183's floor at z 0.0 and walking on |
| **run-20261111-191306-play-r5c (the user)** | p2 | 3660 to 3677 up, 3760-3790 down | **0 to 1228, then down to 192** | 342/341 | **unexplained**, below |

**play-r5c, frames 3655-3900** [read both.tsv and page-0001.log 6855-7020]. Player 2 is apart in 341, player 1 in 342 (both
Pius's chapter, level 1). Player 2 stands at (609, 503, 0) with the Tome open (window 0x805881F0).
- Log, frame 3656: `player 2 equips 9 (hands 2) in the real game`, their moves part reloaded. Then `player 2's Tome closes
  (its own way out) after 142 turns`. Their pad: A (0x0100), then B (0x0200), stick 0.
- Frames 3661-3663: player 2 moves +133.1 x, -29.4 y, +94.4 z every frame, idle (anim 15, state 1).
- They hit something at x 1152.4 and go on at -29.4 y a frame to y -231.8, z rising to 1227.9 at frame 3677.
- They then sink exactly 1.0 a frame (3677-3760), fall faster from 3760 while their Tome is open again (to 192 at 3790),
  bounce to 237 and settle at 192.0. They walk down to z 0 from 3880 to 3942.
- Room 341's own floor at (1000, -300) has rows from 0 to 168 [read: the floor map]: 341 has stairs or a ledge there. So the
  landing at 192 is probably 341's own [inferred].
- So player 2 was moved without input as the Tome copy closed while apart, but did not fall through. Whether this used room
  342's collision, a velocity left over from the copy, or the equip in the real game was not read: **an open lead for the
  code readers** (the Tome copy's close while coop_stay_apart).

No other jump in any apart run [read].

## 8. Other recorded falls in rooms 180-184, all together mode and all ED_PUT

[read: the deep-z scan] Each came from a sweep or test put below a floor, or onto a spot with no floor, in the Nov 5-10
runs, before separate rooms:
- 182: `sweep-L01-r182` at (-769, 1506) z -7 to below -7597 (173348). `own-words-B` at (-820, 600) z -201 (191100).
  `stepB-run0-ownwordsA` at (-790, 900) z -202 (213915). Puts at z 0 or -7 under 182's z 78..109 floors.
- 181: sweep 172914, (-709, 2289) z 81 to below -3405. Sweep 173348, p2 at (-704, 2295) to z -207. 182's door spot asked of
  181.
- 180 statue-look x3 at (-750, 903) z -214. 183 stepB runs at (1433, 1661) and (-144, 2193), both recorded in
  coop-failed-attempts.md:698.

What they show [read]:
1. **A player put under a floor, even 7 units under a floor about 100 above them, falls for ever.** The floor test does not
   reach up to a floor above them. The 341 ED_PUT put them 49 under and they fell (cam-lf). The sweep's own put at z 30
   under 182's z 78 was grounded to 78.4 (p2-follows 4701-4704), but that is the put tool's own grounding [inferred], not
   the game's.
2. **Asked of 181, 182's door end has no floor** (sweep 172914).
3. Each fall stayed state 1. Together, the coop_follow rescue would place player 2 beside player 1 once they are 2000 below
   where they last stood still (COOP_FALLEN, hle.c:21271), and not in a scene. Apart, there is none (hle.c:21380-21389).

## 9. Leads, by how well they fit "about to follow, fell through the floor, could not move"

1. **[inferred, strongest geometry]** Anything that grounds, places or steps player 2 once against 181's collision while
   they are in 182's north half. Every point there is either over no 181 floor (the door end) or 78 to 109 above 181's
   floor. The test tool did exactly this in 341/343 today. In the user's layout (player 2 a few steps behind player 1 at the
   door, z 78) any such frame is fatal. In the scripted p1-to-181 layout (z 0, south) many such frames would not show.
2. **[inferred, untested timing]** Player 2 moving through player 1's ticket and change. They are "not held" by player 1's
   ticket [read: `goes on through ... not held`]. The window is frames 4761-4763 of the p1-to-181 run: table +0 still
   182's slot while the room in play is 181, and 182's record dead until the put-back. hle.c says the put-back comes
   before the behaviour pass [read 14355-14357]. Whether any step, placing (0x80130EF0 uses 0x801551A0(2), the current
   section [read enemy-readers.md:65]) or push of player 2's runs inside that window when they are moving, at the door,
   next to player 1, was never recorded. Both runs had player 2 idle there.
3. **[inferred]** Player 2 inside Region02 (radius 111) when player 1's ticket begins or when the tail lets them go. Their
   own door would ask at once, in the frames right after player 1's change. The p2-follows run asked 85 frames later and
   waited for the 2033 scene. The asking-at-once case is untested. Their door is the same 124 (room_queue 181, go_to_room
   181), with 181 resident.
4. **[read, other room]** The play-r5c lift at the Tome copy's close while apart (section 7): a recorded case of player 2
   moved with no input while apart, in the same chapter. If the user opened the Tome, equipped or used something in 182
   while player 1 was in 181, this path is a candidate. With 182's floors it would be fatal, not a landing.
5. **[read, ruled out]** The steady state. Player 2 standing or walking at z 0 and at z 78 in 182 while player 1 is in 181
   is covered by the two runs, including spots over no 181 floor. The per-frame step uses their own room. Player 2's
   follow, with its wait for 2033's scene, worked.
6. **[read, ruled out]** Scripts and events into 182 at the change. There are none, and the statue is not touched.

### Places for a reproduction (not run; for whoever runs next)

Fresh from Jump to Game, per the user's rule:
- Both players walk the north corridor together. Player 1 walks into Region02 at (-762, 2414, 80). Player 2 follows about
  50-150 behind at z 78, holding the stick toward the door through player 1's ticket and change, and again with player 2
  inside the sphere when player 1 enters it.
- The same with flag 546 on (a second visit: 1161, no scene).
- The same with the roles swapped: player 2 takes the door and player 1 follows at z 78.
- Player 2 opens and closes the Tome, or equips something, at z 78 or 109 in 182 while player 1 is in 181. Compare with
  play-r5c's lift.
- Detector: p2z (or p1z) leaving 78.x or 109.x while that player's room stays 182. Any value under 70 at y > 600 is under
  182's floor.

## 10. Open questions

- 181's real floor (its collision mesh) at x -800..-700, y 600..2023: inferred from camera boxes and markers only. No ed
  command reads a room's collision. A mesh reader, or a probe of 0x80138E8C at a few points in each slot, would settle it.
- Whether any frame between player 1's change (4761) and the put-back (4763) steps a player 2 who is moving. hle.c says
  no [read 14355-14357], and no recording tests it.
- The play-r5c lift: what moved player 2 at +133/-29/+94 a frame as the Tome copy closed, apart, idle. It needs a code
  reader of the copy's close and the "equips in the real game" path, with the slot and room word at that moment.
- Whether 182 had live enemies in the user's play, and whether a hit (state 7) knocked player 2 while apart. r4b-c7 shows
  a knock-back apart landing correctly in 183.
