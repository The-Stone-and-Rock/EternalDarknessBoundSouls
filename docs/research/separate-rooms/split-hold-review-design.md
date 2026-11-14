# Split hold: review against the design and the disassembly

Read only. I changed nothing, built nothing and ran nothing. I reviewed the working tree's runtime/host/hle.c (the split
block at 24517-24965 and its call sites) against design-split-hold.md §2-3, §11 and §19, and against
build/ed-db/disasm.txt. [real] means I read the code or disassembly that proves the point. [maybe] means it is inferred,
or the path that triggers it was not traced.

## Verdict

Every register read at each lr, every install code word, and the order in coop_busy_ask match the design and the
disassembly. The dispatcher's passthrough and tail-call rules are followed. One real deviation from the design changes
play: G2 waits for a cutscene that was already up when the ticket was written (finding 1). Finding 2 is a real gap
with a small effect. The rest are [maybe] or are notes.

## Findings

### 1. [real] G2 waits for a cutscene that was up before the ticket, so a door ticketed during a scene can stall up to 900 frames

- **Where:** hle.c:24868-24869 (`split_tick`) and hle.c:24811 (`split_ticket_open`, which records `word0` but has no
  cutscene baseline).
- **What is wrong:**
  - The design waits only for "a scene, held words, a find or a cutscene **begun in the window**" (§3.9, §7 "in a
    cutscene: both held, unchanged; one begun in the window makes G2 wait").
  - §9 also says that for a scene-first door (L03/420's event 82), "the scene's bits are up at the ticket, so they are in
    word0 and G2 does not wait for them".
  - The code baselines the busy bits (`extra = ... & ~g_split.word0`) but not `cutscene_playing()`. It waits whenever
    `cut` is true and `+0 > 1`.
- **Failure:**
  - Co-op with ED_COOP_STAY, either player the mover. The game writes a ticket with a countdown above 1 while the
    letterbox scene flag is set or a movie is open. Examples: fn66 inside a scripted scene, or a scene-first door such as
    L03/420 (K12).
  - split_ticket_open opens the window. It never tests for a cutscene, and both rooms are valid.
  - From the first tick, `cut` is true. The countdown and the fade delay stop until the letterbox drops, or until
    SPLIT_WAIT_LONGEST (900 ticks).
  - If the scene's letterbox stays up until the room change, the door fires about 900 world frames late. Solo and today's
    build fire it on time.
  - With player 1 as the mover, their input lock is also renewed for the whole wait.
  - The cinematic's own return ticket (0x8012D2EC) has countdown 1 (transitions.md:297), so it never waits.
  - The code is [real]. That some scripted door is ticketed under the letterbox is [inferred]. No run was made.
- **Fix:** add `bool cut0;` to g_split. In split_ticket_open set `g_split.cut0 = cutscene_playing(cpu);`. In
  split_tick use `const bool cut = cutscene_playing(cpu) && !g_split.cut0;`.
  - Simpler and closer to §7: do not open the window at all when `cutscene_playing(cpu)` is true at the ticket. Use
    `no = "a cutscene at the ticket"`, so that today's hold applies.

### 2. [real, minor] hle_save_held does not wait for G4's interrupt bracket, though it waits for R6b's

- **Where:** hle.c:29619 (`... || g_r6b.open;`). G4's bracket is hle.c:24941-24951.
- **What is wrong:**
  - R6b clears MSR[EE] across a route job and makes saves wait while it is open.
  - G4 clears the same bit on the same thread (split_job_open) but is not in hle_save_held.
- **Failure:**
  - A state is saved inside a G4 job: inside a door's countdown, a route job built outside R6b's bracket.
  - On load, hle_coop_state_load zeroes g_split_job, but the background thread's saved MSR has EE off.
  - At the job's OSDisableInterrupts (lr 0x800BB9E4), split_job_close is not called because `open` is false. mfmsr then
    reads EE=0, and OSRestoreInterrupts puts back 0. The route thread keeps running with interrupts off after that.
  - Each of its jobs then runs whole and cannot be pre-empted at the retrace. That is G4's state, made permanent; it is
    not a hang.
  - Whether a save point can be reached while that thread runs with EE off is [inferred]. R6b's author assumed it can.
- **Fix:** add `|| g_split_job.open` to hle_save_held's return.

### 3. [maybe] G2 clears the dispatcher's flags before its guest call (safe today, fragile)

- **Where:** hle.c:24886-24898.
- **What:**
  - `g_hle_passthrough = false; g_hle_tail_call = false;` are written before `hle_guest_call(0x800443E4)`.
  - Any intercept dispatched inside the call would overwrite both globals. If it left passthrough true, the outer
    dispatcher would run the tick anyway, and the wait would silently not happen.
- **Read:**
  - 0x800443E4 is `bl 0x800068E0; bl 0x8013E99C`, and both are leaf max-stores.
  - None of the three entries has a hook (grep of hle.c), so hle_dispatch returns false without touching the flags
    (hle.c:5554-5556).
  - hle_guest_call saves and restores CPUState up to srr0, including lr, pc and msr (main.c:57-112).
  - Calling it from an intercept is the same pattern coop_player1_turn uses.
  - So it is **safe now** [real].
- **Fix (hardening):** set the two flags after the guest call, just before returning from split_tick.

### 4. [maybe] G3 can read the wrong player's spell word when r13-21488 is swapped without the casting words

- **Where:** hle.c:24732-24734.
- **What:**
  - `p2_in` is decided by r13-21488 == player 2's id. That holds for coop_be_player2, which swaps r13-21488 together with
    the four casting words (hle.c:12360-12379).
  - coop_player1_turn (hle.c:19850-19863) writes r13-21488 := player 1's id without touching the casting words.
- **Failure:**
  - If coop_be_player2 is active when coop_player1_turn runs (player 1's turn inside player 2's screen) and player 1
    writes a door ticket there, then mover 1 / stayer 2 reads g_coop_casting[0], which is player 1's spell.
  - The window then opens with player 2 mid-cast, or is refused when player 2 is not casting.
  - I did not prove that be_player2 is on in that turn.
- **Fix:** decide from the host's own state, not from the id word. For example, track a `g_coop_casting_swapped` flag
  that coop_be_player2 toggles. Alternatively, read player 2's spell as `g_coop_p2_is_player ? r13-22336 :
  g_coop_casting[0]`.

### 5. [maybe] S3 takes player 1's id from r13-21492, not from r6_player1_node

- **Where:** hle.c:26049 (`p1_node = r13-21492`; this is earlier code) feeds `ids[0]`, which S3 now uses as "the
  stayer, player 1" (hle.c:26099-26101).
- **What:**
  - The task's rule is that player 1's node comes from r6_player1_node.
  - coop_enemy_target returns early under g_coop_turn and g_coop_step_running. Any other swap would make
    `ids[0] == ids[1] ==` player 2.
  - Example: R4's bracket or R6c's depth while together. R6c is off while together, so I found no concrete case.
  - With mover 2 / stayer 1, S3 would then set `want = ids[0]`, which is player 2: the mover.
- **Fix:** `const u32 p1_node = r6_player1_node(cpu);` (and p1 = its +52). This is harmless for today's callers.

### 6. [real, note] G1's line is not "once per (lr, room)" as the notes say

- **Where:** hle.c:24826.
- **What:** `again` compares only with the last refused (lr, room). Two writers that alternate print every time. One
  writer that repeats is suppressed, which is the case that matters (a timer). Harmless.
- **Fix:** keep a small seen list if the notes' wording should hold.

### 7. [maybe, design-level] G2 freezes the fade delay too

While G2 waits, +4 is not counted either, which matches the design's "+0 and +4 not counted". A fade that already began
(+4 reached 0 before the wait) runs on in its own timer. The mover's half can then sit faded for the whole wait. The
door's countdown also stops while the mover's state-121 walk finishes. The design accepts this (K9 checks it). I list it
so the K9 run looks at the mover's half.

## Checked and correct

### Registers at each lr

| Site (bl, lr) | Register read | Disassembly | |
|---|---|---|---|
| A1 0x8000749C, lr 0x800074A0 | r28 | `or r3,r28,r28; bl 0x801F68D4` at 0x800074A8 | ok |
| A1 0x800271F0/0x80027298/0x80027764/0x800277F4, lr +4 | r31 | `or r3,r31,r31` before each 0x801F68D4 (0x800271FC, 0x800272A4, 0x80027770, 0x80027800). An answer of 0 takes the not-held path at each one (0x80027218, 0x800272B4, 0x80027780, 0x80027810) | ok |
| K1/A3 0x80058E80, lr 0x80058E84 | r31 node, r28 message, r18 event | 0x80058DF8 `or r31,r3`; 0x80058DF4 `or r28,r5`; 0x80058E10 `or r18,r3` from 0x801F5384(r28). The message is +0 event, +4 sender, +8 receiver (0x801F57E0-0x801F57EC) | ok |
| A2 0x801F541C, lr 0x801F5420 | r29 message, +8 receiver id, +0 event | 0x801F5408/0x801F54FC load r29; `lwz r3,8(r29); bl 0x801F5F88`; event 75 at `lwz r0,0(r29)` | ok |
| S2 0x801F5688, lr 0x801F568C | r31 receiver node | broadcast loop 0x801F564C-0x801F5694; r3 is not read after the call, and r31/f31 are callee-saved | ok |

Other notes on these sites:

- The host's r6_node_of_id copies 0x801F5F88: the current id gives r13-21492, else a walk of the list by +16.
- coop_object_room (+52 → +592) is the same as 0x801F662C → 0x80118CAC (`lwz r3,592(r3)`).
- The node's kind is +8 (0x801F62D0).

### Order in coop_busy_ask (hle.c:19715-19740)

1. split_busy_kept (K1, then the K7 sites) comes first and keeps the game's word.
2. Today's four cases come next, unchanged.
3. The allow list is an `else if (split_busy_allows)` under split_on, last.

The K7 kinds are 9, 28, 32, 39, 50, 56, 73, 74 and 82 (§19.1). The K7 lr sites are 0x800E891C, 0x800A29B8 and
0x800A2C24 (§19.2): each is a `bl 0x80155738` + 4 in disasm.txt.

### Dispatcher (hle.c:5554-5590)

- The dispatcher clears both flags before the intercept, runs the notify only on passthrough, and calls hle_return
  unless g_hle_tail_call is set.
- Every new intercept sets passthrough=true first, and sets both false only when it handles the call. All four
  functions are void to their callers:
  - the writer 0x80155770 ends at 0x80155814 with no r3;
  - the tick 0x80155830;
  - the dispatcher 0x800B7208 (0x800B72FC-0x800B7308);
  - the send at the broadcast site.

### Install words (hle.c:29415-29441), against the disassembly

| Entry | Words checked |
|---|---|
| 0x80155770 | 0x9421FFF0; +0x38 0x801557A8 `stb r0,-23392(r13)` = 0x980DA4A0 |
| 0x800B7208 | 0x9421FFF0; +0xC 0x800B7214 `lwz r4,-25172(r13)` = 0x808D9DAC |
| 0x80155830 | 0x9421FFF0; +0x10 0x80155840 `addi r5,r3,-20484` = 0x38A3AFFC |
| 0x801F57C0 | 0x9421FFC0; 0x801F5688 = 0x48000139 (offset 0x138) |
| 0x800443E4 | +0x14 = 0x4BFC24E9 (bl 0x800068E0) |

### Coexistence with hooks already on these entries

- notify_room_ticket (hle.c:29135-29137) and notify_change_tick (kChange, hle.c:29159) are installed earlier in the
  same find_coop. The split install accepts exactly those notifies.
- The hurt log's notify on 0x801F57C0 (hle.c:29449-29456) is installed later and accepts `intercept == split_send`.
- 0x80155738 holds only coop_busy_ask, and the split code is inside it.
- 0x80152298 keeps notify_room_change, and G5 is its first line.
- 0x801F5620 keeps notify_change_hold_event. It is a different entry, and it uses split_send_keeps for its count.
- 0x800B7208 had no hook.
- No other hle.c install targets these entries.
- In the copy turn the dispatcher skips an entry whose notify is in tome_copy_hook_off. split_counting is false there
  anyway.

### Order inside the writer

The record's +0 is stored at 0x801557BC before the busy set at 0x801557EC. The busy word is stored at 0x801556F4 before
the broadcast at 0x80155720. So split_on is already true at the door's own 102s, and S2 sees them.

### G1's request pass-over

- The code mirrors `addic. r0,r3,-1; stw; bgt` at 0x800B722C-0x800B7234: it passes over only when `delay - 1 <= 0`
  (signed), for request 1, 16, 32 or 64 with the mask matching.
- Skipping the entry leaves r13-25172 and -25176 alone. The clear is at 0x800B72F0, past the skip.

### G2 (apart from finding 1)

- It waits only while `+0 > 1`, so the tick that fires (+0 == 1) always runs, and so do notify_change_tick and the
  +0 == 1 readers.
- The renewal is for player 1 as the mover only, outside g_coop_turn, with count + 3. 0x800068E0 and 0x8013E99C are
  max-stores, so this keeps the solo margin.
- R4b's case passes through.

### S2, S3 and S4

- S2: event 102 at lr 0x801F568C only, split shape only, nodes of g_split.room, with kind 19 and K7 still told.
- S3: in a split, `want` is the stayer, or 0 when the stayer is held; only B is written when the answer is 0, as the
  design says.
- S4: `if (split_on) return;` sits before the same-room branch.

### G4

- G4 opens only at lr 0x800BB918, outside R6b's bracket and under split_counting. It closes at lr 0x800BB9E4, before
  OSDisableInterrupts reads the MSR.
- r6b_frame closes a bracket that is still open in the frame.

### G5 and the state-load reset

- G5 logs at the top of notify_room_change under split_counting, and is always printed.
- §11: split_state_load zeroes g_split and g_split_job inside hle_coop_state_load.

### Players and nodes

- Player 1's node always comes from r6_player1_node, and player 2's from g_coop_p2. This holds in split_mover_node,
  split_stayer_node and split_ticket_open.
- The mover is taken from g_coop_room_p2, which notify_room_ticket sets just before split_ticket_open.

### Co-op only

- Nothing is installed without coop_stay() and the strip-walk intercept, so follow mode and failed slot installs have
  none of it.
- With the hooks installed, every intercept passes through unless g_split.on is set. Only split_ticket_open sets it,
  and only with mod_coop, g_coop_p2, coop_stay() and outside g_tome_copy_turn.
- split_counting tests mod_coop, g_coop_p2 and g_tome_copy_turn again, so a mid-window switch-off goes inert at once.

## Not in hle.c (as the notes say)

`ed timeline` listing `[split]` lines (§3.13) was not built.
