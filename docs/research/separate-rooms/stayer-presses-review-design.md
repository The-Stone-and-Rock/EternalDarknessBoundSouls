# Review: the stayer's presses (Steps A and B) against the design and the disassembly

Read only. Nothing was changed, built or run. Sources: runtime/host/hle.c (working tree), runtime/host/main.c,
build/ed-db/disasm.txt (`ed dis`), design-stayer-presses.md (final, §13, §15), stayer-presses-impl-notes.md.
[real] = I read the code that proves it. [maybe] = reasoned from the code, not proven to happen.

## Findings, most serious first

### 1. [real] B5: after player 1's look in player 2's door, their turn comes back from before the door
hle.c:20586-20593 and 20627.

- What is wrong: `p1_stood` (the flag that makes the next room_turn start from the game's copy) is set only when
  `p1_stands && !p1_look && !p2_hold && coop_room_change_other(cpu, false)`. Player 1's look sets `p1_look`, so a look
  never sets it. B3 now keeps that look up through the window. Line 20590 also clears `p1_stood` whenever
  `coop_room_change_other(cpu, false)` is false, and that happens mid-window under the stayer's own bits 8, 16 or 32
  (their own scene, which B2 lets run).
- The failure: player 1 stays and player 2 takes OPEN DOOR. Player 1 presses EXAMINE, then DETAIL, then RETURN inside
  the window. room_turn is off through the look (`!p1_look`) and comes back with `room_was` false and `p1_stood` false.
  So `g_coop_p1_from_free` is true and coop_player1_turn (20025-20032) loads `g_coop_p1_free`. That copy is saved only
  at busy 0 (20538-20542), which is before the ticket. Player 1's stick and buttons go back to how they were before
  player 2's door: the exact case design §3.5 and refuter point G-1.13 wanted closed. Player 1's own scene (bit 32) in
  the window does the same through the clear at 20590.
- The fix: set `p1_stood = true` on every frame room_turn is off for the stayer's own reason in the window
  (`stayer_window(cpu, 0) && (p1_stands || p1_look)`), with no `!p1_look` and no room-change test. Clear it when the
  window is over (`!split_counting(cpu)`) or once room_turn has used it at 20631. Do not clear it on
  `!coop_room_change_other`.

### 2. [real, small effect] split_tick does not set g_hle_passthrough again after stayer_after_tail's guest call
hle.c:25312-25316 (split_tick's not-counting branch), 25519 (the guest call of 0x800069DC).

- What is wrong: split_tick sets `g_hle_passthrough = true`, then calls stayer_after_tail, which guest-calls
  0x800069DC. That call goes through the nested dispatches: coop_quick_cast_ask on 0x800069DC, then the cast's own
  hooks. Each nested dispatch sets the global again (hle.c:5621-5623), and hle_guest_call does not save it
  (main.c:61-112 saves only CPUState). split_tick then returns with whatever the last nested intercept left. G2's own
  guest call has the comment "Set after the guest call, which may dispatch hooks of its own" (25369-25371) and sets
  the flag after. This branch does not.
- The failure: player 1's summon was kept from player 2's door and is cast at the first quiet tick. If the last nested
  hook handled its call, the dispatcher calls hle_return for 0x80155830. That frame's tick and notify_change_tick are
  skipped. With +0 at 0 the tick does nothing (0x80155848 goes to 0x80155918), so the harm is small today. It breaks at
  once if the tick ever gets work at +0 == 0.
- The fix: in split_tick, after `stayer_after_tail(cpu);`, add `g_hle_passthrough = true; g_hle_tail_call = false;`.

### 3. [real, logging] M13's always-printed loss line is missed when the stayer's own bits are up
hle.c:16875-16880.

- What is wrong: the line is printed only inside R4's branch, `coop_stay() && coop_room_change_on(cpu)`.
  coop_room_change_on is false under 8, 16 or 32 (11388-11394). B2 now keeps the room playing under the stayer's own
  scene, so this case is new. Design §5.3 puts the line under `stayer_window(cpu, who)`, not under R4.
- The failure: player 2 stays at player 1's door. Their own scene (bit 32, in g_stayer.bits) has a script that reaches
  fn66 in the window. R4 is skipped, fn66 runs on, and G1 refuses the ticket (split_ticket_ask: `[split] a second room
  ticket refused`). The `[stayer] A ROOM ASKED FOR BY PLAYER N'S SCRIPT ...` line never prints, so S17-style runs and
  the user's check 7 can't find it.
- The fix: print the line in its own test before R4, under `stayer_current(cpu) >= 0 && stayer_window(cpu, stayer)`.
  Optional, beyond the design: also pass over under `split_counting(cpu)` there, so the stayer's fn66 doesn't half run
  (let-go, motion, fade) before G1 refuses its ticket.

### 4. [maybe] Player 2's kept Tome item can lose its first step: use_script/use_item are not kept with it
hle.c:14951-14958 (the keep), 27935-27944 (stayer_p2_left_go), 28680 (`g_tome_copy_use_script == script`), 28749 (the
clear), 18852-18855 (the global is set by any copy's item use).

- What is wrong: player 1's deferred item keeps `use_script` and `use_item` in its own record (14886-14887).
  `g_stayer_p2_left` keeps only script, next_cast and spell_item. tome_copy_left reads the globals
  `g_tome_copy_use_script` and `g_tome_copy_use_item` when it finally runs.
- The failure: the script waits until the window is over and nothing is busy. In that gap, any copy Tome item use
  writes the globals: player 1's copy in a close-up of player 2's, or player 2's next copy opened while busy is still up
  after the arrival. The deferred run then sees `use_script != script`. It skips the first COOP_RUN_SCRIPT(script,
  item), and the item's real effect is lost. A second deferral in a later window before the first has run also
  overwrites the one slot (14953).
- The fix: keep `use_script` and `use_item` in g_stayer_p2_left at the close, and put them back into the globals just
  before stayer_p2_left_go calls tome_copy_left. If one is already pending, run the old one first or queue it.

### 5. [maybe] L1 checks "the game's copy is player 1's" only by !g_coop_turn
hle.c:25465.

- What is wrong: coop_events_words (22192-22210, flag `g_coop_events_own`) also swaps player 2's input copy
  (g_coop_state[1]) into the game's words outside their turn. A ticket written there, with lr 0x801557F8, would pass
  L1's test. L1 would then keep the binding lock out of player 2's own copy, and player 2 is the mover.
- I found no ticket written from inside coop_events. 0x800443E4's other callers are 0x800C35F4 (event 202) and
  0x8011105C, and the lr check excludes both. So this is a guard, not a seen failure.
- The fix: add `&& !g_coop_events_own` to L1's test.

### 6. [maybe] Player 1's no-hold find in the window stops every script timer, the mover's door script included
hle.c:19764-19765 (player 1's no-hold find sets r13-24048 bit 2), with the main loop's 0x8011727C(2) at 0x80008490
before the timers.

- B4 now opens player 1's find with no hold inside player 2's countdown. Its bit 2 stops all script waits (see the
  comment at 19771-19777), so any wait in the mover's door script after its fn66 waits for player 1's find. The
  change itself fires on the tick, not a script timer, so the door still goes. The effect depends on the door script.
- The fix, if a run shows it: answer bit 2 clear at lr 0x80008494 under stayer_window(cpu, 0) for player 1's free
  find. That is the same branch coop_flags_ask already has for player 2's find, and player 1's own timers then need
  coop_script_timers' kind of split.

### 7. [maybe, logging] G2's always-printed line names player 2 for any wait
hle.c:25330-25337.

- `extra` holds every bit begun in the window (25321), and a cutscene sets `cut`. With player 1 the mover, words
  opened by player 1's own door script after fn66, or any cutscene, print `THE COUNTDOWN WAITS FOR PLAYER 2'S
  WORDS/CUTSCENE ... PLAYER 1 IN THE BLACK`.
- Player 2's lend-route words and finds write 8 and 16 directly (19624-19625, 19746-19747), not through 0x801556CC,
  so g_stayer.bits can't tell whose bit it is either.
- The fix: name player 2 only when `g_coop_words_theirs`, `g_coop_find_theirs`, `coop_scene_on_of(1)` or G2b is up.
  Otherwise print "the countdown waits (not the stayer's)".

### 8. [maybe] Player 1's kept summon can be cast while their copy Tome is open
hle.c:25505-25513.

- stayer_after_tail waits for busy, cutscene, menu and words, but not for `g_tome_copy_open && g_tome_copy_p1`.
- The world pad is emptied while that copy is open (4396-4402). A summon's body switch would take player 1's character
  in the real game behind their Tome.
- The fix: add `!(g_tome_copy_open || g_tome_copy_ask)` to its wait.

### 9. [real, disclosed] §8's save prompt in the window is not kept
The design asks for a save prompt to be kept as an exit is. It was not built because its prompt id isn't known
(impl notes, "Not confirmed"). A stayer's B on a save prompt inside the window goes in, and opens a game mode during
the other's countdown (coop_room_change_on goes off: 11393). That is the one design line not built, and the notes say
so.

## Each piece checked (no problem found unless listed above)

- **stayer_window, stayer_window_on, switch** (24920-24945): `stayer_switch_allows(who) && split_counting &&
  g_split.mover == other`. g_split.on is set only after split_ticket_open's co-op gate (25173). The copy's turn is
  excluded by split_counting (24897). Solo, follow mode, no ED_ROOM_SLOTS, ED_SPLIT_HOLD=0, ED_STAYER_PRESSES=0 and the
  copy's turn are all false. The stayer's node is split_stayer_node, so player 1's is r6_player1_node. No new code
  reads r13-26524 for player 1's node.
- **g_stayer** (3291-3307): zeroed in split_ticket_open after split_over (25171, 25176), so L1's put-back runs first,
  and zeroed in split_state_load (25446). prompts0 is read at the writer's entry, before 0x80117260(0x10000, 1) at
  0x801557E0 [disasm]. Word0's bits go in only when the stayer alone has a thing up (25220-25223), as §1 says.
- **L1** (25463-25479): lr 0x801557F8 matches `bl 0x800443E4` at 0x801557F4 [disasm]. 0x800443E4 is void: it is
  0x800068E0(r3) then 0x8013E99C(r3) (0x800443F8, 0x80044400). The writer loads r3 = 0 at 0x801557F8, so nothing is
  read back. 0x800068E0 is a leaf unsigned max into r13-26232 [disasm 0x800068E0-0x800068F4]. G2's own call has lr
  0x0BADF010 (main.c:52) and passes. The flag is set after the guest call. Installed with intercept and notify both
  NULL required (30155-30157) and the bl at +0x1C and the writer's bl checked. It doesn't call its own entry, so it
  needs no running guard.
- **Start lock put back** (25155-25165): `now <= lock_n && now > solo`, lowered to `max(0, lock_n - frames)`. A larger
  count is left. It runs whether or not logging is on.
- **P1, flags ask** (19785-19793): lr 0x800084E0 matches `bl 0x8011727C` at 0x800084DC [disasm]. It answers
  `word & r3 & ~door` through hle_set_u32, handled. The timers case (lr 0x80008494) is unchanged. Bit 4 is not masked
  (§15).
- **P1, chooser wrapper** (20165-20189): 0x80117628 has one caller, 0x800084F8 [disasm grep], after the play channel at
  0x800084F4. It is one wrapper round today's body. `g_prompt_choose_running` guards every nested guest call of
  0x80117628: the wrapper's own, and today's look and close-up branches. The bits are put back with `| (word & door)`,
  and the flag is set false after the guest call. The install is unchanged (same function name).
- **Zone lifts**: player 1 at 20412-20413 (refusal lifted), 16787 (`|| stayer_window(cpu, 0)`; its only readers are
  20426 and 20478), 20440-20443 (zones on, put back at 20463-20464). Player 2 at 21266-21276 (look lifted), 16781 (the
  only reader is 16934), 16978-16979 (zones on, put back by coop_words_back of r13-24040 + 2060 bytes, which covers
  -23040).
- **B lifts**: player 2's B through coop_player_turn (21296-21323). M11 keeps an exit (21305-21312). Its release waits
  in the window (21341). Player 1's B goes by the press route (stayer_p1_presses, 27945-27970), and
  coop_p1_press_kept takes no new B in the window (27883).
- **Quick casts and summons**: player 2's wait is `(room_other && !quick_lifted) || coop_left_behind` (21377-21379), as
  §2.6 says. coop_quick_cast_ask (19961-19978) keeps a summon by r4's word, refuses it (handled) and always prints it,
  and lets anything else through. Player 2's kept summon is cast by coop_quick_cast_one at a turn outside the window
  (12527-12533). Player 1's is cast by stayer_after_tail (finding 2).
- **G3** (25102-25115): with the switch, only `(spell & 0x1FF0) == 0x820` holds the window. The mask and class match
  0x801C43D8 and 0x801C4438 (impl notes' reads).
- **stayer_cast_end** (25486-25500): lr 0x80155800 matches `bl 0x801C5118` at 0x801557FC [disasm]. It is void, and
  the caller ignores r3. It skips only when r13-21488 is the stayer's node +16, and prints the skip itself.
  notify_cast_trace stays on the entry (30163) and runs on passthrough only (5624-5633). Coexistence works in either
  install order: the trace's install needs only notify NULL (29464).
- **Start for both**: player 2 at 28043 (the shares block, already mode 2) and 28279-28290 (start_room lifted only with
  mode 2, always printed otherwise). Player 1 at 27972-27985 (mode 2 opens the copy, otherwise always printed). While
  player 1's copy is open, the world pad is emptied (4396-4402), so player 1 stands in the world.
- **The copy's lock** (15058-15069): right after the memcpy, with the real memory still in (`sda` is gpr[13],
  15002). It runs under the copy owner's window.
- **Copy close deferrals**: player 2's at 14951-14963 (stayer_window is read after `cpu->ram = real`, 14929; the close
  runs after `g_tome_copy_turn = false`, 15168, 15193). Player 1's at 28524-28541 (cast at busy 4/2/1 only, not a
  summon; the script waits for the non-window path's coop_busy). See finding 4.
- **M11**: player 1 at 27954-27965 (shown record +4 == 2, +8 id, +12 script; B cleared from the pad), player 2 at
  21305. State-load marks: 25437-25445.
- **M12** (16857-16874): the first test in coop_script_go_to_room, under the window only. `waits` is today's, and
  today's order (R4, follow, waits) is kept below it.
- **M13's line**: finding 3.
- **B1** (19894-19900): after split_busy_kept, and only split_busy_allows answers.
- **B2** (24906-24914): today's test while bits is 0. It calls nothing that writes.
- **B3** (19442, 20557, 19850-19852): the room bit, no hold-open for player 2's turn, and coop_player_held(1). See
  finding 1 for the room_turn side.
- **B4** (19478-19518, callers 19529, 19537, 20048): the room bit is masked only with room_ok, and the door's hold
  (word0 & 2) is never let go.
- **B5**: finding 1.
- **B6** (18400-18409, 28200-28209): busy_before from coop_use_start (18314). The closeup and script-hold clear test
  `busy & ~4`.
- **G2b** (25325-25329): `mover 1 && allows(1) && !screen0 && g_coop_screen`. The always-printed line has finding 7's
  label problem.
- **Logging** (§7): every listed line exists. The always-printed ones (summon waits, both START REFUSED lines, M13's
  line, COUNTDOWN WAITS) use plain fprintf. The ED_STAYER_LOG=1 frame line (25525-25551) prints every field §7 lists.
- **State load** (25429-25449): it zeroes g_split, g_stayer, g_stayer_p2_left and the marks. It drops M11 keeps and M12
  waits only when they still match their marks. It is called at 30359.
- **R6**: as the notes say, not settled. The deferral is as designed: counts settled at the close by coop_tome_player
  in the copy's memory, and the script is run later.

## Not silently dropped
C2 and M13 (beyond its line) were left out as the task said. R2 (player 2's chooser) waits for run S4, as designed.
The save prompt (finding 9) is the one design line not built, and the notes say so.
