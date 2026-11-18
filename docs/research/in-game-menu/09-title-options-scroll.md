# The title's Options as a scrolling list (PAL, GEDP01)

Research only, 2026-11-17. Nothing was built or run. No file but this one was changed.

Tags: **[read]** = read in `build/ed-db/disasm.txt`, in `generated/main.dol` (bytes dumped with a small DOL reader), or in
the file named. **[measured]** = computed from `res/video/options.rgba` or a screenshot already in the repo.
**[inferred]** = follows from what was read, not checked in a run. `r13 = 0x80625420`, `r2 = 0x80628460`.
TM = the title record `0x80309124`. Picture coordinates are the 850x480 Options picture upright.

Builds on 02 (title menu), 04 (Options sub-pages), 05 (critic), 06 (texture bank). Not redone here.

Short answer:

- The game's Options screen is one draw function, `0x8001D52C`. It draws the picture, then the value words, then the
  glow of the chosen row. Every value word and glow is one quad from the table `0x8023C780`. **Only this function uses
  the Options quads** (5-9 and 12-23 in English).
- The 8 painted row names in the picture **split cleanly into 8 rows** by connected pieces of ink (52 pieces, none in
  doubt), and the ground under them is black. So the host can lift each row and move it, and paint ours in the gaps.
- Plan: keep TM+28 as the game's own cursor over **14 rows** (its 8, then Display, Sound, Graphics, Controls, Mods,
  Quit game). Show 9 rows at a time. The host repaints texture 2 for each (scroll, lit row). The game's value words and
  glows follow their rows because the Options draw is given a **shifted copy of the quad table**, picked by scroll.
- Hooks: the 3 that exist today (Up/Down, A, draw entry) with new contents, plus **one new notify** on `0x8019D358`
  when it is called from the Options draw. No draw call is intercepted by lr. The game's table is never written.
- Our labels in La Belle Aurore look close to the game's hand. At the size used today (44 px) they are about 80% of the
  game's size. 52 px matches the game's width and x-height.
- English only. The other languages keep the game's 8 rows, as today.

---

## 1. The draw: `0x8001D52C(cursor, &glow, alpha)`

Called from the screen draw `0x8001DF90` for state 14 (`0x8001E478`) and state 16, Volume (`0x8001E494`, then the
volume bar `0x801151F0`) [read]. r3 = cursor (TM+28, or the old screen's during a cross-fade), r4 = a word holding the
glow colour, r5 = the fade alpha [read `0x8001E464`-`0x8001E494`, `0x8001DFA8`-`0x8001DFD4`].

Colours [read: DOL words at `r2-32408`..`r2-32388`; each gets the fade alpha in its low byte]:

| Where | Word | Use |
|---|---|---|
| `r2-32408` | `0x58737BFF` | the value words, teal (88, 115, 123) |
| `r2-32404` | `0xFFFFFFFF` | white; language 5 only |
| `r2-32400` | `0x000000FF` | black: the Audio Mode word's shadow, drawn first |
| `r2-32388` (caller) | `0xA0E3F1FF` | the glow tint, pale cyan |

### 1.1 The order of the draw [read `0x8001D52C`-`0x8001DE14`]

Each word is `0x8019C7C8(&colour, 5, texture, 0x80000000)` (the material) then `0x8019D3B4(0, quad*4, 5)` (one quad of
4 indexed vertices). The quad number comes from `0x80018824(n)`, which returns n in English [read `0x80018844`,
`0x80018D84`, `0x80018E8C`, `0x80018CD4`: language 0 takes the plain `blr`].

1. `0x8019D4EC(2, alpha, 1)`: texture 2, the picture, on quad 3 of a different table, `0x80254D20` (x -106..745,
   y 0..479) [read; DOL dump]. So picture x = menu x + 106, picture y = menu y (within 1 px).
2. `0x8019D358(0x80254D10, 0x8023C780)` at `0x8001D594` (return address `0x8001D598`): texcoords and the positions
   array. Inside, `GXSetArray(9 = POS, 0x8023C780, stride 6)` and `GXSetArray(13 = TEX0, 0x80254D10, stride 4)`
   [read `0x8019D358`-`0x8019D3B0`].
3. Value words, always drawn, whatever the cursor:

| Row | Setting | Test | Drawn |
|---|---|---|---|
| 1 Subtitles | bit 0 of `0x80307180` | set | tex 26 at quad 16 |
| | | clear | tex 27 at quad 17 |
| | (language 5) | `r13-24720 == 1` | tex 30, else 31, both at quad 16 |
| 3 Rumble | `0x8013D97C(1)` | non-zero | tex 27 at quad 13 |
| | | zero | tex 26 at quad 12 |
| 2 Widescreen | `r13-24660` | non-zero | tex 28 at quad 18 |
| | | zero | tex 29 at quad 19 |
| (language 5) | | always | tex 25 at quad 41 (`li r4,164`), white |
| 6 Audio Mode | `r13-23012` | 0 | tex 42 (black) then tex 45 (teal), quad 20 |
| | | 1 | tex 43 then 46, quad 21 |
| | | 2 | tex 44 then 47, quad 22 |

   What the textures say [inferred]: 26 "On", 27 "Off" (sizes of quads 12/16 vs 13/17; "On" seen at quad 16's place in
   `Screenshots for Claude/menus-2026-11-17/6-title-more-options.jpg`). So `0x8013D97C(1) != 0` means rumble off;
   that matches A on row 3, which buzzes only when it turns rumble on [read `0x8001AAE4`-`0x8001AB10`]. 28 "16:9"
   (seen), 29 "Standard" (04 §5 saw "Widescreen 16:9 Standard" in the Tome's art). Audio mode 2's word is the wide
   "Stereo Surround" (quad 22 is the wide one, seen in the shot).
4. `0x8001D934`: `cmplwi r29, 7; bgt 0x8001DDD8`. **A cursor above 7 skips all glow** [read].
5. The glow, by the jump table `0x8023DD80` (8 entries, DOL dump), tinted `0xA0E3F1`:

| Row | Handler | Name glow | Value glow |
|---|---|---|---|
| 0 Save Settings | `0x8001D954` | tex 48 at quad 23 | |
| 1 Subtitles | `0x8001D98C` | tex 34 at quad 14 | bit set: tex 35 at quad 16; clear: tex 36 at quad 17 (language 5: 78/79 at 16) |
| 2 Widescreen | `0x8001DACC` | tex 38 at quad 15 | on: tex 39 at quad 18; off: tex 37 at quad 19 |
| 3 Rumble Feature | `0x8001DB7C` | tex 14 at quad 5 | `0x8013D97C(1)` non-zero: tex 36 at quad 13; zero: tex 35 at quad 12 |
| 4 Volume Adjustment | `0x8001DC30` | tex 15 at quad 6 | |
| 5 Brightness Adjustment | `0x8001DC68` | tex 16 at quad 7 | |
| 6 Audio Mode | `0x8001DCA0` | tex 41 at quad 8 | mode 0/1/2: tex 42/43/44 at quad 20/21/22 |
| 7 Language Select | `0x8001DDA4` | tex 77 at quad 9 | |

6. `0x801E1B74(TM+16)` (the bank back), then the card notice `0x800B0598(alpha)` if `0x800AF00C()` [read].

### 1.2 The quads the Options page uses [read: `0x8023C780` dumped, 24 bytes each, 4 corners of (x, y, z) s16]

| Quad | Menu x0..x1 | y0..y1 | Picture x | Used for |
|---|---|---|---|---|
| 23 | 71..214 | 77..147 | 177..320 | row 0 name glow |
| 14 | 68..177 | 112..158 | 174..283 | row 1 name glow |
| 16 | 187..228 | 124..159 | 293..334 | row 1 value, short ("On") |
| 17 | 189..245 | 106..164 | 295..351 | row 1 value, long ("Off") |
| 15 | 64..196 | 158..200 | 170..302 | row 2 name glow |
| 18 | 208..255 | 159..198 | 314..361 | row 2 value "16:9" |
| 19 | 208..328 | 160..197 | 314..434 | row 2 value "Standard" |
| 5 | 64..257 | 200..246 | 170..363 | row 3 name glow |
| 12 | 273..314 | 209..244 | 379..420 | row 3 value "On" |
| 13 | 274..330 | 190..248 | 380..436 | row 3 value "Off" |
| 6 | 59..307 | 239..303 | 165..413 | row 4 name glow |
| 7 | 57..327 | 285..354 | 163..433 | row 5 name glow |
| 8 | 54..214 | 332..385 | 160..320 | row 6 name glow |
| 20 | 221..292 | 342..382 | 327..398 | row 6 value, mode 0 |
| 21 | 217..295 | 338..385 | 323..401 | row 6 value, mode 1 |
| 22 | 217..413 | 335..386 | 323..519 | row 6 value, mode 2 |
| 9 | 59..264 | 367..442 | 165..370 | row 7 name glow |
| 41 | 72..240 | 418..441 | | language 5's extra word |

All corners have z = -1. Quads 0-4 are the main menu's words. 24-41 are language 5's (it maps n=5 to 37, n=16 to 36,
n=23 to 39) [read `0x80018DB4`, `0x80018EBC`, `0x80018D04`]. The other languages map to quads past 41.

**Who else uses quads 5-9 and 12-23.** Every `li r3,n; bl 0x80018824` with n in that set is inside `0x8001D52C`
[read: all 44 such call sites listed by script; n=5 `0x8001DB98`, n=16 six sites `0x8001D5CC`..`0x8001DA78`, etc.].

---

## 2. The input on state 14

The title channel `0x8023D95C` has 7 rows [read, DOL]: A/Start `0x80019F30`, B `0x8001AE28`, Up/Down `0x8001B588`,
Left/Right `0x8001B300`, and two empty ones. Each handler first calls `0x8013E154`, then switches on the state byte
TM+62 [read].

### 2.1 Up/Down `0x8001B588(dir)`, state 14 → `0x8001B6A4` [read `0x8001B6A4`-`0x8001B708`]

- A card notice up: `0x800AFD54(dir)` takes it, the cursor stays.
- Else cursor += dir; `>= TM+40` → 0; `< 0` → TM+40 - 1. A plain wrap.
- Then sound 635 in both cases.
- **TM+40 is read only by Up/Down** (`0x8001B670` states 9/17, `0x8001B6CC` state 14, `0x8001B710` state 11,
  `0x8001B78C` state 7, `0x8001B8D0` the default) [read: every `lwz ,40(` in `0x80018000`-`0x80021000`].
- State 15 (Brightness) takes Up/Down away: it writes 0 into the channel's row 3 handler word and puts the slider
  `0x801160D4` in row 6 (`0x8001ABA4`-`0x8001ABB8`); B puts `0x8001B588` back (`0x8001B218`-`0x8001B234`) [read]. State
  16 (Volume) goes to `0x8001B900`: nothing [read, table `0x8023DBF0`].

### 2.2 A `0x80019F30`, state 14 → `0x8001A9FC` [read `0x8001A9FC`-`0x8001AE0C`, table `0x8023DB64`]

- A card notice up: `0x800B01C0()`, no sound.
- Cursor > 7: nothing, then sound 636 (`0x8001AE04`). So rows 8 and up are silent in the game but for the click.
- Rows:

| Row | Handler | What A does | Next state |
|---|---|---|---|
| 0 Save Settings | `0x8001AA38` | card save `0x800AEE4C(1, 0x8001F9C0)`, `0x800B3FE8(0,1)`, `0x800AFC20(1,0)` | 14 (card notice) |
| 1 Subtitles | `0x8001AA64` | flips bit 0 of `0x80307180` (language 5: flips `r13-24720`, reloads text) | 14 |
| 2 Widescreen | `0x8001AAD4` | flips `r13-24660` | 14 |
| 3 Rumble | `0x8001AAE4` | `0x8013D93C(1, !x)`, buzz 30 frames when turned on | 14 |
| 4 Volume | `0x8001AB18` | camera push, row 4 of the channel replaced, `0x80114FA4(102,154,254)`, state byte := 16 directly (no fade) | 16 |
| 5 Brightness | `0x8001ABA4` | slider made, `0x80019470(15, 0)` (a fade) | 15 |
| 6 Audio Mode | `0x8001ABF4` | `r13-23012` 0→1→2→0, `0x8019D944(x,1,1)` | 14 |
| 7 Language Select | `0x8001AC50` | return point 14 / row 7 in `r13-26056` / `r13-26060`; `0x800195D8(32, 0)` | 32 |

- A on state 15 (Brightness): nothing (`0x8001ADF8`). A on 16 (Volume): `0x8001AD68`, puts row 4 back, `0x80115120`,
  then state 14, cursor 4, TM+40 = 8, `r13-30872 = 1` [read].
- **State 17 is not an Options page.** It shares the chapter grid's Up/Down (`0x8001B65C`, steps of 4) and A
  (`0x8001A68C`) with state 9, and B sends it to state 9 [read `0x8001B1F4`]. The Options sub-pages are 15, 16 and 32.

### 2.3 Left/Right `0x8001B300`, state 14 → `0x8001B558` [read, table `0x8023DB84` entry 10]

Only a card notice takes it (`0x800AFE50(-dir)`). **Left/Right is free on Options.**

### 2.4 B `0x8001AE28`

| State | Where | What [read] |
|---|---|---|
| 14 | `0x8001B0C8` | card notice up: `0x800AFF14()`. Else if the settings differ from the snapshot at `0x803093DC` (`0x800AD368`), the card asks to save (`0x800AFC20(11,0)`). Else `0x8001FA38(1)`: whisper 631, TM+40 = 5, main menu (state 7) at cursor 2 if `r13-30872` |
| 15 | `0x8001B218` | slider freed, Up/Down handler put back, `0x80019470(14, 5)`, `r13-30872 = 1`, TM+40 = 8 |
| 16 | `0x8001B264` | channel row 4 put back, state byte := 14, cursor 4, TM+40 = 8, `r13-30872 = 1`, camera pop, `0x80115120(0)` |

### 2.5 Every write of TM+40 [read: every `stw ,40(` in the title module whose base is TM]

| Value | Where | When |
|---|---|---|
| 8 | `0x8001A618` | main menu A on Options → state 14, cursor 0 |
| 8 | `0x8001ADE0` | Volume A → state 14, cursor 4 |
| 8 | `0x8001B25C` | Brightness B → state 14, cursor 5 |
| 8 | `0x8001B2B8` | Volume B → state 14, cursor 4 |
| 8 | `0x8001FD30` | `0x8001FCB0(1)`, a card flow back to Options (snapshot taken again), cursor 0 |
| 5 | `0x8001B0B8`, `0x8001B19C`, `0x8001C250`, `0x8001C454`, `0x8001C720`, `0x8001FA6C`, `0x8001FB80`, `0x8001FE38` | back to the main menu |
| 12 | `0x8001A664`, `0x8001AFAC`, `0x8001B028`, `0x8001B1EC`, `0x8001B210` | chapter grids (9, 17) |
| 3 | `0x8001A768`, `0x8001A7A4`, `0x8001A990`, `0x8001A9F4`, `0x8001AFD0` | states 20, 22, 11 |
| 2 | `0x8001A5E4`, `0x8001A7E0`, `0x8001AD3C` | Load Game's card screen (13), state 250 |
| 7, 1, 0 | `0x8001A78C`, `0x8001C158`, `0x8001A144` | states 21, 18, 19 |

Language Select's return to state 14 writes no row count. The writes at `0x800195B8`, `0x80019708`, `0x8001F988` have
another base (`0x8023D7E0`, `r30+4192`), not TM [read].

Because only Up/Down reads TM+40, today's notify (write it before each Up/Down) covers every one of these writes.

---

## 3. What the host does today [read]

- `hle.c:17150-17185`: notifies on `0x8001B588` (TM+40 := 9), `0x80019F30` (cursor 8 → `frontend_open_page(
  FRONTEND_PAGE_TITLE_SETTINGS)`), `0x8001D52C` (records r3 for the picture). All gated by `title_on_options`: state
  14, language word `r13-26284 == 0`, no card notice. Installed at `hle.c:33449-33466` with first-word checks.
- `titlevideo.cpp:303-368`: texture 2 (`kOptionsHash 0xbefc5ef6c1629595`) is repainted with "More Options" under
  Language Select, 44 px La Belle Aurore (`tome_hand_cover`), left edge 174, baseline 452, ink (72, 97, 104), lit
  `0xA0E3F1` with a 6 px glow at 0.35. Two cached versions (lit / not).
- `titlevideo_generation` (`titlevideo.cpp:526-532`) keys the 850x480 size on `0x4D4F52450000 | lit`, or on the
  video's frame serial when the title video is on. The renderer mixes the generation into the texture's cache key
  (`gxcore_draw.cpp:2726-2730`), so a new generation means a new apply and upload [read].
- `frontend.cpp:2304-2330`: the page opened from the title shows Display, Sound, Graphics, Controls, then Mods and
  Quit game.

---

## 4. The picture: can the game's rows be moved? Yes [measured]

From `res/video/options.rgba` (stored upside down; flipped):

- Ink = luminance > 6 in x 140..440, y 60..470, leaving out the logo box (x >= 400, y < 200). It makes **52 connected
  pieces**. Each one given to the nearest row (centre against baseline - 8): **no piece is in doubt** (the second
  nearest row is always 8 px or more further). A picture coloured by row showed every row whole and nothing mixed.
- The ground under the words is black. Outside the ink grown by 3 px, the brightest point in that box is luminance
  3.2. So "erase the words" = paint the box black, and "lift a row" = copy its pieces (grown by 1-2 px for the soft
  edge) to a new place.
- Rows, as painted (upright picture px):

| Row | Ink y | Ink x | Baseline (where the ink stops) | x-height top |
|---|---|---|---|---|
| 0 Save Settings | 87..136 | 182..312 | 111 | 97 |
| 1 Subtitles | 120..152 | 182..273 | 150 | |
| 2 Widescreen | 162..190 | 178..293 | 188 | |
| 3 Rumble Feature | 206..247 | 179..354 | 236 | |
| 4 Volume Adjustment | 246..298 | 182..398 | 276 | 261 |
| 5 Brightness Adjustment | 291..349 | 177..423 | 323 | |
| 6 Audio Mode | 342..373 | 173..307 | 370 | |
| 7 Language Select | 376..432 | 174..351 | 406 | 393 |
| (8, today's More Options) | | 174.. | 452 | |

  Row gaps: 39, 38, 48, 40, 47, 47, 36 (then 46 to the ninth). Uneven, as painted.
- Ink colour: median (66, 89, 90), 95th percentile (82, 109, 115). Capitals 23-32 px tall, x-height about 14 px.
- Room: 9 rows fit (baselines 111..452; descenders end near 470 of 480). A tenth does not.
- The skull's glow starts at picture x 380-500 depending on the line; the text box up to x 440 holds nothing but words.

---

## 5. The design

### 5.1 The list

- 14 rows: the game's 0-7, then ours: 8 Display, 9 Sound, 10 Graphics, 11 Controls, 12 Mods, 13 Quit game (the rows
  `build_options` gives the title today, `frontend.cpp:2280-2330`). "Choose Character" goes on the main menu (the
  user's ask), not here.
- 9 slots. Slot k's baseline is the painted row k's (111, 150, 188, 236, 276, 323, 370, 406), slot 8's is 452. So with
  no scroll the screen is the game's own, pixel for pixel, with Display where More Options is today.
- Scroll s = 0..5. Row r shows in slot r - s when 0 <= r - s <= 8.
- **The cursor stays the game's.** TM+28 runs 0..13. The game's Up/Down wraps it with TM+40 = 14.
- s follows the cursor, kept by the host: `s = clamp(s_old, cursor - 8, cursor)`, then clamp to 0..5. Worked out in the
  draw notify from r3. It needs no other state: entering Options at cursor 0 gives s = 0; coming back from Volume (4),
  Brightness (5), Language Select (7) or a card flow (0) keeps a valid s; after a state load the first draw fixes it.

### 5.2 Hooks (4; three exist)

| # | Hook | Change |
|---|---|---|
| H1 | notify `0x8001B588` (exists) | TM+40 := 14 instead of 9 |
| H2 | notify `0x80019F30` (exists) | cursor 8..13 → open that row's page (§5.5) |
| H3 | notify `0x8001D52C` (exists) | from r3: work out s; give titlevideo one word (s, lit row or none); when s > 0 write the copy table for s (§5.3) |
| H4 | **new** notify `0x8019D358` | when lr == `0x8001D598`, language 0 and s > 0: r4 := the copy table for s |

All gated as today (`title_on_options`: state 14, English, no card notice), except that H3 and H4 must also run in state
16 (Volume draws the same screen) and during the fade out, using the draw's own cursor. The first word of `0x8019D358`
is `stwu r1,-16(r1)` = `0x9421FFF0` [read; encoding inferred]. Nothing hooks it today (grep). It has 43 call sites
(the kit, `ed fn`), title and Tome; the extra dispatch per call is small [inferred]. Install it with the others, before
`publish_codegen_options` (05 C6).

H4 rewrites an argument in a notify. A notify runs before the function on the same `cpu` (`hle.c:5765-5767`), so the
function should read the new r4 [inferred]. If that is not wanted in a notify, use an intercept that sets r4 and passes
through (05 §3.6: a passed-through intercept runs the function).

### 5.3 Moving the game's value words and glows: a copy of the quad table

For each s in 1..5, a copy of quads 0..23 (576 bytes) lives in host scratch, say `SCRATCH_MENU + 0xA000 + s * 0x240`
(free: the plaque bank ends at `+0xA000`, SPELL_COLOUR is at `+0x10000`, `hle.c:17192`). In copy s:
(Since step P7 the bank holds five plaques and ends at `+0xC000`; the copies moved to `SCRATCH_MENU + 0xC000`.)

- every quad of a row r >= s has its 4 y values moved by `dy = slot_y(r - s) - slot_y(r)`;
- every quad of a row r < s (scrolled off the top) has its 4 corners set to one point: a quad of no size draws nothing
  [inferred];
- quads 0-4 are left as they are.

Rows' quads: row 0: 23. Row 1: 14, 16, 17. Row 2: 15, 18, 19. Row 3: 5, 12, 13. Row 4: 6. Row 5: 7. Row 6: 8, 20, 21, 22.
Row 7: 9.

The moves (picture px = menu units):

| s | r=1 | r=2 | r=3 | r=4 | r=5 | r=6 | r=7 |
|---|---|---|---|---|---|---|---|
| 1 | -39 | -38 | -48 | -40 | -47 | -47 | -36 |
| 2 | off | -77 | -86 | -88 | -87 | -94 | -83 |
| 3 | off | off | -125 | -126 | -135 | -134 | -130 |
| 4 | off | off | off | -165 | -173 | -182 | -170 |
| 5 | off | off | off | off | -212 | -220 | -218 |

Why a copy and not the game's table:

- **Aurora reuses an array's upload within a frame while its base pointer is the same** (`command_processor.cpp:
  1927-1933`; the cache is dropped only when the base changes, `:2028-2034`, `:1443-1449`, or at the frame's end,
  `common.cpp:2156`) [read]. Writing new y values into `0x8023C780` mid-frame can be missed, depending on what drew from
  it first. A different base pointer per s always gets a fresh upload.
- Each copy's bytes are the same every time it is written. So H3 can rewrite copy s every frame (no canary, safe after a
  state load), and a GX thread still reading last frame's copy sees the same bytes [inferred].
- The game's table stays as it was: the main menu, the cross-fade and every other screen are untouched.
- Nothing else in the title draws quads 5-9 or 12-23 (§1.2), but other screens load other data from `0x8023C780`'s
  neighbourhood (the channel at `+4572`), another reason not to write there.

Not chosen:
- Intercepting `0x8019D3B4` / `0x8019C7C8` by lr (120 and 177 call sites, every call would dispatch) can hide a word but
  cannot move one without a table anyway.
- Writing the game's table in place (the Aurora cache above; a save state would keep the shifted values).

Check that `SCRATCH_MENU` resolves as a vertex array: the plaque bank there is read by the renderer as textures, which go
through the same guest resolver (`recomp.cpp:48`) [inferred]. One `[trace]` line on H4 and a picture settle it.

### 5.4 The picture (titlevideo.cpp)

Once, from the decoded picture (`apply` gets it as RGBA):
- the clean ground: the text box x 140..440, y 60..470 (less the logo box x >= 400, y < 200) painted black;
- 8 row masks: the 52 ink pieces by row (§4). A small flood fill in C++; check 8 rows, none empty.

For each (s, lit), on demand, a few kept:
- clean ground;
- each game row r >= s copied from the original at `dy` (§5.3), so its glow quad lands on it exactly;
- each of our rows j with slot k = 8 + j - s in 0..8: La Belle Aurore at slot k's baseline, left edge 174 as today, ink
  colour; lit (pale cyan plus glow, as `paint_more`) when cursor == 8 + j;
- optional: a faint mark above slot 0 when s > 0 and below slot 8 when rows are hidden below (the user's call).

The game draws its own glow for rows 0-7 (on the moved quads); the host lights only ours, as today.

Generation: one word from H3, for example `s * 16 + (lit ? cursor - 8 : 15)`, replacing `g_options_row == 8` in
`titlevideo_generation` (both branches: with the video on, `serial * 64 + key`). Up to 6 x 7 versions of 1.6 MB; the
renderer's cache keeps them by key [read `gxcore_draw.cpp:2726`; how long it keeps old ones not read].

The same clean ground is the background the user wants behind the sub-pages ("the background being the same as the
options menu"): it could be saved once (say `res/video/options-clean.png`, upright) for the RmlUi pages [inferred].

### 5.5 Opening ours

- H2: cursor 8 + j → a page per row: Display (Options section 1), Sound (section 2), Graphics, Controls, Mods, Quit.
  `frontend_open_page` takes one page id today (`FRONTEND_PAGE_TITLE_SETTINGS` opens the Options list,
  `frontend.cpp:4499-4502`); it needs one id per row, each opening that screen as the root so Back closes it
  (`g_root_closes`, `frontend.cpp:597-600`). Quit may want a Yes/No first. That is the menu side's work.
- The game plays 636 for the press itself; the game's state is untouched while the page is up, so closing it shows the
  title Options at the same cursor and scroll.
- `run_menu` waits until the buttons are let go before returning (05 C9), so the B that closes the page does not also
  leave Options.

### 5.6 The cases to watch

| Case | What happens |
|---|---|
| Wrap Up from 0 | cursor 13, s = 5 (the game's wrap, `0x8001B6F4`) |
| Wrap Down from 13 | cursor 0, s = 0 |
| Volume (state 16) | cursor 4 is always in view (s <= 4); the same screen and copy are drawn; the bar and its words are placed absolutely [inferred] |
| Brightness (15) | its own picture (texture 24, `0x8001ECBC`-`0x8001ECC4`); the cross-fade draws Options with the old cursor, same s |
| B to the main menu | the fade-out draws Options with its old cursor, same s; the main menu uses the game's table |
| Card notice | H1/H2 stand aside (`title_on_options`), the game's notice code takes the press |
| Save state loaded on Options | s recomputed at the first draw; copies rewritten each frame |
| Other languages | no hook acts, the game's 8 rows as today |

---

## 6. Does the painted hand match? [measured]

- The game's words: a thin, slanted, angular script with long loops (capitals, g, j, f). Capitals 23-32 px, x-height
  about 14 px, "Language Select" 177 px wide.
- La Belle Aurore (stbtt pixel height = ascent - descent, as `tome_hand_cover` uses):

| px | x-height | capital L | "Language Select" width |
|---|---|---|---|
| 44 (today) | 11 | 17 | 153 |
| 52 | 13 | 20 | 178 |
| 56 | 14 | 22 | 190 |
| 60 | 15 | 23 | 206 |

- In the shot of today's row (`menus-2026-11-17/6-title-more-options.jpg`, cropped and enlarged) "More Options"
  is lit and reads as a cousin of the game's hand: same slant, same pale cyan when lit, the glow close to the game's.
  It is clearly smaller than "Language Select" above it, and rounder, with short capitals. **About 80% of the game's
  size.** At 52 px it matches the game's width and x-height; its capitals stay shorter (LBA's are small). Suggest 52 px,
  bold about 0.8, ink about (68, 92, 96) (the measured median is a little greener than today's (72, 97, 104)).
- The game's own words cannot spell ours: only "Settings" (in Save Settings) and "Adjustment" are near (02 §6).
- Re-lettering the game's 8 rows too ("all in one hand") would need the 8 name glows hidden (they are pictures of the
  game's hand, they would no longer sit on the words). That is easy in the copy table (collapse quads 23, 14, 15, 5-9),
  but the value words (On, 16:9, Stereo Surround) would still be the game's hand beside them. Lifting the game's rows
  keeps them exactly native; only our six differ. Recommended.

## 7. Other languages

- Each language has its own pack (`UMnMenuF/G/S/I.cmp`), its own Options picture (another hash), and other quads:
  `0x80018824` sends French, German, Spanish and Italian to quads past 41, and language 5 to 24-41 [read
  `0x80018844`-`0x80018F90`].
- None was measured. English only is the plan; `title_on_options` already requires language 0 (07 review).
- Gap: in another language there is no title route to our settings at all (Esc opens the menu over the title only with
  the debug menu on, `frontend.cpp:4470-4473`). A fallback would be the free Y on the title (`0x8001B920`, a `blr`;
  02 Way C) [inferred]. The user decides.

---

## 8. Risks

| Risk | Why | Answer |
|---|---|---|
| H4's r4 rewrite not taken | notify rewriting an argument is not used elsewhere that I read | trace once; or an intercept that passes through |
| Scratch not resolved as a vertex array | `SCRATCH_MENU` is past 32 MB | the plaque bank there is drawn; trace once |
| One frame of mismatch on a scroll | the texture's generation is read when the renderer resolves the texture, the copy is chosen on the game thread; with threaded GX they can be a frame apart [inferred] | small; both change on the same game frame |
| A quad of no size draws something | GPUs draw nothing for it [inferred] | a picture with s > 0 |
| Texture cache growth | 42 versions of 1.6 MB at worst | keep only a few in titlevideo; the renderer's eviction not read |
| Up/Down while Brightness is up | its handler is removed from the channel | nothing to do: H1 never fires there |
| A new row count elsewhere | TM+40 is read only by Up/Down (§2.1) | H1 rewrites it on each press |
| The title video on | the 850x480 generation is the video's serial | fold the key into it (§5.4) |
| The hooks.json of the kit | `ed fn 0x8001D52C` says "no dispatch entry", though `hle.c:33449` installs a notify | the kit's facts may be stale; trust hle.c [read] |

---

## Open questions

1. The rows and their order: Display, Sound, Graphics, Controls, Mods, Quit game? Anything else (Save States from the
   title)? Does Quit need a Yes/No?
2. Scroll marks above/below the list, or none?
3. The label size: 52 px recommended; one picture beside the game's rows settles it.
4. Lift the game's 8 rows (recommended) or re-letter all 14 in La Belle Aurore?
5. Does H4's rewritten r4 reach `GXSetArray`, and does the scratch copy resolve? One traced run.
6. What do the title's other states draw while the menu is paused over Options (music, the fade)? Not read here.
7. Another language: leave it, or add the Y fallback?
