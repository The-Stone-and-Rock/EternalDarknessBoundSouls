# Co-op: what was tried and didn't work

Check here before writing a fix, and don't bring these back. Add an entry whenever an attempt fails: the symptom, the code, what happened and what worked instead. If a fix was never tried in play, say so.

## Don't call this while logging

**`0x801F59B0(0xAF, id, id, 0)` to see if a cast is allowed.** It looks like a question, but it changes the character: they went into state 64, and every later cast was refused.

```c
const u32 ask[4] = {0xAFu, id, id, 0u};
hle_guest_call(cpu, 0x801F59B0u, ask, 4, 0.0, &asked);
```

Logging should only read memory. Calling a game function from a log can change the game. (Run `logs/run-20261102-191307-enchant-order` is invalid because of this.)

## Player 2's close-up froze the picture

**Treating every menu as a Tome that keeps its views back.** From `bac33e8`, in `aurora.cpp`:

```cpp
s_keepOnly = split_menu_player() >= 0;
```

Nothing new reached the window during player 2's close-up. The game ran on, and no hang was reported, because the watchdog counts game frames and not pictures. Fixed in `2e00b69`:

```cpp
s_keepOnly = split_menu_player() >= 0 && (menuFrame || worldFrame);
```

## "Paul cannot use that now"

**Making player 2 only the player and the controlled character.** The message stayed, with `who 103 ctl 103` in the log (`logs/run-20261102-194324-use-p2-fix2`):

```c
coop_be_player2(cpu, true, was);
hle_guest_call(cpu, COOP_RUN_SCRIPT, args, 3, 0.0, &ignore);
coop_be_player2(cpu, false, was);
```

The script's check, `0x8016A7DC`, reads the zones' "inside" words, and player 1's were in. It needed `coop_zones_swap(cpu, info, true)` around the call as well (`066076c`).

## The clock freeze for player 2

**Finding a window's close only by the return address on the stack.** That fixed player 1, but player 2's script is started by `coop_use_start`, so the address isn't on their stack, and the window was still passed over (`logs/run-20261102-202844-p2-clock`). It also has to check `g_coop_use.resume` (`3cd22cc`).

## Anthony's first Tome room scene showed player 1 when player 2 triggered it

**Running the scene's script as player 2.** Script 1351 ran with `who 76 ctl 76`, and the picture still showed Anthony (`logs/cap-slot2-scene-p2`). The camera is aimed at the room's arrival spot, so it shows whoever stands there. What worked: player 2 takes the arrival spot, and player 1 goes beside them, in `coop_follow`.

**Treating the move as a door, or comparing the scene with slot 3.** Slot 3 is Karim's chapter, with no first-entry scene. Compare frames with a solo run of the same save instead.

## Two game Tomes at once

Set aside. Don't retry this by swapping more memory ranges. Details are in `docs/coop-two-tomes.md`.

| Where | What |
|---|---|
| Branch `two-tomes-wip` (`618d29f`) | The attempt, for reference and not for merging |
| Branch `tome-world-wip` (`c5c266a`) | An earlier version of the world behind the Tome |
| `logs/saved-changes/two-tomes-*` | Builds and patches of each step. Only on this machine, not in git |

## Player 2's Tome in a copy of memory (test 2)

Details are in `docs/coop-two-tomes-design.md`.

**Closing by checking only the mode asked for.** It hung on close at `0x80110DC4`: Start sets `r13-24244` to 2, and the next frame ran the game's way out. Checking `r13-24244` before the frame fixed it (run `logs/run-20261102-221819-t2a`):

```c
leaving = mem_read32(cpu, sda - 26188u) != 3u;
```

**Leaving disc callbacks to the main loop.** The page turn spun forever at `0x80103C54`, and the callback would have run in real memory (`logs/run-20261102-222253-t2c`).

**Matching tab labels by a `.` before the word.** The `.` in `[trace] words` lines is how the log prints a control byte. It isn't in the text.

**Running a used item's script in the real game with the real spell record.** An item's script takes its item from `0x80335748` word 0 (`0x80015C30`, by `0x80110478`). The Tome's cursor sets that word in the copy, but the real game's still held the last cast's item. In `logs/run-20261103-093522-hand9` that was the Tulwar (item 9, count 0), so the Talisman said "drained" after two uses of five. With the Chakrams there (count 19) the script read that count instead. What worked: set word 0 from the copy for both steps, and put it back after (`logs/run-20261103-095904-r8`).

**Always running the script's second step.** When the first step opened words, the second opened them again on the same frame:

```c
hle_guest_call(cpu, COOP_RUN_SCRIPT, asked, 3, 0.0, &ignore);
hle_guest_call(cpu, COOP_RUN_SCRIPT, args, 3, 0.0, &ignore);
```

Read in the code, not traced: `0x80024310` stores each new menu in `r13-25960`, while `0x8013DB1C` only gives the game's one focus (`r13-23556`) to the first. `0x80024220` then closes the second and clears `r13-25960`, so later presses find nothing to close and the first keeps the focus. Player 1's 21 B presses did nothing. The second step now runs only if `r13-25960` is 0. No test has reached this guard since the first fix, so it hasn't run.

## The freeze with both players as Paul

**Passing over the empty animation, and nothing else.** It stopped the freeze, but player 1 couldn't walk: every move had no set, 587 in one run (`logs/run-20261103-104740-s7walk2`).

```c
const bool chosen = player_model() >= 0;
if (!chosen && (!g_settings.mod_coop || g_coop_p2 == 0u))
    return;
```

**Calling the game's moves call to repair player 1.** Their part was already in the game's slot, so `0x80132970` gave them the table at `0x80582010` as it was, and that was empty, because their first ask had gone to a slot of ours (same character as player 2). What worked: `moves_repair` makes the table from the sets when it's empty (`logs/run-20261103-104830-s7walk3`: 21 sets, no empty moves, player 1 walked).

**Calling it an animation with no data.** A solo trace (`logs/run-20261103-104327-paul-solo`, the user's idea) showed `0x80516F70` is the player's own record, with a set in solo. It's filled from the table for each move, so an empty table made it look empty.

## Older attempts

Older failures are mostly recorded in code comments in `runtime/host/hle.c`, as what the code used to do and what went wrong, and in commit messages. Search both for the function or address before changing it.

## Player 2's item whose script opens words

**Leaving the script's word in the game's table.** In `tome_copy_left` both steps ran in the game's table. Closing the words then ran the script for player 2 from their own table (`coop_use_start`), where it had no word, so it began again from its start. It asked for the Tome again and waited for ever, with the words gone and no rune (`logs/run-20261103-122356-s1-p2`). What worked: move the word to their table when the words open.

**Setting `r13-24928` and `r13-24924` back for the invisible players.** They differed in the invisible save, so I took them for the show flags, without checking. They aren't: the setter at `0x80043BE8` gives `r13-26276` and `r13-26272`, which were 1 all along. Nothing changed. What worked: `r13-23456`, found by patching a copy of the save and looking.

## A thing found by player 1 going to player 2

**Copying the found script's whole word to the other player's table.** Keeping both tables alike is right ("what is found is gone for both"), but the word has the script's timer at +4. With the urn, player 1's Broken Blue Urn left a timer for the rune, the copy ran out in `coop_script_timers`, and the rune, its close-up and the Tome's page all went to player 2 (`logs/run-20261103-*-s1-p1d`, frame 7457). What worked: the copy leaves the timer out.

## Keeping the other player moving during a find

**Passing over the play hold for words and finds (`0x800429D0`..`0x80042C10`), and running player 2's turn from the busy check.** Player 2 only crept a few units during the urn and rune, and soon after the user found Paul's chapter's cutscenes didn't take them to the right place. Which of these did it, if either, isn't known. All of it was taken out; the code and that build are in `logs/saved-changes/hold-experiment-removed/`. The user is making a save where the other player keeps moving, to compare with.

## Showing a found item to one player

**Skipping the close-up (the user did not ask for this, and rejected it).** Passing over `ACQUIRE` left the item ungiven: the rune never went in and could be found again and again, with its notice repeated (`logs/run-20261103-140848-find1`). Its opening and closing are what give the thing. Closing it by itself in a frame (bit 0 of `r8`) gave the thing but took the close-up away, which they also rejected. They want the item shown to its finder, once.

**Holding the other half (`g_coop_p1_cast_shown`).** Keeps the item out, but with no pause the half is visibly frozen.

**What works:** `coop_found_for_finder` and `coop_found_mark` mark the item's matrices as it is drawn (`0x8011A278`), and `build_view` (`gfx/interp.cpp`, `marked_out_of`) leaves them out of the other view. Seen in pictures for player 1's urn and rune (`logs/run-20261103-141926-one1`).
