# Refuter: R4d (player 1's door or arrival beside a scene of player 2's) against the current implementation (2026-11-12)

Lens: conflict with what is built (R1-R6c, R4b, R4c, level-3 own words/close-up/find, Tome copy, lend route, coop_follow,
hooks on shared addresses, save/load). Read only: runtime/host/hle.c (working tree on 5eb040d), build/ed-db/disasm.txt,
`ed dis/fn/door/script`, design-r4d.md, r4d-change-readers.md, r4d-arrive-readers.md, docs/coop-failed-attempts.md,
docs/coop-glitch-todo.md, docs/coop-todo.md. Nothing edited, built or launched. [read] = seen at the address or file:line
given; [inferred] = reasoned from those.

Verdict: **needs changes.** D1 (g_coop_p2_tickets), D2, D3 and D8 hold up against the code. Three of D6's paths
collide with code that is already there: the press runs inside player 1's turn (finding 1), player 1's close-up opens
the hold for player 2's turn while player 2 is in their own scene (2), and there is only one script-hold flag (3). Two
gaps in when the change counts as ran or ended (5, 6) can refuse doors for both players or lose a scene's room change
for good. D5 leaves the screen route's guards on (4).

---------------------------------------------------------------------------------------------------------------
## What holds (checked, no conflict)

- **D1's counter.** The only readers of the player-2 stamps are coop_p2_scene_on (hle.c:16928), coop_scene_p2s_on (16940)
  and their stamps (17051, 17069). Player 2's waiting way out reads g_coop_tickets at 16468/16471/16488/16560, and the
  shared coop_press_keep stamps at 19004 and drops at 20755. Each is a stamp/compare pair, so moving both sides of a pair to
  g_coop_p2_tickets keeps every other answer as it is now [read]. Solo and together bump it on every ticket [inferred from
  the construction]. Player 1's records stay on g_coop_p1_tickets (16895, 17112) [read].
- **D2/D3 as second branches** of stay_waits_clear / stay_busy_clear (hle.c:24402-24446). They are the one intercept on
  0x80163F04 / 0x801556CC (installed at 27829-27835, only with ED_COOP_STAY). A guest call to 0x801556CC from inside
  re-enters with another lr and passes through (the existing pattern) [read].
- **D8.** r13-23280 has exactly three sites: the write at 0x80152204 and the reads at 0x801521AC and 0x80154CD0
  (disasm lines 341876, 341898, 344637). The host touches it only inside coop_prompt_look (16721, 16754) and
  coop_scene_set's keep (16996), neither of which runs inside the change [read].
- **Order inside one frame.** Countdown 0x80155830 at 0x800083EC, then the behaviour pass 0x8014FDB8 at lr 0x80008400,
  then the prompt chooser at 0x800084F8 (disasm 4100, 4167). The dispatch runs a notify before the recompiled body
  (hle.c:5552-5564), so r4d_change_end at the behaviour pass's entry comes before any node update or controller of that
  frame [read]. The R4 log confirms that a whole change happens in one frame (run-20261111-221141-rooms-play: "change to
  room 342" and "change done" both at frame 2291) [read].

---------------------------------------------------------------------------------------------------------------
## Problems

### 1. BLOCKER: player 1's B on a door during player 2's hold runs inside coop_player1_turn, and coop_hold_shut wipes the ticket's busy bit
- How the press is run [read]: when coop_p2_holds is true, hle_coop_controller (an intercept on the controller
  0x80045BC4, hle.c:27007) opens the hold, runs coop_player1_turn, and shuts it again (20033-20041). It then sets
  `g_hle_passthrough = !p2_hold` (20088), so the game's own controller call for player 1 is skipped. The turn runs the
  controller with busy 0 (19602). The prompt dispatch is inside that controller: 0x80045D44 calls 0x8013E160, which reaches
  0x8013E334 and the use 0x80006EBC. So the B that D6a's shown prompt invites, and D6c's kept B that "goes in by the game's
  pad" (g_coop_p1_b_press inside the turn's COOP_PAD_UPDATE), both run their script inside the turn.
- C3's own door does fn66 at the press: L01/27 state 1 is `go_to_room[fn66](182, ...)` (`ed script 27`). fn66 runs
  0x80155770, which calls 0x801556CC(4, 1) (busy |= 4) at 0x801557E4 [read]. coop_hold_shut then writes back all four
  kept words (r13-23344, -25696, -23040, -24048) and the channel counts (19523-19531) [read]. **So the ticket's bit 4 is
  lost.** The busy word goes back to 34 while the ticket's countdown at 0x8058AFFC keeps running (the countdown does not
  look at busy: 0x80155830-0x801558F4) [read].
- What follows during player 1's own countdown [read code, effects inferred]:
  - coop_room_change_on (11281-11288) stays false. The R4 one-change refusal (16505), coop_quick_cast_ask and
    coop_node_held_ask are all off.
  - coop_p2_holds (19388-19390) stays true (34 & ~33 == 2). So hle_coop_controller keeps giving player 1 a turn every
    frame, and coop_busy_ask answers "not busy" (19443-19446).
  - In solo the asker's controller is skipped during the door's 62 ticks (busy 4). Here player 1's stick moves them
    through their own door motion and fade.
  - A door that holds first has the same problem: its 0x80199EA0(0) hold, channel counts and bit 2 are written back too.
- The mirror already has the fix: player 2's look keeps a ticket's busy word and mark when the look asked for a room
  (`asked`, coop_prompt_look 16711 and 16759-16762). coop_player1_turn has no such keep. design-r4d does not mention that
  the press runs inside the turn: its U2 asks only whether the press reaches 0x80006EBC.
- Fix: in the p2_hold branch, stamp g_coop_tickets before coop_player1_turn. If it changed, OR COOP_BUSY_ROOM into
  `p2_kept.words[0]` and keep r13-23392 before coop_hold_shut, as `asked` does. Keep the script's own hold words too:
  r13-25696 and the channel counts, if it set them. Or run D6's press outside the turn. Gate the fix on g_r4d.change so the
  together case is unchanged. Add a check to C3: busy in both.tsv should read 38 (0x26) through the countdown, not 34.

### 2. MAJOR: player 1's close-up apart (D6, check C4) opens the hold for player 2's turn inside player 2's own scene
- [read] In hle_coop_controller, `both_scenes = coop_scene_on_of(0) && coop_scene_on_of(1)` (20018) and
  `p1_look = coop_p1_close_up(cpu) && !both_scenes` (20019).
  - A plain EXAMINE close-up is not a scene. It is g_coop_p1_look kind 1, set at coop_play_hold 13112-13114, and
    g_coop_scene_camera is only set by fn22/fn139 (coop_scene_set). So both_scenes is false.
  - coop_p1_close_up counts the close-up as held with player 2's scene bits masked: 34|2 & ~(32|1) == 2 (19028).
  - So p1_look opens the hold (20025-20027), then coop_player_turn runs.
  - coop_player_turn refuses player 2's turn only on `coop_busy` (20579), which reads 0 inside the opened hold. It has
    no own-scene test (20541-20579).
- Result [inferred]: player 2 walks, with their turn and camera step, inside their own fn22 scene for as long as player
  1's close-up lasts. This is the C13 failure: the code comment at 20015-20017 ("Player 2's own scene's hold was opened
  for their turn inside it, through player 1's look, and they walked in it", blocks-user3 8352-8675) and
  docs/coop-glitch-todo.md:68 ("neither gets the other's hold opened while both are on"). The both_scenes rule only covers
  scene plus scene.
- The same thing happens through a door that asks first. Its fn21(0) by player 1 sets g_coop_p1_look kind 2
  (13121-13126).
- design-r4d section D6 says this case is covered by "both_scenes in hle_coop_controller (20018-20026)". That is wrong for
  a close-up beside a scene.
- Fix: do not open the hold for player 2's turn while coop_scene_on_of(1) (and while g_coop_script_hold), whatever player
  1 has up. Their refused turn already steps their scene's camera (20588-20589). C4 must read p2x/p2y in both.tsv: they
  should stay still through player 1's close-up.

### 3. MAJOR: D6 opens player 1's holds during player 2's script hold, but the host has one script-hold flag and keeps the other's hold only for scenes
- [read] D6a's gate includes `|| g_coop_script_hold`, and so does D3's. But coop_play_hold sets
  `g_coop_script_hold = r3 == 0 && the player is player 2` at every script hold step (13120). So player 1's fn21(0)
  (a door that asks first, L01/103) sets it to **false** while player 2 is still in their own climb (script 132).
- Player 1's later let-go fn21(1) goes to the R4c branch. There `other_scene = coop_p2_scene_on() || coop_scene_p2s_on()`
  (13080), which does not include player 2's script hold, so it passes through. The game's 0x80199EA0(1) then lets go of
  the one hold and clears bit 2 for player 2 too [inferred from the R4c comment at 13073-13077: the same let-go freed
  player 1's zombie scene in 222138-rooms-play 2390].
- design-r4d's claim that "the host's both-orders keeps exist" (D6) is true for scenes and close-ups only, not for script
  holds.
- Fix: leave `g_coop_script_hold` out of D6a's gate (player 1 apart then waits as today during player 2's climb), or make
  the script hold per player (who held it) and add player 2's to `other_scene` for player 1's let-go. D3's gate then needs
  the per-player value.

### 4. MAJOR: D5's lend round trip leaves g_coop_screen on, and two screen-keyed guards then act on player 1's change
- [read] D5 calls `coop_cameras_lend(cpu, false)` at the runner. It does not clear g_coop_screen.
  - (a) coop_close_up_kept (19117-19129) passes over every 0x80024CDC(-2) while `g_coop_screen` is on and player 1 is the
    player. The arrival's prompt reset 0x801172B0 runs exactly that cancel (0x801172B0 -> 0x80117050 -> 0x80024CDC ->
    0x80024B24(0), docs/research/separate-rooms/r4d-change-readers.md:67). So the cancel that D5 says "lands in player 1's
    records" is skipped, and player 1's look record keeps a close-up from the old room.
  - (b) coop_scene_set gives a scene to player 2 when `g_coop_screen && running != g_coop_p1_used_script` (16976). The
    runner runs Y's arrival script at 0x801527F4, inside the D5 window. A fn22(1) or fn139(1) there of player 1's own
    (unless it is their last pressed script) is then recorded as player 2's scene (g_coop_scene_p2s, 17066-17069). Player 1
    would then walk in it and player 2 would be held by it.
- Fix: inside the change (g_r4d.lent) treat the screen as off for these two decisions. Give coop_close_up_kept an
  `&& !g_r4d.lent` test, and make coop_scene_set's owner use `(g_coop_screen && !g_r4d.lent)`. Or state D5's limits
  plainly. It is only reached by a scripted change while lent, which is rare but is exactly the "Bring X to Y" route the
  design names.

### 5. MAJOR: a type-17 ticket never reaches notify_ticket_run, so g_r4d stays "change, not ran" for 900 frames
- [read] When the ticket's +20 is 17, the countdown calls 0x80152298 directly (0x80155894-0x801558A8) and sends event 62
  itself (0x801558C0-0x801558D0). It never calls the runner 0x80152614, where notify_ticket_run is hooked (27615-27622).
  Type 17 is the sanity effect's "it wasn't real" return (0x8006BFAC, ticket at 0x8006BFF4, docs/game/systems/sanity.md:99)
  and ed29 (docs/game/systems/doors.md:3). Mods' Sanity Effects tool and real sanity effects both reach it.
- With player 1's sanity return apart during player 2's scene [inferred from the design's gates]:
  - D1 has already kept player 2's scene as on.
  - The arrival's clears (0x80152140, 0x80152188) run with `g_r4d.ran` false, so D2 and D3 do nothing: busy goes to 0
    under a scene the host still holds as on.
  - D4, D8 and the R1 stale guard live in r4d_change_end and need `ran`, so none of them runs.
  - **The side guard `g_r4d.change && !g_r4d.ran` passes over every fn66 of both players, including player 2's own doors,
    until the 900-frame run-out.** hle_save_held answers held for the same time, and r4d_p1_free is false.
- Fix: count the change as ran at the change itself. A notify on 0x80152298 covers both routes (0x801558A8 and the runner's
  own call). Better, end the "not ran" state when the ticket's countdown reaches 0, not after a frame timer. The R1 stale
  guard should also run in that case. (R4's bracket has the same hole for player 2's type-17 change: not this item's.)

### 6. MAJOR: the side guard drops a scene's own fn66 for good
- [read] The R4 refusal sets `cpu->gpr[3] = 0` and returns (16505-16513), so the script steps past its fn66 as if done.
  Its comment ("the second waits for a new press") assumes a press. design-r4d widens the refusal to "player 2's scene
  script's own, a timer's" fn66 during player 1's countdown.
- A scene whose last step is its room change (a scripted move, a chapter's way on) would lose that change. The scene is
  left with no ticket and its holds still set. That is a stuck game in a linear chapter [inferred]. This is the same
  kind of loss as p1scene 3 (docs/coop-failed-attempts.md:514: a scene whose end never came, both players frozen).
- Fix: refuse only presses and zone look-ins. Keep a scripted fn66 that arrives during the countdown (its seven
  arguments) and write it through the game's own 0x80155770 at the next free behaviour pass after r4d_change_end.

### 7. MINOR: D4 also frees player 1 from their own arrival scene
- [read] The arrival holds the current player's node (0x801F6828(r31, 1) at 0x80152190). Then the runner runs Y's
  arrival script at 0x801527F4. A fn22(1) there sets busy 32|2 from player 1 (the sender is skipped by event 102).
- D4 lets player 1's node go whenever `busy != 0 && bit 8`, without asking whose bits are on. In solo, that arrival-scene
  hold is what holds the player through the scene [inferred: held = busy && bit 8, r4d-arrive-readers.md G2].
- Fix: remember D3's `keep` in g_r4d, and let go only if the busy word still equals it (nothing of player 1's was added
  in the arrival).

### 8. MINOR: D7 misses Case B's first look
- [read] coop_camera_chosen counts player 1's first look in the room (p1_zones == 1, set by coop_scan_room_start at
  19777-19798) as a room choice (17230-17234). Only coop_stay_apart turns it away, and it is false once they are
  together. g_r4d is already cleared by then (r4d_change_end).
- If player 2 is still `p2_bare` (a scene that began before any zone of theirs chose, such as an arrival scene),
  coop_camera_p2_in's follow (15404-15416) remakes their set from player 1's camera in the middle of their scene.
- Fix: in Case B, do not count room choices while coop_scene_on_of(1).

### 9. MINOR: after D1, a stale player-2 scene flag survives a state load longer
- [read] Neither hle_coop_state_load (28020-28090) nor coop_state_loaded (28225-28235) resets g_coop_p2_scene,
  g_coop_scene_p2s or g_coop_scene_camera.on. Today the next ticket of either player cuts the first two.
- With D1, a ticket of player 1's apart no longer cuts them, so a flag carried over from a state load stays until player 2
  changes room. While it stays, player 1's way out waits for good once together (coop_way_out_p2_up, 16435-16438).
- Fix: in the Step E reset, also clear g_coop_p2_scene and g_coop_scene_p2s, or set them again from the loaded RAM:
  r13-23280 and busy 32, as coop_state_loaded does for the close-up.

### 10. MINOR: D3's gate reads g_coop_script_hold, which problem 3 shows can be wrong
- If player 1's own hold reset the flag earlier in the same countdown, D3 does not fire for player 2's climb, and the
  arrival clears its bit 2.

---------------------------------------------------------------------------------------------------------------
## Failed attempts checked
- p1scene 3 (docs/coop-failed-attempts.md:514). The design is not a repeat in Case A, because D1-D3 keep the scene's end.
  But problem 6 reopens its failure: a scene whose end never comes.
- doorhold-1/3 (542-543). Not repeated: D4 uses the game's own let-go, and problem 1 is about the busy word, not the held
  ask.
- C13 / blocks-user3 (in the hle.c:20015-20017 comment and docs/coop-glitch-todo.md:68, not in the failed-attempts file).
  D6 repeats it with a close-up beside a scene (problem 2).
