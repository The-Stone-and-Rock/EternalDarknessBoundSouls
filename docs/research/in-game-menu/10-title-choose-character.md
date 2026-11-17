# "Choose Character" on the title's main menu (PAL, GEDP01)

Research only. Nothing here is built or run. Every address is the PAL disc's.
"[read]" means I read the code, the data or the picture. "[inferred]" means it follows but was not checked in a run.

The ask (the user, 2026-11-17): on the title's main menu, a row "Choose Character" **above Options**, opening the
launcher's character page (`frontend_open_page(FRONTEND_PAGE_CHARACTERS, 0)`).

Read first: 02-title-menu.md sections 1-3, 7, 8. This page only adds what that one did not settle.

Short answer:
- Give the new row the game's **spare index 5** (one past Credits). The game already treats row 5 as a real row:
  the cursor can sit there, A on it plays the choose sound and does nothing else, and Up/Down plays no whisper there.
- The host **reorders the moves**, not the rows: on each Up/Down it sets the game's cursor so the game's own `+dir`
  lands where the host wants. The visual order becomes Start, Load, **Choose Character**, Options, Jump, Credits.
  The game's mapping of cursor to action (0 Start, 1 Load, 2 Options, 3 Jump, 4 Credits) is not touched.
- Room on screen: move Options, Jump to Game and Credits down **34 pixels**. Jump and Credits are their own
  textures, so only their quads move (an s16 write to the table in RAM). Options is painted into texture 1, so the
  host repaints texture 1: Options lifted and put back 34 lower, and "Choose Character" painted where Options was.
  The lowest ink then ends at y 456 of 480, and every row stays inside picture x 106..745.
- Hooks: extend the two notifies that are already there (`title_more_updown`, `title_more_choose`), add one notify on
  the screen draw `0x8001DF90` and one pass-through intercept on the quad draw `0x8019D3B4` (to hide the game's
  near-black row-5 glow). Plus the texture work in titlevideo.cpp.

---

## 1. What the game does on the main menu (state 7)

### 1.1 Up/Down `0x8001B588(dir)`, state 7 case at `0x8001B778` [read]

1. `cursor += dir` (TM+28, `0x80309140`). If `cursor >= count` (TM+40, `0x8030914C`) then 0. If `cursor < 0` then
   `count - 1` [read `0x8001B784`-`0x8001B7B8`].
2. Skip hidden rows, by moving again in the same direction:
   - row 4 (Credits) when fewer than 1 bit is set in the byte `0x80308A88` [read `0x8001B7BC`-`0x8001B7DC`];
   - row 3 (Jump to Game) when fewer than 2 bits [read `0x8001B7E0`-`0x8001B7FC`].
   `0x801DBDA8` is a plain bit count of its argument [read `0x801DBDA8`-`0x801DBDD0`].
   Only rows 3 and 4 are ever skipped. **Row 5 is never skipped.**
3. Stop the last whisper (TM+56) [read `0x8001B810`-`0x8001B828`], then play the new row's:

   | Row | Sound | Evidence |
   |---|---|---|
   | 0 Start Game | 629 | `0x8001B85C` [read] |
   | 1 Load Game | 630 | `0x8001B870` [read] |
   | 2 Options | 631 | `0x8001B884` [read] |
   | 3 Jump to Game | **724** | `0x8001B858` → `0x8001B8AC` [read] |
   | 4 Credits | **632** | `0x8001B850` → `0x8001B898` [read] |
   | 5 and up | none | `0x8001B854` → `0x8001B900` [read] |

   (02-title-menu.md 2.1 has 632 and 724 the other way round. The code above is the right one.)
4. State 7 does **not** ask about a memory card notice. Only other states call `0x800AF00C` here
   [read `0x8001B5DC`, `0x8001B644`; none in `0x8001B778`-`0x8001B8BC`].

### 1.2 A / Start `0x80019F30`, state 7 case at `0x8001A464` [read]

1. Stop both sounds (TM+56, TM+52) [read `0x8001A464`-`0x8001A49C`].
2. By cursor [read `0x8001A4A4`-`0x8001A4D0`]: 0 Start (`0x8001A4D4`), 1 Load (`0x8001A50C`), 2 Options
   (`0x8001A5EC`), 3 Jump (`0x8001A638`), 4 Credits (`0x8001A628`). Anything else, row 5 included, goes straight to
   `0x8001A678`.
3. Every row, row 5 too, ends with sound 633 (`0x8001A67C`) [read]. r28 is 0, so no "denied" 636 [read `0x8001A678`,
   `0x8001ADFC`].
4. No card notice check in state 7 either [read: no `0x800AF00C` call in `0x8001A464`-`0x8001A688`].

So A on row 5 = the choose sound and nothing else. A notify there can open the page.

### 1.3 The draw `0x8001DF90(state, cursor, alpha)` [read]

- A real function: `stwu r1,-352(r1)` (word `0x9421FEA0`) at its entry; it keeps state in r27, **cursor in r28**,
  alpha in r29 [read `0x8001DF90`-`0x8001DFB4`]. Called by `bl` from `0x8001F638` (the old screen during a
  cross-fade), `0x8001F658`, `0x8001F690`, `0x8001F6F4`, `0x8001F98C` [read; `ed fn 0x8001DF90`].
- State 7's code at `0x8001E2F0` is a **case label, not a function** (no prologue) [read]. A hook there would not fire.
  Hook `0x8001DF90` and look at r3 == 7.
- State 7 draws [read `0x8001E2F0`-`0x8001E430`]:
  1. texture 1, the whole 850x480 picture, `0x8019D4EC(1, alpha, 1)` at `0x8001E304`;
  2. binds the quad table, `0x8019D358(0x80254D10, 0x8023C780)` at `0x8001E314`. That function is two
     GXSetArray-style calls: positions from `0x8023C780`, stride 6 (three s16 per corner) [read `0x8019D358`-
     `0x8019D398`]. The quads are **indexed vertex data read from RAM** at draw time;
  3. only if `r13-26460 != 0`: Credits unlit (texture 69, quad `0x80018824(4)`) when 1+ bits; Jump unlit (texture 70,
     quad `0x80018824(3)`) when 2+ bits [read `0x8001E334`-`0x8001E3C4`];
  4. the glow: material texture `cursor + 6` at `0x8001E414`, quad `0x80018824(cursor)` at `0x8001E41C`, drawn by
     `0x8019D3B4(0, quad*4, 5)` at `0x8001E42C` (return address `0x8001E430`). Tint pale cyan `0xA0E3F1`
     (`0x806205DC` = r2-32388, dumped) [read]; gold (255, 249, 183) for Jump when 3+ bits [read `0x8001E3C8`-`0x8001E3F4`].
- **For cursor 5 the game draws texture 11 at quad 5.** Texture 11 is a 2x2 near-black blank (02-title-menu.md 3.2).
  Quad 5 is x 64..257, y 200..246 [read: dumped]. A dark box on the left. It must be hidden (hook H4).
- Quad 5 is **not free to move**: the Options screen draws texture 14 at `0x80018824(5)` at `0x8001DB98`, one of its
  row glows [read `0x8001DB7C`-`0x8001DBAC`]. Its box (y 200..246 on the left) matches the 4th Options row, Rumble
  Feature [inferred from the rows' spacing].

### 1.4 The quad index `0x80018824(n)` [read]

- `n > 23` returns n as it is (`bclr` at `0x80018828`).
- Else a jump table at `0x8023D8FC` [read: dumped]. Row 0's case `0x80018B4C`: language 0 returns n (0); 1 → 42;
  2 → 85; 3 → 107; 4 → 63; **5 returns n too** [read `0x80018B4C`-`0x80018B94`]. Row 1 the same plus one [read
  `0x80018B98`-`0x80018BE0`].
- The only callers with n = 2, 3, 4 are the main menu draw (`0x8001E3B0` li 3, `0x8001E36C` li 4, and r28) [read: all
  36 call sites listed]. So English quads 2..4 belong to the main menu alone [read for `0x80018824`'s callers; no
  title code was seen writing the table, inferred].

### 1.5 The idle clock (state 7 only) [read `0x8001C55C`-`0x8001C684`]

- TM+48 (`0x80309154`) bit 0x2 clear: the step sets it and starts and resets the stopwatch at `0x80309168`.
- Any press resets it (`0x8013DC24(-1, 0, 0)`).
- At 180 s (`r2-32420`) the menu goes to state 23 (the attract movie).
- So: clear bit 0x2 once **after** the page closes, and the game restarts its own clock next frame.

---

## 2. The painted words, measured

Texture 1 was decoded from the unpacked `UMnMenu.cmp` (the scratch copy of an earlier session, `tex/t01.png`). It is
stored upside down. Figures below are upright. Picture x = menu x + 106, picture y = menu y: the picture quad is
x -106..745 over 850 texels [read 02-title-menu.md 3.2]. This matches the ink: Start Game's quad x 436..603 is
picture 542..709, and its ink is 540..701 [read: measured].

Ink = pixels with brightness over 55 and blue over red by more than 16 (titlevideo.cpp's own test for these words,
titlevideo.cpp:585). Jump and Credits measured in their own textures, placed by their quads.

| Row | Where it is | Ink, picture x | Ink, y | Quad (menu space) | Glow texture |
|---|---|---|---|---|---|
| 0 Start Game | painted in texture 1 | 540..701 | 259..289 | 436..603, 243..305 | 6 (168x63) |
| 1 Load Game | painted in texture 1 | 540..683 | 291..324 | 427..586, 279..340 | 7 (160x62) |
| 2 Options | painted in texture 1 | 562..652 | 326..367 (the p's tail) | 452..556, 323..378 | 8 (104x55) |
| 3 Jump to Game | texture 70, unlit | 486..707 | 353..421 | 367..614, 348..428 | 9 (247x80) |
| 4 Credits | texture 69, unlit | 557..659 | 390..425 | 442..564, 382..441 | 10 (122x59) |

[read: measured on the decoded textures; glow textures agree to ±2 px with the painted ink]

Also in the picture [read: measured]:
- The painted ink colour: mean (71, 93, 98), brightest (90, 117, 123). The same as the Options picture's rows
  (titlevideo.cpp:306-307).
- Her face, chin and neck: x 475..560 from y 250 to 386 (brightness 30-57, faintly teal).
- Under y 386 and right of x 560 the picture is almost black (mean 2-5), except a faint teal strand at x 580..600,
  y 434..474.
- The copyright line: x 352..488, y 414..430. The J of "Jump" reaches x 486 at y 397 today.
- The rows touch: Start ends 289, Load starts 291; Load ends 324, Options starts 326. The game's own gap is 2 px.
- The pitch, top to top: 32, then 35.

---

## 3. The layout with the new row

Move Options, Jump to Game and Credits down by **D = 34** (one row's pitch). Put "Choose Character" where Options was.
Move Jump 10 px right as well, or its J hook (x 486) touches "Nintendo" (ends x 488) at y about 428-431.

| Row (visual order) | Ink x | Ink y | Quad (menu space, s16) | Change |
|---|---|---|---|---|
| Start Game | 540..701 | 259..289 | 436..603, 243..305 | none |
| Load Game | 540..683 | 291..324 | 427..586, 279..340 | none |
| **Choose Character** (new, game row 5) | about 502..734 | about 327..358 | none (host paints it, lit or not) | new paint in texture 1 |
| Options | 562..652 | 360..401 | 452..556, **357..412** | quad 2 +34; its paint moved +34 |
| Jump to Game | 496..717 | 387..455 | **377..624, 382..462** | quad 3 +10 x, +34 y |
| Credits | 557..659 | 424..459 | 442..564, **416..475** | quad 4 +34 |

Checks [inferred from the figures above]:
- Lowest ink 459, lowest quad 475: under 480, and ink above 470.
- Widest right edge 734 (new word), 717 (Jump): inside 745, so the 4:3 crop keeps them.
- New word 327..358 sits between Load (ends 324) and moved Options (starts 360): 3 and 2 px gaps, as the game's own.
- "Choose Character" has no letters that hang below the line, so 31 px of height is enough at about the size of
  Start Game.
- Width: at Start Game's letter size (16 px per letter) it would be about 256 px. Use about 90% (about 232 px,
  x 502..734). Its first 40-60 px cross her chin, as "Jump" does today. If the user wants it clear of the face,
  "Characters" fits in x 560..720 at full size. The user decides the label.

### 3.1 The RAM writes (English only)

The table `0x8023C780` holds 4 corners x (x, y, z) s16 per quad: (x0,y0,-1) (x1,y0,-1) (x1,y1,-1) (x0,y1,-1)
[read: dumped from main.dol].

| Quad | Address | Original | New |
|---|---|---|---|
| 2 Options | `0x8023C7B0` | x 452/556, y 323/378 | x 452/556, y 357/412 |
| 3 Jump | `0x8023C7C8` | x 367/614, y 348/428 | x 377/624, y 382/462 |
| 4 Credits | `0x8023C7E0` | x 442/564, y 382/441 | x 442/564, y 416/475 |

- Write absolute values, never "+34", so a repeat is harmless.
- Write them in the `0x8001DF90` notify when r3 == 7, before the bind at `0x8001E314`, so they are in place whichever
  way the backend reads the array. Write the originals back when the language is not 0: language 5 also uses quads
  0..4 (section 1.4).
- Save states carry this RAM. Writing every state-7 frame (three 24-byte compares) also covers a state saved by an
  older build [inferred].

### 3.2 The texture-1 repaint (titlevideo.cpp)

From the disc's texture 1 (`kTitleHash`, titlevideo.cpp:165), two cached variants (unlit, lit), as `g_more` does for
the Options picture (titlevideo.cpp:308-368):
1. **Lift Options**: the ink mask in x 556..660, y 324..370 (the same test as titlevideo.cpp:585, soft edges).
2. **Fill the hole**: masked pixels take a blur of the unmasked pixels round them (radius about 6). The ground there
   is dark hair (median about 8), so this is easy.
3. **Put Options back** 34 px lower: colour mixed in by the mask. The ground there is almost black.
4. **Paint "Choose Character"** with `tome_hand_cover` (as `paint_more`, titlevideo.cpp:317-355): ink (72, 97, 104),
   at 0.9 strength; baseline about y 356; size so ascender to baseline is 28-30 px and the width at most 232 px.
5. **Lit variant** (game cursor 5): the light colour `0xA0E3F1` and the faint 6-px halo, as `paint_more` lights
   "More Options". This matches the game's cyan glow tint (section 1.3).
- Fold `paint_more` into one helper (text, left, baseline, size, lit) used by both pictures.
- Stored upside down: write at row `479 - y` (as `paint_more`'s `put` does).

What changes in titlevideo.cpp [read the current code]:
- `titlevideo_claims_hash` claims `kTitleHash` only while the video is on (titlevideo.cpp:520). It must claim it
  always when the row is in use (English).
- `titlevideo_generation` keys all 850x480 textures on one number (titlevideo.cpp:526-532). Add the main menu's lit
  bit beside the Options one, e.g. `more | (menu_lit << 1)`, and `serial * 4 + ...` with the video on.
- `title_picture` (video on) lifts the words from the original in the box `{505, 85, 745, 220}` stored = upright
  x 505..745, y 259..394 (titlevideo.cpp:42). Lift from the **repainted** picture instead, and widen the box to
  stored `{490, 69, 745, 225}` (upright y 254..410) so moved Options (to 401) and the new word (from x 502) come
  across. The lit paint passes the lift's teal test (brightness about 208, blue over red 81) [inferred].
- `title_picture` returns 2 ("the picture meanwhile") until the first video frame (titlevideo.cpp:565). Then it must
  hand the repainted picture, not the original: the Options glow quad has already moved.
- A new `titlevideo_menu_row(int)` beside `titlevideo_options_row`, set from the draw notify.

---

## 4. The hooks

The dispatch entries `0x8001B588` and `0x80019F30` already carry `title_more_updown` / `title_more_choose` as
**notifies**, and the install refuses a taken entry (hle.c:33450-33466) [read]. So the new work goes **inside those
two functions**, not in new entries.

| # | Kind | Address (first word) | What it does |
|---|---|---|---|
| H1 | notify (extend `title_more_updown`) | `0x8001B588` (`0x9421FFE0`) | state 7, English: TM+40 := 6; work out where the cursor should land; TM+28 := dest - dir |
| H2 | notify (extend `title_more_choose`) | `0x80019F30` (`0x9421FED0`) | state 7, English, TM+28 == 5: `frontend_open_page(FRONTEND_PAGE_CHARACTERS, 0)`; mark "reset the idle clock after it closes" |
| H3 | notify (new) | `0x8001DF90` (`0x9421FEA0`) | r3 == 7: `titlevideo_menu_row(r4)`; write the quads (3.1); after the page has closed, clear TM+48 bit 0x2 once |
| H4 | intercept, pass-through (new) | `0x8019D3B4` (`0x9421FFE0`) | skip the call when `lr == 0x8001E430` and `r28 == 5` (the row-5 glow); pass every other call, like `scene_bars` (hle.c:6439-6443) |

### 4.1 H1 in detail: the host's order, the game's step

The visual order, read as the game reads it: `[0, 1, 5, 2]`, then 3 if 2+ bits are set in `0x80308A88`, then 4 if
1+ bit. Wrap at both ends. Then write TM+28 := dest - dir, and TM+40 := 6. The game adds dir and lands exactly on
dest: dest is a shown row, so its skip loop does not run; dest - dir is at least -1 and at most 6, which its wrap
test handles [read `0x8001B784`-`0x8001B7B8`].

Every move with Jump and Credits shown (5 bits or more, say):

| From | Press | Host writes TM+28 | Game lands | Whisper (game's own) |
|---|---|---|---|---|
| 1 Load | Down | 4 | 5 Choose | none (old one stopped) |
| 5 Choose | Down | 1 | 2 Options | 631 |
| 2 Options | Up | 6 | 5 Choose | none |
| 5 Choose | Up | 2 | 1 Load | 630 |
| 4 Credits | Down | -1 | 0 Start | 629 |
| 0 Start | Up | 5 | 4 Credits | 632 |

With Credits and Jump hidden (a new card): Options is the last row, Down from it gives 0, Up from 0 gives 2.
Without the remap the game would go 2 → 5 (Down) and 0 → 5 (Up), which is why the remap is needed.

Why not the other designs:
- **A "virtual" row (the game's cursor stays 1 while ours is lit)**: the game would glow "Load Game" under ours,
  A would start Load Game, and Down would need cursor := 0 so it lands on 1 again, replaying the "Load Game"
  whisper. Each needs another hook. In the end A must set the cursor to 5 anyway. Rejected.
- **A real row at index 2** (Options to 3, and so on): the game reads the cursor inline in four places (move and
  skip, whisper, A, draw with `cursor + 6` and the Jump gold test at r28 == 3). No notify can remap those reads.
  Rejected.

### 4.2 H2: what A does

- The game itself stops the whispers and plays 633 (section 1.2). The host only asks for the page.
- `frontend_open_page(FRONTEND_PAGE_CHARACTERS, 0)` opens player 1's roster over the paused title with
  `g_root_closes`. Back from it closes the whole menu, since `past_root` treats Characters' way out as the root
  [read frontend.cpp:2804-2812, 4468-4509, 1086-1115].
- On close the title is still state 7, cursor 5: the row is lit, where the player left it [inferred].
- The game asks no card notice in state 7 (1.1, 1.2). Mirror that. A guest call to `0x800AF00C` as in
  `title_on_options` is harmless if wanted.

### 4.3 H3 and H4: the draw

- H3 sees every screen draw. Only r3 == 7 matters. During a cross-fade the old screen is drawn first with its own
  cursor (`0x8001F638`), so the cursor in r4 is the right one for each picture (the 07-review fix for More Options).
- H4 is needed because the game draws texture 11 (a near-black 2x2) at quad 5 when the cursor is 5 (section 1.3).
  Other ways fail: quad 5 is shared with an Options glow; the 2x2 blanks all share one hash, so overriding texture 11
  would change 0, 4, 30-33 and 40 as well; and no other cursor value is safe to feed the draw.
  `0x8019D3B4` has no host hook today [read `ed fn 0x8019D3B4`; grep of runtime/]. It has 120 callers, the Tome's
  draws among them. A pass-through costs one dispatch each [inferred].
- Skipping only `0x8019D3B4` leaves the material call at `0x8001E414` (texture 11 set, nothing drawn). Every later
  draw sets its own material [inferred]. It does not need hiding.

### 4.4 The idle clock

- The roster can stay open for minutes. The stopwatch measures guest time, and the guest clock follows the wall
  clock (02-title-menu.md section 8). Without care, the attract movie starts on the first frame after closing.
- Fix: H2 sets a flag. H3 clears TM+48 bit 0x2 (`0x80309154`) on the first state-7 draw **after the page has
  closed**, then drops the flag. The step at `0x8001C568` then restarts the stopwatch [read].
- "After it has closed" needs a sign from frontend.cpp. A close counter bumped where `run_menu` returns
  (frontend.cpp:4514) is enough [inferred]. If the menu only opens a frame after H2, clearing the bit early does no
  harm: the step just restarts the clock.

---

## 5. Risks

| Risk | Why | What to do |
|---|---|---|
| **tools/ed/jtg.sh breaks** | It presses Down x3 to reach Jump to Game (jtg.sh:22). With the new row, Down x3 lands on Options | Use Up x2 from Start (Start → Credits → Jump) in jtg.sh: the same landing before and after. Or Down x4. Check every `ed` script that walks the title |
| A press leaks back into the title | Start is on the same binding as A (mask `0x1100`). If Start, or a fresh A, closes the page, the game may take it as A on row 5 and open the page again | Check in a run, closing each way (B, the Back row, Start, Esc). More Options has the same exposure. If it leaks, hold the pad until release after closing (02-title-menu.md 8) |
| HD pack's title picture lost with the video off | A claimed texture skips the pack (gxcore_draw.cpp:2709-2738). The GED pack has texture 1 (`tex1_850x480_2a6eea64eb8fbbbb_14.dds`, 3400x1920 BC7). The user's settings have `hd_textures = 1` and `mod_title_video = 1` (logs/run-20261117-032351-jtg-titlemore/settings.ini) | With the video on, nothing changes: texture 1 is already claimed. With it off, the title drops to the disc's 850x480. More Options already did the same to the Options picture (`befc5ef6c1629595` is in the PAL supplement). Tell the user |
| Row reachable but not painted | Another disc revision or a language with another `kTitleHash` | Gate H1-H4 on language 0 (as `title_on_options`). Better: titlevideo sets a flag the first time it repaints `kTitleHash`, and H1 adds the row only after that |
| The backend reads the quad array late | Quads are indexed from RAM (1.3). If the renderer copies arrays at bind time, the write must come first | H3 writes before the bind at `0x8001E314`. Either way works [inferred] |
| New word crosses her chin | Chin at x 475..560 for these rows | Same as "Jump" today. Or the label "Characters", clear of the face |
| Whisper | No whispered "Choose Character" on the disc. The game is silent on row 5 | Leave it silent. Or the host plays 635 (Options' move tick) with `0x8019DB90(635, 100)` and keeps the handle in TM+56 so the next move stops it. The user decides |
| Card notices | None asked by state 7's Up/Down or A (1.1, 1.2) | Mirror the game |
| Two notifies grow | `title_more_updown` / `title_more_choose` now serve states 7 and 14 | Keep one function each, switching on TM+62. Rename them `title_rows_*` |

---

## 6. Files and addresses

| Need | Where |
|---|---|
| State / cursor / count / flags | `0x80309162` / `0x80309140` / `0x8030914C` / `0x80309154` (bit 0x2 idle) |
| Shown-rows byte | `0x80308A88` (bit count: 1+ Credits, 2+ Jump, 3+ Jump in gold) |
| Up/Down, A | `0x8001B588` (state 7 at `0x8001B778`), `0x80019F30` (state 7 at `0x8001A464`) |
| Screen draw | `0x8001DF90(state, cursor, alpha)`; state 7 case `0x8001E2F0`; glow `0x8001E3F8`-`0x8001E42C` |
| Glow draw return address | `0x8001E430` (quad draw `0x8019D3B4`), material `0x8001E418` (`0x8019C7C8`) |
| Quad table, English rows 2-4 | `0x8023C7B0`, `0x8023C7C8`, `0x8023C7E0` (24 bytes each) |
| Quad index | `0x80018824`, table `0x8023D8FC` |
| Language word | r13-26284 (`0x8061ED74`) |
| Existing hooks | hle.c:17150-17185 (`title_more_*`), install hle.c:33450-33466 |
| Page API | frontend.h:47-57; open at frontend.cpp:4468-4509 |
| Painting | titlevideo.cpp:303-368 (`paint_more`, `options_picture`), 518-532 (claims, generation), 558-605 (`title_picture`) |
| Scripted title walk | tools/ed/jtg.sh:18-29 |

---

## Open questions

1. The label: "Choose Character" (asked; about 232 px wide, crosses her chin) or "Characters" (clear of the face)?
2. A sound when the row is reached: silent (the game's own behaviour on row 5) or the Options tick 635?
3. In co-op, the title row opens player 1's roster. Should player 2 reach theirs from the title too, for example
   the roster switching player with a shoulder button, or a second row? The title's input channel does not say
   which pad pressed [inferred].
4. Do the notifies on `0x8001DF90` and the intercept on `0x8019D3B4` fire? Both are reached by `bl` [read], so they
   should. One traced run settles it.
5. Does a press leak back after closing (section 5)? Same test as for More Options.
6. HD pack with the title video off: accept the disc's picture, or find a way to paint on the pack's 3400x1920
   picture? (The host has no CPU BC7 decoder today [inferred: texpack.cpp uploads BC blocks as they are].)
