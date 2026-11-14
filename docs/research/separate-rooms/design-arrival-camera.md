# Design: the camera at an arrival apart (final, 2026-11-12)

The item: when a player enters a room while the other is in a different room, their camera jumps or glides for a
moment. The user still sees it ("camera jitter when players enter separate rooms"). The kit flagged a nine-frame CAMERA
blip in logs/run-20261111-172710-rooms-r4c-t1-p2door.

Inputs: `arrival-camera-log-readers.md`, `arrival-camera-code-readers.md`, the two refuters
(`refute-arrival-camera-impl.md`, `refute-arrival-camera-goal.md`), hle.c at HEAD 5eb040d, the disassembly through `ed`,
the generated C, the r4c-t1 recording and the baseline runs named below.

Nothing was built, run or edited. Facts are **[read]** (seen, with the place) or **[inf]** (inferred). Every refuter
problem was checked against the code; the last section lists each and what was done.

## 1. What happens, frame by frame, both halves

### The solo arrival: how the game sets the camera

`0x80152614(room, entry, camera hash, script, flags)` is the ticket runner. At 0x80152654 it calls `0x80152298` (load,
slot swap, placing), then the camera block [read `ed dis 0x80152790`]:

| Address | Call | What it does |
|---|---|---|
| 0x80152790 | `0x801EE998(hash, 2, 0)` | Finds the door's camera in the current slot's room record |
| 0x801527CC | `0x801EE764(0x80603288, index, r5..r10 = 0; stack 0, 1)` | Sets eye record 2 and its target. r9 is the cut argument `c`: with r9 = 0 it writes 1 to 0x806037D8+64 (cut pending) [read 0x801EE898-0x801EE8B4] |
| 0x801527D0 | `0x801ECCE0` | Empties the camera stack (depth r13-21700 := 0) |
| 0x801527D8 | `0x801EEB58(2)` | Index r13-26864 := 2 |
| 0x801527DC | `0x801EEA9C` | **The cut**: record 2's place into the shown records 0x806037D8 / 0x80603860 |
| 0x801527F4 | The arrival script | |

- fn8's wrapper 0x801EE8E0 passes its own `c` (r7) to 0x801EE764 as r9 [read 0x801EE900, 0x801EE954]: `fn8(…, c = 0)`
  is the game's own cut, `c != 0` a blend (camera.md: "c, 0 cuts, not 0 blends") [read].
- `0x80152298` also has direct callers that do not pass through 0x80152614 [read: generated/chunks/chunk_0007 26339,
  chunk_0019 17722, chunk_0042 32330, 34781, 40458], and its own camera set at 0x80152588 [read camera.md:39].
- At door 119 (183 → 180, EntryMkr02) the arrival region runs script 1062, `fn8(Camera02, 0, 2, 1, 0)` then `fn3(20)`,
  `fn2(20)`; at door 29 (180 → 183) script 294, `fn8(Camera25, …, 1, 0)` then `fn3(10)`, `fn2(10)` [read in the earlier
  design via `ed script`]. The doors' arrival cameras are the same, Camera02 and Camera25. So in solo the region blends
  from the door's camera to itself and nothing moves [inf; solo -030139 shows a clean cut, log readers §1].

Whose data this reads: the camera block reads no player word except 0x801EE43C (+104 "who it follows", from r13-26524).
It works on whichever camera set is in memory (records 0x80602B18..0x806038E0, stack to 0x8060394C, depth, index:
`COOP_CAMERA_SET`, hle.c:15346-15361) [read]. The roll `0x800860B8` (called by 0x801EEA9C, and by 0x801EE764 when r9 = 0
and its stack +76 is 0, 0x801EE8B8-0x801EE8C8) finds its character by node kind 32, not by r13-21492 (U5).

### Player 1 arrives (player 2 stays elsewhere)

- No bracket: notify_ticket_run returns unless `g_coop_room_p2` (hle.c:13859) [read].
- The game's set is player 1's; their cut is solo's [read in the readers: -182711-rooms-r5-s2-p1door, E cuts to Camera02
  at 3833].
- Player 2's half: `coop_camera_chosen` returns at `coop_stay_apart` before `room_choices++` (hle.c:17233-17236) [read];
  their look is skipped through player 1's change [read code readers]; their list does not change [read 15975].
- **Case D (inferred, never seen in a log).** Player 1 comes into the room player 2 is already in while player 2's set is
  bare (brought there by a JTG or chapter start, no zone of theirs chose since: -030834 shows player 2 bare in 341 for the
  whole run, "[stay] player 2 stays in room 341" at page:1805-1808 and no "zone chose" line among 1435 [coop] lines
  [read]). Two counts then run:
  - At player 1's room change, the room word is the new room = player 2's room, so `coop_stay_apart` is false and
    `room_choices++` runs (17236) [read].
  - One frame later player 1's first look in the fade (`coop_scan_room_start`, 19777-19799: player 1's look run while
    p2_bare, `p1_zones = 1` when the list differs and the fade ticks r13-21784 > 0) also reaches `room_choices++` through
    `!room_change && p1_zones == 1` (17230-17236) [read]. The pattern "the room change, frame 2086" then "player 1's first
    look, frame 2087" is in -182534/page-0001.log [read].
  - At player 2's next turn `coop_camera_p2_in` follows (15398-15416): their set is remade from player 1's door camera
    and their half cuts though they never moved [inf from code].

### Player 2 arrives (player 1 stays elsewhere): player 2's half takes player 1's camera, then glides

Run r4c-t1 [read page log and both.tsv, as in the earlier design]:

| Frame | Log / row | Player 2's half (p2e) | Player 1's half (E) |
|---|---|---|---|
| 3822 | "change to room 180 runs as theirs" | 352,2364,349 (183's Camera25) | -754.4,-147.6,565.7 |
| 3824 | "change done"; "player 2's camera jumps 2760" | step result = player 1's eye | follow ease only |
| 3826 | fade 9; "player 2's zone chose their camera" | -757.1,-153.8,566.2 (= E) | -755.7,-150.7,565.9 |
| 3827-3834 | fade 8 → 1 | -722 → -689 → … → -523 (fn3(20)) | continuous |
| ≈3900 | settles on Camera02 (-63,276,486) | | |

The way back (180 → 183) is the same: 4362 "jumps 959", 4364 p2e = E, then "jumps 263 … 60" toward Camera25. The glide
lasts 55-80 frames, the fade 10. The kit's CAMERA rule (tools/ed/recording.py:228-262: a step over 120 units landing more
than 60 from every camera) saw only 4364-4373 [read]. Player 1's half is continuous at both.

The chain [read in hle.c]:
1. `notify_ticket_run` (13857-13869): player 2 made the player, the room word their room, `coop_cameras_in(cpu, 2)` puts
   in **player 2's own set** (p2_set true, no follow: 15398-15410).
2. The camera block cuts player 2's set to the door's camera; `coop_camera_chosen`'s bracket branch clears `p2_bare`
   (17219-17222).
3. `coop_stay_change_end` (13870-13888) at the behaviour pass: `coop_cameras_in(cpu, 1)` saves the game's set into
   `g_coop_cameras.p2` (15429-15433). **The kept set holds the arrival's cut.** `stay_scene_look_again` arms
   `g_stay_arrival_camera` only with player 1's scene on (17165-17168); not here.
4. Player 2's turn, same pass: `coop_prompt_look` (16544) takes `0x80123F70(object)` and calls `coop_zones_new` (16551).
   The list is new, the flag clear, so 15967-15978 runs: `p2_set = false`, `p2_bare = true`, `g_coop_own_view_frame = 0`.
   (Proof: "player 2's zone chose" prints only while p2_bare (17225-17227); 15976 is the only writer that sets it.)
5. `coop_cameras_in(cpu, 2)` (16566) → `coop_camera_p2_in` → `coop_camera_set_fresh` (15365-15373): the set is now the
   game's, **player 1's records and shown records**; `coop_camera_step` renames the records following player 1 (16081-16083).
6. The region's fn8 (blend) eases from player 1's eye (player 1's room's coordinates) to player 2's room's camera: the
   glide. With no region at the entry spot, player 2 keeps player 1's camera (user play 1, 2291 to the end).

`0x80123F70(object)` [read `ed dis 0x80123F70`]: when `object == r13-26524` it returns the current slot's info
(0x801551A0(2), 0x80123F8C-0x80123FA0); otherwise it walks slots 2 and 3 for the record whose +0x8140 room equals
object+592 with both loaded bytes set (0x80123FA4-0x80123FFC). **The zone list at info+180 is per slot.** Which one you
get depends on whether the object passed is the current player's and which slot is current.

## 2. The approach: player 2's arrival camera is kept for the room it was cut for, once

The game already makes the right camera for player 2's own arrival, in their own set (steps 1-3). The host throws it away
at step 4 and copies player 1's at step 5. The rule:

> At player 2's own change (the R4 bracket), the set the game's own arrival cut is tagged with the room, the zone list of
> that room and the ticket count. The first new zone list of player 2's after it is checked against the tag: if it is
> the same list, the same room (their +592) and no ticket has run since, the set is kept (zone words, prompts, looks and
> states still reset, as today). The tag is used up at that first list change, kept or not.

This is the R4c keep, generalised, and tied to the one list it was made for.

### The hooks, all in runtime/host/hle.c

**H1. The tag.** Add to `g_coop_cameras` (15382-15392):

```c
bool     p2_arrived;          // player 2's set holds the game's own arrival cut (their bracketed change, R4)
s32      p2_arrived_room;     // the room it was cut for (r13-23276 at the bracket's end, before the put-back)
u32      p2_arrived_zones;    // that room's zone list (info+180 of 0x80123F70(their object) at the bracket's end)
unsigned p2_arrived_tickets;  // g_coop_tickets then
bool     p2_foreign;          // the set was last made from player 1's while apart (another room's camera): H9, H10
```

and `static bool g_stay_bracket_chose;` beside `g_stay_bracket`.

**H2. notify_ticket_run** (13857). After `coop_cameras_in(cpu, 2)`: `g_stay_bracket_chose = false;`. If
`g_coop_cameras.lent`, log `[stay] player 2's change with the cameras lent (frame N)` (U2: `coop_camera_chosen` returns
at `lent`, 17213, so nothing is tagged: today's path).

**H3. coop_camera_chosen's bracket branch** (17219-17222): `g_stay_bracket_chose = true;` beside `p2_bare = false`. That
branch is reached by the change's own `0x801EE764` on record 2 inside the bracket, the arrival cut in player 2's set
[read 17207-17222]. During the Tome copy's turn `coop_camera_chosen` is off (`OFF(coop_camera_chosen)` at 26609, applied
at hle_dispatch 5533-5535 when `g_tome_copy_turn`) [read]; a bracket there stays untagged, which is today's drop (U6).

**H4. coop_stay_change_end** (13870). After the put-back block (13876-13880), before `g_stay_bracket = 0`:

```c
g_coop_cameras.p2_arrived = false;
if (g_stay_bracket_chose && !g_coop_cameras.lent) {
    const u32 object = mem_read32(cpu, g_coop_p2 + 52u);   // not r13-26524 now: be_player2(false) ran (13873)
    u32 info = 0u;
    if (hle_guest_call(cpu, COOP_CHARACTER_INFO, &object, 1, 0.0, &info) && guest_ram_range_valid(cpu, info, 184u)) {
        g_coop_cameras.p2_arrived = true;
        g_coop_cameras.p2_arrived_room = new_room;
        g_coop_cameras.p2_arrived_zones = mem_read32(cpu, info + 180u);
        g_coop_cameras.p2_arrived_tickets = g_coop_tickets;
        g_coop_cameras.p2_foreign = false;  // the game's own cut replaced whatever was there
    }
}
g_stay_bracket_chose = false;
```

- Whose data: player 2's object is not the current one here (r13-26524 is player 1's again after 13873), so
  0x80123F70 searches the slot records by player 2's +592 and returns the info of the slot their new room is in [read
  0x80123FA4-0x80123FFC]. Player 1's slot and info are not touched (a read-only getter).
- `new_room` is r13-23276 read at 13875, before the put-back: the room the change went to [read].
- `g_coop_tickets` is counted at every ticket's ask (notify_room_ticket, 11297-11299), which comes before the runner
  [read]; so the count recorded is the one after player 2's own ticket.
- Log under ED_COOP_LOG: `[stay] player 2's set holds their arrival's camera for room R (zones 0x…, frame N)`.

**H5. coop_zones_new** (15952-15979). One branch:

```c
const u32 zones = mem_read32(cpu, info + 180u);
if (zones == g_coop_zones.list) return;
coop_p2_own_cancel(cpu, "a new room's zones");
g_coop_zones.list = zones;  /* zone words, prompt slots, looks, g_coop_p2_looks_set, states: as today */
const bool kept = g_coop_cameras.p2_arrived && zones == g_coop_cameras.p2_arrived_zones &&
                  g_coop_tickets == g_coop_cameras.p2_arrived_tickets &&
                  coop_object_room(cpu, g_coop_p2) == g_coop_cameras.p2_arrived_room;
g_coop_cameras.p2_arrived = false;               // one use, kept or not
if (kept) {
    if (!(g_settings.mod_p2_camera == 2 || coop_p2_look_up()))
        g_coop_own_view_frame = 0ull;            // modes 0/1: as today's drop (coop_camera_step won't refresh it, 16072)
    log "[stay] player 2's first look in room R keeps their arrival's camera (frame N)";
} else {
    g_coop_cameras.p2_set = false; g_coop_cameras.p2_bare = true; g_coop_own_view_frame = 0ull;
}
```

- Remove `g_stay_arrival_camera` (15951-15965) and its arm in `stay_scene_look_again` (17168). Keep the rest of
  `stay_scene_look_again` (the scene look's re-arm and its log). R4c's case is a subset: the bracket during player 1's
  scene is tagged like any other.
- Why each test, for both players:
  - `zones == p2_arrived_zones`: the list is player 2's new room's. It fails in the case the goal refuter found: player 2
    brought by player 1's change (chapter page, chapter end, JTG, left behind). There `coop_prompt_look` calls
    `coop_zones_new` (16551) before its `coop_left_behind` return (16552), and `coop_follow` writes the new +592 only after
    player 2's turn (21204, 21296-21306) [read]; in that turn r13-26524 is their object (20679-20685), so the info is the
    current slot's, player 1's new room's, while +592 still names the old tagged room [read]. The list differs from the
    tag's, and the ticket count moved too: the drop runs, as today.
  - `g_coop_tickets` unchanged: no change of anyone's ran since (a same-room ticket, a later door, a chapter start) [read
    11299 counts every ask].
  - +592: player 2 is still in the room the cut was for.
  - One use: after it, any later list change in that room (the game's own load or continue, a re-unpack, a teleport,
    a JTG into the same room) drops as today.
- Mode 2: the same turn's `coop_camera_step` (20802, after `coop_prompt_look` at 20713) rewrites `g_coop_own_view` from
  the kept set before any draw (`hle_coop_player2_camera`, 16294) [read the call order], so it is not zeroed there and
  player 2's half never shows the game's view (player 1's) for a frame. Modes 0 and 1: zeroed exactly as today's drop
  does, so the next lend (15479-15481) and `coop_found_place` (16172) behave as today.

**H6. Clears.** The tag (`p2_arrived = false`) in `coop_camera_set_fresh` (15365), the only remake of the set from the
game's. In `hle_coop_state_load` (beside `g_coop_zones.list = 0u`, 28059): `p2_arrived`, `p2_foreign`,
`g_stay_bracket_chose`, and H8's record (`g_stay_start.on`). `coop_scene_set`'s afresh needs no clear: it sets
`p2_set = false`, and the remake that follows clears the tag in `set_fresh`. `coop_cutscene_over`'s put-back (25657-25666)
is **not** a clear: the set it saves is player 2's own after a cutscene in their room (a cutscene ending in another room
returns first, 25657-25660) [read].

### Why it is right for each player, each direction

| Case | Player 2's half | Player 1's half |
|---|---|---|
| Player 2's door apart (183 → 180) | The door's camera (Camera02) from the first frame, cut under their own fade; the region's fn8 blends Camera02 to Camera02, as solo [inf from §1] | Unchanged: their set is put back at the bracket's end (`coop_cameras_in(1)`), as today |
| Player 2's door back into player 1's room (180 → 183) | The same: Camera25 from the first frame | Unchanged |
| Player 2's door during player 1's scene (R4c) | Same branch; the scene's look re-arm kept | The scene goes on |
| Player 2's entry spot in no camera region (user play 1, 342) | Keeps the door's camera until a region of theirs chooses, as solo [read: the cut at 0x801527DC] | Unchanged |
| Player 1's door apart | Unchanged (no bracket) | The game's cut, as solo |
| Player 1's door into player 2's room (case D) | H8: their set stays theirs, bare or not | The game's cut |
| Player 2 brought by player 1's change (JTG, chapter page or end, left behind) | Different list and a new ticket: the drop and the room's-start follow, as today | Unchanged |
| A later list change in the room player 2 arrived in | Tag used up: the drop, as today | Unchanged |

## 3. The other guards and fixes (same build)

**H7 (guard E): player 1's scene ending does not remake the camera of a bare player in another room.**
- Today: `afresh = !own && (fresh || (chose && shared)) && !coop_left_behind(cpu)` (17141); `coop_left_behind` is false
  for a resident other room (15992-15995) [read].
- Change to `… && !coop_left_behind(cpu) && !(r6_apart(cpu) && !fresh)`.
  - `r6_apart` (24197-24202): mod_coop, coop_stay, no bracket, both players' +592 differ and both rooms resident, player
    1's node from `r6_player1_node` (r13-21492 unless swapped, else `g_coop_turn_p1`/`g_coop_p1_node`) [read]. It is the
    rule the enemies and sound already use; no new helper.
  - Blocked: a bare, not-fresh set apart. That is player 2's own room's start camera (JTG brought, player 1 left).
    Remaking it from the game's set at the scene's end gives them player 1's room's scene camera [inf from 17129-17141
    and 15365]. Kept, they stay on their own room's camera.
  - Not blocked: a fresh set (copied from the game's in the scene). Apart, that copy is already another room's camera
    (player 1's scene), so neither keeping nor remaking is right. It keeps today's remake; the remake is marked foreign
    (H9) and H10 cuts it at their next own zone choice. Changing nothing more here keeps the code's own branch.
- For both players: player 1's half is not touched at all by H7 (it only decides player 2's `p2_set`).

**H8 (guard D): player 1's arrival into the room player 2 is already in is not "the room's start" for player 2.**
- A per-change record, set at `notify_ticket_run`'s entry **before its gate**, for player 1's changes only:

```c
static struct { bool on, apart; s32 target, p2_room; unsigned tickets; } g_stay_start;
g_stay_start.on = false;
if (coop_stay() && g_coop_p2 != 0u && sda_ready(cpu) && !g_coop_room_p2 && g_stay_bracket == 0) {
    const s32 p1_room = (s32)mem_read32(cpu, cpu->gpr[13] - 23276u), p2_room = coop_object_room(cpu, g_coop_p2);
    g_stay_start = (typeof(g_stay_start)){true, p2_room != -1 && p2_room != p1_room, (s32)cpu->gpr[3], p2_room, g_coop_tickets};
}
```

  - r13-23276 at the runner's entry is player 1's room (no bracket: the room word is the room in play, player 1's)
    [read 13832-13836: `coop_stay_apart` compares the same words]. `gpr[3]` is the target room (the runner's first
    argument, printed the same way at 13866) [read].
- In `coop_camera_chosen`, right after `if (coop_stay_apart(cpu)) return;` (17233-17234), so it covers **both** counting
  paths (the room change and player 1's first look, `p1_zones == 1`):

```c
if (g_stay_start.on && g_stay_start.apart && g_stay_start.tickets == g_coop_tickets &&
    g_stay_start.p2_room == g_stay_start.target && (s32)mem_read32(cpu, cpu->gpr[13] - 23276u) == g_stay_start.target &&
    coop_object_room(cpu, g_coop_p2) == g_stay_start.target) {
    log once per change "[stay] player 1 came into player 2's room R: player 2's camera stays theirs (%s, frame N)";
    return;
}
```

- Cleared: in `coop_scan_room_start` after the look it ran with `p1_zones == 1` (player 1's first look in the fade is
  the last count of the room's start); by the next ticket (the count moves); at a state load (H6).
- The together case is not changed: a same-room change of player 1's while together has `apart` false (player 2's +592
  equals player 1's room at the entry). A chapter start or JTG that brings player 2 from another room has
  `p2_room != target`. A change that does not go through 0x80152614 (direct 0x80152298 callers) never sets the record
  (`on` stays false from the last runner entry, or the ticket count has moved), so it counts as today (U9).
- Player 2 is not required to be bare: a non-bare player 2 is not followed anyway (15403), so the early return changes
  nothing for them.

**H9 (log, and the foreign mark).** In `coop_camera_p2_in`, before `coop_camera_set_fresh` (15414):
`g_coop_cameras.p2_foreign = r6_apart(cpu);` and, when true, one log line per room: `[stay] player 2's set remade from
player 1's while apart (their room R, player 1's Q, frame N)`. `r6_apart` is false inside the bracket
(`g_stay_bracket != 0`), so the remake at notify_ticket_run's `coop_cameras_in(2)` when `!p2_set` (after a state load or
a scene's afresh) is not logged or marked; the arrival cut overwrites it anyway [read 13865, 15414, 24198].

**H10 (the foreign set's cut): a set made from another room's camera cuts, not blends, at its first own zone choice.**
- In `coop_camera_chosen`'s player-2 branch (`!room_change && (g_coop_turn || g_coop_prompt_look || coop_use_theirs() ||
  r13-21492 == g_coop_p2)`, 17224-17229), when `cpu->pc == CAMERA_CHOOSE && g_coop_cameras.p2_foreign`: set
  `cpu->gpr[9] = 0` and `p2_foreign = false`, log `[stay] player 2's first own camera in room R after another room's:
  cut (frame N)`.
- That is the game's own `fn8(…, c = 0)`: 0x801EE764 then writes the cut-pending flag in the shown record of the set in
  memory, player 2's (0x801EE898-0x801EE8B4) [read], the same one solo scripts use for a cut [read camera.md]. It is a
  notify at the function's entry (27466), so r9 is still the argument [inf: notifies run before the body, as
  notify_ticket_run reads gpr[3]].
- Whose set: the branch is reached only for a choice of player 2's (their turn, look, use, or them the player), with
  their set in (`coop_cameras_in(2)` precedes their look's scan at 16566 and 16592) [read]. Player 1's choices never
  reach it.
- Where it applies: a state loaded apart (the drop resets their zone words, so their first look re-enters the region
  they stand in and its fn8 is the cut), a scene's afresh apart (cut at the next region they enter), a teleport without a
  ticket. Never in solo or together (H9's mark needs `r6_apart`).

**H11 (the Tome item route's order).** At 26427-26444, `coop_tome_player(cpu, true)` makes r13-26524 player 2's object
(25524-25531), so `COOP_CHARACTER_INFO` takes the current slot's branch (0x80123F8C-0x80123FA0); `coop_stay_room_in`
runs only at 26444, after `coop_zones_swap` (26435), which calls `coop_zones_new` (16006) [read]. Apart, it hands player
2's zone words player 1's room's list: their set is dropped (no tag on a player who stayed), their half gets player 1's
room's camera after any item from their Tome, and their zone words are swapped into player 1's room's zones.
- Fix: the same order as `coop_use_start` (17912-17921): `coop_tome_player(true)`, `coop_be_player2(true, was)`,
  `g_coop_turn_p1 = was[1]`, then `room_in = coop_stay_room_in(cpu, room_keep)`, then `COOP_CHARACTER_INFO`, then the
  swaps. `coop_stay_room_out` stays where it is.
- Together: `coop_stay_apart` is false, `room_in` false, the current slot is the shared room: the info is the same as
  today's. Solo: the route needs `g_coop_p2`. The timer/arrival route at 18490 already takes the info by slot search
  (player 2's object is not r13-26524 there) and is right; it is unchanged.

**H12 (player 1's half at player 2's arrival scene end).** 18525:
`if (arrival_scene && scene_was && !g_coop_p2_scene && g_coop_cameras.lent && g_coop_p1_own_camera != -1)` runs
`coop_p1_scene_end_camera`, which copies the game's (lent, player 2's) set into `g_coop_cameras.p1` (16046-16063). With
the stay switch `g_coop_p1_own_camera` is set only when `!coop_stay()` (17059) and is reset to 0 at every ticket
(11321), so 0 passes the test [read]. Apart, that gives player 1's half player 2's room's camera. Add `&& !r6_apart(cpu)`.
No stay run's log has the path's line, so it is reachable, not seen [inf].

**Kit (no game code): an ARRIVAL rule in tools/ed/recording.py `check`.**
- For each half, at each row where that half's room changes (p1room/p2room) and, on the rows that follow, p1room !=
  p2room:
  - flag if, while the rooms differ on that row, the half's eye (p1ex.. / p2ex..) is within 60 units of a camera of the
    other player's room and more than 60 from every camera of its own (`nearest_camera`, recording.py), within 10 rows
    of the arrival;
  - flag a glide: in the 60 rows after the fade ends, the half's eye travels more than 300 units while that player's own
    position moves less than 20.
- Equal eyes at a shared door camera (players together, a rejoin, C3) are not flagged: the rooms are equal on those rows.

## 4. Alternatives, and why not

- **Only arm `g_stay_arrival_camera` at every bracket** (both readers' minimum): one-shot, spent by whatever list comes
  first; the bring case and the Tome route's wrong list both spend or misuse it. The tag here has the list, room and
  ticket tests.
- **Do the new room's reset in `coop_stay_change_end`** (code readers): moves the zone-word reset away from the one place
  every caller meets. H4 uses the same getter only to record the list.
- **Refuse `coop_camera_set_fresh` while apart** (log readers' B): player 2 would have no set. Kept as H9's mark.
- **Snap player 2's first step with `0x801EEA9C` at every arrival** (log readers' C): the bracket already cut their set.
  H10 uses the game's own c = 0 only where the set is foreign.
- **Run the game's arrival camera sequence (0x801EE998 → 0x801EE764 → 0x801ECCE0 → 0x801EEB58(2) → 0x801EEA9C) at a
  scene's end or a load apart** (goal refuter, problem 4): it needs a door's camera hash, which a scene's end or a load
  does not have. H10 gives the same result from the game's own region choice. The sequence stays the plan for the
  teleport (§6), which has a ticket.
- **Re-running the arrival's camera in player 2's turn**: the game already ran it once, as theirs.
- **Moving `g_coop_zones.list` to the new list at the bracket's end**: skips the new room's zone-word reset.
- docs/coop-failed-attempts.md has no attempt on this camera path [read in the earlier design]; line 166 (the left-behind
  look) and 243 (light easing) are other systems.

## 5. The gate: solo and together

- **Solo:** `g_coop_p2 == 0`: no bracket (13859), no player 2 turn, no `coop_zones_new`; H8's record needs `g_coop_p2`;
  H7, H9, H12 use `r6_apart` (needs `mod_coop` and `g_coop_p2`); H10 needs `p2_foreign`; H11's route needs `g_coop_p2`.
  Since 5eb040d the switches are set only with Mods > Co-op on (frontend.cpp `rooms_follow_coop`) [read in the earlier
  design].
- **Co-op without ED_COOP_STAY:** `coop_stay()` is 0: no bracket, so no tag and `kept` always false (today's drop); H8's
  record needs `coop_stay()`; `r6_apart` needs it; H11's `coop_stay_room_in` is a no-op.
- **Together, with the stay:** H8 needs `apart` at the runner's entry; H7, H9, H12 need `r6_apart`; H10 needs a set made
  while apart; H11 is the same info as today. The only path that runs from a together start is player 2's own door,
  which under ED_COOP_STAY is already the R4 bracket (it begins together and ends apart): its arrival camera is
  player 2's own, as in solo. The rejoin (player 2's door into player 1's room) is a bracket too and ends together; its
  camera is again player 2's own door camera. Neither exists without the stay switch.

## 6. Interactions

- **R1-R3 (slots, stayer):** H4 reads player 2's +592 through the game's getter and the room word; no slot writes.
- **R4 (bracket):** H2-H4 sit inside notify and end; `coop_cameras_in`'s order is unchanged.
- **R4b:** untouched. **R4c:** `g_stay_arrival_camera` replaced by the tag; the scene re-arm kept.
- **R5, R6, R6b, R6c:** no camera reads; H7, H9, H12 reuse `r6_apart`, so "apart" means the same everywhere.
- **The close-up (step B, record 10):** `coop_p2_own_cancel` stays before the keep or drop. `coop_camera_p2_in`'s `rest`
  test and the lend route are unchanged.
- **The Tome copy:** H11 fixes its info; the tag's list test means a Tome route's swap of the right list in the tagged
  room keeps (and uses up) the tag, a wrong list drops. A Tome item that changes room (L01/1100-1103) is a bracket only if
  it is player 2's ticket (U3).
- **The renderer (camera.c):** player 2's half draws `g_coop_own_view` (16294-16304); see H5 for when it is zeroed.
- **The future teleport ("Bring X to Y"), both directions:** bringing player 2 to player 1 is a ticket asked as player
  2's (`g_coop_room_p2` from who asked, 11309-11310; notify_ticket_run needs it, 13859): it gets the bracket, the
  arrival cut in their set and the tag. Bringing player 1 to player 2 is player 1's ordinary change: their own cut, and
  H8 keeps player 2's set. A placement without a ticket remakes player 2's set (H9 logs it, H10 cuts it at their first
  region); for player 1 nothing re-aims at all. That is a rule for that design.

## 7. Save/load and room-change edges

- **State load:** player 2's camera set is not in `HleCoopState` (hle.c:27900-27920) [read in the earlier design]. After a
  load `g_coop_zones.list = 0` (28059), so the next look drops and remakes. Apart, the remake is player 1's room's
  camera; H9 marks it foreign and H10 cuts at their first look's region choice (their zone words were reset by the drop,
  so the region they stand in chooses again) [inf]. H6 clears the new flags. `g_stay_bracket` and `g_coop_room_p2` are
  not cleared at a load and `hle_save_held` (28008-28017) does not wait on the bracket [read]: older, and the bracket
  lasts one tick; noted for the "saving apart" item the user put last.
- **In-game saves:** only player 1 saves today; nothing here writes a save.
- **Two changes of player 2's before a look:** each bracket end re-tags with its own list and count.
- **A change with the cameras lent:** not tagged (H2, U2).
- **Player 2 changes while player 1 changes:** the second is refused (R4 record).
- **A third room:** the tag holds a room, a list and a count, no slot.

## 8. Risks and unknowns a run must settle

- **U1, the zone-list pointer repeating** for another room in the same slot: `coop_zones_new` returns early and nothing
  resets (older than this item). The tag then stays until a different list, which fails the list test, or a ticket.
  Log line (ED_COOP_LOG) in `coop_zones_new`: `[stay] player 2's zones: list 0x… -> 0x…, their room R (slot S),
  kept/dropped/same list`.
- **U2, a change with the cameras lent:** H2's line. If it prints, the lent path needs its own rule.
- **U3, the Tome items' room changes** (1100-1103): whose ticket? The `[stay] player 2's change to room` line shows it.
- **U4, an entry spot whose region camera differs from the door's:** player 2 blends from the door's to the region's,
  as solo does there. Not this bug.
- **U5, 0x800860B8's roll** at player 2's cut (and at H10's c = 0, 0x801EE8C8) uses the first kind-32 node, probably
  player 1 [inf]. A roll, not a jump; separate.
- **U6, a bracket during the Tome copy's turn:** `coop_camera_chosen` is off, so no tag; today's drop. If U3 shows
  player 2's Tome item tickets as brackets, set the chose flag from notify_ticket_run itself (the camera block runs on
  both branches, 0x8015275C → 0x80152784).
- **U7, a fresh set apart at player 1's scene end** keeps today's remake (player 1's room's camera) until player 2 walks
  into a camera region of theirs, where H10 cuts. The remaining time on the wrong camera is today's, not new.
- **U8, H10's consumer:** the cut-pending flag (+64 of the shown record) is read by the shown record's own step function
  (+108, called from 0x801EB9A0) [inf: not traced to the read]. The same flag is what every solo `fn8(…, 0)` sets, so it
  is the game's path; a run must show p2e jump to the region's camera in one row.
- **U9, room changes not through 0x80152614** (the direct 0x80152298 callers): no bracket, no H8 record: today's
  behaviour for both halves.
- **U10, the third-person wall test in modes 0/1** uses player 1's room's collision while apart (camera.c:1747-1782,
  code readers §5f) [read by the goal refuter]: out of scope, recorded for the lights/camera items.
- **Out of scope**, same frames, other items: the 2-7 frame freeze with no pad read (the freeze item); the renderer's
  in-between moment at any cut (code readers §5e).

## 9. Scripted checks, one build

Build: `cmd //c "$(cygpath -w tools/ed/build-win.bat)"`. Host-only, so a state made on this build from Jump to Game
loads (fresh state rule).

```sh
cd "/e/Eternal Darkness Recomp Fork"
export ED_ROOM_SLOTS=2 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_SHOW_TRACE=1 ED_FIN_LOG=1 ED_HEAP_CHECK=2000 ED_RUN_SHOTS_MS=200
rm -f saves/rooms-ch1-183.state saves/rooms-ch1-341-apart.state
# S0: the together state without the stay (player 2 brought through 101, 105, 111, 29)
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 cam-state </dev/null
export ED_COOP_STAY=1; ST=saves/rooms-ch1-183.state
# C1: player 2's door 183 -> 180 (3760) and back (4300); player 1 stays in 183
ED_RUN_FRAMES=4900 ED_RUN_SECONDS=140 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3760,3770,b;4100,4200,lup;4300,4310,b;4500,4600,lup" ED_INPUT_SCRIPT="3900,4000,0,0,127;4500,4600,0,0,-127,0" sh tools/ed/run.sh $ST cam-c1-p2door </dev/null
# C1m0: C1 with mod_p2_camera=0 in the run's settings (the default camera mode)
# C2: player 1's door 183 -> 180 and back by 29; player 2 stays in 183
ED_RUN_FRAMES=4900 ED_RUN_SECONDS=140 ED_PUT="3680,2,1305,1508,185" ED_RUN_SCRIPT=110:119,170:119,650:29,710:29 ED_PAD2_SCRIPT="3900,4000,lup;4500,4600,lleft" ED_INPUT_SCRIPT="4100,4200,0,0,127" sh tools/ed/run.sh $ST cam-c2-p1door </dev/null
# C3: player 2's door, then player 1 follows into 180 where player 2 is
ED_RUN_FRAMES=4900 ED_RUN_SECONDS=140 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_RUN_SCRIPT=500:119,560:119 ED_PAD2_SCRIPT="3760,3770,b;4100,4200,lup;4500,4600,lleft" ED_INPUT_SCRIPT="3900,4000,0,0,127;4500,4600,0,0,-127,0" sh tools/ed/run.sh $ST cam-c3-p2door-p1follows </dev/null
# C4: together, no doors
ED_RUN_FRAMES=4400 ED_RUN_SECONDS=120 ED_PUT="3680,1,1305,1508,185;3680,2,1250,1650,185" sh tools/ed/run.sh $ST cam-c4-together </dev/null
# D: JTG with the stay: player 2 bare in 341, player 1 to 343 (the JTG route) and back into 341; save the apart state
JTG_SLOT=1 ED_SAVE_STATE=2400:saves/rooms-ch1-341-apart.state ED_RUN_FRAMES=3750 ED_RUN_SCRIPT=<2600:the 343->341 door script from `ed door 343`> sh tools/ed/jtg.sh 1 cam-d-p1-into-bare-p2 </dev/null
# F: from D's state, player 2's own door 341 -> 342 (L01/103 at Region01, EXAMINE then DETAIL: user play 1's stuck spot)
ED_RUN_FRAMES=<+1200> ED_PUT="<f>,2,<Region01's spot from ed room 341>" ED_PAD2_SCRIPT="<f+80>,<f+90>,b;<f+200>,<f+210>,b" sh tools/ed/run.sh saves/rooms-ch1-341-apart.state cam-f-p2-341-342 </dev/null
# L: load D's apart state (the state-load path: H9 then H10)
ED_RUN_FRAMES=<+600> sh tools/ed/run.sh saves/rooms-ch1-341-apart.state cam-l-load-apart </dev/null
# J1: JTG in co-op with the stay (compare with -030834); J2: solo (compare with -030651)
JTG_SLOT=1 ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 cam-jtg-coop </dev/null
JTG_SLOT=1 JTG_SET="mod_coop=0" ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 cam-jtg-solo </dev/null
```

Placeholders in angle brackets are filled from `ed door 343`, `ed room 341` and D's both.tsv before the run. Further,
outside chapter 1's 183/180 pair:
- **H, another chapter:** a JTG of another slot with the stay, and player 2's door in its first room (`ed door` for the
  room).
- **K, a non-door ticket of player 2's:** player 2's Tome item L01/1100 in 183 (a transition into 183, U3), from C1's
  state. It also exercises H11.
- **E, player 1's scene ends while player 2 is bare apart:** from D's state, player 1 walks into a scene region of 343
  (find one with `ed room 343`, a script with fn22). If none is reachable by script, it goes to the user's play list.

What to read, for both halves in every run: `ed timeline RUN`, `ed check RUN`, EVENTS.log, both.tsv (E/p1e = player 1's
half, p2e = player 2's half), the shots.

- **C1:** in order "runs as theirs", "change done", "set holds their arrival's camera for room 180", "first look in room
  180 keeps their arrival's camera". No "player 2's zone chose their camera" at ≈3826. First p2e row in 180 is
  -63,276,486 (Camera02), then |Δp2e| < 5 a frame to 3900; at most one "camera jumps" line, at 3824. The way back: p2e ≈
  352,2364,349 (Camera25) on the first row; no CAMERA 4364-4373 in `ed check`; no ARRIVAL. E continuous across
  3822-3830 and 4362-4366. No H9, H10 or H8 line. Shots ≈3830-3850: the right half shows 180 from the door's camera.
- **C1m0:** the same log lines; p2e is camera.c's own follow (no glide existed in this mode); no regression from
  -172710's mode-0 behaviour; record what the wall test does (U10).
- **C2:** E cuts to Camera02 and back to Camera25; p2e continuous. On the way back into 183, H8's line once ("player 1
  came into player 2's room 183"). No tag, H9 or H10 line.
- **C3:** player 2 arrives as C1; at player 1's arrival into 180, H8's line; p2e stays on Camera02, E cuts to Camera02.
  ARRIVAL not flagged (the rooms are equal on those rows).
- **C4:** no [stay] camera lines; `ed check` as r6b-c4-together.
- **D:** at player 1's change to 343, "player 2 stays in room 341" as -030834 page:1805-1808. At player 1's return into
  341, H8's line for the room change and for the first look (both counting paths); **no** "[coop] player 2's camera: the
  room's start chose one, theirs too" line (15413); p2e unchanged across the return.
- **E:** at the scene's end, the scene-over line says "kept"; p2e stays in 341's camera; no H9 line.
- **F:** "runs as theirs" for room 342, the tag and keep lines; p2e on 342's door camera (Camera01 of L01/103) and held
  there (no region at the entry: user play 1's 22,-2348,833 is the expected eye); no H9 line.
- **L:** the H9 line once after the load, then the H10 line at their first look; p2e jumps in one row to a camera of 341,
  no glide; p1e unchanged.
- **H, K:** the same lines as C1 for H; for K, whose ticket it is (U3), and the info line (U1) shows player 2's room's list.
- **J1:** the same [stay] and camera lines as -030834-jtg-coop-gate (no "room's start chose" or "zone chose" lines at the
  343 change, "player 2 stays in room 341" at 2085-2087); no tag, H8, H9 or H10 line before player 1's own return.
- **J2, solo:** "separate rooms off for this run", no [stay] or [coop] camera lines; E as -030651-jtg-solo-gate.
- **S0** (no stay): no [stay] lines; the "room's start" lines as -182534 (page-0001.log has "the room change, frame
  2086", "player 1's first look, frame 2087", "player 2's zone chose their camera (frame 2089)") [read].

**Not scriptable here** (the user's play-rooms-log.bat): player 2's door during player 1's scene (R4c's case).

## 10. Refuted or corrected

Implementation refuter:
1. *H8 misses player 1's first look (p1_zones == 1).* **Real** [read 17230-17236, 19785-19795]. H8 now sits after the
   `coop_stay_apart` return, which both counting paths pass, and its record lives until the first look or the next
   ticket, not the behaviour pass.
2. *H8 changes the together case.* **Real.** The record now needs `apart` at the runner's entry (player 2's +592 != the
   room word, player 1's room).
3. *The tag is never consumed.* **Real.** One use in H5, plus the list and ticket tests; H6 clears at the remake and the
   state load.
4. *The kept branch does not zero g_coop_own_view_frame.* **Real** for modes 0/1 [read 16072, 15479-15484, 16172]. Zeroed
   there; in mode 2 the same turn's step rewrites it first [read 20713 → 20802], so it is left.
5. *H7's helper duplicates r6_apart.* **Real.** H7, H9 and H12 use `r6_apart`; no new helper.
6. *H9 fires at the bracket's start.* **Real** [read 13865]. `r6_apart` is false in the bracket.
7. *H3 depends on coop_camera_chosen (lent, Tome copy turn).* **Real, safe.** Named as U2 and U6, with the fallback.
8. *New flags not cleared at a state load.* **Real.** H6 clears them and H8's record; the older words are noted in §7.
9. *H6's clear in coop_cutscene_over is not needed.* **Real** [read 25652-25666]. Removed.
10. *The kit rule flags correct arrivals.* **Real.** It now counts only rows where the rooms differ, compares with the
    rooms' cameras, and measures a glide only while the player stands still.

Goal refuter:
1. *The tag is never used up; the brought case keeps the old room's camera.* **Real** [read 16551-16552, 21204-21306,
   0x80123F70]. The list, ticket and room tests and the one-use clear in H5.
2. *H8 does not close case D; it can match players who are together.* **Real.** As impl 1 and 2.
3. *The Tome item route takes player 1's room's zone list while apart.* **Real** [read 26427-26444, 25524-25531,
   0x80123F8C-0x80123FA0]. H11 puts `coop_stay_room_in` before the info, as `coop_use_start`.
4. *H7 blocks the remake even when the set is fresh.* **Real.** H7 now blocks only a bare, not-fresh set apart. The
   game's arrival sequence needs a door hash a scene's end lacks (§4); H9's foreign mark and H10's c = 0 cut give the
   player their own room's camera from the game's own region choice.
5. *The checks would not prove what matters.* **Real.** Added D (bare apart, H8), E (scene end apart, H7), F (341 → 342,
   the user's stuck spot), L (state load apart, H9/H10), H (another chapter), K (a non-door ticket), C1m0 (mode 0); J1
   compares with -030834, whose lines it really has [read: 0 "room's start"/"zone chose" lines in its page log, 1435
   [coop] lines], and S0 with -182534 [read].
6. *Player 2's arrival scene apart overwrites player 1's kept set.* **Real, reachable** [read 18525, 17059, 11321,
   16046-16063]. H12.
7. *Only mode 2 is designed.* **Real.** H5 keeps modes 0/1 identical to today's drop for the view; C1m0 added; the wall
   test is U10.
8. *H7 helper mixes nodes and objects.* **Moot**: the helper is gone; `r6_apart` reads nodes through
   `r6_player1_node` and +592 through `coop_object_room` (node+52 → object) [read 13820-13823, 24190-24202].
9. *The teleport note does not say whose ticket.* **Real.** §6 states both directions.
