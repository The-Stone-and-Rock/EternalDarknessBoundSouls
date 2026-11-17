# The game's own Options pages, as templates for native-looking settings pages

Read only research, 2026-11-17. PAL disc (GEDP01). Addresses are guest addresses. "Read" means read in
`build/ed-db/disasm.txt` or `generated/main.dol`. "Seen" means the picture was decoded from the disc and looked at.
"Inferred" means not verified. Nothing here was run.

How the art was checked: `UBookPak.bin` (the English Tome art) was read from `dump/GEDP01.iso` with `tools/ed/disc.py`,
its first item (a TPL of 95 CMPR images) decoded and looked at. Each image's Dolphin hash was computed (XXH64 of the
image data). The method gives `0x8680aee4cb0d0a97` for image 31, the same hash `runtime/host/tome.cpp:39` names for the
controls page, so the other hashes below can be trusted.

## 1. The short answer

- **No Options page draws its words with the font.** Every label and value on the Tome's Options pages is a painted
  picture from `UBookPak.bin`. Words like "Controller", "Mode", "Mono", "Subtitles", "ON" and "16:9" are pictures (seen).
  The only font text on these pages is the L/R tab hints and the volume slider's "A Accept B Cancel" hint.
- **The selected value is shown by a second picture.** Each value has a "glow" twin. The game draws the twin over the
  chosen value, tinted `0xC8C800FF`.
- **The cursor is the same everywhere.** `0x8019CC10(x, y, w, h, -1, 3)` draws the selection frame round a rectangle
  given in 640x480 screen units.
- **Cursor sounds:** Options cross move 545 at volume 50; a list move 635 at 50; select 546 at 100; back 611 at 50;
  tab change 495 at 50. All are played with `0x8019DB90(sound, volume)`.
- **The title screen's Options reuse Tome code.** They call the Tome's volume slider (`0x80114FA4`/`0x80115120`), the
  Tome's brightness step (`0x801160D4`) and the same memory card dialog. The game's sub-page code works outside the Tome.
- **Best way to add a native row (inferred):** paint it into the page picture itself, by texture replacement of image
  27 (hash `0x7cd7b88669459bbb`) as `tome.cpp` does for image 31. Then move the game's own ring there by intercepting
  `0x8019CC10` when it is called from `0x80101A18`. Details in section 11.

## 2. How a Tome page works

Every Tome page sets the same few words. Options is the template.

| Word | What it holds | Evidence |
|---|---|---|
| `r13-24304` | the page's draw function, called each Tome frame with the fade alpha (0-255) in r3 | set at `0x80101638`; called at `0x8011157C` in the Tome frame `0x80111234` |
| `r13-24308` | the page's leave (cleanup) function | set at `0x80101648`; called by the Tome close at `0x80110E10` and at `0x80111750` |
| `r13-24252` | the page's input channel: `0x8013DB3C(count, binding table, 0)` | `0x8010164C` |
| `r13-24544` | the Options cross's cursor object | `0x80101654`, freed at `0x801016E8` |
| `r13-24316` | the Tome art: item 0 of `UBookPak.bin` (a TPL), language-specific | `0x80110864`; file names at `0x802522B4` |
| `r13-24320` | the book frame's art (second texture set) | `0x80110A38`; bound at `0x80111878` |
| `r13-24244` | 2 asks the Tome to close | `0x80101820` |

- **Binding rows** are 32 bytes: `+0` pad mask, `+4` 0 pressed or 1 held, `+8` handler, `+16/+18` the range of the
  r3 value the handler gets (`docs/game/research/verbs.md:236`). Every handler ends with `0x8013E154`, which sets
  `r13-23564 = 1` ("input taken").
- **Holding input:** `0x8013DBC8(channel)` holds a channel and `0x8013DBC0(channel, 0)` frees it (verbs.md:236). A host
  overlay could hold `r13-24252` to stop the Options page reacting under it (inferred).
- **Pad tests:** `0x8013DC24(mask, held, pad)` tests pad 0's pressed bits (`r4 = 0`) or held bits (`r4 = 1`).
  Read at `0x8013DC24`.
- **Generic list cursor** (36 bytes, made by `0x801DD2BC()` from the pool `r13-22200`, freed by `0x801DD2F4`). Read at
  `0x801DD280`-`0x801DD580`:

| Call | Does |
|---|---|
| `0x801DD31C(c, first, count, visible)` | `+0` min, `+4` max = first+count-1, `+8` visible rows |
| `0x801DD340(c, step, flags, can_select, on_change)` | flags bit 0 wraps; `on_change` runs when the index changes |
| `0x801DD354(c, index, top)` | sets the index (no clamping) and calls `on_change` if it changed |
| `0x801DD39C(c, delta)` | moves; skips rows `can_select` refuses; clamps or wraps |
| `0x801DD564(c)` / `0x801DD554(c)` / `0x801DD56C(c)` | index / index minus top / top |

The Tome's standard `on_change` is `0x801104AC`: it plays sound 635 at volume 50.

## 3. The Options page (the cross of four)

The page picture is image 27 (432x480). Its painted plaques are Controller at the top, Audio on the left, Display on
the right and Load/Save at the bottom (seen). The book frame and tab art come from `0x801117F4(0, alpha)`.

| | Address |
|---|---|
| Enter (runs once as the tab opens) | `0x80101600`. It waits on `r13-24276`/`r13-24284`, sets draw `0x80101980` and leave `0x801016D8`, opens channel 5 on table `0x8024EDD8`, makes the cursor, makes the two hints, sets the cursor to 0 |
| Draw | `0x80101980(alpha)`: `0x801117F4(0, alpha)`, then the ring `0x8019CC10(rect, -1, 3)` at `rects[0x801DD554(cursor)]`, with the 4 rects copied from `0x8023A130` |
| Leave | `0x801016D8`: frees the cursor, closes the channel, frees both hint elements (`0x801DA4D8`) |
| Host hooks today | `tome_options_page` intercepts `0x80101600` (`hle.c:16895`, installed at `hle.c:33020`) |

**Ring rectangles** (x, y, w, h, 640x480 units), from `0x8023A130`, read:

| Index | Plaque | Rect |
|---|---|---|
| 0 | Controller | 294, 43, 120, 32 |
| 1 | Audio | 217, 62, 72, 32 |
| 2 | Display | 420, 62, 86, 32 |
| 3 | Load/Save | 292, 87, 121, 32 |

**Input table `0x8024EDD8`** (read):

| Pad | Handler | Does |
|---|---|---|
| Start, L or R (`0x03001000`) | `0x801018C0` | Ignored while Z is held. Otherwise it leaves the page. Start closes the Tome; L goes to the Journal (`0x80106FD4`); R goes to the Inventory (`0x80102810`). The tab change plays sound 495 at 50 |
| B | `0x8010180C` | Leaves the page and closes the Tome (`r13-24244 = 2`) |
| A | `0x80101838` | Sound 546 at 100, leave, then by index: 0 `0x80114894`, 1 `0x80115608`, 2 `0x80115FEC`, 3 `0x801140D4`. **Index 4 or more: the page is left and nothing opens** |
| Stick x or D-pad x | `0x8010179C` | Negative goes to 1 (Audio), positive to 2 (Display) |
| Stick y or D-pad y (r3 times -1) | `0x8010172C` | Up goes to 0 (Controller), down to 3 (Load/Save). Sound 545 at 50, only if the index changed |

- **Cursor moves are direct sets, not steps.** Down on Load/Save sets 3 again: no change and no sound
  (`0x80101770`). There is no fifth stop to reach.
- **Hints:** `0x801DB374(r13-26488, 1, 2, 0, 1, "ButtonText")` gives "R Inventory" and `(…, 1, 9, …)` gives
  "L Journal". `r13-26488` is the prompts text; the entries are `BOOK_BUTTONS` 1,2 and 1,9 in `build/ed-db/text.json`.

## 4. Controller (index 0, `0x80114894`)

| | Address or value |
|---|---|
| Enter | `0x80114894`: draw `0x80114BD8`, leave `0x801148E0`, channel 5 on `0x80252B00` |
| Background | image 31 (432x480, the controller picture with its labels) at (144, 0)-(575, 479) |
| The only control | Rumble On/Off |
| Rumble word | `r13-23608` is the rumble-**off** mask. `0x8013D97C(mask)` reads it; `0x8013D93C(mask, set)` sets or clears it, and on setting stops the motor (`0x8013D900`) |
| A (`0x80114B68`) | Flips bit 1. When rumble is turned on, `r13-24236 = 30`, so the Tome frame runs the motor for 30 frames (`0x80111260`, `PADControlMotor` = `0x8020DA98`). Sound 546 at 100 |
| Value pictures | Rumble on: image 28 "On" in ink, then its glow image 41 tinted. Rumble off: image 29 "Off", then glow 42. Rects per language are at `0x8023A580` and `0x8023A5A8`; English is (407, 392)-(447, 425) for On and (409, 384)-(456, 432) for Off. Then image 43, the "Rumble Feature :" glow, tinted at a language rect |
| Stick and D-pad | Only reach the memory card dialog when it is up |
| B (`0x80114A48`) | If the settings changed, the save prompt (section 8). Otherwise sound 611 at 50, leave, `0x80101600`, cursor 0 |
| Start, L, R | `0x8011495C`, the same as the cross |
| Host already | `tome.cpp` redraws image 31 (hash `0x8680aee4cb0d0a97`) as a list of the player's controls in La Belle Aurore |

## 5. Audio (index 1, `0x80115608`)

| | Address or value |
|---|---|
| Enter | `0x80115608`: draw `0x80115950`, leave `0x801156A0`, channel 5 on `0x80252BAC`, row cursor `r13-24076` = `(0, 2, 2)` with `on_change` `0x801104AC`, sub-state `r13-24072 = 0` |
| Pictures | Image 27, then image 38 (324x106, seen: "Mode" and "Volume" plaques, and "Mono", "Stereo", "Stereo Surround") at English (214, 150)-(538, 256) |
| Rows | 0 Mode at ring (223, 157, 72, 32); 1 Volume at ring (223, 217, 72, 33). Some languages use other widths |
| Mode value | `r13-23012`: 0 Mono, 1 Stereo, 2 Stereo Surround. Glow pictures 40, 57 and 39 are tinted `r13-27112` (`0xC8C800FF`) over the painted word |
| A (`0x801157FC`) | Sound 546 at 100. Row 0 cycles the mode 0→1→2→0 and applies it with `0x8019D944(mode, 1, 1)`. Row 1 opens the slider with `0x80114FA4(140, 154, 254)` and sets sub-state 1 |
| Up/Down (`0x80115900`) | `0x801DD39C(r13-24076, delta)`, sound 635 through `on_change` |
| B (`0x80115724`) | Sub-state 1: sound 611 and `0x80115120(0)` (revert). Sub-state 0: the save prompt or back to the cross with cursor 1 |

**The volume slider** (`0x80114FA4(x, y, alpha_max)`, read):

- Its cursor `r13-24080` runs 0 to 127 (`0x801DD31C(c, 0, 128, 128)`). It starts at `0x8019DDCC()`, the current volume.
- It makes the hint `0x801DB374(r13-26492, 0, 37, 0, 1, "ItemText")` at `(x+225, 350)` (`0x801DA9CC`). The text is
  boot text 0,37, `VOLUME_SETTINGS`: "\i20 Accept \i21 Cancel".
- It starts the looping test sound 688 at volume 100 and keeps its handle in `r13-24108`.
- Left/Right is a **held** row with repeat (`0x80252BAC`, range 8191, 3). `0x80115088` moves the cursor and sets the
  volume live with `0x8019DCDC(2, v, v)`.
- Close `0x80115120(keep)`: keep=1 applies with `0x8019DCDC(0, v, v)`; keep=0 restores the old value. It stops sound
  688 (`0x8019DC00`) and frees the hint.
- Draw `0x801151F0(30, 18, 91, &colour, …, 64, 254)`. Image 18 (324x143, seen) is an empty bone-framed panel with a
  long bar. Image 30 is a soft white glow. The knob x is `value*237/128 + x + 59`. The glow's alpha pulses between the
  last two arguments.

## 6. Display (index 2, `0x80115FEC`)

| | Address or value |
|---|---|
| Enter | `0x80115FEC`: draw `0x80116470`, leave `0x80116214`, channel 6 on `0x80252C68`. Brightness cursor `r13-24056` = `(0, 51, 51)` at `r13-24664`. Row cursor `r13-24060` = `(0, 3, 3)`. Both use `on_change` `0x801104AC`. State word `r13-24052` |
| Pictures | `0x801117F4`, then image 33 (354x149, seen: "Brightness Adjustment", "Subtitles ON OFF", "Widescreen 16:9 Standard") at English (188, 197)-(542, 346) |
| Rows | Ring `(198, 208 + 46*row, 333, 34)` for rows 0-2 (`0x80116B9C`) |
| Row 0, Brightness | A enters state 1. That state draws image 21 (640x480 test picture, hash `0x9e51063f6b37567a`, glyph_art's `kBrightness`) full screen, greyed by the value, with a bar at `y = 440 - v*400/50` (`0x8019C8FC`). Up/Down calls `0x801160D4(delta)`, which sets `r13-24664` and applies it with `0x801E12C0(v)`. Entering writes repeat 20/5 into the binding table (`+84/+88`). B returns to state 0 and **keeps** the value |
| Row 1, Subtitles | A flips bit 0 of `0x80307180`. The value glow is image 37 "ON" or 36 "OFF". For Japanese, row 1 instead flips `r13-24720` (JJS/JES text) and reloads files |
| Row 2, Widescreen | A starts a fade (state 2, alpha byte at `r13-29024+3` +5 each frame). At 255, state 3. Three frames later, state 4 flips `r13-24660` and reloads the Tome art (`0x8002089C`, `0x80110A1C`, `0x80110844`), then fades back. The glow is 35 "16:9" or 34 "Standard". **The host already hooks this:** `hle.c:6196` `find_widescreen_option` (toggle `0x80116CE8`, menu `0x80116470`) |
| B (`0x801162A0`) | State 1: sound 611 and back to state 0. State 0: the save prompt or back to the cross with cursor 2 |

## 7. Load/Save (index 3, `0x801140D4`)

| | Address or value |
|---|---|
| Enter | `0x801140D4`: draw `0x8011440C`, leave `0x80114200`, channel 5 on `0x80252A48`. Starts the memory card manager `0x800AEE4C(0, 0x801143D8)` and `0x800AFC20(0, 0)`. Reads flags 476, 475 and 823 (823 is the scripted save offer, verbs.md:332) |
| Pictures | Image 27, then image 22 (421x331, seen: "Save" and "Load" plaques, "Memory Card Slot A/B" plaques, a large **blank** bone-framed parchment panel) at (154, 131)-(574, 461). Image 24 "Save" at (183, 138)-(246, 169) |
| Rings | Plain ring `0x8019CC10` on the chosen item; a dimmed ring `0x8019C9C8(…, &r13-27100)` (`0xC8C80064`) on the others. Slot rings at (267, 148, 221, 33) and (267, 196, 221, 33) (`0x8023A570`) |
| Input | The card manager's own: Up/Down `0x800AFD54`, Left/Right `0x800AFE50`, A `0x800B01C0`, B `0x800AFF14`. Its sounds: move 635 at 50 (`0x800AF1A0`), A 546 or 636 at 100 (`0x800B0230`/`0x800B0240`), B 611 at 50 (`0x800AFF64`) |
| Draw of its dialog | `0x800B0598(alpha)` |
| Back | `0x801143D8`: leave, `0x80101600`, cursor 3 |

## 8. The "save settings?" prompt, shared by every sub-page

- **Snapshot:** `0x800AD218(record)` copies the settings into a 36-byte record. The Tome's record is `0x80335768`; the
  title's is `0x803093DC`.
- **Restore:** `0x800AD2B0(record, also_language)` puts them back.
- **Compare:** `0x800AD368(record)` returns 1 if anything changed.

| Record offset | Setting | Source |
|---|---|---|
| +0 | language `r13-26284` (0 English, 1 German, 2 French, 3 Spanish, 4 Italian, 5 Japanese; the order of the file names at `0x802522B4`, inferred) | `0x800AD230` |
| +4 | Japanese text set `r13-24720` | |
| +8 | brightness `r13-24664` | |
| +12 | rumble-off bit 1 | `0x8013D97C(1)` |
| +16 | widescreen `r13-24660` | |
| +20 | subtitles `0x80307180` | |
| +24 | byte `0x80308A88` (`0x80042180`) | not identified |
| +25, +26 | the two volume bytes `0x8019DDCC()`, `0x8019DDC4()` | |
| +28 | sound mode `r13-23012` | |
| +32 | word `0x80308A8C` | not identified |

If B finds changes, the game opens `0x800AEE4C(1, callback)`, `0x800B3FE8(0, 1)` and `0x800AFC20(11, 0)`. That is
UMemcardText 0,11, `CONFIRM_SAVE_PREFERENCES` ("Save Game Settings … Would you like to save your changes to a Memory
Card?", read from the disc). The callback (`0x80114914`, `0x801156DC`, `0x80116258`) ends the manager, takes a new
snapshot and returns to the cross.

## 9. The title screen's Options

| | Address or value |
|---|---|
| Picture | One 850x480 texture (hash `0xbefc5ef6c1629595`, `titlevideo.cpp:183`) with the eight rows painted in a pale script (seen from `res/video/options.rgba`) |
| State | Byte `0x80309124+62`: 14 is the Options list, 15 Brightness, 16 Volume, 32 Language select. The cursor is at `+28` and the row count at `+40` (8) |
| Bindings | `0x8023D95C`, 7 rows: A/Start `0x80019F30`, B `0x8001AE28`, Up/Down `0x8001B588`, Left/Right `0x8001B300`, Y `0x8001B920` (does nothing), and row 6 (D-y and C-y), empty until Brightness |
| Up/Down in state 14 | `0x8001B6A4`: wraps over 8 rows, sound 635 at 100. Left/Right does nothing there (`0x8001B558`) |
| A in state 14 | Jump table `0x8023DB64`, then sound 636 at 100 (`0x8001AE04`) |
| Draw of the values | `0x8001D52C`: reads every setting and draws the value pictures from the title's own texture set, using quads chosen by `0x80018824(n)` per language. Called from `0x8001E478`/`0x8001E494` |

| Row | Painted label | A does |
|---|---|---|
| 0 | Save Settings | memory card manager `0x800AEE4C(1, 0x8001F9C0)`, `0x800AFC20(1, 0)` |
| 1 | Subtitles | flips `0x80307180` (Japanese: `r13-24720`) |
| 2 | Widescreen | flips `r13-24660` |
| 3 | Rumble Feature | flips the rumble bit; rumbles 30 frames on (`0x8013D6CC(1, 0, 30)`) |
| 4 | Volume Adjustment | state 16, swaps the Left/Right row, **the Tome's slider** `0x80114FA4(102, 154, 254)`. B: `0x80115120(0)`; A: `0x80115120(1)` (`0x8001ADB0`) |
| 5 | Brightness Adjustment | row 6's handler becomes **the Tome's** `0x801160D4`; cursor `r13-24056` = `(0, 51, 51)`; state 15 |
| 6 | Audio Mode | cycles `r13-23012`, `0x8019D944(m, 1, 1)` |
| 7 | Language Select | state 32 (`0x800195D8(32, 0)`); `r13-26140` 0-4 |

B in state 14 (`0x8001B0C8`) opens the same save prompt if the settings changed (record `0x803093DC`). Otherwise
`0x8001FA38(1)` returns to the title menu.

## 10. Drawing calls (all 640x480 screen units, read)

| Call | Does |
|---|---|
| `0x801E1B74(tpl)` | Binds a texture set for the next picks (inferred name). The Tome passes `r13-24316` |
| `0x8019C7C8(&rgba, field, image, 0x80000000)` | Copies a 16-short material template (`0x8023B060`), puts `image` into short `field`, and sets the colour. The pages use field 0 for ink pictures and field 5 for glow twins (what field 5 means is inferred). `field = -1` changes no picture |
| `0x8019D1A4(x0, y0, x1, y1, z, 0, 5)` | One textured quad, from the texcoords set by `0x8019D178(0x80254D10)` |
| `0x8019D084(texcoords, x0, y0, x1, y1, z, 0, 5)` | A quad with a given texcoord array |
| `0x8019D358(texcoords, positions)` + `0x8019D3B4(a, quad, 5)` | Pre-built quads (the book, the page backgrounds) |
| `0x8019CC10(x, y, w, h, -1, 3)` | **The selection ring:** mode `0x801E1D74(1, 1282, 1, 0, 0)` with f1 2.0, then the frame `0x8019C9C8` in `0x646400FF`. Its dashed or animated look comes from that mode (inferred) |
| `0x8019C9C8(x, y, w, h, -1, 3, &rgba)` | A plain rectangle frame of 4 quads, no picture. With `0xC8C80064` it is the dimmed ring |
| `0x8019CB6C(…)` | The same frame in `0x320000FF` with mode 1058 (another ring style, not used on these pages) |
| `0x801117F4(tab, alpha)` | The book frame and that tab's page background. Tab 0 is Options and uses image 27 |

**Text** (the game's own font). `hle.c:13730`, `hle.c:17217` and `hle.c:22942` already use these:

| Call | Does |
|---|---|
| `0x801DB374(textfile, group, entry, a, b, name)` | Takes one of **10 shared elements** at `0x805F87D0` (0x660 bytes each). `name` is only a label ("ButtonText", "ItemText") |
| `0x801DA8CC(element, fmt, r, g, b, a, string)` | Writes any words into an element (used for FINISH HIM) |
| `0x801DB864(textfile, group, id, 1)` | Returns the words of an entry |
| `0x801DA9CC(element, x, y)` | Moves an element |
| `0x801DA4D8(element)` | Frees an element |
| `0x801DA574` | Ticks and draws the elements each frame |
| `0x801DB088` | Lays out an element |

Text codes, from the parser `0x801D8464` (jump table `0x8025A4E0`), read:

| Code | Meaning |
|---|---|
| `\i<n>` | Icon n from the prompt icon sheet (20 A, 21 B, 16 L, 17 R, 18 Y, 19 X: inferred from the disc's strings) |
| `\s<float>` | Scale |
| `\c` + 4 bytes | RGBA colour |
| `\n` | New line |
| `\f<digit>` | Font n (`0x801D8400`, `0x801D7C2C`, current font `r13-22264`). The element's default font is byte +25 |
| `\j<letter>` | Alignment (`0x801DA064`) |
| `~NNN` | Literal character NNN. "~5" and "~6" are the handwritten sheet's button glyphs |

A pool of 10 elements is already short in co-op (`docs/coop-level3-design.md:91`).

## 11. Ways to draw host-defined rows

### A. Paint into the game's own picture (the most native, recommended for the 5th cross stop; inferred)

- **Replace image 27.** Claim its hash `0x7cd7b88669459bbb` the way `tome.cpp` claims image 31. The HD pack's copy is
  `textures/GED/splash, menu, and text/tex1_432x480_7cd7b88669459bbb_14.dds`; the override must win over it, as
  `buttons.cpp:285` arranges.
- **Paint a fifth plaque** ("Characters") under Load/Save, copying the plaque frame and ink from the painted ones. Write
  the word in La Belle Aurore, the Tome's hand that `tome.cpp` already uses with stb_truetype.
- **The word written below** is the art's own lettering: a calligraphic hand that is neither font exactly (seen).
- **Moving the ring:** intercept `0x8019CC10`, passing it through unless `lr == 0x80101A1C` (the call at
  `0x80101A18`) and the host stop is chosen. Then draw at the new rect instead. `hle_card_frame` (`hle.c:3932`) is the
  same pattern.
- **Down:** intercept `0x8010172C`. If the cursor (`*(r13-24544)+12`) is 3 and r3 > 0, select the host stop and play
  545 at 50. If the host stop is chosen and r3 < 0, deselect it. In both cases call `0x8013E154`.
- **A:** intercept `0x80101838` while the host stop is chosen. The game's A would call `0x801016D8` and open nothing for
  index 4. Play 546 at 100 and open the host page.
- **Left/Right and the tabs:** clear the host stop. `tome_other_page` and `tome_journal_page` already notice tab changes.
- **Where:** the free space under Load/Save in image 27 is the skeleton art (seen). A rect near (292, 131, 121, 32)
  needs a picture to place it.

### B. Call the game's draw calls from a hook, with host values (inferred)

- From a notify after the page draw (for example `0x80101980`), in the same frame and on the game thread:
  `0x801E1B74(r13-24316)`, then `0x8019C7C8(&0xFFFFFFFF, 0, image, 0x80000000)` and `0x8019D1A4(x0, y0, x1, y1, -1, 0, 5)`.
- **Usable blank art:**

| Image | Size | What it is | Hash |
|---|---|---|---|
| 18 | 324x143 | an empty slider panel | `0x03406308bcc79b0f` |
| 22 | 421x331 | the lower part is a large blank panel; use texcoords for a sub-rect with `0x8019D084` | `0x334f41197a692f84` |
| 30 | 175x84 | a soft glow | |

- **Values:** reuse the glow twins for generic values: ON 37, OFF 36, On 41, Off 42.
- **Labels:** labels need the font (`0x801DB374` then `0x801DA8CC`), and that takes from the 10 shared elements.
- **Risks:** GX state between the game's draws; the copy Tome's turns (`g_tome_copy_turn`); widescreen, where the book
  uses other quads (`0x8019D358(…, +740)`, `0x8011188C`).

### C. A host overlay drawn to look the same (what `frontend_tome_button` does now)

- **Today:** ImGui with IM Fell English TTF on a drawn parchment plaque (`frontend.cpp:3325`).
- **Font sheets:** `res/fonts/game/tome-0.png` (IM Fell English with an outline: the plain sheet) and `tome-1.png`
  (La Belle Aurore: the handwritten sheet) are 2048x2048 with 128-pixel cells. Character c is at column c>>4, row
  15-(c&15) of the file, upside down (`fonts.cpp:268`, `make_game_fonts.py`).
- **Drawing them:** load the PNG with `aurora_imgui_add_texture` (as `frontend.cpp:3456` does) and draw each glyph with
  `AddImage` and flipped v. Advances must come from each cell's ink box, because the game's own width table was not
  found. Codes 0x01-0x1F (the button icons) are not in the PNGs; use `res/ui/glyphs` through `frontend.cpp`'s
  `glyph()`. Simpler and close: ImGui with `res/fonts/LaBelleAurore.ttf` or `IMFellEnglish-Regular.ttf`, the TTFs the
  sheets were made from.
- **Matching the look:** copy the game's colours: ring `0x646400FF`, dimmed ring `0xC8C80064`, chosen glow `0xC8C800FF`.

### What the host already has

- `fonts.cpp` swaps the game's two font sheets.
- `glyph_art.cpp` redraws painted button pictures (image 21 among them).
- `buttons.cpp` swaps the prompt icon sheet.
- `tome.cpp` redraws image 31.
- `titlevideo.cpp` knows the title Options picture.
- `frontend_tome_button` and `frontend_open_characters` exist.
- `frontend.cpp`'s `Row` (label, value, `change(±1)`, `activate`, `toggle`, `slider` 0-100) is a ready model for any
  host page's rows.

**Settings both sides hold:**

| Game | Host | Linked today? |
|---|---|---|
| rumble bit (`r13-23608`) | `settings.vibration` | no hook found |
| game volume (`0x8019DCDC`) | `music_volume`, `effects_volume` (host scaling, `audio.c:194`) | separate |
| widescreen `r13-24660` | `aspect` | synced (`hle.c:6236`) |

## 12. The game's own interface sounds (`0x8019DB90(id, volume)` returns a handle; `0x8019DC00(handle)` stops it)

| Id, volume | Where | Use |
|---|---|---|
| 545, 50 | `0x80101780`, `0x801017F0` | Options cross move |
| 635, 50 | `0x801104C0`, `0x800AF1A0` | list cursor move, card manager move |
| 635, 100 | `0x8001B704` | title Options move |
| 546, 100 | 32 sites (`0x8010185C`, …) | select or toggle |
| 636, 100 | `0x8001AE0C`, `0x800B0240` | title confirm, card manager confirm |
| 611, 50 | 25 sites (`0x800AFF64`, …) | back or cancel |
| 495, 50 | 18 sites | tab change |
| 568, 100 | `0x8011121C` | Tome opens |
| 569, 100 | `0x80110E08` | Tome closes (`hle.c:15201` plays it too) |
| 688, 100 | `0x80115068` | the volume slider's looping test sound (keep its handle, stop it) |
| 629-632 and 724, 100 | `0x8001B85C`-`0x8001B8B4` | the title menu's rows, one per row (what they sound like not checked) |
| 509, 722, 723, 633, 634 | `0x80110FD8`, title code | not identified |

**How `hle.c` plays one:** `u32 a[2] = {id, vol}; hle_guest_call(cpu, SOUND_PLAIN /*0x8019DB90*/, a, 2, 0.0, &handle);`
(`hle.c:14868`, `15202`). It must run on the game thread with the real memory in, not in a copy Tome turn
(`tome_copy_sound_live`, `hle.c:15160`). A sound asked for from the ImGui side should be queued to a game-thread hook
(inferred).

## Open questions

1. Is the selection ring really "dashed", and does it animate? Mode `0x801E1D74(1, 1282, …)` was not decoded. A picture
   of the ring at full size would settle it.
2. Where is the fifth plaque best placed in image 27? Which rect, and does the ring fit there in 4:3, 16:9 and
   co-op halves? It needs pictures.
3. Which font sheet does the Tome's hint text use by default (element byte +25)? Is `\f0` the plain sheet and `\f1` the
   handwritten one? A run with a trace, or a picture, would settle it.
4. The game's own per-character width table was not found. It is needed for exact spacing if the host draws glyphs
   from `tome-*.png`.
5. Record fields +24 (`0x80308A88`) and +32 (`0x80308A8C`) are not identified.
6. Material field 5 versus field 0 in `0x8019C7C8`: blend or stage? Not decoded.
7. Should the game's rumble and volume follow the launcher's `vibration` and `music_volume`/`effects_volume` (or the
   reverse), as widescreen already does? This is the user's call.
8. Which approach does the user want for the cross stop: a repainted page (A), game-drawn pieces (B) or an overlay (C)?
   A looks the most native, but every language's image 27 would need repainting. Only English is redrawn today, as for
   image 31.
