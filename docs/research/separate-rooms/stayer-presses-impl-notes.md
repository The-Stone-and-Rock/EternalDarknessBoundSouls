# The stayer's presses: implementation notes (implementer 1: step A, M11, M12, the M13 line, §7, §8, G2b)

This is what was built from design-stayer-presses.md. Everything is in runtime/host/hle.c, and the line numbers below are
from the working tree after the change. The build is clean: build.bat ends at "Linking CXX executable EternalDarkness.exe"
with no errors. The only hle.c warning that isn't a deprecation is the old one (now at 5654). The game was not run, `ed
test` was not run (it launches the game), and nothing was committed. hle.c stays LF.

**Not built here (implementer 2's, step B):** B1 (coop_busy_ask's blanket cases in the window), B2 (split_on by
g_stayer.bits), B3 (coop_p1_close_up's room bit; coop_player_held(1); no hold-open for player 2's turn), B4
(coop_script_hold_go's room_ok; coop_their_close_up passes false), B5 (room_turn under player 1's own look/words/find),
B6 (player 2's uses up by their own bits). The g_stayer.bits record B2 needs is built here (stay_busy_clear).
**Not built (as told):** C2 and M13 (only M13's always-printed loss line).

## Pieces and where they are

| Piece | hle.c | What |
|---|---|---|
| Forward declarations, `g_stayer`, `g_stayer_p2_left`, `g_stayer_marks`, `STAYER_SUMMON` | 3277-3320 | the record (prompts0, bits, screen0, L1's lock, the over line's counts, said bits, the frame line's frames, kept summons); player 2's kept Tome item; M11/M12's marks for a state load |
| ED_STAYER_PRESSES, stayer_switch_allows, stayer_window, stayer_say, stayer_current, stayer_things_up, stayer_door_bits | 24832-24890 | §1. `stayer_window(cpu, who)` = the switch allows who, split_counting, and g_split.mover is the other one |
| G3 narrowed | split_stayer_casting, 25012 | with the switch allowing the stayer: only a summon-class spell word (`(spell & 0x1FF0) == 0x820`) opens no window; a record-only cast is told and its window opens |
| Window open | split_ticket_open, 25079 (memset of g_stayer after the co-op gate; prompts0, screen0, bits at the end) | §1 and the open line |
| Over line, Start lock put back | split_over, 25045 | the `stayer: looks ...` line; L1's Start lock lowered to `max(0, lock_n - frames)` when not above lock_n |
| G2's always-printed line, G2b, frame line, player 1's kept summon | split_tick, 25222 | see below |
| §8 state load | split_state_load, 25339 | zeroes g_stayer, g_stayer_p2_left, the marks; drops M11 keeps and M12 waits made in a window |
| L1 | stayer_lock, 25373; install 30055-30062 | intercept on 0x800443E4: lr 0x801557F8, !g_coop_turn, stayer_window(cpu, 0): guest call of 0x800068E0(r3) only; 0x8013E99C not called |
| Cast end at the ticket | stayer_cast_end, 25396; install after L1's | intercept on 0x801C5118: lr 0x80155800, the stayer's window, r13-21488 the stayer's id: skipped and told |
| Player 1's kept summon cast after the tail | stayer_after_tail, 25415 (from split_tick's not-counting branch) | lr 0x800083F0 only, outside any turn, nothing busy, player 1 current: 0x800069DC(0, COOP_QUICK_SLOT) |
| Frame line (ED_STAYER_LOG=1) | stayer_frame_line, 25435 | once per window frame at the tick |
| The busy bits the stayer set | stay_busy_clear, 26040 (first block, before its own test) | under split_counting and the stayer's window, by the current id: set bits OR'd, cleared bits taken off |
| P1, the flags ask | coop_flags_ask, 19754 | lr 0x800084E0 under stayer_window(cpu, 0): `word & r3 & ~door_bits` when the door's bit is in the word |
| P1, the chooser | coop_prompt_choose (wrapper) 20120; today's body now coop_prompt_choose_today, its `running` static now `g_prompt_choose_running` | one wrapper first: door_bits out of r13-24048, today's code run (or the plain choosing guest-called inside the bracket), the bits put back |
| Quick casts: the ask | coop_quick_cast_ask, 19916 | under stayer_window(cpu, p2 ? 1 : 0): a summon (by r4's word) kept and refused, always told; anything else passes |
| Quick casts: player 2's wait | coop_player_turn, 21306 | `(room_other && !(stayer_window(cpu, 1) && tome_copy_mode() == 2)) || coop_left_behind(cpu)` |
| Player 2's kept summon cast after the tail | coop_quick_cast, 12519; its slot body now coop_quick_cast_one | at their first turn that would take a press, !split_counting |
| Zone look, player 2 | coop_player_turn 21195 (the lift), coop_way_out_p1_up 16777 (`|| stayer_window(cpu, 1)`), coop_prompt_look 16976 (zones on for the scan) | |
| Zone look, player 1 | coop_scan_player1 20368 (the lift), 20395 (zones on, the line); coop_way_out_p2_up (`|| stayer_window(cpu, 0)`) | |
| M11, player 2 | coop_player_turn 21234 (keep), 21270 (release waits in the window) | |
| M11, player 1; player 1's Start | stayer_p1_presses 27855, called after coop_p1_press_kept at 28214 | |
| §2.4: no new keep of player 1's B in the window | coop_p1_press_kept, 27793 | |
| M12 and the M13 line | coop_script_go_to_room, 16852 (first test), 16877 (the loss line in R4's branch) | |
| Player 2's Start | coop_menu_input 27953 (the ask beside player 1's Tome), 28184 (start_room) | |
| The copy's binding lock | tome_copy_step, right after the memcpy (about 15050-15065) | r13-23592 written 0 in the copy, under stayer_window of the copy's owner |
| Player 2's copy close in the window | tome_copy_close, 14949; stayer_p2_left_go 27845, called from tw_mode_change 15335 | |
| Player 1's copy close in the window | tome_copy_p1_left, 28419 | the split pending flag |
| room_turn for the frame line | hle_coop_controller, 20558 | `g_stayer.room_turn_frame` |

### Install

Inside the split hold's install (find_coop's ED_COOP_STAY block, after `g_split_ready = true`), so only with coop_stay()
and the split hooks installed:

| Entry | Checked | Coexists with |
|---|---|---|
| 0x800443E4 | g_split_lock_ok (stwu, bl 0x800068E0 at +0x14), bl 0x8013E99C at +0x1C, the writer's bl at 0x801557F4 | nothing on it (intercept and notify NULL required) |
| 0x801C5118 | stwu r1,-32(r1), bl 0x801F6258 at +0x1C, the writer's bl at 0x801557FC | ED_COOP_CAST_TRACE's notify_cast_trace (installed with the Tome hooks) is accepted; it runs on passthrough only |

Each intercept runs first; on passthrough the notify runs. Neither is in tome_copy_hook_off's list: each tests
split_counting, which is false in the copy's turn. Neither guest-calls its own entry, so no running guard is needed. The
chooser wrapper guest-calls 0x80117628 from its own intercept with `g_prompt_choose_running` set, which sends the nested
call straight through today's code.

## Where the build departs from the design, and why

1. **G2b and G2's always-printed line are built here.** They are §4.2/§7, and the task's list gave me §7's lines. G2b
   is `g_split.mover == 1 && stayer_switch_allows(1) && !g_stayer.screen0 && g_coop_screen` as one more wait reason in
   split_tick; `g_stayer.screen0` is g_coop_screen at the ticket. The line names SCENE (bit 32), USE (G2b or player 2's use
   active), WORDS (8/16) or CUTSCENE. **Without B6, G2b holds player 1 in the black for any lend-route use of player 2's in
   the window** (B6's "a use is up only by its own bits" is what keeps g_coop_screen down for a use that opens nothing).
   Implementer 2: don't build G2b again.
2. **The kept summons are kept by spell word, not player 2's slot index.** Their Tome can change the slots before the
   tail. Player 2's is cast through a refactored coop_quick_cast (its slot body is now coop_quick_cast_one, unchanged but
   for the spell word passed in), so the carry-list swap and the Tome-ask route are the same as a press's.
3. **Player 1's kept summon is cast from split_tick's not-counting branch** (lr 0x800083F0, the world frame's tick, outside
   any turn), only with nothing busy, no words, no cutscene or menu, and player 1 current. The design said "after the tail"
   without a place. It waits until those hold; a new ticket or a state load drops it.
4. **Player 2's kept Tome item runs at the loop's top** (tw_mode_change, beside tome_copy_p1_left), not "at the first player
   2 turn". tome_copy_left does its own coop_be_player2, room and state swaps, and runs today from tome_copy_close at that
   same place, outside any turn; inside their turn it would swap twice. It waits for !split_counting, !coop_busy, no copy
   open or asked, no use, no words, no cutscene or menu, and !coop_p2_own_up.
5. **g_stayer_p2_left is not zeroed at a new ticket**, only at a state load (the design zeroes "g_stayer" at each ticket
   and lists the Tome item among its parts). The item may already be off their counts (below, R6), so dropping its script
   at the next door would lose the item for nothing.
6. **A summon-class cast left by player 2's copy waits with the item** (`g_stayer_p2_left.next_cast`); the copy's close
   then calls tome_copy_left(cpu, 0, 0, item) at once, which backs out an Enchant cast the copy was opened for
   (g_tome_copy_spell_open). Any other cast goes at once.
7. **Player 1's Start route also refuses under their own scene** (coop_p1_scene_on) and under words up in the game's window
   (r13-25960, not player 2's lend-route words), beside the design's look, words and find. In solo the game refuses Start
   in a scene. Refused there, the Start stays in the pad and the game's Start lock refuses it.
8. **M11's release for player 2 waits on `stayer_switch_allows(1) && split_counting(cpu)`**, not bare split_counting, so
   ED_STAYER_PRESSES=0 (or p1) leaves today's release exactly as it was.
9. **State load drops M11 keeps and M12 waits made in a window** (§8: "dropped"; their tickets rule wouldn't). They are
   marked when kept (tickets count and prompt for a keep, tickets for a way out) in `g_stayer_marks`, and dropped in
   split_state_load only when the kept record still matches the mark. A keep or way out from outside a window is left as
   today.
10. **The over line's stayer counts print only under split_log and when the switch allows that stayer**; the Start lock
    put back runs whether or not the logs are on.
11. **stay_busy_clear's record runs for any busy set with bits 8/16/32 in r3**, before its own (unchanged) test, and only
    under the stayer's window with the current id the stayer's. It writes only g_stayer.bits.
12. **The chooser's `running` static became a file static** (`g_prompt_choose_running`) so the wrapper's own guest call is
    seen by today's body; the body is otherwise unchanged.
13. **The copy's binding-lock clear keys on stayer_window of the copy's owner** (player 1 for g_tome_copy_p1, else player
    2), so a copy of the mover's (player 2 opening theirs in their own door, which today's code allows) is today's.
14. **The cast end skip uses stayer_window** (the design said split_counting): with the switch off, the cast end is today's.
15. **Line numbers moved.** Checked against the file before each edit: coop_flags_ask was at 19628 (design 19628), the
    chooser 19923, coop_quick_cast_ask 19774, coop_scan_player1's refusal 20183, coop_player_turn's look lift 21000, its
    quick wait 21090, coop_way_out_p1_up/p2_up 16686/16691, coop_script_go_to_room 16760, coop_prompt_look's zones line
    16856, coop_p1_press_kept's keep 27328, coop_menu_input's Start ask 27424 and start_room 27647, its play branch's
    coop_p1_press_kept 27668, the memcpy 14977, tome_copy_close's tome_copy_left 14887, tome_copy_p1_left's busy test
    27881, stay_busy_clear 25590, split_over 24761, split_ticket_open 24778, split_tick 24905, split_stayer_casting 24736,
    split_state_load 25007. All as the design describes them.

## Addresses checked against the disassembly

- Ticket writer 0x80155770-0x8015580C: +0 at 0x801557BC; 0x80117260(0x10000, 1) at 0x801557E0; busy 4 at 0x801557EC;
  0x800443E4(r30 + 3) at 0x801557F4 (lr 0x801557F8); 0x801C5118(0) at 0x801557FC (lr 0x80155800). As designed.
- 0x800443E4: bl 0x800068E0 at 0x800443F8 (+0x14), bl 0x8013E99C at 0x80044400 (+0x1C). 0x800068E0 is the unsigned max
  into r13-26232.
- 0x801C5118: void; bl 0x801F6258 (lwz r3,-21488(r13)) at +0x1C, then r13-22336.
- 0x800084DC bl 0x8011727C, lr 0x800084E0; 0x8011727C is `lwz r0,-24048(r13); and r3,r0,r3`. The chooser reads
  r13-24048 at 0x801176B4.
- The tick 0x80155830 is called at 0x800083EC, lr 0x800083F0.
- 0x801C43D8 `rlwinm r0,r29,0,19,27` (0x1FF0), 0x801C4438 `cmpwi r0,2080` (0x820).
- 0x800069DC: r4 is the slot address (`or r31,r4,r4`), cast by 0x801C4250(1, r31).
- 0x801556CC: r4 != 0 sets r3's bits, r4 == 0 clears them.

## R6, read as far as the deferral needs

Not settled where the copy Tome's Use takes the item off player 2's list. What was read:
- The copy's Use runs the item script's first step **in the copy** (COOP_RUN_SCRIPT with lr in TOME_ITEM_CODE records
  g_tome_copy_use_script/item, hle.c about 18722), and tome_copy_left runs it again in the real game with that item.
- At the copy's close, coop_tome_player(cpu, false) runs **in the copy's memory**, and its coop_kinds(cpu, false) takes
  player 2's counts out of the copy. So a count the copy took comes back to host memory at the close, before any script.
- The rest of the copy's memory is discarded; only the magick record, spells, equips, the script and the cast come back.

So deferring the script doesn't change what the count is; it only delays the script's real effect to after the tail. That
is why the kept item is not dropped by a new ticket (departure 5). A state load drops it (told), and the load brings back
its own counts with its RAM; whether coop_kinds' host counts are in the state was not read.

## Co-op only

- Every new path starts from stayer_window (switch, split_counting, the mover) or from split_counting with the switch:
  g_split.on needs g_split_ready, mod_coop, g_coop_p2, coop_stay() and no copy turn. Solo, follow mode (no ED_COOP_STAY),
  no ED_ROOM_SLOTS installs, ED_SPLIT_HOLD=0 (no window), ED_STAYER_PRESSES=0 (stayer_switch_allows false everywhere) and
  the copy's turn all run today's code.
- The new intercepts are installed only with the split hold's hooks, and pass through outside a window.
- g_stayer is zeroed at split_ticket_open only after its co-op gate.
- g_stayer_p2_left, the kept summons and the marks are set only inside a window.

## Not confirmed (no run was made)

- That any of it works in play: none of runs S1-S19 was made, nor `ed test`.
- Whether the play channel's +16 is open at player 1's B in player 2's door (the frame line prints it).
- R2: whether player 2's shown icon goes through 0x80117628 with the shared word. Their chooser has no P1 wrapper
  (the wrapper is player 1's only, as designed until run S4 settles it).
- Player 1's copy Tome opened outside player 2's close-up (tome_copy_step's player 1 views and its "over player 2's
  close-up" line are today's; run S8).
- Whether a stayer's 0x810 spell's 0x80199EA0(0) in the window harms the mover (run S7b).
- That the M11 release for player 1 sees the exit prompt on show after the tail in a split (it needs the game to choose it
  again; COOP_PRESS_UNSHOWN drops it after 180 frames otherwise).
- §8's save prompt kept as an exit: not built (its prompt id isn't known; it isn't one of coop_prompt_is_exit's).
- The G3 narrowing lets a record-only cast charging at the ticket go through a window; that its record crosses the firing
  is the design's reading (effects §1), not seen.

# Step B (implementer 2: B1-B6; G2b checked, not rebuilt)

Built from design-stayer-presses.md §3.1-§3.6 on top of step A, which is kept whole. Everything is in runtime/host/hle.c;
line numbers are from the working tree after the change. The build is clean: build.bat (hle.c rebuilt from a touch) ends at
"Linking CXX executable EternalDarkness.exe", exit 0, no errors; the only hle.c warning that isn't a deprecation is the old
one at 5656. The game was not run, `ed test` was not run, nothing was committed, and controls.cpp wasn't touched. hle.c stays
LF.

## Pieces and where they are

| Piece | hle.c | What |
|---|---|---|
| Forward declarations | 3282-3283 | `stayer_window_on`, `split_door_holds_play` |
| `stayer_window_on(cpu)` | 24941 | step B's window: `stayer_window(cpu, stayer)` with the stayer from g_split.mover (either player can be the mover) |
| `split_door_holds_play()` | 24947 | B4: `g_split.on && (g_split.word0 & 2)`, the door's hold up at the ticket |
| B1 | coop_busy_ask, 19890-19900 | after split_busy_kept: under `stayer_window_on`, only split_busy_allows answers 0, else the game's word; cases (a), (b), p1_free and (d) are not asked |
| B2 | split_on, 24906 | with g_stayer.bits 0, today's `split_counting && coop_room_change_on`; otherwise split_counting, busy 4, `(busy & ~(4|2|1) & ~g_stayer.bits) == 0`, no cutscene, no menu. Calls nothing that writes |
| B3, the room bit | coop_p1_close_up, 19442 | `coop_room_change_other(cpu, true) \|\| stayer_window(cpu, 0)` |
| B3, no hold-open | hle_coop_controller, 20557 (`look_hold_open`) and its shut | p1_look's coop_hold_open for player 2's turn only when `!stayer_window(cpu, 0)`; p1_look itself (room_turn off, passthrough) is unchanged |
| B3, player 2 held | coop_player_held(cpu, 1), 19850 | `\|\| (stayer_window(cpu, 1) && coop_p2_scene_on())`; coop_p2_holds not widened |
| B4 | coop_script_hold_go(cpu, room_ok), 19473-19515 | busy read `& ~(room_ok ? 4 : 0)`; with room_ok, bit 2 up and the door's hold at the ticket: true with no let-go (told, once a second at most) |
| B4's callers | coop_their_close_up 19529 (`false`), coop_no_pause 19537 (`stayer_window(cpu, 0)`), coop_player1_turn 20048 (`false`) | |
| B5 | hle_coop_controller, 20583-20599 | `p1_stands = stayer_window(cpu, 0) && (g_coop_words_free \|\| g_coop_find_free \|\| g_coop_p1_look \|\| (r13-25960 != 0 && !g_coop_words_theirs))`; room_turn gains `&& !p1_stands` |
| B5, fresh turn after | the room_turn block (about 20625) | static `p1_stood`, set while B5 pauses room_turn, cleared when player 2's room change is off: room_turn's first turn after a pause has `g_coop_p1_from_free = false` (from the game's copy) |
| B6, continue_coop_use | 18397-18405 | `own_bits = close_use \|\| stayer_window_on(cpu)`: busy_new by `busy & ~busy_before` |
| B6, coop_menu_input | 28197-28209 | `b6_busy` = `(busy & ~4) != 0` under `stayer_window_on`, else coop_busy: the g_coop_script_hold clear and closeup's busy test |
| G2b | split_tick (implementer 1's, 25325) | checked: `g_split.mover == 1 && stayer_switch_allows(1) && !g_stayer.screen0 && g_coop_screen`, with the always-printed `THE COUNTDOWN WAITS ... (G2b)` line. Not built again |

### New log lines (under split_log, once per window unless said)

- `[stayer] player 1's words or find open with no hold in player 2's door (busy 0x..)` (B4, stayer_say kind 9);
- `[stayer] player 1's words or find open with no hold: the door's own hold (bit 2 at the ticket) stays` (B4, at most once
  a second);
- `[stayer] player 1 stands in their own look, words or find through player 2's door (...)` (B5, kind 11);
- `[stayer] player 1's turn through player 2's door again, from the game's copy` (B5, kind 12);
- `[stayer] player 2's use is up by its own bits only: 0x.. (busy, before; script)` (B6, kind 10; §7's line).
The existing `[coop] player 1's words open with no hold` / `find opens with no hold` lines still print under ED_COOP_LOG.

## Where the build departs from the design, and why

1. **"Under split_counting with the switch on" is `stayer_window_on`** (B1, B6): split_counting and ED_STAYER_PRESSES
   allowing whoever stays. With `p1` or `p2`, the other direction's window is today's code in coop_busy_ask, continue_coop_use
   and coop_menu_input, so the switch's one-direction runs compare cleanly.
2. **B2 keeps today's exact test while g_stayer.bits is 0** (no stayer bits recorded, or the switch off), and the ticket
   test of coop_room_change_on is split_counting's own.
3. **B4's no-let-go sits at the let-go itself**, after the refusal test, so it also covers `busy == 2|1` (a look's own
   fn91(1) beside the door's hold). The design's "return true with no let-go" is the same in the `busy == 2` case.
4. **B5 is gated on stayer_window(cpu, 0)**, not added to room_turn bare: in follow mode (and with the switch off) player
   1's words in player 2's door still take the game's hold (B4's room_ok is false), so room_turn is today's there.
   `g_coop_p1_look` with p1_look false (the look not yet "held", or held false a frame) stands them too, as the design's
   list has it.
5. **B6's continue_coop_use log** prints only for a use that isn't a close_use (close_use already reads its own bits).

## Not built (as told)

- C2 (§4.2): G2 and G2b keep waiting for player 2's lend-route things and scenes. coop_their_close_up passes room_ok false.
- M13 (§5.3): only implementer 1's always-printed loss line in coop_script_go_to_room.

## R6, as far as the deferral needs (checked again)

Implementer 1's deferral (tome_copy_close 14951-14963, stayer_p2_left_go) is kept as built. Read again: tome_copy_close
sets `cpu->ram = g_tome_copy.copy`, puts back g_coop_kinds_in from the copy's record and calls coop_tome_player(cpu, false)
there, whose coop_kinds(cpu, false) (12167-12193) takes player 2's counts out of the copy's kind records into
`own->count` (host) or into the guest `own` record (+THING_KIND_COUNT), before any item script runs. So the count is
settled at the close exactly as at any time, and the deferral moves only when the script runs. Not settled: whether a
count the copy's Use took is written back when the `own` record is a guest record (the write lands in the copy's memory,
which is then dropped). That is the copy route's own question at any time, not the window's; run S8 (a Mix and an item)
shows it.

## Co-op only (step B)

- B1, B6: `stayer_window_on` needs g_split.on (co-op, player 2 there, ED_COOP_STAY with ED_ROOM_SLOTS=2's installs, the
  split hold's window, not the copy's turn) and the switch.
- B2: g_stayer.bits is written only under the stayer's window (stay_busy_clear) and at the ticket under the switch; it
  is zeroed at each ticket and state load. With it 0 split_on is today's.
- B3, B4, B5: stayer_window(cpu, 0) / (cpu, 1). coop_script_hold_go's room_ok is false at every caller outside the window.
- Solo, follow mode, no ED_ROOM_SLOTS, ED_SPLIT_HOLD=0, ED_STAYER_PRESSES=0 and the copy's turn run today's code.

## Not confirmed (no run was made)

- That player 1's look stays up through player 2's arrival with B3 (no `[trace] player 1's look over` between the ticket
  and the firing, run S6) and that R4b then carries it.
- That the frame after the firing reads player 1's look as held (busy 2 after the arrival's clear keeps all but 4).
- That player 2 in state 121 runs right with player 1's look up and no hold-open for their turn (S6).
- B5: that the game's controller stands player 1 in their no-hold words (both.tsv, S6w), and the fresh turn after.
- B6: a use that opens nothing in player 1's door gives player 2 no screen (S5c), and one that opens a close-up still does
  and G2b waits for it.
- B4 at a door whose hold spans its ticket (script 132, S15/S16).

# Review fixes (stayer-presses-review-design.md "D", stayer-presses-review-safety.md "S")

Each point was checked against hle.c before anything was changed. Line numbers are from the working tree after these fixes.
The build is clean: build.bat ends at "[3/4] Linking CXX executable EternalDarkness.exe", exit 0, no errors. The only hle.c
warning that isn't a deprecation is the old string-plus-int one (now at 5669). The game was not run, `ed test` was not run
(it launches the game), nothing was committed, and controls.cpp wasn't touched. hle.c stays LF (no CR in the file).

| Point | Verdict | What changed |
|---|---|---|
| D1 / S1: room_turn comes back from g_coop_p1_free after a look or scene pause in the window | **Real.** p1_stood was set only under `p1_stands && !p1_look`, and B3 keeps the look up (p1_look true), so a look never set it. `!coop_room_change_other` also cleared it on every frame of player 1's own scene (bit 32, which B2 lets run). | hle_coop_controller 20632-20648: p1_stood is set on any window frame where room_turn is off for player 1's own thing: `stayer_window(cpu, 0) && !room_turn && (p1_stands \|\| p1_look \|\| coop_p1_scene_on())`. It is cleared only when `!stayer_window(cpu, 0) && !coop_room_change_other(cpu, false)`, and (as before) once room_turn has used it. A pause for something that isn't player 1's (p2_hold) doesn't set it, so the window's first room_turn still starts from g_coop_p1_free (the fix in coop-failed-attempts 543). The B5 log line (kind 11) is unchanged. |
| D2 / S2: split_tick's passthrough after stayer_after_tail's guest call | **Real.** The nested dispatches inside 0x800069DC write g_hle_passthrough, and hle_guest_call doesn't save it. | split_tick 25362-25369: `g_hle_passthrough = true; g_hle_tail_call = false;` after `stayer_after_tail(cpu)`. |
| D3 / S15: M13's always-printed line is missed under the stayer's own 8/16/32 | **Real.** coop_room_change_on is false under 8/16/32, so R4's branch (where the line was) isn't reached. fn66 then ran on and G1 refused its ticket. | coop_script_go_to_room: the line moved to its own test before R4, under `stayer_current(cpu) >= 0 && stayer_window(cpu, stayer)`. fn66 is passed over there (r3 0, handled), as R4 does and as design §6 has it ("until M13: passed over and printed"). So the stayer's fn66 no longer half runs (let-go, motion, fade) before G1 refuses its ticket. The line now gives the busy word and says when R4 was off. M13 itself (asking again after the tail) is still not built. |
| D4 / S3: player 2's kept Tome item doesn't keep use_script / use_item | **Real.** tome_copy_left reads the globals g_tome_copy_use_script/item when it finally runs. Any copy Use, or another tome_copy_left with a script (which writes 0), changes them in between. | g_stayer_p2_left (3310-3328) is now a queue of StayerP2Left records: script, next_cast, use_script, use_item, room and spell_item. tome_copy_close keeps the two globals in the record and clears g_tome_copy_use_script. stayer_p2_left_go puts them back just before tome_copy_left, only when there is a script (with a summon alone, tome_copy_left returns before its clear). |
| S4: a second deferral writes over the first | **Real.** The deferral wrote the one record without a test. | tome_copy_close 14963-14990: the record is added to the queue (4 deep). While one waits, any later close of player 2's with a script or summon queues behind it, in or out of a window, so items run in the order used. This is only reachable after a window. When the queue is full, a line is always printed and the item runs at once (today's). stayer_p2_left_go runs the first, then the next on later frames. |
| S5: a save between the deferral and its script loses the item | **Real enough** (design §8 says nothing straddles a save, and this does). | hle_save_held: `\|\| g_stayer_p2_left.n > 0u`. (characters_reload also waits on hle_save_held; it only delays a reload.) This departs from §8's "hle_save_held is unchanged" because the record is host memory that a state can't keep. |
| S6: the deferred script can run in another room | **Partly.** Not refused. The kept script is the item's own (script_left from r13-26324, for example L01/1100 for the Granite Block). It is the same script that a Tome use anywhere runs in solo, and that script checks where it is used. Dropping it, or refusing player 2's own room changes, would lose the item or hold them for something the game handles. | The room is kept at the deferral (coop_object_room of g_coop_p2). When it differs at the run, `[stayer] PLAYER 2'S KEPT TOME ITEM SCRIPT S RUNS IN ROOM N, NOT ROOM M WHERE IT WAS USED` is always printed. |
| S7: P1 unmasks player 2's lent prompts | **Real.** coop_cameras_lend swaps the prompt slots with the cameras. The lend is given back only once !coop_busy, so it can stay up through the window. The B routes (coop_p1_press_kept, stayer_p1_presses) already test `!g_coop_cameras.lent`. | coop_flags_ask's P1 branch and the coop_prompt_choose wrapper both need `!g_coop_cameras.lent`. With the cameras lent, the door's 0x10000 stays, as today. |
| S8: M11 takes player 1's B from their words or look | **Real.** The B branch had none of the Start branch's tests. | stayer_p1_presses 28045-28050: `own_up` (look, words, find, scene, or words in the game's window that aren't player 2's) is computed once. The B keep and the B count run only with !own_up, so the B stays in the pad. The Start branch uses the same own_up. |
| S9: B4 reads word0's bit 2 as the door's hold when it is player 1's own look | **Real** (both directions can be reached in the code). | g_stayer.look0 (player 1 staying, with their own look or scene up at the ticket) is set in split_ticket_open, and split_door_holds_play is `(word0 & 2) && !look0`. In coop_script_hold_go with room_ok, bit 2 is let go only as the hold of player 1's own look or scene. It stays when it was the door's at the ticket, or when player 1 has no look or scene now (a door that holds just after its ticket). The line says which. |
| S10: G2b keys on screen0 only | **Real.** A close-up that was up at the ticket, ended, and was followed by a new one was never waited for. | g_stayer.screen_down is set at any counting tick with g_coop_screen down. G2b is `mover 1 && allows(1) && (!screen0 \|\| screen_down) && g_coop_screen`. |
| S11: the kept-summon line prints every frame | **Real enough.** The print and the count were unconditional. Whether 0x800069DC is asked again while the button is held wasn't read, so it is guarded either way. | coop_quick_cast_ask: a summon already kept with the same spell is refused (handled) with no line and no count. |
| D8 / S12: player 1's kept summon is cast with their copy Tome open or player 2's use or close-up up | **Real.** stayer_after_tail had no copy, use or screen test. | stayer_after_tail also waits on `g_tome_copy_open \|\| g_tome_copy_ask \|\| g_coop_use.active \|\| g_coop_screen`. |
| S13: player 2's kept summon doesn't test the copy's turn | **Hardening, done.** No path runs coop_player_turn in the copy's turn, but the task forbids any new path there. | coop_quick_cast 12540: `&& !g_tome_copy_turn`. |
| D5: L1 tests only !g_coop_turn | **Guard, done.** coop_events_words swaps player 2's input copy in outside their turn. No ticket was found written there (the lr test already leaves out 0x800C35F4 and 0x8011105C), but the guard costs nothing. | stayer_lock: `\|\| g_coop_events_own` passes through. |
| D7: G2's always-printed line names player 2 for any wait | **Real (logging).** `extra` is every bit begun in the window, and player 2's lend-route words and finds write 8/16 directly. | split_tick: the line names player 2 only when G2b, a use of theirs, g_coop_words_theirs, g_coop_find_theirs or coop_scene_on_of(1) is up. Otherwise it prints `[stayer] THE COUNTDOWN WAITS FOR <what> (G2, not player 2's): PLAYER 1 IN THE BLACK`. It is still always printed, once per window. |
| S16: the deferred items' waits aren't told | **Partly.** Each wait ends when its condition clears. | stayer_p2_left_go: one always-printed line when the first kept item has waited 300 frames after the tail, with what holds it (busy word, use, cutscene, menu, their own words/close-up/find, words in the game's window). Player 1's window-kept item doesn't get one. After the tail it falls into tome_copy_p1_left's non-window path, whose waits are today's and tell nothing today either. The window adds at most its own length. |
| (Found here, the mirror of S4) player 1's second copy close in the window writes over a waiting item | **Real.** tome_copy_p1_close writes g_tome_copy_p1_left without a test, and stayer_p1_presses let player 1 open a second copy while the first one's item waited for the door. | stayer_p1_presses: Start is refused while `g_tome_copy_p1_left.pending` (left to the game's Start lock, like the other refusals) and logged. |
| D6: player 1's no-hold find stops every script timer, the mover's door script's too | **Rejected for now (left to a run).** The door fires on the tick, not a script timer, so the change still goes. What waits is only a later timed wait in the door script itself. The fix (answering bit 2 clear at lr 0x80008494 for player 1's free find) also needs player 1's own timers split, as coop_script_timers splits player 2's. The same stop exists today for any no-hold find of player 1's beside a script of player 2's. Runs S6b/S15 show whether a door script waits. | |
| D9: §8's save prompt is not kept | **Rejected (can't be built as designed).** There is no save prompt id. `ed prompts` lists all 63 prompts and none is a save. `ed how save` shows that saving starts from a region's enter script (room 93's SaveRgn, script 960), which opens Yes/No words, not a prompt M11 can key on. A save in the window would be a Yes in words, which belongs with M13 (a script's later request). Left to the save design, as §8 says. | |
| S14: B1 drops case (a) for player 2's lend-route close-up | **Rejected (as designed).** Design §3.1 skips (a), (b), p1_free and (d) on purpose: answering 0 to every asker in the window also answers 0 to the mover's node update and motion. The stayer's own node passes A1. If run S1/S5c shows an unread asker stalling on bit 4, it gets added to the allow list under stayer_window(cpu, 1); case (a) doesn't come back. | |

## Co-op only (the fixes)

Every changed path still starts from stayer_window / stayer_window_on (switch, split_counting, the mover), or from records
written only inside a window:
- p1_stood is set only under stayer_window(cpu, 0).
- M13's pass-over needs stayer_window of the current stayer.
- g_stayer.look0 and screen_down live in g_stayer, which is zeroed at each ticket after the co-op gate and at a state load.
- Queue entries are made only in a window, or behind one made in a window.
- hle_save_held's new term is 0 outside that.
- The P1 and M11 guards sit inside window branches, and so do the summon guards.
- split_tick's passthrough line is in its not-counting branch, where the flag was already true unless stayer_after_tail's
  guest call ran.

Solo, follow mode, no ED_ROOM_SLOTS, ED_SPLIT_HOLD=0, ED_STAYER_PRESSES=0 and the copy's turn all run today's code.

# Runs (2026-11-13, fresh chapter-1 state, build-run-stayer-presses.sh)

All from logs/run-20261113-16*-stayer-*; every door fired on time (62-67 frames, "0 frames waited"); no hang reports.
| Run | Result |
|---|---|
| S1 player 2 examines and reads through player 1's door | pass: close-up 3828, words 3850 in the window, closed 3900, RETURN 3950; shots: words in the right half while the left is black |
| S2 / S2b player 2's quick spell | the press goes in ("goes in through player 1's door") but "not cast" in and out of the window: chapters 1-2 refuse quick casts (0x800069DC reads 0x80307178; stayer-test-setups.md). Not a stayer bug; casting runs need chapter 3 + ED_CHEAT |
| S3 player 2's Start | the copy opens in the window, its lock cleared (was 55); BUT the door's fade darkens the copy to black (F3) |
| S4 player 2's zone look | pass: close-up at 3870 in the window |
| S5 player 2's exit B kept | pass: kept, goes in at 3894, their own door, both in 180 |
| S5b player 2's pickup | not reached: the spot gives EXAMINE (prompt 6); words in the window pass |
| S6 / S6b player 1 examines, words, across player 2's firing | pass: L1 kept the lock out, B 3770, words 3790 standing, RETURN after; shots right |
| S7 player 1's quick spell | the press goes in; chapter 1 casts nothing (as S2) |
| S8 player 1's Start | the copy opens and closes in the window; it dims with the door's fade too (F3 reader measured it) |
| S9 player 1's zone look then close-up | the close-up opens in the window, but player 2's arrival cancels it (F2): DETAIL dead, body shown, camera stuck |
| S10 player 1's exit B | FAILED, then fixed by hand (b_eat; StayerAgain): see coop-failed-attempts.md; rerun -164514-stayer-s10c passes |
| S11 apart, player 2 stays | pass: close-up in player 1's second door's window, words after |
| S11b apart, player 1 stays | not reached: room 180's Region03 is a ladder that asks (CLIMB UP needs a B) |
| S12 off / S13 follow / S14 solo | no [stayer] lines; solo the same as split-k6-solo |

F2 and F3 are designed in design-stayer-findings.md and built per stayer-findings-impl-notes.md.
