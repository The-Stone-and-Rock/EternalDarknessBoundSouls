# R6c (2026-11-11 night): the creatures of player 2's room as that room's own game

Readers: `behaviour-readers.md` (the behaviour bracket; the enemy kinds). Built in runtime/host/hle.c (`r6c_creature`), under
ED_ROOM_SLOTS=2 with ED_COOP_STAY=1; nothing acts while the players are together (`r6_apart`).

## Why

After R6 and R6b, room 183's zombies hit and chase player 2 apart, but the enemies of player 2's room still lived with
player 1's room as "the room in play" and player 1 as "the player":
- their behaviours' 29 room-in-play tests (enemy-readers-2.md, enemy-life (2)): a downed one never gets up while apart
  (state 9's event 24 re-posted, 0x8003D97C; the sibling 0x8003E74C), the spawner (type 78, 0x8002FC94) waits, event 30's
  effects are skipped, sounds are tagged with player 1's room, room constants (room 183's chase speed) wrong;
- every enemy kind but type 4 looks only for "the player" (types 6, 8, 42, 44, 53, 63, 74, 89 read the player words in their
  own code; 5, 6, 7, 8, 41, 42, 44, 53, 63, 89 give the sight's sanity hit, 0x800CA370 / 0x800CA4E4, to the player only): in
  chapters 2-11 such enemies of player 2's room would never go after player 2 apart. Chapter 1 (the runs) has only zombies.

## What was built

- An intercept on the handlers' delivery 0x801F5294(node, state, message, answer*) (every event, synchronous, queued or
  broadcast, reaches the type's function through its bctrl at 0x801F52F4). Apart, for a creature (actor at node +40, its
  +158 class 2) or a spawner (type 78; its room at actor +124 -> +28) whose room is resident, loaded and not the room in play:
  the call runs again (hle_guest_call on 0x801F5294) with that room current (coop_room_in) and that room's player as "the
  player" (r13-26524 object, -21492 node, -21496 id: player 2 for their room). The nested call finds the room current and
  passes through. Skipped: the players' nodes (type 2: coop_events; 19, the held one: coop_grab_events), event 4 (no creature
  kind has a case: readers), a room change (event 61 to the room left is sent mid-change: coop_camera_at_room_change), 4 deep.
  Player 1's node is kept for the call (g_r6c_p1) so r6_player1_node stays right inside it.
- coop_events (player 2's own behaviour events in the frame) and coop_grab_events (player 2 held) run with their room current
  apart (coop_stay_room_in; together it does nothing): their finisher's look (0x800330E0 takes bodies of the room in play) and
  the held behaviour's re-posting events 138/139/140 (0x800613C0, 0x800614FC, 0x80061624).
- The coop_sighting comment's reason (the 0x1000 flag) was wrong: zombies don't carry it.

## Not verified by script (chapter 1 has only zombies; no downed zombie was produced)

A downed zombie rising apart, the spawner apart, the other kinds apart (chapters 2-11), FINISH HIM apart. Each needs play or
a state of another chapter.

## Left as is

- coop_sound_gain reads coop_stay_apart (the room word): a sound started inside these brackets takes its first gain from
  player 1's eye; the next tick corrects it (as in player 2's turn today).
- Event 203's sanity hit 0x8003A8AC goes to "the player": inside the bracket player 2 (right for their room).
