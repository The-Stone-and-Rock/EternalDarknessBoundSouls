# F2 readers: player 2's arrival cancels the stayer's close-up

The run is logs/run-20261113-163201-stayer-s9-p1-zone (S9). Player 1 stays in room 183. Player 2's OPEN DOOR ticket comes at
3760, and player 1 presses EXAMINE at 3800. At player 2's arrival (3823) player 1's close-up is cancelled behind their back.
Their half stays on the close-up camera with DETAIL still showing, and their body walks into view. The contrasts are S6
(-163109-stayer-s6-p1-examine: player 1 in the close-up's words at the arrival, which survives) and the mirror S4/S1
(player 2's own close-up across player 1's arrival, which survives). This was read only: no code was changed and nothing was run.
Tags: [read] = seen at the address or line, [read there] = in the named doc or log, [inferred].

## 1. The arrival's chain, as the game has it

0x80152614 (the ticket's runner) -> 0x80152298 (the change) -> 0x801523A4 `bl 0x80151FDC` (the arrival, flags in r30) [read
there: design-r4d.md, `ed fn 0x80151FDC`: one bl site 0x801523A4].

At 0x8015212C-0x80152134: unless flags & 4, `bl 0x801172B0` [read `ed dis 0x80151FDC`].

**0x801172B0, the prompt reset** [read `ed dis 0x801172B0`]:
- r13-29016 := -1 (0x801172D4). The 7th record (the shown prompt, 0x80335AE0 = 0x80335A38 + 6*28) is zeroed (0x801172D8),
  and its +8 := -1.
- For slot 0..5: `0x80116E70(slot, 1)` (bl at 0x801172F8).
- `0x80117050` (0x80117308): frees the prompt element r13-24032 (0x801DA4D8), r13-24032 := 0, r13-29016 := -1.

**0x80116E70(slot, r4)** [read]: if r4 != 0, it calls the callback r13-24024 (bctrl at 0x80116EAC) with r3 = the slot's +12 (its
script) and r4 = slot. Then the 28-byte slot is zeroed and +8, +20, +4 := -1. The slots are 0x80335A38 (lis -32717, +23096).
r13-24024 is 0x80024CDC: the init 0x80117320 stores r5 there (0x80117340). Its one caller is 0x800214D4, which passes
r5 = 0x80020000 + 19676 = 0x80024CDC and r4 = 0x80024C78 into r13-24028 [read disasm.txt 29745-29758].

**0x80024CDC(script, slot), the let-go** [read]:
- It acts only on script -2 (0x80024CE4).
- slot 2 sets the looks' index (0x80307108 +64) := 0 (0x80024D28), slot 4 sets it to 1 (0x80024D3C), and other slots leave it.
- It prints (0x801DC80C), then calls `0x80024B24(0)` at 0x80024D64.

**0x80024B24(r3), the close-up cancel, on looks[+64]** (PROMPT_LOOKS 0x80307108: lis -32720, +28936) [read]:
- kind (+0) 1, a zone's look made ready: kind := 0. If its +8 == 2, 0x80024384 (words close).
- kind 2 or 3, a close-up, at 0x80024B98: kind := 0 (0x80024B9C). If +8 (the stage, "count" in the trace) > 0:
  - if +8 == 2: 0x80024384, the words closed with no script (0x80024BC0);
  - **0x80199EA0(1)** at 0x80024BC8, the hold let go;
  - **0x801ECCEC** at 0x80024BCC, the camera pop;
  - if !(+24 & 1): **0x801EEA9C** at 0x80024BF0, the snap of the shown records;
  - if +24 & 4: **0x80043C88(1)** at 0x80024C0C, the show;
  - **0x8002576C** at 0x80024C10, the hide-others end.
- If r3 != 0, the look's script runs (0x80163C6C, 0x801642D4). Here r3 is 0, so no script runs.
- kind 0 or kind >= 4 do nothing.

**0x80199EA0(1)** [read]: lets go of the control channels r13-23060, -23052 (0x8013DBC0(.., 0), 0x80199EE4/0x80199EF0), then
`0x801556CC(2, 0)` at 0x80199F24, **lr 0x80199F28**. This is the S9 line
`[trace] busy r3 2 r4 0 lr 0x80199F28 who 59` (log line 999). The close-up's open is 0x80199EA0(0) at 0x80024E8C ->
busy_set(2, 1) at lr 0x80199F38 (log 759, frame 3800).

So, the busy clear's call chain is:
0x80151FDC -(0x80152134)-> 0x801172B0 -(0x801172F8)-> 0x80116E70(2, 1) -(bctrl 0x80116EAC)-> 0x80024CDC(-2, 2)
-(0x80024D64)-> 0x80024B24(0) -(0x80024BC8)-> 0x80199EA0(1) -(0x80199F24)-> 0x801556CC(2, 0).
[read: each bl; the slot is 2 by S9's `shown 34/2` from 3819 to 3822 (`[stayer] frame` lines, the 7th record's +8/+4) and
`shown 6/2` at 3773-3800; that slot 2 is the let-go's r4: inferred from those]

The close-up's opening (0x80024D78's kind-2 branch, 0x80024E70-0x80024F84) [read]:
- object flag 4 on r13-26524's object (0x80024E84);
- 0x80199EA0(0), the hold, busy 2 (0x80024E8C);
- 0x801ECD68, the push (0x80024E90);
- 0x801EEB58(10), record 10 (0x80024E98);
- +24 |= 4 with bit 0 cleared when the player is in view (0x801F0638/0x801F077C, 0x80024F30-0x80024F48);
- 0x80043C88(0), the hide, if +24 & 4 (0x80024F64);
- 0x800255A4, hide the others (0x80024F80).

## 2. What R4's bracket has in memory at 0x80152134

The bracket opens at notify_ticket_run (hle.c 14133-14156) and closes at coop_stay_change_end (14157-14191) [read]:

| Swapped in the bracket | Not swapped: the game's, so player 1's |
|---|---|
| identity r13-26524, -21492, -21496, -21488, -21484 and the casting words (coop_be_player2 12419) | the prompt slots 0x80335A38 (7*28) |
| room word r13-23276 := player 2's room | the looks 0x80307108 (PROMPT_LOOKS, 68 bytes, the +64 index) |
| camera set: coop_cameras_in(cpu, 2) (15790): player 2's set in, g_coop_set_in = 2; the set holds the stack depth r13-21700 (15757) | the busy word r13-23344 |
| | the control channels r13-23060/-23056/-23052 |
| | the words window r13-25960 |
| | the play-hold word r13-25696 |
| | the script waits 0x805CAA40 |

- **The slots are saved, not swapped.** `coop_bytes(0x80335A38, g_stay_bracket_slots, .., true)` at 14149 saves player 1's
  slots and leaves them in. The bracket's end writes them back (14161) [read].
- Only coop_cameras_lend (15827-15857) swaps PROMPT_SLOTS and PROMPT_LOOKS together with the set. The bracket uses
  coop_cameras_in, which swaps only the camera set [read].
- The cameras were not lent in S9's window (`lent 0` in every gate line 3801-3821) [read there].

So 0x801172B0 walks **player 1's** slots, 0x80024CDC moves **player 1's** look index, and 0x80024B24 cancels **player 1's**
look. Its camera pop and snap land in **player 2's** set, and "the player" is player 2 [read code; inferred as the effect].

## 3. S9 at 3823, line by line (logs/run-20261113-163201-stayer-s9-p1-zone/page-0001.log) [read there]

| Line | What | Where in the code |
|---|---|---|
| 980 | `[stay] player 2's change to room 180 runs as theirs` (3822): the bracket opens | notify_ticket_run |
| 997 | `the arrival's walk passes over player 1's node` | 0x80152088 -> 0x80043A5C (coop_stay_strip_walk 14660) |
| 998 | `[trace] player 1's look over: ... busy 6 ... lr 0x80058E84 frame 3823` | the host clears g_coop_p1_look (section 4, A) |
| 999 | `busy r3 2 r4 0 lr 0x80199F28 who 59`, look `stage 0 script 92 count 1`, busy 6 | the cancel (section 1): kind already 0 (0x80024B9C), count 1 (+8 never 2: no words close) |
| 1000 | `event 102 from player 2's busy set (busy 0x4) to room 180: 10 nodes told` | that busy_set(2, 0), sent under player 2's identity to the room in play (180) |
| 1001 | `keeps script 1265's wait` | 0x8015213C, stay_waits_clear (R4b) |
| 1002 | `busy r3 -1 r4 0 lr 0x80152188`, busy **4** at entry | bit 2 is already gone. stay_busy_clear's keep = 4 & ~4 = 0, so it returns |
| 1003 | `hold bit := 1 on node 59 (P2) from lr 0x80152194` | 0x80152190 0x801F6828(r31, 1): r31 = 0x801F62B0() at 0x80152014, the current player = player 2 |
| 1016-1017 | `hold bit := 0 on node 1 (P1)`, `player 1's hold from the ticket let go` | notify_room_slots_tail 14618-14624, the stayer's +28 bit 8 |
| 1021-1022 | the bracket ends | coop_stay_change_end: player 1's set back, slots written back |

- `ed see 3830` and `ed see 4300` (S9): `the game's camera records: index 10, Camera06`, `busy 0x0`, player 1 `doing: free`.
  Player 1 walks 1334 -> 1383 and is hit (health 125 -> 118) [read there].
- shots/000037.jpg (~4300): the left half is still on the wall close-up, with **B DETAIL** shown and player 1's body in front.
  The right half shows player 2 at room 180's door [read there].

## 4. Every place in the arrival that touches the stayer's (player 1's) look

**A. The host, before 0x80152134: coop_busy_ask -> coop_p1_close_up at lr 0x80058E84 (hle.c 19465-19492, 19925-19956)** [read code + log 998]
- In the window, coop_busy_ask returns early through `stayer_window_on` (19935), so coop_p1_close_up is not asked there.
- At the arrival the window is over: split_counting needs COOP_ROOM_TICKET +0 > 0 (24944-24950). The runner runs once the
  count is out. `+0 1` is the last window line, at 3822 [inferred: +0 <= 0 in the runner].
- So the ask reaches coop_p1_close_up. There, `room_bit = coop_room_change_other(cpu, true) || stayer_window(cpu, 0)`, and
  both are false: coop_room_change_on needs the ticket's +0 > 0 too (11401-11406).
- The test is `held = (busy & ~(32|1|room_bit|...)) == 2`. With busy 6 that is 6 != 2, so the look is not held. `on` was
  true, so the host prints "look over" and sets **g_coop_p1_look := false**.
- lr 0x80058E84 is the player handler 0x80058DD8's busy ask (SPLIT_PLAYER_FILTER 24877). It runs between the walk
  (0x80152088) and 0x80152134: that is event 61 sent at 0x801520DC (to 0x801F62B8()) and 0x801520F4 (to the old room's
  nodes, player 1's room 183). [inferred from the log order and the code order]
- Effects:
  - the controller gate stops holding player 1 (coop_p1_close_up false);
  - **stay_busy_clear's gate `coop_p1_in_close_up` is false at 0x80152184**, so R4b would not keep bit 2 even if it were
    still there [read 26213-26215, 19415-19425].
- B3's comment (19474-19478) says the look is kept "so R4b carries it over player 2's arrival". It is kept only while the
  window counts. The window ends before the arrival.

**B. The game: 0x80152134 0x801172B0, the cancel (section 1).** It acts on player 1's records [read code; S9 999]:

| Record | What happens to it |
|---|---|
| PROMPT_LOOKS looks[0] | kind 2 -> 0 (0x80024B9C). Script 92 and count 1 stay. **Cleared.** |
| looks +64 (the index) | := 0 by slot 2 (0x80024D28). It was 0 already. |
| busy bit 2 | **cleared**: 0x801556CC(2, 0) at 0x80199F24 / lr 0x80199F28. Event 102 goes as player 2 to room 180 (log 1000). |
| control channels r13-23060, -23052 | **let go** by 0x80199EA0(1) (0x80199EE4/0x80199EF0). Player 1 can walk. |
| camera stack and record 10 | **not touched.** 0x801ECCEC pops the set in, player 2's (g_coop_set_in 2). It is guarded at depth <= 0 (0x801ECD08), so player 2's set (depth 0 before the runner's 0x801ECCE0) is a no-op [inferred]. Player 1's set stays in host memory with depth 1 and record 10 in. The bracket's end puts it back, so player 1's half stays on Camera06 [read: `ed see` index 10 through 4300]. |
| shown records' snap 0x801EEA9C | on player 2's records, if the blend bit was clear. The runner's arrival camera overwrites them [inferred]. |
| the show 0x80043C88(1) at 0x80024C0C (+24 & 4: player 1 was hidden at 3800, log 761) | host coop_show_asked (18512): `coop_look_shows(0x80024C10)` gives owner 0, because coop_p2_words_call, g_coop_screen and coop_close_up_p2s are all false. **g_coop_show_kept[0] := false, g_coop_unseen_out_of[0] := -1**: player 1 is drawn in their own half again [read code; picture]. |
| 0x8002576C, hide-others end | host coop_hide_others_end passes through (not a player 2 words call), so the game's own runs: r13-25952 := 2, and the others hidden by player 1's close-up are unfrozen [read there: COOP_HIDE_OTHERS_END comment 18604]. |
| object flag 4, set on r13-26524's object at the open (0x80024E84) | the cancel's path 0x80024B98-0x80024C10 has no write of it [read]. Whether anything clears it on player 1's object later: **not read**. |
| prompt slots and the shown record | zeroed by 0x801172B0, element freed by 0x80117050. **Then the bracket's end writes player 1's saved slots back** (14161): slot 2 again holds DETAIL 34/-2 and the shown record is back, but its look is kind 0. DETAIL is drawn again (shots/000037) [read there; that the element is remade from the restored record: inferred]. A press would go to a kind-0 look: what 0x80024D78 does with kind 0 is **not read**. |
| r13-25696, the play-hold word | 0 throughout (`hold 0` in every `[stayer] frame` line). The close-up never used it. Its hold is the channels plus busy 2 [read there]. |
| the zones word | `zones 1` (r13-23040) is unchanged in the window lines. The `[trace] player 1's zones` line reads g_coop_zones.list (20847-20853), which coop_zones_new made player 2's room 180 list at 3824. Its `0xC6000400 -> 0x00038000` at 3827 is room 180's zones, not a change of player 1's [inferred: misnamed after a split]. |

**C. 0x8015213C 0x80163F04(1), the waits clear.** R4b's stay_waits_clear puts back the waits that aren't player 2's (log 1001:
1265). A close-up has no wait in that table [inferred].

**D. 0x80152184 0x801556CC(-1, 0), the busy clear.** R4b's stay_busy_clear doesn't act: its gate is false (A), and its keep
would be 0 because bit 2 is already gone (B) [read log 1002].

**E. 0x80152190 0x801F6828(0x801F62B0(), 1).** The arriver, player 2, is held (log 1003). Not player 1's look.

**F. 0x8015211C 0x801EB550** (the camera records of the set in) and **the runner's camera block** (0x801EE998 / 0x801EE764 /
0x801ECCE0 / 0x801EEB58(2) / 0x801EEA9C). Both act on player 2's set (r4d-arrive-readers.md 3), so they never reach
player 1's record 10 [read there; inferred for the bracket].

**G. notify_room_change -> coop_room_change_words (11320-11346).** It returns for player 2's door under R4 (11324:
`coop_stay() && g_coop_room_p2`). Player 1's words are left alone [read].

**H. coop_stay_change_end (14157).** Player 1's set comes back with record 10 still pushed. The slots are written back over
a cancelled look (B). stay_scene_look_again (R4c) is only for player 1's scene [read].

## 5. S6 for contrast: why player 1's words survived (logs/run-20261113-163109-stayer-s6-p1-examine) [read there]

- The DETAIL press at 3790 opens the words: `busy r3 2 r4 0 lr 0x80199F28 who 1` (the game's own let-go into its words),
  and look `count 2`. The DETAIL slot's -2 is gone after it (`shown -1/0` at 3821-3822, and hle.c 11312: "the DETAIL press
  cleared its slot's -2").
- At 3791 `player 1's look over ... lr 0x800074C8`: the host's look ends there. g_coop_words_free is on (`words 1`).
- At the arrival (3823) there is no -2 slot, so the let-go never calls 0x80024B24. There is no 0x80199F28 line, and the busy
  clear sees busy 4.
- coop_room_change_words returns for R4, so the words stay open. RETURN follows at 3830 and the close at 3880, with the
  game's own pop on player 1's set.
- So the cancel hits a close-up at **stage 1** (DETAIL up, slot -2) and at **stage 3** (RETURN up: the RETURN prompt asks
  -2 into the look's slot, as player 2's `4:7/-2/4` after words in S1 line 924). It does not hit stage 2 (the words)
  [inferred from the slot rule; stage 3 not run].
- **A zone's ready EXAMINE** (kind 1, slot 2/4, -2) of player 1's is also cleared at the arrival: kind := 0 (0x80024B70). The
  bracket's end then writes the -2 slot back over a kind-0 look [inferred; not run].

## 6. A close-up of player 1's begun BEFORE player 2's ticket (the R4b case)

- **The game side is the same.** 0x801172B0 runs in every change without flag 4, whoever's ticket it is.
- In the window the slots are not emptied. The door's prompt bit 0x10000 makes 0x800084DC/0x800084EC call
  `0x80117360(0)`, which zeroes only the 7th (shown) record. Its 0x80116E70 call is only for r3 != 0 (0x801173B8)
  [read `ed dis 0x80117360`, disasm 4159-4165].
- So a DETAIL/RETURN -2 slot from before the ticket is still in at the arrival and is cancelled the same way [inferred].
- **The host side is the same.** Before the arrival, coop_room_change_other (ticket +0 > 0) gives room_bit 4 and the look
  reads held. At the arrival +0 is out, so it reads over [inferred from 19479-19486].
- R4b keeps bits only for `coop_scene_on_of(0) || coop_p1_in_close_up`. A scene (fn22 hold, no -2 slot, no look) survives.
  A close-up does not.
- **Today this is mostly unreachable.** Player 2's B on OPEN DOOR during player 1's close-up is kept
  (coop_player1_mid_use -> coop_press_keep "player 1's look or find is up", 21403-21404; 20809-20811 for the use route) until
  the look is over [read].
- A ticket of player 2's that doesn't come through a kept press (a walk-in exit's zone script, a script's teleport) could
  still reach it [inferred]. So this is a pre-existing gap. The stayer-presses build makes it common: it lets player 1 open
  the close-up inside the window, and P1 keeps their prompts working (`prompts unmasked 62`).

## 7. The mirror: player 2's own close-up at player 1's arrival

- Player 1's change opens no bracket (notify_ticket_run 14143: only `g_coop_room_p2`).
- The game's slots, looks, busy word and camera set in memory are player 1's.
- Player 2's own (step B) close-up lives in host copies the arrival never reaches: g_coop_prompt_slots,
  g_coop_p2_looks, their set g_coop_cameras.p2 with its own depth, and their busy kept in their copy ("the game's word kept
  4"). They are swapped in only for their turn's calls [read there: presses-effects-readers.md "Who is in memory at the
  firing"].
- coop_zones_new (16322), which would cancel it (coop_p2_own_cancel), runs only when **their** zone list changes. They
  stay, so it doesn't run [read code; inferred for the case].
- **S4** (-163024-stayer-s4-p2-zone) is the exact mirror of S9:
  - player 2's own close-up at stage 1 (`press id 6 on look 1 (kind 2, count 1 ...)`, 3870);
  - player 1's change at 3883 (the busy clear at 3884 sees the game's look `stage 0 script 349`);
  - `ed see 3950`: player 2 `doing: close-up`, camera Camera06 [read there].
- **S1** (-162932-stayer-s1-p2-examine, words stage at the arrival) goes on to RETURN at 3900 and closes at 3950. **S5b**
  (pickup, kind 3) goes the same way: words at the arrival (3886), closed 3990 [read there].
- On the lend route (g_coop_screen, cameras lent: slots and looks are player 2's in game memory), coop_close_up_kept on
  0x80024CDC (19591-19603) passes over the let-go of a -2 while `r13-21492 != g_coop_p2`. So player 1's arrival doesn't
  cancel it [read]. The rest of 0x801172B0 still zeroes the lent slots, and 0x801EB550 and the runner's camera block land in
  the lent set (r4d-arrive-readers.md 3(b)) [read there].
- Inside R4's bracket, coop_close_up_kept would **not** keep: r13-21492 is player 2 there [read 19593-19594].

## 8. Where it goes wrong, in one line each

1. The bracket swaps the identity and the camera set but not the prompt slots or looks. The game's "reset the player's
   prompts" at the arrival therefore resets the stayer's, and cancels their close-up (0x80152134 -> ... -> 0x80024B24(0)).
2. The cancel's half that is per-set (the pop and snap) lands in the mover's set. The half that is global (the busy bit,
   the channels, the show, hide-others, the look's kind) lands on the stayer. The bracket's end then writes the stayer's -2
   slot back. The result is half a close-up: the camera is stuck on record 10, DETAIL shows, the look is dead, and the body
   walks.
3. The host's coop_p1_close_up reads player 1's look as over at the first busy ask after the count runs out (lr 0x80058E84,
   still inside the arrival). B3's room-bit rule covers only the counting window, not the bracket, so R4b's gate is false
   by 0x80152184.

Directions this suggests (not designed, not tried; check docs/coop-failed-attempts.md first) [inferred]:
- In the bracket, put the mover's own prompt slots and looks in, as coop_cameras_lend does with the set. The game's
  arrival then resets the arriver's prompts and looks: player 2's, which coop_zones_new resets anyway. Player 1's stay
  in g_stay_bracket_slots plus a saved looks copy.
- Or pass over 0x80024CDC's -2 let-go inside g_stay_bracket, in the way coop_close_up_kept does for the lend route.
- And count `g_stay_bracket != 0` as room_bit in coop_p1_close_up, so the host keeps player 1's look over the arrival.
- Both directions mirror. Player 1's change has no bracket today, and player 2's records are host-held (section 7).

## Not read

- 0x80024D78 with a kind-0 look (a DETAIL press after the cancel).
- Who clears the open's object flag 4 (0x80024E84).
- A stage 3 (RETURN up) close-up across the arrival.
- A walk-in exit of player 2's during player 1's close-up begun before the ticket.
