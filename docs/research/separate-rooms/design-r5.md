# R5 sound: player 1 holds the music; the stayer's room keeps its sounds; player 2's Tome ticks (design 2026-11-11 evening)

Base: the uncommitted hle.c with R1-R4 (ED_ROOM_SLOTS=2, ED_COOP_STAY=1: two resident rooms, the stayer, each half its room,
a door of either player's while the other stays). Readers: rooms/sound-seed.md (the first sound reader) and rooms/sound-readers.md
(five readers of 2026-11-11 evening). The user's rule: player 1 holds the music; every in-game effect stays for both (one output).

## The facts the design stands on (all from the readers, with their evidence)

F1. The DTK music is changed only by scripts through SIX Lua wrappers (fn168 0x8016A2C4 music_set, fn54 0x80169E10 stream_play,
    fn56 0x80169F3C stream_stop, fn147 0x8016A20C music_step, fn57 0x80169FA4 pause, fn58 0x8016A058 fade) plus fn59 0x8016A160
    (music_wait: a cell that holds the one music queue), and by GAME code keyed to the game, not a player: the chapter load
    0x80007E4C (level theme), the music step 0x801A5E3C (room word and story flags, from the main loop's sound tick), the sanity
    tracker 0x800AC9C4 (from 0x80069018 on the main loop's sanity clock: player 1's sanity), sanity effect streams 0x8006C008,
    bosses 0x800DE148/0x800E01A8, the Tome's and insanity screens' resets, level end, front end, cinematics. No host hook exists on
    any of it today. fn62 (door sounds) and fn59 ride the SAME queue (type 8/9 cells): refuse at the wrappers, never at the queue.
F2. The host knows a script's owner by r13-21492 == g_coop_p2 (coop_script_cameras 17785, coop_scene_set 16470: `coop_use_theirs()
    || r13-21492 == g_coop_p2 || r13-21496 == p2's id`), and inside R4's bracket (g_stay_bracket != 0) coop_be_player2 holds so
    player 2's arrival script at 0x801527F4 has r13-21492 == g_coop_p2. A waited script of theirs resumes as theirs only through
    coop_p2_script_is (18027-18035), then the use predicate holds; otherwise it resumes as the game's (player 1's).
F3. The change's sound acts are owner-blind and room-blind: fn66's 0x801A05EC(len,1,0) at lr 0x801668FC (cut) / 0x80166970 (fade)
    at the PRESS, in the door script's own run; 0x80152298's 0x801A0EAC (lr 0x80152344: every playing stream-backed track's slot
    released) and 0x801A05EC(10,1,0) (lr 0x80152354: all 80 tracks faded, mode 6 over 20 for stream-backed with a handle, mode 4
    over 10 for the rest, (track,0,4) for stream-backed without); on arrival 0x8019F3F0(room data) at lr 0x8015205C (the NEW room's
    emitter table started, tagged with the room data's own id: right in any bracket) and 0x8019E068(0,100,0)/(1,100,0) at lr
    0x8015206C/0x8015207C (two GLOBAL ducking groups reset to 100: undoes fn151's 25). The track update 0x801A0398 never reads a
    track's room (+132): a track of a room not in play keeps playing and updating. Nothing restarts a room's emitters once faded
    (0x8019F3F0's three callers: arrival, the Tome's return, 0x800270D8). One-shots (state 2) free their track at the start:
    the 80-track list holds the looped emitters, ambience and stream-backed tracks only. tw_keep is already the ONE intercept on
    0x801A05EC and 0x8019E068 (and 0x80152298); 0x801A0EAC has no entry yet; 0x8019F360(track, fade, mode) is the per-track
    primitive both walks use.
F4. The game's gain per positioned track is 0x801A01D0(pos*, f1 range, min, max, mode) -> u8, called with the track's own fields at
    lr 0x8019F230 (start) and 0x801A0310 (the per-tick update; track = r3 - 80, room at +132) and for the 4 MusyX streams at lr
    0x801A47FC/0x801A4A1C (record = r3 - 332, no room). It reads the camera eye from the view block 0x803012E0+0..8 (0x801EB01C)
    and r13-26524's position (+250 up) and keeps the louder. The result (+136) is MusyX's emitter maxVol; MusyX then pans and
    cuts against its ONE listener (r13-22456, fed by 0x8019EDD4 from the same block, maxDis 50000: a small cut inside a room).
    camera.c's camera_player2_view(eye, target) gives the eye player 2's half is drawn from, every frame, every mode.
F5. Player 2's copy Tome runs the game's own Tome code on a COPY of all of MEM1 (tome_copy_swap flips cpu->ram between
    g_tome_copy.copy and .real); its 0x8019DB90 (plain sound) calls are reached but start voices in the copied MusyX state, which
    is thrown away: silent. 0x8019DB90 has no host handler; 0x8019DC00(handle) is the plain stop (the looped 33 of an item under
    the cursor, 509, 688 keep handles). The hush (0x801A0F30/0x801A05EC/0x801A4FCC/0x8019E068 at the Tome's open) is already
    skipped by tw_keep for every co-op Tome. tome_copy_tab already makes a nested hle_guest_call inside an intercept in the copy's
    turn. The Tome's ids: 611 cursor tick, 546 select, 495 page turn, 264 refusal, 33 item loop, 568 open, 569 close.
F6. The host prints nothing about the change's sound acts or the music; run.sh doesn't set ED_DTK_LOG; pagelog's KEY keeps no
    [sound]/[dtk] line; the kit has no sound column. judge_door's 'same room' check contradicts the stayer by design.

## The build

P1 THE MUSIC IS PLAYER 1'S (every co-op run with the stayer on: coop_stay()). Intercepts on the seven wrappers (byte-checked
   `stwu r1` first word as the other script intercepts are): if coop_music_theirs(cpu) -> gpr[3] = 0 (no results), passthrough
   false, tail false, `[sound] music fn%d (script %d) by player 2: passed over (frame)` under ED_COOP_LOG; else passthrough.
   coop_music_theirs = mod_coop && g_coop_p2 && sda_ready && coop_stay() && (r13-21492 == g_coop_p2 || r13-21496 == p2's id ||
   coop_use_theirs() || g_stay_bracket != 0). fn196 (read) and fn62 (door sounds, the user's rule) untouched; the game's own music
   (F1) untouched. Gated on coop_stay() so the rule lives with the separate-rooms mode; the switch name stays ED_COOP_STAY.
P2 THE STAYER'S ROOM KEEPS ITS SOUNDS. One helper coop_sound_fade_rooms(cpu, keep_room, ticks, stream_mode): the host's own walk
   of the 80 tracks at 0x805D80C0: a track with +143 != 0 and +132 == keep_room is left alone; any other playing track gets
   exactly 0x801A05EC(ticks,1,0)'s per-track act by guest call to 0x8019F360: +146 == 0 -> (track, ticks, 4); else +116 != -1 ->
   (track, min(ticks+10,127), 6); else (track, 0, 4). Logs `[sound] fade at <site>: mover %d keeps room %d: %d tracks kept, %d
   faded (frame)`. The stayer and their room: coop_stay_stayer(cpu) and coop_object_room (the mover is the other).
   - fn66's fade (lr 0x801668FC / 0x80166970, inside tw_keep's 0x801A05EC intercept): when coop_stay() && g_coop_p2 && sda_ready:
     the mover is player 2 if the script runs as theirs (coop_use_theirs() || r13-21492 == g_coop_p2) else player 1; keep the
     OTHER player's room; replace the call (len from gpr[3], as the game passes it: the cut passes 0 -> (track,0,4) for all but
     the kept). When both are in ONE room, the kept room is that room: nothing fades at the press (the mover leaves it; the
     stayer stays); the mover's new room's emitters start on arrival as the game does.
   - 0x80152298's pair: 0x801A0EAC (a new entry, intercept) skipped when a stayer keeps a room (coop_stay() && g_coop_p2 &&
     the stayer is resident: coop_stay_apart || g_stay_bracket != 0 || both in one room); the stream-backed tracks of the mover's
     old room still fade through the walk (mode 6, their handle stopped by the state machine 0x8019F7A8 at the fade's end).
     0x801A05EC at lr 0x80152354: the walk with keep_room = the stayer's room, ticks 10.
   - 0x8019E068 at lr 0x8015206C/0x8015207C: skipped when g_stay_bracket != 0 (player 2's arrival must not undo player 1's duck);
     player 1's arrival keeps the game's reset (player 2's duck, if any, is theirs to end).
   - 0x8019F3F0 at lr 0x8015205C: untouched (the new room's own emitters, tagged with its id; the duplicate check refuses a
     second copy when the other player is already there).
   - When the stayer is NOT resident (coop_stay off, or no player 2): the game's calls pass as today.
P3 THE GAIN OF A TRACK IN PLAYER 2'S ROOM IS THEIRS. Intercept on 0x801A01D0 (running guard): when coop_stay_apart(cpu) and lr is
   0x8019F230 or 0x801A0310 and the track's +132 == coop_object_room(cpu, g_coop_p2): save 0x803012E0+0..12 and r13-26524; write
   player 2's eye (camera_player2_view) into the block and r13-26524 := player 2's object (node+52); guest-call 0x801A01D0 with the
   same r3..r6 and f1; restore; gpr[3] = result; passthrough false. Streams (no room) and tracks of player 1's room: passthrough.
   The pan stays player 1's camera's (one MusyX listener): accepted, noted for play. A rate-limited log line (first 8, then every
   256th): `[sound] gain of track %d (sound %u, room %d) as player 2: %u (frame)`.
P4 THE COPY TOME'S SOUNDS ARE LIVE. Intercepts on 0x8019DB90 and 0x8019DC00 (byte-checked): if !g_tome_copy_turn -> passthrough;
   else cpu->ram = g_tome_copy.real; hle_guest_call(same address, gpr[3..4], 2 / 1, 0.0, &out); cpu->ram = g_tome_copy.copy;
   gpr[3] = out; passthrough false, tail false. (A raw ram flip, not tome_copy_swap: no GX record copy; the renderer's turn stays
   the copy's.) The copy keeps the LIVE handle so its later 0x8019DC00 stops the live voice. Log under ED_COOP_LOG: the first 8
   forwards `[sound] tome copy: sound %u at %u forwarded live (frame)`. Nothing of the hush is forwarded (F5).
P5 LOGS AND THE KIT. run.sh sets ED_DTK_LOG=1; hle_DVDPrepareStreamAsync/Cancel print `[dtk] prepare|cancel from lr 0x%08X,
   bracket %d (frame %llu)` under ED_COOP_LOG; pagelog's KEY gains `^\[sound\]|^\[dtk\]`; coopjudge's judge_door: the 'same room'
   check only when the stayer is off (plan two_rooms false); a new check under the stayer: in a door's window no `[dtk] prepare|
   cancel` line, and the `[sound] fade` line's kept count for the stayer's room > 0 when that room had playing tracks (reported
   as info when 0: a room with no looped sound). both.tsv gains no column (the proof is in the lines).

## What is NOT in R5 (said so in the docs)

- A per-side mix (left/right or per-player volume of the game's mix): a host audio.c change, later.
- MusyX's pan and distance cut for player 2's room's emitters (one listener): the gain is theirs, the pan player 1's camera's.
- The sanity music (player 1's sanity, the main loop) and the insanity screens (game modes, both players): the game's.
- A duck (fn151) is global: player 2's look ducks player 1's sound too; untouched.
- The copy Tome's DTK layer tick (0x801A1328 on the copy): whether it ever issued a DTK command is measured by the [dtk] lines
  in the Tome check, not changed.

## The checks (a fresh state from Jump to Game on the build; ED_ROOM_SLOTS=2 ED_COOP_STAY=1; the same recipe as R4's state)

- s1 (player 2's door, player 1 stays in 183): the R4 t1 recipe. Lines: at the press `[sound] fade at fn66: mover 2 keeps room 183:
  N kept` (N = room 183's looped tracks, > 0 expected: the room data's emitters started at its load), at the change the same for
  0x80152354 with 0x801A0EAC skipped, no `[dtk]` line in the window, the arrival's 0x8019E068 pair skipped. The listener line for
  room 180's emitters as player 2 while apart. After the way back: room 183's tracks still N (a `[sound] tracks: room 183 n,
  room 180 m` line every 120 frames while apart, from room_slots_frame's [stay] cadence).
- s2 (player 1's door, player 2 stays in 183): ED_RUN_SCRIPT 119 twice at ~3760 (the R2 recipe): the same lines with mover 1
  keeping room 183; the arrival's 0x8019E068 pair PASSED (player 1's).
- s3 (the Tome ticks): player 2 opens their Tome (ED_PAD2_SCRIPT start) and presses right/left/B a few times, then closes: the
  `[sound] tome copy: sound 611/495/546 forwarded live` lines; no `[dtk]` line while it is up.
- s4 (music, in play): chapter 1's doors set no music; the refusal's line is read from the user's play in a later chapter (L02/2325
  fn54; the save room's 2410 fn168(26)). Said so in the record.
