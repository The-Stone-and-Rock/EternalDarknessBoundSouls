# Fog apart: the host and the renderer (reader B, 2026-11-13)

Item: the user's report (2026-11-13), with the players in two rooms (ED_ROOM_SLOTS=2, ED_COOP_STAY=1): "fog from the block
room is seen in the other player's screen (the room right next to the block room) where fog is not present, and vice
versa ... depending on which player leaves the room, the fog changes." This reader covers how the host and the renderer
draw two rooms in two halves today, which render state is set ONCE a frame (or from the room in play, r13-23276) rather
than per section or per view, and what the pictures say about which half is wrong. Nothing was edited, built or run.
Tags: [read] = seen at the file:line, address or picture given; [read there] = read in the doc named; [inferred] =
reasoning from what was read.

Rooms: 183 is "the block room" (BlockRegion01, the Granite Block, `ed room 183`) [read]; 180 is the room next to it
(183's Region01 is the door to 180, `ed room 183`) [read]. Views: view 0 = player 1's half (left), view 1 = player 2's
(right). Apart, the room in play is player 1's (coop_stay_apart, hle.c:14112-14118) [read].

## Short answers

1. **The fog is not the renderer's.** The user's settings.ini (last written 2026-11-13 15:09, before every stayer run,
   16:28-16:45) has `haze = 0`, `shafts = 0`, `glow = 0`, `ssao = 0` (hdr 1, char_shadows 2) [read: settings.ini;
   settings.c:356-382 applies them]. In the logged lights2 runs both views' Frames carry `vol 0` (no lamp haze)
   while the big room's fog shows plainly (logs/run-20261112-172052-lights2-on-183-zombies-door page-0001.log:641-647,
   shots/000008.jpg) [read]. So the haze is something the GAME draws; the renderer's own per-view haze/shadow work
   (lights stage 2) is not involved.
2. **Which half is wrong (pictures, section 3):**
   - Room 183's fog shows only while 183 is the room in play. With 180 in play it is gone from 183's OWN half: the same
     camera in 183's big room, together/183 in play (lights2-on shots/000008.jpg, frame 3790) is hazed; apart/180 in play
     (shots/000024.jpg, frame 4121) is clear [read].
   - With 183 in play and player 1 in 183's big room, player 2's half in 180 gets a pale veil over 180's walls and floor
     (stayer-s6 shots/000014.jpg frame 3885, 000030.jpg frame 4180) where 180 in play looks clear apart from its own
     brazier glow (stayer-s11 shots/000020.jpg frame 3992; solo stayer-s14 shots/000216.jpg frame 4012) [read].
   - With 183 in play but player 1 at 183's door (far from the big room), player 2's half in 180 has NO veil (stayer-s10c
     shots/000012.jpg frame 3851) [read]. And together at 183's door with player 1 in the big room, player 2's half at the
     door gets a pale band over the lower screen (stayer-s11 shots/000010.jpg frame 3809) that is absent with both at the
     door (s11 shots/000003.jpg frame 3661) and solo at the door (s14 shots/000186.jpg frame 3456) [read].
   - So the fog follows the room in play AND player 1's place in it, and shows over the whole of the other half, even
     where player 2's camera looks away from the big room. That is the signature of a screen-space / whole-frame pass
     driven by the main player (r13-26524) in the room in play, not of 183's world geometry or effects (which the marks
     already keep per half) [inferred].
   - The "vice versa": 180's own look (its brazier flames and the pale floor glow round them) vanishes from 180's half
     whenever 183 is in play (s6 000030.jpg, s10c 000012.jpg vs s11 000020.jpg, s13 000021.jpg) [read]. Its veil-free
     half then looks "clear" next to 183's fogged one [inferred].
3. **Every per-section/per-view path built so far is per room** (room sections, their lights, the effects through
   0x8017C1D8, the actors, the renderer's two lamp sets and per-view Frames) [read]. What is set once and shows in both
   halves is listed in section 2.3; the strongest candidates for this fog are the game's whole-screen 2D draws, which the
   renderer keeps unmoved in BOTH views by design (interp.cpp:195-196, 223-246, 599) [read], and whose strength or colour
   the game sets from player 1 / the room in play: `0x801F164C` (a textured full-screen quad) and `0x801E9534` (the EFB
   copy drawn back) [read code; which one draws 183's fog is NOT settled: probe P1].

## 1. How the two rooms are drawn in two halves today

### 1.1 Host (runtime/host/hle.c)
- **Room sections.** `room_section_draw` intercepts 0x801E75B4 (opaque) and 0x801E73D4 (translucent), section 3 (the
  other slot) then 2 (the current) [read hle.c:13938-14026; order 0x801E85C4/0x801E85CC, `ed dis 0x801E851C`]. Apart and
  split: section 3 (player 2's room) runs `lights_section(cpu, 3)` then `room_mtx_load(cpu, 0)` (camera copy in 27 marked
  D2/A0/E5: out of view 0); section 2 (the room in play) runs `lights_section(cpu, 2)` then `room_mtx_load(cpu, 1)` (out of
  view 1); after each, the clean camera goes back into 27 [read hle.c:13996-14022]. A section of a room nobody is in, or
  section 3 with the frame shown whole, draws nothing [read 13997-14008].
- **Effects.** `effect_draw_by_room` intercepts 0x8017C1D8 (every effect's draw), marks 27 and the identity in 60 by
  the effect's +56 room (room0 -> view 1, room1 -> view 0, else 2 = out of both) [read hle.c:14027-14090].
- **Actors.** The object draw's lr-gated mark by object +592 (coop_found_object_draw, gate 0x8011A278..0x8011B400) [read
  there: particles-reader.md, refute-r3.md].
- **Lights.** `lights_frame` (from room_slots_frame at section 3, hle.c:13912) sends set A (room in play) and set B
  (player 2's room) and turns the per-view rule on: `[lights] set A/set B`, `per view on at section 3` [read s11
  page-0001.log:782-785; s6 page-0001.log:854-857]. G1/G2 light player 2's room's actors from their own grid
  (`[lights] room 183's characters lit from their own room's grid`, s11 :803) [read].

### 1.2 Renderer (.deps/GXRuntime/graphics/aurora/lib/gfx)
- The game frame is recorded once; each view is a replay (`build_view`, interp.cpp:584-723): perspective projections go
  through the view's camera, matrices marked out of the view are zeroed, small 2D draws (HUD) are hidden or moved, and
  **whole-screen 2D draws (`screenWide`) and 2D draws that add light (`addsLight`, dst factor One) are kept as drawn in
  every view** (`isHud = ortho && !screenWide && !addsLight`, interp.cpp:599; set at note_draw, 195-196, 223-246) [read].
- Per-draw state (TEV colours, GX fog registers, lights, textures) is whatever the recorded draw carried: per section
  when the game set it per section [read: build_view copies the frame's uniforms and changes only projection and marked
  matrices, 588-590, 662-722].
- Shadows and lamp haze: `fill_shadow` fills `shadowView[0]` from set 0 (room in play) and `shadowView[1]` from set 1
  while per view (gxcore_draw.cpp:1617-1652); `render_character_shadows` picks the encoding view's Frame
  (common.cpp:2727-2729); the lamp-haze shader marches each pixel's own ray (shadow.cpp:265-327) [read]. With `haze = 0`
  it adds nothing (`volumetric() <= 0` returns after the tiles, gxcore_draw.cpp:1540-1545) [read].
- GX fog is rendered when a draw's state has a fog type (shader.cpp:1497-1527, from BP 0xEE-0xF2 or GXSetFog,
  command_processor.cpp:1040-1102) [read]; the game's only GXSetFog call is GX init with type NONE [read there:
  lights-game-readers.md 1.7; confirmed: the only `bl 0x8021E844` is 0x80217E74, disasm.txt].

## 2. Render state set once a frame (or from the room in play), not per section/view

### 2.1 Per section or per view already (not the cause)
| State | Where | Per what |
|---|---|---|
| Room sections' matrices | room_section_draw | section's room [read] |
| Room sections' live lights (0x801F2CB4) | lights_section G3 | section's room [read] |
| Effects via 0x8017C1D8 | effect_draw_by_room | effect +56 [read] |
| Actors | object draw mark | object +592 [read there] |
| Lamp sets, tiles, lamp haze, camera offset | lights_frame + fill_shadow | view (set A / set B) [read] |
| The actor light grid | G1 (0x80155130 answer) | object's room [read log s11:802-803] |

### 2.2 Renderer state that is once a frame
- Spell lights (`set_spell_lights`) carry no view: a rune's light shows in both halves (design-lights.md P7) [read there].
- `g_sceneUniformOffset` (the projection the shade pass inverts) is the frame's last 3D draw; per view the uniforms at
  that offset are the view's own [read gxcore_draw.cpp:1149-1152, interp.cpp:662-668]. Not a cross-room carrier.
- Post passes (shafts, glow, SSAO, SMAA, sharpen) are screen-space per view replay [read common.cpp:2163-2214]; all but
  sharpen are off in the user's settings [read].
- HDR glow textures 9 and 14 (settings.c:362-367) are matched per draw by texture [read]; per room by their draws.

### 2.3 Game state the halves share (drawn once, unmarked, or set from the room in play)
| # | What | Read | Why it reaches both halves |
|---|---|---|---|
| S1 | **0x801F164C, a textured whole-screen 2D quad** (bl at 0x80008164, after the actor list, before the effects): ortho projection, quad +-10240 x +-7680, texture 2751 (0x801E21FC(0x80301448, 0, 2751) at 0x801F1768), colour `r13-21692` with alpha forced to 105 (0x801F1668-0x801F1698), blend SRCALPHA/INVSRCALPHA (0x8021EAF4(1,4,5)), z test off (0x801E1648(0,3,0)). Drawn only when `r13-25676 == 0` (0x800442F4 at 0x80008158-0x80008160). The colour's setter 0x801F1434 is called from 0x80021718 (a reset) and 0x80083264/0x80083278/0x80083338 inside 0x80082090, the per-frame world animator (bl at 0x80008208; reads r13-23276 at 0x80082A04, 0x800833E8, 0x8008347C) | `ed dis 0x801F164C 200`, `ed dis 0x80008148 12`, disasm.txt grep `-21692(r13)`, `-25676(r13)` | an ortho draw over the whole screen: `screenWide`, kept in both views unmoved [read interp.cpp:246, 599] |
| S2 | **0x801E9534, the EFB copy drawn back** (bl at 0x800081D8): strength `0x801E9130()` = `r13-26876` times (1 - distance of the main player r13-26524 from the point at 0x803012B8 / radius r13-26880)^2 while word 0x80316488 == -2; copies the EFB (GXSetTexCopySrc/Dst, GXCopyTex 0x801E9590-0x801E95B8) and draws it back with 0x801E9234 (GXSetTevColor 0x801E9344, a 4-corner quad 0x801E9490-0x801E94F0). Runs when 0x80044254() (the ed284 countdown, r13-25680/-25684) is nonzero (0x800081CC). Set by ed284/ed285 scripts (L03, L04, L06, ... none in L01, chapter 1's level) and by node type 58's handler 0x800A8000 (0x800A80D0) | `ed fn ed285`, `ed fn ed284`, `ed dis 0x801E9534 80`, `ed dis 0x801E9130`, grep of build/ed-db/scripts | a whole-screen 2D draw (both views); its strength is by PLAYER 1's distance, so player 2's half takes player 1's |
| S3 | Draws through the CLEAN index 27 after the room passes that are not effects: 0x801DC420/0x801DC55C, 0x801819C4 (refute-r3.md 4b) | [read there] | unmarked camera: drawn in both halves at their world places |
| S4 | 0x801F4158 (the scene blob, "sparkles and mist", clean 27) | `ed dis 0x801F4158`: skips when 0x80316488 == -2 (0x801F418C-0x801F41C0) [read] | scene-only (the word is -2 in all 12 saves, `ed fn ed285`) [read there]: not this fog |
| S5 | Once per room pass, before both sections: 0x801E0F74 x4 (global texture-animation tables 0x8025B9F8, 0x8025BB14, 0x8025BC30, 0x8025BD4C), 0x801E14F4(&white) (constant TEV colour), 0x801E16D0(2) (vertex format) | `ed dis 0x801E851C 50` [read] | same for both rooms by construction; ed284 also writes 0x8025BC30 +12/+16 (0x80044228-0x8004424C) [read] |
| S6 | The light-set byte r13-22136, the room TEX1 set (0x801E51F8), material setup (0x801E2358...), texture swap (0x801E7930): one byte from the CURRENT slot's record | [read there: lights-game-readers.md 1.6] | one byte for both rooms; lightning rooms only (183, 180 have none, [1]/[2] count 0) |
| S7 | Effects the game stops making or stepping when their room is not r13-23276 (the sparkle step 0x8018ED60 ends an effect whose +56 != r13-23276; the light node's handler 0x80145994 runs its timed step only for the room in play) | [read there: particles-reader.md, design-lights.md 1.4] | not "both halves", but "only the room in play": fits 180's flames and floor glow vanishing when 183 is in play |
| S8 | 0x801E9014's one-time EFB copy (r13-21792 guard) | `ed dis 0x801E9014` [read] | once, for a later menu background; not per frame |

Not in the table: GX fog (never set by the game, 1.2). The four other per-frame world calls that read r13-23276 /
r13-26524 and draw (0x80083464 at 0x8000813C, 0x800ED7F8 at 0x80008190, 0x80082090 at 0x80008208) were only scanned
(callee lists and r13 words, scratchpad fnscan) [read]; what each draws in 183 was not read.

## 3. The pictures, both players

Shots are 960x540, left = player 1, right = player 2; frames by nearest both.tsv row (scratchpad shotframes.py).

| Run / shot / frame | P1 (left) | P2 (right) | Room in play | What shows |
|---|---|---|---|---|
| s14-solo 000186 / 3456 | 183 door | - | 183 | clear, dark blue corridor |
| s14-solo 000216 / 4012 | 180 braziers | - | 180 | flames, pale glow on the floor round the braziers |
| s11 000003 / 3661 | 183 door | 183 door | 183 (together) | both clear |
| s11 000010 / 3809 | 183 big room | 183 door | 183 (together) | grey veil in both; right: pale band over the lower screen at the door |
| lights2-on 000008 / 3790 | 183 big room | 183 big room | 183 (together) | heavy pale fog over the stairs, both halves |
| s13 000021 / 4016 | 180 | 180 | 180 (together) | as solo 180, both halves |
| s11 000020 / 3992 | 180 | 183 door | 180 (apart) | left as solo 180; right clear as solo 183 door |
| lights2-on 000024 / 4121 | 180 | 183 big room | 180 (apart) | left as solo 180; **right: the big room with NO fog** |
| s6 000014 / 3885, 000030 / 4180 | 183 big room | 180 | 183 (apart) | left fogged; **right: 180 under a pale veil, no flames, no floor glow** |
| s10c 000012 / 3851 | 183 door | 180 | 183 (apart) | left clear; right: 180 without flames or floor glow, NO veil |

Reading them [inferred from the table]:
- 183's fog belongs to 183 (together it is there; the user calls 183 the room with fog). Apart it is drawn only when 183
  is the room in play, and then in BOTH halves; with 180 in play it is in neither, its own half included.
- Its strength follows player 1's place: big room strong, door none (s10c vs s6; s11 3661 vs 3809). Player 2's place
  does not matter (s11 3809: player 2 at the door gets a band only because player 1 is in the big room).
- It covers player 2's half where their camera looks away from the big room (s11 3809: player 2's camera at (352,2361)
  looks +y to the door, the big room is behind it, `ed room 183` Camera25) and over 180's geometry: screen-space, not
  183's world. So it is one of the S1/S2-type passes (or another whole-screen 2D pass of the same kind), with its
  strength/colour computed by the game from player 1 / the room in play, and kept in both views by the renderer.
- So: **wrong = the half whose room is not the room in play.** It gets the room in play's fog (183's veil in 180's half),
  and its own room's fog-like effects are missing (183's half loses its fog when 180 is in play; 180's half loses its
  flames and floor glow when 183 is in play).
- Also seen, not this item: in s6 000030 player 2's half in 180 has several shadow blobs at their feet (180 has only
  player 2 there); possibly casters of 183 in view 1 [inferred, unsure; AURORA_SHADOW_LOG on a stay run would show the
  view-1 casters].

## 4. What a fix needs from the host and renderer (no code here)

- **Per half, with each half's own player and room.** A whole-screen 2D pass is drawn once in the game frame. For two
  rooms it needs either (a) the game's own pass run twice, once with player 1's words and once under player 2's (their
  room in r13-23276, their object in r13-26524, their room's record current: the coop_stay_room_in shape), each copy
  marked out of the other view, or (b) the one draw marked for the half whose room set it and a second copy computed for
  the other half. Marks for a 2D draw: its position matrix is GXLoadPosMtxImm'd by the pass itself (0x801F1710: index 0,
  a trans matrix), so a lr-gated mark on that load, as the fade's (notify_GXLoadPosMtxImm whole-word marks), would leave
  it out of a view; `marked_out_of` already zeroes Vs posnormal rows for 2D draws too (interp.cpp:713-714) [read], and
  `screenWide` keeps it otherwise.
- **The renderer needs nothing new for that**: the replay already drops a marked draw per view. It does need the marks
  to survive: whole words for an identity-like matrix (particles-reader.md hazards) [read there].
- **Solo and together untouched**: gate on coop_stay_apart && camera_split_on (as room_section_draw), keep the game's
  single draw otherwise.
- **The other room's own effects that stop when not in play** (S7) are game-side: the effect steps' r13-23276 tests.

## 5. Probes that settle it (not run here; check-runs are running)

- **P1 (which pass is the fog):** one frame apart with 183 in play and player 1 in the big room, and one with 180 in
  play: log every whole-screen/additive 2D draw at `note_draw` (ED_PHOTO_LOG's line, interp.cpp:470-488, only prints in
  photo mode today) or, host side, log entries of 0x801F164C (with r13-21692, r13-25676) and 0x801E9534 (with
  0x801E9130's result, r13-25680, r13-25684, 0x803012B8, r13-26880, r13-26876) once a second. Expect the fog's pass to
  run with 183 in play and player 1 in the big room, not otherwise.
- **P2:** the same with player 1 at 183's door (s10c's place): the pass's strength should drop to 0.
- **P3 (the vice versa):** log the effects drawn through 0x8017C1D8 with +56 and kind (+328) per frame apart (an
  `[effects]` line, particles-reader.md open item): 180's brazier flame effects should be missing while 183 is in play.

## 6. Facts for the designer

- room_section_draw / effect_draw_by_room / the object mark cover 3D draws only; no host path marks 2D draws today except
  the fade (notify_GXLoadPosMtxImm, hle.c:3325-3349 per particles-reader.md) [read there].
- The renderer keeps whole-screen and light-adding 2D draws in both views by design (photo mode / split) [read]; the
  frame's 2D draws from player 1's turn (S1, S2) therefore reach player 2's half unchanged.
- All the 2026-11-12/13 lights work is per view and correct for this item; the renderer haze was off in every run read.
- The world draw's order (0x800080E4): 0x801E851C rooms (0x80008118), camera 0x801E9728, node walk, 0x801E85E0
  translucent, 0x80119D60 actors, [0x801F164C overlay if r13-25676 == 0], 0x8017C370/0x8014FC24/0x8017C37C effects,
  0x801F4158 blob, 0x8000865C effect lists, ..., 0x801E9014 (one-time copy), ..., 0x801E9534 (copy effect, if on),
  ..., 0x80082090 world animator (0x80008208) [read `ed dis 0x800080E4 80`].
