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
