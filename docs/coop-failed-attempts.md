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

## Character shadows (2026-11-04)

How the game does it, from `AURORA_DRAW_DUMP=N`: each character's mesh, flattened on the floor, is drawn twice with colour off and alpha on. One full-screen quad at the end (src InvDstAlpha 7, dst DstAlpha 6) darkens by that alpha. Characters are skinned on the CPU, so `has_pos_mtx_idx` is 0 on them. What worked is in `gfx/shadow.hpp`.

**What didn't work, finding who casts:**

- **`has_pos_mtx_idx`, as skinned draws.** Nothing has it.
- **Every lit draw.** Slot 7's room is lit too. Only some rooms are, so the bookshelf room hid this.
- **The lit draws up to the next flattened draw.** A character's solid draws come far earlier, with the room's. Only their translucent parts are just before the flattened ones.
- **The unbroken run of nearby draws before the flattened one.** The same, and it gave a tile of minimum size.
- What worked: the lit draw just before a flattened draw has that character's own lights, and every lit draw with exactly those lights is them.

**What didn't work, choosing the light:**

- **The strongest by the game's attenuation.** It's the slow fill light past the camera (`distatt` z near 0.04), so shadows fell behind the characters where nobody sees them.
- **The strongest room lamp (`distatt` z 0.1 or more), with no other test.** In slot 1 it was below the floor: the characters went black, with a dark disc round them. A light has to be at least a little above them, using where they stand to their middle as up.

**Also:** a place past the tile's depth range compared with the atlas's cleared 1 as blocked, which darkened the whole room. A contact shadow cut off at a set distance drew a hard disc on the floor: it fades now.

**Comparing:** the lanterns flicker, so brightness can't be compared between two runs. Use `AURORA_SHADOW_SHOW=1`.

**The hang seen in these runs isn't from this:** `hang-20261104-093201` is the `0x80325188` bad priority thread after the character reload, in `docs/coop-no-pause.md`'s limits. It hit two of the runs on slot 7, one on a build before these shadows' casters, and it wasn't reproduced after.

**Later the same day:**

- **Looking along the depth buffer for furniture between a shadow and its caster, to stop one going through a bench onto the wall.** It broke long floor shadows into speckles, as the floor itself read as in the way at a low angle, and it didn't remove the faint shadow past the bench. Taken out. A shorter fade (`along` in `tile_shadow`) hides that tail. The room still isn't a caster, so a shadow can pass through furniture.
- **Contact shadows from anything in the depth buffer.** The room's corners made a round patch where the effect fades. Only what's in the character's tile counts now (`is_caster`).
- **Easing the light's direction through a camera cut.** It's in view space, so a shadow would swing from the wrong way for a dozen frames. It snaps if the camera turned (`g_sceneCameraCut`). No cut has been watched in a run yet.

## Spells, skin and lit air (2026-11-04)

To cast in a test: `ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_PAD2_SCRIPT="23700,23712,up"` on slot 7. The script's button is `up`, not `dup`, and a press too soon after a load does nothing. It takes a few seconds: the column and ring have gone by frame 23950, so look at several frames.

A spell is: flat sparks (a 63x63 sheet, no normals), one mesh with normals that leaves no depth (146 vertices), the rune and ring, and a real light on the room. So a rim shader can only go on the mesh. The sparks can only glow.

**What didn't work:**

- **The mesh in the glow pass, with a strong rim.** The column blew out to solid white, and its wisps were lost. Only the sparks glow now, and the rim is slight.
- **Skin scattering on every lit draw.** Lit rooms took it too: the desk's paper went orange. A character has several lights and a lit room surface one, so it needs three or more (`plan.cpp`). The colour test is still a guess at what's skin: nothing says what a surface is.
- **Volumetrics at 0.0016 a unit, radius 220.** The whole room went yellow.

## Shadows and light in play: what the user saw (2026-11-04)

Karim's chapter, the Tome room, `logs/run-20261104-104226-kariem-live`. They saw shadows that changed with the camera, showed only when running, and lights flickering. All were from these effects, and none from the game.

**What each character's eight lights are** (`AURORA_SHADOW_LOG=1` prints them, `[shadow-lights]`):

| Slots | What | How to tell |
|---|---|---|
| 0 to 2 | Fills, on a ring about 1,000 units out, about level with them | `distatt` z 0.0385 |
| 3 | A fill about 600 units out towards the camera, which goes with it. Its view-space place follows the camera | `distatt` z 0.0385 |
| 4 | Far off, about 1,000,000 units | `distatt` 128, 0, -127 |
| 5 to 7 | The room's real lamps, nearest first, so **they change slots as a character moves** | `distatt` z 0.8 |

A lamp can be chest height or in the floor: the Tome room's glowing circle has one at the knees.

| What they saw | Cause | Fix |
|---|---|---|
| Shadows only when running | "A light has to be above them" (upness 0.3) rejected nearly every lamp, so standing fell back to straight down | Only that it's above the floor and not inside them (`casting_lamp`) |
| Shadows depending on the camera | With no lamp through that test it fell back to the fills, slot 3 among them | Lamps only. With none, from straight above them |
| Shadows not drawing properly | Cast as if from a far-off light along one direction, when the lamps are close | In perspective from the lamp (`tile_rows`) |
| Lights flickering, in both halves at once | The haze's lamps were the characters' nearest three, so a lamp went fully on and off as anyone walked | Each lamp is kept in the world once seen, and fades (`g_lamps`) |
| Black characters | Their own shadow on the side already turned from the lamp, then another player's shadow on their face | On a character it's a light shade (`body` in `shade`) |
| A knee gone white | A lamp almost touching it: per pixel its fall-off climbs without end | Capped when per pixel (`emit_light`) |
| A flame washed to white | The haze added over what's bright | It screens over it, and tends to a limit |

**Comparing:** measure flicker on `shots.csv`'s left and right columns. Both jumping at once with one player standing is something global. Standing still showed nothing in any setting: it needs movement. `saves/karim-start.state` is a copy from just after Jump to Game's load-in, for `ed test shadow-karim`.

**Shader names:** `cast` and `self` are reserved in WGSL, as many ordinary words are. Each crashed the game on load, whenever shadows were on, and only showed in a run with them on.

**Still not done:** nothing bounces. There's no light passing from surface to surface: only the game's own baked light, these lamps, and the haze. The room isn't in the atlas, so a shadow can go through furniture.

## Shadows that followed player 1 (2026-11-04)

The user, on `saves/slot10.state` (Karim, room 338): player 2's shadow disappears or misbehaves out of player 1's view, and shadows jump between lamps. Recorded in `logs/run-20261104-112543-roomwalk`. Player 2 stood still at (-677,-625) while their shadow changed direction and vanished as player 1 walked.

`AURORA_SHADOW_LOG=30` logs each frame's flattened draws and lots of lights (`[shadow-set]`). Test: `ED_INPUT_SCRIPT="14750,15700,0x0000,0,127,0" ED_RUN_FRAMES=16000 sh tools/ed/run.sh saves/slot10.state NAME`, which walks player 1 away with player 2 still.

| Tried | What happened | Why |
|---|---|---|
| The lights of the last lit draw before a flattened draw say who it is | The corpse's flat took player 2's lights, and two characters became one. A flat could also take the room's | The order isn't what it looked like: not everyone has translucent parts just before their flat |
| One lot of lights is one character | Player 2's shadow vanished with them standing still: only their translucent parts cast | A body and its translucent parts can have different lights |
| Any lit thing the size of a person casts too, as flats might go missing out of view | It looked right for a while, and it was covering the fault above | In room 338 flats didn't go missing: both players had one in all 49 samples, in view or not. Not checked in other rooms, or for enemies |
| The lamp from each character's own slots 5 to 7 | Player 2's lamp changed as player 1 walked | Not proven. Those slots swap order as a character moves, and the fault was gone once lamps were picked from the room's list by distance |
| Up is from where they stand to the middle of their body | Any way at all on someone lying down | Height is the world's: the camera's third column |

**What works:** each place with a flat draw is one character (within 80 units). Every lot of lights no bigger than a person with that place inside it is theirs (`fill_shadow`). Each keeps a slot by their place in the world, and each lamp in the room has a weight for them: the nearest two, the second at 0.55, less with distance from 500 to 1700 units, easing at 5% a frame. So two lamps' shadows overlap as someone passes between them, and there's no moment it switches. The 8 strongest shadows get a place in the atlas (3072, in 9 tiles of 1024).

**A dark polygon or square on a pillar:** a hexagon at F15350 and a square at F15300, gone once draws were matched by place. Probably the room's draws in a character's tile, cut by the tile's near plane, but that wasn't confirmed.

**Not yet seen:** enemies; the user's own eyes on any of this; two players crossing the same lamps.

## Shadows and lights flickering, tripping together (2026-11-04)

The user, after the fix above: "The shadows flicker frequently still. Some of the lights appear to trip together." Recorded in `logs/run-20261104-115843-roomwalk2`.

**How to measure it:** the screen video, not the quarter-second pictures. A half with a still camera and a still player shows anything that changes as a fault. Count frames that jump and come straight back, per half, reading `video.mkv` with ffmpeg at 60 frames a second. The scripts for this aren't in the kit yet. The video has to have the game in front. The user's own walk had 63 such frames in 92 seconds, 21 of them in both halves at once.

| Tried | What happened | Why |
|---|---|---|
| Choosing where the frame's effects run from the frame before's count of 3D-to-flat switches | One frame with no character shadows and different light, in both halves, whenever the order changed. The Examine prompt appearing is one: 40 to 31.6 mean brightness in player 2's half for that one frame | Run too early, the flattened draws haven't happened, so there are no casters, and ours replace the game's. Run too late, the HUD is in it |
| The haze's six lamps nearest the characters | One player walking turned lamps on and off in the other's half, several at once, with the pillar's outline left dark in the difference pictures | The list depended on where the characters were. It's now every lamp the room has, up to 12, kept once seen |
| The haze's colour taken from whichever character's lights came last | Not measured alone | Each lamp's colour now eases |
| Shadows reaching 1000 units from what casts them | Huge black shapes sweeping over a pillar from someone near a lamp | Now they fade out from 1.2 to 3.5 times the character's radius |
| Fading from 0.7 to 2.6 radii, and a lamp right beside someone fading to nothing | Too much: player 2's shadow all but went | Kept at 1.2 to 3.5, and a close lamp fades to half |
| Lamps from any lit draw | Tome pages and lit icon models could have been taken for room lamps | Only the lights on characters and on the room (`fromWorld` in `fill_shadow`) |

**What works:** `glow_scene_mark` notes each switch from the scene to something flat, and at the display copy, when the frame is over, `glow_scene_split` cuts the still-open pass at the last one. The effects run once a frame at the place the scene really ended. If the pass changed in between, `glow_scene_here` runs them at its end, and `AURORA_GLOW_LOG=1` prints a line for each such frame. There were none in these runs.

**Still open: single frames in player 2's half.** About one every 3 seconds, up to about 1.3 of 255 in that half's mean brightness (the frames this fixed reached 10.7), even with both players standing still. In the difference picture every shadow in that half changes at once, including a sharp-edged patch by the corpse. `logs/run-20261104-124412-fix4`, 28.4 s in the video. Counted over the same 45-second scripted walk with `ED_SETTINGS` pointing at a copy of `settings.ini`:

| Setting | Player 1's half | Player 2's half |
|---|---|---|
| As the user has it | 1 to 2 | 10 to 15 |
| `frame_rate = 60` | 2 | 3 |
| `volumetric = 0` | 1 | 7 |
| `char_shadows`, `contact_shadows`, `volumetric` all 0 | 1 | 0 |

| Tried for this | What happened | Why not |
|---|---|---|
| A character's draws in an earlier pass, so left out of the atlas | `AURORA_SHADOW_DRAWS=1`: none elsewhere in 8,517 lines, always 3 passes | Ruled out |
| Each half's projection from the frame's first room draw, not its last (which could be a flare or a spark) | 13 spikes in player 2's half against 10 to 15: no change | Reverted |

Not looked at yet: how `interp.cpp` rebuilds uniforms for each view (`build_view`) and the order each view's `WriteBuffer` and submit run in.

**Also still open:**

- With either Tome open, the frame's effects don't run at all: no `[glow]` line, no casters. The Tome pages are fine, but the other player's half loses these shadows until it closes (`logs/run-20261104-121409-tomes1`).
- Player 2's view jumps with them standing still, when player 1's camera starts a sweep (`logs/run-20261104-120740-fix1`, about F15277 and F15472), and stays offset between the two. `both.tsv` shows player 2's eye and target unchanged, so it's in how their view is built from the game camera's (`camera.c`, `aurora_split_set_view`), and `up` from the game's camera is used for both. Not traced further.

## Lamps found by watching the draws: why shadows jumped, went missing and never faded (2026-11-04)

The user, after the flicker fixes below: shadows jump about or don't show, aren't tied to each player and enemy, and the points of light don't fade. `ed flicker` passed throughout, as it only sees a value jump and come straight back. **The game has every lamp in a table: `ed system graphics`, The game's lights.**

| Tried | What happened | Why |
|---|---|---|
| Working lamps out from the lights on the draws, kept once seen | Lamps at places there are none, kept for ever, as a haze and as a place to cast from. Lamps in the table missed until someone stood near: the corpse in room 338 cast from a lamp further off than (-1209,-1,234) | A light's place in a draw is in view space. One taken during a camera sweep came out somewhere else. Room 338's (95,-1,85) came out at (49,36,80) and (120,66,71) too |
| One curve for all lamps, 500 to 1700 units | No lamp faded by its own light, and the haze was the same radius for all | A lamp's power (1,500 to 10,000) is how far it carries |
| A lamp within a character's radius (about 200) counted as inside them | Their shadow swung to another lamp as they passed under one, and back | The bounds are the test now |
| Lamps as low as the head | A black shape across the floor from a torch beside someone | They're lifted for the shadow |
| Eight places for who casts | With enemies, no shadow for the players at all. Room 138: 8 places, all the enemies' | `kMaxCasters`, 32 |
| Contact shadows at full on a body | Legs went black, once for each tile near | `body`, as shadows |
| Fading a lamp's shadows in from nothing whenever someone's found | The user: the Tome room's effects come after the room loads in | At once when they're first found. The easing is only for walking between lamps |
| Taking `0x804F0784` as it stood | Other rooms' lamps near doors, back and forth | The next room's is the higher address |
| Running scripts that open words as door teleports (`ed chapters`) | Their words stayed up over every room after | They're tried last |
| Measuring the flash in room 52 with all shadows off | Nothing to read: with no tiles the probe has no grid | Turn off only the haze |

## The flicker found: four causes, measured from inside the renderer (2026-11-04)

`ed flicker` measures each view of each moment with no capture. `docs/game/systems/graphics.md` has how it all works now. Not yet played by the user. The open items above are answered here.

| What was seen | Cause | Fix | Measured |
|---|---|---|---|
| Single frames where shadows change, with everyone still | The game leaves some characters' flattened draws out now and then, and those decided who casts: tiles went from 6 to 2 for one frame. Timing around them was an even 17 ms, so it isn't a late frame. Why it does isn't traced | Someone flattened in the last 30 frames still casts, while a lit body stands there (`kFlatMemory`) | Slot 10 standing: 3 to 7 spikes a run to 0 |
| Player 2's picture jumping when player 1's camera moves, and staying offset through a sweep | The view was set straight away from the game thread, up to a frame ahead of the draws, so it was worked out for a camera the frame wasn't drawn with | `dol_aurora_split_set_view`, in the GX stream | The largest step in their picture over the walk, 6.2 to 1.5. Their view and the drawn eye now change on the same frame |
| Whole rooms with no character shadows (155, 152, 151), and a way to lose them when a prompt shows | "The scene's last draw's uniforms" was a flag for the next draw to reach the upload. A 3D draw that went no further left it to the HUD's, so places were worked out with a flat projection, or player 2's hidden HUD's, all zeros | That draw's own, and only a scene draw's | Slots 4, 5, 7, 8: mask 255 everywhere to 243 to 253 |
| Shadows shifting on moments between frames with a moving camera | Tiles were in the view space of the frame's own camera, and a moment has a blended one | `at_this_moment` | Two small spikes on moments in the walk, not seen after. Few moments ran in co-op, so this is mostly unmeasured |

**Also found and fixed:** a character appearing or passing close took another's slot, and all its lamps' weights (closest pairs first now). Ambient occlusion in a half used the whole window's shape, so it looked twice as far sideways (`ssao::set_view_shape`). The darkening quad wasn't left out of a frame with nobody flattened.

**The 120 fps setting did nothing in co-op: always 60.**

| Tried | What happened | Why |
|---|---|---|
| As it was | `shown=61/61, caught_up=61`, even with every effect off. Solo is 120 | "The game waits at its present" was taken as behind. It does that whenever a frame takes a little over one game frame from its retrace, and the pace waited up to 30 ms for the worst of the last second, which kept each frame starting late |
| Pace at most 19 ms, behind only over 33 ms, with a back-off | Latency from 27.7 to 18.9 ms, the game's wait from about 9 to under 6 ms. **Still 60 in co-op**, now because it costs too much: with moments on, the game slowed to about 50 Hz and the back-off stopped them | 5 full redraws a game frame. `[split-cost]`: submitting and showing went from 2.3 to 10 ms on the worker |

Not done: leaving out the game's own frame, which nobody sees in a split. Its EFB copies and readbacks are used next frame, so it isn't a simple skip.

**Not a fault:** room 147's lamps differ between saves. A character only holds the lamps nearest them. The characters' places worked out from the camera match `both.tsv`, so the camera's right.

**`ed test` on the final build:** 17 of 20 in one run. `lever-p1` (player 1 walked 13, wanted 20) and `lever-p2` (script 1794 "frame 131..", which already varied by run) passed when run again, and `shadow-steady` passed once its threshold was set above the 0.13 moments differ from frames by. Not established: that `lever-p1` isn't affected by the new timing. It failed once and passed once.

## Other forms for characters (2026-11-04)

**Giving each ghost its own file's moves, as the Liche had:** `costume_moves` laid them over the wearer's. The user saw the bodies distorted, and they couldn't hold what's equipped. The bones of most ghosts are already their character's, so nothing needs laying over: read the skeleton word.

**Choosing a body by size and bone count:** `cin0077.bin` body 36 was Alex, 37 the Liche, `cin0002.bin` body 39 the Liche too, `cin0024.bin` body 38 an enemy, 37 Edwin and 36 the Liche. Bone counts agree by chance. Look at the scene that shows it, and at pictures, and the user knows them.

**Fitting a move's tracks each frame, or at `0x8012E504`:** a move's data is in a cache, and the record's first word stays when it's taken: some of what was fitted was freed memory, filled with `0x06060606`, or half read, and the walk stayed twisted though the first standing pose looked right. Fit it where it comes in, on the return of `0x8012E588`.

**Max's straitjacket on his moves, for free arms:** its bones do follow, but its arms are in one mesh group with its body, so nothing shows. Fitting moves can't change that, so the user had the form taken out.

**A ghost's weapon points all moved to one point of the group:** a thin line by Karim's hand. Each point follows its own bones, so they spread as the arm bent. Each to its nearest point of the group made it shorter, a streak from the forearm over the hand, and all at the wrist joint left a shard, as some follow the fingers too. Putting a strip's corners on one point (`+64`, `+68`) changed nothing drawn, and the swords were back: the head and shoulders photos hid that, so look at the whole body. They go on one point of their own bone's batch.

**Moving a form's floating parts to another bone by the table at `+48`:** spikes off the shoulders and a stick by Edward's hand, even with 3 points moved. Its numbers aren't bones: they go through `+44`.

**The nearest bone by place, for every bone with no name in common:** on the Liche it put two cape bones on other tracks. Only when most names don't match.

## A freeze that looked like waiting (2026-11-04)

**Reading the main thread's wait at `0x800085E8` as the cause:** it was only waiting. The log has the cause a line above: `OSCheckActiveThreads` on a priority of `0x01020304`. Look at the bad words in the saved state: a back-chain then a return address is a stack frame. The fix is in `pad_read_by_monitor`.

**Counting runs to prove a rare freeze gone:** 40 clean runs are still a 1 in 16 chance at that rate. A case that always froze, the slot 4 tour, proves more.
