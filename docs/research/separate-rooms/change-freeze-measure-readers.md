# The other player's brief freeze at a room change: measured from the recordings

Reader report, read only (no code changed, nothing built, the game not launched). Item: with the players apart, when one
player changes room the OTHER player's picture pauses briefly (the user: player 1 fighting in one room, player 2 goes into
another, player 1's screen pauses for about a second, not black). Sources: every run in logs/ from 2026-11-11/12 with
both.tsv's p1room/p2room columns and a "[stay] ... stays in room" line (54 runs, listed at the end), plus two solo runs and
the jump-to-game "together" runs. [read] = seen in a log row, a log line, code or disassembly (file:line or address given);
[inf] = inferred.

## Summary

1. **Game frames stop; this is not a host lag spike.** At every room change, solo, together and apart, the game loop runs
   ONE iteration across 3 to 10 retraces (50 to 168 ms) instead of 1. Within that time nothing in either room advances:
   the stayer's walk and camera move exactly one frame's step across the gap, and the lost frames are not made up later.
   The stayer's half shows the same picture for that time. That is the "slight freeze frame".
2. **Solo has the same stop and the same length.** Solo fresh loads cost 3 to 5 retraces (50 to 84 ms), the same as
   apart (3 to 7, mostly 4 to 6). Solo hides it because the screen is black at that moment (the door fade). Co-op shows
   it because R4 deliberately keeps the stayer's half out of the fade (g_fade_mover, hle.c:3256, 3341-3349, 11314).
3. **Most of the cost is the fresh unpack and the new room's first frame.** A change into a room already resident in the
   other slot ("no unpack", or rejoining the other player) costs 2 retraces (33 ms). A fresh load costs 4 to 10. Apart
   fresh changes into room 180 (0xB4) cost 1 to 3 retraces more in builds from R5 on than in solo or R4 builds. Rooms
   0x157, 0x158 and 0xB7 cost the same apart as solo.
4. **A second, longer stop is possible for an idle stayer, but it has not been measured for a fighting one.** During the
   62-tick ticket the game holds everyone but the asker (event 102, node+28 bit 8). In one run
   (run-20261112-033642-p1-to-181), an idle stayer pressed the stick 11 ticks before the change and did not move until
   after it. A stayer already walking was not stopped in any run. The user's case, the stayer attacking and an enemy
   fighting during the other's ticket, is in no recording. That makes it the main open question: event 102 might hold
   the stayer's new actions and/or the enemy for about 1 s, which would match "a brief second" better than 100 ms.

## How the recording measures (what a row is)

- [read] both.tsv is written by both_record (hle.c:17584-17640), called from the PADRead hook (hle.c:4234) inside
  `if (!monitor)`. Rows are de-duplicated on `frame = cpu->timebase / 675000` (hle.c:17601-17604). `ms` is
  timespec_get (wall clock, ms) (hle.c:17606-17608). So there is one row per game pad read, normally once per game-loop
  iteration. PADRead is reached from 0x8013D9F4 ("[coop] PADRead fills 0x805880A0 (from 0x8013D9F4)" in every run).
- [read] The guest timebase follows the wall clock. interrupt_poll converts wall time to VI retraces (interrupt.c:433-486)
  and adds the ticks per retrace (interrupt.c:511-514). A single gap between polls is capped at WALL_MAX_STEP_NS = 100 ms
  ("after a stall, drop time, don't race", interrupt.c:70, 446-447).
- So [inf]: a row step of dF frames with ms ≈ 16.7·dF means one game iteration spanned dF retraces of paced time (the
  game thread busy or blocked in guest time, or host stalls each under 100 ms). A step with ms well above 16.7·dF would
  be a host stall over 100 ms. Rows with ms > 16.7·dF do occur, but only away from changes (for example
  run-20261111-214942-r6g-c1-far 3808->3814, +6 frames in 426 ms, and run-20261111-214044-r6f 3805->3811, 236 ms).
  **At every change ms ≈ 16.7·dF.**

## Measurements

Per change: the pad row before the change frame, the next pad row, and the retraces and ms between them. Kind: "fresh" =
"[rooms] slot swap (a fresh load ...)"; "resident" = "(the room is already in the other slot: no unpack ...)"; "?" = no
slot-swap line (a change into the room the other player already holds). All [read] from both.tsv and page-0001.log.

### Solo (no player 2, p2room 0)

| run | changes (room: +retraces/ms) |
|---|---|
| run-20261112-030139-jtg-solo-returns | 0x157 +3/50; 0x158 +4/66; 0xB4 +4/67 (x4: 2884, 3624, 4424, 5224); 0xB7 +5/83 (x4: 3284, 4024, 4824, 5624) |
| run-20261112-030651-jtg-solo-gate | 0x157 +3/50; 0x158 +4/66; 0xB4 +4/67 (x3); 0xB7 +5/83, +4/67 |

### Together (player 2 brought along by coop_follow)

| run | changes |
|---|---|
| run-20261111-171338 / 171718 / 172602 -jtg-rooms-r4-state | 0x157 +3..4/51-67; 0x158 +4/66; 0xB4 +4/67; 0xB7 +5/83 |
| run-20261111-182534-jtg-rooms-r5-state | 0x157 +4/67; 0x158 +5/84; 0xB4 +5/84; 0xB7 +7/117 |

### Apart (one player stays)

| run | change (who, room, kind) | +retraces / ms |
|---|---|---|
| run-20261111-221141-rooms-play (user) | F1843 p1 -> 0x156 fresh (p2 stays) | +4 / 66 |
| run-20261111-222138-rooms-play (user) | F1957 p1 -> 0x156 fresh (p2 stays) | +4 / 67 |
| run-20261111-191306-play-r5c (user) | 1838 0x156 +4/67; 4380 0x157 +3/50; 4641 0x158 +5/84; 9485 p2 -> 0x156 +4/67; 9609 0xB4 +5/83 | |
| run-20261111-184254-play-r5b | 31548 0x156 +5/84; 33324 0x157 +3/50; 33586 0x158 +5/83 | |
| r4 runs 171448/171514/171826/171852/172710 | p2 -> 0xB4 fresh, p1 stays | +4/67, +3/50, +4/66, +4/67, +4/66 |
| run-20261111-182644-rooms-r5-s1-p2door | p2 -> 0xB4 fresh | +6 / 100 |
| run-20261111-182711-rooms-r5-s2-p1door | p1 -> 0xB4 fresh | +7 / 116 |
| r6 runs (194435..214942) p1 or p2 -> 0xB4 fresh | | +5..+10 / 83-168 (204019: +10/168; 204924: +8/134; 195206: +8/134) |
| run-20261111-205757-r6c-walk3 | p1 -> 0xB4 fresh, p2 walking | +6 / 100 |
| run-20261111-221649 / 221713 / 221737 / 221859 (r4b) | 0xB4 fresh | +4/67, +5/83, +5/83, +5/84 |
| run-20261111-223853 / 223919 / 223944 (r4c) | 0xB4 fresh | +5/83, +6/100, +6/100 |
| run-20261112-033038-follow-test | 3882 p1 -> 0xB4 +5/84; 4522 p1 -> 0xB7 +5/84 | |
| run-20261112-030834-jtg-coop-gate | 2084 0x157 +3/51; 2484 0x158 +4/67 | |
| run-20261112-033642 / 033738 | p1 -> 0xB5 fresh | +6 / 100, +6 / 101 |

### Resident or rejoining (no unpack)

All +2 retraces / 33-34 ms: 171514 and 171852 F4272 (resident); 182711 F4363 (resident); 171826 / 172710 / 182644 /
221649 / 223853 F4362 (p2 back into player 1's room); 033038 F4062 and F4712; 033738 F5052; 223919 F4363 (+3/50).
Changes into the other player's room during their scene (221141 F2291, 222138 F2452): +1 (no lost retrace).

### The ticket window itself

[read] Every apart run: during the 62 ticks from the ticket to the change, rows step +1 frame / 16-17 ms (apart from
isolated +2 rows that also appear away from changes). The ticket's ticks run at full rate, and so does the stayer's walk
(walk3: p2y falls by about 3.5 a row from 3762 to 3822; r4b-c7: about 3.5 a row from 3816 to 3823).

## Which frames the stayer loses (game frames, both players)

- [read] run-20261111-221737-r4b-c7-apart-finish. Player 1 goes to 0xB4 and player 2 stays in 183, walking (stick
  0,-127). Rows 3816-3823: p2y 2603.9, 2600.4, 2596.9, 2593.4, 2589.9, 2586.4, 2582.9, 2579.4 (3.5 a row). Then
  3823 -> 3828 (+5 retraces, 83 ms): p2y 2579.4 -> 2575.9, **one step**. Player 2's camera eye p2ey 2088.2 -> 2087.2,
  also one step (about 1.05 a row before and after). From 3829 on, 3.5 a row again. So four game frames of player 2's
  walk and camera are not run, and the picture holds for 83 ms.
- [read] run-20261111-204125-r6-c3-p2swings. The stayer (player 2) is idle while their camera sweeps: p2ex 3818-3823
  -737.5, -739.2, -740.8, -742.4, -744.0, -745.6 (1.6 a row). 3823 -> 3829 (+6 retraces, 100 ms): -745.6 -> -747.2, one
  step.
- [read] run-20261111-205757-r6c-walk3: the same, p2 2616.0 at 3822, 2609.0 at 3829 (two 3.5 steps across 3822->3823
  and 3823->3829).
- [inf] The game is frame-stepped (positions move a fixed amount per iteration), so every actor in both rooms (the
  stayer, enemies, effects, the camera) gets one step for the whole gap. Per room or slot makes no difference: the one
  game thread runs both rooms' turns in the same iteration.
- [read] Pictures: run-20261111-205757-r6c-walk3/shots/000010.jpg, taken during the gap. Player 1's half is black (the
  door fade), player 2's half shows their room. Stayer's half not black, mover's black: this matches the user's "not
  black, a slight freeze".

## Where the iteration's time goes

[read] The change runs synchronously inside the main loop. The call chain in every "[room] change" line is 0x80005418 ->
0x80018704 -> 0x800083F0 -> 0x801558F8 -> 0x80152658 (door changes) or 0x80020420 / 0x800206D8 (other entries).
0x80152298 does, in order (ed dis 0x80152298):
- 0x80152340 0x801A0EAC and 0x801A05EC(10,1,0): the sound fade.
- 0x80152370-0x8015237C: a spin on GXReadDrawSync (0x8021AA34) until token 0xBEEF.
- 0x80152388: the load 0x80154C94.
- 0x80152398: OSReceiveMessage (0x802019D4) with block = 1, the main thread waiting on the loader thread.
- 0x8015239C: GXInvalidateTexAll (0x8021D244).
- 0x801523A4: 0x80151FDC, the arrival (events 61 to the old room's nodes and 62 to the new room's; doors-readers.md:124).

Host side of each step:
- [read] Draw-sync token: notify_GXSetDrawSync commits the token at once on the game thread (hle.c:4655-4656 ->
  interrupt.c:314-316 commit_pe_token_now), so the 0xBEEF spin ends at once [inf].
- [read] GXInvalidateTexAll is a no-op in the renderer (GXTexture.cpp:350-352), and gxcore's tmem_invalidate is called
  only from tests (texture.cpp:294; tests/gxcore_tmem_test.cpp:216). No texture flush.
- [read] Disc reads finish at once. Their callbacks wait DVD_CALLBACK_VISITS of the asking thread (hle.c:455-470,
  1215-1243).
- [read] HD texture-pack files are read on a loader thread the first time a texture is drawn, with the original shown
  until ready (texpack.cpp:4-5), so that is not a stall.

[read] Frame stamps in the logs split the gap in two (log lines carry the guest timebase frame):
- the change at F;
- "after the change's tail" at F to F+2 (solo 2084->2084 ... 3284->3286; apart 3823->3825, 3826->3828, 1843->1844);
- the next frame-stamped line from the same iteration ([detail] frame N at the draw's character detail, or
  room_slots_frame's "[rooms] frame N" at section 3's room draw, hle.c:13617-13658): F+1 to F+3;
- the next pad row: F+3 to F+7.

So about 1-2 retraces go to the main thread blocked on the loader (unpack) and 2-4 to the rest of the iteration (the
arrival, room scripts, the first draw of the new room, and the present) [inf]. No run had DOL_GX_TIME=1 ([gx-time]:
aurora_backend.cpp:451-455, 3599; it reports wait = the game thread held at its present), ED_DVD_LOG or a host timer
around 0x80152298, so the split inside the second part is not measured.

[inf] Why apart costs 1-3 retraces more for 0xB4 in R5+ builds: two rooms are drawn on the first frame ("[rooms] frame
3826: BOTH rooms drawn"), and there is more host work and logging at the change (sound gains, the enemy roster, [stay]
lines). Unproven: the solo-vs-apart difference is within log noise for 0x157, 0x158 and 0xB7.

## What each player sees, both players

- **The mover** (whoever's ticket it is: g_coop_room_p2 picks player 1 or 2, hle.c:11308-11314): their half is in the
  game's fade (black from about tick 60 to the change, then a 10-tick fade-in; both.tsv `fade` 59..0 then 9..0). The
  stop happens while their half is black, as in solo.
- **The stayer**: the fade quad (lr 0x801E7F60) is marked out of the stayer's half from the ticket until the fade-in's
  ticks are spent (hle.c:3332-3349). So the stayer watches a live picture that stops for 3-10 retraces at the change.
  This is symmetric: player 1 stays for player 2's changes (r4/r5-s1 runs), player 2 for player 1's (user runs, r5-s2,
  r6), and the numbers are the same either way.
- **During the ticket**, both play at full rate (no row gaps). The stayer is under the game's event-102 hold (node+28
  bit 8, set by the ticket's busy set 0x801556CC(4,1) -> 0x801F5620). The host answers the held ask 0x801F68D4 with 0
  for the other player during coop_room_change_on (readers.md:124; hle.c coop_node_held_ask) and lets the stayer go at
  the change's tail with 0x801F6828(stayer, 0) (notify_room_slots_tail, hle.c:14290-14320, "[stay] player N's hold from
  the ticket let go").
  - [read] A walking stayer keeps walking through the ticket (walk3, r4b/r4c/r6e/f/g-c7, 68-80 rows of held stick, no
    stalled frame).
  - [read] In run-20261112-033642-p1-to-181, player 2 (idle, anim 15) pressed the stick (0,127) from 4750; the change
    was at 4761. They stayed at -758.0,-349.0 with anim 15 through 4771, and the walk ramp began at 4772-4773 (anim 2).
    That is about 22 frames from the press to moving: 11 of them in the ticket, 6 retraces in the change, and the rest
    the walk's own start ramp [inf].
  - [inf] So an idle stayer may be unable to START an action during the other's ticket, which lasts up to about 1 s.
    For a fighting stayer, that would read as the fight pausing. Unproven: no recording has a stayer attacking, or an
    enemy acting, through the other's ticket. The [enemy] roster is logged only every 120 frames (hle.c:13642-13648).

## What the host does today about it

Nothing aimed at the stop. R4 made the stayer's half skip the fade (g_fade_mover); that is what makes the stop visible.
coop_node_held_ask and the tail's let-go keep the stayer able to walk. The change itself is the game's own,
unchanged.

## Candidate approaches (each co-op apart only; none is a decision)

A. **Measure first (no behaviour change).** Rerun one apart door (r4b-c7, the stayer walking) and one solo door with
   DOL_GX_TIME=1 and a host timestamp log (ns) at 0x80152298's entry, at the OSReceiveMessage return (0x8015239C), at
   the tail (0x80154B54 at lr 0x80152434, already hooked by notify_room_slots_tail), at room_slots_frame, and at the next
   PADRead. That splits loader vs scripts vs draw vs present wait. Add a stayer-attacks-during-the-ticket run with
   ED_COOP_ENEMY_LOG per frame for the 70 frames around the change. Touches: logging only.

B. **Unpack ahead, during the ticket, using the game's own loader.** The change already takes 2 retraces instead of
   4-10 when the room is resident ("no unpack", 0x80154D64/0x80154D94 path). The game has a "next room" preload (fn71
   queues a room, 0x80154EC4(0) asks the loader; readers.md 'have'). But with two players apart, both slots are taken
   (the stayer's room and the mover's current room, which is still in play until the change), so a preload has nowhere
   to go without a third slot. Touches: the slot table 0x8058AFC0 (count r13-23292 = 2), the 48 MB RAM layout (slot 1 at
   0x83000000), room_slots_return, the R6b second route graph. Large; for the refuters. Gain: about 2-6 retraces at most,
   and only if the loader part dominates (unmeasured).

C. **Speed the host side of the first frame** (if A shows present wait or decode dominating): this is renderer work,
   texture decode or hash and pipeline builds of the new room. Engine-generic, but it changes solo timing too, so it
   must be gated or argued as not changing play. Touches .deps/GXRuntime. Gain unknown until A.

D. **Let the stayer's half keep moving without new game frames.** It cannot: the stayer's world is the same game thread.
   Holding or re-presenting a half is a still picture, which the user's rule forbids ("Don't: hold a half on a still
   picture", readers.md 'Holding a half'). Listed so it is not proposed.

E. **The ticket hold (if the run in A shows it).** Have the event-102 hold not apply to the stayer and their room's
   enemies while apart: the busy set's broadcast 0x801F5620 from 0x80155720, or the stayer's node+28 bit 8, cleared or
   answered for the stayer when coop_stay_apart and the asker's room differs from theirs. Touches coop_node_held_ask
   (which today answers only 0x801F68D4) and the tail's let-go. It must keep the together case (the follower is brought
   along) unchanged. Check docs/coop-failed-attempts.md:542-559 (doorhold attempts, the grab) first.

## Open questions

1. Is the user's "about a second" the 66-168 ms change stop, or the up-to-62-tick ticket hold acting on an attacking or
   idle stayer and/or the enemy? No recording covers a stayer fighting through the other's ticket. The user's own two
   play runs only have the stayer idle (stick 0) at player 1's changes, and player 2's changes there are rejoins.
2. Within the 3-10 retraces: how much is the loader thread (unpack, OSReceiveMessage) vs the first frame (scripts,
   GX decode, present wait)? Needs A.
3. Do enemies in the stayer's room honour event 102 from another room's ticket (0x801F5620 reaches "everyone but the
   sender", hle.c:22017-22020)? If yes, the stayer's fight pauses for the whole ticket.
4. Why do R5+ builds cost 1-3 more retraces than R4 builds for 0xB4 apart, when solo stays at 4? Is it the second
   room's draw or host logging?

## Runs read

Separate rooms: run-20261111-164408-rooms-r3-t1-stay, 171338/171718/172602-jtg-rooms-r4-state, 171448/171826-rooms-r4-t1-p2door,
171514/171852-rooms-r4-t2-p2door-p1follows, 172710-rooms-r4c-t1-p2door, 182534-jtg-rooms-r5-state, 182644-rooms-r5-s1-p2door,
182711-rooms-r5-s2-p1door, 184027-jtg-rooms-r5-jtg-stay, 184254-play-r5b, 191119/191217-rooms-r5c-s1-orbs(40), 191306-play-r5c,
194435..214942 (r6, r6b, r6c walk1-5, r6d, r6e, r6f, r6g), 221141-rooms-play, 221649/221713/221737/221859 (r4b),
222138-rooms-play, 223853/223919/223944 (r4c), run-20261112-030834-jtg-coop-gate, 033038-follow-test, 033642-p1-to-181,
033738-p2-follows-181. Solo: run-20261112-030139-jtg-solo-returns, 030651-jtg-solo-gate. Note: the r6 runs 194435,
195105, 205125, 205542, 205647, 210014, 210144 and 214817 ran at about 20 fps throughout (rows +3 frames each), so their
change gaps are not comparable.
