# Co-op: where things stand

Written 2026-11-02. Read this first, then update it at the end of the session.

## Start here (updated 2026-11-03)

**Status on 2026-11-08: everything below up to f2b2565 is committed and released as v0.1.0 (pre-release).** The entries below are a dated log. Their "uncommitted" and "not yet played" were true when written, and many aren't any more. Played and confirmed by the user: the dream (camera, Shotgun in and out, no Tome), player 2's reload and the Dual Flintlocks, Anthony's and Ellia's gun moves (hair and reload), the fireplace puzzle, its flare and icons, and the menus. Also fine as far as the user knows (2026-11-08): E1 enemies seeing player 2, the monk's bell rung by player 2, player 1 vanishing after the fireplace. Not played yet: Summon Trapper's switch, W2 FINISH prompt. **Next (the user, game-breaking): I2/I4, Enchant Item mending Anthony's chapter's broken urns.** The user will make a save with the spells, where they can mix and enchant the urns, and trace solo first. The Cheats menu's all-spells option is bugged (not looked into; the user will get the spells another way). Still open:

**Anthony's casket by player 2 (2026-11-08, the user played it: fixed).** Opening the Tome room before the sword is the game's own (solo, logs/run-20261108-130144-solo-anthony-ref). The casket is L03/1587 (EXAMINE, Yes, its scene), which runs on to L03/1939 with fn73: cinematic 128, then the sword (ed3 at "the player", ed52), the monk's walk (fn17), Anthony to his mark, let go. Solo, 1939 is called from 0x80164488 (fn73), 0x80026FE4 (the cinematic's end), 0x80163E5C, 0x800244C0 (find closed), 0x80168388 (the walk done) (logs/run-20261108-130307-solo-anthony-sword). In co-op the cinematic's end ran 1939 as player 1's from the game's table, with no word for it: the cinematic played twice, player 1 found the sword and was walked to Anthony's mark, player 2 held for good (logs/run-20261108-130550-coop-casket-p2, the user's play; `Screenshots for Claude/casket-p2-frame5937.png`). Now, in `coop_run_script`: a script that ran as player 2's is theirs (`g_coop_p2_scripts`) until player 1's press or zone runs it, and the game's call-backs into it (the cinematic's end, a walk done, a find or window closed) run it as theirs. At the cinematic's end the co-op cutscene's end runs first (`coop_cutscene_over`), or player 1's half kept the casket's camera. A find in a scene of theirs (their script's own hold, busy 0x32) is their close-up (`g_coop_script_hold` off), or its words went to player 1's half and pad. Checked by script on the user's slot 2 (logs/run-20261108-131515-fix2-casket-p2): one cinematic, the words in player 2's half closed by their B, player 1's own camera and free, the scene to its end as player 2's, player 2 free after. The bell (`0x80168388` was the game's, e940905) now goes on as player 2's too: retest it. `ed test` runs the old slot saves the user ruled out: don't run it.
- Player 2's camera on entering the Tome room at the chapter's start: the same glitch as one fixed before (the user, 2026-11-08). Not looked into. the table under "Open bugs" and `docs/coop-todo.md`.

**New game's opening dream (2026-11-07, after b6e80a8, uncommitted, not yet played by the user):** the dream is chapter 16 (L00/1911's `ed87(0, 16, 0)`, word `0x80307178`). Three changes, each seen in `tools/ed/jtg.sh` new-game runs (`JTG_NEWGAME=1`, logs/run-20261107-135950-jtg-dream-p2d and -135615-jtg-dream-p2b):
- Player 2's half had player 1's opening shots (DreamCam03's close-up, DreamCam01's turn round them). Through those two shots, while player 1 is held, player 2's view is moved by where player 2 stands from player 1 (`hle_coop_dream_shift`, applied in `camera.c`), eased out at the game's own rate as DreamCam02 takes over, and kept in front of walls (a first try showed wallpaper as player 2 walked off). After control DreamCam02 already followed each player (`coop_camera_step` re-points records naming player 1).
- The dream's Shotgun (`ed3(..., 3)`: effect 3 lists it with `0x80150D9C` at `0x8000EABC`) went to player 1's hand only. `notify_spawn_equip` notes it, and `coop_give_held` lists it for player 2 and puts their copy in hand (`0x800C994C`, as their Tome does) at their next turn's end. This is any script's effect-3 spawn, not the dream's alone. Holding without listing first was undone by `coop_rehold` ("held nothing").
- No Tome for either player in chapter 16 outside a scene or menu (`coop_dream_no_tome`): both pads' Start is dropped there, so a scene's Start (skip) still works.
Not checked: the dream's end and the Tome working again in the mansion after it; real controllers; `ed test`.
- Committed as ef6e79f. After it (uncommitted): player 2 kept the Shotgun after the dream. How the game takes player 1's, traced with `ED_ITEM_TRACE=1` (logs/run-20261107-142252-jtg-dream-end, frame 3471): the dream's input mode (ed56(7)) counts 1400 down (`0x8007B51C`) and its end (`0x8007E3C8`) runs L00/2027 (flag 440, bars off, `ed55(0)`); the dream's last script 1920 moves to room 69 with arrival script L00/397: `ed96()` the weapon in hand, `ed147()` its ammo, `ed51(player, weapon, 0)` lets go of it, `fn93` on each takes it off the list and destroys it (`0x801F965C` at lr `0x8016CD80`, then `0x801DCB58(2, node)`). Player 2's copy is in no list, so nothing took it. Now `coop_note_taken` notes each thing `fn93` takes, and `coop_take_held` at player 2's turn's end lets go of their copy of it (`0x80150FA0`), makes their moves again (`0x8007A788`) and destroys the copy: at frame 3467 in logs/run-20261107-142603-jtg-dream-end2, equipment 1 -> 0, part 0 loaded. A first try instead polled once a second for copies with no carried original; it took the Shotgun in the dream (`0x801F60BC` matches a thing's tag, not its handle) and left the gun's stance: removed. Save 1 was made on the old build after the event, so it keeps the copy: test from a new game or a save made before room 69.

**After 23e8164 (2026-11-07 afternoon), e20079b and after, not yet played by the user:** player 2 at Max's chapter's fireplace puzzle (the user's slot 1, room 188's L05/188: EXAMINE, DETAIL, the words, then `ed56(3)`, the raven slid onto Ulyaoth's tile). Its MOVE / EXIT / SELECT showed in player 1's half, and after A player 2 stood held and unseen in their own view for good. The interfaces (`ed56(n)`, code 0x8007A968..0x80083700) opened in player 2's close-up are theirs now: their asks stay in player 2's prompts (`coop_prompt_player1s`), and the script each runs as it closes or selects (23 `bl 0x801642D4` sites, `coop_interface_ran`) runs as player 2's, as a prompt's use does. Checked: prompts in player 2's half; B out and a wrong tile's A both ran L05/188 as player 2, who then walked off (logs/run-20261107-125115-fire-fix, -125442-fire-solve3). Not checked: the right tile's A (a stick press script couldn't stop the raven on it; SELECT shows only at rest on a tile). A wrong tile's flare hit player 1, not player 2 who chose (125 -> 115). Not script 1946 (thing 46's own path step, a coincidence in time): the interface's wrong-tile path 0x80080D70 sends the hit itself, event 39 with 10 (0x8008108C), to the game's current player (id 0x801F62B8 at 0x80080EBC, place 0x801F62B0 at 0x80080EC4). Committed e20079b for the prompts and scripts; after it, `coop_interface_player` answers those two calls with player 2 in their close-up: player 2 takes it (logs/run-20261107-131337-flare-p2b: player 2 125 -> 115, player 1 125), then walks off. The interface's icons jumped right for a frame as they changed (SELECT as the raven stops on a tile, MOVE EXIT as it leaves): the game makes its prompt anew in the frame (0x8011708C from the chooser 0x80117850, often at the same address), after the controller read moved the last one (`coop_close_up_prompt`). `coop_game_prompt_made` puts each new one in player 2's half as it's made (logs/run-20261107-132644-icons-p2b, pictures every 16 ms).
- Committed 756b57d for the icons. After it, uncommitted: player 1 vanished from both halves after their own wrong tile. A script's hide (`fn111(0)` -> 0x80043C34) sets two words for the next frame, -25692 (applied by 0x80043D04 from 0x80043CB4) and the main one -25688 (0x80043C88 at 0x80043C6C, applied by 0x80043D70). The host kept the first as a hide from their own view alone, but the main one reached the game (r13-26272 1 -> 0), and the show (`fn111(1)`) was swallowed whole because a hide was kept, so the game never showed them again. `coop_show_asked` now lets the game's show go on as it clears a kept hide (showing what's shown is nothing). Checked: player 1's wrong tile (logs/run-20261107-133257-p1-wrong-fix), player 2's (-133349-p2-wrong-fix), player 1 backing out (-133452-p1-exit). `ED_SHOW_TRACE=1` prints each show or hide asked.

**After 269b37f (2026-11-07 midday), uncommitted, not yet played by the user:**
- Player 2's reload in the user's slot 1 (Pius with the Dual Flintlocks, Anthony's chapter): RELOAD showed and B did nothing. Two causes. (1) Their box count was kept in a host table no save state keeps (`g_coop_boxes`), so after a load it started again at player 1's 1 while `coop_kinds` (saved) had their 5. `coop_box_count` now keeps it in `coop_kinds`' own (by the kind's mark). (2) The Dual Flintlocks' right gun was full and still marked empty (0x20), so RELOAD stayed. B's reload (event 198, `0x800598D0`) loads the right gun first only when it isn't full (state 90, then 204 → state 77 for the left), so it went straight to the left one. `coop_reload_test` now clears 0x20 on a full gun, as the game's loads do. Checked (logs/run-20261107-111233-s1-p2fix): fire, then B loads right then left, with player 2's boxes 5 → 4 → 3 and player 1's 1; a second cycle 3 → 2.
- Gun moves by character (`kGunMoves`, `kGuns` in `wpn_part`): the user's choice of whose moves a character plays with a gun from outside their chapter. Only Anthony so far (Peter's), as asked: "test only anthony first". Peter has no Dual Flintlocks part (119), so those stay Max's. Checked as player 2 from saves/reload-p2.state with a settings copy (player 2 = Anthony): Peter's 63 fitted (62 of 70 bones), reload, aim, fire and recoil (logs/run-20261107-112046-p2anthony-reload). Next, when the user says so: Ellia, her forms and the Maid (wearer 70) get Alex's (9) gun moves ("they swell up", only her gun moves), and Inspector Legrasse (wearer 74, form 5) gets Peter's.
- Gun movers now also: Ellia, her forms and the Maid (wearer 70) get Alex's; Inspector Legrasse (74, form 5, read from the player's chosen form, `costume_on`) gets Peter's (logs/run-20261107-114941-v-legrasse: 62 of 70). Plain Ellia's hair stands out in spikes on Alex's moves (docs/coop-failed-attempts.md, "Plain Ellia's hair"); the Maid and rotten Ellia fit better. Fixed by the 45-degree rule below (aa1b306), and the user saw it fixed in play.
- How the game poses a bone (read 2026-11-07, `0x8011FF74`, `0x80120E2C`, `0x8011D394`): a track's list 0 is its translation (s16 / 16, absolute from the parent), list 1 its rotation (quaternion x, y, z, w, s16 / 16384); key word 0 is time << 2 | type (1 translation, 2 rotation, 3 none). Local = [R(q) | T]; world = parent's world times local; skin = world times the bone's +12 matrix. A bone with no rotation track gets the inverse of its +12 (its whole place as made) on top of its parent's: why an empty track sends a bone flying.
- Gun moves by name match leave out a match turned more than 45 degrees from the home's as made, and what hangs under it (`kFinTurnsOf` from `tools/ed/facts/weapon_homes.py --turns`; `wpn_bind`): plain Ellia's side braid on Alex's ponytail track (54, 61 degrees) stood out in spikes. Anthony with Peter's: none left out.
- A gun reloads by its own home's reload (`wpn_reload_swap`, moves 10, 158, 159, 160): Anthony and Ellia ram the Flintlock with Max's reload, not Peter's or Alex's revolver's. It goes through the finisher's swap (`fin_swap_move`; the finisher's own move 65 unchanged), with the home's turns as they are (turning them by the difference as made spread Ellia's arms: docs/coop-failed-attempts.md) and each bone but the hips at the holder's own place from its parent (`g_fin[k].length`, M12(parent) inverse(M12(bone))): with Max's places their bodies swelled during the reload (the user, 2026-11-07; logs/run-20261107-123051-len-ellia, -123106-len-anthony after). Checked: logs/run-20261107-122607-rt-ellia, logs/run-20261107-122627-rt-anthony.
- `ED_WPN_BONES=1` prints the holder's bones (parent, place, the home's bone it plays) at each binding.
- `ED_WPN_EQUIP` toggles a weapon already in hand off, and a second equip in the same run left recompiled code (logs/run-20261107-111711-anthony-peter3). Use a settings copy and a save with the weapon in hand instead.

**Tier 1 of the 2026-11-07 playtest, uncommitted, not yet played by the user** (the triage and full plan: the session's plan-v2; the user's rules there are hard rules for the whole game). All in `hle.c` unless named:
- E1, the enemy's view cone misses player 2: event 58 skips `coop_events`' early branch (from b924bbd) and goes to `coop_as_player2` again.
- I7, player 2's reload: fixed 2026-11-07, on by default (`ED_COOP_P2_RELOAD=0` turns it off). RELOAD shows for them (the ammo look answers from either list, `coop_reload_lists`; an empty weapon with ammo is marked as the game marks it, `coop_reload_test`); its state 90 ends (event 204, posted to "the controlled", is sent to them, `coop_send_event`); the load (`coop_p2_load`, from the move's frame, outside their turn) runs with player 2 the player and their own count of the box (`g_coop_boxes`): a box they use up stays player 1's. Their count shows in their half. Checked as Peter on `saves/reload-p2.state` (the user's slot 14) with `ED_SETTINGS` mod_p2_character = 9: three reloads, theirs 5, 4, 3, 2 and player 1's 5 (logs/run-20261107-021607-reload-counts8, -022030-reload-hud3). A body without the weapon's moves part (Pius with a pistol) still gets none: W1. `ED_RELOAD_TRACE=1` traces it all.
- E9, sprint never tires: r13-29528/-29532 (breath) kept per CoopPlayerState beside it (`coop_breath`; save states unchanged). Checked with `ED_BREATH_LOG=1` (logs/run-20261107-012329-jtg-t1-breath): player 2's breath fell 190, 166, 136, 106 while player 1 stood.
- Summon Trapper's "switch player" reload: `characters_reload` doesn't reload a player in a non-playable body (play.log: player 1 75, now 28).
- I2/I4, Enchant Item on a broken thing (template bit 0) takes the game's own mending branch (`coop_controlled_id`). Weapons unchanged. Not run.
- W2, FINISH: player 2's (and player 1's in player 2's close-up) prompt styled as the game styles its own (`coop_prompt_style`, `\i6` above the words); the icon cell is no longer dimmed (`buttons.cpp`). Not run. The disc's words are FINISH HIM.
- G3: one Anti-aliasing row, Off / SMAA / 4x MSAA / 4x MSAA + SMAA (`frontend.cpp`).
- T1: player 2 on Default is named as player 1's character; no renaming in a full cinematic's words.
- W1, every character uses any weapon with its own moves (2026-11-07, uncommitted, not yet played by the user): a weapon from outside the holder's chapter plays its home character's moves part (`kWeaponIn`, from tools/ed/facts/weapon_homes.py), fitted to the holder by bone name as the finisher's are (`wpn_part`, `wpn_bind`, `wpn_fit`, `wpn_check`; player 1's in host slots 7 and 8 at 0x82600000, player 2's in their 0 and 1). `fin_*` unchanged. `ED_WEAPON_MOVES=0` turns it off, `ED_WPN_LOG=1` logs it. Checked: player 2 and player 1 as Pius with the Flintlock (Max's 63: reload, aim, fire; logs/run-20261107-024001-w1-p2pius, -024129-w1-p1pius); Peter with the Chakrams (Karim's 63, logs/run-20261107-024739-w1-chakrams); weapon switching; a state saved and loaded; Peter with his own Flintlock unchanged (-024350-w1-vanilla against -024309-w1-off). Changes melee too: Pius, Alex or Peter with Karim's Tulwar now swing Karim's 42. Not tried: player 2 taking up a weapon in play, a form put on while holding one, solo, the Elephant Gun on Peter.
Left: I5 (player 2's sword in their Tome copy, risky). Smoke: cold Jump to Game ch1 and ch2, control at 1810 / 1622, no hang. `ed test` not run: its scenarios load the old slot saves the user ruled out.

**Whole-game co-op rounds, 2026-11-05 night to 2026-11-06, uncommitted, not yet played by the user.** The user asked for the core loop to work in every chapter for both players: pickups, cameras, finishers, doors and room changes. Nothing is tested from saves now: `tools/ed/jtg.sh` boots cold, goes through the title and Jump to Game, and records both players (`JTG_SLOT=auto` shares 3 slots between agents; `tools/ed/variant.sh NAME DIFF` builds `build-win/var-NAME.exe` from a patch and always puts the tree back). The old `ed test` scenarios load slot saves made on older code, so their results don't count. Merged in `runtime/host/hle.c`:
- round 1: p2door, nopause, p2cam, finds, finisher slots, doorown (`build/fix/merged-hle.c`, `var-merged.exe`);
- round 2: p2exit (player 2's door, climb and passage prompts run as player 2), the real-press check for doorown, follow (a form's collision size; the follower's placement), camstack (per-player camera stack). Source is `build/fix/merged2-hle.c`, and `var-r2.exe` is the build. `var-base.exe` is the code before round 1.

What was checked, chapter by chapter and player by player, is in `docs/coop-no-pause.md` (the 2026-11-05 evening and 2026-11-06 rows), and what failed is in `docs/coop-failed-attempts.md`. Still open after round 2:
- FINISH HIM was never reached in a fight;
- chapter 6 hangs in co-op (pc `0x8011FDA8`, solo is fine);
- player 2 couldn't walk at the start of chapters 8 and 11 on older builds;
- player 2 has no camera at chapter 9's start;
- player 1's statue scene freezes player 2;
- player 2's examine during player 1's Tome;
- player 2 put inside chapter 2's zone exit L02/81.

Round 3 (2026-11-06, agents) is in `build/fix/merged3-hle.c`, which is now `runtime/host/hle.c`, built as `var-r3.exe`. Each of its fixes passed an independent checker:
- startfreeze: player 2's turn lets go of the input focus, so they walk at the ch8 and ch11 starts;
- ch6hang: a moves part is never loaded over the move still playing;
- p2camstart: player 2's camera follows the room start's choice (ch9);
- zone81: player 2's zone line is the game's own step line;
- `tw_menu_depth_is_camera`: each Tome mode keeps its own camera depth;
- `g_coop_p2_scene`: in player 2's own scene, their prompt press starts nothing.

Rejected, with reasons in `docs/coop-failed-attempts.md`:
- p1scene: player 2 walked in player 1's scenes, but an exit mid-scene locked both;
- finisher3: player 2 got FINISH HIM but stayed in state 30;
- the behind-the-Tome close-up routing.

`tools/ed/jtg.sh` has `JTG_SLOW=1` for a loaded machine. Smoke-checked on var-r3: ch6 ran 8900 frames past control with no hang; ch8 and ch11, player 2 walks during player 1's words; ch9, player 2 on Camera07 from the start.

Round 6 (2026-11-06, agents) is in `build/fix/merged6-hle.c` (md5 b0583ad4a50675cb0e2764cc1eb777cb), which is now `runtime/host/hle.c`, built as `var-m6.exe` (build/fix/merged6.diff against merged5). Each fix passed an independent checker; patches in `build/fix/<key>/`:
- events (2.diff): player 2's pickup words with no close-up close (a window their use opens takes the game's input, not their copy's); round 5's grab ported, and the other player's busy-set event 102 no longer frees a held player (either player);
- follow2 (4.diff): the follower is never placed inside a scripted zone the arrival spot isn't in, facing from the entry marker;
- scenecam (2.diff): player 2's own-zone camera kept after player 1's scene; player 2 held by their own walk-in scene (busy, zones and r13-23280 kept after their look); player 1's B kept in a player 2 close-up is no longer dropped when its prompt isn't on show yet;
- tome11 (1.diff): the ch11 player 2 Tome crash (FPU handed over at OSLoadContext in the copy's turn, GQRs put back).

The user's rule (2026-11-06): a full cinematic (play_cinematic, e.g. the ch3 casket's 128) holds both players, as the host already does; only in-world scenes are walked through by the other player.

The pattern behind the scene bug: `coop_prompt_look` puts back r13-24040..-21980 after player 2's look. That range holds the busy word, the zones word (r13-23040) and r13-23280, so anything the game sets there in player 2's look (a scene's hold) is undone unless it is kept explicitly.

Round 6's 12-chapter check of var-m5 (reg-a ch1-6, reg-b ch7-12, about 90 Jump to Game runs) found no round-5 regression. Still open after round 6:  both players starting scene 1538 at once leaves player 1 on ZombieCam01 (old); ch7 158's follower stands on the stairs until the owner moves; player 2's finisher with Finishcam01-03 never seen; player 2's ch3 casket examine untested (the test put missed the casket). `ed test` (old slot saves): 7 of 20 pass on var-m6, the same 7 as on var-m5. Smoke on var-m6:
- ch5 DESK by player 2: the words close with their B (logs/run-20261106-155300-jtg-regm4-c5bp-m6, 'a window of player 2's use took the input: 6 words', 2036 at 3392);
- ch5 BOOKSHELVES: player 1's B in player 2's shelf close-up is kept and player 1's ShelfCam02 close-up opens after (logs/run-20261106-155902-jtg-rega5-c5c2-m6; failed on m4 and m5);
- ch1 exit sphere L01/9: player 1's B during player 2's CHATT_BLOCK pickup is kept and goes in at 7470 (logs/run-20261106-155834-jtg-rega5-c1w-m6; failed on m4 and m5); during player 2's hall scene 779, player 2 held, kept B goes in at 8068 (logs/run-20261106-155940-jtg-regm4-c1hL-m6);
- ch2 138 scene 1538: player 2 held by their own and it plays once, flag 986 (logs/run-20261106-155416-jtg-vsc-c2q-m6); after player 1's, player 2's own camera kept (logs/run-20261106-155623-jtg-vsc-c2r1-m6);
- ch2 166 statue by player 1 with player 2 walking, then player 2's exit to 138, then player 2's own 1538 held with player 1's exit waiting (logs/run-20261106-160416-jtg-regm4-c2aL-m6; the shorter c2a ran out of scripted presses as the scene ran long);
- ch4 player 2's CLIMB DOWN: both on Camera01 (logs/run-20261106-155622-jtg-regm4-c4b-m6); ch1 ladder by player 2 (logs/run-20261106-160044-jtg-regm4-c1d-m6);
- ch7 1213 by player 2: player 1 placed at 869,-225, no trip back (logs/run-20261106-155319-jtg-rb5-c7p2158-m6); ch8 c8r: follower refused inside 227, player 1's own B later (logs/run-20261106-155452-jtg-rb5-c8r-m6);
- ch11: 4 player 2 Tome openings, no crash (logs/run-20261106-155735-jtg-t11-g4m6). The scripted player 1 was grabbed, didn't wiggle and died: game over.

Round 7 (2026-11-06, the finisher, agents) is in `build/fix/merged7-hle.c` (md5 11c6676d9f0cf77ff4ee8bf304a85861), which is now `runtime/host/hle.c`, built as `var-m7.exe` (build/fix/merged7.diff against merged6). The user asked that each weapon play its own finish, for every chapter, character, form and both players, and that only the finish move changes:
- weaponfinish (`build/fix/weaponfinish/4.diff`, `fin_swap`): at FINISH HIM's one call for move 65 (lr `0x800C3258`), the held weapon's home character's move 65 is played, fitted to the holder by bone name. The table comes from the disc via `tools/ed/facts/weapon_homes.py`. Checked on slot13 with the Gladius on Karim, Alex, Alex in Chandra's ghost (the user's 77841), Ellia, Lindsey, Max, Paul, Edward and Michael (blade point-down into the body); Pius unchanged; again after a quick-load. The checker found the quick-load bug in 3.diff, fixed in 4.diff;
- fincam (`build/fix/fincam/fix.diff`, `coop_finish_camera`, `coop_finish_body`): each player's own finish close-up (count, flag, rotation and camera set), only in their own half; a form uses its wearer's body's Finishcams (solo too). It also carries the ED_FIN_TRACE trace.
Not run: player 2's finish (slot13's player 2 holds no weapon) and the 14 other home weapons; player 1's finish while player 2 has the cameras lent; both finishing close together; other chapters. `ed test` (old slot saves) on var-m7: 7 of 20 pass, the same 7 as on var-m6. Pictures: `Screenshots for Claude/finisher-m7/` (before and after sheets). Later, with the user: rearrange the character selection screens (Chandra sits under Karim's forms but uses Alex's moves).

**Character screens (2026-11-06, `runtime/host/frontend.cpp`, `res/ui/menu.rcss`):** the user asked for a reorganised page, with nothing about the characters changed. Default (each chapter's own character, the project's emblem as its picture) is the first frame, in place of the old "Vanilla Character" row. After it come each character and then each of their forms (29 frames), four to a line with three lines in view. The view moves a line at a time with the arrows, the right stick (kScrollUp/kScrollDown, held it repeats every 160 ms), Page Up/Down or the mouse wheel (`roster_keep_in_view`, `roster_wheel`). The scroll bar is an ink rail between two of the Tome's diamond marks with a red, gold-edged slider; the details are a card with a leather band behind the name over a lighter parchment leaf, with a shadow. RmlUi's own `overflow: auto` scrolled the content less than its scroll value said (about 0.89 of it), so the bottom line was cut off; it isn't used. Every frame's panel has the same layout: the name, the form in red between the Tome's ornaments, the year, the words (each form has its own, in `kForms`), whose health, magick and sanity it plays with (Chandra: Karim's, on Alex's moves), then the meters. Choosing a frame writes the same settings as before (`mod_pN_character`, and the form's 4 bits in `mod_pN_costume`), so hle.c reads them as it always has. Checked by pictures at the top, middle and bottom of the list (`Screenshots for Claude/roster/`). Not yet tried by hand: choosing a frame, the right stick and the mouse wheel. Next, if wanted: a turning model of each character on hover, from photo-mode captures.

**Character screens, round 2 (2026-11-06, uncommitted):** names are Spirit, Corpse, Liche, Cursed and Mutilated; no "Original form" line; no "whose stats" line, only the bars; the chosen frame glows gold (`.tile.chosen`); each form has its own words in `kForms`; Default reads "Play as the game intended". New forms, all checked by picture from cold Jump to Game runs: the Custodian (replacing the "monk" that was Paul), Anthony's Cursed (his last stage; his corpse is now form 3), the Maid, the Maid's spirit, the Servant, the Footman and Inspector Legrasse (on Paul's bones). `docs/game/systems/players.md` has where each body is. Charlemagne is left out: his body has one bone. Next with the user: give Charlemagne's mesh a skeleton (split it by nearest bone of Paul's robed skeleton when the body is made), then a turning model on hover. Found on the way, not looked into: with player 1 as Michael, a subtitle in Anthony's last scene read "Michael" where the game says "Anthony".

**Launcher and character page (2026-11-06, uncommitted):** the launcher has Choose Character and Co-op under Load game (the Mods pages of sections 3 and 1, whose Back now returns to the launcher: `g_mods_page_return`); the Mods list shows Characters only in a game. Choosing a frame on the character page stays on the page (the gold frame moves); B goes back. The character rows show the frame's own name (The Maid, Chandra · Spirit), not whose form it is, which read as the name not changing. Not yet tried by hand: choosing a frame and B back to the launcher.

**New game: the carry-over question (2026-11-06):** with a finished game on the card, a new game asks over the memory card screen whether to carry the alignments over. `skip_card_inserted` (made for Jump to Game) pressed A all through the screen's step, so it answered Yes for the player. Now the card screen still passes, but while the question's notice is up (r13-25148 == 0x8001FC54, `card_carry_over_up`) no A is pressed and the screen is drawn as the game draws it (logs/run-20261106-224511-jtg-carry-new). Jump to Game is unchanged (control at 1872).

Line endings: `runtime/host/hle.c` and the docs are LF again, as in git, since the round-7 commit. The earlier rounds' diffs in `build/fix/` were made against a CRLF copy and need `--ignore-whitespace` or a conversion to apply.

Round 5 (2026-11-06, agents) is in `build/fix/merged5-hle.c` (md5 d337e680beed550fc33730c897f4a98c), which is now `runtime/host/hle.c`, built as `var-m5.exe`, with the prompts change in `.deps/GXRuntime` (patch regenerated). Each fix passed an independent checker; patches in `build/fix/<key>/`, reports in the session's r5-fixes.txt:
- scenewords (1.diff): one "a way out waits" rule for both players: a zone's room request during the other's scene, words, find, close-up or hold is refused, remembered and let go through the game's own scan when that's over;
- finisher5 (1.diff): one body, one finisher (event 193 answered 0 and event 202 passed over for the other's victim). Player 1's finisher matched solo frame by frame;
- moves8 (1.diff): moves7 narrowed to answers that landed a weapon on bare hands (Lindsey and Michael only), by majority;
- doorcasts (4.diff): no quick casts or player 2 Tome for the non-owner in a room change; player 2's CLIMB DOWN camera;
- follower (2.diff, hand-merged): the follower's first look passes over a way back's prompt and room change; player 1's look waits until placed;
- prompts (GXRuntime interp.cpp): player 2's DETAIL in their half while player 1 is on a prompt.

Not merged: grab (player 1's B freed a grabbed player 2). Round 5's chapter checks on var-m4 found: player 2's pickup words with no close-up never close (ch5 DESK, since finisher4: a regression); player 1 placed inside room 158's way back after following player 2 through ch7 1213 (since round 4); player 2's Tome crash in ch11 (old); player 2's camera after player 1's scene 1538 and player 2 not held by their own (old). Round 6 (agents) takes those, the grab, and a 12-chapter check of var-m5.

Round 4 (2026-11-06, agents) is in `build/fix/merged4-hle.c` (md5 6e83cec2bc3a1dd64a3bcc524de28caa), which is now `runtime/host/hle.c`, built as `var-m4.exe`. Each fix passed an independent checker; the patches are in `build/fix/<key>/`:
- p1scene2 (7.diff): player 2 walks through player 1's scenes; the other player's exit during a scene that isn't theirs waits for the scene and its words, with no lock;
- finisher4 (1.diff): player 2's FINISH HIM, which completes (`coop_held_weapon`, events between turns in player 2's own words);
- moves7 (1.diff): Lindsey and Michael swing a sword (their part 42 in place of the missing 14);
- presswait (2.diff): a B during the other's close-up or scene is kept and runs after it;
- doorhold (6.diff): the other player walks through a door's countdown and fade, and follows;
- hurt (final.diff): the ch12 shield hits player 2 once per touch, with knock-back; a player held in words, a close-up or a Tome isn't attacked;
- tomewords (3.diff): player 2's close-up behind player 1's real Tome, each with their own words and buttons.

What the checkers asked to change (round 5): one body, one finisher; the held exit to fire when let go, and an exit during the scene's trailing words; moves7's choice kept to cases that would get bare hands; player 1's quick casts and player 2's Tome during the other's room change. Old bugs re-found on var-r3 by the chapter checks, all in `docs/coop-no-pause.md`: the follower arriving inside the way-back door; player 2 grabbed with BREAK FREE in player 1's half; player 2's camera after player 1's scene 1538 and after CLIMB DOWN; words carried through a door. Smoke-checked on var-m4: ch2 statue by player 1 with player 2 walking through scene 2242, and ch2 examines and the zone exit each way.

**The co-op sweep, Pius's chapter, 2026-11-05 night, uncommitted, not yet played by the user:** `tools/ed/sweep.py` (`ed` skill). In room 341 (the dais, the wall) and 342 to 344 (the blocks, the corpse), both players' examines and pickups were whole in their own half with the other walking, both cameras back after. What it found and fixed is in `docs/coop-no-pause.md` (the five rows of 2026-11-05). Since: player 2's PICK UP is theirs (the find drawn and named for them, the notice to player 1) and words in their close-up carry their name; rooms 342 to 344 pass with either player first. The urn tests point at `saves/lever.state` and pass (16 of 20). Not done: the pit in 341 (its zone overlaps others), rooms 183 and 182 (floors at many heights: the runner can't place players there yet), room 342's skeleton scene (script 779 on SKPlane01, its plane's place isn't read: a player has to walk the corridor to y -1565, and the stick's way turns with the camera), and chapters 2 to 12. **The user's choice, 2026-11-05:** the Health, Sanity and Magick help pages that open the Tome by themselves stay off for everyone (`meter_help_off`): they only explain, mark themselves seen and highlight the meter, and a forced Tome is what paused play. Later, perhaps a quick notice in the player's own half that doesn't disrupt anything: not yet designed.

**Player 1's camera after a Tome, 2026-11-05 evening, uncommitted, not yet played by the user:** closing a Tome in co-op left the game's camera on the Tome's own record, so player 1's view stood still until a room change, in every chapter, and a reload for a change of characters then showed player 1 black. Fixed in `kTwSwitches` and `camera_index_heal`; `docs/game/systems/camera.md`. `ed check` now flags it (`CAMERA ... record 8`). The 6 kit tests that fail load saves that now hold chapter starts: `lever.state` still has the urn, room 147.

**Everyone at full detail, and Chandra's two forms, 2026-11-05 afternoon, uncommitted, not yet played by the user:** the game lowers detail for three reasons, and each is now handled; `docs/game/systems/graphics.md`, Character detail. Every pair of cameras and solo read 1.0 for everyone (`build/fxtest/detail/`, pictures in `Screenshots for Claude/detail/`). Chandra, her ghost and Mutated Chandra are forms worn by Alex on Karim's screen. Her own body broke until fitting by place went by where bones are, for a body turned otherwise than its wearer (`docs/coop-failed-attempts.md`). Not done: the user's own play, her portraits on the character screen, a whole-game tour with the monitor. The user wants the original camera kept on (`camera = 0`). `ed test` uses `settings.ini`: run it with `ED_SETTINGS` on a copy with co-op on.

**Ghost forms' blue sparkles, 2026-11-05, uncommitted, not yet played by the user:** `ed system players`, Bodies. The user turned down the full-body glow (`fn144`), and any aura or mist, for clean sparkles alone. Seen in pictures: Edward's ghost and Max's spirit walking, solo, Max's spirit and Roberto's ghost in co-op, through a door and scene 64, and none on Max himself, the Liche or the Bishop. Their co-op sparkles face player 1's camera, as the torches do (a known problem, paused by the user): round sprites hide that, so nothing was done.

**Shadows jumping in Karim's first room, the user's report of 2026-11-05, fixed in a scripted walk, uncommitted, not yet played by the user:** nothing had been overwritten: the renderer patch was unchanged since `f190c92`. Room 338 has 8 casters (both players and the corpses) and `kMaxTiles` was 8, with everyone's strongest shadow first, so second lamps got a tile on only 32 of 9472 tiles. The easing between two lamps needs both drawn: with one tile, the moment the two weights crossed, the shadow changed lamp in a single frame, and lamps either side of someone turned it right round. Before: `logs/run-20261105-012705-karim-shadow-review`, the largest changes in the mask around 11125 (player 2), 11301 (player 1) and 11542 to 11544 (both), and `Screenshots for Claude/shadow-review/p2-after-11125.jpg`, shots 64 to 65. `ed flicker` reports 0 spikes on that run, so it doesn't catch this.

The fix, in `fill_shadow` (`gxcore_draw.cpp`, `lampShown`, "Who gets a place") and `shadow.cpp`: 12 tiles in a 4 by 3 atlas; first shadows, then the second lamps of whoever moved in the last 2 seconds (`kMoveApart`, `kMoveMemory`), then the rest; a shadow keeps its tile until it has faded out, and a new one comes up from 0 (`kShownStep`), so with no tile to spare a change of lamp dips and comes back. After: `logs/run-20261105-013730-karim-slots-after`, the same walk, largest mask step player 1 7.40 to 3.07, player 2 10.66 to 4.61, with those frames no longer among the largest. Cost, 40 seconds standing in one run of each: about 0.9 ms a frame on the GPU, still 60 pictures a second. On this build `ed chapters 4` was run (`Screenshots for Claude/chapters/slots`). Not run, at the user's word: `ed test`, and the other chapters.

**Left as it is:** someone walking within 80 units of a corpse merges with it into one caster for a moment (`-475,-716 r262 sets 2` at game frame 11581, the largest step left). The pictures through it looked fine.

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

**Forms, late 2026-11-04, uncommitted, photographed but not yet played by the user:** weapons gone from every ghost, eyes and Anthony's jewel on the face, Augustine renamed The Bishop as the user asked (he isn't Augustine), "Original Form" for a character's own body, the form on each frame and a heading in the Tome's style beside them (its divider and marks, `form_panel_rml`), and a larger character screen. The Bishop's and the black robes' faces and Karim's and Roberto's hands were checked in photos while walking, not in a fight or a sprint.

**Other forms for characters (built 2026-11-04, not yet confirmed by the user):** each form has its own frame on the character screens (since 2026-11-06; before, LB, RB or C on the character's frame), for either player: Pius's skeletal body on his human moves, Ellia's ghost and rotten body, Anthony's ghost and rotten body, Paul's ghost, the Bishop, monk and black robes, Karim's, Roberto's and Edward's ghosts, Max's spirit. Weapons built into a ghost are left out, each form has its picture (`tools/ed/form_portraits.py`), and Pius's cape holds the Liche's standing pose. `Bodies` in `ed system players`.

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
