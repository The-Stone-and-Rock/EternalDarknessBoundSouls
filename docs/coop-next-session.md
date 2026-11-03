# Co-op: where things stand

Written 2026-11-02. Read this first, then update it at the end of the session.

## How to work on this

These come from the user, some of them more than once.

- **Check first.** Before calling something unknown or writing a fix, search the code comments, `git log` and these docs. A lot is already written down.
- **Don't guess.** Say what's confirmed and what isn't. The logs don't show everything, so if the user says the game did something, believe them and find out why.
- **The game allows one player.** Co-op works by making player 2 "the player" while they act. That switching is the design, not a bug. Only raise it when a real symptom points to it.
- **When one player enters a room, the other follows automatically.** "Enters first" means which player triggered the move. There is no walking in second.
- **Scenes and transitions belong to whoever triggered them.** If player 1 walks up to a zombie, player 1 gets bitten.
- **Names in the game's text** come from the chosen model, because player selection is on.
- **Keep what you learn.** Add findings to these docs as you go. Don't remove the `[trace]` logging.
- **The user plays the tests.** Their real play finds what scripts miss, and they press the buttons while you capture. Scripted presses for player 1 don't work with real controllers plugged in: the script pad joins as controller 3.
- Commit only when told. Never commit `runtime/host/controls.cpp`.

## Two Tomes (branch `tome-copy`)

Both players' Tomes work at once, with player 2's in its own copy of memory. It's on by default since 2026-11-03, for every character and chapter, as the user asked. `ED_COOP_TOME_COPY=0` gives the old way. Details, results and limits are in `docs/coop-two-tomes-design.md`, so read that before changing it. A restore point from before this work is in `logs/saved-changes/working-01b3d85-before-tome-copy/` (see its `RESTORE.txt`), with the tag `coop-working-before-tome-copy`.

To run it from PowerShell:

```
$env:ED_COOP_TOME_COPY = '2'; & 'C:\Program Files\Git\bin\sh.exe' tools/coop-test/play.sh saves/slot3.state NAME
```

Each player's spells made on the Magick page are their own (`coop_spells_in`, kept in save states as `COSP`). Runes and circles are one for both, and a spell with its name known in the game's record goes to player 2 at their next Tome. Not done: sound from player 2's Tome, Mix, labels in other languages. Both pressing Enchant at the same moment passed by hand on 2026-11-03 (`logs/run-20261103-093139-hand8`, presses 2 and 3 frames apart). Page turns with both up, and player 1 closing with player 2's staying, passed in the logs on 2026-11-03 (`logs/run-20261103-093522-hand9`). To confirm by hand: player 1 pressing Start with player 2's already up. The scripted test edits in `runtime/host/controls.cpp` (`l`, `r`, right stick) are uncommitted, as asked, and scripted page turns need them.

## Next up

1. **Trace a tablet and a spell page.** The user will make a solo save just before picking up each. Log how it's called, then build what they asked for: whoever picks up the last object that finds the spell gets the Tome's spell-found page, and the other player gets the spell with a notice like `(Character name) found "Enchant Spell"`. A rune and a page must reach both, as there is only one of each. What a page writes isn't traced: `0x08000000` as "name known" is inferred from `0x0C010304` against a made `0x04010421`. Also confirm by hand that player 2 can cast a spell only in their list.
2. **The flicker on making a spell.** The user saw it slightly. `You Have Just Discovered` was logged about 660 times in `logs/run-20261103-114339-magick-p2`, which may be the cause. Not looked into. Which half isn't known.
1. **Trace Mix.** The user will make a save just before mixing a two-part key, and will say when it's ready. Have player 1 mix first, then reload and have player 2 do it. Add both parts to `docs/items-log.md`.
2. **Retest the monk's scene** (player 2 rings the bell). `48b6c88` stopped a script's own hold counting as player 2's close-up, and that scene may use the same hold. It should still show in player 2's half.
3. **Check other arrivals when player 2 triggers the move.** `48b6c88` puts player 2 on the arrival spot in any room their script asks for. Only Anthony's first Tome room entry has been seen.
4. **Finish the player 2 menu traces:** the item list walk, the Magick page, and story scripts on opening the Tome. See `docs/coop-player2-menu.md`. No menu code exists yet.

## Open bugs

| Bug | What's known |
|---|---|
| Picture still with no hang report, first time | `logs/run-20261103-105156-paul-coop` (two Tomes off, player 1's Tome up, player 2 pressing Start). The same picture as the menu bug below, but that bug was only reproduced with player 2's Tome in the copy, and the menu shows on this path by script. Whether the user had pressed Back then isn't known. With `ED_COOP_TRACE` on, `[stall]` and `[pcdump]` show where the game is if it comes back |
| Using a long item holds the other player | When player 1 used the Bronze Necklace (script `0x5E6`), player 2 couldn't move until it finished, and no presses of theirs were logged. The user wants them able to move. Player 1 during player 2's Use wasn't tested |
| Camera glitch after player 2 closes a close-up | Slight, and not diagnosed. Which half and how long aren't known yet |
| Player 2 pressing Enchant while player 1's Tome is open | Player 1's cast takes player 2's rune. `0x80335748` is shared. Left for two-Tome work |
| Player 2 backing out of their Tome without casting | Later quick spell presses failed once. It may have been the stray-press bug fixed in `74b9891`. Not confirmed |
| Player 1's camera stuck | The user saw it just before closing `logs/run-20261102-223052-test2` (slot 3, about frame 31995, player 1 near `1228 429 26`, far from player 2, some time after player 1's Enchant and last Tome). Player 2's Tome never opened in that run, so no turn ran in a memory copy. Not diagnosed. The user wants it looked at after the two-Tome tests |
| Scroll in an examine's Details | The user mentioned an error around the scroll. Nothing in the trace shows inside the window |
| Right half during Anthony's Tome room scene | Stays on a close-up for the whole scene. The user was happy with it when player 2 triggered it |

## Fixed on 2026-11-03 (branch `tome-copy`, not committed yet)

| What | Fix |
|---|---|
| Freeze with both players as the same character (Paul's chapter from Jump to Game, slot 7; `saves/reports/hang-20261103-103136`, `hang-20261102-215350`) | Player 1's moves table stayed empty after `moves_repair`, so every move came back with no set and the next waited for ever. `moves_repair` now makes the table, and `hle_play_animation` also passes over a move with no set in co-op. Passed by script on slot 7 (`logs/run-20261103-104830-s7walk3`), not yet in the user's play from Jump to Game |
| The in-game menu didn't show with player 2's Tome up, so the game looked frozen (`logs/run-20261103-105932-paul-coop2`) | The renderer took the menu's frames for the Tome's and the world's, and only kept them. `hle_coop_ui_over` makes them plain frames while the menu is up and puts the views back after. The menu also no longer opens in a turn of the copy. Passed by script (`ED_MENU_TEST`, `build/runs/enchant/m2.sh`, `m3.sh`), Resume included. Quit not yet tried by hand. Behind the menu the Tome's picture can fill the screen, with stray page labels. Photo mode with a Tome up isn't tested |
| Player 2's Talisman said "drained" and the message never closed (`logs/run-20261103-093522-hand9`) | `tome_copy_left` now gives the script the item their Tome chose, and doesn't run the second step if the first opened words. Details in `docs/coop-failed-attempts.md`. Passed by script and in the user's play (`logs/run-20261103-100630-hand10`) |

## Fixed on 2026-11-02 (branch `mods`, nothing pushed)

| Commit | Fix |
|---|---|
| `2e00b69` | Player 2's close-up no longer freezes the picture |
| `cefb179` | Player 1 isn't sent out of bounds after the Tome cutscene |
| `638166c` | Player 1's camera follows player 1. What writes the wrong record isn't known. A `camera record ... named player 2` log line marks when it happens |
| `74b9891` | Player 2's quick slot casts their rune, and player 1 can still cast |
| `066076c` | Player 2 can use items from their Tome (the Bronze Necklace) |
| `8fbe025` | Player 2 can examine the same thing more than once |
| `3cd22cc` | Both players can take the key from the clock, and player 2 gets the Examine icon |
| `dcafeca` | Player 2's close-up ends when it asks for another room |
| `48b6c88` | A room's first scene plays on whoever asked for the room |
| `bea486b`, `9dd5698` | The `[trace]` log, and these docs |

## The pattern behind many of these bugs

Game code runs while the wrong player's data is active. So when something works for player 1 and not player 2, check these first:

- who "the player" is (`r13-26524`, `-21492`, `-21496`)
- who the controlled character is (`-21488`, `-21484`)
- the zones' "inside" words (`coop_zones_swap`)
- the script words (`coop_states_swap`)
- what a chapter script is told the character is (`hle_object_model`)
- buttons still held when a screen closes, which reach the world as new presses

## Tools

| Tool | Use |
|---|---|
| `sh tools/coop-test/play.sh saves/slotN.state NAME` | Runs with every log and `[trace]` on. Logs go to `logs/run-*-NAME/`, which is not in git |
| `sh tools/coop-test/launcher.sh NAME` | The launcher with every log and `ED_ANIM_TRACE` on, for tracing from Jump to Game. `ED_ANIM_TRACE=1` logs each new animation played, with who asked |
| `tools/coop-test/burst.ps1 -seconds N -out NAME` | Captures frames while the user plays, into `Screenshots for Claude\NAME`. Start it a few seconds after launch |
| `tools/coop-test/sheet.ps1 -dir DIR -out FILE.jpg -pics 0,40,80` | Puts captured frames on one contact sheet, in `Screenshots for Claude\`. Run it from PowerShell |
| `python -I tools/ppcd.py generated/main.dol ADDR COUNT` | Disassembles game code, and shows many instructions only as an opcode number |

To compare what's on screen, capture a solo run of the same save first (`mod_coop = 0` in `settings.ini`), since that shows how it's meant to look. Set it back to 1 afterwards. Saves are in different chapters, so compare within one chapter.

## Saves

The user re-saves these slots, so check a slot's date and what's in it before relying on this.

| Slot | When last noted | Where |
|---|---|---|
| 1 | 2026-11-02 20:21 | The room with the clock and key. Chapter not confirmed |
| 2 | 2026-11-02 21:17 | Anthony's chapter: on the stairs just before the first Tome room entry |
| 3 | 2026-11-02 23:16 | Karim's chapter, with the Tome: Enchant on D-pad up for both, Tulwar and Chakrams to enchant. Karim and Paul, no reload |
| 5 | 2026-11-02 19:01 | Both players have Enchant on a quick slot, but only keys to carry, and keys can't be enchanted, so it can't test Enchant |
| 10 | 2026-11-03 11:20 | Made for the Magick page, in a run with player 1 character 1 (model 0) and player 2 character 10 (model 69). The Bronze Necklace save that was here is gone |


## Other docs

| Doc | What it holds |
|---|---|
| `docs/coop-player2-menu.md` | The plan for a custom player 2 menu, and what's traced for it |
| `docs/coop-failed-attempts.md` | What was tried and didn't work. Check it before writing a fix |
| `docs/coop-two-tomes.md` | Why two game Tomes at once was set aside, and how the Tome works |
| `docs/coop-two-tomes-design.md` | The design to try next for two Tomes: player 2's in its own copy of memory. Not built |
| `docs/coop-todo.md` | The longer list of open issues |
| `docs/items-log.md` | Items seen so far. Add to it in any session |
