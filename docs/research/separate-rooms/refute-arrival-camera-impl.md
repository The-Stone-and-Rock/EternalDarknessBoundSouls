# Refuter: the arrival camera design against the current implementation (2026-11-12)

Item: `design-arrival-camera.md` (H1-H9 and the kit's ARRIVAL rule). Lens: conflicts with the code as it stands at
HEAD 5eb040d. I read hle.c, camera.c, the disassembly through `ed` and the two reader reports. I edited no code, built
nothing and ran nothing. **[read]** means seen at the place given; **[inf]** means inferred.

Verdict: **needs changes.** The core rule (H1-H6: keep player 2's own arrival cut, keyed to their room) fits the
bracket and breaks no hook. H8 has a hole and also changes the together case. The tag lives too long. The keep leaves a
stale view behind for camera modes 0 and 1. These need fixing before the build.

## What holds (checked, no conflict)

- **Hooks and passthrough.** H2-H4 and H8 go into existing notifies: `notify_ticket_run` on 0x80152614 and
  `notify_behaviour_pass` on 0x8014FDB8 (installed together, hle.c:27615-27623). H3 goes into `coop_camera_chosen`, the
  notify on 0x801EE764 and 0x801EE998 (27462-27467). There is no new address, intercept or passthrough flag. A notify runs
  after any intercept and lets the guest body run (hle.c:5537-5565) [read].
- **H3 always fires inside the bracket.** The ticket runner's camera block is unconditional: 0x80152654 `bl 0x80152298`,
  then both branches reach 0x80152784 → 0x80152790 `0x801EE998` → 0x801527CC `0x801EE764(0x80603288…)` [read
  `ed dis 0x80152614`, `0x80152740`]. There are two exceptions, below.
- **Player 2's info is per slot, whoever asks.** Both paths of `0x80123F70` return `0x801551A0(slot)`. The current
  player's object gets slot 2. Any other object gets the slot whose record +0x8140 equals object+592 [read
  `ed dis 0x80123F70`]. So the callers outside player 2's turn (18490-18505 timers, 26420-26433 Tome copy, 17920-17927 use)
  get the same list as their look does. The key on their object's room is sound for all of them.
- **`g_stay_arrival_camera` has no other reader** than 15951/15954/15955/17168 [read grep]. Removing it is safe.
- **Solo and co-op without the stay.** The bracket needs `coop_stay()` and `g_coop_p2` (13859). Without a bracket there
  is no tag, so H5 is today's drop [read].
- **Failed attempts.** Nothing here is repeated. p2camstart 2.diff (failed-attempts:524) counted *more* choices as the
  room's. H8 counts fewer. p1scene 3.diff (514) set `p2_set = false` at a scene's end. H7 only narrows that.

## Problems

### 1. MAJOR: H8 misses the path that actually counts for a bare player 2: player 1's first look in the fade

- H8 returns only when `room_change && g_stay_change_on && …` before `room_choices++` (design §3).
- But `coop_camera_chosen` also counts a choice made by **player 1's first look in the room** (`p1_zones == 1`):
  `if (!room_change && p1_zones != 1) return; if (coop_stay_apart) return; room_choices++` (hle.c:17230-17236) [read].
- `coop_scan_room_start` runs that look while `p2_bare` (19788). It marks it first when player 1's list differs from
  `p1_room` and the fade ticks r13-21784 are above 0 (19785-19795) [read].
- In case D (player 1 comes into player 2's room while player 2 is bare), the room word is player 2's room, so
  `coop_stay_apart` is false. The region at player 1's entry spot chooses during the fade, which the design's own §1 says
  is the normal case. So `room_choices++` still runs, and player 2 still follows at their next turn (15405-15416).
  [read code; inf effect]
- `g_stay_change_on` cannot cover it either. H8 clears it at the behaviour pass of the change's own frame, and the fade's
  first look comes in the following frames [inf from the 10-tick fade at 0x801521A8].
- **Fix:** key D on the room, not on the change. Skip `room_choices++` (both the room_change and the `p1_zones == 1`
  branch) when `coop_stay()`, player 2's object +592 equals r13-23276, and player 2 was in that room before player 1's
  change began. That needs a per-change "player 2's room at the change" kept until player 1's first look is done, not
  cleared at the behaviour pass. C2's check would not catch the gap: player 2 is not bare there.

### 2. MAJOR: H8 changes the together case at a same-room change

- H8's test is "player 2's room before the change == the new room, and `p2_set`". With the players **together**, any
  change of player 1's whose target is the room they are both in passes that test. The ticket runner handles a
  same-room change: R4's notes use "the same room" (hle.c:13847).
- The design names one itself in §6: L01/1100-1103 "transition 0 into 183". From room 183 that is a change into 183.
- After Jump to Game or a chapter start, player 2 is bare until a zone of theirs chooses. A same-room change of player
  1's then no longer counts for them, and they stay on the old camera while player 1 cuts. That breaks the rule "nothing
  may change the players-together case" [inf].
- **Fix:** also require that the players were apart when the change began: player 2's room ≠ r13-23276 at
  `notify_ticket_run`'s entry (the room word is player 1's there).

### 3. MAJOR: the tag outlives the arrival

- H5's kept branch never clears `p2_arrived`. Only the drop branch, `coop_camera_set_fresh`, the scene's afresh, the
  cutscene put-back and the state load clear it (H5, H6).
- So while player 2 stays in room R, **every** later new zone list keeps whatever set they then have, with `p2_bare`
  false. The room's-start follow (15405) is then skipped for them.
- Paths that give a new list in the same room number without a bracket, an H6 clear or a drop first:
  - an in-game load or continue into R;
  - a chapter or JTG start in R;
  - the planned "Bring X to Y" teleport putting player 2 back into R;
  - a room re-unpacked into another slot.
  None of these runs `coop_camera_set_fresh` before the list changes: the only remake is in `coop_camera_p2_in`, which
  runs after `coop_zones_new` [read 15398-15416, 16551, 16566]. Rooms of one place come back in later chapters (Pius's and
  Karim's both in the Forbidden City) [inf: whether they share room numbers is not read].
- In all of these player 2 keeps the camera of an earlier visit, which is the bug in another form.
- **Fix:** clear the tag at every ticket run that is not player 2's own bracket and that moves player 2's object
  (compare +592 before and after, at `notify_behaviour_pass`), and on the JTG / new-game / title paths. Better still, keep
  the tag only for the zone list of the arrival's first look: store that list in the kept branch, and drop on any other
  list in R. That still allows the old-room `g_coop_use.info` round trip the design worries about, because the old list
  is another room's.

### 4. MAJOR: the keep leaves player 2's old-room view live in camera modes 0 and 1

- Today's drop zeroes `g_coop_own_view_frame` at every new room (15977). H5's kept branch does not.
- In `mod_p2_camera` 0 (the default, settings.c:64) and 1, `coop_camera_step` returns before stepping (16072). So
  `g_coop_own_view` is only written at a lend's end (15483-15484): it is the view of their last close-up, possibly rooms
  ago [read].
- With the frame kept non-zero across an own door:
  - `coop_cameras_lend(on)` writes that stale view into COOP_CAMERA at their next lent close-up (15479-15481), so the
    close-up's camera eases from another room's view. This is the effect the comment at 15468-15473 describes.
  - `coop_found_place` places their found thing by it (16172 onward).
  R4c already has this, but only inside player 1's scene. H5 makes it happen at every own door [read code; inf effect].
- **Fix:** zero `g_coop_own_view_frame` in the kept branch too. In mode 2 the same turn's `coop_camera_step` (20802)
  writes it again from the kept set before any draw. `hle_coop_player2_camera` falls back only while it is 0 (16295), and
  camera.c reads it at the draw (1728). Or zero it unless `mod_p2_camera == 2`. The design's claim "the keep never zeroes
  `g_coop_own_view_frame`, so their half never shows the game's view" then needs the turn-order argument instead.

### 5. MINOR: H7 adds a third copy of an existing helper

- The new `coop_apart_by_objects` is `r6_apart` (hle.c:24197-24202): co-op on, `coop_stay()`, no bracket, both objects'
  rooms differ and are resident.
- `coop_rooms_apart` (13926) is the same without the resident test.
- Use `r6_apart`, the rule R6/R5 already gate on, so the camera and the enemies/sound agree on "apart".

### 6. MINOR: H9's log fires falsely at the bracket's start

- `notify_ticket_run` calls `coop_cameras_in(cpu, 2)` (13865). With `!p2_set` (after a state load, or after a scene's
  afresh), that remakes from player 1's set in `coop_camera_p2_in` while the objects are apart. That is H9's condition.
- The remake is then overwritten by the arrival's cut, so the line misleads, and C1's "No H9 line" could fail for that
  reason.
- **Fix:** gate H9 on `g_stay_bracket == 0` (`r6_apart` already does).

### 7. MINOR: H3 is skipped when `coop_camera_chosen` stands aside or returns early

- `coop_camera_chosen` returns at `lent` (17213) and is on the Tome copy's stand-aside list (26609, checked in
  `hle_dispatch` 5533-5535). `notify_ticket_run` is not on that list.
- The design covers `lent` (U2). It does not cover a bracket run during `g_tome_copy_turn`. That case is untagged, which
  is today's drop and so safe, but U3's log reading should name it [inf: whether a ticket runs inside the copy's turn is
  not read].

### 8. MINOR: new flags are not cleared at a state load

- `hle_coop_state_load` (28020-28064) clears `g_coop_zones.list` and `lent`. It does not clear `g_stay_bracket` or
  `g_coop_room_p2`, which is older than this design. It would not clear the new `g_stay_bracket_chose` or
  `g_stay_change_on` either.
- `hle_save_held` (28008-28017) does not wait on `g_stay_bracket`. If a load inside 0x80152298 spans frames, a state could
  be taken mid-bracket [inf].
- Clear the two new flags beside H6's tag clear.

### 9. MINOR: H6's clear in `coop_cutscene_over` is not needed and costs a drop

- `coop_cutscene_over` (25664-25666) saves the live set as player 2's after **their** cutscene in the same room. A
  cutscene that ends in another room returns before it (25657-25660).
- Clearing the tag there makes the next list change in R drop to player 1's set. If the tag is kept as problem 3
  suggests, leave this clear out.

### 10. MINOR: the kit's ARRIVAL rule flags correct arrivals

- "Eye within 5 units of the other half's eye" is true for a correct rejoin at a shared door camera. The design's own C3
  expects both halves on Camera02, and -033738-p2-follows-181 has both on Camera01 (code readers §2c).
- "More than 300 units in 60 frames" can be crossed by a following camera while a player runs.
- Require that the other half's eye be in the other room (p1room ≠ p2room on that row), and measure the travel only
  while the player stands still.

## Interactions read and found clean

- **R4b** (`stay_waits_clear`, `stay_busy_clear`, `g_coop_p1_tickets`): untouched.
- **R4c's scene re-arm** in `stay_scene_look_again`: kept.
- **`coop_p2_own_cancel`** still runs before the keep or drop. It puts player 2's set in itself (15782-15783).
- **The arrival-scene timers path** (18471-18526) calls `coop_zones_swap` with their info. With the keep that is only a
  zone reset. `coop_p1_scene_end_camera` (18525) needs `lent`.
- **`coop_camera_step`'s rename** of player 1's object to player 2's (16081-16083) finds nothing in the kept set. The
  bracket's `coop_be_player2` made r13-26524 player 2's object for 0x801EE43C (12297-12304) [read].
- **The rejoin** (player 2's door into player 1's room) leaves player 2 "own", not bare. At player 1's next scene's end,
  `own` keeps their camera (17140), as it does for any together player whose zone has chosen. That is acceptable, but
  it is a change to the together case and should be named as such.
