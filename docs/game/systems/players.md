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

**Where bones go by place they take the wearer's names too,** as what a character holds finds its bone by name: Ellia's rotten body held her sword wrongly until then.

**Max's straitjacket isn't a form, because it can't free its arms.** `/levels/level00/cin0020.bin` body 40, the last scene of his chapter (`L05/2034`, `ed55(25)`). 55 of its 58 bones are Max's by name, and it walks on his moves, but its mesh has 5 groups to his 16: arms and body are one piece. So its arms stay bound while their bones move, and what he holds is held by a hand that isn't drawn. The user had it taken out for that.

**A bone with no track stays as made.** The Liche's cape chain, bones 17, 40 and 74, is on nothing of Pius's, and it was modelled flying out. Its hanging pose is in its own file, whose moves are packed on the disc (a record's `+284` the size there, `+288` unpacked, read by `0x80154118`). So `held_file` has the game stream one standing move from that file, once, and keeps each bone's first keys. On each of the wearer's moves those bones get two keys of it, at the move's first and last times, in a list moved to `SCRATCH_TRACKS`.

**A ghost's weapon is part of its mesh.** A mesh group is 276 bytes from the body's `+40` (`+8` how many points, `+10` the first), its points 6 bytes each from `+80`. `costume_strip` puts the points the living body hasn't, in that group, all in one place, so they draw nothing: Ellia's group 5, Anthony's 6, Karim's 14, Paul's 5, Roberto's 14. A stray point of the weapon in another group goes to the nearest of that group's own: left alone, it drew a thin line. What's equipped is another object, held by a bone, so it isn't touched. Max's spirit has none.

**A form's picture on the character screens** is `res/ui/portraits/NAME-FORM.png`, not committed, as the others aren't. `tools/ed/form_portraits.py` makes them by photo mode (`ED_PHOTO_PORTRAIT`). With none, the normal one shows.

**A cutscene's pack is the same kind of pack.** `0x80007FA0` makes `cinNNNN.bin` the list at `r13-26452` while the scene plays, with its actors from 36 up, so a level's list has 160 places and a scene's about 40. Table `0x80242150`, 32 bytes each: `+0` high half the `cin` file, `+8` high half the room, `+16` top byte the level folder, next byte the `.bin`.

**The freezes at `0x800085E8`, right after a reload or at a door in costume, were a stack overflow on the game's monitor thread:** `0x80325188` in `docs/coop-no-pause.md`.
