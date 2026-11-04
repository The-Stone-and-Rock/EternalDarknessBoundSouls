> Kept as the evidence behind `docs/game/systems/`. Reports were checked by skeptics before the kit was built. Where a report says the kit is wrong, the kit has since been corrected. `tools/ed/facts` holds the scripts it cites, and `ed build facts` remakes the tables.

# Use, Equip, Check, Mix, Mode: how the Tome's Inventory works

Every statement is marked:

- **proved**: checked against live memory, or data and code agree
- **read**: from the code alone
- **inferred**: from usage

What failed review is in [Not established](#not-established). Addresses are PAL. `r13` is `0x80625420`.

Check commands, from the repo root:

| Command | Gives |
|---|---|
| `python -I tools/ed/facts/usemix_ann.py START END` | Annotated disassembly |
| `python -I tools/ed/facts/usemix_saves.py [STATE...]` | Each state's carried items and equipment, named through the disc. Defaults to `saves/` and `build/runs/`. Fixed: it now skips states with no player and exits 0 |
| `python -I tools/ed/facts/usemix_list.py` | All 244 kinds |
| `python -I tools/ed/facts/mix_table.py` | Rebuilds `mix_table.json`. Fixed: it now reads Npcs tables 3 and 5 and `build/runs/`, and lists where each piece comes from |
| `python -I tools/ed/facts/checks/reconcile-usemix/live.py [items]` | Every state: room, flags 601 and 602, record, page state; bytes that ever differ from the disc |
| `python -I tools/ed/facts/checks/reconcile-usemix/equip.py` | Equipment, InvU in memory, urn nodes, `r2` doubles, the level's script table |
| `python -I tools/ed/facts/checks/reconcile-usemix/scripts.py` | Counts over all scripts: who uses `ed20`, `fn92`, `ed140`, `ed167`, `fn162`, `ed9`, `ed11`, `fn25` |
| `python -I tools/ed/facts/checks/reconcile-usemix/refs.py HEX...` | `lis`+`addi` references to an address |

## The game's functions, to call instead of rebuilding

| To do this | Call | Mark |
|---|---|---|
| Run a script | `0x801642D4(script, a, b)`. `a` becomes Lua `pTrigger`, `b` becomes `pTarget`. **Does nothing** if the script is <= 0, another is running (`r13-23228` nonzero, `0x801642FC`), or the loaded level has no such script (`0x80164334`) | read; level gap proved |
| Id to node, node to object | `0x801F5F88(id)`, then `0x801F6398(node)` (`node+44`) | proved |
| Object to kind | `0x80151184(obj)` (`+12`), script `fn92(id)` | proved |
| Equip or let go | `0x800C9FD4(holder id, item id, 0, hands, 0)`. It toggles | proved |
| Build the list | `0x80101DC0(first, 0, 6)` | read |
| Weapon and ammo check | `ed140(a, b)`, `0x800168AC` | proved |
| Find carried ammo | `0x801F9B18(player node, weapon object)` | read |
| Take an item from the controlled character | `fn93(id)`, `0x8016CD00` | proved |
| Read the Tome's record | `0x80110478(i)`, script `ed20(i)` | proved |
| Put words up | `0x801DB374` then `0x80024310`, script `ed11(group, entry, script)` | read |

## Three disc files hold every item

| File | What | Layout | Reached by | Mark |
|---|---|---|---|---|
| `InvU.bin` | 244 item **kinds** | `{244, 0x6B5}`, then `{offset, size}` per kind; each is a 16 byte header + 88 bytes | `0x8015062C(kind)` copies the 88 bytes; table in `r13-23412` | proved |
| `InvS.bin` | 55 **templates** | Same shape, 332 bytes each, the data at `+0x60` | `0x801505F0(n)`; table in `r13-23416`. Shared, not copied | proved |
| `InvE.bin` | 6 **hand sets** | Same shape, masks at chunk `+16`: `[]`, `[2]`, `[4]`, `[16]`, `[1]`, `[2, 1]` | Template `+0` | read |

**Proof for InvU (proved).**

- The table in memory equals the file for all 244 records, in every state tried. Run `equip.py`, or `sh tools/ed/facts/checks/skeptic/usemix/run.sh e` for 13 states. In memory the offsets are fixed up to pointers.
- Over 407 carried records in `saves/`, `build/runs/` and `logs/`, only bytes 7, 28-31, 38-39, 69, 81 and 82 ever differ from the file.
- In `usemix_saves.py`, 94 of 98 listed items match on `+36..+43` and `+76..+79`. Many are the same items repeated, so the table compare is the strong evidence.

**The 4 that differ (proved).** They are kinds 70 and 74, ids 16 and 17, in `coop-apart` and `coop-recover`, with Use 864 against 0 on the file. `L12/2324` is what wrote it:

- With `ed17()==1` it spawns exactly that carried set: kinds 168, 170, 64, 88, 156, 30, 216, 70, 74.
- It calls `fn162(x, 0, 864)` on 70 and 74.
- It gives 88 and 156 a count of 100 with `fn121`, and live they read 100.
- 2324 is in the loaded script table in `coop-apart`.

`L00/2412`, `L12/2412`, `L13/2412` and `L00/853` also write 864, on one rune each (read).

**The chain, id to name.** id → `0x801F5F88` node → `node+44` object → `object+4` record → text `(+76 group, +79 entry)` in the boot pack (proved). These are starting words only: `fn162` which=3 and 4 rewrite an object's own `+79` and `+78` at run time (`L05/483`, `L09/2522`, `L09/2523`, `L16/2522`, `L16/2523`) (read).

**The object (`0x801506C8`, read; layout proved by the walk above).** `+0` template pointer, `+4` pointer to its private record copy (`0x80150714`), `+8` template number, `+12` kind (`0x80150728`), `+16` reload bits, `+20`/`+24` dual link.

**Kinds are fixed; ids belong to one play (proved).**

- In room 147, `slot1` has Message Scroll 9, Scramasax 81, Tome 82. `slot9` has Message Scroll 65, Scramasax 66, Tome 67.
- In room 5, under `logs/saved-changes/working-01b3d85-before-tome-copy/saves/reports`, the Tulwar is id 9 in `hang-20261101-031036`, 89 in `hang-20261101-114539` and 90 in `hang-20261101-115901`.
- Kinds: Tulwar **84**, Chakrams **82**, Talisman **150** (Use 1874, count 5), Bronze Necklace **21** (Use 1510), Blue Urn **31** (Use 1429). Live in `build/runs/s1.state`: id 9 kind 84 template 10, id 10 kind 150 count 5, id 11 kind 82 template 51 count 19.
- **Key on the kind, not the label.** Labels repeat: Dual Tulwar is 83 and 109, Piece of Green Urn is 35 and 188, Revolver is 3 kinds (`scripts.py`).

### The kind record

| Offset | Reader | What | Evidence | Mark |
|---|---|---|---|---|
| `+0` | `0x80150814` | Current fire mode bit | Mode writes it, `0x80150A88`. Only kind 136 starts at 16 | read |
| `+4` | `0x801507FC` | Flags. Bit 0: refused by the Enchant pick's equip (`0x80103FE0`). Bit `0x20`: Chapter Page, A in Check runs its Use | Bit 0 on 32, 37, 123, 131, 159, 227, 230, 238. `0x20` on 200-208, 213, 219, all Chapter Pages | read, data |
| `+36` | `0x80150868` | **Pickup script**, run with `pTarget` = the item's id | `0x800D9B80`..`0x800D9BA4` | read |
| `+38` | `0x80150850` | **Use script** | `0x80104330`, `0x8010435C`, run at `0x801043D0` | proved |
| `+40` | `0x8015085C` | **Check script**: A while reading its words | `0x80104114`, run at `0x80104214`. Only kinds 60, 67, 154, 160, 161, 165, 167, 176 | read, data |
| `+42` | `0x80150874` | **Mix script**, also run by the reload path | `0x8010433C`, `0x80104368`, `0x800C2D24` | proved |
| `+50`, `+52` | `0x801507B4`, `0x801507C0` | Numbers the Tome's slot keeps (`0x80101AA0`) | Meaning not traced | read |
| `+68` | `0x801508FC` | Count: in the clip, box or charges. **Only when `+0` is not 16.** At 16 it reads byte `+74` | `0x80150820`, `0x80150924`, `0x80150930` | read |
| `+70` | `0x801508B0` | Clip size. `+72` when `+0` is 16 | `0x801508D8`, `0x801508E4` | read |
| `+76` | `0x80150B5C` | **Text group. 3 = weapon, 4 = ammo** | `ed140`, `0x801F9B8C` | read |
| `+77`, `+78`, `+79` | `0x80150B2C`, `0x80150B38`, `0x80150B44` | Pickup, description, inventory name entries | `0x8010384C`, `0x8010483C` | read |
| `+81` | `0x80150A20` | Enchant pick refuses when this is set and flag `0x100` is on | `0x80103F5C`..`0x80103F6C` | read |

`fn162(id, which, value)` (`0x8016C688`, jump table `0x802543DC`) writes: 0 `+38`, 1 `+40`, 2 `+77`, 3 `+79`, 4 `+78`, 5 `+36`, 6 `+81`, 7 `+82` (read). In scripts, which=1 is only ever value 0 (2522, 2523), so no kind gains a Check script at run time (read).

### The template

`+0` hand set, `+16` flags (`0x801507F0`), `+20` modes (`0x80150808`), `+24` third word of the equipment entry (`0x800CA0D4`), `+28` **ammo class, u16** (`0x8015119C`) (read; live templates match InvS, `run.sh l`, proved).

The Mode action writes `+28` when the class has `0x500`: 0x400 for mode bit 0x10, else 0x100 (`0x80150A50`..`0x80150A80`, `0x801511B8`). Since `0x801505F0` hands out the shared record, that write reaches every object on that template (read).

The template comes from the thing's definition: `ai/ed_chrs.dat`, 224 byte records from `+0x20`, `+4` id, `+136` template (`0x80031D80`). So `ed3(def, model, -1, kind, ...)` picks both (proved: `L04/1203` `ed3(39, ...84)`, `ed3(137, ...82)` and live `s1` templates 10 and 51).

## The page

Handlers in the table at `0x8024EF0C`, from the dol (read):

- Mask `0x100` → `0x80103EAC`.
- Mask `0x200` → `0x8010495C`.
- Mask `0x03001000` → `0x80103BAC`.

Enter is `0x80102810`, draw `0x80103198` (`0x8010286C`), leave `0x80103A90` (`0x80102888`). The hover function `0x80104798` is called at `0x80102C10` and `0x80103A00`. `ed53` leaves and re-enters the page (`0x80012C50`, `0x80012C54`) (read).

**Page state is `r13-24492`** (read; state 2 proved live in `slot5`).

| State | Meaning | A | B |
|---|---|---|---|
| 0 | The list | `0x80104700`: state 1 | `0x80104D9C`: `r13-24244` = 2 |
| 1 | Action menu, cursor `0x80334D30` | `0x80104304`, the actions | `0x80104D84`: state 0 |
| 2 | Mix: second item, cursors `0x80334D34`, `0x80334D38` | `0x8010422C` | `0x80104D34`: state 0, list rebuilt |
| 3 | Check: zooming in | Nothing | Nothing |
| 4 | Check: reading | `0x801040EC` | `0x80104A10` onward: state 5 (`0x80104A60`) |
| 5 | Check: zooming out | Nothing | Nothing |

**Action order (proved).** `0x80104370` switches 0..4 to Use `0x801043A8`, Equip `0x801043F0`, Check `0x801044C0`, Mix `0x80104580`, Mode `0x801045EC`. `boot 14,0..4` are USE, EQUIP, CHECK, MIX, MODE.

`0x80102C28(n)` says whether an action can be picked (read):

| n | When | Code |
|---|---|---|
| 0 Use | `+38` not 0. Kind 12 (Archaeologist's Brush) also needs event 52 to answer | `0x80102CC8`..`0x80102D14` |
| 1 Equip | Already held, or `0x801512F8` finds a hand set | `0x80102D1C` |
| 2 Check | Always | `0x80102D58` |
| 3 Mix | Flag 602 on, and `+42` not 0 | `0x80102D60` |
| 4 Mode | Template `+20` has 2 or more bits | `0x80102D88`, `0x801DBDA8` |

**Flag 602 (proved).** Three writers:

- `L03/865`, the Mix tutorial, which the page runs at `0x80102BB0` when flag 601 is on and 602 off. 865 exists only in L03. Flag 601 is set by `L03/830`, the pickup script of kind 188.
- `0x8001CA5C` in `0x8001C86C` (called at `0x8001FC34`), through jump table `0x8023DD3C` on the word `0x80307178`: values 4 to 15 reach it.
- `0x80007680` in `0x800074E8` (called at `0x80007EC0`). Which values reach it is not mapped.

Live, by that word (`live.py`):

| Word | Flag 602 off | Flag 602 on | Note |
|---|---|---|---|
| 0 | 7 states | 4 states | |
| 1 | 18 | 0 | |
| 2 | 1 | 0 | |
| 3 | 10 | 4 | On in `slot5`-`slot8`, after the urn pieces |
| 4 | 0 | 28 | |
| 7 | 0 | 50 | |

What the word means is not traced. It is 3 in Anthony's saves and 4 in Karim's.

## Action → game function → data

| Action | Function | Reads | Writes | Runs | Mark |
|---|---|---|---|---|---|
| **Use** | `0x801043A8` | Slot's `+38` and id (`+792` slot first if linked, `0x80104318`) | `r13-24496` = 2, state 0 | `0x801642D4(use, id, 0)` at `0x801043D0` | read; live trace below |
| **Equip** | `0x801043F0` | `0x8006DB04(0x1000)`; first hand mask (`0x8010440C`..`0x80104414`) | `r13-24336` = 1, list rebuilt, state 0 | `0x800C9FD4(player id, id, 0, hands, 0)` at `0x80104450`. If `0x8006DB04` answers: `boot 0,36` NO_ARMS | read |
| **Check** | `0x801044C0`, draw at `0x80103838`..`0x80103894` | `+76`, `+78`. Kind 31 with flag 266 off starts `0x801A4884(12, ...)`; kind 61 starts `0x801A4884(68, ...)`, kept in `r13-24484` | State 3, 4 | At state 4, once the words window is ready: flag `0x20` and `+38` set runs `0x801642D4(+38, id, 0)` at `0x801041C0`; then `+40` set runs it at `0x80104214` | read |
| **Mix, first** | `0x80104580` | | `r13-24536` = first pick's `+42`, stored on A at the menu **for every action** (`0x80104340`, `0x8010436C`). State 2, window cleared | | proved in `slot5` |
| **Mix, second** | `0x8010422C` | Second cursor, must differ from the first (`0x80104274`) | | `0x801642D4(r13-24536, 0, 0)` at `0x801042A0`. If `r13-24244` is still 3, state 0 and the list is rebuilt | read |
| **Mode** | `0x801045EC` | `0x80150808`, `0x80150814`. Cycles left to the next set bit | `0x80150A2C`. Object `+16`: count not 0 clears `0xA0`; ammo carried clears `0x80` sets `0x20`; else clears `0x20` sets `0x80` | `0x80124768`, `0x801258BC` | read |

### The record: `0x80335748`, six words

| Word | What | Mark |
|---|---|---|
| 0 | Hovered, so the first pick | proved |
| 1 | The second pick. Index chosen at `0x801047B4`..`0x801047C0`: word 1 only in state 2 | proved |
| 2, 3, 4 | `0x8011048C(text entry, x)`. Bit 0 of word 4 is **Enchant-pick mode**: words `boot 0,1` at `0x80103240`, called from the Magick page `0x8010D6F8` and `0x801C45B0`. A in that mode is `0x80103F70`, which equips it, or says `boot 0,3` at `0x80104094` | read |
| 5 | That window (`0x80103258`) | read |

**When word 0 or 1 is written (read).** At `0x80104880`, only when the name window `r13-24504` is 0, `r13-25960` is 0 and the list count `r13-24508` is nonzero (`0x801047C4`..`0x801047E4`). The same block makes the window (`0x8010487C`), so it is written **once per window**. Clearing the window re-arms it, as `0x801045E4` does on entering state 2.

Other writers: the 24 byte `memset` at `0x801102A8`, and `0x80111198` + `0x801111AC` (words 0 and 1 cleared on entering Tome mode).

**Live (proved).**

- `saves/slot5.state`: record `[105, 66, 0, 0, 0, 0]`, page state 2, `r13-24536` = 1498, Broken Green Urn id 106 (kind 37) carried, nodes 66 and 105 gone.
- Id 66 is a kind 188 node in `slot1`-`slot4`, on no carried list. `Npcs3.npc` table 3 places kind 188 three times in room 152.
- `slot1` record `[62, ...]`, the Blue Urn. `build/runs/s1.state` record `[9, ...]`, the Tulwar.

Id 105 being kind 36 is **inferred**: no state holds that node.

### The slots: `0x80334388`

Arrays of 6, 44 bytes each: `+264` (`0x80334490`) the list, `+792` (`0x803346A0`) the linked item. The hands come from three calls at `0x80102538`..`0x801025DC`. Slot `+0` from `0x80101AB0`, `+32` a value from `0x80101AC4`, `+36` object, `+40` id. Built by `0x80101DC0(first, 0, 6)` walking `player node +48` (read). `+36`/`+40` also proved: 562 of 588 filled slots at `+264` link, in `item-ids.md`. Slots go stale.

**Dual weapons are not a Mix (read; no live pair exists).** `0x80150948` stores object `+20`, `+24`. `ed167` (`0x800162FC`) calls it twice: the first item gets (0, second), the second gets (first, 0). `ed167` also re-equips if either was held (`0x80016400`..`0x80016480`).

Its only script callers are `L04/1978` (pickup of kinds 83 and 109), `L05/483` (pickup of kind 229), and `L04/448`, which `L04/1381` starts with `ed187`, not a pickup. Native code links at `0x800B8F4C` and `0x800B8F64`.

The list builder skips an item with `+20` set (`0x80102614`). One with `+24` set has the partner in `+264` and itself in `+792` (`0x80102650`..`0x80102674`). Over 1,756 live item nodes, none has `+20` or `+24` set (`live.py`).

**A linked pair splits word 0 from the id (read).** Hover takes the id from `+264` (`0x80104820`), the partner. Use, Equip and Mix take `+792` first (`0x80104314`..`0x8010433C`). So `ed20(0)` and `pTrigger` can name different objects on dual weapons.

## How Use runs a script

`0x801642D4` writes `r13-23192`, `r13-23196`, and `0x80164388` sets `pTrigger` (string `0x80253EDC`) and `pTarget` (`r13-28412`) (read). They are pushed with `0x801637EC`, and scripts read them through `fn85` (`0x80165B40`, which calls `0x80163600`), not as plain numbers (read).

**Level gap (proved).** The table in `r13-23212` is the loaded level's. Live sizes from `equip.py`:

| State | Present | Absent (size 0) |
|---|---|---|
| `slot1` | 1498, 1429 | 1582, 2013, 2221, 1359 |
| `s1` | 1582, 1874, 1359 | 1498, 2013, 2221 |
| `coop-apart` | 2013, 1582, 2324 | 1498 |

Per the script packs: 1498 is in L03 only, 2221 in L05 only, 1359 in L04 only. 1582 and 2013 are in many levels, but not all. A call for a missing script does nothing, silently.

**The urn (proved by the trace and script).** `docs/coop-tome-pickups.md` frame 7413 is "script 1429 with item 62", and `slot1` has id 62 kind 31. `L03/1429` step 0:

1. `ed20(0)`, then `fn93`.
2. `ed9(1429)`, then `fn1(1)`.
3. As the Tome closes, `0x800082A8` runs `0x801642D4(r13-26324, 0, 0)`. `TOME_SCRIPT_RAN` in `hle.c` is that return address, `0x800082AC`.

**How a script reads the item (proved by count, `scripts.py`).** 64 of 68 Use scripts use `ed20(0)`, which is `0x80110478` at `0x80010CF4`. The exceptions:

- 1400 (kind 119) reads `fn85(pTrigger)`.
- 1874 uses `ed158(0)`, whose native reads word 0 itself (`0x80015C50`).
- 1427 and 1609 read no item.

`pTrigger` is the id on a Tome Use, and 0 on a Mix and on the run at the Tome's end.

**`ed9` (read; `r13-24244` values proved).**

- Writes `r13-24244` = 2, `0x8031649A` = 0, `0x80316490` = -1 (`0x8000F900`..`0x8000F908`), and `r13-26324` = arg 1 (`0x8000F980`).
- `ed9(script, mode)` sets the mode instead; `ed9(script, mode, x)` also sets `0x8031649A`. In scripts only `mode` 6 is used.
- `r13-24244` is 3 with the Tome up (`0x80111174`; live 3 in `hang-20261101-031036`, 2 in most saves, -1 before play).
- As the Tome ends, `0x80110DE8` passes it to `0x80018808`, which sets the game mode. The mode table at `0x80238810` has enter and frame functions: mode 2 enter is `0x80008234`, which holds `0x800082A8`; mode 3 is the Tome, `0x80111078` and `0x80111234`. Modes 7, 8, 10 and 11 enter through `0x80112D98`, which calls `0x80111078` and sets `r13-24244` to 2.
- So the left script runs on entering mode 2. With mode 6 that site does not run at the Tome's end.
- `r13-26324` is read only at `0x80008280` and `0x8000829C`.
- Of 83 scripts calling `ed9`, 77 are on item records. The other 6 are 1454, 1455, 1871, 1879, 1588, 1589.
- The first argument need not be the caller: 1460 passes 344, 1100 passes 2116.

**Nothing in `ed9` opens a Tome (read).** Mode 3 is set by `0x80018808(3)` at `0x800264D8`, `0x8004F8B4` and `0x8004FDF0`. The first tests byte `0x8031649C` == 2.

**`fn25()` is the room number (proved).** `0x8016A9D4` returns `r13-23276`. Live: `slot1` 147, `slot5` 151, `slot6` 153, `slot7` 155, `coop-apart` 100, `title` -1. Of 160 values scripts compare it with, 159 are rooms in `rooms.json`; the other is 403 (L09 and L16 script 61). So 1429's 153 and 155 are room tests.

**Refusing (read).**

- `ed11(group, entry, script)` is `0x8000FF20`: words only, no item touched.
- Default refusal: `ed11(0, 0, 0)`, `boot 0,0` CannotUse, in 59 of 68 Use scripts.
- Others in Use scripts: `0,30` TOO_DANGEROUS_BRUSH (14 sites), `13,46` (4), `0,38` NO_CHARGES (2), `8,44` C4_NOT_HERE (1).
- Mix refusals: `0,28` MIX_NO_MATCH (1498, 1606, 1383, 1882), `0,0` (1198, 1359, 2514).
- A branch-following scan found no `fn93` before a refusal (`sh tools/ed/facts/checks/skeptic/usemix/run.sh r`; its one hit, 1518, is guarded).

**`ed158(n)` (read).** `n` = 0 returns the count of word 0's item. `n` = 1 or 2 takes one, if the count is nonzero and `0x801C4250`, `0x80068D18` and `0x80043218` pass (`0x80015D18`..`0x80015D68`).

**`fn93(id)` (proved).** Id 0 is skipped. Otherwise `0x801F965C(r13-21484's node, node)` removes it from the carried list, then `0x801DCB58(2, node)` queues message 2. The queue's consumer was not read. Live: nodes 66 and 105 are gone in `slot5`.

**`fn92(id)` (proved).** Reaches `0x80151184`, the kind. Used in exactly 9 scripts: the 6 live piece scripts and the unreached 1588, 1589, 2514.

## Equip

Equipment is at `0x80588DF0`, 20 records of 408 bytes (`0x80151500`). `+0` holder id, `+4 + 12n` entries `{id, hand mask, third word}`, `+388` count, `+392` hands it has, `+396` free, `+400`/`+404` hold and let-go callbacks (proved: `equip.py`, +392 is 6, +400 `0x800C994C`, +404 `0x800C9DD4`, set by `0x80151634` at `0x800B8EE4`).

`0x800C9FD4(holder, id, 0, hands, 0)` toggles (read):

- **Held** (`0x8015147C` not -1): `0x80150FA0` lets go of it and any partner.
- **Not held:** lets go of everything on those hands (`0x80150F48` loop), then `0x80150D9C(eq, id, hands, template +24)`.
- **Id 0:** lets go of all (`0x800CA110`).

A partner is added with mask 1 by calling itself (`0x800CA100`).

`0x80150D9C` writes the entry, sets `+396 = (old & ~hands) | third word` (`0x80150E10`..`0x80150E24`), and calls `+400`. It has 7 other direct callers, which bypass the toggle (`0x8000EABC`, `0x8006B9A0`, `0x8006B9F4`, `0x8006BA48`, `0x8009E080`, `0x8009E0F0`, `0x8009E14C`).

**Live (proved).**

- `slot3` `(81, 2, 0)` with `+396` = 4.
- `slot7` `(68, 2, 0)`.
- `s1` `(9, 2, 1)` with `+396` = 5, the Tulwar, template 10, third word 1.
- `coop_rehold`'s `11, 2, 0` fits Chakrams template 51, third word 0, hand set `[2]`. That is from the disc, as no state has the Chakrams equipped.

## THE MIX TABLE

**There is no recipe table (proved).** Mix runs `+42` of the first picked item: 44 kinds, 9 scripts. Six piece scripts compare `fn92` kinds; three ammo scripts call `ed140` and never call `fn92`. All of it is in `mix_table.json`.

### Pieces (proved: both orders read in each script, `+42` from the file)

| Script | Level | First + second, kinds | Makes | Wrong pair says | Inputs come from |
|---|---|---|---|---|---|
| 1498 | 03 | 188+188, 188+35, 35+188 (35+35 refused) | 36 | `boot 0,28` | 188: `Npcs3` table 3, room 152, three times. **35: none found** |
| 1498 | 03 | 36 + 35 or 188, either way | 37 (live: id 106 in `slot5`) | `boot 0,28` | 36: made by 1498 |
| 1606 | 09, 16 | 186 + 187, either way | 159 | `boot 0,28` | `L09/2026`, `L09/2090` and the L16 copies |
| 1383 | 11 | 130 + 128, either way | 129 | `boot 0,28` | `Npcs11` table 3 |
| 1882 | 11 | 217 + 214, either way | 131 | `boot 0,28` | `Npcs11` table 3 |
| 1198 | 12 | 236 + 237, either way | 238 | `boot 0,0` | `Npcs0` table 3 |
| 1359 | 04 | 122 + 121, either way | 123 | `boot 0,0` | **122: none found.** 121: `L05/2005` only |

Kind 121 is Letter #4 with `+42` = 0, so 1359 can only start from 122 (proved). "None found" means no literal `ed3` and no Npcs table 3 or 5 placing; tables 1, 2 and 4 are unread. As found, the Green Urn in play is all kind 188, and no way for 1359 to run was found (inferred).

Results 37, 123, 131, 159, 238 have flag bit 0.

Kinds 162, 163, 129 have no mix script. They have Use scripts 1456, 1457, 1384 (read).

**Not reached (read).** 1588, 1589 and 2514 are named by no item record field and no other script. The only native literal is `li r5, 1588` at `0x8007BF70`, a `memset` size.

- 1588, 1589 (L00, L12, L13): 209, 210, 211 → 212 → 213, with `boot 0,29`.
- 2514 (L00): second item must be the Tome, kind 30; first is a Chapter Page, which goes to `fn73(1572..1580)`.
- Kinds 209-212 have `+42` = 0, and Mix can't be picked on them. Kind 211 has pickup script 1602.
- No script spawns 209-211 by literal, and Npcs tables 3 and 5 place none.
- `BLANKONETHIRD` (13,37) and `BLANKTWOTHIRD` (13,40) have words and no kind record.

### Ammo through Mix

**Scripts (proved).** 1582 (17 kinds), 2013 (12), 2221 (3). All 32 kinds are text group 3 or 4 (`scripts.py`).

```
step 0: ed82() == 1 (r13-24244 is 3: 0x80012ED0)
          code = ed140(ed20(0), ed20(1))
          code == 1: ed9(script), fn1(1)
          else: ed130(code)
        else: the reload now
step 1: fn1(0), the reload
```

**`ed140(a, b)` (`0x800168AC`).** Codes and words proved: the doubles were read live, and `ed130` (`0x80012F58`) maps them.

| Code | When | `ed130` says |
|---|---|---|
| -1 | An id has no node | Nothing |
| 2 | See the two branches below; or the weapon's class is 0 | `boot 0,31` MIX_ERR_NO_WEAPON_OR_AMMO |
| 3 | The weapon isn't held by the current player (`0x80151664(id, 0x801F62B8())`) | `boot 0,32` MIX_ERR_WEAPON_NOT_EQUIPPED |
| 6 | Classes don't overlap, and both are 0x100 or 0x400 | `boot 0,35` MIX_ERR_NOT_RIGHT_MODE |
| 4 | Classes don't overlap | `boot 0,33` MIX_ERR_AMMO_NO_MATCH |
| 5 | Loaded is not below the clip (counts a partner's too) | `boot 0,34` MIX_ERR_WEAPON_FULLY_LOADED |
| 1 | OK | |

**The two branches are not symmetric (read).** `a` in group 3 requires `b` in group 4 (`0x80016980`). `a` not in group 3 only requires `b` in group 3, and never checks `a` is group 4 (`0x80016A94`). It does not show through these scripts, since all 32 kinds are group 3 or 4.

**Fits = template `+28` bits overlap** (`0x80016A04`, `0x80016B18`) (read). Ties below (proved: script `ed3` literals, Npcs tables 3 and 5, live nodes; `mix_table.py` finds no kind with two classes):

| Class | Weapons (kind) | Ammo (kind) |
|---|---|---|
| 0x1 | Shotgun 100, 168; Double Shotgun 185 | Shotgun Shells 88 |
| 0x2 | Revolver 155, 170 | .38 Revolver Ammo 156, 215 |
| 0x4 | Blowgun 91 (`+42` = 0) | Template 31 has it; no ammo kind tied |
| 0x8 | Automatic Pistol 90 | .45 Pistol Ammo 143 |
| 0x10 | Pistol 138 | 9mm Pistol Ammo 139 |
| 0x20 | Elephant Gun 157 | Elephant Gun Shells 158 |
| 0x40 | Revolver 140 | 8mm Revolver Ammo 141 |
| 0x80 | Rifle 142 | .303 Rifle Ammo 171 |
| 0x100 | Assault Rifle 137 | 5.56mm Rifle Ammo 135 |
| 0x200 | Crossbow 93 | Crossbow Bolts 86 |
| 0x400 | (Mode switches 0x100 to it) | OICW Grenades 136 |
| 0x800 | Flintlock Pistol 149, Dual Flintlocks 229 | Flintlock Pistol Ammo 87 (`Npcs5` table 3, def 121, template 38) |
| 0x1000 | Flash Pan 59 | Flash Powder 68 |
| 0x2000 | Chakrams 82 (`+42` = 0, count 19) | No ammo kind tied |

Still untied: 85, 95, 233, 243.

**The reload, by script (read, followed through the code).**

| Script | Native | Event | Path |
|---|---|---|---|
| 1582 | `ed80` | 155 (`0x80012D40`) | `0x80059958` → `0x80079798` → `0x8007A1DC` on an animation frame (`0x800798B0`) |
| 2221 | `ed129` | 198 (`0x80012E8C`) | `0x80059914` → `0x80079C58`, or `0x80059924` → `0x80079E48`, both registering `0x8007A1DC` (`0x80079D6C`, `0x80079E08`, `0x8007A05C`). Event 204 also reaches `0x80079E48` (`0x80053D48`) |
| 2013 | `ed81(p, x)` | 177 if `x` is 1, else 208 (`0x80012DE0`) | `0x80059874` or `0x80059840` → `0x80079900`, then one at a time, below |

**Script 2013's path (read).**

1. `0x80079900` starts animation 158, player state 75, and `0x800BFD38`.
2. State 75's handler `0x800530F4` (`0x8005AAD0`) posts 179 on event 6 (`0x80053184`), and on 179 calls `0x800799F8` (`0x80053298`).
3. `0x800799F8` starts animation 10, state 72, with `0x8007A09C` on a frame (`0x80079B88`). While more can go in, it asks for 179 again (`0x80079AE4`).
4. State 72's handler `0x800533A8` (`0x8005AAEC`), on 179: goes round if byte `r13-25592` is set, or bit `0x2000` of `0x8019B038(0x800460E4())` is set (`0x80053528`..`0x80053538`). Otherwise `0x80079BB0`, animation 159, state 76.

Byte `r13-25592` is 1 only for event 208 (`0x80059838`). That event 6 means "animation ended" is **inferred**.

`x` is `fn85(pTrigger)`. Z's run passes 1 (`0x800C2D28`, inside `0x800C2CA4`, which event 207 calls at `0x800597F8`, `0x8005980C`). A prompt press passes 0 (`0x8000703C`..`0x80007048`), and so does the run at the Tome's end. **So Z gives 177, and the RELOAD prompt and the Tome give 208** (read; corrected in the completeness check). Bit `0x2000` below is Z held, set by `0x8019A1A0`, so Z loads one at a time for as long as Z stays held, and the prompt or Tome loads until full.

**The two loads (read).**

- `0x8007A1DC`: `0x801F9B18` finds the box. If it fits, clip += box and the box is removed and destroyed (`0x801F965C`, `0x801DCB58(2, ...)`). Otherwise box -= room and clip is full.
- `0x8007A09C`: box -1 (destroyed at the last), clip +1.
- Both clear object `+16` bit `0x20` (`0x8007A35C`, `0x8007A1A8`).

**No frame, no ammo used (read).** If the animation lookup returns -1, the clip is filled to full with no ammo taken and no box destroyed: `0x80079880`, `0x80079B44`, `0x80079D40`, `0x80079D98`, `0x80079F74`.

**`0x801F9B18` (read).** First carried item that isn't the weapon, is group 4, and has an overlapping class.

**Reload from outside the Tome.** `0x800C2CA4(player, hand)`, called for hand 1 then 0 (`0x800597F8`, `0x8005980C`): held item with `+42`, ammo carried, clip not full → `0x801642D4(+42, 1, 0)` (read). `0x800C05F0`: object `+16` bit `0x20` and ammo carried gives `+42`, or **1582** when 0 (`0x800C0690`), and `0x800C0360` then shows prompt 21 (`0x800C0374`) (read). `prompts 0,21` is RELOAD, so "the RELOAD prompt" is **inferred** from that number.

**Picking up ammo (read).** 13 of the 14 group 4 kinds have pickup script 2036 (Flash Powder 68 doesn't): `ed131(pTarget)`, then `fn93` if it returned 1. `ed131` (`0x800130EC`) walks the carried list for group 4, same class, not itself, and adds the count (`0x8001325C`).

## What is the one player's

All read above. Each means "the" player, which matters for co-op.

| Data | Used by |
|---|---|
| `r13-21496` player id (`0x801F62B8`), `r13-21492` player node (`0x801F62B0`) | Equip, list builder, `ed140` held test, `0x8007A1F0`, Mode's ammo test |
| `r13-21484` controlled character (`0x801F6250`) | **`fn93`**, so a script's taking of an item |
| `r13-26524` player's object | Mode, `0x80124768` |
| Player node `+48` carry list | List builder, `0x801F9B18` |
| `0x80588DF0` equipment, by holder id | Equip, `0x80102D24` |
| **`0x80335748` record, one** | `ed20`, `ed158`, hover, Enchant |
| **`0x80334388` slots, one** | Everything on the page |
| `r13-24492` page state, `r13-24536` mix script, `r13-24504` name window, `0x8024EF78` words | Page |
| `r13-24244` end mode, `r13-26324` left script | `ed9`, `ed82`, `0x800082A8`, B |
| `r13-25592` reload byte | 2013's reload |
| Flag table `r13-26528`, flag 602 | Mix available |
| Template `+28`, shared | Mode writes it |
| `r13-23228` running script | Second `0x801642D4` during a script does nothing |
| Lua `pTrigger`, `pTarget` | Every script |

The two loads differ: `0x8007A1DC` uses the one player node (`0x8007A1F0`). `0x8007A09C` gets its owner from the animation (`0x8019B734`, `0x8007A0B4`) (read).

## Corrections for the kit

Not changed here.

| Row | Says | Is | Mark |
|---|---|---|---|
| `tools/ed/primitives.tsv` line 15, `fn25 used_thing` | What was used | The room number. `docs/game/systems/items.md` line 12 too | proved |
| `fn93 face_a` | Guess | Takes an item from the controlled character | proved |
| `ed9 tome_open` line 40 | Asks for the Tome | Sets the mode the Tome ends in, leaves a script | read |
| `ed20` | Word 0 | Word N of the record | read |
| Missing | | `fn92` kind, `ed82` Tome up, `ed140`, `ed130`, `ed158`, `fn162`, `ed167`, `ed80`, `ed81`, `ed129`, `ed131` | read |
| `items.md`, `pickups.md` "Open" | Table from id to name | `InvU.bin` | proved |
| `docs/items-log.md` | Tulwar 9, Chakrams 11, Talisman 10, Necklace 102 | Ids for one play. Kinds 84, 82, 150, 21 | proved |
| `docs/game/research/item-ids.md` "Not worked out: Flag 602" | Open | See [Flag 602](#the-page) | proved |

## Not established

### Failed review: do not rely on these

| Earlier claim | Why it failed |
|---|---|
| "Ids are per level" | Ids change within one room and level. `slot1` and `slot9` are both room 147 with different ids; room 5 has the Tulwar at 9, 89, 90 |
| "Flag 602 is set only by `L03/865`" | Native code sets it too: `0x80007680`, `0x8001CA5C` |
| "Record word 0 or 1 is written each frame at `0x80104880`" | Once per window, gated at `0x801047C4`..`0x801047E4` |
| "The mix script compares kinds with `fn92`", as a general rule | Only the 6 piece scripts. The 3 ammo scripts, 32 of 44 kinds, use `ed140` |
| "The load is `0x8007A1DC`", for every reload | Not for 2013, whose events 177 and 208 go through `0x80079900` to `0x8007A09C` |
| "Flintlock ammo kind 87 is untied" | `Npcs5.npc` table 3, def 121, template 38, class 0x800 |
| "A script reads the item with `ed20(0)`, not `pTrigger`" | 1400 uses `fn85(pTrigger)` |
| "`ed9(script)` only writes" two words | It also writes `0x8031649A` and `0x80316490` |
| "Scripts 2412 and 853 wrote the 864 on the carried 70 and 74" | They write one rune each; `L12/2324` fits the carried set exactly |
| "No save holds a Tulwar, Chakrams or Talisman" | True of `saves/` only. `build/runs/s1.state` holds all three |
| "The earlier saves carry pieces 65, 66, 67" | Those are nodes in `slot1`-`slot4`, on no carried list |
| "All ammo has pickup 2036" | 13 of 14. Flash Powder 68 doesn't |
| "One must have group 3, one group 4" in `ed140` | Half of the code: `0x80016A84` onward never tests `a` |
| "`+68` is the count", unconditionally | Only when `+0` is not 16 |
| "Room records were not decoded" | Npcs tables 3 and 5 are decoded. Tables 1, 2, 4 still aren't |
| Bit 1 of word 4 → `0x80106E60`, "a 300 tick timer" | Bit 1 only skips making the window at `0x80103230`; the link to `0x80106E60` at `0x80103A70` was not shown. Dropped |

### Not ruled out

- **"Nothing can write `+42`".** No `fn162` case does, and bytes 42-43 never differ in 407 carried records, but about 50 `sth rX, 42(rY)` sites exist and not all were traced. A computed address can't be excluded. Use "no setter found".
- **Whether 1588, 1589, 2514, kinds 35, 122, 209-211 are ever reached.** Npcs tables 1, 2 and 4 are unread. Variable-kind `ed3` calls seen (`L04/2180`, `L04/2310`, `L05/2288`, `L05/2589`, `L05/2775`, `L12/2324`) assign other kinds.
- **That a refusal keeps the item, at all 295 `ed11(0, 0, 0)` sites.** Scanned for Use scripts, not read one by one.

### Not traced

- `0x801DCB58` message 2's consumer. Only the outcome is seen (`slot5`).
- How a script opens the Tome. Where mode 3 is set is known; what leads there isn't.
- The meaning of word `0x80307178`, and which values reach `0x80007680`.
- `0x8006DB04(0x1000)`, the test behind NO_ARMS.
- `0x80106DD4`, at A and B in Check.
- The words window flag bits at `0x80104124`..`0x80104150`.
- `0x800BFD38`, `0x80101AA0`'s use of `+50`, `+52`.
- Hand bits' body parts. `+82`..`+85` of the kind record. Flags other than bits 0 and `0x20` (127 has `0x80`, 169 has `0x500`).
- What finishes results 37, 123, 131, 159, 238 from flag bit 0.
- No state has a linked dual pair, or Chakrams equipped, so those are from code and disc only.
