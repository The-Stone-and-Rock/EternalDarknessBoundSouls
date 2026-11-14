# Review: the stayer's presses (steps A and B), co-op safety for both players and solo

Read only: nothing was changed, built or run. Read: runtime/host/hle.c (working tree), runtime/host/main.c (hle_guest_call),
design-stayer-presses.md (final), stayer-presses-impl-notes.md. Line numbers are the working tree's.
[real] = I read the code that shows it. [maybe] = the code allows it; whether it happens needs a run or a read not made.

## What holds (checked, no change needed)

- **Solo, follow mode, no ED_ROOM_SLOTS, ED_SPLIT_HOLD=0.** Every new path goes through split_counting (24896: g_split.on,
  !g_tome_copy_turn, mod_coop, g_coop_p2, +0 > 0, the stayer still in g_split.room). g_split.on is set only in
  split_ticket_open (25169) after the gate on g_split_ready, mod_coop, g_coop_p2 and coop_stay(), and after the
  ED_SPLIT_HOLD=0, cutscene and G3 refusals. The two new intercepts (stayer_lock, stayer_cast_end) are installed only inside
  the split hold's install block (30150-30166), and they pass through outside a window. g_stayer is zeroed at 25176, after
  the co-op gate.
- **ED_STAYER_PRESSES=0.** stayer_switch_allows is false everywhere. split_on stays today's because g_stayer.bits stays 0
  (24909). split_stayer_casting returns `spell != 0` (25114). G2b is gated on the switch (25325). B6 uses coop_busy (28201,
  18401). The M11 release term is false (21341). `g_coop_p1_from_free = !room_was && !p1_stood` is `!room_was`, because
  p1_stood is set only under p1_stands (20592). coop_prompt_choose calls today's body, whose `running` static became
  g_prompt_choose_running with the same meaning. The cast-trace notify on 0x801C5118 still runs on passthrough.
- **The Tome copy's turn.** split_counting is false in it, so stayer_window is false in it too. stayer_after_tail,
  stayer_p2_left_go and tome_copy_p1_left test g_tome_copy_turn (or are only called with it false, 15335). tome_copy_close
  runs only after g_tome_copy_turn is cleared (15168, 15193).
- **L1** (25463): only with lr 0x801557F8, !g_coop_turn and stayer_window(cpu, 0). G2's renewal call (lr 0x0BADF010)
  passes through. The mover's copy never held this lock at player 2's prompt door (today the lock landed in player 1's
  copy), so dropping it gives the mover nothing new.
- **K1/K7 and B1** (19884-19900): split_busy_kept runs first. In the window only split_busy_allows answers 0, and A1 leaves
  out the mover. Cases (a), (b), p1_free and (d) are skipped, so the mover's node update and motion get the game's word.
- **B3's hold-open skip** (20557): player 2's turn as the mover in state 121 runs with the busy word as the game has it,
  as it did before B3, when p1_look was false at player 2's door.
- **Each player's words.** split_stayer_node uses r6_player1_node or g_coop_p2, never r13-26524. stayer_current and
  stayer_cast_end compare the current or main id with the stayer node's +16, so they are right with player 2's words
  swapped in or not. coop_quick_cast_ask's `p2` is read from r13-21496 inside coop_be_player2.
- **G2/G2b.** Both share g_split.waiting and the 900-frame SPLIT_WAIT_LONGEST cap (25339). The THE COUNTDOWN WAITS line is
  always printed once per window for mover 1 (25330), and the 900-frame "goes on under it" line is always printed.
- **The Start lock.** It is put back in split_over (25155), which runs at the tail (14593) and at a new ticket (25171). A
  state load brings back RAM. If a ticket is cancelled without a tail, lock_set stays until the next ticket, but r13-26232
  still counts down whenever the play channel runs, so Start is never held for good.
- **Kept presses.** The summons, the M11 B and the M12 way out all end. The summons are dropped by the next ticket
  (memset) or a state load. M11 and M12 are dropped by their own tickets rule, or by the marks at a state load
  (25437-25445). Each is released after the window by its own release path.

## Problems

### 1. [real] Player 1's room_turn comes back with old input after a pause that B5's own test did not cause

hle.c:20586-20599, 20627. p1_stood is set only when `p1_stands && !p1_look && !p2_hold` (20592). It is cleared on any frame
where `!coop_room_change_other(cpu, false)` (20590). With B3, player 1's look stays up through player 2's door, so
`p1_look` is true and room_turn is off, but p1_stood is never set. A scene of player 1's (bit 32), which B2 now lets run in
the window, makes coop_room_change_on false, and that clears p1_stood every frame. When the look or scene ends inside the
window, room_turn starts again with room_was false and p1_stood false. Then `g_coop_p1_from_free = true` (20627), and
coop_player1_turn loads g_coop_p1_free (20028-20030). That copy was saved only at busy 0 (20538), so it is from before the
ticket.

- **Failure.** Player 1 stays at player 2's door. EXAMINE opens at 3770 inside the window (3760-3822), or a lever's scene
  begins. It ends before the firing. Player 1's next turn runs from their input copy and controller state from before
  3760. This is the stale replay the design's G-1.13 marked real, and B3/B2 make it reachable for the first time (before
  them, a look at player 2's door was read "over" at once, and player 1 could not press).
- **Fix.** Record any pause in the window, whatever caused it:
  `if (stayer_window(cpu, 0) && !room_turn) p1_stood = true;` placed after room_turn is computed. Clear p1_stood only when
  `!stayer_window(cpu, 0) && !coop_room_change_other(cpu, false)`, not on every frame where the room change reads off under
  the stayer's own bits.

### 2. [real] stayer_after_tail's guest call can leave the tick skipped

hle.c:25312-25316, 25505-25523. split_tick sets `g_hle_passthrough = true` and then calls stayer_after_tail. That function
guest-calls 0x800069DC (25519) and returns. split_tick then returns without setting the passthrough again.
hle_guest_call (main.c:57-) saves and restores the CPU span, but not g_hle_passthrough. Every nested dispatch inside the
cast writes that flag (hle.c:5617-5619). split_tick's own G2 path says this at 25369: "Set after the guest call, which may
dispatch hooks of its own."

- **Failure.** The frame player 1's kept summon is cast, after player 2's door, the last nested intercept may leave the flag
  false. Then the dispatcher returns at once (5620-5623), and 0x80155830 and notify_change_tick don't run that frame. With
  +0 at 0 this probably costs only one tick frame, but the world frame's tick is skipped.
- **Fix.** In stayer_after_tail, after the guest call, set `g_hle_passthrough = true; g_hle_tail_call = false;` (or set it
  in split_tick right after the stayer_after_tail call).

### 3. [real] Player 2's deferred copy item doesn't keep which item the copy's Use asked for

hle.c:3310-3315, 14951-14962, 27943, 28680-28683. g_stayer_p2_left keeps script, next_cast and spell_item, but not
g_tome_copy_use_script and g_tome_copy_use_item. tome_copy_left reads both globals when it runs (28680:
`if (g_tome_copy_use_script == script)` runs the first step with use_item). In the deferred case they are read later, at
stayer_p2_left_go. Player 1's version does keep them (g_tome_copy_p1_left.use_script and .use_item, 14886-14887, 28569).

- **What changes them in between.** Any copy Use (18853). Any other tome_copy_left with a script, which writes 0 at 28749.
  Player 2's own copy can open again before the deferred run: tome_copy_step opens a copy at the loop's top (copy_ran), and
  stayer_p2_left_go runs only when `!copy_ran` (15334-15336).
- **Failure.** Player 2 uses item A from their copy in player 1's door. The copy's Use takes its count (the impl notes'
  R6: coop_kinds out of the copy at the close), and A's script waits. After the tail the busy word or player 1's words
  (r13-25960) hold it back for some frames. Player 2 presses Start, uses item B, and closes: B runs at once and resets
  g_tome_copy_use_script to 0. Then A's script runs without its first step, so A is used up and its first state is never
  asked with A. If B shares A's script, A's step runs with B's item id.
- **Fix.** Keep `use_script` and `use_item` in g_stayer_p2_left at the deferral. In stayer_p2_left_go, put them back into
  g_tome_copy_use_script and g_tome_copy_use_item just before tome_copy_left. Also refuse a new copy of player 2's while
  `g_stayer_p2_left.pending` (their Start at 28043 and 28373, and the Tome-asking quick cast at 12586), with a line, or run
  the pending script first.

### 4. [real] A second deferral writes over the first, so its item is lost

hle.c:14951-14957. The deferral writes g_stayer_p2_left without testing `.pending`.

- **Failure.** Player 2 closes their copy in a window with item A's script kept. Before stayer_p2_left_go can run, they
  close a copy again inside a window. That can be the same window stretched by a G2 wait (up to 900 frames), or the next
  window when apart, with the busy word kept up by player 1's room. Item B's record replaces A's. A's count was already
  taken in the copy, and its script never runs: the item is lost.
- **Fix.** Same as 3: no second copy of player 2's while one is pending, or a small queue. At the least, print a line
  every time when a pending record is overwritten.

### 5. [maybe] A save can be taken between the deferral and its script, so a load loses the item

hle.c:30343-30351 (hle_save_held). Saves are not held while `g_stayer_p2_left.pending` (nor `g_tome_copy_p1_left.pending`,
which is not new). The design's §8 says "Nothing of this straddles a save".

- **Failure.** Player 2's copy closes in player 1's door with an item script kept. A save is taken while the script waits
  (busy or player 1's words). The saved state has the copy's count taken (if the count lands in the guest own record)
  and no script. Loading it drops the pending record at split_state_load (25435), and the item is gone with nothing done.
- **Fix.** Add `g_stayer_p2_left.pending` to hle_save_held. A pending kept summon could be added too, though losing it
  costs no item.

### 6. [maybe] The deferred item script can run in another room

hle.c:27935-27943. stayer_p2_left_go waits on split_counting, coop_busy, r13-25960, coop_p2_own_up, the copy and a use, but
not on player 2 still being in the room where they used the item. g_stayer_p2_left is also kept across new tickets on
purpose (impl notes, departure 5).

- **Failure.** Apart. Player 1 arrives and reads words (r13-25960), so player 2's kept key-item script waits. Player 2's own
  turn is not held by player 1's words, and they walk out by a way out or a door of their own. The kept script then runs
  in their new room (coop_stay_room_in puts the new room in), and can open the wrong door or nothing. The item is used up.
- **Fix.** At the deferral, keep `coop_object_room(cpu, g_coop_p2)`. In stayer_p2_left_go, if it differs, don't run the
  script: print loudly every time, and either give the count back with coop_kinds or (simpler) refuse player 2's own room
  requests while the script is pending.

### 7. [maybe] P1 can show and press player 2's lent prompts with player 1's B

hle.c:19785 (coop_flags_ask) and 20166 (coop_prompt_choose wrapper). Neither tests `g_coop_cameras.lent`. When player 2's
cameras and prompts are lent in, the game's shown record is theirs (coop_p1_press_kept and stayer_p1_presses both test
`!g_coop_cameras.lent` for that reason, 27879 and 27958). The camera lend is given back only once `!coop_busy` (28237), so a
lend that is up at player 2's ticket stays up through the window.

- **Failure.** Player 2's ladder Yes comes from a lend-route close-up, the cameras are still lent at the ticket, and player
  1 stays. P1 unmasks, the chooser chooses from the lent set, and L1 lets player 1's play-channel B reach 0x80006EBC on
  player 2's prompt. Player 2's script then runs as player 1's, in player 2's door. Today the door's 0x10000 hid every
  prompt for that window.
- **Fix.** Add `!g_coop_cameras.lent` to both P1 tests. Keep the bit while lent.

### 8. [maybe] Player 1's M11 B is taken from their words or look

hle.c:27955-27966. The B branch of stayer_p1_presses keeps any new B while the shown record is an exit prompt (with
`on_show`). It does not test g_coop_words_free, g_coop_find_free, g_coop_p1_look, coop_p1_scene_on or r13-25960 the way the
Start branch does (27975-27976).

- **Failure.** Player 1 is in no-hold words (B4) while standing in an OPEN DOOR zone, and the shown record still holds the
  door's prompt. Their B meant for the words is cleared from the pad (27960), so the words don't go on. An OPEN DOOR is
  kept and goes in after the tail, and player 1 leaves the room without having asked to.
- **Fix.** In the B branch, take the keep only when none of player 1's look, words, find or scene is up (the same test as
  the Start branch). Otherwise leave the B in the pad.

### 9. [maybe] B4 reads word0's bit 2 as the door's hold when it can be player 1's own look

hle.c:24947 (split_door_holds_play), 19490. g_split.word0 is the busy word at the ticket. Its bit 2 is player 1's look hold
whenever player 1 is already in a close-up as player 2's ticket is written.

- **Failure 1 (bit 2 is player 1's).** Player 1's DETAIL words in the window take "the door's own hold stays", and their
  own look's hold is never let go at the words. That is a different path from today's words in a look (where the let-go
  ends the look). It is probably harmless: R4b and p1_look then carry it. But the log line names the wrong owner, and S15
  and S16 can't tell a door that holds from a look that held.
- **Failure 2 (the mirror).** A door that sets its hold just after its ticket has word0 without bit 2. Player 1's words in
  the window then run the let-go: busy 2 cleared, the game copy's walking channels rewritten, r13-25696 0, zones on,
  r13-24048 &= ~4, all under the mover's door in the split shape (the design's S15 "must not show").
- **Fix.** Keep the stayer's own hold apart from the door's. At the ticket, record `g_stayer.look0 = g_coop_p1_look ||
  coop_scene_on_of(0)`, and make split_door_holds_play `(word0 & 2) && !look0`. Later, a per-player hold bit (coop-todo
  "Half").

### 10. [maybe] G2b keys on screen0 only, so a second close-up of player 2's in the same window isn't waited for

hle.c:25325. `!g_stayer.screen0 && g_coop_screen`.

- **Failure.** Player 2 had a lend-route close-up up at player 1's ticket (screen0 true). It ends in the window, and they
  open another one, which sets only bit 2, so G2 masks it. G2b doesn't wait, and player 1's arrival cuts it (R4d is not
  built).
- **Fix.** Track a rising edge: at each tick, `if (!g_coop_screen) g_stayer.screen_down = true;` and wait when
  `g_coop_screen && (!screen0 || screen_down)`.

### 11. [maybe] The kept-summon line can print every frame

hle.c:19964-19972. The summon branch prints with an unconditional fprintf on each call. The press is "not taken" (the
same return as today's refusal). If the play channel's quick row calls 0x800069DC again on each frame the button stays
down, the always-printed line repeats about 60 times a second for the hold, and g_stayer.kept is counted each time.
Harmless to play, bad for EVENTS.log.

- **Fix.** Print and count only when `!g_stayer.summon[who].on || g_stayer.summon[who].spell != spell`.

### 12. [maybe] Player 1's kept summon is cast with nothing of player 2's or a copy tested

hle.c:25506-25513. stayer_after_tail waits on coop_busy, cutscene, menu, r13-25960 and player 1 being current. It does not
test g_tome_copy_open or g_tome_copy_ask (player 1's own copy, opened by stayer_p1_presses after their summon press),
g_coop_use.active or g_coop_screen.

- **Failure.** Player 1 presses a summon, then Start in the same window, so their copy opens. After the tail the summon is
  cast in the real game while their Tome copy is up and they stand. The real game casts it, but the cast state (64) and its
  words come with the copy open.
- **Fix.** Add `g_tome_copy_open || g_tome_copy_ask || g_coop_use.active || g_coop_screen` to the wait.

### 13. [maybe, hardening] Player 2's kept summon doesn't test the copy's turn itself

hle.c:12527. `!split_counting(cpu)` is true in the Tome copy's turn. I found no path that runs coop_player_turn inside
tome_copy_step's turn, so this is only a guard for the task's "never in the copy turn". Add `&& !g_tome_copy_turn`.

### 14. [maybe] B1 drops case (a) for player 2's lend-route close-up in player 1's door

hle.c:19894-19900. Under stayer_window_on, a close-up of player 2's with the game's camera (g_coop_screen, busy 2) no
longer gets "not busy" for everyone. That was right for the mover's node (the point of B1). But askers of 0x80155738 that
are not on the allow list (A1 nodes of the room, A2 the stayer's events) now get 4|2. Solo never runs a close-up with
busy 4 up.

- **Failure.** Player 2 stays and opens a lend-route close-up. The words' tick or the stepper, through a busy ask that
  isn't A1-A3, waits on bit 4 and stalls until the tail.
- **Fix.** None until run S1 or S5c shows it. If it stalls, add those lrs to the allow list under stayer_window(cpu, 1)
  rather than bringing case (a) back.

### 15. [maybe, logging] The M13 loss line misses fn66 asked under the stayer's own bits

hle.c:16875-16880. The always-printed line sits inside R4's branch, which needs coop_room_change_on. Under the stayer's own
8, 16 or 32 (which B2 now lets run), R4 is not reached, fn66 goes on, and G1 refuses the second ticket with its own
always-printed line (25248). So the loss is still printed, but not as the M13 line. Print the M13 line from split_ticket_ask
too when stayer_current is the stayer's.

### 16. [maybe, logging] The deferred copy items' waits are neither capped nor told while waiting

hle.c:27935-27939 and the window branch of tome_copy_p1_left (28524-28541) print only when the deferred item finally runs.
- **Fix.** Print once when a deferred item has waited more than about 300 frames after the tail, with the reason that
  holds it (busy, words, own up, copy). The task asks for every kept thing to end. These end only when their conditions
  clear, which today is always eventually, but nothing is printed if they don't.

## R6 (the copy Tome's Use and player 2's list), only as far as the deferral needs

The code matches the implementers' reading. At the copy's close, tome_copy_close sets `cpu->ram = g_tome_copy.copy` and
calls coop_tome_player(cpu, false) there (14900, 14918-14922). That takes player 2's counts out of the copy before any
script, so a count the copy's Use took is already off when the script is kept. The deferral moves only the script, so the
item can be lost (3, 4, 5, 6 above) but never doubled. Nothing calls tome_copy_left twice with the same script: the first
call at 14961 passes script 0 and returns at 28615. Not settled, as before: whether a count written into a guest `own`
record lands in the copy's memory and is dropped. Also, coop_kinds' host counts are not in the co-op state
(hle_coop_state_save, 30258-), so any state load leaves them as they were. That is not new to the window.

## Summary for the implementers

Must fix before runs:
- 1: the stale room_turn after a look or scene pause.
- 2: the tick's passthrough after the summon cast.
- 3 and 4: the deferred item's use_script and use_item, and overwriting a pending record.

Worth fixing with them:
- 5: hold saves while an item is pending.
- 7: P1 with cameras lent.
- 8: M11's B under player 1's words.
- 11: the summon line spam.
- 12: the summon cast's guards.

Left to runs: 6, 9, 10, 13-16.
