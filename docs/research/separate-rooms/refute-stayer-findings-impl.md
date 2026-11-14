# Refute: design-stayer-findings.md (F2, F3) against the current code

Read only. Nothing was changed, built, launched or run. HEAD 5eb040d plus the uncommitted hle.c. Every line number and
address that design-stayer-findings.md cites was opened, along with f2-arrival-closeup-readers.md,
f3-fade-tome-copy-readers.md and the S9 log (logs/run-20261113-163201-stayer-s9-p1-zone/page-0001.log).

Tags: [read] seen at an address or line, [read there] in a named doc or log, [inferred] not seen.

## Verdict in short

- **F2's cause is right, and so is the shape of its fix.** The chain 0x80152134 -> 0x801172B0 -> 0x80116E70(2, 1) ->
  0x80024CDC(-2, 2) -> 0x80024B24(0) -> 0x80199EA0(1) -> 0x801556CC(2, 0) matches the code and the S9 log order: look-over
  at lr 0x80058E84, then busy r3 2 lr 0x80199F28, then R4b at lr 0x80152188 sees busy 4. F2a (skip the -2 let-go in the
  bracket) and F2b (don't read player 1's look in the bracket) are both needed. F2a alone still loses the look at
  0x80058E84, which runs before 0x80152134.
- The current code already intends this. `split_r4b_case` (hle.c 25415) ends G2's wait for "player 1's own scene or
  close-up" because R4b is meant to carry it over the arrival. F2 makes that true for a close-up.
- **F3's cause is right.** The copy's own fade comes from the memcpy at 15080 and is ticked by 0x80111790 in the copy.
  F3a's setters do what the design says.
- The problems found are below. One claim is wrong (A). Two details are wrong (B, C). Several paths are missed or
  over-claimed (D-I). Nothing found breaks solo, follow mode, ED_SPLIT_HOLD=0 or ED_STAYER_PRESSES=0: every new branch sits
  behind g_stay_bracket plus g_split.on (co-op stay only), or behind stayer_window and the switch.

## Points

### A. [real] F2b's "every answer in the bracket is exactly today's" is not true once F2a is in

- The design argues (F2.3, F2b) that with `bracket_p1` the coop_p1_close_up term is false without the call, so every ask
  answers as today. That holds only for asks made before 0x80152134. F2a changes the game's word after that point:
  - from 0x80152138 to 0x80152184: today the word is 4 (bit 2 cleared at lr 0x80199F28). With F2a it is 6.
  - after 0x80152188: today 0x801556CC(-1, 0) leaves 0. With F2a, R4b's stay_busy_clear writes keep = 6 & ~4 = 2
    (hle.c 26276-26286: guest-calls the clear, then `mem_write32(..., keep)`).
  - So any coop_busy_ask between there and coop_stay_change_end that falls through to "left to the game" (19999-20010)
    reads busy 2, where today it reads 0 (not busy).
- Which asks run there: the direct `bl 0x80155738` sites are 0x8000749C, 0x800271F0, ..., 0x80165570, 0x801F22A0,
  0x801F248C, 0x801F2514, 0x801F541C [read disasm.txt]. None of them is in 0x80151FDC, 0x80152298, 0x80152614 or 0x80153B6C.
  The callees after 0x80152188 (0x801F6828, 0x80174B84, 0x801E7C88, 0x801E4A80, the `later` 0x80153B6C) were not followed.
- R4b's own scene case already runs with a nonzero word kept there (busy 34), so the shape is not new [read 26276-26290].
- **Effect:** probably nothing. But the claim must not be relied on. Add a trace of every coop_busy_ask answer inside the
  bracket after lr 0x80152188 (lr, node, answer) to S9's run.

### B. [real] F2a's log reads the wrong look

- F2.5 says the F2a log prints "look kind K stage S", read at PROMPT_LOOKS + 32 * (+64 index).
- But the +64 index is written by 0x80024CDC itself, from the slot: slot 2 sets it to 0 (0x80024D1C-0x80024D28), slot 4
  sets it to 1 (0x80024D30-0x80024D3C), and other slots leave it [read `ed dis 0x80024CDC`]. F2a skips that function, so
  +64 is still whatever the last let-go or open wrote.
- A -2 in slot 4 with +64 = 0 would print look 0's kind.
- **Fix:** derive the index from r4 as the game would (2 -> 0, 4 -> 1, else +64). The skip itself is right. Leaving +64
  unwritten is what we want.

### C. [real] Today's S9 state is worse than "half a close-up", and F2.2's open question is answered

- What a press on a kind-0 look does was "not read" in both the readers and the design. It is now read:
  0x80024D78 adds 1 to the stage (0x80024DB0-0x80024DC0), then for kind 0 goes through 0x80024DD0, 0x80024DD8, 0x80024DDC
  and 0x80024DE0 to 0x80024DE4 `b 0x80025590`, which only returns [read `ed dis 0x80024D78`, `0x80025590`].
- So in today's S9, player 1's written-back DETAIL does nothing. Their set keeps record 10 pushed (Camera06) with no RETURN
  that could pop it. The camera stays stuck until some other close-up or cancel pops it. S9's 000037 (~4300) agrees.
- The kind-1 zone case (F2.2's "wider pre-existing case", S9z-off) is a dead EXAMINE: the press goes nowhere. The game asks
  slots 2 and 4 again only when the zone is left (hle.c 17233-17237), so it stays dead until the player leaves the zone.
- That case is reachable in **every** R4 door with ED_STAYER_PRESSES=0 or ED_SPLIT_HOLD=0: player 1 just stands in an
  EXAMINE zone at player 2's arrival.
  - F2a's reasoning ("inside player 2's change the game's slots are player 1's") holds for every non-lent R4 bracket,
    whatever the switches.
  - The gate on g_split.on and stayer_switch_allows(0) only keeps the off switches byte-for-byte. That is right under this
    workflow's rule. But the always-printed WARNING then marks a known dead prompt, not a guess. The user should be told
    that, not "observation only".

### D. [maybe] Missed path: a door whose hold spans its ticket lets go of player 1's carried-over close-up after the arrival

- F2 carries player 1's close-up over the arrival. With split_door_holds_play (script 132-style ladders, g_split.word0 & 2;
  hle.c 25059), the mover's own door script still has its fn21(1) to run after the arrival. That is 0x8016820C's
  0x80199EA0(1) (bl at 0x8016829C).
- coop_play_hold's script-step keep (hle.c 13215-13219) asks only `theirs ? coop_p1_scene_on() : ...`. It keeps the hold
  for player 1's **scene**, not for their close-up.
- So the door's let-go passes through. It lets go of the walking channels (r13-23060/-23052) and busy bit 2. Those are the
  same one hold that player 1's close-up rests on. The result is the same body-walks-in-a-close-up as F2, one step later.
- R4b also keeps the door's own bit 2 as if it were player 1's (keep = busy & ~4). Whose bit it is cannot be told apart
  there.
- Not read: whether that script step runs with player 1's channels in (player 2 apart, in their turn or not).
- F2.6 has no run of this shape. Add S15/S16 (design-stayer-presses' script-132 ladders) with player 1's EXAMINE in the
  window and DETAIL after the arrival.

### E. [maybe] F2b's `!coop_scene_on_of(0)` exclusion drops a close-up inside player 1's own scene

- A close-up can sit inside a scene of player 1's: "the circle: its scene, then DETAIL" (hle.c 13125-13126), and
  coop_p1_in_close_up is true there (19415-19425).
- For that case F2b leaves today's path, so the look is cleared at 0x80058E84. F2a still keeps the game's close-up (its -2
  slot). R4b keeps the bits (scene). stay_scene_look_again (17584-17594) then re-arms the look only as **kind 3**.
- With kind 3, the scene on and no words, coop_p1_in_close_up reads "scene only" and returns false. So after the arrival:
  - coop_player1_mid_use is false;
  - player 2's B is no longer kept behind player 1's look (21403);
  - player 2's way out no longer waits for it (16805);
  - all while the game's one close-up stepper is player 1's.
- Either drop the exclusion (F2b's term is safe for the scene case too: its answers are the other terms, as today) or keep
  g_coop_p1_look_kind when re-arming.

### F. [maybe] F2.4's "the lend-route mirror is unreachable at a firing" is too strong

- G2b's wait (hle.c 25443-25445) is ended at `count <= 1` ("its last count"). The wait also gives up after
  SPLIT_WAIT_LONGEST = 900 frames (24942, 25467-25473: "it goes on under it").
- So a lend-route screen of player 2's (g_coop_screen; set at 18327, 18444, 28915) begun at the last count, or one up longer
  than 900 frames, has player 1's arrival fire under it.
- Player 1's change has no bracket. 0x801172B0 zeroes the **lent** slots, and nothing writes them back. coop_close_up_kept
  (19643-19655) keeps only the cancel.
- After that, player 2's DETAIL/RETURN -2 is gone from their slots, and their lent close-up has no prompt to end it
  [inferred].
- This is not F2's fix, but the runs' "lent 0 at the firing" check cannot show it. A run with player 2's lever use (S5c)
  pressed at the last count would.

### G. [maybe] F2.2's "player 2's B and ways out wait for player 1's look" covers the press, not the ticket

- The keep is tested when player 2 presses (coop_player1_mid_use at 21403). A door script that waits before asking for its
  room ("script 132 about 145" ticks, hle.c 11419-11421) leaves frames between an accepted B and the ticket.
- Whether player 1's EXAMINE goes in during those frames was not read.
- F2c's flag is set by the window, not by when the look began, so the fix still covers it. Only the predictions for S6c
  and c3-p1-exam-before ("will show the keep") may be wrong for such doors.

### H. [maybe] F3's gate leaves out every fade that isn't the window's, and the host's Start paths never ask the fade

- The game refuses Start in any fade: `bl 0x801E7C98` at 0x80006CF4, with r3 != 0 -> 0x80006EA0 [read
  `ed dis 0x80006CDC`].
- The host's copy openings never ask 0x801E7C98: player 2's at hle.c 28225 and 28461, player 1's stayer Start at
  28150-28160.
- So a copy taken outside the window and its 12-frame tail still carries the real fade. Examples:
  - a door script's fade before its ticket;
  - a fade from the other player's scene or a cutscene's end;
  - the fade-in after a window that was refused (casting, ED_SPLIT_HOLD=0), where player 2's Start opens a copy once the
    count is out (28225 needs only `!coop_room_change_other`).
- F3a/F3b keep within the switches, which the rules here require. But add an always-printed line at every copy opening
  where 0x801E7C98 still answers 1 in the copy, so the remaining cases show up in logs.

### I. [maybe] R4's g_fade_mover is never forgotten after a door's fade-in (pre-existing; F3b and F3c lean on it)

- The forget (hle.c 3418-3419) runs only when a quad is drawn with ticks <= 0 and no change on.
- The tick's last step lands exactly on the target: live + (target - live) / ticks with ticks 1 (0x801E7EA0-0x801E7EBC),
  and ticks - 1 is stored at 0x801E7EA8. A fade-in then ends at alpha 0, and no quad is drawn (0x801E7ECC-0x801E7ED4)
  [read].
- Nothing else writes it but 11434 (the next ticket), and no state load resets it [read: grep g_fade_mover]. So after every
  stay door, g_fade_mover stays set.
- A later fade with ticks > 0 and no room change (a cutscene's, a script's fn112) is then marked out of the old stayer's
  half while it ticks (3420-3424). It cuts to black only when the fade holds at full [inferred].
- For F3: F3b's helper must keep its frame bound (fade_tail_frame + 12) and its ticket count. It must never be widened to
  "g_fade_mover set". F3c's guard is unaffected.

### J. Checked and holding (no change needed)

- 0x80024CDC acts on -2 only and calls 0x80024B24(0) for any -2 slot. 0x80116E70 zeroes the slot after the callback
  whether or not it ran, so skipping the callback leaves the game's slot reset intact [read].
- Inside the bracket r13-21492 is player 2 (S9 `who 59`). So today's coop_close_up_kept returns at its first test, and a
  new first branch is needed [read 19645-19647].
- The bracket saves the slots and writes them back but swaps neither slots nor looks (14149, 14161) [read].
- The busy set broadcasts event 102 only when the new word is nonzero (0x801556F0-0x80155720) [read]. So with F2a:
  - the S9 102 to room 180 (from busy_set(2, 0) leaving 4) goes away;
  - stay_busy_clear's guest-called clear to 0 sends none;
  - the "written, not set" keep sends none.
  That is the same as solo's arrival.
- split_over at the tail (14606) runs inside the bracket, before coop_stay_change_end. So F2c must be taken at the open, as
  the design says. g_split.on is also cleared by split_ticket_open and split_state_load, not "only" at the tail. That is
  harmless here.
- Refusing "count the bracket as room_bit" is right. split_busy_kept is false once the count is out (needs split_counting),
  so a held look would answer 0 to the arrival's 0x80058E84 asks, which today get the game's word [read 19983-20010,
  25172-25185].
- hle_coop_controller (the other coop_p1_close_up caller, 20645) has no gate line between 980 and 1022 in S9 [read there].
- F3: the tick draws only on the live alpha at 0x80602A38 (0x801E7ECC-0x801E7ED4). 0x801E7D24 then 0x801E7C88(&0, 0) clear
  everything the tick and 0x801E7C98 read [read]. 0x801E7C40 leaves ticks and target, and its save (0x801E7B88) would
  overwrite the saved fade [read]. The world's restore 0x801E7BE4 runs only at 0x80008578 (gated on r13-21932) and 0x80050BBC,
  never in a copy turn [read].
- TW_MODE_CHANGE 0x80018728 calls no fade start directly [read `ed dis 0x80018728`].
- tome_copy_close takes back no fade state [read 14906-14950].
- The stayer_window read for F3a must come before tome_copy_swap: g_tome_copy_turn is set only at 15174, after the opening
  block, so after the swap split_counting would read the copy's busy 0 and ticket. The design has it before the swap
  [read 15084, 15122-15123, 15174].
- The task text's "the mirror S8 draws unfaded" is wrong, as the readers measured: it dims 54.7 -> 34.0 and closes before
  black. The design already says so.

### K. [maybe] Low: F2a keeps player 1's game-side hide-others across the arrival

- Player 1's close-up runs the game's own 0x800255A4. The host passes player 1's calls through (18593-18604). It marks
  every seen character node of the room but r13-21492 (player 1 at the open).
- Today the cancel's 0x8002576C at 0x80024C10 unfreezes them at the arrival. With F2a they stay marked until player 1's
  RETURN, which is the close-up going on as solo would.
- If player 2's node was seen by the close-up's camera at the open, it would stay frozen in its new room until then. But
  that freeze already applies to any shared-room close-up of player 1's [inferred; not read whether player 2's node can
  pass 0x8014ED1C's 0x8002790C test].
