# Co-op: to do

## Independent sanity effects (requested)

Each player should have their own sanity effects, each tied to that player's own
sanity level. When the game would normally call for one (blood running down the
wall, for example), each player gets their own random pick, so the two can
experience different ones at the same moment.

Seen in co-op as it stands: the blood effect fires often around Brother Andrew's
room, and a capture showed a red streak in player 2's half and not player 1's (one
frame, not confirmed to be the effect).

## Open issues

- The Cheats menu's all-magick option (fixed 2026-11-08, uncommitted): it gave the circles and runes without the flags that say the Tome's Magick page has taught them (`kMagickTaught`); the page then waited on a teaching script (2791, 1306) that Anthony's chapter (level 03) doesn't have, and went to the Magick page on every opening. Checked by script: logs/run-20261108-162124-cheat-magick-fix. It no longer gives the Tome (`cheat_tome` removed): given as soon as it was there, the Tome room's own taking was passed over, its transition's sound played on and its way back came at the casket's cinematic (the user). The Tome is the story's; all magick is in it once taken.
- The world behind the Tome (in testing, not committed): each player's Tome and quick spells leave the world running.
  - Both the world and the Tome run at 60 while a Tome is open, measured in the save in slot 7. `TW_WORLD_FRAMES` in `runtime/host/hle.c` is the balance if a heavier room falls short.
  - The Tome's yellow cursor border pulses twice too fast in co-op (175 ms a cycle, 342 in solo). It goes by the game loop's passes, not the Tome's frames: about 180, 230 and 260 ms with `ED_TW_WORLD_FRAMES` 1, 2 and 3, which is 342 x 60 / passes a second. Which counter it reads isn't found. Ruled out by holding them still across one kind of pass, with the pulse staying at 170 ms: `8061EDFC`, `8061F744`, `8061F810`, `8061FB10`, `8061FCCC`, `8061FD14`, `80601814`, `8061EC8C`, `806085C8`, `806017E4`, `806EDD10`, `805DB48C` (room 147, `saves/lever.state`, heap addresses so they may differ elsewhere). Those are words that move on both kinds of pass, from a tally of guest memory at the turn in `tw_mode_change`. That tally listed only the first 400 of 1,921 by address, so what's above `0x806F` isn't looked at, and `0x807065C0` (a sweeping texture, steps each pass) isn't tied to the cursor. To measure: `ED_RUN_SHOTS_MS=15`, how yellow the box is in each picture, and the strongest period over the last 3 seconds.
  - Both players' Tomes open together: the game runs at about 80% speed. The state swapped is in `runtime/host/tome_state.inc`.
  - The other player's bars and prompts are hidden while a Tome is open.
  - Untested: a door, cutscene, death or area change while a Tome is open; quick saving with a Tome open; save or load from the Tome; a Tome page other than the inventory.
- Using an item holds player 2: when player 1 used an item from the Tome in slot 1 (script `0x5E6`), player 2 could not move until it finished. They should be able to move. Run `logs/run-20261102-193536-use-coop`, frames 8797 to 9210: player 2 stays at `-1225 -147 0` and no press of theirs is logged. Cause not found. A custom player 2 menu needs this fixed too.
- Retest the monk's scene (player 2 rings the bell): a script's hold no longer counts as player 2's close-up, and that scene may use the same hold.
- In the Tome room's first-entry scene, the right half (player 2's) stays on a close-up for the whole scene, while the left half plays the camera's moves. The user was happy with how it looks when player 2 triggers it.
- Player 2 cannot cast in room `0x72` (needs a run with `ED_COOP_CAST_LOG=1`).
- Both players held for about 30 seconds at the bottom of the stairs (room `0x67`),
  seen once, just after "the game's collision test ran on".
- `OSCheckActiveThreads` crash seconds after loading a save: seen once, cause unknown.
- Barrels in room `0x72` set aside: the co-op run may just have missed the right one.

## The launcher's music for everyone (requested 2026-11-08, later)

The launcher plays res/music/launcher.wav, a loop of the game's Options song recorded from the game as it plays (MusyX, built from note data, so no file on the disc to copy; `tools/launcher_music.py` cuts it from an `ED_PCM_OUT` recording). It's the game's own music, so it can't ship. The user's wish: on first play, once the launcher has found the right disc, the recomp makes it itself, as games cache their shaders: run the game unseen to the title Options, record a loop and keep it, then the launcher plays it from then on. Other things the recomp needs from the disc can be made the same way at first run.
- With it (the user, 2026-11-08): a "Building... Please wait" screen with a progress bar for anything first play makes that takes time (the music's recording; the start's pipeline build already runs behind the launcher, 5611 in about 27 s on the user's PC, and needs none).
- Done 2026-11-08: the forms' portraits are now the user's own art (AI-made, from their sheet), in res/ui/portraits/NAME-FORM.png and shipped; the old game photos are kept in build/portraits-photos-backup. Was: the 12 characters' are written from the disc by the menu (`ensure_portraits`), but the 22 forms' (NAME-FORM.png) are photos `tools/ed/form_portraits.py` takes in a run on the user's own slot 5 save, so a new player's character page shows each form with its character's own portrait (`portrait_rml` falls back). First play should take them itself, from a start that needs no save (Jump to Game), behind the same building screen.
