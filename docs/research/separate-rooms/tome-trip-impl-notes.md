# The Tome trip, built (2026-11-14): implementation notes

What was built from design-tome-room.md (P1-P11; P9 dropped by the design; P12 and §11 items 2-4 not built, as the user
decided), where it is in runtime/host/hle.c, where it departs from the design, and what is not confirmed. **Built and
linked; not run.** Nothing committed. The runs are build-run-tome.sh (here).

The user's decision on §11 item 1: each player goes back exactly where they stood when the change into the Tome room
came. So `ED_TOME_BACK` defaults to **exact**; `ED_TOME_BACK=marker` puts the taker at the door's EntryMkr_TomeReturn,
as the design's first default did. `ED_TOME_TRIP=0` is today's behaviour, with no `[tome-trip]` line.

Line numbers are hle.c's after the build (working tree on tools/game-kit).

## The pieces

| Piece | Where | What |
|---|---|---|
| P1 record, switches | 11357-11441 (`TOME_ROOM`, `TomeTripPlayer`, `g_tome_trip`, `g_tome_trip_hooks`, `tome_trip_switch`, `tome_back_exact`, `tome_trip_on`, `tome_trip_change`, forward declarations, `tome_trip_state_loaded`) | One record. `tome_trip_on` = co-op on, player 2 there, `sda_ready`, `coop_stay()`, `ED_TOME_TRIP` not 0, **and both hook sets installed** (`g_tome_trip_hooks == 3`). |
| P2 the trip begins | `tome_trip_ticket` 15011, called from `notify_room_ticket` 11524 right after `g_coop_room_p2 = g_coop_entered_p2`; `g_fade_mover = 0` at 11530 after the mover line; `split_ticket_open` 26503-26504 | r3 == 5 and the asker (player 2 when `g_coop_entered_p2`, else r13-23276) not in room 5: state 1, the taker, the ticket count, `g_coop_room_p2 = false`. No stayer window ("the Tome room: both go"). A cutscene or the copy's turn is a WARNING line, not a refusal. |
| P2b passed over | `tome_trip_passed_over` 15039, called at the three pass-overs of `coop_script_go_to_room` (17961 M12's wait, 17978 M13, 17989 R4's one at a time), before their `r3 = 0` | r3 == 5: `0x80044200(0)` (ed284 off) and a WARNING. |
| P3 noted | `tome_trip_note` 15053, called first thing in `notify_room_change` 11448 (before `room_slots_return`) | State 1, r3 == 5, the room in play not 5, not noted yet: each player's +592, place, facing words +44..+59, heading `2*atan2(+52,+56)`, camera hash (`tome_trip_camera_of` 16848). |
| P4 pre-stay arrival pieces | `coop_p1_unplaced` 21539; the arrival scene's own camera 18544 | `coop_stay()` gates opened for `tome_trip_change()`. |
| P5 the bring | `tome_trip_brings` 15083, `tome_trip_brought` 15103; in `coop_follow` 23081-23084 and 23159-23160 | The stay branch passed over when the trip's change is over and player 2 is free; the existing bring runs; then event 61 to the room player 2 left and its direct streams stopped; state 2. |
| P6 the keep | `tome_trip_keep` 15121, first branch of `split_ticket_ask` 26557 | Our own tickets pass. A fn66 ticket (lr 0x801668EC / 0x80166934) out of room 5 asked from room 5 by 1599 or 1413: kept (r3-r10 and the caller's r1+8/+12/+16), the writer void, A and B chosen; another script's refused; another lr passed and told. State 3. |
| P7 B back | `tome_trip_go(…, 4)` 15235, `tome_trip_write` 15209, from `tome_trip_step` 15339, called in `notify_behaviour_pass` 14585 after `coop_stay_change_end` | When nothing is counting, busy 4 clear, no bracket: B's ticket by the game's writer 0x80155770 (B's noted room, flags 1 = saved place, B's camera, no script, countdown 1, stack 0, 0, 1), wrapped as player 2's (`coop_be_player2` + `coop_stay_room_in`) when B is player 2. |
| P7/P8 at the runner | `tome_trip_runner` 15282, first line of `notify_ticket_run` 14486 | Our change's runner: slot 0 of 0x805CC2BC lent (B's or A's place as three s16, cut toward zero), the come-into mark `g_follow_fresh = who` (3600 frames). |
| P7/P8 at the end | `tome_trip_change_end` 15305 | Facing by `0x80124BE4(object, heading)` for a saved-place arrival, slot 0 put back, the "back in room" line, WARNING if in the wrong room or more than 4 units off. |
| P8 A back | `tome_trip_go(…, 5)` from `tome_trip_step` | After B's end, B's mark used (`g_follow_fresh == 0`) or 120 frames (WARNING), then free: the kept ticket as A's (flags `| 1` and A's camera when exact; wrapped when A is player 2). At its end: "over", the record cleared. |
| P10 state load | `tw_state_loaded` 13656 | The record cleared (a lent slot 0 is the state's own, not put back). |
| P11 timeouts | `tome_trip_step`, `tome_trip_stuck` | State 1 with no change in 600 frames; 3 never free in 600; 4/5 whose runner never ran in 600; B back but never free for A's in 600: WARNING "stuck", slot 0 back, cleared. |
| hooks flag | 32097 (the stay's runner and pass notifies), 32428 (the writer's intercept) | `g_tome_trip_hooks` bits; the writer's install line prints `[tome-trip] the Tome room with the players apart: ...` (only with the switch on). |
| EVENTS.log | tools/coop-test/pagelog.py `KEY` | `[tome-trip]` lines listed (design §9: how EVENTS.log picks its lines was not read: it is this regex). `ed timeline` reads both.tsv only, not log lines. |

Log lines, all `[tome-trip]`, always printed: the switch line at install; `player N's door from room R to room 5: both go
there (...)`; `player N noted in room ...`; `the bring waits: ...` (once); `both in room 5: player 2 brought from room ...`;
`the way back kept: ... A player N (exact|marker), B player N (exact|marker|stays)`; `NO RECORD: ...`; `player N goes back
first|second ...`; `player N's change to room R runs (...)`; `player N back in room R at ... (noted ...)`; `over: ...`.
WARNING lines: begun in a cutscene or the copy's turn; a way in passed over (P2b); a player not noted; a camera not found;
a ticket out of room 5 by another lr (passed) or another script (refused); nobody in room 5; A's own room not the way
back's; a ticket not written; B's mark not used in 120 frames; no object after a change; the wrong room; more than 4 units
off; stuck.

## Address claims checked against the disassembly

- 0x80155770 (the writer): `stwu r1,-16`, then 24/28/32(r1) = the caller's r1+8/+12/+16 → ticket +36/+40/+24; r8 <= 0
  becomes 1; +0 countdown, +4 delay, +8 room, +12 script, +16 flags, +20 transition, +28 entry, +32 camera. **As the design.**
- 0x80155830 (the tick): +0 counted down, then at 0 the runner 0x80152614(+8, +28, +32, +12, +16), or for transition 17 a
  direct 0x80152298(+8, +16) with no runner; +4 (the delay) runs the fade 0x801E7C88 when it reaches 0, so **a delay of 0
  has no fade-out** (B's ticket: 1599's white is already up); +36 is a callback (bctrl, r3 = +40) after the runner, so the
  stack words 0, 0 mean none. Not in the design; used.
- fn66's writer calls: 0x801668E8 (lr 0x801668EC, countdown 1, then the sound cut 0x801A05EC(0,1,0)) and 0x80166930 (lr
  0x80166934, countdown r22+2, then fn66's own fade 0x801E7C88 unless transition 16). The other writer calls (0x8006BFF4,
  0x8009B808, 0x800A281C, 0x800ADAB8, 0x800C0594, 0x800E8D1C, 0x800E8FE4, 0x8012D2EC) are not fn66's. **As the design.**
- 0x80152614 (the runner): 0x80152298(room, flags) first; then unless r13-26524 is 0 or r13-26280 set: flags bit 1 →
  0x80043BE8(0); **flags bit 0 → 0x8016490C(0)** = 0x805CC2BC (`mulli r4,r3,6`; `lis -32675; addi -15684`), else the
  marker 0x80155348/0x80151914 into 16(r1); then 0x8016E764(r1+20, place), 0x80130EF0(player, r1+20), and
  0x80124BE4(player, f1 = 16(r1) [+ -22240(r2)]): with a saved place 16(r1) is never written. **As the design**; the
  facing after the change is ours.
- 0x80044200 (ed284): 0 writes r13-25680 := 0 (and two words of 0x8025BC30); 1 writes 1 and r13-25684 := 2. **As the design.**
- 0x801520E0-0x801520F4: `0x801F5620(61, 0, 0, r13-23288)` with f1 = -22248(r2). **As the design.**
- 0x801556CC (the busy set): the word r13-23344, and when non-zero event 102 by `0x801F5620(102, player, 0, room)` (the
  player from 0x801F62B8 passed as r4). The arrival clears every bit: `0x801556CC(-1, 0)` at 0x80152184. So "busy 4 clear"
  is true right after the arrival; the end of our change is told by the runner having run and the countdown out.

## Departures from the design (each a reading of the code, not a run)

1. **Where P1 lives.** Before `notify_room_change` (11357), not after `coop_stay_spared`: `notify_room_change`,
   `notify_room_ticket` and `tw_state_loaded` (13656) all come before 14370. The pieces are one block after
   `coop_stay_room_out` (14981-15407); the camera helper `tome_trip_camera_of` after `coop_cameras_in` (16848), where
   `g_coop_cameras` and `g_coop_set_in` are declared.
2. **`tome_trip_on` also needs the hooks** (`g_tome_trip_hooks == 3`: the stay's runner and behaviour-pass notifies and the
   writer's intercept). Without the pass's notify the kept ticket would never be written and A would stay in room 5.
3. **ED_TOME_BACK defaults to exact** (the user's decision), so with a record every player goes back by saved place; the
   marker only with `ED_TOME_BACK=marker`, and for the fallback (NO RECORD).
4. **P2's refusal of the stayer window is the first test** in `split_ticket_open` (before the copy's turn), so the split log
   says "the Tome room: both go".
5. **P3's camera reads `g_coop_set_in`**: player 1's set is in memory only when it is the one in (else `g_coop_cameras.p1`);
   player 2's is read from memory when theirs is in or when they have none yet (`!p2_set`, theirs is made from the game's).
   The kept set's records are the raw guest bytes (big-endian, `coop_bytes`), its index a host word.
6. **`tome_trip_change` lasts from the noting to the next ticket** (states 1 and 2 with the trip's ticket count), not only to
   the bring: the arrival scene's fn22 (Anthony's) may come after the bring in the same frame or the next.
7. **P5's "not casting"** reads player 2's spell word where it is (`g_coop_casting_p2_in` ? r13-22336 : `g_coop_casting[0]`):
   `split_stayer_casting` depends on `g_split.mover` and narrows to summons under the stayer presses. **P5's 61** is sent when
   the room player 2 left is not player 1's noted room (the room the game's change moved player 1 out of and already sent its
   61), in place of r13-23288 read at the change: the same room.
8. **P6, A's room.** A is player 1 if on the trip and noted in the way back's room, else player 2 if on the trip, **else player
   1 if on the trip** (player 2 never brought after their own door: player 1 is in room 5 noted in RO). When A's noted room is
   not the way back's room, A's kept ticket goes to A's own room at their place (a WARNING): the design had "nobody". The
   refusal also covers a second fn66 by 1599 once the way back is kept (state 3 and on).
9. **The come-into mark is set at our change's runner** (`notify_ticket_run`), not at its end: the end is the behaviour pass
   after the change, and whether the first look in the new room comes before that pass was not read; the runner moves the
   player in the same call, so no look comes between. P8 then waits while any mark is pending (`g_follow_fresh != 0`), only
   when B went by saved place. A is marked the same way when exact.
10. **A by saved place asks A's noted camera** (when found) in place of the kept Camera01/Camera04; 868's follow-ups (531,
    1061, 1161; 1349 in Karim's chapter) then set what they set, as in solo.
11. **B's entry**: the kept entry when B's room is the way back's room (the together case), else 0 (an entry is its own
    room's; with flags 1 the runner does not look it up).
12. **The end of our change** is "its runner ran (r3 == our room) and the countdown is out and no bracket", not "busy 4 clear"
    (the arrival clears busy itself, above). "Free to write" is countdown out, busy 4 clear, no bracket.
13. **Each host ticket is checked** after the guest call (+0 > 0 and +8 the room asked): a WARNING when not written.
14. The two new `getenv` reads are inside a clang `-Wdeprecated-declarations` push/pop, so the build's warning count is
    unchanged (421, all from code not touched here; the one non-deprecation warning, hle.c:5676 `-Wstring-plus-int`, is old).

## Not confirmed (read, inferred or open; nothing here has been played)

- The whole route: no run. build-run-tome.sh has the design's runs plus chapter 6's zone exit (6a-6c).
- The heading 0x80124BE4 takes is `2*atan2(qz, qw)` of the facing words: inferred from `coop_place_facing`, which treats the
  marker heading and that angle as the same. The "back in room" line prints both; the noted facing words are kept in the
  record but not written back.
- Chapter 6's exact return puts the taker inside BoxRegion02 with the Tome carried, where 335 tickets to room 143 at once
  (`run_again` to its state 2). The come-into mark refuses a fn66 or an exit prompt asked inside the first look's scan
  (`g_follow_refusing`, `coop_script_go_to_room` 17995); whether `run_again`'s second pass runs inside that scan was not
  read. If it does not, the taker leaves for room 143 on arrival: run 6a shows it ("[room] ticket" to 143 after "back").
  Chapter 3's L03/892 is a plane (SKPlane01): crossed again only by walking back over it.
- The behaviour pass at lr 0x80008400 / 0x80050A9C runs every frame of play after the film, so `tome_trip_step` runs.
- A ticket written from `notify_behaviour_pass` (a guest call at 0x8014FDB8's entry) behaves as one written by a script:
  `notify_room_ticket` runs on it (passthrough through `split_ticket_ask`'s guard), the countdown 1 runs at the next tick.
- When K is player 2, 1599 runs as their use; its fn66's ticket is voided, so `g_coop_use.tickets` sees no ticket: what the
  use's end does then was not read.
- B's and A's changes are ordinary changes: a stayer window may open for the host's tickets (shared: the one in room 5), for
  a countdown of 1 (B) or 1599's own countdown (A).
- Slot 0 is lent at the runner's entry and read at 0x80152688 after 0x80152298 returns; a room script started inside
  0x80152298 that calls fn104(0, ...) would overwrite it (none known in rooms the way back reaches; not read).
- With T = player 2, `g_coop_room_p2` is off from the ticket (the design's P2), so through the 12/62-tick countdown player 2
  (the asker) is "the other" for `coop_room_change_other`: their quick cast and Start are refused and their turn goes on;
  player 1 is "the asker" there, so player 1's look is not refused by the room bit while their room got no event 102 (the
  door script ran in player 2's room as their use). Player 1 may begin a look or a find in RO in that second; their words
  are closed at the change (`coop_room_change_words`), a find is §11.15. The alternative (keep `g_coop_room_p2` on to the
  runner and clear it there) was not built: the design's reasons for clearing it at the ticket (player 1's ticket counted,
  the sound rule's mover 1) stand, and this is the design's §11.15.
- The design's own open items stand: §11.5-16 (player 2's find of the Tome, camera index -1, 61 with the left room in play,
  `deny_menu` and the copy Tome, the frames of room 5 in A's half between B's change and A's, stream 81, ed285's strength for
  player 2's door, the pre-stay gates in P4 being all of them, a death in RO during the countdown, 0x8006E2F0's gate, player
  1 mid-find, the return markers in 150 and 303).
- P12 (chapter 13's film way in), Max's chapter, the mansion Tome: not built (the user's decision). Chapter 13's 1413 way back
  is still kept by P6 (the design's P6 lists it): with no trip begun it is the fallback (NO RECORD), which writes it back as
  player 1's at the game's spot a frame later.

## The build

`cmd //c build.bat` (cmake --build build-win --target EternalDarkness): hle.c compiled, `[3/4] Linking CXX executable
EternalDarkness.exe`, no error, 421 warnings (all `-Wdeprecated-declarations` from code not touched here, and hle.c:5676's
old `-Wstring-plus-int`). `sh -n docs/research/separate-rooms/build-run-tome.sh` passes. LF endings kept (`git diff --stat`:
no whole-file rewrite).

## Review fixes (2026-11-14, after tome-trip-review-design.md and tome-trip-review-safety.md)

Every point was checked against the code and, where it named game code, against `ed script` / `ed dis`. **Built and linked;
not run.** Nothing committed. The line numbers in the table above are from before these fixes; the pieces now sit at:
`tome_trip_change` 11491; the trip block 14999-15620 (`tome_trip_room_of` 15006, `tome_trip_casting` 15011,
`tome_trip_changing` 15016, `tome_trip_not_free` 15023, `tome_trip_slot0_back` 15040, `tome_trip_ticket` 15067,
`tome_trip_passed_over` 15100, `tome_trip_brings` 15150, `tome_trip_brought` 15174, `tome_trip_keep` 15204, `tome_trip_go`
15343, `tome_trip_runner` 15393, `tome_trip_rescue` 15419, `tome_trip_next` 15437, `tome_trip_change_end` 15457,
`tome_trip_wait` 15494, `tome_trip_orphan` 15503, `tome_trip_step` 15523); the quick-state blob 22297-22320.

### Fixed

- **R1 / safety §8 [real], a timeout dropped the kept way back.** Checked: 1599 asks once (`state_keep(0)` before its fn66,
  `ed script 1599 --level 4`), so a cleared record left the room-5 player there for good. Now no timeout clears a kept way
  back while a player is in room 5:
  - "not free to write" (states 3 and 4 after B's end) only waits, with one WARNING at 600 frames (`tome_trip_wait`);
  - a lost ticket (the write failed: `failed`, set by `tome_trip_go`; or its runner never came in 600 frames with nothing
    counting) runs **`tome_trip_rescue`**: the kept ticket as the game asked it (`asked_room`, the kept entry, camera and
    868, at the marker), as player 1's when player 1 is in room 5, else player 2's;
  - after any A's change, player 2 still in room 5 with A = player 1 gives **state 6**: `tome_trip_brings` lets the bring
    run once player 1's change is over in the way back's room (as co-op did before separate rooms); `tome_trip_brought`
    ends the trip there;
  - a second loss after the rescue clears with a WARNING (nothing more to do).
- **R2 / safety §10 [real], the fallback took tickets the game should keep.** Checked `ed script 1599 --level 13` (model
  142 and 9 branches: `fn66(64, ..., 1828, 0)`) and 1413. `tome_trip_keep` now:
  - passes 1413 (chapter 13, P12 not built) as written, clearing any trip record;
  - with no record, keeps (the fallback) only a ticket carrying 868 with both players in room 5; otherwise "left to the
    game" (a log line, not a WARNING), as written;
  - with a record, keeps 1599's or any ticket carrying 868 (safety §8's maybe: the way back is told by its 868 too, not
    only by r13-23228), and refuses others (§12 G13's door in flight);
  - refuses everything once the way back is held (state 3 on), as before.
- **M1 / safety §9 [maybe, confirmed in the code], a use of player 2's that ran 1599 ended as "no room asked".** Read
  `continue_coop_use`: `room = g_coop_tickets != g_coop_use.tickets`, and with no room, a busy bit or a find gives player 2
  the screen and holds player 1. The keep now counts as a room asked: `g_coop_tickets++`, `g_coop_room_asked` and
  `g_coop_room_asked_now` (the last two as `notify_room_ticket` sets them; `coop_cutscene_over` already treats "the Tome
  taken" as a room asked). The use's end then takes the room route and gives the cameras back.
- **M2 / safety §7 [maybe], slot 0 put back over a script's own write.** `tome_trip_runner` keeps what it wrote
  (`slot0_ours`); `tome_trip_slot0_back` (the change's end, and every clear) puts the old bytes back only while slot 0 still
  holds ours, else keeps the game's value with a WARNING. A script that wrote it before the runner's read would also show
  as "placed N units from their noted place".
- **M3 [maybe], the facing by an inferred heading.** The change's end now writes the four noted facing words back at
  +44..+59 (what 0x80124BE4 writes whole, review-design §1); the heading call stays only for all-zero words. The one frame
  between the runner (its unset 16(r1)) and the pass is left: it needs a hook after the runner's facing call, not built.
- **M9 / safety §4 [maybe], a record that outlives its game.** Each mover is checked in room 5 before their ticket
  (`tome_trip_next`): B not there is left where they are; A not there goes to the rescue (which clears when nobody is in
  room 5). State 2 with player 1 in another room clears (a game over, a reload). Co-op off or player 2 gone for 120 frames
  (`tome_trip_orphan`, from the step when `tome_trip_on` is false): player 1 still in room 5 in states 3-6 goes back by the
  way back as the game asked it, then the record is cleared, so a later player 2 never finds it. A reboot is a state load
  of the boot state (quickstate.c `restart_machine` → `load_state`), so P10 already clears it.
- **Safety §4 [real], A free in room 5 between B's change and A's ticket.** Checked: 0x80007490 holds a node only while the
  busy word is non-zero (`bl 0x80155738` before the held test 0x801F68D4), and B's arrival clears every bit, so the game's
  hold cannot be set without a busy bit of its own. Not held; A's ticket now waits (`tome_trip_not_free`) for no menu, no
  cutscene, player 2's copy closed and neither player casting, on top of the change machinery. B's ticket (state 3, at the
  film's own end, as solo's) has the same gate without the cutscene test. A can still walk in room 5 for those frames:
  harmless to their place (exact uses the noted one), but 852 (`ed script 852`) sets flag 614 again if its zone is entered
  with 614 and 679 clear (1599 just cleared them): **open, for run 1's pictures and flags**.
- **Safety §5 [real], a state taken with the way back held stranded room 5.** A new optional quick-state blob **COTT**
  (`hle_coop_tome_trip_*`, hle.h, quickstate.c after COPR, whose `tw_state_loaded` clears first): the record in states 2-6;
  a state without it, or taken in states 0-1, clears it as before. On load the trip's ticket count is made stale
  (`tickets = g_coop_tickets + 1`: P4's gates stay shut) and the guard off. Old states load as before (no section: cleared);
  a later host whose record has another size refuses the state, as for every blob.
- **Safety §2 [real], P2b put the dissolve out under the trip's own countdown.** `tome_trip_passed_over` now only logs
  while state 1 and the trip's change has not come ("the trip on its way takes them there"): the one passed over is brought.
- **Safety §3 [maybe, confirmed], the state-1 timeout after a long copy turn.** Judged only with no ticket counting.
- **Safety §1 [real, cosmetic], `g_coop_tickets` declared twice.** `tome_trip_change` is forward-declared with the others
  and defined after `g_coop_room_p2`; the first declaration is gone.
- **Safety §4 [maybe], player 1 moved mid-cast or in a menu by player 2's door.** Told: a WARNING at the trip's ticket. Not
  refused: the ticket is already in the game's record there (H8).

### Rejected, with the reason

- **M4, an exact A on 868's follow-up camera.** 531, 1061 and 1161 are the game's own follow-ups of its way back, as in
  solo; A's noted camera is asked and the follow-up then sets its own. Pictures of runs 3 and 5 will show it.
- **M5 / safety §10, A sent to their own room still carries 868.** Needed: 1599's `fade_set(white)` is cleared only by 868's
  `fade(0,0,0,0,40)`; without it A's half would stay white. The camera follow-up by name in another room is cosmetic and
  told (WARNING "own room").
- **M6 / safety §10, the exact return inside chapter 3's or 6's way in.** `run_again` reruns inside the same 0x801642D4
  call (review-design M6), and `coop_follow_look` runs the scan inside `g_follow_refusing`, so 335's fn66 should be refused;
  only run 6a can show it. No code change.
- **M7, the other half's white on the way back; M8, the roles in player 2's countdown; safety §9's `g_coop_room_p2` off
  from the ticket.** The design's choices (§7, §11.15), for the pictures and the logs.
- **M10, the run script's states.** States made on the current build by its own `s4 s6 s8 s11` runs from Jump to Game are
  what the user's rule allows (made fresh, thrown away at the next code change); the default order makes them first. Follow
  mode for the chapter 8/11 states is needed so player 2 is brought into the Tome door's room; if a run fails oddly, repeat
  it from `jtg.sh` before reading anything into it.
- **Safety §2 [maybe], M12's way out waits rather than being lost.** The Tome doors' fn66 comes from their state 2 after
  `wait(120)` (`ed script 132 --level 4`: state 1 `ed284(1)`, `wait(120)`; state 2 `fn66(5, ...)`), from the timer, never
  inside a look, so M12's wait (a look's way out) is not reached for them. Left as built: if it were, a dissolve left on with
  a way dropped later is worse than one put off.
- **Safety §10, the fallback puts both on one marker.** Now only with both in room 5 and no record (an old state loaded in
  room 5): `coop_follow`'s "stood inside player 1" rescue parts them. Rare; not worth a second place.
- **Safety §4, a door of B's during the wait; R4 brackets; identity swaps; passthrough flags.** Checked as the review says:
  no finding.

### The build

`cmd //c build.bat`: quickstate.c, main.c (hle.h) and hle.c compiled, `[5/6] Linking CXX executable EternalDarkness.exe`, no
error. hle.c 421 warnings, as before (all `-Wdeprecated-declarations` outside the code touched, and the old hle.c:5676
`-Wstring-plus-int`); quickstate.c's 14 and main.c's 31 are the old getenv/fopen deprecations, none on a changed line. LF
kept in hle.c, hle.h and quickstate.c. `sh -n build-run-tome.sh` passes; it uses only states it makes from Jump to Game on
this build, `ED_RUN_AWAY=1`, and no `ed test` or `ed chapters`.

### New log lines to read in the runs

`the way out of room 5 ... left to the game` (chapter 13, one player in room 5); `player N's way into room 5 passed over
(...): the trip on its way takes them there`; WARNINGs: `the way back has waited 600 frames (...)`, `... the way back as the
game asked it` (the rescue), `player 2 is still in room 5: the bring takes them` then `over: player 2 brought from room 5`,
`slot 0 of the saved places was written during the change`, `player N is not in room 5 ...: they stay where they are`,
`player 1 is in room R with no way back kept`, `player 2 gone|co-op off with the trip in state N`, `player 1 is casting|in a
menu as player 2's door takes them`; `a state loaded with the trip in state N`.

## Runs (2026-11-15, after the marker lookup in the taker's own room; fresh Jump-to-Game states, ED_RUN_AWAY=1)

`sh docs/research/separate-rooms/build-run-tome.sh s8 z8a z8b s6 6a 6b s4 z4 solo follow off`. The Tome's pickup is
started as script 1599 (test presses don't move its EXAMINE words; the user's pads do). No `[tome-trip] WARNING` in any
run, and no new hang report (the only one is saves/reports/hang-20261113-182925.txt, from before). The `ed check` findings
are the runs' own ED_PUT moves, and the chapter loads' stalls in the state runs.

| Run | What | Result |
|---|---|---|
| z8a | ch 8 apart: player 2 down in 211, player 1 takes 327 | both to room 5 (event 61 sent to 211); back: player 2 in 211 at -19 -1282 (noted -19 -1282), then player 1 in 204 at -20 679 (noted -21 680) |
| z8b | ch 8 apart: player 1 down in 211, player 2 takes 327 | "the taker stood in the way in's zone 2 (script 327); the game's return marker is in it: exact kept" (the fix: the lookup in player 2's room 204, not player 1's 211); back: player 1 in 211 at -24 -1258 (noted the same), player 2 in 204 at 257 -546 (noted 257 -547) |
| 6a | ch 6 together, player 1 in the unprompted exit zone 335 | taker at the marker (-6 -1425: "the game's return marker is not in it"), player 2 exact (-84 -413 -56) |
| 6b | ch 6 apart, player 2 in 59 | taker at the marker in 303, player 2 back in 59 at -211 -1346 (noted -211 -1347) |
| z4 | ch 4 (Karim) together, the ladder 132 | both back in 338 within 1 unit, then 1250/1349 (the zombies) run as player 1's, as in solo |
| solo | ch 4, co-op off | no [tome-trip], [stay] or [split] line; the same two room changes as before |
| follow | ch 4, ED_COOP_STAY unset | no [tome-trip] line; the bring as before |
| off | ch 4, ED_TOME_TRIP=0 | no [tome-trip] line; player 1 alone to room 5 |

Seen in z4, not a fault of the trip: at 4854, after the way back, the zombies' hits knocked the idle player 1 across 338
into the ladder's zone (-755 -23), and 132 took them to room 5 a second time, with both going. 132 sends the player to
room 5 only while they carry neither thing 733 nor 831 (`fn78(5, ...)`, the player's list); the shortcut (1599 started
directly) gives no Tome, so the ladder still leads to the Tome room. With a real pickup it leads to 339. Still to look at
with the user's play: whose list 132 reads when player 2 holds the Tome and player 1 climbs (the two-Tome build).
