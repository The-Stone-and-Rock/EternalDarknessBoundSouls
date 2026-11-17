# Two new looks for the launcher's pages: the title skin and the Tome skin

Read-only research, 2026-11-17. Nothing was built or run. Area: `runtime/host/frontend.cpp` (RmlUi) and `res/ui`.

The user's ask, in short: the title's Options screen lists all of our options in a list that scrolls, in the title's own
look (skull picture, hand lettering). Each entry opens its page on the same background. In game, the Tome's plaques open
the same pages, but drawn on the Tome's page, not the launcher's parchment. Every page has Back, and the B button / Esc
works too. The title's main menu gets "Choose Character" above Options, which opens the launcher's Choose Character page.

This file maps how a page is built and drawn today, and says what to add so one row model can be drawn three ways:
the launcher (today), **title**, and **tome(player, rect)**.

Tags: **[read]** = read in the file and line named. **[inferred]** = not checked in a run or in the source of a library.
Doc 01 to 07 in this folder are not repeated. Line numbers are for the commit on `tools/game-kit` at 78dc1e9.

---

## 1. The short answer

- **One document, one row list, many classes.** `res/ui/menu.rml` is one document. C++ rebuilds the rows as markup
  (`render()`) and sets classes on `<body>`. A skin is two more body classes (`skin-title`, `skin-tome`) and a block of
  rcss for each. No second document is needed. [read frontend.cpp:2646-2802, menu.rml:1-23]
- **The row model needs no change.** `Row` (frontend.cpp:325-344) already says what each row is (toggle, choice, slider,
  page that opens, key capture, portrait tile, player card). Only the drawing differs.
- **The page can be drawn with no background.** RmlUi is drawn last, over the whole window, with premultiplied alpha
  (aurora.cpp:1020-1027). With `body { background-color: transparent; decorator: none; }` the frozen game frame shows
  through. Today `body.overlay` puts a dark veil `#000000b0` there (menu.rcss:27-30). [read]
- **Do not use `backdrop-filter`.** Any visible backdrop filter turns RmlUi into "not an overlay"
  (rmlui.cpp:443-469). Then RmlUi's own render target replaces the game picture (aurora.cpp:905-906, 1009-1018). [read]
- **The Tome page's place on screen can be worked out in the host with no renderer change.**
  `dol_aurora_get_display_info(&aspect, &w, &h)` gives the window size in pixels and the picture's shape
  (aurora_backend.cpp, the function after `dol_aurora_ui_frame`). RmlUi's context is the same window size
  (rmlui.cpp:56-60 "the whole window"). The picture's place is `calculate_present_viewport` (webgpu/gpu.cpp:338-363),
  ten lines to copy. The halves are `half_of` (aurora.cpp:756-765). [read]
- **One gap is in the renderer, not the frontend.** Over a co-op Tome, `hle_coop_ui_over(true)` turns the split menu off
  (`aurora_split_menu(-1, 0)`, hle.c:13896-13903). The frozen picture is then shown whole, not in halves
  [inferred from aurora.cpp:934-1018]. The Tome skin needs the halves kept. See section 7.
- **Scrolling must stay done by hand.** RmlUi's `overflow: auto` scrolled about 0.89 of its value and cut the last line
  (docs/coop-next-session.md:697). The roster does it by drawing only the lines in view (`roster_keep_in_view`,
  frontend.cpp:2588-2597). The same idea, per row, gives every list a scroll. [read]

---

## 2. How a page is built today

### 2.1 The Screen enum and the build functions

`enum class Screen` (frontend.cpp:322-323) [read]:

| Screen | Built by | What it is |
|---|---|---|
| Launcher | `build_launcher` :1418 | Continue, Main menu, Load game, Choose Character, Co-op, Options, Mods, Quit |
| Play | `build_play` :1565 | Load game: used slots, 10 to a page |
| States | `build_states` :1537 | Save states: all 100 slots, 10 to a page |
| Slot, SlotDelete, SlotOverwrite | :1611, :1656, :1686 | One slot's actions; the two "asked first" pages |
| Options | `build_options` :2269 | Display, Sound, Graphics, Controls (+ disc; + Mods/Quit or Return to title/Quit when opened from the game's menus) |
| OptionsPage | `build_options_page` :2210 | One section of `build_options_rows` (:1903): 1 Display, 2 Sound, 3 Image, 5 Game, 7 Shadows, 8 Lighting, 9 Effects |
| Graphics | `build_graphics` :2244 | Image, Shadows, Lighting, Effects |
| Controls | `build_controls` :2353 | Device, controller, type, layout, hold/toggle, deadzone, vibration, Button mapping |
| Mapping | `build_mapping` :2426 | One row per action (and per direction key on the right page) |
| InGame | `build_in_game` :2491 | The debug pause menu |
| Problem | `build_problem` :2563 | A fault page |
| Mods | `build_mods` :1327 | Co-op, Challenge, (Characters in game), Cheats, Sanity Effects, Skip intro, Title video |
| ModsPage | `build_mods_page` :1381 | One section of `build_mods_rows` (:1147): 1 Co-op, 2 Challenge, 3 Characters (the two cards), 4 Cheats, 5 Sanity Effects |
| Characters | `build_characters` :1086 | The roster: every character and form, then Stats and Back |

A section page is made by building the long list and keeping one section: `section_number` reads the Roman numeral at
the start of `Row.section` (:2201-2207). [read]

### 2.2 The Row struct and every kind of row

`struct Row` (frontend.cpp:325-344) [read]:

| Field | Meaning |
|---|---|
| `label` | the name |
| `value` | the value as text; empty for an action |
| `value_rml` | markup instead of `value` (button pictures, a portrait) |
| `enabled`, `browse` | greyed; `browse` lets Left/Right work while greyed |
| `note` | help under the list while chosen |
| `change(int)` | Left/Right (-1/+1); null: no value |
| `activate()` | A or click; null: `change(+1)` (:2855-2867) |
| `section`, `gap` | a heading before it; a space (or ornament) before it |
| `tile`, `chosen` | a roster portrait frame; the choice in force |
| `card` | 1 or 2: a player's card on Choose Character |
| `big` | the launcher's large Main menu row |
| `page` | 1: the right-hand column of a two-column page |
| `toggle` | On/Off drawn as a switch |
| `slider` | 0-100 drawn as a bar |
| `experimental` | the "(Experimental, expect glitches)" tag |

The kinds, as `render()` sorts them (:2754-2758) [read]:

| Kind | How it is made | How it is drawn (`row_control`, :2624-2644) |
|---|---|---|
| Toggle | `toggle = true`, value "On"/"Off", `change` flips | `.switch` with `.knob` |
| Choice | `change` set, value text | `‹ value ›` (`.chev.prev`, `.pick`, `.chev.next`) |
| Slider | `slider` 0-100 (the volumes :2037, :2045; haze :2063) | `‹ bar value ›` |
| Submenu (opens a page) | `activate` set, no `change`; class `opens` on settings pages | `›` at the right |
| Action | `activate` only, not on a settings page | the label alone |
| Key capture | Mapping rows: `value_rml` = glyphs, `activate` = `controls::begin_capture(a)`, `change` steps the binding (:2443-2479) | glyphs; while capturing, `run_menu` hands keys to `controls::poll_capture` (:3124-3137) and `KeyListener` turns RmlUi keys into scancodes (:158-171) |
| Roster tile | `tile = true`, `value_rml` = portrait `<img>`, `chosen` (:1093-1107) | `.tile` in a `.roster` grid, the lines in view only (:2700-2721) |
| Player card | `card = 1/2` (:1217-1240) | `player_card_rml` (:1050-1082) inside `.pcards` |
| Save slot | "Slot N", value = time, `change = slots_jump` (:1537-1563, :1565-1591) | ten to a page (`kSlotsShown`, :384), LB/RB turn the page (:3179-3182) |
| Mods rows | built in `build_mods_rows` sections I-V (:1147-1324) | as any setting |

### 2.3 Moving, choosing, going back

- `run_menu` (:3113-3226) polls the pad and the keys each frame. Up/Down call `move_selection` (:2846-2853), which skips
  rows that are greyed and have no `change`. On Characters the arrows move in the grid (`move_in_roster`, :1121-1145).
  On the cards page Left/Right jump between the two cards (:3163-3170). A is `activate_selected`; B (or Esc) is
  `go_back` (:2869-2932). After any press, `build_rows()` rebuilds and redraws (:3210-3211). [read]
- **`go_back`** has one case per screen. Each page remembers where to return: `g_options_return`, `g_mods_return`,
  `g_mods_page_return(_row)`, `g_controls_return(_row)`, `g_characters_return_row`, `g_graphics_row`,
  `g_options_row` (:583-601, :2188-2199). [read]
- **`g_root_closes`, `g_root`** (:599-600): set when the game's own menus opened the page. `build_rows` closes the menu as
  soon as the screen goes "past the root": to InGame or Launcher, or to ModsPage when the root is Characters
  (`past_root`, :2804-2815). This is what makes Back from a Tome page land on the Tome again. [read]
- **`g_from_title`** (:601): set for `FRONTEND_PAGE_TITLE_SETTINGS`. It adds Mods to Options and names the heading
  "More Options" (:2312-2339, :2656). [read]
- The mouse: `MenuListener` (:2935-2981) finds the element whose id starts with `row`, and uses the number after it as the
  index into `g_rows`. So hidden rows (not drawn) do not break the mouse, as long as ids stay `row<index>`. The wheel
  scrolls the roster only (:2938-2946). [read]

### 2.4 Opening from the game's menus

`frontend_open_page(page, player)` (:4339-4344) stores two atomics. The next `frontend_frame` (:4389-4522) takes them:
[read]

- sets `g_in_game = true`, `g_screen = InGame`, `g_options_return = g_mods_return = InGame` (:4475-4478) - so the
  title's page is also "over the game" and gets `body.overlay` (:2792-2801);
- `g_root_closes = page >= 0`, `g_from_title = page == FRONTEND_PAGE_TITLE_SETTINGS`, `g_menu_player = player`
  (:4480-4489);
- picks the root screen: CHARACTERS → Characters (that player's), SETTINGS / TITLE_SETTINGS → Options, MODS → Mods,
  STATES → States (:4490-4510);
- `report_pause(true)`, `hle_coop_ui_over(true)`, `run_menu()`, then both undone (:4512-4520).

Callers: the Tome stops (`tome_stops_choose`, hle.c:17086-17105; stops tables hle.c:16964-16973) and the title's ninth
row (`title_more_choose`, hle.c:17177-17182). Page numbers: frontend.h:49-55. [read]

### 2.5 Whose pad, whose page

- `g_menu_player` (:105) is the player whose page it is. `menu_gamepad()` (:107-124) returns player 2's controller when
  `g_menu_player == 1` and they have one (by `pad_slot[1]`, or "the next one player 1 is not using"); otherwise the first
  controller. `read_buttons` reads only that pad (:126-149). [read]
- The keyboard is not per player in the menu: `KeyListener` (:154-202) takes every RmlUi key event. [read]
- `controls::set_menu_player` is a different thing: whose Controls page is shown (:1171, :2293). [read]

### 2.6 How the game is paused and what is behind

- The menu loop runs on the main thread. The game thread stops at its next present, at most one frame later (doc 01
  §3.2, doc 05 claim 17). Any page pauses both players. [read in those docs]
- Each menu frame is `dol_aurora_ui_frame()` → `aurora_backend_present()` (aurora_backend.cpp, `dol_aurora_ui_frame`):
  "the last display copy plus the UI overlay" (aurora_backend.h:137-139). So the frozen frame is the game's last picture:
  the title's Options screen, or the Tome's Options page. [read]
- RmlUi is recorded over the whole window (`uiViewport`, aurora.cpp:657-666) and laid over the picture with the
  premultiplied pipeline (aurora.cpp:1020-1027). [read]

---

## 3. How the document is loaded and styled

- `ensure_ui` (:3009-3038): loads five fonts (Fira Sans Regular/Bold, IM Fell English Regular/Italic, IM Fell English SC),
  then `menu.rml`; adds the key listener to the context root and click / mousemove / wheel listeners to `#book`. [read]
- **La Belle Aurore is not loaded into RmlUi.** It is used only by `tome.cpp` through stb_truetype (`hand()`,
  tome.cpp:220-240; `tome_hand_cover`, tome.cpp:579). Its family name in the file is "La Belle Aurore" (name table read).
  It has every special letter the menus use: `·` `‹` `›` `×` `—` and the no-break space (cmap read). One more
  `Rml::LoadFontFace(res + "/fonts/LaBelleAurore.ttf")` in the loop at :3019-3023 makes it usable as
  `font-family: "La Belle Aurore"`. [read; the RmlUi call inferred to work as for the others]
- `menu.rml` (23 lines): `#book` holds `#heading`, `#pages` (two `.page`s: `#menu0`, `#menu1`), and `.note#note`;
  `#hint` sits outside the book. [read]
- `menu.rcss` (940 lines). All sizes are `vh` (window height) or `dp`. The body classes set by `render()` (:2776-2801):
  `spread`, `launcher`, `pause`, `centered`, `dense`, `settings`, `roster`, `cards`, `overlay`. [read]
- **The background picture** is a decorator on `body`: `image(launcher.png cover)` dimmed by `image-color: #5c5c5c`
  (menu.rcss:22-23); full brightness on the launcher (:626-629); none over the game (:27-30). The sheet is `#book`'s
  decorator `image(pause.png fill)` (:39). Images load through Aurora's `LoadTexture` (WebGPURenderInterface.cpp:410-447):
  a PNG by path, **or** an in-memory picture from a registered provider. [read]
- **The in-memory provider** (aurora/rmlui.hpp:33-43, RuntimeTextureProvider.cpp:27-77):
  `aurora::rmlui::register_texture_provider("ed", fn)` makes any source `ed://name` call `fn("ed://name")`, which returns
  RGBA8 width × height. Nothing in the project uses it yet (grep). Whether RmlUi passes `ed://…` through its path join
  unchanged was not checked (the RmlUi sources are not in the tree) [inferred: its default join keeps a path whose `:`
  comes before the first `/`]. The proven route is a PNG file, as `ensure_portraits` does (:996-1031, `write_png` :866).

---

## 4. The output size, the picture, the halves, the Tome page

All in window pixels, the units of RmlUi's `px` here (context = `native_fb_width × native_fb_height`,
rmlui.cpp:56-60, :437-441). [read]

1. **The game picture in the window**: `calculate_present_viewport(W, H, cw, ch)` (gpu.cpp:338-363): as wide as the window,
   or as tall, keeping the picture's shape, centred. The shape is `aspect` from `dol_aurora_get_display_info`. [read]
2. **A co-op half**: `half_of(player, layout)` (aurora.cpp:756-765): side by side (layout 0) the half is `W/2 × H` at
   `x = W/2 * player`; one above the other (layout 1) `W × H/2` at `y = H/2 * player`. Layout is `g_settings.mod_split`
   (frontend.cpp:3434 uses the same). [read]
3. **A menu in a half** (the Tome in co-op): the whole picture, fitted: `scale = min(half.w / vp.w, half.h / vp.h)`,
   centred in the half (aurora.cpp:991-1001). At 1920×1080, 16:9, side by side: the Tome is 960×540 in the middle of the
   left or right half. One above the other: 960×540 in the middle of the top or bottom half. [read; numbers computed]
4. **The Tome's page in the picture**: x 144..575 of the 640 menu units, y 0..479, in both book layouts (doc 05 claim 11).
   As parts of the picture: 0.225..0.900 in 4:3; about 0.294..0.800 in the wide layout, where the 640 units sit in
   -106..745 (doc 05 §3, [inferred]). Whether the game uses the wide layout follows the launcher's aspect
   (`sync_widescreen_option`, doc 05 claim 16). 21:9 was not looked at.

So at 1920×1080, 16:9: solo page ≈ x 564..1536, 972×1080. Co-op side by side, player 1: ≈ x 282..768, y 270..810,
486×540; player 2: add 960 to x. [computed from the above; inferred until seen in a picture]

A sketch for frontend.cpp (one function, no renderer change):

```cpp
struct PageRect { float x, y, w, h; };
// The Tome's page in window pixels: half -1 solo, 0 or 1 that player's co-op half.
PageRect tome_page_rect(int half) {
    float aspect = 16.f / 9.f;
    uint32_t W = 0, H = 0;
    dol_aurora_get_display_info(&aspect, &W, &H);
    float vw = W, vh = std::round(W / aspect);          // gpu.cpp:344-354
    if (vh > H) { vh = H; vw = std::round(H * aspect); }
    float vx = (W - vw) / 2, vy = (H - vh) / 2;
    if (half >= 0) {                                     // aurora.cpp:756-765, :991-1001
        const bool side = g_settings.mod_split != 1;
        const float hx = side ? W * 0.5f * half : 0, hy = side ? 0 : H * 0.5f * half;
        const float hw = side ? W * 0.5f : W, hh = side ? H : H * 0.5f;
        const float s = std::min(hw / vw, hh / vh);
        vw *= s; vh *= s;
        vx = hx + (hw - vw) / 2; vy = hy + (hh - vh) / 2;
    }
    const bool wide = aspect > 1.5f;                     // the game's wide book layout [inferred]
    const float l = wide ? 250.f / 851.f : 144.f / 640.f, r = wide ? 682.f / 851.f : 576.f / 640.f;
    return {vx + vw * l, vy, vw * (r - l), vh};
}
```

Which half: the `player` given to `frontend_open_page` is whose Tome it is (`tome_stops_player`, hle.c:16988-16992), and
halves are per player (`half_of(player)`). Use `half = player` when co-op's split is showing
(`camera_split_on()`, camera.c:1560, or the Tome's own `hle_coop_tome_shares()` / `hle_coop_player1_copy_up()`,
hle.c:13677, :16415), else -1. [read; the choice of test inferred]

The page's edge has the Tome's own border painted in. Inset the rect a little (about 5% each side) so that border shows.
Tune it on a picture. [inferred]

---

## 5. What to add: skins

### 5.1 State and opening

```cpp
enum class Skin { Launcher, Title, Tome };
Skin g_skin = Skin::Launcher;
int g_skin_half = -1;  // Tome: -1 solo, else the co-op half
```

- In `frontend_frame`, after the page is taken (:4468-4511): `g_skin = page == FRONTEND_PAGE_TITLE_SETTINGS ||
  page == <a new TITLE_CHARACTERS> ? Skin::Title : page >= 0 ? Skin::Tome : Skin::Launcher;` and
  `g_skin_half = (g_skin == Skin::Tome && coop split showing) ? player : -1;`. Reset to Launcher after `run_menu`
  (next to `g_root_closes = false`, :4519). The Esc debug menu and the launcher keep today's look.
- No new public call is needed: the page number already says title or Tome. If wanted later,
  `frontend_open_page_skin(page, player, skin)` is the same two atomics plus one.
- **Choose Character on the title main menu**: a new `FRONTEND_PAGE_TITLE_CHARACTERS` (5) opening
  `Screen::ModsPage`, `g_mods_section = 3`, with `g_root = Screen::ModsPage` and `g_mods_page_return = Screen::InGame`.
  Back then goes to InGame, which is past the root, so the menu closes (`mods_page_back` :1402-1406, `past_root`
  :2804-2808). A card opens the roster; the roster's Back returns to the cards (:2917-2921). [read; the flow inferred]
  The painted row on the title's main menu and its hook are hle.c / titlevideo.cpp work (doc 02), not this file's.
- **The title's Options as one list**: the user wants our options listed where "More Options" is now. In the frontend this
  is a root list in the title skin: Display, Sound, Graphics, Controls, Co-op, Characters, Challenge, Cheats, Title video,
  Quit game, Back. Each `activate` opens the page it opens today. It can be `build_options` with `g_from_title` adding the
  Mods pages as rows (Co-op 1, Challenge 2, Cheats 4, as `build_mods` :1328-1338 makes them) instead of the one "Mods" row
  (:2313-2316). Whether the game's own eight rows (Save Settings … Language Select) join that list is for the user and
  for the title researcher: the game draws and drives them, not RmlUi.

### 5.2 render()

At the end of `render()` (:2776-2801):

```cpp
g_doc->SetClass("skin-title", g_skin == Skin::Title);
g_doc->SetClass("skin-tome", g_skin == Skin::Tome);
g_doc->SetClass("overlay", g_skin == Skin::Launcher && <today's test>);
if (g_skin == Skin::Tome) {
    const PageRect r = tome_page_rect(g_skin_half);   // each render: the window can change size
    Rml::Element* book = g_doc->GetElementById("book");
    book->SetProperty("left", std::to_string(r.x) + "px");   // (position: absolute in the rcss)
    book->SetProperty("top", std::to_string(r.y) + "px");
    book->SetProperty("width", std::to_string(r.w) + "px");
    book->SetProperty("height", std::to_string(r.h) + "px");
    book->SetProperty("font-size", std::to_string(r.h * 0.04f) + "px");
} else {
    book->RemoveProperty("left"); /* … and the others */
}
```

Then every size inside `body.skin-tome` is in `em`, so the page scales with its rect: 0.04 × 540 ≈ 22 px in a co-op half
at 1080p, about 15 rows at 1.5 em; 43 px solo. [inferred; the factor to tune on a picture]

`#hint` is outside `#book` (menu.rml:21), at the window's bottom (menu.rcss:294-302). In the Tome skin that would land
across both halves. Either move it inside `#book` (and fix the launcher's rcss), or place it with the same
`SetProperty` at the rect's bottom, or hide it. [read; choice open]

### 5.3 What must differ in the rcss

**Title skin** (`body.skin-title`), drawn over the whole window:

| Rule | Value | Why |
|---|---|---|
| `body` background | `#000` and `decorator: image(<the clean Options picture> cover)`; or transparent over the frozen title | the skull picture. See 5.4 |
| `body` font | `font-family: "La Belle Aurore"; color: rgb(72, 97, 104)` | the painted rows' hand and teal (titlevideo.cpp:304-307, :325) |
| `#book` | `position: absolute; decorator: none;` left ≈ 20%, top ≈ 17%, width ≈ 34%, height ≈ 72% of the picture | where the painted words sit: x 170..425, y 85..415 of 850×480 (seen in res/video/options.rgba) |
| `.row` | no border, no box-shadow, `justify-content: flex-start` | the game's rows are bare words |
| `.row.selected` | `color: #A0E3F1; font-effect: glow(0 6dp #A0E3F159);` | the game's lit word, tinted 0xA0E3F1 (doc 02 §3.2), and `paint_more`'s soft glow (titlevideo.cpp:333-348). `font-effect: glow` is already used (menu.rcss:840) |
| `.row.disabled` | a darker teal, e.g. `rgb(40, 55, 60)` | |
| `.switch`, `.slider`, `.chev`, `.slider-fill` | teal outlines, cyan when on/lit | today they are red ink on parchment (menu.rcss:181-249) |
| `.section` | smaller, dimmer teal | |
| `.gap` | space only, no `tome/rule.png` | |
| `.note`, `#hint` | IM Fell English, teal, small | the hand is hard to read small |
| `.tag` | teal-grey, not red | |
| roster / cards | keep their geometry (`vh`), drop `pause.png`; a dark, half-clear panel behind the frames so the skull doesn't fight them | the roster is full-window-sized already (menu.rcss:336-342) |

**Tome skin** (`body.skin-tome`), drawn in the page rect only:

| Rule | Value | Why |
|---|---|---|
| `body` | `background-color: transparent; decorator: none;` | the Tome's frame, the other half and the frozen game stay visible |
| `#book` | `position: absolute; padding: 1.5em 1.2em; box-sizing: border-box; decorator: image(tome/parchment.png fill);` plus the rect from C++ | a clean leaf over the Options cross painted on the page (the page picture is image 27, doc 03) |
| text | `color: rgb(38, 27, 17)` | the Tome's ink, `kInk` (tome.cpp:40-41, alpha 0.86) |
| font | labels "La Belle Aurore" (the painted labels' hand, tome.h); values, notes "IM FELL English" | the game's font sheets (`res/fonts/game/tome-*.png`, 2048² sheets with a `.cells` mask) are textures for the game's own text, not fonts RmlUi can load. Using them would need a custom `Rml::FontEngineInterface`. Not worth it [inferred] |
| every size | `em` | scales with the rect |
| `.row.selected` | red ink `#7a1616` and a thin gold ring, no blue glow | the Tome's chosen plaque is a gold ring, colour `0x646400FF` (doc 05 claim 7); the launcher's blue glow belongs to the memory-card screen |
| `#heading` | 1.6em, centred | |
| `.note` | 0.7em, at most two lines | small page |
| roster | see section 6 | |

The order matters: the skin rules must beat `body.overlay`, `body.settings`, `body.roster`, `body.cards` (same
specificity: put them later in the file, or write `body.skin-tome.roster …`). [inferred from CSS rules, which RmlUi
follows]

### 5.4 The title's background picture

- `res/video/options.rgba` is the game's Options picture, raw RGBA, 850×480, **stored upside down**, written the first time
  the game draws it (`keep_skull`, titlevideo.cpp:194-224, `kTitleW/H` :33). It is git-ignored (`.gitignore`
  `res/video/*`). It is the picture **without** "More Options" (kept before `paint_more`, titlevideo.cpp:549-551). [read]
- Its left side is black with the eight painted rows in teal, x ≈ 170..425, y ≈ 85..415; the logo starts at x ≈ 410 for
  y < 170 (picture converted to PNG and looked at). To use it as our background, those words must go: fill
  x 160..405, y 75..170 and x 160..445, y 170..470 with black. [seen]
- Two ways:
  1. **A file, like the portraits** (proven): read `options.rgba`, turn it upright, black out the words, `write_png` to
     `res/ui/title/options.png` (git-ignore it: it is the game's art). Then `decorator: image(title/options.png cover)`.
  2. **In memory**: `register_texture_provider("ed", …)` in `ensure_ui`, source `ed://title-options`. No file, but the
     path-join question in section 3 must be checked first.
- **Or no picture at all**: the frozen frame behind *is* the title's Options screen. A transparent body and a black panel
  over the word column would show the real skull, cropped exactly as the game crops it in 4:3. The panel's place then
  needs the same picture-to-window math as section 4 (picture x/850 across the picture, in the wide layout). This also
  hides the game's lit "More Options" behind our rows. [inferred]
- `cover` fills the window. The game picture itself is fitted (letterboxed) to its viewport. At 16:9 they match; at 4:3
  the sides are cut both ways; at 21:9 `cover` cuts top and bottom while the game shows bars. Putting the picture on a
  div sized to the viewport rect (section 4, step 1) avoids that. [inferred]
- If `options.rgba` is missing (it never is after the title's Options was drawn once), fall back to black.

---

## 6. Scrolling, and a Tome-sized character chooser

### 6.1 Today

- Only the roster scrolls: `g_roster_top` is the first line in view, `kRosterColumns = 4`, `kRosterLines = 3`
  (:620-622). `roster_keep_in_view` (:2588-2597) moves the view to the highlighted line; `roster_wheel` (:2600-2607)
  scrolls and carries the highlight. `render()` skips tiles out of view (:2704-2710). The bar is drawn from
  `g_roster_top / lines` (:2609-2620). The right stick and Page Up/Down are `kScrollUp/Down`, repeating every 160 ms
  (:146-148, :224-227). [read]
- Slot lists page by ten (`g_slots_page`, :2683-2690). Settings pages don't scroll; they are short sections, and
  `.menu` centres them (menu.rcss:61-66). [read]

### 6.2 One scroll for every list

The title skin's root list and any Tome page can be longer than the space. The same idea, per row:

```cpp
int g_list_top = 0;      // the first row in view
int g_list_lines = 0;    // rows that fit: 0 no scroll (the launcher's look)
void list_keep_in_view() {  // as roster_keep_in_view, by rows
    if (g_list_lines <= 0) return;
    if (g_selected < g_list_top) g_list_top = g_selected;
    else if (g_selected >= g_list_top + g_list_lines) g_list_top = g_selected - g_list_lines + 1;
    g_list_top = std::clamp(g_list_top, 0, std::max(int(g_rows.size()) - g_list_lines, 0));
}
```

- In `render()`'s row loop, skip rows outside `[g_list_top, g_list_top + g_list_lines)` but keep each drawn row's id
  `row<i>`, so the mouse still works (:2949-2957).
- `g_list_lines` comes from the skin: title ≈ 9-10 rows (the eight painted rows' spacing, 41-47 px of 480,
  titlevideo.cpp:306); Tome ≈ rect height / (1.5 em). A section heading or a gap takes space too: count it as half a
  row, or keep headings out of the Tome skin.
- Draw the roster's bar (`.roster-bar`, menu.rcss:380-425) beside the list when it scrolls, recoloured per skin.
- Reset `g_list_top = 0` when the screen changes (in `build_rows`, where `built` is compared, :2836-2840).
- The wheel: in `MenuListener`, scroll the list when `g_list_lines > 0` (as :2940-2944 does for the roster).

### 6.3 The roster's size today

- 35 frames: Default, 12 characters, 22 forms (`kForms`, :644-709; `roster_tiles`, :747-761) → 9 lines of 4.
  [read; counted]
- Book 150vh × 95vh (menu.rcss:336-342); grid 70vh wide (:369-376), 3 lines in 56vh (:363-367); a tile 15vh wide with a
  10.6vh portrait (:427-443); the details card beside it (`character_details_rml`, :820-850; rcss :524-621).
- The code: data and rows :615-1145, drawing :2582-2620 and :2700-2724, input :3140-3151; rcss :334-621 (roster) and
  :761-940 (cards). [read]
- Portraits: 126×89 pictures written from the disc (`ensure_portraits`, :996-1031), forms' own in `res/ui/portraits`.

### 6.4 A compact chooser for the Tome page (same data)

The co-op Tome page is about 486×540 px at 1080p (section 4). Keep `roster_tiles`, `build_characters`, `form_set`,
`chosen_character()` and the Stats/Back rows. Change only the layout numbers and the panel:

- Make `kRosterColumns` and `kRosterLines` variables set by the skin (`move_in_roster` :1121-1145,
  `roster_keep_in_view`, `roster_wheel` and the bar all read them). Tome: **3 columns, 3 lines** (12 lines in all).
- A tile ≈ 30% of the page width: ≈ 140 px wide, portrait ≈ 100 px tall (the 126:89 shape), name 0.8 em, form 0.7 em.
- Drop the side panel. Under the grid, one strip for the highlighted frame: name, form in red, year, and the three
  meters in one row (reuse `.meter` with em sizes). The long "about" text goes, or into `.note`.
- Then Stats and Back as today.
- Player 1's Tome opens player 1's roster, player 2's copy Tome player 2's (`g_characters_player = player`,
  :4491-4496). The two-card page is not needed in the Tome: each player has their own Tome.
- In the title skin the roster can keep today's full-window geometry, with the skin's colours.

---

## 7. The co-op picture behind a Tome page (renderer side)

- Today: `hle_coop_ui_over(true)` (hle.c:13896-13903) calls `aurora_split_menu(-1, 0)`, `aurora_split_only_view(-1)`,
  `split_skip_views_set(0)`. Its comment: without it "nothing of the menu was shown and the game looked frozen". [read]
- With the split menu off, the present takes the plain path: the last display copy over the whole window
  (aurora.cpp:1009-1018). In co-op that copy is the Tome's own frame, so the Tome would fill the window under the page,
  not one half [inferred: not seen in a picture]. Doc 05 §3.5 says the same: "A player 2 page needs the split kept".
- What the Tome skin needs is what `menuHalf` already draws (aurora.cpp:938-1008): the Tome's picture fitted into its
  owner's half, the partner's last view (`s_shown`/`s_held`) in the other, then the RmlUi overlay on top. That path runs
  when `split_menu_player() >= 0`, the split views are not being shown this frame, and RmlUi is an overlay (no backdrop
  filter). [read]
- So the likely change is in `hle_coop_ui_over`: keep `aurora_split_menu(owner, mod_split)` and only stop the sharing /
  view skipping that hid the menu. Which of the three calls caused "nothing shown" was not traced. This needs a run with
  pictures before it is built, and an entry in docs/coop-failed-attempts.md if it fails. [inferred]
- `frontend_frame` is not called during player 2's copy turn (doc 01 §3.1, hle.c:5511); the open request is atomic and
  is taken at the next real frame. [read in doc 01]

---

## 8. A build order (frontend only)

1. Load La Belle Aurore in `ensure_ui`.
2. `Skin`, `g_skin`, `g_skin_half`; set in `frontend_frame`; classes in `render()`. No look change yet (empty rcss
   blocks). Check the launcher and the debug menu still look the same.
3. Title skin: the clean picture (PNG route), the rcss block, the list scroll (6.2). Open with `ED_PAGE_TEST_MS` and take
   pictures at 16:9, 4:3, 21:9.
4. The title's Options root list (5.1) and `FRONTEND_PAGE_TITLE_CHARACTERS` (with the hle.c row, done elsewhere).
5. Tome skin solo: `tome_page_rect(-1)`, the rcss block in em, then the compact roster (6.4).
6. Tome skin in co-op: section 7 first, then `tome_page_rect(player)`.

Run `ed test` after each change (CLAUDE.md).

---

## 9. Open questions

1. Does the title list hold the game's own eight rows too, or only ours (with the game's rows left as they are)?
2. Title background: the cleaned picture (exact, fixed) or the frozen frame with a black panel (exact crop, shows any
   language's picture)? Other languages' Options pictures were not measured (doc 05 §4).
3. Does RmlUi pass `ed://name` to the provider unchanged (path join)?
4. The game's 2D layout at 21:9: is the Tome page still at 0.294..0.800 of the picture?
5. Which call in `hle_coop_ui_over` must stay so the menu shows, while the split menu is kept?
6. Player 2's page pauses player 1 too (any menu pauses both). Is that wanted?
7. The hint line in the Tome skin: inside the page, or none?
