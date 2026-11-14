# Refuter: the change-freeze design against the current implementation

The item: design-change-freeze.md (F1 the game's own unpack ahead at a split ticket, its firing guard and the stacked-change
guard G2; F2 the asker counted as held; F3 the timing stamps). The lens is conflict with the host code as it stands:
double-runs, hooks that fight, the passthrough pattern, state left set across a save, a load or a room change, and repeats of
failed attempts. Read only: runtime/host/hle.c, main.c, quickstate.c, frontend.cpp, the disassembly (build/ed-db/disasm.txt,
`ed dis`) and docs/coop-failed-attempts.md. Nothing was built or run. [read] means seen at the address or line given.
[inferred] means reasoned and not seen.

## What holds (checked, no conflict)

- **0x80155830 is free.** No intercept or notify is on it anywhere in runtime/ [read: grep]. Its only callers are
  0x800083EC and 0x80050A90, so the return addresses are 0x800083F0 and 0x80050A94 [read].
- **The design's copy of the tick is exact for the frames it arms on.** With +0 >= 3 and +4 != 1 the tick only stores
  +0-1, and +4-1 when +4 is set. The fade (0x8015587C) and the firing (0x80155894 on) are not reached, and the +36 callback
  runs only after a firing (0x801558F8-0x80155914) [read 0x80155844-0x80155924].
- **The tail-call pattern exists.** The ARQ callback at hle.c:1050-1057 sets pc and g_hle_tail_call, and the dispatcher
  skips hle_return (hle.c:5551-5557). The game's 0x80154EC4 runs natively on the main thread, so the slot thread switch
  happens in guest code, not inside an hle_guest_call [read].
- **Guest threads really are scheduled.** VIWaitForRetrace is a notify only (hle.c:3718, 5198), and the retrace interrupt
  goes through __OSDispatchInterrupt -> OSLoadContext (interrupt.c:375-408). So a reader at a lower priority than main can run
  while main sleeps at the frame's end [read]. Each slot-thread case sets the reader's own priority: case 6 gives 21
  (0x80154A34-0x80154A40), case 10 gives 11 (0x80154AD8-0x80154AE4) [read]. Nothing is left raised for the next fresh load.
- **The resident path at the firing.** 0x80154C94 compares only the other record's room id (0x80154D84-0x80154D8C). It
  swaps, stores +20 (0x80154E30), replies at once if loaded == 1 (0x80154E48-0x80154E7C), and otherwise leaves +20 for the
  reader. It clears +8 (0x80154E94) [read]. The reader's completion posts only when +20 != 0 (0x80153504-0x80153530) [read].
- **The R4 bracket and the tail are unchanged by F1.** notify_ticket_run, coop_stay_change_end (hle.c:13858-13892) and
  notify_room_slots_tail (14298-14323) give the same result after a swap as after today's fresh load. For S2,
  coop_stay_change_end's swap-back uses room_slot_of(g_stay_bracket_room) (13878-13883), the same as today [read].
- **Section 3 is not drawn while the players are together.** room_section_draw returns 0 for any section 3 unless
  `apart && live && room == room1 && room != room0` (hle.c:13711-13726), so the ahead room is never drawn [read].
- **g_fade_mover survives the held frames.** It is forgotten only when `!coop_room_change_on && fade ticks <= 0`
  (hle.c:3342), and a held countdown of 1 keeps coop_room_change_on true (11285) [read].
- **A quick save mid-unpack is a whole machine.** quickstate saves MEM1, ARAM (quickstate.c:385-386), the CPU and GX. A
  reader thread caught mid-unpack resumes from its context in MEM1 [read].

## Problems found

### P1 (major): G2's host flag does not survive a quick save taken inside the stacked change. Loading such a state hangs the game.

The design's §8 says: "g_ahead_stack spans only one blocked change, inside one frame, where no quick save is taken". That is
false here. quickstate_service runs at the top of every dispatch iteration, whichever guest thread is current (main.c:367-371).
It is gated only by an idle GX FIFO (quickstate.c:815-821) and by hle_save_held (hle.c:28008-28017). hle_save_held has no
room-change term. During the block, main is asleep in OSReceiveMessage (0x80152398) after the GXReadDrawSync spin, so the FIFO
is idle and a save goes ahead while the reader and slot threads run [read].

What a state saved inside the G2 window holds:
- Step (b) has already written +20 := 0 for the current entry, in MEM1.
- The record has been renamed D (0x80154DE4) and is still pending C.
- g_ahead_stack is a host static. It is not in the COOP chunk, and hle_coop_state_load (hle.c:28020-28100) does not touch it.

After a load in a new process, (c) is not armed:
1. The reader finishes C and writes loaded := 1 (0x801534D0).
2. It posts nothing, because +20 is 0.
3. The slot thread sees the current record loaded, so no case 10 runs.
4. Main waits on 0x8058B028 forever, with the stayer's half frozen on its last picture.

The result is a freeze [inferred from the reads above].

After a load in the same process, a stale g_ahead_stack applies (c) to whatever unpack of slot Y finishes next.

**Fix (the pattern design-r4d.md already uses for g_r4d):**
- Add `|| g_ahead_stack.on` to hle_save_held.
- Add `memset(&g_ahead_stack, 0, ...)` to hle_coop_state_load.
- In (c), act only when the record's room is still D, the room (a) saw.

(Context, not this design's fault: g_stay_bracket and g_stay_bracket_was are not reset on a load either.)

### P2 (major): the firing guard can hold the countdown at 1 for ever. That soft-locks both players.

§3 step 3: "Never fire while loaded == 1 and +0x8143 == 0: keep holding and log each 60 frames". There is no exit. While the
ticket reads 1, coop_room_change_on stays true (hle.c:11281-11287). So, with no end:
- The stayer's quick spells are refused (hle.c:19486-19496).
- Player 2's Start opens no Tome (25895, 26118).
- Their B and zone look wait.
- Every later fn66 is passed over (16505).
- The mover's half stays black (g_fade_mover kept, 3342).

Only quitting ends it.

**Fix:** after the cap, give the record the game's own dead mark (room -1, loaded 0, +0x8143 0, +0x8144 0, as the tail's
0x801523A8-0x80152404) and let the tick fire. 0x80154C94 then takes the game's fresh path (0x80154DBC on), and the change
blocks the way it does today. Do the same for the 300-frame cap.

Also, the guard is "stateless": it holds for ANY incomplete other record whose room equals the ticket's room. It cannot tell an
ahead unpack the host armed from a record G2 left, or from the game's own case 4/5 load of the other slot. Require
`table +8 == ticket +8` as well. The host is the only writer of a non -1 value there [read: the two game writers store -1,
0x80152A34 and 0x80154E94], so this test is still stateless and names only armed tickets.

### P3 (major): F2 widens coop_player_held, which brings back the failed attempt "making player 1 held stopped the enemies from coming at all".

coop_player_held feeds event 59 for every threat search (0x8003230C), hazard touch (0x8014512C) and damage field (0x801CB658):
hle.c:21076-21090 answers "no" for a held player. It also feeds both sighting paths (22166, 22198).
docs/coop-failed-attempts.md:562 records: "making player 1 'held' with a test helper stopped the enemies from coming at all
(their threat search asks only 'the player')".

With F2, in S1 (player 1 leaves, player 2 stays in the shared room and fights), player 1 is held for the whole countdown, about
62 ticks. Any creature whose threat search asks only "the player" asks about player 1 and is told no
[read 21081-21088; inferred for each kind]. So no creature starts on player 2 then. That is the same effect, on the user's
exact case, in the direction the design did not test (K3 has player 2 fighting while player 1 leaves).

**Fix:**
- Leave coop_player_held as it is.
- Add a separate `coop_player_leaving(cpu, k)` (`coop_stay() && room_slots() == 2 && coop_room_change_on(cpu) &&
  (k == 1) == g_coop_room_p2`).
- Use it only in coop_enemy_target's `held[]` (hle.c:24630), so creatures already engaged with the leaver switch to the stayer.
- Leave event 59 and the sightings to the game's own event 102.

F2's answer also flips inside the turns:
- coop_hold_open writes busy := 0 (hle.c:19518) for player 1's room_turn (20069-20072), and for player 2's turns under a
  hold, so coop_room_change_on is false inside a turn and true outside it.
- coop_enemy_target skips turns (24579), so the separate predicate above is not affected.
- The event-59 use would answer differently inside and outside a turn [inferred].

### P4 (minor): the arm copies the game's tick in the host. That breaks "use what the game has".

Today the copy is correct (above). But it is a host copy of 0x80155844-0x80155890, and it would go wrong without notice if the
gate ever let +4 == 1 through. A game-native alternative needs no copy: on the arm frame, skip the tick (no passthrough). The
countdown is then one frame longer, exactly as the guard already does, and the intercept tail-calls 0x80154EC4(0). The cost
is one more black frame for the mover. No game logic is copied.

### P5 (minor): the stacked-change detection (a) also fires for a direct change into the room in play.

If a direct 0x80152298 asks for the current record's room A while C is pending in Y:
1. 0x80154C94 takes "the current already holds it" (0x80154D98-0x80154DB8 -> 0x80154E1C).
2. It replies at once and zeroes +20 (0x80154E68-0x80154E7C).
3. (a)'s test (the other record pending, its room != wanted, +8 names it) is still true, so g_ahead_stack is set and a false
   WARNING is logged.
4. (b) saves 0.
5. (c) later turns Y's completed C record back to loaded 0 and writes +20 := 0.
6. The slot thread's case 4/5 (the other record has a room and is not loaded) unpacks C again for nothing.

**Fix:** (a) must also require `current record room != wanted`, so that only the fresh path, which renames the pending
record, counts.

Also, (c) has no slot argument: the call is 0x80151CD4(4) at 0x80153534. The slot is in r18 (×4) at that point (the
completion indexes the table with r18, 0x801534B4-0x80153530) [read]. The design should say it reads gpr[18] and checks
`gpr[18] / 4 == Y`, or (c) cannot tell the slot.

### P6 (minor): the new hooks run inside the Tome copy's turn.

- The dispatcher skips only handlers in tome_copy_hook_off's list (hle.c:5533-5535, 26598-26608). change_ahead_tick, the
  0x80152C5C and 0x80151CD4 notifies, and the new code in notify_room_change are not in it.
- The arm is gated by `!g_tome_copy_turn`, but the clean-up (step 1), the guard (step 3) and G2's (a)-(c) are not.
- A copy turn that reaches 0x80152298 (the Tome's read-back, lr 0x80110E8C) would run (a) against the copy's records and set
  a host flag that the real memory then obeys [inferred].

**Fix:** return passthrough under `g_tome_copy_turn` in every new hook, or add them to kOff.

### P7 (minor): "No creature is in it before the arrival" contradicts the host's own reading. A ticket that never fires leaves a live room nobody is in.

§7 R6 says no creature of the ahead room exists before the arrival "[inferred: nodes are made at the arrival]". But:
- docs/coop-level3-design.md:1916 says nodes of the whole level live in one list, stamped by room.
- hle.c's r6_enemy_apart guards against "a node left over from a room no longer loaded (342-344 in chapter 1)" (just after
  hle.c:24514) [read].

Once C is loaded and set up in Y:
- 0x80123F70 hands any object with +592 == C slot Y's zones. It matches the room id and both loaded bytes,
  0x80123FBC-0x80123FDC [read].
- room_slot_of(C) answers Y from room_slots_frame (13626-13636).
- So leftover C nodes see a resident room during the countdown.

Together, the R6 host paths are gated by r6_apart (24197-24202), so the host does little with them. What the game itself does
with leftover nodes of a resident room is unread.

The "ticket that never fires" case (§8) leaves C loaded with nobody in it until the next change. The design treats it as
resident for a later return. That is the kind of state docs/coop-failed-attempts.md's 2026-11-12 entry ended:
room_slots_return now loads fresh into a room nobody stayed in (hle.c:14330-14336). The orphan also makes F1 refuse the next
split, because the other record is loaded.

**Fix:** when step 1 clears +8 without a firing, also give the ahead record the game's dead mark, if it holds +8's room and
nobody is in it. Add a reader for leftover nodes of a resident room before trusting "no creature".

### P8 (minor): F3's solo baseline cannot be taken. The hooks are not installed in solo.

- §6 installs change_ahead_tick inside `room_slots() == 2` (hle.c:27578-27660).
- room_slots_count (t1) is also there.
- With Mods > Co-op off, rooms_follow_coop unsets ED_ROOM_SLOTS (frontend.cpp:483-499), so room_slots() is 1
  (hle.c:13492-13501).

So t0 and t1 never stamp in solo, and K8's expected `[change-time]` line cannot appear. §3 F3's "In solo it runs the same way,
as logging, for the baseline" conflicts with the design's own install site.

**Fix:** put the stamps on notify-only hooks installed outside the two-slot block, under ED_CHANGE_TIME:
- t0: a notify on 0x80155830 when +0 == 1;
- t1: a notify on 0x80154C94's entry;
- t2: 0x80151FDC at lr 0x801523A8.

Each must stay passthrough, so that in solo the hooks only read.

### P9 (minor): moving the unpack into the frames' idle time changes who runs then. This was never exercised.

Today the reader runs only inside the block. With F1 it runs during the countdown's normal frames:
- **The route-job thread waits.** The reader at 21 now takes main's idle time ahead of the route-job thread at priority 31
  (0x80154084, the design's own table), so R6b's route jobs for both rooms wait until the unpack ends. Creatures asking for
  a route (state 1, "asks again") may stand meanwhile [inferred]. That would look like the fight pausing, which is the
  user's complaint.
- **Case 7 runs above main.** Case 7 (the file not in ARAM) runs the reader at priority 11 (0x80154A5C-0x80154A70), above
  main, so the disc -> ARAM read happens synchronously in the arm frame. That one countdown frame stops for both players
  [inferred: the cost is a host file read].
- **The retrace may wake main late.** interrupt_poll runs between dispatch iterations (main.c:372) and chained blocks stay
  inside a chunk. Whether a tight unpack loop delays main's retrace wake was never seen, because today nothing runs in idle
  time while a picture is wanted [inferred].

K1 to K4 should check that both.tsv has no countdown row more than 1 retrace apart, and check the `[enemy]` route lines in
the countdown.

### P10 (minor): a misleading log line in the runs meant to be read.

At the firing, room_slots_return prints "a return into room C (slot Y): kept, a player stayed in it" (hle.c:14337-14341),
because g_room_slot_room[Y] == C and the record is loaded. Nobody stayed in C: it was unpacked ahead. Say "unpacked ahead"
when table +8 or the ticket names the room.

### P11 (minor, goal side): apart -> apart is left as today.

The user's example (player 1 fighting in one room, player 2 entering another) may be the apart case. F1 only arms on a split,
and the design measures A1/A2 without changing them. This is for the goal refuter, noted here so it is not lost.

## Failed attempts checked

- **A revived dead record (2026-11-12, the solo freeze).** F1 never revives a dead record: it unpacks fresh into one. But the
  never-fired orphan (P7) leaves a room nobody is in live, which the rule written after that freeze avoids.
- **A held player 1 (grab tests, failed-attempts.md:562).** Repeated by F2 for S1 (P3).
- **The fade quad marked in place (R4).** Not touched: F1 does not change the fade marking.
- **The title state without slots.** Not touched.

## Summary

| # | Severity | Problem | Fix |
|---|---|---|---|
| P1 | major | g_ahead_stack is lost or stale across a quick save inside the block, and the load hangs | hle_save_held, a reset at load, a room check in (c) |
| P2 | major | the guard can hold the countdown at 1 for ever, soft-locking both players | after the cap: the game's dead mark, then fire; also require table +8 == ticket +8 |
| P3 | major | F2 through coop_player_held repeats "player 1 held, the enemies never come" in S1 | a separate predicate used only by coop_enemy_target |
| P4 | minor | the arm copies the game's tick in the host | skip the tick on the arm frame instead |
| P5 | minor | (a) fires for a direct change into the room in play; (c) has no slot argument | require current room != wanted; read gpr[18] |
| P6 | minor | the new hooks run in the Tome copy's turn | passthrough under g_tome_copy_turn, or add them to kOff |
| P7 | minor | leftover nodes of the ahead room; the never-fired orphan stays live | dead-mark at the clean-up; a reader for leftover nodes |
| P8 | minor | no solo baseline: the hooks are not installed in solo | notify-only stamps outside the two-slot block |
| P9 | minor | the route-job thread starved, the case-7 read in the arm frame, retrace delivery | measure in K1 to K4 |
| P10 | minor | a misleading "kept, a player stayed in it" line | say "unpacked ahead" |
| P11 | minor | apart -> apart is unchanged | goal refuter |
