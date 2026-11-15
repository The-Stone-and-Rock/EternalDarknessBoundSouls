# Reader 1: how the game takes the hero into a Tome room and back (2026-11-14)

For the feature: players apart (ED_ROOM_SLOTS=2, ED_COOP_STAY=1), either takes the way into the Tome room, both go there,
either takes the Tome, the film plays, and each goes back to their own room and spot. This reader covers only the game's
side: scripts, natives, transitions, return, flags. Read only: no code changed, nothing run.

Tags: [read] = seen at the script, address, file:line or log line given. [inferred] = worked out, not seen.
Script labels (`L95`) are from `python -I tools/ed/ed.py script ID --level N`; `.lua` lines are build/ed-db/scripts/LNN/ID.lua.

## 1. The short answer

- **Room 5 is the one Tome room for seven chapters** (Anthony, Karim, Lindsey, Paul, Roberto, Peter, the Major), and
  chapter 13's Alex. Max (room 258), Edward (room 236) and Alex in the mansion (room 71) take the Tome where it lies, with
  no trip to room 5. Pius and Ellia have no Tome. [read: section 5]
- **The way in is a per-chapter door script** (L03/892, L04/132, L06/335, L07/1111, L08/327, L10/1215, L11/358). Each
  checks "is the player this chapter's hero, and do they not carry the Tome yet", then plays the swirl for 120 ticks and
  calls `fn66(5, EntryMkr01, Camera02, 2 or 3, 1161, 0)`. [read: section 2]
- **The "dissolve" is not a transition number.** It is the screen effect `ed284(1)` + `ed285(marker, radius, strength)`,
  run in the door script's state 1 before the room change. The room change after it is a plain fade (transition 2, 10
  ticks, or 3, 60 ticks). Room 5's own room script 2675 starts the effect again on arrival for 30 ticks. [read: section 3]
- **The way back is in the Tome's after-find script 1599**, the same in every level. It branches on the player's model
  (`ed166`), plays that chapter's film (`ed55(112..120)`), and when the film ends calls
  `fn66(return room, EntryMkr_TomeReturn, Camera01, 0, 868, 0)`. Transition 0 is a cut, under a white `fn209` fade
  that 868 fades out over 40 ticks. [read: section 4]
- **The game does not remember where the hero was.** The return room is written into 1599 per model, and the spot is
  the fixed marker `EntryMkr_TomeReturn` (hash 933455309) in that room, placed in or next to the region of the door
  the hero came in by. Nothing is saved at the way in. [read: sections 4, 6]
- **The game does have its own "go somewhere and come back" mechanisms**, used by other scenes. They are not used for
  the Tome: `fn104(0, player)` + `fn66(..., 6th arg 1)` (L06/1838), and `ed164`/`ed165` + `fn134` + `ed303`
  (L09/1183, L13/1828). [read: section 7]
- **Room 5 vs 339 in L04/132**: room 5 only when `ed166(player) == 150` (Karim) and the player carries neither thing 733
  nor thing 831. Otherwise 339. No flag, and no insanity check. [read: section 2.2]

## 2. The ways in to room 5

### 2.1 Every entry (all literal `fn66(5, ...)` calls in the game)

`grep -rnE "fn66\(5, " build/ed-db/scripts/` finds exactly these seven [read]. `ed door 5` lists the same seven [read].

| Chapter (level, model) | Script, from room / region | Prompt | Gate for room 5 (else) | Swirl `ed285(marker, radius, strength)` | fn66 to room 5 |
|---|---|---|---|---|---|
| Anthony (L03, 85) | L03/892, room 150 SKPlane01 (a plane, cross_a) | none | not carrying 733, model 85, `ed61()==0` (else room 147, three variants by flags 326 and 55) | EntryMkr02, 1200, 0.5 | transition 2, then 1161 (.lua:41) |
| Karim (L04, 150) | L04/132, room 338 Region01 | CLIMB DOWN `ed301(2,4)` | model 150, not carrying 733 or 831 (else 339, transition 15) | EntryMkr01, 1200, 0.5 | transition 3, then 1161 (.lua:39) |
| Lindsey (L06, 73) | L06/335, room 303 BoxRegion02 | none | not carrying 733, model 73, `ed61()==0` (else 143, transition 2, then 1061) | EntryMkr02, 1200, 0.5 | transition 3, then 1161 (.lua:38) |
| Paul (L07, 74) | L07/1111, room 115 Region01 | OPEN DOOR `ed301(2,1)` | flag 902 off, flag 644 on, not carrying 733, model 74, `ed61()==0` (else 103 by door, or "locked" words when 902 is on) | EntryMkr01, 600, 0.5 | transition 2, then 1161 (.lua:55) |
| Roberto (L08, 76) | L08/327, room 204 Region02 | CLIMB DOWN | not carrying 733, model 76, `ed61()==0` (else 210, transition 15) | EntryMkr02, 1000, 0.4 | transition 2, then 1161 (.lua:38) |
| Peter (L10, 75) | L10/1215, room 17 Region01 | OPEN DOOR | (flag 471 off: flag 707 on) or (flag 471 on); not carrying 733, model 75, `ed61()==0` (else room 1 or 346 by door) | EntryMkr01, 900, 0.4 | transition 3, then 1161 (.lua:57) |
| Major (L11, 81) | L11/358, room 43 Region02 | CLIMB DOWN; whole script skipped once flag 253 is on | not carrying 733, model 81, `ed61()==0` (else 54, transition 15, then 1161) | EntryMkr02, 900, 0.4 | transition 3, then 1161 (.lua:39) |

[read: each script with `ed script ID --level N`; levels and models from build/ed-db/facts/levels.json]

- "Carrying" is `fn78(5, id)`: thing `id` in the player's list [read: `ed fn fn78`]. Thing 733 is the Tome spawned in
  room 5 by 2675 (types 30, 120 or 234). Thing 831 is Karim's second Tome, spawned in room 193 by L04/1381 [read: items
  in build/ed-db/facts/item_table.json, `spawned_by` of types 30/120/234].
- `ed61()` is `insanity_effect_id`: six of the seven refuse the Tome room while an insanity effect runs. L04/132
  doesn't check it [read].
- The ed285 marker is looked up in the room the hero is leaving (EntryMkr01 or EntryMkr02 of the source room)
  [read: `ed fn ed285` says 0x80155348/0x80151914 find the marker in the current room].

### 2.2 L04/132 in full: what decides room 5 vs 339

[read: `ed script 132 --level 4`]

```
state 0 (Region01 enter): prompt CLIMB DOWN (ed301(2,4))
    if fn78(5,733) or fn78(5,831) or ed166(player) ~= 150 -> room_queue(339), state 2
    else                                                 -> room_queue(5),   state 1
state 1 (the press, room 5 picked): hold(0); ed284(1); ed285(EntryMkr01, 1200, 0.5); stream_fade(0, 90);
    gTomeSndID = sound(33, 100); deny_menu(200); state 2; wait 120
state 2: the same test again
    339: fn66(339, EntryMkr01, Camera01, 15, 0, 0)            (climb down, motion 104, 62 ticks)
    5:   music_step(1); flag 679 := 0; send_event(fn78(3,32), 82, 0); fn66(5, EntryMkr01, Camera02, 3, 1161, 0)
    then state 0
```

- So: room 5 only for Karim's model, before he carries either Tome. After the first Tome (733) he carries it and the
  ladder is a plain climb to 339 [read].
- The test runs twice: at the region (state 0) and after the 120-tick swirl (state 2). If the Tome were taken between
  the two, state 1's swirl would play and state 2 would still go to 339 [inferred from the flow].
- In co-op the host already answers both parts for the two players: `hle_object_model` tells chapter scripts the
  chapter's own model (150 here) for player 1, and for player 2 in their turn [read: runtime/host/hle.c:11214-11236];
  `coop_script_item` makes `fn78(5, id)` answer yes when either list has the thing [read: hle.c:23885-23910]. So once
  either player carries the Tome, both players' ladders go to 339 [inferred].
- In the fresh Jump-to-Game run the ticket was `0x00000005 0x0CA41C95` (room 5, EntryMkr01 = 212081813) at frame 2942
  and the change at 3004, 62 frames later = transition 3 [read: logs/run-20261114-160001-fx-a0-before/EVENTS.log
  lines 11 and 13]. Player 2 stayed in 338 [read: same file lines 31-32].

### 2.3 The common steps every entry shares

[read: the seven scripts]
1. `hold(0)` (`fn21`): the hero stops [read].
2. `ed284(1)` and `ed285(...)`: the swirl [read].
3. `stream_fade(0, 90)`, `gTomeSndID = sound(33, 100)`: music down, the Tome's sound on (a script global) [read].
4. `deny_menu(200)` (`ed217`): no Tome menu for 200 ticks [read: `ed fn ed217`].
5. wait 120, then `flag 679 := 0`, `send_event(fn78(3, 32), 82, 0)`, `fn66(5, EntryMkr01, Camera02, 2|3, 1161, 0)` [read].
   - Event 82 to the kind-32 thing: docs/game/systems/sanity.md:51 says a running sanity effect "ended at the next
     door with the door's own event 82" [read there]. What the kind-32 thing is isn't traced (docs/game/research/verbs.md:30).

## 3. The dissolve: what code runs it

- `ed284` = 0x8000AEF4: `ed284(on)` reaches 0x80044200, which sets r13-25680 = on and, when on, r13-25684 = 2 and
  bytes at 0x8025BC30 +12/+16 to 124/132; off sets them to 128/128 and leaves r13-25684 for 0x80044254 to count
  down. 0x800081D8 runs the screen effect 0x801E9534 while 0x80044254 is nonzero [read: `ed fn ed284`].
- `ed285` = 0x8000AD70: stores the marker's place at 0x803012B8 (0x801E9208), the radius in r13-26880, the strength in
  r13-26876. 0x801E9130 gives the strength to 0x801E9534, scaled by (1 - player distance/radius)^2 [read: `ed fn ed285`].
- 0x801E9534 copies the picture out of the frame buffer (GXSetTexCopySrc, GXCopyTex) and draws it back warped
  [read: `ed fn ed285`; docs/coop-failed-attempts.md:640].
- No host code touches it: no match for 0x801E9534, 0x80044200, 0x80044254 or r13-25684 in runtime/host/hle.c [read: grep].
- In room 5, the room script 2675 calls `ed284(1)`, `ed285(EntryMkr01, 300, 0.4)` (not in chapter 13), and in its
  state 1, 30 ticks later, `ed284(0)` [read: `ed script 2675`]. So the swirl runs out of the old room and into room 5.
- The transition numbers used: 2 = 10-tick fade, no motion; 3 = 60-tick fade, no motion; 0 = a cut [read: `ed transition`].
- 2026-11-08: in co-op the swirl did not show, only the fade (docs/coop-failed-attempts.md:638-656, docs/coop-glitch-todo.md
  C8) [read there]. The computed task says a 2026-11-14 run showed it, also in the stayer's half: not checked here.

## 4. The way back: script 1599, then 868

### 4.1 How the Tome is taken in room 5

[read: `ed room 5`, `ed script 2675`, `ed script 852`, `ed script 1413`]
1. **Arrival**: 1161 (fn66's 5th argument) sets Camera01 and eases the eye 20 ticks [read: `ed script 1161`].
2. **Room 5's room script 2675** (the same in levels 00, 03-11, 13, 16): spawns the Tome as thing 733 at TomeSpawnMkr
   if it isn't there (`fn78(2,733)` nil): type 120 for model 150, type 234 in chapter 13, type 30 otherwise. Shows the
   chapter-page statues by chapter (`fn87`/`fn88` on things 2207-2212: `ed143() > 3, > 4, > 5, > 7, > 8`, chapter 10 off,
   `> 9`). The swirl as above, `deny_menu(180)`, and fades gTomeSndID 85, 70, 55, 40, then stops it [read].
   - Who starts 2675: no script names it [read: no "2675" in build/ed-db/scripts/L04/*.lua]. The room change's tail
     0x80152298 calls 0x80153B2C(2, 0) (lr 0x80152430); 0x80153BD8 reads the room record's `lha +136` (0x80153C08), and
     0x80153B2C passes it to 0x80163C6C(script, -1, 1) at 0x80153B68 [read: `ed dis 0x80153B2C 60`]. The log shows
     `[room] later 0x00000A73` (2675) from 0x80153B6C at frame 3007, then `script run r3 2675 ... lr 0x80163E5C`
     [read: logs/run-20261114-160001-fx-a0-before/page-0001.log lines 211-226]. That +136 is a field of the room's file
     is [inferred].
3. **Walking in**: CameraRgn01 runs 1351 (cameras; chapter 3's first-entry scene, flag 111) [read: `ed script 1351`].
   FaceRgn runs 852: once (flag 614), sound 639, event 18 to thing 679, texture animations, then after 120 ticks
   flag 679 := 1; if then in CameraRgn02 it runs 1413 [read: `ed script 852`].
4. **The take**: CameraRgn02 (x -1370..-1203) runs 1413: with flag 679 on, prompt `ed301(2, 6)`, the TOME words
   (room words 205,0) with Yes/No, then `set_machine(733, 30)` and `find(733)` (`ed52`) [read: `ed script 1413`].
   In chapter 13 instead: prompt 19, flash, `fn66(64, EntryMkr01, FightCam04, 2, 1828, 0)` [read].
5. The find gives the item, and its AFTER FIND script is 1599 (items 30 and 120) [read: `ed how tome`, `ed item 30`].

### 4.2 Script 1599 (the same in levels 00, 03-11, 13, 16)

First, for everyone: `send_event(fn78(3,32), 82, 0)`, flags 679 and 614 := 0, `stops_on_hold(player, 1)` [read].
Then by `ed166(player)` [read: `ed script 1599`; models from levels.json]:

| Model (hero) | Condition | Film | Return `fn66` | After |
|---|---|---|---|---|
| 85 Anthony | any room | `ed55(112)` | 150, EntryMkr_TomeReturn, Camera01, 0 | 868 |
| 150 Karim | room 5 | `ed55(113)` | 338, EntryMkr_TomeReturn, Camera01, 0 | 868 |
| 150 Karim | room 193 | none | none: flag 9 := 0 | |
| 18 Max | room 258, flag 944 off | `ed55(114)` after `hold_scene(1)`, `set_turns_into` on things 871/862/658/487, flag 321 on | 255, SpawnMkr72, HorrorCamera01, 0 (flag 944 := 1, `fn22(1,1)`) | 868, which runs 1599 again |
| 18 Max | room 255, flag 944 on | none: a summon scene (states 0-5) | 258, MaxSpawnMkr01, Camera01, 0 | 1599 again |
| 18 Max | room 258, flag 944 on | none: words 12,85 (the letter), spawns thing 490, `find(490)`, flag 490, `magick_give(-1)` | none | |
| 73 Lindsey | any room | `ed55(115)` | 303, EntryMkr_TomeReturn, Camera01, 0 | 868 |
| 74 Paul | any room | `ed55(116)` | 115, EntryMkr_TomeReturn, Camera01, 0 | 868 |
| 69 Edward | any room | `ed55(118)` (after event 82, `hold(0)`, flash) | none: fade, Camera01, flag 515, 2306 if in Region01, `magick_give(-1)` | |
| 75 Peter | any room | `ed55(119)`; then event 57 to things 844, 840, 841 | 17, EntryMkr_TomeReturn, Camera01, 0 | 868 |
| 81 Major | any room | `ed55(120)` | 43, EntryMkr_TomeReturn, Camera01, 0 | 868 |
| 76 Roberto | any room | `ed55(117)` | 204, EntryMkr_TomeReturn, **Camera04**, 0 | 868 |
| 142 | state 0 | none | 64, EntryMkr02, FightCam02, 2 | 1828 |
| 9 Alex | room 5, state 0 | none | 64, EntryMkr02, ArtifactCam02, 2 | 1828 |

- The film branches use `state_keep(1)` and state 1 is run when the film ends. `ed55` stores the running script in the
  cinematic block (+468), and the record's +14 is the script the film's end runs (0x80026FCC -> 0x801642D4); record
  112's +14 is 1599 [read: `ed fn ed55`]. That records 113-120 also name 1599 is [inferred]: their branches have the
  same shape.
- State 1 of each film branch: `state_keep(0)`, `fade_set(255,255,255,255)` (white), `stream_play(81, 100, 5, 3)`, then
  the return fn66 [read].
- Seen in a run: 1599 from the find (lr 0x800D9BA8) at frame 2541; at the film's end the game reloads the same room
  (`[room] change 0x00000096 0x00000044`, calls 0x80157590 0x800264C8) and runs 1599 again from lr 0x80026FE4 at 2649;
  that writes the ticket `0x00000096 0x37A365CD` (room 150, EntryMkr_TomeReturn = 933455309); the change is at 2650 and
  868 runs from 0x801527F8 [read: logs/run-20261113-181338-jtg-stayer-c3-153/page-0001.log lines 2475, 2572, 2580, 2595,
  2600, 2609]. That run was in room 150 with player models 0 and 70, so the model-85 branch was chosen by the host's
  chapter-model answer [inferred from hle.c:11225-11236 and the room chosen].

### 4.3 Script 868 (the same in levels 00-13, 16)

`fade_set(255,255,255,255)` then `fade(0,0,0,0,40)`: out of the white over 40 ticks. Then by model: 150 -> `run_next(1349)`,
18 -> 1599, 74 -> 531, 73 -> 1061, 81 -> 1161; 85, 75 and 76 nothing more [read: `ed script 868`].
- 1349 in room 338 (Karim): a scene (`fn22(1,1)`, ZombieCam01-03, flag 522, sounds, events 24 to things 174-178), then
  `set_quarry` of those five on the player, and `state_set(0, 132)` + `run_next(132)`: the ladder script again, so its
  state 0 offers CLIMB DOWN (to 339 now) where the hero stands. In room 193 it clears flag 9 [read: `ed script 1349 --level 4`].
- 531 and 1061 set Camera01; 1161 sets Camera01 and eases [read].

## 5. Every Tome acquisition in the game

| Chapter | Where the Tome is | Taken by | Trip | Back to |
|---|---|---|---|---|
| L00 Alex (mansion) | room 71, type 30 tag 333 | L00/1571 (TomeExamine01/02): films 122 and 1, `find`, flag 333 | none | |
| L01 Pius, L02 Ellia | none | 1599 has no branch for models 0 or 70 | | |
| L03 Anthony | room 5 (2675) | 1413 -> 1599 | 150 -> 5 by L03/892 | 150 EntryMkr_TomeReturn |
| L04 Karim | room 5 (2675), then thing 831 in room 193 | 1413 -> 1599; then L04/1381 (Tome used at ExamineRgn: taken, `ed9(1381)`, flag 9, a new Tome thing 831 at TomeMkr01) | 338 -> 5 by L04/132 | 338 EntryMkr_TomeReturn |
| L05 Max | room 258, type 30 tag 2041 | L05/2434 (TomeRegion, DeskCam) -> 1599 | 258 -> 255 -> 258 inside 1599 | 258 MaxSpawnMkr01 |
| L06 Lindsey | room 5 | 1413 -> 1599 | 303 -> 5 by L06/335 | 303 EntryMkr_TomeReturn |
| L07 Paul | room 5 | 1413 -> 1599 | 115 -> 5 by L07/1111 | 115 EntryMkr_TomeReturn |
| L08 Roberto | room 5 | 1413 -> 1599 | 204 -> 5 by L08/327 | 204 EntryMkr_TomeReturn, Camera04 |
| L09 Edward | room 236, type 30 tag 2066 | L09/2306 (Region01, Camera11) -> 1599 | none | |
| L10 Peter | room 5 | 1413 -> 1599 | 17 -> 5 by L10/1215 | 17 EntryMkr_TomeReturn |
| L11 Major | room 5 | 1413 -> 1599 | 43 -> 5 by L11/358 | 43 EntryMkr_TomeReturn |
| L13 Alex (final) | room 5 (type 234) | 1413's chapter-13 branch, or 1599's model 9 / 142 branches | to room 64 | 64 EntryMkr01/02 and 1828 |

[read: `ed how tome`, `ed item 30`, the scripts named, `ed door 5`, `ed door 258`, `ed door 64`]
- How chapter 13's Alex gets into room 5 isn't found: no literal `fn66(5, ...)` in L13, and `ed door --level 13`
  lists none [read]. Not traced (game code or a variable).
- Room 1 also has an EntryMkr_TomeReturn, but no script names hash 933455309 except 1599, and 1599 never names room 1
  [read: grep of build/ed-db/scripts].

## 6. Where the return spot is

`EntryMkr_TomeReturn` (hash 933455309, flags 16) in each return room, against the region of the way in [read: build/ed-db/rooms.json]:

| Room | EntryMkr_TomeReturn | The way-in region | Inside? |
|---|---|---|---|
| 338 | (-950, -194, 1), angle pi | Region01 box (-1131..-754, -302..132, -76..256), 132 | yes |
| 115 | (-249, -268, 0), pi/2 | Region01 box (-304..-125, -342..-193, 0..141), 1111 | yes |
| 204 | (256, -503, 6), pi | Region02 box (151..363, -721..-441), 327 | yes |
| 17 | (-227, -266, 7), pi/2 | Region01 sphere at (-263, -268, 0), radius 109, 1215 | yes (about 36 away) |
| 43 | (-134, 1200, -78), pi/2 | Region02 box (-222..53, 1130..1280), 358 | yes |
| 303 | (-6, -1425, 7), pi | BoxRegion02 box (-235..248, -1637..-1558), 335 | no, about 133 past its y edge |
| 150 | (218, 865, 993), pi/2 | SKPlane01, a plane (892 on cross_a) | not computed |

So the game puts the hero back at the door they came in by, by a marker, not by a saved place [read for the table;
"by the door" is inferred from the overlap]. Arriving inside the way-in region: docs/game/systems/doors.md:77 says the
game doesn't keep an arriving player out of the region they arrive in [read there]; for Karim, 1349 re-runs 132 itself.

## 7. The game's own "go and come back to where you were" (not used by the Tome)

1. **L06/1838 (Lindsey, to room 313 and back)** [read: `ed script 1838 --level 6`]:
   `VampReturnRoom = fn25()`; `fn104(0, player)`; `fn66(313, EntryMkr01, Camera06, 0, 1838, 0, 0)`; ... at the end
   `fn66(VampReturnRoom, EntryMkr01, Camera01, 0, 2252, 1)`. The 6th argument's bit 1 arrives at saved place slot 0
   (0x8016490C(0), table 0x805CC2BC, 6 bytes, s16 x3) instead of the marker; it leaves the heading unset
   [read: docs/game/research/transitions.md "6th, flags"; `ed fn fn104`]. 2252 sets Camera01, then `ed181(player)`.
   - `fn104` slot 0 is written by all 152 `fn104` calls (all slot 0), for example 1599's Max branch places the letter
     with `fn104(0, player)` [read: transitions.md; 1599 L332].
2. **L09/1183 (Edward's servant attack, and L16)** [read: `ed script 1183 --level 9`]:
   `ServantAttackRm = fn25()`; `ed164(player)` (place to the one buffer 0x80308E08); `fn134(1, 3)` (camera saved);
   `ed303(0)` (the 68-byte look at 0x80307108 and the script table rows saved); `fn66(room, ServantMkr, ServantCam, 3, 1183, 2, 0)`;
   back with `fn66(ServantAttackRm, ServantMkr, ServantCam, 3, 1183, 0, 0)`; then `ed165(player)` (place back, +2.0 on z
   each call), `fn134(2, 3)`, `ed303(1)` [read: `ed fn ed164`, `ed165`, `fn134`, `ed303`].
3. **L13/1828 and L07/1828 (room 64)**: `fn127() == 6` -> `fn134(2, 5)` + `ed165(player)`; else `fn134(1, 5)` +
   `ed164(player)` + `ed55(134)` [read: `ed script 1828`].

All three hold ONE place and one camera: fn104 slot 0, the ed164 buffer, fn134's single owner key [read: the `ed fn`
entries]. Two players need two of each, kept by the host [inferred].

## 8. What the Tome changes in the world

- **Carrying thing 733** (or 831) is what changes the doors: every way-in script tests `fn78(5, 733)` [read: section 2].
  The host answers it from either player's list [read: hle.c:23885-23910].
- **Casting** asks 0x80109A68 ("cannot cast spells without the Tome"); the host answers yes when either player carries
  it (`coop_ask_has_tome`, HAS_TOME) [read: hle.c:23834-23841].
- **Flags**: 679 (the hand is open, the Tome can be taken: 852 sets it, the doors and 1599 clear it), 614 (852 once),
  111 (Anthony's first entry, 1351), 9 (Karim, room 193), 321, 944, 490 (Max), 515 (Edward), 333 (Alex) [read].
- **Magick**: the record 0x80334D50 is one for the game [read: `ed fn ed10`]. 1599 calls `ed10(-1)` (check only, gives
  nothing) in Max's and Edward's branches only [read].
- **Per chapter, the film's end**: Karim's 1349 zombie ambush in 338 (flag 522, five things set to hunt the player);
  Peter's event 57 to things 844, 840, 841 before the return [read].
- Room 5's statues by chapter (2675) [read].

## 9. What is whole-game and what is per chapter

- **The same in every level that has it**: 1351, 1161, 868, 852, 1413, 2637, 2675, 1599 (each "1 different version")
  [read: `ed script`]. 1599 and 868 branch by model inside.
- **Per chapter**: the way-in script, its gate (model, Tome carried, flags, `ed61`), the swirl's marker, radius and
  strength, transition 2 or 3, the return room (by model in 1599), the film number, the follow-up after 868, and the
  return marker's place in each room [read: sections 2, 4, 6].

## 10. For the design (inferred, not read)

- The way in has no "where they were" at all, and the way back goes to one fixed room by model. So "each back to their
  own room and spot" means keeping, at the moment of the way in, each player's room and place (the game's own
  `fn25` + `fn104`/`ed164` pieces, one per player), and after 1599's return fn66 putting the player who was not in the
  return room back in theirs.
- The finder of the way in is the one whose return the game already does, but only when they came in from the return
  room by that chapter's door. A player who came from elsewhere (player 2 following from room 339, say) would land in
  338 at EntryMkr_TomeReturn unless the host moves them.
- 1599's state 1 return runs at the film's end, after the same-room reload (flags 0x44) that the film's end does: with
  two room slots that reload is of the room in play (room 5).
- Max (258 -> 255 -> 258), Edward and Alex in the mansion have no Tome-room trip; the feature applies to the seven room-5
  chapters and chapter 13.
