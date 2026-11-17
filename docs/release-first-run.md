# The first-run "compile build": the game's own art and music made on the player's PC

The user's wish (2026-11-17): when this is released, a first-run step makes everything the recomp takes from the disc,
on the player's own PC, from their own disc, the way games build their shaders on the first start. Nothing taken from
the disc may ship in the release zip (docs/memory: release-steps; the package excludes .dol/.iso/.state/settings.ini).

## What comes from the disc today, and how it is made

| Piece | Used by | Made today | First-run plan |
|---|---|---|---|
| The 12 character portraits (`res/ui/portraits/NAME.png`) | the Characters pages | `frontend.cpp ensure_portraits` reads the disc and writes them the first time the roster opens (fast) | Make them in the first-run step, so the roster never waits |
| The title Options picture (`res/video/options.rgba`) | the Load screen's backdrop (titlevideo.cpp) and More Options' row | kept the first time the title's Options screen is drawn (`keep_skull`) | Run the game unseen to the title Options once (the step below) |
| The launcher's music (`res/music/launcher.wav`) | the launcher | by hand: record `ED_PCM_OUT` at the title Options, `tools/launcher_music.py` cuts one loop | The same recording, automatic: the step below, then the cut in C++ (one loop is 2048297 samples at 32 kHz, from a fixed offset after the screen opens) |
| The Tome's Options plaques (`plaques.cpp`) | the Tome's Options page | at run time from the page picture in the game's memory (UBookPak image 27); **no file** | Nothing to do: made live, every start |
| "More Options" on the title Options | titlevideo.cpp | painted live into the game's own picture; **no file** | Nothing to do |
| The project's own art (fonts, `res/ui/tome/*`, glyphs, the title video, form portraits `*-*.png`) | menus | in git; ships | — |

## The step (to build before the next release)

1. **When:** after the launcher has found a playable disc (`disc_playable()`), if any piece above is missing or older
   than the build that makes it (a version number per piece in `res/firstrun.txt`).
2. **Screen:** the launcher shows "Preparing the game from your disc..." in the Tome's style with a progress bar
   (the user asked for a "Building... Please wait" screen with a bar, docs/coop-todo.md "The launcher's music").
3. **Work:**
   - portraits: call `ensure_portraits()` (seconds);
   - the title Options picture and the music: start the game unseen (ED_RUN_AWAY's way: off the desktop, muted to the
     speakers but with `ED_PCM_OUT` recording), press through to the title Options with the scripted pad (as
     `tools/ed/jtg.sh` presses the title: Down, Down, A at frames 700/740/800), wait about 70 s of the Options music,
     keep the picture (`keep_skull` already writes it) and cut the loop; close the game.
4. **Then** the launcher carries on as normal. A failure leaves the piece missing: the launcher's music is silent, the
   Load screen uses the video alone (both already handled), and the step is offered again next start.
5. **Package:** `tools/win/make-win-package.sh` keeps excluding `res/music/`, `res/video/options.rgba` and the 12
   disc portraits (already in .gitignore); add a check that fails the package if any of them is inside the zip.

## Not decided

- Whether the first-run step should also build the shader cache it already builds behind the launcher (it is fast:
  5611 pipelines in about 27 s on the user's PC) or leave that as it is.
- Other languages: the plaques and More Options are drawn for the English pictures; another language's Options page
  keeps the plain fallback (the plaques' words as text) until measured.
