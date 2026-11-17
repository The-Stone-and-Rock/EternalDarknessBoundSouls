# The two-path hero gates: what each does, and what player 2 should take

Research only (2026-11-17). Nothing built or run. Sources: `ed herogates --all`, the decompiled scripts in
build/ed-db/scripts/LNN/ID.lua (line numbers below are from those files), `ed script`, `ed catalogue --script`,
`ed room`, `ed door`, `ed check --dry`, and the disassembly in build/ed-db/disasm.txt.

A hero gate is `L0 = fn78(1, 0)` (the player's handle, r13-21496), then `if L0 ~= 1`. The switch ED_COOP_HERO_ID=1
(hle.c coop_hero_chunk, table kHeroGates at hle.c:20629) opens only the 31 gates that turn the player away. The 21
two-path compares below are not in that table. So in co-op today **player 2 always takes the "other" branch** of
each, switch on or off.

## The main finding: in solo, "someone else" is a summoned creature

In solo, fn78(1, 0) is not always 1. When Alex or Edward casts a Summon and takes control of the creature, the
player's handle becomes the creature's (docs/coop-glitch-todo.md E8: "player 1 75, now 28"). L12/2778 even calls
the handle `SummonedMonster` (line 61). So most "other" branches are **the summon's path**. Several of them end the
summon: `fn79(handle, 135, 0)`. Event 135 to a summon ends it (docs/game/research/transitions.md:372, kind 50's
handler). Player 2 is a person, not a summon. That is the question for each gate: should player 2 act like the
chapter's character, or like a summoned creature?

Two of the 21 are **not hero gates** at all (L12/2778, L09/L16 2026). The herogates tracker keeps a slot after a
later `local Lk = call()` puts a different value there. Those two compare a model and an alignment. Details at
the end.

## Summary

| Script (compares) | Where | Character's path / other path | Recommendation | `ed check --dry` |
|---|---|---|---|---|
| L00/1081 (line 3) | Mansion, level 00, Alex. Room 78, ExtiRgn01, the walk into room 77 | Ghost maid scene on arrival (script 583) / plain arrival (camera 1169) | **Ask.** Lean yes when ED_COOP_HERO_ID is on, to match L00/583, which that switch already opens | No route: level 00 needs a new game |
| L07/1335, L10/1335 (line 7) | Peter's chapter (level 10). Room 11, BoxRegion01, the door to room 27 | Sound 366 before the door, when the window scene is armed / the door only | **Yes, if 1593 is opened.** Harmless either way | Plan made. Needs `--flag 160 --flag 161` |
| L10/1593 (line 5); copies in L12, L13 unused | Peter's chapter. Room 27, CameraRgn14 | The exploding window scene (897) / Camera15 only | **Yes, low priority.** A one-time scare. No progress depends on it | Plan made. Needs `--flag 160 --flag 161` |
| L09/858, L12/858, L13/858 (line 26) | The array rooms' return pads. Edward: rooms 269, 284-291. Mansion: 37, 261-268. L13: 267 | Flag 36 off: the pad goes dark / flag 201 on: the pad stays lit, and the next arrival skips its fade-in | **Ask.** Lean: the character's path when together; today's path when apart | No route: no door into those rooms in the tables |
| 1010, first compare (line 87), L09/L12/L13 | The hub teleporter. Edward: room 274. Mansion: room 93, TeleportRgn05 | Clears the chosen button (34, 657 off, ArrayToGoTo 0) / the button stays armed | **Ask.** Same lean as 858 | Plan made via 274/93, but a button must be pressed first. ArrayToGoTo is a Lua global, not a flag |
| 1010, second compare (line 152) | Room 93 with flag 37 on: the game's end | The ending films and chapter_start(12, 13, 1) / the summon's end: model off, events 57 and 135 | **Yes (or turn player 2 away). Never today's path** | L12 plan made. Needs `--flag 37` |
| L12/2792 (lines 10, 57, 85) | Mansion, room 94. Started by the game's code, not a region | Camera on Alex, a barrier, thing 911 hidden / a ScareCam, a flash, event 135, flag 698 off | **No change needed.** The game runs it as player 1's. Yes if it ever runs as player 2's | No route: not a region's script |
| L12/2778 (line 87) | Mansion, room 92, the three pads | Not a hero gate: line 87 compares a creature's model with 1 | Nothing to decide | Plan made (needs flag 619 off) |
| L09/2026, L16/2026 (line 74) | Edward's chapter, arrival in room 237 | Not a hero gate: line 74 compares the alignment with 1 | Nothing to decide | No route: an arrival script |

Count: 1 + 2 + 3 + 3 + 3 + 3 + 3 + 1 + 2 = 21 compares.

---

## L00/1081: the ghost maid on the way into room 77

- **Where:** level 00 (Alex in the mansion, before flag 787). Room 78, region ExtiRgn01 (enter). A walk-through
  door, transition 2, to room 77 at EntryMkr02, camera Camera09 (`ed door 77`). Line 1 queues room 77. Level 12's
  catalogue lists the same region, but level 12 has no script 1081, so it runs nothing there.
- **The gate:** line 3, `if L0 ~= 1 then goto L48`.
- **Alex's path (lines 4-9):** if flag 751 is off (the maid not seen yet) and flag 314 is on, line 7 sends event 82
  to the sanity tracker (`fn78(3, 32)`, the one kind-32 thing). Line 8 goes to room 77 with **script 583** after it.
  583 is the ghost maid scene: VampireCam02, the maid spawned at VampireSpawnMkr03, stream 84, its subtitles, then
  flag 751 on (583 line 49). Flag 314 is set by L00/1580 line 6, the Tome page that starts Peter's chapter (1580
  line 19). Otherwise (lines 10-11) the same door with script 1169.
- **Other path (lines 13-14):** the same door, then 1169: Camera09 and an ease (1169 lines 1-2). A summon walking
  through doesn't start the scene.
- **Player 2 today:** the plain arrival. Only player 1 can start the scene.
- **Note:** L00/583 has its own gate (`{0, 583, 0x1AB}` in kHeroGates). With ED_COOP_HERO_ID=1 it already opens for
  player 2 when they walk into ServantGhostRgn. So today, with the switch on, player 2 gets the maid one way (the
  region) and not the other (this door).
- **Recommendation: ask; lean yes, with the switch.** It is a one-time scare and no progress depends on flag 751.
  Opening it makes the two ways in agree. The cost is the subtitle problem already written up: a scene of player
  2's puts its lines in player 1's half (docs/research/coop-gaps/subtitles.md, "The ghost maid").
- **`ed check`:** none. "level 00 is no Jump to Game chapter ... needs a new game".

## 1335 (L07, L10): a sound before the door to room 27

- **Where:** Peter's chapter (level 10). Room 11, BoxRegion01 (enter), a door (transition 3) to room 27 at
  EntryMkr06, camera Camera13, or to room 345 when flag 471 is on (lines 2, 12-14). L07 (Paul) has the same file and
  room. But flags 160 and 161 are set only by level 10's scripts (L10/1330, 1334, 2771), so L07's gate is never
  reached.
- **The gate:** line 7, behind flags 160 on, 161 on, 643 off (lines 4-6): the window scene is armed and hasn't
  played.
- **Peter's path:** line 8, sound 366 at volume 127, then the door (line 10).
- **Other path:** the door only (line 10).
- **Player 2 today:** no sound. The door works.
- **Recommendation: yes if 1593 is opened, otherwise leave it.** It is only a sound cue that leads into the window
  scene. Harmless either way.
- **`ed check L10/1335 --dry`:** a plan. Room 11 BoxRegion01, chapter "9 (Peter)", door in L10/1243, out L10/1336.
  The script tests flags 160, 161, 471, 643, so run with `--flag 160 --flag 161`. L07/1335 also plans, but its gate
  needs Peter's flags.

## L10/1593: the exploding window in room 27

- **Where:** Peter's chapter, room 27 (the chapter's start room), CameraRgn14 (enter). L12 and L13 have the same
  file, but no room in those levels runs 1593 (it is missing from their catalogues).
- **The gate:** line 5, behind flags 160 on, 161 on, 643 off (lines 2-4).
- **Peter's path (lines 6-12):** if no insanity effect is running (`ed61`), and flag 754 is off, line 8 turns 754
  on and line 9 runs **script 897** next. 897 is the window scene: menu and quick cast denied, ExplodingWindowCam,
  sound 365, the storm, events 18 to things 640-642, stream 493, then Camera15, flag 643 on and 754 off (897 lines
  33-36). With Reveal Invisible running at size 3 it also runs 1598 (897 line 40). If an insanity effect is running,
  line 12 sets Camera15 only.
- **Other path (line 15):** Camera15 only.
- **Player 2 today:** Camera15 in their view. The scene waits for player 1.
- **What depends on it:** flag 643 is read by 1335 (the sound), L10/1814 line 104 (a stream and an event on room
  arrival) and L10/2532 / 2638 (flag 922). 1598 also runs from L10/1814 line 122 and L10/2238 line 36. No door or
  item waits on 643.
- **Recommendation: yes, low priority.** It matches the story: whoever walks in sees the window go. Apart, it would
  be a scene of player 2's in their half. Once played (643 on), player 1 never sees it, as in solo. Harmless if left.
- **`ed check L10/1593 --dry`:** a plan. Room 27 CameraRgn14, no door in (the start room), out L10/1238 to room 123.
  Run with `--flag 160 --flag 161` (643 and 754 off). The handoff already names this as a reachable check.

## 858 (L09, L12, L13): the return pad in each array room

**How the teleporters work.** A hub room has nine buttons around a central pad: room 93 in the mansion, room 274 in
Edward's chapter. A button (L12/871-879, L09/1001-1009) needs flags 34 and 37 off and the player's subclass 1
(871 lines 6-8). It sets flags 34, 24, 657 and the global ArrayToGoTo (871 lines 26-29). The hub pad (1010) then
sends the player to one of nine array rooms. Each array room's return pad (858) works while flag 36 is on (858
line 4). Flag 36 is set when the room's task is done (L12/2216 lines 205, 269, 288; L12/2204 line 161). The pad
sends the player to a ring room (line 209). Mansion: 37→90, 261→91, 262→92, 263→94, 264→98, 265→99, 266→97,
267→102, 268→136. Edward: 269→271, 284→272, 285→273, 286→275, 287→277, 288→278, 289→276, 290→281, 291→282.

- **The gate:** line 26, after the walk to EntryMkr01 (lines 13-16) and the fade-out (line 25).
- **Character's path:** line 27, flag 36 off. The pad goes dark, and the next array room starts unlit.
- **Other path (the summon's):** line 30, flag 201 on. Flag 36 stays on. In solo this lets a summon go back
  through the pad while Alex can still use it. Flag 201 is read in three places:
  - L12/2778 (room 92, reached from 262's pad) uses it to end the summon on the pads (2778 lines 74, 92, 110, 258).
  - The array room's arrival L12/2782 line 28 runs 449 when 201 is on.
  - 449 line 18 then **skips the fade-in**. The fade-in path is the one that turns the model back on (449 lines
    20-31, `fn88` at line 26). The 201 path (lines 33-35) only clears 201. 1010 turns the traveller's model off
    before the trip (1010 line 32).
- **Player 2 today:** the summon's path. Flag 36 stays on and 201 goes on.
  - Apart, that helps: player 1, still in the array room, can use the pad.
  - Together (one door takes both, `coop_follow`), player 1 never runs 858. Flag 36 then stays on into the next
    array room, so its pad may be lit before its task is done.
  - A stale 201 may make the next array-room arrival skip the fade-in. That could leave the arriving player's model
    off (inferred from the order above, not run). Solo has the same order after a summon uses a pad, so something
    not read here may cover it.
- **If player 2 took the character's path:** apart, flag 36 goes off and **player 1 is stuck in the array room**.
  858 line 4 refuses them until the room's task sets 36 again. That would block co-op progress.
- **Recommendation: ask.** Lean: the character's path when the players are together (both travel through player 2's
  pad), today's path when apart (player 1 still needs the pad). kHeroGates opens a gate for every run of player 2's.
  A together-only rule would be a new condition in coop_hero_chunk.
- **`ed check --dry`:** no route. "no door script into room 37 in level 09's tables". Rooms 269 (L09) and 262 (L12)
  give the same answer. L13 is not a Jump to Game chapter. A test needs a save already in an array room with flag
  36 on.

## 1010 (L09, L12, L13): the hub pad

The same file in three levels. It runs from TeleportRgn05 in room 93 (mansion) and room 274 (Edward). In L13 it
runs only as the end script of the ending films and cinematics (level-09 catalogue lines 3335-3348), at states the
gates don't reach.

### First compare, line 87: who uses up the chosen button

- **Where:** state 2 of the normal trip (flag 37 off, ArrayToGoTo set: lines 3-4). The walk to TeleportMkr1 (lines
  6-20), the fade (lines 24-28), the model hidden (lines 32-33), the destination picked (lines 34-84), then line 87.
  Both paths then travel: `fn66(L1, EntryMkr01, Camera01, 2, 449)` at line 96. In room 93, the "room visited"
  flags 740-745, 693, 695, 696 are set on both paths (lines 45-84).
- **Character's path (lines 88-90):** flags 34 and 657 off, ArrayToGoTo = 0. The button and the pad go dark. A new
  button can be pressed.
- **Other path:** all three stay set. In solo that is the summon going through while Alex can still follow.
- **Player 2 today:** the other path.
  - Apart, player 1 can follow to the same room.
  - Together, both travel and the button stays armed. Flag 34 on makes every button refuse (871 line 6), and the
    hub's arrival relights the glow (L12/2196 lines 23-31). To choose a new room, player 1 must take the pad once
    as the character. That is a nuisance, not a block.
- **If player 2 took the character's path:** apart, player 1 must press the button again. Also not a block.
- **Recommendation: ask; the same lean as 858** (the character's path together, today's apart). Keep 858 and 1010
  on the same rule, or the two pads disagree.
- **`ed check --dry`:** `ed check 274:TeleportRgn05 --level 09 --dry` and `ed check L12/1010 --dry` make plans (put
  at -766,926,1915, door in L09/2204 / L12/2204). But this compare needs ArrayToGoTo, which only a button press sets
  (a Lua global, not a flag). The run would have to step on a button region first (room 93's ButtonRgn01 at
  -348,337,1773).

### Second compare, line 152: the game's end

- **Where:** room 93 with flag 37 on (line 3). Flag 37 is set when the hub's rune slots are right (L12/2216 line
  194). In room 274 the hub's arrival sets flag 659 (L09/2196 line 2), and line 100 then skips all of this. So in
  practice this is the mansion only (L12/2196 line 7 clears 659 in room 93).
- **State 0 (anyone):** play held (`fn21(0)`, line 102), the walk to TeleportMkr1, events 57 to things 2102, 2100,
  2101, 2001, 337, 338, 516 if the player carries them (lines 119-145).
- **The gate:** state 1, after the fade (line 150).
- **Character's path (state 2 onward, lines 153-265):** model hidden, a black fade, flag 657 off, then by alignment
  cinematics 131/132/87, films 34-36, films 30-32, cinematics 165/166/156, films 31/32/30. Then the spell slots are
  cleared and `chapter_start(12, 13, 1)` (line 265) starts the last chapter (level 13).
- **Other path (state 11, lines 268-274):** model off and held items hidden, play let go (`fn21(1)`), then event 57
  (delay 1) and event 135 to the player's own handle. This ends a summon that walked onto the pad.
- **Player 2 today:** state 11. Their model is turned off with nothing to turn it back on. Events 57 and 135 go to a
  player body, which was never meant to get them (what they do to a player body is not traced). The ending never
  starts. This is a likely real bug: player 2 left invisible on the hub pad at the end.
- **Recommendation: yes** (player 2 starts the ending, as Alex would; it is full-screen films for both). If the user
  wants only player 1 to start the end, the other choice is to turn player 2 away before state 1. Today's path is
  wrong either way.
- **`ed check L12/1010 --dry`:** a plan (room 93 TeleportRgn05, chapter 12, out L12/650 to room 100). Run with
  `--flag 37`. Expect the films to take the run past the default `--wait 1200`.

## L12/2792: the barrier scene in room 94

- **Where:** mansion, level 12. No region runs it (`ed check`: "no region's own script"). The game's own code starts
  it:
  - The sanity-level function 0x80069018 calls 0x80163C6C(2792, -1, 1) at 0x80069488, then sets flag 698
    (0x8006949C). It does this when a counter (r13-25500) passes 3 while the player is in one of five zones (0x80069368-0x800693B4).
  - 0x800E8C70 starts it at state 7 when flag 698 is on.
  - It uses thing 911 (chr 122 model 247, placed in room 94: `ed tag 911`), room 94's FirstPersonRgn01-08 and
    ScareCamRgn01-06, and ShldMkr05-08.
  - Room 94's arrival (L12/2648 lines 219-226) remakes the barrier and hides 911 while flag 698 is on.
- **The gates:** line 10 (state 0), line 57 (state 0), line 85 (state 3). All test `L2 = fn78(1, 0)`.
- **Alex's path:**
  - State 0: cutscene on, Alex turned to SanityMkr07 (line 9). A camera on her: one kind inside a FirstPersonRgn,
    another outside (lines 11-25). Camera record 3 points at -402,1224,-450 (lines 26-28). Line 20 is a decompiler
    misread around `fn111`.
  - The barrier at ShldMkr05-08 in the alignment's colour (line 58).
  - States 1-2 pan the camera (lines 69-79). State 3: thing 911 hidden, a flash, control back (lines 83-90).
  - State 4: `fn46(1)`, a render switch (line 100).
- **Other path:** a ScareCam picked by ScareCamRgn (lines 31-51) and room 263 queued (line 52). State 3: a flash
  (line 94). State 5: event 135 to the player (the summon's end), flag 698 off, thing 911 shown (lines 104-106).
- **Player 2 today:** most likely never reaches it. The sanity tracker reads the current player (r13-26524), and
  there is one tracker for player 1 (docs/game/systems/sanity.md, "Co-op, after single player"). So 2792 runs as
  player 1's and takes Alex's path. Not run.
- **Recommendation: no change needed.** If the host ever runs the sanity clock as player 2's
  (docs/research/separate-rooms/sound-readers.md:207), player 2 should take Alex's path. The other path ends a summon.
- **`ed check`:** no route.

## Not hero gates: L12/2778 and 2026 (L09, L16)

The herogates tracker (tools/ed/herogates.py `gates_of`) marks the slot where fn78(1, 0)'s result lands. It forgets
the slot only on a SETLOCAL that doesn't follow a CALL. A later `local Lk = call()` puts a new value in the same slot
with no SETLOCAL, so the tracker keeps the slot.

- **L12/2778 (room 92, Teleport01-03, the three pads that each take one kind of summoned creature):** line 61
  `SummonedMonster = fn78(1, 0)` (a global; the result passes through slot 2). Line 62 `local L2 = ed166(...)` puts
  the **model** in slot 2. Line 87 `if L2 ~= 1` asks "is the creature model 1, pad 2's kind" (lines 69 and 105 test
  79 and 15 for pads 1 and 3). Nothing to decide.
  - A side note for co-op: in solo, Alex herself is skipped at line 7 (model 9). A player 2 with another model
    passes line 8 when the run is theirs, walks onto the pad under a cutscene (lines 17-27), and gets the refusal:
    sound 552, then state 4 sends event 135 to them (lines 123-125, 271-275). That is a short pause, probably
    harmless; not run.
  - `ed check L12/2778 --dry` makes a plan (flag 619 off), but it would test that side note, not a hero gate.
- **L09/2026, L16/2026 (Edward's chapter, the arrival script of the doors into room 237: level-09 catalogue lines
  878-912):** line 15 `PlayerUID = fn78(1, 0)` (a global, through slot 1). Line 69 `local L1 = ed17()` puts the
  **alignment** in slot 1. Line 74 `if L1 ~= 1` picks the Chattur'gha creature (lines 75, 79, 83 spawn one per
  alignment). Nothing to decide. `ed check`: no route (an arrival script).

The `ed patterns` count of uncovered two-path gates includes these two. A fix to the tracker would be to forget the
slot when a CALL's result lands in it.

## What a decision changes in the host

Opening a two-path compare for player 2 is one more kHeroGates entry (level, script, offset, slot, jmpeq). Take the
offsets from `ed herogates --all`: 1081 +0x9C; 1335 +0xF4; 1593 +0xCC; 858 +0x315; 1010 +0x54B (line 87) and +0x883
(line 152); 2792 +0x278, +0x424, +0x550. The two 1010 compares are separate entries, so the end (line 152) can
open while the button compare (line 87) stays as it is. "Together only" for 858 and the first 1010 compare is not
something the table can say today. Unrelated but nearby: the buttons (871 line 8) test the player's subclass, not
the handle, so they aren't hero gates. Whether player 2's body has subclass 1 was not checked.
