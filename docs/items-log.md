# Eternal Darkness: items seen

Items found while tracing, in any session. Only what was seen in a log or written in the code. A blank means not seen yet.

| Item | Item id | Use script | Hands | Description (from the game) | Source |
|---|---|---|---|---|---|
| Bronze Necklace | 102 | 1510 (`0x5E6`), then `0x8C0` | | "A well-crafted Bronze Necklace, resplendent with glowing gems." | Runs on slot 1 and slot 10, 2026-11-02. A shared quest item. Refuses with "cannot use that now" outside its zone |
| Strange Necklace | | | | "A Strange Necklace, fashioned from a dark ancient stone and engraved with weird designs. It c..." (log cut off) | Slot 10, 2026-11-02 |
| Short Sword | | | | "A Short Sword with a wooden grip - light and durable. A small curve running from the handle m..." (log cut off) | Slot 10, 2026-11-02 |
| Tulwar | 9 | | 2, 1 | | Comment at `coop_rehold`, `runtime/host/hle.c` |
| Chakrams | 11 | | 2, 0 | | Comment at `coop_rehold`, `runtime/host/hle.c` |

## Not yet matched to a name

- An item whose Use runs script 2243 (`0x8C3`) and casts spell `0x40011041`, in slot 1.
- Item ids a script asked for in Paul's chapter: 678 and 733. The save had them, but the log gives no names.

## Player 2's Tome tabs

Player 2 doesn't need Journal or Cinematics in their Tome for now, so they aren't planned for a player 2 menu.
