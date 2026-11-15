# The Tome room and its scenes, as the game runs them (reader 2)

Read only. Nothing was changed, built or run. HEAD 3542bf0 (branch tools/game-kit) plus the uncommitted files in the
status. Marks: [read] seen at the address, script line, file:line or log line given; [read there] stated in a named
doc; [inferred] not seen, worked out from what was read.

The feature this is for (the user, 2026-11-14): players apart; either takes the way into the Tome room; both go
there as co-op did before separate rooms; either picks up the Tome; after the scenes, each goes back to their own
room and place from the moment of the transition. World flags and magick as the game sets them.

## Answer in short

- **The Tome room is room 5 in seven chapters, and the same scripts run it in all of them.** Its ways in are
  L03/892, L04/132, L06/335, L07/1111, L08/327, L10/1215, L11/358. They all arrive at EntryMkr01, then run 1161
  [read `ed door 5`]. Max (level 05: room 258, script 2434), Edward (level 09: room 236), Alex (level 00: room 71,
  script 1571) and Ellia (level 02: her start gives it, L02/1201) get their Tome somewhere else. Pius's level has no
  Tome [read `ed item 30`, `ed script 1201`; docs/research/separate-rooms/readers.md:398].
- **The pickup:** room 5's hand opens (852). Then 1413 asks EXAMINE and Yes/No, and calls `find(handle(2, 733))`,
  the game's own find (ed52 -> 0x80024528). The Tome's after-find script **1599** then plays the chapter's cinematic.
  At the cinematic's end, 1599 runs again: a white fade, then `fn66(<the chapter's room>, EntryMkr_TomeReturn,
  Camera01, transition 0, 868)` [read `ed script 1413`, `ed script 1599`].
- **The game has no saved place to go back to.** The way back is a fixed marker for each chapter. In Karim's room 338,
  EntryMkr_TomeReturn is at the ladder, the same place as EntryMkr01 [read rooms.json; `ed room 338 --level 4`].
- **"The world changes" means three things.** Nothing is set at the pickup itself.
  - (a) The Tome is in the list. Each chapter's way into room 5 tests `fn78(5, 733)`. Once the Tome is carried, the
    same door goes to the chapter's other room (338's ladder goes to 339).
  - (b) Flags 679 and 614 (the hand) are put back off, and event 82 goes to the kind-32 thing.
  - (c) One scene after the return, in Karim's chapter only: 868 -> 1349, which runs only `if room == 338`. It sets
    flag 522 and wakes zombies 174-178 against "the player".

  Peter's branch also sends event 57 to things 844, 840 and 841 before its fn66. Max's branch sets flags 321, 944 and
  490, changes 4 NPCs' alignment, and gives a letter [read 1599, 868, 1349, 132].
- **Magick is the Tome in the list. No grant runs at a room-5 pickup.** 0x80109A68 says yes when the caster's list
  holds item type 30 or 120. The spells are one global record (0x80334D50). Only Max's and Edward's branches call
  `magick_give(-1)`, which gives nothing and only checks [read 0x80109A68-0x80109B44; `ed fn ed10`].
- **Co-op today already covers both players for the Tome after either one picks it up.**
  - The find goes into player 1's list (hle_coop_found, hle.c:18783).
  - "Has the Tome" is answered from either list (coop_has_tome, hle.c:23855; the intercept at hle.c:31011).
  - The scripts' `fn78(5, id)` is answered from either list too (coop_script_item, hle.c:23895; the intercept at
    hle.c:31020).
  - Player 2's Tome is the copy (ED_COOP_TOME_COPY, default 2, hle.c:4021-4026), and it lists player 1's things.
  - So after a pickup by either player, both players can cast, and both players' doors take the "Tome carried" route
    [read].
- **The return ticket is player 1's even when player 2 took the Tome.** In the co-op runs, 1599 ran as player 1 ("who
  1") from the find's after-script call and from the cinematic's end. Its fn66 was logged as "a room change of player
  1's" [read logs/run-20261108-165807-p2-tome-moves page-0001.log:3079, 3245, 3259-3260]. With ED_COOP_STAY on, a
  change of player 1's leaves player 2 where they are (coop_follow's stay branch). So the game's own return, unchanged,
  would take player 1 alone back to 338's marker and leave player 2 in room 5 [inferred from hle.c:14321-14327 and
  22540-22548; not seen in a run].

## 1. The way in (each chapter's door into room 5)

Every one of the seven door scripts has the same shape [read `ed script 892 --level 3`, 132 --level 4,
335 --level 6, 1111 --level 7, 327 --level 8, 1215 --level 10, 358 --level 11]:

| Step | Karim's L04/132 | Who it reads |
|---|---|---|
| Prompt (CLIMB DOWN 2,4 for Karim; OPEN DOOR or none elsewhere) | `prompt(2, 4)` | the prompt's player |
| The Tome carried? | `fn78(5, 733)` or `fn78(5, 831)` (Karim's Tome placed on the pedestal later, L04/1381) | "the player's list" (fn78 kind 5 [read `ed fn fn78`]); co-op: either list (coop_script_item) [read hle.c:23887-23921] |
| The chapter's own hero? | `model_of(fn78(1, 0)) == 150` | fn78(1) = "the player", r13-21496 [read `ed fn fn78`]. hle_object_model gives the chapter's model for the script's player, player 2 in their turn too [read hle.c:11224-11235] |
| Preload | `room_queue(5)` or `room_queue(339)` | none |
| Hold, then dissolve | `hold(0)`, `screen_effect_on(1)`, `screen_effect_point(EntryMkr01, 1200, 0.5)`, `stream_fade(0, 90)`, `gTomeSndID = sound(33, 100)`, `deny_menu(200)`, wait 120 | hold is the player's (r13-26524) [read `ed fn fn21`]; the screen effect, deny_menu and the sound are single global words [read `ed fn ed284`, `ed fn ed285`, `ed fn ed217`] |
| Reset the hand, end a sanity effect | `music_step(1)`, `flag_set(679, 0)`, `send_event(fn78(3, 32), 82, 0)` | kind 32's event 82 ends a running sanity effect [read there: docs/game/systems/sanity.md:51] |
| Go | `fn66(5, EntryMkr01, Camera02, 3, 1161, 0)` | fn66 is the asker's change and clears the asker's hold bit [read there: hold-readers.md:24] |

- The dissolve is the full-screen copy effect 0x801E9534 (GXCopyTex), scaled by the player's distance to the marker
  [read `ed fn ed285`]. It is one screen effect, so it also showed in the staying player's half (docs/coop-todo.md:157-159)
  [read there].
- The carried test (`fn78(5, 733)`) is what makes the door one-way. Once the Tome is in either list, the door takes
  its other branch: room 339 for Karim, 143 for Lindsey, 103 for Paul, 210 for Roberto, 1 or 346 for Peter, 54 for the
  Major, 147 for Anthony [read the go_to_room lines of each script above; L03/2517 is the same test for Anthony's other
  room_queue].
- In a stay run, player 1's ladder from shared room 338 took player 1 alone to room 5. Player 2 stayed in 338 (slot
  1) [read logs/run-20261114-163059-fx-a-0 page-0001.log:108-110, 285, 295].

## 2. Inside room 5

Room 5's regions, cameras and markers: `ed room 5 --level 4` [read].

| Script | When | What it does | Reads "the player" |
|---|---|---|---|
| 1161 | the ticket's after-script, at arrival (lr 0x801527F8) | `camera_lead(0)`, Camera01, eye ease 20 | the camera only [read `ed script 1161`; log fx-a-0:222] |
| 2675 | from arrival, through the timer path (lr 0x80163E5C, ARRIVAL_TIMER_RAN hle.c:19004); runs every 15-30 ticks through 6 states | `hold(1)`; spawns the Tome at TomeSpawnMkr with tag 733 if `fn78(2, 733)` is nil: type 120 for model 150 (Karim), 234 in chapter 13, else 30. Shows the busts of earlier chapters by `ed143()`. `screen_effect_on(1)` and `screen_effect_point(EntryMkr01, 300, 0.4)` (the dissolve in), then off after 30 ticks. `deny_menu(180)`. Fades gTomeSndID out | `model_of(fn78(1,0))` for the Tome's type; `fn78(2, 733)` is "in this room" (object +592 = r13-23276) [read `ed script 2675`; `ed fn fn78`; log fx-a-0:226, 344-553] |
| 1351 | CameraRgn01 enter, CameraPlane01 cross, CameraRgn02 leave | Cameras by zone. In chapter 3 with flag 111 off: the first-entry scene (`cutscene(1,1)`, IntroCam01/02, look_at, face_morph on `fn78(1,0)`), once | `fn78(1,0)`; `in_zone` is the player's zones [read `ed script 1351`]. Co-op runs it as the one arriving and gives player 1 their own camera (docs/coop-next-session.md:207; hle.c:19434-19486) [read] |
| 852 | FaceRgn enter (the long box over the far half) | If flags 679 and 614 are off: sets 614, sound 639, event 18 to the hand (tag 679: chr 122 model 236 at [-1450, 39, 338]), wait 120. Then flag 679 on, and if `in_zone(CameraRgn02)` run 1413 | `in_zone` [read `ed script 852`; `ed tag 679`] |
| 2637 | FaceRgn01-06 enter/leave | The wall faces' texture animation by zone | `in_zone` [read `ed script 2637`] |
| 1190-1197 | SphereRegion01-08 (the statues) | Chapters > 4: EXAMINE prompt, the statue's close camera, `player_on(0)` while it shows (flags 206 for Karim's) | the player [read `ed script 1193`] |
| **1413** | CameraRgn02 enter (the hand, x -1370..-1203) | Camera03. With flag 679 on: `prompt(2, 6)` EXAMINE, then `room_words(205, 0)` TOME ("...Should X take it?"), then on Yes: `iTome = fn78(2, 733); set_machine(iTome, 30); find(iTome)`. No: Camera02, `cutscene(0,1)` | the prompt's player [read `ed script 1413`] |

**The pickup is the game's own find.** `ed52(thing)` is 0x80012A58. It reads the thing's words and calls 0x80024528
(ACQUIRE) with the controlled id [read `ed fn ed52`]. The words are TOME_PICKUP 2,2 "<player> has acquired the Tome
of Eternal Darkness!" [read `ed how tome`].

- Co-op: when player 2 is the finder, hle_coop_found puts the Tome into player 1's list (gpr5 rewritten,
  hle.c:18806-18809). It also copies the script's word into the other player's table, so the find is done for both
  [read hle.c:18783-18830; log p2-tome-moves:3039 "script 1413 found something: done for player 1 too"].
- When the find window closes, event 190 runs the thing's after-script 1599 through 0x801642D4 (FOUND_AFTER_RAN
  0x800D9BA8, hle.c:18998) [read].

In co-op both players were in room 5 for this, by follow (2026-11-08). Player 2 took the Tome: 852 and 1413 ran for
player 2 and the close-up was theirs. Then 1599 ran as player 1 [read logs/run-20261108-165807-p2-tome-moves
EVENTS.log 0001:02919-03039, page-0001.log:3079].

## 3. The scenes and the return (script 1599, then 868)

1599 is one script, the same in levels 00, 03-11, 13 and 16. It branches on `model_of(fn78(1, 0))` and then on
`room()` (r13-23276, the room in play) [read `ed script 1599`]. It starts with `send_event(fn78(3,32), 82, 0)`,
`flag_set(679, 0)`, `flag_set(614, 0)`, `stops_on_hold(fn78(1,0), 1)`.

| Hero (model) | Room test | Cinematic | Then (state 1, at the cinematic's end) |
|---|---|---|---|
| Anthony (85) | none | 112 | white, `fn66(150, EntryMkr_TomeReturn, Camera01, 0, 868)` |
| Karim (150) | room 5 (room 193: flag 9 off only) | 113 | white, `fn66(338, EntryMkr_TomeReturn, Camera01, 0, 868)` |
| Lindsey (73) | none | 115 | `fn66(303, ...)` |
| Paul (74) | none | 116 | `fn66(115, ...)` |
| Roberto (76) | none | 117 | `fn66(204, EntryMkr_TomeReturn, Camera04, 0, 868)` |
| Peter (75) | none | 119 | event 57 to things 844, 840, 841; `fn66(17, ...)` |
| Major (81) | none | 120 | `fn66(43, ...)` |
| Max (18) | room 258 / 255 | 114 | flag 944, `fn66(255, SpawnMkr72, HorrorCamera01, 0, 868)`; 255's summon scene; back to 258 with 1599; the letter (flag 490, find with 1599), `magick_give(-1)` |
| Edward (69) | none | 118 | no room change: flag 515, Camera01, `magick_give(-1)`, Region01 -> 2306 |
| 142, 9 (chapter 13 / Alex) | room 5 for 9 | none | `fn66(64, EntryMkr02, ..., 2, 1828)` |

- **play_cinematic (ed55, 0x8001265C):** plays record n of the 173-entry, 32-byte table at 0x80242150 through
  0x8004F374, which asks for game mode 6. Record +14 is the script run at the end [read `ed fn ed55`]. Records
  112-120 all have +14 = 1599 [read generated/main.dol at 0x80242150 + n*32].
- Their halfword at +28 is 150, 338, -1, 303, 115, 204, -1, 1, 43 for 112-120. That matches each branch's return room,
  except 119's 1 against Peter's 17. What reads +28 wasn't found [read the bytes; the meaning is inferred].
- At the end, the cinematic's script (lr 0x80026FE4, CINEMATIC_END_RAN_B hle.c:19232) runs 1599 state 1. Its fn66
  writes the ticket from inside mode 6's frame. The logged call chain is 0x801668EC ... 0x80026FE4 0x80050AC0 [read
  p2-tome-moves page-0001.log:3245, 3260].
- The cinematic is its own game mode: co-op's split goes off for it ("[split] off ... (a cutscene)": camera.c:1631-1635,
  `!g_play`). "[coop] a cutscene: the screen and the pad are player 1's again" (hle.c:30115) [read; EVENTS
  0001:03105-03107]. The cinematic is full screen for both players. While it plays, no world frame runs for either
  player [inferred from mode 6 replacing the world's mode; not traced].
- 868 runs as the ticket's after-script at arrival (lr 0x801527F8) [read log :3297]:
  - `fade_set(white)`, `fade(0,0,0,0,40)`;
  - then by `model_of(fn78(1,0))`: 150 -> 1349, 18 -> 1599, 74 -> 531, 73 -> 1061, 81 -> 1161. 531 and 1061 are only
    cameras.

  [read `ed script 868`, `531 --level 7`, `1061 --level 6`]
- **1349 (Karim's return scene), only `if room == 338`** [read `ed script 1349 --level 4`]:
  - `cutscene(1,1)`, ZombieCam01-03, `flag_set(522, 1)`, sounds at SpawnMkr55/61/97/145;
  - event 24 to zombies 174-178, `set_quarry(zombie, fn78(1,0))`;
  - `cutscene(0,1)`, then `state_set(0, 132); run_next(132)`, so the ladder's prompt comes back at once. With the
    Tome carried, it goes to 339.

  Co-op's enemy code then retargets a zombie to the nearer player (hle.c:25432-25438 comment) [read].

Co-op in follow mode, 2026-11-08 [read EVENTS.log 0001:03259-03320]: "a room change of player 1's: the other goes on
through it", ticket 0x96 (150) at 0x37A365CD (EntryMkr_TomeReturn), and "player 2 brought to player 1's room
0x00000096, at 193 975 987".

## 4. Magick and the Tome menu

- **Casting** asks 0x80109A68 [read dis 0x80109A68-0x80109B50]:
  - Yes if the player's node flag bit 0x4000 (0x801F68E0) is set.
  - No if the chapter word 0x80307178 is 2.
  - In chapter 0, no unless flag 307 is set.
  - Otherwise yes if the player's carried list (0x801F63A0, then 0x801F9938) holds a thing of type 120 or 30. Type 234,
    chapter 13's Tome, is not one of them.
  - When it says no, the words are 0,13 "cannot cast spells without the Tome" [read `ed how tome`].
- **Spells and runes** are one global record at 0x80334D50 (word 0: runes, word 1: circles), written by
  `magick_give(bits)` from the runes' own after-find scripts (1360-1373, 1375, 1376, L03/2129). `magick_give(-1)`
  gives nothing: it only runs the auto-spell check 0x80109DC8, which can ask for the Tome to open (r13-26256)
  [read `ed fn ed10`]. Nothing in room 5's pickup changes the record [read 1599's room-5 branches].
- **The Tome menu** opens by 0x80006CDC(1, 0). Its gates are no fade (0x801E7C98), r13-26232 == 0 (deny_menu), and a
  live control channel. No Tome-carried test is listed [read `ed verb tome`]. 2675's `deny_menu(180)` and the door's
  `deny_menu(200)` keep it shut through the dissolve; r13-26232 is one word for the game [read `ed fn ed217`].
  Whether player 2's copy Tome honours it wasn't read.
- First-visit help: scripts 2020 and 887, flags 989 and 990 [read there: docs/game/systems/tome.md]. Co-op sets the
  meter-help flags as done (meter_help_off, hle.c:6330) [read there: `ed host tome`].

## 5. Today's co-op: the Tome for both

| What | Where | Covers both players after one pickup? |
|---|---|---|
| The find goes into player 1's list, and is done for both | hle_coop_found, hle.c:18783-18830 | yes [read] |
| "Has the Tome" for casting: either list | coop_has_tome, hle.c:23855-23885; intercept at hle.c:31007-31014 | yes [read] |
| A script's "is X carried" (fn78(5, id)): either list | coop_script_item, hle.c:23887-23921; intercept at hle.c:31020 | yes: both players' doors take the Tome-carried branch [read] |
| A chapter-model test made by player 2 in their turn | hle_object_model, hle.c:11224-11235 | yes [read] |
| Player 2's Tome | the copy, ED_COOP_TOME_COPY default 2 (hle.c:4021-4026); lists player 1's things (comment at hle.c:18805-18807) | yes; quick spells are each player's own (magick_trace, hle.c:12454) [read there: `ed host tome`] |
| Copy Tome while apart | tome_copy_fade and stayer windows (hle.c:15593-15643) | opens in the stayer's half [read there: f3-fade-tome-copy-readers.md] |

So nothing extra is needed for magick or for the carried tests after either player picks up the Tome [read the table
rows]. What is not covered is who the scenes and the return belong to (section 6).

## 6. What reads player 1, or the room in play, only

- **1599 runs as player 1 even when player 2 found the Tome** [read p2-tome-moves:3079, 3245]. Its branches read:
  - `fn78(1,0)` (r13-21496) for the model, `stops_on_hold`, and Max's `alignment_of` and `place_of`;
  - `room()` (r13-23276, the room in play) for Karim's room-5 test and Max's 258 and 255.

  Apart, the room in play is player 1's except inside player 2's turn (coop_stay_room_in) [read hle.c:14321-14327]. So
  if player 2 took the Tome in room 5 while player 1 was in another room, Karim's branch would see player 1's room, not
  5. Then no cinematic and no return [inferred; not run]. The feature avoids this only if both players are in room 5
  at the pickup.
- **fn66 in 1599 is the asker's change, and the asker is player 1** ("a room change of player 1's"). Under
  ED_COOP_STAY, coop_follow's stay branch keeps the other player in their room. So the game's return, unchanged, would
  leave player 2 in room 5 [inferred from hle.c:22540-22548 and 14321; not seen in a stay run].
- **The return place is fixed.** Every branch uses EntryMkr_TomeReturn (Max: SpawnMkr72, then MaxSpawnMkr01). Karim's
  is at [-950, -194, 1] in room 338, inside the ladder's Region01 [read rooms.json; `ed room 338 --level 4`].
  - Putting each player back at their own place means that player's return is not the game's ticket. It needs a ticket
    to their room with a placing [inferred].
  - Karim's world-change scene 1349 runs from 868 only `if room == 338`, for the one 868 runs as. If the player taken
    back by the game's ticket is not taken to 338, flag 522 and the zombies' wake are skipped [read 1349 line 1;
    the consequence is inferred].
  - Peter's event 57s and Max's flags are sent before the fn66 inside 1599, so they happen whatever the return is
    [read 1599].
- **One-of-a-kind globals used by these scenes:** the screen effect (r13-25680/-25684, 0x803012B8), deny_menu
  (r13-26232), gTomeSndID, the fade (0x80602A2C), mode 6, the room word r13-23276, and the running-script word. Each
  is single for the game [read `ed fn ed284/ed285/ed217/fn209`; readers.md:339 for the room and script words].
- **The hand's flags 679 and 614 are world flags, not per player.** 852 opens the hand once, and only 1599 or the next
  door's `flag_set(679, 0)` closes it [read 852, 1599, 132].
- **The Tome's spawn depends on what is in this room.** 2675 spawns the Tome only when `fn78(2, 733)` finds no tag 733
  in the room in play [read 2675]. That test is "in this room" by r13-23276, so it needs room 5 to be the room in play
  when it runs [read `ed fn fn78`].

## 7. Still open (not read)

- What reads cinematic record +28, and whether mode 6 skips the world frame entirely.
- Whether player 2's copy Tome honours deny_menu (r13-26232).
- What runs 2675 the first time. It is in the timer path at arrival, on the same frame as 1161 (fx-a-0:222, 226), but
  its starter wasn't traced.
- 1599's state 0 sets `state_keep(1)`. How that word reaches the cinematic's end with player 2's table in wasn't traced.
  In the log it ran as player 1's from the game's table.
