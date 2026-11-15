# Reader 4: putting each player back after the Tome room (2026-11-14)

Read only. Nothing was edited, built or run. Sources: `runtime/host/hle.c` (working tree on 3542bf0), `runtime/host/main.c`,
`build/ed-db/disasm.txt` through `ed dis` / `ed fn`, `ed script`, `ed door`, `ed room`, `ed item`, the DOL's cinematic table
(read with `tools/ed/disc.py`'s `Dol.word`), `docs/game/systems/doors.md`, `docs/game/research/transitions.md`,
`docs/coop-level3-design.md` (R1-R5), the separate-rooms readers named below, and `logs/run-20261114-173144-fx-a-1`.
**[read]** = seen at the place given. **[inferred]** = reasoned from those reads.

Words used:
- **T**, the taker: the player who took the transition into the Tome room. **RT**: their room then. **PT**: their place then.
- **O**, the other: brought into the Tome room. **RO**, **PO**: their room and place then.
- **K**, the finder: whoever picks up the Tome (T or O).
- The Tome room is room 5 in every chapter that has one (below).

---------------------------------------------------------------------------------------------------------------
## 0. In short

1. **RT is always gone by the return. RO is still in its slot, unless a change's tail marked its record dead.** The trip's
   own change into room 5 unpacks into T's old slot, and the flashback loads no room (sections 2 and 3).
2. **The game already has "go somewhere, then come back to where you were".** `fn104(0, player)` saves a place in slot 0
   of `0x805CC2BC`. `fn66(room, ..., flags 1)` then arrives at that place instead of a marker. L06/1838 does exactly this
   (section 4).
3. **The cheapest correct order is O first, then the game's own return for T.** Keep the ticket that 1599's `fn66` writes.
   Write O's ticket in its place: room RO, flags 1, slot 0 := PO, O's camera hash, no script. When O's change is done,
   write the kept ticket as T's. T arrives at EntryMkr_TomeReturn and 868 runs as theirs, with its scene and the flag that
   changes the world.
   - Any other order costs a second fresh load, and puts O's change inside T's cutscene (section 7).
4. **Nothing of a room needs saving by the host across an unload.** In solo every return to a room is a fresh load already,
   and what lasts is level-wide: nodes, flags and script words. The host keeps only the two places, the two facings and
   O's camera hash, and puts back the 6 bytes of saved-place slot 0 after O's change (section 6).

---------------------------------------------------------------------------------------------------------------
## 1. What the game's Tome trip does, as far as the return needs it

- **Room 5 and the returns pair up.** The ways into room 5 come from rooms 150 (L03/892), 338 (L04/132), 303 (L06/335),
  115 (L07/1111), 204 (L08/327), 17 (L10/1215) and 43 (L11/358). Script 1599's `fn66` returns go to 150, 338, 303, 115,
  17, 43 and 204, each at EntryMkr_TomeReturn with transition 0, then script 868 [read: `ed door 5`].
  - So the game's return room is the room the trip was taken from, RT [inferred from the pairing].
  - The exceptions in 1599: models 9 and 142 go on to room 64 at EntryMkr02, with transition 2 and script 1828. Model 18's
    branches (255, 258) don't start in room 5. Model 69's branch (cinematic 118) has no `fn66` at all: it gives the magick
    in place (`ed10(-1)`, flag 515) [read: `ed script 1599 --level 4`, lines 237-262 and 317-327 of its print].
- **1599 is the Tome's after-find script.** Item 30, Tome of Eternal Darkness: "AFTER FIND script 1599", in levels 00,
  03-11, 13 and 16 [read: `ed item 30`].
  - In Karim's branch (model 150, room 5), state 0 is `play_cinematic(113)`, then `fn1(1)`. State 1 is
    `fade_set(255,255,255,255)`, `stream_play(81,...)`, then `fn66(338, EntryMkr_TomeReturn, Camera01, 0, 868, 0)`
    [read: `ed script 1599 --level 4`].
- **The flashback cinematics load no room.** Table `0x80242150` has 32 bytes a row. Rows 112-120 (the Tome cinematics
  1599 plays) all hold 0xFFFF in the room halfword at +8, and 0x063F = 1599 in the low half of +12 [read: the DOL's
  bytes].
  - Mode 6's start reads that halfword (`lha 8(r3)` at 0x80050354). At -1, and with the row not 96, it skips the direct
    load `0x801525D4(room, -2, 0, 0, 128)` [read 0x80050340-0x800503F8].
  - So no slot is touched during the flashback, and at the cinematic's end the game runs 1599 again [inferred: "record +14
    is the script run at its end", readers.md:506].
  - Rows 112, 113, 115, 116, 117, 119 and 120 also hold a room in the high half of +28: 150, 338, 303, 115, 204, 1 and 43
    [read]. What reads +28 was not traced.
- **Karim's 868 runs 1349, and 1349 is the world change in RT.** 868 sets the fade to white, fades in over 40, then runs
  the next script by the player's model: 150 runs 1349 [read: `ed script 868`].
  - 1349, in room 338: `fn22(1,1)` (a cutscene), `flag_set(522, 1)`, the zombies with tags 174-178 woken, their quarry set
    to `fn78(1,0)` (the player), `fn22(0,1)`, then `run_next(132)` (the ladder) [read: `ed script 1349 --level 4`].
  - So the return must arrive as the player who is in RT, with RT the room in play, or the zombies hunt the wrong
    body [inferred].
- **In room 338 the return marker is where the taker stood.** EntryMkr_TomeReturn is at [-950, -194, 1], the same place
  as EntryMkr01, inside Region01 (the ladder's CLIMB DOWN box) [read: `ed room 338`].
  - In room 43 the marker [-134, 1200, -78] lies inside Region02, its way to room 5 [read: `ed room 43`].
  - So for T, the game's own return is already "back where they took the transition" [inferred for the chapters not
    read].

---------------------------------------------------------------------------------------------------------------
## 2. How a change loads a room, with two slots

- **The runner, 0x80152614(room, entry, camera, script, flags)** [read `ed dis 0x80152614`]:
  - It calls `0x80152298(room, flags)` at 0x80152654.
  - It places the player r13-26524, unless r13-26280 is set: `0x80155348(entry, 2)` and `0x80151914` give the marker,
    `0x8016E764` turns it into floats, `0x80130EF0(player, pos, 10.0)` places them.
  - It sends event 238. A motion turns them by the heading + pi (event 191). No motion faces them by the heading
    (`0x80124BE4` at 0x80152734).
  - Then `0x80043380`, event 62 to the nodes of r13-23276 (0x80152780), the camera block (`0x801EE998(hash, 2, 0)`,
    `0x801EE764`, `0x801ECCE0`, `0x801EEB58(2)`, `0x801EEA9C`), and the script at 0x801527E0.
- **The load, 0x80152298 to 0x80154C94(wanted, reply)**:
  - The old room goes to r13-23288 and r13-23276 := wanted. The asker's object +592 := wanted when the 7th argument is not
    0. Then the level-wide strip 0x80043AB8 [read change-freeze-code-readers.md §2].
  - "Already in the other slot" compares only the room id of the other record's +0x8140. Then it swaps and replies at
    once, with no unpack.
  - Otherwise it swaps and unpacks into the former other slot. **The load has no choice of which slot to evict** [read
    there].
  - Then the arrival `0x80151FDC`: the walk 0x80043A5C, event 61 to the room left (0x801520F4), the prompt slots reset,
    every busy bit cleared (0x80152184), the 10-tick fade-in (0x801521A8).
  - Then the tail 0x801523A8-0x80152404 marks the other slot's record dead (room -1, loaded 0) [read there].
- **What the host does with two slots** (hle.c):
  - `room_slots_return`, at the change's entry (15092-15117). When player 2 is apart in the slot the engine would unpack
    into, it swaps the table's +0/+4 so that player 1's old slot is evicted. A return into a room nobody stayed in is a
    fresh load, because its record is dead [read].
  - `notify_room_slots_tail`, at 0x80154B54 from lr 0x80152434 (15062-15087). It puts back the record of every room a
    player is in (the room in play, and the stayer's), and lets go of the stayer's ticket hold (`0x801F6828(stayer, 0)`)
    [read].
  - The strips pass over the stayer's node: `coop_stay_strip_walk` and `coop_stay_kind_ask` (15121-15145) [read].
- **R4: a change of player 2's runs as theirs.** `notify_ticket_run` (14395-14431) opens the bracket when
  `g_coop_room_p2`: player 2 made the player, r13-23276 := their room, the prompt slots saved, their camera set in.
  - The table is left as it is, so the engine unpacks into player 2's old slot [read doors-readers.md].
  - `coop_stay_change_end`, at the behaviour pass (lr 0x80008400, or 0x80050A9C for mode 6's pass), puts player 1's room
    and slot back as current. It tags player 2's arrival camera (14432-14510) [read].
- **Who owns a ticket.** `notify_room_ticket` marks the change player 2's when r13-21496 is player 2's id at the ticket, or
  when the ticket comes from their way-out script (11425-11428) [read]. So a ticket written with `coop_be_player2` on is
  player 2's, and the R4 bracket runs it [inferred from 11427 and 14408].
- **Seen: the trip from a fresh Jump to Game** (logs/run-20261114-173144-fx-a-1/page-0001.log 194-298) [read]:
  - Player 1 took 338's ladder into room 5. Room 5 was unpacked into slot 0, which was empty (room -1). Room 338 stayed in
    slot 1, its record put back ("a player is in it", frame 3152).
  - "player 2 stays in room 338 (slot 1) while player 1 went to room 5". The change took frames 3145-3152.

---------------------------------------------------------------------------------------------------------------
## 3. Where each room is when the return comes

- **The trip evicts RT.** Both rooms are resident before it.
  - T is player 1: `room_slots_return` swaps so that player 1's old slot, RT, is the one unpacked into (hle.c:15108-15115).
    The swap has run in play: "player 2's room 341 is in slot 1: the indices swapped so room 344 is unpacked into slot 0,
    player 1's old" (logs/run-20261111-154849-jtg-rooms-r2-state/page-0001.log) [read].
  - T is player 2: the bracket leaves the table, so the engine unpacks into player 2's old slot, RT [read
    doors-readers.md; level3 R4].
- **RO stays in its slot**, its data untouched as long as nothing unpacks into it [inferred from the two above]. Its
  record is live after the trip's tail, because O was still in it [read 15077-15080]. It stays live until the next
  change's tail with nobody in it.
  - O brought by a host move (the bring) runs no change, so RO's record is still live at the return [inferred].
  - O brought by a change of their own into room 5 kills RO's record at that change's tail. RO is then reloaded fresh,
    by the existing rule [inferred from 15096-15107].
- **The flashback loads no room** (section 1). So at the return the slots are {room 5 current, RO other}, and RO may be
  live or dead [inferred].
- **RT == RO (together at the trip):** room 5 replaced RT, and the other slot holds whatever was there before [inferred].

---------------------------------------------------------------------------------------------------------------
## 4. Setting a player's place and facing

- **The marker.** `0x80155348(entry, 2)` gives the index, `0x80151914(index, 2, &heading)` gives three halfwords at +44 and
  the heading, `0x80130EF0` places [read 0x80152694-0x801526CC; doors.md].
- **The placer `0x80130EF0(object, pos, f1=10.0)`** rays 10 units against the CURRENT section (`0x801551A0(2)`). It places
  at the hit through `0x80118264(x, y, z)`, else at the point [read 0x80130EF0-0x80130FD4]. Inside a change the new room is
  current, so it is the right floor for the mover [inferred].
- **The saved place, the game's own "come back to where you were":**
  - `fn104(slot, handle)` (0x8016AD10-0x8016AD70) writes the thing's place as three s16 at `0x805CC2BC + slot*6`, cut by
    `fctiwz`. `0x8016490C(slot)` returns that address [read].
  - The runner's flag bit 1 (`rlwinm. r0,r25,0,31,31` at 0x80152680) calls `0x8016490C(0)` and skips the marker lookup
    (0x8015268C-0x80152690). The placing then goes on as for a marker [read].
  - **The heading is left unset with bit 1.** 16(r1) is never written, and 0x80152730 or 0x80152704 passes it to
    `0x80124BE4` [read; transitions.md "Bit 1 leaves the heading unset"].
  - All 152 `fn104` calls name slot 0 [read transitions.md]. So slot 0 is a shared word that a script may be holding.
- **The game's precedent, L06/1838:**
  - `VampReturnRoom = fn25()`, `fn104(0, player)`, then `fn66(313, EntryMkr01, Camera06, 0, 1838, 0, 0)`.
  - Later `fn66(VampReturnRoom, EntryMkr01, Camera01, 0, 2252, 1)`: flags 1, the saved place. Script 2252 then sets
    Camera01 and ends its scene [read: `ed script 1838 --level 6`, `ed script 2252 --level 6`].
- **Facing.** `0x80124BE4(object, f1 angle)` writes the object's +44 rotation [read 0x80124BE4-0x80124C10]. The host already
  calls it (hle.c:9652, 9816, 28805) [read]. coop_follow copies the four words +44..+59 between players, and logs the
  heading as `2*atan2(+52, +56)` (hle.c:22533, 22563-22564) [read].
- **The host's own direct puts** [read]:
  - ED_PUT (`sweep_put`, hle.c:15784-15838) is test-only. It writes +0..+11 after a floor ray in the player's own room's
    world (`camera_collide_object`), and leaves the facing alone.
  - coop_follow writes place and facing, then stamps the room with `0x801F64C8(node, room)` (22555-22568).
  - Neither runs a room's arrival (events, camera, zones). They are for the same room only [inferred].

---------------------------------------------------------------------------------------------------------------
## 5. The camera after

- The runner looks up its camera by hash in the current slot: `0x801EE998(hash, 2, 0)` walks the 120-byte entries for +52
  == hash, and gives the index or -1 [read 0x801EE998-0x801EEA24].
  - What `0x801EE764` does with -1 was not read. So the hash given must be one of the arrival room's cameras.
- **The hash a player is on.** The eye record `0x80603178 + index*136` holds the room's entry at +124, and the hash is at
  that entry's +52. The index is r13-26864 (hle.c:18610-18617, the kit's game-cam) [read].
  - The +124 entry points into the room's slot data. Keep the hash, never the pointer: the pointer is stale once the
    room is evicted [inferred from §5 of change-freeze-code-readers.md, "the slot data is position-dependent"].
  - Player 2's set is in memory only in their turn or a bracket; otherwise it is the host's copy (`g_coop_cameras`) [read
    design-arrival-camera.md step 3].
- **Player 2's change.** The bracket cuts their own set to that camera. `coop_stay_change_end` tags it with the room and
  its zone list, and `coop_zones_new` keeps it at their first new list (hle.c:14467-14481) [read].
- **Player 1's change.** The game's own set gets the cut [read design-arrival-camera.md].
- **The first look picks again.** The first zone look in the 10-tick fade-in lets the camera region the player stands in
  choose (camera.md:40) [read]. With O's zones marked as come-into (section 7, step 2d), no region's enter script runs, and
  the saved camera stays [inferred].
- **T's camera** is the kept ticket's (Camera01 in 338), then 868's and 1349's own `fn8` [read: scripts above].

---------------------------------------------------------------------------------------------------------------
## 6. What a room's unload and reload keeps, and what the host must keep

- **Level-wide, so it survives any unload** [read readers.md:339, 376; enemy-readers-2.md:118]:
  - the one node list (enemies, pickups, characters), each stamped with its room at +592;
  - the flags (fn29/fn30);
  - the script words (already per player in the host);
  - the carried lists, the Tome, the element pool.
- **Per slot, rebuilt by every fresh load** [read readers.md:46, 77; design.md fact 2]:
  - geometry and collision;
  - the zones and their inside bits (+104);
  - markers, cameras (eye entries), the light grid;
  - the sound and ambience emitters (`0x8019F3F0` at the arrival);
  - the AI point graph (`0x800BA530` at the change).
- **In solo every return to a room is a fresh load**, since N is pinned to 1 [read design.md fact 1]. So a fresh load of
  RO or RT is the solo re-entry, with nothing for the host to save of the room [inferred].
- **The events of a change:**
  - 61 goes to the nodes of the room left. Enemies in state 0 drop their route and unregister. Some kinds end themselves
    [read doors-readers.md:60; change-freeze-code-readers.md §3].
  - 62 goes to the new room's nodes [read 0x80152780].
  - A host-moved O (the bring) sends no 61 to RO's nodes, so RO's enemies keep chasing a body that left. At the return,
    RO's nodes get 62 from O's change [inferred]. Whether the trip should send RO's nodes the game's 61 belongs to the trip
    reader; it is noted here because the return sends the 62.
- **Never reuse a dead record.** A return into a room nobody stayed in must load fresh (the 2026-11-12 solo freeze,
  hang-20261112-025508; hle.c:15096-15104) [read]. A live record with its data untouched is the game's own "already in
  the other slot" path, the reunion case in the R2-R4 runs [read].
- **What the host must keep from the trip to the return** [inferred]:
  - per player: room (+592), place (+0..+11), the facing words (+44..+59) or their heading, and the camera hash (the eye
    record's +124 entry's +52, with that player's set in);
  - for the return: the kept ticket (registers r3-r10 and the three stack words);
  - the 6 bytes of saved-place slot 0, written before O's change and put back after it;
  - a host blob for save states taken during the trip, as R7 says for "whose room" (design.md R7).

---------------------------------------------------------------------------------------------------------------
## 7. The sequence proposed: O first, then the game's own return for T

### Why this order

- **T first can't be cheap.** At the return the slots are {5 current, RO other} (section 3). T's change 5 -> RT is a fresh
  load, so it must evict something:
  - With no steer, the engine evicts the other slot, RO. Then O, still standing in room 5, needs a second fresh load
    later.
  - With a steer, it evicts room 5's slot, but O is standing in room 5.
  - Worse, 868 -> 1349 starts T's `fn22` scene at once (section 1). So O's change would have to run during the other
    player's cutscene, which the host doesn't support today:
    - `coop_room_change_on` reads false under a cutscene (hle.c:11408-11412) [read];
    - no stayer window opens under one (design-stayer-presses.md §15) [read];
    - R4d, a door beside the other's scene, is designed but not built [read: no `g_r4d` in hle.c].
  - Otherwise O waits in room 5 for the whole scene, about 930 ticks of waits in 1349 [inferred from the waits read].
- **O first works.** O's change 5 -> RO finds RO live in the other slot (no unpack), or loads it fresh into RO's own slot.
  Room 5 stays for T either way:
  - O is player 1: no steer, since T's room 5 is the current slot, not the other, so the fresh load goes into the other
    slot.
  - O is player 2: the bracket unpacks into the former other slot.

  Then T's change 5 -> RT loads fresh into room 5's slot:
  - T is player 2: the bracket evicts their old slot, room 5.
  - T is player 1: `room_slots_return` sees player 2's RO in the other slot and swaps, so player 1's old slot, room 5, is
    evicted [read 15108-15115].

  This ends with both resident and apart. The cost is one fresh load plus one load that is fresh or none [inferred].
- **Together at the trip (RT == RO) is the same sequence.** O's change 5 -> RT is fresh. T's change 5 -> RT is the reunion,
  "already in the other slot", since O is in it and the tail puts it back [inferred].

### The steps

1. **At the trip**, before its change runs: record for each player what section 6 lists, and mark "a Tome trip is on".
   - The saved place should be taken where the player stands still, not in a door's countdown, a close-up or a scene
     [inferred]. When O can be taken belongs to the trip reader.
2. **At the Tome's return ticket:** 1599's `fn66`, while the trip is on. Its transition-0 path calls the writer at
   0x801668E8 with r8 1, r9 0, r10 0 and stack words 0, 0, 1 [read 0x801668B8-0x801668E8].
   - a. **Keep it.** In the existing intercept on the writer 0x80155770 (`split_ticket_ask`, the G1 refusal, hle.c
     ~26000), save r3-r10 and the caller's three stack words at r1+8, r1+12 and r1+16. The writer reads them as 24/28/32
     after its `stwu -16`: +36, +40 and +24 of the ticket [read 0x8015577C-0x801557DC]. Then void the writer, as G1 does.
     - `fn66` still lets the player go (0x801F6828) and cuts the sound 0x801A05EC(0,1,0) [read 0x80166798-0x801668F8].
     - D11 in design-r4d.md designed this same keep (`g_r4d_later`), but it is not built: the name isn't in hle.c [read: a
       grep].
   - b. **Write O's ticket.** Save slot 0's 6 bytes, then write PO into it as s16, cut as `fn104` cuts.
     - Guest-call 0x80155770 with: room RO, entry any hash (unused for placing with bit 1, but passed on to
       `0x80043380`), camera O's saved hash, script 0, flags 1, countdown 1, delay 0, transition 0, and stack words 0, 0,
       1 (the 7th must be 1: the stamp of +592 at 0x80154D20).
     - The stack words go at `((r1 - 0x800) & ~15) + 8..16`, the r1 the guest call will use [read main.c:79-80].
     - If O is player 2, wrap the call in `coop_be_player2`, so the ticket is theirs and R4's bracket runs it [read
       hle.c:11427, 14408]. A running guard is needed on the intercept's own re-entry [read refute-r5.md P4: the same
       address re-enters its intercept].
   - c. **O's facing.** With bit 1 the runner faces O by stack garbage. Answer `0x80124BE4`'s f1 with O's saved heading,
       in an intercept at lr 0x80152738 (the no-motion branch) and 0x80152714 (the motion branch) for this change only
       [read 0x801526F8-0x80152734]. Or write the four facing words at the change's end, as coop_follow does.
   - d. **O's zones.** Mark the zones O stands in as come-into without running their scripts, as a brought player's first
       look does (`g_follow_fresh`, `coop_follow_look`; the game's `0x80134D48` marks what the player stands in as inside)
       [read doors.md round 5].
     - O was inside those zones before the trip. Without this, a zone exit or scene region at PO runs again: the game
       doesn't keep an arriver out of the region they arrive in [read doors.md] [inferred].
   - e. **When O's change is done** (`coop_stay_change_end` for player 2, or the behaviour pass after the runner for player
       1; both at lr 0x80008400 or 0x80050A9C, hle.c:14514-14516), put slot 0's 6 bytes back.
3. **Write the kept ticket as T's**, at the next point a change is allowed: busy 4 clear (0x80151FDC cleared every busy
   bit), no window open (G1). Wrap it in `coop_be_player2` if T is player 2.
   - The game does the rest as in solo: T placed at EntryMkr_TomeReturn, 868 then the model's script run as T in RT
     (1349's scene, flag 522, the zombies' quarry T), T's camera.
   - O, the stayer in RO, is spared by the strips. RO's record is put back at the tail, the R5 walk keeps RO's sounds, and
     the fade is marked out of O's half by `g_fade_mover` [read 14359-14367, 15077-15080; level3 R4/R5].
4. **No game return to RT** (model 69's branch has no `fn66`): only O needs to go back, through step 2b with no kept
   ticket. **A return elsewhere** (models 9 and 142 to room 64): T goes where the game sends them, and O goes back to RO
   [inferred: the user's rule applied to the game's own end].

### What each step needs that the host lacks today [read: greps of hle.c]

- the trip record (section 6);
- the keep and replay of a ticket (D11's shape);
- a host-written ticket for a player, with the guest call's stack words;
- the heading answer for a bit-1 arrival;
- slot 0 saved and put back;
- the fresh-look mark for O at a change of their own (today `g_follow_fresh` is armed only by the bring and the fall
  rescue: 22508-22509, 22566-22567).

---------------------------------------------------------------------------------------------------------------
## 8. Considered and not proposed

- **T exactly at PT, with flags 1 on the kept ticket and slot 0 := PT.** Possible by the same means. But the game's marker
  is already at the trip spot in 338 and 43 (section 1). In chapters 3 and 6 the way in is an unprompted zone exit
  (L03/892 SKPlane01, L06/335 BoxRegion02, "asks nothing") [read `ed door 5`], so PT lies inside the exit. The game's own
  marker is the spot the game made for this. Left to the user.
- **O host-moved into a live RO in the same change as T's** (one load only). The steps would be: O's +592 := RO through
  `0x801F64C8`, place and facing written at the runner's entry, the table steered so room 5's slot is evicted.
  - It runs none of O's arrival: no event 62, no camera cut, no zone reset. Player 1's camera set would still be room 5's.
    Each would have to be rebuilt by hand, against "the game's own functions".
  - It works only while RO's record is live. Not proposed [inferred].
- **ED_PUT or coop_follow's writes across rooms.** Same room only (section 4). Kept as the same-room fallback: for
  example, O put at PO after a bring when the two shared RT and a second change is unwanted.

---------------------------------------------------------------------------------------------------------------
## 9. Open, not read

- What `0x801EE764` does with index -1 (a saved hash not in RO's cameras: an eye on index 8 or 10, a finisher's camera).
  The fallback proposed is a camera of RO from `ed room` [inferred].
- r13-26280: the runner skips the placing while it is set (0x80152664) and clears it after the script. Its writers were
  not read. If it is set at O's change, O is not placed at all.
- The global fade during the two changes. 1599 sets the fade colour white before its `fn66`, and 868 fades from white
  over 40. With O's change in between, the white quad is marked out of T's half by `g_fade_mover` for O's change only.
  What each half shows in those frames is not known [read doors-readers.md: one fade state].
- Event 61 at O's change goes to room 5's nodes, T among them, standing in the white: "mild standing" by doors-readers.md:60
  [read there]. Not seen in a run.
- Whether mode 6's end writes its own ticket for rows 112-120 (`0x8012D1A0` needs a record's +120 room). What +28's room in
  those rows means. Both decide whether a third load happens between the flashback and 1599's state 1.
- That every chapter's TomeReturn marker lies at its trip spot: read for 338 and 43 only.
- Whether a host-written ticket's lack of fn66's press-time sound cut matters. R5's walk acts at the change itself (lr
  0x80152354) [read level3 R5].
