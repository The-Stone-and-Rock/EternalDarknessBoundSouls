# Chapter tour: what's logged for the user to review

From `ed chapters 1` to `12` on 2026-11-04, on saves 1 to 12 at each chapter's start. Rooms are reached by running the game's own door scripts from the host (`ED_RUN_SCRIPT`), so **a door here is a teleport**: flags and items aren't as play would have them. Sheets of every room, and each chapter's report, are in `Screenshots for Claude/chapters/final`.

The runs here were scripted. The user has since looked at the result and said it looks right, and their eyes outrank all of it.

**Build:** the tour of all 12 was on a build whose shadows faded too soon. On the final one, chapters 1, 4, 6 and 12 were run again (`chapters/recheck`), with the same doors, the same freeze in Karim's chapter and no new kind of finding. Chapters 2, 3, 5, 7, 8, 9, 10 and 11 weren't run on it.

**`ed test`:** 14 of 20, before the last shadow change, and `shadow-steady`, `shadow-karim`, `lever-tomes` and `p2cam-door` passed again on the final build. The 6 that fail (`both-walk`, `costume-door`, `door-climb`, `door-open`, `urn-p1`, `urn-p2`) use saves/slot1 to 10, which are now the chapters' starts, so they play from the wrong place. They say nothing either way about those features until each has a save of its own.

**`ed graphics`, on slot 1:** every option changed the picture, with no errors, except contact shadows and spell glow, which can't be told apart in that room (a spell has to be cast for spell glow). HDR and frame rate don't show in pictures. Haze against 100%: 50% changes it by 5.3, 25% by 7.6, off by 9.8. It ran before the last shadow change. The one large `shadow_hard` figure wasn't real: the two pictures look the same.

## Freezes: 3, each with a saved state

All three are in the `saves/reports` folder, and all show the known `0x80325188` thread in the thread list. The user thinks a door leading to another chapter is the cause, which fits the first: see Connecting the chapters in `docs/game/keep.md`.

| Chapter | Where | What it was doing | Report |
|---|---|---|---|
| Karim (slot 4) | Room 186, then on into room 71, a mansion room | Door script 167. 3 of 3 runs that walked from 185 into 193 first. A direct door to 186 from 338 wasn't tried | `hang-20261104-164118` |
| Paul (slot 7) | Room 123 | During a cutscene, script 66 running, frame 9691 | `hang-20261104-165853` |
| Edward (slot 10) | Room 238 | After its ghost cutscene, door script 333 to room 241 | `hang-20261104-172850` |

## Player 2 falling through the floor on arrival

Player 2 drops about 2,000 units under the room, then gets sent back (and in room 11, 12's chain, falls about 175 units a frame). Seen on arrival by door script, not while walking. Not repeated in a longer run of room 254. It may be from the teleport.

| Chapter | Rooms |
|---|---|
| Max | 254 |
| Paul | 105 (arrived 1,739 from the entry, at height -1,728) |
| Peter | 11, 12 |

## Player 2's half during a room's first scene

While player 1's half plays a room's opening scene, player 2's half stays on the old room's camera, or a blank one. It looks like black, or a stray view. It's right again when the scene ends. `docs/coop-todo.md` already has one of these, for the Tome room. In the player 2 camera column, `-` is a camera at nothing.

| Chapter | Rooms |
|---|---|
| Pius | 181 |
| Ellia | 159, 217, 234 |
| Anthony | 153 |
| Karim | 194 |
| Max | 292 |
| Paul | 104, 117, 120 |
| Roberto | 208, 213 |
| Edward | 279 |
| Michael | 41 |
| Alex | 92, 97 |

## A door's camera, against the one they arrive on

Both players on the same camera, but not the one the door script names. A region picks another as they arrive, so these are probably right. Worth a look only if a room looks wrong.

| Chapter | Room: door says, shows |
|---|---|
| Pius | 341: Camera03, player 1 on Camera01 and player 2 on Camera02 |
| Ellia | 159: Camera08, a free camera (a moving one) |
| Lindsey | 162: Camera02, Camera01 |
| Paul | 131: Camera02, Camera01 |
| Roberto | 5: Camera02, Camera01. 196: Camera03, Camera07. 203: Camera05, Camera04. 213: Camera01, Camera08 |
| Edward | 237: Camera10, Camera09 |
| Michael | 223: Camera02, Camera01 |
| Alex | 51: Camera03, Camera01 |

## Doors that didn't go

The script ran and the room didn't change. Most need something from play first (script 127 tests flag 19; 1413 is chapter 13's), or are the second half of a chain. So these rooms **weren't looked at**.

| Chapter | Rooms not reached (door script) |
|---|---|
| Pius | 64 (1413), 184 (127) |
| Ellia | 64 (1413), 89 (1602) |
| Anthony | 64 (1413), 147 (892), 152 (2300) |
| Karim | 339 (132), 340 (138). 186 (167) left at once |
| Max | 64 (1413), 69 (283), 235 (838), 258 (930), 259 (284) |
| Lindsey | 143 (335), 168 (1463), 172 (758), 233 (1029), 306 (1479), 307 (1483), 308 (1482), 311 (769). 167 (1027), 299 (630) and 305 (1466) left at once: 305 goes on into 71 |
| Paul | 19 (1242), 27 (1146), 115 (2138), 116 (669), 119 (1224), 123 (1243), 345 (1335) |
| Peter | 5 (1215), 17 (1244), 123 (1243), 170 (1241), 345 (1335), 346 (1220), 347 (1245) |
| Edward | 93 (2204), 239 (1254), 241 (333), 255 (902), 274 (2204), 346 (1220). 71 (889) left at once |
| Michael | 5 (358), 46 (1035), 47 (95), 48 (1033), 49 (48), 50 (355), 54 (358), 221 (466), 226 (274), 228 (363), 337 (429). 225 (426) left at once |
| Alex | 93 (2204), 274 (2204) |

Doors that open words wait for an answer, and stayed up over every room after them, so they're tried last: rooms 64 and 6 were left out for that.

## One-frame picture jumps

- **Room 52 (Lindsey), 59, 139:** the whole picture dips for one frame, in both halves, to about what it is with no haze, with the camera still. With haze off there were none, in one run. The user thinks it's a sanity effect, which may be: nothing here has separated the two. Cause not found. Not the projection, which is the same on those frames.
- **Rooms 271, 276 (Edward):** small steps with the shadow mask not moving at all, and still there with haze off. That's the game's flickering lights, not a fault.
- **Rooms 169, 211, 104, 129, 12:** seen once in the brief check, and not in the longer run.

## What the checks can't say

- Tomes: both opened, side by side, in the first room of all 12 chapters, and each was closed and both players walked after. A door, a scene or a pickup with a Tome open wasn't tried.
- Enemies' shadows: from the renderer's lines, others cast in most rooms with enemies, and they look right on the sheets. In the dark rooms a shadow can't be told by eye.
- Starting positions: both players within 500 units of the door's entry, except the falls above. This is by script, not a real door walked through.
- `ED_INPUT_SCRIPT` goes round `controls.cpp`, and LB and RB on the save state pages were tested with Q and E.
