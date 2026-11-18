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

## Built: step P2, the two-card page and Controls through it (plan steps 1 and 2; 2026-11-17)

- `Screen::Pick` (frontend.cpp) with `PickKind::Mode` (Solo / Co-op) and `PickKind::Controls` (Player I / Player II),
  its own `g_pick_return` / `g_pick_return_row` (skeptic's wrong 11), `open_pick()`, `pick_back()`, `build_pick()`: the
  two cards (rows card 1 and 2) and Back under them, as Choose Character's page (Back is there for the mouse).
  `card_screen()` (Choose Character's or Pick) is used in the six card tests (missed 1).
- `pick_card_rml`: Choose Character's `.pcard` mount, larger (`.pcard.pickcard`), no meters. The picture is
  `portrait_rml` of each player's choice, so Default's Tome and skull, Random's silhouettes (never the roll) and a form's
  own come from P1. Player 2 on Default shows player 1's frame with no form (hle.c `coop_p2_choice` -1 and `costume_on`
  NULL: player 1's character in its own body) and says "The same as player 1" (wrong 10). Co-op: player 1's mount in
  front, player 2's behind and up to the right (`.pcard-pair`, `.behind` `.p2-frame` blue, `.front` gold), the names
  "A and B" (one name and "For both players" when player 2 is on Default).
- The first card lit: Solo / Co-op on `mod_coop`; Player I / II on `g_menu_player` (missed 3; player 2's Tome has no
  Settings plaque today, so it is always Player I in practice).
- `FRONTEND_PAGE_TITLE_PICK = 12` (frontend.h; `player` is the title row, 0 Start Game, 1 Load Game: the heading), a
  root that closes on Back. `g_on_title` takes 4..12. A on Solo / Co-op only logs `[frontend] pick: solo|co-op`: nothing
  opens the page from the game yet (step 3).
- Controls: the title's page 9 (Options > Controls), `build_options`' Controls row (the Tome's Settings > Controls and
  the launcher's Options > Controls) open the Player I / Player II page first. A card: that player's Controls
  (`[frontend] pick: player N's controls`); Back from it (button or row) comes back to the page on that card; Back again
  goes where Controls went before (Options' Controls row, or closes the title page). `past_root`: a Pick root closes at
  Options.
- menu.rcss: `.pcard.pickcard`, `.pcard-pair`, `.p2-frame` in the launcher look, the title look (`border` gold and blue on
  the pair) and the Tome look (em sizes on the leaf). Body class `pickpage` (not `pick`: docs/coop-failed-attempts.md).
- Test doors: `ED_MENU_OPEN=pick-mode`, `pick-load`, `pick-controls` (the launcher); `ED_TITLE_PAGE_TEST=FRAME:PAGE[:PLAYER]`
  (hle.c `title_main_draw`: once, on the main menu from guest frame FRAME, the page opens in the title look as Choose
  Character's does, with the idle clock and the pad held).
- Seen (Screenshots for Claude/solo-coop-picker/P2-pick-page/, runs logs/run-20261117-1713*..1726*-jtg-p2-*): the title
  Controls page with each card lit, Player 2 Controls, Player 2 Mapping, Back to the page on Player II, Back to the
  title Options list on Controls (-172624); Start Game and Load Game's Solo / Co-op over the title with player 1 on
  Random, Default and Paul (no form) and player 2 on Ellia's second form (only as the picture behind: the pair's line
  has no form names) and Lindsey (-171306, -171541, -172100); player 1's form on this page only in -173951 (below);
  4:3 (-172100); the Tome's Settings > Controls in solo
  (-171913) and in co-op in player 1's half, through Player 2 Controls and back to Settings (-171709), co-op 4:3 with
  Random and player 2 on Default (-172226); the launcher's pages (ED_MENU_OPEN, scratch settings). The title music runs
  under the pages as for the other title pages (live page lines in each run; not listened to).
- Not seen: the Solo / Co-op page in the Tome look (nothing opens it there), another language, the user's play, a mouse.
  The first title page of a run with no `res/ui/portraits/hd` yet made the HD portraits while it opened (about 300 ms,
  "set up in 314.6 ms" in -172624): fixed in the review below.

## Built: step P2's review fixes (2026-11-17, the P2 checkpoint commit)

- **No HD pictures made over a live title page.** `hd_pictures_wait()` (frontend.cpp): on a title-look page that the
  title runs under (not with ED_TITLE_PAGE_PAUSE=1), `make_portraits_hd` and `make_default_hd` do not write; the disc's
  portraits (and Default's disc picture) show, and the answer is not kept (`g_hd_pictures_waited`), so the next page
  that stops the game (the Tome's, the launcher's) or the next start's preparing page makes them. One log line:
  "HD portraits not made over a live title page". This also covers the title's Choose Character, which had the same
  cost before P2. The disc portraits are still written on a page when missing (12 small PNGs, as before P2).
  Seen: logs/run-20261117-173951-jtg-p2fix-mode-form-hdwait, no `portraits/hd`, HD textures on: page 12 "set up in
  18.0 ms" (was 314.6 ms for page 9 in -172624), slowest live step 1.6 ms; Lindsey's disc picture behind; no files
  written to res/ui/portraits. The later making on a Tome page was not run (read in the code only).
- **Player 1's form on Solo / Co-op** seen in the same run: Karim's Spirit form (from the costume bits) on Solo with
  the line "Spirit", and in front on Co-op with "Karim and Lindsey" (pictures 17 and 18 in P2-pick-page/).
- Review finding rejected in part: the Mode runs did have a form (player 2's Ellia, form 2, from the costume bits), and
  Ellia-2's picture is behind on Co-op in pictures 07 and 08. Only player 1's form had not been seen there.

## Built: step P3, Start Game and Load Game ask first (plan step 3 with the skeptic's 1, 2, 8, 9; 2026-11-17, checkpoint commit)

- **The intercept** (hle.c `title_main_start_load`, on `TITLE_A` 0x80019F30, installed after the `kMore` notifies when
  the entry has no intercept, its `bl 0x8013E154` is at +0x2C and 0x8001A67C is `li r3, 633`). It passes through unless
  the title is in state 7 on row 0 or 1 with no card notice. Then it does the A's first steps as the game: guest-calls
  0x8013E154 (input taken this frame, wrong 1), stops the whispers at TM+56 and TM+52 (0x8019DC00, then -1), plays 633
  once, sets the idle reset and the live page's pad hold, logs `[title] Start Game: Solo or Co-op?` and opens page 12
  (`player` = the row). While the page is up a title A is swallowed. Start is the same handler (mask 0x1100): seen.
- **The pick** (frontend.cpp, the Mode card's A over the title): `rooms_would_change(coop)` false (the same rooms as this
  run) sets `mod_coop`, `settings_coop_chosen()`, `changed()`, `settings_save()`, stores the answer in an atomic
  (`g_title_pick_result` = row * 2 + co-op, `frontend_title_pick_take`) and closes. True: the card's line says "Co-op
  needs the game started again: not ready yet" (`g_pick_note`), the page stays (step 4 builds the restart). If no page
  can show (`ensure_ui` fails), the answer is this run's mode, so Start never goes dead.
- **The replay** (hle.c `title_pick_replay`, in `hle_PADRead` after the live hold): once the close count has moved (every
  button let go), it takes the answer; for a pick it presses A three reads in six on the same row until the intercept
  sees it (`g_title_replay_row`), which then passes it through to the game's own Start (634, state 8, the card screen,
  the carry-over question) or Load (card manager, state 13). Dropped with a log line after 60 frames, or if the state
  or the cursor moved (not seen). **Sound 633 once** (wrong 2): the replayed A's own 633 (the call at 0x8001A684,
  return 0x8001A688) is left out in `tome_copy_sound`, the SOUND_PLAIN intercept, on that frame only. Load Game plays
  no 634 of its own (read in the disassembly), so its pick is silent, as the game's Load is after its one 633.
- **ED_TITLE_PICK=solo|coop** (wrong 9): no page; the A passes through, and `mod_coop` is set for this run (not saved)
  when the rooms fit; else it is kept and logged. `tools/ed/jtg.sh` exports it from the run's `mod_coop` unless it is
  already in the environment (`ED_TITLE_PICK=` empty asks).
- **A boot state's mode** (wrong 8): main.c calls `coop_follow_boot_state()` before `rooms_follow_coop()`. With
  `ED_QUICKLOAD`, `quickstate_file_mem1_size()` reads the state's MEM1 size from the section headers: 56 MB co-op,
  smaller solo (unless co-op has no separate rooms, `ED_ROOM_SLOTS=0`). `settings_run_coop()` sets this run's mode and
  `settings_save` writes the file's own `mod_coop` until a pick or Mods > Co-op (`settings_coop_chosen`).
- **Seen** (Screenshots for Claude/solo-coop-picker/P3-start-load/, scratch card copy `ED_CARD`, jtg settings copies):
  - -175205-jtg-p3-start-solo (solo): Start Game -> the page, Solo lit first; Right lit Co-op, Left Solo, A: "solo for
    Start Game, saved", the replay seen by the intercept on the next frame, 633 left out, the title fades, the
    carry-over question (pictures 01, 06, 07).
  - -175333-jtg-p3-start-solo-dream: the same, then Right and A on No: room 96, the opening (Silicon Knights, "before
    humanity") (02).
  - -175447-jtg-p3-load-coop (co-op, separate rooms on): Load Game -> the page, Co-op lit first; Solo, A: "needs the game
    started again", the card's line says so, the page stays (08); Co-op, A: the save list (03).
  - -175605-jtg-p3-back-start (solo): B: "closed with no pick", the main menu with Start Game lit; Down lit Load Game, Up
    Start Game; the Start button on Start Game opened the page again; A: the carry-over question (04). One state change,
    no second page.
  - -175805-jtg-p3-newgame-auto: `JTG_NEWGAME=1` with jtg's `ED_TITLE_PICK=coop`: "Start Game: co-op (ED_TITLE_PICK, no
    page)", the new game went on (05). -175719-jtg-p3-jtg-timing: `jtg.sh 1`, control at frame 1795 (1768-1836 in the
    P1 and P2 runs).
  - run.sh -175904-p3-bootmode-slot3 (scratch settings `mod_coop = 0`): "slot3.state is a co-op state (56 MB): this run
    plays co-op", the rooms on, the state loaded, player 2 there; the scratch file's hash unchanged. -175948-p3-bootmode-slot1
    (a copy of the user's settings, co-op): "a solo state (48 MB): this run plays solo", loaded, player 2 parked; the copy
    still equals settings.ini.
  - The page's opens: set up in 13-14 ms and no "ran dry" in four of five; the first open of the first run after the
    build: 34.4 ms and "ran dry after a 53 ms pause". Not listened to (muted runs).
- **Not seen:** another language (the intercept needs no English; the page shows on black there, missed 4), the page left
  up 200 s (the idle reset is Choose Character's code: its "starts again" line was seen), a replay dropped, the
  `ensure_ui` fallback, `ED_TITLE_PAGE_PAUSE=1`, the user's play. The frontend's "page 12 opened ... for player 2" line
  means the row (Load Game), not a player.
- **Finished 2026-11-17 (checkpoint).** The user said the page did not appear on Start Game or Load Game. Their last
  play (logs/play.log, 16:16) was before this step was built (17:51), so it is expected. The reviewers found nothing.
  The finisher also ran the user's own path: the launcher's Play (`ED_LAUNCHER_TEST_KEYS=a`), play.bat's
  `ED_ROOM_SLOTS=2 ED_COOP_STAY=1`, a copy of the user's settings (co-op, Skip intro) and card, so
  saves/title-rooms.state is loaded through the launcher (`ED_QUICKLOAD` set by `start_game`), and no `ED_TITLE_PICK`.
  - -181610-p3f-playbat-start: Start Game opened the page at frame 701, Co-op lit; Solo, A: the "needs the game
    started again" line, the page stayed; Co-op, A: saved, the A replayed at 1034, the carry-over question.
  - -181655-p3f-playbat-load: Down, A on Load Game: the page at 741, Co-op lit; A: the save list.
  - Pictures 09 and 10 in P3-start-load/. settings.ini, the card, title-rooms.state, title.state and slot3.state kept
    their hashes.

## Built: step P4, a pick of the other mode starts the game again in it (plan step 4 with the skeptic's 3-7; 2026-11-17)

- **The card's A** (frontend.cpp `build_pick`, Mode): when `rooms_would_change(coop)`, `mod_coop` is set,
  `settings_coop_chosen()`, `settings_save()`, the card's line says "Starting the game again in co-op" (or solo), and
  `g_pick_restart_row` is the title row. Nothing in the environment is changed here (wrong 4).
- **The restart point** (wrong 7): `menu_step` returns at once while a restart is pending (no B can close the page); 300 ms
  after the A, `pick_restart_now` presents the page by itself with `dol_aurora_ui_frame` (as a blocking page does, from the
  frame hook inside the Present item), until the game's pad reads (`hle_pad_reads`, hle.c) stand still for 100 ms (at most
  2 s), then `restart_process("start"|"load")`. Seen: "the game is held (no pad read for 100 ms) after 100-122 ms" in
  every run. A failed `_execv` puts this run's mode back and the card says "The game could not start again" (not run).
- **restart_process(resume)** drops ED_QUICKLOAD, ED_SKIP_INTRO, ED_TITLE_STATE and ED_TITLE_RESUME for every caller
  (wrong 5's list); with a resume it sets ED_TITLE_RESUME and ED_ISO = `g_run_disc` (the disc this run plays).
- **main.c:** ED_TITLE_RESUME skips the launcher (as ED_QUICKLOAD does), takes ED_ISO over argv's `--iso` and logs a
  difference (missed 8; seen only as slashes, "dump\GEDP01.iso" vs argv's "dump/GEDP01.iso"). After `rooms_follow_coop`,
  `frontend_title_resume_boot()` picks the title machine of the new run's rooms if its MEM1 is this run's (56 MB rooms,
  48 MB without), else ED_SKIP_INTRO + ED_TITLE_STATE (made fresh, the start-up behind black) (wrong 3). Nothing is unset
  at boot (wrong 5). The user's saves/title.state is 32 MB, so the first solo restart makes a 48 MB one over it (~10 s).
- **hle.c `title_resume_step`** (in `title_pick_replay`, each pad read): ED_TITLE_RESUME read once; when the title is in
  state 7 with no page, no hold, no replay, the intro skip over, `hle_start_up_hidden()` false and `g_title_save_at == 0`
  (wrong 6) for 20 frames in a row: TM+28 = the row and the pick's replay armed. The intercept passes that A through. Seen
  after the fresh title: "title menu saved" then the A two frames later (frame 592-595).
- **Test kit:** `ED_INPUT_SCRIPT_RESUMED` is the restarted run's script (else none). tools/ed/shots.ps1 and away.ps1
  follow the game into its new process (looked for 5 s; pictures go on in the same folder and shots.csv).
- **Seen** (Screenshots for Claude/solo-coop-picker/P4-restart/, scratch card, jtg settings copies or a copy of the
  user's settings):
  - -182818-jtg-p4-solo-start-coop: solo, Start Game, the page (01), Co-op: the note (02), restart in 1.3 s,
    "separate rooms on", title-rooms.state, Start Game lit (03), A at 604, the dream with both players and the screen
    split (04; control at 1715).
  - -183004-jtg-p4-coop-load-solo: co-op, Load Game, the page (05), Solo: the note (06), "title.state is from a smaller
    memory": black while it is made (07), saved, A at 595, the save list in solo (08).
  - -183125-jtg-p4-coop-start-solo: co-op, Start, Solo, now from the fresh 48 MB title.state: the dream with one player
    (09; timeline P2 none).
  - -183252-p4-playbat-coop-start-solo: run.sh launcher with play.bat's ED_ROOM_SLOTS=2 ED_COOP_STAY=1 and a copy of the
    user's settings (co-op, Skip intro): the launcher's Play, title-rooms.state, Start, Solo: the restart skipped the
    launcher, the title made fresh, Start Game, the dream in solo.
  - -183425-jtg-p4-same-mode-solo: the same mode still goes on with no restart.
  - saves/title.state and saves/boot.state (both rewritten by the runs) put back byte-identical; settings.ini, the card,
    title-rooms.state and slot3.state never changed.
- **Not seen:** a failed restart, B pressed during the note, ED_TITLE_PAGE_PAUSE=1 (blocking page), another language,
  the Mac build (`execv`), the sound by ear, where the new window opens on the user's desktop (the runs moved it away),
  a slow machine where the game thread holds longer than 2 s.

## Built: step P4 review fixes (2026-11-17, checkpoint commit)

Four reviewer findings were checked against the code. All four were real, and all four are fixed:
- **No disc list after a restart.** A restarted program skips the launcher, and only the launcher filled `g_discs`.
  Return to launcher then showed "None found" with Play turned off. Now main.c calls `frontend_resume_discs(iso)`. It
  fills the list as `frontend_run_launcher` does: the disc being played is chosen, with `g_disc_on_main` and
  `g_running_disc` set the same way. Seen in the log only: "started again past the launcher: 3 disc(s), dump\GEDP01.iso
  chosen".
- **`_execv` split arguments with spaces.** The Windows CRT joins argv with spaces and adds no quotes. An exe started from
  Explorer or a shortcut as "E:\Eternal Darkness Recomp Fork\..." came back split, and the restart failed. Now
  `restart_process` quotes each argument the way CommandLineToArgvW reads it back. Seen working: the exe, the dol and
  the disc given as full paths with spaces, then solo, Start, Co-op, and the co-op dream after the restart. The old,
  unquoted build was not run that way.
- **`g_title_resume` stayed set when the player acted first.** Two fixes in hle.c. In `title_main_start_load`, the
  player's own A ends the resume. On the resume's row it goes on with no page; on the other row the page asks. In
  `title_resume_step`, once the title has been ready, leaving the main menu ends it. Seen: A pressed again and again as
  the title came up: "the player's own A, started again for it: it goes on, no page", then the co-op dream. The
  "left the main menu" path and Return to title were not run.
- **A press on the black start-up showed the intro.** `title_resume_cover()` (ED_TITLE_RESUME with the start-up hidden)
  clears the players' pads, and test scripts too, before the skip's own taps. While it holds, a press cannot end the
  skip. Seen: A, Start, B and Down pressed every 20 frames from frame 100 to 560 on the black. The intro ended at 452 as
  usual, the screen stayed black, "title menu saved", the A at 595, then the solo save list. The Mods Skip intro by
  itself, with no restart, is unchanged.
- Runs: logs/run-20261117-185008-jtg-p4f-fresh-presses, -185154-p4f-spaces-mash, -185308-p4f-playbat-coop-start-solo
  (the play.bat way, regression). Pictures: Screenshots for Claude/solo-coop-picker/P4-review/01-07. saves/title.state
  and boot.state were rewritten by the runs and put back byte-identical, with their file times. settings.ini, the card,
  title-rooms.state and slot3.state were never changed.

## Built: step P5, Mods > Co-op goes and its rows find homes (plan step 5; 2026-11-17, checkpoint commit)

- **Gone:** Mods > Co-op (the whole section 1 of `build_mods_rows`: "Split-screen co-op", "Screen split", "Player 2 plays
  with", "Player 2 controls", "Player 1 camera", "Player 2 camera"), the Mods list's "Co-op" row (`build_mods`), the
  launcher's "Co-op" row (`build_launcher`) and the test door `ED_MENU_OPEN=coop` (no script in tools/ used it). Mods'
  other sections keep their numbers (2 Challenge, 3 Characters, 4 Cheats, 5 Sanity Effects); `g_mods_section` starts at
  2. The settings keys (`mod_coop`, `mod_split`, `mod_p2_input`, `mod_p2_camera`) are unchanged in settings.c, so old
  settings.ini files load as before.
- **Repeats dropped:** "Player 2 plays with" is player 2's Controls "Plays with"; "Player 1 camera" is Display's Camera.
- **Moved to Display, under Camera** (`build_options_rows`): "Co-op screen split" (Side by side / Above and below,
  `mod_split`) and "Player 2 camera" (`mod_p2_camera`). Shown and changeable always. Notes: "For co-op games." / in a
  co-op game "Changes when the menu closes." (the Tome page keeps its halves, `g_skin_split`); "Player 2's half in
  co-op. Original: the game's own cameras."
- **Never-shown rows deleted:** Options' sections IV (Controls) and VI (Mods, with its Disc row) in
  `build_options_rows`. Options itself has its own Controls, Mods and Disc rows (`build_options`).
- **Words:** Sanity Effects in a co-op game: "Single player for now: choose Solo from Start Game or Load Game." Frame
  rate: greyed (and 60) only in a co-op game (`mod_coop && in_play()`); before a game it can be set for solo, note
  "Co-op plays at 60 frames a second." Player 2's Choose Character card is dimmed only in a solo game ("Solo game: pick
  Co-op at Start Game"); before a game, with solo picked last, it is not dimmed and says "For co-op: pick it at Start
  Game".
- **Tidy:** the title's Mods hides Characters (Choose Character is the title's own row), Cheats and Sanity Effects;
  Title video is hidden in a game. `card_screen()` was already the one helper for the six card tests (P2).
- Docs: coop-play-test.md (sections 6, 8, 9, 13), coop-player2-menu.md, tools/win/README.txt and play.bat's comment
  no longer point at Mods > Co-op.
- **Seen** (Screenshots for Claude/solo-coop-picker/P5-menus/, scratch settings, the scratch card copy):
  - Launcher (scratchpad runs, ED_MENU_OPEN, scratch settings): the main list with no Co-op (01); Mods: Challenge,
    Cheats, Sanity Effects, Skip intro, Title video (02); Display with the two rows under Camera, solo (03); co-op:
    Frame rate not greyed, 120, "Co-op plays at 60 frames a second." (04).
  - Title (jtg, `ED_TITLE_PAGE_TEST`): logs/run-20261117-190802-jtg-p5-title-mods-solo, Mods = Challenge, Title video,
    Back (05); -190901-jtg-p5-title-display-solo, "Co-op screen split" lit with "For co-op games." and "Player 2 camera"
    with its note, the list scrolling (06, 07); -191026-jtg-p5-title-display-coop, Frame rate open in co-op on the title
    with its note (08); -190947-jtg-p5-title-chars-solo, player 2's card not dimmed, "For co-op: pick it at Start Game"
    (09).
  - Tome, co-op (-191143-jtg-p5-tome-coop): Settings > Display in player 1's half, Frame rate greyed, the split changed
    to "Above and below" with "Changes when the menu closes." while the page kept side by side (10); after the close
    the screen split above and below, the Tome in the top half (11); the Mods plaque: Challenge, Characters, Cheats,
    Sanity Effects, Back (12); Sanity Effects' greyed rows say "Single player for now: choose Solo ..." (13).
  - Tome, solo (-191446-jtg-p5-tome-solo): Display with Frame rate open (14); Mods with no Title video (15); Mods >
    Characters: player 2's card dimmed, "Solo game: pick Co-op at Start Game" (16).
- **Not seen:** another language; the mouse; the Solo / Co-op page itself again (its code is unchanged); a co-op game
  started from the picker after this build (the jtg co-op run goes through ED_TITLE_PICK=coop and split as before);
  "Player 2 camera" changed and then played; the user's play. `ed test` not run, as told.

## Built: step P5 review fix (2026-11-17, the P5 checkpoint commit)

- **Frame rate's note** (`build_options_rows`): the test was `coop_game || faster`, so on any screen faster than 60
  Hz a solo player saw "Co-op plays at 60 frames a second.", even in a solo game. Now the co-op note shows only in a
  co-op game or, on a faster screen, when co-op was the last mode picked (`coop_game || (faster && mod_coop)`). A solo
  player on a fast screen gets no note, as before P5; a 60 Hz screen still gets the screen-rate hint.
- **Seen** (launcher, `ED_MENU_OPEN=display`, keys d,d, scratch settings copies, 120 Hz screen): solo, Frame rate lit
  and no note (P5-menus/17); co-op, Frame rate lit with "Co-op plays at 60 frames a second." (18). Not seen: the
  same row in a solo Tome game (the same function and test, `mod_coop` 0 there), a 60 Hz screen.

## Built: step P6, save states made the other way (plan step 6; 2026-11-17, checkpoint commit)

- **Before a game it already worked** (P3's `coop_follow_boot_state`): the launcher's Continue and Load game set
  `ED_QUICKLOAD`, and main.c plays the state's mode (56 MB co-op, smaller solo). Nothing changed there. The Slot page's
  Load row now says so: "Made in co-op: it plays in co-op." (only when it differs from the mode picked last).
- **In a game** (the Tome's Save States, or the launcher over a running game; both reach `start_game(slot)` with
  `g_in_game`): new `state_other_mode(path)` in frontend.cpp reads the state's MEM1 size (`quickstate_file_mem1_size`)
  and asks `rooms_would_change(coop)`, the same rule as the boot. When it is the other mode, `restart_process(nullptr,
  path)` starts the program again with `ED_QUICKLOAD` = the state, `ED_STATE_RESUME` = the state and `ED_ISO` = this
  run's disc. The new process's `coop_follow_boot_state` and `rooms_follow_coop` choose the mode; settings.ini keeps the
  mode picked last (`settings_run_coop`, as for any boot state). No page and no question: a state can only play in its
  own mode. The Load row's note says first: "Made in co-op: the game starts again in co-op to load it."
- `restart_process` drops `ED_STATE_RESUME` for every caller, and on a failed restart puts back what it set (the page
  stays as it was; not run). main.c: `ED_STATE_RESUME` is handled like `ED_TITLE_RESUME` for the disc (ED_ISO over
  argv's `--iso`, `frontend_resume_discs`), so Return to launcher has the disc list. The launcher is skipped because
  `ED_QUICKLOAD` is set. hle.c: such a run plays `ED_INPUT_SCRIPT_RESUMED` (test scripts), not the first run's presses.
- A 32-48 MB state with no player 2 in it (made in solo) in a co-op game with the separate rooms now restarts into solo
  (it was refused). A 32-48 MB state with player 2 in it is not a solo state: see the review fix below. A 56 MB state
  where co-op has no separate rooms (`ED_ROOM_SLOTS=0`) is still refused, as at boot.
- The memory card's saves need nothing: `coop_park`, `coop_spawn` and `coop_adopt` were not changed (not run again).
- **Seen** (Screenshots for Claude/solo-coop-picker/P6-state-mode/, scratch settings copies and a scratch card, run.sh
  `launcher` with `ED_MENU_OPEN=slotN` + `ED_LAUNCHER_TEST_KEYS=a` so the launcher fills the disc list):
  - -193534-p6-solo-tome-load-slot3: a fresh solo state (slot 5, made for the test from a jtg solo run and deleted after)
    loaded from the launcher, solo; the Tome's Save States (01), Slot 3's Load with "Made in co-op: the game starts
    again in co-op to load it." (02); A: "slot 3 is a co-op save state ... starts again", the restart in 2 s, the disc
    list filled, separate rooms on, slot3.state loaded in room 69, both players, the screen split (03, 04).
  - -194822-p6-coop-tome-load-slot5-e: slot 3 from the launcher with scratch `mod_coop = 0` (the launcher's note "Made in
    co-op: it plays in co-op.", 05), co-op; the Tome in player 1's half, Slot 5's Load with "Made in solo: the game
    starts again in solo to load it." (06); A: the restart, separate rooms off, slot5.state in room 341, one player
    (timeline "P2 none"), 121 fps (07).
- **Test notes:** in a run with no launcher (jtg.sh, run.sh with a state) the disc list is empty, so the Slot page's
  Load is greyed and skipped: a "d" from Save here lands on Duplicate (the first try copied slot 3 to slot 4, removed
  after a byte check). slot3.state opens the Tome on four first-time hints (Use, Equip, Check, L/R): four B presses
  before L. saves/slot1.state loaded in solo stops after about 60 frames (saves/reports/hang-20261117-193301.txt), the
  same place as P3's run (hang-20261117-175959.txt, P3's build): older than P6, not looked into.
- **Not seen:** F11 (load the newest state) still refuses a state of the other mode; the launcher over a running game
  (same `start_game` code); a failed restart; the Mac build; the user's play. `ed test` not run, as told.

## Built: step P6 review fix (2026-11-17, the P6 checkpoint commit)

- **The finding (real).** `state_other_mode`, like P3's `coop_follow_boot_state`, took every state that is not 56 MB as
  made in solo. The user's saves/slot1.state and slot2.state are 48 MB, but they hold a player 2 in play (their COOP
  section: player 2 found, not parked). They were made in co-op before the separate rooms. So in a co-op game the Tome's
  Slot 1 said "Made in solo" and restarted the program into solo, ending the co-op game, and slot 1 then stops in solo
  after about 60 frames (hang-20261117-193301.txt). Before P6 the load was refused and the co-op game went on.
- **How a state's mode is read now.** New `quickstate_file_coop_p2(path)` (quickstate.c) reads the COOP section and asks
  `hle_coop_state_p2_in_play` (hle.c: player 2 found and not put out of the way). Read from the saves folder: every
  56 MB state and slot1/slot2, test-cofm, jtg-ch1-room183, reload-p2, karim-start, lever, p2cam-door, shadow-room338,
  coop-apart, coop-recover have player 2 in play; a fresh solo state (made by a jtg solo run) has none.
- **The rule** (frontend.cpp `state_old_coop`, `state_other_mode`): a 32-48 MB state with player 2 in play is an old
  co-op state. In a co-op game with the separate rooms it returns -2: no restart; the Slot page greys Load with
  "Made in co-op before the separate rooms: it loads only in a solo game." (still lit by Up/Down, so the note can be
  read; A does nothing). `start_game` logs and returns for it (the launcher's other ways in). In a solo game it loads in
  place, as before P6. A 32-48 MB state with no player 2 still restarts a co-op game into solo (P6, unchanged).
- **Before a game** (the launcher, P3's rule, kept): such a state plays in solo, player 2 out of the way. Its Load row
  now says "Made in co-op before the separate rooms: it plays in solo, without player 2." (it said "Made in solo"), and
  the boot log says "a co-op (before the separate rooms) state (48 MB): this run plays solo, player 2 out of the way".
- **Seen** (Screenshots for Claude/solo-coop-picker/P6-review/, scratch settings copies and a scratch card, play.bat's
  ED_ROOM_SLOTS=2 ED_COOP_STAY=1):
  - -200302-p6f-coop-tome-slot1-old-coop: co-op game from slot 3, Tome > Save States > Slot 1: Load greyed and lit,
    the note (01); A on it: nothing, still the co-op game, both halves (02). No restart line, no hang report.
  - -200442-p6f-coop-tome-slot5-solo: the fresh solo state as slot 5: "Made in solo: the game starts again in solo to
    load it." (03); A: the restart, "separate rooms off", room 341 with one player (timeline "P2 none") (04).
  - -200644-p6f-launcher-slot1-note: the launcher's Slot 1 with co-op settings: the new note (05).
  - -201037-p6f-playbat-start and -201117-p6f-playbat-load: the user's way (launcher Play, a copy of their settings,
    no ED_TITLE_PICK): Start Game and Load Game each open the Solo / Co-op page (06, 07).
- The test's slot 5 was deleted. settings.ini, slot1-3, boot.state, title.state and title-rooms.state have their
  hashes from before the runs.
- **Not seen:** slot 1 or 2 loaded in a solo game from the Tome (unchanged from before P6); the launcher over a running
  co-op game choosing slot 1 (same `start_game` and `build_slot` code); another language; the Mac build. The slot1 hang
  in solo is still not looked into.

## Built: step P7, Exit Game in player 1's Tome (2026-11-17)

docs/research/solo-coop-picker/next-requests.md, P7. The user: player 1 needs an exit game button in the Tome's Options.
- **Where.** A fifth plaque, "Exit Game", on the Tome's own Options page, alone under Settings and Mods (ring x 292,
  y 215, under Load/Save as player 2's single Characters plaque). Only in `kTomeStopsP1` (hle.c): player 2's copy Tome
  keeps its one Characters plaque, and player 1's copy Tome (in player 2's close-up) has no stops, as before. Down from
  Settings or Mods goes to it; Up from it goes to Settings.
- **What it does.** A goes to a new page id, `FRONTEND_PAGE_QUIT` (13, frontend.h), outside the title range so
  `g_on_title` stays false. frontend.cpp opens the title Quit's own ask page on it (`Screen::QuitAsk`) in the Tome look.
  In the Tome the heading is "Exit the game?", the rows "No, go back" ("Back to the Tome.") and "Yes, exit" ("Unsaved
  progress is lost. These settings are saved."). On the title the words are as before. No (and B) close the page,
  back on the Tome with the ring on Exit Game. Yes does what the hidden in-game launcher's Quit does: `settings_save()`,
  `dol_aurora_request_quit()`, close the page. The program then leaves through the usual way out (the main loop's
  "window closed", the atexit waits from F3).
- **The plaque's picture.** `PLAQUE_COUNT` 5 (plaques.h), "Exit Game" in plaques.cpp `kWords`. Five 280x64 stubs do not
  fit the old bank space (+0x1000..+0xA000), so the bank now runs to `SCRATCH_MENU + 0xC000` and `TITLE_QUAD_COPIES`
  moved from +0xA000 to +0xC000 (to +0xCD80; written on every title Options draw, so a state with the old copies is
  fine). The bank's canary has the count in it, so a state with the old 4-stub bank gets the new one written.
  +0xCD80..+0x10000 is still free.
- **Seen in pictures** (Screenshots for Claude/solo-coop-picker/P7-exit-game/, fresh Jump to Game runs, 16:9 1280x720):
  - Solo (-202123-jtg-p7-solo-exit): the plaque lit (01, zoom 06), the ask with No lit (02) and Yes lit (03), No back
    on the Tome (04), A again and Yes (05): log "[frontend] Quit game from the Tome", "[run] stopped: window closed",
    "[gx-core] shutdown", "game closed". The run's settings.ini was written at the quit. No hang report.
  - Co-op, both Tomes open (-202338-jtg-p7-coop-tomes): player 1's half has Exit Game, player 2's Options shows only
    Characters after three Downs (07); the ask in player 1's half with player 2's dimmed (08, 09); No back on both
    Tomes (10); both closed and walked after (11; timeline P1 and P2 walk 2900-2990).
  - Co-op Yes (-202535-jtg-p7-coop-yes): "Quit game from the Tome", "window closed", "game closed", no hang report.
  - The title's Quit ask is unchanged ("Quit the game?", "Yes, quit", the old note; -202659-jtg-p7-title-quit-check, 12).
- **Not seen:** a real B, Esc or mouse on the ask (test keys only); 4:3 and 21:9; other languages (the plaques fall
  back to text labels when the Options picture isn't 432x480; that path was not run with five); player 2 holding the
  real Tome (`g_coop_tome_owner == 1`, the P2 stops by `tome_stops_player`, from the code).
- **Left as it was:** the Tome's Settings page still has its own "Quit game" row (no ask) and "Return to title screen".
  The user may want that row gone now, or asking too.

## Built: the cards' explainer lines removed (2026-11-17)

- The user: the gold lines "The same as player 1" and "Two players, the screen split" don't make sense; the form text
  stays. `pick_line` now gives only a form name (none for Default and Random); the Co-op card has no line; Player II's
  "Plays with a controller / the keyboard" is gone too. The white "A - ..." line is kept.
- Seen: logs/run-20261117-203856-jtg-goldless-pick2 (ED_TITLE_PAGE_TEST=700:12:0, Karim Spirit and Lindsey):
  Screenshots for Claude/solo-coop-picker/P7b-no-gold-lines/01-start-game-cards-form-only.jpg. The Controls picker and
  player 2 on Default: from the code only (same function).
