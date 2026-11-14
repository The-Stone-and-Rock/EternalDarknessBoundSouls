# Refuter: R4d (player 1's door or arrival beside a scene of player 2's), lens: the overall goal and both players (2026-11-12)

Read only: design-r4d.md, r4d-change-readers.md, r4d-arrive-readers.md, runtime/host/hle.c (working tree, HEAD 5eb040d),
the disassembly (`ed dis`, `ed fn`, `ed door`, build/ed-db/disasm.txt), docs/game/systems/sanity.md,
docs/game/research/transitions.md, docs/coop-failed-attempts.md and the user's testing notes in memory. Nothing was edited,
built or launched. [read] means seen at the address or file:line given. [inferred] means reasoned from those reads.

Verdict: **needs changes.** The core (D1-D3, D5, D8) holds up for player 2's fn22 scene. The problems are these. The gate
is wider than the design says, and D4 and D7 run on every apart door of player 1's. Player 2 gets no matching freedom.
Several of player 1's actions are still blocked by player 2's scene through the shared scene-hold word. The side guard can
lose a scene's own room change. And C6 expects the wrong log, so the checks can't prove the build as written.

---------------------------------------------------------------------------------------------------------------
## Blocker

### B1. The change gate fires for every one of player 1's doors apart, not only beside player 2's scene. D4 and D7 then change doors that work today, and C6 s2 expects the wrong result.
- The gate (design section 3) is `mod_coop && coop_stay() && g_coop_p2 && !g_coop_room_p2 && r6_apart(cpu)` plus a
  running-script test. Nothing in it needs any of player 2's things to be on [read design-r4d.md:119-122]. r6_apart is
  true for any two different resident rooms [read hle.c:24197-24202].
- What then runs on an ordinary apart door with no scene:
  - **D7** returns before `room_choices++` for player 1's arrival camera [design:241-245]. On a Case B door with no scene
    (player 1 joins player 2's room), that changes the camera rule that has worked since R4. A bare player 2 set
    (p2_bare) is no longer remade from the room's start, while coop_camera_chosen still remakes it today
    [read hle.c:17227-17236].
  - **D4** lets go of player 1's node whenever `busy != 0 && bit 8`, with no test of whose bits those are
    [design:194-197]. On any apart door into a room whose arrival script starts something of player 1's, busy is not 0
    at the behaviour pass. The arrival script runs at 0x801527F4, after the busy clear at 0x80152184 and the re-hold at
    0x80152190, and before the behaviour pass [read dis 0x80152184-0x80152190, 0x801527E0-0x801527F4]. Examples: a
    scene, a hold, or the block items' arrival 1161 [read `ed door 183`: items 1100-1103 "then 1161"]. See M1 for the
    effect.
  - **D1** stops player 1's apart tickets from dropping player 2's waiting way out and kept B. That is a change in today's
    apart doors too. It is probably the better behaviour, but it is a change.
- **C6 s2 contradicts the gate.** The s2 recipe runs `ED_RUN_SCRIPT=110:119,170:119,650:29,710:29` with
  `ED_PUT="3680,2,1305,1508,185"` [design:421]. Script 119 is 183 -> 180 and 29 is 180 -> 183 [read `ed door 183/180`].
  1305,1508,185 is room 183's Puzzle_Mkr04 spot (the user's testing note). So at 29's ticket player 1 is in 180 and
  player 2 is in 183: apart. The gate fires, `[r4d] a room change of player 1's beside player 2 in room 183` prints, and
  D7 runs. The design expects "in s2 no [r4d] line ... together at the ticket" [design:450-451]. A correct build would
  read as failing, or a reader would wave the line through.
- **Fix:** either
  - narrow the gate to "a thing of player 2's is on at the ticket or comes on before the runner". Store it as
    `g_r4d.beside`, set at notify_room_ticket and again at notify_ticket_run from
    `coop_scene_on_of(1) || g_coop_script_hold || g_coop_screen || g_coop_words_theirs || g_coop_find_theirs || coop_p2_own_up()`.
    Then gate D4, D5, D7 and the log on it, and leave D1 on the broad gate if wanted; or
  - keep the broad gate deliberately, rewrite C6 s2's expected lines, and add a no-scene apart Case B check that compares
    both halves' cameras with the last build's.

---------------------------------------------------------------------------------------------------------------
## Major

### M1. D4 is built on a wrong solo premise, and its let-go removes the hold that every arrival leaves on the player.
- The design says the arrival's re-hold "in solo is inert" and that after D4 "player 1 is as solo (not held)"
  [design:191-199]. But 0x801F6828(r31, 1) at 0x80152190 runs on every arrival, solo included, so after any door the
  solo player's node has +28 bit 8 set [read dis 0x80152188-0x80152190].
  - fn66 is what clears it for the next door ("lets the player move while busy", transitions.md:71 [read]).
  - Event 102 skips its sender [read design fact 3, 0x801F5654-0x801F568C]. So in a scene the player starts, the player
    is posed only through that kept bit 8 [inferred].
- Whose data: D4 writes player 1's node, read by the motion steps (0x800271F0..0x80027804) and the node tick for player 1
  alone. It never touches player 2's node.
- When the bit matters in co-op: coop_busy_ask already answers "not busy" to every caller while player 1's close-up or
  scene holds (coop_p1_close_up), and while player 2's fn22 scene or script hold holds (coop_p2_holds) [read
  hle.c:19435-19446, 19015-19046, 19378-19391]. So D4 is never needed for the fn22-scene and script-hold kinds, which
  are the only ones D6 makes reachable.
  - It matters for the busy states the host does not answer: player 1's own words or find with their hold (busy 8 or 16
    with 2), and player 2's lend-route words, find or screen.
  - In those states, after D4, player 1 in room Y is not posed until their next door [inferred].
- **Fix:** don't write the game's bit.
  - Answer "not held" for player 1's node alone in the existing per-node intercept coop_node_held_ask (0x801F68D4,
    hle.c:19458-19479), while the busy bits are exactly the ones D3 kept (record them in `g_r4d.kept`) and nothing of
    player 1's is on (`!coop_scene_on_of(0) && !g_coop_p1_look`).
  - Or drop D4 and limit D3's keep to kinds coop_p2_holds answers.
  - Either way the arrival's re-hold stays as solo has it.

### M2. Player 2 gets no matching freedom: apart, they still wait beside player 1's scene, close-up, find or words.
- The design says "Player 2's prompts while player 1's scene is up apart ... are already free; D6 is the same freedom
  for player 1" [design:238-239]. Only a prompt press beside player 1's bare fn22 scene is free.
  - **Walk-in way out:** player 2's look passes `waits = p1_up` with `p1_up = coop_way_out_p1_up(cpu)`. That covers
    player 1's scene, mid-use, words or find, with no room test [read hle.c:16430-16433, 16559, 16602]. A zone's fn66 of
    player 2's then waits [read hle.c:16523-16530].
  - **B on a prompt:** kept while `coop_player1_mid_use` (player 1's close-up or find) [read hle.c:20739-20740]. It goes
    in only when none of `coop_player1_mid_use`, `g_coop_tome_owner >= 0` and the rest are up [read hle.c:20761-20763].
    No room test.
- After R4d the rules split: player 1 may leave while player 2 examines in another room, but player 2 may not leave while
  player 1 examines in another room. That breaks the user's rule to read and build every case for both players.
- **Fix:**
  - Build the mirror in the same build: `r4d_p2_free` is r6_apart with nothing of player 2's own up.
  - Pass `p1_up && !p2_free` at hle.c:16602.
  - Use `coop_player1_mid_use(cpu) && !p2_free` at 20739 and in the go-in wait at 20761.
  - Add one check: player 2 walks out of 183 while player 1 examines in 180.
  - Or record it as R4e explicitly. The mirror case's busy and wait keeps already exist (R4b: stay_busy_clear keeps
    player 1's close-up, hle.c:24429-24430).

### M3. D6 frees nobody for player 2's p2s or fn139 scenes: player 1's controller is held there.
- D6a opens for `coop_scene_on_of(1) || coop_p2_holds || g_coop_script_hold` [design:217-218], and coop_scene_on_of(1)
  includes coop_scene_p2s_on [read hle.c:16957].
- Who reads what:
  - A fn139 scene of player 2's, or a fn22 begun as player 1 under their screen, is recorded only as g_coop_scene_p2s.
    g_coop_p2_scene is set only for fn22 with p2_acts [read hle.c:17046-17066].
  - coop_p2_holds needs `g_coop_script_hold || coop_p2_scene_on()` [read hle.c:19383-19386], so it is false.
  - fn139 sets busy 2 [read r4d-arrive-readers G3]. With player 1's node holding the arrival's bit 8 (M1), busy is not
    answered 0 and player 1 is posed in X.
- So "they walk by coop_p2_holds as today" [design:272] is true only for player 2's own fn22 scene and their script
  hold [inferred; not in any log].
- **Fix:** read and extend coop_p2_holds' scene term to p2s scenes while apart, gated on r6_apart, or list these kinds as
  outside R4d. Either way, add a p2s or fn139 case to U7 rather than leaving it "for play".

### M4. Player 2's scene still blocks player 1's other actions, through the one scene-hold word r13-25696, which D8's pattern doesn't cover.
- r13-25696 is set by fn22, fn139 and the words (stores at 0x800429EC, 0x80042AF0, 0x80042B7C [read]). It is read
  through 0x8004303C at 20 or more sites [read disasm]. While player 2's scene is on in R2, every reader in player 1's
  room sees "a scene is on":
  - **Player 1's Tome or quick-spell press:** 0x80006CDC returns at 0x80006D0C-0x80006D14 when it is set [read]. hle.c
    names it "the Tome's opening asks (0x80006D0C)" [read hle.c:19071].
  - **Sanity effects at player 1's doors:** 0x8006DE30-0x8006DE40 and 0x8006DEE8-0x8006DEF8 return 0 when it is set
    [read]. sanity.md:85 says the game turns every effect away while it is on [read]. D6 now sends player 1 through doors
    during player 2's scenes, and those doors lose their effects.
  - **Creatures in player 1's room:** the zombie behaviour branches away at 0x8003B38C-0x8003B394 [read; what the
    0x8003B42C branch skips is not read]. R6 enemies-by-room never swaps this word [read: no 25696 write in r6_*].
- The design holds r13-23280 across the change (D8) for the same reason, a shared scene word read by the change, but not
  r13-25696.
- **Fix:**
  - Hold r13-25696 at 0 across player 1's change, from the runner to r4d_change_end, as D8 does. That covers
    0x80043380's effect start at 0x80152748, inside the change.
  - Record the Tome, quick-spell and creature reads as known R4d gaps, with a probe: in C1, ED_COOP_ENEMY_LOG shows
    room 180's or 182's creatures during 419; and an attempt at player 1's quick spell during 419.

### M5. The side guard can lose a scene's own room change for good, which breaks the linear game.
- The guard passes over any fn66 while `g_r4d.change && !g_r4d.ran` [design:257-260], as R4's refusal does
  (`cpu->gpr[3] = 0`, hle.c:16505-16513 [read]). R4's comment says "the second waits for a new press": that holds for a
  press, but not for a script step.
- A scene's own fn66 (player 2's scene script asking for its room as its last step, or a timer's) is not asked again.
  The scene never moves on and its hold stays [inferred].
- D6 makes this window reachable by an ordinary press of player 1's, 62 frames of countdown per door, during exactly the
  scenes that are most likely to end with a room change.
- **Fix:** defer instead of refusing: keep the second fn66's arguments and who asked, and run it through the game's own
  fn66 path as that player after r4d_change_end. Or refuse only presses (`g_coop_way_look` / `COOP_PROMPT_RUNS` lr) and
  defer script steps. Log which.

### M6. With ED_COOP_STAY, "together at the ticket" is a separation, and it keeps today's broken cut of player 2's scene.
- With the stay switch, player 2 never moves on player 1's change: they are the stayer (coop_stay_stayer,
  hle.c:13843 [read]). The readers' A1 counted every change of player 1's under coop_stay() for that reason
  [r4d-arrive-readers.md:191-198].
- The design narrows the gate to apart at the ticket [design:119], so if player 1 leaves R2 while together, player 2's
  scene in R2 is still cut:
  - at the ticket, the g_coop_tickets bump (hle.c:11300) [read];
  - at the arrival, busy is cleared [read dis 0x80152184];
  - its end is lost (r4d-change-readers 2.6).
- Ways this happens:
  - not by a press: the kept B and the waiting way out hold player 1 together;
  - by a script step, timer or cinematic end of player 1's;
  - by player 1's Tome item that moves them (L01/1100-1103 to 180 EntryMkr04 [read `ed door 180`]);
  - by the planned "Bring X to Y".
- The task rule says the players-together case must not change. This case starts together and ends apart, so the user
  should decide.
- **Recommendation:** open D1-D3 when `coop_stay() && g_coop_p2 && !g_coop_room_p2 && coop_scene_on_of(1)` at the
  ticket, whatever the rooms. Player 2 stays either way, so only the separation case changes, never a
  together-staying-together case. Ask the user before building it.

### M7. The checks can't prove several pieces, and they ignore the user's testing rules.
- **Player 1's presses (C3, C4)** use ED_INPUT_SCRIPT. The user's testing note says it "skips controls.cpp, so it can
  pass where the real pad fails", and asks for ED_PAD_SCRIPT with pad_slot_p1 = 3 in a settings copy for player 1's
  buttons. D6a and D6c are exactly about player 1's B. Use ED_PAD_SCRIPT.
- **Not tested at all:**
  - D4 and M1: an apart door into a room whose arrival starts something of player 1's. In chapter 1, player 1 using a
    block item from 180 (1100-1103, arrival 1161 into 183) while player 2's 419 runs, which is Case B plus player 1's
    own arrival scene.
  - D2: 419 waits on its camera path, not in the game's table [design:76-79]. C1 and C2 never put a wait of player 2's
    there, so U3 stays open after every check.
  - D5 (lent): never reached.
  - The p2s and fn139 kinds (M3).
  - The mirror direction (M2).
- **Player 2's side in C1-C4:** only a stick, never a B. Nothing tests player 2 pressing during or after player 1's
  change (D1's kept B and way out). Add a player 2 B on a prompt after 419's end in C1.
- **Pictures of both halves:** listed for C1 only. C2-C5 name one half or none. For C3 and C4, add "player 2's half on
  419's cameras through player 1's prompt", since D6a zeroes busy, r13-25696 and r13-24048 round the chooser every
  frame. For C5, add both halves.
- **The user's reported hitch** (one half pauses while the other changes room): C1 should read the `ms` column of
  both.tsv across player 1's load for player 2's half. D6 makes player 1's loads mid-scene common.
- **Run count:** 10 launches (the state, C0-C5, two C6, C7). The user asked for few runs and as little opening and
  closing as possible.
  - Drop C0: walk3 and C1's own pre-ticket rows are the baseline.
  - Fold C4 into C3 (EXAMINE first, then the door in the same 419 window).
  - Fold C6 s1 and s2 into one run (player 2's door, then player 1's door).
  - Use the shots (`ed see`) and `ed timeline` for both halves instead of extra runs.

---------------------------------------------------------------------------------------------------------------
## Minor

- **m1. D8's put-back can leave r13-23280 stuck at 1.**
  - D8 writes the word back if it is still 0 at r4d_change_end [design:250-252]. If player 2's scene ends inside the
    window, its fn22(0) wrote 0 legitimately, and the put-back makes it 1 with no scene. Every later arrival of either
    player then skips the visited mark (0x801521AC [read], 0x80154CD0).
  - The window runs from the runner to the behaviour pass. Case B's event 62 to R2's nodes and the arrival script both
    run inside it.
  - **Fix:** put it back only if `coop_scene_on_of(1)` is still true.
- **m2. The free gate's `g_coop_tome_owner != 0` treats "player 2 has the shared Tome" as free.**
  - The owner is -1 for none, 0 for player 1 and 1 for player 2, and is "who has the screens that stop play" [read
    hle.c:3920].
  - Use `g_coop_tome_owner < 0`, or exclude owner 1 when `g_tw.sharing`.
- **m3. D6a shows player 1's save point prompt during player 2's scene apart.**
  - The design notes this [design:328-332], but in-game saves while apart are untested by the user's own account, and
    the save would write busy 34 and r13-25696 = 1 [inferred].
  - Until that test, leave a save prompt hidden apart during player 2's thing (the save verb's script, found the
    engine-level way), or make the save wait as hle_save_held does (hle.c:28008-28015).
- **m4. Event 62 into R2 (U4) is only measured in a scene.** The user's own example is a fight. Case B with a creature
  engaged with player 2 is every apart Case B, scene or not, since R4. Add a creature-engaged entry to C2, or one run
  where player 2 is fighting in 183 when player 1's 29 arrives.
- **m5. The running-script test keeps a known one-player misattribution.**
  - A fn66 from player 2's scene script that runs as player 1 moves player 1 [design:121-122]. That breaks "whoever
    triggers a room gets it".
  - The host already knows the script (`running == g_coop_p2_scene_script` / `g_coop_scene_of[1].script`), so it could
    set g_coop_room_p2 there and run it as R4's bracket. Read which game paths reach this first:
    coop_run_script's p2_scene_end and coop_p2_script_is routes (hle.c:18483-18560) already make most of them player 2's.
- **m6. D3 drops busy bit 1 for player 1's change, while R4b keeps it for player 2's.** stay_busy_clear keeps
  `busy & ~4` [read hle.c:24434]. The two directions follow different rules. Keep them the same, or record why in both.
- **m7. Player 2's letterboxed cutscene or lend route (screen) apart still holds player 1 at doors.** The free gate
  excludes `cutscene_playing`, `g_coop_screen` and `lent` [design:126-129]. That is consistent but incomplete. Record it
  as a gap, not as "free".

---------------------------------------------------------------------------------------------------------------
## What holds (checked, no problem found)
- Design fact 3: player 1's ticket's event 102 holds only player 1's room's nodes. The readers' B4 is not needed
  [design:51-55].
- D1's construction leaves solo and same-room tickets as today: every stamp is taken from the counter it is compared with.
- D6a's hold open/shut round one guest call uses the existing CoopHoldKept, which restores all four words and the
  channels' counts [read hle.c:19507-19531].
- The finisher's r13-24048 bit 8 is already kept per player (g_coop_p2_busy_bits, hle.c:21591-21600), so a fight of
  player 2's does not hide player 1's prompts through that bit.
- The fresh-state recipe (Jump to Game, door scripts 101/105/111/29, state thrown away on the next change) follows the
  user's rule.

## Answers to the design's questions (section 12)
1. Bit 1: not settled from code. Keep it as R4b does (m6), unless a log shows it stuck.
2. Linear game: D6 does not make chapters independent. But M5 (a scene's room change lost during player 1's countdown) and
   M4 (player 1's door sanity effects turned away by player 2's scene) are rule breaks D6 makes reachable.
3. D1 and player 2's way out: with D1, player 2's waited exit is let go with `here` true after player 1's apart door, and
   their next look comes into it. That is new behaviour for today's apart doors (B1) and needs a check.
4. D5 is unreachable by D6 and has no check. Leave it out of this build, or add a scripted lent case.
5. D8: the visited index has no other reader [design fact 7]. The risk is the put-back (m1), not the hold.
