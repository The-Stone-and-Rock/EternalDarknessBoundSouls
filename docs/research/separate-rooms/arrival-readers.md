# R4c readers (2026-11-11 night): a player arriving during the other's in-world scene

Read only. Facts carry EVIDENCE; [inf] marks inferences.

######## read:state121
READER REPORT: player 2's node state 121, run-20261111-222138-rooms-play. I read the disassembly and the logs only; I changed nothing.

1. STATE 121 IN 0x80058DD8 [read]
- How it is entered: event 253 at 0x80059188. The marker 0x80151B90 is found, then 0x80122920(obj, m, 0x100, 5) plays the motion at 0x80059254. If that returns non-null, 0x8012206C/0x80122088 (0x800592AC on) register the end callback 0x801F8FA4. That callback sends event 6 at the motion's end and event 7 when the motion is replaced. Then the state is set to 121. Player 2's door function reaches this through 0x801669AC, from fn66 0x801666A4.
- The state's handler is at 0x8005AE90 ('cmpwi r20,121'):
  - Event 6 goes to state 1 (0x8005AE98-0x8005AEB4: 0x801F64A0(node,1) and 0x801F6488(node,1)).
  - Event 7 goes to state 1 only if 0x80032AD0 returns 0.
  - Events 52, 32, 107, 153, 44, 105, 125, 175, 40, 147 and 43 return 1. Everything else goes to 0x8005B964, which returns 0, and falls back to state 0.
- Nothing in state 121 tests the busy word (0x80155738 / r13-23344), the hold (0x801F68D4: node+28 bit 8), the ticket countdown (COOP_ROOM_TICKET 0x8058AFFC) or r13-25696.
- The second way out is the arrival. The placing 0x80152614 sends event 238 at 0x801526D8 to 0x801F62B8()'s id and asks for a reply. State 0's handler for 238 is at 0x8005A8DC. It reads the current motion (0x801222F0 at 0x8005A950) and answers a mapped motion only for 25, 99, 103 or 104 (25→25, 99→99, 103→104, 104→103). Any other motion gets -1 (0x8005A9D8-0x8005A9E0). When the reply is not -1, the placing sends event 191 with that motion (0x80152720), and 191 starts state 80. When it is -1, there is no event 191 and the state stays 121 (0x801526F8).
- Event 6 can only come from the advancing motion update 0x80123698: its end callbacks run at 0x80123804 (transitions.md:120-123). The motion step 0x800271B4 has a gate at 0x800271F0-0x8002721C:
  - If 0x80155738() is non-zero and 0x801F68D4(node) is non-zero, it calls 0x80123B70 instead. That function holds the pose and does not advance.
  - Otherwise it calls 0x80123698.
  - The same busy-and-held test guards the node's behaviour tick 0x801F5D48 (0x800074AC, 0x80027774, 0x80027804) and the other step 0x8002723C (0x800272A8).
- The host's ask: coop_node_held_ask (hle.c:19398-19418) answers "not held" only while coop_room_change_on() is true. That function (hle.c:11283) needs the busy word to have no bit except 4, 2 and 1, so with bit 32 set it passes the question through. It also passes through for the player who asked for the room change (g_coop_room_p2 == p2).

2. PLAYER 1 ALONE, SAME LADDER L01/103, IN THIS RUN [log]
- both.tsv:
  - 1896: p1st 121, p1anim 104 at once, busy 0x4.
  - 1961: state 80, motion 103 (event 238 mapped 104→103, then 191).
  - 1988: state 1.
- So state 121 lasts 65 frames (1896-1960): the ticket's 62 frames plus the change. State 80 then lasts 27 frames.
- Player 2's own door under R4 with no scene on (run-20261111-221649-r4b-s1-p2door): 3761 state 121, motion 25, busy 0x4; 3826 state 80; 3886 state 81. Same as player 1: R4's arrival 238 reaches player 2 and works.

3. PLAYER 2, 2390 TO 3057 [log]
- 2390: script 103 runs for player 2 ('who 59': coop_be_player2 swaps r13-21492/-21496/-26524, hle.c:12296-12300). No "[coop] player 2 opened it" line, so coop_door_motion's own path did not run and g_coop_door_turns_until stays 0.
- From 2391: p2st 121, but p2anim stays 15, the idle. Player 1's row on the same door showed 104 at once.
- In the same frame (2390, before the 253) fn66 cleared "the player's" hold, 0x801F6828(0x801F62B0(), 0) at 0x801667A4. With the swap that is player 2's node [read; inf that the swap was in].
- The trace's busy sets at 2390 are 1 clear, 2 clear (lr 0x80199F28) and 4 set, all with who 59. Each one's event 102 is sent from player 2 (0x801556CC at 0x80155720), and 0x801F5620 skips the sender (0x801F5660). I found no logged event 102 that holds player 2.
- 2452: the arrival places player 2 at EntryMkr01 (195,-2269). Their motion is still 15, so the 238 answer is -1, no 191 comes, and there is no state 80 [inf from p2anim].
- 2452-2981: busy 0x20, hold column (r13-25696) 0x1, p2st 121, p2anim 15, position unchanged.
- 2982: busy 0x0, hold 0x0. Player 2 is still in 121.
- 3057: state 1, exactly 75 frames after busy reached 0. The stick goes to -127 in the same frame, but see the earlier run.
- The earlier user run without R4b (run-20261111-221141-rooms-play) shows the same thing:
  - 2230: 121, motion 15, busy 0x24.
  - 2292: arrival; busy cleared to 0 (the bug R4b fixed).
  - 2367: state 1, again 75 frames later, with the stick at 0 0 the whole time.
- So the way out was not the stick. It is the door motion's end, event 6, which arrives 75 frames after the busy word goes to 0 [inf, strong: the same 75 in two runs, nothing pressed in one]. Player 2's 104 would then be about 75 updates long [inf].

4. ANSWER: WHICH TEST KEPT PLAYER 2 IN 121, AND WHERE IT RUNS
- The test that kept them there is the motion step's 'busy && held' gate, 0x800271F0-0x8002721C [read]. While 0x80155738() answered non-zero (busy 0x24, then 0x20: player 1's scene bit 32, kept through the arrival by R4b's stay_busy_clear, hle.c:24372-24390), player 2's door motion was posed and never advanced. So no event 6 came, and state 121 is left only by event 6 or 7. Because the motion never advanced, it also never became the current motion (p2anim 15), so the arrival's 238 answered -1 and the state 80 route was lost too.
- That gate also needs 0x801F68D4(player 2's node), node+28 bit 8, to answer non-zero [inf]. The host passes that question through (hle.c:19401-19406). Nothing logged shows who set the bit after fn66 cleared it at 2390: it is not in both.tsv, and the 102 broadcasts I read exclude player 2 as the sender. So this part is unproved.
  - A probe that settles it: log player 2's node+28 & 8 and 0x801F68D4's answer per frame, and hook 0x801F6828 with lr and r4.
  - The other suspects are 0x80046738 (the controller-region loop calling 0x801F6828) and 0x8000A32C [not read].
- Where it runs: in the frame's holder pass, through player 2's object holder callback 0x800271B4 (or 0x8002723C). It runs in neither coop_events nor player 2's turn. Event 6, once sent by 0x801F8FA4, is delivered through 0x801F59B0 and reaches player 2's node through coop_events (hle.c:21655), which has no filter for it. The player's busy filter 0x800CA964 does not swallow events 6, 7 or 238 (transitions.md:131-134).
- The game's delayed-message queue also postpones messages to a held node while busy (0x801F541C-0x801F5460, 0x801F55D4). It did not decide this case: when busy cleared, state 1 came 75 frames later, not in the next frame [inf].
- Separately, player 2's controller did not run from 2391 to 2981. coop_player_turn refuses it at hle.c:20519 ('coop_busy && !coop_door_turns && !room_other'): busy 32, g_coop_door_turns_until 0, and room_other false because it is player 2's own change. Player 1's look ended at 2391 ("[trace] player 1's look over ... busy 36"). That happened because player 2's ladder words cleared the shared bit 2 at 2390, so coop_p1_close_up's 'held' test (hle.c:18968: busy & ~33 == 2, where 36 & ~33 = 4) failed and nothing opened the hold for player 2's turn. The missing '[coop] gate' lines do not prove the turn was missing: coop_gate_log prints only while g_coop_p1_look is set (hle.c:19363), and that flag went false at 2391.

5. THE FOUR CANDIDATES, FOR PLAYER 2, 2452 TO 2981
- Busy word 0x80155738 / r13-23344: TRUE (0x20). It gates the motion step, not state 121's handler.
- Hold node+28 bit 8 (0x801F68D4): inferred TRUE, not logged. Cleared by fn66 at 2390; who set it again was not found.
- r13-25696 (the 'hold' column): 0x1, TRUE. It is not part of the 121 exit or the motion gate [read].
- Ticket COOP_ROOM_TICKET 0x8058AFFC / countdown: 0 after 2452. State 121 never reads it [read].
- coop_room_change_on: false from 2391, because of bit 32.

Files: E:/Eternal Darkness Recomp Fork/runtime/host/hle.c (lines 11283, 18955-18976, 19398-19418, 20519, 21423-21443, 21655, 24329-24390), E:/Eternal Darkness Recomp Fork/build/ed-db/disasm.txt, E:/Eternal Darkness Recomp Fork/logs/run-20261111-222138-rooms-play/both.tsv and page-0001.log, E:/Eternal Darkness Recomp Fork/logs/run-20261111-221141-rooms-play/both.tsv, E:/Eternal Darkness Recomp Fork/logs/run-20261111-221649-r4b-s1-p2door/both.tsv, E:/Eternal Darkness Recomp Fork/docs/game/research/transitions.md (lines 112-134).

######## read:gates
REPORT: player 2 frozen from 2391 (not 2452) to 2981, run-20261111-222138-rooms-play

SHORT ANSWER
The host did not run player 2's turn at all from frame 2391 to 2981. Nothing ran silently: no code path wrote their camera view in that window. The window starts at 2391, not 2452. both.tsv already shows p2ex frozen at 491.9,-738.4,1107.4 from row 2400, and the gate log goes quiet after frame 2390 (page-0001.log 4341/4342 are the last gate lines).

The cause is player 2's own ladder script, 103. At frame 2390 its fn21(1), the "let go", cleared busy bit 2. That is the game's only hold bit, and player 1's scene was using it too (busy 34 = 32|2). After that the busy word held only 32 (plus 4 for the ticket), and none of the host's exemptions accept a busy word in that state.

1. What happened at 2390-2391 (evidence)
- Before 2390, player 2 had their ladder words up beside player 1's scene: busy 43, screen 1, lent 1. The gate lines read "controller: player 1 held in their own scene under player 2's screen" (log 4272-4341).
- At 2390, script 103 runs as player 2's (log 4344-4345: "goes on as player 2's (lr 0x800242E4)", which is WINDOW_CLOSE_RAN, hle.c:17963). Its busy writes, in order:
  - clear bit 1 at lr 0x80165660 (35 -> 34)
  - clear bit 2 at lr 0x80199F28, i.e. 0x80199F1C-0x80199F24 li r3,2 / li r4,0 / bl 0x801556CC. This is fn21's let-go path (ed fn 0x8016820C: "busy bit 2 ... cleared only on arg 1").
  - set bit 4 at lr 0x801557F0 (the ticket) -> busy 36 (log 4355-4363)
- At 2391, coop_p1_close_up (hle.c:18955) decides player 1's look is over. Its test at 18968 is `(busy & ~(32|1|room_bit|8?|16?)) == 2`. With busy 36 that gives 4, not 2. room_bit is 0 here because coop_room_change_other(cpu, true) only counts a change of player 1's. So 18973 sets g_coop_p1_look = false (log 4367: "player 1's look over ... busy 36 ... frame 2391").
- Nothing sets it again. The only scene setter is hle.c:17062, and it runs only at an fn22(1) with bit 2 set.

2. The exact conditions that stopped player 2's turn
a) The gate log is silent, which is not by itself a sign the turn didn't run. hle.c:19363 returns unless g_coop_p1_look is set, so every "[coop] gate ..." line, including "player 2's turn refused: busy" (20520), stops printing at 2391 whatever happens.

b) A room change of player 2's is not recognised while bit 32 is set. coop_room_change_on (hle.c:11285) requires `(busy & ~(4|2|1)) == 0`, so busy 36 makes it false for 2391-2452. Log 4361 ("a room change of player 2's: the other goes on through it") printed, but none of the exemptions that hang off coop_room_change_on applied:
  - coop_node_held_ask returns early (19401).
  - room_turn in the controller is false (19991).
  - room_other in the turn is false (20518).

c) The busy ask leaves the answer to the game. coop_busy_ask (hle.c:19382) answers "not busy" only for one of these, and none holds:
  - g_coop_screen (now 0)
  - coop_p1_close_up (false since 2391)
  - p1_free (needs busy == 1)
  - coop_p2_holds (19329 needs `(busy & ~(1|room_bit|32 if their own scene)) == 2`; 36 and then 32 both fail)
  So 0x80155738 returns nonzero.

d) Player 1's controller is passed over, and player 2's turn lives inside it. Main's update (dis 0x8000749C-0x800074C4) calls the busy ask; if nonzero, it calls 0x801F68D4(player 1's node), and if that says held it branches past `bl 0x80045BC4` (COOP_CONTROLLER, hle.c:11926). hle_coop_controller is that function's intercept (26947), and coop_player_turn is called only from it (20000).
  - [inf] Player 1's node is held: event 102 from the scene's busy set ("[flag] 102 on (frame 2100)"), and player 1 stands still the whole window.
  - Even if the controller were reached, coop_player_turn would refuse at hle.c:20519. coop_busy is true (32). coop_door_turns is false: g_coop_door_turns_until is set only at 21435, the log has no "[coop] player 2 opened it" line, and script 103 came in through 18499 rather than the controller's exit branch (20105-20115) that sets g_coop_door_p2_until. room_other is false (from b). So the refusal is silent (from a).

e) After the arrival at 2452, R4b keeps the scene's bit but not the hold. stay_busy_clear (hle.c:24367-24384) writes busy 32 (log 4479: "keeps player 1's busy 32"). Bit 2 was already gone at 2390. From 2452 to 2981:
  - busy = 32 exactly (both.tsv 0x20)
  - coop_room_change_on is false (ticket 0)
  - coop_p1_close_up is false (flag off)
  - coop_busy_ask leaves it to the game
  - so (d) applies on every frame until fn22(0) clears 32 at 2981 (log 4894-4905).
  Bit 32 alone is a state that no host test accepts: 18968 and 19329 want exactly 2 once the other bits are masked off, and 11285 wants nothing above bit 4.

Side effect: player 2's own ladder motion did not move either. Their position stays at -244.9,-90.3,16 from 2390 to 2452, and state 121 / anim 15 hold until after 2981. [inf] The motion step's held check (0x80027200 and others, see the hle.c:19392 comment) is exempted only by coop_node_held_ask, which needs coop_room_change_on.

3. Why player 2's camera eye froze
- g_coop_own_view, which is both.tsv p2ex, has only two writers:
  - coop_camera_step (hle.c:16062), called only from coop_player_turn: 20529 and 20541 in a refused turn, 20742 in a running one.
  - coop_cameras_lend(off) (hle.c:15462).
- At 2390/2391 the lend ends ("play goes on: the pad is player 1's again", lent 1 -> 0). 15462 copies the live view, which is player 2's ladder close-up camera (491.9,-738.4,1107.4), into g_coop_own_view. That is the last write before 2981.
- With no turn there is no step. A refused turn would also not step it: 20528 needs coop_p2_scene_on and 20530 needs coop_p2_look_up, and both are false.
- The view is drawn as follows. settings.ini:55 has mod_p2_camera = 2, so camera.c:1725-1730 draws player 2's half from hle_coop_player2_camera (hle.c:16256-16265), which returns g_coop_own_view while g_coop_own_view_frame != 0. The result is a frozen eye. hle_coop_screen() and hle_coop_cameras_lent() are both false, so camera.c:1625 does not apply.
- These are NOT the cause:
  - the lent check (16030): lent is 0
  - g_coop_scene_camera.p2_fresh: only read at the scene's end (17093), where it picks player 2's camera. At 2981, log 4896/4905 show "player 1's scene over ... player 2's kept" and "player 2's zone chose their camera".
  - [inf] By both.tsv, player 2's half was never on the scene's camera (ex -2,694,91, then 31,-1324,190). It was frozen on their own last close-up eye, looking at whatever room 342 shows from there.

4. How the together case gets past each condition
It is the same route this run used from 2100 to 2295, with player 2 in room 341. Log 3030-3031 and 3239-3240 show "controller, hold let go, to player 2's turn: busy 0" followed by "player 2's turn reads their pad". Living in different rooms is not what broke it; script 103's let-go is.
- Look flag: coop_scene_set (17062) sets g_coop_p1_look (kind 3) at fn22(1), with busy 34 having bit 2 and not bit 4 (log 3010). The flag lasts as long as 18968 sees exactly 2 after masking 32 and 1, so no one may clear bit 2 mid-scene.
- (a) passes because g_coop_p1_look is set.
- (c) passes because coop_p1_close_up is true at 19382, so the busy ask answers 0 (log 3017) and the controller is reached whether or not player 1's node is held (d).
- Controller: 19959 gives p1_look, 19967 coop_hold_open sets the busy word to 0 for the turn, and then coop_player_turn's 20519 coop_busy is false, so the turn runs from 20552 onward, including the pad read and coop_camera_step at 20742. Player 2 gets their own zones and camera (docs/coop-todo.md, "Player 2 walks in player 1's scene").
- Scene end: 17081-17110 chooses player 2's camera.
- Room change (b): there is no exemption for it with 32 set. Together, outside ED_COOP_STAY, a room change cuts player 1's scene (g_coop_p1_tickets, 16857), so 32 never sits beside bit 4. [inf] Together, with player 2 pressing a door that asks first (fn21 hold, then let go, as L01/103 does), the same let-go would clear bit 2 and drop the look the same way. docs/coop-failed-attempts.md:738 records this same class of fault: "the use's end clears hold bit 2 ... which is player 1's close-up's hold too", with the fix named there as a busy word kept per player.

FILES
- E:/Eternal Darkness Recomp Fork/runtime/host/hle.c (lines above)
- E:/Eternal Darkness Recomp Fork/runtime/host/camera.c (1567-1573, 1625, 1725-1730)
- E:/Eternal Darkness Recomp Fork/settings.ini (line 55)
- E:/Eternal Darkness Recomp Fork/logs/run-20261111-222138-rooms-play/page-0001.log (3007-3032, 4272-4367, 4468-4488, 4894-4905)
- E:/Eternal Darkness Recomp Fork/logs/run-20261111-222138-rooms-play/both.tsv (rows 2380-3000)

######## read:together
READER REPORT: a player arriving during the other player's in-world scene (run logs/run-20261111-222138-rooms-play)

## Correction to the case
The case says nothing was set up for player 2 because the scene began while they were in another room. That is wrong. The together route was set up at the scene's start and worked while player 2 was in room 341. It was lost at frame 2390-2391, during player 2's own door script and before the arrival, and nothing set it up again.

- page-0001.log:3007 and 3010: scene 779 begins as player 1's, and "player 1's scene holds them alone (busy 34 frame 2099)" (g_coop_p1_look = true, kind 3).
- From 3017 to 4342: "busy ask answers not busy" and "player 2's turn reads their pad" on every frame. Player 2 walked in room 341 during the scene. The last gate line is at 4342 (frame 2390).
- 3999-4007 (frame 2295): script 103 (the ladder) runs as player 2's (who 59). Its own hold sets 0x80199EA0(0) (busy_set(2,1) at lr 0x80199F38) and busy_set(1,1). The game has one bit 2, so this hold merged into the scene's: busy 35.
- 4355 and 4357 (frame 2390): script 103's own steps clear the bits. busy_set(1,0) at lr 0x80165660, then the script hold step 0x8016820C calls 0x80199EA0(1) at 0x8016829C, which runs busy_set(2,0) at lr 0x80199F28. The busy word goes 34 -> 32. That bit 2 and its hold channels were player 1's scene's too.
- 4363: the ticket's busy_set(4,1) at lr 0x801557F0 makes busy 36.
- 4367 (frame 2391): "[trace] player 1's look over: busy 36". In coop_p1_close_up (hle.c 18966-18974), held = (busy & ~(32|1|room_bit|...)) == 2, which is false: bit 2 is gone, and room_bit is only for player 1's own change (coop_room_change_other(cpu,true)). It would have been 0 anyway, because coop_room_change_on (11285) is false whenever bit 32 is set. With on=true and held false, g_coop_p1_look becomes false for good. Only coop_scene_set at fn22(1) sets it again (17062).
- both.tsv: from 2391, p2st is 121, the busy word 0x24, and p2ex stuck at 491.9,-738.4,1107.4. **The freeze starts at 2391, not at the arrival.**
- 4478-4483 (frame 2452): R4b's stay_busy_clear (24367-24384) keeps busy & ~4 = 32, as designed. The arrival script 1161 runs at lr 0x801527F8 as player 2 (who 59) with busy 32.
- 4896 and 4905 (frame 2981): "player 1's scene over ... the camera it began on, player 2's kept", then "player 2's zone chose their camera". both.tsv frame 2982: p2ex jumps to 22,-2348,833. p2st becomes 1 only at 3057.

Why there are no lines from 2391 to 2981: coop_gate_log (19360-19373) returns when !g_coop_p1_look. Player 2's turn runs only inside hle_coop_controller, which hooks 0x80045BC4, the player controller. That controller is called from 0x800074C4 only if the busy ask 0x80155738 answers 0 (at 0x8000749C), or if player 1's node is not held (0x801F68D4 at 0x800074AC). With the look off, coop_busy_ask (19375-19390) leaves the answer to the game (busy 32). So no controller, no coop_player_turn and no coop_camera_step run. hle_coop_player2_camera (16254-16266) keeps the last stepped view, which is room 341's close-up eye. The right half of shots/000135.jpg and 000150.jpg shows that frozen top-down view.

## (1) The together route: how player 2 walks during player 1's scene with their own camera
1. **coop_scene_set (16923-17118)** intercepts fn22 (0x80042974) and fn139 (0x80042B30).
   - It decides whose scene it is: owner at 16938-16943.
   - For player 1's scene, it calls the game's function (17043-17049).
   - If busy has 2 and not 4, it sets g_coop_p1_look = true, kind 3 (17061-17068).
   - It notes the start camera, record 2's eye and target, in g_coop_scene_camera, with tickets = g_coop_p1_tickets (17069-17074).
   - overlapped is set when the other player's scene is on or the cameras are lent (17041).
2. **coop_p1_close_up (18955-18988)** treats the look as held while (busy & ~(32 | 1 | room_bit | 8 if player 2's words | 16 if player 2's find)) == 2. It needs bit 2. coop_p1_in_close_up (18905-18915) treats a bare scene as not a close-up, so player 2's presses are not kept (coop_player1_mid_use, 18916).
3. **coop_busy_ask (19375-19390)** answers 0 when coop_p1_close_up is true. Player 1's controller is then called, and hle_coop_controller runs (19916).
4. **hle_coop_controller (19958-20006):** when p1_look is true, coop_hold_open (19447) saves and zeroes the hold's channels (r13-23060, -23052, -23056 at +16) and the words (-23344, -25696, -23040, -24048). It runs coop_player_turn (20000), then coop_hold_shut (20005). Player 1's controller then runs with the hold back on, so they stand.
5. **coop_player_turn (20481-20759):** busy reads 0 inside the opened hold, so the turn runs (20519). It reads player 2's pad (20606), runs their look in coop_prompt_look with their camera set in (16528), runs their controller (20740) and steps their camera (20742).
   - In coop_camera_step (16029-16075), their set is put in (coop_cameras_in(cpu,2)) and the game's CAMERA_STEP 0x801EB9A0(3) runs. The result goes to g_coop_own_view (16061-16063).
   - camera.c 1725-1730 draws player 2's half from it. If g_coop_own_view_frame == 0, camera.c draws the **game's** eye and target instead, which is player 1's camera and so the scene's.
6. **Which set the scene's cameras land in:** the scene's script runs as player 1 with g_coop_set_in == 1 (coop_script_cameras, 18234-18280, puts the owner's set in). Its fn8, fn10, fn3 and fn11 go into player 1's set. coop_camera_chosen (17146-17178) gives player 2 only room starts and their own zones' choices (17160-17171).
7. **The scene's end** (17075-17115) decides player 2's camera. It keeps theirs if one of their zones chose it (own: p2_set && !p2_bare && !fresh, 17102). Otherwise it takes theirs afresh (p2_set = false). It leaves theirs alone when the scene began overlapped.
8. **The scene's hold kept against the other player's let-go:**
   - coop_play_hold (13068-13100) keeps the channels and the four words when a close-up's let-go (lr 0x80024D78-0x800255A4, r3 == 1) comes while the other player's scene is on.
   - coop_scene_set (16951-16999) does the same when the other player's scene ends.
   - **Gap:** neither covers a script step's let-go (0x8016820C-0x801682A8, here lr 0x801682A0). At 13110-13118 it only sets g_coop_script_hold. This is the gap hit at frame 2390.
9. **The reverse (player 2's scene, player 1 walks):** coop_p2_holds (19318-19330) lets bits 32, 1 and the room bit pass, but only when !g_coop_p1_look. In hle_coop_controller (19973-19984) player 1 gets their turn with the hold opened. Player 2's scene camera is stepped in a refused turn (20528-20529).

## (2) What player 2's arrival through their own door does
- **notify_ticket_run (13848-13859),** at the entry of 0x80152614:
  - g_stay_bracket = 2, and player 2 becomes "the player" (coop_be_player2).
  - The room word is set to their old room.
  - The slot bytes are saved.
  - coop_cameras_in(cpu,2) (13856) puts their set in. coop_camera_p2_in (15377-15395) puts their kept set in; with g_coop_p1_look true there is no follow.
- **Inside the arrival:**
  - Its record-2 choices (0x801EE998, 0x801EE764) land in player 2's set. coop_camera_chosen sets p2_bare = false (17160-17162).
  - Script 1161 at lr 0x801527F8 runs fn4(0), fn8(Camera01) and fn3(20) (ed: 0x80168504 fn4, 0x801669EC fn8, 0x80168450 fn3), also into their set.
  - R4b keeps 779's wait (24342-24366, log 4476) and busy 32 (24367-24384, log 4479).
- **coop_stay_change_end (13860-13877)** runs coop_cameras_in(cpu,1). Their set, now on Camera01, is saved into g_coop_cameras.p2 with p2_set = true, and player 1's set goes back in. Their node and identity are restored, and the room word and slot table are player 1's again. The arrival's camera is in player 2's set when this ends. [inf: Camera01, from the script lines; there is no dump of the set.]

What is skipped or undone because player 1's scene is on:
- **Lost before the arrival:** the scene's bit 2, its hold channels (zeroed at frame 2390) and g_coop_p1_look (frame 2391). With them went coop_busy_ask's answer and player 2's turn. R4b keeps only what was still in the word (32).
- **No turn means no step:** g_coop_own_view stays on the old room's eye (it is set only at 16061-16063), and their node stays in state 121 until their controller runs [inf: 121 is the door state; it went to 1 only at 3057, after turns resumed at 2982].
- **The camera trap still to come, once their turn runs.** The first look in room 342 calls coop_zones_new (15927-15939), because the zone list changed. Nothing in R4 updates g_coop_zones.list. That call sets p2_set = false, p2_bare = true and g_coop_own_view_frame = 0. coop_prompt_look then puts their set in at 16528, and coop_camera_p2_in remakes it from the game's set, which is player 1's and so **the scene's camera**. It does this through coop_camera_set_fresh (15344) and coop_scene_p2_fresh (16873-16878, which marks p2_fresh with the scene's camera). The arrival's Camera01 is dropped, and player 2 sees the scene unless a zone of theirs chooses in that look. At frame 2981 a zone did choose (log 4905). [inf: EntryMkr01 is inside a camera zone. If an arrival point is in none, the scene would show in player 2's half.] Also, while own_view_frame is 0, camera.c 1729 falls back to the game's (scene) view.
- **Not an issue:**
  - coop_p1_scene_on is not cut, because R4 does not bump g_coop_p1_tickets (11268-11270).
  - The scene's end handles the arrival correctly when theirs counts as own: p2_set, and p2_bare false from 17160, give own = true and the camera is kept (17102).
  - overlapped was false (lent 0 at 2099, log 3007).

## (3) The reverse: player 1 arrives through their own door into room R, where player 2's scene runs
- **There is no bracket:** notify_ticket_run needs g_coop_room_p2 (13849), so g_stay_bracket == 0. Both R4b clears pass through (24344, 24369).
  - 0x80163F04(1) clears player 2's scene script's wait.
  - busy_set(-1,0) clears 32 and 2.
  - This is the failure R4b fixed, mirrored [inf]: the scene's last step and its fn22(0) never come.
- **The host forgets player 2's scene:** player 1's ticket bumps g_coop_tickets. That ends coop_p2_scene_on (16889-16893) and coop_scene_p2s_on (16901-16905), and the stale scene ownership in g_coop_scene_of[1] stays.
- **The rule that should apply, mirrored from (1):**
  - Player 1's turn comes through coop_p2_holds, which needs (busy & ~(1 | room | 32)) == 2 (19329).
  - Player 1's arrival camera must stay in player 1's set. coop_camera_chosen returns early when g_coop_cameras.lent (17152), so with player 2's scene lent, the arrival's choice would land in player 2's set [inf].
  - When player 1 arrives into player 2's room they are together, so coop_stay_apart is false and room_choices++ runs (17174). If p2_bare, coop_camera_p2_in would make player 2 follow player 1's arrival camera (15384) [inf].
  - The same let-go gap as (1) item 8 applies to a script hold of player 1's during player 2's scene.

## (4) Smallest changes recommended
All four are host-only and use the game's own words.

**A. Keep the scene's hold when the other player's script step lets go.**
In coop_play_hold, widen the "other's scene keeps its hold" branch (13071-13100) from the close-up stepper's lr range to also accept lr in SCRIPT_HOLD_FROM..SCRIPT_HOLD_TO (0x8016820C..0x801682A8) with r3 == 1, gated on coop_stay().
- Whose step it is comes from who is the player: r13-21496 == player 2's id (who 59 at log 4357).
- The other's scene is coop_p1_scene_on() for player 2's step, or coop_p2_scene_on() || coop_scene_p2s_on() for player 1's.
- In that branch, also clear g_coop_script_hold. It returns before 13110, which would otherwise clear it.
- Effect at frame 2390: bit 2 and the scene's channels survive. The busy word becomes 34, then 38 after the ticket.

**B. Arm player 1's scene look again at the end of player 2's change.**
In coop_stay_change_end, after coop_be_player2(...,false): if coop_scene_on_of(0) && g_coop_scene_of[0].function == 0x80042974 && !g_coop_p1_look && (busy & 34) == 34 && !cutscene_playing, then set g_coop_p1_look = true and g_coop_p1_look_kind = 3 (as 17062-17064 does).
- With A, R4b keeps 34, so coop_p1_close_up's held test passes (34 & ~33 == 2). coop_busy_ask then answers 0, and the controller runs player 2's turn with the hold opened and shut round it. That is the together route, from the next frame.
- The look still ends during the door, at frame 2391 (busy 38, room bit not allowed). That does no harm: player 2 is in their door motion and the arrival completed without a turn (2452). This avoids touching coop_p1_close_up and coop_room_change_on.

**C. Keep the arrival's camera through player 2's first look in the new room.**
Add a one-shot flag, set in coop_stay_change_end only when coop_scene_on_of(0). When it is set, coop_zones_new (15936-15939) leaves p2_set, p2_bare and g_coop_own_view_frame as they are; the zone words, prompts, looks and states still reset; then the flag clears.
- Their first turn then steps their own set: Camera01, a cut (fn8 c=0). It does not remake the set from the scene's camera through coop_camera_set_fresh, and camera.c never falls back to the game's view.
- The scene's end then counts it as own (17102) and keeps it.
- The same drop happens on a scene-free R4 arrival, where a zone's choice masks it [inf]. It is left alone here to keep the change scoped.

**D. The reverse (player 1's door into player 2's scene) is not needed for this case.** Note it for later:
- the same branch as A, with the roles swapped;
- an R4b-style keep for player 1's own stay change while player 2's scene is on (stay_waits_clear and stay_busy_clear accept g_stay_bracket == 0 with !g_coop_room_p2 and coop_scene_on_of(1));
- player 2's scene flags not cut by a ticket of player 1's that player 2 stays through, the counterpart of g_coop_p1_tickets.

**Nothing acts with no scene on, or with ED_COOP_STAY unset:**
- A: gated on coop_stay(), the script step's lr range with r3 == 1, and the other player's scene being on. With no scene it is skipped, and the original code from 13102 onward runs unchanged.
- B and C: only inside coop_stay_change_end, which returns at once unless g_stay_bracket != 0 (13861). That is set only by notify_ticket_run, which needs coop_stay() (13849). Both also need coop_scene_on_of(0).
- With ED_COOP_STAY unset, coop_stay() is 0, so none of A-C runs.

**Latent together-mode gap (not seen in a log):** without the stay gate, A's gap means a script hold and let-go of player 2's (a lever, a door that asks first) beside player 1's bare scene ends player 1's look too. Player 2 would then stand until the scene ends.

Files: E:\Eternal Darkness Recomp Fork\runtime\host\hle.c, E:\Eternal Darkness Recomp Fork\runtime\host\camera.c, E:\Eternal Darkness Recomp Fork\logs\run-20261111-222138-rooms-play\page-0001.log, both.tsv, shots\000135.jpg and 000150.jpg (frames 2558 and 2832).
