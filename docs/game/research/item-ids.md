> Kept as the evidence behind `docs/game/systems/`. Reports were checked by skeptics before the kit was built. Where a report says the kit is wrong, the kit has since been corrected. `tools/ed/facts` holds the scripts it cites, and `ed build facts` remakes the tables.

# Item ids: the game's table, and the numbers that get mixed up with it

Every statement carries one mark:

- **[proved]** checked against live memory in save states, or data and code agreeing.
- **[read]** from the code alone. No run was made for this report.
- **[inferred]** from usage.

Anything that failed or could not be settled is under [Not established](#not-established).

**The table is `InvU.bin`, 244 entries, and an item's type id is its entry number.** [proved] Regenerate with `python -I tools/ed/facts/item_table.py`, which writes `build/ed-db/facts/item_table.json` (`rows[id]`) and prints its checks.

```
python -I -c "import json;r=json.load(open('build/ed-db/facts/item_table.json',encoding='utf8'))['rows'][21];print(r['id'],r['label'],r['name'],r['use_script'],r['tags'])"
python -I tools/ed/facts/checks/reconcile-items/scan.py     # every state: lists, records, tags, slots. Writes scan.json
python -I tools/ed/facts/checks/reconcile-items/rows.py     # label forms, InvS modes and masks, key rows
python -I tools/ed/facts/checks/reconcile-items/tags.py     # where the tags scripts search for come from
python -I tools/ed/facts/checks/reconcile-items/slots.py    # Tome slots against the thing list
python -I tools/ed/facts/checks/reconcile-items/livematch.py  # live things against table 3 and 5, tag 481's object rooms
python -I tools/ed/facts/checks/reconcile-items/word24244.py  # the word ed82 tests, by state
```

What "all states" means below: 226 state files under the repo, 178 with a thing list, 107 of those distinct by memory (the rest are copies).

## Four numbers, not one

| Number | Where it lives | Stable? | Examples |
|---|---|---|---|
| **Type id** | `InvU.bin` entry, info `+12`, `0x80151184(info)`, script `fn92(handle)` | The same everywhere | Bronze Necklace 21, Blue Urn 31, Tome 30, 120 and 234, Tulwar 84, Chakrams 82, Talisman 150 |
| **Handle** | node `+16`, carried entry `+8`, Tome slot `+40`, `0x80335748` word 0. `0x801F5F88(handle)` gives the node | This play only | Blue Urn 62 in slot 1, 9 elsewhere. Tulwar 9, 89 or 90 |
| **Tag** | node `+36` then `+24`. `fn78(2, tag)`, `fn78(4, tag)`, `fn78(5, tag)` | Set by the placing, not unique | See below |
| **Chr record** | ed3 argument 1, Npcs record, `ai/ed_chrs.dat` | Fixed | 25 on 92 of the 181 table 3 records |

**Type id.** [proved] Pack count 244, magic `0x6B5`, every entry `{1, 0xFB90, 16, 88}` then 88 bytes, ending exactly at file size 25880. `0x801506C8` stores its `r4` at info `+12` and passes it to `0x8015062C`, which reads the pack at `r13-23412` by that index and copies 88 bytes. The pack in memory equals the disc in all 178 states with a thing list. A thing's live record equals `rows[info+12]` except for the bytes under [Live changes](#live-changes). `0x801044E8` to `0x801044F4` compares a type with 31 and then tests flag 266, the flag script 1429 (Blue Urn) sets.

- **A type can be -1.** [proved] 13 things in 12 states have info `+12` = -1, with record pointer 0 or `0x81FF0000` (handles 20, 71, 92, 126; kinds 0, 10, 51). `0x8015062C` returns 0 for a negative type. Check before indexing `rows[type]`.

**The ids in `docs/items-log.md` are handles, not type ids.** [proved] `rows[102]` is the Saif, `rows[9]` the Bronze Bracelet, `rows[11]` the Silver Bracelet. Live: `build/runs/s1.state` handle 9 type 84, 10 type 150, 11 type 82; `logs/saved-changes/working-01b3d85-before-tome-copy/saves/slot10.state` handle 102 type 21; `saves/slot1.state` handle 62 type 31 (that one is from `docs/coop-tome-pickups.md` line 11, not the items log). The Tome passes slot `+40` as the script's `r4` (`0x801043C8`, `0x80104210`), which is where "script 1429 with item 62" comes from.

**Handles change.** [proved] Over all states: Tulwar {9, 89, 90}, Talisman {10, 91, 92}, Chakrams {11, 90, 91}, Blue Urn {9, 62}, Tome 30 has nine. 63 of the 98 handle numbers seen sit on more than one type. The Bronze Necklace was only ever seen as 102.

**`0x80335748` word 0 is a handle.** [proved] 62 in `saves/slot1.state` and 83 in `saves/slot3.state`, the first carried thing in each.

**"678 and 733" in the log are tags.** [proved] `saves/slot10.state`: handle 88 type 79 Emerald tag 678, handle 89 type 30 Tome tag 733.

**Tags.** [proved]

- Read by `0x801F6160` and `0x801F60BC` (node `+36` then `+24`). Written by `0x801F6554`.
- 481 is live on both the Scramasax (type 103) and the Broken Blue Urn (type 32): handles 81 and 83 in slots 2, 3, 5 and 7. 733 is live on types 30 and 120, and is 0 on the Tome in the archived slot 10 (handle 100). 480 is live only on type 37 in these states, but placings and literal `ed3` calls give it to 22 types.
- The default is 0 (`0x80040E94` leaves `+36` clear). An Npcs `0xFFFF` stays 65535 (slot 1's Torch).

**Where tags come from.** [proved for the counts, read for the fields] `ed3` argument 7 (`0x8000E5F4`, stored by `0x800308E4`), and Npcs table 1 `+34` (`0x80041A18`, spawn `+36`), table 2 `+98` (`0x800418B4`), table 3 `+40` (`0x80041C3C`), table 5 `+56` (`0x80042094`). Table 4 `+46` is compared with a tag at `0x80041760`. Of the 475 distinct literals scripts pass to `fn78(2, ...)` or `fn78(4, ...)`, 332 appear only in tables 1, 2 or 4, so `rows[].tags`, which holds only tables 3 and 5 and literal `ed3`, does not cover them.

**Chr record.** [read] Its `+136` is the kind passed to `0x801F937C` (`lha` at `0x80031D80`, called at `0x80031D84`), with the type from spawn `+24`.

**How a thing gets them.** [read] `0x801506C8(kind, type)` builds a thing's info: `+0` the InvS record, `+4` a private copy of the InvU record, `+8` the kind, `+12` the type. The InvS record is the pack's own, not a copy (`0x801505F0`), so a write through it affects every thing of that kind.

## Path from a thing to its words

`handle` → `0x801F5F88` node → `0x801F6398` node `+44` info → `+4` record. [proved, walked in all states]

Every getter and setter below was disassembled and has the offset shown. [read]

| Record field | Getter | Setter | What |
|---|---|---|---|
| `+76` byte | `0x80150B5C` | | Text group in `UBootPak.bin` section 4, the pack at `r13-26492`. [proved] |
| `+77` byte | `0x80150B2C` | `0x80150AFC` | Pickup words (`0x800C3008`). [proved] |
| `+78` byte | `0x80150B38` | `0x80150B20` | Description (`0x8010383C`, `0x8010384C`). [proved] |
| `+79` byte | `0x80150B44` | `0x80150B14` | Inventory name (`0x8010482C`, `0x8010483C`). [proved] |
| `+80` byte | `0x80150B50` | | Entry in group 0 after an enchant or repair (`0x801C6FB8`). 14 ENCHANTED_ITEM on 47 rows, 15 REPAIRED_ITEM on 8. [proved] |
| `+36` s16 | `0x80150868` | `0x80150B08` | Script after a find. See below. [proved] |
| `+38` s16 | `0x80150850` | `0x80150AB0` | **Use script.** See below. [proved] |
| `+40` s16 | `0x8015085C` | `0x80150ABC` | Script after Check's words. See below. [read] |
| `+42` s16 | `0x80150874` | none found | **Mix and reload script.** See below. [read] |
| `+68`, `+70` u16 | `0x801508FC`, `0x801508B0` | `0x80150990` (count) | Count and capacity. `+74` (a byte) and `+72` instead when `+0` is 16 (`0x80150820`). [read] |
| `+0` u32 | `0x80150814` | `0x80150A2C` | Fire mode. See below. Mechanics [read], name [inferred] |
| `+4` u32 | `0x801507FC` | `0x80150AE8` or's, `0x80150AD4` clears | Flags. `0x20`: `+38` runs after Check (`0x80104170`), on 11 rows, all Chapter Pages. `0x200`: set by an enchant (`0x801C6F8C`). [read] |
| `+81`, `+82` bytes | `0x80150A20`, `0x80150880` | `0x80150AC8`, `0x80150AA4` | Written by a cast (`0x801C6F5C`, `0x801C6F68`, for type 131). Otherwise not traced. [read] |

**Text bytes.** [proved] `0x800C3008`, `0x8010383C` and `0x8010482C` pass them to `0x801DB374` with `r3 = r13-26492`, and memory matches section 4 in slots 1 and 3. Bronze Necklace is 5/32/30/31, the same as `ed item Bronze`.

- The byte numbers hold in every language. [proved] `0x800218EC` to `0x80021A00` load `UBootPak.bin` or `UBootPkF/G/I/S` by language, all with the same 749 keys and labels. The JSON's words are English only.
- 223 rows have `_PICKUP`, `_DESC` and `_INV` where the field says. The other 21: 8 rows whose `+78` is an `X_BEFORE` entry (60, 154, 160, 161, 165, 167, 176, 242); 11 placeholders (94, 98, 99, 124 to 126, 177 to 181); type 45, whose name byte points at `ARETAK_DESC`; and type 97, whose pickup byte points at `BLOWGUN_INV`. [proved]

**`+36`, after a find.** [proved] `0x800D9B64` runs it, from event 190 (`0x800D89E4`, `0x800DAE8C`) and for a linked thing through `0x800D97E0`. `docs/coop-tome-pickups.md` line 14 logs script 1362 with lr `0x800D9BA8`, and only type 57, the rune in that trace, has `+36` = 1362.

- **Not every find goes through it.** [proved] The 3-Point Circle (type 43, Npcs3 room 147, tag 632) has all four script fields 0. The log's "2129 (the find)" is a world script: `rooms.json` has it on room 147's desk regions, and `L03/2129.lua` opens with `fn78(2, 632)`.

**`+38`, Use.** [proved] `0x80102D10` gates Tome action 0 on it; `0x80104330` loads it and `0x801043D0` runs it with the handle. Bronze Necklace 1510, Blue Urn 1429 and Talisman 1874 match the logged Use scripts. Literal `fn162(x, 0, value)` calls only write 0, 864, 1871 and 1879. Types 70 and 74 were seen live going from 0 to 864.

**`+40`, after Check.** [read] Action 2 sets state 3, `0x8010388C` sets state 4 after building the description, and `0x80104114` reads `+40`, run at `0x80104214` once the words are done. The 8 rows with it are readable items (envelopes, journals, parcel, message tube, spice jar, pulp novel). [proved for the rows]

**`+68`, `+70`.** [read] Used as count and capacity by ed140 (`0x80016C24` to `0x80016C48`) and `0x800C2D18`. Data: Chakrams 19/19, Assault Rifle 30/30, and 5/5 in mode 16. Talisman is 5/0 and Strange Necklace 10/0, so those have a count and no capacity. No state shows a weapon below capacity.

**Script access.** [read] `fn212(handle, which, ...)` reads and `fn162(handle, which, value)` writes (jump tables `0x802543FC`, `0x802543DC`). `which` is 0 `+38`, 1 `+40`, 2 `+77`, 3 `+79`, 4 `+78`, 5 `+36`, 6 `+81`, 7 `+82`.

**`0x80103240` isn't an item's name.** [read] It asks for group 0, entry `0x80335748 + 8` (`r23` is set to `0x80335748` at `0x80103214`): the Tome's prompt.

### Live changes

**The JSON holds starting values.** [proved] Over all states, 3,096 things (3,083 with a record):

- Text bytes differ only on type 71's `+77`: 18 on 95 things, 12 on 34, 9 on 20, instead of 15. These are spell pickup entries in group 6. `ed168` (`0x8001651C`) writes them; its table `0x8023C454` can write 12 values.
- Other bytes differ too: `+7`, `+28` to `+31`, `+38`/`+39`, `+69`, `+81`, `+82`.
- This is a measurement, not a rule. Scripts also write text bytes no state happened to catch: `fn162(L0, 4, 33)` in 2522, `fn162(L0, 4, 36)` in 2523, `fn162(L1, 3, 32)` in L05 script 483. Native code changes records too: `0x8006C514` clears `+36`, and `0x800B8E0C` to `0x800B8E54` write `+38`, `+40`, `+36`, `+78`, `+79`, `+77` and `+0` back from a stack block (purpose not traced). [read]

## Tome actions

| Action | Label in boot group 14 | What the code does [read] |
|---|---|---|
| 0 | USE | Runs `+38` with the handle. Offered when `+38` is non-zero (`0x80102D10`) |
| 1 | EQUIP | Calls `0x800C9FD4` (`0x80104450`) |
| 2 | CHECK | Always offered. Shows `+78`, then runs `+40` |
| 3 | MIX | Offered when flag 602 is set and `+42` is non-zero (`0x80102D64` to `0x80102D78`). Runs `+42` of the first item chosen |
| 4 | MODE | Cycles `+0` through InvS `+20` (`0x801045EC` to `0x8010463C`) |

The labels are on disc [proved], group `BOOK_TUTORIAL`, entries 0 to 4, and in the JSON as `tome_actions`. **The game's word for Examine is Check.** Which entry goes with which action number is [inferred] from their order and from what each entry's words describe. Nothing was traced loading those entries for the menu.

## Fire mode: record `+0`

**The name.** [inferred] Boot 14,4 says Mode changes "the fire selector on guns", and boot 0,35 `MIX_ERR_NOT_RIGHT_MODE` says the OICW must be in the proper mode to reload that ammo. The JSON field is `fire_mode`.

**The data.** [proved] InvS `+20`, the bits it cycles through (JSON `inv_kinds[].fire_modes`), is non-zero on 4 of 55 kinds, all guns: 16 Pistol `0x1`, 21 Elephant Gun `0x81`, 24 Assault Rifle `0x1D`, 42 Double Shotgun `0x60`. Starting `+0` is 0 on 190 rows, 1 on 52, 16 only on OICW Grenades, 32 only on the Double Shotgun.

**The code.** [read]

- `0x80150820` picks the second count pair when `+0` is 16.
- `0x80150A2C` writes the InvS mask through `0x801511B8`: `0x400` in mode 16, and `0x100` otherwise if the mask has a `0x500` bit. The write lands in the shared InvS record (see above).
- `0x80036E74` to `0x80036E84` reads `+0` and compares it with `0x40`. What that branch does was not traced.

## Mix and reload: record `+42`

**In the Tome.** [proved] `r13-24536` is written only at `0x80104340` and `0x8010436C` (action menu, state 1) and read only at `0x80104294` (second pick, run at `0x801042A0`). 44 rows use 9 scripts. There is no partner field. Exhaustive over those 9:

- **The script compares types:** 1198, 1359, 1383, 1498, 1606, 1882 test `fn92` of `ed20(0)` and `ed20(1)`. JSON: `mix_script_tests_types`.
- **`ed140(a, b)`:** 1582, 2013, 2221.

**Outside the Tome, `+42` also runs.** [read] This is not only Mix.

- `0x800C2CA4` (from `0x800597F8`, `0x8005980C`) runs the held weapon's `+42` with `r4 = 1` at `0x800C2D30`, when `0x801F9B18` finds another carried thing sharing a mask bit and count differs from capacity.
- `0x800598CC` reads it the same way.
- `0x800C067C` to `0x800C0690` returns it and falls back to **1582 when it is 0**, so `+42 = 0` does not always mean no reload.
- Scripts 1582, 2013 and 2221 branch on `ed82()`, which is `r13-24244 == 3` (`0x80012ED0`): `ed140` on one side, `ed80`, `ed81` or `ed129` on the other.

**One-sided Mix.** [read] Action 3 needs the first item's `+42`, and pairs can have it on one row only. Type 122 Piece of Broken Key has 1359, which tests 121 and 122, and type 121 has `+42 = 0`. Not tested in a run.

**`ed140` results.** [proved] Code at `0x800168AC`, doubles at `r2-32520` and neighbours, and `ed130` (`0x80012F58`) turns each failure into a group 0 entry whose label names the condition. In the JSON as `ed140_results`.

| Result | When | `ed130` shows |
|---|---|---|
| 1 | Both exist, weapon equipped, masks share a bit, room left | Script goes on to `ed9` |
| 2 | `a` in group 3 without `b` in group 4, or `a` not in group 3 without `b` in group 3 (`a`'s group isn't checked on that side), or the weapon's mask is 0 | 0,31 `MIX_ERR_NO_WEAPON_OR_AMMO` |
| 3 | Weapon not in the player's equipped list (`0x80151664`) | 0,32 `MIX_ERR_WEAPON_NOT_EQUIPPED` |
| 4 | Masks share no bit | 0,33 `MIX_ERR_AMMO_NO_MATCH` |
| 5 | Capacity minus count is not above 0 (`0x80016C44`) | 0,34 `MIX_ERR_WEAPON_FULLY_LOADED` |
| 6 | No shared bit, and both masks are `0x100` or `0x400` | 0,35 `MIX_ERR_NOT_RIGHT_MODE` |
| -1 | A handle has no node | Nothing |

**`reload_partners` in the JSON.** Possible pairs from the masks only, not a promise a reload works. [proved for the masks, read for the mode rule] Mode 16 is now counted: Assault Rifle (137) lists 135 and 136, and OICW Grenades (136) lists 137. It is `null` for rows outside groups 3 and 4 and for 9 weapon or ammo rows with no known kind (65, 85, 92, 95, 106, 109, 222, 233, 243). `[]` means masks are known and nothing matches.

## What a carried list holds

[proved] Node `+48` is `{+0 zero, +4 first entry, +8 count}`. Each entry is `{+0 previous, +4 next, +8 handle}`. Walked in all states: 617 lists, 1,012 entries, every count and every previous link right, every handle resolves to a node. A first entry's `+0` points at the header, and only the header has `+0 == 0` (`0x801F60E0`). 439 of the 617 lists are on things other than handle 1, including enemies. [read] `0x801F95A0` moves an entry out of its old list first, so a thing is in one list at a time. The type is two steps from the handle: `0x801F5F88`, node `+44`, `+12`.

## Tome slots

[proved] At `0x80334388`, 44-byte slots, slot `+36` info, `+40` handle. Block `+264` is the first; `0x80103260` to `0x8010327C` uses the row at `+792` instead when its `+36` is non-zero.

**Slots go stale.** [proved] Across all states, block `+264` has 588 filled slots and 562 link to a node whose `+44` is that info. The other 26 don't, either because the handle has no node or because the node's info moved (`saves/slot2.state` handle 62, `saves/slot5.state` handles 105 and 66, `saves/slot9.state`). Check the node before trusting a slot. Filled slots were also seen at `+0` (47) and `+1056` (26); those blocks were not traced.

## fn78(kind, value)

From `0x80165C94`. [read]

| Kind | Calls | Finds |
|---|---|---|
| 1 | `0x801F62B8` | The current player's handle |
| 2 | `0x801F6160(tag, r13-23276)` | First thing with that tag whose object says it is in the current room |
| 3 | Walks the list comparing node `+8` | Its handle. **No break, so the last match** (`0x80165DD4` to `0x80165DF8`) |
| 4 | `0x801F6160(tag, -1)` | First thing with that tag, any room |
| 5 | `0x801F60BC(tag, player's list)` | Carried thing with that tag |
| 6 | Argument is an object | Its handle (`0x801F62C8`) |

**`fn78(2, 480)` in script 1429 is a tag search, not an item id.** [proved] It gets what the script just made. Lines 37, 41, 44 and 84 are `ed3` with tag 480, and lines 52 and 91 are the `fn78`. Line 20 is `ed3` with tag 481 and line 27 is `fn78(2, 481)`. Room argument -1 keeps the current room (`0x8000E548`). The script is in `L03` and `L07`.

**Why the fresh thing wins, and the trap for co-op.** [proved]

- The list runs newest first, strictly descending handles in 178 of 178 states, and the first match is returned.
- Kind 2 is not strict about the room. It compares the object's `+592` (`0x801F61A0`), and carried things keep one: the Scramasax, tag 481, has object room 150, 21, 153, 152 in slots 1, 3, 7, 5. So in slot 3, room 21, `fn78(2, 481)` with nothing newer returns the carried Scramasax.
- A thing with no object skips the room test (`0x801F6198`). [read]

**ed3.** `ed3(chr, model, room, type, a5, a6, tag, a8, a9, position, a11)`. Arguments 3, 4 and 7 [read]: `-1` keeps the default, 4 goes to spawn `+24`, 7 to `+36` (`0x8000E4A4` to `0x8000E5F4`). Arguments 1 and 2 [inferred] from matching Npcs chr and model (`ed3(25, 54, -1, 31, ...)` at 1429 line 130, Npcs3 model 54). The rest not traced. Script 1429 makes type 32 Broken Blue Urn, 33 Filled, and rune 57, 47 or 56 by `ed16(player)`. [read]

## Where things are placed

[proved] All 14 `NpcsN.npc` start with `0x2C` and five `{count, bytes}` pairs, with records of 40, 104, 44, 52 and 60 bytes, summing exactly to file size. Class by `ed_chrs.dat`: table 1 all class 2 (323), table 2 all class 3 (621), table 3 all class 4 (181), table 4 all class 2 (612), table 5 all class 4 (15). So only tables 3 and 5 hold class 4, the pickups. Scripts make others with `ed3`, and 1,311 of 3,083 live things match no table 3 or 5 (type, tag).

- **Table 3** (loader `0x80041AFC`): `+0` x, y, z, `+6` room, `+8` word (low byte a mask), `+28` model, `+30` type, `+32` chr, `+38` flags, `+40` tag. [read]
- **Table 5** (loader `0x80041ED4`): the same place and `+56` tag, with three `{model, chr, flags, type}` sets at `+32`, `+40`, `+48`.
- **Table 5's set is chosen by a word: 1, 2 or 3.** [read] `r13-26320` holds a pointer and the chooser is the word it points at (`0x80041F78`, `0x80041F88`). 1 selects `+32`, 2 `+40`, 3 `+48`. 0, or 4 and above, selects none. Table 4 has the same shape (`0x80041D2C`). [proved] `0x8003F4BC(n)` points it into `0x8023B97C + 12n + 4`, whose first words are 0, 1, 2, 3. `ed17` (`0x800103EC`) reads this word. Live it is 1 in 170 states and 0 in 8. Table 5's room 39 sets are the three Essences (types 3, 4, 1 for sets 1, 2, 3).

**The Blue Urn.** [proved] `Npcs3.npc` table 3 index 0: room 147, type 31, chr 25, model 54, tag 633. Slot 1's handle 62 is type 31, tag 633, object room 147. In the JSON: `placed`, `spawned_by` (literal `ed3`), `tags`, `inv_kinds`.

## Checks run

From `python -I tools/ed/facts/item_table.py`, re-run after the fixes below, all lines ok:

| Check | Result |
|---|---|
| Names and Use scripts: 21 Bronze Necklace 1510, 31 Blue Urn 1429, 150 Talisman 1874, 24 Strange Necklace 2243 | Pass. Each of those script ids sits on one row only |
| `saves/slot1.state`, room 147 | Handles 62 type 31 Blue Urn tag 633, 82 type 30 Tome tag 733, 81 type 103 Scramasax tag 481, 9 type 26 Message Scroll tag 0 |
| `saves/slot3.state`, room 21 | Handles 83 type 32 Broken Blue Urn, 82 Tome, 81 Scramasax, 9 Message Scroll. No node of type 84, 82 or 150 anywhere in it |
| `build/runs/s1.state`, room 338 | Handles 9 type 84 Tulwar tag 915, 10 type 150 Talisman tag 723, 11 type 82 Chakrams tag 0 |
| `logs/saved-changes/working-01b3d85-before-tome-copy/saves/slot10.state`, room 166 | Handle 102 type 21 Bronze Necklace tag 2029, 101 type 24 Strange Necklace tag 685 |
| Carried records against the table | Only slot 3's Scramasax differs in these four: byte 7, the low byte of `+4` |
| Table 5 sets used, reload pairs for 137, 136, 135, 100, 84 | Pass |

**`saves/slot3.state` no longer holds Karim's items.** [proved] Karim's Tulwar, Chakrams and Talisman were checked in `build/runs/s1.state` instead. An older copy, `logs/saved-changes/working-01b3d85-before-tome-copy/saves/slot3.state`, still has the Tulwar in room 338. That s1 is Karim's chapter is [inferred] from those items.

## Fixes made to the delivered files

`tools/ed/facts/item_table.py` was corrected and re-run. `python -I tools/ed/facts/checks/reconcile-items/compare.py` compares against the earlier JSON, kept beside it. Apart from the fields here and `record`, all 244 rows are unchanged.

- `grip` is now `fire_mode`, and `capacity_grip16`, `count_grip16` and `word20` are now `capacity_mode16`, `count_mode16` and `fire_modes`.
- Table 5's `set`: the 5 rows that said 4 now say 3.
- `reload_partners`: Assault Rifle and OICW Grenades corrected. Unknown is now `null`, not `[]`. `reload_masks` added.
- `carried()` no longer breaks on a type of -1, and reads type, tag and kind as signed.
- Added `tome_actions`, `ed140_results` and `notes`.

## Corrections for the kit

These aren't changed, as this job only writes under `build/ed-db/facts/`.

- **`fn25` is the current room, not "what a thing had used on it".** [proved] `0x8016A9D4` returns `r13-23276`. That word equals the player object's `+592` in 178 of 178 states (147 in slot 1, 151 in slot 5, 155 in slot 7). 4 of the hashes 1429 tests under `fn25() == 155` appear only in `rooms.json` room 155. Wrong in `tools/ed/primitives.tsv` line 15 and `docs/game/systems/items.md` line 12. During a load the word can be something else (`-1` at `0x8002216C`, 96 at `0x8001C794`). [read]
- **`ed item` loses things.** [proved] `items.json` is keyed by label, and 9 labels are in two groups: ULY, XEL, MANT (1 and 11) and BLOWGUN, CROSSBOW, 45, 38, 8MM_REVOLVER, 303_RIFLE (3 and 4). `ed item BLOWGUN` prints "WEAPONS" with the darts' words, and the Blowgun is gone. "208" counts labels, including non-items such as `NO_ITEMS_IN`. There are 244 types, and 31 `_INV` labels no row points to (`item_labels_with_no_row`; one is `NO_ITEMS_IN_INV`).
- **Tome labels for the kit:** use Check, not Examine, and Mode, not Grip. [proved for the labels]

## Not established

**Refuted, and replaced above:**

- **"`+0` is grip."** Refuted by all three checks: non-zero modes exist on 4 guns only, and the game's words say fire selector.
- **"Table 5's set is chosen by the word being 1, 2 or 4."** The values are 1, 2, 3. A 4 selects nothing.

**Failed on my own check, and replaced above:**

- **"612 slots link."** The right figures are under Tome slots. Slots do not all link.
- **"The Tome's labels are in textures."** They are in boot group 14. Whether the menu also draws them from textures was not checked.
- **"`ed140`: group 3 with group 4 when masks share a bit."** Necessary, not enough. See the results table.
- **`reload_partners` as first delivered.** Wrong for the OICW. Fixed.
- **"The word at `r13-26320` was 1 in the saves."** True of those four, but it is 0 in 8 other states.
- **"480 is on both the Two-Thirds and the Broken Green Urn."** Too narrow: 22 types carry it.

**Not found, or not traced:**

- **2243 is the Strange Necklace, as a statement about the log.** The record field is [proved]: type 24's `+38` is 2243, alone on the table, and scripts 1510 and 2243 share room 166 and region 3624994851. No run log ties a handle to it, the log's spell `0x40011041` is not a literal in `L02/2243.lua`, and the log's "in slot 1" was not reproduced: `saves/slot1.state` doesn't carry type 24 today.
- **What sets `ed82` true when.** It is `r13-24244 == 3`, and 3 with the Tome up in a hang state (`use-mix.md`). That word is -1 in 105 states, 3 in 90, 2 in 29 and 0 in 2, and has 23 store sites (`0x80111174` stores 3, in the code `hle.c` names as the Tome's opening). So which path a Tome Mix and an out-of-Tome reload each take is not proved.
- **`0x800C2CA4` is Z (settled in the completeness check):** both callers are in event 207, which the controller sends for Z.
- **Live proof of mode 16.** No state holds a kind 24 gun, so the mask rewrite was never seen live. `ed140` result 6 is from code and the words.
- **Things of type -1.** What they are. Some may be co-op copies.
- **Flag 602:** settled in `use-mix.md`: `L03/865`, and native code at `0x8001CA5C` and `0x80007680`.
- **Other `+80` values**: 2, 20, 26, 56 on types 122, 37, 159, 123. Entries 2, 20 and 26 are unrelated words and 0,56 does not exist. `0x801C6FA0` to `0x801C6FB0` also forces 15 when `+78` is 15.
- **Fields with read sites but no traced meaning:** `word8`, `rotation_12`, `short44_46_48` (`0x80150B74`), `tome_50`, `tome_52`, `shorts54_66`, `byte75`, `byte83`, `byte84`, `byte85`, other `+4` flags. Raw values are under `record`.
- **Which Ancient goes with which set:** settled in `arguments.md`: 1 Chattur'gha, 2 Ulyaoth, 3 Xel'lotath [read]. Set words 2 and 3 were never seen live.
- **53 ids with no placing or literal `ed3`**, so `inv_kinds` is empty. 11 are the placeholders. Some may come from `ed3` with a variable type.
- **9 tags scripts search for, with no source found:** 765, 780, 843, 944, 998, 999, 2233, 2264, 2268.
- **Scripts 1588 and 1589** (L00, L12, L13) look like Mix scripts on types 209 to 212, but no InvU record points to them. How they run: not found. They may be dead; that is unproven. `li r5, 1588` at `0x8007BF70` is a clear size, not a script id.
- **Script 1359 and type 121.** It tests 121 and 122, but 121 is "Letter #4", with no key piece of that id. Stale script or unexplained partner: not traced.
- **The Blue Urn's story across states.** Slots 2 and 3 show a Broken Blue Urn at handle 83 with the object address the Blue Urn had in slot 1, and its tag changed from 633 to 481. How: not traced.
- **The "Hands" column in `docs/items-log.md`** (2, 1 and 2, 0). It cites the comment at `coop_rehold`. Whether any field of this table holds those values was not checked.
- **Tables and files not used:** `Record.bin`, `UBootPak.bin` sections 3 and 5, `U_LevelNN.bin`.
