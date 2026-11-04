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

**Open:** who writes a wrong record (`0x80602C90` was never found), and player 1's camera stuck in `logs/run-20261102-223052-test2`.
