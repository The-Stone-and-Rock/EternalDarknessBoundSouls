# Players in separate rooms: the design after the readers (2026-11-11)

HEAD 6b2a584. Six readers' facts in rooms/readers.md (summaries in readers-short.md). This is a design with options; nothing is
built. The user's direction: think first, don't guess; Pius's chapter (chapter 1 = LEVEL 1, rooms 341, 342, 180-184, 343, 344)
for the first tests; sanity effects don't exist in chapter 1; the randomizer last.

## The facts the design rests on

1. The room engine was written for N resident rooms and this build pins N to 1. 0x8004304C is `li r3,1; blr` with 8 callers
   (0x80020ECC the play-start allocation, which multiplies the 2,916,384-byte slot and the 335,008-byte per-slot area by it;
   0x80153C80 the slot layout 0x80153C68, laying out N records from r13-23304[i]; 0x80154C5C the slot swap 0x80154C50 (current
   and other indices xor 1, only when N == 2); 0x80154CB8 inside the load 0x80154C94 ("is the wanted room already in the other
   slot": 0x80154D64-0x80154D94); 0x801517E8, 0x8015282C, 0x80153DC0, 0x80155470). The slot table is 0x8058AFC0: +0 the
   current slot, +4 the other slot or -1, +12.. a record pointer per slot; 0x80155130(3) returns the other slot's record. The
   resident-rooms table 0x8058AF60 (24-byte entries, count r13-23292 = 2: the current room and fn71's queued next) and the
   ARAM cache of 2 room files (1,677,824 bytes each at ARAM 0x600000) already prefetch the next room. The host's guest RAM is 48
   MB with the game's 24 MB untouched. [room-memory reader; ed dis 0x8004304C, 0x80020EB4, 0x80154C50, 0x80153C68]
2. One "room in play" word, r13-23276 (fn25), 15 writers, 675 readers; r13-23288 the old room. Every room-shaped thing is the
   resident record's: zones (0x80123F70(object)+180, via 0x80155130(2)), markers, cameras (eye records 0x80602B18..), collision
   and geometry (pointers set by 0x801E1B74(record+0x813C) at 0x80151FDC), the light grid (0x804F0784), the sanity list, the
   sound/ambience zones (0x80155234(2)). Nodes of the whole level live in ONE list, each stamped with its room (object +592,
   0x801F64C8 sets it); room-wide walks filter by r13-23276 (hide-others, fn76, the tag find, events 61/62). Flags, scripts,
   words, the carried lists, the Tome and the element pool are level-wide. [room-memory, actors-world, sanity readers]
3. A room change is one synchronous whole-game event: the ticket (0x8058AFFC, busy 4), 0x80152298 waits for the GPU, fades
   all 80 sound tracks, stops the background thread, blocks until the loader unpacked ARAM into the slot (3-4 frames here),
   then 0x80151FDC: event 61 to the OLD room's nodes, prompt slots reset, busy cleared for all, the arriver placed on the
   entry marker, event 62 to the NEW room's nodes, the arrival camera and script. The fade is a screen-wide game quad plus
   VISetBlack: both halves black. [room-memory, render, host readers]
4. The host already has: ownership of a change by its asker (notify_room_ticket, g_coop_room_p2, g_coop_entered_p2: the arrival
   spot, scene, camera and door motion are the asker's); the other player walking through the fade; the left-behind state
   named (coop_left_behind: object +592 differs) with every host look skipped in it, ended by coop_follow's bring; per-player
   copies of everything a room gives a player (zone inside words, camera sets, prompt slots and looks, script words, the
   windows), swapped into the game's one table for that player's turn; coop_character_step (player 2's character step run by
   the host with their state in; its second scan off by default). [host-today reader]
5. The picture: the renderer records ONE frame's draws and replays the whole packet per view with that player's projection;
   nothing in it knows rooms; per-view exclusion exists only as the three-byte matrix mark on an object's draw (coop_found_mark,
   marked_out_of); lights and shadows are per frame from one grid; co-op is capped at 60 because three full renders per game
   frame already fill 16.7 ms here. The Tome copy is the one case of two worlds: a whole-RAM copy run in alternate frames, its
   frame's picture kept and fitted into the owner's half (split_skip_views, s_menuKept). [render reader]
6. Sound: one MusyX engine, one host mixer, one DTK music stream, one listener fed from the live camera block (player 1's) each
   sound tick by one small function 0x8019EDD4; positioned tracks carry the room they started in (+132) and fn52 filters by it;
   a room change drops stream tracks and fades all 80. Both players already hear every centred effect. [sound reader]
7. Scripts run one chunk at a time to its end (r13-23228); their words (0x805CAA40) are level-wide and already per player
   (coop_states_swap, coop_script_timers). Enemies are level-wide placings stamped with a room; no "Tome found switches
   enemies on" rule exists in level 1 (the Tome is not in it). Pius's chapter has no sanity (r13-29244 == 0). [actors, sanity]

## The options

A. TWO RESIDENT ROOM SLOTS IN THE ONE GAME (the engine's own dormant two-slot system), each player's room the "room in play"
   for their turn. N := 2 (a host intercept on 0x8004304C answering 2: the allocation, the layout, the swap and the "already in
   the other slot" path all follow). Both rooms' data live in the game's own memory as the engine laid it out for. The player
   who changes room loads into the slot that is free of the other player (the host steers the table's +0/+4 before the change
   so the engine's "other slot" is the one to evict); the other player stays, their room resident, their object's +592 their
   room, their turn run with their room in (r13-23276, the slot's current index, and every pointer `ed needs 0x80152298 /
   0x80152614 / 0x80155130 / 0x80123F70` lists, as the camera sets are swapped today). Progress stays one game: one node
   list, one flag set, one carry list, one Tome: nothing to merge. The picture needs a per-view room draw (D below), sound a
   listener rule (E), the change's whole-game effects an owner filter (F).
B. TWO WORLDS: player 2's room runs in a whole-RAM copy in alternate frames (the Tome copy's machinery), drawn into their half
   as a kept picture. Rejected as the main road: the two worlds never see each other (an item picked up, a flag set, an enemy
   killed in one is not in the other: co-op's shared progress breaks), ARAM is not copied (the room files' cache is one), the
   copy's sound is never heard, and the game side roughly doubles (two full behaviour steps and draws per round) on a machine
   already at the cap. Kept as a known fallback only for a whole-chapter separation much later, where sharing is not wanted.
C. NO SECOND ROOM: the other player waits (held, their half still) while the mover is away, or is brought along as today.
   Not separate rooms. Kept as the behaviour when a change can't be served (a level change, a cinematic that replaces the
   actor list: mode 6).

The recommendation is A: it is the game's own code ("the game already has the function"), progress stays shared, and its
first step costs nothing of co-op.

## The sub-steps (each from a fresh Jump to Game state on its build, chapter 1; the method as A-F: readers where needed,
## refuters, one build, scripted checks, then the user plays)

R1 PROBE: two slots in SOLO. ED_ROOM_SLOTS=2 (testing): the host answers 2 at 0x8004304C (an intercept setting r3 and
   returning, as coop_busy_ask answers the busy ask); a log of the slot table (+0, +4, r13-23304[0..1], r13-23292, the
   record's room/loaded bytes) at every ticket, change and arrival (room_log's places). Run solo from Jump to Game through
   341 -> 342 (L01/103, transition 15) and 183 <-> 180 <-> 183 (119, 29, 119: the third change should find room 183 "already
   in the other slot", 0x80154D64, and skip the unpack), with ED_ROOM_LOG=1 ED_HEAP_CHECK=N AURORA_SHADOW_LOG=1: does the
   dormant path run clean (no crash, the right room in play, both grids seen by room_lights_update, the heap walk quiet, the
   allocation 2.9 MB larger), and how long a change takes. Then the same run in co-op with nothing else changed (coop_follow
   still brings): no regression. Nothing of this is co-op code; it answers the first open question of three readers.
R2 THE OTHER PLAYER STAYS: ED_COOP_STAY=1 (testing): coop_follow doesn't bring player 2 when the two slots hold both rooms;
   player 2's turn (coop_player_turn) runs with THEIR room in: r13-23276, the slot table's +0/+4 and r13-23288 swapped, plus
   what the `ed needs` lists give for 0x80152298, 0x80152614, 0x80155130, 0x80123F70 and the character step (readers first:
   this list is the step's substance); coop_character_step steps them against their slot's collision; coop_zones_new keys per
   room (the info pointer differs per slot: fine). coop_left_behind's skips become "their room is the other slot": the looks,
   camera step and prompts run with their slot in. Check from the state: player 1 through 119 to room 180, player 2 stays in
   183 and walks (positions and floor in both.tsv, no FELL/SENT), examines the block (their own look in their room), player 1
   walks in 180; then player 1 back through 29: both in 183 again, the follow's "already here" case. Player 2's half shows the
   wrong room until R3: expected.
R3 THE PICTURE: each half draws its player's room. The room geometry draw (the pointers 0x801E1B74 sets, the list consumer
   0x80119D60 / 0x8011A278: a reader finds the draw's entry) is issued twice per frame, once per slot, each run bracketed by a
   host GX-stream op "these draws are view N's" that the renderer honours in build_view (interp.cpp) as it honours the
   three-byte mark; actors drawn by their +592 (the mark); lights: a grid per view (room_lights_update knows both grids;
   dol_aurora_set_room_lights per view) and the shadow pass per view; the fade: the fade quad kept in the mover's view only
   and VISetBlack answered per half (open: render reader). Cost: one extra room geometry draw per frame, not a full render.
   Measure with DOL_INTERP_LOG and [fps] before and after; the cap stays 60.
R4 DOORS FROM EITHER SIDE: the change by its owner only: the slot to evict is the mover's old slot unless the other player is
   in it (both in one room: the free slot); one ticket at a time (the second player's door waits as the way-out wait does,
   logged); the change's whole-game acts filtered by owner: the sound fade of the mover's room's tracks (tags +132, fn52's
   filter), event 61 to the old room's nodes only when nobody stays in it, the prompt reset and busy clear for the mover (the
   stayer's turn keeps theirs), the arrival placing and camera the mover's (today's ownership). The stayer's zones, script
   marks and windows untouched (step E's cancels apply to the mover alone). Scenes a room runs once (419, 1040) run for the
   first arriver as the game does in solo.
R5 SOUND: the listener follows the camera of the player whose room has positioned sounds, else player 1's (0x8019EDD4's one
   call site); the DTK music and centred effects stay shared (the user: "we might be fine"); the room change's fade only for
   the mover's room (R4). Later: a per-side mix (the host owns left/right: audio.c, dtk.c).
R6 ENEMIES AND THINGS IN TWO ROOMS: whether a node whose room is the other slot steps against its own slot's collision (open:
   actors reader; the behaviour pass runs with player 1's room in: coop_character_step's pattern per node of the other room,
   or the game's own filter skips them and they wake when a player enters); coop_enemy_target's "both in its room".
R7 THE TOME, SAVES, CUTSCENES WITH TWO ROOMS: tw_keep's room packing stays blocked (the Tome never packs the room in co-op);
   save states hold both slots as RAM (no format change) plus a new host blob for "whose room"; a cutscene (mode 6 replaces
   the actor list) and a level change are option C moments: the other player is brought first.
LATER, NOT THIS STEP: chapters independently (two level packs: scripts, words, bodies at r13-26452: a step of its own);
   "Bring X to Y" in the co-op menu (the todo); the per-side sound mix.

## What would make option A fail (to test in R1/R2 before anything else is built)

- The dormant two-slot path has a slot-0 assumption somewhere: the lights code's "rooms load at r13-25984" (hle.c:6066),
  the loader's 0x4000 branch using r13-23304[slot], 0x80151FDC's record pointer, the Tome's TW_ROOM_BUFFER (0x801538A4), the
  host's 0x81800000 range checks. R1 shows the first three; the others are read.
- The per-player room word is not enough: 675 readers include pointers cached at the change (0x801E1B74, the lights, the
  zones getter) that a per-turn swap doesn't reach without the record's pointers swapped too. `ed needs` on the four
  functions above is R2's reading; the camera-set swap precedent (kTwSwitches grew word by word) says it is a list, not a
  wall.
- A node in the other slot's room is stepped against the current slot's collision (falls). coop_character_step's host-run
  step with their slot in is the answer for the players; enemies are R6.
- The frame budget: an extra room geometry draw per frame on this machine. Measured in R3 before it is kept.

## Refuted or corrected (three refuters, rooms/refute.md)

- "Answer 2 at 0x8004304C and the allocation, layout and swap follow": REFUTED twice. (1) 0x80020EB4 sizes the block for N but
  lays the model area, the per-slot areas and the level heap after ONE slot (0x80020F64/0x80020F7C), so slot 1 would lie on
  the model area and the level heap. (2) The game's OS heap is 17.1 MB with 192 KB free in the chapter-1 states (heapwalk on
  the states), not 24 MB: the +3.25 MB block fails (OSAllocFromHeap returns 0, the game memsets at address 0). So: the
  allocation's caller (lr 0x80020ED0) is answered 1, slot 1 and area 1 are host-placed past the pools (ROOM_SLOT1 0x83000000,
  ROOM_AREA1 0x83300000; cpu.c gives 8 MB more under ED_ROOM_SLOTS: 56 MB), the two per-slot records (73 KB) come from the
  game's heap and the free total is logged.
- "183 -> 180 -> 183 finds room 183 already in the other slot": REFUTED as the engine stands. 0x80152298's tail (0x801523A8-
  0x80152404) marks the other slot's record dead (room -1, loaded 0, +0x8143 0) after EVERY change, and nothing in this build
  writes the queued room at table +8 (both writers store -1), so the engine only alternates slots and never reuses the other.
  The probe undoes the tail at the next room draw for a slot whose room is still in it (nothing unpacked into it since: the
  posts 0x80151DC4(room, slot) are watched), so the "already in the other slot" path (0x80154D64 -> 0x80154C50 from lr
  0x80154D94) can run. This is also what R2 needs to keep the stayer's room resident.
- "The probe is nothing of co-op and safe": REFUTED on two points. A state saved under N=1 (+4 = -1, no second record) loaded
  into an N=2 build breaks at the first change (the swap makes +4 = -2, the tail stores through it): the probe boots cold
  through Jump to Game (the layout runs once at play start, 0x80020EB4 is guarded by r13-25984 != 0). And 0x80155450(room)
  ("is it resident") answers 1 for every exit neighbour of a loaded slot's room when the count is 2 (0x801554C8-0x80155510),
  for 8 callers: logged, compared with today's run, answered 1 at lr 0x80155474 if unwanted.
- The slot-0 assumptions found: the game's 0x801538A4 (`lwz r3, -23304(r13)`: slot 0's base whatever the table says; 8
  callers: the Tome's packing, menus) and room_lights_update's "the lower grid is the room's" rule (hle.c:6066-6075) with its
  0x81800000 bounds: the rule is now "the grid inside the current slot's span" with two slots, the bounds the RAM's size.
  The loader's 0x4000 branch, the slot thread, the swap, the table init, 0x80155130 and 0x80151FDC's record pointer all index
  by the table or the message: clean.
- R2's word list: `ed needs` cannot give it (tools/ed/player_data.tsv has no room rows: to add). From the disassembly the
  turn swap must carry r13-23276, the table's +0/+4 (xor-paired), and the things copy r13-25060/-25064 (0x800BA530 rebuilds a
  28000-byte list of the current record's +100 things at a change and at the frame's 0x800BB914; script verbs read it: a
  second buffer per player). Not needed per turn: r13-23288, -23284, -22132, -22140. coop_character_step must swap the table's
  +0/+4 too, since 0x80123F70's player path short-circuits to the CURRENT slot (only non-player nodes search by +592). The
  change's walk 0x80043A5C -> 0x80043820 strips the parts and moves of every character not in the NEW room: the stayer's too
  (an intercept: "in this room" when the node's room is loaded in any slot). The streamer stamps requests with r13-23276 and
  0x801481FC refuses a request whose stamp differs (UNSURE: where player 2's body's requests are made).
- R3: the room is NOT drawn through the object list but by 0x801E75B4(section) from 0x801E851C, section 3 (the other slot)
  then 2, every frame: the engine already draws both resident slots; the probe shows it as double geometry. 0x801E1B74 is a
  texture-bank word, not geometry pointers (drop it from any swap list). A GX-stream "view op" can't split the two room draws
  in build_view (they share one byte-identical uniform block): the per-view exclusion is the existing three-byte matrix mark,
  re-issued by the host at 0x801E75B4's entry for the room's matrix (index 27 from 0x8060248C, to read with ED_MATRIX_LOG);
  the fade quad (the fade tick's own GXLoadPosMtxImm at lr 0x801E7F60) can be marked the same way; VISetBlack is never called
  by the game. Lights: one grid and one fill_shadow per frame; per-view lighting is a renderer change (fill_shadow twice),
  first build keeps player 1's room's grid for both. No frustum cull in the draw path (distance for detail only). The cost is
  the second room's draws replayed in every output (3, or 5 with moments), not "one draw": unmeasured; the game's own hidden
  frame and whole-frame moments (a cutscene, a menu) show both rooms overlapping while +4 != -1, so section 3 must answer 0
  when both players share a room or the frame is shown whole.
