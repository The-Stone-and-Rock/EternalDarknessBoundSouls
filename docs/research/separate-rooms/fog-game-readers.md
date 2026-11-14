# Separate rooms: the game's room mist (reader A, 2026-11-13)

Item: the user's report, players apart (ED_ROOM_SLOTS=2, ED_COOP_STAY=1): "fog from the block room is seen in the other
player's screen (the room right next to the block room) where fog is not present, and vice versa ... depending on which
player leaves the room, the fog changes." This reader covers THE GAME: how it draws a room's haze, where the numbers
come from, and which of them is one global. Read only: `ed dis`, `build/ed-db/disasm.txt`, `generated/symbols.tsv`,
`generated/main.dol` (.data/.sdata2), save states read with `tools/ed/savestate.py` (no game run), the run pictures
named below. Nothing edited, built or run.

Tags: [read] = seen at the address/line given. [read there] = from the doc named. [inferred] = reasoning from what was read.

## 0. Short answers

1. **It is not GX fog and not the renderer's haze.** `GXSetFog` (0x8021E844) has one call site in the whole game
   [read: `grep -c "bl      0x8021e844"` = 1; it is GX init, lights-game-readers.md 1.7, read there]. The renderer's
   own haze is off in the user's settings (`settings.ini:19 haze = 0` [read]; it only lights for spells then,
   patch `misc[0] = max(volumetric(), spell_lit() ? ...)` [read]).
2. **The haze is a mist layer in the game's own materials: "layer 9".** Every material the game sets up through
   `0x801E21FC` (room sections' per-frame lists AND actors) gets two extra TEV stages that ADD
   `K2 colour x ramp(depth, height) x scrolling noise` to the surface [read, section 1]. That is the pale floor mist in
   the pictures.
3. **Every input of that layer is the ROOM IN PLAY's, read once per frame, and applied to BOTH sections.** The mist record
   is `record+124` of the CURRENT slot (`0x80153998(2)` -> `0x80155130(2)` = slot table +0) [read]. Its height, ramp
   texture, palette animation and scroll, the texture bank the ramp is taken from (`r13-21808`), the two texture
   matrices (built from the game camera), the noise palette and the K2 colour are each ONE global. Section 3 (the other
   slot's room) is drawn with them unchanged [read, section 2].
4. **During the draw the current slot is player 1's room** (`[stay] ... the room in play is 183 (slot 1)` while player 2
   stays in 180, run-163109; `the room in play is 180 (slot 0)` while player 2 stays in 183, run-163231 [read]). So
   **both halves show player 1's room's mist**, and whose room that is changes when player 1 changes room: "depending
   on which player leaves". A room in play with NO mist record (341, 343) turns the mist off in both halves [read:
   `r13-22144` and the r28 gate only on E != 0].
5. **The depth the mist grows with is measured from the game camera** (`0x8060248C`'s third row, built into texture
   matrix 54 at 0x801E4EE4-0x801E4F3C [read]). In player 2's half that is player 1's eye, in another room whose
   coordinates overlap [inferred: the per-view replay changes only the projection, lights-render-readers.md 2.4, read
   there].
6. **Which rooms have it (chapter 1):** 180 (top 196.1, ramp texture 67), 182 (239.2, 88), 183 (306.2, 107) have a mist
   record; 341 and 343 have none (`record+124 == 0`) [read, save states, section 4]. 344 was not read (no state holds it).
   **The block room is 183** (BlockRegion01 / the Granite Block, `MantBlockCam01`; `ed room 183` [read]); "the room right
   next to it" is **180**, joined to it by `Region02 -> 183/EntryMkr01` (`ed room 180` [read]), and the room the two
   runs used.
7. **Which is correct:** each room's own record. Solo, 180 shows a faint pale floor mist (run-20261113-163339-jtg-stayer-s14-solo
   shot 000222, frame ~4124, [read picture]) and the 183 corridor at its close camera shows none visible (shot 000192,
   frame ~3568 [read picture]). Apart: 180 under 183's record is hazy (run-163109 shots 000014/000030 right half [read
   picture]), the 183 corridor under 180's record is clear (run-163231 shot 000020 right half [read picture]).

## 1. How the game draws the mist (the whole path)

### 1.1 The mist record E = `record+124` of the current slot
- `0x80153998(2)`: returns `0x80153954()` if that is non-zero (0x805B2620 when `0x801538B8()` says so; not traced),
  else `0x80155130(2)` = the record of slot table +0 (0x8058AFC0), and its `+124` only if `+0x8142`/`+0x8143` (loaded) are
  set [read 0x80153998-0x801539E0, 0x80155130-0x8015519C].
- E's fields as the code uses them [read 0x801E0DFC, 0x801E4AB0, 0x801E4EBC; values from save states, section 4]:
  - `+0` float: the mist's top height. Band = world z from `E0-200` to `E0` (see 1.4). Moved toward a target by
    `0x801E4AB0` when `r13-21920` frames are set (script `fn124 room_layer_height`, tools/ed/primitives.tsv:94 [read];
    no chapter-1 script calls it: `ed uses fn124` = L06/08/09/11/12/16 only [read]); a room change resets the target
    (0x801521DC -> 0x801E4A80) [read].
  - `+4`, `+8` floats: scroll per frame of the noise in x and y (`0x801E4B4C-0x801E4B90` add them to `r13-21928`,
    `r13-21924`, wrapped) [read]. 0 and -1.25 in 180, 182, 183.
  - `+12`, `+16`, `+20`, `+24`, `+28`, `+40`, `+44`: the noise palette's random walk (min 150, max 250, step 2,
    lengths) [read 0x801E0DFC-0x801E0F60, 0x801E055C].
  - `+36`: the ramp texture's index in the room's texture bank (67, 88, 107) [read 0x801E2268: `lwz r0,36(r3)` ->
    material `+18`].

### 1.2 Once a frame, from the room in play
- Frame begin `0x801E1164` -> `0x801E4AB0` (bl at 0x801E1188): height ease and the scroll [read].
- Camera `0x801E4BDC` (0x801E4EB8-0x801E5000): if E, builds into `0x8060238C + 96*r13-21824` (two 48-byte texture
  matrices, double-buffered) [read]:
  - matrix A (texmtx 54): row 0 = `-(camera 0x8060248C row 2) / 2000` (depth from the game camera), row 1 =
    `(0, 0, 0.005, 0)`, concatenated with `Trans(0, 0, 200 - E0)`. So s = depth/2000, t = (z + 200 - E0)/200
    [read; constants r2-19188 = 2000.0, r2-19184 = 0.005, r2-19192 = 200.0 read from main.dol, r2 = 0x80628460].
  - matrix B (texmtx 57): `0.000625 x` and `0.000625 y` after `Trans(r13-21928, r13-21924, 0)`: world-XY noise, one
    tile per 1600 units, scrolling [read; r2-19180 = 0.000625].
  - `DCFlushRange` and `GXSetArray(23, ...)` of them [read 0x801E4FE4, 0x801E5000].
- Frame draw `0x800080E4`: `r13-22144 := 1` if E (0x801E1CEC(1) at 0x80008110), back to 0 after the draw
  (0x80008188) [read].
- Opaque pass `0x801E851C`: `0x801E0DFC(E)` steps the noise palette TLUT 0 (`r13-22124`, 256 IA8 entries,
  `(255-v)<<8 | v`) one entry along a random walk inside E's limits, then DCFlush [read 0x801E852C-0x801E8530,
  0x801E0DFC-0x801E0F60]. The four `0x801E0F74` calls step TLUTs 2-5 from fixed .data descriptors (0x8025B9F8...;
  global, not per room) [read].

### 1.3 Per section, in BOTH section draws
- `0x801E75B4(section)`: `r28 = 0x80153998(2)` (the CURRENT slot's E, whatever the section) [read 0x801E75D0-0x801E75D8];
  `r13-21808 := current slot record +0x813C` (the current room's texture bank) [read 0x801E75DC-0x801E75F8]; if r28:
  `GXLoadTlut(0x80601700, 0)`, `GXLoadTexMtxImm(A, 54)`, `GXLoadTexMtxImm(B, 57)`, `GXLoadTexObj(0x806016C0, 7)`
  [read 0x801E7630-0x801E7698]. The translucent pass `0x801E73D4` does the same (0x801E744C, 0x801E74A8-0x801E74FC)
  [read].
- Then the section's per-frame list is rebuilt every frame: `GXBeginDisplayList(record+296[...])`,
  `0x801E53FC(record, record+28, 0)` -> `0x801E50C4` per piece -> `0x801E21FC(material, ..., mask 691)` at 0x801E5270
  [read 0x801E7768-0x801E77AC, 0x801E5448/0x801E5488, 0x801E5258-0x801E5270].

### 1.4 The layer itself (`0x801E21FC` -> `0x801E2700`)
- `0x801E21FC`: `r30 = r13-22144 & r13-22140` (0x801E1D0C; `r13-22140` is set to 1 at arrival by 0x801E1CFC(1),
  readers.md, read there); `E = 0x80153998(2)`; material `+18 := r30 ? E+36 : (stays -1)` [read 0x801E2224-0x801E2274,
  0x801E2470-0x801E2474].
- Layers are emitted in the order of .data 0x8025B9D8 = `1, 4, 7, 0, 2, 5, 9, 3` [read main.dol]; a layer whose index
  is -1 or not in the mask is skipped (0x801E23D0-0x801E23F4). Mask 691 (room sections) has bit 9 [arithmetic].
- Layer 9 [read]:
  - Texture: `0x801569F8(r13-21808, out, material+18)`, i.e. index E+36 from the CURRENT room's bank (0x801E2AA4-
    0x801E2AB0; when `0x801538B8()` the bank is `r13-21812` instead, 0x801E2A84-0x801E2A9C), loaded into the layer's
    texmap (GXLoadTexObj 0x801E2CCC).
  - Texcoords (jump table 0x8025BED0 case 9 = 0x801E2EA4): coord n from position through texmtx 54 (matrix A),
    coord n+1 from position through texmtx 57 (matrix B) with map 7 [read 0x801E2EA4-0x801E2ED8].
  - TEV (jump table 0x8025BEA4 case 9 = 0x801E3494): stage n: `KColorSel(14)`, colour `ZERO, KONST, TEXC, ZERO` ->
    TEVREG1 (K2 x ramp); stage n+1: colour `ZERO, TEXC, C1, d` -> PREV, with d = the previous colour (r24): the
    surface PLUS noise x (K2 x ramp) [read 0x801E3494-0x801E35A4; wrappers 0x801E3780 = GXSetTevColorIn,
    0x801E39A0 = GXSetTevKColorSel, 0x801E37FC = GXSetTevColorOp, generated/symbols.tsv].
- The noise: one 256x256 C8 texture (`r13-21988`), made by a diamond-square fill at each room change
  (`0x801E01D8`: corners 120, 0x801E0A98/0x801E0AD0, then tiled through the write-gather pipe), on texobj 0x806016C0 with
  TLUT 0 (`0x801E065C`, GXInitTexObjCI with tlut 0, GXInitTlutObj(0x80601700, r13-22124, IA8, 256)) [read].
- K2 = `r13-21956`, sent by `0x801E133C` when it changes and resent by `0x801E13CC` [read]. At arrival
  `0x801E0170(E)` sets it to `0x80301208` = 0xFFFFFFFF, white [read 0x801E018C-0x801E01A0, main.dol]. All save states
  read hold 0xFFFFFFFF [read]. Other setters: 0x8007CCF0/0x8007CD38 (a level whose per-frame code checks room 83:
  a colour from .data 0x802392BC picked by an object's alignment, by the room in play), 0x80016890, 0x800B8410
  [read; not chapter 1, not traced further].
- Actors get the same layer: the object draw builds texmtx 54/57 as `A x model` and `B x model` from the same
  `0x8060238C` (0x8011A748-0x8011A7BC) and sets materials through `0x801E21FC` (0x8011AB50) [read]. So both players'
  bodies and every enemy get the room in play's mist too [inferred from the shared code].

### 1.5 At a room change (current slot only)
`0x80151FDC` (unless flag 8): `0x80153998` -> `0x801E0170(E)` (K2 white, palette walk state), `0x801E01D8` (new
noise), `0x801E055C(E)` (fills TLUT 0's history) [read 0x80152160-0x80152170; r4d-arrive-readers.md, read there];
`0x801E4A80` resets the height target (0x801521DC) [read]. Nothing runs for the other slot.

## 2. What is one global (all read once per frame or per draw, from the room in play)

| Input | Where | Set from |
|---|---|---|
| E (has mist, its fields) | `0x80153998(2)` in 0x801E21FC, 0x801E75B4, 0x801E73D4, 0x801E4AB0, 0x801E4BDC, 0x800080E4, 0x801E851C | slot table +0 = current slot [read] |
| frame flag | `r13-22144` | 1 if current E, for the whole frame draw [read] |
| ramp texture | material +18 = E+36, from bank `r13-21808` | current record +0x813C (0x801E75F8), not the section's [read] |
| height band, depth | texmtx 54 in `0x8060238C[buf]` | E0 of the current room; the game camera 0x8060248C [read] |
| noise scroll | texmtx 57; `r13-21928`, `r13-21924` | advanced by current E+4/+8 [read] |
| noise palette | TLUT 0, `r13-22124` | walked from current E (one history) [read] |
| noise texture | `r13-21988`, texobj 0x806016C0, map 7 | regenerated at each change [read] |
| colour | K2, `r13-21956` | white at arrival; per-level scripts [read] |

The section's own record supplies only its geometry, its own texture bank for its other layers (0x801E1B74(record+
0x813C) at the section draw's start) and its lights (picture-readers.md, read there). Nothing in the mist path takes the
section's record.

## 3. What the user sees, explained [inferred from 1 and 2]
- Player 1 in 180, player 2 in 183 (room in play 180): 183 is drawn with 180's band (z -4 to 196), 180's ramp (bank
  texture 67) and depth from player 1's camera in 180. The 183 corridor floor sits at z ~177 (run-163231 `[stay]`
  player 2 at 1305 1508 179) near the band's top: its own band (106 to 306) put it inside. Picture: clear
  (run-163231 shot 000020, right).
- Player 1 in 183, player 2 in 180 (room in play 183): 180 (floor z ~0, player 2 at 2 -554 0) is drawn with 183's band
  and ramp: hazy (run-163109 shots 000014, 000030, right).
- How dense a point is depends on the ramp texture's values and wrap mode at those coordinates, which were not read.
- Player 1 in 341 or 343 (no record): `r13-22144` stays 0 and the section draw loads nothing: player 2's 180/182/183
  half loses its mist entirely. Not seen in a picture yet.
- Together in one room in the split, player 2's half still takes its depth from player 1's camera (matrix A). Seen
  only as a guess: run-163231 shot 000005 (both in the 183 corridor) shows a pale haze in both halves where the solo
  shot 000192 at a close camera shows none.

## 4. The save states read (tools/ed/savestate.py, `record+124` of each slot)

| State | In play | Slot records |
|---|---|---|
| slot1.state | 180 | 180: E0 196.1, tex 67, scroll 0,-1.25 |
| slot2.state, jtg-ch1-room183.state | 183 | 183: E0 306.2, tex 107 |
| rooms-ch1-182.state | 182 | 182: E0 239.2, tex 88; dead slot (room -1): 196.1 / 67 (180's) |
| rooms-ch1-183.state, rooms-r6-base.state | 183 | 183: 306.2 / 107; dead slot: 196.1 / 67 (180's) |
| rooms-ch1-341-apart.state | 343 | 343: none; 341: none |
| others | 100: 201.6 / 69; 338, 147, 249, 153: none | |

`E+12..+44` were the same in 180, 182, 183 (150, 250, 2, 4, 2, 0, -, 1, 1) [read].

## 5. Other things seen in the same pictures (not the mist)
- 180's brazier flames show when 180 is the room in play (run-163231 shot 000020 left; solo shot 000222) and are
  missing in player 2's 180 half when 183 is in play (run-163109 shots 000014, 000030, right) [read pictures]. Particles
  are their own item (particles-reader.md).

## 6. Not read
- The ramp textures (bank index 67, 88, 107) themselves, and their wrap mode: how dense each band is.
- Where the record's `+124` is filled at a room load (no direct `stw ..,124(rN)` in 0x8015xxxx code; a copy or a
  relocation loop, not found).
- 344's record. `0x801538B8` / `0x80153954` (the override record 0x805B2620 and bank `r13-21812`). `0x801DF940`'s use of E
  (a z-mode branch). 0x801105F4/0x80110620 (callers with another argument).

## 7. Host today
No host code reads or hooks any of this: no match in hle.c, camera.c or interp.cpp for 0x80153998, 0x8060238C,
0x801E4AB0, 0x801E0DFC, 0x801E21FC, r13-21928/-21924/-21956/-22144 or 0x806016C0 [read: grep]. During the draw the host
keeps player 1's room current (`coop_stay_room_in` swaps the table only round player 2's turn, picture-readers.md, read
there), which is why the mist is always player 1's room's.
