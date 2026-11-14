# The room mist per room, players apart: the design, revised (2026-11-13)

Item: the user's report (2026-11-13), players apart (ED_ROOM_SLOTS=2, ED_COOP_STAY=1): "When players are separated,
graphics such as fog from the block room is seen in the other player's screen (the room right next to the block room)
where fog is not present, and vice versa. I'm not sure which is correct at the moment but depending on which player leaves
the room, the fog changes."

Inputs: readers `fog-game-readers.md` (reader A, the game) and `fog-host-readers.md` (reader B, host and renderer); the
two refutations `refute-fog-impl.md` (against the code) and `refute-fog-goal.md` (against "built by design"). Every point
of both refutations was checked again here in the code (section 2 has the table). Read only: nothing edited, built or
run; the game was not launched. Tags: [read] = seen at the address, file:line, save state or picture given; [read there]
= from the doc named; [inferred] = reasoning from what was read.

Words: **room0** = the room in play, `r13-23276` (apart: player 1's); **room1** = player 2's room
(`coop_object_room(cpu, g_coop_p2)`); **view 0** = player 1's half (left), **view 1** = player 2's (right); **E** = a
room's mist record, `record+124`; **E0/E1** = room0's/room1's; **slot table** = 0x8058AFC0 (+0 the current slot, +4 the
other); **C** = the game camera, 0x8060248C; **buf** = the mist matrices, `0x8060238C + 96*r13-21824` (A at +0, B at
+48); **k2000, k005, k200, k0625** = the floats at r2-19188, r2-19184, r2-19192, r2-19180 (2000.0, 0.005, 200.0,
0.000625 [read there: reader A]).

## 0. Short answers

1. **The block room is 183** (BlockRegion01, the Granite Block, MantBlockCam01); the room next to it is **180**
   (183's Region01 -> 180, 180's Region02 / script 29 -> 183) [read: `ed room 183`, `ed door 180`].
2. **The "fog" is the game's own mist layer ("layer 9")**, two TEV stages that `0x801E21FC` (material setup) adds to
   room pieces and actors: `K2 x ramp(depth, height) x scrolling noise` [read there: reader A 1.4]. Not GX fog
   (`GXSetFog` has one call, GX init) and not the renderer's haze (off in the user's settings) [read there: reader A 0.1].
3. **Why it crosses:** the layer's inputs are the room in play's: the record `0x80153998(2)`, the ramp bank `r13-21808`,
   the frame gate `r13-22144`, texture matrix 54 (built from E0's top and C's depth row) and 57 (the scroll) [read,
   1.1]. Apart, the room in play is player 1's, so both halves take player 1's room's mist, and it swaps with whoever
   changed room last.
4. **Which is correct:** each room's own. 183 has a heavy pale floor mist over its pit; 180 a faint one. Apart, 180's
   half under 183's numbers shows a pale veil (s6 000014 right; its braziers also unlit), 183's half under 180's numbers
   shows none (s11 000020 right) [read pictures].
5. **New since the first design (from the refutations, checked here):**
   - The room sections' static pieces are compiled ONCE into cached display lists, and the gate, ramp index and ramp
     texture address are baked into them from whatever room was current at the compile [read 1.2]. The fix must
     compile inside the right room's numbers and throw away lists baked with the wrong room's (A4).
   - Actors take the layer through `0x8011BE00 -> 0x8011C11C` and `0x8011B97C -> 0x8011BAC0` inside the object draw,
     not through `0x8011AB50` (mask 0); `0x8011BE00` has a rule keyed to the room in play's NUMBER (A3).
   - Effects never load texture matrices 54/57: they reuse the last actor's (A7).
   - At every door the game rebuilds the one shared noise and resets its scroll: the stayer's mist jumps (A8).
   - The mist height ease is one global on the room in play's E (A9).
   - Texture matrices are not corrected per view by the renderer: together, player 2's mist depth is player 1's
     camera's; while player 2's close-up holds the game camera, player 1's depth is player 2's camera's (Part B).
6. **The fix, two parts.** Part A (host, apart): a "mist scope" that gives each room's things that room's own mist
   (record, bank, gate, band, list, scroll, height, the actor rule, the effects' matrices) and player 2's camera depth
   for room1's things, plus the stayer's noise kept through the other's door. Part B (renderer patch, together and
   apart): texture matrix 54's depth row rewritten per view from that view's camera. Co-op only; solo, follow without
   the split, and the Tome copy turn run today's code.

## 1. Facts this design rests on (checked here)

### 1.1 The inputs of layer 9
- `0x80153998(n)`: the override record `0x80153954()` if non-zero, else `0x80155130(n)+124` when `+0x8142` and `+0x8143`
  are set [read there: refute-impl 8]. `0x80155130(n)`: n=2 the current slot's record, 3 the other's, 0/1 that slot's
  [read there]. First word 0x9421FFF0 [read there]. Its 13 bl sites [read: grep]: 0x800080F8 (frame gate), 0x800206E4
  (game load), 0x801105F4, 0x80110620, 0x8011BE58 (actor rule), 0x80152154 (room change, asks with the slot index),
  0x801DF940, 0x801E2240 (material setup, return 0x801E2244), 0x801E4AD8 (height ease and scroll), 0x801E4EBC (matrix
  build), 0x801E744C (translucent section, return 0x801E7450), 0x801E75D4 (opaque section, return 0x801E75D8),
  0x801E852C (palette walk).
- Opaque section draw `0x801E75B4`: r28 = `0x80153998(2)`, `r13-21808 := 0x80155130(2)+0x813C` (0x801E75DC-0x801E75F8,
  the only writer [read: grep]); if r28: GXLoadTlut(0x80601700, 0) 0x801E7644, texmtx 54 from buf 0x801E7664, 57 from
  buf+48 0x801E7688, GXLoadTexObj(0x806016C0, 7) 0x801E7698 [read]. Translucent `0x801E73D4`: asks at 0x801E744C,
  loads 54/57 at 0x801E74CC/0x801E74F0, never writes `r13-21808` [read].
- Material setup `0x801E21FC`: r30 = `r13-22144 & r13-22140` (0x801E2224 -> 0x801E1D0C); `0x80153998(2)` at 0x801E2240;
  `material+18 := r30 ? E+36 : -1` (0x801E2258-0x801E2274; with r30 = 1 and E = 0 it reads address 36) [read]. The
  ramp: `0x801569F8(r13-21808, out, material+18)` at 0x801E2AA4-0x801E2AB0, or bank `r13-21812` at 0x801E2A90 [read].
  E+36 indexes the bank: the record and the bank must be the same room's [inferred].
- The gate `r13-22144`: one writer, `0x801E1CEC` [read: grep]; set from 0x80008110 when E0 != 0, cleared at 0x80008188,
  and set to 0 for its own draw by an object whose +580 is 104 (0x8011A390-0x8011A39C) [read]. 0 in every save read
  (states are taken outside the draw) [read: savestate.py].
- Matrix A (texmtx 54) is built only when E0 != 0, at 0x801E4EB8-0x801E4F60: `row0 = -(C row2)/k2000`, `row1 =
  (0,0,k005,0)`, `row2 = 0`, then `A = A x Trans(0,0,k200 - E0[0])`, written to buf (r30 = 0x8060238C + 96*idx) [read].
  B (texmtx 57) at 0x801E4F64-0x801E4FCC: `k0625` on x and y, then `x Trans(r13-21928, r13-21924, 0)` [read]. The
  buffer is also set as array 23 (0x801E4FF0-0x801E5000) [read]; the game's one `GXLoadTexMtxIndx` (0x80193318) sets its
  own array 23 first [read there: refute-impl 8].
- The only `GXLoadTexMtxImm` (0x8021FC64) calls with 54 or 57 [read: all 14 call sites listed]: 0x8011A7AC/0x8011A7BC
  (the object draw: `A x model`, `B x model` built at 0x8011A748-0x8011A788 from buf), 0x801E74CC/0x801E74F0,
  0x801E7664/0x801E7688. Nothing else in the game loads 54 or 57.
- The scroll (`0x801E4AB0`, from frame begin 0x801E1188): only when E0 != 0 and `r13-21912`, `r13-21936` are 0;
  `r13-21928 += E0[+4]`, `r13-21924 += E0[+8]`, each less r2-19228 when above it (0x801E4B4C-0x801E4B90) [read]. The
  height ease (same function, 0x801E4AE4-0x801E4B48): with `r13-21920` = n frames left and `r13-21916` = target,
  `n -= 1`; n == 0: `E0[0] := target`, target := 0; else `E0[0] += (target - E0[0]) / n` [read]. `0x801E4A80(n, target)`
  only stores the pair [read]; its callers: 0x801521DC (every change: `(0, r2-22248)`), 0x80083244 (world animator),
  0x8016D0D0 (script fn124 `room_layer_height`, scripts L06-L16/2586 and others; not in chapter 1) [read: grep, `ed uses`].
- Draw order of a frame (0x800080E4-0x80008170) [read]: the gate, both opaque sections (0x801E851C: palette walk
  `0x801E0DFC(E0)` at 0x801E8530, then section 3 at 0x801E85C4 and 2 at 0x801E85CC), translucent sections
  (0x801E85E0: 3 at 0x801E86CC, 2 at 0x801E86D4), the actor list 0x80119D60 at 0x80008148, the full-screen quad
  0x801F164C when `r13-25676 == 0` (0x80008158-0x80008164), effects 0x8017C370 / 0x8014FC24 / 0x8017C37C.

### 1.2 The cached section lists (new)
- `0x801E75B4`: when `record+0x8138` (the cached list's size) is non-zero, it calls the cached list
  `GXCallDisplayList(record+288, size)` (0x801E7744-0x801E7750) and builds only a per-frame list of animated pieces into
  `record+296[r13-21908]` (0x801E775C-0x801E77AC). When it is 0 (the first draw) it records `record+288` (0x801E77B4-
  0x801E7830, size stored at 0x801E7830), then the translucent list `record+292` (size at `+304`, 0x801E7864-0x801E7880),
  then calls the new list and builds the per-frame one (0x801E7888-0x801E7924) [read].
- `0x801E5330` buckets a piece whose material `+24 & 0x1FFFFFFF` is 0 only while `+0x8138 == 0` (the compile), an
  animated piece only while it is non-zero (0x801E53A8-0x801E53EC) [read]. Every piece's material setup (`0x801E50C4 ->
  0x801E21FC(.., 691)` at 0x801E5270) runs while the list is being recorded [read there: refute-impl 1.1], so its TEV
  stages, the layer's on/off and the ramp's `GXInitTexObj` address are bytes in the list.
- The renderer replays those bytes as recorded: the game's own GXBeginDisplayList records into guest RAM, and the host's
  `GXCallDisplayList` hands the guest bytes to `dol_platform_call_display_list` (hle.c:2260-2315; aurora
  GXDispList.cpp:49-62 writes them to the FIFO) [read]. Only what the section draw loads outside the list changes per
  frame: texmtx 54/57, TLUT 0, texmap 7, K2 (0x801E13CC after the list).
- Writers of `+0x8138` [read: grep `stw ..,-32456(`]: the record's load (0x80151FB8, with `+304` at 0x80151FBC), the
  compile (0x801E7830), and `0x801E837C`, which clears BOTH slots' lists (0x801E84B0-0x801E84F0) when `r13-24640` is set.
  `r13-24640` is set by `0x801E8370` (from 0x801538D4) and at 0x8012AEE0, 0x8012AF14, 0x8012AF34 (`0x8012AEC0`, by
  arriving room number) and 0x8012D054 [read]. `0x801E837C` also rewrites `r13-26912` and calls 0x80045618 for both
  rooms (0x801E83A0-0x801E8474) [read]: not to be called by the host.
- So a section-3 list compiled while another room is current holds that room's gate and ramp until the room is
  reloaded or the game clears both lists [inferred from the above]. Whether s6's 180 (unpacked at 3822 into slot 0
  while player 2's change had 180 current for frames 3822-3823, then drawn as section 3 under 183 from 3824 [read:
  run-20261113-163109 page-0001.log]) was compiled under 183 or under itself is not known: the compile log (4) tells.

### 1.3 Actors (corrected)
- The 31 call sites of `0x801E21FC` [read: grep of `li r5` before each]: 24 pass 703, 3 pass 2751 (0x8018127C,
  0x801F1768, 0x801F43A4), the room pieces 691 (0x801E5270), `0x8011AB50` passes **0** (`li r5,0` at 0x8011AB38: no
  layer), 0x80119158 and 0x8011C11C a computed mask. Bit 9 (512) is the mist layer.
- An actor's layer: `0x8011A278` (object draw) -> 0x8011A880 `0x8011B814` -> 0x8011B92C `0x8011BE00` -> 0x8011C11C
  `0x801E21FC(.., r30)`, then the piece's geometry list by `GXCallDisplayList` at 0x8011B940 (per frame, outside any
  recording); and 0x8011A8B0/0x8011A8C4 `0x8011B97C` -> 0x8011BAC0 (703) [read]. All inside the object draw.
- `0x8011BE00`: r24 = the object (0x8011BE20; the object draw's r15 = its r3, 0x8011A298, passed at 0x8011A864) [read].
  r30 := 703 (0x8011BE50); `0x80153998(2)` at 0x8011BE58; if E and **`r13-23276` is 83, 99 or 213** and `E[0] < object
  z` (+8): bit 9 cleared (0x8011BE90) [read]. Bit 9 is touched nowhere else before `r5 := r30` at 0x8011C114 [read].

### 1.4 Effects, the overlay, the noise
- Effects (`0x801811EC` 703, `0x8018127C` 2751) are drawn after the actor list and load no texmtx 54/57 (1.1), so they
  use the LAST object draw's `A x model` / `B x model` — in solo too [read; inferred].
- `0x801F164C` (full-screen quad) is DRAWN every frame: `r13-25676` is 0 in slot1, rooms-ch1-183, rooms-ch1-182,
  jtg-ch1-room183, rooms-ch1-341-apart and coop-apart states [read: savestate.py]. It is not the pale mist: its colour
  `r13-21692` is 0x000000FF (black) in all six, it sets the mist gate `r13-22140 := 0` for itself (0x801F1678-0x801F167C)
  and puts it back (0x801F1898-0x801F189C), and blends `GXSetBlendMode(1, 7, 6)` (0x801F1740-0x801F1750) [read].
- The noise: `r13-21988` (its texture) is written once, at init (0x801E06D4) [read: grep], non-zero in all six states
  [read]. It is FILLED by `0x801E01D8` at a change into a room with E (0x80152168) and at a game load (0x80020700)
  [read]. At a change (0x80152154-0x80152170), when the arriving room has E: `0x801E0170(E)` resets the scroll
  (0x801E0188 -> 0x801E3B50), sets K2 (0x801E133C) and copies E+20/24/28/44 to the palette walk words; `0x801E01D8`
  makes a new noise and resets the scroll again (0x801E0208); `0x801E055C(E)` refills TLUT 0's history [read].

### 1.5 Host and renderer
- Host: `room_section_draw` (hle.c:13966-14024): section 3 under `lights_section(cpu,3)` and `room_mtx_load(cpu,0)`,
  the game's draw by `hle_guest_call` (14016); whole frames pass through to the game's own draw for the owner's room
  (13977-13991); "one room for both" returns before any work for section 2 (13999-14001) [read]. `effect_draw_by_room`
  (14048-14088): view 0 for `room == room1`, view 1 for room0 [read]. `lights_apart` (25949-25953): co-op on, no Tome
  copy turn, no whole owner, the split on, apart, room1's record loaded [read]. G1 on 0x80155130 is lr-gated to
  0x8002798C (25976-25981) [read]. The object draw's notify `coop_found_object_draw` installs only when the entry has no
  intercept and no notify (29969-29973); the kLights entries install only with no notify (30562) [read]. The dispatch
  runs an intercept, then on passthrough the notify (5627-5654); in the Tome copy turn it skips any entry whose hook is
  on the hook-off list (5623-5625) [read]. `coop_stay_stayer`/`coop_stay_spared` name the stayer of a change, player 1
  inside player 2's bracket (`g_stay_bracket`) (14125-14133) [read]. `hle_guest_call` takes up to 8 words (hle.h:109).
  Scratch: `SCRATCH_HUD` 0x81FFF000; +0xE00 and +0xE40 are taken (hle.c:13950, 14034), nothing else uses +0xE80..+0xFFF
  [read: grep].
- Cameras: `camera_player2_view(eye, target)` is player 2's view in world space, false while `g_p2_view` is false
  (camera.c:1567-1573). While player 2's close-up or lent cameras are up (`hle_coop_screen() ||
  hle_coop_cameras_lent()`, camera.c:1629-1666), C is player 2's camera, view 1 is `kSame`, view 0 is player 1's own
  camera sent as rot/position in C's view space, and `g_p2_view = false` [read]. Each view is sent to the renderer as
  `dol_aurora_split_set_view(player, rot[9], x, y, z, zoom, hide_hud)`: rot's columns are the view's right, up and back,
  x/y/z its position, both in C's view space (aurora.h comment above `aurora_split_set_view`) [read].
- Renderer: `build_view` (interp.cpp:582-723) copies the frame's uniforms per view and replaces only the projection
  (and HUD/flare cases), then zeroes the matrices marked out of the view [read]. `Vs.texmatrices` (3 rows per texgen,
  8 texgens) are copied unchanged into every view; between game frames they are only blended (interp.cpp:349-351, 424)
  [read]. So one draw's texmtx 54 serves both halves.

## 2. The refutations' points, checked

Verdicts: **fold** = real, folded into the design (where); **fix fact** = the first design's fact was wrong, corrected;
**limit** = real, named and logged, not fixed here; **not taken** = not borne out by the code.

| # | Point (source) | Checked here | Verdict |
|---|---|---|---|
| 1 | Cached section lists bake gate, ramp and bank at the compile (impl 1) | 0x801E7714-0x801E7924, 0x801E53A8-0x801E53EC, list replay hle.c:2260 / GXDispList.cpp [read] | fold: A4 (scoped compile, list hygiene), runs F6/F7 |
| 1b | s6's 180 was compiled under 183 (impl 1.2) | 180 was current at 3822-3823 in player 2's bracket [read log]: unproven | fix fact: unknown; the compile log settles it |
| 1c | `r13-24640` writers (impl 1.1) | also 0x8012D054 [read] | fix fact (1.2) |
| 1d | A list compiled outside a scope stays wrong even when its room is current later (new here) | section 2 never clears a list (1.2) [read]; together, no scope opens | fold: A4 checks section 2 as well |
| 2 | 0x801F164C is drawn, not gated off (impl 2, goal M2) | 0x80008158-0x80008164, six states [read] | fix fact (1.4); conclusion stands; colour logged (4) |
| 3 | Actors take the layer via 0x8011C11C/0x8011BAC0; 0x8011AB50 is mask 0 (impl 3) | 0x8011AB38, chain 0x8011A880-0x8011C11C [read] | fix fact (1.3); the actor scope still covers them |
| 3b | 0x8011BE58 runs inside the actor scope; rule by room NUMBER 83/99/213 (impl 3) | 0x8011BE50-0x8011BE90 [read] | fold: A2 whitelist, A3 the rule for room1 |
| 3c | Call-site masks "21 x 703, 2 x 2751" (impl 3) | 24 x 703, 3 x 2751, 691, 0, two computed [read] | fix fact (1.3) |
| 4 | Effects never load 54/57; they use the last actor's (impl 4) | the 14 GXLoadTexMtxImm sites [read] | fold: A7 (each room's last actor's matrices) |
| 5 | The noise test `r13-21988 != 0` never fails (impl 5) | one writer at init; non-zero in six states [read] | fold: a host "noise filled" flag (A6, A8) |
| 6 | Whole frames of player 2's draw room1 with room0's mist and can bake it (impl 6, goal M5) | hle.c:13977-13991 passes through; lights_apart false there [read] | fold for the list (A4); the look stays today's, like the lights (limit) |
| 7 | Lent / close-up cameras: player 1's half takes player 2's depth (impl 6, goal R4) | camera.c:1629-1666 [read] | fold: Part B (A5's fallback covers view 1) |
| 8 | Scroll, palette, K2 one per frame from room0 (impl 6, goal M1) | 0x801E4B4C-0x801E4B90, 0x801E8530 [read] | fold for the scroll (A6, any E1 whose speed differs); palette and K2: limit, logged |
| 9 | Height ease global (impl 6, goal R6) | 0x801E4A80-0x801E4B48, callers [read] | fold: A9 |
| 10 | 180's braziers unlit in the stayer's half (impl 6, goal R5) | s6 000014 right unlit, s11 000020 left lit [read pictures]; 0x80145A70 tests `r13-23276` [read] | limit: a separate item (effect steps by room), named to the user |
| 11 | Object-draw intercept install order (impl 7) | hle.c:29969-29973, 30562, 5627-5654 [read] | fold: A10 install notes |
| 12 | Cost of a hook on 0x80153998 (impl 7) | thousands of calls a frame [inferred] | fold: measured in the runs (frame time logged) |
| 13 | The copy turn skips the object-draw entry (impl 7) | hle.c:5623-5625 [read] | right; kept |
| 14 | Together, player 2's mist depth is player 1's camera (goal R1) | build_view replaces only projections [read] | fold: Part B |
| 15 | The stayer sees a pop at the other's door (goal R2) | depth source changes at the door (R1) and the noise is rebuilt (R3) [inferred from 7, 14, 16] | fold: Part B + A8; measured in F8 |
| 16 | Every door rebuilds the shared noise; the stayer's mist jumps (goal R3) | 0x80152154-0x80152170, 0x801E0188, 0x801E0208 [read] | fold: A8 |
| 17 | The proof's reference and measures (goal M3) | F2's "together" reference is itself wrong without Part B [inferred] | fold: solo reference F5b, frame-to-frame measures F8/F9 |
| 18 | Unmarked world drawers keep room0's mist in both halves (goal M4) | masks 703 at 0x800EBF50-0x800ED3E0, 0x801DC46C/0x801DC584 [read] | limit (Part B corrects their depth per view) |
| 19 | The camera build 0x801E4BDC may run inside an actor scope (goal M6) | its actor-list call 0x80119FB0 is between object draws [read 0x80119F80-0x80119FFC]; 0x80106114/0x80106A1C/0x8010F310 are table-reached, not on 0x8011A278's tree as read | not taken; guarded anyway: A2 never rewrites the build's ask (lr 0x801E4EC0) |
| 20 | Design 2.2's list of asks "outside any scope" (impl 3) | 0x8011BE58 is inside; 0x800206E4 was missing [read] | fix fact: A2 rewrites only listed returns |

## 3. Part A: the host, players apart

### A0. Predicate and install
`FA` = `lights_apart(cpu) && g_fog_rooms != 0`. `g_fog_rooms` from `ED_FOG_ROOMS` (unset = 1; "0" off; "2" records
only, no A'). Everything is installed only with ED_ROOM_SLOTS=2 and ED_COOP_STAY, beside the kLights table (hle.c:30547),
and each handler passes straight through unless `g_settings.mod_coop`. Solo, follow, together, the split off and the Tome
copy turn: no scope opens (the same gate as lights G1-G3, so the lights and the mist always agree on a thing's room).

### A1. The mist scope (new, beside `lights_section`)
```c
typedef struct { bool open; int slot; s32 room; u32 gate, bank, buf[24]; int prev_slot; s32 prev_room; } FogSave;
static int g_fog_slot = -1;     // the slot whose mist the game's asks get, or -1: the game's own
static s32 g_fog_room = -1;     // that slot's room (for A3's rule)
static bool fog_begin(CPUState* cpu, s32 room, FogSave* s);  // false: nothing changed
static void fog_end(CPUState* cpu, const FogSave* s);
```
`fog_begin(room)`, only when FA, `room == room1`, `room1 != room0`, `slot = room_slot_of(cpu, room1) >= 0`:
1. `E1 = 0x80153998(slot)` by `hle_guest_call` (the game's own ask: loaded test and override record included);
   `rec1 = room_slot_record(cpu, slot)`.
2. Save `r13-22144`, `r13-21808`, `g_fog_slot`, `g_fog_room`, and the 96 bytes of buf.
3. `r13-22144 := E1 != 0`; `r13-21808 := mem_read32(rec1 + 0x813C)`; `g_fog_slot := slot`; `g_fog_room := room1`.
4. If E1: write A' (A5) into buf (with `g_fog_rooms == 2`: the game's A rebuilt with C's row and E1's top, the band
   alone); write B' (A6) into buf+48 when room1 has its own scroll.

`fog_end` puts all of it back. Scopes nest (a weapon drawn inside its holder's draw): each saves what it found, so an
inner scope of the same room changes nothing and an end restores the outer state.

### A2. The game's record ask (new intercept on 0x80153998, first word 0x9421FFF0)
```c
static void fog_record_ask(CPUState* cpu) {
    g_hle_passthrough = true;                       // the game's own function always runs
    if (g_fog_slot < 0 || cpu->gpr[3] != 2u)
        return;
    switch (cpu->lr) {
    case 0x801E75D8u: case 0x801E7450u:             // the section draws (opaque, translucent)
    case 0x8011BE5Cu:                               // the actor rule's top test (A3 settles the room number)
        cpu->gpr[3] = (u32)g_fog_slot;
        return;
    case 0x801E2244u:                               // material setup: the opaque draw rewrote the bank at 0x801E75F8
        cpu->gpr[3] = (u32)g_fog_slot;
        mem_write32(cpu, cpu->gpr[13] - 21808u, mem_read32(cpu, room_slot_record(cpu, g_fog_slot) + 0x813Cu));
        return;
    default:                                        // the build 0x801E4EC0, the ease, the palette, the change: the game's
        return;
    }
}
```
The bank rewrite is needed because `0x801E75B4` writes the current bank at 0x801E75F8 after `fog_begin` [read]; it
lands before layer 9's read at 0x801E2AA4 in the same material setup [read]. G1's 0x80155130 intercept sees lr
0x801539C4 here, not 0x8002798C, and passes through [read hle.c:25978].

### A3. The actor rule for room1 (new intercept on 0x801E21FC, first word read at build, acts only at lr 0x8011C120)
Inside a scope (`g_fog_slot >= 0`), when `lr == 0x8011C120`: `obj = r24` of 0x8011BE00 = the object draw's object
(1.3); `rule = E1 && g_fog_room in {83, 99, 213} && E1[0] < f32(obj+8)`; `r5 := rule ? r5 & ~512 : r5 | 512`. Every
other call passes through untouched. This is the game's own rule (0x8011BE64-0x8011BE90) with room1's number; with A2
the game's own test already reads E1's top. Not seen in chapter 1; engine-generic.

### A4. The cached lists: compile in the right room, throw away the wrong ones (new)
State per slot: `g_fog_list_ok[2]` (the list was compiled with its own room's mist), set false at every state load
(`hle_coop_state_load`, hle.c:30817) and at first install.
- In `room_section_draw`, for `fn == ROOM_DRAW` only, BEFORE the "one room for both" return and the whole-frame branch:
  `rec = room_slot_record(section == 3 ? other : current)`; when `rec != 0` and `mem_read32(rec + 0x8138) == 0` this
  call compiles: mark `g_fog_list_ok[slot] = (section == 2) || (the scope will open for this call)`; log the compile (4).
  A section-2 compile is always its own room's (it is current); a section-3 compile is its own only inside a scope.
- When the list is about to be CALLED (`+0x8138 != 0`) and `g_fog_list_ok[slot]` is false: for section 3 only inside
  a scope (FA), for section 2 always (with `g_fog_rooms != 0`): clear `+0x8138` and `+304` of that record, exactly as
  the game's own clear does (0x801E84B0-0x801E84B8), and log it. The game then recompiles in this same call: section 3
  inside the scope, section 2 as its own room. One recompile per bad list.
- The translucent pass needs nothing: its cached list (`+292`) is recorded by the opaque compile (1.2), which runs
  first in the frame (0x80008118 before 0x80008140) [read].
- The game's own clears (load, `0x801E837C`) set `+0x8138` to 0, so the next compile is seen and marked.

### A5. A': room1's things measured from player 2's camera
From `camera_player2_view(eye2, target2)`: `z = normalize(eye2 - target2)`, `r = (z.x, z.y, z.z, -dot(z, eye2))` (row 2 of
a LookAt, as C's row 2 is; the up vector does not enter row 2 [read camera.c:1577-1581]); `d = k200 - E1[0]`:
```
A'[0] = { -r0/k2000, -r1/k2000, -r2/k2000, (-r3 - r2*d)/k2000 }
A'[1] = { 0, 0, k005, k005*d }
A'[2] = { 0, 0, 0, 0 }
```
This is the game's build (1.1) with player 2's camera row for C's. When `camera_player2_view` is false (player 2's
close-up or lent cameras: C is player 2's camera, view 1 is `kSame`), use C's own row 2 (`0x8060248C + 32..44`): right
for view 1. Player 1's half in that branch is Part B's (7 in the table).

### A6. B', the scroll and the palette for room1
Host scroll pair `(sx1, sy1)`, used only when E1 != 0 and (E0 == 0 or `E1[+4], E1[+8]` differ from E0's): once a frame
from `room_slots_frame` while FA, when `r13-21912` and `r13-21936` are 0 (the game's own gates, 0x801E4ABC-0x801E4AD0):
`sx1 += E1[+4]`, `sy1 += E1[+8]`, each less r2-19228 when above it (the game's wrap, 0x801E4B4C-0x801E4B90). B' = the
game's B with `(sx1, sy1)`: `k0625` at [0][0] and [1][1], translation `(k0625*sx1, k0625*sy1, 0)`. When E0 == 0, also
call the game's palette step `0x801E0DFC(E1)` once a frame (the opaque pass does it for E0 at 0x801E8530) [read]. The
noise: when E0 == 0 and no noise was ever filled (`g_fog_noise_filled`, set by a notify on 0x801E01D8 from any caller,
false at state load), the scope sets the gate to 0 for room1 and logs it (no layer rather than an unfilled texture).
When E0 != 0 and the scroll speeds match, B, the scroll and the palette stay the game's (one noise for both halves;
180, 182, 183 share them [read there: reader A 4]). Palette limits (E+12..+44) that differ between the two rooms and a
tinted K2 (`r13-21956`, tinted only for room 83 by 0x8007CCF0 [read there]) stay one per frame: logged (limit).

### A7. Effects: each room's last actor's matrices (new)
- A notify on `GXLoadTexMtxImm` (0x8021FC64; a new `kAddrHandlers` entry by name, under ED_AURORA, beside
  `GXLoadPosMtxImm`, hle.c:5287) that acts only at lr 0x8011A7B0 (54) and 0x8011A7C0 (57): copy the 48 bytes at r3 into
  `g_fog_last[r][0/1]` with `r = (g_fog_slot >= 0) ? 1 : 0` and mark them this frame's.
- In `effect_draw_by_room`, when FA and the effect's room is room0 or room1 (views 1 and 0): if that room's last
  matrices are this frame's, write them to `SCRATCH_HUD + 0xE80` (54) and `+0xEB0` (57) and load them by
  `hle_guest_call(GXLoadTexMtxImm, {at, 54, 0})` and `({at+48, 57, 0})` before the effect's own draw; for room1's
  effects also `fog_begin` / `fog_end` round the draw (records). This gives each effect what solo gives it: the last
  actor of its own room's matrices.

### A8. The stayer's mist kept through the other's door (new; goal R3)
At a co-op change with a stayer (`coop_stay_spared(cpu, coop_stay_stayer(cpu))`), when the stayer's room has E (`0x80153998(
room_slot_of(stayer's room))`, asked before the change's own ask) and `g_fog_noise_filled`:
- intercept `0x801E0170` at lr 0x80152164: save `r13-21928`, `r13-21924`; pass through (K2 and the palette words come
  as the game sets them);
- intercept `0x801E01D8` at lr 0x8015216C: skip it (no new noise; `hle_set_u32(0)`, no passthrough: the caller ignores
  r3 [read 0x8015216C]) and put the saved scroll pair back;
- `0x801E055C` at lr 0x80152174: pass through when the arriving E's palette words (+12..+44) differ from the stayer's
  (the arriving room's own palette has to come; logged), else skip.
The mover is under the door's fade, so they see no difference; the stayer's mist does not jump. Any other caller and
any change without a stayer: the game's. ED_FOG_KEEP=0 turns it off.

### A9. The mist height per room (new; goal R6)
Intercept `0x801E4A80` (first word `stw r3,-21920(r13)`): apart (`coop_stay_apart`), inside player 2's turn
(`g_coop_turn`: `r13-23276` and the slot table are player 2's) or at the change's reset (lr 0x801521E0) inside player
2's bracket (`g_stay_bracket != 0`): store `(n, target)` into the host pair H1 for room1 and do NOT pass through (the
game's pair stays player 1's room's). Everything else: the game's. Once a frame (`room_slots_frame`), when H1.n > 0 and
`r13-21912`, `r13-21936` are 0: the game's step on E1[0] (1.1). H1 is dropped when room1 == room0 (together) or room1
changes. Not reachable in chapter 1 (no fn124 caller); logged.

### A10. Where scopes open (sites and install)
- **Sections** (`room_section_draw`): in the `section == 3u` branch, after `lights_section(cpu,3u)` and
  `room_mtx_load(cpu,0)`: A4's check, `fog_begin(cpu, room1, &s)`; after `hle_guest_call` and before
  `room_mtx_load(cpu,-1)`: `fog_end`. Both passes (ROOM_DRAW, ROOM_DRAW_ALPHA share the function). Section 2: A4's check
  only, no scope; it reloads 54/57 from the game's own buffer when E0 != 0, so nothing of A' outlives section 3 [read
  0x801E7648]. When E0 == 0, XF 54 keeps A' after section 3, but room0's things then have gate 0 (no layer) [inferred].
- **Actors** (new intercept on `COOP_OBJECT_DRAW` 0x8011A278, set AFTER the notify install at hle.c:29971, which
  refuses an entry with an intercept; it must not copy the kLights "no notify" test): a `running` guard as in
  `room_section_draw`; if not FA, or the object's +592 is not room1, or running: passthrough (the notify and the game's
  draw run as today). Else `fog_begin`, `hle_guest_call(0x8011A278, r3..r8, 6)` (the inner dispatch passes through and
  runs the notify, which marks the object as today), `fog_end`, `hle_set_u32(out)`, no passthrough. The object draw then
  builds `A' x model` at 0x8011A764 and its pieces' materials under the scope (1.3). Player 2's body, weapon and room1's
  creatures get room1's mist from player 2's eye. Add the intercept to the Tome copy turn's hook-off list too (it is
  skipped with the notify today, hle.c:5623, but the list is the safe place).
- **Effects** (`effect_draw_by_room`): A7.

### A11. What stays as the game draws it
- Section 2, player 1's actors and effects: the game's own inputs (room0 is the room in play).
- Whole frames (Tome copy turn, the shared Tome's frames): player 2's room shows room0's mist, as its lights do today
  (`lights_apart` is false there). Only the list is guarded (A4). A limit, logged if seen.
- The unmarked world drawers (0x800EBF50-0x800ED3E0, 0x801DC46C/0x801DC584): room0's record in both halves (limit).
- The full-screen quad's colour `r13-21692`: one per frame, black in every state read; logged.
- 180's flames and floor glow missing in the stayer's half (effect steps keyed to `r13-23276`, 0x80145A70 [read],
  0x8018ED60 [read there: reader B S7]): not this layer. A separate item, and the user should be told it is not fixed
  by this one.

## 4. Part B: the depth per view, in the renderer (together and apart)

Why: a draw's texture matrices are the same in both views (1.5), and layer 9's depth (texmtx 54 row 0) comes from C.
Together, player 2's half takes player 1's camera depth (goal R1); while player 2's close-up holds C, player 1's half
takes player 2's camera depth (goal R4); and the stayer's depth source changes at the other's door (goal R2). A host
scope cannot fix a draw both views share.

The math [inferred from 1.1; checked by the self-test in 5]. For a draw with position matrix P (= C x M; the sections'
M is the identity) and the mist texgen's row 0 `t = -(row2(C x T(d) x M))/k2000`:
`row2(C x T(d) x M) = P[2] + (0, 0, 0, d*C[2][2])` (T adds d along world z; M's last row is 0 0 0 1). A view v whose
camera, in C's view space, has back axis `b` (rot column 2: rot[2], rot[5], rot[8]) and position `p` sees eye depth
`b.(e - p)`, so its row is `t_v[i] = -(b.P[:,i] + (i == 3 ? -b.p + d*(b.c2) : 0))/k2000`, with `c2` = C's column 2
(world z in C's view space: C[0][2], C[1][2], C[2][2]). View 0 with `rot = I, p = 0` gives back t.

Build:
- Host, once a frame in `camera_split_update` (camera.c, beside the `dol_aurora_split_set_view` calls, so it is in the
  stream with the frame's draws like them): `dol_aurora_mist_set(on, c2[3], d0, d1, k2000)` with `d0 = k200 - E0[0]`,
  `d1 = k200 - E1[0]` (NaN when that room has no E or when together `d1 = d0`), `on = ED_FOG_VIEWS != 0 && mod_coop &&
  the split on`. A new op beside `split_set_view_op` (aurora_backend.cpp:2488) and its ring item.
- Renderer, in `build_view` before the marks loop (it needs P before a mark zeroes it): for each distinct `d.vs`
  offset, for each texgen g, `t = src.texmatrices[3*g]`, `P = src.posnormalmatrix`: it is the mist row when
  `|t[i] + P[2][i]/k| <= 1e-3*(|P[2][i]|/k + 1e-6)` for i = 0..2; its room is the one whose
  `|t[3] + (P[2][3] + dR*c2[2])/k|` is within the same tolerance (d0 first). Then write `t_v` into `dst.texmatrices[3*g]`
  for this view (rows 1 and 2 unchanged: the band is world height). A draw that does not match (an effect using the
  last actor's matrix, a room1 draw already given A' from player 2's eye) is left as it is. The views' `rot/p` are the
  ones `aurora_split_set_view` stored for that player.
- With Part A: room1 draws carry A' (player 2's eye) and do not match, so they are left; when A' fell back to C (lent),
  they match and view 1 (`kSame`) leaves them equal. Room0 draws match and get each view's depth: player 1's own
  camera in view 0 while lent. So Part B supersedes nothing of A and fixes 7, 14 and the depth half of 15.
- Patch file: rebuild with the VS2022 tools, then `git -C .deps/GXRuntime diff > patches/gxruntime/eternal-darkness.patch`.

## 5. Logging (`ED_FOG_LOG=1`, also on under ED_COOP_LOG)
- `[fog] frame F: room in play R0 E top T0 ramp I0 scroll X0,Y0 (or none) | room1 R1 slot S top T1 ramp I1 (or none) |
  overlay colour r13-21692, r13-25676 | rule on|off|records-only`: when a room, E or the rule changes, then every 600
  frames.
- `[fog] list: room R (slot S) compiled as section 2|3 [in its own mist|under room R0's]` at each compile (A4), and
  `[fog] list: room R (slot S) compiled outside its mist: cleared (frame F)`.
- `[fog] frame F: section 3 (room R1) in its own mist: gate G, bank 0x..., A' row0 (a,b,c,d) row1 (..) from player 2's
  eye (x,y,z)->(x,y,z) | the game's A row0 (..); N actors, M effects of R1 in its mist since the last line`.
- `[fog] loaded: texmtx 54 row0 (..) at lr 0x801E7668 in section 3`, once per change (A7's notify, inside a section-3
  scope only): proof the matrix reached the GX call.
- `[fog] self-test`, once per room change while FA: A by the host's formula from C's row 2 vs the game's buf A (D1);
  player 1's camera row from `hle_coop_player1_camera` vs C's row 2 when not lent (D2). Both under 1e-3; a larger D2
  means A5's LookAt reading is wrong. Part B: `[fog-view] frame F: N mist rows rewritten (view 0 a, view 1 b), M not
  matched` every 600 frames from the renderer.
- `[fog] door: the stayer's (room R) noise and scroll kept; palette kept|taken from R'` (A8); `[fog] height: room R1
  eases to T over N frames (fn124|animator|reset)` (A9); `[fog] skipped: ...` (no noise yet, gate with E = 0, record not
  loaded, palette or K2 differ), up to 8 lines each.
- The frame time: `ed timeline` already gives it; A2/A3 add one host call per call of two busy functions.

## 6. What it must not break
- **Lights (G1-G3)**: scopes open after `lights_section(cpu,3)` and close before `room_mtx_load(-1)`; no light word is
  written. G1's intercept is lr-gated to 0x8002798C.
- **Shadows, lamp haze, fade marks**: no position matrix changes; only texmtx 54/57 (and Part B's copies of them), three
  SDA words, a record's two list sizes (the game's own clear) and, at a stayer's door, the noise refill.
- **Found/unseen marks, effects by room**: the object and effect marks are made inside the inner calls as today.
- **The Tome copy, whole frames, cutscenes, the split off**: `lights_apart` is false; scopes do not open. A4's
  bookkeeping runs (it only marks and, outside a scope, clears section-2 lists that are its own room's after a bad
  compile, which is the game's own first-draw path).
- **A room change**: the game's mist setup runs outside any scope (A2 never rewrites the change's ask; it passes the
  slot index, not 2 [read 0x80152148-0x80152154]). Buffers are restored after each scope.
- **Solo**: nothing installed without ED_ROOM_SLOTS=2 and ED_COOP_STAY; every handler passes through without mod_coop;
  Part B's op is sent only with co-op and the split on.

## 7. Switches
`ED_FOG_ROOMS` 0 off (today's, the baseline), unset/1 on, 2 records only (no A'). `ED_FOG_LISTS=0` turns A4 off (to
show the baked lists). `ED_FOG_KEEP=0` turns A8 off. `ED_FOG_VIEWS=1` turns Part B on (off until its runs pass).
`ED_FOG_LOG=1` for section 5.

## 8. Runs (fresh Jump-to-Game state, as build-run-stayer-presses.sh makes it; one state for all)
```sh
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_RUN_SHOTS_MS=250 ED_FOG_LOG=1
L="ED_COOP_LOG=1 ED_STAYER_LOG=1"
rm -f saves/rooms-ch1-183.state saves/fog-apart-off.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 \
  ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 fog-s0-183 </dev/null
SET=$(ls -d logs/run-*-jtg-fog-s0-183 | tail -1)/settings.ini; export ED_SETTINGS="$SET"; ST=saves/rooms-ch1-183.state
P1GO="ED_RUN_SCRIPT=110:119,170:119"     # player 1's door 183 -> 180, ticket ~3822
D2="3760,3770,b"                          # player 2's OPEN DOOR at 183's door to 180
for F in 0 1 2; do
  # F1: player 1 in 183's pit, player 2 to 180 (183 in play, 180 is room1).
  env $L ED_FOG_ROOMS=$F ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" \
    ED_PAD2_SCRIPT="$D2" sh tools/ed/run.sh $ST fog-f1-p1-haze-$F </dev/null
  # F2: player 2 in 183's pit, player 1 to 180 (180 in play, 183 is room1).
  env $L $P1GO ED_FOG_ROOMS=$F ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1250,1650,185" \
    sh tools/ed/run.sh $ST fog-f2-p2-haze-$F </dev/null
done
# F3 together in 183's pit; F4 follow into 180: on and off must match (no scope); Part B changes player 2's half only.
for V in 0 1; do
  env $L ED_FOG_VIEWS=$V ED_RUN_FRAMES=4100 ED_PUT="3680,1,1305,1508,185;3680,2,1250,1650,185" sh tools/ed/run.sh $ST fog-f3-together-183-v$V </dev/null
done
env -u ED_COOP_STAY $L $P1GO ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1508,185" sh tools/ed/run.sh $ST fog-f4-follow-180 </dev/null
# F6 a state saved apart with the fix off (lists baked outside a scope), then loaded with it on: one "cleared" line.
env $L ED_FOG_ROOMS=0 ED_RUN_FRAMES=4060 ED_SAVE_STATE=4000:saves/fog-apart-off.state \
  ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" sh tools/ed/run.sh $ST fog-f6a-save-off </dev/null
env $L ED_FOG_ROOMS=1 ED_RUN_FRAMES=4300 sh tools/ed/run.sh saves/fog-apart-off.state fog-f6b-load-on </dev/null
# F7 the order turned: player 2 into 180 first, player 1 follows (together in 180), player 1 back to 183 (apart again).
for F in 0 1; do
  env $L ED_FOG_ROOMS=$F ED_RUN_FRAMES=4600 ED_PUT="3680,1,1110,1170,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" \
    ED_RUN_SCRIPT=250:119,310:119,600:29,660:29 sh tools/ed/run.sh $ST fog-f7-order-$F </dev/null
done
# F8 the stayer's mist across the other's door (A8): player 2 still in 183's pit, player 1 leaves; shots every 50 ms.
for K in 0 1; do
  env $L $P1GO ED_FOG_KEEP=$K ED_RUN_SHOTS_MS=50 ED_RUN_FRAMES=3950 ED_PUT="3680,1,1110,1170,185;3680,2,1250,1650,185" \
    sh tools/ed/run.sh $ST fog-f8-keep-$K </dev/null
done
# F9 player 2's examine close-up while apart (lent cameras): player 1's half (180) must not change with it (Part B).
for V in 0 1; do
  env $L $P1GO ED_FOG_VIEWS=$V ED_RUN_SHOTS_MS=50 ED_RUN_FRAMES=4100 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1508,185" \
    ED_PAD2_SCRIPT="3900,3906,b;4000,4006,b" sh tools/ed/run.sh $ST fog-f9-lent-v$V </dev/null
done
# F5 solo, unchanged (no [fog] line); F5b solo reference: the hero where F2's player 2 stands (183's pit).
unset ED_SETTINGS
JTG_SLOT=1 JTG_SET="mod_coop=0" env $L ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29,3820:119,3880:119 \
  ED_RUN_FRAMES=4300 sh tools/ed/jtg.sh 1 fog-f5-solo </dev/null
JTG_SLOT=1 JTG_SET="mod_coop=0" env $L ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 \
  ED_PUT="3680,1,1250,1650,185" ED_RUN_FRAMES=4000 sh tools/ed/jtg.sh 1 fog-f5b-solo-183-pit </dev/null
```
Also with ED_FOG_ROOMS=1: stayer S3 (player 2's copy Tome apart) and S8 (player 1's) from build-run-stayer-presses.sh
(no scope may open during the copy: the section-3 count is 0 then), and `ed chapters 1` with ED_FOG_VIEWS=0 and 1
(`--compare`), since Part B is a graphics change. Then `ed test`.

For each run: `ed timeline RUN`, `ed check RUN`, `grep "\[fog" RUN/page-*.log`, `ed sheet RUN` of the settled frames
(about 3950-4250), and the numbers in 9.

## 9. Pictures and numbers: what proves it
Measure per half (the picture scaled to 960x270 grey, ffmpeg): `low3 p5` (the lower third's 5th-percentile luma: the
black floor the mist lifts) and its mean; for F8/F9 also `jump` = the half's mean absolute difference between two
consecutive shots, against that half's median over the run.

Today (the off baseline), measured from existing runs [read: ffmpeg on the shots, first design]:

| Picture | Half | Room / mist drawn | low3 p5 | low3 mean |
|---|---|---|---|---|
| lights2-on 000008 (together, 183 in play) | L / R | 183 pit, own | 37 / 37 | 66.5 / 66.6 |
| s6 000014, 000030 (apart, 183 in play) | L | 183 pit, own | 39 / 39 | 68.8 / 66.6 |
| s6 000014, 000030 | R | 180 under 183's | **16 / 17** | 45.5 / 38.8 |
| s11 000020 (apart, 180 in play) | L | 180, own | 5 | 30.7 |
| s13 000021 (follow, 180) | L / R | 180, own | 5 / 5 | 29.3 / 29.3 |
| lights2-on 000024 (apart, 180 in play) | R | 183 pit under 180's | **2** | 21.9 |
| solo s14 000216 | whole | 180, own | 2-3 | 19.5-26.2 |

Pass:
- F1 (ROOMS=1): right half low3 p5 within 3 of F4's (180's own, about 5), not 16-17; left as F1-0 (37-39).
- F2 (ROOMS=1): right half low3 p5 in the 30s, within 5 of F5b's (the hero's own view of 183's pit; the camera
  differs, so the band is wide); not 2. Left as F2-0 (about 5).
- F1-0 / F2-0 reproduce the baseline: the switch is the only difference (`ed diff`).
- ROOMS=2: right halves move most of the way; what remains follows player 1's camera (the depth half).
- F3, F4: ROOMS on and off equal. F3-v1: the left half unchanged from F3-v0; the right half's mist no longer follows
  player 1's moves (read the sheet; low3 p5 within 5 of F5b).
- F5: no `[fog]` line. F5b: the reference numbers.
- F6b: exactly one `list ... cleared` line for room 180 (slot 0), then its right half equals F1-1's (within 3).
  (Amended in review, fog-impl-notes.md "Review fixes" 4: a load makes both slots' verdicts unknown and A4 clears a
  section-2 list whose verdict is unknown, so one `cleared` line per resident slot (180 and 183), then none.)
- F7: the compile lines show where each list was made; with ROOMS=1 no list is "under room R0's" and 180's half
  together and apart matches F4/F1-1; with ROOMS=0 the log shows whether a list baked under 183 is drawn in 180 together
  (a today's bug to report if so).
- F8: with KEEP=1, the right half's `jump` at the ticket (~3822) is within 2x its median; with KEEP=0, a spike there.
  Log: one `door: ... kept` line.
- F9: with VIEWS=1, the left half's `jump` at player 2's examine (~3900) and return (~4000) is within 2x its median;
  with VIEWS=0, a change of the floor mist there. Log: `[fog-view]` counts non-zero.
- Logs: self-test D1, D2 under 1e-3; the `loaded` row0 equals A' row0; section-3 and actor counts non-zero apart, zero
  together and in solo; frame time within 2% of the off run (`ed timeline`).
- Sheets for both players: 183's mist shows in player 2's half while player 1 is in 180, and is gone from 180's half
  while player 1 is in 183; nothing pops when either player moves or opens a door.

## 10. Not read / open
- The ramp textures (banks 67, 88, 107) and their wrap: why 180's band leaves 183's pit clear [inferred].
- 344's record; `0x801538B8`/`0x80153954` (the override record and bank `r13-21812`): the scope keeps whatever they say.
- Where `record+124` is filled at a load (reader A 6).
- Whether the shared Tome's or the copy's whole frames show the world at all (A11).
- `0x8018ED60`'s room test (reader B S7, read there only) and the brazier item (A11).
- Part B's tolerance on blended (in-between) frames: P and t are blended alike, so the match should hold [inferred];
  the `[fog-view]` "not matched" count shows it.
- Whether the renderer's `Vs.texmatrices` row layout for a 2x4 texgen is rows 0-1 of the 3 (read gxcore_draw before
  writing Part B).
