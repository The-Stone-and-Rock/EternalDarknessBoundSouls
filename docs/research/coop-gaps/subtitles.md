# Subtitles in co-op (research, read only, nothing run)

2026-11-17. The question: `ed patterns` lists four subtitle calls as uncovered (EDStartSubtitlesForAlexClockPuzzle
L00/1551 and L12/1551, EDStartSubtitlesForGhostMaid L00/583, EDStartSubtitlesForInsanityPhoneCall and
EDCleanUpSubtitles L00/1801). The subtitle record 0x80308DE8 is one per game (tools/ed/player_data.tsv:67). This page
covers how a subtitle is started, timed, drawn and cleaned up; which half shows it in co-op; and what to fix.

Evidence is a guest address in build/ed-db/disasm.txt, a file:line, or a log. "Inferred" means read from code and not seen
in a run.

## 1. How the game does subtitles

### The record (0x80308DE8, 32 bytes, not 24)

All the writers are in 0x80008C6C..0x80008E54 (`lis r?,-32719; addi -29208` = 0x80308DE8):

| Offset | What | Written by |
|---|---|---|
| +0 | text group (-1: none) | starts (r4), 0x80008DD4 (-1), 0x80008E40 (-1) |
| +4 | the record table: 6-byte rows (start frame, line, frames) | starts (r3) |
| +8 | frame counter | starts (0), updater (+1 a frame) |
| +12 | frames left on the line being shown | updater |
| +16 | number of rows | starts (r5) |
| +20 | the line's text element (one of the ten in 0x805F87D0) | updater (0x80008A64, 0x80008C00), freed by 0x80008D7C / 0x80008DD4 |
| +24 | wait mode: 0 none, 1 until the sound's stream exists, 2 until the music stream is this one | starts |
| +25 | still waiting (byte) | starts, updater |
| +26 | show even with the Subtitles option off (byte) | 0x80008DC0 sets 1 |
| +28 | sound or stream id waited on | starts (r6) |

The code writes up to +28 (`stw r6,28(r8)` at 0x80008D6C), so the record is 32 bytes. player_data.tsv says 24, which
should be corrected. That matters for any copy or swap of the record.

### Starting one

- **0x80008D48(table, group, rows, sound)**: mode 1, wait on.
  - Called by the six script natives: EDStartSubtitleForEdward 0x8000911C (bl at 0x80009178), PhoneCall 0x80009194
    (0x800091F4), GhostMaid 0x80009210 (0x80009270), AlexClock 0x8000928C (0x800092E8), MikeDestroyThis 0x80009304 (0x80009360).
  - Also called by the rune-scribing A handler 0x80080308 (Edward's lines 42-45, group 7; input mode 9/10 in chapter 13, bindings
    0x80244840, docs/game/research/verbs.md:637).
  - Each start overwrites the whole record except +20. A start never frees a line already on screen. That line stays until
    the new record's first line replaces it (0x80008A14 calls 0x80008D7C first) or until EDCleanUpSubtitles.
- **0x80008C6C(table, group, rows)**: mode 0, no wait. Used by:
  - three creature steps, 0x800CEBA8, 0x800D2390 and 0x800D4B38, each followed by 0x80008DC0 (always shown) and with
    tables picked by the language word r13-26284 (primitives.tsv ed241: 1 F, 2 G, 3 S, 4 I). Which creature these are was
    not identified.
  - a Tome menu, 0x80107B28 (bl at 0x80108044), which plays an audio clip (0x8022ACD0) with subtitles. It reads as the
    replay viewer (docs/game/research/kit-groundwork.md:61).
- **0x80008D0C(table, group, rows, stream)**: mode 2. Used by the event-91 handler 0x8006CEAC (0x8006D108 and
  0x8006D124): a streamed speech in text group 3, ended by event 92 with 0x80008DD4 (0x8006D154). Not identified.
- 0x80008CA0 restarts a mode-2 record (the music tracker, 0x800AB5A8). 0x80008E40 marks the record empty at the game-mode
  start 0x80021648 (0x8002176C). That is a mode switch, not an ordinary room change (inferred from its callers
  0x800186E4 and 0x800226CC).

### Timing: the updater 0x800088A0(view)

- **Callers.** Three: the world frame (`li r3,2` at 0x8000852C, call 0x80008530, right after 0x801DA574 draws all
  elements at 0x80008528); the title/menu frame (0x80020278); and the Tome's frame (`li r3,3`, 0x801115D8).
- **Text pack.** View 3 takes its lines from r13-26268, the Tome's own copy of the subtitle pack. It is loaded at Tome
  open (0x80110894, part 3 of the Tome's file r13-24300) and dropped at close (0x80110AB0, 0x80111724). Any other view
  takes r13-26264, play's pack (0x80021B04).
- **Waiting.** Mode 1 waits until 0x801A4DCC(+28), the stream record for that sound, exists (0x80008958). Mode 2 waits
  until 0x801A19C8() is +28 (0x8000893C). While it waits nothing counts.
- **Counting.** +8 goes up by 1 a call. When a film or audio clip is playing (0x8022AAE0 nonzero), the updater instead
  catches up to that clip's frame, 0x8022AAF4 (0x800088C8..0x800088D8, loop at 0x80008C34..0x80008C54). When
  r13-26904 is set, every fifth frame counts twice (0x80008AFC..0x80008B64), which reads as a 50/60 Hz correction
  (inferred).
- **Making a line.** When +8 equals a row's start, the old line is freed (0x80008D7C) and the new one is made with
  `0x801DB374(pack, group, line, 0x10008, 1, "Subtitles" 0x8023BDF4)` (0x80008A60 and 0x80008BFC). This happens only
  when bit 0 of 0x80307180 (the Subtitles option, docs/research/in-game-menu/04-options-subpages.md:148) is on, or +26 is
  set. +12 then counts that row's frames down, and the line is freed at 0 (0x80008C14..0x80008C30).
- **Ending.** When +8 passes the last row's start plus frames, the updater calls 0x80008DD4 itself (0x800089D0).
  EDCleanUpSubtitles is only a safety net.
- **Debug.** With r13-24724 on, the updater prints " Audio SMPTE" and the time (0x80008A68..0x80008AF8).

### Drawing

- **Element pool.** The line is one of the ten text elements at 0x805F87D0. These are shared with the words windows,
  the prompts and the find's words (docs/coop-level3-design.md:15, 207). All ten are drawn each frame by 0x801DA574,
  which runs before the updater in the same world frame, so a new line first shows a frame later.
- **Where the line goes.** Its place is the "Subtitles" style's x and y. Nothing in the updater moves it (the style's
  numbers were not read; it is at the bottom of the screen in solo play, inferred).
- **Which half of the split.** Decided by x alone:
  - Player 1's half: the game's frame, with HUD.
  - Player 2's half: no HUD except draws past the right edge, which are brought back by 4096 (interp.cpp:762-764, 848-855,
    kHudAway 568).
  - While player 2 has the screen (hle_coop_screen) or the cameras are lent: player 2's half is the game's frame with
    HUD, and player 1's half hides the HUD except draws past 6000 (camera.c:1641-1675, interp.cpp:842-847).
- **The trace.** The line goes through the words layout hook, so it shows in `[trace] words:` lines (hle.c:21527; seen:
  logs/run-20261115-210507-jtg-dream-end-story/page-0001.log:1832, "Remember me, Alex?").

### Other subtitle sources, not this record

- **In-engine cinematics** (mode 6, cin*.bin) have their own subtitle step, 0x8004FFC4. It is called from the cinematic
  frame at 0x800EBA24; it makes its line with 0x801DB374 at 0x800500B8 and frees it at 0x8005013C, and its lines are drawn by
  0x801DA574 (0x800EBA34). In co-op a cinematic is drawn whole, because the split goes off for a cutscene
  (camera.c:1633-1635; seen "[split] off ... (a cutscene)" 187 times in logs/run-202611*). So both players see the same
  thing, and nothing is per player. The dream's "Remember me, Alex?" and "The Darkness comes." are of this kind (log
  above, room 96, scene 2027).
- **Films** (mode 4 FMV) and their replays: drawn whole (`g_movie_file`, hle.c:3895). Their subtitle path was not traced.
- **The Tome's own uses** (0x80107B28) run in the Tome's frame. Player 2's Tome is a full RAM copy, so a subtitle
  started there is in the copy's record. That makes it per player already.

## 2. The four calls in co-op

| Call | Where | Lines | Record (from main.dol, primitives.tsv:407-409) |
|---|---|---|---|
| AlexClockPuzzle | L00/L12/L13 1551, room 78 Region04 (the study), the third wrong try (AlexClockTry >= 2) | group 6: "Three thirty three..." | (2,0,175), waits on stream 2 (fn126(2,...) at SanityMkr01, the line before) |
| GhostMaid | L00/583, room 77 ServantGhostRgn, also the arrival script of door 1081 (78 -> 77) | group 4: "Help me...", "Please help me..." | (8,0,44) (54,1,68), waits on stream 84 |
| InsanityPhoneCall | L00/1801, room 69 Region10 (the main hall), state 2, after ANSWER_PHONE (prompt 36, button 2) | group 8: "Hello?", "Remember me, Alex?" | (26,0,28) (75,1,124), waits on sound 479 (ed64(0,479,...)) |
| CleanUpSubtitles | L00/1801 state 5, about 290 frames after the start | | the record had already ended itself at frame 199 |

All four are shown only with the Subtitles option on (none sets +26).

None of the four holds anyone. The holds and the scene come from the scripts' own fn22, fn21 and fn111, which the host
already handles (`ed catalogue --script L00/1551` and the others).

### Who sees what (predicted; nothing here was run)

The rule from section 1: the line is drawn where the game's HUD is. That is player 1's half, unless player 2 has the
screen or the lent cameras, or the whole frame while a scene of player 1's is on.

1. **The clock, by player 2 (follow or apart).**
   - **Likely right.** Their EXAMINE gives them the close-up (seen: "player 2 has the close-up", "screen 1",
     logs/run-20261115-220457-clock-p2-icons/page-0001.log:1826, 2085). Their fn22(1,1) scene does not turn the split off:
     after their scene 2142 at frame 6567 there is no "[split] off" line in that run.
   - So "Three thirty three..." is expected in player 2's half while their close-up holds. The line ends at frame 177,
     inside the script's 220-frame wait.
   - **The risk** is the close-up being taken for over during the 220 frames. Then the line drops into player 1's half,
     and player 2's half would show no subtitle. This is a run-time fact: that branch has never been run (no
     "Three thirty" in any log).
   - In follow, player 1 hears the voice with no words (inferred).
2. **The clock, by player 1.**
   - Their fn22(1,1) is a scene of player 1's. The split goes off for it, as for scene 2027 (dream run log:1561), so both
     players see the whole frame and the subtitle (inferred for the clock).
   - Apart, player 2 in another room gets player 1's scene on the whole screen too. That is how every scene of player 1's
     behaves today, not something subtitles add.
3. **The phone, by player 2.** **Likely wrong.**
   - Nothing gates it to player 1 (no fn78(1,0) test, `ed herogates` does not list 1801). Player 2 can answer it in
     Region10.
   - Their press is a use, and fn22(1,1) inside it is "a scene of theirs", g_coop_script_hold. In that case the host gives
     them no screen (hle.c:20554-20566: `!g_coop_script_hold` is needed for g_coop_screen).
   - The same shape already went wrong once for a find: "its words went to the game's pad, player 1's, in player 1's
     half" (hle.c:20433-20436, the sword after Anthony's casket). That was fixed for finds by g_coop_assist_at; subtitles
     have no such step.
   - So "Hello?" and "Remember me, Alex?" are expected in **player 1's half**, and in none of player 2's. Apart, player 1
     may be in another room, where they read a phone call that isn't theirs. If player 1 has words up at the time, the two
     can sit on top of each other at the bottom of their half (inferred: the style's y wasn't read).
   - ed64(60,...) at state 5 picks its voice by the character in r13-26524. In player 2's bracket that is player 2's,
     which is right.
4. **The phone, by player 1.** A scene of player 1's: the whole frame, both see the lines (inferred).
5. **The ghost maid.**
   - **Turned away for player 2 by default.** L00/583's first test is `fn78(1,0) ~= 1` (player 1's id), which is a hero gate
     (hle.c:20631 kHeroGates `{0, 583, 0x1AB}`; `ed herogates`: "L00/583 pc 5 ... turns away"). With ED_COOP_HERO_ID unset,
     player 2 entering ServantGhostRgn, or arriving by door 1081, starts nothing. Only player 1 starts the scene, as in
     solo: the whole frame, both see it.
   - With ED_COOP_HERO_ID=1 (testing) it is a zone scene of player 2's: the phone's shape, so it is expected in player 1's
     half (inferred).
6. **Two subtitles at once** (two players in two places, which solo never has).
   - The second start overwrites the first. The first one's line stays on screen until the second's first line replaces
     it. If the second never stops waiting (its stream never plays), the old line stays until a clean-up.
   - The phone's EDCleanUpSubtitles at state 5 kills whatever subtitle is running then, whoever started it.
   - Example: player 1 answers the phone in room 69 while player 2 is on the clock's third try in room 78 (apart). One
     player loses their lines. Rare, but possible only in co-op.
7. **Player 1's real Tome up during a subtitle** (co-op only: the world keeps running behind it).
   - One world frame and one Tome frame run per shown frame (hle.c:16246-16250, 17578-17585; the Tome's frames skip the
     retrace).
   - The updater runs in both (0x80008530 and 0x801115D8), so +8 counts twice. The lines play at double speed, half as
     long.
   - A line that begins in a Tome frame is made from the Tome's pack r13-26268, and the two-mode swap then treats it as
     the Tome's element: hidden in the world's frames (tw_element_laid, hle.c:13728-13760).
   - Where it ends up on screen was not worked out (inferred).
8. **Player 2's copy Tome opened during a subtitle.**
   - The copy is a snapshot, record included. tome_copy_no_window (hle.c:16374) clears the words window, not the record,
     so the copy's Tome frame goes on with it.
   - That means player 1's subtitle can show on player 2's Tome page (inferred), while the real one goes on in the world.
     In solo a subtitle does carry on over the Tome, which is why the Tome's frame calls the updater at all.

### Which of the four matter in play

- **The phone (L00/1801): yes.** Player 2 can trigger it, and its lines most likely land in player 1's half.
- **The clock (L00/L12 1551): probably fine.** The user already plays player 2 at this clock (2026-11-15). Its lines
  should be in player 2's half; one run of the third try is needed to confirm. L12 has the same script, but no door into
  room 78 is known in level 12 (docs/research/kit-bugs/build-run-duck.sh).
- **The ghost maid (L00/583): fine as shipped.** Player 1 only (hero gate). It becomes the phone's case if the hero
  gates are opened.
- **EDCleanUpSubtitles: matters only for overlap** (case 6).
- **Outside the four, the same rule decides:**
  - Edward's L09/L12/L13 2204 and the chapter 13 rune lines.
  - Mike's L11/1521 and 1805.
  - The always-shown creature lines (0x800CEBA8 and the others), which are made in the world frame and so always go to
    player 1's half, even when only player 2 is near (inferred).
  - `ed patterns` doesn't list these, because the catalogue places only calls in a room's region scripts.

## 3. Fix plan (the game's own functions, solo untouched)

Every step is gated on `g_settings.mod_coop && g_coop_p2 != 0`. Solo never reaches any of it.

- **A. Know whose subtitle it is.**
  - Pass-through intercepts on the three starts: 0x80008D48 (mode 1), 0x80008C6C (mode 0) and 0x80008D0C (mode 2).
  - Owner = 2 when player 2 is the player at the call: r13-21496 equals player 2's id (+16 of g_coop_p2), as inside
    coop_be_player2 (hle.c:12739), their turn, or their use. Otherwise owner = 1.
  - The owner goes back to none at 0x80008DD4 and at 0x80008E40.
  - A creature or object start (lr 0x800D1064, 0x800D10A8, 0x800D3F20, 0x800D3F38, 0x800D6700, 0x800D673C, 0x8006D10C,
    0x8006D128) could take the owner of the room it is in (coop_object_room). That's an option, not needed for the four.
- **B. Put the line in the owner's half, the way prompts already are.**
  - Intercept the make 0x801DB374 only when lr is 0x80008A64 or 0x80008C00 (the updater's two calls). Copy
    coop_game_prompt_made (hle.c:31561): guest-call it with a running flag, then move the returned element's +8. The
    layout's centring is relative, as hle_words_layout's x + 4096 shows (hle.c:21549-21554).
  - Owner 2: x + 4096 (COOP_BAR_AWAY). This shows in player 2's half both with and without their close-up, because their
    view brings back past-the-edge HUD in both cases (interp.cpp:848).
  - Owner 1 while player 2 has the screen or the lent cameras: x + 8192 (COOP_PROMPT_P1_AWAY, as coop_p1_icon_show,
    hle.c:23376-23382).
  - Split off (a whole-frame scene): the game's own x.
  - The screen can change while a line is up (a line lasts up to 175 frames), so place it again once a frame. Read the
    record's +20 in the controller step and keep its own x, as g_close_up_prompt_moved and own_x do.
  - Players together (follow, same room): open question 1 decides between "the owner's half only" and both halves. For
    both halves, make a second element with the same 0x801DB374 arguments at the other half's x, and free it with
    0x801DA4D8 whenever the game frees +20.
- **C. Two tracks, if overlap matters (case 6).** The CoopPlayerState pattern: a host copy of the 32-byte record for
  player 2.
  - A start or 0x80008DD4 with owner 2 runs with player 2's record swapped in and player 1's saved.
  - The world frame's updater call (intercept 0x800088A0 at lr 0x80008534) runs as the game's for player 1. Then the host
    guest-calls 0x800088A0(2) again with player 2's record in, and swaps it back.
  - The two lines are two elements of the shared pool. Five of ten are free in normal play (inferred from
    docs/coop-level3-design.md:91's list).
  - This is the only step that touches the updater. It also makes the phone's clean-up kill only its own player's record.
- **D. No double count behind player 1's Tome.**
  - While the Tome shares with the world (`g_tw.sharing && !g_tome_copy.stands_in`, hle_coop_player1_tome_up
    hle.c:16418), pass over the Tome frame's call (0x800088A0 with lr 0x801115DC), unless the record was started in a
    Tome frame (the replay page, lr 0x80108048). That start is marked in A.
  - In solo the world doesn't run behind the Tome, so the Tome's call stays as the game has it.
- **E. Player 1's subtitle out of player 2's copy Tome.** In tome_copy_no_window, when the record's owner is player 1,
  guest-call 0x80008DD4 in the copy. That is the game's own clean-up: it frees the copy's element and empties the copy's
  record, and the real one plays on in the world.

Write each attempt that fails into docs/coop-failed-attempts.md with its code. B alone fixes the phone. A and B are the
smallest useful fix; C, D and E are separate steps.

## 4. Testing it with scripted runs

- **A state.** Make a fresh one per step (the user, 2026-11-11). Level 00 isn't a Jump to Game chapter. Use a new game:
  `JTG_NEWGAME=1 JTG_SLOT=1 sh tools/ed/jtg.sh 1 sub-probe`, with the dream run's settings
  (logs/run-20261115-210507-jtg-dream-end-story/env.txt: JTG_BASE=1860, JTG_FRAMES=2500, JTG_SKIP_EVERY=150,
  JTG_SKIP_UNTIL=3200). Read "control at frame F, room R" from jtg-report.txt.
- **Reaching the rooms.** Use `ed door R` from wherever control lands to room 69, then
  ED_RUN_SCRIPT door scripts:
  - 69 -> 77 is L00/1000, which asks OPEN DOOR, so run it twice 60 frames apart.
  - 77 -> 78 is L00/1389, which asks nothing, so run it once.
  - Save with `ED_SAVE_STATE=FRAME:saves/sub-L00-69.state`, and likewise `sub-L00-78.state`, both players idle, with
    ED_ROOM_SLOTS=2 (follow).
- **Fallback.** saves/slot3.state, the user's level 00 save, apart in rooms 69 and 77, used by the clock runs on
  2026-11-15. It was made on an older build.
- **Common env:** ED_RUN_AWAY=1 ED_ROOM_SLOTS=2 ED_COOP_LOG=1 ED_RUN_SHOTS_MS=100
  ED_TRACE_FN=0x80008D48,0x80008D7C,0x80008DD4,0x801DB374 ED_TRACE_FROM=<press-10>. The `[fn]` lines give each start, free
  and make with its lr; the `[trace] words:` lines give the text.
- **Subtitles on.** These calls show nothing with the option off. Make sure the run's settings or card have Subtitles ON
  (0x80307180 bit 0); there is no env for it today.
- **Reading a run.** Read pictures for which half shows the words: `ed sheet RUN` frames about press+60 to press+280.
  `ed timeline RUN` first, then EVENTS.log.

The runs, in the style of docs/research/kit-bugs/build-run-duck.sh (each refuses an older run's logs):

1. **phone-p2-follow** (state sub-L00-69).
   - `ED_SET_FLAG="20:885,22:116"` (887 is off in a fresh game).
   - `ED_PUT="P,2,-1033,-116,0"` (Region10's box is -1147..-919 by -314..81), then `ED_PAD2_SCRIPT="P+2,P+8,lup;P+60,P+66,b"`.
     Their B is ANSWER_PHONE.
   - Expect "[coop] script 1801 runs for player 2", a `[fn] 0x80008D48 lr=0x800091F8` about P+92, "[trace] words: Hello?"
     about 26 frames later and "Remember me, Alex?" about 49 after that, and `[fn] 0x80008DD4 lr=0x80009104` about P+350.
   - Before the fix: the words in player 1's half (the prediction). After B: in player 2's.
2. **phone-p2-apart.**
   - The same, with ED_COOP_STAY=1 and player 1 sent out first: `ED_RUN_SCRIPT="100:1000,160:1000"`, player 1 to room 77.
   - Expect "[split] player 1's door ... player 2 stays" before P.
   - Before the fix, player 1 in room 77 shows the phone's lines. After it, only player 2's half does.
3. **phone-p1** (the negative check). Player 1 into Region10 (`ED_PUT="P,1,-1033,-116,0"`, a step, then
   `ED_INPUT_SCRIPT` B 0x0200 at P+60). Expect "[split] off ... (a cutscene)" and the lines on the whole frame, the same
   before and after.
4. **phone-p2-tome** (case 7). Run 1 again, with player 1's Start (0x1000) at P+100, between the two lines.
   - Count `[fn] 0x800088A0` calls by lr: 0x80008534 against 0x801115DC, with ED_TRACE_FN=0x800088A0 and a narrow
     ED_TRACE_FROM.
   - The gap between the two `0x801DB374 lr=0x80008A64|0x80008C00` makes should be 49 frames. About 25 means the double
     count. After D: 49.
5. **phone-p2-copy** (case 8). Run 3 with player 2's Start at the first line: player 1's subtitle should not draw on
   player 2's Tome page after E.
6. **clock-p2-third** (state sub-L00-78).
   - `ED_PUT="P,2,372,-1660,0"` (Region04 is 299..446 by -1747..-1580), a step, then b on EXAMINE and on TEXT, a on the
     words' Yes, and b to leave the dials, three times.
   - The gaps can't be known without a run. Read them from the user's play, logs/run-20261115-220457-clock-p2-icons: their
     B at 6094, the words at 6164, the dials at 6270, the dials' end at 6567.
   - Expect on the third pass "[trace] words: Three thirty three..." and the `[fn] 0x80008D48 lr=0x800092EC`, with the
     line in player 2's half before and after.
   - Check the log for player 2's close-up still on through the 220-frame wait.
   - If it can't be scripted, this is the user's play, as in docs/research/kit-bugs/plans.md:242.
7. **overlap** (case 6, after C). Apart: player 2 on the clock's third try in room 78 while player 1 answers the phone in
   room 69 with the flags set. Each half should keep its own lines, and the phone's clean-up shouldn't end the clock's.
8. **Every run:** no new saves/reports/hang-*.txt, and `ed test` after any host change.

Not reachable by script: hearing whose voice plays, and the user's own sense of where the words belong.
ED_INPUT_SCRIPT goes round controls.cpp, so player 1's real button handling needs the user's play.

## 5. Open questions

1. With both players in one room (follow), should a subtitle show in both halves (both hear the voice) or only in the
   half of the player who started it? That is the user's call. Today's words and finds go to the owner's half only.
   **Answered by the user, 2026-11-17:** the speaker's half first (A + B); then both halves only while both players
   are in the same room (apart: the speaker's half only), if it can be done without breaking the other co-op fixes.
   Only with the game's Subtitles option on, as today.
2. Does player 2's clock close-up really hold through the third try's 220-frame wait (case 1)? Not seen.
3. Where exactly does a subtitle end up behind player 1's real Tome (case 7): which mode's frame lays it out, and which
   half shows that frame?
4. The "Subtitles" style's x and y were not read, so whether a subtitle and a words box overlap in one half is inferred.
5. Which creatures are 0x800CEBA8, 0x800D2390 and 0x800D4B38 (always-shown lines, by language), and which object takes
   event 91 in 0x8006CEAC?
6. Does a scene of player 1's (fn22(1,1)) turn the split off apart as it does together? camera.c doesn't check the stay,
   but no apart log was read for it.
7. The Subtitles option's default on the user's card. If it is off, none of the four shows anything today.
8. player_data.tsv:67 says the record is 24 bytes, but the code writes +28. It should say 32.

## Skeptic's check

2026-11-17, read only. I checked the claims against build/ed-db/disasm.txt, runtime/host/hle.c, runtime/host/camera.c,
.deps/GXRuntime/graphics/aurora/lib/gfx/interp.cpp, tools/ed/primitives.tsv and two of the logs the page cites. Nothing
was run.

### Confirmed

1. **The record is 32 bytes.** 0x80008D48 writes `stw r6,28(r8)` at 0x80008D6C, and 0x80008C6C writes -1 to +28 at
   0x80008C90. tools/ed/player_data.tsv:67 still says 24.
2. **The three starts and their modes.** 0x80008D48 sets +24=1 and +25=1. 0x80008D0C sets +24=2 and +25=1. 0x80008C6C
   sets +24=0, +25=0 and +26=0. None of them frees +20. 0x80008DC0 only sets +26=1.
3. **The bl sites match the page.** The six natives call it at 0x80009178, 0x800091F4, 0x80009270, 0x800092E8 and
   0x80009360. The rune handler 0x80080308 (its function start is at 0x80080308) calls it at 0x80080610, 0x8008065C,
   0x800806A4 and 0x800806EC. The creature functions 0x800CEBA8, 0x800D2390 and 0x800D4B38 are the right function
   starts. Event 91 calls it at 0x8006D108 and 0x8006D124, and the stop is at 0x8006D154. The Tome's start is at
   0x80108044. The mode-2 restart is at 0x800AB5A8. 0x80008E40 is called at 0x8002176C.
4. **The updater 0x800088A0.** It has exactly three callers: 0x80008530 (`li r3,2`, right after 0x801DA574 at
   0x80008528), 0x80020278 and 0x801115D8 (`li r3,3`, right after 0x801DA574 at 0x801115D0). View 3 reads its pack from
   r13-26268 and any other view reads r13-26264 (0x8000897C..0x8000898C). Mode 1 waits on 0x801A4DCC(+28) != 0
   (0x80008958) and mode 2 on 0x801A19C8() == +28 (0x8000893C). The end check against the last row's start plus frames
   is at 0x800089B0..0x800089D0. The line is made at 0x80008A60 and 0x80008BFC with 0x10008, 1 and 0x8023BDF4. The
   option word is 0x80307170+16 = 0x80307180, bit 0 (r31 is set up at 0x80008904..0x8000890C). The frame-counter
   catch-up runs only with that option on (0x80008C40).
5. **The four natives' rows** are as primitives.tsv:406-409 gives them. The phone ends itself when +8 passes 75+124 =
   199, after the wait for sound 479.
6. **The phone has no hero gate.** kHeroGates (hle.c:20630-20636) has `{0, 583, 0x1AB}` and `{0, 1438, ...}` for
   level 0, and no 1801. `ed catalogue --script L00/1801` shows its states: state 0 asks ANSWER_PHONE, the press runs
   state 1 (cutscene(1,1), hold(0), waits 32), and state 2 starts the subtitle 32 frames later.
7. **The screen is withheld in a script hold.** hle.c:20563 needs `!g_coop_script_hold` (or a find) before it sets
   g_coop_screen. If neither holds and the cameras weren't lent before, they are given back (hle.c:20566-20567).
8. **The +4096 placement shows in player 2's half in both cases.** split_build passes `away = (player == 1)` every
   time (interp.cpp:542, 552), so player 2's view brings back a past-the-edge HUD block whether or not they have the
   screen (interp.cpp:848-855). Player 1's view brings back only draws past 6000, and moves them by 8192
   (interp.cpp:764, 842-847; kHudFarFrom 6000 at interp.cpp:569; COOP_PROMPT_P1_AWAY 8192 at hle.c:23376).
9. **The line goes through the words layout hook.** 0x801DB374 calls the layout 0x801DB088 at 0x801DB538, so
   hle_words_layout sees it with lr 0x801DB53C (hle.c:21527). That call runs tw_element_laid as well (hle.c:21550), so
   case 7's ownership claim holds as code.
10. **The Tome doubles the count.** TW_WORLD_FRAMES is 1 (hle.c:13661), so one world frame and one Tome frame run per
    shown frame. Both call the updater. Case 7's double speed follows; it is still not seen in a run.
11. **Plan A's lr list for the mode-1 natives and for event 91** (0x800091F8, 0x800092EC, 0x8006D10C, 0x8006D128), and
    plan D's 0x80008534, 0x801115DC and 0x80108048, are the right return addresses.

### Wrong, with corrections

1. **"kHudAway 568".** 568 is the line number in interp.cpp. The value is 4096 (`constexpr float kHudAway = 4096.f`),
   the same as COOP_BAR_AWAY (hle.c:25322). The plan's +4096 is still right.
2. **What sets g_coop_script_hold.** It is not fn22(1,1). It is the script's hold call, hold(0), made while player 2
   is the player (hle.c:13621-13622, the SCRIPT_HOLD_FROM..TO range). fn22 sets g_coop_p2_scene (hle.c:19584-19601).
   The phone does both, one after the other in state 1, so the prediction is unchanged. The reasoning should name
   hold(0).
3. **"The split goes off for a scene of player 1's" (cases 2 and 4, and the ghost maid) is not shown.**
   - The split goes off only for `cutscene_playing` (camera.c:1633-1635), which needs the game's scene flag and the
     letterbox bars above 0 (hle.c:3895-3900). It has no player test.
   - fn22 itself draws no bars: per `ed fn fn22`, "Nothing in it touches the HUD directly".
   - The dream run's own log shows the gap. Scene 2027 began as player 1's at frame 3216 (page-0001.log:1543). The
     cutscene started at 3237 (log:1560) and the split went off at 3238 (log:1561), with script 2266 running
     (log:1556), not 2027.
   - Player 2's fn22 scene 2142 (clock-p2-icons log:2085) did not turn the split off. Because the test has no player
     check, the same fn22 from player 1 most likely won't turn it off either.
   - **Correction:** for a start of player 1's, expect the split to stay on, with the line in player 1's half only.
     That is the right owner, but in follow player 2 sees nothing beside them. This holds unless the scene also brings
     up letterbox bars. Whether the phone's, the clock's or the maid's do is not known (inferred, needs a run).
   - Cases 2 and 4 then move from "both see it" to "only player 1 sees it", the same shape as open question 1. Case
     2's apart note (player 2 gets player 1's scene on the whole screen) falls away unless there are bars.
4. **Plan A's lr list for the creature starts is incomplete.** Each creature function makes three 0x80008C6C calls,
   not two. The language branch that isn't followed directly by a 0x80008DC0 call branches to one: 0x800D1090 goes
   `b 0x800D10A8`. So the lrs are 0x800D1064, 0x800D1094 and 0x800D10A8 in 0x800CEBA8. By the same layout they are
   0x800D3F0C, 0x800D3F20 and 0x800D3F38, and 0x800D6700, 0x800D6728 and 0x800D673C (bl sites 0x800D3F08, 0x800D3F1C,
   0x800D3F34, 0x800D66FC, 0x800D6724 and 0x800D6738). The text group passed is 0 at every site, and r13-26284 picks
   only the table and the row count. This doesn't touch the four calls. It matters only if A's "creature owner" option
   is taken.
5. **"0x80008E40 marks the record empty."** It writes only +0 = -1 and +8 = 0 (0x80008E40..0x80008E54). It does not
   free +20 or clear +4, +16 or +24. Harmless for the plan, but a host owner reset there shouldn't assume +20 is 0.

### Missed, and what it changes in the plan

1. **The Tome's replay page also stops the shared record.** 0x80008DD4 is called at 0x8010751C (in 0x801074FC) and
   at 0x80107B04 (in 0x80107A8C), each right after 0x8022C200. Reading both as the replay's clip being stopped is
   inferred. The plan only marks the replay's start, 0x80108044.
   - In co-op the world runs behind player 1's real Tome. If player 1 plays or stops a replay there while player 2's
     phone line is up, that line is overwritten (the start) or ended (these two stops).
   - Plan C needs the Tome's start and its two stops to count as player 1's track, or as a third track (the Tome's).
   - Plan D's "started in a Tome frame" mark should be cleared at these two stops as well as at 0x80008DD4.
2. **The phone's subtitle starts in a chained re-run, not in the press.** State 2 runs 32 frames after the press (the
   chain's "waits 32 frames").
   - Plan A's owner test, r13-21496 equal to player 2's id, holds only if the host's timer re-run brackets player 2.
     It appears to do so: "script N runs for player 2" with lr 0x7FFF0070 and who 137 (clock-p2-icons log:2109, 2126,
     2169), and coop_states_swap (hle.c:32521).
   - The same goes for the phone's EDCleanUpSubtitles in state 5. Run 1's `[fn] 0x80008D48` line should show this.
     Read r13-21496 at that call, not g_coop_use.active, which is over by then.
3. **A simpler hook point for plan B.** The layout hook (hle_words_layout) already sees every subtitle line, with lr
   0x801DB53C, and already moves player 2's own words by +4096 there (hle.c:21553-21557).
   - A flag set around the updater's two make calls can be read in that hook. One way: intercept 0x800088A0 and set
     "in the updater". Another: test the stack's saved lr, 0x80008A64 or 0x80008C00.
   - Either way the x moves at layout time, as coop_p2_words does, so no second guest call into 0x801DB374 is needed.
   - The per-frame re-place through +20 is still needed, because the screen can change while a line is up.
4. **Turning on Subtitles for a scripted run** needs a write to bit 0 of 0x80307180. There is no env for it; an
   ED_POKE-style setting, or a card with it on, is a prerequisite for every run in section 4. Otherwise "before the fix"
   shows nothing in either half, and that looks like a pass.

### Verdict on the plan

- The phone by player 2 is a real gap by the code: no hero gate, the screen withheld in a script hold, the line at the
  game's x. It shows as player 1's half carrying the lines, and player 2's half carrying none. Not seen in a run.
- Steps A and B are sound and use the game's own data (+20, the layout).
- Steps C and D need the Tome's replay start and stops added.
- The "player 1's scene shows the whole frame" premise is unproven and probably wrong. That widens open question 1
  from follow-with-player-2's-start to every start: in follow, does the other player see the line?
