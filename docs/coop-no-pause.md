# Co-op: pickups and words don't stop anyone

**The user's rule:** when one player picks something up, examines or reads words, the other keeps playing. The finder sees the item and its words, drawn once, in their half. The other player gets only a notice at the top right of their half. This holds for any character and chapter, so fix it in shared code, never per room or per model.

Confirmed in the user's play on 2026-11-03: slot 1's urn and rune from both players, the spell page with player 1 free, and Paul's podium.

## If it breaks: symptom, cause, where

| Symptom | Cause | Where |
|---|---|---|
| Examine shows no words | The window was passed over | `hle_message_open` |
| The other player stops during words | The words' hold (`0x80042A8C`) | `coop_words_hold` |
| The other player stops during a find | A find's hold (`0x80042BB8`) | `coop_find_hold` |
| Their stick is read, but they can't start from standing | A script's own hold (`0x8016820C`, the urn) is still on. Clearing its busy bit isn't enough: let go with `0x80199EA0(1)` | `coop_script_hold_go` |
| Player 2's item from their Tome: words in player 1's half, then a freeze | No use of theirs is up then (`tome_copy_left`), so ask whether they are the player (`r13-21492`) as well as `g_coop_use` | `coop_no_pause`, `coop_their_close_up` |
| Player 1's stick is read during player 2's words, but they stand still (event 45, not 44) | The window has the input focus (`r13-23552`, `-23556`), and the controller's channel is passed over. Player 2's saved copy of those words keeps them free. Player 1's turn clears them | `coop_player1_turn` |
| Player 1 stops when a Tome page's help opens | The same focus, over a Tome the game opened for player 2 | `hle_coop_controller` |
| The item shows in both halves | It's a world object tied to the game's camera. Its matrices are marked as it's drawn, and the other view collapses them | `coop_found_for_finder`, `coop_found_mark`, `build_view` in `gfx/interp.cpp` |
| Player 1's half is a still picture in player 2's close-up | Their half is drawn from their last camera, following them | `camera_split_update` |
| Both players in attack position | Seen in one build, which called `0x801223B8` to clear an object's mark. Taken out and not traced. Don't call it for this. The players not being each other's threat is `coop_send_event` | `docs/coop-failed-attempts.md` |

## Don't

- **Don't skip the close-up.** Its opening and closing give the item. Skipped, the rune was never given and could be found again and again, its notice repeating. The user rejected self-closing too.
- **Don't hold the other half on a still picture.** It only looked right when the whole world was paused (`d0b1fdc`).
- **Don't widen a hold check to an address range.** `0x800429D0`..`0x80042C10` takes in other holds, and Paul's cutscenes stopped going to the right place. Which part did it wasn't proven.
- **Don't treat the two players' mechanics as different.** A fix for one is the fix for the other. Start by asking what the other player's turn has that this one lacks.

## How to test

Scripts have missed these bugs, so test the way the user plays.

- **Start the other player from standing still.** One already walking coasts on and looks fine.
- **Run the sequence past its last step,** into the spell page's help and out of it.
- **Run both players.** `build/runs/enchant/both.sh NAME EXE` replays player 1's urn with player 2 walking. `P1SCRIPT=... sh build/runs/enchant/p2urn.sh NAME` replays player 2's with player 1 walking. Both are gitignored. `[trace] no pause` and `[trace] player 1's turn` log positions.
- **Then have the user play it.** Believe what they see over the log.

## Limits

- Notices read the game's English words (`has picked up`, `has found`). Other wordings get no notice, and a log line says so.
- Spell pages aren't traced, so a spell found may not give a notice.
- Only slot 1's urn and Paul's podium have been played.
- Player 1's bars are likely hidden during player 2's close-up, and their camera follows them without turning.
