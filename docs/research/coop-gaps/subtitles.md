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

## Built: step U1, plan A and B (2026-11-17, checkpoint commit, not yet played by the user)

What it does: a subtitle line shows in the half of the player who started it. Solo is untouched: every step is
gated on `g_settings.mod_coop && g_coop_p2 != 0`, and player 2's copy Tome (`g_tome_copy_turn`) is passed over.

- **A, the owner** (`coop_subtitle_started`, hle.c): notify hooks on the three starts 0x80008D48, 0x80008C6C and
  0x80008D0C. Owner 2 when r13-21496 is player 2's id at the call, else 1. The Tome's replay start (lr 0x80108048)
  is player 1's. The owner goes back to none at 0x80008DD4 and 0x80008E40 (`coop_subtitle_ended`).
- **B, the place** (`coop_subtitle_laid`, in `hle_words_layout`): only the world updater's own makes, found by the
  stack: 0x801DB374's saved lr is 0x80008A64 or 0x80008C00, and the updater's is 0x80008534.
  - Owner 2: x + 4096 - 80.
  - Owner 1 while player 2 has the screen or the lent cameras: x + 8192 - 80.
  - Owner 1 otherwise, or the split off: the game's x.
  - The 80 is the renderer's widescreen HUD shift (`coop_subtitle_wide`: 80 with the game's 16:9 on, 205 at 21:9,
    0 at 4:3). See docs/coop-failed-attempts.md.
- **Each frame** (`coop_subtitle_place`, beside `coop_p1_icon_show`): the line is placed again when the screen or
  the split changes. Only while the record's +20 is still that element at the x last written, so a pool element made
  again as something else is let go.
- **The name in the line**: an owner-2 line now names player 2's character ("Remember me, Ellia?", not "Pius").
  hle_words_layout's rename takes it as theirs.
- **ED_SUBTITLES=1** (test only, `test_subtitles`): holds bit 0 of 0x80307180 on. The user's slot3 save already has
  Subtitles on.

Runs, all from saves/slot3.state (the fallback; the new-game states were not made), with ED_RUN_AWAY=1:

| Run | What it showed in pictures |
|---|---|
| run-20261117-103854-sub-before-p2-follow | Before: player 2's "Hello?" and "Remember me, Pius?" in player 1's half, none in player 2's |
| run-20261117-105400-sub-after2-p2-follow | After: both lines in player 2's half, centred; none in player 1's |
| run-20261117-110024-sub-after2-p2-apart | Player 1 in room 77, player 2 at the phone in 69: lines only in player 2's half |
| run-20261117-104355-sub-after-p1 | Player 1's phone: lines in player 1's half, as before (owner 1, game x; no "[split] off") |
| run-20261117-105448-sub-mix-p1phone-p2clock | Player 1's phone lines in their half, player 2's EXAMINE prompt and carriage clock words in theirs |
| run-20261117-105525-sub-mix-p2phone-p1clock | Player 2's phone lines in their half, player 1's EXAMINE prompt and words in theirs |

Pictures: `Screenshots for Claude/U1-subtitles/`.

Seen in these runs:
- The skeptic's correction holds: player 1's phone scene did not turn the split off.
- Each player's press is held while the other's scene is up ("kept: ... it goes in as that's over"). So in these
  runs a subtitle never overlapped the other player's close-up, and the +8192 path (owner 1 while player 2 has the
  screen) was never reached. It is not seen in a picture.
- Already there before this change (in the before run too): after player 2's call ends, their ANSWER PHONE prompt
  shows again; and the phone's flash lights player 1's half, not player 2's.

Not done:
- **clock-p2-third**: not scripted. Room 77's clock spot (405,-1613, from the user's play) had no floor for a put in
  follow: player 2 fell through it with both ED_PUT and ED_COOP_P2_PUT (run-20261117-105924-sub-clock-p2-third3).
  The research's room 78 for Region04 is ed's numbering; the user's play reached it in the game's room 77.
- Both halves while both players are in one room (the user's second wish): a later step.
- C, D and E of the plan.

### Review fixes (2026-11-17)
Three review findings, two of them the same. Both real, both fixed in hle.c:
- **A loaded state kept the old owner.** `g_sub` was not cleared when a state loads. Player 2's call up, then a load
  of a state saved in one of player 1's lines: the loaded line would go to player 2's half, with player 2's name.
  Now `hle_coop_state_load` clears `g_sub`, so a line the loaded state has up is player 1's, as the comment said.
- **A new start moved the old line.** A start never frees the line on screen (the record's +20 keeps it until the new
  record's first row). The per-frame re-place read the new start's owner, so it could move the old line into the
  other half. Now `g_sub.line_owner` is the owner the line was laid for. The re-place and the name use it.
Seen after the fix (pictures 10-12 in `Screenshots for Claude/U1-subtitles/`):
- run-20261117-111103-sub-review-p2-follow: player 2's call, "Hello?" and "Remember me, Ellia?" centred in their half.
- run-20261117-111154-sub-review-mix-p1phone-p2clock: player 1's "Remember me, Pius?" in their half, player 2's
  EXAMINE prompt and carriage clock words in theirs.
Not seen: a state load mid-line, and a start over the other player's line. Neither was run; both fixes are read from
the code.

## Built: step U2, both halves in one room (2026-11-17, checkpoint commit, not yet played by the user)

The user's call: the line in both halves while both players are in the same room; apart, only in the owner's half
(U1). Only if it doesn't break the other co-op fixes. Solo is untouched: the same gate as U1.

How (hle.c, `coop_subtitle_copy`, called each frame after `coop_subtitle_place`):
- **The copy is the game's own make.** The line's make arguments are read at its layout: 0x801DB374 keeps them in
  r24..r27 and r29 until it calls the layout (r28 is the text by then; the updater passes 1). The copy is a second
  guest call of 0x801DB374 with them. Its layout adds the other half's away (`coop_subtitle_away` of the other owner),
  and it takes the same name as the line (an owner-2 line names player 2's character in both halves).
- **Made only when:** the record's +20 is still the line, the split is on, the players are not in two rooms
  (`coop_rooms_apart`, by their own nodes), no Tome is up (`g_tw.sharing`, `g_tome_copy_open`, `g_coop_tome_p2`), and
  at least two of the ten elements are free (one stays free after it).
- **Freed with the game's free (0x801DA4D8):**
  - at 0x80008D7C (new notify: the line's own free, the next row or its frames out);
  - at 0x80008DD4 and 0x80008E40 (`coop_subtitle_ended`);
  - each frame when the line is gone, the split goes off, the players are in two rooms, a Tome comes up, or the pool
    has no free element.
  - A state load forgets it without a free: `g_sub` is cleared there. (Safe only because a save now waits while a
    copy is up: see "Review fixes for U2" below. A state with a copy in it would keep the copy for ever.)
  - An element the game cleared (0x801DA074 at a mode's start) is only forgotten. One laid out again as something else
    is forgotten at its layout, never freed by the host.
- **Placed again** when the screen changes, as the line is.
- The log (`ED_COOP_LOG`) gives the free count of the ten at each lay, copy made, copy freed, line over and line gone.

Runs, all from saves/slot3.state (the fallback), ED_RUN_AWAY=1, ED_SUBTITLES=1. Pictures in
`Screenshots for Claude/U2-subtitles-both-halves/`:

| Run | Seen in pictures | Pool (free of 10) |
|---|---|---|
| run-20261117-111931-u2-p2-follow | Player 2's "Hello?" and "Remember me, Ellia?" centred in both halves; both gone at the end (01) | 9 line, 8 with copy, 10 after |
| run-20261117-112010-u2-p2-apart | Player 1 in room 77: the lines only in player 2's half (02) | 9, 10 after |
| run-20261117-112102-u2-p1-follow | Player 1's lines, "Remember me, Pius?", in both halves (03) | 9, 8, 10 after |
| run-20261117-112136-u2-p1-leaves | Player 1 takes door 1000 during "Remember me": the copy in their half until the door, then none in room 77; player 2's stays (04) | 8 with copy and a prompt, 9 after (player 2's prompt) |
| run-20261117-112237-u2-mix-p1phone-p2clock | Player 1's lines in both halves, player 2's EXAMINE prompt in its place beside the copy; then the clock's words in player 2's half only (05) | 7 with copy and prompt, 9 after (the prompt) |
| run-20261117-112315-u2-mix-p2phone-p1clock | Player 2's lines in both halves; then player 1's clock words in their half only (06) | 8, 10 after |
| run-20261117-112425-u2-load-mid-line | A state load at 4700 with the copy up: no crash, no line left in either half after it | not logged after the load |

In the leave run, player 2's ANSWER PHONE prompt came back at player 1's door, during their call. The log has the
prompt made at frame 4734, in the same pad read as "subtitle copy freed: the players are in two rooms" (frame 4734),
just before it. Picture 04 agrees: no prompt at F4690 or F4720, the prompt at F4745. So the two come at the same
moment, the room change. That U2 did not cause it is only inferred: `coop_prompt_show` runs before
`coop_subtitle_copy` in `coop_pad_read`, and it does not read `g_sub`. It was not checked with the copy turned off.
U1 noted the same prompt coming back after the call. (The first report of U2 said "frame 4668, before any copy was
freed": that was wrong; 4668 is when the second line's copy was made.)

Not seen:
- Player 1's Start during player 2's call: the Tome did not open (the scene holds it), so the "a Tome is up" free
  never ran (run-20261117-112355-u2-p2-phone-p1-tome).
- The copy moved when the screen changes (player 2 taking the screen during player 1's line): never reached.
- A full pool (no copy made): never reached; the most in use was 3 of 10.
- Players coming together into one room partway through a line (the copy made then): not run.
- The clock, the ghost maid, the creature and event-91 lines: not run.

### Review fixes for U2 (2026-11-17, checkpoint commit)

Four review findings. Three were real code problems, one was a wrong frame in the report.
- **A save with the copy up (two findings, one cause).** Only the host frees the copy. A state saved while it was
  up kept it as a used element of the ten. After loading, `g_sub` is cleared, so nothing frees it: the line would
  stay drawn in the other half and the pool would have one element fewer until a mode start. Fixed by holding the
  save: `hle_save_held()` now also waits while `g_sub.copy != 0`, as for the other host-only windows. Every save
  (F1-F10, the quick key, the save-now file, ED_SAVE_STATE, the recording's start) goes through that wait in
  quickstate.c. No load-time sweep was added: with the hold, no state can hold a copy. (`characters_reload` also
  checks `hle_save_held()`, so a character change waits for the copy too: at most one line.)
- **Co-op turned off, or player 2 gone, with a line up.** Every free was behind the co-op gate, so the copy stayed.
  New `coop_subtitle_stop`, called in `coop_pad_read` while playing whenever `mod_coop` is off or `g_coop_p2` is 0:
  it frees the copy with the game's free, puts an owner-2 line back at the game's own x (it was out at +4016, where
  the full screen does not show it), and forgets the line. Not in player 2's copy Tome's turn. Solo never has a
  line or copy in `g_sub`, so it returns at once there.
- New test switch: `ED_TEST_COOP_OFF=FRAME` turns co-op off at that frame, as the mods menu's toggle does.
- **The ANSWER PHONE frame** is corrected above (4734, not 4668).

Runs (saves/slot3.state, ED_RUN_AWAY=1, ED_SUBTITLES=1, the p2-follow phone script). Pictures in
`Screenshots for Claude/U2b-subtitles-review-fixes/`:

| Run | Log | Seen in pictures |
|---|---|---|
| run-20261117-113619-u2b-save-hold | ED_SAVE_STATE at 4625, copy up: "the save ... waits", then saved just after the copy freed at 4645. Pool 9, 8, 10 at the end | Both lines in both halves, as before (01) |
| run-20261117-113646-u2b-load-held-save | Loaded that state. Next line laid with 9 free, copy 8, back to 9 when the line freed: no orphan | Both halves had the line; nothing left in either half at F4850 and F4890 (02) |
| run-20261117-113718-u2b-coop-off | ED_TEST_COOP_OFF=4630: "copy freed: co-op is off", "put back at the game's x", "[split] off" | Both halves at F4625; one centred "Hello?" on the full screen at F4640; "Remember me" centred at F4700; none at F4800 (03) |

The line after the load named player 1's character ("Pius"): a loaded state's line is player 1's (U1's rule), as
before. The pictures run up to about ten frames behind the log's frame numbers around a save (the save stalls the
shots), so both halves' "Hello?" still shows in the picture labelled F4656, after the 4645 free.

Not checked: player 2 gone mid-line (an area change); the leave and mix runs again after these fixes (the changed
code only runs with co-op off, player 2 gone, or a save waiting).

## Built: step U3, plans D and E (2026-11-17, not yet played by the user)

Both are in runtime/host/hle.c and gated on co-op with player 2 in. Solo never reaches them.

- **D, behind player 1's real Tome** (`coop_subtitle_tick`, an intercept on the updater 0x800088A0).
  - The world's frame (lr 0x80008534) and the Tome's frame (lr 0x801115DC) both ran it each shown frame. The count went
    up by two, and a line made in a Tome frame came from the Tome's own pack.
  - Now, while player 1's Tome shares with the world (`hle_coop_player1_tome_up`), the Tome frame's call is passed over.
    The world's call alone counts and makes the lines, as solo does with no Tome.
  - A record the Tome's replay page started (lr 0x80108048) keeps the Tome's call (`g_sub.tome_started`). The mark is
    cleared at every end (0x80008DD4, which the replay's stops at 0x8010751C and 0x80107B04 call) and at 0x80008E40.
  - A copy Tome's frame is never passed over: it is its own memory.
- **E, a copy Tome** (`coop_subtitle_out_of_copy`, called right after `tome_copy_no_window` at the copy's opening).
  - In the copy, the game's own end 0x80008DD4 frees the line and empties the record, and U2's copy of the line is
    freed with the game's free. The real record plays on in the world, in its owner's half.
  - This is done whoever owns the line, not only for player 1's (the plan said player 1's). The copy's frame runs only
    the Tome's updater, which makes the next line from the Tome's pack: the wrong words for any world line.
  - The real record's hooks are kept out while this runs (`g_sub.in_copy`), so the real line and copy stay tracked.
  - A copy made beside player 1's real Tome is taken from the memory as that Tome opened. Its U2 copy is the one that
    was up then (`g_sub_clean_copy`, kept when that memory is taken).

Runs, from saves/slot3.state (the fallback), ED_RUN_AWAY=1, ED_SUBTITLES=1. Pictures in
`Screenshots for Claude/U3-subtitles-tome/`:

| Run | Log | Seen in pictures |
|---|---|---|
| run-20261117-114238-u3-before-p1tome-first (old build) | Player 1's Start at 4520, then player 2 answers the phone behind the Tome. 485 calls at lr 0x80008534 and 485 at 0x801115DC. "Hello?" made at 4605 (13 after the start), the record over at 4692. Line 2 made in a Tome frame: "The HORROR!!", "text 0 is the Tome's" | "Hello?" in player 2's half for about 15 frames; then "The HORROR!!" on player 1's Tome page; no "Remember me" anywhere (01) |
| run-20261117-114634-u3-after-p1tome-first | "the Tome's frame passes over the updater" at 4593. Lines made at 4618 and 4667 (49 apart), over at 4792, as with no Tome | "Hello?" and "Remember me, Ellia?" in player 2's half for their whole length; nothing on player 1's Tome page (02) |
| run-20261117-114334-u3-before-p2copy (old build) | Player 1 answers, player 2's Start at 4625 | "Hello?" on player 2's Tome page, then "The HORROR!!" there (03) |
| run-20261117-114702-u3-after-p2copy | "subtitle left out of player 2's copy Tome: line 0x805F87D0, record ended, U2's copy freed (9 of 10 free in the copy)". The real record went on: line 2 at 4669, over at 4794. The real U2 copy freed as "a Tome is up" at 4627 | Nothing on player 2's Tome page; "Hello?" and "Remember me, Pius?" in player 1's half (04) |
| run-20261117-114749-u3-check-p2-follow | Same pool counts and frames as U2's run | Both lines in both halves, as in U2 (05) |

Seen besides: the phone's white flash with a stray triangle in player 1's half at about 4890 to 4960. It is the same in
the before and after runs, so U3 did not cause it (the sanity flash, already researched).

Not seen:
- A Tome closed mid-line, and U2's copy made again after it. Scripted presses did not close either Tome in these runs
  (Start, or B up to three times, 60 to 120 frames after opening), even with no phone call
  (run-20261117-115107-u3-p2copy-nophone-b). An older run did not close it either (run-20261116-010024-tome-tabs).
  An earlier JTG run did close player 2's copy with B, after other presses, 630 turns after it opened
  (run-20261117-031922-jtg-stopsp2). Why these did not was not looked into.
- Both Tomes up with a line: not reachable with the phone. Each player's Start is refused in their own scene, and the
  other's scene holds the game's Start. So the copy made from the Tome's opening memory was not run with a line.
- The Tome's replay page (its start and stops): not run.
- The world's call during a Tome-started replay: fixed in the review (next section), from the code only.
- Solo: checked by reading only (every path returns at once without co-op and player 2).

## Built: step U3 review fix, the replay's world call (2026-11-17, not yet played by the user)

- **The problem (read from the code, not run).** For a record the Tome's replay page started, D kept the Tome
  frame's call. But the world frame's call (lr 0x80008534) still ran behind player 1's real Tome. The updater reads
  the Tome's pack only for view 3 (0x8000897C..0x8000898C); the world's call is view 2, so it reads play's pack. The
  world frame runs first each shown frame, so it would reach each row's start first and make the replay's line
  from play's pack: wrong words, or a group out of range. With no clip to catch up to, +8 would count twice a frame.
- **The fix** (`coop_subtitle_tick`): the intercept on 0x800088A0 now also looks at the world's call. When the
  record is the Tome's replay (`g_sub.tome_started`) and player 1's real Tome shares with the world, the world's call
  is passed over. The Tome's view-3 call alone counts and makes the lines, from the Tome's pack. This is the mirror of
  D. The caller does not read r3 after 0x80008534 (the next word is a call). Logged once per line as "the world's
  frame passes over the updater (the Tome's own replay ...)".
- A replay line made in the Tome's frame is not the world's (`coop_subtitle_laying`), so it is not moved or copied.
  It stays where the game puts it, on player 1's Tome page.
- The copy Tome, solo and co-op off return before this, as before.

Runs on the new build (saves/slot3.state, ED_RUN_AWAY=1, ED_SUBTITLES=1, the same switches as the U3 runs).
Pictures 06 to 09 in `Screenshots for Claude/U3-subtitles-tome/`:

| Run | Log | Seen in pictures |
|---|---|---|
| run-20261117-115953-u3r-p1tome-first | Same as U3's after run: Tome's frame passes over at 4593, lines at 4618 and 4667, over at 4792. No world pass-over line (no replay) | "Hello?" then "Remember me, Ellia?" in player 2's half, nothing on player 1's Tome page (06, 07) |
| run-20261117-120046-u3r-check-p2-follow | Same frames and pool counts as U2's and U3's follow runs | "Remember me, Ellia?" in both halves (09) |
| run-20261117-120120-u3r-p2copy | Left out of player 2's copy as in U3's run; line 2 at 4670, over at 4795 (one frame later than U3's run) | Nothing on player 2's Tome page, "Remember me, Pius?" in player 1's half (08) |

Not seen: the replay page itself. The fix is from the code only. No run here opens the replay page.

## Built: step F2, player 2's copy centred (2026-11-17, checkpoint commit, not yet played by the user)

Review (2026-11-17): the reviewers found nothing. The build used for the runs is newer than the renderer edit, and
pictures 02 and 08 were looked at again: both copies centred, the flash in player 2's half only.

- **The cause.** With the line in both halves, player 2's copy sat 48 px left of their half's middle
  (logs/run-20261117-111931-u2-p2-follow, shot 46: left 239, right 671, the half's middle 720). `coop_subtitle_wide`
  was right: the renderer moves a draw past the edge out by 80 units and the host takes 80 off. But the renderer
  (`widescreen_hud_shift`, gxcore_draw.cpp) leaves alone any draw on a height band where something crossed the
  screen's middle this frame or the last. The line in player 1's half is centred on the same band, so the copy at
  +4016 was not moved, and the host's 80 put it 80 units (48 px in a half) left. Its own bands were player 1's.
- **The fix (renderer).** A draw wholly past the screen's right edge (x0 > 1) is always moved out by the spare width.
  Only a split view brings such a draw back (player 2's HUD at +4096, player 1's at +8192 in player 2's close-up), so
  this screen's bands are not its own. Before, it was moved anyway unless such a band blocked it, so nothing that was
  placed by eye with the shift in moves.
- **The flash** (`flash_away`, step F1) put half the shift back for the same reason. It now uses the subtitle's place
  as it is, so the disc is centred in its half. The grown rim stays.
- **4:3 and 21:9, from the code.** At 4:3 the game's widescreen is off (aspect 1), the HUD spans the whole screen, the
  renderer moves nothing and `coop_subtitle_wide` gives 0. At 21:9 the renderer's spare is 1 - 0.8 x 16/21 = 0.39 NDC,
  205 HUD units at the narrowed scale, which is what `coop_subtitle_wide` gives; it is now always applied too.

Runs (saves/slot3.state, ED_RUN_AWAY=1, ED_SUBTITLES=1, the U2 scripts). Pictures in
`Screenshots for Claude/F2-subtitle-leftovers/`. Text centres measured from the pictures (960 px wide, halves of 480):

| Run | Seen |
|---|---|
| run-20261117-123652-f2-p2-follow | Player 2's phone, one room: "Hello?" and "Remember me, Ellia?" at 239 and 719 in both halves (02). The flash fills player 2's half, "moved 4016" (08). ANSWER PHONE after it in the same place as before |
| run-20261117-123836-f2-p2-apart | Player 1 in room 77: the lines at 719 in player 2's half only, as before (03) |
| run-20261117-123928-f2-mix-p1phone-p2clock | Player 1's "Remember me, Pius?" at 239 and 719 (was 671 in U2's run, 04 and 05). Player 2's EXAMINE prompt and the clock's words in the same places as U2's run (05, 06) |
| run-20261117-124032-f2-p2-follow-219 | A scratch settings file at aspect 4: "laid at x 320 + 3891" (205 off), both copies at 239.5 and 719.5 (07); the flash, "moved 3891", fills player 2's half (09). The window was the user's 16:9 screen |

Not seen: a real 21:9 or 4:3 screen (4:3 is from the code), the +8192 path (player 1's line while player 2 has the
screen), solo (a scratch solo settings file reached only the title, picture 11; the change only touches draws wholly
off the screen, which solo does not make).

**U2b picture 02 (a state saved in player 2's scene) is older than the subtitle work.** Seen again with a state saved
at frame 4575, before the call's first line (4592), with no subtitle in the run (logs/run-20261117-124127-f2-p2scene-save
and -124145-f2-p2scene-load, picture 10): both halves show the close-up camera, player 2's camera "-", for the whole
run. The scene also stayed held (busy 0x22, no line) for 370 frames. From the code: the save does not wait in player 2's
own scene, and `hle_coop_state_load` (step E, 1bc19f3) clears `g_coop_screen`, `g_play_script_p2`, the lent cameras and
player 2's camera zones. Left as it is; written up in docs/coop-next-session.md.
