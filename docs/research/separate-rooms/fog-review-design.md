# Review: the room mist per room (design-fog.md, Parts A and B) against the design and the disassembly (2026-11-13)

Read only: nothing edited, built or run (this file is the only write). Inputs: design-fog.md (final), fog-impl-notes.md,
hle.c (the `fog_` block 26177-26929, room_section_draw 13988-14059, effect_draw_by_room 14083-14125, the installs
31365-31367, state load 31622, hook-off list 30058), the `.deps/GXRuntime` diff (interp.cpp `mist_rows` 626-689 and
`build_view` 819-821, common.cpp 1486-1720, aurora_backend.cpp the SplitSetMist op), build/ed-db/disasm.txt.
Tags: **[real]** = wrong as written (the failure follows from the code read); **[maybe]** = depends on something not
read or not reachable in chapter 1, or a design gap.

## 1. What was checked and holds

- **lrs and returns** [read in disasm]: 0x801E75D4 `bl 0x80153998` -> lr 0x801E75D8 (opaque section); 0x801E744C -> 0x801E7450
  (translucent); 0x8011BE58 -> 0x8011BE5C (actor rule; the room-number test 83/99/213 at 0x8011BE64-0x8011BE90, bit 9
  cleared only at 0x8011BE90: the other r30 writes between 0x8011BE94 and 0x8011C114 touch 0x800/0x8000/16/128/4 only);
  0x801E2240 -> 0x801E2244 (material setup; the layer's bank read at 0x801E2AA4 is in the same call); 0x8011C11C ->
  0x8011C120 (r24 = the object from 0x8011BE20, not rewritten before the call); 0x80152160/68/70 -> 0x80152164/6C/74
  (r3 = r29 = the arriving E at all three); 0x801521DC -> 0x801521E0; 0x800080F8 -> 0x800080FC; 0x8011A7AC/0x8011A7BC
  -> 0x8011A7B0/0x8011A7C0 (r3 = r1+260 / r1+212 = A x model / B x model, r15 = the object). All match hle.c.
- **The section draw writes the CURRENT slot's bank** (`li r3,2; bl 0x80155130` at 0x801E75DC-0x801E75F8) even for
  section 3, and asks E with 2: the game is self-consistent (E0 with bank0); the scope gives E1 with bank1 via the
  0x801E2244 case. `room_slot_record(slot)` reads the same table word as `0x80155130(slot)` (0x8058AFC0 + 12 + 4n). Holds.
- **Saved/restored words**: r13-22144 (one writer 0x801E1CEC, returns the old value; the 104 rule at 0x8011A398 saves
  and restores it inside the scope), r13-21808 (only writer 0x801E75F8), the 96 bytes at `0x8060238C + 96*r13-21824`
  (the build 0x801E4EB8-0x801E5000 and every reader use the same index; A' x model is built at 0x8011A764 from it),
  the scroll pair (0x801E3B50 zeroes exactly r13-21928/-21924; 0x801E01D8 reads E only as a null test and makes a random
  noise, so skipping it at lr 0x8015216C changes no E-dependent state). fog_end restores all of them. Holds.
- **A5 maths**: the game's A = [-(C row 2)/2000; (0,0,0.005,0); 0] x Trans(0,0,200-top) (0x801E4EC8-0x801E4F60), so
  row 0 = (-r0, -r1, -r2, -r3 - r2 d)/2000 and row 1 = (0, 0, 0.005, 0.005 d): `fog_mist_a` is exact. C = 0x8060248C
  (r31 + 396 with r31 = 0x80602300). B' = diag(0.000625, 0.000625, 0) x Trans(sx, sy, 0) matches 0x801E4F64-0x801E4FCC.
- **The list clear** (hle.c 26604-26605) is the game's own (0x801E84B0-0x801E84B8: +0x8138 and +304, only when +0x8142
  and +0x8143 are set; fog_list tests the same two bytes). The display-list replay copies the bytes into the stream
  (`call_display_list_op` -> `append(Op::CallDisplayList, data, size)`), so a recompile cannot change an earlier queued
  call. Holds.
- **Part B per-view row**: `photo::projection` builds D = R^T [I | -p] (common.cpp 1848-1855), so D row 2 = (rot[0][2],
  rot[1][2], rot[2][2], -b.p) = what `mist_for` takes (back = `view.rot[i][2]`). The derivation: the mist depth of
  C T(d) M v is b.(P v + d c2 - p), so t_v[i] = -(b.P[:,i])/k, t_v[3] = -(b.P[:,3] - b.p + d b.c2)/k: `mist_rows`
  (interp.cpp 680-682) is exact, and gives back t for b = (0,0,1), p = 0. camera.c's rot (1641-1652, 1788-1795) is
  axis c of the view expressed in C's axes, position = C's view coordinates of the eye: same convention. The actors'
  position matrix is C x model (PSMTXConcat(0x8060248C, model) at 0x8011A790-0x8011A79C, loaded as PNMTX0 at 0x8011A7C8),
  so actor rows can match. Holds.
- **In-stream ordering**: `SplitSetMist` is appended on the game thread like `SplitSetView` (aurora_backend.cpp 2505-2517)
  and applied in stream order on the main thread; the views are sent from the C_MTXLookAt notify in frame begin
  (0x8000843C -> 0x801E1164 -> 0x801E1204 -> 0x801E4BDC), the mist from section 3's opaque draw inside the frame draw
  (0x80008118), both before that frame's Present, where `plan_outputs` takes `g_mist` (common.cpp 1716-1720). Frame
  begin eases the top (0x801E1188) before the build (0x801E1204), so d0 sent at section 3 is the top A was built with.
  Holds.
- **Patch**: `git -C .deps/GXRuntime diff` equals patches/gxruntime/eternal-darkness.patch byte for byte (1,440,000).
  Against HEAD's patch the only fog-related lines that changed are the four `split_build`/`split_between` signatures
  (the mist argument); the other changed lines are the earlier uncommitted lights work (per-set lamp arrays).
- **Solo / follow / ROOMS=0**: installed only under `coop_stay()` with the room draws' intercepts; with ED_FOG_ROOMS=0
  every hook passes through (fog_record_ask only counts frames, fog_list only marks, fog_frame only asks and logs,
  fog_door_setup returns on `g_fog_rooms == 0`). Holds.

## 2. Problems

### 1. [real] The self-test D1 cannot see an error in the depth direction (hle.c:26311)
`d1 = max |a[i] - buf[i]| / (1 + |a[i]|)`. Row 0's x, y, z entries are |C row 2|/2000, at most 5e-4 each, so even a
full sign flip of all three (2|c|/2000 <= 1e-3, divided by 1 + 5e-4) stays at or under 0.001 and is never "LARGE".
D1 only tests row 0's constant (a[3], O(1)) and row 1. The design's purpose (5: "A by the host's formula ... under
1e-3") is exactly to catch a wrong A5 row, and it is blind there. **Fix**: compare per row relative to the row's own
size, e.g. `d1 = max_i |k2000*a[i] - k2000*buf[i]| / (1 + |k2000*a[i]|)` for row 0 (or divide row 0's differences by
`max(|a[0..2]|)`), as `mist_rows` already does for its rows test.

### 2. [real] Part B's "self-test residual" can never fail on the per-view maths (interp.cpp:655, 667, 676)
`residual` is taken only from rows that already passed `e <= 1e-3*scale`, so it is <= 1e-3 by construction; the band
term `best` is bounded by the band tolerance (it reads LARGE only when |P[2][3]| is near 0). Neither involves `out`, the
rewritten row, so a transposed `rot`, a wrong sign on `bp` or on `d*bc2` would still log a residual under 1e-3. The
lens asks for a test of the per-view row maths; this one is a restatement of the match test. **Fix**: test `out` against
an independent build of the view's depth: take the view's D row 2 as `photo::projection` builds it (d[2][j] = r[j][2],
d[2][3] = -sum_j r[j][2] p[j]; factor that into a shared helper both use), and for the matched row check
`max_i |out[i] + (D2 . Q[:,i] + (i == 3 ? D2[3] + d*(D2 . c2) : 0))/k| / scale`; also log once that |b| = 1 within 1e-4.

### 3. [real] Room 1's own scroll starts from 0 (or a stale value) when it takes its own scroll without a room 1 change (hle.c:26643-26657; with A8 at 26784-26794)
`g_fog_scroll.sx/sy` are reset only when `room1` changes. Together, `room1 == room0`, so when player 1 leaves and player 2
stays, `room1` is the same room: no reset, and `sx/sy` are whatever an earlier apart period left (0 after a state load).
If the mover's new room has no record, or a different scroll speed (`g_fog_scroll.on` turns true), B' is built from
that value while A8 has just kept the game's pair for this room: the stayer's noise jumps at the other's door, the R3
failure A8 is there to stop. The reverse hand-off jumps too: room 1 on its own scroll, player 1 moves into a room whose
speed matches, `on` turns false and room 1 is drawn with the game's pair (A8 restored the mover's previous pair, not
sx1). Failure: together in a misty room, player 1 walks into a mistless room; next frame player 2's mist pattern
translates by `0.000625*(old pair)`. Not reachable in chapter 1 (180/182/183 share speeds and all have a record), so
F8 will not show it. **Fix**: keep `on` from the last frame; on the off-to-on edge with the same room 1, seed `sx/sy`
from the game's pair (r13-21928/-21924, which A8 kept); reset to 0 only when room 1 changes (player 2's own arrival,
whose game reset A8 does not keep). At a door where the stayer is player 2 and `g_fog_scroll.on` was true, have
`fog_noise_fill` restore `(sx1, sy1)` into the game's pair instead of the saved pair.

### 4. [real] F6b's pass criterion ("exactly one cleared line for room 180") cannot pass as built (hle.c:26845, 26602)
`fog_state_load` sets both slots' `g_fog_list_ok` false, and A4 clears a section-2 list whenever its flag is false
(design A4: "for section 2 always"). After loading saves/fog-apart-off.state (player 1 in 183 in play, player 2 in
180), the first frame clears 180's list in section 3's scope AND 183's list in section 2: two `cleared` lines. This
is the design's own rule (a section-2 list may have been compiled as section 3 under the other room before the save),
so the code is right and the criterion is wrong. **Fix**: change F6b's pass to "one cleared line per resident slot,
then none"; 180's half equal to F1-1's within 3.

### 5. [maybe] A list compiled in its own mist is drawn with no mist inputs in player 2's whole frames when the room in play has no record (hle.c:14002-14016)
In a whole frame of player 2's (`g_room_whole_owner == 2`), section 3 (room 1) is the game's own draw with no scope.
Its cached list, compiled in a scope, has layer 9's TEV stages and ramp baked on. The section draw asks E with 2: when
E0 == 0 it skips GXLoadTlut 0, texmtx 54/57 and texmap 7 (0x801E7630-0x801E7698), so the baked layer samples whatever
the XF and texmap 7 hold from the last draw of the previous frame (an actor's or effect's A x model). Before the fix
that list would have been compiled under E0 (no layer). Today's limit (A11) is "room 0's mist"; here it is undefined
inputs, a patchy veil in the Tome copy's or shared Tome's frames. Only when room 0 has no record and room 1 has one.
**Fix**: in that branch, when E1 != 0 and E0 == 0, open the scope for the game's draw (fog_begin with the
whole-owner test relaxed, A' from the game camera, which is player 2's in those frames), or at least load 54/57,
texmap 7 and TLUT 0 as the section draw would for E1.

### 6. [maybe] The reunion still rebuilds the noise and resets the scroll under player 2 (hle.c:26766-26768)
A8 runs only for a change with a spared stayer. When player 1 walks into player 2's room (the reunion), the arriving
room has E, there is no stayer, and 0x801E0170 / 0x801E01D8 reset the scroll and make a new random noise in player 2's
half, which is not under a fade. Design 9 asks "nothing pops when either player moves or opens a door"; the notes
name this as left to the game. **Fix** (design): treat the reunion like a stayer's door for the noise and scroll:
keep the noise, and set the pair to the room's current scroll (the game's when speeds matched, else sx1/sy1).

### 7. [maybe] Part B picks a room by the band alone; with a near-level camera the two rooms can't be told apart (interp.cpp:663-671)
`shift = d*c2[2]`; with c2[2] near 0 (world up nearly perpendicular to C's eye z) both rooms fit within the tolerance,
and d0 wins the tie. A room 1 draw that matches (ED_FOG_ROOMS=0 or 2, or A' fell back to the game camera) is then
rewritten with d0, an error of (d1 - d0)*(b.c2)/k in view 1, where b.c2 is not near 0. **Fix**: use the draw's view
mark, which the host already puts on every room draw apart (0xA0 in [1][3]'s low byte = out of view 0 = room 1's;
0xA1 = room 0's; unmarked = together, d0 == d1), and keep the band test only as the check.

### 8. [maybe] Part B leaves any draw whose position matrix isn't C x (the 54 load's model) (interp.cpp:648-657)
Row matching needs t[0..2] = -P[2][0..2]/k with P the draw's `posnormalmatrix`. A piece drawn with another PNMTX than the
one `A x model` was built for (per-bone matrices, `Xf.transformmatrices`) fails the rows test and is skipped without
being counted (not even "not matched"), so the [fog-view] line cannot show it. If player 2's own body is drawn that way,
together its mist keeps player 1's camera depth with VIEWS=1. **Fix**: count rows-test failures for texgens whose row 1
has the mist's shape (0, 0, 0.005 m, .) so the run shows them; if non-zero, rebuild M from texmtx 54 row 1 and 57
rows 0-1 (B x M and (0,0,k005,k005 d) x M give M's three rows) and use C x M instead of P.

### 9. [maybe] After a state load the noise counts as "not made" although the loaded RAM holds it (hle.c:26846)
`g_fog_noise_filled = false` at every load, but the noise texture is in guest RAM, which the state brings back
(0x801E01D8 fills a texture `r13-21988` set once at init). With room 0 mistless and room 1 misty after a load, room 1
is drawn without its mist ("degraded") until the next door into a misty room, though the noise is there. Departure 5
covers only E0 != 0. **Fix**: at load, set the flag when the noise texture's bytes are not all zero (read a few words
of the texture `r13-21988` points at), or when the state's frame had E0 != 0.

### 10. [maybe] A7's install does not check for an intercept on GXLoadTexMtxImm (hle.c:26904)
The notify is installed when `notify == NULL`; a notify runs only on passthrough (5627-5654). No intercept is on it today
(GXLoadTexMtxImm is not in kHandlers), but an intercept added later would silently stop A7. **Fix**: also require
`texmtx->intercept == NULL`, or log it.

## 3. Not problems (looked at, left)
- Departure 1 (the gate only where the game's is on, or the frame draw is open with E0 == 0): needed; the window range
  0x80008114-0x80008180 covers the returns of the sections (0x8000811C, 0x80008144), the actor list (0x8000814C) and
  the effects (0x8000816C-0x8000817C).
- `fog_object_draw`'s guest call: the object draw takes no float arguments (f31/f30 are loaded from the object), so
  passing f1 is harmless; six int arguments r3-r8 as at 0x80119ED8-0x80119EF0.
- A3 forcing bit 9 on outside the rule: bit 9 starts set (703) and is cleared only by the game's rule, so `| 512` only
  undoes the room-0-number rule.
- The palette step 0x801E0DFC returns on a null record (0x801E0E04), so the extra call with E1 when E0 == 0 does not
  double-step the game's.
- The [fog] loaded line reads the buffer the scope just wrote, so it can only agree with A' (design asked for it as
  proof the GX call got it; it proves the call site, not the value).
- A recompile (A4) also runs 0x801E00C8 (resets r13-22104/-22044/-22048 and the record's piece caches) and sets
  r13-21780 from the compiled room: the game's own first-draw path, once per bad list.
