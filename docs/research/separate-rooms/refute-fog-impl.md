# Refuting design-fog.md against the current code (2026-11-13)

Item: the user's report (2026-11-13), players apart (ED_ROOM_SLOTS=2, ED_COOP_STAY=1): "fog from the block room is seen in
the other player's screen (the room right next to the block room) where fog is not present, and vice versa ... depending
on which player leaves the room, the fog changes." Under review: `design-fog.md` (the room mist per room), with its readers
`fog-game-readers.md` and `fog-host-readers.md`. Read only: nothing edited, built or run; the game was not launched.
Tools: `ed dis`, `ed fn`, `build/ed-db/disasm.txt`, `generated/symbols.tsv`, `tools/ed/savestate.py` on saves/, the run
logs and pictures named below. Tags: [read] = seen at the address, file:line, log or picture given; [read there] = from
the doc named; [inferred] = reasoning from what was read.

Verdicts: **[real]** = the code shows the design is wrong or misses a path; **[maybe]** = a risk the code makes possible
that the design does not handle, not proven to show. What the design gets right is listed at the end.

## 0. Short answers

1. The design's core (the mist is the game's own layer 9; its band and depth come from texmtx 54 built from the room in
   play's E and the game camera; apart, the room in play is player 1's) holds [read: 0x801E4EB8-0x801E4F60,
   0x801E75D4-0x801E7698, 0x801E21FC-0x801E2274, 0x801E2AA4]. The cited hle.c lines are current (room_section_draw
   13966-14024, effect_draw_by_room 14048-14088, lights_apart 25949-25953, G1 25976-25981, the object notify 3366 and its
   install 29970-29973, kLights 30551) [read].
2. **It misses the room sections' cached display lists** [real]. Most of a room's pieces are compiled ONCE into
   `record+288` (opaque) and `record+292` (translucent) at the section's first draw after a load or an invalidation, and
   only animated pieces are rebuilt per frame. Layer 9's gate, its ramp index (material+18) and the ramp texture's
   address (bank `r13-21808`) are BAKED into those lists from whatever room was current at the compile. The design treats
   them as per-frame inputs for every piece. So the scope fixes static pieces only when their compile happens inside it;
   a list compiled outside a scope keeps the other room's ramp and gate until the room is reloaded or the game
   invalidates both slots. Section 1.
3. **`0x801F164C` is not gated off** [real]: `r13-25676` is 0 in every save read, so the full-screen quad is drawn every
   frame. It is still not the pale mist (black, destination-alpha blend, its own mist gate forced off), so the
   design's conclusion stands on different grounds. Section 2.
4. **The actor path is misread** [real]: `0x8011AB50` passes mask 0 (no mist layer). Actors take layer 9 through
   `0x8011C11C` (in `0x8011BE00`) and `0x8011BAC0`, and `0x8011BE58`'s mist ask runs INSIDE the object draw, so inside the
   design's actor scope, contrary to 2.2. That ask feeds a rule keyed to the room in play's NUMBER (83/99/213) [maybe,
   engine-generic]. Section 3.
5. **Effects never load texmtx 54/57** [real]: only three sites in the game do. A room-1 effect in the design's effect
   scope gets room 1's ramp/gate with the LAST object draw's `A x model` matrix, which can be a room-0 actor's. Section 4.
6. Smaller: the noise test `r13-21988 != 0` is always true after init [real]; view 0 is not always right (player 2's
   close-up or lent cameras) [maybe]; whole frames of player 2's are left to room 0's mist and can bake it [maybe];
   per-room scroll/palette and the height ease stay room 0's [maybe]; an install-order trap on 0x8011A278 [maybe].
   Sections 5-9.

## 1. [real] The cached section lists: the gate, ramp and bank are baked at the compile

### 1.1 The code
- `0x801E75B4` (opaque section draw) after the matrix loads: `if record+0x8138 != 0` (the cached list's size,
  `addis r3,r31,1; lwz r0,-32456(r3)` at 0x801E7714-0x801E7720) it calls the cached list
  `GXCallDisplayList(record+288, size)` (0x801E7744-0x801E7750) and then builds ONLY the per-frame list
  (`GXBeginDisplayList(record+296[r13-21908], 20000)`, `0x801E53FC(record, record+28, 0)`, End, Call; 0x801E775C-0x801E77AC)
  [read].
- Else (first draw): `GXBeginDisplayList(record+288, 0x51CA0)` (0x801E77C4-0x801E77CC), `memset(record+312, 0, 0x8000)`,
  `0x801E5330(record)` (fill the buckets), `0x801E53FC(record, .., 0)`, `GXEndDisplayList` -> size stored at `+0x8138`
  (0x801E7828-0x801E7830); then the translucent list `record+292` with `0x801E53FC(.., 1)` -> size at `+304`
  (0x801E7864-0x801E7880); then the buckets are filled again and the per-frame list is built (0x801E7898-0x801E7924)
  [read]. `GXBeginDisplayList` = 0x8021F7C8, `GXCallDisplayList` = 0x8021F964 [read symbols.tsv].
- `0x801E5330`: a piece whose material `+24 & 0x1FFFFFFF` is 0 is bucketed only while `+0x8138 == 0` (the compile); a
  piece with those bits is bucketed only while `+0x8138 != 0` (0x801E53A8-0x801E53EC) [read]. So **static pieces are in the
  cached list, animated pieces in the per-frame list**.
- `0x801E73D4` (translucent) builds a per-frame list of the animated pieces (0x801E7540-0x801E7570) and calls the cached
  `record+292` (0x801E7578-0x801E7588) [read].
- Every piece goes through `0x801E50C4` -> `0x801E21FC(material, .., 691)` (0x801E5270) inside the list being recorded
  [read]. Layer 9 there takes `material+18 := gate ? E+36 : -1` (0x801E2224-0x801E2274), `GXInitTexObj` from bank
  `r13-21808` (0x801569F8 -> 0x8021C91C at 0x80156A58) and `GXLoadTexObj` (0x801E2CCC), all written into the list
  [read]. The renderer decodes a called list's bytes (`command_processor.cpp:447`, `CP_CMD_CALL_DL`;
  `aurora_backend.cpp:2060`) [read]: **the texture address, the TEV stages and the gate are fixed in the list**. Only
  what the section draw loads outside the list changes per frame: texmtx 54/57 (0x801E7664, 0x801E7688), TLUT 0
  (0x801E7644), texmap 7 (0x801E7698), K2 (0x801E13CC after the list).
- Invalidation (only these): the record's load clears `+0x8138` and `+304` (0x80151FB8-0x80151FBC); `0x801E837C`, when
  `r13-24640` is set, clears both for slot 0 AND slot 1 (0x801E84B0-0x801E84F0) [read]. `r13-24640` is set by
  `0x801E8370` (from `0x801538C0`, the override record word `r13-23372` that `0x801538B8` returns: called from 0x80021B60,
  0x80021DC8, 0x80021F9C, 0x80022194, 0x80022218, 0x800167AC, 0x80050570, 0x8006CE68, 0x80153908) and by `0x8012AEC0`
  (by an arriving room NUMBER: 174, 336/24, else; it also sets `r13-29280` and `0x803011FC+56`) [read]. Nothing else
  writes `+0x8138` [read: grep of `stw ..,-32456(`].

### 1.2 What it means for the report and for the design
- In the s6 run the 180 that player 2 walks into is unpacked fresh into slot 0 at frame 3822 and drawn only as section
  3 with 183 current from 3824 on (`[rooms] unpack asked: room 180 into slot 0`, `[stay] ... the room in play is 183`,
  run-20261113-163109 page-0001.log [read]). Its static list was therefore compiled with 183's gate and 183's ramp
  (index 107 from 183's bank) [inferred from 1.1]. Its veil (s6 shots 000014/000030, right) is 183's ramp texture in 183's
  band from player 1's eye, not only 183's band.
- In s11 and lights2-on, 183 is the save's current room (compiled own) and 180 becomes current at the change (compiled
  own) (`unpack asked: room 180 into slot 0`, then `the room in play is 180`, both logs [read]). There, 183's missing
  mist (lights2-on 000024 right; s11 000020 right) is texmtx 54/57 and the load gate alone [inferred].
- So the design's 0.3 ("every input of that layer is read from the room in play, once, and used for BOTH room
  sections") is wrong for static pieces: the ramp and the gate are the COMPILE room's. The design's fix still works on
  them **only if the compile runs inside the section-3 scope** (then 2.2's bank rewrite at lr 0x801E2244 and the gate are
  baked right). The design never says so, and has no answer for a list compiled outside a scope:
  - a save state made apart before the fix (its lists are in guest RAM; e.g. saves/rooms-ch1-341-apart.state),
  - a section-3 compile in a frame the scope does not open (section 6: player 2's whole frames),
  - `ED_FOG_ROOMS=2` ("records only", design 5): for static pieces it changes nothing a list already holds; the switch
    cannot separate the record half from the camera half as 7 says.
- A compile outside a scope also bakes a texture ADDRESS inside the room in play's slot memory. When that slot is later
  reused for another room, room 1's cached list points into that room's data: a wrong or garbage ramp until room 1 is
  reloaded [inferred; today's latent hazard with two slots, the same mechanism].
- Needed in the design: (a) state that the compile must be scoped and check it (log the `+0x8138` 0 -> non-zero edge of
  each slot with the room current and whether a scope was open); (b) when a scope opens for a room whose list was compiled
  outside one (or at the first scope after a state load), clear that record's `+0x8138` and `+304` as the game itself does
  at 0x801E84B0-0x801E84B8, so the next section-3 draw recompiles inside the scope. Do not call `0x801E8370`: its
  consumer `0x801E837C` also clears `r13-22136`, recomputes `r13-26912` from `r13-29276..-29268` and calls 0x80045618
  for both rooms (0x801E839C-0x801E8474) [read].
- The tests in design 6 do not reach this: F1's 180 is a fresh unpack (compiled under the scope), and F2's rooms are each
  compiled while current [inferred from the save's slots, fog-game-readers 4]. A run that loads a state saved apart, and
  one that toggles the order (player 2 enters the new room first, player 1 follows, then leaves), are needed.

## 2. [real] 0x801F164C is drawn every frame (the design says gated off)
- `0x80008158: bl 0x800442F4` (which returns `r13-25676`, 0x800442F4); `bne` skips; else `bl 0x801F164C` (0x80008160-
  0x80008164) [read]. `r13-25676 = 0` in slot1.state, rooms-ch1-183.state, rooms-ch1-182.state, jtg-ch1-room183.state and
  rooms-ch1-341-apart.state [read: savestate.py, `State.sda(-25676)`]. So the quad is **drawn**, not gated off as design 0.5
  says.
- Why it is still not the pale mist [read]: the colour is `r13-21692` = 0x000000FF (black) in all five saves, alpha forced
  to 105 (0x801F1668-0x801F1688); it sets the mist gate `r13-22140 := 0` for its own material (0x801F167C) and puts it
  back after (0x801F1898); it blends `GXSetBlendMode(1, 7, 6)` = INVDSTALPHA / DSTALPHA (0x801F1750) over a
  +-10240 x +-7680 quad. Layer 9 itself leaves alpha untouched (its alpha stages take the previous alpha:
  0x801E34F4-0x801E3524, 0x801E3570-0x801E35A0). A black destination-alpha pass is the same colour in both halves.
- Left open: its colour is one global set from the world animator by the room in play (0x801F1434, reader B S1 [read
  there]); a level that tints it per room shows the same crossing. Log `r13-21692` in the `[fog]` lines.

## 3. [real] The actors' mist calls are elsewhere; 0x8011BE58 is inside the actor scope
- The 31 call sites of `0x801E21FC` [read: grep]: 24 pass 703, 3 pass 2751 (0x8018127C, 0x801F1768, 0x801F43A4), the
  room pieces 0x801E5270 pass **691**, `0x8011AB50` passes **0** (`li r5,0` at 0x8011AB38: no layer at all; a fixed
  material at 0x80252DB8), 0x80119158 and 0x8011C11C pass a computed mask. The design's "21 pass 703, 2 pass 2751" and
  "actors (0x8011AB50 ...)" are wrong.
- An actor's mist comes from `0x8011C11C` (mask r30 = 703 less bits, in `0x8011BE00`) and `0x8011BAC0` (703, in
  `0x8011B97C`). Chain [read]: 0x8011A278 -> 0x8011A880 `bl 0x8011B814` -> 0x8011B92C `bl 0x8011BE00`; and 0x8011A8B0 /
  0x8011A8C4 `bl 0x8011B97C`. Both are inside the object draw, so the design's actor scope still covers them (its
  outcome holds), but its stated path does not.
- `0x8011BE00` asks `0x80153998(2)` at 0x8011BE58 and then: if E, and the ROOM IN PLAY (`r13-23276`, 0x8011BE64) is 83,
  99 or 213, and `E[0] < object z` (+8), it clears bit 9 (0x8011BE90): no mist on an actor above that room's mist top
  [read]. Design 2.2 lists 0x8011BE58 among asks that "run outside any scope"; it runs inside the actor scope and gets
  E1 there, while the room number it tests stays room 0's. Apart with room 1 in {83, 99, 213} and room 0 not (or the
  reverse), room 1's actors take the wrong rule [maybe; not chapter 1; engine-generic]. The scope needs room 1 in
  `r13-23276` for that read (lr 0x8011BE5C marks the window; 0x8011BE64 is its only read there), not a chapter test.

## 4. [real] Effects use whatever texmtx 54/57 was loaded last
- The only `GXLoadTexMtxImm(.., 54)` / `(.., 57)` in the game: 0x8011A7AC / 0x8011A7BC (the object draw, `A x model`,
  `B x model`), 0x801E74CC / 0x801E74F0 (translucent section), 0x801E7664 / 0x801E7688 (opaque section) [read: grep of
  `li r4,54|57` before `bl 0x8021FC64`]. Design 2.5's open question ("whether an effect kind loads texmtx 54 itself was
  not read") is answered: none does.
- Effects are drawn after the actor list (0x80008148 actors, then 0x8017C370 / 0x8014FC24 / 0x8017C37C) [read
  0x800080E4]. An effect with layer 9 (0x801811EC 703, 0x8018127C 2751) therefore uses the LAST object draw's
  `A x model`. Under the design: a room-1 effect gets room 1's gate, E+36 and bank, with that matrix, which is a room-0
  actor's whenever a room-0 actor was drawn last (player 1's depth, room 0's band, that actor's model space); and a room-0
  effect after a room-1 actor gets `A' x model`. The effect scope is a half-fix; A' never reaches an effect [inferred].
  Either the effect scope also loads A' and B into 54/57 by guest call (and the clean game A after), or effects are
  named out of scope as a known limit. The world drawers (0x800EBF50-0x800ED3E0, 0x801DC46C/0x801DC584) are the same.

## 5. [real] The noise test in 2.4 cannot fail
- `r13-21988` is written once, by `0x801E065C` (`stw r29,-21988(r13)` at 0x801E06D4, after `0x801F31D4` allocates it),
  called from init (0x80021B58), and only when `r13-22124` and `r13-21992` are 0 [read]. The noise is FILLED by
  `0x801E01D8` at a change into a room with E (0x8015215C-0x80152168) [read]. So "apply the scope only if `r13-21988` is
  non-zero; a room in play without E since boot never made one" never skips: the buffer exists, its contents may be
  what the allocator left. Track "filled" on the host (a notify on 0x801E01D8) if the case matters.

## 6. [maybe] Paths the predicate leaves out
- **Whole frames of player 2's** (`g_room_whole_owner == 2`): `room_section_draw` draws the owner's room with the game's
  own draw (hle.c:13979-13991) [read]; `lights_apart` is false (hle.c:25950) [read], so no scope: player 2's room in
  that frame shows room 0's mist (today's bug, kept), and a list compiled in it bakes room 0's ramp (section 1). The
  design (2.0) leaves whole frames as today's on purpose; it should at least not compile there (or clear the list after).
  Whether the shared Tome's or the copy's whole frames draw room sections at all was not read.
- **Player 2's close-up or lent cameras apart**: camera.c:1625-1666 [read]: the game camera (0x8060248C) is player 2's,
  view 1 is `kSame`, view 0 is player 1's own camera by `rot/rel`, and `g_p2_view = false`. The design's fallback (C row 2)
  is right for section 3 then, but section 2 and room 0's actors keep the game's A, built from player 2's camera: player
  1's half's mist depth comes from player 2's eye. Design 0.4 ("the room-in-play half is always right") and 2.6 are wrong
  in that branch (stayer s1 "p2-examine" reaches it). The rule per half is "that half's camera": view 0 from
  `hle_coop_player1_camera` when the cameras are lent.
- **Scroll, palette, height**: with room 0 having E, room 1 keeps room 0's scroll (`E+4/+8`), palette limits (one TLUT 0
  walked by `0x801E0DFC(room 0's E)`, 0x801E852C-0x801E8530) and height ease (`0x801E4AB0` eases the CURRENT E[0] from the
  globals `r13-21916/-21920`, 0x801E4AFC-0x801E4B48; a fn124 in player 2's room eases room 0's top) [read]. Design 2.4
  rests on 180/182/183 sharing them, a chapter-1 fact, not engine-generic. Log both rooms' E+4/+8/+12..+44 and fn124 calls.
- **The flames**: 180's brazier flames and floor glow missing in player 2's half while 183 is in play (reader A 5, reader
  B 3) are not this layer (effect steps keyed to `r13-23276`, reader B S7 [read there]). The user's "vice versa" may be
  partly that; the design does not cover it, and should say so.

## 7. [maybe] Install and hook details
- `COOP_OBJECT_DRAW`'s notify installs only if the entry has no intercept (hle.c:29971); the kLights pattern installs only
  if it has no notify (hle.c:30562) [read]. The new actor intercept must go in after the notify and must not copy the
  kLights test. The dispatch runs an intercept and then, on passthrough, the notify (hle.c:5641-5654) [read], so the
  design's re-entry shape works.
- `0x80153998` runs once per material setup (thousands of calls a frame), and `0x80155130` already has the G1 hook; the
  new hook adds one host call per call in solo too when the env switches are set (installed by env, gated at run time).
  Cheap, but measure it.
- In the copy turn the dispatch skips the entry because its notify is on the hook-off list (hle.c:29262,
  `coop_found_object_draw`; 5623-5625) [read]: the new intercept is skipped with it, as the design wants.

## 8. Checked and right
- `0x80153998(n)`, `0x80155130(n)` (n = 2 current, 3 other, 0/1 a slot) [read 0x80153998-0x801539F4, 0x80155130-0x8015519C];
  the game itself asks with a slot index at a change (0x80152148-0x80152154) [read].
- `r13-21808`: one writer (0x801E75F8), one reader (0x801E2AA4); `r13-22144`: written only by 0x801E1CEC, read only by
  0x801E1D0C [read: grep]. The scope's writes cannot reach the lights, shadows or the renderer's lamps.
- texmtx 54's build and A' (row 0 `-(C row 2)/2000` then `x Trans(0,0,200-E0)`, row 1 `(0,0,0.005,0)`) [read 0x801E4EC8-
  0x801E4F60, C row 2 at r31+428 = 0x806024AC]; the LookAt row `(z, -z.eye)` with `z = normalize(eye - target)` matches
  `look_axes` (camera.c:1577-1581); up is the game's k, not +Z, which row 2 does not use [read].
- The object draw's six word arguments (0x8011A298-0x8011A2AC) and its `A x model` from the 0x8060238C buffer
  (0x8011A748-0x8011A7BC) [read]. The buffer is loaded Imm everywhere it matters (the one `GXLoadTexMtxIndx`,
  0x80193318, sets its own array 23 first, 0x801932A0) [read], so saving and restoring the 96 bytes round a scope is safe.
- Section 3 is drawn before section 2 in both passes (0x801E85C4/0x801E85CC); section 2 reloads 54/57 when room 0 has E
  [read]. When room 0 has no E, XF 54 keeps A' after the scope, but nothing of room 0 then has the layer (gate 0) [inferred].
- Solo, follow and together: no scope opens (lights_apart needs `mod_coop`, the split and apart) [read].

## 9. What to add before building
1. Section 1: scoped compiles, a compile log line per slot, and the game's own list clear (`+0x8138`, `+304`) for a room
   whose list was compiled outside a scope; tests that start from a state saved apart.
2. The actor rule at 0x8011BE64 (room 1 in `r13-23276` for that read), and correct the call-site list.
3. Effects: load A'/B into 54/57 inside the effect scope, or name the limit.
4. Per half's own camera in the lent/close-up branch (view 0 from player 1's camera).
5. A host "noise filled" flag instead of `r13-21988 != 0`.
6. Fix 0.5's facts on 0x801F164C and log `r13-21692` and `r13-25676`.
