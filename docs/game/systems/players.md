# How the game becomes two players

The game has one of each thing. To act for player 2, put their data in, call the game's own function, and put player 1's back. **Calling game functions is how this project works.** Only the recorder (`both_record`) must never call one, because a recording mustn't change the game.

**Before calling any game function for player 2:** `ed needs ADDR`. It walks the function and its callees, and lists the one-player data they reach, with the helper for each. The list is `tools/ed/player_data.tsv`; add to it when a mismatch is found.

| Data | Helper |
|---|---|
| The player, the controlled character, the casting words | `coop_be_player2` |
| Pads, control blocks, focus, interact | `coop_state` |
| Zones' inside words | `coop_zones_swap` |
| Script steps and timers, `0x805CAA40` | `coop_states_swap` |
| Cameras | `g_coop_cameras`, `coop_camera_step` |
| Prompts, `0x80335A38` | `coop_prompt_player1s`, `coop_scan_player1` |
| The other player's threat look | `g_coop_threat` |
| Running a script as them | `coop_use_start`, `coop_run_script`. A script run from their turn broke the game's threads, so it runs at player 1's controller |

**The game has two player pairs, not one. Move both, or functions disagree with no error.**

| Pair | Id, node | Set by | Read by |
|---|---|---|---|
| **Current** | `r13-21496`, `-21492` | `0x801F626C`: at spawn and 5 sites | Tome list, Equip, Mode, `ed140`, the find, `fn66`, door motion, arrival, `fn78`, `ed301`, prompt press |
| **Main** | `r13-21488`, `-21484` | `0x801F6260`: at spawn only | For example **`fn93`**, **`ed221`** and its carried test, **`ed52`**, examine's bit 4, the controller gate. The getters have 104 call sites, about 40 of them in magick |

Every save holds the same in both, so only the code tells them apart. `0x801F5F88` (handle to node) also shortcuts on current's id. `ed who [ADDR|NAME]` scans any function. `r13-26524` is the model the zone scan and planes use, a third thing.

**Which pair moves when:** `coop_be_player2` moves both. In player 2's look (the zone scan, `0x800C030C`), `coop_player_turn` moves current and the model, so main stays player 1 there. A press of theirs runs under `coop_be_player2`, except an exit prompt (1, 2, 3, 4, 10), which runs entirely as player 1. **`hle_guest_call` returns `r3` only and restores registers:** a function answering in `r4` can't be read through it.

**When something works for one player and not the other:** run the working function through `ed needs` and compare what their turn has with what this one lacks. See "The pattern behind many of these bugs" in `docs/coop-next-session.md`.

**Each turn:** `hle_coop_controller` is player 1's, and `coop_player1_turn` is theirs during player 2's close-up.

## Bodies

**A body is only the mesh and its bones.** Moves come from the character's file (`chars/charNNNN.cpk`), and name and stats from who they're made as. Each level's `U_LevelNN.bin` has a list of bodies by character number: `ED_BORROW_CHAPTER=N` with a character whose body isn't there prints the level's list, with sizes and bones. Level 1 has only 0, Pius. Level 13 has 9, 86, 96 to 98, 139 and 142 to 147.

**A player's body is read from the chapter's list (`r13-26452`) by their number as they're made.** So to change only the body, file another body under their number before the making, as `costume_file` does for Pius (number 0) with body 86 from level 13. `ed151` changes it in play for the story, Anthony's rotting, but also changes the number and reopens the moves.

**A body twists on another's moves, unless its bones are in the same places.** Each body's `+4` and the head of each part of a character's file hold the same skeleton word. A body plays any file with its own word as it is, and many share one (`tools/ed/facts/checks/moves_of.py`). A move's tracks are a list by bone number, 16 bytes each, and each bone has a name's hash at its `+64` in the table at the body's `+36`, 80 bytes each.

**The ghosts are in Alex's last chapter and they fit their characters.** In `U_Level13.bin`: 142 Ellia, 143 Anthony, 144 Karim, 145 Max, 146 Paul, 147 by its look Roberto, each with the word of the living one's file. `U_Level00.bin` has 140, Max's blue spirit, made by script 1438 at `MaxGhostMkr`. The ghosts' own files (17 moves or so) aren't needed: they play the living one's moves.

**Other bodies are fitted by bone.** `kCostumes` lists a form's body. Where its skeleton word isn't the wearer's, `hle_tracks_load` catches each move as it streams in (`0x8012E588`, which makes the list's addresses), and puts each track where this body's bone of that name is, and the root track after the last bone. `costume_tracks_on` moves the root's place in the part's `+36`. When most names don't match, it goes by nearest bone under the same parent. Edward's ghost has 62 bones to his 72, 61 of them by name, in `/levels/level17/cin0077.bin` as body 39, the Edward's Ghost insanity scene. Ellia's rotten body has none of her names and 59 fit by place, in `/levels/level00/cin0025.bin` as body 37, the last scene of Lindsey's chapter (`L06/906`). The pack's other bodies: 38 Edwin. In `/levels/level06/cin0024.bin`, 36 is the Liche, 37 Edwin and 38 the alignment's enemy.

**By place is by each matrix's `t` first, then by where each bone is.** A bone's matrix at `+12` is world to bone, so its `t` is its place turned into its own frame. A body turned as its wearer is fits the same either way, and the forms were all fitted by `t`. Chandra's own body (`/levels/level04/cin0014.bin` 36) has her arms turned down from Alex's: by `t` 17 of 61 fitted and her hair and fingers were drawn into slabs. Where fewer than half fit, it goes again by `-R^T t`, and she fits 57. `ED_BONE_DUMP=1` prints both skeletons.

**Where bones go by place they take the wearer's names too,** as what a character holds finds its bone by name: Ellia's rotten body held her sword wrongly until then.

**Forms from Paul's chapter:** Anthony's rotten body is 137 in `U_Level07.bin`, the enemy in room 104 (character 132), with his bones under 56 of 70 names. In `/levels/level07/cin0030.bin`, 38 is the Bishop in red, 40 a monk and 37 a man in black, all Paul's forms. `cin0107.bin` body 36 and others with no names in common are Paul himself in those scenes.

**Max's straitjacket isn't a form, because it can't free its arms.** `/levels/level00/cin0020.bin` body 40, the last scene of his chapter (`L05/2034`, `ed55(25)`). 55 of its 58 bones are Max's by name, and it walks on his moves, but its mesh has 5 groups to his 16: arms and body are one piece. So its arms stay bound while their bones move, and what he holds is held by a hand that isn't drawn. The user had it taken out for that.

**A loose bone becomes the head's twin.** A scene's body has bones with no parent and no track, and `costume_loose_bones` gives each the head's place as made, its parent and its track. The map is made again each time a player is made, on the same body, so a bone already made a twin is kept as one. This isn't what fixed the eyes.

**Eyes and Anthony's jewel follow the face.** They're small mesh groups of one batch, at any place in the list (Paul's red robe 0 and 1, his black robes 6 and 7, rotten Anthony 0 and 1), and on a scene's body that batch follows something the wearer's moves don't move, so they float off. `costume_eyes_on_face` sets each to what the face's largest batch follows: the first of a batch's 6 bytes at `+48`. Points are in the body's place, so they stay where they are on the face.

**A bone with no track stays as made.** The Liche's cape chain, bones 17, 40 and 74, is on nothing of Pius's, and it was modelled flying out. Its hanging pose is in its own file, whose moves are packed on the disc (a record's `+284` the size there, `+288` unpacked, read by `0x80154118`). So `held_file` has the game stream one standing move from that file, once, and keeps each bone's first keys. On each of the wearer's moves those bones get two keys of it, at the move's first and last times, in a list moved to `SCRATCH_TRACKS`.

**A ghost's weapon is part of its mesh, in whole pieces.** A mesh group is 276 bytes from the body's `+40`: `+0` how many strips and the first (at `+64`), `+4` the same for texture places (`+72`), `+8` for points (`+80`), `+16` for batches (`+48`). A strip's `+1` is how many corners and `+2` the first, 4 bytes each from `+68`. **A corner's `+0` is its point's number, and its `+2` a place in the texture list, not a point.** The table at `+176` has each point's number times 6, in the points' order, so it's what turns a corner into a point. Joined that way, a weapon is pieces that don't touch the body, and none of whose points is in the wearer's body: Roberto's 3 pieces of 255 points, Karim's 2 of 153, Paul's 3 of 135, Anthony's 1 of 226, Ellia's 1 of 184, each in the group in `kCostumes`. No other group has such a piece. `costume_strip` puts all of a piece on one of its points. A few of its points follow a finger rather than the hand, so that one is the hand batch's point nearest them. What's equipped is another object, held by a bone, so it isn't touched. Max's spirit has none.

**A ghost form has blue sparkles, and only a ghost.** `ghost_sparkles_step`, for each player whose form has `sparkles` in `kCostumes`: Ellia's, Anthony's, Karim's, Paul's, Roberto's and Edward's ghosts and Max's spirit. Not the Liche, the rotten bodies, the Bishop, the monk or the black robes. The ghosts in scenes, Max's in the clock room (room 238, scene 64, script 1536) and Edward's (scene 77), have theirs baked into the scene's file, with its mist: the blob at the file's `+108`, set up by `0x8012BD64`, drawn by `0x801F4158`, which can't follow anyone. So a form uses the sparkle of `ed184` (`0x8014CF74`, sprite 57, as in script 2410), in `0x47C1EA`, the game's blue next to `particle.c`'s name: one effect for each player, which keeps going for 65000 frames, with its centre moved to them each frame. It lasts until the room changes or its frames end, and it's made again; a scene ends it (`ghost_sparkles_scene`, on `0x801753F4`, as the player's frame doesn't run in a scene). Each effect is 336 bytes in the list at `r13-23128`, `r13-23132` of them. Its numbers: `ed184(place, a, b, sprite, alignment, d, e, f, g, h, i)` has a sprites, each up to b in size, starting within e of the centre (a power of 2) and rising at d to d+3 for h to h+7 frames, a new one each g+1 frames, for i frames in all. A form's are `12, 6, 57, 1, 64, 0, 3, 64`, and `ED_SPARKLES` changes them to compare. The user chose small and steady: the b of 8 looked too flashy.

**The sparkles aren't lit, though the game says they are.** All of them come as one draw (`tex=44x44`, no normals, `lit=1`, `mask=00`), and `gxcore_draw.cpp` took it for a lit thing: a second set of lights for the wearer, whose shadow radius went from about 210 to 300. It's now left out by that sheet, like the spell's 63x63. Other effects' draws, marked the same, are as they were.

**A form's picture on the character screens** is `res/ui/portraits/NAME-FORM.png`, not committed, as the others aren't. `tools/ed/form_portraits.py` makes them by photo mode (`ED_PHOTO_PORTRAIT`). With none, the normal one shows.

**A cutscene's pack is the same kind of pack.** `0x80007FA0` makes `cinNNNN.bin` the list at `r13-26452` while the scene plays, with its actors from 36 up, so a level's list has 160 places and a scene's about 40. Table `0x80242150`, 32 bytes each: `+0` high half the `cin` file, `+8` high half the room, `+16` top byte the level folder, next byte the `.bin`.

**A form changes a player's size, unless corrected (2026-11-06).** `0x80117F64` makes a character with radius max(f32 r13-28952 (`0x8013A364`), body +248), written to object +624, +628 and +620 (the z of the offset at +612). The step sweeps a ball of +628 (`0x80118858`, then `0x801388CC` and `0x8013085C`, which tests other characters too). Rotten Ellia's body (cin0025 body 37) gives 135.6 against her own 50, and she couldn't move in room 158's corridor. `costume_size` gives a player in a form their own body's size, each frame, only while the radius is still the form's.

**The freezes at `0x800085E8`, right after a reload or at a door in costume, were a stack overflow on the game's monitor thread:** `0x80325188` in `docs/coop-no-pause.md`.

**Moves parts and slots (2026-11-06, round 3).**
- Object +64 is the animation block. The controller in use is at +2176 (u16) times 272. Its +184 is the set record (16 bytes: id, animation, part, flags), and +240 is the move id.
- Object +348 holds 32 track states of 24 bytes each, and +668 holds the track flags, 40 bytes each.
- `0x80118D98(object, part, force)` sets a part. It asks through `0x801322C0`, then `0x80132970`, and once that is ready, `0x80118D30` sets +660 and sets bit 0x1000 in +596.
- `0x80123698` is the per-object animation update. With 0x400 clear and 0x1000 set in +596, it looks the set up again by id (`0x80123580`) and starts it (`0x80123224`). That start frees the old part's slot through `0x80132BB0`. Otherwise it runs the tracks (`0x8011FF74`, key search `0x8011FD64`).
- The game's slots are at `0x80582000`: 80 bytes each from +696, with +764 ready, +768 sets, +772 load state and +773 in use.
- Co-op's host slots for player 2 (`hle_moves_want`) must not load over the slot whose move still plays (`moves_slot_playing`). A ch6 standing player asks for parts 63, 0 and 4 in a row, the third went over the playing move, and `0x8011FD64` walked memory for ever (the ch6 hang).

**What a character holds to fight with:** `0x801F99F0(node)` returns the first entry on node+48 (+4 next, +8 id) whose record (the found node's +44) has flag 0x2 in `0x80150BE8`. In co-op, player 2's carried list is always empty. Their weapon is their equipment's copy: `0x80151500(id, 0)`, count at +388, ids at +4 + 12i. FINISH HIM's look `0x800330E0` returns 0 when this answers nothing (`0x800331F4`).

**Weapon sevens:** Lindsey (73) and Michael (81) have no sword seven 14. Their swings and move 65 are in part 42 (42-47); Michael's 77-82 have swings 5-8 but no 65.

## Grabbed: BREAK FREE (round 5, 2026-11-06)

- The grab is the player's behaviour kind 19, `0x80060EC8` fn(node, state, message, answer), registered at `0x80030E14`. It is set by `0x8005FF34` (`0x80060050` li r4,19), which records the grabber at actor+140 -> +20 and the old kind at +24.
- Event 1 in state 0: state 26 (seized, motion 35). If the one held is a player (actor +158 == 1, `0x8003392C`), `0x80117960(1)` blocks all six prompts (slot +16 = 1). Event 35: motion 37, state 28. Event 2 leaving 26: `0x80117960(0)`.
- State 28's tick (event 3), for a player: `0x80117404(6, 20, 0, 0, 50, 1, 0)` at `0x80061A64`, BREAK FREE. The tick also sends event 102 (let go) when the busy ask `0x80155738` says yes and the held one is the player (`0x80061A8C..0x80061AB4`). That 102 is sent from the held one's own id. The other way to be let go: the game's busy set `0x801556CC` sends event 102 from the player to everyone else (`0x801F5620` at `0x80155720`), and kind 19 lets go on any event 102 (`0x80061148`: event 38 to the grabber at actor+140 -> +20 and to itself; answers 1). In co-op that broadcast from the other player's examine or pickup freed a held player; round 6 passes it over unless busy has bit 4 (a room change).
- Event 44 (the controller's, data = the control block): `0x80060DC4` counts stick sector changes (8 sectors, a dead zone of 20) into actor+140 -> +357. At 8: event 37 to self, motion 39, event 37 to the grabber, state 123; event 6 ends 123 and the old kind comes back.
- Hits come as event 39 from the grabber, about every 8-10 frames, 1 health each (`0x80060188`). Event 38 from anyone at state 0 lets go.
- Measured the same in solo and co-op: wiggling the stick left/right counts 1 per change and frees at 8.

## Finisher (round 5, 2026-11-06)

- The look `0x800330E0` goes through the room's nodes. For each downed body (state 8 or 9) it asks event 193 at `0x8003335C` (r4 the looker's id, r5 the body's). `0x80033534` chooses the body's bone and place (look +56..64, part at +84); the body id goes at look +116. The look block is node+40 -> +0.
- `0x800C30CC` (event 202): the victim goes to node+40 -> +140 -> +28; the reach comes from `0x80150B74(0x8019BA14(block))`; the slide toward the bone is put in the move channel's +228..236 by `0x80121FD0` and cleared at the hit by `0x80122028`. The turn and slide differ with the side of the body (47 vs 138 units to the bone in ch1 room 342), in solo too.
- Finishcam01-03: state 30's entry (event 1 of `0x80051F0C`) counts r13-30420 down; at 0, with r13-25580 == 0, the room not in the list at `0x80243850`, the victim not in `0x8030AB10` and no engaged enemy within 700, the game tries the cameras (`0x80051D48` tests, `0x80051E28` -> `0x801ECE90` puts on). Seen in room 342 (2026-11-06 traces). The cameras are not the room's: they are in the finisher's body model (object+60 -> +132 count, +136 entries of 120 bytes, +52 hash, +56 name). Forms' bodies have none (Chandra's ghost and rotten Ellia: 0).
- **Which finish plays (round 7, 2026-11-06).** FINISH HIM plays only move 65: `bl 0x80122920(object, 65, 256, 6)` at `0x800C3254` (lr `0x800C3258`), from the holder's own table (object+648). The table is built by `0x800CBE7C` -> `0x80118D98` -> `0x801322C0` -> `0x80132970` -> `0x80133310` from the held weapon's part (InvU +54, a group of seven, plus the place from health and sanity). Each character file has one move 65 per group, made for that character's own weapon: Karim's and Alex's for the swords (part 14) drive both fists down with the blade held up. A missing move 65 falls back to table[15]. Guns play no move 65.
- **The weapon's own finish (round 7, `fin_swap` in hle.c).** At that call, for either player, the host plays the held weapon's home character's move 65 for the weapon's group at the holder's place, fitted to the holder's body (and form) bone by bone by name. Bones the home doesn't have hold the first key of the move played just before. The holder's own move 65 goes back on their next move. The holder's own weapon, a weapon with no home, or a home file with no move 65 there: nothing changes. The held item comes from the finisher's attack block (r25 +104: +4 its InvU record, +12 its type).
- **Weapon homes** (`python -I tools/ed/facts/weapon_homes.py --c` makes the table from the disc: where `Npcs*.npc` places each weapon, else the level of the script that gives it, lowest level first): Pius Gladius 96; Alex Gladius 64; Edward (69) Sabre 63; Peter (75) Two-Edged Sword 66; Karim Ram Dao 81, Dual Tulwar 83, Tulwar 84; Anthony Two-Edged Sword 89, Scramasax 103; Lindsey Kukri 97; Max Sabre 101; Roberto Saif 102; Ellia Short Sword 104; Michael Fire Ax 133; Paul Mace 62. No change: Enchanted Gladius 169 (Michael's file has no part 14); Two-Edged Sword 65, Ram Dao 92, Dual Tulwar 109 and Mace 106 are placed nowhere on the disc.
- **Each player's close-up (round 7, `coop_finish_camera`, `coop_finish_body`).** States 30, 74 and 84 (`0x80051F0C`, `0x80052384`, `0x80054E70`) put the close-up on at event 1 and pop it at event 2. Each player has their own count, flag, rotation and camera set; player 2's words are `g_fin_p2_words`, reset at play start (`0x80021648`) as the game resets its own. A form uses its wearer's own body's cameras for the test and the put only; this also works in solo. Player 2's event 1 comes while the cameras are lent, because FINISH HIM runs script 2244 as their use.

## Weapon parts with no fallback (round 5, 2026-11-06)

- The game's only missing-part rule is `0x800DAF10..0x800DAF88` in `0x800DA8E0` (a thing takes the part of what it belongs to): the part, else part 0, else the file's first part (`0x8003F478`). The player's own equip path (`0x800CBE7C` -> `0x80118D98` -> `0x801322C0` -> `0x80132970`) has none.
- Moves each part adds over bare hands, by file: 14 {5, 6, 7, 8, 9, 65} (Ellia and Paul lack 8); 42 {5, 6, 7, 9, 65}; 56 (Paul) {5, 6, 7, 8, 9, 65}; 63 {10, 113} (Karim's adds nothing); 77 {10, 113, 158, 159}; 84 (Peter) {10, 113, 158, 159}; 105 (Paul, Roberto) {10, 113}; 112 (Ellia) {113}.
