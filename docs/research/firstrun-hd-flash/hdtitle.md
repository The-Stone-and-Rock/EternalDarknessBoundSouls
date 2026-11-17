# The HD title picture with the title video OFF

The user's words (2026-11-17): "The Video title screen, when set to off, the HD texture background needs to be present."

Research only. Nothing was built, run or changed. Evidence is a file:line, a log line or a picture path. Anything else
is marked [inferred]. Two investigators looked at this on their own; this page joins what they found and what was
checked again here.

## 1. What happens today

### 1.1 The renderer: a claimed texture never reaches the HD pack

All in `.deps/GXRuntime/graphics/aurora/lib/gfx/gxcore_draw.cpp` (working tree; that tree has other uncommitted
changes from another workflow, not these lines).

- 2709-2720: `claimed` comes from the host's `claims(format, w, h)`, then `claims_hash(...)`. The hash is XXH64 of the
  base level. The HD pack's file names use the same hash.
- 2725-2729: a claimed texture's cache key is mixed with the host's `generation(...)` word.
- 2732-2739: `pack_generation = !claimed && !ci && pack ? pack.generation() : 0`. The comment says "not for
  host-redrawn or paletted textures". So a claimed texture gets 0, and the pack lookup is skipped.
- 2987-2989: a claimed texture is sent to `ask_override(decoded, 850, 480, hash)`. `decoded` is the disc's CMPR picture
  as RGBA8. `ask_override` (2079-2088) is called on the renderer's thread and waits for the answer.
- 2990-2991: answer 1 is uploaded at whatever size the host gives back (`upload_replaced`, 2813-2820, one mip level).
- 2994-3003: answer 2 means "not ready". The disc picture shows and the host is asked again later.
- So the host may hand back a picture bigger than 850x480. The title video already does: 1280x720 frames.

### 1.2 The host claims the title, Options and Load pictures

- `runtime/host/titlevideo.cpp:986-989` `titlevideo_claims_hash`:
  `return hash == kTitleHash ? video_on() || g_menu_row.load() >= 0 : hash == kLoadScreenHash || hash == kOptionsHash;`
- Before commit 9d4d49c (Native menus s2) the title part was `video_on()` only (`git show 9d4d49c`). So with the video
  off, the title was not claimed and the HD pack's title showed.
- `g_menu_row` is set every title draw from `runtime/host/hle.c:17291`
  `titlevideo_menu_row(english ? (int)cpu->gpr[4] : -1)`. On any English main menu it is 0 or more. So now the title
  is always claimed in English, video or not.
- The Options picture (`kOptionsHash`) and the Load screen picture (`kLoadScreenHash`) are always claimed, since
  commit 1c1e718 (2026-10-30). So their HD versions have not shown for a long time either, video on or off.
- Constants: `kTitleHash = 0x2a6eea64eb8fbbbb` (titlevideo.cpp:168), `kLoadScreenHash = 0x14eea9ccc025ff46` (:169),
  `kOptionsHash = 0xbefc5ef6c1629595` (:187).

### 1.3 What the host paints with the video off

- `title_picture` (titlevideo.cpp:1036-1043): `menu_picture` repaints the picture it was given, then
  `if (!video_on()) return menu.empty() ? 0 : give(menu, width, height, ...)`. That is 850x480. The pack is never read.
- `menu_picture` (:793-828) keeps a lit and an unlit copy in `g_menu[2]` (:663). It checks the size is w*h*4.
- `paint_menu` (:672-790) is measured on the disc's picture, in disc pixels:
  - the hole box 554..668 x 325..374 (:679);
  - the ink test `menu_ink` (:667-670): brightness 45..80, blue minus red 12..24;
  - "not the picture measured" if the ink sum is under 200 (:697-698);
  - Options moved 34 down (`kMenuShift`, :662, used at :726);
  - "Choose Character" in the Tome's hand (`tome_hand_cover`), at most 232 wide and 30 tall (:738-753), centred on
    x 618, baseline 356 (:756); colours paint (72,97,104), lit (0xA0,0xE3,0xF1) (:757); shadow radius `kR = 6` (:768).
- `hle.c:17292` `title_quads_put(cpu, english && titlevideo_menu_painted())` moves the Options, Jump and Credits glow
  quads only once the picture was painted. Those quads are drawn in screen space, so a bigger picture does not move
  them [inferred from the quads being separate draws].
- `titlevideo_generation` (:995-1010): one word for every 850x480 picture. Video off: `kOptionsMoreSerial | key`
  (`kOptionsMoreSerial`, :326). `key = list*8 + (row>=0 ? 4 : 0) + (row==5 ? 1 : 0)` (:1008). Bit 1 (+2) is unused.
  `g_settings.hd_textures` is not in it.

### 1.4 The HD pack and its reader

- `runtime/host/texpack.cpp:443-480` `texpack_read_rgba(format, w, h, hash, &rgba, &w, &h)` reads the pack's file and
  gives the base level as RGBA8. DDS BC1/2/3/7 are decoded on the CPU with bcdec (`bcdec_bc7`, :473). It reads the file
  whole and waits, so it belongs off the renderer's thread.
- It returns false when HD textures are off or there is no pack (`generation()==0`, :445 and :360-364), or the pack has
  no such texture (:447-449).
- `docs/research/in-game-menu/10-title-choose-character.md:332-333` says "The host has no CPU BC7 decoder today". That
  is out of date: texpack.cpp:473 decodes BC7.
- The model to copy: `runtime/host/glyph_art.cpp`.
  - `base()` (:588-626) starts from `texpack_read_rgba(kCmpr, ...)`, else the game's picture enlarged.
  - `art_apply` (:676-710) builds on a detached thread and answers 2 until the build is ready, then 1.
  - `signature_now()` (:659-661) XORs `g_settings.hd_textures` in, so flipping HD textures re-asks.
  - It flips the pack image when reading it (:615-619, "Stored upside down, as the game's textures are").

### 1.5 The pack's files in this install

Seen with `ls`:

| Picture | File | Size |
|---|---|---|
| Title (`2a6eea64eb8fbbbb`) | `textures/GED/splash, menu, and text/tex1_850x480_2a6eea64eb8fbbbb_14.dds` | 6,528,148 bytes |
| Load (`14eea9ccc025ff46`) | `textures/GED/splash, menu, and text/tex1_850x480_14eea9ccc025ff46_14.dds` | 6,528,148 bytes |
| Options (`befc5ef6c1629595`) | `textures/Eternal Darkness HD Texture Pack PAL supplement/tex1_850x480_befc5ef6c1629595_14.dds` | 8,706,180 bytes |

- Both investigators read the DDS headers: 3400x1920 (exactly 4x), BC7 (DXGI 98). Title and Load have 1 mip; Options
  has 12.
- Other 850x480 pictures in the pack that nothing claims keep their HD: `05f69fb2f794aa0f`, `479901ec22ccc4ac` (GED),
  `7910715fce98efbf` (supplement). What they show was not checked.
- The pack is not in git. A release user without it sees the disc picture anyway.
- The log line: `[texpack] 7719 textures in E:\Eternal Darkness Recomp Fork\textures (0 skipped)`
  (logs/run-20261117-073249-jtg-s2-cc-off2/page-0001.log:3).

### 1.6 What the user sees

- The only video-off runs with HD on are after 9d4d49c: logs/run-20261117-073028-jtg-s2-cc-off and
  logs/run-20261117-073249-jtg-s2-cc-off2 (settings.ini: `hd_textures = 1` line 6, `mod_title_video = 0` line 71).
- Picture: `Screenshots for Claude/s2-choose-character/02-off-choose-lit-(second-paint).jpg`. This is the repainted
  disc picture at 850x480.
- `docs/coop-next-session.md:252-253` already noted this loss and said to tell the user.
- The video is on by default (`mod_title_video` default 1, `runtime/host/settings.c:80`). The video-off case comes up
  when the user turns it off, or when `res/video/title.mpg` is missing (`video_on()`, titlevideo.cpp:229-234).

### 1.7 The HD pictures, measured (investigator 2)

Investigator 2 decoded the pack files with the repo's `build/texconv/texconv.exe` into the scratchpad only (this
folder is temporary and may be cleared):

- `C:\Users\stevi\AppData\Local\Temp\claude\f9cbcc8b2540bd73\d00f5d6d-ca63-4474-8e8e-f158d97e6e17\scratchpad\hd\small\tex1_850x480_2a6eea64eb8fbbbb_14.jpg`
- `C:\Users\stevi\AppData\Local\Temp\claude\f9cbcc8b2540bd73\d00f5d6d-ca63-4474-8e8e-f158d97e6e17\scratchpad\hd\small\tex1_850x480_befc5ef6c1629595_14.jpg`

What they found:

- Both are stored upside down, like the game's textures. So titlevideo's `h - 1 - y` rule works on them as they are,
  with h = 1920.
- Same painting, same words in the same places, 4x the size.
- The HD title's words are greyer. Median blue minus red of the ink: 20 on HD, 29 on the disc. `menu_ink` finds only
  about 38% as much ink on HD. Options' box scores 119 on HD; `paint_menu` needs 200. **So the disc-tuned colour tests
  must not run on the HD picture.**
- The HD letters sit inside the disc's masks: 3.4% of HD title letter pixels fall outside the disc's `wide` mask, 0%
  once it is grown by 1 disc pixel. For Options, every HD pixel brighter than 6 in the words' box is inside the disc
  ink grown 3 px; the HD ground there is pure black.

## 2. What is wrong, and why

1. With the video off, the user wants the HD title. The host claims the title (for Choose Character), and a claimed
   texture skips the pack (gxcore_draw.cpp:2732-2739). The host then paints on the disc's 850x480 picture
   (titlevideo.cpp:1036-1043). So the HD title is lost.
2. The same rule already hides the HD Options picture and the HD Load picture (titlevideo.cpp:989).
3. The title look's backdrop `res/video/options-clean.png` is made from the kept disc Options picture
   (`titlevideo_title_art`, titlevideo.cpp:1109-1166, checks 850x480 at :1150). On a fresh install it does not exist
   until Options is seen, so the Choose Character cards show on black
   (`Screenshots for Claude/s4-review-fixes/05-fresh-run-cards-first-black-behind.jpg`;
   `ensure_title_art`, `runtime/host/frontend.cpp:2858-2870`, logs "NOT written (no Options picture kept yet)").
4. Toggling HD textures does not re-ask for a claimed picture: `titlevideo_generation` has no `hd_textures` in it.

Rejected: just "don't claim the title when the video is off and the pack has it". It brings the HD picture back but
loses Choose Character (hle.c only adds the row once `titlevideo_menu_painted()`). That is a loss, not a fix.

The rule for the fix: **measure on the disc's 850x480, paint on the pack's HD.**

## 3. The fix, in build steps

All in `runtime/host/titlevideo.cpp` unless named. The game's own functions and the host's are reused; nothing new is
decoded by hand.

### Step 1. An HD base reader

- Add `hd_base(uint64_t hash, std::vector<uint8_t>* rgba, uint32_t* w, uint32_t* h, int* k)`:
  `texpack_read_rgba(kCmpr, kTitleW, kTitleH, hash, ...)`, then `k = w / 850`, and accept only if `w == 850*k` and
  `h == 480*k` and k >= 2. Else false (keep 1x).
- Keep the pack's row order as it is (stored upside down), so the `h - 1 - y` rule matches. Do not flip, unlike
  glyph_art's canvas (glyph_art.cpp:615-619 flips because its canvas is upright).
- Cache the decoded base per hash. Drop it when `g_settings.hd_textures` changes.
- Touches: nothing else. Without a pack or with HD off, `texpack_read_rgba` returns false and all of today's paths stay.

### Step 2. The title's HD paint on a worker

- Add `paint_menu_hd(disc, hd, k, lit)`:
  1. Masks `mask`, `wide`, `lift` made exactly as now, from the disc picture at 1x (titlevideo.cpp:677-698). Grow
     `wide` and `lift` by 1 disc pixel. Sample them bilinearly at ((X+0.5)/k - 0.5, (Y+0.5)/k - 0.5).
  2. Hole fill at HD from the HD colours: `keep = 1 - wide(up)`, blur radius 7k, box 554k..668k x 325k..374k. Filling
     from disc colours would leave a seam [inferred from the colour shift in 1.7].
  3. Options moved down `kMenuShift * k`, mixed by the upsampled `lift`.
  4. "Choose Character": pick the size at 1x with today's loop (:738-753), then draw again with `tome_hand_cover` at
     px*k. Left `(618 - mid)*k`, baseline `356*k`. Shadow blur 4k, `kR = 6k`. The "not over other ink" test uses the
     upsampled 1x mask, not `menu_ink` on HD colours (1.7).
  5. Everything else (copyright line, Jump, Credits) stays the pack's own pixels or the game's quads.
- Better than a second copy of `paint_menu`: give `paint_menu` a scale and a base (disc for measuring, base for painting)
  so the 1x path is the same code with k = 1 [inferred: less to keep in step].
- Build both lit and unlit on a detached thread, like glyph_art's `Build` (glyph_art.cpp:649-657, 676-710). Start it on
  the first `kTitleHash` claim with the video off, or at boot when `mod_title_video == 0` and the pack has the hash.
- While it builds, answer 1 with today's 1x repaint, **not** 2. Answer 2 shows the raw disc picture with no Choose
  Character row (gxcore_draw.cpp:2994-3003), and hle.c would drop the row.
- In `title_picture`, with the video off: if the HD pair is ready and HD is on, `give` the HD copy (3400x1920).
- Touches: `g_menu` memory. Each HD copy is 3400*1920*4 = 26.1 MB; the pair is 52 MB, plus the decoded base.
- Risk: the hand-font cover at 4x size: check `tome_hand_cover` gives a clean result at px up to 256 [inferred].

### Step 3. The generation word

- In `titlevideo_generation`, video off: add `+2` (bit 1 of `key`, free today, :1008) when the HD title pair is ready
  AND `g_settings.hd_textures` AND the pack has the hash.
- The renderer then drops the 1x one and asks again (the generation keys the cache, gxcore_draw.cpp:2725-2729).
- This also makes the HD toggle on the title's Graphics page work: off drops the bit, the renderer asks again, gets 1x.
- Risk: the word is shared by all 850x480 pictures (:996-999). The bit must be the same for the title and Options
  paints, or it must be set only from title state. Each change re-uploads 26 MB once [inferred: a small hitch on lit
  swaps; the paint itself is cached].

### Step 4. The Options picture (kOptionsHash), same pattern

- A clean HD base, made once from the pack alone: the words' box blacked out at k scale
  (x 140k..441k, y 60k..471k upright, less the logo x >= 400k, y < 200k). Measured: only words are brighter than 6 there.
- Each (row, scroll, values) paint (`paint_options`, titlevideo.cpp:477-616): copy the game's rows from the HD source
  with the upsampled `g_row_of`, shifted `(kSlotY - kRowY)*k`; our rows with `tome_hand_cover` at 52k px; the logo
  shade and the glow made at 1x and upsampled; chevrons x k.
- The glow loop is about (2kR+1)^2 per pixel: at k = 4 roughly 150M operations [inferred: too slow per key press].
  Paint on a worker with the 1x as stand-in, or at 2x (1700x960).
- Cut `g_options_paints` from 8 (:633-634) to about 3: 8 x 26 MB = 209 MB.
- Touches: `options_list` (:619-651) runs on the renderer thread today. Measure before keeping it there.

### Step 5. The title look's backdrop from the pack (fixes the fresh-install black page when the pack is there)

- When the pack has `befc5ef6c1629595`, write `res/video/options-clean-hd.png` from the pack ALONE (the blacked box of
  step 4, flipped upright). No disc picture and no Options visit needed.
- Make it at boot on a worker, after `texpack_init`. Write it only if missing or older than the pack file
  (a 3400x1920 PNG write would hitch about a second [inferred]). Today `ensure_title_art` rewrites the 850 file every
  run on the main thread (frontend.cpp:2858-2870).
- In `frontend.cpp`, add a ready flag like `title_art_ready` (:2880-2888) and a body class (for example
  `title-art-hd`). In `res/ui/menu.rcss` (near :998-1002) add
  `body.skin-title.title-art-hd #backdrop-picture { decorator: image(../video/options-clean-hd.png fill); }`.
  A separate path is safer: RmlUi never tries a picture again after it failed once (menu.rcss:998-999).
- `fill` stretches any size into `title_picture_rect` (frontend.cpp:2767), so no layout change.
- Without the pack: the 850x480 file as now. A fresh install still needs `options.rgba` from the first-run step
  (docs/release-first-run.md, table row 2) or an Options visit. This step does not replace that step.

### Step 6 (optional). The Load screen (kLoadScreenHash) with the video off

- Same pattern: pack base, the top-left fade and the panel rects x k (`load_screen`, titlevideo.cpp:838-941). The pack
  also has the Load word `tex1_160x62_a46c75052d75745a_14.dds` (investigator 2).

### Step 7. Match the new words' colour

- "Choose Character"'s ink (72, 97, 104) was matched to the disc's teal (titlevideo.cpp:757). The HD words are greyer
  (1.7). Pick the colour from the HD words' median when painting on HD [inferred: may need a second look by eye].

## 4. Risks and side effects

- Memory: 52 MB for the title pair, up to 3 x 26 MB for Options paints, plus decoded bases. Both lit and unlit may sit in
  the renderer's cache at once (different generation words) [inferred].
- One mip level on the replaced upload (gxcore_draw.cpp:2816). The pack's own title DDS also has 1 mip, so the title is
  no worse than the pack. The Options DDS has 12 mips, so a 3400 Options shrunk to a small window may shimmer a little
  more than the pack's own [inferred].
- Time: BC7 decode of 6.5 MB plus a 16x larger paint is too slow for the renderer thread [inferred]. Every HD step runs
  on a worker.
- The video ON case: unchanged. Optionally its word lift (titlevideo.cpp:1055-1087) could take the HD repaint for
  sharper words [inferred].
- Other languages: `kTitleHash` is English only. Other languages are not claimed, so the pack shows on its own
  (titlevideo.cpp:989).
- Open question for the user: should the title look's backdrop (step 5) stay HD when HD textures are turned off?

## 5. How to check each step (for whoever is allowed to run)

Only fresh title or Jump-to-Game runs, with `ED_RUN_AWAY=1`. Never `ed test` for this.

Switches:

- `settings.ini` keys `hd_textures` (default 1, settings.c:19) and `mod_title_video` (default 1, settings.c:80).
  `JTG_SET="key=value;key=value"` sets them for one run (tools/ed/jtg.sh:157-160).
- `ED_TEXTURE_PACK=<folder>` picks the pack; an empty folder is the no-pack case (texpack.cpp:365-366).
- `ED_TEXPACK_LOG=1` logs each pack file loaded (texpack.cpp:327-329).
- `ED_TITLE_PAINT_DUMP=PATH` writes the repaints as PATH-lit.png, PATH-unlit.png (titlevideo.cpp:812-824) and
  PATH-options-ROW-SCROLL.png (:637-649).
- Put every picture folder inside `E:\Eternal Darkness Recomp Fork\Screenshots for Claude`.

The proven recipe (logs/run-20261117-073249-jtg-s2-cc-off2/env.txt), wider shots for sharpness:

```
JTG_SET="mod_title_video=0" JTG_SLOT=1 JTG_TITLE_AT=90000 \
ED_INPUT_SCRIPT="700,706,0x0004;760,766,0x0004;840,846,0x0004" \
ED_RUN_FRAMES=920 ED_RUN_SECONDS=120 ED_RUN_SHOTS_W=1920 ED_RUN_AWAY=1 \
ED_TITLE_PAINT_DUMP="Screenshots for Claude/hdtitle/paint-off" sh tools/ed/jtg.sh 1 hdtitle-off
```

`JTG_TITLE_AT=90000` keeps jtg's own presses away, so the run stays on the title. The script's Downs move to row 5 and
back, so lit and unlit both show.

| Step | Run | Expect |
|---|---|---|
| 1-3 | the recipe above | `paint-off-lit.png` and `-unlit.png` are 3400x1920. In shots/, the face, hair, logo and moon as sharp as the HD reference jpg; Choose Character in the same place as `02-off-choose-lit-(second-paint).jpg`; Options 34 (x k) lower; no seam round the filled hole; first frames may show the 1x paint, then HD. |
| 3 | same, with `JTG_SET="mod_title_video=0;hd_textures=0"` | today's 1x picture, unchanged |
| 3 | same, with `ED_TEXTURE_PACK=` an empty folder | today's 1x picture, unchanged |
| 3 | `JTG_SET="mod_title_video=1"` | the video, unchanged (compare `08-video-choose-lit.jpg`) |
| 3 | lit swap | no long frame in both.tsv when Down reaches row 5 and leaves it |
| 4 | the recipe, pressing into Options and scrolling | `paint-off-options-*.png` at HD; skull sharp; scroll keys show no long frame |
| 5 | fresh backdrop: move `res/video/options.rgba` and `options-clean*.png` aside (as run -090616-jtg-s4fix-fresh-map, docs/coop-next-session.md:128-131), open Choose Character first | with the pack: the HD skull behind the cards at once; without the pack: black as today (`05-fresh-run-cards-first-black-behind.jpg`); the log names the HD file written |
| 6 | Load Game from the title, video off | the Load backdrop sharp, its fade and panels in place |
| 7 | by eye on the step 1-3 shots | Choose Character's teal matches Start Game and Load Game |

Compare against: before, `Screenshots for Claude/s2-choose-character/02-off-choose-lit-(second-paint).jpg` (the disc
picture); after, the HD reference jpgs in section 1.7.

## Skeptic's check

Read only. Checked against the code, `git show`/`git log -S`, the pack's DDS headers (a small header reader in the
scratchpad), the two HD reference jpgs, and the cc-off2 run's files. Nothing was built or run.

### Confirmed

- 1.1 holds. gxcore_draw.cpp:2709-2720 (claims, then claims_hash on XXH64 of the base level), 2732-2739 (a claimed
  texture gets pack generation 0), 2987-2991 (asked with the disc picture; answer 1 uploaded at the host's size, one
  mip, 2813-2820), 2994-3003 (answer 2: disc picture shown, kept pending, asked again on later frames, 2825-2843).
- The skip rule is old. It entered the patch in 2cd509c (2026-10-15, `git log -S"not for host-redrawn" --
  patches/gxruntime/eternal-darkness.patch`). 1c1e718 (2026-10-30) added the Options and Load claims (`git show
  1c1e718`: Options answered 0, "kept as it is"). So HD Options and HD Load have been lost since 2026-10-30, video on
  or off.
- 1.2 holds. titlevideo.cpp:986-989 is exact. `git show 9d4d49c` changes `video_on()` to
  `video_on() || g_menu_row.load() >= 0`. Hashes at :168, :169, :187. hle.c:17291-17292 as said.
- 1.3 holds. title_picture :1036-1043, menu_picture :793-828, paint_menu :672-790. The hole box, ink test, 200,
  kMenuShift, 232/30, 618/356, colours and kR all match. In the generation word, bit 1 (+2) is free: key is
  list*8 + 4 + 1 at most (:1008). hd_textures is not in it.
- 1.4 holds. texpack_read_rgba (:443-480) returns false when HD is off or there is no pack (:445, :360-364) or no file
  (:447-449). It decodes BC7 (:473), base level only. 10-title-choose-character.md:332-333 is out of date (it was
  marked [inferred] there).
- 1.5 holds. `find textures -name "tex1_850x480_*"` gives the six files and sizes. Headers: all 3400x1920, DX10,
  DXGI 98 (BC7). Mips: 1 for the four GED files, 12 for the two supplement files.
- 1.6 holds. In all of logs/, only two runs have `mod_title_video = 0` with `hd_textures = 1`: cc-off and cc-off2.
  settings.ini lines 6 and 71, page-0001.log:3, settings.c:19 and :80, coop-next-session.md:252-253 all match.
- 1.7 orientation holds, by code, not only by the jpg. The renderer uploads a pack image (`upload_pack`) and a decoded
  disc texture the same way, so the pack's rows are in the decoded texture's order, or HD textures would show
  flipped in game. glyph_art.cpp:615-619 flips the pack image and the game's enlarged picture by the same rule. (The
  jpgs are upright, so the investigator flipped them to look at [inferred].)
- 1.7 colour holds by eye: the HD jpg's Start Game, Load Game and Options are grey-white; in
  `02-off-choose-lit-(second-paint).jpg` they are teal. The numbers (20 vs 29, 38%, 119, 3.4%/0%) were not re-measured.
- The Options box of steps 4-5 holds by eye on the HD Options jpg: words at about disc x 172..425, the logo from
  about x 412 and y 45..165, the skull's glow from about x 460. Box x 140..441 minus the logo corner holds words only.
- Workers are safe: glyph_art already calls texpack_read_rgba on a detached thread (base() inside compose(),
  glyph_art.cpp:695-704). The hand font loads under std::call_once (tome.cpp:220-223).
- The other refs: frontend.cpp:2767, 2858-2888; menu.rcss:999-1002; jtg.sh:157-160 (jtg.sh:95 and :174 also set
  ED_NO_BOOT_STATE and the shot interval, as in cc-off2's env.txt); texpack.cpp:366-367.

### Wrong, with corrections

1. Step 2, "answer 2 ... hle.c would drop the row": no. `g_menu_painted` is set at titlevideo.cpp:811 and never
   cleared. Once painted, hle.c keeps the row and the moved glow quads. Answer 2 would show the disc picture: no
   Choose Character words, Options in its old place, its glow 34 lower. Answering 1 with the 1x paint is still right.
2. Step 2, "or at boot when mod_title_video == 0": the masks need the disc picture. The host only gets it in the
   renderer's first ask (titlevideo_apply). At boot only step 1's decode can start. The paint starts at the first ask.
3. Steps 2.4 and 4, "tome_hand_cover at px*k": `bold` is in absolute pixels (tome.cpp:579, :616 `reach = bold + 1`).
   At 4x size with bold 0.8 the strokes are a quarter as thick. Pass `0.8f * k`. Also take ix0, ix1 and base from the
   HD cover itself; 4x the 1x numbers is off by a few pixels (stbtt rounding).
4. Section 4, "a 16x larger paint is too slow": the paint is local. It touches the hole box, Options moved to y 408,
   and Choose Character with its shadow: about 0.3M HD pixels at k = 4. The box blur is 2(2r+1) adds per pixel
   (titlevideo.cpp:142-158), about 114 per pass at r = 28. Tens of ms [inferred]. The slow parts cover the whole
   picture: the BC7 decode of 6.5M pixels and the 26 MB copies. A worker is still right, for those.
5. Section 4, "both lit and unlit may sit in the renderer's cache at once": no. A new key for the same guest address
   evicts the old entry (gxcore_draw.cpp:2883-2898). One at a time.
6. Step 3, "each change re-uploads 26 MB once": every swap, not once (follows from 5). Each time the cursor enters or
   leaves row 5, the word changes (:1008), the host is asked on the renderer's thread, `*out = p.rgba` copies 26 MB
   (:826), give() copies again (:295-304), and 26 MB goes to the GPU [inferred: one long frame, 10-30 ms]. Today it
   is 1.6 MB. Give straight from the kept copy (one copy).
7. Section 5, "ED_TEXPACK_LOG=1 (texpack.cpp:327-329)": the switch is read at texpack.cpp:380; 327-329 is the print.
   It prints only from the pack's own loader, not texpack_read_rgba. Step 1 needs its own log line.
8. Step 3, "AND the pack has the hash": texpack.h has no such call (only texpack_count, texpack_busy,
   texpack_read_rgba). Use "the HD pair is built" plus `g_settings.hd_textures`.
9. Step 1, "drop the base when hd_textures changes": not needed. Turning HD back on would decode again. Keep it and
   gate on the setting; step 3's bit does the swap.

### Missed

1. The lit "Load Game" word (kLoadWordHash, 160x62) is always claimed (titlevideo.cpp:991) and answered 0 (:1023).
   So its HD version never shows. The pack has it twice (`GED/splash, menu, and text/` and the PAL supplement,
   158,868 bytes). With an HD title, a lit Load Game would be the disc's. Fix: after `g_word.assign` (:1021), answer
   1 with texpack_read_rgba (a small read). The claim must stay, to keep g_word for the Load screen.
2. The pop. Every boot with the video off: 1x first, then HD after decode and paint, a soft-to-sharp jump. Section 5
   accepts it. To hide it: decode the HD base at boot on a worker, after texpack_init (main.c:680). Then only the
   small paint is left at the first ask. It fits the user's first-run "compile" wish too.
3. Maybe an earlier HD-to-1x drop. g_menu_row is -1 until the first state-7 draw (hle.c:17287-17291). If an earlier
   title state draws this picture, it is unclaimed then, so the pack's HD shows, then 1x when state 7 starts. Not
   checked which states draw it.
4. When paint_menu fails (no hand font, or not as measured), title_picture answers 0 for a claimed texture (:1043):
   the disc picture, not the pack. The HD path should then give the HD base unpainted. The row is not added then
   (g_menu_painted stays false), so that is right.
5. Step 4 flicker. A worker with a 1x stand-in means each Options key press shows 1x, then HD: soft/sharp on every
   press. The Options paint is local too (x 140..441, and the logo 360..640 x 30..240). Copy the HD base, paint only
   that region, cache the six row covers (they never change), and measure; it may be fast enough to do at once
   [inferred].
6. Step 5 on the UI side: RmlUi decodes a 3400x1920 PNG when the page first asks [inferred: a hitch on the first
   title-look page]. 2x (1700x960) may be enough below 4K windows.
7. Scope. The user asked for the title with the video off: steps 1-3 and 7. Steps 4 and 6 are older losses that also
   happen with the video on. Step 5 is the fresh-install backdrop. Do them after, separately.

### What it changes in the plan

- Step 1: start the BC7 decode at boot on a worker. Keep the base; gate on hd_textures. Add a log line.
- Step 2: start the paint at the first ask. Bold `0.8f * k`. Centre from the HD cover. Answer 1 with 1x meanwhile.
  Keep one HD base plus two small lit/unlit patches, or give from the kept copy, with one copy per ask. If paint_menu
  fails, give the HD base unpainted.
- Step 3: the +2 bit when the HD pair is built and hd_textures is on. No pack query.
- Step 4: paint only the changed region at HD; no 1x stand-in per press, if measured fast enough.
- Step 5: consider 2x. Otherwise as written.
- New, small: the lit Load Game word from the pack.

### Verdict

The diagnosis holds. Every address, line and history claim checked is right, bar small line offsets. "Measure on the
disc's 850x480, paint on the pack's HD" is sound, and the pack files are as described. The plan works with the
corrections. The ones that matter most: the bold width at 4x, when the worker can start (not before the disc picture
is in hand), and the 26 MB moves on each lit swap. The HD ink numbers in 1.7 were not re-measured here.
