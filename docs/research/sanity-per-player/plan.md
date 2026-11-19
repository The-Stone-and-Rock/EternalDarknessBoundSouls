# Each player's own sanity effects: what is built, and the plan for the rest

Read from the game's code by four readers on 2026-11-18 (nothing here was run unless it says "seen"). The status is on
the board in `docs/coop-next-session.md`, items 12a and 12b; the memory map of one player's sanity is in
`docs/game/systems/sanity.md`, "Co-op: each player's own effects". Code: `hle.c`, `sanity_p2_step` and the functions
round it (`SanityOwn`, `sanity_set_io`, `sanity_level`, `sanity_later`, `sanity_overlay`, `kSanityP2`, `sanity_rules`).

## Built and seen (2026-11-18)

- Player 2's own set, the game's tracker update run once more a frame as them, at the pad read (`coop_pad_read`), so a
  hold of either player's does not stop it.
- Their level from their own meter; the game's own clock and roll for them (seen: TV Mute, then Bleeding Walls, 3600
  ticks apart, player 1 at rest).
- Their overlay in their half alone: TV Volume, TV Mute, TV Shut Off (the whole half black) seen in pictures.
- An effect of theirs that holds play (TV Shut Off) holds THEM alone: the busy bit it sets is in the game's word only
  during their update (`g_sanity_p2_busy`); player 1 walked through it.
- An effect of theirs that goes on in the background (Blood Drip seen) ends when they come into another room, as the
  game ends player 1's at a door.
- Allowed for them now (`kSanityP2`): 1, 4, 5, 6, 7, 8, 24, 31, 38. Not seen in a picture: 1, 5 (blood: not found in
  the rooms tried), 6, 7, 8, 38.

- Late 2026-11-18: steps 1 to 3 of the door plan below are BUILT for stay mode, by the whole-bracket way
  (`sanity_p2_door`), with 14 Bug Crawl seen at player 2's own door. Next in that plan: 10 and 21, then step 4.

## Lessons of the build (do not repeat)

- In split-screen a flat picture is player 1's as drawn and player 2's when moved across by `coop_subtitle_away(cpu, 2)`
  (about 4096) on the identity the game's 2D setup loads (0x8019CFD4, lr 0x8019D068). A view mark on that identity hid
  player 2's overlay from BOTH halves. A quad made half again as wide vanished from both halves too; 1.16 from its left
  edge covers a wide half.
- `split_on()` means the door's countdown, not "the halves are shown": that is `camera_split_on()`.
- 0x801F57C0 (an event for later) already has the intercept `split_send`: player 2's tracker's own sends are taken at
  its top (`sanity_later`).
- The holder's +8 and +12 are always set (records 0 and 2, always on): "at rest" is state 1 with +4 == 0.
- The step must not run from player 1's turn: an effect that holds play stops that turn, and the effect then never ends.

## Next: the effects the game starts at a door, for a door player 2 takes

How the game does it:
- A door-started effect is PICKED by the roll (state 41, the record's flags +24 bit 2 set) and then WAITS: nothing is
  sent. While waiting (event 3 in state 41, 0x8006D4D0) it is dropped with event 82 when the roll timer reaches 8000
  or when the rules 0x8006EB6C say no: so today `sanity_rules` drops any waiting effect outside `kSanityP2` on player
  2's next update. Door effects must be added to `kSanityP2` or they never wait.
- Before the load, 0x8004305C(next room) (called at 0x80152640): finds the tracker by the CURRENT PLAYER's
  info+140 -> +68 (the tracker's id; r13-21496 -> 0x801F62B8), reads holder+4's record with stage 5, may ask for 12 or
  22 itself, and if an effect waits calls 0x8006BE04(player node, tracker): the return record at data+72 (room, place,
  angle, four stats), r13-25496 = 1.
- After the load, 0x80043380(effect, next room, marker, camera) (at 0x80152748, only if r13-26524 != 0 and r13-26280
  == 0): finds the holder by first-of-kind-32, checks the room's list, the gate 0x8006DD70, sets the room up
  (0x8006C008, which acts on r13-26524 and fills 0x80319578) and sends event 84 at once: state 42.
- The return is event 81 (not 82), sent by an effect's step to the watched character's info+140 -> +68. State 42 on 81
  -> 0x8006C850: flash 0x8006B270, then 0x8006B438 -> 0x8006BFAC: a ticket with transition 17 back to the kept room,
  whose tick calls 0x80152298 directly (NOT the runner 0x80152614 the co-op bracket hangs on), then the callback
  0x8006B45C: 0x8006B7B8 puts holder+56's character at the kept place and sets four stats.
- Returning effects (switch 0x80244294 -> 0x8006CC08): 9, 11, 12, 13, 15, 16, 17, 18, 19, 22, 23, 29, 32, 33, 35. Not
  returning: 10, 14, 21.

Co-op today:
- Stay mode (the user's): player 2's door runs inside `notify_ticket_run` (opens at 0x80152614's entry: coop_be_player2,
  their room word, their cameras) until `coop_stay_change_end`. Player 2 IS the player at both game calls.
- Follow mode: the runner runs as player 1; player 1's waiting effect starts at player 2's door today.
- Missing link: player 2's info+140 -> +68 is never set (0x800CBC38, its only writer, is called once at 0x80007B78 for
  the chapter's character), so 0x8004305C finds no tracker for them and their effect's 81 goes nowhere.

Plan, each step one run:
1. Player 2's +68 := the tracker's id while their set is in (restore after, or their death's 81 at 0x800C05CC reaches
   player 1's set).
2. Make the hook on 0x8004305C an intercept (today `sanity_doors_notify`, a solo notify) and extend `sanity_door_rules`
   (already the intercept on 0x80043380): when the door is player 2's (`g_stay_bracket`), their set in, their +68,
   `g_sanity_p2_in` on, the game's call, undo. As `sanity_overlay` does it.
3. First effect: 14 Bug Crawl (a 2D effect object, draw 0x80192F10, loads its own identity at lr 0x8019D068: the same
   move across; no return; one-shot runner 0x8006BAB0). Then 10 Locked in Room and 21 Phantom Creatures (no return;
   both players see them in a shared room).
4. Then the ones on the player alone, which return: 32 Shrink, 33 Expand (scale 0x800BF934(obj, 15), r13-25380 /
   -25384), 35 Sinking, 11 Blood Bath. Needs: a bracket on the transition-17 tick with the callback run in player 2's
   set and as them, a guard in `sanity_p2_step`, and `SanityOwn` extended with 0x80319578 (128 bytes, it starts exactly
   where the screen object's copy ends), 0x80319608, 0x803196C8, r13-25380, -25384, -25508, -25512 (sizes not checked).
5. Off until read further: 22 (its own room change), 13 (the other-era room), 23 (body swap), 29 (sets the game's busy
   and an overlay), 27 (saving: off in co-op by the user's decision), 12 and 30 (the story's); 9, 16, 17, 18, 19 until
   their spawns are traced.
Risks: follow mode has no bracket; a return ticket written in player 2's update sets `g_coop_room_p2`; the event 62
after a load reaches whichever set is in.

## Next: the screens that take over the whole game, for one player (the user's own example: a fake blue screen for
one while the other plays on)

How the game does it:
- The mode table is at 0x80238810, 8 bytes a mode {enter, frame}. Modes 7, 8, 10, 11, 12, 13 share the enter
  0x80112D98; each frame function is step + draw + the frame's own begin, end and present in one. No separate draw.
- The enter takes the room's data buffer (r13-24316), runs the Tome's enter 0x80111078, loads UInsane*.cmp (1.39 MB,
  by language) OVER THE ROOM, sets picture r13-24136 = 2, state -24148 = 0, stops the music. The exit is the Tome's
  own 0x80110DA8 (the room read back, mode r13-24244 = 2).

| Mode | Frame | Effect, asked at | What it shows | Ends |
|---|---|---|---|---|
| 7 | 0x80112F24 | 26 Game Reset (0x80006E30) | picture 2; tick 3 starts stream 0x801A1750(7,0,0,0) | tick 150 |
| 8 | 0x801132F4 | 25 TV Video (0x80006DFC) | black rect 0x8019C8FC(-106,0,853,480), green "VIDEO" at 455,31; sounds at ticks 46, 140 | tick 300 |
| 10 | 0x80113590 | 28 Blue Screen (0x80006E64) | picture 6; sound 583 at tick 1 and at each press | 6 presses or tick 300 |
| 11 | 0x801138D0 | 15 To be Continued (step 0x8009CA84) | picture 7, crossfade to 8 at tick 200 | 200 ticks later |
| 12 | 0x80113B84 | the idle screen, not an effect | picture 9 | any input |

- Their common end: 0x80110DA8, event 81 to the tracker, 0x8006B270(holder) (the flash, sound 536, the character's
  line), the music back. 20, 25, 26, 27, 28 end with the record's steps only (0x8006CB1C), no room change. 15 returns
  by transition 17.
- The pictures are ONE full-screen quad (0x8019D470 / 0x8019D4EC): 2.0 x 2.0 clip units, which the split never brings
  back into a half ("screenWide"). The move across works only for the text.

Design that fits what exists (not run; the Tome copy is the only thing that runs a whole-game screen for one player):
1. 25, 26, 28: a "screen copy": the Tome copy's turn (`tome_copy_*`, hle.c near 16621) with the asked mode 8, 7 or 10
   in place of 3. Opened at the owner's Start press when their set has the effect waiting (state 41).
2. In the copy: their sanity set in, event 84 sent, r13-26188 = N, 0x80018728; each turn the mode's frame ([r13-26192])
   and the five after-frame functions the Tome copy calls (0x801DC9F8 moves the timer).
3. End: intercept 0x80110DA8 in the copy's turn and return at once (it spins on load flags); close after that frame.
4. After the close, in real memory: their set carried out of the copy, then 0x8006B270(holder) as them for the flash
   and the line, in their half.
5. Hold: the copy's open flag is already a term of `coop_player_held`; their pad is read only by the copy.
6. Sound: the copy's sounds replayed live as `tome_copy_sound` does; the stream calls 0x801A16A0, 0x801A172C,
   0x801A1750, 0x801A17C4 made to do nothing in the copy.
7. Player 1: the same copy (the `g_tome_copy_p1` pattern), their waiting effect answered "none" at lr 0x80006D54, or the
   real Start switches the whole game for both, as today.
8. 15 To be Continued: the same copy with mode 11, flag 837 set in real memory and the effect dropped from their set
   with no 81 (no return). Or left whole-game: it comes once, at a chapter's start.
9. 20 Empty Inventory: its start sets bit 2 (value 2) of 0x80335758; the inventory page counts in r13-24532 (event 84
   at 1; at 300 it clears the bit, rebuilds, sends 81 and flashes). Player 2's copy Tome zeroes 0x80335748..+24 at its
   opening, so it never shows for them and their arming would leave the bit for player 1. Keep the bit and r13-24532 in
   `SanityOwn`; at their copy's opening put their set in the copy and set the bit when 20 waits.
10. Add 20, 25, 26, 28 to `kSanityP2` when each works.
Fallback if a copy misbehaves: draw the screen as an overlay in real memory (UInsane unpacked into 1.39 MB of its own
guest memory, bound with 0x801E1B74 and the bank before put back, a host timer, drawn last in the frame before
0x8014FA84 at 0x800085A0), the full-screen quad with its matrix marked out of the other view (whole words 0x0C8000D2,
0x0C8000A0 + view, 0x0C8000E5 at +12/+28/+44, as `r4_fade_quad`), the text with the move across.

## Also open

- World effects in a shared room (blood 1 and 5: effect objects made by 0x801413E8 at a room marker, drawn through
  0x8017C1D8 by 0x8017E938 / 0x8017E274 / 0x80190628): kept to their player's half with marks on matrices 27 and 60 as
  `effect_draw_by_room` does. The objects made in player 2's update have to be noted as theirs when made.
- Sound: a mute (0x8019DCDC(5,0,0)) or a volume fall is for both; the sanity music follows player 1's meter (the
  user's earlier wish: each player's on their own side).
- Save states do not carry player 2's set. The Mods menu's Sanity Effects is solo only. Events other code sends to the
  tracker outside player 2's update reach player 1's set.

## The user's reminders (2026-11-18, late): do not leave these out

- **The body falling apart, and the head that can be picked up and recites** (Head of Shakespeare, 17; "the body
  breaking apart" has no name of its own in the table, see sanity.md: it may be 17 or another's variant, to be seen
  forced). Setup 0x8009A7AC, step 0x8009A8D4 (body animation, a stream, event 53). Door-started, on the player and the
  room, and it RETURNS (event 81, transition 17 back to the kept place). Needs step 4 of the door plan above (the
  return's bracket for player 2) and its parts' spawns traced, so the parts and the head are that player's.
- **Walk on Ceiling** (22), only where the chapter calls for it: the game starts it only at a door into its own ten
  rooms (30, 105, 151, 161, 167, 243, 248, 253, 255, 257: 0x800689D0(room, 1)), the door's own look asks for it
  (0x8004305C -> 0x80068770). Setup 0x8009B520, step 0x8009B760: it writes its OWN room change (the ticket
  0x80155770, 0x801E4610) and returns. For player 2: their door's bracket already has their set in; its own ticket
  and the return need the same care as step 4. Keep the game's room rule: never start it elsewhere.
- Built since this plan was written: TV Video (25) for player 2 by the host doing mode 8's work (board, 12a). Blue
  Screen (28, mode 10 0x80113590: picture 6, sound 583 at tick 1 and at each press, 6 presses or 300 ticks) and Game
  Reset (26, mode 7 0x80112F24: picture 2, stream at tick 3, 150 ticks) go the same way once UInsane's pictures are in
  memory of their own (1.39 MB, loaded by 0x801562B8, unpacked by 0x80156924, bound with 0x801E1B74; the picture drawn
  by 0x8019D470(n, alpha, 0) is ONE whole-screen quad: draw it as two halves or with a marked matrix, or it is lost
  from both halves as the single black rectangle was).

## 2026-11-18 (night, second session): what was built, and what the four new readers found

Read these before the next step. Each is a spec read from the game's code, marked READ or GUESS:
- `read-blood-bath.md`: effect 11, the fake death (0x8009E218, 0x800C04EC), the parts, the prompts-off bits.
- `read-ceiling.md`: effect 22. Room + 1000 is a real room file; the runner's bracket takes its ticket; the test is
  Jump to Game 5 (Max), 249 -> 252 -> 256 -> 257 by three plain doors (its command is in the file). To do first: 22 in
  `kSanityP2` with the forced start kept off it, the black of VISetBlack kept to their half, the guard on a counting
  ticket, `sanity_later`'s frames-as-seconds.
- `read-screens.md`: Blue Screen (28) and Game Reset (26). No 1.39 MB load: pictures 2 and 6 are 153,600 bytes each
  in `/UInsane.tpl`, read with `dvd_read_to_guest` as `tome_plaques_bank` does; drawn as two half-height quads with
  0x8019D084. Reset's stream 7 is `/audio/music/poe.adp` (sound only). Start's code 0x80006CDC.
- `read-shared-room.md`: phantoms (21) are real creatures kept in the set's data +188/+252 (count +85), their quarry
  flag 0x1000 is "the current player"; blood (1, 5) is made in two stages, the second in the frame's drain from
  PLAYER 1's bytes (so player 2's running blood is probably never made). What each other allowed effect still shares.

### Head of Shakespeare (17) for player 2: DONE apart, seen in pictures
- The game offers the head (PICK UP) only while 17 or 9 is "the effect running" (0x800DA688 asks 0x8006A3A8(holder,
  0x20200)); in player 2's turn that read PLAYER 1's set. `sanity_is_on` / `sanity_is_on_n` (intercepts on 0x8006A3A8
  and 0x8006A32C) answer from player 2's own kept set whenever player 2 is the player and their set is out. This is
  the general rule for all 24 callers (behaviour under an effect, the pickup, the door code).
- The head fell through the floor when apart: `part_step_own_room` (the guard; `docs/coop-failed-attempts.md` has the
  whole story and what is NOT known).
- The pickup runs with their set and link in (`sanity_pickup`, built before); the other player's press on a head is
  refused. Seen: PICK UP in their half, B, the head in close-up in their half reciting with its subtitles there,
  player 1 walking in room 100 throughout, the return (logs/run-20261118-195634-jtg-sanity2-head-final; pictures
  08, 08b, 09 in Screenshots for Claude/sanity-per-player/).
- The game's prompt of that find (REPLACE HEAD) is put in their half while their find is open (`g_sanity_p2_head_pick`
  in `coop_close_up_prompt`), and its slot is cleared at their return if the effect ended with the head still held.
  NOT seen yet: both of these (built after the last picture).
- NOT done: with BOTH players in the effect's room, player 2 stood in the gate's examine region and B read the gate
  (so the pickup together is not seen); player 1 sees player 2's head and headless body (the shared-room step);
  player 1's half is dimmed for the find's first 20 frames; the recital goes on after the return (not compared with
  single player); the head's cleanup in a room that stays loaded.

### Blood Bath (11) for player 2: DONE apart, seen in pictures
- The death that ends it is asked of the one tracker (0x8009E218) from the dying character's handlers and answered for
  player 2 from their own kept set (`sanity_fake_death`, the asker by return address); its 81 is kept for their next
  update and the body made whole (`sanity_death_ends` on 0x800C04EC). Before: a real death, the game over for both.
- The two prompts-off bits it sets (64, 256 of r13-24048) are theirs alone, as the busy bits are (`g_sanity_p2_prompts`).
- Their set starts with data +156, +160, +164 at -1 and +148 at 0.
- Seen: started at their door, the head off at 115 frames (picture 10's first frame), the death at 1255 frames "yes, not
  real, by their own set", the return as theirs, player 2 whole beside player 1 in room 100, player 1's half on room 100
  and player 1 walking throughout, no game over (logs/run-20261118-202252-jtg-sanity2-bloodbath5; picture 11).
- NOT seen: the body in pieces close up (the gate's words box covered it in the test's spot, below); what they held put
  back in their hands after the return (read-blood-bath.md section 7); the reader's step 6 (event 8 swallowed while the
  game is busy) is NOT built: if their Blood Bath never ends in a busy moment, start there.

### A fault of the two-room code found on the way (FIXED): player 2's plain words while apart
- `coop_words_tick` (their words' own step, at the main loop's 0x800084FC) made THEIR room the room in play each frame
  and put it back only for a close-up's words. With plain words (a region's ed12: the gate of room 51, which Blood
  Bath's stagger and Shrink both walked player 2 into) the room in play stayed theirs: the players read as together,
  `coop_follow` brought player 2 to player 1's room, player 1's half showed player 2's room and went black after the
  return. Last night's Shrink run has the same lines (logs/run-20261118-183045-jtg-sanity2-shrink).
- Fixed: put back after the step, whatever the words. And `room_in_check` (once a frame, from GXSetDrawDone) now says
  "[rooms] a room made current for a call was not put back ... lr 0x..." and puts it back if any other place does it.
- Also: the step's own region look is passed over for player 2's body inside their sanity update (`coop_scan_player1`,
  `in_their_update`): their own look runs the region a frame later, in their turn.

### Walk on Ceiling (22) for player 2: DONE, seen in pictures (Max's chapter)
- Built as `read-ceiling.md` says, items 1, 2, 3, 4, 5, 8: 22 in `kSanityP2`; never forced (`sanity_door_rules`); no
  update of theirs while a room ticket counts; the black of its change is their half alone (`sanity_black` on
  0x801E4610, drawn in `sanity_overlay`); its own change into the second room is not "another room"; the ten rooms'
  once-a-room bits start at 0 in their set.
- Seen, first run, nothing forced: Jump to Game 5, player 2 through 249 -> 252 -> 256 -> 257 with their meter at 10; at
  the third door the game's own rule starts 22 for them, their half black for 54 frames, the change to room 1257 as
  theirs, the room upside down in their half for 1500 frames, the 81, the return to room 256 at the door they pressed;
  player 1 at Max's desk untouched, never black (logs/run-20261118-203109-jtg-sanity2-ceiling; picture 12).
- NOT built: item 6 (the early end on B at a door reads PLAYER 1's shown prompt), item 7 (`sanity_later` counts frames as
  seconds: 22's 81 came 1 frame after it was sent, not 50), item 9. NOT seen: player 2 walking on the ceiling (the test
  stood still); player 1's own 22 in co-op still blacks both halves.

### Game Reset (26) and Blue Screen (28) for player 2: DONE, seen in pictures
- Built as `read-screens.md` says, steps 1 to 6: `sanity_screen_bank` reads pictures 2 and 6 of `/UInsane.tpl` once
  into host scratch (SANITY_BANK 0x82320000); the press, the ticks and the end are in `sanity_p2_step`'s screen block
  (one block for 25, 26, 28); the draw is two half-height quads in `sanity_overlay`. Both refused if the file is not
  as expected.
- Seen, first run: the blue "BootOS ... fatal exception" screen in their half alone, ended by their presses after 194
  ticks; then the Poe quote screen in their half for 150 ticks; the flash after each; player 1 walking throughout
  (logs/run-20261118-203524-jtg-sanity2-screens; picture 13).
- NOT heard (runs are muted): the beep, the narration. By the game's own design the narration is the music stream:
  both players hear it and the room's music stops for those 150 ticks, then comes back. `[DECIDE]` if the user wants
  the picture alone with the music left (drop the stream calls in the 26 branch).
- NOT built: step 7, player 1's own (their Start with 25, 26 or 28 waiting still switches the whole game for both).

### Phantom Creatures (21) kept to their player in a shared room: DONE, seen in pictures
- `g_phantoms` (hle.c, beside `coop_found_object_draw`) is filled from both sets by `sanity_phantoms_note` (after their
  update and as a door of theirs ends); each one is left out of the other player's half (`coop_found_object_draw`) and
  told its owner is the player it looks for (`coop_interface_player`, lr 0x8003256C).
- Seen: both players in room 51, five phantoms of player 2's in player 2's half closing on player 2, none in player
  1's half (logs/run-20261118-203810-jtg-sanity2-phantoms-shared; picture 14).
- NOT built: hits across owners (read-shared-room.md step 4: player 1 can still be hit by, and hit, a phantom they
  cannot see), sending them away per owner (step 5), the shadow of a hidden phantom (step 6).
- NOT built at all: blood (1, 5) per player, steps 7 to 12. The reader's finding stands: player 2's running blood is
  probably never made (stage 2 is made in the frame's drain from player 1's bytes). Start there.

### Next
1. Blood per player: `read-shared-room.md` steps 7 to 12. **Pick the test room first.** Room 100 of Alex's last chapter
   shows no blood from its camera even in SINGLE PLAYER with Blood Pool running (logs/run-20261118-204922-jtg-blood5-solo:
   "5 Blood pool, ceiling drip running" at 2538, nothing in the picture to 3082), and none for player 2 in co-op
   (-204710-jtg-sanity2-blood-look), so neither run says whether player 2's blood is made. Take the single-player
   reference in a room with a close camera (Max's bedroom 249, Jump to Game 5, is one), then the same room in co-op,
   BEFORE building: the lesson of the head (docs/coop-failed-attempts.md, last section).
2. The phantoms' hits across owners, and the head and body parts of one player hidden from the other (the same list
   and draw rule as the phantoms: `g_phantoms` could hold them).
3. The pickup of the head with both players in the room (put player 2 clear of the gate's examine region first).
4. Player 1's own screens and Walk on Ceiling's black in co-op (they still take both halves).
5. `sanity_later`'s frames-as-seconds, the early end of 22 on B, Blood Bath's swallowed event 8.

## 2026-11-18, late night: the user's two rules, and the sweep in Max's chapter

- **Test sanity effects in Max's chapter** (Jump to Game 5, the start `max-249`): the cameras are close. The exact way to
  run from its state is on the board, rule 8. **After an effect a player is never moved to the other player**: rule 9.
- What the sweep showed is the board's 12e; what it found wrong, and how it was put right, is 12f: "B REPLACE HEAD" is the
  words of the head's own window (record 0x8030A704), not a prompt; its close is 0x80024818.
- Shrink (32) does not end while its player stands still (the same in single player: sanity.md, "stays on with no one
  walking"); a door taken while shrunk sends the host's event 82, which the game ignores for it. With walking it ended
  after 740 frames. So a test of 32, 33 or 35 walks.
- Blood: still not built. Take its reference in Max's bedroom first (`ED_SANITY_TEST=1,...` and `5,...` in single player
  from Jump to Game 5, then the same two as player 2 from the state).
