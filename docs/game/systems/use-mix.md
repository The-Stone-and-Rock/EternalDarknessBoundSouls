# Use, Equip, Check, Mix, Mode: the Tome's Inventory

**The same five actions for every item in the game.** `ed verb use` (or equip, check, mix, mode) gives the function, arguments, gates and which player it reads. `ed item ID` gives one item's scripts. `ed mix` lists every Mix. Evidence: `docs/game/research/use-mix.md`.

These functions are switch cases in the page's A handler, `0x80104304`, not callable: what a host can call is `0x801642D4` and `0x800C9FD4`. A Tome copy runs the page itself.

| Action, words in boot group 14 | Offered when (`0x80102C28`) | Page code | What runs |
|---|---|---|---|
| 0 USE | `+38` not 0. Kind 12 also needs event 52 answered | `0x801043A8` | `0x801642D4(+38, handle, 0)` |
| 1 EQUIP | Already held, or `0x801512F8` finds a hand set | `0x801043F0`, then `0x800C9FD4(holder, handle, 0, hands, 0)` | A toggle |
| 2 CHECK | Always | `0x801044C0` | Words `+76`,`+78`. Then `+38` if flag `0x20`, then `+40` |
| 3 MIX | Flag 602 on and `+42` not 0 | `0x80104580`, second pick `0x8010422C` | `0x801642D4(first's +42, 0, 0)` |
| 4 MODE | Template `+20` has 2 or more bits | `0x801045EC` | Cycles `+0`. Mode 16 rewrites the shared template's ammo class |

**Examine is the prompt in the world, and Check is the Tome's action.** They're different functions.

## The page

Page state is `r13-24492`:

| State | Meaning | A | B |
|---|---|---|---|
| 0 | The list | `0x80104700`: state 1 | `0x80104D9C`: `r13-24244` = 2 |
| 1 | Action menu, cursor `0x80334D30` | `0x80104304` | `0x80104D84`: state 0 |
| 2 | Mix: second item, cursors `0x80334D34`, `0x80334D38` | `0x8010422C` | `0x80104D34`: state 0, list rebuilt |
| 3, 5 | Check: zooming in, out | | |
| 4 | Check: reading | `0x801040EC` | `0x80104A10` on: state 5 |

**The record, `0x80335748`, six words:** word 0 the hovered or first pick, word 1 the second pick, both handles. `ed20(i)`, `0x80110478(i)`, reads it. Written once per name window (`0x80104880`, gated at `0x801047C4`), so clearing the window re-arms it. Live in `slot5`: `[105, 66, 0, 0, 0, 0]`, page state 2, mix script 1498.

**The slots, `0x80334388`:** 44 bytes each, `+264` the list, `+792` the linked item. `+36` object, `+40` handle. Built by `0x80101DC0(first, 0, 6)` from `player node +48`. Slots go stale: 562 of 588 link. Check the node.

## How a Use script works

`0x801642D4(script, a, b)` makes `a` Lua's `pTrigger` and `b` `pTarget`. **It does nothing if another script is running (`r13-23228`), the script is 0 or below, or the loaded level lacks it (proved from live table sizes).** So an item whose script isn't in this level silently does nothing: `ed item ID` shows each script's levels.

The usual shape, drawn from `L03/1429`, the urn, which tests the room only once its flag 266 is on. Of the 68 Use scripts, 64 read `ed20(0)`, 62 test `fn25`, 52 take the item with `fn93`, and 49 have this whole shape, so using an item doesn't always consume it:

```
ed20(0)                 -- which item
fn25()                  -- the room: refuse elsewhere with ed11(0, 0, 0), words 0,0 CannotUse
fn93(item)              -- take it, from main's list
ed9(1429); fn1(1)       -- run on, at word 1, as the Tome closes (0x800082A8, TOME_SCRIPT_RAN)
```

- **`ed9(script[, mode[, x]])`** leaves a script for the Tome's end, and the mode it ends in. Nothing in it opens a Tome.
- **`fn25()` is the current room**, not what was used.
- **`fn93(handle)`** takes from main (`r13-21484`).
- **`ed158(0)`** counts word 0's item. `ed158(1)` or `(2)` casts through `0x801C4250`, and takes a charge if the cast and two more gates pass: the Talisman, 1874.

## Mix: there's no recipe table

Mix runs the first pick's `+42`: 44 kinds, 9 scripts. **Six scripts compare `fn92(ed20(0))` with `fn92(ed20(1))`,** and three (1582, 2013, 2221) call `ed140`. `ed mix` has every pair and class.

| Script | Level | Pieces |
|---|---|---|
| 1498 | 03 | 188 + 188 or 35 → 36. 36 + 35 or 188 → 37 |
| 1606 | 09, 16 | 186 + 187 → 159 |
| 1383, 1882 | 11 | 130 + 128 → 129. 217 + 214 → 131 |
| 1198 | 12 | 236 + 237 → 238 |
| 1359 | 04 | 122 + 121 → 123 |

1588, 1589 and 2514 are named by no item, so nothing is known to run them.

**Flag 602 opens Mix.** `L03/865`, the tutorial, sets it, and so does native code by chapter (`0x8001CA5C`, `0x80007680`).

**`ed140(a, b)`,** then `ed130(code)` for the words:

| Code | When | Words |
|---|---|---|
| 1 | Fine | |
| 2 | No weapon or ammo pair, or weapon class 0 | 0,31 MIX_ERR_NO_WEAPON_OR_AMMO |
| 3 | Weapon not held by **current** | 0,32 MIX_ERR_WEAPON_NOT_EQUIPPED |
| 4 | Classes don't overlap | 0,33 MIX_ERR_AMMO_NO_MATCH |
| 5 | Already full | 0,34 MIX_ERR_WEAPON_FULLY_LOADED |
| 6 | Classes don't overlap, both `0x100` or `0x400` | 0,35 MIX_ERR_NOT_RIGHT_MODE |

## Reload

| How | Goes through | Then |
|---|---|---|
| **Z**, event 207 | `0x800C2CA4(player, hand)`: the weapon's `+42` with `pTrigger` 1, if ammo is carried and it isn't full. No script means no reload | |
| **RELOAD prompt**, id 21 | `0x800C05F0` tests, asked at `0x800C0388`: the weapon's `+42`, or 1582 if 0 | `pTrigger` 0 |
| **Tome Mix** | `+42` with `ed82()` true, then `ed140`, `ed9`, and on again as the Tome closes | `pTrigger` 0 |

| Script | Native | Load |
|---|---|---|
| 1582 | `ed80`, event 155 | All at once, `0x8007A1DC`, on an animation frame |
| 2221 | `ed129`, event 198 | `0x8007A1DC`, by way of `0x80079C58` or `0x80079E48` |
| 2013 | `ed81`: 177 from Z, 208 from a prompt or the Tome | One at a time, `0x8007A09C`. 177 goes on while Z stays held, 208 until full |

Ammo kinds (13 of 14) have after-find script 2036: `ed131(pTarget)` adds to a carried box of the same class, and `fn93` takes the new one.

## Which player each reads

**There are two pairs, and a co-op fix must move both** (`ed who`, `ed system players`).

| Reads | Functions |
|---|---|
| **Current**, `r13-21496`, `-21492` | Tome list, action test, Equip, Mode, `ed140`, `ed131`, the load-all reload, the find |
| **Main**, `r13-21488`, `-21484` | **`fn93`**, `ed221` and its carried test, `ed52`, among others: `ed who ADDR` |

One of each, for both players today: the record `0x80335748`, the slots `0x80334388`, page state `r13-24492`, mix script `r13-24536`, reload byte `r13-25592`, the flag table, and template `+28`, which Mode writes.

**For two players:** a Tome copy already runs the page, for Mix as for Use. `coop_run_script` catches the call by its return address (`0x801042A4`, in `TOME_ITEM_CODE`), and `tome_copy_left` replays it in the real game as them, if the script left itself with `ed9`. A script that refuses in the copy never reaches the real game. **Mix, Mode and reload have never been run for player 2.** Separate because they live in the copy: page state, name window, mix script, cursors, slots.

What reading found, for Mix, in that path (nothing here was run):

1. On the script path, `tome_copy_left` and `tome_copy_p1_left` write only word 0 of the record (a cast writes all of it). `ed20(1)` needs word 1.
2. The script's word moves to player 2's table only if words opened. 1498's step 1 opens none and leaves `fn36(3)`, so its next step would run from the game's timer as player 1.
3. Flag 602, if the tutorial runs in a copy, can stay in the copy.

`ed needs --script 1498` lists what the script reaches. No current save holds unmixed pieces.

**Not traced:** `0x801DCB58` message 2's consumer. `0x8006DB04(0x1000)`, behind NO_ARMS. Which body part each hand bit is.
