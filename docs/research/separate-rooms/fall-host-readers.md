# Fall readers: the host paths that move, place or step a player (separate rooms)

The bug (the user, 2026-11-12, separate rooms on): in Pius's chapter, player 1 went from the statue room 182 into 181. Player 2,
still in 182 and about to follow, fell through the floor and could not move after. The user's log is lost. Scripted attempts
did not reproduce it.

This reading covers every host path that writes a player's place, or runs a player's movement or grounding, for both
players (player 1 can be the stayer). For each one it asks whether the path can run player 2's movement or grounding with
player 1's room current, or place them by the floor of the room in play. Nothing was edited, built or run.

`runtime/host/hle.c` was being edited by another session during this reading (`git diff --stat` showed +330/-38 on top
of 5eb040d), so its line numbers moved by up to about 150 between greps. Functions are named first, and their line numbers
are approximate.

## 1. The one mechanism, from the disassembly

- [read] `0x80123F70(object)` is the collision world getter that each step and the camera use (`ed dis 0x80123F70`). At
  0x80123F8C-0x80123F90 it gets the current slot's record (`0x801551A0(2)`: table `0x8058AFC0`+0, returned only when its
  +0x8142 and +0x8143 are non-zero). At 0x80123F94-0x80123FA0, **when the object is `r13-26524` (the player's object), it
  returns that current slot whatever the object's +592 says.** For any other object, 0x80123FA4-0x80123FFC loops over n=2,3
  (0x80155130: table +0 and +4, with +4 == -1 skipped) and takes the slot whose record +0x8140 equals object+592 and whose
  +0x8142 and +0x8143 are set. If no slot matches it returns 0 (0x80124008).
- [read] Each move goes through `0x801240B8(object, to, mode)`. It builds from and to with the object's +612 offset
  (0x80124170-0x8012419C), then calls the step `0x80133434` (0x801241B4), which calls `0x80123F70(object)` at 0x80133468.
  When that returns 0, the step stores `from` back (0x8013348C-0x8013349C), so the object **stays put**: it does not fall.
- [read] 0x801240B8 has 13 call sites. They are the player's event dispatcher at 0x8005A72C (event 157, "move to", mode
  3: `cmpwi r18,157` at 0x8005A70C), the sanity-effect code (0x8006B8BC/0x8006B90C, 0x8009D3B8/0x8009D4A0,
  0x8009E520/0x8009E5B8 = effect 35 Sinking Feeling, 0x8009E8C0/0x8009E95C, 0x800A0964, 0x800A3540), 0x800C4694 (inside
  0x800C45A8, the player's event 165 at 0x8005A700), and the general mover 0x80124030 (from 0x801237C0 and 0x80123DBC). The
  step 0x80133434 itself is also called from 0x8004582C and 0x801FA494 (mode 1/3, from == to: not a move; comment at
  `coop_p2_step_seen`).
- So the bug's mechanism needs one of these conditions:
  - **(A)** a frame or call where `r13-26524` is player 2's object while table +0 is player 1's room.
  - **(B)** player 2's object+592 names a room that isn't the one they stand in.
  - **(C)** a host write of player 2's place measured on the wrong room's floor (the ED_PUT bug, now fixed).
  - **(D)** the slot holding player 2's room itself has a damaged collision tree. That is outside this reading; see §6.

  [inferred] In each case the body is grounded once against a floor that isn't under it, ends below its own floor, and the
  next step against its own room finds nothing under it.

## 2. Path by path

The verdict column says whether the path can ground or place a player against the other room.

| Path (hle.c) | What it does to a body | Room it collides with | Verdict |
|---|---|---|---|
| The game's own behaviour pass (`0x8014FDB8`, frame 0x800083FC) | Steps every body. Player 2 is a non-player object (`r13-26524` = player 1's) | Player 2: their slot by +592 [read 0x80123FA4]. Player 1: the current slot = their own | Safe while `r13-26524` is player 1's at the pass and the stayer's record is loaded |
| `coop_character_step` (~24907; hook on 0x80133434) | With `ED_COOP_P2_STEP=1` only: runs player 2's step with who[3] = player 2 and **no room in** | **Player 1's room** (player 2 is then "the player": current slot) | Off by default [read: `off = getenv("ED_COOP_P2_STEP") == NULL`]. The user's builds and play.bat don't set it [read: grep finds it only in hle.c]. **A trap if ever turned on apart** |
| `coop_player_turn` (~20652) | who[3] = player 2 (r13-26524, -21492, -21496), then `coop_stay_room_in` (their room word and table +0/+4), then pad, threat look, `COOP_CONTROLLER` 0x80045BC4 (their state handlers, which reach 0x801240B8), camera step; room out, then who back | Their own room, **when `coop_stay_apart` is true** | Safe apart. **Hole H2**: when the rooms differ but `coop_stay_apart` is false, who is still player 2's and table +0 is player 1's (§3) |
| `coop_events` (~21826) | Player 2's events outside their turn (event 157 "move to", 165, hurt reactions...). `room_in` when `r6_apart && !coop_camera_at_room_change` | Who is not swapped (`r13-26524` = player 1's), so player 2 is found by +592 in any case | Safe. Inside a room change it skips room_in, but by +592 player 2 still gets their own slot |
| `coop_as_player2(COOP_EVENTS)` for event 58 (~21900) | who = player 2, **no room in**. The handler for 58 (0x80059BF4) sends 39, whose handler (0x8005A5D4 → 0x80061B9C) is the sanity hit | Current = player 1's (outside r6c) | No mover reached [read: neither 58 nor 39 calls 0x801240B8]. Safe [inferred] |
| `coop_seen_hit` (~22234) | who = player 2 for `COOP_SEND_EVENT` (event 39) | Event to player 2's node goes through `coop_events` (room in apart) | Safe |
| `coop_hazard_touch` (~21213) | who = player 2 for `0x80144F38`, no room in | Current = player 1's | 0x80144F38 is not among 0x801240B8's callers. Its events to player 2 go through `coop_events`. Safe [inferred] |
| `coop_door_motion` → `coop_as_player2(0x801669AC)` (~21614) | Event 253 to player 2 | Through `coop_events`: room in apart | Safe |
| `coop_sighting` → `coop_as_player2(0x8003B1C8)` | An enemy's look with player 2 as the player | — | Reads only (and `coop_in_view`'s line test). Safe |
| `coop_quick_cast` (~12413), `coop_cast` (~12498), `coop_spells_update` (~12756), `tome_copy_left`'s cast (~26675), `coop_p2_load`, `coop_prompt_choose`, `coop_enchanted_look`, `coop_move_sound`, `coop_p2_own_cancel` | `coop_be_player2` (5 who words and the casting words), **no room in** | Current = player 1's | No mover in these calls found. A spell's own update (`spell+28`) wasn't read; Pius's chapter has no spells by then. Low |
| `coop_use_start` / `continue_coop_use` (~18010) | who = player 2 and `coop_stay_room_in` for the script, which can send 157 | Their room | Safe (the script runs to its end in the call) |
| Player 2's script timers (~18609), words close (~15961), the Tome item's script (`tome_copy_left`, ~26692) | `coop_be_player2` and `coop_stay_room_in` | Their room | Safe |
| `r6c_creature` (~24711) | A creature of the other resident room: `coop_room_in(room)` when not current, then who = that room's player (player 2 for theirs) | Their room | Safe. Events to type 2 nodes (the players) are left to `coop_events` [read: `type == 2u` returns] |
| `r6_hit_room` (~24500: hit 0x8006255C, damage 0x80061B9C, hurt 0x800637B8) | Room swapped alone (who stays) | Player 2 by +592; player 1 as "the player" while the other room is current | Safe for player 2. For player 1 as "the player" inside it, the game's own gates refuse a hit across rooms [read comment] |
| `r6b_job_open` / `r6b_close` (~24582) | The route job's room in, across the background thread, with MSR[EE] off | — | Moves nobody. If it ever leaked to the main thread's frame, **player 1** (the player) would be stepped on player 2's room (§4 H5) |
| `notify_ticket_run` / `coop_stay_change_end` (~13885 / ~13909): R4, player 2's change | `coop_be_player2` from the runner 0x80152614's entry; room word = player 2's old room; **slot table untouched (+0 = player 1's room)** until the engine's own swap; ends at the behaviour pass's entry (lr 0x80008400 / 0x80050A9C) | Before the engine's swap: player 1's room for player 2 (they are "the player"). After it: the new room | Safe in the normal frame order [read: the runner runs at 0x800083EC, the pass at 0x800083FC, lr 0x80008400]. **Hole H1b** if the bracket outlives that pass (§3) |
| The strips (`coop_stay_kind_ask` at lr 0x80043B00, `coop_stay_strip_walk` 0x80043820) via `coop_stay_spared` | Spare the stayer's body (either player) | — | Spared in the scripted 182→181 run [read: logs/run-20261112-033738-p2-follows-181 page-0001.log:210, 217] |
| `notify_room_slots_tail` (lr 0x80152434) | Puts the stayer's record back right after the tail, and lets the ticket's hold go (0x801F6828) | — | Record back in the same call [read: same log 220-224] |
| `room_slots_return` | Swaps the indices so the engine unpacks into player 1's old slot | — | Doesn't move a body. A wrong decision would evict the stayer's room: then `0x80123F70` gives 0 (stays put) and `coop_follow` brings them (§3 H3) |
| `coop_follow` (~21315), same room | `ED_COOP_P2_PUT` (testing: writes the place, no floor test). Fallen (z < stood_z - 2000) or stood inside player 1 for 60 reads: `coop_place(o1,o2)` | `camera_collide` with `r13-26524` (player 1's) = current = the shared room | Right room. **Only in the same room** (§5) |
| `coop_follow`, apart (`coop_stay_apart`) | Nothing: `stood_z = NAN`, return | — | Safe, but **no fall rescue apart** (§5) |
| `coop_follow`, the bring (rooms differ, not apart) | `coop_place_facing(o1, …)`, writes o2's place, `0x801F64C8(p2, room)` (sets +592), facing | Player 1's room's floor beside player 1 | Right room: they are moved into it |
| `coop_adopt`, `coop_park`, `coop_spawn` | Place beside player 1, set room = the room word | Player 1's room | Right room (adopt and park are at load or co-op on/off, together) |
| `sweep_put` (ED_PUT, ~14975) | `camera_collide_object(cpu, object, …)` for the put player's own object | The object's own room by +592, or the current slot when the object is the player's | Fixed today. Testing only |
| `coop_position_copy` (0x80118274) | Copies object +0..11 out. With lr 0x8011CB74 (the detail share) only, a place in front of the camera | — | Never writes a body. Off that lr the copy is the original's, r5/r0 included [read 0x80118274-0x8011828C] |
| `coop_cutscene_place` / `_held` / `_over` | Writes 60 bytes of a place back into `r13-26524`'s object, when it is `g_coop_cutscene.object` (player 1's, taken at a pad read with `!g_coop_tome_p2`) | — | Player 1 only, their own place. Not apart-related |
| `coop_trade_places` (ED_COOP_SWAP_AT) | Swaps the places | — | Testing only |
| Third-person and own camera (camera.c `line_reach` / `wall_reach` / player 2's eye at ~1764) via `camera_collide` | `camera_collide` takes `r13-26524`'s object, so the world is the current slot. Apart, outside player 2's turn (the camera is computed in the frame), that is **player 1's room** | Player 1's room | Moves only the eye, never a body. A wrong eye for player 2 apart (a known item) |
| `coop_place*` / `coop_floor` / `coop_in_view` | Use `camera_collide`, so the current player's room | — | Placements are only used where the target is player 1's room. The sight line is a test only |
| `camera_collide_object`'s test 0x80130684 | Writes only `out` (r31) [read 0x80130778-0x80130838]. Sub-calls 0x80138E8C and 0x80130A84 weren't read for side effects | — | Read-only at its own level |
| The Tome copy's turn (`tome_copy_step`, ~14749; `g_tome_copy_turn`) | Runs in a whole copy of memory (`tome_copy_swap`: `cpu->ram = copy`). Player 2 is the player inside it (`coop_tome_player(true)`), and the copy is thrown away | — | It can't move the real player 2. What comes back (equips, a script through `tome_copy_left`) runs with room in. It **does not run player 2's step in the real game** |
| `coop_tome_player(true)` held across frames (owner 1: `coop_menu_input` with a quick spell or Start, modes 0/1; tw's Tome frame at ~15145) | `r13-26524` = player 2 for **every frame** until `coop_tome_player(false)` | If a world frame's behaviour pass runs inside it: **player 1's room for player 2** | Mode 2 is the default (`tome_copy_mode` default 2; Start and the quick spell's Tome go to the copy [read ~26461, ~12454]). tw's world frames put player 1 back first (~15148). **Hole H1a** for modes 0/1 (§3) |
| Sanity effects (the game's) | Effect 35 Sinking Feeling moves its target down with 0x801240B8 mode 3 (0x8009E4C8-0x8009E520), its target by id from the tracker +56 (`0x801F5F88`, 0x8009E454) [read]. The others listed above use the same mover | +592 for a non-player target, the current slot for "the player" | Not this bug's door: room 181 has no effect list, and 35 needs the next room to have one [read: logs/sanity-effects.log "room 181: room's own effect list: none"; docs/game/systems/sanity.md table "14, 16 … 35: the next room has a list"]. Not built for co-op at all (one tracker) |

## 3. The holes where player 2 is stepped against player 1's room

All of these need `r13-26524 == player 2's object` while table +0 is player 1's room, and a mover (the behaviour pass,
the controller's state handlers, or event 157) running inside that window.

**H1a: `coop_tome_player(true)` held across a world frame (the old Tome routes).** [read] `coop_tome_player`
(~25785) swaps `r13-26524` and `r13-21496` to player 2 and sets `g_coop_tome_p2`, and they stay so until it is called
with false. In `coop_menu_input` the quick-spell path (`g_coop_quick_tome` → owner 1 → `coop_tome_player(true)`) and the
Start path (`start_wait`, owner 1) set it at a pad read. The game then opens the Tome on a later frame, so at least the
world frame after that read runs with player 2 as the player. The behaviour pass in those frames steps player 2's body by
the current slot, which is player 1's room. [read] In mode 2 both go to the copy instead: `g_tome_copy_ask` at ~26461, and
the cast's `g_tome_copy_ask` at ~12454. So this applies only with `ED_COOP_TOME_COPY=0/1`. tw's frames (~15145/15148) put
player 1 back before each world frame.

**H1b: the R4 bracket outliving its behaviour pass.** [read] `notify_ticket_run` opens it at the runner 0x80152614's entry
for a change of player 2's (`g_coop_room_p2`), and only `coop_stay_change_end` at `0x8014FDB8` with lr 0x80008400 or
0x80050A9C closes it. The runner is reached from 0x801558F4 (inside the ticket tick 0x80155830, which is called at
0x800083EC and at 0x80050A90, each followed by a closing pass at 0x800083FC/0x80050A98) and from 0x800AE000 (a load path
that sets `r13-23276` first). [read] The other passes (0x800084AC, 0x8002016C/74, 0x80050AA4, 0x801114C4/CC) don't close
it. While it is open, `coop_stay_apart` and `r6_apart` are false (`g_stay_bracket != 0`). Until the engine's own swap,
table +0 is player 1's room with player 2 as the player. [inferred] In the default frame order it closes in the same
frame, before any body is stepped. A change of player 2's that the runner refuses or leaves early (no swap), followed by
the pass at 0x800083FC, still closes it, and in that frame nobody had been stepped while it was open. This is a hole only
if the runner is ever entered on the 0x800AE000 path in co-op, or from the second loop without its pass.

**H2: player 2's turn with the rooms different but `coop_stay_apart` false.** [read] `coop_player_turn` swaps who[3] to
player 2 whatever `stay_in` returns, then runs `COOP_CONTROLLER`. `coop_stay_apart` is false when any of these holds:
- `g_stay_bracket != 0`;
- player 2's +592 equals the room word;
- `room_slot_of(r2) < 0`, meaning the host's `g_room_slot_room[]` doesn't hold their room. It is set to -1 by
  `notify_room_unpack_post` for a slot being unpacked into, and relearned only at `room_slots_frame` (the room draw) or
  the tail notify.

In those frames the controller's state handlers (and event 157 from a script of theirs) collide player 2 against table +0,
which is player 1's room. [inferred] The pad read's `coop_follow` runs first in the frame and would bring player 2 in the
`room_slot_of < 0` case, so the turn after it is in player 1's room legitimately. The window is a frame where the rooms
differ and `coop_follow` didn't bring them (the bracket case, or a frame between an unpack and the next room draw, if the
turn ever runs there).

**H3: player 2's +592 wrong.** [read] The host sets +592 only through `0x801F64C8` (bring, adopt, spawn), and the engine's
stamp 0x80154D30 stamps `r13-26524`'s object (stay-readers.md). [inferred] If a stamp, or a host bring, ever names player 2
with player 1's room while their body stays in 182, every later step grounds them on 181's floor at 182's coordinates. Then
`coop_stay_apart` is false (same room by +592) and `coop_follow`'s same-room branch would see them fall 2000 below
`stood_z` and put them beside player 1. That gives a fall followed by a teleport, which doesn't match "could not move after".

**H4: `ED_COOP_P2_STEP`.** [read] It is off in all play. Turned on apart, it would step player 2 against player 1's room
every frame (who[3] swapped, no `coop_stay_room_in` in `coop_character_step`).

**H5 (player 1's side): the route job's bracket.** [read] `r6b_job_open` puts player 2's room current from the
background thread, with EE off, and closes it at the job's OSDisableInterrupts. `r6b_frame` warns at the room draw if it is
still open. [inferred] If the main thread ever ran a frame inside it, the stayer (player 1, "the player") would be stepped
on player 2's room. Player 2 would not. It is safe unless the job blocks before 0x8020102C, and that wasn't checked.

## 4. The stayer, both ways

- [read] After a change of player 1's, table +0 is the mover's room, and `notify_room_slots_tail` puts the stayer's record
  back. Player 2 is stepped by +592 in the pass, and by `coop_stay_room_in` in their turn.
- [read] After a change of player 2's (R4), `coop_stay_change_end` puts the room word and table +0 back to player 1's room.
  Player 1 stays "the player" in the room in play, so the current slot is their own. The strips spare player 1 by
  `coop_stay_stayer` (`g_stay_bracket_was[1]`).
- [read] The same holes apply mirrored, with one exception: `coop_follow` writes only player 2 and has no rescue for player 1.

## 5. A fall apart is never undone

- [read] `coop_follow`'s rescue ("fell out of the room", z below `stood_z - COOP_FALLEN` (2000), then `coop_place` beside
  player 1) runs only when o2+592 equals o1+592. While apart it sets `stood_z = NAN` and returns. When the rooms meet again,
  `stood_z` is taken afresh from the current z (`!isfinite(stood_z)` → `stood_z = z2`), so a body that fell while apart is
  never seen as fallen.
- [read] Player 1 has no rescue at all.
- [inferred] That fits "could not move after": the fall itself needs one bad grounding, and nothing on the host side ever
  brings them back up.
- [read] If player 2's room's record is dead or absent, `0x80123F70` returns 0 and the step keeps them where they were (no
  movement and no fall). A body "stuck" with a NULL world is a second way to "could not move". `coop_follow` then brings
  them (when `room_slot_of < 0`), unless `room_slot_of` still remembers the room.

## 6. What the records say about this session

- [read] `logs/play.log` (Nov 12 03:19) and `logs/sanity-effects.log` (lines 753-765) contain a play through Pius's chapter
  with 182 first entered at frame 14228, 181 at 14989 and 184 at 16286. Its 6th line says
  `[rooms] separate rooms off for this run: co-op is off`, and only its last line says separate rooms are on. **It is not
  the bug's session** (the user had separate rooms on), and it gives no fact about the fall. Rooms 181 and 182 have no
  sanity-effect list, and no effect started in it.
- [read] In the scripted 182→181 run (logs/run-20261112-033738-p2-follows-181, page-0001.log 205-265):
  - frame 4764: slot 1 holds 182 and is current; the strip passes over player 2.
  - 181 is unpacked into slot 0 ("unpack asked: room 181 into slot 0").
  - frame 4766: the walk passes over player 2, and slot 1's record is put back.
  - From frame 4767 player 2 stays in 182 (slot 1) at z 78 while the room in play is 181 (slot 0).

  This is the steady state this reading assumes. It held there.
- (D), outside this reading but the strongest candidate not yet ruled out by the host paths: the double-copy and put-back
  path. When player 2's door brings them into player 1's room, or a return puts a record back, the stayer's slot can be a
  record the engine marked dead and the host put back. This morning's hang (saves/reports/hang-20261112-025508, a put-back
  record over overwritten data whose collision tree pointed at cells -4633 and -7699, quoted in `room_slots_return`) shows
  a put-back slot's collision can be garbage. [inferred] A body standing on such a tree can drop through it. Worth checking
  for 182 on the user's route: which slot held 182, and whether that record had been put back after being marked dead.

## 7. Probes that would tell the cases apart (for the next session; nothing was built)

1. At the behaviour pass's entry (0x8014FDB8, lr 0x80008400) while the players' +592 differ, log once per frame:
   - `r13-26524` against both objects;
   - the room word and table +0/+4;
   - `g_stay_bracket`;
   - `room_slot_of` of player 2's room;
   - whether player 2's slot record is loaded.

   Any frame with `r13-26524` == player 2's object, or player 2's room not current inside their own turn, is (A).
2. Per frame apart, `0x80123F70(o2)`'s answer against `room_slot_record(room_slot_of(o2+592))`. A mismatch or a 0 is (B), or
   a dead record.
3. A fall detector for either player apart: z below the last still z by 500, logged with 1. and 2. and with the
   slot records' +0x8140..+0x8144. It wouldn't rescue anyone, but it would catch the frame.
4. With `ED_COOP_LOG`, the existing `[stay] frame N: player 2 stays in room … at x y z` line (every 120 frames from
   `room_slots_frame`) already gives player 2's z apart in a user's play log. Have it on in the user's next separate-rooms play.
