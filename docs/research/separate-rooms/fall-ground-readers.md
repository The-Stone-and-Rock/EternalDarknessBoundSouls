# How the game grounds a character each frame, and which room's floor player 2 stands on apart (reader)

Reader for the user's report of 2026-11-12: with separate rooms on, in Pius's chapter, player 1 went from the statue room
(182) into room 181 and player 2, still in 182 and about to follow, fell through the floor and could not move after. The
user's log was lost. I read only: the disassembly (`build/ed-db/disasm.txt`, `ed dis`), `runtime/host/hle.c` and
`runtime/host/camera.c` as they are now (uncommitted work included; another session is editing hle.c, so line numbers can
shift by a few: the function is named each time), the room files unpacked from the disc (to the scratchpad), and existing
recordings. Nothing was built or launched. [read] = seen at the address, line or log given. [inferred] = reasoned, not seen.

PAL build: `generated/game_id.txt` is GEDP01 [read]. Every address below is PAL. The task's "0x8012AB2C non-PAL" is the
same function as 0x80123F70: the decomp name of 0x80123F70 is `fn_8012AB2C` (the USA address), and find_player
(hle.c ~6576-6593) picks 0x80123F70 for GEDP01 and 0x8012AB2C otherwise [read].

## 1. The answer in short

- **One function decides which room a character collides with: 0x80123F70(object).** Every move that collides (a
  character's own step, root motion, a direct move by event 157, the body push, gravity) goes through the step
  0x80133434, which asks 0x80123F70 for the room record and tests only that record's geometry [read, section 2]. The task's
  premise that 0x801E1B74(record+0x813C) at 0x80151FDC sets collision pointers is wrong. It stores r13-22132, and only the
  renderer reads that word [read: 0x801E1B84-0x801E1B98; readers 0x800ED160, 0x800EDDE4, 0x801DFFAC, 0x801E2C40/58/80,
  0x801E3A94]. A room change leaves no collision pointer behind to go stale.
- **0x80123F70 works two ways.** For "the player" (the object equal to r13-26524), it returns the CURRENT slot's record
  (table 0x8058AFC0 +0), whatever the object's +592 says. For every other object, it returns the slot whose record's room
  (+0x8140) equals the object's +592, if that record is marked loaded (+0x8142 and +0x8143). With no such slot it returns 0
  [read 0x80123F8C-0x80124010].
- **No floor means falling for ever, accelerating.** The step's gravity drop moves the body by the fall speed (+40) along
  the gravity direction (+24). It adds +36 to the fall speed every frame no floor is hit (cap 1e6), and resets it on a
  floor. There is no fall state, kill plane or respawn in the step [read, section 2.4]. Measured for player 2 in
  logs/run-20261112-160254-cam-lf-load-p2door: dz -2, -3, ... -10 a frame, then a slide, and again, without end. One frame
  against the wrong room drops a grounded body only about 1 unit, which the next frame's own floor catches. A fall needs
  either several frames against a room with no floor there, or a push from the wrong room's surfaces or bodies that puts the
  body's ball under its own floor. From below, nothing catches it again [inferred: the swept test from behind a face; seen
  in 160254].
- **A step with no record (0) freezes the body.** It doesn't fall: no move and no gravity (0x80133488-0x8013349C stores
  the step's FROM point). stay-readers.md called this "the wanted position stored". It is the start position, so the move
  is dropped [read].
- **Apart, player 2's own step is on their own floor in every kind of frame I could trace** (section 3): the behaviour pass
  (non-player path by +592), their turn (player path with coop_stay_room_in), the R6c creature brackets, the change frame
  (record put back before the behaviour pass), scenes, their own close-ups, and the Tome copy. The same holds the other way
  round, with player 1 as the stayer.
- **What is NOT player 2's own while apart, even in a step against the right record:**
  - (a) 0x8013085C's body test collides player 2 with the bodies of the ROOM IN PLAY (r13-23276), at player 2's
    coordinates [read 0x801308F4-0x80130900]. Player 1 and room 181's three creatures, at (-760,557), (-911,1580) and
    (-1308,935) in 181's coordinates, are invisible solid bodies on 182's floor. Room 182's own bodies in that test are not
    there for player 2.
  - (b) The room-50 floor-slope constant is chosen by r13-23276 [read 0x80133CC0].
  - (c) Pieces flagged "only the player" never stop player 2 outside their turn (together too) [read 0x801309E4]. Room 182
    has none [read: the room file].
- **The ways player 2 could be stepped against player 1's room** are host windows that make player 2 "the player"
  (r13-26524 := their object) without putting their room in (section 4). The one that lasts across world frames is
  coop_tome_player(true): a quick-spell Tome, player 2's Start in the old route, the shared Tome owned by player 2, or a
  close-up kept as theirs (owner 1). In those frames the game's own pass steps player 2 by the player path, so against the
  current slot, which is player 1's room. Whether any of these runs in mode 2 (the default) apart is not traced.
- **The ways the stayer's record can be dead or replaced** (body frozen, their room not drawn in their half: possibly what
  "fell through the floor and could not move" looked like) are in section 5. The tail's put-back has two gates. I checked
  the per-frame neighbour prefetch 0x80155038 and found it does nothing: no room file has entries for it.
- **What the scripted attempts did not do** (section 6): player 1 never walked about in 181 (they stood at its entry), and
  player 2 never stood or walked where 181's bodies lie in 182's coordinates. The first-visit scene (2033) did run in
  logs/run-20261112-033738-p2-follows-181, and nothing went wrong in it.

## 2. The game's grounding of a character, each frame

### 2.1 Who moves a body, and through what

- **The behaviour pass.** The frame (0x800083C8) runs the ticket tick 0x80155830 (0x800083EC), the behaviour pass
  0x8014FDB8 (0x800083FC), things 0x80008400, the camera 0x80008424, scripts 0x8000849C, the second pass 0x800084AC, the
  draw, then 0x80155038(r13-26524) at 0x80008544 [read: ed dis 0x80008530; readers.md "actor passes"]. A character node's
  step 0x801FA44C (by table: no bl sites):
  - `pos = obj+0`; `0x80133434(obj, pos, pos, out, 3)`, an in-place step: push-out plus gravity; `0x80118248(obj, out)`;
    then the animation advance 0x80123698 or 0x8011DFD0 [read 0x801FA460-0x801FA4CC].
- **Root motion.** The animation advance reaches 0x80124030 (from 0x801237C0 and 0x80123DBC), which calls 0x801240B8(obj,
  to, flags 3, or 2 when the animation's +44 bit 0x4 is set) [read 0x80124050-0x80124098].
  - For kind 3, or a few model ids, it sets the place directly and calls 0x8013364C.
  - Otherwise it calls `0x80133434(obj, pos+offset, to+offset, out, flags)`, then `obj+0 = out - offset` (offset = the
    vector at +612, 0x801188D0) [read 0x801240E0-0x801241C4].
- **Direct moves with collision.** 0x801240B8 has 13 callers: 0x8005A72C in the player's state dispatcher (event 157:
  `0x801240B8(obj, data, 3)`), 0x8006B8BC/0x8006B90C, 0x8009D3B8/0x8009D4A0/0x8009E520/0x8009E5B8/0x8009E8C0/0x8009E95C,
  0x800A0964, 0x800A3540, 0x800C4694 (table-reached: script verbs and behaviours) and the root motion 0x80124098 [read].
- **The body push.** 0x80045658(obj) pushes every other node out of obj, with `0x80133434(other, ..., 1)`. It only pushes
  nodes whose object's +592 equals r13-23276 [read 0x80045708-0x80045718]. It has 24 callers, among them enemy behaviours
  [read: grep].
- **Direct places without collision.** 0x80118248 (`stw` to +0) has 93 callers [read].

### 2.2 The step 0x80133434(obj, from, to, out, flags)

- `info = 0x80123F70(obj)` [0x80133468].
- `info == 0`: `out = from`; return. No move, no gravity, no zone scan [read 0x80133488-0x8013349C]. r25 is the second
  argument: the FROM.
- Otherwise:
  - segment = 0x801388CC(radius +628, from, to);
  - the sweep `0x80133A44(obj, info, seg, 0, hit, flags)` [0x801334D0];
  - the gravity drop `0x8013332C(obj, seg_end, flags)` [0x801334E4];
  - `out = seg_end`.
- Then the zone scan 0x80134A44(obj, info+180, info+176, ...). It runs if (kind <= 2 and node state != 29) or obj ==
  r13-26524 [0x80133588-0x801335C4]. If the scan moved the object, the segment is rebuilt [0x801335C8-0x8013361C].
- For the player only: 0x8014EA24(seg, 0) [0x80133620-0x80133634].

### 2.3 The sweep 0x80133A44 and what each of its tests is keyed on

The sweep runs up to 8 rounds (0x801341D0-0x801341E0). Each round runs three tests and takes the nearest hit (0x80133BD4-0x80133C3C).

| test | runs when | collides with | keyed on |
|---|---|---|---|
| [0] world: 0x8013085C(obj, seg, info, out, flags) [0x80133B50] | not +596 bit 0x2 and r13-29304 != 0 [0x80133AFC-0x80133B10] | 0x80130684: the tree at info+32 (0x80138E8C) and the pieces at info+160 (0x80130A84: count u16 at record +220, 8-byte entries at +224) [0x801306B8-0x801306E4]; the dynamic colliders 0x8013CEEC when flags bit 0x1 [0x801306E8-0x80130704]; then, when flags bit 0x2, BODIES [0x801308C8-0x801309B0] | the record from 0x80123F70; pieces by story flag 0x801DC22C(r13-26528, entry+0) and entry+5 bit 0x2 "not the player" / bit 0x1 "only the player" against r13-26524 [0x80130A08-0x80130A44]; dynamic colliders: 12 entries of 20 bytes at 0x805852F0, global, no room test, bit 0x2 entries only for the player node's object or 0x8004D8A0's [0x8013CFA8-0x8013D0A4]; **bodies: nodes whose room 0x801F662C == r13-23276** (the room in play), object +596 bit 0x80, node 0x801F6450 bit 0x4, test 0x80130504 [0x801308E0-0x80130950] |
| [1] characters: 0x801338D4 [0x80133BAC] | r13-29300 != 0 and +596 bit 0x40 [0x80133B58-0x80133B70] | other characters (+596 bit 0x80, node flag 0x4 CLEAR) | **the stepping object's own +592** [0x80133914, 0x80133944-0x8013394C] |
| [2] 0x80131500 [0x80133BC0] | same gate | spheres in a global table at 0x80581E00 (+0 bit 1 on, +8 owner, +12 centre, +24 radius) | no room [0x80131518-0x80131564] |

- The three gate words r13-29296 (gravity), r13-29300 (characters) and r13-29304 (world) are 1 in the dol (0x8061E1B0,
  0x8061E1AC, 0x8061E1A8). Nothing stores to them by r13 [read: no store in the disassembly; dol bytes].
- A body hit from the room-in-play test merges into test [0]'s result (0x80130968-0x801309B0 keeps the nearest). For the
  stepping character it is world geometry: a wall, a floor (stood on) or a ceiling [read].

### 2.4 Gravity, the floor rule, falling

- **The drop 0x8013332C(obj, pos, flags).** One caller: 0x801334E4 [read].
  - It returns early if f = +40 (0x80118A64) == 0, if +596 bit 0x800 (no gravity) is set, or if r13-29296 == 0 [0x80133350-0x8013337C].
  - Otherwise `to = pos + f * (+24)` (0x80118A5C is `addi r3,r3,24`). It sweeps `0x80133A44(obj, 0x80123F70(obj), seg,
    1, ...)` and writes `pos = seg end` [0x80133380-0x80133414].
- **The floor rule, in the drop (r6 = 1), on a world hit** [0x80133C6C-0x80133D50]:
  - d = dot(gravity direction, hit normal).
  - d <= -0.69 (r2-23088; -0.62, r2-23084, when **r13-23276 == 50**, 0x80133CC0-0x80133CD0) is a floor: the body stands
    at the hit.
  - If d > -0.99939 and the object is the player, +596 bit 0x10 is cleared (0x80133D14-0x80133D2C).
  - Then: d > -0.56 (r2-23076) gives `0x80118A78`, the fall speed grows; otherwise `0x80118A6C`, +40 := +36 (reset).
  - Constants read from the dol at r2 = 0x80628460 (`lis r2,-32670; ori 0x8460` at 0x80005424).
- **No hit at all in the drop:** 0x801341A8-0x801341C0 calls 0x80118A78 [read].
- **0x80118A78:** `if (+40 < 1e6) +40 += +36` (r2-23680 = 1,000,000) [read 0x80118A78-0x80118A94]. It has two callers,
  both in the sweep. +40 is read only by 0x80118A64, whose one caller is 0x80133350 [read: grep].
  - So no "falling" state is fed from the fall speed, and no z limit or respawn is tested anywhere in 0x80133434,
    0x8013332C or 0x80133A44 [read].
  - The only "fell out" rescue is the host's: coop_follow's COOP_FALLEN (2000 below where they last stood still,
    hle.c ~21271/21355). It runs only when both players' +592 match. Apart, `stood_z = NAN` (coop_follow, the
    coop_stay_apart branch) [read]. **Apart, a fall is for ever.**
- **The fall, measured** (logs/run-20261112-160254-cam-lf-load-p2door/both.tsv, player 2, frames 2401-2445; the put at
  frame 2400 was by room 343's floor while they were in 341):
  - z -49, -51, -54, -58, -62.6, -68.6, -75.6, then dz -0.7, then again -1.4, -2.3 ... -10.0, and so on: z -250 by frame
    2445, still falling.
  - The fall speed grows by 1.0 a frame (+36 = 1.0 for that character [inferred from dz]) and is reset each time the
    body slides on a steep face, which d > -0.56 doesn't count as floor [read: rows; inferred: why it resets].
- **One wrong frame.** A grounded body's +40 is +36 (reset every grounded frame), so the drop is about 1 unit and the next
  frame's in-place step pushes the ball back up [inferred]. What can lose the floor in ONE frame:
  - a push-out from the wrong geometry or a wrong body that moves the ball down by up to its radius;
  - a place written without collision (0x80118248, the host's writes: ED_PUT's old way).
  - Several frames against a room with no floor there add up: 1+2+...+10 is 55 units in 10 frames [inferred].

### 2.5 The getter 0x80123F70(object) [read 0x80123F70-0x80124020, 0x801551A0, 0x80155130]

- `r31 = 0x801551A0(2)`: the current slot's record (table +0) if its +0x8142 and +0x8143 are non-zero, else 0.
- If object == r13-26524: return r31, the **player path**.
- Otherwise, for section 2 (current) then 3 (other; 0 when table +4 == -1): the record with +0x8142 and +0x8143 set and
  `lha +0x8140 == lwz object+592` → return 0x801551A0(that section). None → 0. This is the **non-player path**.
- So a non-player's step lands in its own room's slot whether or not that slot is current. coop_room_in's swap of +0/+4
  doesn't change it. The player's lands in the current slot, whatever room they are really in.

### 2.6 Rooms 182 and 181 from the disc [read: unpacked to the scratchpad, header words]

- **The pieces' count, u16 at record +220:** 182: 0; 181: 1; 183: 1; 180: 0. Room 182's floor is all tree (+32), with no
  flag-gated or player-only pieces.
- **The per-frame neighbour list's count, u16 at +192, read by 0x80155314:** 0 in all 297 rooms (section 5).
- **182 (`ed room 182`):** Region02 (the walk-in door to 181) is a sphere at (-762,2414,80), r 111. EntryMkr02 is at
  (-769,2229,81): the floor by that door is about 78 high (player 2 stood at z 78.4 there in 033738). The statue's examine
  Region03 is at (-790,1234,0). The level's one creature placed in 182 is model 5, chr 12, at (-767,1212,111): the statue's
  place [read: coopplan.enemies(1,182); inferred: that it is the statue].
- **181:** EntryMkr01 is at (-2147,1061,3), inside CameraRgn12, whose script 2033 is the first-visit scene. Three
  creatures (model 8, chr 24) are at (-760,557,0), (-911,1580,0) and (-1308,935,0). The floor grid of SpawnMkrs runs over
  x -1718..201, y 345..1785, z 1 or 27. These coordinates lie inside 182's floor area (182's zones span x -1762..290,
  y -572..2185) [read].

## 3. Player 2's step, kind of frame by kind of frame (separate rooms, co-op, apart)

The table names the room record the step uses and the bodies it collides with. "Theirs" is player 2's room's record.
"Room-in-play bodies" are the 0x8013085C body test's (2.3).

| frame / bracket | r13-26524 | table +0 | player 2's step: record | bodies (0x8013085C) |
|---|---|---|---|---|
| the behaviour pass (0x8014FDB8, both passes), any frame in play | player 1's object [read: every bracket puts it back] | player 1's slot | **non-player path, by +592: theirs** if loaded [read 2.5] | room in play: **player 1's room's**, at player 2's coordinates |
| their turn (coop_player_turn ~20652: who[] swapped, then `stay_in = coop_stay_room_in`) | player 2's | theirs (when coop_stay_apart) [read] | player path: **theirs**. The turn's guest calls step no body directly (no static path from 0x8013E518, 0x800CBE7C, 0x80134A44, 0x80045BC4 or 0x801EB9A0 to 0x80133434/0x801240B8/0x80045658/0x8013332C within 5 levels [read: call-graph walk]). Events the controller delivers to their node (0x801F59B0 -> 0x80058DD8) can move them (event 157, 0x8005A72C) | their room's (r13-23276 swapped) |
| coop_events / coop_grab_events (their behaviour's events outside the turn; room in when r6_apart and not inside a room change) | player 1's | theirs, if room in | non-player: theirs, either way | theirs if room in, else player 1's room's |
| R6c (r6c_creature: a creature of their room run as its room's: coop_room_in + who[] := player 2's) | player 2's | theirs | player path: theirs | theirs |
| R6 hits (r6_hit_room: coop_room_in only) | player 1's | the hit's room | non-player: theirs | the hit's room's |
| lights (lights_room_in, draw time) | player 2's | theirs | (no step at draw) | (none) |
| ticket countdown and fade-out frames of player 1's door (both still in one room) | player 1's | the shared slot | theirs (= the room in play) | right |
| player 1's change frame: the change runs inside the ticket tick, before the pass (change-freeze-code-readers.md 1) | player 1's | the old slot, then the new after the swap | no step runs inside 0x80152298 for them [inferred: the load's strip and the arrival's walk pass over their node (coop_stay_kind_ask, coop_stay_strip_walk); event 61 to the room left reaches their node while their record is still loaded, as the tail comes after 0x80151FDC returns (0x801523A4 then 0x801523A8)] | (none) |
| the same frame, after the tail | player 1's | the new slot | non-player: theirs, because notify_room_slots_tail puts their record back at 0x80154B54 (lr 0x80152434), before the behaviour pass [read hle.c ~14357; disasm 0x801523A8-0x80152434: only 0x801EB7A0 and 0x80153B2C between] | room in play, now the NEW room's |
| fade-in frames after player 1's change | player 1's | the new | theirs | the new room's |
| player 2's own change (R4 bracket: notify_ticket_run to coop_stay_change_end) | player 2's | player 1's slot until the load's swap, then the new | no step inside; the arrival places them with 0x80130EF0 (it reads r13-26524 at 0x80130F6C) after the swap, so in the new room [inferred] | - |
| after it (table and room word back to player 1's, coop_stay_change_end ~13912) | player 1's | player 1's | non-player: their new room | player 1's room's (the stayer's) |
| player 1's in-world scene (fn22 0x80042974, e.g. 181's 2033) | player 1's | player 1's | theirs (the scene changes nothing a step reads; fn22's player flag 0x801223B8(r13-26524, 4) is player 1's) [read fn22's description: `ed fn fn22`; dis 0x8004299C-0x800429B8] | player 1's room's |
| player 2's own close-up / words (their turn's look, coop_words_tick with room in, coop_p2_own_cancel: be_player2 without room in) | player 2's inside them | theirs in the turn and words tick; player 1's in coop_p2_own_cancel | no body step in a close-up cancel [inferred: 0x80024E90's cancel, camera only] | - |
| player 1's Tome (the game's, shared frames) | player 1's (r13-26524 is not in the tw switches, kTwSwitches hle.c ~13168) | player 1's | theirs | player 1's room's |
| player 2's Tome copy (mode 2, the default: tome_copy_mode ~3936) | the copy memory's; the real world runs on with player 1's | - | theirs, in the real world's pass | player 1's room's |
| **frames with coop_tome_player(true) in force** (owner 1: quick-spell Tome ~26271, legacy Start ~26474, tw_mode_change owner 1 ~15149, the close-up kept as theirs ~26372 then owner 1) | **player 2's, across world frames** | **player 1's** (no room in) | **player path: PLAYER 1'S ROOM**. If any world frame runs then, apart [inferred: not traced in mode 2] | player 1's room's |

Player 1 as the stayer (player 2 took a door): after coop_stay_change_end, player 1 is the room in play again. Their step
takes the player path to the current slot, their own [read]. Player 2's takes the non-player path to their new room.
Every row holds with the rooms exchanged.

- Player 1 can be stepped against player 2's room only with player 2's room current while r13-26524 is still player 1's.
  That happens in coop_events, r6_hit_room and the r6b route job, and none of them steps player 1's body [inferred].
- The r6b job opens on the game's background thread with interrupts off. If it were ever still open in a main-thread
  frame, player 1 (player path) would be stepped against player 2's room. r6b_frame says so with "[enemy] WARNING: a route
  job ... was still open in the frame" [read hle.c ~24622].

## 4. Windows where player 2 is "the player" without their room in

Inside any of these, 0x80123F70(player 2's object) takes the player path to the current slot, player 1's room apart. A
body step, push or drop of player 2 inside one is a step against player 1's room [read 2.5]. Found by grep for
coop_be_player2 and for writes of r13-26524 [read; the step content of each is inferred unless marked]:

- Lasting across frames: **coop_tome_player(true)** (hle.c ~25785) leaves r13-26524 = player 2's object until
  coop_tome_player(false). Callers: tw_mode_change ~15149 (owner 1), coop_menu_input's quick-spell Tome ~26271 and Start
  ~26474, the copy's opening (copy memory only, ~14835), and the equip helpers ~25442/25503/25556 (one call each). While it
  is in force and the world's frames run, the game's own behaviour pass steps player 2 by the player path. Only these
  frames would put player 2 on player 1's room's floor EVERY frame, which is the shape of the fall [inferred].
- One call each, no room in: coop_hazard_touch (~21213; 0x80144F38 plus its event 237 hurt; the hurt reaction's motion
  plays later, in the pass [inferred]); coop_as_player2 (~21568; event 58, 'seen'); coop_seen_hit; coop_talk_check;
  coop_prompt_choose; coop_p2_own_cancel (~15830); coop_p2_load; coop_spells_update / coop_cast (coop_quick_cast in the
  turn has the turn's room in); coop_enchanted_look; coop_move_sound; the sound gain (~14230); notify_copy_watch and
  hle_find_by_id (object and id only); cheat_all_magick; the magick test.
- With their room in, right: coop_player_turn, coop_words_tick (~15961), coop_use_start (~18018), coop_run_script
  (~18614), the Tome item use in coop_menu_input (~26542), r6c_creature, lights_room_in.
- coop_character_step (~24907) would be a third kind: player 2 the player round the game's step, no room in. It is off
  unless ED_COOP_P2_STEP=1 [read].

## 5. Ways the stayer's record can be dead or replaced (frozen, not drawn)

With their record not loaded, or holding another room, 0x80123F70(their object) returns 0. Every step returns `out = from`
(2.2): they can't move and don't fall. room_section_draw (~13693) draws section 3 only for a live record of their room, so
their half shows them with no room round them [read]. That would look like "fell through the floor and could not move".

- **The change's tail** (0x801523A8-0x80152404) marks the other slot dead after every change [read]. It is put back by
  notify_room_slots_tail (~14357) through room_slot_put_back (~14344), but only if:
  - `g_room_slot_room[slot]` is the stayer's room (learned at the room draw, room_slots_frame ~13626; set to -1 by
    notify_room_unpack_post ~13607);
  - the record's +0x8144 is 0 (no prefetch pending);
  - the word reads exactly room -1 and loaded 0.

  A miss leaves them frozen and not drawn [read]. A run shows it as no "[rooms] slot N's record put back ... (a player is
  in it)" line after "after the change's tail".
- **The loader thread** (0x801546A4; fn71 room_queue = 0x80154EC4 posts to the queue at 0x805B27BC, read by the loader at
  0x801546E0) reads table +4 when it takes a message (0x80154760) and works on that slot.
  - Apart, outside brackets, +4 is the stayer's slot.
  - Some branches skip when that record is loaded (0x80154834-0x8015483C, 0x801548B0-0x801548B8) [read]. The rest of its
    case table (0x80154974 bctr, cases 2-11) is not traced.
  - Any "unpack asked: room R into slot S" line (notify_room_unpack_post) with S the stayer's slot while apart is this.
- **0x80155038(r13-26524), every frame at 0x80008544** [read]. Through 0x80154EF4 it finds the entry nearest the player
  from the current record's list (count u16 at +192, 92-byte entries). If table +4's record holds another room, loaded or
  dead, it overwrites it: room := entry +40, loaded 0, +0x8143 0, then 0x80154EC4(0) [read 0x8015505C-0x8015511C]. That
  would evict the stayer's room by player 1's position. **It does nothing on this disc:** +192 is 0 in all 297 room files,
  and no store to record+192 is in the room code 0x80151000-0x80156000 [read: the scan of every rmed*.bpe; grep].
- **The room reloads.** 0x801556A8 marks the current record not loaded, then 0x80152298 runs again on the room in play:
  - 0x8004FC3C (from 0x8004FBA0/0x8004FBD0);
  - 0x80110DA8, the Tome giving the room back (7 Tome sites; doors-readers.md);
  - 0x80157458 (from 0x80026484/0x800264A4/0x800264C4) [read].

  room_slots_return runs on every 0x80152298 (notify_room_change ~11239). It swaps +0/+4 if the stayer is in +4, and the
  load's "already in the other slot" case (0x80154D64-0x80154D90, which tests only the room halfword) swaps them back.
  The not-loaded current is then reloaded by the loader's own branch (0x80154780: current not loaded) [read the cases;
  inferred: the outcome]. The stayer's slot isn't the one reloaded [inferred].

## 6. What the scripted attempts covered, from their logs

- logs/run-20261112-033738-p2-follows-181 [read: both.tsv and page-0001.log]:
  - Player 1's change 182 -> 181 is at frame 4764-4770: "slot swap (a fresh load)", "unpack asked: room 181 into slot 0",
    "slot 1's record put back: room 182 ... (a player is in it)", "[stay] player 2 stays in room 182 (slot 1)".
  - Script 205 ran, then **2033: the first-visit scene** ("scene 2033 begins as player 1's", busy 34, frame 4766). Player
    2's turn was answered "not busy" and went on.
  - Player 2 stood at (-769,2229,78.4) by EntryMkr02 through it, then walked into Region02 at 4991. Their own change to
    181 (R4, 5052-5054) was clean.
  - **Player 1 stood at 181's entry the whole time** (x -2147 to -2139.8).
- So the scripts did not cover:
  - player 1 walking about in 181 while player 2 stays (181's bodies move across 182's coordinates);
  - player 2 standing or walking where 181's bodies or player 1's body lie in 182's coordinates (round the statue,
    x -1718..201, y 345..1785);
  - player 2 moving in the change frame itself;
  - fighting, a hazard, a quick spell or a Tome of player 2's while apart (section 4);
  - a second change of player 1's (to 184 or back) with player 2 still in 182.

## 7. What a log of the bug would settle (open)

- **Their room's record when they fell.** Look for the put-back line after the tail, and for any "unpack asked ... into
  slot <theirs>" or "slot swap" while apart. In both.tsv: p2z falling (a real fall, section 2.4) or p2z constant with no
  motion (a dead record, section 5).
- **Whether they fell by the room-in-play bodies.** Player 1's or a 181 creature's place (in 181's coordinates) within a
  body's reach of player 2's (in 182's) in the frames before p2z drops.
- **Whether any coop_tome_player(true) or quick-Tome line** ("player 2 opens the Tome", "player 2 has the ...") came
  while apart before the fall.
- **The fn22 scene in 181 and player 2's turn.** The run above says it answered "not busy". A user play with player 2
  moving through it is not recorded.
- **Not read:** 0x80130504's body shape (sphere at +624 when +596 bit 0x100000, else 0x8013012C), and so whether a
  room-in-play body's push can point down; the loader thread's cases 2-11; whether owner 1 with coop_tome_player(true) is
  reachable in mode 2 while apart.
