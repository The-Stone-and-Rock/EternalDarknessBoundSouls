# Refute: the Tome-room design against the game (2026-11-14)

Checks docs/research/separate-rooms/design-tome-room.md (and its four readers) against the game: the disassembly
(`ed dis`, build/ed-db/disasm.txt), the scripts (`ed script`, build/ed-db/scripts/*.lua), the DOL's cinematic table at
0x80242150 (read with tools/ed/disc.py `Dol.word`), build/ed-db/rooms.json, and the logs named. Read only: nothing was
edited, built or run.

Each point is **[real]** (wrong as written, with the evidence) or **[maybe]** (a risk or a missed path, not proved
wrong). Inside a point, each claim is tagged [read] (address, script line, file:line or log line given) or [inferred].

---------------------------------------------------------------------------------------------------------------------------
## 1. [real] RO (and RT when together) is not live at the return: the film's end kills the other slot

The design says O's return "finds RO live in the other slot ... RO put back at the trip's tail and untouched since"
(P7), "RO is live in the other slot, so a swap and no unpack" (§4.1 step 8), "O goes first ... into RT's live record: a
swap and no load" (§4.3), and run 3 passes on "`[rooms] a return into room 211 ... kept`".

- The film's end reloads the room in play through the ordinary load: 0x80157584 `lwz r3,-23276(r13)`,
  `li r4,68`, `bl 0x80152298` [read `ed dis 0x80157458`]. It is taken when the row's +24 halfword is -1
  (0x8015756C-0x80157580) [read]. Rows 112-120 all have +24 = -1 [read: DOL bytes, halves index 12]. The log shows it:
  `[room] change 0x00000096 0x00000044 ... calls: 0x80157590 0x800264C8 0x80050AC0` [read
  logs/run-20261113-181338-jtg-stayer-c3-153/page-0001.log, the line after "player 1's scene over" at frame 2649].
- Every 0x80152298 runs the tail 0x801523A8-0x80152404, which marks the other slot's record dead (+0x8140 := -1,
  +0x8142 := 0, +0x8143 := 0) whenever table +4 is not -1 [read `ed dis 0x801523A8`]. The flags' 0x40 bit skips only the
  room-script start 0x80153B2C (0x80152408-0x8015242C), not the dead mark [read].
- The host's tail then puts back only the rooms a player is in: the room in play and the stayer's
  (`notify_room_slots_tail`, hle.c:15071-15078) [read]. At the film's end both players are in room 5, so RO (or RT in the
  together case) is not put back [inferred from the two reads].
- So at O's return, `room_slots_return` prints "nobody stayed in it: loaded fresh" (hle.c:15100-15104) and the engine
  unpacks RO fresh [inferred]. This is safe (the dead-record rule), but it is a fresh load, not "kept".
- Fix the design's expectations: O's change is always a fresh load. Run 3's pass line must be "loaded fresh". In §4.3 the
  first change back (O's) is a fresh load of RT; T's then finds RT put back by O's tail (a player is in it), so only T's
  is "kept" [inferred]. Reader 4 §3's "RO may be live or dead" was right; the design hardened it to "live".
- The same reload happens with O never brought (O casting or in their Tome): then the stayer's RO is put back at that
  tail (hle.c:15072-15078) [inferred], so §5.4's case is unaffected.

## 2. [real] The trip trigger (P2) fires again after a trip that player 2 took

- P2's test is "the dissolve word r13-25680 non-zero (the asker's copy)", with no test of the ticket's room [read the
  design, P2].
- r13-25680 is the first word of the per-player block swapped round player 2's turn (`u8 globals[48]; // r13 - 25680 ..
  - 25632`, hle.c:12079; `coop_bytes(... - 25680u ...)`, hle.c:12362) [read].
- 0x80044200(on) writes only r13-25680 (and r13-25684 := 2 on "on") [read `ed dis 0x80044200`, 0x80044220, 0x80044248].
- The only `ed284(0)` on the trip's path is room 5's own 2675 (state 1, `screen_effect_on[ed284](0)`) [read
  `ed script 2675`]. It runs from the arrival's timers (lr 0x80163E5C) as the change's player, player 1 [read there:
  fx-a-1 log in reader 3 §6; that player 1's block is in then is inferred].
- The design itself says "When T is player 2, their copy keeps the 1 from their door" (P9) [read the design]. Then the
  next ordinary door player 2 takes, in their turn, reads r13-25680 = 1 and P2 marks it a Tome trip: both players are
  taken to that room [inferred from the reads above]. P9 is "build only if run 2 shows a lasting warp", so nothing stops
  this.
- Fix: make P2 also require the ticket's room r3 == 5 (every way into the Tome room is `fn66(5, ...)`: the seven doors
  [read `grep -l "fn66(5," build/ed-db/scripts/*/*.lua`: L03/892, L04/132, L06/335, L07/1111, L08/327, L10/1215,
  L11/358]), and clear the asker's copy at the trip (0x80044200(0) in player 2's turn, after their door) [inferred].
- See also point 11 (game code that turns the effect on).

## 3. [real] Chapter 13's way into room 5 exists: a film loads it directly, and the game saves and restores the place

The design's §11 item 11 says "how he gets into room 5 was not found, so no trip is marked there". Found:

- L13/1828 in room 64: `if fn25() ~= 64` skip; with `fn127() ~= 6`: `fn134(1, 5)` (camera saved), `ed164(L0)` (place
  saved), `ed55(134)`; with `fn127() == 6`: `fn134(2, 5)` and `ed165(L0)` (camera and place restored) [read
  build/ed-db/scripts/L13/1828.lua]. L07/1828 is the same script [read: `grep -l "ed55(134)"` gives L07/1828 and
  L13/1828].
- Cinematic row 134: +8 = 5, +14 = 352 [read: DOL bytes `00890000 00000086 00050087 FFFF0160 ...`].
- Mode 6's start reads row +8 (0x80050354). When it is a room other than the room in play it calls
  `0x801525D4(room, -2, 0, 0, 128)` (0x800503B4-0x800503D0): a direct load with no ticket [read `ed dis 0x80050340`].
- The film's end runs 352 (level 13 only): `place_at(player, EntryMkr02)`, `place_at_marker`, Camera04, `run_next(2675)`
  [read `ed script 352 --level 13`].
- The way back is 1413's chapter-13 branch, `fn66(64, EntryMkr01, FightCam04, 2, 1828, 0)` [read `ed door 5 --level 13`],
  so 1828 runs again in 64 with `fn127() == 6` and restores the one saved place and camera [inferred from the script].
- So in chapter 13 the game itself does "go to the Tome room and come back to where you were", for one player: `ed164`
  holds one place (buffer 0x80308E08) and `fn134` one owner key [read `ed fn ed164`, `ed fn fn134`]. No dissolve (2675 skips
  `ed284` in chapter 13, `if chapter() == 13 then goto L207`) and no ticket into room 5, so P2 never begins a trip there,
  and under the stay the other player is left where they are [inferred].

## 4. [real] fn66 cuts the sound after the writer, not before

- P6 says "`fn66` has already let the player go and cut the sound before the writer". In fn66's transition-0 path the
  let-go is before (0x801667A4 `bl 0x801F6828`), but the writer is at 0x801668E8 and the sound cut
  `0x801A05EC(0, 1, 0)` is after it, at 0x801668EC-0x801668F8 [read `ed dis 0x80166798`].
- Voiding the writer returns into fn66, so the cut still runs at the keep [inferred]. The two host-written tickets
  (P7, P8) call the writer directly and get no cut of their own; the load's own cut (0x80152340-0x80152354) still runs
  at each change to another room [read `ed dis 0x80152298`]. Minor, but the wording should be fixed.

## 5. [maybe] When player 2 takes the Tome, 1599 now runs as player 2's, not player 1's

- F17, F18, §4.1 step 6, §4.2 step 5 and run 2's pass ("1599 runs as player 1, `room() == 5`") rest on the 2026-11-08
  follow-mode log, where 1599 ran "who 1" [read logs/run-20261108-165807-p2-tome-moves/page-0001.log:3079, 3245].
- That run predates steps C and F. Today a find of player 2's leaves the thing's after-script (lr 0x800D9BA8,
  FOUND_AFTER_RAN) to run "as a use of theirs" (`coop_close_script_push`, hle.c:19338-19355), and a use of theirs runs with
  their identity, their room in and their script states (`coop_be_player2`, `coop_stay_room_in`, `coop_states_swap`,
  hle.c:18872-18876) [read]. A cinematic's end (lr 0x80026FE4) of a script marked theirs runs again as their use
  (hle.c:19518-19535) [read].
- So when K is player 2, 1599's film and its state-1 `fn66` would be player 2's, and that ticket would be a change of
  player 2's (g_coop_room_p2) [inferred]. P6's keep does not care who asks, but the fallback (P6's "mark the ticket as P2
  marks it") and the run expectations do. No run since 2026-11-11 has a player-2 Tome find (`grep -c "r3 1599 "` over
  run-2026111[1-4]-*: only run-20261113-181338, a player-1 find) [read]. Read this route before building.

## 6. [maybe] Chapter 3: a cutscene starts on the trip's arrival, and P5 waits for it

- 1351 (CameraRgn01 enter) in chapter 3 with flag 111 off: `cutscene[fn22](1, 1)`, `face_morph_to`, waits 100, then
  IntroCam01/IntroCam02 and look-ats over states 1-4 [read `ed script 1351 --level 3`].
- The trip arrives at room 5's EntryMkr01 (1072, -5, 337) [read rooms.json], inside CameraRgn01's box (969..1237,
  -161..161, 284..534) [read `ed room 5 --level 3`]. 2675 also sets IntroCam01 then [read `ed script 2675`].
- P5's bring waits for `!cutscene_playing`. The pre-stay bring had no such gate: the cutscene test in `coop_follow` is on
  the fall rescue only (hle.c:22511, `!cutscene_playing(cpu)`), not on the bring (22541 on) [read]. So in Anthony's chapter
  O may stay apart for the whole first-entry scene, unlike before separate rooms [inferred; it depends on whether
  `cutscene_playing` is already true in the bring's frame (the letterbox), not read].

## 7. [maybe] Max's Tome is a two-room trip that the design leaves to "today"

- 1599's Max branch (model 18): in 258 with flag 944 off, film 114, then `fn66(255, SpawnMkr72, HorrorCamera01, 0, 868,
  0)`; 868 runs 1599 again (`if L1 ~= 18 ... run_next(1599)`); in 255, a summon scene; then `fn66(258, MaxSpawnMkr01,
  Camera01, 0, 1599, 0)`; in 258 with flag 944 on, the letter (`fn104(0, player)` as the spawn place, `find(490)`, flag
  490, `magick_give(-1)`) [read `ed script 1599 --level 4`, lines L133-L404; `ed script 868`; `ed door 5` lists the 255
  and 258 returns].
- It has `room()` tests (258, 255) and no dissolve, so no trip begins (§8 says "they run as today") [read the design].
  Under the stay "today" is: a change of player 1's leaves player 2 where they are (coop_follow's stay branch,
  hle.c:22532-22540) [read]. So player 1 goes to 255 and back alone [inferred]. The user's rule ("each player goes ... as it
  did before") is not applied to this chapter's Tome scene.
- Karim's room-193 branches (1599 `if room() ~= 193`, flag 9; 1349 `if room() ~= 193`) also read the room in play [read].
  They change no room.

## 8. [maybe] The film's own ticket (0x8012D1A0) could be "the first ticket out of the Tome room"

- 0x8012D1A0, run from mode 6's frame, writes a ticket (transition 17, flags 32, entry -2, lr 0x8012D2F0) when its
  record's +120 is non-zero (0x8012D25C-0x8012D2EC) [read `ed dis 0x8012D25C`].
- For row 112 none was written: the film's end in two runs shows only the 0x80157458 reload, then 1599's ticket from lr
  0x801668EC [read 11-08 log :3245-3260; 11-13 log frame 2649]. Rows 113-120 were not seen.
- P6 keeps "the first ticket out of the Tome room". Safer: keep only the ticket from fn66's transition-0 call (lr
  0x801668EC) with entry 933455309 (EntryMkr_TomeReturn) or script 868, and log any other [inferred].

## 9. [maybe] Row +28 is a preload of the return room (answers §11 item 7)

- 0x8012D1A0 reads the row's +28 (`lha r3, 28(r3)` at 0x8012D1EC) and, when the block's +476 is 0 and it is not -1,
  calls 0x80155600(room) (0x8012D200-0x8012D214), when `(record+52) - r5 == 100` [read]. 0x80155600 is fn71, the ARAM
  room-file queue (table 0x8058AF60) [read `ed fn 0x80155600`: docs/game/research/arguments.md:291].
- Rows 112-120 +28: 150, 338, -1, 303, 115, 204, -1, 1, 43 [read DOL]. So the film pre-reads RT's file. The cache has two
  entries (readers.md:48) [read there]; RO's file may be pushed out, so O's fresh load (point 1) may read from disc
  [inferred]. Peter's row 119 queues room 1, while 1599 returns him to 17 [read]: a game quirk, harmless.

## 10. [maybe] A dissolve begun by player 2's door lasts about two frames

- 0x80044254 (called at 0x800081CC, after the frame's draw, in the world frame) decrements r13-25684 each frame while
  r13-25680 is 0, and 0x800081D8 draws the effect only while it returns non-zero [read `ed dis 0x80044254`,
  `ed dis 0x800081B0`]. r13-25684 is outside the swapped block (which starts at -25680, hle.c:12079) [read].
- The world frame's tick is "outside any turn" (hle.c:26285-26288) [read], so player 1's copy of r13-25680 is in there
  [inferred]. With player 2's door, player 1's copy is 0: r13-25684 = 2 counts out in two frames [inferred]. The
  strength is also scaled by the main player's (r13-26524) distance to the point (fog-host-readers.md:106) [read there].
- So §7's "the dissolve shows in both halves" holds for player 1's door (the 2026-11-14 test). For player 2's it likely
  does not. This partly answers §11 item 1.

## 11. [maybe] Game code, not only scripts, turns the screen effect on

- F2 is true of scripts. But 0x80044200 has 10 call sites: 0x8000AF58 (ed284), 0x8001E4F0, 0x8001FE28, 0x800A88DC,
  0x800A8F14, 0x800A8FC8, 0x800A905C, 0x800A90A8, 0x800A90BC, 0x800A90D0 [read `ed fn 0x80044200`]. Those at 0x8001E4F0,
  0x800A88DC, 0x800A8F14 and 0x800A8FC8 pass 1 (`li r3, 1`) [read `ed dis`]. What they are was not found.
- A door taken while one of them has the word on would trip P2 [inferred]. The r3 == 5 test of point 2 closes this.

## 12. [maybe] Saved-place slot 0 is a scratch word between P7's write and the runner's read

- `fn104` writes slot 0 in all 152 of its calls, 131 script files, mostly as a spawn place `fn104(0, fn78(1,0))` [read
  `ed fn fn104`]. The runner reads it at the change (0x80152688-0x8015268C) [read], one tick after P7 writes it.
- A script running in that frame (T's room 5 or another) could overwrite PO [inferred]. Write slot 0 at the runner's
  entry for O's change (notify_ticket_run) and put it back at that change's end [inferred].

## 13. [maybe] O's own door in flight when T's door calls fn66

- The door scripts turn the effect on in their state 1 and call fn66 only after `wait(120)` [read L04/132 L41-L75 and the
  six others: `fn21(0)`, `ed284(1)`, `fn36(120)` then `fn66(5, ...)`]. If O's ticket is on its way then, T's fn66 is
  passed over (hle.c:17437-17444) [read there: reader 3]; the script goes to `state_set(0)` [read L128], and the effect
  stays on, since 2675 never runs [inferred]. In solo this can't happen.
- For a door of player 2's, state 2 resumes from the timers (lr 0x80163E5C, as in fx-a-1). That lr is not in the host's
  "goes on as player 2's" list (hle.c:19518-19520) [read]. Whether state 2 then runs as theirs, so that the ticket and
  "taker 2" are theirs, was not read.

## 14. [maybe] Saves in or after the trip

- `ed how save` finds save regions only in rooms 93 and 274 (design-r4d.md:102) [read there]. Saving through the Tome
  menu was not read. In room 5, `deny_menu(180)` (2675) and `deny_menu(200)` (the door) end after a few seconds [read].
- A host save state taken in room 5 has no trip record. P6's fallback then takes both to RT together, which loses "each to
  their own room" [inferred]. Reader 4 §6 asked for a host blob for the record; the design says "no format change" [read
  both]. A save after the return, apart, is the general R7 question (not built: reader 3 §5) [read there].

## 15. [maybe] The mansion Tome (L00/1571) chains films that load rooms

- Row 122: +8 = 71, script 1571; row 1: +8 = 174, +28 = 341, script 2028 [read DOL]. L00/1571 plays 122 and then 1
  (`ed55(122)` at line 42, `ed55(1)` at line 67) [read]. So the mansion Tome ends in a film whose start loads room 174
  directly (point 3's path), a chapter's start. Outside the design; the stay leaves player 2 behind there too [inferred].

---------------------------------------------------------------------------------------------------------------------------
## 16. Open items the game answers

- **§11 item 4, r13-26280:** only 0x800A2834 writes 1, inside the game-code ticket writer that starts at 0x800A2548 (one
  of doors.md:61's seven). The runner clears it after its script (0x801527FC), and 0x801525B8, 0x801558D8, 0x800216A0 clear
  it too [read `grep "-26280(r13)" disasm.txt`, `ed dis`]. So at O's change it is 0 unless that writer's ticket was the
  last [inferred].
- **§11 item 5, entry with flags 1:** the runner skips the marker lookup (0x80152680-0x80152690) but passes the entry on
  to 0x80043380(effect, room, entry, camera) at 0x80152738-0x80152748 [read]. That is the door's sanity-effect starter; it
  returns at once unless 0x8004305C(room) gave an effect (> 0, 0x80043390-0x800433A4) [read]. Entry 0 matters only to an
  effect that uses it [inferred].
- **§11 item 7:** see points 8 and 9.
- **§11 item 11:** see point 3.

## 17. Claims checked and found right

- F1: exactly seven literal `fn66(5, ...)` [read grep]. Transition 2 is 12 ticks, 3 is 62 (`ed door 5`); reader 1's "10
  ticks" is the fade length, not the ticket's [read `ed door 5`].
- F2 for scripts: `ed284` in the seven doors and 2675 (L00, L03-L11, L13, L16) [read grep].
- F3: the block swap [read hle.c:12079, 12362].
- F5: L04/132's tests are `fn78(5,733)`, `fn78(5,831)`, `model_of == 150`, twice (state 0 and state 2); no flag, no
  `ed61` [read `ed script 132 --level 4`]. F6: 338's only way out is that ladder [read `ed door 338 --level 4`].
- F16: 2675 spawns only if `fn78(2, 733)` is nil [read]. Its start at the tail is skipped when the flags have 0x40
  (0x80152408-0x8015242C), so the film's 0x44 reload does not restart it [read].
- F19: rows 112-120 have +8 = -1 (no load at mode 6's start) [read DOL; 0x80050354-0x800503F8]. The film's end reload of
  the room in play does not unpack: 0x80154D64-0x80154DB8 finds the wanted room in the current slot and stops [read].
- F20: the seven return rooms and 868 [read `ed script 1599`]. 1599's `fn66` passes 6 arguments, so the 7th is fn66's
  default 1 (`li r31, 1` at 0x801666B8) [read].
- F21, and more: in all seven rooms EntryMkr_TomeReturn sits on the same spot as the marker the door's swirl centres on
  (`ed285`): 150 EntryMkr02 (218, 865, 993), 338 EntryMkr01, 303 EntryMkr02, 115 EntryMkr01, 204 EntryMkr02, 17 EntryMkr01,
  43 EntryMkr02 [read rooms.json and each door's `ed285`].
- F22: 868's follow-ups; 531, 1061 and 1161 are cameras only; 1349 is the world change in 338, with its zombies found
  "in this room" (`fn78(2, 174..178)`) [read the scripts].
- F23: bit 1 at 0x80152680 calls 0x8016490C(0); the heading 16(r1) is unset on that path (0x80152700-0x80152734) [read].
- F24: the writer takes r3-r10 and the caller's r1+8, +12, +16 into ticket +36, +40, +24 (0x8015577C-0x801557DC); the
  +592 stamp reads ticket +24 (0x80154D18-0x80154D28) [read]. The countdown is forced to at least 1 (0x8015578C-0x80155798)
  [read].
- F26: event 61 to r13-23288's room with f1 = -22248(r2) (0x801520E0-0x801520F4) [read]; r13-23288 is the old room in
  play, set by the load (0x80154CD4-0x80154CDC) [read].
- F29: 0x80109A68 tests type 120 (0x80109B10) and 30 (0x80109B1C) [read].
- Peter's event 57 goes to things 844, 840, 841, all placed in room 27 (`ed tag 844/840/841`), by `fn78(4, ...)` (any room)
  [read], so it works whichever rooms are resident.
- 2675's `hold(1)` is a let-go of the player (`ed fn fn21`: fn21(1) lets go) [read], so it lets go player 1 only.
