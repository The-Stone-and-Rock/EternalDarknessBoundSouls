# Pickups and finds

**For a normal player:** the PICK UP prompt (19), B, the item in a close-up with `<player> has found a X!`, B, it's in the Tome.

`ed verb pick up`. `ed item NAME` gives the words, the placing, the tag and the Examine that offers it. Evidence: `docs/game/research/verbs.md`.

**Three things ask for prompt 19.** The close-up always reaches the find, a thing in the world only with `+56` of 2 and bit `0x4000` of `0x80118C4C` off, and a region's script only if it calls `ed52`.

| Asker | Asks | B runs |
|---|---|---|
| **A thing in the world** | `0x800C030C`, each tick of the player's state 1, if `0x800DA688` finds a kind 30 thing near: `(2, 19, 2246, 0, 49, 1, 0)` | Script 2246, the same in 15 levels: `fn79(player, 71, 0)`. Event 71: `0x800C362C` |
| **A close-up** | `ed221` with its thing there, press 2 | `0x80025408`: `0x801F9DE8(thing, player, 30)`, then the find |
| **A region's script** | `ed301(2, 19)`: 28 calls, 9 script ids | The script again. What it does differs: `L00/1413` goes to a room |

**The find:** `0x80024528(element, -2, player, handle, 0, 0, 0)`. Five call sites: `0x800254F0`, `0x800C3804`, `0x80012BE8` (`ed52`), `0x800C3054`, `0x801C6FF4`. **`ed52(handle, script, n)` is the find from a script.** 112 files use it and never ask prompt 19, so a find doesn't always have a prompt. Its opening and closing **give** the item: never skip it.

| Step | Game function | Notes |
|---|---|---|
| Making it pick-upable | `0x801F9DE8(thing, player, 30)`: asks the thing with event 240, then node `+12` = 30, `+33` = 1 | `fn90(thing, 30)` from a script |
| The words | `0x80024434`, the record at `0x8030A6E8` | Row `+76`, `+77`: `ed item` |
| A find's hold | `0x80042BB8` | Bit 2 of `r13-24048`, `r13-25932`, which script timers wait on |
| After | The row's `+36` script, event 190 (`0x800D9B64`), `pTarget` the handle | Not every find has one: 3-Point Circle's is 0, and 2129 is a world script |

`0x800C030C` checks pick up, reload, autopsy, finish him in that order: only the first true one asks.

Ammo, script 2036: `ed131(pTarget)` adds it to a carried box, and `fn93` takes the new one.

**Reading a save:** `ed carried SAVE`.

**For two players:** `coop_thing_look` asks for the thing in the world as them. `coop_found_for_finder`, `coop_found_mark`, `coop_found_notice`, `coop_find_hold`, `coop_run_script` (only the finder runs it). The find gives to its third argument, `r5`: current from a close-up and `0x800C3054` (thing 856), main from `ed52`, the event's node from `0x800C3804`, `r27` from `0x801C6FF4` (the enchant's caster, by the host). `hle_coop_found` rewrites `r5` so player 2's find goes on player 1's list. `ed221` tests **main**'s list for "already carried": `ed who`. `docs/coop-tome-pickups.md` traces the urn.

**Not read one by one:** every `ed52` call as a pickup.
