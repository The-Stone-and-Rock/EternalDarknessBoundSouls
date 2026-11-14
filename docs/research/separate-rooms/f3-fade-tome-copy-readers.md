# F3: the door's fade in a stayer's copy Tome (readers)

Read only. Nothing was changed, launched or run. HEAD 5eb040d plus the uncommitted hle.c and gxruntime patch.
Marks: [read] seen at an address or line, [read there] in a named doc or log, [measured] worked out from a run's
pictures (mean brightness of each half, using System.Drawing over `shots/*.jpg`), [inferred] not seen.

## Answer in short

- The fade belongs to the copy itself, not to the world view, so the host's mark never comes into it.
  `tome_copy_step` copies all of guest RAM when the copy opens. That includes the fade's state: ticks r13-21784, target
  r13-26896 and the live RGBA floats at 0x80602A2C. The Tome frame calls the fade tick 0x801E7DA4 at 0x80111790, once
  per copy turn. So the copy carries the door's fade toward black at the same speed as the real one, and draws the fade
  quad over its own Tome pages.
- The copy's frames are whole frames, with views skipped (`split_skip_views_set`). The renderer only honours R4's
  quad mark (`marked_out_of`) in per-view replays (`build_view` with player >= 0). So the quad stays in the copy's
  picture, and that picture is fitted into the stayer's half.
- **Player 1's copy has the same problem.** The pictures show it. In S8, player 1's copy Tome dims in step with player
  2's door fade, from about 54.7 to 34.0 mean brightness between ~3776 and ~3794 [measured]. It closed at 3807, when the
  fade was about 77% done and before it reached black. No shot shows that Tome near black, which is why it looked
  "unfaded". The two halves are not handled differently. The difference is when each copy closed.
- **A copy open across the firing stays black until it is closed.** The copy never runs the ticket tick 0x80155830 or
  the arrival's fade-in at 0x801521A8, so its alpha stays 255. The copy's own turn can also clear the global
  `g_fade_mover` early. Details are below.
- The old follow-mode note ("a copy opened in the fade showed black once the new room was in") had the same cause. The
  stayer-presses design guessed a room change was the reason (design-stayer-presses.md §2.7, "their half never changes
  room"). That guess was wrong: the black is the copy's own fade.

## 1. The fade: state, tick, quad

- 0x801E7C88(colour*, ticks) stores the ticks at r13-21784 and the target colour word at r13-26896, and nothing else
  [read: dis 0x801E7C88].
- 0x801E7C98 is the "a fade is on" test [read: dis 0x801E7C98]. It returns 1 when any of these hold:
  - the ticks are not 0;
  - any target byte is not 0;
  - any live float at 0x80602A2C..38 is not 0.0.
- 0x801E7D24(colour*) sets the target and the live floats at once and leaves the ticks alone [read: dis 0x801E7D24].
  Its callers are 0x800204AC, 0x80026730, 0x8004F7F0, 0x8004FE48, 0x80156C84, 0x8016BC34, 0x801E9C78 and 0x801E9DB8.
  None of them is in the Tome's code [read: grep in disasm.txt].
- 0x801E7DA4 is the tick [read: dis 0x801E7DA4, 0x801E7EA0]:
  - While the ticks are not 0, it moves each live channel toward the target and takes one off the ticks (0x801E7EA8).
  - It then draws the quad whenever the live alpha (0x80602A38) is above 0.0 (0x801E7EC0-0x801E7ED4, return at
    0x801E8100).
  - The quad uses PSMTXIdentity in the tick's own stack buffer, loaded by GXLoadPosMtxImm at lr 0x801E7F60
    [read there: doors-readers.md fade-flow, lines 112-116].
- Callers of the tick [read: grep `bl 0x801E7DA4`]:
  - 0x80008560, the world frame;
  - 0x8002026C;
  - 0x80050BA4;
  - 0x800EBA38;
  - **0x80111790, the Tome frame TW_TOME_FRAME 0x80111234**. It runs every Tome frame after 0x8019CFD4(5), with no
    branch around it [read: dis 0x80111770].
- Callers of 0x801E7C88, the fade start: 22 sites, none in 0x80100000-0x80117000 [read: grep]. The Tome never starts,
  clears or restores a fade of its own:
  - 0x801E7C40 (clear) is called only from 0x80021B30, the game's start-up;
  - 0x801E7BE4 (restore) is called only from 0x80008578 (the world frame) and 0x80050BBC.
- The ticket tick 0x80155830 is called only from 0x800083EC (the world frame) and 0x80050A90. The arrival's fade-in is
  at 0x801521A8, inside 0x80151FDC, which only the ticket's firing reaches [read there: doors-readers.md, lines 46 and 122].

## 2. How R4 keeps the fade out of the stayer's half in the world

- `g_fade_mover` (hle.c:3266) is set at every ticket under `coop_stay()` with player 2's node present: 2 when player 2
  is the mover, else 1 (notify_room_ticket, hle.c:11433-11434) [read].
- notify_GXLoadPosMtxImm (hle.c:3411-3426) runs on every load at lr 0x801E7F60 while `g_fade_mover != 0` [read]:
  - If `!coop_room_change_on(cpu)` and r13-21784 <= 0, it forgets the mover (`g_fade_mover = 0`).
  - Otherwise it writes whole-word marks into the quad's identity: 0x0C8000D2 at +12, 0x0C8000A0 + (stayer's view) at
    +28, and 0x0C8000E5 at +44. The view is 0 when the mover is 2, else 1.
- coop_room_change_on (hle.c:11401-11407) needs busy r13-23344 to have bit 4 and no higher bits, plus the ticket
  countdown 0x8058AFFC > 0, no cutscene and no menu [read].
- Renderer side (patches/gxruntime/eternal-darkness.patch) [read]:
  - `marked_out_of` (patch ~11582-11591) reads the low bytes D2 / A0+player (or A2) / E5.
  - `build_view` zeroes a marked matrix only for `player >= 0` (patch ~11720-11735).
  - `split::plan` (patch ~5562-5612) calls `split_build` per player, and returns nothing at all while `g_skipViews` is
    set.
- So the mark works only on a world frame replayed per view. A whole frame keeps the quad. This was already flagged as
  open in doors-readers.md line 98: "the quad's fate in a whole frame is open" [read there].

## 3. How a copy Tome is drawn into a half

- **Player 2's copy** (tome_copy_step, hle.c:15025-15219) [read]:
  - At opening (15080) it does `memcpy(copy, sharing ? clean : cpu->ram)`. The copy is the whole real RAM at that
    moment, inside the door window, with the fade part done.
  - With no Tome shared, `stands_in = true`, `aurora_split_menu(1, layout)` and `aurora_split_only_view(0)`
    (15156-15159).
  - Each turn (15111) it calls `split_skip_views_set(2)`, swaps to the copy, and runs TW_MODE_CHANGE once, then the
    frame at r13-26192 (TW_TOME_FRAME) and kAfterFrame (15188-15191). Afterwards it calls `split_skip_views_set(0)`
    (15202).
  - There is one copy turn per world frame (comment at 15055) [read].
- **Player 1's copy**: `split_skip_views_set(1)`, `aurora_split_menu(0)` and `aurora_split_only_view(sharing_was ? 2 : 1)`
  (15111, 15167-15170) [read].
- Renderer [read]:
  - plan_outputs (patch ~5738-5790): a frame with views skipped while sharing becomes `build_turn(turn)` frames in
    between, plus one **Real**, which is the frame as the game drew it, with no `split_build` and no marks.
  - A world frame while sharing plans only `g_onlyView`, whose last Split is turned into the Real.
  - end_frame (patch ~4420-4700) keeps the menu's picture (`s_menuKept` / `s_menuBetween`). The present (`menuHalf`)
    draws the other player's view in their half and fits the menu's picture into the menu player's half.
- So whatever the copy's frame draws, including a full-screen quad, fills the copy owner's half. The world's single view
  is always the mover's: the copy is always the stayer's in a window. The mover's half is correct either way, since the
  fade belongs there [inferred from the above].
- The tome.cpp and frontend.cpp host files have nothing on the fade or the copy's split [read: grep].

## 4. Why the mark "fails" during the copy

- No mark is failing. The quad on the stayer's half during the copy is the copy's own quad, drawn in a whole frame that
  never goes through `build_view`.
- The world's quad in that frame only reaches view 0 (the mover's), where it belongs.
- In the copy's turn, notify_GXLoadPosMtxImm is not stood aside: it is not in tome_copy_hook_off's list (hle.c:28974-28986)
  [read]. It still marks the copy's quad while the copy's ticks are above 0. That has no effect on a whole frame.

### The pictures [measured, mean luma 0-255]

| Run / shot | ≈ frame | real fade done | copy Tome half | world half |
|---|---|---|---|---|
| S3 000011 | 3828 | 12% | (before the copy) 17.9 | 19.6 |
| S3 000012 | 3845 | 40% | 45.6 | 14.0 |
| S3 000013 | 3864 | 72% | 23.5 | 7.3 |
| S3 000014 | 3883 | 100% | 2.4 (faint Tome outline) | 1.1 |
| S3 000015 | 3897 | fade-in done | 17.1 (room 183) | 25.9 |
| S8 000008 | 3776 | 25% | 54.7 | 18.9 |
| S8 000009 | 3794 | 55% | 34.0 | 12.0 |
| S8 000010 | 3810 | 82% | (copy closed 3807) 46.8, the stayer's world, marked | 5.3 |

- An undimmed Tome comes out at about 73-84 in both runs (value / (1 - fade)). Both copies fade in step with the real
  fade.
- S3 fade timing: the ticket is at 3821 and the fade lasts 60 ticks ('fade 55' at 3826, 'fade 1' at 3882 by
  `ed see`) [read there: page-0001.log, ed see].
- S8 fade timing: the ticket is at 3760 ('fade 56' at 3765).
- S3: the copy opened at 3830 and closed on B (pad 0x0200 at 3880) after 51 turns, just before 3882. The firing was at
  3884 [read there: S3 page-0001.log lines 306-310 and 741-766].
- S8: the copy opened at 3770 and closed at 3807 after 37 turns. The firing was at 3824 [read there: S8 page-0001.log
  lines 317-319, 687 and 888].

## 5. A copy open across the firing, and after

[inferred from the code above. Not seen in a run since R4. The old note's run, logs/run-20261106-081752-jtg-vdh6-c1p1-p2start,
showed the same black: read there, hle.c:28383-28386.]

- **The copy's fade.** It reaches its target, alpha 255 and ticks 0, about ticket+60 copy turns after the copy opened.
  It stays there: the copy never runs the ticket tick, the firing (0x80152614 / 0x80151FDC) or the arrival's 10-tick
  fade-in (0x801521A8). The tick draws the quad every frame while alpha > 0, so the copy owner's half is a black Tome
  from then until they close it blind with B. The real world goes through the firing and fades in. None of that reaches
  the copy.
- **The copy can clear R4's `g_fade_mover` early.**
  - At opening, tome_copy_no_window writes busy r13-23344 := 0 in the copy (hle.c:14831) [read]. So in a copy turn
    `coop_room_change_on(cpu)` is false, because it reads the copy's RAM.
  - On the first copy turn whose quad is drawn with the copy's r13-21784 <= 0, hle.c:3418-3419 sets the global
    `g_fade_mover = 0`.
  - From then on, the real game's fade quad (the rest of the black, and the arrival's fade-in) is unmarked.
  - While player 2's copy is open this cannot be seen, because only view 0 is drawn. If the copy closes after that
    point but before the real fade-in ends (ticket+~74 in S3's numbers), the stayer's half shows the mover's black and
    fade-in for those frames.
  - The same applies to player 1's copy across player 2's firing (with view 1 and view 0 swapped).
  - In S3 the copy ran about 50 frames from ticks of about 51, so whether it hit 0 before B is borderline. No log line
    tells.
- **A copy opened before the ticket.** The fade is off in it (ticks 0, alpha 0) and it never starts one. It stays whole
  through the door, as hle.c:28385-28386 says [read]. That copy and F3's differ only in when the RAM was taken.
- **After the copy closes.** tome_copy_close (hle.c:14949-14955) calls `aurora_split_only_view(-1)` and
  `aurora_split_menu(-1)` [read]. The stayer's half is a per-view replay of the world again, with the mark if
  `g_fade_mover` is still set. That is S3 000015-000016: room 183 drawn normally.

## 6. What a fix would touch (not built)

The fix uses the game's own functions and works for both players and every door. It is co-op only, because only
`tome_copy_step` makes copies. Solo is untouched.

1. **The copy starts as the game's Tome always starts: with no fade.**
   - The game never opens a Tome during a fade: 0x80006CDC refuses Start while 0x801E7C98 says a fade is on, at
     0x80006CF4 [read there: docs/game/research/verbs.md:324].
   - So in the copy, at opening, when the copy is taken in a door window (`stayer_window(cpu, owner)`, or
     `g_fade_mover != 0`), call 0x801E7D24(&zero colour) and 0x801E7C88(&zero colour, 0) in the copy's RAM. That clears
     the live floats, the target and the ticks.
   - The place is beside the binding-lock clear (hle.c:15084-15094) or in tome_copy_no_window, which already runs in
     the copy. The binding-lock clear is the same shape: something of the mover's left in the copy.
   - Then 0x801E7C98 answers 0 in the copy, and 0x801E7DA4 returns at 0x801E7ED4 without drawing.
   - This covers both players' copies, the fade before the firing, the firing, and after it.
   - Other fades (a script's fn112, a cutscene end) are left alone outside the window.
2. **Belt and braces:** skip R4's fade block in notify_GXLoadPosMtxImm while `g_tome_copy_turn` is set, so a copy's
   quad can never set the global `g_fade_mover` to 0. With fix 1 the copy draws no quad, so this branch is not reached
   anyway.
3. **Check in runs** (none were run here):
   - S3 and S8 again: the Tome half keeps its brightness (about 75) through ticket+60.
   - A copy held open across the firing, for each player: the Tome stays clear, and after B the half shows the stayer's
     room.
   - A copy closed in the gap between the copy's black and the real fade-in's end: the stayer's half is not black.
   - Solo and follow mode: no change.
