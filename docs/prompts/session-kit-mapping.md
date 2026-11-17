# Prompt: the kit mapping session

Paste everything below the line into a new Claude Code session opened on `E:\Eternal Darkness Recomp Fork`.

Revised 2026-11-16 after a read-only check of this plan against the code, every branch and stash, the earlier session
transcripts and the memories: the corrections and the gaps it found are folded in below. Nothing in it needs the game
played: goals 1 to 3 are built from the disc data and the code alone.

**Done 2026-11-16 (goals 1 to 3, committed through e8338a3): `ed needs`, `ed catalogue` (docs/game/catalogue/) and
`ed patterns`. What was built, what it found and what's next: the to-do list at the top of docs/coop-next-session.md.
Goal 4 (the play-check runner) is not started.**

---

This session is dedicated to the project's knowledge kit (the `ed` tool), so that every later feature finds the co-op
rules it will hit before anyone plays it. No gameplay fixes this session unless I ask.

**Read first, in this order, before anything else:**
1. `docs/coop-next-session.md`: the top section "THE TO-DO LIST, in order" (this session is items 1 to 3, and 5 if
   time allows), then the handoff sections below it for what exists and how I want work done.
2. The memory index (MEMORY.md) and the memories it points to, especially: take full control on co-op bugs, read the
   gates before building, co-op fixes are engine-generic, co-op never touches solo, fresh save states only, test runs
   away and muted, Python edits keep LF, edits with escapes, publishing needs Manual mode.
3. `CLAUDE.md`, then `python -I tools/ed/ed.py` with no arguments (its command list), `python -I tools/ed/ed.py keep`
   (what already works as I like it), and `docs/game/systems/` (a page per system). CLAUDE.md's "after any change, run
   `ed test`" and the ed skill's "a fix isn't done until it passes" predate my rule: never run `ed test` or `ed chapters`
   (they run old save states; decided 2026-11-06 and 2026-11-13, memory "fresh save states only").
4. What the kit already holds: `build/ed-db/` (rooms.json: 297 rooms, 2,832 regions; scripts.json and scripts/: 2,172
   distinct script ids, 4,404 (level, script) files over 16 script folders, L-1 the boot's; items.json, text.json,
   calls.json, camera_paths.json, natives.tsv, lights, facts: doors_table, examines_table, placed_things, regions,
   regions_summary, verbs, who_table, levels, item_table, mix_table), `tools/ed/player_data.tsv` (the one-player words
   `ed needs` knows), `tools/ed/primitives.tsv` (what each script command does, by address), `tools/ed/verbs.tsv`,
   `tools/ed/facts/`, `tools/ed/sweep_list.py` (every examine and pickup region of a chapter), and the readers in
   `tools/ed/ed.py`, `know.py` and `gameinfo.py` (room, script, door, examine, item, prompts, text, needs, host, fn, who,
   uses, tag, level; know.summary turns a script into a catalogue line).
5. What the co-op code already does, so the kit can name it: `runtime/host/hle.c` (very large: grep, never read whole).
   The per-player helpers to map include coop_stay_room_in / coop_room_in (a room current for one call), coop_use_start
   (a script run as player 2's use), coop_cameras_lend / coop_cameras_in (camera sets), coop_states_swap (script states),
   the prompt slots (g_coop_prompt_slots), player 2's input copy (CoopPlayerState), coop_run_script (who a script run
   belongs to), the Tome copy (tome_copy_*), the stay and split code (coop_stay_*, split_*, split_busy_allows), lights /
   fog / effects per room (lights_room_in, fog_begin / fog_end, fx_ticks, fx_emitters, effect_draw_by_room), and the
   newest rules: coop_story_ticket, notify_script_later, coop_input_mode_start / coop_dial_run, keep_request_hold /
   keep_filter, coop_state_loaded. `docs/coop-failed-attempts.md` and `docs/coop-todo.md` record what was learned and
   where it broke; `docs/research/separate-rooms/` (design-fog.md, lights-game-readers.md, particles-reader.md) has the
   lights, mist and effects words already worked out.

**The goal, in order:**
1. **`ed needs` up to date with separate rooms.** Its table of one-player words (`tools/ed/player_data.tsv`) predates
   the two room slots. **Done 2026-11-16, all three acceptance checks pass (the to-do list's item 1 in
   coop-next-session.md has what was built and what it found): start at goal 2.** The text below is kept for what each
   piece is.
   - Add: the room in play (r13-23276; helpers coop_stay_room_in / coop_room_in); the slot table (0x8058AFC0,
     ROOM_SLOT_TABLE: +0 current slot, +4 the other or -1, +8 queued room, +12 a record per slot; its size from the code,
     not guessed; coop_room_in swaps it); the input mode byte (0x80323AEB, an s8, -1 none); the keep (0x80307184,
     KEEP_DEST); the subtitle record (0x80308DE8); the mist's two words (r13-22144 the gate, r13-21808 the ramp bank,
     fog_begin / fog_end); the light grid (0x804F0784) and the 12 dynamic light pointers (0x80604E60, each light's room
     at +42). Effects and emitters carry their own room (+56, +4900) and the game compares it with the room in play, so
     the r13-23276 row is what catches them: say so in its text rather than adding rows that can't match.
   - Update, not add: the busy word (r13-23344) and the running script (r13-23228) are already rows. Bring their
     helpers up to date (split_busy_allows / coop_busy_ask_answer; coop_run_script's room-script block with
     notify_script_later).
   - The helper column says the truth: where nothing puts player 2's in, write "one per game: ..." or "none". The input
     mode byte is one per game (coop_input_mode_start records whose it is), the keep is one per game and stays solo's
     (keep_filter leaves player 2's things out), the subtitle record has no host fix at all.
   - Fix the scanner in know.py scan(): a base built with lis + addi + addi (0x800B7100 builds the keep this way) never
     tests the address table on the second addi, so a keep row is never found; and a register's tracked value is never
     cleared except by a new lis to it (not by li, a load, or/mr, an addi from an untracked base, nor across a bl's
     r3-r12), so a wide row can give false hits. r13-23040 is two rows (coop_play_hold
     and coop_scan_player1) and the table keeps only the last: let rows that share an address merge.
   - Add `--level N` (or `L00/1816`) to `ed needs --script`: today it merges a script's calls over every level the id
     appears in (642 ids appear in more than one), and `--script 1816 --level 0` is refused.
   - A noise rule before the catalogue uses it: with the room in play in the table, 68 of the 413 script commands reach
     it within 3 calls. Separate a depth-0 compare or write from a pass-through read deeper in, and show which ones
     matter when the players can be apart.
   - Acceptance: `ed needs 0x8007F254` (the clock dials' step; it reads the room in play at 0x8007F27C, its 11th
     instruction) names the room in play and coop_stay_room_in; `ed needs --script 1816 --level 0` (the level room script
     L00/1816) names it too; `ed needs 0x800B7100` finds the keep. The function header prints decomp names with USA
     addresses (fn_8008210C for 0x8007F254 on this disc): say "USA name" beside it so nobody reads it as a different
     function.
2. **A per-room catalogue generated from the disc data**, by one new `ed` command that reuses the existing readers
   (know.summary, gameinfo's door / examine / prompt / item / tag tables, sweep_list.py, ed.py annotate; refactor cmd_needs
   to return records, and load the disassembly once and cache each function's scan: running the command line per
   script would take 15 minutes or more): every level, room and region with what it runs (examine words named,
   prompts, items given or taken, doors and where they lead, flags checked, set and cleared, input modes, cutscenes and
   films, spawns, cameras), with names and IDs, plus a co-op column: the per-player words each interaction reaches
   (from the updated `ed needs --script ID --level N`) and which helper covers each, flagged when none does. Output:
   one document per level under `docs/game/catalogue/` (Markdown, plus a TSV), with an index. Camera-only regions
   (about 830) summarised, not listed one by one. Not built anywhere yet, and these pieces are new:
   - Each room's own load script, run from no region: the room file's s16 at +136 (checked on the disc: 1816 for rooms
     70, 81 and 87; 2530 for room 69; -1 in 106 of the 297 rooms), read by 0x80153BD8 (`lha r5,136` at 0x80153C08),
     called from 0x80153B2C, which queues it (0x80153B68) or runs it at once (0x80153B80). Add it to rooms.json or read
     it at catalogue time. This is the room 81 hanged-body bug's path, so every room needs a "runs at load" row.
   - Rooms belong to levels through facts/levels.json's Npcs placements, its door walk and the start rooms: 25 rooms are
     in neither list (205 among them; 24 counting start rooms, room 0 being level 17's). Rooms 5 and 83 are named by every
     level's Npcs file and left out of the walk on purpose; level 16 has 3 Npcs-placed rooms (5, 39, 83) and no walk.
     Give the unplaced rooms their own section rather than dropping them.
   - A script shared by many regions is listed once per room with its regions (1061 runs in 270 regions; 857 in five of
     room 69's).
   - Flags: know.summary catches fn29(N, 1) and fn30 only; add fn29(N, 0), flag off (481 calls). fn31 is never called.
   - Names for cinematics (ed55 n: the 173 x 32-byte table at 0x80242150), films (ed83 n: 65 x 24 bytes at 0x80241B38)
     and input modes (ed56 n: the jump table at 0x80244CAC; facts/verbs.py already names the bindings of modes 0-4, 7,
     8, 9/10 and 11; mode 5's aren't read, mode 6 does nothing).
   - A door's branches have no per-branch conditions today: `ed door` lists only the rooms a door script compares fn25
     with, per script (gameinfo.room_tests); room 69's door scripts list room 245 and the real room. Attach the flag,
     state (fn0) or room test guarding each fn66 (L00/824's branches hang on fn0, ed60 and fn30(335)).
   - Plane regions (403) have no position. Examines whose words a variable picks (32 of 624) show "one of: ..."
     (reuse gameinfo.variable_words); another 83 are ed219 with entry -1, a close-up camera with no words: say so.
3. **The known patterns as checks**, shown in the catalogue's co-op column and in `ed needs` itself (any address or
   script), so later features get them before building. Coverage is per address, not per pattern: build a map from
   game address to host hook from all of hle.c's installs (the kHandlers, kAddrHandlers and kNotify tables in
   initialize_hle_dispatch, and every hle_dispatch_entry(...)->intercept / ->notify; resolve the #define names and note
   which are conditional, e.g. coop_dial_step only when the prologue words match), so "covered" means that function
   is hooked. The patterns, each linked to its fix (the fix's own function and commit; `git log -S` on a
   hook's name can give the wrong commit, e.g. notify_script_later dates from 2e6bca6, its room-script fix is f8e5d82):
   - asks the room in play (coop_stay_room_in / coop_room_in; the clock: coop_dial_run, f8e5d82);
   - who "the player" is (r13-26524/-21492/-21496, coop_be_player2; coop-next-session.md "The pattern behind many of
     these bugs");
   - one global per game: busy word (coop_no_pause, coop_busy_ask, split_busy_allows), input mode
     (coop_input_mode_start, f8e5d82), show words fn111 (coop_show_asked, 00232b7), hold words, the zones-on word, the
     looks on offer (0x803090C4 / 0x80307108: one copy, never swapped), the six prompts (g_coop_prompt_slots), and
     subtitles: NO fix exists on any branch (0x80308DE8 is only named as the keep overflow's victim); mark it "no fix
     yet";
   - one script's state machine for both players (the door script L01/119; coop_states_swap, StayerAgain);
   - game tables player 2's extra things fill: the keep has no bound (keep_filter / keep_request_hold, 74a7989), the
     80 sound tracks, the 12 dynamic lights, the six prompts;
   - things stamped with the room in play when made (rune sparks: rune_flame inside coop_room_in);
   - queued for a later frame: film and cinematic next scripts are story changes (coop_story_ticket: both players go,
     f8e5d82); the load's room script is a different fix (notify_script_later plus coop_run_script's first block: run
     with that room current, f8e5d82) and is also "run inside player 2's change";
   - run inside player 2's change (the R4 bracket in notify_ticket_run, 9957a4f);
   - one camera: the script camera set has one owner (coop_cameras_in / coop_cameras_lend; coop-todo.md room 183) as
     well as effects drawn from one camera (fx_facing_begin, 12e0d87 / 3542bf0). Draw-time camera reads go through draw
     callbacks a static walk can't follow: say that this check misses them.
   Say which checks are reliable from the data (script calls: high) and which are run-time only (whether two players
   really collide, timing, held buttons, save-state host data, the renderer's shared buffers).
   One candidate is already known: 0x8007E508, the MOVE STAFF EXIT step (input mode 0, L11/1384's ed56(0)), reads the
   room in play at 0x8007E7D4 with no room hook. It is not one of mode 0's bindings (table 0x80244920: 0x8007DAEC,
   0x8007DE4C, 0x8007DFC4, none of which reads it); it's called from ed68(0)'s stop (0x8007A9AC) and from a per-frame
   room updater, 0x80083464 (from the main loop), which picks by the room in play itself (0x8008347C). coop_dial_run
   covers only the clock's 0x8007F254 / 0x8007F140; the interface code's other reads (50 direct reads of r13-23276 in
   0x8007A968..0x80083700, more through callees) are unchecked. The catalogue should
   find it and its siblings on its own; this session doesn't fix them.
4. If time allows: the play-check runner (to-do item 5), only after showing me the catalogue. About half exists
   (tools/ed/jtg.sh, coopplan.py, coopcheck.py, coopjudge.py, recording.py; room states by door hops by hand in
   docs/research/separate-rooms/build-run-*.sh). Missing and known limits: Jump to Game has chapters 1 to 12 only (level
   0's mansion rooms only through JTG_NEWGAME's dream; levels 13 to 15 none); ED_PUT moves a player only inside their
   own room and ED_RUN_SCRIPT always runs as player 1, so players get apart through a real door (coopplan.take_door);
   no log tags each script call with the player and the room in play (the [script] lines, 40 calls after each script
   run with ED_COOP_TRACE or 200 after a trigger with ED_ROOM_LOG, name neither): that needs a testing switch in hle.c, off unless set, solo untouched, which I approve before it's built.

**How I want it worked:** ultracode is on: use workflows for the reading and for checking the generated catalogue
against the code (readers, then adversarial checks on every flagged row and a sample of rooms per level), and do small
things yourself. Read before building; no guesses stated as facts; say plainly what the catalogue can't know (story
gates, timing, what's only visible on screen, anything behind a variable or a table). Runs only if needed, only from
fresh Jump-to-Game states (tools/ed/jtg.sh), every run with ED_RUN_AWAY=1, never `ed test` or `ed chapters`, kill the
whole process tree including EDrun*.exe when stopping runs. Commit only when I say, never runtime/host/controls.cpp or
tools/ed/.run_exe_tmp.sh; don't push. Keep me posted in plain words; I may be on my phone.

**Done looks like:** the updated `ed needs` passing its three acceptance checks; `docs/game/catalogue/` generated for
all levels with its index and the unplaced rooms; a short report of how many interactions the catalogue flags as
uncovered, by pattern, with the top ones named, ranked as a fix list for the next session (it becomes the co-op sweep's
order); and the handoff's to-do list updated with what's done and what the catalogue found.
