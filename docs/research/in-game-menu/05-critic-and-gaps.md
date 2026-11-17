# Critic's check of reports 01-04, and the gaps for building

Read only research, 2026-11-17. PAL disc (GEDP01). Nothing was built or run.

This file checks the four reports in this folder against the code and the disassembly, lists where they disagree, and fills
the gaps that matter for building:

- (a) a native-looking entry and page in the title screen's Options,
- (b) the launcher's settings as Tome pages for player 1,
- (c) later, the same for player 2 in their own copy Tome.

Tags: **[read]** means read in `build/ed-db/disasm.txt`, in `generated/main.dol` (bytes read with a small DOL reader), or in
the file named. **[seen]** means seen in a picture already in the repo. **[inferred]** means not verified. `r13 = 0x80625420`,
`r2 = 0x80628460`.

---

## 1. Claims checked and confirmed

Eighteen spot checks. Each one matched.

| # | Report | Claim | Evidence |
|---|---|---|---|
| 1 | 03, 04 | The ring table at `0x8023A130` is Controller (294,43,120,32), Audio (217,62,72,32), Display (420,62,86,32), Load/Save (292,87,121,32) | DOL bytes read: `294 43 120 32 217 62 72 32 420 62 86 32 292 87 121 32` |
| 2 | 03, 04 | Options binding table `0x8024EDD8`: 5 rows, masks `0x03001000`, `0x200`, `0x100`, `0x10030000`, `0x200C0000`, handlers `0x801018C0`, `0x8010180C`, `0x80101838`, `0x8010179C`, `0x8010172C`; vertical row multiplier -1 | DOL words read; vertical row's +16 word is `0x1FFFFFFF` (dead zone 8191, multiplier -1) |
| 3 | 03, 04 | Up/Down writes an absolute index: negative gives 0, otherwise 3; sound 545 at 50 only if the index changed | `0x80101748` `extsh.`, `0x80101758` branch, `0x80101764` `bl 0x801DD354`, `0x80101770`-`0x80101780` [read] |
| 4 | 03, 04 | A on index 4 or more: sound 546, page exit `0x801016D8`, then no page | `0x80101854`-`0x801018A8`: `cmpwi r31,4; bge 0x801018A8` [read] |
| 5 | 03 | The Options frame reads 8 bytes past its 32-byte stack copy at index 4, into the saved r30/r31 | copy at `r1+8..+39`, r30 saved at `r1+40`, r31 at `r1+44` (`0x80101994`-`0x801019E0`) [read] |
| 6 | 04 | The ring call in the Options frame is at `0x80101A18`, return address `0x80101A1C` | [read] |
| 7 | 03, 04 | The gold ring `0x8019CC10` sets effect `0x801E1D74(1, 1282, …)` with f1 = 2.0 and colour `0x646400FF`; the dark ring `0x8019CB6C` uses 1058, 1.5 and `0x320000FF` | DOL words at `0x806233E0`: `320000FF 3FC00000 3F800000 646400FF 40000000` [read] |
| 8 | 03 | Options entry sets frame `0x80101980`, exit `0x801016D8`, channel 5 on `0x8024EDD8`, the two hints with style `0x8024EE78` = "ButtonText", range (0,3,3), index 0 | `0x80101624`-`0x801016C4` [read]; string read from the DOL |
| 9 | 03 | L/R/Start does nothing while Z is held; Start closes; L Journal; R Inventory; sound 495 | `0x801018C8` mask 16 held; `0x80101910` mask 4096; `0x80101934` `lis r3,256` = L [read] |
| 10 | 03 | The Tome reopens on the last section in `r13-29060`; any value past 4 opens no page | `0x8011181C` stores it; switch `0x80111410`-`0x80111478` [read] |
| 11 | 03 | Both book layouts put the page at x 144..575; only the wings move in the wide layout | position tables `0x80252324` and `0x8025251C`, slot 7 identical; slots 12/13 go to -106 and 745 [read] |
| 12 | 02 | The title record is `0x80309124` and state 7's Up/Down skips Credits and Jump to Game by the bits of `0x80308A88` | `0x8001B5AC` `lis -32719; addi -28380`; `0x8001B7BC`-`0x8001B804` [read] |
| 13 | 02 | The title's Options draws no glow for a cursor above 7 | `0x8001D934` `cmplwi r29,7; bgt` [read] |
| 14 | 02 | Main menu A on Options: snapshot `0x800AD218(0x803093DC)`, state 14, `r13-30872 := 1`, 8 rows, sound 723 | `0x8001A5EC`-`0x8001A620` [read] |
| 15 | 02 | The title channel `0x8023D95C` has 7 rows; the X/Y/Z and Y handlers are lone `blr`s | DOL words read; `0x8001AE24: blr`, `0x8001B920: blr` [read] |
| 16 | 02 | The title's widescreen row only flips `r13-24660`; the host's sync writes the launcher's value back next frame | `0x8001AAD4`-`0x8001AAE0` [read]; `sync_widescreen_option` hle.c:6236-6267 takes a game change only after a Tome toggle call (`g_options_toggle_calls`) |
| 17 | 01 | The menu runs on the main thread in the default threaded GX; the game thread waits at most one frame later | aurora_backend.cpp:2324-2337 (`g_frame_hook` after Present), :1635-1640 (`ready` wait); main.c:903; `DOL_GX_THREAD=0` is the only way to one thread (aurora_backend.cpp:1367) |
| 18 | 01 | Back from the Tome's roster walks Characters → ModsPage → `g_mods_page_return` → Mods → `g_mods_return`, which starts as `Screen::Launcher` | frontend.cpp:560, :565, :2836-2840, :1373-1377, :2831-2834; `frontend_frame` sets only `g_options_return` and `g_characters_return_row = 0` (:4384-4392) |

Also confirmed: `mod_enemies` has no reader outside its own row (frontend.cpp:1170-1173 only) (01); image 27's hash
`0x7cd7b88669459bbb` names a 432x480 file in the HD pack, `textures/GED/splash, menu, and text/tex1_432x480_7cd7b88669459bbb_14.dds`
(04), which supports 04's hash method.

---

## 2. Claims corrected

| # | Report | Claim as written | Correction | Evidence |
|---|---|---|---|---|
| C1 | 04 §8 | Language word: 0 English, 1 German, 2 French, 3 Spanish, 4 Italian, "the order of the file names at `0x802522B4`" | **0 English, 1 French, 2 German, 3 Spanish, 4 Italian** (02 is right). Language 5 loads the English card text | Switch at `0x80020064`: 1 → `UMemcardTextF.bin` (`0x8023E000`), 2 → `…G.bin` (`0x8023DFEC`), 3 → `…S`, 4 → `…I`, 0 and 5 → `UMemcardText.bin`. The book loader `0x8002089C` agrees: 1 → `UWideInF.tpl`, 2 → `UWideInG.tpl`. The order of names in a table is not the index [read] |
| C2 | 03 §7 | The launcher's in-game menu "is a modal loop on the game thread" | **It is a loop on the main thread.** The game thread stops because its next `present()` waits (claim 17). Only with `DOL_GX_THREAD=0` does it run on the game thread (hle.c:5689-5693). So a host page can't guest-call the game from inside the menu; game calls must be queued to a game-thread hook | aurora_backend.cpp:2324-2337, :1635-1640 [read] |
| C3 | 01 §3.6.1 | Back goes to the ModsPage "at `g_characters_return_row`, which is whatever `g_mods_section` was" | The **row** is always 0 (`frontend_frame` sets it, frontend.cpp:4389). The **page** shown is `g_mods_section`'s (1 Co-op at start, or the section last opened this run). The rest of 01's path holds | frontend.cpp:561, :2836-2840, :4389 [read] |
| C4 | 01 §5 table | `0x80111078` / `0x80111234` are "the options book menu's open / frame" | They are the **whole Tome's** open and frame (every page), not an options menu. hle.c calls them the "menu" because the widescreen book reload watches them. `0x80111234` already has an intercept (`hle_book_menu_frame`) | hle.c:6160-6181 (`find_book`), 03 §4.1; `TW_TOME_FRAME` [read] |
| C5 | 04 §2 | The page draw is "called at `0x8011157C`" | `0x8011157C` loads the pointer; the call (`bctrl`) is at **`0x80111590`** (03 is right). Return address for a hook: `0x80111594` | [read] |
| C6 | 02 §5 | Hooks "fire only where the guest calls that address … a case label inside a function does not" | Right in effect, but the reason matters for building: every call (`bl` or pointer) goes through `dolrecomp_call` → `hle_dispatch` **only if the target is a chain stop**. Chain stops are computed once in `publish_codegen_options` (hle.c:5871-5891). **A hook installed after it must call it again**, or chunks chain straight past it (hle.c:5857-5859). `find_coop` and `find_widescreen_option` run before it (hle.c:34306-34317) | generated.h:737-777; hle.c:5854-5891 [read] |
| C7 | 03 §5, §9.3 | Place the label by writing element `+8`, `+10` | Writes alone don't re-lay the words. Use **`0x801DA9CC(elem, x, y)`**: it stores `+8`, `+10` and calls the layout `0x801DB088` (04 lists it). The element's colour is at **`+4..+7`** (RGBA): `0x801DA230(elem)` returns `elem+4`, and the Tome frame writes the hints' alpha to its `+3` | `0x801DA9CC`-`0x801DA9F0`, `0x801DA230`-`0x801DA23C`, `0x80111594`-`0x801115B8` [read] |
| C8 | 03 §4.1 | The Tome frame step 8 is `0x801DA574` "after the page frame" | Correct, but `0x8010FF0C(0,0,0,0)` runs between them (`0x801115CC`). Not traced; it may draw over the page too [inferred] | [read] |
| C9 | 02 §8, risk "A press leaks into the title" | Hold the pad after closing, as `g_coop_p2_hold` does | Mostly done already: `run_menu` waits until every button and key is let go before it returns (frontend.cpp:3139-3145). The A that opened it was taken by the game on its press edge before the menu opened [inferred] | frontend.cpp:3032-3146 [read] |
| C10 | 03 §9.7 | Install "next to the existing `0x80101600` hook" | That block runs only when `tome_copy_mode() == 2` (the default; `ED_COOP_TOME_COPY` changes it, hle.c:4046-4051), and only inside `find_coop`, which returns early off PAL or without the camera offset (hle.c:32680-32681). Not gated on `mod_coop`, so solo gets it too | hle.c:33019-33031 [read] |

### Disagreements between the reports (not errors)

| Topic | 03 says | 04 says | Verdict |
|---|---|---|---|
| How to draw the fifth stop | Take over the whole frame `0x80101980`; label as a game text element; a dark ring by `0x8019C9C8` | Repaint image 27 with a fifth plaque; move the ring by intercepting `0x8019CC10` when `lr == 0x80101A1C` | Both work. 03's is cheaper to run: the frame, Up/Down and A are Options-only functions. An intercept on `0x8019CC10` makes all 30 of its call sites leave chained code (C6). 04's painted plaque looks most native but repaints one texture per language. Best: 03's frame takeover now, 04's repaint later |
| Image 27's identity | "inferred", hash unknown (Open question 7) | hash `0x7cd7b88669459bbb`, decoded from `UBookPak.bin` | 04 settles it (HD pack file with that hash exists, §1). Still open: whether, like image 31 in `tome.cpp:39`, a second hash exists (tome.cpp lists two hashes for image 31) |
| Label sheet | open question: handwritten or plain | not stated | The corner hints made from the same entry (1,2) show their words in the old-book face, gold, with grey button plates [seen: `Screenshots for Claude/tome-characters/options-zoom.jpg`]. So a label made from entry (1,2) draws in the **plain sheet** = `tome-0` (IM Fell English) when Game text = Tome [inferred from the picture] |

---

## 3. Gaps filled by reading

### 3.1 Every hook can be guest-called around, and guest calls go through the hooks

- `hle_guest_call` (main.c:57-110) runs the function through `dolrecomp_call`, so **intercepts fire inside host guest
  calls too**, with `lr` = `0x0BADF010` at the top call. Examples that matter here:
  - `0x8019DB90` (the sound) has an intercept in copy mode 2 (`tome_copy_sound`, hle.c:33033-33041). A host sound
    played from a Tome hook goes through it. That is the path 04 §12 describes.
  - `0x8019C9C8` (the plain frame) has an intercept `hle_card_frame` (hle.c:3929-3931). It passes through unless `lr`
    is inside the title card screen's draw (`0x8001D0E8`..`0x8001D4A4`). A host ring call is safe.
  - `0x80111234` (the Tome frame) has `hle_book_menu_frame` (C4).
- Scratch for a format string: the stack gap `hle_guest_call` leaves (it moves r1 down 0x800, main.c:79) holds a string
  for one call. `0x801DA8CC` lays the words out into the element, so the string need not outlive the call [inferred].
  `0x81FFFE00` (03's choice) is named free only by one note (`docs/research/separate-rooms/picture-readers.md:64`);
  `SCRATCH_HUD` is `0x81FFF000` and its +0xC00 is in use (hle.c:7661).

### 3.2 (a) The title: text elements are drawn there, so a label in the game's own font is possible

- The title frame `0x80020144` draws all text elements at `0x800201DC` (`bl 0x801DA574`), right after the title draw
  `0x8001F5A4`, **in every title state except 10** (`0x800201D0`-`0x800201D8`). [read]
- The title's own Volume row makes a text element: `0x80114FA4` calls `0x801DB374(r13-26492, 0, 37, 0, 1, "ItemText")` and
  places it with `0x801DA9CC(elem, x+225, 350)` (`0x80114FF8`-`0x80115034`). So the boot text bank `r13-26492` is loaded
  on the title, and an element made from it draws there. [read]
- So report 02's Way B can use a **game text element** for its label instead of repainting texture 2:
  make it from an existing entry (`r13-26492`, group 0, entry 37 is known to work), set the words with `0x801DA8CC`, place
  it with `0x801DA9CC` under "Language Select", and show "chosen" by its colour (`+4..+7`) [inferred]. It draws in the
  game's font sheet, which the host swaps for `tome-*` (fonts.cpp:1-20). The title's own words are hand-painted pictures
  in a pale teal script, so the element will look like the Volume page's hint, not like the painted rows. The user
  chooses (Open questions).
- Free it on every way off state 14: B (`0x8001B0C8` path), A on rows that change state (`0x8001A9FC` table
  `0x8023DB64`: rows 4, 5, 7 change state), and the state-load reset.

### 3.3 (a) What the title already does with the in-game menu

- `frontend_frame` runs at every shown frame in any game mode, title included (hle.c:5507-5511). Esc or Back already
  opens the in-game root over the title [read; not seen in a run].
- So a title "Settings" entry needs no new pause machinery. It needs:
  1. A new atomic request, built like `g_open_characters` (frontend.cpp:4256-4257), carrying the screen to open
     (`Screen::Options`).
  2. **A way for Back on that screen to close the menu.** Options' Back goes to `g_options_return` (frontend.cpp:2271-2277),
     and Back on InGame is the only Back that closes (`go_back`, :2846-2848). Today nothing means "close on Back". A
     `g_return_closes` flag, or a screen value meaning "close", is needed. The same flag serves the Tome (Back from the
     roster straight to the Tome, the user's ask).
  3. Rows to hide on the title: Disc is already hidden in game (`g_options_return == Screen::Launcher` only,
     frontend.cpp:2265). Skip intro is launcher only (01 §2.4).
- Two hooks from 02 Way B suffice on the game side: a notify on `0x8001B588` (Up/Down) that writes `TM+40 = 9` while
  `TM+62 == 14`, and a notify on `0x80019F30` (A) that sends the request when `TM+28 == 8` and `0x800AF00C()` is 0.
  The channel handlers are reached by pointer through `0x8013E160`, so their entries dispatch (C6) [read].
- Up/Down on the title calls `0x8013E154` **first**, then switches (`0x8001B5A8`). A notify still runs before all of it
  [read].
- Down gives +1 on the title: the main menu is Start Game (row 0, y 243) above Load Game (row 1, y 279), the handler adds
  `dir` to the cursor (`0x8001B790`), and tools/ed/jtg.sh:17 records that 6 Downs from Start Game land on Load Game (5
  rows). The title's vertical row has the same multiplier, -1, as the Tome's Options row (DOL: `0x3FFFFFFF` vs
  `0x1FFFFFFF`, same low half). So **Down gives +1 on the Tome's Options too**, settling 03's Open question 1 as far as
  reading can [inferred, strongly].
- Way A's hidden-glow hook (02 A3) has a language trap 02 didn't name: `0x80018824(5)` returns quad 5 in English but
  42+5, 63+5, 85+5, 107+5 in the other languages (02 §3.2), which may be another row's word. Way B has no glow to hide.
  Another reason to start with Way B.

### 3.4 (b) Player 1's Tome pages: what the host has and lacks

| Need | Today | Gap |
|---|---|---|
| Open a chosen launcher page from a game hook | Only `frontend_open_characters()` (frontend.cpp:4256-4257), fixed to `Screen::Characters`, player 0 | A general request: screen, row, player, "Back closes" |
| Back returns to the Tome | No (C3, claim 18) | The "Back closes" flag of §3.3 |
| Look like a Tome page | RmlUi pages with parchment, `body.pause`, `body.overlay`, `body.roster` (menu.rcss:27, :336-:518, :726) and art in `res/ui/tome/` | No `body.tome` skin. RmlUi uses TTF fonts only (`Rml::LoadFontFace`, frontend.cpp:2941); the `tome-*.png` sheets are atlases and can't be loaded as RmlUi fonts. The same faces are the TTFs: IM Fell English (loaded) for sheet 0, La Belle Aurore (`res/fonts/LaBelleAurore.ttf`, **not loaded** in RmlUi) for sheet 1 |
| Place a panel over the Tome's page | RmlUi covers the game picture (frontend.cpp:3285-3287) | The Tome's page is x 144..575 of 640 in **both** layouts (claim 11). As parts of the picture: 0.225..0.898 in the 4:3 layout; in the wide layout the 640 units are inside -106..745, so about 0.293..0.799 of a 16:9 picture [inferred from the quad tables]. In co-op the Tome is in a half; RmlUi has one context for the whole picture, so a half is only CSS layout |
| Anchor ImGui to game units | `draw_tome_button` uses fractions of the **window** (frontend.cpp:3349-3369) | No host function gives the game picture's rectangle in window pixels. RmlUi's own uses `webgpu::calculate_present_viewport` (.deps/GXRuntime/graphics/aurora/lib/rmlui/SystemInterface_Aurora.cpp:68). Inside 4:3 pillar bars the current button's fractions are off [inferred] |
| The cursor stop | Nothing hooks `0x8010172C`, `0x8010179C`, `0x80101838`, `0x80101980`, `0x801016D8` (03 §8, grep) | 03 §9's four or five hooks |
| Label in the Tome font | ImGui IM Fell English TTF (frontend.cpp:3161-3164) | A text element from entry (1,2), §2 verdict |
| Label lifetime | - | The element lives in guest RAM. Free it in the exit notify (03 hook 5); the Tome's close also calls the exit through `r13-24308` (`0x80110E10`, `0x80111750`) [read]. **A save-state load restores guest RAM**, so the host must forget its element pointer then, as `tw_state_loaded` (hle.c:13922) resets the Tome's host state |

**Player 1's own copy Tome has no Options page.** Player 1 gets a copy Tome when they press Start during player 2's
close-up or door (`g_tome_copy_p1 = true`, hle.c:31424, :31913). In the copy's turn, `tome_options_page` calls
`tome_copy_tab`, which sends every Options and Journal entry on to Inventory/Map/Magick **without checking whose copy it
is** (hle.c:16876-16889). So in that case player 1 has no Options, no Load/Save and no new entry. Neither 01 nor 03 says
so; 03 names only player 2. Whether that is wanted is for the user [read; effect not seen in a run].

**Which turn sees the label.** In co-op, world frames run between player 1's Tome frames (`g_tw.sharing`, hle.c:13640
comment). The world frame also draws text elements (`0x80008xxx` path, not traced). The two corner hints already live
through those frames, so a host label would behave like them [inferred]. Check in a run that it doesn't show in player 2's
half.

### 3.5 (c) Player 2's pages in their copy Tome

| Need | Today | Gap |
|---|---|---|
| An Options page in their copy | `tome_copy_tab` skips Options and Journal for any copy (hle.c:16876-16889) | Let Options through for player 2 but keep Load/Save (index 3, `0x801140D4`) closed: only player 1 saves (docs/coop-todo.md:236-241, decided 2026-11-15). Index 3 is reached by Down and by A; flag 823 also opens Load/Save straight (`0x80111404`), which the copy must not do either |
| Their Controller page | `tome.cpp` repaints image 31 with the bindings of `controls::bindings_signature()` = player 1's glyph set and layout (controls.cpp:1057-1062; tome.cpp:574) | A texture is one picture for both halves. Player 2's Controller page would show player 1's controls. Per-player content has to be drawn by game calls in the copy's turn or by an overlay, not by texture replacement |
| Their input in the copy | `tome_copy_pad` feeds the copy player 2's pad (`controls_read_player2`, hle.c:4226-4240) | Fine for the cursor stop (game hooks see the copy's pad) |
| Their input in a host page | `read_buttons` uses `gamepad()` = the first connected controller and the keyboard (frontend.cpp:89-99, :103) | `controls::gamepad(int player)` exists (controls.cpp:177) but `controls.h` doesn't export it (only `player_has_controller`, `player_has_keyboard`, controls.h:44-50). Player 2 on the keyboard also needs their own keys, not the menu's arrows/WASD |
| Opening the page | `frontend_frame` is not called during a copy turn (hle.c:5508-5511) | A request set in the copy's turn is taken at the next real frame. The flag is atomic, so this works [inferred] |
| Drawing only in their half | `hle_coop_ui_over(true)` turns the split off for any menu over a Tome (hle.c:13897-13903) | A player 2 page needs the split kept (player 1's half still showing) and a half-sized layout. **Any menu pauses both players** (claim 17). Whether player 2's page may pause player 1 is for the user |
| What they may change | 01 §4: `mod_p2_character`, `mod_p2_costume`, `mod_p2_stats`, `mod_p2_camera`, `mod_p2_input`, `pad_slot[1]`, `pad_type[1]`, `mod_p2_layout`, `mod_p2_custom` | Their hold/toggle and deadzone are shared by layout (controls.cpp:1349) and edited only from player 1's page |
| Copy Tome hooks | `tome_copy_hook_off` lists hooks that stand aside in the copy's turn (hle.c:32309-32335) | 03's new hooks must **not** be listed there if player 2 is to have them; they must gate on whose copy instead (`g_tome_copy_turn`, `g_tome_copy_p1`). Guest calls in the copy's turn act on the copy's memory, including its own element pool [inferred] |

### 3.6 Hook install, in one place

- Install all new hooks in a `find_*` routine called before `publish_codegen_options` (hle.c:34306-34317), or call
  `publish_codegen_options` again after installing (C6).
- Check the first words before installing, as hle.c does elsewhere. Verified from the disassembly: `0x80101980` is
  `stwu r1,-48(r1)` = `0x9421FFD0`; `0x8010172C`, `0x8010179C`, `0x8010180C`, `0x80101838`, `0x801018C0` are
  `stwu r1,-16(r1)` = `0x9421FFF0`; `0x8001B588` is `stwu r1,-32(r1)` = `0x9421FFE0` [read; the hex encodings inferred].
- An intercept and a notify can share an entry; the notify runs after a passed-through intercept
  (hle.c:5741-5773: the intercept runs, and only on pass-through does the notify run) [read].

---

## 4. What is still missing (for the build or a run)

### For (a), the title

1. The label's look: a game text element (§3.2, the game's font) or a repaint of texture 2 (02 B4, the painted script).
   The user decides.
2. The idle clock (02 §8): it only runs on the main menu (state 7), so Way B is safe; Way A must clear `TM+48` bit 0x2 on
   closing.
3. Whether title music keeps playing while the menu holds the game thread. Not read.
4. Other languages: Way B's row count and A test are language-free; the label position under "Language Select" changes
   per language picture (`UMnMenuF/G/S/I.cmp`). Only English was measured.

### For (b), player 1's Tome

1. The general open request and "Back closes" (§3.3).
2. A `body.tome` style: parchment page inside x 0.225..0.898 (4:3) or about 0.293..0.799 (16:9) of the picture, the
   co-op half, La Belle Aurore loaded into RmlUi if the handwritten hand is wanted.
3. The element's size and exact place under Load/Save (03's proposed `x 282, y 124, w 141, h 32` is not checked).
4. Player 1's copy Tome skips Options (§3.4).
5. Which launcher pages belong in the Tome (01 §6, docs/coop-todo.md:245-248: Characters, Graphics, Co-op, Mods and
   cheats, Save states). The user decides.

### For (c), player 2

1. `tome_copy_tab` change with Load/Save closed (§3.5).
2. Player 2's pad and keys in `read_buttons`, through an export of `controls::gamepad(1)`.
3. A half-only menu that keeps the split, and the pausing rule.
4. Their Controller page shows player 1's controls (one texture).

---

## Open questions

1. **Sign of Up/Down on the Tome.** Reading says Down = +1 (§3.3). One log line in hook 1 settles it.
2. **Do the hooks fire?** `0x8010172C`, `0x80101838`, `0x80101980`, `0x801016D8`, `0x8001B588`, `0x80019F30` are all
   reached by pointer or `bl`, so they should (C6). A first run with a `[trace]` line on each confirms it.
3. **Which sheet the label uses.** The picture says the hints' words are in the plain sheet (IM Fell English, gold). A
   picture of the new label confirms it, and shows whether its size matches the painted labels.
4. **The label in co-op world frames.** Does an element made in player 1's Tome show in player 2's half during the shared
   world frames? Check the existing corner hints in a run first.
5. **Character choice with the Tome open in co-op.** `characters_reload` waits for asked mode 2 (`r13-26188`) and 60
   frames (hle.c:32400-32470), and runs from `coop_pad_read` when `playing` (hle.c:32490-32525). Whether `playing` and
   mode 2 hold in the world frames behind player 1's open Tome, so the reload starts with the Tome still up, was not
   settled. The built Y button has been played, so watch for it in its logs.
6. **`0x8010FF0C(0,0,0,0)`** runs between the page frame and the element draw (`0x801115CC`). What it draws is unknown.
7. **A second hash for image 27** (as image 31 has two in tome.cpp:39), for 04's repaint route.
8. **Player 1's copy Tome without Options** (§3.4): intended or not? The user decides.
9. **May player 2's page pause player 1?** Any menu stops the game thread for both. The user decides.
10. **Label on the title:** game-font element or painted script? The user decides.
11. **The title's widescreen row** is undone by the host each frame (claim 16). Should it set the launcher's Aspect
    ratio, as the Tome's toggle does? The user decides (02 Open question 7).
12. **Sound during the menu** (01 Open question 2) is still not read.
