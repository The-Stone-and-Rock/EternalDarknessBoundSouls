# Refute: the split hold (design-split-hold.md), the goal and both players

Lens: the goal and both players. That means both directions and every stayer state. It means every door kind (doors,
ladders, scripted transitions, a scene at the arrival, chapter pages), engine-level, with solo, together and apart
unchanged. It means the mover's door can never break (hit mid-door, a creature following them through). And it means the
checks would prove all of this.

Read: design-split-hold.md in full; mover-readers.md §0-§8; hold-readers.md §0, §3, §4-§6; stayer-readers.md §0,
§2.6, §2.13-§2.15, §3-§4; enemy-readers-2.md (event 61); docs/game/systems/doors.md; docs/game/research/transitions.md
("Every other way a room changes"); docs/game/systems/sanity.md; hle.c (working tree, 29,549 lines); the disassembly
(build/ed-db/disasm.txt, `ed dis`, `ed fn`, `ed door 183`, `ed where`, `ed uses ed87`).
No code was changed, nothing was built and the game was not run.
[read] = seen at the address, hle.c line or file given. [inferred] = reasoned, not seen.

## Verdict: needs changes

The core mechanism holds. Answering the one busy word "not busy" for everyone but the mover's filter is the right lever,
and the design's checked facts are correct where I re-read them:
- the broadcast loop 0x801F5648-0x801F5694 (r31 the receiver, r3 ignored after 0x801F5688);
- the player handler's r31 at 0x80058DF8 and its busy ask at 0x80058E80;
- the 35 callers of 0x80155738, with no other read of r13-23344 anywhere (3 hits: 0x801556E0, 0x801556F4, 0x80155738);
- coop_room_change_on (hle.c:11334);
- notify_room_ticket (11350-11380);
- coop_enemy_target's held choice (25602-25608).

The design's central safety claim is wrong. It says "busy 0 for all but the mover can reach nothing ordinary play does
not reach" (§0.2). In fact the game has creature code, held back only by that busy ask, that writes a second room ticket.
The host's one-ticket rule does not cover it. The design also does not read or check several of the stayer states and
door kinds it claims. Its checks cannot show the mover protected, because no run puts a creature at the mover.

## 1. BLOCKER: a creature can write a second ticket in the countdown, and the one-ticket rule does not see it

- **The Trapper's capture is gated only by the busy ask that S1 answers 0.**
  - Kind 39's tick, at 0x800E8918, runs `bl 0x80155738; cmplwi r3,0; bne skip`. It then sends 133 to its target
    (0x800E8950-0x800E8958) [read].
  - If the answer is 1, it does the following [read 0x800E896C-0x800E8A64]:
    - takes the **current player's** node and object (0x801F62B0, 0x801F633C);
    - clears the hold bits of the kind-39 nodes (0x801F6828(n, 0) at 0x800E89EC);
    - **stops the current player's motion** (0x80124788 at 0x800E8A40);
    - sends 53 (0x800E8A54);
    - calls 0x800E8E14. That writes a ticket: transition 16, countdown 190, delay 128, to room 83 (the
      `bl 0x80155770` at 0x800E8FE4) [read; transitions.md:291].
  - Room 83 is the Trapper dimension. Script 1573 refuses a chapter page there ("cannot use that now"), and
    coop-glitch-todo.md E6/E7/C7 name Karim's Trapper dimension [read]. So this is kind 39, the Trapper [inferred from those].
  - In solo none of this can happen during a door, because the busy ask says 4.
- **R4's one-ticket rule cannot refuse it.** coop_script_go_to_room (hle.c:16747-16756) is the fn66 intercept, and the
  Trapper calls the ticket writer 0x80155770 directly. The host has only a notify on 0x80155770 (notify_room_ticket,
  installed at 28632-28634) [read].
  - **Player 2 leaves, player 1 stays near a Trapper.**
    - The current player is player 1, the stayer. S1 answers their filter 0, so they answer 133 and are captured.
    - The 0x80155770 record (one record, `+0` the countdown) is **overwritten** with room 83's ticket.
    - notify_room_ticket runs again: g_coop_room_p2 becomes false, g_fade_mover becomes 1, g_coop_p1_tickets++, and
      g_split is set again.
    - Player 2's door motion plays on with no ticket of theirs. Their door is broken, and the next change is a 190-tick
      Trapper change of player 1's [inferred from the code read].
  - **Player 1 leaves.** The current player is the mover. K1 swallows 133 (it is on 0x800CA964's always-taken list,
    mover-readers §2), so the capture fails. Whether kind 39 can be pointed at player 2 is unknown (coop-glitch-todo E6).
- **The same class, not read to the end:**
  - Kind 50's busy-gated path at 0x800A29B4. It counts down a timer, takes an item count (0x80034E20/0x800354F8) and
    sends 135 to r13-26424 (0x800A2AA4-0x800A2AB4).
  - That event's handler (case 135 at 0x800A3274) calls the ticket writer 0x800A2548 (0x800A32A8). That writer makes a
    transition-17 ticket (0x800A281C) "if a linked object's room isn't the current room" [read the calls; owner and effect
    not read].
  - The sanity tracker's end 0x8006BFAC writes a ticket (0x8006BFF4). It is reached from 0x8006B438, and a sanity effect
    ends through it (sanity.md "What failed") [read]. Whether its tick is held by busy was not read.
  - The direct room changes with no ticket (0x80152298 from 0x800AD6F4, 0x80157458, 0x800ADADC, 0x800AE61C, 0x8004FC3C,
    0x80020328) were not checked for a busy gate either.
- **The game itself splits "busy" from "a door counting".** The ticket countdown getter 0x80155818 has 8 callers:
  0x80064644, 0x8007D1E0, 0x8007D49C, 0x8007E29C, 0x8008D380, 0x800E0664, 0x801423E4 and 0x8014E0EC [read grep].
  - 0x8008D340 tests busy (0x8008D3C8) **and** `ticket > 0` (0x8008D3DC) separately [read], so that site stays safe.
  - 0x8007E140 tests the ticket only on its busy path (0x8007E288-0x8007E2A4) [read]. With S1 it takes the other branch.
  - So "busy 0 with a ticket counting" is a state the game's code distinguishes and solo never produces. It is not
    "an ordinary frame".
- **Fix:**
  - Add a K7 to the keep list: every busy ask whose path reaches 0x80155770, 0x80152298 or a player's motion. At least
    0x800E891C (kind 39); 0x800A29B8 (kind 50) once read.
  - As a backstop, add an intercept on 0x80155770 (the entry carries only a notify today; intercept and notify can
    share it). Under split_counting, a second ticket is refused and logged, as R4 refuses a second fn66.
    - This alone is not enough: kind 39 stops the motion and sends 53 before it writes. So K7 is still needed.
  - Read the 0x80155818 callers and record which way each goes with busy answered 0.
  - Correct §0.2's claim in the design.

## 2. MAJOR: the stayer's death in the countdown is new, unread and unchecked

- §7 accepts that the stayer "can now be hurt and killed during it". The basis is one sighting: a game over about 1500
  frames after a death (coop-no-pause.md:75).
- Row 79 of the same table: player 2 drained to 0 gave "game over for both" [read]. So a co-op death does lead to the
  game over.
- For player 2 leaving, the stayer is player 1: the game's own "the player died" (event 8 inside 0x80061E90, mover-readers
  §4), with player 2's ticket counting and the R4 bracket coming. Solo never reaches that combination.
- What the death does in its first 62 frames was not read (stayer-readers §2.14, Q7). Does it set a busy bit (which turns
  coop_room_change_on off mid-window), start a scene, or go through fn66 (R4 would drop it, 16748) or 0x80152298 directly
  (a stacked change)?
- **Fix:** read event 8's player path to its room call before building. Add a check that kills the stayer in the window,
  in both directions. A test helper that sets health low, if one exists, would do it.

## 3. MAJOR: casts and spell effects finishing in the window can ask for a scene or a room, which F4 exists to stop

- The host's own reason for refusing quick spells in the other's change: "Not held, player 1 could cast through player
  2's door motion and fade, which may ask for a scene or another room" (hle.c:19751-19752) [read].
- S1 answers 0 at the spell and effect timers (0x801F22A0, 0x801F248C, 0x801F2514, 0x8017BF14) and at the record steppers.
  So a cast **begun before** the ticket now completes inside the window.
- The ticket's cast end 0x801C5118(0) ends only the controlled player's cast record (§1.1, read by the design), not the
  stayer's.
- So the stayer's cast completes in the window. A summon, Summon Trapper's "switch player" reload (coop-glitch-todo E8),
  or any spell whose end starts a scene can do what F4 forbids. This is a state the "Casting" row of §7 does not cover
  [inferred].
- **Fix:** read which spell ends ask for a scene, a room or a body switch (stayer-readers Q6). Keep their record or effect
  sites on the keep list, or hold just those spells, in a split. Add a cast check (a stayer mid-cast at the ticket, both
  directions).

## 4. MAJOR: the checks cannot show that the mover's door never breaks

- **No run puts a creature at the mover.**
  - Room 183 has one door region: Region01, the box [183,2994]-[541,3194] (`ed room 183`, `ed door 183`: L01/119)
    [read].
  - **K1** puts player 1 at 1305,1508, outside it (`ed where`: CameraRgn05/07 only), and fires the door by
    ED_RUN_SCRIPT. The mover never walks a doorway, and the motion plays toward the nearest GravityMkr.
  - **K2** puts the mover (player 2) in Region01 at 360,3090, about 2,000 units from the zombies at about 1042,1101 [read].
  - S3 turns every creature to the stayer at its next step, so in both runs nothing goes at the mover.
  - So K1's "no [hurt] 53 or 39 to 1" and K2's "P2 in state 121" are vacuous. K1 (the keep-list entry) is never
    exercised.
- **Nothing logs what the mover's filter took.** S6 prints "the mover's events keep the game's busy" once. It needs a
  line per event the mover's filter swallowed (11, 39, 53, 32, 59, 133, 55), with the sender, so a run proves the
  protection.
- **"A creature following them through" has no check.** Add a must-read:
  - no creature's object +592 becomes the new room;
  - no row of room 180 gains a node of 183;
  - the mover's arrival motion runs (state 80 then 81 with motion 25/104/103: event 238's answer), not "no motion" (the
    hurt-move outcome of mover-readers §4).
- **Fix:**
  - Add K1b: player 1 inside Region01 with a zombie in state 3 after them within swing reach, pressing B on OPEN DOOR.
  - Add K2b, the mirror: player 2 at the door with a zombie on them and player 1 elsewhere in 183.
  - In both, put the stayer in their words, so that S3 picks nobody and the creatures stay on the mover.
  - Must-read for all of them: the per-event swallow lines, the mover's state 121 to the firing, health unchanged, the
    arrival motion, and no +592 change on any creature.

## 5. MAJOR: the user's literal report is the apart case, and this design leaves it

- The user's 2026-11-12 play report (memory coop-separate-rooms.md): "player 1 fighting in one room, player 2 enters
  another: player 1's screen pauses a moment".
- hold-readers §0.6 and stayer-readers §0.2 read the hold as the one busy word and a standing bit 8. That holds apart
  too. Room 180's zombie 33 showed hold 1 in M3 with no 102 sent there [read in those reports]. The stayer's filter also
  swallows hits and 59 apart (mover-readers §0.8).
- "Apart -> apart the picture stop alone" (coop-level3-design.md:2642) was never measured with creature rows (M5 had none).
- The design changes the split only. K3 measures apart, and §14.5 defers the per-room rule.
- So after this build, the exact case the user described is very likely unchanged [inferred, strong].
- **Fix:** run K3's apart measurement first, in the same build. Either include the per-room answer (busy 0 to nodes not
  in the mover's room, K1 for the mover) or tell the user plainly that their reported case is the next item.

## 6. MAJOR: the stayer states are claimed but not checked; two of them have open paths

The checks cover only these: the stayer readied and attacking (K1, player 2), the stayer walking (K2, player 1), and
the user's play.

Not run at all:
- a grabbed stayer;
- a stayer mid-cast;
- player 1 in a close-up during player 2's door;
- player 2 in their words, find or Tome copy;
- a stayer killed (section 2).

Open paths:
- **A grabbed stayer.**
  - §6 says BREAK FREE counts the stick "in their turn".
  - In player 2's door the input lock 0x800443E4(count+3) is written in player 2's use. Player 1's room_turn exists
    because "their input block never had the stick" (hle.c:19640-19643) [read].
  - Whether a grabbed player 1 can wiggle during player 2's countdown, or only takes the bites, was not read.
  - The game's grabs are in ch11 room 50 (coop-no-pause.md:75, 79), with ED_TEST_TARGET=2 as the helper. Add a split
    there in both directions.
- **A creature killed in the window starts a room script.**
  - Solo cannot kill a creature in a door's countdown; the stayer now can. A death event can wake room scripts.
  - The script's busy getter is kept (K3), but a script woken by an event and calling fn66 is **passed over and lost**
    by R4 (16748: "the second waits for a new press": a scripted door has no press).
  - fn22 or a cinematic turns coop_room_change_on off mid-window. The room then freezes again, and the ticket fires
    under a scene [inferred].
  - 0x800E0664 shows the game guarding a post-death path by "no ticket" itself (after 0x80061B9C, r27 == 8, then
    `0x80155818 == 0`) [read].
  - **Fix:** read the death and wake paths, or keep "a script started in the window" waiting to the firing. Add a
    must-read for `[stay] a room asked for ... passed over` in the window.

## 7. MINOR: door kinds listed but not checked

§9 reasons about each kind but runs only two of them: transition 9 (K1, K2) and 15 (K4). Add one run each for these:
- a zone exit (transition 2, 12 ticks, 422 uses, the most common way out);
- an item door from the Tome. Room 183 has them: L01/1100-1103, transition 2 to 180 and 0 within 183 [read `ed door 183`].
  It checks a ticket written after the Tome and g_tome_copy_turn's gate;
- a door from a look with its hold still on (busy 2|4, CLIMB DOWN from player 1's look);
- a door preceded by a scene (L03/420's event 82).

Chapter pages are settled. ed87 runs only from the page scripts 1572-1581/2028/1010, which are Tome uses
(after_tome, hold_scene), not tickets [read `ed uses ed87`, `ed script 1573`]. So §9's "never a split" stands.

## 8. MINOR: the firing now lands on a live fight

- Event 61 to the room left (0x801520DC-0x801520F4) is left to the game (§8). With this design the stayer is likely
  mid-fight at the firing.
- 61 sends creatures in states that do not take it to state 0, where they drop their route and are unregistered
  (0x8003B850-0x8003B884). In state 33, 0x8003A1D0(id) == 4 sends 57 and the creature goes inert (enemy-readers-2:127-129)
  [read there].
- The user may see the fight hitch at the firing instead of the one-second stand.
- **Fix:** K1/K2's must-read should run to firing+20 (3882-3902). Each zombie's state should be back to 3 after the
  stayer within a few frames (M3: 40 by 3884, 39 by 3890), with none inert. Name 61 as the next visible stop.

## 9. What holds (checked, no change asked)

- **Gates.** co-op && coop_stay() && r1 == r2 && !g_tome_copy_turn, set at every ticket. Solo, follow mode, co-op without
  ED_COOP_STAY, rejoin and apart -> apart take none of it. K5 and K6 read that. coop_stay() is the env plus two slots
  (hle.c:13986-13991) [read].
- **K1's register.** r31 is the handler's node from 0x80058DF8 to the ask at 0x80058E80 [read]. Both directions reach
  0x80058DD8: player 2 through coop_events [read mover-readers §5]. K1 ahead of today's cases closes the coop_p2_holds
  and close-up exposure of mover-readers §6 in a split.
- **S2's condition.** It is exact: lr 0x801F568C is the only broadcast call, 102 there is only the busy set's (0x80155718),
  and the receiver is r31 [read 0x801F5620-0x801F56AC, 0x801556CC-0x80155734]. The 102s from the creature-code busy
  clears (0x80085098, 0x800851A8, 0x80085654, 0x8009C3E8: busy &= ~2 with 4 left, a broadcast) are kept from the room by
  the same rule [read].
- **The pump (K5).** The structure matches 0x801F541C-0x801F54C0 [read].
- **The forced-hit pair K2.** At 0x8008106C (39 to a receiver past the filter) it belongs to the fireplace's wrong tile
  0x80080D70 (hle.c:19962), a close-up, so it cannot fire in a split countdown. Keeping it is harmless.

## 10. Facts read for this report

| Fact | Where | Mark |
|---|---|---|
| Kind 39's tick: busy ask, 133 to target, current player's motion stopped, 53, room-83 ticket | 0x800E8918-0x800E8A64; 0x800E8E14; bl 0x80155770 at 0x800E8FE4 | [read] |
| The ten ticket-writer calls (fn66 two, ed29, seven game code) | grep `bl 0x80155770`: 0x801668E8, 0x80166930, 0x800E8D1C, 0x800E8FE4, 0x8006BFF4, 0x8009B808, 0x800A281C, 0x800ADAB8, 0x800C0594, 0x8012D2EC | [read] |
| R4's one-ticket rule is on fn66 only; 0x80155770 has a notify only | hle.c:16747-16756, 28632-28634 | [read] |
| Room 83 = the Trapper dimension | script 1573 (fn25 == 83), coop-glitch-todo.md E6/E7/C7 | [read; identity inferred] |
| Kind 50: busy-gated 135 to r13-26424, case 135 calls the ticket writer 0x800A2548 | 0x800A29B4-0x800A2AB4, 0x800A3274-0x800A32A8 | [read; owner not read] |
| The ticket countdown getter and its 8 callers; 0x8008D340 tests busy and ticket apart | 0x80155818; 0x8008D36C-0x8008D3F0 | [read] |
| Only three reads/writes of r13-23344 | 0x801556E0, 0x801556F4, 0x80155738 | [read] |
| Creature-code busy clears (busy &= ~2) broadcast 102 when 4 is left | 0x80085098, 0x800851A8, 0x80085654, 0x8009C3E8; 0x801556CC-0x80155720 | [read] |
| The broadcast loop, r31 the receiver | 0x801F5648-0x801F5694 | [read] |
| The player handler's r31 and busy ask | 0x80058DF8-0x80058E88 | [read] |
| The fireplace pair 0x8008106C | 0x80080D70 (one caller 0x80080C0C); hle.c:19962 | [read] |
| Room 183's one door region and the K-run places | `ed door 183`, `ed room 183`, `ed where 183 360 3090`, `ed where 183 1305 1508` | [read] |
| Chapter pages are Tome uses | `ed uses ed87`, `ed script 1573` | [read] |
| The quick-spell refusal's reason | hle.c:19748-19752 | [read] |
| coop_room_change_on, notify_room_ticket, coop_enemy_target, coop_follow, notify_room_slots_tail | hle.c:11334, 11350, 25554, 21475, 14516 | [read] |
| The user's apart report | memory coop-separate-rooms.md (2026-11-12 play report) | [read] |
| Event 61 in state 0 / 33 | enemy-readers-2.md:126-129 | [read there] |
