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
| Player 1's half is a still picture, or a fixed angle, in player 2's close-up | Their half is drawn from the game's camera step run in their turn, with their records in, as player 2's Original camera is. The user's rule: it stays as it was before the close-up | `coop_player1_camera`, `camera_split_update` |
| Player 1's text shows in player 2's Tome, and player 2's B skips player 1's finds | The copy of memory took player 1's window and its script: their B closed it there, and the script ran on in the copy as theirs. Left out of the copy as it opens | `tome_copy_no_window` |
| Player 2's Tome plays the Magick page's help every time, on that page, then closes | The copy came from a snapshot taken before player 1's Tome, with flag 989 on and 990 off, so it looked like a first visit. The copy takes the real flags, and the help counts as done in the copy alone. Player 1's real text isn't touched | `tome_copy_step`, `tome_copy_no_window` |
| Player 1's words go, and their prompt moves to the wrong side, when player 2 opens their Tome | Player 1's view hid the HUD whenever any Tome shared the frames, and hiding also brings back what's drawn for player 2's half. Only their own Tome hides it now | `hle_coop_player1_tome_up`, `camera_split_update` |
| No notice for a find | The words weren't recognised (a Circle of Power says "has acquired"). Other wordings with "has" now show as worded | `coop_found_notice` |
| Player 2's look at a thing stops at its prompt, and only player 1's press goes on | A script's hold was taken for a way out's, not their close-up. A hold that asks for a look's prompt (`0x8000A424`) is a close-up | `coop_look_prompt` |
| Player 1 can't move during player 2's look | The script's hold is asked for again as the look goes on. Let go in player 1's turn, its bit kept for this host | `coop_player1_turn` |
| No spell page after player 2's find | The game asks whether the player has the Tome (`0x80109900`), and in a use of theirs only their own list was asked. The Tome is shared | `coop_has_tome` |
| Tome won't open after a hold is let go | `r13-25696` stays on with only `0x80199EA0(1)`, and `0x80006D0C` asks it | `coop_script_hold_go` |
| Player 1's camera locks onto player 2 | `0x80042B30(0)` ends with `0x80134D48` on "the player", who is player 2 then. Don't call it: clear what it clears | `coop_script_hold_go` |
| Player 2's icon shows in player 1's half, or at player 1's corner | A view hiding its HUD brought back what's drawn for player 2's half, in player 1's view too: now in player 2's only. In their close-up the game's prompt goes where theirs does, at `COOP_PROMPT_AT`: its position doesn't take a negative, and -120 left it off screen. Player 2's view always brings that range back | `split_build` in `gfx/interp.cpp`, `coop_close_up_prompt` |
| Player 2's close-up icon goes, player 1's Examine shows instead, and player 2's B does nothing | Player 1 in a zone ran its script on the game's prompts, which are player 2's while lent. Player 1's now go to their kept copy | `coop_prompt_player1s` |
| Player 2's Tome opens over their close-up and locks | The game's pad is theirs in a close-up, so their Start opened the game's Tome (`logs/run-20261103-163655-lock1`, frame 10164). Held until it closes | `coop_menu_input` |
| Player 2's close-up icon goes, and their B stops working, when player 1 walks out of a zone | Leaving a zone clears what its script asked for (`0x80134758`, `0x801170D4`), on the prompts that are player 2's then. Player 1's whole zone look runs on their own kept prompts (`logs/run-20261103-165129-leave1`) | `coop_scan_player1` |
| The notice names the wrong player, on the finder's own half | The find ran twice: a camera's arriving (`0x801EC1C8`) went on with player 2's script as player 1's, from player 1's word (`logs/run-20261103-165129-leave1`, frames 11200 and 11410) | `coop_run_script` |
| Player 1's half framed from player 2's camera in their close-up | The game's camera is lent a few frames before the close-up counts, and player 1's was kept in those frames too. Only kept while not lent | `camera_split_update` |
| Player 1 walking changes player 2's close-up camera | Zone scripts set the cameras, which are player 2's while lent. Player 1's zone look runs with their own cameras and prompts in | `coop_scan_player1` |
| Player 1's stick goes the wrong way in player 2's close-up | The controller reads the game's camera, player 2's then. Player 1's turn puts theirs in, as player 2's turn does | `coop_player1_turn` |
| Player 2's close-up icon goes when player 1 runs, or shows before a scene ends so B skips it | `0x800460EC` asks about "the player", and with no hold about their moves. `r13-24048` wasn't it. Leaving the asking out showed the pick up during the circle's scene. The choosing runs as in a close-up: the hold's channel on, player 2 the player | `coop_prompt_choose` |
| Player 1's half looks at player 2 when they walk to the bookshelf in player 2's close-up | Their camera records can name player 2, set with player 2 the player. They're put back as their turn steps them. Lending the camera code's other words too (`r13-21704` and so on) put their view behind the roof: don't | `coop_player1_camera` |
| Neither player moves after player 2's spell page closes, with the game still drawing | The script asks for RETURN under a hold with player 2 the player but no use up, so it wasn't taken for their close-up, and the bit player 1's turn kept stayed on (`logs/run-*-s2-run2`, frame 10789) | `coop_look_prompt`, `coop_close_up_prompt` |
| The find's words doubled, the item in both halves, when player 1 walks up to what player 2 is finding | Player 1's zone ran the script from the word copied as player 2 found it, and found it again (`logs/run-20261103-173000-shelf2`, frame 10383). Left to the finder in their find, whichever player that is, as is a press on it (`logs/run-20261103-181117-p1cam`, frame 11116), and player 2's last word goes across as their close-up ends | `coop_run_script`, `coop_close_up_prompt` |
| Player 2 can't move while player 1 looks at something with no words up yet (the circle's scene and DETAIL) | The hold's only let go at words or a find (`coop_no_pause`), and player 2's turn is skipped under any hold. A hold from a script player 1 pressed on is their look: everyone's told nobody's busy, the hold's channels stay on so player 1 stands, and player 2's turn runs with them let go. Their counts go down each frame, so each is put back (`logs/run-20261103-175913-p1full`). Player 1's own stick still ends the look, as it does without this | `coop_p1_close_up`, `coop_play_hold`, `coop_busy_ask`, `hle_coop_controller` |
| Both halves show player 1's close-up, player 2's camera gone, after player 2 walks up to something | Their EXAMINE, asked for in their turn (`0x8000A424`), was taken for their close-up opening: for 120 pad reads any hold of player 1's became player 2's (`logs/run-20261103-180133-p1look`, frame 15722) | `coop_look_prompt`, `closeup` in `coop_menu_input` |
| Player 1's Tome page in player 2's half, and player 2's close-up in player 1's | Player 2's press in player 1's find waited, then started on the frame the find ended, as player 1's Tome was on its way (`logs/run-20261103-181505-both`, frame 10703). A press while player 1's look or find is up, or their Tome is coming, does nothing now | `coop_player1_mid_use` |
| Player 1's Start just after player 2's pickup opens a Tome in player 2's half, with player 2 stuck in it | What's marked as player 2's (`g_coop_assist_at`) lasts 120 pad reads after their page's RETURN, and took the Tome player 1 opened (`logs/run-20261103-182310-s1-room`, frame 10278). Player 1's Start clears it | `coop_menu_input` |
| The other player vanishes in a scene from a press, and neither walks | The game's show flags (`fn111`, a look's flag 4). They hide player 1's body whoever asked, and stop the controller both players' turns run from | `coop_show_asked` |
| The other player stops in a scene, with the show flags kept out | Busy is 34 in a scene, and the hold's let go only at exactly 2 | `coop_p1_close_up`, `coop_hold_open` |
| An icon half off its half during a scene | The scene's bars count as the HUD's middle, so side elements stay in | `widescreen_hud_shift` in `gxcore_draw.cpp` |
| Both players in attack position | Seen in one build, which called `0x801223B8` to clear an object's mark. Taken out and not traced. Don't call it for this. The players not being each other's threat is `coop_send_event` | `docs/coop-failed-attempts.md` |

## Don't

- **Don't skip the close-up.** Its opening and closing give the item. Skipped, the rune was never given and could be found again and again, its notice repeating. The user rejected self-closing too.
- **Don't hold the other half on a still picture.** It only looked right when the whole world was paused (`d0b1fdc`).
- **Don't widen a hold check to an address range.** `0x800429D0`..`0x80042C10` takes in other holds, and Paul's cutscenes stopped going to the right place. Which part did it wasn't proven.
- **Don't treat the two players' mechanics as different.** A fix for one is the fix for the other. Start by asking what the other player's turn has that this one lacks.

**The two Tomes stay separate.** Anything up when player 2's Tome opens stays in the real game. Fix what the copy takes, inside the copy. Never change the real game's scripts or text for it.

## How to test

Scripts have missed these bugs, so test the way the user plays.

- **Start the other player from standing still.** One already walking coasts on and looks fine.
- **Run the sequence past its last step,** into the spell page's help and out of it.
- **Run both players.** `build/runs/enchant/both.sh NAME EXE` replays player 1's urn with player 2 walking. `P1SCRIPT=... sh build/runs/enchant/p2urn.sh NAME` replays player 2's with player 1 walking. Both are gitignored. `[trace] no pause` and `[trace] player 1's turn` log positions.
- **Press Start during the other player's text and finds.** `P1CUT=frame sh build/runs/enchant/p1start.sh NAME P2SCRIPT` replays player 1's urn with player 2's presses. Live controllers mix into scripted runs, so check the pad log for presses not in the script.
- **Then have the user play it.** Believe what they see over the log.

## Limits

- Notices read the game's English words (`has picked up`, `has found`, `has acquired`, or anything with `has`). Words without `has` get no notice, and a log line says so.
- Spell pages aren't traced, so a spell found may not give a notice.
- Player 1's Start in player 2's text, finds or close-up opens their Tome in a copy of memory, as player 2's runs, with the real game carrying on (`g_tome_copy_p1`). Their item or cast from it waits for the close-up to end. Player 2's own Start waits for their own close-up, and for player 1's copy to close.
- In that copy, player 1's Tome has player 2's tabs: no Journal or Options.
- **`OSCheckActiveThreads` failing with a thread at `0x80325188` holding a bad priority, found 2026-11-04:** a stack overflow, not the number of threads. The disc and reset monitor is a thread of its own, made at `0x800AB91C`, at `0x80325188`, with a 4 KB stack right after its record. It reads the pad too (`0x800AB344`, `0x800AB4CC`, `0x800AB7FC`). `hle_PADRead` did co-op's work on whichever thread read first, and laying out a prompt's words goes deep: over that stack and over its record. Then `OSCheckActiveThreads` stopped the game on the bad priority, which is the frames' bytes. The main thread's wait at `0x800085E8` was only it waiting. `pad_read_by_monitor` leaves co-op's work for the game's own read, and logs `[pad]` the first time. Before the fix it came in 15 runs from 2026-11-03 on. After it, 40 runs of a character reload passed, and the slot 4 tour through rooms 186 and 71 that froze on all 3 runs went on.
- Only slot 1's urn and Paul's podium have been played.
- Player 1's bars are likely hidden during player 2's close-up. Their icon isn't: `coop_p1_icon_show`.
