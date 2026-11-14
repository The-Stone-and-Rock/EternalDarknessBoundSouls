# Split hold: safety review (co-op both players, solo)

This reviews the split-hold build in runtime/host/hle.c (working tree) against design-split-hold.md §2-3, §11 and §19, and the
notes in split-hold-impl-notes.md. It was read only: nothing was changed, built or run. Line numbers are from the working
tree as read on 2026-11-13.

- [real] means I read the code that proves it.
- [maybe] means the code fact is read, but whether play reaches it, or what it does there, is not proved.

## What holds (checked, no finding)

- **Solo, follow mode, no ED_ROOM_SLOTS installs.**
  - g_split.on is set in one place only, split_ticket_open (24806). That needs g_split_ready, mod_coop, g_coop_p2,
    coop_stay() and sda_ready (24777).
  - g_split_ready is set only in the install block (29432). That block needs coop_stay() and the stay's own
    coop_stay_strip_walk intercept, which is installed only after the two-slot install succeeded (29061-29101).
  - find_coop runs at boot whatever mod_coop is, so with ED_COOP_STAY and ED_ROOM_SLOTS=2 set in solo the hooks are
    installed. Every runtime path still starts from g_split.on, split_counting or split_on, and split_counting re-tests
    mod_coop and g_coop_p2 (24603). So nothing acts in solo.
  - Follow mode (no ED_COOP_STAY) never installs anything.
- **The Tome copy turn.**
  - split_counting returns false in the copy turn (24603), and split_ticket_open refuses with a reason (24786).
  - notify_room_ticket is not on tome_copy_hook_off's list (28079-28091). So a ticket written in the copy turn still
    clears g_split.on (24776).
  - coop_busy_ask, coop_character_step, r6b_job_open and r6b_job_close are on the list, so K1/K7/A1-A3, S3 and G4 don't
    run there at all.
- **ED_SPLIT_HOLD=0.**
  - The hooks are installed, but split_ticket_open never sets on, so every intercept passes through. The dispatcher runs
    the notify after a passthrough exactly as it does with no intercept (5568-5582).
  - coop_busy_ask's two new branches return false and fall through to the old code unchanged (19719, 19731).
  - coop_follow's `split_on` is false (21526).
  - coop_enemy_target: with split_target_stayer at -1 the old `held[0] != held[1]` branch runs. The new `want == 0`
    branch can't be reached, because the player ids are non-zero. So it matches HEAD (HEAD 24630-24640).
  - G4 needs split_counting (24939).
  - The hurt log's notify on 0x801F57C0 still installs (29454).
  - I found no other installer on 0x80155770, 0x80155830, 0x800B7208 or 0x801F57C0 later in find_coop that the new
    intercepts would now refuse. The ticket notify (29135) and the tick notify (29159) are installed earlier.
  - The only differences are log text: the [split] "holds as today (ED_SPLIT_HOLD=0)" line, and [change-hold]'s frame
    line, which gains two words.
- **Both players' nodes.**
  - split_mover_node and split_stayer_node take player 1's node from r6_player1_node (24598-24599). That covers player
    2's turn (g_coop_turn_p1), R6c (g_r6c_p1) and coop_as_player2 / coop_use (g_coop_p1_node).
  - split_ticket_open takes both rooms from the players' own nodes (24782).
  - K1 compares r31 with the mover's node for either mover. Player 2's filter runs through coop_as_player2(COOP_EVENTS)
    (22082) with r31 = g_coop_p2.
  - A3 compares r31 with the stayer's node.
  - If r6_player1_node returns 0, the check fails safe:
    - with player 2 the mover, split_counting goes false;
    - with player 1 the mover, K1 doesn't match, and A1 already refuses node 0.
- **The stayer's room in both shapes.** g_split.room = rs, which is the stayer's own room (24808). Every ask re-tests the
  stayer's own room (24606).
- **S2's exceptions.**
  - A kind-19 or K7 receiver is passed through (24921).
  - A roomless node gets -1 from coop_object_room (13996), and g_split.room is never -1 (24788). So roomless nodes always
    get the 102.
  - Only the broadcast's call (lr 0x801F568C) is filtered, so kind 19's own 102s go through.
  - At the entry, r31 is the loop's node: 0x801F568C reloads r31 from 4(r31) after the call (disasm 509100).
- **The window's end.**
  - It ends at the tail (14530), at a new ticket (24774), and at a state load (29627).
  - split_counting needs ticket +0 > 0, so a window left open is inert once the ticket fires or is cleared.
  - G2 has a cap of SPLIT_WAIT_LONGEST = 900 frames per wait. A new wait can start only after a frame where the tick
    counted, so the total is bounded by count × 900.
  - G2 never holds +0 at 1 (24869).
  - The lock renewal runs only for player 1 as the mover, outside g_coop_turn (24895).
  - hle_guest_call saves and restores the whole CPU state up to srr0, so the intercept's return through lr is safe
    (main.c:70-108).
- **G1's request pass-over leaves the request in place.** Per the disassembly, an early return skips the decrement's
  store and the clear (0x800B7228-0x800B7234, 0x800B72F0). The function is void.

## Problems

### 1. G2 waits on any cutscene, not one begun in the window [real code; failure maybe]

- **Where:** hle.c:24868-24869 (split_tick), with nothing recorded at split_ticket_open (24806-24812).
- **What is wrong:**
  - `cut = cutscene_playing(cpu)` is tested raw.
  - The design waits for "a cutscene begun in the window" (§0.6, §3.9). word0 masks the scene bits that were up at the
    ticket, but nothing masks a cutscene that was already playing.
  - split_ticket_open opens a window whatever coop_room_change_on says, so a ticket written under a letterbox or a movie
    opens one.
- **Failure (either player the mover, both shapes):**
  - The ticket is written while cutscene_playing is true. Candidates:
    - ed145's scene return, 0x800ADA24 through request 1 (b0-k7 §2.1);
    - a cinematic's own return, 0x8012D2EC (b0-k7 §2.2);
    - a scene-first door such as L03/420 (§9), if its letterbox is up.
  - If the letterbox or the movie stays up until the load, the countdown holds for the full 900 frames. Today it fires in
    about 62.
  - With player 1 as the mover, the lock is renewed all that time. With player 2 as the mover, see #5.
- **Fix:** add `bool cut0` to g_split and set `g_split.cut0 = cutscene_playing(cpu)` in split_ticket_open. In split_tick:

  ```c
  if (g_split.cut0 && !cut) g_split.cut0 = false;   // the one up at the ticket is over
  const bool cut_new = cut && !g_split.cut0;
  ```

  Then use cut_new where cut is used now. A simpler alternative is to open no window when cutscene_playing is true at the
  ticket, as G3 does for a cast: `no = "a cutscene at the ticket"`.

### 2. S2 is limited to the shared shape, but apart the door's 102 can reach the stayer's room [real code; failure maybe]

- **Where:** hle.c:24911 (split_send_keeps) and 24916 (split_send): `g_split.shared` is required.
- **What is wrong:**
  - The design says "apart, 102 never reaches the stayer's room" (§3.4). That holds only when the current id at the busy
    set is the mover.
  - The busy set broadcasts to the room of the current id's node (0x80155700-0x80155710: 0x801F5F88, then 0x801F662C).
  - notify_room_ticket also makes player 2 the mover through `exit_p2` (11368-11369): player 2's way-out script whose
    later steps "go on from the game (a wait's end, run as player 1's)" (11316-11319). The current id is then player 1's.
- **Failure:**
  - Player 2, apart in room B, takes a scripted way out whose ticket comes after a wait, so it is written as player 1's.
  - The 102 goes to room A, which is player 1's room: the stayer's room.
  - S2 is skipped because the shape is apart. Room A's creatures are chase-cut and get bit 8, while A1 frees their
    update. What the stayer sees is the M3 stop's half: creatures that stop chasing at the ticket.
  - (In that case the game's input lock also lands on player 1's copy. That is today's behaviour, not new.)
- **Fix:**
  - Drop `g_split.shared` from split_send and split_send_keeps. The `coop_object_room(node) == g_split.room` test
    already limits it to the stayer's room.
  - Apart with the mover as the sender, the broadcast's own room filter never reaches that room, so nothing changes
    there.
  - S3 can stay shared-only (R6 D4 covers apart).

### 3. G1 with G2 makes risk 12 certain whenever a cinematic starts inside the window [real code; outcome maybe]

- **Where:** hle.c:24820-24833 (split_ticket_ask) with 24868-24892 (split_tick).
- **What is wrong:**
  - A cinematic begun in the window makes G2 hold the countdown for as long as it plays.
  - At its end, 0x8012D1A0 writes its return ticket while the door's ticket still counts, because G2 kept it counting.
    G1 refuses it.
  - b0-k7-readers §2.2 asked for a rule ("let it replace the door's ticket, or end the window first"). §19.3 kept "refuse
    and log". The build follows §19, so this isn't an implementation error. But G2 turns a rare overlap into one that
    always happens once a cinematic begins in the window.
- **Failure:**
  - Event 242 to the stayer is not in the busy filter's list, and A3 frees the stayer's filter in any case. Or a kill
    wakes a script with ed55.
  - Mode 6 runs, and its room change (record +120) is lost. Per b0-k7 §2.1 and §2.2 [inferred], the players are left in
    the cinematic's room and camera.
- **Fix (pick one, and log it):**
  - In split_ticket_ask, at lr 0x8012D2F0, call `split_over(cpu, "a cinematic's own ticket")` and pass through, so the
    cinematic's ticket replaces the door's as solo would order them.
  - Or, in split_tick, don't wait for a cutscene in mode 6 (while the tick is called from 0x80050A90), so the door fires
    first and the cinematic's ticket comes after the window.

### 4. S3 reads player 1's id from r13-21492 [maybe]

- **Where:** hle.c:26049 (`p1_node = r13-21492`) and 26086 (`ids[0]`), used by S3 at 26099-26102.
- **What is wrong:**
  - The line is old code, but S3 is the new user that has to be right for both movers.
  - coop_enemy_target returns in g_coop_turn. It doesn't return in the other swaps: coop_use's coop_be_player2
    (18191-18264) and coop_as_player2.
  - In those, r13-21492 and r13-26524 are player 2's. Then ids[0] == ids[1] == player 2's id, and p1 == p2.
- **Failure:** player 2 is the mover in a split (shared), and a creature step runs while player 2's words are in. stayer
  = 0, so `want = ids[0]`, which is player 2's id: the mover. The creature is turned onto the mover for the window. That
  is exactly what K1 and S3 are meant to stop.
- **Fix:** in S3 take player 1's id from r6_player1_node:

  ```c
  const u32 n1 = r6_player1_node(cpu);
  const u32 sid[2] = {guest_ram_range_valid(cpu, n1, 20u) ? mem_read32(cpu, n1 + 16u) : 0u, ids[1]};
  if (stayer >= 0) want = held[stayer] || sid[stayer] == 0u ? 0u : sid[stayer];
  ```

  Or return from S3 when `mem_read32(r13-21492) == g_coop_p2`.

### 5. Player 2 as the mover in a G2 wait: their door turns run out [real code; consequence maybe]

- **Where:** hle.c:24899-24904 (it only logs at 51 frames); 21788 (`g_coop_door_turns_until = frame + COOP_DOOR_FADE`,
  120); 20868 (player 2's turn is refused while busy unless coop_door_turns).
- **What is wrong:** b3-input-lock §5-6 found that the ticket's lock never held player 2 when they are the mover.
  - In a long wait, player 2's door motion and state 121 end before the firing. Their own unlocked copy then walks them
    until 120 frames after the motion.
  - After that, their turn is refused as busy.
- **Failure:** a G2 wait longer than about 58 frames in player 2's door.
  - Player 2 (the mover) can walk, attack and so on from about +65 to +120, still in the room they are leaving.
  - Then they stand frozen until the firing, which can be up to 900 frames later. The R4 runner still places them at the
    firing, so they are not stranded.
  - Player 1 (the stayer) is fine: their game copy is locked 65 frames, then free.
- **Fix:** while g_split.waiting with mover 2, renew `g_coop_door_turns_until = frame + COOP_DOOR_FADE` each waiting frame
  (host state only). Also lock player 2's own copy as b3 §6 describes: their g_coop_state[1] r13-23592 := max(…, +0 + 3).
  Or cap G2 for mover 2 at about 50 frames.

### 6. G3 decides whose casting words are in memory by r13-21488 [maybe]

- **Where:** hle.c:24732-24734.
- **What is wrong:**
  - coop_be_player2 swaps the casting words together with r13-21488 (12361-12379). coop_player1_turn writes r13-21488 :=
    player 1's id with no casting swap (19854-19863), inside player 2's scene, where player 2's casting words are in.
  - Then p2_in is false while player 2's words are in memory.
- **Failure:** a ticket of player 1's written inside coop_player1_turn (player 1 takes a door during player 2's scene).
  stayer_p2 is true and p2_in false, so G3 reads g_coop_casting[0], which is player 1's own word. A casting player 2 gets
  a window: the case G3 exists to stop.
- **Fix:** track the swap in host state, for example a `g_coop_casting_p2_in` bool flipped in coop_be_player2's casting
  loop, and use it instead of r13-21488.

### 7. G4's open job isn't in hle_save_held [maybe]

- **Where:** hle.c:29612-29619 (has `g_r6b.open`, not `g_split_job.open`) and 29627 (split_state_load zeroes
  g_split_job).
- **What is wrong:** R6b holds saves while its bracket has MSR[EE] cleared. G4 clears MSR[EE] the same way (24941-24942)
  but doesn't hold saves.
- **Failure:** a state saved while the route thread is inside a G4 job.
  - On load, g_split_job.open is 0, so split_job_close never puts EE back on that thread at the job's done.
  - This is unlikely, because the main thread can't run while EE is off unless the job blocks. It is the same reason
    R6b gives for holding saves.
- **Fix:** `... || g_r6b.open || g_split_job.open;`.

### 8. A room request held by G1 fires in the new room, as whoever is current [maybe]

- **Where:** hle.c:24837-24851; the world frame calls the dispatcher right after the tick (0x800083EC, then 0x800083F4).
- **What is wrong:**
  - A held request 1/16 (ed145's scene return, 0x800ADA24) or 32/64 (0x800AD6F4) runs in the same frame the door fires,
    after the arrival.
  - 0x800ADA24 tickets the current player 0x801F62B0 to the scene's kept room (b0-k7 §2.1).
  - If the scene that asked was in the stayer's room, the player made current there is the arrival's player (the
    mover's room is in play).
- **Failure:** shared, player 1 the mover:
  - player 2's or the room's scene in room A ends inside the window (ed145);
  - player 1 arrives in room C;
  - the request runs, and player 1 is ticketed straight back to A (transition 17), or put on the kept place by 0x800AD6F4.
  - Solo never meets a request held across a change.
- **Fix:**
  - Hold the request after the window while the room in play isn't g_split.room. Keep a `requests_room = g_split.room`,
    and pass the dispatcher over until a frame where the room word is that room again, or the stayer has left it.
  - At least, print a loud `[split] a held room request runs in room N, not room R` at the first call after the window.

### 9. S4: a stayer's fall that begins in the window is never rescued if the change leaves them apart [maybe]

- **Where:** hle.c:21526 (S4's return before the same-room branch), and 21560-21567 (apart: `stood_z = NAN`, no rescue).
- **What is wrong:**
  - The design accepts dropping the fall rescue for the window (§3.6). But the creatures that now move and push the
    stayer are new in the window.
  - After a player-1 door the shape is apart, and the apart branch has no fall rescue.
- **Failure:** player 2, the stayer, is pushed off a ledge in the last frames of player 1's countdown and falls for good in
  their own room.
- **Fix:** let only the `fallen` half of the same-room branch run under split_on (coop_place beside player 1 is wrong
  then, because player 1 is the mover). Or keep a stood-still place for player 2 and put them back there. Otherwise list it
  with design-fall.md's open apart falls.

### 10. Small points [real]

- **A3 for a held stayer.** A3 frees the stayer's filter even when coop_player_held(stayer) is true (24722-24723).
  - The game's quarry run then answers 59 for them, and S3 writes B := 0 at the next step (26101-26108).
  - A creature can flip between "after the stayer" and "nobody" every frame while player 2 is in words or a close-up.
    Today's outside-the-window rule does the same thing with the other player, so it's not new in kind.
  - Watch for it in K1 runs with player 2 in words.
- **COOP_TARGET_A isn't touched** when S3 writes B := 0 (26104-26110). This is as R6 D4 does. If A holds the mover's id,
  it stays [maybe: what A drives is unread].
- **The mover's children pass A1.** split_node_freed leaves out the mover's node only (24630). Their weapon, light and
  similar child nodes in the shared room are freed (impl notes 6). Watch both halves' shots in K1 and K2.
- **The window opens whatever coop_room_change_on says.** G1, G2, K1 and K7 then run under a menu as well. A Tome item door
  of the stayer's that writes its ticket outside the request dispatcher is refused by G1 [maybe: the item is used and the
  door lost]. K11 covers it.

## Summary for the next build

Must fix before play:
- #1: snapshot the cutscene at the ticket.
- #2: drop `shared` from S2.
- #4: S3's player-1 id from r6_player1_node.

Should fix:
- #3: a rule for lr 0x8012D2F0.
- #5: player 2's door turns in a wait.
- #7: hle_save_held.

Watch in runs:
- #6, #8, #9, and the items under #10.

No new path runs in solo, in follow mode, without the two-slot installs, or in the Tome copy turn. ED_SPLIT_HOLD=0 behaves
as before, apart from log text.
