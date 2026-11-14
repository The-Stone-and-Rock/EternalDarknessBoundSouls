# Presses precedent: how the host already lets one player act while the other is busy (readers)

For the item "the stayer keeps every action through the other's door countdown" (the user, 2026-11-13). This is the
reuse map: each mechanism the host already has for "one player acts while the other is busy", what it covers, and
whether it can carry the stayer's casts, examines, pickups and Start through a door's countdown (busy 4 and the
ticket). Read only: no code changed, nothing built, the game not run.

Sources: runtime/host/hle.c (working tree, uncommitted, 30,141 lines; line numbers as it stands now),
`ed fn 0x80155738`, `ed dis 0x800084C0` and `ed dis 0x800069DC`, docs/coop-todo.md (steps A-F), docs/coop-failed-attempts.md
(rounds 4-5), design-split-hold.md §0, §3, §7, §8, §18-19, b3-input-lock-readers.md, stayer-readers.md.
[read] = seen at the address or hle.c line given. [inferred] = reasoned, not seen.

Words: the **mover** takes the door; the **stayer** stays. "The window" is the ticket's countdown (+0 of 0x8058AFFC > 0).
"Split" is both in one room; "apart" is not. `split_counting` (hle.c:24607) is the window with the stayer still in the room
they stayed in, whatever else is in the busy word; `coop_room_change_on` (11343) is the window with nothing but 4|2|1 in
the busy word.

## 0. In short

1. **Every precedent keys on one of two things: a host-kept set for player 2, or a host answer at one game ask.** Player
   2's own words, close-up and find (steps A-C) live in their own set and write nothing into the game's busy word.
   Player 1's things are the game's own (approach 1: "Player 1's objects ARE the game's memory",
   coop-level3-design.md:613) and always write the one busy word r13-23344.
2. **So the two directions are not alike.**
   - **Player 2 the stayer** (player 1's door): what refuses them is host code only. Four lines do it:
     - the look and B skip, `room_other` (hle.c:21000-21002);
     - the quick-cast `wait` (21090);
     - the quick-cast intercept (19776-19785);
     - two Start refusals (27424, 27647).
     The game's input lock is in player 1's copy, not theirs [read b3 §4]. Their own looks would run under the window
     with no busy bit, and the countdown would not wait for them [read 15873, coop-todo.md:119-122].
   - **Player 1 the stayer** (player 2's door): host refusals (20183, 19779) and **three game locks** in the world
     frame, where player 1's presses are dispatched (0x800084F4 → 0x80006A94 → the play channel's rows):
     - the input lock r13-23592, 65 frames, written into the game's copy (player 1's) by player 2's ticket [read b3 §4];
     - the shared Start lock r13-26232 [read b3 §3];
     - the door script's hold words: zones off and prompts cleared. coop_hold_open undoes r13-25696, r13-23040 and
       r13-24048 bit 4 for player 1's turn alone (19807-19810) [read].
     Player 1's walking turn (room_turn) runs with all of that let go. The press channel is not stepped in that turn
     [read 19823-19909, b3 §3].
3. **Each press has a precedent that carries it, with an extension.** None carries it as it stands, because every
   one of them was written to refuse inside a room change. That was right in follow mode, where the stayer is brought
   along. The map is in §3.
4. **The busy-ask cases carry the stayer's examine only if they accept the other player's bit 4.** `coop_p1_close_up`
   and `coop_p2_holds` accept the room bit only for their own player's door (19308, 19670). An action of the stayer's
   that adds bit 2, 8, 16 or 32 turns `coop_room_change_on` false. That switches off `split_busy_allows` (A1-A3),
   `coop_node_held_ask` and `room_other` all at once. The stayer's room then stands for their action, and the mover's
   countdown waits for it (G2) [read 24614, 19750, 20869, 24916].
5. **What no precedent covers.**
   - The one ticket record: a way out the stayer walks into is passed over by R4 (16761) and lost, unless it is kept
     by the way-out wait. The way-out wait is not triggered by a ticket today.
   - The mirror of R4b for player 2 the stayer (R4d, not built): player 1's arrival clears every script wait and busy
     bit, so a scene of player 2's begun in the window loses its waits.
   - The one busy word shared by the stayer's game-window action and the mover's door: the "busy word per player"
     row of coop-todo.md:21, not built.

## 1. What refuses the stayer today (the list the fix undoes)

| Press | Player 2 the stayer (player 1's door) | Player 1 the stayer (player 2's door) |
|---|---|---|
| zone look (prompts, way outs, zone words and scenes) | `coop_player_turn`: `g_coop_prompt = -1`, `coop_prompt_look` skipped while `room_other` (21000-21002) [read] | `coop_scan_player1` answers "nothing come into" while `coop_room_change_other(cpu, false)` (20183-20188) [read]; zones off by the door's hold in the world frame [inferred from coop_hold_open's writes, 19809] |
| B on a prompt | none shown (the look is skipped), so nothing to press [read] | play channel rows skipped by r13-23592 in the game's copy [read b3 §2, §4]; prompts cleared each frame while r13-24048 is not 0 (0x800084D8-0x800084EC) [read] |
| quick spell | `coop_quick_cast(cpu, room_other \|\| coop_left_behind)` drops the press (21090, 12477-12480) [read]; the intercept on 0x800069DC refuses too (19776-19785: `p2` from r13-21496, which coop_be_player2 makes theirs) [read] | the intercept on 0x800069DC (19779) [read]; the play channel's quick row skipped by the lock [read b3 §2] |
| Start / Tome | the copy's ask (27424) and `start_room` (27647) [read] | 0x80006CDC returns on r13-26232 (shared, 65) and on r13-25696 [read b3 §3]; "an intercept on player 1's Start 0x80006CDC in player 2's room change was never reached" (coop-failed-attempts.md:560) [read] |
| a use's script (doors, use prompts, locked words, Tome items) | `g_coop_use_script` is only set from a press their look made (21018-21021); with the look skipped it is never set [read] | runs in the game's script system from the game's prompt use (none reached) [inferred] |
| walking | free: `coop_node_held_ask` (19747) and their turn through `room_other` (20869) [read] | free: room_turn with the hold opened, from `g_coop_p1_free` (20342-20372, 20301-20304) [read] |

## 2. The mechanisms, one by one

### M1. `coop_busy_ask`: the four "not busy" cases (hle.c:19717-19739; intercept on 0x80155738)

- **What it does.** It answers 0 to every one of the 35 callers when one of these holds [read 19728-19729]:
  - (a) `g_coop_screen`, with `g_coop_words_theirs` or a busy word, and no cutscene or menu: player 2's close-up on the
    lend route;
  - (b) `coop_p1_close_up`: player 1's look;
  - (c) `p1_free`: player 1's words or find with no hold, under busy exactly 1;
  - (d) `coop_p2_holds`: player 2's own script hold or scene.
  Before those, `split_busy_kept` keeps the real word for the mover's filter (K1) and the K7 sites (19721-19722). After
  them, `split_busy_allows` answers 0 for A1-A3 (19733-19735) [read].
- **Bits.** Case (a) any. Case (b) 2, with 32, 1, 8 (theirs), 16 (theirs) and 4 for player 1's own door. Case (c) 1
  only. Case (d) 2, with 1, 32 and 4 for player 2's own door. A1-A3 only under `split_on`, that is 4|2|1 [read].
- **Who asks.** None of the 35 sites is in the press paths. The play channel (0x80006A94), the prompt chooser
  (0x80117628), the quick-cast handler (0x800069DC) and the menu opener (0x80006CDC) never ask it [read the site list:
  `ed fn 0x80155738`; 0x800069DC's gates are its own, `ed dis`].
- **Can it carry the stayer's presses?** Not by itself: presses do not ask it. It decides whether the stayer's room
  and the stayer stand while the stayer's action is up. It is the half that keeps the room going once the stayer has
  pressed. With a **small extension** it carries that half (M2, M3). K1 and K7 come first, whatever the case, so the
  mover is protected in every case (19721) [read].

### M2. `coop_p1_close_up` / `coop_p1_in_close_up` (hle.c:19297-19335, 19247-19258)

- **What it does.** Player 1's look (`g_coop_p1_look`) holds only them. The busy ask answers 0 (M1 case b), and
  `hle_coop_controller` opens the hold for player 2's turn (`coop_hold_open`, `g_coop_p1_look_open`, 20318-20321,
  20350-20353) [read].
- **Bits.** It reads the look as on while busy, less 32, 1, the room bit, 8 (player 2's words) and 16 (player 2's find),
  is exactly 2 (19310). The room bit counts only when `coop_room_change_other(cpu, true)`, that is, **player 1's own
  door** (CLIMB DOWN from a look of theirs) (19308) [read].
- **In player 2's door.** Player 1's close-up gives busy 2|4 with no room bit allowed, so `held` is false.
  - Begun before the ticket, the look reads "over" at the next ask: `g_coop_p1_look = on = false` (19313-19318).
  - Begun inside the window, it never reads on.
  - Either way nothing answers 0, and the world stands for it. design-split-hold.md §7 has the same row [read code;
    consequence inferred].
  - There is a tension with R4b. `stay_busy_clear` and `split_r4b_case` key on `coop_p1_in_close_up`, which is just
    `g_coop_p1_look` plus its kind. That flag may already have been cleared by the "over" path [inferred; to check in
    a run].
- **Can it carry the stayer?** **Small extension.** Also allow `COOP_BUSY_ROOM` when the window is the other player's
  (`split_counting` with `g_split.mover == 2`). Player 1's examine, DETAIL, words and find then hold player 1 alone,
  the room plays on (M1 b), and K1/K7 still keep the mover's word. The look must not open the hold for the mover's
  turn: player 2 is in state 121 with door turns. `p1_look` would take that branch (20318). It needs a guard
  [inferred].

### M3. `coop_p2_holds` and the hold-opened turn (hle.c:19660-19672, 20322-20337; coop_hold_open/shut 19796-19820)

- **What it does.** Player 2's own script hold (`g_coop_script_hold`), or their scene (`coop_p2_scene_on`), holds them
  alone. The busy ask answers 0 (M1 d). Player 1 gets a turn with the hold let go: `coop_hold_open` keeps the three hold
  channels' counts and the words r13-23344, -25696, -23040 and -24048, then writes busy 0, play hold 0, zones on and
  24048 & ~4; `coop_hold_shut` puts them back [read].
- **Bits.** Exactly 2, with 1 (`COOP_BUSY_SCRIPT`), 32 (a scene) and 4 only for player 2's own door
  (`coop_room_change_other(cpu, false)`, 19670) [read].
- **In player 1's door, with player 2 the stayer in a scene.** Their scene's fn22 sets 32 and 2. `coop_room_change_on`
  goes false, so the room bit is 0. Busy 4|32|2 less 1|32 is 6, not 2, so the answer is false. Nothing answers 0, A1-A3
  are off (`split_on` false), and both rooms stand. G2 waits for the scene (24916-24918) [read code; consequence
  inferred].
- **Can it carry the stayer?** **Small extension**, as M2: the room bit also when the window is player 1's door and
  player 2 the stayer (`split_counting`). `coop_hold_open` is the tool that frees a held player's channels for a turn.
  It is already used three ways: player 2's turn in player 1's look, player 1's turn in player 2's hold, and player 1's
  turn in player 2's room change. A fourth, keyed on the window, is the same call [read 20318, 20328, 20365].

### M4. `coop_node_held_ask` (hle.c:19747-19766; intercept on 0x801F68D4)

- **What it does.** During `coop_room_change_on` it answers "not held" for the non-asker's player node only. It never
  writes [read].
- **Bits.** 4, with 2 and 1 only [read 11346-11348].
- **Can it carry the stayer?** As it is, it carries the stayer's body: walking, swinging, a cast already under way.
  The moment the stayer's own action adds 8, 16 or 32, `coop_room_change_on` is false and it stops answering. That is
  harmless while the game's own hold of that action holds the stayer, as in solo. It matters for a stayer action that
  must not hold its owner, such as player 2's own looks: those add no bits, so it keeps answering [read 19750; inferred].
  **Small extension if needed:** key it on `split_counting` instead, and keep the "asker as the game has them" line
  (19755).

### M5. room_turn: player 1's turn through player 2's room change (hle.c:20338-20372; coop_player1_turn 19823-19909)

- **What it does.** After player 2's turn, player 1 gets a turn with the hold opened. It starts from their own input
  state as it was on their last free frame (`g_coop_p1_free`, saved at 20301-20304 while not busy; the doorhold fix,
  coop-failed-attempts.md:546) [read]. Inside it, their words, focus and masks are let go (19864-19869). The game's pad
  update 0x8013E518 runs with their own copy in, and their controller 0x80045BC4 runs (19870-19887) [read].
- **States.** Only while `coop_room_change_other(cpu, false)` and not p1_look and not p2_hold (20342) [read].
- **Can it carry the stayer's presses?** **Not as it is.** The turn runs the controller, the walking channels only.
  Player 1's prompt B, Start and quick spell are rows of the play channel r13-26224. That channel is stepped only from
  the world frame (0x800084F4 → 0x80006A94), with the game's copy in, outside the turn [read b3 §3; `ed dis 0x800084C0`].
  The game's copy holds player 2's ticket lock r13-23592 (65 frames), so the rows are skipped [read b3 §2, §4].
  - **Extension A (the lock where it belongs).** The precedent is `coop_events_words` (21902-21921). It swaps player
    2's input copy in (`coop_state(&g_coop_state[1])`) round their behaviour's events, so that 0x800443E4 from their
    FINISH HIM locks their copy and not player 1's. "Player 1 stood 200 to 330 frames" is the comment at 21889 [read].
    The same swap round player 2's ticket writer call would put the 65-frame lock into player 2's own copy, which b3
    §6 says is where it belongs.
    - r13-26232 is in no copy. It would need a keep and restore for player 1 round the same call [inferred].
    - The door's hold words (25696, 23040, 24048 bit 4) are still in the world frame. M7's zones-on precedent covers
      the look. Start's 25696 gate is covered by M8's copy route [inferred].
  - **Extension B.** Step the play channel and the prompt chooser inside room_turn with the hold open. This is a
    second call of world-frame functions in one frame, and r13-26232 would count down twice. It is riskier than A
    [inferred].

### M6. `room_other` and `coop_door_turns`: player 2's turn while busy (hle.c:20869-20871, 16627-16629)

- **What it does.** Player 2's turn is refused while busy (20870), except in their own door's fade (`coop_door_turns`,
  120 frames) or when they are the non-asker of player 1's change (`room_other`). Inside the turn, `room_other` then
  turns off their look and B (21000) and their quick casts (21090) [read].
- **Bits.** `room_other` is only 4|2|1 (`coop_room_change_on`) [read].
- **Can it carry the stayer?** **Small extension.** The turn already runs for player 2 the stayer. Dropping the two
  `room_other` refusals under `split_counting`, co-op and stay, lets `coop_prompt_look` run. Then:
  - their own EXAMINE, DETAIL, words, PICK UP and find (steps A-C) open in their own set with no busy bit [read 15873;
    coop-todo.md:101-122];
  - their own close-up survives player 1's change: it is cancelled only when their zone list changes
    (`coop_zones_new`, 16208), and the stayer's room does not change [read; inferred that the list stays];
  - a prompt with a script (a door, a use) goes to `g_coop_use_script` → `coop_use_start` (M10). That writes game
    bits, so `room_other` goes false and the next turn is refused as busy (20870) until the use ends. The use's
    close-up then runs on the lend route (`g_coop_screen`), which has its own pad path [read 20449-20470, 27690-27760].
  A stayer use that adds 8/16/32 inside the window refuses player 2's turn for its length: the same shape as the
  cutscene note in coop-todo.md:17 ("player 2's turn is refused while the busy word is set").

### M7. Player 1's own look beside player 2's scene or hold (hle.c:20186-20232, 20236-20258)

- **What it does.** While player 2's scene, close-up, words or hold is up (`coop_way_out_p2_up`, 16691-16694), player
  1's own game look runs from the host. It turns the zones word r13-23040 on for the scan when player 2's scene had it
  off (20208-20211, 20220-20225). It marks a way out it comes into as waiting (`coop_way_look_begin(..., true)`).
  Player 1's look in player 2's close-up is run the same way (`g_coop_p1_scan`, 20236-20258) [read].
- **Can it carry player 1 the stayer's zone look?** **Small extension.**
  - Drop the refusal at 20183 under `split_counting` (player 2 the mover).
  - Add "player 2's ticket counting" to `coop_way_out_p2_up`, and to the zones-on test at 20209
    (`coop_p2_scene_on()` today), so the door's hold does not leave their zones off.
  - A way out they walk into waits (M12) instead of being passed over and lost by R4.
  [read the branch; extension inferred]

### M8. Start: the Tome copy (tome_copy_step 14922-15000; ask sites 27424, 27647, 27728-27735, 27800-27808)

- **What it does.** With ED_COOP_TOME_COPY=2 (`tome_copy_mode() == 2`), player 2's Tome is a copy of all RAM, run in
  turns of its own while the real world goes on. It opens while the game's mode is play (`world_stays`: mode 2 or 3,
  no cutscene, 14927) [read].
  - A copy opened before a ticket "stays whole through it" (run-20261106-101418, cited at 27644-27646) [read the
    comment].
  - The refusal in a room change is follow mode's: the copy "showed black in their half once the new room was in"
    (27643-27645) [read]. With the stay, the stayer's half does not change room.
  - **Player 1 already opens a copy of their own** when the game's pad is lent away. Their Start in player 2's
    close-up (`g_coop_screen`) is read from their raw pad `g_tome_copy_p1_pad` (set at 4338), outside the game's
    locked channels, and sets `g_tome_copy_p1 = g_tome_copy_ask = true` (27803-27809) [read].
- **Bits.** None. The copy never reads the busy word to open. Step D refuses it under player 2's own words, close-up
  or find (`coop_p2_own_up`, 27425, 14964) [read].
- **Can it carry the stayer's Start?**
  - **Player 2 the stayer: small extension.** Drop `!coop_room_change_other(cpu, true)` at 27424 and `start_room` at
    27647 under `split_counting`. Keep step D's refusal. A run must show the copy's close finds the stayer's room whole
    (stayer-readers.md Q8).
  - **Player 1 the stayer: small extension of the same route.** Add "player 2's ticket counting, player 1 the stayer"
    beside `g_coop_screen` at 27805, so player 1's Start opens their copy, as it already does in player 2's close-up.
    This sidesteps r13-26232, r13-25696 and the lock, which the game's own Start meets [inferred]. The real Tome for
    player 1 in a window (the game's mode with the change firing behind it) stays out: stayer-readers.md §2.9 calls it
    a design of its own.
  - A Tome item used from either copy leaves through `tome_copy_left` (27921) to the use route (M10), with its busy
    bits. G2 then waits [read 24916; inferred].

### M9. Player 2's own set: words, close-up and find (steps A-C; coop_prompt_look 16800-16870; 15873-15885; 19550-19575)

- **What it does.** Their window's words, bindings, find byte, look record and camera set are host memory, swapped in
  only round their own calls. Their holds run in their input copy, with the game's busy word and the hold words put
  back (`coop_words_hold`, `coop_play_hold`'s own branch, `coop_find_hold`'s own branch at 19550-19575: "busy bit 16
  and r13-24048's bit 2 are never written") [read 19563-19574; coop-todo.md:101-122].
  - Player 1's script timers run on through their find (`coop_flags_ask`, 19628-19641) [read].
  - Their quick cast is refused under their own window (12482-12486), and so is their Start (27425) [read].
- **Can it carry player 2 the stayer's examine and pickup?** **As it is, once M6's skip is lifted.** It writes nothing
  the window reads.
  - `coop_room_change_on` stays true, A1-A3 stay on, the room plays on, and G2 does not wait (no extra bits, 24916)
    [read; inferred].
  - What the window's end touches: player 1's arrival clears every script wait (0x80163F04(1)) and every busy bit. Their
    own set holds neither: their close scripts are a host queue (`close_scripts`, run at player 1's controller,
    20385-20431) [read; inferred that the arrival misses them].
  - This is the cheapest path in the map.

### M10. The lend route: `coop_use_start`, `g_coop_screen`, `coop_their_close_up`, `coop_no_pause` (18182-18240, 19370-19386, 19343-19368)

- **What it does.** A script of player 2's runs in the game's one script system as "a use". This covers a door, an
  exit, a use prompt, a locked door's words, a passed-over examine's script, a Tome item's script, cast words and
  timers. The player words are swapped by `coop_be_player2`, and so are their room (`coop_stay_room_in`), kinds,
  states, zones and cameras (lent). The coop_state input copy is **not** swapped (18191-18230) [read; b3 §4].
  - A close-up it opens is theirs on the game's pad (`g_coop_screen`).
  - Its words and hold bits stay in the game's word for the host's asking, with the play hold let go
    (`coop_script_hold_go`).
  - The busy ask answers 0 (M1 a), and player 1 gets their own turn (20275-20294) [read].
  - `coop_no_pause` is the mirror: player 1's words or find with no hold, player 2 playing on (19380-19386, M1 c).
- **Bits.** 2, 8, 16, 1, all in the one word [read].
- **Can it carry the stayer's uses through the window?** **Only with G2's wait, as built.** A use writes game bits, so
  `coop_room_change_on` is false. G2 then waits for it while +0 > 1, up to `SPLIT_WAIT_LONGEST` (24916-24931). For
  player 1 the mover, G2 renews the mover's lock each waiting frame (24937-24941) [read].
  - So the stayer's door-like uses work, and the mover waits in the black. For a prompt door of the stayer's, the R4
    rule passes the second fn66 over (16761-16768) [read].
  - The use's own ticket is the one-record problem (§4). Without G2's wait, a lend close-up would straddle the
    firing: the reason G2 exists (design-split-hold.md §3.9).
  - `coop_use_start` does not swap the input copy. A stayer's use that locks input would lock player 1, the same
    defect as the door's lock (M5 extension A) [inferred].

### M11. Kept presses: `coop_press_keep`, player 2's kept B, `coop_p1_press_kept`, `g_coop_p1_b_waits` (19268-19296, 21039-21065, 27317-27370, 27812-27816, 27701-27712)

- **What it does.** A B on one's own prompt while the other's look, scene, hold or close-up is up is kept. It is
  pressed by the game's own path once the other's thing has been over for 10 frames (`COOP_PRESS_SETTLE`). It is let
  go when the player walks off the prompt (6 frames), when the prompt is not shown within 180 frames, or
  **when any room was asked for** (`kept->tickets != g_coop_tickets`, 21046 and 27340) [read].
  - Player 1's B is read from the raw pad in `coop_menu_input` (27317). It is kept while `coop_p2_scene_or_hold`,
    which includes player 2's use (`g_coop_use.active && g_coop_use.p2`) and the lent cameras (27293-27296) [read].
- **Can it carry the stayer's B?** **As a fallback only, with a small extension.** It defers: a press inside the
  window goes in about 62 + 10 frames later. That is not "exactly as at any other time".
  - The tickets test works for a press kept inside the window, since g_coop_tickets was already counted at the ticket
    (11363). It drops a press kept before the ticket [read; inferred].
  - Extension: count only tickets that move the keeper (a `g_coop_p2_tickets` beside `g_coop_p1_tickets`, 11373-11374).
  - Use: the stayer's door-like prompts (a way out or a door of their own inside the window), where acting at once
    would ask for a second ticket.

### M12. One ticket at a time, and the way-out wait (coop_script_go_to_room 16760-16798; coop_way_out_wait/go 16716-16758)

- **What it does.** Under the stay, a second fn66 while a ticket counts is passed over with r3 = 0: "the second waits
  for a new press" (16761-16768) [read].
  - Separately, a way out walked into during the other's scene, words, find, close-up or hold is remembered by zone
    (`g_coop_way_out[who]`). Its zone is marked "not come into" once the other's thing is over, so the next look
    fires it by the game's own path (16716-16758) [read].
- **Can it carry the stayer's way outs?** **Small extension.** Today the R4 pass-over loses the way out until the
  player steps out and back in (stayer-readers.md §2.7). The wait is set only when the look began with the other's
  thing up (`waits`, 16778-16780), and the `*_up` predicates do not include a ticket.
  - Adding "the other's ticket counting" to `coop_way_out_p1_up` and `coop_way_out_p2_up` turns the R4 pass-over into a
    remembered wait, released after the change.
  - The release's `tickets` test keeps a way out recorded inside the window (same count) [read 16742; inferred].

### M13. R4b/R4c: the stayer's scene or close-up kept over the mover's arrival (stay_waits_clear 25565-25588, stay_busy_clear 25590-25610, stay_scene_look_again 17424, split_r4b_case 24898, g_coop_p1_tickets 11373-11374)

- **What it does.** Only inside player 2's bracket (`g_stay_bracket != 0`), that is, player 2's door:
  - at the arrival's 0x80163F04 the waits of scripts not player 2's are put back;
  - at the arrival's busy_set(-1, 0) player 1's bits but 4 are kept when their scene or close-up is on;
  - player 1's scene look is re-armed;
  - player 1's scene is cut only by tickets that move player 1;
  - G2 does not wait in this case (`split_r4b_case`, 24898, 24916) [read].
- **Can it carry the stayer's actions?**
  - **Player 1 the stayer: as it is,** for a scene or close-up that is up when the change fires, subject to M2's
    tension (the look may already read "over").
  - **Player 2 the stayer: not at all today.** The mirror (R4d: `g_coop_p2_tickets`, waits and scene bits kept at
    player 1's arrival) is not built [read: no R4d in hle.c; stayer-readers.md §2.11; design-r4d.md].
    - Player 2's scene records are cut by any ticket (`g_coop_tickets`, 17183-17187, 17195-17199).
    - A scene they begin inside the window has the new count and survives to the arrival. There it loses its waits and
      bits [read; inferred].

### M14. The split hold's guards (G1 24826-24846, the request hold 24889-24898, G2 24905-24944, G3 24803)

- **What they do.**
  - G1 refuses a second ticket inside the window.
  - The request dispatcher's room requests are held until after the change.
  - G2 stops the countdown while bits 8, 16 or 32 (not in `word0`), or a cutscene, begun in the window are up.
  - G3 opens no window for a stayer already casting [read].
- **Can they carry the stayer?** **They are what makes the presses safe; they carry nothing themselves.**
  - G2 is the rule that already lets the stayer's game-window action (M10, and M2's extended close-up of player 1's)
    finish before the firing.
  - G1 and the request hold catch what a stayer's cast or scene might ask for.
  - G3 is a refusal at the window's open. It should stay. With casts let through, it should also not be needed inside
    the window: a cast begun in the window meets G1 and G2 [inferred].

### M15. `coop_player_held` (hle.c:19677-19686)

- **What it does.** It names who the game holds (words, find, close-up, script hold, Tome, own set), so creatures do
  not target them (event 59, `coop_enemy_target`; S3 picks nobody for a held stayer) [read].
- **Can it carry the stayer?** **As it is.** It already counts player 2's own set (19684-19685) and both Tome copies.
  A stayer in a close-up inside the window is left alone by the room's creatures, as at any other time [read].

### M16. Player 2's quick cast beside player 1's (hle.c:12475-12530)

- **What it does.** Their cast runs the game's own 0x800069DC as them (`coop_be_player2`, which also swaps the casting
  words, 12356-12361). Their carry list is lent from player 1's for the cast. When their cast asks for their Tome while
  player 1's cast has the record (`beside`, 12506), the real record is put back and the copy takes theirs
  (`g_tome_copy_ask`) [read].
- **Can it carry player 2 the stayer's quick spell?** **Small extension:** drop `room_other` from the `wait` at 21090,
  and the intercept's refusal at 19779, under `split_counting`. Keep `coop_left_behind`, which is false for a resident
  room (16248-16251), and the step-D refusal [read].
  - Which spells reach a ticket, a scene or a cinematic was not read (stayer-readers.md Q6). G1 refuses a ticket in the
    window and G2 waits for a scene or cinematic, so those are caught [inferred].
  - **Player 1 the stayer:** the same intercept line, plus the lock (M5 extension A).

## 3. The reuse map (what carries each stayer action, per direction)

| Stayer action | Player 2 stays (player 1's door) | Player 1 stays (player 2's door) |
|---|---|---|
| Body: walk, fight, a cast under way | M4 + their turn (`room_other`): works today | M4 + M5 room_turn: works today |
| Room plays on while they act | A1-A3 stay on for M9 (no bits). For a scene or use: M3 / M10 with the room bit at `split_counting` | M2 with the room bit at `split_counting`; guard p1_look's hold-open for the mover |
| Zone look and prompts | M6: lift `room_other`'s skip (21000) → M9's own looks | M7: lift 20183, zones on, way outs wait (M12) |
| B: EXAMINE, DETAIL, words, PICK UP | M9 as it is | the game's own close-up (player 1's objects are the game's). Needs the lock out of their copy (M5 A); then M2 + G2 |
| B on a door or way out | M12 (way out waits) or M11 (kept, small extension); one ticket record | the same |
| B on a use prompt (a script) | M10 + G2's wait | the game's prompt use + G2 |
| Quick spell | M16: lift 21090 and 19779 | 19779 + M5 A (the lock) |
| Start / Tome | M8: lift 27424, 27647 | M8: player 1's copy as in player 2's close-up (27805), keyed on the window |
| A Tome item used | `tome_copy_left` → M10 → G2 | the same |
| Their action survives the arrival | own set (M9): untouched; a scene: R4d missing (M13) | M13 R4b/R4c (check M2's tension) |
| Creatures leave a held stayer | M15 as it is | M15 as it is |

Order, if built: M5 A (the lock, player 2's door) → M2/M3 room bit → M6/M7 look lifts with M12 → M16 casts →
M8 Starts → R4d (M13 mirror). Each change is behind `g_settings.mod_coop && g_coop_p2 && coop_stay() &&
split_counting(cpu)`, so solo and follow mode keep every refusal.

## 4. What no precedent covers

- **The one ticket record.** A stayer door inside the window cannot have its own ticket. G1 refuses one; R4 passes the
  fn66 over. M11 and M12 defer it; nothing acts on it at once [read 24826, 16761].
- **The one busy word.** Player 1 the stayer's game-window action and the mover's door share r13-23344. The extensions
  above tolerate bit 4 beside the action's bits; they do not split the word. The arrival's busy_set(-1, 0) clears both
  players' bits, and only R4b keeps player 1's. coop-todo.md:21 names the per-player word as the real fix, not built
  [read].
- **r13-26232 is shared.** Every route through the game's own Start meets it while either player's ticket counts.
  M8's copies avoid it [read b3 §3; inferred].
- **G2 makes the mover wait for the stayer's game-window actions.** Player 2 the mover has no lock renewal (24942) and
  120 frames of door turns (`COOP_DOOR_FADE`). A long close-up of player 1's under player 2's door could outlast the
  turns. The 50-frame line tells (24942-24946) [read].

## 5. Facts read

| Fact | Where | Mark |
|---|---|---|
| Busy ask: K1/K7, four cases, then A1-A3 | hle.c:19717-19739 | [read] |
| No press path asks the busy word | `ed fn 0x80155738` site list; 0x800069DC's own gates | [read] |
| Player 1's look: room bit only for their own door | hle.c:19308-19310 | [read] |
| Player 2's hold: room bit only for their own door | hle.c:19670-19671 | [read] |
| `coop_room_change_on`: 4 with 2 and 1 at most, ticket > 0 | hle.c:11343-11349 | [read] |
| `split_on` = `split_counting` && `coop_room_change_on` | hle.c:24607-24614 | [read] |
| Held ask: non-asker player node only | hle.c:19747-19766 | [read] |
| Hold open/shut: 3 channels, 4 words | hle.c:19791-19820 | [read] |
| room_turn from `g_coop_p1_free`, hold opened | hle.c:20296-20304, 20338-20372 | [read] |
| Player 1's turn runs pad update and controller only | hle.c:19823-19909 | [read] |
| Play channel stepped only in the world frame | 0x800084F4 → 0x80006A94 | [read `ed dis`; b3 §3] |
| Prompts cleared each frame while r13-24048 is not 0 | 0x800084D8-0x800084EC | [read] |
| Quick cast 0x800069DC sends event 185 to the current id | 0x800069F0-0x80006A5C | [read] |
| Player 2's turn refused while busy, unless door turns or `room_other` | hle.c:20869-20870 | [read] |
| Look and B skipped by `room_other` | hle.c:21000-21002 | [read] |
| Quick casts wait on `room_other` | hle.c:21090; 12475-12480 | [read] |
| Quick-cast intercept refuses the non-asker | hle.c:19773-19786 | [read] |
| Player 1's zones refused in player 2's change | hle.c:20183-20188 | [read] |
| Player 1's own look turns zones on in player 2's scene | hle.c:20196-20232 | [read] |
| Player 2's Start refused in player 1's change | hle.c:27424, 27647-27651 | [read] |
| Player 1's Tome copy from their raw pad in player 2's close-up | hle.c:27800-27809; pad at 4338 | [read] |
| Copy opens only in play mode | hle.c:14927-14937 | [read] |
| Input copy swapped for player 2's events (FINISH HIM's lock) | hle.c:21882-21921 | [read] |
| `coop_use_start` swaps no input copy | hle.c:18182-18240 | [read] |
| Own find writes no busy bit 16 or 24048 bit 2 | hle.c:19550-19575; coop-todo.md:122 | [read] |
| Own close-up cancelled only on a new zone list | hle.c:16203-16208 | [read] |
| Kept presses dropped on any ticket | hle.c:21046, 27340 | [read] |
| One fn66 at a time under the stay | hle.c:16760-16769 | [read] |
| Way out waits only when the look began with the other's up | hle.c:16696-16758, 16776-16786 | [read] |
| R4b/R4c guards only in player 2's bracket | hle.c:25565-25610 | [read] |
| Player 2's scenes cut by any ticket; player 1's by their own | hle.c:17150-17160, 17183-17199 | [read] |
| G2 waits for bits begun in the window, not in R4b's case | hle.c:24898-24944 | [read] |
| `coop_player_held` counts own set and both copies | hle.c:19677-19686 | [read] |
| Player 1's Start intercept never reached in player 2's change | coop-failed-attempts.md:560 | [read] |
