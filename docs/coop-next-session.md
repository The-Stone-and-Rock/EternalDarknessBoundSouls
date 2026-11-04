# Co-op: where things stand

Written 2026-11-02. Read this first, then update it at the end of the session.

## Start here (updated 2026-11-03)

**Shadows and light, 2026-11-04, the latest word, the user says it looks right:** lamps now come from the game's own table, one for each room: `ed system graphics`, The game's lights. Every room was toured with `ed chapters 1` to `12`: what's for the user to review is in `docs/chapter-tour-review.md`, three freezes among it. The Health, Sanity and Magick help is off (`meter_help_off` in `hle.c`), and the Use and Equip help (script 1979) was left, as they didn't name it: ask. **Slots 1 to 12 are now the chapters' starts**, so scenarios that used the old slots (`both-walk`, `costume-door`, `door-climb`, `door-open`, `urn-p1`, `urn-p2`) have the wrong save and frames, and need a copy of a save of their own.

**Player 2's camera when player 1 leaves a room: fixed 2026-11-04, confirmed by the user in play, uncommitted.** Their first look in the new room ran at the old room's numbers: `docs/coop-failed-attempts.md`. `ed test p2cam-door`.

**The bookshelf's lever: fixed 2026-11-04 by script only, uncommitted, the user hasn't played it.** Player 2's Yes hung both. Five causes, in `docs/coop-failed-attempts.md`. `ed test lever-p2`. `saves/lever.state` and `saves/p2cam-door.state` are copies for the scenarios, so re-saving a slot doesn't break them.

**The other player vanishing and stopping during a press's scene: fixed 2026-11-04 by script only, uncommitted, the user hasn't played it.** The game's show flags: `coop_show_asked`. `ed test lever-scene-p1-walks` and `lever-scene-p2-walks`. Each player's icon stays through the other's scene too. A press by player 1 on their own icon during player 2's scene is the thing to watch.

**100 save state slots (2026-11-04).** The user will save glitched rooms in them to test. Ask which slot and what goes wrong, and copy a slot under its own name before a scenario uses it.

**New graphics options (2026-11-04), under Options > Graphics, not yet seen by the user:** per-pixel lighting, High occlusion quality (GTAO), SMAA and Low sharpening are on by default, and light shafts are off, as they only make a soft halo so far. `gfx/post.cpp`, `gfx/ssao.cpp`, and `per_pixel_channels` in gxcore's `lighting.cpp`. To compare: `AURORA_SMAA`, `AURORA_SHARPEN`, `AURORA_SHAFTS`, `AURORA_SSAO_HIGH`, `AURORA_PER_PIXEL` override the settings, `AURORA_SSAO_SHOW=1` and `AURORA_SMAA_SHOW=1` show their working, `AURORA_GPU_TIME_LOG=1` the cost, and `ED_RUN_SHOTS_W=2560` takes full-size pictures. The lantern light flickers, so brightness can't be compared between two runs. Character shadows (shadow maps) are still to do, if asked.

**Character and contact shadows (2026-11-04), Options > Graphics, not yet seen by the user:** Character shadows Light, Shadow edges Soft and Contact shadows on by default. `gfx/shadow.cpp`, how it finds casters and lights in `gxcore_draw.cpp` (`fill_shadow`, `strongest_light`), failures in `docs/coop-failed-attempts.md`. `AURORA_CHAR_SHADOWS`, `AURORA_SHADOW_HARD`, `AURORA_CONTACT`, `AURORA_SHADOW_SHOW=1`, `AURORA_SHADOW_LOG=1`, `AURORA_DRAW_DUMP=N`. To watch: a character is hidden from their own view in a close-up but still casts there; shadows have no alpha cut-outs yet, so a plume casts as a solid shape; rooms with no usable lamp fall back to straight down.

**Shadows, 2026-11-04:** in a scripted test on save state 10 they no longer depend on player 1's view, and they ease between lamps: the last section of `docs/coop-failed-attempts.md`. The flicker they then reported is the last section of that file, with two things still open: effects off behind a Tome, and player 2's view jumping when player 1's camera sweeps. They haven't tried this build. `ed test` on it: 17 of 19. `lever-p2` and `lever-tome-in-scene` want script 1794 at "frame 131..", and it ran for player 2 at 13200 to 13210 in the failing runs and 13180 to 13186 in the passing ones, on the same build. Left as they are for the user to decide. A full run also hit the known `0x80325188` freeze once (`hang-20261104-123658`). Enemies come after the room walk, as they asked. Enchant is dpad up on that save.

**Shadows and frame rate, later on 2026-11-04, not yet played by the user:** four causes of the flicker found and fixed, among them whole rooms with no character shadows and player 2's picture jumping with player 1's camera. The last section of `docs/coop-failed-attempts.md`, and `ed system graphics`. Everything's uncommitted. To watch in play: shadows in rooms that had none (155, 152, 151), where nobody has looked at them moving, and enemies. Co-op is capped at 60 fps, as the user asked, and the setting at 120 is for solo. Lifting it is held until they ask. HDR's option can't be seen in the pictures, and spell glow needs a cast: neither was measured.

**The user's next steps on graphics:** they said shadows and light first, then spells: `brightness overpowers spells` is still to do. What they saw and the fixes are in the last section of `docs/coop-failed-attempts.md`. They haven't seen these fixes.

**Volumetric lighting, skin scattering and spell glow (2026-11-04), Options > Graphics, not yet seen by the user:** all on by default, Volumetric at Soft. `apply_volumetric` in `gfx/shadow.cpp`, `emit_skin_band` in gxcore's `lighting.cpp`, the end of `emit_tev_fragment` in `tev.cpp`. `AURORA_VOLUMETRIC`, `AURORA_SKIN`, `AURORA_SPELL_RIM`. How to cast in a test is in `docs/coop-failed-attempts.md`.

**Held by the user:** unstable frame rates and true 120 fps in co-op, to review only when they ask: `ed keep`, and Frame rate in `ed system graphics`. A door tour in the kit (each door in a chapter, both players arriving with the right camera) and a way to move player 1. Don't build them until asked.

**To add, when the user asks:** Spawn enemy in the Mods menu's Cheats page. It's only on `origin/main`, in its debug menu: `8e2fcf4` (`spawn_requested` and `SPAWN_*` in `hle.c`, the row in `frontend.cpp`). Bring that across rather than writing it again, and test it with two players. `origin` is the original project the user forked from, griches/eternaldarknessrecomp, and their own fork isn't uploaded yet: don't push there.

**Pius's skeletal body, for either player (built 2026-11-04, the user has issues still to work out with the model: ask them what).** RB or C on Pius's frame in Character select. He's still made as Pius, with body 86 from level 13 filed under his number, and that body's own moves over his where it has them. `ed system players`, Bodies. Not established: RB on a real controller, the first-frame fix by eye, and one freeze at a door that didn't come back (`saves/reports/hang-20261104-004653`). His file has no door or ladder moves. The staff isn't added: the user may want it later.

**Pickups and words don't stop either player, and the item shows only to whoever found it.** The user confirmed this in play for slot 1's urn, from both players. If it breaks again, read `docs/coop-no-pause.md` first: it lists each symptom and its cause. Don't rebuild it from scratch, and don't skip the close-up.

Don't argue with what the user saw in play. Don't ask them to name or re-explain things they've already said. Test with a real run before saying anything works, and test from standing still.

**Tracing planned by the user:** slots 1 to 7, in `docs/coop-tome-pickups.md`. Only slot 1 is done.

## Examine: what was found (2026-11-03, the session after)

Written before the user has tried the fix. Not confirmed in play.

There are two ways words open, and they went wrong in opposite ways, which is why the reports looked contradictory:

| How it opens | Examples | Before the fix |
|---|---|---|
| `0x80010134` (return `0x80010138`) | Paul's podium and drawer (script 1448) | `hle_message_open` threw the words away: no text, no hold. The last session read this as "working". It is the user's missing text |
| Any other call of `0x80024310`, outside the Tome | The other 32 callers | The window opened and `0x80042A8C` held play: the other player stopped |

The first has been in the code since `2ae2c3a` (2026-11-01), after the user asked to skip close-ups in co-op. They meant the zoom, not the text. Script 1448 was passed over the same way in `logs/run-20261102-0025`.

**The fix (uncommitted, `coop_words_hold`):** only the window's opening and closing call `0x80042A8C`. For player 1's words it now does nothing, and `hle_message_open` lets their words open. The game already opens words with no hold over its Tome. Cutscene words and player 2's own words keep the old way. Backup: `logs/saved-changes/before-examine-words/`.

**The removed hold experiment** covered `0x800429D0`..`0x80042C10`, which takes in other functions' holds as well as the words'. That is probably what broke Paul's cutscenes. Not proven.

**Not covered by this fix:** words opened from the Tome (the urn: the game skips this hold there, so `coop_words_hold` never runs, and they arrive with busy 2 or 0), finds (`0x80042BB8`, its own busy bit 16), and player 2's words holding player 1. A scripted slot 1 run (`logs/run-20261103-133038-words1`) ran cleanly but never reached the new code. To watch in play: pressing to close player 1's words starting the examine again, player 2's press waiting for the words to close, prompts staying up during words, and player 1 taking the Tome (the 2026-11-01 crash was there).

## No pause for pickups (2026-11-03)

The user confirmed the podium in play: text shows, player 2 moves. Their rule: the game doesn't pause for pickups or spells at all.

Player 1's words and finds now set no hold (`coop_no_pause`, `coop_words_hold`, `coop_find_hold`, uncommitted). A script's hold just before them (the urn) has its bit cleared. A find keeps `r13-24048` bit 2 and `r13-25932`, which the script timers wait on. Player 1 is kept still by their controller not running. In a scripted slot 1 run (`logs/run-20261103-134338-nop1`) player 2 walked through the urn and rune, and the rune went in. Not yet confirmed in play, and that run stopped before the spell page.

**Doesn't work: answering "not busy" for player 2 (`0x80155738`).** Their node never reaches the ask at `0x8000749C` (0 asks in `logs/run-20261103-133957-dbg2`). Their stick was read and they didn't move. Other askers run each frame (`0x80027768`, `0x800277F8`, `0x800271F4`, `0x8002729C`, `0x800820BC`, `0x800E4F98`). Clearing the busy word worked where answering didn't.

**Clearing only the busy bit of a script's hold isn't enough.** The user saw player 2 unable to start from standing during the urn's words (`logs/run-20261103-134455-urn-nopause`, stick read at frames 7449..7767, position still `-530 289`). `logs/run-20261103-134338-nop1` had looked fine because its player 2 was already walking. `coop_no_pause` now lets go with `0x80199EA0(1)`, which also turns off the hold's input channels (`r13-23060`, `-23052`). Those channels as the cause is inferred, not traced. `logs/run-20261103-134741-nop2` has player 2 restarting from bursts through all three steps, and the rune goes in. Not yet confirmed in play.

**The found item shows in both halves.** It is a 3D object in the world, not a HUD draw, so `hideHud` doesn't take it out. To leave it out of one half, its draws must be marked (`build_view` in `gfx/interp.cpp` works per draw) from the game's call that draws it, which isn't found yet. Don't use `g_coop_p1_cast_shown`: it froze the picture.

**Found notices.** The user confirmed player 2 moves in the urn. They asked for a notice in the other player's top right: `(Player name) has found (item)`. `coop_found_notice` builds it from the find's words at `hle_coop_found`, with the chosen character's name, and `frontend_toast_player` draws it in that half. Logged and seen for player 1's urn and rune (`logs/run-20261103-135512-note2`). Player 2's finds and spell pages not yet tried. A spell may not come through a find: its words aren't traced.

**The user's Karim's chapter weapons are `d0b1fdc`:** the enchant's item stays in player 1's half with `g_coop_p1_cast_shown` holding player 2's half. Don't tell them it can't be done. It looked right then because the world was paused; with no pause, the hold is a visible frozen half.

**One item, in the finder's half (built, uncommitted):** the user's rule: the finder sees the item, drawn once; the other player gets only the notice at the top of their screen. See `docs/coop-failed-attempts.md`. Player 2 as finder isn't tested yet. Their finds still hold player 1 and keep player 1's half still.

The earlier one-half items (monk's key `862e14d`, enchant `9bba6ba`, player 2's finds) keep the other half as a still picture (`coop_hold_player1`, `g_coop_p1_cast_shown`). Nothing yet leaves one draw out of a live half. The find's item comes through `0x801F9DE8`/event 71 in `0x80024528`; which call draws it isn't traced.

**Committed:** `c8f69bc`, `50c1d62` on `tome-copy`, not pushed. The user confirmed player 2's urn. They then asked for player 1 not to be paused during it, and for all of this to hold for any model and chapter. The first is the open job: see the last section of `docs/coop-failed-attempts.md`.

**Not done:** spells aren't checked against the no-pause rule, and only slot 1's urn has been played.

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
2. **Player 1 must stay free during player 2's words and pages.** The user has pointed at this more than once, with pictures (`Screenshots for Claude/p1-held-1.png`, `p1-held-2.png`). They report player 2 can move during player 1's finds, so the way back isn't there yet. See `docs/coop-tome-pickups.md`.
3. **The flicker on making a spell.** The user saw it slightly. `You Have Just Discovered` was logged about 660 times in `logs/run-20261103-114339-magick-p2`, which may be the cause. Not looked into. Which half isn't known.
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
| `python -I tools/ed/ed.py` | **Start here.** The whole game from the disc: `how`, `needs`, `host`, `keep`, `system`, `verb`, `item`, `mix`, `door`, `transition`, `camera`, `where`, `tag`, `examine`, `who`, `carried`, `level`, `prompts`, `script`, `room`, `text`, `uses`, `fn`, `dis`. Both players in a run: `check`, `timeline`, `see`, `sheet`. Regression: `test`. See `.claude/skills/ed/SKILL.md` |
| `sh tools/ed/run.sh saves/slotN.state NAME` | As `play.sh`, and records both players each frame and pictures for `ed` |
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
| `docs/coop-no-pause.md` | Pickups, words and items in co-op: each symptom, its cause and its fix |
| `docs/coop-failed-attempts.md` | What was tried and didn't work. Check it before writing a fix |
| `docs/coop-two-tomes.md` | Why two game Tomes at once was set aside, and how the Tome works |
| `docs/coop-two-tomes-design.md` | The design to try next for two Tomes: player 2's in its own copy of memory. Not built |
| `docs/coop-todo.md` | The longer list of open issues |
| `docs/items-log.md` | Items seen so far. Add to it in any session |
