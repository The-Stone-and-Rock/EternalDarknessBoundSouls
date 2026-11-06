# Co-op: both players' Tomes open at once

**Status: set aside, not working.** `mods` allows one Tome at a time. The attempt is on the local
branch `two-tomes-wip` (commit `618d29f`), for reference and not for merging. Backups with builds are
in `logs/saved-changes/two-tomes-*` (not in git; only on this machine).

The design to try next, not yet built: `docs/coop-two-tomes-design.md`.

What `mods` keeps, checked by replayed runs on 2026-11-02:

- The world keeps running behind one player's Tome (`bac33e8`, `c3b7f54`).
- Instant quick spells cast while the other player is in their Tome. No code was needed for this.
- Enchant goes on with no close-up and no pause (`d0b1fdc`, `7d2daf7`).
- Player 2's presses in their Tome no longer reach the world's prompts.

Known limit on `mods`: Enchant can't be cast by one player while the other has their Tome open. The
second player's Start does nothing while a Tome is open.

## Read this before changing anything

The attempt went wrong by looping: form a theory, patch a memory range, test, get a new symptom.
Several theories were stated as fixes and were wrong. So:

- Read this file and the code before experimenting. Don't repeat a test that only confirms a theory.
- The user's real play finds what scripts miss. Replay their presses from their log first.
- Whole-half brightness misses item bugs. Use `items.ps1` or look at the frames.
- Say what was and wasn't checked. Limit tests to two per change, as the user asked.

## How the game works here (from reading its code)

The game was built for one Tome and one set of Inventory items.

| What | Where | Note |
|---|---|---|
| Game loop, frame routine | `0x800186E8`, mode `r13-26240` | Pad read applies to the next frame |
| Page routines (`r13-24304`) | Inventory `80105F9C`, Map `80112594`, Magick `8010F228`, Journal `8010952C`, Options `80101980` | |
| Inventory's item builder | `80101DC0` | Called on page entry (returns to `801028F0`) and on equip (`80104480` etc.) |
| Item slots | `0x80334388`, 44 bytes each; lists at `+264`, `+792` | `+4` is the object |
| Page leave teardown | `8011031C`, callback `801102F4` | Queues everything on the menu list to be freed; object is node `+56` |
| Frame clean-up | `8014FE24` marks, `8014FA84` + `8015012C` sweep | Runs in each kind of frame; a Tome frame sees only its own page |
| Thing lists | Heads `r13-23428`, `-23432`, `-23448`, `-23444`, selector `r13-23456` (set by `8014ED04`) | Selector picks the world's list or the menu's |
| Pools | Records `0x810D70D8` (28 bytes each); nodes `0x81100720`, links `0x81108620` | |
| Frame side | `r13-21824`, flipped each frame in `80022354` | Buffers and display-list cache are per side |
| Display-list cache | `0x804F0ADC`, write places `r13-23996..-23956` | Reset by `801197E8` |
| Item buffers | `8011F460` allocates, `8011FC8C` frees; counts `r13-23904`, `-23900`, `-23896` | `+672` still the stub at `0x8057BBD4` means not drawn |
| Tome draw of an item | `80105F30` > `8011A278` > `8011B814` | Selected item hidden on purpose when `r13-24492` is 3..5 |

## What each two-Tome bug turned out to be

| Symptom | Cause | What fixed it on the branch |
|---|---|---|
| Selected item blinks | Display-list counters kept per Tome over one shared cache | Share them |
| Huge, warped items | World/A/B order makes the frame side drift | Set Tome B back to Tome A's side |
| Items vanish when the other turns a page | `8011031C` clears the whole menu list | Keep objects the other Tome drew lately |
| Freeze at `80150420` or `8014F218`, "left recompiled code" | Heap words kept per view included list links, while the heads were not | Keep links and pool records out of the kept words |
| Other Tome's items go after an equip | It held the old slots | Copy the new slots across |
| Black half after B in a Tome | Tome's B reached the world's prompt, and its script closed the menu | Fixed on `mods` |

**The world's turns kept the Tome's camera (fixed 2026-11-05):** `kTwSwitches` swapped the camera stack's depth (`r13-21700`) between the world and the Tome but not which record is on (`r13-26864`). Closing a Tome popped nothing, the game's camera stayed on the Tome's record 8, and player 1's view stood still until a room change, in every chapter, from 2026-11-02 (`bac33e8`). A save made then, reloaded for a change of characters, showed player 1 black. Both are now in the list. `docs/game/systems/camera.md`.

## Still unsolved

1. **Only the Mace listed.** After player 1 visits Journal, Magick or Map and closes the Tome, the
   next Tome has slots 1 to 4 empty, so the page lists one thing. It happens with one Tome open on
   the branch. Not a buffer leak (counts stay steady), and not fixed by keeping all of
   `0x81100000..0x81120000` out of the kept words. Next place to read: how the builder's walk
   (`801F63A0`, `801F9938`, `801F98C8`, `801F5F88`) and `hle_find_by_id` answer for that player.
2. **Flat items, missing key.** Two solo equips, then both Tomes open, then an equip. Objects and
   slots match a good run apart from timer `+2CC`; vertex buffers at object `+168` differ.
3. Two Tomes run at about 80% speed. Part of the swapped memory (`0x80644560`, 637 KB) is a frame
   image and could probably go.
4. Not tried: a pick-up while the other's Tome is open, Use and Mix, the cinematics lock.

## Recording and replaying a session

```bash
sh tools/coop-test/play.sh saves/slot7.state my-test
```

This runs the game with every log on and writes `logs/run-<time>-my-test/`: `page-NNNN.log`,
`INDEX.txt`, and `EVENTS.log` (read that first). With `ED_COOP_PAD_LOG=1`, each player's button
presses are logged with their frame number. Freezes write `saves/reports/hang-*.txt` with the stuck
address and call chain.

To play a session again from its log, `tools/coop-test/replay.py` makes a temporary test build (its
header explains how). Slots start at different frames, so read the first `[split] on at frame` line
first. Keep controllers untouched during a replay.

- `winshot.ps1`: picture of the game window, to `Screenshots for Claude\`.
- `burst.ps1 -seconds N -out DIR`: a run of frames plus `frames.csv`.
- `items.ps1 DIR`: per-half count of item cells drawing, one digit per sample. A page with no items,
  like Options, reads low correctly.
