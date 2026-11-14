# Design (final): the other player's brief freeze when one changes room (separate rooms)

The user's report of 2026-11-12: with separate rooms on, when one player changes room the OTHER player's half pauses
"for a brief second". It does not go black. The user's example: player 1 is fighting in one room, player 2 enters another,
and player 1's screen pauses. Sources: change-freeze-measure-readers.md, change-freeze-code-readers.md, the first version of
this design, and its two refuters (refute-change-freeze-impl.md, refute-change-freeze-goal.md). Every refuter point was
checked again against the disassembly (`build/ed-db/disasm.txt`, `ed dis`), `runtime/host/hle.c`, `frontend.cpp` and the
user's own run logs/run-20261111-221141-rooms-play. No code was changed, nothing was built and the game was not run.
[read] = seen at the address, line or log row given. [inferred] = reasoned, not seen.

## 0. In short

**What the stayer loses when the other player takes a door. There are three things, and the first version only went after
the smallest part of the first:**

1. **The picture stop: 66-116 ms, every door.** One world frame runs long. The ticket tick 0x80155830 fires the change,
   0x80152298 blocks the main thread in OSReceiveMessage (0x80152398) until the room is unpacked, and the same frame then
   arrives, runs, draws and presents [read]. The stayer's half is kept lit by R4's per-half fade, so it shows its last
   picture for the whole long frame.
   - In the user's own play (run-20261111-221141, a split, player 1 341 -> 342), both.tsv goes 1843 -> 1847, which is 66 ms
     [read]. page-0001.log shows the change and "unpack asked" at 1843, then the arrival walk, the tail, `[detail]` and
     "BOTH rooms drawn" at 1844 [read 2630-2683].
   - So the unpack is about 1 of the 4 retraces. 3 come after the arrival: the rest of the frame's logic, the host's
     logging, the lights' call_sync, the new room's first draw on the render thread, and the present. How those 3 split was
     never measured.
2. **The stayer's creatures are held for the countdown (about 62 ticks, about 1 s), when the players are together at the
   door (a split).** The busy set 0x801556CC broadcasts event 102 to every node in the asker's room except the asker
   (0x801556FC-0x80155720; the room filter at 0x801F5654-0x801F5674) [read]. The host's own reading is that 102 sets each
   receiver's node+28 bit 8, and a node with that bit set does not update while busy (0x801F68D4, asked by 0x800074AC and
   the motion steps) [read: hle.c:11272-11277, 14310-14311, 19451-19455]. In a split the stayer's room IS the asker's room,
   so its creatures are told 102 and stand still for about a second. Then busy goes 0 at the arrival, and they move again.
   [inferred: the host lets only the other PLAYER out, coop_node_held_ask, hle.c:19456-19476.] The exact setter of the
   bit is not found statically: see the probe in P1.3.
3. **The stayer's own presses wait for the same ~1 s, in BOTH split and apart cases.** Quick spells (coop_quick_cast_ask,
   hle.c:19480-19497), player 1's zone look during player 2's change (19889-19896), player 2's Start (25895, 26113-26119)
   and B [read]. The waits key on coop_room_change_other, which does not look at rooms. So they apply apart -> apart too,
   which is the user's literal example.

Items 2 and 3 are about a second long. A picture stop of 66-116 ms is not, so "a brief second" may be 2 or 3 as much as 1
[inferred]. The design measures all three before it changes any of them.

**The plan, in phases:**

- **Phase 1 (this build): logging only. It changes nothing for either player.**
  - The `[change-time]` stamps, installed on notify-only hooks of their own, so that solo can give a baseline (P1.1).
  - `[gx-time]` (DOL_GX_TIME=1, which already exists in aurora_backend.cpp:449-452).
  - The `[change-hold]` roster, which now also logs node+28 & 8, +24, +32 and the position step (P1.2).
  - A probe on 0x801F6828 that finds who sets the hold bit (P1.3).
  - A **dry run of F1's corrected gate**. It logs "would unpack ahead / no unpack ahead because ..." at every ticket and
    writes nothing (P1.4). This checks the gate in real play, backtracks included, before F1 exists.
  - Quiet runs, with only the switches the stamps need, so the host's own logging is not what gets measured.
- **Phase 1b (built only if the Phase 1 rows show the creatures held in a split): the fight through the other's door.**
  - F5: the shared room's creatures are not held at a split ticket.
  - F2': engaged creatures switch to the stayer (a new coop_player_leaving, used only by coop_enemy_target).
  - F6: a grabbed stayer is not let go by the other's door (the follow-mode rule at hle.c:22021-22035).
- **Phase 2 (built only if Phase 1's numbers say it pays): the picture stop.**
  - F1, the game's own dormant next-room unpack, with every refuter fix (section 4).
  - F1 is built together with the warm draw of the room unpacked ahead (§4.6). F1 alone can win at most the 1 unpack
    retrace.
  - Or the asynchronous lights A2, or nothing, chosen from the numbers (§6).
- **Sibling item F4: the stayer's waits during the other's ticket** (item 3). Each wait has a reason that still holds
  with separate rooms (one ticket record, a scene over a load, a Tome copy taken with the change in it). Lifting them is
  its own design, with the reads listed in §5. Phase 1 measures them.

**What to tell the user plainly:** this item does not yet change what they see. Phase 1 finds which of the three is "the
brief second". Doors taken while already apart (apart -> apart) keep the picture stop in every phase planned here: F1
needs a free slot, and apart there is none (§4.7). Items 2 and 3 can be fixed apart -> apart: item 2 does not happen
there [read: 102 never reaches the stayer's room when apart], and item 3 is F4.

## 1. The facts this design builds on

### 1.1 The change, and whose data each step reads

| Step | Address | What it reads/writes | Player 1's door | Player 2's door |
|---|---|---|---|---|
| Ticket written | 0x80155770 (fn66), host notify_room_ticket hle.c:11297 | ticket 0x8058AFFC (+0 count, +4 fade delay, +8 room, +20 kind ...); busy set 0x801556CC(4,1) from the who id r13-21496 [read] | player 1's id, object, room | in their use, with their who words in, so their id; the host sets g_coop_room_p2 (11308) [read] |
| Event 102 | 0x801F5620 from 0x80155720 | nodes whose object +592 equals the room passed or -1, except the sender (+16 id) [read 0x801F5654-0x801F5674] | player 1's room's nodes | player 2's room's nodes |
| Tick | 0x80155830 (callers 0x800083EC, 0x80050A90) | +0 == 0: nothing (0x80155848 -> 0x80155918); else +0 -= 1, +4 handling, at 0 the runner or (kind 17) the load [read] | no player data, outside any turn | the same |
| Runner | 0x80152614 (host notify_ticket_run 13858) | r13-26524, camera, arrival script | player 1's | the R4 bracket puts player 2's words, camera, room word, prompt slots in [read 13858-13869] |
| Load | 0x80152298 (host notify_room_change, installed outside the two-slot block, hle.c:27577-27579) | 0x80154C94, then the block in OSReceiveMessage at 0x80152398, then the arrival 0x80151FDC and the tail [read] | room word = player 1's | room word = player 2's old room (bracket) |
| Slot load | 0x80154C94 | resident test on the other record's room id only (0x80154D84); a fresh path renames the record; +20 := reply queue (0x80154E30); replies at once if loaded == 1 (0x80154E48-0x80154E7C); +8 := -1 (0x80154E94); wake 0x80154EC4(0) [read] | the asker's object | the asker's object (bracket) |
| Arrival | 0x80151FDC | the current record, strips, event 61 to the room left, busy := 0 [read] | old room = the room left | player 2's old room |

### 1.2 The engine's own background unpack (dormant)

- **Slot thread 0x801546A4**, priority 10 (0x80154050/0x8015405C). Reader 0x801531CC, thread 0x805B7540, created at 29
  (0x80154034). Route-job thread at 31 (0x80154084) [read].
- Cases [read 0x8015475C-0x80154A74]:
  - 11/10: the current record not loaded. Case 10 raises the reader to 11 (0x80154AD8-0x80154AE4) and unpacks into the
    current slot.
  - **7/6: table +8 (halfword 0x8058AFC8) != -1, the other slot valid, the other record not loaded.** Case 6 sets the
    reader to 21 with 0x8020472C (`lis r3,-32677; addi r3,r3,30016` = 0x805B7540, r4 21; 0x80154A34-0x80154A40), then
    0x80151DC4(+8 room, other slot). Case 7 sets the reader to 11 the same way (0x80154A5C-0x80154A68), then
    0x80151D24(room): disc to ARAM [read]. 0x8020472C is OSSetThreadPriority [inferred from the arguments and the case 10
    use].
  - 5/4: the other record has a room and is not loaded. 3/2: ARAM prefetch.
- **0x80151DC4(room, slot)** does nothing while +0x8144 (pending) is set (0x80151E08-0x80151E10). Otherwise it writes
  room, loaded 0, +0x8143 0, +0x8144 1, and posts to the reader [read].
- **The reader's completion** [read 0x801534AC-0x8015353C]:
  1. 0x80154118 unpacks.
  2. With interrupts off (0x8020102C / 0x80201054), it writes loaded := 1, +0x8143 := 0, +0x8144 := 0 (0x801534C0-0x801534E8).
  3. **0x80151EE0(r17)**, the slot's set-up (r3 = r17 at 0x801534F0), which ends with +0x8143 := 1.
  4. Only if the entry's +20 != 0: OSSendMessage(+20), then +20 := 0 (0x80153504-0x80153530).
  5. 0x80151CD4(4), a stub (0x80153534-0x80153538).
  6. A send to the slot thread (0x8015389C).

  The entry offset is in r18 (`addi r7, r18, 12`, `addi r3, r18, 20`). The slot index is in r17, which survives the
  set-up call (callee-saved) [read].
- **The set-up window:** between step 2 and the end of step 3, the record reads loaded 1, +0x8143 0, pending 0. The reader
  runs below main, so a retrace can put main back in the middle of it [inferred].
- **0x801DDE9C** (called by 0x801DEF34 inside the set-up): copies a 12-byte header into record+4, halts through 0x8014EB18
  on a version other than 12, and relocates one pointer. Its callee 0x801DD8A4 only reads r13-26936 (an alignment, written
  only by 0x801DD8D0). So 0x801DDE9C writes the record and its own stack only [read]. The rest of 0x801DEF34 and 0x801568E4
  are still unread (risk R3).
- table +8 has two writers in the game, both storing -1: 0x80152A34 (init) and 0x80154E94 [read refute.md, rechecked by
  the impl refuter].

## 2. The cases, both players, both directions

| Case | Example | Picture stop | Creatures held (item 2) | Stayer's waits (item 3) | Phase that addresses it |
|---|---|---|---|---|---|
| S1 split, player 1 leaves | both in 183, player 1 to 180 | fresh load, 4-7 retraces | yes: 102 reaches 183 | yes (player 2's quick spell, B, Start) | 1 measures; 1b; 2 (F1 + warm draw); F4 |
| S2 split, player 2 leaves | both in 183, player 2 to 180 | the same, in the R4 bracket | yes | yes (player 1's quick spell, zone look) | the same |
| A1 apart -> apart, player 1 | player 1 180 -> 182, player 2 in 183 | 5-7 retraces | no: 102 is sent to 180 only [read 0x801F5668] | yes | 1 measures; F4. Picture: not in any phase here (§4.7) |
| A2 apart -> apart, player 2 (the user's example read one way) | player 2 183 -> x, player 1 in 180 fighting | the same | no | yes (player 1's) | the same |
| R rejoin | into the other's room | 2 retraces (warm, resident) | no (102 goes to the room left) | yes | F4 |
| T together, no door | - | - | - | - | nothing runs |
| Follow mode (ED_COOP_STAY off) | brought along | as today | as today | as today | nothing runs (coop_stay()) |
| Solo (Mods > Co-op off) | - | 3-5 under the black fade | - | - | nothing runs; Phase 1's stamps only under ED_CHANGE_TIME |

## 3. Phase 1: measure (logging only; this build)

Every new hook in this phase is notify-only: it reads and logs, and writes no guest memory. Every one returns at once
under `g_tome_copy_turn` (P6), so a Tome copy's turn is never measured as play.

### P1.1 `[change-time]` (ED_CHANGE_TIME=1)

Host monotonic ns stamps (clock_gettime CLOCK_MONOTONIC, as tome_copy_ns, hle.c:13419-13423). They are installed **outside
the two-slot block**, and only when ED_CHANGE_TIME is set. So solo with the switch unset has the same hook table as today,
and solo with it set gains read-only notifies [read: install block hle.c:27577-27611; the entries below are free: grep for
each address in runtime/host finds no hook].

| Stamp | Hook | Note |
|---|---|---|
| t0 | new notify on 0x80155830's entry, when ticket +0 == 1 (this tick fires) | Phase 2's intercept takes this entry and stamps t0 itself |
| t1 | new notify on 0x80154C94's entry (the slot load, both modes) | free entry |
| t2 | new notify on 0x80151FDC's entry with lr 0x801523A8 (the block is over) | free entry |
| t3 | 0x80154B54: inside notify_room_slots_tail when two slots; a stamp-only notify when one slot (installed only if its notify is NULL) | the tail |
| t4a, t4b | room_lights_update, around dol_aurora_set_room_lights (hle.c:6184); runs in solo too [read 6099] | the call_sync |
| t5 | notify_VIFlush (hle.c:3685) of that frame | the present |
| t6 | the next hle_PADRead | the next pad row |

One line: `[change-time] frame F room A->B fresh|resident|ahead (player N's door, split|apart|rejoin|solo): to the load
x ms (t0-t1), block y (t1-t2), arrival z (t2-t3), to the lights a (t3-t4a), lights w (t4a-t4b), to the present v
(t4b-t5), present to the next pad u (t5-t6); total T ms`. The kind (split/apart/rejoin) is read from both players' nodes'
rooms at t0: r6_player1_node and g_coop_p2 through coop_object_room. It never reads r13-26524 alone.

### P1.2 `[change-hold]` (ED_COOP_ENEMY_LOG)

- From notify_room_ticket until 60 frames after the tail, every frame for the first 10 and then every 3. One row per node
  of the asker's room and of the stayer's room (players included, marked P1/P2): id, room, type (+8), state, target,
  health, **+28 & 8, +24, the +32 byte, and the position step since the last row**. This settles whether the stayer's
  room's creatures stand still through the countdown (goal refuter point 7).
- A notify on 0x801F5620's entry with r3 == 102 and lr 0x80155724: `[change-hold] event 102 from player N's busy set to
  room R (frame F)`.
- The stayer's presses through the countdown, from the lines that already exist (`player N's quick spell waits`,
  `player 2's Start in player 1's room change`), plus their anim column in both.tsv.

### P1.3 The hold-bit probe (ED_COOP_ENEMY_LOG)

- 0x801F6828(node, on) sets or clears +28 bit 8 on a node and its children, except kind 21 [read 0x801F6828-0x801F68BC].
  It has 89 call sites. A static search for the one that answers event 102 found none within 40 lines of a 102 compare.
  The handler is likely reached through a table [inferred], so the address is found at run time.
- Notify on 0x801F6828's entry, skipping its own recursion (lr 0x801F6880). From the ticket until 60 frames after the tail:
  `[change-hold] hold bit := r4 on node <id> (room R, type T) from lr L (frame F)`.

### P1.4 The F1 gate as a dry run (ED_ROOM_LOG)

At the first tick of each ticket, evaluate §4.1's corrected gate and log only: `[change] would unpack ahead: room R into
slot Y (player N's door; player M stays in A)`, or `[change] no unpack ahead for room R: <reason>`. Nothing is written.
This shows in real play, backtracks included, whether F1 would arm (goal refuter point 2: the host's slot table is stale
after a tail).

### P1.5 The runs (Phase 1, one build)

The build is made fresh, and then a state is made fresh on it from Jump to Game (save-states-fresh-only), the
rooms-ch1-183 recipe in the first version of this design. **Quiet runs** set only `ED_BOTH ED_CHANGE_TIME=1 DOL_GX_TIME=1
ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1` (ED_COOP_ENEMY_LOG for M3/M4 only: its rows, up to 41 lines a frame,
and its hold-bit lines fall inside t0-t6, so M3/M4's `[change-time]` ms are not the baseline). **Judge by the ms column and by
`[gx-time]`'s present gaps, not by retraces.** The timebase drops time over 100 ms (interrupt.c:70, 446-447, read by the
measure readers).

- **M1 S1:** player 1 leaves 183 for 180, player 2 stays and sweeps their camera.
- **M2 S2:** player 2 leaves, player 1 walks through the ticket.
- **M3 the fight, split:** player 2 among the zombies, readied and swinging, while player 1 leaves. Player 2 presses a
  quick spell, B and Start in the middle of the countdown.
- **M4 the mirror:** player 1 pressing A among the zombies while player 2 leaves. Player 1 presses a quick spell and B
  mid-countdown.
  - A scripted player 1 cannot ready: ED_INPUT_SCRIPT has buttons, the stick and analog A only [read hle.c:177-183].
    **Ask the user for one play** of player 1 readied and fighting while player 2 leaves, in both cases: split, and apart
    (player 2 already elsewhere).
- **M5 apart -> apart with a fight (K5b):** player 1 among the zombies in their room while player 2, apart, changes room.
  Expected: no `[change-hold] event 102 ... to room <player 1's>`, and player 1's quick spell waits (item 3).
- **M6 a backtrack in one process, no state load between:** together 180 -> 183 (a follow or a rejoin), then one player
  back to 180. Expected: `[change] would unpack ahead: room 180` (the corrected gate) and today's "nobody stayed in it:
  loaded fresh".
- **M7 M1 with every log on** (run.sh as usual), against M1 quiet. This is U6: how much of the stop is the host's own
  logging.
- **M8 solo baseline:** Mods > Co-op off, Jump to Game through 183 -> 180, with ED_CHANGE_TIME set. Expected: one-slot
  `[change-time]` lines and no `[change]` gate lines.

Read in each, for both halves: `ed timeline`, `ed check`, EVENTS.log, both.tsv (the stayer's position and anim columns
through the countdown and the change), `[change-time]`, `[change-hold]`, `[gx-time]` for the change frame and the two
after, and `[stay]`/`[rooms]`.

## 4. Phase 2: F1, the game's own unpack ahead, corrected (built only on Phase 1's numbers)

F1 sets table +8 at a split ticket, so that the slot thread unpacks the new room into the free slot during the countdown,
and the firing becomes the resident swap. Everything below is the corrected version.

### 4.1 The gate (`change_ahead_split`), both players from their own nodes

- `g_settings.mod_coop && coop_stay() && room_slots() == 2 && g_coop_p2 != 0u && sda_ready(cpu)`, plus a test switch
  `ED_CHANGE_AHEAD` (unset or 1 = on, 0 = off, for the A/B).
- `coop_room_change_on(cpu)`: busy has 4 and nothing but 4|2|1, the ticket counting, no cutscene, no menu (hle.c:11281-11287)
  [read]. This replaces "busy & 4" (goal point 10).
- `!coop_p1_scene_on() && !coop_p2_scene_on()`: no busy-32 scene of either player (R4b), whose scripts may change room
  directly (goal point 10).
- `!g_tome_copy_open && !g_tw.sharing && !g_tome_copy_turn && !hle_coop_tome_shares()`. The arm does not rely on the press
  waits to keep a Tome shut (goal point 4).
- The caller is the world frame: `cpu->lr == 0x800083F0u || cpu->lr == 0x80050A94u`.
- Ticket +20 kind != 17 (the runner path only), and ticket +0 >= 3.
- Table +8 == -1.
- **A split:** `r1 = coop_object_room(r6_player1_node(cpu))`, `r2 = coop_object_room(g_coop_p2)`, both != -1, r1 == r2, and
  the ticket's room C != r1.
- **Residency read from the guest records, not g_room_slot_room** (goal point 2): for both slots, the record's room
  (+0x8140 halfword) == C and loaded (+0x8142) != 0 means resident: no arm. This is what 0x80154D84 tests [read].
  room_slot_of reads the host array, which keeps a dead slot's old room after the tail [read: writers 13608, 13634, 14304;
  nothing at the tail's kill 0x801523A8-0x80152404].
- **The free slot:** `other = table +4` in 0..1 and != table +0. Its record has loaded 0 and +0x8144 0 (dead or never
  used), and its room is in neither player's room.

**Why it opens while the players are together** (goal point 9): with ED_COOP_STAY a door taken together always splits
them (the asker leaves, the other stays). So the ticket's first tick is the moment of parting, and nothing opens at a
together moment that stays together. With no door, nothing runs (no ticket). Check K7b: a together fight with no door is
unchanged.

### 4.2 The arm (no host copy of the tick; impl point 4)

A new intercept `change_ahead_tick` on 0x80155830's entry. It checks the first word (0x9421FFF0) at install, and it is
installed next to notify_room_slots_tail under coop_stay().

- Every call: under `g_tome_copy_turn`, passthrough and return (impl point 6).
- **The arm** (once per ticket, when the gate passes): `mem_write16(0x8058AFC8, C)`. Then the tick is **skipped this
  frame**: the countdown is one frame longer, the same thing the guard does. Then tail-call the game's wake 0x80154EC4
  with r3 = 0 (`cpu->gpr[3] = 0; cpu->pc = 0x80154EC4; g_hle_tail_call = true; g_hle_passthrough = false`). Its blr
  returns to the tick's caller (the ARQ pattern, hle.c:1050-1057) [read]. No game logic is copied.
- Log `[change] room C unpacked ahead into slot Y for player N's door (player M stays in room A; frame F)`, and at each
  later tick, once, `[change] room C in slot Y complete at frame F (n frames after the arm)` (U3).
- Every other call is passthrough, apart from the guard below.

### 4.3 The firing guard: only the set-up window, never for pending (goal points 5, 6; impl point 2)

The guard holds only when all of these are true:
- this tick would fire (+0 == 1, whatever +4 is);
- table +8 == ticket +8 == C (only the host writes a non -1 value there, so this names an armed ticket and nothing else);
- the other record's room == C;
- the record is **in its set-up window**: loaded 1, +0x8143 0, +0x8144 0.

- **Pending (loaded 0, +0x8144 1) is not held. The tick fires.** The resident path swaps, finds loaded 0 and stores +20
  (0x80154E30). The slot thread's case 10 is then refused by the pending byte (0x80151E08). The reader finishes C, sets
  it up and posts to +20 (0x80153504-0x8015351C). Main goes on with C in, which is today's fresh-load wait, only shorter
  [read the order]. Nothing is held for pending, so no 300-frame cap and no long black for the mover.
- **The set-up window must be held.** The resident path's reply-at-once tests the loaded byte alone (0x80154E48), so the
  arrival would read a record whose +0x813C header is not yet set up [read].
  - On the first held frame, skip the tick and tail-call the game's own priority call 0x8020472C(0x805B7540, 11), which
    case 7 makes (0x80154A5C-0x80154A68). It cannot be an hle_guest_call: a raised thread switches inside the call, and
    hle_guest_call follows one CPUState (main.c:57-118) [read].
  - Above main, the reader finishes the set-up when it is next made ready. The next tick finds the record complete and
    fires. The next slot-thread case sets the reader's priority again (case 6: 21, case 10: 11) [read].
  - Log `[change] the firing waits for room C's set-up in slot Y (n frames; frame F)`. Past 10 frames, log
    `[change] WARNING: ...` every 60 frames.
- **No forced firing out of the set-up window.** Firing into an unfinished set-up is the fault the guard prevents, and
  dead-marking a record the reader is still setting up lets the reader's late +0x8143 := 1 and +20 post race a second
  unpack [inferred from the completion order]. The hold has no exit only if the reader never runs again: a halt in
  0x801DEF34 (0x8014EB18), which today hangs the game in the same block 0x80152398 on the same reader. So there is no new
  soft-lock (impl point 2's case "loaded 1, +0x8143 0" answered).
- When it fires after holding: `[change] firing: room C ready in slot Y after n frames held (frame F)`.

### 4.4 The clean-up of a ticket that never fires (impl point 7)

Every call, under the same co-op gate: **if ticket +0 <= 0 and table +8 != -1** (the normal firing clears +8 itself at
0x80154E94, so this is only a ticket that ended some other way):
- If the other record's room == +8's room, it is complete (loaded 1, +0x8143 1, +0x8144 0), and neither player's node is
  in that room: give it the game's dead mark (room -1, loaded 0, +0x8143 0, +0x8144 0, as the tail does at
  0x801523A8-0x80152404). Then +8 := -1. Log `[change] room R unpacked ahead and not entered: marked dead (frame F)`.
- If it is still pending or in its set-up window: leave +8 and retry at the next call (logged once).
- If the record no longer names +8's room: +8 := -1.

This keeps the rule written after the 2026-11-12 solo freeze: a room nobody is in is loaded fresh (hle.c:14330-14336). It
also stops an orphan from refusing the next split.

### 4.5 G2, the stacked change, corrected (impl points 1, 5, 6; goal point 5)

This is a direct 0x80152298 to room D (script callers 0x80020358, 0x8004FDAC, 0x800AD7C8, 0x800ADC24, 0x800AE768,
0x80110E88, 0x8015758C [read grep]) while C is unpacking ahead in slot Y.

- **(a) Detection**, in notify_room_change, under !g_tome_copy_turn. All of these must hold:
  - table +8 != -1 and == the other record's room C;
  - the record is in flight: pending 1, **or loaded 1 and +0x8143 0** (the set-up window);
  - wanted D != C, **and the current record's room != D**. A change into the room in play takes "current already holds
    it" (0x80154D98-0x80154DB8) and renames nothing.

  Then `g_ahead_stack = {on, slot Y, C, D}` and log the WARNING line.
- **(b)** A notify on 0x80152C5C's entry with lr 0x80154E84 (inside 0x80154C94, after +20 is stored). When on, save the
  current entry's +20 and write 0.
- **(c)** A notify on 0x80151CD4's entry with lr 0x8015353C (the reader, after the +20 test). It acts only when on, the
  slot `gpr[17] == Y` (the slot index the set-up was given, §1.2), and **the record's room is still D**:
  - **record pending 0** (C was pending at the direct change, so case 10's 0x80151DC4(D, Y) was refused): write loaded 0,
    +0x8143 0, and +20 := the saved queue. The reader's send to the slot thread then gives case 10 its D unpack, and its
    completion posts to +20.
  - **record pending 1** (the set-up window case: case 10 already queued D, because the pending byte was 0): restore +20
    only. D's completion writes loaded 1, +0x8143 0 and runs the set-up again. Clearing pending here would let a second D
    unpack overwrite the slot in play [inferred from the order].
  - Then clear g_ahead_stack.
- **Saves:** add `|| g_ahead_stack.on` to hle_save_held (hle.c:28008-28017) and reset g_ahead_stack in
  hle_coop_state_load (28020). quickstate_service runs on any thread at the top of each dispatch iteration (main.c:367-371),
  so a state can be taken inside the block [read by the impl refuter]. This is the pattern design-r4d.md uses for g_r4d.
- **Not relied on:** the press waits (item 3). G2 is armed by every direct change, whatever pressed it.

### 4.6 The warm draw (built with F1)

A room unpacked ahead was never drawn. A rejoin's 2 retraces are warm: the stayer's half drew that room every frame. So F1
alone is expected to give about 3-6 retraces, not 2-3 [inferred, goal point 1]. The enabler: during the countdown, draw
section 3 of slot Y with its actors and effects marked out of both views, so that the render thread decodes its textures
and builds its pipelines before the firing.
- To read before building: gfx/interp.cpp build_view's marked_out_of. Does it leave a marked draw out after decode or
  before? If before, the warm draw needs a mark that is decoded and not submitted.
- room_section_draw today draws only a room a player is in (hle.c:13711-13732) [read]. The warm draw adds one case: slot Y,
  armed, and only after the record is complete.

### 4.7 Why apart -> apart has no picture fix in this design

No slot is free: one holds the stayer's room and the other is the mover's own, which is still played until the firing.
The mover's nodes step against it through 0x80123F70 by +592, it is drawn and its light grid is read. A third resident
slot or unpacking into the mover's slot in the last black frames is a separate design (the first version's §5, kept in
§7). Phase 1's M5 measures it.

### 4.8 Log wording (impl point 10, goal point 12)

room_slots_return (hle.c:14337-14341): when the ticket's room or table +8 named the room, print `unpacked ahead` instead
of `kept, a player stayed in it`.

### 4.9 The Phase 2 checks

- **A/B in one build:** run K1 (S1) and K2 (S2) quiet, with ED_CHANGE_AHEAD=0 and then with it unset. Compare ms and the
  `[gx-time]` present gaps of the stayer's frames.
- **K6, the set-up hold:** a test switch arms at +0 == 3, so the firing can land in the set-up window. Expect the hold,
  the priority line, then `firing ... after n frames held` with n <= 2.
- **K6b, G2 with the ticket still counting:** a test switch makes a direct 0x80152298(182) 4 ticks after the arm, from a
  notify-free host path that does not touch the ticket. Then the original ticket fires later from 182. Read both firings,
  `ed check` and the hang reports. K6c is the same in the set-up window.
- **K6d, a quick save** taken in the G2 window (ED_SAVE_STATE at the frame of the WARNING line). Expect the save refused
  and taken after, then loaded cleanly.
- **M6 again (the backtrack)**, now with the arm.
- **P9's watch items:** no countdown row more than 1 retrace apart in both.tsv; `[enemy]` route lines during the countdown
  (the route-job thread at 31 waits behind the reader at 21); the arm frame's ms when case 7 (disc to ARAM at priority
  11) is taken.
- **K7/K7b:** follow mode and a together fight with no door: no `[change]` line. **K8:** solo: no `[change]` line.

## 5. Phase 1b and the sibling item (the second each loses)

### F5: the shared room's creatures are not held at a split ticket (only if P1.2 shows +28 & 8 set and no movement)

- **Where:** coop_node_held_ask (0x801F68D4, hle.c:19456). Today it answers "not held" only for the other PLAYER.
- **The rule:** also for a node whose object's room is the stayer's room, when coop_stay(), two slots,
  coop_room_change_on, and the players are together at the ticket (a split, by both nodes' +592). The asker's own node is
  left to the game.
- **Apart:** the asker's room has nobody staying, so it is held as in solo. Nothing changes.
- **Before building, read:** what a creature's hit does to a player in their door motion (state 121 and the door script).
  Freed creatures can reach the asker during the countdown, which the game's hold prevents in solo. F2' points engaged
  creatures at the stayer, but a swing already in motion may land.

### F2': engaged creatures go after the stayer (with F5)

- A new `coop_player_leaving(cpu, k)`: `g_settings.mod_coop && g_coop_p2 && coop_stay() && room_slots() == 2 &&
  coop_room_change_on(cpu) && (k == 1) == g_coop_room_p2 && both players' object rooms are equal`.
- It is used **only** in coop_enemy_target's held[] (hle.c:24630): `held[k] = coop_player_held(cpu, k) ||
  coop_player_leaving(cpu, k)`.
- coop_player_held stays as it is. Event 59 and the sightings (21076-21090, 22166, 22198) stay with the game's own 102,
  which avoids repeating failed-attempts.md:562 ("player 1 held stopped the enemies from coming at all") (impl point 3).
- coop_enemy_target returns in a turn (24579) [read], so coop_hold_open's busy := 0 inside turns does not flip it.
- The "both in one room" term keeps it to the split. In a rejoin the asker's room's creatures are not sent to a stayer in
  another room.

### F6: the other's door does not free a grabbed stayer (with ED_COOP_STAY)

hle.c:22021-22035 passes the room change's 102 through on purpose, so that the grabbed one is "let go as the game lets
go, and brought along (coop_follow)". With ED_COOP_STAY nobody is brought along. The rule: with coop_stay() and two
slots, treat busy 4 like any other busy set (answer 1, handled), when the held one is the other player. Follow mode is
kept as it is. Both directions: player 1 grabbed while player 2 leaves, and player 2 grabbed while player 1 leaves.

### F4 (sibling item): the stayer's waits during the other's ticket

Each wait is a follow-mode rule, but its reason still has to be answered with separate rooms:
- **Quick spells** (19480-19497): "may ask for a scene or another room". There is one ticket record: a second fn66 is
  passed over while one counts (16505) [read]. To read: which quick spells ask for a ticket or a scene, and whether a cast
  in the stayer's room during the other's countdown touches the slot table or the room word.
- **Player 1's zone look during player 2's change** (19889-19896): "a way out or a scene would ask for a second room, or
  play, over the one on its way". The same ticket question, and a scene over a load.
- **Player 2's Start** (25895, 26113-26119): a copy opened in the fade was taken with the room change in it and showed
  black once the new room was in (run-20261106-081752). With the stay, the stayer's room does not change, but the copy is
  of all of memory, the slot table included. To read: what the Tome copy keeps of the slot table and the room word across
  a change it did not take part in.
- **B:** the same reason as quick spells.
- **U5:** an idle stayer who could not start walking (run-20261112-033642: stick at 4750, a move at 4772). Phase 1's M2
  reads this with player 1 pushing from rest.

Phase 1 logs each refusal (M3/M4/M5). F4's design follows from those rows.

## 6. How Phase 1's numbers choose Phase 2

All numbers come from the quiet runs, in ms:

| If | Then |
|---|---|
| block y (t1-t2) >= 16 ms in S1/S2 and the first-draw part (t5-t6 with `[gx-time]` full/present on the change frame) is large | F1 with the warm draw (§4) |
| block y >= 16 ms and the first draw small | F1 alone |
| lights w (t4a-t4b) >= 16 ms | A2: an ordered stream op for the room lights, used only with co-op apart (a GXRuntime change, patch regenerated) |
| M7 minus M1 >= 16 ms | the host's logging at the change. Not a game fix: the user's normal play runs with fewer logs |
| the stop is mostly the frame's own logic (t2-t3, t3-t4a) | a reader set of its own on the arrival's work, before any design |
| P1.2 shows the stayer's room held for the countdown | Phase 1b (F5, F2', F6) |
| the refusals in M3-M5 line up with the user's "brief second" | F4 next |

## 7. Alternatives kept from the first version, and why not

- **A third resident slot** (apart -> apart). The 48 MB layout holds slot 1 at 0x83000000, area 1 at 0x83300000 and the
  R6b graph at 0x83400000. The table and every per-slot reader assume 2.
- **A staging copy:** slot data holds absolute pointers into its slot (18,073 words) [read code reader §5].
- **Unpacking into the mover's own slot in the last black frames:** that slot is still played until the firing (§4.7).
- **Answering the stayer's room's 102 as "sent"** in place of F5. This is broader than F5: it would also skip the 102
  handlers' let-go (the zombie's 0x80124788, +24, +32; 0x8003C5B0-0x8003C5DC). F5 asks the hold alone.
- **Re-presenting or blending the last picture:** a still picture all the same (`ed keep`).
- **Running the stayer's world during the block:** hle_guest_call cannot follow a guest thread switch (main.c:57-118).
- **Waking the slot thread or raising the reader with hle_guest_call:** the same reason. The game's own calls are
  tail-called instead.
- **Holding the firing while the unpack is pending:** dropped (goal point 6). The game's own +20 wait covers pending.
- **F2 through coop_player_held:** dropped (impl point 3). It repeats a failed attempt.

## 8. The gate: solo, together, follow mode

- **Solo:** Phase 1's stamps are installed only under ED_CHANGE_TIME, and notify-only. Phase 2's hooks and Phase 1b's
  rules sit behind mod_coop, coop_stay() and room_slots() == 2. With co-op off, rooms_follow_coop unsets both switches
  (frontend.cpp:483-499) [read].
- **Together, no door:** no ticket, so nothing.
- **Follow mode:** coop_stay() is false, so nothing (F6 keeps the follow rule).
- **Apart:** F1 never arms (no free slot), F5 does not apply (the asker's room has nobody staying), and F2' requires the
  players in one room.
- **Neither player's data from r13-26524 alone:** rooms come from r6_player1_node (player 1 inside any swap,
  hle.c:24188-24195) and g_coop_p2.

## 9. Interactions (Phase 2 unless said)

- **R1/R2:** the same slot as today's fresh load, used earlier. The tail's dead mark and put-back are unchanged.
  room_slots_return's swap (14347-14356) needs player 2 apart in the other slot, which a split is not [read].
- **R3:** room_section_draw draws no room nobody is in (13711-13732), until the warm draw's one case. room_lights_update
  takes only the current slot's grid (6120-6127).
- **R4, R4b, R4c:** notify_room_ticket, notify_ticket_run, coop_stay_change_end, g_fade_mover and coop_node_held_ask
  (Phase 2) are unchanged. g_fade_mover survives held frames: it is forgotten only when !coop_room_change_on (3342) [read].
  coop_stay_change_end's swap-back uses room_slot_of(g_stay_bracket_room), the room in the current slot before the
  change, which the array has right [read 13870-13888].
- **R4d** (design-r4d.md): F1 refuses during either player's scene (§4.1), so R4d's case never arms.
- **R5 sound:** unchanged.
- **R6:** r6_room_loaded (24215-24222) and room_slot_of read the host array, and the gate reads the records. Leftover nodes
  of a room no longer loaded exist (the r6_enemy_apart guard). Once C is complete in Y, 0x80123F70 would hand such a node
  C's zones (0x80123FBC-0x80123FDC) [read]. Phase 1's `[change-hold]` lists any node stamped C during M1/M2 before the
  arrival. If there are any, a reader on them comes before Phase 2.
- **R6b:** the second graph's key is emptied when slot Y holds a new room, as at today's fresh load (24358-24376).
- **R6c:** creatures run by their room. Leftover C nodes are the question above.
- **The Tome copy:** every new hook returns under g_tome_copy_turn, and F1 refuses while any Tome is open or shared.
- **0x80155450's 8 callers ("is it resident"):** with 2 slots they already answer 1 for every exit of a loaded slot's
  room (refute.md) [inferred: unchanged].

## 10. Exact changes

### Phase 1 (this build; host only, logging only)

All in `runtime/host/hle.c`, under `getenv` switches read once:
1. `g_change_time` (t0..t6, the kind, both rooms) and its line. New notify-only hooks on 0x80155830, 0x80154C94 and
   0x80151FDC (lr 0x801523A8). The one-slot notify on 0x80154B54. Stamps in notify_room_slots_tail, room_lights_update,
   notify_VIFlush and hle_PADRead. All under ED_CHANGE_TIME, installed outside the two-slot block, each returning under
   g_tome_copy_turn.
2. `[change-hold]` rows (r6_enemy_roster's line with the gate "a ticket or 60 frames after", plus +28 & 8, +24, +32 and the
   step), the 0x801F5620 notify (102, lr 0x80155724) and the 0x801F6828 notify. All under ED_COOP_ENEMY_LOG.
3. `change_ahead_split` as a pure reader, logging the dry run at the first tick of each ticket (ED_ROOM_LOG).
4. `python -I tools/ed/ed.py test` after the build. Add `[change-time]` and `[change]` to `ed timeline` if it lists room
   events.

### Phase 1b (on the rows)

coop_node_held_ask's split rule (F5), coop_player_leaving and its one use (F2'), and the 102 filter's stay rule (F6),
after the door-motion hit read.

### Phase 2 (on the numbers)

Constants (ROOM_SLOT_QUEUED 0x8058AFC8, TICKET_TICK 0x80155830, SLOT_WAKE 0x80154EC4, READER_THREAD 0x805B7540,
SET_PRIORITY 0x8020472C). Also: change_ahead_tick (arm, guard, clean-up), G2's (a)-(c) with the save gate and the load
reset, the log wording, the warm draw, the test switches ED_CHANGE_AHEAD, _LATE and _STACK, and `ed test`.

## 11. Unknowns only a run settles

- **U1:** how the 3+ post-arrival retraces split (P1.1 in M1/M2/M5/M8).
- **U2:** whether the stayer's room's creatures are held for the countdown, and by which setter (P1.2, P1.3 in M3/M4).
- **U3:** the reader's share of idle time (Phase 2: the "complete at frame" line).
- **U4:** how each creature kind behaves after 102 (P1.2).
- **U5:** the idle stayer's late start (M2).
- **U6:** the host's logging cost (M7 against M1).
- **U7:** a short press lost in the stop. Not scripted; M3's presses are 10 frames long.
- **U8:** leftover nodes stamped with the ahead room (P1.2 in M1/M2).

## 12. Refuted or corrected

Impl refuter (refute-change-freeze-impl.md):

| # | Problem | Checked | What was done |
|---|---|---|---|
| P1 | g_ahead_stack lost across a quick save in the block | real: hle_save_held has no room term [read 28008-28017]; hle_coop_state_load does not reset it | §4.5: save held while on, reset at load, (c) acts only while the record's room is still D |
| P2 | the guard can hold for ever; stateless guard can't tell an armed ticket | real for the first version's "keep holding" rule | §4.3: no hold for pending; hold only the set-up window, with the reader raised by the game's own 0x8020472C; requires table +8 == ticket +8; the only endless case is a reader halt that hangs today too. Dead-marking in the set-up window was rejected (the reader's late writes race a second unpack) |
| P3 | F2 through coop_player_held repeats a failed attempt | real [read 21076-21090; failed-attempts.md:562] | F2 dropped; F2' (coop_enemy_target only, split only), Phase 1b |
| P4 | the arm copies the tick | real (correct today, fragile) | §4.2: the tick is skipped on the arm frame, then the wake is tail-called |
| P5 | (a) fires for a change into the room in play; (c) has no slot | real [read 0x80154D98; the completion uses r18/r17] | §4.5: current room != D; (c) reads gpr[17] (the slot passed to 0x80151EE0) |
| P6 | new hooks run inside the Tome copy's turn | real [read 5533-5535] | every new hook returns under g_tome_copy_turn |
| P7 | the never-fired orphan; leftover nodes | real | §4.4 dead mark when complete and empty; U8 / §9 R6 reader before Phase 2 |
| P8 | no solo baseline: stamps installed with two slots only | real [read 27580-27611; frontend.cpp:483-499] | P1.1: own notify-only installs under ED_CHANGE_TIME; touching solo's hook table only when that test switch is set, said so |
| P9 | the unpack in idle time was never exercised | real, unmeasurable without a run | §4.9 watch items |
| P10 | misleading "kept" line | real [read 14337-14341] | §4.8 |
| P11 | apart -> apart unchanged | real | §0, §4.7: said plainly; M5 measures it; items 2 and 3 cover what apart can be fixed |

Goal refuter (refute-change-freeze-goal.md):

| # | Problem | Checked | What was done |
|---|---|---|---|
| 1 | F1 removes at most 1 of 4-7 retraces | real: user's run 1843 unpack, 1844 arrival and draw, 1847 next row, 66 ms [read] | Phase 1 is logging only; F1 is Phase 2, built with the warm draw and only on the numbers (§6) |
| 2 | residency from the stale host array | real [read writers 13608/13634/14304] | §4.1 reads the guest records; M6 backtrack in one process, already in Phase 1's dry run |
| 3 | apart -> apart unchanged; no fight check | real | told plainly (§0); M5 = K5b with a fight; F4 covers the apart waits |
| 4 | the stayer's ~1 s waits are left and relied on | real [read 19480-19497, 19889-19896, 25895, 26113-26119] | F4 sibling item with its reads; Phase 1 logs the refusals (M3-M5); F1 refuses on any Tome state itself, G2 does not rely on the waits |
| 5 | G2 misses the set-up window; guard tied to +4 == 0 | real [read 0x801534C0-0x80153530] | §4.5 (a) includes the set-up window, (c) handles both sub-cases; §4.3 the guard ignores +4 |
| 6 | holding for pending can leave the mover black for seconds | real | §4.3 no hold for pending; the set-up hold raises the reader |
| 7 | the creatures' hold bit not read; grab release is a follow rule | partly read: the host's reading says 102 sets +28 & 8 [read 11274, 14310]; the setter address was not found statically | P1.2/P1.3 log and find it; F5 and F6 in Phase 1b |
| 8 | the checks would not prove it | real | quiet runs, ms and `[gx-time]` (P1.5), ED_CHANGE_AHEAD A/B (§4.9), solo stamps (P1.1), ask the user for player 1's readied fight (M4), K6b keeps the ticket counting |
| 9 | F1/F2 open while together | real | §4.1: the split is the moment of parting under ED_COOP_STAY; F2' needs the split; K7b |
| 10 | F1 can arm during a letterbox-free scene | real [read 3784-3790] | §4.1: coop_room_change_on's busy test and no scene of either player |
| 11 | the set-up never ran on a non-current slot | partly read: 0x801DDE9C writes the record and stack only [read] | the rest of 0x801DEF34 and 0x801568E4 listed as R3, to read before Phase 2 |
| 12 | misleading log | real | §4.8 |

## 13. Facts read for this design

| Fact | Where | Mark |
|---|---|---|
| The user's split: unpack asked 1843, arrival and draw 1844, next pad row 1847 (66 ms) | logs/run-20261111-221141-rooms-play page-0001.log 2630-2683, both.tsv | [read] |
| Event 102 reaches the room passed and -1 only, never the sender | 0x801F5654-0x801F5674 | [read] |
| 102 sets the receivers' +28's 8; held nodes don't update while busy | hle.c:11272-11277, 14310-14311, 19451-19455 (host readings) | [read] host; setter address unread |
| 0x801F64A0 stores +24, 0x801F6488 stores the +32 byte (the zombie's 102 handler's calls) | 0x801F6488-0x801F64A4 | [read] |
| 0x801F6828 sets/clears +28 & 8 on a node and children, not kind 21; 89 call sites | 0x801F6828-0x801F68BC; grep | [read] |
| Case 6/7 priority calls: 0x8020472C(0x805B7540, 21 / 11) | 0x80154A34-0x80154A68 | [read] |
| The tick does nothing at +0 == 0 | 0x80155844-0x8015584C | [read] |
| Completion: slot in r17 into 0x80151EE0, entry in r18, post only if +20 != 0 | 0x801534B4-0x8015353C | [read] |
| 0x801DDE9C writes the record and its stack only | 0x801DDE9C-0x801DDF38, 0x801DD8A4 | [read] |
| g_room_slot_room keeps a dead slot's room | hle.c:13503, 13608, 13634, 14304 | [read] |
| ED_INPUT_SCRIPT: buttons, stick, analog A only | hle.c:177-183 | [read] |
| The stay waits: quick spell, zone look, Start | hle.c:19480-19497, 19889-19896, 25895, 26113-26119 | [read] |
| fn66 passed over while a ticket counts (one ticket record) | hle.c:16504-16508 | [read] |
| coop_enemy_target skips turns | hle.c:24579-24581 | [read] |
| Install block: two-slot hooks inside room_slots() == 2; notify_room_change outside it | hle.c:27577-27611 | [read] |
| The entries 0x80155830, 0x80154C94, 0x80151FDC, 0x80152C5C, 0x80151CD4, 0x801F5620, 0x801F6828 carry no host hook | grep runtime/host | [read] |
