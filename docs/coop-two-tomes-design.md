# Co-op: two Tomes, the design to try next

**Status: tests 1 and 2 are built on branch `tome-copy`, off unless `ED_COOP_TOME_COPY` is set to 1 or 2. Test 1 passed in the user's play. Test 2 passed by script and in the user's play. Equip and Use from test 4 passed by script and in the user's play. Tests 3 and 5, both Tomes open together and Enchant in every order, passed by script on slot 3 and are waiting on the user's play. Sound, Mix and new spells aren't built. Nothing is committed.** Written 2026-11-02 from reading `mods`, the `two-tomes-wip` branch and the game's code. Read `docs/coop-two-tomes.md` first for what went wrong before.

## Safety rules for this work

- The work is on the branch `tome-copy`, cut from `mods` at `01b3d85`, and never from `two-tomes-wip`. That branch is older and lacks later fixes. Use it only to read.
- Restore point: the tag `coop-working-before-tome-copy` and `logs/saved-changes/working-01b3d85-before-tome-copy/`, with steps in its `RESTORE.txt`. It holds the game exe, the whole build, `.deps`, generated code, saves and settings. It is only on this drive.
- Every new code path stays behind a switch that's off by default, until the user says otherwise.
- Test with `saves/slot3.state` for now, and match the characters to the save before loading, so it doesn't reload them. Slot 3 is Karim with Paul as player 2, which the settings on 2026-11-02 already match (`mod_p1_character = 0`, `mod_p2_character = 7`).

## Test results

**Test 1, scripted, slot 3, no one playing (`logs/run-20261102-220141-t1d-on`), 75 seconds:** 4,200 turns on a copy, 0 failed. Real memory and stack were unchanged after every turn. A fresh 40 MB copy took 3 to 7 ms. A turn usually took about 10 to 24 us, with one of 1.9 ms. No hang report and no character reload.

What that doesn't show: walking, fighting, sound, a Tome, or a door. Each turn ran one small game call (`0x801F6258`), and the renderer doesn't read the copy yet. The user's play is the real test 1.

Found on the way: `hle_guest_call` runs its own loop and never polls interrupts, so a turn run that way can't be interrupted. Running player 2's Tome frame (`0x80111234`) as a guest call may therefore not need the `interrupt_poll` skip below. Not tried yet.

**Test 1, the user's play, slot 3 (`logs/run-20261102-220741-test1`):** 14,400 turns, 0 failed, longest turn about 150 us, with both players' Tomes opened and a room change. No hang. The user saw nothing wrong.

**Test 2, scripted, slot 3 (`logs/run-20261102-222755-t2g`, pictures in `logs/cap-t2g`), `ED_COOP_TOME_COPY=2`:** player 2's Tome opened twice in the copy (2,002 and 703 turns), longest turn 7.35 ms, with player 1's half at about 60 FPS. Inventory items and the Map page drew correctly. Labels named only Map and Inventory (this save has no magick). No hang. Switch off afterwards: the old Tome path opened, turned a page and closed (`logs/run-20261102-222945-t2off`), no `[tome-copy]` lines.

Not covered by test 2: the Magick page (no magick in slot 3), a real controller, walking or fighting beside the Tome.

**Test 2, the user's play (`logs/run-20261102-224053-test2b`):** six opens and closes, no hang, items showing. The user found Equip and Use had no effect, as expected for that build.

**Test 4 part 1, Equip and Use, scripted, slot 3 (`logs/run-20261102-224958-t4b`, pictures in `logs/cap-t4b`):** the Talisman's Use ran script 1874 as player 2 and cast spell `0x40011041` twice, 125 frames apart. Older logs show the same two casts for either player, so that's the item's own. Its count went from 5 to 4 in the log. Equipping the Tulwar made player 2's copy and loaded their moves, and the reopened Tome shows it in the equipped slot. Player 2 was off-camera, so the sword in their hand wasn't seen. Cast from the Tome is written but not run. Switch off afterwards (`logs/run-20261102-225148-t4off`): the old path ran the same script steps and cast at frames 2605 and 2729, against 2603 and 2728 in the copy.

**Test 4 part 1, the user's play (`logs/run-20261102-225236-test4`):** nine opens, both weapons equipped, the Talisman used. The user confirmed the sword in hand and the heal.

**Test 3, both Tomes, scripted, slot 3 (`logs/run-20261102-225841-t3a`, pictures in `logs/cap-t3a`):** player 2 opened, player 1 opened beside it, player 2 turned to the Map with player 1 on the Inventory, player 1 closed and reopened, player 2 closed. Each half was right in every picture: player 1 kept Options, player 2 didn't, and each showed their own equipment. No hang, no `rejected FIFO` lines. 4,815 game frames in about 80 seconds, so the game kept its speed. Switch off afterwards (`logs/run-20261102-230042-t3off`): both old Tome paths opened and closed, pictures right, player 2's label still `Journal`. Not run yet: player 2 reopening while player 1's is open was scripted but cut off by the run's end at frame 6605. Items in both Tomes at once, by real players.

How test 3 fits together: when player 1 has no Tome, the copy stands in for the world-behind-a-Tome state (`g_tome_copy.stands_in`). When they ask for theirs, that state goes back to its own path on `mods` and the copy gets one turn after each of player 1's Tome frames. A snapshot of memory is kept as player 1's Tome is asked for (`g_tome_copy.clean`), for player 2 opening while it's up. The renderer's two kept pictures came from `two-tomes-wip`, applied on top of `mods`; nothing else came from that branch.

**Test 5, Enchant, scripted, slot 3 as saved 2026-11-02 23:16 (Enchant on D-pad up for both, `logs/run-20261102-233058-t5h`, pictures in `logs/cap-t5h`):** player 2 alone; player 1 then player 2 with both Tomes up; both on the same frame (player 1's first cast at frame 11601, player 2's at 11604); player 2 alone again; then plain Start beside player 1's Tome with a page turn. Each cast was by the right player, with no failed calls, no hang, no `rejected FIFO`. Only the Chakrams and the sword can be enchanted, by the game's design: the game refuses the Tome and the Talisman, and the Enchant Tome stays open.

**Test 5, the user's play, both pressing Enchant together, slot 3 (`logs/run-20261103-093139-hand8`):** D-pad up at frames 8166 (player 2) and 8168 (player 1), A at 8511 and 8514. Both casts ran on frame 8515 and again on 8640, each by the right character (`0x5B`, `0x1`). Player 1 enchanted the Tulwar and player 2 their copy of it. No hang, `rejected=0 failed=0`, no exception. Done once. Each alone also worked in the same run (player 1 at 7313, player 2 at 7787).

That also had player 1's Tome open beside player 2's, by Enchant, from frame 8170 to 8514. Not in that run: either player's Start, a page turn with both up, or one Tome closing with the other staying. So test 3 by hand was still open.

**Test 3, the user's play, slot 3 (`logs/run-20261103-093522-hand9`):** player 2's Tome opened beside player 1's twice, and each pressed page buttons while both were up (player 2's RB at frames 7416 to 7563, player 1's LB at 7746 to 7791). Player 1 closed first both times and player 2's stayed open and took more presses. Not in this run: player 1 pressing Start with player 2's already up, so `player 1's Tome has closed; player 2's stays` never showed. The logs can't show what each half drew. Longest turns reached 52.79 and 45.71 ms, against about 19 before. Not looked into.

The run ended stuck on a message, now fixed: see `docs/coop-failed-attempts.md`.

**Use, the record fix, scripted, slot 3:** the failing case, player 1 enchanting the Tulwar and player 2 then using the Talisman, heals player 2 (`logs/run-20261103-095904-r8`, `its item 0x0000000A, the real record's 0x00000009`). Five uses all heal. On the sixth the Talisman really is empty, its count shows 0, and the words open inside the copy (no picture of them was taken). B closes them, Start closes the Tome, and player 2 can walk and open Enchant, twice over (`logs/run-20261103-095529-r6`). Switch off afterwards: player 2's old Tome opened, no `[tome-copy]` lines, but no item was used in it (`logs/run-20261103-095811-r7off`).

**Use, the record fix, the user's play (`logs/run-20261103-100630-hand10`):** after player 1 enchanted the Tulwar, player 2's Talisman healed five times, then said "drained" on the sixth, and B closed it. Player 1's Talisman then healed five times and said "drained" on its sixth, so the counts stay separate. Later Enchants by both worked. The user saw the messages on the right screens. No hang, `rejected=0 failed=0`. The `no second step` guard still hasn't run.

**Test scripts:** the Inventory is a grid, Chakrams and Tome on top, Talisman and Tulwar below. In Enchant, Down from the Chakrams stays put because the Talisman can't be chosen. Right from the Chakrams goes to the Tulwar.

What test 5 found:

- **Enchant beside player 1's:** `coop_quick_cast` saw the flag player 1's cast left in `0x80335748` and didn't see theirs as asked. The flag is cleared for their call, and the record and mode words are put back.
- **Opening beside player 1's** failed with exception 32 (FP unavailable, at `0x80018728`, lr `0x801110B0`). The clean snapshot was taken before the FPU was taken, so the copy told the Tome's start that another thread had it. The snapshot now takes it first.
- **Player 2's Start beside player 1's** was only seen on the Tome's reads, about half of them. It's seen on all.
- **Cast from their Tome** now runs at close with the real record and next-cast word put back after, so it can't touch player 1's Tome.
- **Backing out of Enchant:** B exits its item screen. Start can't: the page skips its Start and tab checks while a spell waits on an item (`0x80103BC4`), by the game's design. The cast leaves player 2 in state 64, and every later cast was refused after a back out. `tome_copy_left` now takes them out of it, as `coop_talk_over` does (`logs/run-20261102-234047-t6b`: back out, walk, back out, cast, all working, and player 1 backing out too). This is probably the older "later quick spell presses failed" bug in `coop-next-session.md`, on the player 2 Tome path only.
- **Moving ends a cast (player 2):** moving calls `0x801C4F48(character)` from `0x8005806C`, which sets `r13-22312` only if that character is the controlled one. In player 2's turn that's player 1, so their cast went on. `coop_cast_cancel` sets their kept word (`g_coop_casting[3]`). Only registered with `ED_COOP_TOME_COPY=2` for now, so `mods`' behaviour is the same with the switch off. `ED_COOP_CAST_TRACE=1` logs who ends a cast (`logs/run-20261102-234747-t7b`).
- **Test scripts:** the cursor starts on the Tome (92). Down goes to the Chakrams (11). Down again goes to the Talisman (10). A copy opened after player 1's cast starts where player 1's cursor was.

Limits of this build, on purpose for now: Quick spell edits come back at close, but a new spell made on the Magick page isn't sent, and its sounds aren't heard (tests 4 and 6). Mix isn't sent.

What test 2 found:

- **Run the Tome frame as guest calls.** `tome_copy_step` runs the way in and each frame with `hle_guest_call`, so no interrupt skip is needed.
- **Way out:** never run the frame once `r13-24244` isn't 3. It goes to the game's way out, which spins at `0x80110DC4`.
- **Disc callbacks:** a host DVD read writes at once to `cpu->ram`, but its callback was queued for the main loop, so it would run in real memory. Callbacks asked for in the copy's turn now run in the copy (`tome_copy_callbacks`).
- **Loader:** a page turn spins on `r13-24276` and `r13-24272`, set by the Tome's chained loader callbacks. They must finish before the frame.
- **Labels:** the tab labels go through `hle_words_layout` (a control byte, `\ay`, control bytes, the word), and are swapped there in the copy's turn. English names only.
- **GX record:** `r13-26688` points at it (`0x80608300` as `GXInit` sets it), `0x4F8` bytes. Copied across at each swap.
- **Page turns in test scripts** need the analog triggers (`l`, `r` in `ED_PAD2_SCRIPT` now set them, in the uncommitted `controls.cpp` test edits).
- **Equip:** `0x800C9FD4` is seen in the copy (`coop_equip_take`) and run in the real game after that turn, with them the player (`tome_copy_equips`), then `0x8007A788`, which the game's way out would do.
- **Use has two stages:** the Tome's call (script, item) leaves its word in the script table and `r13-26324`, then the world runs the script with item 0. Both must run in the real game, or the first use does nothing. `r13-26324` must then be cleared, as `0x800082B0` does (`tome_copy_left`).
- **A thread sleeps once on opening,** at `0x80201A2C` (`OSReceiveMessage`). The way in still returns. Not looked into.

The FPU has to be taken in real memory before the swap. The first version passed those turns over, and that was nearly every turn, so it tested nothing while reporting no failures. Always check the turn count, not just the failure count.

Goal: both players have a Tome up at once, the world keeps running, items stay shared as they are now, player 2's Tome has no Journal/Cinematics or Options tab, and spells (Enchant included) work with either Tome open.

## The idea

**Player 2's Tome runs in its own full copy of the game's memory. Player 1's Tome and the world stay exactly as they are on `mods`.**

The game was built for one Tome with the world stopped. A private copy gives player 2's Tome exactly that: to the game's code, it is a solo game, paused, with player 2 as the player. Player 1's Tome and the world never see it.

The copy costs almost nothing per turn, because the swap is one pointer, not a memory copy.

## Why the last attempt failed and this one shouldn't

`two-tomes-wip` put two Tomes in one memory and tried to list what each one owns: 186 ranges in `tome_state.inc`, plus heap words found by comparing memory, plus exceptions for what must stay shared. Any range missed is a new bug, and any range wrongly kept causes a freeze. That is the loop the old notes describe.

This design removes the list. Each old bug comes from something the two Tomes shared, and none of those things is shared here:

| Old bug (`docs/coop-two-tomes.md`) | What was shared | Here |
|---|---|---|
| Selected item blinks | Display-list counters and cache | Each memory has its own |
| Huge, warped items | Frame side `r13-21824` | Each memory has its own |
| Items vanish when the other turns a page | `8011031C` clears the menu list | Each has its own list |
| Freeze at `80150420`, `8014F218` | List links and heads out of step | Nothing is swapped, so they can't drift |
| Other Tome's items go after an equip | Item slots `0x80334388` | Each has its own slots |
| Only the Mace listed | Not diagnosed. It also happens with one Tome on the branch, and not on `mods` as far as the notes say, so the branch's swapping is the likely cause (**not proven**) | No swapping. Test 3 checks it |
| Flat items, missing key | Vertex buffers at object `+168` | Each has its own buffers |
| 80% speed | 186 ranges and heap copied each turn, and waiting for the renderer | One pointer and one queued message |
| Player 2's Enchant changes player 1's rune | `0x80335748` | Each has its own |

## How it runs

Frames go in turn, at the loop's existing hook (`TW_MODE_CHANGE`, `0x80018728`):

```
World  ->  Player 1's Tome, if open  ->  Player 2's Tome, if open  ->  World
```

The first two are unchanged. The world keeps running because it still gets its frame every round, and nothing here adds a pause.

- **Opening:** copy memory, point `cpu->ram` at the copy, make player 2 the player in it, then set `r13-26188 = 3`. The game's own way into the Tome then runs, as if Start was pressed solo.
- **Each turn:** swap the pointer, run one Tome frame, swap back.
- **Closing:** when the copy asks to leave mode 3, read what the Tome decided, then throw the copy away. The game's way out never runs, so nothing has to be put back.

### Facts this rests on (read in the code)

- Recompiled code reads through `mem_read32(ctx, ea)`, and `resolve_addr` reads `cpu->ram` every time (`.deps/GXRuntime/src/core/cpu.c:102`). So a pointer swap takes effect at once.
- Host-side RAM use goes through `cpu->ram` too (`hle.c`, `dvd.c`, `mmio.c`). The one kept copy found is the renderer's `g_mem1`.
- The renderer reads guest RAM on its own thread, later than the game writes the commands. Its queue is in order (`gxq::Op`), so a new "memory is now X" op switches it at exactly the right frame. The branch instead made the game wait (`dol_aurora_gx_caught_up`).
- Textures are cached with a content hash in the key (`TextureKey`, `gxcore_draw.cpp:299`). The same address with different contents is safe.
- Interrupts and audio are polled from one place on the game thread (`interrupt_poll`, `main.c:372`). Time missed comes out on the next poll, so skipping it for a few milliseconds is normal for this code.

### What the copy must not do

| Thing | Why | Rule |
|---|---|---|
| Interrupts, retraces, audio poll | They change OS thread queues, and would change the copy instead of the game | Skip `interrupt_poll` during player 2's turn |
| Frame-end waits | Nothing can wake them with interrupts skipped | Existing skips (`tw_frame_wait`, `tw_retrace_wait`) also apply to this turn |
| `GXSetDrawDone` note | Would arm a finish no one is waiting on | Ignore it in this turn |
| Co-op hooks that change host state (`g_coop_copies`, `g_coop_kinds_in`, the player's turn in `hle_PADRead`) | Host variables are not copied with memory, so they would go out of step | One gate at hook dispatch: in this turn, only a listed set runs. There are 138 hook registrations to sort, once |
| Way out of the Tome | Not needed | Never runs |

**Must stay shared:** the GX library's own record (`__GXData`, and the FIFO's record if the game keeps one). It mirrors the GPU's registers, and there is only one GPU. Copy it across at each swap. On top of that, the existing fog call runs on entry, as the Tome does today. This is the most likely place for a first picture bug, and test 2 looks for it.

### Opening while player 1's Tome is up

Memory then holds player 1's page objects, so a copy of it isn't a clean world. Fix: as player 1's Tome starts (`tw_tome_start`, while memory is still the world's), keep a snapshot, and player 2 copies from that. Their list can be a few seconds old, which the checks under "What the Tome's choices do" cover.

## What the Tome's choices do

The copy is thrown away, so anything that has to last is sent to the real game. Every one of these calls already works for player 2 in the real game. Only where it gets called from changes.

| Choice in player 2's Tome | Seen in the copy by | In the real game |
|---|---|---|
| Equip | `0x800C9FD4` at `80104070` | Same call, with player 2 and their weapon copies (`coop_rehold`, `coop_restore_held`) |
| Use | `0x801642D4(script, item, 0)` | `coop_run_script` / `coop_use_start`, unchanged |
| Cast, Enchant target | `0x80334DFC`, `0x80335748` read at close | `coop_cast`, with `0x80335748` set only for the call, as `coop-player2-menu.md` asks |
| Quick spells | `0x80334DE8` read at close | Into `g_coop_quick` |
| New spell made on the Magick page | `MAGICK_RECORD` (`0x68` bytes) compared at close | Write only the words they changed, so player 1's edits survive (**page not yet traced**) |
| Sounds | `0x8019DB90(id, volume)` (inferred from where it's called) | Played in the real game, because sound voices live in guest memory and a sound in the copy is never heard |
| Mix | **Not traced yet** | Needs the save already planned in `coop-next-session.md` |

**A tool that finds what's missing:** with a debug switch on, compare the copy at close to how it was at opening, and log every changed word outside the Tome's own memory. Do each action once and the log says exactly what that action changes. That replaces guessing with a measurement.

**Item checks:** before sending a choice, check the item still exists, because the other player may have used it. If it's gone, refuse with the game's own "cannot use" words. A pick-up while player 2's Tome is open first shows on their next opening. A live refresh can come later.

## Enchant

**Today, as read in the code:** the cast writes the shared record `0x80335748`, sets `+16` bit 0 and asks for mode 3. `tw_mode_change` sees the Tome already open and answers "it is open", but the record stays written. That is the documented rune mix-up, and it is where to look for the break when both cast Enchant. **The break itself has not been reproduced or logged.**

**New:**

1. Player 2's Enchant runs in the real game as it does now (`coop_quick_cast`).
2. Right after the call, if it asked for a Tome: take the record's 24 bytes, put the real game's record and mode ask back as they were.
3. Open their copy with those bytes in place, so it opens at the item choice.
4. On choosing, `coop_cast` runs in the real game.

Player 1's Enchant is untouched. Neither can reach the other's record, in any order. If player 2 already has a Tome up, their Enchant waits or is refused, as a solo player can't cast one from inside the Tome.

Other spells don't open a Tome, so they stay as they are.

## Player 2's tabs

From the code, the tabs form a ring with one set of page-start functions:

```
Inventory 80102810 <-> Map 80111B40 <-> Magick 8010AAF0 <-> Journal 80106FD4 <-> Options 80101600 <-> Inventory
```

Only three calls lead into the two tabs player 2 doesn't need. In their turn only, send those elsewhere, using the return-address check the hooks already use:

| From | Call at | Goes to | Player 2 goes to |
|---|---|---|---|
| Inventory | `80103C40` | Options | Magick, or Map before they have magick (`80109A68`) |
| Map | `80112554` | Journal (no magick yet) | Inventory |
| Magick | `8010D8B4` | Journal | Inventory |

Also, in the copy only: clear `r13-24448` so a new journal entry doesn't open the Journal, and set the remembered page `r13-29060` to a page they have. Player 1 keeps all their tabs, since none of this reaches the real game.

This also removes the need for the branch's Load/Save, Cinematics and Display locks for player 2.

**Not checked:** whether a page shows the next tab's name or a tab strip. If it does, that text needs changing too. Look at this on the first run.

## Items, drawing and text, every chapter

No per-chapter list is needed. Item models, buffers and Check text are made by the game's own code from that chapter's data, and the copy holds all of it. The Tome draws them the normal way (`80105F30` > `8011A278` > `8011B814`).

- **Shared carried list:** stays as it is, by `g_coop_tome_p2` in the existing code.
- **Own counts and enchantments:** `coop_kinds` at opening, in the copy.
- **Own weapons:** `coop_equip_names` at opening, in the copy, so the Tome shows the originals. The real copies are only made in the real game.
- **Character names:** `hle_words_layout` rewrites words in memory, so each Tome can show its own player's name without clashing.
- **Chosen characters:** nothing changes. `g_coop_tome_p2` is set only during player 2's turn, as `tw_mode_change` does today.

## Renderer

The branch's renderer changes are worth keeping: two kept pictures, each player's Tome in their half, and `aurora_split_only_view(2)`, which skips drawing the world when both halves are Tomes. The notes record independent cursors and page turns working there in scripted runs. They need porting by hand, since `mods` has moved on (`2e00b69`).

New: one queue op that sets which memory the renderer reads.

## When the game itself changes

Anything that takes the real game out of play closes player 2's Tome with no harm, because the real game never knew about it: a door, cutscene, death, room change, or Load/Save or Cinematics from player 1's Tome.

Quick save needs no refusal any more. It saves the real game, and on load player 2's Tome is simply closed.

## Build and test order

Stop at the first test that fails, and look at why before going on. Each is a small change on its own, and `mods` stays as it is until test 3 passes. Limit of two runs per change, as asked. The user's play decides each one, and frames are compared with `items.ps1` or by eye.

| # | Build | Pass means | If it fails |
|---|---|---|---|
| 1 | Copy, swap, one empty turn per frame, Tome closed. Everything else unchanged | World plays the same, sound clean, no hang | Something else holds the RAM pointer. Find it. No Tome work is at risk yet |
| 2 | Player 2's Tome in the copy, Inventory only, with player 1 walking about | Items are drawn right, and the world keeps running at speed | Most likely `__GXData`. Then the hook gate |
| 3 | Both Tomes up, page turns, tabs removed | Each player's cursor is their own, tabs are right, and repeated open and close never gives "only the Mace" | Compare with player 2's Tome alone |
| 4 | Equip, Use, quick spell page, cast, in that order, with the comparing tool on | Same results as `mods` today | The log shows what the action changes that isn't sent |
| 5 | Enchant in every order: P1 then P2, P2 then P1, both while a Tome is up | Each gets their own rune and item, no break | |
| 6 | Mix, Magick page, sounds; door or cutscene with a Tome up; quick save | As described above | |

**Stop rule:** if test 1 or 2 can't be made to pass in a few focused tries, the idea is wrong for this game. Drop it before any more is built, and go to the fallback.

## Known risks

1. **Something in a Tome frame sleeps or waits on the disc.** With interrupts skipped that is a hang, which the watchdog reports with its address. Normal Tome frames shouldn't. The wait at `80111708` is in a branch that only runs outside the page state (`r13-24244` not 3). Map and Magick may read the disc: **not checked**. Fix if so: finish that read before the swap back.
2. **ARAM isn't copied.** The room packing that writes to it is already blocked (`tw_keep`). Log any ARAM write during this turn.
3. **Speed.** Three frames per round, which no one has measured this way. The branch's slowdown had other causes, and skipping the world's draw should help. Test 2 measures it.
4. **Story scripts on opening the Tome** (`80102A90` on: 480, 1979, 694, 821) would run in the copy and be lost. Player 1 opening their Tome still triggers them. If that leaves a story flag waiting on player 2, it would need sending across.
5. **Long-item hold.** Using a long item (`0x5E6`) holds the other player today. This design doesn't fix that open bug.

## Fallback

A custom player 2 menu (`docs/coop-player2-menu.md`). It's safe but large: every page and the 3D item drawing would be new, and it wouldn't look like the game's Tome. Worth building only if the copy fails tests 1 or 2. Most of this design carries over, since its table of game calls is the same one.

**Not recommended:** going back to `two-tomes-wip` and fixing its open bugs. Two are undiagnosed, and it costs 20% of the speed.
