# Items

**All 244 are in `InvU.bin`, and an item's type id is its row.** `ed item TEXT|ID` gives a row's words, its Use, Check, Mix and after-find scripts, its ammo, where it's placed and its Examine. `ed item --list` for all. Evidence: `docs/game/research/item-ids.md`.

**Four numbers get mixed up. Check which one you have.**

| Number | Where it lives | Lasts | Bronze Necklace, for example |
|---|---|---|---|
| **Type id** | `InvU.bin` row, object `+12`, `0x80151184(object)`, script `fn92(handle)` | Always | 21 |
| **Handle** | node `+16`, carried entry `+8`, Tome slot `+40`, `0x80335748` word 0. `0x801F5F88(handle)` gives the node | One play | 102 in one save |
| **Tag** | node `+36` then `+24`, from the placing. `fn78(2, tag)`, `ed221`'s 5th argument | Fixed by the placing, not unique | 2029 in room 217, 270 in room 172 |
| **Chr record** | `ed3`'s first argument, `ai/ed_chrs.dat` | Always | 25 |

The ids in `docs/items-log.md` and most logs are handles: Tulwar 9, 89 or 90 is type 84, Chakrams type 82, Talisman type 150. Key on the type id. Labels repeat, so a name can be several rows: Torch is 105, 222, 235 and 240. `ed carried SAVE [HANDLE]` shows a save's handles with their types, names and what's in hand. Saves get re-saved, so a save may not hold what an old log saw.

**From a handle to its words:** `0x801F5F88` node, node `+44` object, object `+4` its own copy of the row. Text is in `UBootPak.bin`: group `+76`, pickup `+77`, description `+78`, name `+79`.

## The row, 88 bytes

| Offset | Getter | Setter | What |
|---|---|---|---|
| `+0` | `0x80150814` | `0x80150A2C` | Fire mode bit. Non-zero on 54 rows, but a template's `+20` lists modes on only 4, and only Elephant Gun, Assault Rifle and Double Shotgun have the 2 that offer MODE |
| `+4` | `0x801507FC` | `0x80150AE8` or's, `0x80150AD4` clears | Flags. `0x20` Chapter Page: A in Check runs Use. `0x200` set by an enchant |
| `+36` | `0x80150868` | `0x80150B08` | Script after a find, with `pTarget` the handle |
| `+38` | `0x80150850` | `0x80150AB0` | **Use script** |
| `+40` | `0x8015085C` | `0x80150ABC` | **Check script**, A while reading: 8 kinds only |
| `+42` | `0x80150874` | none found | **Mix and reload script** |
| `+68`, `+70` | `0x801508FC`, `0x801508B0` | `0x80150990` writes the count, no capacity setter found | Count and capacity. `+74`, `+72` when `+0` is 16 |
| `+76` to `+79` | `0x80150B5C`, `0x80150B2C`, `0x80150B38`, `0x80150B44` | `0x80150AFC`, `0x80150B20`, `0x80150B14` | Text group, pickup, description, name |

**Scripts change rows as the game goes:** `fn162(handle, which, value)` writes, `fn212` reads: `which` 0 `+38`, 1 `+40`, 2 `+77`, 3 `+79`, 4 `+78`, 5 `+36`, 6 `+81`, 7 `+82`. So the table is starting values. `L12/2324` gave kinds 70 and 74 a Use of 864, seen live.

**What a player carries:** node `+48` is `{+0 zero, +4 first entry, +8 count}`, each entry `{+0 previous, +4 next, +8 handle}`. A thing is in one list at a time.

**Finding things from a script:** `fn78(kind, n)`. 1 the current player, 2 tag in this room, 3 last thing of node kind n, 4 tag in any room, 5 tag carried by current. A carried thing keeps an object room, so `fn78(2, tag)` can return what's carried. The list runs newest first, so the newest match wins.

**Where things are placed:** `NpcsN.npc`, five tables. Tables 3 and 5 hold type ids (table 5 one per alignment: 1 Chattur'gha, 2 Ulyaoth, 3 Xel'lotath). Scripts make others with `ed3(chr, model, room, type, a5, a6, tag, node kind, facing, position, effect)`.

**For two players:** one of each thing, so what's found is gone for both (`coop_run_script`, `g_coop_found_word`). Player 2's list is empty, so theirs borrows player 1's for a cast (`COOP_SPELL_ITEM`). Their weapons are their own (`coop_rehold`). The Tome reads one pair and `fn93` the other: `ed system use-mix`.

**Not worked out:** 10 rows hold an unexpanded `~` code as a name, and 12 aren't names: `ed item` shows them as they are. Fields `tome_50`, `tome_52` and several others. What type -1 things are.
