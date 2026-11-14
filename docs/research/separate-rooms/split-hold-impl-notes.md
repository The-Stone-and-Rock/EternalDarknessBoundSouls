# Split hold: implementation notes

This is what was built from design-split-hold.md: §2-3, §11, and the final decisions of §19, which override the earlier
sections. Everything is in runtime/host/hle.c, and the line numbers below are from the working tree after the change. The
build is clean: build.bat ends at "Linking CXX executable EternalDarkness.exe" with no errors. The only hle.c warning that
isn't a getenv deprecation is the existing one at 5611. The game was not run, `ed test` was not run (it launches the game
through run.sh), and nothing was committed. hle.c stays LF.

## Pieces and where they are

| Piece | hle.c | What |
|---|---|---|
| Forward declarations | 3270-3277 | split_counting, split_on, split_busy_kept, split_busy_allows, split_ticket_open, split_over, split_room_load |
| The block (defines, g_split, g_split_job, switches) | 24517-24600 | SPLIT_* addresses, g_split_ready, g_split_lock_ok, `g_split` {on, shared, mover, room, from, to, word0, logs}, `split_switch` (ED_SPLIT_HOLD), `split_target_mover_test` (ED_SPLIT_TARGET_MOVER=1), `split_log` (ED_COOP_ENEMY_LOG or ED_COOP_LOG) |
| split_counting / split_on | 24602-24609 | §2. split_counting also needs mod_coop and g_coop_p2. Player 1's node comes from r6_player1_node, player 2's from g_coop_p2 (split_mover_node, split_stayer_node) |
| K7 kinds, kind 19, K7 sites | 24612-24628 | §19.1 kinds 9, 28, 32, 39, 50, 56, 73, 74, 82; §19.2 lr 0x800E891C, 0x800A29B8, 0x800A2C24 |
| A1's test | split_node_freed, 24629 | the object room (+52 → +592) is g_split.room, the node is not the mover, its kind is not 19 or K7. Roomless nodes never pass |
| S1 K1/K7 | split_busy_kept, 24690; called first in coop_busy_ask, 19719 | K1 (lr 0x80058E84 and r31 == the mover's node) and K7 sites keep the game's word, before today's cases |
| S1 allow list A1-A3 | split_busy_allows, 24709; coop_busy_ask's new `else if`, 19731 | A1 lr 0x800074A0 (r28) and 0x800271F4/0x8002729C/0x80027768/0x800277F8 (r31); A2 lr 0x801F5420 (message r29, not event 75, receiver from +8 through r6_node_of_id); A3 lr 0x80058E84 with r31 == the stayer's node. Only under split_on, after today's four cases |
| G3 | split_stayer_casting, 24731; used in split_ticket_open | no window for a stayer with a spell word set |
| Window open | split_ticket_open, 24773; called at the end of notify_room_ticket, 11389 | §2 Set, with the reason logged when it doesn't open (switch, apart under "split", casting, copy turn, no room) |
| Window end | split_over, 24756; called in notify_room_slots_tail, 14530; also when a new ticket comes with no tail between | the over line with the counts, then one line per lr |
| G1 tickets | split_ticket_ask (intercept on 0x80155770) | a second ticket in the window is refused and always logged. lr 0x8012D2F0 (a cinematic's own return) is logged as lost (risk 12) |
| G1 requests | split_requests (intercept on 0x800B7208) | §19.3: passes over requests 1/16/32/64 inside the window, always logged once per window |
| G2 | split_tick (intercept on 0x80155830), split_r4b_case 24854 | waits while +0 > 1 and busy bits 8/16/32 begun in the window or a cutscene are up, except R4b's case. Player 1 as mover: 0x800443E4(+0 + 3) each waiting frame. Player 2 as mover: no renewal, and a line after 50 frames |
| S2 | split_send (intercept on 0x801F57C0), split_send_keeps | in a split, the door's 102 (lr 0x801F568C, event 102) is not sent to the shared room's nodes; kind 19 and K7 still get it. The sum is printed at the next tick |
| S2 in [change-hold] | notify_change_hold_event, 26010 | `, kept from room R's nodes by the split rule (n)`. Kept nodes are left out of the "told" count |
| [change-hold] frame line | change_hold_frame, about 25947 | gains `r13-24048 0x..` and `r13-23392 n` |
| S3 | split_target_stayer; coop_enemy_target, 26096-26112 | split shape, not under ED_SPLIT_TARGET_MOVER: `want` is the stayer, or 0 when the game holds the stayer (B := 0, `[coop] N passed over: player M is held`) |
| S4 | coop_follow, 21526 | `if (split_on(cpu)) return;` after the trackers' declarations, before the same-room branch |
| G4 | split_job_open / split_job_close (24938-24951); r6b_job_open 25421 (both early returns), r6b_job_close, r6b_frame | a route job's graph build (lr 0x800BB918) that R6b doesn't bracket, inside the window, runs with MSR[EE] off until lr 0x800BB9E4 |
| G5 | split_room_load; first line of notify_room_change, 11297 | a direct 0x80152298 inside the window, always logged |
| §11 | split_state_load; hle_coop_state_load, about 29626 | g_split and g_split_job zeroed |
| Install | find_coop's ED_COOP_STAY block after R4b, 29408-29441; the hurt log's accept, about 29453 | see below |

### Install

- **Where:** inside find_coop's `getenv("ED_COOP_STAY")` block, after R4b. It needs coop_stay() (ED_COOP_STAY and
  ED_ROOM_SLOTS=2) and the stay's own install to have succeeded: 0x80043820's intercept must be coop_stay_strip_walk.
- **Checked against main.dol** (all match):

  | Function | Address | Words checked |
  |---|---|---|
  | ticket writer | 0x80155770 | 0x9421FFF0; +0x38 = 0x980DA4A0 (stb r0,-23392(r13)) |
  | dispatcher | 0x800B7208 | 0x9421FFF0; +0xC = 0x808D9DAC (lwz r4,-25172(r13)) |
  | tick | 0x80155830 | 0x9421FFF0; +0x10 = 0x38A3AFFC |
  | send | 0x801F57C0 | 0x9421FFC0; the call at 0x801F5688 = 0x48000139 |
  | lock | 0x800443E4 | 0x9421FFF0; +0x14 = 0x4BFC24E9 (bl 0x800068E0). It only gates g_split_lock_ok |

- **The notifies already on these entries are accepted:** notify_room_ticket on the writer, notify_change_tick on the
  tick, and nothing on the dispatcher or the send. The ticket's and the tick's notifies are installed earlier in find_coop
  (about 29136 and 29158), so they are already there when this runs.
- **The hurt log's notify on 0x801F57C0 is installed later.** It now accepts `intercept == split_send`.
- **The intercept runs first, and the notify only on passthrough** (the dispatcher at about 5560). So a refused ticket runs
  no notify_room_ticket, a waiting tick runs no notify_change_tick, and a kept 102 runs no hurt_note_later.
- **None of the new hooks is in tome_copy_hook_off's list.** Adding one would also have turned off the notifies that share
  its entry in the copy's turn. Each new path tests g_tome_copy_turn itself, through split_counting and split_ticket_open.

### Solo, follow mode, copy turn

- g_split.on is set only in split_ticket_open, and only with all of these:
  - g_split_ready (the hooks installed);
  - mod_coop;
  - g_coop_p2;
  - coop_stay();
  - outside g_tome_copy_turn.
- Every predicate starts from g_split.on or split_counting. So:
  - in solo (mod_coop off), in follow mode (no ED_COOP_STAY), or without the two-slot and stay installs, nothing is
    installed, or what is installed passes through;
  - in the copy's turn, split_counting is false.
- ED_SPLIT_HOLD=0 installs the hooks, but no window ever opens.

## Where the build departs from the design, and why

1. **The mover's filter is logged by its event list, not by its answer (S6).**
   - The design asked for a notify on 0x80058E84's return path. That address is inside 0x80058DD8, not a function entry,
     and the dispatcher hooks entries only.
   - So the line is printed at the K1 ask itself. The event is in r18 (0x801F5384(r28) at 0x80058E10) and the sender at
     message +4.
   - It is printed only when the real word is non-zero and the event is in 0x800CA964's taken list, as mover-readers §2
     decodes it: 8, 10, 11, 24, 25, 30, 32, 39, 50, 53, 55, 57, 59, 60, 63, 67, 101, 103, 107, 125, 133, 202.
   - The room 208/64 exceptions for 8/11/39/59 are not modelled, so in those two rooms the line can claim a take that
     didn't happen.
   - It prints once per (event, sender) per window, and the rest are counted.
2. **G1's request pass-over runs only when the dispatcher would act.**
   - It passes 0x800B7208 over only when the request is 1/16/32/64, the mask matches, and the delay would run out this
     call: `(s32)delay - 1 <= 0`, which is the code's `addic.`/`bgt` at 0x800B722C-0x800B7234.
   - A call that would only count the delay down is left to the game, so the request's timing is the game's until it
     would fire.
   - The disassembly confirms the pass-over leaves the request in place. Skipping the entry leaves r13-25172 and r13-25176
     as they are, and the function is void (0x800B72F0-0x800B7308; every caller ignores r3). So no fallback log was
     needed.
3. **G2 has a safety cap (SPLIT_WAIT_LONGEST = 900 frames).**
   - The design has the countdown wait for as long as a scene, words, a find or a cutscene begun in the window is up.
   - If a bit begun in the window were never cleared before the change (for example a door script that sets bit 32 after
     its own ticket and waits for the change), the door would never fire.
   - After 900 frames of one wait, the tick goes on (it fires under the scene, as it does today), with an always-printed
     line.
4. **G2 is told in [split] lines.** The design didn't give the wording:
   - "the countdown waits at N: busy 0x.. begun in the window[, a cutscene] (player M the mover)";
   - "the countdown goes on at N after W frames (why)";
   - "has waited over 50 frames in player 2's door".
5. **G2's lock renewal is skipped inside player 2's turn (g_coop_turn).** In that turn the game's input copy might not
   be in. The tick is called at the top of the world frame and of the cinematic frame, outside any turn, so this should
   never come up.
6. **A1 never frees the mover's own node.** The design's A1 test was "room and kind". The mover's node and the
   mover's children (whose bit 8 fn66 clears) would run anyway, because the held ask answers "not held". Leaving the mover
   out keeps it on the game's word everywhere, as K1 intends.
7. **G1's line is printed once per (lr, room) in a window.** It is still always printed. Repeats are counted on the over
   line. A writer called every frame (a timer) would otherwise flood stderr.
8. **The S2 sum is printed at the next tick, not per send.** The intercept is called per receiver, and the line sums one
   broadcast. It is printed at the next tick and on the over line.
9. **The window's end has two more places.**
   - split_over runs at the change's tail, as designed.
   - It also runs when a new ticket arrives while g_split.on is still set (a change that didn't reach the tail). That
     ticket can only get there with +0 == 0, because G1 refuses one inside the window.
   - split_counting needs +0 > 0, so a stale g_split.on is inert either way.
10. **The install also needs the stay's own strip-walk intercept** (coop_stay_strip_walk on 0x80043820). Without it,
    ED_ROOM_SLOTS=2 + ED_COOP_STAY would install split hooks even though the two-slot install had failed. That is "without
    ED_ROOM_SLOTS installs" from the task.
11. **The G3 casting read uses the controlled id.**
    - The design says "r13-22336 when the stayer's words are in memory, g_coop_casting[0] when swapped out".
    - The build decides "in memory" by r13-21488 == player 2's id. coop_be_player2 swaps the casting words together with
      r13-21488 (hle.c:12350-12370). coop_as_player2 swaps only the three who words, not the casting words.
12. **Design line numbers have shifted.**
    - coop_busy_ask is now at 19715 (design 19704).
    - coop_follow's same-room branch was at 21507 before the edit (design 21502).
    - notify_room_slots_tail is at 14526 (design 14516), hle_coop_state_load at 29624 (design 29086), and
      coop_enemy_target at 26045 (design 25554).
    - The R4b install block and the hurt log moved by about 500 lines. That is earlier uncommitted work plus this block.
    - The content at each site was as the design describes.
13. **The registers were checked, not assumed.** The design's 0x80058E84 "node in r31" and 0x801F5420 "message in r29"
    were checked in build/ed-db/disasm.txt (0x80058DF8, 0x801F5408-0x801F542C). So were the A1 registers: r28 at
    0x800074A8, and r31 at 0x800271FC, 0x800272A4, 0x80027770 and 0x80027800. All are as the design says.

## Not confirmed (no run was made)

- **That any of it works in play.** K1-K12 from §14 have not been run. `ed test` was not run either, because it launches
  the game.
- **The S2 log's "kind-19/K7 nodes told" count** includes every K7 or kind-19 node of the room that the broadcast reached.
  The broadcast's own sender skip is the game's, so the sender is never counted.
- **G4's msr handling on the background thread.** It copies R6b's way (MSR[EE] cleared at the build, put back at the
  job's OSDisableInterrupts). split_counting is evaluated there, on the background thread. It reads r13 words and the
  stayer's object room, which should be stable while the main thread waits for the retrace [inferred].
- **G4 exists only where R6b's notifies are installed** (ED_COOP_STAY and ED_ROOM_SLOTS set, and R6b's code check
  passing). Without them, jobs aren't held.
- **The K7 list is §19's,** from the readers. The open points stay open:
  - who sends event 67 to kind 28;
  - whether anything sets the tracker's bit 8;
  - kind 56 is kept for safety.
- **coop_p1_close_up has a side effect** (its `on` static and the look's end). It is no longer called for asks that K1
  or K7 answer first. It is still called at every other ask in the same frame, so its state should step as before
  [inferred].
- **S3's "passed over"** writes only COOP_TARGET_B, as R6 D4 does. COOP_TARGET_A is left as it was.
- **`ed timeline` listing `[split]` lines** (§3.13) is a tools/ed change and was not made. The task kept this to hle.c.
- **The risks of design §15 remain as written:**
  - risk 3: a scripted fn66 woken in the window is lost to R4;
  - risk 5: event 61 at the firing;
  - risk 12: a cinematic's own ticket refused.

## Review fixes

The two reviews are split-hold-review-design.md (D1-D7) and split-hold-review-safety.md (S1-S10). Each point was checked
against hle.c and build/ed-db/disasm.txt. Line numbers are from the working tree after these fixes. The build is clean:
build.bat ends at "Linking CXX executable EternalDarkness.exe", and the only hle.c warning that isn't a getenv deprecation
is the existing one at 5611. The game was not run and nothing was committed. hle.c stays LF.

### Fixed

| Point | Verdict | What changed |
|---|---|---|
| D1 / S1: G2 waits for a cutscene that was up before the ticket | **real.** split_tick tested `cutscene_playing` raw, and word0 masks only the busy bits | split_ticket_open opens no window when `cutscene_playing` is true at the ticket: `no = "a cutscene at the ticket"` (hle.c:24799). Today's hold applies, as §7 says ("in a cutscene: both held, unchanged"), and so do §9's scene-first doors under a letterbox. Inside a window, G2's `cut` can now only be a cutscene begun in it. A cinematic's own return ticket (countdown 1) now never opens a window either |
| D2 / S7: hle_save_held doesn't wait for G4's bracket | **real.** G4 clears MSR[EE] on the route thread as R6b does, but only R6b held saves | `|| g_split_job.open` added to hle_save_held (hle.c:29672) |
| D3: G2 clears the dispatcher's flags before its guest call | **real as hardening** (safe today: none of 0x800443E4, 0x800068E0, 0x8013E99C has a hook) | `g_hle_passthrough = false; g_hle_tail_call = false;` moved to the end of split_tick, after the guest call (hle.c:24947) |
| D4 / S6: G3 decides whose casting words are in memory by r13-21488 | **real.** coop_player1_turn writes r13-21488 := player 1's id (hle.c:19862) with no casting swap, so the id word is no guide | New `g_coop_casting_p2_in` (hle.c:12356), flipped by coop_be_player2 each time it swaps the casting words (12381). coop_be_player2 is the only place that swaps them (grep of kCoopCasting), so the flag is exactly whose words are in memory. It is reset to false in hle_coop_magick_load (29659), where g_coop_casting is restored. split_stayer_casting reads it (24737). Only G3 reads the flag |
| D5 / S4: S3 takes player 1's id from r13-21492 | **real for S3** (the old function's p1_node is r13-21492; in a swap ids[0] == ids[1] == player 2's id) | In S3's branch, the stayer's id comes from split_stayer_node (r6_player1_node for player 1, g_coop_p2 for player 2), its +16 (hle.c:26150). An id of 0 means nobody. The "goes after player N" log names the stayer by index in a split. The rest of coop_enemy_target (today's code, outside the split) is unchanged |
| D6: G1's line is not "once per (lr, room)" | **real** (it compared with the last pair only) | A seen list of 8 (lr, room) pairs per window (hle.c:24569, 24834-24839). Past eight pairs, each new one is printed. The count is on the over line, as before |
| S2: S2 is limited to the shared shape | **real.** The busy set broadcasts 102 to the room of the current id (0x801556FC 0x801F62B8, 0x80155704 0x801F5F88, 0x80155708 0x801F662C, 0x80155720 0x801F5620). notify_room_ticket makes player 2 the mover through `exit_p2` (hle.c:11367-11372) while the current id may be player 1's ("a wait's end, run as player 1's"). Apart, the 102 then goes to the stayer's room | `g_split.shared` is dropped from split_send and split_send_keeps. The room test (`coop_object_room(node) == g_split.room`) still limits it to the stayer's room. Apart with the mover current, the broadcast never reaches that room, so nothing changes there. S3 stays shared-only (R6 D4 covers apart) |
| S8: a room request held by G1 runs after the change wherever the game is | **real** (b0-k7 §2.1: 0x800ADA24 tickets the current player to the scene's kept room; the world frame calls the dispatcher at 0x800083F4 right after the tick that fires). The proposed hold-until-the-room-is-back was **not** built: the dispatcher has one request word (r13-25172), so holding it open-ended would also hold a game over (8), ed88 (4) or request 2 set later | The reviewer's minimum: `g_split_request` (hle.c:24849-24880) remembers a held request and the stayer's room. At the first dispatcher call after the window that runs it, split_request_after prints `[split] THE HELD ROOM REQUEST n RUNS IN ROOM r, NOT ROOM s WHERE IT WAS ASKED` (always printed) when r13-23276 isn't that room, or a quieter line under the logs when it is. A request cleared or replaced first is forgotten. It is reset in split_state_load. Behaviour is unchanged |

### Rejected

| Point | Why, from the code |
|---|---|
| D7: G2 freezes the fade delay too | Not a defect: §3.9 has "+0 and +4 not counted this frame", and the reviewer lists it only for K9. No change. K9 should look at the mover's half |
| S3: G1 with G2 makes risk 12 certain once a cinematic begins in the window | The build follows §19.3 ("refused and logged loudly: a known rare risk"), and the reviewer agrees it is not an implementation error. Both proposed rules have unread outcomes. (a) Passing 0x8012D2F0 through overwrites the door's record while the mover is mid-door (fn66 cleared their bit, state 121), so the mover's door is lost instead. (b) Not waiting in mode 6 fires the door under the cinematic, which is what G2 exists to stop. The always-printed "A CINEMATIC'S OWN RETURN TICKET, LOST (risk 12)" line stays the watch. **Open design decision** for the next session, with b0-k7 §2.2 |
| S5: player 2 as the mover in a long G2 wait (door turns run out) | §19.4 decides "player 2 the mover: nothing is renewed; a wait over 50 frames is logged", and the 50-frame line is built. The fixes proposed are new player-2 lock semantics. Renewing `g_coop_door_turns_until` alone lengthens the stretch where their unlocked copy walks them. Writing a lock into g_coop_state[1]'s pad word is the copy that hle.c:20909-20912 says "kept a lock for good" once, and it is counted down only in their turns, so with their turn refused it would come back after the arrival. The R4 runner still places them at the firing. Left for K9 and the 50-frame line |
| S9: a stayer's fall in the window isn't rescued if the change leaves them apart | §3.6 drops the fall rescue for the window on purpose. The only rescue coop_follow has (hle.c:21543-21561) is coop_place beside `player` (player 1). With player 1 the mover, that would put player 2 in the door. The apart branch (21564-21573) has no rescue at any time, so this belongs with design-fall.md's open apart falls, not here |
| S10: A3 for a held stayer; COOP_TARGET_A untouched; the mover's children pass A1; the window under a menu | Watch items, no defect. A3's flip with S3's B := 0 is the same in kind as today's rule outside the window. A is left as R6 D4 leaves it. A1 frees nodes of the stayer's room that aren't kind 19 or K7, and the mover's children are such nodes (as §3.2 has it). A Tome item door under the window is K11 |

### Superseded in the notes above

- Departure 7: G1's line is now once per (lr, room) for up to eight pairs.
- Departure 11: G3 no longer uses r13-21488. It uses g_coop_casting_p2_in.
- §2's `shared` is now used only by S3 and the logs.

### Co-op only, again

- The new cutscene test is inside split_ticket_open, after its co-op gates.
- g_split_request is set only inside a window. split_request_after needs it pending and is skipped in the Tome copy turn.
- `g_split_job.open` is set only by G4, under split_counting.
- g_coop_casting_p2_in changes only where coop_be_player2 swaps (co-op paths), and only G3 reads it.
- S3's new id read is inside `stayer >= 0`, which split_target_stayer gives only in a window.
- Solo, follow mode, no ED_ROOM_SLOTS installs and the copy turn are as before.
