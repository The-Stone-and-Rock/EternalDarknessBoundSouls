# Examine: one family of game functions, for the whole game

**For a normal player:** walk into a region, its script asks for the EXAMINE prompt, B opens a close-up with its own camera, DETAIL, words show, RETURN or PICK UP.

`ed examine ROOM|TEXT` lists all 624 calls (each copy of a script in a level counts) with words, camera and what each offers. `ed how bronze necklace`. Evidence: `docs/game/research/verbs.md`, `arguments.md`.

| Call | Record `+0` | What the player gets | Calls |
|---|---|---|---|
| `ed220(group, entry)` | 1 | Words, no close-up | 98 |
| `ed219(group, entry, camera hash, n)` | 2 | Close-up, DETAIL, words, RETURN | 402 |
| `ed221(group, entry, camera hash, n, tag, entry once taken)` | 3, or 2 or 1 once taken | Close-up, DETAIL, words, **PICK UP** | 124 |

Group and entry are in the level's room text.

**`n` is bits:** 1 blends into the close-up, or it cuts. 2 (`ed221` only) falls back to words only. 4 calls `0x80043C88(0)` on opening and `(1)` on closing. That runs later in the frame (`0x80043CCC`, from `0x80008214`), on **main**'s list, and sets `r13-26272`, which gates the controller at `0x80007460`. In co-op it's kept out of a press: `coop_show_asked`. The game changes `n` itself if `0x801F0638` or `0x801F077C` answers (kind 3 tests only the first), and `0x800255A4` answering clears bit 1.

**`ed221`'s tag** is the placed thing's: `0x801F6160(tag, r13-23276)`, the same as `fn78(2, tag)`. Script 1263 in level 2: `ed221(69, 3, Camera09, 0, 2029, -1)` is type 21, Bronze Necklace, room 217. If the thing is gone or carried it uses the last argument as the entry, and **if that's negative (116 of 124 calls) it stops asking**, so EXAMINE on it is gone for good.

## How it runs

| Step | Game function | Notes |
|---|---|---|
| The region's enter script runs | `0x80134A44` scan, `0x80134758` | `ed room N`. Another player's model reads other fields: `ed system players` |
| Gates | `0x8006DB04(0x4000)` must be off, all three. `ed221` also needs the actor's `+158`, `+159` to be 1 | |
| The record | `0x800170D8`, `0x800172C8`, `0x800174A0`, into `0x803090C4` | `+0` kind, `+4` script, `+8` step, `+12` group, `+16` entry, `+20` camera, `+24` n, `+28` handle |
| The ask | `0x80117404(2, 6, -2, script, 50, 2, 0)` | Returns the slot. Slot 2 copies to look 0 at `0x80307108`, slot 4 to look 1 at `0x80307128` |
| B | `0x80006EBC` | Script -2 goes to `0x80024D78`. `0x8000704C` is the return point inside it, and also a branch target, not a function |
| Each press | `0x80024D78`, counting `+8` | Table below |

| Press | Kind 1 | Kind 2 | Kind 3 |
|---|---|---|---|
| 0 | Words, done | Hold, camera 10 pushed, asks DETAIL 34. Entry below 0: RETURN, jump to 3 | The same, always DETAIL |
| 1 | | Words, `0x80024310(element, -2, 0)` | Same |
| 2 | | Asks RETURN 7 | Asks **PICK UP 19** |
| 3 | | Close, pop camera, run the script again | `0x801F9DE8(thing, player, 30)`, the find |
| 4 | | | Close, run the script again |

**Slots 2 and 4** are zones'. A slot 2 ask moves to 4 if the player is inside a sphere or box whose `+110` is 90 or more, and whose enter or stay script is this zone script, or whose stay script is the running script. A press, leaving the zone or opening an interface clears them. Leaving clears the script's word unless it's marked (`fn35`), so coming back asks again.

**For two players:** `ed needs 0x80017604`. Host: `coop_prompt_look` presses player 2's EXAMINE, DETAIL, RETURN and PICK UP inside their look with their own set in (`g_coop_p2_look`, `g_coop_p2_words`, `g_coop_p2_find`: the look record `0x803090C4`, the find's record and bindings and the window's words are in that set, the looks `0x80307108` their own copy `g_coop_p2_looks`, their camera set 2); `coop_words_tick` steps their window. `coop_use_start` for a script of theirs run as a use (a door, a zone's use prompt, a Tome item), which opens the game's own window and close-up with the cameras lent (`coop_cameras_lend`, `g_coop_screen`). `coop_scan_player1`, `coop_look_prompt`, `coop_close_up_prompt`. `coop_player_turn` moves current and the model, not main, so `ed221`'s gates read player 1 during player 2's look. Symptoms: `docs/coop-no-pause.md`.
