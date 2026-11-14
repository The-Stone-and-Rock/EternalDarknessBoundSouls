# Refuting design-lights.md: the overall goal and both players (2026-11-12)

Lens: the game must believe it was built for two players. Engine-level, co-op only (solo and together unchanged), every case
read for both players and both directions, the user's rules, the linear game. Read only: `design-lights.md`,
`lights-game-readers.md`, `lights-render-readers.md`, `stay-readers.md`, `coop-failed-attempts.md`, hle.c,
gxcore_draw.cpp, the disassembly (`build/ed-db/disasm.txt`, `ed dis`), and the save states in `saves/` (read with
`tools/ed/savestate.py`, no game run). Nothing was edited, built or launched.
Tags: [read] = seen at the address or file:line given. [inferred] = reasoning from what was read.

**Verdict: needs changes.** The core (G1, G2, G3, two lamp sets, one Frame per view) fits the goal and the code. G1's
single use of r21, G2's three call sites and the frame order all check out (section 3). Nine things must change before
building: one each in the spell masks, the dynamic-light table, the loaded test, the story of player 2's door, the checks,
the solo cap, the whole-owner frames, the user's two-player decision and the save-state rule.

## 1. Problems, most severe first

### P-A (major) The spell masks follow player 1's room for BOTH players' runes
- The design (R1.6) sets a rune's view mask from "the rune object's +592" and expects it to be its caster's room (U3).
- What is read: `rune_spawn` (hle.c:9637-9660) takes the room from `rec+44`, which `COOP_SPAWN_INIT` 0x80040E94 fills
  with the room in play (0x80040ED8 `lwz r3,-23276(r13)`, 0x80040EDC `stw r3,44(r31)`). It then sets that room with
  `COOP_SPAWN_SET_ROOM` [read].
- `runes_frame` is reached from `coop_pad_read` (hle.c:26778, 26816 -> 10083), and `coop_pad_read` from the pad hook
  (hle.c:4205). That runs outside player 2's turn, so `r13-23276` is player 1's room there [inferred: the turn is a
  separate bracket].
- Result: a rune of player 2's spell, cast apart, gets room0. That gives mask 1, shown in player 1's half only: the
  opposite of the rule.
- The same stamp already makes `coop_found_object_draw` mark the rune model itself out of player 2's view (by_room for
  room0 = 1, hle.c:3301-3313) [read code; inferred consequence]. So player 2 does not see their own runes apart today,
  if they are spawned for their spell at all.
- The host already does it right for weapon copies: `coop_weapon_copy` sets the holder's room explicitly
  (hle.c:25048-25063) [read].
- **Fix:**
  - Give a rune its caster's room: `COOP_SPAWN_SET_ROOM` with `coop_object_room(caster)`, as `coop_weapon_copy` does.
  - Then derive the mask from that room.
  - First read which field of the spell record (the r13-22324 list) names its caster, and whether player 2's quick cast
    puts its record in that list. Neither is read by the design or here.
  - U3 must log the caster's node next to the room.

### P-B (major) Two resident rooms share the game's 12 dynamic lights, and a new light evicts the last
- The allocator `0x801F1B40` keeps the 12 pointers at 0x80604E60 ordered by type (+45), scanning at 0x801F1B74-0x801F1B8C.
  A new light takes ENTRY 11's record (0x801F1B9C `lwzu r31,44(r4)`), shifts the rest down (0x801F1BB4-0x801F1C14) and
  stores the record at its rank (0x801F1C4C). Whatever light was in entry 11 is overwritten [read].
- Lights are made only by node events and held lights: 0x80026EC0 and 0x801458A0/0x801458C4 [read: the only `bl
  0x801F1D84` sites]. They are freed with their nodes: 0x80145974/0x80145AE4, and 0x8012C0F0 for the lying torches' three
  records [read: callers of 0x801F1C7C].
- In solo only one room's lights (plus held ones) are alive. With two resident rooms, both rooms' lights and both
  players' torches compete for 12 records. A light made in one room can silently replace one of the other room's, and
  nothing makes it again. That stays true after the players reunite, because the slot isn't reloaded on a swap
  [inferred].
- G2 and G3 can only show what the table still holds. The design (1.4, U4) treats the table as complete.
- **Fix:**
  - Extend U4: per room, the number of active records, and a line for every allocation while all 12 are active, naming
    the light that lost its record.
  - Run it in J-A, S-E and the torch run below.
  - Until that is measured, say in the design that "player 2's room's dynamic lights" can be missing for this reason.

### P-C (major) Set A and set B are read from records without the record's loaded test
- H's `room_light_grid` tests only the span, `grid+32 == grid+36` and the cell count.
- What is read in the save states (tools/ed/savestate.py):
  - In `saves/rooms-ch1-183.state`, `rooms-ch1-182.state` and `rooms-r6-base.state`, slot 0's record 0x806DA640 is dead
    (+0x8140 room = 0xFFFF).
  - Its +112 still points at a good-looking table: 0x807931A8 -> [0] = 0x807931B4, 1089 cells, [1] and [2] with
    count 0.
  - That passes all three of H's tests [read].
- Set B is gated only through `room_slot_of`. Set A (the current slot) has no gate at all. Yet the current slot is the
  one being unpacked during player 1's own door, and `coop_stay_apart` can stay true then: its slot test is on player 2's
  room (hle.c:13832-13837) [read].
- **Fix:** `room_light_grid` returns 0 unless all of these hold:
  - the record's +0x8140 room is the room expected;
  - its loaded bytes +0x8142/+0x8143 are set (the test 0x80155544 makes; `r6_room_loaded`, hle.c:24216).
  - While that fails, H keeps the set last sent: no re-send and no slot reset. It doesn't turn it off.

### P-D (major) Player 2's door: the design's story is wrong, and the frames that matter aren't checked
- Section 4 and S-D assume `g_stay_bracket` covers drawn frames: "B goes off and the set-1 slots go (the mover's half is
  under the fade)". In fact the bracket opens at the ticket runner (`notify_ticket_run`, hle.c:13858-13861) and closes
  at the behaviour pass that returns to 0x80008400 (`coop_stay_change_end`, hle.c:13868-13891) [read].
- Frame order [read `ed dis 0x800083D8`, `0x80008500`]:
  1. 0x800083EC: the ticket tick 0x80155830;
  2. 0x800083FC: the behaviour pass 0x8014FDB8;
  3. 0x8000851C: the world draw 0x800080E4, which holds the camera and `room_lights_update` (0x80008128).
- So no draw ever sees the bracket. S-D's criterion "while `[stay] runs as theirs` ... no set B change" is always met
  and proves nothing.
- The frames that DO turn Apart off are the new room's unpack frames: `g_room_slot_room = -1` makes `room_slot_of` fail
  (hle.c:13820-13837).
- In those frames, with the design as written [inferred from the gates]:
  - `g_otherOn` goes false, so R1 falls back to one Frame;
  - `coop_found_object_draw`'s room marks are off (they need `coop_stay_apart`, hle.c:3301), so room1's casters join
    player 1's Frame with set A's lamps;
  - set 1's slots are forgotten.
- That is a shadow change in PLAYER 1's half caused by player 2's door, on the very frames the user describes as
  "player 1's screen will pause for a brief second". Nothing establishes that either half is hidden by a fade then. The
  user says the screen does not go black.
- **Fix:**
  - Restate section 4 and S-D.
  - While a slot is unpacking for player 2, hold set B and `perView` at their last values. Send B again only when the
    new room's record passes P-C's loaded test.
  - S-D must log the unpack frames: Apart false, B held or sent, each half's tile count. Its pictures must show the left
    half (player 1) keeping its shadows through player 2's door.

### P-E (major) The scripted checks don't prove G1, G2 or G3 on screen, and miss the lens's cases
- **G2/G3 are untested.** Nothing shows that rooms 341 and 342 have any dynamic light. U6's "section lights S > 0" counts
  calls, not light. A room with no dynamic light passes J-A whether G2/G3 work or not.
- **G1's effect isn't measured.** No criterion says player 2's body is now lit by 341's colours (lamp 0 blue-grey
  92,107,124) instead of 342's amber (226,169,47). The renderer already prints each character's eight lights
  (`[shadow-lights]`, coop-failed-attempts.md:261) [read].
- **Player 1's half is only checked for P4.** G3 reloads player 1's lights before section 2 apart, so view 0 must
  otherwise equal a run of the build before. J-T covers this only for the together case.
- **Missing cases, each for both players and both directions:**
  - a close-up by player 2 apart (lent camera);
  - the Tome by player 2 apart (whole-owner frames, P-G);
  - a spell by each player apart (P-A);
  - a hit in player 2's room (S-E scripts no attack);
  - a scene in player 2's room while player 1 plays on.
  - S-D (player 2 moves) has no picture criterion at all.
- **What the game already gives for a real dynamic-light test:** a held torch's light follows its holder's room every
  step. Unless +41 bit 0x04 is set, the light node's step calls `0x801F1F90(light, 0x80118CAC(holder))`
  (0x80145B7C-0x80145B98). The node is made by 0x801458A0, which sets its callback 0x80145AD4 at 0x80145924-0x80145928
  [read].
  - So player 2's lit torch apart carries room1. Today it lights NOTHING in either half: the count needs
    +42 == `r13-23276`, 0x801F19D0-0x801F19DC [inferred].
  - After G2/G3 it should light their body and section 3 only.
  - That is a clean before/after for both players. Player 1's torch is the control.
- **Fix:**
  - Add a torch run: both players' torches lit, then apart. The two-torch case the user played is
    logs/run-20261108-161130-two-torches.
  - Add `[shadow-lights]` colour criteria for player 2's body.
  - Add a view-0 comparison against the build before.
  - Add the close-up, Tome and spell cases.
  - Merge J-A with J-R, and S0 with S-E and S-D, into fewer launches. The user asked for fewer game opens.

### P-F (minor) The sets cap 48 -> 96 changes solo and the together case
- R1.2 raises the sets cap with no gate: `ShadowLights sets[48]` (gxcore_draw.cpp:1015) and `g_sh.setCount < 48`
  (2195) [read].
- A solo frame with more than 48 lit sets would then group differently. That breaks "byte for byte".
- **Fix:** size the array 96, but cap at `g_otherOn ? 96 : 48`.

### P-G (minor) Whole-owner frames: player 2's room lit by player 1's room's live lights, its actors by their own
- In the whole-owner branch (`g_room_whole_owner == 2`: player 2's Tome copy, or the shared Tome's frames,
  hle.c:14762, 15137), `room_section_draw` draws section 3 (room1) alone, with no marks (hle.c:13698-13710) [read].
- G3 is skipped there (design 2 G3, section 8). G1 and G2 still answer: their gate is Apart, with no split test.
- So in those frames room1's walls get room0's dynamic lights and light 7 over player 1, while room1's actors get their
  own [inferred].
- "Tome frames run no effects" (failed-attempts.md:322) is about the renderer's shadows, not the game's GX lights. The
  design treats the two as one.
- **Fix:** run G3's bracketed `0x801F2CB4` before section 3 in that branch too. Section 2 is not drawn there, and the
  actors reload their own lights, so no reload is needed. Also gate set B and `perView` like `room_section_draw`
  (`camera_split_on() || whole`, hle.c:13697), as the render reader asked (lights-render-readers.md:272).

### P-H (minor, the user's decision) One-player lighting stays in the together case, and an apart-only fix would flip at doors
- Two things in today's game are player 1's only:
  - the player budget: room lamps 5-7 only for `obj == r13-26524` (0x8011A3F8-0x8011A408; r18 == 1 gives r19 = 0 at
    0x801E652C);
  - light 7, the room's specular light above `r13-26524` (0x801F2D28, loaded at 0x801F2DC0) [read].
- Apart, G3 gives player 2's room its own light 7. G2's intercept could also pass the budget: `gpr[7]` at 0x801E6244's
  entry is r25, per the call at 0x8011A570.
- Done apart only, player 2's body would change look at every door (together -> apart -> together). That is the "not
  built for two" feel the goal forbids.
- **Fix:** put ONE decision to the user, as design section 8 starts to. Either both players' bodies get the player budget
  in co-op, together and apart (co-op only, changes the together case), or neither does. Don't build an apart-only L3.

### P-I (minor) S-E and S-D are keyed to a save state
- The checks load `saves/rooms-ch1-183.state` (made by S0). The user's rule is fresh states only: Jump to Game, then
  door scripts and ED_PUT, and never key checks to a state (memory save-states-fresh-only).
- **Fix:** chain S-E and S-D after S0's door script in the same Jump-to-Game run.

### Facts in the design to correct (minor, no behaviour change)
- 1.3 says `0x801F2CB4` "writes no game memory but the light objects and r13-21800". It also calls `0x801E15FC`, which
  writes `r13-22020` and the byte at `r13-26919` and calls GXSetTevKColor (0x801E1618, 0x801E1628, 0x801E1634;
  0x8021E31C = GXSetTevKColor in generated/symbols.tsv) [read]. G3's reload before section 2 restores these too.
- 1.2 says "no host hook on any of" the light functions. That is true of the functions themselves, but not of their
  callees. `0x801F2CB4` calls 0x801F62B0 (intercept `coop_interface_player`, hle.c:27136-27141) and 0x80118274 (intercept
  `coop_position_copy`, hle.c:27758-27762) [read].
  - Both are lr-gated to other sites, so G3's swap doesn't reach their bodies today [read].
  - Risk 2 should name them.
- Reader open question 2 (are table entries [1] and [2] valid?) is answered: yes.
  - Every record read has three pointers at +112, with [1] and [2] counting 0 in rooms without lightning.
  - Rooms read: 100, 183, 338, 147, 249, 180, 341 (saves/*.state) [read].
  - So G1 indexing room1's table with room0's lightning byte falls back to [0]. G1 is safe here.
  - Section 3's TEX1 set under player 1's lightning (0x801E51F8) is still unknown. That is today's R3 behaviour. No
    lightning room was found in these states (+142 = 0 in all), so L5's check needs one.

## 2. Each player, each direction: what holds

| Case | Player 1's half | Player 2's half | Verdict |
|---|---|---|---|
| Apart, standing | A, own casters, own camera (P4 gone) | B, G1/G2/G3 | holds, if P-C and P-F are fixed |
| Player 1's door, player 2 stays | A re-sent | B kept | holds, if P-C (A from a slot being unpacked) is fixed |
| Player 2's door, player 1 stays | **changes in today's design** (P-D) | new B | needs P-D |
| Reunite either way | today's A path | one Frame | holds. Player 2's set-1 slots reset (as today at any door) |
| Player 2's torch apart | must not light (U4) | today: nothing; after: their body and room | the check is missing (P-E) |
| Player 2's spell apart | **shown here** (P-A) | **left out** (P-A) | wrong |
| Player 2's Tome apart | today's | walls lit by player 1's room's lights (P-G) | needs P-G |
| Player 2's close-up / scene apart | not checked | not checked | P-E |
| Lightning in player 1's room | flashes | flashes with player 1's (L5) | as the design says; check needed |

## 3. Checked and holding (so later readers don't re-read)
- **G1:** r21 is used once, at 0x80027998 (+112), and rewritten at 0x80027A30 / 0x80027BE4 before any other read [read:
  every r21 in 0x8002790C..0x80028258]. The object is in r30 at the getter's entry.
- **Other draw callback:** 0x8002825C, the other value a character's +24 can take, is a bare `blr` [read]. So no other
  callback needs a grid.
- **G2's call sites:** 0x8011A57C -> 0x8011A580, 0x8011A5B4 -> 0x8011A5B8, 0x8011A5EC -> 0x8011A5F0.
  - 0x801E6244 takes f1 (`fmr f31,f1` at 0x801E6268) and six ints, and r3 is a pointer into the caller's frame [read].
    The design passes fpr[1], which is right.
- **Actors and light 7:** an actor's COLOR1 mask is light 4, not 7 (`li r24,16` at 0x801E6290, `and r4,r26,r24` at
  0x801E6914) [read]. So light 7 over player 1 doesn't reach player 2's body.
- **G3 order:** 0x801E16D0 is vertex-descriptor state (GXClearVtxDesc/GXSetVtxDesc via 0x8021A480/0x80219978), so G3's
  re-run doesn't undo it [read]. r13-21888 is only ever set to 0 (0x801E4C58) [read].
- **Frame order:** opaque pass, then camera and `room_lights_update` (0x80008128), then node walk, then actor list
  [read]. H's on/off lands before the frame's actor draws.
