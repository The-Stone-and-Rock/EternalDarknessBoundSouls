# Build notes: the stayer-presses findings F2 and F3 (design-stayer-findings.md revision 2)

Built in runtime/host/hle.c in the doc's order: F2c, F2a, F2b and their logs (with the always-printed WARNING), then F3a,
F3c, F3b and F3g, then F3d, F3e, F3f and F3h. It builds and links clean with the VS2022 tools: the only non-deprecation
warning is the old one at hle.c 5672. **Nothing was run.** No game launch, no run.sh or jtg.sh, no `ed test`. Every
behaviour below is unconfirmed until build-run-stayer-findings.sh has run. Not built, as the doc marks them open: impl D,
impl F (R4d) and goal 1.6. controls.cpp was not touched, and nothing was committed.

The line numbers are hle.c as it stands after this build.

## The design's citations against the code

Before writing, each cited line and address was checked against the current file and `ed dis`:
- **Addresses:** all hold.
  - 0x80116E70 calls the callback r13-24024 at 0x80116EAC with r3 = the slot's +12 and r4 = the slot, then zeroes the slot.
  - 0x80024CDC: slot 2 sets +64 := 0, slot 4 sets +64 := 1, then `0x80024B24(0)` at 0x80024D64.
  - 0x801172B0 runs the six 0x80116E70(slot, 1) calls from 0x801172F8 (lr 0x801172FC).
  - 0x80024D78 reads the kind at +0 and the stage at +8. Kind 0 goes to 0x80025590.
  - 0x801E7C88 stores the ticks and the target. 0x801E7D24 stores the target and the live floats at 0x80602A2C. 0x801E7C98
    is the fade test. 0x801E7EA8 stores the ticks, and 0x801E7ED4 is the alpha test.
- **Line numbers:** they hold to within a few lines (uncommitted work had moved some). notify_ticket_run 14133, the
  bracket's end 14157-14191, coop_close_up_kept 19643, coop_busy_ask 19977, stay_busy_clear 26276, R4's mark 3417,
  split_counting 25006, stayer_switch_allows 25044, split_over's tail call 14606, stayer_p1_presses' window test 28127, the
  state load's `g_stay_bracket_chose` 30584.
  - Player 2's play-branch Start was at ~28562, not 28549.
  - The sharing-path Start was at ~28437, not 28225.
- **One departure from the design's plan:** the design asks for forward declarations of cutscene_playing and split_switch
  before 3408 (R4's mark). The code instead moves R4's whole fade-quad block into `r4_fade_quad`, defined after
  stay_bracket_p1_allowed (25458). Only that function is forward-declared (3287). The reason: the new tests also read
  `g_split.on`, `g_split.mover` and g_stayer, which are defined at ~24975. A struct can't be forward-declared the way a
  function can, so moving the block avoids a set of one-line accessors. Behaviour is the same.

## Each piece

### F2c: the bracket's flag
- **Declarations** (13768-13777):
  - `g_stay_bracket_p1`;
  - `g_stay_bracket_p1_log`, which holds `let_go`, `kept_said`, `passed` and `asks` (the log counters for one bracket).
- **Helper:** `stay_bracket_p1_allowed()` is forward-declared at 3285 and defined at 25439. It returns
  `g_split_ready && split_switch() != 0 && stayer_switch_allows(0)`, as designed.
- **Set** in notify_ticket_run right after `g_stay_bracket = 2;` (14163) as `stay_bracket_p1_allowed() && !coop_cameras_lent()`.
  The log record is zeroed at the same point.
- **Cleared** in two places:
  - in coop_stay_change_end, next to `g_stay_bracket = 0;` (14228);
  - in the state load, next to `g_stay_bracket_chose = false;` (30815).

### F2a: the let-go passed over
- **Where:** a new first branch in coop_close_up_kept (19743-19767), the 0x80024CDC intercept.
- **The skip:** under `g_stay_bracket != 0 && g_stay_bracket_p1 && mod_coop && sda_ready`, a call with r3 == -2 and the
  cameras not lent is skipped (passthrough false, no tail call), so +64 is not written.
- **The let-go mark:**
  - Every callback in the bracket whose back chain holds lr 0x80152138 sets `let_go`. That is the arrival's 0x801172B0, and
    the chain is read with guest_backchain_contains.
  - Every pass sets it too.
  - The trace of asks after the let-go keys on this flag.
- **The log:** at most 8 per bracket, under split_log (ED_COOP_LOG or the R6 log).
  - The look comes from the new `stay_slot_look` (14178): slot 2 is look 0, slot 4 is look 1, any other slot uses +64 & 1
    (impl B).
  - "from 0x..." is the caller of 0x80116E70, read from its LR save at r1+20 (0x80116E7C stores it there). Expected values
    are 0x801172FC (the arrival's reset), 0x801170D8 (a zone leave) and 0x8007B608/0x8007B614.

### F2b: the look not read in the bracket
- **Rename:** coop_busy_ask's body is now `coop_busy_ask_answer` (20102). The installed name `coop_busy_ask` (20146) is a
  wrapper that calls it and then writes the after-let-go trace.
- **The answer:**
  - `bracket_p1 = g_stay_bracket && g_stay_bracket_p1 && coop_p1_in_close_up(cpu)`, with no scene exclusion (impl E).
  - The coop_p1_close_up term becomes `(!bracket_p1 && coop_p1_close_up(cpu))`.
  - The first term is now named `screen_term`. Its evaluation order and short-circuit are as before.
- **The traces** (both under ED_COOP_TRACE):
  - `[trace] player 1's look kept through player 2's change: not read at lr ...`, once per bracket. It prints only when
    the ask actually reached the skipped term.
  - `[trace] bracket ask after the let-go: lr, node (r31; r28 also printed, since the node register differs by lr), busy,
    answered not busy|left to the game`, at most 16 per bracket. The answer is read from g_hle_passthrough after the call.

### F2.5: the WARNING and the carried-over line
Both are in coop_stay_change_end (14189-14202), right after the slots are written back. coop_cameras_in(1) has already
run, so the camera words are player 1's.
- **WARNING**, always printed: a slot 0..5 whose +12 is -2 while its look (stay_slot_look) has kind 0.
- **Carried-over line**, under ED_COOP_LOG with the flag set, when F2a passed at least one prompt or coop_p1_in_close_up
  is true. It gives the kind and stage of the +64 look, the busy word, the camera index (r13-26864) and the depth
  (r13-21700).

### F3a / F3b / F3g: the copy's fade
- **Fade constants and helper** (15067-15083): FADE_* constants and `fade_on_now()`, which guest-calls 0x801E7C98. None of
  the fade functions is hooked, so no running guard is needed.
- **Taken before the swap,** in tome_copy_step's opening, right after the memcpy (15171-15179). With g_tome_copy_turn still
  false, it records `g_tome_copy.fade_out`: 1 for stayer_window(owner), 2 for stayer_fade_tail(owner), else 0. It also
  zeroes `g_tome_copy.fade_quads`. Both fields are new, at 13525-13529.
- **Applied after the swap:** `tome_copy_fade(cpu)` (15088-15122) runs right after tome_copy_no_window (15223), so it acts
  on the copy.
  - With fade_out set: a zero word goes at `(r1 - 0x200) & ~15`. Then it calls 0x801E7D24(&0) and 0x801E7C88(&0, 0).
    Under split_log it writes `[stayer] player N's copy: the door's fade left out of it (ticks, alpha, target; the window|the fade-in)`.
    If 0x801E7C98 still answers 1, it writes `[stayer] WARNING: player N's copy still has a fade on`, always printed.
  - With fade_out 0 (F3g): if 0x801E7C98 answers 1, it writes `[tome-copy] player N's copy opened in a fade (...; not the
    stayer window's)`, always printed. Nothing is changed.
- **The tail record** (F3b), in split_over (25424-25433): at `why == "the change's tail"`, with the switch allowing the
  stayer, it records `g_stayer.fade_tail`, `fade_tail_tickets` and `fade_tail_frame`. Those are new g_stayer fields
  (3314-3318), zeroed by the existing memsets at the ticket and the state load.
- **`stayer_fade_tail`** (25443-25448): all of the design's terms, plus `!g_tome_copy_turn` and sda_ready.
- **The close line** (14948-14952), at the top of tome_copy_close so it covers both owners' copies:
  `[stayer] player N's copy drew the fade quad K times`. It prints under split_log when fade_out was set, and always when
  K > 0.

### F3c / F3d / F3h: R4's fade quad (r4_fade_quad, 25458-25499; called from notify_GXLoadPosMtxImm 3426-3429)
- **F3c, in a copy's turn:** the quad is counted (fade_quads) and nothing is marked or forgotten. **Departure:** the quad is
  counted whether or not g_fade_mover is set. The design's count sits under the old `g_fade_mover != 0` test. Counting
  every copy quad makes "K must be 0" a stricter check.
- **F3d:** `door_on = coop_room_change_on || (stayer_window_on && !cutscene_playing)` replaces the old forget test. When the
  mark was kept by the new term alone (the change off, ticks <= 0), it writes `[stayer] the door's fade stays out of player
  N's half under their own busy 0x..`. That line prints once per window, through stayer_say kind 20 (unused before).
- **F3h:** after the mark, under `split_switch() != 0 && stayer_switch() != 0 && !door_on && !g_split.on && ticks == 1`,
  the mover is forgotten, with `[stayer] the door's fade-in is over: mover N forgotten` under split_log.

### F3e: player 2's Start refused in their own door's fade
- **Where:** coop_menu_input's play branch (28779-28784), as a new `else if` between step D's refusal and
  `g_tome_copy_ask = true`.
- **The test:** `g_split.on && g_split.mover == 2 && stayer_switch() != 0 && !g_tome_copy_turn && sda_ready && fade_on_now()`.
  It always prints `[stayer] player 2's Start refused: their own door's fade is on, as the game refuses the player's`.
- **Departure (scope kept):** F3e is not put in the sharing-path opening (~28437, player 2's copy beside player 1's real
  Tome). The design names the play branch only. A copy opened there in a fade is still logged by F3g.

### F3f: player 1's Start in the fade-in tail (stayer_p1_presses, 28340-28354 and the Start branch)
- **The early return:**
  `if (!b_new && !start_new || !sda_ready) return; window = stayer_window(cpu, 0); tail = !window && stayer_fade_tail(cpu, 0); if (!(window || (tail && start_new))) return;`
- **The B branch** needs `window`.
- **The Start branch** is as today: it eats the Start (start_eat) and asks for player 1's copy. In the tail its log is
  `[stayer] player 1's Start in player 2's fade-in: their Tome in a copy`.
- **Small addition:** in the tail with ED_COOP_TOME_COPY != 2, the "PLAYER 1'S START REFUSED" line is not printed. The pad
  is not eaten there, so the game's own refusal stands, as today.
- **Kept:** the hand-made b_eat/start_eat fixes in this function and the StayerAgain code near coop_press_drop are
  untouched.

## Gating (solo, follow mode, the switches off)
- **F2:** every new path needs g_stay_bracket, which is co-op stay only, and g_stay_bracket_p1.
  - g_stay_bracket_p1 is false with ED_SPLIT_HOLD=0, with ED_STAYER_PRESSES=0 or `p2`, without the two-slot installs, or
    with the cameras lent.
  - The WARNING is the one exception: it prints in any bracket. The design asks for that, as the detector for today's
    dead prompt.
- **F3a/F3b/F3f** need stayer_window or the tail, which needs g_split.on or a split_over at the tail, and so the switches.
- **F3c/F3d/F3h** sit behind g_fade_mover (co-op stay only), or a copy's turn for the count.
  - F3d's new term needs stayer_window_on.
  - F3h needs both switches.
- **F3e** needs g_split.on with player 2 as the mover and ED_STAYER_PRESSES on.
- **F3g** only prints. A copy exists only in co-op.

## What is unconfirmed (to read in the runs)
- **All of it.** None of the expected lines has been seen.
- **The let-go mark** is set only by the 0x80024CDC intercept. If the arrival's callback r13-24024 is ever not 0x80024CDC
  (0x80117320 sets it), `let_go` is never set. Then F2b's after-let-go trace stays silent, although F2a has nothing to
  pass over either.
- **F2b's "effect expected none"** after the let-go (bit 2 left in the word): the trace lists the asks, and S9's player 2
  state 80/81/1 frames are to be compared with today's (3887, 3910).
- **F3f's copy for player 1 in the tail** runs after the window: world_stays, the copy-route assumptions and its "over player
  2's close-up" log line come from the window case. S8t shows whether it opens and closes cleanly.
- **F3e covers only the window** (g_split.on). After the tail, player 2's Start in their own fade-in (~10 frames) is not
  refused. A copy taken then carries the fade-in, and F3g reports it.
- **F3h in a refused window:** with the stayer's own busy 8/16/32 up and no window (g_split.on false), door_on is false.
  The mover is then forgotten at ticks == 1 of the door's fade-out, one frame before today's forget at ticks <= 0. That
  is not expected to be visible.
- **Run timings that are guesses** (marked in the script): S6f (the find up at ~3818), S11r (the rejoin through 180's
  Region02, script 29, and player 1's EXAMINE in its window), and the ch3 lines' T0 = 3050.
- **Not runnable here** (comments in the script): S15e (a level-4 state for script 132's door), S5d (no S5c lever line yet),
  and the arrival-scene observation (goal 1.6).

## Review fixes

The two reviews (stayer-findings-review-design.md, -safety.md) were checked point by point in hle.c and `ed dis`. The fixed
ones are below, then the rejected ones with the reason from the code. It builds and links clean again with the VS2022 tools:
the only non-deprecation warning is still the old one at hle.c 5676. **Nothing was run.** The line numbers are hle.c after
these fixes.

### Fixed
- **Design 1, F3h without the split installs** (25529). F3h's test now starts with `g_split_ready &&`. Both switch functions
  only read the environment, and g_fade_mover is set at any co-op stay ticket (11437). So with ED_ROOM_SLOTS=2 but the split
  installs refused, F3h forgot the mover on every door.
- **Design 2, the tail's last frame** (3321-3322, 25466-25481, 25532-25533). F3h forgets the mover at the quad with ticks 1,
  after the store at 0x801E7EA8. On the next frame the fade is still on: ticks 1, and its last step at 0x801E7DA4 hasn't run.
  So player 1's Start went to the game's refusal (0x80006CF4), and player 2's copy opened with `fade_out` 0.
  - F3h now records the mover it forgot and the tickets count (`g_stayer.fade_forgot`, `fade_forgot_tickets`).
  - `r4_fade_forgot(cpu, mover)` is true only for that mover, the same tickets, g_fade_mover 0 and ticks exactly 1. That is
    the one step left.
  - stayer_fade_tail accepts it in place of `g_fade_mover == mover`. g_stayer is zeroed at the next ticket and at a state
    load as before.
- **Design 7 / safety 7, the tail's frame bound** (25476). 12 timebase frames became 60. split_frame is timebase/675000, and
  at 16 fps the 10 ticks take ~37 of them. F3h (with the forgot step) ends the tail at the fade-in's last quad anyway. The
  tickets, the mover and `ticks > 0` terms stay, so it is still not "g_fade_mover set".
- **Design 3, the binding-lock clear in the tail** (15207). The copy's input-lock clear now tests `g_tome_copy.fade_out != 0`,
  which is set just above. So the lock clear and the fade clear share one gate: the window or its fade-in tail, as the
  design's "the same gate" asks. In the window it is the same test as before (fade_out 1 is stayer_window).
- **Design 4 / safety 10, the WARNING made exact** (13782-13788, 14176-14179, 14210-14218).
  - At every bracket's open, with or without the flag, the code saves the two looks' kinds (0x80307108 +0, +32), the +64
    index and whether the cameras were lent.
  - At the end it warns only for a -2 slot whose look was up at the open and is kind 0 now. An idle look beside a -2 pair
    is not warned.
  - The index is the game's: slot 2 is look 0, slot 4 is look 1, any other slot uses the +64 at the open (impl B). The line
    prints the look index and its kind at the open.
  - It names player 2 when the cameras were lent: the game's slots are then their lent ones.
  - S9z-off (kind 1 to 0) and today's S9 (kind 2 to 0) still print it.
- **Safety 1 / design 6, F3e outside the window** (25486-25492, 28824). The refusal now asks `r4_own_fade_on(cpu, 2)`. It
  holds with the split installs, ED_SPLIT_HOLD and ED_STAYER_PRESSES not 0, co-op stay, player 2 there and not in a copy's
  turn, and 0x801E7C98 answering on. "Their door" is any one of:
  - their ticket counting (g_coop_room_p2 with COOP_ROOM_TICKET > 0). This covers refused windows (a stayer's summon, a
    cutscene at the ticket, apart with ED_SPLIT_HOLD=split), where g_split.on is false;
  - the window (`g_split.on`, mover 2), which is the design's own case;
  - R4's `g_fade_mover == 2`, for the fade-in after the arrival;
  - F3h's forgot step.

  This is a superset of the design's gate inside the same switches, and the game refuses the hero's Start in any fade.
  Solo and follow mode never reach it (no g_split_ready, or no coop_stay). New runs: S3m-tail and c3-s3m-cast.
- **Safety 8, g_fade_mover at a state load** (30863). It is reset to 0 beside `g_stay_bracket_p1 = false`. That is a fresh
  launch's value. Solo never sets it.
- **Safety 3, player 2's set after F2a (a log, no change)** (14203-14205). Under ED_COOP_LOG, with the flag set and F2a
  having passed something, the code prints `[stay] player 2's set at the end of their change (F2a passed N): camera index I
  depth D` before coop_cameras_in(1), so r13-26864 and r13-21700 are still player 2's. S9's pass line should add: these
  match S5/S11 at the same door.

### Rejected
- **Design 5, a bracket that never reaches its end keeps the flag.** g_stay_bracket_p1 lives exactly as long as
  g_stay_bracket: it is set only after `g_stay_bracket = 2` (14173) and cleared beside `g_stay_bracket = 0`. A bracket that
  missed coop_stay_change_end would already leave `coop_be_player2(cpu, true, ...)` unreverted (r13-21492 player 2's), the
  camera set player 2's and player 1's slots never written back (14207). That is far worse than F2a's skip, and it is
  pre-existing. A frame stamp on the flag alone would not make such a bracket safe, so none was added.
- **Safety 2, a gap between the count's 0 and the tail.** It is unreachable. The ticket tick 0x80155830 decrements the count
  (0x80155854-0x80155858) and fires in the same call when it reaches 0: 0x80152298 at 0x801558A8, or 0x80152614 at
  0x801558F4, which calls 0x80152298 unconditionally at 0x80152654. 0x80152298 runs the arrival and then the tail
  (0x80154B54 at 0x80152430, split_over at hle.c 14670). No frame, and so no tome_copy_step or fade quad, falls between them.
  G2 holds the count above 1, never at 0.
- **Safety 4, answering `busy & ~2` after the let-go.** The design keeps this to a trace ("expected none; checked, not
  assumed", F2b, impl A): no `bl 0x80155738` site was found in the arrival's functions, and S9 showed no ask there. The
  reviewer's own fix says to use it only if the F2b trace lists an ask. Nothing has run, so it waits for S9's
  `bracket ask after the let-go` lines.
- **Safety 5, F2a keeps a bare-scene look (kind 3).** It was kept as built. Nothing cancels a solo player's scene at an
  arrival. A kept kind 3 is ended by the scene's own close, as in a shared-room scene with no door. Today's
  clear-and-re-arm (stay_scene_look_again) exists only because the cancel killed it. No run covers the scene-inside-a-zone
  shape yet. It is listed in build-run-stayer-findings.sh's "not runnable" comments. If it shows a hold, limit F2a to kinds
  1 and 2.
- **Safety 6, F3d hides a stayer's own scene fade.** It was kept as designed. The fade state is one global (0x801E7C88
  stores one ticks and target), so a stayer's scene fade in the window replaces the door's for both halves. Today it is
  already marked out of the stayer's half while it ticks. Excluding bit 32 would reopen goal 1.2 for the stayer's own scene
  under the door's fade-out, which is the case F3d was built for. Telling a scene fade from the door's would need a hook on
  0x801E7C88, and none is installed. This is noted for a run with a fading fn22 scene in the window.
- **Safety 9, player 1 as the mover opening a copy in their own fade.** That path (Start with g_coop_screen up during
  player 1's own countdown) exists only when player 2's lend-route close-up is up at player 1's ticket. That is the impl F
  shape (G2b's last count, or the 900-frame give-up), which the design marks open (R4d, F2.7) and this build must not
  build. It is recorded there. Run S5d sets it up.
- **Design review "not problems"** were confirmed and left: the F3c count, F3d's term, F3e not on the sharing path (the copy
  is from the clean snapshot), fade_out not reset by tw_state_loaded.

### build-run-stayer-findings.sh against the design's run list
- **Checked:** every F2.6 and F3.5 line is present with the design's env, and the env names are the kit's:
  - ED_PUT `frame,player,x,y,z`;
  - ED_PAD2_SCRIPT `start,end,id`, with ids b, start, right, up, lup from controls.cpp's kButtons and stick ids;
  - ED_INPUT_SCRIPT `start,end,hexbuttons[,x,y,analog]`;
  - ED_RUN_SCRIPT `DELAY:SCRIPT`;
  - ED_COOP_P1_QUICK, ED_COOP_P2_QUICK and ED_COOP_P2_MAGICK, all read in hle.c.
  The ch3 $L3, $DOOR, $FOUNT and $D23 are stayer-test-setups.md §1c's $L, $DOOR, $FOUNT and $D2. S11d is §2's S11c line
  with the design's player 1 presses. `sh -n` passes.
- **Added:**
  - S3m-tail: player 2's Start in their own fade-in;
  - c3-s3m-cast: player 2's Start in their own countdown in a window refused for player 1's summon. Both are for F3e's
    widened gate.
  - Two comments under "not runnable": S10c (named in the design, defined nowhere) and the R4c-with-a-zone shape
    (safety 5).

# Runs (2026-11-13 evening, fresh states made on this build: rooms-ch1-183, rooms-ch3-153 with ED_CHEAT=1)

No `ed test`: its scenarios load the old slot saves the user forbids (it was started by mistake, saw "load REFUSED" under
the run's ED_ROOM_SLOTS and booted through the intros; stopped, removed from the script).
| Run | Result |
|---|---|
| S9 player 1's close-up through player 2's arrival | pass: F2a passes over the let-go (slot 4, look 1), R4b keeps busy 2, "carried over"; ed see 4300: player 1 `doing: close-up` on Camera06 (DETAIL shown, body hidden), player 2 free in 180; 0 frames waited |
| S9w, S6r (RETURN up, stage 3), S6 (words) | pass: the words and RETURN close by the game's own pop afterwards |
| S9z (ready EXAMINE at the arrival) | pass: B at 3900 opens the close-up after |
| S6c, c3-p1-exam-before (begun before the ticket) | pass: player 2's B kept until RETURN, then their door, the look carried |
| S11d apart, S11r rejoin | pass: carried in both |
| S3 / S3h player 2's copy in player 1's door (and held across the firing) | pass: the copy half steady at luma ~35 while player 1's half fades 21 -> 1; "drew the fade quad 0 times"; room 183 back after the close |
| S8 / S8h player 1's copy in player 2's door | pass: the copy half steady ~35, the other fades to 1-5 |
| S3t / S8t a copy in the fade-in tail | pass: "the fade-in" cleared; F3f opened player 1's copy there |
| S3m / S3m-tail / c3-s3m-cast the mover's own Start | refused in their own door's fade (F3e), as the game refuses the hero's |
| S6f (F3d) | not reached: no stayer bits in the window |
| c3-p1-quick | pass: player 1's quick spell cast in player 2's window (3475 -> cast 3477) |
| c3-p2-quick-in / c3-p2-810-in | pass: player 2's 0x504 and 0x810 cast in player 1's window (3480), the door on time |
| c3-p2-summon-in | pass: kept ("waits for player 1's door"), cast at the tail 3524 |
| c3-p2-start-in | pass: the copy opens at 3475 in the window with the fade left out, quad 0 times |
| c3-p1-zone-ready | pass: the FOUNTAIN's ready EXAMINE survives player 2's arrival; B at 3600 opens it, DETAIL its words |
| c3-p1-summon-zone | G3: no window while player 1 charges a summon (as designed); its later B was the summon's |
| S12 off, S13 follow, S14 solo, S9/S3 with ED_SPLIT_HOLD=0 | no [stayer] lines; follow prints only the renderer's two [split] lines as before; solo as before |

The chapter-3 state loads into a TransformCam01 scene (busy 0x22 to frame 3330): its runs are shifted 300 frames
(build-run-stayer-findings.sh). Room 153's door ticket fires on the first ED_RUN_SCRIPT call when player 1 stands in its
zone (state 1 already), at ~3461.
