# 3D runes on the floor when casting (the user's wish, 2026-11-09)

**Paused (2026-11-09).** The user played it and didn't like it: the 3D runes didn't fade or blend in well, the effects
didn't show well, and **black blocks appeared**. Remastered spells is now off by default (settings.c `spell_fx`); off,
every spell is the game's own. Everything below is kept for when it's picked up again; start with the black blocks.

The runes the game draws on the floor's Circle of Power while a spell is cast are flat glyphs. The wish: each rune's
own 3D item model there instead, tinted and glowing in the spell's alignment colour (Chattur'gha red, Ulyaoth blue,
Xel'lotath green, Mantorok purple). Only how they're drawn changes, never how casting works. Started 2026-11-09 (uncommitted, `ED_RUNES_3D=1`): see Progress.

## Seeing a cast in a test (works, logs/run-20261109-010604-cast-before)

From the archived chapter 7 save, player 1 (Paul) has no Tome, so the spawner gives them one first:

    ED_CHEAT=1 ED_ITEM_SPAWN=2250,30,30,300,1 ED_QUICK_SET=2300,0,0x00010481 \
    ED_INPUT_SCRIPT="2280,2288,0x0200;2340,2348,0x0200;2380,2388,0x0200;2420,2428,0x0200;2700,2708,0x0800" \
    ED_RUN_SHOTS_MS=150 ED_RUN_FRAMES=3250 sh tools/ed/run.sh saves/old-2026-11-09/slot7.state cast-before

- `ED_CHEAT=1`: All magick (runes, circles and spells written in the Tome).
- Type 30 is the Tome; its find runs the story's scene (scripts 2460, 137, 2456) and moves both players to room 115.
- `ED_QUICK_SET` (hle.c `quick_test_set`) puts spell `0x00010481` in player 1's first quick slot (0x80334DE8); GameCube
  Y (0x0800) casts it at 2700: `[coop] spell 0x812D36D8 (0x00010481) cast by 0x1` at 2702.
- Pictures: the ring at ~2730 (shots/000032.jpg), a flat red glyph on the floor at ~2770 (shots/000034.jpg); copies in
  `Screenshots for Claude/runes-before/`. Both halves draw player 1's circle. The room's camera hides most of it: a
  better place to cast would show the whole circle.

## What's known

- The HD pack has 551 rune textures in `textures/GED/runes` (mips in `textures/GED/mipmaps/runes`): Dolphin names,
  `tex1_WxH_HASH_14`, 14 the GX format (CMPR). Tome pages, rune items and floor glyphs are mixed in; which are the
  floor's isn't found yet.
- The renderer already picks textures out by that hash: `aurora_glow_add_texture(format, w, h, hash, strength)`
  (settings.c `settings_apply_graphics`), so the floor glyphs can be made to glow at once, once known.
- The rune items have world models (runes are item types; `runtime/host/item_spawn.inc`), and the game's own spawn
  (`item_spawn_at`, ed3) can put a model at a place.

## Next

1. Find the floor glyphs' draw: the textures bound during a cast (a texture log in the renderer, or the pack's names
   matched against a frame's draws), and the game function that places them (the circle's points).
2. Smallest first: those glyphs glowing in the alignment colour (renderer only).
3. Then the 3D swap: at each glyph's place, the rune's own model (kept per player's view in co-op), tinted, turning
   slowly; the flat glyph hidden.

## Progress (2026-11-09, uncommitted)

- hle.c `runes_frame` / `rune_glyph_draw` / `rune_spawn`, on with `ED_RUNES_3D=1`: walks the spells being cast, places
  each glyph's rune model above it (machine 62, effect 1, as ed187's vortex; tinted with 0x801C7B24's colour and the glow
  bit) and skips drawing that flat glyph; a rune whose model the level hasn't loaded (0x801DCED0) keeps its glyph.
- Seen working: `ED_RUNE_SHOW=2500,20,2` (hle.c `rune_test_show`) put the Ulyaoth rune (model 20) beside player 2 in
  chapter 3, upright, glowing Ulyaoth blue, in both halves (logs/run-20261109-013458-jtg-rune-show,
  `Screenshots for Claude/runes-after/`).
- In a cast: Ulyaoth Dispel `0x00010482` from a cold chapter 3 (spawn the Tome, type 30) placed the Ulyaoth model on its
  glyph (logs/run-20261109-013133-jtg-runes-uly2), but the room's camera is too far to judge it.
- **Models not loaded:** chapter 3 has the Ulyaoth model (20) but not Nethlek (14) or Redgormor (23); chapter 7 has none
  of 14, 18, 23. The game loads only the item models a level uses (r13-26448's list). To show every rune anywhere, the
  rune models (12-25) need loading with the level, as the bodies are borrowed for other characters.
- Enchant Item (Ulyaoth, 3-point: `0x00010302`, Antorbok + Magormor) from a quick slot didn't start in chapter 3; it opens
  an item picker first (0x8011048C), which the test presses didn't get through.
- Seen in the logs: a spell is listed twice (the circle's record 0x812D36D8 and the spell's 0x812D26E0), so the model was
  made twice at one place; one per glyph place is enough.

## Progress, later the same night

- **Working in a real cast** (logs/run-20261109-020232-jtg-runes-c25, `Screenshots for Claude/runes-after/cast-3d-c25*`):
  Ulyaoth Dispel `0x00010482` cast by player 1 at (-620, 200) in chapter 3's room 150: the Ulyaoth model lies face up on
  its own glyph at (-840, 75), the flat glyph kept under it for the game's glow (the user wants the original's glow).
- The glyph is a 128 x 128 square on the floor, square to the world, top toward +y, 10 above the floor (its corners at
  +80). The model is laid with a quarter turn about x (quaternion at object +44, `rune_lie`; `ED_RUNES_FLIP=1` the
  other way over) and made 25 back along y to sit centred (it stands on its base; 0 and 50 back were both off).
- Its lifetime is the spell's place in the list (r13-22324): 0x8017F9D4 is reached through the effect's +328, so its
  hook doesn't run, and an early version took that for the glyphs being gone and deleted each model 10 frames in.
  `ED_RUNES_3D=2` (hide the flat glyph) doesn't work for the same reason.
- The three glyphs of 0x00010482 from (-620, 200): Nethlek (-623, 450), Redgormor (-407, 74), Ulyaoth (-840, 75), at
  radius 250 (`ED_RUNES_LOG=1` prints each glyph's rune and place).
- Test placement: put the caster with `ED_PUT` after the Tome's pickup scene (it moves player 1 up the stairs), and on
  level floor: (-476, 245) stood them on a ledge, the circle in the air.

## Still to do

- Load the other rune models with the level (12-25), so every rune shows, not only the level's own.
- Size: the model is about 100 long against the glyph's 128 (`0x8012602C` scales, as ed187 does).
- Turn: done 2026-11-09 for Ulyaoth. `ED_RUNES_TURN` 0-3 tried (sheet `Screenshots for Claude/runes-after/turns.png`): turn 2 (face down, top toward -y) is the glyph's own shape and way (pulled 80 clear in logs/run-20261109-021337-jtg-runes-dy-80), lying on it with a shift of -10 along y (logs/run-20261109-021119-jtg-runes-dy-10). Now the default. Check the other runes once their models load.
- Fade: the model appears and goes at once while the glyph fades in and out; the game's colour step 0x80125A70(obj, 15, &a, &step, &b, mode), as ed187 uses it, may fade its alpha (argument order untested).
- The user's look (their reference picture): rune slabs glowing in the alignment colour with coloured flames rising and
  light trails round the caster: the game's own spell particles, to be found.
- Enchant Item's item picker in a test.

## Overnight 2026-11-09: one spell remastered (Chattur'gha Recover, chapter 4)

Test: `ED_RUNES_3D=1 ED_CHEAT=1 ED_ITEM_SPAWN=2400,30,30,300,1 ED_QUICK_SET=2500,0,0x00011041
ED_INPUT_SCRIPT="2430,2438,0x0200;2490,2498,0x0200;2530,2538,0x0200;2570,2578,0x0200;2780,2788,0x0800"
sh tools/ed/jtg.sh 4 NAME` (Karim casts on the round dais in room 338; all three runes' models, Narokath 15, Santak 25
and Chattur'gha 18, are level 4's own). Pictures: `Screenshots for Claude/runes-after/chattur'gha-recover-before-after.png`,
`chat-compare.png` (original / 3D / 3D with full flames over time), `chat-fire2.png`.

Done:
- All three runes in 3D, laid on their glyphs (turn 2).
- They fade in with their glyphs: the glyph's alpha is +43 of the record at +76, 0 to 255 in 18 frames
  (logs/run-20261109-024126-jtg-rec-chat, `ED_RUNES_LOG=1`). They hold for the whole spell, kept by the spell's word
  (its circle record and then its spell record both carry it), and fade out over 40 frames after it.
- Colour set each frame by the game's own `0x80126234(obj, 15, &rgba, 1)`. Chattur'gha's is a red ember with orange in
  it, flickering (`rune_colour`).
- Rising wisps: the game's own end-of-spell smoke (0x80177588), every 30 frames at alpha 0x48 (`ED_RUNES_FLAME_A`,
  `ED_RUNES_FLAMES=0` none). At full strength every 9 frames they made a red fog over the circle; faint, they barely
  show. The wisp is a smoke cloud, not a flame.

Not yet the reference's look:
- **Burning.** The runes read as red glass slabs with bright edges, not burning. Candidates from the trace: the game's
  star sparkle and streak (0x80147ED4, textures 49/53/56 + alignment, needing a 192-byte record that lasts) as rising
  embers, and the room's torch fire (0x80197690, its 144-byte block not yet decoded) as real flame.
- **Bloom.** Glow Strong (HDR off) halos the beams and glyphs but not the runes, which are alpha-blended, not additive.
  The renderer plan (a host-set "spell" tag feeding the emit chain with SrcAlpha/One, shown in SDR) is in the trace
  report: workflow wf_8702c13a-32c, part "glow".
- **Every rune anywhere.** Load the 14 rune models from levels 3-7's U_LevelNN.bin at chapter load (trace part
  "models": hle_pack_load / continue_pack_load, SCRATCH_RUNES 0x82300000, about 210 KB).

## Smooth glow on the floor runes (2026-11-09, texture pack, uncommitted)

The blocky, streaked edge the user saw is each glyph's glow mask (textures/GED/shadows and transparency, 64x64): noisy
upscales. `tools/rune_glow.py apply` rebuilds the paired masks as smooth halos (the mask blurred so its streaks go, plus
a soft bloom from the rune), originals to `textures-backup-glow/` (ignored by git). Pairs come from the game, not
guessed: `ED_TEXPACK_LOG=1` logs each pack file at its first load, and a glyph loads its rune then its mask
(logs/run-20261109-030729-jtg-texlog for Chattur'gha Recover). Shape matching (`rune_glow.py pair`) found only 1 of 3
known pairs and isn't used. Done for 5 masks: Recover's (7e6fc5ed > a0539bd0, 31f0d2db > 8e81d609, c1388d9a > 19e29cae)
and Dispel's (d9d14d3f > 2e22c13c, 35fe32ae > d53fb783), listed in build/rune_glow/pairs.txt. Picture:
`Screenshots for Claude/runes-after/chat-aura-before-after.png`: a clean red rim in place of the scratchy edge. The aura
can't spread past the glyph's square (the mask shapes the glyph's own glow), so a wider aura needs bloom.

Next: log every rune's pair by casting each spell once (ED_TEXPACK_LOG) and smooth them all; the left 3D rune of Recover
(Santak's or Narokath's model) reads pale yellow, not ember red; flames and bloom as above.

## Spell light, soft effects (2026-11-09, uncommitted)

- **Rune light on surfaces (renderer).** The host sends each lit rune as a spell light (`dol_aurora_set_spell_lights`,
  hle.c `runes_frame`: 40 above the rune, its alignment's colour, Chattur'gha an ember, as bright as it shows, flickering).
  The renderer's haze pass (shadow.cpp `vol`) lights the surfaces round it from all of the rune (distance from its edge,
  64), soft, by which way a surface faces, with a little shine, and marches toward it through the depth for shadows
  (`spell_shadow`). It runs whatever the haze setting (`kSpellHaze`, `AURORA_SPELL_HAZE`), needs Character shadows on
  (the atlas), and only while a spell is lit. Tuning: `ED_RUNE_LIGHT=BRIGHTNESS,FALLOFF` (default 1.4,90 and air 0.6, as in the pictures; at 2.5 and above the air fogged the room). Picture:
  `Screenshots for Claude/runes-after/chat-light-shadow.png`. Patch regenerated (patches/gxruntime).
- **Feathered effect sprites (texture pack).** `tools/vfx_soften.py LIST` softens the hard cut-out edges and dark rings of
  the spell's streaks, sparkles and glows (28 for Chattur'gha Recover, list from `ED_TEXPACK_LOG`: build/rune_glow/vfx.txt,
  plus Ulyaoth's streak 7d9052f3). Originals in textures-backup-glow/. Picture: chat-soft.png.

Still to do: the runes' own bloom (spell
glow in the renderer, planned in the trace); the white "spirit" streaks and sparkles in the rune's colour (the streak
spawn 0x80147ED4 is given white 0x80301208 for textures 49 and 53: its colour word could be the alignment's); the pale
3D rune; every other spell's glyph masks and sprites (cast each once with `ED_TEXPACK_LOG=1`).

## The remaster as an option (2026-11-09, uncommitted)

**Options > Graphics > Effects > Remastered spells** (`spell_fx` in settings.ini, on by default; `ED_RUNES_3D=0/1/2`
overrides it for tests). On, a spell gets: its runes as 3D stones on their glyphs, fading with them; their light on the
surfaces round them with its bloom pool and shadows (renderer, shadow.cpp `vol`); its spirit streaks and sparkles in
its alignment's colour (hle.c `streak_watch`: the white 0x80301208 given to 0x80147ED4 swapped for the alignment's, a
word at 0x82300000). Off, the spell is the game's, apart from the texture pack's smoothed glows and feathered sprites
(tools/rune_glow.py, tools/vfx_soften.py; to undo those, copy textures-backup-glow/ back over their files).

Tested on Chattur'gha Recover only (the user: the three runes first): logs/run-20261109-062606-jtg-chat-full,
`Screenshots for Claude/runes-after/chat-full.png`.

Open:
- One rune's 3D model (the left one in room 338) stays pale grey glass: its colour doesn't take the tint (0x80126234 with
  mask 15 or 0xFF, logs/run-20261109-062737-jtg-chat-tint), and its outline sits a little off its glyph. Each rune's
  model needs its own fit (turn, shift, size) checked; a rune whose model doesn't fit could keep just its flat glyph.
- The other spells: their glyph masks and sprites, logged by casting each once (ED_TEXPACK_LOG=1), then the two tools.
- Shadows from the rune light are in but hard to see at room 338's camera.

## Soft beams, pulsing light (2026-11-09, uncommitted)

- **The beams** (the straight columns rising at a cast) are the 32 strips tex1_16x64_* (build/rune_glow/beams.txt): proven
  by making them see-through, when the columns went (logs/run-20261109-064032-jtg-beam-clear). They're drawn added: alpha
  is 255 throughout and the colour is the light, and a strip's colour didn't reach 0 at its sides (36 of 255), so each
  beam ended in a straight hard edge. `tools/vfx_soften.py --beam` fades the colour to nothing at both sides, the core
  a little brighter (`--clear` makes them see-through, for finding what draws). Picture: chat-beam2.png.
- **Pulse:** the rune light breathes, a slow swell about once a second (hle.c, `pulse`), over the ember flicker; brighter
  (1.8, falloff 95) and its bloom pool wider and brighter (shadow.cpp: exp(-edge / 36) * 1.3). Picture over time:
  chat-pulse-sequence.png.

## Fire columns and what blocks the reference's look (2026-11-09, uncommitted)

- The beam strips are grey and coloured by the game per alignment, so `tools/spell_fire.py` redraws all 32 as a round
  column of flame (the original's rise and fade along each kept, flame tongues across, different per strip). Kept no
  brighter than the original: brighter, the tint washed to pink-white (logs/run-20261109-065654-jtg-chat-fire3).
  Picture: chat-fire4.png. Chattur'gha's light, 3D runes and colour now a deep red (it read orange).
- **The beams' width is the game's**: the strip is a narrow card, and the fire already fills it. To make each column as
  wide as its rune its geometry has to change, and it isn't found yet:
  - not stored as four corners at +80 like the glyphs (no upright card within 400 objects of a glyph);
  - the spirit streaks' spawn 0x80147ED4 (whose `16, 4` arguments might be its size) is called directly inside the
    recompiled code, so its hook (installed, logs "[runes] streak watch on") never runs: the streaks' recolour
    (`streak_watch`) doesn't work either: the redder streaks came from the rune light;
  - no streak objects (update 0x80175D08 / 0x8018D3B4) within 400 objects of a glyph either.
  Next: log the beam strips' draw (which function binds tex1_16x64_*) to find its object and its size, then widen it there.

## Off means the original, textures too (2026-11-09, uncommitted)

The smoothed glows, feathered sprites and fire beams were written into textures/, so they showed with Remastered spells
off. Now textures/ has the originals again (from textures-backup-glow/, which is now only a duplicate of them), and the
remaster's 64 textures are in **textures-spellfx/** (same paths, ignored by git). texpack.cpp reads that folder too and
loads a texture from it only while `spell_fx` is on (`texture_path`); its `generation()` includes the option, so the
renderer reloads what it has when it's switched. The three tools read textures/ and write textures-spellfx/; textures/
is never written. Checked from a cold chapter 4: off, no textures-spellfx file loads and no rune is made, and the cast
looks as before any of this; on, 61 load and the three runes are made (logs/run-20261109-112839-jtg-fx0, -112937-jtg-fx1;
`Screenshots for Claude/runes-after/spellfx-off-on.png`). Switching it from the menu mid-game isn't tested.
