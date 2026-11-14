# Design: the stayer-presses findings F2 (the arrival cancels the stayer's close-up) and F3 (the door's fade in a stayer's copy Tome)

This is a design only. Nothing was built, run or launched. HEAD 5eb040d plus the uncommitted work. The readers are
f2-arrival-closeup-readers.md and f3-fade-tome-copy-readers.md. The runs are stayer-test-setups.md and
build-run-stayer-presses.sh. docs/coop-failed-attempts.md has no entry on either cause. Its R4 fade-mark entry (marks must
be whole words, not low bytes) still applies to F3c and F3d below.

**Revision 2 (2026-11-13).** Folds in refute-stayer-findings-impl.md (points A-K) and refute-stayer-findings-goal.md
(§1.1-§1.7). Each point was checked again in the code for this revision; the verdicts are in the **Review** table at the
end. What changed:
- F2c's gate is the bracket and the switches, not the window (goal 1.4).
- F2b drops its scene exclusion (impl E).
- F2b's "every answer is today's" is limited to before the let-go, with a trace (impl A, goal 1.5).
- F2a's log reads the look by the slot (impl B).
- What a press on a dead look does is now read (impl C).
- New: F3d, R4's forget test keeps the mover through the stayer's own bits (goal 1.2).
- New: F3e, player 2's Start refused in their own door's fade, as the game refuses the hero's (goal 1.3).
- New: F3f, player 1's Start in the fade-in tail opens a copy (goal 1.1).
- New: F3g, an always-printed line for a copy opened in any other fade (impl H).
- New: F3h, R4 forgets the mover at the fade-in's last drawn step (impl I).
- Open items, not in this build: impl D (mechanism corrected), F and G, goal 1.6 and 1.7.

Tags: [read] seen at an address or line, [read there] in a named doc or log, [measured here] taken from the shots for this
revision, [inferred] not seen.

## Summary

| | Cause | Fix | Reused host pieces | Gate |
|---|---|---|---|---|
| F2 | Inside R4's bracket the game's prompt slots and looks are still **player 1's**. The arrival's prompt reset 0x801172B0 (0x80152134, lr 0x80152138) runs the let-go 0x80024CDC(-2, slot), and that cancels player 1's close-up through 0x80024B24(0). Earlier in the same arrival, the host's busy ask reads player 1's look as over (lr 0x80058E84). R4b then has nothing left to keep. | **F2a:** in the bracket, pass over 0x80024CDC for script -2 (the bracket writes those slots back anyway). **F2b:** in the bracket, coop_busy_ask doesn't ask coop_p1_close_up while player 1 is in a close-up, so the look isn't cleared. **F2c:** a flag, set at the bracket's open, from the bracket and the switches. | coop_close_up_kept (the 0x80024CDC intercept, same shape as its lend-route keep); the bracket's slot save and write-back; R4b's stay_busy_clear (now reached, keeps bit 2) | co-op stay, R4's bracket (player 2's change), split installs with ED_SPLIT_HOLD not 0, ED_STAYER_PRESSES allowing player 1, cameras not lent |
| F3 | The copy is a memcpy of real RAM, taken inside the door window, so it carries the door's fade (ticks, target, live RGBA). The Tome frame ticks it at 0x80111790 and draws the quad in a whole frame, where R4's per-view mark has no effect. Around it, R4's mark is dropped under the stayer's own busy bits and is never forgotten after a fade-in. The two players' Starts differ in the fade. | **F3a/F3b:** the copy opened in the owner's stayer window or its fade-in tail starts with no fade, through the game's own setters 0x801E7D24(&0) and 0x801E7C88(&0, 0). **F3c:** R4's mark is not run in a copy's turn. **F3d:** R4's forget test keeps the mover while the stayer's window is on. **F3e:** player 2's Start is refused in their own door's fade. **F3f:** player 1's Start in the fade-in tail opens a copy. **F3g:** a log for any other fade in a copy. **F3h:** R4 forgets the mover at the fade-in's last drawn quad. | the binding-lock clear's gate and place in tome_copy_step; tome_copy_no_window's "left out of the copy" rule; R4's g_fade_mover; stayer_window_on (B2's own bits); the game's fade test 0x801E7C98; stayer_p1_presses' copy route | the copy owner's stayer window or its tail (F3a/F3b/F3f), the window (F3d), player 2's own window as the mover (F3e), with ED_STAYER_PRESSES allowing and ED_SPLIT_HOLD not 0 |

Solo, follow mode, ED_SPLIT_HOLD=0, ED_STAYER_PRESSES=0, the mover's own prompts and the mover's own copy begun before
their ticket all run today's code (see each "must not break" section). F3e's refusal of the mover's own Start in their
window is the one deliberate change on the mover's side.

---

## F2: player 2's arrival cancels player 1's close-up

### F2.1 What happens (from the readers, checked again)

- 0x80151FDC at 0x8015212C: `rlwinm. r0,r30,0,29,29` (flags & 4) branches to 0x80152140. Otherwise
  `bl 0x801172B0` at 0x80152134 (lr 0x80152138), then `0x80163F04(1)` at 0x8015213C [read `ed dis 0x80152100`].
- 0x801172B0 does three things [read `ed dis 0x801172B0`]:
  - zeroes the shown record 0x80335AE0 (r13-29016 := -1, its +8 := -1);
  - calls `0x80116E70(slot, 1)` for slots 0..5;
  - calls 0x80117050 at 0x80117308.
- 0x80116E70 with r4 != 0 runs the callback r13-24024 (bctrl at 0x80116EAC, return 0x80116EB0) on the slot's +12, then zeroes
  the slot (0x80116EC8) [read `ed dis 0x80116E70`].
- That callback is 0x80024CDC. It does nothing unless r3 == -2 (0x80024CE4/0x80024CF4). Slot 2 sets the looks' +64 := 0
  (0x80024D1C-0x80024D28), slot 4 sets it := 1 (0x80024D30-0x80024D3C), other slots leave it. Then `0x80024B24(0)` at
  0x80024D64, the close-up cancel, for any -2 slot [read `ed dis 0x80024CDC`].
- 0x80024B24 sets the indexed look's kind to 0 for kind 1 (0x80024B70) and for kinds 2/3 (0x80024B98, then the hold's
  let-go and the camera pop when its stage is above 0) [read `ed dis 0x80024B24`].
- In R4's bracket (notify_ticket_run 14133-14156, coop_stay_change_end 14157-14191) the identity, the room word and the camera
  set are player 2's. The slots are **saved** (`coop_bytes(..., true)` at 14149), not swapped, and written back at 14161. The
  looks 0x80307108, the busy word and the control channels are not touched [read hle.c].
- The bracket opens for every change of player 2's under co-op stay with player 2 in a room. It does not need a window
  (14143: no g_split test) [read]. So the cancel below also runs in refused windows and with ED_SPLIT_HOLD=0.
- So the cancel's global half lands on player 1: look kind 2 -> 0, busy bit 2 cleared at lr 0x80199F28 (S9 line 999), the
  channels let go, the show, hide-others. Its per-set half (the pop, the snap) lands in player 2's set. The bracket's end
  then writes player 1's -2 slot back over a dead look [read there: S9 page-0001.log 980-1031, shots/000013, 000037].
- Before that, at lr 0x80058E84 (the player handler's busy ask, event 61 of the arrival), coop_busy_ask reaches
  coop_p1_close_up. The window is over (ticket +0 <= 0), so room_bit is 0, and busy 6 & ~(32|1) != 2. It prints
  `[trace] player 1's look over` and sets g_coop_p1_look := false (S9 998). That makes R4b's stay_busy_clear gate
  (`coop_scene_on_of(0) || coop_p1_in_close_up`) false at 0x80152184 [read hle.c 19517-19548, 26276-26278].
- S9 shows one bracket in one game frame: no `gate controller` line between the bracket's open (980) and its end (1022).
  The timebase "frame" moves 3822 -> 3824 inside it [read there]. hle_coop_controller, the other caller of
  coop_p1_close_up, does not run in the bracket [read there]. The behaviour pass (the node updates) starts after the
  bracket ends: coop_stay_change_end runs at its entry (notify_behaviour_pass, lr 0x80008400/0x80050A9C) [read 14192-14194].
- **What today's result is** (impl C, goal 1.4, now read):
  - A press on a kind-0 look does nothing. 0x80024D78 adds 1 to the look's stage (0x80024DBC-0x80024DC0), reads the kind
    (0x80024DCC), and kind 0 falls through 0x80024DD0-0x80024DE0 to `b 0x80025590`, which only returns
    [read `ed dis 0x80024D78`, `0x80025590`].
  - So in S9, player 1's written-back DETAIL is drawn but dead. Their set keeps record 10 pushed (Camera06) with no
    RETURN that could pop it, and the body walks. S9's 000037 (~4300) agrees [read there].
  - A kind-1 zone EXAMINE cancelled the same way is dead too. The game re-asks slots 2 and 4 (a zone's, ed301) only when
    the zone is left or the prompt is used (the game's rule as noted at hle.c 17233-17236) [read there], so it stays dead
    until player 1 leaves the zone.

### F2.2 The close-up begun BEFORE player 2's ticket (the R4b case)

- **The game side is the same.** 0x80117360(0), the door's per-frame prompt mask, zeroes only the shown record. A DETAIL or
  RETURN -2 slot from before the ticket is still in the slots at the arrival [read there: f2 readers §6].
- **But today it is mostly not reachable from player 2's own presses or ways out**:
  - Player 2's B on any prompt is kept while player 1's look is up: `coop_player1_mid_use(cpu)` ->
    `coop_press_keep(..., "player 1's look or find is up")` at hle.c 21403-21404. That comes before M11's `p2_stays` branch,
    so it holds with the stayer presses on too [read].
  - Player 2's ways out (walk-in exits, fn66) wait on `coop_way_out_p1_up`, which includes coop_player1_mid_use (16805-16807) [read].
  - These keeps are tested **at the press**. A door script that waits between its accepted B and its fn66 leaves frames
    in which player 1 can open EXAMINE before the ticket. Script 132 (level 04) holds, then `wait[fn36](120)`, then fn66
    [read `ed script 132 --level 4`]. A script's teleport also comes through neither keep [inferred].
- **Consequence for the planned runs:** `stayer-c3-p1-exam-before` and the ch1 `stayer-s6c-p1-exam-before` (player 1's B
  before player 2's) should show **the keep** at doors whose ticket follows the B at once (183 -> 180, room 153's 1139).
  They stay in the run list as a check of the keep, with player 1's DETAIL/RETURN presses added so the kept door goes
  through after the look (F2.6). At a waiting door (S15/S16's script 132) they would show F2 instead.
- The F2 fix covers every one of these by construction: F2c's flag is set by the bracket, not by when the look began.
- **The wider pre-existing case**: player 1 just **standing in an EXAMINE zone** (kind 1, a -2 slot with no close-up) when
  player 2's arrival fires. The result is a dead EXAMINE until the zone is left (F2.1, read). It is reachable in every R4
  door, ED_STAYER_PRESSES=0 and ED_SPLIT_HOLD=0 included. F2a covers it under F2c's gate. With the switches off it stays as
  today under this workflow's rule; the always-printed WARNING (F2.5) marks it, and the user can choose to widen the gate.

### F2.3 The change

**F2c, the bracket's mark** (beside g_stay_bracket, hle.c ~13760):
```c
static bool g_stay_bracket_p1;  // R4's bracket is player 2's change, player 1's prompts and look are theirs (F2)
```
- Set in notify_ticket_run after `g_stay_bracket = 2;`:
  `g_stay_bracket_p1 = stay_bracket_p1_allowed() && !coop_cameras_lent();`
- `stay_bracket_p1_allowed(void)` is a new one-line helper:
  `{ return g_split_ready && split_switch() != 0 && stayer_switch_allows(0); }`.
  It is forward-declared beside split_counting (3271) and defined after stayer_switch_allows (25044), because
  notify_ticket_run (14133) comes before g_split_ready (24943) and split_switch (24981).
- **Revision 2 (goal 1.4):** the gate no longer asks g_split.on (a window). A window is refused with the presses on for a
  stayer charging a summon (G3) or a cutscene at the ticket (split_ticket_open 25296+), but the bracket still runs then
  (14143 has no window test) and cancels player 1's zone look as in S9. F2a's rule ("inside player 2's change the game's
  slots are player 1's") holds for every non-lent bracket. The switch terms only keep ED_SPLIT_HOLD=0 and
  ED_STAYER_PRESSES=0 (or `p2`) on today's code.
- It is taken once at the open, so the bracket's later parts (after split_over at the tail, 14606) see the same answer.
- Cleared in coop_stay_change_end next to `g_stay_bracket = 0;` (14187), and in the state load next to
  `g_stay_bracket_chose = false;` (30584).
- With ED_SPLIT_HOLD=0, ED_STAYER_PRESSES=0 or `p2`, follow mode (no bracket), solo (no bracket) or the cameras lent, it is
  false and everything below is today's code.

**F2a, the let-go passed over** (coop_close_up_kept, hle.c 19643, the intercept on 0x80024CDC). A new first branch, before
today's `!g_coop_screen` return:
```c
g_hle_passthrough = true;
if (g_stay_bracket != 0 && g_stay_bracket_p1 && g_settings.mod_coop && (s32)cpu->gpr[3] == -2 && !coop_cameras_lent()) {
    // log (F2.5), then skip the call:
    g_hle_passthrough = false; g_hle_tail_call = false; return;
}
```
- Today's function returns at its first test inside the bracket (r13-21492 is player 2 there: S9 `who 59`), so the new
  branch must come first [read 19645-19647].
- The rule: inside player 2's change the game's slots are player 1's (saved at the open, written back at the end). The
  arrival's "reset the player's prompts" is meant for the player who arrives. Player 2's own prompts are host-held
  (g_coop_prompt_slots), and their new room's zone list resets them (coop_zones_new / coop_p2_own_cancel). So no let-go of a
  -2 in the game's slots belongs to this change. This is the lend-route keep in this function ("only player 2's own side
  lets go of it"), mirrored to the bracket.
- Skipping the callback leaves the looks' +64 unwritten. That is wanted: it is player 1's look index [read 0x80024CF8-0x80024D3C].
- What stays as the game does it:
  - 0x80116E70 zeroes the slot after the callback whether or not it ran (0x80116EC8), and 0x801172B0 zeroes the shown
    record. The bracket writes the slots back [read].
  - 0x80117050's freeing of the prompt element. In S9 the element was drawn again from the written-back record
    (shots/000037) [read there]. Whether it is missing for one frame is checked in S9's shots (goal 1.7).
  - Every non -2 callback, which 0x80024CDC returns from at 0x80024CF4 anyway.
- It needs no lr test. Every 0x80116E70(.., 1) caller inside the bracket acts on player 1's slots: the arrival's 0x801172F8,
  and a zone leave 0x801170D4 or an interface open 0x8007B604/0x8007B610 run as "the player" (player 2) [read: `ed fn
  0x80116E70` bl sites]. The log prints the caller, so a run shows which ones fire.
- Not passed over with the cameras lent. The game slots are then player 2's lent ones, and today's code is right for them.

**F2b, the look not read in the bracket** (coop_busy_ask, hle.c 19999):
```c
const bool bracket_p1 = g_stay_bracket != 0 && g_stay_bracket_p1 && coop_p1_in_close_up(cpu);
... || (!bracket_p1 && coop_p1_close_up(cpu)) || ...
```
- **Revision 2 (impl E):** the `!coop_scene_on_of(0)` exclusion is dropped. A close-up can sit inside player 1's scene ("the
  circle: its scene, then DETAIL", hle.c 13125-13126), and coop_p1_in_close_up is true there [read 19415-19425]. With the
  exclusion, today's path would clear the look, and stay_scene_look_again would re-arm it only as kind 3 (17584-17594).
  coop_p1_in_close_up then reads "scene only" and is false, so player 2's B and ways out would stop waiting for player 1's
  look while the game's one close-up stepper is still theirs [read 19421-19424, 21403, 16805].
  - Without the exclusion the look stays on with its kind. stay_scene_look_again sees g_coop_p1_look true and logs
    "still on" [read 17588-17592].
  - R4c's bare scene is untouched: there coop_p1_in_close_up is false, so bracket_p1 is false and today's clear-and-re-arm
    runs [read 19421-19425].
- What it changes: the bracket's asks are answered by the other terms (g_coop_screen..., p1_free, coop_p2_holds,
  split_busy_allows) or left to the game, as today. Only the clear of g_coop_p1_look is gone.
- **Revision 2 (impl A, goal 1.5): this is today's answer only up to 0x80152134.** After it, F2a changes the game's word:
  - from 0x80152138 to 0x80152184 it is 6 where today it is 4;
  - after 0x80152188 R4b writes keep = 6 & ~4 = 2 where today 0x801556CC(-1, 0) leaves 0 [read 26276-26286].
  - So an ask after the let-go that is left to the game sees bit 2 where today it saw 4 or 0.
  - Which asks run there: no direct `bl 0x80155738` site is in 0x80151FDC, 0x80152298, 0x80152614 or 0x80153B6C
    [read there: impl A, disasm.txt]. The callees after 0x80152188 (0x801F6828, 0x80174B84, 0x801E7C88, 0x801E4A80, the
    `later` 0x80153B6C) were not followed. S9's lines 1001-1022 show no ask there, but the gate lines are thinned [read there].
    The node updates run after the bracket (F2.1).
  - After the bracket, busy 2 is player 1's close-up as in any shared-room close-up. coop_p1_close_up reads it held and
    answers 0 for everyone. S6 shows player 2's arrival walk going on under player 1's busy 2 (states 80 -> 81 at 3887 ->
    1 at 3910, the same frames as S9 with busy 0) [read there: both.tsv].
  - R4b's own scene case already runs with a nonzero kept word there (busy 34) [read 26276-26290].
  - **Effect: expected none. It is checked, not assumed:** F2b's trace (F2.5) lists every ask in the bracket after lr
    0x80152138 with its lr, node and answer. S9's player 2 state 80/81/1 frames are compared with today's.
- Rejected: counting `g_stay_bracket` as room_bit in coop_p1_close_up. split_busy_kept is false once the count is out, so a
  held look would make coop_busy_ask answer **0 (not busy)** to the arrival's 0x80058E84 asks, where solo and today give
  the game's word. That would change the mover's arrival [read 19983-20010, 25172-25185].
- After it, R4b works as built:
  - At 0x80152184, stay_busy_clear's gate `coop_p1_in_close_up` is true and the busy word is still 6 (bit 2 not cleared, F2a).
    keep = 6 & ~4 = 2 is guest-called, then written. Event 102 is not sent [read 26276-26290; 0x801556F0-0x80155720 sends
    it only for a nonzero new word].
  - The S9 line `event 102 from player 2's busy set (busy 0x4) to room 180` goes away. That is the same as solo's arrival.
  - After the bracket: busy 2, so coop_p1_close_up reads held. The channels were never let go, so player 1 stands in the
    close-up. Their set comes back with record 10 still pushed (coop_cameras_in(1)), so Camera06 is right. DETAIL works,
    and the close-up ends by the game's own pop on player 1's set, as in S6 [inferred from S6's close at 3880].
  - The change's tail lets go of player 1's node +28 bit 8 (the ticket's event-102 hold; S9 `hold bit := 0 on node 1`)
    [read 14617-14625]. That is not the close-up's hold: 0x80199EA0 holds the walking channels -23060/-23056/-23052 and busy
    bit 2 only [read 0x80199EA0-0x80199F30]. So the close-up still holds player 1.
  - Player 1's hide-others (the game's 0x800255A4, passed through for player 1, 18593-18604) stays until their RETURN, as in
    solo. Player 2's node is not frozen by it in shared-room close-ups of player 1's today, which the user has played
    [inferred from the played core loop; impl K].

**Rejected alternatives**
- *Swap player 2's slots and looks into the bracket, as coop_cameras_lend does.* It is bigger: three host copies, plus the
  lend's own swap on top. It would also reset player 2's host-held prompts by the game, which coop_zones_new already does.
- *Skip 0x801172B0 at lr 0x80152138.* It would leave other let-go callers in the bracket (a zone leave run as player 2)
  cancelling player 1's look.
- *Count the bracket as room_bit.* It would change the mover's arrival answers (above).
- *Gate on g_split.on (revision 1).* It left the dead zone EXAMINE in windows refused with the presses on (goal 1.4).

### F2.4 The mirror and both shapes

- **Player 2 staying, player 1 arriving** (S4, S1, S5b survive today). Player 1's change has no bracket. Player 2's step-B
  close-up lives in host copies the arrival never reaches. coop_zones_new doesn't run, because the stayer's zone list is
  unchanged [read there: f2 readers §7]. Nothing is added.
- **The lend route in that mirror** (impl F; revision 2 corrects "unreachable"):
  - With the cameras lent at player 1's arrival, 0x801172B0 would zero player 2's lent slots, and nothing writes them back.
    coop_close_up_kept keeps only the cancel [read 19643-19655].
  - Player 1's way out waits on `coop_way_out_p2_up`, which includes g_coop_screen (16810), so no ticket comes with the
    screen up [read].
  - But G2b's wait ends at `count <= 1` ("its last count") and gives up after SPLIT_WAIT_LONGEST = 900 frames
    [read 25443-25473, 24942]. So a lend-route screen begun at the last count, or up for more than 900 frames, has player
    1's arrival fire under it. Player 2's DETAIL/RETURN -2 is then gone from their lent slots [inferred].
  - This is pre-existing and not F2's change (R4d is not built). It is recorded as open, with run S5d (F2.6) to see it.
- **Apart** (player 2 in another room, their ladder or door, player 1 in theirs): the same bracket, the same slots, so the
  same fix. Run S11d below.
- **Rejoin** (player 2's door into room 183 while player 1 is in a close-up): the same bracket, so F2a and F2b apply.
  coop_zones_new then builds player 2's list for room 183 with the zone player 1 is using; player 2's B on it is kept by
  coop_player1_mid_use (21403) [read]. Run S11r below (goal 1.7).
- **The mover's own prompts at their arrival**: host-held and untouched. coop_zones_new still makes their new room's list
  (S9 1031). Nothing in F2 reads or writes them.

### F2.5 Logging

- F2a, under `ED_COOP_LOG` or split_log, at most 8 per bracket:
  `[stay] player 2's change passes over the let-go of player 1's close-up prompt: slot N, look I kind K stage S (from 0x<caller>): it stays theirs (frame F)`.
  **Revision 2 (impl B):** the look index I is taken from the slot as the game would: slot 2 -> 0, slot 4 -> 1, any other
  slot -> the looks' +64 (0x80307108 + 64). Kind and stage are read at 0x80307108 + 32 * I (+0 and +8).
- F2b, under coop_trace, once per bracket:
  `[trace] player 1's look kept through player 2's change: not read at lr 0x... (busy B, frame F)`.
  And at most 16 per bracket, every coop_busy_ask in the bracket after the let-go point (the F2a log has fired, or lr
  0x80152138 passed): `[trace] bracket ask after the let-go: lr 0x..., node 0x..., busy B, answered not busy|left to the game (frame F)`.
- At the bracket's end with g_stay_bracket_p1, under `ED_COOP_LOG`:
  `[stay] player 1's close-up carried over player 2's arrival: look kind K stage S, busy B, their camera index I depth D (frame F)`.
  Read after coop_cameras_in(1), so r13-21700 is player 1's.
- Always printed, the detector for today's dead prompt: at the bracket's end, after the slots are written back, a slot
  0..5 whose +12 is -2 while its look (index as above) has kind 0:
  `[stay] WARNING: player 1's -2 prompt (slot N) written back over a look of kind 0: a dead prompt until the zone is left (frame F)`.
  It is not gated on the flag. With the switches off it marks a known dead prompt (F2.1, read), not a guess.
- The existing R4b line `[stay] player 2's change keeps player 1's busy 2: their scene or close-up goes on` must now appear.

### F2.6 Runs (fresh states; build-run-stayer-presses.sh's $L, $ST, $D2; read `ed timeline`, EVENTS.log, page-0001.log first)

| Run | Line | Shows |
|---|---|---|
| **S9** (as is) | build-run line 35 | No `busy r3 2 r4 0 lr 0x80199F28 who 59` and no `player 1's look over ... lr 0x80058E84` at the arrival. The F2a line (slot, look 0 or 1), F2b's lines with the asks after the let-go listed, R4b's `keeps player 1's busy 2`, the carried-over line, no WARNING. Player 2's state 80/81/1 frames as today (3887, 3910). `ed see 3830` / `ed see 4300`: player 1 `doing: close-up`, busy 0x2, camera index 10, not walked from 1334. shots/000013+: the left half on Camera06, DETAIL shown, the body out of view; no frame with the prompt missing. |
| **S9w** (S9, then its words to the end) | S9's line with `ED_INPUT_SCRIPT="3700,3714,0,0,127;3775,3789,0,0,127;3800,3806,0x0200;3850,3856,0x0200;3900,3906,0x0200;3950,3956,0x0200;4000,4006,0x0200"` | DETAIL after the arrival opens the words, RETURN closes them, and the game's own pop puts player 1's camera back. `ed see 4100`: free, busy 0x0, the room's camera. Player 2 walks room 180 throughout. |
| **S9z** (zone EXAMINE ready, no B, at the arrival) | S9's line with `ED_INPUT_SCRIPT="3700,3714,0,0,127;3775,3789,0,0,127;3900,3906,0x0200;3950,3956,0x0200"` | The F2a line with kind 1. EXAMINE still works after: the B at 3900 opens the close-up (`look stage` 1, kind 2). |
| **S9z-off** | S9z with `ED_STAYER_PRESSES=0` | Today's code: the always-printed WARNING, no F2a line, and the B at 3900 does nothing (the dead EXAMINE, read). For the user's gate decision (F2.2). |
| **c3-p1-summon-zone** (a refused window, goal 1.4) | `env $L ED_COOP_P1_QUICK=00020821 ED_RUN_FRAMES=3600 ED_PUT="3080,1,$FOUNT;3080,2,$DOOR" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3140,3146,0x0008;3300,3306,0x0200" sh tools/ed/run.sh $ST stayer-c3-p1-summon-zone` (stayer-test-setups.md §1c's $L, $ST, $DOOR, $FOUNT, $D2) | Counts only if the ticket's line says the window is refused for the summon. Then the F2a line with kind 1 and no WARNING, and the B at 3300 opens the FOUNTAIN close-up. |
| **S6r** (RETURN up at the arrival: stage 3) | `env $L ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3765,3771,0x0200;3775,3781,0x0200;3800,3806,0x0200;3900,3906,0x0200" sh tools/ed/run.sh $ST stayer-s6r-p1-return` | It counts only if the `[stayer] frame 3822` line's `shown` is a -2 slot (RETURN). If the words still type at 3800, move that B later. Then the F2a line, and RETURN at 3900 closes by the game's pop. |
| **S6** (words at the arrival) | line 29 | Unchanged: no F2a line (no -2 slot), words survive, RETURN 3830, close 3880. |
| **S6c** (begun before the ticket: the keep) | stayer-test-setups.md §1c's ch1 line, with player 1's DETAIL/RETURN added: `ED_INPUT_SCRIPT="3740,3746,0x0200;3800,3806,0x0200;3880,3886,0x0200;3930,3936,0x0200"` | `[coop] ... player 1's look or find is up` keeps player 2's B, and there is no ticket while the look is up. After RETURN the kept B goes in and the door runs. With player 1 free at that ticket, no F2 lines. |
| **S15e** (script 132's door, which waits after its B; impl D, G) | design-stayer-presses' S15 line with player 1's EXAMINE pressed after player 2's B and before the ticket, then DETAIL/RETURN after the arrival | Whether player 1's look goes in before the ticket (impl G). If it does: the F2 lines, R4b's keep (the door's bit 2 and player 1's are one bit), then after RETURN `ed see`: player 1 free, busy 0, walking. If player 1 stays held after RETURN, impl D is real: see F2.7. |
| **S11d** (apart, player 1 stays in 183) | stayer-test-setups.md §2's S11c line with `ED_INPUT_SCRIPT="4185,4191,0x0200;4260,4266,0x0200;4300,4306,0x0200;4350,4356,0x0200"` (EXAMINE inside player 2's ladder window, ~4170-4232; DETAIL after the arrival) | The same lines as S9, in the apart shape: `[stay] player 2's change to room 344 runs as theirs`, then F2a/F2b, R4b's keep, no WARNING. Player 1 is still in the close-up at `ed see 4240`. |
| **S11r** (rejoin: player 2's door back into 183 under player 1's close-up) | S11's line (39), then player 2 back through room 180's door into 183 while player 1, EXAMINE-ing in 183, is at DETAIL | F2a/F2b at the rejoin's bracket, player 1's close-up kept; player 2's B on the FOUNTAIN-style zone kept while it is up. |
| **c3-p1-exam-in** (a casting chapter; FOUNTAIN 1274, ed219 on Camera03) | stayer-test-setups.md §1c | The same as S9 in level 3, room 153. Add `ED_INPUT_SCRIPT="3180,3186,0x0200;3250,3256,0x0200;3300,3306,0x0200;3350,3356,0x0200"` to see the words through. |
| **c3-p1-exam-before** | §1c | As S6c: the keep. Add the DETAIL/RETURN presses as above. |
| **Mirror S4, S1, S5b** | lines 20, 13, 24 | Unchanged: player 2's own close-up and words across player 1's arrival. No F2 lines (no bracket). `lent 0` at the firing. |
| **S5d** (lend route at the last count; impl F, observation) | S5c's line with player 2's lever B moved to the countdown's last count (ticket + 61) | Today's state: `lent 1` at the firing; whether player 2's lent close-up loses its RETURN. Not fixed by this build. |
| **Mover's own S5, S10/S10c, S11** | lines 22, 36, 39 | The mover's prompts in the new room: `[coop] player 2's prompt` in 180 after their arrival (S5, S11). No F2 lines where the stayer has nothing up. |
| **Off: S12, S13 follow, S14 solo, and S9 with ED_SPLIT_HOLD=0** | lines 52-55; S9 + `ED_SPLIT_HOLD=0` | No F2a/F2b/carried lines. S12 and ED_SPLIT_HOLD=0 keep today's hold. Solo and follow are byte-for-byte today's paths (no bracket, or no flag). S9 + ED_SPLIT_HOLD=0 may print the WARNING. |
| `ed test` | | the suite |

### F2.7 Open, not in this build

- **Impl D, mechanism corrected.** The refutation said the mover's door script runs a fn21(1) after the arrival. The two
  read scripts don't: script 132 (level 04) has `hold[fn21](0)` and no fn21(1) at all; script 103 (level 01) lets go
  before its fn66 [read `ed script 132 --level 4`, `ed script 103 --level 1`]. What is real: R4b's keep (busy & ~4)
  cannot tell the door's bit 2 from player 1's close-up's. 0x80199EA0 sets and clears flags (0x8013DBC0/0x8013DBC8), not
  counts [read 0x80199EA0-0x80199F10], so player 1's RETURN would let go of both, which is where solo's player would be.
  Whether anything in solo's arrival lets go of script 132's channels was not read. S15e shows it. If it is wrong, the
  fix is in coop_play_hold's script-step keep (13215-13219: it asks only `coop_p1_scene_on()` for player 2's step), not in F2.
- **Impl F:** the lend-route mirror at the last count (F2.4). R4d.
- **Goal 1.6:** a mover's arrival cutscene closes the stayer's copy. tome_copy_step's world_stays needs
  `!cutscene_playing(cpu)` [read 15030-15040], and cutscene_playing is global (the scene flag and the letterbox)
  [read 3862-3868]. That is today's shared-room rule (a cutscene closes player 2's copy). Whether a cutscene in the
  mover's other room should close it is a separate-rooms cutscene question (R6's scenes). A run into a room with an
  arrival scene shows it; no change here.

---

## F3: the door's fade in the stayer's copy Tome

### F3.1 What happens (from the readers, checked again)

- The fade state [read `ed dis 0x801E7C40`, `0x801E7C88`, `0x801E7C98`, `0x801E7D24`, `0x801E7DA4`]:
  - ticks r13-21784, target colour r13-26896, live RGBA floats 0x80602A2C..0x80602A38.
  - 0x801E7C88(colour*, ticks) stores the ticks and the target and nothing else.
  - 0x801E7D24(colour*) stores the target and the four live floats.
  - 0x801E7C98 is "a fade is on": ticks, target bytes or live floats not 0. Its one caller is the Start handler at
    0x80006CF4 [read `ed fn 0x801E7C98`].
  - 0x801E7DA4 steps the live colour toward the target while the ticks last: live + (target - live) / ticks, and stores
    ticks - 1 (0x801E7EA8) before it draws. It draws the quad (matrix at lr 0x801E7F60) only while the alpha 0x80602A38 is
    above 0 (0x801E7ECC-0x801E7ED4) [read 0x801E7DA4-0x801E7F30].
- 0x801E7C40, the game's own full clear, zeroes the live floats and a second block at 0x80602A4C, then saves through
  0x801E7B88. It leaves the ticks and target as they are, so it alone would not stop a door fade. It is not used.
- tome_copy_step's opening memcpys real RAM into the copy (hle.c 15080) inside the window, fade and all. The Tome frame
  ticks the fade at 0x80111790. The copy's frames are whole frames (views skipped, split_skip_views_set), and the marks are
  honoured only in per-view builds (`build_view`, `if (player >= 0)`) [read .deps/GXRuntime/graphics/aurora/lib/gfx/interp.cpp
  707-721]. So the copy owner's half shows the door's fade, and stays black from ticket+60 until the copy closes, because
  the copy never runs the firing's fade-in [read there: f3 readers §1-§5].
- Both copies do it. S3: the right half (player 2's copy) goes 23.6 -> 12.5 -> 1.7 at shots 000012-000014, then 17.9 in
  the world (000015). S8: the left half (player 1's copy) goes 40.7 -> 28.4 -> 17.9 at 000007-000009, then 45.6 after the
  close [measured here: whole-half luma, 8-pixel grid]. 000009 shows the Tome dim beside player 2's door [seen here]. The
  task note "S8 draws unfaded" is not right: it dims and closes before black.
- The game itself never opens a Tome in a fade: 0x80006CDC refuses Start at 0x80006CF4 while 0x801E7C98 is on [read
  `ed dis 0x80006CDC`]. A copy that starts with no fade is therefore the game's Tome as it always starts.
- The host's copy openings never ask 0x801E7C98: player 2's at hle.c 28225 and 28549-28558, player 1's stayer Start at
  28150-28165 [read].
- **R4's mark itself** (notify_GXLoadPosMtxImm, 3417-3425) [read]:
  - It marks the quad while `coop_room_change_on(cpu) || ticks > 0`, and forgets the mover when a quad is drawn with no
    change on and no ticks left.
  - coop_room_change_on needs the busy word to hold nothing but 4, 2 and 1, no cutscene, no menu [read 11401-11406]. B2 made
    split_on allow the stayer's own bits (g_stayer.bits) [read 25016-25024]; this test was not changed (goal 1.2).
  - A fade-in ends with its alpha landing on 0 at ticks 0, so no quad is drawn then: the forget never runs after a door's
    fade-in, and nothing else clears g_fade_mover but the next ticket (11434) [read 0x801E7EA0-0x801E7ED4; grep
    g_fade_mover] (impl I).

### F3.2 The change

**F3a, the copy taken in the owner's stayer window starts with no fade** (tome_copy_step, hle.c 15025+):
- Before the swap, beside the binding-lock block (15084-15094), with the same gate: compute
  `const int owner = g_tome_copy_p1 ? 0 : 1; const bool fade_out = stayer_window(cpu, owner) || stayer_fade_tail(cpu, owner);`
  (F3b). It reads real RAM (split_counting reads the ticket count and the stayer's room; g_tome_copy_turn is set only at
  15174), so it must be taken before `tome_copy_swap(cpu, true)` [read 15084, 15122-15123, 15174].
- After the swap, in `if (opening)` right after `tome_copy_no_window(cpu)`, so the writes land in the copy:
  - read the copy's ticks, alpha and target for the log;
  - write a zero word at the scratch below the stack, `(cpu->gpr[1] - 0x200u) & ~15u` (the pattern at hle.c 6392);
  - guest-call **0x801E7D24(scratch)** (live floats := 0, target := 0) and **0x801E7C88(scratch, 0)** (ticks := 0);
  - guest-call 0x801E7C98 to confirm it returns 0 (F3g uses the same answer).
- The copy's 0x801E7DA4 then returns at 0x801E7ED4 every turn, and no quad is drawn in the copy.
- Nothing comes back from it. At the close only the magick record, the spells, the equips, the script and the cast are taken
  from the copy (stayer-presses-impl-notes R6), and tome_copy_close takes back no fade state [read 14906-14950].
- It covers both players' copies, the fade before the firing, a copy held across the firing (no black at ticket+60, and
  after the close the per-view world replay with R4's mark), and both shapes (the window is split_counting's, shared or
  apart).

**F3b, the fade-in tail.**
- After the firing, the arrival's fade-in (0x801521A8: 0x801E7C88(.., 10)) runs 10 ticks. In S9 both.tsv it is 9 down to 0
  over 3827-3836 [read there]. R4 still marks it out of the stayer's half while g_fade_mover is set (3417-3425).
- Change:
  - In split_over, at `why == "the change's tail"` with `stayer_switch_allows(stayer)`, set
    `g_stayer.fade_tail = stayer + 1`, `g_stayer.fade_tail_tickets = g_coop_tickets` and
    `g_stayer.fade_tail_frame = split_frame(cpu)`. These are new fields. g_stayer is zeroed at the next ticket
    (split_ticket_open's memset) and at a state load (split_state_load) as today [read 25285-25286].
  - `stayer_fade_tail(cpu, owner)` = `g_stayer.fade_tail == owner + 1 && g_coop_tickets == g_stayer.fade_tail_tickets &&
    frame <= fade_tail_frame + 12 && g_fade_mover == (owner == 0 ? 2 : 1) && (s32)r13-21784 > 0`. It is R4's own marking
    rule for the stayer's half, limited to the fade-in of the change that closed that window. It is never widened to
    "g_fade_mover set" (impl I).
  - With ED_SPLIT_HOLD=0 there is no window, so no tail.
- Player 2's Start in the tail opens a copy today (start_room needs coop_room_change_other, false with the count out:
  28461-28467) [read]. F3a clears that copy's fade-in. Player 1's Start in the tail is F3f.

**F3c, R4's mark not run in a copy's turn** (notify_GXLoadPosMtxImm, 3417):
`if (cpu->lr == 0x801E7F60u && g_fade_mover != 0 && !g_tome_copy_turn)`.
- In a copy's turn the quad belongs to a whole frame, where the mark does nothing (F3.1, interp.cpp 708).
- Its forget test reads the copy's RAM there: tome_copy_no_window wrote busy := 0 in the copy (14831), so
  coop_room_change_on is false. A copy quad with ticks <= 0 would zero the global g_fade_mover early, and the real fade-in
  would then come unmarked into the stayer's half [read there: f3 readers §5].
- With F3a no stayer copy draws a quad, so this is a guard. It also covers the mover's own copy begun before their ticket,
  which F3a leaves as today.
- A counter `g_tome_copy.fade_quads` is incremented here in a copy's turn, for the log.

**F3d, R4's mark kept through the stayer's own bits** (goal 1.2; the same test at 3418):
```c
const bool door_on = coop_room_change_on(cpu) || (stayer_window_on(cpu) && !cutscene_playing(cpu));
if (!door_on && (s32)mem_read32(cpu, cpu->gpr[13] - 21784u) <= 0) { g_fade_mover = 0; ... }
```
- Today a stayer with their own scene (32), find (16) or busy-8 words up when the fade-out's ticks are spent has the mover
  forgotten. The ticks run out about 2 frames before the firing (S9 both.tsv: tickets 3, 2, 1 at 3820-3822 with fade 0)
  [read there], and the quad goes on being drawn at alpha 255. The last black frames and the whole fade-in are then drawn
  into **the stayer's** half: what R4 was built to prevent [read 3417-3419, 11401-11406].
- Under G2 it is longer. G2 holds the countdown at a count above 1 while a scene, held words or a find begun in the window
  is up [read split_tick 25436-25446], but the fade ticks on, so the stayer's half would go black for the rest of the wait
  [inferred from the two reads].
- stayer_window_on is split_counting with the switch allowing whoever stays (25051-25055). It is B2's window: the stayer's
  own bits are the reason split_on (25016) stays on. It is false in a copy's turn (split_counting asks g_tome_copy_turn), so
  F3c still decides there. It is forward-declared (3282); cutscene_playing (3862) needs a forward declaration before 3408.
- Not the refutation's `g_split.on && ticket > 0`: that also keeps the mark through a cutscene begun in the window and
  under ED_STAYER_PRESSES=0. Here a cutscene's fade is forgotten as today, and with the presses off stayer_window_on is
  false: today's test.
- Not seen yet: all 24 runs printed `bits 0x0` [read there]. Run S6f shows it.

**F3e, player 2's Start refused in their own door's fade** (goal 1.3; coop_menu_input's play branch, 28549):
- Today start_room refuses player 2's Start only in **player 1's** change (`coop_room_change_other(cpu, true)`) [read
  11409, 28461-28462]. In their own countdown start_pressed is true, and in mode 2 it asks for a copy (28549-28558), with
  coop_p2_own_up the only refusal [read].
- That copy is taken mid-fade, reaches black at ticket+60 and never runs the arrival's fade-in, so player 2's half is a black
  Tome from about ticket+60 until they press B blind, while their body changes room behind it.
- In solo, and for player 1 as the mover, the game refuses Start while the fade is on (0x80006CF4) [read]. The fix does what
  the game does: before `g_tome_copy_ask = true`,
  `if (g_split.on && g_split.mover == 2 && stayer_switch() != 0 && coop_fade_on(cpu)) { log; return; }`
  where coop_fade_on guest-calls 0x801E7C98 in real RAM (it only reads).
- g_split.on is co-op stay with ED_SPLIT_HOLD not 0. stayer_switch() != 0 keeps ED_STAYER_PRESSES=0 on today's code. Follow
  mode and solo never set g_split.on.
- Revision 1 accepted the black in S3m's pass line ("fades as today"). That line is changed (F3.5).

**F3f, player 1's Start in the fade-in tail opens a copy** (goal 1.1; stayer_p1_presses, 28108):
- stayer_p1_presses returns unless `stayer_window(cpu, 0)` [read 28127]. From the firing on, player 1's Start goes to the
  game's 0x80006CDC, which refuses it while the fade-in is on [read]. Player 1's half shows no fade then (R4 marks it out),
  so the press is a dead button for about 10 frames. Player 2's mirror opens a copy (F3b).
- Change: `const bool window = stayer_window(cpu, 0); const bool tail = !window && stayer_fade_tail(cpu, 0);` The early return
  becomes `if ((!b_new && !start_new) || !(window || (tail && start_new)) || !sda_ready(cpu)) return;`, and the B branch
  runs only with `window` (M11's kept B is for the window). The Start branch is as today. It eats the pad's Start
  (start_eat), so the game's handler doesn't also refuse it, and asks for player 1's copy. F3a/F3b clear the copy's fade.
- Not the alternative "keep the press until the fade is off". That would have to wait for both 0x801E7C98 and L1's Start
  lock (r13-26232, put back to 2 at the tail: S9 `the Start lock put back to 2 (was 3)`) [read there]. The copy route
  needs neither.

**F3g, a copy opened in any other fade is logged** (impl H):
- F3a/F3b keep within the switches. A copy taken outside the window and its tail still carries a real fade: a door script's
  fade before its ticket, a fade from the other player's scene or a cutscene's end, or the fade-in after a refused window
  [read 28225, 28461-28467].
- After every copy opening (both owners, any mode), with the answer of 0x801E7C98 in the copy:
  `[tome-copy] player N's copy opened in a fade (ticks T, alpha A; not the stayer window's) (frame F)`, always printed.
  It is a log only: no behaviour changes, follow mode and the switches off included.

**F3h, R4 forgets the mover at the fade-in's last drawn quad** (impl I; the same block):
- After marking, `if (split_switch() != 0 && stayer_switch() != 0 && !door_on && !g_split.on && ticks == 1) g_fade_mover = 0;`
  (ticks read after the tick's store at 0x801E7EA8, so the fade-in's last drawn quad has ticks 1, and the next step draws
  nothing [read 0x801E7EA8, 0x801E7ECC-0x801E7ED4]).
- `!g_split.on`: only after the change's tail. A door's own fade-out passes ticks 1 with the change on, and a cutscene in
  the window has g_split.on.
- Today a later fade with ticks > 0 and no room change (a cutscene's, a script's) is marked out of the old stayer's half
  while it ticks, and cuts to black there only when it holds at full [inferred from 3417-3425]. After F3h it is drawn in
  both halves, as the game draws it.
- F3b's tail is unaffected: stayer_fade_tail needs ticks > 0, and after the forget no quad is left to draw.
- split_switch (24981) needs a forward declaration before 3408. The two switch terms keep ED_SPLIT_HOLD=0 and
  ED_STAYER_PRESSES=0 on today's code; the user can widen it later.

### F3.3 What it must not break

- **The mover's own copy begun before their ticket**: no fade at its opening, so F3a/F3g are no-ops and F3e is not
  reached (no Start). It stays whole through the door as in follow mode's run (hle.c 28456-28458's note) [read there].
- **The mover's Start in their own door**: refused by F3e, as the game refuses the hero's. Before the ticket nothing changes.
- **The Tome copy turn:**
  - The clear runs once, at the opening, in the copy, before TW_MODE_CHANGE, like tome_copy_no_window and the binding-lock
    clear.
  - The Tome code never starts, clears or restores a fade (no 0x801E7C88/0x801E7C40/0x801E7BE4 caller in
    0x80100000-0x80117000) [read there]. TW_MODE_CHANGE 0x80018728 calls no fade start [read there: impl J]. The world's
    restore 0x801E7BE4 runs only at 0x80008578 and 0x80050BBC, never in a copy turn [read there: impl J].
  - A copy begun before the ticket has no fade and is untouched (the gate is false; the clear would be a no-op anyway).
- **Sharing** (a copy from `g_tome_copy.clean`): the clean is taken at player 1's real Tome, which the game opens only with
  no fade. Same code, no-op [inferred].
- **R4's mark in the world** (F3d, F3h): both are behind g_fade_mover (co-op stay only), and F3d's new term behind the
  stayer's window. Solo and follow mode never set g_fade_mover (11433: `coop_stay() && g_coop_p2`).
- **Follow mode, solo, ED_SPLIT_HOLD=0, ED_STAYER_PRESSES=0:** stayer_window, the tail, stayer_window_on and F3e's g_split.on
  test are false (no window, or the switch off). F3c matters only while g_fade_mover is set and a copy draws a quad. F3h
  is behind both switches. F3g only prints.
- **The follow-mode note at hle.c 28455-28458** (a copy opened in the fade went black once the new room was in) has the same
  cause: the copy's own fade [read]. It is left as it is: follow mode stays unchanged, and today's start_room refuses that
  Start. F3a's clear under a follow-mode gate would fix it if that refusal is ever lifted.

### F3.4 Logging

- F3a/F3b, under split_log:
  `[stayer] player N's copy: the door's fade left out of it (ticks T, alpha A, target 0x...; the window|the fade-in) (frame F)`.
- Always printed if 0x801E7C98 still answers 1 after the clear: `[stayer] WARNING: player N's copy still has a fade on`.
- F3g, always printed (above).
- At the copy's close (tome_copy_close), under split_log when fade_out was set: `[stayer] player N's copy drew the fade quad
  K times`. K must be 0. Always printed if K > 0 with fade_out set.
- F3d, under split_log, once per window: `[stayer] the door's fade stays out of player N's half under their own busy 0x%X (frame F)`
  when the new term is what kept the mark (coop_room_change_on false).
- F3e, always printed: `[stayer] player 2's Start refused: their own door's fade is on, as the game refuses the player's (frame F)`.
- F3f, under split_log: `[stayer] player 1's Start in player 2's fade-in: their Tome in a copy (frame F)`.
- F3h, under split_log: `[stayer] the door's fade-in is over: mover N forgotten (frame F)`.

### F3.5 Runs

Measure the mean luma of each half from shots/*.jpg (System.Drawing, whole half). An undimmed Tome half is about 40-45 on
the whole-half grid used here (S8's left after the close: 44-45).

| Run | Line | Shows |
|---|---|---|
| **S3** | build-run line 18 | The F3a line at ~3830. shots 000012-000014: the right half (the copy) stays at its undimmed level, not 23.6 -> 12.5 -> 1.7. The left (mover) fades. The close at 3880, then room 183 normal (000016). `copy drew the fade quad 0 times`. |
| **S3h** (copy held across the firing ~3884) | S3's line with `ED_PAD2_SCRIPT="3830,3836,start;3860,3866,right;3990,3996,b;4010,4016,b"` | The Tome half clear through 3884-3990. After B the right half shows room 183. No early g_fade_mover clear: the left half's fade-in ends ~3894 and the right half is never black. F3h's line once the fade-in is over. |
| **S3t** (player 2's Start in the fade-in tail) | S3's line with `ED_PAD2_SCRIPT="3886,3892,start;3950,3956,b"` | The F3a line naming `the fade-in`. The copy half is never dark at 3886-3896. Counts only if the Start is taken (`[tome-copy] player 2's Tome opens` at ~3886). |
| **S8** | line 34 | The F3a line for player 1 at ~3770. The left half stays at its undimmed level at 000007-000009 (today 40.7 -> 28.4 -> 17.9). |
| **S8h** (player 1's copy across player 2's firing ~3824) | S8's line with `ED_INPUT_SCRIPT="3770,3776,0x1000;3790,3796,0x0002;3900,3906,0x0200;3930,3936,0x0200"` | The left half clear across 3824. Then room 183. |
| **S8t** (player 1's Start in the fade-in tail; F3f) | S8's line with `ED_INPUT_SCRIPT="3826,3832,0x1000;3900,3906,0x0200"` | `[stayer] player 1's Start in player 2's fade-in` and the F3a line naming `the fade-in`. The left half never dark. Without F3f the press goes to the game and is refused. |
| **S3m** (the mover's own Start: F3e) | `env $L ED_RUN_FRAMES=4200 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3760,3770,b;3775,3781,start;3800,3806,start;3860,3866,b" sh tools/ed/run.sh $ST stayer-s3m-mover-copy` | No F3a line. Each Start in the countdown that is read (the one at 3775 may fall in the B's assist window) prints F3e's refusal; no copy opens; player 2's half fades with their door and comes in with room 180. |
| **S6f** (the stayer's own find across the fade's end; F3d) | S6b's line (player 1's pickup) with the presses moved so the find (busy 16) is up at ~3818-3822 | `bits 0x10` on the window lines and F3d's line. shots 3818-3836: the left half never dark. |
| **S11s** (apart, player 2 stays in 183) | S11's line (39) with `ED_PAD2_SCRIPT="4140,4146,start;4170,4176,right;4250,4256,b"` | The same as S3 in the apart shape. |
| **c3-p2-start** (chapter 3, a page and a cast from the copy) | stayer-test-setups.md §1c | The F3a line. `[coop] spell ... cast by` from the copy if the page's B casts. The Tome half clear. |
| **c3-p1-start** (chapter 3, player 1's copy in player 2's door) | `env $L ED_RUN_FRAMES=3600 ED_PUT="3080,1,$FOUNT;3080,2,$DOOR" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3170,3176,0x1000;3190,3196,0x0002;3205,3211,0x0200;3230,3236,0x0200;3270,3276,0x0200" sh tools/ed/run.sh $ST stayer-c3-p1-start` | The mirror of c3-p2-start (S8's presses moved by the ticket, ~3160). |
| **c3-p2-quick, c3-p2-810, c3-p2-summon, c3-p1-quick** | stayer-test-setups.md §1c | Regression of the casts beside F2/F3: casts or keeps as before, no F2/F3 lines. |
| **Arrival scene** (goal 1.6, observation) | a door into a room with an arrival cinematic, the stayer's copy open at the firing | Whether `the world asks for something else` closes the copy. No change in this build. |
| **Off: S12, S13 follow, S14 solo, S3 with ED_SPLIT_HOLD=0** | | No F3 lines but F3g's (if a copy opens in a fade). S12 and ED_SPLIT_HOLD=0: player 2's Start refused in the window as today. |
| `ed test` | | |

---

## Order of building and the facts it rests on

1. F2c (the flag and its helper), F2a (one branch in coop_close_up_kept), F2b (one term in coop_busy_ask, no scene
   exclusion), and the F2 logs with the corrected look index and the asks-after-the-let-go trace.
2. F3a, F3c, F3b (a split_over record and a helper), F3g, and the F3 logs.
3. F3d (the forget test), F3e (player 2's Start in their own fade), F3f (player 1's tail Start), F3h (the forget at the
   fade-in's end). Forward declarations for cutscene_playing and split_switch before 3408.
4. Runs: S9, S9w, S9z, S9z-off, S11d, S6r, S3, S3h, S3t, S8, S8h, S8t, S3m, S6f first. Then c3-p1-summon-zone, S15e, S11r
   and the chapter 3 lines. Then the off, follow and solo lines and `ed test`. Add any attempt that fails to
   docs/coop-failed-attempts.md with its code.

| Fact | Where | Tag |
|---|---|---|
| flags & 4 skips 0x801172B0 and the waits clear; 0x801172B0 at 0x80152134, lr 0x80152138 | 0x8015212C-0x8015213C | [read] |
| 0x801172B0: shown record, 0x80116E70(slot, 1) x6, 0x80117050 | 0x801172B0-0x8011731C | [read] |
| 0x80116E70: callback r13-24024 (bctrl 0x80116EAC) before zeroing the slot | 0x80116E70-0x80116EF8 | [read] |
| 0x80024CDC acts on -2 only; slot 2 -> +64 := 0, slot 4 -> +64 := 1, others leave it; 0x80024B24(0) at 0x80024D64 | 0x80024CDC-0x80024D74 | [read] |
| 0x80024B24: kind 1 -> 0 (0x80024B70); kinds 2/3 -> 0 then let-go (0x80024B98) | 0x80024B24-0x80024BC0 | [read] |
| a press on a kind-0 look adds to the stage and returns | 0x80024DBC-0x80024DE4, 0x80025590 | [read] |
| the bracket opens without a window; saves the slots, doesn't swap slots or looks | hle.c 14143-14161 | [read] |
| coop_close_up_kept is the 0x80024CDC intercept; it returns when r13-21492 is player 2 | hle.c 19643-19655 | [read] |
| coop_busy_ask's coop_p1_close_up term answers 0 for everyone | hle.c 19999-20003 | [read] |
| coop_p1_in_close_up reads "scene only" for kind 3 with the scene on and no words | hle.c 19415-19425 | [read] |
| stay_scene_look_again re-arms only kind 3, and only with the look off | hle.c 17584-17594 | [read] |
| stay_busy_clear: R4b's gate and keep; the stayer's bits 8/16/32 | hle.c 26260-26290 | [read] |
| 0x80199EA0: channels set/cleared by 0x8013DBC0/0x8013DBC8, then busy 2 | 0x80199EA0-0x80199F30 | [read] |
| the tail lets go of the stayer's node +28 bit 8 | hle.c 14617-14625; S9 1015-1016 | [read], [read there] |
| split_over runs at the change's tail, after the arrival; the bracket's end is at the behaviour pass | hle.c 14606, 14192-14194; S9 1006 after 999-1003 | [read], [read there] |
| g_split_ready and split_switch are defined after notify_ticket_run | hle.c 24943, 24981 vs 14133 | [read] |
| player 2's B and ways out wait for player 1's look, at the press | hle.c 21403-21404, 16805-16807 | [read] |
| script 132 waits 120 after its hold, has no fn21(1); script 103 lets go before fn66 | `ed script 132 --level 4`, `ed script 103 --level 1` | [read] |
| coop_play_hold's script-step keep asks only the other's scene | hle.c 13209-13219 | [read] |
| G2b ends at the last count or after 900 frames | hle.c 25443-25473, 24942 | [read] |
| fade setters and test; 0x801E7C40 leaves ticks and target; the tick's last step and draw test | 0x801E7C40-0x801E7ED4 | [read] |
| the game refuses Start in a fade; 0x801E7C98's one caller | 0x80006CF4 | [read] |
| copy opening: memcpy, the binding-lock gate, swap, tome_copy_no_window; g_tome_copy_turn set later | hle.c 15076-15128, 15174 | [read] |
| world_stays needs no cutscene; cutscene_playing is global | hle.c 15030-15040, 3862-3868 | [read] |
| R4's mark and its forget test; coop_room_change_on's bits | hle.c 3408-3426, 11401-11406 | [read] |
| split_on allows the stayer's own bits; stayer_window_on | hle.c 25016-25024, 25051-25055 | [read] |
| marks honoured only in per-view builds | interp.cpp 707-721 | [read] |
| player 2's Start: refused only in player 1's change; asks a copy in their own | hle.c 28461-28472, 28549-28558 | [read] |
| player 1's stayer presses need the window | hle.c 28127 | [read] |
| the copies dim with the door; S3/S8 luma | logs/run-20261113-163010/-163149 shots 000006-000016 | [measured here] |
| the bracket is one game frame (no controller in it) | S9 page-0001.log 980-1022 | [read there] |
| player 2's arrival walk is unchanged under player 1's busy 2 | S6 both.tsv 3831-3910 | [read there] |
| answering 0 at the arrival's 0x80058E84 would change the mover's arrival | from 19983-20010 and 25172-25185 | [read] |
| F3d under G2: the stayer's half black for the wait | from 3417-3419 and split_tick 25436-25446 | [inferred] |

---

## Review: the two refutations, point by point

Each point was checked again in the code or the logs for this revision.

| Source | Point | Checked | Verdict | In the design |
|---|---|---|---|---|
| impl A / goal 1.5 | F2b's "every answer in the bracket is today's" is false after the let-go (6 then 2 where today 4 then 0) | 26276-26286; S9 1001-1022; the node updates come after the bracket (14192) | **Real** (the claim). Effect expected none. | Claim limited to before 0x80152134; F2b's trace lists the later asks; S9 compares player 2's 80/81/1 frames |
| impl B | F2a's log reads the look by +64, which the skipped function would have written | 0x80024CF8-0x80024D3C | **Real** | Index from the slot (2 -> 0, 4 -> 1, else +64) |
| impl C / goal 1.4 (read part) | a press on a kind-0 look does nothing; today's S9 DETAIL and zone EXAMINE are dead | 0x80024D78-0x80024DE4, 0x80025590; 0x80024B70 | **Real** | F2.1 "What today's result is"; WARNING text names a dead prompt |
| impl D | a door that holds through its ticket lets go of player 1's carried close-up after the arrival (fn21(1)) | script 132 has no fn21(1); script 103 lets go before fn66; 0x80199EA0 sets/clears | **Mechanism refuted**; the merged bit 2 under R4b is real, its effect not read | F2.7 open; run S15e |
| impl E | the scene exclusion in F2b drops a close-up inside player 1's scene to kind 3 "scene only" | 19415-19425, 17584-17594, 21403, 16805 | **Real** | Exclusion dropped |
| impl F | the lend-route mirror can fire at the last count or after 900 frames | 25443-25473, 24942 | **Real**, pre-existing, not F2 | F2.4 corrected; F2.7 open; run S5d |
| impl G | the keep is at the press; a waiting door lets player 1's look in before the ticket | 21403; script 132's wait 120 | **Real** for the predictions only; the fix covers it | F2.2; run S15e |
| impl H | copies outside the window and tail still carry a real fade; the host never asks 0x801E7C98 | 28150-28165, 28225, 28549; 0x80006CF4 | **Real** | F3g (log); F3e for the mover's own case |
| impl I | g_fade_mover is never forgotten after a fade-in | 0x801E7EA8, 0x801E7ECC-0x801E7ED4; grep g_fade_mover | **Real**, pre-existing | F3h (gated); F3b kept to its bounds |
| impl J | the chain, the intercept, the slots, event 102, split_over's place, F3a's setters and place | as listed there | **Holds** | unchanged |
| impl K | F2a keeps player 1's hide-others across the arrival | 18593-18604 | **Holds** as solo; player 2 not frozen by it in the played core loop [inferred] | F2.3 note |
| goal 1.1 | player 1's Start in player 2's fade-in tail is refused by the game; player 2's opens a copy | 28127, 25006-25010, 0x80006CF4, 0x801521A8; S9 both.tsv | **Real** | F3f |
| goal 1.2 | R4's mark is dropped under the stayer's own 8/16/32 | 3417-3419, 11401-11406, 25016-25024 | **Real** (worse under G2's wait) | F3d, with stayer_window_on, not `g_split.on && ticket > 0` |
| goal 1.3 | player 2's own copy in their own door goes black; S3m accepted it | 11409, 28461-28472, 28549-28558 | **Real** | F3e; S3m's pass line changed |
| goal 1.4 | F2's gate leaves the zone EXAMINE dead in refused windows with the presses on | 14143 (no window test), 25296+ | **Real** | F2c from the bracket and the switches; run c3-p1-summon-zone |
| goal 1.6 | a mover's arrival cutscene closes the stayer's copy | 15030-15040, 3862-3868 | **Real mechanism**; policy open (today's shared-room rule) | F2.7 open; an observation run |
| goal 1.7 | rejoin, stage 3, the prompt element's frame | 21403, 14161; S6 | **Maybe**: covered by construction, runs settle it | runs S11r, S6r, S9's shots |
| task note | "S8 draws unfaded in the left half" | S8 shots 000007-000009 | **Refuted**: it dims 40.7 -> 17.9 and closes before black [measured here] | F3.1 |
