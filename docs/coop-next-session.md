# HANDOFF 2026-11-18 (evening): START HERE. Everything is committed on tools/game-kit and NOT pushed (past v0.1.6; the evening's two commits are items 1, 3, 5 and then 2a, 2b, 6 of the board); controls.cpp's test edits stay uncommitted by rule. What to play: docs/coop-play-test.md

## The status board (keep it true: every session marks what it finishes)

**The marks, at the start of a line, in this file and in docs/coop-todo.md:**
`[FIXED]` a fault put right, `[DONE]` a piece of work finished, `[OPEN]` not started or not finished,
`[NOT VERIFIED]` built but not seen working, `[DECIDE]` waits for the user. A `[FIXED]` or `[DONE]` line says in one
sentence HOW it was done and where (function, file, commit), so nobody rebuilds it, breaks it or checks it again.
When you finish something, change its mark here in the same commit as the work. Never delete a line: mark it.

### How the user wants this worked on (their words of 2026-11-18, in priority order)
1. **Use every tool, skill, kit and table that exists before fixing anything in the co-op mod.** The `ed` skill first
   (`python -I tools/ed/ed.py keep`, `how`, `verb`, `system`, `needs`, `host`), the pages in `docs/game/systems/`,
   the tables in `tools/ed/*.tsv` and `tools/ed/facts/`, `docs/coop-failed-attempts.md` before writing a fix. The
   game already has the function: find it, don't rebuild it. (2026-11-18 showed why: Karim's shotgun was "fixed" at
   first with another character's moveset, against the mapped rule, and the user had to refuse it.)
2. **Solo play is not touched** unless the task says so (a character fix, a menu that solo has too). Gate co-op work
   on co-op, and run the solo case as well.
3. **Fixes are for the whole game**: every chapter, character, form and both players. Never a chapter's or a
   character's special case where a general rule exists.
4. **Few test runs. No fix-break-fix-break.** Read the code and the tables first, build once, then one run that
   records both players (`sh tools/ed/run.sh`, `sh tools/ed/jtg.sh`, `ed timeline`, `ed check`, `ed stops`). Where
   reading is wide, use several agents at once on separate questions rather than many runs.
   `ED_RUN_AWAY=1` on every run. `JTG_EXE` only works with `JTG_SLOT=1` (without it the run uses today's exe).
5. **The goal is a true co-op game**, as if made for two, that runs smoothly: optimise what is built; the tables,
   ids, doors, sanity effects and the co-op code that exist make it reachable without breaking features.
6. **Usage: check the usage tracker (`mcp__ccd_session_mgmt__get_usage`) before starting and after each step. At
   95% of the weekly Fable limit or of the weekly all-models limit: stop at a commit, update this board, write the
   hand-off, and tell the user**, so they can reset the counts with nothing half done.
7. Plain words, pictures first, no questions the user has to decode. A weapon plays its own chapter's character's
   moveset on every body: never swap in another character's.

### Open work, in the user's order
- `[NOT VERIFIED]` **1. A high-pitched break in the sound when the game loads from the title screen** (the user, 2026-11-18).
  Built 2026-11-18 (evening), confirmed in a recording, NOT yet heard by the user. What it was: the host presses A for
  the player on the memory card screen that is passed over (Start Game, Jump to Game, Load Game), and the title's own
  choose sound for that press (636, from 0x8001AE0C) played alone, a 3 ms crack on top of the chapter's first music.
  How: `skip_card_inserted` / `skip_card_select` note the frame of their press (`g_card_press_frame`) and
  `tome_copy_sound` (hle.c) leaves that one sound out within 30 frames of it (log: "[title] sound 636 ... left out,
  nobody pressed"). Seen: the recording before has the sound three times (two presses and the stray one), after only
  the two presses (logs/run-20261118-140502-jtg-sound-title-jtg against -142146-jtg-sound-fix; ED_PCM_OUT); the same
  line on Start Game's path (-142354-jtg-dream-change-keep). The disc music's decode was checked sample for sample:
  clean. `[OPEN]` still, if the user still hears a break: the speakers' buffer runs empty for about 270 ms on the first
  frames of the first room of every start ("[audio] ran dry after a 270 ms pause", logs/play.log line 79 and every
  title-to-game run): the game thread is held that long and the sound is paced by it (interrupt.c `interrupt_poll`).
  The user can press M in play when they hear it (controls.cpp writes logs/mark-N.wav with a DRY table). Before this:
  Not looked at. Where to start: `docs/research/firstrun-hd-flash/title-music.md` (the title's music under the pages,
  "[audio] ran dry"), `runtime/host/audio.c`, and what changed last at the title: 9d0f752 (the Choose Character
  row's loop sound) and the Solo / Co-op picker's restart (P3, P4). Memory: co-op must not restore MusyX globals.
  One logged run from the title into a game (`sh tools/ed/run.sh launcher NAME`, ED_DTK_LOG is on) before any change.
- `[DONE]` **2a. "Remastered spells" reworked (2026-11-18 evening, the user's answers the same day).** The first one
  (3D rune stones, their flames, the streaks' colours, textures-spellfx; setting `spell_fx`) is out of the code. The glow
  the user likes (setting `spell_rim`, once shown as "Spell glow") is now shown as **Remastered spells** and also turns
  on the spells' own light: each flat rune glyph of a cast lights the surfaces round it in its alignment's colour
  (`spell_lights_frame` in hle.c sends them through the renderer's `set_spell_lights`; each light in its caster's
  room's half while apart). The user's first look: "a round red cloud just sitting on top of the runes". So the
  renderer was changed the same evening (shadow.cpp, the patch regenerated): the light is no longer added over the
  picture with the haze; a pass of its own (`spell`: distance from the rune's edge, which way the surface faces, a
  little shine, none behind a character or pillar) is drawn with a blend that MULTIPLIES the picture (`spell_add`,
  the `lighting` blend: picture x light + picture), so stone keeps its pattern and takes the colour; the round bloom
  pool is gone and only a tenth of the light is in the air. Off: the original look. The light's strength is 1.4 with reach 60 (the user
  found the first 1.8 / 95 too bright on the ground, the pools bleeding together; `ED_RUNE_LIGHT=B,R` tries others
  with no build). Cost: about 0.2 ms of GPU time a frame while a spell is cast. Seen: Karim's Chattur'gha Recover in
  chapter 4, co-op, on and off, HD textures on and off (logs/run-20261118-15*-jtg-spell-*; sheets in Screenshots for
  Claude/remastered-spells/: 07 the cloud against the surface light, 09 all four colours by Alex in the last
  chapter, logs/run-20261118-15[45]*-jtg-alexz-*). **Test spells with Alex's last chapter only** (`sh tools/ed/jtg.sh
  12 NAME` with `ED_QUICK_SET=FRAME,0,0x0001104N` and Y at a later frame; N 1 red, 2 blue, 4 green, 8 purple): the
  Tome and every spell are hers from the first frame (the user, 2026-11-18; Karim has spells but not every rune,
  Anthony none that early). Not seen: larger circles, a cast while apart, solo, the user's play (they have not said
  yet whether the surface light is right). Not
  done: with Glow or HDR on, spells still bloom as every bright thing does (the Glow option's own doing).
- `[DONE]` **2b. The heavy options measured, then cut or eased (2026-11-18 evening).** Measured one at a time on top of
  everything-off, the user's settings (1920x1080 at 2x), chapter 4's start room, GPU ms a frame (`AURORA_GPU_TIME_LOG=1`,
  "[gpu-pass]"; logs/run-20261118-14*-jtg-gfx-{coop,solo}-*; pictures in Screenshots for Claude/graphics-options/).
  Co-op, off 3.1: Occlusion High +2.5, Haze +1.1, Contact shadows +0.9, Light shafts +0.8, Character shadows +0.7,
  Glow +0.6, Occlusion Standard +0.3, SMAA +0.25, Sharpening +0.2, HDR / Per-pixel / Skin 0; everything on 8.8. Solo
  (120 pictures a second), off 8.6: every option under +0.8, everything on 10.4. Nothing dropped the game below 60 in
  that room. Done: **Contact shadows removed** (no change seen in the picture, and with it on the long character
  shadow was missing); **Light shafts removed** (a faint halo for 0.8 ms); **Occlusion quality High is solo's alone**
  (co-op draws Standard). How: settings.c `settings_apply` holds shafts and contact at 0 and passes `ssao_high &&
  !mod_coop`; the rows are gone from frontend.cpp; the renderer's code for them is unreached, not deleted.
- `[OPEN]` **2c. What is left of the graphics work.** (1) The whole game in pictures (`ed chapters N`, before and
  after): not run, saves 4 to 12 are in saves/old-2026-11-09 and are old states (make fresh ones as `ed test` does).
  (2) The biggest saving left in co-op: every effect runs three times a game frame (each half and the game's own
  frame, which nobody sees); leaving the effects out of the hidden frame would take about a third off each (common.cpp
  `render`, the `passInfo.glowScene` block; care: glow::take_scene and the EFB copies the game reads). (3) The
  renderer's unreached parts (contact shadows, shafts) and the folder textures-spellfx/ with its three tools can be
  deleted. (4) Measure a heavy room (many casters, room 338 with enemies) and a spell-heavy fight.
  The first wording of this item, kept: (a) Take "Remastered spells" out (settings `spell_fx`; frontend.cpp
  `enhancement::kSpellFx`; its renderer side in .deps/GXRuntime) and bring back the original look of spells, with HD
  textures on and off. Decide with the user what happens to "Spell glow" (`spell_rim`) and to the spell lights by
  room (P7, built on Remastered spells only). (b) Contact shadows and the other heavy options (haze, light shafts,
  occlusion, character shadows) cause slowdowns, high GPU use or do nothing in play: measure each
  (`ed graphics [SAVE] [OPTION]`, the FPS counter, `ED_DETAIL_LOG`), then optimise or remove. (c) The aim: the game
  looks like a beautiful remaster, breaks nothing, and works the same in co-op; changing these options in co-op
  broke features before, so each is checked in co-op too. Judge on the whole game in pictures: `ed chapters N` for
  saves 1 to 12, before and after (`ed system graphics`, `docs/game/systems/graphics.md`).
  **Read 2026-11-18 evening (nothing changed yet):** Remastered spells off is NOT the original look: Spell glow
  (`spell_rim`, on by default: a rim on spell meshes and 63x63 sparks into the glow) and Glow / HDR bloom (every
  additive draw, the streak textures always registered as emitters, settings.c:449-453) still change spells; the 64
  HD spell textures are the pack's own, untouched by the remaster. The removal list: settings.h:33,
  settings.c:38/176/373, frontend.cpp's kSpellFx rows, texpack.cpp:234-422 (the textures-spellfx parts), hle.c's runes
  block 10016-10568 with P7/P7b's rune lights, the renderer's rune lights (shadow.cpp, shadow.hpp, gxcore_draw.cpp,
  aurora_backend), then the patch regenerated. Costs logged once (2026-11-04, logs/run-20261104-1401*-time2-*, GPU ms
  a frame): all off 2.4-3.5, all on 11.2 co-op / 14.3 solo; occlusion 1.2 (the largest zone), sharpening+SMAA 0.94,
  character shadows 0.26-0.41, glow 0.23, haze 0.1; about half the cost is unzoned (per-pixel lighting, glow's second
  draws, the shadow atlas). Contact shadows showed no visible change on slot 1 (chapter-tour-review.md:11); light
  shafts are "only a soft halo". In co-op the whole chain runs for each half AND the hidden game frame. Measure with
  `AURORA_GPU_TIME_LOG=1` ("[gpu-pass]" lines) on Jump to Game runs, solo and co-op, one option at a time. The
  user's own settings.ini already has occlusion, contact shadows and haze off, glow 2, HDR on.
- `[DONE]` **3. Jump to Game on the title screen asks Solo or Co-op** (2026-11-18 evening). How: `title_main_start_load`
  (hle.c) lets the game's row 3 through as well as 0 and 1 (`TITLE_JUMP_ROW`; `ED_TITLE_RESUME=jump` for the restart),
  and frontend.cpp carries the row itself (`g_open_page_row`, `pick_title_row_name`, restart word "jump", test door
  `pick-jump`). The page comes before the chapter grid, so only the row crosses a restart. Scripts are unchanged:
  jtg.sh answers with `ED_TITLE_PICK`. Seen: the page under the heading Jump to Game, Co-op picked from a co-op start
  goes on into chapter 1 (logs/run-20261118-141958-jtg-jump-pick-coop), Solo picked from a co-op start restarts and
  opens the chapter grid in solo (-142236-jtg-jump-pick-solo-restart); pictures in Screenshots for Claude/
  jump-to-game-pick/. Not seen: Back on the page, a real controller, the user's play. Was: does not ask Solo or Co-op.
  Start Game and Load Game do (the
  picker, steps P1 to P7 below: `FRONTEND_PAGE_TITLE_PICK`, hle.c `title_main_start_load`,
  `docs/research/solo-coop-picker/plan.md`). Add the same page before Jump to Game's chapter grid.
- `[NOT VERIFIED]` **4. The shotgun on Karim: the reload**, and Roberto, Lindsey and Max with it (same fix, not run).
  See the Karim section below. One logged play by the user settles it (Z once in the open).
- `[FIXED]` **5. Changing character in the middle of the opening dream drops the dream's own mode** (2026-11-18
  evening). Cause: the reload is the game's own end-and-load (0x80022328 -> 0x80021648), which puts three words back to
  a new game's that no save carries and only a script's start sets: the screen path (fn46, r13-25672: the dream's and
  the other-era rooms' colours), the effects gate (ed276, r13-29000: the bars) and the prompts-off bit (ed277,
  r13-24044). How: `reload_keep_take` / `reload_keep_put` in hle.c, round `characters_reload`'s GAME_END: read before,
  put back with the game's own setters (0x80044190, 0x80119090, 0x80117288) once play is on again. Any chapter, either
  player's change, no dream test. Seen: solo new game, Alex to Karim at frame 2300 in the dream, "the reload is back
  in play: screen path 0 (was 1), effects gate 1 (was 0), prompts off 0 (was 1): put back", Karim with the Shotgun in
  sepia with no bars (logs/run-20261118-142354-jtg-dream-change-keep; Screenshots for Claude/dream-character-change/).
  Not seen: co-op and player 2's change (the same path), the user's play. Not carried: the dream's own clock
  (0x80323F1C, 1400 ticks to script 2027) starts over at a reload, so the dream runs longer after a change. The input
  mode (ed56's byte 0x80323AEB) is in the save and the game sets it again itself. So FINISH HIM in the dream (item 7)
  after a change was the prompts-off bit lost, not a new rule. Was: (the sepia look
  goes, the bars come on): seen in logs/run-20261118-131413-karim-solo-test at the reload. `characters_reload` (hle.c).
- `[DONE]` **6. `ed test` passes again, from fresh Jump to Game states (2026-11-18 evening).** How: tools/ed/scenarios.py
  makes each start's state itself (`STARTS`, `fresh_state`: a cold boot through the title and Jump to Game, a state
  saved with both players standing, kept as saves/test-START.state and made again whenever the exe is newer; a bad
  start is retried), runs every scenario as the user plays (two room slots, the other player staying), moves each
  scenario's frames by the fresh state's, never starts over a game the user has open, and counts a missing probe as
  a FAIL. Result: `5 of 5 passed, 18 skipped (not run)`: alex-both-walk, alex-spell-cast, alex-tomes (new, Alex's
  last chapter), shadow-karim, shadow-steady (chapter 4).
- `[OPEN]` **6b. Eighteen scenarios are SKIPPED, not passed**: the seven lever ones, the door, ladder, desk and urn ones
  were written for Anthony's bookshelf room 147 on a save of the user's. From a fresh chapter start that room is past
  the Tome: the plane's script 892 sends Anthony to the Tome's room 5 until that room's own Tome (tag 733) is his, and
  three scripted tries at that pickup did not get it (the user then said to stop: test in Alex's last chapter, where
  the Tome and every spell are there from the first frame). Write each again in Alex's chapter (start `alex-100`): a
  scene of one player's with the other walking, a door with the other staying and with them following, a ladder, a
  find, an item used from each Tome. Each keeps its old text with a `skip` line saying why.
- The first wording of item 6, kept: **6. `ed test` has not passed since the launcher work**: it stopped in `test-shadow-steady` on a missing
  `probe.txt` (last had one 2026-11-13) and was then cut short so the user could play. Run it (`ED_RUN_AWAY=1`),
  find why the probe file is not written, and get it green before the next co-op fix.
  **Found 2026-11-18 evening (read, nothing changed yet):** not the launcher. Since P3 (213a350, 2026-11-17) a boot
  state smaller than 56 MB makes the run SOLO (`coop_follow_boot_state`, frontend.cpp), so all 20 scenarios, which load
  33 to 41 MB states of 2026-11-04, have run without player 2 since; the probe file is written only for split views,
  and scenarios.py:91 crashes on its absence. Also saves/slot7.state is gone (door-open, costume-door) and
  saves/slot1.state is the user's own slot now (both-walk, door-climb): those four failed before too. The user's rule
  (2026-11-18): `ed test` starts from Jump to Game and makes fresh states on today's exe, never old states. Plan: each
  scenario names its start (chapter, door scripts, puts), the driver makes one 56 MB state per start per exe with
  jtg.sh + ED_SAVE_STATE (as tools/ed/check.py already does) and runs the scenario from it; two starts (chapter 3 room
  147 by door script 892, chapter 4 room 338) cover 18 of 20; frames become relative to the state's.
- `[DECIDE]` **7. FINISH HIM inside the opening dream for player 1.** The game does not draw it there (its chooser
  0x80117628 takes slot 3 only on the walking or R-held channel; the dream has its own input mode). Player 2 has it
  there in co-op because the host draws theirs. Left as the game has it. Do not ask the user about this again
  unless they bring it up.
- `[NOT VERIFIED]` **8. The new launcher** (section below): a real mouse and controller on the tabs, Fullscreen,
  a key rebind on Button mapping, Duplicate and Delete of a slot, the Update button's own press, a Tome page's
  picture after the change, HDR calibration from the Display tab, the Mac and Steam Deck builds.
- `[DECIDE]` **9. The launcher's open choices**: whether Continue stays the third button; what Credits shows; the
  exact look of the toggles (the user will send a photo; step4-toggles.png is where it stands, they said it looks
  good); a larger backdrop picture.
- `[OPEN]` **10. Push** (ten commits past v0.1.6 are local). Only when the user says: fork/main, never origin.
  A release then needs `res/version.txt` raised.
- `[DONE]` **12a. Each player's own sanity effects, the first step (2026-11-18 night).** Player 2 has a sanity set of
  their own (hle.c `SanityOwn`, `sanity_set_io`: the tracker's 200-byte holder and its node's state, the 44 x 36
  table 0x80317638, the clock 0x80317C68, the data 0x80317C8C, the screen object 0x80317DF0, twelve r13 words). Once a
  frame `sanity_p2_step` puts it in, runs the game's own tracker update (0x801F5D48) with player 2 the player
  (`coop_state`, `coop_be_player2`, their cameras and room) and takes it out. In that update the level function
  0x80069018 is answered by `sanity_level` (their level from their own meter; no music, no story scenes twice), an
  event the tracker sends itself for later is kept and given in a later update of theirs (`sanity_later`, called from
  `split_send`, which already has 0x801F57C0), and the rules answer "not allowed" for every effect but 1, 4, 5, 8,
  24, 31, 38 (`kSanityP2`). The overlay's draw 0x800AC71C runs once for each player with their set in
  (`sanity_overlay`), player 2's moved across into their half as their words are (`coop_subtitle_away` on the
  identity loaded at lr 0x8019D068). Seen: player 2's meter at 10 of 75 reads level 3, effect 31 (TV volume) asked
  for in their set is picked, started by the kept event 84, runs 384 frames and ends, the VOLUME bar in their half
  alone, player 1's tracker at rest and player 1 walking (logs/run-20261118-165903-jtg-sanity2-vol6; Screenshots for
  Claude/sanity-per-player/01; `ed test alex-sanity-p2`). Test doors: `ED_COOP_P2_SANITY=N`,
  `ED_SANITY_P2_TEST=FRAME,EFFECT`, `ED_SANITY_P2_LOG=1` (why no step), `ED_SANITY_P2=0` (off). Solo untouched.
- `[DONE]` **12a, seen the same night: the game's own roll for player 2, nothing forced.** With their meter at 10 of 75
  and player 1's full, the game's clock (3600 ticks) picked TV Mute (24) for player 2 at frame 5058, ran it 216 frames,
  ended it, and 3600 ticks on picked Bleeding Walls (1); player 1's tracker stayed at rest throughout. MUTE is in player
  2's half alone (logs/run-20261118-170714-jtg-sanity2-roll; Screenshots for Claude/sanity-per-player/02). The blood
  of effect 1 was not found in the picture (room 100's far camera): not judged.
- `[DONE]` **12a, later the same night.** Player 2's set is stepped at the pad read (a hold of either player's does not
  stop it); an effect of theirs that holds play holds THEM alone (`g_sanity_p2_busy`: TV Shut Off ran 165 frames with
  player 1 walking, their whole half black); an effect of theirs that goes on in the background ends when they come
  into another room (Blood Drip, seen); 6 and 7 are allowed them (their controller state is in for the update);
  `ED_SANITY_P2_TEST` takes several, `F,E;F,E`. Pictures: Screenshots for Claude/sanity-per-player/01 to 03.
  **The plan for everything left, with every address the readers found, is `docs/research/sanity-per-player/plan.md`:
  read it before the next step** (door effects for a door player 2 takes, then the whole-game screens in one half by
  the Tome copy's way, then the world effects' halves, sound, save states, the menu).
- `[DONE]` **12a, the door step (2026-11-18, late).** A door player 2 takes with player 1 staying has THEIR sanity set in
  for the whole of the mod's bracket (`sanity_p2_door`, called from `notify_ticket_run` and `coop_stay_change_end`),
  with their own link to the tracker (`sanity_p2_link`: info +140, then +68, which the game never set for them), so
  the game's door code looks at their waiting effect and starts it in their new room. Bug Crawl (14) is allowed them:
  seen picked, waiting, started at their door into room 51 and a bug on their half alone, player 1 in room 100
  (logs/run-20261118-173901-jtg-sanity2-door4; Screenshots for Claude/sanity-per-player/04). Its bugs are an effect
  object (draw 0x80192F10), moved into its player's half in `effect_draw_by_room`. The rule that ends a background
  effect at their next room leaves alone an effect that very door started. In the test the start was forced whatever
  the next room's list says (`sanity_door_rules`, only for `ED_SANITY_P2_TEST`'s effect); in play the game's own rule
  stands (the next room must have a list), not seen yet. Follow mode has no bracket: not done there.
- `[DONE]` **12a, Phantom Creatures (21) and Locked in Room (10) allowed player 2.** 21 seen: asked for in their set,
  started at their own door, three phantoms round them in room 51, none in player 1's room 100, player 1 walking
  (logs/run-20261118-174701-jtg-sanity2-phantoms; Screenshots for Claude/sanity-per-player/05). 10 not run. In a
  SHARED room both players would see the phantoms: not kept to a half yet.
- `[DONE]` **12a, the first whole-game screen for one player: TV Video (25).** When it waits in player 2's set and they
  press Start, the host does mode 8's work itself with the game's own calls (in `sanity_p2_step`: event 84, the play
  hold as them, the sounds at ticks 46, 66, 140, 160, at 300 event 81 and the flash 0x8006B270; drawn in
  `sanity_overlay`: 0x8019CFD4(5), the black as TWO half-height rectangles from -106, 853 across, the green word at
  455, 31). The game's mode is never asked, so the world and player 1 go on; their Start is not their Tome's while it
  waits (`g_sanity_p2_takes_start` in `coop_p2_own_up`). Seen: their whole half black with VIDEO, player 1 walking,
  let go after 300 frames (logs/run-20261118-180717-jtg-sanity2-video4; Screenshots for Claude/sanity-per-player/06).
  Lesson: ONE rectangle of the whole screen is a "whole screen" picture to the split and is lost from both halves.
  Blue Screen (28) and Game Reset (26) need UInsane's pictures (plan.md): the same way, with the picture as two
  halves or a marked matrix. Not heard: the sounds (runs are muted).
- `[OPEN]` **12c. The user's reminders (2026-11-18, late): the body falling apart with the head that is picked up and
  recites (17), and Walk on Ceiling (22) where the chapter calls for it, for each player on their own.** Both are
  door-started and return to the room before: `docs/research/sanity-per-player/plan.md`, "The user's reminders".
- `[OPEN]` **12b. What is left of it, in order.** (1) NOT SEEN YET: effects 1, 4, 5, 8, 38 for player 2 in a picture,
  and the user's play. Effect 4 (TV shut off) draws
  full-screen rectangles, which the renderer may not bring back into a half: look first. (2) Player 1's own overlays
  when the cameras are lent. (3) World effects kept to their player's half in a shared room (blood 1 and 5: effect
  objects made by 0x801413E8, drawn through 0x8017C1D8; `effect_draw_by_room`'s marks). (4) The effects that need
  the player's controller state or a door (6, 7, then the door-started ones with their return, for whoever took the
  door). (5) The whole-game screens in one half (15, 25, 26, 28: the user wants them per player, e.g. a fake blue
  screen for one while the other plays on; the Tome's copy of memory is the only thing that runs a whole-game screen
  for one player today). (6) Sound: a mute or a volume fall is for both players now; the sanity music follows player
  1's meter. (7) Save states do not carry player 2's set (made again after a load: their deck starts over). (8) The
  Mods menu's Sanity Effects for either player in co-op. (9) Events other code sends to the tracker outside their
  update (a script's event 82) reach player 1's set.
- The first wording of item 12, kept: **12. Independent sanity effects for each player in co-op** (the user, 2026-11-18; docs/coop-todo.md's first
  section). Read 2026-11-18 evening, nothing built: the game has ONE tracker (the thing of kind 32, handler 0x8006CEAC),
  one clock and one deck (41 records at r13-25476, the same table for the whole game; chapter limits are code,
  0x8006EB6C; each room file has its own list), watching player 1 only. Player 2 has their own meter and bar already
  and nothing rolls for them. The vehicle that exists: player 2's own set as their words have (`CoopWordsOwn`,
  `coop_words_tick`: a kept copy put in, the game's function called under `coop_be_player2`, put back), and for the
  picture the two ways a draw is kept to one half (the +4096 move of `flash_submit_notify`, the out-of-view matrix mark
  of `coop_found_mark`). Steps, one run each: (1) player 2's level from their own meter, no rolls; (2) their rolls for
  the picture and camera effects (1, 4, 5, 6, 7, 8, 14, 24, 31) kept to their half; (3) player 1's kept to theirs;
  (4) door effects for whoever took the door; (5) the whole-game screens (15, 25, 26, 28) and the sound by the user's
  answers; (6) the menu. `[DECIDE]` the user's answers on the whole-game screens, body effects, shared-room things
  and sound are asked 2026-11-18.
- `[OPEN]` **11. Everything older that is still open** is in `docs/coop-todo.md` ("Open issues", "Requested
  2026-11-13") and `docs/coop-glitch-todo.md`; the co-op sweep chapter by chapter; the kit session
  (`docs/prompts/session-kit-mapping.md`). Put the marks on those lists as each is touched.

### Finished on 2026-11-18 (do not redo, do not recheck)
- `[FIXED]` **Karim (player 2) drawn out in spikes and sheets at the start of Alex's last chapter** (the user's own
  frame, 2026-11-18 evening, Jump to Game 12 with Pius and Karim; Screenshots for Claude/karim-distorted/). Not from
  the evening's changes. Cause: a player fitted to another character's weapon moves has bones those moves haven't
  (Karim 24 of 72, Ellia 29 of 77), held at the pose of the move the body plays as the fit is made (`wpn_bind`).
  Two ways they had no pose and the game drew each by its whole place as made on top of its parent's: (1) the weapon
  is in hand as the body is made (Alex's last chapter), so the fit came before any move had played ("0 held");
  (2) after ANY save state's load with such a weapon in hand, the fit is made again and the keys held before were
  host memory only ("as modelled"). How: (1) `wpn_serve` answers "not yet" while the body plays no move at all
  (`wpn_a_move_plays`), 90 frames at most: seen 33 frames, then "24 held"; (2) save states carry the fits (section
  COWP: `hle_wpn_save`, `g_wpn_loaded`, read by `wpn_bind`'s `kept`): "held as the fit before held them ... 29 held"
  after a load. Seen: logs/run-20261118-162907-jtg-karim-fit-final; `ed test` 5 of 5 (its Alex scenarios run from a
  loaded state with Ellia holding Alex's weapon). A state made before this build has no COWP: load it and it is as
  before; make it again. Not seen: the user's play of it; the other characters there.
- `[FIXED]` **Karim (and Roberto, Lindsey, Max) did not hold the opening dream's Shotgun** (their arms froze).
  How: `wpn_bind` (hle.c) keeps a joint with a limb under it on the weapon's own moveset when it sits 45 to 90
  degrees from the home's as made, and a weapon asked for twice keeps the first fit's held bones. Commit c6e4c63.
  The user saw the hold in play. Not last night's b2fdc04 and not the launcher: the 2026-11-15 exe had it too.
- `[DONE]` **The new main launcher** (mock-up 7): side bar of tabs, Play page, Check Updates, styled toggles,
  first-start and no-disc pages, 4:3 / 16:9 / 21:9. How: `launcher_dark()` in frontend.cpp, `body.launcher-dark`
  in menu.rcss, updatecheck.cpp. Commits f79b175 to 602ee61. The in-game menu and the title pages are unchanged.
- `[FIXED]` **The Tome character-change freeze; "not created by Eternal Darkness" on the Tome's Load/Save; the
  launcher menu on Esc / Back; Settings' old Quit row**: the sections of 2026-11-18 further down say how.
- `[DONE]` **The pass-through sweep's ten hooks** (b2fdc04) and its check with the players apart (100848b).

## The hand-off before this one (2026-11-17, morning): all committed through native menus step 6, the Tome look in co-op

## 2026-11-18: [FIXED] Karim's arms on the opening dream's Shotgun (Alex's moveset on any body); committed c6e4c63
- **What the user saw** (their play: logs/run-20261118-123512-karim-shotgun, Start Game in co-op, player 1 Ellia,
  player 2 Karim): Karim did not hold the dream's Shotgun; it stood up by his face and his arms did not follow.
- **The rule (the user, 2026-11-18): a weapon plays its own chapter's character's moves on every body.** The dream's
  Shotgun is Alex's moveset, the only one made for it. Do NOT give Karim another character's (I tried Peter's first:
  the hand off the barrel, the reload floating over his head, the user refused it; the row is gone from `kGunMoves`).
- **Cause** (`wpn_bind`, hle.c): a bone matched by name is left out when it sits more than 45 degrees from the home's
  as made, with everything under it. The only such joints with a limb under them, on the whole disc, are the two upper
  arms: Karim's, Roberto's, Lindsey's and Max's sit 49 to 65 degrees from Alex's and Ellia's (counted from
  `kFinBones*` / `kFinTurns*`), so with Alex's moves both whole arms were left out and held as they stood (18 of
  Karim's 72 bones followed). Pius, Edward, Paul, Peter, Michael and Anthony are under 45 and hold it as Alex does,
  which is why it "worked before" for the others.
- **Fix.** A joint 45 to 90 degrees off with 8 or more bones under it is kept on the home's moves (a track turns a
  bone from its parent outright, as the reload's fit already takes them: `fin_fit`). Hair and a thumb turned right
  round are left out as before. Also: a weapon asked for twice running (player 2's copy: part 77 as it is made, 81 a
  few frames on) keeps the held bones' keys of the first fit instead of dropping them ("held as the fit before held
  them"). Test doors: `ED_WPN_LIMBS=0` the old rule; `ED_GUN_MOVER=WEARER:MOVER` another character's gun moves.
- **Not new, checked properly.** The same scripted start (`JTG_NEWGAME=1 ED_TITLE_PICK=coop JTG_P1=2 JTG_P2=4 sh
  tools/ed/jtg.sh 1 NAME`) on the 2026-11-15 exe and on a build without b2fdc04's ten fixes, each with `JTG_SLOT=1
  JTG_EXE=...` (runs -jtg-karim7-nov15, -karim7-nosweep): the same "18 of 72 bones by name (38 more turned too far)".
  **`JTG_EXE` does nothing without `JTG_SLOT`**: my first three "older build" runs (-karim2-*) all ran today's exe.
- **Seen.** Co-op, run -jtg-karim9-coop-alex: "player 2 (Karim) holds Alex's weapon ... 48 of 72 bones by name (8
  more turned too far), 24 held" at both asks; solo as Karim the same (-jtg-karim5-solo-alex). Pictures in
  `Screenshots for Claude/zoom/` (-karim2-now-1790 before, -karim9-coop-alex-1790 after: the gun low in both hands,
  as Ellia beside him holds hers) and `Screenshots for Claude/karim-shotgun/`.
- **FINISH HIM in the dream, solo (the user: missing as Karim, there as Alex; logs/run-20261118-131413-karim-solo-test).**
  Not a Karim matter. In a fresh dream the game's look for someone down (0x800330E0) passes the same for Karim and
  Alex (same calls, same arguments: runs -jtg-fintrace-alex, -fintrace-karim, `ED_TRACE_FN=0x80033534,0x801F8BC8`),
  and the game asks for the prompt each tick (`0x80117404(2, 23, 2244, ...)` from 0x800C03FC, 77 to 200 times a run),
  but player 1's words are never drawn: as Alex herself too, and the same on the 2026-11-15 exe and without b2fdc04
  (runs -jtg-finold-nov15, -finold-nosweep, each with JTG_SLOT=1). The user's Alex had it only after the character
  change reloaded the game in the middle of the dream, which also dropped the dream's sepia look (so the dream's own
  mode, ed56(7), was gone: a bug of its own). In co-op player 2 has the prompt in the dream because the host draws
  theirs. No earlier log has it drawn for player 1 in room 96. **Why (read in the game's code):** the prompts'
  chooser 0x80117628 takes slots 1, 3 and 5 (FINISH HIM is slot 3) only while the walking channel (r13-23060) or
  the R-held channel (r13-23056) is the one in use (`0x80199F50`, `0x80199F74` -> `0x8013DBD8`); the dream runs on
  its own input mode (ed56(7)), so the game itself does not draw them there. It is the game's rule, not a fault of
  the fit or of Karim. The user took my question about it badly ("that is a literal function the game has"): do not
  ask again; FINISH HIM everywhere else is untouched. Left as the game has it unless the user asks for it in the dream.
  New log line with ED_FIN_LOG: "[fin] the look for someone down: the thing held to fight with is object ..., model N".
- **NOT verified.** The reload (my scripted reload was hidden behind the zombies; the user's test did not press Z as
  Karim). Roberto, Lindsey and Max with the Shotgun (the same joints, not run). The front hand's exact
  place on the barrel (the pictures are dark). `ed test` was stopped after one scenario so the user could play.

## 2026-11-18: [DONE] the new MAIN launcher is built (the user's mock-up 7); committed, not pushed
What the user chose is `docs/research/launcher-redesign/decisions.md`; pictures of every step are in
`Screenshots for Claude/launcher-redesign/` (step1-home, step2-update, step3-display, sheet-*.png).
- **What was built.** Only the launcher the program opens with. A side bar of tabs (Play, Choose Character, Display,
  Graphics, Sound, Controls, Mods, Credits, and Quit at its foot), the lit tab's page in the panel beside it, one line
  of directions at the window's foot, the user's own picture dimmed behind (`res/ui/launcher-art.png`). The Play page:
  the one-line title with its glow, BOUND SOULS, READY (or NO GAME DISC FOUND), the version with **Check Updates**
  (it becomes a lit **Update** when a newer release is out, and then only opens the GitHub Releases page), Play /
  Load / Continue, three tiles (HD textures, Resolution, Frame rate: A changes them), "Recommended settings: coming
  soon" with a greyed Auto-set, both players' controllers with Configure, the save states used, the game disc.
  After the user's asks the same day: the tabs' pages have a large name over a thin line, larger rows on one dark
  sheet with the values in one column, and On / Off is a styled toggle switch (a ticked box was tried and refused; the
  user will send a photo of the toggle they want: picture step4-toggles is where it stands).
- **How it is made** (`runtime/host/frontend.cpp`, the block before `render()`; `res/ui/menu.rcss`, the last block,
  `body.launcher-dark`; `runtime/host/updatecheck.cpp`). `launcher_dark()` is true only while `frontend_run_launcher`
  has its menu up (`g_launcher_up`), so the menu over a game, "Return to launcher" there, the title look and the Tome
  look take none of it. The body wears `launcher-dark` plus the title look's own class `skin-title` (its colours and
  hands), never `launcher`. Tabs open the old launcher's own pages (`open_tab`); a tab's own page has no Back row,
  and Back or Left there goes to the side bar. The Play page's rows are `build_launcher_home` (fixed order, the
  `kHome*` enum), drawn by `home_rml`, moved over by `home_move` (lines and columns). The tiles are the Display and
  Image pages' own rows (built by `build_options_rows`, picked by name), so a tile and its row cannot differ.
  `res/version.txt` holds this build's version: **raise it with every release** (the update check compares it with the
  highest tag in the releases list; GitHub's "latest" skips pre-releases, so the list is read).
- **Doors for tests.** `ED_LAUNCHER_OLD=1` the launcher as it was. `ED_LAUNCHER_TEST_KEYS`: `q` / `e` are LB / RB,
  `c=row3` or `c=stab2` clicks that element (tabs are `stab0`..`stab7`, Quit `stab8`). `ED_UPDATE_TEST=0.1.7` (or
  `fail`) answers the update check with no request. `ED_PREPARE=1 ED_PREPARE_HOLD_MS=6000` keeps the first-start
  page up for a picture. `ED_FRONTEND_LOG=1` lines now end with the tab and the bar's place.
- **Seen in pictures and logs.** The Play page against mock-ups 7a and 7b at 1920x1080; every tab's page; the bar
  (Up / Down, A / Right, B back to it, LB / RB), the Play page's arrows, Configure, Load > a slot > Back, the two
  cards, the roster, by scripted presses with the log; a hover by real window messages and clicks through RmlUi
  (`c=`); Play starts the game (the title came up); the first-start page; the no-disc state (a folder with no disc);
  4:3 (1280x960: the roster and the cards drawn smaller), 16:9, 21:9; the real update check ("this is 0.1.6, the
  newest release is 0.1.6") and a failed one; the old look by `ED_LAUNCHER_OLD=1`; the in-game menu and the title
  look's Display page after a start from the new launcher (both as before).
- **Settings through the tabs.** One setting changed on each of Display (FPS counter), Sound (Music), Graphics >
  Image (Sharpening) and Mods (Skip intro) by scripted presses, then Back: all four were in the settings file after.
- **Not tested.** A Tome-look page (no picture: it needs a game with the Tome open; nothing of this change is
  reachable there by the gate and the stylesheet's prefix). A real hand on the mouse and a real controller (presses
  were scripted). The Update button's own press (it would open the browser on the user's desktop: `SDL_OpenURL`).
  Unplugging a controller while the Play page is up. HDR calibration from the Display tab. The Mac and Steam Deck
  builds (updatecheck.cpp uses `curl` there: not compiled here). `ed test` stopped in `test-shadow-steady` on a missing
  `probe.txt` (a run from a save state, past the launcher; the last run of it that had the file was 2026-11-13): not
  looked into, and I ran it without `ED_RUN_AWAY=1` by mistake (two short game windows on the desktop).
- **One review at the end** (a fresh reader of the whole change): nothing that reaches the other menus; three small
  things fixed (LB / RB on the side bar while the save states' list is shown; the update check's thread outliving its
  own data at exit; a download cut off part way counted as an answer). Left as it is: the check takes the HIGHEST
  version among the fork's releases, so an upstream tag (v0.8-v0.9.6 exist as tags) must never be made a release there.
- **Differences from the mock-up, for the user to judge.** No "F11: fullscreen" in the directions (F11 is "load the
  newest save state" in this game). Play's cut corner is not drawn. A "Game disc" line was added under Saves (the old
  launcher's disc row has to stay reachable) and one help line under it. The small letters are a little larger than
  the mock-up's.
- **Still the user's to decide** (decisions.md): whether Continue stays the third button; what Credits shows (now the
  README's credits); whether the title screen's and the Tome's switches should be ticked boxes too (only the main
  launcher's were changed); a larger backdrop picture if they have one (this one is 1200x896).

## 2026-11-18: the Tome character change freeze fixed; the launcher menu on Esc / Back is back; Settings' old Quit row gone
- **The freeze (the user, 2026-11-17 evening; hang-20261117-204821, also 163217/163606/163817): fixed, committed.**
  A character chosen on the Tome's Characters page in co-op reloaded the game while the Tome was still open (the world
  goes on behind player 1's Tome, so the mode read "play"). The game ended under the Tome ("the world asks for mode 5
  with the Tome open"), the prompt chooser then made its words from a freed pack (0x8011708C -> 0x801DB374) and the
  byte-pair unpacker 0x801559D8 never ended. `characters_reload` (hle.c) now waits until no Tome is open (player 1's,
  a copy Tome, a Tome asked for), as solo's did already (its Tome is mode 3). The log line ends "(it waited for the Tome
  to close)". So: choose in the Tome, close the Tome, the game reloads where they stand about a second later.
  Seen: logs/run-20261117-210755-jtg-fix-tome-change (chapter 1, co-op; the same presses as the hanging baseline
  -163718, plus Start to close the Tome): reload at the close, both players walking after, no hang report. Picture:
  Screenshots for Claude/tome-character-change-fix/01. Player 2's change from their own copy Tome too (run
  -212655-jtg-p2-tome-change, picture 02: it waited, reloaded, both walked). Not seen: the user's play.
- **The Tome's Load/Save said the save "was not created by Eternal Darkness" (the user, 2026-11-17): fixed.**
  The card and the save are fine. The message is UMemcardText.bin group 0 entry 36 (card dialog state 66), raised only by
  0x800B9A68: it compares the save's game code (CARDGetStatus +40) with the disc's name the IPL leaves at 0x80000000,
  and that name had been wiped. Cause: the pad read (`hle_PADRead`) is the host's whole, but its nested game calls
  left `g_hle_passthrough` on at its end on some frames, so the dispatcher then ran the game's own PADRead with r3 = the
  read's return (0): four "no controller" records written at address 0, over 0x80000000-0x8000002F (the disc's name, the
  boot magic, the memory size). The title's states have the name whole (so the title's Load Game worked); the user's
  slot 1-3 states and most in-game states since 2026-11-02 have it wiped. Fix (hle.c `pad_read_ends`,
  `low_memory_heal`): the flags are cleared at the read's end (logged the first 5 times: "[pad] the read's nested
  calls left pass-through on"), and the 0x30 bytes are put back at the next pad read wherever a game or a loaded state
  lost them ("[boot] the disc's name at 0x80000000 was lost ... put back").
  Seen, old build against new, the same presses (chapter 1 co-op, Tome > Options > Load/Save > A, A): before, "Game File
  Access Failed! ... not created by Eternal Darkness"; after, the save list (logs/run-20261117-211534-jtg-tome-load-
  before, -211537-jtg-tome-load-after; Load itself, -212413-jtg-tome-load-list; Screenshots for Claude/tome-load-fix/). The user's slot3.state: put back at its
  first frame (-heal-probe-slot3). The same hazard may sit in other whole-replacement hooks that make nested game calls
  and end without clearing the flags: not looked for yet.
- **The title's Choose Character row has Credits' loop, and it plays on under the page (the user's answer, 2026-11-18):**
  docs/research/solo-coop-picker/next-requests.md, "Built: P8 and P9". Seen in a sound trace and a recording, not by ear.
  The Tome's pages still pause the game for both players: the user's choice for now (2026-11-18: "leave those settings
  ... to see how it plays").
- **Next, in order:** (1) the title sounds are built (above); hear them by ear. (2) The sweep for other hooks that can leave
  pass-through wrong after nested game calls is done (247 hooks read in six parts, each part's finds re-read by a
  skeptic; docs/research/passthrough-sweep/result.json has every find with its fix and risk). No second memory wipe.
  Fixed 2026-11-18 (the user's go-ahead), each as the skeptic's smallest fix: `coop_words_tick`, `hle_find_by_id`,
  `hle_moves_want`'s player 1 return, `coop_script_timers`, `tome_stops_return`, and the hardening of `room_section_draw`,
  `tw_mode_change`, `fog_object_draw`, `fog_door_setup`, `coop_held_weapon`. NOT changed: `hle_moves_want`'s failed
  `p2_moves_load` return (passing through there is what twisted Paul; left as it is). Refuted: `coop_sign_follow`,
  `title_main_start_load`, `sanity_door_rules`, `coop_reload_trace`, `coop_grab_events`, `coop_equip_take`.
  Seen: both players' words up together, player 1 closing theirs first, player 2 theirs after, the same on the build
  before these fixes and after (runs -222809-jtg-two-windows3-before, -222833-...-after; Screenshots for Claude/two-
  windows-at-once/): it already worked, as the user said, and `coop_words_tick`'s case never came up in the run (its
  log line "the words tick's nested calls left pass-through off" would say so). The general check -222456-jtg-sweep-
  shared-check (ED_ROOM_SLOTS=2 ED_COOP_STAY=1, the Tome, a character change, the reload, both walking): no hang.
  The players apart, by the kit's own check (`ed check L09/1951 --modes apart`, run -223703-check-L09-1951-apart: player
  1 through the door to room 279 with its arrival scene 1624, player 2 staying in 273, each half its own room for 2,000
  frames, the script to its line 11, no new hang report; picture 03 in the same folder). Not run: a failed weapon copy
  (it needs the copy to fail), a compare of that apart run with the build before (none was kept). (3) F11 still refuses a state made in the other mode. (4) By
  ear: every sound and the music (all runs were muted); real B/Esc/mouse on the pages; other languages; 4:3 and 21:9.
  (5) The user to try: play.bat, Tome > Load/Save > Load on their own saves; a character change from the Tome (it reloads
  when the Tome closes); the new character page; Esc / Back for the launcher's menu.
- **The launcher's menu opens over the game again on Esc / Back** (the user, 2026-11-18: a fallback while the Tome's
  pages settle). `debug_menu` is on by default (settings.c) and set to 1 in the user's settings.ini; the row is now
  "Launcher menu in game" (launcher Options > Display). The Tome's pages stay as they are. Not seen in a run (scripts
  cannot press Esc or Back); it is the same switch as before.
- **The Tome's Settings page has no "Quit game" row any more** (it quit with no question): Exit Game, the plaque beside
  Settings, asks first. The title's Options keeps its own.
- **The Tome's character page is a list and a large picture now (the user chose mock-up C, 2026-11-18).** Tome look only
  (the title's and the launcher's grids are as before): the names one to a line, twelve in view, a form in red after its
  name, the scroll bar beside them; right of it the lit one's portrait at 10.6em (about 2.5 times the old frames), the
  name, form, year, who they are and the three meters stacked. frontend.cpp `roster_columns` (1 on a Tome page),
  `roster_view_lines`, `character_look_rml`; menu.rcss "Choose a character on a Tome page" (.look*). Up/Down and
  Left/Right both step through the list; Stats and Back under it as before. Random's words are clear of Stats now.
  Seen: Screenshots for Claude/tome-character-layout/built-C-solo.jpg and built-C-coop.jpg (runs -212329-jtg-layout-c3-
  solo, -212124-jtg-layout-c2-coop), and player 2's in their half (built-C-player2-half.jpg). Not seen: the mouse, 4:3 and 21:9, HD portraits
  of the twelve at this size beside the disc's (both showed in the runs: Chandra's and Pius's forms are their own
  pictures). The mock-ups A, B, C are in the same folder.

**REMIND THE USER (asked 2026-11-17):** play Karim's skeletal pedestal (room 193) with logging so it can be traced:
docs/coop-play-test.md section 7. Its saves unblock the L04 elevator and Artifact-room tests. Drop this line once played.

## 2026-11-17: step P7, Exit Game in player 1's Tome (committed)
docs/research/solo-coop-picker/plan.md, "Built: step P7". Reviewers found nothing; the memory move was checked (five
stubs end exactly at +0xC000; the copies at +0xC000..+0xCD80 touch nothing else). Next: P8, then P9.
To try in play: docs/coop-play-test.md section 13, "Step P7".
Runs: logs/run-20261117-202123-jtg-p7-solo-exit, -202338-jtg-p7-coop-tomes, -202535-jtg-p7-coop-yes,
-202659-jtg-p7-title-quit-check.
- A fifth plaque, "Exit Game", on the Tome's Options page under Settings and Mods, player 1's Tome only (solo and
  co-op; player 2's copy Tome still has only Characters). A opens the title Quit's ask in the Tome look: "Exit the
  game?", No first (back to the Tome), Yes saves the settings and quits as the hidden launcher's Quit does.
- Code: hle.c `kTomeStopsP1` (5 stops, `TOME_STOP_MAX` 5, the plaque bank to +0xC000, `TITLE_QUAD_COPIES` moved to
  +0xC000), plaques.h/.cpp (5 plaques), frontend.h `FRONTEND_PAGE_QUIT`, frontend.cpp `build_quit_ask` words.
- Seen in pictures (Screenshots for Claude/solo-coop-picker/P7-exit-game/ 01-12): solo and co-op player 1's plaque, the
  ask, No back on the Tome, Yes closing the game cleanly (log "window closed", no hang report), player 2 without it,
  both walking after, the title's Quit ask unchanged. Not seen: real B/Esc/mouse, 4:3, 21:9, other languages.
- The Tome's Settings page still has its own "Quit game" row (no ask). Ask the user whether to keep it.
- saves/boot.state was rewritten by the jtg runs and put back byte-identical; settings.ini and the card untouched.

## 2026-11-17: the Solo/Co-op picker, step P6 review fix (checkpoint commit with P6)
- The user said again that Start Game and Load Game show no Solo / Co-op page. Their last play (logs/play.log, 16:16)
  is still older than P3 (18:17), so that build had no page. With today's build, the user's way (launcher Play, a copy
  of their settings, play.bat's ED_ROOM_SLOTS=2 ED_COOP_STAY=1) opens the page on both: logs/run-20261117-201037-p6f-
  playbat-start, -201117-p6f-playbat-load (P6-review/06, 07). They need to start play.bat again.
- Review finding, real and fixed: the user's slot1.state and slot2.state are 48 MB but made in co-op (player 2 in play,
  before the separate rooms). P6 called them solo, so in a co-op game the Tome's Slot 1 said "Made in solo" and
  restarted into solo, ending the co-op game (and slot 1 stops in solo after ~60 frames). Now the state's COOP section
  is read (`quickstate_file_coop_p2`, `hle_coop_state_p2_in_play`, frontend.cpp `state_old_coop`). In a co-op game,
  Load is greyed with "Made in co-op before the separate rooms: it loads only in a solo game." and nothing restarts.
  Real solo states (no player 2) still restart a co-op game into solo. The launcher's note for slot 1/2 now says "it
  plays in solo, without player 2."
- Seen: logs/run-20261117-200302-p6f-coop-tome-slot1-old-coop (greyed Load, A does nothing, still co-op),
  -200442-p6f-coop-tome-slot5-solo (a fresh solo state still restarts into solo, one player), -200644-p6f-launcher-
  slot1-note. Pictures: Screenshots for Claude/solo-coop-picker/P6-review/01-07.
- Not seen: slot 1/2 from a solo game's Tome; the launcher over a running co-op game; another language; the Mac build.
  The slot1 freeze in solo (older than P6) is still not looked into.

## 2026-11-17: the Solo/Co-op picker, step P6: save states made the other way (checkpoint commit)
- A save state plays only in its own mode (56 MB co-op, 48 MB solo). Loading one made in the other mode from the
  Tome's Save States (or the launcher over a game) now starts the game again into that mode, loading the state. Before,
  it was refused with a log line. The Slot page's Load row says so first. From the launcher before a game it already
  worked (P3), and the row now says "Made in co-op: it plays in co-op." settings.ini keeps the mode picked last.
- Code: frontend.cpp `state_other_mode`, `start_game`, `restart_process(resume, state)`, `build_slot`; main.c
  `ED_STATE_RESUME`; hle.c `ED_INPUT_SCRIPT_RESUMED` also for it.
- Seen both ways in pictures (Screenshots for Claude/solo-coop-picker/P6-state-mode/): solo game -> slot 3 -> co-op
  in room 69 with both players; co-op game -> a fresh solo slot -> solo with one player. Runs
  logs/run-20261117-193534-p6-solo-tome-load-slot3, -194822-p6-coop-tome-load-slot5-e.
- Found, older than P6: saves/slot1.state loaded in solo stops after about 60 frames (hang-20261117-193301.txt, and
  P3's hang-20261117-175959.txt at the same place). In no-launcher test runs the Slot page's Load is greyed (no disc
  list). F11 still refuses a state of the other mode.
- The user's settings.ini, slot1-3, boot.state, title.state and title-rooms.state have the same hashes as before; the
  test's slot 4 and slot 5 were removed.

## 2026-11-17: the Solo/Co-op picker, step P5 review fix (checkpoint commit with P5)
- Frame rate's note "Co-op plays at 60 frames a second." showed to solo players on any screen faster than 60 Hz.
  Now it shows only in a co-op game, or before a game when co-op was the last mode picked. Solo on a fast screen: no
  note, as before P5.
- Seen: launcher Display, cursor on Frame rate, 120 Hz screen, scratch settings: solo no note
  (Screenshots for Claude/solo-coop-picker/P5-menus/17), co-op with the note (18). Not seen: the solo Tome's Display
  on Frame rate (same code), a 60 Hz screen, the user's play. settings.ini and saves/boot.state unchanged.

## 2026-11-17: the Solo/Co-op picker, step P5: Mods > Co-op goes (checkpoint commit)
docs/research/solo-coop-picker/plan.md, "Built: step P5".
- **No co-op switch any more.** Mods > Co-op, the Mods list's Co-op row and the launcher's Co-op row are gone. Co-op is
  picked on Start Game / Load Game (P3, P4). Mid-game the mode cannot change (Return to title, then Start or Load);
  `ED_TEST_COOP_OFF` stays for tests. Test kits keep setting `mod_coop` themselves (`JTG_SET`, `ED_TITLE_PICK`).
- **Rows moved:** "Co-op screen split" and "Player 2 camera" are on Display under Camera (title, launcher, the Tome's
  Settings). "Player 2 plays with" lives only on Controls > Player II. "Player 1 camera" was Display's Camera.
- **Words:** Sanity Effects in co-op points at Solo on Start Game / Load Game; Frame rate greys only in a co-op game;
  player 2's Choose Character card is dimmed only in a solo game and points at Start Game's Co-op.
- **Tidy:** the title's Mods is Challenge and Title video only; the Tome's Mods has no Title video; Options' two
  never-shown sections (IV Controls, VI Mods and its Disc row) deleted. `ED_MENU_OPEN=coop` is gone.
- Seen in pictures: Screenshots for Claude/solo-coop-picker/P5-menus/ 01-16 (launcher, title, Tome solo and co-op).
  The split changed in the Tome took effect as the menu closed (11). Not seen: other languages, the mouse, the user's
  play. saves/boot.state was rewritten by the jtg runs and put back byte-identical (with its time); settings.ini, the
  card, title.state, title-rooms.state and slot3.state were not changed.

## 2026-11-17: the Solo/Co-op picker, step P4 review fixes (checkpoint commit with P4)
docs/research/solo-coop-picker/plan.md, "Built: step P4 review fixes". Four review findings, all real, all fixed:
- **Disc list after a restart:** main.c `frontend_resume_discs` fills it as the launcher does, so Return to launcher
  works. Checked in the log only ("3 disc(s), dump\GEDP01.iso chosen"). The launcher picture was not taken.
- **Paths with spaces:** `restart_process` quotes argv for `_execv`. Seen: full paths with spaces, solo -> co-op restart
  -> co-op dream (-185154, pictures 03-05).
- **The resume's A never stays set:** the player's own A ends it (on its row it goes on with no page). Leaving the main
  menu after it was ready ends it too. Seen: A mashed as the title came up -> "goes on, no page" -> co-op dream
  (-185154). Not run: the leave path, Return to title afterwards.
- **Presses on the black start-up:** `title_resume_cover` clears the pads, and the skip is not ended by a press. Seen:
  presses every 20 frames on black, the cover held, the title was saved, then the solo save list (-185008, pictures
  01-02).
- Regression on the play.bat path: co-op -> Start -> Solo -> the solo dream (-185308, pictures 06-07).
- Not verified: the user's own play, a double-click start on the user's desktop, Mac `execv`, other languages, sound.

## 2026-11-17: the Solo/Co-op picker, step P4: picking the other mode starts the game again in it (committed with the review fixes)
docs/research/solo-coop-picker/plan.md, "Built: step P4".
- **Picking the other mode now restarts into it.** The mode is saved, the card says "Starting the game again in
  co-op" (or solo) for about a third of a second, the window closes and opens again past the launcher, and the game goes
  straight on into the same Start Game or Load Game (no second page). About 3 seconds with a fitting title machine.
- **The first solo restart takes longer (about 10 s on black):** the old saves/title.state is 32 MB, not this build's
  48 MB, so the new process makes a fresh one (the intro passed over behind black, as Skip intro does the first time)
  and saves it over saves/title.state. Later solo restarts load it. Co-op uses saves/title-rooms.state (56 MB).
- Code: frontend.cpp `restart_process(resume)` (drops ED_QUICKLOAD, ED_SKIP_INTRO, ED_TITLE_STATE, ED_TITLE_RESUME, then
  sets ED_TITLE_RESUME=start|load and ED_ISO to this run's disc), `pick_restart_now` (from the page's step: presents the
  page by itself until the game's pad reads stand still 100 ms, so the game is held at its present before `_execv`),
  `frontend_title_resume_boot` (main.c, after `rooms_follow_coop`: the title machine of the new mode if its memory size
  fits, else made fresh); main.c skips the launcher for ED_TITLE_RESUME and takes ED_ISO over argv's --iso; hle.c
  `title_resume_step` (state 7 for 20 frames, nothing over it, the title saved: cursor to the row, the pick's A replayed).
- **Test kit:** a restarted run plays `ED_INPUT_SCRIPT_RESUMED` instead of `ED_INPUT_SCRIPT` (the guest frames start
  again near the same numbers). tools/ed/shots.ps1 and away.ps1 now follow the game into its new process (5 s look).
  both.tsv starts again with the new process (its first frames are the new one's).
- Seen (Screenshots for Claude/solo-coop-picker/P4-restart/): solo -> Start -> Co-op -> restart -> co-op split screen
  in the dream (-182818); co-op -> Load -> Solo -> fresh title -> the save list in solo (-183004); co-op -> Start -> Solo
  from the fresh 48 MB title.state -> one player in the dream (-183125); play.bat's path (launcher Play, ED_ROOM_SLOTS=2,
  a copy of the user's settings) co-op -> Start -> Solo, no launcher on the restart (-183252); the same mode still goes
  on with no restart (-183425). User files put back byte-identical (title.state, boot.state rewritten by the runs).

## 2026-11-17: the Solo/Co-op picker, step P3: Start Game and Load Game ask Solo or Co-op (checkpoint commit)
docs/research/solo-coop-picker/plan.md, "Built: step P3".
- **Start Game and Load Game (A or Start) now open the Solo / Co-op page** over the live title. The card first lit is
  the mode played last. Picking the mode this run is in saves it (settings.ini `mod_coop`) and the game goes on into its
  own Start (the carry-over question, the opening) or Load (the save list), as before. B goes back to the main menu with
  the same row lit. hle.c `title_main_start_load` (an intercept on the title's A 0x80019F30, rows 0 and 1) and
  `title_pick_replay` (the host presses A once on that row after the page closed).
- **Picking the other mode does not restart yet** (plan step 4, P4): the card says "Co-op needs the game started again:
  not ready yet" (or Solo), and the page stays; B or the other card still work. To change mode today: Mods > Co-op and
  start the game again.
- **Test kit:** `ED_TITLE_PICK=solo|coop` answers the page with no page; `tools/ed/jtg.sh` sets it from its run's
  `mod_coop` (give `ED_TITLE_PICK=` empty to get the page). A run that boots a state (`ED_QUICKLOAD`, so `run.sh`) plays
  that state's mode: 56 MB co-op, smaller solo (`coop_follow_boot_state`), and settings.ini keeps its own `mod_coop`
  (`settings_run_coop`). So `run.sh saves/slot1.state` (48 MB) now loads solo even with your co-op setting (it was refused
  under the separate rooms before), and slot3 (56 MB) co-op even with solo picked.
- Seen in pictures: Screenshots for Claude/solo-coop-picker/P3-start-load/. Not seen: another language, the idle 200 s
  under the page, a replay that fails to land (the one-second drop), the user's play; the music was not listened to.
- **The user's report ("solo and co-op screen does not appear"):** their last play (logs/play.log, 16:16) ran a build
  from before P3 (built 17:51). With P3, play.bat's own path was run: the launcher's Play, title-rooms.state, a copy of
  the user's co-op settings and card. Start Game and Load Game both opened the page (runs -181610-p3f-playbat-start and
  -181655-p3f-playbat-load, pictures 09 and 10). Ask the user to try it again with this build.

## 2026-11-17: the Solo/Co-op picker, step P2: the two-card page, and Controls through Player I / Player II (checkpoint commit)
docs/research/solo-coop-picker/plan.md, "Built: step P2".
- **Controls now asks whose first**, everywhere: the title's Options > Controls, the Tome's Settings > Controls and the
  launcher's Options > Controls show two cards, "Player I" and "Player II", each with that player's Choose Character
  picture and name (Default's Tome and skull, Random's silhouettes, player 2 on Default "The same as player 1"). A card
  opens that player's Controls; Back comes back to the cards, then to where it was.
- **The Solo / Co-op page is built but not hooked:** page 12 (`FRONTEND_PAGE_TITLE_PICK`), Solo (player 1's picture)
  and Co-op (both, player 1 gold in front, player 2 blue behind), under "Start Game" or "Load Game". A on it only logs.
  Start Game and Load Game still go straight on: plan step 3 (P3) hooks them. Test it with
  `ED_TITLE_PAGE_TEST=700:12:0` (title) or `ED_MENU_OPEN=pick-mode` (launcher).
- Seen in pictures: Screenshots for Claude/solo-coop-picker/P2-pick-page/ (title 16:9 and 4:3, Tome solo and co-op,
  launcher). Not seen: other languages, the user's play.
- **The user said (2026-11-17): "when selecting start game and load, solo and co-op screen does not appear."** That is
  right for this step: P2 only builds the page. Hooking Start Game and Load Game to it is step P3, next.
- **Review fix:** the HD portraits are no longer made over a live title page (it held the title about 300 ms, a gap in
  its music, the first time Controls or Choose Character opened on the title without `portraits/hd`). The disc's show
  there; the next Tome or launcher page, or the next start's preparing page, makes them. Run
  logs/run-20261117-173951-jtg-p2fix-mode-form-hdwait: page 12 set up in 18 ms (page 9 was 314.6 ms in -172624).
  Pictures 17 and 18 in P2-pick-page/ also show player 1's form (Karim's Spirit) on Solo and Co-op for the first time.
  Not run: the later making on a Tome page; not listened to: the music.

## 2026-11-17: the Solo/Co-op picker, step P1: Default's and Random's pictures, and Random (checkpoint commit)
docs/research/solo-coop-picker/plan.md (step P1 of the picker; the Solo/Co-op page itself is a later step: Start Game
and Load Game do not ask yet).
- **Default's picture is the game's own Tome and skull** (UMnMenu.tpl image 26, cropped to the card shape) on Choose
  Character's cards and roster, title and Tome looks. Made from the player's own disc with the portraits
  (`res/ui/portraits/Default.png`), and from the HD pack when HD textures are on (`DefaultHD.png`, stamped by the pack
  file in `DefaultHD.txt`). emblem.png only if neither can be made. `kPortraitsVersion` is 2, so the first start after
  this build shows the preparing step once (fast) and makes them. Nothing of the disc ships (make-win-package.sh checks).
- **HD portraits (plan step 7) are in:** with HD textures on, the twelve come from the pack's 504x356 copies
  (`res/ui/portraits/hd/`, found by each disc picture's XXH64, rows turned upright, stamped). HD off: the disc's.
- **Random, a new choice beside Default** (both players, every look). Choosing it rolls one of the twelve (the host's
  random; another than the last roll), kept in settings.ini (`mod_pN_character = 13`, `mod_pN_random = 1-12`) across
  chapters and restarts until the player chooses again. A again on Random rolls someone new. Every card and name shows
  "Random" and the user's silhouettes (`res/ui/random.png`, from random-source.jpg, crop x 20 y 30 1096x770, 630x445),
  never who was rolled. Only the log names it: `[settings] Random for player N: rolled NAME`, and
  `[player] chapter C's character ... played as M (Random: NAME)`. The game reads the roll through
  `settings_character()` / `settings_costume()` (settings.c): the rolled character in their own body, their own stats.
  Player 2 on Default still plays player 1's character (the roll too). Player 2's Random does nothing in solo.
- Seen (Screenshots for Claude/solo-coop-picker/P1-default-random/): title cards and roster with Default (HD), Random
  lit and chosen; the Tome roster (co-op half, and solo with HD off); a run with the saved roll (13/3) playing Anthony
  and player 2's load-time roll playing Ellia; a re-roll in the Tome reloading player 1 as Lindsey; solo Random Max.
- **Found, older than P1:** changing player 1's character from the Tome's Characters page in a co-op game (chapter 1,
  room 341) hangs after the reload, in the game's words unpacking (pc 0x80155AA4, lr 0x801DB4C4, from the prompt
  chooser 0x80117854). The HEAD build does the same (run -163718-jtg-p1-baseline-tome-change, an explicit character);
  `ED_TEST_P1_CHARACTER` (a change outside the Tome) does not. saves/reports/hang-20261117-163217/163606/163817.
- Not seen: player 2's roster in the Tome look, other languages (no row counts changed), the user's own play.
- **The Solo/Co-op page itself is NOT built yet.** Start Game and Load Game still go straight on (the user noticed).
  It comes in the plan's step 1 (the page) and step 3 (Start Game and Load ask).
- **Review fixes (same commit):** a pack picture that can't be used is tried once, not at every start (stamp
  `failed <id>`); an old portraits stamp makes Default's HD picture again too; the HD portraits keep no answer while no
  disc is known. Seen with scratch res/settings and the launcher's preparing step (runs A-F, pictures and logs in
  Screenshots for Claude/solo-coop-picker/P1-review-fixes/): the preparing step made everything (A, F), remade DefaultHD
  on an old stamp (B), stamped a 640x480 pack copy `failed` (C), then said "everything prepared already" (D), and the
  Default card fell back to the disc's picture (E). Not run: the no-disc case.

## 2026-11-17: the title's music under the title-look pages, step F5 (checkpoint commit)
docs/research/firstrun-hd-flash/title-music.md ("Built: step F5" and "Built: step F5 review fixes").
- **ASK THE USER FIRST:** the game's main menu has no song, so Choose Character is still quiet after the A's sound.
  F5 does not give music there. Should it play a song we choose (for example the Options song)? Build that only on a yes.
- **Review fix:** quick-state keys are also blocked in the frame or two between the game's ask for a title page and
  the page opening (`hle_title_page_hold`, quickstate.c). From the code only: scripts cannot press keyboard keys.
  The finding about the problem page and the live page on two threads was rejected: both run on the render thread.
- Final build seen in run -154055-jtg-f5-final-main: Choose Character and the Sound page live, presses held, no
  "ran dry", no hang report, the Options song unbroken under Sound (pictures 05 and 06 in f5-title-music).
- **The title now runs under Choose Character and the title Options pages** (Display, Sound, Graphics, Controls,
  Mods, Quit), so the game's own sound goes on as on its own menus. The page steps once a game frame; the guest's pad
  is held from the A that opens it until every button is let go after it closes. Tome pages still pause (Way B).
- **What the game plays there:** the Options song goes on under the Options pages, with no hole and no restart (PCM
  of run -151023-jtg-f5-main). The main menu itself has no song: left alone 200 s it is silent after the title's
  opening sound (run -152346). So Choose Character is quiet after the A's sound, as the game's main menu is. Tell the
  user; if they want a song there, it would have to be chosen by us (ask first).
- **The attract movie does not start under Choose Character** (200 s, run -151443); with no page it starts at 180 s.
- Two stalls at a page's open were found and fixed (the screen-rate measure, about 190 ms; the first page's pictures
  and layout). No "ran dry" at any open in the final runs. Failed first try in coop-failed-attempts.md.
- Pictures: `Screenshots for Claude/f5-title-music/`. For the user: docs/coop-play-test.md section 12.
- Not seen: the launcher path, a fresh install, a real hang under a page, HDR calibration from the title's Graphics.

## 2026-11-17: step F4 review fixes (checkpoint commit with F4)
docs/research/firstrun-hd-flash/firstrun.md ("Built: step F4 review fixes").
- **The first start now builds only the shaders for the player's anti-aliasing setting.** The shipped list has 4,568
  rows for 4x MSAA and 2,074 others; a new player is at 1x, so the 4x ones are left unbuilt (kept for later). A cold
  fresh install's page went from 132 s to 45 s (run -143748-f4fix-cold1x; pictures 01-04 in
  `Screenshots for Claude/f4-review-fixes/`: the page "313 of 2,074", the launcher, Choose Character on the skull, the
  roster's portraits). GXRuntime patch: pipeline_cache.cpp `load_pipeline_cache_entries`.
- **Turning 4x on shows the page once more** (`prepared.txt` now ends "msaa N"): run -144018, all 6,642 in 113 s,
  picture 05. Back to 1x: 0.7 s, no page (run -144237).
- **The package's list check reads the versions of the packaged commit** (`pipeline_seed.py check --commit`), not the
  working tree's `.deps`. Tried on old commits (passes and fails as it should). The full package script was not run.
- Not seen: 4x, then 1x, then 4x again (Dawn's prune should make the 4x rows cold again: from the code).

## 2026-11-17: the first-run preparing step, step F4 (checkpoint commit with its review fixes)
docs/research/firstrun-hd-flash/firstrun.md ("Built: step F4"). The launcher music (step 5) was left out: the user
said it is fine to keep.
- **The launcher shows "Preparing the game from your disc"** on the first start, after an update (a new shipped
  pipeline list), after the cache is cleared, or with a new HD pack: a bar on the launcher's parchment while the
  portraits are made, the shaders are built from the shipped list and the HD title pictures are made. Stamps:
  `res/firstrun.txt` and `prepared.txt` in the cache folder. The second start skips it. B skips it for now.
- **The shipped pipeline list is current again** (6,642 rows at GXCore 13; it was 295 rows at version 10, all thrown
  away at each start). `tools/pipeline_seed.py make` / `check`; the package fails on a stale one. The seed merge can no
  longer age a newer row (GXRuntime patch).
- **The title's Options picture is taken from the game's memory on the title's first frame** (`title_options_from_bank`
  in hle.c, decoded with the renderer's own decoder). So Choose Character from the title has the skull behind it on a
  fresh install, before Options is ever opened. The file is byte-identical to the one Options used to make.
- **The portraits are made wherever one is shown** (a card, the roster), also with no launcher.
- Seen in a true fresh install in a scratch folder (run -141833-fresh4-first, pictures in
  `Screenshots for Claude/f4-first-run/`): the page and bar (01-03), the cards on the skull (04), the roster's portraits
  (05); a second start with no page; HD on (07); no launcher with Karim chosen (06).
- Timings here: a cold first start 132 s for the shaders; a warm rebuild 3 s; portraits 20 ms; HD 0.3 s.
- Found: a UI loop with no wait starves the pipeline worker (warm 30 s instead of 3 s). The page waits 33 ms a frame.
  The launcher menu's loop has no wait either [inferred to slow pipelines behind it; not changed].
- The user's own next start shows the page once (their cache has no stamp yet), about 3-5 s.
- For the user: docs/coop-play-test.md section 11.

## 2026-11-17: the HD title picture with the title video off, step F3 (checkpoint commit, with the review's fixes)
docs/research/firstrun-hd-flash/hdtitle.md ("Built: step F3" and "Built: step F3 review fixes").
- **Review fixes:** the HD backdrop now has a stamp beside it (`options-clean-hd.png.stamp`: code version, scale, the
  pack file's path, size and time), so a swapped pack or a newer build makes it again (log "not stamped: made again"
  in run -132942, "up to date" in run -134014). The ghost of "Opt" under Choose Character is gone at 2x in the paint
  dump (run -133138, `Screenshots for Claude/f3-review-fixes/compare-ghost-before-after-lit-2x.png`). HD textures
  turned on after boot now bring the HD backdrop at once, without a key (run -133835, fresh scratch res, pictures
  01-03). The HD state is never destroyed and the exit waits for the HD workers (`titlevideo_shutdown`; from the code
  only). New pictures: video-on Options list on HD (04, 05), the final build's title with Choose Character lit (02).
- Not seen after the fixes: a 4K screen (a faint lighter smudge, no letters, stays at 2x where Options was); closing
  the launcher during the boot read.
- **With the video off and the HD pack there, the title is the pack's 3400x1920 picture** with Choose Character
  painted on it (measured on the disc's 850x480, painted at 4x). The HD Options list (2x) and the title look's HD
  backdrop (`res/video/options-clean-hd.png`, made at boot from the pack) also show, with the video on or off. A fresh
  install with the pack no longer has black behind the Choose Character cards.
- The pack is read at boot on workers (`titlevideo_preload`, main.c). The 1x paint shows until the HD one is ready.
- Without the pack or with HD textures off, nothing changed (runs -131223-jtg-f3-hdoff-list, -131541-jtg-f3-nopack).
- Seen: runs 20261117-1305 to 1315 (jtg f3-*). Pictures: `Screenshots for Claude/f3-hd-title/`.
- Not seen: a 4K screen, the HD toggle while on the title, the first 1x frames. Older, not changed: the dark box past
  "Stereo Surround" at scroll 3 (`options_shade` ends at y 240). For the user: docs/coop-play-test.md section 10.

## 2026-11-17: subtitle leftovers, step F2 (checkpoint commit; review found nothing)
docs/research/coop-gaps/subtitles.md ("Built: step F2").
- **Player 2's copy of a line sat 48 px left of their half's middle** when the line was in both halves. The cause was
  the renderer, not `coop_subtitle_wide`: `widescreen_hud_shift` (gxcore_draw.cpp) skipped its shift for any draw on
  a height band where something crossed the screen's middle. Player 1's centred line is on the same band, so the copy
  at +4016 was not shifted, while the host still took 80 off. Now a draw wholly past the right edge (another half's
  HUD) is always shifted. The flash no longer puts half the shift back (`flash_away`), so its disc is centred too.
- Seen: both halves centred for player 2's phone (left 239, right 719 px; was 671) and player 1's phone; at aspect 4
  too. Apart, the EXAMINE prompt, the clock's words and the ANSWER PHONE prompt: the same places as before.
- **Older, not from the subtitle work (not fixed):** a state saved in player 2's own scene loads as player 1's.
  Both halves show the scene's close-up camera and player 2's camera is "-" (U2b's picture 02; its line naming player
  1's character is U1's rule for a loaded line). Seen again with a state saved before any subtitle started and no subtitle in the run
  (logs/run-20261117-124145-f2-p2scene-load). From the code: `hle_save_held` does not wait in player 2's own scene,
  and `hle_coop_state_load` (step E, 1bc19f3) clears `g_coop_screen`, `g_play_script_p2`, the lent cameras and their
  camera zones, so the loaded scene is the game's. In that run the scene also stayed held: busy 0x22 and no line for
  the 370 frames after the load (it was saved before the scene's script ran, frame 4575 of 4592). A fix would hold the
  save through player 2's own scene, as for their close-up. Not done here.
- Pictures: `Screenshots for Claude/F2-subtitle-leftovers/`.

## 2026-11-17: the sanity/phone white flash, step F1 (checkpoint commit)
docs/research/firstrun-hd-flash/flash.md ("Built" and "Built: review fixes"), in hle.c (`flash_submit_notify`):
- **The blocky flash:** the game fills 21 of the disc's 64 pairs; the rest sat at 0,0 with z +1 and the host kept
  them, a hard triangle from the middle to the top-left. They are now folded onto the last used pair. No renderer change.
- **Co-op:** the flash goes to the half of the player who set it off (the subtitle owner's test, moved as the
  subtitle line). The owner now holds for the disc's whole life (240 frames), so a host stall can't send it back to
  player 1's half. A moved disc is grown by half so its half's corners stay covered at 21:9.
- Seen: player 2's phone (one room, apart), player 1's phone, solo a = 0 and a = 20, a forced player 2 flash
  (runs 20261117-1216 to 1220); after the review: a 400 ms stall (run-20261117-122919-flash-p2-stall), aspect 4
  (run-20261117-123004-flash-p2-219), solo again (run-20261117-123048-flash-solo-a0-recheck). Pictures:
  `Screenshots for Claude/f1-sanity-flash/`.
- Not seen: a real sanity effect's own end in solo (slot 2 is chapter 1 now), 0x8006B270's flash, the 0x801D68F4
  user, the split off, a real 21:9 screen, player 2's copy Tome. Title, Options, Choose Character, Tome and subtitle
  runs were not re-run (nothing on those paths changed). For the user: docs/coop-play-test.md section 9.

## 2026-11-17: co-op subtitles, step U3 review fix (committed with step U3)
- The Tome's replay page behind player 1's real Tome: the world frame's updater call still ran and would make the
  replay's lines from play's pack (wrong words) or count twice a frame. Now `coop_subtitle_tick` passes over the
  world's call while the record is the Tome's replay, so the Tome's call alone runs it. Read from the code only: no
  run opens the replay page.
- Seen unchanged on the new build: plan D (run-20261117-115953-u3r-p1tome-first), plan E
  (run-20261117-120120-u3r-p2copy) and U2's follow run (run-20261117-120046-u3r-check-p2-follow). Pictures 06 to 09 in
  `Screenshots for Claude/U3-subtitles-tome/`.

## 2026-11-17: co-op subtitles, step U3: the Tomes (committed)
Plans D and E of docs/research/coop-gaps/subtitles.md ("Built: step U3"), in hle.c:
- **D** (`coop_subtitle_tick`): behind player 1's real Tome the updater ran in both the world's and the Tome's frame,
  so a line went at double speed, and its second line came from the Tome's pack ("The HORROR!!" on player 1's Tome
  page). Now the Tome frame's call is passed over while player 1's Tome shares with the world, unless the Tome's
  replay page started the record. Seen: lines made 49 frames apart (before: about 25), both in player 2's half, nothing on
  the Tome page (run-20261117-114634-u3-after-p1tome-first; before: run-20261117-114238-u3-before-p1tome-first).
- **E** (`coop_subtitle_out_of_copy`): a copy Tome carried the world's line and U2's copy on its page, then the
  Tome pack's wrong line. Now, at the copy's opening, the game's end 0x80008DD4 runs in the copy and U2's copy is
  freed there. Done for any owner's line. Seen: player 2's Tome page clean, player 1's lines in their half
  (run-20261117-114702-u3-after-p2copy; before: run-20261117-114334-u3-before-p2copy).
- Player 1's Tome must be opened before player 2 answers: the game refuses Start in a scene.
- Pictures: `Screenshots for Claude/U3-subtitles-tome/` (01 to 05). U2's follow run is unchanged (05).
- Not seen: a Tome closed mid-line (scripted B and Start did not close either Tome here, even with no phone);
  both Tomes up with a line (not reachable with the phone); the Tome's replay page. All runs used saves/slot3.state.

## 2026-11-17: co-op subtitles, step U2 review fixes (committed with step U2)
- A save now waits while a subtitle's copy is up (`hle_save_held`). Before, a state saved then kept the copy, and
  after loading it nothing freed it: the line stayed drawn in the other half. Seen: the save waited and landed just
  after the copy went (run-20261117-113619-u2b-save-hold); loading that state left no stray line and the pool back
  to its count (run-20261117-113646-u2b-load-held-save).
- Co-op turned off, or player 2 gone, mid-line: the copy is freed and player 2's line put back at the game's x
  (`coop_subtitle_stop`). New test switch `ED_TEST_COOP_OFF=FRAME`. Seen: one centred line on the full screen after
  co-op went off, nothing left after (run-20261117-113718-u2b-coop-off).
- Pictures: `Screenshots for Claude/U2b-subtitles-review-fixes/` (01 to 03).
- The returning ANSWER PHONE prompt in the leave run: made at frame 4734, the same pad read as the copy's free at the
  room change, not 4668 as first said. Not caused by U2 is only inferred from the call order; not checked.
- Not checked: player 2 gone mid-line by an area change; the leave and mix runs again.

## 2026-11-17: co-op subtitles, step U2 (checkpoint commit)
The user's second wish: with both players in one room the line shows in both halves; apart, only in the owner's (U1).
Built in hle.c (`coop_subtitle_copy`; detail in docs/research/coop-gaps/subtitles.md, "Built: step U2"):
- A second element made with the line's own make (0x801DB374, its arguments read at the line's layout), at the other
  half's x. Freed with the game's free 0x801DA4D8 whenever the line goes (0x80008D7C, 0x80008DD4, 0x80008E40), and
  when the players are in two rooms, the split goes off or a Tome is up. Only made with two of the ten elements free.
- Seen in pictures (`Screenshots for Claude/U2-subtitles-both-halves/`): player 2's call and player 1's call in both
  halves in follow (01, 03); apart, owner only (02); player 1 leaving by door mid-line, the copy goes (04); the
  EXAMINE prompts and the clock's words still in their owner's half (05, 06). The pool went back to its count each time.
- Not seen: the copy moved for a screen change; a full pool; the Tome-up free (Start does nothing in the other's
  scene); players meeting mid-line; the clock, maid, creature and event-91 lines. All runs used saves/slot3.state.
- Next: the user's play (docs/coop-play-test.md section 8).

## 2026-11-17: co-op subtitles, step U1 (checkpoint commit)
The user's call: a subtitle may show only for the player who started it. Built that way (plan A and B of
docs/research/coop-gaps/subtitles.md, its "Built" section has the detail).
- What changed (hle.c only, all gated on co-op with player 2 in; solo never reaches it):
  - The three subtitle starts note who started the line (player 2 when it runs as theirs). The Tome replay is player 1's.
  - The world updater's line is moved into its owner's half as it is laid out: player 2's at x + 4096 - 80, player 1's
    at x + 8192 - 80 while player 2 has the screen, else the game's x. The 80 undoes the renderer's widescreen HUD shift.
  - Each frame the line is placed again if the screen or split changes.
  - Player 2's line names player 2's character.
  - Review fixes: a state load clears the owner; a new start no longer moves the old line (`line_owner`).
  - Test switch `ED_SUBTITLES=1` (bit 0 of 0x80307180 held on), in the ed skill.
- Seen in pictures (`Screenshots for Claude/U1-subtitles/`):
  - Before (1): player 2's phone lines in player 1's half.
  - After: player 2's lines centred in their half, follow and apart (2-4, 10); player 1's phone lines in theirs (5, 11);
    words windows and EXAMINE prompts still in their owner's half with a subtitle up (6, 7, 12).
  - Runs: logs/run-20261117-105400-sub-after2-p2-follow, -110024-sub-after2-p2-apart, -105448/-105525 (mixed), and
    after the review fixes -111103-sub-review-p2-follow and -111154-sub-review-mix-p1phone-p2clock.
- Not seen: player 1's line moved to +8192 (no subtitle ever overlapped the other's close-up, as presses wait); the
  per-frame move; a state load mid-line; a start over the other's line; the creature, event-91 and Tome replay starts;
  the ghost maid; the clock's third try by player 2 (player 2 fell through room 77's floor at the put spot). All runs
  used saves/slot3.state. Not played by the user.
- Already broken before (not from this): player 2's ANSWER PHONE prompt shows again after their call; the phone's
  flash lights player 1's half.
- Next: the user's play (docs/coop-play-test.md section 8). Both halves when both are in one room is a later step.

## 2026-11-17 (morning): native menus, step 6 review fix (committed with step 6)
A review found one real problem (two reviewers, the same one). It is fixed and seen in pictures
(`Screenshots for Claude/s6-tome-coop/` 16-22).
- The problem. The renderer took the split layout once, as the page opened (hle.c gives `g_settings.mod_split` to
  `aurora_split_page`). The page read `mod_split` again every frame (`window_half`, and `tome_units_box` with a half).
  Player 1's Tome has Mods, and Mods > Co-op > "Screen split" changes `mod_split` at once. So after a change the leaf and
  the veil moved to the new halves while the renderer still drew the old ones.
- The fix (frontend.cpp). New `g_skin_split`, set from `mod_split` just before `hle_coop_ui_over(true, ...)`, the same
  value hle.c hands the renderer. `window_half` and `tome_units_box` use it. A split change shows once the page closes.
- Seen:
  - Side by side, player 1's Mods > Co-op, Screen split changed to "Above and below": the leaf stays on player 1's Tome
    in the left half, player 2's half dimmed (16). After Back, the plaques are in the top half (17), and both halves
    play one above the other (18; run logs/run-20261117-102519-jtg-s6-split-change, "[split] on ... (above and below)").
  - Over and under from the start: the Co-op page sits on player 1's Tome in the top half, the bottom half dimmed (19).
    Changed to "Side by side": the leaf stays in the top half (20; run -102706-jtg-s6-split-over-under; that run's game
    was closed by the run's time limit before the last Back, as my key list was too long).
  - Over and under, Settings: on the Tome in the top half (21), then after Back both played one above the other, both
    walked (22; run -102825-jtg-s6-over-under-close; P1 -11,832 to -70,967, P2 -10,682 to -145,900).
- No failed attempts.
- The "over-and-under not seen" line below is now seen (19-22). The rest of that list still stands.

## 2026-11-17 (morning): native menus, step 6: the Tome look in co-op (committed)
A Tome page in co-op now opens in its owner's half, on their own Tome. The partner's half keeps their last picture,
dimmed. Pictures: `Screenshots for Claude/s6-tome-coop/` 01-15.
- How. Before step 6, `hle_coop_ui_over(true)` put the split off for any page over a co-op Tome (split menu -1, only
  view -1, skip views 0), so the window showed one whole frame. That stays: it is what lets the page's frames reach the
  window (the "nothing shown" bug in its comment, and the `s_keepOnly` entry in coop-failed-attempts.md). What is new
  is only how the paused frames are drawn.
  - aurora.cpp `aurora_split_page(player, layout)`: told by hle.c just before the split goes off. It notes where the
    halves' pictures come from, while the split state still says it: both menus up (only view 2) uses each menu's kept
    picture (`s_menuKept`); a menu sharing frames with the world uses the menu's kept picture and the world's last
    (`s_held`); otherwise the frame shown and the partner's last view (as `menuHalf`).
  - In the present, `pageHalves`: the partner's picture in their half, the page owner's Tome fitted in theirs, then the
    RmlUi page over the whole window. The "menu kept" copy is not taken while a page is up.
  - hle.c `hle_coop_ui_over(on, half)` now answers whether the page is in a half. frontend.cpp sets `g_skin_half` to
    -1 when it is not, so the page then has the whole window as before.
  - frontend.cpp: `kTomeInHalves = true`. `tome_under_page()` is true for every Tome page (no veil on the Tome). A new
    `#veil` (menu.rml, menu.rcss `body.skin-tome.tome-halves #veil`) dims the partner's half, placed by `window_half`.
  - The GXRuntime patch is regenerated (patches/gxruntime/eternal-darkness.patch).
- Seen (all fresh Jump to Game runs, chapter 1, 16:9, side by side, 1280x720 shots):
  - Player 1's Settings and Display on their Tome in the left half, player 2's half dimmed (01, 02; run
    logs/run-20261117-101118-jtg-s6-p1-settings, log "[coop] a Tome page in player 1's half (sharing 1, copy 0, owner 0)").
    Back twice: plaques back (03), then both walked (04; both.tsv: P1 -11,832 to -70,967, P2 -10,682 to -145,900).
  - Player 1's Characters in their half, a portrait chosen with the test keys (05, 06), plaques after (07), both walked
    (08; run -101323-jtg-s6-p1-chars).
  - Player 2's Characters on their copy Tome in the right half, player 1's world dimmed (09), their plaque after (10),
    both walked (11; run -101441-jtg-s6-p2-chars, "sharing 1, copy 1, owner -1").
  - Both Tomes open, player 1's Settings: player 2's Tome (Inventory) dimmed in the right half (12), both Tomes back after
    (13), both walked after closing them (14; run -101601-jtg-s6-two-tomes).
  - Solo is unchanged: the page on the whole Tome, no "[coop] a Tome page" line (15; run -101716-jtg-s6-solo-check).
- No failed attempts: the first build worked in every case above.
- Not seen: over-and-under split (mod_split 1); 4:3 and 21:9 in co-op; player 1 with a copy Tome over player 2's close-up
  (`g_tome_copy_p1`); player 2 holding the real Tome (`g_coop_tome_owner == 1`); a state loaded from Save States while
  a co-op page is up (the kept pictures would stay until the page closes); real B, Esc, mouse and wheel presses (only
  the test keys); a window resize with a co-op page open. At 1280x720 the page in a half is small (about 13 px letters);
  the user should say if it is too small on their screen.
- Committed with its review fix (above).

## 2026-11-17 (morning): native menus, step 5 review fixes (committed with step 5)
A review found two real problems. Both are fixed and seen in pictures (`Screenshots for Claude/s5-tome-skin/` 24-29).
- Co-op player 1's page lay with no veil on the bright world, with the HUD's "B Examine" showing, placed for a Tome
  that is not drawn. Before step 5 that page was dimmed. Now the no-veil rule needs the body class `tome-under`
  (frontend.cpp `tome_under_page`): solo, or player 2's copy Tome (half 1), where the pictures show the Tome under the
  page. Player 1 in co-op keeps `body.overlay`'s dim, and the leaf moves to the middle (`kTomeLeafCentre`, -40 units).
  Seen: co-op player 1's Settings and Display dimmed, leaf in the middle, plaques back after (pictures 24, 25, 27; run
  logs/run-20261117-095733-jtg-s5-fix-coop-p1). Player 2's Characters still on their Tome with no veil (picture 29,
  run -100102-jtg-s5-fix-p2). Solo still has no veil (picture 28, run -095924-jtg-s5-fix-solo-43).
- Aspect ratio was left out of Tome pages, so 16:9 to 21:9 could not be changed in play. The game's Widescreen only
  goes to 4:3 and back to the shape it remembers. Now the row is there: in a wide shape it moves only between 16:9 and
  21:9 (both are "wide" to the game, so `sync_widescreen_option` leaves its word alone). In 4:3 it is greyed, with a
  line saying to turn the game's Widescreen on first. Seen: co-op 16:9 > 21:9 > 16:9, the leaf follows, no
  "[widescreen] the game's option set" line after the menu opened (pictures 25, 26; run -095733). Solo 4:3: greyed
  with its line; Right did nothing, settings stayed 4:3 (picture 28; run -095924).
- Not seen: a solo 16:9 page after these fixes (solo was seen at 4:3 only; the no-veil rule is the same); 21:9 on a
  real 21:9 window (the shots are 16:9, so 21:9 shows letterboxed); player 1 with a copy Tome (`g_tome_copy_p1`), which
  keeps the veil by this rule.

## 2026-11-17 (morning): native menus, step 5: the Tome look, solo (committed)
Every page opened from player 1's Tome plaques (Characters, Save States, Settings and all its pages, Mods and all its
pages, the two cards) is drawn in the Tome look, not the launcher's parchment.
- frontend.cpp `tome_units_box(half, u0, v0, u1, v1)`: a box in the Tome's 640x480 units in the window (the 2D is
  -80..720 wide, 0..640 in 4:3, the middle 16:9 box at 21:9, a co-op half fitted as aurora.cpp does). `tome_page_rect`
  is the page (x 144..576). The page is a leaf laid on it, `kTomeLeaf` = x 166..554, y 10..384: clear of the medallion
  and of the "RB Inventory" hint at the foot. Its letters are 0.036 of the page's height (`kTomeFont`); every size in
  the rcss block is em of that. 9 rows show at a time (`kTomeLines`); the rest scroll with the Tome's diamond marks.
- A Tome page is one column (`build_rows` sets every row's page to 0): Button Mapping's keys and the Mods rows scroll as
  one list. Aspect ratio in the Tome look: 16:9 and 21:9 only (see the review fixes above). The "Experimental" tag goes into the help line, as on the title. The directions line is hidden.
- The roster: `roster_columns()` (3 in the Tome, 4 elsewhere) replaces `kRosterColumns`. `character_strip_rml`: under
  the frames, the Tome's rule, name, form, year on one line, the three bars side by side.
- `slots` body class on the slot lists (the Tome look sets ten slots a little closer). `run_menu` re-places a Tome page
  when the window or aspect changes, as for the title.
- `kTomeInHalves = false`: in co-op the page is placed on the whole window for now. The picture behind is one whole
  picture (`hle_coop_ui_over` turns the split off), so a page in one half looked small and lost (seen, run
  -094229-jtg-s5-tome-coop). Step 6 keeps the split and turns this on.
- menu.rml: `#rule` (the ornament under the heading, Tome look only). menu.rcss: the `body.skin-tome` block at the end.
- Seen in pictures (`Screenshots for Claude/s5-tome-skin/`), solo, 16:9, co-op off (`JTG_SET=mod_coop=0`): Settings,
  Display (no Aspect ratio, HDR's note), Sound, Graphics > Image and Effects (a two-line note), Controls scrolled to
  Back with the mark above, Button Mapping as one list at the top and scrolled, Characters (top, a frame lit, Back lit,
  scrolled to the end), Save States, a slot's page, Mods, Mods > Co-op, Mods > Characters (the two cards); 4:3:
  Settings and Display. Runs logs/run-20261117-0925..0941 (`-jtg-s5-tome-*`). Co-op: player 2's Characters from their
  copy Tome (over their Tome, -094421-jtg-s5-tome-p2) and player 1's Display (over the world, -094534-jtg-s5-tome-coop2),
  plaques back after. The title look (-094658-jtg-s5-title-check, Aspect ratio still there) and the launcher
  (-094801-s5-launcher-after) look as before.
- Careful in tests: in Save States, A on an empty slot saves at once. My first States run did that (slot 5, a new
  file); I deleted saves/slot5.state after. And jtg.sh closes the game when the recording stops for 20 s, which a menu
  does: keep a page's ED_PAGE_TEST_KEYS under about 18 s in all.
- Not seen: a real B, Esc, mouse or wheel press (only the test keys); 21:9; a window resize or Fullscreen change with a
  Tome page open; Cheats, Challenge and Sanity Effects pages in the Tome (same rows as Co-op's page); key capture on
  Mapping; a roster frame chosen with A; the Default frame's strip.
- Next: commit when told; then step 6 (the Tome look in co-op: keep the split under the page, then `kTomeInHalves`).

## 2026-11-17 (morning): native menus, step 4 review fixes (committed with step 4)
A review found four real problems. All four are fixed and seen in pictures (`Screenshots for Claude/s4-review-fixes/`).
- 21:9 was placed wrong. At 21:9 the renderer keeps the game's 2D at 16:9, narrowed into the middle
  (aurora_backend.cpp `set_wide_scale`, gxcore_draw.cpp `wide_scale`). `title_picture_rect` now uses a 16:9 box in
  the middle when `g_settings.aspect == 4`. Seen: the page's skull and logo sit where the game's own 21:9 Options has
  them (pictures 01, 02; run logs/run-20261117-090350-jtg-s4fix-219).
- The page did not follow a new aspect or window size. The renderer takes a new aspect only at its next present.
  `run_menu` now checks the window size, the shown aspect and the aspect setting each frame on a title page, and
  calls `render()` again when one changes. Seen: Aspect ratio 21:9 > 16:9 > 21:9 on the Display page, the page
  follows each time (pictures 03, 04; same run). Not tried: Fullscreen on/off, or dragging the window.
- A page opened before the picture was written stayed black all run. RmlUi never loads a picture again once a load
  failed. The picture's decorator now needs the body class `title-art`, set only once `options-clean.png` exists
  (`title_art_ready`). `render()` also tries `ensure_title_art` again on each title page. Seen with res/video's
  options.rgba and options-clean.png moved aside: Choose Character first, black behind (as expected); then Controls,
  picture shown (pictures 05, 06; run -090616-jtg-s4fix-fresh-map). The two files came back the same (md5).
- Button Mapping for a controller-only player had no right-hand rows, so it became a list in the skull column.
  `title_wide_page()` now counts Mapping as wide always, and the scroll marks go by it too: a wide page on the title
  panel uses the launcher's small red mark (the chevrons are only for the column). Seen: controller-only Mapping on
  the panel, scrolled to Back, and with the mark under the list; keyboard Mapping as before; Display's marks as
  before (pictures 07-10; runs -090616-jtg-s4fix-fresh-map, -090846-jtg-s4fix-final). Picture 07 is from the build
  before the mark change.
- Rejected: taking the shape from `g_settings.aspect` alone (a reviewer's idea). The per-frame check covers it and
  also covers window size changes.

## 2026-11-17 (morning): native menus, step 4: the title look for every page opened from the title (committed)
Every page opened from the title (Display, Sound, Graphics and its pages, Controls, Button Mapping, Mods and its pages,
Quit's question, the two cards, both rosters) is drawn in the title look, not the launcher's parchment.
- titlevideo.cpp `titlevideo_title_art(dir)`: writes `res/video/options-clean.png` (the kept Options picture upright,
  its painted words erased with the same ink map as the list, `find_rows`) and the scroll marks `mark-up.png`,
  `mark-down.png` (the list's pen chevrons, larger). res/video is git-ignored: the picture comes from the player's disc.
  frontend.cpp `ensure_title_art` calls it once a run, when the first title page opens. If no Options picture was ever
  kept, the log says "NOT written" and the page has black behind it.
- frontend.cpp: `game_viewport`, `title_picture_rect`, `title_box`: the game's picture in the window (as gpu.cpp), and
  the 850x480 picture in it (the title's 2D is -80..720 across in the wide layout, 0..640 in 4:3; the picture's quad is
  x -106..745). `render()` places `#backdrop` on the viewport and `#backdrop-picture` in it, and a list page's `#book`
  on picture x 150..480, y 56..476 with its letters 0.05 of the picture's height (class `title-list`; every size in its
  rcss is em). Wide pages (roster, cards, Button Mapping) keep their vh sizes on a dark panel. Title pages scroll 9 rows
  (`kTitleLines`). In the title look the scroll marks are images in the first and last row in view (`row-mark`), and a
  row's experimental tag goes into its help line. A row that opens a page has class `opener` (its arrow follows the name).
- menu.rml: `#backdrop` > `#backdrop-picture`. menu.rcss: the `body.skin-title` block (teal 72,97,104 family, lit
  0xA0E3F1 with a glow, values in IM Fell with a dark halo over the logo).
- RmlUi traps met (both fixed, both in coop-failed-attempts.md): a positioned box paints over boxes in the flow, so the
  absolute backdrop hid every wide page (fix: `z-index: -1`); and `overflow: hidden` does not clip a positioned child
  (fix: `clip: always`), so at 4:3 the picture ran into the black side bars.
- Testing switch: `ED_PAGE_TEST_KEYS="d,d,a,b|..."` presses in a page the game opened (u d l r a b, w s = the right
  stick), one every `ED_PAGE_TEST_STEP_MS` (700), "|" between pages opened in turn. Logged as "[frontend] test key".
- Seen in pictures (Screenshots for Claude/s4-title-skin/), 16:9: Display at the top and scrolled (marks above and
  below), Sound, Graphics > Effects, Controls (sections, long values), Button Mapping, Mods > Co-op, Challenge, Cheats
  (greyed), Quit's question, the cards, player 1's and player 2's rosters (runs logs/run-20261117-083544-jtg-s4-title-b,
  -084438-jtg-s4-title-final, -084649-jtg-s4-title-final2). 4:3 (aspect 1 in a 16:9 window): Display and the roster;
  the picture sits where the game's own 4:3 Options picture sits (-084055-jtg-s4-title-43, -084259-jtg-s4-title-43b).
  The Tome's Settings page and plaques as before (-084910-jtg-s4-tome).
  The last build (ensure_title_art tries again until the picture is kept) was run once more: Display, the
  marks, the log line "picture written" (-085134-jtg-s4-title-last).
- Not seen: a window that is itself 4:3 (only 4:3 pillarboxed in a 16:9 window; 21:9 seen letterboxed in a 16:9 window, see the review fixes above); a real B / Esc / mouse press
  (only the test keys); the mouse wheel; the title video OFF (all runs had it on, which does not change these pages);
  a first run with no options.rgba kept is now seen (review fixes above); the Mods root page on the title shows a
  Characters row (opens the cards: as before this step).
- Next: step 5 (the Tome look, solo).

## 2026-11-17 (morning): native menus, step 3: the title's Options as one scrolling list (committed)
The title's Options is one list of 14 rows: the game's 8, then Display, Sound, Graphics, Controls, Mods, Quit game.
Nine show at a time. "More Options" and its paint are gone. English only; other languages keep the game's 8 rows.
Our rows open their pages in the launcher look for now (step 4 is the title look).
- hle.c `title_more_*`: Up/Down sets the row count to 14 (the game's own cursor and step do the rest). A on rows
  8-13 opens page ids 6-11 (`FRONTEND_PAGE_TITLE_DISPLAY` + row - 8). The draw notify on `0x8001D52C` works out the
  scroll s (0..5) from the draw's own cursor: `s = clamp(s, cursor - 8, cursor)`. It writes a copy of quads 0-23 for
  that s into host scratch (`SCRATCH_MENU + 0xA000 + s * 0x240`), each row's quads moved to its slot, rows above the
  view collapsed to a point. A new notify on `0x8019D358` (lr `0x8001D598` only) swaps r4 to that copy, so the game's
  value words (On, 16:9, Stereo Surround) and glows follow their rows. The game's table is never written. It also
  tells titlevideo the cursor, s, and which value words show.
- titlevideo.cpp `find_rows`, `paint_options`, `options_list`: the Options picture (texture 2) split into its 8 rows
  once (52 pieces of ink; a dot goes to its nearest letter's row), the words erased (black ground), each shown row
  copied up to its slot, ours painted in La Belle Aurore 52 px (lit pale cyan with a glow), small chevrons left of the
  top or bottom row when rows are hidden. Rows moved into the logo get a soft dark patch behind them and their value
  word. The last 8 paints are kept. The 850x480 generation word now holds (lit row of ours, s, values). If the picture
  isn't as measured, nothing is painted and hle.c keeps the game's 8 rows.
- **The slots are evenly spaced** (baselines 111, 153, ... 406, 448), not the disc's uneven 39-48 px. The first try
  used the disc's baselines as slots: rows moved up looked cramped or far apart. So at the top the game's rows sit up
  to 7 px from where the disc painted them. Told the user (coop-play-test.md section 6).
- Seen in pictures (Screenshots for Claude/s3-options-scroll/, runs logs/run-20261117-075554-jtg-s3-scroll1 (the
  traced run, first look), -080018-jtg-s3-scroll2, -080444-jtg-s3-scroll3 (final paint)): the top (s 0), the middle
  (s 2, 3, 4) and the bottom (s 5); wrap Up 0 -> 13 and Down 13 -> 0; the copy taken at every s (log
  "[title] Options quads from the copy for scroll N"); quads of no size draw nothing; Audio Mode changed at s 5,
  Subtitles at s 1, Rumble at s 2 (Right does nothing there, as in the game); Volume's page at s 1 and s 4; Display,
  Sound, Graphics, Controls and Mods opened and closed by the test Back, the list back on the same row and s; B from s 5
  fades to the main menu, then Jump to Game reached chapter 1 (control at 2208, -080647-jtg-s3-back-jump); the Tome's
  plaques and a Settings page as before (-080753-jtg-s3-tome).
- **Careful in tests: Audio Mode is kept across runs** in `ed-GEDP01-sram.bin` (the console's SRAM, mmio.c). My first
  run pressed it to Mono; that file did not exist before, so I deleted it (and its .tmp) to give back the default
  (stereo). Also seen: mmio.c `save_sram` can only write that file once on Windows: `rename` onto an existing file
  fails, leaving `.sram.bin.tmp`. Not fixed (not this step's).
- After review, two fixes:
  - **Another language keeps 8 rows.** Language Select's return to Options writes no row count, so the 14 from English
    stayed: in French, Down from row 7 walked 6 invisible rows. Now `title_more_updown` writes 8 back (and puts a
    cursor past 7 on 7) whenever Options is up without the list. Seen in -081859-jtg-s3-quit-lang: French chosen, then
    Down from Choix de la langue went to Sauvegarde des parametres, then Sous-titres (log "Options without the list:
    8 rows again"; pictures 16-19).
  - **Quit game asks first.** Up from Save Settings (where Options opens) wraps to Quit game, and Quit skipped the
    game's card save. Page id 11 is now a page, `Screen::QuitAsk` in frontend.cpp: "Quit the game?", No first, then
    Yes. Back or No closes it to the list. Seen in the same run: the page (picture 14), and the test Back back on
    Quit game (picture 15). Yes was not pressed in a run.
- Not seen: a real B, Start or Esc press on a page (only ED_PAGE_TEST_BACK); Yes on the quit page; Save Settings and Language Select pressed while scrolled; Brightness while scrolled; the title video OFF on
  this build (all runs had it on, from the run settings) and the HD pack on; German, Spanish or Italian (French seen); a 4:3 window (the
  chevrons sit at picture x 150, inside 4:3's view by the numbers); a save state loaded on Options.
- Next: step 4 of the plan (the title look for every page opened from the title).

## 2026-11-17 (morning): native menus, step 2: "Choose Character" on the title's main menu (committed)
The title's main menu now reads Start Game, Load Game, **Choose Character**, Options, Jump to Game, Credits. A on the
new row opens the launcher's two-player cards (in the launcher look for now). Back closes them to the title, the row
still lit. English only; other languages keep the game's five rows. No sound of its own on reaching it.
- hle.c `title_main_*`: the row is the game's spare row 5. Up/Down: the host sets the row count to 6 and the cursor one
  step short of where it should land; the game's own step does the rest (and plays its own whispers). A on row 5 opens
  `FRONTEND_PAGE_TITLE_CHARACTERS`. A new notify on the screen draw `0x8001DF90` (state 7) tells titlevideo the lit
  row, writes quads 2-4 (Options, Jump, Credits) 34 lower in RAM (Jump 10 right too), and restarts the idle clock once
  the page has closed (the menu-closed count). A pass-through intercept on the quad draw `0x8019D3B4` skips only the
  game's row-5 glow (`lr 0x8001E430`, r28 5), a near-black box. The row only turns on after titlevideo has repainted
  the picture.
- titlevideo.cpp `paint_menu`: the main menu's picture (texture 1) is now always claimed in English. Options is lifted,
  its place filled from the hair round it, put back 34 lower, and "Choose Character" painted in the Tome's hand where it
  was (lit pale cyan with a glow on row 5, a soft dark shadow so it reads over her chin). One generation word for all
  850x480 pictures. With the video on, the words are lifted from the repainted picture (the box widened).
- tools/ed/jtg.sh: Up x2 (Start -> Credits -> Jump) instead of Down x3. Same landing with or without the row. No other
  script walks the title (check.py, coopcheck.py and the rest only call jtg.sh).
- Testing switch: `ED_PAGE_TEST_BACK=1` with ED_PAGE_TEST_MS: the timed close presses Back once instead of closing.
  `ED_TITLE_PAINT_DUMP=PATH` writes the repainted picture as PATH-lit.png / PATH-unlit.png.
- Seen in pictures (Screenshots for Claude/s2-choose-character/): every row lit in turn, Down and Up, video off
  (logs/run-20261117-073028-jtg-s2-cc-off, -073249-jtg-s2-cc-off2) and on (-073410-jtg-s2-cc-video,
  -073702-jtg-s2-cc-video2, the last paint): Options, Jump and Credits lit at their new places; no dark box on row 5; A
  opens the cards; Back (ED_PAGE_TEST_BACK) closes them, "Choose Character" still lit, Down then reaches Options. Jump
  to Game by jtg.sh reached chapter 1, room 341, both in control at 1810 (-073459-jtg-s2-jtg-ch1) and, on the last
  build, at 1801 (-073823-jtg-s2-jtg-final).
- Fixed on the way: the first hole box took Load Game's foot and moved it under the new words (a stray mark). The box
  now starts under Load Game, and the shadow leaves other words' ink alone.
- Not seen: a real B, Start or Esc press closing the cards (only the test Back); a card's roster and back to the cards;
  the idle clock after a long stay (only the log line); another language; the HD pack's title picture with the video
  OFF is now replaced by the disc's 850x480 one (the claim skips the pack). Tell the user.
- Next: step 3 of the plan (the title Options scroll, 09).

## 2026-11-17 (morning): native menus, step 1 (groundwork) of the plan in docs/research/in-game-menu/12-critic-and-plan.md
The user's ask: the title's Options lists all our options and scrolls ("More Options" goes); "Choose Character" above
Options on the title; the Tome's pages in a Tome look. Step 1 is only the groundwork. Nothing new shows yet.
- frontend.cpp/.h: page looks (`Skin` Launcher / Title / Tome, `frontend_open_page_skin`), new title page ids 5-11
  (Choose Character = the launcher's two cards, Display, Sound, Graphics, Controls, Mods, Quit), Back from any page
  root closes the menu, `in_play()` (in a game, not on the title), a hand-made list scroll (`g_list_lines`, off unless
  ED_LIST_LINES=N), a menu-closed count, and La Belle Aurore loaded. hle.c: the Tome passes the Tome look, the title's
  More Options passes the title look. menu.rcss: the scroll marks.
- Review fixes: the scroll stays off on two-column spreads (Button Mapping's keys, Mods), where its marks would only go in
  the left column. A page id that opens nothing (unknown, or RmlUi not loading) now counts as closed too.
- Seen in pictures (Screenshots for Claude/step1-frontend-groundwork/): the launcher unchanged
  (logs/run-20261117-071151-step1-launcher-after vs -070757-step1-launcher-before); the Tome's Characters and Settings
  plaque pages unchanged, plaques back after (run-20261117-071236-jtg-step1-tome); ED_LIST_LINES=3 shows three rows and
  the mark below, and again after the review fixes (run-20261117-071437-jtg-step1-listscroll, -072129-jtg-step1-fix-listscroll).
- Not seen: page ids 5-11 (nothing calls them yet), the title route in its new look, the title's greyed Cheats and
  Sanity rows, scrolling past the first view, the mark above, wheel and right stick. The skin classes have no rcss yet.
- Next: step 2 of the plan (section 4 of 12-critic-and-plan.md).

## Overnight 2026-11-17: the kit's likely co-op bugs, fixed and shown in runs (read docs/research/kit-bugs/results.md)
The kit (items 1 to 3 below) named five likely bugs; plans in docs/research/kit-bugs/plans.md, a run script per bug
beside it (build-run-*.sh, from fresh Jump-to-Game states, both players recorded, shots every 250 ms). In
runtime/host/hle.c, **uncommitted, for the user to review and play** (`git diff runtime/host/hle.c`):
1. **The duck (fn151)**: coop_sound_change_act tested the call's address 0x80164DD4, not its return 0x80164DD8. Fixed:
   player 2's lever scene apart passes over the duck once, player 1's music stays.
2. **Arrival scripts (ed113 / ed248)**: player 2's own room's queued arrival script now runs as player 2's
   (coop_be_player2 around it) when they stand in that room: ed113 reads their node.
3. **The L06 traps and the L04 elevators**: the game's step scanned player 2's body as "anyone else" and ran other_*
   zone scripts as player 1. coop_scan_player1 passes over player 2's object for that scan (lr 0x801335C8): trap runs as
   player 1 went 273/190/201 -> 1/0/1. ED_COOP_P2_OTHER_ZONES=1 keeps the old route. Elevator run not done (needs thing
   733 or 831 carried; ED_ITEM_SPAWN makes item types 0-243, and which type is thing 733/831 isn't known yet).
4. **ed177 play-start scripts**: who stored the script is kept (notify on 0x8000F9C4); the play-start run goes as player
   2's when they stored it.
5. **Hero gates (new; testing switch ED_COOP_HERO_ID=1, off by default)**: 31 scripts turn away anyone whose handle isn't
   1 (`L0 = fn78(1, 0)`, `if L0 ~= 1`), so player 2 could never pull Edward's lever (L09/1951) and the like.
   coop_hero_chunk on lua_dobuffer (0x80159D38) rewrites that one PUSHINT in the loaded chunk to player 2's id for their
   runs and back to 1 for anyone else's (the game keeps loaded chunks, so the way back is needed and was seen working).
   Shown: player 2's lever apart and together, player 1's lever after player 2's, solo (no change). `ed herogates` lists
   them; the 21 two-path compares (a branch for each character) are left for the user to decide.
Kit added overnight: `ed stops RUN [SCRIPT] [--p2] [--level N]` (each script run: who ran it, its calls, where it
stopped, hero gates flagged), `ed herogates [--all|--c]`. No hang report in any run.
A review (a reviewer per fix, two skeptics per finding) passed the duck, arrival and zone fixes; its two low findings
(ed177's owner not reset on a state load; a gate left patched in a saved state with the switch off) are fixed, and every
run was repeated on that build with the same results (results.md, "The review").
**Next:** the user plays the five fixes (play-rooms-log.bat turns ED_COOP_HERO_ID=1 on) and decides the two-path gates.

## 2026-11-17 (night, the user asleep): the launcher's settings in the game's own menus (to-do item 7, done in its first form)
The user: the launcher's in-game menu goes (hidden, debug only); everything in the game's own menus, looking built in;
in co-op only player 1's Tome Options get the extra (shared) settings, player 2's only their own Choose Character; a
first-run "compile build" for the disc-made art before release. Research: docs/research/in-game-menu/01-06 (the
launcher's every setting, the title menu, the Tome's Options page and sub-pages, the critic's checks, the texture bank).
Built (901f2e5 and after; pictures in "Screenshots for Claude/zoom/"):
- **Tome > Options**: under the cross, player 1 gets four plaques, Characters / Save States / Settings / Mods;
  player 2's copy Tome (now allowed onto Options) only their Characters. The game's own cursor, gold ring and sounds
  (hooks on the page's Up/Down 0x8010172C, Left/Right 0x8010179C, A 0x80101838, frame 0x80101980, exit 0x801016D8:
  hle.c tome_stops_*). The plaques are pictures drawn through the game's own art path: a TEXPalette of transparent
  stubs at 0x822F1000 swapped by the texture override (runtime/host/plaques.cpp), made from the game's painted Load/Save
  plaque in the page picture in memory, its words rubbed out and the entry's word in the Tome's hand. Fallback without
  that picture: a wash, a rim and the words as game text.
- **A on a plaque** opens that page over the paused game for that player (frontend_open_page; player 2's controller
  drives theirs), and Back returns to the Tome (g_root_closes). Settings has Return to title screen and Quit.
- **Title > Options**: a ninth row "More Options" painted under Language Select in the same teal hand, lit when chosen
  (titlevideo.cpp; hooks title_more_*); it opens Settings with Mods (co-op, characters) and Quit.
- **The launcher's menu over the game**: only with Options > Display > Debug menu in game, or ED_DEBUG_MENU=1.
- Kit: `ed zoom RUN FRAME [X Y W H] [--after MS]` (a run's picture cropped and enlarged into Screenshots for Claude).
- docs/release-first-run.md: the plan for the first-run step that makes the disc's art and music on the player's PC.
- The catalogue's patterns count tonight's fixes: only the looks on offer (ed219-221), four subtitle calls and the 10
  two-path hero gates are left uncovered (`ed patterns`).
**Next:** the user plays it (docs/coop-play-test.md section 6) and says what to move, rename or restyle.

## 2026-11-17 (later that night): the kit's last uncovered rows mapped, and `ed check`
- **`ed check LNN/ID`** (tools/ed/check.py, in the ed skill): one script or region checked in play from the kit's tables:
  the chapter, a door in and out (doors_table), the flags it tests named, each player put in and pressing, solo /
  together / apart runs with pictures, every run of the script compared with solo (build/check/NAME/report.md). Shown
  on Edward's lever (4bb75cd). It is the fast loop for a bug: `ed check` before, fix, `ed check` after.
- **The looks on offer** (docs/research/coop-gaps/looks-on-offer.md, with its skeptic): the kit's alarm was false
  (player 2 has their own looks since 2026-11-05; the tables are corrected). The likely real bug left: ONE thing
  offered to both players (ed221 PICK UP, follow mode or both in one room apart) and taken by one: the other's offer
  stays, and their press re-runs the find (a second "has found", the after-script again): "an item on both screens".
  Plan: after hle_coop_found, withdraw the other player's matching offer through the game's own zone leave
  (0x801170D4) with their whole set in, so their next look re-runs ed221's own taken test. Not built: needs a run
  first (two players in one pickup sphere: L01/183's Granite Block, L02/217's Bronze Necklace).
- **The subtitles** (docs/research/coop-gaps/subtitles.md, with its skeptic): the one record (0x80308DE8) has no owner.
  Player 2 answering the mansion phone (L00/1801) gets "Hello?" / "Remember me, Alex?" in player 1's half. Plan A+B:
  note who started it (r13-21496 at the start calls), and move the line into their half in the words layout hook
  (+4096, as player 2's own words). **The user decides first:** with both players in one room, should both halves show
  a subtitle? Tests need a new game into the mansion (JTG_NEWGAME, doors 1000, 1389) and the Subtitles option on.
- **The two-path hero gates**: docs/research/coop-gaps/two-path-hero-gates.md (each gate's two paths and a
  recommendation for the user). The real list is 18 compares in 6 scripts (`ed herogates` kept a slot past a later
  `local Lk = call()`: fixed). In solo the "other" branch is the creature a Summon spell controls, so player 2 takes the
  summon's branch. **One opened** (the reader's "likely real bug"): L09/L12/L13 1010 line 152 (+0x883), room 93 with
  flag 37 at the game's end, whose other branch turns player 2's model off and never starts the ending; now in
  kHeroGates (with ED_COOP_HERO_ID=1). NOT RUN: `ed check` has no route into room 93 (its door 2204 is run by no
  region). The other 17 wait for the user (858 and 1010's hub pad: lean "the character's path together, today's apart").
- The catalogue now leaves uncovered only the four subtitle calls and the 10 two-path hero gates (`ed patterns`).
Order after the user's play: the user's menu changes; the subtitles (A+B once they decide); the shared pickup offer;
the to-do list from item 4 (one "run as player N" helper, then camera modes per player).

# HANDOFF 2026-11-16 (committed through f5db56f on tools/game-kit; to-do items 1 to 3, the kit, done 2026-11-16)

## THE TO-DO LIST, in order (the user, 2026-11-16)
The kit first, so later work finds each co-op rule before play instead of during it (the user will run a session
dedicated to it; its prompt is docs/prompts/session-kit-mapping.md, revised 2026-11-16 after a read-only check of the
plan against the code, every branch and stash, the earlier transcripts and the memories: none of items 1 to 3 was built
before, on any branch; the addresses for lights, mist and effects were already worked out in docs/research/separate-rooms/).
Items 1 to 3 need no play: they are built from the disc data and the code.
Waiting on the user beside this list: the play test (the Open list below, item 1: nothing in it played yet).
1. **(Done 2026-11-16.)** **Kit: `ed needs` up to date with separate rooms.** The one-player words it knows (tools/ed/player_data.tsv) predate
   the two room slots: add the room in play (r13-23276), the slot table (0x8058AFC0), the input mode byte (0x80323AEB),
   the keep (0x80307184), the subtitle record (0x80308DE8), the mist's r13-22144 and r13-21808, the light grid
   (0x804F0784) and dynamic lights (0x80604E60), each with the host helper that puts player 2's in, or "one per game" /
   "none" where nothing does; update the busy word and running script rows (already there) with the newer helpers. Also
   the scanner's lis+addi+addi miss (the keep is never found), merged rows for r13-23040, a `--level` for
   `--script`, and a noise rule. Test: `ed needs 0x8007F254` (the clock dials' step; the room-in-play read is at
   0x8007F27C) must name the room in play; tonight it didn't. Details in the prompt.
   **Part done 2026-11-16 (uncommitted, no play needed):** player_data.tsv now has the room in play, the slot table, the
   input mode byte, the keep, the subtitle record, the mist's two words, the light grid and the 12 dynamic lights (each
   with its helper or "one per game" / "none"); the busy word and running script name the newer helpers; r13-23040's two
   rows are one. Checked: `ed needs 0x8007F254` names the room in play at depth 0 under coop_stay_room_in;
   `ed needs --script 1816` names it (depth 0, through fn25 0x8016A988); 0x801E21FC's callees show the mist words.
   **Item 1 done 2026-11-16 (the rows committed in f890944; the rest uncommitted, no play needed):**
   - The scanner (know.py scan()): chained addi and lis+ori are followed and tested at every step, so `ed needs
     0x800B7100` finds the keep; a register forgets its address on any other write, a load into it, an update-form base
     and across a call (r0, r3-r12); a base plus an index (add) and mr copies keep it. Old against new over the 899
     functions the script commands reach directly: nothing lost, 23 hits gained, all inside tables already listed
     (e.g. the magick record through an mr copy in 0x80109DC8).
   - `ed needs --script ID --level N` and `--script L00/ID`; a script id in more than one level says it's merged. Found
     on the way: the 642 ids shared between levels call the same commands in every level, so merging never changed a
     result; the option is for the catalogue's per-level rows.
   - The noise rule: a read whose value the function branches on within 6 instructions of the same path (a cmp, a
     dot-form test, or the compiler's branchless subfic/xori "is it N") is "compare" (it picks by it), else "read"; a
     call ends r0/r3-r12, so a call's return value isn't counted; a "Picks by, within one call" line names those at
     their shallowest compare. Of the 413 script commands, 68 reach the room in play within 3 calls, 37 compare it
     somewhere, 21 within one call, 8 in the command itself (EDCountChararactersInRegion, EDSpecialRegionCheck, ed162,
     ed24, ed253, ed265, ed3, fn148). fn25 only returns it: scripts compare it themselves.
   - Reviewed by a workflow (2 reviewers, a skeptic per finding): 9 real defects found in the first version and fixed
     (a crash on call-row roots, false compares across calls and branches, update-form loads and stores now move the
     base instead of losing it, missing lhau/lfsu/lfdu/stfsu/stfdu/psq forms, dot-form tests, the Picks-by depth, the
     header with an address and --script). Known limit kept: the walk is in address order, not by path.
   - A table's hits are one row with the offsets seen; the header names the script, or says a decomp name's number is
     the USA address (fn_8008210C is this disc's 0x8007F254).
   Then items 2 and 3.
2. **(Done 2026-11-16.)** **Kit: a per-room catalogue generated from the disc** (`build/ed-db`: 297 rooms, 2,832 regions, 2,172 script ids in
   4,404 level files): every level, room and region with what it runs (examine words, prompts, items given or taken,
   doors and where, flags, input modes, cutscenes, spawns, cameras), names and IDs, plus a co-op column (the per-player
   words each interaction reaches and which helper covers it). Written as docs per level, by one new `ed` command over
   the existing readers. New pieces: each room's own load script (the room 81 body's path), the 25 rooms in no level,
   flag-off calls, names for cinematics, films and input modes.
   **The catalogue, built 2026-11-16 (committed checkpoint by checkpoint: 76c7321, 29b2143, 3ec6720, 2a4e839, 16c71af;
   no play; item 2 done):**
   `python -I tools/ed/ed.py catalogue` writes docs/game/catalogue/ in about 2 s: index.md, level-NN.md and level-NN.tsv
   for the 15 levels with rooms, and unplaced.md (25 rooms in no level). Per room: its own scripts, doors out, every
   region's scripts grouped, call by call with the nearest tests guarding each; camera-only scripts summed; a shared
   script (L00/1816 in rooms 70, 81, 87) shows only the room's own branch, by following its fn25 tests over the jumps
   (catalogue.room_states); other calls summed per script. `ed catalogue --script L00/ID [--room N]` for one script.
   `ed needs` is callable (know.Needs). **The co-op column (2a4e839, then the review's fixes in 16c71af):** each row
   lists the per-player words its call reaches, as hooked (a live host hook on the script function or on the function
   holding the compare), keyed, per run, routed, helper (player_data.tsv names one; whether it's in force on that run
   isn't known here) or one per game (legend in index.md), and is flagged when a word is branched on within one call
   and none of hooked / keyed / per run applies. A cmp against 0 or -1 is a "zero-test", not a compare.
   **The report: 1,016 rows flagged over all levels** (level 12 199, 09 175, 00 124, 07 85, 10 68, 03 55, 02 49, 05 47,
   08 47, 06 44, 04 41, 11 29, 13 24, 01 16, 16 13). By word: the room in play 615 (by call: ed3 spawn 285, ed24
   nearest_class2 119, fn148 stream_prepare 69, EDCountChararactersInRegion 54, ed91 and ed155 16 each, ed162 15, ed248
   10, ed157 9, ed156 7), the script words 173 (all fn35 state_keep), the room in play inside an input mode's own code
   133 (ed56 86 and ed68 47: modes 9/10 rune rooms, 11 candles, 2 piano/organ, 3, 4), the look being set up 93 (all
   ed221), the magick record 2. Per row in the TSVs' coop_flagged column. The two biggest are what the
   brackets around player 2's own runs put in (coop_use_start, coop_run_script's first block, coop_stay_room_in with
   coop_states_swap); a flag matters where a run happens outside them (other_enter scripts, queued scripts, timers,
   an arrival run for player 2 with player 1 still the character). A flag is a place to look, not a bug found.
   **The likeliest real co-op bugs the catalogue and its review found (read only, none run or fixed):**
   (1) hle.c:15084's lr == 0x80164DD4 can never match (item 9: fn151's duck by player 2 apart ducks player 1's music).
   (2) ed113 and ed248 in rooms' arrival scripts (L08/2079 room 202, L12/2477 room 90, L00/1672 room 67, L10/1814 room
   27, L05/1774, L10/2238, L11/1877, L12/2777): coop_run_script's first block runs player 2's arrival with their room
   in (coop_room_in) but not coop_be_player2, so ed113's spell test (r13-21484, main) and ed248's "is the controlled
   character in the room in play" (0x800DC46C) answer for player 1 in player 2's room.
   (3) ed156's trap records in L06 (2105, 2106, 2108 room 168; 1618 room 233; 1711 room 306; 1781 room 308; 1741 room
   313) pick a record by the room in play (0x800849AC) and run from both TrapRgn enter and other_enter: player 2
   tripping one apart through other_enter gets player 1's room's record or none, unless that region event runs inside
   coop_stay_room_in (the R6 bracket at hle.c ~24330 covers player 2's own events, not other_enter scripts).
   Refuted by the review's skeptics: ed10 magick_give giving player 2's rune to player 1, ed162's teleport, fn148's
   room-39 case.
   **Item 3 done 2026-11-16 (f7f4c38, then a review's 12 real findings in f5db56f; no play): `ed patterns [KEY]`,
   tools/ed/patterns.py.** The nine known patterns, each with its detection rule over catalogue rows, the fix in hle.c
   (definition line and commit; a fix can name its own commit where the symbol is older than the fix) and how far the
   check can be trusted. Every row is tagged (TSV column `patterns`); counted in units, per script or per distinct
   call, each covered, bracket (covered only when player 2's run goes through their bracket: a region's own
   enter/leave/stay script), uncovered or n/a (a run-time fact). Counts and uncovered units with places in
   docs/game/catalogue/patterns.json and index.md; `ed needs` names the patterns of what a function picks by. Found on
   the way (hle.c's own comment at coop_run_script): a script run next with fn73 runs inside the same run (0x80164488),
   so it keeps the bracket of the script that ran it on; the review had assumed it ran later.
   **The counts (units: covered / bracket / uncovered / n/a):** room 264: 80 / 182 / 2 / 0; identity 1,512: 1,260 /
   249 / 3 / 0; global 30: 23 / 0 / 7 / 0; stamped 197: 122 / 75 / 0 / 0; later 575: 546 / 0 / 29 / 0; change 252: 205
   / 0 / 0 / 47; state 846, fills 3 and camera 358 all n/a (whether two players meet in them is a run-time fact).
   **The uncovered fix list:** (a) rooms 189/190's elevator scripts L04/2301, 2726, 2738 run only from other_* fields
   (for anyone but player 1: ElevatorRgn01/02 other_leave, MonsterCheckRgn00 other_enter), outside player 2's bracket,
   and pick by the room in play and the zones' words: the same shape as bug (3), the L06 traps. (b) the looks on offer
   (ed219 315 places, ed221 93, ed220 85): one copy, never swapped; fine one at a time, uncovered if both players look
   at once. (c) the subtitles (EDStartSubtitlesForAlexClockPuzzle in the clock's L00/1551, ...GhostMaid L00/583,
   ...InsanityPhoneCall and EDCleanUpSubtitles L00/1801): one record, no fix. (d) 29 scripts leave a script for play
   mode's next start with ed177 (L*/1188 in room 83 in every level, L03/1011, 933, 2465, L04/2346, L05/929, L06/1981,
   L11/2407, L12/2196): hle.c has no handling of r13-24248, so it runs as whoever is the player then.
   **Ready for testing (2026-11-16, read only, nothing run): docs/research/kit-bugs/.** plans.md has the five likely
   bugs, each traced in the code by a reader and checked by a skeptic, with the smallest fix and a test plan; all five
   came out likely real: (1) the duck (one character, 0x80164DD8u), (2) arrival scripts run for player 2 with player 1
   still the character (coop_be_player2 around coop_run_script's first block, guarded on player 2 standing in that
   room), (3) the L06 traps and (4) the L04 elevators (the game's own zone look runs a region's other_* script for
   player 2's body beside the host's run as theirs, with player 1's room in play: one rule for both), (5) ed177's
   play-start scripts (record who stored it, run it as theirs at mode 2's enter). build-run-duck.sh, -arrival.sh,
   -traps.sh, -elevator.sh and -onplay.sh run each test: their header says the steps and their order (a probe, a
   fresh state on the build, the runs before the fix; then the fix from plans.md, a rebuild, a new state and the same
   runs after it), what each step's output means and what only a run can show. Each was written by one agent, checked
   by a second and corrected by a third; `sh -n` only. Every run ED_RUN_AWAY=1, from fresh Jump-to-Game states.
   **Next, when the user asks: run those before-fix steps, then the fixes one at a time, then to-do item 4 (one "run
   this as player N" helper) or item 5 (the play-check runner).**
   **Groundwork done 2026-11-16 (committed in ec7f923; every field read from the code that uses it, by a reader
   and a refuter each: docs/game/research/kit-groundwork.md):**
   - `ed cinematic [N|TEXT]`, `ed film [N]`, `ed mode [N]` (tools/ed/facts/cinematics.py -> facts/cinematics.json): the
     173 cinematics (ed55: level, room, start and end scripts, sound, files, alignment, seen flag, chapter, what plays
     next, the room queued; 139 have both files on the disc, 118 are played by a script, 3 of them through a variable
     the script sets: L01/655's ed55((L0 + 1)) plays 2, 3 or 4), the 65 films (ed83: fmv files, end script, alignment,
     -1 any; 9 have no video on the disc, and a sound file that isn't there means no sound of its own) and the 12 input
     modes (ed56: 0 the staff, 1 the safe, 2 piano and organ,
     3 the mantelpiece tiles, 4 the mirror cranks, 5 the bomb countdown, 6 nothing, 7 the opening dream's B and Start,
     8 the clocks, 9/10 scribing a rune, 11 the candles; handlers, which read the room in play, who starts each).
   - Each room's own scripts in rooms.json (`ed build rooms`; `ed room N` shows them): arrival (+136: 191 rooms) and
     leave (+138: 65 rooms), from no region. Room 69: 2530 / 2430; rooms 70, 81, 87: 1816 / none.
   - The hook map (tools/ed/facts/hooks.py -> facts/hooks.json, read from hle.c without running it): 404 rows on 378
     game addresses (260 intercept, 144 notify), none unresolved, the same set as a reader's hand check; 29 name-table
     rows name SDK symbols this disc lacks. Each row says when it may not be in effect: 45 are logs or traces, 22 share
     a slot with another install (which one holds it, from their == NULL checks), the GX ones need ED_GX_HLE=1. Also
     216 return-address filters (cpu->lr == X) in 93 calling functions. `ed fn ADDR` lists a function's hooks and the
     calls of it that a handler picks out by return address; `ed needs` marks with * the functions with a live hook. It
     can't know whether an install's checks pass on a given run, install order beyond the == NULL checks, or the
     return-address lists kept in arrays (coop_interface_ran's kRan).
   - Reviewed by a workflow (3 reviewers, a skeptic per finding): 13 real defects fixed (a call by a variable missed,
     the rune modes' tables, missing film files shown as present, the search, alignment -1, the disc's spelling, slots,
     debug hooks, the GX switch, the arrival script's wording, return-address hooks, ed fn's "not hooked"), then a
     re-check of those found 8 more, fixed: a comment that hid two cinematic fields, sound number 60, the room scripts'
     wording, the search matching field names, return-address rows read backwards and filed under the wrong function
     (function starts now include prologues after a blr), `NAME + 4u`, and debug installs taking a slot. Each
     return-address test is now checked to follow a call (that found the fn151 bug in item 9).
3. **(Done 2026-11-16, see item 2's notes above: `ed patterns`.)** **Kit: the known patterns as checks** flagged per interaction and in `ed needs`: asks the room in play; who "the
   player" is; one global per game (busy word, input mode, keep, show and hold words, prompts, subtitles: subtitles have
   no fix yet); one script state for both; tables player 2's things fill; stamped with the room when made; queued for a
   later frame; run inside player 2's change; one camera. "Covered" per address, from a map of hle.c's hooks.
4. **Code: one "run this as player N" helper** taking a game function and a player and swapping in what the kit says it
   needs (room, cameras, prompts, input copy, identity, states), for new features to reuse instead of their own swaps.
5. **Kit: a play-check runner** from fresh Jump-to-Game states: each player into each risky region (from the catalogue),
   the press or script, both players' call paths compared; differences reported. Unattended, ED_RUN_AWAY.
6. **Camera modes per player** (third and first person in co-op): a reading pass first; one camera brain per player in
   camera.c (all its state per player, fed by that player's body, room and stick); the game's cameras win in scenes,
   close-ups, doors and puzzles as now. Player 1's brief camera jitter around player 2's clock first (needs a 60 fps
   picture run and the moment named).
7. **The native in-game menu in the Tome** (coop-todo.md "the overall direction"): the Choose Character button as a real
   cursor stop (read the Options page's cursor code first), the Tome's own lettering, Back to the Tome; then every launcher
   page as a Tome-skinned overlay; then player 2's in their half.
8. **Showcase**: clips recorded with the user, a README section (media via YouTube/GitHub uploads, never in the repo); the
   README's Getting started already points to Releases (committed, not pushed yet).
9. Open co-op items: chapter 13's Tome room, Chapter Pages in the real mansion, the ladder's Yes in the other's door
   (M13), independent sanity effects per player. Found by reading (2026-11-16, not run): 0x8007E508, the MOVE STAFF EXIT
   step (input mode 0, L11/1384's ed56(0)), reads the room in play at 0x8007E7D4 with no room hook, as the clock did
   before coop_dial_run. It isn't one of mode 0's bindings (0x80244920); it runs from ed68(0)'s stop and from a
   per-frame room updater, 0x80083464, which picks by the room in play itself (0x8008347C). The interface code has 50
   direct reads of that word (0x8007A968..0x80083700); the kit's catalogue should list which are uncovered.
   **Found by the kit's hook map (2026-11-16, not run): hle.c:15084, coop_sound_change_act, tests `cpu->lr ==
   0x80164DD4u` for fn151's duck (sound_duck, 0x80164D58), but 0x80164DD4 is the `bl 0x8019E068` itself: its return
   address is 0x80164DD8, so the test never matches.** The other return addresses in that function (0x801668FC,
   0x80166970, 0x80152354, 0x8015206C, 0x8015207C) are right. What it means, by reading: player 2's duck while apart
   (fn151(1) at a look's start, e.g. the clock's L00/1551) is not passed over, so player 1's music ducks with it.
   Likely one character (0x80164DD8u); confirm in play before and after. **(Fixed 2026-11-17, uncommitted, shown in
   runs: see "Overnight 2026-11-17" at the top.)** `python -I tools/ed/facts/hooks.py` prints
   every such test that can't match (only this one today).
10. Later: reflections and the cheap visual upgrades (automatic surface detail, rounder models, a cinematic finish);
    online co-op (stream player 2's view first); sharing the PAL names with the decompilation.

## The evening (committed in 74a7989 and f8e5d82; the dream, the body and the clock played by the user): an audit of the afternoon, every check re-run, and the Chapter Page keep fix
- **Audit** (a read-only workflow over b42fb68..821e1ac): solo safe by code, controls.cpp never committed, but several
  checks named in the commit messages had run on builds older than the committed code, player 1's page from a copy Tome had
  never run, Max's letter never reached its end, and ED_RUN_AWAY could not be shown from any log.
- **Tooling:** every run folder now has env.txt (all ED_/JTG_ switches, so ED_RUN_AWAY=1 shows); build-run-fx.sh sets
  ED_RUN_AWAY=1; Max's m1/m2 press B through the letter (flag 490 now on); a repeatable Chapter Page script,
  docs/research/build-run-pages.sh (p1 p2 p1a p2a both bothx noitem p1copy solo door; JTG_SLOT 2, PAGE_SLOT=3 beside it).
  Testing switches (off unless set): ED_STALL_MS (the stall watchdog's gap, dumps each slow frame), ED_PCDUMP_WORDS=ADDR[:N]
  (registers and guest words with each dump), ED_PROBE_WORD=ADDR (which host hook changed a real-memory word: memcpys the
  store watch can't see).
- **Re-run on the current build from fresh states (all pass):** the Tome trip ch 4 (z4), 6 (6a, 6b), 8 (z8a, z8b), Max
  together and apart (to the letter's end), solo, follow, off; the Chapter Pages p1, p2, p1a, p2a, both, p1copy (player 1's
  page from a copy Tome: first run ever, works), solo (room changes identical), door. Chapter 4's second trip at ~5065 in
  every z4 run, old and new, is the game's: the zombies knock idle player 1 back into the ladder's walk-in zone.
- **Fixed: the Chapter Page keep overflow** (hle.c keep_request_hold, keep_requests, keep_walk, keep_filter; tome_copy_film's
  order; the copies' own id). The game's state keep (ed88) has no bound and Jump to Game's chapter 12 already fills it to
  0x80308D88 in solo; co-op added player 2's body, their weapon copy and the used page, onto CHAPTER_PARTS (0x80308DB8:
  "body part ... not 262144") and the subtitle record (0x80308DE8: the both-Tomes route's 2-4 s frames, its updater scanning
  ~20M entries). Now player 2's things are left out of the keep's own walk, the world's frames behind player 1's real Tome
  leave the keep for that Tome's close (as solo), and the copy's no-Tome film runs the close's own order (mode,
  0x80025FF0, keep). Every route writes solo's keep exactly: 159 entries ending 0x80308D40, no stall
  (logs/run-20261115-1741*..1749*-jtg-page-*). The whole trace and the failed tries: coop-failed-attempts.md (last entry).
- **Separate rooms are now co-op's default in every launch** (frontend.cpp rooms_follow_coop): a launch that names
  neither ED_ROOM_SLOTS nor ED_COOP_STAY (the exe alone, the Mac's play.sh, a shortcut, a release) gets both, as play.bat
  asks; one that names either keeps what it names (the scripts' follow mode: ED_ROOM_SLOTS=2, ED_COOP_STAY unset); =0 is
  off; solo never. Checked: a co-op run with neither named logs every install (rooms, stay, fog, effects, Tome trip with
  all hooks, split hold, keep) and nothing "NOT installed"; solo the same as before. A sweep of every getenv found no
  other fix or feature off by default (ED_FOG_VIEWS stays off by design).
- **Story changes take both (committed in f8e5d82; played by the user 2026-11-15 evening: it worked, on the build before coop_follow's "left room marked gone", which the dream's bring now also takes: not re-run since; the user's play 2026-11-15 evening: after a new game's dream player 1
  went to the mansion, player 2 stayed in the dream, room 96).** The stay treated every ticket of player 1's as their door.
  Now a ticket asked by a script the game's event queue started (0x80025FF0's row, lr 0x80026FE4: a film's or cinematic's
  next script, the dream's 1920 to room 69) or by a cinematic's own ticket (lr 0x8012D2F0: the dream's end cinematic into
  room 205) is a story change: no stayer window, and once player 1 is in the room it asked, coop_follow brings player 2
  beside them and tells their old room (event 61) (hle.c coop_story_ticket, g_story_bring). Checked once from a new game
  (logs/run-20261115-211200-jtg-dream-end-story4: "player 2 brought from room 96 to room 69 with player 1"). Seen on the
  way: with no input the dream's end cinematic holds in room 205 a long time (the game's own; the user's play skipped it).
- **A room player 2 enters first is set up as that room (committed in f8e5d82, played fixed below; the user's play 2026-11-15 evening: the
  hanged body in room 81, L00 tag 691, by the window at the game's start).** Level 0's room script L00/1816 is queued by
  every room load (0x80152430 -> 0x80163C6C at lr 0x80153B6C) and run a frame later from the waits (lr 0x80163E5C); its
  room 81 branch hides the body (`model_off`) and shows it only for Alex at insanity tier 2. Player 2 went into 81 first
  (their change runs as theirs, R4's bracket), so 1816 ran after the bracket with player 1's room in play: room 81's
  branch never ran and the placed body stayed shown. Now the room script queued inside player 2's change is noted
  (notify_script_later, always installed; the log still only with ED_ROOM_LOG) and run once with that room current
  (coop_run_script's first block: "[stay] room N's own script S (queued by player 2's arrival) ran with room N current").
  Not reproduced by script: in three runs (logs/run-20261115-2126*..2132*-jtg-room81-body*) player 2's B on room 69's
  Region08 door went through only after player 1 had entered 81 first; why their door waited is not traced. The user's
  play shows it: look for that "[stay] room 81's own script 1816" line in logs/play.log.
  **Played by the user (2026-11-15 night): fixed, the body no longer there.**
- **Player 2's clock dials (committed in f8e5d82, played fixed below; the user's play 2026-11-15 night: player 2 in the study, player 1 in the
  main hall: the dials didn't move for player 2, the instructions were in player 1's half).** The clock (L00/1551) lets go
  of its hold and starts input mode 8 (ed56(8), 0x8007AA6C; the mode in play is byte 0x80323AEB, -1 none); with busy 0 the
  pad routing ended player 2's close-up ("play goes on: the pad is player 1's again"). Now a mode begun while player 2 has
  the screen (coop_input_mode_start) keeps their close-up, pad and half until the byte leaves it ("[coop] input mode 8
  begun in player 2's close-up: theirs" ... "is over"). Not run by script (the clock is deep in the mansion's story).
  **Fixed and played by the user (2026-11-15 night, slot 3): the hands turn and the puzzle completes, the icons right.**
  The dials' every press (0x8007F254) and their end (0x8007F140) ask "is the room in play 78" to pick the clock: run with
  player 2's room current while the mode is theirs (coop_dial_run; the start was already theirs inside their use). The
  doubled B and garbled words: their zone's own prompt (the clock's EXAMINE, prompt 6) was drawn over the mode's prompt;
  not shown through an input mode of theirs. Open: player 1's camera "jitters for a slight moment" around player 2's
  clock; not in the 3-a-second pictures (logs/run-20261115-220457-clock-p2-icons: player 1's half still at every
  transition); needs a run with ED_RUN_SHOTS_MS=16 and the moment named.
  Earlier state of it: **(the user's slot 3): their icons on their side now, BUT the B icon is doubled, and the hands don't turn
  though the sound plays.** Open, not traced: the hands are things of player 2's room (78) while player 1's (69) is in
  play; the mode's step or the hands' update likely tests the room in play (as the room script did). Next: read the
  mode-8 step (0x8007AA6C's case 8 and its per-frame handler) for room tests, and where the B prompt is drawn twice.
- **A state saved apart loads apart (committed in f8e5d82; the user's slot 3: library things in the foyer after loading).** The
  host learned the two slots' rooms only at the next room draw; coop_follow ran first, didn't find player 2's room (77)
  in a slot, and "brought" them into room 69 while 77 stayed loaded and drawn. coop_state_loaded now reads both slot
  records at the load ("[rooms] the load's slots: 69 and 77"). Checked with the user's slot 3 on a copy exe
  (logs/run-20261115-214728-slot3-load-fix: player 2 stays in 77).
  **Then the user's rule (2026-11-15): any load of a co-op save state brings player 2 back to player 1** (as a memory card
  save does, coop_adopt), so states saved apart need no mapping. coop_state_loaded sets the story change's bring
  (g_story_bring) when the load is apart; coop_follow brings player 2 beside player 1, sends event 61 to the room left and
  now marks that room's slot record gone as the change's tail does (room -1, loaded 0) and forgets it, so its things are
  neither stepped nor drawn (also for the dream's bring). Checked with slot 3: "player 2 brought from room 77 to room
  69", "slot 1's room 77 left by both: its record marked gone", the library's table, bell jar and board gone from the
  foyer's picture (logs/run-20261115-215219-slot3-load-bring2/shots/000005.jpg).
- **Still for the user's play test** (the Open list below, item 1) plus: player 2's page while player 1's Tome is open.

## Where things stood at the afternoon handoff (committed through b049fc3)
Everything below is committed and checked by scripted runs from fresh Jump-to-Game states. **None of it has been played
by the user yet**: their next step is a play test (list at the end). Commits, newest first:
- **b049fc3 Chapter Pages.** A mansion Chapter Page used by either player plays its film full screen for both, then the
  chapter's opening, then starts the chapter with both players side by side, as Jump to Game does (player 2 is re-made
  beside player 1 by the usual coop_spawn after "player 2 is gone"). Player 1's page already worked; player 2's page (their
  copy Tome) used to change chapter at once with no film. Fix: `tome_copy_film` (both players, in hle.c next to
  tome_copy_p1_left), called when an item's use step asked the Tome's close for a film (r13-24244 == 6, ed9(script, 6,
  film)): the film's mode (0x80018808) then the page's state keep (0x800B7208(4), ed88), as the real Tome's close does;
  with player 1's real Tome open, nothing is forced and that Tome's close plays the film. Found on the way, a general copy
  Tome bug: the copy writes its pages over ARAM's level-file caches and marked them gone only in its own memory, so the
  real game read the Tome's pictures as level data (the keep halted on an assert). `tome_copy_aram_words` now carries the
  copy's "gone" (r13-28976/-28972 = -1) to the real game after every copy turn and at the close.
  Tested from Jump to Game chapter 12 (Alex End; its scripts hold pages 1572-1580) with the chapter 2 page spawned
  (`ED_ITEM_SPAWN=2300,200,200,120,WHO`), runs logs/run-20261115-1149..1240-jtg-page-*. Readers:
  docs/research/chapter-page-readers.md, chapter-page-keep-reader.md. Failed tries: coop-failed-attempts.md (last entry).
- **fbd1ecf Max's Tome.** The Tome trip's room is now a value (`g_tome_trip.trip_room`, `TRIP_ROOM`, `tome_trip_way_in`):
  room 5 for the seven Tome-room chapters, 255 for Max (1599's model-18 branch: film 114 in his desk room 258, the world
  changes, 255's summon scene, back to 258 for the letter). Both go to 255, each comes back to their own room and spot.
  Run script docs/research/separate-rooms/build-run-tome.sh (`s5 m1 m2`; `s5` sets flag 184 so 253's passage 930 opens).
- **b42fb68 Tome trip + spells apart.** The Tome trip (both players to room 5 and back to their own spots; ch 4, 6, 8
  checked); player 2's spells while apart (they used to die on their first step: the game ends a spell whose room isn't
  the room in play) now step in their own room, with the bolt, queued effects, runes and rune flames in the caster's room
  and the spell's lights and full-screen flash in the caster's half only (ED_SPELL_ROOMS=0 turns the spell part off).
  The "green glow on player 1" seen during player 2's cast was room 151's own blue lamp (same with no cast, and in solo).
  Run script docs/research/separate-rooms/build-run-spell-lights.sh (s3 a b c d).

## How the user wants this worked on (read the memory index too)
- Take full control: trace and test without asking, no fix without evidence, check docs/coop-failed-attempts.md before a
  fix and add every failed attempt with its code. Engine-generic fixes (the game's own functions, every chapter, both
  players); co-op changes never change solo; test solo too.
- **Never run `ed test` or `ed chapters`, never use old save states**: fresh Jump-to-Game states made on the current build
  (tools/ed/jtg.sh, or the run scripts' `s*` steps), remade after each code change.
- **Every test run with ED_RUN_AWAY=1** (window off the desktop, muted) until the user asks to see the game. To stop runs,
  kill the whole tree: the run scripts, sh children, powershell helpers, python (pagelog, wait_frame) AND the game copies
  (EternalDarkness.exe and EDrun*.exe); a leftover EDrun1.exe blocks every later run.
- Commit only when told, never controls.cpp or tools/ed/.run_exe_tmp.sh; push only when asked, only to the fork's main.
- Edits with backslashes (`\n`, `\|`) through the Edit tool, never sed or Python heredocs; Python edits keep LF. Never
  edit a run script while sh is running it.
- Ultracode is on: readers/refuters/reviewers as workflows for substantive work, then build, run, review, fix.
- After .deps/GXRuntime changes: rebuild and regenerate patches/gxruntime/eternal-darkness.patch. Build: the scratchpad
  build.bat (VS2022 tools, `cmake --build build-win --target EternalDarkness`), called from PowerShell.

## Open, in order
1. **The user's play test** (nothing above has been played) (the overnight fixes: docs/coop-play-test.md): (a) the Tome trip apart and together, either player taking
   the way in, including player 2 being the one who picks the Tome up (scripted runs can't press through the Tome's
   EXAMINE words; they start the after-find script 1599 as player 1's); (b) Max's Tome apart; (c) spells while apart,
   quick and from player 2's Tome (the Tome cast apart is built but unrun: chapter 3's list had nothing to enchant);
   (d) a Chapter Page by each player in the real mansion.
2. **Chapter 13's Tome (Alex)**, offered to the user, not yet decided: room 64, script L13/1828 saves the place and camera
   (ed164, fn134(1,5)) and plays film 134, which loads room 5 directly (mode 6, 0x801525D4); 1413's fn66(64, EntryMkr01,
   FightCam04, 2, 1828) brings the reader back and 1828 restores the saved place. Plan in design-tome-room.md §3 P12: start
   the trip at notify_room_change with r3 == 5 from that film (lr 0x801525FC), both go, each back to their own spot.
3. **Chapter Pages left:** the real mansion between chapters (level 0, reached through the prologue; JTG_NEWGAME=1 in
   jtg.sh); the chapter 3 and 11 pages (1573, 1581: they play films in-level with ed55/ed83, then ed87 inside a use of
   player 2's: check it); a chapter's end back to the mansion (ed89 restores the keep at 0x80307184); the mansion Tome.
4. Smaller separate-rooms gaps (docs/coop-todo.md): a ladder's Yes answered in the other's door is lost (M13); examine
   and pick up during the other player's cutscene; the Tome room's first-entry scene camera in player 2's half.
5. Saving is DECIDED (the user, 2026-11-15): only player 1 saves; any co-op save loads both together where player 1
   saved (built, see coop-todo.md "Saving"); only player 1 can open the save-state pause menu. Later, theirs: an
   Options page in each player's Tome with character selection (coop-todo.md); "Bring X to Y" in the co-op menu; the randomizer last.

# HANDOFF 2026-11-14: effects per room and per camera, uncommitted on top of 96745f8

**Built and checked by script from fresh Jump-to-Game states, NOT committed, NOT yet played by the user** (ED_FX_ROOMS,
on by default with separate rooms; ED_FX_FACING=0 turns off the per-camera cards; ED_FX_KINDS=1 logs every effect drawn
per room): every effect kind steps with its own room current (fx_ticks, 0x80182F98), the emitters that make them are
walked with their own room (fx_emitters, 0x80144B34), and each half's effect cards face its own camera, apart and
together (fx_facing_begin in effect_draw_by_room). Karim's altar flame (room 338) burns in the stayer's half and is no
longer a flat sliver from player 2's side; chapter 1's 180 braziers burn in player 2's half when entered apart.
docs/coop-todo.md (the brazier item), run script rooms/build-run-fx.sh, pictures Screenshots for Claude/fx-flames/.
- Checked after: player 2's lighting by the braziers apart matches together on the same spot (not missing; coop-todo.md).

# HANDOFF 2026-11-13 evening: the stayer keeps every action through the other's door, uncommitted

**Built and checked by script from fresh Jump-to-Game states, NOT committed, NOT yet played by the user:** while one
player's door counts down, the other (the stayer) keeps their presses, together or apart, both directions: EXAMINE and its
words, zone looks, quick spells and the 0x810 spell (cast at once; a summon is kept to the door's tail), Start (their Tome
in a copy, with no door fade in it), B on the same exit (kept, goes in after; pressed once more if the door's shared script
state was set back). Player 1's close-up now survives player 2's arrival (F2), and no copy Tome goes dark in a door (F3).
The mover's own Start is refused in their door's fade, as the game refuses the hero's. Switches: ED_STAYER_PRESSES (unset/1,
p1, p2, 0), ED_SPLIT_HOLD. Docs: rooms/design-stayer-presses.md + stayer-presses-impl-notes.md (runs table), rooms/
design-stayer-findings.md + stayer-findings-impl-notes.md (runs table), run scripts build-run-stayer-presses.sh and
build-run-stayer-findings.sh. Quick casts never work in chapters 1-2 (0x800069DC reads the chapter word): cast checks use
chapter 3 with ED_CHEAT=1 (stayer-test-setups.md). **`ed test` is NOT to be run**: its scenarios load old slot saves.
- Still open: a ladder's Yes answered in the other's door is lost (M13, needs read R1; always printed); player 2's
  lend-route use/scene at player 1's door makes player 1 wait in the black (C2, R4d); 180's brazier flames and their light
  missing in a half where 180 is not the room in play (readers next).
- **The mist per room apart, built the same evening (uncommitted, not yet played):** the fog the user saw follow the room
  in play was the game's own mist layer, every input taken from the room in play. Each half now draws its own room's mist
  (ED_FOG_ROOMS, default on with separate rooms; ED_FOG_VIEWS, the renderer's per-half depth, default off). Checked from a
  fresh state both ways round, turned order, a load, together, solo: rooms/fog-impl-notes.md "Runs"; pictures in
  Screenshots for Claude/fog-rooms/. Run script rooms/build-run-fog.sh (no ed test, no ed chapters).
- For the user's play: see "What to play" in the session's last message (both directions, a door during an EXAMINE, a
  quick spell in chapter 3+, Start, both at one door).

# HANDOFF 2026-11-13: + the stayer's room plays on through the other's door (Phase 1b), uncommitted

**Built and checked by script, NOT committed, NOT yet played by the user:** while one player's door counts down (the
~1 s before the room changes), the other player's room no longer stands still, together in one room or apart (the user's
own case, measured held before the change: logs/run-20261113-124840-split-k3-apart-today). Its zombies walk, swing and hit
the stayer; the player going through the door stays protected as in solo. docs/coop-level3-design.md "The change freeze,
Phase 1b"; design rooms/design-split-hold.md (§18-19 final), impl notes rooms/split-hold-impl-notes.md, runs
rooms/build-run-split-hold.sh. ED_SPLIT_HOLD=0 brings back the old hold (testing). (`ed test` was run on its old slot saves: that proves nothing and
breaks the fresh-states rule; it is not to be run until its scenarios start from fresh Jump-to-Game states.)
- Still held for that second, by design: the stayer's presses (quick spell, B, zone looks, player 2's Start), flame
  flicker, sanity hits. Next visible stop: the arrival's event 61 to the room left lands on a live fight at the firing.
- Not reached by script: a kill or a grab or a cast inside the window, item doors, zone exits, CLIMB DOWN from a look.
  In the user's play look for always-printed `[split]` lines in capitals or "refused" / "inside the countdown".

**The user's next play** is as below, plus: a door taken while the other fights (both directions, same room and apart):
the fighter's screen should no longer pause.

# HANDOFF 2026-11-12 (evening): uncommitted on top of 5eb040d: the arrival camera, lights per room 1+2, the floor probe, the change-freeze measurement, the restart log fix

**Built and checked by script today, NOT committed, NOT yet played by the user** (each in docs/coop-level3-design.md under
its own heading; designs, readers and refuters in docs/research/separate-rooms/):
- The camera at an arrival apart: player 2 arrives on their own door's camera, no glide from player 1's eye; player 1
  coming into player 2's room leaves player 2's camera alone. Kit: `ed check` ARRIVAL rule.
- Lights per room, stage 1 (the game's own lighting of player 2's room by its own grid and dynamic lights) and stage 2
  (two lamp sets, shadows and haze per view, a camera per view; renderer in .deps/GXRuntime, patch regenerated).
  ED_LIGHTS_OFF=1 turns both off to compare. Not done: P7 (spell lights by the caster's room). L3 decided by the user
  ("truly built by design"): player 2's body lit as a hero too (the room's nearest lamps), co-op together and apart.
- The floor probe: `[floor] wrong room / falling / under / dead`, always on, for the user's fall in 182 (cause not found).
- The change freeze measured: the picture stop is the solo game's own (~66 ms); in a split the door's countdown holds the
  room left behind (zombies and the stayer) for ~1 s: Phase 1b (design-change-freeze.md §5) is the fix to build next.
- The log kept through the launcher's co-op restart (report_before_restart). ED_PUT finds the put player's own floor.
- `ed test` loads old slot saves (2026-11-09/10): NOT to be run (the user, 2026-11-13) until remade from fresh states.

**The user's next play (play-rooms-log.bat, from the launcher with co-op already on):** Pius's chapter: the statue
room 182 -> 181 as before (the fall: read `[floor]` lines first); doors apart both ways (the arrival camera); player 2 in a
room apart, lit and shadowed by their own room; a door while both are in one room with enemies about (the ~1 s hold);
player 2's torch apart (stage 1's dynamic lights); a spell cast apart (P7 not done: both halves show every spell light).
FINISH HIM apart, to test later (the user). Saving apart: last.

**NEXT (after the user's play):** Phase 1b of the change freeze (the split hold), then R4d (player 1's door into player
2's scene: design-r4d.md, refuted and revised, not built), P7, the mansion's chapter pages for both players, the co-op
teleport ("Bring X to Y" / "Y to X"), the `ed test` scenarios remade from fresh states.

# HANDOFF 2026-11-12: separate rooms R1-R6 (+R4b, R4c) committed; played by the user in Pius's and Karim's chapters

**Where things stand.** Committed on `tools/game-kit` and pushed to the fork (2026-11-12, the user: "very happy so far"): R6
(hits by room), R6b (routes in player 2's room), R6c (player 2's room's creatures as that room's own game), R4b (player 2's
door keeps player 1's running scene) and R4c (a player arriving during the other's scene keeps their own camera and plays
on). `play.bat` now has the separate-rooms switches on (ED_ROOM_SLOTS=2, ED_COOP_STAY=1); `play-rooms-log.bat` plays the
same with everything recorded (logs/run-*-rooms-play/). The user's play after R4c: "a few issues in Pius' chapter" (not yet
described or read: their recordings are the newest logs/run-*-rooms-play), Karim's chapter got much farther than expected.
Records: `docs/coop-level3-design.md` "R6", "R6b and R6c", "R4b", "R4c"; `docs/research/separate-rooms/` (enemy-readers*,
waypoint-readers, behaviour-readers, arrival-readers, design-r6*, refute-r6, build-run-r6.sh); `docs/coop-todo.md`;
`docs/coop-failed-attempts.md` (a JTG state made on a busy machine held player 2: check `hold 0x0`).
`runtime/host/controls.cpp` holds the user's uncommitted test edits: never commit it.

**NEXT:** the Pius issues the user saw (ask them, read their recordings), then R6's open list (FINISH HIM apart, a downed
zombie's rise, the spawner, other enemy kinds apart) and the reverse of R4b/R4c (player 1's door into player 2's scene).

**The user's report and direction, 2026-11-12 (supersedes the "After R6" list below where they differ):**
- Karim's glow apart: fixed. FINISH HIM apart: probably works, the user tests it later (still to test).
- Still to fix: shadows and lights per room; the camera jitter when a player enters a room apart; a brief freeze of the
  OTHER player's picture when one changes room (player 1 fighting in one room, player 2 enters another: player 1's screen
  pauses a moment; it is not a black screen, so "VISetBlack per half" below is this freeze).
- The Tome opens for both players independently, as intended: nothing to do.
- Saving apart: untested (only player 1 has save states and in-game saves); test after everything else.
- NO chapters independently: the game is linear and stays so. Instead, the mansion between chapters (Alex regaining
  control, a Chapter Page used) must give BOTH players the scene and carry them into the chapter together, as Jump to Game
  does. Normal play's Chapter Page path is not traced yet; the mansion comes once Jump to Game play is stable.
- A teleport selection in the mod menu under Co-op: "Bring X to Y" and "Bring Y to X", for a player who gets stuck.
- The player 2 fall through the floor in room 182 (the statue room) as player 1 entered 181: not reproduced by script; the
  user records it with play-rooms-log.bat. A floor probe now logs `[floor] wrong room / falling / under / dead` in every play
  (docs/coop-level3-design.md "The floor probe"): read those lines first in the user's recording.
- The randomizer last, once co-op is truly playable and stable.
- `ed test` (2026-11-12): 5 of 20 pass, the SAME 5 with the same failure reasons on a baseline exe of 5eb040d's host
  (build-win/var-base.exe, built by tools/ed/variant.sh with today's host changes reversed): the 15 failures are the
  scenarios' old save slots and frames (slots 1-12 became the chapters' starts, 2026-11-04), not today's code. Repairing
  the scenarios (fresh Jump-to-Game states for each) is its own task.
- DONE (2026-11-12, uncommitted): the camera at an arrival apart (docs/coop-level3-design.md, the section of that name).
- FIXED (2026-11-12, uncommitted): the log was lost after the launcher's co-op switch restarted the game. report.c tees
  descriptors 1 and 2 through pipes for the crash report's tail; `_execv` handed the new process those pipes, whose
  threads died with the old one (logs/play.log ended at "[rooms] separate rooms on for this run"). restart_process now
  calls report_before_restart (untee, then waits up to 0.5 s for the tee threads to drain) and logs "[frontend] starting
  again". Proven on a stand-in with the same tee (10 of 10 complete logs, 0 without); not yet seen with the game itself.
- Method: readers read every point for BOTH players; refuters check each design idea against the current implementation
  and the overall goal; engine-level changes only (never a save, room or chapter); in-game tests with the recording tools
  (run.sh, `ed timeline`/`see`/`sheet`) so the game isn't opened and closed many times.

**Together nothing changes** (every new path opens with the players apart by their own rooms; checked in every build).
Apart: enemies of each room go after, chase (routes included), hit and are hit by that room's player only; player 2's own
events and grabs run in their room. Not verified by script: FINISH HIM apart, a downed zombie getting up, the spawner, other
enemy kinds (chapters 2-11).

**How the user plays it (PowerShell):**
```
$env:ED_ROOM_SLOTS = '2'; $env:ED_COOP_STAY = '1'; $env:ED_COOP_ENEMY_LOG = '1'
& 'C:\Program Files\Git\bin\sh.exe' tools/ed/run.sh launcher NAME
```
What to try: Pius's chapter, room 183 (the zombies' hall): player 1 leaves by a door, player 2 stays near the zombies: they
should attack player 2 (also from across the hall: they climb to them) and player 2's swings should hurt and kill them;
then the other way round; and together, as before.

**How the user wants this worked (their method, in their words: "Let's not guess", "I'm going to let you drive"):** for
each step, readers on the game's own code (the `ed` tool: `python -I tools/ed/ed.py keep`, `dis`, `fn`, `needs`, `host`,
`system`) and on the host, a written design, refuters against it (skip the panel when the user's budget is tight: refute it
yourself from the readers' facts), ONE build, a fresh Jump-to-Game state on that build, scripted checks read from the logs
and shots, then the user plays and reports. Commit only when told; never push. Add failed attempts with their code to
`docs/coop-failed-attempts.md`; items seen to `docs/items-log.md`. The user's rules so far: player 2 has no music but all
their in-game sounds (done); "Bring (character) to (character)" in the co-op menu later; the randomizer is the last step.

**R6 (done above) was briefed as:** The user saw (2026-11-11): with the players in different rooms, enemies see player 2
in the other room and try to attack them (no damage lands); player 2's attacks on the other room's enemies aim but land
nothing. Same-room hits while apart DO land (R2's runs: room 183's zombies hurt player 2 while player 1 was in 180). The
host today: `coop_enemy_target` (hle.c ~24061, from `coop_character_step` on 0x80133434) retargets an enemy to the nearer
player only when both are in its room, else leaves the game's choice (the game only ever targets player 1); `coop_sighting`
(~21469, on 0x8003B1C8) gives player 2 the sanity-cone sight (E1); the hurt twin for player 2 is in the 'hurt' notes of this
file (the ch12 shield). Five readers were asked (sight/target, hits both ways, steps and collision of the other room's
nodes, damage and death apart, the host and the kit); their reports: `docs/research/separate-rooms/enemy-readers.md` if it
exists, else the workflow journal `C:/Users/stevi/.claude/projects/E--Eternal-Darkness-Recomp-Fork/73f71524-55f9-4337-bea8-
8369f9fa7835/subagents/workflows/wf_8d47c1cd-706/journal.jsonl` (one {"type":"result"} line per reader, field
result.report). The design to write from them: an enemy sees, targets, chases and hurts only a player whose object +592
room is its own; a player's swing/shot/spell tests victims in their own room (run the hit test with their slot current,
`coop_stay_room_in`'s pattern, or skip victims of another room); the other room's nodes step against their own room's
collision (0x80123F70 by +592 answers it for non-players already: confirm). Checks planned in `build-run-r6.sh` (c1 player 1
away, c2 player 2 away, c3 player 2 swings in their room), read from `[coop] N's health a -> b` / `goes after player` lines
(ED_COOP_ENEMY_LOG=1) and both.tsv's p1hp/p2hp.

**After R6, in this order:** the shadow pass and the lights per room (a shadow of the room in play falls in the other half;
one light grid for both halves); the camera's small jump at an arrival (the kit's CAMERA blip, nine frames); the orbs'
eye-space place in the other half when both are in one room (coop-glitch-todo U4); VISetBlack per half (the load's 3-4
frames black both); R7 the Tome, saves and cutscenes while apart; chapters independently; "Bring X to Y"; the randomizer.

# Co-op: where things stand

Written 2026-11-02. Read this first, then update it at the end of the session.

## Start here (updated 2026-11-03)

**2026-11-10, committed as d50f804 (the user: "commit what we have so far"): everything of this day below. The user's direction: a truly co-op game, each player using and examining different things at the same time; the plan is the note "Both players acting at once" in `docs/coop-todo.md`, in order: scenes, then words, then close-ups. Step 1 (two scenes at once) is in the code and played; player 2's pickup beside player 1's scene goes through, with the camera and prompt gates read from the game's code and handled (`docs/coop-todo.md`). Level 3 (close-ups, words and finds per player): round 2's design is in `docs/coop-level3-design.md`'s session record and the scratchpad (the memo's round 1 is refuted; round 2's design with its four fixes is what step A is built from). **Step A, player 2's own words window, is built behind `ED_COOP_OWN_WORDS=1` and script-verified both ways (`docs/coop-todo.md`): unplayed. Step B, player 2's own close-up (ed219: EXAMINE, DETAIL, words, RETURN, each player's in their own half with the other free), is built behind `ED_COOP_OWN_CLOSEUP=1` (with `ED_COOP_OWN_WORDS=1`; 2026-11-10 night, uncommitted) and script-verified in room 183 from `saves/slot2.state`: both holes at once in both orders, camera modes 2 and 0, player 1 alone, and a room change under it (`docs/coop-todo.md`, the build record at the end of `docs/coop-level3-design.md`). Step C, player 2's own find (PICK UP: the thing floating in their half, placed from their camera, the game's words carrying nothing of it), is built behind `ED_COOP_OWN_FIND=1` (with the other two; 2026-11-11 night) and script-verified in room 183 the same way, the switch unset and steps A+B alone identical to today's lend route (the record at the end of `docs/coop-level3-design.md`, the paragraph at the end of this file). Unplayed: the user plays B and C next, `sh tools/ed/run.sh saves/slot2.state NAME` (the switches are gone since step F). Then step D (a Tome during an own close-up or find), the same way: design from the game's gates first, build once, check by script, one play.**

**2026-11-11 late afternoon, separate rooms begun (uncommitted): the readers, option A (two resident room slots, the game's own dormant two-slot room engine), and R1 the probe built and run clean (`ED_ROOM_SLOTS=2`, logs/run-20261111-150751-jtg-rooms-r1).** `docs/coop-todo.md` "Both players acting at once" has the paragraph (what the readers settled, the option, R1's result, R2-R7); `docs/coop-level3-design.md` "Separate rooms" the whole record. The user's direction: think first, Pius's chapter for the first tests, sanity effects likely fine, the Tome-found world change to keep in mind, "Bring X to Y" in the co-op menu after the tests. R2 (the other player stays, `ED_COOP_STAY=1`) is built and functional the same day (the todo's paragraph; `docs/coop-level3-design.md` "R2"): the stayer examines, picks up and walks in their room while player 1 is in another. The user's sound rule: player 2 has no music, their in-game sounds stay. NOT to be committed until functional as a whole (the user). R3 the picture is built and functional (each half its player's room: the marked camera matrix per room section; `docs/coop-level3-design.md` "R3"). The stayer flow R1-R3 is playable from Jump to Game with `ED_ROOM_SLOTS=2 ED_COOP_STAY=1 sh tools/ed/run.sh launcher NAME` (or from `saves/rooms-ch1-183.state`, made with the switches). R4 doors from either side is built and functional (player 2's door while player 1 stays: the ticket's runner bracketed as player 2, the stayer whoever didn't move, the fade quad the mover's half alone; `docs/coop-level3-design.md` "R4"). R5 sound is built and functional in its scripted checks (player 1 holds the music: player 2's music scripts pass together and are deferred apart, replayed when player 1 arrives; a door's fades leave the other player's room alone; a track in player 2's room is heard as theirs; player 2's copy Tome sounds live; `docs/coop-level3-design.md` "R5"). Not yet heard by the user, and the deferral is untested in a run (chapter 1 sets no music at a door): the user plays chapter 2's room 294 or the save room apart. Next: R6 enemies of the other room, R7 the Tome, saves and cutscenes while apart; then the cutscene examine issue the user saw, "Bring X to Y" in the co-op menu, lights per room, chapters independently, the randomizer last. Still uncommitted by the user's wish until functional as a whole: the user plays it first.

**2026-11-11 afternoon, step F built, refuted, script-checked, committed and played by the user from Jump to Game (A to F; one issue, the cutscene examine, in the todo): the switches are gone, the look's windows are one route, the use route stays.** The readers found the lend route is the use route (a script of player 2's run as a use, a Tome item's, cast words, a spawned item's PICK UP), not dead code: `docs/coop-todo.md` "Both players acting at once" has the paragraph, `docs/coop-level3-design.md` "Step F" the readers, the refuted points, the build and every run. both.tsv gained `own2`. Next: the user plays A to F (`sh tools/ed/run.sh saves/jtg-ch1-room183.state NAME`, no switch); then separate rooms; the randomizer last. A later cleanup can still take the use route onto the own sets (a use's words into `g_coop_p2_words`), which would retire `g_coop_screen` and the lend: not step F's.

**2026-11-11 midday, step E built, refuted and script-checked (uncommitted; HEAD fcbf71d is step D): a room change, a cutscene and a save/load under player 2's own words, close-up and find.** `docs/coop-todo.md` "Both players acting at once" has the paragraph and the play; `docs/coop-level3-design.md` "Step E" the readers, the refuted points, the build and every run; the open notes in the todo. The kit gained ED_LOAD_STATE (the `ed` skill). Next: the user plays B to E; step F (built 2026-11-11 afternoon: the entry above); then separate rooms; the randomizer last.

**2026-11-11 later, fixed (committed as fcbf71d with step D, not yet played): the user's play of Pius's chapter from Jump to Game froze both players when player 2 examined the dais as the chapter began** (the intro's end was run as player 2's; `docs/coop-todo.md` open issues, the first entry, with what to play). The ed kit's lesson: `ED_SHOW_TRACE=1`'s 'script run' lines with their lr tell which host rule took a script; `JTG_BASE=1875 JTG_P2_SCRIPT="0,10,b"` is a press at the first frame of control in chapter 1.

**2026-11-11, step D built, reviewed and script-checked (committed as fcbf71d, the user: "I trust you that it's fixed. Commit."): player 1's Tome beside player 2's own words, close-up and find, the user's option A.** `docs/coop-todo.md` "Both players acting at once" has the paragraph and the play command; `docs/coop-level3-design.md` "Step D" the readers, the refuted design points, the build and every run. One check fails and is older than step D (the Tome item beside player 1's close-up: the game's one busy word; an open issue in the todo). The fresh state is `saves/jtg-ch1-room183.state` (its command in the `ed` skill); throw it away when the code changes. Next: the user plays B, C and D; then step E (a state load, a room change and a cutscene under their own windows: the cancels exist, the checks don't), step F (cleanup: one switch, the lend route's dead branches), then separate rooms, the randomizer last.

**Testing rule (the user, 2026-11-11): test from Jump to Game, then teleport.** Save states can be corrupted after code changes, so a scripted check is no longer keyed to a `.state` (steps A to C were checked from `saves/slot2.state`, made before them). From now on: `sh tools/ed/jtg.sh CHAPTER NAME` boots cold through the title and Jump to Game, the room is reached by the game's own door scripts (`ED_RUN_SCRIPT=DELAY:SCRIPT`, a locked door's flag by `ED_SET_FLAG=DELAY:FLAG`) and the players are put at their spots by `ED_PUT`/`JTG_PUT`. Step D's first job before its check: the door chain from chapter 1's room 341 to room 183, written down here. The user may also play B and C from Jump to Game rather than slot 2. The route: chapter 1 (Pius) starts in room 341; door scripts L01/103 (341 to 342, an EXAMINE then DETAIL door), 101 (342 to 343), 105 (343 to 344), 111 (344 to 180, CLIMB DOWN), 29 (180 to 183, OPEN DOOR); a prompted door's script runs twice 60 frames apart.

**Step D's decision (the user, 2026-11-11): option A.** Player 1 opening, using and closing their Tome must not touch player 2's own words, close-up or find at all; they keep stepping in their half and their presses keep working ("no co-op game would disrupt a function of another player because they checked their inventory"). The Tome already alternates its frames with the world's (`g_tw.sharing`), so player 2 keeps walking during player 1's Tome today; step D makes their own windows keep stepping the same way. The randomizer is its own, separate, LAST step, after the whole game completes in co-op; separate rooms per player stays as a step of its own, before it.

**2026-11-10 afternoon: room 183 played and confirmed by the user** (the scene camera, player 1's block with player 2 walking; a fifth cause, the hide rule, in `docs/coop-todo.md`). **Then room 182's statue words** crossed into by player 2 went to player 1's half with nobody held: a window opened inside player 2's own look lived in their input copy. Now a close-up of theirs (`coop_prompt_look`, `coop_window_to_game`), their body held by the game's own channel hold in their copy; script-checked (statue-look7), the user's second play pending: `docs/coop-todo.md`. Also there: a block from player 1's Tome in the copy ran as player 2's (fixed, unplayed), and player 1's copy item waits for player 2's scene to end (by design, to decide). Then (the user's third play): the cameras stayed lent after player 2's block scene and player 1's examine went to their half; now one rule at every pad read gives them back when nothing of player 2's is up (unplayed), and the lend carries each player's live view both ways (both halves jumped as player 2's block scene ended, blocks-user3 8676). The user asks for different examines and uses at the same time by both players: the game has one words window, one close-up stepper and one scene flag; see the note at the end of `docs/coop-todo.md`'s open issues. New test hook `ED_SET_FLAG=DELAY:FLAG` (the `ed` skill) for doors a save hasn't unlocked.

**2026-11-10: every glitch the user reported from 2026-11-07 on is checked against HEAD in `docs/coop-glitch-todo.md`:** open, partial, fixed-unplayed and confirmed, with the first step for each and the four whole-game rules behind most of the open ones. The kit's saves and `ed test` scenarios are outdated: only `saves/slot1.state` (2026-11-09) is current.

**2026-11-09: room 183 (Pius's chapter, the block holes) fixed and played by the user.** Player 2 starting the scene no
longer holds them for good, and a form keeps its shape after a save state is loaded (save states now carry the forms' own
bodies, section COFM: saves made before this don't, so remake them from Jump to Game). Details in `docs/coop-todo.md`.
**2026-11-10, uncommitted, not yet played by the user:** in that room, player 1's camera stayed on player 2 through player
2's scene (player 2's scene turns the game's zones off, and player 1's look with them off only clears: now on for player
1's look, as in player 2's close-up), player 1's stick went by a view kept from player 2's last close-up, player 2 was held
through player 1's block scene (a hold set with player 1 the player is player 1's, whatever else is up), and a block used
from player 2's Tome played in player 1's half (now a use of theirs). Under all of it one owner for the camera set
(`g_coop_set_in`, `coop_cameras_in`, `coop_script_cameras`): a script runs with the set of whoever is the player, whatever
route reached it. All in `docs/coop-todo.md`, with what to play. The user's save state 1 is at the room 180 door.

**Status on 2026-11-08: everything below up to f2b2565 is committed and released as v0.1.0 (pre-release).** The entries below are a dated log. Their "uncommitted" and "not yet played" were true when written, and many aren't any more. Played and confirmed by the user: the dream (camera, Shotgun in and out, no Tome), player 2's reload and the Dual Flintlocks, Anthony's and Ellia's gun moves (hair and reload), the fireplace puzzle, its flare and icons, and the menus. Also fine as far as the user knows (2026-11-08): E1 enemies seeing player 2, the monk's bell rung by player 2, player 1 vanishing after the fireplace. Not played yet: Summon Trapper's switch, W2 FINISH prompt. **Next (the user, game-breaking): I2/I4, Enchant Item mending Anthony's chapter's broken urns.** The user will make a save with the spells, where they can mix and enchant the urns, and trace solo first. The Cheats menu's all-spells option is bugged (not looked into; the user will get the spells another way). Still open:

**Anthony's casket by player 2 (2026-11-08, the user played it: fixed).** Opening the Tome room before the sword is the game's own (solo, logs/run-20261108-130144-solo-anthony-ref). The casket is L03/1587 (EXAMINE, Yes, its scene), which runs on to L03/1939 with fn73: cinematic 128, then the sword (ed3 at "the player", ed52), the monk's walk (fn17), Anthony to his mark, let go. Solo, 1939 is called from 0x80164488 (fn73), 0x80026FE4 (the cinematic's end), 0x80163E5C, 0x800244C0 (find closed), 0x80168388 (the walk done) (logs/run-20261108-130307-solo-anthony-sword). In co-op the cinematic's end ran 1939 as player 1's from the game's table, with no word for it: the cinematic played twice, player 1 found the sword and was walked to Anthony's mark, player 2 held for good (logs/run-20261108-130550-coop-casket-p2, the user's play; `Screenshots for Claude/casket-p2-frame5937.png`). Now, in `coop_run_script`: a script that ran as player 2's is theirs (`g_coop_p2_scripts`) until player 1's press or zone runs it, and the game's call-backs into it (the cinematic's end, a walk done, a find or window closed) run it as theirs. At the cinematic's end the co-op cutscene's end runs first (`coop_cutscene_over`), or player 1's half kept the casket's camera. A find in a scene of theirs (their script's own hold, busy 0x32) is their close-up (`g_coop_script_hold` off), or its words went to player 1's half and pad. Checked by script on the user's slot 2 (logs/run-20261108-131515-fix2-casket-p2): one cinematic, the words in player 2's half closed by their B, player 1's own camera and free, the scene to its end as player 2's, player 2 free after. The bell (`0x80168388` was the game's, e940905) now goes on as player 2's too: retest it. `ed test` runs the old slot saves the user ruled out: don't run it.
- Player 2 entering Anthony's Tome room first (2026-11-08, the user played it: fixed): they stood held for good after its first scene (L03/1351), and player 1's half stayed on the scene's IntroCam02, following player 2 (logs/run-20261108-133922-p2-tome-room-first, the user's play). The scene's timers ran it as player 2's, its word in their table, and its end (the camera path arriving, `arrival_scene` in `coop_run_script`) ran it from the game's table: state 0, not 4. Now that run has their table in when the word is theirs, and when their arrival scene ends player 1 takes the camera set it ended on (Camera01), its records naming player 2 re-pointed to player 1 (`coop_p1_scene_end_camera`). Checked by script on slot 2 (`ED_COOP_P2_PUT=2120,225,845,1003`, up and right 2150-2175 crosses SKPlane01; logs/run-20261108-135141-fix7-tome-room-p2): both on Camera01 from 2700, player 2 walks. Then the user: player 2 shown on both halves through the scene (its shots frame the arrival spot, player 2's). The user's choice: player 1 plays on with their own camera. As player 2's arrival scene begins, its script runs once as player 1's (`g_coop_p1_own_camera`, in `coop_script_timers`): 1351 with flag 111 set picks Camera02 for one outside CameraRgn01, following player 1. The scene-end copy then stays out. Checked: logs/run-20261108-135930-own-cam-tome-room (player 1 on Camera02 from 2440 and walking in player 2's scene). Another room's arrival scene whose state 0 doesn't set its own flag first would run its scene for player 1 too: not seen, watch for it.
- The room change's swirl (the picture ripples out of the stairs and in to the Tome room, solo logs/run-20261108-130144-solo-anthony-ref 2040-2110) doesn't show in co-op: both halves fade to dark and cut (the user, 2026-11-08). Not looked into. the table under "Open bugs" and `docs/coop-todo.md`.

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

**Enchant Item in co-op (2026-11-08, uncommitted; the user played the urn by both players, player 2's sword checked in their run's pictures):**
- The repair branch's test was read the wrong way: a template's bit 0 (0x801507F0) set takes runes (a weapon), clear is mended (0x801C6EF0). Read as set for broken, the urn went on as a weapon and stayed broken (logs/run-20261108-142023-coop-repair-p1). `coop_controlled_id` now lets the game's own answer stand for bit 0 clear: the game's close-up, which the user is fine with.
- Player 2's cast words (repaired, enchanted) are theirs whatever the item (`g_coop_cast_words` set for any item, in `coop_cast` and `tome_copy_left`); only for a copy, the urn's came in player 1's half and waited for player 1's B.
- The other player's notice names the thing: "Pius has repaired the Green Urn" (`coop_thing_name` from `item_names.inc`, made from build/ed-db/facts/item_table.json; the game's own words, 0x801DB864, asked from the host hung in the unpacking 0x801559D8: saves/reports/hang-20261108-143841.txt).
- Player 2's weapon enchant was refused ("This Item cannot be enchanted."): the cast (0x801C4250) needs the weapon in the controlled character's equipment (0x801C44FC; 0x801514B8, a count at +388, 12-byte entries {id, hands, word} from +4). The game's own choose-to-equip (0x80103F70..0x80104070: slot 0x801512F8, hands from the template's table, EQUIP_TAKE) puts it there, replayed for player 2 by `tome_copy_equips`, but player 2's record (0x80588F88) existed with no hands (+392 = 0, made with no set-up, likely by 0x801F901C), so no slot. `coop_equip_record` now sets up such a record with the game's own values (0x80151634(rec, 6, 0x800C994C, 0x800C9DD4), as 0x80007BAC, 0x8004280C and 0x800B8EE4 do). A cast-time equip of my own (`coop_cast_item_theirs`) was invented and removed (docs/coop-failed-attempts.md).
- Swing moves with another chapter's weapon (`wpn_part`, `wpn_own_sword_part`, `weapon_lists.inc` from tools/ed/facts/weapon_lists.py): a weapon that swings from the swords' InvS attack list (templates 0, 10, 13, 14, 45-49: the same move per direction) in the hands of one whose own file has a sword-list part at its place plays their own (Pius with the Scramasax: his part 14), as before W1; the Torch's list is its own, so Anthony's torch moves still. Checked: logs/run-20261108-152216-enchant-sound-trace ("part 42 ... part 14 played").
- Player 2's copies had no holder (0x8014ED1C(node) = 0): the torch's flame (0x8014C104) and light (0x8014DD1C) and an enchant's aura (0x80141824) were made on nothing. `coop_copy_holder` gives a copy the holder the game's held making gives (0x8004269C..0x80042770). Player 2's swing sounds (0x8019E240, the alignment's 533/534 after the swing) need the swinger to be the player and the attack block's weapon (+104): `coop_move_sound` runs it as player 2's with their saved attack block's weapon. Checked in the user's play: logs/run-20261108-155345-torch-hum-test (torch lit in pictures 18140-18310, 534 four times). Not yet: one lit torch for the game (flag 1127, r13-24988/-24992, the tracker 0x8030AA98).
- Each player's own torch (2026-11-08, played by the user: both lit at once, through room changes, beside an enchanted glow; logs/run-20261108-161130-two-torches). The game keeps one: the record 0x8030AA98 (120 bytes; a held torch's, 0x80079318, and an enchanted weapon's glow, 0x800DA554; its light made by the queue's op 20, 0x80026EE4), the crackle's slot r13-25468 (sound 413, kept at the torch by the light's frame callback 0x80078E2C), the flame and light handles r13-24992/-24988 and flag 1127. Player 2's are kept in `g_coop_torch` and swapped in (`coop_torch_swap`) around the hold and let go of a thing in their equipment (`coop_torch_held`, `coop_torch_let_go`), their spell steps (`coop_spells_update`) and their light's frame callback (`coop_torch_frame`); a queued op 20 for the record is done there and then (`coop_torch_light_now`, as the sanity door cancels one: 0x801DCD8C, 0x801DCC60). Not done: the room relight of lying torches (0x80044748, record 0x80316240), the Tome's deferred light (flags 666/667), a torch hit setting things alight (0x800E1FFC, 0x801F9130 per attacker).
- Player 1 (Anthony) drawn flat after player 2 took the Tome (2026-11-08, logs/run-20261108-163231-cheat-story-tome, frame 3397): after Anthony's Tome film (L03/1599, cinematic 112, the model-85 branch) the game's remake of the player's moves (0x80131CF8) left the id-to-set table at 0x80582010 empty, its ask (0x801322C0 -> 0x80132970) having given nothing: every move passed over. Not seen again in four scripted runs (logs/run-20261108-1655*-repro*) or the user's next play (logs/run-20261108-165807-p2-tome-moves). `wpn_check` now asks again with the game's own 0x80118D98(player, part, 1) when player 1's table is empty, and logs the moves memory's owner (r13-23696, kind r13-23700): when "[player] player 1's moves were empty" shows in a log, that names what held it. Not yet seen to run.

**Step C is built (2026-11-11 night; the one route since step F): player 2's own find.** Their PICK UP of a thing (ed221, the kind-3 close-up: EXAMINE, DETAIL, the words, PICK UP, the thing floating in front of their camera, B) runs on their own tables as step B's close-up does, with the find's record (0x8030A6E8) and bindings (0x8023E4B8) in their set, its window's channel in their input copy, stepped by their tick (`coop_words_tick`), and its hold answered in their copy with the game's four hold words put back (`coop_find_hold`'s own branch: busy bit 16 and r13-24048's bit 2 are never written, the find byte lives in their set), so player 1's timers, prompts and turn run on through it. The thing goes on the one carry list (player 1's, as today), is drawn in player 2's half alone (per-owner marks, `coop_found_object_draw`) and placed from their camera: the game's own placement step runs with their view in the camera block (`coop_found_place`) and its unproject's point is moved from player 1's frame to theirs (`coop_found_unproject`). The look's script and the thing's after-script are left for the controller (a queue, `close_scripts`), each a use of theirs with r5 restored and, for the look script, main kept player 1's. A room change with their find up closes it by hand (`coop_p2_find_cancel`: the B's free, the channel, their room record, events 47 and 190 to the floating thing). **Checked by script in room 183 from saves/slot2.state** (the picker at 360,340 in BlockRegion01, the other at a hole; the chain in the scratch's stepc/runs.sh): logs/run-20261110-235404-stepC-run0a-unset and -235427-run0b-AB (today's lend route line for line), -235449-run1-p1find (player 1's find as today, player 2 walking), -235512-run2-p2find and -235533-run2m0-p2find (their whole PICK UP in their copy, the game's words never carrying it, the block floating in their half alone, 1181 as theirs after; sheet-15020-15600.jpg), -235556-run3-p2find-p1hole and -235618-run4-p1find-p2hole (each pairing in its own half), -235640-run5a-roomchange (cancelled cleanly, no hang). The cancel inside the two frames before the thing floats can't be reached through a door (the press is refused during the room change) and stays untested. **Reviewed after the build (21 agents, five lenses, two refuters per finding): eight findings, five confirmed, all fixed and rechecked** (logs/run-20261111-0039*-stepC2-*): two unset-path slips in the per-owner marks (one pair, one flag, as today's), the placement passing through on today's lend route (a use's or a scene's find of theirs, checked with a spawned Sheet of Music, -004132-stepC2-run6b-lendfind-close), the unproject against this frame's view (the matrices are rebuilt between the camera step and the placement: the design had it wrong, docs/coop-failed-attempts.md), and the four-frame clear for a lend-route find whose hold goes native. **Unplayed.** To play (slot 2 in room 183): `sh tools/ed/run.sh saves/slot2.state NAME`; player 2 walks onto the dais with the Granite Block and presses EXAMINE, DETAIL, B, PICK UP, B while player 1 walks about or examines a hole; then the reverse. Watch: the block's place and size in player 2's half (centred, a little high) and whether it vanishes when player 1's camera doesn't see it (culling, risk 6); the feel of the hold; a Tome during it (step D, not wired); two finds at once in a room with two things. Step D (a Tome during an own close-up or find) next, the same way.

