# R4d design (final): player 1's door or arrival beside a thing of player 2's, apart (2026-11-12)

Read only: runtime/host/hle.c (working tree on HEAD 5eb040d; line numbers as of this reading), the disassembly
(build/ed-db/disasm.txt, `ed dis`, `ed who`, `ed door 180`, `ed how save`), the readers (r4d-change-readers.md,
r4d-arrive-readers.md), the two refuters (refute-r4d-impl.md, refute-r4d-goal.md), docs/coop-level3-design.md (R4 to R4c),
docs/coop-failed-attempts.md, the user's testing notes (memory: ed-coop-testing.md) and the logs named below. Nothing
was edited, built or launched. [read] = seen at the address or file:line given; [inferred] = reasoned from those reads.
This replaces the first draft. Section 13 lists each refuter problem and what was done with it.

Words used here:
- **Apart**: Mods > Co-op on, ED_ROOM_SLOTS=2, ED_COOP_STAY=1, both players' object+592 rooms differ and both are
  resident: the host's `r6_apart(cpu)` (hle.c:24197-24202: tests mod_coop, coop_stay(), no R4 bracket, both rooms in a
  slot) [read].
- **Player 2's thing**: any of: their fn22 scene (`g_coop_p2_scene`), a scene recorded as not player 1's
  (`g_coop_scene_p2s`: fn139, or a fn22 begun as player 1 under player 2's screen), their own script hold (the new
  per-player record `g_r4d_p2_hold`, D0), their lend-route close-up/words/find (`g_coop_screen`, `g_coop_words_theirs`,
  `g_coop_find_theirs`), their own-copy words/close-up/find (`g_coop_p2_words.up`, `coop_p2_look_up()`,
  `g_coop_p2_find.up`).
- **R4d change**: a ticket of player 1's asked apart while a thing of player 2's is on (section 3). Only R4d changes get
  the new pieces; every other door (solo, together, apart with nothing of player 2's on) runs exactly today's code.

---------------------------------------------------------------------------------------------------------------
## 0. The decision in short

Build the mirror of R4b/R4c for player 1's change, host only, narrowed to R4d changes, plus the freedoms that let
either player leave their own room while the other's thing goes on in another room.

| | What | Where | Why |
|---|---|---|---|
| D0 | `g_r4d_p2_hold`: player 2's own script hold, per player | coop_play_hold's hold step hle.c:13119-13127 | `g_coop_script_hold` is one bool any player's hold step rewrites (13120) [read] |
| D1 | `g_coop_p2_tickets`: player 2's records cut only by tickets that move player 2 | notify_room_ticket 11297-11330; coop_p2_scene_on 16927; coop_scene_p2s_on 16939; stamps 17051, 17069; player 2's way out and kept B | every ticket bumps g_coop_tickets (11300) and cuts player 2's scene [read] |
| D2 | the arrival's wait clear keeps player 2's game-table waits | stay_waits_clear 24402 (second branch) | 0x80163F04(1) at 0x8015213C clears every game-table wait [read] |
| D3 | the arrival's busy clear keeps player 2's bits | stay_busy_clear 24427 (second branch) | 0x801556CC(-1,0) at 0x80152184 clears the one busy word [read] |
| D5 | player 1's own set and slots in for the change if the cameras are lent | r4d_change_begin (from notify_room_change, 0x80152298's entry) and r4d_change_end | the arrival writes the game's set, slots and close-up record |
| D6 | player 1 free apart beside player 2's thing: prompts chosen, way out not waiting, kept B not waiting; their press under player 2's hold keeps its bits; player 2 not walked by player 1's close-up | coop_prompt_choose 19634; coop_scan_player1 19899-19938; coop_p1_press_kept 25788-25850; hle_coop_controller 20018-20045; coop_play_hold 13079-13108 | today player 1 apart stands at a door until a scene in another room ends |
| D7 | Case B: player 1's arrival camera and their first look there never remake player 2's set while player 2's scene is on | coop_camera_chosen 17219-17236 | room_choices++ would remake a bare player 2 set mid-scene |
| D8 | the two scene words held at 0 across the change: r13-23280 (fn22 on) and r13-25696 (play hold) | r4d_change_begin / r4d_change_end | 0x80154CD0 / 0x801521AC skip the visited mark; 0x8004303C (53 readers) answers "held" |
| D9 | player 2 free apart beside player 1's scene: their way out does not wait for it | coop_prompt_look 16557-16560, 16602 | the mirror of D6 (the goal refuter's M2) |
| D10 | a ticket asked by player 2's own scene's script, run as player 1, apart, is player 2's | notify_room_ticket after 11308 | "whoever triggers a room gets it" |
| D11 | a second ticket by the OTHER player inside an R4d change is kept and written after it, not lost | an intercept on 0x80155770 (beside its notify) | the one ticket record would be overwritten; refusing a scripted fn66 loses it for good |

Plus: the R1 stale guard (section 4, "Guards"), hle_save_held, the load reset, the run-out by the ticket's own countdown.

Dropped from the first draft: **D4** (the node let-go). Reason in section 4.

---------------------------------------------------------------------------------------------------------------
## 1. What I checked myself

1. **Every ticket cuts player 2's scene; the test itself cuts it.** notify_room_ticket does `g_coop_tickets++` first
   (hle.c:11300) and `g_coop_p1_tickets++` only `if (!(coop_stay() && g_coop_room_p2))` (11311-11312). coop_p2_scene_on and
   coop_scene_p2s_on CLEAR their flag when the stamp differs from g_coop_tickets (16927-16931, 16939-16943) [read]. So any
   test of player 2's scene made inside notify_room_ticket after line 11300 with today's compare drops the scene by asking.
   D1's compare must be in place before section 3's gate reads coop_scene_on_of(1) there. [read + inferred]
2. **The controller runs while busy if the node is not held.** 0x8000749C calls the busy ask 0x80155738; only if it
   answers nonzero does 0x800074AC ask the node's held bit (0x801F68D4); held -> skip; else 0x800074C4 runs the
   controller 0x80045BC4 [read dis 0x80007490-0x800074CC]. hle_coop_controller is the intercept on that controller
   (hle.c:27007) [read]. fn66 lets the asker's node go (0x801F6828(player,0), r4d-arrive-readers G1) and event 102 of the
   ticket's busy set skips the sender and reaches only the sender's room (0x801556FC-0x80155720, 0x801F5654-0x801F568C,
   first draft fact 3) [read]. So through player 1's own countdown their controller runs, hle_coop_controller runs, and
   player 2's refused turn steps player 2's scene camera (hle.c:20579-20589) [read], provided busy keeps bit 4 (see 3).
3. **A press inside player 1's turn under player 2's hold loses bit 4.** hle_coop_controller's p2_hold branch
   (20033-20045) does coop_hold_open (busy := 0, 25696 := 0, zones := 1, 24048 &= ~4), coop_player1_turn, then
   coop_hold_shut, which writes the four kept words back (19523-19531) [read]. The ticket written inside the turn
   (0x80155770: `stb 1,-23392(r13)` at 0x801557A8, busy_set(4,1) at 0x801557E4) [read dis] has its busy 4 overwritten by
   the kept 34. coop_p2_holds(34) stays true (19388-19390), so player 1 gets turns with the hold opened through their own
   countdown, and coop_room_change_on (11281-11288) is false. r13-23392 is not one of the four words (19506), so the
   ticket's mark byte survives [read]. (The impl refuter's blocker, confirmed.)
4. **With bit 4 kept, the countdown runs as solo for player 1.** busy 38 (32|4|2): coop_p2_holds answers false
   (38 & ~(1|32) = 6, room_bit is 0 because coop_room_change_on needs `busy & ~7 == 0`, 11285-11286) [read]; the busy
   ask is left to the game; player 1's node is not held (fn66 cleared bit 8); their controller runs as the game's
   (passthrough, 20087-20088); player 2's turn is refused and steps their scene camera. [read + inferred]
5. **The type-17 ticket skips the runner.** The countdown 0x80155830: when the count reaches 0, `+20 == 17` calls
   0x80152298 directly and sends event 62 itself (0x80155894-0x801558D0); otherwise 0x80152614 (0x801558F4) [read dis].
   0x80152614 calls 0x80152298 at 0x80152654; 0x80152298 calls the arrival 0x80151FDC at 0x801523A4 [read: the call
   list `bl 0x80152298` at 0x80020358, 0x8004FDAC, 0x800AD7C8, 0x800ADC24, 0x800AE768, 0x80110E88, 0x801525F8,
   0x80152654, 0x801558A8; `bl 0x80151FDC` at 0x801523A4]. The host already has a notify on 0x80152298
   (notify_room_change, hle.c:11235-11243, registered 27577-27579) [read]. So "the change ran" is marked there: every
   route of every ticket.
6. **The play hold word r13-25696 has one getter and 53 readers.** Written at 0x800429EC, 0x80042AF0, 0x80042B7C (fn22's
   function and the hold's), read only by 0x8004303C (`lwz r3,-25696(r13); blr`), which is called from 53 sites, among
   them the Tome's opening 0x80006D0C (returns at 0x80006D14 when nonzero), sanity 0x8006DE30 / 0x8006DEE8, the zombie
   0x8003B38C [read: disasm grep]. The word is global: player 2's fn22 scene apart sets it for player 1's room too.
7. **The scene owner and the close-up keep read g_coop_screen.** coop_scene_set: `owner = (p2_acts || g_coop_turn ||
   (g_coop_screen && running != g_coop_p1_used_script)) ? 1 : 0` (16976); coop_close_up_kept passes over every
   0x80024CDC(-2) while g_coop_screen and player 1 the player (19117-19129) [read].
8. **g_coop_script_hold is one bool.** coop_play_hold's hold step: `g_coop_script_hold = r3 == 0 && the player is player 2`
   (13120), on every hold step of anyone; the R4c let-go branch's `other_scene` for player 1's let-go is
   `coop_p2_scene_on() || coop_scene_p2s_on()` (13080), no script hold [read].
9. **Player 1's close-up apart beside player 2's fn22 scene.** p1_look = coop_p1_close_up && !both_scenes (20018-20019)
   opens the hold for player 2's turn (20025-20027) [read]; player 2's turn is refused only on busy (20579) [read], so
   they walk inside their own scene (the blocks-user3 8352-8675 failure, comment 20015-20017). And coop_p1_close_up's held
   test `busy & ~(32|1|room|8?|16?) == 2` (19029) stays true after player 1's RETURN, because the let-go branch keeps
   player 2's 34 (13081-13108) and bit 2 is the same bit for both: g_coop_p1_look would stay on until player 2's scene
   ends [read 19015-19036 + inferred]. Not raised by the refuters; fixed in D6e.
10. **fn66's return.** 0x801666A4 reads its arguments through 0x80163414 / 0x80163510 on the script's block (r3), calls
   0x80155770 at 0x801668E8 and returns 1 (0x80166998) [read dis]. A refused fn66 (host r3 := 0, coop_script_go_to_room
   16505-16513) never writes the ticket; for a script step nothing asks again [inferred, both refuters]. D11 therefore
   keeps the ticket at 0x80155770 (after fn66 parsed it) instead of refusing fn66.
11. **The dispatcher runs an intercept, then the notify only if the intercept passed through** (hle.c:5551-5564) [read]:
   an intercept on 0x80155770 can keep a ticket and return, and notify_room_ticket then does not run for it.
12. **The in-game save.** `ed how save` finds only room 93's SaveRgn (script 960, words, level 12) and room 274's
   [read]. The Tome's opening is refused while r13-25696 is set (fact 6), so player 1 cannot open their Tome during
   player 2's scene apart today [read]; whether the save menu is reached only through the Tome is [inferred] from the game.

---------------------------------------------------------------------------------------------------------------
## 2. Whose data each function reads, and what it does in each player's turn

| Function | Reads / writes (whose) | During player 1's R4d change (world tick / their turn) | If it runs in player 2's turn |
|---|---|---|---|
| fn66 0x801666A4 -> ticket 0x80155770 | r13-26524/-21492/-21496 (the player: who asks), busy word, ticket 0x8058AFFC, r13-23392 | player 1's: node let go, busy 4, event 102 to player 1's room only, fade (0x800443E4) | player 2's (identity), g_coop_room_p2 (11307-11308), R4's bracket |
| notify_room_ticket (host) | g_coop_tickets, g_coop_p1_tickets, new g_coop_p2_tickets, g_coop_room_p2, r13-23228 | D1/D10 decide; g_r4d set | g_coop_room_p2 = true; R4 |
| countdown 0x80155830 | ticket +0/+4/+20 | global; type 17 -> 0x80152298 direct + event 62 | the same (global) |
| 0x80152614 runner / 0x80152298 change / 0x80151FDC arrival | the current player (r13-26524's object stamped +592), r13-23276/-23288/-23284/-23280, slot table, the game's set/slots/looks, wait table, busy word, the current player's node (0x801F6828(r31,1) at 0x80152190) | player 1 the player (no bracket); D2/D3/D5/D8 in their window | only in R4's bracket (player 2 the player, their set/slots in) |
| 0x80163F04(1) waits clear | the game's wait table 0x805CAA40 (player 1's waits at rest; player 2's only those moved in) | D2 puts player 2's back | R4b keeps player 1's |
| 0x801556CC(-1,0) busy clear | the one busy word (both players' bits) | D3 keeps player 2's | R4b keeps player 1's |
| 0x80155738 busy ask / coop_busy_ask | busy word; host: coop_p2_holds, coop_p1_close_up, screen | answers every caller (all nodes, all rooms) at once | the same |
| 0x801F68D4 held ask / coop_node_held_ask | node +28 bit 8 | asked only after a nonzero busy answer (fact 2) | the same |
| 0x8004303C play-hold getter | r13-25696 (one word, set by whoever's fn22/hold) | 0 inside the window (D8); player 2's 1 otherwise: player 1's Tome refused, sanity and creatures in player 1's room see "held" (gap G1) | the same word |
| 0x80117628 prompt chooser (from 0x800084F8) | the game's six slots (player 1's unless lent), r13-24048 hide, r13-24040's 0x800460EC (r13-26524, hold channel) | D6a calls it with the hold opened and the hide 0 | coop_prompt_choose's screen branch calls it as player 2 (19656-19672) |
| 0x80006EBC press dispatch (from the controller: 0x80045D44 -> 0x8013E160 -> 0x8013E334) | shown slot 0x80335AE0 | player 1's B, inside coop_player1_turn under player 2's hold (fact 3) | player 2's presses go through the host's element (coop_use_start) |
| 0x801EE764 / 0x801EE998 arrival camera | eye record 2 of the set in | player 1's set (D5 if lent) | player 2's set in the bracket |
| coop_camera_chosen (host) | room_choices, p2_bare | D7: R4d change and Case B scene: not counted | R4 bracket: p2_bare false |
| 0x8016820C hold step / coop_play_hold | the hold channels, the four hold words; r13-21496 (whose step) | D0 records player 2's per player; D6c keeps the other's hold for player 1's let-go apart | player 2's step: D0 sets/clears g_r4d_p2_hold |

The r13 player words (r13-26524 object, -21492 node, -21496 id) are player 1's for the whole R4d change: notify_ticket_run
brackets only `g_coop_room_p2` tickets (13858-13860) [read]. Player 2's are touched only by D11's replay of a ticket of
theirs (through coop_be_player2, 12296) and by D10 (which makes the ticket theirs, so R4's bracket runs it).

---------------------------------------------------------------------------------------------------------------
## 3. The gates

```c
// R4d (rooms/design-r4d.md): a room change of player 1's asked apart beside a thing of player 2's. Host only.
static struct {
    bool change;   // the ticket is player 1's, asked apart, a thing of player 2's on (at the ticket or at the change)
    bool apart;    // asked apart (r6_apart at the ticket) and player 1's: change may still open at the runner
    bool ran;      // 0x80152298 ran for it (notify_room_change: every route, type 17 included)
    bool case_b;   // the room asked for was player 2's room
    bool lent;     // D5: the cameras were lent at the change: player 1's set and slots put in
    bool w23280, w25696;  // D8: held at 0 for the change, with the values kept
    u32 v23280, v25696;
    s32 r2;        // player 2's room at the ticket
} g_r4d;
static bool g_r4d_p2_hold;          // D0: player 2's own script hold, per player
static u32  g_r4d_bit1_owner;       // D3: who set busy bit 1 last (0 none, 1 player 1, 2 player 2)
static bool g_r4d_b_scene;          // D7: together after a Case B R4d change, player 2's scene still on
static struct { bool on; int who; u32 regs[8], stack[3]; s32 running; } g_r4d_later;  // D11: a kept ticket
```

- **Player 2's thing** (`r4d_p2_thing(cpu)`): `coop_scene_on_of(1) || g_r4d_p2_hold || g_coop_screen || g_coop_words_theirs
  || g_coop_find_theirs || g_coop_p2_words.up || coop_p2_look_up() || g_coop_p2_find.up`. Read only after D1's compare is in
  (fact 1).
- **The change gate**, in notify_room_ticket after g_coop_room_p2 is known and D10 has run, before `g_coop_p2_tickets++`:
  `apart = g_settings.mod_coop && coop_stay() && g_coop_p2 != 0 && !g_coop_room_p2 && r6_apart(cpu)`; `change = apart &&
  r4d_p2_thing(cpu)`. Re-asked once at r4d_change_begin (0x80152298's entry): `if (apart && !change && r4d_p2_thing(cpu))
  change = true` (a thing of player 2's that began during the countdown; D1 needs nothing then, the thing's stamp was taken
  after the bump). The goal refuter's B1: with nothing of player 2's on, no R4d piece runs.
- **Player 1 free** (D6), per frame:
  ```c
  static bool r4d_p1_free(CPUState* cpu) {  // apart, a thing of player 2's on, nothing of player 1's own up
      return r6_apart(cpu) && r4d_p2_thing(cpu) && !g_r4d.change && !g_r4d_later.on && !coop_room_change_on(cpu) &&
             !g_coop_cameras.lent && !g_coop_screen && !g_coop_use.active && !g_coop_words_theirs && !g_coop_find_theirs &&
             mem_read32(cpu, cpu->gpr[13] - 25960u) == 0u && !coop_scene_on_of(0) && !g_coop_p1_look && !g_coop_words_free &&
             !g_coop_find_free && !coop_player1_mid_use(cpu) && g_coop_tome_owner < 0 && !g_tome_copy_open &&
             !cutscene_playing(cpu) && !game_menu_open(cpu);
  }
  ```
  (g_coop_tome_owner: -1 none, 0 player 1, 1 player 2, hle.c:3920 per the goal refuter; `< 0` so a shared Tome of either
  player is not free.) The lend kinds (screen, lent, words_theirs, find_theirs) are excluded: player 1 stays as today
  beside them.
- **Player 2 free** (D9), per frame:
  ```c
  static bool r4d_p2_free(CPUState* cpu) {  // apart, player 1's thing is only their scene, nothing of player 2's own up
      return r6_apart(cpu) && coop_p1_scene_on() && !coop_player1_mid_use(cpu) && !g_coop_words_free && !g_coop_find_free &&
             !g_coop_p1_look && mem_read32(cpu, cpu->gpr[13] - 25960u) == 0u && !coop_scene_on_of(1) && !g_r4d_p2_hold &&
             !g_coop_screen && !g_coop_use.active && !g_coop_p2_words.up && !coop_p2_look_up() && !g_coop_p2_find.up &&
             !g_r4d.change && !cutscene_playing(cpu) && !game_menu_open(cpu);
  }
  ```
  Only beside player 1's scene: R4b keeps player 1's scene and close-up bits (stay_busy_clear 24429-24431) [read], but a
  change of player 2's beside player 1's close-up, words or find has never been played, and the arrival's close-up cancel
  (0x801172B0 -> 0x80024CDC -> 0x80024B24) is guarded only while g_coop_screen (fact 7). Those stay waiting (R4e).
- **Solo, together, ED_COOP_STAY off**: r6_apart is false (it tests mod_coop and coop_stay, and needs different rooms), so
  apart, change, both free gates, D7's Case-B flag, D10 and D11 are all false/off; D1's counter is bumped on every ticket
  exactly as g_coop_tickets, so each `stamp != counter` answers as today; D0's record is read only behind r6_apart; the
  busy-bit-1 record (D3) is written but read only by D3. [inferred from the construction]
  The one together state touched is the one an R4d Case B change itself makes (D7b, gated on g_r4d_b_scene).

---------------------------------------------------------------------------------------------------------------
## 4. The pieces, exactly

### D0. Player 2's own script hold, per player (coop_play_hold 13119-13127, and the clears)
- At the hold step (13119), after today's line: `if (player is player 2) g_r4d_p2_hold = (r3 == 0);` (a step of player
  1's leaves it). Cleared where g_coop_script_hold is cleared for player 2's ends (17876, 18948, 19190, 19309, 26044:
  each read at build to confirm it is player 2's end), on a ticket that moves player 2 (D1's bump), and at load.
  g_coop_script_hold itself is not changed (the together paths keep reading it).
- Used: r4d_p2_thing; D3's gate; D6a's gate; coop_p2_holds apart (`(g_coop_script_hold || (r6_apart(cpu) &&
  g_r4d_p2_hold)) || scene`, 19385); and coop_play_hold's let-go branch for player 1's script let-go apart: `other_scene =
  theirs ? coop_p1_scene_on() : (coop_p2_scene_on() || coop_scene_p2s_on() || (r6_apart(cpu) && g_r4d_p2_hold))`, so player
  1's door that asks first does not free player 2's climb hold (the impl refuter's problem 3).

### D1. g_coop_p2_tickets (hle.c:11268-11330, 16927-16943, 17051, 17069, 16466-16484, 16560, 19004, 20755)
- `static unsigned g_coop_p2_tickets;` beside g_coop_p1_tickets (11270): "the rooms asked for that move player 2: all but
  an R4d change (player 1's, apart, beside a thing of player 2's)".
- notify_room_ticket, after 11308: D10, then the gate (section 3), then `if (!g_r4d.change) { g_coop_p2_tickets++;
  g_r4d_p2_hold = false; }`. Log when set: `[r4d] a room change of player 1's beside player 2 in room %d (scene %d/%d,
  hold %d, screen %d, own %d/%d/%d): their records go on (frame %llu)`.
- coop_p2_scene_on / coop_scene_p2s_on compare with g_coop_p2_tickets; their stamps (17051, 17069) take it.
- Player 2's way out: coop_way_out_wait / coop_way_out_go (16466-16484) use `who == 1 ? g_coop_p2_tickets : g_coop_tickets`;
  coop_prompt_look's let-go (16560) compares with g_coop_p2_tickets. Player 1's (who 0) stays on g_coop_tickets.
- Player 2's kept B: coop_press_keep's stamp for who 2 and the drop at 20755 use g_coop_p2_tickets.
- Left on g_coop_tickets: g_coop_way_look, g_coop_p1_kept, the found words, g_coop_use.tickets (risk R7).
- Effect (player 2, Case A): their scene stays known through player 1's countdown and after: their refused turn keeps
  stepping their scene camera (20588-20589, with fact 4), the camera path's end re-runs as theirs (p2_scene_end,
  18483-18485), coop_p2_holds' scene branch stays, the R4c let-go sees other_scene. Player 1: none (g_coop_p1_tickets).

### D2. stay_waits_clear, second branch (hle.c:24402-24425)
- With `g_stay_bracket == 0 && g_r4d.change && g_r4d.ran && cpu->lr == STAY_WAITS_CLEAR_AT`: run 0x80163F04(1) as the R4b
  branch does, then put back each entry whose script is player 2's: `coop_p2_script_is(script) || (coop_p2_scene_on() &&
  script == g_coop_p2_scene_script) || (coop_scene_on_of(1) && script == g_coop_scene_of[1].script)`. Log `[r4d] player 1's
  change keeps script %d's wait (%u frames): player 2's`. Player 1's own waits are cleared, as solo.

### D3. stay_busy_clear, second branch (hle.c:24427-24446), and the bit-1 record
- Record: in stay_busy_clear's entry (it is the intercept on 0x801556CC for every call, 27835) [read], before anything,
  `if (mod_coop && (r3 & 1)) g_r4d_bit1_owner = r4 != 0 ? (player is player 2 ? 2 : 1) : 0;` (read only).
- Branch: with `g_stay_bracket == 0 && g_r4d.change && g_r4d.ran && lr == STAY_BUSY_CLEAR_AT && r3 == -1 && r4 == 0` and
  player 2's thing on the word (`coop_scene_on_of(1) || g_r4d_p2_hold || g_coop_words_theirs || g_coop_find_theirs ||
  (g_coop_screen && coop_use_theirs())`): run the clear, then WRITE (no event 102) `keep = busy & ~(COOP_BUSY_ROOM |
  (g_r4d_bit1_owner == 2 ? 0 : COOP_BUSY_SCRIPT))`. Log `[r4d] player 1's change keeps player 2's busy %u`.
- So bit 1 stays only if player 2 set it (the impl refuter's per-player point; the goal refuter's "same rule"): player 1's
  own door's fn91(1) is cleared by their arrival, as solo. R4b keeps every bit but 4 (24434); the same record would let it
  drop a bit 1 of player 2's own door. Not changed in this build (R4b is played and working); a follow-up (risk R3).
- Player 2's own-copy kinds put nothing in the word (CoopWordsOwn, 15566-15575): D3 does not fire for them alone.

### D4 (dropped). Player 1's node let-go
- The arrival re-holds player 1's node (0x801F6828(r31,1) at 0x80152190) on every arrival, solo too [read]. The held bit
  is asked only when the busy ask answers nonzero (fact 2). With D3's bits kept, the busy ask answers 0 for every kind
  D3 keeps: player 2's fn22 scene and script hold through coop_p2_holds (19378-19391, with D0 and D6f), the lend kinds
  through coop_busy_ask's screen term (19443) [read]. So player 1 is not posed, and no write of the game's bit is needed.
  Writing it (the first draft) would have freed player 1 from their own arrival scene's or words' hold (both refuters).
  U5 checks it.

### D5. Player 1's set and slots for the change while lent (r4d_change_begin; r4d_change_end)
- r4d_change_begin(cpu), first thing in notify_room_change (11235): `if (g_stay_bracket != 0 || !g_r4d.apart || g_r4d.ran)
  return; g_r4d.ran = true;` re-ask the gate (section 3); if !change return; `g_r4d.case_b = (s32)cpu->gpr[3] == g_r4d.r2`
  (0x80152298's r3 is the room, as at the runner: read at build); `if (g_coop_cameras.lent) { coop_cameras_lend(cpu,
  false); g_r4d.lent = true; } else if (g_coop_set_in == 2) coop_cameras_in(cpu, 1);` then D8. Log `[r4d] player 1's change
  to room %d runs with their own set (lent %d, case %c, player 2 in room %d)`.
- While g_r4d.lent: coop_close_up_kept treats the screen as off (`&& !g_r4d.lent` beside `g_coop_screen`, 19119) so the
  arrival's close-up cancel lands in player 1's record; coop_scene_set's owner reads `(g_coop_screen && !g_r4d.lent &&
  running != g_coop_p1_used_script)` (16976) so Y's arrival scene is player 1's. (The impl refuter's problem 4.)
- r4d_change_end: `if (g_r4d.lent) coop_cameras_lend(cpu, true);`.
- Reached only by a change that starts while lent (a timer or later step of player 1's script, a Tome item, the coming
  "Bring X to Y"); D6 never starts one (free excludes lent and screen).

### D6. Player 1 free apart
- **D6a, prompts.** First branch of coop_prompt_choose (the intercept on 0x80117628, main loop, player 1 the player):
  `if (!running && !g_coop_turn && r4d_p1_free(cpu) && rd(r13-24048) != 0)`: `CoopHoldKept k; coop_hold_open(cpu, &k);
  wr(r13-24048, 0); running = true; guest-call 0x80117628; running = false; coop_hold_shut(cpu, &k);` The chooser picks
  from player 1's own slots with the world as it is for them. Log on change: `[r4d] player 1's prompt chosen apart beside
  player 2's %s: shown %d/%d (hide 0x%X)`.
- **D6b, way out.** In coop_scan_player1's own-look branch (19899-19938): `const bool free = p2_up && r4d_p1_free(cpu);`
  `coop_way_out_p1_go(cpu, p2_up && !free)`; `coop_way_look_begin(cpu, 0, ..., !free)`. zones_on widened when free to
  `coop_scene_on_of(1) || g_r4d_p2_hold` (fn139 and the hold turn the one zones word off too).
- **D6c, kept B.** coop_p1_press_kept: `const bool free = r4d_p1_free(cpu); const bool up = coop_p2_scene_or_hold(cpu) &&
  !free;` and the go-in wait (25822) uses `coop_busy(cpu) && !free`.
- **D6d, a press inside the turn keeps its bits (the impl refuter's blocker).** In hle_coop_controller's p2_hold branch
  (20033-20041): `const bool free = r4d_p1_free(cpu); const unsigned t0 = g_coop_tickets;` before coop_hold_open; after
  coop_player1_turn and before coop_hold_shut: `if (free) { const u32 added = rd(r13-23344); if (added != 0u) {
  p2_kept.words[0] |= added; log "[r4d] player 1's turn beside player 2's hold added busy %u%s" (", a ticket" when
  g_coop_tickets != t0) } }`. The turn opened the word at 0, so all it holds then is player 1's own (4 of a ticket, 1 of
  a fn91, 8/16 of words/find). The hold channels and the other three words go back to player 2's as today (they hold as
  much). From the next frame busy reads 0x26 (38) for a door: fact 4 (no turn with the hold opened; R4 refusal sees the
  change only through D11's window, since coop_room_change_on stays false with bit 32).
- **D6e, player 1's close-up does not walk player 2 (the impl refuter's problem 2).** hle_coop_controller 20019:
  `const bool p2_own = r6_apart(cpu) && (coop_scene_on_of(1) || g_r4d_p2_hold); const bool p1_look = coop_p1_close_up(cpu)
  && !both_scenes && !p2_own;` Player 2's turn is then refused (busy) and steps their scene camera (20588). Player 1's own
  close-up runs as the game's (passthrough). And in coop_play_hold's let-go branch, for player 1's close-up let-go apart
  (`!theirs && !script_step && r6_apart(cpu)`) set `g_coop_p1_look = false` after the kept words are written back (fact 9:
  otherwise the kept bit 2 keeps the look "on" till player 2's scene ends).
- **D6f, the p2s and fn139 kinds (the goal refuter's M3).** coop_p2_holds' scene term apart: `const bool scene =
  coop_p2_scene_on() || (r6_apart(cpu) && coop_scene_p2s_on());` (19384). fn139 keeps busy 2 (r4d-arrive-readers G3), a
  p2s fn22 34: both answer (== 2 after the mask). So player 1 apart is not held by them before, during or after their
  change.
- What player 1 may do apart beside player 2's thing: any prompt of theirs and any walk-in way out. The close-up,
  words, find and door-that-asks-first combinations go through the existing both-orders keeps (coop_play_hold
  13069-13108, coop_scene_set 16985-17031) with D0, D6d, D6e. T1 covers a close-up and a door; words and a find beside
  player 2's scene are read from the user's play (U9).

### D7. Case B cameras (coop_camera_chosen 17219-17236)
- D7a: beside the R4 line (17220): `if (room_change && g_r4d.change && g_r4d.case_b) return;` (Case A already returns at
  coop_stay_apart, 17232). Log once `[r4d] player 1's arrival camera: player 2's set kept (case B)`.
- D7b: r4d_change_end sets `g_r4d_b_scene = g_r4d.case_b && coop_scene_on_of(1)`; before `room_choices++` (17234):
  `if (g_r4d_b_scene) { if (coop_scene_on_of(1)) return; g_r4d_b_scene = false; }`. Cleared too at the next ticket and at
  load. Player 1's first look in R2 then doesn't remake a bare player 2 set mid-scene (the impl refuter's problem 7).
- Player 1's set is never remade from player 2's (coop_camera_p2_in remakes only player 2's, 15398-15420) [read].

### D8. The scene words across the change (r4d_change_begin; r4d_change_end)
- At begin: `if (rd(r13-23280) != 0 && coop_scene_on_of(1) && !coop_scene_on_of(0))` keep it and write 0 (w23280); `if
  (rd(r13-25696) != 0 && (coop_scene_on_of(1) || g_r4d_p2_hold) && !coop_scene_on_of(0) && !g_coop_p1_look)` keep it and
  write 0 (w25696).
- At end: write each back only if the word is still 0 AND player 2's thing that set it is still on (coop_scene_on_of(1),
  or g_r4d_p2_hold for 25696). A fn22(1) of player 1's own arrival scene in between keeps its own 1; a scene of player
  2's that ended inside the window stays 0 (the goal refuter's minor).
- r13-23280's only readers are 0x80154CD0 and 0x801521AC (first draft fact 7) [read]; 25696's are the 53 sites of
  0x8004303C (fact 6). The window is one frame (0x80152298 to the behaviour pass after it), so the runner's own reads
  (the effect start at 0x80152748, the arrival) see player 1's world as solo.

### D9. Player 2 free apart beside player 1's scene (coop_prompt_look 16557-16560, 16602)
- `const bool p1_up = coop_way_out_p1_up(cpu) && !r4d_p2_free(cpu);` at 16557: the let-go and coop_way_look_begin(…, p1_up)
  (16602) both use it. Player 2's presses already pass player 1's scene (only coop_player1_mid_use keeps them, 20739,
  20761) [read]. Their change runs in R4's bracket with R4b/R4c.

### D10. A room asked by player 2's scene's own script (notify_room_ticket, after 11308)
- `if (!g_coop_room_p2 && coop_stay() && r6_apart(cpu) && coop_scene_on_of(1) && running > 0 && (running ==
  g_coop_p2_scene_script || running == g_coop_scene_of[1].script) && !(coop_scene_on_of(0) && running ==
  g_coop_scene_of[0].script)) { g_coop_room_p2 = g_coop_entered_p2 = true; log "[r4d] room %d asked by player 2's scene
  script %d run as player 1: theirs"; }` — before the g_coop_p1_tickets and g_fade_mover lines, so player 1 is not moved
  and the fade is player 2's. R4's bracket moves player 2 (the goal refuter's last major-minor). The ticket's own event 102
  already went to player 1's room (risk R9).

### D11. A second ticket by the other player inside an R4d change (intercept on 0x80155770)
- r4d_ticket_later, the intercept on 0x80155770 (beside notify_room_ticket, registered where the notify is, 27654; only
  if the entry has no intercept): passes through unless `g_r4d.change` is on (the window: the R4d ticket to
  r4d_change_end; an apart door with nothing of player 2's keeps today's path, where R4's fn66 refusal already holds
  because coop_room_change_on is true with busy 4 alone). Inside it: compute who asks as notify_room_ticket would (identity r13-21496, the way-out script, D10's
  scene-script rule): if it is player 1 (the same asker), pass through (the game's own rule: the last ask overwrites the
  record, as solo). If it is player 2: keep r3..r10 and the three stack words (caller's sp+8/+12/+16, read by 0x80155770
  at 0x80155784, 0x8015577C, 0x80155790) [read dis], the running script, `who = 2`, and return without writing (fn66 then
  returns 1 and its script steps on, as after any ticket). Log `[r4d] player 2's room %d asked during player 1's change:
  kept for after it`.
- Replay at the first behaviour pass after r4d_change_end: coop_be_player2(cpu, true, was) round a guest call of
  0x80155770 with the kept registers and stack words, r13-23228 set to the kept script round it and put back; then
  notify_room_ticket judges it player 2's and R4 brackets it. Log `[r4d] player 2's kept room %d asked now`.
- R4's own refusal at fn66 (coop_script_go_to_room 16505, coop_room_change_on) is unchanged; it is false during an R4d
  countdown (bit 32), so D11 is the only guard there. The first draft's widened refusal is not built (both refuters).

### Guards
- **Run-out**: at each behaviour pass, `if ((g_r4d.change || g_r4d.apart) && !g_r4d.ran && (s32)rd(COOP_ROOM_TICKET) <= 0)`
  clear g_r4d (log `[r4d] player 1's change never ran: forgotten`): the countdown is over without 0x80152298.
- **r4d_change_end** (notify_behaviour_pass 13890, beside coop_stay_change_end, when g_r4d.ran): D5's lend back, D8's
  put-back, D7b's flag, the R1 stale guard, clear g_r4d; D11's replay is armed for the next pass.
- **R1 stale guard** (r4d_change_end, coop_stay_change_end, and at load; only with coop_stay()): `if (coop_scene_on_of(1)
  && rd(r13-23280) == 0 && (busy & (32|2)) == 0) { g_coop_p2_scene = g_coop_scene_p2s = false; log "[r4d] player 2's
  scene record gone stale: dropped" }`. A load state's RAM with no scene drops the record (the impl refuter's minor: the
  load reset none of them, 28020-28090, 28225-28235) [read]; with a scene in RAM it is kept.
- **hle_save_held** (28008-28015): `|| g_r4d.change || g_r4d_later.on`.
- **Load** (coop_state_loaded 28225 / the Step E reset): `memset(&g_r4d, 0, …); g_r4d_p2_hold = false; g_r4d_b_scene =
  false; g_r4d_later.on = false;`, then the R1 guard.

---------------------------------------------------------------------------------------------------------------
## 5. What happens, per player

### Case A: player 1 in X takes a door to Y; player 2 in R2 (R2 != X, Y) in their fn22 scene
| Moment | Player 1 | Player 2 |
|---|---|---|
| before | prompts shown (D6a), exits not waiting (D6b); turns with the hold opened (coop_p2_holds) | scene in their half, busy 34, turn refused, their camera stepped |
| press | inside their turn: fn66, ticket, busy 4 kept into the word (D6d) | - |
| ticket | g_r4d.change; g_coop_p2_tickets not bumped (D1) | scene stays known |
| countdown (~62) | busy 38: their controller runs as the game's, their door motion (fact 4) | refused turn steps their scene camera (fact 2) |
| change (0x80152298) | ran; D5 if lent; 23280/25696 to 0 (D8) | set, slots, looks, copies untouched |
| arrival | their waits cleared (solo), 61 to X, 62 to Y, Y current | waits put back (D2), busy 34 put back (D3) |
| end (behaviour pass) | lend back; words back (D8) | R2 back in the other slot (R2's tail) |
| after | apart in Y; busy ask 0 by coop_p2_holds; not posed (D4's reason) | scene goes on; its end runs as theirs; fn22(0) clears it |

### Case B: player 1's door into R2 during player 2's scene
- As Case A to the change; the load finds R2 resident and swaps only (room_slots_return 14326-14351) [read].
- Arrival: 61 to X; 62 to R2's running nodes (A3, measured: U4); D7a keeps player 2's set.
- After: together. coop_p2_holds opens the hold for player 1's turn (20033-20045); their look's zones are on
  (19915-19922, needs coop_p2_scene_on: D1); D6/D9 off (not apart); D7b keeps player 1's first look from remaking player
  2's set until the scene ends. R5 replays R2's deferred music (14112-14118).

### The other kinds of player 2's thing
- p2s / fn139 scene: D1 keeps the record, D3 keeps busy 2 (fn139) or 34, D6f lets player 1 play beside it.
- Own-copy words/close-up/find: nothing in the word or the game's table; D1 keeps their way out and kept B; D6 frees
  player 1.
- Lend kinds (screen, words/find theirs): D1-D3, D5; player 1 not freed by D6 (as today).

### Player 2's way out beside player 1's scene (D9)
- Player 2 apart walks into an exit during player 1's scene: their look doesn't wait (p1_up false); fn66 as theirs; R4's
  bracket, R4b keeps player 1's waits and bits (24402-24446), R4c's script step and look-again.

---------------------------------------------------------------------------------------------------------------
## 6. Interactions

- R1/R2: the stayer is player 2 (no bracket); the tail's let-go of the stayer's bit 8 (14315) is a no-op (fact 2).
- R3/R4 fade: g_fade_mover = 1 (2 under D10).
- R4 bracket: separate state; coop_stay_apart, r6_apart, coop_stay_stayer, coop_camera_chosen's bracket line and R6c keep
  their meaning. A ticket is R4's (g_coop_room_p2) or R4d's, never both.
- R4b: g_coop_p1_tickets and the bracket branches unchanged; the bit-1 record is only read by D3. R4c: the let-go branch
  gains D0's term apart; D6e's g_coop_p1_look clear is apart only.
- R5: unchanged; Case B replays R2's music.
- R6/R6b/R6c: R2's creatures run with their room's rules; Case B's 62 runs with R2 current inside the change.
- Tome copy: excluded from both free gates (`!g_tome_copy_open`), and its item scripts' waits are in g_coop_states.p2,
  untouched.
- docs/coop-failed-attempts.md:514 (p1scene 3: a room change mid-scene froze both, r13-25696 left 1) was together with the
  scene's end lost; here the end is kept (D1-D3) and D8 puts 25696 back only while the scene is on. :518-520 (doorhold:
  answering held asks for the other's door): not used (D4 dropped; no held ask is answered).

---------------------------------------------------------------------------------------------------------------
## 7. Save/load and room-change edges

- In-game save apart during player 2's scene: player 1's Tome is refused while r13-25696 is set (fact 6), so the Tome's
  save is not reachable then (today and after R4d); room 93's SaveRgn (level 12) runs through the zones look, which R4d
  doesn't change. Both go to the user's later save test (the goal refuter's save-prompt minor: no prompt of a save is
  opened by D6).
- Save states: hle_save_held waits through the change and a kept ticket (section 4).
- Load: g_r4d and the new records cleared; R1 guard on the loaded RAM.
- Two tickets: same asker overwrites (as solo); the other asker's is kept (D11). A change of player 2's: R4's refusal.
- Player 1 leaving player 2's room while player 2's scene runs there (together at the ticket, a scripted change or the
  coming Bring): NOT changed; today's cut stays (open question Q1).
- "Bring X to Y": should ask through the game's ticket (0x80155770) as the moved player, so Case B / R4 / D11 apply.

---------------------------------------------------------------------------------------------------------------
## 8. Alternatives and why not

- **Keep player 1 waiting (no D6)**: against independent play apart and asymmetric with D9/R4b.
- **Hold player 1's ticket until player 2's scene ends**: fn66 has already started their door motion and fade.
- **Refuse a second fn66 in the window (first draft)**: loses a scripted room ask for good (fact 10). D11 instead.
- **D4's write of the game's bit**: frees player 1 from their own arrival holds; not needed (section 4).
- **Run D6's press outside player 1's turn**: the press dispatch is inside the controller (0x80045D44 ->
  0x80006EBC); moving it would rebuild the game's input path. D6d keeps the bits instead.
- **One g_stay_bracket for both directions**: g_stay_bracket != 0 means "player 2's change" to five readers.
- **The play-hold word by room for the whole scene (G1)**: the getter 0x8004303C has 53 callers and no node argument;
  answering per room needs the asker's room at each site. A separate item, after R4d (gap G1).

---------------------------------------------------------------------------------------------------------------
## 9. Gaps (known, not in this build)

- G1. While player 2's fn22 scene (or hold) is on apart, r13-25696 is 1 for player 1's room too: player 1's Tome opening
  (0x80006D0C) and quick spells, the sanity effects (0x8006DE30, 0x8006DEE8) and creature behaviours (0x8003B38C and the
  other 0x8004303C callers) read "play held". Today's behaviour, not made worse; probes in T1 (a Start and a quick-spell
  press of player 1's during 419, ED_COOP_ENEMY_LOG).
- G2. Player 2 beside player 1's close-up, words or find apart still waits (R4e).
- G3. The together case of D6e (player 1's close-up begun before player 2's scene, together) keeps today's hold opening.
- G4. A block item (L01/1100-1103, arrival 1161) of player 1's during player 2's scene apart is unreachable while G1
  stands (the Tome is refused).

---------------------------------------------------------------------------------------------------------------
## 10. Checks for ONE build (few launches; pictures of both halves and the ms column in every run)

Build: `cmd //c "$(cygpath -w tools/ed/build-win.bat)"`. Player 1's buttons by ED_PAD_SCRIPT with a temporary copy of the
user's settings.ini (pad_slot_p1 = 3, pad_slot_p2 = 1: the testing notes); player 1's places by ED_PUT, sticks by
ED_INPUT_SCRIPT; player 2 by ED_PAD2_SCRIPT. State made fresh on the build from Jump to Game (thrown away after):

```sh
cd "/e/Eternal Darkness Recomp Fork"
export ED_ROOM_SLOTS=2 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_COOP_TRACE=1 ED_COOP_ENEMY_LOG=1 ED_SHOW_TRACE=1 ED_HEAP_CHECK=2000 ED_RUN_SHOTS_MS=250
rm -f saves/rooms-ch1-183.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 r4d-state </dev/null
export ED_COOP_STAY=1; ST=saves/rooms-ch1-183.state
```

- **T1 Case A by press, a close-up first, player 2's B after (D1, D3, D6a-e, U1-U2, U5-U6, G1 probes).** Player 1 to 180
  (119), player 2 walks south into 183's hall (419 as theirs ~4012-4547 in walk3). ED_PUT player 1 at 180's LookRegion01
  (x -858..-624, y -96..263) at 4100; B 4130 (EXAMINE), B 4200, B 4260 (RETURN); Start 4280 and a D-pad quick spell 4290
  (G1 probes, expected refused); ED_PUT south of Region01 (sphere 4,751 r150) at 4300, stick north 4310-4340, B 4360.
  Player 2: `3700,5900,ldown;4700,4710,b`.
  Read: `[r4d] player 1's prompt chosen apart` (shown N, not -1); "player 1's close-up holds them alone"; p2x/p2y still
  through the close-up (D6e); "player 1's close-up lets go; player 2's scene goes on: hold kept" and no g_coop_p1_look
  left on after it; `[r4d] player 1's turn beside player 2's hold added busy 4, a ticket`; busy 0x26 every row of the
  countdown; `[r4d] a room change of player 1's beside player 2 in room 183`, `... keeps player 2's busy 34`; p2ex..p2tz
  moving through the countdown (no run of ~60 equal rows); p1x/p1y moving after the arrival in 182 (not posed, U5); 419's
  end "ran as player 2's" near C0's frame; player 2's B after it takes (no "kept" left on). ms column: no step > 50 ms in
  either half around the ticket and the arrival frames (the user's "brief freeze" symptom). Shots: player 2's half on
  419's cameras then Camera01, player 1's on 180 then 182.
- **T2 Case B by script, then together (D7a/b, U4, the together kept B).** ED_RUN_SCRIPT=110:119,170:119,520:29,580:29
  during 419; then ED_PUT player 1 at 183's Region01 approach (365,2930,185) after the arrival, stick into it, B.
  Read: `case B`, `player 2's set kept (case B)`; p1room == p2room == 183 after; player 1's half on Camera25/zones', not
  Camera24; player 2's half unchanged through the arrival; no "the room's start chose a camera (player 1's first look)"
  while 419 is on (D7b); [enemy] rows of 183 before/after the arrival (U4); "player 1's B ... kept: player 2's scene or
  hold is up" then "goes in" after 419's end (together: today's path).
- **T3 apart with nothing of player 2's on (no R4d) and a fight (the goal refuter's B1 and event-62 minor).** The R5
  s1/s2 recipes in one run: player 2 put at Puzzle_Mkr04 (1305,1508,185), player 1's 119 then 29 (Case B, no scene); and,
  if `ed room 183` lists a creature there, player 2 driven at it before 29's arrival. Read: no `[r4d]` line at all; the
  same `ed check` as R4b/R5; [enemy] rows of the fight across the arrival frame; both halves' ms.
- **T4 the mirror (D9).** Player 1 walks south into 183's hall (419 as player 1's); player 2 put in 180 and sent to a
  walk-in exit of 180 if `ed door 180` has one (it lists only prompt doors 27/28/29: then player 2's B on 29's OPEN DOOR,
  which checks R4b beside player 1's scene and that D9 changes nothing for a press). Read: no "player 2's zone N asked for
  a room while player 1's scene ... waits" for the walk-in case; `[stay] player 2's change keeps player 1's busy`; 419's
  end as player 1's.
- **T5 solo** (JTG_SET="mod_coop=0", the switches on, through 183 -> 180 -> 182): no [r4d] and no [stay] keep lines; the
  rooms as solo; [fps] as before.
- Every run: `ed timeline`, `ed check`, EVENTS.log, `ed see` at the ticket and arrival frames for both halves, [fps],
  saves/reports/hang-* absent.

---------------------------------------------------------------------------------------------------------------
## 11. Unknowns only a run settles, and the line each needs

- U1. D6a's chooser with the hold opened and the hide 0 shows player 1's zone prompt? `[r4d] ... shown %d/%d`; -1 with a
  door in the slots means 0x800460EC still says no (the hold's channel): open the channel too (the screen branch's mask).
- U2. Player 1's B reaches 0x80006EBC inside the turn and runs the door's script? `[trace] script run r3 27 ... lr`.
- U3. Which kinds leave a game-table wait at player 1's arrival? D2's keep line over T1/T2 and the user's play.
- U4. Event 62 to R2's running nodes in Case B (scene and fight)? [enemy] rows; a reset seen -> pass over 0x801F5620 at
  lr 0x80152784 for nodes of R2 when g_r4d.case_b.
- U5. Player 1 not posed after the arrival (D4's reason)? both.tsv p1st/p1anim, p1x/p1y.
- U6. Player 2's scene camera keeps stepping through the countdown? p2ex..p2tz.
- U7. A p2s / fn139 scene through player 1's change (D6f, D1)? No chapter-1 script recipe known here: D1's ticket log
  prints scene %d/%d; read it in the user's play.
- U8. A use of player 2's spanning player 1's ticket (R7): `[r4d] player 2's use %d saw player 1's ticket`.
- U9. Player 1's words or find apart beside player 2's scene: the existing keep lines in the user's play.
- U10. D11's replay (no scripted case in chapter 1 known): its two log lines.

---------------------------------------------------------------------------------------------------------------
## 12. Risks

- R1. A stale player 2 scene record (now cut only by their own tickets): the R1 guard drops it when RAM shows no scene.
- R2. D6 opens combinations never reached before (player 1's close-up/words/find beside player 2's scene apart): D0, D6d,
  D6e close the holes read here; words and a find are read from play (U9).
- R3. Busy bit 1: D3 keeps it only if player 2 set it; R4b still keeps every bit but 4, so a bit 1 of player 2's own door
  survives their arrival beside player 1's scene. Follow-up: the same record in R4b's branch, checked by T4.
- R4. Game-code tickets (type 17, cinematic ends) during player 2's thing apart are judged player 1's unless D10's script
  test catches them.
- R5. D5's lend round trip across a room change has never run (scripted changes while lent only).
- R6. 0x800428D0 / 0x800428F0 clear two global tables on every arrival (first draft fact 6), since R2.
- R7. g_coop_use.tickets still counts player 1's tickets (a use of player 2's spanning one).
- R8. The game's save apart (section 7).
- R9. D10's ticket's event 102 went to player 1's room (sender player 1): its creatures are node-held through the
  countdown (~62 frames) while player 1 plays on.
- R10. D11's replay writes r13-23228 round one guest call; a script engine reader inside 0x80155770 of that word is not
  read (none seen in 0x80155770-0x80155808) [read dis].

---------------------------------------------------------------------------------------------------------------
## 13. Refuted or corrected (each refuter problem, and what was done)

refute-r4d-impl.md:
1. (blocker) Player 1's press under player 2's hold loses bit 4 at coop_hold_shut: **real** (fact 3). D6d ORs the turn's
   added busy bits into the kept word when player 1 is free apart; T1 reads busy 0x26 through the countdown. r13-23392 is
   not one of the hold's words, so it needs no keep (19506).
2. (major) Player 1's close-up opens the hold for player 2's turn: **real** (fact 9). D6e: no hold opened for player 2's
   turn apart while their scene or hold is on, and the look's flag cleared at its kept let-go. T1 checks p2x/p2y.
3. (major) g_coop_script_hold is one bool: **real** (fact 8). D0, used in the let-go branch apart.
4. (major) D5 leaves g_coop_screen on: **real** (fact 7). `&& !g_r4d.lent` in coop_close_up_kept and coop_scene_set.
5. (major) Type 17 skips the runner: **real** (fact 5). "Ran" and D5/D8 moved to 0x80152298's entry (notify_room_change);
   run-out by the ticket's count, not frames; R1 guard at the end.
6. (major) The side guard loses scripted fn66: **real** (fact 10). Not built; D11 keeps the other player's ticket and
   writes it after the change; the same asker overwrites as the game does.
7. (minor) D4 frees player 1 from their own arrival hold: **real**. D4 dropped (section 4).
8. (minor) Case B's first look counts a room choice: **real** (17227-17236, 19777-19798). D7b.
9. (minor) Stale player-2 scene after a load: **real** (no reset at 28020-28090, 28225-28235). R1 guard at load, coop_stay
   only.
10. (minor) D3's gate reads the stale bool: **real**. D0 in D3's gate.

refute-r4d-goal.md:
1. (blocker) The gate fires on every apart door: **real**. Narrowed to "a thing of player 2's on" (section 3); T3 expects
   no [r4d] line.
2. (major) D4's solo premise: **real**; D4 dropped.
3. (major) No matching freedom for player 2: **real in part.** D9 frees player 2's way out beside player 1's scene;
   their presses already pass it (20739, 20761 test only coop_player1_mid_use). Beside player 1's close-up/words/find:
   R4e (G2), because the arrival's close-up cancel is guarded only under g_coop_screen.
4. (major) p2s / fn139 not covered by coop_p2_holds: **real** (19383-19386). D6f, apart.
5. (major) r13-25696 blocks player 1's Tome, spells, sanity, creatures: **real** (fact 6). Held 0 in the change's window
   (D8); the whole-scene case recorded as G1 with probes in T1.
6. (major) The side guard soft-locks a scene: **real**; D11 (as impl 6).
7. (major) Player 1 leaving player 2's room during their scene (together at the ticket): **real, not built**: it starts
   together, so the user decides (Q1). Today's cut stays.
8. (major) Checks ignore the testing rules: **real.** ED_PAD_SCRIPT for player 1's buttons, merged runs (5 + the state),
   pictures and ms in every run, player 2's B (T1), the mirror (T4). The block-item Case B is unreachable (G4); p2s/fn139
   have no known script recipe (U7).
9. (minor) D8's put-back with no scene on: **real**; put back only while player 2's thing is on.
10. (minor) g_coop_tome_owner != 0: **real**; `< 0`.
11. (minor) A save prompt apart: **not as stated.** The Tome (the save's way, [inferred]) is refused while r13-25696 is
   set (0x80006D0C-0x80006D14); D6 opens no save. Left to the save test.
12. (minor) Event 62 measured only in a scene: **real**; T3's fight.
13. (minor) A fn66 of player 2's scene script run as player 1 moves player 1: **real**; D10.
14. (minor) Bit 1 differs between directions: **real**; D3's setter record, R4b's matching use a follow-up (R3).

## 14. Questions for the user

- Q1. When player 1 leaves the room where player 2's scene is running (by a script, a Tome item or the coming Bring),
  should player 2's scene keep its record and bits as in R4d (they stay, and become apart), or should such a leave wait
  for the scene's end? Recommended: keep (open D1-D3 for `coop_stay && !g_coop_room_p2 && coop_scene_on_of(1)` at the
  ticket, whatever the rooms), but it starts together, so it is the user's call.
