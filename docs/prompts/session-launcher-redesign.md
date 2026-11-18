Continue with Eternal Darkness Recomp (E:\Eternal Darkness Recomp Fork, branch tools/game-kit): build the new MAIN
launcher. Read CLAUDE.md, then docs/research/launcher-redesign/decisions.md (what I chose) and prep.json in the same
folder (how the launcher is built, the step plan and the skeptic's fixes). The look to match is
"Screenshots for Claude/launcher-redesign/mock-7a-check-updates.png" and mock-7b-update-available.png.

Rules: only the main launcher changes (not the menu over a running game, not the title or Tome pages). No renames of
files or the exe. Build in small steps, a picture of the launcher after each (ED_MENU_OPEN and ED_LAUNCHER_TEST_KEYS
open and drive it without a game), checkpoint commit after each step, never commit runtime/host/controls.cpp or
tools/ed/.run_exe_tmp.sh, never push unless I say, never git stash. ED_RUN_AWAY=1 on every run. Pictures only inside
"Screenshots for Claude". I'm on my phone: plain words, and send me pictures as you go.

Usage: check my plan usage (the usage tool) before you start and after every step. Stop at a checkpoint and tell me if
the 5-hour limit or the weekly limit reaches 90%. Keep it lean: build directly, and use one review at the end rather
than many helpers.
