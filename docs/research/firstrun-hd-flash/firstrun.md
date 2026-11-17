# The first-run preparing step: make the disc's art, music and shaders before they are needed

The user's words (2026-11-17): "The character selection screen and music etc, on a fresh install, we need to make an
compiler of shaders, assets etc that will create all of that at the very beginning during a fresh boot that way it
doesn't look silly."

Research only. Nothing was built, run or changed. Evidence is a file:line, a guest address in
build/ed-db/disasm.txt, a log line or a picture path. Anything else is marked [inferred]. Two investigators looked at
this on their own; this page joins what they found with what was checked again here. The plan that already exists is
docs/release-first-run.md (commit 88ce3c5). This page checks it against the code and adds what it misses. The HD title
picture with the video off has its own page: docs/research/firstrun-hd-flash/hdtitle.md.

## 1. What a fresh install has, and what it lacks

A fresh install is the release zip: `git archive res` (tools/win/make-win-package.sh:33). Every file below is
git-ignored, so none of them ships (.gitignore:4-12). Each one is made the first time something asks for it.

| Piece | Made by | Made when | Without it |
|---|---|---|---|
| `res/video/options.rgba` (850x480 RGBA, 1,632,000 B) | `keep_skull` (runtime/host/titlevideo.cpp:215-227), from `titlevideo_apply` (:1027-1029) | only when the renderer first decodes texture 2, i.e. the title's Options screen is drawn | Load screen has no skull (:204-213); `options-clean.png` can't be made |
| `res/video/options-clean.png`, `mark-up.png`, `mark-down.png` | `titlevideo_title_art` (titlevideo.cpp:1109-1166), from `ensure_title_art` (frontend.cpp:2858-2869) | at each title-look page open / render (frontend.cpp:3124, :4966); the clean picture only once options.rgba exists | Choose Character and every title-look page sit on black |
| `res/ui/portraits/NAME.png` (12 x 126x89) | `ensure_portraits` (frontend.cpp:1050-1085) from disc `UMnMenu.tpl` | only from the roster page (frontend.cpp:1141); needs `disc_playable()` (frontend.cpp:397-399) | an empty frame on a card |
| `res/music/launcher.wav` (8,193,232 B) | by hand: `ED_PCM_OUT` (runtime/host/audio.c:226-238) then `tools/launcher_music.py` | never by the runtime | the launcher is silent (frontend.cpp:3491-3496) |
| `generated/main.dol` | `extract_dol` (runtime/host/main.c, at boot) | at boot when missing | already fine |
| `%APPDATA%\Eternal Darkness\pipeline_cache.db` (pipeline keys) | aurora, every pipeline seen | first draw of each pipeline | each new pipeline's draw is skipped until it compiles |
| `%APPDATA%\Eternal Darkness\dawn_cache.db` (compiled shaders) | Dawn | first compile | slower compiles; per GPU and driver, can't ship |

What the user saw, with evidence:

- Log, logs/run-20261117-090616-jtg-s4fix-fresh-map/page-0001.log:86:
  "[frontend] the title look's picture NOT written (no Options picture kept yet): black behind".
- Same log, :98 "[titlevideo] the Options picture kept in ...options.rgba" only after Options was drawn, then :112
  "[frontend] the title look's picture written (res/video/options-clean.png)".
- Pictures: Screenshots for Claude/s4-review-fixes/05-fresh-run-cards-first-black-behind.jpg (cards on black) and
  06-fresh-run-then-controls-picture-shows.jpg (after Options was seen).
- docs/coop-next-session.md:127-131 records the same run (the two files moved aside, then back, same md5).

"Music etc" most likely means the silent launcher on a fresh install [inferred: the launcher is the only music the
host makes; the game's own music needs nothing].

## 2. What is wrong, and why

### 2.1 The Options picture waits for the player to open Options

- `keep_skull` is called only from `titlevideo_apply`, and only for `kOptionsHash` (titlevideo.cpp:1027-1029).
  The renderer asks the host only when it decodes that texture, i.e. when the title's Options screen is drawn.
- `title_art_ready` (frontend.cpp:2882-2889) and `ensure_title_art` (:2858-2869) can't make `options-clean.png`
  before that. `titlevideo_title_art` returns false when the kept picture is missing (titlevideo.cpp:1150-1151).
- RmlUi never loads a picture again once a load failed (frontend.cpp:2880-2881). That is why the page now asks for it
  only once the file exists. So the file must be there before the first title page opens.

### 2.2 The cards don't make the portraits

- `player_card_rml` (frontend.cpp:1104-1113) calls `portrait_rml`, which returns "" when the file is missing
  (frontend.cpp:1089-1098). It never calls `ensure_portraits`.
- `ensure_portraits` needs `disc_playable()`, which needs the launcher's disc scan (`g_discs`, frontend.cpp:397-399).
  So in a no-launcher run (jtg.sh, run.sh with a save) it can never write them [inferred from the code].
- With the default settings the card shows the emblem (`t.character == 0`, frontend.cpp:1090-1091; default
  `mod_p1_character = 0`, settings.c:73). So a fresh install with fresh settings is fine; a kept settings.ini with a
  chosen character is not [inferred].

### 2.3 The launcher music can only come from a game run

- It is MusyX output, built from note data, so there is no disc file to copy (docs/coop-todo.md:115;
  tools/launcher_music.py:4-8).
- One loop is 2,048,297 samples at 32 kHz (tools/launcher_music.py:20).
- `ED_PCM_OUT` records before `ED_MUTE` silences the output (audio.c:226-245). So a muted run can record it.

### 2.4 The shipped pipeline list is stale: a fresh install compiles every game pipeline cold

This is the biggest find, and docs/release-first-run.md does not know it.

- res/initial_pipeline_cache.db rows (read with sqlite, read-only): type 1 (GX) v13 x8, type 2 (Rml) v2 x2,
  type 3 (GXCore) v10 x295.
- The renderer wants GXCore v13: `GXCorePipelineConfigVersion = 13`
  (.deps/GXRuntime/graphics/aurora/lib/gfx/gxcore_draw.hpp:61). v11 glow, v12 per-pixel lighting, v13 skin and spell
  edge (:58-60).
- At each start the seed is merged (pipeline_cache.cpp:658-766), then `prune_old_pipeline_cache_versions` deletes
  GXCore rows with `config_version < 13` (pipeline_cache.cpp:908-916). The loader also reads only rows at the current
  version (pipeline_cache.cpp:839-842).
- So all 295 shipped game pipelines are merged and deleted again every start [inferred from the code; the "Seeded"
  line is below the log level, so no log shows it].
- The seed was last changed in d8c2f2b (2026-10-16). Before that 82e61cd and 1874dc1 (git log). Three version bumps
  came after it.
- On a fresh install each new pipeline waits up to 50 ms a frame and is then skipped until it compiles
  (pipeline_cache.cpp:1209-1255; "async skip draw", :382). Commit 82e61cd names one such case: "the sepia flashback
  overlay on a fresh PC".
- The user's own list has 6,624 GXCore v13 rows (read with sqlite, read-only). With a warm Dawn cache:
  "[pipelines] 6638 cached pipelines built at start in 5648 ms" (page-0001.log:66).
- Cold Dawn cache, from logs: "2954 cached pipelines built at start in 19248 ms"
  (logs/run-20261102-121247-coop-slot7-tome120/page-0001.log), "5506 ... 22128 ms"
  (logs/run-20261108-130307-solo-anthony-sword). The "5611 in about 27 s" in docs/coop-todo.md:116 and
  docs/release-first-run.md:38 came from the user's own list, not an empty one.

### 2.5 The HD pack's title pictures are skipped

With the video off, the title shows the disc's 850x480 picture, not the pack's 3400x1920 one. See hdtitle.md. A
preparing step can make the HD version once per pack (step 7 below).

## 3. Things checked again here

- The title pack loader stores the unpacked bank at TITLE_RECORD+16: 0x80019850 `lis r3,-32719`, 0x80019854
  `addi r31,r3,-28380` (= 0x80309124), 0x80019858 `lwz r3,16(r31)`, 0x8001985C `bl 0x80156924` (disasm.txt:21789-21792).
  `TITLE_RECORD` is 0x80309124 (runtime/host/hle.c:17175). 0x80156924 is the game's unpack of the bank
  (docs/research/in-game-menu/06-texture-bank.md:47).
- Texture 2 of `UMnMenu.cmp` is the 850x480 Options picture (docs/research/in-game-menu/02-title-menu.md:200-209).
  That it is index 2 of the bank at *(0x80309134) is [inferred] from that numbering. The hash check in step 2 makes a
  wrong index harmless.
- The same "copy a picture out of a game bank" code exists: `tome_plaques_page` (hle.c:17438-17463). It reads the
  bank's count (+4) and descriptors (+8), the header's h, w, format and data, checks 14 / 432x480, copies the bytes.
- `title_main_draw` (hle.c:17287-17292) runs on every title main-menu draw. It is the place to hand texture 2 over.
- The two CMPR decoders differ:
  - the renderer: `ExpandTo8` (bit copy) and `S3TCBlend` = (3a+5b)/8, and in 3-colour mode the 4th colour is the
    half blend with alpha 0; rows top first (.deps/GXRuntime/graphics/aurora/lib/gfx/texture_convert.cpp:113-128,
    :418-480);
  - the host's `decode_cmpr`: v*255/31, (2a+b)/3, the 4th colour all zero, and rows flipped (frontend.cpp:1017-1045).
  - `titlevideo_title_art` reads options.rgba bottom row first (titlevideo.cpp:1152-1155). So the rows the host gets
    from the renderer are the other way up from texture_convert's order [inferred: there is a flip between them].
  - `find_rows` (titlevideo.cpp:1151) and the erase use brightness and colour limits. A picture a few levels off may
    erase different pixels. So step 2 must make the same bytes as today's file.
- `ED_RES_DIR` moves the whole res folder, options.rgba included (frontend.cpp:3389-3397; `skull_path` uses
  `frontend_res_dir()`, titlevideo.cpp:197-199). It needs `ui/menu.rml` inside. `AURORA_CACHE_DIR` moves both caches
  (aurora.cpp:434-444). `ED_TEXTURE_PACK` names the pack, else `textures/` beside res (texpack.cpp:366-376). So a
  fresh install can be tested without touching the user's files.
- The seed is found beside the exe, one level up, or `res/` in the working folder (pipeline_cache.cpp:603-626). Not
  by `ED_RES_DIR`.
- jtg.sh's title presses changed today: Up x2 to Jump to Game, and "Down x3 would land on Options now" with the
  Choose Character row (tools/ed/jtg.sh, header comment). docs/release-first-run.md still says Down, Down, A.

## 4. The fix, in build steps

Order: the cheap, sure steps first. Steps 1-3 fix what the user saw, with no new screen.

### Step 1. Refresh the shipped pipeline list

- New tool `tools/pipeline_seed.py`: open the player's `pipeline_cache.db` read-only, copy the rows at the current
  versions (GX 13, Rml 2, GXCore 13, Clear current) into a new `res/initial_pipeline_cache.db`. Merge with
  INSERT OR IGNORE, as 82e61cd's message says.
- Read the current versions from the source (`GXCorePipelineConfigVersion` in gxcore_draw.hpp:61, the others beside
  the prune code, pipeline_cache.cpp:879-908), not by hand.
- A release check: the seed's GXCore rows must equal `GXCorePipelineConfigVersion`. Fail the package if not
  (tools/win/make-win-package.sh).
- Optional: in the patch, make the "Seeded N rows" line an `fprintf(stderr, ...)`, so logs show it.
- Risks:
  - The user's list was filled by test runs with debug switches. Some keys may never occur in normal play. They only
    cost start-up compile time.
  - Rows the GXCore checker rejects are deleted at load (pipeline_cache.cpp:1038-1061, :1112-1114). No harm.
  - Size: 7.4 MB for 6,624 rows today [read]. Fine for a zip.
  - Every future version bump empties the seed again. The release check catches that.
- Touches: start-up compile time on every PC (about 6,600 in the background; 5.6 s warm, 20-45 s cold, frames slower
  meanwhile, page-0001.log:66-67 fps 38.5) [cold figure inferred from the logs above].

### Step 2. Take the Options picture from the game's memory on the title's first frame

- In `title_main_draw` (hle.c:17287), once per run, and only while `res/video/options.rgba` is missing:
  - read the bank at `mem_read32(TITLE_RECORD + 16)`;
  - take descriptor 2 the same way `tome_plaques_page` does (hle.c:17444-17452);
  - check format 14, 850x480, and `XXH64(bytes, size, 0) == kOptionsHash` (titlevideo.cpp:187; XXH64 is already used
    by plaques.cpp:60). Any other language or a wrong index then does nothing;
  - hand the raw CMPR bytes to a new `titlevideo_options_from_bank(bytes, w, h)` in titlevideo.cpp.
- `titlevideo_options_from_bank` decodes with the renderer's maths (copy `ExpandTo8`, `S3TCBlend`, the 3-colour rule
  from texture_convert.cpp:418-480), puts the rows in the order `keep_skull` gets today, and calls `keep_skull`. Do the
  decode and the PNG off the guest thread if it is slow [inferred: 850x480 decode is well under a frame; the PNG
  write is the slow part].
- Then call `titlevideo_title_art(res + "/video")` right away, so `options-clean.png` exists long before the
  Choose Character row can be pressed. The title's first draw is about frame 530 (jtg.sh header), Choose Character
  needs at least one press after that.
- Reuse: `keep_skull`, `titlevideo_title_art`, `find_rows`, `tome_plaques_page`'s bank walk. Nothing new is invented.
- Not chosen: the disc-only route. `UMnMenu.cmp` is SK_ASC packed; its decompressor 0x801412F8 is about 3300
  instructions (tools/ed/ppcemu.py:1-9; 85 s in Python, 02-title-menu.md:374). It only matters for the launcher,
  and the launcher does not use this picture.
- Risks:
  - Byte match: if the decode is off by one level, `find_rows` may fail or erase a different set. The check in the
    verify list below compares md5 with today's kept file.
  - Row order: get it wrong and the picture is upside down. Same check.
  - The bank must be unpacked when the title draws. 0x8001985C runs in the title load, before any title draw
    [read]. Guard with `guest_ram_range_valid` as `tome_plaques_page` does.
- Touches: the Load screen (gets its skull from the first run on), More Options' paint (`options_picture`, it reads
  the same kept picture), the Tome's More Options. None change if the bytes match.

### Step 3. Make the portraits wherever a picture of one is shown

- Call `ensure_portraits()` at the top of the Choose Character page build (where `player_card_rml` is used), as
  `build_characters` does (frontend.cpp:1141).
- For no-launcher runs: `ensure_portraits` needs `g_discs` filled. Let it fall back to the `--iso` path main.c already
  has (`iso_path`), e.g. a `frontend_set_disc(path)` from main.c before the guest starts [inferred: a small new call].
- Risk: the static `tried` flag (frontend.cpp:1064) means one try per run. Fine.
- Touches: nothing else; the files are the same ones the roster writes.

### Step 4. The launcher's "Preparing the game from your disc..." page

- Where: `frontend_run_launcher` (frontend.cpp:3720), right after the disc scan, once `disc_playable()` is true.
  docs/release-first-run.md:17-18 puts it there too. Skip it for `--no-launcher`, `ED_QUICKLOAD` and `ED_NO_LAUNCHER`
  (main.c:681-682), so test runs don't change.
- When: if a stamp is missing or old (step 6), or a piece is missing.
- Look: a new Screen in the Tome or launcher skin with a bar. The page can redraw itself each frame with no guest
  (`dol_aurora_ui_frame`, used in run_menu's loop, frontend.cpp:3562, :3669). The stat meter style is already there
  ("smeter", frontend.cpp ~895-902). docs/coop-todo.md:116 has the user's own wish: "Building... Please wait" with a
  bar.
- Work, each with its share of the bar:
  1. portraits: `ensure_portraits()` (12 steps);
  2. pipelines: `aurora_get_stats()` gives `createdPipelines` and `queuedPipelines`
     (.deps/GXRuntime/graphics/aurora/include/aurora/gfx.h:19-31). Bar = created / (created + queued). No patch
     needed. Wait until the queue is empty, only on a first run;
  3. the HD title pictures (step 7), if the pack is on;
  4. the music (step 5).
- Risk: a warm start still takes about 5.6 s (page-0001.log:66). Show the page only on the first run or after a
  version change, never on every start.

### Step 5. The launcher music, made by a hidden game run

- From the Preparing page, start the same exe as a child with a new `--prepare-music OUT.wav` switch [inferred: new].
  The child:
  - skips the launcher, mutes (`ED_MUTE`), and lives off the desktop (as `ED_RUN_AWAY` does with tools/ed/away.ps1,
    tools/ed/run.sh:17-18), or uses a hidden SDL window [inferred];
  - taps the intro through (`ED_SKIP_INTRO`, as jtg.sh) and presses to the title Options with the scripted pad. With
    the Choose Character row that is Down x3, A (jtg.sh header). Check it with a picture before trusting it;
  - records the PCM in memory (the `ED_PCM_OUT` path, audio.c:226-238), and once the loop is in, cuts one loop of
    2,048,297 samples from the fixed offset `tools/launcher_music.py` uses, writes `res/music/launcher.wav`, and quits.
- Time: Options is drawn at about frame 962, about 16 s (page-0001.log:94-98), then a 64 s loop, so about 85-90 s
  [inferred]. Step 2 also runs in this child, so options.rgba is made here too.
- Second choice, no child: record the song in the normal game the first time the player sits on the title Options
  long enough, and write the wav then. The launcher stays silent until that happens [inferred].
- Not chosen: fast-forward. Interrupts run 4x (hle.c:4421) and the chunk is written before the fast-forward return
  (audio.c:238 vs :246), but MusyX at 4x is untested.
- Risks:
  - Two processes on one `pipeline_cache.db`. The launcher and the child both open it; aurora aborts the cache on an
    SQLite error (`pipeline_cache_abort`, pipeline_cache.cpp:883-916) [inferred: a busy lock could do it]. Either let
    the child use `AURORA_CACHE_DIR` set to a temp folder, or let the launcher close its pipeline work first.
  - The child's settings: give it its own copy (`ED_SETTINGS`, as jtg.sh does), so the user's settings.ini isn't
    touched and the title video setting can't change the timing.
  - A failure leaves the wav missing. The launcher already copes (frontend.cpp:3493-3495).
- Touches: the child warms Dawn's shader cache while it waits [inferred], which also helps step 4's bar.

### Step 6. Stamp each piece

- File: `res/firstrun.txt` (docs/release-first-run.md:17), or the SDL pref path if res/ may not be writable
  [inferred].
- One line per piece: its own version number (bump it only when its maker changes), the disc id (`ed_game_id`) and
  language word, and for the HD pieces the pack file's path, size and mtime that the index picked
  (texpack.cpp:403-407).
- Pipelines need no stamp of ours: aurora keys rows by `config_version`, Dawn by adapter. To decide whether to show
  the wait, record "prepared for adapter + GXCore version" [inferred].
- A failed piece is offered again next start. Every user of a piece already copes with it missing (frontend.cpp:2867;
  :3493-3495; titlevideo.cpp:204-213).

### Step 7. Optional: the HD title pictures

- With the pack on, the Preparing page reads the pack's title (`tex1_850x480_2a6eea64eb8fbbbb_14.dds`) with
  `texpack_read_rgba` (texpack.cpp:443-479; it decodes BC7 with bcdec), paints Choose Character at 4x, and keeps it for
  the title with the video off. The full plan is in hdtitle.md.
- The same could make a 4x `options-clean.png` from `tex1_850x480_befc5ef6c1629595_14.dds`
  (textures/Eternal Darkness HD Texture Pack PAL supplement/). The erase mask is in 850x480 units; scale it by 4
  [inferred].
- `texpack_read_rgba` reads files; call it off the renderer's thread (texpack.h:20-23).

### Step 8. Package checks

- make-win-package.sh already leaves out every git-ignored piece (`git archive res`, :33).
- Add: fail if `res/music/`, `res/video/options*.rgba|png`, `mark-*.png`, or the 12 disc portraits are in the zip
  (docs/release-first-run.md:31-32), and the seed version check from step 1.
- Fix the stale note in tools/title_video.py:6 (it says title.mpg is git-ignored; .gitignore:12 ships it).

## 5. How to check each step

Never `ed test`. Use fresh Jump-to-Game or title runs only, off the desktop: `ED_RUN_AWAY=1`. For a fresh install,
don't move the user's files: copy `res/` to a scratch folder without the made files and point the run at it, with
empty caches.

Setup for a "fresh install" run [inferred: the switches exist, this mix is untested]:

- `ED_RES_DIR=<scratch>/res` (a copy of res/ without `video/options*`, `video/mark-*`, `music/launcher.wav`,
  `ui/portraits/<12 names>.png`);
- `AURORA_CACHE_DIR=<empty scratch folder>` (both caches empty);
- `ED_TEXTURE_PACK=E:\Eternal Darkness Recomp Fork\textures` (the pack is looked for beside res/ otherwise,
  texpack.cpp:370-373);
- the run as `logs/run-20261117-090616-jtg-s4fix-fresh-map` was made (its env.txt: `ED_RUN_AWAY=1`,
  `ED_RUN_FRAMES=1500`, `ED_PAGE_TEST_KEYS`, `JTG_TITLE_AT`).

| Step | Run | What to see |
|---|---|---|
| 1 | fresh-install setup, a jtg.sh chapter run | the "Seeded" line (if made visible) and "[pipelines] N cached pipelines built at start" with N near the seed's row count, not about 10; the sepia flashback or first room's effects present in the first shots |
| 2 | fresh-install setup, a title run that opens Choose Character first (as -090616's `ED_PAGE_TEST_KEYS`) | the log line "the Options picture kept" before "the title look's picture written", and no "NOT written" line; shot: the cards on the Options picture, not black (compare with 05-fresh-run-cards-first-black-behind.jpg); md5 of the new options.rgba and options-clean.png equal to the user's own res/video copies |
| 2 | same, then Load Game | the Load screen with the skull glow |
| 3 | fresh-install setup with a copy settings.ini that chooses a character (`JTG_P1=4`) | the card shows Karim's portrait; "[frontend] character portraits written" in the log |
| 4 | a launcher run (`run.sh launcher NAME`) with the fresh-install setup | the Preparing page and its bar in the shots; it ends when the queue is empty; a second launcher run shows no Preparing page |
| 5 | same | `res/music/launcher.wav` in the scratch res, 2,048,297 samples at 32 kHz; the user listens to the launcher on the next start; the user's settings.ini unchanged (md5) |
| 7 | fresh-install setup, title video off in the copy settings | the title shot sharp at 1280 wide (`ED_RUN_SHOTS_W=1280`), next to Screenshots for Claude/s2-choose-character/02-off-choose-lit-(second-paint).jpg |
| 8 | the package script | it fails on a zip with a disc-made file or a stale seed |

## 6. Open, not checked

- That texture 2 is index 2 of the bank at *(0x80309134). The hash check covers it, but it is not seen.
- The exact row order and byte match of a renderer-maths decode against today's options.rgba.
- How long a cold Dawn compile of about 6,600 pipelines takes on this PC. No log of a fully empty cache exists.
- Whether two processes can share `pipeline_cache.db` safely.
- MusyX timing at 4x.
- Other languages: only the English Options hash is known (02-title-menu.md:353). Steps 2 and 7 do nothing elsewhere.
- docs/release-first-run.md should be updated with steps 1-3 and the new title presses when this is built (not done
  here: this research writes only this page).

## Skeptic's check

Read only. Nothing was built, run or changed, and only this section was added to this page. Every claim was read
again in the code, disasm.txt, the logs and both pipeline databases (sqlite, opened `mode=ro`).

### Confirmed

- The log lines:
  - "NOT written ... black behind" (page-0001.log:86);
  - "the Options picture kept" (:98), right after the A at frames 960/962;
  - "written (res/video/options-clean.png)" (:112);
  - "6638 cached pipelines built at start in 5648 ms" (:66).
  - "2954 ... 19248 ms" and "5506 ... 22128 ms" are in the two logs named.
- `keep_skull` has one caller: titlevideo.cpp:1028, for `kOptionsHash` only. `skull()` reads the file only once
  (:202-213). `titlevideo_title_art` returns false without the kept picture (:1150-1151).
- `ensure_title_art` tries again on each title render until it works (frontend.cpp:2858-2869, :3124, :4966).
  `title_art_ready` asks RmlUi for the picture only once the file exists (:2878-2889).
- The seed: GX v13 x8, Rml v2 x2, GXCore v10 x295 (read).
  - The code wants GXCore 13 (gxcore_draw.hpp:61), GX 13 (gx/pipeline.hpp:19) and Rml 2 (rmlui/pipeline.hpp:13).
  - The user's cache: GXCore v13 x6,624, Rml v2 x8, GX v13 x8; the file is 7,454,720 B (read).
  - The order at start is seed, then prune, then load (pipeline_cache.cpp:866, :1138, :1155). So the 295 seeded
    GXCore rows are merged and deleted again at every start. Confirmed.
- The old rows can't be converted. Seed GXCore configs are 872 bytes and the user's v13 ones are 880 (read). A new
  seed must come from real v13 rows, as step 1 says.
- A new pipeline's draw waits up to 50 ms, then is skipped (pipeline_cache.cpp:1215-1250).
  - Cached rows queue as Background (:380).
  - A new draw's pipeline moves to the front of the one worker queue (:1236-1240).
  - `aurora_get_stats` has `queuedPipelines` and `createdPipelines` (gfx.h:19-31).
  - The "built at start" line prints when the queue is empty (:432-433).
- The bank:
  - 0x80019850-0x8001985C are as quoted (disasm.txt:21789-21792);
  - TITLE_RECORD is 0x80309124 (hle.c:17175);
  - `tome_plaques_page`'s walk and size formula (hle.c:17438-17463);
  - `title_main_draw` (hle.c:17287-17299).
- The override hash is `XXH64(bytes, base_level_size, 0)` of the raw texels (gxcore_draw.cpp:2715-2716). So a hash
  check on bytes copied from guest memory is the same check the renderer makes.
- `disc_playable` needs `g_discs`. `g_discs` is filled only in `frontend_run_launcher` (frontend.cpp:3726). So a
  no-launcher run never writes portraits. This is confirmed and no longer [inferred].
- The cards never call `ensure_portraits` (frontend.cpp:1104-1113); the roster does (:1141). `tried` is set only after
  the disc check (:1064-1067), so an early call with no disc doesn't use up the one try.
- `ED_PCM_OUT` writes before `ED_MUTE` and before the fast-forward return (audio.c:236-246).
- The launcher and its switches are as quoted:
  - the launcher, its skip switches and its frame call (main.c:680-690; frontend.cpp:3562, :3669, :3720);
  - `ED_RES_DIR` (frontend.cpp:3396);
  - `AURORA_CACHE_DIR` (.deps/GXRuntime/graphics/aurora/lib/aurora.cpp:434-444);
  - `ED_TEXTURE_PACK` (texpack.cpp:366-376);
  - the seed path (pipeline_cache.cpp:603-622);
  - `git archive "$COMMIT" res` (make-win-package.sh:33).
  - Of these pieces, git tracks only `res/initial_pipeline_cache.db` and `res/video/title.mpg` (git ls-files).

### Wrong, with corrections

1. **The renderer doesn't decode with texture_convert.cpp.**
   - The GXCore path calls `gxc::decode_texture` (gxcore_draw.cpp:2954), in
     .deps/GXRuntime/graphics/gxcore/src/texture_decode.cpp:240. CMPR is at :154-199.
   - Its maths match texture_convert's: expand5/6, (3a+5b)>>3, and the half blend with alpha 0 (:26-43, :172-181).
   - The host can call that same function. It is public (gxruntime/gxcore/texture_decode.hpp:50).
   - It is built as `GXRuntime::gxcore`, and aurora already links it (GXRuntime CMakeLists.txt:106). The game target
     needs one `target_link_libraries(... GXRuntime::gxcore)` line in the project's CMakeLists.txt, and no GXRuntime
     patch [inferred: not built].
   - This removes the byte-match risk: the same function gets the same bytes and gives the same output.
2. **There is no flip between the renderer and the host.**
   - `decode_texture` writes rows in stored order, top of memory first (texture_decode.cpp:185-188).
   - `ask_override` passes them on unchanged (gxcore_draw.cpp:2079-2087, :2988).
   - The picture itself is stored upside down (titlevideo.cpp:2, :37, :49, :476 say so). That is why
     `titlevideo_title_art` turns it over.
   - So hand `decode_texture`'s output straight to `keep_skull`, with no flip.
3. **The music has no "fixed offset".**
   - `tools/launcher_music.py` takes SECONDS from the user: "a few seconds after the screen opened" (:12-14, :27).
   - Its doc asks for over two minutes of recording (:9-10).
   - So the child must choose its own start, some seconds after it sees the Options state [inferred: any start past
     the song's first bars loops, because the cut is exactly one loop].
4. **Step 2 should not call `titlevideo_title_art` from `title_main_draw`.**
   - It isn't needed. Once `keep_skull` has run, `ensure_title_art` makes the PNG at the next title page
     (frontend.cpp:3124, :4966).
   - It does harm. It would write the PNG on the game thread, and libpng is slow. It could also race the frontend's
     own call, which writes the same three files.
   - Only `keep_skull` belongs in the hook. Even that writes 1,632,000 B on the game thread (titlevideo.cpp:222-224).
     If a hitch shows, move the file write to another thread [inferred].
5. **Step 5 contradicts itself.**
   - It gives the child its own `AURORA_CACHE_DIR`, then says the child warms Dawn's cache for step 4. With its own
     folder, the child warms only its own cache. Choose one.
   - With a private folder, the child also compiles all ~6,600 pipelines cold while the launcher compiles them, on the
     same CPU and GPU [inferred: slower for both].
6. **At run time the seed merge is not INSERT OR IGNORE.**
   - The runtime merges with an UPSERT that overwrites `config_version` and `config` (pipeline_cache.cpp:852-857).
   - A seed row with the same (type, hash) as a newer user row would make that row old, and the prune would then
     delete it.
   - Today 0 of the 295 overlap (read), so nothing is lost now. A current seed (step 1) is harmless too, and step 1's
     release check keeps it current.
7. **Some line numbers have drifted.**
   - `find_rows` is titlevideo.cpp:1153, not :1151.
   - The flip is :1155-1158, not :1152-1155.
   - The RmlUi comment is frontend.cpp:2878-2879.
   - The stale "git ignores it" note is tools/title_video.py:7, not :6.
   - The seed merge is pipeline_cache.cpp:658-754.
8. **No log shows that the two older runs had a cold Dawn cache.**
   - The logs give only counts and times.
   - They are slower per pipeline (6.5 and 4.0 ms, against 0.85 ms warm), which fits a cold cache.
   - Mark it [inferred].

### Missed

- **The child run and the memory card.**
  - A child opens the default card and its `-sram.bin` unless told otherwise (main.c:745-779). Give it `--no-card` or a
    scratch `ED_CARD`.
  - A missing or empty card may bring up a card notice on the title, and that notice takes the presses
    (TITLE_CARD_NOTICE 0x800AF00C, hle.c:17182) [inferred].
  - On a true fresh install there is no card yet. So the blind press script is the weakest part of step 5.
- **Drive the child by state, not by frames.**
  - `ED_INPUT_SCRIPT` frames shift with loads (jtg.sh:19-20).
  - The Down x3 path also needs the Choose Character row. That row exists only in English, and only once the menu
    picture is repainted (`titlevideo_menu_painted`, hle.c:17291).
  - Better: press until `TITLE_STATE` (TITLE_RECORD+62) reads 14, then start the clock [inferred: a small hle hook].
- **The away window is a test tool.** `ED_RUN_AWAY` uses tools/ed/away.ps1 (run.sh:17-18), which isn't in the zip. A
  release child needs its own hidden SDL window [inferred].
- **A cheap magic check.**
  - The game's unpack checks the magic 0x0020AF30 (06-texture-bank.md:47).
  - Step 2 can check `mem_read32(bank) == 0x0020AF30` and a count above 2 before it takes descriptor 2.
  - The portraits code checks the TPL the same way (frontend.cpp:1072).
- **"Music etc" may also mean the title music stopping.**
  - When Choose Character opens in the fresh-map run, the log has "[audio] ran dry after a 1946 ms pause"
    (page-0001.log:90).
  - That fits the page pausing the title for the two test keys (`ED_PAGE_TEST_STEP_MS=900`, env.txt), not a compile
    stall [inferred].
  - Even so, it means the title's music stops while Choose Character is open.
  - Ask the user which "music" they mean before building step 5.
- **The fresh-map run was not a fresh install.**
  - Only options.rgba and options-clean.png were moved aside (coop-next-session.md:127-131).
  - The caches were warm (6,638 built).
  - No run has shown a truly empty cache.
- **The pack doesn't change step 2.**
  - A claimed texture skips the pack (gxcore_draw.cpp:2733-2737, `!claimed`).
  - So the kept Options picture is always the disc's, with or without the pack, and step 2's bytes match it.
- **Where res/ is in a release.**
  - `res_dir` prefers `res/` in the working folder (frontend.cpp:3389-3400), and play.bat runs `cd /d "%~dp0"`
    (play.bat:7). So the made files go in the game folder.
  - That folder must be writable. It is, unless the user unpacks the game under Program Files [inferred].

### What it changes in the plan

- Step 1: no change. Keep the release check, which also guards against the UPSERT problem above.
- Step 2: simpler and safer.
  - In `title_main_draw`, once per run, check the magic and the count, and that descriptor 2 is format 14 and 850x480,
    with XXH64 == `kOptionsHash`.
  - Then call `gxc::decode_texture(14, 850, 480, bytes, size)` and pass the result to `keep_skull`, with no flip.
  - Don't call `titlevideo_title_art`.
  - Add the `GXRuntime::gxcore` link line.
  - The md5 check against today's options.rgba is still the proof.
- Step 3: no change.
- Step 4: no change. On a cold Dawn cache, the pipeline part alone takes 20-45 s [inferred].
- Step 5:
  - drive the child by `TITLE_STATE` == 14, and let it choose the cut start;
  - give it a scratch card, a hidden window and its own settings;
  - decide which cache folder it uses, one way or the other;
  - or build the "second choice" first: record during the player's own first visit to the title Options. It needs no
    child, no card and no presses.
- Ask the user what "music etc" means before step 5.

### Verdict

- The page is right about what the user saw and why.
- It is right about the biggest find: the shipped pipeline list is stale and does nothing today.
- Steps 1-4 hold.
- Step 2 needs the fixes above: call the renderer's own decoder, don't flip, and don't call the title-art code on the
  game thread. With them, step 2 is less risky than the page says.
- Step 5 is the weak part. Its press script, card and cache folder need the changes above, and the user should first
  say which "music" they mean.

## Built: step F4 (2026-11-17)

Steps 1-4, 6 and 8 are built. Step 5 (the launcher music) is not: the user said the launcher music is fine to keep.
Step 7 was already done by F3 (the boot workers); the page waits for it. Nothing is committed.

### What was built

- **Step 1, the shipped pipeline list.** `tools/pipeline_seed.py make` copies the player's `pipeline_cache.db` (and its
  -wal) to a temp folder and reads only the copy. It keeps only rows at the current versions, read from the renderer's
  source (GX 13, Rml 2, GXCore 13, Clear 2). `res/initial_pipeline_cache.db` is now 6,642 rows (6,626 GXCore v13),
  7,073,792 B. `tools/pipeline_seed.py check` fails on a stale list; the old one failed ("295 GXCore rows at version 10").
- **The seed merge (GXRuntime patch, pipeline_cache.cpp).** A seed row at another version is skipped. A seed row replaces
  a player's row only when the player's row is older (`... DO UPDATE ... WHERE pipeline_cache.config_version <
  excluded.config_version`). So a seed can never age a newer row. The merge prints on stderr: "[pipelines] seeded from
  '...' (6642 rows merged, 0 skipped ...)".
- **Step 2, the Options picture from the game's memory.** `title_options_from_bank` (hle.c), called from
  `title_main_draw`. Once a run, and only while `options.rgba` is missing, it reads the bank at TITLE_RECORD+16, checks
  the magic 0x0020AF30 and the count, and walks the pictures (picture 2 first). Only an 850x480 CMPR picture whose
  XXH64 is `kOptionsHash` is handed to `titlevideo_options_from_bank` (titlevideo.cpp). That decodes it on a worker with
  `gxruntime::gxcore::decode_texture` and calls `keep_skull`. No flip, and no `titlevideo_title_art` call in the hook.
  CMakeLists.txt links `GXRuntime::gxcore` into the host.
- **Step 3, the portraits.** `portrait_rml` calls `ensure_portraits` when a portrait file is missing, so a card or the
  roster makes them. `ensure_portraits` reads the launcher's disc, else the run's disc (`frontend_set_disc`, called by
  main.c once the disc path is known), so it also works with no launcher.
- **Step 4, the "Preparing the game from your disc" page** (`run_preparing`, frontend.cpp; style `body.preparing` in
  menu.rcss). It sits on the launcher's parchment, before the launcher menu, once a playable disc is found. It shows what
  is being made, a bar, and "N of M" for the shaders. Pieces: the portraits (`ensure_portraits`), the pipelines (waits
  until `aurora_get_stats()->queuedPipelines` is 0; bar = created / (created + queued)), and the HD title pictures (waits
  for the boot's HD workers, `titlevideo_hd_busy`). B skips; what is not done is offered again next start. It stops after
  5 minutes. The page is shown only when the work takes longer than 0.7 s, so a quick piece doesn't flash it. The page
  waits 33 ms a frame (`ED_PREPARE_FRAME_MS`), see "Found" below. The launcher music goes on under it.
  `ED_PREPARE=0` skips it, `ED_PREPARE=1` makes every piece again. A test with `ED_MENU_OPEN` skips it.
  `--no-launcher`, `ED_QUICKLOAD` and `ED_NO_LAUNCHER` never reach it.
- **Step 6, the stamps.** `res/firstrun.txt` ("portraits 1 GEDP01", "hd ..." with the pack file's size, time and path,
  or "hd off") and `prepared.txt` in the renderer's cache folder ("pipelines SIZE XXH64" of the shipped list). A cleared
  cache, a new shipped list, an older portraits version, or a new pack shows the page again. `.gitignore` has
  `res/firstrun.txt`.
- **Step 8, package checks** (make-win-package.sh): it fails if `res/music/*`, `res/video/options*`, `mark-*`,
  `res/firstrun.txt` or one of the 12 disc portraits is in the package, and runs `pipeline_seed.py check` on the packed
  list. Tried on a made-up folder (clean, then with three made files, then with the old list). The whole script was not
  run (no stage folder here). The stale note in tools/title_video.py is fixed.
- Test switch: `ED_LAUNCHER_TEST_KEYS="a"` presses on the launcher (as `ED_PAGE_TEST_KEYS`).

### What was seen (runs with a scratch install: ED_RES_DIR, AURORA_CACHE_DIR, ED_SETTINGS and ED_CARD in a scratch
folder; the user's res, caches, settings and card not used; the user's pipeline_cache.db md5 unchanged)

- **A true fresh install** (empty res made files, empty caches, new settings and card): logs/run-20261117-141833-fresh4-first.
  - The page and its bar: Screenshots for Claude/f4-first-run/01 to 03 (36 of 6,642, then the bar near the end).
  - "[prepare] done in 132227 ms (portraits 23 ms, pipelines 132225 ms)". The seed merged 6,642 rows.
  - Then the title (English, "Continue without saving" on the new card's questions), then Choose Character at frame
    1121, before Options was ever opened. The log has "the Options picture handed over from the title bank (picture 2
    of 83, frame 861)", "taken from the game's memory at the title (5 ms)", then "the title look's picture written" and
    "the title look's backdrop: the disc's". No "NOT written ... black" line. Picture 04: the cards on the skull, not
    black. Picture 05: the roster with the disc's portraits.
  - The kept `options.rgba` and the written `options-clean.png` have the same md5 as the user's own files
    (f38273fc... and 8e720ee7...), here and in run -140943.
- **The second start** (same scratch): "[prepare] everything prepared already", no page; the warm list built in the
  background in 4,724 ms (run -141417-fresh3-second).
- **HD turned on** (same scratch, the pack named with ED_TEXTURE_PACK): only the HD piece, 319 ms; Choose Character
  on the HD skull (picture 07, run -141459-fresh3-hd). After the 0.7 s rule, the same case (235 ms) shows no page
  (run -141617).
- **No launcher, a chosen character** (`mod_p1_character = 4`, no portraits in res): the card made them and shows
  Karim (picture 06, run -142420-fresh6-nolauncher-card).

### Timings on this PC

- Cold (empty Dawn cache): 163 s and 162 s with a busy page loop; 132 s with 33 ms frames (run -141833); 132 s with
  100 ms frames (run -142146-fresh5-cold-100ms). So 132 s is the compile itself, about 20 ms a pipeline on one worker.
- Warm (Dawn cache kept, `ED_PREPARE=1`): 30.1 s with the busy loop (run -141626-fresh3-warm-again), 3.1 s with 33 ms frames
  (run -141808-fresh3-warm-33ms).
- Portraits: 18-26 ms. HD pictures: 233-306 ms. Options picture from memory: 5-6 ms on the worker.

### Found

- **A busy UI loop starves the pipeline worker.** With the page drawn as fast as it could go, a warm build took 30 s;
  with a 33 ms wait a frame, 3 s. The launcher menu's own loop (run_menu) has no wait either, so pipelines building
  behind the launcher menu are likely slowed the same way [inferred, not measured]. Not changed here.
- **A new card asks for the language first** ("Please select your language", then "Save Game Settings", then which slot).
  This is the game's own first-start flow, not ours. A fresh install's player sees it once.

### Not seen

- A real release folder (play.bat, the zip) and the full package script.
- Another language's disc: step 2 then takes nothing (the hash check), and Options keeps it as before.
- The user's own first start: their cache has no `prepared.txt` and their res has no `firstrun.txt`, so they will see
  the page once, for about 3-5 s (warm), then never again until an update.
- Closing the window during the page (it returns false, and the launcher closes: from the code only).

## Built: step F4 review fixes (2026-11-17)

Three review findings. Two were the same problem, and both were real. The third was real too.

- **The shipped list held both anti-aliasing settings, and all of it was built.** The list came from the user's own
  cache, which has rows from play at 4x MSAA as well as 1x. Of its 6,626 GXCore rows, 4,568 are 4x and 2,058 are 1x (counted again
  here: the run's log says "4568 ... left unbuilt"). A new player starts with anti-aliasing off (settings.c), so the
  renderer runs at 1x (main.c `.msaa`), and its sample count is fixed for the run (webgpu `g_graphicsConfig`). The
  page still waited for all 6,642.
  - **Fix (GXRuntime patch, pipeline_cache.cpp `load_pipeline_cache_entries`):** a row whose `msaaSamples` is not 1
    and not this run's count is skipped at start. It is not deleted, so a later run with that setting still builds it.
    1x rows are always built, because the offscreen passes draw at 1x (common.cpp). The log says
    "[pipelines] N cached pipelines of type 3 left unbuilt: another anti-aliasing setting (1x now)".
  - **The stamp now names the setting** (frontend.cpp `run_preparing`): `prepared.txt` is
    "pipelines SIZE XXH64 msaa N". Turning 4x on (or off) shows the page once more, for the other setting's rows.
  - The list itself is unchanged (`tools/pipeline_seed.py` keeps both settings, and says so).
- **The package check read the versions from this checkout's `.deps`, not from the packaged commit.** On a machine
  with no `.deps`, or with `.deps` unpatched, it crashed or read GXCore 8 and failed a good list.
  - **Fix:** `pipeline_seed.py check DB --commit COMMIT` reads the GXRuntime pin from that commit's
    `tools/setup-deps.sh`, the pinned header with `git show`, and the patch's own `+constexpr ... = N;` line from that
    commit's `patches/gxruntime/eternal-darkness.patch`. The working tree's `.deps` state does not matter. A missing
    `.deps` gives a clear message. `make-win-package.sh` passes `--commit "$COMMIT"`.
  - Tried: HEAD and the working tree pass (GXCore 13); the old 295-row list fails at HEAD; the new list fails at
    7a4c662 (wants 11) and at 76986e6 (no patch line, the pinned header's 8); the old list passes at 77d35b0 (10).

### Seen (scratch install in the scratch folder f4r1: ED_RES_DIR, AURORA_CACHE_DIR, ED_SETTINGS, ED_CARD)

- **Fresh install, default settings (1x), cold:** logs/run-20261117-143748-f4fix-cold1x. "4568 ... left unbuilt",
  "2074 cached pipelines built at start in 46023 ms", "[prepare] done in 45307 ms". It was 132 s before the fix.
  Pictures in `Screenshots for Claude/f4-review-fixes/`: 01 the page at "313 of 2,074", 02 the launcher after it,
  03 Choose Character on the skull (frame 1121, before Options was opened), 04 the roster with the disc's portraits.
  The Options picture was again taken from the title bank (picture 2 of 83, frame 861).
- **The same scratch, 4x turned on:** logs/run-20261117-144018-f4fix-aa4-after1x. The page came again (stamp
  "msaa 1" against "msaa 4"). No rows left unbuilt; all 6,642 built in 113 s (the 4x ones cold). Picture 05: the page at
  "1,448 of 6,642".
- **Back to 1x:** logs/run-20261117-144237-f4fix-back-to-1x. 2,074 built in 1.3 s; the step was done in 689 ms, under
  the 0.7 s rule, so no page was shown.
- The user's own `pipeline_cache.db` and -wal md5 are unchanged, and there is still no `prepared.txt` in their folder.

### Not seen

- A player who turns 4x on after a 1x run builds the 4x rows cold once (the page, about 70-110 s here). After a 1x run,
  Dawn's prune (gpu_cache.cpp `cache_prune`) drops the 4x compiled blobs it did not use. So going 4x, 1x, 4x builds the
  4x rows cold again [from the code; the 1x-then-4x case was seen, the second 4x was not run].
- The full package script on a real stage folder (there is none here).
- The user's own next start. Their settings.ini says `anti_aliasing = 0` now, so the 4x rows in their cache will be
  left unbuilt, and the page should be shorter than the 3-5 s expected before (warm, 1x rows only).
