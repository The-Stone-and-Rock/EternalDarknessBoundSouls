# Review (safety): the stayer findings F2/F3 build (design-stayer-findings.md revision 2)

Read only. Nothing was built, run or launched. hle.c line numbers are as the file stands now (after the build in
stayer-findings-impl-notes.md). The lens: solo, follow mode, no ED_ROOM_SLOTS, and the switches off get nothing new.
The mover's own close-up, prompts and arrival fade-in are unchanged. Player 1 is never left with a dead prompt, a stuck
camera or a hidden body. g_fade_mover is never left stale. F3e refuses only the mover's Start in their own door's fade. No
copy Tome opens black or stays black. The bracket flag can't leak past the change.

Tags: **[real]** a concrete failure read in the code. **[maybe]** it depends on something not read or not run.

## Findings

### 1. [real] F3e is gated on the window, so the mover's own copy still goes black where no window is open (hle.c 28778)
- **What's wrong:** `g_split.on && g_split.mover == 2 && ...`. g_split.on is false in two cases:
  - **A refused window** (split_ticket_open 25508-25531): the stayer casting (G3), "a cutscene at the ticket", or apart with
    ED_SPLIT_HOLD=split. The fade still runs, and start_room only refuses in player 1's change (28686).
  - **The fade-in after the tail.** The notes say so themselves ("F3e covers only the window").
- **Failure, player 2, refused window:** player 1 charges a summon in room 183, and player 2 takes the door to 180
  (`[split] player 1 is casting: their room holds as today`). Player 2 presses Start during their countdown, which makes a
  mode-2 copy taken mid-fade. fade_out is 0 because player 2 is not a stayer, so F3a doesn't clear it, and F3g only prints.
  The copy reaches alpha 255 at about ticket+60 and never runs the firing's fade-in (f3 readers §5). Player 2's half is a
  black Tome until they press B blind. This is goal 1.3's bug, left in place by the gate. It predates this build, but the
  lens forbids it.
- **Failure, player 2, the tail:** a Start in their own fade-in (about 10 frames) opens a copy that carries the fade-in.
  The copy opens dark and clears by its own ticks (0x80111790). The game would have refused that Start (0x80006CF4).
- **Fix:** gate on the mover's own fade, not the window, and keep the switches:
  `split_switch() != 0 && stayer_switch() != 0 && g_fade_mover == 2 && !g_tome_copy_turn && sda_ready(cpu) && fade_on_now(cpu)`.
  g_fade_mover == 2 is set at each of player 2's tickets under co-op stay only (11437), so solo and follow mode never see it.
  If g_fade_mover is stale (item 8), this also refuses a Start in a later scene or script fade. The game refuses the hero's
  Start in any fade, so that is still the game's rule. Add run S3m-cast: S3m with player 1 charging a summon at the ticket.

### 2. [maybe] The gap from the count's 0 to the tail is in neither the window nor the tail (15177, 25443, 25468)
- **What's wrong:** F3a/F3b decide fade_out with `stayer_window(owner)` (split_counting: count > 0) or `stayer_fade_tail`
  (set only in split_over at "the change's tail"). F3d's new term is also stayer_window_on (count > 0). Between the count
  reaching 0 and split_over, g_split.on is still true but all three are false.
- **Failure, the stayer, either player:** a copy opening in that gap takes the fade-out at alpha 255 with ticks 0. The copy
  never runs the fade-in, so it stays black until B. F3g only prints. With the stayer's own bits up, F3d's forget in the
  same gap drops the mover, and the fade-in is drawn into the stayer's half.
- **Whether it's reachable:** that depends on whether a world frame or a tome_copy_step can fall between the firing and the
  tail. S9 shows them in one bracket, but the ticket tick's own frame order (0x80155830 -> 0x80152614 -> 0x80154B54) was
  not read for a frame boundary.
- **Fix:** take the window as "the ticket to the tail", not "count > 0", for these three tests only:
  `g_split.on && g_split.mover == (owner == 0 ? 2 : 1) && stayer_switch_allows(owner) && !g_tome_copy_turn`. It stays inside
  the switches and inside the change.

### 3. [maybe] F2a changes the mover's camera set at their own arrival (19756-19765)
- **What's wrong:** today the cancel 0x80024B24(0) runs inside the bracket with coop_cameras_in(2) in place. Its per-set
  half (kinds 2/3, stage > 0: 0x801ECCEC, 0x801EEA9C, 0x8002576C at 0x80024BCC-0x80024C10) lands in **player 2's** set
  (design F2.1). F2a now skips it, so player 2's set comes out of the arrival without that pop and snap.
- **Assessment:** this is most likely a correction, because the pop belonged to player 1's close-up. But it is a change on
  the mover's side, and no run checks it:
  - S9's pass line reads player 1's camera, and the carried-over line logs only player 1's index and depth;
  - g_stay_arrived's tag and coop_camera_chosen depend on the set the arrival cut.
- **Failure if it is wrong:** player 2's half keeps a camera record one deeper than in S5/S11, so their first RETURN in room
  180 pops to the wrong camera, or their arrival glides.
- **Fix (check only):** log player 2's camera index and depth (r13-26864 / r13-21700 with set 2 in) in
  coop_stay_change_end before coop_cameras_in(1). Add to S9's pass line that they match S5/S11 at the same door.

### 4. [maybe] F2b's leftover bit 2 reaches the mover's arrival code; a one-line exact fix exists (20135)
- **What's wrong:** this is impl A. After the let-go, an ask left to the game sees 6 and then 2, where today it saw 4 and
  then 0. The design only traces this.
- **Failure:** any ask from player 2's arrival after 0x80152138 that reads bit 2 as "busy" changes the mover's arrival. For
  example, the arrival state 80/81/1 could start a frame late, or a node could be held.
- **Fix:** with `bracket_p1` and the ask left to the game after `g_stay_bracket_p1_log.let_go`, answer
  `busy & ~2u` (hle_set_u32, passthrough false).
  - That is exactly today's word: today's cancel always cleared bit 2 at lr 0x80199F28, whether the bit was the door's or
    player 1's.
  - R4b's stay_busy_clear reads the word from memory, so it is unaffected.
  - Use it only if the F2b trace lists any ask there. It keeps the mover's arrival byte-for-byte today's.

### 5. [maybe] F2a also keeps a bare-scene look (kind 3), which changes R4c's tested path (19756)
- **What's wrong:** F2a skips every -2 let-go in the bracket, whatever the look's kind. The design checked R4c only for
  F2b ("bracket_p1 is false in the bare scene"). If player 1 is in a fn22 scene (look kind 3) while standing in an EXAMINE
  zone (a -2 slot), then:
  - today: kind 3 -> 0, 0x80199EA0(1) lets go of the scene's channels and bit 2, R4b keeps 32 only, and
    stay_scene_look_again re-arms the look;
  - now: kind 3 stays, bit 2 stays, and R4b keeps 2|32.
- **Assessment:** that is probably closer to solo, since nothing cancels a solo player's scene at an arrival. But R4c's
  runs were made without it.
- **Failure if it is wrong:** player 1 stays held in a scene whose script already ended, until something lets go of the
  bit 2.
- **Fix:** run R4c's player-2-door run with player 1 in a scene inside a zone. If it shows a hold, limit F2a to kinds 1/2
  (read the look's kind with stay_slot_look and pass kind 3 through).

### 6. [maybe] F3d hides the stayer's own held fade from the stayer for the whole window (25468)
- **What's wrong:** `stayer_window_on && !cutscene_playing` keeps the mark on any quad, not only on the door's fade.
  - A stayer's fn22 scene (bit 32, no letterbox, so not cutscene_playing) can start its own script fade. The fade state is
    global, so that fade replaces the door's.
  - Today it is marked while ticks > 0 (an older flaw) and is drawn in both halves once it is held at full.
  - With F3d the held part also stays out of the stayer's half, for as long as the window lasts: under G2 that is up to 900
    frames.
- **Failure, the stayer:** player 1's own scene fades to black and holds (a scripted cut). Player 1's half shows no black,
  and player 2's half shows player 1's black.
- **Fix:** keep F3d's term for the stayer's words and find (8, 16) only: `... && (g_stayer.bits & 32u) == 0u`. Or record
  the door's fade target at the ticket and require it unchanged.

### 7. [maybe] F3b's 12-frame bound is in timebase frames; a slow fade-in outlasts it (25445)
- **What's wrong:** split_frame is timebase/675000 (1/60 s). The arrival frame itself jumped 3822 -> 3824 (S9). Under load,
  10 game ticks can take more than 12 of these frames, and the tail then ends with ticks left.
- **Failure:**
  - Player 1: their Start late in the fade-in goes to the game and is refused (F3f's dead button again).
  - Either stayer: a copy then carries the fade's remainder and opens dim (F3g prints it).
  - Not black: the copy ticks it out.
- **Fix:** a tail record always implies both switches, so F3h always ends the tail at the fade-in's last quad. That makes
  the frame bound redundant. Widen it (for example to 60) or drop it. Keep the tickets, g_fade_mover and ticks > 0 terms,
  so it is still not "g_fade_mover set".

### 8. [maybe] g_fade_mover is still left stale with the switches off, and F3c removes the one accidental clear
- **What's wrong:**
  - F3h (25485) runs only with both switches.
  - With ED_SPLIT_HOLD=0 or ED_STAYER_PRESSES=0 under co-op stay, g_fade_mover is still kept to the next ticket, as today
    (impl I).
  - F3c's early return (25459) is not switch-gated. In those settings, a copy's quad at ticks <= 0 no longer drops the mover.
  - The state load doesn't reset g_fade_mover either (grep: only 11437, 25470, 25487 write it).
- **Failure:** after a load, or after a door with the switches off, a later scene or script fade with ticks > 0 is marked
  out of the old stayer's half while it ticks. That is the same as every door without a copy today, so nothing gets worse
  in the common case.
- **Fix:** reset `g_fade_mover = 0` in the state load next to `g_stay_bracket_p1 = false` (30815). Optionally, run F3c's
  early return only under `split_switch() != 0 && stayer_switch() != 0`, and keep today's code in a copy's turn otherwise.

### 9. [maybe] Player 1 as the mover can still open a copy in their own fade (28859)
- **What's wrong:** "Player 1's Start in player 2's close-up" opens a copy whenever g_coop_screen is up. The coop_way_out_p2_up
  keep is tested only at the press. G2b ends at the last count or after SPLIT_WAIT_LONGEST = 900 frames (25089, 25680), so player 2's lend-route
  close-up can be up during player 1's own countdown.
- **Failure, player 1:** a Start there takes their own door's fade into the copy. It reaches black and never fades in, so
  it stays black until B.
- **Fix:** the mirror of item 1 at 28859:
  `!(split_switch() != 0 && stayer_switch() != 0 && g_fade_mover == 1 && fade_on_now(cpu))`, with an always-printed
  refusal. Run S5d already sets up this shape.

### 10. [maybe] The WARNING's look for slots 0, 1, 3 and 5 is the +64 look (14189-14195)
- After the write-back, +64 may name the other look. A -2 in one of those slots can then print a WARNING that doesn't
  apply, or miss one that does.
- It's a log only, and slots 2 and 4 (DETAIL/EXAMINE) are exact. Print the look index with the line, so a run can tell.

## Checked and holding

- **Solo, follow mode and no slots:**
  - every F2 path needs g_stay_bracket, which is set only by notify_ticket_run. That is installed only with the two-slot
    installs and coop_stay() (30218-30229).
  - g_fade_mover is co-op stay only (11436).
  - F3c in follow mode only counts.
  - F3g is one guest call of the read-only 0x801E7C98 (0x801E7C98-0x801E7D20 read only) and a print.
- **The switches off:**
  - g_stay_bracket_p1 is false (25439, and 14163 with lent).
  - stayer_window, stayer_window_on and the tail need g_split.on (not set with ED_SPLIT_HOLD=0, 25512) and the switch.
  - F3h needs both switches.
  - F3e needs g_split.on and stayer_switch().
  - The always-printed WARNING is in co-op stay brackets only, as the design asks.
- **The bracket flag:**
  - It is set once at the open (14163) and cleared at the end (14228) and at the state load (30815).
  - Every reader also tests `g_stay_bracket != 0` (19751, 20128, 20151; the carried-over line runs inside the end).
  - It can't outlive the bracket.
- **F2a's skip:** it is the existing keep pattern (passthrough false, tail_call false). It is guarded by mod_coop, sda_ready,
  r3 == -2 and not lent at the call. The game still zeroes the slot (0x80116EC8), the bracket's 7*28-byte write-back covers
  the shown record (0x80335AE0 = slot 6), and 0x80024CDC's slot to look rule (slot 2 -> 0, slot 4 -> 1) was read at
  0x80024CF8-0x80024D3C.
- **F2b:**
  - The refactor keeps the order and short-circuit (screen_term first; coop_p1_close_up not evaluated after it).
  - coop_p1_in_close_up only reads.
  - The wrapper's trace is behind the bracket flag.
- **F3a:**
  - The fade setters were read: 0x801E7C88 stores ticks and target; 0x801E7D24 stores the target and floats from the
    colour's bytes.
  - None of them is hooked, so no running guard is needed.
  - The scratch at (r1-0x200)&~15 is in the copy, above hle_guest_call's own frame at (r1-0x800)&~15 (main.c 80).
  - 0x801E7DA4 skips its step only at ticks == 0 (0x801E7DB8). With alpha 0 it never draws (0x801E7ED0-0x801E7ED4).
  - Nothing goes back to the real game at the close.
- **F3c:** it neither marks nor forgets in a copy's turn, so the real world's quads decide, as the design wants.
- **F3h:**
  - `ticks == 1` after the store at 0x801E7EA8 is the fade-in's last drawn quad. The step at ticks 1 lands on the target,
    alpha 0, and draws nothing.
  - In a refused window, forgetting at ticks 1 gives the same picture as today's forget at ticks 0. The ticks-1 quad is
    marked in both cases, and the ticks-0 quad is unmarked in both.
  - coop_room_change_on needs a count > 0, so it is false through any fade-in.
- **The mover's fade-in:** the mark only removes the quad from the stayer's view. The mover always sees their own fade.
  Nothing writes the real fade.
- **F3f:**
  - The B branch needs `window`, so the tail only takes Start.
  - With player 1's look up (kept by F2), the Start is left to the game, as today.
  - The hand-made b_eat/start_eat code (28326-28339) and StayerAgain (near coop_press_drop) are unchanged.
- **F3e:** it is only in the play branch, after step D's refusal, and it only refuses. Under ED_STAYER_PRESSES=p2 it also
  refuses player 2's own Start as the mover. That is the game's rule and was intended by the design's `stayer_switch() != 0`.
