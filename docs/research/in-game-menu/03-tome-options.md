# The Tome's Options page, its cursor, and a fifth cursor stop

Research for the user's ask (2026-11-17): bring the launcher's settings into the game, in the Tome, looking native. The first
piece is "Choose Character" as a real cursor stop on player 1's Options page: Down from Load/Save selects it, the game's ring
moves to it, A opens, Up returns.

How to read the tags: **[read]** means read in `build/ed-db/disasm.txt` or `generated/main.dol` (data), or in the file named.
**[seen]** means seen in a picture already in the repo. **[inferred]** means not verified. Nothing here was run.

Absolute addresses of `r13` words use `r13 = 0x80625420` and `r2 = 0x80628460` (set at `0x80005424`..`0x80005430`) [read].

---

## 1. Summary

- Options has **four entries in a cross**: 0 Controller (top), 1 Audio (left), 2 Display (right), 3 Load/Save (bottom).
  [seen: `Screenshots for Claude/tome-characters/options-zoom.jpg`; the ring rectangles below, read from the DOL]
- **The labels are not text.** They are painted into the Options page picture: sprite 27 of the Tome texture bank
  `r13-24316`, drawn over the whole page area. Each label already has its dark ring painted around it. [read: `0x801117F4`;
  seen]
- **The cursor index lives in a small list object**: its pointer is at `r13-24544` (`0x8061F440`), the index at `+12`.
  [read]
- **The game's only per-frame drawing of its own on Options is the gold ring**: `0x8019CC10(x, y, w, h, -1, 3)` around the
  selected entry, from a 4-row table at `0x8023A130`. [read]
- **Up always goes to 0, Down always to 3, Left to 1, Right to 2.** There is no wrap. Down on Load/Save does nothing (no
  sound). [read]
- **A** calls the page's exit (`0x801016D8`) and then the sub-page entry for the index. Index 4 or more gets the exit and
  no new page, which would leave the Tome with no page. So A on a fifth entry must be taken by the host. [read]
- **L and R** go straight to the neighbour page (Journal, Inventory). There is no page-turn animation in the code; a sound
  (495) plays. [read]
- **The world is paused** because the game loop calls only the Tome's frame (`0x80111234`) while the mode is 3. [read:
  `docs/coop-two-tomes.md` table; hle.c `TW_TOME_FRAME`]
- **Plan (section 9):** five host intercepts on the Options page's own functions, all guest addresses below. No guest code
  changes. A label drawn as the game draws words, under Load/Save.

---

## 2. Where the Options page is, and its state

### 2.1 Entry, exit and the words it sets

`0x80101600` is the Options page's entry (the tab's "enter"). It runs once when the page is entered. [read]

| Step | Address | What it does |
|---|---|---|
| Wait for loads | `0x8010160C`..`0x80101620` | Spins until `r13-24276` and `r13-24284` are non-zero. These are Tome load flags, set to 1 at `0x80111B28` and `0x801102B8`.. [read] |
| Page frame | `0x80101638` | `r13-24304` (`0x8061F530`) := `0x80101980` (the frame) [read] |
| Page exit | `0x80101648` | `r13-24308` (`0x8061F52C`) := `0x801016D8` (the exit) [read] |
| Input channel | `0x8010164C` | `r13-24252` (`0x8061F564`) := `0x8013DB3C(5, 0x8024EDD8, 0)`: 5 binding rows, pad 0 [read] |
| Cursor object | `0x80101654` | `r13-24544` (`0x8061F440`) := `0x801DD2BC()`, a 36-byte list object from the pool `r13-22200` [read] |
| Right hint | `0x80101678` | `r13-24264` (`0x8061F558`) := `0x801DB374(r13-26488, 1, 2, 0, 1, 0x8024EE78)` [read] |
| Left hint | `0x8010169C` | `r13-24268` (`0x8061F554`) := `0x801DB374(r13-26488, 1, 9, 0, 1, 0x8024EE78)` [read] |
| Range | `0x801016B4` | `0x801DD31C(obj, 0, 3, 3)`: first 0, count 3, 3 shown [read] |
| Index | `0x801016C4` | `0x801DD354(obj, 0, 0)`: index 0, top 0. **Every entry to Options starts on Controller.** [read] |

The two hints are game text elements (words), made from the prompts pack (`r13-26488`, `0x8061ECA8`), group 1
BOOK_BUTTONS [read: `build/ed-db/text.json`]:

| Element | Entry | Label | Text |
|---|---|---|---|
| `r13-24264` | 1,2 | RIGHT_INVENTORY | `\i17~5 Inventory` |
| `r13-24268` | 1,9 | LEFT_CINEMATIC | `~6\i16 Journal` |

The style name at `0x8024EE78` is the string "ButtonText" [read: DOL bytes `42757474 6f6e5465 7874`]. (hle.c's
`PROMPT_STYLE 0x80252D68` is another copy of the same name, used by the prompts.)

`0x801016D8` is the exit. It frees the cursor object (`0x801DD2F4`), closes the channel (`0x8013DB94(r13-24252)`), writes
`r13-24308 := 0`, and frees both hint elements (`0x801DA4D8`), writing 0 to `r13-24264` and `r13-24268`. [read:
`0x801016D8`..`0x80101728`]

### 2.2 The cursor object (made by `0x801DD2BC`)

36 bytes, cleared by `0x801DD280`, then `+20 := 1`. [read]

| Offset | Meaning | Set by |
|---|---|---|
| `+0` | first index | `0x801DD31C` |
| `+4` | last index, `max(first, first + count - 1)` | `0x801DD31C` |
| `+8` | how many are shown | `0x801DD31C` |
| `+12` | **the current index** | `0x801DD354` writes it with no clamp; `0x801DD564` reads it |
| `+16` | the top of the shown window | `0x801DD354`; `0x801DD56C` reads it |
| `+20` | step (1) | `0x801DD280`; `0x801DD340` sets `+20..+32` |
| `+24` | flags (bit 0 wrap, bit 1 clamp the window) | `0x801DD340` |
| `+28` | a filter function, or 0 | `0x801DD340` |
| `+32` | a "changed" function, or 0 (0 on Options) | `0x801DD340`; called by `0x801DD354` when the index changes |

- `0x801DD554(obj)` returns `+12 - +16`, the index on screen. Options draws by this. [read]
- `0x801DD39C(obj, delta)` is the clamped/wrapping step. **Options never calls it.** Its handlers write absolute indexes
  with `0x801DD354`. [read]
- So Options' range is set to 0..2, but index 3 works because `0x801DD354` does not clamp. An index of 4 is accepted the
  same way. [read]

**Where the host reads the index:** `obj = mem_read32(sda - 24544)`, then `index = mem_read32(obj + 12)`. The object is
allocated, so its address changes on every entry to Options. [read]

---

## 3. The bindings: how the buttons move the cursor

The channel's table is `0x8024EDD8`, 5 rows of 32 bytes. Row layout: `+0` mask, `+4` 0 pressed / 1 held, `+8` handler,
`+12` its r4, `+16` dead zone, `+18` multiplier (the r3 range). [read: DOL; layout from `docs/game/research/verbs.md` line
236, and the dispatcher `0x8013E160`]

| Row | Mask | Buttons | Handler | `+16`, `+18` |
|---|---|---|---|---|
| `0x8024EDD8` | `0x03001000` | L trigger, R trigger, Start | `0x801018C0` | 0, 1 |
| `0x8024EDF8` | `0x00000200` | B | `0x8010180C` | 0, 1 |
| `0x8024EE18` | `0x00000100` | A | `0x80101838` | 0, 1 |
| `0x8024EE38` | `0x10030000` | D-pad left/right, stick x | `0x8010179C` | 8191, **+1** |
| `0x8024EE58` | `0x200C0000` | D-pad up/down, stick y | `0x8010172C` | 8191, **-1** |

All five are "pressed" rows (`+4` = 0): one call per press, no repeat. [read: `0x8013E2E0` path]

The pad bits: `0x10000000` D-pad x, `0x20000000` D-pad y, `0x00010000`/`0x00020000` stick x -/+, `0x00040000`/`0x00080000`
stick y -/+, `0x01000000` L trigger, `0x02000000` R trigger. [read: `0x8013DC74`, `0x8013DF40`; matches verbs.md line 234]

**The value a direction row passes in r3** comes from `0x8013DF40`. D-pad Left and Down give a negative raw value, Right and
Up a positive one (`0x8013DF88`..`0x8013DFD4`: pad bit 0x1 Left and 0x4 Down negate). The row's multiplier then scales it to
-1 or +1 (`0x8013E03C`..`0x8013E148`). The vertical row's multiplier is -1, so **Up gives -1 and Down gives +1**. The
horizontal row's is +1, so **Left gives -1 and Right +1**. [read; the sign through the rounding was worked by hand, so
the final sign is inferred until a run logs it]

### 3.1 The handlers

| Handler | Code | Result |
|---|---|---|
| Up/Down `0x8010172C(dir)` | `dir < 0` (Up) -> index 0; `dir >= 0` (Down) -> index 3 | `0x801DD354(obj, i, 0)`; if the index changed, sound `0x8019DB90(545, 50)`; then `0x8013E154()` [read] |
| Left/Right `0x8010179C(dir)` | `dir < 0` (Left) -> 1; `dir >= 0` (Right) -> 2 | same sound rule, same `0x8013E154()` [read] |
| A `0x80101838` | sound `0x8019DB90(546, 100)`; exit `0x801016D8()`; then by index: 0 `0x80114894`, 1 `0x80115608`, 2 `0x80115FEC`, 3 `0x801140D4`, anything else nothing; then `0x8013E154()` [read] |
| B `0x8010180C` | exit `0x801016D8()`; `r13-24244` (`0x8061F56C`) := 2 (close the Tome); `0x8013E154()` [read] |
| L/R/Start `0x801018C0` | see section 6 [read] |

`0x8013E154` writes `r13-23564 := 1` ("input taken this frame"), so later rows of the same step don't fire. [read]

**What each direction does from each entry:**

| From | Up | Down | Left | Right |
|---|---|---|---|---|
| 0 Controller | stays (no sound) | 3 Load/Save | 1 Audio | 2 Display |
| 1 Audio | 0 | 3 | stays | 2 |
| 2 Display | 0 | 3 | 1 | stays |
| 3 Load/Save | 0 | **stays, no sound** | 1 | 2 |

[read: from the handlers' absolute writes]

### 3.2 The four sub-pages, and coming back

| Index | Entry | Its frame (`r13-24304`) | Its exit (`r13-24308`) | Its bindings | Back to Options with |
|---|---|---|---|---|---|
| 0 Controller | `0x80114894` | `0x80114BD8` | `0x801148E0` | `0x80252B00` (5 rows) | `0x80114914`, `0x80114ABC`: `0x80101600` then `0x801DD354(obj, 0, 0)` |
| 1 Audio | `0x80115608` | `0x80115950` | `0x801156A0` | `0x80252BAC` (5) | `0x801156DC`, `0x801157B4`: index 1 |
| 2 Display | `0x80115FEC` | `0x80116470` | `0x80116214` | `0x80252C68` (6) | `0x80116258`, `0x80116330`: index 2 |
| 3 Load/Save | `0x801140D4` | `0x8011440C` | `0x80114200` | `0x80252A48` (5) | `0x801143D8`: index 3 |

[read: the `lis/addi` pairs at each entry; the returns at `0x801143E8`, `0x80114938`, `0x80114ABC`, `0x80115700`,
`0x801157B4`, `0x8011627C`, `0x80116330`]

- The names per index come from the picture: Controller top, Audio left, Display right, Load/Save bottom [seen]. Index 3
  being Load/Save also fits its memory card calls (`0x800AEE4C`, `0x800AFC20`, flags 475, 476, 823) [read]. The other
  three names are by position only [inferred].
- The backs of 0, 1 and 2 first save the options block (`0x800AEFC8`, then `0x800AD218(0x80335768)`) [read]. What is in that
  block was not read.
- The pattern to copy: **a sub-page leaves by calling its exit, re-entering Options with `0x80101600`, and putting the
  cursor back with `0x801DD354(obj, its index, 0)`.** A host page that leaves the Tome page in place doesn't need this.

---

## 4. How the page is drawn

### 4.1 The Tome's frame calls the page's frame

`0x80111234` is the Tome's frame (hle.c `TW_TOME_FRAME`). In order [read: `0x80111234`..`0x8011172C`]:

1. On the first frame (`r13-24228` set) it picks the page to open (section 6.2).
2. `0x80024AF0()`, the words tick.
3. Input: if `r13-24256` (`0x8061F560`, a sub-channel) is set, `0x8013E160(r13-24256)`; else `0x8013E160(r13-24252)`, the
   page's channel. **Only one channel is stepped a frame.**
4. Texture bank `0x801E1B74(r13-24320)`, then `0x8019CFD4(5)`.
5. A pulse: `0x8019CF68(382, -128, r13-29050, r13-29048)`, clamped to 0..255 as `alpha` (`0x80111554`..`0x80111580`).
   `r13-24296` flips each time the pulse hits 382.
6. **The page frame:** `(*r13-24304)(alpha, r13-24296 byte)` at `0x80111590`.
7. Each hint element's alpha byte (`0x801DA230(elem) + 3`) := `alpha` for `r13-24264` and `r13-24268`. This is the corner
   hints' pulse.
8. `0x801DA574()` at `0x801115D0`: **all game text elements are drawn after the page frame**, so a text element sits on top
   of the page art and the ring.

### 4.2 The Options frame, `0x80101980(alpha, toggle)`

The whole function [read: `0x80101980`..`0x80101A30`]:

```
copy the 32-byte ring table at 0x8023A130 to the stack
i = 0x801DD554(*(r13-24544))          ; the index on screen
0x801117F4(0, alpha)                   ; the page's backdrop and art, section 0
x, y, w, h = table[i]                  ; 4 shorts each
0x8019CC10(x, y, w, h, -1, 3)          ; the gold ring, z -1, 3 units thick
```

**The ring table, `0x8023A130`** (640x480 screen units) [read: DOL]:

| Index | Entry | x | y | w | h | Right edge | Bottom edge |
|---|---|---|---|---|---|---|---|
| 0 | Controller | 294 | 43 | 120 | 32 | 414 | 75 |
| 1 | Audio | 217 | 62 | 72 | 32 | 289 | 94 |
| 2 | Display | 420 | 62 | 86 | 32 | 506 | 94 |
| 3 | Load/Save | 292 | 87 | 121 | 32 | 413 | 119 |

**Hazard:** the table has 4 rows. With index 4 the frame reads 8 bytes past the copy (into the saved r30/r31 on the stack)
and draws the ring at a garbage place. A fifth entry needs the frame taken over (section 9). [read]

### 4.3 The backdrop, `0x801117F4(section, alpha)`

[read: `0x801117F4`..`0x80111B24`]

- `r13-29060` (`0x8061E29C`) := section. This is "the last page shown". The Tome reopens on it (section 6.2).
- Sections: 0 Options, 1 Inventory, 2 Map, 3 Magick, 4 Journal (the call sites: `0x801019FC` 0, `0x80105FF8` 1,
  `0x80112D60` 2, `0x8010F294` 3, `0x80109554` 4). The four Options sub-pages also call it with 0.
- Layout: `0x8019D358(texcoords, positions)` sets indexed arrays: texcoords at `0x80252714` (`0x80252238 + 1244`),
  positions at `0x80252324` (`+236`), or at `0x8025251C` (`+740`) when `r13-24660` (`0x8061F3CC`) is 1. The `+740` table
  pushes the outer wings out to x -106 and 745, a 16:9 picture. So `r13-24660 == 1` is the game's own widescreen mode
  [inferred from the numbers].
- `0x8019D3B4(tex, pos, 5)` draws one quad by index. A "slot" is 4 position indexes; each position is 3 shorts (x, y, z).
- `0x8019C7C8(&colour, field, value, 0x80000000)` builds a 32-byte 2D state block from the template at `0x8023B060`, sets one
  short of it (field 0 = the texture number in the bound bank), and sends it with `0x801E21FC(block, 0, 703, 0)`.

Slots that matter for Options (normal layout) [read: DOL]:

| Slot (index) | Rectangle (x, y)-(x, y) | z | Drawn for Options with |
|---|---|---|---|
| 0..4 (0..16) | small 16x12 quads on an arc, (114,164) (129,202) (142,239) (139,269) (116,299) | -1 or -30360 | the current section's marker on the medallion: slot = section (`0x80111AEC`), hidden while the blink counter `r13-24295` is above 0 |
| 7 (28) | (144,0)-(575,479), 432x480 | -30360 | **texture 27 of bank `r13-24316`: the Options page picture, the four painted labels and their dark rings** |
| 12, 13 | the left and right wings | -1 | the Tome's covers |
| 16 (64) | (10,0)-(143,164) | -1 | texture 32 of bank `r13-24316`, with the pulse's alpha |
| 18 (72) | (10,320)-(143,480) | -1 | texture 50, only when `r13-24268` is 0 (it isn't on Options) |

For section 0 the code binds bank `r13-24316` (`0x8061F524`) at `0x80111940` and picks textures 32, 50, 27 (`0x80111930`..
`0x8011193C`). Texture 27 at index 28 is drawn at `0x801119BC`..`0x801119CC`. [read] That texture 27 is the painted page is
[inferred] from its rectangle being the 432x480 page area (the same size tome.cpp gives the Tome's pages) and from the
picture: the labels and their dark rings sit on the parchment [seen].

### 4.4 The rings

| Function | Use | Colour (`r2` word) | Effect set first |
|---|---|---|---|
| `0x8019CC10(x, y, w, h, z, t)` | the gold selection ring (Options, Inventory, Magick, the sub-pages: 30 call sites) | `0x646400FF` at `r2-20596` (`0x806233EC`) | `0x801E1D74(1, 1282, 1, 0, 0)` with f1 = 2.0 (`r2-20592`) |
| `0x8019CB6C(x, y, w, h, z, t)` | a dark ring (one site: Inventory `0x80106268`, z -30359) | `0x320000FF` at `r2-20608` (`0x806233E0`) | `0x801E1D74(1, 1058, 1, 0, 0)` with f1 = 1.5 (`r2-20604`) |
| `0x8019C9C8(x, y, w, h, z, t, &colour)` | the frame itself, no effect | the caller's | none |

[read: `0x8019CB6C`..`0x8019CCB0`, constants from the DOL]

- `0x8019C9C8` draws four bars (16 vertices, `GXBegin` via `0x8021AF5C(0x80, 5, 16)`), each `t` units thick, at depth `z`,
  vertices written by `0x8019D6F0(x, y, z)` to the gather pipe. [read]
- Both ring functions put the effect back with `0x801E1D74(0, 2, 1, 0, 0)` (f1 = 1.0) and call `0x8019CFD4(5)`. [read]
- The "dashed" look the user sees on the gold ring [seen: `options-button.jpg`, yellow and dark dashes] is [inferred] to
  come from `0x801E1D74`'s mode 1282: its case 2 builds a scaled matrix from f1 (`0x801E1EF8`..`0x801E1F90`), likely a
  pattern texture's. Not read further. Calling `0x8019CC10` itself gives the same look whatever it is.
- The unselected entries have **no ring drawn by code**. Their dark rings are painted in the page picture. [read: the frame
  draws one ring only; seen]

### 4.5 The font sheets (`res/fonts/game/tome-*`)

[read: `runtime/host/fonts.cpp` 1-140, 270-367; `tools/ui/make_game_fonts.py` header]

- The game's words come from two 512x512 CMPR sheets in `UFonts.tpl`, 32x32 cells. Character `c` sits at column `c >> 4`, row
  `15 - (c & 15)`, stored upside down. Sheet 0 is the plain font (white with a black outline). Sheet 1 is the handwritten one
  (white); its codes 0x01-0x1F are button pictures.
- `tome-0.png` and `tome-1.png` (2048x2048, 128 px a cell) are this project's redraws: sheet 0 in IM Fell English, sheet 1 in
  La Belle Aurore. `tome-N.cells` is 256 characters, `1` where the cell was redrawn.
- **The host does not draw text with them.** `fonts_apply` swaps them in when the game decodes its own font sheet
  (recognised by `signatures.txt`), when the setting `game_font` is 1, the default (`settings.c` line 18). So anything the
  game draws as words is drawn with these sheets.
- The current "Y Choose Character" plaque is ImGui text in IM Fell English (`frontend.cpp` 3161-3164, 3318-3379), not the
  sheets.
- `~N` in game text is replaced by the byte N (`0x801D9F08`..`0x801D9FA0`), so `~5` and `~6` in the hints are the sheet's
  button-picture cells. `\n` is a new line (`0x801D9EC8`). Which code picks sheet 0 or sheet 1 was not found.

**So the way to get "the game's own Tome font" for a label is to make it a game text element.** Then the game lays it out
with its own metrics and the host's sheet replaces the letters as for all other words.

---

## 5. Making a game text element from the host

The host already does this for player 2's prompts (hle.c `coop_prompt_show` ~22961, `coop_prompt_style` ~22950) [read]:

| Call | Address | Arguments |
|---|---|---|
| Make | `PROMPT_MAKE 0x801DB374` | (text bank, group, entry, flags, 1, style name). Picks the first free of ten elements at `0x805F87D0` (0x660 bytes each). Returns the element. |
| Rewrite its words | `PROMPT_FORMAT 0x801DA8CC` | (element, format, R, G, B, A, argument): the words from a printf format, with a colour |
| Free | `PROMPT_FREE 0x801DA4D8` | (element) |

Element fields (from `docs/coop-level3-design.md` lines 207, 546-548) [read there]: `+7` alpha, `+8` x, `+10` y, `+16` flags
(0x40 fading in, 0x80 fading out, 0x4000 made). The x and y are [inferred] in the same 640x480 units as the ring table.

**Element budget:** there are ten. On Options, the two hints use two. A word window or prompt may use more. If none is free,
`0x801DB374` prints and the element has no text (design doc line 91). [read there]

---

## 6. The tabs

### 6.1 L and R, and the page ring

`0x801018C0` (Options' L/R/Start row) [read: `0x801018C0`..`0x8010197C`]:

1. `0x8013DC24(0x10, 1, 0)`: **if Z is held, nothing happens.**
2. Spins until `r13-24276`, `r13-24272`, `r13-24284` are all set (the Tome's loads).
3. Exit `0x801016D8()`, `0x8013E154()`.
4. Start pressed: `r13-24244 := 2`, close the Tome.
5. L trigger pressed (`0x01000000`): sound `0x8019DB90(495, 50)`, then Journal `0x80106FD4`.
6. Otherwise (R): the same sound, then Inventory `0x80102810`.

**There is no animation code.** The next page's entry is called straight away. Sound 495 is the page sound. [read]

The ring of pages, each page calling its neighbour's entry [read]:

| Page | Entry | Section | L goes to | R goes to |
|---|---|---|---|---|
| Options | `0x80101600` | 0 | Journal `0x80106FD4` | Inventory `0x80102810` |
| Inventory | `0x80102810` | 1 | Options (`0x80103C40`, return address `0x80103C44`) | Map `0x80111B40` (`0x80103C70`) |
| Map | `0x80111B40` | 2 | Inventory (`0x8011252C`) | Magick `0x8010AAF0` if `0x80109A68()`, else Journal (`0x80112540`..`0x80112554`) |
| Magick | `0x8010AAF0` | 3 | not traced | not traced |
| Journal | `0x80106FD4` | 4 | Magick if `0x80109A68()`, else Map (`0x801083B8`..`0x801083CC`) | Options (`0x801083E4`) |

The per-frame functions (`r13-24304`): Inventory `0x80105F9C`, Map `0x80112594`, Magick `0x8010F228`, Journal `0x8010952C`,
Options `0x80101980` [from `docs/game/systems/tome.md`; Options' read here].

hle.c already hooks the tab entries: `0x80101600` (`tome_options_page`), `0x80106FD4` (`tome_journal_page`), and notifies on
Inventory, Map and Magick (`tome_other_page`) (hle.c ~33018-33032, only when `tome_copy_mode() == 2`). Player 2's copy Tome
skips Journal and Options (`tome_copy_tab`, ~16876). [read]

### 6.2 Which page the Tome opens on

On the first Tome frame (`r13-24228` set; `0x801112D8`..`0x8011147C`) [read]:

1. `0x80109900()` true: Magick.
2. `r13-24448` set: Journal.
3. The help flags: unless every pair 694/864, 716/717, 599/600, 601/602, 603/604 is answered, Inventory.
4. Flag 823 (the scripted save offer): **Load/Save itself** (`0x801140D4`), not Options.
5. Otherwise by `r13-29060`, the last section: 0 Options, 1 Inventory, 2 Map, 3 Magick (or Inventory with no Magick), 4
   Journal. **Any other value opens no page.**

### 6.3 Could a sixth tab be added natively?

Possible, but not cheap, and not needed for this step. What it would take [inferred from the reads above]:

- **Links:** the ring is hard-coded in each page's L/R handler. Options' R (`0x801018C0`) and Inventory's L (the path to
  `0x80103C40`) would need host intercepts to go to the new page instead.
- **A page in the game's sense** is three words: `r13-24304` (frame), `r13-24308` (exit), `r13-24252` (channel), all guest
  addresses. The host would need guest code addresses to hang intercepts on (hle_dispatch_entry accepts any 4-aligned code
  address) and a binding table in guest memory.
- **Art:** `0x801117F4`'s switch has no case past 4. A new page must draw with section 0..4's look, or draw its own.
- **Hazard:** if `r13-29060` is ever left at 5, the next opening takes no branch at `0x80111410` and opens no page. A host
  page must call `0x801117F4` with 0..4 only.
- **The medallion's marker:** the five arc slots 0..4 have no sixth position.

The user's own direction (coop-todo.md line 245) is "Not new pages", so the cursor stop plus an overlay is the right shape.

---

## 7. How the Tome pauses the world

- The game loop (`0x800186E8`) calls the frame in `r13-26192` (`0x8061EDD0`): the world's `0x800083C8` in mode 2, the Tome's
  `0x80111234` in mode 3 (`r13-26240`, `0x8061EDA0`; the mode asked for, `r13-26188`). In the Tome the world's frame is not
  called at all, so everything in it stops. [read: hle.c ~13640-13656 comment and `TW_*` defines; `docs/coop-two-tomes.md`]
- In co-op the host runs both frames in turn (`tw_*`, `g_tw.sharing`), so the world goes on behind player 1's Tome. [read:
  hle.c comment ~13640]
- The launcher's in-game menu, opened by `frontend_open_characters`, is a modal loop on the game thread (`frontend.cpp`
  ~4380-4398: `report_pause(true)`, `hle_coop_ui_over(true)`, `run_menu()`). While it is up, nothing of the game runs,
  Tome and world alike. [read]

---

## 8. What the host has today for this

| Piece | Where | State |
|---|---|---|
| Options entered | hle.c `tome_options_page` on `0x80101600` (intercept, passes through) | sets `g_tome_on_options` for player 1 |
| Other tabs entered | `tome_journal_page` on `0x80106FD4`; `tome_other_page` (notify) on Inventory, Map, Magick | clear it |
| The button | `tome_options_button` (~16912), called from the pad read at ~4488 with player 1's pad | draws "Y   Choose Character" with `frontend_tome_button(text, 0, 0.54, 0.405)` in side-by-side co-op, a toast otherwise; Y within 4 frames opens `frontend_open_characters()`; Y is masked from the game |
| The plaque | `frontend.cpp` `draw_tome_button` (~3318) | ImGui, IM Fell English, parchment and a doubled dark rule |
| The roster | `frontend.cpp` ~4380 | opens straight on `Screen::Characters`; **its Back goes to `Screen::ModsPage`** (`frontend.cpp` 1059-1062, 2836-2839), not back to the Tome |

No hle.c code reads or hooks `0x8010172C`, `0x8010179C`, `0x80101838`, `0x80101980`, `0x801016D8`, `0x8023A130` or
`r13-24544` today. [read: grep]

---

## 9. The plan: "Characters" as a real cursor stop

Index 4 is the new entry. All work is host intercepts on the Options page's own functions, gated to player 1's real Tome.
No guest code or data is changed.

### 9.1 The gate (every hook)

```
on = sda_ready(cpu) && !g_tome_copy_turn && mem_read32(sda - 24304) == 0x80101980   // the Options page is the page
     && [the co-op/solo choice, see Open questions]
obj = mem_read32(sda - 24544); idx = mem_read32(obj + 12)
```

- `!g_tome_copy_turn`: player 2's copy Tome runs the same code in a memory copy. It skips Options today, but the hooks must not
  act there. Add the new intercepts to `tome_copy_hook_off` (hle.c ~32309), as every intercept that must stand aside in the
  copy's turn is listed there (hle.c ~5715).
- `r13-24304 == 0x80101980` is a sturdier "on Options" test than `g_tome_on_options`: it is the page's own word, written by
  the entry and by no sub-page. [read: the stores to `r13-24304`, section 3.2]

### 9.2 The five hooks

| # | Guest address | Kind | When `on` |
|---|---|---|---|
| 1 | `0x8010172C` Up/Down | intercept | `dir = (s16)r3`. idx 3 and `dir >= 0`: guest-call `0x801DD354(obj, 4, 0)`, `0x8019DB90(545, 50)`, `0x8013E154()`, return. idx 4 and `dir < 0`: `0x801DD354(obj, 3, 0)`, the same sound, `0x8013E154()`, return ("Up returns" to Load/Save, not to Controller). idx 4 and `dir >= 0`: `0x8013E154()` only (no sound, as the game does at an edge). Anything else: pass through. |
| 2 | `0x8010179C` Left/Right | pass through | From 4 the game's own code goes to Audio (1) or Display (2), as it does from Load/Save. Hook only if the user wants Left/Right dead on the new entry. |
| 3 | `0x80101838` A | intercept | idx 4: guest-call `0x8019DB90(546, 100)` (the game's choose sound), `0x8013E154()`, then `frontend_open_characters()`, return. **Do not run the game's A:** it would call the exit `0x801016D8` and then no page (section 3.1). Any other idx: pass through. |
| 4 | `0x80101980` the frame | intercept (always, while `on`) | Do the frame's three calls with the fifth entry in: guest-call `0x801117F4(0, r3)`; draw the new entry's dark ring when idx is not 4 (section 9.3); then the gold ring: idx 0..3 from `0x8023A130 + 8*idx`, idx 4 from the new rectangle, `0x8019CC10(x, y, w, h, -1u, 3)`. Make the label element on the first frame (no element yet) and set its alpha by idx. Return. When not `on`: pass through. |
| 5 | `0x801016D8` the exit | notify | Free the label element with `0x801DA4D8` and forget it. Every way off the page goes through here: A (`0x80101860`), B (`0x80101818`), L/R/Start (`0x80101908`). [read] |

Replacing the frame whole (hook 4) is safe because the original is only the table copy and those three calls [read:
`0x80101980`..`0x80101A30`]. It also removes the stack overread at index 4 (section 4.2).

### 9.3 Where and how to draw the entry

**Rectangle** (640x480 units, the ring table's): centred under Load/Save, the same height, a little wider for the word.
Proposed `x 282, y 124, w 141, h 32`. Load/Save's ring ends at y 119. The cross's rows step 19 and 25 units. [the numbers are
inferred; check by picture and nudge]

**Label, as the game draws words (recommended):**

1. Make: `0x801DB374(r13-26488, 1, 2, 0, 1, 0x8024EE78)`, the same call that makes the Inventory hint (an existing entry, so the
   element is laid out and styled as the Tome's hints).
2. Words: `0x801DA8CC(element, fmt, R, G, B, A, 0)` with `fmt` a guest copy of "Characters" (or "Choose Character") in free
   scratch. Candidates: the free part of the `SCRATCH_HUD` page, e.g. `0x81FFFE00` (`docs/research/separate-rooms/picture-readers.md`
   line 64 names it unused) [inferred free]. Colour: the hints' own, or the dark ink of the painted labels.
3. Place: `+8 := x`, `+10 := y` of the rectangle's text origin.
4. Look: selected, alpha `+7 := 255`; unselected, about 160 (translucent, as the user asked).
5. Size: the hints' size first. The painted labels are about 32 units tall; the hints' scale codes (`\s0.6` style) may be
   needed in the format to match.

The element is drawn by `0x801DA574` after the page frame (section 4.1, step 8), so it sits on the art and the ring, with the
host's `tome-*` letters (`game_font` 1).

**Unselected dark ring:** `0x8019C9C8(x, y, w, h, -1u, 2, &colour)` with a dark brown close to the painted rings, written to
guest scratch. Or `0x8019CB6C(x, y, w, h, -1u, 3)`, the game's own dark-red ring (with its effect). Pick by picture.

**Selected:** `0x8019CC10(x, y, w, h, -1u, 3)`, exactly as the game rings its four. Since hook 4 draws only one gold ring, "the
game's ring hidden meanwhile" holds by construction.

**Later, a fully painted fifth label (optional polish):** redraw texture 27 of bank `r13-24316` (the Options page picture) with
a fifth plaque painted under Load/Save, claimed at decode by its name hash, as `runtime/host/tome.cpp` does for the controls
page (`kPages`, `tome_claims`, ~546). Then no element and no dark ring are needed, only hooks 1, 3 and the gold ring at index 4.
The name hash is not known yet (Open questions).

### 9.4 Back from the roster straight to the Tome

- `frontend_open_characters()` opens `Screen::Characters` in the in-game menu's modal loop. The Tome stays on Options at index 4
  underneath, since hook 3 never left the page.
- Today the roster's Back goes to the Mods page (`frontend.cpp` 1059-1062 and 2836-2839). For "back straight to the Tome", a flag
  set by `frontend_open_characters` should make Back (and a choice, if wanted) set `g_close = true` instead. Then `run_menu`
  returns and the game goes on with the Tome as it was.
- A choice of character runs the built `characters_reload` (the game saved and loaded again), which closes the Tome anyway.

### 9.5 Retiring the Y button

When the stop works, remove the plaque call and the Y mask in `tome_options_button` (keep the toast for layouts not yet
measured, if wanted). The `ED_TOME_HINT_ANY` switch can stay for pictures.

### 9.6 More entries later (the launcher's pages)

The same hooks scale to a column under the cross: index 5, 6, ... with Down stepping 4 -> 5 -> ..., Up stepping back, each with
its own rectangle and element. The element budget (ten, two used by the hints) limits this to a few labels at once; a
painted page (9.3, later) has no such limit. Candidates from coop-todo.md line 245: Characters, Graphics, Co-op, Mods and cheats,
Save states.

### 9.7 Install

Next to the existing `0x80101600` hook (hle.c ~33018), with the same checks the file uses elsewhere: `hle_dispatch_entry(addr)`
not NULL and `intercept == NULL`, and the first instruction word as expected, for example `mem_read32(0x80101980) ==
0x9421FFD0` (`stwu r1, -48(r1)`), `0x8010172C`/`0x8010179C`/`0x80101838` == `0x9421FFF0` (`stwu r1, -16(r1)`). [the instruction
words are inferred from the disassembly text; check against memory]

### 9.8 Tests to run (after building)

1. Solo and side-by-side co-op: Tome, Options, Down, Down: the gold ring moves to the new entry; one move sound; the four
   painted labels show no gold ring.
2. Up from the new entry: back on Load/Save, with sound. Down again on it: nothing, no sound.
3. Left/Right from the new entry: Audio/Display (if hook 2 is left out).
4. A on it: the roster opens with the game paused; B returns to the Tome on Options with the new entry still selected; no A or
   B leaks into the Tome as a choose or a close.
5. A on each of the four: their sub-pages open and come back as before.
6. L/R and Start from the new entry: Journal, Inventory, close. Then reopen the Tome: Options again at Controller (the game's
   entry resets the index).
7. Player 2's copy Tome meanwhile: unchanged (it skips Options). `[tome-copy] stood aside` lines name the new hooks.
8. Look: the label is in the `tome-*` lettering at the painted labels' size, translucent when not selected.
9. `ed test` after the change, as CLAUDE.md asks.

---

## 10. Addresses at a glance

| Thing | Address |
|---|---|
| Options entry / frame / exit | `0x80101600` / `0x80101980` / `0x801016D8` |
| Options handlers: L/R/Start, B, A, Left/Right, Up/Down | `0x801018C0`, `0x8010180C`, `0x80101838`, `0x8010179C`, `0x8010172C` |
| Options bindings | `0x8024EDD8` (5 x 32 bytes) |
| Ring table | `0x8023A130` (4 x {x, y, w, h} shorts) |
| Cursor object pointer | `r13-24544` = `0x8061F440`; index at `obj+12` |
| Page frame / exit / channel words | `r13-24304` `0x8061F530` / `r13-24308` `0x8061F52C` / `r13-24252` `0x8061F564` |
| Sub-channel | `r13-24256` `0x8061F560` |
| Hint elements | `r13-24264` `0x8061F558` (Inventory), `r13-24268` `0x8061F554` (Journal) |
| Close request | `r13-24244` `0x8061F56C` := 2 |
| Last section | `r13-29060` `0x8061E29C` |
| Widescreen layout flag | `r13-24660` `0x8061F3CC` |
| Tome load flags | `r13-24272/-24276/-24280/-24284` `0x8061F550/54C/548/544` |
| Input taken | `r13-23564` `0x8061F814` (`0x8013E154`) |
| List object: make / free / range / set / on-screen / index | `0x801DD2BC` / `0x801DD2F4` / `0x801DD31C` / `0x801DD354` / `0x801DD554` / `0x801DD564` |
| Channel: open / close / step | `0x8013DB3C` / `0x8013DB94` / `0x8013E160` |
| Pad tests | `0x8013DC24` (mask, held?, pad), `0x8013DC74`, `0x8013DF40` |
| Sound | `0x8019DB90(id, volume)`: 545 move, 546 choose, 495 page, 611 sub-page back |
| Backdrop | `0x801117F4(section, alpha)` |
| Gold ring / dark ring / plain frame | `0x8019CC10` / `0x8019CB6C` / `0x8019C9C8` |
| Ring colours | `0x806233EC` gold `0x646400FF`, `0x806233E0` dark `0x320000FF` |
| Text element make / format / free / draw all | `0x801DB374` / `0x801DA8CC` / `0x801DA4D8` / `0x801DA574` |
| Tome frame | `0x80111234`, page frame called at `0x80111590`, elements at `0x801115D0` |
| Sub-page entries 0..3 | `0x80114894`, `0x80115608`, `0x80115FEC`, `0x801140D4` |

---

## Open questions

1. **Signs.** Down gives +1 and Up -1 to `0x8010172C`, by reading `0x8013DF40`'s rounding by hand. A run with a log line in hook 1
   (`dir`, idx before and after) settles it.
2. **The other three names.** Controller, Audio and Display by index are from their place in the picture. A run that presses A on
   each, or reading their frames' text, confirms them.
3. **Element units and size.** Whether `+8`/`+10` are in the ring table's 640x480 units, and which scale code makes the label the
   painted labels' size. Needs a picture.
4. **Which sheet.** Whether "Characters" should be in the handwritten sheet (La Belle Aurore, like the painted labels' hand) or the
   plain one (IM Fell English), and which text code selects it. The user's choice, then a picture.
5. **Where the hint elements' colour and size come from** (their entry's record floats). Not read.
6. **A free scratch word for the format string and the dark ring's colour.** `0x81FFFE00` is named free by one research note; check
   nothing else uses it now.
7. **The painted route.** The name hash of texture 27 of bank `r13-24316` (and which file: `UBookPak.bin` is likely, as for the
   controls page) needs a texture dump from a run.
8. **Solo, stacked halves, player 2.** The current button shows only side by side. The cursor stop has no layout dependence (the
   game draws it), so it could be on in solo and stacked halves too. The user decides. Player 2's own entry in their copy Tome
   needs the copy to stop skipping Options (`tome_copy_tab`) first.
9. **Left/Right on the new entry:** go to Audio/Display (the game's way from Load/Save), or do nothing. The user's choice.
10. **"Dashed":** whether the dashes on the gold ring come from `0x801E1D74`'s mode 1282. Only matters if the host ever draws the
    ring itself; calling `0x8019CC10` gives the game's look either way.
11. **The roster's Back.** Confirm by hand that B from the roster opened this way lands on the Tome with no stray press, once the
    Back route is changed.
