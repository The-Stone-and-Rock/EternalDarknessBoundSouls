I didn't find it. I couldn't trace, from the code alone, what puts the green on player 1's body. Below is what the code does show, what it rules out, and one run that would settle it. Nothing was edited, built or run.

**What the game does during the charge**
- The charge step is 0x801C316C. That is the step the log lists for record 0x812E54D8 from frame 3720 to 3810. [read there]
- At spell clock 20 (0x801C3F58-0x801C3F8C) it calls 0x801D6D30 with:
  - the light handle at spell+644 +68,
  - the place spell+248,
  - a colour from 0x801C7BA4. [read 0x801C3F58]
- 0x801D6D30 builds a light with type 4 (+56) and stamps its room from r13-23276 (0x801D6D5C). It hands the light to queue op 19 (0x801D6DE8). [read 0x801D6D30]
- The place spell+248 is set when the cast is made: 0x801C27DC takes the position of r13-26524 at 0x801C286C. The cast ran in player 2's turn, so the place is player 2's. [read 0x801C286C]
- At clock 20 and later the same step fades that light through the handle: 0x801F2684, 0x801F2648, 0x801F2734 and 0x801F276C at 0x801C3224-0x801C3250. [read there]
- So record 0 in the probe (type 4, room 153) is the spell's own light, standing at the caster. [inferred]
- Nothing else in the charge step touches a body. Its two colour-layer calls go to other things:
  - 0x80125A70 at 0x801C3608 colours a node it has just made (0x800422FC(91,104,...) at 0x801C353C), probably the rune.
  - 0x80125A70 at 0x801C3CB8 belongs to the end-of-charge placement.
  - The streak calls (0x80147ED4) are effects at the spell's place. [read]

**How that light reaches a body, and when**
- It is read at draw time, outside any bracket. The actor draw 0x8011A278 calls the light function 0x801E6244 (at 0x8011A57C). [read]
- 0x801E6244 loads dynamic lights for every actor, the hero included:
  - the count comes from 0x801F19A0, capped at 3 (0x801E6370-0x801E6390);
  - the load loop is 0x801E666C-0x801E6800;
  - a light is used only if its room (+42) equals r13-23276 (0x801E6698).
  - The loop does not test +40 (active); only the count does. [read]
- Together, the caster stands at the light in the room in play, so the caster glows. When player 1 casts apart, the same holds. [inferred]

**Ruled out as the cause of player 1's green**
- The room-153 light lighting player 1. At player 1's draw, r13-23276 is 151, and both 0x801F19A0 and 0x801E6698 reject a room-153 light. 0x801E87F4 and 0x801E8B9C apply the same room test. [read]
- Light 7 above r13-26524. 0x801F2CB4 colours it from 0x803011FC, never from a spell. On a body, COLOR1 is only the far light (r24 starts at 16, at 0x801E6290, so the mask is r26&r24). [read]
- The player-only extra draw. The test at 0x8011A9E8 lets r13-26524 through to a second pass, but that pass is the shadow. Its material 0x80252DB8 is written only by the actor draw itself. [read]
- The fade flag r13-22064 / r13-22060. It changes alpha only (0x801E14F4). [read]
- Every read of r13-26524, r13-21492, r13-21496 and the casting words in the actor draw's call tree, three calls deep. The only ones are the budget, the fade, the shadow test, the 0x08 rule and a debug draw (0x80129B64). [read]
- The body glow (+722/+724/+700/+704). Only 0x80119C34 writes it (82 callers). The callers in spell code (0x801D3268, 0x801D4290) glow the target spell+224, from step functions that are not this spell's. Those in effects code are the fn144 script and the hit system. [read]
- The torch record. Nothing in spell code references 0x8030AA98. [read]

**Why player 1 gets it apart: not found**
- No game path I read can put a room-153 light, or a glow on an object, onto player 1. The green has the look of a light shading the body; the probe says the only light on is in room 153.
- So the transfer most likely happens in host code that is only active while the players are apart. In order of suspicion: [inferred]
  1. The renderer's spell lights per half: `dol_aurora_set_spell_lights_views`, hle.c ~10187-10223. Check that it was really off in the "norunes" run, and its view bit (bit 0 is player 1's half).
  2. Lights loaded inside the per-room effect brackets (`fx_emitters`, the effect-step bracket, `fx_request_make`) that run 0x801E87F4 through the effect model draws. Note 0x8011035C and 0x80105F30 call 0x8011A278 with no grid.
  3. G3 (`lights_section`), which loads room 153's lights before section 3.

**The run that would settle it (one run, logs only)**
At frames 3760-3790, log:
- for player 1's object: r13-23276 when 0x801E6244 is entered, the count 0x801F19A0 returns, and which records the loop takes;
- the light mask passed to 0x801E58C8 at 0x801E691C;
- player 1's object +720/+722 and layer 15 (+576[15] +44..+56);
- the views mask given to `dol_aurora_set_spell_lights_views`.

Then repeat with these turned off one at a time: `ED_LIGHTS_OFF` (G1-G3), `ED_FX_ROOMS=0`, `ED_RUNES_3D=0`, and with no rune lights at all.

**The smallest host change, for each possible answer**
- Spell lights per view: give the light the caster's room (spell +12, then the room via `coop_object_room`), so view bit 1 is set apart. Keep `views 3` together and the game's own path solo.
- A bracket that lights player 1 while room 153 is in: in `lights_actor_call`, for any object whose +592 is not room1 while `lights_apart`, put r13-23276 to player 1's room for that call. Equivalently, open no room-153 bracket around a player-1 draw.
- Player 2 then gets the caster glow from the game's own light through G2 (room 153 in, r13-21492 theirs), provided the light's place is theirs, which 0x801C286C confirms. Solo, together and player 1's own cast stay as they are, because every change is gated on `lights_apart`.

I can't name the change until that run shows which of these is happening.

### REFUTE
I checked the reader's claims against the disassembly, hle.c, the run logs and the pictures. Most of its game-side claims hold. I couldn't find the cause from static reading either, so I don't name a fix. Below: the verdicts, what I can add, what I can rule out, and the probe that settles it. Nothing was edited, built or run.

**Reader's claims: verdicts**
- Charge step 0x801C316C. At clock 20 it calls 0x801D6D30 with handle spell+644+68, place spell+248 and colour 0x801C7BA4 (0x801C3F58-0x801C3F78). [confirmed]
- 0x801D6D30 builds a request with room r13-23276 (0x801D6D5C), +56=4, and queues op 19 (0x801D6DE8). [confirmed]
  - Addition: the record itself is filled at the queue's drain by 0x801F29F0. Its +42 is the request's room, or r13-23276 at the drain if that is 0 (0x801F2A20-0x801F2A38). [read]
  - So the record is room 153, matching the probe.
- spell+248 is set from r13-26524 at 0x801C286C. [confirmed, via 0x80118274]
- Fades at 0x801C3224-0x801C3250. [confirmed]
- 0x80125A70 at 0x801C3608 colours a node from 0x800422FC(91,104, spell room +8) with layer 15 in the alignment colour. These are the rune pieces, not a body. [confirmed]
- 0x801E6244: count from 0x801F19A0, capped by r13-26892 and 3 (0x801E6370-0x801E6390). The load loop 0x801E666C-0x801E6800 tests +42 against r13-23276 (0x801E6698) but not +40. [confirmed]
- 0x801E87F4 and 0x801E8B9C both apply the same room test (0x801E8A8C, 0x801E8C18). [confirmed]
- Light 7's colour is the .rodata constant 0x803011FC, through 0x801F1DF4 (0x801F2D84-0x801F2D9C). [confirmed]
- r24 starts at 16 (0x801E6290). [confirmed]
- The second pass after 0x8011A9E8 is the flattened shadow: 0x80119A30 flattens it and it reads layer 15 for alpha. [confirmed as read; I didn't trace the material]
- The fade flag r13-22064 (0x801E14C8, 0x801E14F4) only changes alpha. [confirmed]
- +722 is written only by 0x80119C34. [confirmed; I didn't check all 82 callers]
- Nothing in spell code references TORCH_RECORD. [unverified]
- The "norunes" run had the host rune lights off. [confirmed: its log has no `[runes] model` line, and hle.c:10187-10223 sends only g_runes]

**What I add**
- Only actors with +720 bit 0x100 get COLOR1 specular. Every other actor has COLOR1 turned off by 0x801E5D34(0) at 0x8011A5FC. [read]
  - So player 1 keeps a specular channel that a creature next to him does not.
  - Player 1's green has the mottled, shiny look of specular on armour. The creature beside him is not green (shots 000034 in both apart runs).
- For the hero budget, COLOR1 holds only copies of the grid lights loaded in the same call (0x801E6594, index r29+4, r7=1). Its mask is r26&r24, set every call at 0x801E691C. [read]
  - No mask carries over between draws.
- The grid lights come from the room's grid (0x801E5DDC). Their tint r13-26916 (0x801E1334) is never written by game code. [read]
- At clock 10 the charge step makes a kind-23 node with 0x800CC8BC (0x801C3F14), stores it at spell+644+48 and steps it each frame (0x801C31F0-0x801C3214). [read]
  - 0x800CB238 fills it with a 640x480 screen quad, a colour and a 10-frame clock. [read]
  - The reader missed this. It is a screen-space colour quad. If it blends by destination alpha, its colour lands on whatever left alpha in the EFB. [inferred, unverified]
- Player 1's room word is 151 (both.tsv p1room = +592), so G2 (`lights_actor_call`, gated on +592 == room1) never brackets player 1's light call. [read hle.c:27504-27515]
- G3's plain reload before section 2 puts back room 151's masks before any actor draws. [read hle.c:27548-27560]
- Positions: player 1 is at -250,127,0 in 151 and player 2 at 1150,-100,0 in 153, so they are about 1400 apart. [both.tsv]
  - The light's power is 600 (0x801D6DD0), so its reach is about 600 units. [inferred]
  - Even overlapping coordinates couldn't put a light at player 2's place onto player 1.

**Why player 1 gets it apart: not found.** Within the game's lighting, the only green source is that one type-4 light. A room-151 draw can only take it if r13-23276 is 153 during player 1's light call, and none of the host room brackets I read encloses the actor draw list 0x80119D60. So two explanations remain:
1. A host or renderer path that is only active apart. The strongest candidates are in the GXRuntime patch [inferred]:
   - the renderer's per-view "lots of lights", which are matched to characters by their place, with view 0 treated as the game's own frame;
   - per-pixel lighting or skin scatter taking another view's lot.
   - Each player sits in a similar spot of their own half, so a lot matched by view-space place could swap players. This fits "player 1 green, the caster not" apart and "the caster green" together.
   - It does not obviously fit "player 1's own cast apart lights only player 1", unless only view 0 takes the wrong lot.
2. The kind-23 screen quad blending by destination alpha that the hero's pass left. [unverified]

**The run that settles it (logs only), frames 3740-3800, player 2's quick cast apart:**
- At player 1's 0x801E6244 entry (lr 0x8011A580, r15 = player 1's object): r13-23276, 0x801F19A0's count, and the masks passed at 0x801E691C.
  - A count of 0 here proves the game gives player 1 no green light.
- The kind-23 node's colour and its blend mode, from an `AURORA_DRAW_DUMP` of one charge frame: look for a full-screen draw with DSTALPHA.
- The same frame with each of these off in turn: the renderer's per-pixel lighting and skin scatter (their settings), `ED_LIGHTS_OFF`, and `ED_P2_HERO_LIGHT=0`.

**The smallest change, by answer**
- If the renderer's per-view lot matching is the cause: match each character's lot within its own view's room set. That is in the patch, not hle.c, and solo or together is untouched because there is one view set.
- If the kind-23 quad is the cause: for a spell of player 2's room while `lights_apart`, have that quad's draw read the alpha player 2's body leaves, or run it under player 2's identity. Gate it on `lights_apart` so solo, together and player 1's own cast are unchanged.
- If it's neither: the dump above names the draw.

Also seen: in logs/run-20261115-050015-spell-lights-probe-a, at frame 3890 (the release, after the charge) record 1 was on with room 151, type 4. Player 2's second spell light was stamped with player 1's room. That run is 16 minutes older than the norunes run, so it may predate the spell-room fix. It needs checking on the current build.