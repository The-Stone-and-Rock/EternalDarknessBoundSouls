# R4d reader: player 1's change while player 2's scene runs in another room (2026-11-12)

Read only: runtime/host/hle.c (line numbers as of this reading), the disassembly (`ed dis` / `ed fn`), the earlier
readers (doors-readers.md, stay-readers.md, arrival-readers.md), docs/coop-failed-attempts.md, and the user's two
rooms-play recordings. Nothing was edited, built or run. Facts are marked [read] (address or file:line) or [inferred].

"Apart" below means ED_ROOM_SLOTS=2, ED_COOP_STAY=1, Mods > Co-op on, and the two players' object+592 rooms differ and
both are resident (coop_stay_apart / r6_apart). "Player 2's scene" means any of: their fn22 scene (g_coop_p2_scene,
coop_p2_scene_on), a scene begun as player 1 under their screen (g_coop_scene_p2s), their script hold
(g_coop_script_hold), a use of theirs on the screen route (g_coop_screen, cameras lent), and their own-copy words /
close-up / find (g_coop_p2_words, g_coop_p2_look, g_coop_p2_find, steps A-F).

---------------------------------------------------------------------------------------------------------------
## 0. Short answer

1. **Today the case is mostly gated off, not handled.** Player 1 cannot normally start a door while player 2's scene or
   hold is up, apart or together:
   - Player 1's B on a door prompt is *kept* (not run) while `coop_p2_scene_or_hold` (hle.c:25764: p2 scene, p2 holds,
     script hold, p2 use, cameras lent, screen) is true, and pressed for them only once player 2's is over
     (coop_p1_press_kept, hle.c:25788-25850) [read]. The reason given in the code: the game's one hold hides player 1's
     prompts, so the press would find nothing (hle.c:25760-25763) [read].
   - Player 1's walk-in way out *waits* (coop_way_out_wait) while `coop_way_out_p2_up` (hle.c:16435-16438: p2s scene, p2
     scene, screen, words theirs, p2 holds, own-copy words, own close-up, game words) is true: the game's own look of
     player 1 runs as a waiting look (coop_scan_player1, hle.c:19900-19945) [read]. Nothing in either gate tests
     coop_stay or the rooms: it applies apart too [read].
   - These waits exist because a room change mid-scene froze both players for good (r13-25696 left 1:
     docs/coop-failed-attempts.md:514, p1scene 3, REJECTED; hle.c:18990-18993's comment) [read].
2. **Paths that still reach a change of player 1's during player 2's scene apart** [read where cited, else inferred]:
   - fn66 from a step of player 1's own script that is not inside a look or a press: their own scene's later step,
     a timer (0x80163E00), a cinematic's end, a Tome item's script. coop_script_go_to_room (hle.c:16504-16540) only
     refuses inside a waiting look (g_coop_way_look.waits) or a second change on its way [read].
   - Player 1's door while player 2 is in their **own-copy** words / close-up / find (steps A-F): those don't touch the
     game's busy word or prompt flags (CoopWordsOwn comment, hle.c:15566-15575; coop_play_hold own branch,
     hle.c:13047-13067) [read], and `coop_p2_scene_or_hold` does not list them [read 25764-25767], so player 1's
     prompt press goes through [inferred: the shown prompt is not hidden then].
   - The planned "Bring X to Y" teleport (the user's request) will be a host-made change of either player at any time
     [inferred].
   - Lifting the two waits when apart (so player 1 can leave a room while player 2 is in a scene elsewhere, as player 2's
     prompt doors already can beside player 1's scene: R4b's case) [design choice, not built].
3. **When player 1's change does run beside player 2's scene, it breaks it the way R4b's case broke player 1's, plus one
   host-only cut that R4b did not have:** the host forgets player 2's scene at the TICKET (g_coop_tickets++,
   hle.c:11300; coop_p2_scene_on / coop_scene_p2s_on cut on any ticket, hle.c:16927-16943), ~62 frames before the
   arrival, so a straight mirror of stay_busy_clear keyed on coop_scene_on_of(1) would never fire (it is false by then)
   [read]. The mirror needs a player-2 counterpart of g_coop_p1_tickets first.

---------------------------------------------------------------------------------------------------------------
## 1. The game's change for player 1's own door, in order, and whose data each step reads

Runner: the world tick 0x800083EC -> 0x80155830 -> 0x80152614(room, entry, camera, script, flags), outside any turn, so
r13-26524 / -21492 / -21496 are player 1's (doors-readers.md:3, :132) [read]. For player 1's change the host opens NO
bracket: notify_ticket_run returns unless g_coop_room_p2 (hle.c:13858-13860) [read], so g_stay_bracket == 0 and every R4b
intercept passes through (stay_waits_clear hle.c:24404, stay_busy_clear hle.c:24429) [read]. The camera set in is
whatever g_coop_set_in is (1 at rest; 2 while lent: coop_cameras_lend, hle.c:15461-15467) [read].

| Step | Address | What it touches | Global or per room | Player 2's scene state hit? |
|---|---|---|---|---|
| fn66: let the player go, motion, ticket | 0x801666A4: 0x801F6828(player,0), 0x80155770 busy\|=4 and event 102 from player 1 (0x801556CC(4,1) -> 0x80155720) | busy word; +28 bit 8 of every node but the sender | global | **yes**: player 2's node held (bit 8) for the countdown [read doors-readers.md:124; dis 0x801556CC] |
| host at the ticket | notify_room_ticket hle.c:11297-11330 | g_coop_tickets++ (11300); g_coop_p1_tickets++ (11311-11312, player 1's change); g_fade_mover=1 (11314) | host | **yes**: coop_p2_scene_on, coop_scene_p2s_on (16927-16943), g_coop_way_out[1] (16488, 16560), g_coop_p2_kept (20755), g_coop_use.tickets (17988) all read a changed g_coop_tickets as "a room was asked for" [read] |
| countdown | 0x80155830 | ticket +0 | global | busy has 4 on top of player 2's 32\|2: coop_room_change_on wants `busy & ~7 == 0` (hle.c:11281-11286), false with bit 32, so coop_node_held_ask (hle.c:19458-19479) does not answer "not held" for player 2 [read] |
| 0x80152298 head | 0x801A0EAC, 0x801A05EC(10,1,0) (sound; R5 keeps the stayer's room: coop_sound_change_act hle.c:14084, coop_sound_stream_drop 14137) | sound tracks (+132 room) | per room via R5 | no (R5 keeps player 2's room's tracks) [read] |
| load | 0x80154C94 (room_slots_return swaps the indices so player 2's slot is kept, hle.c:14343-14351) | slot table, stamp of r13-26524's object +592 | per slot | no [read] |
| arrival 0x80151FDC | see below | | | |
| 0x80151FDC: 0x801E1CFC(1), 0x801E1B74(record+0x813C), 0x801DFA7C(0), 0x80117260(0x10000,0), 0x80138D84 | texture bank, prompt flag bit 0x10000 only | global | no scene word: fn22's prompt bit is 1, not 0x10000 [read dis 0x80152024-0x80152048, 0x800429F4-0x800429FC] |
| 0x8019F3F0, 0x8019E068(0/1,100,0) | sound groups (un-duck) | global | player 2's duck apart is already passed over (hle.c:14125-14131); the reset itself passes for player 1 and runs coop_music_replay(new room) (hle.c:14112-14118) [read] |
| 0x800BA530(0), 0x80043A5C(record) | strip walk; stayer spared (coop_stay_strip_walk hle.c:14355) | per node | no [read] |
| r13-26508 == 0 (another room): direct event 61 to player 1 (0x801520DC), 61 to r13-23288 = the OLD room's nodes (0x801520F4), 0x80044428, 0x80157604, 0x80044748 (torch walk over the new room), 0x801EB550 | old room A's nodes; new room's torches; **0x801EB550 zeroes +108/+124 of the live camera records 0x80602B18 / 0x80603178** | per room (61, torches); camera records = the set in | camera: only if player 2's set is in (lent) [read dis 0x801EB550] |
| flags bit 4 clear: 0x801172B0 | the game's six prompt slots, r13-29016 := -1, 0x80116E70 x6, 0x80117050 -> 0x80024CDC -> 0x80024B24(0) the close-up cancel on the game's look record | the slots/look in memory | only if player 2's slots/look are in (lent / g_coop_screen) [read dis 0x801172B0; doors-readers.md:95, :138] |
| **0x80163F04(1) at 0x8015213C** | zeroes every entry of the 10x8 table 0x805CAA40 whose +6 keep is 0 | the game's table (player 1's states at rest; player 2's are in g_coop_states.p2 except round their own calls) | **yes, for player 2's scripts whose words are in the game's table** (below) [read dis 0x80163F04-0x80163F6C] |
| flags bit 8 clear: 0x80153998 -> 0x801E0170/0x801E01D8/0x801E055C | current slot's record | per slot (new room) | no [read] |
| **0x801556CC(-1, 0) at 0x80152184** | busy word := 0 (andc; no event 102 when the result is 0) | global | **yes: player 2's 32, 2, 1, 8, 16** [read dis 0x801556E0-0x801556F8] |
| 0x801F6828(r31, 1) at 0x80152190 | re-holds r31 = 0x801F62B0() taken at 0x80152014 = player 1's node | per node | no [read] |
| 0x801E7C88(.., 10) fade in | fade state | global, marked per half by R4 (g_fade_mover=1) | no [read] |
| r13-23280 test (0x801521AC) | fn22's scene word (set by 0x80152204 from fn22 at 0x800429C0): when 0, a bit r13-23284 is set in the bitset r13-23328 (0x801DC1A4) | global | **read, not written**: player 2's scene on means player 1's arrival skips that bit [read; meaning of the bitset not read] |
| 0x80152614 after 0x80152298 | placing of r13-26524's object, 238/191, heading, 0x80043380, 0x800BA530(0), 0x800428D0 (-> 0x80039FF4), 0x800428F0 (-> 0x800647C8), event 62 to r13-23276's nodes (0x80152780, when r13-26508 == 0), the arrival camera 0x801EE998(hash,2,0) / 0x801EE764 on eye record 2 / **0x801ECCE0: r13-21700 (CAMERA_DEPTH) := 0** / 0x801EEB58(2) / 0x801EEA9C, then the arrival script 0x801642D4 at 0x801527F4 | new room's nodes (62); the set in (camera, depth) | per room (62); camera = set in | 62: only when player 1 arrives in player 2's room (below). Camera/depth: only if player 2's set is in [read dis 0x80152740-0x801527F8, 0x801ECCE0] |
| notify_behaviour_pass -> coop_stay_change_end | returns at once (bracket 0) hle.c:13870-13872 | | | nothing restored [read] |
| notify_room_slots_tail (0x80154B54 at lr 0x80152434) | puts back the stayer's slot record; lets go of the stayer's +28 bit 8 (0x801F6828(stayer,0)) | per slot / node | the stayer's ticket hold goes [read hle.c:14298-14322] |

Not touched by the change at all (so they stay as player 2's scene left them) [read: no write in 0x80151DC0-0x80152900 to
-25696, -24048, -21900, -23060/-23052/-23056, -23040, -25960; only -23280 is read at 0x801521AC]: the play-hold word
r13-25696, the prompts word r13-24048 (fn22's bit 1), r13-21900, the hold's channels' +16 (0x80199EA0's), the zones word
r13-23040 (fn22's 0x8019C088), the words window r13-25960.

---------------------------------------------------------------------------------------------------------------
## 2. Each piece of player 2's scene, what player 1's change does to it, what R4b did for the mirror, and whether the guard carries over

### 2.1 The script waits (0x80163F04(1), table 0x805CAA40)
- Where player 2's waits live [read]: the game's table is player 1's at rest; player 2's ten entries are the host's
  g_coop_states.p2 (hle.c:15518-15530), swapped in only round their own calls: coop_prompt_look (16570 / 16706),
  coop_use_start / continue_coop_use (17915 / 17977), the arrival/own-scene end re-run (18507 / 18514), the Tome copy's
  item (26442 / 26558). Their timers count in the host (coop_script_timers, hle.c:20258-20271) and a run-out runs the
  script as theirs (coop_use_start). So a wait of a scene that runs entirely as player 2's is NOT in the game's table
  during player 1's change and survives 0x80163F04(1) [inferred from those reads; the change is outside every swap].
- Player 2's scripts whose words ARE in the game's table [read]:
  - the game going on with a script of theirs from its own wait (fn17, lr 0x80168388): its word is moved to the game's
    table (coop_state_to_game, hle.c:15536-15554, called at 18580-18592);
  - a scene begun as player 1 under player 2's screen (g_coop_scene_p2s: the statue's find 2242 from the words' Yes runs
    "who 1", hle.c:16933-16937) runs with the game's table;
  - player 2's own door's arrival script inside R4's bracket (0x801527F4) runs with no states swap (notify_ticket_run
    does coop_be_player2 and coop_cameras_in(2), no coop_states_swap: hle.c:13858-13868), so any wait it leaves is in
    the game's table [read; whether 1161-style arrival scripts wait: not read];
  - a found word copied across (coop_script_timers' found-word block, hle.c:20233-20255).
  Such entries are cleared by player 1's change, and the scene's later step (its fn22(0)) never comes: r13-25696 stays 1,
  the zones word stays off, prompts stay hidden: the frozen-both failure of docs/coop-failed-attempts.md:514 [inferred,
  the mirror of R4b's read of 779].
- What R4b does (stay_waits_clear, hle.c:24402-24425): only inside player 2's bracket; puts back entries whose script is
  NOT coop_p2_script_is. **Mirror:** inside a change of player 1's with player 2 apart, put back entries whose script IS
  player 2's: coop_p2_script_is(script) (g_coop_p2_scripts, hle.c:18269-18274), g_coop_p2_scene_script, or
  g_coop_scene_of[1].script. The same intercept (lr STAY_WAITS_CLEAR_AT 0x80152140) can carry both directions; only the
  gate (bracket 0, !g_coop_room_p2, coop_stay, player 2 resident apart) and the filter's sense change [inferred].
- Caveat [read]: g_coop_p2_scripts is cleared for a script when a press or zone of player 1's runs it (hle.c:18533-18539);
  for a scene that is player 2's but runs "as player 1" (p2s), coop_p2_script_is may be false. g_coop_scene_of[1].script
  covers that case: it is set for any scene coop_scene_set counts as player 2's (hle.c:17046-17047).

### 2.2 The busy word (0x801556CC(-1,0) at 0x80152184)
- Player 2's bits there [read]: fn22's 32 (0x800429E0) and its hold's 2 (0x80199EA0 at 0x800429D4), fn91's 1, words' 8
  and find's 16 when on the lend/screen route. Their own-copy words/close-up/find put nothing in it (hle.c:13041-13067,
  15566-15575).
- What clearing does [read pieces, inferred whole]: busy 0 at the arrival; r13-25696, the channels, zones word and
  r13-24048 stay as the scene set them. From then player 2's turn is no longer refused (coop_busy false,
  hle.c:20579) and runs mid-scene; the world's "everyone stands" (node update 0x800074AC, motion step 0x800271F0) no
  longer holds the scene's actors. The scene goes on from its own steps if its wait survived (2.1) and its fn22(0) clears
  the rest. With bit 2 gone, coop_p2_holds (hle.c:19378-19390) is false: harmless for player 1 then (busy 0).
- What R4b does (stay_busy_clear, hle.c:24427-24446): inside player 2's bracket with player 1's scene or close-up on,
  writes back busy & ~4 (written, not set: no event 102). **Mirror:** the same keep inside player 1's change with player
  2's scene/hold on. But its condition `coop_scene_on_of(1)` is ALREADY false at 0x80152184: the ticket bumped
  g_coop_tickets ~62 frames before (2.6) [read 11300, 16927-16943]. It needs 2.6's counter first, or a flag taken at the
  ticket.
- Keeping the bits has a cost to check (the R4c lesson, arrival-readers.md:1-70): with busy 34 kept and player 1
  re-held (0x80152190), player 1's arrival (state 121, door motion) advances only if the busy ask 0x80155738 answers 0.
  coop_busy_ask answers 0 for coop_p2_holds (hle.c:19440-19443), which needs `g_coop_script_hold || coop_p2_scene_on()`
  and `(busy & ~(1 | room_bit | 32)) == 2` (19388-19390): with 2.6's counter the scene stays on and 34 & ~33 == 2, so
  player 1's controller runs with the hold opened (hle_coop_controller p2_hold, hle.c:20033-20045) and their motion
  steps [inferred]. Without the counter, keeping the bits freezes player 1 in 121 exactly as player 2 froze in R4c
  [inferred mirror of arrival-readers.md §1, §4].

### 2.3 The hold bit 2 and the hold's channels before the arrival
- Player 1's own door script may let go of a hold of its own before the ticket (a door that asks first: fn21(1) ->
  0x8016820C -> 0x80199EA0(1)): that clears the one bit 2 and the channels, player 2's scene's too.
- R4c already made coop_play_hold keep the other's scene hold at a script step's let-go in BOTH directions: `theirs` by
  r13-21496, `other_scene = theirs ? coop_p1_scene_on() : (coop_p2_scene_on() || coop_scene_p2s_on())`
  (hle.c:13075-13114, gated on coop_stay()) [read]. It holds only if the let-go comes before the ticket (the scene is cut
  at the ticket). In L01/103 the let-go came just before the ticket in the same frame (arrival-readers.md:28-33) [read];
  a door whose let-go follows its fn66 would find the scene already cut [inferred]. 2.6 removes that order dependence.
- The arrival never touches the channels or r13-25696 [read §1].

### 2.4 The camera set, stack, depth and index
- Player 2's scene cameras are in their set: a scene begun in their look (coop_prompt_look puts coop_cameras_in(2),
  hle.c:16565) or a use of theirs (coop_use_start lends, hle.c:17935-17936; given back for a script-hold scene,
  hle.c:17994-17998) runs its fn8/fn10/fn11 into it; at rest their set is in g_coop_cameras.p2 with depth and index at
  +COOP_CAMERA_BYTES (coop_camera_set_io, hle.c:15346-15361) [read]. Then player 1's change writes only player 1's set:
  0x801EB550's clears, the arrival camera into eye record 2, 0x801ECCE0's depth := 0 [read §1].
- **Unless lent** (g_coop_set_in == 2: player 2's close-up / use on the screen route, cameras lent): then all three land in
  player 2's set, and the depth reset drops their close-up's push; coop_camera_chosen returns early when lent
  (hle.c:17214-17215), so nothing notes it [read]. Today such a change is blocked by the kept B / waiting way out
  (`g_coop_cameras.lent` and `g_coop_screen` are in both gates) [read 16436, 25765]; a scripted fn66 or the teleport would
  hit it [inferred].
- R4's guard for the mirror is coop_cameras_in(2) at notify_ticket_run and coop_cameras_in(1) at coop_stay_change_end
  (hle.c:13866, 13873) [read]. **Mirror:** for player 1's change apart, coop_cameras_in(1) (or coop_cameras_lend(false)
  with its prompt/look swap) round the change if g_coop_set_in == 2, and back after [inferred].
- Player 1 arriving INTO player 2's room: coop_camera_chosen's room-change branch (hle.c:17222-17246) is not the bracket
  and coop_stay_apart is false by then (r13-23276 is B, player 2's room) [inferred], so room_choices++; if player 2's set
  is p2_bare, coop_camera_p2_in's follow (hle.c:15398-15420; it needs !g_coop_p1_look && !g_coop_screen && depth <= 0)
  remakes player 2's set from player 1's arrival camera at their next set-in: player 2's half would leave their scene's
  camera for player 1's arrival camera [inferred]. p2_bare is false once a zone of theirs chose in that room or their
  own R4 arrival chose (hle.c:17220-17223) [read], so this needs a scene begun with player 2's set still bare.

### 2.5 Prompt slots, the look record, the words window
- Player 2's prompt slots are the host's g_coop_prompt_slots, their looks g_coop_p2_looks, swapped in round their look and
  their lend (hle.c:16563-16575, 15489-15500) [read]. Player 1's change resets the GAME's slots and cancels the game's
  look record's close-up (0x801172B0 -> 0x80024B24(0)): player 1's own, the mover's, which is right [read
  doors-readers.md:138]. Only when lent / g_coop_screen are player 2's slots and look in memory: then the reset and the
  cancel are player 2's [inferred]. R4's mirror guard was save/restore of 0x80335A38 7x28 (g_stay_bracket_slots,
  hle.c:13865, 13874) [read].
- coop_room_change_words (hle.c:11202-11230, from notify_room_change at 0x80152298's entry) closes PLAYER 1's words at
  their change and returns if g_coop_screen or g_coop_words_theirs (player 2's words in the game's window), so a window of
  player 2's in the game's memory is not closed by it [read]. The close-up cancel inside 0x801172B0 still runs on the
  game's look record [read].
- Player 2's own-copy words/close-up/find (g_coop_p2_words, g_coop_p2_look, g_coop_p2_find): the change does not touch
  their copies. coop_p2_own_cancel runs only from coop_zones_new on a new zone list (hle.c:15952-15981) [read], and player
  2's zone list is 0x80123F70(their object)+180, the slot whose +0x8140 room == object+592 (dis 0x80123FA4-0x80123FF0)
  [read], i.e. their own room's record, which the stayer's put-back keeps in place [inferred]. So their copies survive.
  What does reach them: the ticket's event 102 holds their node (+28 bit 8) for the countdown, and the tail lets it go
  (hle.c:14311-14320) [read]; whether their words' tick (coop_words_tick) or close-up stepper needs the node free was
  not read [open].

### 2.6 The host's own records of player 2's scene (the cut at the ticket)
- g_coop_tickets++ for every ticket, player 1's included (hle.c:11300) [read]. Cut by it:
  - coop_p2_scene_on (g_coop_p2_scene_tickets, hle.c:16927-16931) and coop_scene_p2s_on (hle.c:16939-16943) [read];
  - g_coop_scene_of[1] is left stale (no clear) [read];
  - player 2's waiting way out g_coop_way_out[1]: let go "a room was asked for since" without clearing the zone's inside
    bit (coop_way_out_go's `here` false, hle.c:16483-16502), so their zone stays come into with nothing run until they
    step out and in [read];
  - player 2's kept B g_coop_p2_kept: dropped (hle.c:20755) [read].
- What the cut does at once, before the arrival [read the pieces, inferred the whole]:
  - their refused turn steps their scene's camera only while coop_p2_scene_on (hle.c:20588-20589): from the ticket their
    half freezes on the scene camera's current place for the countdown (busy 38 still refuses their turn);
  - a camera arriving in their scene (fn11, ARRIVAL_END_RAN 0x801EC1C8) re-runs its script as theirs only while
    coop_p2_scene_on (p2_scene_end, hle.c:18483-18485); after the cut it runs as player 1 from the game's table, which
    the comment there says finds state 0, not the scene's own (room 183's L01/419);
  - coop_p2_holds (needs scene) and coop_scan_player1's zones-on for player 1 (needs coop_p2_scene_on, hle.c:19919) go off.
- What R4b did for the mirror: g_coop_p1_tickets, counting "all but a change of player 2's own that player 1 stays
  through" (hle.c:11268-11270, 11311-11312), used by coop_p1_scene_on and g_coop_scene_camera.tickets [read].
  **Mirror:** g_coop_p2_tickets, bumped for every ticket but a change of player 1's that player 2 stays through (coop_stay,
  !g_coop_room_p2, player 2 resident apart and the room asked for not theirs... and, see 3, also when it IS theirs:
  player 2 does not move either way), used by coop_p2_scene_on / coop_scene_p2s_on (and, if wanted, g_coop_way_out[1]
  and g_coop_p2_kept) [inferred].

### 2.7 Music deferral (R5)
- Player 2's music acts apart are deferred per room (coop_music_act, hle.c:13949-13985) and replayed when player 1
  arrives in that room (coop_music_replay at the arrival's group reset lr 0x8015206C, hle.c:14112-14118) [read]. Player 1's
  change into ANOTHER room: nothing of player 2's music is touched [read]. Player 1 arriving INTO player 2's room during
  their scene: the scene's deferred music is played then, as designed [read]. After that, together, player 2's further
  music calls pass (coop_rooms_apart false, hle.c:13958-13964) [read].

### 2.8 Holds on player 2's node during the countdown
- fn66's busy_set(4,1) sends event 102 from player 1 to every other node: player 2's +28 bit 8 set [read doors-readers.md:124].
  coop_node_held_ask answers "not held" for the other player only while coop_room_change_on (hle.c:19461-19463), false
  while bit 32 is set (hle.c:11285) [read]. So during the ~62-frame countdown player 2's node update and motion are posed
  (0x800074AC, 0x800271F0) [read arrival-readers.md §1], i.e. a script walk (fn17) or a motion in their scene stops for
  the countdown and resumes after the tail's let-go [inferred]. R4/R4b have no guard for this for player 1 either: in R4c
  player 2 was in their own door motion and player 1 in a held scene [read arrival-readers.md].

---------------------------------------------------------------------------------------------------------------
## 3. Player 1 arriving in the room where player 2's scene runs (they become together)

- The load: room_slots_return finds B in the other slot (g_room_slot_room[other] == wanted, "kept, a player stayed in
  it") and only swaps the indices: no unpack, B's nodes, the scene's actors and player 2 stay (hle.c:14326-14351) [read].
- r13-26508 == 0 (A != B), so the full arrival runs (§1): the 61s go to A's nodes and player 1 (not B's) [read]; the torch
  walk 0x80044748 walks B's nodes for player 1's torch [read doors-readers.md:50]; **event 62 goes to B's nodes from 0
  (0x80152780) while player 2's scene runs there** [read]; what 62 does to a scene actor or a creature mid-scene was not
  read [open].
- The arrival script (the 5th arg) runs as player 1 with player 1's set (coop_run_script's floor coop_script_cameras) in
  B: its cameras are player 1's (right). Whether an arrival script of B can start B's scene again or touch the running
  scene's script words (it runs with the game's table, player 1's) was not read [open].
- After it, coop_stay_apart / r6_apart are false (same room) [read 13832-13837, 24197-24202]: every apart path goes back
  to the together paths. The together route for "player 2's scene, player 1 walks" is coop_p2_holds + hle_coop_controller's
  p2_hold (hold opened round player 1's turn) and coop_scan_player1's zones-on for player 1's own look; both need
  coop_p2_scene_on, cut at the ticket (2.6) [read]. With the cut, and busy cleared: player 1 walks (busy 0) but their look
  with the scene's zones word off only clears what they're inside (hle.c:19915-19922's comment: player 1 never got
  CameraRgn07's camera in player 2's block-holes scene); player 1's half stays on their arrival camera until the scene's
  fn22(0) [inferred]. That is the user's rule (player 1 keeps their own camera; the scene stays in player 2's half), but by
  accident; with 2.6 it is by the together route.
- The user's rule "the camera comes back to whoever triggered it": player 2's scene's end (fn22(0)) runs as theirs or as
  the game's; coop_scene_set's owner-1 end only clears flags and passes (hle.c:17037-17045) [read]; its end camera goes
  into whichever set is in as it runs (theirs in their use; the game's, player 1's, if it runs as player 1 from the
  game's table) [read coop_script_cameras' rule, hle.c:18290-18302]. A p2s scene's end run as player 1 would set its end
  camera in player 1's set: player 1's half would take player 2's scene's end camera [inferred].
- Player 1's own R4c-style arrival camera flag (g_stay_arrival_camera) is player 2's only (coop_zones_new, hle.c:15952-15965)
  [read]; player 1's camera is never remade from player 2's set (coop_camera_p2_in remakes only player 2's) [read], so the
  mirror needs no such flag [inferred].

---------------------------------------------------------------------------------------------------------------
## 4. Per room / per slot / global (what may need a per-player keep)

- Per room or slot in the game: object+592, the slot records (+0x8140 room, +0x8142/+0x8143 loaded), the slot table
  0x8058AFC0, the 61/62 broadcasts (by r13-23288 / r13-23276), sound tracks' +132 room [read].
- One for the whole game, shared by both players' scenes: busy word r13-23344, the waits table 0x805CAA40 (player 2's kept
  by the host), the hold's channels r13-23060/-23052/-23056 +16, r13-25696, r13-24048, r13-23040 (zones word), r13-21900,
  r13-23280 (fn22's scene word, read by the arrival), the camera records 0x80602B18/0x80603178 with depth r13-21700 and
  index r13-26864 (player 2's kept by the host), prompt slots 0x80335A38 and looks 0x80307108 (player 2's kept by the host),
  the words window r13-25960 (player 2's own copy since step F), the fade [read].
- Host, per player: g_coop_scene_camera (+tickets = g_coop_p1_tickets), g_coop_p2_scene(+_tickets, _script),
  g_coop_scene_p2s(+_tickets), g_coop_scene_of[2], g_coop_way_out[2], g_coop_p1_kept / g_coop_p2_kept, g_coop_states,
  g_coop_cameras.p1/p2, g_coop_p2_words/look/find [read].

---------------------------------------------------------------------------------------------------------------
## 5. Candidate approaches (what each would touch; all gated on coop_stay() and the players apart; none for solo or together)

**A. Keep today's waits; guard only the paths that get through.** Player 1's door press stays kept and their walk-in way
out keeps waiting while player 2's scene/hold is up, apart too. Add the mirror guards (B2-B5 below) only for a change of
player 1's that starts anyway (their own script's fn66, a timer, a cinematic end, the coming teleport, a door during player
2's own-copy words). Touches: notify_room_ticket (a flag "player 1's change beside player 2's scene"), stay_waits_clear /
stay_busy_clear gates. Cost: player 1, alone in another room, cannot leave it while player 2 is in any scene or hold (a
long scene, a Tome item scene, a ladder's words with a hold); that is the together-era rule applied apart. Note the
asymmetry today: player 2's prompt doors are not kept beside player 1's scene (R4b's case went through), player 1's are.

**B. Full mirror of R4b/R4c (player 1 free to leave apart).** In order of dependency:
- B1. g_coop_p2_tickets (2.6): coop_p2_scene_on, coop_scene_p2s_on (and maybe g_coop_way_out[1], g_coop_p2_kept) compare
  against it; notify_room_ticket bumps it unless the change is player 1's and player 2 stays (coop_stay && !g_coop_room_p2
  && player 2 resident). Touches hle.c:11297-11330, 16927-16943 (+16488/16560, 20755). Everything that reads those
  functions changes meaning only in that one case.
- B2. stay_waits_clear both ways (2.1): inside player 1's change apart, put back the game-table waits of player 2's scripts
  (coop_p2_script_is, g_coop_scene_of[1].script, g_coop_p2_scene_script). Touches hle.c:24402-24425's gate and filter.
- B3. stay_busy_clear both ways (2.2): with player 2's scene/hold on (B1), keep busy & ~4. Touches hle.c:24427-24446.
  Depends on B1 for player 1's arrival motion (coop_p2_holds answers the busy ask).
- B4. The countdown (2.8): player 2's node not held by player 1's ticket while apart (coop_node_held_ask, or
  coop_room_change_on accepting the other's 32|2 apart). Touches hle.c:11281-11286 or 19458-19479: shared asks, read by
  many; narrowest is coop_node_held_ask's own gate.
- B5. The set in (2.4, 2.5): if lent at the ticket, player 1's set and slots in for the change, player 2's back after
  (coop_cameras_lend(false)/(true), or coop_cameras_in + the slots save as R4). Touches notify_ticket_run /
  coop_stay_change_end (a second, player-1 bracket kind) or the lend.
- B6. Lift the waits only when apart: coop_way_out_p2_up's use in coop_scan_player1 (hle.c:19905) and coop_p1_press_kept's
  `up` (hle.c:25795) false when the players' rooms differ. Player 1's door prompt is then hidden by player 2's scene's
  r13-24048 bit 1 (hle.c:17001-17004's comment), so the press still finds nothing; player 1's own look and prompts would need
  the scene's prompt hide off for them, as coop_use_start does for player 2 beside player 1's scene (g_coop_p1_scene_prompts_off,
  hle.c:17937-17945) [inferred]. This is the largest piece and the one most likely to conflict with the together rules.
- B7. Arrival into player 2's room (3): nothing extra for cameras if B1 is in (together route); event 62 to B's nodes and the
  arrival script beside the running scene need reading first.

**C. Hold the change until player 2's scene is over, per change, not per press** (a ticket of player 1's that arrives
while player 2's scene is up waits in the ticket tick). Rejected-shaped: the ticket is one record and fn66 has already
let player 1 go and started their motion and fade; docs/coop-failed-attempts.md has no attempt of this, but R4's
"one change at a time" refusal (coop_script_go_to_room hle.c:16505-16513) shows the host only refuses before fn66 writes
the ticket [read]. Not recommended [inferred].

---------------------------------------------------------------------------------------------------------------
## 6. Corrections to the earlier sketch (arrival-readers.md §4 D)

- D's "stay_waits_clear and stay_busy_clear accept g_stay_bracket == 0 with !g_coop_room_p2 and coop_scene_on_of(1)" cannot
  fire as written: coop_scene_on_of(1) is false from the ticket (2.6). B1 comes first.
- D's waits filter must be the opposite sense (keep player 2's), and player 2's scene waits mostly live in the host's
  g_coop_states.p2 already, so B2 matters only for the game-table cases listed in 2.1.
- D's "the same branch as A, with the roles swapped" for coop_play_hold is already built: R4c's script_step branch reads
  `theirs` from r13-21496 and checks the other's scene either way (hle.c:13075-13079).

---------------------------------------------------------------------------------------------------------------
## 7. Logs

- No recording shows a change of player 1's during a scene of player 2's apart: the two rooms-play runs have no player 2
  scene (`grep "begins as player 2\|player 2's own scene"` empty in logs/run-20261111-221141-rooms-play and
  -222138-rooms-play), and no "player 1's zone ... asked for a room while" or "player 1's B on prompt" waits [read].
- Together-mode runs before stay with a player-1 ticket after a player-2 scene began (logs/run-20261110-110144-p1block-user4
  page-0003 16442, scenes-user7/8) were player 1's own scene 1103 ending in a room, not the mirror [read].

---------------------------------------------------------------------------------------------------------------
## 8. Open questions (each with how to settle it without guessing)

1. Does a door prompt of player 1's show at all while player 2's fn22 scene sets r13-24048 bit 1 ("every prompt but slot
   0")? Which slot do door prompts take? Settle: `ed dis 0x80117628` (the chooser) and a trace of the shown slot
   (PROMPT_SLOTS+168) in a run with player 2's scene apart.
2. What event 62 (0x80152780, arrival, from 0 to r13-23276's nodes) does to a node mid-scene (a scene actor, a creature
   held by the scene). Settle: list the kinds' 62 handlers (`ed system` / the node tables) before B7.
3. Whether player 2's own-copy words tick / close-up stepper needs their node free during player 1's countdown (2.5).
   Settle: read coop_words_tick's call chain for 0x801F68D4 asks.
4. Which chapter scenes of player 2's put a wait in the game's table (2.1's cases) in practice: log `[stay]` entries kept
   by B2 in scripted runs; until then the cases are from code only.
5. Whether r13-23284/r13-23328's bit, skipped at player 1's arrival when player 2's fn22 scene word r13-23280 is on, is a
   room-visited flag the game needs (0x801521AC-0x801521D0). Settle: `ed who 0x801DC1A4`'s readers of r13-23328.
6. 0x800428D0 (-> 0x80039FF4) and 0x800428F0 (-> 0x800647C8) at 0x80152754/0x80152758: not read; check they touch no
   scene word.
7. Whether the user wants A (player 1 waits at a door apart while player 2 is in a scene elsewhere, as today) or B (free).
   The user's goal "both players acting at once" and separate rooms points to B, but B6 reopens the failure class of
   docs/coop-failed-attempts.md:514; refuters should weigh B6 against keeping the waits.
