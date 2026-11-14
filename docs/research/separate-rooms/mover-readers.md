# Reader: the mover in their door motion, if the room's creatures are not held (separate rooms, the change freeze's F5)

Reader for design-change-freeze.md section 5 ("Before building, read: what a creature's hit does to a player in their door
motion"). The question: in a split (both players in one room, one takes a door, the other stays), if the shared room's
creatures are NOT held through the ~62-tick countdown, what can they do to the player walking through the door, and what
happens to a creature that was after the mover once the mover has left. Both directions (player 1 the mover, player 2 the
mover). Read only: the disassembly (`build/ed-db/disasm.txt`, `ed dis`, `ed fn`), `runtime/host/hle.c` (working tree), the
docs named, and the logs named. No code changed, nothing built, the game not run.
[read] = seen at the address, hle.c line or log line given. [inferred] = reasoned, not seen.

## 0. In short

1. **The game protects the player in a door countdown with five layers, and holding the creatures is only the first.**
   Layer 2 is the strong one: while the busy word r13-23344 is non-zero, the player's event handler (0x80058DD8, kind 2)
   offers every event to 0x800CA964 first. That function swallows events 8 (death), 11 (hit), 32 (grab), 39 (damage),
   53 (hurt), 59 ("are you a target?"), 202 (FINISH HIM) and others, and writes no answer [read 0x80058E80-0x80058EB4,
   0x800CA964-0x800CAB08]. So the mover cannot be targeted, hit, hurt, damaged, killed or grabbed while busy, whatever
   the creatures do. The table is in section 2.
2. **So F5 (creatures not held) is safe for the mover. A swing already in motion does not land.** The 11 is swallowed and
   the door motion, state 121 and the ticket go on. The ticket tick 0x80155830 reads only the ticket record, and no hit
   code writes it [read: the only references to 0x8058AFFC are 0x80154D1C, 0x80155754, 0x801557A4, 0x8015581C and the
   tick]. So the design's worry in section 5 is answered: nothing reaches the mover as long as layer 2 holds for them.
3. **The same layer also covers the stayer, and that changes F5.** The busy word is one word. It is not per player:
   coop_events does not swap it, and CoopPlayerState does not hold it [read hle.c:12000-12007, 21986-22055]. So during
   the countdown the stayer's node also swallows 11, 39, 53, 8, 32 and 59. With F5 alone:
   - the freed creatures cannot hurt the stayer. They swing through them for about a second;
   - the creatures lose both players. The quarry keeps or takes a player only if that player answers 59 with 1, and a
     busy player answers 0 [read 0x800327F4-0x80032820, 0x80032A68-0x80032AB4, sweep 0x80036780-0x80036790]. So each
     creature drops its target at its next quarry run (every 8, 16, 32 or 64 frames: enemy-readers-2.md §2) and gives up
     the chase;
   - F2' (coop_enemy_target pointing creatures at the stayer) is undone at that same quarry run.

   For "the fight goes on, as if built for two", the stayer's node needs the game's own "not busy" answer at the player
   handler's busy ask (lr 0x80058E84, node in r31), in a split ticket only. The mover keeps the game's answer (section 8).
4. **What a hit would do if it got through.** This happens in rooms 208 and 64 (the filter's own exceptions), and
   whenever coop_busy_ask answers "not busy" during a ticket (section 6).
   - State 121 does not handle 11, 39 or 53, so they fall to state 0. Its 53 runs 0x800637B8: the hurt move replaces the
     door motion, and the state becomes 7.
   - The ticket still counts and fires. The runner places "the player" on the entry marker, which erases any knockback.
   - The arrival's event 238 finds no door motion playing, so the mover arrives with no motion, at the marker's heading.
   - A death (event 8, sent inside the damage) would carry a dying player into the new room [inferred].
   - The game's own solo never allows any of this.
5. **A grab of the mover is refused twice.** State 121 answers 32 itself, with no answer written [read 0x8005AF1C-0x8005AF28].
   Busy swallows 32 as well. The grab start needs a non-zero answer (0x80039A9C-0x80039AA4), else it stops its own move
   (0x80039AF4) [read]. For transitions with no motion (0-3, 16, 17) only the busy layer refuses it.
6. **After the firing, a creature that was after the mover loses them three ways, all already in place.**
   - The host's D4 (r6_enemy_apart) drops a target in another room at the creature's next character step.
   - The game's own quarry clears a target whose room is not the creature's own.
   - A stale swing across rooms is refused by R6's room bracket (r6_hit_room) and the game's gate 0x800625A4.

   Seen in logs/run-20261112-180114-freeze-m3-p1-leaves-fight: zombie 41 "dropped player 1 (room 180)" at 3884, and
   zombies 40 and 39 went from "after 1" to "after 59" (the stayer) by 3884-3890 [read page-0001.log 1634, 1662, 1786].
7. **Both directions are the same at the mover, because the busy word is the same word.** For player 2's door, the ticket
   is written in their use, and the 102 goes out from their id. Their events, inside coop_events or as the game's, run
   0x80058DD8 with the shared busy word 4 in. The R4 bracket only covers the firing frame, and it closes at the
   behaviour pass's entry (lr 0x80008400), before any creature runs [read hle.c:14047-14110, 28593-28601].
8. **A new candidate for the apart -> apart "brief second" [inferred, to measure].** Apart, the stayer's room gets no 102,
   so its creatures are not held. But the busy word is 4 for the whole of the other's countdown, so the stayer's node
   swallows 59 and 11. A stayer fighting in their own room while the other changes room would see the creatures lose
   them (drop the target at their next quarry run) and their swings whiff for about a second. Their own FINISH HIM (202)
   and the other events in the list are swallowed too. This is the user's literal example (player 1 fighting, player 2
   changing room elsewhere). No log measures it: the one `[change-hold]` run with zombies is a split. The check is in
   section 10.

## 1. The mover in their door motion (what the game has)

- **fn66 (0x801666A4), in this order** [read 0x80166788-0x80166960]:
  1. The gate 0x800440D0. It tests only the player's kind-32 linked thing (doors-readers.md); 0 means no ticket.
  2. 0x801F6828(0x801F62B0() = the player's node, 0): the mover's hold bit 8 is cleared.
  3. The transition table at 0x80253F90 (r25 = the transition) picks the motion and the fade. Transitions 7-9 use motion
     25, 10-12 use 103, 13-15 use 104, and 4-6 use motion 2. 0-3, 16 and 17 have no motion (the `ed transition` table
     agrees).
  4. 0x801669AC(motion): event 253 to the player.
  5. 0x80155770, the ticket, with a count of fade+2 (62 for 9/12/15).
  6. The fade 0x801E7C88.
- **The ticket writer 0x80155770** [read 0x80155770-0x80155814]:
  - it stores the record at 0x8058AFFC, and sets r13-23392 := 1 and 0x80117260(0x10000, 1);
  - **0x801556CC(4, 1)**: busy |= 4, and event 102 to the rest of the asker's room;
  - **0x800443E4(count+3)**: the move's input lock. It writes r13-26232 through 0x800068E0 and r13-23592 through
    0x8013E99C (docs/game/systems/words.md:34);
  - **0x801C5118(0)**: a cast end (hle.c kCastEnds, 28245).
- **Event 253 -> state 121** (transitions.md, "The motion on the player"):
  - the motion plays with an end callback (event 6) and a replacement callback (event 7), both 0x801F8FA4;
  - state 121 is left by event 6 (state 1) or event 7 (state 1 if 0x80032AD0 answers 0) [read 0x8005AE98-0x8005AF08];
  - state 121 returns 1 itself, with no answer written, for 52, **32**, 107, 153, 44, 105, 125, 175, 40, 147 and 43
    [read 0x8005AF0C-0x8005AFA4]. **Every other event falls back to state 0**, including 11, 39 and 53.
- **The motion steps** (0x800271F0, 0x800272A8, 0x80027774, 0x80027804) skip a node only when busy AND its +28 & 8 are
  set (arrival-readers.md §1). The mover's bit was cleared at step 2. Player 2's bit is also cleared by coop_door_motion
  (hle.c, `go_on`) when the press was theirs [read].
- **Lengths**: motion 25 is 104-106 updates, 103 is 80-82 and 104 is 60-82, at 2 updates a frame (transitions.md, "How
  long each motion runs"). In the M3 run, player 1 was still in state 121 at +60, and their node's position did not change
  until +60 (1305,1508 -> 1278,1554) [read m3 page-0001.log 865-1590]. In a ladder (stepE-r6b-p1words-door, 2223) the node
  moved every frame [read].
- **The ticket tick 0x80155830** [read 0x80155830-0x80155924]:
  - +0 -= 1, and +4's delayed fade;
  - at 0: kind 17 -> 0x80152298 and event 62; otherwise the runner 0x80152614; then the callback at +36.
  - **It reads no player, no state and no health.** Nothing cancels a ticket but 0x80155740's zeroing, called from
    0x801540AC, and the next fn66 (one record).
- **At the firing**:
  - the runner places "the player" on the entry marker (0x80155348 / 0x80151914 / 0x80130EF0);
  - event 238 asks which motion to finish, by the motion now playing: 25 gives 25, 103 gives 104, 104 gives 103, anything
    else gives none (transitions.md, "Arrival" step 6);
  - the arrival clears busy for every bit (0x801556CC(-1, 0) at 0x80152184) and re-holds the player (0x80152194; M3 line
    1616) [read].

## 2. The five layers that keep a player safe in a countdown (solo)

| # | Layer | Where | What it stops | Whose node |
|---|---|---|---|---|
| 1 | The creatures are held: 102 sets +28 & 8, and while busy a held node gets no event 3/4 tick (the holder step 0x800276C4 -> 0x80027774/0x80027784), no motion step (0x800271F0..0x80027804), and its delayed messages wait (the pump 0x801F53E8: 0x801F541C busy, 0x801F5438 held, its delay pushed at 0x801F5450-0x801F545C) | behaviour-readers.md:25 [read there]; 0x801F5410-0x801F5460 [read] | everything a creature would do: look, chase, swing, grab, a delayed hit (55, posted at 0x80063510) | every node of the asker's room but the asker |
| 2 | **The player's busy filter**: 0x80058E80 asks 0x80155738 (busy). If it is non-zero: 0x800C3D28(id, msg), which for a 39 with block flag 0x2 and a positive amount sends 201, then 0x800CA964(node, msg, answer*). A non-zero result returns 1 at once, with the answer left 0 | 0x80058E80-0x80058EB4, 0x800C3D28-0x800C3DC4, 0x800CA964-0x800CAB08 [read] | swallowed: 10, 24, 25, 30, 32, 50, 53, 55, 57, 60, 63, 67, 101, 103, 107, 125, 133, 202 always, and 8, 11, 39, 59 unless room 208 (from an attacker whose 0x80033968 is 40) or room 64 (59 with data 1) [read: the decode below]. Not swallowed: 6, 7, 61, 62, 191, 238, 253 (transitions.md) | **every kind-2 player node: the mover AND the stayer** |
| 3 | State 121's own answers: 32 and ten others return 1 with no answer | 0x8005AF0C-0x8005AFA4 [read] | a grab of the mover even when not busy | the mover only |
| 4 | The grab needs an answer: the kind-6 attack sends 32, and on answer 0 stops its own move | 0x80039A88-0x80039AF8 [read] | the grab start | any victim that answers 0 |
| 5 | Target candidacy needs 59 == 1: the quarry's locked target (i), its walk (ii), its old target (iii), and the swing's sweep | 0x80032800-0x80032820, 0x80032A80-0x80032AB4, 0x80036770-0x80036790 [read] | a busy player is never a candidate, and a kept target is cleared (0x80032AAC-0x80032AB4: stats+20 := 0) | any victim that answers 0 |

The decode of 0x800CA964 [read]:
- 57, 32, 10, 30, 24, 25, 53, 50, 55, 103, 63, 60, 101, 67, 133, 125, 107 and 202 go to `li r3,1` (0x800CAAE8);
- 11, 8, 39 and 59 go to 0x800CAA94. There, room 208 returns 0 only for a 0x80033968 == 40 attacker; room 64 returns 0
  only for 59 with r3 == 1. Otherwise they go to 0x800CAAE8;
- the rest go to 0x800CAAF0 (`li r3,0`).

The answer is the word at 12(r1) of 0x801F5000. It starts at 0 (0x801F5024-0x801F502C) and is returned in r4
(0x801F5058). The senders test it with `and r0,r4,-1; cmplwi r0,1` [read]. So a swallowed 59 answers 0.

The first test of a swing does not ask 59 (0x80036094's quarry test: distance and facing only, enemy-readers-2.md §2). So
a creature can still send 11 to a busy player it is already after. Layer 2 then swallows it.

## 3. If the shared room's creatures are not held (F5): what they can do to the mover

Layer 1 goes. Layers 2-5 stay, for both players' nodes.

| What | Can it reach the mover? | Why |
|---|---|---|
| Target the mover (the quarry's walk) | no | candidacy needs 59 == 1; busy answers 0 [read §2] |
| Keep chasing the mover it was already after | until its next quarry run | between runs the chase reads stats+20 raw, with no room or 59 test (0x8003AE14-0x8003AE34, enemy-readers-2.md §2): it may walk up and swing. At the run, (i)/(iii) need 59 == 1 and clear it [read 0x80032A64-0x80032AB8] |
| coop_enemy_target choosing the mover | yes, today's host would | in a split both players' +592 equal the creature's, so it runs its nearest-player choice (hle.c:25554-25615). Today, a stayer held by the host (words, close-up, Tome: coop_player_held) makes it choose the other one, the mover (25605-25608) [read]. The pick is harmless (layer 2), but the creature swings at the door |
| Hit the mover (11, then 53 and 39 nested) | no | layer 2 swallows the 11 at the mover's handler. 0x8006255C never runs, so neither 53 nor 39 is sent [read] |
| A swing already in motion as F5 frees it | no | the same 11 |
| A delayed hit (55, from a hit that landed before the ticket) | no | the pump delivers it (the mover is not held), and layer 2 swallows 55 [read] |
| Damage from elsewhere sent as 39 (a seen sanity hit, a hazard) | no | layer 2 swallows 39. The game's own seen hit 0x800CA370 asks 0x80155738 == 0 (enemy-readers-2.md §3), and the host's coop_seen_later asks coop_busy (hle.c:22455-22470) [read] |
| Grab the mover | no | layers 2, 3 and 4 |
| Be swept as an extra victim by a creature's area swing | no | the sweep's 59 [read 0x80036780] |
| Push the mover's body (character collision) | possibly, by a few units | creatures walking again are swept against the characters of the room in play (0x8013085C, enemy-readers.md hazards). The placement at the firing puts the mover on the marker anyway (0x80130EF0) [inferred] |
| Cancel or delay the ticket | no | the tick reads only its own record [read §1] |

**Transitions with no motion** (0, 1, 2, 3, 16, 17): the mover stands in their own state with input locked (0x800443E4).
Layers 2, 4 and 5 still refuse every hit, grab and target. Only the state-121 answers (layer 3) are missing.

**Ladders** (12: motion 103, 15: motion 104), and doors taken from a look (EXAMINE, words, Yes, fn66): the same ticket and
state 121, with one difference, in section 6. A look's hold (busy 2) beside the ticket's 4 can change what the host's busy
ask answers.

**Transitions inside a scene** (busy 32 with 4): layer 2 still holds, since the word is non-zero. coop_room_change_on is
false then (bit 32), so the host's current split rules (coop_node_held_ask) do not apply (arrival-readers.md, R4b).

**Chapter pages** (ed87): not a fn66 ticket. Not read here.

## 4. What a hit does to a player in a door motion, when one gets through

This is the record for any change that lifts layer 2 for the mover, and for the exceptions in section 6. Read:
- 11 in state 121 is not handled, so it falls to state 0's case (0x80059C44-0x80059C68) -> 0x8006255C(block).
  - Its gate compares the victim's room to r13-23276 (0x80062594-0x800625B8). In a split, the shared room is the room in
    play for both directions: player 2's bracket is not on until the firing. So the gate passes, and R6's r6_hit_room
    passes through, since r6_apart is false while the players are together (hle.c:24468-24473, 24869-24873).
  - It sends 53 (0x8006367C) and 39 (0x80063764), synchronously.
- **53 -> 0x800637B8** [read 0x800637B8-0x800638E8]:
  - the room gate again (0x8006383C);
  - the hurt move from the hit block (0x8019B724), played by 0x80122920 with flags 48 (|4 for a block flag);
  - callbacks 0x80122088/0x8012206C, and **state := 7** (0x800638B0-0x800638C0).
  - Playing a new move replaces the door motion. Its replacement callback sends event 7 to the node, which is now in
    state 7, not 121 [inferred from the callbacks registered at 253, transitions.md].
- **39 -> 0x8005A604 -> 0x80061B9C -> 0x80061E90**: health. At 0 or below, event 8 (the death) is sent inside the call
  (enemy-readers-2.md "enemy-life" §0).
- **The ticket goes on and fires** [read §1].
  - The runner places the player on the entry marker, so the knockback is undone.
  - 238 asks the motion now playing, the hurt move, and gets "none". So there is no arrival motion, at the marker's
    heading (transitions.md step 9).
  - The player stays in state 7 until its move ends, then state 1. Their input lock (count+3) runs out at about the
    arrival.
  - Dead: the death state and its move are carried into the new room, and what the game-over flow does in a room just
    changed was not read [inferred].
- For player 2 as the mover, the same, through coop_events. At the firing their R4 bracket places them (they are "the
  player" in it).

**So layer 2 must stay on for the mover in any design.** Solo never lets a door walker be hit. Lifting it for them would
let a hit replace their door motion and send them through the door hurt, with no arrival motion.

## 5. Both directions: whose busy word, whose handler

- **The busy word r13-23344 is one word.**
  - coop_events_words swaps only r13-24048's bit 8 (COOP_OWN_BUSY_BITS) and the CoopPlayerState blocks (input, control,
    aim, the controller's globals, the pad's globals, the interact words). There is no 23344 among them [read
    hle.c:11998-12007, 21867-21895].
  - coop_as_player2 swaps only the three who words [read 21726-21746].
- **Player 1 the mover, player 2 the stayer.**
  - The ticket comes from player 1's script in the frame: 102 from id 1 to the room (M3 line 854: "player 1 not, player
    2 told").
  - coop_node_held_ask answers "not held" for player 2 (hle.c:19727-19749; M3: "player 2 goes on through player 1's room
    change: not held (lr 0x80027778)").
  - Both nodes see busy 4 at 0x80058E80, through the game's own call for player 1 and coop_events' hle_guest_call for
    player 2 (hle.c:22035). So layer 2 covers both.
- **Player 2 the mover, player 1 the stayer.**
  - The ticket is written in player 2's use with their who words in (M2 line 506: `busy r3 4 r4 1 ... who 59`).
  - 102 goes out from id 59. Player 1 is told and set free by coop_node_held_ask (M2 line 510, lr 0x80027204).
  - Player 2's door motion is theirs (coop_door_motion: "player 2 opened it: motion 25 is theirs", M2 line 501). Their
    events run through coop_events, or as the game's in their use when coop_p2_input_in_game. Either way it is
    0x80058DD8 with the shared busy word, so layer 2 covers them.
  - The R4 bracket (notify_ticket_run at the runner, coop_stay_change_end on 0x8014FDB8's entry at lr 0x80008400) opens
    only in the firing frame. It closes before the behaviour pass runs any creature [read hle.c:14047-14110, 28593-28601].
- **The stayer's own turn**:
  - player 1 during player 2's countdown gets room_turn, with coop_hold_open: the busy word is 0 inside player 1's
    controller only (hle.c:20338-20353);
  - creatures' swings are sent from their holder steps and key callbacks, outside that turn, so layer 2 applies to them
    there;
  - an event sent synchronously inside player 1's turn would see busy 0 [inferred: none from a creature's attack found].

## 6. When today's host answers "not busy" during a ticket (the mover exposed today)

coop_busy_ask is an intercept on 0x80155738 itself, so it answers every caller (hle.c:19704-19720; installed at 28389-28390):
the node update, the holder step, the motion steps, the pump, **and layer 2 at lr 0x80058E84**. That last one is seen in
stepE-r6b-p1words-door line ~2718: `player 1's look over ... lr 0x80058E84`. When it answers 0, layers 1 and 2 are off for
everyone: the creatures are not held, and no player's events are filtered.

It answers 0 when any of these hold:
- `g_coop_screen && (g_coop_words_theirs || busy)`: player 2's close-up has the screen;
- `coop_p1_close_up`: player 1's look. With player 2's room change on, its busy test also allows bit 4 (hle.c:
  `room_bit`, coop_p1_close_up);
- `p1_free`: player 1's words or find with no hold, and busy exactly 1;
- `coop_p2_holds`: player 2's script hold or own scene, with busy 2 (plus 4 when their own change is on).

So, read from the code:
- **player 1 in their own close-up while player 2's ticket counts, or player 2's script hold through their own ticket
  (busy 2|4)**: the whole world is free, and the mover can be hit in their door motion (section 4) [inferred: the
  conditions read, no run measured];
- **player 1's door from their own look** (fn66 inside a look, busy 2 then 4): seen to end at once. In
  stepE-r6b-p1words-door, busy went 6 -> 4 in the ticket frame and the look was "over" (lines ~2715-2720) [read]. A
  ladder or look whose hold (busy 2) lasts into the countdown was not seen;
- across the 2026-11-10..12 logs, the busy values seen in `[trace] room move` rows (a ticket counting) were 4, 5, 6 (once),
  12, 32, 34, 36 and 38 [read: a grep of every page]. No `[hurt]` line from a creature falls inside any ticket's 62 frames
  in any run from 2026-11-10 to 2026-11-12 [read: a scan of every page, ticket frame to +64]. So nothing so far shows a
  mover being hit.

## 7. After the firing: a creature that was after the mover

| Step | What | Mark |
|---|---|---|
| The stamp | 0x80154C94 writes the mover's object +592 := the new room (0x80154D2C-0x80154D30, on r13-26524's object: player 1's, or player 2's inside the bracket) | [read] |
| Apart now | r6_apart is true once the bracket is closed (hle.c:24468-24473) | [read] |
| The host's D4 | at the creature's next character step, coop_enemy_target -> r6_enemy_apart: its target is a player of another room, so +20 := 0 (hle.c:25145-25185). M3: `[enemy] 41 (room 183) dropped player 1 (room 180) (frame 3884)` | [read] |
| The game's chase between quarry runs | it walks at stats+20's object coordinates (another room's) with no room test, and may swing | [read enemy-readers-2.md §2] |
| A swing across rooms | r6_hit_room makes the attacker's room current when the rooms differ. So 0x8006255C's gate (0x800625A4), then 0x80061B9C's and 0x800637B8's, refuse it (hle.c:24849-24918) | [read] |
| The game's quarry run | (i)/(iii): the target's room != the creature's, so +20 := 0. (ii): the walk takes the nearest in-room candidate. With R6c, player 2 is "the player" for room 183's creatures (`[enemy] 38 ... runs as its room's game, player 2 the player`, M3 1632). M3: 40 "state 3 after 59" at 3884 and 39 "state 3 after 59" by 3890 (it was "state 21 after 1" through the countdown) | [read 0x800327D0-0x80032AB8; M3 1662, 1786] |
| Event 61 to the room left | the arrival sends 61 to every node of the room left (0x801520DC-0x801520F4). In a split that is the stayer's room. Its creatures in a state that does not handle 61 fall to state 0: they drop their route and are unregistered (0x800BB0C0, 0x8003A2E8, 0x80064A7C, 0x800E8464). In state 33, 61 may send 57 (inert). The stayer's node gets it too. M3: `room 183's route graph built for its enemies (slot 1; for 40; frame 3886)`, rebuilt two frames later | [read enemy-readers-2.md "enemy-life" §1; M3 1681] |

In solo, event 61 is "the player left this room". In a split it lands on the room still being played, which belongs to
the stayer readers (change-freeze-code-readers.md open question 5). It is not the mover's.

## 8. What this means for Phase 1b (F5, F2', F6)

1. **F5 is safe for the mover as designed.** Keep "the asker's own node is left to the game" (coop_node_held_ask's
   `g_coop_room_p2 == p2` return), and never lift layer 2 for the mover.
2. **F5 alone does not give a fight that goes on.** The freed creatures:
   - lose both players at their next quarry run (59 answered 0);
   - give up the chase (0x8003AE1C -> 0x8003B100, enemy-readers-2.md §2);
   - and their swings at the stayer are swallowed.

   The stayer sees zombies that walk again, turn away and whiff, then come back after the arrival [inferred: no run with
   F5 yet].
3. **The game's own way to let the stayer be fought: answer the player handler's busy ask "not busy" for the stayer's
   node.** That is what the solo game would see if the stayer were not in a countdown at all. The site:
   - the intercept already on 0x80155738 (coop_busy_ask), at lr 0x80058E84;
   - the node is the caller's r31 (`or r31,r3,r3` at 0x80058DFC). 0x80155738 is `lwz r3,-23344(r13); blr` and leaves r31
     alone, so cpu->gpr[31] is the node at the intercept [read 0x80155738-0x8015573C, 0x80058DFC];
   - the rule: co-op, coop_stay(), two slots, coop_room_change_on, both players' +592 equal (a split), and r31 == the
     stayer's node, meaning not the asker by g_coop_room_p2.

   Then the stayer:
   - takes hits (53, 39) and grabs (32 to their state, not 121);
   - answers 59 normally, so the game's own quarry picks the stayer, since the mover still answers 0. That makes F2'
     immediate rather than needed;
   - takes their own events (202 FINISH HIM, 30, 55 ...) as in solo. This is the sibling item F4's "stayer fighting", by
     the same rule.

   Leave the other busy asks as they are: node update 0x8000749C, the motion steps, the pump, 0x80061750/0x80061A8C. They
   go through coop_node_held_ask's hold-bit answer.
   - Apart (a non-split ticket), the same per-node answer could serve section 9's case. That is a separate decision,
     after its measurement.
   - Not this: a global "not busy" (coop_busy_ask's current cases). It frees the mover too (section 6).
   - Not this: making the mover "held" for coop_player_held. It stopped enemies coming at all in an earlier test
     (coop-failed-attempts.md:562).
4. **F2' adjustment.** With F2' as written (`held[k] = coop_player_held || coop_player_leaving`), a stayer held by the
   host (their words, close-up, Tome) makes both players "held". held[0] == held[1], so the nearest-player choice runs
   and may pick the mover [read hle.c:25603-25608].
   - Under a split ticket, choose the stayer only if they are not held; otherwise nobody (+20 := 0). That is the game's
     own "nobody", as r6_enemy_apart's "passed over" does (25178-25182).
   - Never the mover.
5. **F6 has a second release to cover [read 0x80061740-0x8006177C, 0x80061A8C-0x80061AB8; inferred for co-op].**
   - The held behaviour (kind 19, 0x80060EC8) sends itself 102 in state 26 on event 3 (and at 0x80061A8C) while busy, if
     its id == r13-21496 (the player). That is player 1 outside any turn.
   - So a grabbed player 1 stayer is let go by their own tick during player 2's countdown.
   - coop_grab_events' rule (hle.c:22293-22305) passes a 102 whose from == held. F6 must also cover this self-sent one,
     for player 1 as the stayer, which is the busy ask at those two sites.
   - A grabbed player 2 stayer: their tick runs with r13-21496 = player 1, so there is no self 102 [read].
   - The grabber's bites (39 at 0x80060B80..) go to kind 19's case 39 (0x80060188), which has no busy filter
     (enemy-readers-2.md §3; 0x80060EC8 has busy asks only at 0x80061750/0x80061A8C) [read]. So with F5, the bites land on
     a grabbed stayer during the countdown, as in solo outside a countdown.
6. **Check list for building F5** (only F5's behaviour and a new rule each needs):
   - the mover's rows: state 121 kept, health unchanged, no 53 or 39 to the mover's id between the ticket and the firing;
   - the stayer's rows: health changes and "after" stays theirs through the countdown, once (3) is in.

## 9. Apart -> apart: the same filter during the other's countdown [inferred, to measure]

- Apart, the 102 goes only to the asker's room (0x801F5654-0x801F5674, design-change-freeze.md §1.1). So the stayer's
  creatures are not held.
- The busy word is 4 for the whole countdown, and the stayer's node runs 0x80058DD8 with it:
  - creatures already after the stayer swing and whiff (11 swallowed);
  - at their next quarry run they drop the stayer (59 answered 0) and give up;
  - the stayer's own 202 (FINISH HIM), 30, 55 and 57 are swallowed.
- The stayer sees their fight stop for about a second while the screen keeps moving. That matches "player 1 is fighting in
  one room, player 2 enters another, and player 1's screen pauses" as much as the 66 ms picture stop.
- No run measures it. M5 (logs/run-20261112-180142-jtg-freeze-m5-apart-apart) was in rooms 341-344, with no creatures and
  no `[change-hold]` rows.

## 10. Checks a run could settle (for the next build; nothing run here)

- **K-m1, F5's mover.** M3/M4's recipe with F5 built, ED_COOP_ENEMY_LOG and ED_HURT_LOG:
  - expect `[hurt] event 11 from <zombie> to <mover>` with no 53, 39 or damage after it;
  - the mover's row: state 121 and the motion through to the firing, then an arrival on 25 (or 103/104 for ladders);
  - both directions.
- **K-m2, the stayer with the per-node answer.** As K-m1: expect 53, 39 and damage on the stayer, and "after <stayer>"
  kept through the countdown.
- **K-m3, apart -> apart with a fight (section 9), today's build.**
  - Player 1 among room 183's zombies, readied (the user's play: ED_INPUT_SCRIPT cannot ready, design M4), while player 2,
    apart, changes room.
  - Expect today: zombies "after 1" -> "after 0" within 8-64 frames of the other's ticket, `[hurt] event 11 ... to 1`
    with no 53 or 39.
  - For that, `[change-hold]`'s rows should also cover the stayer's room when apart. They do: "one row per node of the
    asker's room and of the stayer's room".
- **K-m4, section 6's exposure.** Player 1 in a close-up of their own while player 2 takes a door, near zombies, in a
  split. Expect today: zombies moving through the countdown (step != 0) and, if one reaches player 2, a hit landing on the
  mover in state 121.
- **K-m5, rooms 208 and 64.** `ed door 208`, `ed door 64`: is any fn66 door there in reach of a kind-40 attacker? (Room 208
  lets that attacker's 8/11/39 through while busy.)

## 11. Facts read for this report

| Fact | Where | Mark |
|---|---|---|
| The player's busy filter: 0x80155738, then 0x800C3D28 and 0x800CA964, and on non-zero return 1 | 0x80058E80-0x80058EB4 | [read] |
| 0x800CA964's set: always 10, 24, 25, 30, 32, 50, 53, 55, 57, 60, 63, 67, 101, 103, 107, 125, 133, 202; and 8, 11, 39, 59 except rooms 208 and 64 | 0x800CA990-0x800CAAF0 | [read] |
| The answer word starts 0 and is returned in r4; senders test r4 == 1 | 0x801F5024-0x801F505C, 0x80036784-0x8003678C, 0x80032814-0x8003281C | [read] |
| 0x800C3D28 sends 201 for a 39 with block flag 0x2 and amount > 0 | 0x800C3D50-0x800C3DAC | [read] |
| State 121: 6 -> state 1; 7 -> state 1 if 0x80032AD0 == 0; 52, 32, 107, 153, 44, 105, 125, 175, 40, 147 return 1; the rest fall to state 0 | 0x8005AE90-0x8005AFA4 | [read] |
| fn66's order: gate, the player let go, motion (event 253), ticket, fade; the motions by transition | 0x80166788-0x80166960 | [read] |
| Ticket writer: busy 4, 0x800443E4(count+3), 0x801C5118(0) | 0x801557A8-0x801557FC | [read] |
| The tick reads only the ticket; the ticket's only references | 0x80155830-0x80155924; grep `, -20484` | [read] |
| Grab start needs a non-zero 32 answer, else 0x80124788 | 0x80039A88-0x80039AF8 | [read] |
| Quarry (i), (iii): same room and 59 == 1, else cleared | 0x800327D0-0x80032838, 0x80032A64-0x80032AB8 | [read] |
| Sweep: same room, 59 == 1 | 0x80036754-0x80036790 | [read] |
| Hurt taker: room gate, hurt move, state 7 | 0x800637B8-0x800638E8 | [read] |
| The pump: delayed messages of held nodes wait while busy | 0x801F5410-0x801F5460, 0x801F55B8-0x801F55F4 | [read] |
| Held behaviour sends itself 102 while busy when it is the player | 0x80061740-0x8006177C, 0x80061A8C-0x80061AB8 | [read] |
| The busy word is not per player | hle.c:11998-12007, 21867-21895, 21726-21746 | [read] |
| coop_busy_ask answers every caller of 0x80155738 | hle.c:19704-19720, 28389-28390; stepE-r6b-p1words-door (lr 0x80058E84) | [read] |
| coop_enemy_target's held/nearest choice | hle.c:25554-25615 | [read] |
| r6_enemy_apart drops a target of another room | hle.c:25145-25185; M3 page 1634 | [read] |
| r6_hit_room refuses across rooms when apart, passes together | hle.c:24849-24918, 24468-24473 | [read] |
| R4 bracket: runner to 0x8014FDB8's entry at lr 0x80008400 | hle.c:14047-14110, 28593-28601; 0x800083EC-0x80008400 | [read] |
| M3: zombies held through the countdown (step 0.0, hold 1), the mover in state 121 with hold 0, the stayer let go at the tail, 41 dropped player 1, 40 and 39 after 59 | logs/run-20261112-180114-freeze-m3-p1-leaves-fight page-0001.log 852-1790 | [read] |
| M2: player 2's ticket with who 59, "motion 25 is theirs", player 1 not held | logs/run-20261112-180128-freeze-m2-p2-leaves page-0001.log 500-512 | [read] |
| No creature hit inside any ticket's countdown, 2026-11-10..12 logs | scan of every page | [read] |
| The consequences for F5 alone, the apart case and section 6's exposure | sections 3, 6, 8, 9 | [inferred] |
