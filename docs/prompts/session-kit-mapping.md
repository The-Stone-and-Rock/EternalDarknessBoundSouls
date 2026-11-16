# Prompt: the kit mapping session

Paste everything below the line into a new Claude Code session opened on `E:\Eternal Darkness Recomp Fork`.

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
   (what already works as I like it), and `docs/game/systems/` (a page per system).
4. What the kit already holds: `build/ed-db/` (rooms.json: 297 rooms, 2,832 regions; scripts.json and scripts/: 2,172
   scripts over 16 levels; items.json, text.json, calls.json, camera_paths.json, natives.tsv, lights, facts),
   `tools/ed/primitives.tsv` (what each script command does, by address), `tools/ed/verbs.tsv`, `tools/ed/facts/`, and the
   readers in `tools/ed/ed.py` (room, script, door, examine, item, prompts, text, needs, host, fn, who, uses, tag, level).
5. What the co-op code already does, so the kit can name it: `runtime/host/hle.c` (very large: grep, never read whole).
   The per-player helpers to map include coop_stay_room_in / coop_room_in (a room current for one call), coop_use_start
   (a script run as player 2's use), coop_cameras_lend / coop_cameras_in (camera sets), coop_states_swap (script states),
   the prompt slots (g_coop_prompt_slots), player 2's input copy (CoopPlayerState), coop_run_script (who a script run
   belongs to), the Tome copy (tome_copy_*), the stay and split code (coop_stay_*, split_*), lights/fog/effects per room
   (lights_*, fog_*, fx_*), and the newest rules: coop_story_ticket, notify_script_later, coop_input_mode_start /
   coop_dial_run, keep_request_hold / keep_filter, coop_state_loaded. `docs/coop-failed-attempts.md` and
   `docs/coop-todo.md` record what was learned and where it broke.

**The goal, in order:**
1. **`ed needs` up to date with separate rooms.** Its catalogue of one-player words predates the two room slots. Add the
   room in play (r13-23276), the slot table (0x8058AFC0), the input mode byte (0x80323AEB), the running script
   (r13-23228), the busy word (r13-23344), the keep (0x80307184), the subtitle record (0x80308DE8), and the per-room
   words of lights, mist and effects; for each, the host helper that puts player 2's in. Acceptance: `ed needs 0x8007F254`
   (the clock dials' step, which reads the room in play in its third instruction) names the room in play and
   coop_stay_room_in; `ed needs` on the level room script L00/1816 path names the room in play too.
2. **A per-room catalogue generated from the disc data**, by one new `ed` command that reuses the existing readers:
   every level, room and region with what it runs (examine words named, prompts, items given or taken, doors and where
   they lead, flags checked and set, input modes, cutscenes and films, spawns, cameras), with names and IDs, plus a co-op
   column: the per-player words each interaction reaches (from the updated `ed needs --script`) and which helper covers
   each, flagged when none does. Output: one document per level under `docs/game/catalogue/` (Markdown, plus a TSV), with
   an index. Camera-only regions summarised, not listed one by one.
3. **The known patterns as checks**, shown in the catalogue's co-op column: asks the room in play; one global per game
   (busy word, input mode, keep, subtitles); queued for a later frame (the load's room script, film and cinematic next
   scripts); run inside player 2's change; drawn from one camera. Each pattern links to the fix that already exists for
   it in hle.c (the code or the commit), from coop-failed-attempts.md and the handoff.
4. If time allows: the play-check runner (to-do item 5), only after showing me the catalogue.

**How I want it worked:** ultracode is on: use workflows for the reading and for checking the generated catalogue
against the code (readers, then adversarial checks), and do small things yourself. Read before building; no guesses
stated as facts; say plainly what the catalogue can't know (story gates, timing, what's only visible on screen). Runs
only if needed, only from fresh Jump-to-Game states (tools/ed/jtg.sh), every run with ED_RUN_AWAY=1, never `ed test` or
`ed chapters`, kill the whole process tree including EDrun*.exe when stopping runs. Commit only when I say, never
runtime/host/controls.cpp or tools/ed/.run_exe_tmp.sh; don't push. Keep me posted in plain words; I may be on my phone.

**Done looks like:** the updated `ed needs` passing its acceptance checks; `docs/game/catalogue/` generated for all
levels with its index; a short report of how many interactions the catalogue flags as uncovered, by pattern, with the
top ones named; and the handoff's to-do list updated with what's done and what the catalogue found.
