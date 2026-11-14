# Review: the F2/F3 build against design-stayer-findings.md (revision 2)

Read only. Nothing was built, run or launched. The code is runtime/host/hle.c as it stands now (uncommitted work included).
The addresses were checked with `ed dis` and build/ed-db/disasm.txt.
Lens: the build against the design and the disassembly. That covers the lrs and addresses, the bracket flag's lifetime, the
copy's memcpy point, the F3 tail window, and anything the design asks for that was dropped.

## Verdict

The build follows the design. No [real] defect was found: every address, lr and gate the design names is in the code as
designed, and nothing the design asks for was dropped (checklist at the end). Six [maybe] points follow. Each is an edge the
design did not cover or a log that could mislead. None changes solo, follow mode, ED_SPLIT_HOLD=0 or ED_STAYER_PRESSES=0.

## What was checked and holds

- **The let-go chain** (F2a, hle.c 19751-19767).
  - 0x80116E70 opens with `stwu r1,-16(r1)` and saves its LR with `stw r0,20(r1)` (0x80116E7C). At the 0x80024CDC
    intercept's entry, `r1+20` is therefore 0x80116E70's return, so the "from 0x..." caller is right.
  - 0x80116E70 calls the callback for every slot when r4 != 0, whatever the slot holds (`cmpwi r4,0` at 0x80116E78).
    So `let_go` is set at the first of the six calls.
  - guest_backchain_contains(0x80152138, 0x8015213C) reaches 0x801172B0's saved LR on its second frame, and the range
    includes 0x80152138.
  - The slot-to-look mapping in stay_slot_look (14178) matches 0x80024CF8-0x80024D3C: slot 3 and slots 0-1 leave +64,
    slot 2 sets 0, slot 4 sets 1, slot 5 and up leave it.
  - The skip (passthrough false, no tail call) is today's lend-route keep. The host never calls 0x80024CDC itself; it calls
    0x80024B24 directly (11344), so F2a cannot swallow a host cancel.
- **The bracket flag's lifetime.**
  - Set after `g_stay_bracket = 2` (14163) and only there.
  - Cleared beside `g_stay_bracket = 0` (14228) and in the state load (30815).
  - A second notify_ticket_run cannot reopen it, because of the `g_stay_bracket != 0` return.
  - Its log record is zeroed at each open.
- **F2b** (20128-20137): the term is `(!bracket_p1 && coop_p1_close_up(cpu))` with no scene exclusion. The other terms keep
  today's order and short-circuits. The traces match F2.5.
- **WARNING and the carried-over line** (14189-14202): both come after the write-back and after coop_cameras_in(1). They
  read the slot's +12 (0x80116EA4) and the look's +0/+8. The WARNING is always printed.
- **The fade setters** (15074-15122). 0x801E7C88 is `lwz r0,0(r3); stw r4,-21784(r13); stw r0,-26896(r13)`, and 0x801E7D24
  reads the colour bytes through r3. Both take a pointer, as the code passes them.
  - The scratch at `(r1-0x200)&~15` lies between the caller's frame and hle_guest_call's own stack. That stack starts at
    `(r1-0x800)&~15` (main.c 80), so the scratch is not overwritten.
  - None of 0x801E7C88, 0x801E7D24, 0x801E7C98 or 0x801E7DA4 is hooked, so no running guard is needed.
  - hle_guest_call takes the FPU itself (main.c 65), which 0x801E7C98's `lfs`/`fcmpu` need.
- **The memcpy point** (15172-15179):
  - `fade_out` is read after the memcpy, with cpu->ram still real and g_tome_copy_turn false. That is before
    tome_copy_swap(cpu, true) (15218).
  - The clear runs after the swap (15223), before TW_MODE_CHANGE and before g_tome_copy_turn is set (15274).
  - The swap copies only the GX record (r13-26688, 0x4F8 bytes) back into the copy. That is not the fade state.
- **The fade tick** (`ed dis`):
  - 0x801E7EA8 stores ticks-1 before the alpha test at 0x801E7ED4.
  - The quad's GXLoadPosMtxImm call is at 0x801E7F5C, so lr 0x801E7F60 is right. Its ticks are read after the store.
  - The fade-in's last drawn quad has ticks 1, and the next step lands the alpha exactly on its target. F3h's `ticks == 1`
    is right.
- **R4's mark, moved to r4_fade_quad** (25458-25491; called at 3428):
  - Outside a copy's turn, with the switches off and no stayer bits, it is byte-for-byte the old block (HEAD 3341-3350).
  - The forget test is `!door_on && ticks <= 0`.
  - The marked words are the same: 0x0C8000D2, 0x0C8000A0+view, 0x0C8000E5.
- **The F3 tail order.** 0x80152298 calls the arrival 0x80151FDC at 0x801523A4, which starts the fade-in at 0x801521A8
  (`li r4,10`). The tail's 0x80154B54 follows at 0x80152430 (lr 0x80152434, split_over at 14647). So the fade-in has 10
  ticks when the tail is recorded, and stayer_fade_tail's `ticks > 0` holds through it. A copy step either just before or
  just after the firing frame gets `fade_out` 1 (the window) or 2 (the tail).
- **Gates.**
  - F2 needs g_stay_bracket, which comes from coop_stay (ED_COOP_STAY plus room_slots()==2), and the flag, which needs
    g_split_ready, ED_SPLIT_HOLD not 0 and ED_STAYER_PRESSES allowing player 1.
  - F3a, F3b and F3f need stayer_window or the tail record (split_over returns at `!g_split.on`, and a refused window has
    g_stayer zeroed).
  - F3d needs stayer_window_on.
  - F3e needs g_split.on with mover 2.
  - F3g only prints.
  - So solo and follow mode reach none of it. They also have no copy turn, no g_fade_mover and no bracket.

## Points

### 1. [maybe] F3h runs without the split installs (hle.c 25485)
- **What:** the test is `split_switch() != 0 && stayer_switch() != 0 && !door_on && !g_split.on && ticks == 1`. Both switch
  functions read only the environment.
- **Failure:** with co-op stay and room_slots()==2 but the split installs refused (g_split_ready false: the code check at
  30547 fails, for example on a different game build), g_split.on is never true. R4 then forgets the mover at the fade-in's
  ticks 1 on every door. Today the mover is kept until the next ticket. The task's rule says no new path without the
  installs. Visible effect: a later fade is drawn into the old stayer's half too, which is what F3h is meant to do, but
  without the design's gate. Both players' doors are affected.
- **Fix:** add `g_split_ready &&` to the F3h test, as stay_bracket_p1_allowed has it.

### 2. [maybe] A one-frame gap at the end of the tail (hle.c 25445 and 25485)
- **What:** F3h forgets the mover inside the world frame's quad with ticks 1. stayer_fade_tail needs `g_fade_mover ==
  mover` and `ticks > 0`. On the frame after that quad, the fade is still on by 0x801E7C98 (ticks 1, live alpha above 0,
  and the last step is still to run), but the tail test is false.
- **Failure, player 1 staying:** player 1's Start on that frame is not taken by F3f (tail false), so it goes to
  0x80006CDC, which refuses it at 0x80006CF4. That is one dead press, the case goal 1.1 meant to remove.
- **Failure, player 2 staying:** player 2's Start then opens a copy with `fade_out` 0. F3g prints "opened in a fade", and
  the copy draws one last quad (whole frame, near-zero alpha).
- **Fix:** in F3h, before zeroing, record `g_stayer.fade_in_last = split_frame(cpu)`. In stayer_fade_tail, accept
  `g_fade_mover == 0 && g_stayer.fade_in_last + 1 >= split_frame(cpu)` (same tickets) together with `ticks > 0`. Or let F3h
  forget at the first quad-less step instead of at ticks 1. The S8t/S3t runs would show it only if the press lands on that
  frame.

### 3. [maybe] The binding-lock clear doesn't cover a copy opened in the tail (hle.c 15183)
- **What:** F3a/F3b's fade clear covers the window and the tail (`fade_out` 1 or 2). The binding-lock block beside it,
  which the design says to keep "with the same gate", still tests only `stayer_window`.
- **Failure:** player 1's door with player 2 staying. The ticket writer put the binding lock r13-23592 (~65 frames, from
  0x801557F4) on the game's input copy. L1 (stayer_lock) keeps it off only for player 2's door, with player 1 staying. A
  copy that player 2 opens in the fade-in tail (about ticket+60 to +72) carries what is left of the lock. Its Tome then
  takes no button for those turns: up to ~5, more if G2 waited. This was already true for player 2's tail Start, but F3b
  now makes that copy an intended path.
- **Fix:** test `g_tome_copy.fade_out != 0` in the binding-lock block (it is set just above), so the two clears share one
  gate. Not in the design. It is a one-term widening inside the same switches.

### 4. [maybe] The WARNING can fire for a -2 slot whose look was already idle (hle.c 14189-14195)
- **What:** it warns for any slot 0..5 holding -2 whose look (slot 2 -> 0, slot 4 -> 1, else +64) has kind 0 after the
  write-back.
- **Failure:** if a zone keeps -2 in both slot 2 and slot 4 (the ed301 pair, F2.1) while only one look is armed, the idle
  one has kind 0 before the bracket as well. The line then says "written back over a look of kind 0: a dead prompt" when
  nothing was cancelled. That could come with F2 on, where S9 expects no WARNING, or with the switches off, where S9z-off
  uses the WARNING as its detector.
  - With the cameras lent, the slots written back are player 2's lent ones, so "player 1's" is the wrong player.
- **Fix:** at the open (14163), save the six looks' kinds as the slots are saved. At the end, warn only where the slot is -2
  and its look went from nonzero to 0 inside the bracket, and name the player by `coop_cameras_lent()`. This makes the
  detector exact for the user's gate decision (F2.2).

### 5. [maybe] A bracket that never reaches its behaviour pass keeps the flag (hle.c 14163, 14228)
- **What:** the flag is cleared only in coop_stay_change_end, which runs only at lr 0x80008400/0x80050A9C, or at a state
  load. This was already so for g_stay_bracket itself.
- **Failure:** if the end is ever missed (a runner path that leaves without that behaviour pass), the flag stays set. F2a
  would then pass over every later -2 let-go, including player 1's own zone leaves (0x801170D4), so their close-up would no
  longer end on leaving a zone. F2b would keep their look from ever being read over. Today, a stuck bracket at least lets
  those cancels run.
- **Fix:** stamp the open with `g_coop_tickets` and the frame. In F2a and F2b, treat the flag as off once the frame has
  moved past open+2 (S9: the bracket is one game frame, 3822 to 3824). Print one `[stay] WARNING: bracket open past its
  frame` line when that happens.

### 6. [maybe] F3e covers only the window (hle.c 28778)
- **What:** this is as designed (the g_split.on gate), and the build notes say so. It is listed because goal 1.3 ("a black
  Tome till B") is closed only for that case.
- **Failure:** player 2's Start in their own door in a refused window (a cutscene at the ticket, G3 casting, apart with
  ED_SPLIT_HOLD=split) still opens a copy mid-fade. That copy goes black at ticket+60 until B, as before this build. In the
  ~10-frame fade-in after their own tail, a copy opens with the fade-in and brightens in the copy, which is not black.
  F3g logs both.
- **Fix, if wanted:** gate F3e on `g_fade_mover == 2 && coop_stay()` instead of `g_split.on`, still behind `stayer_switch()
  != 0`. That is the user's call; it needs no change in this build.

## Not problems (checked)

- F3c counts copy quads with or without g_fade_mover (the notes' departure). That only makes the close line stricter.
- F3d's new term is `stayer_window_on && !cutscene_playing`, as the design writes it. It holds the mark for anything that
  turns coop_room_change_on off in the window, not only the stayer's bits. For the mover's own door words that is the
  wanted R4 picture.
- F3e is not in the sharing-path opening. That copy is taken from `g_tome_copy.clean` (player 1's real Tome, opened with
  no fade), so it has no fade to clear.
- g_tome_copy.fade_out is not cleared by tw_state_loaded. That is harmless: every opening sets it, and only a close prints
  it.
- F3b's record when the window ended by the stayer leaving the room (g_split.on still set): harmless, since a copy then
  only gets a no-op clear.

## Design items against the build (nothing dropped)

| Item | Where | Built |
|---|---|---|
| F2c flag, helper, set, two clears | 13771, 3285/25439, 14163, 14228, 30815 | yes |
| F2a first branch, -2 only, not lent, log (slot, look by slot, kind, stage, caller), at most 8 | 19751-19767 | yes |
| F2b term without the scene exclusion; "not read" trace once; asks after the let-go, at most 16 | 20128-20157 | yes |
| Carried-over line (ED_COOP_LOG, after cameras_in(1)); always-printed WARNING | 14189-14202 | yes |
| F3a fade_out before the swap; setters 0x801E7D24(&0), 0x801E7C88(&0,0) after it; 0x801E7C98 check and WARNING; log | 15172-15179, 15088-15114, 15223 | yes |
| F3b tail record at "the change's tail"; stayer_fade_tail with all its terms | 25424-25433, 25443-25448 | yes |
| F3c no mark in a copy's turn; fade_quads; close line | 25459-25463, 14948-14952 | yes |
| F3d door_on with stayer_window_on and no cutscene; once-per-window line | 25466-25483 | yes |
| F3e refusal in the play branch, always printed | 28778-28784 | yes |
| F3f tail Start (window or tail-and-Start), B only in the window, tail log | 28340-28393 | yes |
| F3g always-printed line at every opening outside the window and tail | 15095-15098 | yes |
| F3h forget at ticks 1, behind both switches and !g_split.on | 25485-25490 | yes (point 1: also needs g_split_ready) |
| Open: impl D, impl F (R4d), goal 1.6 | | not built, as marked |

The hand-made b_eat/start_eat lines in stayer_p1_presses (28326-28339) and the StayerAgain code (19580-19620) are unchanged.
