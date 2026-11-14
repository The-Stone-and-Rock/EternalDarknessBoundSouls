# Design: the stayer's room plays on through the other's door countdown (final)

The item: when one player takes a door and the other stays (ED_ROOM_SLOTS=2 + ED_COOP_STAY=1, co-op on), the stayer's room
stands still for the ~62-frame countdown. In a split (both in one room) this is measured: logs/run-20261112-180114-freeze-
m3-p1-leaves-fight, zombies step 0.0 from 3822 to 3881, then take up the stayer. The user sees "the screen pauses a brief
second". Their own report (memory coop-separate-rooms.md, 2026-11-12) is the apart shape: player 1 fighting in one room
while player 2 enters another. The ~66 ms picture stop at the firing is the solo game's own hitch and is not this item.

This is the final design after two refutations (refute-split-hold-impl.md, refute-split-hold-goal.md). Section 16 lists
each point raised, what the code showed and what changed. The main change: the first draft answered "not busy" to every
caller but a keep list, and the refuters showed that this reaches game states that solo never reaches. Those are a second
room ticket, a capture, a body switch, a sanity effect, a route job straddling the change, and a scene or cutscene under
which the ticket fires. This version answers "not busy" only to a short, read **allow list** (the stayer's room's nodes
and the stayer's own events). It adds five guards on what can still begin in the window, and it covers the apart shape
with the same rule.

Inputs: hold-readers.md, mover-readers.md, stayer-readers.md, design-change-freeze.md §0-2 and §5,
change-freeze-code-readers.md, refute-change-freeze-*.md, refute-split-hold-*.md, docs/coop-no-pause.md,
docs/coop-failed-attempts.md, docs/game/systems/sanity.md, docs/game/research/transitions.md, runtime/host/hle.c (working
tree, uncommitted, 29,549 lines), build/ed-db/disasm.txt (`ed fn`), and the M3 log. No code was changed, nothing was built
and the game was not run.
[read] = seen at the address, hle.c line, doc line or log given. [inferred] = reasoned, not seen.

## 0. In short

1. **One word holds the room.** The busy word r13-23344 is read only by the busy set (0x801556E0, 0x801556F4) and the
   busy ask 0x80155738, which has 35 callers [read; impl refuter §1]. A node's update and its motion stop on "busy and the
   node's standing bit (+28 & 8)" [read 0x8000749C-0x800074B4, 0x800271F0-0x80027208]. The stayer's events are swallowed
   on the word alone (the player handler's filter, ask at 0x80058E80) [read].
2. **The fix answers "not busy" to an allow list, during a ticket's countdown, for the stayer's room only** (S1, §3.1):
   - A1: the node update and motion asks for a node whose object room (+592) is the stayer's room. The node is in r28 at
     0x8000749C and in r31 at the four motion sites [read].
   - A2: the pump's ask 0x801F541C for a message to such a node, except event 75 [read: message in r29, receiver id at +8].
   - A3: the player handler's filter when its node (r31) is the stayer's.
   - Everything else keeps the real word, as in solo: the scripts' getter, the self-asking creature kinds, the light and
     record steppers, the sight's sanity hit, kind 19's ticks, the forced-hit pairs, event 75, and every roomless node.
   - The mover's own filter is decided first and always keeps the real word (K1).
   - Kinds whose code reaches a room ticket, a body switch or a capture (K7: 39, 50, 82, 32 read so far) are never
     answered 0 and still get the door's 102.
3. **Nothing is written.** The word stays 4, the ticket and its countdown are the game's, and coop_room_change_on and
   every host rule that reads the word are unchanged.
4. **The door's event 102 is kept from the shared room's nodes** (S2) in a split, so the room's chasers are not cut and
   the room keeps no new standing bits. The exceptions still get it as in solo: kind 19 (a grabbed stayer is let go as
   today), K7 kinds, and roomless nodes.
5. **Creatures after the mover take up the stayer at once** (S3, split only), or nobody when the game holds the stayer.
6. **Five guards on what can begin inside the window:**
   - G1: a second room ticket is refused and logged (an intercept on the ticket writer 0x80155770).
   - G2: the countdown waits while a scene, held words, a find or a cutscene begun in the window is up (an intercept on
     the tick 0x80155830). R4b's case is left as it is today.
   - G3: the window does not open for a stayer already casting.
   - G4: a route job opened in the window runs whole, with interrupts off as R6b does, so it can never straddle the
     firing.
   - G5: a direct room load (0x80152298) inside the window is logged loudly.
7. **Both shapes, one rule.** "The stayer's room" is the shared room in a split, and the stayer's own room when apart. In
   the apart shape the mover's room stands as in solo and the stayer's plays on. A testing switch,
   `ED_SPLIT_HOLD=0|split|1`, compares them in one build. Solo, follow mode and co-op without ED_COOP_STAY take none of it.
8. **What still waits for the second:**
   - the stayer's presses (quick spell, zone look, B, player 2's Start: F4, the sibling item);
   - a sanity hit on anyone;
   - the room's lights' flicker steps and room animators;
   - scripts that ask "busy?";
   - a cast already under way at the ticket;
   - a grab (let go at the ticket, as today).
   The arrival's event 61 to the room left still lands at the firing: it is the next visible stop, its own item (§8).

## 1. What this builds on, checked again

### 1.1 The mechanism

| Fact | Where | Mark |
|---|---|---|
| The ticket writer: sets byte r13-23392 := 1, writes the record 0x8058AFFC (+0 countdown, +4 fade delay, +8 room, +20 transition), sets prompt flag 0x10000 (0x80117260(0x10000, 1)), busy set(4, 1), input lock 0x800443E4(count+3), cast end 0x801C5118(0) | 0x80155770-0x80155814 | [read] |
| The busy set stores the word, then broadcasts 102 from the current id to that node's room (store 0x801556F4, broadcast 0x80155720) | 0x801556CC-0x80155734 | [read] |
| 102 is broadcast only there; kind 19 sends itself 102 at 0x80061770 and 0x80061AAC | disasm grep | [read] |
| The broadcast walks every node, skips the sender, sends to nodes of the room or -1 through 0x801F57C0 (receiver node in r31, lr 0x801F568C); a skipped call leaves the loop intact | 0x801F5648-0x801F5694; 0x801F57D4, 0x801F582C | [read] |
| The tick counts +0 down, counts +4 (fade start 0x801E7C88 at 0), and at +0 == 0 runs the change in the same call (transition 17: 0x80152298 then event 62; else 0x80152614), then the record's callback +36 | 0x80155830-0x80155924 | [read] |
| The tick is called at the top of the world frame (0x800083EC) and of the other mode frame (0x80050A90), with no busy or scene test before it | 0x800083C8-0x800083EC, 0x80050A68-0x80050A90 | [read]; that 0x80050A68's frame is the cinematic's: [inferred] |
| Node update: busy, then 0x801F68D4(r28) | 0x8000749C-0x800074B4 | [read] |
| Motion and physics: busy, then 0x801F68D4(r31) | 0x800271F0, 0x80027298, 0x80027764, 0x800277F4 (+0xC each) | [read] |
| The pump: message in r29 (+0 event, +8 receiver id), busy, then the receiver's held bit; event 75 waits unless the player is free | 0x801F5408-0x801F54D0 | [read] |
| The player handler: node in r31 from 0x80058DF8 to the busy ask at 0x80058E80 | 0x80058DF8-0x80058EB4 | [read] |
| coop_busy_ask today: four "not busy for everyone" cases (player 2's screen close-up, player 1's close-up, player 1's free words or find, player 2's script hold or scene) | hle.c:19704-19721 | [read] |
| coop_p1_close_up includes the room bit for player 1's own change (CLIMB DOWN from their look: busy 2\|4). So today the whole world, the mover's filter included, is already answered 0 through such a countdown | hle.c:19283-19300 | [read]. A precedent of "busy 0 while a ticket counts". It is not examined anywhere for kinds 39 or 50 [inferred] |
| coop_room_change_on: bit 4, nothing but 4\|2\|1, ticket +0 > 0, no cutscene, no menu | hle.c:11333-11341 | [read] |
| R4: a second fn66 while coop_room_change_on is passed over (returns 0) | hle.c:16747-16756 | [read]. Only fn66; 0x80155770 has a notify only (hle.c:28632-28634) |
| R4b: at player 2's arrival, player 1's own scene or close-up keeps its busy bits (stay_busy_clear, g_stay_bracket, lr 0x80152188) | hle.c:25060-25075 | [read] |
| R6b's bracket: a route job of another room's creature (lr 0x800BB918 to 0x800BB9E4) runs with the graph pair swapped and MSR[EE] off, so the main thread never runs a frame inside it | hle.c:24924-24990 | [read] |
| Each player's casting words (r13-22336 the spell, -22320/-22316/-22312), player 2's kept in g_coop_casting outside their swap | hle.c:12339-12370 | [read] |
| A node's kind is +8 (0x801F62D0 = lwz 8) | coop-level3-design.md:1370; kind 39's loop at 0x800E8990 | [read] |
| The sanity tracker is the one node of kind 32 | sanity.md:91 | [read] |

### 1.2 Corrections to what the readers or earlier designs say

- **"Event 102 sets the hold" (hle.c:11327-11329, design-change-freeze §0.2)** is wrong. Bit 8 is a standing mark and the
  word is what holds (hold-readers §0.1; M3 page-0001.log 1688-1701) [read].
- **M3 was not a fight.** Player 2's `b` presses were the prompt button, and the zombies were after player 1, the mover
  (page-0001.log 145-146, 869-871) [read].
- **"Apart -> apart doesn't do it" is unmeasured and probably wrong.** Room 180's zombie 33 showed hold 1 in M3 with no
  102 sent there (hold-readers §0.6, stayer-readers §0.2). The stayer's filter swallows hits and 59 apart too
  (mover-readers §0.8). coop-level3-design.md:2642's "the picture stop alone" had no creature rows. This design therefore
  covers apart (§2) and K3 measures both.
- **"Busy 0 for all but the mover reaches nothing ordinary play does not" (first draft §0.2) is false.** "Busy 0 while a
  ticket counts" is a state the game's own code tells apart:
  - 0x8008D340 tests busy (0x8008D3C8) and the ticket (0x8008D3DC) separately [read];
  - 0x800E0664 skips a post-death path while a ticket counts [read].
  There are seven game-code ticket writers (§3.8) and kind 39's capture behind one busy ask (§16 B1). Hence the allow list.
- **The record sites are mostly lights and animators, not spells:**
  - 0x80145A0C is in 0x80145994, the light node's event handler (design-lights.md:139);
  - 0x801F22A0, 0x801F248C and 0x801F2514 lie in the light-records module 0x801F1B40-0x801F2858 and test the same
    +41 & 1 byte [inferred from the range and field, impl refuter P4];
  - 0x80082090 (ask 0x800820B8) is a per-room animator called from the world frame at 0x80008208;
  - 0x8007E140 steps a global record at 0x80323920 [read there].
  All keep the real word here.
- **The ticket's flag 0x10000 in r13-24048 is the game's "prompts off during a door".** The world frame calls
  0x80117360(0), the prompt clear, every frame while any of r13-24048 is set (0x800084D8-0x800084EC). The prompt
  module's own reader is 0x801176B4, and 0x80117338 zeroes the word. The change clears the bit at 0x8015203C
  (0x80117260(0x10000, 0)) [read]. It holds no creature [read: every reader is prompt code]. For the stayer it means
  that no game prompt shows in the window, which agrees with F4.

## 2. The window

```c
// A countdown with one player staying (ED_COOP_STAY): the door's taker (mover) and the room the other stays in.
static struct { bool on; bool shared; int mover; s32 room; u32 word0; } g_split;   // mover 1 or 2; room: the stayer's
static int split_switch(void);  // ED_SPLIT_HOLD (testing): unset or 1 both shapes, "split" the shared room only, 0 off
```

- **Set** in notify_room_ticket (hle.c:11350; it runs before the record and busy are written [read]), after
  `g_coop_room_p2 = g_coop_entered_p2` (11363):
  - `rm = coop_object_room(mover's node)`, `rs = coop_object_room(stayer's node)`, both from their own nodes. Player 1's
    node comes from r6_player1_node (hle.c:24459-24466), never r13-26524 [read].
  - `on = mod_coop && g_coop_p2 && coop_stay() && !g_tome_copy_turn && rs != -1 && switch allows (rm == rs ? any : 1) &&
    !stayer_casting()` (G3);
  - `shared = rm == rs`, `mover = g_coop_room_p2 ? 2 : 1`, `room = rs`,
    `word0 = busy word & ~4` (the bits already up before the ticket).
  - Every ticket sets it again; G1 means only the first ticket of a window gets here.
- **Cleared** at the change's tail (notify_room_slots_tail, hle.c:14516) and in hle_coop_state_load (hle.c:29086).
- **Predicates:**
  - `split_counting(cpu)`: `g_split.on && !g_tome_copy_turn && sda_ready && (s32)ticket+0 > 0 &&
    coop_object_room(stayer) == g_split.room`.
    Used by K1, K7, G1, G2, G4 and G5, so the protection holds whatever else is in the word.
  - `split_on(cpu)`: `split_counting(cpu) && coop_room_change_on(cpu)`. Used by the allow list, S2, S3 and S4.
- The window ends when the tick takes +0 to 0, before the runner, the load and the arrival (the same call,
  0x80155858-0x801558F4) [read]. Nothing here runs inside the change itself.

## 3. The changes (hle.c only; no guest code changed)

### 3.1 S1: coop_busy_ask (hle.c:19704)

```c
static void coop_busy_ask(CPUState* cpu) {
    g_hle_passthrough = true;
    if (split_counting(cpu)) {
        if (cpu->lr == 0x80058E84u && cpu->gpr[31] == split_mover_node(cpu)) { split_log_ask(cpu, "kept: the mover"); return; }  // K1
        if (split_k7_site(cpu->lr)) { split_log_ask(cpu, "kept: a room-changer"); return; }                                 // K7
    }
    // ... today's four cases, unchanged (they answer for everyone, as today) ...
    if (split_on(cpu) && split_allows(cpu)) {                    // A1-A3, the stayer's room only
        hle_set_u32(cpu, 0u);
        g_hle_passthrough = false;
        split_log_ask(cpu, "not busy: the stayer's room");
    }
    // else: the game's word
}
```

The order matters:
- **K1 and K7 come before today's cases.** Today's cases answer 0 to everyone, and the CLIMB DOWN look of §1.1 is one of
  them, inside a countdown. The mover and the room-changers are therefore protected in those cases too. This also closes
  mover-readers §6's exposure, in every window.
- **The allow list comes after today's cases.** Player 2's screen close-up and the other cases still get today's "not
  busy" for their scripts and records, so nothing that works today changes [read hle.c:19708-19720].

### 3.2 The allow list (split_allows)

| # | lr (after the bl) | Answered 0 when | What it frees | Mark |
|---|---|---|---|---|
| A1 | 0x800074A0 | node r28's +592 == g_split.room, kind not 19 and not K7 | the node's update (its controller, events 3/4) | [read] |
| A1 | 0x800271F4, 0x8002729C, 0x80027768, 0x800277F8 | node r31: the same test | motion advance and the physics step | [read] |
| A2 | 0x801F5420 | message r29: +0 != 75, and its receiver (0x801F5F88(+8)) passes A1's test | delayed messages to the room's nodes (a hit on a creature no longer waits) | [read] |
| A3 | 0x80058E84 | r31 == the stayer's node | the stayer's events: hits, damage, 59 target asks, 202 | [read] |

- Roomless nodes (+592 == -1) are never on the list. They stand as in solo. Examples: the kind-32 sanity tracker, if
  roomless, and global controllers [inferred].
- The kind test reads +8 of the node. coop_object_room returns -1 for a node with no valid object [read impl refuter §1].
- In the apart shape g_split.room is the stayer's room, so the mover's room's nodes (+592 the mover's room) are not on the
  list and stand as in solo.

### 3.3 Every other caller keeps the game's word (as solo; corrected labels)

| Site (lr - 4) | What it is | Why it keeps the word | Mark |
|---|---|---|---|
| 0x80058E80 with the mover's node | the mover's filter (K1) | nothing hits, hurts, grabs or targets a door walker | [read] |
| 0x800E8918 | kind 39's capture (K7) | answer 0 leads to 133, then the current player's motion stop, 53, and a ticket to room 83 (§16 B1) | [read] |
| 0x800A29B4, 0x800A2C20 | kind 50 (K7): a busy-gated timer that takes an item count (0x80034E20/0x800354F8) and sends 135 to r13-26424; case 135 calls the ticket writer 0x800A2548 (transition 17) | a body switch or ticket in the window | [read the path; kind from transitions.md:294; "possession" inferred] |
| 0x80165570 | the scripts' busy getter | one script system; a woken script could ask for a room or a scene | [read site; effect inferred] |
| 0x800CA41C | the sight's sanity hit (12 creature callers) | a sanity hit in the window waits for both players, as in solo (it was K4 for the mover only) | [read] |
| 0x80061750, 0x80061A8C | kind 19's ticks (self-102 when the held one is the current id) | the grab is let go at the ticket as today (§6) | [read] |
| 0x800644A4, 0x8008106C | the save-clear-send-restore pairs (8, 39) | exact as in solo; 0x8008106C is the fireplace's close-up (0x80080D70) | [read] |
| 0x801F541C, event 75 | the delayed kind | its meaning is unread | [read structure] |
| 0x80145A0C, 0x801F22A0, 0x801F248C, 0x801F2514 | light node handler and light-record steps | lights stand for the second, as in solo | [read / inferred, §1.2] |
| 0x80144B98, 0x8017BF14, 0x80182A60, 0x80182B18 | records with a holdable flag (moves streamer, record and effect timers) | owners unread | [read the flag tests] |
| 0x800820B8, 0x8007E140 | per-room animator; global record at 0x80323920 | as solo | [read impl refuter P4] |
| 0x8005D2E0, 0x800846C8, 0x80086160, 0x80087D90, 0x8008D36C, 0x8008F478, 0x800DFBF0, 0x800E4F94 | creature and hazard self-asks (owners partly read) | unread kinds stay as solo. 0x800846C8 acts on r13-26524 by object (first draft risk 2) and is no longer reached | [read sites] |
| 0x800585E0 | answer unused | none | [read] |

K7 (split_k7_site and the kind set used by A1 and S2) is the set of busy asks and kinds whose path reaches the ticket
writer, 0x80152298 or a player's motion:
- read: sites 0x800E891C and 0x800A29B8/0x800A2C24; kinds 39, 50, 82 (0x800A475C calls 0x800A2548, transitions.md:294)
  and 32 (the sanity tracker, which ends effects through 0x8006BFAC's ticket);
- to read before building (B0, §13): the owners of the ticket writers 0x800ADA24, 0x8012D1A0, 0x800C04EC and 0x8009B760,
  and of the direct loads 0x800AD6F4, 0x80157458, 0x800ADADC, 0x800AE61C, 0x8004FC3C and 0x80020328. Any of these that
  is a node kind goes into K7.

### 3.4 S2: the door's 102 kept from the shared room (intercept split_send on 0x801F57C0)

```c
static void split_send(CPUState* cpu) {   // 0x801F57C0(event, sender, receiver id); the broadcast's call, lr 0x801F568C, receiver node r31
    g_hle_passthrough = true;
    if (cpu->lr != 0x801F568Cu || cpu->gpr[3] != 102u || !split_on(cpu) || !g_split.shared ||
        coop_object_room(cpu, cpu->gpr[31]) != g_split.room || split_kind_kept(cpu, cpu->gpr[31]))  // kind 19 or K7: as solo
        return;
    g_hle_passthrough = false;    // not sent; the loop ignores r3 [read]
    g_hle_tail_call = false;
    split_log_send(cpu);
}
```

- Install it in the stay block beside R4b (hle.c:28892-28903): first word 0x9421FFC0, the call word at 0x801F5688 =
  0x48000139 [read].
- The hurt log's notify on the same entry (hle.c:28912-28919) must accept `e->intercept == split_send` [read: it refuses
  any intercept today]. The intercept runs first and the notify only on passthrough (hle.c:5554-5577) [read].
- notify_change_hold_event's line (hle.c:25509) gets `, kept from room R's nodes by the split rule (n)` while split_on.
- Apart, 102 never reaches the stayer's room (the broadcast's own filter), so S2 has nothing to do there [read].
- 102s from the creature-code busy clears (0x80085098, 0x800851A8, 0x80085654, 0x8009C3E8) go through the same call and
  are kept the same way [read goal refuter §9].

### 3.5 S3: who the creatures go after (coop_enemy_target, hle.c:25554; split shape only)

```c
const bool held[2] = {coop_player_held(cpu, 0), coop_player_held(cpu, 1)};
if (split_on(cpu) && g_split.shared && !split_target_mover_test()) {   // ED_SPLIT_TARGET_MOVER=1 (testing, K1b/K2b): left off
    const int s = g_split.mover == 2 ? 0 : 1;                           // the stayer
    want = held[s] ? 0u : ids[s];                                       // never the mover; nobody if the game holds the stayer
} else if (held[0] != held[1])
    want = held[0] ? ids[1] : ids[0];
if (want == after) return;
if (want == 0u) { mem_write32(cpu, stats + COOP_TARGET_B, 0u); /* "[coop] N passed over: player M is held" */ return; }
```

- coop_player_held stays as it is: making the mover "held" stopped enemies coming at all (coop-failed-attempts.md:562).
- It runs from a stepped creature only (hle.c:25616), and with A1 every room creature not in K7 is stepped.
- The game agrees at its next quarry run: the stayer answers 59 (A3) and the mover answers 0 (K1) [read 0x80032A64-
  0x80032AB8 via mover-readers §2].
- Apart, R6 D4 (r6_enemy_apart) already keeps each room's creatures on that room's player [read hle.c:25145-25185].

### 3.6 S4: no re-place during the window (coop_follow, hle.c:21475)

Add `if (split_on(cpu)) return;` before the same-room branch (hle.c:21502), so nobody is brought along with the stay. It
also freezes the `was`, `stood_z` and `inside` trackers (21499-21508) for the window, and drops the fall rescue there:
that would have put a stayer player 2 beside the mover in the door, which is worse. After the window the apart branch sets
stood_z = NAN (21549), so nothing stale is left [read].

### 3.7 (The first draft's S5 is withdrawn)

- coop_seen_later (hle.c:22465) is unchanged: the sanity hit waits for both players, matching 0x800CA41C on the keep list.
- coop_talk_check (21789) is unchanged: in player 1's room change player 2's turn clears their prompt and starts nothing on
  B (hle.c:20978-20981) [read], so a TALK prompt there would have nothing behind it. The stayer's presses are F4's.

### 3.8 G1: one ticket per window (intercept on 0x80155770)

```c
static void split_ticket_ask(CPUState* cpu) {     // shares the entry with notify_room_ticket (intercept first, notify on passthrough)
    g_hle_passthrough = true;
    if (!split_counting(cpu)) return;
    g_hle_passthrough = false; g_hle_tail_call = false;
    fprintf(stderr, "[split] a second room ticket refused: room %d transition %d (lr 0x%08X, frame %llu)\n", ...);  // always printed
}
```

- The writer is void, and its whole body (record, busy, lock, cast end) is skipped, so the refused ticket also does not
  recompute g_split or g_fade_mover [read 0x80155770-0x80155814].
- Writers: fn66 (twice, already refused by R4 under coop_room_change_on), ed29 0x800E8D1C, 0x800E8FE4 (kind 39),
  0x8006BFF4 (sanity end), 0x8009B808, 0x800A281C (kinds 50/82), 0x800ADAB8, 0x800C0594 and 0x8012D2EC
  [read transitions.md table].
- G1 catches the cases where coop_room_change_on has gone false (a scene bit) and R4 lets fn66 through.
- G1 alone is not enough for kind 39, which stops the current player's motion and sends 53 before it writes (§16 B1). K7
  keeps it from starting.

### 3.9 G2: the countdown waits for what began inside the window (intercept on 0x80155830)

```c
static void split_tick(CPUState* cpu) {
    g_hle_passthrough = true;
    if (!split_counting(cpu)) return;
    const u32 extra = (busy word & ~(4u | 2u | 1u)) & ~g_split.word0;    // 8 words, 16 find, 32 scene: begun in the window
    if ((extra == 0u && !cutscene_playing(cpu)) || split_r4b_case(cpu)) return;  // R4b: mover 2, player 1's own scene/close-up
    g_hle_passthrough = false; g_hle_tail_call = false;                   // +0 and +4 not counted this frame
    split_log_wait(cpu, extra);                                           // first and last frame of each wait
}
```

- Why: the tick has no scene test and runs in both frames (§1.1). A kill in the window can wake a room script, and a
  scene or cinematic it starts would otherwise have the change fire under it. R4b handles only player 1's scene through
  player 2's door. Player 2's scene through player 1's door, and a cutscene, have no handling (impl refuter P1) [read].
- While it waits, coop_room_change_on is false (bits 8/16/32 or the cutscene). Today's no-pause rules then hold the room
  for the scene's owner, as outside a door.
- Menus are left alone: player 1's real Tome is a game mode; player 2's Start and the copy Tome are refused or kept out by
  g_tome_copy_turn.
- Open point (B3): the mover's input lock, 0x800443E4(count+3), was sized for the countdown. A long wait could outlast it
  [inferred]. Before building, read 0x800443E4 → 0x800068E0/0x8013E99C. If the lock is a count, renew it each waiting
  frame with the game's own 0x800443E4(3) in the mover's use, and check that the mover stays in state 121 (K9).
- The tick's probe notify (notify_change_tick, hle.c:28656) must accept the intercept on its entry: today it refuses an
  entry with one.

### 3.10 G3: a stayer mid-cast at the ticket (in notify_room_ticket)

- `stayer_casting()` reads the stayer's spell word: r13-22336 when the stayer's words are the ones in memory, and
  g_coop_casting[0] when they are swapped out (hle.c:12339-12370). At player 2's ticket, written in their use, player 1's
  spell is in g_coop_casting [inferred from "the ticket is written in their use"; must-read K8].
- If it is non-zero, the window does not open (`[split] player M is casting: their room holds as today`). The ticket's
  cast end 0x801C5118(0) ends the controlled id's cast record only [read 0x801C5134-0x801C5168], so the stayer's cast
  would otherwise finish in the window, and a summon or a body switch could follow (goal refuter §3; coop-glitch-todo E8).
- No new cast begins in the window: quick spells are refused (hle.c:19753-19765), and the copy Tome is out under
  g_tome_copy_turn. A cast from player 2's copy Tome closed during the window starts as it does today (not held today
  either: coop_node_held_ask's "not held") [read 19730-19746].

### 3.11 G4: a route job runs whole in the window (beside r6b_job_open, hle.c:24951)

- What was read:
  - Routes are built on the background thread 0x800BB730. The world frame sends the queue 0x805B2AFC a message when
    r13-25048 (0x800BB728) is 0 (0x80008400-0x8000841C) [read; OSSendMessage inferred].
  - Nothing found in 0x80152298 or the slot load waits for or flushes the thread [not found, not proved absent].
  - Today and in solo every room creature is held for the countdown, so no job is asked just before the firing
    [inferred].
- The guard: under split_counting, a job's graph build (lr R6B_JOB_BUILT 0x800BB918) that r6b_job_open did not bracket
  clears MSR[EE] until the job's done (lr 0x800BB9E4), as R6b does [read 24984-24990], with no graph swap.
  - The job can then not be pre-empted at the retrace, so it can never resume after the load.
  - One frame's worth of search runs at once, which R6b already accepts apart.
- Apart, jobs of the stayer's room when that room is not the room in play are R6b's today [read 24952-24954].

### 3.12 G5: a direct room load inside the window (notify on 0x80152298)

Under split_counting, log `[split] a room load inside the countdown: room R (lr 0x...)`, always printed. A ticket's own
load comes after the window, so this never fires for the door. Refusing it could leave its caller waiting on the loader's
queue, so G5 only reports it. K7 (B0) is what keeps its callers from running.

### 3.13 S6: logging (ED_COOP_ENEMY_LOG or ED_COOP_LOG; G1/G5 always)

- At the ticket: `[split] player N's door from room R (to room T): player M stays in room S (shared|apart); their room
  plays on (frame F)`, or the reason it doesn't open (switch, casting, copy turn).
- `[split] busy asked at lr 0x...: <kept: the mover | kept: a room-changer | not busy: the stayer's room>`, the first time
  per lr per window, with a count at the end.
- **Each event the mover's filter swallowed**, with the sender and its kind: `[split] the mover's filter kept event E from
  S (kind K)`. Events of interest are 11, 39, 53, 32, 59, 133 and 55. For this, notify on 0x80058E84's return path: the
  event is read before the ask, in r31's message [read mover-readers §2 for the list].
- `[split] the door's 102 kept from room R's nodes: n (kind-19/K7 nodes told: k)`.
- G2's waits, G1's refusals, G4's `[split] a route job run whole (for N)`.
- At the tail: `[split] over at frame F: a asks answered not busy, k kept, w frames waited`.
- [change-hold]'s frame line gains r13-24048 and r13-23392, so K1/K2 show that the word alone frees the room in both
  directions.
- `ed timeline` lists `[split]` lines with the room events.

## 4. The mover, both directions

| | Player 1 leaves | Player 2 leaves |
|---|---|---|
| Hold bit | cleared by fn66 (0x801667A4) [read] | fn66 in their use, and coop_door_motion (hle.c:21767-21774) [read] |
| Their events | handler at lr 0x80058E84 with r31 = node 1: K1, real word | coop_events (hle.c:21986) runs 0x80058DD8 for g_coop_p2: K1 |
| State 121, motion 25/103/104 | the game's (event 253) | theirs ("motion 25 is theirs", M2 line 501) |
| Input | 0x800443E4(count+3), the game's | the same, in their use |
| Targeted, hit, grabbed, captured | no: 59 answered 0; 11/39/53/32/133 swallowed; kind 39 kept (K7) | the same |
| A creature's body pushing them | by a few units; the runner places them on the entry marker at the firing [inferred] | the same, in the R4 bracket |
| Ticket | read by its tick only; G1 keeps it the only one | the same |
| Re-placed by coop_follow | not affected (it moves player 2) | skipped (S4) |
| Sanity sight | waits (keep list) | waits |
| Their room's creatures (apart) | stand as solo (not on the list) | the same |

## 5. Creatures after the mover take up the stayer (split)

1. At the ticket there is no chase cut (S2), so a creature in state 3 after the mover stays in state 3.
2. At its next stepped frame, coop_enemy_target turns it to the stayer, or to nobody when the game holds the stayer.
3. Between quarry runs the chase reads +20 raw (enemy-readers-2 §2), so it walks at the stayer.
4. At its quarry run the stayer answers 59 (A3) and the mover answers 0 (K1). A target kept on the mover is cleared there
   [read 0x80032A64-0x80032AB8 via mover-readers].
5. A swing already in motion at the mover: its 11 is swallowed by K1 and logged (S6). The creature finishes the swing and
   goes on [read mover-readers §3].
6. A swing at the stayer: 11, then 53 and 39 synchronously. The room gate 0x800625A4 passes (the shared room is the room
   in play), and R6's r6_hit_room passes while together (hle.c:24869-24873) [read].
7. After the firing, R6 D4 drops targets in another room, and R6c runs the room's creatures as its own game, as in M3
   [read].

## 6. A grabbed stayer: as today

- The door's 102 still reaches kind 19 (S2's exception), which lets go (0x80061148) because coop_grab_events passes a
  busy-4 102 through (hle.c:22294-22307). Kind 19's ticks keep the real word [read].
- This keeps today's outcome and avoids an unread path: whether a grabbed player 1 can wiggle under player 2's input lock
  (player 1's block had no stick, hle.c:19640-19643).
- A grab begun inside the window [inferred]:
  - for player 1, their kind-19 tick self-sends 102 while busy and lets go at once (0x80061A8C);
  - for player 2 it depends on the current id in coop_grab_events' turn.
  K10 checks both.

## 7. The stayer, state by state (both directions, both shapes)

| The stayer is... | Today | With this design |
|---|---|---|
| walking | walks (coop_node_held_ask "not held"; player 1 in room_turn) | the same |
| fighting | their swing advances; a hit on a held zombie waits; zombie swings frozen | the room's creatures move, chase, swing and are hit; the stayer can be hurt; health changes |
| killing a creature | cannot | can. The death runs (event 8 synchronous, hle.c:24860). A script it wakes: a scene or cinematic makes G2 wait; a ticket is refused (G1); fn66 is passed over by R4 and lost for a scripted door (risk 3, K1k) |
| dying | cannot be hurt | can be killed. The death motion runs. The one co-op game over came about 1500 frames after a death (coop-no-pause.md:75) [read], outside the window. Busy bits a death sets make G2 wait. The death path is unread (B2) |
| casting (under way at the ticket) | handler runs; holdable records freeze | G3: the window does not open; today's hold |
| pressing a quick spell, B, a zone, player 2's Start | refused (hle.c:19752-19765, 20978-20982 and others) | unchanged: F4 |
| player 1 in a close-up during player 2's door (bit 2) | busy 2\|4: coop_p1_close_up false, world held [read 19295-19297] | the room's nodes play; S3 picks nobody for player 1 (g_coop_p1_look) |
| player 1 in CLIMB DOWN's look (they are the mover) | everyone answered 0, the mover's filter too | K1 and K7 first, then today's 0 |
| player 1 in held words, a find or a scene (8/16/32) at the ticket | coop_room_change_on false | unchanged (word0 has the bits; G2 does not wait for them) |
| player 2 in their own close-up, words or find | held by the countdown's word | the room plays; coop_player_held(1) makes S3 pick nobody for them |
| player 2 in their own scene (32) | coop_p2_holds | unchanged |
| player 2 in their Tome copy | creatures leave them | the same; nothing of this runs in the copy's turn |
| player 1 in the real Tome | the game's mode | unchanged |
| in a cutscene | both held | unchanged; one begun in the window makes G2 wait |
| grabbed | let go by the door's 102 | the same (§6) |
| low sanity | sanity hits skip | the same (the sight's hit kept); the tracker stands (K7 kind 32) |

## 8. What stays held, and why

| Stays | Why |
|---|---|
| The mover (events, input, 121, ticket, fade) | the solo door |
| Roomless nodes, K7 kinds, the mover's room's nodes apart | as solo: their paths reach a room, a body switch or a capture, or are unread |
| Scripts asking "busy?" | one script system |
| Sanity hits, the tracker | sanity effects include room visits ending in a ticket (0x8006BE04/0x8006BFAC) and game-mode screens (sanity.md:99, readers.md:432) |
| Lights' flicker, record steppers, room animators | owners partly read; a second of stillness, as solo. A later allow-list entry, once each is read |
| Kind 19 (grabs) | §6 |
| The stayer's presses (F4) | the sibling item |
| The arrival's event 61 to the room left (0x801520DC-0x801520F4) | it lands at the firing, now often on a live fight. States that do not take it go to state 0 (route dropped, unregistered, 0x8003B850-0x8003B884); in state 33 a creature can go inert (enemy-readers-2:126-129) [read there]. M3: 40 back after the stayer by 3884, 39 by 3890. **The next visible stop**, its own item, with R6b's graph keying read first |

## 9. Door kinds

- **Doors and ladders (fn66, transitions 4-15):** the same ticket and busy 4, so the rule applies.
  - A ladder that lets its look's hold go before the ticket (L01/103) is plain busy 4.
  - CLIMB DOWN from player 1's look (busy 2|4) is inside coop_room_change_on: K1/K7 first, then today's case (§3.1).
- **Zone exits and item doors (transition 2, 12 ticks; 422 uses in doors.md):** the same window, 12 frames long.
  - An item door used from the Tome writes the ticket after the Tome closes. Player 2's copy Tome is out under
    g_tome_copy_turn; the ticket comes in their use afterwards [inferred]. K11 checks it.
- **Transitions with no motion (0-3, 16, 17):** the same window while +0 > 0. Kind 17 loads at 0 and sends 62 (0x801558C0)
  after the window [read].
- **A door whose script plays a scene first (L03/420's event 82):** the scene's bits are up at the ticket, so they are in
  word0 and G2 does not wait for them. coop_room_change_on is false, so today's rules apply [inferred]. K12 checks it.
- **Chapter pages:** Tome uses (`ed uses ed87`: page scripts 1572-1581/2028/1010, after_tome and hold_scene), never tickets
  [read goal refuter §7]. Never a split.
- **A direct 0x80152298 from a script:** no ticket and no window. G5 reports one inside a window.
- **Every room and chapter:** the rule keys on the ticket record, the busy word, the nodes' +592 and +8, and the players'
  nodes. It never keys on a room, script or level.

## 10. Interactions

| With | What happens |
|---|---|
| R1/R2 | untouched; the window ends before the load; the tail's let-go of the stayer's bit (hle.c:14529-14540) still runs |
| R4 (one ticket, bracket, g_fade_mover) | untouched; G1 adds the same rule for the ticket writer itself |
| R4b/R4c | untouched; G2 leaves R4b's case to it |
| R4d (not built) | separate; it changes the arrival |
| R5 sound | no busy read |
| R6 (hits by room) | together: r6_hit_room passes; apart: as today, now with the stayer's room moving |
| R6b | its bracket is unchanged; G4 adds the interrupt hold for jobs it does not bracket |
| R6c | off while together, on after the firing, and apart as today |
| The arrival camera | after the window |
| **Lights per room, player 2 as hero** | the light node's handler (0x80145A0C) and the light-record steps (0x801F22A0/248C/2514) ask busy and keep the word, so lights hold for the window as in solo. The first draft's "no busy read" was wrong. Both halves' shots in K1/K2 show it |
| The change-freeze probe | rows unchanged. Read the step, state, after and health columns: hold stays 1 (a standing bit) and busy 0x4 (answered, not written) |
| coop_node_held_ask | unchanged; it still answers the asks without a word test |
| coop_grab_events | unchanged |
| The Tome copy | every new path returns under g_tome_copy_turn; the dispatcher skips coop_busy_ask there (hle.c:5550-5552, kOff 27587) [read] |

## 11. Saves and loads

- Nothing new is written to guest memory but S3's target word (the field coop_enemy_target writes today) and G4's MSR
  bit, which is put back at the job's done.
- A state saved in the window holds the game's word 4 and the ticket as the game left them.
- g_split is host memory, reset in hle_coop_state_load. A state loaded mid-countdown plays the rest held, as today. The
  creatures were not chase-cut (S2), so they stand mid-chase until the change, then R6 D4 drops targets in the other room
  [inferred].
- G2's wait is host state as well. A load in the middle of a wait resumes the tick at once [inferred, harmless].

## 12. Alternatives, and why not

| Alternative | Why not |
|---|---|
| The first draft: 0 to every caller but a keep list | reaches kind 39's capture, kind 50's switch, sanity effects, spell ends, route jobs over the firing and unread kinds (§16) |
| F5: the held ask 0x801F68D4 only | 6 of 35 sites; the stayer's filter still swallows hits and 59 |
| Write busy := 0 for the window | turns coop_room_change_on off; the mover loses the filter; a save carries it |
| Clear the creatures' bit 8 | a standing mark; outlasts the door |
| No hits for the window (option (a) of impl P1) | keeps deaths out, but swings pass through for a second, and the user asked for a room that plays. G1/G2 bound what a death can start instead |
| Refuse a scene begun in the window | a script's fn22 has no clean refusal; waiting the ticket (G2) uses the game's own record |
| Let the grab play across the ticket | player 1's stick under player 2's lock is unread |
| A busy word per player | "only inside player 2's turn" failed (coop-failed-attempts.md:141) |
| Make the mover "held" | stopped enemies coming at all (coop-failed-attempts.md:562) |
| Withhold the arrival's 61 now | R6b's graph keying first (§8) |

## 13. Before building (reads, no code)

- **B0, K7's rest:** for each of 0x800ADA24, 0x8012D1A0, 0x800C04EC, 0x8009B760, 0x800AD6F4, 0x80157458, 0x800ADADC,
  0x800AE61C, 0x8004FC3C and 0x80020328, find the function's callers up to a kind handler (`ed fn`) and add the kind to
  K7. Read 0x800A2C20's path as 0x800A29B4's was. Check whether the kind-32 tracker's +592 is -1 or a room; it is in K7
  either way.
- **B1, the 8 ticket getter callers:** they read the ticket directly, not the word: 0x80064644 (room 41), 0x8007D1E0,
  0x8007D49C, 0x8007E29C (0x8007E140's busy path), 0x8008D380, 0x800E0664 (after a creature's event 8), 0x801423E4
  (room != 83) and 0x8014E0EC [read the sites].
  - Their branches do not change with S1. Whether any is in an A1-freed node's controller and acts on "a ticket counts"
    in a way solo never meets: read 0x800E0664's owner kind (a death path that skips while counting) [owner not read].
- **B2, deaths:** follow event 8 for a player (0x80061B9C's send, inside the damage taker) and for a zombie (state 8) to
  any busy set, fn22, play_cinematic, fn66, 0x80155770 or 0x80152298. Record the frame count to the game over (room 69,
  script 1500) [one sighting: about 1500 frames].
- **B3, the input lock:** 0x800443E4 → 0x800068E0, 0x8013E99C: a count or a flag, and who counts it down. That decides
  G2's renewal.

## 14. Checks for one build

After the build, run `python -I tools/ed/ed.py test` (expect the same 5 of 20 as 5eb040d's baseline,
coop-next-session.md). For every run read `ed check`, `ed timeline`, EVENTS.log, both.tsv, `[split]`, `[change-hold]`
(ED_CHANGE_HOLD=1 rows), `[change-time]`, `[hurt]` and `[enemy]`.

```sh
#!/bin/sh
# The stayer's room plays on (design-split-hold.md). Fresh Jump-to-Game states, checked before use.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_CHANGE_TIME=1 ED_RUN_SHOTS_MS=250 JTG_SLOW=1
ED="python -I tools/ed/ed.py"
ok_state() {  # coop-failed-attempts.md:772-780: hold 0x0 at 3640, script 349 before the first forced door, nothing else running
  RUN=$(ls -d logs/run-*-jtg-$1 | tail -1)
  $ED see 3640 "$RUN" | grep -q "hold 0x0" || { echo "STATE $1 NOT CLEAN: stop"; exit 1; }
  grep -n "script 349" "$RUN"/EVENTS.log | head -1 ; echo "check by eye: 349 comes before the first forced door ($RUN)"
}
rm -f saves/rooms-ch1-183.state saves/rooms-ch1-344.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 split-s0-183 </dev/null; ok_state split-s0-183
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105 ED_SAVE_STATE=2650:saves/rooms-ch1-344.state ED_RUN_FRAMES=2750 sh tools/ed/jtg.sh 1 split-s0-344 </dev/null
SET=$(ls -d logs/run-*-jtg-split-s0-183 | tail -1)/settings.ini; export ED_SETTINGS="$SET"; ST=saves/rooms-ch1-183.state
L="ED_COOP_ENEMY_LOG=1 ED_CHANGE_HOLD=1 ED_HURT_LOG=1 ED_COOP_LOG=1"
P2FIGHT="3700,3900,r;3790,3795,a;3805,3810,a;3820,3825,a;3835,3840,a;3850,3855,a;3865,3870,a;3800,3805,up"
PUT1="3680,1,1305,1508,185;3680,2,1110,1170,185"
# K3 first (measure apart, rule off): player 1 to 180, then 180 -> 344 (L01/28) while player 2 stays in 183 with the zombies.
env $L ED_SPLIT_HOLD=0 ED_RUN_FRAMES=4700 ED_PUT="$PUT1" ED_RUN_SCRIPT=110:119,170:119,420:28,480:28 sh tools/ed/run.sh $ST split-k3-apart-off </dev/null
# K3-on: the same, the rule on (apart: room 183 plays through player 1's ladder countdown).
env $L ED_RUN_FRAMES=4700 ED_PUT="$PUT1" ED_RUN_SCRIPT=110:119,170:119,420:28,480:28 ED_PAD2_SCRIPT="4100,4500,r;4300,4305,a;4330,4335,a" sh tools/ed/run.sh $ST split-k3-apart-on </dev/null
# K1: split, player 1 leaves 183, player 2 readied among the zombies and attacking.  K1-off: the same, rule off.
env $L ED_RUN_FRAMES=4300 ED_PUT="$PUT1" ED_RUN_SCRIPT=110:119,170:119 ED_PAD2_SCRIPT="$P2FIGHT" sh tools/ed/run.sh $ST split-k1-p1-leaves </dev/null
env $L ED_SPLIT_HOLD=0 ED_RUN_FRAMES=4300 ED_PUT="$PUT1" ED_RUN_SCRIPT=110:119,170:119 ED_PAD2_SCRIPT="$P2FIGHT" sh tools/ed/run.sh $ST split-k1-off </dev/null
# K1k: K1 with player 2 attacking from 3690, to bring a kill inside the window.
env $L ED_RUN_FRAMES=4300 ED_PUT="$PUT1" ED_RUN_SCRIPT=110:119,170:119 ED_PAD2_SCRIPT="3690,3900,r;3700,3705,a;3720,3725,a;3740,3745,a;3760,3765,a;3780,3785,a;3800,3805,a;3820,3825,a;3840,3845,a;3860,3865,a" sh tools/ed/run.sh $ST split-k1k-kill </dev/null
# K1b: the mover at the door with a zombie on them: player 1 put inside Region01 ([183,2994]-[541,3194]) and a zombie
# beside them, presses B on OPEN DOOR; creatures left on the mover (testing switch) so K1 is exercised the whole window.
env $L ED_SPLIT_TARGET_MOVER=1 ED_RUN_FRAMES=4300 ED_PUT="3680,1,360,3090,185;3680,2,1305,1508,185" ED_INPUT_SCRIPT="3700,3760,0,0,127;3790,3800,b" sh tools/ed/run.sh $ST split-k1b-mover-hit </dev/null
# K2: split, player 2 leaves (their B at the door), player 1 stays walking.  K2b: the mirror of K1b.
env $L ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3760,3770,b" ED_INPUT_SCRIPT="3700,4100,0,0,127" sh tools/ed/run.sh $ST split-k2-p2-leaves </dev/null
env $L ED_SPLIT_TARGET_MOVER=1 ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3700,3760,up;3790,3800,b" sh tools/ed/run.sh $ST split-k2b-mover-hit </dev/null
# K4: a ladder split in 344 (L01/111, transition 15).  K5: follow mode.  K6: solo.
env $L ED_RUN_FRAMES=3000 ED_RUN_SCRIPT=110:111,170:111 sh tools/ed/run.sh saves/rooms-ch1-344.state split-k4-ladder </dev/null
env -u ED_COOP_STAY $L ED_RUN_FRAMES=4300 ED_PUT="$PUT1" ED_RUN_SCRIPT=110:119,170:119 sh tools/ed/run.sh $ST split-k5-follow </dev/null
unset ED_SETTINGS
JTG_SLOT=1 JTG_SET="mod_coop=0" env $L ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29,3820:119,3880:119 ED_RUN_FRAMES=4300 sh tools/ed/jtg.sh 1 split-k6-solo </dev/null
echo RUNS DONE
```

The places (zombie 40 at 1042 1101 93 at 3822 in M3 [log 870]; Region01 from `ed door 183` [read goal refuter]) are
confirmed by each run's first row. If ED_PUT moved a player, take the zombie's place from that row and run again. For
K1b/K2b, put the zombie beside the mover with ED_PUT of its node if the kit allows; otherwise walk the mover past the
zombies to the door first, from the rows' places.

| Run | Must read | Settles |
|---|---|---|
| K3-off | room 183's zombie rows through player 1's ladder countdown: `step` 0.0 or not; `[change-hold]` r13-24048/-23392 | apart measured before the rule (the user's report) |
| K3-on | `[split] ... apart`; room 183's zombies step > 0 through it; player 2's attacks land; room 180's nodes `step 0.0` (the mover's room as solo); P1 in state 121 to the firing and arriving in 344 | the apart shape plays on |
| K1 | `[split] player 1's door ... shared` at 3821; `102 kept from room 183's nodes`; rows 3822-3881: busy 0x4, ticket counting down, zombies `step` > 0 and `after` the stayer within the first rows; player 2's anim changing; `[hurt]` to or from 59 inside the window wanted, not required; P1 state 121 every row, health unchanged, arrival in 180 with `hold bit := 1 on node 1 ... lr 0x80152194`; `[split] over ...`; **firing+20 (3882-3902): each zombie back in state 3 after the stayer, none inert**; lights: both halves' shots 3822-3881 (flicker held, as solo) | the room plays on; the door is the game's |
| K1-off | zombies `step 0.0` 3822-3881, no `[split]` lines | the A/B in one build |
| K1k | a zombie's death (state 8) inside 3822-3881, if reached; any `[split] a second room ticket refused`, `[split] ... waits`, `[stay] a room asked for ... passed over` inside the window; the change firing on time otherwise | a kill in the window |
| K1b | `[split] the mover's filter kept event 11/39/53 from <zombie> (kind ...)` lines; P1 state 121 to the firing, health unchanged; arrival state 80/81 with motion 25/104/103 (event 238's answer); no creature's +592 becomes 180; no room-180 row gains a node of 183 | the mover protected; nothing follows them |
| K2 | `[split] player 2's door ...`; `motion 25 is theirs`; room 183's zombies moving through player 2's countdown, `after` player 1; P2 in 121 to the firing and arriving in 180; player 1 walking every row | the other direction |
| K2b | as K1b for player 2 | the mover protected, other direction |
| K4 | `[split]` for room 344; P1's climb and arrival on 103; player 2 untouched | a ladder split |
| K5 | no `[split]` lines; zombies `step 0.0`; player 2 brought along | follow mode unchanged |
| K6 | no `[split]` or co-op lines; zombies `hold 1 step 0.0`; `[change-time] ... solo` | solo unchanged |
| any | `ed check` stalls; `[enemy]` route lines through the firing; `[split] a route job run whole` where a creature was more than 550 units off | G4 |

Further runs (states made the same way; `ed door`, `ed where` and `ed transition` give the places and doors):
- **K7, a grabbed stayer** (ch11 room 50, the test helper ED_TEST_TARGET=2 of coop-no-pause.md:79), both directions:
  let go at the ticket as today, no BREAK FREE left on either half.
- **K8, the stayer mid-cast at the ticket**, both directions: `[split] player M is casting: their room holds as today`.
- **K9, a wait:** a scene begun in the window (from K1k's wake, or a room script found with `ed script`): `[split] the
  countdown waits` first and last frames, the mover in 121 throughout, the firing after the scene.
- **K10, the stayer killed or grabbed inside the window**, both directions, with the stat test helper ED_SPLIT_TEST_STAT
  (testing: at the ticket, the game's own setter 0x800354F8(character, 0, value, 1), as kind 50 calls it at 0x800A2A84
  [read the call; the health kind 0 from hle.c:6859]): the death motion, the game over's frame, no ticket refused.
- **K11:** a Tome item door from room 183 (L01/1100-1103, transition 2) by each player, and a zone exit.
- **K12:** CLIMB DOWN from player 1's look (344's L01/111 with player 1's B: ED_INPUT_SCRIPT), and the scene-first door
  L03/420 (chapter 3).
- **The user's play** (play-rooms-log.bat): a door while both are in one room with enemies, in both directions, the
  stayer fighting; then player 1 fighting in one room while player 2 enters another (their report). A scripted player 1
  cannot ready a weapon (ED_INPUT_SCRIPT has buttons, the stick and analog A only, hle.c:177-183).

## 15. Risks and open points

1. **K7 is incomplete until B0.** Ten room-change sites have unread owners. Until they are read, G1 refuses their tickets
   and G5 reports their loads, but a kind that stops a player's motion before writing (as kind 39 does) would act first.
2. **Deaths (B2) are unread.** What a creature's or a stayer's death starts in its first 62 frames is not known. G1 and G2
   bound it to "no second room and no firing under a scene", but they don't cover flags or drops set by a death script
   that solo would have run after the door.
3. **A scripted fn66 woken in the window is lost.** R4 passes it over and returns 0; a scripted door has no press to come
   again. Look for `[stay] ... passed over` inside windows in K1k and in the user's play.
4. **G2's wait and the mover's lock (B3).** A long scene in the window could let the mover's lock run out mid-door.
5. **Event 61 at the firing** now meets a live fight (§8): a hitch at the firing, or a creature gone inert. It is the
   next item.
6. **Unread allow-list effects.** A1 frees every room node of a kind not in K7, including kinds never read: hazards and
   room machinery whose update is plain node-gated. `ed check` stills and stalls and both halves' shots are the watch.
   Any that misbehaves goes into K7.
7. **Lights and room animators hold** for the window (§8). The stayer may notice still flames for a second.
8. **F4** (the stayer's presses) is unchanged: "my quick spell did nothing" for a second.
9. **A grab begun inside the window** (§6): unread for player 2.
10. **The apart shape** is included without a measured "before" (K3-off is the first). If K3-off shows apart already
    plays on, `ED_SPLIT_HOLD=split` is the shipping value and apart needs nothing.
11. **The Trapper's capture of "the current player"** (0x800E896C uses 0x801F62B0, not the target it sent 133 to)
    [read]. In co-op a Trapper after player 2 would take player 1 outside any door. That is a co-op item of its own
    (coop-glitch-todo E6), not this one.

## 16. Refuted or corrected

| # | Point (refuter) | Checked against the code | Verdict | What changed |
|---|---|---|---|---|
| B1 | A creature can write a second ticket; kind 39's capture is behind S1's busy ask (goal, blocker) | 0x800E8918 `bl 0x80155738; bne`, also 0x8004303C, r13-22216 & 7, 0x800E912C; 133 to +384 (0x800E8958); on 1: 0x801F62B0/0x801F633C (the current player), kind-39 hold bits cleared (0x800E89EC), 0x80124788 at 0x800E8A40, 53 at 0x800E8A54, 0x800E8E14 → `li r3,83 ... li r8,190; bl 0x80155770` at 0x800E8FE4 [read]. Kind 50: 0x800A29B4 busy-gated timer and item count, 135 to r13-26424, case 135 → 0x800A2548 → ticket at 0x800A281C [read] | **real** | S1 became an allow list; K7 (sites and kinds) is decided before everything; G1 refuses a second ticket; S2 still sends 102 to K7 kinds; §0/§1.2 corrected |
| B1b | The 8 callers of 0x80155818 | read at each site (B1 of §13) | real (reads), no change in behaviour | listed; 0x800E0664's owner to read |
| G2r | The stayer's death is new and unread (goal, major) | the game over came about 1500 frames after a death [read coop-no-pause.md:75]; event 8 is synchronous (hle.c:24860); the path itself is still unread | **real** | G1 and G2 bound it; B2 read before building; K10 kills the stayer in the window |
| G3r | Casts begun before the ticket finish in the window (goal, major) | the ticket's cast end matches the controlled id only [read 0x801C5134-0x801C5168]; casting words per player [read hle.c:12339-12370] | **real** | G3: no window for a stayer mid-cast; record and effect sites keep the word; K8 |
| G4r | No check exercises the mover's protection (goal, major) | Region01 and the K places [read refuter]; S3 turns creatures away | **real** | K1b/K2b with ED_SPLIT_TARGET_MOVER=1; a log line per swallowed event; must-reads for nothing following and the arrival motion. (The refuter's "stayer in words so S3 picks nobody" would clear the target rather than keep it on the mover; the testing switch does that job) |
| G5r | The user's report is the apart case (goal, major) | the readers' standing-bit reading applies apart [read their reports]; no creature rows apart | **real** | the rule covers both shapes ("the stayer's room"); K3-off measures first; `ED_SPLIT_HOLD=split` limits it |
| G6r | Stayer states not run; grab and kill-woken scripts open (goal, major) | player 1's block has no stick in player 2's change (hle.c:19640-19643) [read]; R4 drops a second fn66 (16748) [read]; 0x800E0664 guards a post-death path by "no ticket" [read] | **real** | a grab is let go as today (§6); G2 for scenes; risk 3 for fn66; K7-K12 runs |
| G7r | Door kinds not run (goal, minor) | chapter pages are Tome uses [read refuter] | real | K11, K12; §9 adds zone exits, item doors and scene-first doors |
| G8r | Event 61 lands on a live fight (goal, minor) | 0x801520DC-0x801520F4 [read]; enemy-readers-2:126-129 | real | firing+20 must-read in K1/K2; named as the next stop |
| P1 | Kills, death scripts and sanity hits reachable with a ticket live; the tick has no scene test (impl, major) | 0x80155830 tests only +0/+4; called at 0x800083EC and 0x80050A90 [read] | **real** | sanity hit kept for both; G1, G2; K1k; option (a) declined (§12) |
| P2 | Route jobs can straddle the firing (impl, major) | R6b's job open is apart only (24952) [read]; the world frame feeds 0x805B2AFC (0x80008400-0x8000841C) [read]; no flush found in the change [not found] | **real** | G4: interrupts held for a job opened in a window; check `[enemy]` lines through the firing |
| P3 | Flag 0x10000 of r13-24048 left out (impl, minor) | set at 0x801557E0, cleared at 0x8015203C; the world frame's prompt clear at 0x800084DC-0x800084EC [read] | real; harmless | §1.2: the game's "prompts off during a door", no creature reads it; both it and the byte r13-23392 go on the [change-hold] line |
| P4 | Mislabelled sites; "lights: no busy read" wrong (impl, minor) | as the refuter read | real | §1.2, §3.3, §10 corrected; the lights keep the word; shots in K1/K2 |
| P5 | coop_talk_check would show a dead TALK prompt (impl, minor) | hle.c:20978-20981 [read] | real | S5 withdrawn (§3.7) |
| P6 | S4 freezes coop_follow's trackers (impl, minor) | hle.c:21499-21508, 21549 [read] | real, harmless | stated in §3.6 |
| P7 | The run script remakes the 183 state without the fresh-state check (impl, minor) | coop-failed-attempts.md:772-780 [read refuter] | real | `ok_state` stops the script on a dirty state; JTG_SLOW=1 |

## 17. Facts read for this design

| Fact | Where | Mark |
|---|---|---|
| ticket writer body, incl. r13-23392 := 1 and flag 0x10000 | 0x80155770-0x80155814 | [read] |
| ticket getter; tick and its two callers | 0x80155818-0x80155924; 0x800083EC, 0x80050A90 | [read] |
| r13-24048's setter/getter, the world frame's prompt clear, the change's clear of 0x10000, 0x801176B4, 0x80117338 | 0x80117260-0x80117284, 0x800084D8-0x800084EC, 0x8015203C-0x80152044, 0x801176B4, 0x80117338 | [read] |
| kind 39's capture and room-83 ticket | 0x800E88FC-0x800E8A68; 0x800E8E14 calls to 0x800E8FE4 | [read] |
| kind 50's busy-gated 135 and the ticket writer 0x800A2548 | 0x800A2990-0x800A2ABC, 0x800A3274-0x800A32A8, 0x800A2548-0x800A282C (calls) | [read] |
| the 8 ticket getter callers | 0x80064644, 0x8007D1E0, 0x8007D49C, 0x8007E29C, 0x8008D380, 0x800E0664, 0x801423E4, 0x8014E0EC | [read sites] |
| post-death "no ticket" guard | 0x800E065C-0x800E066C | [read] |
| node registers at the node-gated sites; the pump's message and receiver | 0x8000749C-0x800074B4 (r28); 0x800271F0/0x80027298/0x80027764/0x800277F4 (r31); 0x801F5408-0x801F542C (r29, +8) | [read] |
| the 35 busy-ask callers | grep `bl 0x80155738` | [read] |
| the route thread flag and the world frame's job message | 0x800BB728 (r13-25048), 0x800BB730-0x800BB754, 0x80008400-0x8000841C | [read] |
| 0x800443E4 calls 0x800068E0 and 0x8013E99C | 0x800443E4-0x80044414 | [read; meaning not read] |
| the cast end matches the controlled id | 0x801C5118-0x801C5190 | [read] |
| coop_room_change_on, notify_room_ticket | hle.c:11333-11380 | [read] |
| R4's fn66 rule | hle.c:16747-16756 | [read] |
| coop_p1_close_up's room bit, coop_p2_holds, coop_player_held, coop_busy_ask, coop_node_held_ask, the quick-spell refusal | hle.c:19283-19300, 19645-19674, 19704-19765 | [read] |
| the casting words and the swap | hle.c:12339-12370 | [read] |
| R4b's stay_busy_clear | hle.c:25060-25075 | [read] |
| R6b's bracket | hle.c:24924-24990 | [read] |
| the ticket's and the tick's hook installs (notify only; the probe refuses an intercept) | hle.c:28632-28634, 28650-28675 | [read] |
| player 2's turn in player 1's change: no prompt, nothing on B | hle.c:20978-20982 | [read] |
| coop_follow's trackers | hle.c:21495-21510 | [read] |
| the ticket writers and direct loads | transitions.md "Every other way a room changes" | [read] |
| the sanity tracker (kind 32), its return ticket | sanity.md:91, 99; readers.md:432 | [read] |
| the game over after a co-op death; the grab tests in ch11 room 50 | coop-no-pause.md:75, 79 | [read] |
| M3's rows, the 102 to 19 nodes, the arrival's re-hold | logs/run-20261112-180114-freeze-m3-p1-leaves-fight page-0001.log 145-146, 852-880, 1600-1700 | [log, via the first draft] |

## 18. K3-off measured (2026-11-13, today's build, before any change)

logs/run-20261113-124840-split-k3-apart-today (docs/research/separate-rooms/build-run-split-k3.sh). Player 1 went 183 -> 180
by 119 (ticket 3822, the split shape: room 183's zombies `step 0.0` through 3884, as M3). Then, apart, player 1 took
180's ladder 28 to 344 (ticket 4132) while player 2 stayed in 183, where zombies 41, 39 and 38 were in state 3 after
player 2 (`after 59`). Player 2's health had gone 60 -> 53 from their hits between the two doors. Through player 1's ladder
countdown (rows 4133-4192, busy 0x4, ticket 61 -> 2), every one of them shows `step 0.0` and the same place each row,
hold 1. [log]

So the apart shape is held exactly as the readers inferred: the stayer's room stands for the other's countdown. This is
the user's own report. `ED_SPLIT_HOLD` ships with both shapes on (unset or 1); `split` stays a testing value.

## 19. Decisions after B0-B3 (what is built)

From b0-k7-readers.md, b1b2-ticket-death-readers.md and b3-input-lock-readers.md:

1. **K7 kinds (node +8), never on the allow list, still sent the door's 102:** 9, 28, 32, 39, 50, 56, 73, 74, 82.
   - 28, 39, 50, 82: paths to the ticket writer or a capture [read].
   - 32: the sanity tracker (Walk on Ceiling's ticket 0x8009B760); roomless (+52 0) so A1 never reaches it anyway.
   - 73: reaches fn66 through script 1183. 56: a boss's movie start (optional, kept for safety).
   - 9 and 74: refuse to die while a ticket counts (0x8008D380, 0x800E0664), so a killing blow inside the window would
     leave them at health 1, which solo never meets. They stand as in solo.
2. **K7 busy-ask sites (lr) that keep the real word:** 0x800E891C (kind 39), 0x800A29B8 and 0x800A2C24 (kind 50, the
   second is kind 50's copy of the player's busy filter), and 0x80058E84 when r31 is the mover's node (K1).
3. **G1** refuses a second ticket inside the window, always printed. In addition, the scene-request dispatcher
   0x800B7208 is passed over inside the window while its request is 1, 16, 32 or 64 (ed145 and the other room
   requests), so the request stays and runs after the change instead of being lost [b0 §fixes; the implementer reads
   0x800B7208 to confirm the pass-over leaves the request in place, else this falls back to a loud log]. A cinematic's
   own ticket at its end (lr 0x8012D2F0) is refused and logged loudly: a known rare risk (risk 12).
4. **G2** waits only while ticket +0 > 1 (callers act on +0 == 1: 0x8007D1E0, 0x8007D49C, 0x8014E0EC). While waiting:
   player 1 the mover: the game's own 0x800443E4(+0 + 3) once per waiting frame (a guest call from the tick intercept,
   the game's copy in). Player 2 the mover: nothing is renewed (the lock is not theirs); a wait over 50 frames is logged
   (their 120-frame door-turn window).
5. G3, G4, G5, S2, S3, S4, S6 as sections 3.4-3.13. The testing switches: ED_SPLIT_HOLD (unset/1 both shapes, `split`,
   0 off), ED_SPLIT_TARGET_MOVER=1. Everything is behind mod_coop, g_coop_p2, ED_COOP_STAY's stay and ED_ROOM_SLOTS's
   installs; solo and follow mode run none of it.
6. Risk 12: a scene or cinematic begun inside the window whose end writes its own ticket loses that ticket (G1). It needs
   a kill inside a one-second window in a room whose kill wakes a scene; watched by the always-printed line.
