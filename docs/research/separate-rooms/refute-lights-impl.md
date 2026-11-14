# Lights and shadows per room: refuting the design against the current implementation (2026-11-12)

Item: `design-lights.md` (G1, G2, G3, H, R1). Lens: conflict with the current host code: hooks on the same addresses, the
passthrough pattern, double-run game functions, state left across a save, load or room change, and existing fixes it
could undo. I also checked it against `docs/coop-failed-attempts.md`. Nothing was edited, built or run. Tags: [read] means
seen at the address or file:line given; [inferred] means reasoned from what was read.

**Verdict: needs changes.** Nothing refutes the approach:
- G1, G2 and G3 bracket only functions with no host hook. Each bracket puts back every word it changes in the same call.
- The design repeats no failed attempt.

But four facts the design depends on are wrong or missing. Each needs a change before building (1-4 below). Five smaller
points follow (5-9).

## What holds (checked, no conflict)

- **No hook on any address the design takes:**
  - `0x80155130`, `0x801E6244`, `0x801E87F4`, `0x801E8B9C`, `0x801F2CB4` and `0x801E58C8`: no hook in runtime/ (grep of
    *.c, *.cpp, *.h) [read].
  - The Tome's host calls of `0x801E58C8(0,0,0)` (hle.c:14838, 15068) are calls, not hooks, so G1-G3 never wrap them [read].
- **Install order:** the install block is in `find_coop`, which runs before `publish_codegen_options()` (hle.c:28348-28354).
  So new intercepts get chain stops. hle.c:5645-5647 warns that a hook added after that point needs it run again [read].
- **The passthrough pattern fits:** dispatch runs the intercept, then the notify only on passthrough (hle.c:5537-5564)
  [read]. G2's depth flag plus its lr gates covers the nested `0x801E87F4` call from `0x801E6350` (return 0x801E6354 is not
  an actor-draw site) [read 0x801E6350].
- **G2's arguments:**
  - `0x801E6244` takes f1: `fmr f1,f30` at 0x8011A560, before the bl at 0x8011A57C. So passing `cpu->fpr[1]` is required
    and right [read].
  - r3 is a pointer into the caller's frame (`addi r3,r1,104`). That is safe under `hle_guest_call`, as with r6c's pointer
    arguments [read 0x8011A578].
  - Only r3 comes back (`or r23,r3,r3`, 0x8011A580) [read].
- **G1's register:** r30 is the object at the bl `0x80027988`. It is set at 0x80027938, and only callees run between
  [read 0x80027930-0x80027988]. The grid in r16 is used in 0x8011A278 only for the null test (0x8011A47C) and as r5 of the
  light call (0x8011A568) [read: every r16 use in 0x8011A278-0x8011B400]. So G1 changes nothing but lighting.
- **G2 nests correctly with `coop_found_object_draw`:**
  - It is a notify at 0x8011A278's entry (hle.c:3274-3320) and reads the unswapped room word.
  - `coop_found_mark` marks only loads with lr inside 0x8011A278..0x8011B400 (hle.c:3323-3330). A matrix load inside the
    light functions would have an lr outside that range, so it is never marked [read].
- **G3's order:**
  - The opaque pass calls `0x801F2CB4` (0x801E8584), `0x801E14F4` (0x801E859C), `0x801E16D0(2)` (0x801E85B0), then
    sections 3 and 2 (0x801E85C4, 0x801E85CC) [read].
  - `0x801E16D0` sets only vertex descriptors (GXInvalidateVtxCache, GXClearVtxDesc, GXSetVtxDesc: 0x801E16F4-0x801E1724,
    names from generated/symbols.tsv) [read]. So reloading the lights after it does not undo it.
- **Save/load:** every bracket puts its words back inside one host call. A save state is taken between frames.
  `hle_save_held` (hle.c:28008-28016) needs nothing new [inferred].
- **Failed attempts:** H reads the game's per-slot grids. It does not read lamps from the draws (coop-failed-attempts.md:352)
  and does not read `0x804F0784` as it stands (359). R1 keys sets by print and mark, not "one lot of lights is one character"
  (297). Nothing marks a matrix with 0.0f translations (744-751). Every new path is co-op only, which follows the solo lesson
  (788-796) [read].

## Problems

### 1. (major) Set H is read from a slot record with no test that the record is loaded

H's `room_light_grid(cpu, slot)` does the following:
- `room_slot_record` (hle.c:13504-13509) checks only that the pointer lies in RAM [read];
- then `*(record+112)`, then `table[0]`, then the span test and the grid's own tests.

Set B's slot comes from `room_slot_of(room1)`, which reads the host's `g_room_slot_room[]` [read hle.c:13824-13830]. That
array is not reliable:
- `room_slots_frame` updates a slot's entry only while its record is loaded (`loaded != 0 && room != -1`, hle.c:13627-13636).
  A slot whose record is dead keeps its old room [read].
- `hle_coop_state_load` (hle.c:28020-28090) never resets `g_room_slot_room`. Its only writers are hle.c:13608 (an unpack on
  its way) and 13634 [read: grep].

So after a state load, or while a record is marked dead, `coop_stay_apart` can be true and `room_slot_of` can name a slot
whose +112 table points into freed or overwritten room data. coop-failed-attempts.md:784-787 records exactly that: a dead
slot's room data overwritten with texture bytes [read].

Set A has the same gap during player 1's door. H reads the current record's +112 live. The slot table can point at the new
slot (the swap 0x80154C50) while the reader thread is still unpacking into it. The grid tests (`grid+32 == grid+36`, count ==
dims) can pass on old bytes, which sends wrong lamps and resets every set-0 slot one extra time [inferred].

G1 and G2 do not have this gap: both use `r6_room_loaded`, which checks the record's live room, +0x8142 and +0x8143
(hle.c:24216-24222) [read].

**Fix:** use the game's own loaded test, the one `0x80155544` makes (record room == room, +0x8143 set), for BOTH sets:
- B only when `r6_room_loaded(cpu, room1)`;
- A only when the current record's +0x8140 room == `r13-23276` and its loaded bytes are set;
- otherwise keep the last set sent (A) or send B off.

Clearing `g_room_slot_room` in `hle_coop_state_load` would also close the stale-slot case for R2-R6c, but that touches every
earlier item. Name it as a separate decision, not part of this one.

### 2. (major) `room_lights_update` does not run once a frame at 0x80008128: it runs at every `0x801E4BDC` call, one of them inside the actor list

`notify_look_at` tests only `lr == g_camera_look_at_return` (hle.c:6219-6221). That return address is the C_MTXLookAt
inside `0x801E4BDC` (find_camera, hle.c:6688-6700; PAL `from` 0x801E4C1C; bl 0x80205ABC at 0x801E4D58) [read]. That
function has 9 callers [read: disasm]:
- `0x801E1204` (frame begin 0x801E1164, from 0x8000843C, before the world draw);
- `0x801E97D0` (0x801E9728, at 0x80008128);
- `0x801E9104` (0x801E9014, from 0x800081A4 after the actor list, **and from 0x80119FB0, inside the actor list 0x80119D60**,
  between its two loops);
- `0x801E9640` (from 0x800081D8), `0x801EA0F0` (from 0x80008538, 0x80020270, 0x80050BA0, 0x800EB924), `0x801F192C` (from
  0x80008164);
- `0x80106114`, `0x80106A1C`, `0x8010F310`.

So:
- **H's order claim is false.** "B's on/off lands in the stream before every flattened and lit actor draw of the same frame"
  does not hold. The call at 0x80119FB0 is mid-list, and the one at 0x800081A4 comes after the list.
- **1.5's claim holds only for the first two calls.** "0x804F0784 read there is the PREVIOUS frame's entry 0" is wrong for
  the later calls, which read this frame's.

At an edge where Apart changes (the slots' rooms are learned in `room_slots_frame` at section 3, hle.c:13691-13692), the
renderer can get B's change between the draws of one frame. Then part of that frame's flats and lit sets are collected with
the outOf keying and part without, while `fill_shadow` reads one `g_otherOn` at the display copy [inferred].

**Fix:** work out A and B once a frame at one fixed point. Two options:
- in `room_slots_frame` (the opaque pass's section 3, after the slots are learned and before any actor registers);
- or latch by frame number, so only the first call after `room_slots_frame` decides.

Also make the renderer latch `g_otherOn` at its frame start, so that one frame is collected and filled under one rule.
Today's together path can stay where it is.

### 3. (major) G1 and G2 turn on with Apart alone, but R3's marks need Apart AND the split; H sends B with no split test

R3 marks room1's actors only when `coop_stay_apart(cpu) && camera_split_on()` (coop_found_object_draw, hle.c:3300) [read].
Its sections are drawn per view only when `coop_stay_apart && (camera_split_on() || whole)` (room_section_draw,
hle.c:13698) [read].

The design's G1 and G2 gates do not include the split (sections 2 G1.2, G2 "Gate"). H's set B opens on Apart alone. The
render reader's advice was B "only while `coop_stay_apart(cpu) && camera_split_on()`" (lights-render-readers.md, short
answer 5) [read].

When the split is off while apart (a scene shown whole, R4b/R4c; whole-owner frames), room1's actors are drawn unmarked
into one picture of player 1's room. With G1/G2 they are lit by room1's grid and dynamic lights. G3 is off then, and set B is
on. That changes frames R4b/R4c already show, and the design's checks (section 11) have no run for them [inferred].

**Fix:** one predicate for every new path, identical to R3's:
`g_settings.mod_coop && coop_stay_apart(cpu) && camera_split_on() && g_room_whole_owner == 0 && !g_tome_copy_turn`.
Put it in one helper beside `coop_stay_apart`, so the marks and the lights can't drift apart.

### 4. (major) G2 can take player 2's torch light away from player 2 if the torch light carries player 1's room

G2 counts only dynamic lights with +42 == room1, the room word it swaps in (0x801E6698-0x801E66A8, 0x801F19D0-0x801F19DC)
[read: game reader 1.3/1.4].

Player 2's torch light is made by the host. `coop_torch_light_now` runs `0x801458A0(holder, TORCH_RECORD)` (hle.c:12669-12682)
inside `coop_torch_hold`. That is an intercept on the game's hold 0x800C994C, which runs whenever the game holds a thing of
player 2's equipment (hle.c:12714-12724) [read].
- The light's room is stamped from desc+48, or from `r13-23276` when that is 0 (0x801F2A20-0x801F2A38) [read: game reader
  1.4].
- If the hold runs outside player 2's turn, or before they parted, the light carries player 1's room [inferred].
- Nothing read here moves a light's +42 when its holder changes room without a light remake. `0x801F1F90(id, room)` exists,
  from 0x80145B98 [read], but who calls it at a door is not read.

So the outcomes differ [inferred]:
- **Today:** a torch light stamped with player 1's room lights everybody's draws, player 2's body included. Every actor
  counts room0's lights.
- **Under G2:** player 2's body loses it, and player 1's half keeps it. G3 also puts that light on room0's section 2 only.

That reverses the bug for exactly the item the user plays with: the torch. U4 only logs it.

**Fix:** settle it before building, with no new run:
- read who sets +42 for a held torch's light (0x801458A0 -> 0x801458C4 -> 0x801F1D84, and 0x801F1F90's callers);
- check in the existing logs whether `coop_torch_hold` runs inside or outside player 2's turn.

If the light keeps a stale room, the engine-level answer is the game's own field. Set the light's +42 to its holder's object
room (+592) through `0x801F1F90` when the holder is in the other room. Do it in the stamp path, not per frame.

### 5. (minor) G3 double-runs 0x801F2CB4: it writes more words than the design lists, and section 2's extra run doesn't depend on section 3's

The design says 0x801F2CB4 "writes no game memory but the light objects and r13-21800" (1.3). What was read:
- `0x801E15FC` writes `r13-22020` and byte `r13-26919` (the GXSetTevKColor cache, 0x801E1618, 0x801E1628). Its argument is
  the constant 255 (0x801F2D58, 0x801F2DD0), so it is idempotent [read].
- `0x801E54BC` decrements the float at `r13-26884` on each light-0 load (0x801E5530-0x801E5540). No other instruction reads it
  (disasm grep: only these two, and no `addi rX,r13,-26884`) [read]. It is harmless, but it is a game word changed by each
  extra run.
- `0x801E58C8` writes `r13-21892`, `r13-21896` and `r13-21796` as well as `r13-21800` (0x801E5908, 0x801E5A98, 0x801E590C,
  0x801E59E0) [read].

Section 2's run puts each of these back to the game's value, so the end state matches the game's. Two loose ends:
- **The section-2 reload runs even when section 3 wasn't drawn.** room_section_draw takes section 2 through the per-view
  path whenever `apart && room1 != room0` (hle.c:13730-13733), even when section 3 returned early because its record isn't
  live or isn't room1 (13715-13722). Run the reload only when G3 ran for section 3 in the same pass (a flag reset per pass).
- **Leftover lights reach later draws' prints.** XF light slots that room1's run loads past room0's dynamic count stay
  loaded, masked off. The renderer's lit-set print hashes all 8 lights whatever the mask (gxcore_draw.cpp:2186-2189) [read].
  So the prints of later room0 draws change with room1's dynamic-light count. Today the same happens between actors, so it's
  minor. Note it beside U7/U8, or hash only the lights in the draw's channel masks.

Add `!g_tome_copy_turn` to G3's own gate, as `room_slots_frame`'s call has (hle.c:13693). Today it is reached only through
the whole-owner test.

### 6. (minor) `hle_draw_characters` is an intercept on 0x8002790C itself, not "a different function"

`find_show_player` takes the function holding `lwz r0,show(r13); cmpwi r0,0; bne +16` (hle.c:6450-6465). With
`show = -26276` (the setter's `stw r30,-26276(r13)` at 0x80043C10) that is 0x8002794C-0x80027954. The nearest stwu before it
is 0x8002790C [read]. The other test, at 0x80007480, is a `beq` and doesn't match [read].

So every character callback runs nested in `hle_guest_call` (hle.c:6411-6433), with `r13-26276/-26272` set to the camera's
"drawn". This doesn't break G1, which is on 0x80155130 with return 0x8002798C inside the nested call. But design 5
("unrelated ... different function") is wrong, and anything G1 assumes about being outermost is false. U5/U6 count inside
the nested call [read/inferred].

### 7. (minor) Don't refactor r6c_creature's swap into a shared helper

The design factors r6c's word swap into `room_player_in/out` (G2). `r6c_creature` (hle.c:24447-24497) is a committed,
played fix:
- its room swap is conditional (`if (!current) coop_room_in`, 24477, 24490);
- it carries `g_r6c_p1` and `g_r6c_depth`, which `r6_player1_node` reads (24188-24195) [read].

A shared helper either carries those two words, which makes a draw bracket look like a creature's event, or leaves them out,
which changes r6c. Write the draw brackets' own helper and leave r6c as it is.

### 8. (minor) P7's rune mask from the rune object's +592 is likely player 1's room for both players' runes

Rune models are made by the host in `runes_frame` (`rune_spawn`, hle.c:9903) from `item_test_spawn` (hle.c:10083), outside
any turn, so the room stamped on them is the room in play [inferred]. If so, R3 already marks player 2's rune models out of
player 2's own half, and the mask would hide their rune's haze light in their half.

The game's own record knows the room: the spell's +8 (`coop_spells_update`, hle.c:12776). `g_runes[i].spell` holds the record
(hle.c:9593). **Fix:** use `spell + 8`, and keep U3 to confirm it.

### 9. (minor) The edge cases the design's tables claim

- **Player 2's door** (section 4, "A unchanged, not re-sent"): `g_stay_bracket` turns Apart off, so A goes back to today's
  `0x804F0784` path, which uses `table[r13-22136]`. Apart, H uses `table[0]`.
  - In a lightning room (`r13-22136` = 1 or 2) the two differ, so A is re-sent at both edges and player 1's set-0 shadows
    reset while player 2 takes a door [read 0x800279A4-0x800279D8; inferred for the consequence].
  - Fix: while a stay bracket runs, keep A as last sent instead of switching paths.
- **H's statics across loads:** `sent_grid/sent_mark` (hle.c:6101) and B's copies survive a state load. A load into the same
  rooms with the same grids keeps the renderer's remembered slots at their pre-load places for `kFlatMemory` frames. That is
  today's behaviour for A. It is now doubled, so say so in section 6.

## Checks to add to the design's section 11

- **Whole frames apart:** a scene shown whole while apart (R4b's, and the arrival camera's C1). Expect no `[lights]` G1/G2
  answers and no set-B line (problem 3).
- **Loaded test:** a state load while apart into a state where slot 1 holds another room. Expect no set B from a record
  that isn't loaded (problem 1).
- **One decision a frame:** `[lights]` edge lines carry the lr of the 0x801E4BDC caller. Expect each A/B change to land only
  at the chosen point (problem 2).
- **Torch:** before building, read the torch light's +42 for player 2 in the existing logs, or by `ed dis` of the stamp path
  (problem 4).
