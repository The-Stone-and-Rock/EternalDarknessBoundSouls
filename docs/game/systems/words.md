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
