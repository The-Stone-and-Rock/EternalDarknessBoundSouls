# Refute: design-stayer-presses.md against the goal

The goal (the user, 2026-11-13): the stayer keeps every action through the other's door countdown (~62 frames, and a
12-frame zone exit's), exactly as at any other time: quick spells, spells from the Tome, examine and pick up, zone looks,
Start and the Tome. Both shapes (split, apart). Both directions. Engine-level, co-op only, solo and follow mode untouched.

Read only. No code was changed, nothing was built, the game was not run. Sources: runtime/host/hle.c (working tree, 30,141
lines), build/ed-db/disasm.txt, `ed script`, `ed fn`, `ed dis`, the readers beside the design, logs/run-20261113-1348*-split-*.
[read] = seen at the address or line given. [inferred] = reasoned, not seen. [real] = the fault is in the code as read.
[maybe] = it needs a run or one more read to settle.

## 0. Verdict

The design's frame is right: one predicate (stayer_window) on top of split_counting, L1 for the lock that lands on player
1, P1 for the door's prompt bit, the host's follow-mode refusals lifted one line each. Most of its reads check out (§3).

But it is not yet safe to build as written. Six problems are in the code as read:

1. A use of player 2's in player 1's window is taken for a close-up of theirs, because the window's busy 4 is in the word.
   The game's pad goes to player 2, g_coop_screen goes on, and the design's own G2b then holds player 1 in black for 900
   frames (§1.1).
2. P3 opens player 2's hold branch, which gives **the mover** (player 1) a turn with the busy word at 0. K1 cannot keep the
   mover's filter then (§1.2).
3. Under the window, the busy ask's "not busy for everyone" cases now fire and free the K7 kinds the split hold keeps
   held (§1.3).
4. Moving the way-out wait above R4 also moves it above follow mode's way-back refusal, and it is not behind the switch
   (§1.4).
5. A look whose later step asks for a room (EXAMINE, DETAIL, Yes: the ladder) loses the stayer's answer (§1.5).
6. The runs do not exercise the goal: no spell cast from a Tome, no words inside the window, no 12-frame exit, no player 2
   walk-in door, no apart run with player 1 staying, no pickup for player 1 (§2).

The rest are [maybe] points or deviations the design already names but that the goal does not allow (§1.6-§1.14).

## 1. Problems

### 1.1 Player 2's use in player 1's window becomes a lend-route close-up [real]

**What is wrong.** The design lets player 2's non-exit script prompts (PULL LEVER, ENGAGE ARRAY, any use) "run at once"
(§2.4). The use's own code reads the window's busy 4 as "something of theirs is up".
- coop_use_start sets g_coop_assist_at = 1 (hle.c:18215) [read].
- coop_menu_input: `closeup = (... g_coop_assist_at != 0u && !g_coop_script_hold) && coop_busy(cpu) && !coop_p1_in_close_up(cpu)`
  (27579). coop_busy is `busy != 0` (3974). Busy 4 makes it true [read].
- Then owner becomes 1, "player 2 has the close-up": the game's pad is player 2's (27775-27780) [read].
- continue_coop_use: no new ticket (room false), so `busy_new = coop_busy(cpu)` is true, and `g_coop_screen = true`
  (18270-18276) [read].
- The design's G2b (§4) waits while g_coop_screen is up and was not at the ticket. g_coop_screen is
  `(closeup && owner == 1)` each read (27789), and closeup stays true while busy 4 is in the word. Busy 4 stays while the
  countdown waits. So the wait ends only at SPLIT_WAIT_LONGEST, 900 frames (24543), then the change fires "under it"
  (24921-24928) [read; the loop is inferred].
- Meanwhile coop_busy_ask's case (a) answers 0 to all 35 callers (19728) [read].

**Who.** Player 2 staying. Player 1, the mover, stands in black up to 900 frames with their lock renewed (24937-24941),
and their pad is not the game's.

**Fix.** In the window, a use is "up" only by the bits it set itself:
- continue_coop_use: under stayer_window(cpu, 1), `busy_new = (busy & ~g_coop_use.busy_before) != 0` (the close_use form,
  18274, already does this);
- coop_menu_input's closeup: use `busy & ~COOP_BUSY_ROOM` when split_counting;
- or set g_coop_assist_at only when the use set a bit.

Add a run: player 2 presses a use prompt with no words in player 1's window. It must show no `player 2 has the close-up`
and no G2 wait.

### 1.2 P3 gives the mover a hold-opened turn [real]

**What is wrong.** P3 lets coop_p2_holds accept bit 4 in player 1's window (`|| stayer_window(cpu, 1)`). coop_p2_holds is
true for player 2's own scene (bit 32 in the game's word) or their script hold (hle.c:19667-19671) [read]. Then:
- hle_coop_controller's p2_hold branch runs coop_hold_open (busy word 0, the three hold channels' +16 to 0, r13-25696 0,
  zones on) and coop_player1_turn for player 1 (20322-20333) [read]. Player 1 is the mover, in their door.
- Inside that turn the busy word is 0. K1 (split_busy_kept, 24697-24701) passes the game's word through, which is 0
  just then. So the mover's busy filter (lr 0x80058E80) sees "not busy" and takes events 8, 11, 39 and the rest
  (split_filter_takes, 24641-24648). Solo never lets its mover be hit or grabbed inside a door [inferred from K1's purpose].
- P3b guards only the mirror (p1_look's hold-open for player 2's turn).

**Who.** Player 2 staying with their scene or script hold up. Player 1, the mover, is the one hurt.

**Fix.** Under stayer_window(cpu, 1), p2_hold opens no hold and runs no coop_player1_turn. The game's controller runs
for player 1 (passthrough), locked as in solo.

### 1.3 The "not busy for everyone" cases free the K7 kinds [real]

**What is wrong.** coop_busy_ask's cases (a) g_coop_screen, (b) coop_p1_close_up and (d) coop_p2_holds answer 0 to every
caller (19728-19732) [read]. Today none of them can be true inside a window: the room bit is accepted only for one's own
door (19308, 19670). After P3, and §1.1, they are.
- The split hold keeps kind 19 and the K7 kinds (9, 28, 32, 39, 50, 56, 73, 74, 82) off its allow list on purpose:
  split_node_freed (24634-24638) and split_kind_kept (24626-24629) [read]. K7 is "kinds whose paths reach the ticket
  writer, a capture, fn66 or a death solo never meets with a ticket counting" (24615-24616) [read].
- split_busy_kept keeps only three K7 lrs (split_k7_site, 24631) and the mover's filter. The K7 kinds' node update and
  motion asks (A1's lrs) are not among them [read].
- So under player 1's close-up in player 2's window, or player 2's scene in player 1's, case (b) or (d) answers 0 to the
  K7 kinds' updates. Kind 32 is the sanity tracker, whose effects end in a return ticket (b0-k7-readers.md:9, 106)
  [read there].
- P3b narrows only by room ("outside g_split.room, or roomless"). In the split shape the K7 nodes are in g_split.room.

**Who.** Both directions, the split shape most.

**Fix.** Under split_counting, cases (a), (b) and (d) answer 0 at A1/A2 lrs only for a node that split_node_freed passes,
or for the stayer's own node. Elsewhere they answer as today.

### 1.4 The way-out wait moved above R4 also changes follow mode [real]

**What is wrong.** §2.3 says: "Move the `waits` test (16776-16786) above R4's." In coop_script_go_to_room the order is
R4 (16761), then g_follow_refusing (16770), then waits (16776) [read]. Above R4 means above the follow refusal too. The
move itself is not behind stayer_window.
- Follow mode: player 1, brought along and standing inside a way back while player 2's scene or look is up (p2_up),
  takes the wait branch instead of "brought along, stands inside a way back: passed over". coop_way_out_go later clears
  the zone's inside bit (16739-16758), and the next look comes into the way back: player 1 is sent back through the door
  they came by [inferred].
- Co-op stay with no window (ED_SPLIT_HOLD=0, G3, a cutscene at the ticket, ED_STAYER_PRESSES=0): a look that waits is
  now kept instead of passed over. So §7's "off, every refusal of today stands" is not true, and run S12 cannot compare.

**Who.** Both players; follow mode and stay-without-window.

**Fix.** Leave today's order. Add a new first test: `if (stayer_window(cpu, who == 1 ? 1 : 0) && waits) { wait; return 0; }`.

### 1.5 A look that asks for a room later loses the stayer's answer [real]

**What is wrong.** M11 keeps only exit prompts (ids 1, 2, 3, 4, 10: coop_prompt_is_exit, 15412-15414) [read]. Many ways
out start from EXAMINE (id 6):
- Room 183's ladder, L01/103 [read `ed script 103`]: EXAMINE, then hold, busy_set(1), DETAIL, words with Yes/No. On Yes:
  `busy_set(0)`, `hold(1)`, `state_set(0)`, then fn66.
- The effects readers list transition 6 after EXAMINE, and CLIMB DOWN after EXAMINE or TEXT (presses-effects §4.3)
  [read there].
- If the Yes comes inside the window, fn66 is passed over by R4 (16761) or its ticket refused by G1 (24827). The script
  has already reset its state, so the answer is lost. The stayer stands, and must walk off and do the whole look again.
- The design's §5 row 3 (a use script's own room request: "passed over, always printed") is the same loss, by design.

**Who.** Both players, both shapes. The goal says "never lost".

**Fix.** Keep the room request, not the script. Under stayer_window, coop_script_go_to_room stores fn66's six arguments
and the asker. It returns 0 to the script, and makes the ticket after the change's tail as that player, the way
split_requests holds the dispatcher's request (24876-24903). Read fn66 (0x801666A4) first for what it does besides the
ticket [inferred: not read here].

### 1.6 The mover waits in black for the stayer, then the stayer is cut anyway [real]

**What is wrong.** Two of the stayer's actions still hold the countdown (§0.8, §4): player 2's scene (32) and, with G2b,
player 2's lend-route close-up or use, at player 1's door.
- The mover waits in the black with their lock renewed, up to 900 frames (24543, 24937-24941) [read].
- After 900 frames the countdown goes on "under it" (24921-24928) [read]. Player 1's arrival then clears every script wait
  and busy bit (0x80163F04(1), 0x801556CC(-1, 0): presses-effects §3.3) [read there]. So the stayer's action is cut.
- The user asked for the brief freezes to be gone. A mover held in black is the same complaint from the other side.

**Who.** Player 2 staying (both shapes). Player 1 waits.

**Fix.** Build R4d (design-r4d.md D1-D3): player 2's scene and close-up waits and bits kept over player 1's arrival, the
mirror of R4b. Then G2 need not wait for them.

### 1.7 The stayer's busy sets send 102 to the mover [maybe]

**What is wrong.** 0x801556CC sends event 102 from the current id to its room whenever the word is still nonzero after
the set or the clear (0x801556E0-0x80155720) [read]. In the window busy 4 is always in the word. So, from the stayer:
- player 1's close-up hold (0x80199EA0(0) -> 0x801556CC(2, 1), 0x80199F2C-0x80199F34);
- its let-go;
- coop_script_hold_go's let-go under P4 (COOP_PLAY_HOLD(1) with 4 still in the word, 19356-19359);
- a lend-route use of player 2's at player 1's door.

Each sends 102 to the room. split_send_keeps keeps it only from nodes that are not kind 19 or K7 (24957-24959) [read].
Players get it: "Player 1 is told and set free by coop_node_held_ask" (mover-readers.md:204) [read there].
coop_node_held_ask frees only the non-asker (19747-19766) [read]. So in the split shape the mover is held (+28 & 8), and
the motion step's "busy and held" gate (0x800271F0) stops their door walk (arrival-readers.md:46) [read there]. In solo
the mover gets no 102 in their door: the ticket's own goes "to the rest of the asker's room" (mover-readers.md:78).

**Who.** The mover in the split shape, in the first ~25 frames of their door motion, when the stayer presses EXAMINE.

**Fix.**
- In the window, keep a 102 whose sender is the stayer away from the mover's node (split_send_keeps).
- Zero the word round the let-go in coop_script_hold_go when room_ok is set, as coop_play_hold does (13116-13121).
- Add to S6 and S1: the mover's position in both.tsv keeps moving from the ticket to ~ticket+25.

### 1.8 P5 calls a stateful test from split_on [maybe]

**What is wrong.** P5 puts coop_p1_close_up / coop_p2_holds inside split_on, and only when coop_room_change_on is false
(the `||`). coop_p1_close_up clears g_coop_p1_look once it reads "over" (19313-19318) [read]. split_on is asked many
times a frame: S2, S3, S4 and the allow list from the 35 busy asks. An ask made while a bracket has rewritten the word
(coop_hold_open's 0, 19807; coop_play_hold's zeroing, 13119) ends player 1's look for good. Then R4b does not carry it
(stay_busy_clear and split_r4b_case key on coop_p1_in_close_up, 24898), and the arrival cancels it.

**Who.** Player 1 staying.

**Fix.** In split_on, use a test that writes nothing: `g_coop_p1_look && coop_p1_in_close_up(cpu)`, or cache the
frame's answer once.

### 1.9 A summon charging at the ticket keeps the old freeze; one pressed in the window is refused [real]

**What is wrong.**
- L3 keeps G3 for a charging summon: no window opens, "their room holds as today" (24801-24806) [read]. That is the old
  whole-room freeze for the stayer, and every press refused.
- A summon pressed inside the window is refused and logged (§2.6). The press is lost.

**Who.** Both.

**Fix.** Read the summon's body switch and return (design §12.4: who sends kind 2's event 250). Then open the window, and
hold only the summon's last step (the body switch or its ticket) until the tail.

### 1.10 Player 1's Tome in the window is not the whole Tome [maybe]

**What is wrong.** Player 1's Start opens the copy route built for player 2's close-up (27800-27809). Its close writes
back only:
- the magick record (14800-14806);
- equips (tome_copy_equips, 27846-27857);
- a script left and a cast (27873-27918).

[read] What the real Tome writes elsewhere (a Mix, the item order, other pages' words) stays in the copy [inferred: no
write-back for them was found].

Enchant Item by player 1's quick spell with no item chosen is refused (the design's §5). It ends cleanly
(0x801C4570-0x801C45A4: the spell word is cleared and event 176 sent) [read], but nothing is cast.

**Who.** Player 1 staying.

**Fix.**
- List what the Inventory page's code (0x80102000-0x80104800) writes, and carry each change back, or run a Mix in S8.
- Route Enchant's "choose an item" to player 1's copy, as coop_quick_cast already does for player 2
  (g_tome_copy_spell, 12515-12520).

### 1.11 door_bits' bit 4 has the wrong source [real, small]

**What is wrong.** §2.2 says player 2's door script hold is 0x80042B30 and sets bit 4, r13-25696 and the zones word. But:
- g_coop_script_hold is set at the script step's call of 0x80199EA0: SCRIPT_HOLD_FROM..TO 0x8016820C-0x801682A8, the bl at
  0x8016829C (hle.c:13043-13044, 13185-13186) [read]. That is fn21.
- 0x80199EA0 touches only the three walking channels and busy 2 (0x80199EA0-0x80199F34) [read].
- Bit 4, r13-25696 and the zones word are fn139's (0x80042B7C-0x80042B90) [read].
- Door script 119 calls neither (`ed script 119`) [read].

So masking 4 "while g_coop_script_hold" can unmask a bit 4 that is player 1's own (their fn139 hold) and show their
prompts under it.

**Fix.** door_bits = 0x10000 & ~prompts0, and no bit 4.

### 1.12 Player 2's zone look under a door that turns zones off [maybe]

**What is wrong.** §2.3 turns zones on for player 1's look (line 20210), not player 2's.
- Player 2's look (coop_prompt_look) runs the game's scan 0x80134A44. The scan asks the zones word (0x8019C098 at
  0x80134A80) and on 0 only clears what they are inside (0x80134BE8) [read].
- The zones word is set to 0 by fn22, fn139, 0x80042A8C and a script function (0x8016AAA8) [read: the five callers of
  0x8019C088].
- 134 scripts call fn139 (`ed uses fn139`). Whether any door calls it before its fn66 was not read.

Under such a door of player 1's, player 2 gets no prompts and no zone looks.

**Fix.** In coop_prompt_look, under stayer_window(cpu, 1), turn the zones word on for the scan and put it back after, as
coop_scan_player1 does (20208-20227).

### 1.13 room_turn comes back from a stale input copy [maybe]

**What is wrong.** P6 turns room_turn off during player 1's own look, words or find, and on again after. On again, the
turn is fresh (20375, then 20362-20363), with `g_coop_p1_from_free`. So it starts from g_coop_p1_free [read].
g_coop_p1_free is saved only while the busy word is 0 (20301-20304). In the window that is never, so the copy is from
before the ticket [read]. Player 1's walk comes back with the stick, held buttons and channel timers of before the door.

**Fix.** After a P6 pause, start room_turn from the game's copy: set g_coop_p1_turn_fresh with from_free false.

### 1.14 Player 2's deferred Tome item [maybe]

**What is wrong.** Player 2's item script from the copy waits for the window's end (g_stayer_p2_left). But their counts
come out of the copy at the close (coop_tome_player(cpu, false), 14864) [read]. If the copy's Use already took the item
from their list, a dropped wait (a state load, §8) loses the item with no effect [inferred: whether Use removes the
item in the copy or in its script was not read].

**Fix.** Read where the item leaves the list. If it is in the copy, keep the count with the deferred script, or keep the
Tome open until the window ends.

## 2. The checks do not exercise the goal [real]

Window frames, from the design's own numbers: player 1's door ~3822-3884; player 2's door ~3763-3826.

| Gap | Proof | What to add |
|---|---|---|
| No spell is cast from a Tome | S3 and S8 open, turn a page, close | a Tome cast in the window, both players |
| Words inside the window, never | S1 DETAIL at 3935 (window ends ~3884); S6 DETAIL at 3880 (ends ~3826). S6's "~3880 player 1's words open with no hold" is outside the window, today's path. P4 is never run | EXAMINE before the ticket, DETAIL inside the window, so the words cross the firing |
| No pickup for player 1 | only S5b (player 2) | player 1's PICK UP and find words in player 2's window (P4's no-hold find) |
| No 12-frame zone exit | every run uses a 62-frame door | a walk-in exit by each player, with a press 2-8 frames after its ticket |
| No player 2 walk-in door | S6-S10 use OPEN DOOR by B (a use) | player 2 walks into a way out: the L1 `g_coop_turn` case, their copy's lock, and stayer_cast_end, the one place that intercept acts |
| Apart with player 1 staying | S11b is a comment | write it, with examine, quick spell, Start and pickup |
| Casting may not happen in chapter 1 | the design: "the cast's words 0,13 show the cast ran ... re-checked in the user's play" | a Jump-to-Game slot whose character has spells, so a spell record crosses the firing |
| A non-exit use by player 2 (§1.1) | none | PULL LEVER or another use in player 1's window |
| A look that asks for a room (§1.5) | none | the ladder L01/103's Yes inside the window |
| The mover's motion (§1.7) | no row checks it | both.tsv: the mover keeps moving through ticket+25 |
| A summon refusal, a G2b wait | none | one each |
| Off equals today | S12 cannot show it (§1.4) | after §1.4's fix |

The user's play list (§11) is good. Add: a Tome spell (not only a page), and a look that ends in a room (the ladder).

## 3. What checked out

- L1's path. At player 2's prompt door the ticket runs in a use (coop_use_start, 18182-18240), not in their turn: g_coop_turn
  is set only at 20903 and cleared at 21118 [read]. Player 1's input copy is in. split_ticket_open runs at the writer's
  entry (notify_room_ticket, 11388), before the lock call at 0x801557F4 [read].
- Reads before building, §12.2: g_coop_use.active ends when the door script returns. The use is a tail call into
  COOP_RUN_SCRIPT with lr HLE_USE_RETURN, and continue_coop_use clears it (18233-18245) [read].
- The firing and the tail are in one frame: "[split] over at frame 3883 (the change's tail)" for a 3821 ticket
  (logs/run-20261113-134809-split-k1-p1-leaves/page-0001.log:556, 1377) [read]. So "+0 > 0" is the whole window.
- r13-24048 has three readers: 0x80117260 (set), 0x8011727C (ask) and the chooser at 0x801176B4 [read the lwz list]. The
  ask is called from 0x80008490, 0x800084DC and 0x8009E7BC [read]. P1 covers player 1's prompts fully.
- The world frame's order: the tick (0x800083EC), the prompt clear (0x800084DC-0x800084EC), the play channel (0x800084F4),
  the chooser (0x800084F8) [read]. No press is cut by the firing.
- 0x80006EBC's B path asks no walking hold. The test at 0x80199F50/74 is only for r4 == 3 (0x80006EE8-0x80006F08) [read].
- The 0x801C5118 intercept can sit beside ED_COOP_CAST_TRACE's notify: HleDispatchEntry has its own intercept and notify
  fields (hle.c:56-59, 28798-28803) [read].
- tome_copy_mode() is 2 unless ED_COOP_TOME_COPY says otherwise (3956-3960) [read]. Player 1's copy route is on by
  default.
- coop_way_out_p1_up / _p2_up are read only at 16815, 20196 and 20241 [read].
- The copy opens by mode words and TW_MODE_CHANGE, not 0x80006CDC (14998-15057) [read]. So the copied Start lock does not
  refuse it.
- Solo: every change keys on split_counting -> g_split.on, which needs g_split_ready, mod_coop, g_coop_p2 and coop_stay()
  (24781-24782) [read]. The exceptions are §1.4's reorder and §1.3's cases, which are fixed above.

## 4. Facts read

| Fact | Where | Mark |
|---|---|---|
| coop_busy is busy != 0 | hle.c:3974 | [read] |
| coop_use_start: assist_at = 1, busy_before kept | hle.c:18190, 18215 | [read] |
| continue_coop_use: busy_new = coop_busy, then g_coop_screen | hle.c:18270-18276 | [read] |
| coop_menu_input closeup and owner | hle.c:27579, 27775-27780, 27789 | [read] |
| G2's 900-frame cap and the lock renewal | hle.c:24543, 24921-24941 | [read] |
| p2_hold opens the hold and runs player 1's turn | hle.c:20322-20333 | [read] |
| coop_p2_holds' room bit | hle.c:19667-19671 | [read] |
| busy ask cases answer 0 to all | hle.c:19728-19732 | [read] |
| split_node_freed, split_kind_kept, K7 kinds, K7 lrs | hle.c:24615-24638 | [read] |
| go_to_room order: R4, follow, waits | hle.c:16760-16786 | [read] |
| script 103: state reset before fn66 | `ed script 103 --level 1` | [read] |
| script 119 has no fn21 or fn139 | `ed script 119 --level 1` | [read] |
| 0x801556CC sends 102 when the word stays nonzero | 0x801556E0-0x80155720 | [read] |
| split_send_keeps lets kind 19/K7 through | hle.c:24957-24959 | [read] |
| g_coop_script_hold from the stepper's 0x80199EA0 | hle.c:13043-13044, 13185-13186; 0x8016829C | [read] |
| 0x80199EA0 touches channels and busy 2 only | 0x80199EA0-0x80199F34 | [read] |
| fn139 sets 25696, the zones word, bit 4 | 0x80042B7C-0x80042B90 | [read] |
| the scan asks the zones word | 0x80134A80-0x80134A88 | [read] |
| zones word writers | 0x800429F0, 0x80042AF8, 0x80042B84, 0x8016AAA8, 0x8019BFCC | [read] |
| p1 copy close writes the magick record only; equips replayed | hle.c:14787-14830, 27846-27857 | [read] |
| Enchant's refused Tome ends with event 176 | 0x801C4560-0x801C45B4 | [read] |
| g_coop_p1_free saved only at busy 0 | hle.c:20301-20304 | [read] |
| room_turn fresh from free after a pause | hle.c:20362-20363, 20375 | [read] |
| firing and tail in one frame | split-k1-p1-leaves page-0001.log:556, 1377 | [read] |
| coop_prompt_is_exit ids | hle.c:15412-15414 | [read] |
