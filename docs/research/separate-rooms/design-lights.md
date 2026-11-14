# Lights and shadows per room: the final design (2026-11-12)

Item: with the players apart (ED_ROOM_SLOTS=2, ED_COOP_STAY=1, Mods > Co-op on), each half is lit, hazed and shadowed by its
own room. Readers: `lights-game-readers.md` (the game's lighting) and `lights-render-readers.md` (host and renderer).
Refuters: `refute-lights-impl.md` (the current implementation) and `refute-lights-goal.md` (the goal, both players). Every
refuter problem was checked again against the code and the logs here; section 12 lists each and what was done.
Nothing was edited, built or run. Tags: [read] = seen at the address, file:line or log line given; [inferred] = reasoning
from what was read.

Words used:
- **room0** = the room in play, `r13-23276` outside a turn. Apart, that is player 1's room (hle.c:13832-13837, 13697) [read].
- **room1** = player 2's room, `coop_object_room(cpu, g_coop_p2)` (hle.c:13820) [read].
- **View 0** = player 1's half, **view 1** = player 2's.
- **coop_stay_apart** (hle.c:13832-13837) [read]: ED_COOP_STAY and two slots are set, player 2's room is resident in a slot
  the host knows (`room_slot_of`), it is not the room in play, and no stay bracket is running. It does NOT test
  `g_settings.mod_coop` (`r6_apart` does, hle.c:24197) [read].
- **LA** (lights apart), the ONE predicate of every new path, defined in section 2.0.

## 0. The chosen approach, in one paragraph

The game already lights an actor from its own room's record inside the cell lookup. Its one gap is which grid the
character draw registers. The game also already marks each actor and room section by room for the halves (R3). So:
- **The game side (hle.c):** each thing is lit by its own room's game, using the game's own functions with that room's
  words in. All three brackets open only under LA, keyed on object +592 and the slot records:
  - **G1**: an actor of player 2's room registers with its own record's grid (an lr-gated answer of the game's record
    getter 0x80155130).
  - **G2**: its lights are made with its room and that room's player in (a bracket around the game's own actor-light
    functions).
  - **G3**: player 2's room section gets its own room's live lights (the game's own loader 0x801F2CB4, run under that room's
    words before section 3 and run again plain before section 2).
- **The renderer side:** the host decides once a frame (at the opaque pass's section 3) and sends two lamp sets read from
  the game's own per-slot light grids, each only from a record that passes the game's own loaded test. The renderer fills
  one shadow/haze Frame per view, each from the casters that view keeps (their R3 marks) and that view's room's lamps. It
  takes the per-view rule at its own frame start. Each view gets its own camera offset. The spell lights get a view mask
  from the caster's room.

Solo and the players together run today's code unchanged.

## 1. Facts this design rests on (verified here)

### 1.1 Which grid an actor gets
- `0x8002790C` (the character draw callback, every character, node +24) registers its object with
  `0x80155130(2)` (the CURRENT slot's record) -> `table = record+112` -> `g = table[byte r13-22136]`; if `*(g+4) == 0` then
  `table[0]`; no table -> grid 0; then `0x80119B94(object, g, 3000)` [read 0x80027984-0x800279EC]. The object is r30
  (`0x801F633C(node)` = node+52, 0x80027934-0x80027938) [read].
- r21 (the record) is read ONCE, at 0x80027998 (+112), and next written at 0x80027A30 / 0x80027BE4. So an answer for this
  one call changes only the grid choice [read: every r21 use in 0x8002790C..0x80028258; both refuters agree].
- The grid in r16 is used in 0x8011A278 only for the null test (0x8011A47C) and as r5 of the light call (0x8011A568)
  [read: impl refuter]. So G1 changes nothing but lighting.
- `0x80155130(n)`: n == 2 -> `*(0x8058AFC0 + 12 + 4 * table[+0])`; n == 3 -> the other slot's record (0 when +4 == -1);
  else `*(0x8058AFC0 + 12 + 4n)` [read 0x80155130-0x8015519C]. First word `cmpwi r3,2` (0x2C030002) [read].
- **The game is already room-aware below this:** `0x8011A278` sets `r17 = 0x80118CAC(obj)` = obj+592, the object's ROOM
  (0x8011A4BC-0x8011A4C4), and calls `0x801E6244(&pos, r14, grid r16, room r17, budget r25, flags r23)`
  (0x8011A564-0x8011A57C) [read]. Inside, `0x801E5DDC` calls `0x80155544(room)` (0x801E5E28). That returns 2 if the current
  record's room (+0x8140) is that room with +0x8143 set, 3 if the other slot's is, else -1 [read 0x80155544-0x801555FC].
  Then `r25 = 0x80155130(that)` and `r27 = r25 + 160`, a per-room table [read 0x801E5E9C-0x801E5EDC]. **Only the grid
  handed down from the registration is the current slot's.** G1 closes exactly that gap.
- The other slot's +112 table is never read by the game [read: game reader 1.2].
- Every record read has three pointers at +112. In rooms without lightning, [1] and [2] count 0 (rooms 100, 183, 338,
  147, 249, 180, 341 in saves/*.state) [read: goal refuter, tools/ed/savestate.py]. So G1 indexing room1's table with
  room0's lightning byte falls back to [0] in those rooms. No lightning room was found in the states (+142 = 0 in all).
- **A dead record keeps a good-looking table.** In saves/rooms-ch1-183.state, rooms-ch1-182.state and rooms-r6-base.state,
  slot 0's record 0x806DA640 has room 0xFFFF, yet +112 = 0x807931A8 -> [0] 0x807931B4 with 1089 cells [read: goal
  refuter]. So any read of +112 needs the record's loaded test first.

### 1.2 Words read while an actor is lit (callee scan of 0x8011A278, depth 3)
- `0x801E6244`: `r13-23276` at 0x801E6698 (the dynamic light's room must equal it). `0x801F19A0` (the count): +42 ==
  `r13-23276` (0x801F19D0-0x801F19DC). +41 bit 0x08: the light's room must equal the room of `r13-21492`'s object
  (0x801F19FC-0x801F1A28). `0x801E6B7C` (the cell lookup) reads `r13-23276 == 292` at 0x801E72F0 [read].
- `0x801E87F4` (no grid / outside) reads `r13-23276` at 0x801E8A8C. `0x801E8B9C` (object +596 bit 0x8) reads it at
  0x801E8C18 [read].
- The object is in r15 of the actor draw: r15 = r3 at 0x8011A298, `or r4,r15,r15` at 0x8011A588 and 0x8011A5C4. The three
  light calls return to 0x8011A580 (0x801E6244), 0x8011A5B8 (0x801E87F4) and 0x8011A5F0 (0x801E8B9C) [read]. 0x801E6244
  takes f1 (`fmr f1,f30` at 0x8011A560; `fmr f31,f1` at 0x801E6268) and r3 is a pointer into the caller's frame (`addi
  r3,r1,104`, 0x8011A578); only r3 comes back (`or r23,r3,r3`, 0x8011A580) [read: both refuters].
- **What each actor gets** [read: game reader 1.3, 0x801E652C-0x801E654C]: fills 0-3 and one far light 4 from the cell
  lookup (so from the grid), then dynamic lights, then, ONLY for `obj == r13-26524` (budget 4), up to 3 room lamps (lights
  5-7). Player 2's body gets the non-player budget, together and apart (L3, section 8). So G1 changes player 2's fills and
  far light, G2 their dynamic lights.
- An actor's COLOR1 mask is light 4, not 7 (`li r24,16` at 0x801E6290, `and r4,r26,r24` at 0x801E6914) [read: goal
  refuter]. So the room's light 7 (above `r13-26524`) never reaches a body.
- Not lights, and not in this design: `0x8011A278` reads `r13-26524` at 0x8011A3C0 / 0x8011A3F8 / 0x8011A9E8 (hold flag,
  the player-only budget, a draw test). `0x8011BE00` (from 0x8011B92C) tests `r13-23276` against rooms 83/99/213 and
  145/311/334 (0x8011BE64-0x8011BE7C, 0x8011C058-0x8011C070) [read]. See section 8.
- **Host hooks.** None sits on 0x801E54BC, 0x801E58C8, 0x801E6244, 0x801E87F4, 0x801E8B9C, 0x801F2CB4, 0x801F19A0,
  0x801E5DDC, 0x80119B94, 0x80155544 or 0x80155130 [read: grep of runtime/host, both refuters]. The Tome's
  `0x801E58C8(0,0,0)` (hle.c:14838, 15068) are host calls, not hooks [read]. Two CALLEES of 0x801F2CB4 have intercepts:
  0x801F62B0 (`coop_interface_player`, hle.c:19701, lr-gated to 0x80080EC0/0x80080EC8 and a lent close-up) and 0x80118274
  (`coop_position_copy`, hle.c:21363, acting only at lr 0x8011CB74) [read]. Neither gate is met from 0x801F2CB4, so G3's
  bracket doesn't reach their bodies today [read].
- **The character callback runs nested.** `hle_draw_characters` is an intercept on 0x8002790C itself: `find_show_player`
  takes the function holding `lwz r0,-26276(r13); cmpwi r0,0; bne` (0x8002794C-0x80027954), whose stwu is 0x8002790C
  (hle.c:6436-6465) [read]. So every character callback runs inside `hle_guest_call` (hle.c:6411-6433) with the show flags
  set. G1's getter call, at return 0x8002798C, sits inside that nested call [read].

### 1.3 The room sections' live lights
- Opaque pass `0x801E851C`: 4 x `0x801E0F74`, `0x801F2CB4()` (0x801E8584), `0x801E14F4(&white)` (0x801E859C),
  `0x801E16D0(2)` (0x801E85B0), then `0x801E75B4(3)` and `0x801E75B4(2)` (0x801E85C4/0x801E85CC). Translucent pass
  `0x801E85E0`: `0x801F2CB4()` at 0x801E8684, `0x801E14F4`, `0x801E16D0(2)`, then `0x801E73D4(3)`, `0x801E73D4(2)`
  (0x801E86CC/0x801E86D4) [read]. **Section 3 is always drawn before section 2, in both passes.**
- `0x801F2CB4` reads `r13-26524` (light 7's place, 0x801F2D28), `r13-23276` (the dynamic lights' room, 0x801F2E8C) and
  `r13-21492` through 0x801F62B0 (rules 0x02/0x08). Its last call `0x801E58C8` reads `r13-23276 == 50` (0x801E5C80) [read].
- **What it writes** (corrected from the first design): the light objects (through 0x801E54BC); `r13-22020` and the byte at
  `r13-26919` (the GXSetTevKColor cache in 0x801E15FC, 0x801E1618/0x801E1628, GXSetTevKColor 0x8021E31C at 0x801E1634; its
  argument is the constant 255, 0x801F2D58/0x801F2DD0, so it is idempotent); the float `r13-26884`, decremented on each
  light-0 load (0x801E5534-0x801E5540; its only other reader is the same lfs, so it is a write-only counter); and
  `r13-21800`, `r13-21892`, `r13-21896`, `r13-21796` in 0x801E58C8 (0x801E5908, 0x801E590C, 0x801E59E0, 0x801E5A98) [read:
  this session for 0x801E15FC and r13-26884; impl refuter for 0x801E58C8]. The slot table is never read [read].
- `0x801E58C8` issues GXSetChanCtrl / GXSetChanMatColor / GXSetChanAmbColor and GXLoadTexMtxImm (45). `0x801E14F4` issues
  GXSetTevColor (0x8021E234), cached in `r13-22012`. `0x801E16D0` sets only vertex descriptors [read]. **So running
  0x801F2CB4 again after 0x801E14F4 and 0x801E16D0 undoes neither.**
- The light places go into the eye space of `0x8060248C` (camera matrix 27, the game camera) [read: 0x801E54BC].
  room_section_draw loads a MARKED COPY into GX index 27 and never writes 0x8060248C (hle.c:13671-13687) [read].

### 1.4 Dynamic lights
- 12 records (pointers at 0x80604E60). A light is counted only if active and +42 == `r13-23276` [read 0x801F19C4-0x801F19DC].
  The room is stamped at creation: desc+48, or `r13-23276` when that is 0 [read 0x801F2A20-0x801F2A38]. Lights are made
  only at 0x80026EC0 (the node event handler) and 0x801458C4 (in 0x801458A0) [read: the only `bl 0x801F1D84` sites].
- **A held light follows its holder's room by itself** [read this session]:
  - The torch record 0x8030AA98 is filled by 0x80079318: holder id at +100 (0x800793D0), desc+48 = 0 (0x800793AC), so the
    stamp is the room in play at creation, and **+104 = 0x80145AF8** (0x80079370 `addi r4,r3,23288` with r3 = 0x80140000,
    stored at 0x800793CC).
  - `0x801458A0(holder, record)` makes the light (0x801458C4), then sets the light node's step to record+104
    (0x8014594C-0x80145954, `lwz r4,104(r30)`; `bl 0x8014F86C`) and attaches it to the holder (0x80145960, 0x8014FEE8).
  - The step `0x80145AF8` sets the light's room to the holder object's room every time it runs:
    `0x801F1F90(light, 0x80118CAC(holder object))` unless +41 bit 0x04 (0x80145B7C-0x80145B98). `0x801F1F90` is
    `sth room, 42(light record)` (0x801F1FB0). It is 0x801F1F90's only caller [read: grep].
  - The enchanted weapon's record (0x800DA554) also stores 0x80145AF8 (0x800DA5F4) [read].
  - So player 2's lit torch apart carries room1 once its step has run [inferred: the step runs with the node walk; U4
    confirms]. Today it lights NOTHING in either half: every draw counts only +42 == room0 [inferred from the count test].
  - Player 2's torch light is made by the host with the game's own function (`coop_torch_light_now`, hle.c:12669-12682, the
    game's 0x801458A0 with player 2's record and holder) [read], so the same step follows it.
- **Two rooms share the 12 records.** The allocator `0x801F1B40` keeps the 12 ordered by type (+45; scan
  0x801F1B74-0x801F1B8C), takes ENTRY 11's record (0x801F1B9C `lwzu r31,44(r4)`), shifts the rest down
  (0x801F1BB4-0x801F1C14) and stores at the found rank (0x801F1C4C) [read]. Whatever entry 11 held is overwritten. Lights
  are freed with their nodes (0x80145974, 0x80145AE4, 0x8012C0F0) [read: goal refuter]. With two resident rooms, both
  rooms' lights and both players' torches compete for 12 records. That exists already since R1 and is not caused by this
  design; it limits what G2/G3 can show (section 8, U4).
- The light node's event handler `0x80145994` compares the room of the MAIN player's object (`0x801F6250` = `lwz r3,
  -21484(r13)`) with `r13-23276` and runs `0x801F2858` (a timed step of the record at +38/+48) only when they match or +41
  bit 0x10 is set (0x80145A68-0x80145A8C) [read]. Which event this is was not read (section 9).

### 1.5 The host today
- `room_lights_update` (hle.c:6099-6188) runs from `notify_look_at` (hle.c:6219-6234), which tests only `lr ==
  g_camera_look_at_return` [read]. That return is the C_MTXLookAt inside `0x801E4BDC` (find_camera, hle.c:6689-6700) [read].
- **It runs on EVERY call of 0x801E4BDC, not once a frame** (corrected). 0x801E4BDC has 9 bl sites [read: disasm.txt]:
  0x801E1204 (frame begin 0x801E1164, from 0x8000843C), 0x801E97D0 (0x801E9728, from 0x80008128), 0x801E9104 (0x801E9014,
  from 0x800081A4 after the actor list AND from 0x80119FB0 inside the actor list 0x80119D60), 0x801E9640 (0x801E9534, from
  0x800081D8), 0x801EA0F0, 0x801F192C, 0x80106114, 0x80106A1C, 0x8010F310.
- So 0x804F0784 (the list's first entry) is the PREVIOUS frame's entry only for the calls before the actor list; the
  calls at 0x80119FB0 and 0x800081A4 read this frame's [inferred: the reset at frame begin stores only the count].
- With two slots it keeps a grid only inside the current slot's span (hle.c:6125-6131). It sends one set
  (`dol_aurora_set_room_lights`, an in-stream `gxq::call_sync`, aurora_backend.cpp:3209-3219) [read].
- **The host's slot rooms.** `g_room_slot_room[]` (hle.c:13503) is written at hle.c:13608 (an unpack asked: -1), 13634
  (room_slots_frame, only while the record is loaded) and 14304 (the change's tail: the current slot's room at once)
  [read: grep]. `hle_coop_state_load` (hle.c:28020-28090) never resets it [read]. `room_slot_record` checks only the
  pointer range (hle.c:13505-13509) [read]. `r6_room_loaded` (hle.c:24216-24222) checks the record's live room,
  +0x8142 and +0x8143, as the game does [read].
- The user's run: room 341's grid 0x8307EC28 (18 lamps, slot 1) and 342's 0x807CEA6C (9 lamps, slot 0). Apart, only 342's
  is sent (logs/run-20261111-222138-rooms-play page-0001.log:172, 2813, 2821) [read]. `ed lights 341`: lamp 0 blue-grey
  (92,107,124). `ed lights 342`: all amber (226,169,47) [read].

### 1.6 The frame and the doors
- Frame order [read: disasm 0x800083E4-0x8000851C]: 0x800083EC the ticket tick 0x80155830 (which runs the change through
  0x80152614 from 0x801558F4); 0x800083FC the behaviour pass 0x8014FDB8 (return 0x80008400); 0x8000843C frame begin
  0x801E1164; 0x8000851C the world draw 0x800080E4 (opaque pass at 0x80008118, camera at 0x80008128, node walk 0x8000812C,
  actor list 0x80008148).
- **Player 2's door.** `notify_ticket_run` opens `g_stay_bracket` at the runner (hle.c:13858-13868) and
  `coop_stay_change_end` closes it at the behaviour pass returning to 0x80008400 / 0x80050A9C (hle.c:13870-13895) [read].
  On the ticket route both are before the frame's world draw, so **no draw sees the bracket** [read: frame order]. The
  runner's other caller, 0x800AE000, is a script route (no bl site, reached by a table) [read: `ed fn`]. Where its script
  runs in the frame was not read, so a bracket opened there could last over one draw [inferred: not settled].
- **The unpack is inside the change.** In logs/run-20261111-172710-rooms-r4c-t1-p2door the change opens at frame 3822, the
  unpack is asked at 3822, and the tail (record loaded, host room known) and the bracket's end are at 3824
  (page-0001.log:644-666). both.tsv has NO rows for frames 3823-3825 (ms 1794436037241 -> 1794436037307, 66 ms) [read]: the
  change's load ran with no world draw. The user's two play runs show the same (unpack -> tail in 1-2 frames, inside the
  change: logs/run-20261111-221141-rooms-play and -222138-, `[rooms] unpack asked` / `after the change's tail`) [read].
  So in every run read, **no drawn frame had Apart off because a slot was unpacking**. This 66 ms with no draw is the
  user's "player 1's screen pauses a brief second" [inferred]: it is the change-freeze item (design-change-freeze.md), not
  this one.

### 1.7 The renderer today
- One lamp set (`g_lamps`, `g_gridCells`, gxcore_draw.cpp:1025-1038). `set_room_lights` forgets EVERY shadow slot
  (1567-1569) [read].
- `submit_draw_plan`'s frame start: `if (g_sh.frame != current_frame())` resets flats, setCount, seenCamera and
  `g_cameraUniformOffset` (gxcore_draw.cpp:2125-2131) [read]. The camera is the FIRST scene draw with no matrix index
  (2133-2136, offset at 3096). Flattened draws are grouped within 80 units (2148-2172). Lit sets are keyed by the FNV print
  of the draw's 8 XF lights, whatever the mask (2184-2212), capped at 48 (`ShadowLights sets[48]`, 1015; `setCount < 48`,
  2195). None of this reads the marks [read].
- `fill_shadow` (1111-1541) runs once a game frame at the display copy (1723), only when the frame had enough 3D draws
  [read]. Slots are matched by world place within 150 (1268-1290). Each caster's lamps come from the one grid. The 12 tiles
  are scored by distance from the game camera (1447). The haze takes the 12 nearest lamps (1500-1539), plus every spell
  light (1095-1109) [read].
- The replay: `build_view` changes only projections and zeroes the matrices `marked_out_of` the view (interp.cpp:569-578,
  704-719). `render_character_shadows` runs per view per glowScene pass (common.cpp:2558-2562, 2715-2774) [read].
- `at_this_moment` (shadow.cpp:842-878) reads the view's Vs at `frame.cameraOffset`; if that block differs from
  `frame.camera` it moves every tile and haze light by the moment's camera [read]. `probe_label` stores the view being
  encoded for every Split / SplitBetween output (common.cpp:2169, 2200; shadow.cpp:880-886) [read].
- **P4, a likely bug today in player 1's half:** apart, if section 3 (drawn with the 0xA0 mark = out of view 0) is the
  frame's first scene draw, view 0's camera offset points at a block zeroed in view 0, and at_this_moment collapses every
  tile and haze light to the eye [inferred]. Picture 000121 of logs/run-20261111-222138-rooms-play (frame 2300, apart)
  shows a dark shape under player 1 that may be a shadow, so P4 is NOT confirmed. U1 settles it; R1.3 is right either way.

## 2. The design

### 2.0 One predicate, one decision a frame (host, hle.c, beside coop_stay_apart)
- **`lights_apart(cpu)`**, the gate of every new path (G1, G2, G3, H's set B and per-view, the spell masks):
  ```c
  g_settings.mod_coop && !g_tome_copy_turn && g_room_whole_owner == 0 && camera_split_on() &&
  coop_stay_apart(cpu) && r6_room_loaded(cpu, room1)   // room1 = coop_object_room(cpu, g_coop_p2)
  ```
  - It is R3's own test (coop_found_object_draw: `coop_stay_apart && camera_split_on()`, hle.c:3300; the per-view path of
    room_section_draw, hle.c:13698-13744, which is reached only with the split on and no whole owner) plus co-op on, no Tome
    copy turn and the record loaded. So the lights follow exactly what the picture shows.
  - room1 != room0 is inside coop_stay_apart. In player 2's turn `r13-23276` is room1 (coop_stay_room_in), so
    coop_stay_apart is false there (hle.c:13835-13836) [read]: no new path ever opens inside a turn.
  - G1, G2 and G3 evaluate it live, at the same moment as R3's marks. A frame where the split changes between the room
    sections and the actors is treated as R3 treats it today.
- **The frame's decision (H):** made ONCE a frame in `room_slots_frame` (hle.c:13622), which runs at the opaque pass's
  section 3 (hle.c:13693-13694), before any room section is drawn and before any actor registers [read: 1.3, 1.6]. It
  stores `g_lights_frame = lights_apart(cpu)` and does all of H's sends there (2.H). `room_lights_update` makes no
  decision for the new paths; it only steps aside while H owns set A.

### G1. An actor of player 2's room registers with its own room's grid (host, hle.c)
- **Hook:** an intercept on `0x80155130`. Install it next to the other room hooks when `coop_stay()` (the install block in
  `find_coop`, which runs before `publish_codegen_options()`, hle.c:28348-28354, so the new intercept gets its chain stop),
  checking the first word 0x2C030002 and that no hook is there yet.
- **Body** (`room_record_for_actor`), passthrough unless every test holds:
  1. `cpu->lr == 0x8002798C` and `gpr[3] == 2`;
  2. `lights_apart(cpu)`;
  3. object `o = gpr[30]` valid for 596 bytes, with `o+592 == room1`.
  - Then answer `room_slot_record(cpu, room_slot_of(cpu, room1))`. That is the record the game's own
    `0x80155130(0x80155544(room1))` gives (the other slot's, 3).
  - `hle_set_u32`, `g_hle_passthrough = false`, `g_hle_tail_call = false`, as room_slots_count does (hle.c:13540-13555).
- **What the game then does by itself:** the table at that record's +112, the light-set index and the empty-cells fallback
  (0x80027998-0x800279D8). The grid reaches 0x8011A278 -> 0x801E6244 -> the cell lookup with the right room's cells and the
  right record's +160 table, which 0x801E5DDC already picks by room.
- **Both players:** player 2's body and every character of room1 (enemies, NPCs, characters they hold) get room1's grid.
  Player 1's body and room0's characters get the current record as today (no answer for room0 objects). The callback runs
  inside hle_draw_characters' nested call (1.2); the getter's answer doesn't depend on nesting.
- **Fallback if the getter's cost shows (U6):** an intercept on 0x80119B94 at lr 0x800279C8/0x800279DC that redoes the three
  selection lines.

### G2. An actor of player 2's room is lit with its room and that room's player in (host, hle.c)
- **Hooks:** intercepts on `0x801E6244` (first word 0x9421FE20), `0x801E87F4` (0x9421FF00) and `0x801E8B9C` (0x9421FF50).
  Each opens only at its call site in the actor draw: lr 0x8011A580, 0x8011A5B8 and 0x8011A5F0 in turn. Any other lr (the
  nested `0x801E87F4` from 0x801E6350, return 0x801E6354; any other caller) passes through.
- **Gate:** `lights_apart(cpu)`; object `o = gpr[15]` valid, with `o+592 == room1`; not already inside (a static depth flag).
- **Bracket**, its own helper `room_draw_in / room_draw_out` (NOT shared with r6c_creature, which stays as it is):
  - `coop_room_in(cpu, room1, keep)` (the room word and the slot table, hle.c:14262);
  - the three player words `r13-26524 / -21492 / -21496` := player 2's object, node and id, the old ones kept;
  - `hle_guest_call(cpu, fn, gpr[3..8], 6, cpu->fpr[1], &out)`;
  - the words back, `coop_stay_room_out`, `gpr[3] = out`, no passthrough.
- **What the game then does by itself:**
  - the dynamic lights counted and loaded are room1's (+42 == `r13-23276`), so player 2's torch (1.4) lights player 2's body
    and room1's actors, and player 1's torch no longer lights them;
  - the 0x08 rule tests player 2's room; the room-292 cell rule sees room1;
  - `0x80155544(room1)` now returns 2: the current record, which is room1's, the same record as without the swap (1.1).
- **Not swapped:** nothing in 0x8011A278 itself. coop_found_object_draw (a notify at its entry, hle.c:3276) runs before the
  bracket and reads the unswapped words, so the marks are as today. The budget test (`obj == r13-26524`, 0x8011A3F8) runs
  before the bracket, so player 2's body keeps the non-player budget, together and apart alike (no flip at a door; L3).

### G3. Player 2's room section gets its room's live lights (host, hle.c, inside room_section_draw)
- **Where:** the per-view path of `room_section_draw` (hle.c:13713-13744), both passes, with `lights_apart(cpu)` true.
  - Section 3, only when it is really drawn (live, room == room1, room != room0), before `room_mtx_load(cpu, 0)`:
    `room_draw_in(room1)`, `hle_guest_call(0x801F2CB4)`, `room_draw_out`. Set a per-pass flag `g_g3_ran = true`.
  - Section 2, before `room_mtx_load(cpu, 1)`, ONLY if `g_g3_ran`: `hle_guest_call(0x801F2CB4)` with no bracket, then clear
    the flag. That puts back exactly what the game loaded before section 3: the light objects, light 7 over player 1, the
    KColor cache, and the four 0x801E58C8 words (1.3). `r13-26884` is decremented once more per run; nothing reads it.
  - Without `g_g3_ran` (section 3 returned early: not live, not room1), section 2 takes today's per-view path untouched.
- **What it gives:** section 3's draws carry room1's dynamic lights on COLOR0 and light 7 above player 2 on COLOR1 (the game's
  own rule for its one player: player 2 is room1's only player). Section 2's draws carry player 1's, as today. The room-50
  test in `0x801E58C8` sees each room. GX loads are recorded per draw (lighting.cpp:23-82), so each section's draws keep
  their own [inferred].
- **Order safety:** section 3 always comes before section 2 (1.3). Everything after section 2 sees the game's own state,
  because each actor reloads its lights (0x801E6244 -> 0x801E58C8) [read].
- **Leftover XF lights:** light slots room1's run loads past room0's dynamic count stay loaded (masked off) into section 2
  until reloaded. The renderer's lit-set print hashes all 8 lights whatever the mask (gxcore_draw.cpp:2186-2189), so later
  draws' prints can change with room1's count. The same already happens between actors today, so this is noted, not
  changed (U8 counts sets).
- **The whole-owner branch (`g_room_whole_owner != 0`) and Tome copy turns:** untouched; LA is false there (section 8).

### H. The host's two lamp sets, decided once a frame (hle.c, room_slots_frame and room_lights_update)
- **New helper `room_light_grid(cpu, slot, room)`** returns 0 unless ALL hold; else the grid:
  - `record = room_slot_record(cpu, slot)` != 0, its +0x8140 halfword == `room`, and +0x8142 and +0x8143 set (the game's
    own loaded test, as 0x80155544 / r6_room_loaded);
  - `table = *(record+112)` and `grid = table[0]` (the game's always-valid fallback, 0x800279CC) lie in RAM;
  - the grid lies inside that slot's span (`r13-23304 + 4*slot` base, ROOM_SLOT_BYTES);
  - `grid+32 == grid+36`, `count == dims product` (today's tests, hle.c:6111, 6146).
  - Index 0, not `[r13-22136]`: the renderer's lamps are for shadows and haze; following the lightning index would re-send
    and reset slots on every flash. U2 logs all three entries and the index.
- **State** (file scope, shared with today's path): `g_lights_sent_grid / g_lights_sent_mark` (today's statics
  `sent_grid / sent_mark`, hle.c:6101, moved out so both paths see one record of what set A holds), `g_lights_own_a`, and
  set B's `g_lights_b_on / _room / _grid / _mark`.
- **Each frame in room_slots_frame** (after the slots' rooms are learned, hle.c:13627-13636), with `ap = lights_apart(cpu)`:
  - **While `g_stay_bracket != 0`** (only possible on the script route, 1.6): change nothing, send nothing.
  - **Set A**, when `ap`: `g_lights_own_a = true`; `grid = room_light_grid(current slot, r13-23276)`; send through today's
    `dol_aurora_set_room_lights` when grid != 0 and its grid or mark differs from the shared sent pair. A grid of 0 (the
    current record not loaded) keeps A as last sent: no send, no slot reset.
  - **Set A**, when not `ap`: `g_lights_own_a = false` (today's path takes A back at its next call).
  - **Set B**, by the players' rooms, not by `ap` alone:
    - The players together, co-op off, player 2 gone or ED_COOP_STAY off (`!g_settings.mod_coop || !coop_stay() ||
      g_coop_p2 == 0 || room1 == -1 || room1 == room0`): if B is on, `dol_aurora_set_room_lights_other(0, ...)` once, B off.
    - Apart and room1's record passes (`room_light_grid(room_slot_of(room1), room1)` != 0): send set B when its room, grid
      or mark changed (the same cell walk as today's).
    - Otherwise (in different rooms but the test fails: a slot not yet known, a record put back later, a state just
      loaded): if B's room == room1, keep B as it is; else send B off. Never a set B from a record that isn't loaded.
  - **Per view this frame:** `dol_aurora_set_lights_per_view(ap && g_lights_b_on)`, sent only when it changes.
- **room_lights_update** (today's path, every 0x801E4BDC call): unchanged byte for byte, except one line after the `[sync]`
  log: `if (g_lights_own_a) return;`. With `g_lights_own_a` false (solo, together, any frame LA is off) it is today's code.
  - Why H owns A while apart: G1 can make 0x804F0784's first entry a room1 object, whose grid the span rule rejects; A would
    then stay stale after player 1's next door.
  - At the edge back to together, today's path compares 0x804F0784's grid with the SHARED sent pair, so the same grid is
    not re-sent. In a lightning room (`table[r13-22136]` != `table[0]`) it is re-sent once at that edge (one set-0 reset at
    a door, as today at any room change).
- **During g_tome_copy_turn:** room_slots_frame doesn't run (hle.c:13693), so nothing changes.

### R1. Two lamp sets, per-view Frames, per-view camera (renderer, .deps/GXRuntime)
Files: `graphics/aurora/lib/gfx/gxcore_draw.cpp`, `gxcore_draw.hpp`, `shadow.hpp`, `shadow.cpp`, `ssao.hpp`, `common.cpp`,
`interp.cpp` (+ its header), `backends/aurora/aurora_backend.cpp`, `include/gxruntime/aurora_backend.h`. Then
`git -C .deps/GXRuntime diff > patches/gxruntime/eternal-darkness.patch`.

1. **Lamp sets.** `g_lamps[2][96]`, `g_lampCount[2]`, the grid fields `[2]`, `g_lampsFresh[2]`, `g_setBOn`.
   - `set_room_lights` (set 0) keeps its signature and forgets only set-0 slots. In solo every slot is set 0: today's.
   - New `set_room_lights_other(on, ...)` fills set 1 and forgets only set-1 slots. `on == 0` clears set 1 and its slots.
2. **The per-view rule, latched per frame.** New `set_lights_per_view(on)` stores `g_perViewNext`. At the frame start in
   `submit_draw_plan` (the `g_sh.frame != current_frame()` block, gxcore_draw.cpp:2125-2131): `g_perView = g_perViewNext &&
   g_setBOn`. `fill_shadow` and every collection of the frame read `g_perView`, so one frame is collected and filled under
   one rule. The host sends it at section 3, before the frame's actor draws (2.0), so in practice it lands the same frame.
3. **Marks on what is collected** (submit_draw_plan, 2124-2213).
   - Expose `marked_out_of` from interp.cpp's anonymous namespace. For each flattened draw and each lit-set draw,
     `outOf = (marked_out_of(M,0) ? 1 : 0) | (marked_out_of(M,1) ? 2 : 0)`. M is `plan.constants.posnormalmatrix`, or for
     an indexed draw the transform of vertex 0's index, as view_bounds reads it (1046-1062).
   - **Only when `g_perView`:** group flats within 80 units AND with the same outOf; a set's identity is (print, outOf).
     The rooms overlap in world numbers, so two rooms' characters at the same place must not merge.
   - **Sets cap:** the array is sized 96; the cap is `g_perView ? 96 : 48`. With `g_perView` false, grouping, keys and cap
     are today's (the together case's found/unseen marks keep today's behaviour).
4. **A camera per view.** In the same block: for v = 0, 1, the first scene draw with no matrix index that is NOT
   `marked_out_of(.., v)` gives `g_cameraViewOffset[v]` (set where 3096 sets today's) and `g_sh.viewOf[v]`. With no marks,
   both equal today's draw.
5. **Fill per view.** Split `fill_shadow`'s body into `fill_one(Frame& f, int set, int view, const float V[3][4], uint32_t
   camOffset)`: casters with `outOf & (1 << view)` skipped (view -1: none); lamps, grid, cells and haze weights of `set`;
   slots matched and taken only among `slot.set == set`; the remembered-caster pass reads only that set's slots;
   `add_spell_lights(f, V, view)`. `g_slotClock` and `g_framesNoCharacters` stay once a frame in `fill_shadow`.
   - `g_perView` false: `fill_one(scene.shadow, 0, -1, g_sh.view, g_cameraUniformOffset)`: today's. Set-1 slots are NOT
     forgotten in such frames, so view 1's shadows go on when the rule returns.
   - `g_perView` true: `fill_one(scene.shadowView[0], 0, 0, viewOf[0], camOff[0])`, `fill_one(scene.shadowView[1], 1, 1,
     viewOf[1], camOff[1])`, `scene.perView = true`, `scene.shadow = scene.shadowView[0]`.
   - Each view gets its own 12 tiles, scored from its own casters. Haze weights are per set.
6. **The choice at render time.** `ssao::Scene` (ssao.hpp:19-24) gains `shadow::Frame shadowView[2]` and `bool perView`.
   `shadow::encoding_view()` returns `g_probe.player` while `g_probe.kind` is 1 or 2, else -1. `render_character_shadows`
   (common.cpp:2717) uses `perView && v >= 0 ? shadowView[v] : shadow`. at_this_moment then reads that Frame's own
   `cameraOffset`, with `frame.camera` that view's camera matrix: this fixes P4.

### P7. Spell lights per view, by the caster's room (host + renderer)
- **Renderer:** `dol_aurora_set_spell_light_views(const uint8_t* views, int n)`: bit v = shown in view v, default 3, stored
  next to `g_spell` (shadow.cpp:676-692). `add_spell_lights` skips a light whose mask lacks the view (view -1: all).
- **Host, the rune's room** (new field `g_runes[i].room`, set once when the rune is made in runes_frame, hle.c:9845-9903,
  where its spell record is known): the caster's room = `coop_object_room(r6_node_of_id(spell+12))` (spell +12 is the
  caster's id, as coop_spells_update compares it, hle.c:12773-12780); if that is -1, the spell's own room `spell+8` (the
  game's field, the one 0x80155450 tests, hle.c:12771-12773); if that is not a resident room either, today's stamp. Kept
  in the rune record because the spell record can end before the rune fades.
- **Host, the masks** (runes_frame's light loop, hle.c:10027-10052): while `lights_apart(cpu)`: rune room == room0 -> 1,
  == room1 -> 2, else 0. Otherwise 3. The mask no longer comes from the rune object's +592, which COOP_SPAWN_INIT stamps
  from the room in play (0x80040ED8-0x80040EDC) outside any turn (runes_frame is reached from coop_pad_read,
  hle.c:26778 -> 26816 -> 10083), so it is room0 for both players' runes [read; the turn bracket being separate: inferred].
- **P7b, the rune model's own room (read before building):** today the model gets room0 for player 2's runes too, so R3
  marks player 2's own rune models out of their own half apart [inferred from the stamp and hle.c:3301-3313]. With
  `g_settings.mod_coop && coop_stay_apart(cpu)`, rune_spawn passes the rune's room (above) to COOP_SPAWN_SET_ROOM
  (hle.c:9655-9657) instead of rec+44, as coop_weapon_copy sets a holder's room (hle.c:25048-25063). rune_spawn calls
  `0x8011F460(object)` only when the room is the room in play (hle.c:9659-9660); for a room1 rune it would run inside
  `coop_room_in(room1)` / `coop_stay_room_out`. **0x8011F460 was not read far enough** (it tests object +672, +60's +8 and
  `r13-23904`, and clears a +596 bit, 0x8011F488-0x8011F4EC [read]); build P7b only after reading it. P7 (the masks) does
  not depend on P7b.

### What the renderer pieces do NOT change
- The per-pixel lighting of a draw is the game's lights of that draw (lighting.cpp), now per room through G1-G3.
- The atlas and mask passes already run per view (common.cpp:2558-2562): no new GPU passes.
- The game's own shadow (Character shadows off) is already per room: flattened draws are marked, and the darkening quad
  goes per view's EFB alpha (render reader 10) [inferred].

## 3. The gate: solo and together exactly as today

| Path | Opens only when | Solo | Together (co-op, one room) |
|---|---|---|---|
| G1 lr 0x8002798C answer | LA, object in room1 | `coop_stay()` off (switches cleared with co-op off, frontend.cpp rooms_follow_coop, 5eb040d) and `mod_coop` tested | room1 == room0: never |
| G2 light bracket | LA, object in room1 | never | never |
| G3 section lights | LA and section 3 really drawn per view | never (section 3 not drawn) | never (hle.c:13730-13731 passes to the game) |
| H set A from the record | LA | never: today's path | never: today's path |
| H set B | players in different rooms, record loaded | never sent | sent off once at the edge, then never |
| R1 per view | `g_perView` (host: LA and B on) | off: `fill_one(..., -1, ...)` = today | off |
| R1 mark keying, sets cap 96 | `g_perView` | off (cap 48) | off: found/unseen marks as today |
| P7 masks | LA | 3 (all) | 3 |
| P7b rune room | co-op and coop_stay_apart | today's stamp | today's stamp |

Solo must still be checked in a run (J-S), because R1 moves today's fill body into `fill_one` (code motion).

## 4. Each player, each direction

After any door the shape is the same: player 1's room is the room in play, player 2's the other slot (R4, hle.c:13878-13884)
[read]. So every new path keys only on room1.

| Event | Player 1's half (view 0) | Player 2's half (view 1) |
|---|---|---|
| Apart, standing | A = room0's lamps; casters = room0's (marks keep them out of view 1); camera = the first draw kept in view 0 (P4 gone); G3 puts player 1's lights back before section 2 | B = room1's lamps; casters = player 2 and room1's enemies, lit by G1 (own grid) and G2 (own dynamic lights, player 2's torch); section 3 with room1's dynamic lights and light 7 over player 2 |
| Player 1's door, player 2 stays | inside the change (no draw); next frame A from the new current record: set-0 slots reset (today's room-change behaviour) | B unchanged: set-1 slots kept |
| Player 2's door, player 1 stays (ticket route) | the bracket and the unpack lie inside the change, before the draw (1.6): A is not re-sent, set-0 slots kept | next frame B = the new room once its record passes the loaded test: set-1 slots reset (fresh room) |
| Player 2's door (script route, if a bracket lasts over a draw) | H changes nothing while `g_stay_bracket` (no A re-send, no path switch) | the same; LA is false then, so the frame is one Frame with today's A, as R3 draws one room |
| A drawn frame with LA off while the players are in different rooms (split off, a scene shown whole, whole owner, a slot not yet known) | one Frame, set 0, today's: every caster in it, as R3 draws them all unmarked into one room's picture | the same Frame; set-1 data and slots kept for when LA returns |
| Player 1 enters player 2's room | together: B off, H gives A back; today's path re-sends A (it was B's grid): one set-0 reset at the door | one room, today |
| Player 2 enters player 1's room | together: B off; A unchanged (same grid) | one room, today |
| Player 2 casts (rune) apart | mask leaves it out (rune room = player 2's) [U3] | their rune's haze light |
| Player 1 casts apart | their rune's haze light | left out |
| Player 2's torch lit apart | not lit by it (its +42 = room1) | lights player 2's body, room1's actors and section 3 (today: nothing) |
| Player 1's torch lit apart | lights player 1's body, room0's actors and section 2, as today | no longer lights room1's actors |

## 5. Interactions

- **R1/R2 (slots, the stayer):** read only through `room_slot_record`, `room_slot_of`, `r6_room_loaded` and the loaded test
  of `room_light_grid`. A slot being unpacked (`g_room_slot_room[slot] = -1`) or a record marked dead fails those tests, so
  G1/G2/G3 are off and H keeps its sets.
- **R3 (the picture):** LA contains R3's test, so the lights follow the marks. G3 sits inside room_section_draw's per-view
  path, before each `room_mtx_load`. The marks are untouched and are what R1 reads.
- **R4 (doors):** section 4. `notify_ticket_run`'s word swaps never overlap a draw on the ticket route; H's bracket rule
  covers the script route.
- **R4b/R4c (scenes beside the other player, arrival during the other's scene):** a scene shown whole has the split off, so
  LA is false: no G1/G2/G3, one Frame (today's). A scene shown in the halves is lit per room like any frame apart.
- **R5 (sound):** coop_sound_gain swaps `r13-26524` around gain calls (hle.c:14170-14178), not reached from any light
  function.
- **R6/R6b/R6c (enemies):** room1's enemies are characters through 0x8002790C, so G1 gives them room1's grid. G2 has its own
  helper; r6c_creature (hle.c:24447-24497, with its conditional `if (!current) coop_room_in` and `g_r6c_p1 /
  g_r6c_depth`, read by r6_player1_node, hle.c:24188-24195) is left exactly as it is. r6c runs in the behaviour pass, never
  in the draw, so there is no nesting [inferred].
- **coop_found_object_draw** (notify on 0x8011A278, hle.c:3276-3320) runs before G2's bracket and reads the unswapped words.
  `coop_found_mark` marks only loads with lr inside 0x8011A278..0x8011B400 (hle.c:3322-3330), so a load inside the light
  functions is never marked [read]. This is why G2 brackets the light functions and not 0x8011A278.
- **hle_draw_characters** (an intercept on 0x8002790C itself, 1.2): G1 runs inside its nested call, the show flags set. G1
  reads only lr, r3, r30 and the slot state, none of which it changes [read].
- **The Tome copy (ED_COOP_TOME_COPY=2):** LA has `!g_tome_copy_turn`; room_slots_frame is skipped in the copy turn.
- **effect_draw_by_room** (hle.c:13757-13791): unrelated. Effects draw with whatever channel state the last actor left, as
  today [inferred].
- **The torch code** (coop_torch_*, hle.c:12609-12745): unchanged. The light it makes follows its holder's room by the
  game's own step (1.4), which is what G2/G3 count.

## 6. Save, load and room-change edges

- **A save-state load** (player 1's F-keys, ED_LOAD_STATE): `hle_coop_state_load` resets H's set-B record and
  `g_lights_own_a` (new co-op-only words; today's set-A pair is left as it is). The first frame after decides again: B is
  sent only from a record that passes the loaded test (1.1: a dead record in saves/rooms-ch1-183.state would otherwise pass),
  so a stale `g_room_slot_room` (not reset at a load, 1.5) cannot give B a dead room's lamps. The renderer's remembered
  slots carry over a load for both sets (as set A's do today) until a set changes or `kFlatMemory` frames pass.
- **Resetting `g_room_slot_room` on a state load** would close the stale-slot case for R2-R6c too. It touches every earlier
  item, so it is a separate decision, not part of this one (section 9).
- **In-game save while apart** (the user's later test): nothing here writes lasting guest state (every bracket restores its
  words in the same call; G3's reload puts back the light words). A save holds what the game holds [inferred].
  `hle_save_held` needs nothing new.
- **Load into together:** B goes off at the first frame.
- **A slot swap** (0x80154C50 XORs table +0/+4): every read goes through the table each frame.
- **`g_framesNoCharacters > 8`** (a long load) forgets every slot of both sets. Both come back fresh, as today.

## 7. Alternatives and why not

| Alternative | Why not |
|---|---|
| Keep reading 0x804F0784 for set A while apart | With G1 its first entry can be a room1 object; the span rule then keeps A stale after player 1's next door (1.5). |
| Decide H in room_lights_update | It runs on every 0x801E4BDC call, one inside the actor list (1.5): B could change between one frame's draws. |
| Lamps by the grid that contains a caster's world place | The rooms overlap in world numbers (341: x -1170..1158; 342: x -247..306, `ed lights`). Marks, not places. |
| Lamps from the draws' lights | coop-failed-attempts.md:352 (view-space places taken during camera sweeps). Not proposed. |
| A bracket around the whole 0x8011A278 | It re-runs coop_found_object_draw under room1 (wrong marks), and brackets the budget, hold-flag and room-specific tests (1.2). |
| Swapping the slot table during the registration walk | The walk registers both rooms' characters in one pass. Only per-object answers can be right for both. |
| One Frame with view masks per tile | It keeps one 12-tile budget scored from player 1's camera. Two Frames cost the same on the GPU. |
| Drawing the world twice per frame, once per room | Far more cost and risk. The marks already split one draw into two halves. |
| Sharing r6c_creature's word swap with G2/G3 | r6c is a committed, played fix with a conditional swap and its own words; a shared helper would change it or blur it (impl refuter 7). |
| Re-stamping player 2's torch light's +42 by the host | The game's own step 0x80145AF8 already sets it to the holder's room (1.4). |
| Holding per-view on through a drawn frame with LA off (goal refuter P-D) | In such a frame R3 draws one room in both halves; one Frame is what that picture shows. Set-1 data and slots are held instead. |
| Lighting player 2's body as "the player" (budget 4) apart only | Their body would change look at every door (goal refuter P-H). The user's decision, together and apart (section 8). |

## 8. Not in this build (named, with what was read), and the user's decisions

- **L3, the user's decision: player 2 lit as "the player".** Today, together and apart, only `r13-26524` (player 1) gets
  the player budget (room lamps 5-7, 0x8011A3F8-0x8011A408; r18 == 1 gives r19 = 0 at 0x801E652C) and the room's light 7
  above them (0x801F2D28, loaded at 0x801F2DC0) [read]. ONE question for the user: in co-op, should both players' bodies get
  the player budget, together and apart (a co-op-only change that changes the together case), or neither (as this build)?
  An apart-only version is not built. G2 could pass `gpr[7] = 4` at 0x801E6244's entry for player 2's object if the
  answer is yes (r25 from the call at 0x8011A570) [read]. Note: G3 gives room1's walls light 7 above player 2, the game's
  one-player rule applied to the room they are alone in; together, the one light 7 stays over player 1 as today.
- **L5, lightning:** `r13-22136` is one byte driven by the current record (0x800E2BB0). Player 2's room flashes with player
  1's room's lightning, and its own never runs [read: game reader 1.6]. A later item using the per-room swap pattern. No
  lightning room was found in the states read (+142 = 0 in all), so its check needs one.
- **The 12 shared dynamic-light records (1.4):** one room's light can take another's record. Measured by U4; a fix would
  be its own item.
- **Whole-owner frames** (`g_room_whole_owner != 0`: player 2's Tome copy at hle.c:14762, the shared Tome's own frames at
  hle.c:15137): LA is false, so the game lights them as today. In owner-2 frames room1's section is drawn whole with room0's
  live lights (the game's single load) [read hle.c:13702-13712]. Actors and walls are then lit the same way, so there is no
  mixed lighting. Running G3's bracket before section 3 there (goal refuter P-G) is a later, separate step: those frames run
  no renderer effects (fill_shadow needs enough 3D draws, 1.7), and the copy turn's frames run in the copy's memory.
- `0x801E5D34`'s per-section COLOR1 test (`r13-23276 == 50`, `r13-21780` of the last-built record) is not bracketed for
  section 3 [read: game reader 1.5]. Only room 50 and the specular groups.
- The actor draw's room-specific tests in 0x8011BE00 (rooms 83/99/213, 145/311/334) see room0 for room1's actors (1.2). They
  are not lights.
- Specular in view 1 is computed for player 1's eye (light places in the game camera's view space; the replay only
  reprojects). Diffuse is right in both views [inferred].
- P7b waits on the read of 0x8011F460 (2.P7).

## 9. Risks

1. **The 0x80155130 intercept** runs on every call of a busy getter (75 callers). The lr test fails cheaply. U6 measures
   calls per frame. The fallback is the 0x80119B94 intercept (G1).
2. **Player words swapped inside a draw** (G2/G3). No hook on the functions themselves; the two callee intercepts
   (0x801F62B0 coop_interface_player, 0x80118274 coop_position_copy) are lr-gated to other sites (1.2). A future hook on any
   of these must tolerate player 2 as "the player".
3. **G3's extra 0x801F2CB4 runs** write the words of 1.3; section 2's reload (only after G3 ran) puts each back to the
   game's value, except the write-only `r13-26884`.
4. **Marks missing on some caster draw** (the skin-matrix caveat): such a draw has outOf 0 and counts in both views. U7.
5. **Caps:** two rooms' casters share `groups[32]`; sets go to 96 only while per view. U8.
6. **P4 fixed by construction.** If it was real, player 1's shadows and a clear haze return apart: a visible, intended
   change in view 0.
7. **Code motion in fill_shadow** could change solo. J-S and J-T compare with today.
8. **`table[0]` for the lamps** where the actors use `table[r13-22136]`: in a lightning room the shadows keep the base set
   during a flash, and set A is re-sent once at the edge back to together (U2).
9. **The 12 shared dynamic-light records** (1.4, section 8): player 2's torch or a room light can be missing for that
   reason, whatever G2/G3 do. U4 names every eviction.
10. **The script route of player 2's change** (0x800AE000): whether its bracket can last over a draw was not read; H
    freezes its sets while the bracket is open.
11. **Player 2's torch's step:** 0x80145AF8 runs with the node walk [inferred], and the light's event handler 0x80145994
    tests the MAIN player's (r13-21484) room against the room in play before 0x801F2858 (1.4), which event was not read.
    U4 shows each record's room and active flag, so a torch light that goes out apart shows.
12. **`g_room_slot_room` is not reset on a state load** (1.5): H is safe through its loaded test, but R2-R6c read it too.
    A separate decision for the user.

## 10. Unknowns only a run settles, and the log line to add

| # | Unknown | Log line (new) |
|---|---|---|
| U1 | Is P4 real today, and gone after? | `AURORA_SHADOW_PROBE` file + `ed flicker FILE`. Renderer, with AURORA_SHADOW_LOG: `[shadow-view] frame F view V casters N tiles T vol H set S lamps K camera O (whole O0, first draw marked 0x..) unmarked U` |
| U2 | Entries of record+112 and what `r13-22136` selects | host, at each A/B send: `[lights] set A/B: room R slot S grid 0x.. (N lamps, X x Y x Z) table 0x.. [0] 0x.. [1] 0x.. [2] 0x.. light set L (frame F)`; and `[lights] set A/B kept: room R record not loaded (frame F)` once per stretch |
| U3 | The rune's room: caster's, spell +8, model +592 | `[runes] rune i spell 0x.. caster 0x.. (player P) caster room R spell room S model room M views V (frame F)` at each rune made and each mask change |
| U4 | Each dynamic light's room, and evictions | every 120 frames apart (with `[stay]`): `[lights] dynamic i room R type T flags 0xFF active A holder 0x.. (frame F)` for each of the 12; per room, the active count; and, from a notify on 0x801F1B40 while all 12 are active: `[lights] a new light (type T, room R) takes record i from type T2 room R2 (frame F)` |
| U5 | Does 0x8002790C ever run in player 2's turn | in G1 at lr 0x8002798C when `g_coop_turn`: `[lights] grid asked in player 2's turn (frame F)` (once) |
| U6 | The getter's cost and the brackets' use | `[lights] frame F: 0x80155130 calls N, answered for room R1 M; actors lit as room R1 K; section lights S` every 120 frames apart (counted inside the nested character call) |
| U7 | Casters with no mark while per view | in `[shadow-view]`: `unmarked U` |
| U8 | Caps reached | `[shadow] sets full (96) / casters full (32) frame F` once a second at most |
| U9 | One decision a frame | at each edge of LA, of B and of per-view: `[lights] per view on/off at section 3 (frame F)`; the renderer: `[shadow-view] per view taken at frame start F` |

## 11. Scripted checks, one build, fewer launches

Build: `cmd //c "$(cygpath -w tools/ed/build-win.bat)"`, then `git -C .deps/GXRuntime diff >
patches/gxruntime/eternal-darkness.patch`. One run at a time, never with the user's game open (ED_RUN_FRAMES closes every
EternalDarkness.exe). Exact frames are set from `ed timeline` of the before run (B-0), whose script is the same as C-1's.

```sh
cd "/e/Eternal Darkness Recomp Fork"
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_SHOW_TRACE=1 ED_FIN_LOG=1 ED_HEAP_CHECK=2000 \
       ED_RUN_SHOTS_MS=200 AURORA_SHADOW_LOG=30 ED_RUNES_3D=1 ED_RUNES_LOG=1 ED_COOP_CAST_LOG=1
# B-0 (on TODAY's build, before building): C-1's script. Gives the view-0 pictures, player 2's [shadow-lights] colours
# and the probe file to compare with.
# C-1 apart in chapter 1 from Jump to Game, one launch: player 1 by door 103 (341 -> 342, twice, 60 apart); player 2 stays
# in 341; both put where the rooms overlap (the user's spots, run-20261111-222138 frame 2300); both walk; player 2 casts
# (D-pad up, ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60, `up` in their script); player 1 casts (ED_QUICK_SET slot 0,
# 0x00010481, then their D-pad up); player 2 opens and closes the Tome (start); player 2 examines a thing in 341 (a
# close-up: `ed room 341` for the spot); player 1 hits (B) in 342; then player 1 back by door 9 (342 -> 341): together.
JTG_SLOT=1 AURORA_SHADOW_PROBE=logs/probe-lights-apart.txt ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 \
  ED_QUICK_SET=2700,0,0x00010481 ED_RUN_SCRIPT=1950:103,2010:103,3600:9,3660:9 \
  JTG_PUT="400,1,33,-1565,0;400,2,-245,-90,16" JTG_P1_SCRIPT="..." JTG_P2_SCRIPT="..." JTG_FRAMES=2000 \
  sh tools/ed/jtg.sh 1 lights-apart </dev/null
# C-2 together in 341, no door; both walk.
JTG_SLOT=1 AURORA_SHADOW_PROBE=logs/probe-lights-together.txt JTG_P1_SCRIPT="100,280,0,0,127" JTG_P2_SCRIPT="100,280,lup" \
  JTG_FRAMES=600 sh tools/ed/jtg.sh 1 lights-together </dev/null
# C-3 solo.
JTG_SLOT=1 JTG_SET="mod_coop=0" AURORA_SHADOW_PROBE=logs/probe-lights-solo.txt JTG_P1_SCRIPT="100,280,0,0,127" \
  JTG_FRAMES=600 sh tools/ed/jtg.sh 1 lights-solo </dev/null
# C-4 enemies and player 2's door, one Jump-to-Game launch with ED_COOP_STAY on throughout: player 1 by 101, 105, 111, 29
# to 183, player 2 put at each door after player 1 and taking it by their own press (positions from `ed door ROOM`),
# so both arrive in 183 together; then player 1 to 180 by 119 (player 2 stays among 183's zombies, a hit by player 2);
# then player 2's own door 183 -> 180 and back (player 1 stays in 180, fighting if a zombie followed).
```

If C-4's chain fails (a door press of player 2's not taken), the fallback is the state made fresh by S0 on this same
build in the same session (the R6 and arrival-camera method), and the user is told, since their rule is fresh states only.

For every run: `ed timeline RUN`, `ed check RUN`, EVENTS.log, both.tsv (p1room/p2room, E and p2e), the shots (left half =
player 1, right half = player 2). Then `ed test` as CLAUDE.md asks, read against an `ed test` of the build before.

**C-1 (apart), against B-0:**
- Page log, in order: `[stay] ... player 2 stays in room 341 ... the room in play is 342`; `[lights] set A: room 342 ...
  (9 lamps ...)` and `[lights] set B: room 341 ... (18 lamps ...)`, each grid inside its slot's span, `[0]` = the grid
  sent (U2); `[lights] per view on at section 3` (U9) the same frame or the next; no `[room-lights] grid` line while apart.
- Every 120 frames: `answered for room 341 M >= 1; actors lit as room 341 K >= 1; section lights S > 0` (U6), no U5 line.
- `[shadow-view]`: view 0 set 0 lamps 9, tiles > 0 near a lamp, camera offset != the whole one; view 1 set 1 lamps 18,
  tiles > 0; unmarked 0 (U7).
- **G1 on the body:** `[shadow-lights]` for player 2 (coop-failed-attempts.md:261): in B-0 their fills are 342's amber
  (226,169,47); in C-1 they are 341's, with lamp 0's blue-grey (92,107,124) among them. Player 1's lines equal B-0's.
- **G2/G3:** U4 lists each dynamic light with its room. If 341 or 342 has none, say so: G2/G3 are then unproven here and
  the torch play check (below) is the proof.
- **View 0 against B-0:** the left half's pictures equal B-0's except P4 (U1: if B-0's probe shows view 0 with 0 tiles
  apart, C-1's shows tiles) and the shadows of room1's characters, which must be gone from the left half.
- **Spells:** U3: player 2's rune has caster room 341 and views 2; player 1's 342 and 1. The right half shows player 2's
  rune light, the left half player 1's, neither the other's.
- **Tome (whole-owner frames):** while player 2's Tome is up, no G1/G2/G3 answers in those frames (LA false), no B change;
  after it closes, per view back on with no set-1 reset (B unchanged).
- **Close-up by player 2 (lent camera):** per view stays on if the split stays on; the right half keeps 341's lamps.
- **Hit by player 1 in 342:** no change in view 1's casters.
- **Reunite (door 9):** `[lights] set B off` once; per view off; `[room-lights] grid ... 18 lamps` once (today's path takes A
  back); no `[shadow-view]` after; both.tsv p1room = p2room = 341.
- `ed flicker logs/probe-lights-apart.txt`: 0 spikes in both views. `ed check`: nothing new.

**C-2 (together):** no `[lights]`, no `[shadow-view]`, no set B; `[room-lights] grid` as before this build; `ed flicker` 0
spikes; pictures as a together run of the build before.

**C-3 (solo):** no `[stay]`, `[lights]`, `[rooms] two room slots`, `[shadow-view]` or set B; `[room-lights]` and `[shadow]`
lines of the same form as a solo run before; `ed flicker` 0 spikes.

**C-4 (enemies, player 2's door):**
- Apart in 183: `answered for room 183 M`, M at least player 2 plus the zombies in view; `[shadow] caster ... set 1` for
  each; the right half's zombies' and player 2's shadows from 183's lamps (365,325,546 ...); the left half (180) shows
  none of 183's casters.
- Player 2's door 183 -> 180: the frames of the change have no both.tsv rows (as 1.6); in the drawn frames before and
  after, the LEFT half keeps player 1's shadows (pictures: their tiles in `[shadow-view] view 0` never drop to 0 because of
  the door), set A not re-sent (no `[lights] set A` line), then `[lights] set B: room 180`. If a `[lights] set B kept:
  record not loaded` line shows, it names the frames, and the left half's pictures there must still show player 1's
  shadows. Back: B = 183 again.
- No CAMERA findings beyond arrival-camera's known ones.

**For the user's play afterwards (not scriptable: no switch lights a torch):**
- First: player 2 lights a torch in their room while apart. Expected: it lights their walls and body, never player 1's
  half; U4 shows its room = player 2's. Then player 1's torch the other way.
- A rune cast by each player apart.
- In a lightning room, whether each half flashes (L5 is not done).
- Together: player 2's body against player 1's under the same lamp (L3, their decision).

## 12. Refuted or corrected

Impl refuter (`refute-lights-impl.md`):
1. **(major) Set H read from a record with no loaded test.** Real (hle.c:13505-13509, 13627-13636; no reset in
   hle_coop_state_load). Fixed: `room_light_grid` requires the record's room and loaded bytes; both sets are kept as last
   sent while it fails (2.H). Also corrected: g_room_slot_room has a third writer, hle.c:14304. The reset on load stays a
   separate decision (6, risk 12).
2. **(major) room_lights_update is not once a frame.** Real: 9 bl sites of 0x801E4BDC, one inside the actor list via
   0x80119FB0 (disasm.txt). Fixed: H decides once a frame in room_slots_frame (2.0), and the renderer latches per view at
   its frame start (R1.2). Fact 1.5 corrected.
3. **(major) G1/G2/B on Apart alone, without the split.** Real (hle.c:3300, 13698). Fixed: the one predicate LA (2.0) with
   the split, no whole owner, co-op on, no copy turn and the loaded test; a whole-scene and a Tome case in C-1.
4. **(major) G2 could take player 2's torch light away.** Checked and not so: the torch record's +104 makes the light
   node's step 0x80145AF8, which sets +42 to the holder's room through 0x801F1F90 (0x800793CC, 0x8014594C-0x80145954,
   0x80145B7C-0x80145B98). So the light carries player 2's room and today lights nothing apart; G2/G3 make it light their
   side. No host re-stamp needed (1.4, 7). U4 confirms the step runs (risk 11).
5. **(minor) G3 writes more words; section 2's reload without section 3.** Real. Fact 1.3 corrected; the reload runs only
   after G3 ran (`g_g3_ran`); `!g_tome_copy_turn` is in LA. The leftover XF lights in prints are noted (G3).
6. **(minor) hle_draw_characters is on 0x8002790C itself.** Real (0x8002794C-0x80027954, hle.c:6436-6465). Corrected in
   1.2 and 5; U6 counts inside the nested call.
7. **(minor) Don't factor r6c_creature's swap.** Agreed: G2/G3 have their own `room_draw_in/out`; r6c unchanged.
8. **(minor) The rune mask from the model's +592.** Real (COOP_SPAWN_INIT stamps the room in play, 0x80040ED8). Fixed: the
   mask comes from the rune's room, the caster's (spell +12) or spell +8 (P7).
9. **(minor) Player 2's door edges, statics over loads.** The bracket never spans a draw on the ticket route (1.6), so the
   path switch it feared does not happen there; for the script route H changes nothing while the bracket is open, and the
   sent pair is shared so the edge back to together doesn't re-send the same grid. Section 6 now states that remembered
   slots of both sets carry over a load, and H's B record is reset at a load.

Goal refuter (`refute-lights-goal.md`):
- **P-A (major) spell masks follow player 1's room.** Real; fixed as impl 8, with the caster's room first and U3 logging
  the caster. The rune model's own room (P7b) is in the design, built only after 0x8011F460 is read.
- **P-B (major) 12 shared dynamic-light records.** Real (0x801F1B40). Not caused by this design; stated as a limit (1.4, 8,
  risk 9) and measured by U4 with an eviction line.
- **P-C (major) no loaded test.** Real; fixed as impl 1.
- **P-D (major) player 2's door story.** Half right: no draw sees the bracket on the ticket route (frame order read). But
  the unpack frames are not drawn either: in every run read, unpack and tail lie inside the change, with no both.tsv rows
  (1.6). So no drawn frame changes player 1's half there. Section 4 and C-4 restated; set-1 data and slots are held
  through any frame with LA off; holding per-view itself was not adopted (7). The user's "brief pause" is the change's
  66 ms with no draw: the change-freeze item.
- **P-E (major) the checks prove too little.** Fixed: a before run B-0; `[shadow-lights]` colour criteria for player 2; a
  view-0 comparison; spells, Tome, close-up and a hit in C-1; C-4 with pictures for player 2's door; launches cut from
  seven to five. A torch can't be lit by a switch (none found among hle.c's ED_ switches), so the torch before/after is
  the user's first play check, made self-checking by U4.
- **P-F (minor) the 96 cap changes solo.** Real; the cap is `g_perView ? 96 : 48` (R1.3).
- **P-G (minor) whole-owner frames mixed.** With LA false in whole frames, actors and walls are both lit as today, so the
  mix doesn't arise. G3 in that branch is deferred and named (8).
- **P-H (minor) one-player lighting together.** Agreed: no apart-only L3; one decision for the user (8).
- **P-I (minor) S-E/S-D keyed to a state.** Changed: C-4 is one Jump-to-Game launch with both players taking the doors
  (stay on throughout, since `coop_stay()` reads its switch once); the same-build state is only the fallback, said to the
  user.
- **Facts corrected:** 1.3 (0x801F2CB4's writes), 1.2 (the two callee intercepts), and reader question 2 answered (1.1:
  [1] and [2] count 0 outside lightning).
