# What the user already has, the way they like it

**Extend these. Never rebuild them.** When the user asks for a correction or to move something, find it here or with `ed host WORD`, change the smallest thing, and run `ed test`.

The goal: the game believes it was built for two players, using what the game already has. The game has one of everything, so co-op works by putting player 2's data in and calling the game's own functions: `ed needs ADDR` says which data, and the helper that puts it in.

| Feature | Status from the user | Where | Don't |
|---|---|---|---|
| Two Tomes at once, each in its half | Works | `tome_copy_*` in `hle.c`, player 2's in a copy of memory. `docs/coop-two-tomes-design.md` | Swap more memory ranges for it (`docs/coop-failed-attempts.md`) |
| Spell casting, each with their own quick spells and made spells | Works | `coop_quick_cast`, `coop_quick_swap`, `coop_spells_in` | Change a cast to fix a Tome |
| Split-screen layout, both ways round | Works | `camera_split_update` in `camera.c`, `split_build` and `build_view` in `gfx/interp.cpp`, `mod_split` | Hold a half on a still picture |
| Pickups, words and examines don't stop the other player | Confirmed on slot 1's urn and rune, and Paul's podium | `coop_no_pause`, `coop_words_hold`, `coop_find_hold`. `docs/coop-no-pause.md` lists each symptom | Skip the close-up: its opening and closing give the item |
| The item shows only to its finder, once; the other gets a notice | Confirmed on the urn | `coop_found_for_finder`, `coop_found_mark`, `coop_found_notice` | Hold the other half |
| Character choice for each player, their names in the game's words | Works | `mod_p1_character`, `mod_p2_character`, `hle_object_model` | Compare a scene across chapters: compare with solo on the same save |
| A character's other forms, each player's, LB, RB or C on their frame on the character screens: Pius's skeletal, Ellia's ghost and rotten, Anthony's ghost and rotten, Paul's ghost, the Bishop, monk and black robes, Karim's, Roberto's and Edward's ghosts, Max's spirit | Built 2026-11-04. Not yet confirmed by the user | `kCostumes`, `costume_on`, `costume_file`, `costume_tracks_on`, `costume_strip` in `hle.c`, `kForms` in `frontend.cpp` | Give a form a file of moves: each plays its wearer's, and holds what they equip |
| A scene of player 2's stays theirs to its end, each player's prompts and zones kept | Bookshelf lever, built 2026-11-04, the camera half confirmed by the user, the lever not yet | `coop_script_sends`, `coop_scan_player1`, `coop_prompt_clear`, `coop_close_up_prompt`, `closeup` in `coop_menu_input` | Run a thing's call back as player 1 |
| The other player stays in a press's scene and keeps walking, and the one who pressed leaves only their own view | Bookshelf lever, built 2026-11-04, not yet played by the user | `coop_show_asked`, `coop_hold_open`, `coop_p1_icon_show`, `g_coop_unseen` | Let `r13-26276` or `r13-26272` go to 0 in a press: they hide player 1 and stop both controllers |
| Whoever triggers a room or scene gets it; the other follows | The user's rule | `coop_follow`, `48b6c88` | Treat one player walking in second: there's no such thing |
| Player 2's prompts, and their close-ups in their half | Works | `coop_close_up_prompt`, `coop_prompt_player1s`, `coop_scan_player1` | |
| Player 2's own bars, weapons and hold | Works | `coop_rehold`, `0832701` | |
| Mods, Sanity Effects: any effect on a save, single player, with `logs/sanity-effects.log` | Built 2026-11-04. Nine effects through a door confirmed by the user, the menu page not yet tried | `sanity_ask`, `sanity_door_run` in `hle.c`, `docs/game/systems/sanity.md` | Start a door effect where you stand: it needs the door's own steps |

**The camera is the part that isn't settled:** it works sometimes. Its fixed cameras are in the disc's rooms (`ed room N`), and `ed check` flags any eye that isn't one of them. See `ed system camera`.

## Held until the user asks

- **Connecting the chapters, for sanity effects and or a randomizer.** The user's idea for later: rooms that lead into another chapter, used on purpose. What's known: `ed87(a, level, c)` changes chapter (scripts 2028 and 1572 to 1581, the Chapter Pages), and `ed system levels`. A door tour on slot 4 goes from room 186 on into room 71, a mansion room, with player 2 along. It froze on 3 runs until the `0x80325188` stack overflow was fixed (`docs/coop-no-pause.md`), and now goes on: `ed chapters 4` reaches it. The user thinks it's a door that leads to another chapter, so two players crossing chapters is still the first thing to look at.
- **Unstable frame rates, and true 120 fps in co-op.** The user recalls true 120 causing issues in the past, so don't work on it until they ask. When they do, start at Frame rate in `ed system graphics`. Co-op is capped at 60 for now, by `settings_apply`, and the setting stays for solo. Most co-op games do the same, as the user says. Lifting the cap brings back frames in between that cost too much: the game slowed to about 50 Hz until the back-off stopped them.

## Their rules for working

- Use the normal player as the reference: `ed how THING` first, and a solo run of the same save to look at.
- Believe what they saw in play over any log.
- A fix for one player is the fix for the other. Ask what the other player's turn has that this one lacks.
- Fix in shared code, never per room, chapter or character.
- Commit only when told, and don't push unless asked.
