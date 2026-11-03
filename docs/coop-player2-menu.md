# Co-op: a menu for player 2

**Status: nothing built.** The goal is a menu player 2 can use while player 1 has the game's Tome, doing what the Tome does through the game's own functions. Two game Tomes at once were tried and set aside (`docs/coop-two-tomes.md`).

The game allows one player. Co-op works by making player 2 "the player" while they act, so that switching is the design, not a bug.

To see how a player's actions run, use `sh tools/coop-test/play.sh`: it sets `ED_COOP_TRACE=1`, which adds read-only `[trace]` lines to the log. Run with a player 1 action first, since it's the working path to compare with.

## What's known, by action

| Action | How the game does it | For a menu | Seen working |
|---|---|---|---|
| Use | The item's data holds its script (`0x80150850`). The Tome calls `0x801642D4(script, item id, 0)`, then the world frame runs the script left in `r13-26324` (`800082A8`) | Run both with player 2 as the player and their zone words in (`coop_run_script`) | From player 2's Tome: Bronze Necklace |
| Check | Item page states in `r13-24492`: 0 list, 1 actions, 3 zoom in, 4 text, 5 zoom out. Text ids are in the item's data at `+0x4C`, `+0x4E`. No scripts run | Read the text directly. The 3D zoom uses the shared item drawing | From both players' Tomes, in the log only |
| Equip | `0x800C9FD4(character, item, 0, hands, 0)` at `80104070`, after checks `801514B8` and `801512F8`, then `8011031C` and `80101DC0` | `coop_restore_held` already equips player 2 outside the Tome | Outside the Tome: `coop_restore_held` exists, not run in this work |
| Quick spells | Player 2's are in `g_coop_quick[5]` | Edit that list | From player 2's Tome |
| Casting, Enchant | `coop_cast`, `COOP_QUICK_CAST`, `coop_be_player2` | The same calls | From player 2's Tome and quick slot |

Items and counts:

- **Carried list:** the Tome lists what player 1's node carries, and both players share it (`hle.c`, by `g_coop_tome_p2`).
- **Counts:** player 2's own, through `coop_kinds`.
- **Weapons:** player 2's own copies, in `g_coop_copies`.
- **Equipped:** their own record in the table at `0x80588DF0`.

## World examine

This doesn't use the Tome, so a menu doesn't replace it. One record at `0x80307108`, shared by both players. Each press moves its counter (`+8`):

| Press | Counter | What happens |
|---|---|---|
| Examine | 0 to 1 | Close-up opens, Details prompt (34) offered |
| Details | 1 to 2 | Words open, from the record's `+C`, `+10` |
| Close the words | 2 to 3 | Through the window's close. Exit prompt (7) offered |
| Exit | 3 to 4 | Stage set to 0, end script runs (2240 at the place traced) |

Scene and transition belong to whoever triggered the room move. The other player is brought along automatically. To see how something should look, capture a solo run of the same save (`tools/coop-test/burst.ps1`), since logs don't show the picture.

A chapter's scripts can also ask which character an object is (`0x8000C724`). Player 2 is told they're the chapter's own while they're the player.

## What a menu has to do

- Don't open while the close-up flag `r13-23344` is set.
- Treat a button held as the menu closes as not pressed. It reached the world as a press three times: an attack, an examine, a stray cast.
- Make player 2 the player, with their zone words, for each game call.
- Set `0x80335748` (the spell and item waiting on a Tome) only for the call, and put it back. It's shared, and it's the cause of the rune mix-up.

## Still to trace

| What | Why |
|---|---|
| Mix | Nothing known. A save with a two-part key is coming |
| Item list walk (`801F63A0`, `801F9938`, `801F98C8`) | The menu's list has to match the game's |
| Magick page and spell building | The record at `0x80334D50` is known (`hle.c`, by the All magick cheat). The page isn't |
| Map | Draws through the Tome. May stay there |
| Story scripts on opening the Tome (`80102A90` on: 480, 1979, 694, 821) | Flag-gated. A menu that skips the Tome's code wouldn't run them. Script 1979 ran as player 2's Tome opened |
| Script 2240's contents | Its effect is seen, not read |
| Scrolling in a window | Not visible in the trace |

Not needed for player 2 for now: Journal, Cinematics, Options.

## Open problems

- Using a long item (`0x5E6`) holds the other player. In `docs/coop-todo.md`.
- A camera glitch after player 2 closes a close-up. Not diagnosed.
- Player 2 pressing Enchant while player 1's Tome is open changes player 1's rune.
