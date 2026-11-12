# R3, the picture: each half shows its player's room (design 2026-11-11, from rooms/picture-readers.md)

Base: the uncommitted hle.c with ED_ROOM_SLOTS=2 (R1) and ED_COOP_STAY=1 (R2): both rooms resident, drawn piled in both halves.

## The facts

- The frame function 0x800080E4 draws the room twice: the opaque pass 0x801E851C (bl 0x80008118) calls 0x801E75B4(3) then
  0x801E75B4(2); the translucent pass 0x801E85E0 (bl 0x80008140) loads the camera matrix 0x8060248C into index 27 (and its
  normal matrix) and calls 0x801E73D4(3) then 0x801E73D4(2). Section 3 = the OTHER slot's record (0x80155130(3): table +4,
  0 when -1), section 2 = the current's. The actors are drawn after, by 0x80119D60(1) -> 0x8011A278, with no room test.
- The room's vertex formats carry no matrix index (0x801E16D0: POS/NRM/TEX only): the room draws with the CURRENT matrix,
  index 27, which the opaque pass inherits from the update's camera build (0x801E4DC8 in 0x801E4BDC, before the pass) and
  the translucent pass loads itself (0x801E85F8). 0x801E75B4 loads only a texture matrix (index 48). Later draws use 27
  without reloading it (0x801F41C8 at 0x80008174, 0x801DC450/0x801DC55C, 0x80129BA0, the menu code).
- The renderer replays the whole frame per view; its one per-view geometry filter is marked_out_of (interp.cpp:569):
  a draw whose position matrix carries 0xD2 at +15, 0xA0+player at +31, 0xE5 at +47 has that matrix zeroed in that
  player's view (the draw collapses to a point). Uniform blocks are deduplicated against the previous draw by content
  hash, so a different mark per section gives the two rooms separate blocks. One mark excludes ONE view.
- Whole-frame moments (split_off: a cutscene, the Tome with no sharing, no player or camera; the renderer's own fallbacks)
  show the game's Real frame, which carries both rooms: section 3 must draw nothing then. 0x801E75B4 treats a record of 0
  as nothing to draw (0x801E769C) and skips its texture-state side effects too (the N=1 shape).
- coop_found_mark (hle.c ~3291) plants the three bytes on an actor's matrices during the object draw (lr gate
  0x8011A278..0x8011B400) while g_coop_found_drawing is set, with g_coop_mark_view the view to leave; coop_found_object_draw
  (~3261, at 0x8011A278's entry) decides it from the found/unseen lists.
- The kit records 'room' as the game's room word; player 2's camera is judged against it (the false CAMERA findings).
  tools/ed/coopcheck-recording.diff already specifies p1room/p2room columns (object +592).

## The build

P1 (the room per half). An intercept on both 0x801E75B4 and 0x801E73D4 (r3 = section; a `running` guard; the pattern of
  coop_stay_strip_walk and room_slots_layout):
  - room0 = the room of table +0's record (the room in play, player 1's half, view 0); room1 = coop_stay_apart(cpu) ?
    player 2's room (object +592) : room0; the section's record: 3 -> table +4, 2 -> table +0; its room from +0x8140.
  - Section 3: drawn only when room1 != room0, the section's record is live and its room == room1, and the halves are
    shown (camera.c's g_split_on, exported as camera_split_on()); otherwise answer 0 without calling the game (section 3
    draws nothing: the N=1 shape). Section 2: always drawn.
  - When room1 != room0 and the split is on: before the game's call, load a MARKED copy of the camera matrix into 27
    (section 3: out of view 0, byte 0xA0; section 2: out of view 1, byte 0xA1), by writing the 48 bytes of 0x8060248C
    with the mark to a scratch guest address and guest-calling GXLoadPosMtxImm(scratch, 27); after the call, load the
    unmarked 0x8060248C into 27 again (later draws use 27). Never mark 0x8060248C in place (the game reads it).
  - Else: pass the call through untouched.
P2 (the actors per half). coop_found_object_draw: when no found/unseen list matched and room1 != room0: the object's
  +592 == room0 -> g_coop_found_drawing = true, g_coop_mark_view = 1 (out of player 2's half); == room1 -> view 0; neither
  -> out of view 0 (one view only, logged once): the stayer's body leaves player 1's half, player 1's leaves the stayer's.
P3 (the kit). both.tsv gains p1room and p2room (object +592 of each player's node, 0 without); recording.py's room_of(r,
  half) = r.get(half+'room') or r['room'] used by the camera checks (check_cameras, coopjudge's on_room_camera /
  camera_label / camera_back / LOST) for that half; coopjudge's TWO_ROOMS skipped when the run's log says the stayer is on.
P4 (not now). The fade quad per half (mark at lr 0x801E7F60 for the non-mover's view: R4, with the mover's identity);
  lights per room (one grid, player 1's room's, for both: a renderer change later); the per-command skip of the excluded
  room (measure first: DOL_INTERP_LOG [split-cost], [fps]).

## The checks (the state remade on the build; the stayer's recipe)

- t1: player 1 through 119 to 180, player 2 stays in 183 and examines/picks up; pictures of both halves while apart: each
  half its own room, no piled geometry; the stayer's body not in player 1's half; player 1 not in the stayer's; after the
  return both in 183 with one room drawn; [fps] against R2's 60.
- t2: the bring (switch off): unchanged pictures, one room.
- t3: a cutscene while apart? (chapter 1's skeleton scene 419 in 183 by player 1 while player 2 stays... player 1 is in
  180: scene 419 is a 183 script) -> player 2 (stayer in 183) triggers 419 while player 1 is in 180: a whole-frame moment
  with the split off: the frame shows room 180 (view 0's) only, no pile. If scripting it is awkward, a note for the user.
