# Separate rooms: the game's own lighting (reader, 2026-11-12)

Item: lights and shadows per room. This reader covers THE GAME'S lighting: what lights room geometry, actors (both
players' bodies, enemies, items) and effects; where the light grid lives; what is per slot; which player's words each
function reads. Read only: disassembly (the `// ADDR:` comments in `generated/chunks/*.c`, `ed dis`), `runtime/host/hle.c`,
`.deps/GXRuntime/graphics/aurora/lib/gfx/*.cpp`, `generated/main.dol` (.data), the user's play run
`logs/run-20261111-221141-rooms-play`. Nothing edited, built or run.

Tags: [read] = seen at the address/line given. [inferred] = reasoning from what was read, not seen directly.

## 0. Short answers

1. **`0x804F0784` is not a room or slot pointer.** `0x804F0780` is the frame's actor draw list (16-byte entries: +0 object,
   +4 that object's light grid, +8 a priority, +12 distance squared from the camera), count `r13-23956`, reset once a frame,
   filled during the frame's draw, sorted far-to-near and drawn by `0x80119D60`. `0x804F0784` is only entry 0's grid field.
   No room load, slot swap or room section writes it. Only the list's add `0x80119B94` and the sort write it.
2. **The grid is per slot, but the game always takes the CURRENT slot's.** Each slot's record holds a table of grids at
   record +112, indexed by the light-set byte `r13-22136`. The grids sit inside the slot's span (room 341's grid
   0x8307EC28 in slot 1 at 0x83000000; room 342's 0x807CEA6C in slot 0). The one place that picks an actor's grid,
   the character draw callback `0x8002790C`, asks `0x80155130(2)`: the CURRENT slot's record. That is player 1's room
   whenever the frame is drawn. **So player 2's body and every enemy of player 2's room are lit from player 1's room's
   grid.** Their position is looked up in a grid of another room, and the rooms' coordinates overlap.
3. **Room geometry uses no grid.** It is baked: per-vertex index sets choose lightmap coordinates (TEX1) from that
   section's own record, so each half's baked light is its own room's. There are three sets, chosen by `r13-22136`. Live
   lights come on top of that. Up to 7 dynamic lights of the room in play (`r13-23276`) go on COLOR0, and one
   specular-type light 7 sits above `r13-26524` (player 1) on COLOR1. `0x801F2CB4` loads both ONCE per pass, before
   section 3 and section 2. **So player 2's room's geometry gets player 1's room's dynamic lights and player 1's light 7,
   at overlapping coordinates.**
4. **Dynamic lights** (12 slots at 0x80604E60: spells, placed lights, character lights) each carry a room (+42). Each
   created light gets the room in play at its creation, or its descriptor's room. Each light test compares that room with
   `r13-23276` at draw time. Lights of player 2's room never light anything while apart. Lights of player 1's room also
   light player 2's room's things.
5. **Ambient and material colours are constants**, the same for every room. Ambient 0x0B0B0BFF comes from .data
   0x80301234. **There is no fog.** `GXSetFog` has one call site, in GX init, with type NONE.
6. **The light set `r13-22136`** (0, 1 or 2: lightning) is driven once a frame by `0x800E2BB0` from the CURRENT slot's
   record (+140/+142/+146). It is one global byte that selects the actor grid, the room's lightmap set and texture swaps
   for BOTH sections.
7. **Light positions go into the eye space of `0x8060248C`** (player 1's camera matrix 27). The renderer's per-view replay
   changes only the projection (interp.cpp build_view). So a light stays where it is in world numbers in both halves. That
   is why a light of one room shows on the other room's things at the same numbers.
8. **An asymmetry that is not about rooms.** `0x8011A278` gives the object that equals `r13-26524` a larger light budget
   (r25 = 4) than every other actor (1). Non-players get NO room lamps (the cell's lamp list, the "slots 5 to 7" of
   coop-failed-attempts.md). At draw time `r13-26524` is player 1, so **player 2's body is lit as a non-player even when
   the players are together.**

## 1. The facts, with addresses

### 1.1 The actor draw list 0x804F0780 (the "grid pointer" the host reads)

- Add `0x80119B94(object r3, grid r4, priority r5)` [read 0x80119B94-0x80119C30]. It computes distance squared via
  `0x80205FA4(object, 0x806037D8, out)` and stores entry `0x804F0780 + 16*n`: +0 object (0x80119BF4), +4 grid
  (0x80119BFC), +8 priority (0x80119C08), +12 dist² (0x80119C14). n is `r13-23956` (0x80119BDC, ++ at 0x80119C10).
  0x806037D8 is the shown eye camera record, player 1's live one in the draw.
- Callers [read]: 0x800279C4/0x800279D8/0x800279EC are in the character draw `0x8002790C`, priority 3000.
  0x801FA434 is in `0x801FA400`, with grid 0 and priority 30000, for things that are not characters [inferred: items and props].
- Reset: `0x801197E8` stores 0 to `r13-23956` (0x8011985C). Its one caller is 0x801E1208, in the frame begin
  `0x801E1164` [read].
- Draw: `0x80119D60(1)` from the frame draw at 0x80008148. It sorts with `0x800F42FC(0x804F0780, n, 16, 0x80119B40)`
  (0x80119E50-0x80119E64). The comparator orders far first, and a thing whose object +372 is the other entry's object
  (held or attached) after its owner [read 0x80119B40-0x80119B90]. Then it draws each entry with
  `0x8011A278(obj, grid, prio, pass 0/1, 0x801E3B04(), 1)` (0x80119E94-0x8011A080).
  `0x80119D60(0)` from 0x80028348 runs the flattened (shadow) passes, with 8th arg 0 (0x8011A088-0x8011A208).
- **Writers of 0x804F0784**: the stores above and the sort's swaps. Nothing at a room load or the slot swap writes it.
  Checked with an xref of every `lis 0x804F` with offset 1920..1924 [read]. The slot swap `0x80154C50` only XORs the table's
  +0 and +4 [read 0x80154C50-0x80154C90].

### 1.2 Which grid an actor gets (player 1, player 2, enemies)

The character draw callback `0x8002790C` is node +24 for every character: players, enemies and NPCs (hle.c:18148 names
`0x8014ED1C`'s +24 == `0x8002790C` as "a character's"). The node draw walk `0x8014FB6C` runs it in the frame draw
(0x8000812C) [read 0x8014FBC0-0x8014FBE4: `bctrl` node+24 with node+56]. The host intercepts it as `hle_draw_characters`
(hle.c:6410). That changes only the show flags, not the slot table or the player words [read hle.c:6410-6437].

Inside `0x8002790C` [read 0x80027984-0x800279EC]:
```
r21 = 0x80155130(2)                 ; the CURRENT slot's record (0x8058AFC0 +0 -> +12+4*slot)
table = *(r21 + 112)
if table: g = table[ byte r13-22136 ]; if *(g+4) == 0 (no cells): g = table[0]
else g = 0
0x80119B94(object, g, 3000)
```
`0x80155130`: arg 2 gives table[3 + *(0x8058AFC0)], the current slot's record. Arg 3 gives the other slot's record (0 when
+4 == -1) [read 0x80155130-0x8015519C]. Of the getter's callers, only 0x80027988 reads record +112 [read: scan of every
`bl 0x80155130` for a +112 load within 30 instructions]. No caller asks for slot 3's +112. **The other slot's grid table
is never read by the game.**

Both players, at draw time: the frame draw runs outside every host turn bracket. room_section_draw's comment says "the
draw runs outside any turn", hle.c:13697. coop_stay_room_in brackets only player 2's calls, hle.c:14270-14275 [read]. So
table +0 is player 1's slot during `0x8002790C`.
- Player 1's body, enemies of player 1's room: their own room's grid. Correct.
- Player 2's body, enemies and characters of player 2's room (object +592 = their room): **player 1's room's grid**.
  What the game would do if this registration ran during player 2's turn (table swapped) is moot: it never runs there.

Seen in the user's run [read `logs/run-20261111-221141-rooms-play/page-0001.log`]: line 174 `[room-lights] grid
0x8307EC28: 18 lamps` (room 341, slot 1). Then player 2 stays in 341 from frame 1844 (line 2676) while room 342 (slot 0)
is in play, and line 2684 `[room-lights] grid 0x807CEA6C: 9 lamps`. No further grid until the end (frame 2291). The disc
lamps of both rooms are centred near 0 (`ed lights 341`: x -1170..1158; `ed lights 342`: x -247..306, y -2344..1972)
[read]. So player 2 at (-402,-253,8) in room 341 is looked up in room 342's cells at those numbers [inferred].

### 1.3 How an actor is lit from its grid (0x8011A278 -> 0x801E6244)

`0x8011A278(obj, grid, prio, pass, r7, r8)` [read 0x8011A278-0x8011A5F0]:
- r25 = 1, set to 4 when `obj == r13-26524` (0x8011A3F8-0x8011A408). This is a **player-only light budget**.
- `0x801E14C8(obj == r13-26524 || obj+372 == r13-26524)` sets `r13-22064` (0x8011A3C0-0x8011A3E8). That is the player and
  what they hold; it is read back by `0x801E14D8` at 0x8011B750 [inferred: not lighting].
- `r13-26524` is read a third time at 0x8011A9E8 (a draw test that forces r3 = 1 for the player). No `r13-23276` or
  `r13-21492` is read in 0x8011A278..0x8011B400 itself [read: scan of the range].
- grid != 0: `0x801E6244(&pos, r14, grid, r17, r25, flags)` (0x8011A57C).
  grid == 0: `0x801E87F4(&pos, r17)` (0x8011A5B4).
  object +596 bit 0x8: `0x801E8B9C(&pos)` (0x8011A5EC).

`0x801E6244` (its one caller is 0x8011A57C) [read 0x801E6244-0x801E6930]:
- It clamps the position to at least the grid's corner at grid+8 (0x801E62D0-0x801E631C). Then `0x801E5DDC(pos, ..., grid,
  &n, lights)`, which calls the cell lookup `0x801E6B7C` (0x801E5F1C). -1 (outside) gives `0x801E87F4(obj, 3000)`.
- Lights 0..n-1 (n ≤ 4): the blended fills, through `0x801E54BC` (0x801E6404/0x801E645C).
- Then r28 more from the same set at index 4+, in its direction mode (0x801E6594-0x801E665C, r7 = 1): the far light.
- Dynamic lights: count from `0x801F19A0`, capped by `r13-26892` and 3. Loop 0x801E666C-0x801E6800: each light record
  copied; its room (+42) must equal `r13-23276` (0x801E6698-0x801E66A8). If `0x80157364()`, only type 7 (+45). If +41 has
  bit 0x08, the light's room must equal the room of `r13-21492`'s object (0x801E66C8-0x801E66F4).
- The cell's own lamp list (grid+32 cells, cell +12 list, count byte +11): loop 0x801E6830-0x801E6908, r19 of them, at
  index 4 + r28 + dynamic count.
- **`r18` (the r25 passed in) == 1 makes r19 = 0** (0x801E652C-0x801E6538), and r28 ≤ r18 (0x801E6544-0x801E654C).
  - The player (`r13-26524`) gets: fills 0-3, one far light 4, then room lamps up to 3 (lights 5-7).
  - Everyone else gets: fills 0-3, one far light 4, dynamic lights, **no room lamps**.
  - This matches coop-failed-attempts.md's table of "each character's eight lights", which was measured on player 1.
- The mask goes to `0x801E58C8(mask, mask2, 0)` (0x801E691C).

`0x801E54BC(lamp, ..., index r6, mode r7, ...)` [read 0x801E54BC-0x801E5830]:
- It builds a GXLightObj at `0x80602300 + 540 + (r13-21824 << 9) + ((index + 8*r13-21888) << 6)`.
- Colour is lamp +12. `GXInitLightAttn` uses `a0 = 1.5 x power`, as graphics.md says.
- The position goes through `0x80205C48(0x8060248C, lamp, out)`: camera matrix 27, which is **player 1's eye space**.
- Then `GXLoadLightObjImm(obj, 1 << index)` at 0x801E5810, **the game's only call site of it**.

`0x801E87F4` (no grid, or outside it) [read 0x801E87F4-0x801E88C0]: a fixed light built from the object's own position
and constants (`-19084(r2)` colour), plus the dynamic lights (`0x801F19A0`/`0x801F1988`, the same room tests).

### 1.4 Dynamic lights (spells, placed lights, character lights)

- Table: 12 pointers at 0x80604E60 (`0x801F1988` returns it), records of 124 bytes at 0x80604E90.
  `0x801F1A60` clears them, from the graphics init 0x801E87A4 [read 0x801F1A60-0x801F1AE0].
- Record fields: +40 active, +41 flags (0x08: only while the current player's object is in the light's room; 0x02: only
  while `0x800440C8() && 0x80128E98(player object, 15)`), +42 room (s16), +45 type, +24 kind (1 = spot), +52.. place, +96..
  [read in 0x801F19A0 and 0x801F2E58-0x801F2F3C].
- Count `0x801F19A0`: active, +42 == `r13-23276` (0x801F19D0-0x801F19DC), the `0x80157364` type-7 rule, the +41 0x08 rule
  with `r13-21492` (0x801F19FC-0x801F1A28) [read].
- Creation: `0x801F1D84(desc)` (callers 0x80026EC0 in the node event handler 0x80025FF0, and 0x801458C4, a script
  function) -> `0x801F29F0`. That sets **+42 := desc+48, or `r13-23276` if 0** (0x801F2A20-0x801F2A38) [read].
  `0x801F1F90(id, room)` (from 0x80145B98) sets a light's room [read 0x801F1FA0-0x801F1FB0].
- So: a light made during player 2's bracketed call (their room in `r13-23276`) gets their room. One made in player 1's
  flow, or by a door's arrival events outside a bracket, gets player 1's [inferred from the store].

### 1.5 Room geometry: what lights each section

Frame order [read 0x800080E4-0x80008148]: `0x801E6158`, then the opaque pass `0x801E851C` (at 0x80008118), the camera
`0x801E9728`, the node draw walk `0x8014FB6C` (actors register), the translucent pass `0x801E85E0`, and the actor list
`0x80119D60(1)`.

- Opaque pass `0x801E851C` [read 0x801E851C-0x801E85DC]: material setup (`0x801E0DFC`, 4 x `0x801E0F74`), then
  **`0x801F2CB4()` once (0x801E8584)**, `0x801E14F4` (white), then `0x801E75B4(3)` and `0x801E75B4(2)`.
  The translucent pass calls `0x801F2CB4` at 0x801E8684 the same way [read: bl list].
- `0x801F2CB4`, the room's live lights [read 0x801F2CB4-0x801F2F7C]:
  - **Light 7 at `r13-26524`'s position** (`0x80118274(obj)`, height + `-18736(r2)`), colour via `0x801F1DF4`, loaded by
    `0x801E54BC(0x8060546C, ..., 7)` at 0x801F2DC0. With no player object, a fixed light from .data 0x8023B5E0
    (0x801F2E40). Mask bit 0x80.
  - Dynamic lights of `r13-23276` (0x801F2E88-0x801F2E94) as lights 0..6, with the 0x08 rule on `r13-21492`'s object
    (0x801F2EB4-0x801F2EE0). The 0x02 rule uses `r13-21492`'s object through `0x80128E98(obj, 15)` (0x801F2CE0-0x801F2D24).
    Loop bound `r13-26892` (`0x801E3AFC`).
  - `0x801E58C8(dynamic mask, 0x80, 1)` (0x801F2F68).
- `0x801E58C8(maskA, maskB, ...)` [read 0x801E58C8-0x801E5D30]:
  - Channel COLOR0: enabled, sources REG/REG, lights maskA, diffuse CLAMP, attn SPOT (0x801E5910).
  - Material colour: .data 0x803011FC +12 (white) when maskA != 0, else +0 (black). The `ori r0,r0,2` at 0x801E597C makes
    the second test always pass.
  - **Ambient: `GXSetChanAmbColor(0/1, *(0x803011FC+56))`** (0x801E59DC, 0x801E5C7C).
  - `r13-21800 := maskB` (0x801E59E0).
  - COLOR1: lights maskB with diffuse NONE and attn SPEC (0x801E5A00), so light 7 is a specular light. A texture matrix 45
    is built from the chosen specular light (0x801E5AA0-0x801E5C44).
  - Room `r13-23276 == 50` keeps COLOR1 on light mask `r13-21800` regardless (0x801E5C80-0x801E5CB0).
- The section draw `0x801E75B4(section)`:
  - record = `0x80155130(section)`. It sets arrays from that record, including TEX1 = record+60
    (picture-readers.md, 0x801E75B4 line).
  - Then `0x801E5D34(r13-21780 != 0)` (0x801E7724-0x801E7740) [read]. That is COLOR1 on mask `r13-21800` (light 7) if
    `r13-21780 && r13-21800`, else COLOR1 off. **It reads `r13-23276 == 50`** (0x801E5D40) [read].
  - Then the record's prebuilt display list. On a first draw it builds instead (0x801E77B4-0x801E77F8):
    `r13-21780 := 0`, then `0x801E5330(record)` sets it to 1 if any group has +14 != -1 (0x801E539C-0x801E53A4).
    **`r13-21780` is one word for whichever record was built last, not per record** [read; the consequence is inferred].
  - Each group's draw `0x801E50C4` writes 4 indices per vertex straight to the FIFO 0xCC008000: pos, two more, and
    `+4 + 2*set` where set = 1, 2 or 3 from `r13-22136` (0, 1 or 2) (0x801E51F8-0x801E5230, 0x801E529C-0x801E52D8) [read].
    With the room's vertex format POS/NRM/TEX0/TEX1 (picture-readers.md), that last index is TEX1 [inferred: **lightmap
    coordinates, three sets per room, chosen by the light set**].
  - Tail `0x801DFF28(record, r13-22136)` (0x801E7930) swaps per-set textures from a table at 0x805FF6C0 unless
    `r13-22104` (the last set applied) already equals it (0x801DFF40-0x801DFF48) [read]. The first-draw build resets that
    cache (0x801E00D4).
- No GX light is loaded between section 3 and section 2. Nothing runs between 0x801E85C4 and 0x801E85CC [read].
  **Both sections use the same live lights (player 1's room's dynamic lights, light 7 over player 1) and the same light
  set.** Their baked light (lightmap coordinates, textures, record arrays) is each section's own.

### 1.6 The light set (lightning) r13-22136

- Written [read]: in `0x800E2BB0`, the world tick, from 0x80008508 and 0x80050AF8 just before the frame draw
  (0x8000851C / 0x80050B4C, both inside 0x800083C8 / 0x80050A00).
  - Its record is `0x80155130(2)` (0x800E2BD4) and `0x801551EC(2)`, **the current slot**. It reads lightning parameters
    +140, +142, +146 (0x800E2C00-0x800E2C0C), plays thunder with room `r13-23276` (0x800E2C58-0x800E2C70), and stores 0,
    1 or 2 (0x800E2C18, 0x800E2E34, 0x800E2FD8, 0x800E3014, 0x800E3074).
  - Its state is in `r13-24955/-24960/-24942/-24932/-24934/-24936/-24938` and float 0x8032CA70.
  - Also written: 0 in 0x800E2848 (0x800E2884), in 0x800E2B58 (0x800E2B8C/0x800E2B98), and 0 in 0x801E837C (0x801E83A8).
- Read [read]:
  - 0x800279A4: the actor grid's table index.
  - 0x801E51F8: the room's TEX1 set.
  - 0x801E2358, 0x801E2800, 0x801E2964: the material setup.
  - 0x801E7930: the texture swap.
- One byte for both rooms, driven from player 1's room's record [inferred: the world tick is not in a host bracket].

### 1.7 Ambient, material, fog

- Ambient 0x0B0B0BFF and material white/black come from the .data colour table 0x803011FC (`generated/main.dol`:
  `000000ff 404040ff 808080ff ffffffff ... 0b0b0bff` at +56) [read]. It is a constant palette used across the game (100+
  references). **Not per room**.
- Fog: `GXSetFog` (0x8021E844) has one call site, 0x80217E74, in 0x802177A0 (GX defaults: type 0, `GXSetFogRangeAdj(0)`)
  [read]. **The game sets no per-room fog.** Any room darkness is in its lightmaps and textures [inferred].
- The two host calls `0x801E58C8(0,0,0)` named "fog" (hle.c:14838, 15068) are this channel reset, not fog [read].

### 1.8 Effects

No light-setting call (0x801E58C8, 0x801E6244, 0x801E54BC, 0x801F2CB4) has a caller in the effect code 0x8017xxxx
[read: caller lists]. Effect sprites draw with whatever channel state is current, or none [inferred]. A spell's light on
the room is a dynamic light (1.4), stamped with its creation room.

### 1.9 The renderer: what a recorded light means per view

- `build_view` (interp.cpp:576 on) rewrites only each draw's projection through `fn`, zeroes marked draws
  (`marked_out_of`), and moves the HUD. It does not touch lights [read interp.cpp:660-712].
- Lights were loaded in player 1's eye space (1.3). So in player 2's half a light stays at its world numbers [inferred].
- GX light objects persist in XF memory between loads. A draw's mask picks which are used. `shared_constants->lights` (what
  `fill_shadow` fingerprints, gxcore_draw.cpp:2187-2200) may hold all 8 regardless of mask [inferred, not checked].
  That would explain "Player 2's lamp changed as player 1 walked" in coop-failed-attempts.md: player 2's lights 5-7 were
  left over from player 1's draw (1.3).

## 2. Both players, function by function

| Function | Player words / room data it reads | When it runs | Player 1's room / body | Player 2's room / body, apart |
|---|---|---|---|---|
| 0x8002790C (registers an actor + grid) | 0x80155130(2) = table +0's record; r13-22136; r13-26524 / -26276 / -26272 (hide gates) | frame draw (node walk 0x8000812C), outside turns | own grid | **player 1's room's grid** |
| 0x80119B94 (list add) | 0x806037D8 (shown eye camera) for the sort distance | frame draw | fine | sort distance from player 1's eye [inferred: only the order of translucent things] |
| 0x8011A278 (actor draw) | r13-26524 three times (budget r25, r13-22064, 0x8011A9E8) | frame draw | player 1 gets the player budget | **player 2 gets the non-player budget (also together)** |
| 0x801E6244 / 0x801F19A0 / 0x801E87F4 / 0x801E8B9C | r13-23276 (light room), r13-21492 (+41 0x08 rule), r13-26892 | frame draw | right lights | **player 1's room's dynamic lights; none of their own** |
| 0x801F2CB4 (room's live lights) | r13-26524 (light 7 place), r13-21492 (0x02/0x08 rules), r13-23276 | once per pass, before sections 3 and 2 | right | **light 7 over player 1 and player 1's room's dynamic lights on their room** |
| 0x801E5D34 (COLOR1 per section) | r13-23276 == 50, r13-21780, r13-21800 | each section | right | room 50 test against player 1's room; r13-21780 of the last-built record |
| 0x801E50C4 / 0x801DFF28 (baked light) | r13-22136, r13-22104 cache, the section's record | each section | own lightmap set | own lightmaps, **player 1's room's lightning set** |
| 0x800E2BB0 (lightning) | 0x80155130(2), 0x801551EC(2), r13-23276 | world tick, before the draw | right | player 2's room has no lightning of its own |
| 0x801F29F0 (light's room at creation) | r13-23276 | wherever a light is made | right | right if made inside a player 2 bracket; else player 1's room |
| 0x801E58C8 (channels, ambient) | constants; r13-23276 == 50 | per light set | same | same (constant) |

During player 2's turn (coop_stay_room_in: `r13-23276` and the table +0/+4 swapped, hle.c:14258-14279), none of the
lighting functions run, except a dynamic light created in that bracket [inferred from where each is called].

## 3. What the host does today

- `room_lights_update` (hle.c:6099-6188), called from `notify_look_at` (hle.c:6232):
  - It reads `0x804F0784`, the draw list's entry 0 grid (1.1). Its comment calls 0x804F0780 "a list of the room's models",
    which is right in effect.
  - It checks `grid+32 == grid+36`. With two slots it accepts the grid only inside the current slot's span
    (hle.c:6127-6134).
  - It sends one lamp set and cell table with `dol_aurora_set_room_lights`. Since every character entry carries the
    current record's grid (1.2), the span test is a safeguard, not a selection [inferred].
  - The "alternating near a door" note (graphics.md:29) is unexplained by this reading: see open question 1.
- gxcore `set_room_lights` (gxcore_draw.cpp:1543-1569) keeps ONE lamp set (`g_lamps`, `g_gridCells`).
  `fill_shadow` (1111) runs once per frame at the display copy (1723). Casters come from the game's flattened draws (2172,
  `0x80119D60(0)`'s passes). Their lamps are the one grid's cell at their world place, clamped to the edge
  (1333-1339). The haze uses the same lamps (1527-1540).
- `room_section_draw` (hle.c:13688-13744) wraps each section in a marked matrix. It loads no lights.
  `coop_found_object_draw` (hle.c:3270-3320) marks an actor's matrices out of the other room's half by object +592. It is
  a notify on 0x8011A278 and changes no light words.
- Nothing in the host writes `r13-22136`, `r13-21780/-21800`, 0x80604E60, or `r13-26524` around a draw. The only
  `r13-26524` swap near drawing is the sound gain (hle.c:14174-14178) [read grep].

## 4. Candidate approaches (each gated: co-op on AND coop_stay_apart AND the thing's room == player 2's room)

All use the game's own functions and data. All are engine-level: every room, chapter, character and form, because they
key on object +592 and the slot records, never on a room number.

**L1. Actors of player 2's room get their own room's grid (the game's own selection, other record).**
- Intercept `0x80155130` only at lr 0x8002798C (the call inside 0x8002790C). When apart and the drawn object's room
  (`cpu->gpr[30]` +592 there; r30 = node+52 at 0x80027938) is the other resident room, answer that slot's record
  (`room_slot_record(cpu, room_slot_of(cpu, room))`).
- The game's own +112 table, the `r13-22136` index and the cell-count fallback then do the rest.
- Touches: one lr-gated answer. 0x80155130 has many callers (picture-readers counts 75), so the lr gate is mandatory.
  Precedent: picture-readers.md's lr-gated 0x80155130 idea.
- Fixes: player 2's fills, far light and lookup cell, and their enemies'.

**L2. The dynamic lights of player 2's room on their room's actors.**
- Around `0x8011A278` for an object whose +592 is player 2's room: `r13-23276` := that room, and `r13-21492` := player
  2's node for the +41 0x08 rule. Put them back after.
- Touches: `coop_found_object_draw` is a notify today. Bracketing needs an intercept wrapping the call with
  `hle_guest_call` (precedent `hle_draw_characters`, `room_section_draw`), or a notify at entry plus a restore at its
  return.
- Must read before building: every callee of 0x8011A278 for `r13-23276` / `r13-21492` / slot reads (0x80129DDC,
  0x8016E040, 0x8011C3C0, 0x80118F94, 0x80118CAC, ...). Only the light callees were read here.

**L3. Player 2's own body lit as "the player" (budget 4, room lamps 5-7).**
- `r13-26524` := player 2's object around 0x8011A278 when obj == player 2's object.
- This also changes the together case, and `r13-26524` is read twice more there (0x8011A3C0 hold flag `r13-22064`,
  0x8011A9E8). **It needs the user's word:** the rule "nothing may change the together case" versus "both players the same".
- If kept apart-only, gate it like L1.

**L4. The room geometry of section 3 lit by its own room.**
- In `room_section_draw`, for section 3 of both passes (already a `hle_guest_call` wrapper):
  1. `coop_room_in(room1)` (`r13-23276` and table +0/+4), `r13-26524`/`r13-21492` := player 2's, call `0x801F2CB4()`.
  2. Run the section.
  3. Put the words back and call `0x801F2CB4()` again before section 2 (player 1's lights back).
- `0x801E5D34`'s room-50 test then sees the right room. GX loads are recorded in order, so each section's draws carry its
  own lights [inferred].
- `r13-21780` needs a per-record value. Either note each record's flag when its list is built (0x801E77B4-0x801E77F8,
  by record), or recompute it with the game's `0x801E5330` rule, and put it in per section.
- Touches: two extra guest calls per pass. Section 2 then runs with lights reloaded.

**L5. The light set (lightning) per room.**
- Run `0x800E2BB0` for player 2's room with that room in, and per-room copies of its state words (list in 1.6), as the
  host already swaps script state per player. Draw section 3 and player 2's actors with their room's `r13-22136`.
- `0x801DFF28` re-swaps textures when the set differs between the sections (`r13-22104` cache): a cost to read first.
- Only rooms with lightning parameters matter. Low priority.

**L6. The renderer's shadows and haze per room** (for the shadow readers; listed so the designs line up).
- `room_lights_update` reads both resident records' grids the game's way: record = 0x8058AFC0 + 12 + 4*slot, then
  `*(record+112)[r13-22136]` (if its +4 is 0, `[0]`), each tagged with its room. It no longer reads the draw list's entry 0.
- gxcore keeps two lamp sets. `fill_shadow`'s lamp pick and the haze run per view, each with its own room's lamps. A
  caster belongs to the view its flattened draws are not marked out of (the same `coop_found_object_draw` mark, since the
  flattened passes go through 0x8011A278 too) [inferred].

## 5. For the refuters: where these could conflict

- R3's marks: lights reloaded for section 3 must not stay for section 2 or the actors (L4 reloads after).
  0x801E6244 reloads lights for each actor anyway [read].
- `hle_draw_characters` is already an intercept on 0x8002790C (re-entrancy flag `g_draw_characters_running`). L1 hooks
  0x80155130 inside it, so they don't nest.
- The whole-frame case (split off, `camera_split_on()` false): `coop_found_object_draw` doesn't mark then (hle.c:3301), and
  section 3 isn't drawn. Lights for player 2's room's actors would not matter in a whole frame of player 1's room. Gate
  L1-L4 on the same `camera_split_on() || g_room_whole_owner` test as room_section_draw.
- Solo and together: every path keys on `coop_stay_apart` (player 2's room resident and different). L3 is the exception
  if done for both cases.
- `0x80155130` and `r13-23276` are read by many systems. Only lr-gated or bracketed answers, never a lasting write.
- Don't confuse with the failed attempts: this proposes no change to the renderer's lamp choice rules
  (coop-failed-attempts.md "Shadows that followed player 1", "flickering"). Those stay; only which room's lamps they see
  changes.

## 6. Open questions

1. graphics.md:29 says `0x804F0784` alternated between two grids near a door with ONE slot (room 1: 12 and 34 lamps). By
   this reading, every character entry carries the current record's `+112[r13-22136]`. So is the alternation the light
   set (lightning: table[1] vs [0], both inside the same file, the higher one later), or a record whose +112 changes
   during a load? Test: print `r13-22136` and the record's +112 table on each `[room-lights]` line.
2. How many entries does each record's +112 table hold, and are 1 and 2 empty (count 0, falling back to [0]) in rooms
   without lightning? Read from a record in memory, or `ed lights`' loader.
3. Does the room's TEV use COLOR0 (the dynamic lights) at all, or only COLOR1 (light 7) for groups with +14 != -1? Read
   `0x801E21FC`'s TEV setup (0x801E2700, 0x801E24D8). coop-failed-attempts.md says a spell is "a real light on the room",
   which suggests COLOR0 is used.
4. `0x801DFF28`'s table 0x805FF6C0: is it the current room's alone, or both resident rooms'? What does drawing section 3
   with it do?
5. Does gxcore's per-draw light record (`shared_constants->lights`) keep lights outside the channel mask? That decides
   whether the renderer's "lights of a character" include stale ones from the draw before.
6. What is `0x80128E98(obj, 15)` (light flag 0x02's rule)? Something the player's object carries [unknown].
7. The flattened shadow passes `0x80119D60(0)` (0x80028348) also go through 0x8011A278 with the grid: does the game's own
   flattened shadow take its direction from that grid's lights (the renderer replaces the game's shadows, so it may not
   matter)?
8. Player 2's own creations: do their spells' dynamic lights already get their room (made inside the cast's bracket)?
   Read where `0x801F1D84` is reached from the spell code during `coop_quick_cast`.
