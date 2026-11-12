
## R2, the other player stays: the readers, the build, the runs (2026-11-11 late afternoon)

### What three readers settled (rooms/stay-readers.md)

- The host's one notion of "left behind" is coop_follow's bring test (o2's +592 != o1's) and coop_left_behind, used by the
  look, the camera step, the prompt show, the scene set and the quick cast. Nothing in the host swapped a room word for a
  turn: coop_player_turn swaps the identity (r13-26524/-21492/-21496), the state copies, the input masks, the focus, the
  camera block; coop_character_step (the host-run step) is OFF by default, so player 2's body is stepped by the game's own
  pass as a NON-player node, and 0x80123F70's non-player path finds the room's collision by the node's +592 in any loaded
  slot: the stayer walks on their own floor by the engine itself, as long as their slot's record is marked loaded.
- 0x80123F70's PLAYER path (the object == r13-26524) short-circuits to the CURRENT slot: in player 2's turn (their identity
  in) their look's scan, zones, controller and camera step see the room in play unless the slot table's +0 is theirs:
  the turn must swap r13-23276 and the table's +0/+4 (xor-paired, as 0x80154C50 keeps them). The "things copy"
  r13-25060/-25064 turned out to be the room's AI point graph, read only by script point verbs: not carried in R2.
- The change strips the stayer twice: the load's 0x80043AB8 strips EVERY character's body, moves and parts level-wide
  (no room test; its per-node block is entered on the node's kind, asked by 0x801F62C0 at lr 0x80043B00), and the arrival's
  walk 0x80043A5C -> 0x80043820(node, record) strips every node not in the NEW room (0x80043898); the in-room branch is a
  no-op for a set body, so sparing the node and running that branch are the same.
- The change's tail (0x801523A8-0x80152404) marks the other slot's record dead inside the ticket tick, BEFORE the behaviour
  pass of the same frame: a draw-time put-back leaves the stayer with no world for that frame. 0x80154B54 (the exits list)
  has ONE caller, right after the tail (lr 0x80152434): the put-back goes there.
- The load 0x80154C94 has no eviction choice: a fresh load always swaps and unpacks into the former other slot. The host
  pre-steers the table before the change (notify_room_change, the entry) so the slot evicted is player 1's old one.
- Events 61 (to the old room's nodes: the stayer's too) and 62 (to the new room's) are mild for a standing player; a
  change of player 2's stamps and places r13-26524's object (player 1's): R2 keeps today's route (the bring) for a change
  of player 2's; R4 takes ownership of it.
- Sound (the user's rule, recorded): player 2 has no music, their in-game sounds stay; one listener on player 1's camera,
  one music stream in the host (dtk.c): R5.

### What was built (ED_COOP_STAY=1, with ED_ROOM_SLOTS=2; runtime/host/hle.c)

- coop_stay_apart: player 2's room (object +592) is resident in a slot (the host's slot list), isn't the room in play, and
  the change was player 1's (!g_coop_room_p2).
- coop_follow: no bring while apart (logged "[stay] player 2 stays in room N (slot s) while player 1 went to room M");
  its floor memory reset for the reunion. coop_left_behind := "their room is in no loaded slot". coop_camera_chosen
  doesn't count the mover's room start for the stayer's set. coop_door_proxy trades no places across rooms.
- coop_player_turn: coop_stay_room_in/out round the turn: r13-23276 := their room, the table's +0/+4 swapped to their
  slot, both back after (with the identity).
- The strips: an intercept on 0x80043820 passes over player 2's node while apart; the kind ask 0x801F62C0 at lr 0x80043B00
  answers 0 for their node, so the load's level-wide strip passes over it too. Both logged.
- The slot records: notify_room_slots_tail (0x80154B54 at lr 0x80152434) puts back, right after the change's tail, the
  record of the room in play and, with the stayer, player 2's room; room_slots_return (at the change's entry) puts back
  the other slot's record when the change is INTO its room (so the load finds it "already in the other slot") and swaps
  the table's indices when player 2's room sits in the slot the engine would unpack into. The probe's draw-time put-back
  is gone (it kept rooms nobody was in alive and drawn: the piling the user saw when both players were in one room).
- A "[stay]" line every two seconds while apart (their room, slot and place; the room in play).
- The stayer's room carried through every call that puts their own set in outside their turn: the window's tick
  (coop_words_tick), a use of theirs (coop_use_start / continue_coop_use: g_coop_use.room_in), a timer's script
  (coop_script_timers) and a Tome item's script (tome_copy_left). Without it the pickup's close (the find's events, the
  close script 1181 as a use) ran with player 1's room in, and player 2 then walked in place at the block's spot.
- The hold the game puts on everyone but the door's user (event 102: the node's +28's 8) let go for player 2 at the
  change's tail on any door of player 1's (0x801F6828(node, 0), as coop_door_motion does for their own door): without it the
  stayer stood held after player 1's door, and again after player 1 came back.

### The runs (saves/rooms-ch1-183.state remade on each build from Jump to Game WITHOUT the stay switch, with ED_ROOM_SLOTS=2;
### the chains in the scratch's rooms/runs-r2*.sh and build-run-t1c.sh; the game at 60 frames a second in every run)

- A first state made WITH the switch on had player 2 stay behind in room 341 during the state's own door chain (the stay
  works from the first door): the state is made without it. One run of player 2's door press from that broken state
  (rooms-r2-t3-p2door) ran on past its frame limit with player 1 idle by room 183's zombies for two minutes: they died and
  the game reset to the title through its own game-over path (OSResetSystem from 0x80022694): a test accident, not the code.
- rooms-r2c-t1b-stay (logs/run-20261111-160310-): player 1 through 119 to 180 at 4073 ("a fresh load" into slot 0, player
  1's old, by the steer; the load's strip and the arrival's walk pass over player 2's node; slot 1's record put back; "player
  2 stays in room 183 (slot 1) while player 1 went to room 180"), BOTH rooms drawn while apart; player 2's own EXAMINE (4200),
  DETAIL (4300) and PICK UP (4500, "own find from lr 0x800254F4", the block floated and placed from their camera) in room
  183 with player 1 in 180; the close at 4701 and script 1181 as theirs; player 1 back through 29 at 4813 ("already in the
  other slot: no unpack"), "one room drawn" after (the slot nobody is in left dead). Player 2 could not walk after.
- rooms-r2c-t2-bring (-160343-, the switch unset): brought both times as today; the left room dead after each change; the
  return "put back (a return into its room)" then "already in the other slot".
- rooms-r2c-t3-p2door (-160414-): player 2 in the door's region presses OPEN DOOR (prompt 1, script 119): a change of
  player 2's keeps today's route: player 2 brought to 180 beside the arrival, then their 29 back: both in 183.
- rooms-r2d-t1c-stay (-160800-): with the hold let go at the tail: player 2 walked 250 units in room 183 while player 1 was
  in 180 (4000-4100), but stood after player 1 came back (held again at that ticket).
- rooms-r2e-t1d-stay (-161032-): the hold let go on any door of player 1's: no stuck after the reunion; but after the
  pickup's close while apart player 2 walked ten units and stood (a walk in place at the block's spot), while the same
  pickup with both in the room (rooms-r2e-cmp-pickup-walk, -161214-) walked freely.
- rooms-r2f-t1e-stay (-161744-): the room carried through the tick and the uses: player 2 walks 240 units down right after
  the pickup's close while apart (4700-4790), then 170 up and 80 left after the reunion. `ed check`: only the kit's camera
  rule, which judges player 2's eye against the game's room (a per-player room column for the kit: with R3).
