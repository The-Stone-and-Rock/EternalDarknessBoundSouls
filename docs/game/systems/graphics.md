# Character shadows and the graphics options

How the renderer's own shadows find what casts them and from where. It's all in the host's GXRuntime patch: `fill_shadow` and `submit_draw_plan` in `gfx/gxcore_draw.cpp`, `gfx/shadow.cpp`, and `render_character_shadows` in `gfx/common.cpp`. Failures: `docs/coop-failed-attempts.md`.

## A frame

1. **The game's own shadow:** each character's mesh, flattened on the floor, drawn twice into the EFB's alpha, then one full-screen quad darkens by it. With Character shadows on, both are left out.
2. **Who casts:** each place with a flattened draw is someone. **The game leaves some flattened draws out now and then, with everyone standing still** (the corpse's and player 2's in room 338, every few seconds; every other world frame with a Tome up). Why isn't traced. So someone flattened in the last 30 frames still casts, for as long as a lit body is where they stand (`kFlatMemory`).
3. **Their draws:** every lot of lights on something no bigger than a person, with their place inside it.
4. **The camera:** the frame's first room draw's matrix (`g_sh.view`).
5. **Lamps:** lights with `distatt` z of 0.1 or more, on characters and the room only, kept in world space for the room (`g_lamps`). Each character casts from their two nearest, easing at 5% a frame. A lamp in them or in the floor casts nothing.
6. **The tiles:** the 8 strongest shadows, each in a 1024 tile of a depth atlas, in perspective from its lamp.
7. **Where the scene ends** (`glow_scene_split`, found at the display copy), each view rebuilds each pixel's place from depth, using the projection of the scene's last 3D draw **as uploaded for that view**, then darkens it.

## What doesn't cast or light

The Tome and interaction icons don't. A prompt's model has a small viewport, so it isn't `scene3d` and gets no fingerprint. The Tome's frames are flat, with too few 3D draws to run effects. A lit thing with no flattened draw near it is no caster, and only the room's and characters' lights become lamps. Checked: `lever.state` with player 1's Tome open keeps 2 casters and 3 lamps, and player 2's half keeps its shadows.

## A view and its frame

The camera code runs on the game thread, up to a frame ahead of the renderer. A view goes in the GX stream (`dol_aurora_split_set_view`) so it's used with the draws it was worked out for. **Anything else sent from the game thread to the renderer for one frame has to go the same way.**

## Lamps seen, by save

From `AURORA_SHADOW_LOG` and `ed lamps`. A room's list depends on where the players stand: each character only holds the lamps nearest them. World units.

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

Lights aren't in `ed room`: the room files' lights haven't been read from the disc.

## Measuring

- `ED_RUN_PROBE=1 sh tools/ed/run.sh SAVE NAME`, then `ed flicker RUN`: each player's shadows and picture, for every view of every moment drawn, from inside the renderer, so no capture and no window in front. The `shadows` lines are the mask; the `picture` lines are after shadows and haze, before post. One line's fields, with the camera and each tile: `logs/run-*/probe.txt`.
- `ed graphics [SAVE] [OPTION...]`: every value of every option, one change at a time. It runs the base twice, as the lamps flicker: a change under that can't be told from it. `detail` is for what's too fine for the small picture (sharpening, SMAA).
- `ed lamps RUN`: needs `AURORA_SHADOW_LOG`.
- `ed test shadow-steady`.
- `AURORA_SHADOW_LOG=N`: every Nth frame, and each frame the flattened draws or lots of lights change. `AURORA_SHADOW_FRAMEDRAWS=1`: every draw of a frame whose flattened draws changed, and of the frame before. `DOL_INTERP_LOG=1`: `[interp]` rates and `[split-cost]`. `DOL_GX_TIME=1`: `[gx-time]`.

## Frame rate

**A Tome open in co-op:** the FPS counter says 120, with one Tome or both, and that's game-loop passes, not pictures. It's by design (`bac33e8`, the comment on `TW_MODE_CHANGE` in `hle.c`): the loop runs the world's frame, then the Tome's, then the other Tome's in the copy, and only the world's waits for the retrace. So each keeps 60. `[screen]` (`DOL_INTERP_LOG=1`) counts what's really shown: 60.0 with one Tome or both. `ED_TW_WORLD_FRAMES=N` (testing) gives the Tome one frame in N+1 passes: at 2 the counter says 90 and the Tome updates 30 times a second, at 3 it says 80 and 20. The world stayed at 60, worst frame 18 ms, at all three. It can't reach 60 passes this way: the world alone is 60.

**Held by the user:** review unstable frame rates and true 120 in co-op only when they ask. They recall true 120 causing issues in the past, and what those issues were isn't recorded here: ask them.

The game steps at 60. Above that, Aurora replays the frame with blended matrices. A frame reaching its present over 33 ms after its retrace is behind and goes alone. Behind 6 times soon stops frames in between for 2 seconds, doubling if it soon trips again. The pace waits at most 19 ms from the retrace.

Solo at 120 shows 120. **Split-screen co-op is capped at 60** (`settings_apply`; the Frame rate row shows 60 and is for solo). Without the cap, 5 full redraws a game frame (each player's two moments, and the game's own frame, which nobody sees) didn't fit in 16.7 ms on this PC, even with every effect off, and the game slowed to about 50 Hz when they ran. To lift it, the game's own frame is the one to cut, and that needs care: its EFB copies and readbacks are what the game uses next frame.

With the cap, against the same scenarios just before it, frames over 25 ms: `shadow-steady` 13 of 1065 to 2 of 1104 (worst 45 to 27 ms), `lever-tomes` 79 of 1358 to 2 of 1469 (worst 36 to 33 ms). One run of each.
