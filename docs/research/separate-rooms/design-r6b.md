# R6b (2026-11-11 night): the enemies' routes in the room player 2 stays in

Readers: `waypoint-readers.md` (the graph, the routes, guest memory). Built in runtime/host/hle.c (`R6b`), under
ED_ROOM_SLOTS=2 with ED_COOP_STAY=1; nothing acts while the players are together (`r6_apart`).

## The problem (r6b-c1, R6's build)

Apart, room 183's zombies chose player 2 (on the ledge at 1305,1508, ~1000 units away) and stood still. Beyond 550 units an
enemy walks a route; the route is requested each chase tick (0x800BBCC8 posts the enemy's id to the queue 0x805B2AFC) and
made by the game's background thread (0x800BB730, priority 31), which runs while the main thread waits for the retrace,
always with player 1's room in play. Its job builds the point graph (r13-25060, 28000 bytes; r13-25064 the room it was built
for) from the current slot (0x800BA530 at 0x800BB914), finds start and end points with sight lines through the current
slot's collision (0x800B9EE4), searches (0x800BB1AC), smooths (0x800BACEC) and keeps the path as POINTERS into the graph
(each graph entry +0 points into the slot record's point list). For player 2's room: room 180's graph and collision, no
start point, "no path" (state +20 bit 4), the enemy back to state 1 every tick.

## The design (as built)

- A second graph for the room player 2 stays in, at 0x83400000 (28000 bytes, in ED_ROOM_SLOTS's memory past slot 1's area,
  which the memory reader found unused by the game and the host), its key beside it (+28000: 'R6BG', room, slot), so it rides
  save states with the RAM. One graph only for A's enemies, as the game has it, so A's routes never point into B's points.
- The job runs in B when its enemy is of B: a notify at 0x800BA530's entry with lr 0x800BB918 (the job's node in r24) opens
  a bracket when the players are apart and the node's room is resident, loaded and not the room in play: the room word and
  slot table (coop_room_in), r13-25060 := the second graph, r13-25064 := B when the key matches (room and slot), else -1 so
  0x800BA530 builds it now; MSR[EE] cleared (the retrace is delivered only with it set, runtime/host/interrupt.c), so the
  main thread can't run a frame with B in. A notify at OSDisableInterrupts (0x8020102C) with lr 0x800BB9E4, reached on every
  path after the build (the no-start/no-end branch at 0x800BB9B0/B8 jumps straight there), closes it: the key written (built
  or not), the graph pair, room word and slot table put back, EE as it was.
- Each frame (room_slots_frame): a bracket found open in the main thread is closed and warned (never expected); the second
  graph is emptied when its room is no longer in its slot (a route into it then reads an empty point: 0x800BC0B0 answers 0,
  the enemy goes to state 1 and asks again).
- Installed only with ED_ROOM_SLOTS and ED_COOP_STAY, after checking the code words (0x800BA530 stwu, its bl at 0x800BB914,
  OSDisableInterrupts' mfmsr, its bl at 0x800BB9E0); both notifies stand aside in the Tome copy's turn.

## Left as is

- R4's change bracket runs the arrival's graph builds (0x80152080, 0x80152750) with player 2's new room in play: they
  rebuild the one A graph for B's new room; the worker is paused through the change, and its first job after rebuilds A's
  (same entries at the same addresses, so A's routes point at the same points again). Not changed.
- The main thread's other graph users for B's nodes (a script verb's wander point 0x800BC1EC -> 0x800B9EE4 mode 8; an
  effect's 0x8014A6BC) read A's graph: not seen in play.

## The runs (logs/run-20261111-21*-r6d-*)

- r6d-c1-far (player 2 on the ledge, player 1 in 180): `room 183's route graph built for its enemies (slot 1; for 40; frame
  3833)`; zombie 39 walked from (880,789,0) up the stairs to (1183,1635,179) and attacked, as did 40 and 38; player 2 68 -> 19,
  player 1 125; no warning. Picture: shots/000041.jpg, a zombie beside player 2 on the ledge; 59 fps.
- r6d-c2, r6d-c4, r6d-c5 (player 1 stays; together; together with player 2 swinging): as R6's and the baseline's, and no
  `[enemy]` line in the together runs.

## After the cold review (refute: the reviewer's report, 2026-11-11 night)

- Sound: every path from the build reaches the close (both early exits jump to 0x800BB9E0); the ~90 callees hold no OS call,
  no mtmsr, no reachable indirect call; mfmsr reads the host's cpu->msr, so clearing EE holds the retrace; the notifies see
  the caller's lr and r24.
- FIXED: a quick save or load inside an open bracket would have kept player 2's room in for good (the words and EE swapped
  in the state, the host's g_r6b not): hle_save_held waits while it is open, and hle_coop_state_load forgets it.
- Known, low: OSDisableInterrupts (214 call sites) is a chain stop with both switches on (59-60 fps in every run); EE is off
  for a job's whole build and search, delaying the retrace while a graph is built (not seen as a hitch); routes of a room
  the second graph held before still point into it after it is rebuilt for another room until they ask again (every tick).

## Also in this build: player 2's own events apart (coop_events)

Their character's events in the frame (ticks, hits taken, state entries) now run with their room current while they stay
apart (coop_stay_room_in round the handler call; together nothing). Seen: FINISH HIM showed for one frame of their turn only
(the finisher's look 0x800330E0 takes bodies of the room in play, from their state 1 tick in coop_events). NOT verified by
script: no run produced a finishable (downed) zombie, together or apart; chapter 1's zombies died outright (state 8).

## The runs on this build (logs/run-20261111-212*-r6e-*)

- r6e-c1-far: route graph built at 3833; zombies climb to player 2 on the ledge; player 2 68 -> 12, player 1 125.
- r6e-c7 (apart, player 2 walks into the hall and fights, B pressed): hits both ways; zombies killed player 2 (68 -> 0,
  state 8 at 7784) and the game ran its own game over and restart (script 1500, room 69) as it does for a player's death.
- r6e-c4, -c5, -c6 (together): as before; no [enemy] line.
