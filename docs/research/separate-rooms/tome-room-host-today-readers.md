# The Tome room apart: what the host does today at a room change (reader 3)

Reader 3 of the Tome-room feature (the user, 2026-11-14): players apart; either takes the way into the Tome room; BOTH
go there as co-op did before separate rooms; either takes the Tome; after the flashback each player goes back to their
OWN room and place from the moment of the transition. This reader covers the host only (runtime/host/hle.c as of
3542bf0 plus the uncommitted effects work in the tree): the stay, player 1's door, player 2's door (R4), the old bring
(coop_follow), scenes while apart, one run's log, and where a per-change "bring both" could be decided.

Read only. Every claim is tagged [read] (an address, file:line or log line given) or [inferred]. Line numbers are
hle.c's unless another file is named.

## 1. What decides "stay" or "bring"

- `coop_stay()` (14328-14333) [read]: true when `ED_COOP_STAY` is set AND `room_slots() == 2`. It is read once and
  cached in a `static int on`, so it cannot change during a run [read 14329-14332]. `room_slots()` (13644-13653) is
  `ED_ROOM_SLOTS` (1 or 2), also cached once [read].
- The frontend sets both variables only while Mods > Co-op is on, and clears them otherwise
  (`rooms_follow_coop`, runtime/host/frontend.cpp:482-503) [read]. So solo never has the stay.
- With `coop_stay()` on, NO room change brings the other player [read + inferred]:
  - player 1's change: player 2 "is not brought along" (comment 14321-14327) [read]; the gate in `coop_follow` is
    `coop_stay_apart(cpu)` (22532) [read].
  - player 2's change: `notify_ticket_run` opens R4's bracket whenever `coop_stay() && g_coop_room_p2` (14405-14407)
    [read]; player 1 is not moved (`coop_p1_unplaced` returns false under `coop_stay()`, 20990; the arrival-scene copy
    for player 1 is skipped under `coop_stay()`, 17995) [read].
  - This holds even when the players are together: a door taken from a shared room leaves the other behind
    (`g_change_time.kind` names that case "split", 28031) [read].
- The R2 comment at 14325 still says "A change of player 2's keeps today's route (the bring)". R4 replaced that
  (14352-14357, `notify_ticket_run`) [read]: the comment is stale [inferred from the two reads].
- The one per-frame test that picks between the two paths in `coop_follow` is `coop_stay_apart` (14346-14352) [read]:
  `coop_stay()`, player 2 spawned, SDA ready, NOT in R4's bracket, player 2's room (their object's +592) not the room
  word r13-23276, AND player 2's room resident in a slot (`room_slot_of(r2) >= 0`). If any part fails while the rooms
  differ, `coop_follow` falls through to the bring code below it (22541 on) [read]. So the bring code is still reachable
  under the stay, e.g. when player 2's room is not in either slot [inferred from 14346-14352 and 22532].

## 2. Player 1 takes a door while player 2 stays

In order, as the hooks fire [read unless tagged]:

1. **The ticket** `0x80155770(room, ...)` (notify `notify_room_ticket`, 11424-11456; installed at 31569-31571). r3 is the
   target room (`change_hold_ticket` reads it so, 28167). Decided here: `g_coop_entered_p2`/`g_coop_room_p2` (the
   asker: player 2 if their way-out script asked, or the current id r13-21496 is theirs, 11435-11436);
   `g_coop_p1_tickets++` unless it is player 2's change under the stay (11437-11438); `g_fade_mover` = the mover
   (11439-11440, R4: the other's half keeps its picture through the fade); then `split_ticket_open` (11455).
2. **The split hold window** `split_ticket_open` (25935 on): with `g_split_ready`, co-op, player 2, `coop_stay()`, it
   records the mover and the stayer's room and opens the window unless: the Tome copy's turn, the stayer has no room,
   `ED_SPLIT_HOLD=0`, apart with `ED_SPLIT_HOLD=split`, **a cutscene at the ticket** (25957: "in a cutscene both stay
   held, as today"), or the stayer casting (G3). While it is open, the game's busy asks from the stayer's room are
   answered "not busy" and the door's event 102 to the stayer's room's nodes is kept (`[split]` lines, section 6), and
   a second ticket is refused (`split_ticket_ask`, 25998-26017): "A cinematic's own return ticket (lr 0x8012D2F0) is
   lost so: told loudly (risk 12)".
3. **The ticket runner** `0x80152614(room, entry, camera, script, flags)` (notify `notify_ticket_run`, 14395-14430,
   installed with the stay at 31527-31540). For a change of player 1's it only fills `g_stay_start` (14397-14404: was
   player 2 apart, the target) and returns at 14405 (not player 2's change).
4. **The change** `0x80152298` (notify `notify_room_change`, 11361-11370; installed at 31492-31494): closes player 1's
   words if open (`coop_room_change_words`, 11327-11358, skipped for player 2's change under the stay, 11333), then
   with two slots `room_slots_return` (15092-15119): a return into the room the other slot holds is "kept" only if a
   player stayed in it, else "loaded fresh"; and with player 2 apart in the slot the engine would unpack into (+4), the
   slot table's +0/+4 are swapped so player 1's old slot is the one evicted (15109-15118).
5. **The strips spare the stayer's body**: the load's level-wide strip asks each node's kind at lr 0x80043B00
   (`coop_stay_kind_ask`, 15133-15144, answers 0 for the stayer) and the arrival's walk 0x80043820
   (`coop_stay_strip_walk`, 15121-15131, returns 0 for the stayer). Both go through `coop_stay_spared` (14360-14368),
   whose stayer is player 2 outside the bracket (14359).
6. **The tail** `0x80154B54` at lr 0x80152434 (notify `notify_room_slots_tail`, 15062-15087): `change_tail`, the split
   window closes (`split_over`), the new room's slot is noted, every slot whose room a player is in gets its record put
   back (`room_slot_put_back`, 15049-15058), and the stayer's hold from the ticket (event 102: node +28 bit 8) is let go
   with the game's `0x801F6828(node, 0)` (15079-15085).
7. **Each frame after**, `coop_follow` (22461; called once a frame from the player turn at 30760) finds the rooms
   differ, `coop_stay_apart` true, logs `[stay] player 2 stays in room ...` once and returns (22532-22540).
8. Through all of it every per-room system keys off the two rooms: `r6_apart` (25458-25463: both rooms resident and
   different, outside the bracket) gates enemies/hits; lights, fog, effects and sound have their own `coop_stay()` gates
   (27519, 31829-31833, 14535-14739) [read].

## 3. Player 2 takes a door while player 1 stays (R4)

[read 14352-14490, 18158-18161, 27742-27810]

- At the runner's entry (`notify_ticket_run`), with `coop_stay() && g_coop_room_p2` and player 2's room known, the
  bracket opens: `g_stay_bracket = 2`; player 1's look slots and camera set are saved; `g_stay_bracket_room` = the room
  word (player 1's room); `coop_be_player2(cpu, true, ...)` (12426-12447) writes player 2's object, node and id into
  the identity words r13-26524, -21492, -21496, -21488, -21484 and swaps the casting words; the room word r13-23276 is
  set to player 2's own room; the 196 bytes at 0x80335A38 (prompt slots) are saved; player 2's camera set goes in
  (`coop_cameras_in(cpu, 2)`). So the game's own change places PLAYER 2 at the entry marker and runs the arrival's
  camera, events and script as theirs (comment 14352-14357).
- The slot table is not touched in the bracket: the engine unpacks into the former other slot, which is player 2's old
  room's (comment 14354-14355) [read].
- Inside the bracket: the arrival's script-waits clear (0x80163F04 at lr 0x80152140) puts back waits of scripts that
  aren't player 2's (`stay_waits_clear`, 27755-27778), and the arrival's busy clear `0x801556CC(-1, 0)` at lr
  0x80152188 keeps player 1's bits when their scene or close-up is on (`stay_busy_clear`, 27780-27810): R4b. The
  arrival's camera choice is marked player 2's (`coop_camera_chosen`, 18158-18161).
- The bracket ends at the behaviour pass after the ticket tick (0x8014FDB8 at lr 0x80008400 or 0x80050A9C,
  `notify_behaviour_pass` 14491-14494 -> `coop_stay_change_end` 14432-14490): player 1's camera set and prompt slots go
  back, identity back (`coop_be_player2(false)`), the room word and slot table are made player 1's again
  (14460-14466), player 2's arrival camera is tagged (`g_stay_arrived`), `g_coop_room_p2 = false`, and R4c's
  `stay_scene_look_again` (18104-18118) re-arms player 1's in-world scene look if one was on.
- The stayer whose body the strips spare is player 1 then (`coop_stay_stayer`, 14359: `g_stay_bracket_was[1]`).
- One change at a time: a second fn66 room ask while a change is on its way is passed over (17437-17444).

## 4. The bring path (before separate rooms, and still the code under the stay's gate)

[read 22404-22622, 11400-11407, 20984-21000]

- The comment block at 22404-22407: "The players stay in one room: when player 2's is not player 1's (player 1 went
  through a door, or player 2 did alone), player 2 is put in player 1's room, beside them."
- During the countdown the other player is not held: "they go on as the one who asked does, until the room changes and
  they are brought along" (11400-11405).
- `coop_follow(cpu, player)` runs once a frame after the turns (30760). When player 2's object room +592 differs from
  player 1's and `coop_stay_apart` is false (22541 on):
  - finds the arrival marker of the last ticket (`coop_arrival_marker`, 22430-22459: ticket +8 room, +16 flags, +28
    entry; `0x80155348(entry, 2)` then `0x80151914(index, 2, &heading)`; not when flags bit 1, "arrives at a saved
    place"),
  - places with `coop_place_facing` beside the arrival spot; if player 2 asked (`g_coop_entered_p2`), player 2 takes
    the arrival spot and PLAYER 1 is moved beside it (22561-22568), because without the stay the game's change moved
    the game's player (player 1's node) [read 22561-22568; the "why" inferred from the comment at 21392],
  - copies player 1's facing (+44..+59) to player 2, calls `COOP_SPAWN_SET_ROOM(player 2, room)` (the game's set-room
    0x801F64C8 per the comment at 22403-22404) [read 22569-22573],
  - resets their look/find state, sees a door motion (state 121) out as arriving (80) with 0x801F64A0/0x801F6488, lets
    go a door hold with 0x801F6828, logs `[coop] player 2 brought to player 1's room` (22576-22621).
- `coop_follow` only ever moves player 2's object into player 1's object's room (the room is read from o1+592, 22465)
  [read]. There is no code that brings player 1 into player 2's room: under the stay a change of player 2's makes
  player 2 the game's player (R4), and nothing then moves player 1 [inferred from 14405-14416 and 22465].
- Also before the stay, `coop_left_behind` (16882-16893) made player 2's look wait until brought, and
  `coop_p1_unplaced` (20989-20999) did the same for player 1 after player 2's door [read].

## 5. Scenes and cutscenes while apart

What is built [read]:
- `cutscene_playing` (3869-3875) is global: a movie file open, or the scene flag with the letterbox up. Nothing in it
  is per room.
- A cutscene at a door's ticket refuses the split window: both stay held, as before (25957).
- R4b: player 2's door during player 1's in-world scene keeps that scene's waits and busy bits (27742-27810).
- R4c: an arrival during the other's in-world scene keeps the arriver's own camera and re-arms the scene's look
  (`stay_scene_look_again`, 18098-18118; 13219-13222 for a door's script step let-go).
- Scene camera ends apart: `coop_scene_set`'s afresh rule leaves player 2's set alone apart (`!(r6_apart(cpu) &&
  !fresh)`, 18077 region), and the arrival-scene camera copy is skipped apart (`!r6_apart(cpu)`, 19495).
- A cutscene of player 2's saves player 1's object place (60 bytes, `g_coop_cutscene`, 29314-29319, written at
  30007-30010) and `coop_cutscene_over` (29385-29408) writes it back (`coop_cutscene_place`, 29321-29328: bytes 0-11
  and 44-59, skipping 12-43), but only in the same room: it returns early if any room was asked since the cutscene
  began, "A cutscene that ends in another room (the Tome taken)" (29390-29393).
- The whole-frame mode (`g_room_whole_owner`, `split_skip_views_set`, 576-577) is set only by the Tome copy's turn
  (15630) and the shared Tome's own frames (16009); every other call sets 0 (13547, 13573, 15351, 15417, 15722,
  15914). Cutscenes do not use it [read].

What is not built [read]:
- docs/research/separate-rooms/design.md:110-112, R7: "a cutscene (mode 6 replaces the actor list) and a level change
  are option C moments: the other player is brought first". No R7 code is in hle.c (no function or comment names R7)
  [read: grep of hle.c]. docs/coop-next-session.md:165 lists "R7 the Tome, saves and cutscenes while apart" as to do.
- The R6 family (25437 on) is enemies, hits, routes and creatures, not scenes [read 25437-25463, 27631, 27731].

So today a cutscene or flashback begun in one player's room while the other is apart runs with no co-op handling of
its own beyond the above: the other player's room and body stay where they are [inferred: no apart branch at the
cutscene start, 29990-30030, and none in `cutscene_playing`].

## 6. The run: logs/run-20261114-173144-fx-a-1 (the latest fx-a-1)

Set up by docs/research/separate-rooms/build-run-fx.sh:3,15,17 [read]: `saves/fx-ch4.state` (a fresh Jump-to-Game
chapter 4 state saved at frame 2700, line 10), `ED_COOP_STAY=1 ED_ROOM_SLOTS=2`, `ED_RUN_SCRIPT=60:132,120:132`
(script 132 run as player 1 twice), player 2 stays in 338. All lines below from page-0001.log [read]:

- 2713 `[split] on ... (side by side)`. Player 1 at 104 -3 10, player 2 at 98 -178 10, both room 338.
- 2774 script 132 run: asks CLIMB DOWN (`[trace] words: ... CLIMB DOWN`). 2833 run again: `busy r3 2 r4 1` (a hold).
- 3021 script 132 resumes from the timers (lr 0x80163E5C), `[coop] a room change of player 1's`, `[room] ticket
  0x00000005 0x0CA41C95`, `[split] player 1's door from room 338 (to room 5): player 2 stays in room 338`, `[stayer]
  player 2 stays: their presses go on`.
- 3021-3145: `[trace] room move +2 .. +124`: the countdown, player 2 at 98 -178 10 throughout. 124 frames for
  transition 3, which `ed door 5` lists as 62 ticks [read: ed door 5 output].
- 3145 `[room] change 0x00000005`; table before +0 1 +4 0, slot 1 holds 338; `[stay] the load's strip passes over
  player 2's node`; `[rooms] slot swap (a fresh load ...)`; `unpack asked: room 5 into slot 0`.
- 3151 `[stay] the arrival's walk passes over player 2's node`.
- 3152 the tail: `[split] over ... 2172 asks answered not busy, 436 kept`, `slot 1's record put back: room 338`,
  `[stay] player 2's hold from the ticket let go`, table after +0 0 +4 1, slot 0 room 5, slot 1 room 338.
- 3153 script 1161 (the ticket's 4th, camera only per docs/game/research/transitions.md:255) then 2675, room 5's own
  script: it spawns the Tome at TomeSpawnMkr (`ed script 2675`: `hold[fn21](1)`, `spawn[ed3](25, 6, -1, 30, ...)`,
  `place_at_marker[ed202](L11, TomeSpawnMkr)`) [read]. So room 5 is the Tome room.
- 3155 on: lights, fog and sound per room; `[stay] frame 3155: player 2 stays in room 338 (slot 1) at 98 -178 10`;
  3159 `[stay] player 2 stays in room 338 (slot 1) while player 1 went to room 5` (the `coop_follow` line).
- 3175 `[stayer] the door's fade-in is over: mover 1 forgotten`.
- Run ends at 3442 with no Tome taken and no cutscene: player 1 at 1072 -5 336 in room 5.
- No log line names the burn-away dissolve. R4's half marking is only on the fade quad at lr 0x801E7F60
  (`r4_fade_quad`, 25897-25932) [read]; whether transition 3's dissolve draws through that quad is not shown by this
  log [inferred: the brief says the dissolve showed in the stayer's half, which suggests another draw].

## 7. Where a per-change "bring both into this room" could be decided

The earliest point that knows the target room and the asker is `notify_room_ticket` (11424; r3 = the room, per 28167)
[read]. `split_ticket_open` runs at its end (11455) and `notify_ticket_run`, `notify_room_change`, the strips and the
tail all come after it in the same change [read, section 2]. A per-ticket decision ("this change brings the other",
e.g. by target room or by the asking script) made there would have to be honoured by every per-change site that today
tests `coop_stay()` or `coop_stay_apart()` [inferred from the reads above]:

| Site | Today under the stay | For a bring |
|---|---|---|
| `notify_room_ticket` 11437-11440 | player 2's change not counted for player 1's scene; `g_fade_mover` set | count as a change of both; fade both halves [inferred] |
| `split_ticket_open` 25935 | opens the stayer's window | no stayer [inferred] |
| `notify_ticket_run` 14405 | R4 bracket for player 2's change | the bracket would still make player 2 the game's player; player 1 then needs bringing [inferred] |
| `room_slots_return` 15109 | swaps so player 1's old slot is evicted | which slot to evict is the question (below) |
| `coop_stay_spared` 14360 (strips) | spares the stayer's body | not spared, as before the stay [inferred] |
| `notify_room_slots_tail` 15071-15085 | stayer's record put back, hold let go | the brought player's old room has no one in it: its record is not put back [read 15073-15078] |
| `coop_follow` 22532 | stays | falls through to the bring (22541-22621) |
| `coop_p1_unplaced` 20990, 17995, 11333 | player 1 not moved by player 2's door | player 1 brought [inferred] |

Constraints read on the way:
- Two slots only [read 13644-13653]. At a bring from apart (A and B resident), the Tome room T must evict one; the
  tail keeps only rooms a player is in (15073-15078), so after both are in T, neither A nor B is "kept" and both
  returns are fresh loads (15096-15108: "A return into a room nobody stayed in is a fresh load"; the solo freeze
  hang-20261112-025508 came from re-using a dead record) [read].
- One change at a time (17437-17444; the ticket is one record) [read]: two returns to two rooms would be two changes in
  sequence [inferred].
- `coop_follow` brings only player 2, into player 1's room (22465) [read]: a bring of player 1 into player 2's room has
  no code [inferred].
- The game has its own "arrive at a saved place": ticket flags bit 1 makes the runner call `0x8016490C(0)` instead of
  the marker lookup (disasm 0x80152680-0x80152690) [read]; docs/game/research/transitions.md:270 says slot 0 is a table
  at 0x805CC2BC, 6 bytes a slot, written by `fn104(slot, object)`, used by L06/1838 only, and bit 1 leaves the heading
  unset (transitions.md:276) [read in docs, not re-traced here]. `coop_arrival_marker` already skips such tickets
  (22437) [read].
- The game also keeps a scene's room: "0x800ADA24 tickets the current player to the scene's kept room" (ed145's scene
  return; comment 26019-26023) [read comment, not traced].
- The Tome's use L00/1599 (the way back) goes to the chapter room's EntryMkr_TomeReturn with transition 0, then 868, for
  rooms 5 193 255 258 (`ed door 5`) [read]. A change with r4 flag 4 is "the Tome giving the room back, event 57's
  reload" (11325, 11331) [read].
- The host's place-restore precedent is `g_coop_cutscene` / `coop_cutscene_place` (29314-29328), same room only [read].

## 8. Open, for the design

- Whether the strips (0x80043AB8, 0x80043820) are safe for a brought player whose +592 is still the old room at the
  arrival walk (the "not-in-room branch would let go of their parts and moves", 15120) [inferred risk, not traced].
- Which room the Tome's return ticket (L00/1599) should go to when the two players came from different rooms, and how
  the second player's return is made (a second ticket, or a saved-place ticket) [open].
- Whether the flashback is a movie (`g_movie_file`) or an in-world scene, and whether its own return ticket would meet
  the split window's refusal (risk 12, 26017) [open].
