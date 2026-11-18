Continue with Eternal Darkness Recomp (E:\Eternal Darkness Recomp Fork, branch tools/game-kit), focusing on the co-op
mod. Read CLAUDE.md first and use the `ed` skill, then docs/coop-next-session.md (the newest sections are at the top),
docs/research/solo-coop-picker/plan.md and docs/research/solo-coop-picker/next-requests.md. Ultracode is on: use
workflows, test every change in the game with pictures, and make a checkpoint commit after each step. Never commit
runtime/host/controls.cpp or tools/ed/.run_exe_tmp.sh, never push, never stash. Use ED_RUN_AWAY=1 on every run, never
`ed test` or `ed chapters`, and save pictures only inside "Screenshots for Claude". I'm on my phone over Remote Control,
so please use plain words.

PRIORITY: bugs I found playing the current build (2026-11-17 evening):

1. Tome > Load Game (the in-game memory card saves, not save states) says the save cannot be loaded because it was
   made outside of Eternal Darkness. Find out why: our card file (ed-GEDP01-slot-a.dolcard), the Solo/Co-op picker
   work (P3-P6), the co-op save changes (coop_park/coop_spawn/coop_adopt), or the play-rooms vs play.bat settings.
   Logs: logs/play-prev.log, logs/play.log, logs/play-rooms.log.

2. Changing player 1's character from the Tome's Characters page (the new character menu in the Tome) froze the game.
   Lines of text filled both halves of the screen, and a crash report was made: saves/reports/hang-20261117-204821.txt
   with its .state. The log (logs/play-prev.log, near the end) shows "characters changed ... reloading where they
   stand", then "player 2 is gone (an area change?)", then the freeze. The game was stuck looping at 0x80155AA4,
   inside the words/subtitle maker (lr 0x801DB4C4; called from 0x801170C0, 0x80117854, 0x800084FC). Changing the
   character on the fly worked before the Tome character menu was added: find what broke it and fix it.

3. The Tome's character selection needs a new layout: the portraits are too small to see. Propose a clearer one that
   still looks native to the Tome (bigger pictures, fewer at a time, or one large picture of the lit choice), show me
   mock-up pictures first, then build the one I pick.

THEN, the sound and music on the title's Choose Character (steps P8 and P9 in next-requests.md):
- P8: pressing A on Choose Character on the title's main menu plays the same sound the game plays when Load Game is
  picked. Exactly one sound, no doubles.
- P9: while Choose Character is open, the title screen's own music plays (the music heard on the title before the main
  menu settles; our earlier runs measured silence on the main menu itself). It should sound like part of the game:
  no doubling with the Options song, the Solo/Co-op page or the attract movie. Check it with sound recordings.

SMALL LEFTOVERS from the last session:
- The Tome's Settings page still has an old "Quit game" row that closes with no question; the new Exit Game plaque asks
  first. Make them the same, or remove the old one.
- F11 (load the newest save state) still refuses a state made in the other mode instead of switching to it, as the
  launcher and the Tome now do.
- In the Tome's Choose Character with HD off, Random's description sits very close to the Stats row.
- Not tested yet: the sounds and music by ear (all runs were muted), real B/Esc/mouse on the new pages, other
  languages, 4:3 and 21:9.

LATER, already in the handoff (docs/coop-next-session.md, "Open, in order", and docs/coop-todo.md):
- Remind me to play Karim's skeletal pedestal (room 193) with logging: docs/coop-play-test.md section 7.
- My play test list: docs/coop-play-test.md (the Tome trip apart and together, Max's Tome apart, spells while apart,
  a Chapter Page by each player).
- Chapter 13's Tome (Alex), and the Chapter Pages left (the real mansion, chapters 3 and 11, a chapter's end).
- Smaller separate-rooms gaps in coop-todo.md: a ladder's Yes in the other's door, examine/pickup during the other
  player's cutscene, the Tome room's first-entry camera in player 2's half.
- Subtitles plan C (two tracks), the shared pickup offer fix, a "run as player N" helper, camera modes per player.
- The remaining two-path hero gates (my choice, one by one).
- Player 2's own Sprint/Creep/Target/deadzone/vibration settings (they share player 1's for now).
- "Bring X to Y" in the co-op menu, and the randomizer last.
