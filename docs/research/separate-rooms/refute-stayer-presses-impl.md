# Refuting design-stayer-presses.md against the code

This checks each point of design-stayer-presses.md against runtime/host/hle.c (working tree, uncommitted, 30,141 lines),
build/ed-db/disasm.txt and the logs. Nothing was changed or built, and the game was not run.

Marks:
- [read] = seen at the address or hle.c line given. [log] = seen in a run log. [inferred] = reasoned, not seen.
- [real] = a defect the code shows. [maybe] = a risk the code allows but no run has shown.
- The default is doubt.

Words: the **mover** takes the door. The **stayer** stays. **The window** runs from the ticket to the change's tail.

## 0. In short

The design reads the game right on its main points. Its four roots are confirmed: L1's lock, P1's prompt flag, the host's
follow-mode refusals, and P4's busy-4 refusal. But eight changes clash with rules that already exist:

1. **P3 opens coop_busy_ask's blanket "not busy" inside the window** (cases b and d come before split_busy_allows). P3b's
   carve-out misses the mover in the split shape, the message pump, kind 19 and K7 nodes, and every other asker. [real]
2. **P3's mirror in hle_coop_controller is not guarded.** p2_hold gives player 1, the mover, a turn with the hold let go.
   [real]
3. **The busy word's bit 2 is one bit.** At doors that hold play (script 132's climb, a door that asks first), P4's
   let-go and the end of the stayer's look both let go of the door's own hold. [real for those doors]
4. **P1's chooser branch, placed first, cuts out player 1's look branch.** Its "less any bit set at the ticket" also drops
   the bit 4 it means to mask. [real]
5. **Player 2's lend-route uses in the window are taken for a close-up of theirs**, because busy 4 alone makes
   coop_busy true. The screen, the pad and the cameras all go to player 2 during player 1's door. [real]
6. **Lifting player 2's Start refusal without the copy** makes player 2 the game's current player during player 1's
   countdown. This happens with ED_COOP_TOME_COPY 0 or 1 only. [real, outside the default]
7. **The kept-B route already in place doubles player 1's B** at doors where player 2's script hold is up. [real for
   those doors]
8. **None of the runs uses a door that holds play.** The door 183 -> 180 has busy 0x4 alone through its countdown
   [log]. So the runs cannot show items 3, 4 or 7.

The rest (§3) are [maybe]s and small slips. §4 lists what was checked and holds.

## 1. The ordering asked about: coop_busy_ask, split_busy_allows, the P-changes

The order today (hle.c:19717-19740) [read]:
1. split_busy_kept: K1 (the mover's own filter, lr 0x80058E84 with r31 the mover) and the K7 sites. These keep the game's
   word.
2. Today's cases, which answer 0 to **every** caller:
   - (a) g_coop_screen with words or busy;
   - (b) coop_p1_close_up;
   - the p1_free case (busy == 1 exactly);
   - (d) coop_p2_holds.
3. split_busy_allows (A1-A3, only under split_on).
4. The game's word.

Today, (b) and (d) never fire in the window:
- At player 2's door, coop_p1_close_up's room bit is set only for player 1's own door (hle.c:19308). So busy 4 | 2 reads
  "over", and g_coop_p1_look is cleared (19313-19318) [read].
- coop_p2_holds's room bit is set only for player 2's own door (19670) [read].

So the split hold's allow list is all that answers in the window. That is the design-split-hold B1 decision: "S1 became
an allow list".

P3 gives each look the other player's room bit. From then on, (b) or (d) answers 0 to every asker before the allow list is
reached. The problems this causes are in §2.1 and §2.2.

## 2. Problems

### 2.1 P3 and P3b: the blanket "not busy" comes back for the whole window [real]

**What is wrong.** With P3, case (b) answers 0 to everything while player 1's look is up in player 2's door. Case (d) does
the same for player 2's hold in player 1's door. P3b keeps the game's word only for "the A1 lr list, for a node outside
g_split.room or roomless". That misses four things:
- **The mover in the split shape.** The mover is inside g_split.room. A1 shuts them out by name: split_node_freed
  returns false for `node == split_mover_node` (hle.c:24634-24636) [read].
- **A2, the message pump (lr 0x801F5420).** The allow list never frees the mover, another room, or event 75 here
  (24718-24721) [read]. P3b only covers A1's lrs.
- **Kind 19 and K7 nodes inside the room.** A1 leaves them out (split_kind_kept, 24626-24629) [read]. P3b answers 0 for
  them.
- **Every other busy asker** (35 sites, presses-gates §0.1). The allow list was built because unknown askers can write a
  second ticket (design-split-hold B1: kind 39's capture, kind 50's timer) [read there]. K7 names only the sites already
  read.

**Failure.**
- Player 1 staying, in their own close-up through player 2's door: the mover's node update and motion steps (split
  shape), its delayed messages, event 75, grabbing kind 19 nodes and unread askers all get "not busy" for the rest of the
  window.
- Player 2 staying with a script hold of theirs through player 1's door (case d): the same, with player 1 the mover.

**Fix.**
- In coop_busy_ask, when split_counting(cpu), skip cases (a), (b), p1_free and (d). Answer only through
  split_busy_allows, with P5's wider split_on.
- The stayer's own look needs no blanket. In solo a look's scripts run with busy 2 up.
- Case (b) exists so that the other player plays on. In the window the other is the mover, and the game already runs the
  mover (fn66 clears their held bit; coop_node_held_ask) [read 11335-11340, 19747-19767].
- The stayer's own node passes A1, so their controller and player 2's turn inside it still run.

### 2.2 P3's mirror: p2_hold gives the mover a turn with the hold let go [real]

**What is wrong.** P3b guards only the p1_look hold-open (hle.c:20310-20321). With P3, coop_p2_holds is true in player
1's door whenever player 2 has a script hold or scene (19666-19671). Then hle_coop_controller does two things
(20324-20333) [read]:
- It runs coop_hold_open and coop_player1_turn for player 1, the mover. coop_hold_open writes busy 0, r13-25696 0, zones
  r13-23040 1 and r13-24048 without bit 4 (19796-19811) [read].
- It sets `g_hle_passthrough = !p2_hold && !room_turn` (20379), which is false. So the game's own controller for the
  mover is replaced.

**Failure.** Player 2 staying, in a hold of theirs, during player 1's door:
- Player 1 runs their controller with busy 0 and zones on in their own door motion.
- Their input is the game copy, and the binding lock keeps it mostly still [read b3 §1].
- The zones word is on for their next character step [inferred].

**Fix.** `p2_hold = !p1_look && coop_p2_holds(cpu) && !stayer_window(cpu, 1)` for this branch. Use P3's room bit only
for reading the look "over" (R4b).

### 2.3 Bit 2 is one bit: P4's let-go and the stayer's look end let go of the door's own hold [real for doors that hold]

**What is wrong.**
- P4 makes coop_script_hold_go pass under busy 4. When bit 2 is up it then lets go:
  - COOP_PLAY_HOLD(1), which is 0x80199EA0(1): busy 2 cleared and the walking channels' +16 rewritten
    (0x80199EA0-0x80199F34);
  - r13-25696 := 0, r13-23040 := 1, r13-24048 &= ~4 (hle.c:19352-19363) [read].
- At a door whose script holds play before fn66, that bit 2 is the door's. Examples: "script 132's hold, then its
  ticket" (19661-19662); "a door that asks first" (19302-19306) [read].
- The stayer's look has the same problem when it ends. Its 0x80199EA0(1) clears bit 2 whoever set it.
  coop-failed-attempts.md says the same about the use route: "the use's end clears hold bit 2 ... which is player 1's
  close-up's hold too" [read there].

**Failure.**
- Player 1 staying (player 2's door): player 1's no-hold words let go of player 2's door hold in the game's memory. That
  means busy 2, the play-hold word and the zones word.
- Player 2 staying (player 1's door): player 2's lend-route words or find (coop_their_close_up with room_ok) let go of
  player 1's hold, the mover's. The mover's zones come on mid-door. This is the shape of failed-attempts p1scene: a way
  out taken mid-hold [read there, line 514].

**Fix.**
- With room_ok, return true without the let-go when the bit 2 is the door's: `(g_split.word0 & 2u) != 0u`, or
  g_coop_script_hold is player 2's door hold.
- At player 2's door the door's hold already holds the game copy's channels, which are player 1's, so they stand in
  their words.
- For the end of the stayer's look, a per-player hold bit is the real fix. coop-todo.md's "Half" row is open for that
  [read there].
- Until then, test only doors without a hold, and log `busy & 2` at the ticket.

### 2.4 P1: the chooser branch pre-empts player 1's look, and bit 4 is masked out of the mask [real]

**What is wrong, part 1.**
- The design puts its chooser branch first, and with it the call (running = true). P3 keeps g_coop_p1_look true through
  the window. So player 1's look branch (hle.c:19927-19944) never runs in the window.
- That branch opens the look's channel (`look_channel + 16 := 0x0FFFFFFF`) round the call and puts the busy word back.
- The chooser does test that channel. For slots 1, 3 and 5 it asks 0x80199F50 and 0x80199F74 (0x801176CC-0x80117704), and
  0x8013DBD8 reads +16 [read].
- The branch's own comment gives the failure it was made for: "the pick up showed before its scene ended, and B skipped
  it" (19926).

**What is wrong, part 2.**
- door_bits is "0x10000, plus 4 while g_coop_script_hold, less any bit already set at the ticket".
- At the doors where bit 4 matters, the hold comes first and then the ticket (19661-19662). So bit 4 is in prompts0 and is
  taken out of the mask again.

**Failure.** Player 1 staying:
- in their own close-up in the window, their DETAIL, RETURN and PICK UP prompts are chosen without the look's channel;
- at a ladder or ask-first door of player 2's, no prompt of theirs is ever shown.

**Fix.**
- One wrapper. When stayer_window(cpu, 0), clear door_bits in r13-24048, run whatever today's code runs (the look branch,
  or the plain passthrough as a guest call), then put the bits back.
- `door_bits = (0x10000 & ~prompts0) | (g_coop_script_hold && g_split.mover == 2 ? 4u : 0u)`. Do not mask the 4 by
  prompts0.

### 2.5 Player 2's lend-route uses in the window become "their close-up": the screen, the pad and the cameras go to them [real]

**What is wrong.** The design runs player 2's non-exit script prompts in the window (§2.4: "Other uses run at once").
Three things then make each one a close-up of theirs, by busy 4 alone:
- coop_use_start sets `g_coop_assist_at = 1u` for any use that is not a close use (hle.c:18212-18213) [read].
- coop_menu_input computes `closeup = ... (g_coop_assist_at != 0u && !g_coop_script_hold)) && coop_busy(cpu) &&
  !coop_p1_in_close_up(cpu)` (27579). coop_busy is "busy != 0" (3974), so busy 4 alone makes it true [read].
- continue_coop_use computes `busy_new = coop_busy(cpu)` for a use that is not a close use (18274). With no new ticket,
  that sets g_coop_screen and coop_hold_player1() (18275-18277) [read].

**Failure.** Player 2 staying, B on a lever or another script prompt in player 1's door:
- owner becomes 1 and the game's pad becomes player 2's (`*pad = p2`, 27831). That is the mover's pad in their own
  countdown.
- g_coop_screen stays up until busy is 0. All that time coop_busy_ask's case (a) answers 0 to every asker (§2.1).
- The design's G2b would then hold player 1 in the black up to SPLIT_WAIT_LONGEST.
- The cameras stay lent into the arrival.

Player 2's own words, pick-up and find scripts also run as uses (20414-20420). They are close uses, so they are not hit
[read 18213, 18274].

**Fix.**
- In the window, keep player 2's non-exit script presses as kept presses, the same way as exits (§2.5 of the design).
  This also fits "R4d is not built".
- Or, under split_counting, take COOP_BUSY_ROOM out of the three tests above.

The design's §5 row ("runs; player 1's change waits for it") understates this.

### 2.6 Player 2's Start, and their Tome-asking quick spell, without the copy [real, ED_COOP_TOME_COPY 0 or 1 only]

**What is wrong.** Lifting start_room (hle.c:27647) also lets start_pressed reach the path that does not use the copy,
`start_wait = COOP_START_WAIT; owner = 1; coop_tome_player(cpu, true)` (27737-27742) [read].
- coop_tome_player(true) makes r13-26524 and r13-21496 player 2's until "play goes on" (27050-27066) [read]. With
  COOP_START_WAIT that is up to 30 reads (26408).
- The Start it presses into the game's pad is refused by the shared Start lock (0x80006D00) [read there: b3 §3].
- The same happens when player 2's quick spell asks for the Tome: g_coop_quick_tome (12528) leads to owner 1 and
  coop_tome_player(true) (27533-27539) [read].

**Failure.** Player 2 staying, Start in player 1's countdown, with the copy off:
- no Tome opens;
- the game's current player is player 2 while the tick can fire player 1's change [inferred];
- the quick spell's cast is left asking for a Tome. The host's own note says state 64 refuses every later cast
  (14907-14922) [read].

**Fix.** Lift start_room, and the quick cast's Tome ask, only with tome_copy_mode() == 2. Otherwise refuse and log it, as
§2.7 already does for player 1. The default mode is 2 (hle.c:3959) [read].

### 2.7 The kept-B route already in place doubles player 1's B at doors with player 2's script hold [real for those doors]

**What is wrong.**
- coop_p1_press_kept keeps player 1's B on any prompt of their own while coop_p2_scene_or_hold is true (27322-27329).
- That test is true for g_coop_script_hold, lent cameras, or player 2's use being active (27293-27297) [read].
- g_coop_script_hold is cleared only once busy is 0 (27573) [read], so it stays true through the window.
- With L1 and P1, the play channel also presses the same B at once.
- After the window the kept one is let go (27352-27370) and pressed again by g_coop_p1_b_press.

**Failure.** Player 1 staying at player 2's ladder or ask-first door: EXAMINE opens, and after RETURN and the change it
opens again by itself.

**Fix.** Under stayer_window(cpu, 0), leave the mover's door hold out of `up` in coop_p1_press_kept (g_coop_script_hold,
and the lent cameras of player 2's door use). Or, under the window, let either the kept route or the press route take a B,
never both.

### 2.8 The runs never meet a door that holds play [real, for the test plan]

- Run run-20261113-134904-split-k2-p2-leaves (183 -> 180, script 119) shows every [change-hold] frame of the window as
  `busy 0x4 ... r13-24048 0x10000`, with "busy before 0x0" [log].
- So S1-S14 cannot show §2.3, the bit-4 half of §2.4, or §2.7.
- **Fix.** Add one run per direction at a door that holds play: script 132's climb (a ladder), or the ask-first door
  L01/103. Read `busy & 2` and r13-24048 at the ticket in the frame line.

## 3. Smaller points and risks

- **Moving the `waits` test above R4 also moves it above g_follow_refusing** (hle.c:16761, 16770, 16780) [read]. A look
  that waits would then hold a way back that a brought-along player stands in, and fire it later. Order it as
  g_follow_refusing, then waits, then R4. [maybe]
- **0x810 spells hold play.** Their record step calls 0x80199EA0(0) when 0x801D7024 says so: busy 2 and the game copy's
  walking channels. The host passes over only Enchant's hold (presses-effects §3.4; 0x801C3740-0x801C376C) [read there].
  The design's "every other class only makes a record" counts direct calls only. A stayer's 0x810 cast in the window puts
  bit 2 into the shared word. For player 2's cast the channels held are player 1's, and the arrival's busy clear
  (0x80152184) takes the bit off under the live spell. Add a 0x810 spell to S2 or S7. [maybe]
- **L3 has no run for a cast already under way at the ticket.** L3 is about casts under way at the ticket, but S2 and S7
  press inside the window. Add one run where the stayer starts charging before the other's door. [maybe]
- **Player 1's copy outside player 2's close-up has never been run.**
  - tome_copy_step's player 1 path sets aurora_split_menu(0) and aurora_split_only_view(sharing_was ? 2 : 1), written
    "over player 2's close-up" (hle.c:15018, 15034-15036) [read].
  - tome_copy_p1_left's gates are of that case too (27873-27882) [read].
  - In the window, player 2's half is their door's fade (g_fade_mover 2, 3359-3366).
  - S8 needs shots of both halves. [maybe]
- **P5 calls coop_p1_close_up and coop_p2_holds from inside split_on.** split_on then runs from every hooked busy ask and
  from split_send inside the busy set 0x801556CC (24958-24963). coop_p1_close_up has side effects: it clears
  g_coop_p1_look and the static `on` (19311-19318). The 102 sent by the stayer's own scene busy set may be kept or not
  depending on whether g_coop_p1_look is already set. [maybe]
- **The Start lock after the window.** r13-26232 counts down only while the play channel runs with r4 == 1, so not while
  words have the focus (0x80006900-0x80006964) [read there: b3 §3]. Player 1's words in the window keep the ticket's 65
  up past the window. After it, no copy route catches their Start, and 0x80006CDC refuses it until the count runs out.
  Zero it at split_over when the ticket set it, or keep the copy route while it is not 0. [maybe]
- **coop_script_hold_go has a third caller,** coop_player1_turn (hle.c:19849). The design lists two. The new argument
  must be false there. [real, small]
- **§4 calls the tick "the world frame's first call".** It is the third: 0x80020B08 and 0x801E3CB0 come first
  (0x800083E4, 0x800083E8) [read]. "Before the behaviour pass and the play channel" still holds. [real, harmless]
- **§10 puts player 2's OPEN DOOR ticket at ~3763.** The log has the press, the script and the ticket all in frame 3760
  (page-0001.log, `[split] player 2's door ... frame 3760`) [log]. Window frames in the runs should start at 3760.
- **The new intercept on 0x801C5118 sits beside ED_COOP_CAST_TRACE's notify** (28799-28803). The notify runs only on
  passthrough (hle.c:5578-5589) [read], so a skipped cast end is not traced. Log the skip itself. [real, small]

## 4. What was checked and holds

- **The ticket writer's order** [read 0x80155770-0x80155814]:
  - the record +0 at 0x801557BC;
  - 0x80117260(0x10000, 1) at 0x801557E0;
  - busy 4 at 0x801557EC;
  - 0x800443E4(r30 + 3) at 0x801557F4, which returns to 0x801557F8;
  - 0x801C5118(0) at 0x801557FC, which returns to 0x80155800.
- **The notify runs on passthrough of the entry's intercept,** before the body (hle.c:5578-5589, 11389). split_counting
  is true by 0x801557F4 [read].
- **At player 2's door the ticket is written in player 2's use,** in player 1's controller and outside g_coop_turn, with
  player 2's words in ("who 59 ctl 59" at lr 0x801557F0, frame 3760) [log]. coop_use_start swaps no coop_state
  (18180-18232) [read]. So L1's gate (lr 0x801557F8, !g_coop_turn) picks the right call. G2's guest call and the other
  callers (0x8011105C, 0x800C35F4) pass through it. No hook sits on 0x800443E4, 0x800068E0 or 0x8013E99C [read grep].
- **0x801C5118 ends a cast by r13-21488 and r13-22336** (0x801F6258 = lwz r13-21488; 0x801C5138) [read]. coop_player_turn
  swaps only r13-26524, -21492 and -21496 (20970-20975) [read]. So at player 2's walk-in door the cast ended is player 1's,
  as the design says.
- **The world frame's order** [read 0x800084D8-0x800084F8]:
  - the flags ask at 0x800084DC (lr 0x800084E0);
  - the clear at 0x800084EC;
  - the play channel at 0x800084F4;
  - the chooser at 0x800084F8.
  The chooser reads r13-24048 directly at 0x801176B4, not through 0x8011727C. So P1 needs both intercepts, as designed.
- **0x80006EBC's B path asks for neither walking channel unless r4 == 3** (0x80006EE8-0x80006F08) [read].
- **0x800069DC's gates hold none of the ticket's words.** Event 185 goes to r13-21496 (0x80006A48-0x80006A6C) [read].
- **Player 2 staying:**
  - their turn runs under room_other (20869);
  - their zones and B are off at 21000-21003;
  - their quick casts are dropped at 21090;
  - coop_left_behind is false under the stay in a resident room (16249-16252).
  [read]
- **The memcpy of cpu->ram at the copy's opening** (14977) carries the game copy's r13-23592 into the copy. The copy's
  after-frame list runs 0x8013E518 (15067-15071) [read]. The Tome frame is 0x80111234 (13218). The tick 0x80155830 is
  called only at 0x800083EC and 0x80050A90 [read grep], so the copy never counts the ticket down.
- **The arrival clears only 0x10000 of r13-24048** (0x8015203C-0x80152044) [read]. A stayer's find bit 2 crosses it.
- **ED_STAYER_PRESSES is safe for solo.** It is behind g_split.on, which needs mod_coop, g_coop_p2, coop_stay() and
  g_split_ready (24781-24782) [read]. No path reaches solo or follow mode.

## 5. Fix order, if the design goes ahead

1. §2.1 (coop_busy_ask skips its blanket cases in the window) and §2.2 (no p2_hold turn for the mover). Without these,
   P3 undoes the split hold's allow list.
2. §2.5: keep player 2's non-exit script presses as kept presses in the window.
3. §2.4: one chooser wrapper; door_bits without the prompts0 mask on bit 4.
4. §2.6: player 2's Start and Tome-asking cast lifted only with the copy.
5. §2.3 and §2.7: guard bit 2 and the kept-B route for doors that hold. Add the hold-door runs (§2.8) before calling
   either direction done.
6. The §3 items as the runs show them.
