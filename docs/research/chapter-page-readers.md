### page
THE CHAPTER PAGE PATH (normal play, mansion, level 0)

1. The Use (script state 0, run from the Tome as the item's Use). For 1572 (chapter 2) and 1574-1580 it goes like this [read ed script 1572..1580]:
- In room 83 the page is refused with "cannot use that now" (ed11(0,0,0)).
- Otherwise it sets flag 306+n (306..314; 315 for chapter 11), sets its state to 1, takes the item (fn93(ed20(0))), and reads and writes stat 3 of handle fn78(1,0) (ed148/ed149; what that stat is was not traced).
- Then fn35(1) (state kept) and ed88: r13-25172=4, so the next 0x800B7208 writes the game state to 0x80307184. That is the mansion snapshot ed89 restores at the chapter's end [read ed fn ed88].
- Last, ed9(script, 6, film). The film depends on alignment ed17 for chapter 2 (6/7/8) and chapter 6 (30/31/32). The others are fixed: 19, 23, 42, 55, 63, 72 [read scripts].
- ed9 writes r13-26324=script, r13-24244=6 and byte 0x8031649A=film [read ed fn ed9; use-mix.md:228-232].

2. The film. As the Tome ends, 0x80110DE8 passes r13-24244 to 0x80018808, so game mode 6 starts in place of mode 2 [read there use-mix.md]. Mode 6's enter reads 0x803162C0+474 (=0x8031649A) at 0x80050254 and indexes the 32-byte table 0x80242150 [read ed dis 0x80050248-0x8005025C]. The rows, read from the dol [read]:
- Film 6/7/8: cin 6, level folder 2, room 166. +14 = 1572.
- Film 19: room 174, script 1574. Film 23: room 249, 1575. Films 30-32: room 303, 1576. Film 42: room 103, 1577. Film 55: room 336 (+28 = 82), 1578. Film 63: room 237, 1579. Film 72: room 27, 1580.
- A row's +14 is the script run when the film ends (0x80026FCC → 0x801642D4) [read ed fn ed55]. So the film's end runs the page script again.

Separately, mode 2's enter (0x80008280-0x800082A8) runs any r13-26324 > 0 [read ed dis 0x80008234]. Which of the two reruns it is [inferred]. Either way the script comes back in state 1.

Chapter 3 (1573) and chapter 11 (1581) leave ed9(script) with mode 2. They then step through their own states:
- 1573: an FMV ed83(2/3/4) by alignment (unless flag 757 is set), then ed55(15), then ed87(3,3,0).
- 1581: ed55 164/82/163, ed83 39/37/38, ed55 85/83/84, then ed87(11,11,0).
[read scripts]. 2028 is simply ed87(1,1,0), with no film.

3. ed87(a, chapter, save) at 0x800154FC [read ed dis 0x800154FC-0x800157E4]:
- Calls 0x800AD218(0x8030714C), the header save (0x800AD46C too if save != 0).
- 0x80307174 takes the old 0x80307170. 0x80307170 = a (0-11). 0x80307178 = chapter (0-11, 13, 15, 16).
- r13-23276 = 0x8023D738[chapter]. That table is 69, 341, 166, 150, 338, 249, 303, 103, 82, 237, 27, 50, 100, 64 [read dol].
- 0x80022328(9): r13-25992=9 and mode 5 [read ed dis 0x80022328].

4. Mode 5's case function 0x80021648(9) (called at 0x800226CC):
- Common teardown first: many globals cleared, 0x80119090, 0x80117288, and others.
- Then the jump table 0x8023E280[9] = 0x80021E28, then the inner table 0x8023E244[9] = 0x80021E58 [read dol].
- That block calls 0x80155928, 0x80153D50(...,351,2), 0x80163BA4 and 0x80042974(0,0) (fn22 off). Then 0x80018808(1) (mode 1), and r13-23276 is set again from 0x8023D738[chapter] (0x80021EB0). The tail calls 0x8004F168(chapter) and 0x8019D780 [read ed dis 0x80021E28-0x80021F3C].

5. Mode 1's frame, 0x80020328: where the new level's player is made [read ed dis 0x80020328-0x8002042C].
- If r13-26048 != -1: the room 0x80152298(r13-23276) and the film 0x8004F374(r13-26048, -26052) are loaded at once. New Game uses this: room 96 and film 96 (0x8001C794, 0x8001C7FC).
- Otherwise (the page path and Jump to Game):
  - Character: 0x80007C8C(chapter) (the host's kChapterModel 9, 0, 70, 85, 150, 18, 73, 74, 76, 69, 75, 81). Era: 0x80007D2C(chapter).
  - Level data: 0x8003FC78(chapter, 1), then 0x8003FD30(0).
  - Scripts: 0x8003F4F4(chapter), or (12) when flag 787 is set and chapter != 13.
  - State: 0x800ADADC(0, 1, 1, character, era).
  - Then 0x8004222C(chapter), and mode 2 (play).
- 0x800ADADC loads the first room, 0x80152298(r13-23276) at 0x800ADC24, then 0x80007E4C → 0x80007EC0 → 0x800074E8, the player maker [read].
- 0x800074E8 builds the record with 0x80040E94, switches on the chapter (table 0x8023BEE8; chapter 0's case tests flag 894 and room 100 and sets about 25 flags), opens the character file 0x80132ED8(model) at 0x80007844, and makes the player with 0x8003121C at 0x8000784C [read]. Where the place is taken from was not read.
- The host swaps the model for player 1's choice: hle_chapter_character, hle_player_file at lr 0x80007848, hle_pack_load [read hle.c:11089-11101, 11697-11730].

JUMP TO GAME (title)
- The grid pick (0x8001A6F0-0x8001A72C) writes 0x80307178 = chapter from a grid table, 0x80307170 = 0x80307174 = a, and r13-26048 = r13-26052 = -1 (0x8001A740-0x8001A74C) [read].
- The fate pick calls 0x8003F4BC(choice+1), which sets the alignment (0x8001AD0C-0x8001AD18) [read].
- The card screen held at 0x8001A164 calls 0x8001FC14(1) → 0x8001C86C. That sets r13-23276 from 0x8023D780 (the same rooms with 9 and 10 swapped: ...82, 27, 237...) and calls 0x80018808(1) (0x8001CC54, 0x8001CCA4) [read].

The differences from the page path:
- No mode 5 teardown and no ed87 header save.
- No ed88 snapshot of the mansion state.
- No flag 306+.
- No film: r13-26048 = -1, and the page's film is played only by the page in level 0.

After that, both paths come together at mode 1's frame, in the same start room with the same chapter character [read]. Chapter 1's own opening is in-level in both.

PLAYER 2 IN BOTH PATHS [read hle.c]
- When the level goes, coop_pad_read logs "player 2 is gone (an area change?)" and clears g_coop_p2 (31958-31967).
- Once player 1 is in play, coop_spawn (12173) remakes player 2. It copies player 1's record (0x80040E94/0x8003121C) placed by coop_place beside player 1, uses player 2's chosen model, and calls set-room, register and add. It tries up to 3 times (31991-31996).
- So the arrival is already the same as Jump to Game's. What differs is before the load.

CO-OP GAPS ON THE PAGE PATH [inferred unless marked]
- (a) The film is player 1's scene. A scene the other player began never shows in their half, by design (hle.c:17826-17827 [read]). So player 2 doesn't see the page's film. The user wants both to see it.
- (b) The page used from a copy Tome. ED_COOP_TOME_COPY=2 gives both players copies. tome_copy_close and tome_copy_p1_close take only r13-26324 and the next cast from the copy RAM (hle.c:16294, 16344 [read]), then run it as an item script (tome_copy_left). The copy's r13-24244=6, the film byte 0x8031649A and ed88's r13-25172 are not carried over. So the film and the mansion snapshot may be lost, and the script may run in the wrong state. Not traced: next is a `[trace]` run of a page used by each player.
- (c) ed89's return at the chapter's end loads the 0x80307184 snapshot (player 1's mansion state). Player 2 is then remade the same way.

### host
THE HOST AT A CHAPTER START (Jump to Game vs a Chapter Page's ed87)

1. How player 2 comes to exist at a Jump to Game chapter start
- No level-load hook makes player 2. They come from the pad read. hle_PADRead calls coop_pad_read(cpu, !menu && !cutscene) on every read except the monitor thread's [read hle.c:4318]. On the first "playing" read with g_coop_p2 == 0 it does two things [read 31985-31996]:
  - coop_adopt(player): looks for a second node of player 1's kind (type/model at actor +152), meaning a player 2 from a save [read 7213-7246].
  - If none is found, coop_spawn(player) runs, at most 3 tries per area (g_coop_attempts), and only once coop_wanted_character() is loaded [read 31992].
- coop_spawn [read 12173-12213]:
  - Spawn record from COOP_SPAWN_INIT 0x80040E94. Type = player 1's actor +152. Model = Mods' Player 2 choice (coop_wanted_character), else player 1's actor +154. rec+40 = player 1's +8.
  - Place = coop_place beside player 1's object (COOP_SPAWN_DISTANCE 150, zone and floor tests) [read 7136-7169].
  - Made with 0x8003121C (g_coop_spawning makes hle_spawn_create pass it through), then SET_ROOM 0x801F64C8 with player 1's room word, REGISTER 0x801F5D18, ADD 0x801DCB58.
- The run log confirms it: "[coop] player 2 (listed)" is coop_spawn's line, and no "from the save" line appears (logs/run-20261115-102912-jtg-tome-state-ch5/page-0001.log:194) [read there].
- Hooks that do fire at the level load, all keyed on addresses and return addresses, not on how the level was entered:
  - CHAPTER_CHARACTER 0x80007C8C sets g_chapter_model and player 1's model [read 11696-11709]. It is called from mode 1's frame 0x80020398 with the chapter word 0x80307178 [read dis].
  - PACK_LOAD 0x8013209C at lr CHAPTER_PACK_CALL 0x800408FC sets g_chapter_model from /levels/levelNN, resets g_playables_made, and borrows player 2's body when they are another chapter's character (borrow_p2) [read 11090-11110].
  - COOP_SPAWN_CREATE (hle_spawn_create): player 1's record is made as the chosen character. A second record of the chapter's kind (a co-op save's) is made as player 2 [read 12075-12152].
- Camera: player 2's first new zone list (coop_zones_new) drops their camera set and remakes it from the room's start [read 17831-17866]. The split turns on right after the spawn ("[split] on", same log) [read there].
- Items: the Tome lists what player 1's node carries, so both share it [read 29908-29918]. Player 2's weapon copies are reset when the player 2 node changes (coop_copies_reset) [read 29933-29937].

2. Would the same run after a Chapter Page's ed87, mid-game?
- ed87 0x800154FC does: r13-23276 = start room table[chapter], then 0x80022328(9). That stores reason 9 at r13-25992 and asks mode 5 [read dis 800157C0-800157E4, 80022328].
- Mode 5 (0x800224B8) calls 0x80021648(reason), the play start. Reason 9's case 0x80021E58 asks mode 1 (li r3,1; bl 0x80018808 at 0x80021E8C) and resets r13-23276 again [read dis; jump tables 0x8023E280 and 0x8023E244 read from generated/main.dol].
- So a Chapter Page goes through the same mode-1 chapter load as any start. CHAPTER_CHARACTER, the pack load and the spawn-record hooks would fire as at Jump to Game [inferred, from the same addresses; the pack-load call chain for reason 9 was not followed past mode 1].
- Pages call ed87(n,n,0), so no full save is written [read `ed fn ed87`]. No player 2 record is carried, so coop_adopt finds nothing and coop_spawn makes a fresh player 2 beside player 1. That is the same route as Jump to Game [inferred].
- The old player 2 node is freed by the level reload. coop_pad_read clears it when it isn't in the list: "player 2 is gone (an area change?)", which also resets own words, g_coop_attempts, p2_state and parked [read 31958-31967].
  - Risk [inferred]: coop_listed compares only the address (6913-6919). A new-level node at the old player 2 address would be taken for player 2 if no pad read happened while the list was empty. The two runs I looked at had different addresses per chapter (0x81129058, 0x8111815C).
- The spawn waits for !cutscene && !menu [read 4318, 31992]. If the chapter's opening scene plays in the new level, player 2 is made only after it, beside player 1, and is not in that scene [inferred].
- Not traced in co-op: I scanned the 3000 newest logs/run-*/EVENTS.log files. Only the jtg new-game and dream runs show two chapters, both at boot or start, so no logged run has an in-play ed87 [read].
- A working mid-game precedent: characters_reload saves and then calls GAME_END(2). Player 2 comes back from the save record through hle_spawn_create and coop_adopt [read 31908-31922, 12094-12104].

3. Separate-rooms state across a level change (nothing resets on it)
- The only host reset at play start 0x80021648 is coop_finish_play_start, which clears g_fin_p2_words [read 24211-24215]. Everything else below resets only on a quickstate load (hle_coop_state_load 33380-33420) [read].
- g_coop_p2 == 0 makes coop_stay_apart false [read 14564-14569]. After the respawn player 2 is in player 1's room, so they are not apart.
- g_room_slot_room[2] (the host's cache of each slot's room) changes only at an unpack ask (set to -1) or the per-frame read of loaded records [read 13971-13978, 14113-14128].
  - Slot 1 sits in host memory at 0x83000000. Its layout ran once, before the title (jtg log line 32) [read there]. Reason 9's case doesn't call the play-start allocation 0x80020EB4 [read dis, partial].
  - Risk [inferred]: room numbers are per level. A mansion room left resident in the other slot could match a new-level room number, and 0x80154D64's "already in the other slot" would then swap it in with no unpack.
- g_coop_room_p2 and g_coop_entered_p2 are set only in notify_room_ticket [read 11596-11609]. A level's first room comes as "[room] change/request", not a ticket (jtg log 140-150) [read there]. So they keep their last door's value; 14705 and 15218 clear them at the end of player 2's change.
- g_stay_bracket is non-zero only inside a ticket runner, so it is not affected.
- g_tome_trip is reset only by tome_trip_state_loaded (11521). If a trip is open when player 2 is gone, tome_trip_orphan clears it after 120 frames. At state 3 or later, with player 1 in room 5 (TRIP_ROOM, which would be the new level's room 5), it first writes player 1's way-back ticket [read 15707-15727; risk inferred].
- g_coop_tome_p2 and g_reload_tried are cleared only on a state load [read 33392-33393].
- Lights (lights_frame) and the split are per-frame state with no level reset [read 14132].

Answer: from what I read, a Chapter Page's level change makes player 2 the same way Jump to Game does: a fresh coop_spawn beside player 1 once play resumes. Player 2 is not left over from the old level, and nothing skips making them [inferred]. What remains unchecked:
- No co-op run has done it.
- The scene that begins the chapter plays with player 2 not yet made.
- Stale host words: the room-slot cache, g_coop_room_p2, the Tome trip, and the node-address match.

### other
Chapter Page use by either player, together and apart (no edits, builds or runs)

How the page works in the game
- Page scripts such as 1572 have two states. State 0: refuses in room 83 ("cannot use that now"), sets flag 306+n, `fn1(1)`, takes the page (`ed20(0)`/`fn93`), `fn35(1)`, `ed88()` (game_keep: sets r13-25172=4 so a later 0x800B7208 call writes the mansion snapshot at 0x80307184 for `ed89`'s way back), then `ed9(1572, 6, x)`. x is 6/7/8 by alignment; 1575 uses 23. State 1 is `ed87(n, n, 0)` [read `ed script 1572/1575 --level 0`].
- `ed9` mode 6 writes r13-24244=6 and byte 0x8031649A=x. As the real Tome closes, 0x80110DE8 passes 6 to 0x80018808, so game mode 6 (the cinematic) starts. The left script runs on entering mode 2, not at the Tome's end (0x80008280-0x800082A8 runs r13-26324). So the page's cutscene plays first, then mode 2 runs 1572 again at state 1, which calls ed87 [read use-mix.md:228-232; `ed dis 0x80008234`]. That x is a cinematic record number is [inferred].
- `ed87` writes 0x80307170/74/78 and sets the start room r13-23276 = 0x8023D738[chapter]. It calls 0x800AD218 (the header). With c=0 it skips 0x800AD46C. Then 0x80022328(9) asks for game mode 5 and a load [read `ed fn ed87`; b0-k7-readers.md:39]. 0x800AD218 "reads neither" player pair [read `ed who`]. So ed87 has no player-identity problem.

Player 1 uses the page (their real Tome)
- The native path runs: Tome close, then mode 6 cinematic, then mode 2, then 1572 state 1 / ed87 by TOME_SCRIPT_RAN, then the load [inferred from the above; tw_keep at hle.c:13814 only drops the Tome's room read-back]. After the load, the old player 2 is unlisted, so "player 2 is gone" clears g_coop_p2 (hle.c:31957). coop_pad_read then calls `coop_spawn` beside player 1 in the new chapter (hle.c:31993-31996, 12173) [read]. Not traced in a run.

Player 2 uses the page (their copy Tome, ED_COOP_TOME_COPY=2)
- In the copy, Use calls 0x801642D4(1572, item). coop_run_script records g_tome_copy_use_script/item (hle.c:20410). Only state 0 runs there, in the copy's memory, which is thrown away [read].
- At close, tome_copy_close carries only r13-26324 (script_left), the next cast and the spell item (hle.c:16341-16346). It does NOT carry r13-24244=6 or 0x8031649A [read].
- tome_copy_left (hle.c:31545) then swaps player 2 in: `coop_be_player2`, `coop_stay_room_in`, zones, `coop_states_swap`, cameras lent. It runs 1572 twice in the real game, back to back:
  - (a) state 0 again, since use_script == script (hle.c:31645): flag, take, ed88, ed9.
  - (b) at once, state 1: ed87 (hle.c:31652).
  [read]
- No real Tome closes, so mode 6 never starts. **Player 2's page skips the page's cutscene and goes straight to the chapter load** [inferred, strong].
- r13-24244=6 and x stay in the real memory. A later real Tome close could turn into mode 6 by mistake [inferred; not checked whether the load or the Tome resets it].
- ed88 and ed87 run in the same host call. Whether 0x800B7208 writes the 0x80307184 snapshot before mode 5's reset is unknown. If it doesn't, ed89's return to the mansion after the chapter would find a stale or empty snapshot [inferred].
- After ed87, `coop_p2_script_mark(1572, true)` and the swaps are put back. Of the host words reset on a state load (hle.c:33366-33390: g_coop_use, g_coop_screen, g_coop_cameras.lent, g_coop_zones.list, g_tome_copy_p1_left, g_stay_*), none is reset by "player 2 is gone" on a chapter change [read].

Apart in the mansion (ED_COOP_STAY / ED_ROOM_SLOTS=2)
1. **Player 2 uses it apart: wrong start room.** `coop_stay_room_in` makes their room current, which is right for 1572's room-83 test [read]. But after the script, `coop_stay_room_out` writes keep[0] (player 1's mansion room) back over r13-23276 and restores the slot-table indices (hle.c:15103-15107, called at the end of tome_copy_left). That wipes ed87's start room before the deferred load reads it. The new level would load a level-0 room number: wrong room or a hang [read the writes; result inferred].
2. **Player 1 uses it apart.** The mode 6 cinematic replaces the actor list and zeroes the camera stack. Its teardown reloads the current room only (op 23, b0-k7-readers.md:147; readers.md:436, 506) [read]. Room changes also mark the other slot's record dead (the ED_ROOM_SLOTS comment, hle.c ~13840) [read]. Player 2's room is torn down under them for the cinematic, and the host's stay state is not reset there [inferred]. The ed87 load then replaces both rooms, and player 2 is made again beside player 1 (the respawn above), so they would arrive together [inferred]. Stale g_stay_*/g_stayer words would carry into the chapter.
3. **Player 1's Start can be refused or made a copy**, in player 2's door window (hle.c:30921-30930). Inside it, player 1's page would take the copy path above (item replayed by tome_copy_p1_left), with the same missing cinematic [inferred from the shared design].
4. The Tome trip notes say the mansion Tome and P12 (chapter 13's film into room 5, a mode 6 direct load 0x801525D4) were not built. Nothing in the host handles mode 6 while apart [read tome-trip-impl-notes.md:3, 134; presses-effects-readers.md:504 "mode 6 ticks; its own end ticket is lost"].

What a fix needs [inferred]
- For player 2's page, the real game should do what the copy's Tome close would have done. Carry r13-24244 and 0x8031649A out of the copy (tome_copy_close), then run state 0 only, and let mode 6, then mode 2, then state 1 (ed87) follow natively, so both see the cinematic.
- Skip `coop_stay_room_out`'s room write when ed87 has changed 0x80307178.
- On a chapter change, clear the host words that a state load clears.

Logs
- I found no Chapter Page use or `ed87` from 1572-1581/2028 in any co-op log. I searched the page-0001.log files for the traced call `calls 0x800154FC` (only runs with the trace on print it). Every hit was L00/1911's `ed87(0, 16, 0)`, the new game's dream: run-20261106-115419-jtg-vfol-c1d-F-bad, run-20261106-222556-jtg-newgame-coop, and run-20261107-142252 / 142603-jtg-dream-end(2).
- What they show (dream-end2 page-0001.log:172-228) [read]:
  - 1911 ran at the opening cinematic's end (lr 0x80026FE4, mode 6's finish), and ed87 was called at frame 1173.
  - "[player] chapter 16's character" at 1174, then player 1 was made at 1176.
  - At 1177-1178, player 2 was listed at -523 -433 1155 (player 1 at -673) and the split came on.
- So a co-op chapter change by ed87 loads cleanly and makes player 2 fresh beside player 1. But player 2 did not exist before that change, so this is not yet proof of carrying two players from the mansion. No hang report names it.

### REFUTE
CHECK OF THE THREE READERS (ed script/dis/fn, the dol, hle.c, camera.c, logs; nothing edited, built or run)

Confirmed
- Page scripts 1572, 1574-1580: state 0 refuses in room 83, sets flag 306+n, takes the item, then fn35(1), ed88, ed9(script,6,film). State 1 is ed87(n,n,0) [read ed script]. These scripts also exist in levels 02-10, 12 and 13 [read].
- ed9 writes r13-26324, r13-24244 and byte 0x8031649A. 0x80110DE8 passes r13-24244 to 0x80018808 [read dis]. Mode 2's enter runs r13-26324 at 0x800082A8 [read].
- Film table 0x80242150: row 6/7/8 is cin 6, room 166, +14 1572, level 2 (+16 top byte). Row 19 is 1574 (+28 = 338). Row 55 is room 336 (+28 = 82). The film's end runs +14 at lr 0x80026FE4 (the host's CINEMATIC_END_RAN_B, hle.c:20224) [read dol + dis].
- ed87 sets r13-23276 = 0x8023D738[ch] and calls 0x80022328(9) [read]. The tables 0x8023E280[9]=0x80021E28 and 0x8023E244[9]=0x80021E58 are right. That case asks mode 1 and sets r13-23276 again from 0x80307178 at 0x80021EB0 [read].
- Mode 1's frame does 0x80007C8C, 0x8003FC78, 0x8003F4F4, 0x800ADADC and 0x8004222C. A r13-26048 of -1 skips the film load [read 0x80020344-0x8002042C].
- Chapter 3's page (1573) goes ed9(1573) mode 2, then in-level states: ed88, fn139, FMV, ed55(15), state 5 ed87(3,3,0) [read].
- tome_copy_close and tome_copy_p1_close carry only r13-26324, the next cast and the spell item. They do not carry r13-24244 or 0x8031649A [read 16291-16346].
- tome_copy_left runs the script twice, state 0 then state 1, with player 2's state table swapped in (coop_states_swap) [read 31580-31654].
- "player 2 is gone" clears g_coop_p2, then coop_adopt runs, then up to 3 coop_spawn tries [read 31958-31996]. coop_listed compares the address only [read 6913].
- The dream precedent is real. ed87 is called, then "chapter 16's character", then player 2 is listed and "[split] on at 1178" [read run-20261107-142603 page-0001.log].
- coop_stay_room_out writes keep[0] to r13-23276 [read 15103]. tome_trip_orphan behaves as described [read 15707]. coop_finish_play_start clears only g_fin_p2_words [read 24211].

Wrong
- "Player 2 doesn't see the page's film" (page (a)). A full cinematic shows the frame whole: camera.c:1532 says "in a cutscene, the Tome or with no player, the frame shows whole", and 1633 calls split_off "a cutscene". The hle.c:17826 comment is about the arrival camera. So when player 1 uses a page, both players see the film on the one screen [read; not run].
- "coop_stay_room_out wipes ed87's start room, so the wrong room or a hang" (other, 1). Mode 5's case rewrites r13-23276 from the chapter word at 0x80021EB0 before mode 1 loads anything [read].
- "The spawn waits for the chapter's opening scene, so player 2 is not in it" (host). In Jump to Game chapter 1, player 2 was listed and the split came on at frame 1396, during the in-level intro (1260-1865) [read logs/run-20261105-195957-jtg-pius-dirs2, jtg.sh:33]. cutscene_playing means a movie or the letterbox only (hle.c:3894).
- "A stale r13-24244=6 turns a later Tome into mode 6" (other). This is unlikely: Start sets 2 and the open Tome sets 3 (tome.md:7, use-mix.md:230) [read docs].

Unverified
- Which re-run calls ed87 in solo: the film's end (likely, as 1911 did at 0x80026FE4), or mode 2's r13-26324. Also, who clears r13-26324: there is no write in the 0x80021648 teardown [read 200 instructions].
- Whether ed88's snapshot (request 4, written by the next 0x800B7208) lands before mode 5 when ed88 and ed87 run in one host call.
- The risks from the mansion slot cache and the node-address reuse.

WHAT HAPPENS TODAY
- Player 1 uses a page with the real Tome, together. The Tome closes into mode 6 and the film plays whole for both. The film's end runs state 1, ed87, then mode 5 and mode 1. "Player 2 is gone", and coop_spawn makes them beside player 1 in the start room. This is the same arrival as Jump to Game, but carrying the mansion's host words [inferred; never run].
- Player 2 uses a page with the copy Tome, together. No real Tome closes and r13-24244 is not carried, so no mode 6 runs. tome_copy_left runs state 0 and then state 1 back to back, so ed87 fires at once. There is no film for either player, and then the respawn as above [inferred, strong].
- Apart. Player 2's page: the same as their page together, but with no film. Player 1's page: mode 6 tears down the rooms, and the g_stay_*, room-slot cache, g_coop_room_p2, Tome-trip and copy-left words carry into the chapter. Nothing resets them on a level change; only a quickstate load does (33380-33420) [read].
- Chapter 3 or 11 page by player 2: the later states run as player 2's, and ed87 ends up inside a player 2 use [inferred].

WHAT JUMP TO GAME DOES DIFFERENTLY FOR PLAYER 2
- The host does nothing special for player 2 at Jump to Game.
- The difference is that it starts from a cold host (no stale words), with no film, no ed88 and no mode 5 teardown.

SMALLEST ENGINE-GENERIC PLAN (co-op only, solo untouched)
1. In tome_copy_close, also read the copy's r13-24244 (the mode its Tome close would ask).
2. In tome_copy_left, when that mode is not 2, treat it as an item leaving a film:
   - Run only the Use step in the real game, as player 2 (their room in, so the room 83 test still holds). This sets real r13-26324, r13-24244 and the film byte, and asks for ed88.
   - Run it in the game's own state table: no coop_states_swap and no coop_p2_script_mark, since a chapter change belongs to everyone.
   - Put the swaps back, then call 0x80018808(r13-24244) as 0x80110DE8 does.
   - The film then plays whole, and its end runs state 1 (ed87) natively, exactly as on player 1's path.
3. When 0x800154FC (ed87) is entered inside a player 2 use or turn, end the use and put the swaps back first. This covers 1573 and 1581.
4. At play start 0x80021648 with reason 9 only:
   - Clear the level's host words, the same set hle_coop_state_load clears.
   - Also set g_room_slot_room[] to -1, and clear g_coop_room_p2, g_coop_entered_p2, g_tome_trip, the p2 script marks and g_tome_copy_p1_left.
   - Set g_coop_p2 = 0, so coop_spawn remakes player 2 as at Jump to Game.

RUNS THAT WOULD PROVE IT
- Getting a usable page:
  - Fresh mansion: JTG_NEWGAME=1 through the dream into level 0 as Alex (the dream-end runs got that far).
  - Quick smoke test: jtg.sh 12 (Alex End). Level 12 holds 1572-1580, but it is not the mansion between chapters.
  - Spawn the page with ED_ITEM_SPAWN=frame,200,200,300,WHO. WHO is 1, 2 or 3 (both); default 3. Pick it up with PICK UP, outside room 83. Item 200 is the chapter 2 page (1572), item 201 the chapter 3 page (1573).
  - Use ED_RUN_AWAY=1 and the trace on.
- R0, solo (mod_coop=0): Tome close, mode 6, a film-end lr, then 0x800154FC, room 166, Ellia. This settles which call runs ed87 and who clears r13-26324.
- R1, co-op, player 1 uses page 200 together. Expect "[split] off ... a cutscene" during the film, then gone/listed/split on in room 166. Diff against jtg.sh 2 for room, character and places.
- R2, co-op, player 2 uses page 200 together. Today: "leaves script 1572", then 0x800154FC with no film. After the fix it should match R1.
- R3 and R4: ED_COOP_STAY=1 ED_ROOM_SLOTS=2, each player uses the page apart. Expect no stale [stay]/[tome-trip] lines in chapter 2 and no saves/reports/hang-*.
- R5: item 201 (1573) by player 2. R6: player 2 in room 83, which should refuse.
- Later: a chapter's end back through ed89, to check the 0x80307184 snapshot.

Files: E:\Eternal Darkness Recomp Fork\runtime\host\hle.c, E:\Eternal Darkness Recomp Fork\runtime\host\camera.c, E:\Eternal Darkness Recomp Fork\tools\ed\jtg.sh