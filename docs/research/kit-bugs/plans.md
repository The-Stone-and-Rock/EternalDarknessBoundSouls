# The kit's likely co-op bugs: fix plans and test plans (2026-11-16, read only)

Five bugs the kit (`ed catalogue`, `ed patterns`, the hook map) found by reading. For each, a reader traced it in the code and
drafted the smallest fix and a test from a fresh Jump-to-Game boot; a skeptic then checked both against the code and the run
conventions. **Nothing here has been run or changed in hle.c.** The skeptic's corrections are listed after each plan and
override it where they differ. The run scripts beside this file (build-run-*.sh) follow the corrected test plans; each is
syntax-checked only. Every run: ED_RUN_AWAY=1, fresh Jump-to-Game states made on the current build, never `ed test`.

| bug | likely real | fix size | test |
|---|---|---|---|
| fn151's music duck by player 2 apart (hle.c:15084's return address) | likely | one character (0x80164DD8u) and a comment | build-run-duck.sh |
| ed113 / ed248 in a room's arrival script run for player 2 with player 1 still the character | likely | coop_be_player2 around coop_run_script's first block, guarded | build-run-arrival.sh |
| L06's traps (ed156) run from other_enter for player 2 apart | likely | skip player 1's other_* run of a zone script player 2 already ran as theirs | build-run-traps.sh |
| L04's elevator scripts (rooms 189/190) run only from other_* fields | likely | the same other_* rule as the traps | build-run-elevator.sh |
| ed177's play-start scripts run as whoever is the player then | likely | record who called ed177; run its script as theirs at mode 2's enter | build-run-onplay.sh |

## 1. fn151's music duck by player 2 apart (hle.c:15084's return address)

**The bug.** hle.c:15084, in coop_sound_change_act, tests `cpu->lr == 0x80164DD4u` for fn151's duck. 0x80164DD4 is the address of the `bl 0x8019E068` instruction itself. The return address the intercept sees is 0x80164DD8, so the test can never match. Effect: when player 2 is in another room and their script calls fn151(1) (sound_duck), the call is not passed over. 0x8019E068(0, 25, 10) runs and ducks the one global sound group 0, so player 1's music and effects drop to 25 for the length of player 2's look or cutscene.

**What the code shows.**

What the code shows (`ed dis 0x80164D58`):
- fn151's wrapper 0x80164D58 ends in 80164DCC `li r4,25` (only when the script's argument is 1, otherwise r4 stays 100), 80164DD0 `li r5,10`, 80164DD4 `bl 0x8019E068`, 80164DD8 `li r3,0`.
- So the lr inside the intercept is 0x80164DD8, and hle.c:15084 can never be true.
- The return value of 0x8019E068 is discarded (r3 is set to 0 at 80164DD8), so passing the call over is safe there.

The other return addresses in the function are right:
- 0x801668FC and 0x80166970 follow the `bl 0x801A05EC` at 801668F8 and 8016696C.
- 0x80152354 follows 80152350.
- 0x8015206C and 0x8015207C follow the `bl 0x8019E068` at 80152068 and 80152078.

How the call reaches this test:
- tw_keep (hle.c:13953) calls coop_sound_change_act first.
- tw_keep is the intercept on 0x8019E068, among the `kept[11]` addresses at hle.c:32833.

What the branch does once it matches (hle.c:15084-15091):
- Conditions: r4 == 25 (the duck only, never the lift), coop_music_theirs(cpu), and coop_rooms_apart(cpu).
- coop_music_theirs (hle.c:14894) is true when player 2's change bracket is open, when the current node or r13-21496 is player 2's, or when the last script run was player 2's (coop_p2_script_is). That last case covers a state resumed after an fn36 wait.
- coop_rooms_apart (hle.c:14888) compares the rooms of the two players' own nodes.
- When all hold, it logs `[sound] a duck (fn151) by player 2 apart: passed over, player 1's music stays (frame N)` under coop_sound_log() and returns true. Before that it sets g_hle_passthrough=false and g_hle_tail_call=false, so 0x8019E068 never runs.
- The whole function returns false at hle.c:15048 unless coop_stay() (ED_COOP_STAY and two room slots), g_coop_p2 != 0 and sda_ready all hold. Solo never gets here.

Nothing else depends on it:
- `git grep 80164DD` finds only this test and its comment (hle.c:15045), plus docs and patch text: docs/coop-level3-design.md:2382, docs/research/separate-rooms/record-r5.md:46, patch_r5.py:273/312 (the patch that wrote it).
- No other hle.c code reads that path's result.

The log switch is ED_COOP_LOG, not ED_SOUND_LOG: coop_sound_log() at hle.c:14858 reads `getenv("ED_COOP_LOG")`.

Not in docs/coop-failed-attempts.md: there is no fn151, duck or 0x8019E068 entry there.

From reading (docs/research/separate-rooms/sound-readers.md:55): group 0 of 0x805D7C90 feeds the master sound and music levels through 0x801A0F3C. So the duck is global, which is why player 1 hears it.

Which scripts can reach it:
- fn151(1) is in L00/1352, 1438, 1551, 583; L04/1381, 1438; L09/1951; L11/1438; L12/1551; L13/1551.
- In chapters 1-12:
  - L09/1951 (Edward, chapter 10, room 273, SphereRegion01 enter, state 8 at line 88, lifted by fn151(0) at state 15).
  - L04/1381 (Karim, room 193: an item used from the Tome on a pedestal, the duck in state 1 after ed9).
  - L12/1551 (the grandfather clock, room 78, Region04: the duck only when AlexClockTry >= 2, i.e. the third try). No door into room 78 is known in level 12, and docs/coop-next-session.md:219 says the clock "is deep in the mansion's story", so it can't be reached by script.
  - L04/1438 and L11/1438 are placed in no room in the catalogue (L00 room 68, SKPlane01).

**Fix plan (the skeptic's corrected version).**

The fix is one change in runtime/host/hle.c, coop_sound_change_act:
- Line 15084: `cpu->lr == 0x80164DD4u` becomes `cpu->lr == 0x80164DD8u`. 0x80164DD8 is the instruction after `bl 0x8019E068` at 80164DD4 in fn151's wrapper 0x80164D58.
- Line 15045, the comment: "(lr 0x80164DD4" becomes "(lr 0x80164DD8".
- Optionally the doc mentions: docs/coop-level3-design.md:2382 and docs/research/separate-rooms/record-r5.md:46. Leave patch_r5.py as history.

Why it is safe:
- The return value of 0x8019E068 is thrown away (80164DD8 sets r3 to 0).
- The pass-over is the same shape as the arrival group-reset branch next to it (g_hle_passthrough=false, g_hle_tail_call=false, return true). That branch is shown working in logs/run-20261111-182644-rooms-r5-s1-p2door.

Gating, unchanged:
- coop_stay(): ED_COOP_STAY set and ED_ROOM_SLOTS=2.
- g_coop_p2 != 0 and sda_ready (line 15048).
- r4 == 25: the duck only. fn151(0)'s lift (r4 100) still goes through.
- coop_music_theirs and coop_rooms_apart.
- Solo, and co-op without the stay, never get here. Together, the duck still goes through.

The fix is not in docs/coop-failed-attempts.md. It is the smallest change: only calls of 0x8019E068 with lr 0x80164DD8 are affected, and fn151 is their only caller.

Risks, inferred and the same as today:
- Player 2's lift fn151(0) while apart can still set group 0 back to 100 and end a duck of player 1's early. Taking over the wrapper 0x80164D58 for both the duck and the lift would be the broader fix.
- If player 1 walks into player 2's room mid-scene, the duck is missing for the rest of that scene. This is cosmetic.

Static check: before the fix, `python -I tools/ed/facts/hooks.py | grep "CAN'T MATCH"` prints the hle.c:15084 line. After the fix it prints nothing for 15084. The session notes say it was the only one.

**Test plan (the skeptic's corrected version).**

Target: L09/1951, Edward, Jump to Game chapter 10 (jtg.sh grid 10, level 09). The chapter starts in room 238 (0xEE) with a ghost cutscene.

Room 273 (0x111), SphereRegion01:
- A sphere at (2145, 1211, -186), radius 124. Enter runs script 1951; leave runs 2430.
- With flag 413 on, flag 172 on, ed61()==0 and L0==1, state 0 shows PULL_LEVER (button 2, b for player 2) and sets state 3.
- The waits are 75, 60, 200, 90 and 30 frames, so state 8's fn151(1) comes about 455 frames after the press (inferred: plus run latency).

Doors (read with `ed script N --level 9`):
- 754: fn71(273), fn66(273, EntryMkr02, Camera01). The way in.
- 732: fn71(279), fn66(279, EntryMkr06, Camera03). Region01's enter door, the way out to room 279 (0x117).

All runs set ED_RUN_AWAY=1 and use a fresh state made on this build by jtg.sh from a cold Jump-to-Game boot. No ed test, no ed chapters, no old saves.

Script (sh, Git Bash):
```
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_RUN_AWAY=1 ED_ROOM_SLOTS=2 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_RUN_SHOTS_MS=250
F=${DUCK_F:?run the probe and set DUCK_F to the control frame}
S=$((F+900)); ST=saves/duck-ch10-273.state
# probe (run once, before setting DUCK_F)
#   env -u ED_COOP_STAY JTG_SLOT=1 JTG_FRAMES=1500 sh tools/ed/jtg.sh 10 duck-probe </dev/null | tee /dev/stderr | grep "both players in control"
#   then read 'control at frame' in the run's jtg-report.txt; LATE means raise JTG_BASE.
# state
rm -f "$ST"
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT="$((F+60)):754,$((F+120)):754" ED_SAVE_STATE="$S:$ST" ED_RUN_FRAMES=$((S+60)) sh tools/ed/jtg.sh 10 duck-state </dev/null
RS=$(ls -d logs/run-*-jtg-duck-state | tail -1); SET="$RS/settings.ini"
grep -h "running script 754\|^\[room\] change\|saving a state" "$RS"/page-*.log
[ -f "$ST" ] || exit 1
tail -1 "$RS/both.tsv"   # check by hand: both players in room 273
# apart
ED_SETTINGS="$SET" ED_COOP_STAY=1 ED_SET_FLAG="20:413,22:172" ED_RUN_SCRIPT="100:732,160:732" \
  ED_PUT="$((S+900)),2,2145,1211,-184" ED_PAD2_SCRIPT="$((S+960)),$((S+966)),b" \
  ED_RUN_FRAMES=$((S+2400)) ED_RUN_SECONDS=240 sh tools/ed/run.sh "$ST" duck-apart </dev/null
# together
ED_SETTINGS="$SET" ED_COOP_STAY=1 ED_SET_FLAG="20:413,22:172" \
  ED_PUT="$((S+400)),2,2145,1211,-184" ED_PAD2_SCRIPT="$((S+460)),$((S+466)),b" \
  ED_RUN_FRAMES=$((S+1900)) ED_RUN_SECONDS=200 sh tools/ed/run.sh "$ST" duck-together </dev/null
for R in $(ls -d logs/run-*-duck-apart | tail -1) $(ls -d logs/run-*-duck-together | tail -1); do
  echo "== $R"
  grep -h "setting flag\|running script 732\|^\[room\] change\|\[sweep\] player 2\|script 1951\|a duck (fn151)\|^\[sound\]\|ASSERT" "$R"/page-*.log | head -60
done
ls saves/reports/hang-*.txt 2>/dev/null | tail -1
python -I tools/ed/facts/hooks.py | grep "CAN'T MATCH"
echo RUNS DONE
```

What to read in each page-*.log, all lines real fprintf strings:
1. `[test] frame N: setting flag 413 (set)` and `... flag 172 (set)` (hle.c:19886).
2. Apart run:
   - `[test] frame N: running script 732` twice (hle.c:19986).
   - Then `[room] change 0x00000117 ...`: player 1 to room 279. This must come before the put at S+900; if not, move the put and the press later.
   - The together run has no 732 and no change line.
3. `[sweep] player 2 put at 2145 1211 Z (frame S+900)`. If the line is `not put: no floor`, change z.
4. `[coop] script 1951 runs for player 2 (frame ~S+960)` (hle.c:20137). Then several `[coop] script 1951 goes on as player 2's (lr 0x...)` (hle.c:20772), one per resumed wait, the last at about press+455. These prove state 8 was reached.
   - If none appear, the script stopped at state 0 (the `L0 ~= 1` risk). Use the user-play fallback.
5. `[sound] a duck (fn151) by player 2 apart: passed over, player 1's music stays (frame N)` (hle.c:15086):
   - Apart, before the fix: absent even though the resume lines show state 8 ran. That is the bug: no log records each 0x8019E068 call, so it only shows as this missing line.
   - Apart, after the fix: exactly one, at N ≈ press+455.
   - Together, before and after: absent.
6. No ASSERT and no new saves/reports/hang-*.txt.
7. Static check, before: the hooks.py line `CAN'T MATCH: hle.c:15084 ... 0x80164DD4u ...`. After: none for 15084.

Not knowable without running:
- The control frame F.
- Whether 754 run from room 238 lands both players in 273.
- Whether the put enters the sphere.
- Whether player 2's L0 passes `~= 1`.
- Whether player 2's cutscene waits resume apart.
- Whether the duck can be heard: ED_RUN_AWAY mutes the window.

Fallback, the user's own play of the clock (L00/1551, room 78, the third try, AlexClockTry >= 2):
- From cmd: `set ED_COOP_LOG=1` then `play-rooms-log.bat duck-clock`. play-rooms-log.bat does not set ED_COOP_LOG itself, and it supplies ED_ROOM_SLOTS=2 and ED_COOP_STAY=1.
- Player 2 in the study (room 78), player 1 in the main hall.
- Grep the run's page-*.log for `a duck (fn151)` and `script 1551`.
- Before the fix: no line, and player 1's music drops during player 2's clock scene. After: one line, and the music holds.

**What only a run or a play can show.** Only a run or a play can show these:
1. Whether player 2 even gets the PULL_LEVER prompt. L09/1951 state 0 exits when `L0 ~= 1`, where L0 = fn78(1,0), the player's id (r13-21496). For player 2's region run, coop_be_player2 puts player 2's id there. If that id isn't 1, the script stops at state 0 and the duck is never reached; the clock by the user's own play is then the test. Whether the decompiled `~= 1` is a literal compare or a truthiness test was not read.
2. Chapter 10's control frame. DUCK_CTRL is a guess: run the probe step.
3. Whether door script 754, run from room 238, lands in room 273 without following its route (inferred from the fx and R5 runs, which ran door scripts straight).
4. Whether player 2's cutscene(1,1) while apart runs its waits to state 8 through the script timers, as coop_p2_script_is assumes.
5. The exact frame of the duck (the script's waits are about 455 frames; loads shift it).
6. Whether the duck is audible. ED_RUN_AWAY mutes the window, and no log records each 0x8019E068 call, so before the fix the bug shows only as the missing line. Hearing player 1's music drop needs the user's play.
7. That group 0 really scales the disc music the user hears. This is read in sound-readers.md:55 (0x801A0F3C sets the master levels), not heard.
8. Whether the remaining lift fn151(0) by player 2 ever cuts a duck of player 1's short in real play (inferred risk, unchanged by this fix).

**The skeptic's notes.**

- None in the reading. The code matches it. `ed dis 0x80164D58` shows 80164DCC li r4,25 (only when the argument is 1, from cmpwi r0,1 / bne 0x80164DD0), 80164DD0 li r5,10, 80164DD4 bl 0x8019E068 and 80164DD8 li r3,0, so the return address is 0x80164DD8. hle.c:15084 tests 0x80164DD4u, and the comment at hle.c:15045 repeats it. The other return addresses follow their calls: 801668F8, 8016696C, 80152350, 80152068, 80152078. tw_keep (hle.c:13953) calls coop_sound_change_act first. kept[11] (hle.c:32833) includes 0x8019E068 and is installed with tw_keep only when the entry's intercept is NULL. The log switch is ED_COOP_LOG (hle.c:14858). There is no fn151, duck or 0x8019E068 entry in docs/coop-failed-attempts.md.
- Further proof of the lr convention, found in a past run log rather than in the code: the sister branch's line '[sound] the arrival's group reset is player 2's: passed over' (lr 0x8015206C, the address after the bl at 80152068) appears in logs/run-20261111-182644-rooms-r5-s1-p2door/page-0001.log. So the intercept does see the return address. No past log contains 'a duck (fn151)'.
- Test, missing ED_SETTINGS: the apart and together steps call tools/ed/run.sh without ED_SETTINGS. run.sh uses the user's own settings.ini, but the state was made by jtg.sh with its own copy (mod_coop = 1 and the jtg characters). build-run-fx.sh and build-run-tome.sh both pass ED_SETTINGS=<the state run>/settings.ini, so the plan should too.
- Test, state saved too early: it is saved at CTRL+500 = F+560, with door 754 run at F+60 and F+120. build-run-tome.sh saves at F+900 after a door script ('the door script's dissolve or motion, its countdown and the arrival, then a rest'). The step also never checks that both players arrived in room 273.
- Test, timing apart: player 1's door 732 runs at S+100 and S+160, but player 2 is put at S+400 and the duck comes at about S+915. In build-run-r5.sh s2, player 1's change took up to about 540 frames, so player 1's room may not have settled when the duck comes. The plan should wait for `[room] change 0x00000117` and put player 2 later.
- Test, room log grep: `[room] change` prints the room in hex ('[room] change 0x%08X 0x%08X (frame N) calls: ...', room_log hle.c:11463). Room 279 is 0x00000117, 273 is 0x00000111 and 238 is 0x000000EE, so the grep must look for those.
- Test, which line names script 1951: the plan only says grep 1951. The real ED_COOP_LOG lines are '[coop] script 1951 runs for player 2 (frame N)' at hle.c:20137 (the press, run as their use) and '[coop] script 1951 goes on as player 2's (lr 0x...)' at hle.c:20772 (each resume after an fn36 wait). Before the fix these lines are the only proof that the script reached state 8. ED_PUT's own lines, '[sweep] player 2 put at X Y Z (frame N)' and '[sweep] player 2 not put: no floor under ...' (sweep_put), should also be read.
- Test, the probe's control line: '[jtg] both players in control at frame F' is printed on jtg.sh's stdout (jtg.sh:188), not in page-*.log. The probe should tee stdout or read jtg-report.txt.
- Test, static check: hooks.py's actual output is "  CAN'T MATCH: hle.c:15084 ... tests lr == 0x80164DD4u ..." (hooks.py:427), so grep "CAN'T MATCH". hooks.py also writes build/ed-db/facts/hooks.json, which is build output, not a repo file.
- Fallback by the user's play: play-rooms-log.bat sets ED_ROOM_SLOTS=2 and ED_COOP_STAY=1 but not ED_COOP_LOG, so the '[sound] a duck (fn151)' line would never be printed. The user must run `set ED_COOP_LOG=1` in the same cmd window first. Play without ED_COOP_STAY (play.bat without it) never reaches this code at all (coop_stay(), hle.c:14688).
- Minor: SphereRegion01's centre is z -186 (radius 124). GravityMkr01 at (2279, 1216, -184) suggests the floor there is about -184, while room 273's entry floor is about -693. ED_PUT probes from z+300 down 1500, so -100 probably works too (inferred), but -184 is closer to the region.
- Minor: ED_COOP_STAY is tested with getenv != NULL (hle.c:14688, 33471), so it can only be turned off by unsetting it. The plan's `env -u ED_COOP_STAY` is right; ED_COOP_STAY=0 would turn it on.
- Still unknown, by reading: L09/1951 state 0 has `if L0 ~= 1`, with L0 = fn78(1,0), the hooked handle. If player 2's handle does not pass this, the script ends at state 0 and the scripted test cannot reach the duck. L04/1381 is no alternative: it picks by the room in play (fn25() == 193), so apart it reads player 1's room. The clock by the user's play is then the test.

<details><summary>The reader's first fix and test plans</summary>

The smallest change is one character in runtime/host/hle.c, coop_sound_change_act:
- Line 15084: `cpu->lr == 0x80164DD4u` becomes `cpu->lr == 0x80164DD8u`.
- Also fix the comment at line 15045, "(lr 0x80164DD4" to "(lr 0x80164DD8", so the next reader isn't misled.
- Optionally fix the two doc mentions: docs/coop-level3-design.md:2382 and docs/research/separate-rooms/record-r5.md:46.

Gating and solo: no new gate is needed. The branch already requires coop_stay(), g_coop_p2, sda_ready (line 15048), r4 == 25, coop_music_theirs and coop_rooms_apart. Solo and together play are untouched: together, the duck still goes through for both, as the design says.

What it touches:
- Only 0x8019E068 calls whose lr is 0x80164DD8, i.e. fn151 alone.
- fn151(0)'s lift (r4 100) still goes through, as it does today.

Risks (by reading, inferred where marked):
1. Player 2's own room gets no duck either, because there is one mixer. This is intended: player 1 holds the sound.
2. Inferred: if player 1 has their own duck in force when player 2's lift fn151(0) runs, that lift sets group 0 back to 100 and ends player 1's duck early. That happens today too and the fix doesn't change it. Intercepting the wrapper 0x80164D58 for both duck and lift, as refute-r5.md suggests, would be the broader fix if it matters.
3. Inferred: if player 2 ducks while apart and player 1 then walks into player 2's room before the lift, the duck is missing for that scene's tail. This is cosmetic.

After the edit, `python -I tools/ed/facts/hooks.py` should list no return-address test that can't match. The session notes say this one was its only finding.

Target: L09/1951, Edward, Jump to Game chapter 10 (grid slot 10; level 09; the chapter starts in room 238, docs/chapter-tour-review.md:21).
- Room 273, SphereRegion01: a sphere at (2145, 1211, -186), radius 124, enter script 1951.
- With flags 413 and 172 on, state 0 draws prompt 11 (PULL_LEVER) and sets state 3.
- The press runs state 3: cutscene(1,1), flag 172, ScareCam03, then waits 75, 60, 200, 90 and 30 frames through states 4 to 7.
- State 8 calls fn151(1) (line 88), so the duck comes about 455 frames after the press (inferred: the script's own waits plus a few frames of run latency).
- State 15 calls fn151(0).
- Exit from 273: door L09/732 (Region01, to room 279). Way in: L09/754 (room 279's BoxRegion05, to room 273 at EntryMkr02).

The pattern follows build-run-fx.sh and build-run-r5.sh: a fresh Jump-to-Game state made on this build first, then runs from it.
- **Step probe:** run chapter 10 once with no presses and read `[jtg] both players in control at frame F`. Chapter 10's control frame has never been measured; the default base of 2300 is a guess. Set `CTRL=F+60` in the script.
- **Step state:** run with ED_COOP_STAY off, so both players move together through the door scripts. ED_RUN_SCRIPT runs 754 twice at CTRL and CTRL+60 (walking in, then the press), which takes both players to room 273. ED_SAVE_STATE saves at CTRL+500.
- **Step apart:** run.sh from that state with ED_COOP_STAY=1, ED_ROOM_SLOTS=2, ED_COOP_LOG=1, ED_ROOM_LOG=1 and ED_RUN_AWAY=1. Frames S = the state's frame (CTRL+500):
  - ED_SET_FLAG=20:413,22:172 (delays from the run's first pad read).
  - ED_RUN_SCRIPT=100:732,160:732: player 1 leaves for room 279 and player 2 stays, as in build-run-r5.sh s2.
  - ED_PUT S+400: player 2 into the sphere at 2145, 1211, -100.
  - ED_PAD2_SCRIPT S+460..S+466: b. Button 2 is b, as for player 2's prompts in build-run-pages.sh and build-run-fx.sh.
  - ED_RUN_FRAMES S+2000.
- **Step together:** the same run without 732, so player 1 stays in room 273. This is the negative check: no pass-over line should appear.

What to read in page-*.log, before and after the fix:
1. `[test] frame N: setting flag 413` and `... flag 172`.
2. `[test] frame N: running script 732` twice, followed by `[room] change` lines: player 1 to room 279, player 2 staying.
3. Player 2's run of 1951: grep `1951`. Which line names it depends on ED_COOP_LOG and ED_SHOW_TRACE; that's inferred, so read whatever line carries the script number.
4. Around press+455: `grep -h "a duck (fn151)"`.
   - Before the fix: no such line in the apart run. The duck went through, so player 1's group 0 was at 25. There is no log of every 0x8019E068 call, so the bug shows only as a missing line, or by ear in play.
   - After the fix: exactly one line, `[sound] a duck (fn151) by player 2 apart: passed over, player 1's music stays (frame N)`, with N about press+455.
5. In the together run, the line should be absent both before and after.
6. Also check: no `ASSERT` and no saves/reports/hang-*.txt.
7. Static check: `python -I tools/ed/facts/hooks.py` lists 15084 before the fix and nothing after.

Fallback, by the user's own play, for the clock (L00/L12 1551): player 2 in the study (room 78), player 1 in the main hall, as the user played it on 2026-11-15. Player 2 examines the clock and tries it three times; on the third try (AlexClockTry >= 2) the cutscene and duck start. Run with ED_COOP_LOG=1 and grep the same `a duck (fn151)` line. Before the fix, player 1's music audibly drops during player 2's clock subtitles; after the fix, it doesn't.

</details>

## 2. ed113 / ed248 in a room's arrival script run for player 2 with player 1 still the character

**The bug.** The arrival script queued by player 2's own room change runs in coop_run_script's first block (hle.c 20554-20577). That block puts player 2's room in (coop_room_in) but leaves player 1 as the player and the main character (no coop_be_player2). Two calls then answer for player 1 in player 2's room. ed113 (0x80011150 -> 0x801D6364, rooms 90 and 202) reads r13-21484 (main node) through 0x801F6250. ed248 (0x8000BCA4 -> 0x800DC46C) reads r13-21488 (main id) through 0x801F6258. So in that run, player 2's active spell never counts, and in ed113 player 1's spell counts in a room they are not in.

**What the code shows.**

Read, not run.

1. The block (hle.c 20554-20577). Its gates are lr == ARRIVAL_TIMER_RAN (0x80163E5C, define at 20223), r3 == g_stay_room_script.script, and the room not in play but resident. It then calls coop_room_in(cpu, want, keep) (15224-15234: only r13-23276 and ROOM_SLOT_TABLE +0/+4), hle_guest_call(COOP_RUN_SCRIPT) and coop_stay_room_out. It never calls coop_be_player2. The queue is filled by notify_script_later (11789-11793) only while g_stay_bracket != 0, the R4 bracket of player 2's own change. That bracket does call coop_be_player2 (14773 on, 14817 off), but the script runs a frame later, after the bracket has closed.

2. coop_be_player2 (12740-12761) writes player 2's values into r13-26524, -21492, -21496, -21488 and -21484 and swaps the four casting words. Outside it, -21488 and -21484 are player 1's.

3. ed113 (ed dis): 0x80011150 calls 0x801D6364(room).
   - 0x801D6364 acts only for room 90 (flag 455) and room 202 (flag 842). Then 0x801D63B0 bl 0x801F6250, which is `lwz r3,-21484(r13)`, the main node.
   - Its +40 -> +140 -> +60 (a spell id) goes to 0x801F5F88. That node's +52 +4 goes through 0x801C7A98 and table 0x8023B948 (by the alignment word *r13-26320).
   - Then r13-23276 == 202 (circle bits 0x20000) or == 90 (0x40000), and if all pass, 0x801DC1D0 on the flag.
   - Then 0x801F6160(flag, room) and 0x801258BC(thing, 15, 0 or 1), plus 0x80125A70.
   - The main node is never checked against the room. coop_room_in makes the room test right, but the spell tested is player 1's character's, wherever player 1 is.

4. ed248 (ed dis): 0x8000BCA4 with argument 0 calls 0x800DC46C(0).
   - 0x800DC484 bl 0x801F6258 (`lwz r3,-21488(r13)`, the main id), then 0x801F5F88, then 0x801F662C. The answer is compared with r13-23276 at 0x800DC494-0x800DC49C; on a mismatch it returns 0.
   - Only on a match does it call 0x801D60DC(id) and 0x801D620C(id), use the table, and test 0x80045578 == 0.
   - The block runs only when player 1 is not in the room, so under the bug ed248(0) always returns 0 in player 2's arrival.

5. The hooks don't cover this, although the catalogue's co-op column marks "main [hooked]":
   - The intercept on 0x801F6258, coop_controlled_id (13430-13435), passes through unless lr is 0x801C6DCC or 0x801C6994 (an enchant).
   - coop_interface_player on 0x801F62B0/0x801F62B8 (22130-22134) acts only at the fireplace tile's return addresses.

6. The scripts:
   - L08/2079 (room 202's arrival): `if fn30(842)==1 and ed248(0)==1 and ed249(L3)==2 then fn73(919)`, then an unconditional `ed113(202)`.
   - L12/2477 (room 90's arrival): an unconditional `ed113(90)`, then `fn25()==90` and flag 741 -> ed70 barrier_make. The fn25 part is already right through coop_room_in.
   - L05/1774 (room 255): ed248(0) -> fn73(1861).
   - L10/2238 (room 345), L10/1814 (room 27), L11/1877 (room 32): ed248 + ed249 == 3 -> fn73.
   - L12/2777 (room 99): with ed248(0) ~= 1, it turns flag 74 on and runs the IntroCam01 scene.
   - The meaning "an active spell of the controlled character" comes from tools/ed/primitives.tsv (ed248 is named from the error string InvisibleSpell(); "which spell" is not traced there). That reading is inferred, not traced.

7. The nested call (running = true) falls through coop_run_script. With identity unchanged it reaches coop_script_cameras (20814-20819) with owner 1, so a camera an arrival script sets (L12/2777's fn8) goes into player 1's set too. Same cause, inferred from the code: no run seen.

8. docs/coop-failed-attempts.md has no attempt on this block. The nearest one ("Paul cannot use that now": coop_be_player2 alone was not enough when the script's check was 0x8016A7DC, the zones' inside words) does not apply to ed113 or ed248: neither reads the zones.

**Fix plan (the skeptic's corrected version).**

Change one place: coop_run_script's first block (hle.c 20560-20575). Nothing else changes.

```c
if (want != (s32)mem_read32(cpu, cpu->gpr[13] - 23276u) && room_slot_of(cpu, want) >= 0) {
    u32 keep[3], was[5], args[3] = {cpu->gpr[3], cpu->gpr[4], cpu->gpr[5]}, ret = 0u;
    const u32 lr = cpu->lr;
    // Player 2 the player and main character for their own room's arrival (ed113 reads r13-21484, ed248 r13-21488),
    // only when they stand in that room and aren't the player already (each be_player2 swaps the casting words).
    const bool as_p2 = coop_listed(cpu, g_coop_p2) && mem_read32(cpu, cpu->gpr[13] - 21492u) != g_coop_p2 &&
                       guest_ram_range_valid(cpu, mem_read32(cpu, g_coop_p2 + 52u), 640u) &&
                       coop_object_room(cpu, g_coop_p2) == want;
    running = true;
    if (as_p2) coop_be_player2(cpu, true, was);
    coop_room_in(cpu, want, keep);
    const u32 main_node = mem_read32(cpu, cpu->gpr[13] - 21484u);
    hle_guest_call(cpu, COOP_RUN_SCRIPT, args, 3, 0.0, &ret);
    coop_stay_room_out(cpu, keep);
    if (as_p2) coop_be_player2(cpu, false, was);
    running = false;
    cpu->lr = lr;
    fprintf(stderr, "[stay] room %d's own script %d (queued by player 2's arrival) ran with room %d current, as player %d's (main node 0x%08X; frame %llu)\n",
            want, (s32)args[0], want, as_p2 ? 2 : 1, main_node, (unsigned long long)(cpu->timebase / 675000ull));
    hle_set_u32(cpu, ret); g_hle_passthrough = false; g_hle_tail_call = false; return;
}
```

**The order:** be_player2 goes on first and room_in second, and they come off the other way round. The two write different words, so they nest cleanly.

**Co-op gated, solo-safe:** the block already needs g_settings.mod_coop, g_coop_p2 != 0, and a queue entry that notify_script_later fills only while g_stay_bracket != 0 (ED_COOP_STAY with room_slots()==2). Solo can't reach it.

**Why not coop_use_start:** it is a deferred tail call that also lends the cameras, swaps the zones, states and kinds, and sets the assist close-up. That is too much for a script run from the frame's waits, and ed113/ed248 need none of it.

**Not a repeat of a failed attempt:** "Paul cannot use that now" needed the zones swap because 0x8016A7DC reads the zones' inside words. ed113 and ed248 don't read them.

**Known effects** (inferred from the code, not run):
- coop_script_cameras, in the nested call, now sees owner 2 and puts player 2's camera set and prompt slots in for the run. A camera an arrival script sets goes to player 2's half, which is right.
- A fn21 hold made inside the run is marked as player 2's (13623).
- g_coop_p1_ran_script is not stored for this run (20634).
- L00/1816 in room 81 now reads player 2's character for its Alex/insanity branch. Its model_off still runs either way.
- A script that waits (1861, 2777) resumes later from the waits as player 1's. That is the queued-script pattern, not fixed here.
- fn0 states still come from the game's table.

**Test plan (the skeptic's corrected version).**

Chapter 12 (Alex End, level 12): start room 100, JTG_BASE=2255 (as in build-run-pages.sh). Room 100's BoxRegion02 (min [1595,-4885,-94] max [1790,-3876,919]) runs L12/635: fn71(90), then fn66(90, EntryMkr02, Camera02, 3, 0, 0). It is a walk-in door with no prompt. Room 90's arrival is L12/2477, and its ed113(90) is unconditional. Every run is a cold Jump to Game through tools/ed/jtg.sh: no state, no door script, ED_RUN_AWAY=1.

**Common settings** for every run: ED_RUN_AWAY=1 ED_COOP_LOG=1 ED_ROOM_LOG=1 JTG_SLOT=2 JTG_BASE=2255 JTG_FRAMES=600 ED_RUN_SECONDS=600.

**Run A (co-op, before the fix and again after):**
- Add ED_COOP_STAY=1 ED_ROOM_SLOTS=2.
- JTG_PUT="30,2,1692,-4380,400" puts player 2 inside BoxRegion02 at frame 2285. Player 1 has no presses and stays in room 100.
- Trace with ED_TRACE_FN=0x801D6364,0x801F6300 ED_TRACE_CHAIN=1 ED_TRACE_FROM=2295. Start after the put: 0x801F6300 is a hot getter and the chain list is capped at 1024 chains.
- If no \"<- 0x801D63C0\" line shows, read the [stay] line's frame N and run again with ED_TRACE_FROM=N-5.

**Run S (solo, on the fixed build):**
- JTG_SET=\"mod_coop=0\" and JTG_PUT=\"30,1,1692,-4380,400\" (player 1 takes the door), with the same trace.
- Expect: no \"[stay] room 90's own script\" line, and the ed113 node below equals player 1's (read it from the same chain line; there is no player 2).

**What to grep** in $R/page-*.log, in this order:
1. \"[coop] player 2's first turn ran (player 1's node 0xAAAAAAAA, player 2's 0xBBBBBBBB)\" (hle.c 23363, unconditional).
2. \"[sweep] player 2 put at 1692 -4380\". If \"[sweep] player 2 not put: no floor under\" shows, move x to 1620, still inside the box.
3. \"[stay] player 2's change to room 90 runs as theirs (from room 100; player 1 stays in room 100\" (the R4 bracket).
4. \"[room] later 0x000009AD\" with \"calls: 0x80153B6C\": 2477 is queued inside the bracket.
5. \"own script 2477 (queued by player 2's arrival) ran with room 90 current\" (hle.c 20569).
6. \"[chain] 0x801D6364 r3=0x0000005A\": ed113(90) was called.
7. \"[chain] 0x801F6300 r3=0x........ frame=N <- 0x801D63C0\": the main node ed113 read (bl at 0x801D63BC, right after 0x801F6250).
8. Also check: \"[stay] WARNING\", \"ASSERT\", any new saves/reports/hang-*.txt, and jtg-report.txt's places.

**Before the fix:** line 7's r3 equals 0xAAAAAAAA (player 1's node), and line 5 has no \"as player\". That is the bug.

**After the fix:**
- Line 7's r3 equals 0xBBBBBBBB.
- Line 5 reads \"... ran with room 90 current, as player 2's (main node 0xBBBBBBBB; ...)\".
- No WARNING, no ASSERT, no new hang report.
- jtg-report.txt has player 2 in room 90 and player 1 in room 100.

**ed248:** no cheap scripted case. L08/2079 is 11 doors from chapter 8's start and needs a state. L12/2777's ed248 needs previous_room==265. ed248 reads -21488, which the same swap writes, so it is covered by reading the code only. If a run is wanted later, trace 0x801F662C and expect \"<- 0x800DC494\" with player 2's node after the fix.

**Room 81 regression:** level 0 is a new game (JTG_NEWGAME=1), not chapter 1. Scripted runs never got player 2 into 81 first, so this is a check in the user's play. Look for \"[stay] room 81's own script 1816 ... as player 2's\" in logs/play.log, and check the hanged body is still hidden.

**Script:** the plan's script with these changes:
- ED_TRACE_FROM=$((BASE + 40)).
- Add grep \"\\[sweep\\] player 2\" to the run's output.
- Add a `solo` case: JTG_SET=mod_coop=0, JTG_PUT=\"30,1,1692,-4380,400\", no ED_COOP_STAY or ED_ROOM_SLOTS.
- Grep line 5 with \"as player\" after the fix.

**What only a run or a play can show.** Only a run can show these:

- **The put:** whether JTG_PUT (1692,-4380) lands on room 100's walkable floor inside BoxRegion02 and fires L12/635 for player 2, and the frames of the change and of the arrival.
- **Trace and queue:** whether ED_TRACE_CHAIN's 1024-chain cap fills before the arrival from ED_TRACE_FROM=2275. If "[chain] 0x801D6364 r3=0x0000005A" shows but no "<- 0x801D63C0" line does, raise ED_TRACE_FROM or use non-chain ED_TRACE_FN=0x801F6300 with ED_TRACE_R3=16 and grep lr=0x801D63C0. Also whether 2477 is in fact queued at lr 0x80153B6C inside the bracket and reaches the first block, not run in the change itself.
- **The visible effect:** what ed113 and ed248 actually change on screen, and which spell they test. The meaning is inferred from primitives.tsv, the effect is not traced, and showing it would need player 2 to cast the matching spell (5-point in room 202, 7-point in room 90, by alignment), which these runs don't script.
- **After the fix:** whether L00/1816's room 81 branch still hides the body when run as player 2's, and what an arrival camera (L12/2777's IntroCam01) now in player 2's set looks like in each half.
- **Waits and states:** a script waiting inside these arrivals (1861, 2777) resuming as player 1's, and the fn0 states coming from the game's table. These are read from the code, not seen.

**The skeptic's notes.**

- The reading holds against the code. coop_run_script's first block (hle.c 20554-20577) calls coop_room_in, then hle_guest_call(COOP_RUN_SCRIPT), then coop_stay_room_out, and never coop_be_player2. coop_room_in (15224-15234) writes only r13-23276 and ROOM_SLOT_TABLE +0/+4. coop_be_player2 (12739-12761) writes -26524, -21492, -21496, -21488 and -21484, and swaps the four casting words (kCoopCasting 22336/22320/22316/22312). The bracket opens at about 14776 (be_player2 on) and closes at about 14820 (off), not 14773/14817. That is off by a few lines and doesn't matter.
- Disassembly confirms both reads. 0x801F6250 is `lwz r3,-21484(r13); blr`, 0x801F6258 is `lwz r3,-21488(r13); blr`, and 0x801F6300 is `lwz r3,40(r3); blr`. In 0x801D6364 the bl 0x801F6250 is at 0x801D63B0 and the bl 0x801F6300 at 0x801D63BC, so its lr is 0x801D63C0 and its r3 is the main node. ed248's 0x800DC46C compares 0x801F662C(0x801F5F88(main id)) with r13-23276 at 0x800DC494-0x800DC49C. Nothing hooks 0x801F6250. 0x801F6258 is hooked only through coop_controlled_id, which passes through except at the enchant lrs.
- Test, trace window: ED_TRACE_FROM=BASE+20 (2275) with 0x801F6300 traced is a risk. 0x801F6300 is a one-instruction getter called from everywhere, and chain mode stops printing after 1024 distinct chains (hle.c 5606-5608). The cap may fill before the arrival (about +90 or later: the put at +30, then the transition-3 fade). Start the trace after the put (BASE+40), or take the arrival frame from a first untraced run.
- Test, the put: the plan doesn't grep the put's own log. sweep_put (hle.c 16976-17030) prints "[sweep] player 2 put at X Y Z" or "[sweep] player 2 not put: no floor under ...". Whether there is floor at (1692,-4380) inside BoxRegion02 is not known: that x is past EntryMkr05 at x 1417, in the doorway. Inferred, not read.
- Regression, room 81: "run chapter 1 with player 2 into room 81 first" is wrong. Room 81's arrival L00/1816 is level 0, Alex's mansion in a new game (start room 69, so JTG_NEWGAME=1), not Pius's chapter 1. Also, coop-next-session.md (about line 210) records three scripted runs (logs/run-20261115-2126*..2132*-jtg-room81-body*) where player 2's door waited until player 1 had entered 81 first. So this check can only be the user's play (logs/play.log), not a scripted run.
- Optional L08/2079 test: the route through room 82 -> door 191 -> room 196 -> Region01 OPEN DOOR -> room 202 was not checked by the plan's author or by me, and it needs a save state made mid-script, which strains the 'fresh Jump-to-Game, no old state' rule. L12/2777's ed248 sits behind previous_room==265 (ed254), so it can't be reached from room 100 either. No ed248 case can be reached cheaply. ed248 is covered by reading the code only, and should be stated that way.
- The test has no solo check. The block is gated on g_settings.mod_coop && g_coop_p2 (code-read), but one solo run (JTG_SET=mod_coop=0, player 1 put into the same box) is cheap. It should show no "[stay] room 90's own script" line and the ed113 node equal to player 1's.
- Fix, guard: the guard doesn't check that player 2 is in the room the script is for. Add coop_object_room(cpu, g_coop_p2) == want, so player 2 becomes the player only for an arrival in the room they actually stand in. The plan's fprintf has a `...` placeholder and doesn't log the main node, so the after-fix check needs the trace. Logging r13-21484 inside the swap makes the [stay] line enough on its own.
- The rest checks out. coop_listed (7008) and guest_ram_range_valid are declared before 20550. The 640-byte object check is coop_use_start's (20081). Line 20634 (g_coop_p1_ran_script) is skipped under the fix because -21496 becomes player 2's id; that is correct. A fn21 hold inside the run becomes player 2's through g_coop_script_hold at 13623, inferred, and that is correct for their arrival. Nothing in coop-failed-attempts.md covers this block. The 'Paul cannot use that now' attempt needed the zones (0x8016A7DC), and neither ed113 nor ed248 reads them. Every log string in the plan is real: hle.c 20569, 14782, 11794/11462 room_log, 23363 "player 2's first turn %s", 5613 "[chain] 0x%08X r3=0x%08X frame=%llu <-", and "[stay] WARNING".

<details><summary>The reader's first fix and test plans</summary>

The fix is in one function, coop_run_script's first block (hle.c 20560-20575). Nothing else changes.

```c
if (want != (s32)mem_read32(cpu, cpu->gpr[13] - 23276u) && room_slot_of(cpu, want) >= 0) {
    u32 keep[3], was[5], args[3] = {cpu->gpr[3], cpu->gpr[4], cpu->gpr[5]}, ret = 0u;
    const u32 lr = cpu->lr;
    // player 2 the player and main for their own arrival (ed113 reads r13-21484, ed248 r13-21488), unless already
    const bool as_p2 = coop_listed(cpu, g_coop_p2) && mem_read32(cpu, cpu->gpr[13] - 21492u) != g_coop_p2 &&
                       guest_ram_range_valid(cpu, mem_read32(cpu, g_coop_p2 + 52u), 640u);
    running = true;
    if (as_p2) coop_be_player2(cpu, true, was);
    coop_room_in(cpu, want, keep);
    hle_guest_call(cpu, COOP_RUN_SCRIPT, args, 3, 0.0, &ret);
    coop_stay_room_out(cpu, keep);
    if (as_p2) coop_be_player2(cpu, false, was);
    running = false;
    cpu->lr = lr;
    fprintf(stderr, "[stay] room %d's own script %d (queued by player 2's arrival) ran with room %d current, as player %d's (frame %llu)\n", want, (s32)args[0], want, as_p2 ? 2 : 1, ...);
```

The rest of the block stays as it is.

- **Why the guard:** coop_be_player2 swaps the casting words on every call (12755-12760). Called while player 2 is already in, it would put player 1's casting words back in. The 640-byte object check is coop_use_start's own (20082).
- **The order:** be_player2 first, then room_in, and out the other way round. The two write different words, so the nesting is clean.
- **Solo is untouched:** the block needs g_settings.mod_coop, g_coop_p2 != 0, and a queue set only inside the stay bracket (ED_COOP_STAY with two room slots).
- **Why not coop_use_start:** it is a deferred tail call that resumes at HLE_USE_RETURN. It also lends the cameras, sets g_coop_assist_at = 1 (their close-up), swaps the kinds, zones and script states, and does more. That is too much for a script run inside the frame's script waits, and the zone and state swaps aren't needed for ed113 or ed248.

Risks and what else it touches. All of this is inferred, none of it run:

- **Cameras and prompts:** the nested call's coop_script_cameras (20490-20548) now sees owner 2 and puts player 2's camera set and prompt slots in for the run. An arrival camera (L12/2777's IntroCam01) then goes to player 2's set, which is right for their room but changes what happens.
- **Room 81's body:** every queued arrival now runs as player 2's, including L00/1816, the hanged body in room 81 that the user played fixed. Its "Alex at insanity tier 2" check now reads player 2's character. Check room 81 again.
- **g_coop_p1_ran_script:** line 20632 no longer stores it for this run (its reader is at 13624). This is correct.
- **What a script does next:** fn73 run_next (919, 1861, 1598, 1878) runs inside the same run (0x80164488), so it keeps the identity. A script that waits (1861's 120 frames, L12/2777's 5) comes back later from the waits as player 1's, with player 1's room in play: g_stay_room_script is already cleared and nothing catches it. That is the queued-script ("later") pattern, not fixed here.
- **Script states:** fn0 states are still read from the game's table, not swapped.

Use chapter 12 (Alex End, level 12): its start room is 100 (`ed level 12` start_room 100; build-run-pages.sh plays chapter 12 in room 100 with base 2255). Room 100's BoxRegion02 (min [1595,-4885,-94] max [1790,-3876,919]) runs L12/635, a walk-in door with no prompt: fn71(90), then fn66(90, EntryMkr02, Camera02, transition 3). Room 90's arrival is L12/2477, whose `ed113(90)` is unconditional. This needs one Jump-to-Game run with no state and no door script.

**The run:**
- **Settings:** ED_RUN_AWAY=1, ED_COOP_STAY=1, ED_ROOM_SLOTS=2 (coop_stay() needs both: hle.c 14688), ED_ROOM_LOG=1, ED_COOP_LOG=1 (jtg.sh turns on its own logs too), JTG_SLOT=2, JTG_BASE=2255, JTG_FRAMES=600.
- **Player 2:** JTG_PUT="30,2,1692,-4380,400" puts them on the floor inside BoxRegion02 at BASE+30 (the same as build-run-pages' p1a put at "30,2,0,850,400" into BoxRegion06). No presses: the door asks nothing.
- **Player 1:** no presses. They stay in room 100.
- **The read:** ED_TRACE_FN=0x801D6364,0x801F6300 with ED_TRACE_CHAIN=1 and ED_TRACE_FROM=2275. Traced addresses become chain stops (hle.c 5884-5885), so direct bl calls are traced. Chain mode prints each distinct caller chain once with r3 at entry.
- **Then:** run it again on the fixed build (the same script with RUN=after).

**What to read** in $R/page-*.log, in this order:

1. "[coop] player 2's first turn ran (player 1's node 0xAAAAAAAA, player 2's 0xBBBBBBBB)" gives both nodes. It is unconditional, at hle.c 23363.
2. "[stay] player 2's change to room 90 runs as theirs (from room 100; player 1 stays in room 100" is the R4 bracket (14780).
3. "[room] later 0x000009AD" with "calls: 0x80153B6C" is 2477 queued inside the bracket (ED_ROOM_LOG, 11794).
4. "[stay] room 90's own script 2477 (queued by player 2's arrival) ran with room 90 current" is the first block (20569).
5. "[chain] 0x801D6364 r3=0x0000005A" is ed113(90) called.
6. "[chain] 0x801F6300 r3=0x........ frame=N <- 0x801D63C0" is the main node ed113 reads (0x801D63BC, after 0x801F6250).

**Before the fix:** that r3 equals 0xAAAAAAAA, player 1's node. That is the bug.

**After the fix:**
- That r3 equals 0xBBBBBBBB.
- Line 4 reads "... ran with room 90 current, as player 2's".
- No "[stay] WARNING", no "ASSERT", no new saves/reports/hang-*.txt.
- Player 2 walks in room 90 and player 1 in room 100 (jtg-report.txt places).

**Regression (on the fixed build):** run chapter 1 with player 2 into room 81 first. Look for "[stay] room 81's own script 1816 ... as player 2's" and check the hanged body is still hidden. The route there is the user's own play, not scripted: earlier scripted runs had player 1 enter 81 first.

**Optional L08/2079 (room 202, Roberto, chapter 8):** start room 82, and 202 is 11 doors away. Door 191 (`fn66(196, EntryMkr02, Camera03)`) run twice with ED_RUN_SCRIPT is a teleport to room 196. But under ED_COOP_STAY player 2 would stay in 82 (hle.c 23816-23825), so it needs build-run-fx.sh's two steps: a state made fresh with STAY off, then a STAY run that puts player 2 into 196's Region01 box (min [-93,-785] max [95,-496]) and presses b on OPEN DOOR. Trace 0x800DC46C and 0x801F662C there: "<- 0x800DC494" shows the main node whose room is compared. Only after the fix should "[chain] 0x801D60DC ... <- 0x800DC4A8" appear (ED_SET_FLAG=...:842 so ed248 is reached).

</details>

## 3. L06's traps (ed156) run from other_enter for player 2 apart

**The bug.** This is the L06 trap bug: ed156 picks its record by the room in play (r13-23276, 0x800849AC), and the scripts that call it are wired to a region's enter and also its other_enter. When player 2's body crosses a TrapRgn, the script runs twice. The first run comes from player 2's own look and uses the enter field, with player 2's room in play. The second comes from the game's own step for player 2's object and uses the other_enter field. That second run happens outside any bracket, so the room in play is player 1's room. ed156 therefore writes player 1's room's record if that room is 313, 233, 168, 306 or 308, and writes nothing if it is any other room. The duplicate run also uses player 1's copy of the script's state, so the trap fires a second time (sound 273, event 154 sent again to handles 396-399).

**What the code shows.**

Which field runs for whom (read from the disassembly):
- 0x80134A44 (fn_8013B538), the zone scan. At 0x80134A64-0x80134A7C it compares the scanned object (r3, kept in r29) with r13-26524 and sets r27/r28 to 0 when they match ("the player") or 1 otherwise ("other"). That flag is passed as r5 to 0x80134758 (at 0x80134AE0).
- 0x80134758, with r23 = that flag. At 0x801348EC-0x8013496C, flag 0 reads the player fields: zone+58/+60/+62 for a sphere, +60/+62/+64 for a box (enter, leave, stay). Flag 1 reads the other fields: +64/+66/+68 for a sphere, +66/+68/+70 for a box.
- 0x80134684 keeps the inside state in zone+104: mask 1 for the player, mask 2 for "other", one bit shared by every non-player body.
- 0x80134B28 saves r13-23276 and calls the zone's script, 0x801642D4 (r3 = script, r4 = r30 = the zone). After it returns (0x80134B38) the scan compares the word and stops if the room changed. At that call r27 and r29 still hold the flag and the object (callee-saved; I saw no write to r27 between 0x80134A7C and 0x80134BB8).
- The step 0x80133434 scans a body at 0x80133588-0x801335C4 when object+584 (0x80117C7C) <= 2 and node+8 (0x801F62D0) != 29, or when the object is r13-26524.

Who runs what in co-op (hle.c):
- coop_character_step (29908) passes through unless ED_COOP_P2_STEP is set (29922-29929). So the game's own step for player 2's object runs with r13-26524 still player 1's object, and player 2's body reads the other fields. docs/game/systems/doors.md:81 and docs/research/separate-rooms/enemy-readers.md say the same: player 2's object is kind 2 and is scanned. I did not check object+584 = 2 in a live run (inferred).
- Player 2's turn (23176-23186) makes them the player (r13-26524, -21492, -21496 written while g_coop_p2_is_player, which is true by default, 12411). It puts their room in with coop_stay_room_in at 23186, then calls coop_prompt_look (23218). coop_prompt_look (18695-18860) calls COOP_SCAN (18759) with their object and their zone words (coop_zones_swap 18720) and script states (coop_states_swap 18721). That run reads the enter field as theirs, inside their bracket, so it is correct.
- coop_run_script (20550) intercepts 0x801642D4 (installed at 33330-33332). For a zone run outside the turn and the look (lr 0x80134B38, !g_coop_turn, !g_coop_prompt_look), no branch claims it. At 20755 it is even marked as player 1's (coop_p2_script_mark false). It then falls to coop_script_cameras (20818), which runs it with player 1 as the player and player 1's room in play. The R6 bracket at 24330-24338 only wraps player 2's behaviour events (COOP_EVENTS), not zone scripts.

What ed156 does (read):
- 0x80015AA8 reads 5 arguments and calls 0x800849AC (at 0x80015B80): r3 = arg1, f1 = arg2, f2 = arg3, r4 = arg4, r5 = arg5.
- 0x800849AC switches on r13-23276. Room 313 -> 0x80323920+296, 233 -> +316, 168 -> +336, 306 -> +356, 308 -> +376. For any other room it returns without writing anything. The record gets +0 f1, +4 f2, +12 r4, +14 r3, +16 0, +18 r5.
- L06/2105, which is TrapRgn04's enter and also its other_enter (room 168), calls unknown_156[ed156](1, 1400, 2, 30, 1) only when its state is 0 and flag 555 is 0. Then it sets state 1 and waits 40.
- The only reader is the world animator 0x80082090 (its one call is at 0x80008208). It switches on r13-23276 with the same five rooms and offsets (0x8008224C-0x80082294), then from 0x800831D4 uses record +0/+8 for sound 245 (0x801A0880) and 0x801E4A80(1, f) (the mist height, which hle.c's fog_height_set at 28769 knows as "animator", lr 0x80083248). So it is a gas or fog ramp that only ever plays for player 1's room.
- Flag 555 is set by L06/1783 (ButtonRgn01 in rooms 139, 296, 297, 298), from the catalogue TSV.

Not in the code: no log or hook prints the room in play at a zone run or at 0x800849AC. ED_ROOM_LOG's notify_script_run (11808, "[room] trigger") is never installed in co-op, because coop_run_script already holds that address (33333-33335). The only existing evidence is ED_COOP_TRACE's "[trace] script run r3 <script> r4 <zone> lr 0x80134B38 who <id>" (20663, trace_look 11768). Its "who" is r13-21496: player 2's id in their look, player 1's id in the frame run.

**Fix plan (the skeptic's corrected version).**

What is wrong (read in the code; the run-time parts are marked inferred). When player 2's body enters a TrapRgn in level 06, L06/2105 (and 2106, 2108, 1618, 1711, 1781, 1741, which have the same shape) runs twice:
(1) Player 2's look (coop_prompt_look, 18759, inside their turn) runs it from the enter field, as player 2, with their room, script states and zone words in. This run is correct.
(2) The game's own step for player 2's object (0x80133434 -> scan 0x80134A44 with r27 = 1, 'other') runs it from the other_enter field at lr 0x80134B38, outside any bracket. coop_run_script lets it fall through to coop_script_cameras, so it runs as player 1, with player 1's room in play and player 1's script states.
Run (2) plays sound 273 again and sends event 154 to 396-399 again. Its ed156 writes the record of player 1's room (313/233/168/306/308), or nothing for any other room. Then, about 40 frames later, the game's timer runs player 1's copy again (lr 0x80163E5C, state 1): player 1 gets the look_at / face_morph_wave meant for player 2.

STEP 0, a log-only build first (no change in behaviour), so the bug can be shown before the fix. Add a notify on 0x800849AC beside the installs at about hle.c:33330, gated by ED_COOP_LOG, using the same 'entry != NULL && entry->notify == NULL && entry->intercept == NULL' pattern:
  fprintf(stderr, "[coop] ed156 record: room in play %d (r3 %d, f1 %.0f, r4 %d; the player's id %u, turn %d, look %d, frame %llu)\n", (s32)mem_read32(cpu, cpu->gpr[13] - 23276u), (s32)cpu->gpr[3], cpu->fpr[1], (s32)cpu->gpr[4], mem_read32(cpu, cpu->gpr[13] - 21496u), g_coop_turn, g_coop_prompt_look, frame);
  g_hle_passthrough = true;
Whether 0x800849AC (fn_80087840 in the kit) has a dispatch entry is inferred. If hle_dispatch_entry returns NULL, put the notify on ed156's own handler 0x80015AA8 instead and print the same words.

STEP 1, the fix in runtime/host/hle.c.

(a) A new static before coop_prompt_look (before 18695): static unsigned long long g_coop_p2_look_frame;. In coop_prompt_look, set it to cpu->timebase / 675000ull just before hle_guest_call(cpu, COOP_SCAN, ...) at 18759. It is then set only when their look really scans; the coop_left_behind return and the turn's room_other skip never reach it.

(b) A new helper before coop_run_script:
  static bool coop_zone_other_dup(CPUState* cpu, u32 zone, s32 script)
It returns false unless guest_ram_range_valid(cpu, zone, COOP_ZONE_SIZE). shape = (s16)mem_read16(cpu, zone + 108u); return false unless shape is 0 or 1. player = zone + (shape ? 60u : 58u), other = zone + (shape ? 66u : 64u). Return true when, for some k in 0..2, (s16)mem_read16(other + 2k) == script and (s16)mem_read16(player + 2k) == script. The offsets were read at 0x80134910-0x8013496C, with r31 = zone + 48.

(c) In coop_run_script, after the 'camera's end script fired by player 2's camera' block (it ends at about line 20630) and BEFORE the block that sets g_coop_p1_ran_script (about 20631), so that no side effect of a run (g_coop_p1_ran_script 20634, g_both_script 20660, g_script_watch 20665) is left for a run that is passed over:
  if (cpu->lr == COOP_ZONE_RUNS && (s32)cpu->gpr[3] > 0 && g_settings.mod_coop && g_coop_p2 != 0u && g_coop_scan && g_coop_p2_is_player && !g_coop_turn && !g_coop_prompt_look && !g_coop_use.active && sda_ready(cpu) && cpu->gpr[27] == 1u && cpu->gpr[29] == mem_read32(cpu, g_coop_p2 + 52u) && cpu->gpr[29] != mem_read32(cpu, cpu->gpr[13] - 26524u) && cpu->timebase / 675000ull <= g_coop_p2_look_frame + 2ull && coop_zone_other_dup(cpu, cpu->gpr[4], (s32)cpu->gpr[3])) {
      if (coop_trace()) trace_look(cpu, "script run");
      if (getenv("ED_COOP_LOG") != NULL) fprintf(stderr, "[coop] zone script %d (player 2's body, other field) passed over: their look runs it as theirs (zone 0x%08X, room in play %d, theirs %d, frame %llu)\n", (s32)cpu->gpr[3], cpu->gpr[4], (s32)mem_read32(cpu, cpu->gpr[13] - 23276u), coop_object_room(cpu, g_coop_p2), frame);
      cpu->gpr[3] = 0u; g_hle_passthrough = false; g_hle_tail_call = false; return;
  }
This drops the run the same way the 'being found' skip does (20606-20611). r13-23276 is not touched, so the room-change compare at 0x80134B3C passes. The inside bit (zone+104, mask 2) was already set by 0x80134684 before the call, so the zone's state is unchanged. g_coop_step_running is NOT in the gate: it is declared later (24005), and the case is already covered by r27 == 1.

Why this fix:
- Solo is safe: g_coop_p2 == 0 there.
- It only ever affects player 2's body. Enemies keep their other_* runs.
- Other-only scripts are untouched (fix list (a): L04/2301, 2726, 2738).
- With ED_COOP_P2_NOT_PLAYER the gate is off.
- When their look did not run in the last 2 frames (player 1's room change, left behind), the old behaviour is kept, so a crossing is never lost.
- It removes the second trap and the second ed156. It also removes the later timer run of player 1's copy (player 1's head turn).

Not chosen: putting coop_stay_room_in around the frame run (the queued-script block at 20557-20575 does this). It fixes the room but keeps the double trap, run as player 1 with player 1's states. ED_COOP_P2_STEP / ED_COOP_P2_SCAN (scanning as player 2 inside the step or the turn) broke doors (29918-29921, 23200-23204) and is not touched.

Left as it is: the world animator 0x80082090 plays only the record of the room in play (the disassembly's switch). So the gas ramp in player 2's half never shows for their room while they are apart. That is a separate issue, not read further.

Risks:
- Zones whose player field and other field hold the same script (the L02 TrapRgns, BoxRegion01/02, the L06 traps) now run once for player 2, in their look. That is solo's semantics for the player.
- Inferred: if player 2 is inside a zone during a look-less stretch longer than 2 frames, the frame run goes through unchanged (the old double run) for that crossing only.

**Test plan (the skeptic's corrected version).**

Chapter 6 (Lindsey, level 06, start room 303). JTG_P1=6 JTG_P2=2 JTG_SLOT=1, ED_RUN_AWAY=1 on every run. jtg.sh and run.sh already set ED_COOP_LOG, ED_COOP_TRACE and ED_ROOM_LOG. Do the whole set twice: on the Step 0 build (log only, before the fix) and on the Step 1 build (after). Run s0 again on each build: per test_save_state's comment (hle.c:19897), a state is made on the current build and thrown away when the code changes. Never run ed test or ed chapters, and never use an older state.

TrapRgn04 (room 168): box x -375..-77, y -1136..-944, z -8..175. Put at the middle (-226,-1040,10), then step, because a put alone doesn't prove the zone fires (coop-failed-attempts.md:527, 566). The step is 6 frames of stick; it is camera-relative, and from the middle of a 298x192 box it stays inside.

1. probe: a cold Jump to Game with no presses, JTG_FRAMES=1500 (closes at 3800; chapter 6's control has never been measured). F is read from '[jtg] both players in control at frame F', or from python -I tools/ed/jtg_report.py --wait RUN 30.

2. s0, follow mode (ED_ROOM_SLOTS=2, ED_COOP_STAY unset: frontend.cpp:487). JTG_BASE=F+30, ED_RUN_SCRIPT="$((F+60)):1467" (L06/1467 is room_queue(168) + go_to_room(168, EntryMkr01)), ED_SAVE_STATE at S=F+400, ED_RUN_FRAMES=S+100. Delete the state first.
   Checks: '[room] change 0x000000A8', no '[stay]' line, and jtg-report.txt showing both in 168 near (-70,-2916). If player 2 is not there, the follow did not bring them; raise S and run again.

3. t, together (follow mode, loads s0). ED_PUT="$((S+60)),2,-226,-1040,10", ED_PAD2_SCRIPT="$((S+62)),$((S+68)),lup", ED_RUN_FRAMES=S+400.

4. a, apart, no record. ED_COOP_STAY=1 ED_ROOM_SLOTS=2, ED_RUN_SCRIPT=60:1465 (counted from the load; player 1 goes to 139 at EntryMkr02). ED_PUT="$((S+400)),2,-226,-1040,10", ED_PAD2_SCRIPT="$((S+402)),$((S+408)),lup", ED_RUN_FRAMES=S+700.

5. b, apart, wrong record. As a, plus ED_RUN_SCRIPT=60:1465,400:1468 (player 1 goes on to 313). Put at S+800, step S+802..S+808, ED_RUN_FRAMES=S+1100.

6. solo, a cold Jump to Game. JTG_SET=mod_coop=0, JTG_BASE=F+30, ED_RUN_SCRIPT="$((F+60)):1467", ED_PUT="$((F+400)),1,-226,-1040,10", ED_INPUT_SCRIPT="$((F+402)),$((F+408)),0,0,127", ED_RUN_FRAMES=F+700. (A stick-only press with buttons 0 is fine, per jtg.sh's header.)

Grep page-*.log (not *.log: EVENTS.log has the same lines again) for:
'script run r3 2105 \|\[sweep\] player\|\[stay\] player 2 stays\|change 0x000000\|zone script 2105\|ed156 record\|running script\|ASSERT'
Also check that saves/reports/hang-*.txt is empty.

Before the fix (Step 0 build):
- t, a, b: '[sweep] player 2 put at ...'. Then, around the frame of the step, two lines '[trace] script run r3 2105 r4 <zone, decimal> lr 0x80134B38 who <A>' and '... who <B>'. A = player 2's id (their look, inside their turn). B = player 1's id (the frame's other_enter run).
- About 40 frames later: '[trace] script run r3 2105 ... lr 0x80163E5C who <B>' (the game's timer runs player 1's copy, state 1: look_at + face_morph on player 1). Player 2's copy's run of its own comes through coop_use_start (its lr is inferred).
- '[coop] ed156 record:' lines:
  - t: two lines with 'room in play 168'.
  - a: one with 'room in play 168' (turn 1, the player's id A) and one with 'room in play 139' (turn 0, id B). 139 is not in 0x800849AC's switch, so nothing is written.
  - b: 'room in play 168' and 'room in play 313', which writes 313's record at 0x80323920+296.
- a and b also show '[stay] player 2 stays in room 168 (slot N) while player 1 went to room 139' (b also '... room 313').
- solo: one 'script run r3 2105 ... lr 0x80134B38 who <player 1>', one 'ed156 record: room in play 168', and one timer run about 40 frames later.
- If no 2105 line appears at all, then either flag 555 was on (L06/1783, ButtonRgn01), or the put or step missed the box.

After the fix (Step 1 build, s0 made again):
- t, a, b: exactly one 'lr 0x80134B38 who <A>' run of 2105 per crossing. The who <B> trace is followed at once by '[coop] zone script 2105 (player 2's body, other field) passed over: their look runs it as theirs (zone ..., room in play 168/139/313, theirs 168, ...)'.
- Exactly one 'ed156 record: room in play 168' per crossing (turn 1, id A, in a and b).
- No 'room in play 139' or 'room in play 313' record line.
- No 'lr 0x80163E5C who <B>' run of 2105.
- solo: unchanged from before (no 'passed over').
- Every run: no ASSERT and no hang report.

Script notes on build-run-traps.sh: it is otherwise right. Its probe gets JTG_FRAMES=1500, look() greps "$R"/page-*.log, the KEYS above, and the ED_PAD2_SCRIPT / ED_INPUT_SCRIPT steps added to t/a/b/solo.
Unread, still inferred:
- whether player 2's body is scanned by the game's step while apart (enemy-readers.md says it is)
- that flag 555 is 0 at chapter 6's start
- that 1465/1468 land cleanly under the stay, with room 168 kept in its slot
- that 0x800849AC has a dispatch entry
- how long chapter 6's start takes to control

**What only a run or a play can show.** These need a play or a run to settle:
- Whether the game's own step really scans player 2's object in co-op. The step needs object+584 <= 2 and node+8 != 29; prior docs say it is kind 2, but I did not check it live. If it doesn't scan, run t shows only the who=player-2 line and the bug is not there.
- Whether that scan still runs for their body while they are apart in the other resident room (enemy-readers.md says it does, using their own room's zones).
- Whether flag 555 is 0 at chapter 6's start. If it is on, 2105 skips ed156 entirely.
- Whether ED_PUT inside the box makes both scans see "enter" in the same frame or in neighbouring frames.
- Chapter 6's control frame.
- Whether ED_RUN_SCRIPT's 1465/1468 teleports land cleanly under stay, with player 2's room 168 kept in its slot through both changes.
- Whether jtg_report.py --wait answers at once on a finished run.
- Whether a dispatch notify can be put on 0x800849AC (the record log).
- What the duplicate trap run actually does on screen (sound twice, gas vents 396-399 sent event 154 twice).
- Whether player 1's script-state copy flipping to 1 really turns their next crossing into the look_at branch (inferred from the script's state test).
- What the gas ramp looks like in either half. The world animator 0x80082090 only plays the room in play's record; the fix doesn't change that.

**The skeptic's notes.**

- Mostly right. I checked these against the disassembly and hle.c and they match: 0x80134A64-0x80134A7C (r28/r27 = 0 for the player, 1 for other) and r5 = r27 at 0x80134AE0. 0x80134758 reads the player fields at zone+58/+60/+62 (sphere) or +60/+62/+64 (box) and the other fields at +64/+66/+68 or +66/+68/+70 (r31 = zone+48). The script call at 0x80134B28-0x80134B3C, with r30 stepping 116 per zone. The step at 0x80133588-0x801335C4 scans when kind <= 2 (bc 12,1 on cmpwi 2), so enemy-readers.md's 'kind == 2' is slightly off. 0x800849AC's switch: 306/233/168/313/308 at 0x80323920+356/316/336/296/376, every other room returns with nothing written. ed156 calls it at 0x80015B80. hle.c lines 12411, 18720-18759, 20449, 20550, 20606-20611, 20663-20664, 20750-20756, 23176-23186, 29922-29929 and 33330-33335 match. Script contents check out: L06/2105, 1467 (to 168, EntryMkr01), 1465 (to 139, EntryMkr02, after-script 1161), 1468 (to 313), and 1783 (ButtonRgn01, flag 555). Room 168's TrapRgn04 box and EntryMkr01 match. Chapter 6 is Lindsey, level 06, start room 303 (coopplan.py:31). coop-failed-attempts.md has nothing on other_* fields or ed156.
- Compile error in the gate. g_coop_step_running is declared at hle.c:24005, after coop_run_script (20550). It is also redundant: it is only set under ED_COOP_P2_STEP, and there the step runs with r13-26524 = player 2's object, so r27 is 0 and the gate already fails. Drop it.
- The insertion point (after 20749) leaves side effects on the call being skipped. Line 20634 sets g_coop_p1_ran_script = 2105 (read at 13624 for player 1's script hold), 20660 sets g_both_script, and 20665 sets g_script_watch = 40. The skip belongs earlier: after the camera's-end block (ends about line 20630), before the 'g_coop_p2 != 0u && sda_ready(cpu) && ... != player 2's id' block at about 20631. It then calls trace_look(cpu, "script run") itself when coop_trace() is on, so the trace line still prints.
- The plan gets the duplicate's later effect wrong. fn36 does not resume the script mid-way: it runs the script again from the top when its timer runs out (docs/game/research/verbs.md:37, prompts.md:15). So about 40 frames later, player 1's copy of 2105 runs again from the game's timer (lr 0x80163E5C = ARRIVAL_TIMER_RAN) in state 1. It does look_at[ed304] with L0 = handle[fn78](1,0), which is player 1 at that point (player 1's head turns to SanityMkr04/07), then face_morph_wave, then sets state 0. Player 1's next crossing is NOT turned into the state-1 branch. Player 2's own copy runs again through coop_script_timers -> coop_use_start (22659-22740). That gives a real log line for the bug: '[trace] script run r3 2105 ... lr 0x80163E5C who <player 1's id>' about 40 frames after the crossing, and it should be gone after the fix.
- The skip is not gated on player 2's look actually running. Their look is skipped during player 1's room change (23209-23211: room_other && !p2_stays), and coop_prompt_look returns before COOP_SCAN when coop_left_behind (18703). In those frames the frame's other_* run is the only run, and the planned gate (g_coop_scan, !g_coop_prompt_look) would throw it away. Gate on a frame stamp set just before COOP_SCAN at 18759 instead.
- The test breaks a documented convention: ED_PUT into a zone does not prove the zone fires. coop-failed-attempts.md:527 says the floor is found about 50 low and the body gets pushed back out ('walk in, or step after a put'). Line 566 says 'put outside, step, put inside, step'. Every put needs a short stick push after it (ED_PAD2_SCRIPT for player 2, ED_INPUT_SCRIPT for player 1 in solo), plus a '[sweep] player N put at' check.
- No log shows the wrong record before the fix. The plan only adds the ed156 notify together with the fix, so runs a and b have no evidence before it. The notify fires on entry to 0x800849AC before the switch, so a log-only first build would print 'room in play 139' (a) and 'room in play 313' (b) before the fix. Also, its lr is always 0x80015B84 (the only caller), so print the turn, look and step flags instead. That 0x800849AC has a dispatch entry is inferred; the fallback is ed156's own handler 0x80015AA8, reached through the script function table.
- States: test_save_state's comment (hle.c:19897-19900) says a state is made fresh on the current build and thrown away when the code changes. So s0 must be run again on the fixed build. The before and after sets each need their own s0, and t/a/b after the fix must not load the state made before it. The plan's script deletes the state in s0, but the plan never says to run s0 again after the fix.
- The probe is too short to be sure of seeing control. JTG_FRAMES=300 from the default base 2300 closes at frame 2600, and chapter 6's control frame has never been measured: chapter 1 came at about 1870, other chapters are guesses. Use JTG_FRAMES=1500 (closes at 3800), and read '[jtg] both players in control at frame F' or jtg_report.py --wait.
- Small script issues. grep "$R"/*.log also reads EVENTS.log (pagelog.py), so lines come out twice: use page-*.log. KEYS should also match '[sweep] player' and 'running script'. In the trace line, r4 (the zone) is printed as signed decimal, not hex. The '[stay] player 2 stays ...' line prints only once per room (static said), and its string is real (hle.c:23820). The '[room] change 0x000000A8' string is real (room_log 'change', 11627).
- Checked and fine: coop_run_script is installed in solo too (find_coop, called at 34163, has no mod_coop gate before 33330), so the solo run's '[trace] script run r3 2105 ... who <player 1>' line does exist. In run t (together) both runs use room 168, so t shows only the double run, never a wrong record. Loading a follow-mode state with ED_COOP_STAY=1 has a precedent (build-run-fx.sh's fx-a). Not read by me, still inferred: that player 2's body is scanned while apart, that flag 555 is 0 at chapter 6's start, and that the 1465/1468 teleports land cleanly under the stay.

<details><summary>The reader's first fix and test plans</summary>

The fix: skip the duplicate run. When the frame's other_* run of player 2's body matches the script in the zone's player field for the same event, skip it. Player 2's look already runs that field as theirs, inside their bracket (their room, states, identity and zone words). This is what solo does: one run per crossing.

Where: runtime/host/hle.c, coop_run_script. Insert a block right after the tome/arrival block ends (after line 20749, before the "Who a script is" block at 20750), so the ED_COOP_TRACE line at 20663 still prints for the skipped call.

Gate (co-op only; solo has g_coop_p2 == 0 and never reaches it):
  cpu->lr == COOP_ZONE_RUNS (0x80134B38) && (s32)cpu->gpr[3] > 0 && g_settings.mod_coop && g_coop_p2 != 0u && g_coop_scan && g_coop_p2_is_player && !g_coop_turn && !g_coop_prompt_look && !g_coop_use.active && !g_coop_step_running && sda_ready(cpu) && cpu->gpr[27] == 1u && cpu->gpr[29] == mem_read32(cpu, g_coop_p2 + 52u) && coop_zone_other_dup(cpu, cpu->gpr[4], (s32)cpu->gpr[3])
- g_coop_scan: their look exists.
- g_coop_p2_is_player: their look reads the player fields.
- cpu->gpr[27] == 1u: the scan's "other" flag.
- cpu->gpr[29] == player 2's object: the scanned object.

New static helper coop_zone_other_dup(zone, script): shape = (s16)mem_read16(zone+108). If shape is 0 or 1, take the player fields at zone + (shape ? 60 : 58) and the other fields at zone + (shape ? 66 : 64). Return true when, for some k in 0..2, other[k] == script && player[k] == script.

Body: log under ED_COOP_LOG: "[coop] zone script %d (player 2's body, other field) passed over: their look runs it as theirs (zone 0x%08X, room in play %d, theirs %d, frame %llu)". Then set cpu->gpr[3] = 0u; g_hle_passthrough = false; g_hle_tail_call = false; return. This is how the "being found" skip at 20606-20611 drops a run. r13-23276 is untouched, so the scan's room-change compare at 0x80134B3C passes.

Test-only addition, gated by ED_COOP_LOG: a notify on 0x800849AC printing "[coop] ed156 record: room in play %d (r3 %d, f1 %.0f, r4 %d; the player's id %u, turn %d, lr 0x%08X, frame %llu)". It goes beside the other installs (about 33330), as the existing ones do. That a dispatch entry can be made for this leaf (fn_80087840) is inferred; the kit says the host has none there today.

Not chosen: the smaller-looking bracket (coop_stay_room_in around the frame's run, as the queued-script block at 20557-20575 does). It would pick player 2's room's record, but the trap would still fire twice: once with player 1's script states (so player 1's next crossing runs the state-1 branch instead of the trap) and once with player 1 as "the player" (handle[fn78](1,0)). Keep it as the fallback only if run t shows no who=player-2 line. The failed ED_COOP_P2_STEP approach (scan inside the step as player 2, which broke doors, 29918-29921) is not touched. docs/coop-failed-attempts.md has nothing on other_enter, the other bit or ed156.

Risks and what else it touches:
- Every zone whose other_* field holds the same script as the player field (the L02 TrapRgns and BoxRegion01/02, the L06 traps) now runs once for player 2, in their look.
- Other-only scripts (fix list (a): L04/2301, 2726, 2738) are unchanged and still unbracketed.
- If their look is skipped while player 1's room change is on (23209-23211), the enter fires when the look comes back, but only if they are still inside (inferred).
- With ED_COOP_P2_NOT_PLAYER, the gate is off.
- A separate issue the fix leaves alone: the world animator plays only the room in play's record, so player 2's half never shows their room's gas ramp. Player 1 walking into 168 later gets whatever record 168 holds (inferred).

Chapter 6 (Lindsey, level 06, start room 303), JTG_P1=6 JTG_P2=2, ED_RUN_AWAY=1, ED_COOP_LOG=1 ED_COOP_TRACE=1 ED_ROOM_LOG=1. Route: 139 joins 168 and 313 (L06/1467 139->168 EntryMkr01, 1465 168->139, 1468 139->313).

1. probe: a cold JTG of chapter 6 with no presses, JTG_FRAMES=300. Read control's frame F from jtg_report.py; chapter 6 has never been measured, and jtg.sh's 2300 is a guess.

2. s0: a cold JTG in follow mode (ED_ROOM_SLOTS=2, ED_COOP_STAY unset; frontend.cpp:487 says this is the test scripts' follow mode). ED_RUN_SCRIPT=(F+60):1467 teleports player 1 and brings player 2, both into room 168 at EntryMkr01 (-70,-2916). ED_SAVE_STATE at S=F+400 (both idle), close at S+100. The state is deleted first and made on this build, as build-run-fx.sh's s0. Check: "change 0x000000A8", no "[stay]" line, both in 168 in jtg-report.txt.

3. t (together, follow mode): ED_PUT="S+60,2,-226,-1040,10" puts player 2 in TrapRgn04's middle (box x -375..-77, y -1136..-944). Run to S+400.

4. a (apart, no record): ED_COOP_STAY=1. ED_RUN_SCRIPT=60:1465 sends player 1 to 139; player 2 stays in 168. ED_PUT player 2 into TrapRgn04 at S+400. Run to S+700.

5. b (apart, wrong record): ED_COOP_STAY=1. ED_RUN_SCRIPT=60:1465,400:1468 sends player 1 on to 313. Player 2 is put in at S+800. Run to S+1100.

6. solo: JTG_SET=mod_coop=0, the same 1467 at F+60, and player 1 put into TrapRgn04 at F+400.

Read before the fix:
- t, a and b each show two lines at the put's frame: "[trace] script run r3 2105 r4 <zone> lr 0x80134B38 who <A>" and the same with who <B>. A is player 2's id (their look's enter run). B is player 1's id (the frame's other_enter run).
- a: also "[stay] player 2 stays in room 168 (slot N) while player 1 went to room 139". The who=B run has room in play 139, so ed156 writes nothing (139 is not in 0x800849AC's switch). This comes from reading the code; no log shows it before the fix.
- b: "[stay] ... went to room 313". The who=B run writes 313's record (0x80323920+296); also from reading, no log.
- About 40 frames later each copy's wait resumes. The trace lr will be a timer's, for example 0x80163E5C (inferred).
- Solo: one trace line for 2105 (who = player 1). If no line appears at all, flag 555 was already on or the put missed the box.

After the fix (with the ed156 notify):
- t, a and b: one "[trace] script run r3 2105 ... lr 0x80134B38 who <A>" line that runs; the who=B line is followed by "[coop] zone script 2105 (player 2's body, other field) passed over ...".
- Exactly one "[coop] ed156 record: room in play 168 ..." per crossing: in t; in a and b inside player 2's turn (turn 1, the player's id A).
- b: no "ed156 record: room in play 313".
- Solo: unchanged (one trace, one ed156 record line with room 168, no "passed over").
- Every run: no ASSERT, no saves/reports/hang-*.txt.

</details>

## 4. L04's elevator scripts (rooms 189/190) run only from other_* fields

**The bug.** Rooms 189/190's elevator (chapter 4, Karim). L04/2301, 2726 and 2738 sit only in the zones' fields for anyone but the player: 189 ElevatorRgn01 other_leave 2301, 189 MonsterCheckRgn00 other_enter 2738, 190 ElevatorRgn02 other_enter 2726 and other_leave 2301. 2726 is also on 189's SKPlane02/03 cross_b. The game's own step scans player 2's object as "anyone else", so player 2 fires them. They run with player 1 as the player, player 1's room in play, player 1's prompt slots and the game's script states.

These scripts are the elevator's "a monster is at the lift" guard. 2726 clears prompt slots 2 and 4 and sets the elevator script's state to 0 (143 in 190, 146 in 189) without running it again. 2301 and 2738 set that state to 0 and run 143 or 146 again, which re-asks. 146 and 143 only count monsters (handles of types 230/2268 and 390-393), so player 2 never blocks the lift on purpose. They only knock out player 1's prompt or restart its state.

What follows (the code shows this; none of it has been run):
(a) Together in 190: player 1 is on the elevator with 143's EXAMINE. Player 2 steps into ElevatorRgn02, the box about 50 units wider around the lift. 2726 clears player 1's EXAMINE and sets 143's state to 0, and nothing re-asks until player 2 leaves ElevatorRgn02 (then 2301 runs 143 again). Two players standing on the lift together means player 1 gets no prompt.
(b) Together in 189: player 2 walking in or out of MonsterCheckRgn00 or ElevatorRgn01 resets 146. If player 1's yes/no words are up (146 state 2), the state goes back to 0 and the words are asked again.
(c) Apart, player 2 has ridden down to 189 and player 1 stands on 190's lift. Player 2 entering 189's MonsterCheckRgn00 runs 2738 with room 190 in play. fn23(ElevatorRgn01) finds 190's own ElevatorRgn01 (same hash, 2959409236) and answers with player 1's bit. So it clears player 1's 143 prompt and runs 189's 146 inside room 190: an EXAMINE with script 146 ("use the device to ascend?"), and Yes goes to room 190/EntryMkr04, not down to 189. Player 2 walking out of 189's ElevatorRgn01 runs 2301's room-190 branch, which restarts player 1's 143.

It is the same rule as the to-do's bug (3), the L06 traps: other_* fields fired for player 2 outside their bracket.

**What the code shows.**

Scripts (ed script N --level 4):
- 2726: if fn25()==190 and fn23(ElevatorRgn01): fn1(0,143), fn211(2,0), fn211(4,0) (no re-run). Elif fn25()==189 and (fn23(ElevatorRgn01) or fn23(MonsterCheckRgn00)): fn1(0,146) and the same clears.
- 2301: room 190 and fn23(ElevatorRgn01): fn1(0,143) and fn73(143). Room 189 and fn23(ElevatorRgn01): fn1(0,146) and fn73(146).
- 2738 (no room test): fn23(ElevatorRgn01) or fn23(MonsterCheckRgn00), then clear slots 2 and 4, fn1(0,146), fn73(146).
- 146 state 0: L3=fn78(2,230), L4=fn78(2,2268), each against ed245(ElevatorCheckMkr00) >= 290. With no monster near, ed301(2,6) EXAMINE, fn71(190), state 1. Otherwise, while in ElevatorRgn01, fn36(5). States 1-5: hold, busy_set(1), room_words(43,3,146) "Should Karim use the device to ascend?", answer, cutscene, wait 260, fade, fn66(190, EntryMkr04, Camera01, 1, 1610).
- 143 is the same shape for 190: needs flag 91; checks monster handles 390-393 within 235; words 39,2; fn66(189, EntryMkr01, ..., 1447).
- 1610 (arrival in 190) ends with fn1(0,143) and fn73(143). 1447 (arrival in 189) ends with fn1(0,146) and fn73(146).
- Room data (ed room): 190 ElevatorRgn01 box [-156,-139]..[127,131] enter 143. ElevatorRgn02 box [-208,-193]..[178,185], other fields only. 190 Region01 (+110 90, enter 2360, an examine) also covers the lift.

Who the other_* fields run for:
- docs/game/research/verbs.md (Regions): the scan 0x80134A44 uses the player's fields only when object == r13-26524. Any other model uses sphere +64/+66/+68 and box +66/+68/+70, and crosses no plane.
- ed dis 0x80134A44: r28 = 0 if the object is r13-26524, else 1. It is the bit index into +104 (slw at 0x80134BF0). Every non-player shares bit 1.
- The step's gate (ed dis 0x80133588..0x801335C4): object+584 <= 2 and node+8 != 29, or the player.
- docs/game/systems/doors.md:81 and enemy-readers.md:62 say the game's own step scans player 2's object (kind 2) with player 1 as the player, on the "other" bit. I read the gate but did not check +584's value for player 2 in a run.
- enemy-readers.md:57 (0x80123F70): a non-player step takes its own room's record by +592, so player 2's scan uses their room's zones.
- fn23 (ed dis 0x8016A7DC) walks 0x80155130(2), the room in play's zones, by hash and tests bit 0 (the player's) of +104. fn25 returns r13-23276.

hle.c:
- coop_character_step (29908) passes through unless ED_COOP_P2_STEP is set.
- coop_scan_player1 (22305), the intercept on 0x80134A44: for player 2's object it takes the branch at 22332 ("|| cpu->gpr[3] == player 2's object") and returns with passthrough. coop_scan_room_start returns at once because the object isn't player 1's node's. So the game's scan runs as is.
- The zone script then reaches coop_run_script (20550) with lr COOP_ZONE_RUNS 0x80134B38 (20449). The only effects are coop_p2_script_mark(..., false) (20755), the trace (20664) and coop_script_cameras (owner player 1, nothing swapped).
- There is no coop_stay_room_in, coop_zones_swap or coop_states_swap. The R6/R6c brackets don't apply: R6c skips the players' own nodes (29249 comment), and coop_events (24335) covers events to player 2's node, not the behaviour pass's step.
- Player 2's own zones (player fields, bit 0 in their zone words, planes) run in coop_prompt_look (18695), called at 23218 inside their turn's coop_stay_room_in (23186) with coop_zones_swap and coop_states_swap. That bracket is fine.
- docs/coop-failed-attempts.md has nothing on skipping player 2's "other" scan.

**Fix plan (the skeptic's corrected version).**

The reading holds: the bug is likely real, and the reader's fix is the smallest one. Change one thing: add g_coop_p2_is_player to the gate.

runtime/host/hle.c, in coop_scan_player1 (line 22305). Insert after its first early return (`if (g_coop_p1_own_look) return;`, lines 22307-22308) and before coop_p1_unplaced:

  #define COOP_STEP_SCAN_RAN 0x801335C8u  // return address of the step 0x80133434's bl 0x80134A44 at 0x801335C4, the scan's only caller (verbs.md:119)
  static int other_zones = -1;
  if (other_zones < 0)
      other_zones = getenv("ED_COOP_P2_OTHER_ZONES") != NULL;  // testing: the old route, player 2 trips other_* fields
  if (!other_zones && g_settings.mod_coop && g_coop_p2 != 0u && g_coop_p2_is_player && cpu->lr == COOP_STEP_SCAN_RAN && sda_ready(cpu) &&
      cpu->gpr[3] == mem_read32(cpu, g_coop_p2 + 52u) && cpu->gpr[3] != mem_read32(cpu, cpu->gpr[13] - 26524u)) {
      static u32 said_zones = 0xFFFFFFFFu;  // once per zones list (per room)
      if (getenv("ED_COOP_LOG") != NULL && said_zones != cpu->gpr[4]) {
          said_zones = cpu->gpr[4];
          fprintf(stderr, "[coop] player 2's object passed over by the game's zone look for others (zones 0x%08X): their own look runs their zones (frame %llu)\n",
                  cpu->gpr[4], (unsigned long long)(cpu->timebase / 675000ull));
      }
      cpu->gpr[3] = 0u;
      g_hle_passthrough = false;
      g_hle_tail_call = false;
      return;
  }

Why it is safe:
- 0x801335C8 throws the answer away (`or r3,r31,r31`).
- The out record r1+104 is written only when a zone script moved the object (0x80134B98-0x80134BCC). Skipping is the same as "no script moved them".
- Host guest calls run with lr = GUEST_CALL_SENTINEL (main.c:81). So player 2's own look (coop_prompt_look, hle.c:18695, run in their turn inside coop_stay_room_in, hle.c:23186) is never caught. Nor are coop_scan_room_start, coop_follow_look, or ED_COOP_P2_STEP, whose step runs with r13-26524 = player 2.
- Solo is untouched: the gate needs mod_coop and g_coop_p2.
- Monsters (kind-2 handles that aren't player 2's object) still scan, and the elevator guard keeps working for them.
- g_coop_p2_is_player keeps the old route for the ED_COOP_P2_NOT_PLAYER test mode, where player 2's own look isn't the player's.
- Every other_* row in the catalogue outside L04 is on a region whose own enter runs the same script (L02/L06 TrapRgn and BoxRegion, L08 208 Region05). Player 2 keeps those through their bracketed look. The together double trip ends (inferred), and so does bug (3)'s apart run through other_enter with player 1's room in play.
- Not in docs/coop-failed-attempts.md.

Rejected alternative: wrapping the step's scan in coop_stay_room_in plus coop_zones_swap. fn23 would then answer with player 2's zone bit, but 143/146 would still run as the game's script with player 1 the player, and the prompt would go to the wrong owner.

After the fix runs clean:
- tools/ed/patterns.py: other_* units become 'never for player 2' rather than 'uncovered'. Then ed build and a catalogue regenerate (index.md:39, 49).
- docs/coop-next-session.md: the uncovered fix list (a), and bug (3)'s "unless that region event runs inside coop_stay_room_in".
- docs/game/systems/doors.md:81 ("the game's own step also scans player 2's object") gets a note: skipped in co-op since this fix.
- docs/research/separate-rooms/enemy-readers.md:62: '== 2' becomes '<= 2'.

Risks needing a run: an L06 trap with player 2 together and apart (each trap once, with player 2's room's ed156 record); any region a companion was meant to trip (none known). If '[coop] the zones' look not found' prints at boot, the hook isn't installed and the fix does nothing.

**Test plan (the skeptic's corrected version).**

General:
- Every run is a cold Jump to Game through tools/ed/jtg.sh, chapter 4, with JTG_P1=4 (Karim) JTG_P2=2 (Ellia, model 70, so 132 sends her to 339) and ED_RUN_AWAY=1. No save states, no ed test/chapters.
- jtg.sh already sets ED_COOP_TRACE=1, ED_ROOM_LOG=1 and ED_COOP_LOG=1.
- B=2300 (jtg.sh's default). Chapter 4's control was measured at 2204/2233 in room 338 (logs/run-20261115-175417-jtg-tome-s0-ch4, -175541-jtg-tome-state-ch4). Confirm that jtg-report.txt doesn't say LATE.
- Frames below are relative to B. ED_SET_FLAG and ED_RUN_SCRIPT are written as $((B+n)), since they count from the first pad read.
- 'Before' = today's build, or the fixed build with ED_COOP_P2_OTHER_ZONES=1. 'After' = the fixed build.
- Grep every page-*.log for:
  - "script run r3 (2726|2738|2301|143|146|132|34) r4"
  - "passed over by the game's zone look for others"
  - "[room] change 0x000000B[DE]"
  - "[stay] player 2's change"
  - "ASSERT"
- Also look for a new saves/reports/hang-*.txt, and run `ed check`.

Common pieces:
- FLAG = ED_SET_FLAG=$((B+20)):91 (143 asks only with flag 91; 2360 is then empty).
- TO190 = ED_RUN_SCRIPT=$((B+50)):144,$((B+110)):144
- TO189 = ED_RUN_SCRIPT=$((B+50)):150,$((B+110)):150
- First check "[room] change 0x000000BE" (or 0xBD) by about +300 to +450. If it comes later than +600, move every put and press below by the difference.

Run ref (follow mode: ED_ROOM_SLOTS=2, ED_COOP_STAY unset), the control.
- FLAG, TO190, JTG_FRAMES=1900.
- JTG_PUT="700,1,0,0,0;700,2,400,-500,0" (player 1 in ElevatorRgn01/02 and Region01; player 2 in CameraRgn01 only).
- JTG_P1_SCRIPT="720,728,0,0,40;950,956,0x0200;1050,1056,0x0100;1110,1116,0x0100;1170,1176,0x0100"
- Expect before and after: "script run r3 143 r4 ... lr 0x80134B38" after the push (player 1's enter), then "script run r3 143 ... lr 0x8000704C" at about +950, then "[room] change 0x000000BD" at about +1400 to +1700 (cutscene plus wait 260 plus fade). No 2726/2301 line.

Run lift: as ref, plus "850,2,150,0,0" in JTG_PUT (player 2 in ElevatorRgn02, outside ElevatorRgn01 at x>127).
- Before: "script run r3 2726 r4 ... lr 0x80134B38" at about +850, no "script run r3 143 ... lr 0x8000704C" after the B at +950, and no 0xBD change by the end. That is the bug.
- After: no 2726, a "[coop] player 2's object passed over by the game's zone look for others" line, and ref's 143 run and 0xBD change.
- If 'before' shows no 2726, the gate or the put didn't make the scan see player 2 inside (cannot_know). Re-run with JTG_P2_SCRIPT="852,856,lup" and check p2x/p2y in both.tsv are still inside x 127..178 / y -193..185.

Run mon189 (follow mode). Log lines only.
- TO189, JTG_FRAMES=1300.
- JTG_PUT="700,1,33,-2347,0;700,2,33,-1800,0;850,2,150,-2300,0;950,2,33,-1800,0;1050,2,150,-2300,0;1150,2,33,-1800,0"
- JTG_P1_SCRIPT="720,728,0,0,40"
- (150,-2300) is inside both MonsterCheckRgn00 and ElevatorRgn01 at z 0, and outside CameraRgn03's raised xy. Check p2z is about 0 in both.tsv.
- Before: "script run r3 2738 ... lr 0x80134B38" at about +850 and +1050, and "script run r3 2301 ... lr 0x80134B38" at about +950 and +1150. A "script run r3 146 ... lr 0x80164488" after each only if player 1's own bit is set in those zones (inferred).
- After: none of them, only the "passed over" line.

Run apart (stay mode: ED_ROOM_SLOTS=2 ED_COOP_STAY=1, both set).
- FLAG, TO190 (player 1 to 190 alone, player 2 stays in 338), JTG_FRAMES=3300.
- Player 2's way, by their own doors:
  - JTG_PUT 400,2,-940,-85,0 (338 Region01, script 132); JTG_P2_SCRIPT 420,428,lup then 500,506,b. Expect "[stay] player 2's change to room 339 runs as theirs".
  - JTG_PUT 800,2,-248,1377,0 (339 Region01, script 34); 820,828,lup then 900,906,b. Expect "[stay] player 2's change to room 190 runs as theirs".
  - JTG_PUT 1150,1,300,-400,0 and 1150,2,0,0,0; 1170,1176,lup; 1250,1256,b (player 2's 143 EXAMINE); 1350,1356,a, 1410,1416,a, 1470,1476,a. Expect "[stay] player 2's change to room 189 runs as theirs", then "change done: they are in room 189", by about +1900.
- If any [stay] line is missing or late, read the frames and shift the later items.
- Then: JTG_PUT 2100,1,0,0,0 with JTG_P1_SCRIPT 2120,2128,0,0,40 (player 1 gets 143's EXAMINE); JTG_PUT 2250,2,33,-1800,0 and 2350,2,150,-2300,0; player 1's 2450,2456,0x0200, 2550,2556,0x0100, 2610,2616,0x0100, 2670,2676,0x0100.
- Before: "script run r3 2738 ... lr 0x80134B38" at about +2350, then "script run r3 146 ... lr 0x80164488" (146 run in room 190), then player 1's B running "script run r3 146 ... lr 0x8000704C", then "[room] change 0x000000BE" or a refused same-room change (cannot know which).
- After: no 2738, "script run r3 143 ... lr 0x8000704C", "[room] change 0x000000BD", and both.tsv p1room = p2room = 189 at the end.

Run solo: JTG_SET="mod_coop=0", FLAG, TO190, JTG_PUT="700,1,0,0,0", ref's JTG_P1_SCRIPT, JTG_FRAMES=1900.
- Expect the same 143 runs and 0xBD change as ref, and no "passed over" line, before and after.

Optional L06 follow-up for risk (1), its own script: an L06 TrapRgn tripped by player 2 together and apart. Expect one trap script run per entry, from player 2's look (lr 0x80134B38 inside their turn), and none from the step's scan after the fix.

Every run: no ASSERT, no new hang-*.txt, ed check clean.

Runs only can tell (inferred, not read):
- whether +584 <= 2 for player 2's object;
- whether a put alone is seen by the step's scan;
- 189's floor height at the puts;
- how many A presses the elevator words take;
- whether player 2's elevator cutscene holds player 1;
- whether 2561's barrier or monsters (handles 390-393/230/2268) block the prompt.

**What only a run or a play can show.** Only a run can show these:
- Whether player 2's object passes the step's scan gate (object+584 <= 2, node+8 != 29) at run time. The docs say it does (doors.md:81, enemy-readers.md:62). Seeing the 2726/2738/2301 trace lines in lift/mon189 before the fix settles it.
- Chapter 4's control frame (ELEV_BASE), and whether ED_SET_FLAG 91 alone is enough for 143 to ask.
- Whether an ED_PUT counts as walking in: the look runs as they move (that's why the stick push is there). A put may also cross 189's SKPlane02/03 and run 146 or 2726 by itself.
- 189's floor at the lift. EntryMkr01 is at z 605 and the spheres are centred at z 47; if the put lands on a raised platform, player 2 may be outside the spheres (ed where: z 600 is in CameraRgn03 only). Check the z values in both.tsv.
- Whether the monster handles 146 and 143 look for (types 230/2268, 390-393) exist in a fresh JTG chapter 4, which would suppress the prompt in every run.
- Which prompt player 1's B runs at (0,0) in 190: 143 (slot 2) or Region01's 2360 examine (+110 90, slot 4).
- Whether Yes is lit first in the elevator words, and how many A presses the words take.
- How fn36's 260-frame wait behaves when 2301/2738 reset the state mid-wait.
- In stay mode: whether player 2's use of 143 holds player 1 through its cutscene, and whether fn66 from room 190 to room 190 (case c) works or refuses.
- Whether script 132 sees player 2's model (70) in their bracket and sends them to 339, not room 5.
- Whether the fix changes the L06 traps for player 2 together (a double trip ending) or apart. That needs its own L06 run.

**The skeptic's notes.**

- The test's 'apart' run is broken as written. It says 'default stay mode, both switches unset', but coop_stay() (hle.c:14685-14690) is on only when ED_COOP_STAY is set AND room_slots()==2 (ED_ROOM_SLOTS=2, hle.c:14001-14010). With both unset the run uses the one-slot route, and player 1's ED_RUN_SCRIPT 144 door brings player 2 along, so the run never reaches 'apart'. The run must set ED_ROOM_SLOTS=2 ED_COOP_STAY=1, as docs/research/build-run-pages.sh's STAY does.
- Chapter 4's control frame has been measured, contrary to the plan: logs/run-20261115-175417-jtg-tome-s0-ch4 and -175541-jtg-tome-state-ch4 report 'control at frame 2204' and '2233', room 338, P2 model 70. jtg.sh's default JTG_BASE 2300 is about 70-100 frames after control, so the probe run is not needed (it can stay as an optional check).
- The worry that Region01's 2360 examine might win player 1's B at (0,0) in room 190 doesn't apply: `ed script 2360 --level 4` is `if flag(91) ~= 0 goto end; examine_plain(39,0)`. With ED_SET_FLAG 91 set, 2360 offers nothing, so 143 is the only prompt there. The 'if B ran 2360' branch can go.
- The puts at +450 are too early. The second 144 run is at +110. From logs/run-20261115-075511-fx-a-1, a door script's fn66 reached '[room] change' 181 frames after its second run (2823 -> 3004), then the arrival and fade. So 190 arrives at about +290 plus its fade, and in follow mode player 2 is still being brought along. A put lands in whatever room the player is in at that moment. Move the puts to +700, with every later press shifted to match.
- mon189's 'in' spot (33,-2240) lies inside the xy of CameraRgn03, the box [-51,-2249,553]..[92,-2105,745], which has a raised floor at z 553-745 (the lift's top; EntryMkr01 is at z 605). ED_PUT puts 'on the floor under x y', so it may land up there, outside both spheres (centres at z 47, radii 222 and 230). (150,-2300) is inside MonsterCheckRgn00 (distance 131) and ElevatorRgn01 (distance 126) at z 0, and outside CameraRgn03's xy. The apart run's +2350 put has the same problem.
- Minor reading corrections. fn73 (run_next) runs the script inside the current run, at lr 0x80164488 (hle.c:20750-20751), not later. The plan's lr is already right. In case (b), resetting 146 to state 0 and re-running it asks the EXAMINE prompt again (ed301(2,6), state 1), not the yes/no words. What happens to words already on screen is inferred.
- The step's gate: 0x80117C7C is `lwz r3,584(r3)`, and 0x80133590-0x801335B0 scan when +584 <= 2 (cmpwi 2; bgt goes to the player test) and node+8 != 29, or when the object is r13-26524. The reader's '<= 2' is right; docs/research/separate-rooms/enemy-readers.md:62 says '== 2'. Handles 390-393/230/2268 are 'kind 2' to fn78. That they are the room's monsters, and so scan zones on bit 1 as the designers meant, is inferred and not checked in a run.
- Fix checks. All addresses hold: 0x801335C4 is the only `bl 0x80134A44` (verbs.md:119); 0x801335C8 is `or r3,r31,r31`, so the answer is thrown away; the out record (r6 = r1+104) is written only at 0x80134B98-0x80134BCC, when a zone script moved the object. Host guest calls set lr to GUEST_CALL_SENTINEL (main.c:81), so the lr test can't catch coop_prompt_look's or coop_scan_room_start's own scans. Without the fix, player 2's object reaches the branch at hle.c:22332 and passes straight through: coop_p1_unplaced, coop_follow_look and the room-change branch all reject an object equal to player 2's, and coop_scan_room_start returns because r3 is not node(r13-21492)+52. One gap: the gate should also test g_coop_p2_is_player (hle.c:12411). Under ED_COOP_P2_NOT_PLAYER, player 2's own look doesn't run as the player (hle.c:23181-23205), so skipping the step's scan would leave them with no zones at all.
- Fix scope, checked against the catalogue TSVs: every other_* row outside L04 (L02 165/169/234/294/295 TrapRgn and BoxRegion01/02, L06 TrapRgn, L08 208 Region05) is on a region whose own enter field runs the same script. The elevator's 2301/2726/2738 are the only scripts that run from other_* fields alone. So after the fix, player 2 still trips every trap through enter in their bracketed look (coop_prompt_look 18695, inside coop_stay_room_in 15236). docs/coop-failed-attempts.md has nothing on skipping player 2's step scan or ED_COOP_P2_STEP. No env name or define clashes (ED_COOP_P2_OTHER_ZONES and COOP_STEP_SCAN_RAN are unused).
- Log strings, grepped in hle.c. Real today: '[trace] script run r3 %d r4 %d lr 0x%08X who %u' (trace_look, 11768, from coop_run_script at 20663 under ED_COOP_TRACE, which jtg.sh sets); '[room] change 0x%08X' (189 = 0x000000BD, 190 = 0x000000BE); "[stay] player 2's change to room %d runs as theirs" (14780) and "[stay] player 2's change done: they are in room %d" (14846); lr 0x8000704C = COOP_PROMPT_RUNS (20448), 0x80134B38 = COOP_ZONE_RUNS (20449). both.tsv has p1room/p2room (columns 72/73). The '[coop] ... passed over by the game's zone look for others' line exists only after the fix. JTG_SET, JTG_P1/JTG_P2, JTG_PUT, JTG_P1_SCRIPT/JTG_P2_SCRIPT, JTG_SLOT and JTG_FRAMES exist in jtg.sh. ED_SET_FLAG and ED_RUN_SCRIPT take 'delay:value' lists separated by commas (hle.c:19866-19890, 19961-19990).
- The door scripts check out (ed script, level 4). 144: state 0 asks the prompt and queues 190; state 1 runs fn66(190, EntryMkr02). 150: the same into 189/EntryMkr02. 132: model 150 goes to room 5, anything else (Ellia, model 70) to 339/EntryMkr01, in one B because state 0 sets state 2. 34: 339 to 190/EntryMkr01. Rooms: 338 Region01 box [-1131,-302]..[-754,132] enter 132; 339 Region01 box [-325,1290]..[-172,1465] enter 34; 190's ElevatorRgn01/02 and 189's spheres as the reader gave them. 190's arrival script 2561 makes a four-marker barrier unless flag 96; whether it closes off the lift is not known (puts get past it anyway).
- The working tree has an uncommitted runtime/host/controls.cpp change (git status). Every 'before' run uses a build that includes it.

<details><summary>The reader's first fix and test plans</summary>

The smallest fix, shared with the L06 traps (bug 3) and all 198 other_enter regions: stop the game's own step from scanning player 2's object as "anyone else". Player 2 is a player. Their own look in their turn already runs the player's fields in their bracket.

runtime/host/hle.c, coop_scan_player1 (starts at 22305). Insert after its first early return (22307-22308, `if (g_coop_p1_own_look) return;`):

  #define COOP_STEP_SCAN_RAN 0x801335C8u  // the step 0x80133434's bl 0x80134A44 at 0x801335C4
  static int other_zones = -1;
  if (other_zones < 0) other_zones = getenv("ED_COOP_P2_OTHER_ZONES") != NULL;  // testing: the old route
  if (!other_zones && g_settings.mod_coop && g_coop_p2 != 0u && cpu->lr == COOP_STEP_SCAN_RAN && sda_ready(cpu) &&
      cpu->gpr[3] == mem_read32(cpu, g_coop_p2 + 52u) && cpu->gpr[3] != mem_read32(cpu, cpu->gpr[13] - 26524u)) {
      static s32 said = -2;  // once per room, ED_COOP_LOG
      if (getenv("ED_COOP_LOG") != NULL && said != (s32)cpu->gpr[4]) { said = (s32)cpu->gpr[4];
          fprintf(stderr, "[coop] player 2's object passed over by the game's zone look for others (zones 0x%08X): their own look runs their zones (frame %llu)\n", cpu->gpr[4], (unsigned long long)(cpu->timebase / 675000ull)); }
      cpu->gpr[3] = 0u; g_hle_passthrough = false; g_hle_tail_call = false; return;
  }

Why this is safe:
- The answer is unused: 0x801335C8 is `or r3,r31,r31`.
- The out record (r1+104) is written by the scan only when a zone script moved the object (0x80134B98..0x80134BCC). Skipping is the same as "no script moved them".
- Every mover reaches the scan through 0x801335C4 (behaviour pass 0x801FA494, root motion 0x801241B4, body push 0x8004582C), so the lr test covers all of them.
- Player 2's own looks run with r13-26524 set to them (coop_prompt_look, coop_be_player2 brackets, ED_COOP_P2_STEP), so they fail the `!= r13-26524` test and are untouched.
- Solo is untouched (g_coop_p2 == 0 and mod_coop off).
- Monsters are untouched. Their shared bit 1 is no longer flipped by player 2, which used to make other_enter and other_leave alternate every frame when a monster and player 2 were on opposite sides of a zone edge (inferred from the shared bit; not seen).

Rejected alternative: bracketing that scan with coop_stay_room_in plus coop_zones_swap. fn23 would then answer for player 2, and 2738 would clear player 2's prompt but run 146 as the game's, with player 1 the player. The prompt would go to the wrong owner.

Risks:
(1) Co-op play that today works only because player 2 trips an other_* field. Most likely the L06 traps (TrapRgn enter plus other_enter): after the fix player 2 trips them only through enter in their look. That is the intended bracket, and it should also end a likely double trip when together (inferred). Needs an L06 trap run.
(2) Any region a companion character is meant to trip, if player 2 was standing in for one (none known).
(3) The kit: tools/ed/patterns.py and docs/game/catalogue/index.md call other_* runs "uncovered". After the fix they should read "never for player 2" (the patterns rule, then `ed build` / catalogue regenerate). Also update docs/coop-next-session.md's fix list (a) and bug (3), and doors.md:81.

Chapter 4, JTG_P1=4 (Karim), JTG_P2=2 (Ellia: model 70, so script 132's ladder in 338 sends them to 339, not room 5; read from the script's model_of != 150 test, inferred for their bracket). Chapter 4's control frame isn't measured. First run `JTG_FRAMES=300 sh tools/ed/jtg.sh 4 elev-probe`, read "[jtg] both players in control at frame F", and use ELEV_BASE=F+30. Every run is a fresh Jump to Game with ED_RUN_AWAY=1. jtg.sh already sets ED_COOP_TRACE=1, ED_ROOM_LOG=1 and ED_COOP_LOG=1. All frames below are relative to the base, except ED_SET_FLAG and ED_RUN_SCRIPT, which the script shifts itself.

Run ref (follow mode, ED_ROOM_SLOTS=2 with ED_COOP_STAY unset, so player 2 is brought along). This is the control.
- ED_SET_FLAG +20:91 (143 needs flag 91).
- ED_RUN_SCRIPT 144 at +50 and +110: the door into 190/EntryMkr02 (the first run sets its prompt, the second runs fn66).
- +450: put player 1 at (0,0) (ed where: ElevatorRgn01, ElevatorRgn02, Region01 2360) and player 2 at (400,-500) (CameraRgn01 only). +470: a small push on player 1's stick so their look runs.
- +700: player 1 presses B. +800, +860, +920: A.
- Expect, before and after the fix: "[trace] script run r3 143 r4 ... lr 0x8000704C" (player 1's prompt runs 143), then "[room] change 0x000000BD" (room 189), at about +1100 to +1300.
- If B ran 2360 (Region01's examine, slot 4) instead, the slot order wins. Then put player 1 where only ElevatorRgn01 applies and re-run.

Run lift: as ref, plus +600 put player 2 at (150,0) (inside ElevatorRgn02, outside ElevatorRgn01).
- Before the fix: "[trace] script run r3 2726 r4 ... lr 0x80134B38 who <player 1's id>" at about +600, then no "script run r3 143 ... lr 0x8000704C" after the B at +700, and no change to 0xBD by the end. This is the bug.
- After the fix: no 2726 line, one "[coop] player 2's object passed over by the game's zone look for others", and the same 143 run and change to 0xBD as ref.

Run mon189 (follow mode). This one is checked by log lines only.
- ED_RUN_SCRIPT 150 at +50 and +110: into 189/EntryMkr02.
- +450: player 1 at (33,-2347) (ElevatorRgn01 and MonsterCheckRgn00 at z 0), player 2 at (33,-1800) (outside both).
- Player 2 is then put at (33,-2240) at +600 and +800, and back at (33,-1800) at +700 and +900.
- Before the fix: "script run r3 2738 r4 ... lr 0x80134B38" at +600 and +800, each followed by "script run r3 146 ... lr 0x80164488" (fn73's inner run), and "script run r3 2301 ... lr 0x80134B38" at +700 and +900.
- After the fix: none of these, only the "passed over" line.

Run apart (default stay mode, both switches unset).
- ED_SET_FLAG 91. ED_RUN_SCRIPT 144 twice: player 1 goes to 190 alone, player 2 stays in 338.
- +400: put player 2 in 338 Region01 (-940,-85). +420: lup. +500: b. That is player 2's own door to 339 ("[stay] player 2's change to room 339 runs as theirs").
- +800: put player 2 in 339 Region01 (-248,1377). +820: lup. +900: b, their door to 190.
- +1150: put player 1 at (300,-400) and player 2 at (0,0). +1170: lup. +1250: b on their 143 EXAMINE. +1350, +1410, +1470: a for Yes. Player 2 rides down to 189 ("[stay] player 2's change to room 189").
- +2100: put player 1 at (0,0) in 190 and push the stick, so they get 143's EXAMINE.
- +2250: put player 2 at (33,-1800) in 189. +2350: at (33,-2240).
- +2450: player 1 presses B. +2550, +2610, +2670: A.
- Before the fix: "script run r3 2738 ... lr 0x80134B38" at about +2350, then "script run r3 146 ... lr 0x8000704C" for player 1's B, and "[room] change 0x000000BE" (player 1 sent to 190's EntryMkr04, not 189).
- After the fix: no 2738, "script run r3 143 ... lr 0x8000704C", "[room] change 0x000000BD", and both.tsv p1room = p2room = 189 at the end.

Run solo (JTG_SET mod_coop=0, the ref presses): the same change to 0xBD as ref and no "passed over" line, before and after the fix.

Every run: no ASSERT, no new saves/reports/hang-*.txt, `ed check` clean.

</details>

## 5. ed177's play-start scripts run as whoever is the player then

**The bug.** ed177 (on_play_script, 0x8000F9C4) keeps one script id in r13-24248. Mode 2's enter (0x80008234) runs it once, at 0x8000827C, whenever play resumes after a screen that stops it. hle.c never says whose script that is, so it always runs as player 1, with player 1's zones and room. When player 2 is the one who stored it, the script's in_zone / fn25 tests answer for player 1. The script then does nothing, and it also fails to re-arm itself.

**What the code shows.**

WHAT THE CODE DOES (read):
- ed177 at 0x8000F9C4..0x8000FA2C converts its argument and stores it with `stw r0,-24248(r13)`. Nothing else is stored with it: no owner, no room.
- Mode 2's enter at 0x80008260..0x8000827C does `lwz r3,-24248(r13)`. If it is above 0, it clears the word and calls 0x801642D4(script,0,0). The return address is 0x80008280. Then 0x80008280..0x800082A8 runs r13-26324 (ed9's left script; its return address is TOME_SCRIPT_RAN 0x800082AC).
- Other places clear the word:
  - 0x80083CD0, in 0x80083CA4 when its r4 != 0. That function compares r3 against 89 and 83, so it is a room routine [inferred].
  - 0x800AD7C0, when the stored room != r13-23276.
  - 0x800ADA7C (b0-k7-readers.md, the ed145 ticket).
- When mode 2 is entered: per the mode table (docs/research/separate-rooms/b0-k7-readers.md section 1, use-mix.md:231), on leaving mode 3 (the real Tome), 4 (a film), 6 (an in-engine cinematic), 7/8/10-13 (Tome-like screens), and after level start or a load [inferred from the table, not traced each].
- Player 2's Tome in a copy (tome_copy_step) never leaves the real game's mode 2. tome_copy_left (hle.c ~31945-32124) only handles r13-26324, so a copy Tome never runs the stored script [inferred from the comments].

HOW HLE.C HANDLES THE RUN (read):
- 0x801642D4 is intercepted by coop_run_script (hle.c:20550, installed at hle.c:33330).
- No branch tests lr 0x80008280. The defines nearby (TOME_SCRIPT_RAN 0x800082AC at hle.c:20221, PROMPT_USE_RAN, WINDOW_CLOSE_RAN, ...) do not include it.
- So the run falls through to `g_hle_passthrough = true` / coop_script_cameras (hle.c ~20800-20815) and runs natively as whoever is current.
- Mode 2's enter runs outside any turn, so that is player 1: their zone words (fn23 0x8016A7DC reads +104 bit 0) and their room in r13-23276.
- There is no hook on 0x8000F9C4 (`ed fn ed177`: no dispatch entry). Nothing in hle.c names 24248.

WHAT THE 29 SCRIPTS ARE (read with `ed script`):
- Room 83:
  - 1188 is HealthRgn/SanityRgn/MagicRgn enter. It re-arms ed177(1188) every time, then sets fn52 track 669/670/671 to full for the pool the player is in.
  - 1189 (leave) turns all three down and calls ed177(-1).
  - Room 83 is in every level's tables, with the same scripts everywhere. It has four TeleportRegions (381) back to spawn markers, the Obelisk and Body examines, and refuses Chapter Pages (chapter-page-readers.md).
  - It is entered only from game code: transition 16 at 0x800E8E14, in kind 39's handler (transitions.md:291). Which place in the story it is, I did not establish.
- L03 room 150:
  - 1011 is the room's arrival. 933 is MuffledVoicesRgn01 enter.
  - Both run the same code: if in_zone(MuffledVoicesRgn01), then ed177(933) and stream_play 40 at MuffledVoicesMkr01. The re-arm is inside the zone test.
  - 934 (leave) is StopAudioStream(40,30).
- L03/2465 (room 152): ed177(2211), then if no item 105 is equipped, the DARK_AREA words. 2211 checks fn25==152 and in_zone(Region04), then run_next 2465.
- L05/929: if in_zone(CricketSoundRgn01), then ed177(929) and group_volume.
- L08/2446 stores 2098. 2098 checks flag 577, fn25==213 and in_zone(SphereRegion02), then stream_prepare and re-arms.
- L09/L12 2196 stores 1427. 1427 sets track 720 by fn25 (274 or 93) and flag 623.
- These only depend on flags, never on the player: L04/2346 (486: track 501), L06/1981-1983/2270 (2245: group volumes, send_event to 555), L06/1987 (flag 809, music, track 485), L10/2018 (tracks 221/222, texture anims), L11/2407 (track 719, model_off).

WHAT THE STORE IS FOR [inferred]: putting the room's ambient sound or stream back after play resumes. All 29 stored scripts only set sound or stream volumes, plus flag-driven model and texture state, and one words window (2465). The kit's own note calls this a guess too.

IS IT A CO-OP BUG (read and inferred): yes, a real one, but cosmetic.
- Who stores them: zone-enter scripts, which run for whoever walks in (player 2 in their own zone scan, lr 0x80134B38), and room arrivals.
- When player 2 stored it (they stand in the zone, player 1 doesn't) and player 1's real Tome, a film or a cinematic ends:
  - The run asks player 1's zones and room, so the sound or stream is not put back.
  - In 933/929/2098 the re-arm sits inside the zone test, so the word is gone for every later resume too.
  - 2211 does not re-show 2465's DARK_AREA words.
- With the players apart, it is one word for two rooms: the last storer wins, and the other player's room gets nothing back.
- In solo, the one player is always the one who stored it, so the behaviour is unchanged.
- Nothing gameplay-relevant changes: no flags that gate progress, no holds, no doors.

**Fix plan (the skeptic's corrected version).**

Low priority: after bugs (1)-(3) and fix list (a). It changes sound and stream restores only, plus one words window (L03/2465 via 2211). Nothing in coop-failed-attempts.md covers it.

1. Record WHO stored, not what. In find_coop, next to hle.c:33327-33338, add `HleDispatchEntry* e = hle_dispatch_entry(0x8000F9C4u); if (e && !e->notify && !e->intercept) e->notify = coop_play_script_left; else fprintf(stderr, "[coop] on_play_script (0x8000F9C4) not hooked: player 2's play-start scripts run as player 1's\n");`. It works on a table-reached function, as fn80's coop_script_sends hook does, and it is published before publish_codegen_options.
   - Body: `if (!g_settings.mod_coop || g_coop_p2 == 0u || !sda_ready(cpu)) { g_play_script_p2 = false; return; } g_play_script_p2 = mem_read32(cpu, cpu->gpr[13]-21496u) == mem_read32(cpu, g_coop_p2+16u);` (the identity test at hle.c:20632).
   - Do not read r3 as the id. r3 is the script context; the id is read later by 0x80163510 and stored at 0x8000FA2C.
   - Under ED_COOP_LOG: `[coop] on_play_script left by player %d (frame %llu)`.
2. `#define PLAY_START_RAN 0x80008280u` beside TOME_SCRIPT_RAN (hle.c:20221).
3. In coop_run_script, a separate block just before `const bool tome_item` (hle.c:20675), after the trace, so the `lr 0x80008280` line still prints.
   - Condition: `cpu->lr == PLAY_START_RAN && (s32)cpu->gpr[3] > 0 && g_play_script_p2 && g_coop_p2 != 0u && !g_coop_use.active && sda_ready(cpu) && coop_listed(cpu, g_coop_p2)`.
   - Clear g_play_script_p2 first. A re-arm inside the run sets it again, because the identity is player 2 there.
   - Then do the same four swaps as the tome_item branch, without g_coop_tome_cast, g_coop_turn_p1 or the scene parts: coop_be_player2(true), coop_zones_swap(info, true) from COOP_CHARACTER_INFO, coop_stay_room_in, hle_guest_call(COOP_RUN_SCRIPT, args), then undo in reverse.
   - Finish with lr restored, g_hle_passthrough = false, g_hle_tail_call = false, return.
   - Under ED_COOP_LOG: `[coop] play start's script %d (left by player 2) ran as player 2's (frame %llu)`.
   - No id match is needed: only ed177 writes r13-24248, the owner is updated on every store, and stale owners after the game's clears are overwritten by the next store.
   - Known leftover: ed177's error path (no store) still sets the owner.
4. Solo: g_coop_p2 == 0 and mod_coop off leave the flag false, so the run stays native. When player 1 stored it, it also stays native.

Risks:
- 2211 -> 2465 opens a DARK_AREA words window as player 2 at mode 2's enter. It is untested. Either check it in L03 room 152 (Region04) or skip the block for that script.
- One slot only: the last storer wins.
- The room-change clears (0x80083CD0, 0x800AD7C0) can wipe the other player's store when the players are apart.
- Leave scripts (934, 1189) silence a shared stream or track for both players. That is a separate issue.

**Test plan (the skeptic's corrected version).**

WHERE: chapter 3 (Anthony, level 03). It starts in room 150. Measured in more than 20 existing runs: control at frame 2114-2124, P1 at (-753,-30,2) and P2 at (-753,120,0) (EntryMkr01), both outside MuffledVoicesRgn01 (box x -301..475, y 155..1048, z 0..288; enter 933, leave 934). No door script and no flag are needed. Use the default JTG_BASE 2300.

Every run: a fresh `sh tools/ed/jtg.sh 3 NAME` with ED_RUN_AWAY=1 ED_MODE_LOG=1. jtg.sh already sets ED_COOP_LOG, ED_COOP_TRACE and ED_ROOM_LOG. Also JTG_FRAMES=2100.

Presses (relative to the base):
- p2in (the bug): JTG_PUT="100,2,300,400,-150" puts player 2 inside the zone. The ray starts at z 150, below the z 300 surface an old put hit. If `[sweep] player 2 put at` shows Z outside 0..288, use 365,180,-150 instead (where player 2 stood in the zone in logs/run-20261106-142801-jtg-rega5-c3a-m5 at 7977).
  - JTG_P1_SCRIPT="900,906,0x1000;1200,1206,0x1000;1500,1506,0x1000;1800,1806,0x1000": player 1's real Tome opened and closed twice. That Start also closes the Tome is inferred; if `[mode] 3` stays, close with 0x0200 B.
  - The opens come after 933's wait(500) ends. Check that `r3 933 r4 0 lr 0x80163E5C` comes before the first `[mode] 3`; if not, move all presses later.
- p1in (control): the same, with JTG_PUT="100,1,300,400,-150".
- solo: JTG_SET="mod_coop=0", player 1 put there. `[mode]` lines may not print in solo [inferred, coop_pad_read]: use the lr lines.

Grep page-*.log for: `[sweep] player`, `script run r3 933 `, `calls 0x8016A7DC`, `calls 0x8000F9C4`, `calls 0x80165020`, `[mode] `, `on_play_script left by`, `play start's script`, `ASSERT`. The `calls` lines belong to the nearest `[trace] script run` above them. List saves/reports/hang-*.txt before and after.

BEFORE the fix (p2in):
- `script run r3 933 ... lr 0x80134B38 who <P2ID>` (P2ID is not 1, e.g. 76), then `calls 0x8016A7DC`, `calls 0x8000F9C4`, `calls 0x80165020`.
- After the first `[mode] 2 (asked 2)` that follows `[mode] 3`: `script run r3 933 r4 0 lr 0x80008280 who 1`, followed only by `calls 0x8016A7DC`.
- After the second close: no `lr 0x80008280` line, because the word was not re-armed.
- p1in and solo: `lr 0x80008280 who 1` after BOTH closes, each followed by `calls 0x8016A7DC`, `calls 0x8000F9C4`, `calls 0x80165020`.
- The same shape is already in logs/run-20261106-142801-jtg-rega5-c3a-m5, frames 7977 and 9671.

AFTER the fix (p2in):
- `[coop] on_play_script left by player 2` at the zone entry.
- At each close: `lr 0x80008280 who 1`, then `[trace] script run r3 933 r4 0 lr 0x0BADF010 who <P2ID>` (the guest call) with `calls 0x8016A7DC`, `0x8000F9C4`, `0x80165020`, then `[coop] play start's script 933 (left by player 2) ran as player 2's`.
- A second `lr 0x80008280` follows the second close.
- p1in shows `on_play_script left by player 1` and no `play start's script`. Solo shows neither line. Both are otherwise unchanged.
- In every run: no ASSERT and no new hang report.

Only a run can show:
- whether the put lands in the box;
- whether a put (rather than a walk) fires the zone's enter for player 2;
- whether Start closes the Tome;
- whether stream 40 is actually stopped by the Tome. Audio is muted, so the restore can't be heard.
Room 83's pools can't be reached with the current tools.

**What only a run or a play can show.** Only a play or a run can show these:
- Whether the Tome, a film or a cinematic actually stops stream 40 or the pool hums, which would make the missing restore audible. Audio is muted under ED_RUN_AWAY, and no log reads the stream records at 0x805DB5C0. ED_STREAM_LOG disc reads go on either way, so they can't tell a restart from a stream that never stopped.
- Whether ED_RUN_SCRIPT=898 brings player 2 into room 150 with player 1, and where player 2 lands.
- Whether flag 77 keeps 1011's first-visit scene from holding the players.
- Chapter 3's real control frame.
- Whether JTG_PUT's floor at (100,600) is the ground floor.
- Whether player 2's zone scan runs 933 as theirs here, which the `lr 0x80134B38` trace would show.
- Whether hle_dispatch_entry has an entry for 0x8000F9C4.
- How a words window (2465 via 2211) behaves when run as player 2 at mode 2 entry.
- Room 83 can't be reached by a door script (`ed door 83`: no ways in; only game code, kind 39), so its pools can't be tested with the current tools.

**The skeptic's notes.**

- Fix step 1 is wrong as written: `g_play_script = (s32)r3` at ed177's entry does not get the script id. Disassembly of 0x8000F9C4 (`ed dis 0x8000F9C4`) shows r3 is the script context (`or r31,r3,r3`). The id comes from `bl 0x80163510(r31,1)`, a float, then `fctiwz` and `stw r0,-24248(r13)` at 0x8000FA2C. At entry the id has not been read yet. Record only who called ed177 there. The id is r3 when coop_run_script sees lr 0x80008280.
- The id comparison in step 2 (`(s32)gpr[3] == g_play_script`) is not needed and would never match. Only ed177 writes r13-24248, and the entry hook updates the owner on every store (ed177(-1) included). Mode 2's enter only calls when the word is above 0. A stale owner after the game's own clears (0x80083CD0, 0x800AD7C0, 0x800ADA7C) is overwritten by the next store. One small leftover: ed177's error path (0x80163414 != 1, at 0x8000F9E4..0x8000FA0C) prints and stores nothing, but the entry hook would still record an owner. That is rare and only matters if the word was stored earlier by the other player.
- The 'who' field is not a node pointer. trace_look prints `mem_read32(r13-21496)`, a player id. Old logs show `who 1` for player 1 and `who 76` (another run: 59) for player 2. Read player 2's id from their own zone line in the same run.
- The plan says the guest run's trace line shows who = player 2. That is right but incomplete. hle_guest_call sets lr = GUEST_CALL_SENTINEL 0x0BADF010 (runtime/host/main.c:54), and the guest call re-enters coop_run_script. Old logs have `[trace] script run r3 419 r4 0 lr 0x0BADF010 who 59`. So after the fix, two lines follow each Tome close: first `lr 0x80008280 who 1` (traced before the swap), then `lr 0x0BADF010 who <p2 id>`. The 'calls' lines belong to the second.
- The test route is unnecessary and adds risk. Chapter 3 (Anthony, level 03) already starts in room 150. Over 20 existing runs report `control at frame 2114..2124, room 150: P1 at -753 -30 2, P2 at -753 120 0` (EntryMkr01, outside MuffledVoicesRgn01), and the solo run logs `[room] change 0x00000096` at 1416. Running ED_RUN_SCRIPT 898 from inside room 150 would reload the room at EntryMkr02 (218,865,993) for no reason. ED_SET_FLAG 77 is not needed either: control already comes with 1011's start scene done. JTG_BASE needs no measuring: the default 2300 is after control.
- Position (100,600,20) is risky. ED_PUT casts a ray from z+300 down to z-1500 (hle.c ~17020) and takes the first floor it hits. An old run put player 1 at (90,950,0) and they landed at z 302, above the region's max z 288, so outside the zone. Use a low z (-150) so the ray starts at 150. Check the logged `[sweep] player 2 put at X Y Z` shows Z in 0..288. A spot known to work: player 2 stood at (365,158,0) when 933 fired for them (logs/run-20261106-142801-jtg-rega5-c3a-m5, frame 7977).
- 933 ends with wait[fn36](500). The script stays parked and goes on later at lr 0x80163E5C (seen in logs/run-20261106-052058-jtg-rega3-c3b-r3: 6606, then 7257). The plan's Tome at +200 after the put would land inside that wait, where what 0x801642D4 does with a parked instance is unknown. Open the Tome after the `r3 933 r4 0 lr 0x80163E5C` line.
- The grep string `room\] change` is real, but `on_play_script` and `play start's script` only exist once the fix is in. ED_MODE_LOG's `[mode]` line is printed from coop_pad_read (hle.c:32355). Whether it prints with mod_coop=0 is not established: an existing solo run had no ED_MODE_LOG set. In solo, rely on the `lr 0x80008280` lines.
- The script's `ls saves/reports/hang-*.txt | tail -1` also shows hang reports from older runs. Compare against a list taken before the run.
- Reading points that do hold (checked): TOME_SCRIPT_RAN 0x800082AC at hle.c:20221; coop_run_script at hle.c:20550, installed at hle.c:33332. No branch tests lr 0x80008280, so the run passes through as player 1. Existing log evidence: logs/run-20261106-142801-jtg-rega5-c3a-m5 frame 7977 has `r3 933 ... lr 0x80134B38 who 76`, followed by `calls 0x8000F9C4` and `calls 0x80165020`. Frame 9671 has `r3 933 r4 0 lr 0x80008280 who 1`, followed only by `calls 0x8016A7DC`. In that run player 2 had already left (934 at 8000), so no restore was correct there; it confirms who runs the script, not a wrong outcome. hle_dispatch_entry gives an entry for any aligned code address. A notify on a table-reached script function has a precedent: fn80 0x8016607C, coop_script_sends. find_coop runs before publish_codegen_options (hle.c:34163/34167), so the new hook becomes a chain stop. coop-failed-attempts.md has nothing on ed177, r13-24248 or sound restores.

<details><summary>The reader's first fix and test plans</summary>

Worth doing only after bugs (1)-(3) and fix list (a): it changes sound only, plus one words window. docs/coop-failed-attempts.md has nothing on ed177, r13-24248 or sound restores, so no earlier attempt is repeated.

SMALLEST CHANGE (co-op only):

1. Record who stored the script.
   - Add a notify on 0x8000F9C4 (ed177's entry) in find_coop's install block, next to hle.c:33330-33338. Use the same pattern: `HleDispatchEntry* e = hle_dispatch_entry(0x8000F9C4u); if (e && !e->notify && !e->intercept) e->notify = coop_play_script_left;`.
   - Its body: `g_play_script = (s32)r3; g_play_script_p2 = g_coop_p2 && sda_ready(cpu) && mem_read32(cpu, r13-21496) == mem_read32(cpu, g_coop_p2+16);` (the same identity test coop_run_script uses at hle.c:20632).
   - Under ED_COOP_LOG: `fprintf(stderr, "[coop] on_play_script %d left by player %d in room %d (frame %llu)\n", ...)`.
   - I have not checked that hle_dispatch_entry gives an entry for a table-reached script function. If it gives none, print one line saying so [inferred].

2. Run it as player 2 when it is theirs.
   - Define `#define PLAY_START_RAN 0x80008280u` beside TOME_SCRIPT_RAN (hle.c:20221).
   - In coop_run_script, just before the `tome_item` const (hle.c:20675), add: `const bool play_start = cpu->lr == PLAY_START_RAN && (s32)cpu->gpr[3] > 0 && (s32)cpu->gpr[3] == g_play_script && g_play_script_p2;`.
   - Clear g_play_script_p2 there.
   - Add `play_start` to the `(tome_item || arrival_scene || p2_scene_end)` condition at hle.c:~20702. That branch already does what is needed: coop_be_player2, coop_zones_swap (so fn23 answers for player 2), coop_stay_room_in (so fn25 answers with player 2's room when apart), a guest run of 0x801642D4, then the undo.
   - Under ED_COOP_LOG, log `[coop] play start's script %d (left by player 2) ran as player 2's (frame %llu)`.
   - That branch also sets g_coop_tome_cast. Better still, keep the new path out of it: put the same four swaps into a small separate block, so no Tome-cast behaviour leaks in.

Solo is untouched: everything is gated on g_coop_p2 != 0 and g_settings.mod_coop. When player 1 stored the script, the run stays native, as now.

RISKS AND WHAT ELSE IT TOUCHES:
- 2211 -> run_next 2465 -> ed220 examine_plain (DARK_AREA) would open a words window as player 2 inside a guest call at mode 2 entry. Player 2's words have their own routes (coop_p2_words_call, g_coop_screen), and this path is untested. Either exclude it by testing that the script is not 2211, or check it separately in L03 room 152, Region04.
- Only one store survives. If both players stood in zones (room 83's pools, say), only the last storer's sound is put back. That is still one slot, as in the game itself.
- The 0x80083CA4 and 0x800AD7C0 clears on room changes can wipe the other player's store when apart. Those are not changed, and that limit stays [inferred].
- Global sound: 1189 and 934 (leave) silence the track or stream for both when either player leaves. That is a separate, unfixed shared-audio issue and not part of this fix.

WHERE: chapter 3 (Anthony, level 03), room 150.
- The case: L03/933 (MuffledVoicesRgn01 enter) stores 933, and the re-arm is inside its zone test.
- Getting there: ED_RUN_SCRIPT=BASE+100:898 teleports both players in through L03/898 (room 147 ExitRgn02 -> room 150 EntryMkr02 at (218,865,993)). That is the balcony, outside the region box [-301..475, 155..1048, z 0..288]. Player 2 follows player 1 through the door [inferred].
- ED_SET_FLAG=BASE+90:77 sets flag 77, so arrival 1011's first-visit scene (state 0: cutscene, Camera11) should not hold anyone [inferred]. If it does, drop the flag and move all presses 600 frames later.
- JTG_BASE: chapter 3 is not measured (default 2300). Run once with no presses, read "[jtg] both players in control at frame F", and use F+30 (PLAY_BASE in the script).

PRESSES (relative to JTG_BASE):
- Run p2in (the bug):
  - +300: JTG_PUT moves player 2 to (100,600,20), inside MuffledVoicesRgn01. `ed where 150 100 600 20 --level 3` shows MuffledVoicesRgn01 and CameraRgn07. Player 1 stays on the balcony.
  - +500: player 1 Start (0x1000) opens the real Tome.
  - +800: Start closes it.
  - +1000 and +1200: Start again, a second open and close.
  - Run 1500 frames.
- Control run p1in: the same, but player 1 is put at (100,600,20) and player 2 stays on the balcony.
- Run solo: mod_coop=0, player 1 put in the zone.

READ IN THE LOGS (ED_COOP_LOG, ED_COOP_TRACE, ED_MODE_LOG):
- Player 2's zone entry: `[trace] script run r3 933 ... lr 0x80134B38 who <player 2's node>`. Then `[script] calls 0x8000F9C4` (ed177) and `[script] calls 0x80165020` (fn126). The call record's first word is the function's address [inferred].
- Each Tome: `[mode] 3 ...`, then `[mode] 2 ...`.

BEFORE THE FIX (p2in):
- After the first `[mode] 2`: `[trace] script run r3 933 r4 0 lr 0x80008280 who <player 1's node>`. Its calls list 0x8016A7DC (fn23) and no 0x80165020 or 0x8000F9C4.
- After the second `[mode] 2`: no `lr 0x80008280` line at all, because the word was not re-armed.
- p1in and solo show 0x80165020 and 0x8000F9C4 after each `[mode] 2`, and a run at both Tome closes.

AFTER THE FIX:
- p2in shows `[coop] on_play_script 933 left by player 2 in room 150` at the zone entry.
- At each `[mode] 2`: `[coop] play start's script 933 (left by player 2) ran as player 2's`. The guest run's `[trace] script run r3 933` line shows who = player 2's node and lists 0x80165020 and 0x8000F9C4.
- A second `lr 0x80008280` run follows the second Tome close.
- p1in and solo are unchanged from before: no `[coop] play start` line.
- In every run: no ASSERT and no saves/reports/hang-*.txt.

</details>

