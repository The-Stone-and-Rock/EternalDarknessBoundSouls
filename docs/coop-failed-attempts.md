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

**Judging "player 2's own" by `g_coop_use` alone.** An item from player 2's Tome runs with no use up (`tome_copy_left`), so `coop_no_pause` took their words for player 1's and let go of the hold: the words showed in player 1's half and the game froze (`logs/run-20261103-142333-before`, `who 70`). It's in commit `c8f69bc`; fixed after it by also asking whether they are the player (`r13-21492`). `build/runs/enchant/p2urn.sh` replays player 2's urn.

## Player 1 free during player 2's words and finds

Confirmed in the user's play for slot 1's urn. See `docs/coop-no-pause.md`. No hold on play for their words and finds, the busy bit kept for this host's asking; `0x80155738` answers 0 while `g_coop_screen`; `coop_player1_turn` mirrors player 2's turn; player 1's half follows them from their last camera.

**What stopped player 1 walking with their stick read:** the input focus (`r13-23552`, `-23556`). The window takes it, so the controller's channel (`r13-25648`) is passed over and it sends event 45 (standing), not 44. Player 2's own saved copy of those words kept them free. Player 1's turn clears them, and `coop_state` puts the game's back.

**Doesn't help, and likely gave them an attack stance:** clearing bit 4 of the object's marks through `0x801223B8` around the controller. The user saw both players in attack position in that build only. `0x801223B8` is a large function with more to it than the bit. Not traced.

Seen by script: `logs/run-20261103-145338-live7` (player 2's urn, player 1 walking), `logs/run-*-regress1` (player 1's urn). `build/runs/enchant/p2urn.sh` replays player 2's urn; `P1SCRIPT` walks player 1.

## Player 2 frozen during player 1's look at the circle (2026-11-03)

**Answering "nobody's busy" only inside player 2's turn, and opening the input masks there.** Their turn ran and read their stick (`[coop] player 2's pad`), and they didn't move: a character's step asks `0x80155738` outside the turn too (`logs/run-20261103-175329-p1look8`).

**Taking a hold's channel at +16 being 0 as the hold being over.** It's still 0 the frame after the hold starts, then counts down from about `0x0FFFFFFF`, so the look ended itself on its first frame. Only the busy word says whether it's over.

**Reading the busy word in `coop_p1_close_up` during player 2's turn.** The turn lets the hold go, so the word reads 0 and the look ended itself. `g_coop_p1_look_open` covers the turn.

## Player 1's Tome in a copy, in player 2's close-up (2026-11-03)

**Their world reads marked as the Tome's.** `camera_set_menu` then told the camera a menu was open, and the user saw player 2's camera bounce. Not measured: the code was changed from what the user saw, and the jump log came after.

**Reading player 1's pad twice a frame, world then Tome, to keep the camera out of it.** Each change marks everything held as held over (`controls.cpp`, `g_tome_was`, `g_held_over`), so no press got through and their Tome couldn't be closed (`logs/run-*-s1-both`). `ED_INPUT_SCRIPT` skips that code, so the replays closed it fine: **scripted presses for player 1 can't test `controls.cpp`.**

## Door arrival for player 2

**Sending event 238 to player 2 on arrival, as the player** (`2fa5936`, in `coop_follow`, with current, its node and the model all theirs for the call). They stopped where they were, twisted, in mid motion. `862e14d` removed it, and sets state 80 on them instead, so the motion they were in runs on.

```c
const u32 event[4] = {COOP_ARRIVED_EVENT /* 238 */, id, id, 0u};
hle_guest_call(cpu, COOP_SEND_EVENT, event, 4, 0.0, &ignore);
```

What it left out, found since (`docs/game/systems/doors.md`, `Arrival`): the game reads this event's answer, the motion to play, and then sends event 191 with it. `hle_guest_call` returns only `r3`, and the answer is in `r4`.

## Player 2's camera lost when player 1 leaves the room (2026-11-04)

Not a failed attempt: the cause, kept here so it isn't chased again. Player 2's turn runs before `coop_follow` brings them, so their first look in the new room used the old room's numbers. In `logs/run-20261104-011724-p2cam-leave` those fell in room 150's `CameraRgn07` and `MuffledVoicesRgn01` (scripts 1067 and 933, `who 70`, frame 11535), and at their place on the stairs no region sets another camera. It depends on where they stood in the old room, so it can look fixed and come back. Fix: `coop_left_behind` skips their look and camera step until they're brought, and `coop_look_line` starts fresh on arrival. Scenario `p2cam-door`. Script-tested only until the user confirms it in play.

## The bookshelf's lever (2026-11-04)

Anthony's chapter, room 147, script 1794. Player 2's Yes hung the game for both. Five things were wrong, each hiding the next. Reference: `lever-p1` and a solo run of `saves/lever.state` (`logs/run-20261104-021859-solo-ref`).

| What | Cause | Fix |
|---|---|---|
| The hang | The shelf calls the script back when it's done (event 6, `0x801F7748`), with player 1 the player and their word in. Player 2's word, at 100, the one that lets go, never ran | `coop_script_sends` notes who sent the thing; `coop_run_script` runs the call back for them |
| Player 1 looked at the shelf with no press, and was held | A scene turns the zones off, and a look with them off clears the looker's inside words (`0x80134BE8`). Alone the controller doesn't run in a scene; player 1's does in player 2's close-up. Every zone then fired for them at its end | `coop_scan_player1` passes that look over |
| The scene never showed in player 2's half | Its script's hold ended their close-up as it began | `closeup` in `coop_menu_input`: a script's hold doesn't begin one, but one begun stays theirs |
| Player 1's EXAMINE gone after any close-up of player 2's | The game's use of a press clears what's on show (`0x80006FD8`), with player 1 still the player, so it took player 1's prompt, and zones ask only on the way in | `coop_prompt_clear` goes to player 1's only in their own turn. Player 2's stale DETAIL over their words went with it |
| Player 1's icon still not seen | The game's element stayed at player 2's place: `coop_close_up_prompt` puts it back but wasn't reached as a close-up ends | Called at "play goes on" |

**What didn't work:**

- **Player 1's icon during player 2's close-up, using player 2's own element at player 1's place.** It showed in player 2's half: with the split's owner set, only their view draws the HUD. What worked: an element of player 1's own at `COOP_PROMPT_P1_AWAY`, which only player 1's view brings back (`coop_p1_icon_show`, `farRight` in `build_view`).
- **Doing fn111 (`player_on`) to whoever asked, for Pius vanishing in the scene.** Run as player 2 at the frame's end (`0x80043CCC`, `0x80043C94`), Pius still vanished. It was the right function and the wrong fix: see below. Don't apply fn111 for the other player. Keep it out of the words altogether.
- **Telling the user that walking during the scene and Pius vanishing were the game's limits.** They weren't, and the no-pause work already had the answer. Read what the words gate before calling something a limit.

**Scripts:** the Yes/No is below the first page: scroll with the stick, or A does nothing. The same goes for the Tome's tutorial words. A press of player 2's during player 1's scene waits, and their close-up opens as it ends.

**Not tested:** the monk's bell (it needs Paul in chapter 7, and no save has it), which is the likeliest other scene to feel `closeup` staying theirs through a hold. A state saved mid-scene: `g_coop_sent_script` isn't in save states, so one loaded there would hang as before.

## The other player vanished and stopped during a scene (2026-11-04)

The one cause for both: the game's show flags. `fn111(0)` (and a look's flag 4, `0x80043C88`) hide "the player" so a scene's camera sees past them. They're one set of words, read as "the player" at that moment:

| Word | Reads it | What it did in co-op |
|---|---|---|
| `r13-26276` | `0x8002794C`: no draw if it's 0 and the object is `r13-26524` | The draw runs outside anyone's turn, so that's always player 1's body: Pius vanished whoever pressed |
| `r13-26272` | `0x80007460`: main's controller doesn't run if it's 0 | Player 1's controller is where player 2's turn and player 1's turn both run (`hle_coop_controller`): nobody walked |

What worked, `coop_show_asked`: asked for in a press of a player's, neither word is touched. The one who pressed goes from their own view alone, with what they hold, through the found item's marks (`g_coop_unseen`, `coop_found_mark`). Scenes with no press still go to the game.

Then each hold had to let the other player go:

| What | Cause | Fix |
|---|---|---|
| Player 2 stopped in player 1's scene | `coop_p1_close_up` asked for busy to be exactly 2, and a scene makes it 34, so the look ended as the scene began | `held` allows bit 32 |
| Player 1 stopped in player 2's scene | `coop_script_hold_go` refuses anything but bit 2, so nothing was let go | `coop_hold_open` and `coop_hold_shut`, the code player 2's turn already had in player 1's look, now shared |
| Player 2's icon made and freed every frame over their own scene | The pad's read in player 1's turn saw busy 0 | That turn puts the busy word back after opening the hold. The game is still told nobody's busy (`coop_busy_ask`) |
| Player 1's EXAMINE stayed after they walked off during player 2's scene | `coop_scan_player1` passed over their zones with zones off, which also stopped them leaving one. Passing over had been the fix for the zones firing again | Their scan runs with zones on and puts the word back after |
| Player 1 had no icon on walking back to the shelf during player 2's scene, or after it till they'd left its zone | `coop_run_script` left the zone's script to player 2's close-up. That's for a find, whose word was copied to them | Only when their own word for it has begun |
| The other player's icon half off their half during a scene | The scene's bars cross the middle of the HUD, so `widescreen_hud_shift` left that band's side elements where they were | What spans a split's whole frame doesn't count as being in the middle (`gxcore_draw.cpp`). The bars' own check first looked for thin and untextured, and didn't catch them |

`cutscene_playing` is false in this scene (`[trace] player 1's look over`: cutscene 0), though it shows bars. Which of its two asks says no wasn't traced. `coop_show_asked` stands aside whenever it's true.

Scenarios: `lever-scene-p1-walks`, `lever-scene-p2-walks`. Test from standing, in the direction away from the shelf: walking into the shelf stops them and looks like this bug.
