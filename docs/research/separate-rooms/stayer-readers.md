# Stayer readers: the stayer during the other player's countdown, and what the host does today

Reader report, read only. No code was changed, nothing was built, and the game was not run. Sources:
`runtime/host/hle.c` (line numbers as the file stands, uncommitted, on top of 5eb040d), the disassembly through
`python -I tools/ed/ed.py dis` / `fn`, `build/ed-db/disasm.txt`, the Phase 1 runs
`logs/run-20261112-180100-freeze-m1-p1-leaves`, `-180114-freeze-m3-p1-leaves-fight` (the only run with `[change-hold]`
rows), `-180128-freeze-m2-p2-leaves`, `-180142-jtg-freeze-m5-apart-apart`, and the docs named in the task.
[read] = seen at the address, hle.c line or log row given. [inferred] = reasoned, not seen.

Words: the **mover** (or asker) takes the door; the **stayer** stays. A **split** is both in one room when the mover's
ticket is written. **Apart** is the mover already in another room. "The countdown" is ticket +0 from about 61 down to 0
(about 62 world frames), from `notify_room_ticket` to the change (0x80152298) and its tail (0x80154B54 at lr 0x80152434).

## 0. In short

1. **The stayer's own body is already free through the countdown, in both directions.** `coop_node_held_ask`
   (hle.c:19727-19746, intercept on 0x801F68D4) answers "not held" for the non-asker's player node while
   `coop_room_change_on`. The M1/M3 logs say `player 2 goes on through player 1's room change: not held (lr 0x80027778)`,
   and M2 says the same for player 1 (lr 0x80027204) [read]. The earlier split walk runs show the stayer walking at full
   rate through the countdown (change-freeze-measure-readers.md, r6c-walk3) [read there]. What stops in a split is
   **their room**, and **their presses**.
2. **The room's creatures stand still because of "busy and held", not because of a fresh event 102.** Each mover/creature
   step asks the busy word (0x80155738) and then the node's +28 bit 8 (0x801F68D4); both non-zero means the step is posed,
   not advanced (0x800074A0-0x800074B4, 0x800271F0-0x8002721C) [read]. In M3 the zombies of room 183 have hold 1 on the
   first row after the ticket, and **keep hold 1 after the change while they walk** (zombie 41, rows 3887-3944: hold 1,
   step 3.5-15.9) [read]. Bit 8 is never cleared by the busy going to 0. It is a stale mark that bites at the next busy.
   - **So the apart case is very likely held too.** A ticket apart sets busy 4 in the one busy word. The stayer's room's
     creatures carry bit 8 from any earlier busy set in that room (an examine, a pickup, an earlier door together), so they
     answer "held" through the mover's whole countdown, although 102 never reaches their room [inferred, strong; not
     measured: M5 has no `[change-hold]` rows]. This is the user's literal example: "player 1 fighting in one room,
     player 2 enters another, player 1's screen pauses". The design (§0, §2 A1/A2, §5 F5 "Apart: ... nothing changes")
     assumes the opposite. **F5 has to be written per room, not per split.**
   - Room 180's zombie 33 also showed hold 1 in M3 before player 1 arrived there [read]: the marks persist in every room.
3. **The stayer's own child node is held too.** In M3, node 60 (type 91, at player 2's exact place, a child that
   0x801F6828 recurses to through 0x80151500) has hold 1 for the whole countdown, while the held ask frees only node 59
   (player 2). The tail's let-go clears both (row 3887: node 60 hold 0) [read]. For player 1 as mover, node 9 (type 91, at
   player 1's place) was cleared by fn66's 0x801F6828(player, 0) [read]. What type 91 is (a weapon, a light, a shadow)
   was not read.
4. **The stayer's presses wait, by host rules written for follow mode.** Both stayers: no zone look (so no prompt and
   no B), no quick spell. Player 2: no Start. Player 1: Start is refused by the game itself (its held control channels).
   Every one of these keys on `coop_room_change_other`, which does not look at rooms [read]. Each has a reason that
   still holds in part with the stay (one ticket record, a scene over a load), listed in §2.7-2.9.
5. **The host's R4b/R4c protections exist for one direction only.** `stay_waits_clear` / `stay_busy_clear` act only inside
   player 2's bracket (`g_stay_bracket != 0`, hle.c:25037, 25062). Player 1's scene record is cut only by tickets that
   move player 1 (`g_coop_p1_tickets`, hle.c:11364-11365, 17138). Player 2's scene records are cut by **any** ticket
   (`g_coop_tickets`, hle.c:17170-17174, 17181-17185) [read]. So player 2 as the stayer in their own scene loses it to
   player 1's door: the mirror R4c lists as "not done".
6. **A grab on the stayer is let go by the mover's ticket**, on purpose, by the follow-mode rule in `coop_grab_events`
   (hle.c:22288-22305: passed through when busy has bit 4) [read]. F6 is right: under `coop_stay()` it no longer has a
   reason.
7. **The mover's own door motion has no host code in the stayer's paths.** fn66 clears the mover's bit (0x801F6828 from
   0x801667A4), `coop_door_motion` clears player 2's for a door of theirs (hle.c:21767-21772), and the arrival re-holds the
   current player (0x801F6828(r13-21492, 1) at lr 0x80152194, M3 row 1616) [read]. A change for the stayer can leave all of
   these alone, if it **never answers for the asker's node** and **never writes bit 8**.

## 1. One split countdown, frame by frame (M3: player 1 leaves 183 for 180, player 2 stays)

| Frame | What | Source |
|---|---|---|
| 3700, 3740, 3770 | player 2's busy sets (busy 2: their earlier presses) send 102 to room 183, player 1 told | page-0001.log 253, 448, 600 [read] |
| 3821 | script 119 asks for room 180; `notify_room_ticket` marks the mover (player 1), `g_fade_mover = 1`, `g_coop_p1_tickets++` | 850-852; hle.c:11350-11383 [read] |
| 3821 | busy set 0x801556CC(4, 1) sends 102 from player 1 to room 183: 19 nodes, player 2 told | 854 [read] |
| 3821 | the held ask for player 2 answered "not held" (lr 0x80027778, the holder step) | 864 [read] |
| 3822-3881 | busy 0x4. Every type-4 zombie and node 60 (player 2's type-91 child) hold 1, step 0.0. Player 2 hold 1 but not held by the ask. Player 1 (node 1) hold 0, state 121, walks off | rows 866-1575 [read] |
| 3852 | player 2's B (both.tsv p2b 0x200): nothing happens, no log line | both.tsv [read]; hle.c:20978-20982 [read] |
| 3882-3883 | the load's strip and the arrival's walk pass over player 2's node; the arrival re-holds player 1 (lr 0x80152194); the tail | 1606-1618 [read] |
| 3883 | the host's own 0x801F6828(player 2, 0) (lr 0x0BADF010): `player 2's hold from the ticket let go` | 1620-1621; hle.c:14529-14538 [read] |
| 3887 on | busy 0. Zombies 40/41 move (hold still 1), targets switch to 59 (player 2) | rows 1688-2258 [read] |

Player 2 had no stick input in M3 (p2sx/p2sy 0 throughout), so the run does not show their walk; the earlier split walk
runs do. M2 (player 2 leaves) has no node rows; player 1 had no input either [read both.tsv].

## 2. Every host path that acts on the stayer between the ticket and the end of the change

### 2.1 `coop_node_held_ask` (hle.c:19721-19746), the held ask 0x801F68D4

- **Today.** It runs only while `coop_room_change_on` (busy has bit 4 and nothing but 4|2|1, the ticket counting, no
  cutscene, no menu: hle.c:11334-11341). It answers 0 only for a **player** node (player 2's by pointer, player 1's by
  `g_coop_p1_id`) that is not the asker (`g_coop_room_p2 == p2` returns to the game). Every other node gets the game's
  answer: bit 8 [read].
- **Callers (12)** [read `ed fn`]: the player's step 0x800074AC; the motion steps 0x80027200, 0x800272A8; the holder step
  0x80027774, 0x80027804; 0x8002C7E4; 0x8006705C; 0x80117E04; 0x80144688 and 0x80144924; and the delayed-message queue
  0x801F5438 and 0x801F55D4, which push back a held node's delayed messages while busy (0x801F541C-0x801F5460) [read].
- **Player 1 as stayer** (player 2's door): answered 0 (M2 line 515). **Player 2 as stayer**: answered 0 (M1 767, M3 864).
- **Not covered:** the stayer's children (node 60 in M3) and every creature, door, spawner and object of the stayer's
  room [read rows].
- **What would have to change (F5, per room).** Under `g_settings.mod_coop && coop_stay() && room_slots() == 2 &&
  g_coop_p2 && coop_room_change_on(cpu)`, answer 0 for any node whose object room (+52 -> +592) is the **stayer's room**,
  taken from the stayer's own node (`r6_player1_node` / `g_coop_p2`), never from r13-23276. Keep returning to the game
  for the asker's own node (the existing `g_coop_room_p2 == p2` line), so the mover's door motion is exactly as today.
  - In a split, the stayer's room is the asker's room: the whole room plays on, the mover left to the game.
  - Apart, the stayer's room is not the asker's: it frees the stale marks there (item 0.2), and the asker's room stays
    held as in solo.
  - On a rejoin (into the stayer's room), the stayer's room is the room asked for: it plays on.
  - It must stay an answer, never a write: each later busy set sends 102 again (the comment at hle.c:19723-19725 already
    says so), and the tail's own 0x801F6828 call is the one write.
  - It must return at once under `g_tome_copy_turn`, like every hook in the design.
  - With bit 8, 16 or 32 in busy (someone's words, find or scene on top), `coop_room_change_on` is false and this answers
    nothing: the game's hold, as today. That is right while those bits mean "the game holds play"; see 2.13.

### 2.2 The tail's let-go (`notify_room_slots_tail`, hle.c:14516-14541)

- **Today.** At 0x80154B54 with lr 0x80152434 (after the tail), when the stayer's +28 has bit 8, it calls
  0x801F6828(stayer, 0) through `hle_guest_call`, which recurses to the stayer's children (M3 row 1620, lr 0x0BADF010)
  [read]. The stayer is `coop_stay_stayer` (hle.c:14011): player 2 outside the bracket, player 1's node (`g_stay_bracket_was[1]`)
  inside it [read]. The tail runs inside 0x80152298, so for player 2's door it runs **inside** the bracket (the runner
  0x80152614 calls the load), and the stayer is player 1 then [read the call order, change-freeze-code-readers.md §2].
- **Why it is still needed:** after the change, busy is 0 and the bit no longer holds anyone. But bit 8 would hold the
  stayer at the next busy that `coop_node_held_ask` does not answer (anything but a ticket) [inferred from 0.2].
- **Both directions:** the same code, by `coop_stay_stayer` [read].
- **What would have to change:** nothing for the stayer. With F5 answering per room during tickets, the creatures need no
  write at the tail. The stale marks outside tickets are a separate question (§4, Q4).

### 2.3 `coop_hold_open` / `coop_hold_shut` and player 1's turn (hle.c:19776-19801, 20322, 20345-20365)

- **Player 1 as stayer** (player 2's door): `hle_coop_controller` takes `room_turn = !p1_look && !p2_hold &&
  coop_room_change_other(cpu, false)` (hle.c:20322). After player 2's turn it opens the hold (the three hold channels'
  counts, busy word 0, r13-25696 0, r13-23040 1, r13-24048 & ~4), runs `coop_player1_turn` from their last free state
  (`g_coop_p1_free`, saved on a non-busy frame at hle.c:20277-20282; the doorhold fix, coop-failed-attempts.md:543), and
  shuts it [read]. Inside that turn the game sees "not busy": player 1's pad and controller run, and they walk.
  - Outside the turn the frame sees busy 4: the world's in-play channel (0x80006A94 at 0x800084F4), the prompt chooser
    0x80117628 (0x800084F8) and the holder steps all run in the main frame with the hold in [read 0x800084F0-0x800084FC].
- **Player 2 as stayer** (player 1's door): no hold is opened. Their turn runs through `room_other =
  coop_room_change_other(cpu, true)` (hle.c:20849-20850), past the busy refusal, with their own input copy (they never
  had the busy-4 hold in their channels) [read]. They walk (row 69 of coop-no-pause.md; the r6c walk runs).
- **What would have to change:** nothing for walking. These are the two places a stayer's presses would be let back in
  (2.7-2.9).

### 2.4 The creatures and the stale mark: split and apart

- **The mark.** The busy set 0x801556CC sends 102 only when the new word is non-zero, from the current player's id to
  that player's room (0x801556F0-0x80155720) [read]. The 0x801F5620 loop skips the sender and any node of another room
  (0x801F5654-0x801F5674) [read]. Nothing in the clear path (`andc`, word 0) sends anything [read 0x801556E0-0x801556F8].
- **The setter.** The P1.3 probe logged no 0x801F6828 call for any zombie at the 3821 ticket, though the window was open
  (the notify runs at 0x80155770's entry, before its busy set) [read]. So the 102 handlers set bit 8 by a path other than
  0x801F6828, or the bits were already set by player 2's busy sets at 3700-3770 [inferred: one or the other].
- **Apart.** The held ask needs only "busy != 0 and bit 8" [read]. So with the stayer's room's creatures marked from
  earlier, any ticket of the mover's (busy 4) poses them for the countdown [inferred, strong]. `coop_busy_ask`
  (hle.c:19704-19717) answers 0 only for player 2's screen/words, player 1's look, player 1's words or find under busy 1,
  and `coop_p2_holds` [read]. It answers nothing for a plain ticket.
- **Where the doors differ (to check in a run).** `coop_p2_holds` takes the room bit while player 2's own script hold is
  on (hle.c:19657), and `coop_p1_close_up` takes it while player 1's look holds (hle.c:19295). If a door's hold is still on
  at its ticket (CLIMB DOWN from a look of player 1's), the busy ask answers 0 for **every** caller, and the room is
  already not held during that countdown today [read code; not seen in a run]. The ladder L01/103 let go of bits 1 and 2
  before its ticket (arrival-readers.md §3), so it is a plain busy-4 countdown [read there].
- **What would have to change:** F5 per room (2.1). F2' (2.5) after it.

### 2.5 Who the creatures go after (`coop_enemy_target`, hle.c:25554-25612; `coop_send_event` event 59, 21339-21351)

- **Today.** `coop_enemy_target` runs from `coop_character_step` (hle.c:25616-25624): it is reached only for a creature
  that is stepped. A held creature is not, so in M3 the targets changed only after busy went to 0 (rows from 3887,
  "after 59") [read]. Its `held[]` takes only `coop_player_held` (words, find, close-up, Tome; hle.c:19664-19674) [read].
- **What would have to change (F2', as designed):** with F5, the engaged creatures keep their old target, the mover,
  who is walking into the door. `coop_player_leaving(cpu, k)` in `held[]` alone, split only (both players' object rooms
  equal), turns them to the stayer. As the design says, not through `coop_player_held`, whose event-59 use stops
  creatures from coming at all when player 1 is "held" (coop-failed-attempts.md:562) [read].
- **A hit already in motion.** The door state 121's handler (0x8005AE90) returns 1 for events 52, 32, 107, 153, 44, 105,
  125, 175, 40, 147, 43, and sends **every other event to 0x8005B964, which returns 0 and falls back to state 0's
  handler** [read arrival-readers.md §1]. So a hit (event 39, 0x80060188) on the mover during the countdown is handled
  by state 0, not passed over [inferred]. What state 0 does with 39 for a node in 121 (a hurt state that drops the door
  motion?) is unread. It must be read before F5 frees anything in the mover's room. A fallback is to pass event 39 and the
  damage events to the asker's node over as handled during a split ticket. The precedent is "a held player can't be
  hurt" (coop-no-pause.md row 76).

### 2.6 The stayer's child node (type 91)

- **Today:** held for the whole countdown (M3 node 60, hold 1, rows 866-1575), and freed with the stayer at the tail
  (row 1688: hold 0) [read]. The mover's child is freed by fn66's recursive clear [read node 9 rows].
- **What would have to change:** F5 per room covers it: it is in the stayer's room. Before that, find out what it is
  (a probe: node 60's +8 type, +40 actor and its model) and whether its update matters to what the stayer sees (a weapon's
  swing trail, a held lamp) [unknown].

### 2.7 The stayer's zone look, prompts and B

- **Player 1 as stayer:** `coop_scan_player1` returns "nothing come into" while `coop_room_change_other(cpu, false)` and
  the look is player 1's own (hle.c:20160-20168): "A way out or a scene of theirs would ask for a second room, or play,
  over the one on its way" [read]. With no look, no zone asks for a prompt. B reaches the game's prompt use in the main
  frame under busy 4 and finds nothing new [inferred].
- **Player 2 as stayer:** `coop_player_turn` sets `g_coop_prompt = -1` and skips `coop_prompt_look` while `room_other`
  (hle.c:20976-20982) [read]. Their B starts nothing: no log line in M3 at 3852 [read].
- **The one-ticket rule behind it.** Under `coop_stay`, a second fn66 while a ticket counts is passed over with r3 = 0:
  "the second waits for a new press" (`coop_script_go_to_room`, hle.c:16747-16756) [read]. A zone way out that came into
  during the countdown would mark itself inside, have its fn66 refused, and not fire again until the player leaves and
  comes back in. That is the "standing still in a held exit" open note of coop-no-pause.md row 66 [inferred].
- **What would have to change (F4 part):** with the stay, the look can run for the stayer in both directions, if:
  - a way out it comes into is **remembered, not dropped**: the round-5 mechanism `coop_way_out_wait` / `coop_way_out_go`
    (hle.c:16694-16745), with a release after the change. Its "gone with a room asked for since" test (`tickets`) must not
    drop it for the very ticket that made it wait;
  - a **scene** begun by the stayer's zone during the countdown survives the arrival. For player 2's door the bracket
    keeps player 1's waits and bits (R4b). For player 1's door nothing does (2.11), so player 2's scene would lose its
    waits and its busy bits at player 1's arrival [inferred from 25035-25073's gate];
  - a **close-up**: player 2's own close-up lives in their own set, with nothing in the busy word (hle.c:13094-13121)
    [read], so the arrival's prompt reset (0x801172B0) and busy clear do not reach it [inferred]. Player 1's close-up
    under player 2's door is kept by `stay_busy_clear` (bits but 4, when `coop_p1_in_close_up`) [read 25060-25073]. Player
    1's close-up beside player 1's own door does not arise (they are the mover).
  - Apart, the same rules apply. The one ticket record is the only shared thing.

### 2.8 Quick spells (`coop_quick_cast_ask`, hle.c:19748-19767; `coop_quick_cast(cpu, wait)`, hle.c:12463, called at 21067-21071)

- **Player 1 as stayer:** the game's handler 0x800069DC, reached from the in-play channel (0x800084F4) in the main frame,
  is refused while `coop_room_change_other(cpu, p2 = false)`: "player 1's quick spell waits" [read].
- **Player 2 as stayer:** `coop_quick_cast(cpu, room_other || coop_left_behind(cpu))` drops the press (`wait`; "a press
  while they wait is not kept") [read 12466-12468]. With the stay, `coop_left_behind` is false for a resident room
  (hle.c:16236-16239) [read], so only `room_other` refuses it.
- **The reason given:** "may ask for a scene or another room" (19751-19752) [read]. With one ticket record, a cast's fn66
  would be passed over (16748). Which quick spells ask for a ticket, a scene or a cinematic was not read [unknown].
- **What would have to change:** under `coop_stay()` with two slots, refuse only while the stayer's cast could reach the
  ticket or a scene. That needs the read of which spells do. A cast with neither (Enchant, Recover and the like:
  [inferred]) can go through. A press must not be lost silently: keep it (`coop_press_keep`) or refuse it with the game's
  own refusal sound.

### 2.9 Start

- **Player 2 as stayer:** two sites refuse their Start while `coop_room_change_other(cpu, true)`: the copy-ask
  (hle.c:26870) and `start_room` (27087-27096). The reason (follow mode): a copy opened in the fade "was taken with the room
  change in it and showed black in their half once the new room was in" (logs/run-20261106-081752) [read the comment].
  With the stay their half never changes room, so that reason is gone in a split. The copy is still a copy of all memory
  with the ticket counting in it. The copy's turn runs only the Tome, and the real world goes on and makes the change
  [inferred]. A run must show that the copy's close finds the stayer's room intact and the copy's draw is not of the
  fade.
- **Player 1 as stayer:** the game refuses it on its own: 0x80006CDC's gates (r13-25696, a live control channel...)
  [read verbs.md:324] see the busy-4 hold, because the in-play channel runs outside player 1's turn, where the hold is
  shut again. "An intercept on player 1's Start 0x80006CDC in player 2's room change was never reached"
  (coop-failed-attempts.md:560) [read].
- **What would have to change:** for player 2, drop the two refusals under `coop_stay()` with two slots, after a run.
  For player 1, opening the real Tome during the countdown means the change would fire under player 1's Tome (`g_tw.sharing`
  world ticks). That is a design of its own. Keep the game's refusal until then, and say so.

### 2.10 `coop_follow`'s same-room branch during the countdown (hle.c:21475-21533; caller 27823)

- **Today.** Player 1's character step calls `coop_follow(player 1)` each frame. While the two objects share a room (a
  split, before the stamp at the change), the same-room branch re-places player 2 beside player 1 when they "fell"
  (z more than 2000 below where they last stood still) or "stood inside player 1" for 60 frames, and guards only
  `!cutscene_playing` [read]. After the stamp, rooms differ and `coop_stay_apart` returns early (21534-21543) [read].
- **Risk in the countdown** [inferred]: in player 2's door the **mover** is player 2. A deep descent (a ladder's climb
  motion) or both standing still within 30 units for 60 of the ~62 frames would put the mover back beside the stayer
  mid-motion. In player 1's door, the stayer player 2 standing still inside the mover is the same case. Neither was seen.
- **What would have to change:** skip the same-room branch while `coop_stay() && coop_room_change_on(cpu)`. It is a
  follow-mode helper, and no one is brought along with the stay.

### 2.11 R4b/R4c: `stay_waits_clear`, `stay_busy_clear`, the scene records (hle.c:25035-25073, 17137-17185, 17411-17424)

- **Today.**
  - At player 2's arrival, inside the bracket: `stay_waits_clear` (on 0x80163F04 at lr 0x80152140) puts back the waits of
    scripts not player 2's.
  - `stay_busy_clear` (0x801556CC(-1, 0) at lr 0x80152188) keeps player 1's busy bits but 4 when player 1's scene or
    close-up is on.
  - `stay_scene_look_again` re-arms player 1's scene look (bits 32|2).
  - `coop_p1_scene_on` is cut only by `g_coop_p1_tickets` (tickets that move player 1) [read].
- **The mirror is missing** (player 1's door, player 2 the stayer in their own scene, begun in their look or a use of
  theirs):
  - no bracket, so both guards return (`g_stay_bracket == 0`);
  - player 1's arrival clears every script wait (0x80163F04(1) at 0x8015213C) and every busy bit (0x80152184);
  - `coop_p2_scene_on` / `coop_scene_p2s_on` are cut by any ticket (`g_coop_tickets`) [read].
  - So `coop_p2_holds`' scene term goes false at the ticket. In `coop_player_turn` their own scene's camera step
    (20853-20854) stops for the countdown, because the turn is refused (busy 32|4, `room_other` false with bit 32). At the
    arrival their scene's waits and hold go [inferred: the R4b fault, mirrored].
- **What would have to change:** the mirror of R4b/R4c for player 1's door:
  - a `g_coop_p2_tickets` (tickets that move player 2) to cut player 2's scene records under `coop_stay()`;
  - at player 1's arrival with player 2's scene on: their scripts' waits put back (`coop_p2_script_is`), and player 2's
    scene bits kept, as `stay_busy_clear` keeps player 1's;
  - player 2's scene look kept after (the R4c re-arm, for their set).
  - This is the "R4d" item named in coop-next-session.md and design-r4d.md. It is the prerequisite for letting the stayer
    begin scenes during the countdown (2.7).

### 2.12 A grab on the stayer (`coop_grab_events`, hle.c:22262-22305; F6)

- **Today.** Kind 19's handler lets go on any event 102 (0x80061148: event 38 to the grabber and to itself)
  [read the comment]. The host passes another player's 102 over as handled (answer 1), **except when busy has bit 4**: "Not
  for a room change of the other's (its ticket, busy 4): they are let go as the game lets go, and brought along
  (coop_follow)" [read 22294-22297]. So the mover's ticket frees a grabbed stayer in a split, in both directions:
  player 1 grabbed while player 2 leaves, and player 2 grabbed while player 1 leaves.
- **Apart:** the 102 never reaches the stayer's room, so no let-go. But the grabber, marked from before, is posed for the
  countdown by 0.2. The grab's ticks (hits every 8th frame) wait, and the stayer's own wiggle (event 44 from their
  controller in their turn) still counts [inferred].
- **What would have to change (F6):** under `coop_stay()` with two slots, drop the `(busy & COOP_BUSY_ROOM) == 0` exception
  when the sender is the other player. Follow mode keeps it. With F5 the grabber also goes on through the countdown,
  so the grab plays out as the game's.

### 2.13 The stayer in a close-up, words, a find, their Tome, a scene or a cutscene when the other's ticket comes

- **A walk-in door usually never writes its ticket then.** A zone's fn66 made in a co-op look while the OTHER player's
  scene, words, find, close-up or script hold is up is refused and remembered (`coop_way_out_p1_up` /
  `coop_way_out_p2_up`, hle.c:16671-16681; `coop_way_out_wait`). Its reason, "the words go with them to the new room"
  (coop-no-pause.md row 81), is a follow-mode reason [read]. With the stay, it is the **mover** who waits (stands in the
  door zone) for the stayer's words: not a stayer freeze, but it does not feel built for two [inferred]. A prompt door
  (B on OPEN DOOR) is not refused this way [read: `waits` needs a look, 16764-16766].
- **Player 1 in their words (game window, bit 8) or find (16), under player 2's door:**
  - `coop_room_change_on` is false while those bits are on (hle.c:11338-11339): no held-ask answer, no `room_turn`, no
    quick-cast refusal;
  - `coop_room_change_words` leaves player 1's words open at player 2's change (R4 return, hle.c:11259-11260) [read];
  - `stay_busy_clear` keeps their bits only for a scene or close-up, not plain words. Whether `coop_p1_in_close_up` covers
    a words-only window: unread [unknown].
- **Player 2 in their own words, close-up or find** (step F, own set): nothing of theirs is in the busy word
  (hle.c:13094-13121) [read]. Their turn runs on `room_other`. `coop_quick_cast` and Start already refuse under
  `coop_p2_own_up` [read]. At the change, their own set is host memory and the arrival does not reach it [inferred].
- **Player 2's Tome copy open at the ticket:** it stays whole through the change (logs/run-20261106-101418, cited at
  27088-27091) [read the comment]. `coop_player_held(1)` is true, so enemies leave them [read 19672].
- **Player 1's real Tome (`g_tw.sharing`) at player 2's ticket:** `game_menu_open` makes `coop_room_change_on` false
  [read]. The world ticks behind the Tome for player 2, and the change fires in a world tick. Not run with the stay
  [unknown].
- **A scene of the stayer's:** player 1's: R4b/R4c (bracket). Player 2's: the missing mirror (2.11).
- **A cutscene (play_cinematic):** `coop_room_change_on` false; the user's rule holds both players [read
  coop-no-pause.md row 74].

### 2.14 Dead or dying

- Nothing in hle.c handles a player's death in co-op [read: no death/game-over handling found by grep]. If the stayer's
  death asks for a room (the game over: room 69, script 1500, coop-no-pause.md row 75) during the countdown, R4's
  one-ticket rule would pass it over (16748) [inferred; whether the game over goes through fn66 or straight to 0x80152298
  was not read]. A direct 0x80152298 call is not refused by the host. During the countdown it would be the stacked change
  of the design's §4.5 [inferred]. To read before Phase 1b is called complete: the death path's room call and the frames
  around a stayer dying in the countdown.

### 2.15 The rest of what touches the stayer in the countdown (unchanged, listed so nothing is missed)

- The fade quad is left out of the stayer's half (`g_fade_mover`, `notify_GXLoadPosMtxImm`, hle.c:3344-3361) [read].
- The sound fade at the press and the change leaves the stayer's room (`coop_sound_change_act`, R5) [read; M3 log
  "room 183 kept (6 tracks)"].
- The load's strip and the arrival's walk pass over the stayer's node (`coop_stay_kind_ask`, `coop_stay_strip_walk`,
  hle.c:14574-14598) [read; M3 1606, 1614]. Their children and summoned things are not passed over by these two
  [inferred; what the strip removes beyond the body's parts was not read].
- `room_slots_return` swaps the slot indices so the stayer's slot is not evicted (hle.c:14545-14572) [read].
- R4's bracket for player 2's door (`notify_ticket_run`, `coop_stay_change_end`, hle.c:14047-14116) runs from the runner to
  the behaviour pass of the same frame, so no frame of the countdown is inside it [read].

## 3. What would have to change, together (Phase 1b), both directions, the mover untouched

Gate for all of it: `g_settings.mod_coop && g_coop_p2 && coop_stay() && room_slots() == 2`, each hook returning at once
under `g_tome_copy_turn`. Solo, together-follow and co-op without ED_COOP_STAY take none of it (`coop_stay()` is false;
rooms_follow_coop unsets the switches with co-op off, frontend.cpp:483-499, per the design §8).

| # | Change | Where | Split | Apart | Mover's door motion |
|---|---|---|---|---|---|
| S1 (F5, per room) | answer "not held" for every node in the stayer's room (by the stayer's own node), never the asker's node; an answer, not a write | `coop_node_held_ask` | the room plays on | the stale-marked creatures play on | asker left to the game (fn66's clear, `coop_door_motion`, the arrival's re-hold) |
| S2 (F2') | `coop_player_leaving` in `coop_enemy_target`'s `held[]` only, split only | hle.c:25605 | engaged creatures turn to the stayer | n/a | not chased |
| S3 | a hit on the asker in state 121 during a split ticket: read state 0's handling of event 39 first; if it breaks the motion, pass hits to the asker over as handled | `coop_send_event` | needed with S1 | n/a | protected |
| S4 (F6) | no let-go of a grabbed stayer by the other's ticket | `coop_grab_events` 22294 | kept grabbed | n/a (no 102) | none |
| S5 | `coop_follow`'s same-room branch skipped during a ticket | hle.c:21500 | no re-placing | n/a | not moved |
| S6 (R4d) | player 2's scene records cut only by tickets that move them; their waits and scene bits kept at player 1's arrival | 17170-17185, 25035-25073 | their scene survives | the same | none |
| S7 (F4) | the stayer's look, B, quick spells and player 2's Start under the stay, with a way out remembered (not dropped) and casts that reach a ticket or a scene still refused | 20160-20168, 20976-20982, 19754-19767, 21067-21071, 26870, 27093 | they keep playing | the same | the one ticket record is untouched |

Order: S1 with S2 and S3 (S1 alone sends freed creatures at the mover), S4, S5, then S6 before S7's scene part.

## 4. Questions only a run or a further read settles

- **Q1 (the apart hold; the user's example).** M5 again, with `ED_COOP_ENEMY_LOG=1 ED_CHANGE_HOLD=1`, player 1 among
  room 183's zombies (marked by an earlier busy set there) while player 2, apart, takes a door. Its rows include the other
  player's room (rooms[1], hle.c:25424-25427). Expected today: the zombies with hold 1 and step 0.0 through player 2's
  countdown, and no event-102 line to room 183. A scripted player 1 cannot ready (ED_INPUT_SCRIPT, design §3 M4), so ask
  the user for that one play, readied and swinging, in both cases.
- **Q2.** What state 0's handler does with event 39 and the damage events for a node in state 121 (0x80058DD8,
  0x8005B964). Read before S1.
- **Q3.** What node type 91 is (node 60 in M3), and whether its being held shows.
- **Q4.** Stale marks outside tickets: apart, the mover's other busy states (a find, words under busy 1, or a scene the
  busy ask does not answer) may pose the stayer's room the same way. The tail re-holds the mover at every arrival
  (lr 0x80152194), and player 1's node keeps that mark [read]. A probe: the `[change-hold]` rows keyed to any busy set
  while apart, not just tickets.
- **Q5.** A door whose hold is still on at its ticket (CLIMB DOWN in a look of player 1's): is the room already free
  through that countdown today (2.4)? The same rows on that door, each direction.
- **Q6.** Which quick spells ask for a ticket, a scene or a cinematic (for S7).
- **Q7.** The stayer's death during the countdown (2.14).
- **Q8.** Player 2's Tome copy opened mid-countdown with the stay (2.9): its draw and its close.

## 5. Facts read for this report

| Fact | Where | Mark |
|---|---|---|
| The busy set sends 102 only for a non-zero word, from the player's id to the player's room | 0x801556CC-0x80155734 | [read] |
| 0x801F5620 skips the sender and nodes of other rooms (-1 rooms told) | 0x801F5654-0x801F5674 | [read] |
| The player's step: busy != 0 and held means skip | 0x8000749C-0x800074B4 | [read] |
| 0x801F68D4's 12 callers, including the delayed-message queue's push-back | `ed fn 0x801F68D4`; 0x801F5410-0x801F5460, 0x801F55B8-0x801F55F4 | [read] |
| 0x801F6828 recurses to the node's children through 0x80151500/0x80150D8C | 0x801F6828-0x801F68A0 | [read] |
| Zombies keep hold 1 after busy goes to 0 and walk | M3 rows 1688-2258 | [read] |
| Player 2's type-91 child held through the countdown, freed at the tail | M3 rows 866-1575, 1688 | [read] |
| No 0x801F6828 call for any zombie at the ticket | M3, P1.3 lines: only 1616 and 1620 | [read] |
| The held ask frees only the non-asker's player node, only while `coop_room_change_on` | hle.c:19727-19746, 11334-11341 | [read] |
| Player 1 stayer's turn with the hold opened, from `g_coop_p1_free` | hle.c:20277-20282, 20322, 20345-20365 | [read] |
| Player 2 stayer's turn through `room_other`; zones and B off | hle.c:20849-20850, 20976-20982 | [read] |
| Player 1's zone look refused in player 2's change | hle.c:20160-20168 | [read] |
| Quick casts refused for the non-asker | hle.c:19754-19767, 21067-21071, 12463-12468 | [read] |
| Player 2's Start refused in player 1's change | hle.c:26870, 27087-27096 | [read] |
| A second fn66 passed over while a ticket counts | hle.c:16747-16756 | [read] |
| The tail's let-go of the stayer | hle.c:14529-14538; M3 1620-1621 | [read] |
| The arrival re-holds the current player | M3 1616 (lr 0x80152194) | [read] |
| R4b/R4c guards only inside player 2's bracket | hle.c:25035-25073 | [read] |
| Player 1's scene cut by `g_coop_p1_tickets`; player 2's by `g_coop_tickets` | hle.c:11364-11365, 17137-17147, 17170-17185 | [read] |
| The grab's 102 passed through when busy has bit 4 | hle.c:22288-22305 | [read] |
| `coop_follow`'s same-room branch has only a cutscene guard | hle.c:21500-21533 | [read] |
| A way out waits for the other's words, scene, find or close-up | hle.c:16671-16681, 16760-16769 | [read] |
| State 121 passes most events to state 0's handler | arrival-readers.md §1 (0x8005AE90, 0x8005B964) | [read there] |
| `coop_enemy_target` runs only from a creature's step | hle.c:25616-25624 | [read] |
| Player 2's own close-up puts nothing in the busy word | hle.c:13094-13121 | [read] |
| The stayer walks at full rate through a split countdown | change-freeze-measure-readers.md (r6c-walk3, r4b-c7) | [read there] |
