# The camera jitter at an arrival apart: code reader (2026-11-12)

I only read code, disassembly and existing logs. I changed no code, built nothing, and did not run the game.
- **[read]** means I saw it at the address, file:line or log line given.
- **[inf]** means an inference.
- hle.c line numbers are for the tree as of HEAD 5eb040d plus the uncommitted controls.cpp.
- The user plays with `mod_p2_camera = 2`, the game's own camera for player 2. Their detail lines read "cam 0/2" (settings.ini:55, logs/run-20261111-222138-rooms-play/page-0001.log:124) [read].

## 1. Answer in short

**Which half.** The jitter is in **player 2's half**. It happens at **every arrival of player 2 through their own door** while ED_COOP_STAY is on and no scene of player 1's is running. That includes going into a room apart and coming back into player 1's room.

**Player 1's half** cuts cleanly at their own arrival apart, as in solo, in both directions. There is one exception: a renderer hitch of a single moment (section 5e). I found no host write that fights player 1's camera at either player's arrival.

**What happens, frame by frame.** The arrival's own camera does reach player 2's set: inside the R4 bracket, the game cuts to the door's camera. Later in the same frame, player 2's first zone look in the new room throws that set away:
1. `coop_zones_new` (hle.c:15952) sees a new zone list. Outside the R4c scene case it clears `p2_set`, sets `p2_bare` and zeroes `g_coop_own_view_frame` (hle.c:15967-15978).
2. The same turn's camera step remakes player 2's set as a copy of the game's set, which is **player 1's** (`coop_camera_p2_in` → `coop_camera_set_fresh`, hle.c:15364-15373, 15398-15416).
3. That copy's current records hold player 1's live eye, in **player 1's room's** coordinates. So player 2's first view after the arrival is player 1's eye.
4. The camera region at player 2's entry spot then runs its `fn8` with **blend on** (c = 1). That eases player 2's camera from player 1's eye to the region's camera, 1/20 or 1/10 of the way each frame.

The result is a swoop of about 0.8 to 1.2 s. It starts at an angle that belongs to the other room: in shots it shows walls and pillars close up. In solo the same `fn8` blends from the door's camera to the same camera, so nothing moves.

The R4c readers predicted this (arrival-readers.md:224-228, "where a zone's choice masks it [inf]"). The logs show the zone's choice does **not** mask it, because that `fn8` blends rather than cuts.

## 2. The frames (logs)

### 2a. Player 2's door 183 → 180, player 1 stays in 183

Run: logs/run-20261111-172710-rooms-r4c-t1-p2door. The same pattern is in -223853-r4c-s1-p2door on the R4c build, at 3830/3833 [read].

- **3822** [read, page-0001.log:644-666]: the bracket starts ("player 2's change to room 180 runs as theirs").
- **3824** [read, page-0001.log:666, 675]: "player 2's change done", then in the same frame "[trace] player 2's camera jumps 2760 in a frame".
  - 2760 is exactly the distance from player 2's old eye (352, 2364, 349) to player 1's eye (-757, -154, 566): √(1109² + 2517² + 217²) = 2759 [read both.tsv 3822/3826; computed].
  - So already at 3824, their step's result was player 1's eye. The arrival's Camera02 never showed.
- **3826** [read, page-0001.log:686-691]: player 2's look runs region script 1062 (`who 59`), which calls `fn8` 0x801669EC. The log line is "[coop] player 2's zone chose their camera".
- **Script 1062** [read, `ed script 1062`]: `fn8(Camera02, 0, 2, 1, 0)`, `fn3(20)`, `fn2(20)`. That is c = 1 (blend) with divisor 20.
- **Door L01/119** (183 → 180 at EntryMkr02) has arrival camera **Camera02** [read, `ed door 180`]. In solo, the region therefore blends Camera02 to Camera02: no movement [inf].
- **both.tsv p2e, 3826 to 3858+** [read]: -757.1 → -722.4 → -689.4 → -658.1 → …
  - Each step is 0.95 of the one before: the 1/20 ease.
  - The goal works out to about (-63, 276, 486). That is where player 1's own arrival through the same door cuts to: E = -63, 276, 486 in logs/run-20261111-182711-rooms-r5-s2-p1door, 3833.
- **`ed timeline`** [read]: cam2 is "free" from 3810 to 3858 and "Camera02" from 3858. `ed see 3830` gives fade 5, so the first ~10 frames are under player 2's fade-in. The rest is visible.
- **Shots** [read]: shots/000013.jpg (≈3835) and 000014.jpg (≈3851). Player 2's half (right) shows room 180's walls and a brazier close up, from player 1's eye position. Player 1's half (left) is steady.

### 2b. Player 2's door 180 → 183, back into player 1's room

This is the kit's "nine-frame CAMERA blip".
- **4362** [read, page-0001.log:1246-1265]: the bracket, then "player 2's camera jumps 959". 959 is the distance from (-63, 276, 486) to player 1's eye (-826, -297, 579) [computed].
- **4364** [read]: region script 294 (`who 59`) runs `fn8(Camera25, 0, 2, 1, 0)`, `fn3(10)`, `fn2(10)` [read, `ed script 294`]. Door L01/29's arrival camera is Camera25 [read, `ed door 180`].
- **both.tsv** [read]:
  - At 4364, p2e = -826.1, -297.0, 579.0, equal to player 1's E.
  - It then steps 292, 263, 236, 213 … units a frame: a 1/10 ease toward Camera25 near (352, 2364, 349). The trace prints one "camera jumps" line a frame, 4364 to 4379.
- **`ed check`** [read]: flags only 4364-4373, the frames more than 240 from any camera of room 183. **`ed timeline`** [read]: cam2 is free from 4350 to 4396 and Camera25 from 4396. So the visible ease lasts about 30 frames.

### 2c. The same on the newest build

Run: logs/run-20261112-033738-p2-follows-181, player 2 into room 181 where player 1 is [read].
- 5052: "camera jumps 1700". From 5054, p2e = player 1's E exactly (-1299.1, 1188.6, 424.8).
- There is no swoop here, because player 2's region camera is the one player 1 is already on (Camera01). The arrival's own camera was still dropped. How visible the drop is depends on the room.

### 2d. Player 1's door apart: player 1's half is clean

- **logs/run-20261111-182711-rooms-r5-s2-p1door** [read both.tsv], 183 → 180 with player 2 staying in 183:
  - E holds at 352, 2364, 349 up to 3826, then at 3833 cuts to -63, 276, 486 (Camera02) and stays still.
  - p2e keeps easing on (-747 → -748.7 …) with no jump across the change.
- **logs/run-20261111-171852-rooms-r4-t2-p2door-p1follows, 4272-4274** [read], player 1 entering the room where player 2 already is:
  - E cuts to Camera02.
  - p2e stays (-63, 276, 486), because player 2's set was their own: `p2_bare` was false since their zone chose at 3826.
- The frames missing from both.tsv during a load (3827-3832; 3823-3825 in t1) are the load itself. That is the separate "brief freeze" item.

## 3. How the game sets the camera at an arrival (solo), and who it reads

The ticket runner **0x80152614(room, entry, camera hash, script, flags)** is called from the ticket tick 0x80155830 at 0x800083EC [read, `ed dis 0x80152614`, `ed dis 0x800083E0`]. After 0x80152298, which does the load, the slot swap and 0x80151FDC, it runs:

| Address | What | Data it uses [read] |
|---|---|---|
| 0x80152790 | `0x801EE998(hash, 2, 0)`: the camera's index | the current slot's record section |
| 0x801527CC | `0x801EE764(0x80603288 = eye record 2, index, 0,0,0,0,0,0; stack: src 0, flag 1)` | `0x80155130(2)`: the CURRENT slot's record (+0x8142/+0x8143 loaded bytes, +164/+172 the camera tables, +162 the count; 0x801EE7A0-0x801EE818). Writes eye record 2 and its target record (+116) through 0x801EE43C twice; +96 := src. c = 0, so the current target record 0x806037D8 +64 := 1 (cut pending, 0x801EE898-0x801EE8B4). flag ≠ 0, so 0x800860B8 runs (the sanity roll, below). |
| 0x801527D0 | `0x801ECCE0`: stack depth r13-21700 := 0 | |
| 0x801527D8 | `0x801EEB58(2)`: index r13-26864 := 2; the current records' +112 := record 2 and its target | |
| 0x801527DC | `0x801EEA9C`: **the cut**. Both current records (0x806037D8, 0x80603860) are copied from their records: place +0..+8, FOV +52, +72..+80; +64 := 0 | It first calls 0x800860B8 |
| 0x801527F4 | the arrival script `0x801642D4(script)` | |

- Inside 0x80152298, **0x801EB550** at 0x8015211C clears +108/+124 of the live records. Those are the movers and the room-entry pointers of the set that is in [read in doors-readers.md:62]. This is the game's own "a set belongs to a room" reset.
- **The step** is `0x801EB9A0(3)`, at 0x80008424 in the main loop, after the ticket tick (0x800083EC) and the behaviour pass (0x800083FC) [read]:
  1. It runs each record's mover (+108, `bctrl`).
  2. If the current +64 is set, it cuts (`0x801EEA9C`).
  3. It writes the view 0x803012E0 **only from the current records**: eye 0x801EB098, target 0x801EB104, FOV 0x801EB14C, up 0x801EB0BC; 0x801EB9A0-0x801EBBA4 [read].
  - The previous view is never read back. All of the ease state is in the set.
- **The room's first frames:**
  - The fade-in is `0x801E7C88(type, 10)` at 0x801521A8, with ticks at r13-21784 [read, camera.md, doors-readers].
  - The player's first zone look happens in the fade-in. The region they stand in runs its `fn8` → 0x801EE8E0 → 0x801EE764. With c = 0 it cuts (+64 := 1). With c ≠ 0 it blends from the current records, using divisors from `fn3`/`fn2` [read 0x801EE8E0-0x801EE994; camera.md].
  - The room regions at a door's entry choose the door's own camera (1062: Camera02; 294: Camera25) with a blend. In solo the screen therefore holds the cut [inf].
- **Whose data the arrival reads:**
  - The arrival camera functions read **no player word**. Their "player" is whichever **camera set is in the live records** 0x80602B18-0x8060394C, plus depth and index [read in doors-readers.md:62]. The exception is 0x801EE43C's +104 := r13-26524 ("who it follows").
  - **0x800860B8** (the roll, reached through every cut) finds its character by walking the global node list for the first node of kind 32 (0x801F6310 → 0x801F5C20 = r13-21468 → +4, then 0x801F8FD8(…, 32)). It does not use r13-21492. It then gates on busy (0x80155738), the hold (0x8004303C), r13-25400 and r13-23276 ≠ 5, and writes the roll into **record 2** of the set that is in, through 0x801EEDB4(2, 1, f) or 0x801EEE90 [read 0x800860B8-0x80086244, 0x801EEDB4].
  - So when player 2's set is cut, its roll is worked out from the first kind-32 node, which is likely player 1 [inf]. This is a roll, not a jump.

## 4. What the host does at an arrival today, for each player

### Player 2's own door, with the stayer (ED_COOP_STAY)

| Order in the frame | Host code | Player 2's set | Player 1's set |
|---|---|---|---|
| Ticket tick, 0x80152614 entry | `notify_ticket_run`, hle.c:13858-13868: `coop_be_player2`; r13-23276 := their room; `coop_cameras_in(cpu, 2)` | In. It is their own set: `coop_camera_p2_in` does not remake it while `p2_bare` is false | Saved into `g_coop_cameras.p1` |
| Inside the change | 0x801EB550 clears; the arrival cut writes record 2 and cuts | Gets the door's camera, cut [read in code] | Untouched [read: E smooth across 3822-3826 and 4362-4364] |
| `0x801EE764` on record 2 | `coop_camera_chosen`, hle.c:17206-17239: `room_change && g_stay_bracket` → `p2_bare = false` (17219-17222) | | |
| Behaviour pass entry (lr 0x80008400) | `coop_stay_change_end`, hle.c:13870-13889: `coop_cameras_in(cpu, 1)` (`p2_set = true`), slot table back; `stay_scene_look_again` (17165) sets `g_stay_arrival_camera` **only if `coop_scene_on_of(0)`** | Saved with the arrival cut in it | Back in |
| Player 2's turn (in the behaviour pass) | `coop_player_turn` (20541): `coop_stay_room_in` (20689) puts their slot current; `coop_prompt_look` (20713) → `coop_zones_new` (16551 → 15952): the zone list differs | **No scene:** the drop at 15967-15978 (`p2_set = false`, `p2_bare = true`, `g_coop_own_view_frame = 0`). **Player 1's scene on:** the R4c keep at 15954-15965 | |
| Same turn | `coop_camera_step` (20802 → 16067): `coop_cameras_in(2)` → `coop_camera_p2_in` → `coop_camera_set_fresh` (15364): player 1's pushes popped, **player 1's records and current records become player 2's**; records naming player 1 renamed to player 2 (16081-16083); step → `g_coop_own_view` = player 1's eye | Remade from player 1's | Restored by `coop_cameras_in(set_was)` |
| Main loop, 0x80008424 | The game's step | | Player 1's view, unchanged |
| Draw | `notify_look_at` (6219) → `camera_split_update` (camera.c:1600). Player 2's half = `hle_coop_player2_camera` (hle.c:16294; the game's eye while `g_coop_own_view_frame == 0`), re-projected relative to the game's eye (camera.c:1785-1805) | Shows player 1's eye, then the ease | kIdentity |

### Player 1's own door, player 2 staying

- There is no bracket: it needs `g_coop_room_p2` (13859). The game's set (player 1's) is in, so the arrival cut is player 1's, as in solo [read].
- `coop_camera_chosen` returns at `coop_stay_apart` (17233-17234), so `room_choices` doesn't move and player 2's set is not remade [read].
- Player 2's turn during player 1's change skips their look (`room_other`, 20700-20702). Their step runs on, with their own slot current [read]. p2e is continuous in s2-p1door [read].

## 5. Writes that can fight at an arrival (both players)

**a. The drop at player 2's first look (the jitter).** `coop_zones_new`, hle.c:15967-15978, then the remake `coop_camera_set_fresh` [read].
- **Player 2:** a view taken from player 1's eye in player 1's room, eased into their region's camera; or, with no region at the entry spot, player 1's records kept (below).
- **Player 1:** none.
- Happens at every own arrival of player 2 apart without player 1's scene, both into another room and back into player 1's room [read, runs 2a-2c].

**b. No chooser at the entry spot** [inf, not seen in a log].
- If player 2's entry spot is in no camera region, nothing chooses in their look. After the drop they then keep player 1's records: player 1's room camera, whose movers and +124 entry pointers point into player 1's slot. They are stepped in player 2's room until player 2 walks into a camera region.
- Failed-attempts:166 records such a spot: Anthony's stairs, where "no region sets another camera".
- Meanwhile `p2_bare` stays true, which opens (c).

**c. Player 1 walks into player 2's room while `p2_bare`** [read code 17233-17239 and 15405-15416; inf for the effect].
- At player 1's arrival the room word is already player 2's room, so `coop_stay_apart` is false and `room_choices++` runs.
- At player 2's next turn `coop_camera_p2_in` "follows". Their set is remade from player 1's, which was just cut to player 1's door camera, and player 2's half cuts to it.
- Player 1's half is unaffected.

**d. Player 1's scene ends while apart** [read 17118-17147; inf for the effect].
- `afresh = !own && (fresh || (chose && shared)) && !coop_left_behind(cpu)` (17141).
- `coop_left_behind` is false for a resident other room (15992-15995), so nothing here stops it for players apart.
- With player 2's set bare, or fresh inside the scene (as it is after (a) with player 1's scene on but no R4c keep), `p2_set = false` (17147). Player 2's half then retakes player 1's room camera at their next step.
- R4c's keep makes their set "own" in the scene case. The bare case after (b) remains open.

**e. The renderer's in-between moments** [read .deps/GXRuntime/graphics/aurora/lib/gfx/common.cpp plan_between ~1549-1610; inf for visibility].
- At 120 Hz (the user's display: play-rooms.log:4 "120.15 Hz"), a moment between two game frames is drawn only when `usable`:
  - the game frame's blend is Blend or Still, and
  - **both** players' views are within 150 units and rotation 1.5 of their last, where the views are relative to the game's (player 1's) camera.
- A cut in either player's camera (either player's arrival), or the interp's own Cut result (`interp.cpp` ~396-401), drops the moment for **both halves**. The other player's picture repeats one refresh.
- Player 2's relative view also jumps when only player 1's camera cuts, even if player 2's world view is still.
- This is a one-refresh hitch, not the swoop. It touches the separate "brief freeze" item.

**f. Third-person and first-person modes for player 2** (`mod_p2_camera` 0 or 1; 0 is the default, settings.c:64) [read hle.c:6263-6310, 6587; camera.c:1760-1781; inf for the effect].
- Player 2's wall test `line_reach` → `camera_collide` takes `r13 + g_camera_player_offset`'s object, which is player 1's, because it runs from the draw's look-at outside any turn. `g_collide_world` 0x80123F70 then finds the collision **by that object's room** (+592).
- When the players are apart, player 2's third-person eye is pushed by player 1's room's walls.
- This is not the user's mode, but the rule must hold in every mode.

**g. The refused-turn camera step** (hle.c:20577-20612) [read; inf for the effect].
- It runs `coop_camera_step` before `coop_stay_room_in` (20689), so with **player 1's slot current**.
- Most of the step reads absolute record pointers, but the cut's roll 0x800860B8 reads r13-23276. Low risk. Note it.

**Not fighting** [read]:
- `camera_index_heal` (14899): only the game's set on index 8 with depth 0, at the mode switch.
- `coop_position_copy` (21363): reads COOP_CAMERA for the detail share and writes no camera.
- `coop_player_turn`'s write of player 2's last view into 0x803012E0 (20671-20676): the step never reads the view (section 3), so this is for the controller's camera-relative stick.

## 6. Per room, per slot, per player

- **Per player** (host copies): the camera set, meaning the records 0x80602B18-0x806038E0 (12 target + 12 eye + 2 current, 136 bytes each), the stack 0x806038E8-0x8060394C, depth r13-21700 and index r13-26864 (`COOP_CAMERA_SET`, hle.c:15335-15361); and the view (`g_coop_own_view`, `g_coop_p1_view`).
- **Per slot:** the camera tables a choice reads (`0x80155130(2)` → the current slot's record +164/+172). The records' movers and +124 entry pointers point into the slot whose camera table built them [inf from 0x801EB550's clear and camera.md's +124].
- **One for the game:** the view 0x803012E0; the fade ticks r13-21784; the busy word; r13-23276.
- **What is not per room today:** player 2's set is not tied to a room. Its only room key is `g_coop_zones.list`, the zone-list pointer, compared in `coop_zones_new`.
  - [inf, open] Two different rooms unpacked into the same slot could give the same pointer. If so, the drop, and the clear of their zone words, prompts and states, would not happen at all.

## 7. Candidate approaches (all co-op only, all inside ED_COOP_STAY's bracket or apart tests)

**A. The smallest change: arm the R4c keep at every own arrival of player 2.**
- What it does: in `coop_stay_change_end` (13870-13889), set `g_stay_arrival_camera = true` for every bracket, not only in `stay_scene_look_again` behind `coop_scene_on_of(0)` (17166-17168). The keep branch, `coop_zones_new` 15954-15965, already does the rest: it clears the zone words, prompts, looks and states, and leaves `p2_set`, `p2_bare` (false since the bracket) and `g_coop_own_view_frame` alone.
- Effect: player 2's first step shows the door's camera, cut under their own fade, and the region's blend goes from that camera to itself, as in solo [inf].
- Solo, and co-op without the stay switch, never reach it (`g_stay_bracket` needs `coop_stay()`).
- Touches one line in hle.c.
- Risk: the flag is consumed only by a later list change. See section 6's pointer question and open question 4 (a use's stale info).

**B. Do the room's reset at player 2's own change, not at their first look.**
- What it does: in `coop_stay_change_end`, after the identity and slot table are back, call 0x80123F70(their object), which finds the info by their +592, i.e. their slot. Take its +180 as `g_coop_zones.list`, and do the keep branch's resets there: `coop_p2_own_cancel`, zone words, prompt slots, looks, `g_coop_states.p2`. Leave the set alone.
- Their first look then finds the same list and drops nothing. This keys on the change itself, as the game's own 0x801EB550 does, not on a pointer. It also covers the same-pointer case, and it can replace R4c's flag.
- Touches `coop_stay_change_end`, `coop_zones_new` (factor its new-room reset into a helper) and the R4c flag (removed).
- Guest calls from this notify are already done in the bracket (`coop_camera_set_fresh`'s pop) [read 15364-15373].

**C. Tag player 2's set with the room and slot it was made for. A remake drops it only when their object's +592 or slot differs.**
- Set the tag at:
  - `coop_stay_change_end`: their new room and slot;
  - any remake (`coop_camera_set_fresh`, `coop_camera_p2_in`): the room in play then, which is theirs inside their turn.
- This also gives the coming "Bring X to Y" teleport its camera rule. A player moved into another room without a door gets a remake, or better the game's own arrival camera: `0x801EE998`/`0x801EE764` + `0x801EEA9C` run on the mover's set, with the slot of the destination current.
- Touches the `g_coop_cameras` struct, `coop_zones_new`, `coop_camera_set_fresh`, `coop_stay_change_end`, and later the teleport.

**D. Guard for (c).** In `coop_camera_chosen` (17230-17239), do not count a room change's choice into `room_choices` when `coop_stay()`, player 2's object is already in the new room, and they have a set of their own (`p2_set`).
- A chapter start that brings player 2 still counts: their +592 is not the new room yet [inf, to confirm].
- Touches hle.c:17233.

**E. Guard for (d).** Add "not apart" to `afresh` at hle.c:17141: `&& !coop_stay_apart(cpu)`. Player 1's scene in their room never remakes the camera of a player in another room.

**F. Renderer moments per half (for (e)).** In `plan_between`, decide `usable` for each half, and compare each player's world view rather than the view relative to player 1's camera.
- Touches .deps/GXRuntime common.cpp (and the patch). Interp's game-frame Cut still kills player 1's moments as in solo.
- Lower priority. It belongs with the "brief freeze" item.

**G. Third-person apart (for (f)).** Let `camera_split_update`'s wall test ask with player 2's object, with their slot current, when apart.
- Touches camera.c (`g_collide` gets a who argument) and hle.c `camera_collide`.

**Recommended order:** B (or A as the minimum), then D and E as apart-only guards. F and G after.

**Checks after a build** (from the existing recipes; none were run here):
- r4c-t1/r4c-s1 p2door: `ed check` should lose the CAMERA 4364-4373 finding. `ed timeline` cam2 should read Camera02/Camera25 from the first frame after the fade, and the trace should show one "camera jumps" line at the arrival (the cut under the fade) and no run after.
- s2-p1door and the together runs should be unchanged.
- A solo run should show no new log line.

## 8. Open questions

1. How many door entry markers lie in no camera region (case b)? `ed where ROOM X Y Z` for every entry marker in every room would settle it. Not done here.
2. Can the zone-list pointer repeat for a different room unpacked into the same slot? A one-line probe settles it: log `g_coop_zones.list` old and new at each `coop_zones_new`, together with player 2's +592. If it can repeat, today's per-room reset of player 2's zone words, prompts and states is also skipped there. That argues for B.
3. Which node 0x800860B8's walk (first kind 32 in r13-21468's list) finds when two players are listed. If it is always player 1, player 2's camera roll at every cut uses player 1's sanity.
4. Can a use of player 2's (`g_coop_use.active`, `coop_zones_swap` with `g_coop_use.info`, hle.c:17927/17979) last across their change with the old room's info? If so it would call `coop_zones_new` with a stale list after the first look and drop the set again. It did not in these runs: the door's use ended at 3761 [read both.tsv lent column].
5. The user's own recordings after R4c have no camera trace: play-rooms.log has no ED_COOP_LOG or trace lines. A `play-rooms-log.bat` recording would confirm the user's jitter is this swoop. Expected signs: "player 2's camera jumps N" at their arrival, and p2e equal to E on the next row.
6. Whether the one-refresh hitch (e) can be seen at 120 Hz, by the screen-video method in coop-failed-attempts.md:312.

Files read: runtime/host/hle.c (lines above), runtime/host/camera.c:1529-1822, .deps/GXRuntime/graphics/aurora/lib/gfx/common.cpp:1500-1680, .deps/GXRuntime/graphics/aurora/lib/gfx/interp.cpp:310-420, 525-560, docs/game/systems/camera.md, docs/research/separate-rooms/arrival-readers.md, doors-readers.md, readers.md, docs/coop-level3-design.md:2287-2550, logs/run-20261111-172710-rooms-r4c-t1-p2door, -223853-r4c-s1-p2door, -182711-rooms-r5-s2-p1door, -171852-rooms-r4-t2-p2door-p1follows, -222138-rooms-play, logs/run-20261112-033738-p2-follows-181, logs/play-rooms.log.
