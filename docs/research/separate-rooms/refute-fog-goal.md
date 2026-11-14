# Refuting design-fog.md against the goal (2026-11-13)

Goal: co-op "truly built by design". Each player sees exactly what the game shows its one hero in that room: together
and apart, in both directions, at arrivals and during doors, with nothing changed in solo. Question: what would the
user still see wrong once `design-fog.md` is built? Read only. No code was changed, nothing was built or run, and the
game was not launched. Tags: [read] = seen at the address, file:line, save state or picture given; [read there] = from
the doc named; [inferred] = reasoning from what was read. Verdicts: **[real]** = the user would see it (or the code makes
it certain); **[maybe]** = possible, or not shown in chapter 1.

## What the design gets right (checked)
- The block room is 183 and the room next to it is 180. The "fog" is layer 9 of the game's own material setup
  (0x801E21FC). Every input is read from the room in play, which is why the mist swaps with whoever changed room last.
  Pictures: s6 000014 right shows 180 under a pale veil; s11 000020 right shows 183's door clear while the left side (180
  in play) has its faint haze [read pictures].
- The design is correct to scope the record, bank, gate and texmtx 54 for section 3, room1's actors and room1's
  effects. Actors load 54/57 with Imm from `0x8060238C + 96*r13-21824` (0x8011A748-0x8011A7BC) [read]. The game has
  only one `GXLoadTexMtxIndx` call (0x80193318), and it belongs to an effect that sets its own array 23 at 0x801932A8
  [read], so no indexed load reads the mist buffer after a scope has ended.
- The A' algebra matches the game's build: row0 = -(C2 x Trans(0,0,d))/2000 and row1 = (0,0,k005,k005*d) [read 0x801E4EC8-0x801E4F60].

## [real] What the user would still see

### R1. Together in the split, player 2's mist is measured from player 1's camera
texmtx 54 row 0 is `-(0x8060248C row 2)/2000`, the depth from the game camera [read there: reader A 1.2]. The renderer
corrects each view's position matrices only. A draw's `texmatrices` are copied unchanged into each view and are only
blended between game frames (interp.cpp:414-420, `VertexShaderConstants.texmatrices`, shader.hpp:537 [read]). The design
leaves this case alone (2.6, "a separate item").
- What the user sees: the players stand together in 183's big room. The floor mist in player 2's half thickens or thins
  as player 1 walks, and a band shows over player 2's view of the door because player 1 is in the big room (s11 3809,
  together [read there: fog-host-readers 3]).
- Under this goal the together case is part of the item, not a separate one. It needs a texmtx 54 per view in the
  renderer, because one draw serves both views. Row 0 for view v would be the same depth taken through that view's
  corrected camera row 2. A host scope cannot do this, since it runs once per draw.

### R2. The handover between together and apart pops in the half that has no fade
- When player 1 takes a door away from player 2, the stayer's half changes source in one frame. Before the door its
  depth and band come from player 1's camera and room (no scope). After it they come from player 2's camera and their
  own room (scope open).
- At the reunion the change runs the other way.
- The fade quad is marked out of the stayer's view (R4 "the fade per half" [read there: coop-level3-design.md 2318-2324]),
  so the stayer watches the jump with no black to hide it. Solo never has a stayer.
- The design's own check ("the mist must not pop when either player moves") does not cover either door.
- Fixing R1 removes most of this pop. R3 still applies.

### R3. Every room change rebuilds the shared mist noise, and the stayer sees it jump
At a change, 0x80152160-0x80152170 runs in this order [read]:
- `0x801E0170(E)`: sets K2 to white and resets the palette walk state.
- `0x801E01D8`: makes a new 256x256 noise from the game's random generator (0x800F4474, an LCG on r13-29224, called
  from the diamond-square fill 0x801E0AD0). It first resets the scroll to 0 (0x801E3B50 at 0x801E0208: r13-21928 and
  r13-21924 := r2-19248) [read].
- `0x801E055C(E)`: refills TLUT 0's history.

The noise, the scroll and the palette are one global shared by both rooms. The design keeps that ("one noise for both
halves", 2.4/2.6).
- What the user sees, both directions: player 1's door, while player 2 waits in 183's mist, makes player 2's mist jump
  to a new pattern with its scroll snapped back. Player 2's door does the same to player 1's half, because the bracket
  runs the same arrival code.
- In solo this all happens under the black fade.
- It happens only when the arriving room has a mist record (0x8015215C skips the steps on E == 0 [read]). In chapter 1
  that means any door into 180, 182 or 183.
- A fix keeps the stayer's noise, scroll and palette when a stayer's room has mist. A new random pattern is invisible to
  the mover, who is under the fade. Per-room copies would also work.

### R4. Player 1's half is wrong while the game camera belongs to player 2
- While player 2's close-up, scene or lent cameras are up (`hle_coop_screen() || hle_coop_cameras_lent()`), the game
  camera is player 2's. Player 1's half is then drawn from player 1's own camera through the view-0 correction, and
  `g_p2_view = false` (camera.c:1622-1665 [read]).
- The game builds A from 0x8060248C, which is now player 2's camera. Section 2, player 1's actors and player 1's effects
  keep "the game's own inputs" (design 2.6), so player 1's mist depth comes from player 2's close-up eye.
- Apart, that eye is in another room whose coordinates overlap player 1's (reader A 0.5). Together it is a camera aimed
  at player 2's object.
- The design's fallback (C's row 2 when `camera_player2_view` is false) fixes only player 2's half. The short answer
  0.4, "the room-in-play half is always right", does not hold then.
- What the user sees: player 1's floor mist changes every time player 2 examines, picks something up, or plays a block
  scene, both together and apart.

### R5. 180's braziers lose their flames and floor glow in the stayer's half
- s6 000014 right shows 180 while 183 is in play: the bowls are dark red with no flames. s11 000020 left shows 180 while
  it is in play: flames lit [read pictures].
- The cause: the game ends effects whose +56 is not r13-23276 (0x8018ED60), and the light node 0x80145994 steps only
  for the room in play [read there: fog-host-readers S7].
- The user reported "graphics such as fog". After the mist is fixed, this half of the report remains.
- The design (and reader A 5) leaves it to "particles". It belongs on the same list, because the user will report it
  as the same bug.

### R6. Mist height moves are global and land on the wrong room
- `0x801E4A80` stores the frames and target in r13-21920/-21916 [read].
- The per-frame ease 0x801E4AB0 moves `0x80153998(2)+0`, which is the room in play's E0 [read 0x801E4AD4-0x801E4B48].
- Every room change resets the target (0x801521D4-0x801521DC) [read].
- Script fn124 `room_layer_height` (levels 06/08/09/11/12/16 [read there: reader A 1.1]) and the world animator's
  `0x801E4A80(1, f)` at 0x80083244 (0x80082090 reads r13-23276) [read] set it.

What follows apart [inferred]:
- A stayer's script that raises their room's mist raises player 1's room's mist instead, and the stayer's own mist never
  moves.
- The other player's door cancels an ease already running in the stayer's room.

The design doesn't mention the ease or fn124. It is not seen in chapter 1, but it breaks the engine-generic rule.

## [maybe]

- **M1. The rest of the noise is still one per frame from the room in play when that room has mist.** This covers the
  scroll speed (E+4/+8), the palette walk limits (E+12..+44), and K2. K2 is tinted by 0x8007CCF0 for room 83, and
  0x80016890 and 0x800B8410 were not traced [read there]. All of these are equal in 180, 182 and 183 [read there:
  reader A 4], so chapter 1 hides the problem. A level where two neighbours differ shows player 1's room's scroll,
  flicker and colour in player 2's mist.
- **M2. The design's reason for ruling out 0x801F164C is wrong.** Its conclusion still holds.
  - The design says the quad is "gated off by r13-25676". In fact the quad is drawn when that word is 0
    (0x80008158-0x80008164 [read]), and r13-25676 is 0 in rooms-ch1-183.state, slot1.state and slot2.state
    (tools/ed/savestate.py [read]). So the full-screen quad is drawn every frame.
  - It carries no mist because it sets r13-22140 := 0 (0x801F167C) and puts the old value back (0x801F189C) [read].
  - Its colour r13-21692 (0x000000FF in all three states [read]) is set from 0x80082090, which reads the room in play
    [read there]. In a level where that colour changes, both halves take player 1's room's overlay. The S2 pass
    (0x801E9534) works the same way, strength by player 1's distance, but not in level 01 [read there].
- **M3. The proof can't show R1-R4, and its reference is wrong.**
  - F2's pass value takes "the together F3" as player 2's own picture. F3's right half is itself wrong (R1). The
    reference should be solo, with the hero standing where player 2 stands, using that camera.
  - `low3 p5` on settled frames cannot catch R2, R3 or R4. Those need frame-to-frame differences of the half that
    doesn't move, measured across the other player's door and across player 2's examine.
- **M4. Unmarked world drawers keep room0's mist in both halves** (0x800EBF50-0x800ED3E0, 0x801DC46C/0x801DC584,
  0x801819C4; masks with bit 9) [read there]. What they draw in 180 and 183 was not read.
- **M5. Whole frames owned by player 2 get player 1's room's mist.** These are player 2's Tome copy turn and the shared
  Tome's frames (`split_skip_views_set(2)`, hle.c:15234, 15613 [read]). `room_section_draw` draws only player 2's room
  (hle.c:13980-13990 [read]). But `lights_apart` is false whenever `g_room_whole_owner != 0` (hle.c:25950 [read]), so no
  scope opens and player 2's room shows player 1's room's mist, if the world is visible in those frames.
- **M6. The camera build can run inside an actor's scope.** 0x801E4BDC, which rebuilds A and B from the current E, is
  also called inside the actor list (hle.c:6253 [read]; callers include 0x80106114, 0x80106A1C and 0x8010F310 [read]).
  If one of those calls runs inside a room1 actor's scope, it rewrites A with room0's E for the rest of that scope. Which
  actors make these calls was not read.

## Not an issue
- Solo stays untouched: everything is installed only with ED_ROOM_SLOTS=2 and ED_COOP_STAY, opens only under
  `lights_apart`, and passes through otherwise.
- The change's own record ask at 0x80152154 passes the table's slot index, not 2 (0x80152148-0x80152154 [read]), so
  `fog_record_ask` never rewrites it.
