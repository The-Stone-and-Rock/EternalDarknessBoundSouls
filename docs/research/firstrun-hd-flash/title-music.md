# The title's own music behind the title-look pages

The user's words (2026-11-17): "The main launcher of the game music is fine to keep. The music for the title page of
the game itself needs to play when character page is open while in game menu, yes. We want it to appear as if it's
native to the game menus."

So: while a title-look page is up, the title's own music keeps playing, as it does on the game's own menus. The title-look
pages are Choose Character (from the main menu) and Display, Sound, Graphics, Controls, Mods and Quit (from the title's
Options list). These are native menus steps 2-4 (docs/coop-next-session.md). The launcher and its launcher.wav stay as
they are.

Read only. Nothing was built, run or changed for this page. Tags: [read] means seen at the file:line, guest address or log
line given. [inferred] means it follows from what was read, but no run has shown it. Line numbers are as of commit
870f12f. hle.c is being edited by another workflow right now, so find code by its function name.

## 1. What stops the music today

### 1.1 The page holds the game

1. The game asks for the page on the game thread.
   - Choose Character: `title_main_choose` calls `frontend_open_page_skin(FRONTEND_PAGE_TITLE_CHARACTERS, 0,
     FRONTEND_SKIN_TITLE, -1)` [read hle.c:17281-17288].
   - Options rows 8-13: `title_more_choose` opens page ids 6-11 [read hle.c:17330-17340].
   - `frontend_open_page_skin` only stores the page id in `g_open_page` [read, per investigator 1, frontend.cpp:4801-4806].
2. The page opens in the frame hook, on the render thread.
   - The hook is `hle_frame_boundary` [read main.c:903]. The render thread calls it after each present
     [read aurora_backend.cpp:2324-2331].
   - It calls `frontend_frame` [read hle.c:5511-5513]. That takes the page (frontend.cpp:4934), sets it up, then calls
     `report_pause(true)`, `hle_coop_ui_over(true, ...)` and `run_menu()` [read frontend.cpp:5068-5074].
3. `run_menu` is a loop that ends only when the page closes [read frontend.cpp:3525-3683].
   - Each turn it presents by itself with `dol_aurora_ui_frame()` [read aurora_backend.cpp:3057-3062].
   - After the close it waits until every button is let go [read frontend.cpp:3677-3681].
4. While it loops, `g_presents_done` is not set for that present [read aurora_backend.cpp:2333-2336].
   - The game thread's next `present()` waits for it [read aurora_backend.cpp:1635-1641].
   - So the game thread stops inside the VIFlush notify, `notify_VIFlush` → `dol_platform_present` [read hle.c:3794-3801].
5. `report_pause` stops nothing. It only tells the hang watchdog (report.c:420-424, per investigator 1).

### 1.2 Why that silences the title's music

- The title's music is MusyX, played from note data by the guest [read tools/launcher_music.py:3-5]. It is not the disc
  stream (DTK): in the fresh-map run the last `[dtk]` line is a cancel at frame 200, long before the title
  [read, per both investigators].
- MusyX runs in the guest's audio DMA interrupt.
  - `salCallback` 0x801C207C → `snd_handle_irq` 0x801C06B0 → `salCtrlDsp` and `synthHandle` (call at 0x801C07C0)
    [read, per investigators; generated/sdk_symbols.inc].
- That interrupt, and the host's `audio_poll` that pushes each chunk, both come from `interrupt_poll`, once per guest block
  [read main.c:397; interrupt.c:424-538].
- A held game thread runs no blocks. So no chunks are made and nothing is pushed to SDL [read audio.c:243-248].
- SDL plays out what was queued (about 40 ms plus the reserve), then silence [inferred from aurora_backend.cpp:196-200].

### 1.3 What the log shows

| Run | Lines |
|---|---|
| logs/run-20261117-090616-jtg-s4fix-fresh-map/page-0001.log | :85 "[title] Choose Character (frame 821)"; :87 "page 5 opened"; :90 "[audio] ran dry after a 1946 ms pause; keeping 100 ms in reserve"; :91 "the page closed ... (frame 827)"; :92 "[fps] 9.9" |
| same log | :131 "[audio] ran dry after a 15516 ms pause; keeping 120 ms in reserve"; :132 "[fps] 1.9" |

- Choose Character was up for about 2 s, but only 6 guest frames went by (821 → 827).
  - The wall-clock step is capped at 100 ms after a stall [per investigator 1: interrupt.c:70, :446-447].
- "Ran dry" is printed by the first push after the page closes, not when it opens [read aurora_backend.cpp:4400-4413].
- Each "ran dry" adds 20 ms to the sound's reserve, up to +120 ms, for the rest of the session [read
  aurora_backend.cpp:4405-4406]. The log climbs 100 → 120 ms.
- On close the song goes on from the note where it stopped [inferred: nothing on the page path touches the sound].

## 2. What the game does on these screens

- **Main menu (state 7), where Choose Character opens: the idle clock.** [read build/ed-db/disasm.txt 0x8001C55C-0x8001C64C]
  - If TM+48 (`TITLE_FLAGS`, 0x80309154) bit 0x2 is clear, the game sets it and calls `OSStartStopwatch` (0x802034CC),
    then 0x80203644 (0x8001C568-0x8001C57C).
  - 0x80203644 is the stopwatch reset [inferred: it is the call between the SDK's `OSCheckStopwatch` 0x802035D4 and
    `__OSThreadInit` 0x802036F0, and the press path calls Stop, 0x80203644, Start at 0x8001C5CC-0x8001C5E0].
  - Each frame `OSCheckStopwatch` (0x8001C584). Any press (`0x8013DC24(-1,0,0)` at 0x8001C5C0) restarts it.
  - At the limit at r2-32420 (180 s, per 02-title-menu.md:225-228) it goes to state 23, the attract movie
    (0x8001C5F4-0x8001C600).
  - So clearing bit 0x2 each frame keeps the clock at zero [inferred from the code above].
- **Options (states 14-17) has no idle clock** (docs/research/in-game-menu/02-title-menu.md:229).
- **Options has its own sound 723**, started on entering Options and stopped on going back to the main menu
  (0x8001A610-0x8001A620, 0x8001FD28-0x8001FD38, 0x8001FA90) [read, per investigator 2]. It is probably the Options song
  [inferred: launcher.wav is "the song of its Options screen", frontend.cpp:3481-3485].
- So Choose Character should hear the main menu's music, and the Options rows the Options song. Letting the game's own
  sound system run gives both, with nothing chosen by the host.

## 3. The fix chosen: the title keeps running behind its pages

For title-look pages only (`Skin::Title`), the page no longer holds the game. The page runs one step per game frame from
the frame hook. The game runs under it as if the player were sitting on its menu.

### Why this one

- **It is the only way that is the game's own sound, whole.** The guest's interrupt, its sound tick 0x8019DC50, its
  fades, the 723 song and the main menu's music all run as on the real menu. Nothing on the host picks a song.
- **The picture stays as it is.** RmlUi is drawn in every present already [read aurora.cpp:675-681; rmlui.cpp:438-470],
  over the whole window [read aurora.cpp:665-673]. The title look's body is opaque black [read res/ui/menu.rcss:978-982],
  with the Options picture on it. So the moving title is hidden, and the page looks as it does today [inferred].
- **No renderer change.** Only frontend.cpp and hle.c.
- **There is a model for it.** Photo mode already returns to the game between its frames (`Exit::Step`,
  frontend.cpp:4782-4785) [read].
- It also stops the "ran dry" lines and the growing reserve after title pages [inferred].

### The others, and why not

| Way | Why not |
|---|---|
| Keep the game held, but park it at VIFlush with interrupts on (investigator 2's b3) | Unknowns on the guest side: MSR.EE at VIFlush not checked; threads below the main thread's priority starve; the game's sound tick 0x8019DC50 stops, so fades and stream chains stop. Keep it on file for the Tome pages. |
| Run the guest's audio interrupt from inside `present()`'s wait (b1) | Nested exception entry from a held host call. The riskiest. |
| Play res/music/launcher.wav during title pages (c1) | Not native: the song jumps to the loop start; it is the Options song, wrong on the main menu; missing on a fresh install (firstrun.md:23). The user wants the game's own music. |
| Host-only disc music behind a held page (c2) | Does nothing for the title: its music is MusyX, not DTK. |

### What stays as it is

- The launcher, the Esc debug menu, `frontend_problem` and the HDR calibration keep the blocking `run_menu`.
- **Tome pages stay paused** (Way B, docs/research/in-game-menu/12-critic-and-plan.md:91). They also cut the room's disc
  music today ("ran dry after a 5041 ms pause", logs/run-20261117-101118-jtg-s6-p1-settings/page-0001.log:1792).
  The user asked only about the title. Ask before changing them; (c2) is the safest way there.

## 4. Build steps

### Step 1. Split `run_menu` into begin, step and end (frontend.cpp; no change in behaviour)

- `menu_begin()`: today's head of `run_menu` (frontend.cpp:3526-3532): `g_close = false`, `build_rows()`,
  `g_doc->Show(...)`, and the first `g_input.poll` that ignores what is held. The `pressed` array moves to a
  namespace-level static, so the step can use it.
- `enum class MenuStep { Open, Closed }` and `menu_step()`: one turn of the loop body (frontend.cpp:3535-3669), without its
  own `dol_aurora_ui_frame()`.
  - Each `break` on `g_close` becomes `return MenuStep::Closed`.
  - The key-capture branch (:3553-3567) presents and `continue`s today. It becomes `return MenuStep::Open` after its
    `build_rows()`.
  - The test close (`g_test_close_at`, :3640-3651), the test keys (:3569-3590), the place on aspect change (:3539-3552)
    and the Controls refresh (:3661-3667) go into the step unchanged.
- `run_menu()` becomes: `menu_begin()`; loop `menu_step()` then `dol_aurora_ui_frame()` until Closed or the window
  closed; `g_doc->Hide()`; today's release wait (:3677-3681).
- Move frontend_frame's tidy-up after the page (frontend.cpp:5076-5089, from `hle_coop_ui_over(false, -1)` to
  `g_menu_closed++`) into one function, `page_closed()`. Both ways call it.

### Step 2. The live title page (frontend.cpp `frontend_frame`)

- A state: `enum class Live { Off, Open, Release }` and `static Live g_live`. Add `extern "C" bool
  frontend_page_live(void)` (an atomic copy; any thread) to frontend.h.
- At the end of the page set-up in `frontend_frame` (after the `switch (page)`, before `report_pause(true)` at :5068):
  if `g_skin == Skin::Title`, then
  - `g_skin_split = g_settings.mod_split;` and `hle_coop_ui_over(true, -1)` as today (on the title it does nothing:
    hle.c:13906 needs a co-op Tome);
  - `menu_begin()`, `g_live = Live::Open`, log `"[frontend] page %d live: the title runs under it (%llu ms)"` with
    `SDL_GetTicks()`;
  - **no** `report_pause`: frames keep coming, so the watchdog stays happy;
  - return.
- Near the top of `frontend_frame`, after `update_start_up_cover`, `update_fps_counter`, `update_skip_prompt` and
  `update_cursor`, and **before** `read_buttons` and the Back/Esc/photo checks (:4877-4910):
  - `Live::Open`: `if (menu_step() == MenuStep::Closed) { g_doc->Hide(); g_live = Live::Release; }` then return.
  - `Live::Release`: `g_input.poll(pressed)`; if `!g_input.any_held()`, call `page_closed()`, set `g_live = Live::Off`,
    log `"[frontend] the live page closed (%llu ms)"`. Return either way.
  - Returning before `read_buttons` keeps a Back held on the page from being read as a tap that opens the debug menu
    [inferred].
- The page is drawn by the game's own presents from now on. It shows one frame after each step (about 16 ms)
  [inferred: the hook runs after `aurora_backend_present`, aurora_backend.cpp:2324-2331].
- A window closed with the page up: the game's run loop already stops on its own ("window closed", main.c:912). The step
  needs no quit check [inferred].
- A test switch to keep today's way, for A/B runs: `ED_TITLE_PAGE_PAUSE=1` makes title pages block as now.

### Step 3. Hold the guest's pad while the page is up (hle.c `hle_PADRead`)

- In `title_main_choose` and `title_more_choose`, where they call `frontend_open_page_skin`, set
  `g_title_page_hold = true` and `g_title_page_hold_count = frontend_menu_closed_count()`. This is on the game thread, so
  the very next read is held, before the render thread has even opened the page.
- In `hle_PADRead`, just before each of its two write-out loops (the player-2 branch at about hle.c:4344, and the main
  one after `skip_card_select` at about :4533):
  - if `g_title_page_hold` and `frontend_menu_closed_count() == g_title_page_hold_count`: zero `state[0..3]`'s buttons,
    sticks, substicks, triggers and analog values. Keep each `error` as it is, so no controller notice appears.
  - if the count has moved on: `g_title_page_hold = false`.
  - This also covers the monitor's read (`pad_read_by_monitor`, hle.c:4291), the scripted presses (`input_script_apply`)
    and the test hooks, which all write `state[0]` before that point [read hle.c:4473-4500].
  - The same zeroing is done today for player 2's Tome pad [read hle.c:4330-4336].
- The count moves on only after the release wait (step 2), so the B or A that closed the page never reaches the game.
- If the page cannot open (`ensure_ui` fails), the count moves on at once [read frontend.cpp:4941-4944], so the hold ends.
- Logs, once each: `"[title] the pad is the page's (frame N)"` and `"[title] the pad is the game's again (frame N)"`.

### Step 4. Keep the idle clock at zero while Choose Character is up (hle.c `title_main_draw`)

- Today the bit is cleared once, after the close [read hle.c:17295-17300].
- Change it: while `g_title_idle_reset` is set (from `title_main_choose` until the close), clear TM+48 bit 0x2 on every
  draw. The game's own code at 0x8001C568 then starts and resets its stopwatch each frame. Keep the one log line at the
  close.
- Fix the wrong comment at hle.c:17258 ("minutes of guest time": today a page adds only about 100 ms of guest time).
- Options pages need nothing: no idle clock there.

### Step 5. No quick save or load while a live page is up (quickstate.c)

- Keys 1 and 2 typed on a page (in the Controls page's key capture, too) would now reach `quickstate_poll` on the running
  game [inferred: today the game is held then].
- In `quickstate_poll`, ignore the keys while `frontend_page_live()` is true, or while hle's `g_title_page_hold` is set.

### Step 6. The test recipes

- Guest frames now pass while a page is up. Presses in `ED_INPUT_SCRIPT` (frame = timebase / 675000) that fall inside a
  page are held (step 3), and later ones land on other screens.
- Space the presses after a page by its open time (60 frames a second), and raise `ED_RUN_FRAMES` to match.
- Add a note to the press list at the top of tools/ed/jtg.sh (:18-30). The jtg chapter runs themselves open no page, so
  they are not changed.

## 5. Risks

1. **The attract movie behind Choose Character.** Without step 4, three minutes on the page plays the movie under it,
   and its sound replaces the music [read: 0x8001C5E4-0x8001C600]. Step 4 is needed before step 2 is used.
2. **A press reaching the game.** The A that opened the page, presses on it, and the B that closed it. Step 3 holds all of
   them from the request until release. A key bound to the guest's pad but not to the page's input would still be held
   at close and seen as held, not as a press [inferred]. The game reacts to new presses, so it should do nothing.
3. **Page code now runs while the game thread runs.** Today the game is held, apart from building one frame ahead.
   - Title pages write `g_settings`, which the game thread reads (audio reads its volumes each chunk) [read audio.c:193-208].
     These are single ints [inferred: harmless].
   - The frontend's calls into hle.c (`hle_cheat_set`, `hle_sanity_*`, `hle_set_skip_intro`, `hle_player_place`) are on the
     launcher's and in-game paths. The Mods page's sanity rows need `in_play()` (frontend.cpp:1317, per investigator 2), so
     they are off on the title [inferred: check the Mods page in a run].
   - The frame hook already reads buttons each frame while the game reads its pad [read frontend.cpp:4876-4877], so
     reading input from both threads is not new.
4. **Card notices.** One could come up under the opaque page and wait for a press. It would be seen after the close
   [inferred].
5. **The title video** keeps decoding under the page. It is silent (titlevideo.cpp:5-6). CPU only [inferred].
6. **Photo mode before the page is taken.** `frontend_frame` returns early in photo mode (:4904-4908), before it takes the
   page. The pad stays held until photo mode ends and the page opens and closes [inferred: harmless].
7. **The Music slider does not move the title's song.** "Music volume" scales only the disc stream; MusyX follows
   "Effects volume" [read audio.c:193-208]. This is true today too, but now the player hears it on the Sound page. Tell
   the user; do not change it unasked.
8. **Tests that time presses by frame** shift (step 6).

## 6. How to check each step

Fresh title runs only, from a cold boot, the way the fresh-map run was made: `tools/ed/jtg.sh` with `ED_RUN_AWAY=1`
(the window off screen, the sound muted: ED_MUTE still sends the silent chunks, so "ran dry" lines still print,
audio.c:241-248). Do not use `ed test` for these checks: its save-state runs never pass through the title.

The base switches, as in logs/run-20261117-090616-jtg-s4fix-fresh-map/env.txt: `ED_RUN_AWAY=1 ED_RUN_SHOTS_MS=250
ED_RUN_SHOTS_W=1280 JTG_TITLE_AT=90000 ED_PAGE_TEST_STEP_MS=900`. Add `ED_AUDIO_LOG=1` and
`ED_PCM_OUT=<scratch>/NAME.pcm` to each run. The PCM is written before the mute [read audio.c:226-243]: 32 kHz, s16
stereo, 128,000 bytes a second.

A small reader for the PCM (put it in the scratchpad, run with `python -I`): RMS of each 100 ms; print the windows under
a floor (say 30) between two frame numbers given, using about 533 samples a frame (32000 / 60)
[inferred: the guest's sound follows guest time].

| Step | Run | What shows it worked |
|---|---|---|
| 1 | Today's fresh-map recipe unchanged (Down 700, Down 760, A 820; `ED_PAGE_TEST_KEYS="r,b\|u,u,a,d,d,d,d,d,d,d,d,d,d,d,d,b,b"`, `ED_PAGE_TEST_MS=60000`, the env.txt presses) | The same lines as run-20261117-090616 (page-0001.log:85-91 and :120-132): "page 5 opened", the test keys on the same screens and rows, and still "ran dry" (the page still blocks). No hang-*.txt in saves/reports. |
| 2+3+4 | Choose Character, live: `ED_INPUT_SCRIPT="700,706,0x0004;760,766,0x0004;820,826,0x0100;880,886,0x0004;1100,1106,0x0004"`, `ED_PAGE_TEST_KEYS="r,b"`, `ED_RUN_FRAMES=1500` | "[frontend] page 5 live" and "the live page closed", about 1.9 s apart. "[title] the page closed (frame G)" with G about 821 + 115, not 827. No "[audio] ran dry" after it. "[fps]" stays about 58, no "9.9". "[audio] AID chunk N" lines keep coming between open and close. The PCM has no quiet window from frame 821 to G (the main menu's music, at its level before the page). The Down at 880 is held: "[title] the pad is the page's" before it, and the shots after the close show the main menu still on Choose Character; the Down at 1100 then moves to Options. The page's shots look like today's (opaque title look). |
| 4 | Idle: as above, but no `ED_PAGE_TEST_KEYS`, `ED_PAGE_TEST_MS=200000`, `ED_RUN_SECONDS=300`, `ED_RUN_FRAMES=16000` | No movie lines ("Before CleanHVQ", "[controls] cutscene started") during the 200 s. The music is in the PCM for all 200 s. The shots after the close show the main menu, not the movie. Then, with step 4 left out on purpose (a local test build only), the movie starts at about 180 s, which shows the check can fail. |
| 2+3 | Options pages, live: Down x3 at 700, 760, 820 (Start Game → Load Game → Choose Character → Options), A at 880, then Up x5 at 1000-1240, 60 apart (row 0 → 13 Quit → 12 → 11 → 10 → 9 Sound), A at 1300; `ED_PAGE_TEST_MS=8000`, `ED_RUN_FRAMES=2400` | "[title] Options row 9, Sound", "page 7 live", closed about 8 s later. The PCM has the Options song across the 8 s, no hole and no jump at open or close. Then the Options list in the shots, on the Sound row, not the main menu (the closing B was held). Repeat with Display, Graphics, Controls (keys "d,d,b": "[frontend] test key" lines act on the page while the game's cursor does not move), Mods and Quit (Quit: "No" closes it). |
| 2 | Aspects: the Choose Character run at 4:3, 16:9 and 21:9 (through `JTG_SET`, as env.txt sets `controls_device`; the aspect's key name in settings.ini is not checked here) | The page's shots match today's at each aspect; nothing of the title moves through the page. |
| 5 | Choose Character live with a key 2 press in the page (an `ED_PAGE_TEST_KEYS` key or a script key, if the test keys can send it) | No "[quick]" save line while the page is up. |
| all | Every run above | No saves/reports/hang-*.txt. `ed timeline` and EVENTS.log read first, as CLAUDE.md asks. |

Then the user listens: Choose Character from the main menu, and each Options row, with the sound on. The music should go
on under the page with no break and no jump, as on the game's own menus.

## 7. Not done here, for later asks

- Tome pages: keep their pause, but give back the room's disc music behind them (way c2, host-only DTK mix while the game
  is held). Ask the user first.
- The game's own cursor and choose sounds on our pages' moves, to sound even more native. Not asked.
- The Music slider and the title's MusyX song (risk 7).

## Files

- E:\Eternal Darkness Recomp Fork\runtime\host\frontend.cpp (`LauncherMusic` :3481-3523, `run_menu` :3525-3683,
  `frontend_frame` :4855-5089)
- E:\Eternal Darkness Recomp Fork\runtime\host\frontend.h (:47-75)
- E:\Eternal Darkness Recomp Fork\runtime\host\hle.c (`pad_read_by_monitor` :4291, `hle_PADRead` :4301-4540,
  `hle_frame_boundary` :5471-5514, `hle_coop_ui_over` :13905-13932, `title_main_choose`, `title_main_draw`,
  `title_more_choose` :17258-17340)
- E:\Eternal Darkness Recomp Fork\runtime\host\audio.c (:186-248)
- E:\Eternal Darkness Recomp Fork\runtime\host\main.c (:397, :903-905)
- E:\Eternal Darkness Recomp Fork\runtime\host\quickstate.c (`quickstate_poll`, `quickstate_service` :824-836)
- E:\Eternal Darkness Recomp Fork\.deps\GXRuntime\backends\aurora\aurora_backend.cpp (:1617-1642, :2315-2336,
  :3057-3066, :4400-4413)
- E:\Eternal Darkness Recomp Fork\.deps\GXRuntime\graphics\aurora\lib\aurora.cpp (:665-681)
- E:\Eternal Darkness Recomp Fork\.deps\GXRuntime\graphics\aurora\lib\rmlui.cpp (:438-470)
- E:\Eternal Darkness Recomp Fork\res\ui\menu.rcss (:972-982)
- E:\Eternal Darkness Recomp Fork\build\ed-db\disasm.txt (0x8001C55C-0x8001C61C)
- E:\Eternal Darkness Recomp Fork\generated\symbols.tsv (:6197-6200, the stopwatch calls)
- E:\Eternal Darkness Recomp Fork\logs\run-20261117-090616-jtg-s4fix-fresh-map\ (page-0001.log :80-132, env.txt)
- E:\Eternal Darkness Recomp Fork\logs\run-20261117-101118-jtg-s6-p1-settings\page-0001.log (:1792)
- E:\Eternal Darkness Recomp Fork\tools\ed\jtg.sh (:1-30, :171-176)
- E:\Eternal Darkness Recomp Fork\tools\launcher_music.py (:1-20)

## Skeptic's check

Read only, like the rest of this page. Nothing built or run. Line numbers are as read on 2026-11-17 (hle.c had moved
about 2 lines since 870f12f: `title_main_choose` is now hle.c:17283).

### Confirmed

- **The page holds the game, and how.** `run_menu` loops until `g_close` and presents by itself
  (frontend.cpp:3525-3683; `dol_aurora_ui_frame` = `aurora_backend_present`, aurora_backend.cpp:3057-3062). The frame hook
  runs inside the Present item, before `g_presents_done = item.n` (aurora_backend.cpp:2321-2336). `present()` waits for
  `g_presents_done + 1 >= n` (aurora_backend.cpp:1635-1640). So the game is at most one frame ahead, then held. [read]
- **`report_pause` only feeds the watchdog** (report.c:420-424, :466). [read]
- **Audio needs guest blocks.** `audio_poll` runs from the block loop (interrupt.c:426, :470; main.c:397). A chunk is
  pushed only from there (audio.c:243-248). "Ran dry" is printed by the next push and adds 20 ms, up to +120 ms
  (aurora_backend.cpp:198-200, :4403-4412). [read]
- **The log.** page-0001.log:85-92 and :131-132 read as the doc says. The Choose Character page was closed by the test
  key b (:89 "test key b on screen 14 row 1"), not by `ED_PAGE_TEST_MS`. [read]
- **0x80203644 is the stopwatch reset. Upgrade from [inferred] to [read].** Its body (disasm 0x80203644-0x80203668) is
  `OSInitStopwatch` 0x802034A0 without the name store: it zeroes total, hits, min and max, and leaves running and last
  alone. So Start then Reset (0x8001C574, 0x8001C57C) leaves the watch running from now with total 0. Clearing bit 0x2
  each draw does keep `OSCheckStopwatch` (0x8001C584) near 0. The 180 s limit and state 23 are at 0x8001C5E4-0x8001C600.
- **Sound 723 on Options** is started at 0x8001A610 and 0x8001FD28 into r13-26144, and that handle is stopped at
  0x8001FA90 (the way back to state 7) and at 0x8001C650 (the attract movie). [read]
- **The picture stays opaque.** `body.skin-title` (menu.rcss:978-982, black) comes after `body.overlay` (menu.rcss:27,
  `#000000b0`) with the same weight, so it wins even where `overlay` is set (frontend.cpp:3105-3114). RmlUi is recorded in
  every present over the whole window (aurora.cpp:665-681; rmlui.cpp:438-470 calls `Context::Update` and `Render`).
  The title video only replaces the game's own texture (titlevideo.cpp:1-7), so it is under RmlUi. [read]
- **Pad hold.** In `hle_PADRead` every host input that reaches the guest is put in `state[]` before the two write-out
  loops (p2 branch hle.c:4344-4357; main loop after `skip_card_select`, hle.c:4531-4545). The p2 Tome zeroing that keeps
  `error` is at hle.c:4330-4336. `coop_character_load` needs a player in play (`coop_player(cpu) != 0`, hle.c:~7405), so
  it does not fire on the title. [read]
- **`hle_coop_ui_over(true, -1)` does nothing on the title** (needs `g_tw.sharing`, a copy Tome or a Tome owner,
  hle.c:13906). [read]
- **The Music slider point (risk 7).** MusyX is scaled by `effects_volume` (audio.c:193-196); `music_volume` only scales
  DTK (audio.c:206-207). [read]
- **Quit's Yes** calls `settings_save()`, `dol_aurora_request_quit()` and sets `g_close` (frontend.cpp:2640-2644). In the
  live way the step returns Closed and the run loop stops on `g_should_quit`. Fine. [read]
- **The step-6 press counts add up.** Down x3 from row 0 is Load, Choose Character, Options (`title_main_updown` order
  {0, 1, 5, 2, ...}, hle.c:17266-17281). Up x5 from Options row 0 is 13, 12, 11, 10, 9 (Sound). The movie log strings
  exist ("[controls] cutscene started", controls.cpp:1423; "Before CleanHVQ" is the game's own, seen in
  logs/run-20261104-151232-jump-title3/page-0001.log:48). [read]
- **The fix shape is right.** Photo mode already returns to the game between steps (`Exit::Step`,
  frontend.cpp:4686-4691, :4782-4785). Nothing on the frontend's title pages writes guest memory: `hle_cheat_set` and
  `hle_set_skip_intro` only set host flags (hle.c:26253-26256, :219-225). [read]

### Wrong, with corrections

1. **`page_closed()` must not hold `report_pause(false)`.** The doc moves frontend.cpp:5076-5089 into it, and that range
   has `report_pause(false)` at :5077. The live way never calls `report_pause(true)`. So after one live title page
   `g_pauses` is -1 (report.c:421). The next blocking page (a Tome page, the Esc debug menu) then counts 0, not 1. The
   watchdog skips only while `g_pauses > 0` (report.c:466), and the hook does not run while `run_menu` blocks. So after
   6 s (`g_hang_ms`, report.c:408) on any later blocking page: a false hang-*.txt and the "Not responding" page.
   Correction: keep `report_pause(true)` and `report_pause(false)` in the blocking way only, outside `page_closed()`.
   Add to step 6: open a Tome page for over 6 s after a live title page; no hang-*.txt.
2. **`menu_was_held` cannot move into `page_closed()` as is.** It is a function-local static of `frontend_frame`
   (frontend.cpp:4879). Make it namespace-level, or reset it at both call sites.
3. **Step 5 covers too few keys.** `quickstate_poll` also acts on F1-F10 (save to slot), F11 (load newest) and F12
   (record) (quickstate.c:55-64, :304-330), not only 1 and 2. Skip all of them while a live page is up. Keep updating
   the `*_key_down` states while skipping, so a key held at the close does not fire as a new press after it.
4. **Small: line numbers.** The hle.c ones have moved (`title_main_choose` :17283, `title_main_draw` :17292, the
   "minutes of guest time" comment :17260, `input_script_apply` :4475). The doc already says to find code by name.

### Missed, and what it changes

1. **The main menu's own song is never shown.** The title's one sound handle, r13-26144, holds only 723 (Options) or
   722 (Jump to Game: 0x8001A668, 0x8001AFF0) [read]. TM+52 and +56 are the whispered row names
   (02-title-menu.md:46), and the main menu's A stops both (0x8001A464-0x8001A49C) [read]. No read here shows a song
   that plays on the main menu itself, and no PCM of the main menu exists in logs/. What it changes:
   - The fix still gives whatever the game plays, so it stays the right one.
   - The step 2+3+4 PCM check ("no quiet window from frame 821") must first measure the main menu before the page
     (say frames 700-820) and compare against that level. If the main menu is quiet apart from the whispers, the check
     as written fails on a correct build.
   - Tell the user what the game itself plays on the main menu, once the PCM shows it.
2. **A real hang under a live page now opens the problem page in the middle of the live state.** With no
   `report_pause`, a game stall over 6 s while a title page is up goes to `report_idle`, then `frontend_problem`, then a
   nested `run_menu` on the render thread (report.c:466-485, :508-516; aurora_backend.cpp:1998-1999). That saves and
   restores only `g_screen` and `g_selected` (frontend.cpp:5091-5106). It leaves the problem rows built and may leave
   `g_close` set, so the live page closes at its next step or shows stale rows [inferred]. Rare, but add to step 2:
   when `frontend_problem` returns while `g_live == Live::Open`, set `g_close = false` and call `build_rows()`.
3. **The page is only as quick as the game's frames.** In the live way the page steps once per game present. Any stall
   of the game thread (a first-run pipeline build, a disc read) now also freezes the page's input and drawing; today the
   page presents by itself [inferred from aurora_backend.cpp:2321-2331]. Worth one look in a fresh-install run.
4. **`both_record` sees the real pad, not the held one.** It runs at hle.c:~4413, before the zeroing. both.tsv will show
   presses on the title that the game never got [read for the order; inferred for the effect]. Note it in step 3, or do
   the zeroing before `coop_watch` and `both_record` when the hold is on.
5. **The Sound page becomes live.** With the title running, the Effects slider changes the title's song as it moves
   (audio.c:193-196). That is native and good, but name it to the user with risk 7.

### Verdict

The plan holds. The diagnosis (the blocking `run_menu` holds the game thread at its next `present()`, so no guest blocks,
no MusyX, no chunks) is right in every link checked. Letting the game run under title-look pages is the right fix for "as
if native". The pad hold and the idle-clock reset are correct and needed. Fix before building: keep `report_pause` out of
`page_closed()` (Wrong 1: a false hang after any later Tome or debug page), make `menu_was_held` reachable (Wrong 2),
and widen step 5 to every quick-state key (Wrong 3). Change the PCM check to compare against the main menu's own level
first (Missed 1), and add the `frontend_problem` re-entry guard (Missed 2).

## Built: step F5 (2026-11-17, not committed)

The plan above, with the Skeptic's corrections. The title runs under its title-look pages (Choose Character from the
main menu; Display, Sound, Graphics, Controls, Mods and Quit from the title's Options list). The Tome pages, the Esc
debug menu, the launcher, the problem page and the HDR calibration still hold the game, as before.

What was built:
- **frontend.cpp.** `run_menu` is split into `menu_begin` and `menu_step` (no change for the blocking pages).
  `frontend_frame` opens a title-look page live (`Live::Open`): one `menu_step` a game frame, before the Back / Esc /
  photo reads. The game's own presents draw it. After the close (`Live::Release`) it waits for every button to be let
  go, then `page_closed()` moves the close count on. `report_pause` stays with the blocking way only (Wrong 1).
  `menu_was_held` is now `g_menu_was_held` (Wrong 2). `frontend_problem` over a live page puts the page back (Missed 2).
  An HDR calibration opened from a live Graphics page tells the watchdog it holds the game.
  `ED_TITLE_PAGE_PAUSE=1` makes title pages block as before (A/B).
- **hle.c.** `title_page_hold_start` (in `title_main_choose` and `title_more_choose`) and `title_page_held` in
  `hle_PADRead`: every pad's buttons, sticks, triggers and analog values are zeroed (each `error` kept) until the close
  count moves on. Fast-forward does not apply under a title page. `title_main_draw` clears TM+48 bit 0x2 on every
  draw while Choose Character is up, so the game's own idle clock stays at zero. both.tsv and coop_watch still see the
  real pad (Missed 4: they read it before the hold).
- **quickstate.c.** No quick-state key (1, 2, F1-F11, F12) acts while a live page is up; the key states are kept.
- **Found in the runs, and fixed: two stalls at a page's open, which the blocking pages had hidden.**
  - The title Options pages asked the renderer to measure the screen's rate again (`dol_aurora_measure_screen`). That
    holds one frame for about 190 ms: "[audio] ran dry after a 186 ms pause" at every open of Display, Sound and
    Graphics. Over a live title page this is no longer asked. The measure from the start is kept, and the renderer
    measures again by itself when the window moves to another screen.
  - The first page of a run wrote the clean picture (about 25 ms), loaded the HD backdrop into RmlUi (about 30 ms), and
    laid the menu out for the first time (about 38 ms with no launcher). `warm_title_look` does each of these ahead,
    one per frame: the layout at the start (shown and hidden in one call, so nothing is drawn), the pictures once the
    title's Options picture is kept. RmlUi is only asked for a picture that is already there.
  - Log lines: "[frontend] page N live: the title runs under it (... set up in X ms)", "[frontend] the live page's
    first 60 frames: slowest step ..., longest frame ...", "[frontend] the live page closed",
    "[title] the pad is the page's / the game's again", "[frontend] the title look ahead of its first page: ...".
- tools/ed/jtg.sh has a note: script presses that fall inside a title page are now lost (step 6).

What the game itself plays (Missed 1, now measured):
- **The main menu has no song.** A PCM of the main menu left alone for 200 s (run -152346-jtg-f5-idle-nopage) is
  silent after the title's opening sound, apart from the whisper of each row moved to and the sound of A. Only 10 of
  1,800 windows of 100 ms were above the floor, all in the first 12 s. So behind Choose Character the player hears
  the A's sound fade, then silence, as on the game's own main menu.
- **Options has its song** (sound 723). It plays on under the Sound page: no hole and no restart across the 12 s page
  (run -151023-jtg-f5-main, PCM 35.8-47.8 s; its phrase accent every 2.6 s goes on through the open and the close).

Seen in runs (all fresh title runs, `ED_RUN_AWAY=1`, the user's settings copied):
- -151023-jtg-f5-main: Choose Character up 12 s, then the Sound page 12 s. No "ran dry" from the open of the first page
  to the end of the second. A Down at frame 900 (on Choose Character) and at 2400 (on Sound) were held: the shots after
  the closes show the main menu still on Choose Character and the Options list still on Sound. Then the Esc debug menu
  (ED_MENU_TEST) held the game for 8 s: no hang report (saves/reports stayed at 349 files).
- -151307-jtg-f5-pages: Quit ("No" closed it), Mods, Controls, Graphics and Display, each opened live. Each A landed on
  the expected Options row (13, 12, 11, 10, 8), so no press on a page reached the list. The test keys moved the pages'
  own rows. No "ran dry". Pictures look as before.
- -151443-jtg-f5-idle: Choose Character up 200 s (frames 821 to 12823). No attract movie ("Before CleanHVQ" or
  "[controls] cutscene") while it was up, and the main menu after. The control run with no page,
  -152346-jtg-f5-idle-nopage, started the movie at frame 11562, 180 s after the last press. So the check can fail.
- -152753-jtg-f5-pause-ab: with `ED_TITLE_PAGE_PAUSE=1` the page blocked as before ("ran dry after a 3011 ms pause").
- Pictures: `Screenshots for Claude/f5-title-music/` (01 the main run, 02 the five other pages, 04 the idle run;
  03 is the main run's PCM loudness, 100 ms windows, with the log's marks).

Not seen, or still open:
- The first open of a title page still costs a little more than later ones (the first Show of the title look about
  17 ms; the Sound page's first draw about 55 ms, its fonts and glows). In these runs it stayed inside the sound's
  reserve (no "ran dry"), but it is close. A slower PC could still hear one short click the first time.
- The user's launcher path (the launcher shows the menu first) was not run: `jtg.sh` passes the launcher over.
- A fresh install (the warm-up then waits for the title's first frame for the pictures) was not run.
- A real hang under a live page (the problem page over it) and HDR calibration from a live Graphics page: from the
  code only.
- The Music slider still does not move the title's song: MusyX follows "Effects & voices" (risk 7). Not changed.
- The intro skip of `jtg.sh` missed three times in a row once (-152058/-152113/-152128-introfail, room 96 at frame
  545); the next run was fine. The warm-up runs before frame 68 in all of them, as in the good runs.

## Built: step F5 review fixes (2026-11-17)

Three review findings, checked against the code.
- **Fixed: a quick-state key in the frame before the page opens.** `title_page_hold_start` runs on the game thread,
  but `frontend_page_live()` turns on only when the frame hook (render thread, after the next present) takes the page.
  A load key pressed in that gap still loaded, and the page then opened live over the loaded game. Now hle.c has
  `hle_title_page_hold()` (true from the game's ask until the close count moves on, read on the game thread), and
  `quickstate_poll` blocks on `frontend_page_live() || hle_title_page_hold()`. Not reproduced in a run: scripts cannot
  press keyboard keys. Checked from the code.
- **Rejected: two threads in the page at once under the problem page.** With the GX thread on, the frame hook
  (`hle_frame_boundary`, so `live_page_frame`) and the idle hook (`report_idle`, so `frontend_problem`) both run on the
  render thread (aurora_backend.cpp: `ring_next` calls the idle hook; the `Present` item calls the frame hook). While
  the problem page's `run_menu` loops, that thread drains no ring items, so no game present reaches the frame hook
  until it returns. Without the GX thread there is no idle hook. `g_live` is read and written only on that one thread.
  The restore after `frontend_problem` stays as it is.
- **Accepted, as words: Choose Character is still quiet.** The game's main menu has no song (PCM of run -152346, and
  again in the run below: all zero while Choose Character was up). So F5 gives music under the Options pages only.
  Under Choose Character the player hears the A's sound fade, then silence, as on the game's own main menu. If the user
  wants music there, it has to be a song we choose (for example the Options song 723): ask them first, then build it as
  its own step. F5 does not close that part of the user's request.

Seen (final build, run -154055-jtg-f5-final-main, ED_RUN_AWAY=1, the same presses as -151023):
- Choose Character live 12 s, then the Sound page live 12 s. No "ran dry" while either was up; the only one came after
  the Esc debug menu's 8 s block, as before. No hang report (saves/reports stayed at 349).
- The page's test keys moved its own rows (Player II, then Player I; Sound's rows). The Downs sent while each page was
  up were held: the main menu stayed on Choose Character and the Options list on Sound after the closes.
- The PCM: silence over Choose Character (the main menu's own quiet), then the Options song without a zero window from
  the A on Options to the end, across the Sound page's open and close.
- Pictures: `Screenshots for Claude/f5-title-music/05-final-build-cc-and-sound-pages.jpg`, and
  `06-final-build-pcm-loudness.txt`.
