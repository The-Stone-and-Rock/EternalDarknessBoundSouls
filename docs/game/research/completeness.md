# Completeness check: what the kit still lacks

**Written before the kit took this in, and kept for its checks.** The research is now in the kit, and contradictions 1 to 5 are corrected in the reports.

- At the time, the kit read none of it.
- `.gitignore` line 12 is `build*/`, so all of it is untracked, and so are the scripts that remake it.
- `tools/ed/primitives.tsv` has 53 rows against the 413 researched. It still says `fn25 used_thing`, `fn34 stop_self`, `fn71 music`, `fn93 face_a`, `fn104 sound_at`, `ed52 run_script_on` and `ed221 ... Those two aren't traced`.

Marks are as in the other reports: **proved**, **read**, **inferred**. Nothing here was run. `python -I tools/ed/facts/completeness_checks.py` reprints the counts (output in `completeness_checks.out`). It takes about 15 seconds.

The four new tables are mine. They have had no skeptic review.

## What was asked for, and where it stands

| Asked for | Researched? | Usable as data? | Still thin |
|---|---|---|---|
| Item table id with name | Yes: `InvU.bin`, 244 rows, proved | Yes: `item_table.json` `rows[id]` | **Names.** See below |
| Use, Mix, Check, Equip, Mode | Yes, down to addresses | Partly | "Which function to call" exists only in `use-mix.md`. `mix_table.json` holds the recipes, not the functions |
| Examine | Yes | Only as counts in `verbs.json` | Had no per-call table. Now `examines_table.json` |
| Pick up | Yes, but no single path: 3 askers of prompt 19, 5 sites calling the find | No | Only prose in `verbs.md`. Not every `ed52` call was read as a pickup |
| What each transition number is | Yes: 17 numbers, fade and motion proved | Yes: `transitions.json` `transitions` | **What it looks like, and how long.** See below |
| What each door uses | **Was not there** | Now `doors_table.json` | `ed door ROOM` lists ways in, with no from-room, no prompt, and no meaning for the number |

### Item names

- **10 rows hold an unexpanded `~` code:** types 1, 2, 3, 4, 49, 57, 69, 70, 74, 216, such as `~x` and `The Essence of ~x`. What expands them was not traced.
- **12 rows aren't names at all:** the 11 placeholders and type 45, whose name is a full sentence.
- **40 names are shared by 110 rows.** Torch is 105, 222, 235 and 240. Binding Hall Key is 11 rows. A lookup by name must return all of them.
- **53 rows have no known source,** 42 of them not placeholders. Types 85 (Blowgun Darts), 95, 233 and 243 still have no ammo class tied.

### Transitions

The fade and motion numbers are solid. These are not settled:

- **On screen:** "door" and "climb" are usage only. Nobody watched motions 2, 25, 103 or 104.
- **Time:** seconds per tick was not measured.
- **Arrival:** whether the door motion is still playing at the room change, which decides what arrival does. `0x800271B4` is registered from at least 10 sites (`reconcile-usemix/refs.py 800271B4`), and I did not follow who calls it either.

## What a tool can't read yet

| Fact | Lives only in |
|---|---|
| Record field to getter and setter (`+36`, `+38`, `+40`, `+42`, ...) | `item-ids.md` table. The JSON has values, not offsets or addresses |
| "To do X, call Y" for items | `use-mix.md` |
| Tome page states, slots, the `0x80335748` record | `use-mix.md` |
| Prompt slots, close-up steps, region script fields | `verbs.md` |
| `press_runs`, and what a prompt does | Prose inside `verbs.json` |
| Native arguments | Free text in column 5 of `natives_NN.tsv`. 385 of 413 rows contain `name(args)` in the prose, none as a field |
| `ed140` conditions | `item_table.json` holds only the error labels, not the conditions |
| Events | `args/events.txt`, which is pattern-matched. "A place to look" by its own report |
| `fn78` kinds, `fn79`, `fn8`, `ed221` arguments | `arguments.md` |
| Names for chr records, models and node kinds | Not found anywhere. `kinds.json` is kind to handler address only |

## The claim that looked least reliable: refuted

`use-mix.md` line 369: "The prompt's run passes 1 (`0x800C2D28`), and the run at the Tome's end passes 0. So the prompt gives 177, and the Tome gives 208." It disagrees with `verbs.md`, so I read the code myself.

**It is wrong: Z gives 177, and the RELOAD prompt gives 208, the same as the Tome.** [read]

| Step | Evidence |
|---|---|
| `0x800C2D28` is the `li r4, 1` inside `0x800C2CA4` | `ed dis 0x800C2CA4 40` |
| `0x800C2CA4` has two callers, both in event 207 | `grep -E "bl +0x800C2CA4" build/ed-db/disasm.txt`: `0x800597F8`, `0x8005980C`. `0x800597B4` compares event 207 |
| Event 207 is Z pressed | `verbs.md` line 254, table row `0x8023AF80`. The controller's `li r3, 207` is at `0x80045EB4` and is the only such literal in the disassembly |
| A prompt press passes 0 | `0x8000703C`..`0x80007048`: `li r4, 0`, `li r5, 0`, `bl 0x801642D4` |
| Script 2013 hands that value on | `L00/2013`: `L0 = fn85(pTrigger)`, then `ed81(L2, L0)` |
| `ed81` picks the event | `0x80012DE0`: 1 sends 177, anything else sends 208 |
| 208 sets the reload byte, 177 clears it | `0x80059838` stores 1 and `0x8005986C` stores 0 at `r13-25592` |
| What the byte does | `0x80053528`..`0x80053538`: go round if block flag `0x2000` is set or the byte is set |
| Flag `0x2000` is Z held | Table row `0x8023AFA0`, Z held, calls `0x8019A1A0`, which ors in `0x2000`. `0x800460E4` returns the block `r13-25660` and `0x8019B038` reads its `+8` |

So on 2013's 12 kinds (shotguns, revolvers, elephant gun and their ammo), Z loads one at a time for as long as Z stays held, and the prompt or Tome loads until full. That also answers two entries listed as open: `item-ids.md` "What triggers `0x800C2CA4`" and `use-mix.md` "Bit `0x2000`".

**Also spot-checked, and held up:**

- **`mix_table.json` pieces:** I read `L03/1498` and `L04/1359` by hand. All 7 and 2 pairs match, and so does the refused 35+35.
- **`r13-24244` is 3 with the Tome up:** `hang-20261101-031036.state` reads 3, with room 5 and record word 0 of 10.

## Files that contradict each other

| # | Says | But | Verdict |
|---|---|---|---|
| 1 | `use-mix.md` 369: the prompt gives 177 | `verbs.md` Reload | `use-mix.md` is wrong, above |
| 2 | `use-mix.md` 186: "612 slots in `item-ids.md`" | `item-ids.md` lists 612 as failed: 562 of 588 at `+264` link | Stale citation. Slots can go stale |
| 3 | `item-ids.md` `ed140` result 2: "not one of group 3 with one of group 4" | `use-mix.md`, `natives_07`: asymmetric | `use-mix.md` is right. `0x80016A94` tests only for group 3 on that side |
| 4 | `natives_05` `ed82`: "no save with the Tome open was read" | `use-mix.md`: live 3 in the hang state | `use-mix.md` is right. I read 3 |
| 5 | `item-ids.md` "not established": flag 602, the `0x800C2CA4` trigger, which Ancient goes with which set | Answered in `use-mix.md`, `verbs.md`, `arguments.md` | Stale. The set is alignment: 1 Chattur'gha, 2 Ulyaoth, 3 Xel'lotath [read] |
| 6 | Marks differ: `+36` proved vs read, `+42` read vs proved, `fn71` proved vs lowered to read, Equip proved vs read within `use-mix.md` | | Use the lower mark until reconciled |

**Not contradictions:**

- **`item_table.json` against `mix_table.json`:** 0 differences over 244 rows on 8 fields, and the same templates on all 59 weapons and ammo.
- **`0x80041AFC` against `0x80041B2C`:** the first is the Npcs table 3 loader's entry, the second its loop body.
- **"118 of 124" `ed221` calls name a placed tag:** that counts any level's file. Per level it is 93. The other 25 name a tag placed only in another level, 5 are spawn tags, and 1 (tag 2042) is not found.

### The same word means different things

This will break a tool that joins the files.

| Word | Meanings |
|---|---|
| **kind** | `verbs.md`: node `+8`. `use-mix.md`: InvU type id. `item-ids.md`: InvS record. `item_table.json` row `kind`: a text group label |
| **object** | `item-ids.md` "info" and `use-mix.md` "object": node `+44`. `arguments.md`, `transitions.md`: node `+52`, which `verbs.md` calls the model |
| **controlled** | `use-mix.md` and the `hle.c` comment at line 8049: `r13-21488`, `-21484`. `arguments.md` and `natives_08` `fn78`: `r13-21496`. See check 2 |
| **table** | `item_table.json` `placed[].table` 3 is `placed_things.json` `section` 2, off by one |
| **template / inv_kind, def / chr** | The same things, named differently by file |
| **field names** | `after_examine_script` is `check_script`, `after_find_script` is `pickup_script`, `capacity` is `count_max`, `fire_mode` is `mode` |

`item-ids.md` says to use Check for the Tome action, yet its JSON and `natives_12` still say Examine. Examine is prompt 6 in the world, and Check is the Tome's action.

## Three further checks

### 1. A table of every door: `doors_table.json`

One row for each of the 1083 `fn66` calls: from-room and region, prompts in the script, target room, entry, camera, transition, motion, fade, after-script, flags.

**Where each call starts from** (text search for "named by", so it is a place to look):

| Transition | Region | Item record | Named by a script | None found |
|---|---|---|---|---|
| 0 | 6 | 118 | 8 | 11 |
| 1 | 2 | | | |
| 2 | 326 | 76 | 4 | 16 |
| 3 | 72 | | 2 | 12 |
| 6 | 2 | | | |
| **9** | **333** | | | |
| **12** | **46** | | | |
| **15** | **49** | | | |

- **The door rule holds, by a stronger test.** `transitions.md` used the last prompt earlier in the text. By the whole script: all 333 transition 9 calls ask OPEN DOOR and all run from an enter region. All 46 transition 12 calls ask CLIMB UP. Of 49 transition 15 calls, 45 ask CLIMB DOWN and 4 use EXAMINE. [proved as counts]
- **New: many "no prompt" room changes are item scripts.** 194 calls, all transition 0 or 2, sit in an item record's script: 162 after a find and 32 on Use, for example the Tome's after-find 1599. A door hook misses none of these, as they all go through `fn66`, but they aren't doors.
- **The entry and camera arguments are confirmed.** Of 1056 calls with a literal room, the camera is in the target room for 1054 and the entry marker for 1030. The misses:
  - **Room 408, 2 calls:** `L09/61` and `L16/61`. Room 408 has no file on the disc.
  - **Room 64, 24 calls:** `EntryMkr02`, hash 212163735, all in script 1599's copies. Room 64's own table has no such marker (`EntryMkr01` is 212081813). [proved]
- **What a missing marker does [read]:** `0x80155348` returns -1, `0x80151A64` returns 0 (`0x80151A84`), and `0x801526B0` jumps to `0x80152738`, past placing the player and event 238. Whether those two calls run in play was not established. Their after-script 1828 exists only in L07 and L13.

**Limits:**

- 139 region rows name a region in a room outside the level's known rooms, so their from-rooms may belong to another level. `from_in_level_rooms` marks them.
- 27 calls have a variable room, such as `VampReturnRoom` or `L1`.
- 39 rows have no source found.
- At the time, `ed door` kept only 1050 of the 1083. It now reads this table.

### 2. Which "player" each function reads: `who_table.json`

There are two pairs, not one. [read]

| Pair | Getter | Written by |
|---|---|---|
| `r13-21496` id, `-21492` node: **current** | `0x801F62B8`, `0x801F62B0` | `0x801F626C`: at spawn (`0x80007868`), and 5 sites that also write `r13-26524` or a state: `0x80059B18`, `0x800A2400`, `0x800A27C0`, `0x800C4904`, `0x800C4C68` |
| `r13-21488` id, `-21484` node: **main** | `0x801F6258`, `0x801F6250` | `0x801F6260`: **one caller**, `0x80007860`, at spawn |

"Main" comes from the game's own name: `EDDestroyMainCharacterDamageField` calls `0x801F6258`. "Current" is my label. Besides these, `0x801F5CEC`..`0x801F5CF8` store r5 into all four words.

All 11 states I checked (`saves/slot1`-`slot10`, `build/runs/s1`) hold the same value in both pairs, so **no save can tell them apart**. The split below comes from the code alone.

| Reads | Functions, direct reads only |
|---|---|
| **current** | Tome list builder, action test, Equip, Mode. `ed140`, `ed131`, the load-all reload `0x8007A1DC`. Close-up stepper, the find `0x80024528`, prompt press. `fn66`, door motion, arrival, `0x80151FDC`. `fn78`, `ed301` |
| **main** | **`fn93`** (takes the item), **`ed221`** and `0x800174A0` (the carried test), **`ed52`** |
| Through one call | `ed158` to `0x801C4250` (main). Prompt press to `0x801C4F48` (main) |

**This matters for co-op.** In the game's own code the Tome lists one pair's items, and `fn93` removes from the other's list. The find gives to current, and `ed221` tests main's list for "already carried". Moving only one pair breaks Use and Mix without an error.

`0x801F5F88` (handle to node) also has a shortcut: if the id equals `r13-21496`, it returns `r13-21492`. So a mismatched current pair makes lookups wrong too. `hle.c` line 7942 lists all five words, including `r13-26524`. I did not read what it does with them.

Limits: the walk follows direct branches only. `fn162`, `ed301` and `fn66` have jump tables, so their cases may be undercounted.

### 3. Item scripts by level, and every Examine: `item_script_levels.json`, `examines_table.json`

**Scripts by level.** A script missing from the loaded level does nothing, silently (`use-mix.md`, Level gap). `item_table.json` didn't say which levels hold each item's script.

- Of 233 script fields on 186 rows, only one never exists in a level the item appears in: type 240 Torch, after-find 1324 (L03, 04, 07, 08, 10), spawned only by script 1205 in L06. This supports the id-to-script mapping as an independent check.
- 28 rows lack a script in some of the levels they appear in. The list is too wide, as it counts every level holding a copy of the spawner, so use it as a warning, not a bug list.

**Examine.** 624 calls: `ed219` 402, `ed220` 98, `ed221` 124. Each row has its words, from-region, and for `ed221` the item it offers.

- **Room text is confirmed.** All 509 calls with a literal entry of 0 or more find words in that level's room text. 0 misses. The other 115 are 83 with entry -1 and 32 with a variable. [proved]
- **`ed221` to item.** 93 of 124 resolve through their own level's Npcs file to exactly one item type, and 90 of those have the record in the script's own room. For example `L03/1293` is Blue Urn, type 31, room 147, region CameraRgn03.
- **The three Codex calls:** `L03/1279`, `L04/346`, `L05/1525` (tags 2154, 2157, 2160) are Npcs section 4 records, whose type is per alignment. All three alignments give type 225, Codex.
- **The other 31** are the 25, 5 and 1 under "Not contradictions". Their rows have `placed_records` 0.

## What to do first

1. **Fix the reports:** contradictions 1 to 5, and one glossary for the words above.
2. **Merge `natives_NN.tsv` into `primitives.tsv`.** 413 rows, with `read` and `guess` marks kept, and give arguments their own field.
3. **Add a kit copy of `item_table.json`, `mix_table.json`, `transitions.json` and `doors_table.json`,** outside `build/`, with `ed item` and `ed door` reading them. Also add a name fix for the 22 bad rows.
4. **Add one verbs table:** verb, entry function, arguments, gates, and which player pair it reads, joining `use-mix.md`, `verbs.md` and `who_table.json`.
5. **Have one run answer what no static work can:** seconds per tick, whether the door motion is still playing at the room change, and how each transition looks.
