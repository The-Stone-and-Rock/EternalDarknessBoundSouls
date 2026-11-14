# B3: the ticket's input lock (readers)

For design-split-hold.md §3.9 (G2) and §13 B3. Read only: no code changed, nothing built, the game not run.
[read] = seen at the address or hle.c line given. [log] = seen in a run log. [inferred] = reasoned, not seen.

## 0. The answer

1. **0x800443E4(n) sets two frame counts, not flags.** Each is "raise to at least n" (unsigned max). It never
   shortens a lock. [read 0x800443E4-0x80044414, 0x800068E0-0x800068F4, 0x8013E99C-0x8013E9B0]
   - **r13-23592, the binding lock.** While it is not 0, no input channel runs its button rows (bindings), on any
     pad. It counts down once per pad update 0x8013E518, the end of every frame.
   - **r13-26232, the Start lock.** While it is not 0, the Tome or menu opener 0x80006CDC returns at once. It counts
     down once per world frame, in the play channel's own handler 0x80006900.
2. **The ticket writes count + 3.** That is 65 for a 62 countdown (transitions 3, 6, 9, 12, 15). The lock outlives the
   firing by about 3 frames. [read 0x8015578C-0x80155798, 0x801557F0]
3. **Neither count reads the ticket.** If G2 skips the tick 0x80155830 for N frames, both locks still run out at about
   ticket + 65. For N of 3 or more, the locked player's buttons and stick come back before the change fires. [read;
   timing inferred]
4. **In co-op the lock goes into the game's input copy.** That is player 1's for both doors. Player 2's door locks
   player 1 (the stayer), not player 2. See §4.
5. **The game's way to renew it is 0x800443E4 itself**, called again with the writer's own formula, ticket +0 + 3.
   Because it is a max, calling it every waiting frame holds the lock at exactly the solo margin. It is right only for
   player 1's door. For player 2's door it would hold the stayer. See §5.

## 1. 0x800443E4: one value into two counts [read]

```
800443F4  mr r31, r3
800443F8  bl 0x800068E0      ; r13-26232 := max(r13-26232, n)
80044400  bl 0x8013E99C      ; r13-23592 := max(r13-23592, n)
```

- 0x800068E0: `lwz -26232; cmplw r3, r0; ble keep; mr r0, r3; stw -26232`. This is an unsigned max. [read 0x800068E0-0x800068F4]
- 0x8013E99C: the same shape on r13-23592. [read 0x8013E99C-0x8013E9B0]
- Getters and clears [read]:
  - 0x800068F8 reads r13-26232.
  - 0x8013E9B4 zeroes r13-23592. It is called by state 30 (Finish him, 0x80051F0C) at 0x80052224.
  - 0x80006988 zeroes r13-26232 and r13-26228 when it makes the play channel (0x800069C0, 0x800069C8). It is called
    from 0x800215AC, 0x80021C80 and 0x800220C4.
  - 0x8013DA54 zeroes r13-23592 in the input's init.
- Callers of 0x800443E4. Each passes a frame count [read]:
  - the ticket writer, 0x801557F4: count + 3;
  - 0x8011105C: 5;
  - 0x800C35F4: a motion's length times 2, only when it is under 600. This is in Finish him's path (hle.c:21865: "player
    1 stood 200 to 330 frames").
- 0x800068E0 alone has 22 callers. Examples: 100 with r13-26256 := 1 at 0x80062028 and 0x80062080; 305 at 0x800A6214;
  20 at 0x800A84C4; 182 and 180 at 0x800C4A70 and 0x800C4D64. 0x8013E99C alone is also called at 0x8005D09C (55). [read]

## 2. r13-23592: who counts it and what it locks

**Counted down** once per call of the pad update 0x8013E518, after the channel timers [read 0x8013E804-0x8013E81C]:

```
8013E804  lwz r3, -23592(r13)
8013E80C  cmplwi r3, 0 ; beq
8013E818  addi r0, r3, -1 ; stw r0, -23592(r13)
```

- 0x8013E518 is called by the main loop after each frame (0x80018708), in every game mode, and at 0x801E4558. [read]
  So it counts frames, cinematic frames included [inferred: the loop at 0x800186E8 runs the mode frame, then the
  after-frame list, hle.c:15058].

**What it stops** is in the channel step 0x8013E160 [read 0x8013E160-0x8013E388]:
- A channel held (+16 != 0) is not run. A channel that is not the focus (r13-23552 set and not this channel) gets only
  its +0 handler with r4 = 2.
- A runnable channel gets its +0 handler with r4 = 1 (0x8013E204). Then, at 0x8013E210:
  `lwz r0, -23592(r13); cmplwi r0, 0; bne 0x8013E344`. **A lock skips the whole row loop** and goes to the +8
  handler.
- The rows are the bindings: a pad mask, then a handler (docs/game/research/verbs.md:236). The mask test 0x8013DC74
  reads the pad by the channel's +24 [read 0x8013DC7C-0x8013DC90].
- So, while the lock is up:
  - the walking channels r13-23052, -23060 and -23056 (stepped by 0x80199F98 from the controller at 0x80045D30) set no
    flags. Their +0 handler 0x80199FCC still clears the control block each step (0x80199FF4-0x8019A004). The player
    gets no A, B, Z, stick, X, L or R: they stand. [read; "stand" inferred from verbs.md's table of flag setters]
  - the play channel r13-26224 runs no row. That means no prompt press (0x80006EBC), no Start (0x80006CDC(1, 0)), no
    quick spell (0x800069DC) and no R row. [read verbs.md:238-247; the skip read here]
  - words windows' channels take no press either. [inferred: the same loop]
- It is one word for every channel and pad. It is not one pad. [read]

## 3. r13-26232: who counts it and what it locks

**Counted down** in 0x80006900, the play channel's +0 handler (set by 0x80006988: 0x8013DBE8(channel, 0x80006900),
at 0x800069A8-0x800069B4) [read 0x80006900-0x80006964]:
- only when r4 == 1, that is, when the channel runs (not held, and the focus or no focus);
- it takes 1 off r13-26228 too, if that is not 0;
- when r13-26232 is already 0 and r13-26256 is set (the script's ed223, "open the menu"), it calls 0x80006CDC(0, 1).
  This is the game's own "do it when the lock runs out" (verbs.md:326).

**Who steps it:** 0x80006A94 calls 0x8013E160(r13-26224) only from the world frame, at 0x800084F4. Its gate 0x800EBEE8
always answers 0 [read 0x80006A94-0x80006AC0; coop-level3-design.md:130]. So it counts world frames only. It does
not count in the other mode's frame (0x80050A68), where the tick still runs. [read: no other bl to 0x80006A94]

**What it stops:**
- 0x80006CDC (the Tome or menu, from Start or from ed223) returns at 0x80006D00 while it is not 0 [read
  0x80006CF8-0x80006D00].
- 0x80006CDC also returns while a fade is on (0x801E7C98, r13-21784 and the bytes from r13-26896), while the play hold
  r13-25696 (0x8004303C) is set, and on other gates [read 0x80006CF4-0x80006D94; verbs.md:324].
- One other reader: 0x800AA554, in a test reached from a player state at 0x80051C84. It answers 0 while locked, for a
  record of kind 13 at 0x80307170 [read the site; what it is for is unread].

## 4. Which player it locks in co-op

**Where each word lives** [read hle.c:11996-12007, 12279-12290]:
- r13-23592 is inside `pad_globals` (r13-23604 to -23548, at byte 12). Each player has a copy. coop_state swaps it with
  the input block 0x805880A0.
- r13-26232 is outside `globals` (-25680 to -25632) and outside `pad_globals`. **It is shared:** one word, never
  swapped.
- The play channel r13-26224 points into whatever input copy is in. In the world frame (0x800084F4) that is the
  game's [read coop-level3-design.md:130].

**Player 1's door (player 1 the mover):**
- fn66 runs in the game's script with player 1's state in. The lock goes into the game's copy, which is player 1's.
  [inferred from fn66's path; nothing swaps round it]
- The game's controller for player 1 reads that copy. Player 1 is locked as in solo.
- The main loop's 0x8013E518 counts it down. [read 0x80018708]
- Player 2 (the stayer) runs on their own copy in their turn (hle.c:20888-20890, 21082). Their copy has no lock.
  Their B, zones and quick spells are refused by the host's room_other rule instead (hle.c:20980-20982, 20849).

**Player 2's door (player 2 the mover), pressed at a prompt:**
- Their B starts the door's script by coop_use_start (hle.c:20430-20443) [read]. M2's log shows
  `[coop] script 119 runs for player 2 (frame 3760)`, then the ticket in the same frame
  (logs/run-20261112-180128-freeze-m2-p2-leaves/page-0001.log:496, 507) [log].
- coop_use_start swaps the player words (coop_be_player2: r13-26524, -21492, -21496, -21488, -21484 and the casting
  words), the kinds, the states, the zones and the cameras. **It does not swap coop_state** [read hle.c:18169-18200].
- So 0x800443E4(65) writes **the game's copy, which is player 1's.** It also writes the shared r13-26232.
  Player 2's own copy (g_coop_state[1]) gets no lock. [read the swap; inferred for the write]
- That is why player 1's controller "as the game's" stood them through player 2's OPEN DOOR. The host's room_turn
  gives player 1 a turn from g_coop_p1_free (their copy from before the ticket) with the hold opened (hle.c:20311-20352,
  19633-19643) [read].
- What keeps player 2 still is not the lock. It is state 121 (their motion; the M2 log shows p2st 121 from frame 3763
  to the firing, and 80 at 3826) and the host's door-turn window, COOP_DOOR_FADE = 120 frames (hle.c:21752, 21766)
  [log; read].
- A player 2 door from a zone walked into (their look, inside their turn) would put the lock into player 2's own copy,
  because their input copy is in then. [inferred; not traced]

**The stayer's presses (F4).** At player 2's door, player 1's game copy is locked for 65 frames. Player 1 plays only
through the host's room_turn. room_turn needs coop_room_change_on, and that is false while bits 8, 16 or 32 or a
cutscene are up (hle.c:11333-11341). [read; consequence inferred]

## 5. G2's wait against the lock

Timing in solo [inferred from the read order]:
- The ticket is written in frame t0 (lock 65, countdown 62).
- The tick at the top of each world frame counts +0 down. It fires at frame t0 + 62.
- 0x8013E518 at the end of each frame takes r13-23592 to 0 at about t0 + 64.
- So the lock covers the firing and a frame or two of the arrival.

With G2 skipping the tick for N frames, the firing moves to t0 + 62 + N. The locks do not move.

**Player 1 the mover:**
- From about t0 + 65, player 1's bindings come back. Their motion (25 for a door) is over by then. State 121 is left on
  event 6 [read mover-readers.md].
- They can walk, attack, reload and cast quick spells (0x800069DC, event 185) in the black.
- No prompt is on show to press (r13-24048 bit 0x10000 makes the world frame clear prompts every frame:
  0x800084D8-0x800084EC) [read].
- Start is still refused while the fade's state holds (0x801E7C98) or the play hold is up [read the gates; that the
  fade's state holds through black is inferred].
- A scene begun in the window (bit 32) may hold the game's walking channels (+16) on its own, as fn22 does. Then
  player 1 stands for it whatever the lock. [inferred]
- Solo never reaches this: an unlocked mover with a door ticket still counting.

**Player 2 the mover:**
- The lock never held them, so G2 changes nothing about the lock for them.
- What changes is that their motion and state 121 end before the firing. Their own unlocked copy then walks them,
  while coop_door_turns lasts (120 frames from the motion). After that, their turn is refused as busy
  (hle.c:20850). [read the gate; consequence inferred]
- The game's copy (player 1's) runs out of its lock at about t0 + 65 as well. During the wait, room_turn is off, so
  player 1 runs on the game's copy. They are held until about t0 + 65, then free on that copy. [inferred]

**The cinematic case.** In a cutscene frame (0x80050A68), the tick and 0x8013E518 run, but the play channel does not.
So r13-23592 runs out and r13-26232 does not. [read the call sites; inferred for a cutscene]

## 6. Renewing it: the game's own way

- **0x800443E4(n)** is the game's renewal. It is a max on both words, so calling it again can only keep or lengthen
  the lock. The ticket writer's own value is +0 + 3 (0x801557F0).
  - Calling `0x800443E4(ticket+0 + 3)` in each frame G2 holds the tick keeps the lock at what solo has when the
    countdown goes on. With +0 frozen, this is a constant. It can never lengthen the lock past what the countdown
    then needs.
  - It must be called with the copy the first lock went into. The tick runs at the top of the world frame
    (0x800083EC), with the game's copy in [read 0x800083C8-0x800083EC]. So a guest call from G2's intercept reaches
    the game's copy, which is player 1's.
- **0x8013E99C(n)** renews r13-23592 alone, and **0x800068E0(n)** renews r13-26232 alone [read].
- **0x8013E9B4** is the game's clear of r13-23592 (state 30, 0x80052224). G2 should never need it.

**Per mover:**
- **Player 1 the mover:** renew with 0x800443E4(+0 + 3) each waiting frame. This is exactly the solo lock, held.
  Check that player 1 stands in state 1 after the motion (K9: they leave 121 on event 6 during a long wait, as solo
  does in its last frames).
- **Player 2 the mover:** do not renew the game's copy. It is player 1's, the stayer's, and it would hold them through
  the whole wait.
  - If player 2 is to stand like a solo mover, the lock belongs in g_coop_state[1].pad_globals + 12 (their r13-23592).
    The way to do it is to swap their copy in and call 0x8013E99C(+0 + 3), or to write the max into the kept copy.
  - That is new for player 2. Today their door never locks them; state 121 and the 120-frame door-turn window do the
    job.
  - Also look at COOP_DOOR_FADE: a wait longer than 120 - 62 frames ends their turns before the firing.
  [inferred: design choice]
- **The shared r13-26232:** renewing it with 0x800068E0 holds the game's Start, which is player 1's (player 2's Start
  is the host's coop_menu_input). For player 2's door, leave it alone. [inferred]

## 7. Facts read

| Fact | Where | Mark |
|---|---|---|
| ticket writer: r30 = count (1 if <= 0), 0x800443E4(r30 + 3) | 0x8015578C-0x80155798, 0x801557F0-0x801557F4 | [read] |
| 0x800443E4 = 0x800068E0(n), 0x8013E99C(n) | 0x800443E4-0x80044414 | [read] |
| both setters are unsigned max | 0x800068E0-0x800068F4, 0x8013E99C-0x8013E9B0 | [read] |
| r13-23592 counted down per pad update | 0x8013E804-0x8013E81C in 0x8013E518 | [read] |
| 0x8013E518's callers: main loop, 0x801E4558 | 0x80018708, 0x801E4558 | [read] |
| a lock skips a channel's rows | 0x8013E210-0x8013E218 → 0x8013E344 | [read] |
| rows test the pad by channel +24 | 0x8013DC74-0x8013DC90 | [read] |
| walking channels stepped by the controller; their +0 clears the block | 0x80045D30, 0x80199F98-0x80199FC8, 0x80199FCC-0x8019A028 | [read] |
| r13-26232 counted down by the play channel's +0, r4 == 1 only | 0x80006900-0x80006964 | [read] |
| play channel made with handler 0x80006900; r13-26232 zeroed there | 0x80006988-0x800069D8 | [read] |
| play channel stepped only in the world frame | 0x800084F4 → 0x80006A94 → 0x8013E160(r13-26224) | [read] |
| 0x80006CDC refuses while r13-26232 != 0, a fade, or the play hold | 0x80006CF4-0x80006D14 | [read] |
| deferred menu: r13-26256 and the lock at 0 | 0x80006940-0x80006954; verbs.md:326 | [read] |
| other 0x800443E4 callers and values | 0x8011105C (5), 0x800C35F4 (motion length x2, < 600) | [read] |
| r13-23592 cleared by state 30 | 0x80052224 → 0x8013E9B4 | [read] |
| r13-23592 in pad_globals; r13-26232 in no copy | hle.c:11996-12007, 12279-12290 | [read] |
| coop_use_start swaps no coop_state | hle.c:18169-18200 | [read] |
| player 2's door script runs as a use, ticket in the same frame | M2 page-0001.log:494-508 | [log] |
| player 2 in state 121 from ticket to firing, then 80 | M2 both.tsv frames 3763-3826 | [log] |
| room_turn: player 1's turn from g_coop_p1_free in player 2's change | hle.c:20311-20352, 19633-19643 | [read] |
| player 2's door turns: 120 frames | hle.c:21752, 21766, 20850 | [read] |
| tick at the world frame's top, game's copy in | 0x800083C8-0x800083EC | [read] |
