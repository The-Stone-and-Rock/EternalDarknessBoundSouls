# The looks on offer (ed219, ed220, ed221) with two players

Read-only research, 2026-11-17, on branch tools/game-kit at 88ce3c5. Nothing was built or run. Evidence is a guest
address in build/ed-db/disasm.txt, a line in runtime/host/hle.c, or marked "inferred".

## Short answer

- `ed patterns` lists ed219 (315 places), ed221 (93) and ed220 (85) as uncovered "global" rows. For the usual case,
  where each player stands in their own examine zone, **this is mostly a false alarm from the kit.**
- The game keeps two things here:
  - **The record being set up** at 0x803090C4. It is filled and used up inside one ed22x call, so it can't carry
    anything from one player to the other.
  - **The two looks on offer** at 0x80307108 (68 bytes). The host has given player 2 their own copy since
    2026-11-05 (`PROMPT_LOOKS` and `g_coop_p2_looks`). Every route that runs a zone script for player 2 swaps that copy
    in.
- The kit still says "one copy, never swapped" for both of them (tools/ed/player_data.tsv:49-50). It also flags the
  three calls by name, whatever their route (tools/ed/patterns.py:218-221).
- **There is one real gap: the same thing offered to both players (ed221, kind 3).** This happens in follow mode, or
  in apart mode when both players are in one room. When one player picks the thing up, the other player's PICK UP
  offer stays. The game only stops a *new* ask (ed221's own taken test). It never withdraws an offer that is already
  in the other player's slots and looks. The stepper's PICK UP press then gives the thing again without checking that
  it is already carried.
- **A second, smaller gap is in apart mode: inside player 2's room change (the R4 bracket) the slots and looks are
  player 1's.** The F2a fix (`coop_close_up_kept`) covers it by default. With F2a off, player 1 is left with a dead
  EXAMINE. This is already known and already tested.

## 1. Where the records live and what each one is

| What | Address | Size | Written by | Read by |
|---|---|---|---|---|
| The record being set up | 0x803090C4 (`lis -32719; addi -28476`) | 32: +0 kind, +4 script (r13-23228), +8 count, +12 group, +16 entry, +20 camera hash, +24 n, +28 thing handle | 0x800170D8 (group and entry, +4 script, +8 = 0), 0x800172C8 (+20 camera, +24 n), 0x800174A0 (ed221: +28 handle by 0x801F6160(tag, r13-23276), and kind or entry when taken) | Only the same call's copy into a look: 0x8001720C-0x800172AC (ed220), 0x800173E4-0x80017484 (ed219), 0x8001770C onwards (ed221) |
| Look 0 | 0x80307108 (`lis -32720; addi 28936`) | 32, the same layout | The copy above when the ask 0x80117404 returns slot 2 | The B press 0x80006EBC (sets +64 at 0x80006FF4-0x8000701C), the stepper 0x80024D78 onwards, the cancel 0x80024B24, the let-go 0x80024CDC (sets +64 at 0x80024D1C-0x80024D3C) |
| Look 1 | 0x80307128 | 32 | The same copy when the ask returns slot 4 | The same readers |
| Which look is open | 0x80307148 (+64) | 4 | 0x80006EBC, 0x80024CDC | 0x80024B24 (0x80024B48), 0x80024CA4 (the look's script for a -2 prompt), the stepper |
| Saved in a game save | the whole 68 bytes | | 0x800259E4 copies them out and turns a kind-3 handle into a tag (0x801F6084) | 0x80025A9C copies them back and finds the handle again by tag in **the room in play** (0x80025AE0: r13-23276). Called from 0x800AD644 and 0x800AE2E4 (save) and from 0x800AD7F4, 0x800AD964 and 0x800AE7D0 (load). That these are the save-game block functions is inferred (both return 96). 0x800CDAEC also copies 68 bytes in. |

**Nothing else reads 0x803090C4.** Every `addi rX, rY, -28476` whose rY comes from `lis -32719` is inside
0x800170D8-0x80017770. The one other `-28476` in the file, 0x80161678, is `addi r5, r13, -28476`: an r13 word, not this
record. No load with an offset from -28444 to -28476 off a non-r13/r2 base turns up either. The record is filled and
copied inside one call, and the game runs on one thread, so a call for one player can't leave anything in it for the
other.

**How the ask chooses a look** (0x80017324 ed219, 0x80017154 ed220, 0x80017604 ed221): the ask is
`0x80117404(2, 6, -2, r13-23228, 50, 2, 0)`. A return of 3, below 2, or 5 and above copies nothing. 2 copies to look 0
and 4 copies to look 1 (0x800173C0-0x800173E0). The ask moves a slot-2 ask to slot 4 by the player's zone inside words
(docs/game/systems/examine.md "Slots 2 and 4").

## 2. Who reads the looks

- **The EXAMINE prompt.** It is one of the six prompt slots, 0x80335A38 (28 bytes each: +8 id 6, +12 script -2, +20
  the slot, 2 or 4). The slot says which look a B opens. `coop_prompt_look` uses the same rule (hle.c:19174:
  `index = slot+20 == 2 ? 0 : 1`).
- **The close-up.** The stepper 0x80024D78 counts +8 for each press (DETAIL 34, RETURN 7, PICK UP 19). It pushes
  camera record 10 and runs the zone's script again at the close (0x80025194: `bl 0x801642D4`).
- **The words.** Kind 1 opens a words window at once. Kinds 2 and 3 open it at press 1:
  `0x80024310(element, -2, 0)`, from the look's +12 and +16.
- **The PICK UP's find**, at press 3 of kind 3 (0x80025408-0x8002545C):
  - It reads the look's +28 handle and finds the node with 0x801F5F88.
  - If the node is found, it calls `0x801F9DE8(node, 0x801F62B8() current id, 30)`, the find (hle.c:13401 names
    it `COOP_THING_CARRIED`).
  - **It never checks that the thing is still uncarried.** That check exists only in ed221's own ask:
    0x80017510-0x80017544 tests 0x801F62D0 for 30, 0x801F62D8 for 36, and 0x801F9228 for whether the thing is on
    main's carry list.
- **The host's own readers:**
  - `coop_look_script` (hle.c:20914)
  - `trace_look` (hle.c:11766)
  - `coop_room_change_words` (hle.c:11505)
  - `stay_slot_look` and the F2.5 check (hle.c:14785, 14806)

## 3. How player 2's examine works today (the swap routes)

Player 2's copy is `g_coop_p2_looks[68]` (hle.c:17614-17617). The comment there gives the bug it fixed: "player 1
walking out of a zone emptied player 2's waiting look, and their B opened nothing" (docs/coop-no-pause.md:53).
`ED_COOP_SHARED_LOOKS=1` brings back the single shared copy (hle.c:13539), for comparing. The table lists every place
the 68 bytes are swapped.

| Route | What runs | Looks swapped | Evidence |
|---|---|---|---|
| Step A/B/C, player 2's look: their zone scan and their B press | Their zones' enter scripts (every ed219, ed220 and ed221 is in an enter field, plus one arrival and one cross_a: see below). Their EXAMINE, DETAIL, RETURN and PICK UP through the game's own 0x80006EBC | Yes, together with slots, zones (`coop_zones_swap`), states, cameras (set 2) and words | hle.c:19107-19114 in, 19226-19239 out; the press at 19195-19200 |
| Their words or close-up tick (press 2 after DETAIL's words) | 0x80024AF0's channel step | Yes, while their close-up is up | hle.c:18360-18368, 18388-18392 |
| Their own close-up cancelled (a new room, a scene) | 0x80024B24 | Yes | hle.c:18261-18286 |
| A use of theirs (`coop_use_start`: a door, a zone's use prompt, a Tome item, a close script) | The script as theirs, cameras lent | Yes: `coop_cameras_lend(true)` swaps in their slots and looks, and false swaps them back | hle.c:17971-17981; called at 20496 |
| Any other script run (0x801642D4) whose owner isn't the set in | Timers, a scene's end, the play-start script, arrival scripts run as theirs | Yes, when the lend and the set agree (`prompts`) | `coop_script_cameras`, hle.c:20951-21000 |
| Player 1's prompt set or clear inside player 2's lent close-up | 0x80117404 and 0x80117360 for player 1 | Yes, player 1's copy put in | `coop_prompt_player1s`, hle.c:22637-22666 |
| Player 1's zone scan inside player 2's lent close-up | 0x80134A44 for player 1 | Yes: `coop_cameras_lend(false)` around it | `coop_scan_player1`, hle.c:22914-22920 |
| Player 2's new room's zone list | | Their looks set to zero, and their slots cleared | `coop_zones_new`, hle.c:18446-18449 |
| **R4 bracket (apart: player 2's own room change)** | The arrival's prompt reset 0x801172B0 (lr 0x80152138) leading to 0x80024CDC(-2, slot) and 0x80024B24 | **No. The slots are saved and put back, the looks are left as they are (player 1's)** | hle.c:14767-14776, 14806-14809 |

**The game's own zone look of "another" object** (0x801335C4) never reaches the examine calls for player 2. The host
skips it for player 2's object (`coop_scan_player1`, hle.c:22802-22822, 7ce15a5). In the catalogue, no ed219, ed220
or ed221 sits in an other_* field anyway:

```
cat docs/game/catalogue/level-*.tsv | awk -F'\t' '$7 ~ /^ed2(19|20|21)$/ && $4 ~ /other/'  -> nothing
```

The only calls that are not in an enter field:
- L01/478, room 341's arrival (the dais). It is queued by the change and runs a frame later, outside the bracket, so
  `coop_script_cameras` covers it.
- L01/1298, cross_a of SKPlane01 in room 182.
- L08/2596, a leave of NewExamineRgn in room 202.

**Room in play, apart.** Player 2's turn puts their room in for the whole turn, their look included (hle.c:23702).
So ed221's `0x801F6160(tag, r13-23276)` finds their room's thing.

**Identity.** ed221's gate reads **main**, r13-21484 (0x8001761C: `0x801F6250` then `0x801F6300`, node +40), at
+158 and +159. `coop_player_turn` moves current, not main, so in player 2's look the gate reads player 1's actor.
- +158 == 1 means "a player" (docs/game/systems/players.md:96). That is true for both players.
- What +159 means is not known (docs/game/research/verbs.md:429).
- Harmless unless +159 differs between the two players. Inferred, and an open question.

## 4. Does a zone script run for one player overwrite the other's offer?

**Follow mode, two different zones: no, on every route above.** Each player's ask goes into their own six slots and
their own two looks. The 0x803090C4 record is used up within the call.

Rooms in the catalogue where the players can stand in two different examine zones at once:

| Room | Examine zones (script: call) |
|---|---|
| **L01 room 341** (Jump to Game chapter 1, Pius's start room) | Region02 WALL (2348: ed219, ExamineCam01; box x -1102..-852, y -325..309)<br>BoxRegion01 PIT (2681: ed220; x -177..386, y -392..383)<br>BoxRegion02 TELEPORT (349: ed219; x -222..203, y 621..1041). Both players start inside this one: jtg.sh lines 37-39 put them at (-47,832) and (139,833) |
| **L00 room 69** (Alex's entrance hall) | Five PictureExamine boxes on one script, 857, which chooses by `fn23`<br>SphereRegion01/02 WINDOWS (2330, 2331)<br>Region12 ENTRANCE (792: ed220)<br>GatingRgn01 CLOCK333 (1825)<br>PackageRegion PACKAGE_DELIVERY (698: **ed221**) |
| **L00 room 71** (Edward's study) | MASK (831), ANGKOR_PAINTING (1875), CATH_PAINTING (571), PILLAR_DRAWING (2605), SERVCITY_PAINTING (573), DESK (2334: ed220 over three boxes), GLADIUS (699: **ed221**) |
| **L01 room 183** (the block-holes room) | CameraRgn02-05 (79, 90, 91, 92: ed219)<br>BlockRegion01 MANT_BLOCK (1181: **ed221**, gate `fn30(83) == 0`; box x 215..504, y 195..485) |
| **L02 room 217** | SphereRegion01 NECKLACE_URN (1263: **ed221**, Bronze Necklace, tag 2029; centre (-297,-616), radius 160, +110 50)<br>BoxRegion01 FLOOR_MOTIF (2257: ed219)<br>DoorExamineRgn DOOR_AJAR (2647: ed219) |
| L05 room 255 (13 calls), L10 room 27 (11), L12 room 93 (10) | The rooms with the most examine calls (from the catalogue) |

In these rooms, before 2026-11-05, both players' asks went to the one look 0 or look 1. Whoever came into a zone last
owned the look, and the other player's B opened the wrong close-up or nothing. The 2026-11-05 note is in
docs/coop-no-pause.md:53. Today each player has their own (inferred from the code, not seen in a run for this report).

**Follow mode, the same ed221 zone (the same thing): yes, a real bug.** This is inferred from the code, and a run is
still needed to confirm it.

1. Both players are inside, for example, L02/217's SphereRegion01. Each was given their own PICK UP-type offer as they
   came in: a kind-3 look whose +28 handle is the Bronze Necklace's node.
2. Player 1 goes EXAMINE, DETAIL, words, PICK UP. Press 3 calls `0x801F9DE8(necklace, player 1, 30)`. Press 4 closes
   the close-up and runs 1263 again for player 1, natively. ed221 then sees the necklace as taken (0x80017514-0x80017544).
   Its sixth argument is negative (116 of 124 calls), so it asks nothing: player 1's offer is gone, as in solo.
3. **Player 2's offer is untouched.** It sits in `g_coop_prompt_slots` and `g_coop_p2_looks`, which only their own
   look changes. Their zone inside word still says "inside", so their scan never re-enters the zone and 1263 never runs
   again for them.
4. Player 2 still sees EXAMINE. Their B opens the close-up of the urn, with the necklace no longer on it. DETAIL shows
   the words "A Bronze Necklace sits atop an ancient clay urn.", then PICK UP 19 appears. Press 3 finds the node by its
   handle (it still exists: it is now carried) and calls `0x801F9DE8(necklace, player 2, 30)` again.
5. What that second find does to a thing already carried is not known. It could be a second "found" notice, the item
   floating in player 2's half (`g_coop_p2_find`, hle.c:19271-19277), the thing moved between lists, or a doubled
   count. **This fits the docs/coop-no-pause.md symptom "an item on both screens".**
6. The same happens the other way round: player 2 picks the thing up first and player 1's offer stays.

The other ed221 rooms where this can happen: L00/69 Package (698), L00/71 Gladius (699), L01/183 Granite Block (1181),
and the 93 ed221 places in all (`ed patterns`).

**Apart mode, different rooms:** the two players can't share a thing. Each room's offers are in its own player's set.
What is left:

- **The R4 bracket**, player 2's change (hle.c:14751-14777). The game's slots and looks are player 1's while player 2
  arrives. The arrival's prompt reset runs the let-go 0x80024CDC(-2, slot), and that cancels player 1's open or
  waiting look through 0x80024B24(0).
  - **F2a** (`coop_close_up_kept`, hle.c:21901-21925) skips that let-go when `g_stay_bracket_p1` is set:
    `stay_bracket_p1_allowed()` (hle.c:27821: the split is ready, `ED_SPLIT_HOLD` is not 0, and `ED_STAYER_PRESSES`
    allows player 1) and the cameras are not lent.
  - With F2a off, player 1's EXAMINE comes back with the slots, but its look now has kind 0. Their B does nothing until
    they leave the zone and come back. The host prints `[stay] WARNING: player 1's -2 prompt (slot N) written back over
    look N ...: a dead prompt` (hle.c:14808).
  - Already tested: docs/research/separate-rooms/build-run-stayer-findings.sh, runs S9z (F2a on) and S9z-off
    (`ED_STAYER_PRESSES=0`).
  - With the cameras lent at the ticket (a door run as a use), the slots and looks in memory are player 2's. The reset
    then clears what is *theirs*, which is right. The bracket's write-back restores player 2's slots from the old room,
    and `coop_zones_new` clears them at their next look (hle.c:18446).
- **A game save or load**:
  - It saves only the looks in memory, which are player 1's, because a save is held while player 2's window is up
    (docs/coop-todo.md, step D).
  - On a load, 0x80025A9C finds kind-3 handles again in the room in play, which is right for player 1.
  - Player 2's set is set to zero (step D). Whether their EXAMINE comes back where they stand after a load depends on
    whether their zone inside words are reset too. That is an open question.

## 5. Verdict: is it a real bug in play?

1. **Two offers at once in different zones (follow and apart): not a bug today.** It was fixed on 2026-11-05
   (`PROMPT_LOOKS`). The kit's 493 "uncovered" places are a false positive. Two things cause it:
   - player_data.tsv:50 says 0x80307108 is "one copy, never swapped", which is out of date.
   - patterns.py:218-221 flags every call named ed219, ed220 or ed221 as uncovered, whatever its route.
2. **The same PICK UP offered to both (follow, or apart with both players in one room): likely a real bug.**
   - What the players see: player 1 takes the Bronze Necklace (L02/217) while player 2 stands beside the urn. Player 2
     keeps EXAMINE, gets a close-up of an empty urn with the necklace's words, and a PICK UP that gives it again or
     shows it in their half.
   - Not seen in a log yet. The test in section 7 settles it.
3. **The R4 bracket (apart):** covered by F2a by default. It shows only with `ED_STAYER_PRESSES=0` or
   `ED_SPLIT_HOLD=0`: player 1's EXAMINE goes dead when player 2 goes through a door.
4. **ed221's gate reads player 1's actor in player 2's look:** probably harmless. +158 is "is a player" for both; +159
   is an open question.

## 6. Fix plan (the game's own functions, solo untouched)

**(a) The kit (no game change).**
- tools/ed/player_data.tsv:
  - Line 49 (0x803090C4): its status becomes "per call: filled and copied into a look within one ed220, ed219 or ed221
    call; nothing else reads it (every `lis -32719/addi -28476` is in 0x800170D8-0x80017770)".
  - Line 50 (0x80307108): its status becomes "swapped: PROMPT_LOOKS, g_coop_p2_looks (coop_prompt_look,
    coop_cameras_lend, coop_script_cameras, coop_words_tick, coop_p2_own_cancel, coop_prompt_player1s; reset by
    coop_zones_new)".
- tools/ed/patterns.py:218-221:
  - ed219 and ed220 drop from `look` and are counted as "bracket": all their places are a region's own enter,
    cross or leave fields, or one arrival.
  - ed221 gets its own unit, "the same thing offered to both", uncovered until (b) lands.
  - Add the R4 bracket as a "run-time only" line.
- docs/coop-todo.md:91 and docs/coop-player2-menu.md:28 still say there is one record shared by both: correct them
  to steps B and C. docs/game/systems/examine.md:40 says the 0x803090C4 record is in player 2's set: it is in no swap,
  and it needs none.

**(b) The same thing offered to both: the other player walks out and back in, the game's way.**

- **Hook:** the find, 0x801F9DE8 (`COOP_THING_CARRIED`, hle.c:13401), at its call from the stepper's press 3 (the
  return address 0x80025460). Optionally also every find that takes a placed thing.
  - Gate: `g_settings.mod_coop && g_coop_p2 != 0`. Solo never has a second set, so it is untouched.
- **After the find**, look in the *other* player's two looks:
  - Player 1 found it: `g_coop_p2_looks` (or the game's, when the cameras are lent).
  - Player 2 found it: the looks in memory, or `g_coop_cameras.p1_looks` when lent.
  - Find an entry with kind 3 and +28 == the found node's handle.
- **If that entry's +8 count is 0 (offered, not open):**
  - In that player's set (the same bracket `coop_prompt_look` and `coop_prompt_player1s` build), call the game's zone
    leave, `0x801170D4(zone)`, for the zone whose enter script is the entry's +4 script. It lets go of that zone's
    asks: the slot through 0x80116E70 and 0x80024CDC, and the look through 0x80024B24.
  - Then set that player's inside word for the zone to 0 (`g_coop_zones.p2[i]`, or player 1's in memory, at
    zone + `COOP_ZONE_INSIDE` 104).
  - Their next scan comes into the zone again and runs the zone's ed221. Its own taken test (0x80017510-0x80017544)
    uses "the entry once taken", and when that is negative it asks nothing, exactly as solo's player sees after a
    pickup. Nothing is rebuilt: the game's leave, the game's enter and the game's taken test.
- **If the entry is open (the other player is mid close-up):** at their own press 3, before 0x801F9DE8, run ed221's
  own taken test on the node (0x801F62D0 == 30, 0x801F62D8 == 36, 0x801F9228(main's list, node)). If it is taken, skip
  the find and go on to press 4 (close, run the script again). The script run then withdraws the offer by itself.
- **Checks first** (docs/coop-failed-attempts.md: probe a maker's calls before hooking it):
  - Probe 0x801F9DE8's callers. The find from the stepper and a script-made find (`FOUND_AFTER_RAN`,
    `FOUND_CLOSE_RAN`) may differ.
  - Confirm that 0x801170D4's r3 is the zone record (it reads +108, +58..+64: the zone's scripts).

**(c) The R4 bracket.** Leave F2a as it is. A wider fix would put player 2's slots and looks in for the bracket,
copying how `coop_cameras_lend` swaps. Then the arrival's reset would clear the arriver's own, as solo does. That is
the user's call, because design-stayer-findings.md:102 left it gated on purpose.

## 7. How to test it (tools/ed/jtg.sh conventions)

All runs below are cold Jump to Game. Use `ED_COOP_LOG=1 ED_COOP_TRACE=1`. Read `ed timeline`, EVENTS.log and
page-0001.log first. The frames come from jtg.sh's header and are relative to `JTG_BASE`.

**T1: two offers at once, follow (a check that the 11-05 fix still holds; expected to pass today).** Chapter 1, room 341.

```
ED_COOP_LOG=1 ED_COOP_TRACE=1 JTG_FRAMES=900 \
  JTG_PUT="30,1,-977,0,0;30,2,100,0,0" \
  JTG_P2_SCRIPT="120,126,b;220,226,b" \
  JTG_P1_SCRIPT="130,136,0x0200;230,236,0x0200;330,336,0x0200;430,436,0x0200" \
  sh tools/ed/jtg.sh 1 looks-two-zones
```

- What it does: player 1 in Region02 WALL (ed219, kind 2), player 2 in BoxRegion01 PIT (ed220, kind 1). Player 2 presses
  B for their words, then B again to close them. Player 1 goes EXAMINE, DETAIL, closes the words, then RETURN.
- Expect:
  - `[coop] player 2's B on their EXAMINE (plain words, look 0)` with the PIT words in player 2's half.
  - Player 1's close-up on ExamineCam01 with the WALL words.
  - No `[stay] WARNING` and no new saves/reports/hang-*.txt.
- Baseline: the same with `ED_COOP_SHARED_LOOKS=1`. The second asker owns look 0, so one player's B opens the other
  player's look, or nothing. `ed see` at the frame of each B.
- Variant T1b, two kind-2 looks at once: no `JTG_PUT` for player 2, who stays in BoxRegion02 TELEPORT. Expect
  `[coop] player 2's own close-up: press id 6 on look N (kind 2 ...)` beside player 1's close-up.

**T2: the same thing offered to both (expected to show the bug today).** Level 02, room 217. `ed catalogue --script
L02/1263` gives the chapter; pass that chapter's number to jtg.sh.

```
ED_COOP_LOG=1 ED_COOP_TRACE=1 JTG_FRAMES=1500 ED_RUN_SCRIPT=<base+60>:747 \
  JTG_PUT="200,1,-297,-616,0;200,2,-250,-580,0" \
  JTG_P1_SCRIPT="260,266,0x0200;330,336,0x0200;400,406,0x0200;470,476,0x0200;540,546,0x0200" \
  JTG_P2_SCRIPT="700,706,b;770,776,b;840,846,b;910,916,b;980,986,b" \
  sh tools/ed/jtg.sh <chapter> looks-same-thing
```

- What it does: door L02/747 takes both players from room 295's ExitRgn05 into 217. Both are put inside SphereRegion01
  (radius 160). Player 1 goes EXAMINE, DETAIL, words, PICK UP, B. Then player 2 does the same.
- Bug today:
  - After player 1's find, player 2's slots still hold id 6 with script -2: the `[trace]` slot lines from hle.c:19341
    onwards.
  - Their B gives `player 2's own close-up: press id 6 ... kind 3`.
  - Their PICK UP gives `[coop] player 2's own find open in their copy: ... thing <the necklace's id>`: a second find
    of a carried thing.
  - The items in both.tsv, and `ed see` at the second find's frame.
- After fix (b):
  - A log line for the withdrawn offer.
  - No second find. Player 2's EXAMINE is gone within a frame or two of player 1's PICK UP.
  - Solo (`JTG_SET=mod_coop=0`) is unchanged.
- Variants:
  - T2r: player 2 picks up first, then player 1. Swap the two scripts.
  - T2o: player 2 presses EXAMINE and DETAIL *before* player 1's PICK UP, for the "open" branch of (b).

**T3: the R4 bracket (apart).** This already exists: docs/research/separate-rooms/build-run-stayer-findings.sh S9z and
S9z-off (`ED_ROOM_SLOTS=2 ED_COOP_STAY=1`, chapter 1 room 183, player 1 in a close-up zone while player 2 takes door
L01/119).
- Expect: S9z prints F2a's `passes over the let-go` and no WARNING. S9z-off prints the WARNING, and the B at 3900 opens
  nothing.
- Run it again after any change to the bracket.

## 8. Open questions

1. What does `0x801F9DE8` do with a thing that is already carried? It could move it from player 1's list, add it a
   second time, or only show it. Only T2 can show this. Also add what is seen to docs/items-log.md.
2. Can two player bodies both stand inside a 160-radius sphere? Do the players collide with each other? If they can't,
   T2 needs a bigger ed221 zone: L01/183's BlockRegion01 box (289 by 290) is one.
3. Is `0x801170D4`'s r3 the zone record, and does calling it for the other player also clear a marked state (fn35)
   that the zone's script relies on? A leave in solo does the same, so this is inferred safe.
4. What does actor +159 mean, and does it ever differ between the two players? If it does, ed221's gate reads player
   1's in player 2's look (main isn't moved).
5. After a load, does player 2 standing in an examine zone get their EXAMINE back, or do their zone inside words keep
   "inside" while their looks are set to zero (step D)?
6. 0x800CDAEC copies 68 bytes into the looks from another buffer. Which path is that, and could it run for player 2's
   room apart? Not followed.
7. A camera's arrival script that runs inside `coop_camera_step` (cameras set 2 in, not lent, owner player 1) gets no
   looks swap from `coop_script_cameras` (`prompts` is false). No examine call sits in a camera script in the
   catalogue, so this is inferred harmless.

## Skeptic's check

Read-only, 2026-11-17. I checked the report's claims against build/ed-db/disasm.txt, runtime/host/hle.c, the kit and the
docs. 14 spot checks are below. "Read" means I looked at the code myself. "Inferred" means I didn't.

### Confirmed

1. **The record being set up is used only inside one call.** Every `addi rX, rY, -28476` is between 0x800170E8 and
   0x80017768. The only other one is 0x80161678, which is `addi r5, r13, -28476`. The addresses work out:
   0x8031 << 16 - 0x6F3C = 0x803090C4, and 0x8030 << 16 + 0x7108 = 0x80307108. (read)
2. **How the ask picks a look.** In ed219, 0x800173C0-0x800173E0: a return of 3 copies nothing, 2 copies to look 0
   (0x800173E4-0x80017430), 4 copies to look 1 (0x80017438-0x80017484), anything else nothing. ed221 is the same at
   0x800176E8-0x800177AC. (read)
3. **The `ed patterns` counts.** ed219 uncovered at 315 of 315, ed221 at 93 of 93, ed220 at 85 of 85 (run today).
4. **The kit's stale rows.** tools/ed/player_data.tsv:49-50 both say "none: one copy, never swapped". tools/ed/patterns.py:216-221
   flags ed219, ed220 and ed221 by name (`look = name in ('ed219', 'ed220', 'ed221')`). One thing to add: the tsv gives
   0x80307108 a size of **64**. The look index at +64 is outside that, so the corrected row should say 68
   (`PROMPT_LOOKS_SIZE`, hle.c:17615).
5. **Press 3 of the kind-3 stepper doesn't test whether the thing is carried.** At 0x80025408-0x8002545C:
   0x801F5F88(look+28), then 0x801F9DE8(node, 0x801F62B8(), 30), with no carried test. (read)
6. **ed221's taken test.** It runs at 0x80017510-0x80017544. The thing counts as taken if node+8 == 30 and node+20 == 36
   (0x801F62D0 and 0x801F62D8 are plain loads of +8 and +20), or if 0x801F9228(main+48, node) is nonzero. main+48 comes
   from 0x801F63A0. When the thing is taken:
   - kind becomes 2 or 1, +28 becomes 0, and the entry comes from argument 6 (0x80017548-0x8001759C);
   - 0x800176C0 then asks nothing if that entry is below 0. That is 116 of the 124 calls (docs/game/research/verbs.md:43, 168).
   (read)
7. **Player 2's turn leaves main alone.** `coop_player_turn` writes only r13-26524, -21492 and -21496 (hle.c:23570 onwards).
   So in player 2's zone scan, ed221's gate and its carried test read player 1's node. In follow mode that is the right
   list, because `hle_coop_found` puts every find on player 1's list (docs/game/systems/pickups.md, "For two players"). (read)
8. **The host lines.** All of these say what the report says:
   - `PROMPT_LOOKS`, `g_coop_p2_looks`: hle.c:17610-17617
   - `ED_COOP_SHARED_LOOKS`: 13539
   - the index rule: 19174
   - `coop_zones_new` clearing the looks: 18446-18447
   - the R4 bracket: it saves the slots and only notes the looks' kinds (14767-14776); the WARNING is at 14808
   - `stay_bracket_p1_allowed`: 27821
   - F2a: 21902-21925 (read)
9. **The look index.** The B press writes +64 at 0x8000701C. The let-go writes it at 0x80024D1C-0x80024D3C. The cancel
   reads it at 0x80024B48. (read)

### Wrong or imprecise, with corrections

1. **Open question 1 is mostly answered already, and the symptom is described wrongly.**
   - The second 0x801F9DE8 does nothing. It returns at once when node+8 already equals its r5 (30): 0x801F9E14-0x801F9E1C
     (read). docs/coop-level3-design.md:935 and :1012 record the same thing ("exits: type already 30, no 240").
   - So "a second find of a carried thing" through 0x801F9DE8 is not the harm. The harm is the rest of press 3, which
     runs anyway:
     - the "has found" element, 0x801DB374 at 0x800254CC, in player 2's half;
     - the find itself, 0x80024528 at 0x800254F0.
   - Inside that find (docs/coop-level3-design.md:561 and :953):
     - `hle_coop_found` tells player 1 "X has found Y";
     - the thing is moved again to player 1's list (0x801F95A0);
     - event 71 to player 1 puts the carried thing back on show (the kind-30 handler, 71 to state 35);
     - the thing is placed in front of the camera again;
     - when the find closes, event 190 runs the item's +36 "after" script a **second time** (0x800D9B64).
   - What the player sees: a second "has found" and the item floating in player 2's half. Some items would also run their
     after-pickup script twice. (inferred from the read find code; not run)
2. **The close's script run for kind 3 is 0x8002558C, not 0x80025194.** 0x80025194 is kind 2's close
   (CLOSE_UP_END_RAN 0x80025198). Kind 3's press 4 calls 0x801642D4(look+4, 0, 0) at 0x8002558C, return address
   0x80025590. Press 4 is reached from the find window's close handler (0x800244D0 calls 0x80024D78 with -2), which runs
   inside player 2's tick. (read: both `bl 0x801642D4`)
3. **The report says main is never moved. That is only true of the turn.** `coop_be_player2` (hle.c:12739-12741) also
   writes -21488 and -21484 (main). It is used by:
   - the R4 bracket (14772)
   - `coop_use_start` (20473)
   - their words and close-up tick (18369)
   - their own cancel (18268)
   - `coop_script_cameras` (21021)

   In any of those, ed221's gate and its carried test read **player 2's** node+48 list, which never holds the thing. Step C
   already hit this for press 4's script run and fixed it by putting main back to player 1's for that run
   (docs/coop-level3-design.md:1015, C5). **For the fix plan:** any ed221 run for player 2 has to have main as player
   1's. The zone scan inside the turn already does. A host call made in a `coop_be_player2` bracket would not.
4. **What 0x801170D4 does (it answers open question 3 and changes plan (b)).** It is the scan's zone leave (its one caller
   is 0x801349C0). Besides the let-go, it does three things:
   - **(a)** It calls 0x80163E84 on each of the zone's three scripts (+58..+64, chosen by +108). That clears the script's
     8-byte entry in SCRIPT_STATES, 0x805CAA40, when its +6 is 0 (0x80163E98-0x80163EE0). That table is per player
     (`coop_states_swap`). So the other player's states have to be swapped in, or player 1's entry for that script is
     wiped.
   - **(b)** If slot 2's look script matches the zone, it lets go of **both slots 2 and 4** (0x80117194-0x801171A8, two
     0x80116E70 calls). It also clears slot 6 when the shown prompt is slot 2 or 4 (0x801171B4-0x801171CC). So if the
     other player has a second offer from another zone in slot 4, that offer is dropped too. Only one zone's inside word
     is reset, so nothing asks again for the other zone until the player walks out and back in.
   - **(c)** Its let-go goes through the host's intercept on 0x80024CDC, `coop_close_up_kept` (hle.c:21902, hooked at
     33648). The tail of that function returns early unless the screen is split, r3 is -2, and r13-21492 is not player 2.
     Whether the cancel runs inside the bracket depends on those tests.

   (all read)
5. **The hook point.** The plan says to hook 0x801F9DE8 at return address 0x80025460. That catches only the stepper's
   PICK UP:
   - 0x801F9DE8 is also called inside the find at 0x80024660, by the host's own enchant give-back (hle.c:13426), and by
     `fn90` from scripts;
   - a thing taken by `ed52` from a script (one of the find's five callers, docs/game/systems/pickups.md) would leave the
     other player's ed221 offer standing just the same.

   Better: do it in `hle_coop_found` (hle.c:20383). It is the host's existing notify at the entry of 0x80024528, it runs
   for all five callers, and it already knows the finder. Read the thing's handle there (r6) and compare it with the
   other player's looks' +28.
6. **The test's door.** In T2, door L02/747's lines 5 and 7 only run when `(fn30(734) ~= 0) or (fn30(292) ~= 1)`
   (docs/game/catalogue/level-02.tsv, room 295). If a cold Jump to Game state doesn't have those flags, the
   `ED_RUN_SCRIPT` teleport does nothing. T2 also leaves the chapter as `<chapter>`. Room 183's Granite Block (L01/1181,
   gated on `fn30(83) == 0`) is the better first target:
   - `saves/jtg-ch1-room183.state` exists and step C was checked on it;
   - its box is 289 by 290, which answers open question 2;
   - no door is needed.
7. **The user's scope.** docs/coop-todo.md:78 says: "each player can use or examine a different thing while the other
   uses or examines theirs. The same thing by both, no."
   - The "offered" branch of (b), a stale offer after the other player's pickup, is still a bug.
   - The "open" branch (both in a close-up of the same thing) is outside the ask. There, the plan's "skip the find at
     press 3" would also have to skip the has-found element and 0x80024528 (0x80025408-0x800254F4), or the player sees
     it anyway.
   - A simpler choice is the game's own cancel of that look, 0x80024B24, with that player's set in. Or leave it as known
     and out of scope.

### Missed, and what it changes in the fix plan

- **Hook:** `hle_coop_found`, not 0x801F9DE8 at 0x80025460. It covers the stepper, `ed52` and the other find sites.
- **The leave call needs the other player's whole set in:**
  - slots (`g_coop_prompt_slots`), looks, zones (`coop_zones_swap`) and **script states** (`coop_states_swap`), because
    0x801170D4 clears SCRIPT_STATES entries;
  - **main left as player 1's**, so that the next ed221 run's carried test sees the one list.

  The turn's look route (hle.c:19100-19114) already makes exactly this set. Running the leave from the next look for that
  player is safer than calling it from inside the finder's bracket.
- **Slot 4 can be dropped with slot 2.** If both slots were filled (two zones), reset the inside word of **every** zone
  whose script owns a cleared slot, so that both ask again. Or check slot 4's script before the call.
- **The second find runs the item's after script again** (event 190). That makes the bug worse than a doubled display:
  record in docs/items-log.md what T2 shows.
- **Test:** put T2 on `saves/jtg-ch1-room183.state` (`sh tools/ed/run.sh saves/jtg-ch1-room183.state looks-same-thing`
  with `ED_PUT` / `ED_INPUT_SCRIPT` / `ED_PAD2_SCRIPT`, or the jtg form for chapter 1 with player 1 walked to room 183).
  Both players inside BlockRegion01, player 1 picks up the block, then player 2 presses B. Look for:
  - a second `[coop] ... has found` notice;
  - a second event 190 / FOUND_AFTER_RAN script run;
  - player 2's slot still id 6 / -2 in the `[trace]` slot lines.

### Verdict

- The bug is real by the code: a stale kind-3 offer after the other player's pickup, in follow mode or in apart mode with
  both players in one room. It is not yet seen in a run.
- The symptom is not a second 0x801F9DE8, which does nothing. It is a second has-found display and find: the item shown
  again, player 1 told again, and the after script run again.
- The plan's idea is sound: the game's own leave, then re-entry, then ed221's own taken test. It needs four changes:
  - hook `hle_coop_found`;
  - run the leave with the other player's whole set in, script states included, and main as player 1's;
  - handle the drop of slot 4 together with slot 2;
  - test first on room 183's block.
- Part (a), the kit's correction, is right. Part (c), F2a, is right as it stands.
