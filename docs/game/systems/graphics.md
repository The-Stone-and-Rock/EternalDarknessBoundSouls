# Character shadows and the graphics options

How the renderer's own shadows find what casts them and from where. It's all in the host's GXRuntime patch: `fill_shadow` and `submit_draw_plan` in `gfx/gxcore_draw.cpp`, `gfx/shadow.cpp`, and `render_character_shadows` in `gfx/common.cpp`. Failures: `docs/coop-failed-attempts.md`.

## A frame

1. **The game's own shadow:** each character's mesh, flattened on the floor, drawn twice into the EFB's alpha, then one full-screen quad darkens by it. With Character shadows on, both are left out.
2. **Who casts:** each place with a flattened draw is someone, up to 32 (`kMaxCasters`). That's more than can show (12 tiles): a monster's two flattened draws can be two places, and with 8 places four monsters left the players with no shadow. **The game leaves some flattened draws out now and then, with everyone standing still** (the corpse's and player 2's in room 338, every few seconds; every other world frame with a Tome up). Why isn't traced. So someone flattened in the last 30 frames still casts, for as long as a lit body is where they stand (`kFlatMemory`).
3. **Their draws:** every lot of lights on something no bigger than a person, with their place inside it.
4. **The camera:** the frame's first room draw's matrix (`g_sh.view`).
5. **Lamps: the room's own, from the game.** See The game's lights. Each character casts from the two strongest of the three their cell lists, each fading by its own power, easing at 5% a frame as they walk and at once when they're first found (a room's arrival). A lamp in their body or in the floor casts nothing. For a shadow a lamp is lifted to at least 1.7 times their height: from about head height the shadow runs nearly flat, and a torch threw a black shape across the floor.
6. **The tiles:** everyone's strongest shadow first, then the second lamps of whoever moved in the last 2 seconds, then other second lamps, 12 in all, each in a 1024 tile of a depth atlas, in perspective from its lamp. A shadow keeps its tile until it has faded out and a new one comes up from nothing (`lampShown`), so one never starts or stops in a frame. With 8 tiles and 8 people in room 338 no one had a second lamp, and a shadow turned round in one frame as the second became the stronger.
7. **Where the scene ends** (`glow_scene_split`, found at the display copy), each view rebuilds each pixel's place from depth, using the projection of the scene's last 3D draw **as uploaded for that view**, then darkens it.

## What doesn't cast or light

The Tome and interaction icons don't. A prompt's model has a small viewport, so it isn't `scene3d` and gets no fingerprint. The Tome's frames are flat, with too few 3D draws to run effects. A lit thing with no flattened draw near it is no caster, and only the room's and characters' lights become lamps. Checked: `lever.state` with player 1's Tome open keeps 2 casters and 3 lamps, and player 2's half keeps its shadows.

## A view and its frame

The camera code runs on the game thread, up to a frame ahead of the renderer. A view goes in the GX stream (`dol_aurora_split_set_view`) so it's used with the draws it was worked out for. **Anything else sent from the game thread to the renderer for one frame has to go the same way.**

## The game's lights

**They're on the disc, and in memory: `ed lights ROOM`.** Each room has one grid over it. `0x804F0780` is a list of the room's models, 16 bytes each, and +4 points at the grid (the same in the start of all 12 chapters): +4 cell count, +8 the far corner, +20 a cell's size as 3 u16, +27 +28 +29 cells across (x fastest), +32 the cells, which follow. A cell is 20 bytes: its middle (3 s16), +8 a lamp count (always 3), +12 its lamps, +16 its fills. A lamp or fill is 20 bytes: place, r g b a, power. The game's lookup is `0x801E6B7C(grid, place, ...)`, which blends the fills of neighbouring cells.

**A lamp's fade is its power:** `0x801E54BC` gives `GXInitLightAttn` a0 = 1.5 x power over k0 + k1 d + k2 d^2. A lamp's k are 1, 1, 0.8 and a fill's 9500, 3, 0.0385. So `reach` (where it's 0.3%) is 25 times the square root of power. A shadow's whole to half of it, and gone at it. A room is mostly baked light, so a lamp that's 1% at 470 units still plainly lights the floor there: faded by 1%, shadows were all but gone. The haze has a radius of its own, by the same power (`haze`).

**Near a door that pointer goes back and forth:** the game has the next room loaded too, and for up to 280 frames seen, `0x804F0784` alternates between this room's grid and that one's. Rooms load at the same place, so the next room's grid is the higher address (rooms 1, 242, 243, 246 against the disc). `room_lights_update` in `hle.c` holds the room's grid while it's still good.

`room_lights_update` sends the lamps and each cell's three in the GX stream when the grid changes (`dol_aurora_set_room_lights`).

**The haze** is a bar now, 0 to 100, in Options > Graphics > Effects (`haze` in settings.ini, 50 by default; an old `volumetric` of 0, 1, 2 reads as 0, 50, 100). It takes the 12 lamps nearest anyone, fading in and out, as a room can have 57.

## Lamps seen, by save

The old way of finding them, from `AURORA_SHADOW_LOG` and `ed lamps`, and now only useful as what was seen. World units.

| Save | Room | Lamp, colour |
|---|---|---|
| slot 1, p2cam-door | 147 | (-531,-456,310) and (-478,-456,310) orange 226,129,31; (-1084,837,-365) 255,227,151, below the floor there, so it casts nothing |
| slot 2, slot 9 | 147 | (3,-456,310) and (-50,-456,310) orange 226,129,31; (753,151,575) 244,195,89 |
| slot 3 | 21 | (1240,-219,231), (1574,218,231), (740,218,231), (479,-219,231) all 255,186,65 |
| slot 4 | 152 | (1447,-373,417) 187,162,94; (1396,-57,378) and (888,-4,483) 255,193,86 |
| slot 5 | 151 | (489,358,580) and (62,-297,387) 255,186,65 |
| slot 6 | 153 | (1125,-334,364) 232,229,219; (1396,-725,378) and (1396,-384,378) 255,206,122 |
| slot 7, slot 8 | 155 | (1025,-153,171) 230,183,39 |
| slot 10 | 338 | (-803,-895,236) 235,195,109; (-959,-2,348) blue 52,174,214 |
| karim-start | 338 | (95,-1,85) 235,195,109; (949,917,236) and (101,1212,180) lilac 211,193,244 |
| coop-apart | 100 | (-54,199,941) 134,96,188; (-1001,-584,522) 79,137,255; (-13,-1019,635) 118,59,132 |


## Measuring

- `ED_RUN_PROBE=1 sh tools/ed/run.sh SAVE NAME`, then `ed flicker RUN`: each player's shadows and picture, for every view of every moment drawn, from inside the renderer, so no capture and no window in front. The `shadows` lines are the mask; the `picture` lines are after shadows and haze, before post. One line's fields, with the camera and each tile: `logs/run-*/probe.txt`.
- `ed graphics [SAVE] [OPTION...]`: every value of every option, one change at a time. It runs the base twice, as the lamps flicker: a change under that can't be told from it. `detail` is for what's too fine for the small picture (sharpening, SMAA).
- `ed chapters N`: every room of a chapter from saves 1 to 12, by door script. It says where each player's shadow is against where they stand, the lamps against the disc, and flicker, starts, cameras and Tomes, with a sheet of every room to read. **Run it before and after a graphics change.** A frame-difference check can't see a shadow on the wrong lamp, or missing, or from the wrong person: `ed flicker` passed all of those. `docs/chapter-tour-review.md` has what the first tour found.
- `ed lights ROOM`: a room's lamps from the disc. A few rooms' tables (6,534 records) aren't read cleanly, so what it says is missing there means little.
- `ed lamps RUN`: needs `AURORA_SHADOW_LOG`.
- `ed test shadow-steady`.
- `AURORA_SHADOW_LOG=N`: every Nth frame, and each frame the flattened draws or lots of lights change. `AURORA_SHADOW_FRAMEDRAWS=1`: every draw of a frame whose flattened draws changed, and of the frame before. `DOL_INTERP_LOG=1`: `[interp]` rates and `[split-cost]`. `DOL_GX_TIME=1`: `[gx-time]`.

## Frame rate

**A Tome open in co-op:** the FPS counter counts pictures shown, so it says 60. It used to say 120, with one Tome or both: those were game-loop passes. The passes are by design (`bac33e8`, the comment on `TW_MODE_CHANGE` in `hle.c`): the loop runs the world's frame, then the Tome's, then the other Tome's in the copy, and only the world's waits for the retrace. So each keeps 60. `[screen]` (`DOL_INTERP_LOG=1`) counts what's really shown: 60.0 with one Tome or both. `ED_TW_WORLD_FRAMES=N` (testing) gives the Tome one frame in N+1 passes: at 2 that's 90 passes a second and the Tome updates 30 times, at 3 it's 80 and 20. The world stayed at 60, worst frame 18 ms, at all three. It can't reach 60 passes this way: the world alone is 60.

**Held by the user:** review unstable frame rates and true 120 in co-op only when they ask. They recall true 120 causing issues in the past, and what those issues were isn't recorded here: ask them.

The game steps at 60. Above that, Aurora replays the frame with blended matrices. A frame reaching its present over 33 ms after its retrace is behind and goes alone. Behind 6 times soon stops frames in between for 2 seconds, doubling if it soon trips again. The pace waits at most 19 ms from the retrace.

Solo at 120 shows 120. **Split-screen co-op is capped at 60** (`settings_apply`; the Frame rate row shows 60 and is for solo). Without the cap, 5 full redraws a game frame (each player's two moments, and the game's own frame, which nobody sees) didn't fit in 16.7 ms on this PC, even with every effect off, and the game slowed to about 50 Hz when they ran. To lift it, the game's own frame is the one to cut, and that needs care: its EFB copies and readbacks are what the game uses next frame.

With the cap, against the same scenarios just before it, frames over 25 ms: `shadow-steady` 13 of 1065 to 2 of 1104 (worst 45 to 27 ms), `lever-tomes` 79 of 1358 to 2 of 1469 (worst 36 to 33 ms). One run of each.
