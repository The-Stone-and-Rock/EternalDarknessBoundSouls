# Refute: design-stayer-findings.md (F2, F3) against the goal

The goal (the user): co-op "truly built by design". The player who stays keeps every action through the other's door
(spells, examine and pick up, zone looks, Start and the Tome), together and apart, in both directions, with no freeze and
no black for either player, and solo unchanged. Constraints for this review: co-op only. Solo, follow mode,
ED_SPLIT_HOLD=0 and ED_STAYER_PRESSES=0 stay as they are.

This was read only. No code was changed, nothing was built and the game was not run. Sources: runtime/host/hle.c (working
tree, 31,004 lines), `ed dis`, `ed fn`, build/ed-db/disasm.txt, logs/run-20261113-16*-stayer-* (page-0001.log, both.tsv),
the two reader docs and stayer-presses-impl-notes.md.

Tags:
- [read]: seen at the address or line given.
- [read there]: seen in the named log or doc.
- [inferred]: reasoned, not seen.
- [real]: the fault is in the code or the design as read.
- [maybe]: a run or one more read would settle it.

## 0. Verdict

**F2.** F2a, F2b and F2c do what the design says for the case it was built for: player 1's close-up at stage 1 or 3 across
player 2's arrival, in a window, with the presses allowed. Its reads check out (§3). Player 2 staying (the mirror) needs
nothing, and S4, S1 and S5b show that.

**F3.** F3a and F3c are right for the stayer's copy, both players' copies, before the firing and across it.

The design still does not reach the goal in four places, and three more need a run:

1. **[real]** Player 1's Start in the 10 frames after player 2's firing is eaten. Player 2's mirror opens a copy (§1.1).
2. **[real]** R4's fade mark is dropped at ticket+60 whenever the stayer has their own scene, find or words bit up. The
   stayer's half then goes black and fades in with the mover's door. F3c guards only the copy's turn (§1.2).
3. **[maybe, criterion real]** Player 2's own Tome copy opened in their own door goes black at ticket+60 and stays black
   across the firing until B. The design's pass line for this ("fades as today") accepts that. Player 1 in the same place
   is refused by the game, as in solo (§1.3).
4. **[maybe]** F2's gate leaves out windows refused with the presses on (a summon charging, a cutscene at the ticket). A
   zone EXAMINE of the stayer's is then killed by the arrival. A press on a kind-0 look does nothing, now read (§1.4).
5. **[maybe]** F2b's "every answer in the bracket is exactly today's" holds only before 0x80152134 (§1.5).
6. **[maybe]** The stayer's copy is closed by a mover's arrival cutscene. The runs miss this, and the rejoin shape too
   (§1.6, §1.7).

## 1. Points

### 1.1 Player 1's Start right after player 2's firing is eaten [real]

**What happens.**
- Player 1's copy route, stayer_p1_presses, returns unless `stayer_window(cpu, 0)` is true [read hle.c 28127].
- stayer_window rests on split_counting, which needs COOP_ROOM_TICKET > 0 [read 25006-25010]. So from the firing on, player
  1's Start goes to the game's 0x80006CDC.
- That gate refuses Start while 0x801E7C98 says a fade is on [read: `ed fn 0x801E7C98`, whose one bl site is 0x80006CF4].
- The arrival starts a 10-tick fade-in at 0x801521A8 (`0x801E7C88(.., 10)`) [read `ed dis 0x80152134`]. In S9 both.tsv the
  fade is 9 down to 0 over 3827-3836 [read there].
- L1's Start lock is also put back to 2 at the tail (S9: `the Start lock put back to 2 (was 3)`) [read there].
- So a Start in about 3823-3836 is refused and lost. Player 1's half shows no fade then, because R4 marks the quad out of it.
  The user sees a dead button.

**The mirror.** Player 2's Start in the same frames opens a copy: start_room is false once the count is out [read
28461-28472]. F3b then clears that copy's fade-in. The design says so itself: "this half is player 2's in practice"
(design F3.2). Against the goal, it is a gap.

**Fix.** Add `stayer_fade_tail(cpu, 0)` beside `stayer_window(cpu, 0)` for stayer_p1_presses' **Start** branch only. The B
branch's M11 keep is for the window. The copy then opens, and F3a/F3b clear its fade.

Alternatively, keep the Start and press it for them once 0x801E7C98 answers 0. That is the coop_press_keep shape. With
that, the game's own Start lock (r13-26232) must also be 0 when it goes in.

**Run.** S8t: S8's line with player 1's Start at about 3826 (just after player 2's firing at 3824). Expect a
`[stayer] player 1's Start` line and the F3a line naming the fade-in.

### 1.2 R4's fade mark is dropped under the stayer's own 8/16/32, so the stayer's half goes black at the end [real]

**What happens.**
- Here is R4's forget test in notify_GXLoadPosMtxImm [read hle.c 3417-3419]:
  ```c
  if (!coop_room_change_on(cpu) && (s32)mem_read32(cpu, cpu->gpr[13] - 21784u) <= 0) {
      g_fade_mover = 0;
  ```
- coop_room_change_on needs the busy word to hold nothing but 4, 2 and 1 [read 11401-11406]. The implementation notes say the
  same: "coop_room_change_on is false under 8/16/32" (impl-notes D3/S15) [read there]. B2 made split_on allow the stayer's
  own bits (g_stayer.bits, 25016-25024), but this test was not changed [read].
- The fade's ticks run out about 2 frames before the firing. S9 both.tsv: tickets 3, 2, 1 at 3820, 3821, 3822 with fade 0
  [read there]. With ticks 0 and alpha 255 the quad is still drawn every frame (readers §1).
- So a stayer with their own scene (32), find (16) or busy-8 words up across ticket+60 has the mover forgotten there. The
  last black frames and the whole 10-tick fade-in are then drawn into **the stayer's** half, unmarked: about 12 frames of
  black, then the fade back in. That is the thing R4 was built to prevent.
- Both directions are affected. For a player 1 stayer this means a pickup's find or a lever's scene that lasts past the
  door's end.
- F3b is hit as well. Its `g_fade_mover == ...` test fails after the drop, so a tail copy would carry the fade again.

**Not seen.** All 24 runs in logs/run-20261113-16*-stayer-* print `bits 0x0` on every `[stayer] frame` line [read there].
No run had a stayer bit up at the end.

**Fix.** Make the forget test the window's own rule, not a "nothing else is busy" rule. For example:
```c
!(coop_room_change_on(cpu) || (g_split.on && (s32)mem_read32(cpu, COOP_ROOM_TICKET) > 0))
```
- This forgets the mover only once the door is over and the ticks are spent.
- g_split.on is co-op-stay only, so follow mode and solo are untouched.
- With ED_SPLIT_HOLD=0 nothing changes, because g_split.on is false.

**Run.** S6f: player 1 picks up an item (S6b's presses) timed so the find (busy 16) is up at ticket+60 (about 3820). Expect
`bits 0x10` on the window lines. Shots 3820-3836: the left half is never dark.

### 1.3 The mover's own copy goes black across their firing [maybe; the design's pass line is real]

**What happens.**
- start_room refuses player 2's Start only in **player 1's** change: `coop_room_change_other(cpu, true)` is
  `g_coop_room_p2 != true` [read 11409, 28462].
- In player 2's own door countdown, start_pressed is therefore true, and in mode 2 it asks for a copy (28549-28558) [read].
  The only refusal there is coop_p2_own_up.
- The copy is taken mid-fade. It reaches black at ticket+60 and never runs the arrival's fade-in (readers §5). So player 2's
  half is a black Tome from about ticket+60 until they press B blind. Meanwhile their body has changed room behind it.
- F3a leaves this copy alone on purpose (F3.3, "the gate is the owner's stayer window"). S3m's pass line is "Player 2's half
  fades as today", which accepts the black.
- In solo, and for player 1 as the mover, the game refuses Start while the fade is on (0x80006CF4) [read]. So the two
  players differ in the same place, and player 2's half shows black.

**Not settled.** Right after player 2's door B, `closeup` is true for COOP_ASSIST_WINDOW reads (g_coop_assist_at, busy 4).
coop_menu_input's play branch, where start_pressed is used, then isn't reached [read 28474; inferred from refute-goal §1.1].
A Start later in the countdown is not refused by anything read. No run has pressed it.

**Fix.** Do what the game does. In coop_menu_input, under `coop_stay() && g_split.on && g_split.mover == 2` (or
`g_fade_mover == 2`), refuse player 2's Start while 0x801E7C98 answers 1 in real RAM, as 0x80006CF4 does for player 1. A
Start refused like this should be logged. S3m then expects the refusal line and no copy.

### 1.4 F2's gate leaves the stayer's zone EXAMINE dead in refused windows [maybe]

**Read here** (the F2 readers list this as not read): a press on a kind-0 look does nothing.
- 0x80024D78 adds 1 to the look's +8 at 0x80024DBC-0x80024DC0.
- It then reads the kind (0x80024DCC). Kind 0 takes `cmpwi 1; bc 4,0` as not-taken and goes `b 0x80025590`, which only
  returns [read `ed dis 0x80024D78`, `0x80025590`].
- So the EXAMINE written back over a cancelled look is drawn but dead [read]. Presumably it stays dead until the zone is
  left and entered again [inferred].

**The gap.**
- F2c needs `split_on_mover(2)`, that is g_split.on.
- With the presses on, a window is still refused for the stayer charging a summon (G3, narrowed to summon-class) or for a
  cutscene at the ticket [read split_ticket_open 25296-25300].
- R4's bracket still runs then (notify_ticket_run 14143 needs no window) [read], so 0x801172B0 cancels player 1's kind-1 zone
  look as in S9.
- Whether a zone's -2 slot stays filled while player 1 charges a spell has not been read.

**Fix.** Take F2c from the bracket and the presses switch, not from the window:
```c
coop_stay() && g_split_ready && split_switch() != 0 && stayer_switch_allows(0) && !coop_cameras_lent()
```
ED_SPLIT_HOLD=0 and ED_STAYER_PRESSES=0 then stay as today. The always-printed WARNING (F2.5) shows which case it is.

**Run.** S9c: a summon charging in room 183's EXAMINE zone at player 2's ticket.

### 1.5 F2b's "every answer in the bracket is exactly today's" holds only up to the let-go [maybe]

- F2a leaves busy 6 in the word from 0x80152138 to 0x80152184, where today it is 4. R4b then writes 2, where today it is 0
  (stay_busy_clear 26276-26290) [read].
- So asks after 0x80152134 inside the bracket see bit 2 and, with F2b's term off, are left to the game's 2. Today those asks
  saw 4 or 0. The arrival code between them is the waits clear, 0x80153998 and 0x801E0170/01D8/055C, the hold
  (0x801F6828), 0x80174B84 and the runner's tail [read `ed dis 0x80152134`]. No ask is listed there in S9 (lines 1001-1022)
  [read there], but the gate lines are thinned.
- **After the bracket, busy 2 does no harm.** In S6, player 1's RETURN set busy 2 from 3831 to 3880 while player 2 was in
  their arrival walk (state 80). They went to state 81 at 3887 and to 1 at 3910, the same frames as S9 with busy 0 [read
  there: both.tsv]. coop_busy_ask's coop_p1_close_up term answers 0 for everyone then [read 19999].
- **To settle it:** F2b's own trace line should count the asks after the let-go and give their lr. In S9 after the fix,
  compare player 2's state 80/81/1 frames with today's.

### 1.6 A mover's arrival cutscene closes the stayer's copy [maybe]

- tome_copy_step closes any open copy when `world_stays` fails, and `!cutscene_playing(cpu)` is part of world_stays [read
  15030-15040].
- A room whose arrival starts a cinematic (a first visit) would therefore shut the stayer's Tome at the mover's firing: "the
  world asks for something else".
- The mode words themselves seem to hold across a firing: the follow-mode run kept a copy open through one [read there:
  hle.c 28456-28458].
- S3h and S8h use rooms 183/180, which have no arrival scene, so they cannot show this.
- **Run:** a door into a room with an arrival scene, the stayer's copy open at the firing. Whether R6's "scenes beside the
  other player" makes cutscene_playing false in the world turn has not been read.

### 1.7 Shapes the runs miss [maybe]

- **Rejoin** (apart to together): player 2's door **into** room 183 while player 1 is in a close-up begun in the window.
  - It is the same bracket, so F2a and F2b apply [inferred].
  - coop_zones_new then builds player 2's list for room 183, with the zone player 1 is using. Player 2's B on it is kept by
    coop_player1_mid_use (21403) [read]. Player 1's hide-others (0x800255A4 marks at the open) never marked player 2's node.
  - Not in S11d (room 344). Add a run.
- **Stage 3** (RETURN up). After the words, S6 shows the look armed again (`close-up holds them alone` at 3830, `busy r3 2 r4
  1`) [read there]. So F2b's `coop_p1_in_close_up` gate is true at stage 3 and F2 covers it. S6r stays the check.
- **A one-frame prompt gap.** 0x80117050 frees the prompt element in the bracket, and the bracket's end restores only the
  slots, not r13-24032 or r13-29016 [read F2.3, 14161]. S9 shows DETAIL drawn again later (000037) [read there]. Whether it
  is missing for a frame at the firing is unknown. Look at S9's shots after the fix.

## 2. The case the task asked about: a close-up begun BEFORE player 2's ticket

The design is right, and this is a gap today only for tickets that come through neither of the keeps.
- Player 2's B is kept while player 1's look is up (coop_player1_mid_use, 21403-21404) [read F2.2].
- Player 2's ways out wait on coop_way_out_p1_up [read F2.2].
- 0x80117360(0) in the window zeroes only the shown record, so the -2 slot is still there at the arrival [read there:
  readers §6].
- F2c is set by the window, not by when the look began, so the fix covers this case as well [read the design].
- A ticket that goes through neither keep (a script's teleport) reaches it only with a window. Under the refused windows of
  §1.4 it is still cancelled.

## 3. What checks out

- **The chain and its parts.** The arrival's 0x801172B0 at 0x80152134, lr 0x80152138, and `flags & 4` at 0x80152140 [read].
  0x80024CDC acts on -2 only and cancels through 0x80024B24(0) [read there, and the readers' bl list].
- **The let-go intercept.** coop_close_up_kept is the 0x80024CDC intercept and today keeps only on g_coop_screen with player
  1 current [read 19643-19655]. F2a's first branch has the same shape.
- **Why the look is read over.** The `look over ... lr 0x80058E84` is coop_busy_ask's term (19999), the only busy-ask caller
  of coop_p1_close_up. The other caller is the controller (20645), which doesn't run in the bracket [read; S9 980-1022 has no
  controller line].
- **What the kept close-up keeps.** Without the cancel, room 183's others stay frozen and player 1 stays hidden until the
  game's own end runs 0x8002576C. That is the close-up as it is in solo [read 18593-18604, readers §1].
- **F3a's place and gate.** The binding-lock block at 15084-15094 runs, with stayer_window, for both copies (S3 line 307,
  S8 line 318) [read there]. F3a goes beside it, and the copy's turn has not begun yet.
- **The fade gate.** Only Start is gated by the fade: 0x801E7C98 has one caller [read]. Quick spells, B and looks are not
  refused by any fade.
- **The world view during a copy.** game_menu_open in the world turn is false for a stand-in copy (`!tw_world_turn()`, 3838;
  world_tick set at the stand-in, 15050) [read]. So in the world turn R4's test reads coop_room_change_on as the door
  intends, except under §1.2's bits.

## 4. Summary for the build

| # | Point | Tag | Change |
|---|---|---|---|
| 1 | Player 1's Start eaten in player 2's fade-in tail | real | stayer_p1_presses Start: `stayer_window(cpu,0) \|\| stayer_fade_tail(cpu,0)`, or keep the press until 0x801E7C98 and the Start lock are 0 |
| 2 | R4's mark dropped under the stayer's own bits: about 12 frames of black in the stayer's half | real | forget only when `!coop_room_change_on && !(g_split.on && ticket > 0) && ticks <= 0` |
| 3 | Player 2 as the mover: their own copy black across the firing; S3m's pass line accepts it | maybe (design real) | refuse player 2's Start in their own door while 0x801E7C98 is on, as 0x80006CF4 |
| 4 | Dead zone EXAMINE in refused windows, presses on; a kind-0 press returns (read) | maybe | F2c from the bracket and the switch, not g_split.on |
| 5 | Bracket asks after the let-go see 6, then 2 | maybe | F2b trace counts them; compare player 2's state 80/81 frames |
| 6 | An arrival cutscene closes the stayer's copy | maybe | a run with a scene room |
| 7 | Rejoin, stage 3, the prompt element's frame | maybe | runs S11r, S6r, S9's shots |
