# The launcher's settings, all of them, and the in-game menu that shows them

Research for bringing the launcher's settings into the game itself (the title menu's Options and Tome pages), 2026-11-17.
Read only: nothing was built or run. "Inferred" marks what was read from code but not seen in a run.

Files read: `runtime/host/frontend.cpp` (4417 lines), `runtime/host/frontend.h`, `runtime/host/settings.c`,
`runtime/host/settings.h`, `res/ui/menu.rml`, `res/ui/menu.rcss`, `runtime/host/fonts.cpp`, `fonts.h`, the heads of
`glyph_art.cpp`, `buttons.cpp`, `tome.cpp`, `titlevideo.cpp`, parts of `controls.cpp`, `camera.c`, `hle.c`, `main.c`,
`report.c`, `texpack.cpp`, `lightmaps.cpp`, `audio.c`, and `.deps/GXRuntime/backends/aurora/aurora_backend.cpp`.

## 1. The short version

- Every launcher page is one RmlUi document, `res/ui/menu.rml`. Its rows are rebuilt from C++ (`build_rows`,
  frontend.cpp:2734) for each screen. The launcher and the in-game menu are the same pages. The in-game menu is the
  launcher's pages over the paused game, with `body.overlay` (menu.rcss:27).
- There are 16 screens (`enum class Screen`, frontend.cpp:299): Launcher, Play, States, Slot, SlotDelete, SlotOverwrite,
  Options, OptionsPage, Graphics, Controls, Mapping, InGame, Problem, Mods, ModsPage, Characters.
- All settings live in one struct, `Settings g_settings` (settings.h, settings.c:14), saved to `settings.ini`
  (`ED_SETTINGS` names another file, settings.c:96). `settings_apply()` (settings.c:340) pushes window, frame rate and
  audio values. `settings_apply_graphics()` (settings.c:356) pushes the renderer's effects and is safe from the game
  thread.
- Almost every setting takes effect at once, in the game too. Only three need a new start: MSAA, texture filtering and
  co-op's separate rooms. Two more change at the next load: the character swap, and the Remastered spells' texture list
  (inferred).
- The in-game menu opens with Esc or a tap of the controller's Back/Select. It pauses the game because the main thread
  stops marking frames as shown, and the game thread may run at most one frame ahead (aurora_backend.cpp:1631-1640).
- Only the first connected controller and the keyboard and mouse drive the menus (`gamepad()`, frontend.cpp:89).
- The Tome's "Choose Character" button already exists. It is an ImGui plaque drawn over the Tome
  (`draw_tome_button`, frontend.cpp:3325). It opens the RmlUi roster through an atomic flag (`frontend_open_characters`,
  frontend.cpp:4257).
- Found by reading: **Back from that roster does not return to the Tome.** It walks Characters, then a Mods page, then
  Mods, then `g_mods_return`. `frontend_frame` never sets `g_mods_return`, so it can be the Launcher screen over the
  running game. See section 3.6.

## 2. Inventory of every setting and row

How to read the "Takes effect" column:

- **At once**: the reader reads `g_settings` each frame or call, or the row calls an apply function.
- **Next start**: read once when the program starts (`main.c`).
- **Next load**: applied at a reload, a save-state load or a chapter load.
- **Launcher only**: means nothing once a game runs, or the row is hidden in game.

"Where": L is the launcher, G is the in-game menu, L+G is both.

### 2.1 Launcher main page (`build_launcher`, frontend.cpp:1389)

| Label | Field / action | Values | Code | Takes effect | Where |
|---|---|---|---|---|---|
| Disc | `g_settings.disc` (char[1024]) | discs found in `dump/` and `.`, GEDP01 only (`scan_discs`, :252) | `disc_row` :388; saved at once | Launcher only. In game a different disc restarts the program (`start_game` :526, `restart_process` :460) | L (on the main page until a playable disc is chosen, then on Options) |
| Continue | action: newest slot | `saves/slotN.state`, N 1-100 | `start_game(newest)` :1398 | loads the state | L |
| Main menu | action | - | `start_game(0)` :1406. With Skip intro it uses `saves/title.state` or `title-rooms.state` (:478, :544) | boots the disc | L |
| Load game (Experimental) | opens Play | used slots | `build_play` :1536 | - | L |
| Choose Character (Experimental) | opens ModsPage section 3 | - | `mods_page` lambda :1422 | - | L |
| Co-op (Experimental) | opens ModsPage section 1 | - | :1438 | - | L |
| Options | opens Options | - | :1440 (calls `dol_aurora_measure_screen`) | - | L |
| Mods | opens Mods | - | `mods_row` :574 | - | L |
| Quit | action | - | :1451 | - | L |

### 2.2 Options (`build_options`, :2231) and its pages (`build_options_rows`, :1874; `build_options_page`, :2172)

Options lists Display, Sound, Graphics (Experimental), Controls, then Disc (launcher only, once a disc is chosen), then
Back. Graphics (`build_graphics`, :2206) lists Image, Shadows, Lighting and Effects. Each page is one section of
`build_options_rows`, picked out by its Roman numeral (`section_number`, :2163). Sections IV (Controls) and VI (Mods)
are only rows that open other pages. Section V ("Game") has no rows left: `ED_MENU_OPEN=game` opens an empty page
(:3231).

| Page | Label as shown | Field | Values / range | Applied by | Takes effect | Where |
|---|---|---|---|---|---|---|
| Display | Fullscreen (not on PS5) | `fullscreen` | On/Off | `changed()` → `settings_apply` → `dol_aurora_set_fullscreen` (settings.c:346) | At once | L+G |
| Display | Resolution | `render_scale` | 0 Window/Screen (WxH), 1 720p, 2 1080p, 3 1440p, 4 4K; the label shows pixels for the current aspect (:1885) | `dol_aurora_set_render_height` (settings.c:345) | At once | L+G |
| Display | Frame rate | `frame_rate` | 60 up to 360 (load clamp, settings.c:127), from the screen's rates (`dol_aurora_frame_rate_choices`, :1934). Disabled in co-op, which shows 60 | `dol_aurora_set_frame_rate(mod_coop ? 60 : frame_rate)` (settings.c:349) | At once | L+G |
| Display | Aspect ratio | `aspect` | 1 4:3, 2 16:9, 4 21:9 (an old 0 or 3 reads as 16:9) | `dol_aurora_set_aspect_mode`, plus the game's own widescreen word each frame (`sync_widescreen_option`, hle.c:6237) | At once | L+G |
| Display | HDR | `hdr` | On/Off; "Not on this display" without HDR | `enhancement::change` → `settings_apply_graphics` → `dol_aurora_set_hdr` | At once | L+G |
| Display | Calibrate HDR (only where HDR exists) | `hdr_peak` 100-4000 step 50, `hdr_paper` 60-400 step 10, `hdr_black` -10..10 | 3 steps (peak, brightness, black) | `run_hdr_calibration` :3582. Hides the RmlUi page, draws test patterns (`dol_aurora_set_hdr_pattern`) and an ImGui caption (:3506) | At once | L+G |
| Display | Camera (Experimental) | `camera` | 0 Original, 2 Third person (beta), 3 First person (beta). Note: right stick turns / looks | `camera_mode()` reads it at each call (camera.c:130) | At once | L+G |
| Display | Game text | `game_font` | 0 Sharp, 1 Tome (default), 2 Original | `fonts_claims` / `fonts_apply` (fonts.cpp:312); `fonts_generation` (:365) changes, so the sheets are rebuilt | At once (inferred: the renderer re-asks for the sheet on a new generation) | L+G |
| Display | FPS counter | `fps_counter` | On/Off | `update_fps_counter` :3400 (ImGui); window title, hle.c:5479 | At once | L+G |
| Sound | Music | `music_volume` | 0-100 step 10 (slider) | audio.c:208 scales the disc stream; launcher music gain :3016 | At once | L+G |
| Sound | Effects & voices | `effects_volume` | 0-100 step 10 (slider) | audio.c:194 scales the game's mix | At once | L+G |
| Image | HD textures | `hd_textures` | On/Off; "No pack" when `texpack_count()==0` | texpack.cpp:361 `generation()` | At once (texpack.cpp:358 says so) | L+G |
| Image | Sharpening | `sharpen` | 0 Off, 1 Low, 2 Medium, 3 High | `dol_aurora_set_sharpen` | At once | L+G |
| Image | Anti-aliasing | `smaa` + `anti_aliasing` | Off / SMAA / 4x MSAA / 4x MSAA + SMAA | SMAA: `dol_aurora_set_smaa`, at once. MSAA: `main.c:663` backend config | SMAA at once. **MSAA from the next start** (note :2052) | L+G |
| Image | Texture filtering | `anisotropy` | 0-4 = 1x, 2x, 4x, 8x, 16x | `main.c:664` backend config | **Next start** (note :2068) | L+G |
| Shadows | Baked shadows | `sharp_shadows` | Sharp / Original | lightmaps.cpp:393, as light maps load | At once for new loads. Photo mode re-holds frames for it (:4078); inferred: the rooms already loaded swap at once through the override's generation | L+G |
| Shadows | Character shadows | `char_shadows` | 0 The game's, 1 Light, 2 Dark | `dol_aurora_set_char_shadows` | At once | L+G |
| Shadows | Shadow edges (disabled with The game's) | `shadow_hard` | Soft / Hard | `dol_aurora_set_shadow_hard` | At once | L+G |
| Shadows | Contact shadows | `contact_shadows` | On/Off | `dol_aurora_set_contact_shadows` | At once | L+G |
| Lighting | Per-pixel lighting | `per_pixel_lighting` | On/Off | `dol_aurora_set_per_pixel_lighting` | At once | L+G |
| Lighting | Skin scattering (needs per-pixel) | `skin_scatter` | On/Off | `dol_aurora_set_skin_scatter` | At once | L+G |
| Lighting | Ambient occlusion | `ssao` | Off / Soft / Strong | `dol_aurora_set_ssao`, and the HDR highlight (settings.c:394) | At once | L+G |
| Lighting | Occlusion quality | `ssao_high` | Standard / High | `dol_aurora_set_ssao_high` | At once | L+G |
| Effects | Glow | `glow` | Off / Soft / Strong | `dol_aurora_set_glow` | At once | L+G |
| Effects | Spell glow | `spell_rim` | On/Off | `dol_aurora_set_spell_rim` | At once | L+G |
| Effects | Remastered spells | `spell_fx` (default Off, docs/runes-3d.md) | On/Off | `runes_frame` reads it each frame (hle.c:10092); texpack picks spell files (texpack.cpp:280, :363) | At once. The spell texture list is read only at start, so with no pack at start there is nothing to swap (inferred) | L+G |
| Effects | Haze | `volumetric` | 0-100 step 5 (slider) | `dol_aurora_set_volumetric(0.9*v/100)` | At once | L+G |
| Effects | Light shafts | `shafts` | Off / Soft / Strong | `dol_aurora_set_shafts` | At once | L+G |

### 2.3 Controls (`build_controls`, :2283) and Button mapping (`build_mapping`, :2356)

The same page serves two players. `controls::set_menu_player(0)` comes from Options > Controls (:2255).
`set_menu_player(1)` comes from Mods > Co-op > Player 2 controls (:1142). The heading is "Controls" or "Player 2
Controls" (:2589).

| Label | Field | Values | Applied by | Takes effect | Player |
|---|---|---|---|---|---|
| Plays with (section "Device") | P1: `controls_device` | 0 Controller and keyboard, 1 Controller, 2 Keyboard | controls.cpp:809-822 each read | At once, saved at once | 1 |
| Plays with | P2: `mod_p2_input` | 0 Controller, 1 Keyboard | the same | At once | 2 |
| Controller (when the player has one) | `pad_slot[p]` | 0 Automatic (name), else the Nth connected (at least 2 places) | `pad_index` controls.cpp:163 | At once (inferred), saved at once | each |
| Controller type | `pad_type[p]` | 0 Automatic (detected), 1 Xbox, 2 XInput, 3 PlayStation, 4 Switch | controls.cpp:371 | At once | each |
| Layout (Keys when keyboard only) | P1 `controls_layout`, P2 `mod_p2_layout` (through `controls::set_layout`, saved by `controls::save`, controls.cpp:480) | Original / Modern / Custom (keys: Default / Custom) | `g_players[p].layout` | At once | each |
| Sprint (section "Handling") | `sprint_toggle[layout]` | Hold / Toggle | controls.cpp:503 | At once | **P1 page only**, but stored by layout (see 4) |
| Creep | `creep_toggle[layout]` | Hold / Toggle | the same | At once | P1 page only |
| Target | `target_toggle[layout]` | Hold / Toggle | the same | At once | P1 page only |
| Stick deadzone | `stick_deadzone[layout]` | 5-25 % step 5 | controls.cpp:341, :541 | At once | P1 page only |
| Vibration | `vibration` | On/Off | `dol_aurora_set_vibration` (settings.c:351) | At once | shared, P1 page only |
| Button mapping | opens Mapping | - | - | - | each |
| (Mapping) 12 actions: Attack / Confirm, Action / Cancel, Creep, Quickcast, Reload, Sprint, Target, Pause, Spell up, Spell down, Spell left, Spell right (controls.cpp:61) | `controls_custom` / `mod_p2_custom` (text) | Left/Right steps a button or key; A captures a press (`controls::begin_capture`); Esc cancels | `controls::step_pad`, `step_key`, `capture_key` | At once | each |
| (Mapping, keyboard players, right page) Move forward, back, left, right; Camera up, down, left, right (controls.cpp:86) | the same strings | keys | `step_key(kActionCount+d)` | At once | each |

### 2.4 Mods (`build_mods`, :1298) and its pages (`build_mods_rows`, :1118; `build_mods_page`, :1352)

The Mods list in game: Co-op (Exp.), Challenge, Characters (Exp., game only), Cheats (Exp.), Sanity Effects, Title
video, Back. On the launcher, Characters is missing (it is "Choose Character" on the main page) and "Skip intro" comes
before Title video (:1316-:1328).

| Page | Label | Field | Values | Applied by | Takes effect | Where |
|---|---|---|---|---|---|---|
| Co-op | Split-screen co-op | `mod_coop` | On/Off | many readers (124 in hle.c, camera.c, controls.cpp); `changed()` for the 60 fps lock | In game: player 2 is adopted or parked at once (`coop_adopt` / `coop_park`, hle.c:32538-32540, inferred from reading). **Separate rooms (`ED_ROOM_SLOTS`, `ED_COOP_STAY`) only follow at the next start or load from the launcher** (`rooms_follow_coop` :489, called in `start_game` :521 and main.c) | L+G |
| Co-op | Screen split (disabled with co-op off) | `mod_split` | 0 Side by side, 1 Above and below | `aurora_split_set(true, mod_split)` each frame (camera.c:1675, :1823); `draw_tome_button` and toasts | At once (inferred) | L+G |
| Co-op | Player 2 plays with | `mod_p2_input` | Controller / Keyboard | controls.cpp:809 | At once | L+G |
| Co-op | Player 2 controls | opens Controls as player 2 | - | - | - | L+G |
| Co-op | Player 1 camera | `camera` (the same field as Display > Camera) | Original / Third person / First person | camera.c:130 | At once | L+G |
| Co-op | Player 2 camera (disabled with co-op off) | `mod_p2_camera` | **0 Third person, 1 First person, 2 Original** (a different code from `camera`) | camera.c:1725-1741, hle.c:18078, :18183 | At once | L+G |
| Challenge | Creatures | `mod_enemies` | Vanilla / Enhanced | **nothing reads it** (note "In development: no effect yet", :1172) | - | L+G |
| Challenge | Sanity effects | `mod_insanity` | 0 Normal, 1 Enhanced (solo: any door below 95 %), 2 Off | hle.c:26611, :26916 at each call | At once | L+G |
| Characters | Player 1 character (a card) | `mod_p1_character` 0-12 (0 Default, 1 Pius ... 12 Alex, :588), `mod_p1_costume` (4 bits per character), `mod_p1_stats` | opens the roster | `characters_reload` (hle.c:32400): once in play (mode 2 at r13-26188), 60 frames settled, it writes a save through `0x800AE098` and ends the game into a load (`0x80022328(2)`) | **Next load**: in game the reload happens after the menu closes, in play (note :1198). On the launcher, also for a save state loaded | L+G |
| Characters | Player 2 character (a card, dimmed with co-op off) | `mod_p2_character`, `mod_p2_costume`, `mod_p2_stats` | the same | hle.c:7476, `coop_wanted_character` | Next load, co-op only | L+G |
| Cheats | All magick (enabled in game only) | not a setting: `hle_cheat_set(1)` (hle.c:25240) | On/Off | the magick record 0x80334D50 and flags 599-602 (hle.c:25200) | At once. Not saved in settings.ini (inferred: settings.c has no key) | G |
| Cheats | Full stats (game only) | `hle_cheat_set(2)` | On/Off | health, sanity and magick held at most, for each player | At once, not saved | G |
| Sanity Effects | Effect | a static index (:1232) | `hle_sanity_count()` effects, named | - | - | L+G |
| Sanity Effects | The game's limits | static bool | Keep / Ignore | - | - | L+G |
| Sanity Effects | Play this effect (game, solo only) | action | - | `hle_sanity_play`, closes the menu | At once | G |
| Sanity Effects | End the effect on now | action | - | `hle_sanity_stop` | At once | G |
| Sanity Effects | Random effects at doors | `hle_sanity_doors_set` | On/Off | - | At once, not saved | G |
| Sanity Effects | Sanity for the doors | action | Full, Below 75 %, Below 50 %, Below 25 % | `hle_sanity_doors_next` | At once | G |
| Sanity Effects | Last result | read only | text; log in logs/sanity-effects.log | - | - | L+G |
| Mods list | Skip intro | `mod_skip_intro` | On/Off | `start_game` :544, `hle_set_skip_intro` | **Launcher only** (hidden in game) | L |
| Mods list | Title video ("none" without res/video/title.mpg) | `mod_title_video` | On/Off | `video_on()` titlevideo.cpp:224, at the title texture's decode | At once at the next title screen (inferred) | L+G |

### 2.5 Choose Character (ModsPage 3) and the roster (`build_characters`, :1057)

- **Choose Character** shows two cards, `player_card_rml` (:1021). Left and right move between the cards (:3082). A opens
  that player's roster. The cards show the portrait, name, form, meters and which stats apply.
- **The roster (Screen::Characters)** has 35 frames (`roster_tiles`, :718): Default, the 12 characters, each followed
  by their story forms, then the "others" (Maid, Maid's spirit, Legrasse, Servant, Footman) at the end (`kForms`, :615,
  22 forms). There are 4 frames to a line and 3 lines in view (:591-592). The right stick, Page Up/Down or the mouse
  wheel scrolls it. There is a details panel (`character_details_rml`, :791).
  - A on a frame sets the character and form, sets stats to 1 ("their own") and saves at once (:1069-1076).
  - Then come a **Stats** row (Vanilla / Chosen character's, `mod_pN_stats`) and **Back**.
- Portraits come from the player's disc. `ensure_portraits` (:967) writes `UMnMenu.tpl` images 50-61 as PNGs into
  `res/ui/portraits/`, which git ignores. The form portraits are tracked art (`res/ui/portraits/*-N.png`).

### 2.6 Save slots (launcher: Play; in game: States)

| Screen | Rows | Code | Where |
|---|---|---|---|
| Load game (Play) | used slots only, 10 a page (LB/RB page) | `build_play` :1536 | L |
| Save states (States) | all 100 slots, "empty" or the file time | `build_states` :1508; an empty slot saves at once (`save_slot` → `quickstate_request_save`) | G |
| Slot N | Save here (from States, asks first), Load, Duplicate (to the first free slot), Delete (asks first), Back | `build_slot` :1582, `build_slot_delete` :1627, `build_slot_overwrite` :1657 | L+G |

F1-F10 also save to slots 1-10 (comment at :358, quickstate.c).

### 2.7 In-game root (`build_in_game`, :2421)

Resume · Save states · Options · Mods · Return to title screen (loads `quickstate_boot_path()`, :2448) · Return to
launcher (the launcher over the paused game, or a restart without a boot state, :2460) · Quit.

### 2.8 Settings with no row of their own

| Field | Why there is no row |
|---|---|
| `audio_buffer_ms` (60) | "not a setting" (settings.h); `dol_aurora_set_audio_buffer_ms` |
| `mod_stats`, `mod_health`, `mod_magick`, `mod_sanity` | read and saved, but **no reader and no row** (grep finds none outside settings.c) |
| `hdr_peak`, `hdr_paper`, `hdr_black` | set by Calibrate HDR |
| `controls_custom`, `mod_p2_custom`, `mod_p2_layout`, `controls_layout` | set by Controls and Mapping |
| `mod_p1_costume`, `mod_p2_costume` | set by the roster |
| `disc` | set by the Disc row and `start_game` |

The game's own Options already sets one launcher setting. PAL's widescreen toggle (`0x80116CE8`, its menu
`0x80116470`, found by `find_widescreen_option`, hle.c:6200-6224) writes `aspect`, applies it and saves it
(hle.c:6253-6258). This is a working precedent for a native control that changes a launcher setting.

## 3. The in-game menu today

### 3.1 How it opens

`frontend_frame()` (frontend.cpp:4302) runs once per shown frame:

| Input | Result | Code |
|---|---|---|
| Esc (key down) | menu at once | :4356-4357 |
| controller Back/Select, a tap (on release) | menu | :4331-4344 |
| Back held 0.5 s, or P | photo mode (`photo::frame`) | :4337, :4345-4350 |
| `g_open_characters` (from the Tome) | menu straight on player 1's roster | :4380-4392 |
| `ED_MENU_TEST=after_s,hold_ms[,every_s]` | testing, opens and closes by itself | :4360-4377 |

It is not called during player 2's Tome copy turn (`if (!g_tome_copy_turn) frontend_frame();`, hle.c:5511), because
memory then holds the copy, which a save state would take for the game.

When it opens it sets `g_in_game = true`, `g_screen = InGame`, `g_options_return = InGame`, then calls
`report_pause(true)` (report.c:420, which tells the hang watchdog), `hle_coop_ui_over(true)` (hle.c:13897) and
`run_menu()` (:3032).

### 3.2 How the game is paused (the threads)

- **Threaded GX, the normal build.** The game thread runs guest code and queues GX work. The main thread replays it.
  After each `Present` it calls `g_frame_hook` (aurora_backend.cpp:2330), which main.c:903 sets to
  `hle_frame_boundary`, which calls `frontend_frame`. So **the menu runs on the main thread.**
  - While `run_menu` loops, `g_presents_done` is not moved on (aurora_backend.cpp:2335 comes after the hook). The game
    thread's next `present()` waits on `g_cv_game` (aurora_backend.cpp:1635-1640: "Never more than one frame ahead").
    The game stops, at most one frame later.
  - Each menu frame is `dol_aurora_ui_frame()` → `aurora_backend_present()` (aurora_backend.cpp:3057): the last display
    copy and the UI overlay, with events pumped.
- **Unthreaded.** `hle_frame_boundary` is called at the VI address on the game thread itself (hle.c:5693), which blocks
  in the menu.
- **Closing.** `run_menu` hides the document, then waits until every button and the game's keys are let go
  (`any_held`, :212, :3139). The game never sees the press that closed it.
- **Over a co-op Tome.** `hle_coop_ui_over(true)` makes the frames plain ones (`aurora_split_menu(-1,0)`,
  `aurora_split_only_view(-1)`). Otherwise nothing of the menu showed. `false` puts the Tome's views back
  (hle.c:13897-13920).
- **Sound.** The menu doesn't mute it (photo mode does, :3856). What the sound does while the game thread waits was not
  checked (see Open questions).

### 3.3 How it draws

- **RmlUi** draws all the pages. Aurora's context (`aurora::rmlui::get_context()`) loads one document
  (`ensure_ui`, :2928). RmlUi has five fonts loaded: FiraSans Regular and Bold, IM Fell English Regular and Italic, IM
  Fell English SC. The style sheet uses only "IM FELL English" (menu.rcss:17) and "IM FELL English SC" (:837).
- `render()` (:2576) writes the heading, `menu0` and `menu1` (the left and right pages of a spread), the note and the
  hint. It sets body classes:

  | Class | When |
  |---|---|
  | `launcher` | the launcher's picture behind |
  | `pause` | InGame: parchment `pause.png`, the rows centred |
  | `overlay` | over the dimmed paused frame, `#000000b0` |
  | `settings` | the value column |
  | `dense` | Controls and Mapping |
  | `roster` | the character roster |
  | `cards` | Choose Character |
  | `spread` | two pages |
  | `centered` | slots and confirmations |

  The chosen row is "the game's own glowing frame (its memory card screen's)": a blue glow (menu.rcss:253).
- The RmlUi context "covers only the game picture" (frontend.cpp:3285-3287). In 4:3 and letterboxed scenes it sits
  inside the bars. In split screen it covers both halves, so there is no per-half page today.
- **ImGui** (Aurora's, `GetForegroundDrawList` of the main viewport, the whole window) draws the overlays that must
  reach the screen's edges:
  - the FPS counter (:3400)
  - the toasts: one at the top right, and one per co-op half that lasts 5 s (:3298, :3381)
  - the Skip prompt with its button glyph (:3445)
  - the HDR calibration caption (:3506)
  - the photo mode legend (:3752)
  - the Tome button (:3325)
  - the start-up cover (:4285)

### 3.4 Which controller drives it

- **Controller:** `gamepad()` (:89) opens `SDL_GetGamepads()[0]`, the first connected, whatever `pad_slot` says. The
  user confirmed only player 1 opens it (docs/coop-todo.md:236-241). Inferred: if player 1 chose controller 2 in
  `pad_slot`, the menu still listens to the first.
- **Buttons** (`read_buttons`, :103):

  | Menu button | Controller | Keys (RmlUi key events, `KeyListener` :131) |
  |---|---|---|
  | Up, Down, Left, Right | D-pad or left stick past 0.5 | arrows or WASD |
  | Accept | South (A) | Enter, Space, J |
  | Back | East (B) | Backspace, K, Escape (also Menu) |
  | Menu | Back/Select | - |
  | Page left / right | LB / RB | Q / E |
  | Scroll | right stick | Page Up / Page Down |
  | Costume (`kCostume`) | RB | C. Read but used nowhere |

  Directions repeat after 380 ms, then every 90 ms (:197-200).
- **Mouse:** moving selects a row and a click activates it; the wheel scrolls the roster (`MenuListener`, :2854). The
  pointer hides after 2 s still (`update_cursor`, :2961).

### 3.5 The Tome's "Choose Character" today (hle.c:16892-16950, frontend.cpp:3318-3379, :4256-4267)

1. **Options entered.** `tome_options_page` is the intercept at `0x80101600`, the Options tab's entry, installed with
   tome copy mode 2 (hle.c:33021-33024). It notes `g_tome_on_options` outside the copy turn. Journal's entry
   `0x80106FD4` and the Inventory, Map and Magick entries (`0x80102810`, `0x80111B40`, `0x8010AAF0`) clear it
   (hle.c:16903-16911, :33025-33030).
2. **Each pad read.** On the game thread (hle.c:4488), `tome_options_button` checks that player 1's Tome is up
   (`hle_coop_player1_tome_up()` or mode word r13-26240 == 3) and on Options.
   - In co-op side by side, with player 2 present, it calls
     `frontend_tome_button("Y   Choose Character", 0, 0.54f, 0.405f)`. That is player 1's half: x 0.54 of the half's
     width, y 0.405 of the height, under the cross of four (Load/Save at 0.54, 0.36).
   - Otherwise it shows the toast "Y: Choose Character" every 45 frames.
   - A Y press (`0x0800`) within 4 frames calls `frontend_open_characters()`. Y is then cleared from player 1's pad, so
     the page never sees it.
3. **`frontend_tome_button`** (any thread) stores the text, half, x, y and a time under `g_toast_mutex`.
   `draw_tome_button` (main thread, inside `update_fps_counter` → `draw_toast`) draws it only while that time is under
   400 ms old.
   - The plaque is parchment `#c9b386` with a doubled dark outline and dark lettering in `g_tome_font` (IM Fell English
     Regular, baked at 64 px in `frontend_imgui_init`, :3153). The text size is the height × 0.026.
   - It logs "[frontend] tome button: drawn N, skipped M" every 2 s.
4. **`frontend_open_characters`** sets `std::atomic<bool> g_open_characters`. The next `frontend_frame` exchanges it and
   opens the menu on `Screen::Characters` for player 0, at the chosen frame.

How the threads talk, in all: from the game thread to the UI there are two mutex-guarded message slots (toast, Tome
button) and one atomic flag. From the UI to the game there are `g_settings` fields, read live by hle.c and camera.c
(no lock), the `hle_*` calls (cheats, sanity, skip intro) and `quickstate_request_*`. The game thread is blocked while
the menu is up, so the live reads don't race the menu's writes there. Inferred: `settings_apply_graphics` is documented
as safe from the game thread (settings.h).

### 3.6 Problems found by reading (inferred, each needs a run)

1. **Back from the Tome's roster does not reach the Tome.**
   - `go_back` on Characters goes to `Screen::ModsPage` at `g_characters_return_row` (:2836-2840), which is whatever
     `g_mods_section` was (1, Co-op, at start).
   - Back again calls `mods_page_back`, which goes to `g_mods_page_return` (start: Mods; Launcher if Choose Character
     was used on the launcher this run).
   - Then Mods goes to `g_mods_return`, which starts as `Screen::Launcher` (:560) and is set to Launcher by the
     launcher's own pages (:1428).
   - `frontend_frame` resets `g_options_return` only (:4385). So with B, B, B the player can land on the **Launcher
     screen drawn over the running game**. From there, Main menu or Continue load a state in place.
   - The Menu button closes the menu only on InGame (:3105).
   - The user's next ask says "Back from the roster straight to the Tome" (docs/coop-todo.md:258).
2. **A character chosen from the Tome isn't played until play resumes.** `characters_reload` waits for mode 2 and 60
   quiet frames (hle.c:32401-32425), so the reload comes after the Tome closes. This is fine, but the roster's note
   doesn't say it.
3. **The roster's overlay depends on `g_options_return == InGame`** (:2726-2731). That holds because `frontend_frame`
   sets it.
4. **Co-op turned on or off in the in-game menu does not change the separate rooms until a load from the launcher or a
   restart** (`rooms_follow_coop` runs only in `start_game` and main.c). Nothing on the row says so.

## 4. Per player or shared

| Setting | Player 1 | Player 2 | Notes |
|---|---|---|---|
| Character | `mod_p1_character` | `mod_p2_character` | 0-12, the same code |
| Form | `mod_p1_costume` | `mod_p2_costume` | 4 bits per character |
| Stats choice | `mod_p1_stats` | `mod_p2_stats` | 0 the chapter's, 1 their own |
| Camera | `camera` (0 Original, 2 third, 3 first) | `mod_p2_camera` (0 third, 1 first, 2 Original) | **different codes** |
| What they play with | `controls_device` (3 ways) | `mod_p2_input` (2 ways) | keyboard lent to player 2 in "Controller and keyboard" (controls.cpp:816) |
| Which controller | `pad_slot[0]` | `pad_slot[1]` | automatic: player 2 takes the next one (controls.cpp:162-170) |
| Controller type | `pad_type[0]` | `pad_type[1]` | |
| Layout | `controls_layout` | `mod_p2_layout` | |
| Custom bindings | `controls_custom` | `mod_p2_custom` | |
| Hold or toggle, deadzone | `sprint/creep/target_toggle[layout]`, `stick_deadzone[layout]` | **the same arrays**, indexed by player 2's layout (controls.cpp:1349) | Shared by layout. Only player 1's Controls page edits them (:2296-2300: "player 1's for now"). Player 1's reads use `g_layout` = player 1's layout (controls.cpp:132) |
| Toasts | `g_toast_player[0]` | `g_toast_player[1]` | per half |
| Button glyphs in prompts | `glyph_set()` is player 1's (controls.cpp:963) | - | shared |
| Everything else | shared: display, sound, graphics, vibration, game text, co-op on/split, creatures, sanity effects, cheats (Full stats applies to each player; All magick is the one shared magick record), title video, skip intro, disc, save slots | | |

The user's rule (docs/coop-todo.md:247-248): player 2's own menu shows "only what's theirs (character, camera,
controller), never shared settings".

## 5. Fonts and art for a game-looking style

| Asset | What it is | Used by now | Notes for a native look |
|---|---|---|---|
| `res/fonts/game/tome-0.png` (2048×2048) + `tome-0.cells` | the game's plain sheet (UFonts.tpl sheet 0) redrawn 4x in **IM Fell English Regular**, white with a black outline | the renderer's texture override, when Game text = Tome (fonts.cpp) | character c sits at column c>>4, row 15-(c&15), stored upside down, cells 128 px (tools/ui/make_game_fonts.py:4-14). Each glyph is fitted to the original's ink box, so the spacing needs the game's own widths (inferred: those widths are on the disc, not in the PNG) |
| `res/fonts/game/tome-1.png` + `.cells` | the handwritten sheet in **La Belle Aurore** (white); its first two columns are the GameCube button icons, taken from the game's sheet at run time and redrawn for the player's buttons (`kIconCells`, fonts.cpp:146) | the same | the Tome's handwriting |
| `res/fonts/game/sharp-0/1.png` | Arimo / Reenie Beanie versions | Game text = Sharp | |
| `res/fonts/game/signatures.txt` | the two sheets' alpha signatures | `fonts_apply` | |
| `res/fonts/IMFellEnglish-Regular.ttf`, `-Italic`, `IMFellEnglishSC-Regular.ttf` | old-book faces | RmlUi (all pages); `g_tome_font` in ImGui | the same face as `tome-0`, so ImGui text in `g_tome_font` already matches Game text = Tome, though without the sheet's outline, and not matching Sharp or Original |
| `res/fonts/LaBelleAurore.ttf` | handwriting | `tome.cpp` (stb_truetype) | not loaded into RmlUi or ImGui |
| `res/fonts/AlegreyaSC-*.ttf` | small caps | **nothing loads it** (grep) | spare |
| `res/fonts/FiraSans-*.ttf` | sans | `g_fps_font` in ImGui (FPS counter, toasts, Skip prompt, HDR caption, photo legend); loaded into RmlUi but unused by the style sheet | not game-like |
| `res/ui/tome/` (tracked, made by `tools/ui/make_tome_art.py`) | `parchment.png` 1024², `leather.png` 1024×640, `mark.png` 32² (the selection mark), `rule.png` 240×24, `divider.png` 600×36, `leader.png`, `leader-red.png` | RmlUi decorators (roster panel, cards, hint bar, gaps) | procedural, no game art |
| `res/ui/pause.png` 706×578, `launcher.png` 1920×1072, `emblem.png` 600² | the parchment sheet, the launcher's picture, the game's emblem | RmlUi | |
| `res/ui/glyphs/{xbox,ps,switch}/` (26 each), `kb/` (117) | Kenney CC0 button and key icons | prompts (`buttons.cpp`), Skip prompt, photo and HDR legends (ImGui through `aurora_imgui_add_texture`, :3483) | |
| `res/ui/glyphs/ink/<set>/` | the same icons as ink on parchment (`tools/ui/make_ink_glyphs.py`) | the menus' button pictures (RmlUi) | best for a parchment page |
| `res/ui/portraits/` | the disc's chapter portraits (made at run time) and the forms' (tracked) | roster, cards | |

The game's own pages are already redrawn by texture override:

- **`tome.cpp`** redraws the Tome Options tab's **Controller page** texture (432×480 CMPR, hashes `0x8680aee4cb0d0a97`
  and `0x7ba7ca0b431bba6a`, UBookPak.bin image 31). It writes a list of the player's controls in La Belle Aurore onto
  the page's own parchment.
- **`glyph_art.cpp`** redraws button pictures painted into textures (the map's "Y Button: Up Floor", "Press A", quick
  spell slots, organ sheets).
- **`buttons.cpp`** redraws the 512×64 prompt sheet.
- **`titlevideo.cpp`** replaces the title picture (850×480). It knows the title words (Start Game, Load Game, Options)
  and the Options screen picture (the X-ray skull, hash `0xbefc5ef6c1629595`, titlevideo.cpp:179-182).

Drawing into the game's own textures like these is the most native-looking way, because the text then lies on the
game's page under the game's own lighting and fades. ImGui and RmlUi draw on top. Inferred.

Game addresses near this work (not traced here):

| Address | What it is |
|---|---|
| `0x80101980` | the Tome Options page (`r13-24304`, docs/game/systems/tome.md) |
| `0x80101600` | the Options tab's entry (hooked) |
| `0x80116470` | PAL's options menu function (notify hooked) |
| `0x80116CE8` | its widescreen toggle |
| `0x80111078` / `0x80111234` | the options book menu's open / frame (hle.c:6160-6163) |

## 6. What this means for the native menus (inferred, for the design)

- **The engine to reuse.** Every setting has one row builder and one apply path. A native page can call the same `Row`
  lambdas, or the same `g_settings` field and apply call: `changed()`, `enhancement::change`, `controls::set_layout`.
  It doesn't need new logic. `ED_MENU_OPEN=<page>[,row]` (frontend.cpp:3208) opens any page at start for pictures.
- **What a title-menu Options entry needs to hide or explain.** Hide: Disc, Skip intro, Load game. Explain: MSAA and
  texture filtering (next start), separate rooms (next start), characters (next load).
- **Player 2's own page (from their copy Tome).** It needs a half-sized surface: RmlUi covers the whole picture today,
  and ImGui can be clipped to a half as `draw_tome_button` does. It needs player 2's controller as input, but
  `gamepad()` is player 1's. It needs only these fields: `mod_p2_character`, `mod_p2_costume`, `mod_p2_stats`,
  `mod_p2_camera`, `mod_p2_input`, `pad_slot[1]`, `pad_type[1]`, `mod_p2_layout`, `mod_p2_custom`.

## Open questions

1. Is the B, B, B path from the Tome's roster really ending on the Launcher screen over the game (section 3.6.1)? A run
   with `ED_TOME_HINT_ANY=1`, Y on the Tome's Options, then Back three times, would show it.
2. What does the sound do while the menu holds the game thread (silence, a held note, a loop)? Not read.
3. Do `game_font` and `sharp_shadows` really swap the sheets and light maps already loaded, at once, in game? Read as
   yes from the generation counters, not seen.
4. Does turning co-op on in the in-game menu, without a reload, play correctly with the solo memory layout (no separate
   rooms)? Only a run can say.
5. Is `pad_slot[0]` set to controller 2 meant to move the menus to that controller too? Today it doesn't (gamepad() is
   the first connected). This is for the user to decide.
6. For the native label: draw from `tome-0.png` with the game's glyph widths (where are they in RAM or on the disc?),
   or use `g_tome_font` (the same face, no outline)? And should it follow the Game text setting (Sharp, Original)? This
   is for the user and a measurement.
7. Should `mod_enemies` (no effect) and the orphans `mod_stats`, `mod_health`, `mod_magick` and `mod_sanity` be shown
   in a native menu at all? This is for the user.
8. Should player 2's hold or toggle and deadzone become their own? Today they are shared by layout and edited only from
   player 1's page. This is for the user.
9. Should cheats and Sanity Effects (testing tools) be on Tome pages, or stay behind the hidden fallback menu
   (docs/coop-todo.md:247)? This is for the user.
