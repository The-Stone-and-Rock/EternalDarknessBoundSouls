# The Tome trip build: safety review (2026-11-14)

What was reviewed: the working-tree diff of runtime/host/hle.c (P1-P11, 574 lines), tome-trip-impl-notes.md and
build-run-tome.sh, against design-tome-room.md. I read the code; nothing was built or run, and nothing was edited except
this file. Line numbers are hle.c's in the working tree.

Tags: **[real]** means the code does this, read at the lines given. **[maybe]** means the code path exists, but whether
it bites depends on game behaviour nobody has read or run yet. Each point gives a suggested fix. None of the fixes are
built.

## Verdict in short

- **Solo, follow mode, and `ED_TOME_TRIP=0` with the stay are untouched.** Every changed expression reduces to what it
  was before when `tome_trip_on()` is false or the record is in state 0.
- **Identity swaps are restored on every path.**
- **Guest calls from inside intercepts are safe.** The passthrough flags are set after the nested calls, and
  `hle_guest_call` restores the CPU state.
- **Re-entrancy is guarded.**
- **The weak spots are the trip's failure and stale paths:**
  - the timeouts drop the kept way back, so the players are stranded in room 5;
  - a record that outlives its game (a game over, player 2 dropping out, or a soft reset) can write a ticket into a later
    game;
  - a host state taken in states 3-5 strands the player left in room 5;
  - between B's change and A's ticket, A is free and unheld in room 5, with no menu or cast check.

## 1. Solo and together untouched

- [real, OK] **Solo.** `tome_trip_on` (11423) needs `g_settings.mod_coop`, `g_coop_p2`, `coop_stay()` and
  `g_tome_trip_hooks == 3`.
  - `split_ticket_ask`, `notify_ticket_run` and `notify_behaviour_pass` are installed only with the stay (32097, 32428).
  - `coop_follow` runs only with co-op on (31300).
  - `tome_trip_runner` (15282) has no `tome_trip_on` test, but it needs state 4/5, which only an "on" path sets.
  - `split_ticket_open`'s new test (26503) and the `g_fade_mover = 0` line (11529) need state 1, which only
    `tome_trip_ticket` sets, and that is behind `tome_trip_on`.
  - `coop_p1_unplaced` (21539) and the arrival-scene gate (18544) reduce exactly to the old expressions when
    `tome_trip_change()` is false.
  - `tw_state_loaded`'s memset is harmless.
- [real, OK] **Follow mode** (`ED_COOP_STAY` unset): `coop_stay()` is false, so the hooks never reach 3 and nothing runs.
- [real, OK] **`ED_TOME_TRIP=0` with the stay:** `tome_trip_keep` returns false at once (`!tome_trip_on`), so
  `split_ticket_ask` behaves as before. No `[tome-trip]` line is printed: the install line is gated on the switch.
- [real, OK] **Together under the stay** (both in one room, any door but room 5): every trip piece first tests
  `r3 == TOME_ROOM`, state != 0, or the asker in room 5.
- [real, cosmetic] **`static unsigned g_coop_tickets;` is declared twice** (11427 and 11480). These are two C tentative
  definitions, so the build is legal, but a later initializer on either one would break the build.
  - Fix: use `extern` or move the declaration up.

## 2. The stayer's door work and the split hold

- [real, OK] **The trip's ticket gets no window.** `split_ticket_open` returns with "the Tome room: both go", the first
  test (26503). The stayer is held through the 12/62 ticks as before split-hold.
- [real, OK] **The host's tickets bypass `split_counting`.** The guard in `tome_trip_keep` (15123) skips G1. They are
  only written when `idle` holds (15344: no countdown, busy 4 clear, no bracket), so no window can be counting.
- [maybe] **B's and A's changes open ordinary stayer windows.** The window is shared when B and A are both in room 5, and
  apart for A's. Countdown 1 for B, and fn66's own for A. The design expects this. It has not been seen.
- [real] **P2b puts the dissolve out during the trip's own countdown.** `tome_trip_passed_over` is called at R4's
  one-change-at-a-time pass-over (17989). There it runs `0x80044200(0)` whenever r3 == 5, with no test of the trip's
  state.
  - The case: together (Karim's 338, both at the ladder), O's door asks for room 5 while T's trip ticket is counting.
  - What happens: the shared word r13-25680 (player 1's copy, H2) is turned off under T's fade.
  - The WARNING then says "take the door again", but O is brought along anyway.
  - The effect is cosmetic plus a misleading log line.
  - Fix: skip P2b while `g_tome_trip.state == 1` and the trip's ticket is the one counting.
- [maybe] **On M12's path (17961) the way out waits; it is not lost.** M12 lets it go "by their next look", so the
  "take the door again" WARNING may be wrong there, and the retried door may run without its dissolve. Only the log and
  the effect are affected.

## 3. The Tome copy

- [real, OK] **The trip waits for player 2's copy.** `tome_trip_brings` waits while `g_tome_copy_open ||
  g_tome_copy_turn` (15092), and `tome_trip_step` returns while `g_tome_copy_turn` (15340). The trip waits; nothing is
  forced.
- [maybe] **The state-1 timeout can fire right after a long copy turn.** Its check (15347) is
  `!noted && frame > begun + 600`, and the step does not run during the copy turn. If the copy turn outlasted 600
  frames before the change, the record is cleared the moment the copy closes, even though the trip ticket is still
  counting. That change then runs as player 1's with no noting and no bring.
  - Fix: judge state 1 stuck only when no ticket is counting.
- [maybe] **A trip can begin during the copy's turn.** That only prints a WARNING (15032). Player 2 is then left in their
  Tome menu in RO, and player 1 alone may finish the Tome (b = 0). This is acceptable, but not tried.

## 4. A death, door, scene or save during the trip

- [real] **Between B's change and A's ticket, A is free in room 5.**
  - B's arrival clears every busy bit (`0x801556CC(-1,0)` at 0x80152184), and the R4 bracket lets the stayer's hold go.
  - A can then act in room 5 for up to 120 frames (the come-into wait) plus B's change: open the pause menu or the Tome
    (which they now carry), cast, or walk.
  - `idle` (15344) does not test `game_menu_open`, `cutscene_playing` or casting, so A's ticket can be written with a
    menu open or mid-cast. Solo never has this window: fn66 holds the player straight away.
  - Fix: hold A for the wait with the game's own hold, or gate A's write on no menu, no cast and no cutscene.
- [maybe] **A death or game over during states 3-5** (B back in RO among enemies; A waiting).
  - Nothing clears the record on a game over, a continue or a soft reset; only `tw_state_loaded` does (13656).
  - After the game reloads, the step finds `idle` and writes the kept ticket (room RT, script 868) into the new game.
  - The 600-frame timeouts don't stop this: in state 3, and in state 4 after the end, `idle` comes first.
  - Fix: before each write, check that the player about to move is still in `TOME_ROOM` (and B's/A's object exists), else
    clear. Also clear the record on a reboot.
- [maybe] **Player 2 drops out mid-trip.**
  - When `g_coop_p2 = 0` ("player 2 is gone", 31276), `tome_trip_on` goes false and the record freezes.
  - In state 3 the voided way back is then never written, and player 1 is stranded in room 5.
  - When player 2 respawns, the stale record resumes.
  - Fix: same check as above, plus "player 2 gone during states 3-5" should write the kept ticket as the game asked.
- [maybe] **A door of B's during the come-into wait** is handled. B's ticket counts, the step waits for idle,
  `tome_trip_runner` ignores it (`ran` is already set), and A's write comes after.
- [maybe] **A scene in B's fresh room.** A room-start cutscene there does not hold A's write (`idle` has no cutscene
  test), so A's change runs during B's scene.
- [maybe] **Player 1 is moved in RO with no wait when T = player 2.**
  - The trip refuses nothing (design H8), and the bring waits only for player 2's casting or copy.
  - So player 1 is moved mid-cast, mid-find, mid-words, mid-cutscene, or with a menu open. The cutscene case prints a
    WARNING; the rest are silent.
  - This is asymmetric with the bring.
  - Fix to consider: test player 1 as the bring tests player 2, or refuse the trip ticket while player 1 is casting or in
    a menu.
- [maybe] **Room 5's scripts keep running during the wait.** 1599, 852 and 2675 go on in A's room for the ~10-150 frames
  of B's change and the wait. In solo the change ends them at once. Nobody has read what any of them does there.

## 5. A state load mid-trip

- [real, OK] **Quick-state loads clear the record.** `tw_state_loaded` runs from the COPR blob's load
  (quickstate.c:123, also called for a state without the blob), and the lent slot 0 is the state's own.
- [real] **A state taken in states 3, 4 or 5 strands the room-5 player when loaded.**
  - In that state 1599 has already called fn66 and its ticket was voided. On load the record is cleared, and nothing
    writes the way back again.
  - Whoever is still in room 5 stays there: the fallback only acts on a new fn66, and 1599 does not run again.
  - Fix: on load, if a player's object is in room 5 and 1599 is past its fn66, write the game's way back. Simpler: copy
    the kept ticket into the COPR blob.
- [maybe] **A state taken in state 1/2, then loaded.**
  - The loaded game's countdown runs with whatever `g_coop_room_p2` holds. With T = player 2 that may be an R4 change of
    player 2 alone into room 5 (the known failure).
  - The fallback then sends only those in room 5 to the marker. This is the degradation the design accepts.

## 6. Guest calls inside intercepts (the passthrough flags)

- [real, OK] **`hle_guest_call` restores the CPU state.** It saves and restores everything up to `srr0`, so gprs, lr and
  fprs are back afterwards (main.c:70-111). The `cpu->lr = lr` re-sets are redundant but harmless.
- [real, OK] **`tome_trip_passed_over`** guest-calls 0x80044200 inside fn66's intercept. The three callers set
  `r3 = 0`, `g_hle_passthrough = false` and `g_hle_tail_call = false` after it (17961-17993). 0x80044200 has no hook.
- [real, OK] **`tome_trip_keep`** makes no guest call. It voids with both flags false, so the dispatcher's `hle_return`
  runs and `notify_room_ticket` doesn't. Its guard path returns with passthrough true, and the writer's notify runs.
- [real, OK] **`tome_trip_brought`**'s 0x801F5620 call comes inside the bring, next to its existing guest calls.
  0x801F5620 has only a notify, which does not touch the flags (5641-5648 only reset them for an intercept).
- [real, OK] **`tome_trip_write` and `tome_trip_change_end`** make their guest calls from `notify_behaviour_pass`, a
  notify, so they cannot decide whether the pass runs.
- [real, OK] **The guest call's stack words line up.** They go at `((r1 - 0x800) & ~15) + 8..16`, and the writer's
  `stwu r1,-16` then reads them at 24/28/32(r1).

## 7. Re-entrancy

- [real, OK] **Our own ticket does not re-enter the keep.** `tome_trip_write` calls the writer, then `split_ticket_ask`,
  where the keep sees the guard and passes, then `notify_room_ticket`, where `tome_trip_ticket` sees the guard and its
  room is not 5 anyway. Countdown 1 means the runner runs at a later tick, not inside the call.
- [real, OK] **Room loads inside the runner** reach `tome_trip_note`, which needs state 1, so it returns for states 4/5.
- [real, OK] **A second ticket into room 5** while a trip is open clears the old one (15018); `tome_trip_clear` puts
  slot 0 back first.
- [maybe] **A room script started inside 0x80152298 can write slot 0** (`fn104(0, ...)`) before the runner reads it at
  0x80152688. B would then be put at the script's place, and `tome_trip_change_end` (15327) would later overwrite the
  script's value with the old one. No such room script is known; none has been read.

## 8. A trip that never ends (timeouts)

- [real] **Stuck in states 3/4/5 strands the room-5 player.** `tome_trip_stuck` clears the record in every state
  (15353, 15361, 15380, 15388), and the kept ticket is lost with it.
  - "The way back kept, never free to write" (3) and "B's change never ran" (4) leave both players in room 5 for good.
    1599 has finished, so no fn66 comes again.
  - "B back, never free to write A's" (4) and "A's change never ran" (5) strand A.
  - The same goes for a write that fails (`tome_trip_go` only prints "not written", 15277). The state stays 4 or 5 until
    the 600-frame timeout clears it.
  - Fix: on a failed write, or a stuck state 3 or 4, write the kept ticket as the game asked (regs and stack unchanged,
    as player 1's or as the asker's), then clear. Only states 1 and 5-after-the-runner may simply clear.
- [real, OK] **State 2 has no timeout**, which is right: the players are in room 5 until the pickup.
- [maybe] **The refusal path can strand both players if the script test is wrong.** A fn66 out of room 5 by a script
  other than 1599 or 1413 is voided in states 0-2 (15137). If r13-23228 is not 1599 when 1599's fn66 runs, the way back
  is refused, and both players stay in room 5 with no recovery. The 2026-11-08 log shows `script run r3 1599` at the
  ticket's frame (page-0001.log:3245), so the test likely holds.
  - Fix: below state 3, keep rather than void (the fallback), and print the WARNING.

## 9. Player 2 / player 1 identity swaps

- [real, OK] **`tome_trip_write` (15209)** follows the use's order: `coop_be_player2(true)`, then `coop_stay_room_in`,
  then the call, then `coop_stay_room_out` (only if `in`), then `coop_be_player2(false)`.
  - No early return sits between them, and the guest call failing still restores both.
  - The casting-word toggle in `coop_be_player2` is paired.
- [real, OK] **The step runs at the behaviour pass's entry** (lr 0x80008400 / 0x80050A9C), outside player 2's turn and
  outside any use (`g_coop_use` lives within a script call). It never writes during an R4 bracket (`g_stay_bracket == 0`
  in `idle`). So the wrap is never nested inside another swap.
- [real, OK] **R4 brackets opened by our tickets** are closed by the existing `coop_stay_change_end` in the same pass
  that the step later reads.
- [maybe] **The voided ticket when K = player 2.** When 1599 runs as player 2's use, the voided ticket never reaches
  `notify_room_ticket`, so `g_coop_tickets` stays put. `continue_coop_use` (19498-19517) then reads "no room asked".
  - If 1599 left any busy bit set, the screen route runs (`g_coop_screen = true; coop_hold_player1()`), and player 2's
    cameras stay lent.
  - B's R4 change would then run with the cameras lent ("today's drop", 14511), and player 1 held.
  - Fix: count the kept ticket as a ticket for the use (bump `g_coop_tickets` at the keep), which also ends
    `tome_trip_change()`.
- [maybe] **T = player 2: `g_coop_room_p2` is off from the ticket** (15027). Player 1, in RO, is "the asker" for
  `coop_room_change_other` during the countdown, while player 2's quick cast and Start are refused. The implementer
  already noted this; the window is one countdown.

## 10. Smaller points

- [maybe] **Exact return inside an unprompted exit** (chapter 6 BoxRegion02; chapter 3's plane). With the Tome now
  carried, 335 tickets to room 143.
  - Only the come-into mark stops it: one `g_follow_fresh` word, cleared by the next ticket (11538).
  - If B's first look does not come within 120 frames, A's ticket clears B's mark too.
  - Run 6a decides this.
- [maybe] **A's own-room redirect** (15190). When A is not noted in the way back's room, A goes to their own room with
  868 still the ticket's script. 868's Camera01 follow-up then sets that room's Camera01. The cut is cosmetic.
- [maybe] **B goes to RO with the way back's camera hash** when their own camera is not found (15255). The design's §11.6
  covers this: 0x801EE998 returns -1, and what 0x801EE764 does with index -1 is unread. A WARNING is printed.
- [maybe] **The fallback overlaps both players.** With both in room 5, both go to the same marker, one after the other.
  The "stood inside player 1" rescue in `coop_follow` (60 still frames) parts them later.
- [maybe] **Chapter 13 is altered although it is not built.** The 1413 way back is kept by the fallback (state 0) and
  written a frame later as player 1's. The ticket is the same but late. Not built, by the user's decision, yet it is no
  longer purely the game's path.
  - Fix: drop 1413 from the keep until P12.
- [maybe] **The runner faces the player with an unset heading.** At 0x8015272C it calls 0x80124BE4 with the unset
  16(r1). The game's own saved-place arrivals do the same, and `tome_trip_change_end` corrects the facing at the next
  pass. This is not new risk, but one frame may show a wrong facing.
- [real, OK] **build-run-tome.sh** removes and writes only its own `saves/tome-chN.state` and `logs/tome-chN.env`. It
  sets `ED_RUN_AWAY=1`, and it does not use `ed test`, `ed chapters` or slot7.

## Fixes in order

1. States 3 and 4 when stuck, and a failed write: write the kept ticket as the game asked; never just clear it (§8).
2. Before each write, check that the mover is in room 5 and the record belongs to this game. Clear the record on a reboot
   and on player 2 dropping out (§4).
3. Gate A's write on no menu, no cast and no cutscene, or hold A for the wait (§4).
4. A state load in states 3-5: re-issue the way back (§5).
5. P2b: skip it while the trip's own ticket is counting (§2).
6. The state-1 timeout only while nothing is counting; bump `g_coop_tickets` at the keep for player 2's use (§3, §9).
