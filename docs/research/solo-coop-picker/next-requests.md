# Requests queued after the picker build (the user, 2026-11-17)

To build once the picker steps P1-P6 are committed, as steps P7-P9.

The user's words: "Also, The in game tome menu under options, player 1 needs an exit game button. That button is
already mapped in the in-game launcher menu (the one we hid). The choose character selection on the main in-game title
screen can have the same audio sound that plays as Load. I'm [In] the choose character menu itself, the music that
plays on the title screen is fine to use."

- **P7. Exit Game in the Tome's Options, player 1 only.** A row in the Tome's Options (player 1's Tome) that quits the
  game. The hidden in-game launcher menu already has this action: find it and reuse it, with the Quit ask
  (Screen::QuitAsk) the title's Quit uses. Not in player 2's Tome.
  Built: a fifth plaque under Settings and Mods; plan.md "Built: step P7".
- **P8. Choose Character's select sound.** Pressing A on Choose Character on the title main menu plays the same sound
  the game plays when Load Game is picked.
- **P9. Music under Choose Character.** The answer to the open question: play the title screen's own music (the
  title's opening music heard before the main menu settles, about 6.5-14.5 s in the F5 PCM,
  Screenshots for Claude/f5-title-music/06-final-build-pcm-loudness.txt) while Choose Character is open. Find what the
  game starts there (sound id, how it is started and stopped) and keep it going or start it under the page; stop it
  the way the game would when the page closes. It must sound native: no restart on every page open if avoidable.

## Built: P7 (2026-11-17)

- Player 1's Tome, Options page: a fifth plaque, "Exit Game", alone under Settings and Mods. Player 2's Tome has
  only Characters, as before.
- A opens the title Quit's ask in the Tome look ("Exit the game?", No first). No or B: back on the Tome. Yes: the
  settings are saved and the game closes, as the hidden launcher's Quit does.
- Seen in pictures: Screenshots for Claude/solo-coop-picker/P7-exit-game/ 01-12 (solo, co-op, the title's Quit
  unchanged). Reviewers found nothing. Not seen: real B/Esc/mouse on the ask, 4:3, 21:9, other languages.
- Open for the user: the Tome's Settings page still has its own "Quit game" row with no ask.

## Read for P8 and P9 (2026-11-18, from the disassembly and the disc's sound data; a reader and a skeptic each; nothing built)

- **P8 as written is already so.** A on Choose Character (the game's spare row 5) runs the game's own
  `0x8019DB90(633, 100)` once at 0x8001A678-0x8001A684, the same instruction Load Game's A ends with. Load Game's A plays
  nothing else (Start Game adds 634, Options 723, Jump to Game 722). Do NOT add a host 633: it would double.
- **What does differ: the lit row's loop.** Each main-menu row plays a looping sound while lit (Up/Down 0x8001B810:
  stop TM+56, then row 0 Start 629, row 1 Load 630, row 2 Options 631, row 3 Jump to Game 724, row 4 Credits 632), its
  handle in TM+56 (0x8030915C). Row 5, Choose Character, starts nothing: it is silent when lit. Any A stops the loop.
  (docs/research/in-game-menu/02-title-menu.md has rows 3 and 4's sounds swapped: 724 is Jump, 632 Credits.)
- **"The title screen's music" is sound 629**, Start Game's own loop, started whenever the main menu comes up on row 0
  (sample 12/13 of ed_menu.sam, 11.2 s with an 8.9 s loop; it ends only on a key-off, with a 1 s fade). No disc stream
  plays on the main menu. The 6.5-14.5 s heard in the F5 recording is 629, then Load's 630 after the Down, then the A's 633.
- **Ask the user which they mean before building** (their words: the selection "can have the same audio sound that plays
  as Load"; "in the choose character menu itself, the music that plays on the title screen is fine"):
  1. the row lit plays Load's loop 630, and the open page plays 629 (reads both sentences literally); or
  2. the row lit plays 629 and it simply goes on under the page (the reader's design A: no restart at each open); or
  3. silent row as now, 629 only while the page is open (restarts at every open).
- **How to build it (design A's parts, checked by the skeptic):** a helper from `title_main_draw` that, on the main menu
  with the cursor on row 5 and TM+56 == -1, calls SOUND_PLAIN(id, 100) and writes the handle to TM+56 (retry a failed
  start after about 60 frames); and a row-5 branch in `title_main_start_load` that does the A's steps itself
  (TITLE_INPUT_TAKEN, 633 once, `title_main_choose`, pass-through off as its last line) so the game's A does not key the
  loop off. Nothing at the page's close: the game stops TM+56 at Up/Down, any A and the idle timeout. Never keep the
  handle in r13-26144 (Options and Jump overwrite it). Check with ED_TRACE_FN=0x8019DB90,0x8019DC00,sndFXStartEx and
  ED_PCM_OUT, as run -154055-jtg-f5-final-main; a quick Down, Down shows two copies of 629 overlapping for a second.
