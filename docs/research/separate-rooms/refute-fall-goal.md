# Refuter: design-fall.md, against the goal and both players

The lens: both directions, every kind of frame (turn, change, scene, Tome, close-up, fight), solo and together unchanged,
engine-level, and whether the checks would really reproduce the fall and then prove the fix. I read design-fall.md and
the three readers' reports (fall-ground-readers.md, fall-host-readers.md, fall-rooms-readers.md). I checked them against
build/ed-db/disasm.txt, runtime/host/hle.c (the uncommitted tree; line numbers drift) and the recordings. Nothing was
edited, built or launched.

Marks: [read] means seen at the address, file:line or log row given. [inferred] means reasoned, not seen.

## Verdict: needs changes

- **F1 itself holds up on what I could check.**
  - The getter really does split player from non-player at 0x80123F94-0x80123FA0, and +0x24 is the `lwz r0,-26524(r13)`
    [read].
  - The stamp 0x80154D30 runs inside 0x80152298 (0x80152298 → 0x80154C94), so it falls inside the stack window [read: a
    call-graph walk].
  - One of the non-step callers I followed, the hit's line test (0x8006255C → 0x800324B8 → 0x800323A8 → 0x80123F70), passes
    the hit node's own object (0x800324C8, 0x800324F0 r22 = 0x801F633C(node), 0x80032738). Inside r6_hit_room that object
    is never player 1 as "the player", so F1 doesn't change hits [read; inferred: that player 1 is never the hit node in
    that bracket, from r6_hit_room's t rule, hle.c ~24711-24716].
- **Four things don't hold:**
  - the guard G;
  - the test hole the checks rest on;
  - the claim that the checks prove the user's bug fixed;
  - the ranking against C2, which fits "could not move" at least as well and gets no repair and no check.

## Problems

### P1 (blocker): G never fires on the fall that was recorded, and would put the body back under the floor

- **G's rule.** G keeps "the last still place: a frame with |d| < 1 on every axis" and fires at z < last still z - 2000.
  coop_follow's together rescue uses the same rule (hle.c:21381-21388, `still` then `stood_z = z2`, `fallen = !(z2 >=
  stood_z - COOP_FALLEN)`) [read].
- **The one recorded fall of this kind breaks it twice** (logs/run-20261112-160254-cam-lf-load-p2door/both.tsv) [read]:
  - **The still frames come back on every slide.** Rows 2409-2413 give dx 0.4-0.5, dy 0, dz -0.6/-0.7. So do 2445-2450,
    2471-2477 and 2498-2504. Each slide's start is "still", so the last still z follows the body down. It is never 2000
    above the body.
  - **The fall ends.** From frame 2532 the body rests at z -704.8, 728.5 below where it stood (23.7). It stays there until
    the end of the run (3507). The design says "falls for ever" (sections 1 and 2), and that is wrong for this run.
- **Where it would put them.** Even if G fired, "the last still place" would be a slide spot under the floor.
  0x80130EF0 rays down from there along gravity (design 4.1), so it would place them on the under-geometry again
  [inferred].
- **So 6.4 fails as written.** The expected line "fell out of room 182 while apart ... put back" would not appear for a
  fall of this shape.
- **Fix: tell "under my own floor" apart with the game's own collision, not with a still-z history.**
  - Latch a stood place only on a long still run, for example 30 frames. The slide runs here are 5-7 frames long.
  - Then test from above: camera_collide_object(cpu, object, at + (0,0,H), at - (0,0,H')) against their own room. If the
    first floor from above is more than a body height over z, they are under their floor.
  - Fire on that, put them back at the latched place, and drop the 2000 threshold for the apart guard.
  - Apply the same latch to coop_follow's together rescue, or say plainly that it has the same blind spot.

### P2 (blocker for the checks): ED_FALL_HOLE as specified may not sink anyone, and it is the only test of player 1's direction

- **How the hole works.** It runs one in-place step against the other room per pad read. The game's own step for the
  same body follows in that frame's behaviour pass (pad read, then the ticket tick at 0x800083EC, then the pass at
  0x800083FC) [read: fall-ground-readers.md 2.1].
- **What the readers found about one wrong frame.** It drops a grounded body only about 1 unit, and the next own-room step
  catches it. The fall speed resets on that floor every frame [fall-ground-readers.md 2.4: "One frame against the wrong
  room drops a grounded body only about 1 unit, which the next frame's own floor catches"].
- **So the hole alternates** one wrong step and one right step every frame for 20 frames. +40 goes back to +36 each frame,
  and the body may never sink [inferred].
- **The design's evidence for "the same mechanism" isn't a step at all.** The 160254 drop is the old ED_PUT write: frame
  2401 has dx -134.3, dy -683.1, dz -72.7 in one frame, a direct place 73 units under the floor [read both.tsv]. The
  sweeps that fell (172914, 173348) were stepped against the wrong room every frame. So the reproducible shapes are:
  - a window that spans the pass (many consecutive steps with no own-room step in between);
  - a direct write;
  - a large push-out.
- **What rests on the hole.** 6.2(b), both holes of 6.3 (the only check of player 1 as the stayer) and 6.4 (G) all depend
  on it. If the hole doesn't sink anyone, "fix off: the fall reproduced" fails. Then "fix on: gone" shows nothing.
- **Fix: make the hole C1's real shape.**
  - Keep the bracket open across the behaviour pass for N frames, with the named player the player and the other room
    current. Close it at the pass's return. Or run the step N times in one call.
  - Make "fix off reproduces" a hard gate before any fix-on run is read.
  - Give player 1 a hole of the same strength. ED_COOP_P2_STEP exists only for player 2.

### P3 (major): no check reproduces the user's bug, and the play checks pass on today's build

- **What each check shows.**
  - 6.2(a) uses ED_COOP_P2_STEP, a test-only path. Its own comment says that with it on, neither player can open a door
    (hle.c:25112-25115) [read]. The user's build never sets it [fall-host-readers.md 2].
  - 6.2(b) and 6.3 build exactly F1's precondition.
  - Together these prove that F1 does what F1 says. They don't prove the user's fall is gone.
- **The play checks can't tell the builds apart.** D1-D4 pass when p2z stays at or above 70. Today's build already passes
  that in the scripted layouts (033738: p2z 78.4 throughout) [read: fall-rooms-readers.md 6]. A D-run with no fall and no
  `[floor]` line tells nothing either way.
- **Fix the wording.** Call F1 what it is: hardening plus a probe for a cause not yet found. Drop "closes the whole class
  at its one door".
  - The class that ends below its own floor also includes things F1 doesn't touch: direct host writes, the no-object
    current-section readers (0x80130EF0, 0x80134FB8; its callers 0x80051DC8, 0x8007076C, 0x80135258 and 0x801ECEC8 aren't
    classified [read]), C2 and C3.
- **Fix the probe so it reaches the user.**
  - play.bat sets no ED_COOP_LOG [read play.bat:13-17]. So `[floor]` lines kept behind ED_COOP_LOG never reach the log of
    a normal play.
  - play.bat keeps only one previous log (`move /y logs\play.log logs\play-prev.log`) [read]. That is how the bug's log
    was lost: logs/play.log of Nov 12 03:19 is a later, co-op-off session [fall-host-readers.md 6].
  - Print the `[floor]` and "fell" lines without a switch (they are rare, and rate-limited), and keep logs by date.

### P4 (major): C2 fits "could not move" as well as C1, and nothing rescues it

- **Under C1 the body can still move.** In 160254, at z -704.8 under the floor, player 2 moved from (14.9, -4.5) to
  (58.4, -132.2) on stick sy 127, frames 3007-3107 [read both.tsv].
- **Under C2 the body cannot move at all.** 0x80123F70 gives 0 and the step stores its FROM (0x80133488-0x8013349C). Their
  half also draws no room, so they stand in the void [read: fall-ground-readers.md 2.2, 5].
- **Nothing rescues a C2 freeze.**
  - coop_stay_apart keys on the host's g_room_slot_room (room_slot_of, hle.c:13861-13874). That word is learned only while
    the record is loaded (room_slots_frame, ~13665) and is never cleared when the record goes dead.
  - So coop_follow keeps taking the apart branch and returns (hle.c:21413-21421) [read]. A dead record with the room still
    remembered is a freeze for good, with no bring.
- **The design has a detector only, with no reproduction and no repair.**
- **The path no apart recording has run:** the room re-run on the room in play.
  - The Tome giving the room back (0x80110DA8, bl 0x80152298 at 0x80110E88) and the reloads (0x801556A8) run the change,
    tail included, again while the stayer is in +4. The readers call the outcome [inferred] (fall-ground-readers.md 5).
  - Not one apart run's `[room] change` line has a caller other than the door runner 0x80152658 or the jump-to-game loads
    0x800AE76C/0x800ADC28 [read: a scan of every page-0001.log with "[stay] player 2 stays"].
- **Fix.**
  - Add a check that forces C2: a test switch that skips one put-back.
  - Add a repair when their record has been dead for N frames:
    - the put-back retried at the pass's entry, under the same three gates;
    - else forget g_room_slot_room for that slot, so coop_follow's bring runs.
  - Add play checks with player 1 opening and closing the game's Tome in 181 while player 2 waits at 182's door end.

### P5 (major): "every kind of frame, both directions" isn't covered by section 6

These are missing from the checks, and each is a frame the user could have had in this exact spot:
- **Player 1's other scenes in 181.** The button scene 290 (prompt, cutscene, Camera03) and the teleport parts (2273). Only
  2033 was ever run [read: fall-rooms-readers.md 3, 6].
- **Player 1's second change with player 2 still in 182.** The teleport 127 to 184, which loads into a slot with a stayer
  present, and 126 back into 182, a return into the stayer's slot. The readers listed both as not covered
  (fall-ground-readers.md 6). The design dropped them.
- **Player 2's own close-up apart.** The statue's examine and words (Region03 at (-790,1234), script 1298, Camera13), on
  the very dais that 181's teleport region lies under.
- **A fight in 182 apart.** 182 has 36 SpawnMkrs [read: fall-rooms-readers.md 6]. D4 has player 2 swing at an empty dais
  only.
- **The user's layout with the roles exchanged.** Player 2 takes 182's Region02 and player 1 stays at 182's z 78/109 door
  end, about to follow.
  - 6.3 keeps player 1 at 181's entry at z 0. That is a no-182-floor spot, but not the user's room or floors.
  - So the fall-prone stayer position for player 1 (182's high floors, with 181 in play) is never tested.
- **One other room pair.** For example 341/343 (where the tool fall happened) or 180/183, to show the fix is not tied to
  182 and 181.

### P6 (minor): C3's "can't press down" was argued in one direction only

- **What the design argued.** In the user's direction, the room-in-play bodies (181's, at z 0) lie below the stayer (182
  at z 78/109), so a push points up.
- **The reverse is not argued.** Player 2 in 181 at z 0, with 182 in play and player 1 staying on the dais. Player 2's
  step collides with 182's bodies at 181's coordinates [read 0x801308E0-0x80130950: node room == r13-23276].
  - Those bodies are player 1 at z 109 and the 182 creature at (-767, 1212, 111), the statue's place [fall-ground-readers.md
    2.6].
  - Their place in 181 is 181's teleport region (-760, 1064) r 219, where player 2 walks. That is a body above the stayer,
    and a push that can point down [inferred; 0x80130504's shape is not read].
- **Fix.** Put this layout in the C3 probe's list. Don't call F2 unneeded until it has run.

### P7 (minor): the interactions table contradicts the stack window for R4

- **What the table says.** In R4, "between the runner's entry and the change", F1 gives player 2 their old room.
- **What the window covers.** It is 0x80152298 and 0x80152614 "to the end of the latter" (hle.c:17305-17306,
  ROOM_CHANGE_FROM/TO) [read]. The whole runner is inside it, so F1 passes through there.
- **What follows.** F1 only acts in frames outside the runner where the bracket is open (H1b). That is harmless, but the
  text should say so.

### P8 (minor): the install gate and the solo check

- **Gate the install on the run's own decision.** Use the one that prints "[rooms] separate rooms off for this run: co-op
  is off" (frontend.cpp rooms_follow_coop), not getenv("ED_ROOM_SLOTS")/("ED_COOP_STAY").
  - play.bat sets both for solo players too [read play.bat:13-14].
  - With that gate a solo run carries no intercept at all, which "co-op never touches solo" asks for.
- **Make the solo check a real one.** Play the same 182 → 181 door solo, not only jtg, and confirm there is no install
  line and no `[floor]` line.

### P9 (minor): the 6.3 setup

- **The put is already inside the door's sphere.** The put at (-2420, 1068, 0) is 248 from Region01's centre (-2668, 1068),
  and the radius is 261 [read: fall-rooms-readers.md 3]. So "walk -x into it" never happens; the door asks on the put.
- **The put can land inside player 2's own change.** Player 2's first door waits for 2033's end, about 230 frames after
  player 1's change, and then runs a 62-tick ticket. So the put at 4500 can land in the middle of that change.
- **Fix.** Gate the put on the `[stay] player 2's change ... runs as theirs` line having appeared, or on p2room being 181
  in both.tsv.

## What would make the design hold

1. **G:** a long still latch plus a from-above floor test against their own room (P1).
2. **The holes:** spanning the pass, with "fix off reproduces" as a hard gate, and one for each player (P2).
3. **C2:** a forced check and a repair when the record has been dead for N frames (P4).
4. **The probe:** unconditional and kept with dated logs, with F1 described as hardening plus probe (P3).
5. **Section 6:** add the missing frames: 290, 127, 126, the statue examine, a fight, the roles exchanged, and one other
   room pair (P5).
