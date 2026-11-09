# Graphics settings review (2026-11-08)

Read-only code review of every graphics setting: four reviewers, then a checker per area that tried to
disprove each claim. Nothing here has been changed or measured in a run yet. Verdicts: confirmed, or
partly (the code is real, but the impact was overstated or depends on settings).

## First: the user's 120 fps is never used

`settings.c:345` applies `mod_coop ? 60 : frame_rate`. That checks the co-op *setting*, not whether a second
player is in. With `mod_coop = 1` in settings.ini, solo play also runs at 60 and no in-between frames are
drawn. So with the current settings, the sword ghosting can't come from the in-between frames.

## Sword-swing ghosting

At 60 (the user's case), the candidates are:
- HDR highlights (partly): with HDR on, additive draws (trails, sparks, magick streaks) feed a soft half-res
  highlight mask that brightens them up to 3.5x (`glow.cpp:84-89, 214-216`, gain `settings.c:388-393`). Test:
  swing in the same spot with HDR off, then on.
- The screen itself: 60 fps on a sample-and-hold LCD/OLED smears fast motion. No temporal pass exists (no TAA,
  no frame blending), so the renderer keeps no previous frame.

Above 60 (solo, once the above is fixed), the in-between frames add their own:
- Characters are skinned on the CPU, so their vertices stay at the newest pose while only matrices blend
  (`interp.cpp:211-218, 410-452`; `docs/coop-failed-attempts.md:218`). The weapon is a separate object held by a
  bone, so blade and hand can separate on in-between frames (partly).
- A matrix that turns more than about 20 degrees in a game frame "jumps" to the new pose while neighbours blend
  (`interp.cpp:83-99, 338-371`), so a fast blade can snap ahead of the arm (partly).

## Frame drops and wasted work (by priority)

| # | Finding | Verdict | Where | Fix size |
|---|---|---|---|---|
| 1 | On a 119.88 or 144 Hz screen, the phase loop drops phase 1.0, so about 96% of game frames are drawn three times (2 in-between + 1 hidden). | confirmed | `aurora_backend.cpp:2708-2712`, `common.cpp:1724` | few lines |
| 2 | A late hidden frame presents nothing, so the last picture holds for 3+ refreshes (a hitch). | confirmed | `common.cpp:2190-2252` | few lines |
| 3 | The back-off that turns in-between frames off after heavy moments can stay off for up to 5 minutes, silently. | confirmed | `aurora_backend.cpp:2685-2706` | few lines |
| 4 | HDR on with Glow off (current settings): the full glow blur pyramid and its cover pass run every frame and nothing reads them. | confirmed | `glow.cpp:615, 760-765, 840-845` | small |
| 5 | Split co-op with HDR: highlights, Glow and emit glow never reach the screen, but all their GPU work runs. | confirmed | `aurora.cpp:804`, `glow.cpp:835-846` | small |
| 6 | Split co-op: the hidden game frame runs the whole post chain (SSAO, shadows, haze, shafts, SMAA, sharpen, glow), and the shadow atlas is built 3 times per game frame. | confirmed | `common.cpp:1671-1699, 2558-2567` | medium |
| 7 | The shown frame reads shadow and haze parameters written for the last in-between moment (shadows half a frame off). | confirmed | `shadow.cpp:575-635, 767-803`, `ssao.cpp:408` | medium |
| 8 | With 4x MSAA, every full-screen effect rewrites and resolves the multisampled image (up to 5 times). | confirmed | `ssao.cpp:414`, `shadow.cpp:680-745`, `post.cpp:540-557` | medium |
| 9 | 7402 of 8591 HD-pack textures (BC7) have no mipmaps: shimmer at distance, extra bandwidth, and Texture filtering can't apply. | confirmed | `texpack.cpp:98, 297` | **done 2026-11-09**: `tools/texpack_mips.py` (full-size levels kept byte for byte; originals in `textures-backup-nomips/`); run `logs/run-20261109-000634-mips-test` clean |
| 10 | HD-pack uploads are limited by count, not size: a room entry can push about 26 MB on the render thread in one frame. | confirmed | `texture_replacement.cpp` | small |
| 11 | Texture filtering 8x/16x does nothing on the game's own textures (uploaded without mip levels). | confirmed | `gxcore_draw.cpp:2799-2801`, `texture.cpp:124-140` | note now; real fix medium |
| 12 | Glow cover recomputes the same 48-tap mask up to 3 times per frame. | partly | `glow.cpp:323-341` | small |
| 13 | HDR present clears and blends a full-window menu layer every frame even with no menu. | partly | `aurora.cpp:1054-1090` | small |

## Duplicates and settings that don't do what they say (low)

- Light shafts, Haze and Glow all add light round lamps, so lamp light is counted two or three times together.
- Glow "Off" still glows with HDR on. Occlusion quality stays live with Occlusion off.
- Contact shadows and Haze build the shadow atlas even with Character shadows on "The game's".
- Per-pixel lighting Off doesn't turn it off for spell meshes when Spell glow is on.
- Spell glow sparks need Glow or HDR on to show.
- HDR calibration's Back doesn't undo, and saves what was set.
- Changing per-pixel lighting, skin or spell glow compiles new pipelines and lit draws vanish until they're built.
- Swinging may widen a character's shadow tile, which then shrinks back over up to 4 s (partly: it depends on
  the weapon sharing the character's lights).

## Modern options that the existing passes make easy (ideas, not started)

Small: graphics presets (Low to Ultra, plus a co-op performance preset); shadow quality and max shadows
(`kTileSize`, `kMaxTiles`); present mode / VRR; a low-latency toggle (env vars exist); film grain, vignette and
colour grading as one end pass; FSR 1 upscaling in place of the bilinear scale (lets Resolution drop for speed
while staying sharp); mipmapped light maps.

Medium: specular highlights from the game's own lights (per-pixel path has position and normal); half-res GTAO
to make High affordable; depth of field for cutscenes and Tome close-ups; screen-space shadows from room lamps;
decoding the game's own mip levels; black-frame insertion for 60 fps co-op on a 120 Hz screen (cuts motion smear).

Large: vertex interpolation for CPU-skinned characters (fixes pose lag and ghosting at 120); motion vectors from
`interp.cpp`'s matched matrices, which would enable TAA, an FSR 2/3-style upscaler or frame generation, and motion
blur; true HDR rendering (16-bit EFB); clustered lighting for all room lamps; SSR / SSGI.

Full reviewer output: the workflow journal for run `wf_f81d7fe6-3c6`.
