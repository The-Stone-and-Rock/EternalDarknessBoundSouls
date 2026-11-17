# The title screen's main menu and its Options (PAL, GEDP01)

Research only. Nothing here is built or run. Every address is the PAL disc's.
"[read]" means I read the code or data. "[inferred]" means it follows from the code but was not checked in a run.
Tools used: `build/ed-db/disasm.txt` (lis/addi pairs resolved by hand-made script), `generated/main.dol` data,
the disc's `UMnMenu.cmp` unpacked with `tools/ed/ppcemu.py` (the game's own decompressor) and its textures decoded,
`res/video/options.rgba`, and a title shot from `logs/run-20261105-194551-jtg-probe2/shots/000020.jpg`.

Short answer:
- The title menu is one state machine in the "title module" (`0x80019000`-`0x80020600`), run by game mode 0.
- Its words are **pictures, not text**. Start Game, Load Game and Options are painted into one 850x480 picture.
  Jump to Game and Credits are separate word pictures. The highlight is a second, glowing picture of the chosen word,
  drawn over it.
- The title's Options is **its own screen** (state 14 of the title module). It is not the Tome's Options page
  `0x80101980`. It only shares the settings words and two sub-pages (Volume, the brightness slider's code).
- The cleanest native-looking additions: a 6th main-menu word under Credits, or a 9th Options row under
  "Language Select", both reached by the game's own cursor. Each needs 3 or 4 small hooks (section 7).

---

## 1. Where the title menu lives

| What | Where | Evidence |
|---|---|---|
| Game mode word (0 = title) | `r13-26240` (`0x8061EDA0`), asked-for mode `r13-26188` | hle.c:13640; mode change `0x80018728` [read] |
| Mode table (start, frame) | `0x80238810`, 8 bytes per mode | `0x80018784`-`0x800187A4` [read] |
| Mode 0 start | `0x8001FFB8` | table entry 0 [read] |
| Mode 0 frame | `0x80020144` | table entry 0 [read] |
| The menu's record ("TM") | `0x80309124` (= `0x80309100 + 36`) | every title function loads it [read] |
| The menu's input channel | `r13-26132` (`0x8061EE0C`) | `0x80020004`: `0x8013DB3C(7, 0x8023D95C, 0)` [read] |
| Menu pictures pack | `UMnMenu.cmp` (English), `UMnMenuF/G/S/I.cmp` | loader `0x80019730`, names at `0x8023DA3C` [read] |
| Language word | `r13-26284` (`0x8061ED74`): 0 E, 1 F, 2 G, 3 S, 4 I, 5 a sixth (J files exist) | `0x80020064` switch picks `UMemcardText?.bin` [read]; 5 = Japanese [inferred] |

r13 is `0x80625420` and r2 is `0x80628460` (set at `0x80005424`-`0x80005430`) [read].

### 1.1 The record TM (`0x80309124`)

| Offset | Address | Meaning | Evidence |
|---|---|---|---|
| +20 | `0x80309138` | fade-in level 0..255, +4 a frame | `0x8001F5D0`-`0x8001F5F4` [read] |
| +28 | `0x80309140` | **cursor** (row index) | up/down `0x8001B784`, A `0x8001A4A4` [read] |
| +32 | `0x80309144` | cursor of the previous screen | `0x800194E4` [read] |
| +36 | `0x80309148` | frames in this state | `0x8001B980`-`0x8001B990` [read] |
| +40 | `0x8030914C` | **number of rows** the cursor wraps over | `0x8001B78C`, set 5/8/12/2 by the screens [read] |
| +48 | `0x80309154` | flags. Bit 0x2: idle clock running. Bit 0x20: cross-fade from the previous screen | `0x8001C55C`, `0x8001F610` [read] |
| +52, +56 | `0x80309158`, `0x8030915C` | sound handles (the whispered row names) | `0x8001B810`-`0x8001B8B8` [read] |
| +62 | `0x80309162` | **screen state** (byte) | every handler switches on it [read] |
| +63 | `0x80309163` | previous screen state | `0x800194D0` [read] |
| `0x80309100+104` | `0x80309168` | idle stopwatch (OSStopwatch) | `0x8001C56C`-`0x8001C5E0`, SDK names in `generated/symbols.tsv` [read] |
| `0x80309100+732` | `0x803093DC` | settings snapshot taken when Options opens | `0x8001A5EC`, compared at `0x8001B0E4` [read] |

### 1.2 Changing screen

- `0x80019470(state, cursor)`: new state and cursor, keeps the old ones in +63/+32, fade back to 0 (the new screen
  fades in), clears flag 0x2, stops and resets the idle stopwatch [read `0x80019470`-`0x800195D4`].
- `0x800195D8(state, cursor)`: the same without restarting the fade [read].

### 1.3 Screen states seen (byte +62)

| State | Screen | Evidence |
|---|---|---|
| 7 | **Main menu** (Start Game, Load Game, Options, Jump to Game, Credits) | its own up/down `0x8001B778`, draw `0x8001E2F0`, A `0x8001A464` [read] |
| 8 | after Start Game | `0x8001A4F4` [read]; what it shows not read |
| 9 | Jump to Game chapter grid (12, 4 across) | `0x8001A644`, up/down `0x8001B65C` steps by 4 [read] |
| 13 | memory card screen of Load Game (2 rows) | `0x8001A5D8` [read]; hle.c:3911 calls its stage 6 |
| 14 | **Options** (8 rows) | `0x8001A5FC`, `0x8001A608` [read] |
| 15 | Options > Brightness Adjustment (a slider) | `0x8001ABE4` [read] |
| 16 | Options > Volume Adjustment | `0x8001AB78` [read] |
| 23 | idle: goes to state 1 and plays a movie | `0x8001C600`, `0x8001C280`-`0x8001C2B8` [read]; "the intro movie" [inferred] |
| 28 | Credits | `0x8001A628` [read] |
| 32 | Language Select (from Options row 7) | `0x8001AC88` [read] |

---

## 2. The input: how Up, Down, A and B work

The channel has 7 bindings of 32 bytes at `0x8023D95C` [read: dumped from main.dol]. Layout per docs/game/research/verbs.md:236.

| Row | Mask | Buttons | Handler | Note |
|---|---|---|---|---|
| 0 | `0x0C10` | X, Y, Z | `0x8001AE24` | a lone `blr`: does nothing |
| 1 | `0x1100` | A, Start | `0x80019F30` | choose |
| 2 | `0x0200` | B | `0x8001AE28` | back |
| 3 | `0x200C0000` | D-pad up/down, stick up/down | `0x8001B588` | move the cursor |
| 4 | `0x10030000` | D-pad left/right, stick left/right | `0x8001B300` | only used by some screens |
| 5 | `0x0800` | Y | `0x8001B920` | a lone `blr`: does nothing |
| 6 | `0x20CC0000` | (vertical, held) | 0 | filled by the brightness slider (`0x801160D4`) |

- The channel only runs when the fade is complete: the step `0x8001B924` calls `0x8013E160(r13-26132)` only if
  TM+20 == 255 [read `0x8001B964`-`0x8001B978`]. So during a fade (64 frames) no press is taken.
- Some screens overwrite rows 3 and 4 of this table in place and put them back later: Load Game copies from
  `0x80238880+64` [read `0x8001A54C`-`0x8001A5D4`], Volume from `0x80238880+332` [read `0x8001AB38`-`0x8001AB98`],
  brightness writes row 3's and row 6's handler words at `+104` and `+200` [read `0x8001ABA4`-`0x8001ABB8`].
- **X, Y and Z are free on the title.** Both of their handlers are empty (`0x8001AE24`, `0x8001B920` are `blr`).

### 2.1 Up/Down `0x8001B588(dir)`

- `dir` is the push direction (Down = +1). Switch on the state through `0x8023DBF0` (states 4..32) [read].
- **Main menu (state 7), `0x8001B778`**: cursor += dir, wrapped to `0..count-1` (count = TM+40 = 5). Then it skips
  rows that are not shown: row 4 (Credits) when fewer than 1 bit is set in the byte `0x80308A88`, row 3 (Jump to
  Game) when fewer than 2 [read `0x8001B7BC`-`0x8001B804`; `0x801DBDA8` counts bits, read]. That byte is
  `0x80307170 + 6424`. Its meaning, the games finished [inferred].
- Then it stops the last whisper (TM+56) and plays the new row's: 629 Start, 630 Load, 631 Options, 632 Jump,
  724 Credits, by `0x8019DB90(id, 100)` [read `0x8001B808`-`0x8001B8B8`]. A row above 4 plays nothing (the switch
  falls through to `0x8001B900`) [read].
- **Options (state 14), `0x8001B6A4`**: cursor wrapped to `0..count-1` (count 8), then sound 635 [read].
- Matches the run notes: with Jump to Game shown the menu has 5 rows and wraps; 6 Downs land on Load Game
  (tools/ed/jtg.sh:17).

### 2.2 A (and Start) `0x80019F30`

First: if a memory card notice is up (`0x800AF00C() != 0`), A goes to it (`0x800B01C0`) [read, e.g. `0x8001A9FC`].
"Notice is up" is [inferred] from how every handler uses it.

**Main menu, state 7 (`0x8001A464`)**: stops both sounds, then by cursor [read `0x8001A4A0`-`0x8001A688`]:

| Row | Word | What A does |
|---|---|---|
| 0 | Start Game | `0x801DC1D0(r13-26528, 894)`; `r13-24328 := 0x8002089C(language)` (loads the Tome book); state 8; `0x80019454()`; sound 634 |
| 1 | Load Game | `0x801DC1D0(.., 894)`; book load; `0x800AEE4C(0, 0x8001FAA8)`, `0x800B3FE8(1,1)`, `0x800AFC20(1,0)` (card); rows 3-4 replaced; state 13; count 2 |
| 2 | Options | settings snapshot `0x800AD218(0x803093DC)`; state 14 cursor 0; `r13-30872 := 1`; count 8; sound 723 |
| 3 | Jump to Game | book load; state 9; `0x801DC1A4(r13-26528, 894)`; count 12; sound 722 |
| 4 | Credits | state 28 |
| 5+ | (none) | nothing but sound 633 (`0x8001A678`) |

All rows end with sound 633 [read `0x8001A67C`]. 894 looks like a flag the game sets for Start/Load and clears for
Jump to Game [inferred].

**Options, state 14 (`0x8001A9FC`)**: a cursor above 7 does nothing but sound 636 [read `0x8001AA14`-`0x8001AA1C`,
`0x8001AE04`]. Rows by `0x8023DB64` [read]:

| Row | Word (as painted) | What A does | Setting it changes |
|---|---|---|---|
| 0 | Save Settings | card save: `0x800AEE4C(1, 0x8001F9C0)`, `0x800B3FE8(0,1)`, `0x800AFC20(1,0)` | memory card |
| 1 | Subtitles | toggles bit 0 of `0x80307180` (language 5: swaps `UBtPakES.bin`/`UBtPakJS.bin`) | `0x80307180` |
| 2 | Widescreen | toggles `r13-24660` (`0x8061F3CC`) | the same word hle.c:6035 syncs with the launcher's Aspect ratio |
| 3 | Rumble Feature | `0x8013D97C(1)` / `0x8013D93C(1, !x)`, a 30-frame buzz `0x8013D6CC(1,0,30)` | pad motor flag [inferred] |
| 4 | Volume Adjustment | camera push, camera 8, state 16, row 4 replaced, `0x80114FA4(102, 154, 254)` | volume (`0x8019DCDC`) |
| 5 | Brightness Adjustment | slider `r13-24056` (0..51), row 6 := `0x801160D4`, state 15 | `r13-24664` (`0x8061F3C8`), applied each frame by `0x801E12C0` |
| 6 | Audio Mode | cycles `r13-23012` 0,1,2; `0x8019D944(x,1,1)` | `r13-23012` (`0x8061FA3C`) |
| 7 | Language Select | return point 14/7 in `r13-26056`/`r13-26060`; state 32 | `r13-26284` |

The words are from the Options picture itself (section 3.3). The code order matches them one for one.

### 2.3 B `0x8001AE28`

- Main menu (state 7): nothing [read `0x8001AE78`].
- Options (state 14, `0x8001B0C8`): if the settings differ from the snapshot (`0x800AD368(0x803093DC)`), the card
  asks to save (`0x800AFC20(11, 0)`); else `0x8001FA38(1)`: back to the main menu, count 5, cursor 2 (Options)
  when `r13-30872` is set, whisper 631 [read `0x8001B0DC`-`0x8001B124`, `0x8001FA38`-`0x8001FAA4`].
- Brightness (15, `0x8001B218`): frees the slider, puts row 3 back, back to Options row 5 [read].
- Volume (16, `0x8001B264`): puts row 4 back, camera pop, back to Options row 4 [read].

### 2.4 Left/Right `0x8001B300`

Main menu and Options do nothing with it except pass it to a card notice (`0x8001B570`, `0x8001B558`) [read,
table `0x8023DB84`].

---

## 3. How the menu is drawn

### 3.1 The order

- Mode 0 frame `0x80020144` calls the title draw `0x8001F5A4` [read `0x800201C4`] and later the step `0x8001B924`
  [read `0x800202AC`].
- `0x8001F5A4` raises the fade (TM+20) and calls the screen draw `0x8001DF90(state, cursor, alpha)`. During a
  cross-fade (TM+48 bit 0x20) it first draws the previous screen (+63, +32) at 255-alpha [read `0x8001F610`-`0x8001F658`].
- `0x8001DF90` switches on the state: 7 → `0x8001E2F0`, 14 → `0x8001D52C`, 16 → `0x8001D52C` plus the volume bar
  `0x801151F0`, 15 → `0x8001ECB8` [read `0x8001DFE4`-`0x8001E0E8`, `0x8001E464`-`0x8001E4C4`].

### 3.2 The main menu (state 7, `0x8001E2F0`)

1. `0x8019D4EC(1, alpha, 1)`: the whole picture, texture 1 of the pack, on the wide quad x -106..745, y 0..479
   (`0x80254D20 + 3*24`) [read `0x8019D4EC`, quads dumped]. In 640x480 menu space, so the 850-wide picture is
   cropped to its middle on 4:3.
2. `0x8019D358(0x80254D10, 0x8023C780)`: from here on, word quads come from the table `0x8023C780`
   (4 corners x (x, y, z) s16, 24 bytes each) [read].
3. Only if `r13-26460 != 0` (the pack is in [inferred]):
   - Credits, unlit, texture 69, if 1+ bits are set in `0x80308A88` [read `0x8001E334`-`0x8001E380`].
   - Jump to Game, unlit, texture 70, if 2+ bits [read `0x8001E384`-`0x8001E3C4`].
   - **The highlight**: texture `cursor + 6`, the chosen word's glowing version, drawn at quad
     `0x80018824(cursor)`, tinted pale cyan `0xA0E3F1` (r2-32388) at the fade's alpha. Jump to Game glows
     `(255, 249, 183)` instead when 3+ bits are set [read `0x8001E3C8`-`0x8001E42C`].
- `0x8019C7C8(&colour, slot, texture, flags)` sets the material (a texture index into the loaded pack) and
  `0x8019D3B4(0, quad*4, 5)` draws one quad from the table [read both]. `0x80018824(n)` returns the quad index
  for row n in the current language: English n, French 42+n, Italian 63+n, German 85+n, Spanish 107+n
  [read `0x80018824`, `0x80018B4C`-`0x80018CD0`].

English quads (640x480 menu space) [read: `0x8023C780` dumped]:

| Row | Quad | x0..x1, y0..y1 | Glow texture (size) |
|---|---|---|---|
| 0 Start Game | 0 | 436..603, 243..305 | 6 (168x63) |
| 1 Load Game | 1 | 427..586, 279..340 | 7 (160x62) |
| 2 Options | 2 | 452..556, 323..378 | 8 (104x55) |
| 3 Jump to Game | 3 | 367..614, 348..428 | 9 (247x80), unlit 70 |
| 4 Credits | 4 | 442..564, 382..441 | 10 (122x59), unlit 69 |

The sizes match the pack's textures exactly [read: `UMnMenu.cmp` unpacked has 83 textures: 1 = 850x480 title,
2 = 850x480 Options, 6..10 the glows, 69/70 the unlit words; 0, 4, 11, 30-33, 40 are identical 2x2 blanks].

**titlevideo.cpp's comment is right** (titlevideo.cpp:1-7): texture 1 holds Start Game, Load Game and Options
painted in (seen in the decoded texture), and the glow, Jump to Game and Credits are drawn over it. The 160x62
"Load Game" it keeps (`kLoadWordHash`) is glow texture 7.

### 3.3 The Options screen (state 14, `0x8001D52C`)

1. `0x8019D4EC(2, alpha, 1)`: texture 2, the X-ray skull picture with all eight row names painted on the left
   [read; seen in res/video/options.rgba]: Save Settings, Subtitles, Widescreen, Rumble Feature, Volume Adjustment,
   Brightness Adjustment, Audio Mode, Language Select.
2. Each setting's value as a small word texture at its own quad: Subtitles (`0x80307180` bit 0) textures 26/27 at
   quads 16/17; Rumble textures 27/26 at quads 13/12; Widescreen textures 28/29 at quads 18/19; Audio Mode
   textures 42-47 at quads 20-22 [read `0x8001D598`-`0x8001D930`]. Which texture says "On" or "Off" [inferred].
3. The glow of the chosen row, by `0x8023DD80` (rows 0..7). Row 0's is texture 48 (143x70, a glowing
   "Save Settings", decoded) at quad 23 (71..214, 77..147) [read `0x8001D954`-`0x8001D988`].
4. **A cursor above 7 draws no glow**: `cmplwi r29, 7; bgt 0x8001DDD8` at `0x8001D934` [read].

There is room under "Language Select" for one more line: the last painted word ends near y 410 of 480 in the
picture (seen in options.rgba).

### 3.4 What is idle on the title

- No world runs: mode 0 only runs the title frame [read the mode table].
- **Idle clock, main menu only.** The state-7 step `0x8001C55C` starts an OSStopwatch at `0x80309168`, resets it
  on any press (`0x8013DC24(-1, 0, 0)`), and after 180000 ms (r2-32420 = `0x806205BC`, 180000.0) goes to state 23
  (33 if `0x80018F94()` says so). State 23 then plays a movie (`0x8004F4D0(0, ...)`) [read `0x8001C55C`-`0x8001C684`,
  `0x8001C280`-`0x8001C2CC`]. So three idle minutes on the main menu plays the attract movie.
- States 14 to 17 (Options and its sub-pages) have **no idle clock**: the step does nothing for them
  [read `0x8001B9D4`-`0x8001B9F8`].

---

## 4. Is the title's Options the Tome's Options? No.

| | Title Options | Tome Options tab | Tome Display sub-page |
|---|---|---|---|
| Entry | state 14 of the title record | `0x80101600` | `0x80115FEC` |
| Page function | (draw `0x8001D52C`) | `0x80101980` in `r13-24304` | `0x80116470` in `r13-24304` |
| Input | the title channel, `0x8023D95C` (7 rows) | channel of 5 rows at `0x8024EDD8` | channel of 6 rows at `0x80252C68`, state `r13-24052` |
| Picture | texture 2 of `UMnMenu.cmp` | the Tome book | the Tome book |

[read: `0x80101624`-`0x8010164C`, `0x80115FEC`-`0x801160C0`; Tome page list in 03-tome-options.md:129,152]

What they share [read]:
- The settings words: widescreen `r13-24660` (title row 2 at `0x8001AAD4`; the Tome's at `0x80116CE8`), brightness
  `r13-24664`, audio mode `r13-23012` (title `0x8001ABF4`, Tome `0x80115880`), volume.
- The Volume page `0x80114FA4(x, y, z)` / `0x80115088` / `0x80115120`: title row 4 calls it with x 102, the Tome's
  Audio page with x 140 (`0x80115878`).
- The brightness slider's stick handler `0x801160D4` is the Tome Display page's; the title installs it in its own
  row 6 (`0x8001ABAC`).
- One difference: the Tome's widescreen toggle also reloads the Tome book (`0x80116CF8`); the title's does not
  (the book loads at Start/Load/Jump, `0x8002089C`) [read].

---

## 5. What the host already does on the title

| Host code | What | Evidence |
|---|---|---|
| hle.c:200-283 `skip_intro_*` | taps A through the intro; saves `saves/title.state` 2 s after the menu is up (`ED_TITLE_STATE`) | hle.c:247-263 |
| frontend.cpp:474-551 | Mods' Skip intro: loads that state, or boots with ED_SKIP_INTRO + ED_TITLE_STATE; black cover meanwhile (`update_start_up_cover`, frontend.cpp:4285) | read |
| hle.c:3911-4014 | Load Game passes the card screen over (`CARD_STAGE 0x8061FE28`, presses A or B); Jump to Game's card screen passed (`0x8001CCC8` notify) | read |
| hle.c:3927-3933 | card frame draw `0x8019C9C8` skipped from the card screen | read |
| hle.c:6035-6268 | the game's widescreen word kept equal to the launcher's Aspect ratio; a change made in the PAL Tome menu is taken back (`0x80116470` notify) | read. It does not watch the title's own toggle (`0x8001AAD4`): the next frame's sync just writes the launcher's value back [inferred] |
| titlevideo.cpp | swaps texture 1 (`kTitleHash`, English) for the video with the painted words lifted; keeps texture 2 (`kOptionsHash`) in `res/video/options.rgba`; rebuilds the Load screen | read |
| buttons.cpp:276-330 | the one texture-override table: claim by format+size, then hash; `generation` per format+size keys the cache | read |

Hooks go through `hle_dispatch_entry(addr)` (hle.c:5406): `notify` runs before the function, `intercept` replaces
it. They fire only where the guest *calls* that address (a `bl` or a pointer call), so a function entry or a
channel handler works, a case label inside a function does not [inferred from hle.c's use; every hook in hle.c is a
function entry].

The launcher's pages are RmlUi (`res/ui/menu.rml`, frontend.cpp:1-20); only overlays like toasts and the Tome
button are Dear ImGui (frontend.cpp:3318-3370). The in-game menu opens from a flag the game thread sets and pauses
the game while it runs (`g_open_characters`, frontend.cpp:4257, 4380-4398).

---

## 6. Words the game already has that fit

The menu words are hand-lettered pictures, not a font. Only words on the disc can be used as they are:
- **"Settings"** is inside "Save Settings": painted (unlit) in the Options picture (texture 2, row 0) and glowing
  in texture 48. It is the only fitting menu word already lettered by the game (seen in the decoded textures).
- Other painted words: Start Game, Load Game, Options, Jump to Game, Credits, Subtitles, Widescreen, Rumble
  Feature, Volume Adjustment, Brightness Adjustment, Audio Mode, Language Select.
- Any other label (Co-op, Mods, Characters) must be drawn by the host. The nearest look is the game's handwritten
  font, redrawn sharp in `res/fonts/game/tome-*` (fonts.cpp:1-20), tinted to the words' pale teal, with a white
  glow for the chosen one [inferred: a style match by eye, not checked].

---

## 7. Ways to add an entry that looks native

### Way A: a 6th word on the main menu, under Credits

The player presses Down from Credits (or from Options when Credits and Jump are hidden) and the game's own
cursor lands on it. A opens the launcher's settings over the title.

| # | Hook | Address | What it does |
|---|---|---|---|
| A1 | notify | `0x8001B588` (Up/Down) | if TM+62 == 7, write TM+40 (`0x8030914C`) = 6. Do it on every call: `0x8001FA38` and others write 5 again |
| A2 | notify | `0x80019F30` (A/Start) | if TM+62 == 7, TM+28 == 5 and no card notice: ask the frontend to open the settings (a new `frontend_open_settings()`, built like `frontend_open_characters`). The game itself plays 633 and stops the whisper |
| A3 | intercept | `0x8019D3B4` and `0x8019C7C8` | when `lr == 0x8001E430` (resp. `0x8001E418`) and `r28 == 5`: return without drawing. Without it the game draws texture 11 (a 2x2 blank, near-black) stretched over quad 5 = x 64..257, y 200..246, a dark box on the left of the title [read: tex 11 bytes `0020 0000 ffffffff`; quad 5 dumped]. `r28` is the cursor argument of `0x8001DF90`, so the cross-fade draw is covered too |
| A4 | texture | titlevideo.cpp, texture 1 (`kTitleHash`) | paint the word under Credits (about x 440..600, y 425..470 in menu space, so 546..706 in the picture) unlit; and glowing while TM+28 == 5 (make `titlevideo_generation` change with that, so 850x480 is rebuilt). Today texture 1 is only claimed while the title video is on (titlevideo.cpp:451); it must be claimed always |

Notes:
- With 0 bits in `0x80308A88` (a new card) Credits and Jump are hidden; the word can move up into Credits' place
  (read the bits as the game does) [inferred].
- Row 5 has no whisper. 723 or 631 could be played by the host, or left silent.
- The spare 2x2 textures cannot carry the new glow: they are all identical, so a hash swap would change all of
  them (section 3.2).

### Way B: a 9th row in the title Options, under "Language Select"

| # | Hook | Address | What it does |
|---|---|---|---|
| B1 | notify | `0x8001B588` | if TM+62 == 14, write TM+40 = 9 (the screens write 8 back at `0x8001A618`, `0x8001ADE0`, `0x8001B25C`, `0x8001B2B8` and more) |
| B2 | notify | `0x80019F30` | if TM+62 == 14, TM+28 == 8, no card notice: open the settings. The game plays 636 by itself |
| B3 | none | | the game draws no glow for row 8 (`0x8001D934`), so nothing to hide |
| B4 | texture | titlevideo.cpp, texture 2 (`kOptionsHash`) | paint the label under "Language Select" (picture x about 175..420, y 425..465) and its glow while TM+28 == 8. Today texture 2 is kept unchanged (titlevideo.cpp:476-478) |

Why B is the safer first step:
- No idle clock on Options (section 3.4), so a long stay in the overlay cannot drop the title into the movie.
- No bogus draw to hide.
- The overlay can wear the Options screen itself: `res/video/options.rgba` is already kept (titlevideo.cpp:179-222).
  Rows in the game's handwriting, teal, the chosen row glowing as the game's words do.
- Label: "Settings" would sit oddly under "Save Settings". A host-drawn label is needed (section 6).

### Way C: Y on the title, no new row

- The Y binding's handler `0x8001B920` and the X/Y/Z one `0x8001AE24` are empty `blr`s. A notify on
  `0x8001B920` fires only when the game itself takes a Y press on the title (fade done, channel live).
- With a game-style hint ("Y  Settings") drawn like the Tome's button (`frontend_tome_button`).
- Cheapest (one hook). Least native: the game never shows button hints on its title.

### Way D: real settings rows inside the title Options

- Left/Right are free on Options (section 2.4), so host rows could change values in place.
- But the page has room for one more line only, and each row needs its own word picture. Not practical beyond
  one or two rows. Use Way B's row to open the launcher's pages instead.

---

## 8. Risks

| Risk | Why | Fix |
|---|---|---|
| A press leaks into the title | The A that opens the overlay and the B that closes it are still held when the game resumes. B on Options leaves Options (may ask to save) | Hold the pad's buttons off until released after closing, as `g_coop_p2_hold` does for player 2 (hle.c:4025) |
| The attract movie right after closing (Way A) | The idle stopwatch measures guest time; the guest clock follows the wall clock (tools/ed/jtg.sh:21). Over 180 s in the overlay → state 23 next frame | On closing, clear TM+48 bit 0x2 (`0x80309154`): the game restarts its own clock next frame (`0x8001C568`) |
| Row count written back | Several screens write TM+40 (= 5 or 8) | Re-write it in the Up/Down notify, every call |
| Card notices | Up/Down and A go to a card notice when `0x800AF00C()` is non-zero | Skip the hooks then (guest-call it, or test its word once found) |
| Other languages | The title picture and the quads differ per pack (`UMnMenuF/G/S/I.cmp`); `kTitleHash` is English only | English first; per-language art later |
| 4:3 output | The picture is drawn on x -106..745 and cropped at 0..639 | Keep the new word inside picture x 106..745 |
| Low on screen | Below Credits the word ends near y 470 of 480 | Fine on a PC screen; tight on a TV's safe area [inferred] |
| Settings changed in the overlay | Title Options compares its snapshot on B (`0x800AD368`) and asks to save to the card | Native behaviour; say so to the user |
| Hook points | `0x8001B588`, `0x80019F30`, `0x8001B920` are reached through the channel by pointer; their dispatch firing is [inferred] | Check each with a `[trace]` line in a first run |

---

## 9. Files and addresses to use

| Need | Address or file |
|---|---|
| State byte / cursor / row count | `0x80309162` / `0x80309140` / `0x8030914C` |
| Fade (input live at 255) | `0x80309138` |
| Idle flag (bit 0x2) | `0x80309154` |
| Up/Down, A, B, Y handlers | `0x8001B588`, `0x80019F30`, `0x8001AE28`, `0x8001B920` |
| Main menu draw, glow call sites | `0x8001E2F0`; `0x8019C7C8` at `0x8001E414`, `0x80018824` at `0x8001E41C`, `0x8019D3B4` at `0x8001E42C` |
| Options draw, no glow above row 7 | `0x8001D52C`, `0x8001D934` |
| Back to main menu | `0x8001FA38` |
| Shown-rows bits | `0x80308A88` |
| Title / Options picture hashes | titlevideo.cpp:163 `kTitleHash`, :182 `kOptionsHash` |
| Pack textures | `UMnMenu.cmp` (unpack: `ppcemu.Unpacker(disasm).unpack(...)`, about 85 s) |

---

## Open questions

1. Do the notifies on `0x8001B588`, `0x80019F30` and `0x8001B920` fire? They are reached through the channel table
   by pointer. One traced run settles it.
2. Way A or Way B, or both? And the label: "Settings" (the game's own lettering, main menu only), or a host-drawn
   word such as "Mods" or "Co-op"? The user decides.
3. Is the near-black box at quad 5 what a 6th row really shows without hook A3? A run with only A1 shows it.
4. Is the overlay's background the title (the menu dimmed) or the Options picture? The user decides.
5. What `0x80308A88`'s bits are (games finished per alignment?) and what flag 894 is. Not needed for the hooks.
6. M1 in docs/coop-glitch-todo.md:27 ("title menu fires a press by itself after about a minute"): the game's own idle
   is 3 minutes and plays a movie, not a press. So M1 is likely not the game's idle [inferred]; a cold-boot run with
   the title left alone would tell.
7. Should a widescreen change in the title Options become the launcher's Aspect ratio, as the Tome menu's does
   (hle.c:6255)? Today the host writes the launcher's value back [inferred].
8. States 8, 18, 19, 25, 27, 29, 33, 250-254 were not read. None is on the main menu or Options path.
