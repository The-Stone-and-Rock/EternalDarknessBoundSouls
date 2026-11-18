Continue with Eternal Darkness: Bound Souls (E:\Eternal Darkness Recomp Fork, branch tools/game-kit). Read CLAUDE.md,
then the status board at the top of docs/coop-next-session.md ("START HERE"): it has how I want this worked on, the
open work in my order, and what is already fixed and how. Do not redo or recheck anything marked [FIXED] or [DONE].

How to work (my rules, in priority order):
1. Before any fix in the co-op mod, use every tool, skill, kit and table that exists: the `ed` skill
   (`python -I tools/ed/ed.py keep`, `how`, `verb`, `system`, `needs`, `host`), docs/game/systems/, the tables in
   tools/ed/, docs/coop-failed-attempts.md. The game already has the function: find it, do not rebuild it.
2. Solo play is not touched unless the task says so. Fixes are for the whole game: every chapter, character and form,
   both players. No chapter-only or character-only special cases.
3. Few test runs, no fix-break-fix-break. Read first, build once, then one run that records both players
   (tools/ed/run.sh, tools/ed/jtg.sh, `ed timeline`, `ed check`). Use several agents at once for wide reading.
   ED_RUN_AWAY=1 on every run. JTG_EXE needs JTG_SLOT=1.
4. The goal is a true co-op game that runs smoothly, built on what exists, without breaking features.
5. Usage: check the usage tracker before you start and after each step. At 95% of the weekly Fable limit or of the
   weekly all-models limit, stop at a commit, update the status board, write the hand-off and tell me.
6. Keep the status board true: when something is finished, mark it [FIXED] or [DONE] with one sentence on how and
   where, in the same commit as the work. Never delete a line.
7. Commit only when I say (checkpoint commits inside a task I gave are fine), never push unless I say, never git stash,
   never commit runtime/host/controls.cpp or tools/ed/.run_exe_tmp.sh. Pictures only inside "Screenshots for Claude".
   Plain words and pictures; I am not a programmer.

The open work, in order (details on the board):
1. A high-pitched break in the sound when the game loads from the title screen.
2. Graphics options reworked: "Remastered spells" removed and the original look of spells restored, with HD textures
   on and off; contact shadows and the other heavy options measured, then optimised or removed (slowdowns, high GPU
   use, or no effect in play); a remaster look that breaks nothing and works the same in co-op.
3. Jump to Game on the title screen must ask Solo or Co-op, as Start Game and Load Game do.
4. Karim's shotgun: the reload, and Roberto, Lindsey and Max with it (the hold is fixed).
5. Changing character in the middle of the opening dream drops the dream's own mode.
6. `ed test` must pass again (it stopped on a missing probe.txt in test-shadow-steady).
7. The rest of the board, then docs/coop-todo.md and docs/coop-glitch-todo.md.

Start with number 1 unless I say otherwise. Tell me in plain words what you found before you change anything large.
