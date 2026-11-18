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
- **Title screen > Options**: since step 3 below, one scrolling list: the game's 8 rows, then Display, Sound,
  Graphics, Controls, **Mods** (co-op on/off, characters) and Quit game. (Before, a ninth row, More Options.)
Check: that everything you used to reach from Esc is reachable this way; how the plaques look (they're the game's own
painted plaque with the word in the Tome's hand); player 2's page on player 2's controller; B always returning to the
Tome or the title; nothing pressed by accident in the game when a page closes. Tell Claude what to move or rename.

**Native menus, step 1 (2026-11-17 morning):** groundwork for the new title Options list and the Tome look. Nothing
should look different yet. Try:
- The launcher, the Tome's four plaques and player 2's Characters plaque: all as before.
- Title > Options > More Options > Mods: the Cheats and Sanity Effects rows are greyed and say "In a game, from its
  menu." The two character cards say a change also applies to a save state you load.
- Tell Claude if anything changed that shouldn't have.

**Native menus, step 2 (2026-11-17 morning):** the title's main menu has **Choose Character** between Load Game and
Options (English). Try:
- Up and Down through every row: each lights as before, at its place. Options, Jump to Game and Credits sit one row lower.
- A on Choose Character: the two players' cards (the launcher's look for now). B, Esc or the Back row: back to the
  title with the row still lit. Also: A on a card, then back to the cards.
- Leave the cards open a few minutes, close them: the title should not jump straight into its attract movie.
- With Mods > Title video off and the HD pack on: the title's picture is now the disc's, not the HD pack's. Say if
  that matters.

**Native menus, step 3 (2026-11-17 morning):** the title's **Options is one list that scrolls** (English). More
Options is gone. The rows: Save Settings, Subtitles, Widescreen, Rumble Feature, Volume Adjustment, Brightness
Adjustment, Audio Mode, Language Select, then **Display, Sound, Graphics, Controls, Mods, Quit game**. Nine show at a
time; a small mark at the left of the top or bottom row says more rows are hidden that way. Try:
- Down through all 14 rows and back Up: the list moves one row at a time once the lit row reaches the top or bottom.
  Up from Save Settings goes to Quit game (the list jumps to its end); Down from Quit game goes to Save Settings.
- The game's own rows at any place in the list: A on Subtitles, Widescreen, Rumble and Audio Mode changes the word
  beside it; Volume and Brightness open their pages; Language Select works; Save Settings saves.
- A on Display, Sound, Graphics, Controls or Mods: that page (still the launcher's look). B, Esc or Back: back to the
  list on the same row. **Quit game asks "Quit the game?"** with No lit first; Yes saves the settings and quits.
  Changes to the game's own rows reach the memory card only when you leave Options with B, so leave that way first.
- Pick another language in Language Select: Options shows the game's own 8 rows in that language, and Down from the
  last row goes back to the first (no hidden rows). Pick English again: the list of 14 comes back.
- The rows are now evenly spaced, so even at the top the game's own rows sit a few pixels from where the disc painted
  them. Say if you'd rather have the disc's spacing at the top.
- Our six rows are in the Tome's hand (La Belle Aurore), as large as the game's. Say if the size or colour is off.

**Native menus, step 4 (2026-11-17 morning, committed):** every page opened from the title now has **the title
look**: the Options screen's skull picture behind it (its words wiped), no parchment, the rows in the game's hand and
teal, the lit row pale cyan with a soft glow, values in small print. Pictures: `Screenshots for Claude/s4-title-skin/`.
Try:
- Title > Options > Display, Sound, Graphics (and its four pages), Controls, Mods (and its pages), Quit game. Each
  page has its name at the top in the game's size, its rows in the column where the game's rows are, and a Back row.
  B, Esc or Back goes back one page; Back from the page you opened returns to the Options list.
- Display and Controls are longer than the space: a small pen-stroke mark at the left of the top or bottom row says
  more rows are hidden. Down past the last row in view moves the list. The right stick also scrolls it.
- Button Mapping, Choose Character's two cards and the character roster are wider: they sit on a dark panel over the
  skull, in teal, with the gold and blue of the two players kept.
- The title's main menu > Choose Character: the same cards, same look.
- If you play in 4:3, the picture should sit where the game's own Options picture sits, black at the sides.
- Say if the rows are too small. They are smaller than the game's own rows so the values fit left of the skull's face.
- The page's help line sits at the bottom of the column. The "Experimental" tag is in the help line on this look.
- At 21:9 (Display > Aspect ratio), the skull and logo should sit exactly where the game's own Options screen has them,
  with black at the sides. Change Aspect ratio on the Display page: the page should follow the new shape at once.
- Turn Fullscreen on and off on the Display page, and drag the window to a new size with a title page open. The
  picture and the list should follow the window at once. (Not tried by Claude yet.)
- Controls > Plays with: Controller, then Button mapping: the same dark panel as with a keyboard, 9 rows at a time,
  with a small red mark under the list while rows are hidden.

**Native menus, step 5 (2026-11-17 morning):** every page opened from player 1's Tome plaques now has
**the Tome look**, played alone (co-op off). The page is a leaf of parchment laid on the Tome's own page, with the
medallion, the bones and the "LB Journal / RB Inventory" hints still showing round it. Names in the Tome's hand, values
in small print, the chosen row in red ink inside a thin gold ring. Pictures: `Screenshots for Claude/s5-tome-skin/`.
Try, with co-op off:
- Tome > Options > Settings: Display, Sound, Graphics (and its four pages), Controls, Button mapping. Each has its name
  at the top, the Tome's ornament under it, and a Back row. B, Esc or Back goes back one page; Back from Settings
  closes it and the plaques are back.
- Display's Aspect ratio in the Tome moves only between 16:9 and 21:9. In 4:3 it is greyed, and its line says to turn
  the game's own Widescreen on first (in the Tome's own Options). That way the Tome's art is always reloaded when the
  game goes in or out of 4:3. Try 16:9 to 21:9 and back: the leaf should follow at once.
- Characters: three portraits to a line, three lines at a time, a scroll bar on the right. Under them, one strip: the
  name, the form, the year, and the three bars. Then Stats and Back.
- Save States: ten slots and Back, the page's line under them. Careful: A on an empty slot saves there at once.
- Mods: Co-op, Challenge, Characters (the two cards fit on the leaf), Cheats, Sanity Effects.
- Button mapping is one list here (the keyboard keys are in the same row). Down scrolls it; a small diamond above or
  below says rows are hidden.
- In 4:3 the leaf sits on the narrower page, clear of the medallion.
- Say if the leaf should be darker, to match the Tome's page, or cover the whole page.
- In co-op: see step 6 below.

**Native menus, step 6 (2026-11-17 morning): the Tome look in co-op.** A Tome page now opens in its owner's half, on
their Tome. The other half keeps the partner's last picture (the world, or their Tome if it is open too), dimmed.
Both players are paused while a page is up, as before. Pictures: `Screenshots for Claude/s6-tome-coop/`.
Try, with co-op on:
- Player 1: Tome > Options > Settings, then Display. The page should sit on player 1's Tome in the left half. Player 2's
  half is dimmed and still. Back twice: the plaques are back, and both players can walk.
- Player 1: Characters. Move round the portraits. Back.
- Player 2: their Tome > Options > Characters. The page sits on their copy Tome in their half, player 1's half dimmed.
- Both Tomes open at once, then player 1's Settings: player 2's Tome stays in their half, dimmed.
- Over and under split (Display > Co-op screen split, since P5): the page should go in the top half for player 1, the
  bottom for player 2.
- Player 1: Tome > Options > Settings > Display, change "Co-op screen split". The page should stay where it is. The new
  split shows once you close the page.
- Say if the page is too small to read in a half on your screen.

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

## 8. Subtitles in co-op (2026-11-17)
A subtitle line shows in both halves while both players are in the same room, and only in the half of the player
who started it when they are in different rooms. Turn **Subtitles on** in the game's options first, or nothing shows.
Play with `play-rooms-log.bat subtitles`, co-op on, in Alex's mansion (chapter 0):
- Player 2 answers the hall phone (room 69) with player 1 in the hall too. "Hello?" and "Remember me, ..." should be
  in both halves, in each one's middle, naming player 2's character.
- The same with player 1 in another room: the lines only in player 2's half.
- Leave the hall with the other player while a line is up: the line should go from the leaver's half.
- Player 1 answers it instead (load a save from before). Both halves in one room; only player 1's half apart.
- One player at the phone, the other at the carriage clock in the next room (or in another room). Each one's words
  and EXAMINE prompts should stay in their own half.
- Try the clock three times with player 2 (Alex's clock lines), and the ghost maid if you reach her.
- Open player 1's Tome first, then have player 2 answer the phone (the game refuses Start once the call has begun).
  Player 2's lines should be in their half at normal speed, "Hello?" then "Remember me, ...", and nothing on player
  1's Tome page. Close the Tome while a line is up and say if the line shows in both halves again.
- Player 1 answers the phone, and player 2 opens their Tome during "Hello?". Player 2's Tome page should show no
  subtitle; player 1's half keeps the lines. Close player 2's Tome mid-line and say what shows.
- If you find a page in player 1's Tome that plays back a recording with subtitles, play it while player 2 walks
  about. Its words should match what is said, at normal speed, on player 1's Tome page. Say if they are wrong words.
- Save a state (F1) while a line is in both halves. The save should wait until the line goes. Load it later: no
  line should be left on either half once the talking stops.
- (Gone since P5: co-op can no longer be turned off in a game. Mods > Co-op was removed; Start Game and Load Game ask
  Solo or Co-op.)
- Say if a line is ever in the wrong half, cut off, left behind after it ends, or in both halves with the players in
  different rooms.
- Since step F2 (2026-11-17): with a line in both halves, player 2's copy should be in the middle of their half, the
  same as player 1's in theirs (it sat a little left before). Check both phone answers. Also say if any of player 2's
  HUD (prompts, the health/sanity/magick bars, the words box) has moved since the last build, mainly while a line or
  a prompt is up in player 1's half at the same height.

## 9. The white flash after the phone call and a sanity effect (2026-11-17)
The flash should be one smooth lavender fade, with no dark triangle from the middle to the top-left corner.
- Player 2 answers the hall phone (room 69). At the end of the call the flash should light player 2's half only, and
  fade evenly. The same with player 1 in another room.
- Player 1 answers it instead: the flash in player 1's half only.
- (Gone since P5: co-op can no longer be turned off during a call.)
- Single player: play a sanity effect to its end (Mods, Sanity Effects). Its closing flash should be smooth too.
- If you can, try the 21:9 aspect setting: the flash should still fill the whole half.
- The first flash after clearing the shader cache may stutter. It should still stay in the right half all the way.
- Say if a flash is ever in the wrong half, leaves a corner of its half uncovered, or still shows a hard edge.
- Since step F2 (2026-11-17) the flash is centred in its half (before, it sat a bit off to one side). It should
  still cover the whole half, with no strip at either edge.

## 10. The HD title picture with the title video off (2026-11-17)
Turn the title video off (Mods) with HD textures on (Graphics), and restart.
- The title should be sharp (her face, the hair, the moons), with Start Game, Load Game, Choose Character and Options
  in the same places as before. Choose Character should look like the game's own words (same thickness and colour).
- Move the cursor up and down a few times. Say if the picture ever turns soft, or if a move feels slower than before.
- The Options list should be sharp too (the skull), with the video on or off. Scroll it to the bottom and back.
- Open Choose Character from the title: the skull behind the cards should be sharp. On a fresh install (with the HD
  pack) it should be there at once, not black.
- Turn HD textures off on the title's Graphics page and back: the title and Options should go soft, then sharp again.
- Say if anything looks patched: a square, a smudge or a ghost of the old Options words behind Choose Character.
- Since the review fixes (2026-11-17):
  - With HD textures OFF, restart, then turn them ON from the title's Graphics page (Image). The skull behind the page
    should turn sharp within a moment, without pressing anything. Then open Choose Character from the title: the
    cards should be on the sharp skull.
  - On a large screen (1440p or 4K, full screen), look closely between "Choose" and "Character": there should be no
    outline of the old "Options" letters. A soft, slightly lighter patch of hair there is expected; say if it shows.
  - If you install a different HD pack, the skull behind Choose Character should match the new pack on the next start.
  - Close the launcher right after it opens: the game should close at once, with no error.

## 11. The first start: "Preparing the game from your disc" (2026-11-17)
- Start the game from play.bat or the launcher as usual. The first time after this change, a page "Preparing the game
  from your disc" should show on the launcher's parchment for a few seconds, with a bar, then the launcher menu.
- Start again: the page should not show.
- On a new PC (or after deleting `%APPDATA%\Eternal Darkness`), the page takes longer (about 2 minutes here, the
  shaders built for the graphics card; about 45 s after the review fix, with anti-aliasing off). Say how long it took, and whether the game looked smoother after it.
- Press B on the page: the launcher should open at once, and the page should come back on the next start.
- Then on the title, open Choose Character before Options: the skull should be behind the cards, not black.
- Say if the page looks out of place, or if the bar ever stops moving for a long time.
- (Added after the review, 2026-11-17.) The page builds only the shaders for your anti-aliasing setting. On a new PC
  with anti-aliasing off it took about 45 s here. Turn 4x MSAA on in the launcher's Options and start again: the page
  should come once more, longer (about 2 minutes here), then not again. Turn it off again: no page, or a very short one.

## 12. The title's sound behind Choose Character and the title Options pages (2026-11-17)
Play with the sound on. Start from the launcher as usual.
- On the title, go to Options, then open Sound. The Options song should keep playing under the page with no break,
  and go on when you press B. Do the same with Display, Graphics, Controls, Mods and Quit ("No").
- On the Sound page, move "Effects & voices": the song's volume should follow it as you move it. "Music" does not
  change the title's song (it is the game's own sound, not disc music). Say if you want that changed.
- Open Choose Character from the main menu. You should hear the A's sound fade, then quiet: the game's own main menu
  has no song (measured). Say if you remember music there, or want one.
- The first page you open after starting may click once. Say if you hear a click or gap, and on which page.
- Press buttons on a page, then close it with B: the title's cursor should not have moved, and nothing should start.
- Leave Choose Character open for more than 3 minutes: the opening movie should not start behind it.
- Keys 1, 2 and F1-F12 on a title page should do nothing (no quick save or load). Also try pressing A on Choose
  Character and F1 at the same moment: the page should open and no save should load.
- Tell us: do you want a song under Choose Character? The game has none there, so it would be one we pick (for
  example the Options song).

## 13. Solo / Co-op picker (2026-11-17)
**Step P7: Exit Game in player 1's Tome.** Start play.bat again (the new build), then try:
- In a game, open the Tome and go to its Options page. Under Settings and Mods there is a fifth plaque, "Exit Game".
  Down from Settings or Mods reaches it, Up goes back. Say if it looks like the other plaques.
- A on it: "Exit the game?" with "No, go back" lit first. Press B, and also try No: you should be back on the Tome
  with Exit Game still lit. Try the mouse and Esc too (only test keys were used so far).
- A again and pick "Yes, exit": the game window should close by itself in a moment. Start again: your settings should
  be as you left them. Progress you did not save is lost, as the ask says.
- In co-op: player 1's Tome has Exit Game; player 2's Tome still shows only Characters. When player 1 asks, only player
  1's half shows the ask. After No, both players should walk again once the Tomes are closed.
- The title's own Quit (Options, Up from the first row) should look and work as before ("Quit the game?").
- Not tried yet: other languages, and 4:3 or very wide windows. Say if the plaques look wrong there.
- Your choice: the Tome's Settings page still has its own "Quit game" row, which quits with no ask. Keep it, remove it,
  or make it ask too?
**Step P6: save states made in the other mode.** Try:
- In a solo game, open the Tome's Save States and choose a co-op save state (slot 3 is one). Its Load row should say
  "Made in co-op: the game starts again in co-op to load it." Press A: the window closes and opens again in a few
  seconds, in co-op, in that save's room, with both players. The other way round works too.
- On the launcher, Load game > a slot made in the other mode: its Load row says "it plays in co-op" (or solo), and it
  loads in that mode with no restart.
- Afterwards, Start Game should still light the mode you picked last on the Solo / Co-op page.
- In a co-op game, Save States > Slot 1 (or Slot 2): those were made in co-op before the separate rooms. Load should be
  grey, with "Made in co-op before the separate rooms: it loads only in a solo game." A does nothing, and the co-op game
  goes on. On the launcher before a game, the same slot says "it plays in solo, without player 2."
- First of all: start play.bat again. Start Game and Load Game should each open the Solo / Co-op page.
- Known, older than this: slot 1 loaded in solo can freeze right after it loads. Say if it does for you.
**Step P5: Mods > Co-op is gone; its rows moved.** Try:
- Mods (the launcher's, the title's Options > Mods, the Tome's Mods plaque) has no Co-op any more. The launcher's main
  list has no Co-op row either. To play co-op, pick Co-op on Start Game or Load Game.
- The title's Mods shows only Challenge and Title video. The Tome's Mods shows Challenge, Characters, Cheats and Sanity
  Effects (no Title video).
- Display (title, launcher or the Tome's Settings) has two new rows under Camera: "Co-op screen split" (Side by side or
  Above and below) and "Player 2 camera". Change the split in the Tome in a co-op game: it should change when you close
  the menu.
- "Player 2 plays with" is only on player 2's Controls page now (Controls > Player II > Plays with).
- Frame rate: before a game you can always set it (it is for solo; co-op plays at 60). In a co-op game it is grey.
  With Co-op picked last, its note says "Co-op plays at 60 frames a second." With Solo picked last, or in a solo
  game, there should be no note on a fast screen.
- Choose Character on the title in solo: player 2's card is no longer dimmed. It says "For co-op: pick it at Start
  Game". In a solo game (the Tome) it is dimmed and says "Solo game: pick Co-op at Start Game".
- Your old settings.ini loads as before: the split, player 2's camera and "plays with" keep what you had.
**Step P3: Start Game and Load Game now ask Solo or Co-op.** You saw no page before: that build was older than this
step. Start play.bat again (the new build) and try:
- On the title, press A (or Start) on Start Game. The Solo / Co-op page should open, the mode you played last lit.
  Pick that same mode: the game should go on exactly as before (the carry-over question if your card has a finished
  game, then the opening). Do the same with Load Game: the save list should come up.
- B on the page: back on the title, the same row lit, nothing started.
- Pick the OTHER mode (step P4): the card says "Starting the game again in ...", the window closes and opens again,
  and the game goes straight on into Start Game or Load Game in that mode. The first time you switch to Solo it shows
  black for about 10 seconds (it makes a new solo title once); after that a few seconds. Say if the window opens in a
  different place, if anything flashes, or if it does not go on by itself.
- While that screen is black, press buttons: nothing should show (no company logos, no movie), and it should still go
  on by itself. The next switch to Solo should then take only a few seconds.
- After the restart, press A on Start Game yourself as soon as the title shows: the game should start in the new mode
  with no Solo / Co-op page again. Later, go back to the title (Return to title screen, or a game over): the title
  should wait for you, never start a game by itself.
- After a restart, open the in-game menu and pick Return to launcher: the launcher should show your disc, and Continue,
  Main menu and Load game should work.
- If you start EternalDarkness.exe by double-clicking it (not play.bat), switching modes should still bring the game
  back. Before this fix it closed for good there.
- Listen as the page opens and closes: the title music should go on, and you should hear one choose sound as the page
  opens, and for Start Game the start sound as you pick (Load Game has only the one, as before). Say if a sound is
  missing or doubled.
Steps P1 and P2 (still to try):
- With HD textures on, open the title's Options > Controls or Choose Character the first time after a start. The title
  music should not skip as the page opens. (If the "Preparing the game" page was skipped, the cards may show the
  smaller disc pictures there until you open a page in the Tome or start again: say if you see that.)
- Controls (the title's Options, the Tome's Settings, the launcher's Options) first shows two cards, Player I and
  Player II, with each player's picture and name. Left/Right moves, A opens that player's controls, B goes back to the
  cards, B again to where you were. Say if it looks right next to Choose Character, and if the cards feel too big or
  too small.
What P1 adds:
- Start from play.bat or the launcher. The "Preparing the game" page may show once, briefly (it makes the new
  pictures). Start again: it should not show.
- Open Choose Character (launcher, title, or the Tome's Options). The **Default** card should show the game's own
  Tome and skull picture, not the Recomp logo. Say if it looks cut off or too dark.
- A new **Random** card sits right after Default, with your silhouette picture. Choose it: the card should still say
  "Random" (it never shows who was rolled). Start a chapter and see who you play. Close the game and start again:
  you should play the same one. Choose Random again: someone new.
- In co-op, player 2 on Default should play player 1's character (Random's roll too).
- With HD textures on, the portraits should look sharper than before. Turn HD textures off: the old ones.
- Known, older than P1: in a co-op game, changing player 1's character from the Tome's Characters page can freeze the
  game after the reload. Avoid that for now, or tell us if it happens.
