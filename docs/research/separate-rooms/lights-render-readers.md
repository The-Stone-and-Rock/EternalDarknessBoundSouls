# Lights and shadows per room: the host and renderer side (reader, 2026-11-12)

Item: with the players apart (ED_ROOM_SLOTS=2, ED_COOP_STAY=1, Mods > Co-op on), each half should be lit, hazed and
shadowed by its own room. This reader covers the host (`room_lights_update`) and the renderer (`set_room_lights`,
`submit_draw_plan`'s shadow collection, `fill_shadow`, `gfx/shadow.cpp`, the per-view replay in `gfx/interp.cpp` and
`gfx/common.cpp`, `lighting.cpp`). Nothing was edited, built or launched. Facts are marked [read] (seen in code, disassembly
or a log, with the place) or [inferred].

Players and views: view 0 = player 1's half, view 1 = player 2's half. `marked_out_of(m, v)` is true when a matrix carries
the bytes D2 / A0+v / E5 (A2 = out of both). Apart, "the room in play" (r13-23276, slot table +0) is player 1's room outside
player 2's turn (hle.c:13838-13843 comment; coop_stay_apart hle.c:13832-13837) [read].

## Short answers

1. **Today both halves get ONE lamp set: the room in play's (player 1's).** `room_lights_update` accepts a grid only inside
   the current slot's span and sends it once per change; the renderer keeps one `g_lamps`/`g_gridCells` [read]. Seen in the
   user's runs: apart from frame 1844, the only grid sent is room 342's (player 1's, slot 0, `0x807CEA6C`, 9 lamps); room
   341's (player 2's, slot 1, `0x8307EC28`, 18 lamps) is never re-sent while apart
   (logs/run-20261111-221141-rooms-play, run-20261111-222138-rooms-play stderr: `[room-lights]` vs `[stay]` lines) [read].
2. **The marks already keep a room's casters out of the other half's shadow atlas**, because the casters are drawn into the
   atlas per view with that view's (zeroed) uniforms [read]. What is wrong is (a) which LAMPS player 2's room's casters are
   given, (b) the haze lamps in player 2's half, (c) the 12-tile budget shared and scored from one camera, and (d) very
   likely, player 1's half losing its shadows entirely and gaining a flat haze veil when apart, from the shadow camera
   offset pointing at a draw that is zeroed in view 0 (P4 below) [inferred from code; one probe settles it].
3. **Per-view lights need:** two lamp sets (A: the room in play's, B: player 2's room's when apart), each caster's room told
   by its draws' marks (not by world position: rooms overlap), and a `shadow::Frame` per view (or per-tile/per-light view
   masks) chosen at the per-view replay. `fill_shadow` can run per view with casters filtered by the same marks R3 writes;
   the marks are readable at `submit_draw_plan` time from the game's own uniforms [read].
4. **Cost:** the GPU already runs the shadow and haze passes once per view replay (common.cpp:2558-2562 inside
   `render()`, called by `encode_between` for each Split/SplitBetween output) [read]. Per-view Frames change only the data
   fed to those passes; the CPU work of `fill_shadow` (≤32 casters x ≤96 lamps) doubles [inferred: negligible against the
   60 fps cap, settings.c:349 `dol_aurora_set_frame_rate(g_settings.mod_coop ? 60 : ...)`, read].
5. **Solo and together stay exactly as today** if every new path is keyed on "lamp set B present", which the host sends
   only while `coop_stay_apart(cpu) && camera_split_on()`, and clears otherwise; with no set B the renderer runs today's
   code unchanged (one Frame, set A, `g_cameraUniformOffset`).
6. A game-side companion (not renderer): every character, both players' and both rooms', is registered for drawing with the
   CURRENT slot's light grid (0x8002790C at 0x80027984-0x800279D8), so player 2 and player 2's room's enemies get their
   per-draw GX lights (the colour and direction of their lighting, and the "lights fingerprint" the renderer's shadows key
   on) from player 1's room's grid [read]. That is the "lights" half of the user's report for the characters themselves.

## 1. The host today: room_lights_update (runtime/host/hle.c:6087-6189)

- Called once a game frame from `notify_look_at` (hle.c:6219-6234), gated on `cpu->lr == g_camera_look_at_return`
  (the C_MTXLookAt inside the game camera 0x801E9728) [read]. 0x801E9728 is called by the world draw 0x800080E4 at
  0x80008128, after the room draw 0x801E851C (0x80008118) and before the layer walk 0x8014FB6C (0x8000812C) and the object
  list 0x80119D60 (0x80008148) [read: `ed dis 0x80008100 24`]. So it runs outside any player's turn: player 1's room is
  current (slot table +0, r13-23276) [read for the frame order; the turn is a separate host call, hle.c:20660-20720].
  During player 2's turn nothing calls it (the turn runs the pad, scan, threat look and controller, hle.c:20664-20720;
  coop_stay_room_in swaps r13-23276 and the slot table only inside it) [read].
- Reads `LIGHT_GRID_POINTER 0x804F0784` (hle.c:6093, 6108). 0x804F0780 is the per-frame draw list (16 bytes an entry:
  object, grid, r5, distance^2; filled by 0x80119B94, count r13-23956) and 0x804F0784 is entry 0's grid [read:
  `ed dis 0x80119B94` stw r29/r30/r31 at +0/+4/+8, 0x80119C14 stfs +12; refute.md:111]. So the pointer is "the first
  registered object's grid", not a room's [read].
- Two slots (`room_slots() == 2`, hle.c:6125-6131): the grid is accepted only if it lies inside the current slot's span
  (base `mem_read32(r13 - 23304 + 4*cur)`, span `ROOM_SLOT_BYTES 2916384`); else return (keep the last) [read]. One slot:
  the old "near a door the lower address is the room's" rule (hle.c:6132-6137) [read].
- Sends on a change of grid or of its mark (first lamp ^ second word ^ grid+8): up to 96 lamps (place, colour, power) and
  each cell's three lamp indices, then `dol_aurora_set_room_lights` (hle.c:6139-6184) [read].
- Per player: reads no player word; it reads the slot table +0 (player 1's slot outside the turn) and the first draw-list
  entry. Player 2's room's grid is never read [read].
- `dol_aurora_set_room_lights` is a `gxq::call_sync` into the GX stream (aurora_backend.cpp:3209-3219), so it lands with
  the draws of the frame it was read in [read].

## 2. The renderer today

### 2.1 set_room_lights (gxcore_draw.cpp:1543-1571)
One lamp set: `g_lamps[96]` (world place, colour, reach = sqrt(625 power), haze = 1.75 sqrt(1.875 power), weight),
`g_gridOrigin/Size/Dims`, `g_gridCells` [read, 1025-1038]. A new set forgets every caster slot (`slot.used = false`) and
sets `g_lampsFresh` [read, 1567-1570].

### 2.2 What submit_draw_plan collects per game frame (gxcore_draw.cpp:2124-2213, 3095-3103)
- `g_sh.view` and `g_cameraUniformOffset`: the frame's FIRST scene3d draw with `has_pos_mtx_idx == 0` (2133-2136, 3095) [read].
- `g_sh.groups`: each flattened draw (persp, colour off, alpha on, unlit, untextured, ≥24 vertices), view-space bounds,
  merged within 80 units (2148-2172); with our shadows on these draws are dropped (2173) and so is the game's darkening quad
  (2178) [read].
- `g_sh.sets`: each distinct lot of lights (FNV print of the draw's 8 XF lights) on a lit perspective scene draw, with its
  bounds (2184-2212); the draw keeps `shadowLights = print` for the atlas pass [read].
- `g_sceneUniformOffset/SharedOffset`: the scene's last 3D draw, whose projection the shade pass inverts (3100-3103) [read].
- None of this looks at the marks today [read].

### 2.3 fill_shadow (gxcore_draw.cpp:1111-1541), once per game frame
Run at the display copy inside the game's own frame (1722-1723) [read]. All places are in the game camera's view space `V`.
Casters = flat groups + remembered slots (`kFlatMemory` 30); each lit set no bigger than a person goes to the caster whose
place is inside it (1198-1231). Slots matched by world place within 150 (1285-1307). Each caster's lamps: the three of the
grid cell at its world place, **clamped into the one grid** (1333-1339), then the two strongest by reach (1341-1381). Then
the 12 tiles (`kMaxTiles`) chosen across ALL casters, scored by their distance from the game camera (1436-1481). Haze: the
12 lamps of `g_lamps` nearest any caster, easing (1500-1539), plus spell lights (1095-1109, 1540) [read].

### 2.4 The per-view replay (interp.cpp, common.cpp)
- The game's frame is recorded once (note_uniform/note_draw, interp.cpp:175-258). For each view, `split_build` /
  `split_between` -> `build_view` copies the uniforms, rewrites every perspective projection through `photo::projection`
  (P x D, D = the player's camera relative to the game camera's view space, common.cpp:1784-1810), and zeroes the three
  position rows of any Vs block or XF matrix `marked_out_of` that view (interp.cpp:704-719), reading marks from the frame as
  drawn [read]. **The view matrix (posnormalmatrix) stays the game camera's for both views**; only the projection differs
  [read]. So one set of view-space tiles serves both views geometrically.
- Each Split / SplitBetween output writes its uniforms, `probe_label(..., output.view, ..., uniforms)`,
  `ssao::set_view_shape`, then `encode_between`, which replays every pass via `render(..., replay=true)`; a pass with
  `glowScene` runs `ssao::apply`, `render_character_shadows`, `post::apply` (common.cpp:2163-2214, 2458-2489, 2558-2562)
  [read]. So shadows and haze already run once per view per glowScene pass. The Real frame is encoded with the game's own
  uniforms [read, common.cpp:2240-2251].
- `render_character_shadows` (common.cpp:2715-2774): `frame = shadow::at_this_moment(passInfo.ssaoScene.shadow)`; the
  atlas pass draws, for each tile, every draw whose `shadowLights` is one of the tile's prints, with the CURRENTLY BOUND
  view uniforms (`render_shadow_caster` binds `data.uniformRange`, gxcore_draw.cpp:902-907; the caster shader uses
  `vsc.posnormal`, shadow.cpp:42-50) [read]. A caster whose matrices are zeroed in this view collapses to a point: its atlas
  tile stays cleared, and `tile_shadow`, `is_caster` and the haze's `open` all read "nothing" (shadow.cpp:133-188, 286-296)
  [read]. **So a room's casters already cast nothing in the other half** (for draws that carry the mark).
- `at_this_moment` (shadow.cpp:842-878): reads the view's Vs at `frame.cameraOffset` (`g_probe.uniforms`, set by
  `probe_label` for every Split/SplitBetween output, cleared after submit) and, if it differs from `frame.camera`, moves
  every tile centre, lamp, floor and haze light "back to the world, then forward by the moment's camera" [read].

### 2.5 Per-pixel lighting and skin (gxcore/src/lighting.cpp)
`emit_light`, `emit_skin_band` and `per_pixel_channels` use only the draw's own XF lights (`vxf.lights[i]`), never the room
lamps (lighting.cpp:23-82, 157-176, 178-193) [read]. Per-draw lighting is therefore whatever the GAME loaded for that draw:
per room exactly when the game gave that draw its own room's lights.

### 2.6 What the game gives each draw (the game side, read for this item)
- Room geometry: each section's draw 0x801E75B4 builds its lights from its own record (0x801E53FC(record, record+28, 0):
  GXInitLight*/GXLoadLightObjImm/GXSetChanCtrl) [read in picture-readers.md:42]. So each room's walls are lit by their own
  room already; the room is mostly baked (graphics.md:27) [read].
- Characters: the character draw-registration step 0x8002790C (a drawable's +24, called by the layer walk 0x8014FB6C at
  0x8014FBCC-0x8014FBE4; hle.c:18148) does `r21 = 0x80155130(2)` (the CURRENT slot's record) at 0x80027984-0x8002798C,
  `list = record+112`, `grid = list[byte r13-22136]` (or `list[0]` when that grid's +4 count is 0), then
  `0x80119B94(object, grid, 3000)` (0x80027998-0x800279D8) [read: `ed dis 0x80027960 40`]. 0x80119D60 passes the entry's
  grid to 0x8011A278 (r4 -> r16), which hands it to the lighting 0x801E6244 (r5 -> r17; 0x8011A564-0x8011A57C), which clamps
  the place to the grid's lower corner (+8/+12/+16, 0x801E62D0-0x801E631C), looks up the cell (0x801E5DDC) and sets the GX
  lights with 0x801E54BC [read]. Since the layer walk runs outside the turn, **every character, player 2 and player 2's
  room's enemies included, is lit from player 1's room's grid** [inferred from the call order; no host hook wraps
  0x8002790C or 0x8014FB6C: grep of hle.c shows only 0x8002790C as a +24 value to match, 18148/18201/24995].
- r13-22136 is one global byte (written at 0x800E2884..0x800E3074 and 0x801E83A8) [read: disasm.txt grep], so both rooms
  would share one "light variant" index [read]; what it means is not read.

### 2.7 Spell lights
`runes_frame` sends rune lights in world space (hle.c:10019-10052) via `shadow::set_spell_lights` (not in the stream); they
are added to every Frame (gxcore_draw.cpp:1503, 1540) with no room [read]. Whose rune and which room is not carried.

## 3. Both players, function by function

| Function | Runs when | Reads | Player 1's turn / outside | Player 2's turn |
|---|---|---|---|---|
| room_lights_update (hle.c:6099) | once a frame, the world draw's camera | 0x804F0784 (draw-list entry 0's grid), slot table +0, r13-23304 bases, r13+g_coop_room_offset | the room in play's grid = player 1's | never called |
| 0x8002790C registration | layer walk, outside turns | 0x80155130(2) = slot table +0's record, record+112, r13-22136 | every object gets player 1's room's grid | never inside player 2's turn [inferred] |
| 0x801E75B4 room sections | room draw, outside turns | its record (section 3 the other slot, 2 the current) | each section its own room's lights | n/a |
| submit_draw_plan collection | render thread, per draw | plan uniforms (marks available, unread) | both rooms' casters in one list | same |
| fill_shadow | once per game frame | g_lamps (set A only), g_gridCells (A), g_sh | player 1's casters: right lamps | player 2's casters: lamps of A's grid clamped |
| render_character_shadows | per view per glowScene pass | the view's uniforms | view 0: see P4 | view 1: player 2's room's casters drawn, player 1's collapse |
| at_this_moment | per view | Vs at cameraOffset in the view's uniforms | view 0: that block is zeroed when apart (P4) | view 1: fine |

## 4. Problems found

- **P1 One lamp set.** Above, section 1 and 2.1 [read; logs as in short answer 1].
- **P2 Player 2's room's casters get player 1's room's lamps.** Their world place is clamped into A's grid (1336) and they
  cast from A's lamps (1344-1381) [read]. Rooms share world space: room 341 (x -1170..1158, y -1074..1179) and 342
  (x -247..306, y -2344..1972) overlap (`ed lights 341`, `ed lights 342`) [read], so player 2 at (-245,-90,16) in 341
  (run-20261111-221141 `[stay]` frame 2204) stands inside 342's grid and casts from 342's lamps such as (-247,393,315)
  [inferred]. Also why lamps cannot be picked by world position (refute.md:136) [read].
- **P3 Haze in player 2's half from player 1's room's lamps, none from their own** (1507-1539 use g_lamps = A; the vol
  pass adds them per pixel in whichever view, shadow.cpp:266-303) [read]. With overlapping rooms they glow at places inside
  player 2's room [inferred].
- **P4 Player 1's half probably loses all character shadows when apart and gets a flat haze veil.** Apart, the room's first
  draw of the frame is section 3 (player 2's room), drawn with the marked camera copy `room_mtx_load(cpu, 0)` = 0xA0, out
  of view 0 (hle.c:13714-13728; order 0x801E851C: section 3 then 2, picture-readers.md "Frame order") [read]. The room's
  vertices carry no matrix index (hle.c:13663) [read], so this draw is the camera draw: `g_cameraUniformOffset` points at a
  Vs block that build_view zeroes in view 0 (interp.cpp:710-711) [read]. In view 0, `at_this_moment` then sees `now` = zero
  rows != `was` and moves every tile and haze light to the view origin (shadow.cpp:849-876) [read]: tiles degenerate
  (centre == lamp, `tile_rows` divides a zero vector, NaN rows), so the casters draw nothing into the atlas and no tile
  shadows show; the haze lights all sit at the eye [inferred]. Rough size: 12 lamps at the eye with haze radius ~130 add a
  near-uniform veil of a few to ~15% of lamp colour at Haze 50 [inferred, arithmetic on shadow.cpp:297-325]. Player 2's half
  is unaffected (section 3 is not marked out of view 1) [read]. Not seen in any log (no probe run apart exists: only
  logs/run-20261111-173616-test-shadow-steady has probe.txt) [read].
- **P5 One 12-tile budget for both rooms, scored by nearness to the game camera** (1447) [read]: player 2's room's casters
  compete with player 1's room's for 12 tiles, and the score favours what is near player 1's camera [inferred: the game
  camera is player 1's].
- **P6 The characters' own GX lights are player 1's room's** (2.6) [read code; inferred consequence]. Player 2 and the
  enemies in their room are lit in colour and direction as if standing in 342's grid. It also changes their lights' prints
  (fine for the shadow grouping, which is per print).
- **P7 Spell lights are per frame** (2.7) [read]: a rune cast in one room lights the same world place in the other half.
- **P8 Caster slots are matched by world place** (1285-1307) [read]: with overlapping rooms a caster of room B can take the
  slot of one of room A at the same place, popping weights [inferred].
- Not confirmed: the older note "a shadow of the room in play falls in the other half" (coop-next-session.md:74,
  coop-level3-design.md:2446). For marked casters the atlas pass collapses them in the other view (2.4) [read]. A cross-room
  shadow could still come from a lit draw of a caster loaded outside the mark's gate 0x8011A278..0x8011B400 (hle.c:3250-3251,
  3323-3330; refute.md:127 caveat) sharing a tile's print [inferred]. The flattened draws ARE inside the gate: their matrix
  is loaded by GXLoadPosMtxImm at 0x8011AAF4 (lr 0x8011AAF8), then 0x8011BCD4 twice at 0x8011ABA0/0x8011ABC0 [read].

## 5. What per-view lights would need

1. **Two lamp sets.** A = the room in play's (today's), B = player 2's room's, sent only while apart.
   - Host: B's grid is the game's own: the slot holding player 2's room (`room_slot_of(cpu, coop_object_room(cpu, g_coop_p2))`,
     hle.c:13824) -> `room_slot_record` (hle.c:13505) -> record+112 list -> `[byte r13-22136]` (or `[0]` when its +4 is 0),
     exactly as 0x80027998-0x800279D8 picks it [read for the game's rule]. Validate with today's test (`grid+32 == grid+36`,
     hle.c:6110) and the same cell walk (6154-6182). Send with a set index; send "B empty" when not apart.
   - While apart, set A is better read the same way from the current slot's record than from 0x804F0784: if the
     registration is ever fixed per room (G below), entry 0 can be a player-2-room object and the two-slot rule would then
     reject every frame and leave A stale after player 1's next door [inferred].
2. **Which view uses which:** view 0 -> A (player 1's room = the room in play when apart, hle.c:13838-13843 [read]);
   view 1 -> B. The Real frame and any frame shown whole -> today's Frame (set A). The Tome copy's whole frame fitted into
   player 2's half (`g_room_whole_owner == 2`, hle.c:13699-13713) shows player 2's room; it would want B, but the Tome
   frames run no effects ("too few 3D draws", graphics.md "What doesn't cast or light") [read], so left as A [inferred].
3. **Each caster's room by its marks, not its place.** At `submit_draw_plan`, for a flattened draw and a lit set's draw,
   read `marked_out_of(plan.constants.posnormalmatrix, v)` for v = 0, 1 (characters are CPU-skinned, `has_pos_mtx_idx`
   0, failed-attempts.md:218 [read]; for an indexed draw test its XF rows as build_view does). Keep an "out of" mask on each
   ShadowGroup and ShadowLights; a caster's mask is the OR of its groups' and sets'. Out of view 0 -> it is player 2's
   room's (lamps B, Frame 1); out of view 1 -> player 1's room's (lamps A, Frame 0); out of both (0xA2: a room nobody is
   in) -> no Frame; unmarked -> both Frames, lamps A (together case: never reached, as set B is absent then).
   `marked_out_of` is in interp.cpp's anonymous namespace (interp.cpp:569-578): expose it or repeat its 3 tests.
4. **Lamp weights per set.** `ShadowSlot::lampWeight/lampShown` index one lamp array (1068-1081): a slot needs the set it
   belongs to; a caster changing set (a door) starts a fresh slot. `set_room_lights` for set B forgets only B's slots, not
   A's (today it forgets all, 1568-1569). Slot matching (1285-1307) only within the same set (P8).
5. **A camera per view.** `g_cameraUniformOffset` per view: the first scene draw with no matrix index that is NOT marked out
   of that view (view 0: section 2's first draw; view 1: section 3's). `at_this_moment` takes the offset of the view it is
   drawing for. Without this, P4 stays whatever else is done.
6. **Haze per view:** the 12 lamps of the view's set nearest that view's casters, with `KnownLamp::weight` per set.
7. **Spell lights per view (optional, P7):** a view mask per spell light; the host would need the rune's room (the
   effect's +56 is its room, hle.c:13747-13752 [read]; whether a rune record links to its effect is not read).

## 6. Can fill_shadow run per view with casters filtered by the marks?

Yes [inferred from the code read]: everything it needs is per game frame and view-independent except the caster subset, the
lamp set and the camera offset. Run its body twice (`fill_view(frame, view)`), each with the casters not out of that view and
that view's lamp set, into `scene.shadowView[0]` and `[1]`; keep `scene.shadow` (today's, whole) for the Real frame and for
"no set B". The atlas, tile buffer and params are already rewritten per view (begin_casters clears the atlas,
shadow.cpp:701-721; write_params per apply, 595-636) [read], so two Frames need no new GPU resources. The choice at render
time needs the view being encoded: publish it around `encode_between` the way `ssao::set_view_shape` is (common.cpp:2170,
2201) [read], e.g. `shadow::set_view(output.view)` and `-1` after; `render_character_shadows` takes
`perView && view >= 0 ? ssaoScene.shadowView[view] : ssaoScene.shadow`.

A cheaper middle (approach B below) keeps one Frame with a view mask per tile and per haze light and drops the masked ones
per view before `begin_casters`/`write_params`; it fixes P1-P4 and P6-side lamps but keeps the shared 12-tile budget (P5).

## 7. Candidate approaches and what each touches

- **G (game side, host only): each character registered with its own room's grid.** An intercept on 0x80155130 gated on
  `cpu->lr == 0x8002798C` (the step's call) that, when `coop_stay_apart && camera_split_on` and the object r30's +592 is the
  other resident room, answers that room's slot record (the game's own table, as room_section_draw answers 0x80155130 for
  the room draw). Touches hle.c only. Fixes the characters' own lighting in player 2's half (P6) for every room and
  character. Precedent: the 0x80155130 intercept idea for the room draw, picture-readers.md "sites" [read]. Must not run in
  a turn's bracket (it doesn't: the layer walk is outside turns) and must leave together/solo alone (gate). Note 0x80155130
  has 75 callers: lr-gated only (picture-readers.md) [read].
- **A (renderer minimal): fix P4 alone.** Per-view camera offset (5.5). Touches gxcore_draw.cpp (two offsets), shadow.hpp
  Frame (`cameraOffset[2]`), shadow.cpp `at_this_moment(frame, view)`, common.cpp (publish the view). Today's lamps still.
- **B (one Frame, masks):** host set B + index; `set_room_lights(set, ...)` (gxcore_draw.cpp, gxcore header, the
  aurora_backend.cpp/.h wrapper, the patch's declarations at patches/gxruntime/eternal-darkness.patch:88, 2650, 10704,
  25445); marks on groups/sets; lamps per caster by its set; Tile/VolLight `outOf` byte (shadow.hpp); per-view compaction
  in render_character_shadows; plus A. Fixes P1-P4, P8 (with set-tagged slots); not P5.
- **C (two Frames, recommended for the renderer):** as B, but `fill_shadow` body run per view into `Scene::shadowView[2]`
  (ssao.hpp:19-24) with `perView` set only when set B is present; per-set slots and lamp weights; plus A. Fixes P1-P5, P8.
  Each half gets its own 12 tiles.
- **D (spell lights per view):** a view mask on `set_spell_lights` entries from the host (P7). Small, after C.

Order: G and C are independent (G = what the characters look like, C = our shadows and haze); A is part of C and could go
first as a one-step check of P4.

## 8. Cost (co-op is capped at 60, settings.c:349 [read])

- GPU: unchanged number of passes. Each view replay already runs ssao, shadows (atlas + mask + apply) and haze
  (common.cpp:2558-2562) [read]; per-view Frames change their inputs. The excluded view's tiles go away instead of being
  drawn collapsed, so the atlas pass gets slightly cheaper [inferred]. Today's shadow cost was measured at about 0.9 ms a
  frame with 12 tiles (coop-level3-design.md:248) [read], per view as the replay stands [inferred].
- CPU (render thread): `fill_shadow` twice. Its loops are ≤32 casters x ≤96 lamps plus a sort of ≤32x97 wanted shadows
  [read: sizes 1002, 1025, 1273] [inferred: tens of microseconds].
- Host (game thread): the cell walk for set B only when B's grid or mark changes (as today for A) [read, 6117-6118].

## 9. Solo and together exactly as today

- Solo: no split, no marks, `room_slots()` may be 1; the host never sends set B; the renderer's `perView` is false and
  `fill_shadow` runs today's code with set A and `g_cameraUniformOffset` unchanged.
- Together (co-op, one room): `coop_stay_apart` false -> no set B; room_section_draw draws the game's own sections with no
  marks (hle.c:13730-13731) and coop_found_object_draw's room rule needs `room0 != room1` (hle.c:3301-3313) [read]. The
  existing found/unseen marks still exist together (a find, a hidden player in a close-up); with no set B they must keep
  today's behaviour, so the caster-mask filtering must be inside the `perView` path only (today a marked caster still takes
  a tile and simply collapses in that view) [read 2.4].
- The host gate for B: `coop_stay_apart(cpu) && camera_split_on()` read in room_lights_update (outside turns), cleared
  (count 0) as soon as it is false, including during `g_stay_bracket` (coop_stay_apart is false then, hle.c:13833) [read].

## 10. Conflicts with current implementations (for the refuters)

- room_lights_update's "two-slot rule" (hle.c:6125-6131) assumes the first draw-list entry is the room in play's; G breaks
  that assumption when apart -> read set A from the current record while apart (5.1).
- `set_room_lights` forgetting every slot (1568) would, with two sets, reset player 1's shadows whenever player 2's room
  changes -> per-set reset (5.4).
- Picking lamps "by the grid containing the caster" was proposed before (picture-readers.md "sites") and is refuted by
  overlapping rooms (refute.md:136; ed lights 341/342 above) -> by marks.
- Lamps from the draws' lights is a failed attempt (coop-failed-attempts.md:352) and taking 0x804F0784 as it stood another
  (:359): neither is proposed; both sets come from the game's own grid table.
- `g_sh.view` (2136) is section 3's marked matrix when apart: the mark changes the low byte of the translations only
  (about 1e-5 relative, picture-readers.md hazards) [read]; harmless for V.
- `g_sceneCameraCut` (2137-2142) compares the first draw's facing frame to frame; sections 2 and 3 share the camera, so no
  false cut [inferred].
- `g_framesNoCharacters > 8` forgets all slots (1133-1136): a door's blocked frames (3-4, refute.md:140) are under 8; a
  longer load forgets both rooms' slots, then they come back at once (`fresh`) [inferred].
- The game's own shadow (Character shadows off): flattened draws are marked and zeroed in the other view, and the darkening
  quad is screen-wide in both views but darkens by each view's own EFB alpha, so it is already per room [inferred].

## 11. Open questions (each with its probe; not run here)

1. P4 in fact: `ED_RUN_PROBE=1` (AURORA_SHADOW_PROBE) on a stay run with player 1 in a room with lamps near them, then
   `ed flicker RUN` and probe.txt: do view 0's tiles/camera go to zero while apart and come back together?
2. Which draw is the frame's first scene draw when apart (expected section 3): `AURORA_SHADOW_FRAMEDRAWS=1` one frame apart.
3. Does every caster draw carry the mark (the skin-matrix caveat, refute.md:127): `AURORA_SHADOW_LOG=1` `[shadow-set]`
   lines apart, with each set's mask once 5.3 logs it; and one picture of player 2's half for a shadow from player 1's room.
4. record+112's list and r13-22136 for the other slot: read both grids' pointers apart (a log line in room_lights_update)
   and compare with `ed lights ROOM` for both rooms (expect 341: 18 lamps, 342: 9).
5. Whether the step 0x8002790C ever runs inside player 2's turn (then it would already take their grid): a log of lr and
   the slot table at 0x80027988 for player 2's node.
6. What r13-22136 selects (light variants per room; 0x800E2884..0x800E3074 look like script ops) and whether the other room
   needs its own value.
7. The haze veil's real size in view 0 (P4) and whether the user saw "player 1's half without shadows" apart.
8. Spell lights: whether a rune can be tied to its caster's room (for D).
