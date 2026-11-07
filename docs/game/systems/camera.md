# The camera is in the game. Never invent an angle

**Every camera in the game is on the disc:** 2,177 in 297 rooms, with position, target, rotation and field of view, and **814 of them move** on keys. `ed room N` lists them, `ed camera NAME --room N` gives one's keys and the scripts naming it, and scripts name only these (2,774 of 2,774 literal `fn8` hashes). An eye on a moving camera is only at the listed place on its first frame. Checked: room 147's `Camera01` is at 470, -145, 359, FOV 65.3; the live view in `saves/slot1.state` at `0x803012E0` is eye 470, -145, 359, FOV 65.25.

| Step | Game function or data |
|---|---|
| A region's script chooses the camera | `fn8(camera hash, a, b, c, d)` `0x801669EC`. 528 script ids, in 1,276 level copies |
| Blend, near the `fn8` | `fn3(n)`, `fn2(n)`, a divisor not a time (`0x801EB974`). Each sets one eye-side and one target-side record |
| Regions | `CameraRgn` and others: `ed room N` shows which camera each sets |
| The records | Target `0x80602B18 + n*136`, eye `0x80603178 + n*136`, 12 of each. `+0` position, `+32` state, `+36` how it picks its place, `+52` FOV, `+68` script on arrival, `+104` who it follows (0 is `r13-26524`), `+116` the eye's target, `+120` place on its path, `+124` the room's entry |
| Which index | 2 for region cameras (every `fn8`, `fn10`), 8 seen in slots 9, 10, 10 for examine's close-up. Current is `r13-26864`. `fn174(handle)` names who it follows |
| The step that moves them | `0x801EB9A0` |
| The result | `0x803012E0`: eye, target, up, `+48` FOV |

**`fn8(hash, a, b, c, d)`:** `a` not 0, the target follows the player. `b`, how the eye picks its place: 0 nearest key, 2 the same place on the path as the target, 1 farthest (used by nothing). `c`, 0 cuts, not 0 blends. `d` restores defaults, and is 0 in every call. `fn10(hash, reverse, c)` plays a path in time, `fn11(script)` runs on arrival, `fn2`, `fn3` set blend divisors, `fn6`, `fn7` follow offsets. The stack is `0x801ECD68`, `0x801ECCEC`: examine pushes and pops.

Also: `ed camera NAME`, and `ed door ROOM` for a door's arrival camera. Evidence: `docs/game/research/arguments.md`.

**For two players:** each half has its own: `coop_camera_step`, `g_coop_cameras` (the game's lent to player 2 in their close-up), `coop_player1_camera`, `camera_split_update`. Zone scripts set the cameras, so whoever's zone look is running must have their own in: `coop_scan_player1`.

## How to look at a camera bug

1. `ed timeline RUN`: both cameras by the room's own names, as they change. `cam1`, `cam2` are the nearest camera to each half's eye, so a whole rail reads as one name. `free x,y,z` means the eye isn't near a camera of that room or its path's keys, which can be right for a follow, a look or a third-person mode.
2. `ed check RUN`: `CAMERA` is a jump to somewhere that isn't one of the room's cameras, and `BOUNCE` is going back and forth. **It can't see a cut to the wrong camera of the same room, or a jump along a rail:** `game-cam` in the timeline and `ed see` is the room camera on the game's current eye record, by hash, set by `fn8`, `fn10` or a room change. It's `index N` when the record has none, as at index 8. No run has shown index 10, examine's close-up, yet.
3. `ed where ROOM X Y Z`: which regions that place is inside. `ed room N`: what each runs. Where regions overlap and a script branches, which camera wins needs `ed script ID`.
4. `ed sheet RUN F1 F2...`: look at it.

**The camera's stack, and the Tome's camera (fixed 2026-11-05):** `0x801ECD68` pushes the record that's on (depth `r13-21700`, entries of 20 bytes at `0x806038E8`), `0x801ECCEC` pops it and puts its index back, and `0x801EEB58(n)` sets which is on (`r13-26864`). The Tome's start (`0x801110CC`) pushes and puts on 8, its own; its close (`0x80110E44`) pops. A script's push (`0x8001AB18`) does the same. So the world is never on 8 with nothing pushed. In co-op the world's turns behind a Tome (`tw_mode_change`) swapped the depth but not the index: the world went on with the Tome's 8, the close popped nothing, and player 1's view stood still until a room change, in every chapter. A save made then came back on an empty record 8, which eases to (200, 1100, 100) and shows nothing. The index now goes with the depth (`kTwSwitches`), and `camera_index_heal` puts the room's record (2) back for a world on 8 with nothing pushed, as old saves have. `ed check` says `CAMERA ... stayed on the Tome's (record 8)`. `ED_CAMSTACK_LOG=1` logs every push, pop and index.

**An examine's camera steps:** the close-up pushes and puts record 10 on (`0x80024E90`); its words' opening, through the words' hold (`0x80042A8C(1)`), clears the stack and puts the room's record 2 on (`0x801EC3A0`, `0x801EEB58(2)`), then its words step pushes record 10 again (`0x80025080`); RETURN pops once (`0x8002511C`) back to 2. In co-op the hold is passed over so the other player plays on, and its camera part is done by `coop_words_hold` for either player. Passed over whole, record 10 stayed on after RETURN.

**The stack holds 5 entries,** from `0x806038E8` to `0x8060394C`, where other data starts (`0x800EDB8C`, `0x801ED890`). Each entry is 20 bytes: the two current records' blends (+48 of `0x806037D8` and `0x80603860`), their record pointers (+112) and the index. `0x801EEB58` rewrites those pointers from the index. Other pops: the close-up cancel `0x80024B24` at `0x80024BCC` (look stage 2 or 3) and `0x801ECE3C` (pop then cut). The step `0x801EB9A0` reads neither depth nor index.

**In co-op each player's camera set is their records, the stack, its depth and the index** (`coop_camera_set_io`, `COOP_CAMERA_SET`, 2026-11-06), swapped wherever the records are. Player 2's first set pops player 1's pushes off with the game's own pop (`coop_camera_set_fresh`), and a scene of player 1's that gives player 2 its last camera gives it with depth 0. Before, one player's close-up pushed and popped on the other's depth and index (`ED_CAMSTACK_LOG`: logs/run-20261105-230130-jtg-cs-r1 line 654).

**Open:** who writes a wrong record (`0x80602C90` was never found). Player 1's camera stuck until a room change (`logs/run-20261102-223052-test2`, the user's of 2026-11-02) is the Tome's camera left on, above.

**A room's start camera (2026-11-06, round 3).** The game gives the player a camera at a room's start three ways:
- Inside the room change, `0x80152614`: `0x801EE998(hash, 2, 0)` at `0x80152790`, `0x801EE764` on eye record 2 (`0x80603288`) at `0x801527CC`, then `0x801ECCE0` and `0x801EEB58(2)`. `0x80152298` has its own set at `0x80152588`. The arrival script runs after it, at `0x801527F4`.
- The player's first zone look (`0x80134A44`) during the room's 10-tick fade-in. `0x801E7C88(type, ticks)` keeps the ticks at r13-21784. The camera zone they spawn in chooses then: in ch9 room 27, player 1 spawns in CameraRgn07, so script 1067 gives Camera07.
- A scene's ending fn8.

`0x801EE764` has four callers: `0x800263CC`, `0x80152588`, `0x801527CC`, and fn8's `0x801EE95C`. fn10 calls `0x801EE998` from `0x801F066C` and `0x801F07BC`.

In co-op, player 2's set was made once per room, at their first look, which can come before either of those choices (ch9: eye 0,0,0). Now player 2's set follows the room start's choices until a zone of their own chooses (`coop_camera_p2_in`, `coop_camera_chosen`, `coop_scan_room_start`; log lines "the room's start chose a camera").

## A scene's end camera (round 6, 2026-11-06)

A scene's end camera is often chosen after its fn22(0) (1538: Camera04 by fn8; 1603: Camera07), so nothing should copy the game's camera at fn22(0) itself. Co-op's `coop_scene_set` no longer hands player 2 the game's set there: player 2's own-zone camera is kept, and a bare or fresh set is taken afresh at their next turn.

## The finish close-up (round 7, 2026-11-06)

- States 30, 74 and 84 (`0x80051F0C`, `0x80052384`, `0x80054E70`) put it on at event 1 and pop it at event 2. The count r13-30420 (`0x8061DD4C`) starts at 1 and goes down with every finisher; at 0 the game tries a close-up, and the next count is rand()%3 + 4. The flag r13-25580 is -1 while it is on, and the rotation r13-25584 picks Finishcam01, 02 or 03. It is skipped when the room is in the list at `0x80243850`, the victim is in `0x8030AB10`, or an engaged enemy is within 700.
- Finishcam01-03 are in the finisher's body model (object+60 -> +132 count, +136 entries of 120 bytes, +52 hash, +56 name), not in rooms. `0x80051D48` tests the line of sight and `0x80051E28` -> `0x801ECE90` puts it on. Forms are scene bodies with no cameras (Chandra's ghost and rotten Ellia have 0).
- Co-op: each player has their own count, flag, rotation and camera set (`coop_finish_camera`; player 2's words are in `g_fin_p2_words`). A form uses its wearer's own body's cameras for the test and the put only (`coop_finish_body`); this works in solo too. Player 2's events 1 and 2 arrive in `coop_events`: event 1 while the cameras are lent (FINISH HIM runs script 2244 as their use), event 2 after.
- Known limit: if player 2's finisher ends without its event 2, their flag stays -1 until the next play start (`0x80021648`), and they get no close-ups until then. The shared flag had the same problem before.
- Not tested: player 1's events while player 2 has the cameras lent; both players finishing close together; a finisher right before a room change; states 74 and 84; other chapters; player 2 cameras other than the game's own (`mod_p2_camera` 0 or 1).

## A camera's end script in co-op (2026-11-06)

`camera_end_script` (fn11) is fired by the camera step (0x801EBF4C, `0x801642D4(script, 0, 0)` at lr `0x801EC1C8`) as the
eye arrives. In co-op each player's camera follows a scene's path and fires it, player 2's in their turn with them the
player. A scene's release in that script (fn22(0), fn139(0)) takes "the player", so run there first it let go of player 2
and moved the script on: the new game's dream (L00/1911) left player 1 held for good. `coop_run_script` leaves such a
run to player 1's camera unless the scene is player 2's own (`coop_p2_scene_on`, their close-up, use or arrival).
`tools/ed/jtg.sh` has `JTG_NEWGAME=1` for a new game from the title (Start pressed to end each opening scene).
