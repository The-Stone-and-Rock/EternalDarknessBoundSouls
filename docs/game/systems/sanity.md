# Sanity effects

`ed system sanity`. Every effect the game has, how it starts and what limits it, for the Sanity effects option (`mod_insanity`).

**How sure:** the numbers, flags, rooms and limits are read from the code and saves, and only what "Seen in play" says was run. "What you see" comes from [StrategyWiki](https://strategywiki.org/wiki/Eternal_Darkness:_Sanity%27s_Requiem/Sanity_effects) and the [Fandom wiki](https://eternaldarkness.fandom.com/wiki/Sanity_Effects), matched to the game's names by hand. A `?` means that match still has to be seen in play. The names, and `ED_INSANITY_FORCE=N` to force one, are on `main` (`runtime/host/hle.c`, `runtime/host/insanity.cpp`). This branch doesn't have them yet.

## Trying them: Mods, Sanity Effects

In a game's menu, single player: pick an effect, choose whether to keep the game's limits, and Play this effect. Every effect, the game's own rolls too, goes to `logs/sanity-effects.log`: who asked and from where, sanity, place, doors, sounds and streams, room and mode changes, how long it ran, and each room's own list as it's first entered. `ED_SANITY_TEST=effect,steps,ignore` does the same with no menu, and `ED_SETTINGS=FILE` runs with co-op off. Code: `sanity_ask`, `sanity_door_pick`, `sanity_door_rules`, `sanity_door_run` in `hle.c`.

**Each effect goes by the trigger it has in the game.** All are asked for with event 83, with sanity counted as lowest for the one asked for. Then:

| Effects | What starts it |
|---|---|
| Bit 2 clear in `+24` (1, 4, 5, 6, 7, 8, 24, 27, 31, 38 among them) | Itself, at once |
| 25, 26, 28 | A press of Start: the pause code sends event 84 and asks for game mode 8, 7 or 10 (`0x80006DA8`) |
| 20 | The Tome opening (`0x80106F4C`) |
| 15, 34, 36 | Other code. The tool keeps your place (`0x8006BE04`) and sends event 84 |
| The rest | A door. The tool runs a door script out of the room, from `runtime/host/sanity_doors.inc` |

**At a door the game does it all:** `0x8004305C` before the load reads the effect waiting and keeps your place, and `0x80043380` after the load decides, sets up the room (`0x8006C008`) and sends event 84. Only a door into a room that suits the effect starts it (the table below), so the tool picks one: suits and tests no flag, then suits, then no flag. If no door suits, Keep leaves it waiting and says so, and Ignore starts it at any door.

`tools/ed/facts/sanity_doors.py` writes the door list: 735 doors, and the 146 of 297 rooms with a list. A door is from the kit's table, so a locked one or one that depends on where you stand may not go.

**What failed:**

- Starting one with event 84 and not calling `0x8006BE04` first. At its end `0x8006BFAC` reads the room to come back to from the tracker's data `+72`, which was 0, so it asked for room 0 and the game hung on a white picture (Head of Shakespeare, `saves/reports/hang-20261104-231749.txt`).
- Starting door effects with event 84 from where you stand, even with those two calls. It skips the door. Walk on Ceiling ended after 2 frames, Phantom creatures after 1.
- Waiting at a door into room 138 from room 166. It has no list, so the door passed the effect by. Room 216, the other door, has list 18, 11.

**Confirmed by the user, slot 2, room 166 through the door to 216, Keep, 2026-11-04:** Bug crawl, Zombie, Phantom creatures, Head of Shakespeare, Controller not found, Shrink player, Sinking feeling, Ammo pickup, Illusions of grandeur. Logged lengths: Head 1030 frames, Illusions 1130, Controller 595, Sinking 552, Ammo 528. Each that returns plays sound 536, then a voice line (stream 281 or 282), and comes back to room 166 at the door's side (1475, 6). Zombie and Shrink player were still running when the run closed at frame 3860. Bug crawl ran 14 frames and Phantom creatures 3 by the log. Not yet run this way: every other effect, and the menu page itself.

## How the game runs them

- **The tracker:** one thing of kind 32 in the world (`0x801F8FD8(first, 32)`). Its `+56` is the character it watches.
- **Sanity level:** `0x80069018` sets `+21` from sanity over most: below 75% is 1, below 50% is 2, below 25% is 3. `ed62` reads it. Level 0 has no rolls.
- **The clock:** `0x8006D2FC` checks a timer against 3600 ticks, refreshes which effects are allowed (`0x80069900`) and sends event 80. How long a tick is hasn't been measured.
- **The roll:** `0x800697C0(count, level)` picks at random among effects that are ready (stage 3), not bit 8 of `+24`, and at or under the level.
- **What's allowed:** `0x8006EB6C(effect, level, character)`. All the chapter limits below are here.
- **The table:** 41 records of 36 bytes at `r13-25476`, the same for the whole game. `+0` number, `+4` lowest level, `+10` stage, `+24` flags. Start functions are at `0x80243D8C`.
- **Why they feel one-time:** starting an effect sets its bit at `+12` or `+16` of the tracker's data (`0x80069A54`), and it can't come again until `0x80069C30` clears the bits. So it works like a deck of cards, not like a once-only event.
- **Blocks on everything:** chapters 13, 15, 16; chapter 9 with flag 518 set; rooms 5, 39, 83, 157, 185, 191, 213, 292 (`0x800689D0(room, 5)`, table `0x80243D20`).
- **Coming back:** `0x8006BE04(player, tracker thing)` keeps the room, place, angle and stats at the tracker's data `+72`, and `0x8006BFAC` sends you back with transition 17. This is the "it wasn't real" return.
- **Each room has its own list of effects:** `+128` count and `+132` list in the room file (`0x80153A00`, `0x80153A24`). The log prints it. Room 150 has 11, 23, 9. Room 100 has 29, 16, 19, -1, 7. Room 216 has 18, 11. A count of 0 means no list.
- **At a door:** `0x80043380(effect, next room, marker, camera)`, called from `0x80152748`, decides if the effect waiting starts there, then sends event 84:

| Effects | Starts at a door when |
|---|---|
| 10, 18 | The next room's list names it |
| 13 | The next room is 27, 69, 123, 245, 255 (`0x800689D0(room, 2)`) |
| 22 | The next room is 30, 105, 151, 161, 167, 243, 248, 253, 255, 257 (`0x800689D0(room, 1)`) |
| 14, 16, 19, 24, 27, 29, 30, 31, 32, 35 | The next room has a list |
| 21, 33 | The next room has a list, and isn't on their own room list (kinds 7, 8) |
| 11, 17, 23 | Rules at `0x8004352C` and `0x80043558`, not read. 17 and 23 start at the door into 216 |
| 12 | Its turn, from flag 959 |
| 15, 20, 25, 26, 28, 34, 36 | Never at a door: other code starts them |

- **Scripts:** 287 checks of `ed61` only compare it with 0, to skip work while an effect runs. Only three effects are tested by number: 12 (script 1600), 13 (`ed60` in 39 door scripts), 30 (`L00/1000`).

## The effects

"Lvl" is the lowest sanity level it can come at. Suggested groups are for the user to decide:

- **Story:** keep out of the random pool, as the game has it.
- **Repeat:** let it come more often.
- **Repeat, co-op off:** repeat in single player, and off in co-op for now.
- **Look first:** see it forced before deciding.

| # | Game's name | Lvl | What you see | Limit in the code | Suggested |
|---|---|---|---|---|---|
| 1 | Bleeding Walls | 1 | Blood runs down walls | Tests flag 749 | Repeat |
| 4 | TV Shut Off | 2 | Screen goes black, as if the TV went off | | Repeat |
| 5 | Blood Pool Ceiling Drip | 2 | Blood drips from the ceiling | Tests flag 749 | Repeat |
| 6 | Target Marker | 2 | ? Attacks or fires at nothing | | Look first |
| 7 | Attack Camera | 2 | Turns and shoots the screen, leaving a bullet hole | Needs a certain weapon class in hand (`0x8006E998`, mask `0x90038`) | Repeat |
| 8 | Horror Cam Shake | 2 | Camera shakes; ? a Horror's steps and growl | | Repeat |
| 9 | Ammo Pickup | 2 | Ammo lying about, gone when it ends | Off in chapters 1, 3, 4 | Repeat |
| 10 | Locked in Room | 2 | Every door locked for a while | | Repeat |
| 11 | Blood Bath | 2 | ? Not the same as Alex's bathtub scene below | | Look first |
| 12 | Edwards Ghost Cine | 2 | Alex and Edward's ghost, in order | Alex's chapter, flag 959 | **Story** |
| 13 | Timeline Visitation | 2 | Another era of the same place, in another colour | See below | Look first |
| 14 | Bug Crawl | 2 | Bugs crawl over the screen | | Repeat |
| 15 | To be Continued | 2 | Fake ending and sequel screen | Sets flag 837 | Repeat, co-op off |
| 16 | Enemy One Hit Kill | 3 | ? Being seen or hit kills at once (fake) | | Look first |
| 17 | Head of Shakespeare | 3 | Head falls off, you carry it, a voice recites Hamlet | | Repeat |
| 18 | Helpless Battle | 3 | ? Shares its steps with 16 | | Look first |
| 19 | Illusions of Grandeur | 3 | ? Enemies too large or small | | Look first |
| 20 | Empty Inventory | 3 | Tome shows nothing carried | | Repeat |
| 21 | Phantom Creatures | 2 | Fake zombies, gone after one hit | | Repeat |
| 22 | Walk On Ceiling | 3 | Room upside down, walking on the ceiling | | Repeat |
| 23 | Zombie | 3 | You turn into a zombie, then "die" | | Repeat |
| 24 | TV Mute | 3 | Sound mutes, with a TV's mute sign | Off while `0x800ACD44` is nonzero | Repeat |
| 25 | TV Video | 3 | Black "VIDEO" screen while you're eaten | | Repeat, co-op off |
| 26 | Game Reset | 3 | Console restart screen, Edward reading The Raven | | Repeat, co-op off |
| 27 | Save/Erase Game | 3 | Fake delete of all saves | Not before chapter 5, flag 475 set when it starts | Repeat, **co-op off (user's decision)** |
| 28 | Blue Screen Of Death | 3 | Fake system crash | | Repeat, co-op off |
| 29 | Controller Not found | 3 | Fake pad message while zombies attack | | Repeat |
| 30 | Alex Phone Call | 3 | Phone rings, "Remember me, Alex?" | Script `L00/1801`, room 69, flags 116 and 885; `SanityMkr99` is only in rooms 69, 245 | **Story** |
| 31 | TV Volume | 3 | Fake volume bar slides down | Off while `0x800ACD44` is nonzero | Repeat |
| 32 | Shrink Player | 2 | You shrink as you walk | | Repeat |
| 33 | Expand Player | 2 | You grow as you walk | | Repeat |
| 34 | Max Shoot Self | 2 | Reloading, the gun goes off into you | Extra setup in chapter 5, flag 1126 | Look first |
| 35 | Sinking Feeling | 2 | You sink through the floor | | Repeat |
| 36 | Spell Cast Bust | 2 | Casting makes your body explode | Extra setup from chapter 4, flag 938 | Look first |
| 38 | Footstep Trail | 2 | ? Footsteps that echo or follow | | Look first |

Records 0 and 2 have start functions but no names, and sit at stage 6 in slot 1. Records 3, 37, 39 and 40 have no start function. **The body breaking apart** (head, arms, torso, parts you can pick up) has no name of its own in the table. It may be 17 or another's variant: see it forced.

## Timeline Visitation, the other-era room

Effect 13's limit is flag 1123 in Alex's chapter and flag 1124 in the others, set as it ends (`0x8009B3F4`), so it comes once per flag. It's allowed only in chapters 0, 5, 7, 9, 10. It sets `+32` bit 4, and while that's on, the 39 door scripts in the table go elsewhere (`ed60`). The same door's normal way is kept.

| Chapter | Who | Door scripts | Goes to | Script and word pack loaded |
|---|---|---|---|---|
| 0 | Alex | `L00` 824 to 828, 1000 | 245 | 19, which is `ScrLvl16` |
| 5 | Maximillian | `L05` 278, 282 to 284, 343 | 69 | 0 |
| 7 | Paul | `L07` 1141 to 1146, 1242, 1243 | 27 | 10 |
| 9 | Edward | `L09` 902, 995 to 999 (and the `L16` copies) | 255 | 5 |
| 10 | Peter | `L10` 1238 to 1243, 2646, 2674 | 123 | 7 |

- **Loading:** on arriving, `0x8006C708` (from `0x8004371C`) loads the pack by the table `0x802441F4`. It loads scripts and words only, not people, enemies or models. It saves two storm flags (`r13-24924`) and sets `0x4000` on the data, which gates examine. `0x8006CDF8` loads the chapter's own again.
- **How long:** 700 ticks in chapters 7 and 10, 300 in the others (`0x8009B168`), then a few more steps before it ends.
- **The list of these rooms:** `0x800689D0(room, 2)` holds exactly 27, 69, 123, 245, 255 (table `0x80243D14`), read by the door (`0x80043488`).
- **Same building, other era:** the wiki says Alex's mansion in Edward's time, Peter's hospital as Paul's cathedral. Rooms in these pairs share marker names, so a door lands in the matching place.
- **Not found yet:** what turns the picture sepia or black and white.

## Scenes tied to a place or the story: leave these alone

These are chapter scripts or depend on story flags, not random rolls.

| Script | Where | What | Why it stays |
|---|---|---|---|
| Effect 12, armed by `0x80068868` | Rooms 186, 305, 196, 225, 255 in other chapters clear flag 959, one after another | Next Edward scene for Alex, from `Levels/Level17` cin0074 to cin0077 | The story's order |
| `L00/1801` | Room 69 | The phone call | Story flags |
| `L00/2723`, in 1816 | Room 81, Alex only (model 9), tests the sanity level | Bathtub: `DeadCam`, Alex dead in the bath | One place, one character |
| `L00/1870`, in 1816 | Room 87, Alex only, sanity level 1 or worse, flag 687 | `BodyCutCam` scene; ? the hanging man | One place, one character |
| `L05/838`, `1044`, `2804` | Room 259, Maximillian, flag 705 | Ladder goes to 235 once instead of 293: the asylum | Story, a different room |
| `L07/1111` | Room 115 | A door's way changes at level 3 | Not yet read in full |
| `L09/2775` | Including rooms 241, 242, 244 to 247 | Edward's scripted scares by room | Not yet read in full |

Worm room's sepia in Ehn'gha and Max's third rune sepia, from the wiki: scripts not found yet.

**Paintings and posters**, level 3 only, each a texture of that one room (`fn41`, `fn42`, `fn189`, hashes), so they can't move: `L00/1546` room 68, `1149`; `L05/1539`; `L07/2340`, `2341` room 105, `2291`; `L09/1268`, `1269`, `1539`, `1543`, `1544` room 243, `2262`, `2264`, `2289`, `2290` room 245; `L10/2343`, `2344` room 17.

**`SanityMkr` isn't a sign of a sanity effect.** 220 of 297 rooms have one, and 119 scripts use them, mostly as plain places for puzzles and sounds. `BloodMkr` is in 123 rooms, one script names it (`L00/2569`), and no string in the code names it, so how blood finds it isn't traced.

## Sounds

- **By place:** `fn126` in regions named `SanitySndRgn` (39 regions, 38 markers), each once by flag: `L00/2519` (including rooms 67 to 70, 77, 78, 80, 89), `L05/2721` (252, 253, 254), `L07/2359` (107, 109, 115, 116, 117), `L10/2735` (4, 11, 16, 19). Sanity level 1 or worse.
- **Files:** `audio/music/insangk.adp`, `inscath.adp`, `ins_mansion.adp`. `Levels/Level17/ins1.aud` to `ins4.aud`.
- **Voice:** `ed64(62, 0, 0, 100)` at level 3, and the character line after an effect: mode 12 "This isn't happening!", asked for while `r13-24140` is set.
- **Screens:** `UInsane.cmp` and its language copies, game modes 7, 8, 10 to 13 (`0x80112D98`).

## Enhanced, single player first

Only while Enhanced is chosen, so Normal is untouched. Each is one place to change:

1. **Twice as often:** 1800 ticks at the clock, in place of 3600.
2. **Come back sooner:** clear an effect's bit in the deck when it ends.
3. **Lift chapter limits one at a time:** `0x8006EB6C`.
4. **Visits to other rooms:** `fn66` and `fn71`, hooked already as `COOP_SCRIPT_GO_TO_ROOM`. Keep to a tested list of rooms. Each room's people and scripts can reach things that aren't loaded, and flags are shared with the story.

## Co-op, after single player

One tracker, one table, one clock, so only one character's sanity rolls effects. `0x80069018` reads `r13-26524` and the current player (`ed needs 0x80069018`). A second clock and table per player would go through `coop_be_player2`. Only one room is loaded, so a visit takes both players (`coop_follow`). Screens that are game modes stop both, and sound shares one set of speakers. **Save/Erase Game stays off in co-op**, by the user's decision: saving in co-op is already fragile.
