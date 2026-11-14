# The stayer's presses through the other's countdown: every gate (readers)

For the goal of 2026-11-13: when one player takes a door and the other stays (ED_ROOM_SLOTS=2 + ED_COOP_STAY=1, co-op
on), the stayer keeps every action through the ~62-frame countdown: quick spells, casts from the Tome, B on prompts, zone
looks, examine and pick up, Start and the Tome. Both shapes (split, apart), both directions.

Read only. No code was changed, nothing was built and the game was not run. Sources: runtime/host/hle.c (working tree,
uncommitted, 30,141 lines; line numbers as it stands), build/ed-db/disasm.txt, `ed fn`/`ed dis`, docs/game/research/verbs.md,
docs/coop-no-pause.md, docs/coop-failed-attempts.md, and the separate-rooms readers named below.
[read] = seen at the address or hle.c line given. [read there] = seen in the named doc. [inferred] = reasoned, not seen.

Words: the **mover** takes the door. The **stayer** stays. **Player 1 staying** = player 2's door. **Player 2 staying** =
player 1's door. The **window** = from the ticket (0x80155770) to the change's tail.

## 0. In short

1. **The busy word 4 refuses none of the stayer's presses by itself.** None of the 35 busy-ask sites (bl 0x80155738) is in
   the quick-spell handler 0x800069DC, the cast 0x801C4250, the Tome opener 0x80006CDC, the prompt use 0x80006EBC, the
   prompt chooser 0x80117628 or the zone scan 0x80134A44 [read: the site list]. The player's busy filter (0x80058E80) does
   not swallow event 135 (the Tome's "may I open") or 185 ("may I cast") [read there: mover-readers.md §2, the 0x800CA964
   decode]. What stops the presses is four other game words and about ten host rules.
2. **The game's four:**
   - the input lock r13-23592 (0x800443E4 from the ticket writer at 0x801557F4). It is per player, and it lands in the
     game's input copy, which is player 1's. At player 2's prompt door it locks player 1, the stayer: B, Start, quick
     spells and R give nothing for ~65 frames;
   - the Start lock r13-26232 (the same call). One shared word: it refuses 0x80006CDC for any player for ~65 frames;
   - the prompt flag 0x10000 of r13-24048 (0x801557E0). One shared word: the world frame clears the shown prompt every
     frame (0x800084D8-0x800084EC) and the chooser shows none (0x801176B4). It kills player 1's B, which presses only the
     shown prompt. Player 2's B reads their own slots and does not need it;
   - the fade's state (0x801E7C98: r13-21784, r13-26896). Shared: once the fade starts it refuses 0x80006CDC.
3. **The host's rules all key on `coop_room_change_other`** (hle.c:11351), which looks at no room. So split and apart are
   refused alike. They were written for follow mode (round 4 doorhold, round 5 doorcasts, 2026-11-06): "their zones and
   presses can't start a second room change", and a Tome opened in the fade "showed black once the new room was in"
   (coop-no-pause.md row 69; hle.c:27642-27646).
4. **Player 1 staying is refused mostly by the game** (lock, Start lock, prompt flag, fade), and the host adds the zone-look
   refusal and the quick-cast refusal. Their room_turn (hle.c:20342-20373) runs only their walking controller. The play
   channel (B, Start, quick spells) is stepped only in the world frame, with the game's (locked) copy in.
5. **Player 2 staying is refused mostly by the host** (zones and B off, quick casts waiting, Start refused). The lock does
   not reach their copy. A copy Tome opened in the window would carry the game's copy's lock with it [inferred].
6. **Two traps for any change:**
   - player 1's own words, find or scene sets busy 8, 16 or 32. Then coop_room_change_on goes false (hle.c:11347), and
     with it room_turn, room_other, the held-ask answer and the whole split hold (split_on, hle.c:24614). G2 also stops
     the countdown for it (hle.c:24911). Player 2's own words, close-up and find put nothing in the busy word
     (hle.c:13107-13110), so they don't do this;
   - the ticket is one record. A second fn66 is passed over (hle.c:16761) and a second ticket is refused (G1,
     hle.c:24827). A stayer's door or zone way-out press is lost, and a cast or scene of theirs that ends in a ticket loses
     it.
7. **Nothing reads r13-23392 but the room load** (0x801522CC, which picks its path and clears it) [read]. The host only
   keeps it round player 2's look (hle.c:16969, 17017). It gates no press.
8. **coop_door_turns never refuses a stayer.** It is set only for player 2's own door (hle.c:21790) and cleared at their
   arrival (hle.c:21623). It refuses the mover's (player 2's) own B and look.

## 1. The host's gates

| # | Gate | Where | Blocks | Why it was added |
|---|---|---|---|---|
| H1 | `coop_room_change_on`: busy has 4, nothing but 4\|2\|1, ticket +0 > 0, no cutscene, no menu | hle.c:11343-11349 | the test every H-gate builds on. False with busy 8/16/32, a cutscene or a menu | "a room change on its way" for round 4's doorhold (2026-11-06) [read the comment 11335-11341] |
| H2 | `coop_room_change_other(cpu, p2)`: H1 and the change is not that player's | hle.c:11351 | each H-gate below asks it for the stayer. It looks at no room: split and apart alike | the same; "the one who didn't ask" |
| H3 | `room_turn`: player 1's turn in player 2's change, the hold opened, from `g_coop_p1_free` | hle.c:20342, 20358-20373; coop_player1_turn 19820-19907 | **player 1 staying.** It gives a walk only. The turn runs the pad update 0x8013E518 and the controller 0x80045BC4 (19876, 19889). The play channel r13-26224 (A/B/Z prompts, Start, quick spells, R) is not stepped in it | player 1 stood ~390 frames through player 2's door (row 69). doorhold-1 and -3..-5 failed (failed-attempts 542-543): the turn had to start from their last free copy |
| H4 | `room_other`: player 2's turn runs past the busy refusal | hle.c:20869-20870 | lets **player 2 staying** have a turn at all | the same round: "they go on through it, as player 1 does through theirs" |
| H5 | player 2's zones and B off while `room_other` | hle.c:21000-21003 (`g_coop_prompt = -1`, no coop_prompt_look) | **player 2 staying**: no zone look, so no prompt, no B, no examine, no pick up | "a way out or a scene would ask for a second room, or play, over the one on its way. Brought along, they look again" [read 20998-20999]. M3 3852: their B gave nothing [read there: stayer-readers §1] |
| H6 | player 1's zone look refused while `coop_room_change_other(cpu, false)` | coop_scan_player1, hle.c:20183-20188 (intercept on 0x80134A44, install 28870) | **player 1 staying**: no zone comes into, no new prompt | the same reason [read 20180-20182] |
| H7 | `coop_quick_cast_ask`: the game's 0x800069DC passed over for the non-asker | hle.c:19774-19786 (install 28959) | **both stayers.** Player 1's press from the play channel; player 2's own cast, which is a guest call of 0x800069DC with their id in (12511-12513) | round 5 doorcasts: "player 1 could cast through player 2's door motion and fade, which may ask for a scene or another room" [read 19770-19772; row 69] |
| H8 | `coop_quick_cast(cpu, room_other \|\| coop_left_behind)`: player 2's press dropped while they wait | hle.c:21090; 12475-12480 | **player 2 staying.** `coop_left_behind` is false in a resident room under the stay (16244-16253), so only `room_other` refuses | player 2 cast at the old room's numbers 30 frames after player 1's change (logs/run-20261106-092849-jtg-dc3-c12p1-p2cast) [read 21086-21088] |
| H9 | player 2's Start refused (`start_room`) | hle.c:27647-27650; no `g_tome_copy_ask` then (27725-27735) | **player 2 staying.** No copy Tome | a copy opened in the fade "showed black in their half once the new room was in" (logs/run-20261106-081752-jtg-vdh6-c1p1-p2start); one opened before the ticket stays whole (run-20261106-101418) [read 27642-27646] |
| H10 | the same refusal in the branch where a Tome already shares the frame | hle.c:27424 | **player 2 staying**, with player 1's Tome up | the same rule |
| H11 | R4: a second fn66 while H1 is on returns 0 | coop_script_go_to_room, hle.c:16761-16769 | **both**: a stayer's door, way-out zone or any script's room request | "one change at a time (the ticket is one record): the second waits for a new press" (R4, 2026-11-11) [read 16761] |
| H12 | G1: a second ticket in the window refused; room requests 1/16/32/64 held at the dispatcher | split_ticket_ask 24827-24850; split_requests 24876-24903 | **both**, in a window | design-split-hold §3.8, §19.3: one ticket record; risk 12 (a cinematic's own return ticket lost) |
| H13 | G2: the countdown waits while busy 8/16/32 begun in the window or a cutscene is up (900-frame cap). Player 1 the mover: their lock renewed with 0x800443E4(+0 + 3) each waiting frame | split_tick 24905-24950 | not a refusal. But the stayer's own words/find/scene stop the mover's door, and (player 1 staying) turn H1 off for everyone | §3.9: a scene, words, find or cutscene begun in the window must not have the change fire under it |
| H14 | G3: no window when the stayer is casting at the ticket | split_ticket_open 24806-24807; split_stayer_casting 24736-24742 | **both**: a stayer mid-cast at the ticket gets today's whole-room hold | §3.10: the ticket's cast end 0x801C5118(0) ends only the controlled id's cast, so the stayer's would finish in the window and could summon or switch bodies (refute-split-hold-goal §3) |
| H15 | `split_on` needs H1 | hle.c:24614 | the split hold's allow list (A1-A3) ends as soon as player 1 staying opens words (busy 8) or a find (16) | §2: "the room change is all that's on" |
| H16 | a kept B is dropped by any ticket | player 2's: hle.c:21046-21047; player 1's: 27340-27342 | **both**: a B kept from a refused frame is let go, "a room was asked for" | row 69: "Both players' B during the other's door is let go, never kept, by design" |
| H17 | `coop_door_turns` (COOP_DOOR_FADE 120) and `g_coop_door_p2_until`/`g_coop_exit_p2_until` | 16625-16629; set 21790; cleared 21623; used 16781, 16789, 16885, 20870, 21024, 21054 | **only player 2 the mover** (their B, their look, their turn through their own fade). Never the stayer | player 2 stood through their own door's fade; a 300-frame window ran out before script 132's ticket (failed-attempts 470) |

Host gates that do NOT refuse the stayer here: `coop_node_held_ask` (19748-19767) frees the stayer's body; `coop_busy_ask`
(19708-19735) answers only its four cases and the split list; `coop_left_behind` is false under the stay; player 2's own
words, close-up and find refusals (`coop_p2_own_up`) are about their own windows, not the change.

## 2. The game's gates

| # | Gate | Where | Blocks | Why (the game's) |
|---|---|---|---|---|
| G-a | The busy word r13-23344 (4) | 0x80155738's 35 sites; the player's filter 0x80058E80-0x80058EB0 | **no press directly.** Not asked by 0x800069DC, 0x801C4250, 0x80006CDC, 0x80006EBC, 0x80117628 or 0x80134A44 [read the site list]. The filter swallows hits, grabs and 202 (FINISH HIM), not 135 or 185 [read there]. In a window A3 answers the stayer's filter "not busy" anyway (split_busy_allows 24733-24734) | solo: everyone but the mover stands |
| G-b | The input lock r13-23592, `0x800443E4(count + 3)` | ticket writer 0x801557F0-0x801557F4; max-setter 0x8013E99C; counted down in 0x8013E518 (0x8013E804-0x8013E81C); a lock skips every channel's rows (0x8013E210 -> 0x8013E344) | it lands in the game's input copy. **Player 1's door:** player 1, the mover, as solo. **Player 2's prompt door** (run as a use, coop_use_start 18182-18220 swaps no input copy): **player 1, the stayer**: no row of the play channel runs, so no B, Start, quick spell or R for ~65 frames. Their walk in room_turn is from their own free copy and is not locked. **Player 2's walk-in zone door** (in their turn, their copy in): their own copy [inferred, not traced] | solo: the mover stands through their door [read there: b3-input-lock-readers.md §1-4] |
| G-c | The Start lock r13-26232 (same call, 0x800068E0) | 0x800068E0-0x800068F4; counted down by the play channel's +0 handler 0x80006900 (only when it runs, world frames); tested at 0x80006CF8-0x80006D00 | one shared word, never swapped (not in CoopPlayerState, hle.c:12010-12017). **Player 1 staying, either door**: their Start refused ~65 frames. Player 2's copy Tome does not go through 0x80006CDC (it writes the mode words, hle.c:15020-15021, and calls TW_MODE_CHANGE, 15060) [read], so this lock doesn't reach it | solo: no Tome over a door |
| G-d | The prompt flag 0x10000 of r13-24048 | set 0x801557E0 (0x80117260(0x10000, 1)); cleared at the change 0x8015203C; the world frame's clear 0x800084D8-0x800084EC (0x8011727C(-1), then 0x80117360(0) zeroes the shown record 0x80335AE0); the chooser returns at 0x801176B4 while the word is not 0 | shared word (not in CoopPlayerState). **Player 1 staying**: no prompt is shown, and their B (0x80006EBC reads the shown record: 0x80006EE4 -> 0x8011799C, +12 of 0x80335AE0) finds nothing. **Player 2 staying**: their B reads their own slots in coop_prompt_look (hle.c:16887-16892, slot +8 and +16), not the shown record, so it does not block them [inferred from the reads]. Only three readers of r13-24048 exist: 0x8011727C, 0x80117260, 0x801176B4 [read] | solo: prompts off during a door |
| G-e | The fade's state | 0x801E7C98 (r13-21784 ticks, r13-26896 bytes), first gate of 0x80006CDC (0x80006CF4, 0x80006D04) | shared. **Player 1 staying**: Start refused once the fade starts (+4 delay out), to its end | solo: no Tome in a fade |
| G-f | 0x80006CDC's other gates | 0x8004303C (r13-25696 the play hold), both walking channels held (0x80199F50/74), event 135 answering 1, 0x801C551C, 0x8006DB04(1) (0x80006D0C-0x80006D94) | not set by the ticket [read: the ticket writer's calls]. Listed so nothing is missed | solo |
| G-g | 0x800069DC's own gates | 0x801F68E0(player) bit 0x4000 (0x800069F8-0x80006A00); the record 0x80307170 +8 == 1 or 2 (0x80006A04-0x80006A1C); flag 307; r13-26228 != 0 (0x80006A3C); event 185 answering 1 (0x80006A5C) | none is the ticket's. r13-26228 is set by 0x80006968 at 0x800179AC and 0x800A621C, not by the ticket [read] | solo |
| G-h | The cast end | ticket writer 0x801C5118(0) | the **controlled** id's cast only: the mover's. The stayer's cast goes on [read there: design-split-hold §3.10] | solo: a door ends the player's cast |
| G-i | Busy 8/16/32 from the stayer's own words, find or scene | the busy set 0x801556CC | **player 1 staying**: their words are the game's window and set 8 (16 a find, 32 a scene). That turns H1 off (H15) and G2 waits (H13). Player 2's own sets don't touch the word [read hle.c:13107-13110] | solo: words hold play |
| G-j | Holdable records and roomless spell nodes | record steppers 0x80144B98, 0x8017BF14, 0x80182A60, 0x80182B18 keep the real word (design-split-hold §3.3); roomless nodes are never on A1 | a **cast's effects** may freeze for the window: which spell records or nodes carry the holdable mark is unread [read there: hold-readers.md:316, "inferred"] | solo |
| G-k | r13-23392 | written 0x801557A8; read only at 0x801522CC (the room load) | nothing. No press reads it [read: the three hits in disasm.txt] | the load's path |

## 3. By direction and action

### 3.1 Player 1 staying (player 2's door), split or apart

| Action | What refuses it, in the order it is met |
|---|---|
| Walk, attack | nothing: room_turn from `g_coop_p1_free` (H3) |
| B on a prompt (examine, pick up, a door) | no zone comes into (H6); the game's shown prompt is cleared every frame (G-d); the play channel's rows are locked (G-b, prompt door only); a door's fn66 would be passed over (H11, H12) |
| Zone look (walk-in examine) | H6 |
| Quick spell | play channel locked (G-b, prompt door); H7 refuses 0x800069DC anyway |
| Start / the Tome | play channel locked (G-b); r13-26232 (G-c); the fade (G-e). Also: the real Tome is the game's menu mode, which turns H1 off (11348) and puts the change under player 1's Tome. design-split-hold §7 left it "unchanged". The host already has a copy-Tome route for player 1 (`g_tome_copy_p1`, hle.c:27799-27808), used only in player 2's close-up today |
| Cast from the Tome | follows Start |
| Opening words, a find or a scene (if a press got through) | G-i: H1 off for everyone, the split hold ends (H15), G2 holds the mover's change (H13) |

Why the play channel matters: coop-failed-attempts.md:560, "An intercept on player 1's Start 0x80006CDC in player 2's room
change was never reached: the game doesn't run it then." That is G-b: the rows don't run [read there: b3 §4].

### 3.2 Player 2 staying (player 1's door), split or apart

| Action | What refuses it |
|---|---|
| Walk, attack | nothing: H4 gives their turn; their own copy has no lock |
| B on a prompt, zone look, examine, pick up | H5 (no coop_prompt_look, `g_coop_prompt = -1`). Not G-d (their slots are their own). A door press would meet H11/H12 |
| Quick spell | H8 (drops the press), then H7 if it got to the guest call |
| Start / the copy Tome | H9 (and H10 when a Tome shares). Not G-c (the copy doesn't call 0x80006CDC). **The copy carries the lock:** tome_copy_step copies cpu->ram (hle.c:14977), taken outside player 2's turn, so the game's input copy (player 1's, locked by their own door, G-b) and its r13-23592 go into the copy. The copy's after-frame list runs 0x8013E518 (hle.c:15071), which counts it down there. So the copy Tome's own input rows may not run for ~65 of its frames [inferred; needs a run] |
| Cast from the copy Tome | follows Start; the cast starts in the real world after the copy closes (design-split-hold §3.10) |
| Their own words, close-up, find | nothing in the busy word (hle.c:13107-13110): H1 stays on, the split hold goes on |

### 3.3 Split and apart

- No gate above looks at rooms. H2 is room-blind, G-b..G-e are single words. So split and apart are refused alike
  [read; stayer-readers.md §0.4 says the same].
- Apart, player 2 staying is in their own resident slot in their turn (`coop_stay_room_in`, hle.c:20980), and
  `coop_left_behind` is false (16244-16253). So nothing extra refuses them apart.

## 4. What a change would touch (for the designer; nothing here is built)

- **The host's own gates (H5-H10)** are follow-mode rules. Under `coop_stay()` the stayer's half never changes room, so
  "black once the new room was in" and "cast at the old room's numbers" no longer apply [inferred]. What stays true is the
  one ticket record (H11, H12). A press whose script asks for a room still has to be refused or kept, never lost
  silently.
- **Player 1 staying needs the play channel run with their own copy.** Their B, Start and quick spells are bindings of
  r13-26224, stepped only by 0x80006A94 at 0x800084F4 with the game's copy in [read there: b3 §3]. Inside room_turn their
  free copy is in and unlocked. The game's own stepper 0x80006A94 could be guest-called there [inferred; not checked
  against the R row and the play channel's +0 handler, which counts r13-26232 down a second time in that frame].
- **The shared words are one per game:** r13-26232, 0x10000 of r13-24048 and the fade. They would have to be answered
  for the stayer, or kept per player, without touching the mover's solo door [inferred].
- **Player 1's words in the window set busy 8.** Before letting player 1 press B on an examine, H1's "nothing but
  4|2|1" (11347) and G2 need a rule for "the stayer's own words", as step F gave player 2 their own set [inferred].
- **Spells:** which spells' ends ask for a scene, a room or a body switch is still unread (stayer-readers Q6,
  refute-split-hold-goal §3). It decides which casts may go through H7/H8 and what H14 must still refuse.

## 5. Runs that would settle what reading can't (user or harness)

1. Player 2's Start in player 1's countdown, with H9 off in a test build: does the copy Tome take player 2's buttons in
   its first ~65 frames (the carried lock), and does its close find the stayer's room whole?
2. Player 1's B, Start and Y in player 2's countdown, a prompt door (OPEN DOOR) and a walk-in zone door: confirms G-b
   lands on player 1 for the first only.
3. A quick spell by each stayer with H7/H8 off: what the cast does when the change fires mid-cast.
4. Player 1 examining a sign (words, busy 8) in player 2's countdown, H6 off: confirms H15 and G2's wait.

## 6. Facts read

| Fact | Where | Mark |
|---|---|---|
| coop_room_change_on / _other | hle.c:11343-11351 | [read] |
| room_turn runs player 1's pad update and controller only | hle.c:20342-20373, 19876, 19889 | [read] |
| player 2's turn past the busy refusal by room_other | hle.c:20869-20870 | [read] |
| player 2's zones and B off by room_other | hle.c:21000-21003 | [read] |
| player 1's zone look refused | hle.c:20183-20188; install 28870 on 0x80134A44 | [read] |
| quick-cast refusal on 0x800069DC | hle.c:19774-19786; install 28959 | [read] |
| player 2's quick cast waits | hle.c:21090, 12475-12480 | [read] |
| player 2's cast is a guest call of 0x800069DC with their id in | hle.c:12511-12513 | [read] |
| player 2's Start refused in player 1's change | hle.c:27424, 27647-27650 | [read] |
| the copy Tome opens by mode words and TW_MODE_CHANGE, not 0x80006CDC | hle.c:15020-15021, 15060 | [read] |
| the copy is a copy of cpu->ram; its after-frame runs 0x8013E518 | hle.c:14977, 15071 | [read] |
| player 1's copy Tome route, in player 2's close-up only | hle.c:27799-27808 | [read] |
| one fn66 at a time | hle.c:16761-16769 | [read] |
| kept B dropped on a ticket | hle.c:21046-21047, 27340-27342 | [read] |
| coop_door_turns set and cleared for player 2's own door only | hle.c:16625-16629, 21790, 21623 | [read] |
| split_on, G1, G2, G3 | hle.c:24614, 24806-24807, 24827-24903, 24905-24950 | [read] |
| player 2's own windows put nothing in the busy word | hle.c:13107-13110 | [read] |
| coop_use_start swaps no input copy | hle.c:18182-18220 (be_player2, kinds, script states, zones, cameras) | [read] |
| CoopPlayerState: input, control, aim, globals -25680..-25632, pad -23604..-23548, five interact words | hle.c:12010-12017 | [read] |
| 0x80006CDC's gates | 0x80006CF4-0x80006D94 | [read] |
| 0x800069DC's gates | 0x800069DC-0x80006A7C | [read] |
| 0x80006EBC reads the shown prompt (0x80335AE0 +12) | 0x80006EE4, 0x8011799C | [read] |
| the world frame's prompt clear; the chooser's r13-24048 test | 0x800084D8-0x800084EC, 0x80117360, 0x801176B4 | [read] |
| r13-24048's only readers | 0x80117260, 0x8011727C, 0x801176B4 (callers of 0x8011727C: 0x80008490, 0x800084DC, 0x8009E7BC) | [read] |
| the play channel stepped at 0x800084F4; its rows | 0x80006A94; verbs.md:240-247 | [read; table read there] |
| the play channel's +0 counts r13-26228 and r13-26232 | 0x80006900-0x80006964 | [read] |
| r13-23392 read only by the room load | 0x801522CC-0x801522DC; written 0x801557A8 | [read] |
| busy-ask sites (35) | disasm.txt `bl 0x80155738` | [read] |
| the filter's swallowed events (not 135, 185) | mover-readers.md §2 (0x800CA964) | [read there] |
| the lock's landing per door | b3-input-lock-readers.md §4 | [read there; the zone-door case inferred there] |
