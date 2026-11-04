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
| Pius's other body, each player's, RB or C on his frame | Built, not yet confirmed by the user | `costume_on`, `costume_file`, `costume_moves` in `hle.c`, `chosen_costume` in `frontend.cpp` | Add a separate character for it: he stays Pius |
| A scene of player 2's stays theirs to its end, each player's prompts and zones kept | Bookshelf lever, built 2026-11-04, the camera half confirmed by the user, the lever not yet | `coop_script_sends`, `coop_scan_player1`, `coop_prompt_clear`, `coop_close_up_prompt`, `closeup` in `coop_menu_input` | Run a thing's call back as player 1 |
| The other player stays in a press's scene and keeps walking, and the one who pressed leaves only their own view | Bookshelf lever, built 2026-11-04, not yet played by the user | `coop_show_asked`, `coop_hold_open`, `coop_p1_icon_show`, `g_coop_unseen` | Let `r13-26276` or `r13-26272` go to 0 in a press: they hide player 1 and stop both controllers |
| Whoever triggers a room or scene gets it; the other follows | The user's rule | `coop_follow`, `48b6c88` | Treat one player walking in second: there's no such thing |
| Player 2's prompts, and their close-ups in their half | Works | `coop_close_up_prompt`, `coop_prompt_player1s`, `coop_scan_player1` | |
| Player 2's own bars, weapons and hold | Works | `coop_rehold`, `0832701` | |

**The camera is the part that isn't settled:** it works sometimes. Its fixed cameras are in the disc's rooms (`ed room N`), and `ed check` flags any eye that isn't one of them. See `ed system camera`.

## Held until the user asks

- **Unstable frame rates, and true 120 fps in co-op.** The user recalls true 120 causing issues in the past, so don't work on it until they ask. When they do, start at Frame rate in `ed system graphics`. What's there now, which they haven't played: co-op still tries frames in between, and when they cost too much the game slows to about 50 Hz until the back-off stops them, retrying at longer and longer gaps.

## Their rules for working

- Use the normal player as the reference: `ed how THING` first, and a solo run of the same save to look at.
- Believe what they saw in play over any log.
- A fix for one player is the fix for the other. Ask what the other player's turn has that this one lacks.
- Fix in shared code, never per room, chapter or character.
- Commit only when told, and don't push unless asked.
