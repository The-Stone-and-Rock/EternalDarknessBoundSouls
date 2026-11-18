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
