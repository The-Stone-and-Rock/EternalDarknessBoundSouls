

## R5, sound: player 1 holds the music; the stayer's room keeps its sounds; player 2's Tome ticks (2026-11-11 night; readers in rooms/sound-readers.md, refuters in refute-r5.md)

### What the readers and refuters settled

- One MusyX engine, one host mixer, one DTK music stream, one output: "player 2 has no music" can only mean the music never
  follows player 2. The DTK music is changed by scripts through seven Lua wrappers (fn168 music_set, fn54 stream_play, fn56
  stream_stop, fn147 music_step, fn57 pause, fn58 fade, fn59 music_wait) whose targets are 0x801A5CB8, 0x801A1750, 0x801A172C,
  0x801A661C/0x801A65E4, 0x801A1990, 0x801A16A0, 0x801A19D0, and by game code keyed to the game (the chapter load's level
  theme, the music step's room-word and story-flag switches from the main loop's sound tick, the sanity tracker on player 1's
  sanity, bosses, the Tome's and insanity screens' resets, level end, front end, cinematics). fn62's door sounds and fn59's
  waits ride the same music queue: refuse at the wrappers' call sites, never at the queue. fn151's duck (0x8019E068(0, 25, 10))
  scales the DTK volume too (0x801A1B50 -> 0x801A0F3C -> 0x8019DE30 -> DTKSetVolume): a music act. A music call behind a
  one-shot flag (L02/2325: flag 802 then fn58/fn56/fn58/fn54) refused outright is lost for the chapter: defer, don't drop.
- The change's sound acts are owner- and room-blind: fn66's 0x801A05EC(len, 1, 0) at the press (lr 0x801668FC the cut,
  0x80166970 the fade; the ticket is written BEFORE both, so g_coop_room_p2 already names the mover), 0x80152298's 0x801A0EAC
  (every playing stream-backed track's slot released, and with 0x801A4B58(-1, -1, 1) ALL four direct stream records stopped) and
  0x801A05EC(10, 1, 0) (all 80 tracks faded), the arrival's 0x8019F3F0 (the new room's emitter table, tagged with the room
  data's own id: right in any bracket) and 0x8019E068(0|1, 100, 0) (two global ducking groups reset). The track update never
  reads a track's room: a kept track of a room not in play keeps playing. Nothing restarts a faded room's emitters. One-shots
  free their track at the start: the 80-track list holds looped emitters, ambience and stream-backed tracks. 0x8019F360(track,
  fade, mode) is the per-track act both walks use; 0x801A05EC's entry and exit write nothing else. Direct stream records carry
  no room (their id at +328, serial +348; 0x801A4B58(id, -1, 1) stops a sound id's records).
- A positioned track's gain is 0x801A01D0(pos, range, min, max, mode) from the view block 0x803012E0's eye and r13-26524's
  position (the louder), at lr 0x8019F230 (start) and 0x801A0310 (each tick; track = r3 - 80, room s16 at +132); the result is
  MusyX's emitter maxVol, and MusyX pans and cuts against its ONE listener. Without a swap a track in player 2's room is judged
  from player 1's eye against a point in another room's frame: an arbitrary loudness, not a reliably quiet one.
- Player 2's copy Tome runs the game's Tome code on a copy of all of MEM1: its 0x8019DB90 (plain sound) calls start voices in
  the copied MusyX state, thrown away at the swap back: silent. The copy's Tome frame also runs the sound tick on the copy,
  which pops the music queue's cells (fn62's door sounds) and the music step's plain sounds: those must not be forwarded or
  they play twice. The copy never runs the Tome's way out (569 close, 509) and the looped item sound 33 (handle r13-24480) is
  stopped only by page code. A nested guest call of the SAME address from its own intercept re-enters the intercept: a running
  guard, as coop_move_sound has.
- The Tome's ids (from the code's context): 611 the cursor tick, 546 select, 495 page turn, 264 refusal, 33 an item's loop,
  568 open, 569 close.

### What was built (ED_COOP_STAY=1 with ED_ROOM_SLOTS=2; nothing without them but the [dtk] lines)

- The music is player 1's: intercepts on the seven targets, taken only from the wrappers' call sites (lr 0x8016A348, 0x80169E94,
  0x80169F8C, 0x8016A280/0x8016A294, 0x8016A03C, 0x8016A13C, 0x8016A1F0; the game's own calls pass) when the script is player
  2's (coop_music_theirs: their change's bracket, player 2 the player (r13-21492 / r13-21496), or a script last run as theirs
  (coop_p2_script_is) resumed after a wait). TOGETHER in one room the call passes (the music is the room's and player 1 is in
  it; logged). APART it is deferred: the last fn168 (track, fade), fn54 (stream) and fn56 (stop) are kept for player 2's room
  (g_music_deferred) and replayed through the game's targets when player 1 arrives in that room (coop_music_replay at the
  arrival's 0x8019E068, lr 0x8015206C, no bracket). fn151's duck (0x8019E068 at lr 0x80164DD4 with r4 25) by player 2 apart is
  passed over; its lift (100) always passes. The [dtk] prepare/cancel lines carry lr, the bracket and the frame.
- The stayer's room keeps its sounds: coop_sound_change_act, first in tw_keep (the one intercept on 0x801A05EC and 0x8019E068):
  at the press (fn66's two sites) and at the change (lr 0x80152354) the mover is g_coop_room_p2 ? 2 : 1 and the kept room the
  OTHER player's (coop_player1_node: player 1's node as the last swap found it, g_coop_p1_node, or the current one); the host's
  own walk coop_sound_fade_rooms does 0x801A05EC(ticks, 1, 0)'s per-track act by guest call to 0x8019F360 for every playing
  track of another room and leaves the kept room's alone. Both in one room: nothing fades at the press. The arrival's group
  reset is passed over for player 2's change (player 1's duck stays). A change of player 2's with no bracket: the game's,
  logged. 0x801A0EAC (its own intercept): with a stayer, only the direct streams of rooms nobody is in are stopped, by sound id
  (g_stream_starts: each 0x801A4884 start remembered with the room in play, from the SANITY_STREAM notify); the mover's old
  room's stream-backed tracks fade through the walk (mode 6).
- A track in player 2's room is heard as theirs: coop_sound_gain (intercept on 0x801A01D0, the two track sites, apart, not in
  the copy's turn): the view block's eye := player 2's drawn eye (camera_player2_view) and r13-26524 := their object for a
  guest call of the game's own function, both restored after; the result handed back. MusyX's pan stays player 1's camera's.
- The copy Tome's sounds are live: tome_copy_sound/_stop (intercepts on 0x8019DB90 and 0x8019DC00, installed with tome_copy_tab):
  in the copy's turn, from the Tome's own code (lr 0x80100000..0x80117000) only, the call is made in the real memory (cpu->ram
  flipped for the guest call; a running guard) and its handle handed to the copy; at the copy's close (tome_copy_close) the
  handles its turn left (r13-24480, -29056, -24108, when they differ from the real memory's) are stopped live and the close
  sound 569 played.
- Logs: [sound] fade / stream drop / gain / music passes|deferred|replayed / tome copy lines, [sound] tracks playing per room
  every 120 frames while apart (the [stay] cadence), [dtk] with frames; run.sh sets ED_DTK_LOG=1; pagelog keeps [sound], [dtk],
  [stay]; coopjudge's judge_door under two_rooms: 'music untouched' (no [dtk] line in the door's window), 'door fade by owner'
  (the [sound] fade line), the kept count as info; the 'same room' check only without the stayer.
- Not in R5 (said so): a per-side mix; MusyX's pan for player 2's room's emitters; sanity music and the insanity screens (the
  game's); player 1's Tome in the copy (g_tome_copy_p1) keeps its sounds as before; more than 80 looped tracks across two rooms
  (the free count is in the tracks line); the music refusal is exercised in play (chapter 1's doors set no music).

### The runs
