# HANDOFF 2026-11-15 evening: START HERE (committed through 5dad661 on tools/game-kit: the evening's work in 74a7989 and f8e5d82, the Choose Character button, the first to-do list and the kit prompt in 5dad661; the 2026-11-16 revision of the list and the prompt and kit item 1's first rows are uncommitted; controls.cpp's test edits stay uncommitted by rule)

## THE TO-DO LIST, in order (the user, 2026-11-16)
The kit first, so later work finds each co-op rule before play instead of during it (the user will run a session
dedicated to it; its prompt is docs/prompts/session-kit-mapping.md, revised 2026-11-16 after a read-only check of the
plan against the code, every branch and stash, the earlier transcripts and the memories: none of items 1 to 3 was built
before, on any branch; the addresses for lights, mist and effects were already worked out in docs/research/separate-rooms/).
Items 1 to 3 need no play: they are built from the disc data and the code.
Waiting on the user beside this list: the play test (the Open list below, item 1: nothing in it played yet).
1. **Kit: `ed needs` up to date with separate rooms.** The one-player words it knows (tools/ed/player_data.tsv) predate
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
2. **Kit: a per-room catalogue generated from the disc** (`build/ed-db`: 297 rooms, 2,832 regions, 2,172 script ids in
   4,404 level files): every level, room and region with what it runs (examine words, prompts, items given or taken,
   doors and where, flags, input modes, cutscenes, spawns, cameras), names and IDs, plus a co-op column (the per-player
   words each interaction reaches and which helper covers it). Written as docs per level, by one new `ed` command over
   the existing readers. New pieces: each room's own load script (the room 81 body's path), the 25 rooms in no level,
   flag-off calls, names for cinematics, films and input modes.
   **The catalogue, built 2026-11-16 (committed checkpoint by checkpoint: 76c7321, 29b2143, 3ec6720; no play):**
   `python -I tools/ed/ed.py catalogue` writes docs/game/catalogue/ in about 2 s: index.md, level-NN.md and level-NN.tsv
   for the 15 levels with rooms, and unplaced.md (25 rooms in no level). Per room: its own scripts, doors out, every
   region's scripts grouped, call by call with the nearest tests guarding each; camera-only scripts summed; a shared
   script (L00/1816 in rooms 70, 81, 87) shows only the room's own branch, by following its fn25 tests over the jumps
   (catalogue.room_states); other calls summed per script. `ed catalogue --script L00/ID [--room N]` for one script.
   `ed needs` is callable (know.Needs). **The co-op column (2a4e839):** each row lists the per-player words its call
   reaches, as hooked (a live host hook where the word is read), helper (player_data.tsv names one; whether it's in
   force on that run isn't known here) or one per game, and is flagged when a word is branched on within one call with
   no live hook there. 888 rows flagged over all levels: the room in play 539 (mostly spawns "in the room in play"),
   equipment records 279, script words 172, the look being set up 93, the running script 81, the magick record 2. A
   cmp against 0 is now a "zero-test", not a compare: that took all 722 flags off the player's object. A flag is a
   place to look, not a bug found. **Left: the review's fixes (a workflow is checking it), then item 3's patterns.**
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
3. **Kit: the known patterns as checks** flagged per interaction and in `ed needs`: asks the room in play; who "the
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
   Likely one character (0x80164DD8u); confirm in play before and after. `python -I tools/ed/facts/hooks.py` prints
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
1. **The user's play test** (nothing above has been played): (a) the Tome trip apart and together, either player taking
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

