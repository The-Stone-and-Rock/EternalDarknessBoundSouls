# The room mist per room: Part A, implementer notes (2026-11-13)

Built from `design-fog.md` (final), Part A (A0-A10) and the section 5 log lines, in `runtime/host/hle.c` only. Part B
(the renderer's per-view depth, `ED_FOG_VIEWS`, the `[fog-view]` line, `dol_aurora_mist_set` in camera.c) is implementer
2's and is not here. Built with the scratchpad build.bat: compiles and links with no errors and no new warnings. Not run:
the game was not launched, and nothing below has been seen working in play. Nothing committed; controls.cpp untouched.
Tags: [read] = checked at the address or line given (disassembly, `generated/main.dol` words); [built] = only compiled.

## What was checked first (design citations against the code)
- Every address the design gives checks out against the disassembly and the DOL's own words [read]: `0x80153998`
  (first word 0x9421FFF0; 0x80155130(n) with n 2/3/slot), the opaque section draw (ask at 0x801E75D4, bank at
  0x801E75F8, 54/57 loads at 0x801E7664/0x801E7688, compile when `+0x8138 == 0` at 0x801E7714-0x801E7830, `+304` at
  0x801E7880), the translucent one (ask 0x801E744C, loads 0x801E74CC/0x801E74F0, cached `+292` at 0x801E7578), the material
  setup (0x9421FFB0; gate at 0x801E2224; ask at 0x801E2240; `material+18` at 0x801E2258-0x801E2274; bank read at
  0x801E2AA4), the actor rule (0x8011BE50-0x8011BE90; no other write of bit 9 before 0x8011C114), the object draw (0x9421FD20;
  `A x model` at 0x8011A764, loads at 0x8011A7AC/0x8011A7BC; its 104 rule at 0x8011A390), the matrix build
  (0x801E4EC8-0x801E4FCC: A = [-(C row 2)/2000; (0,0,0.005,0); 0] x Trans(0,0,200-top); B = diag(0.000625,0.000625,0) x
  Trans(sx,sy,0)), the scroll/ease (0x801E4AB0-0x801E4B90), the change's mist setup (0x80152148-0x80152170, the ease reset
  at 0x801521DC), the game's list clear (0x801E84B0-0x801E84F0, only the two words), the frame draw (0x800080E4: the ask at
  0x800080F8, the gate set at 0x80008110 only when E0 != 0, cleared at 0x80008188).
- First words used by the installs, read from the DOL: 0x801E0170 0x9421FFE0, 0x801E01D8 0x9421FFF0, 0x801E055C
  0x80EDA994, 0x801E4A80 0x906DAA60. Each install also checks the game's `bl` at the site it acts on.
- hle.c line numbers in the design were current before this work (room_section_draw, effect_draw_by_room, lights_apart, G1,
  the object-draw notify, kLights, the dispatch); they moved by the insertions below.

## The pieces and where they are (hle.c, as built)
| Piece | Where | What |
|---|---|---|
| Forward decls, `FogSave` | 13970-13985 | before room_section_draw |
| A4 + A10 sections | room_section_draw 14011 (whole frame), 14034 (one room for both), 14043-14049 (scope + list, both passes; `fog_end` before `room_mtx_load(-1)`) | |
| A7 + effects scope | effect_draw_by_room 14107-14114 | |
| Once a frame | room_slots_frame 13917 -> `fog_frame` | |
| Block | 26175-26860 | switches, `fog_apart` (FA), `fog_ask`, `fog_k`, `fog_mist_a`, `fog_self_test`, A1 `fog_begin`/`fog_end`, A2 `fog_record_ask`, A3 `fog_material`, A10 `fog_object_draw`, A7 `fog_texmtx_load` + `fog_effect_matrices`, A4 `fog_list`, `fog_whole_seen`, A6/A9/log `fog_frame`, A8 `fog_door_setup`/`fog_noise_fill`/`fog_noise_palette`, A9 `fog_height_set`, `fog_state_load`, `fog_install` |
| Tome copy hook-off list | 30002 `OFF(fog_object_draw)` | |
| Install | find_coop 31311, after kLights, under `getenv("ED_COOP_STAY")` and `coop_stay()` | |
| State load | hle_coop_state_load 31566 | |

Hooks installed (all only with ED_ROOM_SLOTS=2 + ED_COOP_STAY, and only when the room draws carry R3's intercepts):
intercepts on 0x80153998 (A2, the core: nothing else acts if it is not taken), 0x801E21FC (A3), 0x8011A278 (A10; its notify
`coop_found_object_draw` stays and runs on the passthrough), 0x801E0170 / 0x801E01D8 / 0x801E055C (A8), 0x801E4A80 (A9); a
notify on GXLoadTexMtxImm (A7). Every handler passes through unless `g_settings.mod_coop`; scopes open only under FA
(`lights_apart` and `ED_FOG_ROOMS != 0`), so solo, follow, together, the split off and the Tome copy turn draw as today.

## Departures from the design (and why)
1. **The gate (A1.3).** The design writes `r13-22144 := E1 != 0`. The game's gate is 1 only inside the frame draw and only
   when the room in play has a record; an object draw outside it (the flattened shadow pass 0x80119D60(0) from 0x80028348,
   the other object-draw callers) would have got a mist the game never gives. Built: gate 1 when E1 != 0 and the game's gate
   was on; or E1 != 0, E0 == 0 and the frame draw is on the guest back-chain (returns 0x80008114-0x8000817C,
   `guest_backchain_contains`); else 0.
2. **GXLoadTexMtxImm's notify (A7)** is installed in `fog_install`, not as a `kNotify` table entry beside
   GXLoadPosMtxImm: the table installs for everyone, solo included.
3. **"This frame's" (A7)** is counted by the frame draw's own ask (lr 0x800080FC in A2), not the timebase frame, which can
   tick over between the actor list and the effects. A7 also tracks whose 54/57 the GX holds (`g_fog_xf_room`, reset by the
   section loads and at each frame draw) so an effect reloads only when it differs.
4. **A7's room for an actor outside a scope** is room 0 only when the object's +592 is the room in play (r15 at the load);
   things of neither room are not recorded (the design took every non-scope actor as room 0's).
5. **The noise flag (A6/A8)** is also set in `fog_frame` whenever the room in play has a record: its arrival (or the game's
   load) filled the noise, so a state loaded in a mist room does not lose room 1's mist until the next door.
6. **A4 with the noise missing**: a section-3 compile inside a scope whose gate was put off for want of noise is NOT marked
   as its own and is not cleared while that lasts (else it would bake "no mist" for good, or recompile every frame).
7. **D2 of the self-test** compares the game camera's row 2 with a LookAt of the game's view record (0x803012E0 eye/target)
   when the cameras are not lent: `hle_coop_player1_camera` holds a view only while lent (hle.c coop_player1_camera), so it
   cannot be the reference when not lent.
8. **A8's palette comparison** uses E+12..+32, +40, +44 (not +36, the ramp index). A8 also needs the stayer's room in the
   other slot than the arriving room's.
9. **A9 in player 2's turn** uses `r6_apart` (player 1's node against player 2's): `coop_stay_apart` is false in the turn,
   since the room word is player 2's then. The bracket's reset (lr 0x801521E0) stores n = 0 (room 1's ease ends, as the
   game's change ends its own) and leaves the game's pair alone.
10. **Cleared-list lines** (`[fog] list: ... cleared`) print without ED_FOG_LOG (first 16): they change the game's memory.

## Switches and log lines
`ED_FOG_ROOMS` (unset/1 on, 0 off = today's, 2 records only: A' from the game camera with room 1's top), `ED_FOG_LISTS=0`,
`ED_FOG_KEEP=0`, `ED_FOG_LOG=1` (also on with ED_COOP_LOG). Lines: `[fog] frame F: room in play ... | room1 ... | overlay
colour ..., r13-25676 ... | rule on|off|records-only` (on change, every 600 frames); `[fog] list: room R (slot S) compiled as
section N in its own mist|under room R0's`; `[fog] list: ... compiled outside its mist: cleared`; `[fog] frame F: section 3
(room R1) in its own mist: gate, bank, A' row0/row1 from player 2's eye (eye)->(target) | the game's A row0; N actors, M
effects`; `[fog] loaded: texmtx 54 row0 ... at lr 0x801E7668 in section 3 (A' row0 ...)` once per pair; `[fog] self-test:
D1 ..., D2 ...` once per pair (LARGE when over 0.001); `[fog] door: the stayer's (room R) noise and scroll kept; palette
kept|taken`; `[fog] height: ...`; `[fog] skipped: ...` (no noise, record not found, room 1 without a record, palettes or K2
differ, whole frames of player 2's), 8 each. `ED_FOG_VIEWS` is Part B's.

## Not built (the design's limits, A11)
Whole frames of player 2's keep room 0's mist (logged); the unmarked world drawers; one palette, K2 and overlay colour a
frame (logged when they differ); 180's braziers and floor glow in the stayer's half (effect steps keyed to `r13-23276`,
0x80145A70: a separate item, not this layer).

## Unconfirmed (needs the runs of design section 8)
- All of it at run time. In particular: that the installs take (look for `[fog] the room mist per room` and no `NOT
  installed`/`not taken` lines), D1/D2 under 0.001, the `loaded` row0 equal to A' row0, one `cleared` line in F6b.
- The cost: one guest call of 0x80153998 per scope (each room-1 actor, section pass and effect) plus one re-entered guest
  call of the object draw per room-1 actor; A2/A3 add a host call per call of two busy functions. Frame time not measured.
- The back-chain window test inside the object draw and the effect draw (expected to find 0x8000814C / 0x8000816C).
- Randomness: A8 passing over 0x801E01D8 and A6's extra 0x801E0DFC steps (room in play without a record) change how many
  times the game's random generator is stepped, apart only.
- The reunion (player 1 into player 2's room) is a change without a stayer: the noise is made again under player 2, as the
  design leaves it ("any change without a stayer: the game's").
- An effect of a room with no actor of its own drawn in that frame draw keeps whatever 54/57 the GX holds.
- A3 and A9 are not reachable in chapter 1 (rooms 83/99/213; no fn124 caller), so they will show nothing there.

## Part B (implementer 2): the mist's depth per half, in the renderer (ED_FOG_VIEWS=1)
Built from design-fog.md section 4 in `.deps/GXRuntime` and `runtime/host/hle.c`; Part A above left as it was. Builds and links
with the scratchpad build.bat (no errors; only the getenv/fopen deprecation warnings every file has). Not run: the game was not
launched. Nothing committed; controls.cpp and camera.c untouched. Tags as above.

### What was read first
- The texture-matrix layout the design left open (10, last item): `Vs.texmatrices` holds 3 rows per texgen resolved from XF;
  an ST (2x4) texgen reads rows 0-1 and only those are required to load (gxcore plan.cpp 1376-1393; wgsl.cpp 450-460) [read].
  So the mist row is `texmatrices[3g]`, rows 1-2 are left. The orthographic texgen re-aliasing in plan.cpp (`alias_screen_texgens`)
  runs only on orthographic draws (`projection_type != 1` returns) [read]: it never touches the mist's perspective draws.
- A view's camera: `PhotoView.rot` columns are its right, up, back in the game camera's view space, `pos` its position;
  `photo::projection` multiplies by `R^T (e - p)` (common.cpp) [read], so a view's depth is `b.(e - p)`, b = `rot[*][2]`.
- `build_view` gets a frame in between's blended uniforms as `f.uniforms` and the frame's own as `real` (split_between swaps
  them) [read]; per-view state reaches the renderer through the ring (Op/Item `SplitSetView`) [read].

### The pieces
| Piece | Where | What |
|---|---|---|
| Host send | hle.c `fog_views_frame` (26717), called from `room_slots_frame` (13919) after `fog_frame`; switch read in `fog_install` | once a frame: `dol_aurora_split_set_mist(true, c2, d0, d1, 2000.0, frame)` |
| Header | include/gxruntime/aurora_backend.h | `dol_aurora_split_set_mist` |
| In-stream op | backends/aurora/aurora_backend.cpp: `Op::SplitSetMist`, `Item::SplitSetMist`, `SplitMistArgs`, decode and dispatch cases, `split_set_mist_op` beside `split_set_view_op`, `dol_aurora_split_set_mist` | as SplitSetView: appended on the game thread, applied on the main thread in the frame's order |
| Aurora API | include/aurora/gfx.h, lib/aurora.cpp `aurora_split_set_mist` -> gfx `split_set_mist` (common.hpp/.cpp) | |
| Per frame | common.cpp: `split::g_mist` (as sent, `fresh`), `g_frameMist` taken at the top of `plan_outputs`, `mist_for(view)` adds the view's back axis and position; passed to `split_build` and `split_between` | a frame whose stream sent nothing draws the rows as the game made them |
| Rewrite | interp.hpp `struct Mist`; interp.cpp `mist_rows` (called in `build_view` before the marks loop), `mist_say` ([fog-view] line), `MistStats` | |

The host values: `c2` = the game camera's column 2 read from 0x8060248C (+8, +24, +40) at section 3's opaque draw (the matrix the
frame's mist rows and position matrices were built from); `d0 = 200 - E0[0]` with `E0 = 0x80153998(2)` (the game's own ask, by
`fog_ask`), `d1 = 200 - E1[0]` with E1 = the ask with player 2's room's slot, `d1 = d0` together, NaN for a room with no record.
On only with `ED_FOG_VIEWS` (not "0"), co-op on, the halves on (`camera_split_on`), no Tome copy turn, no whole frame
(`g_room_whole_owner == 0`), player 2 made; and only where `fog_install` runs (ED_ROOM_SLOTS=2, ED_COOP_STAY, `coop_stay()`).

The renderer, per view build (split frame and frames in between), for each distinct vertex-constant block of the frame's draws
and each of the 8 texgens: row 0 `t` is the mist's when `|t[i] + P[2][i]/k| <= 1e-3 * max_j|P[2][j]|/k` for i = 0..2 (P the
block's position matrix, k = 2000); its room is the nearer of d0, d1 with `|k t[3] + P[2][3] + d c2[2]| <= 1e-3 (|P[2][3]| +
|d c2[2]|) + 0.01`. A matched row in a view that is not the identity (b = (0,0,1), p = 0) becomes `-(b.P[:,i])/k` and, for
i = 3, `-(b.P[:,3] - b.p + d b.c2)/k`. Room 1's draws given A' from player 2's eye by Part A do not match (rows 0-2 are player
2's camera's, not -P[2]/k) and are left; when A' fell back to the game camera (close-up, lent) they match, and view 1 (kSame,
the identity) leaves them.

### Departures from the design (and why)
1. **Sent from `room_slots_frame`, not `camera_split_update` (camera.c).** d0/d1 need the game's record ask (a guest call; the
   camera hook is a notify on C_MTXLookAt, before the game camera is written), and c2 is read from the game camera as the frame
   draws with it. The send is in the GX stream before section 3's and the actors' draws, which is all the op needs (it is taken
   at the frame's present). camera.c is unchanged.
2. **Fresh per frame.** The renderer uses the values only for the frame whose stream sent them (`fresh`, taken in
   `plan_outputs`); no `on = false` op is ever sent. A frame without a send (Tome copy turn, whole frames, the split off, solo) is
   drawn as today.
3. **Tolerance on rows 0-2** is relative to the row's largest entry, not to each entry (an entry of 0 made the design's
   per-entry test 1e-9 wide). The band test takes the nearer room within its tolerance (d0 on a tie).
4. **Identity views are not touched** (player 1's own half, view 1's kSame while lent): exactly the game's matrices there.
5. **Frames in between**: the rows are found in the frame's own uniforms (exact), and rewritten from the blended position matrix
   (so the depth follows the blended model like everything else in that frame).
6. **The self-test** (design 5 "Part B" and its 10 "tolerance on blended frames") is in the `[fog-view]` line: the largest
   residual of a matched row against the game's own build (the formula with the identity view), relative to the row's size,
   `LARGE` over 1e-3; and the count of rows that had the mist's x/y/z but neither room's band ("not matched").

### Switches and log lines
`ED_FOG_VIEWS=1` (default off). Host (with ED_FOG_LOG or ED_COOP_LOG): `[fog] views: each half's mist depth from its own camera
(ED_FOG_VIEWS=1 ...)` at install; `[fog] views: the mist depth per half on|off (...) (frame F)` on a change; `[fog] views: frame
F: sent c2 (x y z), room in play R d D, player 2's room R1 d D1[ (together)]` on a room change and every 600 frames. Renderer:
`[fog-view] frame F: N mist rows rewritten (view 0 a, view 1 b), K left as the game camera's (view 0 .., view 1 ..), M not
matched; rows of the room in play .., of player 2's room ..; self-test residual X[ LARGE] over V view builds (c2 .., d .. ..)`
every 600 game frames (counts are view builds, frames in between included).

### The patch
`patches/gxruntime/eternal-darkness.patch` regenerated with `git -C .deps/GXRuntime diff`. Before: 1,428,956 bytes, 79 files,
468 file and hunk header lines (it equalled the tree's diff byte for byte). After: 1,440,000 bytes, the same 79 files and 468
header lines; hunks changed only in the 8 files above (aurora_backend.cpp/.h, gfx.h, aurora.cpp, common.cpp/.hpp, interp.cpp/
.hpp). Every line an earlier hunk added is still in the tree (four aurora_backend.cpp lines now show as context, the diff
realigned; checked present in the file).

### Runs
`build-run-fog.sh`: the fresh state first (Jump to Game, as build-run-stayer-presses.sh makes saves/rooms-ch1-183.state), then
the design's F1-F9 (F3/F9 with ED_FOG_VIEWS 0 and 1), stayer S3 and S8 with ED_FOG_ROOMS=1 and ED_FOG_VIEWS=1, F5/F5b solo,
`ed chapters 1` with ED_FOG_VIEWS unset and 1 and `ed chapters --compare`, then each run's timeline, check, [fog]/[fog-view]
lines and a sheet of 3950-4250. No `ed test`. `ed chapters` starts from saves/slot1.state as it always does: CHAPTERS=0 skips it.

### Unconfirmed (needs the runs)
- That the rows are found at all: `[fog-view]` "rewritten" non-zero in F3-v1 (view 1) and F9-v1 (view 0 while lent), zero
  rewritten for view 0 outside the lent cameras; the residual under 1e-3; "not matched" near 0.
- That actors' rows match: an actor's 54 is `A x model` and its position matrix `C x model` only if the game loads `C x model`
  as the draw's position matrix (skinned draws with per-vertex matrices have another P and will be "not matched" or not seen).
- With c2[2] near 0 (a level camera) the two rooms' bands cannot be told apart and d0 is taken: logged only as the counts.
  (Review fix 7 below: the view mark now settles it when both bands fit.)
- The cost: one pass over the frame's draws per view build (frames in between included), only with ED_FOG_VIEWS=1.

## Review fixes (2026-11-13, from fog-review-design.md and fog-review-safety.md)
Each point checked again in the code and the disassembly before changing anything. Built with the scratchpad build.bat:
compiles and links with no errors and no new warnings (hle.c's one old `-Wstring-plus-int` at 5676 is not this work). The
patch regenerated (`git -C .deps/GXRuntime diff`, equal byte for byte, 1,442,932 bytes, the same 79 files; only interp.cpp's
hunks changed, every earlier added line still there). Not run: the game was not launched. Nothing committed; controls.cpp
untouched. LF kept.

### Design review
1. **D1 blind in the depth direction: fixed.** Row 0's x/y/z are at most 1/2000, so `|a-b|/(1+|a|)` could never pass 0.001.
   D1 now compares each row at its own size: row 0 x 2000 (the camera row, the depth constant), row 1 / 0.005 (0, 0, 1, d),
   row 2 raw (`fog_self_test`).
2. **Part B's residual could not fail: fixed.** The self-test now takes the view's depth row from the view's own projection
   as build_view just wrote it (photo::projection's p x D; for a perspective draw its w row is -(D row 2), read from
   `dst.projection[3]`), applies it to the draw's position matrix and the band's shift, and compares that with the row
   written; relative to the row's size, `LARGE` over 1e-3, with the count tested. This is what the view is drawn with, not
   a restatement of `mist_for`'s back axis. Also logged: how far the back axis is from unit length (`LARGE` over 1e-4), and
   the old match residual separately as `match`. (No helper was factored out of photo::projection: reading its result is
   more independent and leaves the projection code as it is.)
3. **Room 1's own scroll from a stale value: fixed.** `g_fog_scroll.was_own`: on the edge from the game's pair to its own
   (together, or the speeds matched, then not) with the same room 1, `(sx1, sy1)` start from the game's pair (which A8 kept
   at the other's door); a new room 1 (player 2's own arrival) still starts at 0; together clears it. Frames that are not
   FA (whole frames, the split off) keep it, so coming back from them does not re-seed. And at player 1's door with player 2
   the stayer on their own scroll, A8 puts `(sx1, sy1)` into the game's pair instead of the game's (`g_fog_door.own`; the
   door line says "its own scroll into the game's pair"), so room 1 drawn on the game's pair again after the door doesn't
   jump. New line: `[fog] scroll: room R on its own scroll from the game's pair X,Y`. Not reachable in chapter 1.
4. **F6b's "exactly one cleared line": fixed in the criterion, the code kept.** A load makes both slots' verdicts unknown
   and A4 clears a section-2 list with an unknown verdict (design A4, 1d), so the first frame clears 180's (section 3) and
   183's (section 2). Pass now: one `cleared` line per resident slot, then none (build-run-fog.sh's F6 comment; design-fog
   section 9 amended in place). The script prints a count of `cleared` lines per run.
5. **A list compiled in its own mist drawn with no inputs in player 2's whole frames: fixed, narrowly.** Checked: with E = 0
   the section draw skips TLUT 0, 54/57 and texmap 7 (0x801E7630 -> 0x801E769C), and the layer's stages are in the list.
   Only for section 3 of a whole frame of player 2's, when the room in play has no record, room 1 has one and its list is
   marked its own (`fog_whole_wants`), the game's draw runs inside a scope (`FOG_SITE_WHOLE`: the ask, bank and gate of
   room 1, A' from the game camera, which the whole frame is drawn with, B' from room 1's scroll), by a guest call under
   room_section_draw's `running` guard. Everything else in whole frames stays the limit (A11: room 0's mist, actors
   included). Line: `[fog] whole frame: room R's list holds its own mist and the room in play has none ...` (8, then every
   600th). This only stops undefined inputs; a mist-room-1 list drawn under a misty room 0 in a whole frame is still the
   limit.
6. **The reunion rebuilds the noise: rejected.** Design A8 says it in so many words: "any change without a stayer: the
   game's". Keeping the noise and scroll at a reunion is a design change (which pair, which palette, the arriving room is
   now the room in play for both), not a build fault; left for the design, named in "Unconfirmed" above.
7. **Part B's room by the band alone: fixed where it can be wrong.** The view mark is used only when both rooms' bands fit
   and d0 != d1 (out of view 0: player 2's room's, out of view 1: the room in play's; unmarked: the room in play's), counted
   as "with both rooms' band: N by the view mark". Not taken as the first test, as the review proposed: the same three-byte
   mark (0xD2 / 0xA0+view / 0xE5) is also a found thing's (coop_found_mark), which says "out of that half", not "of that
   room". With the default switches the case doesn't arise apart (room 1's rows carry A' and don't match; lent, room 1's
   draws are out of view 0 and view 1 is the identity); it does with ED_FOG_ROOMS=0/2 and VIEWS=1.
8. **Draws with another position matrix: counted, not rebuilt.** `[fog-view]` now counts "band-shaped rows not of the
   draw's camera": texgens whose row 1 is (0, 0, 0.0005..0.05, .) and whose row 0 failed the rows test. Rebuilding M from
   54/57 is left until a run shows that count non-zero.
9. **The noise counted as not made after a load: fixed.** Checked: the texture (r13-21988, set once at init, 0x801E06D4) is
   a 256x256 C8 buffer (GXGetTexBufferSize 0x801E06B8) in guest RAM that 0x801E01D8 fills; a state brings it back. After a
   state load (`g_fog_noise_check`, not at boot) the first fog_frame samples 256 words 256 bytes apart and counts the noise
   made when at least 128 are non-zero. Line: `[fog] loaded state: the noise texture 0x... has N of 256 sampled words set:
   made|not made yet`.
10. **GXLoadTexMtxImm's intercept: fixed (logged).** The notify is still installed (an intercept that passes through runs
    it); when an intercept is there, `[fog] GXLoadTexMtxImm has an intercept: A7 sees only the calls it passes through`.

### Safety review
1. **r13-21780 after a forced recompile: fixed (the minimal version).** Checked: written at 0x801E77BC/0x801E53A4, read by
   both rooms' cached draws at 0x801E7724/0x801E73E8. When A4 clears a list in the split path, room_section_draw saves the
   word before the game's draw and puts it back after (`fog_list` now returns whether it cleared), so the other room keeps
   what it took before. Together and in whole frames only one room is drawn and the recompile is that room's, so nothing is
   restored there. Not done: a flag per slot (each room its own); that is the two-slot drawing's own question (R3, true
   before this work: the last compile wins for both), and would change player 1's picture against today.
2. **Recompiles together and with no player 2: fixed in part, (a) taken.** `fog_list` returns with no player 2. Together,
   the one recompile of the room in play's list per load (the game's own first-draw path, its own room) is kept, as the
   design's 1d asks; build-run-fog.sh says so at F3, and F3 now has an ED_FOG_ROOMS=0 run (F3-r0) to compare with.
3. **The verdict per slot, not per room: fixed.** `g_fog_list_room[slot]` is written with every verdict (a compile, a
   clear); a slot whose record now holds another room has an unknown verdict, so the clear rule applies to it.
4. **A scope's slot from the host's cache: fixed.** `fog_slot_of` reads the slot table's records (+0x8140's room, +0x8142
   and +0x8143 set, as the game's ask tests) instead of `room_slot_of`'s cache, in fog_begin, fog_frame, fog_views_frame and
   A8; the section draws pass their slot (`want_slot`) and the scope opens only when room 1's record is the one drawn
   (else a `[fog] skipped: ... record ... not found (slot S, the draw's slot T ...)` line).
5. **A failed ask taken as "no mist": fixed.** `fog_ask_ex` reports a guest call that didn't run to its end; the scope is
   then degraded (a list compiled in it isn't marked its own and is cleared once an ask works), with a skipped line.
6. **Frame time: fixed in part.** An actor's or effect's scope is not opened when room 1 has no record and the game's gate
   is off (no layer either way): no ask of the room in play, no back-chain walk, no re-entered object draw, in every
   object-draw pass of a mistless room 1. Not done: caching the record per frame draw; a cached record address could
   outlive a room change between frame draws (the object-draw passes outside the frame draw, 0x80028348's shadow pass), and
   the ask is the game's own loaded test. The cost is still to be measured against ROOMS=0 (`ed timeline`, design 9).

### build-run-fog.sh
- Checked: the fresh Jump-to-Game state first (S0, `env -u ED_COOP_STAY` with the slots on, as build-run-stayer-presses.sh);
  no `ed test`; every env name is one the kit reads (ED_PUT, ED_PAD2_SCRIPT, ED_RUN_SCRIPT, ED_SAVE_STATE, ED_RUN_FRAMES,
  ED_RUN_SHOTS_MS, ED_INPUT_SCRIPT, ED_COOP_P2_MAGICK, ED_CHANGE_HOLD, ED_STAYER_LOG, ED_ROOM_LOG, ED_COOP_TRACE,
  ED_COOP_CAST_TRACE, ED_COOP_ENEMY_LOG, JTG_SLOT, JTG_SET, ED_SETTINGS; the ED_FOG_* ones are hle.c's and interp.cpp's);
  `sh -n` passes.
- Fixed: `ed chapters` ran with ED_ROOM_SLOTS exported, and its saves/slot1.state (made without the slots) does not load
  with ED_ROOM_SLOTS set (quickstate.c:438). It now runs with the slots and the stay unset, and is said to be what it can
  be there: the check that both parts are inert without them (VIEWS 0 and 1 sheets equal). Part B's pictures are F3 and F9.
- Added: F3-r0 (ROOMS=0 together, the off run F3's "on and off equal" needs); the F6 pass rule (4 above); per run, the
  count of `cleared` lines, and for F5 the count of `[fog]` lines other than the install lines (the switches are on in the
  solo runs, so the install lines print; anything else is a failure).

## Runs (2026-11-13, build-run-fog.sh from the fresh state saves/rooms-ch1-183.state, made by Jump to Game on this build)

No `ed test` and no `ed chapters` (both load old slot saves; the script now runs the chapters block only with CHAPTERS=1).
Numbers: the lower third's 5th-percentile luma (p5, the floor the mist lifts) and mean, per half, at 960x270
(scratchpad fogm.py). Pictures: `Screenshots for Claude/fog-rooms/`.

| Run | Left half (player 1) | Right half (player 2) | Verdict |
|---|---|---|---|
| F1-0 off: P1 in 183's pit, P2 to 180 | 183, p5 36-51 | 180 under 183's mist, **p5 16-20** | today's bug reproduced |
| F1-1 on | 183, p5 42-45 | 180, **p5 5**, mean 37-41 | pass (F4's 180 is p5 5) |
| F1-2 (ED_FOG_ROOMS=2) | p5 42-49 | p5 6 | as F1-1 |
| F2-0 off: P2 in 183's pit, P1 to 180 | 180, p5 5 | 183's pit under 180's, **p5 2** | today's bug reproduced |
| F2-1 on | 180, p5 5 | 183's pit, **p5 39-47**, mean 68-79 | pass (F5b solo hero there: 43/53; F3 together: 44-49) |
| F2-2 (ROOMS=2, the game camera's band) | p5 5 | p5 4 | barely moves: ROOMS=2 is not a fix, only a probe |
| F3 together in 183: r0 / v0 / v1 | p5 48-49 / 48 / 48 | same | equal: pass |
| F4 follow into 180 (no stay) | p5 5 | p5 5 | the reference |
| F5 solo | | | 0 `[fog]` lines besides the install: pass |
| F6b a state saved apart with the fix off, loaded with it on | p5 38-50 | p5 5-6 | 2 `cleared` lines (one per slot), then none; equals F1-1: pass |
| F7 the order turned (P2 first, P1 follows, P1 back to 183) | | 180 apart again at 4400-4550: off p5 3 mean 12, on p5 5 mean 30-32 | pass; off, 180 goes very dark under 183's corridor settings |
| F8 KEEP 0/1 (P1 leaves, P2 stays in 183's pit) | | the jump at the arrival (shot 39) is player 2's camera settling from the ED_PUT flight, in both; after it 66 (KEEP=0) and 77 (KEEP=1) | not conclusive: needs a run where player 2's camera has settled before the door |
| F9 VIEWS 0/1 (P2's examine while apart) | 180 steady through the examine and return (jumps under 1.9) in both | | no visible change from Part B in 180 (its mist is faint) |
| S3 / S8 (both copy Tomes apart, ROOMS=1 VIEWS=1) | | | each copy opens and closes its own way; no scope or view send in the copy turn; the stayer's lines as before |

- Logs: D1 and D2 0.000000 every run; `loaded` texmtx 54 row0 equals A' row0; Part B rewrote 14,309 rows in view 1,
  0 not matched, self-test residual 0.000000 (S3).
- A longer F1-1 (fog-f1-long, to 5100): ~10 actors a frame of 180 drawn in 180's mist (5990, 6000 per 600 frames).
- Frame pace 16.67 ms over 3950-4100 in F1, F2 and F3, off and on.
- Defaults: ED_FOG_ROOMS 1 (on wherever ED_ROOM_SLOTS=2 and ED_COOP_STAY are on, co-op only), ED_FOG_VIEWS off.
- Seen, not the mist: in a 180 that is not the room in play the brazier flames and their light are missing, so player 2
  stands unlit by the door (solo's hero there is lit); coop-todo.md's brazier item.
