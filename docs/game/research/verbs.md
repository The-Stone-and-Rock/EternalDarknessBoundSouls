> Kept as the evidence behind `docs/game/systems/`. Reports were checked by skeptics before the kit was built. Where a report says the kit is wrong, the kit has since been corrected. `tools/ed/facts` holds the scripts it cites, and `ed build facts` remakes the tables.

# Every player action, and the game's functions behind it

Made by `python -I tools/ed/facts/verbs.py` from build/ed-db and main.dol, over 4404 script files. The hand-written parts are in `verbs_text.md`. Three skeptics checked an earlier version. What failed is under **Not established**, at the end of the text. Nothing here was seen running: no game was run for any of it.

**How each statement is marked:**

- **[proved]** checked against a save state's memory, or the disc's data and the code agree.
- **[read]** from the code alone (`ed dis ADDR`).
- **[inferred]** from how something is used. Not a fact about what it is.

**One command re-checks what can be checked:** `python -I tools/ed/facts/verbs_live.py` (memory and code literals, prints PASS or FAIL per check), `python -I tools/ed/facts/verbs_regions.py` (every region against every save).

Addresses are PAL. `kind` is a node's `+8`, `state` its `+20` (`0x801F62D0`, `0x801F62D8`). The player is kind 2, handled by `0x80058DD8` (registered at `0x80030C4C`). [read] Live: kind 2, handle 1, state 1 in all 10 saves; saves 9 and 10 have a second kind 2 (handles 70 and 24). [proved]

## How any prompt works

1. **Asked:** `0x80117404(button, id, script, zone script, priority, slot, flag)`. There are 31 `bl` sites, 2 of them its own move below, no other branch to it and no data pointer to it. Scripts reach it only through `ed301`, `ed219`, `ed220` and `ed221`. [proved: 1,009 `ed301` calls in the disc's bytecode, all with literals; `calls.json`; a scan of main.dol]
2. **Slots:** six at `0x80335A38`, 28 bytes each: `+0` priority, `+4` button, `+8` id, `+12` script, `+16` nonzero means skip, `+20` its slot number, `+24` flag. A seventh at `0x80335AE0` is the prompt on show. [proved: `0x801175BC`, `0x801175F4`; live values below]
   - **Slots 0, 1, 3, 5** are written if the priority isn't lower (`0x801175E8`), and cleared every frame (`0x801177F0`..`0x8011781C`, reached from `0x800084F8`), so what asks there asks again each frame. [read]
   - **Slots 2 and 4** are for zones. An ask there is refused unless its zone script is above 0 (`0x80117460`) and its priority is above the slot's `+0` (`0x80117478`). The store at `0x801175BC`..`0x801175CC` never writes `+0`, so it stays 0 and any later ask with a priority above 0 replaces what's there. [proved: `+0` is 0 in slot 2 of saves 2 and 5 and slot 4 of saves 7 and 8]
   - **Slot 2 moves to slot 4** when the player is inside a sphere or box (bit 0 of `+104`) whose enter or stay script is the zone script, or whose stay script is the running script, and that region's `+110` is 90 or more (`0x80117500`, `0x80117528`, `0x80117568`). Never for a plane. [proved: saves 2 and 5 in slot 2 with `+110` of 40 and 50; saves 7 and 8 in slot 4 with `+110` of 90]
   - **What clears slot 2 or 4:** a press (`0x80006FD8` -> `0x80117360(1)` -> `0x80116E70(slot, 0)` at `0x801173E4`); leaving a zone (step 5); opening an interface (`0x8007B604`, `0x8007B610`); `fn211` (`0x8016C66C`, in L00/1879, L04/1871, L04/2726, L04/2738, L07/1453, L11/1384, L12/2778); `0x80116EFC` (from `0x800A2900`); `0x801172B0`, which clears all six (from `0x80056D38`, `0x800585D0`, `0x80059B2C`, `0x800663CC`, `0x80117344`, `0x80152134`); and `0x8002B5A4`, `0x800DFC98`, `0x801CFDFC`, `0x801CFE28`, `0x801D6BC4`. [read]
3. **Shown:** `0x80117628` copies the first slot in use, 0 to 5, to the seventh. Slots 1, 3 and 5 only count while a control channel is live (`0x80199F50` or `0x80199F74` is 1). With `r13-24048` nonzero (bits set by `0x80117260`), or with `r13-24040` set and returning 0, only slot 0 counts. `r13-24044` nonzero shows none. Flag bit 0 needs `0x801EBBA8` of both `0x806037D8` and `0x80603860`; what those are isn't traced. [read]
4. **Pressed:** the `in play` bindings call `0x80006EBC(r3, r4)` (table `0x8023B9B8`, channel `r13-26224`): A gives r3 0, Z 1, B 2, and the stick r4 3. [proved: the table in memory is the table in main.dol, and the live channel's `+4` is `0x8023B9B8`]
   - **Not the stick:** it runs only if `r3` equals the shown prompt's button (`0x80006F9C`). So only prompts with button 0, 1 or 2 can be pressed, and every ask in the game uses 2 or 6. **Button 6 has no binding: those prompts are only shown.** No row anywhere gives r3 6 (5 data pointers to `0x80006EBC`: the four above and `0x802448E8`, B with r3 2). [proved]
   - **The stick:** only with both control channels held (`0x80006EFC`, `0x80006F08`), and only for ids 6, 7, 11, 13, 19, 26, 34, 58 (`0x80006F10`..`0x80006F64`). A script above 0 has its word set to 100 (`0x80163C6C(script, 100, -1)` at `0x80006F80`) and is run. Script -2 calls `0x80024B24(1)`. For a look of kind 2 or 3, that also sets the look's script to word 100 (`0x80024C40`) and runs it (`0x80024C58`). [read] The one stick-listed button-6 ask, id 7 at `0x8007F640`, has script 0, so it does nothing. [read]
   - **Then** (`0x80006FB8`): B calls `0x801C4F48(0x801F62B8())`, the prompt is cleared (`0x80117360(1)`), and the script decides. -2: slot 2 or 4 picks look 0 or 1 at `0x80307148`, then `0x80024D78`. Above 0 or below -2: `0x801642D4(script, 0, 0)`. 0 and -1: nothing. [read]
   - **Second reader of the shown prompt:** `0x8006D63C`, in kind 32's handler `0x8006CEAC` (registered at `0x80030DC8`). If `0x80068D48` returned 22, the shown button is 2, the id is 1, 3 or 4, and B is down (`0x8013DC24(512, 0, 0)`), it sends itself event 81. Beside it, `0x8006D6B4` sends event 82 when `0x80068D48` returned 34. [read] Every save has exactly one kind 32 node. [proved] Scripts reach it with `fn78(3, 32)` and send it event 82 (for example L00/776, L04/132). What it is isn't traced. [read]
   - `0x8000704C` isn't a function. It's the instruction after `bl 0x801642D4` inside `0x80006EBC`, and the host uses it as a return address (`PROMPT_USE_RAN`, `COOP_PROMPT_RUNS` in hle.c). **It is also a branch target** from `0x80006FF0`, `0x80007030`, `0x80007038`, but `lr` holds it only after `0x80007048`. [proved: `lr` compares only, in hle.c]
5. **Left:** when the player leaves a zone, `0x801170D4` (called at `0x801349C0`) clears the three scripts' words with `0x80163E84`. If slot 2's script is one of the three, it clears **both** slots 2 and 4 (`0x8011719C`, `0x801171A8`), and the prompt on show if that was 2 or 4. Otherwise it checks slot 4 alone. [read]

**The script's word** is a table at `0x805CAA40`: 10 entries of 8 bytes, `+0` script, `+2` word, `+4` timer, `+6` keep. `fn0` and `fn1` read and write it. [proved: `0x80163E9C`, and slot 2 has `(2129, 1)` beside prompt `{2, 6, 2129}`]

- `0x80163E84` does **not** clear an entry whose `+6` is nonzero (`0x80163ED0`). `fn35` sets it through `0x80163F88`. [read] 193 files call `fn35`.
- **`fn34()`** sets `r13-23220` to the running script (`0x80168E50`), and `0x80164470`..`0x80164484` runs it again as soon as its chunk ends. **`fn36(frames)`** sets the timer, and `0x80163E58` runs it when that reaches 0. So a script's word can advance with no press. [read]

`ed301` (`0x8000A424`) passes the running script `r13-23228` as both script and zone script, priority 50, slot 2, flag 0, and refuses by two tables. **Filter A** (`0x8023BF58`): while `0x8006DB04(0x4000)`. **Filter B** (`0x8023BE68`): while the actor's `+158, +159` is 1, 6. Neither bit is traced to a meaning. Its answer goes back to the script. In `ed301` the id isn't checked against the filter tables above 59. [read]

## Every prompt

Filters: A and B as above; S means the stick acts on it. Counts are `ed301` calls only. `ed prompts` counts the same, so its 266 for EXAMINE leaves out ed219: 402 calls in 287 files, 210 ids. ed220: 98 calls in 95 files, 76 ids. ed221: 124 calls in 120 files, 93 ids. ed221 fifth argument: 124 with a thing, 0 without. ed221 sixth: {'-1': 116, '0': 6, '1': 1, '2': 1}. ed221 fourth: {'4': 27, '3': 4, '1': 72, '0': 12, 'LIGHT_AFTER_EXAMINE_TWEEN': 5, '6': 4}. ed219 fourth: {'1': 230, '4': 44, '0': 126, 'L0': 2}.

The `marks` column says what each row's parts rest on. "ask proved" means the disc's scripts and `ed301`'s code agree, or the prompt was seen in memory (6 in saves 2, 5, 7, 8; 19 in save 4). "ask read" means a literal in the code. "Asked" only means code that asks exists; nobody checked that each site is reached in play.

| id | label | words | how it is asked | script calls / files / distinct ids | what pressing it runs | filters | marks |
|---|---|---|---|---|---|---|---|
| 0 | INVALID_BUTTON | INVALID BUTTON | nothing found | 0 / 0 / 0 | - |  | proved: nothing asks, by scan |
| 1 | OPEN_DOOR | OPEN DOOR | ed301(2, 1) 0x8000A424 | 272 / 265 / 229 (256 with fn66) | The script again, at the word it set: fn66 go_to_room 0x801666A4 (usually word 1; counts in the doors section) | B | ask proved (scripts and ed301 agree); press read |
| 2 | TELEPORT | TELEPORT | nothing found | 0 / 0 / 0 | - |  | proved: nothing asks, by scan |
| 3 | CLIMB_UP | CLIMB UP | ed301(2, 3) 0x8000A424 | 47 / 47 / 43 (45 with fn66) | The script again, at the word it set: fn66 go_to_room 0x801666A4 (usually word 1; counts in the doors section). 2 of the files (L00/776, L11/776) use ed68, ed83, ed55, ed89 instead | B | ask proved (scripts and ed301 agree); press read |
| 4 | CLIMB_DOWN | CLIMB DOWN | ed301(2, 4) 0x8000A424 | 39 / 39 / 39 (39 with fn66) | The script again, at the word it set: fn66 go_to_room 0x801666A4 (usually word 1; counts in the doors section) | B | ask proved (scripts and ed301 agree); press read |
| 5 | CLIMB_OVER | CLIMB OVER | nothing found | 0 / 0 / 0 | - |  | proved: nothing asks, by scan |
| 6 | EXAMINE | EXAMINE | ed301(2, 6) 0x8000A424<br>ed219 0x80017324<br>ed220 0x80017154<br>ed221 0x80017604 | 266 / 241 / 115 (25 with fn66) | From ed219/220/221 (script -2): 0x80006EBC -> 0x80024D78 steps the close-up. From ed301: the script again | A S | ask proved (live, saves 2, 5, 7, 8); press read |
| 7 | RETURN | RETURN | ed301(2, 7) 0x8000A424<br>0x80025014 in 0x80024D78 (the close-up stepper) button 2 script -2 prio 50 slot 2 or 4 flag 1<br>0x8002510C in 0x80024D78 (the close-up stepper) button 2 script -2 prio 50 slot 2 or 4 flag 1<br>0x8007F640 in 0x8007F610 (interface ed56(11) tick) button 6 script 0 prio 50 slot 0 flag 0 | 212 / 121 / 26 (3 with fn66) | Script -2: the close-up's last step, 0x80024D78 -> 0x801642D4(the look's script). From ed301: the script again. A stick press also ends it: 0x80024B24(1), which for a look of kind 2 or 3 sets its script to word 100 and runs it; from ed301, word 100 and a run (0x80006F80) | S | ask proved (scripts and ed301 agree); press read |
| 8 | OVERVIEW | OVERVIEW | nothing found | 0 / 0 / 0 | - |  | proved: nothing asks, by scan |
| 9 | OPEN_FIREPLACE | OPEN FIREPLACE | nothing found | 0 / 0 / 0 | - |  | proved: nothing asks, by scan |
| 10 | ENTER_SECRET_PASSAGE | ENTER SECRET PASSAGE | ed301(2, 10) 0x8000A424 | 2 / 2 / 2 (2 with fn66) | The script again, at the word it set: fn66 go_to_room 0x801666A4 (usually word 1; counts in the doors section) |  | ask proved (scripts and ed301 agree); press read |
| 11 | PULL_LEVER | PULL LEVER | ed301(2, 11) 0x8000A424 | 12 / 12 / 11 | 0x80006EBC -> 0x801642D4(script, 0, 0): the script again, at the word it left (fn0) | S | ask proved (scripts and ed301 agree); press read |
| 12 | TURN_WHEEL | TURN WHEEL | nothing found | 0 / 0 / 0 | - | A B | proved: nothing asks, by scan |
| 13 | PRESS_BUTTON | PRESS BUTTON | nothing found | 0 / 0 / 0 | - | A B S | proved: nothing asks, by scan |
| 14 | TAKE_ELEVATOR | TAKE ELEVATOR | nothing found | 0 / 0 / 0 | - | A B | proved: nothing asks, by scan |
| 15 | DESCRIPTION | DESCRIPTION | nothing found | 0 / 0 / 0 | - |  | proved: nothing asks, by scan |
| 16 | OPEN_FUSEBOX | OPEN FUSEBOX | ed301(2, 16) 0x8000A424 | 1 / 1 / 1 (1 with fn66) | 0x80006EBC -> 0x801642D4(script, 0, 0): the script again, at the word it left (fn0) | A B | ask proved (scripts and ed301 agree); press read |
| 17 | TRIP_BREAKER | TRIP BREAKER | ed301(2, 17) 0x8000A424 | 1 / 1 / 1 (1 with fn66) | 0x80006EBC -> 0x801642D4(script, 0, 0): the script again, at the word it left (fn0) | A B | ask proved (scripts and ed301 agree); press read |
| 18 | OPEN_DRAWER | OPEN DRAWER | ed301(2, 18) 0x8000A424 | 1 / 1 / 1 | 0x80006EBC -> 0x801642D4(script, 0, 0): the script again, at the word it left (fn0) | A B | ask proved (scripts and ed301 agree); press read |
| 19 | PICK_UP | PICK UP | ed301(2, 19) 0x8000A424<br>0x80025400 in 0x80024D78 (the close-up stepper) button 2 script -2 prio 50 slot 2 or 4 flag 1<br>0x800C0348 in 0x800C030C (a thing's own, kind 2 state 1 event 3) button 2 script 2246 prio 49 slot 1 flag 0 | 28 / 26 / 9 (15 with fn66) | Three askers. Thing in the world, script 2246: fn79(player, 71, 0), then 0x800C362C. From a close-up (script -2, kind 3): 0x80025408 -> 0x801F9DE8(thing, player, 30) and 0x80024528. From ed301: the script again. The find also has ways in with no prompt 19 (ed52) | S | ask proved (live, save 4); press read |
| 20 | BREAK_FREE | BREAK FREE | 0x80053ED8 in 0x80053E4C (kind 2, state 56) button 6 script 0 prio 50 slot 1 flag 0<br>0x800573A0 in 0x8005713C (kind 2, state 67) button 6 script 0 prio 50 slot 1 flag 0<br>0x800578EC in 0x80057790 (kind 2, state 69) button 6 script 0 prio 50 slot 1 flag 0<br>0x80061A64 in 0x80060EC8 (kind 19 behaviour, its tick (event 3)) button 6 script 0 prio 50 slot 1 flag 0 | 0 / 0 / 0 | Nothing: button 6 has no binding. It is the stick: event 44, 0x80060DC4 adds to actor +140 -> +357 when a stick sector changes, at 10 event 126 lets go. Read for state 56 |  | ask read; press read |
| 21 | RELOAD | RELOAD | 0x800C0388 in 0x800C030C (a thing's own, kind 2 state 1 event 3) button 2 script from the weapon prio 50 slot 3 flag 0 | 0 / 0 / 0 | The weapon's script, 0x80150874, or 1582 if that's 0. Script 1582: ed82, ed20, ed140, ed9, ed130, ed80. Z runs the weapon's script with no prompt and no 1582 (0x800C2CA4) |  | ask read; press read |
| 22 | TALK | TALK | ed301(2, 22) 0x8000A424<br>0x800597A8 in 0x80058DD8 (kind 2 event 147) button 2 script 1902 prio 48 slot 3 flag 0 | 1 / 1 / 1 | Script 1902: fn79(player, 147, 0, 1). Event 147 with data: 0x800BF720 gives who, and if they answer 1, state 71 (0x800527A8). From ed301 in L00/583: the script again |  | ask proved (scripts and ed301 agree); press read |
| 23 | FINISH_HIM | FINISH HIM | 0x800C03F8 in 0x800C030C (a thing's own, kind 2 state 1 event 3) button 2 script 2244 prio 49 slot 3 flag 2 | 0 / 0 / 0 | Script 2244: fn79(player, 202, 0). Event 202: 0x800C30CC, one path sets state 30 |  | ask read; press read |
| 24 | PUSH_OFF | PUSH OFF | 0x80056BFC in 0x800566AC (kind 2, state 52) button 6 script 0 prio 50 slot 1 flag 0 | 0 / 0 / 0 | Nothing: button 6. In state 52 event 44 adds through 0x80060DC4; at 8, event 112, state 1 |  | ask read; press read |
| 25 | SELECT | MOVE EXIT SELECT | 0x800812A0 in 0x800810D0 (interface ed56(3) tick) button 2 script 0 prio 50 slot 0 flag 0 | 0 / 0 / 0 | The interface's own bindings (0x80244AA0: 0x80080D70, 0x80080C68) |  | ask read; press read |
| 26 | PLAY_ORGAN | PLAY ORGAN | nothing found | 0 / 0 / 0 | - | A B S | proved: nothing asks, by scan |
| 27 | USE_STAFF | USE STAFF | ed301(2, 27) 0x8000A424 | 1 / 1 / 1 | 0x80006EBC -> 0x801642D4(script, 0, 0): the script again, at the word it left (fn0) | A B | ask proved (scripts and ed301 agree); press read |
| 28 | OPEN_CASKET | OPEN CASKET | nothing found | 0 / 0 / 0 | - | A B | proved: nothing asks, by scan |
| 29 | FLIP_HOURGLASS | FLIP HOURGLASS | nothing found | 0 / 0 / 0 | - | A B | proved: nothing asks, by scan |
| 30 | READ_TOME | READ TOME | nothing found | 0 / 0 / 0 | - | A B | proved: nothing asks, by scan |
| 31 | OPEN_GATE | OPEN GATE | nothing found | 0 / 0 / 0 | - | A B | proved: nothing asks, by scan |
| 32 | MOVE_BOOK | MOVE BOOK | nothing found | 0 / 0 / 0 | - | A B | proved: nothing asks, by scan |
| 33 | LOOK_AROUND | LOOK AROUND | ed301(2, 33) 0x8000A424 | 1 / 1 / 1 | 0x80006EBC -> 0x801642D4(script, 0, 0): the script again, at the word it left (fn0) |  | ask proved (scripts and ed301 agree); press read |
| 34 | TEXT | DETAIL | ed301(2, 34) 0x8000A424<br>0x80025048 in 0x80024D78 (the close-up stepper) button 2 script -2 prio 50 slot 2 or 4 flag 1<br>0x8002533C in 0x80024D78 (the close-up stepper) button 2 script -2 prio 50 slot 2 or 4 flag 1 | 103 / 82 / 59 (4 with fn66) | Script -2: next step of the close-up, its words (0x80024310(element, -2, 0)). From ed301: the script again | S | ask proved (scripts and ed301 agree); press read |
| 35 | ADJUST_MIRROR | ADJUST MIRROR | ed301(2, 35) 0x8000A424 | 1 / 1 / 1 (1 with fn66) | 0x80006EBC -> 0x801642D4(script, 0, 0): the script again, at the word it left (fn0) | A B | ask proved (scripts and ed301 agree); press read |
| 36 | ANSWER_PHONE | ANSWER PHONE | ed301(2, 36) 0x8000A424 | 1 / 1 / 1 | 0x80006EBC -> 0x801642D4(script, 0, 0): the script again, at the word it left (fn0) | A B | ask proved (scripts and ed301 agree); press read |
| 37 | ADJUST_CLOCK | ADJUST CLOCK | nothing found | 0 / 0 / 0 | - | A B | proved: nothing asks, by scan |
| 38 | SEARCH | SEARCH | ed301(2, 38) 0x8000A424 | 1 / 1 / 1 | 0x80006EBC -> 0x801642D4(script, 0, 0): the script again, at the word it left (fn0) |  | ask proved (scripts and ed301 agree); press read |
| 39 | ENGAGE_ARRAY | ENGAGE ARRAY | ed301(2, 39) 0x8000A424 | 18 / 18 / 18 | 0x80006EBC -> 0x801642D4(script, 0, 0): the script again, at the word it left (fn0) | A B | ask proved (scripts and ed301 agree); press read |
| 40 | HIDE | HIDE | nothing found | 0 / 0 / 0 | - |  | proved: nothing asks, by scan |
| 41 | TURN_SPIGOT | TURN SPIGOT | nothing found | 0 / 0 / 0 | - | A B | proved: nothing asks, by scan |
| 42 | PUSH | PUSH | nothing found | 0 / 0 / 0 | - |  | proved: nothing asks, by scan |
| 43 | ATTEMPT_AUTOPSY | ATTEMPT AUTOPSY | 0x800C03BC in 0x800C030C (a thing's own, kind 2 state 1 event 3) button 2 script 2250 prio 49 slot 5 flag 0 | 0 / 0 / 0 | Script 2250: fn79(player, 194, 0). Event 194: 0x800C38B8, state 84. Only in level 5 |  | ask read; press read |
| 44 | MEDITATE | MEDITATE | nothing found | 0 / 0 / 0 | - |  | proved: nothing asks, by scan |
| 45 | RATIONALISE | RATIONALISE | nothing found | 0 / 0 / 0 | - |  | proved: nothing asks, by scan |
| 46 | WIND_CLOCK | WIND CLOCK | nothing found | 0 / 0 / 0 | - | A B | proved: nothing asks, by scan |
| 47 | SCRIBE_RUNE | MOVE SCRIBE RUNE CANCEL | 0x8007E4F4 in 0x8007E41C (interface ed56(9,10) tick) button 2 script 0 prio 50 slot 0 flag 0 | 0 / 0 / 0 | The interface's own bindings | A B | ask read; press read |
| 48 | LOCKED | LOCKED | nothing found | 0 / 0 / 0 | - | A | proved: nothing asks, by scan |
| 49 | CAMERA_PAN | PAN EXIT | 0x801EF0C8 in 0x801EF074 (camera pan/tilt, from 0x800083C8) button 2 script 0 prio 0 slot 0 flag 0 | 0 / 0 / 0 | Nothing runs: script 0. Its stick binding is 0x801EF110; that it pans is from the name |  | ask read; press inferred |
| 50 | SAFE | Spin Open Cancel Reset | 0x8007E9EC in 0x8007E9BC (interface ed56(1) tick) button 6 script 0 prio 50 slot 0 flag 0 | 0 / 0 / 0 | Button 6: not pressed. ed56(1)'s own bindings (0x802449A0) |  | ask read; press read |
| 51 | SURVEY | SURVEY | nothing found | 0 / 0 / 0 | - |  | proved: nothing asks, by scan |
| 52 | RING_BELL | RING BELL | nothing found | 0 / 0 / 0 | - | A B | proved: nothing asks, by scan |
| 53 | MOVE_STAFF_RETURN | MOVE STAFF EXIT | 0x8007E998 in 0x8007E508 (ed68(0) at 0x8007A9AC, and 0x80083598; which n is not settled) button 6 script 0 prio 50 slot 0 flag 0 | 0 / 0 / 0 | Button 6: not pressed. Which interface it belongs to is not settled | A B | ask read; press read |
| 54 | CAMERA_TILT | TILT EXIT | 0x801EF0EC in 0x801EF074 (camera pan/tilt, from 0x800083C8) button 2 script 0 prio 0 slot 0 flag 0 | 0 / 0 / 0 | Nothing runs: script 0. Its stick binding is 0x801EF110; that it tilts is from the name |  | ask read; press inferred |
| 55 | MOVE_HANDS_RETURN | MOVE HANDS EXIT | 0x8007F2C8 in 0x8007F254 (interface ed56(8): called at 0x8007B504, rows 2 and 3 of its table) button 6 script 0 prio 50 slot 0 flag 0 | 0 / 0 / 0 | Button 6: not pressed. ed56(8)'s own bindings (table 0x80244880; rows 2 and 3 are 0x8007F254) | A B | ask read; press read |
| 56 | NEXT | NEXT | nothing found | 0 / 0 / 0 | - |  | proved: nothing asks, by scan |
| 57 | TEXT_A | MOVE EXIT | 0x800812A0 in 0x800810D0 (interface ed56(3) tick) button 2 script 0 prio 50 slot 0 flag 0 | 0 / 0 / 0 | The interface's own bindings (0x80244AA0) |  | ask read; press read |
| 58 | PLAY_PIANO | PLAY PIANO | nothing found | 0 / 0 / 0 | - | A B S | proved: nothing asks, by scan |
| 59 | PRESS_PANEL | PRESS PANEL | ed301(2, 59) 0x8000A424 | 1 / 1 / 1 | 0x80006EBC -> 0x801642D4(script, 0, 0): the script again, at the word it left (fn0) | A B | ask proved (scripts and ed301 agree); press read |
| 60 | BOSS_SCRIBE_RUNE_MOVE | MOVE SCRIBE RUNE | 0x8007E494 in 0x8007E41C (interface ed56(9,10) tick) button 2 script 0 prio 50 slot 0 flag 0 | 0 / 0 / 0 | The interface's own bindings |  | ask read; press read |
| 61 | BOSS_RUNE_MOVE | MOVE | nothing found | 0 / 0 / 0 | - |  | proved: nothing asks, by scan |
| 62 | SCRIBE_RUN_NO_MOVE | SCRIBE RUNE CANCEL | 0x8007E4D0 in 0x8007E41C (interface ed56(9,10) tick) button 2 script 0 prio 50 slot 0 flag 0 | 0 / 0 / 0 | The interface's own bindings |  | ask read; press read |

**Asked by nothing found (0 INVALID_BUTTON, 2 TELEPORT, 5 CLIMB_OVER, 8 OVERVIEW, 9 OPEN_FIREPLACE, 12 TURN_WHEEL, 13 PRESS_BUTTON, 14 TAKE_ELEVATOR, 15 DESCRIPTION, 26 PLAY_ORGAN, 28 OPEN_CASKET, 29 FLIP_HOURGLASS, 30 READ_TOME, 31 OPEN_GATE, 32 MOVE_BOOK, 37 ADJUST_CLOCK, 40 HIDE, 41 TURN_SPIGOT, 42 PUSH, 44 MEDITATE, 45 RATIONALISE, 46 WIND_CLOCK, 48 LOCKED, 51 SURVEY, 52 RING_BELL, 56 NEXT, 58 PLAY_PIANO, 61 BOSS_RUNE_MOVE).** [proved: a static scan] `ed301`'s id is written as a number in all its calls (0 files otherwise), and every native ask is listed below. The only computed native id is `0x800812A0` (r31 is 57 or 25). The bytecode gives the same 20 `ed301` ids as the decompiled text. Words for them exist on the disc, but no code or script asks. READ_TOME, PUSH, LOCKED, NEXT and TELEPORT are among them. This covers the disc's scripts and static code; nothing was run.

## Regions: how a walk-in starts anything

A region is 116 bytes (`0x80155130(2)`: `+176` count, `+180` list). The room file has the same header, and `regions.json` is read from it: 297 rooms, 2832 regions: 1989 boxes, 440 spheres, 403 planes. 2280 enter scripts, 232 leave, 1 stay, 198 enter and 2 leave for other nodes, 403 planes with a script. [proved: all 10 saves' live lists equal the files field by field, `verbs_regions.py`]

Each frame the scan (`0x80134A44`, called only from `0x801335C4` in `0x80133434`) tests a node against each region (`0x80134758`). `+108` is its shape: 0 sphere, 1 box, 2 plane. `0x80134684` keeps inside and outside in `+104` and returns 1 enter, 2 leave, 3 stay. [read]

| Event | Sphere (`+108` 0) | Box (`+108` 1) | When |
|---|---|---|---|
| Enter | `+58` | `+60` | once, on coming in |
| Leave | `+60` | `+62` | once, on going out |
| Stay | `+62` | `+64` | every scan while inside |

[proved: `0x80134910`..`0x8013492C`, `0x801349AC`..`0x80134A2C`; slot 1: ExitRgn01 sphere, enter 1097; MiddleDeskRgn box, enter 2129, leave 1300; Region05 sphere, enter 2449, leave 1300]

- **Planes are different.** A plane has no enter, leave or stay. It runs `+100` or `+102`, picked by which way it's crossed (`0x80134890`, `0x801348D8`), and sets bit 4 or 8 of `+104`. Only `r13-26524` crosses them (`0x80134824`). [proved: slot 1 HorrorPlane01 is -1, 379]
- **`r13-26524` is a model, not a node.** Live, it's the `+52` of kind 2 handle 1 in all 10 saves, and not the model of the second kind 2 in saves 9 and 10. [proved] It has 13 writers (`0x8000799C` .. `0x800C4D08`), not followed.
- **Any other model** uses the fields above them: `+64`, `+66`, `+68` for a sphere, `+66`, `+68`, `+70` for a box (`0x80134954`..`0x8013496C`; `0x80134A64`..`0x80134A78`). **So in the game's own code a second player's model reads these, not the enter script, and crosses no plane.** [read] The caller only scans a model at all if `0x80117C7C` is 2 or less and it isn't kind 29, or if it is `r13-26524` (`0x80133590`..`0x801335B0`). [read] The host has its own use of this scan (hle.c lines 7651, 7680), not checked here.
- **`+110`** is the 40, 50 or 90 that picks slot 2 or 4. [proved, above]
- The stay script is almost unused: one on the disc, room 294 Region01 (enter 1012, leave 2300, stay 2325). [proved]

## Pick up

Prompt 19 has three askers:

| Asker | Asks | B runs |
|---|---|---|
| **A thing in the world** | `0x800C030C`, each tick of kind 2 state 1 (`0x80058C44`): if `0x800DA688`, `(2, 19, 2246, 0, 49, 1, 0)` | Script 2246 (15 levels): `fn79(player, 71, 0)`. Event 71: `0x800C362C` (`0x8005A7E4`). [proved: slot 4 shows `{49, 2, 19, 2246, slot 1}`] |
| **A close-up** | `ed221` with its thing still there. Its kind 3 step 2 asks 19, script -2, at `0x80025400` | `0x80025408`: `0x801F9DE8(thing, player, 30)` (`0x8002545C`) and the find `0x80024528(element, -2, player, handle, 0, 0, 0)` (`0x800254F0`). [read] |
| **A region's script** | `ed301(2, 19)`: 28 calls, 26 files, 9 ids (575, 695, 1381, 1413, 2129, 2137, 2565, 2671, 2692) | The script again. [proved] What that run does differs: in L00/1413 it's `ed85` and `fn66`, not a find |

**The find itself has more ways in than prompt 19.** `0x80024528` is called from 5 sites: `0x800254F0`, `0x800C3804`, `0x80012BE8` (`ed52`), `0x800C3054` (thing 856 through `0x801F60BC`), `0x801C6FF4`. [read] `ed52` has 180 calls in 138 files; 112 of the files never ask 19. [proved: counts] L00/1413's own other branch is `ed301(2, 6)`, a yes/no, then `fn90(iTome, 30); ed52(iTome)`, with no PICK UP prompt. [read] Not every `ed52` call was read as a pickup.

`ed52(handle, script, n)` (`0x80012A58`) is the find from a script: it reads the thing's words (`0x80150B2C`, `0x80150B5C`), shows them (`0x801DB374`), and calls `0x80024528`. [read] `fn90(thing, 30)` stores 30 at node `+12` (`0x801F64A8`), and so does `0x801F9DE8` after asking the thing with event 240 (`0x801F9E4C`). That `+12` is the kind it's to become is [inferred], from `0x801F9E18` comparing the argument to `+8`.

`0x800C362C` (event 71 on the player) works on the thing whose handle is the player's `+36 -> +0`: `r25` is its node, `r24` its model (`0x800C36B0`). [read]

- If **that thing's** model has bit `0x4000` (`0x80118C4C`, `0x800C36EC`): on `0x8006A32C(.., 17)`, words 0,12 REPLACE HEAD (`0x800C3740`), `0x80024A14`, event 91; else on `(.., 9)`, its words and `0x80024A14`. Either way, event 71 to the thing (`0x800C37BC`).
- Otherwise, only a thing with `+56` of 2 does anything: its words, then `0x80024528` (`0x800C3804`).

`0x800DA688` walks the list at `r13-21456`, taking kind 30 things in the player's room, not in state 36, then tests distance. Event 72 puts a kind 30 in state 36 (`0x800D8818`), event 71 in state 35 (`0x800D87F0`). [read]

### What `ed219`, `ed220`, `ed221`'s arguments do

The record is at `0x803090C4`: `+0` kind, `+4` the script, `+8` step, `+12` group, `+16` entry, `+20` camera hash, `+24` flags, `+28` thing's handle. It's copied to look 0 or 1 at `0x80307108` when the ask lands in slot 2 or 4. [proved: `0x800170D8`, `0x800172C8`, `0x800174A0`; the looks below]

| Call | Kind | Arguments | Asks if |
|---|---|---|---|
| `ed220(group, entry)` | 1 | Words only, no camera | not `0x8006DB04(0x4000)` |
| `ed219(group, entry, camera, flags)` | 2 | | not `0x8006DB04(0x4000)` |
| `ed221(group, entry, camera, flags, thing id, entry once taken)` | 3, or 2 / 1 once taken | | not `0x8006DB04(0x4000)`, actor `+158, +159` is 1, 1 (`0x80017674`, `0x80017680`), **and** `+16` isn't negative (`0x800176C4`) |

[read for the gates; `ed219` goes from `0x8001737C` and `ed220` from `0x800171AC` straight to setup]

- **`ed221` stops asking once its thing is taken, if its sixth argument is negative.** That's 116 of its 124 calls. So EXAMINE on those is gone for good. [read: `0x800176C0`, `0x800176C4` branch to `0x800177B0`; counts proved]
- **Flags:** bit 0 goes to the camera calls (`0x801F0638`, `0x801F077C`, `0x800255A4`, `0x801EE8E0`). Bit 1 makes a taken thing kind 1, not 2 (`0x80017558`). Live, slot 1 look 1 holds flags 6 (its script's 3 with bit 0 cleared and bit 2 set). Bit 2 calls `0x80043C88(0)` at step 0 and `0x80043C88(1)` at the end (`0x80024F64`, `0x8002515C`). Step 0 also sets bit 2 and clears bit 0 if `0x801F0638` or `0x801F077C` says so, and clears bit 0 if `0x800255A4` is nonzero. [read]
- **What bit 2 does is read only this far:** `0x80043C88` stores `r3 + 1` at `r13-25688`; `0x80043CCC` then calls `0x80043D70`, which sets `r13-26272` (`0x80043BE0`, which gates the controller at `0x80007460`) and calls `0x801C588C`, `0x801D5D60`, and `0x80043DE8` or `0x80043FD0`, which set a flag on models through `0x801258BC(.., 15, x)`. `fn111` writes a separate word that runs a like sequence (`0x80043C34`, `0x80043D04`). [read]
- **Thing id:** `0x801F6160(id, r13-23276)` finds the node whose `+36 -> +24` is that id, in the current room, the same id as `fn78(2, id)` (`0x80165D7C`). A node with a null `+52` skips the room test (`0x801F619C`). [proved] Live, slot 7: handle 69 has id 645 and L03/2139 is `ed221(139, 1, 3552088789, 1, 645, -1)`; handle 71 has id 2156 and L03/347 is `ed221(62, 6, 15429850, 1, 2156, -1)`.
- **Taken** means a node that isn't kind 30 state 36 and is on the controlled character's carry list, or is kind 30 state 36. Then kind is 2, or 1 with flag bit 1, `+16` becomes the sixth argument and `+28` 0 (`0x80017548`..`0x8001758C`). [read]
- **Not found** (`0x800174EC`, to `0x800175A0`) gets the same kind and `+16`, and `+28` keeps what `0x801F6160` returned. [proved: slot 8 look 1 is `{2, 2369, 0, 62, 1, .., 0, -1}`, and L03/2369 is `ed221(62, 3, 3622868637, 0, 2028, 1)`]
- **Live check, slot 7:** look 0 is `{0, 2139, 5, 139, 1, -742878507, 4, 69}`: done, 5 steps, -742878507 is 3552088789 as a signed word, and its flags 1 became 4. Look 1 is `{3, 347, 0, 62, 6, 15429850, 1, 71}`: kind 3, ready. [proved]
- A negative `+16` reaching the stepper gives no words step, just RETURN (`0x80024FF4`). That can only come from `ed219` or `ed220`, as `ed221` never asks with one: ed219 83 calls and ed220 0 with a negative second argument. [read]

**Steps of `0x80024D78`** (`+8` counts presses): [read]

| Kind | Step 0 | 1 | 2 | 3 | 4 |
|---|---|---|---|---|---|
| 1 | words, then kind 0 | | | | |
| 2 | hold (`0x80199EA0(0)`), camera, asks DETAIL 34, or RETURN 7 if `+16` < 0 and jumps to step 3 | the words (`0x80024310(element, -2, 0)`) | asks RETURN 7 | lets go, `0x801642D4(script, 0, 0)` | |
| 3 | as kind 2 | the words | asks PICK UP 19 | the find, as above | ends, `0x801642D4(script, 0, 0)` |

Slot is 2 for look 0, 4 for look 1 (`0x80024D94`). Flag 1.

## Talk

- **Asked:** `0x80091F0C`, called from kind 44 `0x80094530`, kind 64 `0x80093B94`, `0x8008FECC`, `0x80092A90`, `0x80092D5C`, sends the player event 147 with no data (`0x80092054`), only if `0x800BF5D0` returned nonzero. Kind 2's event 147 (`0x8005971C`) with no data asks `(2, 22, 1902, 0, 48, 3, 0)`. [read] That these kinds are talking characters is [inferred] from this and the prompt's name.
- **Pressed:** script 1902 (9 levels: 02, 03, 04, 05, 07, 08, 09, 10, 16): `fn79(player, 147, 0, 1)`. [proved: script data and event code agree] With data, `0x800BF720` gives a target, event 147 with data 1 goes to it, and **if it answers 1** (`0x80059760`) the player goes to state 71 (`0x800527A8`, set at `0x80059778`), the partner kept at actor `+0 -> +96`. State 71 sets `0x80117260(128, 1)`, so only slot 0 can show. [read]
- `ed301(2, 22)` is in one script only, L00/583. [proved]

## Doors and ladders: walking in asks

A door or ladder is a region whose **enter** script asks, and whose later run is the room change:

```
if fn0() ~= 0 ...      -- L03/1097: ExitRgn01 in room 147, a sphere with enter 1097
ed301(2, 4)            -- CLIMB DOWN
fn71(21); fn1(1)
... fn0() == 1: fn66(21, EntryMkr01, Camera02, 15, 0, 0)
```

**Walking in never changes room.** Over every script asking 1, 3, 4 or 10, no `fn66` sits in the same state as its ask, and none of these scripts is a leave, stay or plane script. Ids 1, 3, 4 and 10 aren't stick-listed, so the stick can't run them. [proved: counts below, made by `verbs.py` with `regions.json`]

| id | label | files | no `fn66` | `fn66` in the same state as the ask | press word holds `fn66` | other | simple | placed as a region's |
|---|---|---|---|---|---|---|---|---|
| 1 | OPEN_DOOR | 265 | 9 (L00/1147, L00/1563, L00/1837, L03/1147, L03/1231, L05/2491, L05/66, L10/2492, L10/2493) | 0 | 245 | 11 (L00/824, L00/825, L04/154, L05/2718, L07/1142, L07/1224, L07/1225, L08/263, L09/653, L09/755, L10/344) | 200 | enter 265 |
| 3 | CLIMB_UP | 47 | 2 (L00/776, L11/776) | 0 | 45 | 0 | 44 | enter 47 |
| 4 | CLIMB_DOWN | 39 | 0 | 0 | 39 | 0 | 34 | enter 39 |
| 10 | ENTER_SECRET_PASSAGE | 2 | 0 | 0 | 2 | 0 | 2 | enter 2 |

"Press word" means the word the script sets right after asking holds the `fn66`. The common shape, ask at 0, word 1, `fn66` at 1, is the "simple" column, and L03/1097 is one. It is not a rule. [read, for each:]

- **L04/35:** state 0 ends in `fn34`, which runs it again, and only then asks at state 1. `fn66` is at state 2.
- **L04/132, L08/327, L11/358:** two paths from the ask. Word 2 is the `fn66`. Word 1 is a scene ending in `fn1(2); fn36(120)`, so there the **timer** reaches the `fn66`, after the press.
- **L04/138:** asks only under `fn30(112)`, and runs a timed scene first while flag 96 is clear.
- **The 11 OPEN DOOR files marked "other":** word 1 tests something first. L00/824 and L04/154: if a flag is set, `fn1(2); fn34()` reaches the `fn66`, else sound 103 and words, back to word 0. L05/2718: a scene, then the timer. The others weren't read.
- **The 9 OPEN DOOR files with no `fn66`:** at least L05/66 is a door that doesn't open: the press plays sound 103 and shows words. "Locked" is [inferred]: prompt 48 LOCKED is asked by nothing. L00/1147 has the same sound and words behind a test. The others weren't read.
- **L00/776, L11/776:** no `fn66`. They test `ed166(..) == 81` and use `ed68(5)`, `ed83(63)`, `ed55(86)`, `ed89()`, with `fn35`. Not traced further.
- `0x8006D63C` (above) also acts on B with ids 1, 3, 4.

Leaving clears the script's word, unless the entry's `+6` is set (`0x80163ED0`), so coming back asks again. [read] No climbing state was found in the player's own behaviour for these.

## Push

- **PUSH (42) is asked by nothing.** [proved: scan] Words `0,42` exist.
- **Scripts push things with EXAMINE and a yes/no.** L07/1449: `ed301(2, 6)`, `ed12(54, 0, 1449)` whose words are "Should <player> push the shelf?", then on yes `fn22`, `fn21`, `fn47`, `fn80(thing 234, 18, -2, 1449)`. L10/1553 uses the same words. Buttons in the same style: L01/290, L02/1558. [read: the scripts and text.json] Which event 18 on the thing moves it isn't traced.
- **PUSH_OFF (24)** isn't pushing an object: `0x800566AC`, kind 2 state 52, entered by event 107 with data through `0x80074CBC` (`0x80059D40`, `0x80074E64`). Senders of event 107: `0x80070E30`, `0x80071224`. `0x80074CBC` is also called from `0x8003BC60`, `0x80094D08`, for other handlers. It asks with button 6 when bit `0x40` of `0x80033874` (actor `+140 -> +0`) is set (`0x80056BD8`). On entry `0x80117260(8, 1)` lets only slot 0 show; event 111 clears that and sets `0x40` (`0x80056784`..`0x80056794`). Event 44 adds through `0x80060DC4` to actor `+140 -> +357`; at 8, unless bits `0x180` of that word are set, event 112 goes to actor `+140 -> +72`; event 112 sets state 1 (`0x80056850`). [read] Who it pushes off isn't traced.
- **Not found:** a player state for free pushing, with no words. "push" isn't in the dol's strings.

## Buttons: where every press goes

**Read and processed once a frame by `0x8013E518`**, PADRead into `0x805880A0`. Each frame's bits: low 16 the pad. `0x10000`/`0x20000` stick x-/+, `0x40000`/`0x80000` stick y-/+, `0x100000`.. C stick, `0x01000000` L trigger, `0x04000000` L at 150, `0x02000000` R trigger, `0x08000000` R at 150, `0x10000000` D-pad x, `0x20000000` D-pad y. [read: `0x8013E5D4`..`0x8013E694`]

**Bindings** are 32 bytes: `+0` mask, `+4` 0 pressed / 1 held, `+8` handler, `+12` its r4, `+16, +18` shorts its r3 runs between, the rest live state. A channel (`0x8013DB3C(count, table, ..)`) is run by `0x8013E160` only when its `+16` is 0, so `0x8013DBC8` holds it and `0x8013DBC0(ch, 0)` frees it. All are listed at the end. [proved: the first 20 bytes of the rows checked in memory equal main.dol, except the triggers' r4, which is filled at run time with the control block `0x805D7BA0`]

**In play, `0x8023B9B8`**, channel `r13-26224`, run by `0x80006A94`:

| Pad | Calls | |
|---|---|---|
| A, B, Z, stick | `0x80006EBC` | Prompts, above |
| Start | `0x80006CDC(1, 0)` | The Tome |
| R at 150 | `0x801DC730` | Steps `r13-26948` down from 7. What for isn't traced |
| Y, D-pad up, down, left, right | `0x800069DC(1, 0x80334DE8` .. `DF8)` | The five quick spells |

[proved for the table; "The Tome" and "quick spells" read from what they call]

**The player's control**, block `r13-25660`, its flags at `+8`. Channel `r13-23060` (walking) is swapped for `r13-23056` (R held) by `0x8019A02C`. `0x80045BC4` reads the flags each frame and sends events. Events 40, 15, 41, 42, 43 carry `r13-25656`, the others `r13-25660`. [read]

| Pad | When | Flag set by | Controller sends | Kind 2 |
|---|---|---|---|---|
| A | pressed | `0x2` `0x8019A134` | event 40; event 15 instead if `0x1000` is set and `0x8019BADC` is 8 (`0x80045E60`) | `0x8005A2AC`: attack |
| A | still held | `0x1000` `0x8019A144` | see above | What event 15 does isn't traced |
| B | pressed | `0x1` `0x8019A124` | event 46 | No case in the first table; `0x8005AD80` and others are per state |
| Z | pressed | `0x4` `0x8019A190` | event 207 | `0x800597B4`: reload |
| Z | held | `0x2000` `0x8019A1A0` | | Read by state 72 at `0x80053528` |
| Stick | held | `0x8` `0x8019A1B0`, `0x8019A1C4`; `+54`, `+56` | | |
| Stick moving | | `0x80` `0x8019A220`, see below | event 44, else event 45, only while R isn't held (`0x80045F40`) | `0x8005A28C` -> `0x800BE0F0`; `0x8005A254` |
| X | held | `0x20` `0x8019A200` | | `0x8019A220`: `+16` = 1 |
| L trigger | held | `0x10` `0x8019A210` | | `0x8019A220`: `+16` = 2, or 3 if `+62` is set. `0x8004D970` reads it (`0x80046E64`, `0x80069350`, `0x80089DAC`) |
| R trigger | pressed | `0x100` `0x8019A080` | event 41 | `0x8005A138` |
| R at 150 | pressed | `0x200` `0x8019A090` | event 42, not if `0x100` is set the same frame | `0x8005A17C` |
| R trigger | held | block `+0` = 1 `0x8019A02C` | event 43 each frame | `0x8005A1D0`: `0x800C1BA4` |

**Flag `0x80` and the stick** [read]: `0x8019A220` computes `x*x + y*y` (`0x8019A260`, `0x8019A274`, `0x8019A290`), on a stick scale of 0..72 (`0x8023AFE0 +18`). Above 100, so a length above 10, it sets `0x80`. At 100 or less it also sets `0x80`, if the angle test at `0x8019A2E4` passes and the low bits of `+40` are clear, with move mode `+16` of 22 or 15 (`0x8019A2E8`..`0x8019A33C`). So event 44 can fire with a small stick too.

The names Creep (X), Sprint (L), Target (R), Reload (Z) and Quickcast (Y) come from `runtime/host/controls.cpp`. The game's code agrees with where each goes but has no names. [inferred]

**Who runs it:** `0x80045BC4`'s one call is `0x800074C4`, in the function at `0x80007370`. hle.c's comment names `0x80007490`, which is a branch inside that function. `0x80006A94` calls `0x8013E160` on the in-play channel. `0x80006EBC` has no `bl` callers, only the five table entries (`0x8023B9C0`, `..9E0`, `..A00`, `..A20`, `0x802448E8`). The fifth is `ed56(7)`'s table, so while that interface is up B still runs prompts and Start still opens the Tome. [read]

## Attack and targeting

- **Attack:** event 40 (`0x8005A2AC`), not while `0x8006DB04(512)`. By the weapon block (`0x8019B75C`, `0x8019BA1C`), it goes to one of: bit `0x20`, event 69 asks and event 70, state 87; `0x800BD62C`, state 96; `0x800C0DD4`, state 108; `0x800C06BC`, state 6 (`0x8005A524`); `0x800C2A5C`, state 85; state 40 (`0x8005A47C`). [read] What each looks like isn't checked.
- **Targeting:** R pressed: event 41 (`0x801F654C`, `0x800C4204`, `0x800BF4D8`, which calls `0x800BC71C`). R held: `0x800463DC(node, block, 0)`, event 43, `0x801295C0`. Let go: `0x800C0258(node, 1)` once, then events 44 or 45. `0x800465E4` reads `0x8019BA14`, then `0x801F9130`, and tests that thing's `+159` against 18. [read] That it is the chosen target is [inferred].

## Reload

"Reload" as what these do comes from the prompt's name and the controls' names. [inferred] The rest is [read].

- **Asked:** `0x800C030C`: if `0x800C05F0`, `(2, 21, script, 0, 50, 3, 0)`. Script is the weapon's: `0x80150874`, its `+4 -> +42`, or 1582 if 0 (`0x800C0690`). `0x800C05F0` needs `0x800465E4`, a weapon (`0x80046170`, `0x80046254`), bit `0x20` of `0x80150BE8`, and `0x801F9B18`.
- **Z, no prompt needed:** event 207 (`0x800597B4`), gated by `0x8011F2C0`, `0x80122328` low byte 16, not `0x8006DB04(0x100000)`: `0x800C2CA4(node, 1)`, then `(node, 0)`. It runs the weapon's script with `0x801642D4(script, 1, 0)` if `0x801508FC` and `0x801508B0` differ (`0x800C2D18`). **Not the same as the prompt:** it gives up when the weapon's script is 0 (`0x800C2CE8`), so no 1582, and it doesn't check `0x80150BE8`. What those two values are isn't read.
- **Script 1582** (13 levels): `ed82()`, `ed20(0)`, `ed20(1)`, `ed140(a, b)`, `ed9(1582)` or `ed130(error)`, `ed80(player)`. `ed130` (`0x80012F58`) shows the MIX_ERR words 0,31 to 0,35.
- **Script events:** `ed80` (`0x80012CE0`) sends event 155, `0x80079798`, state 77. `ed81` (`0x80012D6C`) sends 177 if its second argument is 1, else 208 (`0x80012DF0`, `0x80012E08`), `0x80079900`, state 75. `ed129` (`0x80012E2C`) sends 198, `0x80079C58` state 90 or `0x80079E48` state 77. `0x800799F8`, called from states 75 (`0x80053298`) and 72 (`0x80053544`), gives state 72.

## Finish him and autopsy

Both in `0x800C030C`, whose checks go in this order: pick up, reload, autopsy, finish him. So only the first true one asks each tick. [read]

- **FINISH HIM:** if `0x800330E0`, `(2, 23, 2244, 0, 49, 3, 2)`. Flag 2 takes an extra path in `0x80117628` at `0x8011786C`, not traced. Script 2244 (15 levels): `fn79(player, 202, 0)`. Event 202 (`0x8005A7BC`): `0x800C30CC`, state 30 at `0x800C35BC`. It also has a path sending event 40 (`0x800C3208`, then `0x800C35F8`), and what picks between them isn't traced. `0x800330E0` sends candidates event 194 (`0x800334B8`). [read]
- **ATTEMPT AUTOPSY:** if `0x80032E8C`, which needs actor `+152` of 44 (`0x80032F28`) and more after it: `(2, 43, 2250, 0, 49, 5, 0)`. Script 2250 is in level 5 only: `fn79(player, 194, 0)`. Event 194 (`0x8005A7F0`): `0x800C38B8`, state 84 at `0x800C3A88`. The conditions before that aren't traced. The ask has no level test. [read]

## Break free

Asked with button 6, priority 50, slot 1, each tick, so no press runs it. Its icon is `\i1`, not B's `\i21`. [read]

| Site | Who | Entered by |
|---|---|---|
| `0x80053ED8` | Kind 2 state 56 | Event 125 with data through `0x80090D40` (`0x800596FC`, state at `0x80090E18`); `0x8008B24C` (`0x8008B390`) |
| `0x800573A0` | Kind 2 state 67 | Event 153 (`0x80059E20`); `0x800D4B38` |
| `0x800578EC` | Kind 2 state 69 | Event 158 (`0x80059F2C`); `0x800CEBA8` |
| `0x80061A64` | Kind 19 behaviour `0x80060EC8`, when `+158` is 1 | What kind 19 is isn't traced |

In state 56 [read: `0x80053EE4`..`0x80053F50`]:

- Event 44 calls `0x80060DC4`, which adds to actor `+140 -> +357`. It counts **only when a direction sector changes** (`0x80060E50`..`0x80060E70`), over two pairs, block `+54,+56` and `+58,+60`. Holding the stick one way adds nothing.
- At 10: event 126 to the grabber (actor `+140 -> +84`), state 1.
- Events 40, 41, 42, 43, 45 do nothing here (`0x80054128`), so A does nothing.
- It also ends on events 8, 53, 57 and a received 126.

`0x80060DC4` is also called by states 67, 69, kind 19 and state 52 (`0x8005745C`, `0x800579B0`, `0x800619E0`, `0x8005680C`), which weren't followed to the end. Whether event 44 reaches state 56 in play wasn't seen. Scripts send event 125 too (L09 and L16: 2004, 2026, 2090, 2770); L09/2004's target is thing 505, not the player. The others' targets weren't read.

## Casting

- **Quick:** `0x800069DC(1, slot)`: unless `0x801F68E0` has bit `0x4000`, refused when `0x80307170 +8` is 1 or 2, or is 0 with flag 307 clear. Then needs `r13-26228` 0 (`0x80006980`) and event 185 answering 1, then `0x801C4250(1, slot)`. Read at `0x800069F8`..`0x80006A78`. Words 0,13 and 0,19 are refusals in `0x801C4250`. [read]
- **From the Tome:** the magick record's next cast `0x80334DFC`, cast from `0x80008468`. From the hle.c comment at `COOP_CAST`; not re-read here, so treat as [inferred].

## Sprint and creep

No prompt. L trigger held and X held set flags `0x10` and `0x20`, which `0x8019A220` turns into a move mode at block `+16`: 1 with X, 2 or 3 with L, only when `x*x + y*y` is above 100. Event 44 goes to `0x800BE0F0`, which reads the block (`0x8019B080`, `0x8019B078`). While R is held the swap to `r13-23056` takes those two bindings away, as that table has no X or L row. [read] Which is creep and which is sprint comes from `controls.cpp`. [inferred]

## Tome, Map, Options, save

- **Open:** Start: `0x80006CDC(1, 0)`. Gates, all at `0x80006CF4`..`0x80006D94`: `0x801E7C98` and `r13-26232` 0, `0x8004303C` 0, a control channel live, event 135 answers 1, `0x801C551C` 0, not `0x8006DB04(1)`. Then `0x80018808(3)`, or by `0x80068D18` and event 84 answering 1: 8 for 25, 7 for 26, 10 for 28. Then `0x80175508`, `0x80007368(2)`, `0x801E4610(1)`. [read]
- **By script:** `ed9(script)` (`0x8000F8D4`) writes `r13-26324` and `r13-24244`. 13 calls to `ed9(1582)` alone. [read]
- **READ_TOME (30)** is asked by nothing. Script 1413 (15 levels) has a variable named iTome and uses `ed301(2, 6)`, then `ed12(205, 0, 1413)`, `ed13`, `ed52(iTome)`, and `ed301(2, 19)` when `ed143()` is 13. [read: the script] Not known to be how all reading works.
- **Pages:** from docs/coop-two-tomes.md, not re-read: Inventory `0x80105F9C`, Map `0x80112594`, Magick `0x8010F228`, Journal `0x8010952C`, Options `0x80101980`. Group 1 of the prompts file, RIGHT_MAP etc., are hints made directly with `0x801DB374` on those pages, not prompts: sites in verbs.json. [read]
- **Map:** table `0x802527D8`, 11 entries, made in `0x80111B40`: B or Start `0x80112568`; L or R trigger `0x801124B8`; A `0x80112420`; Z `0x80112398`; X `0x80112210`; Y `0x801121D0`; D-pad and stick `0x80112104`, `..138`, `..168`, `..19C`; C stick y `0x80112254`. What each does isn't checked. [read]
- **Save:**
  - **One scripted save offer exists.** Room 93 has a box region SaveRgn whose enter script is 960 (`p110` 50), and L12/960 is the only script file 960. Under flags 826 clear, 37 set, 659 clear, it shows words 0,42 WOULD_YOU_LIKE_TO_SAVE_NOW with `ed11(0, 42, 960)`. On yes (`ed13()` 0), it sets flag 823 and calls `ed223()`. Either way it sets flag 826, so it asks once. [proved: the room file and the script] Room 274 has a SaveRgn with no script. [proved] No repeating save point was found in these.
  - `ed223` (`0x800179C8`) sets `r13-26256`, and `0x80006900` (the in-play channel's `+0`) then calls `0x80006CDC(0, 1)`, only when `r13-26232` is 0 and r4 is 1. [proved: live, the channel's `+0` is `0x80006900`] `r13-26256` has other writers (`0x8000FB78`, `0x8000FBC0`, `0x8000FCF0`). `ed223` is in five files: L03/830, L09/888, L11/728, L12/960, L16/888. All but L12/960 set flags 601 or 603, not 823. [read] So `ed223` alone doesn't mean save.
  - Options (`0x8024EDD8`), A (`0x80101838`) by index: 0 `0x80114894`, 1 `0x80115608`, 2 `0x80115FEC`, 3 `0x801140D4`. `0x801140D4` reads flag 823 (`0x801141AC`). [read]
- **Light and Extinguish** (group 2) are words for the `ed56(11)` interface, `0x8007AB94`..`0x8007AC80`, whose row table `0x80244740` has Y, A, X to `0x8007FB1C`, r3 0, 1, 2. [read] Their appearing in play wasn't checked.

## Use and mix

**`build/ed-db/facts/use-mix.md` is that step's report. This one only read these:** Inventory's A is `0x80103EAC` (table `0x8024EF2C`, first row `0x100` A). `r13-24492` is read at `0x801040B8`. `0x801642D4` runs at `0x801041C0`, `0x80104214`, `0x801042A0`, `0x801043D0`. `0x8010433C` and `0x80104368` keep an item's script from `0x80150874` in `r13-24536`, run at `0x801042A0`. [read]

## The interfaces: `ed56(n)` opens, `ed68(n)` closes

`ed56` (`0x80013320`) calls `0x8007AA6C(n)`, a jump table at `0x80244CAC`; n above 11 exits. `ed68` (`0x80013398`) calls `0x8007A968(n)`. [read]

| n | Case | Bindings | Ticks and asks |
|---|---|---|---|
| 0 | `0x8007B5A4` | `0x80244920`, 4 rows | Clears slots 2 and 4 (`0x8007B604`, `0x8007B610`). `0x8007E508`, 53 MOVE STAFF, button 6, is called from its close, `ed68(0)` (`0x8007A9AC`), and from `0x80083598`. Its ask is gated by byte `0x80323938` and `0x8004303C` |
| 1 | `0x8007B834` | `0x802449A0`, 4 rows | `0x8007E9BC` (`0x8007B84C`): 50 SAFE, button 6 |
| 2 | `0x8007B90C` | `0x80244A20`, 4 rows | No ask found; not read in full |
| 3 | `0x8007B964` | `0x80244AA0`, 3 rows | `0x800810D0` (`0x8007B994`): 57 TEXT_A (`0x800810E4`) or 25 SELECT (`0x80081250`), the id in r31 |
| 4 | `0x8007BA50` | `0x80244B00`, 4 rows | No ask found; not read in full |
| 5 | `0x8007B548` | **None:** `0x8013DB3C(0, 0, 0)` at `0x8007B58C` | |
| 6 | `0x8007BD30` | **No-op:** the function's exit. No script calls `ed56(6)` | |
| 7 | `0x8007B50C` | `0x802448E0`, 2 rows: B to `0x80006EBC`, Start to `0x80006CDC` | |
| 8 | `0x8007B478` | `0x80244880`, 3 rows | `0x8007F254`, 55 MOVE HANDS, button 6: called at `0x8007B504`, and rows 2 and 3 of its table |
| 9, 10 | `0x8007AC94`, one case | `0x802447E0`, `0x80244840` | `0x8007E41C` (`0x8007ADE4`): 60, 62, 47 SCRIBE RUNE |
| 11 | `0x8007AB04` | `0x80244740`, 5 rows | `0x8007F610` (`0x8007AB34`): 7 RETURN, button 6, script 0 |

[proved: table `0x80244CAC` and the rows from main.dol against the code; the ask sites read]

**`ed68(n)`** has special paths only for 0, 2 and 9 (`0x8007A970`..`0x8007A990`). Every other n, including 10, takes one default (`0x8007AA1C`), freeing whatever channel is in `r13-25456`. Scripts call `ed68(5)`, `(6)`, `(8)`, `(10)`, and `ed68(6)` has no matching `ed56(6)`. [read]

Calls: `ed56(9)` 22, `ed68(9)` 18, `ed68(2)` 16, `ed56(2)` 9, `ed56(8)` 8, `ed56(11)` 6, `ed56(10)` 6, `ed56(0)` 4, others 1 to 3. Example scripts: L00/853 (1), L05/188 (3), L09/2082 and L00/917 (9, 10), L00/575 (11). What each looks like in play isn't checked. Camera pan and tilt (49, 54) are `0x801EF074`, from `0x800083C8` at `0x8000847C`. [read]

## Kind 2's states

`0x80058DD8` calls these from `0x8005AA30` on. Setter sites from `state_setters.json` (only those with a plain `li r4, N` before `0x801F64A0`). [proved: `verbs_live.py` checks every state-to-function row and every setter's literal]

| State | Function | Set at |
|---|---|---|
| 1 | `0x80058BE8` (tick: `0x800C030C`, `0x800DAB5C`) | |
| 74 | `0x80052384` | `0x800C04C8`, `0x800906C8` |
| 71 Talk | `0x800527A8` | `0x80059778` |
| 95 | `0x80052A98` | `0x800C7994` |
| 48 | `0x80052CBC` | `0x8005D120`, `0x800C89F0` |
| 75, 72, 76, 77, 90 Reload family | `0x800530F4`, `0x800533A8`, `0x80053658`, `0x800538D8`, `0x80053BBC` | `0x800799D0`, `0x80079B18`, `0x80079C28`, `0x80079854`, `0x80079E20` |
| 30 Finish him | `0x80051F0C` | `0x800C35BC` |
| 56, 67, 69 Break free | `0x80053E4C`, `0x8005713C`, `0x80057790` | Above |
| 52 Push off | `0x800566AC` | `0x80074E64` |
| 53 | `0x80056CF0` | |
| 7 | `0x80054188` | `0x800638B4` |
| 85, 6, 108, 96, 40, 87 Attack family | `0x800564C0`, `0x8005430C`, `0x800547D4`, `0x80054AF4`, `0x80055188`, `0x80055868` | `0x800C2C70`, `0x8005A524`, `0x800C1064`, `0x800BDCA4`, `0x8005A47C`, `0x8005A374` |
| 84 Autopsy | `0x80054E70` | `0x800C3A88` |
| 39, 88, 122, 64 | `0x800553C8`, `0x80056028`, `0x80057CB0`, `0x80057F68` | `0x8005522C`, `0x80055CCC`, `0x80057BBC`, `0x800546FC` |

The names in the first column are only from what asks or sets them. [inferred]

## Other kinds

`0x801F5A9C(handler, kind, ..)` registers each at `0x80030C28`.. : `kinds.json`. [read] Kind 30 is an item, `0x800D8634`. [inferred from pickup using it] Live: slot 1 handles 62, 65 to 68, 81, 82, slot 7 handle 69, all kind 30, state 1. An id of 65535 at `+36 -> +24` (handles 65 to 68 in slot 1) is a thing without one. [proved for the values; "without one" inferred]

## Corrections to what the kit says today

| Where | It says | What's true | Check |
|---|---|---|---|
| `primitives.tsv` fn25, `docs/game/systems/items.md` | "used_thing", what was used on a thing | **The current room.** `r13-23276` is printed by `0x80006B84` as `RM: %i`. Live: 147 in slot 1, 21 in slot 3, 153 in slot 6, 155 in slot 7, 115 in slot 10. Scripts compare `fn25()` with 160 distinct values, 159 of them rooms on the disc (not: 403). So 1429's branches on 153 and 155 are rooms | `ed dis 0x80006B80 4`; `.sda(-23276)` |
| `primitives.tsv` fn34 | "stop_self" | Runs the script again as soon as its chunk ends | `ed dis 0x80168E48 3`, `0x80164470 6` |
| `primitives.tsv` ed52 | "run_script_on", `ed52(handle, script)` | The find from a script, `0x80024528` | `ed dis 0x80012BA8 20` |
| `primitives.tsv` fn111 | "bars_show", marked guess | No bars seen in the code read; see the bit 2 note | `ed dis 0x80043D04 12` |
| `docs/game/systems/examine.md` | `ed219`, `ed220`, `ed221` check `+158, +159` | Only `ed221` (`0x80017674`). All three check `0x8006DB04(0x4000)` | `ed dis 0x80017324 30` |
| `examine.md` | "Not traced: the last two arguments of ed221, fourth of ed219" | Above | |
| `examine.md`, `hle.c` | `0x8000704C` runs the script | A return point inside `0x80006EBC`, and a join other paths reach with no script run | `ed dis 0x80007048 2` |
| `ed prompts` | `ed301` is "the game's one way to ask" | 31 sites; EXAMINE's total is 266 plus the 624 from `ed219`, `ed220`, `ed221`, and 20, 21, 23, 24, 25, 43, 47, 49, 50, 53, 54, 55, 57, 60, 62 show 0 there though they're asked | `ed fn 0x80117404` |
| `ed room`, `rooms.json` | One script per region | Up to three, plus three for other nodes, or two for a plane. `rooms.json` once lacked 367 planes and 47 spheres or boxes, and most leave scripts, from a name filter. Fixed: tools/ed/roomfile.py reads the whole list, and `ed room` prints every script | `regions.json`; `python -I tools/ed/facts/verbs_regions.py` |

## Not established

These were in the earlier version, or look true, and failed. **Don't build on them.**

| Statement | Why it failed | What holds instead |
|---|---|---|
| "This is why scripts test `fn0() ~= 100`" | 231 files test word 100; 221 set it themselves; 10 have no setter (L01/103, L01/655, L03/2037, L03/795, L04/1381, L07/2482, L08/1032, L08/2137, L08/442, L10/2692). L00/1413, the example given, sets it itself on the No of a yes/no. The stick is one route to word 100, not the reason for the test | The stick path sets word 100 |
| "Slots 2 and 4 stay until the zone is left" | A press clears them, and so do the others listed under Slots. Leaving also clears slot 4 when only slot 2's zone was left | Step 2's list |
| "A region has three scripts" | Planes have two, at other offsets | Regions section |
| "`rooms.json`'s single `script` is the enter script only" | It keeps a sphere's leave as `script2` (46 rows, room 147 Region05 2449 / 1300), and a sphere with no enter keeps its leave as `script` (room 59 AlterRgn02, room 296 SphereRegion04). It drops boxes' leaves and most planes | Corrections table |
| "The enter event runs once, so it stops at state 1. Only B's `0x801642D4` runs it a second time", state 1 being `fn66` | L04/35 and the three two-path ladders | The door counts |
| "Leaving clears the word", unconditionally | `+6` | The word table |
| "Flag bit 2 hides the bars" | No code read touches the bar nodes. It traced to `primitives.tsv`'s guess about a different entry. What it hides on screen is unknown | Read as far as noted |
| "Pick up has three ways" | Three **askers of prompt 19**. The find has 5 call sites and `ed52` is used in 112 files with no prompt 19. Not every `ed52` call was read | Pick up section |
| "`0x800C362C` tests the player's model", and "event 71 goes to the thing" as general | `r24` is reassigned to the thing's model at `0x800C36B0`. Event 71 goes out only on that branch | Pick up section |
| "A negative `+16` means no words step" for a taken thing | `ed221` doesn't ask at all there | `0x800176C4` |
| "Stick past 100" | 100 compares `x*x + y*y`, and `0x80` is set below it too | Flag `0x80` note |
| "No way of pushing an object was found" | Scripts do, by EXAMINE and a yes/no | Push section |
| "Save: no save point found. No prompt, region or script action is one" | Room 93 SaveRgn enter 960. Nothing here found a **repeating** one | Save section |
| "`0x801140D4` is probably the save screen" | Not proved either way. `0x801140D4`'s bindings reach `0x80114378` -> `0x800AFF14` and `0x801143A8` -> `0x800B01C0`, then `0x800B0CC0` -> `0x800AE098` (a save writer per hle.c line 13630), but indexes 0, 1 and 2 reach that too. A separate static search for `0x80170E30` (CARDWriteAsync's caller) from it didn't reach it | Options list |
| "Each `n` has its own bindings and ticks" and `ed68(n)` closing n | `ed56(5)` has none, `ed56(6)` is nothing, 9 and 10 share, and `ed68` has one default | Interfaces table |
| Interface table's "any" for 53 and 55 | 55 is `ed56(8)`. 53 is called from `ed68(0)` and `0x80083598`; **which n it belongs to isn't settled** | Interfaces table |

## Not found

- **What flag `0x4000` of `0x8006DB04`, and actor `+158, +159` of 1, 6 mean.** Writers of `+159` include `0x80031310`, `0x8008F43C`, `0x800966AC`; not followed.
- **A player state for pushing, or a repeating save point.**
- **What kinds 19, 32, 44, 64 are.**
- **The save screen's card write, proved.**
- **What event 15 does, and R at 150's `0x801DC730`.**
- **What flag bit 2 does on screen.**
- **Which of `ed56`'s interfaces 53 belongs to, and whether each interface's name matches what shows in play.**
- **Whether any of this runs as read: nothing was run.**

## How to check

```
python -I tools/ed/facts/verbs.py                          # remakes every count here
python -I tools/ed/facts/verbs_regions.py                  # regions.json, totals, every save against the files
python -I tools/ed/facts/verbs_live.py                     # memory and code literals, PASS or FAIL
python -I tools/ed/ed.py dis 0x80006EBC 115                   # what a press does
python -I tools/ed/ed.py dis 0x80117404 90                    # the ask, its slots
python -I tools/ed/ed.py dis 0x80024D78 300                   # the close-up's steps, 80025400 asks 19
python -I tools/ed/ed.py dis 0x800C030C 60                    # the four things' asks
python -I tools/ed/ed.py dis 0x80045BC4 300                   # the controller's events
python -I tools/ed/ed.py dis 0x80134758 150                   # the region's scripts, and planes
python -I tools/ed/ed.py script 2246                          # 2244, 2250, 1902, 1582, 2139, 347
grep -c "ed301(2, 4)" build/ed-db/scripts/L03/1097.lua
```

A save state, the prompt slots and looks:

```python
# python -I, from the repo root
import sys; sys.path.insert(0, 'tools/ed'); import savestate
S = savestate.State('saves/slot7.state')
print([[S.s32(0x80335A38 + 28 * i + 4 * k) for k in range(4)] for i in range(7)])
print([S.s32(0x80307108 + 4 * k) for k in range(8)], [S.s32(0x80307128 + 4 * k) for k in range(8)])
n = S.u32(S.u32(S.r13 - 21468) + 4)   # the node list: +4 next, +8 kind, +16 handle, +20 state, +36 -> +24 id
while n:
    d = S.u32(n + 36); print(S.s32(n + 16), S.s32(n + 8), S.s32(n + 20), S.u32(d + 24) if d else None); n = S.u32(n + 4)
```

The regions of the room: `r = S.u32(0x8058AFC0 + (S.u32(0x8058AFC0) << 2) + 12)`, count `S.u16(r + 176)`, list `S.u32(r + 180)`, 116 bytes each.

## Every native ask

[read: literals before each `bl`] Arguments in the table's order: r3 button, r4 id, r5 script, r6 arg 4, r7 priority, r8 slot, r9 flag.

| site | function | where | button | id | script | arg 4 | priority | slot | flag |
|---|---|---|---|---|---|---|---|---|---|
| 0x8000A54C | 0x8000A424 | ed301 | None | r31 or from ed301 | - | - | 50 | 2 | 0 |
| 0x800171E4 | 0x80017154 | ed220 | 2 | 6 | -2 | - | 50 | 2 | 0 |
| 0x800173BC | 0x80017324 | ed219 | 2 | 6 | -2 | - | 50 | 2 | 0 |
| 0x800176E4 | 0x80017604 | ed221 | 2 | 6 | -2 | - | 50 | 2 | 0 |
| 0x80025014 | 0x80024D78 | the close-up stepper | 2 | 7 | -2 | - | 50 | 2 or 4 | 1 |
| 0x80025048 | 0x80024D78 | the close-up stepper | 2 | 34 | -2 | - | 50 | 2 or 4 | 1 |
| 0x8002510C | 0x80024D78 | the close-up stepper | 2 | 7 | -2 | - | 50 | 2 or 4 | 1 |
| 0x8002533C | 0x80024D78 | the close-up stepper | 2 | 34 | -2 | - | 50 | 2 or 4 | 1 |
| 0x80025400 | 0x80024D78 | the close-up stepper | 2 | 19 | -2 | - | 50 | 2 or 4 | 1 |
| 0x80053ED8 | 0x80053E4C | kind 2, state 56 | 6 | 20 | 0 | 0 | 50 | 1 | 0 |
| 0x80056BFC | 0x800566AC | kind 2, state 52 | 6 | 24 | 0 | 0 | 50 | 1 | 0 |
| 0x800573A0 | 0x8005713C | kind 2, state 67 | 6 | 20 | 0 | 0 | 50 | 1 | 0 |
| 0x800578EC | 0x80057790 | kind 2, state 69 | 6 | 20 | 0 | 0 | 50 | 1 | 0 |
| 0x800597A8 | 0x80058DD8 | kind 2 event 147 | 2 | 22 | 1902 | 0 | 48 | 3 | 0 |
| 0x80061A64 | 0x80060EC8 | kind 19 behaviour, its tick (event 3) | 6 | 20 | 0 | 0 | 50 | 1 | 0 |
| 0x8007E494 | 0x8007E41C | interface ed56(9,10) tick | 2 | 60 | 0 | 0 | 50 | 0 | 0 |
| 0x8007E4D0 | 0x8007E41C | interface ed56(9,10) tick | 2 | 62 | 0 | 0 | 50 | 0 | 0 |
| 0x8007E4F4 | 0x8007E41C | interface ed56(9,10) tick | 2 | 47 | 0 | 0 | 50 | 0 | 0 |
| 0x8007E998 | 0x8007E508 | ed68(0) at 0x8007A9AC, and 0x80083598; which n is not settled | 6 | 53 | 0 | 0 | 50 | 0 | 0 |
| 0x8007E9EC | 0x8007E9BC | interface ed56(1) tick | 6 | 50 | 0 | 0 | 50 | 0 | 0 |
| 0x8007F2C8 | 0x8007F254 | interface ed56(8): called at 0x8007B504, rows 2 and 3 of its table | 6 | 55 | 0 | 0 | 50 | 0 | 0 |
| 0x8007F640 | 0x8007F610 | interface ed56(11) tick | 6 | 7 | 0 | 0 | 50 | 0 | 0 |
| 0x800812A0 | 0x800810D0 | interface ed56(3) tick | 2 | r31 or from ed301 | 0 | 0 | 50 | 0 | 0 |
| 0x800C0348 | 0x800C030C | a thing's own, kind 2 state 1 event 3 | 2 | 19 | 2246 | 0 | 49 | 1 | 0 |
| 0x800C0388 | 0x800C030C | a thing's own, kind 2 state 1 event 3 | 2 | 21 | - | 0 | 50 | 3 | 0 |
| 0x800C03BC | 0x800C030C | a thing's own, kind 2 state 1 event 3 | 2 | 43 | 2250 | 0 | 49 | 5 | 0 |
| 0x800C03F8 | 0x800C030C | a thing's own, kind 2 state 1 event 3 | 2 | 23 | 2244 | 0 | 49 | 3 | 2 |
| 0x801EF0C8 | 0x801EF074 | camera pan/tilt, from 0x800083C8 | 2 | 49 | 0 | 50 | 0 | 0 | 0 |
| 0x801EF0EC | 0x801EF074 | camera pan/tilt, from 0x800083C8 | 2 | 54 | 0 | 50 | 0 | 0 | 0 |

## Bindings

[proved: from main.dol; the first 20 bytes of the in-play, walking, R, triggers, stick, `ed56(7)` and `ed56(8)` rows equal memory in every save, except the triggers' r4, which memory has as `0x805D7BA0`. The others are from main.dol and the code that makes them. `ed56(5)` has no rows and `ed56(6)` makes no channel.]

**in play: prompts, Tome, quick spells (0x80006988, channel r13-26224, run by 0x80006A94)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x8023B9B8 | A | pressed | 0x80006EBC | 0x0 | 0, 0 |
| 0x8023B9D8 | B | pressed | 0x80006EBC | 0x0 | 0, 2 |
| 0x8023B9F8 | Z | pressed | 0x80006EBC | 0x0 | 0, 1 |
| 0x8023BA18 | stick-x-+stick-x++stick-y-+stick-y+ | pressed | 0x80006EBC | 0x3 | 8191, 0 |
| 0x8023BA38 | Start | pressed | 0x80006CDC | 0x0 | 0, 1 |
| 0x8023BA58 | R-full | pressed | 0x801DC730 | 0x0 | 0, 1 |
| 0x8023BA78 | Y | pressed | 0x800069DC | 0x80334DE8 | 0, 1 |
| 0x8023BA98 | D-up | pressed | 0x800069DC | 0x80334DEC | 0, 1 |
| 0x8023BAB8 | D-down | pressed | 0x800069DC | 0x80334DF0 | 0, 1 |
| 0x8023BAD8 | D-left | pressed | 0x800069DC | 0x80334DF4 | 0, 1 |
| 0x8023BAF8 | D-right | pressed | 0x800069DC | 0x80334DF8 | 0, 1 |

**walking control, into 0x805D79A0 (channel r13-23060, block r13-25660)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x8023AF40 | A | pressed | 0x8019A134 | 0x0 | 0, 1 |
| 0x8023AF60 | A | held | 0x8019A144 | 0x0 | 0, 1 |
| 0x8023AF80 | Z | pressed | 0x8019A190 | 0x0 | 0, 1 |
| 0x8023AFA0 | Z | held | 0x8019A1A0 | 0x0 | 0, 1 |
| 0x8023AFC0 | B | pressed | 0x8019A124 | 0x0 | 0, 1 |
| 0x8023AFE0 | stick-x-+stick-x+ | held | 0x8019A1B0 | 0x0 | 0, 72 |
| 0x8023B000 | stick-y-+stick-y+ | held | 0x8019A1C4 | 0x0 | 0, 72 |
| 0x8023B020 | X | held | 0x8019A200 | 0x0 | 0, 1 |
| 0x8023B040 | L-trigger | held | 0x8019A210 | 0x0 | 0, 1 |

**while R is held, into 0x805D7AC0 (channel r13-23056, block r13-25660)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x8023AE60 | A | pressed | 0x8019A134 | 0x0 | 0, 1 |
| 0x8023AE80 | Z | pressed | 0x8019A190 | 0x0 | 0, 1 |
| 0x8023AEA0 | Z | held | 0x8019A1A0 | 0x0 | 0, 1 |
| 0x8023AEC0 | A | held | 0x8019A144 | 0x0 | 0, 1 |
| 0x8023AEE0 | B | pressed | 0x8019A124 | 0x0 | 0, 1 |
| 0x8023AF00 | stick-x-+stick-x+ | held | 0x8019A8E4 | 0x0 | 0, 30 |
| 0x8023AF20 | stick-y-+stick-y+ | held | 0x8019A8EC | 0x0 | 0, 30 |

**triggers (0x80199C3C, channel r13-23052)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x802549E4 | R-trigger | pressed | 0x8019A080 | 0x0 | 0, 1 |
| 0x80254A04 | R-full | pressed | 0x8019A090 | 0x0 | 0, 1 |
| 0x80254A24 | R-trigger | held | 0x8019A02C | 0x0 | 0, 1 |

**stick for 0x80045BC4 (channel r13-25648)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x8023F1A8 | stick-x-+stick-x+ | held | 0x80045A2C | 0x0 | 0, 100 |
| 0x8023F1C8 | stick-y-+stick-y+ | held | 0x80045A74 | 0x0 | 0, 100 |

**words open (0x80024310)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x8023E458 | A | pressed | 0x80024220 | 0x0 | 0, 0 |
| 0x8023E478 | B | pressed | 0x80024220 | 0x0 | 0, 1 |
| 0x8023E498 | stick-y-+stick-y+ | held | 0x801DA260 | 0x0 | 0, -5 |

**a find open (0x80024528)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x8023E4B8 | B | pressed | 0x800243BC | 0x0 | 0, 1 |
| 0x8023E4D8 | stick-y-+stick-y+ | held | 0x801DA260 | 0x0 | 0, -5 |

**a thing shown (0x80024A14)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x8023E510 | B | pressed | 0x80024818 | 0x0 | 0, 1 |
| 0x8023E530 | stick-y-+stick-y+ | held | 0x801DA260 | 0x0 | 0, -5 |
| 0x8023E550 | Start | pressed | 0x80006CDC | 0x0 | 0, 1 |

**Options page (0x80101600)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x8024EDD8 | Start+L-trigger+R-trigger | pressed | 0x801018C0 | 0x0 | 0, 1 |
| 0x8024EDF8 | B | pressed | 0x8010180C | 0x0 | 0, 1 |
| 0x8024EE18 | A | pressed | 0x80101838 | 0x0 | 0, 1 |
| 0x8024EE38 | stick-x-+stick-x++D-x | pressed | 0x8010179C | 0x0 | 8191, 1 |
| 0x8024EE58 | stick-y-+stick-y++D-y | pressed | 0x8010172C | 0x0 | 8191, -1 |

**Map page (0x80111B40)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x802527D8 | B+Start | pressed | 0x80112568 | 0x0 | 0, 1 |
| 0x802527F8 | L-trigger+R-trigger | pressed | 0x801124B8 | 0x0 | 0, 1 |
| 0x80252818 | A | pressed | 0x80112420 | 0x0 | 0, 1 |
| 0x80252838 | Z | pressed | 0x80112398 | 0x0 | 0, 1 |
| 0x80252858 | X | pressed | 0x80112210 | 0x0 | 0, 1 |
| 0x80252878 | Y | pressed | 0x801121D0 | 0x0 | 0, 1 |
| 0x80252898 | D-left+stick-x- | held | 0x80112104 | 0x0 | 12288, 10 |
| 0x802528B8 | D-right+stick-x+ | held | 0x80112138 | 0x0 | 12288, 10 |
| 0x802528D8 | D-up+stick-y+ | held | 0x80112168 | 0x0 | 12288, 10 |
| 0x802528F8 | D-down+stick-y- | held | 0x8011219C | 0x0 | 12288, 10 |
| 0x80252918 | C-y-+C-y+ | held | 0x80112254 | 0x0 | 12288, -5 |

**interface ed56(11) (0x8007AB04)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x80244740 | Y | pressed | 0x8007FB1C | 0x0 | 0, 0 |
| 0x80244760 | A | pressed | 0x8007FB1C | 0x0 | 0, 1 |
| 0x80244780 | X | pressed | 0x8007FB1C | 0x0 | 0, 2 |
| 0x802447A0 | B | pressed | 0x8007FF3C | 0x0 | 0, 0 |
| 0x802447C0 | stick-x-+stick-x++stick-y-+stick-y+ | pressed | 0x8007FF3C | 0x0 | 8191, 0 |

**interface ed56(9, 10) first (0x8007AC94)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x802447E0 | A | pressed | 0x80080034 | 0x0 | 0, 0 |
| 0x80244800 | B | pressed | 0x80080034 | 0x0 | 0, 1 |
| 0x80244820 | stick-x-+stick-x++D-x | held | 0x80080794 | 0x0 | 1023, 1 |

**interface ed56(9, 10) second**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x80244840 | A | pressed | 0x80080308 | 0x0 | 0, 0 |
| 0x80244860 | stick-x-+stick-x++D-x | held | 0x80080794 | 0x0 | 1023, 1 |

**interface ed56(8) (0x8007B478)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x80244880 | B | pressed | 0x8007F140 | 0x0 | 0, 1 |
| 0x802448A0 | stick-x-+stick-x++D-x | held | 0x8007F254 | 0x0 | 255, 10 |
| 0x802448C0 | 0x0 | held | 0x8007F254 | 0x0 | 0, 0 |

**interface ed56(7) (0x8007B50C)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x802448E0 | B | pressed | 0x80006EBC | 0x0 | 0, 2 |
| 0x80244900 | Start | pressed | 0x80006CDC | 0x0 | 0, 1 |

**interface ed56(0) (0x8007B5A4)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x80244920 | A | pressed | 0x8007DAEC | 0x0 | 0, 1 |
| 0x80244940 | B | pressed | 0x8007DAEC | 0x0 | 0, 1 |
| 0x80244960 | stick-x-+stick-x++D-x | held | 0x8007DE4C | 0x0 | 511, 7 |
| 0x80244980 | stick-y-+stick-y++D-y | held | 0x8007DFC4 | 0x0 | 511, -7 |

**interface ed56(1) (0x8007B834)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x802449A0 | A | pressed | 0x8007EA00 | 0x0 | 0, 0 |
| 0x802449C0 | B | pressed | 0x8007EA00 | 0x0 | 0, 1 |
| 0x802449E0 | X | pressed | 0x8007EA00 | 0x0 | 0, 2 |
| 0x80244A00 | stick-x-+stick-x++D-x | pressed | 0x8007ECF4 | 0x0 | 255, 6 |

**interface ed56(2) (0x8007B90C)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x80244A20 | A | pressed | 0x80080814 | 0x0 | 0, 0 |
| 0x80244A40 | Y | pressed | 0x80080814 | 0x0 | 0, 1 |
| 0x80244A60 | B | pressed | 0x80080814 | 0x0 | 0, 2 |
| 0x80244A80 | X | pressed | 0x80080814 | 0x0 | 0, 3 |

**interface ed56(3) (0x8007B964)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x80244AA0 | A | pressed | 0x80080D70 | 0x0 | 0, 0 |
| 0x80244AC0 | B | pressed | 0x80080D70 | 0x0 | 0, 1 |
| 0x80244AE0 | stick-x-+stick-x++D-x | held | 0x80080C68 | 0x0 | 0, 1 |

**interface ed56(4) (0x8007BA50)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x80244B00 | A | pressed | 0x800813B8 | 0x0 | 0, 1 |
| 0x80244B20 | B | pressed | 0x800812C0 | 0x0 | 0, 0 |
| 0x80244B40 | stick-x-+stick-x++D-x | held | 0x80081F40 | 0x0 | 0, 7 |
| 0x80244B60 | stick-y-+stick-y++D-y | held | 0x80081FE8 | 0x0 | 0, -7 |

**camera pan and tilt (0x801EEF18)**

| at | pad | when | handler | r4 | r3 from, to |
|---|---|---|---|---|---|
| 0x80301348 | A | pressed | 0x801EF254 | 0x0 | 0, 1 |
| 0x80301368 | B | pressed | 0x801EF254 | 0x0 | 0, 1 |
| 0x80301388 | stick-x-+stick-x++D-x | held | 0x801EF110 | 0x0 | 0, 1 |
| 0x803013A8 | A | pressed | 0x801EF254 | 0x0 | 0, 1 |
| 0x803013C8 | B | pressed | 0x801EF254 | 0x0 | 0, 1 |
| 0x803013E8 | stick-y-+stick-y++D-y | held | 0x801EF110 | 0x0 | 0, 1 |

