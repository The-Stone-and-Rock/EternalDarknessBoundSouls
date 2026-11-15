# Review: the Tome trip as built, against the design and the disassembly (2026-11-14)

Reviewed: `git diff runtime/host/hle.c` (the working tree on tools/game-kit, +569/-5), tome-trip-impl-notes.md,
build-run-tome.sh, against design-tome-room.md (final) and the user's decision (ED_TOME_BACK defaults to exact). Read only:
nothing was edited, built or run. Line numbers are hle.c's working tree.

Tags: **[real]** = wrong as built, with the evidence; **[maybe]** = a risk or a gap, not shown to be wrong.

---------------------------------------------------------------------------------------------------------------------------
## 1. Checked and as the design says (no finding)

- **The writer 0x80155770** (disasm 0x80155770-0x801557DC): `stwu r1,-16`; 24/28/32(r1) = the caller's r1+8/+12/+16 →
  ticket +36/+40/+24; r8 <= 0 becomes 1; +0 countdown, +4 delay, +8 room, +12 script, +16 flags, +20 transition, +28 entry,
  +32 camera. `tome_trip_go` fills r3..r10 in that order (room, entry, camera, 0, flags, 1, 0, 0) and stack 0, 0, 1.
  `tome_trip_write` puts the stack words at `((r1 - 0x800) & ~15) + 8..+16`, which is exactly the r1 `hle_guest_call` sets
  (main.c:77-78), and nothing runs between the write and the call that could move r1. Correct.
- **The tick 0x80155830**: a countdown of 1 runs the runner on the next tick; a delay of 0 starts no fade; +36 = 0 means no
  callback (0x80155900-0x80155914). Correct.
- **fn66's writer calls**: 0x801668E8 (lr 0x801668EC: countdown 1, then the sound cut 0x801A05EC(0,1,0)) for r22 = -1 (transition
  0, 1599's), and 0x80166930 (lr 0x80166934: countdown r22+2, then fn66's own fade 0x801E7C88 unless transition 16, then the
  cut). The lr test in `tome_trip_keep` matches both. 1599's way back (transition 0) takes lr 0x801668EC, so no fade is started
  by fn66 when its writer is voided. Correct.
- **The keep's registers**: at the intercept r1 is still fn66's (the writer's `stwu` has not run), so r1+8/+12/+16 are fn66's
  stores at 0x801668C0/0x801668D0/0x801668E0. `regs[i] = r3+i`, the log's `regs[3]` script and `regs[7]` transition are right.
- **The runner 0x80152614** (0x80152614-0x80152810): r3 room, r4 entry, r5 camera, r6 script, r7 flags (kept in r25); at
  0x80152680 bit 0 → `0x8016490C(0)` = 0x805CC2BC + 0*6; else the marker into 16(r1); place by 0x8016E764/0x80130EF0 on
  r13-26524; facing by 0x80124BE4 from 16(r1), unset for a saved place; camera 0x801EE998(r5); script r6 > 0 by 0x801642D4 at
  0x801527F4. `notify_ticket_run` is a notify at its entry, so `tome_trip_runner` sees r3 and r7 as asked, and it runs before
  the R4 bracket opens (it needs nothing of the identity). Lending slot 0 there is in time for 0x80152688. Correct.
- **0x80124BE4** = `0x8016EBF8(axis 0x80252E28, object+44, f1)`: it rewrites the four facing words +44..+59 whole, so the end's
  call replaces whatever the runner's unset 16(r1) gave.
- **The order back**: state 3 → B's ticket (state 4) → B's runner → B's end (no countdown, no bracket; the bracket of player 2's
  change is closed by `coop_stay_change_end` in the same pass, just before `tome_trip_step`) → B's come-into mark used or 120
  frames → A's kept ticket (state 5) → A's end → over. As P7/P8.
- **The wrap as player 2**: `coop_be_player2` writes r13-21496 (hle.c:12516-12519), so `notify_room_ticket` takes the ticket for
  theirs (`g_coop_entered_p2`, `g_coop_room_p2`) and the runner opens the R4 bracket. `coop_stay_room_in` is a no-op when they
  are not apart (B = player 2 leaving room 5 with player 1), and the room word is 5 then anyway. Correct.
- **The come-into mark's value**: `g_follow_fresh` is a player number (1 or 2), read so by `coop_follow_look` (hle.c:21565) and
  written so by the bring (23122). Setting it to `going` is right, for player 1 too (`p1_look`).
- **`g_coop_tickets`** is incremented (11513) before `tome_trip_ticket` (11523), so the trip's `tickets` equals the count for the
  `g_fade_mover = 0` line (11529) and `split_ticket_open`'s refusal (26503). The second `static unsigned g_coop_tickets;` at
  11427 is a legal tentative definition in C.
- **The bring**: `coop_follow` is called every frame from player 1's update (31318), so `tome_trip_brings` waiting is fine; the
  same-room branch returns before it, so the bring never runs on a pair already together. `trip_left` is read before the bring
  moves player 2. The 61's skip test (`left != p[0].room`) is the room the game's arrival already told, in all four shapes.
- **The hooks**: bit 1 at the stay's install (32097: runner and pass notifies), bit 2 at the writer intercept (32428); the writer
  intercept goes in with `coop_stay()` and the code checks only (32414-32427), not with ED_SPLIT_HOLD. `tome_trip_on` = co-op,
  player 2, sda, `coop_stay()`, the switch, both hooks. Solo and follow mode never reach a piece: every new line outside the
  `tome_trip_*` functions is behind `g_tome_trip.state` (0 unless `tome_trip_on`) or `tome_trip_change` (needs `tome_trip_on`).
- **P10**: `hle_coop_prompt_load` → `tw_state_loaded` (22064-22073) is the quick state's COPR chunk (quickstate.c:123), so every
  state load clears the record.
- **P3's camera keeping**: the set is the records' bytes as memory holds them, then depth and index as host words
  (`coop_camera_set_io`, 16740-16755); the index at `COOP_CAMERA_BYTES + 4`, the record at 0x660 + n*136 + 124 big-endian. The
  largest offset read (0xCBC) is inside COOP_CAMERA_BYTES (0xE34). Correct.

---------------------------------------------------------------------------------------------------------------------------
## 2. Findings

### R1 [real] A stuck timeout in states 3 and 4 throws away the kept way back: the player left in room 5 has no way out
`tome_trip_step` 15353 ("the way back kept, never free to write"), 15361 ("B's change never ran") and 15380 ("B back, never
free to write A's") call `tome_trip_stuck` → `tome_trip_clear`, which zeroes `regs`/`stack`. 1599 runs its state 1 once
(`state_keep[fn35](0)` before the fn66: `ed script 1599 --level 4` lines 16-22 / 31-37) and room 5 has no other way out
outside chapter 13 (`ed door 5`: only 1599 and 1413). So after any of these three timeouts both players (state 3) or A (state 4)
stay in room 5 for good: a soft lock, where the design's P11 meant a recovery. The same holds when `tome_trip_write` fails for
B (15276 prints the WARNING but the state is already 4, `ran` never comes, and 15361 clears 600 frames later).
Fix: on these timeouts (and on a failed B write) skip B and write the kept ticket as the game asked it (`regs`/`stack`
unchanged, as player 1's, or A's wrap), then clear; clear without writing only in states 1 and 5.

### R2 [real] The fallback keeps and rewrites tickets the design says run as today (chapter 13, one player in room 5)
Design §8: "1599's other branches (... chapter 13 to room 64): no ticket into room 5 and no keep". But 1599's model-142 branch
calls `fn66(64, EntryMkr02, FightCam02, 2, 1828, 0)` with no room test, and its model-9 branch the same after
`room() == 5` (`ed script 1599 --level 13` lines 317-327); `ed door 5` lists both as ways out of room 5. 1413's chapter-13
fn66(64) (`ed script 1413`) is kept too (P6 names it). With P12 not built (the user's decision), chapter 13 reaches room 5 by
the film with no ticket, so `state == 0`, `record` is false and every one of these goes through NO RECORD (15157-15170): the
writer is voided, the game's ticket is written again a frame later by the host. Chapter 13 is then no longer left to the game.
More generally, a fallback with B = 0 (one player in room 5) has nothing to do: it writes back the game's own ticket.
Fix: in `tome_trip_keep`, when there is no record and fewer than two players are in room 5, return false (pass the ticket as
written); with P12 unbuilt, also drop 1413 from the keep.

### M1 [maybe] Player 2's 1599 (K = player 2) ends as a use that asked no room, which can give them the screen and hold player 1
When 1599 runs as player 2's use (F18), its fn66's ticket is voided by the keep, so `notify_room_ticket` never runs and
`g_coop_tickets` does not move. The use's end then reads `room = false` (19498) and, if the busy word r13-23344 is non-zero
(`coop_busy`, any bit) or the use found something, sets `g_coop_screen = true` and calls `coop_hold_player1()` (19508-19511).
In solo/today the ticket's busy 4 makes `room` true and that branch is skipped. Whether any bit is up after the film is not
read; if one is, player 1 is held with player 2 on the screen while the host's tickets run.
Fix to consider: count the keep as a room asked for (`g_coop_tickets++` in the keep, after state 3; `tome_trip_change` is
over by then), or have the use end treat `g_tome_trip.state == 3` as a room.

### M2 [maybe] Slot 0 put back at the change's end can undo a room script's own fn104(0, ...)
Slot 0 is lent from the runner's entry to the behaviour pass after the change (15315). In between, 0x80152298 unpacks the room
fresh and starts its room scripts (0x80152408-0x8015242C, before the runner's read at 0x80152688). A start script there that
saves a place in slot 0 (fn104(0, thing): 152 calls in 131 scripts, design F25) has its write overwritten by our restore at the
end (and, as the notes say, would overwrite ours before the read). Fix to consider: at the end, put the saved bytes back only
when slot 0 still holds the three s16 we wrote; or restore right after the read (a notify at 0x8016E764's entry, lr
0x801526C0).

### M3 [maybe] Facing: one or more frames with an unset heading, and an inferred convention
With a saved place the runner faces the player with 16(r1) never written (0x8015272C-0x80152734, or 0x80152700-0x80152710 with
r26 != -1): a stack leftover, possibly NaN, into the quaternion, until `tome_trip_change_end` runs at the next behaviour pass.
The heading `2*atan2(qz, qw)` matches `coop_place_facing`'s own treatment (hle.c:7108-7121), so it is likely right, but it is
inferred. The record already keeps the four noted facing words (`p->facing`, unused): writing them back at +44..+59 is exact and
needs no convention; doing it in the runner's notify (after the runner returns, or at 0x8016E764's entry) would also close the
frame with the unset heading.

### M4 [maybe] An exact A gets 868's follow-up camera, not the noted one
Departure 10 asks A's noted camera in the kept ticket, but 868's `run_next` follow-ups 531 (model 74), 1061 (73) and 1161 (81)
set Camera01 and ease inside the same change (design F24), so for those characters A ends on Camera01 at PT, the marker's
camera, until a camera zone of the first look chooses. Look at run 3/5 pictures. Not wrong, only not what departure 10 says.

### M5 [maybe] A = player 1 sent to their own room (player 2 never brought) carries 868
Departure 8 sends the kept ticket to A's noted room (RO) when it is not the way back's. The ticket still carries 868 and the
way back's flags; 868's follow-ups then ask "Camera01" by hash in RO, a different room's camera of the same name or none
(§11.6). Edge case (player 2 casting or in their copy for the whole Tome scene).

### M6 [maybe] Chapters 3 and 6 with the taker exactly back inside the way in
Now that exact is the default, the taker stands back inside L03/892's plane or L06/335's BoxRegion02. The notes leave open
whether 335's second pass runs inside the come-into scan. `ed fn run_again`: fn34 copies the running script to r13-23220 and
0x80164470 runs it again at once as the script returns, inside the same 0x801642D4 call. 335 state 0 with the Tome carried does
`room_queue(143)`, `state_set(2)`, `run_again`, then state 2's `fn66(143, ...)` (`ed script 335 --level 6`): all in one call. So
if the come-into scan runs the region's script synchronously, the fn66 is refused and 335's state put back; that is what run
6a must show. `room_queue(143)` is not refused (a preload only, G9).

### M7 [maybe] The other half's white on the way back
B's ticket has no script and no delay, and sets `g_fade_mover` = B, so 1599's white is drawn only in B's half from B's
ticket on (A's half shows room 5 while waiting, design §7); A's ticket then sets the mover to A, and B's half drops the white
at once with no fade-in (868's fade is drawn in A's half only). For the pictures.

### M8 [maybe] The roles during player 2's trip countdown
With T = player 2 the writer runs inside their use: event 102 (0x801556CC → 0x801F5620(102, player 2, 0, r13-23276 = RT))
holds everyone in RT but player 2, so player 1 in RO is not held for the 12/62 ticks, while `g_coop_room_p2` is already false
(the notes' last-but-one point). A door of player 1's in that second is passed over by R4 (fine); a find of theirs is §11.15.

### M9 [maybe] A stale record outside a quick state load
Only a quick state load clears the record (P10). A return to the title or a memory-card load mid-trip leaves it; a new trip's
ticket resets it (15017-15022), but chapter 13's film way in has no ticket, so 1413/1599's keep there could read an old state-2
record. Rare; clearing the record when the chapter changes (or at any room change into a room that is not 5 while in state 2)
would close it.

### M10 [maybe] The run script makes and loads save states
build-run-tome.sh saves `saves/tome-chN.state` with ED_SAVE_STATE (made fresh on this build) and runs every case through
`run.sh STATE`, and makes the chapter 8/11 states in follow mode (`env -u ED_COOP_STAY`) before loading them with the stay on.
The design's §10 and the user's rule (fresh Jump to Game, then teleport; never key checks to a state) ask for jtg.sh runs. Made
on the same build the corruption risk is small, but the follow-mode state is not what a stay run starts from: if a run fails
oddly, repeat it from jtg.sh before reading the result.

---------------------------------------------------------------------------------------------------------------------------
## 3. Departures from the design, judged

1 (P1's place), 2 (hooks in `tome_trip_on`), 3 (exact default), 4 (the refusal first), 5 (`g_coop_set_in`), 6 (the change to
the next ticket), 7 (the spell word read directly; 61 by the noted room), 9 (the mark at the runner), 11 (B's entry), 12 (the
change's end by the runner and the countdown) and 13 (each ticket checked): all sound. 8 is sound but see M5. 10: see M4.
