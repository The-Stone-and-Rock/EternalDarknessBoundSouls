# Refuter: the change-freeze design against the overall goal and both players

Item: the other player's brief freeze when one changes room (design-change-freeze.md, with change-freeze-code-readers.md
and change-freeze-measure-readers.md). Lens: the user's goal (a game that seems built for two, engine-level, co-op only,
both players, both directions) and whether the scripted checks would prove the result. Read only: disassembly
(`build/ed-db/disasm.txt`, `ed dis`), `runtime/host/hle.c`, `runtime/host/quickstate.c`, existing logs. Nothing was built
or run. [read] = seen at the address, line or log row given. [inferred] = reasoned.

Verdict: **needs changes.** F1 uses the game's own dormant preload, which fits the goal. But it goes after the smallest part
of the stop the user sees. Its "already loaded" test reads a stale host table, so it misses the most common split in real
play, and the scripted checks cannot catch that. G2 and the firing guard have holes on the reader's set-up window. The
apart -> apart case and the stayer's input waits stay exactly as they are.

## 1. BLOCKER: F1 removes at most about 1 of the 4 to 7 retraces

- The user's own run, logs/run-20261111-221141-rooms-play [read: page-0001.log 2630-2683, both.tsv]:
  - 1843: the change, the fresh swap and "unpack asked: room 342 into slot 0";
  - 1844: the arrival's walk, the tail, `[detail] frame 1844` and "BOTH rooms drawn";
  - 1847: the next pad row.
  The unpack is at most 1 of the 4 retraces. 3 come after the first draw.
- The design's own reference run, run-20261111-182711-rooms-r5-s2-p1door [read: page-0001.log 554-605, both.tsv
  3826 -> 3833]: the unpack is asked at 3826 and the arrival walks at 3827, so the unpack is 1 of 7 retraces. Design §1.3
  says this itself: "unpack 1 retrace ... to the next pad read 4".
- The "2-3 retraces" expected (§0, §4) is taken from rejoins. A rejoin goes into the room the stayer's half has been
  drawing every frame, so its textures and pipelines are warm. A split's new room was never drawn [inferred]. Solo
  183 -> 180 is unpack 1, arrival 1, draw 2 (code reader §4), so after F1 expect about 3-6, not 2-3.
- **Fix:** ship F3 alone first. It is logging only and changes nothing for either player. Give the [change-time] stamps and
  `[gx-time]` to the next design. Keep F1 as the enabler of the warm draw in §5: a room has to be resident before it can be
  drawn ahead, marked out of both views. Build F1 only with that warm draw, or once F3 shows the unpack is worth a
  three-hook cross-thread change. The user's instruction is "using all resources to avoid breaking things", and G2 is the
  riskiest part of this build for about 17 ms.

## 2. MAJOR: F1's "not resident" test reads a stale host table, and the scripted check cannot see it

- The gate uses `room_slot_of(cpu, room) < 0` (§3). room_slot_of reads the host array g_room_slot_room
  (hle.c:13824-13830) [read].
- That array has three writers [read]:
  - notify_room_unpack_post sets -1 (13608);
  - room_slots_frame sets a room only while the record is loaded (13624-13636);
  - the tail sets the current slot (14304).
  Nothing clears it when the tail kills the other record (0x801523A8-0x80152404: +0x8140 := -1, loaded 0). So a dead
  slot keeps its old room in the host table.
- Backtracking is the most common shape. The players come together into 183 from 180, by a rejoin or a follow. Then one
  goes back to 180. room_slot_of(180) is the dead slot, so F1 logs "the room is resident" and does not arm. The change is
  then the fresh load it is today: room_slots_return prints "nobody stayed in it: loaded fresh" (14337-14342) [read].
- **The scripted checks can't catch this.** g_room_slot_room is not in the quick state. HleQuickState holds callbacks,
  irq, dvd cwd and cards only (hle.c:28370-28398) [read]. K1-K7 load saves/rooms-ch1-183.state, which was made right
  after 180 -> 183 (door 29), in a new process whose table starts at {-1, -1}. So K1 arms for 180, while the same door in
  continuous play does not.
- **Fix:** decide residency from the guest record, as the game does at 0x80154D84 and r6_room_loaded does
  (hle.c:24216-24223): the other record's room == C and loaded != 0. Then add one check in one process, with no state
  load between the steps: together 180 -> 183, then one player back to 180. Expect the arm line.

## 3. MAJOR: apart -> apart is unchanged, and it is the user's literal example and most doors once apart

- The user: "player 1 is fighting an enemy in one room, player 2 enters another room". That reads as A2 just as well as
  S2. F1 only arms for a split (§3, r1 == r2).
- Once the players are apart, every door except a rejoin is A1/A2: 5-7 retraces, untouched (§2 table). The user's two
  play runs only had splits by player 1 (measure readers), so their next play will mostly be apart -> apart.
- The design says "unchanged in this build", but nothing in §11 tells the user what to expect in play.
- K5 has no fight, and no check covers the user's example as written.
- **Fix:** say in the plan, in plain words, that this build leaves the stop for a door taken while already apart. Add K5b:
  player 1 among the zombies in their room while player 2, apart, changes room. Run it with the [change-hold] roster, to
  confirm 102 never reaches player 1's room (0x801F5654-0x801F5674) and to measure the stop there.

## 4. MAJOR: the stayer's roughly 1 s action waits during the other's ticket, in both directions, are left as they are and relied on

- During the other's ticket the stayer cannot do these [read]:
  - a quick spell (coop_quick_cast_ask, hle.c:19480-19497);
  - player 1's zone look during player 2's change: "their zones aren't looked at till they're brought along"
    (19889-19896);
  - player 2's Start (25895, 26113-26119: "Their B and quick spells wait the same way. A press then is not kept").
- The ticket lasts about 62 ticks (about 1 s). These waits were written for follow mode: "Their B, zone look and Start wait
  for coop_follow", hle.c:19485. With ED_COOP_STAY nobody is brought along.
- The measure readers found an idle stayer who could not start walking (run-20261112-033642: stick at 4750, moving only at
  4772). The design leaves this as U5.
- A fighting stayer who casts, presses B or opens the Tome loses about a second. That is the other reading of "a brief
  second".
- The design also leans on these waits for its safety:
  - G2's "rare" rests on them (§3 G2);
  - the Tome argument (§7: "The non-asker's Start waits during a ticket ... so no Tome opens while armed");
  - K3/K4 press only A, which no wait refuses, so the checks can't see them.
- The user's direction is both players acting at once. These waits are the next thing to lift, and then F1's safety
  reasoning would no longer hold.
- **Fix:**
  - List the waits as part of this item, or as a sibling item to do next.
  - In K3/K4, have the stayer press a quick spell, B and Start in the middle of the countdown. Log the `[coop] player N's
    quick spell waits` / `Start in player 1's room change` lines and the stayer's p?m/anim column.
  - Make G2 and the Tome refusal hold on their own: refuse the arm while any Tome is open, and keep G2's hooks. Don't rely
    on the press waits.

## 5. MAJOR: G2 misses the reader's set-up window, and the guard only covers it when +4 == 0

- The reader's completion runs in this order [read]:
  1. loaded := 1, +0x8143 := 0, +0x8144 := 0 with interrupts off (0x801534C0-0x801534EC);
  2. interrupts back on, then the set-up 0x80151EE0 (0x801534F4);
  3. only after that, the +20 test and post (0x80153504-0x8015351C).
- The reader runs at 21, below the main thread (§1.2), so a retrace can wake main in the middle of the set-up [inferred].
- **G2 hole:** a direct change to D in that frame takes the fresh path, writing room := D, loaded 0, +0x8143 0
  (0x80154DE4-0x80154E14). Its +20 is set at 0x80154E30. +0x8144 is already 0, so 0x80151DC4(D, Y) is accepted and D is
  queued behind.
  - G2's detection (a) needs "+0x8144 == 1", so it does not fire.
  - The reader finishes C's set-up and finds +20 set. It replies to main, and main arrives into C's data under D's record.
  - The queued D unpack then overwrites the slot under the live room [inferred from the read order].
- **The guard on the ticket's own firing** holds only when "+0 == 1 and +4 == 0" (§3 F1.3). 0x80154C94's reply-at-once
  tests the loaded byte alone (0x80154E48-0x80154E4C) [read]. If any armed ticket ever reaches +0 == 1 with +4 != 0, it
  fires into a slot that is not set up. Today's armed kinds seem to have +4 < +0 (transitions.md ticket table) [read
  there], but the guard should not depend on that.
- **Fix:**
  - G2 (a): also treat "loaded == 1 and +0x8143 == 0" as in flight, and keep (b)/(c) as designed.
  - The guard: hold whenever this tick would fire (+0 == 1) and the armed record is not complete, whatever +4 is.

## 6. MAJOR: the guard's hold can leave the mover black for seconds, or for good

- The guard holds the firing while C is pending, up to a 300-frame cap. It never fires while "loaded == 1 and
  +0x8143 == 0", and that rule has no cap at all (§3 F1.3).
- The reader at 21 only runs when main idles. When the host runs behind wall time, the retraces are already due and main
  may find no idle time [inferred: interrupt.c:430-519 pops retraces from wall time]. That is the r6 runs at about 20 fps,
  listed by the measure readers. So the mover's half could stay black for 300 frames (5 s at 60 fps, 15 s at 20), or
  forever in the set-up window.
- The design gives no reason to hold for pending at all. Its own text says pending is safe because the change waits on +20
  and the reader replies after its set-up.
- **Fix:**
  - Don't hold for pending. Fire, and let the game's own +20 wait cover it: the stayer then sees what is left of the unpack
    and no more.
  - Hold only in the set-up window. If it lasts more than a few frames, raise the reader with the game's own call, as
    case 7 does: OSSetThreadPriority(0x805B7540, 11), 0x80154A5C-0x80154A64 [read the design's §1.2].
  - Keep any cap to a handful of frames, and log it as a WARNING.

## 7. MAJOR: the "fighting" part is not settled for the stayer's creatures, and a follow-mode rule remains on the 102 path

- The design reads only the zombie's 102 handler: 0x80124788, 0x801F64A0(node, 1), 0x801F6488(node, 1) [read
  0x8003C5B0-0x8003C5DC]. It does not read who sets the creatures' node +28 bit 8 at the ticket.
- The hold test is "busy AND that bit" (0x801F68D4: +28 & 8; hle.c:11273-11277). If the split's 102 sets the bit on the
  stayer's room's creatures, they stand still for the whole countdown, whatever F2 retargets. That "stops the fight for
  about a second", which is the measure readers' main open question.
- [change-hold] logs state, target and health only.
- The host's 102 filter (hle.c:22021-22035) lets the room-change 102 through on purpose: the other player "is let go as the
  game lets go, and brought along (coop_follow)". Under ED_COOP_STAY nobody is brought along. A stayer grabbed in a split
  is freed by the other's door. That one-player/follow assumption sits on the path this design logs and touches.
- **Fix:**
  - Before building F2, read the setter of node+28 bit 8 at the ticket: the 89 callers of 0x801F6828, and those of
    0x801F64A0 and 0x801F6488.
  - Add node+28 & 8, node+24, node+32 and the position step to the [change-hold] rows, so K3/K4 show whether creatures move.
  - List the grab release as a case to decide, for both directions.

## 8. MAJOR: the scripted checks would not prove the result

- **No A/B in one build.** Every comparison is with old runs from other builds and other logging: 182711 is R5, 221141 is
  the user's. U6 itself says logging adds 1-3 retraces apart. Add `ED_CHANGE_AHEAD=0`, the same build with F1 and F2 off.
  Run K1 and K2 both ways, quiet: only ED_BOTH, ED_CHANGE_TIME, DOL_GX_TIME, ED_ROOM_SLOTS and ED_COOP_STAY set. Compare
  ms, not retraces: the timebase drops time over 100 ms, interrupt.c:70, 446-447 [read via measure readers].
- **The picture is not measured.** both.tsv rows come from the game thread (one per PADRead, hle.c:17584-17608). Shots at
  ED_RUN_SHOTS_MS=250 cannot show a 33-116 ms stop. The freeze is in presents, so the pass rule must read `[gx-time]` per
  frame: present gaps around the change for the stayer's half. both.tsv alone could improve while the picture still holds,
  which is exactly the case A2 warns about.
- **K8's solo baseline won't have the stamps.** The two-slot hooks install only under `room_slots() == 2`
  (hle.c:27580-27611) [read]. With co-op off, rooms_follow_coop makes the slots 1, so room_slots_count (t1),
  notify_room_slots_tail (t3) and change_ahead_tick (t0) are absent. The solo runs the design compares against
  (run-20261112-030139/030651) were two-slot solo, from before that fix. Either give the stamps their own installs under
  ED_CHANGE_TIME, which then touches solo's hook table, so say so, or drop the claimed solo baseline.
- **The user's example has player 1 fighting**, and K4 cannot ready player 1 ("R is digital only"). Player 1's real fight
  as the stayer is never exercised, so the "both players" claim rests on K3 alone. Find a way to script player 1's analog
  R, or ask the user for that one play.
- **K6b** overwrites the ticket with a kind-17 change, so the original ticket never fires after the stacked change. The
  real G2 case is a script's direct 0x80152298 with the ticket still counting. After it, the ticket fires from the new
  room. Check that sequence too, or say it is not covered.

## 9. MINOR: F1 and F2 open while the players are together

- The rule given is that every new path opens "only with co-op on and the players apart". F1 arms only when r1 == r2, and
  F2 holds the asker whenever coop_room_change_on, which includes together, rejoin, A1 and A2.
- Under ED_COOP_STAY a door always splits, so this is the moment of parting. Still, the design should say so and justify
  it, and K7b should show no change to a together fight with no door.

## 10. MINOR: F1 can arm during a scene without a letterbox

- cutscene_playing needs a movie or the letterbox (hle.c:3784-3790) [read]. F1's gate only asks for busy & 4. So it arms
  during a player's busy-32 scene, the R4b case, whose scripts may change room directly. That makes G2 more likely than
  "rare".
- **Fix:** use coop_room_change_on's busy test (hle.c:11284-11286: nothing but 4 | 2 | 1). Also refuse while
  coop_p1_scene_on or coop_p2_scene_on.

## 11. MINOR: the set-up has never run for a slot that isn't current

- In today's fresh path, 0x80154C94 swaps first, so 0x80151EE0 always runs on the current slot. F1 runs it on the other
  slot for the first time.
- First read: 0x80151EE0 writes the record (+0x813C at 0x80151F2C, +0x8143 at 0x80151F7C). 0x801DEF34 copies 288 bytes
  into the record (0x801DEF6C-0x801DEF7C), checks version 23, then calls 0x801DDE9C(data, record+4) [read].
- 0x801DDE9C and what lies below it are unread. Before building, confirm that nothing there writes an r13 word or a
  "current room" global, which A's play would read during the countdown.

## 12. MINOR: a misleading log

- room_slots_return will print "a return into room C (slot Y): kept, a player stayed in it" for a room unpacked ahead
  (hle.c:14337-14342, the loaded byte != 0). `ed timeline` readers will take it for a rejoin.
- **Fix:** add an "unpacked ahead" wording when table +8 named the room.

## What holds

- One world frame runs long. Frames are not skipped [read 0x800083EC, 0x80155830-0x80155924].
- The tick does nothing else while +0 stays above 0 [read 0x80155844-0x80155890 -> 0x80155918].
- The slot thread's case 6/7 conditions are as described: +8, the other index, and the other record's loaded byte
  [read 0x80154808-0x80154878]. A higher case than the job in flight waits [read 0x80154944-0x8015495C].
- 0x80154C94 compares the room id only, replies at once on the loaded byte, and clears +8 [read 0x80154D84-0x80154E94].
- Character lighting always takes the current slot's grid (lights-game-readers.md 0.2), so a resident C does not change
  A's lamps.
- Using the game's own dormant preload and its own wake (0x80154EC4) is the right kind of fix for the goal. Section 1 is
  about where it pays, not whether it is the game's own.
