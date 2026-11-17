# Solo / Co-op on the title, and Player 1 / Player 2 on Controls: the build plan

Date: 2026-11-17. Read-only research. Nothing was edited, built or run for it. Addresses are PAL (GEDP01).
Line numbers drift (another workflow is editing frontend.cpp, hle.c and main.c now). Find code by function name.

## The ask, in short

- Start Game and Load Game on the title first show a page with two large pictures: **Solo** and **Co-op**.
- That page replaces the co-op on/off switch in Mods. The switch goes.
- The rest of Mods > Co-op moves. Controls gets the same kind of page: **Player 1** and **Player 2**, then that player's
  Controls page, the two pages laid out the same.
- Tidy other menus the same way where it helps.
- Keep the game looking native.

## Checked in the code (and two corrections to the research)

Held up:
- `title_more_choose` is a **notify** on the title's A, 0x80019F30 (hle.c, the `kMore` table). It falls through to
  `title_main_choose`, which acts only on row 5 (Choose Character).
- `hle_dispatch` runs an entry's intercept first. When the intercept leaves `g_hle_passthrough` false, the call ends
  there, and the notify and the game's function do not run (hle.c ~5739-5776).
- `rooms_follow_coop()` sets or clears ED_ROOM_SLOTS / ED_COOP_STAY from `mod_coop` and returns true on a change.
  `restart_process()` unsets ED_QUICKLOAD and ED_SKIP_INTRO, so a restart lands on the launcher (frontend.cpp, main.c
  ~683). Only `start_game` calls both today.
- `title_state_path()` is `saves/title-rooms.state` with ED_ROOM_SLOTS, else `saves/title.state`. Both files exist on
  this machine (Nov 11 and Oct 30). quickstate loads an old 32 or 40 MB state into a bigger build (zeroes past its end),
  but refuses a smaller one under ED_ROOM_SLOTS and any other size mismatch.
- `player_card_rml` dims player 2's card with "Co-op is off" when `mod_coop` is 0.
- The Left/Right card move exists only for `Screen::ModsPage`. `past_root` has no entry for a new screen.
- Player 2's Controls page returns early before Handling ("player 1's for now").
- settings.c caps the frame rate at 60 while `mod_coop` is 1, applied by `settings_apply()`.

Corrected:
1. **The notify alone cannot hold Start or Load.** `frontend_open_page_skin` only stores the page id
   (frontend.cpp ~5118). The page opens later, in `frontend_frame`. So after the notify returns, the game's own A runs at
   once: state 8 or 13, the book load, the card manager. Choose Character gets away with it only because row 5's A does
   nothing. The research's "cursor to row 5" trick is not needed either. **Use an intercept** (step 3).
2. **Title pages are about to run "live".** docs/research/firstrun-hd-flash/title-music.md, steps 2-3, is being built
   now: the title's frames keep running under a title-look page, and `g_title_page_hold` zeroes the guest's pad from the
   open request until the closing press is let go. The picker must set that hold too, and its replayed A must come
   **after** the hold ends, or the hold eats it.

## How it will look

Over the title, in the title look (`FRONTEND_SKIN_TITLE`, the cleaned title picture behind, the Tome's hand for words).

```
                         Start Game                 <- heading: the row that was chosen ("Load Game" for Load)

        ---- Solo ----                    ---- Co-op ----
      +--------------+                 +--------------+--+
      |              |                 |   player 2   |  |   <- blue mount, behind
      |   player 1   |               +-+------------+ |  |
      |   portrait   |               |   player 1   | |--+
      |   (gold)     |               |   (gold)     |-+
      +--------------+               +--------------+
         Alex                          Alex and Pius
                    A choose  ·  Left/Right  ·  B back
```

- Two cards side by side, centred, about 30% of the window wide each, with a gap. They do not fill the screen: about a
  third of the height is left above and below.
- The same leather mounts, rules and mark (`tome/mark.png` between them) as Choose Character's cards. No meters, no
  stats.
- Labels "Solo" and "Co-op" in the card head, where "Player I" / "Player II" sit on the character cards.
- Under each picture: the chosen character's name (Co-op: both names).
- The lit card glows (`.selected`), as on Choose Character. The card first lit is the mode saved last
  (`mod_coop`), so A A goes on as before.
- Left/Right moves between the cards. A picks. B goes back to the title, its row still lit.
- The prompt line: "Left/Right choose · A start · B back".

The Controls page is the same page with other words:
- Heading "Controls". Cards "Player I" (gold) and "Player II" (blue), each with that player's portrait and name.
- Player II is shown in solo too, not dimmed: player 2 can set up their pad ahead of time.
- The prompt line: "Left/Right choose the player · A their controls · B back".

### The art (chosen, with a fallback)

- **Pictures:** the players' own Choose Character portraits, from the disc's `UMnMenu.tpl` images 50-61 (126x89),
  already written to `res/ui/portraits/` by `ensure_portraits` and drawn by `portrait_rml`. They are the only in-game
  art that reads as "one person" and "two people", and the user has seen them on Choose Character.
- **Co-op:** player 1's portrait in front (gold), player 2's behind and up to the right (blue). Preview:
  `Screenshots for Claude/solo-coop-picker/01-portraits-one-and-two.jpg`. Controls: `06-portrait-cards-player1-player2.jpg`.
- **Default character (0):** Choose Character shows `emblem.png` for it. Here a face reads better. Use Alex
  (image 50) for player 1 and Pius (image 51) for player 2 when their choice is Default. If the portraits cannot be
  made (no disc picture), use `emblem.png` for each card, as Choose Character does.
- **A form:** its own portrait (`portraits/NAME-FORM.png`), as `portrait_rml` already does.
- **HD (later, step 7):** the pack has 504x356 copies of all 12 portraits (`textures/GED/splash, menu, and
  text/tex1_126x89_<hash>_14.dds`). `ensure_portraits` can read them with `texpack_read_rgba` and flip the rows.
- Not used: the title's face-to-face picture (1x only; the skull is not a player), the Tome door cards (no "one or two"
  meaning), the Tome's controller drawing (arrows cut, always a GameCube pad).

## The rooms problem (why a pick can restart the game)

Separate rooms are set when the program starts: 56 MB of guest memory, the two-room hooks and the title machine file
all follow ED_ROOM_SLOTS at boot. A solo game with them on froze once (saves/reports/hang-20261112-025508). The user's
rule: co-op fixes never apply to solo.

So:
- If the pick matches how this run started, nothing restarts. The game goes on at once.
- If it does not, the program restarts once into the other mode and comes back to the same title row, which is then
  pressed for the player. A few seconds. The window closes and opens again.
- The program always starts in the mode picked last, so a player who keeps one mode never sees a restart.

## The steps

Each step is small and shown in pictures. Every run: `ED_RUN_AWAY=1`, `ED_MUTE=1`, shots every 250 ms at 1280 wide,
pictures into `Screenshots for Claude/solo-coop-picker/<step>/`. Title runs use the recipe of
logs/run-20261117-090616-jtg-s4fix-fresh-map (its env.txt): `jtg.sh` with `JTG_TITLE_AT=90000` (its own presses never
come), absolute `ED_INPUT_SCRIPT` presses on the title (Down 0x0004, Up 0x0008, A 0x0100, B 0x0200), and
`ED_PAGE_TEST_KEYS` / `ED_PAGE_TEST_MS` / `ED_PAGE_TEST_STEP_MS` for presses inside a page. Space presses after a
page by its open time: with the live title page, guest frames go on under it. After each step: `ed test`.

### Step 0. Wait for the live title page, then re-read

- The title-music build (live title pages, `g_title_page_hold`) must land first. Then re-read `title_main_choose`,
  `title_more_choose`, `hle_PADRead`'s write-out and `frontend_frame`, and fix the line numbers here.
- Check docs/coop-failed-attempts.md for anything new on the title.

### Step 1. The two-card page, alone (frontend.cpp, menu.rcss)

- frontend.h: `FRONTEND_PAGE_TITLE_PICK = 12` (Solo / Co-op). Widen the `g_on_title` range in `frontend_frame` to
  include it (it tests TITLE_SETTINGS..TITLE_QUIT today).
- frontend.cpp: a new `Screen::Pick` with `enum class PickKind { Mode, Controls }` and `g_pick_kind`.
  - `build_pick()`: two rows with `card = 1` and `card = 2`, a heading, and nothing else.
  - A new `pick_card_rml(kind, which, selected, index)` beside `player_card_rml`: the same `.pcard` mount and head,
    the label ("Solo" / "Co-op" or "Player I" / "Player II"), one or two portraits, the name line. No meters or foot.
  - Draw `.pcards` for `Screen::Pick` where the card rows are drawn now, and set the `cards` class for it as for
    Choose Character.
  - Left/Right: extend the card move (today `g_screen == Screen::ModsPage`) to `Screen::Pick`.
  - B: add `Screen::Pick` to the Back switch and to `past_root` (a Pick root closes the page).
- menu.rcss: `.pcard.pick` (larger frame, no meters), `.pcard-pair` for the two overlapped portraits, under
  `body.skin-title` and the launcher look. Gold front, blue back, as `.pcard.p1` / `.pcard.p2`.
- A test door: `ED_MENU_OPEN=pick-mode` and `pick-controls` (beside `coop`, `controls2`).
- No hook yet. The page does nothing on A but log `[frontend] pick: solo|co-op|player 1|player 2`.
- **Shows:** the page in the launcher look (ED_MENU_OPEN) and in the title look (step 3 opens it there). Both cards,
  each lit in turn (`ED_PAGE_TEST_KEYS=r,l,b`), Default and a chosen character, a form, 16:9 and 4:3.

### Step 2. Controls through Player 1 / Player 2

- `build_options`' Controls row, the title's page 9 (`FRONTEND_PAGE_TITLE_CONTROLS`) and so the Tome's Settings >
  Controls, all open `Screen::Pick` with `PickKind::Controls` instead of player 1's Controls.
- A card: `controls::set_menu_player(p)`, `g_controls_return = Screen::Pick`, the card's row kept, then
  `Screen::Controls`.
- Back from Controls (button and switch) already resets to player 0 and goes to `g_controls_return`: back to the pick
  page, the same card lit. Back again leaves as Controls left before (Options, or closes the title page).
- `past_root`: `Screen::Pick` as a root covers Controls and Mapping under it; `Screen::Controls` and `Mapping` roots
  are no longer opened straight from the title.
- Player 2's Controls page as today: Plays with, Controller, Controller type, Layout, Button mapping, Back. Handling
  stays player 1's (a user question below).
- Keep `ED_MENU_OPEN=controls2` / `mapping2` working (they open player 2's page straight).
- **Shows:** title Options > Controls: the pick page; Player II, their Controls page, Button mapping, Back, Back (the
  pick page, Player II still lit), Back (the title list on Controls). The same from the Tome's Settings in a fresh
  Jump-to-Game state (`jtg.sh 1`, Start, the Settings plaque). Player 1's page unchanged.

### Step 3. Start Game and Load Game ask first (same mode: no restart)

hle.c, beside `title_main_choose`:
- `title_main_start_load`, an **intercept** on `TITLE_A` 0x80019F30. Install it after the `kMore` notifies, with the
  entry's own check (`intercept == NULL`, first word 0x9421FED0).
- It passes through (`g_hle_passthrough = true`) unless all of these hold: state 7 (TM+62), cursor (TM+28) 0 or 1, no
  card notice up, no replay armed for this row, and the page is not open already. English is **not** needed: Start
  and Load are the game's own rows in every language.
- When they hold: play 633 (`0x8019DB90(633, 100)`) as the game's A would, keep the row, set
  `g_title_idle_reset` and the closed count, set the live page's `g_title_page_hold`, log
  `[title] Start Game: Solo or Co-op? (frame N)`, and `frontend_open_page_skin(FRONTEND_PAGE_TITLE_PICK, 0,
  FRONTEND_SKIN_TITLE, -1)`. Do not pass through.
- The page's heading is the row's word. Its card A sets the result (`hle_title_pick(row, coop)`) and closes. B closes
  with no result.

On a result (frontend.cpp, in the card's A):
- `g_settings.mod_coop = coop; changed(); settings_save();` (`changed()` re-applies the frame-rate cap).
- `rooms_follow_coop()`. If it returns false (the same mode as this run): arm the replay. Step 4 handles true.

The replay (hle.c `hle_PADRead`, after `skip_card_select` and **after** the live hold):
- When armed, the hold is over, A is not held on the real pad, TM+62 is 7, TM+28 is the row and the screen is not
  fading: press A for 3 reads (as `skip_card_select` does), then disarm after the intercept saw it.
- The intercept sees the armed row and lets that one call through. The game then does its own whisper stop, sound 634,
  book load, state 8 or 13 and card manager, exactly as without the page.
- If the cursor has moved or the state left 7 (the attract movie), drop the replay and log it.
- **Shows** (each run once with `JTG_SET=mod_coop=1`, once with 0, so no restart):
  - Start Game: the page, Co-op lit (or Solo); A; the carry-over question on the user's card ("You have previously
    completed..."); No; the dream chapter loads; player 2 there in co-op, not in solo (`[coop] player 2` in the log).
  - Load Game: the page; A; the save list (the card screen passed over as today).
  - B on the page: back on the title with the same row lit; Down then goes to the next row; nothing started.
  - Idle: leave the page up 200 s; no attract movie under it; the clock restarts after the close (log line).
  - One A only: the log shows one `[title] Start Game` and one state change, no second page.

### Step 4. A pick of the other mode restarts into it

- In the card's A, when `rooms_follow_coop()` returned true:
  - `setenv("ED_TITLE_RESUME", "start" | "load")`.
  - `setenv("ED_ISO", <the running disc>)`, so the new process finds the disc without the launcher.
  - If `title_state_path()` (it already follows the new mode's ED_ROOM_SLOTS) exists: `ED_QUICKLOAD` to it.
    Else `ED_SKIP_INTRO=1` and `ED_TITLE_STATE` to it, as `start_game` does for Skip intro (the intro is tapped
    through once, and the title machine is saved for next time).
  - A new `restart_process_keep()` that does not unset those (today's `restart_process` unsets ED_QUICKLOAD and
    ED_SKIP_INTRO; leave it as it is for its own callers).
- main.c: with `ED_TITLE_RESUME` set, skip the launcher as ED_QUICKLOAD does. Read it once into hle and
  `unsetenv` it (and ED_QUICKLOAD / ED_SKIP_INTRO / ED_TITLE_STATE once used), so a later "Return to launcher" restart
  shows the launcher again. Do **not** set ED_NO_LAUNCHER: it would stick for every later restart.
- hle.c: when the resume is set and the title's main menu is up (state 7, faded in), write TM+28 to the row and arm the
  same replay as step 3. No page this time.
- **Shows:** boot solo (`JTG_SET=mod_coop=0`), Load Game, pick Co-op: `[rooms] separate rooms on for this run`, the
  restart line, the new process loads `title-rooms.state`, Load Game lit, the save list comes up. Then the reverse:
  boot co-op, Start Game, Solo: `title.state`, the carry-over question, the dream chapter with one player. And with the
  title file moved aside: the intro tapped through once, the file made, then the resume. No hang-*.txt.

### Step 5. Mods > Co-op goes; its rows find homes

- Remove the "Split-screen co-op" row and the whole Co-op section page (`build_mods_rows` section 1), the Mods "Co-op"
  row (`build_mods`) and the launcher's "Co-op" row (`build_launcher`). Keep the `mod_coop` key: the picker writes it
  and the test tools set it (`JTG_SET=mod_coop=...`).
- Drop the two repeats: "Player 2 plays with" (the same field as player 2's Controls "Plays with") and "Player 1
  camera" (the same field as Display's Camera).
- "Screen split" moves to Display, after Camera, named "Co-op screen split". Shown always, so it can be set before
  a co-op game.
- "Player 2 camera" moves to Display, under Camera (Camera stays on Display by the user's choice of 2026-11-07). Or to
  player 2's Controls page: a user question below.
- Words that point at the old switch:
  - Sanity Effects: "Single player for now: choose Solo from Start Game or Load Game."
  - Frame rate's note: "Co-op plays at 60 frames a second." It greys only in a co-op game, not on the title before a
    pick (test `in_play()`).
  - Player 2's Choose Character card: not dimmed on the title (`off` also needs `in_play()`), and its foot reads "For
    co-op" there.
- Test doors: `ED_MENU_OPEN=coop` goes. Check `tools/ed` for any script that opens it (grep) and change it.
- Tidy (no user choice needed): hide Characters, Cheats and Sanity Effects in the title's Mods (they repeat Choose
  Character or are greyed there); hide Title video in a game; delete the never-shown rows (Options sections "IV ·
  Controls" and "VI · Mods" and its Disc row).
- **Shows:** the title's Mods (Challenge, Title video, Back), the Tome's Mods in a game, the launcher's Mods and main
  list (no Co-op), Display with the two moved rows, in solo and co-op settings. A co-op game started from the picker
  still splits as before, and "Co-op screen split" changed in the Tome takes effect when the page closes.

### Step 6. Save states made the other way

The game's memory-card saves already work both ways, mid-game too:
- A co-op save loaded solo: player 2 is parked out of sight (`coop_park`) and stays parked in a solo save.
- A solo save loaded in co-op: player 2 is made beside player 1 (`coop_spawn`).
- A co-op save loaded in co-op: the saved player 2 is taken over (`coop_adopt`).
So Load Game needs nothing more than steps 3-4.

Save **states** (`saves/slotN.state`) are tied to memory size: a co-op state (56 MB) is refused solo, a solo state
is refused with rooms. Today they fail with a log line.
- The launcher's Continue and Load game, and the Tome's Save States, read the state's MEM1 size first
  (a small reader beside `check_region`). 56 MB: co-op. Smaller: solo.
- If that differs from `mod_coop`: set `mod_coop` to the state's mode and restart with `ED_QUICKLOAD` = the state
  (the step-4 restart). No page: the state can only play in its own mode.
- **Shows:** a co-op state from the launcher with `mod_coop=0`: the restart, the state loads, both players there. A
  solo state from the Tome in a co-op game: the restart, one player.

### Step 7 (optional). HD portraits

- `ensure_portraits`: try the pack first (`texpack_read_rgba(14, 126, 89, hash, ...)`, flip the rows), else the disc.
  Hashes in the art list (Alex 4dc0cc88542db31e, Pius 887860f403544c27, ...).
- **Shows:** the pick page and Choose Character with HD on and off, fresh `res/ui/portraits`.

### Step 8 (only if the user says yes). Jump to Game asks too

- The same intercept for row 3 (Jump to Game), the replay pressing into the chapter grid.
- `ED_TITLE_PICK=solo|coop` answers the page with no press (for scripts). `tools/ed/jtg.sh` sets it from its
  `mod_coop`, so its press times do not change.
- **Shows:** `jtg.sh 1` reaches chapter 1 with both players in control at the usual frame (~1800), solo and co-op.

## Risks

- **The live title page lands first, or not.** Steps 3-4 depend on the pad hold and the release wait. If the live way is
  not merged, the page blocks the game as today, and the replay still works (the hold just is not there).
- **A press leaking.** The A that picks must not reach the game as a second A, and the replayed A must not reach the
  page. The replay waits for the real A to be let go and for the hold to end. Check the log for one state change.
- **The restart.** A few seconds, the window reopens. The disc must come through (ED_ISO). A missing title file costs
  one intro. Old title.state (Oct 30, 40 MB) loads into the 48 MB solo build with zeroes past its end, as today.
- **Another language.** The intercept does not need English, but the page's words are English (as all our pages).
- **Card notices.** The intercept passes through when one is up. State 7 asks none.
- **The test kit.** Whatever the user picks last is saved in settings.ini. `ed test`, `run.sh` and `jtg.sh` must keep
  setting `mod_coop` themselves (they do: `JTG_SET`). Note it in docs/coop-next-session.md.
- **Mid-game.** With the switch gone, a game cannot switch mode in play. Return to title screen, then Start or Load.
  `ED_TEST_COOP_OFF` stays for tests.
- **Return to title screen** reloads the boot state in the same process. A different pick there restarts (step 4), which
  is right.
- **The Tome look.** The Controls pick page also opens over the Tome (Settings > Controls). Its leaf is narrow
  (0.28-0.82 of the picture, half the window in co-op). Stack the two cards there, or make them smaller. Check a
  picture of it in step 2.

## Files

- runtime/host/hle.c: `title_main_choose` and the new `title_main_start_load` intercept; the `kMore` install;
  `hle_PADRead` (the replay, after `skip_card_select` and the live hold); the resume read.
- runtime/host/frontend.cpp: `Screen::Pick`, `build_pick`, `pick_card_rml`, the card move, Back, `past_root`,
  `build_options` Controls, the page switch (`FRONTEND_PAGE_TITLE_CONTROLS`, new `FRONTEND_PAGE_TITLE_PICK`),
  `build_mods`, `build_mods_rows`, `build_launcher`, `build_options_rows` (Display), `player_card_rml`,
  `restart_process_keep`, the state mode reader.
- runtime/host/frontend.h: the new page id.
- runtime/host/main.c: `ED_TITLE_RESUME` skips the launcher, then is unset.
- res/ui/menu.rcss: `.pcard.pick`, `.pcard-pair`.
- runtime/host/quickstate.c: a MEM1 size reader for a state file (step 6).
- tools/ed/jtg.sh: only in step 8.
- Docs after each step: docs/coop-next-session.md, docs/coop-player2-menu.md (the Controls page), and
  docs/coop-failed-attempts.md for any attempt that fails.

## Addresses used

- Title record TM 0x80309124: +20 fade (255 = shown; from the research, check in a run), +28 cursor, +40 row count,
  +48 flags (0x2 idle clock), +62 state (7 main menu).
- Title A 0x80019F30 (state 7 at 0x8001A464; Start 0x8001A4D4; Load 0x8001A50C; Jump 0x8001A638). Sound 0x8019DB90.
- Start's path: state 8, the card screen (host presses A), carry-over notice 0x8001FC54, new game 0x8001C768.
- Load's path: card manager 0x800AEE4C / 0x800AFC20, state 13, the save list (host passes the slot screen), read done
  0x800B2880, action 2 0x80022328(2).

## Questions only the user can answer

1. Changing between Solo and Co-op restarts the game window once (a few seconds), because co-op's separate rooms are
   set at start-up. Is that all right?
2. Jump to Game also starts a game. Should it ask Solo or Co-op too?
3. Player 2's camera: on Display under Camera, or on player 2's Controls page?
4. Player 2's own Sprint, Creep, Target, stick deadzone and vibration: give them their own (new work), or keep sharing
   player 1's for now?
5. When a player's character is "Default", the cards show Alex for player 1 and Pius for player 2. Or the emblem, as
   Choose Character does?

## Skeptic's check

Date: 2026-11-17. Read only: nothing built or run. Each point was checked in the code or the disassembly
(build/ed-db/disasm.txt). `findings.md` is not in this folder: only plan.md was there to check.

### Held up

- Addresses. TM = 0x80309124 (the A handler's r31 0x80309100 + 36). +62 state, +28 cursor, +40 count (Load writes 2 at
  0x8001A5E4). State 7 at 0x8001A464. Start 0x8001A4D4, Load 0x8001A50C, Options 0x8001A5EC, Jump 0x8001A638,
  Credits 0x8001A628. Every row ends at 0x8001A678 with sound 633. [read]
- The A handler is bound to A **and Start** (mask 0x1100). One intercept covers both. [read 02-title-menu.md 2]
- hle_dispatch: an intercept that leaves `g_hle_passthrough` false returns at once. The notify does not run then.
  [read hle.c `hle_dispatch`, the intercept and notify block]
- The kMore install requires `intercept == NULL`, so the new intercept goes in after it. That is safe.
- `frontend_open_page_skin` only stores the ids. The page opens in `frontend_frame`. So an intercept is needed. Right.
- Every co-op read of `mod_coop` in hle.c, camera.c and controls.cpp is live (read each time). So a same-mode pick can
  set `mod_coop` at run time. Saves made the other way: `coop_park` (co-op save, solo) and `coop_adopt` / `coop_spawn`
  are where the plan says (hle.c, the per-frame player block). [read]
- Memory: 24 + 24 MB, plus 8 MB only with ED_ROOM_SLOTS (.deps/GXRuntime/src/core/cpu.c). 48 solo, 56 rooms. [read]
- quickstate refuses any MEM1 size but 32 or 40 MB into a bigger build, and any smaller one under ED_ROOM_SLOTS. [read]
- The Tome look already lays the two cards side by side on the leaf (`body.skin-tome .pcards`, menu.rcss ~2012). The
  Tome risk is smaller than the plan says: reuse those rules.

### Wrong, with fixes

1. **The intercept must do the game's first step.** The A handler calls 0x8013E154 first (0x80019F5C). That sets
   r13-23564 = 1, "input taken this frame". Without it, the channel's later rows (Up/Down) can fire in the same frame.
   Fix: in the intercept, before opening the page, write 1 to r13-23564 (or guest-call 0x8013E154). Also stop the two
   whispers as the game does (TM+56, TM+52: 0x8019DC00 on each, then -1), so "Start Game" is not still whispered
   under the page.
2. **Sound 633 plays twice.** The intercept plays 633. The replayed A runs the game's A, which plays 633 again, then 634.
   Fix: either play nothing in the intercept and let the page's open be the feedback, or accept it and say so. Check
   it by ear in the step 3 run.
3. **The title file is 32 MB, not 40.** `saves/title.state` MEM1 is 0x2000000 (read from the file). title-rooms.state is
   0x3800000 (56 MB). slot1 and slot2 are 48 MB, slot3 is 56 MB. Both title files load, so the plan holds. But the
   picker will load the title file far more often, and title.state is from Oct 30, older than much of hle.c. Fix for
   step 4: use the title file only if its MEM1 size is this run's size. Else make it fresh (ED_SKIP_INTRO +
   ED_TITLE_STATE). (docs/coop-failed-attempts.md "The launcher's title state under the two room slots" says 40 MB
   too. It is 32.)
4. **"Only start_game calls both" is not all.** main.c calls `rooms_follow_coop()` at every boot, after the launcher and
   before the memory is made (main.c, after `frontend_run_launcher`). So a new process already follows the saved
   `mod_coop`. The picker does not need to call `rooms_follow_coop()` or `setenv` at all. Fix:
   - Add a pure test, `rooms_would_change(bool coop)`: true when (coop and the asked value is not "0") differs from
     `getenv("ED_ROOM_SLOTS") != nullptr`. No writes.
   - On a change: `settings_save()`, set `ED_TITLE_RESUME`, restart. Let the **new** main.c choose the title file, after
     its own `rooms_follow_coop()`. Its `title_state_path()` then already has the new mode. main.c's launcher check
     must also skip the launcher for ED_TITLE_RESUME (it tests only ED_QUICKLOAD and ED_NO_LAUNCHER today).
   - This also keeps `setenv` off the render thread while the game thread calls `getenv` (live pages run both at once).
     `setenv` racing `getenv` is not safe on the Mac build (play.sh), and can leave a dangling string on Windows.
5. **Do not unset the env at boot as written.** `ED_SKIP_INTRO` is read in `hle_install` (hle.c, `g_skip_intro =
   getenv(...)`), `ED_QUICKLOAD` in `quickstate_init`, and `ED_TITLE_STATE` again 120 frames after the intro
   (`skip_intro_apply` saves to `getenv("ED_TITLE_STATE")`; `hle_start_up_hidden` reads it each frame). Unsetting them in
   main.c before those run breaks the skip and the save. Fix: leave them set. `restart_process()` already unsets
   ED_QUICKLOAD and ED_SKIP_INTRO. Add ED_TITLE_STATE and ED_TITLE_RESUME to that list. hle copies ED_TITLE_RESUME into
   a static once at install, so "Return to title screen" in the same process does not resume twice.
6. **The resume must wait for the title file to be saved.** With no title file, the new process taps through the
   intro and saves the title 120 frames after it (`g_title_save_at`). The resume presses A as soon as state 7 is
   faded in. That can come first, and the saved title would then be mid-Start. Fix: wait while `g_title_save_at != 0`,
   and one frame more for the save to be done.
7. **The restart under a live page.** The old restarts run from a blocking menu: the game thread waits in `present()`.
   A live title page steps inside the game's own presents, and the game thread can be one frame ahead and running.
   `_execv` (Windows) ends this process while that thread runs. Fix: do the restart from `frontend_frame` inside the
   Present item, once the game thread is waiting in `present()` (the state the blocking menus restart from). Or open
   the page blocking (as `ED_TITLE_PAGE_PAUSE=1`) when the pick needs a restart. Check: no crash report, one restart line.
8. **The test kit does not set `mod_coop` everywhere.** `tools/ed/run.sh` plays "exactly as the user plays it (their
   settings.ini)", with `ED_QUICKLOAD` of a state and no ED_SETTINGS. jtg.sh copies settings.ini (fine). After the
   picker, the user's last pick decides every run.sh run. A 56 MB co-op state with a Solo pick is refused ("memory size
   differs") and the run shows nothing. This repeats docs/coop-failed-attempts.md "`ed test` under `mod_coop = 0`".
   Fix: step 6's MEM1 reader also runs at boot. In main.c, before `rooms_follow_coop()`, when ED_QUICKLOAD names a state,
   set this run's `mod_coop` from its size (56 MB: co-op) and log it. Do this in step 3, not step 6.
9. **jtg.sh's new game hits the page.** `JTG_NEWGAME=1` presses A on Start Game (jtg.sh, the MENU line). The page would
   take it and the run would stall. ED_SKIP_INTRO's A taps can also land on Start Game (jtg_report.py notes it). Fix:
   move `ED_TITLE_PICK=solo|coop` from step 8 into step 3. With it set, the intercept passes through, sets `mod_coop`
   to it, and opens no page. jtg.sh sets it from its `mod_coop`.
10. **Player 2's "Default" is not Pius.** `coop_p2_choice()` returns -1 for choice 0: player 2 then plays player 1's
    character. Fix: for player 2 on Default, show player 1's picture (or the emblem). And for either player on Default,
    the name line must not say "Alex": say "Each chapter's own", as Choose Character does. Change question 5 to match.
11. **The Controls row's return row is overwritten.** `build_options`' Controls row writes `g_controls_return_row =
    g_selected` (its Options row). The card's A then writes the card's row into the same word. Back from Pick would
    land on the wrong Options row. Fix: give Pick its own `g_pick_return` and `g_pick_return_row`, set where Pick is
    opened. Controls returns to Pick with the card's row; Pick returns with its own.

### Missed, and what to add

1. **The card screens are tested in six places, not two.** `Screen::ModsPage && g_mods_section == 3` appears in
   `list_scrolls`, `title_wide_page`, the hint line, the `settings` class, the `cards` class and the Left/Right move.
   Fix: one `card_screen()` helper (ModsPage section 3, or Pick), used in all six. Without `title_wide_page`, the title
   look puts the cards in the word column.
2. **`.pcard.pick` needs the Tome look too.** The plan says title and launcher looks. The Controls pick opens from the
   Tome's Settings. Add it under `body.skin-tome`.
3. **Who opened the Tome page.** In co-op, player 2 can open Settings from their Tome (`g_menu_player` 1). Today the
   Controls row gives them player 1's page. On the Controls pick, light the opener's card first (`g_menu_player`), not
   always Player I.
4. **Another language: black behind.** The intercept needs no English, right. But the title look's picture comes from
   the English Options picture (`titlevideo_options_from_bank` takes only that hash). In another language the page
   shows on black ("NOT written ... black behind"). Fine to ship; say it in the step 3 notes.
5. **The pick result crosses threads.** The card's A runs on the render thread, the replay on the game thread. Make
   the armed row and the result atomics.
6. **A replay that never lands.** If the channel does not take the press (a fade, a card notice), the replay would
   wait for ever. Fix: drop it after about 60 frames, with a log line.
7. **"Co-op is off" on player 2's card in a solo game.** With the switch gone, that word points at nothing. Use
   "Solo game" (in play) and "For co-op" (title, launcher).
8. **Launcher disc and restart.** A restart keeps argv. play.bat passes `--iso`, and argv's `--iso` wins over ED_ISO
   (main.c reads ED_ISO only when no path is given). If the user chose another disc in the launcher, the restart
   comes back on argv's disc. Rare. Compare `g_running_disc` with argv's, and log it.
9. **The newest save state is co-op.** slot3.state (Nov 15) is 56 MB, slot1 and slot2 are 48 MB. The launcher's
   Continue takes the newest. So step 6 is needed as soon as the launcher's Co-op row goes (step 5), not after it.
10. **HD portraits.** `texpack_read_rgba` returns false when HD textures are off. `ensure_portraits` writes the PNGs once,
    so turning HD off later keeps the HD files. Write the HD ones under their own names. The pack has 14 files at
    126x89, not 12: match by the hashes, not by count. The "flip the rows" step is not checked: `glyph_art.cpp base()`
    takes the pack's image as upright. Check one picture before flipping.

### Verdict

The plan's shape holds: an intercept on A and Start at state 7, rows 0-1, a replayed A for the same mode, and one
restart for the other mode. Fix before building: the 0x8013E154 step and the whispers (wrong 1), no `setenv` in the
picker and the new process choosing the title file (wrong 4), no early unset (wrong 5), the title-save wait (wrong 6),
the restart point under a live page (wrong 7), and the test kit (wrong 8 and 9: ED_TITLE_PICK and the boot-time state
mode belong in step 3). Then the six card tests (missed 1) and player 2's Default (wrong 10).

## The user's choices (2026-11-17)

- Solo / Co-op cards: the character portraits (01-portraits-one-and-two.jpg). They follow Choose Character: Solo shows
  player 1's pick, Co-op both players' picks; the Controls cards show each player's picture and name.
- Default's picture: the game over scene (room 69, script 1500): the skull on the open Tome, without the "Eternal
  Darkness: Sanity's Requiem" logo. It is a 3D scene, not a flat picture: capture it from the running game at a high
  resolution with the logo's draw hidden, crop it wide for the card (the skull and the open book), and ship it as
  res/ui/default.png in place of emblem.png on Choose Character, the Solo / Co-op cards and the Controls cards.
- A new "Random" choice on Choose Character, with a shadowed figure as its picture (from the default-art search in
  Screenshots for Claude/default-art/). The user (2026-11-17): the random character is rolled once and kept, across
  chapters and restarts (saved in the settings), until the player changes it on Choose Character at the main menu or in
  the Tome's Options during play. Choosing Random again rolls a new one. It stays a surprise: every card (Choose
  Character, Solo / Co-op, Controls) keeps the shadowed figure and the name "Random", never the rolled character.
- Random's picture (the user, 2026-11-17): the user's own picture of every character's silhouette in the mansion's hall,
  docs/research/solo-coop-picker/random-source.jpg (1316x800). Card crop: x 20, y 30, 1096x770 (keeps every figure;
  Screenshots for Claude/default-art/user-random-card-crops.jpg). Ship it as res/ui/random.jpg at the card's size.
- Default's picture, found (2026-11-17, default-art search): the skull-on-the-Tome scene is already a flat picture
  without the logo: UMnMenu.tpl image 26 (= UMnMenu.cmp image 12), 640x480 CMPR, XXH64 90abb11e0abc338a. HD copy:
  textures/GED/flashbacks/tex1_640x480_90abb11e0abc338a_14.dds, 2560x1920 BC7, stored bottom row first (flip the rows);
  card crop x 100-2560, y 197-1934 upright. Not UMnMenu.tpl image 12 (a redder variant, no HD copy). This replaces the
  live capture of room 69. Mock-up: Screenshots for Claude/default-art/00-top-picks.jpg (pick 1).

## Built: step P1 (2026-11-17, checkpoint commit)

- Default's picture: `ensure_portraits` also writes `portraits/Default.png` from UMnMenu.tpl image 26 (checked by its
  XXH64, crop x 25-640, y 46-480 upright, 615x434); `ensure_default_hd` writes `portraits/DefaultHD.png` from the pack
  (2560x1920 BC7, rows stored bottom first: turned, made half size 1230x868), stamped in `DefaultHD.txt`.
  `portrait_rml` takes HD, else the disc's, else emblem.png. The preparing step makes both (`kPortraitsVersion` 2).
- Step 7 done: `ensure_portraits_hd` writes the twelve 504x356 pack portraits to `portraits/hd/`, found by each disc
  picture's XXH64 over 6144 bytes (the pack names matched all twelve; the rows are stored bottom first, as the disc's).
  Both HD checks are answered from a cache while HD textures stay on or off.
- Random: `SETTINGS_RANDOM` 13, `mod_pN_random`, `settings_character()`, `settings_costume()`,
  `settings_roll_random()` in settings.c. hle.c reads the character through them (player_model, coop_p2_choice,
  costume_on, coop_name_choice). The roster has Random second (index 1), so frame indexes after Default moved by one.
- The cards for the pick page (later steps) take their picture from `portrait_rml` too, so Default and Random follow.

### Built: step P1 review fixes (2026-11-17)

- A pack picture that can't be used (Default's copy not 2 or more times 640x480, a portrait not a whole multiple of
  126x89, a write that fails) is tried once: the stamp then says `failed <id>` (`DefaultHD.txt`,
  `portraits/hd/stamp.txt`), the preparing step leaves it, and the pages don't decode it again. A new pack file (size
  or time) is a new id, so it is tried again. A folder that can't be written can't keep that stamp either.
- An old portraits stamp (a later `kPortraitsVersion`) now makes Default's HD picture again with them (it was deleted
  and left unmade). The preparing step looks at both HD answers afresh (`g_default_hd_for`, `g_portraits_hd_for`).
- `ensure_portraits_hd` keeps no answer while no disc is known (the pack's portraits are found by the disc's pictures).
- Seen: scratch res and settings, the launcher's preparing step. Run B (old stamp) remade DefaultHD; run C (a scratch
  pack with a 640x480 Default) wrote the `failed` stamp; run D said "everything prepared already"; run E showed the
  disc's Default card with no new try; run F (the real pack, ED_PREPARE=1) made everything. Pictures and logs:
  Screenshots for Claude/solo-coop-picker/P1-review-fixes/. Not run: the no-disc case (code only).
