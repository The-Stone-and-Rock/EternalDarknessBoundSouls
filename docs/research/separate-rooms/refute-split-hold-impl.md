# Refuter (implementation lens): design-split-hold.md against the code as it stands

Lens: conflicts with the current implementation: the host code the design touches and what is around it (the holds,
docs/coop-no-pause.md, the R4 bracket, R4b/R4c, coop_follow, coop_enemy_target, the probe), hooks on the same addresses,
double runs, passthrough, save/load, room-change edges, and docs/coop-failed-attempts.md. Sources: runtime/host/hle.c
(uncommitted, 29,549 lines), build/ed-db/disasm.txt, `ed fn`, the Phase 1 runs logs/run-20261112-1801*-freeze-*.
No code was changed, nothing was built, the game was not run.
[read] = seen at the address, hle.c line or log given. [inferred] = reasoned, not seen.

Verdict: **needs-changes.** The mechanics hold against the code: every hook point is free or shareable, the lr and
register reads are right, and the passthrough semantics work as written. But S1 opens game states that neither solo nor
today's split can reach: a creature killed, a sanity hit, or a fresh route job while a room-change ticket is live. The
design argues it opens none (§0.2). Two of these have no guard, and one is a possible memory hazard across the firing.

## 1. What was checked and holds

| Claim in the design | Checked against | Result |
|---|---|---|
| coop_busy_ask is the only intercept on 0x80155738 and answers every caller | install hle.c:28389-28393 (first word 0x806DA4D0, intercept free); the word r13-23344 is read only at 0x801556E0 (busy set), 0x801556F4 (its store) and 0x80155738 in the whole DOL | [read] holds: no direct reader of the word gets past S1 |
| 35 callers; the keep sites' return addresses | `bl 0x80155738` grep: 35 sites; lr+4: 0x80058E84, 0x800644A8, 0x80081070, 0x80165574, 0x800CA420, 0x801F5420, 0x80061754, 0x80061A90 | [read] all correct |
| K1: r31 is the node at the player handler's ask | 0x80058DF8 `or r31,r3,r3`; r31 not written again before 0x80058E80 | [read] |
| K4: the sanity hit is for the current id | 0x800CA3A8 `bl 0x801F62B8` -> r27, compared at 0x800CA3E4; the ask at 0x800CA41C | [read] |
| K5: r29 is the message at 0x801F541C, +0 its event | 0x801F5408, 0x801F5444-0x801F5448 (`cmpwi r0,75`) | [read] |
| K6: r29 is the held id at both kind-19 asks | 0x8006175C-0x80061760, 0x80061A98-0x80061A9C | [read] |
| K2: the pairs clear with the answer, so a 0 answer leaves the word 4 and broadcasts 102 on the "clear" | 0x800644A8 `li r4,0` then busy set(r3=answer,0); 0x80081070 same; busy set's andc at 0x801556E4 | [read] the design's reason for K2 is right |
| Only the busy set broadcasts 102 | 11 `bl 0x801F5620` sites; `li r3,102` before one only (0x80155718) | [read] |
| S2: at 0x801F57C0's entry r31 is the receiver node, and skipping the call is safe for the loop | the loop keeps its state in r27-r31 and f31 only (0x801F5638-0x801F5644, 0x801F564C, 0x801F568C); 0x801F57C0 saves and restores r31/f31 itself (0x801F57D4, 0x801F582C), so a skipped body leaves them as they were | [read] holds. The skip also leaves out the message serial r13-26840 += 1 (0x801F5814): harmless [inferred] |
| The game's room filter is the object's +592 | 0x801F662C -> 0x80118CAC = `lwz r3,592(r3)` | [read] coop_object_room matches; a node with an invalid object reads -1 in the host and still gets 102 (as the design wants) |
| 0x801F57C0 is free of other hooks except the hurt log's notify | only hle.c:28912; install order: the ED_COOP_STAY block (28826-28903) runs before `if (hurt_log())` (28906) | [read] holds. The hurt install (28915-28917) refuses any intercept today, so the design's change there is needed |
| Intercept, then notify only on passthrough | dispatcher hle.c:5554-5577 | [read] holds: hurt_note_later still sees every event the split does not keep |
| The Tome copy | dispatcher 5550-5552 skips coop_busy_ask in the copy's turn (kOff, 27587); split_send is not in kOff, but split_on tests g_tome_copy_turn | [read] holds |
| notify_room_ticket runs before the ticket and busy are written | it is the notify on 0x80155770's entry; the ticket's +0 is stored at 0x801557BC, then busy set(4,1) at 0x801557EC broadcasts | [read] so g_split is set before the 102 broadcast, and split_on is true inside it (word stored at 0x801556F4 before 0x80155720) |
| The window ends before the runner | the tick stores +0-1 (0x80155858), then tests +0 == 0 and calls the runner in the same call (0x80155888-0x801558F4) | [read] split_counting is false inside the runner, load and arrival |
| R4 bracket after the window | g_stay_bracket = 2 only in notify_ticket_run (hle.c:14047-14060, the runner) | [read] |
| coop_grab_events' busy-4 exception is never reached | it acts only on a delivered 102 (hle.c:22296-22307); S2 keeps every broadcast 102 from the shared room; the pairs' restores also broadcast through 0x801F5620 and are kept too | [read] holds |
| The p1_look/room_turn turns write busy 0 in memory | coop_hold_open (hle.c:19788) from hle_coop_controller 20298 (p1_look) and 20348 (room_turn) | [read] inside those turns split_on is false and the word really is 0. No conflict: K1 matters only outside the turns, where the world's hits come from |
| coop_p1_close_up's room bit is only for player 1's own change | hle.c:19295 `coop_room_change_other(cpu, true)` | [read] so a hold opened for player 2's turn never has player 2 as the mover; the exposure in §4's last paragraph cannot arise inside a turn |
| The tsv `hold` (r13-25696, the play hold) stays 0 through every measured countdown | both.tsv col 45: M1, M2 (a B-pressed door), M3, M5, M8, S0 all 0x0 with busy 0x4 | [log] the 53 callers of 0x8004303C are not part of a door's hold |

## 2. Problems

### P1 (major). S1 makes creature deaths, death scripts and sanity hits reachable while a ticket counts down; nothing stops the ticket under what they start

- The design says (§0.2): "busy 0 for all but the mover can reach nothing ordinary play does not reach". That is true of
  each frame on its own, but not of a frame with a live ticket. In solo and in today's split, between 0x80155770 and the
  firing:
  - no creature takes a hit (the held zombies; "a hit on a held zombie waits", design §7);
  - the hero's filter swallows hits;
  - the sanity hit skips (0x800CA41C).
- With S1, the stayer kills creatures and loses health and sanity during the countdown.
  - A creature's death runs its death event (event 8 is synchronous, hle.c:24860). Its kind's or room's scripts react to
    it. In the one script system that can:
    - set flags;
    - start a scene (fn22, busy 32) or a cutscene;
    - queue a room (fn71).
  - A sanity hit on a low-sanity stayer can begin a sanity effect [inferred: ED's effects are keyed on sanity; the trigger
    path was not read].
- The ticket does not wait for any of this:
  - 0x80155830 tests only +0 and +4 [read 0x80155830-0x80155924];
  - it is called at the top of the world frame 0x800083C8 (0x800083EC) and the other mode frame (0x80050A90), with no busy
    or scene test before it [read]. Whether 0x800083C8 runs in a cutscene's frames was not read.
- The only host guard is R4's fn66 refusal (coop_script_go_to_room, hle.c:16748). It refuses neither a scene, nor a
  cutscene, nor fn71's room_queue, nor a 0x80155770 reached any other way.
- What follows:
  - A scene begun by a kill in the window turns coop_room_change_on off (bit 32). From then on today's no-pause rules
    decide the holds.
  - But the runner still fires 62 frames after the ticket, inside that scene. The room in play changes to the mover's new
    room under a scene whose script and cameras belong to the stayer's room. R4b/R4c handle only player 1's scene through
    player 2's door (stay_busy_clear needs g_stay_bracket and lr 0x80152188, hle.c:25060-25064). Player 2's scene through
    player 1's door, or a cutscene started by a death, has no handling.
  - A second 0x80155770 inside the window (fn71, or a creature's room script) would rewrite the one ticket record.
    notify_room_ticket would recompute g_split, with the mover taken from the current id [inferred].
- K3 (the scripts' busy getter) does not cover these: they are started by events, not woken by "not busy".
- Fix: before building, read:
  - a zombie's death path (0x80061B9C's event 8 onward, and what the room's script system does on a death);
  - whether 0x800083C8 runs in a cutscene's frames.
- Then choose one of:
  - (a) keep the stayer's hits and the sanity hit off the creatures for the window. This keeps the room moving but no
    deaths. It costs the fight's feel for one second.
  - (b) refuse or defer a scene, cutscene or ticket begun inside the window, the way R4 defers fn66.
- Add a check: K1 with player 2 killing the last zombie inside 3822-3881 (a zombie at low health from an earlier hit),
  and K1 with the stayer at low sanity.

### P2 (major). Route jobs requested in the window run on the game's background thread and can straddle the firing; R6b does not cover a job opened while together

- hle.c:24946-24950 [read]: routes are built by the game's background thread (0x800BB730, priority 31), which runs while
  the main thread waits for the retrace. A job builds the point graph r13-25060 from the current slot and keeps the path
  as pointers into it.
- r6b_job_open only acts when r6_apart (hle.c:24952) [read]. In the window the players are together, so a job opened then
  runs on the shared room's graph with no bracket. Interrupts are left on, so the main thread can preempt it at the
  retrace [inferred from R6b's own reason for holding MSR[EE]].
- Today, and in solo, every creature of the room is held for the last ~62 frames, so no new route job is queued just
  before the firing [inferred].
- With S1, a creature more than 550 units from the stayer asks for a route on every quarry run, up to the firing frame. A
  job preempted mid-search resumes after the load has changed the current slot and room in play to the mover's new room.
  Its path would then point into a graph rebuilt from the other room's collision.
- The arrival's 61 drops routes held at the firing (design §8). It cannot drop one a job writes after it.
- The design names this graph hazard as its reason to leave the 61 alone (§8, §12), but not as a new effect of S1.
- Fix, either:
  - read whether the change waits for or flushes the job queue 0x805B2AFC and the thread (the runner or the slot load
    0x80154C94), and record it in the design; or
  - keep the route request out of the last frames, by putting its site on the keep list or by a host wait.
- Check: K1 with a creature placed more than 550 units from the stayer, `[enemy]` route lines through the firing, `ed
  check` stalls.

### P3 (minor). The ticket also sets r13-24048 |= 0x10000; the "one word" claim leaves it out

- 0x801557E0 `bl 0x80117260` with r3 = 0x10000 (`lis r3,1`), r4 = 1, sets bit 0x10000 of the flags word r13-24048
  [read 0x801557B0-0x801557E0]. Its readers:
  - 0x8011727C(-1) at 0x800084DC in the world frame, which on any flag calls 0x80117360(0);
  - 0x80117338 and 0x801176B4 [read the grep].
- What 0x80117360 does was not read. coop_hold_open clears bit 4 of this word for a turn, not 0x10000 (hle.c:19785).
- Nothing shows this bit holds creatures: M3's zombies were measured held with busy 0x4. But both.tsv does not record
  r13-24048, so for K2 (player 2's B door, whose zombies were never row-logged in Phase 1) it is unmeasured.
- Fix: add r13-24048 to the [change-hold] frame line, or read 0x80117360, so K1/K2 can show that S1 alone frees the room.

### P4 (minor). Several of the sites answered 0 are not what §3.2 calls them; one contradicts §10

- 0x80145A0C is in 0x80145994, the light node's event handler (design-lights.md:139-140) [read the function start
  0x80145994 and its +41 flag tests]. §10's "Lights per room, player 2 as hero: no busy read" is wrong.
- 0x801F22A0, 0x801F248C and 0x801F2514 lie in the light-records module (0x801F1B40 allocator ... 0x801F2858 timed step).
  They test the same +41 & 1 flag byte that the light handler reads at 0x80145A18. That makes them light-record steps, not
  "spell sites" or "effect timers" [inferred from address range and field].
- 0x80082090 is a main-loop per-room animator: its one caller is 0x80008208, the world frame, and it switches on the room in
  play r13-23276 [read 0x80082190-0x80082230]. It is not a creature.
- 0x8007E140 steps a global record at 0x80323920 (+20 counted mod 3600) [read].
- Effect: lights flicker and the room's own animators run through the countdown. That is likely right for "an ordinary
  frame", but it reaches lights per room (G2/G3) and the light-record budget for the first time in a window.
- Fix: correct §3.2 and §10, and add the lights to K1/K2's watch list (both halves' shots across 3822-3881).

### P5 (minor). S5's coop_talk_check change shows a prompt the stayer cannot press

- Through player 1's room change, player 2's turn clears their prompt and starts nothing on B (hle.c:20978-20981:
  `g_coop_prompt = -1` under room_other) [read].
- Opening coop_talk_check (21789) in the window looks for player 2 as a talker's target and fills their talk slot. That
  gives a TALK prompt in their half with no press behind it.
- Fix: leave coop_talk_check as it is (the stayer's presses are F4, the sibling item). Or gate the talk slot on the same
  room_other test.

### P6 (minor). S4's placement freezes coop_follow's trackers for the window

- `if (split_on) return;` before the same-room branch also skips the updates of `was`, `stood_z` and `inside`
  (hle.c:21499-21508) [read].
- After the window the rooms differ and the apart branch sets stood_z = NAN (21549), so nothing stale survives [read].
  There is no harm, but the skip also drops the fall rescue for a stayer player 2 knocked off a ledge by the now-moving
  creatures during the window. The rescue would have placed them beside the mover in the door, which is worse.
- Fix: none needed; state it in the design.

### P7 (minor). The run script deletes the known-good 183 state and does not check the remake

- S0 runs `rm -f saves/rooms-ch1-183.state saves/rooms-ch1-344.state` and remakes them from Jump to Game.
- coop-failed-attempts.md:772-780 requires a check before a fresh JTG state is used: `ed see 3640 RUN` must show
  `hold 0x0`, script 349 must come before the first forced door, and nothing else may run (or set JTG_SLOW=1). A state
  made inside the intro held player 2 for good.
- Fix: after each S0 run, add the `ed see` check and a stop when it fails, plus JTG_SLOW=1 if other agents may be running.
  The fresh-states rule stays.

## 3. Points checked and not problems

- The double ask in one frame (a creature asking at several sites): S1 is stateless apart from the log table [read §3.1].
- g_split across loads:
  - hle_coop_state_load (hle.c:29083) is the right place to reset it;
  - g_coop_room_p2 is not restored by a load (only 11363, 14102 write it) [read];
  - a load mid-window plays the rest held. The creatures were not chase-cut (S2), so they freeze mid-chase until the
    change, then R6 D4 drops targets in the other room [inferred]. That is acceptable.
- R6 hit takers: r6_hit_room returns at once together (hle.c:24871-24872) [read].
- R6c: r6c_creature returns unless r6_apart (25098) [read].
- notify_change_hold_event is on 0x801F5620, a different entry from split_send's 0x801F57C0. Its count is by the game's
  filter (25516-25523) [read], so the design's extra text on its line is needed.
- coop_enemy_target writes only COOP_TARGET_B for "nobody", as r6_enemy_apart does (25181) [read]. COOP_TARGET_A left on
  the mover is cleared by the game's own quarry, since the mover answers 59 with 0 under K1 [read mover-readers §2].
