# Location: who is where

| What | Where |
|---|---|
| A node's object | node `+52` |
| The object's place | object `+0`, `+4`, `+8`, as floats |
| The room in play | `r13 +` `g_coop_room_offset` |
| What's at a place | `ed where ROOM X Y Z`: regions it's inside, nearest cameras and markers |
| Regions and markers by name | `ed room N`: `fn23(hash)` asks about a region, `fn63(hash)` finds a marker |
| Is the player in this region | `fn23` `0x8016A7DC`, through the zones' inside words: `info +180`, `116` bytes each |

**Each player has their own inside words:** `coop_zones_swap`.

**See both players:** `ed timeline RUN`, `ed see FRAME RUN`. Every frame's place is in the run's `both.tsv`. `ed check` flags `SENT` (moved over 150 units in a few frames with no room change) and `FELL`.
