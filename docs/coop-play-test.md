# What to test in play (2026-11-17): the five co-op fixes

Start every test with **play-rooms-log.bat NAME** (it records both players, pictures and every log for Claude; the hero
gates are switched on there now). Pick the chapter in the launcher's Jump to Game. Two controllers: player 1 and
player 2. "Apart" = the players in different rooms; "together" = the same room. After each, close the game and tell
Claude the NAME: the logs are in logs\run-DATE-TIME-NAME\.

Things scripted runs **can't** check, so they are yours: what you hear, what it feels like, and whether the story still
makes sense with player 2 doing it.

## 1. Edward's lever (chapter 10) — the hero gate and the music duck
Before: player 2 could never pull this lever (the game only let the chapter's own character). Now they can.
- Get to the room with the force field and the lever (room 273, the one with the burning braziers; the lever prompt
  says PULL LEVER). If no prompt shows, the story isn't at that point yet.
- **Apart:** player 1 waits in the next room. Player 2 walks up to the lever and presses B.
  Expect: player 2 sees the whole lever scene (close-ups, the scare) in their half; player 1's half stays normal and
  **player 1's music does not drop in volume**.
- **Together:** the same with both in the room. Expect: the scene plays for player 2; nothing stuck afterwards.
- **Player 1 after player 2:** player 2 walks into the lever spot and away without pressing; then player 1 pulls it.
  Expect: the scene is player 1's, as in the normal game.
- Solo (co-op off): pull it as normal. Expect: exactly the normal game.

## 2. Arrival scripts — a room's entry event for player 2 (chapter 12, Alex)
Before: when player 2 walked alone into a room with its own entry event, the event read player 1's things.
- From the start room (room 100), player 2 alone walks through the doorway into room 90; player 1 stays behind.
- Expect: whatever room 90 does on entry happens for player 2 and looks right in their half; player 1 unaffected.
- Optional (new game, Alex's mansion): player 2 into room 81 first (the room with the hanged body); check the body
  still shows/hides as in the normal game.

## 3. Traps (chapter 6, Lindsey) — and the same fix covers Karim's elevator (chapter 4)
Before: when player 2 stepped in a trap, the game ALSO fired it as player 1, over and over.
- Room 168 has a floor trap (the zone near the middle of the room). **Apart:** player 1 in another room, player 2
  steps on the trap. **Together:** both in 168, player 2 steps on it.
- Expect: the trap goes off **once** per step-in, for player 2, not repeatedly; player 1 not hurt or affected.
- **Chapter 4 (Karim) elevator** (rooms 189/190; not tested by Claude): with player 2 standing next to the lift,
  player 1 should still be able to EXAMINE and ride it; player 2 walking in and out of the lower room shouldn't restart
  player 1's lift prompt.

## 4. Background sounds after the Tome (chapter 3, Anthony)
Before: a sound a player's zone set up (the muffled voices) was put back for the wrong player after the Tome closed.
- Chapter 3 starts in room 150. Player 2 walks into the middle of the room (where the muffled voices play); player 1
  stays at the door. Open and close a Tome (either player), and watch a film/cutscene end too if you can.
- Expect: the voices carry on as before the Tome, for the player who is standing there.
- Also once with player 1 in the voices' zone instead, and once after **loading a quick-save** made while one of you
  stood there.

## 5. Anything else to watch
- Freezes (the game writes saves\reports\hang-*.txt), anyone stuck unable to move after a scene, music stuck quiet.
- Note the chapter, room and roughly when; the pictures (4 a second) let Claude see it.

## Not ready to test yet
- The 21 "two-path" hero checks: scripts that send the chapter's character down one path and anyone else down another.
  Your call whether player 2 should take the character's path. `python -I tools/ed/ed.py herogates --all` lists them.

## 6. The game's own menus hold the launcher's settings now (2026-11-17)
The launcher's menu no longer opens over the game on Esc / Back (it's a debug menu now: launcher Options > Display >
"Debug menu in game", or ED_DEBUG_MENU=1). Everything is in the game's own menus:
- **Player 1's Tome > Options** (L/R to the Options tab): under Controller / Audio / Display / Load/Save are four new
  plaques, **Characters, Save States, Settings, Mods**. Down from Load/Save reaches them; the game's gold ring and
  sounds move with you; A opens that page over the paused game; B comes straight back to the Tome.
  Settings has Display, Sound, Graphics, Controls, then **Return to title screen** and **Quit game**.
- **Player 2's own Tome > Options**: only their **Characters** plaque (their character, on their controller). The four
  of the cross are player 1's (only player 1 saves).
- **Title screen > Options**: a ninth row, **More Options**, under Language Select (Up from Save Settings reaches it).
  It opens Display, Sound, Graphics, Controls, **Mods** (co-op on/off, characters) and Quit.
Check: that everything you used to reach from Esc is reachable this way; how the plaques look (they're the game's own
painted plaque with the word in the Tome's hand); player 2's page on player 2's controller; B always returning to the
Tome or the title; nothing pressed by accident in the game when a page closes. Tell Claude what to move or rename.

## 7. Karim's skeletal pedestal (chapter 4), for Claude to trace
The gate you meant is **room 193** in Karim's chapter (the room numbers are the game's; "room 93" is a different room,
the mansion's teleporter hub in Alex's last chapter, where the game's ending starts). In room 193 the portcullis
(Region01, flag 150) opens only when Karim uses the Tome on the skeletal pedestal (ExamineRgn): the Tome's Use there
(script L04/1381) puts the Tome down as thing 831, opens the gate and hands the Tome back. The next room is the Artifact's,
with the enemies. Karim's elevators also wait on that Tome (thing 831), so this one play unblocks that test too.
Play it with `play-rooms-log.bat karim-pedestal`, co-op on:
- Before the pedestal: player 1's Tome > Options > **Save States**, save a slot (so it can be replayed by script).
- Player 1 uses the Tome at the pedestal (Tome > Inventory > the Tome > USE, standing at the pedestal). Watch: the
  close-up, the gate opening, both players able to go through, player 2's half meanwhile.
- After the gate is open and the Tome is back: save another slot.
- If you have time: load the first slot and let **player 2** use their Tome at the pedestal instead.
- Then on into the Artifact's room (the enemies appear there). Close the game when done.
