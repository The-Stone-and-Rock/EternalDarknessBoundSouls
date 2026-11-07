# Words: the one window

Every message goes through `0x80024310(words, script, 0)`, and the script runs on when it closes.

| Script call | Text from | Game function |
|---|---|---|
| `ed11(group, entry, script)` | `UBootPak.bin`, all chapters | `0x8000FF20`, `0x801DB374`, `0x80024310` |
| `ed12(room group, entry, script)` | The chapter's `URmTxtNN.cmp` | `0x8001000C` |

A window holds play through `0x80042A8C`, takes the input focus (`r13-23552`, `-23556`) and sits in `r13-25960`.

**NPC talk is a third pack,** `ed97`, `r13-26480`, 63 calls, not read by the kit: `ed text` can't find those words.

**Any words:** `ed text WORDS` gives the entry and who opens them. `ed text While fumbling` is the urn: script 1429.

**For two players:** `coop_words_hold`, `hle_message_open`, `coop_player1_turn` (the focus), `tome_copy_no_window`. The rule: they show to whoever opened them, in their half, and the other keeps playing.

**The input focus (2026-11-06, round 3).**
- `0x8013DB1C` gives the focus (r13-23556) to the first window that asks.
- The pad update `0x8013E518` copies it to r13-23552 at its end (`0x8013E808`).
- `0x8013E1A0` passes over every input channel that isn't the focus, so the player gets event 45 (standing) whatever the stick does.
- A window's close, `0x8013DB94`, sets window+28 bit 1 and clears -23556 only if it still names that window, and only in the memory at hand.

So a saved copy of the pad words (player 2's) kept player 1's chapter-start words' focus for good. `coop_player_turn` now lets the focus go for player 2's turn, as `coop_player1_turn` does for player 1's.

## Long words close only on their last page (round 5, 2026-11-06)

Words longer than one box don't close with B until the stick scrolls to the end: the B icon shows only on the last page. The same in solo (DETAILED_BOOKS 47,1 in ch10 room 237, logs/run-20261106-085106-jtg-pr-b3-solo) and for each player in co-op.

## Words opened from an event to the player (round 6, 2026-11-06)

- A find's words opened from an event to the player's node (event 71; ch5 DESK find 2246, no close-up camera) register their input channel (handler `0x80024434`, mask 3, in the first free channel slot, COOP_INPUT+336) and set the focus r13-23556 to the channel's address, in whatever input is in at that moment.
- Finds with a close-up camera (ch1 ChatBlockCam) open their words from the close-up stepper in the world's frame instead.
- `0x800443E4` (a move's input lock) writes r13-26232 (`0x800068E0`) and r13-23592 (`0x8013E99C`).
