# Refuter: the arrival camera design, against the overall goal and both players (2026-11-12)

The design I tested is `design-arrival-camera.md`. I read it with `arrival-camera-code-readers.md`,
`arrival-camera-log-readers.md`, hle.c at 5eb040d, camera.c, `ed dis 0x80123F70` and the logs named below. I edited no
code, built nothing and ran nothing.
- **[read]** means I saw it at the place given.
- **[inf]** means an inference.

**Verdict: needs changes.** The core of the design holds:
- the chain from "the bracket cuts player 2's set" to "the first look drops it" to "it is remade from player 1's";
- keeping player 2's own arrival cut;
- the solo and no-stay gates.

I re-read each link and they are right [read hle.c:13858-13889, 15398-15433, 15952-15979, 17206-17239]. Four problems
break the rule "for every room, chapter, case and both players", and the checks would not prove the parts that matter.

## What holds (verified)

- The drop and the remake happen as the design describes:
  - `coop_zones_new` clears `p2_set` and sets `p2_bare` (15975-15977).
  - `coop_camera_p2_in` then remakes the set with `coop_camera_set_fresh` (15405-15416).
  - The bracket branch clears `p2_bare` (17219-17222).
- `0x80123F70` returns the current slot's info when the object passed is the current player's object (r13-26524). Any
  other object gets a walk of slots 2-3 by +592, and the result is 0 when no slot matches [read `ed dis 0x80123F70`:
  0x80123F94-0x8012400C].
- At a chapter start, player 2 is not in the node list when the first change runs, so H8's "room before the change" is
  not the new room there [read logs/run-20261112-030834-jtg-coop-gate/page-0001.log:104-143: the change at 1319, and
  "player 2 (listed)" after it].
- Solo, and co-op without ED_COOP_STAY, never reach H2-H5 (13859; `coop_stay()` at 13814).

## Problems

### 1 (major). The tag is never used up, and the design's table row for "player 2 brought" is wrong about the order

**What the design says.** H5's keep branch does not clear `p2_arrived`. The tag lasts for as long as player 2's +592
equals the tagged room, through every later change to the zone list. §2's table says that when player 2 is brought by
player 1's change (a chapter start or JTG), "their room differs from the tag … so the drop runs".

**What the code does.** The drop is decided before player 2 is moved:
- `coop_prompt_look` calls `coop_zones_new` **before** its `coop_left_behind` return (hle.c:16551-16552) [read].
- `coop_follow`, the bring, runs **after** player 2's turn and only then writes their new +592 (21204, and
  `COOP_SPAWN_SET_ROOM` at 21304-21306). The code itself says so at 15983-15986 ("player 2 isn't brought yet
  (coop_follow, after their turn): their place is the old room's numbers") and at 21296-21299 ("cancelled with the old
  room's zones (coop_zones_new, in their turn of the change's frame)") [read].
- In player 2's turn, r13-26524 is their object (20679-20685). So `0x80123F70` gives the **current slot's** info, which
  is the new room's list. Their +592 is still the old room.

**The result.** Suppose player 2's last own door tagged room X, and player 1's change then takes the game elsewhere and
player 2 has to be brought. At that first look the list is new, +592 is X, and the tag says X, so the set is **kept**. That
set is X's camera, X's coordinates and X's record pointers. `g_coop_zones.list` moves on to the new list, so after the
bring the next look returns early (15954). `p2_bare` is false, so `coop_camera_p2_in` never follows the room's start
either. Player 2's half stays on the old room's camera in the new room until a zone of theirs chooses, or for good where
none does (the user's play 1, 342).

This is the linear-game case the user named: a chapter page or chapter end that has to carry both players [inf for how a
level change routes with the stay switch; the order of operations is read].

**The same hole without a bring** [inf]. Any change to the zone list that does not change +592 keeps a stale set:
- a ticket into the room player 2 is already in;
- the game's own load or continue, which H6 does not list (H6 lists only `hle_coop_state_load`, 28059);
- the wrong-info Tome route in problem 3.

**Fix.** Key the keep on the change itself as well as the room:
- record `p2_arrived_tickets = g_coop_tickets` at H4. `g_coop_tickets` counts every ticket (`notify_room_ticket`, 11299);
- keep the set only if `g_coop_tickets` is unchanged since then and +592 equals the tagged room;
- clear `p2_arrived` in the keep branch too, so the tag is used once.

The use-info and same-pointer worries that led the design away from option A no longer apply. Any later ticket stops a
keep, so a stale keep cannot outlive its own change.

### 2 (major). H8 does not close case D, and it opens a path for players who are together

**Player 1's first look still counts.** H8 returns only when `room_change` is true, and `g_stay_change_on` is cleared at
the same frame's behaviour pass. Player 1's first look in the new room comes a frame later (`!room_change &&
p1_zones == 1`, 17230-17236) and still runs `room_choices++`.
- The two choices are logged one frame apart: "the room's start chose a camera (the room change, frame 2086)", then
  "(player 1's first look, frame 2087)" [read logs/run-20261111-182534-jtg-rooms-r5-state/page-0001.log:2177, 2189].
- So a bare player 2 whom player 1 walks in on still gets `coop_camera_p2_in`'s follow at their next turn. Their set is
  remade from player 1's first-look camera, the cut that D is about [inf from 15405-15416 and 17236].

**It can catch players who are together.** H8 compares player 2's room before the change with the new room only. A
ticket into the room both players are already in passes that test, and `p2_set` is almost always true (`coop_cameras_in(1)`
sets it, 15431). H8 would then refuse player 2 the room's start choice while together, which breaks "players-together
unchanged" [inf: I have not found which scripts ticket into the current room].

**Fix:**
- Require that player 2 was apart when the change began: their +592 is not r13-23276 at `notify_ticket_run`'s entry, which
  is player 1's old room.
- Hold the rule through player 1's first look of that room (clear it when `p1_zones` leaves 1), not only through the
  change frame.
- Better still, record the fact for the change ("player 2 was apart when this room's start began", reset at every ticket)
  and make `coop_camera_p2_in`'s `follow` refuse while it is set. The two kinds of choice then need no separate guards.

### 3 (major). A Tome-item route drops the set of a player 2 who stayed, using player 1's room's zone list. The tag does not cover them

**What the code does** at hle.c:26427-26444 [read]:
- `coop_tome_player(cpu, true)` writes r13-26524 := player 2's object (25524-25531).
- Then `COOP_CHARACTER_INFO(their object)` runs. With the object current, `0x80123F70` takes the **current slot**
  (0x80123F90), which is the room in play, player 1's.
- `coop_zones_swap(cpu, info, true)` runs before `coop_stay_room_in`, which only comes at 26444.

So while the players are apart, `coop_zones_new` sees player 1's room's list:
- it drops player 2's set (`p2_set = false`, `p2_bare = true`);
- the swap trades player 2's zone words with **player 1's room's** zones.

**Who is not protected.** The design says (§6) that "a Tome route's zone swap in the tagged room never drops the set". That
holds only for a player 2 who moved by their own door. A player 2 who **stayed** (player 1 took the door) has no tag. Each
item they use from their Tome while apart gives their half player 1's room camera, which is the same glide as this item
[inf for the effect; the order is read].

**The right order already exists.** `coop_use_start` does it: first `coop_be_player2`, then `coop_stay_room_in`, then
`COOP_CHARACTER_INFO` (17912-17921) [read]. The `tome_item` route at 18491 takes its info before `coop_be_player2` and so
gets player 2's slot.

**Fix.** At 26429-26436, move `coop_stay_room_in` before the info call, as `coop_use_start` does. This is one order for
every route that gets player 2's info, so it is engine-level. Add it to H9's watch list.

### 4 (major). H7 chooses between two wrong cameras, and nothing checks it

**What H7 blocks.** H7 blocks the remake at the end of player 1's scene whenever the players are apart. But `afresh` is
also true when player 2's set is `fresh`, meaning it was copied from the scene's own camera (17129-17141). While apart, that
copy is a camera of **player 1's room's scene**. Keeping it leaves the other player's scene shot on player 2's screen,
against the user's rule "a scene the other triggered never on your screen". Remaking it instead gives player 1's room's
camera. Both are another room's camera.

**When player 2 is bare and apart.** This happens:
- after a state load apart (§7 of the design);
- after a JTG with the stay switch, where player 2 never had a zone choose. In -030834 player 2 stays in 341 for the
  whole run with no "zone chose" line, although trace lines are on [read].

**Fix.** When apart and bare or fresh, run the game's own arrival camera for player 2's room in their set, with their slot
current. That is the sequence the design gives in §7 (0x801EE998 → 0x801EE764 on record 2 → 0x801ECCE0 → 0x801EEB58(2) →
0x801EEA9C, the camera hash from their entry marker or record), and it is the game's own function. As a minimum, exclude
`fresh` from H7. Either way, add a check (problem 5).

### 5 (major). The scripted checks would not prove the parts that matter

**The bare cases are never exercised.** C1-C4 never make player 2 bare while apart, so neither guard D (H8) nor guard E
(H7) is tested:
- In C2, H8's line "appears once", but the design itself says player 2 is not bare there. The line proves only that the
  condition matched, not that anything was fixed.
- A real D test: JTG with the stay switch (player 2 stays bare in 341, as in -030834), then player 1 goes 341 → 343 and
  back. Read p2e across the return, and look for "player 2's camera: the room's start chose one, theirs too" (15413).
- A real E test: the same state, with a scene of player 1's ending in 343.

**The user's own case is called "not scriptable", but it can be scripted.** The design leaves the no-region entry spot to
play. User play 1 is player 2's own door 341 → 342 ("[stay] player 2's change to room 342 runs as theirs", page:4104) from
the chapter-1 start. The C1 pattern covers it: the JTG state, ED_PUT player 2 at that door, ED_PAD2_SCRIPT b. It is the one
arrival the user saw stuck for 680 frames, so it should be a scripted check.

**J1's baseline does not have the lines it expects.** J1 expects "the room's start chose a camera" and "player 2's zone
chose their camera" lines "as in -030834-jtg-coop-gate". That run has neither line (grep finds none), and in it player 2 is
not brought through the route's doors: "[stay] player 2 stays in room 341" at 1805-2549 [read]. Use a baseline that has
the lines, or state what J1 should show in its own terms.

**One door pair only.** Every check uses the 183/180 pair in chapter 1 with player 2 as the chapter's second character.
Nothing tests:
- U4, an entry region whose camera is not the door's camera;
- a ticket of player 2's that is not a door: a way out, the climb (L04/132 in the record at 18476-18479), or a Tome item's
  room change (U3);
- another chapter or form.

For an engine-level rule, at least one run should be in another chapter with another door.

**The kit's ARRIVAL rule would flag normal cases:**
- "Eye within 5 units of the other half's eye" is true at every correct together or rejoin arrival. C3 expects both
  players on Camera02 by the same door, and J1 expects p2e = E at the start rows. Flag equal eyes only while the two
  players' rooms differ **at that row**.
- "More than 300 units of travel in 60 frames" flags any follow camera behind a player who walks or runs at once. Compare
  each half's eye with the new room's arrival camera or rail (the log readers' D), or with a solo baseline of the same
  door, not with how far it travelled.

### 6 (minor). Player 2's arrival scene apart can still give player 1's half player 2's room camera

**The path** (hle.c:18525-18526) [read]. `coop_p1_scene_end_camera` copies the lent set, which is player 2's, into
player 1's kept set when:
- the arrival scene ends;
- `g_coop_cameras.lent` is true;
- `g_coop_p1_own_camera != -1`.

**Why the stay switch opens it.** With the stay switch, `g_coop_p1_own_camera` is never set to the arrival script, because
17059 gates that with `!coop_stay()`. It stays 0, and 0 passes the `!= -1` test. If player 2's door apart leads into a room
with a first-time arrival scene (the Tome rooms, L03/1351 in the record at 124), player 1's half would take that scene's
last camera, which is in another room.

**Evidence.** No stay run in the logs shows the path's log line [read: grep of the 2026-11-11/12 stay runs]. So this is
[inf: reachable by the code].

**Fix.** Add the apart test to that condition. This is the item's other direction, player 1's half at player 2's arrival,
and the design's "player 1's half unchanged" claim does not cover it.

### 7 (minor). Only camera mode 2 is designed and checked

- The default is `mod_p2_camera = 0` (settings.c:64) [read].
- In modes 0 and 1, `coop_camera_step` returns before the step (16072), and camera.c places player 2's eye from their
  position (1747-1782).
- So the glide does not happen in those modes. But the third-person wall test uses player 1's room's collision while the
  players are apart (code readers §5f). That is wrong for the whole time apart, starting at the arrival.
- Every scripted run uses the settings copy with mode 2 (settings.ini in the runs read).

The design should say that modes 0 and 1 are untouched, and add one C1 run in mode 0 to confirm no regression and to
record §5f. The rule must hold in every mode.

### 8 (minor). The H7 helper's description mixes nodes and objects

- r13-26524 holds an **object**: `0x80123F70` compares it with the object argument at 0x80123F94, and the turn writes
  `p2+52` there (20681).
- `coop_left_behind` reads `g_coop_turn_p1 + 52`, which is a node's object.

"Player 1's node from `g_coop_turn_p1`, or r13-26524 outside a turn" would read +52 of an object outside a turn. Specify
r13-21492 (the node) outside a turn, or read r13-26524 +592 directly.

### 9 (minor). The teleport note must say whose ticket it is

§6 says "Bring X to Y" should go through 0x80152614. Only a ticket asked as player 2's (`g_coop_room_p2`, 11309-11310)
gets the bracket and its tag. A ticket asked as player 1's into player 1's own room would place and cut player 1 again,
and the other player's screen would be disturbed. Bringing player 1 to player 2 is player 1's ordinary change. State both
directions there.

## Summary for the designer

| # | Severity | Change |
|---|---|---|
| 1 | major | The tag also holds `g_coop_tickets`, and it is used once. The "brought" table row is wrong: the drop is decided before the bring writes +592 |
| 2 | major | H8: "player 2 was apart at the ticket", held through player 1's first look, or one apart flag on `follow` |
| 3 | major | 26429-26436: `coop_stay_room_in` before `COOP_CHARACTER_INFO`, as 17912-17921 does |
| 4 | major | H7: re-cut player 2's own arrival camera when bare or fresh apart. At least exclude `fresh` |
| 5 | major | Checks: a bare-apart D and E run, the 341 → 342 no-region door scripted, a J1 baseline that has the lines, a second chapter or door, and an ARRIVAL rule with no false flags |
| 6 | minor | 18525: apart guard on `coop_p1_scene_end_camera` |
| 7 | minor | Say modes 0/1 are untouched; one C1 run in mode 0 |
| 8 | minor | H7 helper: node versus object |
| 9 | minor | Teleport note: whose ticket |
