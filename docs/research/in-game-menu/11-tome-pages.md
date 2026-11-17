# Settings and character choosing as Tome pages (in game)

Read-only research, 2026-11-17. PAL disc (GEDP01). Nothing was built or run.

The user's ask (2026-11-17): in game, choosing Settings (or Characters) on the Tome's Options page should open our
settings **as a Tome page**, not the launcher's page. The Tome's frame stays: the rings, the bones, the medallion,
"LB Journal" and "RB Inventory". Only the page area shows our rows. Each page has a Back row, and B goes back too.
The same for player 2's copy Tome (their Characters).

Tags: **[read]** read in `build/ed-db/disasm.txt`, `generated/main.dol` or the file named. **[seen]** seen in a picture.
**[disc]** decoded from `dump/GEDP01.iso` (UBookPak.bin item 0) with a scratch script built on `tools/ed/disc.py`, and
looked at. **[inferred]** not verified. Reports 01-07 in this folder are cited by number and section, not repeated.

---

## 1. The short answer

- **Recommend Way A: a native page.** Keep the game on its own Options page. Our hooks on that page's own functions
  (five exist today, one new: B) switch it into a "page mode". In page mode the frame hook draws our rows with the
  game's own 2D calls, and the game's own gold ring goes round the chosen row. The rows' logic stays in
  `frontend.cpp` (its `Row` builders), run on the main thread with no RmlUi drawing. [inferred]
- **Why A:** it really is the Tome. Each player's Tome is already drawn in their own half, so the page lands there with
  no new layout work. Player 2's copy Tome gets their own pad with no new input code. And nothing stops: in co-op the
  world keeps running behind player 1's Tome, and player 2's copy Tome never stops player 1. [read: §6]
- **Way B (an RmlUi page cut to the page's rectangle)** is cheaper for solo. But it pauses both players. In co-op it
  turns the split screen off, and no host function gives the page's rectangle in window pixels. [read: §7]
- **The look to copy** is the game's own Display page: framed row bars painted on top of the Options picture, the
  label on the left, the value on the right, and the gold ring round the whole row. [disc, read: §5]

---

## 2. Where the Tome's page is on screen

### 2.1 In game units

- The Tome draws in 640x480 units. The page picture fills **x 144..575, y 0..479** in both layouts. Only the outer
  wings move in the wide layout. [read: 05 claim 11, position tables `0x80252324` and `0x8025251C`]
- The projection is `C_MTXOrtho(top 0, bottom 480, left 0, right 640)`. When `r13-24660 == 1` (the game's
  widescreen word) it is **left -80, right 720**: 800 units across the picture. [read: 06 §5.2, `0x8019CFD4`]
- The free part of the page: the game's cross of four takes y 43..119. Our four plaques take y 128..216. The
  game's own Load/Save sub-page puts its panel at **(154, 131)-(574, 461)**. That is the room a sub-page has under the
  cross. [read: 04 §7; hle.c:16964-16969 for the plaques]

### 2.2 On the real output

The formula: for a picture rectangle (X0, Y0, W, H) on the window,
`x_px = X0 + W * (u - L) / (R - L)` and `y_px = Y0 + H * v / 480`, with (L, R) = (-80, 720) wide or (0, 640) standard.
[inferred from the projection; checked against three pictures below]

| Case | Picture rectangle | Page in pixels | Evidence |
|---|---|---|---|
| Solo, 16:9 window 1280x720, widescreen on | (0, 0, 1280, 720) | **x 358..1050, y 0..720** | [seen] at 960x540 the page runs 268..790 in `Screenshots for Claude/zoom/20261117-033400-jtg-plaqsolo-2300-solo.png`; the formula gives 269..787 |
| Solo, 4:3 (widescreen off), 1280x720 window | (160, 0, 960, 720) | x 376..1024, y 0..720 | [inferred] pillar bars of 160 each side |
| Co-op side by side, 1280x720, player 1 | (0, 180, 640, 360) | **x 179..525, y 180..540** | [seen] `logs/run-20261117-035346-jtg-rc-p1/shots/000098.jpg`: parchment ~180..527; the Characters ring 247..341 (formula 245..342) |
| Co-op side by side, 1280x720, player 2 | (640, 180, 640, 360) | **x 819..1165, y 180..540** | [seen] `logs/run-20261117-035439-jtg-rc-p2/shots/000089.jpg`: parchment ~820..1165; ring 938..1035 |

- In co-op each half shows the game as a 640x360 picture, black above and below. [seen]
- The wide projection is 800x480 units on a 16:9 picture, so one unit is a little wider than tall (1.6 x 1.5 pixels at
  1280x720). [inferred from the numbers] Anything the game draws itself gets this for free. An overlay must copy it.
- The medallion on the left wing reaches into the page's left edge (to about x 165). [seen in the solo picture] A
  panel drawn over the whole page would cover it; the sub-page panel rect (x from 154) only touches it.

---

## 3. Which UBookPak pictures are page backgrounds

All are images of UBookPak.bin item 0 (English), bank `r13-24316`. Sizes and hashes in 06's appendix.

| Image | Size | What it is | Drawn by |
|---|---|---|---|
| 27 | 432x480 | The Options page: the cross of four painted plaques at the top, a column of skeletons and a figure below. No blank page | `0x801117F4(0, …)`, section 0, and every Options sub-page [read: 03 §4.3; seen `Screenshots for Claude/zoom/ubook27.png`] |
| 31 | 432x480 | The Controller page (a GameCube pad with labels). The host already redraws it as a list in the Tome's hand (`tome.cpp`) | Controller sub-page [read: 04 §4] |
| 0 | 432x480 | The Map page: map controls written top left and bottom right, art in two corners, wide plain parchment in the middle | section 2 [read: 06 §6.1; disc] |
| 4 | 432x480 | The Magick page: four plaques on the left (Spell List, New Spell, Runes, Quick Spell), five banners on top, a framed hollow in the middle | section 3 [disc] |
| 44 | 432x480 | The Journal page: a hooded figure with a book, a skeleton, "Cinema" written lower left | section 4 [disc] |
| (book bank) | | The Inventory page's picture comes from the book frame's bank `r13-24320`, not UBookPak | section 1 binds nothing [read: 06 §6.1]; [seen] bird and skeleton art in `Screenshots for Claude/tome-characters/tabs-sheet.jpg` |

**No page picture is blank parchment.** [disc] The blank pieces the game has:

| Image | Size | What it is | Use for us |
|---|---|---|---|
| 22 | 421x331 | Load/Save's panel. Top: "Save", "Load", "Memory Card Slot A", "Memory Card Slot B" plaques. Below them (its y ~110..331): **a blank parchment panel in a bone frame, a skull at the top and bottom** | A list panel, as the game's own Load/Save page uses it [disc; read 04 §7] |
| 33 | 354x149 | Display's three rows: **framed bars** "Brightness Adjustment", "Subtitles ON OFF", "Widescreen 16:9 Standard", 46 units apart | The row look to copy (the bar with its words rubbed out) [disc] |
| 38 | 324x106 | Audio's rows: "Mode" and "Volume" plaques with "Mono / Stereo / Stereo Surround" | [read: 04 §5] |
| 18 | 324x143 | The volume slider's panel: an empty bar between two skulls in a bone frame | A slider row [disc; read 04 §5] |
| 61-73 | 126x89 | **Character portraits** (61 Alex, 62 Pious; the others not matched to names) | A native roster [disc] |
| 75, 92 | 126x89 | Soft white masks (a head shape, a card) | The portraits' masks [disc; inferred] |

- `tome.cpp` already makes **blank parchment from any 432x480 page picture**: `parchment()` fits a smooth surface to
  the page's colour, leaves out the writing and drawings, and adds the page's own mottling and grain
  (`runtime/host/tome.cpp:96-218`). [read] It works on image 27 as well as on 31. [inferred]
- `plaques.cpp` already takes a painted plaque out of image 27 **in memory**, rubs out its words and writes new ones
  (`blank_plaque`, `plaques.cpp:122-151`). [read] The same works for one bar of image 33, or the panel of image 22.
  Nothing of the disc ships. [inferred]

---

## 4. The Tome's own text

- **The Options page and its sub-pages draw no words with the font.** Every label and value there is painted art.
  [read: 04 §1]
- **Words made with the font on Tome pages:**

| Words | Bank | Entry | Style name | Where |
|---|---|---|---|---|
| "LB Journal", "RB Inventory" hints | prompts `r13-26488` | 1,9 and 1,2 (`BOOK_BUTTONS`) | "ButtonText" (`0x8024EE78`) | `0x80101678`, `0x8010169C` [read: 03 §2.1] |
| Inventory item names | boot `r13-26492` | group 0, the item's id | **"ItemText"** (`0x8024F06C`) | `0x80103238`..`0x80103254`, `0x80103850`..`0x8010386C` [read] |
| The volume slider's hint | boot `r13-26492` | 0,37 "\i20 Accept \i21 Cancel" | "ItemText" | `0x80115020` [read: 04 §5] |

- The style name is only a label. The entries carry no colour codes (text.json 1,2 is `\i17~5 Inventory`). [read]
- **Colour:** the hints draw **gold**, in the plain sheet. [seen in both co-op pictures; 05 §2 verdict] Where the
  default gold is set was not traced. [open]
- **Font sheets:** sheet 0 is the plain font, redrawn by the host as `res/fonts/game/tome-0.png` (IM Fell English);
  sheet 1 is the handwritten one, `tome-1.png` (La Belle Aurore). `fonts.cpp` swaps them in when the game decodes its
  sheets. [read: 03 §4.5] Codes in the words: `\s<scale>`, `\c` + 4 bytes RGBA, `\n`, `\f<n>` font, `\j` alignment.
  [read: 04 §10]
- **What the host learned already** (`hle.c:17116-17150`, the plaques' fallback labels): entry 1,2 rewritten with
  `"\s0.78\c%c%c%c%c" + label`, dark ink **52, 34, 18**, alpha 255 chosen and 170 not. The handwritten sheet (`\f1`)
  "drew faint and thin here". [read]
- **The painted words** (plaques, the Display bars, the controls page) are the Tome's hand. The host draws that hand
  with stb_truetype from `res/fonts/LaBelleAurore.ttf` (`tome_hand_cover`, `tome.cpp:579`), ink 38, 27, 17 at 0.86-0.88.
  [read: `tome.cpp:39-41`, `plaques.cpp:46-47`]
- **Budget:** there are **10 text elements** for the whole game (`0x805F87D0`, 0x660 bytes each). The Options page uses 2.
  When none is free, the make prints and the element is empty. [read: 03 §5] In co-op the world behind player 1's Tome
  and player 2's prompts draw from the same 10. [read: 05 §3.4] Player 2's copy Tome has its own copy of the pool.
  [inferred: 05 §3.5]

---

## 5. How the game's own sub-pages draw their rows

From 04 §5-§8, with the art now seen [disc]:

- **Composition.** Every sub-page draws `0x801117F4(0, alpha)` (the book, the tabs, image 27 with the cross), then its
  own panel art **on top of the skeletons**, under the cross:
  Audio image 38 at (214, 150)-(538, 256); Display image 33 at (188, 197)-(542, 346); Load/Save image 22 at
  (154, 131)-(574, 461). The cross stays visible above; the ring is never on it. [read: 04 §5-§7]
- **Rows.** Display's rows are bars 34 units tall, 46 apart: ring `(198, 208 + 46*row, 333, 34)`. Each bar has the label
  at the left and the values at the right. [read: 04 §6; disc]
- **The chosen row:** the gold ring `0x8019CC10(x, y, w, h, -1, 3)` round the whole bar. [read]
- **The chosen value:** a "glow" twin picture tinted `0xC8C800FF` over the painted value. Twins: ON 37, OFF 36,
  16:9 35, Standard 34; Controller's On 41, Off 42. [read: 04 §4, §6]
- **Lists:** the row cursor is the game's list object (`0x801DD31C`, `0x801DD39C`), with `on_change` `0x801104AC` playing
  635 at 50. [read: 04 §2]
- **The slider:** `0x80114FA4(x, y, alpha)` opens it (cursor 0..127, a looping test sound 688, the "Accept / Cancel"
  hint). `0x801151F0(30, 18, 91, …)` draws image 18 (the panel), and the knob at `value*237/128 + x + 59`, with glow 30.
  Left/Right is a **held** row with repeat. [read: 04 §5]
- **Repeat:** a binding row's +4 is 0 for pressed and 1 for held. The Display page writes repeat 20/5 into its own
  table for Brightness (row +20 and +24). The Options page's five rows are all pressed, none repeats:
  `0x8024EE58` (Up/Down) reads `200C0000 0 8010172C 0 1FFFFFFF 0 0 0`. [read: DOL; 04 §6]
- **Sounds:** list move 635 at 50, choose 546 at 100, back 611 at 50, cross move 545 at 50, tab 495 at 50, all with
  `0x8019DB90(id, volume)`. [read: 04 §12]
- **The save prompt:** B on Controller, Audio or Display compares the settings with the Tome's snapshot
  (`0x800AD368(0x80335768)`). If something changed it asks "Save Game Settings … to a Memory Card?". The record holds
  language, brightness, rumble, widescreen, subtitles, the two volumes, the sound mode. [read: 04 §8]

---

## 6. Way A: a native page inside the game's Options page

### 6.1 The idea

- The game stays on its Options page the whole time (`r13-24304 == 0x80101980`). No new game page. No sixth tab.
  (03 §6.3: a real new page needs guest code addresses and art the backdrop switch doesn't have.)
- The host keeps one more word per Tome: **which of our pages is open**, or none. One per memory, as `g_tome_stops`
  keeps its labels (`hle.c:16979-16982`): `[0]` the real memory, `[1]` player 2's copy.
- With a page open, the Options page's own hooks do our page's work instead of the cross's.

### 6.2 Drawing (all in the frame hook, on the game thread, 640x480 units)

1. `0x801117F4(0, alpha)`: the book, the tabs, the medallion, image 27 with the cross. As the game's sub-pages. [read]
2. **The panel**, from a host bank as the plaques are drawn today (`tome_plaques_draw`, `hle.c:17263-17285`): one stub,
   overridden by a picture made at run time from the game's own art. Two choices:
   - the blank bone-framed panel of image 22 (rubbed clean of its plaques), at about (154, 131)-(574, 461), as Load/Save;
   - or no panel: rows straight on the skeletons, as Display.
3. **The rows**, each a picture from the same host bank: **one bar of image 33 with its words rubbed out**, with our
   label and value written in the Tome's hand (`tome_hand_cover`). Display's pitch (bar 34, gap 12) gives **7 rows**
   between y 131 and 461; a pitch of 40 gives 8. A list longer than that scrolls: the host keeps the first row in view.
4. **A title** at the top of the panel (the page's name: "Graphics"), and a small "more above / more below" mark when
   it scrolls. Pictures too.
5. **The ring:** `0x8019CC10(x, y, w, h, -1, 3)` round the chosen bar. As today's `tome_stops_frame` does. [read]
6. **The note** of the chosen row (`Row::note`), if wanted, as a picture strip under the list.
7. The hints ("LB Journal", "RB Inventory") are drawn by the Tome's own frame after ours. Nothing to do. [read: 03 §4.1]

How a row's picture changes when its value changes [inferred]:

- The renderer keys a texture by its guest address and a hash of its bytes, and asks the override only on a miss.
  [read: 06 §1] So each row stub carries a small **content key** in its bytes (as the plaques' stubs carry `kVersion` in
  their second block, `plaques.cpp:227-238`). A new value gives new bytes, a new hash, a new picture.
- Stubs can be tiny: the override may return any size. [read: 06 §1] An 88x8 CMPR stub is 352 bytes, so 16 rows fit in
  6 KB. The room after the plaques' bank is `SCRATCH_MENU + 0xA000` to `+ 0x10000`. [read: hle.c:17192 comment]
- To avoid a blank frame while a picture is drawn (`plaques_apply` returns 2 then, `plaques.cpp:258-276`), draw the
  picture **before** handing the key to the game thread: the main thread builds the rows, draws their pictures, then
  publishes the keys. A row strip is small; a whole 432x480 page at 4x is not (tome.cpp builds that on a worker).
- Values with the game's own art (ON/OFF twins 37/36) are possible, but every row in the hand is simpler and matches
  the plaques. [inferred]

A cheaper first step: the **game's text elements** for the words (as the plaques' fallback does), three elements (title,
labels column with `\n`, values column) over the panel. That proves the flow with no new pictures. The line pitch of the
game's font is not known, so the ring may not sit on the lines until it is measured. [inferred]

### 6.3 Input

The Options page has one input channel, `0x8024EDD8`, five pressed rows. [read: 03 §3] Each hook calls `0x8013E154`
("input taken") and returns, as `tome_stops_*` do now.

| Press | Hook | In page mode |
|---|---|---|
| Up / Down | `0x8010172C` (exists) | Row up / down. Sound 635 at 50 when it moved |
| Left / Right | `0x8010179C` (exists) | The row's `change(-1/+1)`. Sound 635 at 50 (or 546 for a toggle) |
| A | `0x80101838` (exists) | On a plaque: **open the page here** (page mode on) instead of `frontend_open_page`. On a row: its `activate`. Sound 546 at 100 |
| **B** | **`0x8010180C` (new intercept; first word `0x9421FFF0`, read)** | **One level back. At a page's root: page mode off, the cursor back on its plaque (`0x801DD354`), sound 611 at 50.** Without this hook B closes the whole Tome (`r13-24244 = 2`, `0x8010181C`) [read] |
| L / R / Start | `0x801018C0` (no hook) | The game's own: leave Options (L Journal, R Inventory, Start closes). Its exit runs, and the exit hook ends page mode. The game's own sub-pages do the same [read: 04 §4] |
| (leaving) | `0x801016D8` exit (exists, a notify) | Page mode off, settings saved |

- **Repeat** for long lists and sliders: while page mode is on, set the Up/Down and Left/Right rows of `0x8024EDD8` to
  held with a repeat (+4 = 1, +20/+24 = 20/5, as Display does), and put them back on leaving. [inferred from 04 §6]
  Or step sliders by 5 or 10 a press.
- The keyboard works through the game's pad (the host feeds it from the bindings). B is whatever B is bound to.
  [inferred]
- **Player 2:** their copy Tome is fed their pad (`tome_copy_pad`, 05 §3.5). The same hooks fire in their copy's turn.
  Nothing new. [read]

### 6.4 Where the rows' logic runs

- The rows are `frontend.cpp`'s: about 113 `push_back`s with labels, values, notes, sections, `change` and `activate`
  closures (`build_options_rows` `frontend.cpp:1903`, `build_graphics` :2244, `build_controls` :2353, `build_mods` :1327,
  `build_characters` :1086, `build_states` :1537). Rebuilding them in hle.c would be a second copy. [read]
- Their closures call `changed()` → `settings_apply()` (`frontend.cpp:407`), which calls SDL window functions
  (`dol_aurora_set_fullscreen` → `aurora::window::set_fullscreen`, `aurora_backend.cpp:3395`). Those belong on the main
  thread. [read]
- The main thread already runs `frontend_frame` once a shown frame, from `hle_frame_boundary` (`hle.c:5506-5511`,
  installed by `dol_aurora_set_frame_hook`, `main.c:903`). [read]
- So: **the game-thread hooks only post presses** (player, Up/Down/Left/Right/A/B, open page N, close). `frontend_frame`
  drains them, runs the row logic with no RmlUi drawing, draws the row pictures, and publishes a **snapshot** per
  player under a lock: page title, the rows in view (keys, rectangles), the chosen row, the top row, and what happened
  (moved, changed, refused, closed). The frame hook draws the snapshot and plays the sounds for what happened. One or
  two frames late, which a menu does not feel. [inferred]
- What `frontend.cpp` needs for that [read: `build_rows` :2810-2844, `run_menu` :3113-3226, `go_back` :2869-2939]:
  - `build_rows()` without its `render()` call.
  - The press handling inside `run_menu`'s loop (the roster grid, the cards' left/right, the slots' page jump, A, Back)
    as one function both use.
  - The menu's globals (`g_screen`, `g_selected`, `g_root`, `g_root_closes`, `g_characters_player`, `g_menu_player`,
    `g_options_row`, `g_mods_section`, …) kept **per player** and swapped in around each player's step. Player 1 and
    player 2 can both have a page open at once in co-op.
  - "Back past the root closes" already exists (`g_root_closes`, `past_root`, :2804-2814). Here it means page mode off.
- Rows that need a modal screen stay modal and pause as today: key capture (Controls > Mapping, `controls::capturing`),
  HDR calibration, problem dialogs. They open the RmlUi menu as now. [inferred]

### 6.5 The world behind

- **Solo:** the Tome itself stops the world: the game loop calls only the Tome's frame in mode 3. Same as now. [read: 03 §7]
- **Co-op, player 1's real Tome:** the host runs the world's frames between the Tome's (`g_tw.sharing`), so **player 2
  keeps playing** while player 1 changes settings. [read: 03 §7, hle.c comment ~13640]
- **Co-op, player 2's copy Tome:** it runs in its own turn on a copy of memory; **player 1 keeps playing**. [read:
  coop-two-tomes.md via 03]
- Today's RmlUi page stops both (§7.3). Way A removes that for every row that is not modal.
- The user said on 2026-11-15 "paused as the pause menu is" (`docs/coop-todo.md:243-246`). Not pausing is a change from
  that. It is the user's call (open question 1).
- Rows to hold back while the world runs [inferred]:
  - **Aspect ratio.** The game's own Widescreen toggle fades out and **reloads the Tome's art** before flipping
    `r13-24660` (04 §6, state 4). The host's `sync_widescreen_option` (`hle.c:6236-6267`) would flip the word next frame
    with no reload: the book's wide quads with the narrow art, or the reverse. Hide the row in the Tome; the game's own
    Display > Widescreen does it, and the host already follows that.
  - **Co-op on/off** with the other player mid-play: today it is changed with everything paused. Hide it in the Tome,
    or apply it when the Tome closes.
  - **A character change** in co-op: `characters_reload` starts in the world's frames (05 open question 5). It may now
    start behind an open Tome. Needs a run.

### 6.6 Player 2's copy Tome

- Their stops are `kTomeStopsP2`: Characters alone (`hle.c:16971-16973`). Their page mode lives in memory slot `[1]`
  and their snapshot is player 2's. [read]
- Sounds in their turn must start in the real memory, as `tome_stops_sound` does (`hle.c:17010-17019`). [read]
- The row pictures' bank is written into whichever memory is in, at the same address, as the plaques' bank is
  (`hle.c:17186-17190` comment). [read]
- `frontend_frame` does not run in a copy turn (`hle.c:5508-5511`); presses posted then are taken at the next real
  frame. [read]
- Their Characters rows must be built with `g_characters_player = 1` (`frontend.cpp:624`). [read]

### 6.7 The save prompt

- Our rows change the host's settings file (`settings_save`, `settings.c:266`), not the game's settings record. So
  leaving our page asks nothing. [read]
- The game's prompt runs only on B from its own Controller, Audio and Display. It compares the record
  (`0x80335768`). [read: 04 §8] Of our rows, only Aspect ratio touches a record field (widescreen +16), through the
  host's sync. With Aspect ratio hidden in the Tome (6.5), no false prompt can come. [inferred]

### 6.8 Characters as a Tome page

- A grid of portrait tiles on the panel: 3 across, 2 or 3 down at about 120x90 units each, scrolling. The ring round
  the chosen tile. Then the Stats and Back rows. [inferred]
- Portraits: the launcher's own files (`res/ui/portraits/*.png`, made from `UMnMenu.tpl` and `tools/ed/form_portraits.py`,
  `frontend.cpp:996-1047`) as host bank pictures, so the forms match the launcher's roster. The Tome's own portraits
  61-73 are a native choice, but which image is which character is not mapped. [read; disc]
- The roster's moves (`move_in_roster`, `frontend.cpp:1121`) come along with the shared press function (6.4).

### 6.9 Effort

| Part | Where | Size [inferred] |
|---|---|---|
| Page mode, the B hook, drawing the snapshot, the panel and row stubs, the ring, sounds, the per-memory word, the state-load reset | hle.c, beside `tome_stops_*` | 400-600 lines |
| The posted presses and the snapshot; the headless row step; per-player menu state | frontend.cpp, frontend.h | 300-400 lines |
| Row, title and panel pictures (image 33 bar and image 22 panel rubbed clean, words in the hand, a slider bar) | a new module beside plaques.cpp, one more claim in the texture override chain (buttons.cpp) | 300-400 lines |
| The roster grid | both | 150-250 lines |

The biggest risk is the frontend refactor: the menu globals are shared with the title's More Options page and the
debug menu.

---

## 7. Way B: an RmlUi page cut to the page's rectangle

### 7.1 The idea

Keep `frontend_open_page`. Give the RmlUi document a `body.tome` style: no dimming, a parchment panel placed exactly over
the Tome's page rectangle (§2.2), the Tome's fonts, a ring drawn in CSS, over the paused frame.

### 7.2 What it needs

| Need | State | Evidence |
|---|---|---|
| The page's rectangle in window pixels | **No host function gives the game picture's rectangle.** RmlUi's own sizing uses `webgpu::calculate_present_viewport` | [read: 05 §3.4] |
| The co-op half | `hle_coop_ui_over(true)` **turns the split off** for any menu over a Tome, and puts the views back after (`hle.c:13893-13917`). The comment: the renderer kept the menu's frames for the Tome's and the world's, so the menu was not shown. Showing the page in one half with the other half still there is renderer work | [read] |
| Fonts | RmlUi takes TTFs only. IM Fell English is loaded; La Belle Aurore is not (one `LoadFontFace` call). Not the game's sheets, but the same faces | [read: 05 §3.4] |
| Parchment | A file for RmlUi. It can be written at run time from the game's art (as `ensure_portraits` writes the portraits), e.g. `tome.cpp`'s `parchment()` or image 22's panel | [read: `frontend.cpp:996-1031`] |
| Input | `run_menu`'s own. Player 2's controller through `g_menu_player` / `menu_gamepad()` (`frontend.cpp:103-124`). Player 2 on the keyboard shares the menu's keys | [read; 05 §3.5] |
| The save prompt | None: the same as Way A | [read] |

### 7.3 The world behind

- `run_menu` is a loop on the main thread. The game thread stops at its next present and waits. **Both players stop**,
  the Tome too (its medallion, the hints' pulse). [read: 05 claim 17 and C2; `frontend.cpp:4499-4512`]
- That is what `docs/coop-no-pause.md`'s whole list works against, though that list is about pickups and words, not
  menus. [read]

### 7.4 Effort

- Solo: a style sheet and a rectangle sent from the renderer. Small. [inferred]
- Co-op: the split kept under the menu, the page in one half. Renderer work of unknown size. [inferred]
- The look: close but not the game's own. The Tome behind is a still picture, the ring is CSS, the hints don't move.

---

## 8. Side by side

| | Way A (native) | Way B (RmlUi cut to the page) |
|---|---|---|
| Looks like | The game's own sub-pages: the cross above, framed bars, the game's gold ring, the Tome alive behind | A panel over a still picture of the Tome |
| Where it draws | Wherever the Tome draws: each player's half, 4:3 or 16:9, for free | A computed rectangle; the co-op half needs renderer work |
| Input | The Options page's own handlers; player 2's pad in their copy for free; one new hook (B) | `run_menu`; player 2 by `menu_gamepad` |
| The world behind | Keeps running in co-op (both ways round); solo paused by the Tome as always | Everything stops, both players |
| Player 2 | Their copy Tome, their half, their pad | Their pad; the split off |
| Save prompt | None | None |
| Modal rows (key capture, HDR) | Open the RmlUi menu as today | Already there |
| Effort | Large: hle.c + a frontend refactor + a picture module | Small for solo, unknown for co-op |
| Main risk | The frontend globals made per player; rows that must not change live (Aspect, co-op) | It is the launcher page again, just placed |

---

## 9. Recommendation: Way A, with these hooks

**Game hooks** (all on the Options page, gated by `tome_stops_on`: `r13-24304 == 0x80101980`; the player by
`tome_stops_player`):

| Address | First word | Kind | Today | New work |
|---|---|---|---|---|
| `0x80101980` Options frame | `0x9421FFD0` | intercept | `tome_stops_frame` | Page mode: backdrop, panel, row pictures, ring on the row, sounds from the snapshot |
| `0x8010172C` Up/Down | `0x9421FFF0` | intercept | `tome_stops_updown` | Page mode: post Up/Down |
| `0x8010179C` Left/Right | `0x9421FFF0` | intercept | `tome_stops_sides` | Page mode: post Left/Right |
| `0x80101838` A | `0x9421FFF0` | intercept | `tome_stops_choose` | On a plaque: page mode on (not `frontend_open_page`); in page mode: post A |
| **`0x8010180C` B** | **`0x9421FFF0`** [read] | **intercept (new)** | none | Page mode: post Back; at the root, page mode off and the cursor back on the plaque. Otherwise pass through |
| `0x801016D8` Options exit | `0x9421FFF0` | notify | `tome_stops_exit` | Page mode off, post Close (save), put the binding rows back |
| `0x8024EDD8` + rows 3, 4 | data | write while open | none | Optional: held with repeat 20/5, restored on leaving |
| state load | | | `tome_stops_forget`, `tw_state_loaded` | Page mode off for both memories |

Install them with the existing `kStops` table (`hle.c:33426-33447`), before `publish_codegen_options` (05 C6). They must
not be on `tome_copy_hook_off`'s list (`hle.c:32691`), so player 2's copy has them. [read]

**Game calls used** (all already used by hle.c): `0x801117F4` backdrop, `0x801E1B74` bank bind, `0x8019D178(0x80254D10)`
texcoords, `0x8019C7C8` material, `0x8019D1A4` / `0x8019D084` quad, `0x8019CC10` ring, `0x8019C9C8` plain frame,
`0x801DD354` cursor set, `0x8013E154` input taken, `0x8019DB90` sound; for the text-element first step `0x801DB374`,
`0x801DA8CC`, `0x801DA9CC`, `0x801DA4D8`. [read]

**Host side:**

1. `frontend.h`: `frontend_tome_post(player, press)` and `frontend_tome_snapshot(player, &out)`.
2. `frontend.cpp`: the headless step in `frontend_frame` (drain, step, draw pictures, publish); `build_rows` without
   `render`; one press function shared with `run_menu`; per-player menu state.
3. A picture module beside `plaques.cpp`: panel, title, row bars and tiles from the game's art in memory, words in the
   Tome's hand; claimed in the texture override chain.
4. Rows hidden in the Tome: Aspect ratio, Disc, launcher-only rows; co-op on/off per the user.

**Order to build:** (1) page mode with text elements only (Settings root: Display, Sound, Graphics, Controls, Back), to
prove input, Back, scrolling and the exit; (2) the row pictures; (3) the sub-pages; (4) Characters; (5) player 2.

**Checks after building:** `sh tools/ed/run.sh saves/slot7.state NAME` in solo and co-op; a picture of each page in both
halves; B from a sub-page, from the root, and L/R/Start out of a page; player 2 moving while player 1 is on Graphics;
a quick state saved and loaded with a page open; the element count (if text elements are used) with a prompt up behind.

---

## Open questions

1. **Pause or not.** Way A lets the other player keep playing. The user said on 2026-11-15 the Tome pages should pause
   as the pause menu does. Which now?
2. **Panel or bars.** Image 22's blank framed panel (as Load/Save) behind the rows, or the rows straight on the
   skeletons as Display does? A picture of each would settle it.
3. **The rows' lettering.** The Tome's hand (La Belle Aurore, as the plaques and the controls page) or the plain sheet
   (IM Fell English, as the gold hints)? The plaques set the hand.
4. **Co-op on/off and Aspect ratio in the Tome.** Hide, or apply on the Tome's close?
5. **Portraits.** The launcher's files or the Tome's own 61-73 (not yet mapped to characters)?
6. **The game font's line pitch** for the text-element first step. A picture of three lines settles it.
7. **The default gold** of the Tome's words: where it is set was not traced.
8. **Z of the 2D material** (06 open question 1). The panel at z -1 after the backdrop is assumed to sit on top.
