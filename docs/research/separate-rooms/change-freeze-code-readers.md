# The other player's brief freeze at a room change: the code of a change (reader)

Reader for the user's report of 2026-11-12: with the players apart, when one changes room the OTHER player's picture
pauses for a moment (not black). Read only: disassembly (`build/ed-db/disasm.txt`, `ed dis`), `runtime/host/hle.c`,
`runtime/host/interrupt.c`, `.deps/GXRuntime/backends/aurora/aurora_backend.cpp`, existing recordings and one save state
(read offline with `tools/ed/savestate.py`). Nothing built, nothing launched. [read] = seen at the address or line given;
[inferred] = reasoned, not seen.

## 1. The answer in short

- **What stalls is one world frame, not a run of skipped frames.** The ticket tick 0x80155830 runs at the START of the world
  frame 0x800083C8 (bl at 0x800083EC) [read]. When the countdown reaches 0 it calls 0x80152614 in that same frame
  (0x801558F4) [read], which calls 0x80152298, which blocks the game's only main thread in OSReceiveMessage
  (0x802019D4 at 0x80152398, queue 0x8058B028, flag 1 = block) until the slot/reader threads have unpacked the room [read].
  After it returns, the SAME world frame goes on: the behaviour pass 0x8014FDB8(1) at 0x800083FC, the scripts, the second
  pass 0x8014FDB8(0) at 0x800084AC, the draw, the fade tick 0x801E7DA4 at 0x80008560, the frame end [read]. So nobody's world
  step is skipped (not the stayer's, not the mover's): the frame just takes several retraces of wall-clock time.
- **How long.** The recording's `frame` column is the guest timebase / 675000 (one per 60 Hz retrace) and a row is written
  at the first pad read of a frame (both_record, hle.c:17584-17602, called from the PADRead hook at hle.c:4234) [read]; the
  timebase is paced by the wall clock (interrupt.c:430-519: retraces popped from wall time, `cpu->timebase += ticks`) [read].
  So a change shows as one row whose `frame` jumps by N with `ms` about N x 16.7:

  | run | change | who moved / stayed | retraces in the change frame | wall ms |
  |---|---|---|---|---|
  | user play logs/run-20261111-221141-rooms-play | 341 -> 342 at 1847 | P1 moves, P2 stays (were together) | 4 | 66 |
  | user play logs/run-20261111-222138-rooms-play | 341 -> 342 at 1961 | P1 moves, P2 stays (were together) | 4 | 67 |
  | logs/run-20261111-172710-rooms-r4c-t1-p2door | 183 -> 180 at 3826 | P2 moves, P1 stays | 4 | 66 |
  | logs/run-20261111-182644-rooms-r5-s1-p2door | 183 -> 180 at 3829 | P2 moves, P1 stays | 6 | 100 |
  | logs/run-20261111-182711-rooms-r5-s2-p1door | 183 -> 180 at 3833 | P1 moves, P2 stays | 7 | 116 |
  | logs/run-20261112-033738-p2-follows-181 | 182 -> 181 at 4770 | P1 moves, P2 stays | 6 | 101 |
  | the same runs, the way back INTO the stayer's room (4362-4364, 5054) | reunion | either | 2 | 33-34 |
  | solo, mod_coop 0, two slots (logs/run-20261112-030139-jtg-solo-returns) | every door, 2087-5629 | - | 3 to 5 | 50-84 |
  | solo (logs/run-20261112-030651-jtg-solo-gate) | every door | - | 3 to 5 | 50-83 |

  [read: both.tsv of each run, scratch scripts chg.py / gaps.py / roomgaps.py in this session's scratchpad]. Every other
  row of these runs is 16-17 ms apart, including the ticket's countdown frames (ticket 14 -> 1) for both players [read].
  So the freeze is 50 to 116 ms (3 to 7 missing pictures) on a fresh load, 33 ms (1 missing picture) on a return into the
  room the other player kept.
- **What the stayer sees.** The renderer presents only when the game sends a present (gxq::present, aurora_backend.cpp
  1609-1633); while the game thread is blocked the render thread only pumps window events (ring_next, 1972-1990;
  dol_aurora_pump_events_if_idle 3019-3027, and only after 100 ms without a present) [read]. Nothing re-presents. So the
  window keeps the LAST picture for the whole stall: the stayer's half is a held still, then it moves on by ONE game step
  [read code; inferred for the eye]. The mover's half is black by then (fade quad), so on their side the stall is invisible.
- **Why solo never shows it.** The same block exists in solo (3-5 retraces above), but there the fade quad covers the whole
  screen: at the firing the fade's alpha is at 255 (the `fade` column reaches 0 at ticket 3, and the quad is drawn while
  alpha > 0: 0x801E7DA4, doors-readers.md) [read]. R4's per-half fade (notify_GXLoadPosMtxImm, hle.c:3341-3349: the quad's
  matrix marked out of the stayer's view from the ticket on, g_fade_mover set at notify_room_ticket hle.c:3313-3314) keeps the
  stayer's half lit, so for the first time a player LOOKS at the screen during the game's synchronous load. The freeze is
  the game's own load time made visible, plus a part that is larger with two views (section 3).
- **Not VISetBlack.** The game's VISetBlack (0x8020C648) is called only through 0x801E4610, whose 36 callers [read: grep of
  `bl 0x801E4610`] include none in 0x80151000-0x80156000 (the change's code); 0x801E3D6C's VISetBlack(1) is graphics init.
  The host only latches what the game asks (notify_VISetBlack hle.c:3705-3716) [read]. So "VISetBlack per half (the load's
  3-4 frames black both)" in docs/coop-next-session.md is this held picture, not a black latch; the user's "not black"
  matches.

## 2. The change, from the ticket to the arrival, and whose data each step reads

All of it runs from the world frame's tick (0x800083EC -> 0x80155830), OUTSIDE any turn: the who words (r13-26524 object,
r13-21492 node, r13-21496 id), the live camera block (0x80602B18 / 0x80603178), the prompt slots (0x80335A38) and the busy
word r13-23344 in memory are player 1's, unless the host's R4 bracket put player 2's in for a change of theirs
(notify_ticket_run hle.c:13858-13869: coop_be_player2 swaps the who words, r13-23276 := player 2's old room, the six prompt
slots saved, coop_cameras_in(2); undone by coop_stay_change_end hle.c:13870-13888 at the behaviour pass after, lr 0x80008400)
[read]. Player 2's turn (coop_player_turn, run from player 1's controller in hle_coop_controller hle.c:20060) never contains
the change; their door's script writes the ticket in their use, the runner fires later in the world tick [read].

The ticket countdown (before the block) — frames run normally for the stayer:
- 0x80155770 (fn66's ticket): busy |= 4, event 102 to everyone but the asker (their node +28 bit 8), countdown = fade+2
  (doors-readers.md) [read there]. The host: notify_room_ticket (hle.c:11297-11329) marks the mover (g_coop_room_p2 for
  player 2's), g_fade_mover; coop_node_held_ask (hle.c:19459-19479, on 0x801F68D4) answers "not held" for the NON-asker's
  node while coop_room_change_on (hle.c:11281-11287) [read]; for player 2's change player 1 gets a turn of their own with the
  hold opened (room_turn, hle.c:20048-20082) [read]. both.tsv shows 16-17 ms rows through ticket 14 -> 1 [read].

0x80155830 (the ticket tick, the world frame's first act) [read 0x80155830-0x80155924]: ticket 0x8058AFFC +0 countdown,
+4 a delayed-fade countdown (0x801E7C88 at 0); at 0: kind (+20) 17 -> 0x80152298 + event 62 to r13-23276's nodes; else
0x80152614(+8 room, +28 entry, +32 camera, +12 script, +16 flags); then a callback at +36(+40). One ticket, level-wide.

0x80152614 (the runner) [read call list 0x80152614-0x80152810]: 0x800436C4, 0x8004305C (the identity), 0x80152298 (the
load), then reads r13-26524 (0x80152658, 0x801526C0, 0x80152708, 0x8015272C) to place "the player" on the entry marker
(0x80155348, 0x80151914, 0x8016E764, 0x80130EF0) and face them (0x80124BE4); events via 0x801F59B0; 0x800BA530 (the AI point
list rebuild from the current record); 0x801755B8 and event 62 to the NEW room's nodes (0x80152780, room r13-23276); the
arrival camera into the live block (0x801EE998, 0x801EE764, 0x801ECCE0, 0x801EEB58, 0x801EEA9C); the arrival script
(0x801642D4 at 0x801527F4). Player 1's change: all player 1's. Player 2's change: the bracket makes "the player" theirs and
their camera set the live one [read hle.c:13858-13869].

0x80152298(room, flags) (the load) [read 0x80152298-0x80152478]:
1. 0x8022C304(1) at 0x801522C8: OSSetThreadPriority(0x8061C658, 8) (symbol OSSetThreadPriority = 0x8020472C) [read];
   0x8022C304(0) at 0x80152464 sets 18 back. Which thread 0x8061C658 is: not read.
2. Unless flags bit 0x40: callback r13-23264 and 0x80153B98(2) (a leave script through 0x801642D4) [read].
3. Callback r13-23332(&r13-23276) [read].
4. If the room differs from r13-23276 (the room in play: player 1's, or player 2's old room inside the bracket):
   0x801A0EAC (stream tracks dropped), 0x801A05EC(10,1,0) (all 80 tracks fade), 0x80042910 (stops the background job
   thread and waits for it: readers.md "posts to 0x805B2AFC, waits 0x805B2B5C"); else r13-26508 := 1 [read]. The host
   filters the sound to the mover's room (coop_sound_fade_rooms, R5) [read in design R5].
5. 0x8012EDC0(0): the moves streamer's step paused (both players' and every character's), old value kept; restored at
   0x80152440 [read].
6. Spin on GXReadDrawSync (0x8021AA34) until 0xBEEF [read 0x80152370-0x8015237C]. In this host the PE token is committed
   game-side when GXSetDrawSync is called (notify_GXSetDrawSync hle.c:4654 -> interrupt_commit_pe_token interrupt.c:314),
   not by the renderer: this spin does not wait for the render thread [read].
7. 0x80154C94(room, 0x8058B028) (below) [read].
8. OSReceiveMessage(0x8058B028, &msg, 1) at 0x80152398: THE BLOCK. Both players, every node, the camera, the scripts,
   the draw stop here; the guest OS runs the slot thread / reader thread (and interrupts) until the reply [read].
9. GXInvalidateTexAll (0x8021D244) at 0x8015239C [read].
10. 0x80151FDC(flags) (the arrival, below) [read].
11. The tail 0x801523A8-0x80152404: the OTHER slot's record (table +4) marked dead: +0x8140 := -1, +0x8142 := 0,
    +0x8143 := 0 [read]. The host puts back a record a player is in at 0x80154B54 lr 0x80152434 (notify_room_slots_tail
    hle.c:14298-14323) and lets the stayer's ticket hold go (0x801F6828(stayer, 0)) [read].
12. Unless bit 0x40: 0x801EB7A0(0x80603178+272, 0) (the live camera block's record reset) and 0x80153B2C(2,0) [read].
13. 0x80154B54 (the exits list) and 0x80154EC4(0) (wake the slot thread: OSSendMessage to 0x805B27BC) [read].

0x80154C94(wanted, reply) (the slot load) [read 0x80154C94-0x80154EC0]:
- r28 := table 0x8058AFC0 +4 (the other slot) at 0x80154CC8; 0x80151CD4(1); r13-23288 := old room, r13-23276 := wanted
  (0x80154CDC/0x80154CE0); the stamp block reads r13-26524 (0x80154D00-0x80154D5C: the player's object +592 := wanted via
  0x80118CB4); the level-wide strip 0x80043AB8 at 0x80154D60 (the host spares the stayer: coop_stay_kind_ask
  hle.c:14379-14389).
- **"Already in the other slot": compares the other record's ROOM ID ONLY** (0x80154D84 `lha -32448`, cmpw, at 0x80154D8C)
  -> 0x80154C50 (swap +0/+4) and on to 0x80154E1C. Not the loaded byte [read].
- Else if the current record already holds it: nothing. Else swap, and under OSDisableInterrupts the new current record:
  room := wanted, loaded := 0, +0x8143 := 0 (0x80154DC0-0x80154E18) [read].
- 0x80154E1C: current slot entry +20 := the reply queue; if the current record's loaded byte is 1, +20 := 0 and
  OSSendMessage(reply) at once (0x80154E4C-0x80154E7C): the wait in step 8 returns at once [read].
- 0x80152C5C; **table +8 (halfword, "the queued room") := -1** (0x80154E88-0x80154E94); 0x80151CD4(2); 0x80154EC4(0) [read].

0x80151FDC(flags) (the arrival) (readers.md's list, checked by `bl` list 0x80152014-0x801521BC) [read]: the current record
and the player node (0x801F62B0); 0x801E1CFC/0x801E1B74(record +0x813C); 0x80117260; 0x80138D84; sound 0x8019F3F0 /
0x8019E068; 0x800BA530; **0x80043A5C: every node's 0x80043820(node, record)** (the host spares the stayer:
coop_stay_strip_walk hle.c:14365-14375); event 61 to the current id and **0x801F5620(61, 0, 0, r13-23288): every node of
the room LEFT** (0x801520DC-0x801520F4); 0x80044428, 0x80157604, 0x80044748, 0x801EB550; the prompt slots reset 0x801172B0
(the slots in memory: player 1's, or player 2's inside the bracket which saves player 1's) and the script waits 0x80163F04
(R4b's put-back); lights 0x80153998 (flag 8); **busy := 0 for all bits** 0x801556CC(-1,0) at 0x80152184 (the game's word =
player 1's); re-hold 0x801F6828(r13-21492, 1); 0x80174B84; the 10-tick fade-in 0x801E7C88 at 0x801521A8; 0x801DC1C8.

## 3. What the stayer loses, frame by frame

Which player is the stayer makes no difference to the stall: the block is the game's one main thread, so player 1's
world (their node step, their controller in the behaviour pass) and player 2's (their turn inside player 1's controller,
their camera, look, scripts) stop together [read: one thread; hle.c:20060 is inside the behaviour pass].

- Countdown frames (ticket 14 -> 1): nothing skipped. The stayer is not held (coop_node_held_ask; room_turn for player 1);
  their half is lit (fade quad marked out of it) [read].
- The change frame: in game steps, NOTHING of theirs is skipped: their behaviour step, controller/turn, camera, look,
  scripts and draw all run once, after the block, in the same frame [read: main loop order 0x800083EC -> 0x800083FC ->
  0x800084AC -> draw 0x8000851C-0x80008538 -> 0x80008560]. In wall-clock time they lose 3-6 retraces (the frame is
  4-7 retraces long); the game does not catch up (the next rows move them by the usual per-frame amount: e.g. R5 s2
  dp1 0.2-0.3 a frame before and after 3833) [read both.tsv; inferred: a fixed-step game].
- Their picture: held, not redrawn, for the stall [read: no present path while the game thread waits].
- Their pad: not read during the stall (one PADRead per frame, hle.c:4180-4240); a press and release shorter than the stall
  may be missed [inferred: no latch found in runtime/host/controls.cpp by a grep for one; not read in full].
- Game state touched in the change frame that a stayer might also feel (not the picture freeze, but it can look like a
  pause of the fight): event 61 to every node of the room LEFT (0x801520F4). When the players were together and one leaves,
  that room is the stayer's: the stayer's node gets 61 ("a motion in states 80/81, a cast and a finisher are cut",
  doors-readers.md) and its enemies in state 0 drop their route and unregister (0x800BB0C0, 0x8003A2E8 via 0x8003B850-84;
  state 33 may go inert via 57, 0x8003CB54-7C: enemy-readers-2.md) [read there]. Still open in docs/coop-level3-design.md
  ("event 61's route drop (0x800BB0C0) when player 1 leaves the room player 2 stays in"). Also the busy clear
  0x801556CC(-1,0) wipes every bit of the game's busy word (player 1's) even on player 2's change (R4b handles the scene
  case) [read].
- The moves streamer: paused only inside 0x80152298 [read]; but a request stamped with the old r13-23276 (0x80143304-08)
  and served after the room word changed is refused (0x80148460-0x801484B8) (refute.md, stay-readers.md) [read there]: a
  stayer's move requested in the countdown might wait or never come [inferred; '[stream]' probe never logged].

## 4. Where the change frame's time goes (what the frame stamps in the logs show)

The page logs stamp each host line with the timebase frame. In logs/run-20261111-182711-rooms-r5-s2-p1door (player 1
183 -> 180, player 2 stays) [read page-0001.log 554-607]:
- 3826 the change, the table, "unpack asked: room 180 into slot 0 (lr 0x80154B10)", the reader request 0x400010B4 (ARAM ->
  slot, room 180 already in ARAM: no 0x8000 disc request);
- 3827 "the arrival's walk passes over player 2's node" (0x80151FDC running: the unpack is done);
- 3828 the tail, the hold let go, script 1265 from the script timers (lr 0x80163E5C, main loop 0x8000849C);
- 3829 the zone scripts (lr 0x80134B38), sound gains, "[stay] ... the room in play is 180", "BOTH rooms drawn" (room_slots_frame
  at the section-3 room draw), "[room-lights] grid 0x807931B4: 9 lamps";
- 3833 the next frame's first pad read (both.tsv's next row).
So: unpack 1-2 retraces, arrival + the frame's logic 1-2, **draw to next frame 4**. Solo 183 -> 180
(run-20261112-030139-jtg-solo-returns 596-640): unpack 3624 -> 3625, scripts 3626, next row 3628: 1 + 1 + 2. Room 181 apart
(run-20261112-033738-p2-follows-181): change 4764, walk 4766, draw 4767, next row 4770: 2 + 1 + 3 [read].

The draw-side part is the biggest apart and grows with two views. One synchronous host wait sits in it: **room_lights_update
(hle.c:6099-6187, called every frame from notify_look_at hle.c:6229-6232) sends a changed light grid with
dol_aurora_set_room_lights, which on the game thread is `gxq::call_sync`** (aurora_backend.cpp 3208-3218): the game thread
waits until the render thread has replayed EVERYTHING queued before it (call_sync, 1587-1594) [read]. At a change the grid
always changes (the "[room-lights]" line is printed in the change frame in every run above) [read], so at that moment the game
waits for the render thread to finish the new room's first draws queued so far: first-time texture decode/upload and any
new pipelines [inferred: those costs were not measured]. The renderer's present itself waits only for the previous frame
(present 1620-1631) [read]. Not measured in any log: no run had DOL_GX_TIME=1 (`[gx-time]` lines, aurora_backend.cpp
451-452, 3572-3626) [read: grep found none].

## 5. What is per room, per slot, per player, and what is one

- One: the main thread and its block; the ticket 0x8058AFFC; the reply queue 0x8058B028; the slot thread 0x801546A4 (queue
  0x805B27BC); the reader thread 0x801531CC (queue 0x8058AFDC); the ARAM cache of 2 entries (0x8058AF60); the fade state
  (r13-21784, 0x80602A2C); the renderer's presents; the busy word r13-23344 in memory; the moves streamer [read: sections
  above; readers.md].
- Per slot: the record (+0x8140 room, +0x8142 loaded, +0x8143, +0x8144 pending, +20 of the slot-table entry the reply
  queue); the slot table +0/+4 (+8 the queued room) [read].
- Per room: nodes by object +592; event 61/62 broadcasts; the light grid in the slot; section 3 drawn only for the other
  player's room (room_section_draw hle.c:13714-13726: a section 3 whose room nobody is in draws nothing) [read].
- Per player (host): the who words, camera set, prompt slots, busy/state copies swapped per turn; the fade's half
  (g_fade_mover); the stayer (coop_stay_stayer hle.c:13845) [read].
- **The slot data is position-dependent**: in saves/rooms-ch1-183.state (room 183 resident in slot 1 at 0x83000000) 18,073
  of 729,096 words of the slot point into the slot itself, and the slot record (0x806EC440, in the game's heap) holds 21 words
  pointing into it and 2 into area 1 (0x83300000) [read: offline count with tools/ed/savestate.py]. So a room unpacked
  somewhere else and copied into a slot would carry pointers to the wrong place unless the game's own relocation runs for
  the final address. Where those pointers are written (inside the unpack 0x80154118, which takes the slot base, or in
  0x80151FDC) is not read.

## 6. The game's own mechanisms that keep a change from blocking

**The engine was built to preload the NEXT room into the other slot in the background, and to make the door a swap.** It
is dormant only because N = 1 and nothing writes table +8. [read]:
- The slot thread 0x801546A4 picks work by cases (0x80154760-0x80154988, jump table at 0x80253150): with the queued room at
  table +8 (`lha 8(r30)` at 0x80154808/0x80154840) not -1, the other slot (r26 = table +4) valid and its record NOT loaded
  (0x80154834-0x8015483C), case 6 (the queued room in ARAM) or 7 (not yet). Case 6's code: OSSetThreadPriority(0x805B7540,
  21) at 0x80154A34-0x80154A40, then **0x80151DC4(queued room, other slot)** at 0x80154A4C-0x80154A54. Case 7:
  OSSetThreadPriority(…, 11) and 0x80151D24(queued room) (disc -> ARAM first) at 0x80154A5C-0x80154A70. The fresh load at a
  change is 0x80151DC4(current record's room, current slot) at 0x80154AE8-0x80154B0C (the logs' "lr 0x80154B10").
- 0x80151DC4(room, slot) (0x80151DC4-0x80151EC0): under OSDisableInterrupts, unless the record is already pending (+0x8144),
  finds the room's ARAM entry (0x80151CE0); if in ARAM: record room := room, loaded := 0, +0x8143 := 0, +0x8144 := 1, and
  posts (room | 0x40000000 | entry << 12 | slot bits) to the reader queue 0x8058AFDC. So the record carries the room id from
  the moment the unpack is posted.
- 0x80154C94 then takes the "already in the other slot" path for a room id match alone, swaps, and either replies at once
  (loaded) or leaves +20 set so the reader's completion replies (in flight). And it clears +8 to -1 itself (0x80154E94).
- The reunion rows above (2 retraces, 33 ms) are this path running today: a return into the room the other player kept
  (room_slots_return's log "kept, a player stayed in it", hle.c:14340-14345) [read].
- The thread priorities: the slot thread sets its own to 21 for the background job and 11 for others; during a change
  0x8022C304 raises thread 0x8061C658 to 8 [read]. The main thread's priority was not read; [inferred] 21 is below it, so the
  background unpack runs while the main thread waits for the retrace at the frame's end (0x801DFA4C / VIWaitForRetrace:
  hle.c:14391-14394).

What else exists: the ARAM prefetch of the new room's exits after every change (0x80154B54 + 0x80152D84, step 13) keeps the
next file in ARAM [read readers.md]; with the players apart it serves only the mover's room's exits, so the stayer's next
door may miss ARAM (a host file read inside the block: "cheap" per readers.md) [inferred].

## 7. Candidate approaches (each opened only with mod_coop, ED_COOP_STAY, two slots, and the change about to leave the other player behind)

**A1. The engine's own next-room preload, for a change that splits players who were together** (the case in both of the
user's play recordings: 341 -> 342 with player 2 staying in 341). At the ticket (notify_room_ticket, hle.c:11297), when the
other player's room is the room being left (they will stay) and the other slot's record is dead (room -1, loaded 0: no
player in it), write table +8 (halfword 0x8058AFC8) := the ticket's room (+8 of 0x8058AFFC) and wake the slot thread
(0x80154EC4(0), the game's own call). The slot thread posts the unpack into the free slot at priority 21 during the
countdown (fade+2 frames); at the firing 0x80154C94 matches the room id, swaps, replies at once or waits only for the rest;
0x80154C94 resets +8. Expected: the reunion's 2 retraces instead of 4-7 [inferred from the reunion rows].
- Touches: notify_room_ticket (one write and one guest call); nothing in the change itself. The host's later steps already
  handle the swap-only path (room_slots_return, notify_room_slots_tail's put-back of the stayer's record, notify_ticket_run /
  coop_stay_change_end for player 2's change, R4c) [read].
- Both players: for player 1's door the stayer is player 2 (their room = r13-23276 at the ticket, both together); for
  player 2's door the ticket is written in their use with their who words in, the room left is player 1's too. The rule is
  "the other player's object +592 == the room being left", read from both nodes, never from r13-26524 alone.
- Must never: preload into a slot whose record is loaded or whose room a player is in (the slot thread itself refuses a
  loaded other record: 0x8015483C; the host must not put back a dead record before it); leave +8 set if the ticket is not
  served (cutscene, level change, a ticket the host refuses: write -1 back); run for a level change or a scene that replaces
  the actor list; run while apart (both slots taken: the slot thread does nothing, harmless but useless).
- To read first: the reader's 0x4000 completion (0x8015346C branch: the post to slot +20 when +20 is still 0, i.e. an unpack
  finished before the firing; readers.md says "a post to the slot's reply queue"); the full case table at 0x80253150; that
  case 6 is not refused by 0x80157398 / 0x801573C4 (the checks at 0x801547DC / 0x801547F4 / 0x80154910); 0x80155450's eight
  callers now answering "resident" for the preloaded room; the light grid pointer 0x804F0784 near the door with the next
  room resident (room_lights_update's "current slot's span" rule, hle.c:6124-6131, should hold).

**A2. Take the host's synchronous wait out of the change frame's draw** (all three cases, the largest part apart). Make
dol_aurora_set_room_lights an ordered asynchronous stream op (copy lamps and cells into the op, as the stream's other ops
carry data) instead of call_sync, so the game thread is not held while the render thread decodes the new room. Touches:
aurora_backend.cpp 3208-3218 (GXRuntime, patch regenerated). The comment there requires stream order with the room's draws:
an appended op keeps it. Gate it with a host flag set only for co-op apart if the user wants solo byte-identical in timing.
MEASURE FIRST: DOL_GX_TIME=1 plus a timestamp around the call in room_lights_update; if the wait is small, the post-draw
retraces are the render thread's first-frame work and A2 moves the stall to the next present instead of removing it.

**A3. A change between two rooms with both slots taken (apart -> apart: the mover leaves a room only they are in).** No free
slot: the destination is the mover's own old slot, still played (their door motion, their room's enemies and collision, the
draw, the light grid, the sound zones 0x80155234(2)) until the firing. A host staging buffer copied in at the firing is ruled
out by the position-dependent slot data (section 5) unless the game's relocation is found and can be pointed at the final
base. Holding the mover under black for extra frames while unpacking over their old slot would need every per-frame reader
of that slot silenced (their room's nodes' steps by +592 through 0x80123F70, section 2's draw, 0x804F0784, the sound zones,
their turn's coop_stay_room_in swap, 0x80155450, the Tome's 0x801538A4): a reader set of its own. Not proposed for a first
build.

**A4. Shorter unpack** (a native BPE expander for the reader's 0x80154118, byte-identical): 1-2 retraces at most; only after
A1/A2 and a measurement.

**Rejected:** running the stayer's world frames during the block (a guest call or another guest thread re-entering world
code while the main thread is inside 0x80152298 with r13-23276 already the new room and the table swapped: the doors-readers'
deadlock note on hle_guest_call in a blocked change, and every reader sees a half-made room); the Tome copy's second world
(48 MB copies, two worlds that never see each other); re-presenting or blending the last picture (still frozen).

## 8. What must never run for the stayer during the other's change

- Nothing of theirs may run INSIDE the block (between 0x80154C94 and the return of 0x80151FDC): r13-23276 and the table
  already name the new room (0x80154CE0, the swap), the new record is pending, event 61 and the strips are mid-way, the
  prompt slots and busy word are being reset. Their turn, look, camera step, coop_stay_room_in swaps, guest calls into
  world code, a Tome copy turn and a save state must all wait for the frame's behaviour pass (where coop_stay_change_end
  already closes the R4 bracket) [read; inferred for the save state].
- Their own slot must never be the one unpacked into (room_slots_return's index swap, hle.c:14347-14356, does this today for
  player 2 apart; the tail's put-back keeps their record) [read].

## 9. Open questions

1. How the draw-side retraces split between the call_sync in dol_aurora_set_room_lights, the render thread's first frame
   of the new room (textures, pipelines, the second room's draws in three outputs) and guest CPU work. Needs one run with
   DOL_GX_TIME=1 (and [fps]) through a door apart and the same door solo.
2. The reader's 0x4000 completion when slot +20 is 0 (0x8015346C): does it skip the post, or post to address 0? Decides
   whether A1's preload may finish before the firing.
3. The slot thread's case table at 0x80253150 and the 0x80157398 / 0x801573C4 checks: confirm case 6 runs when the host
   writes +8 and nothing else of the dormant N=2 path wakes with it.
4. Thread 0x8061C658 (raised to 8 during a change) and the main thread's priority: whether a priority-21 unpack gets enough
   time in the countdown frames at 54-60 fps apart.
5. Whether event 61 to the room left (the stayer's room when they were together) makes the stayer's fight visibly pause
   (enemy route dropped, a finisher or cast cut). It runs inside the change frame whatever A1 does.
6. Whether a stayer's move request stamped with the old room is refused after the change (0x80148464).
7. Whether a short press is lost during the stall (controls.cpp, PADRead once a frame).
8. Where the slot data's absolute pointers are written (0x80154118 or 0x80151FDC): only matters for A3.
