# Words that mean different things

The research reports, the host and the disc use these differently. In the kit's pages:

| Word | Means here | Also called |
|---|---|---|
| **Type id** | An item's row in `InvU.bin`, object `+12`. Always the same | "kind" in `use-mix.md` |
| **Handle** | A thing's id, node `+16`. Lasts one play | "item id" in `docs/items-log.md` and older logs |
| **Tag** | From a placing, node `+36` then `+24`. `fn78(2, tag)` | "thing id" in `verbs.md` |
| **Chr** | A record in `ai/ed_chrs.dat`, `ed3`'s first argument | "def" in `use-mix.md` |
| **Template** | A record in `InvS.bin`, 55 of them, shared by every object using it | "inv_kind" in `item_table.json` |
| **Node kind** | Node `+8`: 2 the player, 30 an item | "node type" in `arguments.md`, "kind" in `verbs.md` |
| **Node** | A thing in the game's list: `+8` kind, `+16` handle, `+20` state, `+36` spawn, `+44` object info, `+48` carried list, `+52` model | |
| **Object** | `+44`, the item record holder: `+0` template, `+4` its own row, `+8` template number, `+12` type id | "info" in `item-ids.md` |
| **Model** | `+52`, position, animation and room `+592` | "object" in `arguments.md`, `transitions.md` |
| **Current** | `r13-21496` id, `-21492` node. "The player" in most functions | "controlled" in `arguments.md` |
| **Main** | `r13-21488` id, `-21484` node. "The main character" | "controlled" in `use-mix.md` |
| **Tick** | One game loop: one frame, 1/60 s (measured) | |
| **Word (of a script)** | Its step, in the table at `0x805CAA40`: `fn0`, `fn1` | "state" in `primitives.tsv`'s short names |
| **Check** | The Tome's action | Examine is the world's prompt |

**Field names in `item_table.json`:** `after_examine_script` is the row's `+40`, the Check script. `after_find_script` is `+36`. `capacity` is `count_max` in `mix_table.json`. `fire_mode` is `mode`. **`placed[].table` 3 is `placed_things.json`'s `section` 2:** they count from 1 and from 0.

**How sure:** proved is checked against a save's memory or a run, or data and code agree. Read is from the code alone. Inferred or guess is from usage. Use the lower mark where two reports differ.
