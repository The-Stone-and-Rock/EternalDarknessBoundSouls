# The critic's check of 08-11, and the plan to build

Read-only, 2026-11-17. Nothing was built or run. Only this file was written. Line numbers are for 78dc1e9.

Tags: **[read]** = read in the file:line, `build/ed-db/disasm.txt`, or `generated/main.dol` (dumped with a small DOL
reader in the scratchpad). **[inferred]** = follows from what was read, not seen in a run.

The ask, in short (the user, 2026-11-17): (1) the title's Options screen lists all of our options where "More Options"
is now, and it scrolls; (2) each of our rows opens its page on the same skull background; (3) "Choose Character" above
Options on the title's main menu, opening the launcher's character page; (4) in game, the Tome's Settings and Characters
open the same pages, drawn as Tome pages, not the launcher's. Back rows and B / Esc everywhere.

---

## 1. Spot-checks: what holds

### 08 (frontend skins): holds, one number wrong

| Claim | Check |
|---|---|
| `Row` struct at frontend.cpp:325-344, one row model for every kind | [read frontend.cpp:325-344] |
| `past_root` closes on InGame/Launcher, plus ModsPage when the root is Characters | [read frontend.cpp:2804-2808] |
| `frontend_frame` sets `g_in_game`, `g_options_return = InGame`, roots by page, then `report_pause`, `hle_coop_ui_over(true)`, `run_menu` | [read frontend.cpp:4468-4521] |
| La Belle Aurore not loaded into RmlUi (five fonts only) | [read frontend.cpp:3019-3023] |
| `hle_coop_ui_over(true)` turns the split menu off | [read hle.c:13896-13903] |
| `calculate_present_viewport`, `half_of`, the menu fit in a half | [read gpu.cpp:338-363; aurora.cpp:756-765, 991-1001] |
| `dol_aurora_get_display_info` gives the window size and the aspect | [read aurora_backend.cpp:3070-3081] |
| `#hint` sits outside `#book` | [read menu.rml:9-21] |
| The launcher's "Choose Character" is ModsPage section 3 (the two cards) | [read frontend.cpp:1450-1466, 1215-1238] |

**Wrong: the Tome page's place in the wide layout.** 08 §4 uses 250/851..682/851 (0.294..0.800). That is the
*title's* picture quad (x -106..745), not the Tome's. The Tome's 2D projection is `C_MTXOrtho(0, 480, -80, 720)` when
`r13-24660 == 1` [read 0x8019CFF8-0x8019D020; DOL floats 0x806233FC = -80.0, 0x80623400 = 720.0]. So page x 144..576
is **(144+80)/800 = 0.28 .. (576+80)/800 = 0.82** of the picture. 11 §2.2 has this right and checked it on three
pictures. Fix `tome_page_rect`: `l = wide ? 0.28f : 0.225f, r = wide ? 0.82f : 0.9f`.

### 09 (title Options scroll): holds

| Claim | Check |
|---|---|
| `0x8001D934: cmplwi r29,7`: cursor > 7 skips all glow | [read disasm] |
| `0x8001D594: bl 0x8019D358` with r4 = `0x8023C780` (return `0x8001D598`) | [read 0x8001D590-0x8001D594] |
| `0x8019D358(r3 texcoords, r4 positions)`: GXSetArray(9, r4, 6), GXSetArray(13, r3, 4); first word `stwu r1,-16` | [read 0x8019D358-0x8019D3B0] |
| Quads 5, 12, 16, 23 as tabled | [read DOL 0x8023C7F8, 0x8023C8A0, 0x8023C900, 0x8023C9A8] |
| Aurora reuses an array's upload while the base pointer is the same | [read command_processor.cpp:1921-1933, 2027-2034, 1443-1449] |
| A notify runs before the function on the same cpu, so a changed r4 is seen | [read hle.c:5764-5786: notify, then `return false` and the recompiled function runs] |
| 43 callers of `0x8019D358`, no host hook | [read: grep of disasm, of runtime/host] |
| The guest resolver does not care what kind of resource (texture or vertex array) | [read .deps/GXRuntime/src/guest_memory.c:229-317: `resource` is only stored]. So 09's "does scratch resolve as a vertex array" risk is small: the plaque bank there already resolves as textures |

### 10 (Choose Character): holds, two small notes

| Claim | Check |
|---|---|
| State 7 Up/Down wraps over TM+40, skips only rows 4 and 3 | [read 0x8001B784-0x8001B804] |
| Whispers 0 629, 1 630, 2 631, 3 **724**, 4 **632**, 5+ none (02 had 3/4 swapped) | [read 0x8001B82C-0x8001B8BC] |
| A: cursor 5 goes to `0x8001A678`, sound 633, r28 = 0 so no 636 | [read 0x8001A4A4-0x8001A4D0, 0x8001A678-0x8001A688, 0x8001ADFC] |
| `0x8001DF90` entry `stwu r1,-352` (0x9421FEA0); cursor kept in r28 | [read 0x8001DF90-0x8001DFB4] |
| Glow: texture `cursor + 6` at `0x8001E414`, quad `0x80018824(cursor)`, drawn at `0x8001E42C` | [read 0x8001E3F8-0x8001E430] |
| Quads 2, 3, 4 | [read DOL 0x8023C7B0, 0x8023C7C8, 0x8023C7E0] |
| jtg.sh walks Down x3 | [read tools/ed/jtg.sh:22] |
| titlevideo claims `kTitleHash` only with the video on; one generation for every 850x480 picture | [read titlevideo.cpp:518-532] |

- Small correction: the `r13-26460 == 0` test at 0x8001E318 jumps to 0x8001F51C, past the **glow as well**, not only
  the unlit Jump/Credits words [read 0x8001E318-0x8001E320, 0x8001E430]. Harmless for the plan.
- Texture 11's bytes `0020 0000 ffffffff` (02 §3.2) read as one CMPR block in its 4-colour mode, every pixel the
  third colour: opaque, near black [inferred from the CMPR format]. So the dark box is real wherever the picture under
  quad 5 is not black (the title video). H4 is needed.

### 11 (Tome pages): holds, one claim outdated

| Claim | Check |
|---|---|
| B `0x8010180C` (first word 0x9421FFF0) | [read]. Note: it **calls the exit `0x801016D8` itself** (`bl` at 0x80101818), then writes `r13-24244 = 2`, then input taken. So the exit notify fires on B |
| Projection -80..720 in the wide layout | [read, as above] |
| Player 1's four stops, player 2's one | [read hle.c:16964-16973] |
| The five `kStops` hooks, installed together | [read hle.c:33427-33448] |
| `frontend_frame` does not run in a copy turn | [read hle.c:5506-5511] |

- Outdated: "no host function gives the page's rectangle" (11 §7.2). True of a ready function, but 08 shows it is ten
  lines in frontend.cpp from `dol_aurora_get_display_info` [read]. It is not a blocker for Way B.

---

## 2. Where the four disagree, and the answer

| # | Question | 08 | 09 / 10 / 11 | Answer |
|---|---|---|---|---|
| D1 | What "Choose Character" on the title opens | ModsPage section 3, the two players' cards (new `TITLE_CHARACTERS`) | 10: `FRONTEND_PAGE_CHARACTERS`, player 1's roster only | **08.** The user said "the launcher character selection menu", and the launcher's row opens the cards [read frontend.cpp:1466]. It also answers 10's open question 3 (player 2 from the title) for free. And `FRONTEND_PAGE_CHARACTERS` would get the Tome skin under 08's own page-number rule |
| D2 | Where the title's list lives | An RmlUi root list in the title skin | 09: the game's own Options screen, 14 rows, scrolling natively | **09.** The user asked for "our options listed on that options screen ... and the menu can scroll". 08's list is the fallback if 09's quad copy fails its traced run |
| D3 | The rows of that list | Display, Sound, Graphics, Controls, Co-op, Characters, Challenge, Cheats, Title video, Quit, Back | 09: Display, Sound, Graphics, Controls, Mods, Quit | **Open (user).** Every row in 09's native list must open a page: a toggle (Title video) would need its value painted and Left/Right handled by the host. Characters is on the main menu now. Back is the game's B. Suggest: Display, Sound, Graphics, Controls, Co-op, Mods (Challenge, Cheats, Sanity, Title video), Quit game: 15 rows, s = 0..6 |
| D4 | The Tome pages | Way B: the RmlUi page, Tome skin, paused | 11: Way A, native page in the game's Options, not paused | **Way B now.** The user's standing direction (coop-todo.md:242-248, 2026-11-15): "the launcher's existing RmlUi pages and functions, drawn with a Tome skin ... as overlays, paused as the pause menu is". Today's ask says the same ("act the same way as the main menu"). Way A stays on file for a later "don't pause player 2" ask |
| D5 | The Tome page rect, wide | 0.294..0.800 | 11: 0.28..0.82 | **11** (§1 above) |
| D6 | Host scratch `SCRATCH_MENU + 0xA000..+0x10000` | | 09: the quad copies; 11: the row stubs | Both claim it. Give the title copies `+0xA000..+0xAC00` (6 x 0x240 = 0xD80; round up to `+0xB000`) and keep the rest for later |
| D7 | `titlevideo_generation` | | 09: fold (s, lit row); 10: fold the main menu's lit bit | One word for all 850x480 pictures [read titlevideo.cpp:526-532]: `key = s * 32 + options_lit * 2 + menu_lit` (and `serial * 256 + key` with the video on). Both must change it in one place |
| D8 | Which hook carries the title's draw work | | 09: the existing `0x8001D52C` notify; 10: a new `0x8001DF90` notify | Both. `0x8001D52C` for Options (states 14, 16, fade); `0x8001DF90` when r3 == 7. Separate, no clash |

---

## 3. What is still missing, part by part

### Part 1: the title's Options as a scrolling list (native)

1. The row list (D3). The user decides. Nothing else waits on it but the count.
2. **A page id per row** in frontend.h, and a case for each in `frontend_frame`. The `switch` today falls to
   Characters on any unknown id (`case FRONTEND_PAGE_CHARACTERS: default:`) [read frontend.cpp:4491-4492].
3. **Back must close a page opened as the root.** Back from Display (OptionsPage) goes to Options
   (`leave_options_page`, frontend.cpp:2195-2199), from Graphics to Options (:2898-2901), from Mods to `g_mods_return`.
   None of those is "past the root" (`past_root`, :2804-2808), so the user would land on the RmlUi Options list. Needed:
   `past_root(s)` also true for `s == Screen::Options && g_root != Screen::Options` and
   `s == Screen::Mods && g_root == Screen::ModsPage` (or one rule: "the screen Back goes to is not the root nor below
   it"), and Controls opened with `g_controls_return = InGame`.
4. **The gate.** `title_on_options` requires state 14 [read hle.c:17166-17172]. H3/H4 must also run for state 16
   (Volume draws the same screen) and the fade out. Gate H4 on `lr == 0x8001D598` and English only, using the s that
   the same frame's H3 worked out. Volume's bar is its own panel at a fixed place (`0x80114FA4(102, 154, 254)`, 09
   §2.2), so a scrolled row 4 is fine [inferred].
5. **The ink mask in C++.** 09's 52-piece split was measured in a scratch script; titlevideo.cpp needs its own flood fill
   with 09 §4's boxes and baselines, and a check that 8 rows come out, none empty, else fall back to the 9-row screen.
6. **One traced run with pictures at s = 0, 1, 5** before more: does H4's r4 reach `GXSetArray`, does a zero-size quad
   draw nothing.

### Part 2: the title's sub-pages on the skull

1. The skin plumbing (08 §5): `Skin`, `g_skin`, body classes, La Belle Aurore loaded. **Choose the skin by the
   caller, not by the page number**: the same `FRONTEND_PAGE_CHARACTERS` is opened by the Tome [read hle.c:16965,
   16972]. Either title-only ids or a third atomic (`frontend_open_page_skin`).
2. **A "from the title" flag for every title page**, not only `TITLE_SETTINGS`. Today `g_in_game` is true over the
   title (frontend.cpp:4475), so the cards' note says "A change reloads the game where you stand" (:1227), and the
   Cheats rows are enabled (`r.enabled = g_in_game`, :1247). Use `g_from_title` (renamed `g_on_title`) for both.
3. **The background.** titlevideo.cpp already has the upright-able Options picture (`keep_skull`, titlevideo.cpp:549-551)
   and, with Part 1, the clean ground. Write it once as `res/video/options-clean.png` (that folder is git-ignored,
   .gitignore:10; `res/ui` is not) and reference it from the rcss as `../video/options-clean.png`. Put it on a div
   sized to the picture's viewport (the same math as `tome_page_rect`), not `cover`, so 4:3 and 21:9 match the game.
4. **The rows' box**: the erased word column, picture x 170..440, y 85..470 of 850x480, in viewport-relative units.
   About 9 rows fit (09 §4), so Display (many rows) needs the list scroll (08 §6.2): `g_list_top`, `g_list_lines`,
   drawn rows keep their `row<i>` ids.
5. The cards page and the roster in the title skin: their geometry is in `vh` and full-window today (menu.rcss:336-342,
   761-940). They need a title-skin block (colours, no parchment, a dark panel).
6. Where `#hint` goes.

### Part 3: "Choose Character" on the title's main menu (native row, RmlUi cards)

1. The label ("Choose Character", crosses her chin; or "Characters") and the sound on reaching it. The user decides.
2. Open the cards: `TITLE_CHARACTERS` → `Screen::ModsPage`, `g_mods_section = 3`, `g_root = ModsPage`,
   `g_mods_page_return = InGame`. Back from the cards then closes (`mods_page_back` → InGame → `past_root`), and a
   card's roster comes back to the cards (`go_back` Characters → ModsPage, frontend.cpp:2917-2920) [read].
3. **A close signal** for the idle clock reset: a counter bumped where `run_menu` returns (frontend.cpp:4514), read by
   the `0x8001DF90` notify.
4. The state-7 gate for the extended `title_more_updown` / `title_more_choose` (they only know state 14 today).
5. jtg.sh to Up x2; then `ed check` and the other scripts that call it (check.py, coopcheck.py, stops.py, variant.sh,
   away.ps1 name it [read grep]).
6. The HD title picture is lost with the video off once `kTitleHash` is always claimed (10 risk). Tell the user.

### Part 4: the Tome's Settings and Characters as Tome pages (Way B)

1. `tome_page_rect` with the corrected constants (§1), half = player when the split is showing. 21:9 not looked at.
2. **The co-op picture under the page.** Today the Tome fills the window under any menu: "Behind the menu the Tome's
   picture can fill the screen, with stray page labels" [read docs/coop-next-session.md:875]. The fix is in
   `hle_coop_ui_over`: keep `aurora_split_menu(owner, mod_split)` so the `menuHalf` present draws the Tome in its half
   and the partner's last view in the other. Which of its three calls caused the old "nothing shown" is not traced.
   Read the `s_keepOnly` entry in docs/coop-failed-attempts.md:17-29 first: a menu frame that is neither a menu nor a
   world frame must still reach the window. Needs a traced run with pictures before building on it.
3. The Tome rcss block, every size in `em`, transparent body, `tome/parchment.png` on `#book`, Tome ink, La Belle
   Aurore labels and IM Fell values.
4. The compact roster (3 x 3, a details strip) for both players; player 2's from their copy Tome (`g_menu_player = 1`
   gives their pad, frontend.cpp:107-124).
5. Hide Aspect ratio in the Tome skin: `sync_widescreen_option` flips `r13-24660` without the Tome's art reload
   (hle.c:6236-6267, 11 §6.5). This holds for Way B too, since the Tome resumes after the page.
6. `#hint` inside the page or dropped (it would span both halves).
7. Player 2's page pauses player 1. That is the 2026-11-15 direction; say it in the handoff.

---

## 4. The plan (one page)

**Design.** One row model, one RmlUi document, three looks. The rows stay in `frontend.cpp`'s builders; nothing is
rebuilt in hle.c. The game draws only what already lives in its own screens: the title's Options list and the title's
main-menu row, by repainting the game's own pictures and moving its own quads. Every page that opens (Display, Sound,
Graphics, Controls, Co-op, Mods, Save States, the cards, the roster, Button mapping) is the RmlUi page, in the **title
skin** over the title and the **Tome skin** over the Tome, launcher skin otherwise. Every page has its Back row; B / Esc
go back one level, and Back past the page that was opened closes the menu.

| Drawn by | What |
|---|---|
| The game's 2D calls, host-painted pictures (titlevideo.cpp) | Title Options rows 0-13 with scroll (texture 2 repainted per (s, lit), the game's value words and glows on a shifted copy of quads 0-23); the main menu's "Choose Character" row (texture 1 repainted, quads 2-4 moved 34 down) |
| The game's 2D calls, plaque bank (exists) | The Tome's four plaques, player 2's one |
| RmlUi, title skin | Every page opened from the title: cleaned skull background in the picture's viewport, La Belle Aurore teal rows, lit 0xA0E3F1 with a glow, the list scroll |
| RmlUi, Tome skin | Every page opened from a Tome plaque: placed on the Tome's page rect (its half in co-op), parchment, Tome ink, compact roster |

**Hooks** (all English-gated on the title; install with the existing tables, before `publish_codegen_options`).

| Address | Kind | Today | New work |
|---|---|---|---|
| `0x8001B588` Up/Down | notify (exists) | TM+40 := 9 on Options | state 14: TM+40 := 8 + our rows; state 7: TM+40 := 6, TM+28 := dest - dir in the order 0,1,5,2,(3),(4) |
| `0x80019F30` A | notify (exists) | cursor 8 → More Options | state 14: cursor 8.. → that row's page id; state 7, cursor 5 → `TITLE_CHARACTERS`, mark the idle reset |
| `0x8001D52C` Options draw | notify (exists) | records r3 | s = clamp(s_old, c-8, c); hand (s, lit) to titlevideo; write the copy table for s |
| `0x8019D358` bind arrays | **notify (new)** | none | lr == 0x8001D598 and s > 0: r4 := copy[s] |
| `0x8001DF90` screen draw | **notify (new)** | none | r3 == 7: lit row to titlevideo; quads 2-4 absolute (originals for other languages); idle reset after a close |
| `0x8019D3B4` quad draw | **intercept, pass-through (new)** | none | skip only lr == 0x8001E430 and r28 == 5 |
| Tome `0x80101838` A etc. | intercepts (exist) | open a page | unchanged; the page opens in the Tome skin |
| `hle_coop_ui_over` | host | split off | keep the split menu under a Tome page (traced first) |

**Build order** (run `ed test` after each; pictures for every visual step).

1. **Frontend groundwork, no visible change.** La Belle Aurore loaded; `Skin` chosen by the caller; title page ids;
   the generalised `past_root`; `g_on_title` for notes and cheats; the list scroll for every list; a close counter.
   Check the launcher and the Tome pages look as before.
2. **Choose Character on the title** (smallest native piece; proves the `0x8001DF90` notify, the `0x8019D3B4`
   intercept, the texture-1 repaint and the shared generation word). Opens the cards in the launcher look. Fix jtg.sh.
3. **The title's Options scroll** (09). First a traced run at s = 0, 1, 5. Rows open their pages in the launcher
   look.
4. **The title skin** for every page from the title, the cards and the roster included, on the cleaned skull.
5. **The Tome skin, solo**: `tome_page_rect`, the rcss in em, the compact roster, Aspect hidden.
6. **The Tome skin, co-op**: the `hle_coop_ui_over` change with pictures first (record any failure in
   coop-failed-attempts.md), then both halves, player 2's Characters.
7. Later, only if asked: Way A (11), so a Tome page no longer pauses the other player.

**For the user to decide before step 2-3:** the title list's rows (D3); the label "Choose Character" or "Characters";
a sound on the new row; scroll marks or none; English only (other languages keep the game's 8 rows and have no title
route to our pages).
