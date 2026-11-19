# Each player's hallucinations in a SHARED room: what the game does, and what to build (read 2026-11-18, nothing run)

Read with `plan.md` ("Also open") and `return-spec.md`. Every line is marked READ (from the disassembly or the host's
code) or GUESS. `hle.c` is being edited while this was read, so it is cited by function name, not line.

Corrections and new facts first:
- **Phantoms are told apart by their creature type, not a flag.** Type 64, 65 or 66 at info+152 (s16; info = node+40). READ.
- **The flag 0x1000 the door sets on them means "takes only the game's current player as quarry".** So today every
  phantom, player 2's too, goes for player 1. READ of the rule (0x800326C4, 0x8003293C); the result in play is a GUESS.
- **Player 2's blood has a second stage that is asked for by a POINTER into their sanity set** (0x80317DF0+2816, +3012),
  queued with 0x801DCB58 and made later in the frame, when player 1's set is in memory. As read, player 2's second
  stage (the running blood, the drips) is made from player 1's bytes: most likely never made. READ of the pointer; the
  result is a GUESS (blood for player 2 has not been seen in a picture yet).
- **In the shared room `effect_draw_by_room` already draws every effect object twice, once per half.** Leaving an object
  out of one half is skipping one of the two draws. READ.
- **32, 33 and 35 empty the entered room of its enemies for both players** (flag 0x10, room 10000). READ.

## A. Phantom Creatures (21)

### A1. How they are made

| Address | What | Reads / writes |
|---|---|---|
| 0x8009AFD4 | 21's start (table 0x80243D8C[21]) | READ. record+28 (step) := 0x8006BAB0, the one-shot runner; four EMPTY steps; record+24 \|= 0x2004; name "Ins: Phantom Creatures". The base flags are 2 (0x80068684), so 0x2006: no 0x10, the room's real enemies stay. The effect itself is over in a few frames; the phantoms stay behind. |
| 0x80043380 -> 0x8006C008(holder, next room, marker, camera) | the door's room setup, called at 0x80043688 | READ. All the spawning is here. Runs inside the door's bracket for player 2 (`sanity_p2_door`, `g_sanity_p2_door` and `g_sanity_p2_in` both true, their set in memory, they are the player). |
| 0x8006C228-0x8006C298 | count and reach for 21 | READ. Up to 5, placed within 500 of the watched character (holder+56's object, 0x80118274). |
| 0x8006C2A0-0x8006C2D4 | flag 0x2000 | READ. Count := 5 - 0x80043740(room, 1): the kind-1 nodes of that room that answer event 59 from the current player's id with 1. 5 or more there: none. |
| 0x8006E6B8(i, room, 21, &type, &b, &c, data+5) | picks the type | READ. For 21: a constant 41 for the spawn record's +40, and the type from the table 0x8023902C by the chapter's alignment (r13-26320 -> +0: 2, 3, other): **65, 66, 64**. |
| 0x8006F69C(pos, out, 500, 1) | finds a place | READ (its call only). |
| 0x8006D9B4(type, a, b, room, &pos; f1 angle) at 0x8006C3BC (lr 0x8006C3C0) | the spawner; returns the new NODE in r3 | READ. Fills a 56-byte spawn record (0x80040E94: +0 place, +12 angle, +16 type, +44 room) and calls the game's generic spawn **0x8003121C** (at 0x8006DA8C, lr 0x8006DA90; host intercept `hle_spawn_create` is already on it). Then 0x801F64C8(node, room), 0x801F5D18, 0x801DCB58(1, node). 21 gets a random angle (0x800F4474). |
| 0x8006C3C0, 0x8006C45C | where they are kept | READ. **Node at data+188+4i, id at data+252+4i (i < 16), how many at data+85** (data = holder+196 = 0x80317C8C, in `SanityOwn.ext`). data+84: how many nodes were moved away; data+76 the room; data+88 marker; data+92 camera. |
| 0x8006C52C-0x8006C53C | the flag | READ. Every spawn but 9's: info+140 -> +0 \|= **0x1000** (getter 0x80033874, setter 0x800338BC). 9's get object flag 0x4000 (0x80118BEC) instead. |
| 0x8006C550-0x8006C564 | data+4 := 1 | READ. For 21 when at least one was made: "phantoms are out". |

They are real creature objects with the game's own AI (kind 1, made by the same spawn scripts use). GUESS: types 64 to
66 are one-hit copies of the chapter's zombie (the creature table is on the disc, read by 0x8006F5E0; not read).

### A2. What the game does with the type

| Address | What | Reads / writes |
|---|---|---|
| 0x80060B10-0x80060B30 | a creature's swing builds its hit (0x8019B418) and sends event 39 | READ. Types 64-66: 0x8019B7B4(block, 0) in place of 1. GUESS: the amount, so a phantom's hit does no damage. |
| 0x80065D10-0x80065D28 | r19 := 0 for types 64-66 | READ. GUESS: the sanity cost of being seen by one is 0. |
| 0x8006DC00 (one caller, 0x80069724) | sends them away | READ. Walks the world's nodes: kind 1 with type 64..67: event 8 (death) if its room is the room in play (r13-23276), else event 57 (removed). **No owner test: all phantoms in the world.** |
| 0x80069694-0x80069730, in the level setter 0x80069018 | when it runs | READ. When holder+21 changes to **1** and data+4 != 0: 0x8006DC00, data+4 := 0. So they go when sanity comes back above half. |
| 0x8006C5A8(holder, flag) (callers 0x8006B62C, 0x8006BF90) | the door effects' cleanup: event 57 to each data+188 node whose id still matches, moved nodes back from room 10000 | READ. Runs at a RETURN and at a forced end. 21 does not return, so it never runs for 21. |

Co-op today: `sanity_level` answers player 2's level itself and skips the rest of 0x80069018, so player 2's phantoms
are never sent away by player 2's sanity (READ); and player 1's level reaching 1 with player 1's data+4 set sends away
player 2's too (READ).

### A3. Who they go for, and who can hit them

| Address | What | Reads / writes |
|---|---|---|
| 0x800324B8(node, other) | the quarry pick (38 call sites; no host hook on it) | READ. r28 := 0x801F62B8() at **0x80032568 (lr 0x8003256C)**: the current player's id, r13-21496. r15 = the asking node there (set at 0x800324C8, kept). r17 = the asker's flags. |
| 0x800326C4-0x800326D4 | given a candidate | READ. Asker has 0x1000 and the candidate's id != r28: answer 0. |
| 0x8003283C, 0x8003293C-0x80032950 | the walk over the world | READ. Asker has 0x1000: only the node whose id == r28 is a candidate. |
| `coop_sighting` (intercept on the look 0x8003B1C8) | runs the look as player 2 only when the creature is ALREADY after them (stats+20 == their id) | READ. A phantom can never come to be after player 2 by the game's rule, so this never opens for one. |
| 0x8006255C(block), 0x80061B9C(node, msg, ..), 0x800637B8(node, msg, ..) | the three places every hit is taken; host intercept `r6_hit_room` on all three | READ. Their only gate is "the victim's room is the room in play" (0x800625A4-B8, 0x80061BD0-E0, 0x8006383C). No owner test: player 1's swing kills a phantom of player 2's that they cannot see. |

GUESS: phantoms are solid, so the other player bumps into them (the walk's collision was not read).

### A4. How a creature is drawn, and how one is left out of a half

| Where | What |
|---|---|
| 0x80119D60(1) -> **0x8011A278(object, ...)** (16 call sites, the flattened shadow pass from 0x80028348 among them) | READ (docs `separate-rooms/design-r3.md`, `fog-review-safety.md`, and hle.c). Every actor's draw, no room test. r3 is the OBJECT (node+52). Its model and bone matrices are loaded by GXLoadPosMtxImm calls that return into 0x8011A278..0x8011B400. |
| `coop_found_object_draw` (the notify at 0x8011A278's entry; runs on `fog_object_draw`'s passthrough) | READ. Decides for this object: `g_coop_found_drawing` and `g_coop_mark_view` (0: left out of player 1's half, 1: out of player 2's, 2: out of both), from `g_coop_found_objects`, `g_coop_unseen[owner]`, `g_coop_unseen_look`, then the by-room rule when apart (which turns a disagreement into 2). The lists hold OBJECT pointers. |
| `notify_GXLoadPosMtxImm` -> `coop_found_mark` | READ. While `g_coop_found_drawing` and lr is inside 0x8011A278..0x8011B400: bytes 0xD2, 0xA0+view, 0xE5 at +15, +31, +47 of the matrix about to be loaded. |
| `.deps/GXRuntime/graphics/aurora/lib/gfx/interp.cpp`: `marked_out_of`, `build_view` | READ. Each half is the frame replayed (`split_build` -> `build_view(player)`); a draw whose position matrix, or one of its first 10 indexed matrices, carries the mark for that view has it zeroed: it falls to a point. |
| `gxcore_draw.cpp`, the shadow gather (`const uint8_t outOf = g_perView ? draw_out_of(plan) : 0`) | READ. The shadow casters take the marks only while the views are filled apart (two rooms). GUESS: in a shared room a hidden phantom still casts the renderer's shadow in the half that does not see it; the same is true today of a hidden player. To be seen in a picture. |

This is the mechanism to use: it is how a found item is shown to its finder alone and how a player is hidden from their
own close-up.

## B. Blood: Bleeding Walls (1) and Blood Pool / Drip (5)

Both keep their work in "the screen object", data+348 -> **0x80317DF0** (6024 bytes, `SanityOwn.screen`): three effect
requests laid end to end (+0, +2816, +3012) and three words: **+148 the stage-1 effect object, +5952 its serial
(0x80175354 = object+52), +6016 the room point in use (-1 none)**.

| # | Start / runner / steps | READ |
|---|---|---|
| 1 | 0x80086370 / 0x8006BBC4 (looping) / A 0x80086500 (100 ticks), B 0x800868C0 (61), twice over | Once the record's count (+8, one per step run) passes 64 the runner is swapped for 0x8006BAB0 and it ends. Tests flag 749 for the colour. |
| 5 | 0x80087024 / 0x8006BBC4 / A 0x800871F0 (300, 500), B 0x8008741C (10) | Once the count (+8, one per step B) reaches 8 it ends. |

### B1. Stage 1: the object made in the player's own update

| Address | What | Reads / writes |
|---|---|---|
| 0x8006A5AC(2, 256 or 4096, ..) | picks a room point near the watched character (256 walls for 1, 4096 ceiling for 5) | READ (the call). The index goes to +6016. |
| **0x801413E8(request)** at 0x80086834 (**lr 0x80086838**, effect 1) and 0x80087390 (**lr 0x80087394**, effect 5); request = r3 = 0x80317DF0 | makes the effect | READ. Calls the request's maker (+144) with (place, request+164, request); **the new effect object goes to request+148** (0x80141460). r3 on return is the DRAWABLE (0x8014FD08), not the effect. Then 0x801748D0(effect, request+170). |
| maker 0x80173768 (effect 1) | the wall patch | READ. Object from the pool 0x80174D40(r13-23104). **draw (+328) 0x8017E938, step (+332) 0x80173AD4.** The maker and the step are named only here; the draw 0x8017E938 is shared with 12 other kinds, so it does not say "blood". |
| maker 0x8018FEB8 (effect 5) | the pool on the ceiling | READ. **draw 0x80190628, step 0x801902DC**, both named only here. |
| 0x80086844, 0x800873AC | +5952 := the object's serial | READ. |
| 0x80084B08(level), 0x80129378, 0x80129700 | a sound and the character's reaction | READ (calls only). The sound is for both. |

Each round's step A makes a NEW stage-1 object and overwrites +148, so only the newest is in the set. The object is
stamped with the room in play at +56 as it is made (player 2's room in their update: `coop_stay_room_in`). READ.

### B2. Stage 2: asked for by pointer, made in the frame's queue

| Address | What | Reads / writes |
|---|---|---|
| 0x800868C0 (1), 0x8008741C (5): step B | if the stage-1 object still lives (+148, serial == +5952) and its drawable is idle: a block from 0x80143328 into (request2)+192, request2 = **0x80317DF0+2816**; +168 := the stage-1 serial; maker +144 := 0x80178A3C (1) or 0x801789FC (5); then **0x801DCB58(26, request2)** | READ. 5 also: 0x80146EB0(..) at 0x80087664 (lr 0x80087668), and a third request at **0x80317DF0+3012** (0x801479E4: maker 0x8018FFB4, +152 0x80147A50), sent with **0x801DCB58(18, request3)**. |
| 0x801DCB58(op, pointer) (host intercept `coop_thing_gone`) | queues the op | READ. Stores the POINTER, no copy. |
| 0x80025FF0, the queue's drain (called at 0x80008548 in the frame, and 0x80020430, 0x80050ABC, 0x800B8578, 0x8011177C) | op 26 at 0x80026D88, op 18 at 0x80026C80 | READ. Both: effect = 0x80175310(request+168); drawable = 0x8014ED1C(effect); **0x80141824(drawable, request, request+192's block)** (lr 0x80026DC0 for 26, 0x80026CB8 for 18). 26 then sets the new drawable's draw to 0x80141CD0 (0x8014F820). If anything is missing: 0x801433DC(block), request+192 := 0. |
| 0x80141824 (54 call sites, no host hook) | makes the children | READ. Calls the request's maker once per child (0x80141910); **child objects at block+136+4i, how many at block+0 (byte).** |
| makers 0x80178A3C, 0x801789FC | the running blood | READ. **draw 0x8017E274** (named only by these two makers: it does mean "sanity blood, stage 2"); 0x80178A3C's step 0x801796B8. |
| maker 0x8018FFB4 (5's third request) | the drip | READ. **draw 0x80190A18, step 0x80190E98.** Also named at 0x80147C54 (in 0x80147BE0, called from 0x80014CE8, not sanity): the draw alone does not say whose. |
| 0x80141CD0 | the stage-2 drawable's draw | READ. For each child: **0x8017C314(child) -> 0x8017C1D8(child)** (at 0x8017C3EC). So every blood object, stage 1 and 2, passes `effect_draw_by_room` (the intercept on 0x8017C1D8) with r3 = the object. |
| 0x80146EB0 -> pooled callback 0x80146F8C | 5's drops | READ. At ticks 400, 500 .. 1300: 0x800CC934(23, 3, place, ..) (lr 0x80147068), a type-23 thing; at 1301 it frees itself. Its draw path was not traced (GUESS: the things' walk 0x800C844C, not 0x8017C1D8). |

**The fault for player 2 (READ of each part):** their step B runs in `sanity_p2_step` (the pad read) with their set in;
the drain runs later with player 1's set in; request2 and request3 are addresses INSIDE the set. The drain reads
player 1's bytes there: a serial of 0 (or an old one) finds no effect, the block pointer it frees is player 1's, and
player 2's block is never freed. GUESS: player 2 gets the first patch only and never the running blood or the drips.

### B3. All 16 callers of 0x801413E8 (to tell blood from a torch by lr)

0x80086838 Bleeding Walls, 0x80087394 Blood Pool, 0x8009C12C Bug Crawl, 0x8006B360 the sanity flash (0x8006B270),
0x80145854 `ed85`'s flash; the queue's drain 0x80026C7C, 0x80026CEC, 0x80026D34 (ops 16, 22, 25: spells and the like,
what `fx_request_make` takes today); 0x80013EC0, 0x8002E43C, 0x8002E694, 0x801472C0, 0x80147D28, 0x80147FB4,
0x801C76B0, 0x801CBD08 (not sanity). READ. So **lr 0x80086838 or 0x80087394 is sanity blood and nothing else.**

### B4. What `effect_draw_by_room` does today

READ. Bug Crawl's picture is found by its draw 0x80192F10 (named only by its maker) and is "player 2's" if first drawn
within 300 frames of `g_sanity_p2_began_effect == 14`; it is then moved across, not marked. That rule does not carry to
blood: stage 1's draw for effect 1 is a shared one, blood lasts for a minute, both players can have it at once, and
stage 2 is made rounds later. Note the object when it is made instead.

In the shared room with the split on (`!apart || room1 == room0`, `g_fx_rooms == 1`): two draws of the same object,
the first with matrix 27 and identity 60 marked out of view 1 (so it is player 1's half's), then the vertex buffer's
parity flipped and the second marked out of view 0 with player 2's camera (`fx_facing_begin(cpu, 0, ..)`: player 2's
half's). Apart: one draw, marked by the object's room (+56).

## C. The other effects player 2 may roll (`kSanityP2`), in a shared room

| # | What it touches | Per player already? |
|---|---|---|
| 4 TV Shut Off | overlay (r13-25492), play hold, a stream 0x801A4884, the volume 0x8019DCDC | Picture and hold: yes (built). The silence and the stream: both hear. READ. |
| 6 Target Marker (0x80084B90) | the control block at r13-25660 (0x8019B030 / 0x8019B0A0 / 0x8019B0B0), the play hold | READ: the block is in the controller state `sanity_p2_step` puts in (`coop_state`). GUESS: per player. Not seen. |
| 7 Attack Camera (0x800859FC) | as 6, a stream, then the bullet-hole overlay (count r13-25404, flags -25403, alpha -30184, the holes' table 0x80324138: three s16[10] at +0, +20, +40) | Overlay: yes. READ: the set keeps only the first WORD of the table (`word_abs`); the other 56 bytes are shared, so two players' holes would mix. The shot: both hear. |
| 8 Horror Cam Shake (step 0x80085C70) | 0x801EBC34(30, 1): the shake's function into 0x806038CC and r13-21704, -21708, -21712; sounds 181 and 182; rumble 0x8013D6CC(1, 1, 22) | READ: 0x806038CC is inside the per-player camera set (0x80602B18-0x8060394C, `coop_cameras_in`), the three words are shared. GUESS: with their own camera set in, their half alone shakes; with none yet, player 1's does. The sounds: both. The rumble's pad was not traced. |
| 10 Locked in Room (0x8009C6EC; steps 0x8009C81C, 0x8009C8B8) | the door gate 0x800440D0 (callers 0x800100A4, 0x80166788): if 0x8006A32C(holder, 10): data+32 \|= 0x2000 and the door is refused; the steps count three tries, then event 81 | The lock: yes, for whoever is the player at the door (`sanity_is_on_n` answers from player 2's kept set in their turn). READ: but the "tried" bit is written to the set IN MEMORY, which in player 2's turn is player 1's. Player 2's tries are never counted. GUESS: theirs never ends by trying. |
| 14 Bug Crawl | an effect object, draw 0x80192F10 | Yes (built). |
| 17 Head of Shakespeare | the head: a real object (0x801F9FD0), id at data+148; the body's animation | The pickup: yes (`sanity_pickup`). The head on the floor and the headless body are in both halves. READ. |
| 21 Phantom Creatures | section A | No. |
| 24 TV Mute, 31 TV Volume (0x800AC040, 0x800AC34C) | overlay; the volume 0x8019DCDC, the one before kept in r13-25208, -25212 | Picture: yes. The sound: both. READ. |
| 25 TV Video | the host's own screen | Picture: yes. Its sounds: both. |
| 32, 33 Shrink, Expand (0x8009D1D0); 35 Sinking (0x8009E308) | the character's own scale or height; **record+24 \|= 0x10** at the start (0x8009D2D0, 0x8009E408) | READ. Their body is a real object: both halves see it small, large or sunk. And 0x10 makes the door's setup (0x8006C0A0-0x8006C1B0) move every kind-1 node of class 2 (info+158; not info+159 of 31, 32, 33, 35, 36, 40) and every kind-2 node of class 4 in the entered room to room 10000 (0x80118CB4), up to 32, listed at 0x80319578, until the return: **the room's enemies are gone for player 1 too.** |
| 38 Footstep Trail (0x8006B204) | one empty step of 1800 ticks | READ: nothing else in its start, and neither "is it on" question (0x8006A32C, 0x8006A3A8; 24 callers) is ever asked with 38. GUESS: it shows nothing as shipped, or is read some other way. |
| always on: record 2 (0x8008584C) | the camera's lean with low sanity: 0x801EEDB4(2, 0, angle) into camera record 2 at 0x80602B18+272; r13-25408, -25412, -25416 | READ: the record is in the per-player camera set; the three words are shared and not in `kSanityWords`. GUESS: one player's lean sets -25412 and the other's sane update then takes the "came back" branch each frame (a jolt of 15 in -25408). Worth one look in a run. |

## What to build

Phantoms (A):

1. **A list of phantoms with their owner, by reading the two sets; no new intercept.** `g_phantoms[24] {node, id, object,
   owner}`. Once a frame, beside the call of `sanity_p2_step` (when `!g_sanity_p2_in && !g_sanity_p2_door`, so memory
   holds player 1's set): for player 1 read guest memory, for player 2 read `g_sanity_p2.ext` (`sanity_own_read`):
   count = the byte at 0x80317C8C+85 (at most 16), node = +188+4i, id = +252+4i. Keep an entry when the node is valid,
   mem32[node+16] == id, mem32[node+56] == 1 and (s16)mem16[mem32[node+40]+152] is 64..66; object = mem32[node+52].
   Entries are STICKY: a later door effect reuses the slots, so drop one only when its id test fails.
   The first frame player 2's can be read is the one after `sanity_p2_door(false)` puts their set out; to have them from
   their first picture, read them in `sanity_p2_door(false)` just before `sanity_set_io(.., true)`.
   (The other way: an intercept on 0x8006D9B4, r3 = the node on return, owner 2 while `g_sanity_p2_door`. More to install,
   nothing gained.)
2. **Hide in the other half:** in `coop_found_object_draw`, after the `g_coop_unseen_look` loop and before the by-room
   rule: `cpu->gpr[3] == g_phantoms[k].object` -> `g_coop_found_drawing = true; g_coop_mark_view = owner == 2 ? 0 : 1`.
   The by-room rule below it already turns a clash into 2. Nothing else: `coop_found_mark` and the renderer do the rest,
   the game's flattened shadow draw included.
3. **Each goes for its owner:** in `coop_interface_player` (the intercept on 0x801F62B8): at `cpu->lr == 0x8003256C`
   with `cpu->gpr[15]` a listed node, answer the owner's id (player 2's: mem32[g_coop_p2+16]; player 1's: their node's
   +16, so it holds inside a bracket that has player 2 as the player). Once a phantom's stats+20 is player 2's id,
   `coop_sighting` already runs its look as them.
4. **No hits across owners (cheap):** in `r6_hit_room`, before its `r6_apart` test, with the victim and attacker it
   already works out: victim a listed phantom and the attacker the OTHER player, or victim a player and the attacker the
   other's phantom: run the call with r13-23276 set to a room that does not exist (0x7FFF) and put it back, so the
   game's own gate refuses it. (`r6_hit_room` is installed with the stay; check it is on in follow mode.)
5. **Sending them away, per owner (later):** intercept 0x8006DC00 (one caller) to send event 8 / 57 to player 1's
   listed ones only; in `sanity_level`, when player 2's level becomes 1 and their ext+4 != 0, send the same to theirs
   and clear ext+4.
6. Left open: the renderer's shadow of a hidden phantom in a shared room (A4), walking into one you cannot see, their
   sounds.

Blood (B):

7. **An owner table for effect objects:** `g_fx_owner[48] {effect, serial, owner}`, an entry good while
   mem32[effect+52] == serial.
8. **Stage 1, both players:** at the top of `fx_request_make` (the intercept on 0x801413E8): `cpu->lr == 0x80086838 ||
   cpu->lr == 0x80087394`: make the game's call (`hle_guest_call(FX_REQUEST_MAKE, ..)` as it does below), then
   effect = mem32[request+148] (request = r3 at entry), owner = `g_sanity_p2_in` ? 2 : 1. (`fx_request_make` installs
   under `fx_install`; if that can be off, read 0x80317DF0+148 and +5952 right after `SANITY_UPDATE` in
   `sanity_p2_step` instead: at most one new object per update.)
9. **Stage 2 for player 2, which also mends the fault in B2:** in `coop_thing_gone` (the intercept on 0x801DCB58):
   `g_sanity_p2_in`, op 26 or 18, r4 inside 0x80317DF0..+6024: keep (op, request), answer 1, do not queue. In
   `sanity_p2_step` right after `hle_guest_call(SANITY_UPDATE)` (their set still in), for each kept one do the drain's
   work: e = 0x80175310(mem32[req+168]); d = e ? 0x8014ED1C(e) : 0; r = d ? 0x80141824(d, req, mem32[req+192]) : 0;
   op 26 and r: 0x8014F820(r, 0x80141CD0); no r: 0x801433DC(mem32[req+192]) and req+192 := 0. Then note each child
   (mem32[block+136+4i], i < mem8[block]) as owner 2. GUESS: making them there and not in the drain is safe (the drain
   exists to make things outside a list walk; the tracker's update is not one).
10. **Stage 2 for player 1:** a new intercept on 0x80141824: r4 inside 0x80317DF0..+6024 (lr 0x80026DC0 or 0x80026CB8):
    make the call, then note the children with owner = `g_sanity_p2_in` ? 2 : 1. With this in, step 9 needs no noting of
    its own: its 0x80141824 call passes here.
11. **The draw:** in `effect_draw_by_room`, after the Bug Crawl block, look `effect` up in the table. Found, owner o,
    `camera_split_on()`:
    - shared room, `g_fx_rooms == 1`: do ONE of the two draws: owner 1 the first (marked out of view 1), owner 2 the
      second (parity flipped, `fx_facing_begin(cpu, 0, ..)`, marked out of view 0);
    - shared room, no per-half draw: one draw between `room_mtx_load(v)` + `effect_identity_load(v)` and the clean
      ones back, v = the other player's view;
    - apart: the view left out = the other's if the owner stands in the object's room (+56), else 2 (both).
    Split off: the game's own draw, as today.
12. Left open: 5's drops (0x80146EB0, type-23 things from 0x800CC934 at lr 0x80147068, made in the frame, not through
    0x8017C1D8): trace their draw before deciding; and whether a blood object keeps reading its request after it is
    made (0x8017380C and 0x80190058 were not read to their end; 0x80173A38 and 0x80190210 keep no pointer).

The rest (C), each small:

13. 10: run the door gate 0x800440D0 with player 2's set in when they are the asker (as `sanity_overlay` swaps it), or
    set bit 0x2000 in `g_sanity_p2.ext`+32 and clear it in memory. Without it their Locked in Room may never end.
14. 7: put all 60 bytes of 0x80324138 in `SanityOwn`, not one word.
15. 32, 33, 35 in a shared room: the user's decision. They take the room's enemies from player 1 as well; leaving the
    move out when the other player is in that room changes what the effect is.
16. Record 2's three words (r13-25408, -25412, -25416) and 8's three (r13-21704, -21708, -21712) into `kSanityWords`
    if a run shows the lean or the shake crossing over.
17. Sound stays for both (4, 7, 8, 24, 25, 31, the blood's and the phantoms' own): one set of speakers, as plan.md says.
