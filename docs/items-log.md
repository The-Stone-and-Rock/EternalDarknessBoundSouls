# Eternal Darkness: items seen

Items found while tracing, in any session. Only what was seen in a log or written in the code. A blank means not seen yet.

**The Item id column is a handle: it lasts one play, and the same number can be another item later.** The game's own table is `ed item`, by type id, always the same. Bronze Necklace is type 21, Tulwar 84, Chakrams 82, Talisman 150, Blue Urn 31.

| Item | Item id | Use script | Hands | Description (from the game) | Source |
|---|---|---|---|---|---|
| Bronze Necklace | 102 | 1510 (`0x5E6`), then `0x8C0` | | "A well-crafted Bronze Necklace, resplendent with glowing gems." | Runs on slot 1 and slot 10, 2026-11-02. A shared quest item. Refuses with "cannot use that now" outside its zone |
| Strange Necklace | | | | "A Strange Necklace, fashioned from a dark ancient stone and engraved with weird designs. It c..." (log cut off) | Slot 10, 2026-11-02 |
| Short Sword | | | | "A Short Sword with a wooden grip - light and durable. A small curve running from the handle m..." (log cut off) | Slot 10, 2026-11-02 |
| Tulwar | 9 | | 2, 1 | | Comment at `coop_rehold`, `runtime/host/hle.c` |
| Chakrams | 11 | | 2, 0 | | Comment at `coop_rehold`, `runtime/host/hle.c` |
| Talisman | 10 | 1874 | | Five uses in slot 3 for player 2. When used up: "The Magick in this item has been completely drained. It cannot be used anymore." Casts `0x40011041` twice | `logs/run-20261103-093522-hand9` |
| Circle of Power (3 Point) | | 2129 (the find, on the desk in save 2) | | Tome page: "Circles of Power describe the amount of power that a spell will have. The more points within that circle, the stronger the spell will be." | `logs/run-20261103-181505-both` |

## Not yet matched to a name

- An item whose Use runs script 2243 (`0x8C3`) and casts spell `0x40011041`, in slot 1.
- Item ids a script asked for in Paul's chapter: 678 and 733. The save had them, but the log gives no names.

## Player 2's Tome tabs

Player 2 doesn't need Journal or Cinematics in their Tome for now, so they aren't planned for a player 2 menu.
