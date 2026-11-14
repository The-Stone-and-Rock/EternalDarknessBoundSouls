# Arrival camera apart: frame-by-frame readers (2026-11-12)

The item is the camera jitter at an arrival apart: the kit's nine-frame CAMERA blip, which the user still sees. This
report reads the recordings frame by frame, for both halves and both directions. Nothing was built or run. The sources
were existing logs, hle.c, camera.c, the disassembly and the kit.

How the rows were read: both.tsv `ex ey ez tx ty tz` is the game's live view block 0x803012E0, sampled at the game's pad
read (`both_record`, called from `hle_PADRead`, hle.c:4234). The block holds player 1's set at that point, so these
columns are **player 1's half** [read hle.c:17630-17632]. `p2ex..p2tz` is `g_coop_own_view`, the result of player 2's
own camera step, and is **player 2's half**. It prints 0 while `g_coop_own_view_frame == 0`, and camera.c then draws the
game's view in their half [read hle.c:17638-17640, camera.c:1728-1730, hle.c:16294-16304]. `p1ex..` is only player 1's
view kept while the cameras are lent, and it is 0 or stale otherwise [read hle.c:15457-15486]. The scratch scripts that
tabulated the arrivals are not part of the repo.

## Summary

- **Only player 2's half jumps, and only when player 2 makes the room change themselves** (the R4 bracket). This holds
  both apart (player 2 goes into a room player 1 is not in) and on the way back into player 1's room. At that first frame
  their camera becomes **player 1's live camera** (eye and target, in player 1's room's coordinates). The game then
  eases it, by the zone's `fn8` blend, to the right camera of player 2's room: about 74 to 80 frames apart, 55 frames on
  the way back. Where no zone of theirs chooses, their half stays on player 1's camera (user play 1: 680 frames, to the
  end of the recording).
- **Player 1's arrivals are clean in both directions:** a cut to the room's arrival camera on the first frame, as in
  solo. Player 2's half does not move when player 1 changes room apart.
- **Cause** [read, chain below]:
  1. The bracket gives player 2's set the arrival camera correctly: the game's `0x801EE998` / `0x801EE764` / `0x801EEA9C`
     cut, then the arrival script's fn8, all into their set.
  2. Then player 2's first zone look in the new room (`coop_prompt_look`, then `coop_zones_new`) sees a new zone list
     and **drops their set**: `p2_set = false`, `p2_bare = true`, `g_coop_own_view_frame = 0`.
  3. `coop_camera_p2_in` then remakes their set from the game's set (`coop_camera_set_fresh`), which is player 1's.
  4. Their zone's fn8 blends from there.
- R4c added `g_stay_arrival_camera` to skip exactly this drop, but **only when player 1's scene is on**
  (`stay_scene_look_again` returns before arming it otherwise, hle.c:17165-17168). Every ordinary arrival still takes the
  drop.
- **The kit under-reports it.** `ed check`'s CAMERA rule skips the room-change pair of rows and flags only per-frame
  steps over 120 units [read tools/ed/recording.py:228-262]. On the way back the glide's first nine steps (292 down to
  126) exceed 120, which is the "nine-frame blip". The apart glide (about 41 units a frame at most) is never flagged.

## 1. How the game sets a camera at an arrival, in solo (one player)

All reads are in `0x80152614`, the ticket runner [read `ed dis 0x80152740`].

| Address | What it does |
|---|---|
| `0x80152790` | `0x801EE998(r30 = the change's camera hash, 2, 0)` finds the camera in the current slot's room record |
| `0x801527CC` | `0x801EE764(0x80603288 = eye record 2, that entry, 0…; stack 0, 1)` chooses it for the room record. Who the record follows (+104) is set from r13-26524, the current player, per doors-readers.md line 39 |
| `0x801527D0` | `0x801ECCE0`: `r13-21700 := 0`, so the camera stack is emptied [read dis] |
| `0x801527D8` | `0x801EEB58(2)`: index r13-26864 := 2 |
| `0x801527DC` | `0x801EEA9C`: **the cut.** It copies the destination records' position, FOV and +72..+80 into the shown records `0x806037D8` (eye) and `0x80603860` (target), and clears their +64 [read dis 0x801EEA9C-0x801EEB54] |
| `0x801527F4` | The arrival script `0x801642D4(r31, 0, 0)`, for example 1161: fn4, fn8 `0x801669EC`, fn3 `0x80168450` [read log page lines at lr 0x801527F8] |

After the change, the player's first zone look during the 10-tick fade-in (`0x80134A44`; ticks at r13-21784) can choose
the zone's camera by fn8 [read docs/game/systems/camera.md "A room's start camera"]. In every room read here, that is
the same camera as the arrival's, so nothing visibly moves.

Solo recording, `logs/run-20261112-030139-jtg-solo-returns` [read rows]:

| Frame | Change | First eye | After |
|---|---|---|---|
| 2884→2888 | 344→180 | -584,-77,623 → -960,76,214 | Only the follow ease (eye y -77 → -88 by 2900) |
| 3284→3289 | 180→183 | 352,2337,350 → 356,2984,350 (Camera hash 0x200BD23E) | Slides 27 units along its rail in about 11 frames |

The fade value counts 9→0 over the first frames, so the picture comes in from black.

## 2. Every arrival in the recordings, by half

Columns: frames are both.tsv rows. "Gap" is missing rows, meaning the game's pad read did not run (see section 5).
"First eye" is that half's eye on the first row in the new room. "Settles" is the first frame after which the eye
moves less than 1 unit a frame for 3 frames.

### Player 1 arrives apart (player 2 stays in another room): clean

| Run | Change | Gap | P1 first eye | P1 settles | P2 half |
|---|---|---|---|---|---|
| r4b-s2-p1door (221713) | 183→180, f3823→3828 | 5 fr / 83 ms | -63,276,486 (the room's camera) | 1 frame | Moved 3.3 (own follow) |
| r4c-s2-p1door (223919) | 183→180, f3823→3829 | 6 / 100 ms | -63,276,486 | 1 | 3.4 |
| r5-s2-p1door (182711) | 183→180, f3826→3833 | 7 / 116 ms | -63,276,486 | 1 | 3.5 |
| rooms-play 1 (221141, the user) | 341→342, f1843→1847 | 4 / 66 ms | 22,-2348,833 (a fixed camera) | 1 | 0.0 |
| rooms-play 2 (222138, the user) | 341→342, f1957→1961 | 4 / 67 ms | 22,-2348,833 | 1 | 0.0 |
| follow-test (033038) | 183→180, f3882→3887 | 5 / 84 ms | -63,276,486 | 1 | 0.0 |
| follow-test | 180→183, f4522→4527 | 5 / 84 ms | 352,2337,350 | 11 (the rail, as solo) | 0.0 |
| jtg-coop-gate (030834) | 341→343, 343→344 | 3-4 | -17,1004,406 / -262,154,317 | 1 | 0.0 |
| p2-follows-181 (033738) | 182→181, f4764→4770 | 6 / 101 ms | -1314,1157,413 | 2 | 0.9 |

All rows above are [read]. Player 1's half is identical to solo: player 1's set is the game's at rest, the change runs
with it in and cuts. In `coop_camera_chosen`, the stayer's set is not remade from player 1's arrival
(`coop_stay_apart` returns before `room_choices++`, hle.c:17229-17230) [read]. Row data shows player 2's half untouched.

### Player 1 arrives into player 2's room (together again): clean

| Run | Change | P1 first eye | Settles |
|---|---|---|---|
| r4b-s2 | 180→183, f4363→4365 | 352,2337,350 | 1 |
| r4c-s2 | 180→183, f4363→4366 | 352,2337,350 | 1 |

All [read]. Player 2's half stayed at -822.7,-302.8,579.0 through it [read r4c-s2 rows 4358-4385]. Player 2 was not
"bare": their zone had chosen earlier in the room.

### Player 2 arrives apart (player 1 stays in another room): player 2's half glides from player 1's camera

| Run | Change | Gap | P2 first eye | P1's eye then | P2 settles |
|---|---|---|---|---|---|
| r4b-s1-p2door (221649) | 183→180, f3822→3826 | 4 / 67 ms | -757,-154,566 | -754,-148,566 (7 apart) | 3900 (74 frames) |
| r4c-s1-p2door (223853) | 183→180, f3828→3833 | 5 / 83 ms | -746,-128,564 | -742,-121,563 (8) | 3907 (74) |
| r5-s1-p2door (182644) | 183→180, f3823→3829 | 6 / 100 ms | -756,-151,566 | -753,-144,565 (7) | 3909 (80) |
| r4c-t1-p2door (172710) | 183→180, f3822→3826 | 4 / 66 ms | -757,-154,566 | -754,-148,566 (7) | 3900 (74) |

All [read]. The place it glides to is room 180's arrival camera, eye -63,276,486 and target -14,-454,214. That is the
same camera player 1 is cut to when they take that door [read r4c-t1 rows 4340-4362 against r4c-s2 rows 3829+].

Frame by frame for r4c-t1 [read both.tsv rows 3822-3870]:

| Frame | P2 eye | P2 target |
|---|---|---|
| 3822 | 352,2364,349 | 356,2984,350 (room 183's Camera25) |
| 3826 | -757.1,-153.8,566.2 | 1154,1471,265 |

At 3826 player 2's view is player 1's view of room 183 to within a frame of motion: the game's view row 3827 is
-757.1,-153.8,566.2 too. The steps after are -722→-689→-658→-628… (41, 39, 37… units a frame, a ratio of about 0.95,
about 1/20 of the way a frame [inferred: fn3's divisor]). They are still easing at 3870 (-136,231,494) and settle about
3900.

`page-0001.log` for that run [read]:
- line 675: `[trace] player 2's camera jumps 2760 in a frame (frame 3824)`. 2760 is the distance from Camera25's eye to
  player 1's eye.
- lines 689-691 (frame 3826): script 1062 calls `0x801669EC` (fn8), then `[coop] player 2's zone chose their camera`.

Pictures, `logs/run-20261111-223853-r4c-s1-p2door/shots/000012.jpg` (about frame 3840) and 000015/000017: the right half
first shows a close wall of room 180 (player 1's eye position, read in room 180's space), then sweeps round to the
door [read pictures].

### Player 2 arrives into player 1's room (together again): the same glide, or player 1's camera for good

| Run | Change | P2 first eye | P1's eye | Settles |
|---|---|---|---|---|
| r4b-s1 / r4c-s1 / r5-s1 / r4c-t1 | 180→183, f4362→4364 | -826..-830,-291..-297,579 | the same (distance 0) | 4419 (55 frames) |
| p2-follows-181 (033738) | 182→181, f5052→5054 | -1299,1189,425 | the same | 1 frame (see below) |
| follow-test (033038) | 180→183, f4712→4714 and 183→180, f4062→4064 | Equal to player 1's | the same | 1 |
| rooms-play 1 (221141, the user) | 341→342, f2291→2292 | 31,-1324,190 (Camera06) | the same | **never**: Camera06 to the end, f2975 |

All [read].

- **r4c-t1 4364-4400:** player 2 eye -826,-297,579 (player 1's Camera01 view, player 1 at 1356,1549), then
  -708,-31,556 → -602,209,535 → … → 325,2304,354 at 4400, settling about 4419 on the arrival camera 352,2364,349.
  Player 2 stands at the door, 365,3073. Steps 292, 263, 236… (ratio 0.90). `[trace] player 2's camera jumps 958 in a
  frame (frame 4362/4363)`, then `jumps 292…60` for 4364-4379 (page lines 2759-2840 in r4c-s1).
- **The kit's blip:** `ed check`'s CAMERA rule fires for 4364-4373. Those are the nine row pairs whose step exceeds 120
  units (292…126). The 113-unit step at 4373→4374 is under the line [read recording.py:252-262 against the rows]. The
  same glide goes on for 46 more frames below the threshold.
- **When it looks fine:** when player 1 happens to stand on the arrival spot's camera, the copy is right by luck
  (p2-follows-181, follow-test).
- **The user's play 1 (pre-R4c build), 2291 onward:** player 2 arrived at the door (195,-2269), where the arrival camera
  is 22,-2348,833; player 1 got it at 1847 through the same door. Player 2's half took player 1's Camera06
  (31,-1324,190 → 34,-1616,190) and stayed on it, because no zone of player 2's chose (no "zone chose" line after page
  line 4122). That is an arrival spot in no camera zone of theirs: the case arrival-readers.md line 191 had only
  inferred.

### Arrivals together (player 2 brought along by player 1's door, the JTG route without ED_COOP_STAY)

`logs/run-20261111-182534-jtg-rooms-r5-state` 2885→2890 (344→180, "[coop] player 2 brought", page line 2988) [read]:
- First row: player 2's own view is 0, so camera.c draws the game's view, player 1's arrival cut (-584,-77,623).
- From 2891, their own step: eye -584,-82,623, target -955,76,214. Only the target eases toward where player 2 stands
  (79 units over 30 frames). No swoop.
- Log: "the room's start chose a camera (the room change, frame 2887)", then "(player 1's first look, 2887)", then
  "player 2's zone chose their camera (2890)".
- Together, the room's start is player 2's too (`room_choices`, `coop_camera_p2_in`'s follow). Player 1's arrival cut
  is the right camera for both, because both stand at the arrival spot.

`logs/run-20261111-112722-jtg-stepE-r6b-p1words-door` 2284→2287 shows the same [read]: player 2's half is on player 1's
arrival camera -17,1004,406 from the first frame.

## 3. Why player 2's set becomes player 1's: the chain, both players

1. **notify_ticket_run** (hle.c:13858-13869), at 0x80152614's entry. It opens only with `coop_stay() && g_coop_room_p2`
   and player 2 having a room.
   - `coop_be_player2` makes player 2 the player: r13-26524, -21492 and -21496 are theirs.
   - The room word r13-23276 := player 2's old room.
   - `coop_cameras_in(cpu, 2)` puts player 2's camera set in: records 0x80602B18..0x8060394C, depth r13-21700, index
     r13-26864 [read].
   - Precondition: cameras not lent at the change. Both.tsv `lent` is 0 from 3764 in r4c-s1. The door script's own lend
     at 3760 ended long before the change at 3828 [read].
2. **The change's camera block** (section 1) runs with player 2 the player and their set in. Its `0x801EE764` on record
   2 reaches `coop_camera_chosen` (hle.c:17206), and the room-change plus bracket branch sets `p2_bare = false`
   (17220-17222) [read]. The arrival script (1161 in room 181/342: fn4, fn8, fn3) also runs into their set, `who 17`/`59`
   being player 2 [read p2-follows-181 page 1428-1431; rooms-play 1 page ~4113].
3. **coop_stay_change_end** (13870-13887): `coop_cameras_in(cpu, 1)` keeps the game's set as player 2's
   (`g_coop_cameras.p2`, `p2_set = true`, hle.c:15429-15433) and puts player 1's back. So **at the bracket's end player
   2's kept set holds the arrival's cut camera** [read code; inferred contents, since the set was not dumped].
   - `stay_scene_look_again` (17165) arms `g_stay_arrival_camera` only if `coop_scene_on_of(0)`, player 1's scene. In
     these runs it was off [read: no "came in beside" or "first look … keeps" lines in any run].
4. **Player 2's turn, same frame:** `coop_prompt_look` (20713), then the controller, then `coop_camera_step` (20802)
   [read].
   - `coop_prompt_look` (16544) gets player 2's character info from their object (COOP_CHARACTER_INFO), and calls
     `coop_zones_new(cpu, info)` (16551).
   - info+180, their room's zone list, differs from `g_coop_zones.list` and the R4c flag is clear. So hle.c:15967-15978
     runs: `p2_set = false; p2_bare = true; g_coop_own_view_frame = 0` [read].
   - Proof that this ran: `[coop] player 2's zone chose their camera` (r4c-t1 page 691, frame 3826; 2775 in r4c-s1;
     1447 in p2-follows-181) prints only when `p2_bare` (17225). The bracket had cleared it, and the only writer of
     `p2_bare = true` is 15976 [read grep].
5. Next, `coop_cameras_in(cpu, 2)` (16566) leads to `coop_camera_p2_in` (15398): `!p2_set`, so `coop_camera_set_fresh`
   (15365).
   - That is the game's set as it is: **player 1's records, shown blend and index**, with player 1's pushes popped by
     the game's 0x801ECCEC.
   - The records' +124 entries point into player 1's room's camera data [inferred: +124 is "the room's entry", per
     camera.md; the rooms are in different slots]. `coop_camera_step` re-points records naming player 1's object to
     player 2's (16083-16087) [read].
6. **The step** `0x801EB9A0(3)` then gives player 2's view from player 1's shown records. That is the jump of 2760, 2731
   or 1700 in `[trace] player 2's camera jumps` at the bracket frame (3824, 3830, 5052) [read].
7. **Their zone's fn8** (blend c≠0) at the next look sets record 2's destination to their room's camera. The step eases
   the shown records from player 1's place to it: the glide.
   - If no zone of theirs chooses at the arrival spot, nothing replaces player 1's camera: user play 1.

**Which player's data each piece reads:**

| Code | Run during | Player's data |
|---|---|---|
| `0x80152614` and `0x801EE998`/`0x801EE764`/`0x801EEA9C` | Player 2's change (the bracket) | Player 2's identity words and their set; the current slot is their new room's (doors-readers.md line 62) |
| `0x80152614` and `0x801EE998`/`0x801EE764`/`0x801EEA9C` | Player 1's own change | Player 1's set, as in solo |
| `0x80134A44` zone look | `coop_prompt_look` | Player 2's zone words, states and set |
| `0x80134A44` zone look | The game | Player 1's |
| `0x801EB9A0` camera step | Player 2's turn | Player 2's set, result to `g_coop_own_view` (the view block put back after) |
| `0x801EB9A0` camera step | The game's main loop | Player 1's |
| `coop_zones_new` | — | Only player 2's zone list: a change of player 1's while apart never touches player 2's set, as the rows show |

## 4. The other half at the same moment

- **Player 1's half during player 2's change and arrival:** continuous.
  - r4c-t1 3822 to 3826: eye -754.4,-147.6,565.7 → -755.7,-150.7,565.9, the follow ease only.
  - r4c-t1 4362 to 4364: -825.9 → -826.0 [read].
  - The fade is the mover's half alone. In shot 000038 of r4c-s1 the right half is dark and the left full [read picture;
    R4's "the fade quad the mover's half alone"].
- **One case where player 1's half moves:** p2-follows-181 5052→5054.
  - Player 2 arrives on the spot player 1 stands on (-2147,1061 vs -2139.8,1060.9).
  - Player 1 moves 93 units in one frame, to -2046.6,1059.2.
  - Player 1's eye then eases -1299→-1232 over about 50 frames (the follow ease) [read rows].
  - What moved player 1 is not in the log: no "beside" or "place" line [open; inferred the bodies' push-apart or the
    arrival placing].

## 5. Related: the brief freeze (not this item, the same frames)

At every room change the game's pad read does not run for 2-7 frames, so both.tsv has no rows. The [read] row gaps:

| Change | Frames | Time |
|---|---|---|
| Player 2's change apart | 4-6 | 66-100 ms |
| Player 1's change apart | 4-7 | 66-117 ms |
| A rejoin | 2-3 | 33-50 ms |
| Together, JTG | 3-7 | 51-117 ms |

r4c-s2 also skipped single rows after the arrival (3831→3833, 3833→3835).

- [inferred] The main loop is inside the change, so neither half is redrawn: the stayer's picture holds for those
  frames. In solo this is behind the fade's black. This matches the user's "player 1's screen pauses for a brief
  second" and coop-next-session.md's "VISetBlack per half (the load's 3-4 frames)".
- Separate item. It is not a camera move.

## 6. What is per room / per slot / per player today

| Data | Kept as |
|---|---|
| Camera records 0x80602B18 (target), 0x80603178 (eye), 12 × 136 bytes; stack 0x806038E8; depth r13-21700; index r13-26864 | One set in memory. The host keeps one per player (`g_coop_cameras.p1/.p2`, `COOP_CAMERA_SET`), swapped by `coop_cameras_in`. Not per slot [read] |
| A record's +124 (room entry) and +104 (who it follows) | Per room: +124 points into a room's camera data [inferred]. Per player: +104 names a player's object [read] |
| Live view 0x803012E0 | One. Player 2's own copy is `g_coop_own_view` [read] |
| Fade ticks r13-21784 | One word. The fade drawn per half, the mover's [read R4 record] |
| Zone list `g_coop_zones.list` | Player 2's room's list (their info +180). Its change is the "new room" trigger for player 2 [read] |

## 7. Candidate approaches (each opens only in co-op, ED_COOP_STAY, player 2's own bracketed change)

**A. Arm the R4c keep for every bracketed arrival of player 2.** Recommended, smallest. In `coop_stay_change_end` (or
`stay_scene_look_again`), set `g_stay_arrival_camera = true` whatever player 1's scene is. Keep the scene-only
`g_coop_p1_look` re-arm as it is.
- The first look's R4c branch in `coop_zones_new` (15954-15966) already resets player 2's zone words, prompt slots,
  looks, states and own close-up for the new room. It keeps `p2_set` and `p2_bare` false and does not zero
  `g_coop_own_view_frame`.
- Player 2's first step then runs on the arrival's cut camera, from the game's own `0x801EE764` and `0x801EEA9C`, as
  player 1's and solo's do. Their zone's fn8 in the fade-in blends from there, hidden by their own half's fade as in
  solo.
- Touches: one line's position in hle.c. Never runs in solo, because the bracket needs `coop_stay()` and
  `g_coop_room_p2`. Never runs with player 1's door bringing player 2 (no bracket).
- Covers both directions of player 2's door: apart, and back into player 1's room. It also covers user play 1's "no zone
  of theirs" arrival: they keep the arrival camera, as solo would.

**B. Never seed player 2's set from player 1's when their rooms differ.** In `coop_camera_p2_in`, refuse
`coop_camera_set_fresh` while `coop_rooms_apart`. This is a guard, not a fix: by itself player 2 would have no set at
all. Worth adding with A as a belt-and-braces check, logged once, so a later path that drops their set apart cannot copy
another room's camera (+124 into the other slot).

**C. Snap player 2's first step after an arrival** (call the game's `0x801EEA9C` in their set once). This is only needed
if, after A, a zone's camera at the arrival spot differs from the arrival camera and their half's fade-in does not cover
the blend. The data do not show such a room. Keep in reserve.

**D. Kit: an ARRIVAL rule in `ed check`.**
- For each half, the first eye after its player's room change must be within 60 units of a camera, or its rail, of the
  new room.
- Apart, it must not equal the other half's eye.
- It should flag the slow glide, which the current CAMERA rule (more than 120 a frame, change rows skipped) never sees.

**What not to do:**
- Do not move `g_coop_zones.list` to the new list at the bracket's end. That skips the zone-word, prompt and state reset
  for the new room, which the R4c branch does.
- Do not re-run the arrival's camera in player 2's turn. The game already ran it once, as theirs.

**Conflicts to check (for the refuters):**
- `coop_scene_set` at a later scene's end keeps player 2's set when `own = p2_set && !p2_bare && !fresh`. That holds
  after A, which is good.
- `coop_camera_chosen`'s follow (`room_choices++`) for player 1's change into player 2's room still runs only if player
  2 is bare. After A, player 2 is bare only after a together room change.
- The flag is consumed by the first `coop_zones_new` whose list differs. Every caller passes player 2's object's info
  (16548, 17921, 26430) [read the call sites, inferred that the object is player 2's in each]. Confirm no caller passes
  player 1's info while apart, or the flag would be spent on the wrong list.

## 8. Open questions

1. Is the change ever reached with the cameras still lent (an instant door with no motion)? If so:
   - `notify_ticket_run`'s `coop_cameras_in(2)` is a no-op (set_in already 2).
   - `coop_camera_chosen` returns at `lent` (17216-17217), so `p2_bare` is never cleared.
   - The bracket's `coop_cameras_in(1)` runs while still lent.

   No run shows it: `lent` was 0 at every change read.
2. The arrival point in no camera zone with A: is the arrival camera the right one to keep until a zone chooses? Solo
   keeps it [read `0x801EEA9C` cut, no zone in user play 1's 342 door spot], so yes, but it needs one look in play.
3. No post-R4c recording has player 2 arriving during player 1's scene (both rooms-play runs are 22:11 and 22:21; R4c's
   runs are 22:38). R4c's own path is unverified by a recording.
4. What moved player 1 by 93 units when player 2 arrived on their spot (p2-follows-181 5052-5054)?
5. The freeze rows (section 5): which call of the load holds the main loop. That belongs to the freeze item.

Files: hle.c 13858-13887, 15346-15416, 15429-15433, 15952-15980, 16067-16118, 16294-16304, 16544-16566, 17165-17237,
17584-17660, 20713, 20800-20802; camera.c 1724-1730; tools/ed/recording.py 228-262. Runs:
- logs/run-20261111-172710-rooms-r4c-t1-p2door
- logs/run-20261111-221649-r4b-s1-p2door, -221713-r4b-s2-p1door
- logs/run-20261111-223853-r4c-s1-p2door, -223919-r4c-s2-p1door
- logs/run-20261111-182644-rooms-r5-s1-p2door, -182711-rooms-r5-s2-p1door
- logs/run-20261112-033038-follow-test, -033642-p1-to-181, -033738-p2-follows-181, -030834-jtg-coop-gate,
  -030139-jtg-solo-returns
- logs/run-20261111-182534-jtg-rooms-r5-state, -112722-jtg-stepE-r6b-p1words-door
- logs/run-20261111-221141-rooms-play, -222138-rooms-play
