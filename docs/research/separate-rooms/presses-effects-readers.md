# Presses and their effects: what the stayer's actions start, and how each meets the other's door (readers)

For the item after the split hold: the stayer keeps every action through the other player's countdown (about 62 frames,
or 12 for a walk-in zone exit), in both directions and both shapes. Read only: no code changed, nothing built, the game
not run. Sources: runtime/host/hle.c (working tree, uncommitted, line numbers as it stands now), build/ed-db/disasm.txt,
`ed fn` / `ed how` / `ed verb`, docs/game/research/transitions.md and verbs.md, docs/game/systems/pickups.md, and the
readers named in the task (b3-input-lock, stayer, b0-k7, design-split-hold §0-§19, split-hold-impl-notes).
[read] = seen at the address or hle.c line given. [inferred] = reasoned, not seen.

Words: the **mover** takes the door; the **stayer** stays. **Split**: both in one room at the ticket. **Apart**: the mover
already in another room. **The window**: from the ticket (0x80155770) to the change's tail. **The firing**: the tick finds
+0 at 0 and runs the change.

## 0. The answer

1. **The stayer's presses are stopped by different things in each direction.**
   - **Player 1 stays (player 2's door):** the game itself stops them. The ticket's input lock goes into the game's input
     copy, which is player 1's (b3 §4). Player 1's turn (room_turn) runs only the walking controller, never the play
     channel that carries B, Start and the quick spells. The ticket's prompt bit 0x10000 makes every world frame clear the
     game's prompts. The host adds two refusals on top (zone looks, quick spells), and it drops player 1's look as "over"
     under busy 2|4. [read; §2]
   - **Player 2 stays (player 1's door):** only the host stops them. Their input copy has no lock. coop_player_turn turns
     off their look and B, coop_quick_cast drops their quick spell, coop_quick_cast_ask refuses it again, and two sites
     refuse their Start. [read; §2]
2. **The change never fires in the middle of anyone's turn.** The tick is the third call of the world frame (0x800083EC),
   before the behaviour pass where both players' turns run, and before the play channel, the Tome's cast, the spells'
   update and the words tick. So an action started by a press is always one or more whole frames old when the change
   comes. [read 0x800083C8-0x80008500]
3. **What the change does to an action under way** [read 0x80152614, 0x80152298, 0x80151FDC]:
   - it clears the prompt bit, resets the prompt slots (0x801172B0, which cancels the current player's close-up),
     clears every script wait (0x80163F04(1)), clears every busy bit (0x801556CC(-1, 0)), re-holds the current player,
     and resets and chooses the cameras (0x801ECCE0, 0x801EE998, 0x801EE764, 0x801EEB58, 0x801EEA9C);
   - it does not close a plain words window, and it does not touch the spell list (no bl into 0x801C0000-0x801CFFFF).
   - All of this lands on whatever is in the game's memory at the firing.
4. **So the two directions differ again at the firing.**
   - Player 1's change runs with player 1 in. Player 2's own things (words, close-up, find, looks, script states,
     cameras, input) are host memory then. The change cannot reach them. [read: the turn swaps them in and out; inferred
     that nothing else does]
   - Player 2's change runs inside R4's bracket, with player 2's identity, casting words, cameras and prompt slots in. But
     player 1's things live in the game's memory. R4/R4b keep player 1's words window, waits, and busy bits (for a scene or
     a close-up only). They do not keep words-only or find bits. [read hle.c:14059-14116, 11268, 25565-25612]
5. **Must wait for the change** (hold the request, run it after the tail, as the game's own ed223 and request dispatcher
   do; §4):
   - anything that writes a ticket (a way out's fn66, ed145, a controlled summon's return);
   - a body switch (0x801F626C sites, Summon Trapper);
   - a cinematic or movie;
   - player 1's real Tome (a mode change, including Enchant's way of asking for an item);
   - a scene begun by the stayer when the change would clear its waits and bits (player 1's door: R4d is not built).
6. **Can run across the change** [inferred unless marked]:
   - walking and fighting (already);
   - player 2's own examine, words, find and close-up at player 1's door;
   - player 1's examine and words at player 2's door, under R4/R4b, once words-only and find bits are kept too;
   - quick spells whose cast only makes a spell record (no ticket, no load, no busy set, no cinematic and no body switch
     anywhere in the spell code [read]: Recover, Shield, Damage Field, Magick Attack, Reveal, Dispel);
   - player 2's copy Tome, which ticks no world frame of its own.
7. **Player 1's real Tome is a game mode in solo, but not in co-op.**
   - Solo: mode 3 replaces the world frame, so the tick stops and the countdown waits [read: the tick is called only at
     0x800083EC and 0x80050A90].
   - Co-op: the host runs world frames behind the Tome (tw, hle.c:15360-15366). The tick counts and the change can fire
     behind it.
   - With the Tome open, coop_room_change_on is false (game_menu_open, hle.c:11348). So split_on, the held ask, the
     one-ticket rule R4 and room_turn are all off through the window. Only split_counting's guards (G1-G5) stay on. [read]

## 1. The countdown and the firing, as the code has them

**The ticket writer 0x80155770** [read 0x80155770-0x80155814]:
- it writes the record 0x8058AFFC (+0 the countdown, at least 1; +4 the fade delay; +8 the room; +20 the transition);
- it sets byte r13-23392 := 1;
- it sets the prompts-off bit 0x10000 of r13-24048 (0x80117260(0x10000, 1));
- it sets busy 4 (0x801556CC(4, 1));
- it sets the input lock 0x800443E4(count + 3);
- it ends the cast (0x801C5118(0)).

**The tick 0x80155830** [read 0x80155830-0x80155918]:
- It takes 1 off +0. It takes 1 off +4, and when +4 reaches 0 it starts the fade (0x801E7C88).
- At +0 == 0 it runs the change: transition 17 runs 0x80152298 alone, then event 62 unless r13-26508; any other
  transition runs the runner 0x80152614. Then the callback.
- Its callers: the world frame at 0x800083EC and mode 6 (a cinematic) at 0x80050A90. Nothing else. [read grep]
- G2 (split_tick, hle.c:24905) skips the whole tick (+0 and +4) while busy 8, 16 or 32 begun in the window or a cutscene
  is up, and +0 > 1. The cap is 900 frames. [read]

**Countdowns** [read transitions.md:30-59]:
- doors, ladders, climbs (transitions 3, 6, 9, 12, 15): countdown 62, lock 65;
- **transition 2 (no motion, fade 10, countdown 12): 253 script uses, 205 with no prompt.** This is the walk-in zone exit.
  Its lock is 15 frames;
- transition 17: countdown 1.

**The world frame's order** (0x800083C8) [read 0x800083C8-0x80008500]:

| Call | What |
|---|---|
| 0x800083EC | the tick (and the change, at +0 == 0) |
| 0x800083F4 | the request dispatcher 0x800B7208(127) (G1 holds room requests here) |
| 0x800083FC | the behaviour pass 0x8014FDB8(1): node updates, player 1's controller, **player 2's turn** and player 1's room_turn (hle_coop_controller); R4's bracket ends here (coop_stay_change_end, lr 0x80008400) |
| 0x80008468 | the Tome's next cast 0x80334DFC, cast by 0x801C4250(1, &next) the frame after the Tome closes |
| 0x80008484 | the spells' update 0x801C5994 (every cast record's step) |
| 0x8000849C | 0x80163E00, skipped while r13-24048 has bit 2 (a find) |
| 0x800084D8-0x800084EC | any bit of r13-24048: the prompt slots cleared (0x80117360(0)) |
| 0x800084F4 | the play channel 0x80006A94: player 1's B/A/Z/stick prompt presses, Start, quick spells |
| 0x800084F8 | the prompt chooser 0x80117628 (reads r13-24048 at 0x801176B4) |
| 0x800084FC | the words tick 0x80024AF0 |

**The change** [read the bl lists]:
- The runner 0x80152614: 0x80152298, then the entry marker, then events to the current player, then 0x80043380 (the
  sanity door), 0x800BA530 (the route graph), the sound, the room's event (0x801F5620), and the camera block
  0x801EE998 / 0x801EE764 / 0x801ECCE0 / 0x801EEB58 / 0x801EEA9C. Last, 0x801642D4 (the entry's script).
- The load 0x80152298: stream drop 0x801A0EAC, 0x801A05EC, 0x80042910 (the background thread's stop), 0x80154C94 (the
  slot load), the arrival 0x80151FDC, the tail 0x80154B54.
- **The arrival 0x80151FDC:**
  - 0x80117260(0x10000, 0): the prompts come back;
  - the strip walk 0x80043A5C;
  - event 61 to the room left (0x801520DC-0x801520F4);
  - unless the flags have 4: 0x801172B0 (the prompt reset, which cancels the player's close-up: 0x80024CDC ->
    0x80024B24(0), hle.c:11248) and 0x80163F04(1) (every script wait);
  - 0x801556CC(-1, 0) (every busy bit, 0x80152184);
  - 0x801F6828(current player, 1) (the re-hold, 0x80152190);
  - the fade in (0x801E7C88).
- No call in the runner, the load or the arrival goes to 0x801C0000-0x801CFFFF, the spell code. [read] Whether the
  load's strip (0x80043AB8) removes spell things, a sign (kind 34) or a summoned body was not read.

**Who is in memory at the firing:**
- Player 1's door: the game's own state. Player 1 is the current, main and casting player. Player 2's input copy
  (g_coop_state[1]), words set, looks, script states (g_coop_states.p2), casting words (g_coop_casting) and cameras are in
  host memory. They are swapped in only inside player 2's turn (hle.c:20910, 21102), their look or tick, or a use of
  theirs. [read]
- Player 2's door (R4 bracket, notify_ticket_run hle.c:14059-14081):
  - coop_be_player2: r13-26524, -21492, -21496, -21488, -21484 and the casting words;
  - the room in play set to player 2's;
  - the prompt slots 0x80335A38 kept aside;
  - player 2's cameras in.
  - It runs to coop_stay_change_end (hle.c:14083). Player 1's busy word, script waits table 0x805CAA40, words window
    r13-25960, find window, play hold r13-25696 and control channels stay in the game's memory. [read]

## 2. What stops each press today

| Press | Player 1 stays (player 2's door) | Player 2 stays (player 1's door) |
|---|---|---|
| B on a prompt (examine, pick up, a door) | no zone look: coop_scan_player1 answers "nothing come into" (hle.c:20183). The binding lock 65 in the game's copy (r13-23592, pad_globals) skips the play channel's rows, where 0x80006EBC is (b3 §2). The prompt bit clears the game's slots each frame (0x800084D8). | coop_player_turn: g_coop_prompt = -1, no coop_prompt_look while room_other (hle.c:21000-21002) |
| A zone look (walk-in) | coop_scan_player1, hle.c:20183 | the same branch, hle.c:21000 |
| Quick spell | the binding lock (rows skipped); coop_quick_cast_ask on 0x800069DC (hle.c:19774-19787) | coop_quick_cast(cpu, room_other \|\| left_behind) drops the press, unkept (hle.c:21090, 12475-12481); coop_quick_cast_ask refuses it too (their guest call has r13-21496 = theirs) |
| Start / Tome | the binding lock (rows skipped) and the Start lock r13-26232 (shared, one word: 0x80006CDC returns at 0x80006D00). "An intercept on player 1's Start in player 2's room change was never reached" (coop-failed-attempts.md:560) | the copy-ask refused under coop_room_change_other (hle.c:27424); start_room (hle.c:27647) |
| Walking, attacking | free: room_turn runs coop_player1_turn from g_coop_p1_free with the hold opened (hle.c:20342, 20364-20381) | free: their own copy, past the busy refusal (hle.c:20869-20870) |

All [read]. More facts behind the table:
- **Player 1's turn never steps the play channel.** coop_player1_turn calls only the pad update 0x8013E518 and the
  controller 0x80045BC4 (hle.c:19879, 19891). The play channel is stepped only in the world frame (0x800084F4), outside
  any turn, with the game's copy in. [read] So even with the lock gone from the game's copy, player 1's B, Start and quick
  spells would run there under busy 4 with the hold in.
- **The Start lock r13-26232 is shared** (b3 §4). It counts down only when the play channel runs with r4 == 1
  (0x80006900). [read]
- **r13-24048 is shared** (outside the 2060 block -24040..-21981 and outside CoopPlayerState) [read
  coop-level3-design.md:130]. While the ticket's 0x10000 is in it, the world frame clears the game's slots every frame,
  and the chooser shows only slot 0 while any bit is set (coop-no-pause.md row 67(c)). [read] Whether player 2's own
  prompts and look (their own slots, coop_prompt_show) are hit by the same bit was not read [inferred: their chooser runs
  0x80117628 with the shared word].
- **The host drops player 1's look under player 2's door.** coop_p1_close_up tolerates the room bit 4 only for player 1's
  own change (room_bit, hle.c:19308). With busy 2|4 in player 2's countdown, held is false, so g_coop_p1_look is
  cleared: the look is taken for over (hle.c:19309-19316). [read] A close-up of player 1's begun in player 2's window would
  lose its owner on the host side [inferred].
- **Player 2's input copy has no lock** at player 1's door. Their own door locks the game's copy (player 1's), not theirs
  (b3 §4). [read there]

## 3. Each action: what it starts, how it meets the countdown, what the firing does to it

### 3.1 Examine (B on EXAMINE, a close-up)

**What it starts** [read pickups.md, coop-no-pause.md rows 41, 57, 90]:
- a prompt press (0x80006EBC);
- the look's script (0x801642D4);
- the close-up stepper 0x80024D78, which holds play itself with 0x80199EA0(0). That is the walking channels held, plus
  busy 2 by 0x801556CC(2, 1) [read 0x80199EA0-0x80199F34];
- a close-up camera push;
- DETAIL 55-110 frames later, then the words window (0x80024310) with the words' hold 0x80042A8C (busy 8), then RETURN.
- Player 1's examine uses all of this in the game's memory, so the busy bits are shared.
- Player 2's examine (step F, their own looks) runs in their own set: words, window channel, hold and close-up record in
  their copy. Nothing goes into the busy word (hle.c:15866-15877, the own-set comments). [read]

**Meeting the countdown:**
- An examine takes longer than any countdown (DETAIL alone is 55-110 frames after EXAMINE: coop-no-pause row 83). So
  one begun inside a 62- or 12-frame window is always under way at the firing. [inferred from the row]
- **G2 for each bit:**
  - Player 1's busy 2 is not one of G2's bits, so G2 does not wait for it.
  - Player 1's words (8) make G2 wait. They also turn coop_room_change_on off, and with it room_turn, the held ask and
    the allow list (hle.c:11346-11348).
  - Player 2's own examine sets nothing that G2 sees.
  - [read split_tick, coop_room_change_on]

**If the change fires under it:**
- **Player 2 examining, player 1's door (split or apart):**
  - The firing runs with player 1 in. The prompt reset, the waits clear and the busy clear act on player 1's (the mover's)
    slots, table and word. The camera block acts on the game's set, which is player 1's.
  - Player 2's close-up, words and script states are host memory then, so the examine goes on in their next turn.
    [inferred from §1's "who is in memory"]
  - coop_room_change_words closes only the game's window, and not on the lend route (hle.c:11263-11294). [read]
  - After a split's firing, player 2 is apart. Their turns run with their room swapped in (coop_stay_room_in,
    hle.c:20980) [read], so the look's script runs in their room [inferred].
  - **The mover:** unaffected. Player 2's own set writes no shared bit. [inferred]
  - Open: the shared prompt bit 0x10000 during the window (§2).
- **Player 1 examining, player 2's door:**
  - Inside the bracket the arrival's prompt reset acts on player 2's slots. Player 1's are kept aside and put back
    (hle.c:14073, 14087) [read].
  - stay_waits_clear puts back the waits of scripts not player 2's (hle.c:25565-25589) [read].
  - stay_busy_clear keeps player 1's bits (all but 4) when coop_p1_in_close_up or their scene is on
    (hle.c:25590-25612) [read].
  - The cameras are player 2's set for the bracket (coop_cameras_in, hle.c:14074) [read].
  - The words window is left open for player 2's door (hle.c:11268) [read].
  - So a close-up of player 1's crosses player 2's change today. That was built for one begun before the ticket (R4b);
    one begun inside the window takes the same code [inferred].
  - **Not kept:** words-only (8) and find (16) bits. stay_busy_clear needs a scene or a close-up. G2 waits for those bits
    instead, which delays the mover (below). [read]
  - Also: coop_p1_close_up drops the look under busy 2|4 (§2), which has to be fixed for a look begun in the window.
  - **The mover (player 2):**
    - A wait of G2 for player 1's words keeps player 2 in the black. Their door turns last 120 frames (COOP_DOOR_FADE,
      hle.c:21776). After that their turn is refused as busy. split_tick only logs past 50 frames. [read]
    - In a split, player 1's busy set (2 or 8) sends event 102 from player 1 to the room (0x801556F0-0x80155720). S2
      keeps it from the room's nodes only while split_on. With bit 8 set, split_on is false, so the 102 reaches the
      mover's node too [read split_send].
    - What kind 2 does with a 102 in state 121 (a held bit 8 on the mover, frozen mid-motion?) was not read.
      [inferred risk]
- **Player 1 the mover:** has no examine of their own (input locked). **Player 2 the mover:** in state 121, no examine.

### 3.2 Pickup (B on PICK UP, a find)

**What it starts** [read pickups.md]:
- the thing made pick-upable (0x801F9DE8);
- the find 0x80024528(element, -2, player, handle, ...), with its words (0x80024434, record 0x8030A6E8) and the find's
  hold 0x80042BB8;
- r13-24048 bit 2, which the script timers wait on (0x8000849C), and busy 16;
- after the close, the row's +36 script (event 190).
- The opening and the closing give the item. The find gives to its r5. hle_coop_found puts player 2's find on player 1's
  list.
- Player 2's own find (step C) is in their own set, with its after-scripts left for player 1's controller to run as a use
  of theirs (hle.c's CoopWordsOwn scripts). [read]

**Meeting the countdown:**
- Player 1's find (16) makes G2 wait, and coop_room_change_on goes false. [read]
- Player 2's own find sets no shared bit, so G2 does not wait. [read]
- Player 1's find also sets r13-24048 bit 2, which turns the world frame's prompt clear on (any bit) and skips
  0x80163E00 [read 0x8000848C-0x8000849C].

**If the change fires under it:**
- **Player 2's own find, player 1's door:** it survives in host memory [inferred]. Its close and after-script then run as
  a use of theirs, with their room in (tome_copy_left and coop_use_start take coop_stay_room_in: hle.c:27975-27977)
  [read for the Tome item; inferred for the find].
- **Player 1's find, player 2's door:**
  - G2 holds the firing until the find closes. It only fires if the 900-frame cap runs out first.
  - At a firing under it, coop_room_change_words leaves a find window open (its channel names the find's bindings
    0x8023E4B8): "its close would run a script or the stepper in a dead room (an open note)" (hle.c:11250-11252, 11272)
    [read].
  - stay_busy_clear would drop its bit 16 [read].
  - The item is given at the open and the close. A close that never comes, or one that runs in the wrong room, could lose
    the item or give it twice [inferred].
- **The mover:** as 3.1, G2's wait and the door turns.

### 3.3 A zone look (walking in)

**What it starts** [read hle.c:16717-16795, coop-no-pause rows 66, 80, 81]:
- the game's scan marks each zone come into (0x80134684, bit 1 of +104) and runs its script (0x801642D4);
- enter scripts can ask a prompt (0x80117404), open words, start a scene (fn22: busy 32, r13-23280, r13-25696), set
  cameras (fn8, fn10), make a room request (ed145: request 1 at the dispatcher), or ask for a room (fn66).

**Meeting the countdown:**
- **fn66 inside the window:**
  - coop_script_go_to_room passes it over (r3 = 0) while coop_stay() and coop_room_change_on: "the second waits for a new
    press" (hle.c:16760-16769) [read].
  - If coop_room_change_on is false (a scene or words up, or player 1's Tome), R4 lets it through, and G1 refuses it at
    0x80155770 (split_ticket_ask, always printed) [read].
  - Either way the zone stays marked "come into". A walk-in exit then never fires again until the player walks out and
    in. That is the "standing still in a held exit" note (coop-no-pause row 66) [read].
  - The game's own mechanism to keep it is coop_way_out_wait / coop_way_out_go (hle.c:16717-16759). It is used today only
    when the other player's scene, words, find or close-up is up. Its `tickets` test (way->tickets != g_coop_tickets)
    would keep a way out remembered after the door's own ticket, because the count is taken at wait time, after the
    ticket [read]. It needs a release at the change's tail.
- **ed145 (request 1):** split_requests holds it at the dispatcher until the window ends, and it runs then
  (hle.c's split_requests / split_request_after) [read].
- **A scene (32):**
  - G2 waits [read].
  - **Player 1's door with player 2's scene:** the arrival clears all script waits and busy bits, with no bracket to keep
    player 2's. g_coop_tickets cuts player 2's scene records at every ticket (stayer-readers §2.11). That is the R4b fault
    mirrored, and R4d (design-r4d.md D1-D3) is not built (no R4d code in hle.c) [read grep]. So a scene of player 2's
    begun in the window, or one running when player 1 takes a door, can lose its waits and hold at player 1's firing
    [inferred, as R4b's run showed for the mirror].
  - **Player 2's door with player 1's scene:** R4b keeps it [read].
- **Prompts:** during the window the ticket's 0x10000 clears the game's slots every frame (§2). So a prompt the stayer's
  zone asks for is not shown to player 1 [read]. For player 2 this was not read.

**If the change fires under it:** a zone look is one call inside a turn, so it never straddles the firing (§0.2). Only
what it started does: a scene, words, a remembered way out.

**The 12-frame zone exit:** transition 2 has no motion (the mover is not in state 121) and a lock of 15. A stayer's press
in those 12 frames starts nothing that ends before the firing. Every case above straddles it. [read transitions.md;
inferred]

### 3.4 A quick spell

**What it starts** [read 0x800069DC-0x80006A90, 0x801C4250-0x801C4998]:
- **Gates:**
  - the current player's node flags 0x801F68E0 (bit 0x4000 skips the next gates);
  - 0x80307170 +8 not 1 or 2, or 0 with flag 307 set;
  - r13-26228 at 0 (a cooldown, raised by 0x80006968 from 0x800179AC and 0x800A621C);
  - event 185 to the current id answering 1.
- **The cast 0x801C4250(1, slot):**
  - in room 83 it refuses with words 0,19 (the Trapper dimension);
  - without the Tome (0x80109A68) it refuses with words 0,13.
  - Each refusal opens words (0x80024310) and raises the Start lock 0x800068E0(2) (0x801C42E0-0x801C4300,
    0x801C4350-0x801C4370).
  - Then it needs r13-22336 at 0 (nothing being cast), takes the main id and node (0x801F6258, 0x801F6250), and asks the
    main id event 175.
  - It sets r13-22336 := the spell (0x801C43DC).
  - It checks by spell (bits 4-12):
    - 0x300: Enchant Item. It asks the record 0x80335748 for an item. With none and items carried, it opens the Tome
      (0x80006CDC(0, 1)) and clears r13-22336, or, if the mode is already 3, 0x8011048C (0x801C4560-0x801C45B4).
    - 0x810: needs a creature in the room in play: 0x801F662C(...) against r13-23276, else words 0,2
      (0x801C45C4-0x801C463C).
    - 0x820: a summon. It refuses in four rooms (71, 104, 236, 58) with words 0,11 (0x801C4644-0x801C4688).
  - It pays (0x800354F8, the stat write the host also uses for magick).
  - It makes the record (0x801C49AC, 0x801C2968, 0x801C72EC).
  - Each failure path shows words and sends event 176.
- The record goes into the one list r13-22324: caster id at +12, spell at +4, state byte +4081 (0 or 6 while charging,
  1 once live), step at +28. The spells' update 0x801C5994 steps it every world frame (0x80008484) [read hle.c:12620-12630
  comment, 0x801C5118's walk].
- **Record steps** (0x801C316C) [read]:
  - Enchant (768) holds play with 0x80199EA0(0) (busy 2) and sets r13-24048 bit 0x800 (0x801C36A4-0x801C36C4). The host
    passes that hold over in co-op (COOP_ENCHANT_HOLDS 0x801C36B4, hle.c:13038, 13192-13199).
  - 0x810 holds play the same way when 0x801D7024 says so (0x801C3740-0x801C376C), and the host does not pass that over
    [read: only 0x801C36B4].
  - 0x820 spawns a creature (0x8003121C at 0x801C3A00, its type from 50/51/52, 13/22/20 or 14/37/38 by a test). What those
    numbers are (kinds or models) is unread. It also shows words (0x80024310 at 0x801C3DAC).
- **What no spell does:** no bl to the ticket writer, 0x80152298, 0x801556CC, 0x8004F374/0x8004F4D0 (cinematic, movie) or
  0x801F626C (the body switch) anywhere in 0x801C0000-0x801CFFFF [read grep].
- **Moving ends a cast:** 0x801C4F48 sets r13-22312 for the main id. The host keeps player 2's (coop_cast_cancel,
  hle.c:28102-28114) [read].

**Meeting the countdown:**
- **The ticket's own cast end 0x801C5118(0)** [read 0x801C5118-0x801C51F8, 0x801C72AC, 0x801C4F08]:
  - It looks at the main id (r13-21488) and r13-22336.
  - If that id has a record in state 1 (live), it does nothing.
  - Otherwise it ends that id's record for that spell while it is still charging (state 0 or 6). It clears +4080 bit 3,
    calls 0x801C4F08 (+4080 bit 0, then the record's +40 callback), and gives no item refund (r3 = 0).
  - So in solo a door cuts a cast that is still charging, and its cost is already paid [inferred from the order: the pay
    comes before the record].
- **Whose cast it ends:**
  - player 1's door: player 1's (the mover's);
  - player 2's door from a prompt or a use (coop_use_start swaps -21488 and the casting words: coop_be_player2,
    hle.c:12361-12384): player 2's (the mover's) [read];
  - **player 2's door from a zone they walked into in their own turn:** their turn swaps only r13-26524, -21492 and -21496
    (hle.c:20965-20974), so -21488 and -22336 are player 1's. The ticket would end the stayer's charging cast. [inferred:
    whether that fn66 writes its ticket inside their turn is unread]. G3 would also see player 1 casting and open no window
    [read split_stayer_casting].
- **A cast begun inside the window** is not cut: the window's cast end has already run. Its record charges and goes live
  in the window. A 0x810 cast with its hold sets busy 2, which split_on tolerates. [inferred]

**If the change fires under it:**
- The change does not touch the spell list (§1). So the record goes on after the firing, stepped by 0x801C5994. Player
  2's records are stepped with them the player (coop_spells_update) [read the comment, hle.c:12620-12630].
- **Apart after the firing:**
  - Player 2's cast in their turn has their room in (stay_in comes before coop_quick_cast: hle.c:20980, 21090) [read].
  - Their record's later steps run in the world frame's spells update, with the room in play being player 1's. Whether a
    step reads r13-23276 (hits, a summon's spot) was not read (enemy-readers.md:159) [unknown].
- **A summon:**
  - The creature is a node of the room it was spawned in.
  - In the stayer's room it plays on (A1), unless its kind is K7 (28, 50, 82: "paths to the ticket writer or a capture").
    Then it stands till the firing [read split_node_freed; that the summon is K7 is inferred from the 50/51/52 values].
- **The mover:** a spell aimed at a creature in the room can reach the mover's attackers; the mover's filter keeps the
  game's word (K1) [read]. Nothing else of a cast reaches the mover [inferred].

### 3.5 Body switches and possession (what a summon or a spell can lead to)

- **0x801F626C (the current id and node) has six callers** [read completeness.md:150, disasm]:
  - 0x80007868 (spawn);
  - 0x80059B18 (kind 2's handler);
  - 0x800A2400 (in 0x800A2254, reached from 0x8009BC68, a sanity effect's code);
  - 0x800A27C0 (in 0x800A2548, the ticket writer of kinds 28, 50 and 82: b0 §0);
  - 0x800C4904 (in 0x800C46BC: kind 2's event 250 with data 2, at 0x8005A8AC, and 0x800A85C4. It spawns a body,
    0x8003121C at 0x800C4870, and writes r13-26524);
  - 0x800C4C68 (in 0x800C4A8C, from 0x800A421C and 0x800A8804).
  - 0x800C46BC and 0x800C4A8C also raise the Start lock 0x800068E0(182/180) (b3 §1).
- **Summon Trapper** puts the player in a non-playable body (model 28) with the "switch player" screen (hle.c:28242-28244;
  coop-glitch-todo.md E8) [read]. Which spell sends event 250 with data 2 was not read.
- **0x800A2548** writes a ticket when the linked object's room differs (transitions.md:372; b0 §0) [read]. A controlled
  body that ends in another room brings the player back by a ticket. Inside the window, G1 refuses that ticket and the
  return is lost [inferred].
- **So a body switch must wait for the change:**
  - the stayer's identity would move under the host's turn swaps;
  - coop_stay_spared spares the stayer's node by g_coop_p2 or r6_player1_node, not the body they are in
    (hle.c:14587-14610) [read; inferred consequence];
  - the return can need a ticket.

### 3.6 Player 1's real Tome (Start)

**What it starts** [read verbs.md:324; hle.c tw]:
- 0x80006CDC(1, 0). Its gates: a fade, the Start lock, the play hold, a control channel, event 135, 0x801C551C, and not
  0x8006DB04(1).
- Then mode 3 (0x80018808(3)), or modes 7/8/10 by 0x80068D18 and event 84.
- In co-op, tw:
  - the Tome's own pack and read-back of the room are passed over (tw_keep, hle.c:13511-13525: the lrs of the Tome's
    start and end; r13-26508 := 0 at lr 0x80110E8C);
  - the world frame (0x800083C8) runs in turn with the Tome's (TW_WORLD_FRAMES 1, hle.c:15360-15366);
  - the mode words are set to 2 for the world's frames.
- **So in co-op the world frame and the tick do not stop under player 1's Tome.** [read]
- In solo they do: the mode table runs one frame function, and mode 3's frame 0x80111234 has no tick [read b0 §1, the
  tick's two callers].
- Items and casts from it are left for the world:
  - an item's script by ed9 (r13-26324, r13-24244);
  - a cast in the next cast 0x80334DFC, cast at 0x80008468 on the world frame after the Tome closes (hle.c:12544-12546)
    [read].

**Meeting the countdown:**
- At player 2's door player 1's Start is refused by the game (the binding lock and the Start lock, §2).
- If the Tome were let open:
  - game_menu_open makes coop_room_change_on false for the whole time it is up. That turns off the held ask (player 1's
    node), room_turn, R4's one-ticket rule and the allow list. Only G1-G5 stay (split_counting) [read].
  - The change can then fire in a world frame behind the Tome. That world frame runs player 2's bracket (their cameras,
    slots and identity) while the Tome's switches (kTwSwitches) are saved and loaded around each mode's frame [read
    tw_block, tw_switches].
  - The arrival calls 0x801ECCE0, the camera-depth reset that game_menu_open reads (hle.c:13243), inside a world frame
    whose depth the host zeroes anyway (hle.c:15361) [read].
  - Whether the Tome's close then finds the world's camera, slots and room as it left them was never run with the stay
    (stayer-readers §2.13) [unknown].
- **A Tome cast or item script** waits for the Tome's close, which may come after the firing. The next cast then runs at
  0x80008468 with player 1 the caster, in the room in play then [inferred].
- **Enchant by quick spell** opens this Tome through 0x80006CDC(0, 1) (§3.4). That path is a Tome opening too.
- **Solo precedent:** no Tome during a countdown (Start lock), and none can have the countdown fire under it (no tick in
  mode 3).

### 3.7 Player 2's copy Tome (g_tome_copy_turn)

**What it starts** [read hle.c:15030-15085, 27921-27990, 28143-28147, 24607-24609]:
- A copy of memory runs the Tome in its own turn. That turn runs the game's mode change once, then only the Tome's frame
  and the after-frame list, never the world frame. So the copy ticks no countdown of its own [read 15060-15067].
- split_counting is false in its turn.
- Many co-op hooks are off in it (tome_copy_hook_off: coop_cast, coop_script_go_to_room, coop_door_motion and others).
- **At its close (tome_copy_close, then tome_copy_left):**
  - a cast chosen is cast in the real game with player 2 the player and caster (coop_be_player2, then 0x801C4250(1,
    0x80334DFC));
  - an item's script runs as a use of theirs, with their room in (coop_stay_room_in), their states and cameras in, and the
    words in their own set.

**Meeting the countdown:**
- Opening it in player 1's window is refused (hle.c:27424, 27647).
- The follow-mode reason given was that a copy taken with the change in it showed black once the new room was in
  (hle.c:27642-27646) [read].
- With the stay, player 2's half never changes room at player 1's door. In a split their room stays resident, so that
  reason is gone [inferred, as stayer-readers §2.9].
- A copy opened before the ticket stays whole through the change (logs/run-20261106-101418, cited at 27644) [read the
  comment].
- **The close inside the window:**
  - its cast meets the cast rules of 3.4 (not cut: the window's cast end has run);
  - its item script can open words, a scene or a fn66, and meets 3.3 (fn66 refused or held, a scene making G2 wait);
  - player 2's Tome cast or item is not refused today (design §3.10) [read].
- **Player 2's door with the copy open:** not possible. They press the door in the world.

### 3.8 The mover's side, all actions together

| | Player 1 the mover (player 2 stays) | Player 2 the mover (player 1 stays) |
|---|---|---|
| Their input | locked in the game's copy (65, or 15 for a zone exit); G2 renews it each waiting frame (hle.c:24905's guest call) | not locked (their own copy); state 121 and 120 frames of door turns (hle.c:21776) |
| What a stayer's action can do to them | nothing through shared bits: player 2's own set writes none | player 1's words, find or scene (8/16/32) make G2 wait and switch coop_room_change_on off. After 120 frames their turns are refused as busy. In a split, a 102 of player 1's busy set may reach their node (3.1) |
| At the firing | the change is theirs; player 2's things are out of memory | the change is theirs inside the bracket; player 1's are in the game's memory, kept by R4/R4b only for a close-up, words window and scene |

## 4. Solo precedents: what the game does when its one player's action and a door meet

1. **The ticket shuts the door on presses.** Binding lock and Start lock (0x800443E4(count + 3)), the prompts-off bit
   0x10000, busy 4 (the player's filter takes hits, grabs and target asks), and the cast end. In solo nothing new can
   start in a countdown. [read §1]
2. **A cast still charging is cut; a live one goes on.** 0x801C5118(0) ends a record in state 0 or 6 and leaves state 1.
   [read]
3. **A close-up is cancelled by the arrival, words are not.** 0x801172B0 cancels the player's close-up (0x80024CDC ->
   0x80024B24(0)). A plain words window survives into the new room (hle.c:11248-11250). Transition 6 follows EXAMINE
   (L04/117, L04/51), and CLIMB DOWN can follow EXAMINE or TEXT, so solo does change rooms out of a look. [read]
4. **Waits and busy bits are wiped at the arrival** (0x80163F04(1), 0x801556CC(-1, 0)): "right alone, where the room left
   is gone" (hle.c:25553) [read].
5. **"Do it when the lock runs out."** ed223 sets r13-26256, and the play channel's +0 (0x80006900) opens the menu once
   r13-26232 is 0 (verbs.md:326) [read]. This is the game's own deferral of a menu past a lock.
6. **Requests wait for their delay.** The dispatcher runs a request only when its delay is out, then clears it
   (0x800B7208-0x800B72F8) [read b0 §1]. G1 keeps that request over the window.
7. **Timers and deaths wait for a ticket.** The movie timer 0x8007E124 holds its count at 1 while a ticket counts
   (0x8007E29C) [read b0 §3]. Kinds 9 and 74 refuse to die while a ticket counts (0x8008D380, 0x800E0664) [read §19.1].
   Script timers act on +0 == 1 (0x8007D1E0, 0x8007D49C, 0x8014E0EC) [read §19.4].
8. **One ticket record.** A second ticket overwrites 0x8058AFFC [read the writer]. The game avoids it by the lock, not by
   a test.
9. **A cinematic ticks the countdown, and a menu does not.** Mode 6 calls the tick (0x80050A90); mode 3 does not [read].
   So in solo a cinematic can have a door fire under it, and its own end can write a ticket (0x8012D1A0).

So the game has no "two actions at once" rule. It has: shut new presses out (1), cut what cannot cross (2, 3), wipe what
belonged to the room left (4), and defer what must come after (5, 6, 7).

## 5. What must wait, what can run across (both directions, both shapes)

| Action | Across the firing? | Why | The game's own way to make it wait |
|---|---|---|---|
| Walk, attack, be hit | yes (built) | the split hold | - |
| Player 2's examine / words / find / close-up, player 1's door | yes [inferred] | own set in host memory at the firing; no shared bit | - |
| Player 1's examine / close-up / words window, player 2's door | yes, once fixed [inferred] | R4 bracket and R4b keep slots, cameras, words window, waits, close-up/scene bits; also needs words-only and find bits kept, coop_p1_close_up's room bit for player 2's change, and the 102-to-mover question | - |
| Player 1's find, player 2's door | yes only if its window and bit 16 are kept; else hold the firing (G2 today) | the find window is left open by the change "in a dead room" (hle.c:11250) | G2 (today) |
| A way out (fn66) begun in the window | no | one ticket record; G1, R4 | remember (coop_way_out_wait), release at the tail |
| ed145, a held room request | no | ticket | G1's dispatcher pass-over (built) |
| A scene begun in the window | player 2's door: yes (R4b); player 1's door: no until R4d D1-D3 | the arrival's wait and busy clears | G2 (today), or R4d |
| Quick spell: a record only (Recover, Shield, Damage Field, Magick Attack, Reveal, Dispel) | yes [inferred] | spell list untouched by the change; no busy set except 0x810's hold (busy 2) | - |
| Enchant Item | yes when an item is chosen (its hold is passed over in co-op); its "choose an item" path opens player 1's real Tome (mode 3) or asks player 2's copy (coop_quick_cast's g_tome_copy_ask) | a mode change | hold the press (coop_press_keep) |
| A summon (0x820) | the cast yes; a controlled body (Summon Trapper, kinds 28/50/82) no | body switch; return ticket | hold the press |
| Player 1's real Tome | no | mode 3; game_menu_open turns every room-change rule off; firing behind it never run | ed223's way: keep the Start, open after the tail |
| Player 2's copy Tome | open: yes [inferred]; its close's cast and item script: as quick spell / zone script | the copy ticks nothing | the close's leftovers can wait for the tail (tome_copy_left) |
| A cinematic or movie begun in the window | no | mode 6 ticks; its own end ticket is lost to G1 (risk 12) | G2 (today) |

## 6. What a run must show, and what needs the user

Scriptable (fresh Jump-to-Game states, ED_COOP_LOG, ED_COOP_CAST_TRACE, ED_COOP_TRACE, both directions, split and apart):
- player 2's examine, PICK UP and quick spell during player 1's door, with the host refusals off in a test build: the
  close-up, words and item across the firing, and the `[cast-trace]` record still stepped after it;
- player 2's walk-in exit (transition 2) while player 1 is charging a spell: whether the ticket ends player 1's cast
  (`[cast-trace] 0x801C4F08 ... lr` from 0x801C5118) (3.4);
- r13-24048 at each frame of the window, and player 2's prompt shown or not (§2);
- kind 2's answer to a 102 in state 121 (3.1), by a split with player 1 opening words during player 2's door.

Needs the user's hands (ED_INPUT_SCRIPT cannot ready player 1, design §4 Q1):
- player 1 pressing B on an EXAMINE, a quick spell and Start inside player 2's door, once the play channel runs in their
  turn;
- Summon Trapper (or any summon) cast by each player near the other's door;
- player 1's real Tome left open while player 2 takes a door, then closed after the firing.

## 7. Still unread

- Whether player 2's walk-in fn66 writes its ticket inside their turn (decides 3.4's cast end on the stayer).
- What 0x80043AB8's strip removes besides the spared player nodes: a sign, a summon, a possessed body.
- Which spell record steps read r13-23276 (apart, after player 1's firing).
- Which spell sends kind 2's event 250 data 2 (0x800C46BC), and what 50/51/52 etc. are at 0x801C3A00.
- Whether 0x80117404 (prompt ask) or player 2's chooser refuses under r13-24048's bits.
- Kind 2 in state 121 given event 102 (bit 8 on the mover).
- The Tome's close after a change fired behind it in co-op.

## 8. Facts read

| Fact | Where | Mark |
|---|---|---|
| ticket writer: record, r13-23392, prompt bit 0x10000, busy 4, lock count+3, cast end | 0x80155770-0x80155814 | [read] |
| tick: +0 then +4 (fade at +4 == 0), change at +0 == 0, 17 direct | 0x80155830-0x80155918 | [read] |
| tick's callers: world frame, mode 6 | 0x800083EC, 0x80050A90 | [read] |
| world frame order: tick, dispatcher, behaviour pass, next cast, spells update, prompt clear, play channel, chooser, words tick | 0x800083C8-0x80008500 | [read] |
| prompt clear on any r13-24048 bit; 0x8011727C / 0x80117260 are its getter/setter | 0x800084D8-0x800084EC, 0x8011727C, 0x80117260 | [read] |
| arrival: prompt bit off, event 61, prompt reset and waits clear (unless flag 4), busy clear, re-hold, fade in | 0x80151FDC-0x801521A8 | [read] |
| runner's camera block | 0x80152790-0x801527DC | [read] |
| no spell-code call in the change | bl lists of 0x80152614, 0x80152298, 0x80151FDC | [read] |
| cast end: main id, state 1 skips, state 0/6 ended, no refund with r3 0 | 0x801C5118-0x801C51F8, 0x801C72AC, 0x801C4F08 | [read] |
| quick cast gates | 0x800069F0-0x80006A78 | [read] |
| cast: refusals open words and Start lock 2; r13-22336 set; event 175; Enchant opens the Tome; 0x810 needs a creature in the room in play; 0x820's four rooms | 0x801C42BC-0x801C4688 | [read] |
| Enchant's and 0x810's play hold; 0x80199EA0(0) sets busy 2 | 0x801C36A4-0x801C376C, 0x80199EA0-0x80199F34 | [read] |
| host passes Enchant's hold | hle.c:13038, 13192-13199 | [read] |
| summon spawn and words | 0x801C3A00, 0x801C3DAC | [read] |
| no ticket, load, busy set, cinematic or body switch call in 0x801C0000-0x801CFFFF | disasm grep | [read] |
| body switch 0x801F626C's callers; 0x800C46BC from kind 2 event 250 data 2 | disasm, 0x8005A85C-0x8005A8AC | [read] |
| Summon Trapper's non-playable body | hle.c:28242-28244 | [read] |
| coop_room_change_on: 4 with 2/1 only, ticket > 0, no cutscene, no menu | hle.c:11343-11350 | [read] |
| room-change words left for player 2's door; find window left | hle.c:11263-11294 | [read] |
| quick spell refusals | hle.c:19774-19787, 12475-12481, 21090 | [read] |
| player 1's zone look refused | hle.c:20180-20188 | [read] |
| player 2's look and B off | hle.c:21000-21002 | [read] |
| player 2's Start refused | hle.c:27424, 27647 | [read] |
| player 1's turn: pad update and controller only | hle.c:19823-19906 | [read] |
| coop_p1_close_up's room bit only for player 1's own change | hle.c:19308-19316 | [read] |
| R4 bracket contents and end | hle.c:14059-14116 | [read] |
| stay_waits_clear, stay_busy_clear (scene or close-up only) | hle.c:25565-25612 | [read] |
| one-ticket rule; way out wait and release | hle.c:16760-16769, 16717-16759 | [read] |
| G1, G2 (waits on 8/16/32 and cutscene, +0 > 1, 900 cap, lock renewal for mover 1) | hle.c:24827-24976 | [read] |
| split_send keeps 102 only while split_on | hle.c's split_send | [read] |
| tw: world frames behind player 1's Tome; Tome's own pack and read-back passed over | hle.c:15360-15366, 13511-13525 | [read] |
| copy Tome's turn: the Tome's frame and the after-frame list only | hle.c:15060-15067 | [read] |
| copy close: cast and item script into the real game as player 2 | hle.c:27921-27990 | [read] |
| transition 2: countdown 12, no motion, 205 of 253 without a prompt | transitions.md:30 | [read] |
| SCRIPT_STATES = the waits table 0x805CAA40; player 2's kept apart | hle.c:15776, coop_states_swap | [read] |
