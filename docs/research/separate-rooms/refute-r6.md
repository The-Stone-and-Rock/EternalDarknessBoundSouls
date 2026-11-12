# R6 refuters (2026-11-11 night): three lenses on design-r6.md

Read only. Facts carry EVIDENCE.

######## refute:game
REFUTER REPORT, game-code lens: R6 design (docs/research/separate-rooms/design-r6.md) D1, D2 and D5. Read only. I checked every claim against build/ed-db/disasm.txt, `ed dis`/`ed fn` and runtime/host/hle.c, and wrote a static bl-closure scan to depth 9. It does not follow sends or bctrl.

## Problems

**P1. D2 leaks a hit block on every 58 it drops. Sure. Medium.**
- Event 58 carries a hit block. 0x800CA4E4 allocates it and posts it as r6 (0x800CA6A0-A8, 0x801F57C0).
- The receiver owns the block. The player's case 58 (0x80059BF4-0x80059C40) sends 39 synchronously (from = the 58's from, to = the 58's to, data = the block), then frees it with 0x8019B4C4 (0x80059C30-38).
- The delivery code frees a 58 or 237 block only when the message can't be delivered (0x801F5038-0x801F5060, 0x801F5064-0x801F508C). Nothing frees it after the handler has run.
- So "answer 0, the handler not run" never frees the block. The table holds 40 blocks (0x8019B418, 0x805D7BF0, `cmpwi r5,40`). When it is full, 0x8019B334 takes back the oldest block whose attacker isn't in a move. That probably prevents a hard stop, but blocks pile up. State 37 sends a 58 every 128 frames (0x8003B374-0x8003B428), so it repeats.
- **D2 isn't needed.** The forwarded 39 goes to player 1's case 39 (0x8005A604), then to 0x80061B9C. With the attacker in room B and the victim in room A, D1 gives T = B and the game's gate refuses it. The 58 case then frees the block itself. Coming in through coop_as_player2, player 2's 39 is refused the same way.
- **Smallest fix:** drop D2. Or, if it stays, call 0x8019B4C4(message+28) before answering 0.

**P2. The design calls 0x800637B8 "the death". It isn't. Sure. Low: no wrong behaviour, but check c1's wording is wrong.**
- 0x800637B8 is event 53, the hurt reaction. It plays the move at block +136 (0x80063814, then 0x80122920 at 0x80063874) and goes to state 7 (0x800638B0).
- 0x8006255C sends 53 only to victims whose +159 is 1 (0x80063660-7C). Every other victim gets 230 (0x80063694). For zombies, 230 goes to 0x800638EC (0x8003BD70-90), which has no room gate.
- Death is event 8: 0x80061C90 and 0x800621B8, then the zombie's 0x80038E54 or the player's 0x80059F64.
- 0x800637B8's gate has no "-1 passes" exemption (0x8006383C-44 is a plain bne), so a victim room of -1 is refused. D1's rule gives the same answer there.

**P3. Effects made inside the bracket are tagged room B and freed on their first tick. Sure on the code. Low: no crash, but no blood or sparks for player 2's room.**
- 0x801432BC stamps the effect's +4900 with r13-23276 (0x80143304). Inside the bracket that is T, as the design wants.
- The effects' own ticks then run in the frame, where the word is room A. 0x8014D768 (0x8014D780-8C) and 0x80149388 (0x8014939C-A8) compare +4900 against r13-23276 and call 0x8014334C (unlink) when they differ.
- No crash: 0x8006255C throws away the handles (0x80062CF4, 0x80062EBC, 0x80063050).
- The design's "blood lands on the victim's room" is true of the stamp, not of what the player sees. Player 2's turn already behaves this way. Leave it, or list it under "Left as is".

**P4. A host check inside the bracket reads the wrong rooms. Sure. Low.**
- coop_sound_gain (hle.c ~14131) tests coop_stay_apart(). That compares player 2's room with r13-23276, which is B inside the bracket, so it answers "not apart".
- So a track started inside the bracket (lr 0x8019F230: the hit, scream and death sounds of room B) has its start gain measured from player 1's view. The per-tick update in the frame corrects it.
- **Fix:** use coop_rooms_apart there.
- Same warning for D1 itself: the "apart" test must be coop_rooms_apart (hle.c:13903), and the swap must not go through coop_stay_room_in. That function calls coop_stay_apart and always uses player 2's room (14238-14251). If either is reused, nested hits of another room inside a bracket would not re-bracket.

**P5. Neither D1 nor D2 covers a grabbed player. Sure. Low.**
- A grabbed player's node is type 19. Its case 58 (0x80061224) forwards a 39 that its case 39 (0x80061888/0x80061B50) takes in 0x80060188, which has no gate.
- So a room-B enemy's 58 can still cost a grabbed player 1 sanity across rooms.
- The fix, if wanted later, is the room test on 0x800CA4E4 itself (the readers' option 6).

**P6. Hits with no attacker on a room-B victim now land (A == -1 gives T = V). Intended. Note only.**
- For example the hazard touch: attacker 0 (0x801451BC), from -1 (0x80145130). Its collision is the object's own room.
- Today the gate (0x800625A4) refuses these. With R6, player 2 apart takes their own room's hazards. coop_hazard_touch already throttles player 2.
- If the victim's room is -1 and the attacker is in B, the bracket only changes the sound and effect tags. The game passes the gate either way.

## Checked and sound
- **The sends inside are synchronous.** 0x801F59B0 stamps the message time with "now", the frame counter r13-22216 (0x801F5350 at 0x801F59DC; stfs f1 to message +20). 0x801F5000 queues only when that time is later than now (0x801F50A0-B0).
  - Every send in 0x80061B9C and 0x80061E90 is 0x801F59B0: 8, 11, 201, 53, 8, 39, 9 (0x80061C90, 0x80061F9C, 0x800620C0, 0x8006215C, 0x800621B8, 0x80062264, 0x800622B8).
  - In 0x8006255C all are synchronous (86, 230, 203, 53/230, 30, 246, 39 at 0x80063764 among them) except one: 55 at 0x80063510, posted with delay = block +108. Its handler 0x80063B50 reads no room. The death's delayed 17 (0x800391BC) goes to 0x8003DD40, which reads no room either.
- **Refusals match the game's own.** Before each gate the functions call only pure getters:
  - 0x8006255C: 0x8019B72C, 0x801F5F88, 0x801F662C, 0x8019B75C. The refusal at 0x80063798 returns r23 = 0 (set at 0x80062580).
  - 0x80061B9C: 0x801F53AC, 0x801F662C. The refusal at 0x80061E70 writes *answer = 0 and returns r3 = the victim's room.
  - 0x800637B8: 0x801F53AC, 0x801F633C, 0x80118274 (a copy), 0x801F62C8, 0x8019B724, 0x8019B75C. The refusal writes *answer = 0.
  - Setting the word to the attacker's room therefore reaches the same branch with the same results as the game's refusal.
- **Victim and attacker as the design reads them.** The victim is the one each gate tests: block +152 (0x8019B72C) in 0x8006255C, and the r3 node in the other two. 0x8019B734 reads +156. 0x801F662C is object +592 (0x80118CAC), or -1 with no node or object. 0x801F5F88(0) returns 0.
- **Room-word and slot reads inside.** The bl closure of 0x8006255C reads the room word only as sound tags (0x80030454, 0x801A0CC0), the node count 0x801F9FF8, effect stamps (0x80143304, 0x801D6D58), 0x801D1364, the sight ray's shape filter 0x8013C958 and spawn stamps (0x80040ED8, 0x800071D8). All of them should be the victim's room.
  - No current-slot function is reached (0x801551A0, 0x80155234, 0x80155130, 0x80155038's group, 0x801104D4, 0x80020558), from any of 0x8006255C, 0x80061B9C, 0x80061E90, 0x800637B8, 0x80038E54 or 0x80122920.
  - 0x80125A70 calls 0x80155544, which tests only "resident and loaded, or not" (2/3 or -1). The swap doesn't change that answer.
  - 0x800323A8 calls 0x80123F70, which goes by +592 except for r13-26524's object (player 1's). Player 1 is never the seer inside a room-B bracket.
  - The two hits under 0x80122920 (0x801576B0, 0x80151CE4) and 0x80151500 are other tables (0x8058AF60, 0x80588DF0), not the slot table.
  - The swap restores all three words exactly, as the turn does.
  - Not verified: bctrl targets (move-key callbacks), and the window while player 1's room change is in progress.
- **D5 is sound.** I went through all 36 sites that send 59.
  - Senders that ask themselves (0x800437D4, 0x800946F0, 0x800B8678/BC, 0x80165728) are in their own room.
  - Senders with from = 0 or -1 have no node: 0x8014A214, 0x8014A894, 0x8014AF94, 0x801785D8, 0x80183620, 0x800C906C, 0x8008C3F0, 0x8014512C.
  - Senders that are the player (0x800437A8, 0x80009B20, 0x800DE3B8, the TARGET verb 0x800BC71C) are already answered 0 between the two players (hle.c 20975-20985).
  - Everything else is an enemy, kind or spell asking about a target, where refusing across rooms is the intent.
  - Every 59 is a synchronous 0x801F59B0 call, and the answer is read in r4, which coop_send_event already zeroes.
  - I found no script, door or hazard that asks 59 of a player across rooms on purpose. The one caveat (low): a node whose +592 isn't the room it stands in, such as something spawned in the frame stamped with r13-23276, would lose its targets.
- **Other 58 receivers.** Type 4 (0x8003B9C8), type 18 (0x80060678) and type 19 (0x80061224) each turn the 58 into a 39. No sender reads an answer: 0x800CA4E4's 58 is a delayed post. So dropping one breaks nothing beyond the leak in P1.
- **Together nothing changes.** D1, D2 and D5 all begin with the apart test.
- **Re-entry without a flag works.** The nested call computes the same T, finds it equal to the word, and passes through at step 4.

Scratch files: C:\Users\stevi\AppData\Local\Temp\claude\9282620f7b4851ea\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\closure3.py (the closure scan), C:\Users\stevi\AppData\Local\Temp\claude\9282620f7b4851ea\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\s59b.txt (the 59 send sites), C:\Users\stevi\AppData\Local\Temp\claude\9282620f7b4851ea\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\f255c.txt and C:\Users\stevi\AppData\Local\Temp\claude\9282620f7b4851ea\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\f1b9c.txt (disassembly of 0x8006255C and 0x80061B9C).

######## refute:together
REFUTER REPORT (together lens), design-r6.md against the code. Read only: no file changed, nothing run or built.

## Verdict
D1 (the hit's room) can't change a together game, even if its apart test were wrong. D4's together branch stays unchanged only if the new code is gated as noted in problem 3. D3 as written is not gated on apart, so the together game is not literally unchanged. D2 and D5 need explicit gates. No crash or hang found on the together paths.

## Problems

**1. D3 is not gated on apart. It changes the together game (sure; small effect).**
- What it adds: the design adds "enemy room == player 2's room" to coop_sighting's swap with no coop_stay() or apart test.
- Today's code: coop_sighting (hle.c:21469-21481) checks mod_coop, g_coop_p2, g_coop_p2_state and stats+20 == player 2's id, and nothing else.
- When it bites, together: an enemy of a room the players left together still has +20 = player 2. This happens with ED_ROOM_SLOTS=2 (the neighbour room stays resident), or with nodes from other rooms that keep ticking. It lasts up to 63 frames, until the quarry clears it (0x80032AAC-B4).
- What changes: today the look runs as player 2. With D3 it runs as the game's own.
- Effect: almost none. 0x800CA370 is already blocked by the enemy's "has seen" bit (bit 8, 0x800CA3C4), and acquisition needs stats+20 == 0 (0x8003B24C). But it is not "unchanged".
- Fix: add the new test only when `g_settings.mod_coop && coop_stay() && coop_rooms_apart(cpu)`.

**2. D2 and D5 don't name mod_coop or the R4 bracket. A solo game can take them (sure, under ED_COOP_STAY).**
- With mod_coop off, a co-op save keeps player 2 parked (coop_park, hle.c:7200) and g_coop_p2 stays set.
- coop_pad_read returns before coop_follow when mod_coop is off (26305-26308), so the parked body's +592 stays in the old room after player 1's door.
- coop_rooms_apart (13903-13906) then answers "apart" in a solo game.
- D2 at coop_events would then refuse cross-room 58s to player 1. coop_events itself has no mod_coop test at its entry.
- D5 would answer 0 to 59s sent to the parked body. That one is harmless.
- D3 and D4 are covered by their functions' existing mod_coop tests (21472, 24062). D1 names mod_coop and g_stay_bracket itself.
- Fix: open every D with `g_settings.mod_coop && coop_stay() && g_stay_bracket == 0 && coop_rooms_apart(cpu)`. Also place D2 before coop_seen_shared (coop_events entry, 21624). Otherwise a 58 that D2 refuses is still shared out to the other player.

**3. D4: the together branch stays unchanged only if "apart" is tested on its own (likely risk at build time).**
- The current gate (24071-24073) returns when the three rooms aren't all equal. That return also covers "players together, enemy in another room".
- If the new drop code goes straight into that return branch, without its own coop_stay() && coop_rooms_apart test, it fires in a together N=2 game. Enemies of the room the players left would drop their target at once, instead of chasing a point for up to 63 frames.
- Fix: inside the return branch, run the drop only when `coop_stay() && coop_rooms_apart(cpu)`. Otherwise return exactly as today.
- Keep the together order as it is today: node walk, health log, coop_seen_later, `after` test, distance/held choice, then the +20 and +12 writes.
- Writing +20 := 0 is safe: 0x801F5F88(0) returns 0 (0x801F5FBC-C0, then 0x801F5FE8), so the locked test fails and clears bit 0.

**4. coop_player1_node's fallback can be stale or 0 inside who-swaps (sure about the code; low effect).**
- coop_player1_node (13898-13901) falls back to g_coop_p1_node whenever r13-21492 == g_coop_p2.
- g_coop_p1_node is set only by coop_be_player2 (12305). It is never reset at a load or an area change.
- These swaps do not refresh it: the turn's own swap (20603-20611 keeps g_coop_turn_p1 instead), coop_as_player2 (21372), coop_seen_hit (22011), coop_hazard_touch (21035) and coop_character_step (24149).
- Together: no harm. D1 still passes through at step 4, because both rooms equal r13-23276. D2 and D5 also need the sender's room to differ from the player's.
- Apart, while the value is still 0 (before the first coop_be_player2): coop_rooms_apart says "not apart" inside those swaps. So a hazard touch or shared sanity hit on player 2 falls to the game's gate and is refused, as it is today.
- Fix: set g_coop_p1_node = was[1] in the turn (20610) and in those swaps. Or have coop_player1_node use g_coop_turn_p1 while in the turn.

**5. D1 step 5: +0x8140 is a halfword (sure; breaks apart only).**
- 0x80123FD4 reads it with `lha` (offset -32448 = 0x8140); +0x8142 and +0x8143 are bytes.
- The host reads the same thing as one word (room_slots_frame 13608-13613: room = (s16)(w >> 16)).
- If the code compares mem_read32(+0x8140) == T, it never matches and D1 never brackets.
- Fix: compare (s16)(w >> 16) == T, and test byte (w >> 8) and byte w for non-zero.

## Windows where a together game reaches the new paths
1. **The follow lag under ED_COOP_STAY when player 2's room isn't resident** (player 2 gets brought along).
   - The window runs from the mover's +592 stamp in the change to coop_follow.
   - coop_follow only runs from coop_pad_read with no menu and no cutscene (4205, 26324). So the window can last a whole arrival cutscene, a menu or the Tome.
   - D1 passes through: T == r13-23276 for the new room's hits, and step 5 (old room not resident) for the rest. Hits are the same as today.
   - D4, D5 and D2 can only act on targets that a script (event 101, the other writers of 0x801F654C) points at player 2. Practically inert.
   - Optional fix: also require room_slot_of() >= 0 for both players' rooms in the "apart" test.
2. **Story room changes under ED_COOP_STAY.** A scripted room change of player 1's leaves player 2 behind (R2: coop_follow 21182, when their room is resident). That makes the players apart on purpose, so R6 applies by design. The user should know a cutscene's door splits them.
3. **ED_COOP_STAY unset, or solo with g_coop_p2 == 0.** coop_stay() is false (13793-13798), or coop_object_room(0) returns -1, so everything is skipped. The exceptions are D3 as written (problem 1) and the parked body with mod_coop off (problem 2).
4. **Can r13-23276 differ from the shared room while the players are together?** Only inside the ticket runner of a change. With coop_stay() false, D1 is off then. With coop_stay() true, the other player stays and is apart, except in window 1. coop_stay_room_in never fires together: coop_stay_apart (13811-13816) needs player 2's room to differ from r13-23276. R4's bracket is excluded by g_stay_bracket.

## Checked and sound
- **The three gates.** 0x800625A4 (cmpw with r13-23276, then a -1 test, exit 0x80063798), 0x80061BD0 (same shape, exit 0x80061E70) and 0x8006383C (no -1 exception, exit 0x800638CC).
- **The sends inside 0x8006255C.** 39 at 0x80063764 goes through 0x801F59B0, synchronously. The 53 at 0x8006367C is sent only when actor+159 == 1 (0x80063660-68); the design states it as unconditional, which is minor.
- **D1 together.** Step 1 is false while together. Even if it were true, step 4 passes, because the attacker's and victim's rooms both equal r13-23276. Nested hits re-enter and pass at step 4. Stale cross-room hits still go to the game's own refusal.
- **R4 window and solo.** g_stay_bracket excludes R4's window, and solo or no player 2 gives -1, so nothing runs.
- **D4.** The together branch (24061-24120) is untouched if it is gated as in problem 3.
- **D5's placement.** The existing 59 branches at 20975-21001 have the same shape D5 would use.
- **D2's source.** The 58 is posted at 0x800CA6A8 (0x801F57C0, delayed post), so it is delivered from the queue to coop_events, as the design says.

Files: E:/Eternal Darkness Recomp Fork/docs/research/separate-rooms/design-r6.md, E:/Eternal Darkness Recomp Fork/runtime/host/hle.c

######## refute:host
REFUTER REPORT, host mechanics lens (D1, plus the host side of D2/D5). Read only. hle.c means runtime/host/hle.c.

## Problems (most severe first)

**P1. The design never says to reset the dispatch flags after the nested call. If they are left as is, the function runs a second time without the bracket and the answer is overwritten. Sure.**
- Evidence: hle_dispatch sets `g_hle_passthrough=false` and then calls the intercept (5551-5560). When the passthrough flag comes back true, the recompiled function runs natively after the intercept.
- D1's nested call re-enters the same intercept. Step 4 passes through, which sets `g_hle_passthrough=true`, and the global keeps whatever the last nested intercept set.
- What breaks: the outer call returns, and the same function then runs again with the restored (player 1's) word. The gate refuses it, but the exits write the answer anyway:
  - 0x80061E70-78 does `stw r30,0(r28)` with r30 = 0.
  - 0x800638CC-D4 does `stw r31,0(r30)`.
  - So the answer of a hit that landed is overwritten with 0, and 0x8006255C's r3 becomes r23 = 0 (0x80063798).
- Fix: imitate coop_as_player2 (21368-21385) or hle_draw_characters (6410-6432):
  - set `g_hle_passthrough = true` first, as coop_hazard_touch does at 21015, so every early return passes through;
  - after `hle_guest_call(cpu, fn, args, 8, cpu->fpr[1], &result)`, set `cpu->gpr[3] = result; g_hle_passthrough = false; g_hle_tail_call = false;`. The comment at 6431 says why: "the nested dispatch passed itself through".

**P2. coop_stay_room_in cannot be reused. Sure.**
- It returns false unless coop_stay_apart() (14239). coop_stay_apart compares player 2's room with the room word (13811-13816), so it is false inside player 2's turn, which is exactly where a hit with T = A needs the bracket.
- It also always writes player 2's room (14242).
- Fix: factor its body into `coop_room_in(cpu, room, keep[3])` and make it the one way the room word and slot table are set:
  - keep[] read from live memory (14243-14245);
  - room word := T;
  - `if (+0 != slot) { +0 := slot; +4 := old +0 }` (14246-14250);
  - restore with coop_stay_room_out (14253-14257).
- Gate it on coop_rooms_apart (13903, node based), not coop_stay_apart.
- Side effect (low): inside a bracket with the room word = B, every host hook keyed on coop_stay_apart() (R5 sound and music) sees "together". That is the same as inside the turn today.

**P3. The new intercept is missing from the Tome copy's stand-aside list. Fairly sure.**
- tome_copy_hook_off (26077-26098) disables co-op hooks during the copy's turn ("the game is alone"). coop_events, coop_sighting, coop_send_event and hle_find_by_id are on that list.
- Without it, D1 would bracket rooms inside the copy's turn.
- Fix: add `OFF(coop_hit_room)`, or whatever the new handler is called, to kOff.

**P4. There is no depth limit on nested guest calls. Sure on the code; the stack budget is an inference.**
- hle_guest_call (main.c 57-110) has no depth counter, only a 50M-block budget per level. Each level lowers the guest stack pointer by 0x800 (main.c 79), plus the game's frames (0x8006255C's frame is 464 bytes).
- Normal depth: coop_events (1) → 0x8006255C bracket (2) → a hit on a node in another room, e.g. the protector's 11 from 0x80061F48-9C (3). That is about 2.5 KB per level.
- Main stack: the top is 0x80634550 (set at 0x8000541C), and r2 = 0x80628460 sits below it, so the stack is at most about 48 KB [inf].
- The recursion does stop: on re-entry V and A are worked out the same way, and the room word is now T, so step 4 passes through. No re-entry flag is needed, and a boolean flag would wrongly block a nested hit that needs its own bracket.
- Smallest fix: a static depth counter. Pass through above about 4 and log it.

**P5. The record check must read +0x8140 as a halfword. Sure.**
- 0x80123F70 reads it with `lha` at 0x80123FD4 (-32448 = +0x8140) and `lbz` at 0x80123FBC/FC8 (+0x8142, +0x8143).
- "+0x8140 == T" written as a 32-bit compare never matches.
- Imitate room_slots_frame 13611-13613: `w = mem_read32(p+0x8140); room = (s32)(s16)(w>>16); loaded = (w>>8)&0xFF; b43 = w&0xFF`, with room_slot_record (13491-13496).
- The check is needed: room_slot_of (13803-13809) reads the host's g_room_slot_room, which is only refreshed when a slot is loaded (13614-13617) and can be stale.

**P6. A victim with no node (V = -1) gets bracketed. Low.**
- When V = -1 and A = B, T = B, so the bracket runs. 0x8006255C's gate lets V = -1 through whatever the room word is (0x800625B4-B8), so the only change is the room of the effects of a hit on nothing.
- Note that 0x800637B8 has no -1 exception (0x80063840-44: bne only).
- Fix: pass through when V == -1.

**P7. Player 1's own room change is not excluded. Low, inference.**
- Step 1 excludes only the R4 bracket. The readers noted that no flag exists for "a change of player 1's is running".
- The bracket is restored afterwards (last in, first out), and room_slot_of = -1 while a slot is being unpacked makes it pass through, so I found no break.
- If such a flag is ever added, test it here too.

## Checked and sound
- **Not already hooked.** None of 0x8006255C, 0x80061B9C or 0x800637B8 has an intercept or notify:
  - grep of hle.c finds none, and every 0x8006xxxx address in the host was listed;
  - `ed fn` shows "host code" empty for all three;
  - ED_HURT_LOG hooks only 0x80061E90, 0x801CB580 and 0x801F57C0 (27270-27286);
  - no `b`/`bc` branches to them and no other references; all three start with stwu.
- **Install** in find_coop next to coop_hazard_touch (27262-27268). That runs before publish_codegen_options (27757; chain stops are read once, comment at 5643-5646). Same checks: `intercept == NULL && notify == NULL`, plus code words:
  - 0x8006255C = 0x9421FE30, with +0x48 (0x800625A4) = 0x800DA514 (`lwz r0,-23276(r13)`);
  - 0x80061B9C = 0x9421FFD0, with +0x34 = 0x800DA514;
  - 0x800637B8 = 0x9421FFC0, with +0x84 = 0x800DA514.
- **Arguments and return.**
  - 0x8006255C takes the block in r3 (0x80059C64-68, 0x8003BA90-94) and returns r23 in r3 (0x80063798).
  - The other two take (node r3, message r4, answer* r5) and write *answer at exit; r3 holds nothing meaningful there.
  - I checked the 2 instructions after every bl site (39 + 22 + 31): none reads r4 or f1 after the call. There are no stack arguments.
  - hle_guest_call restores the whole CPUState, so r3 must be set from `result` (P1).
- **Nested calls.** A guest call inside another guest call is safe.
  - Inside coop_events' call (21658), the 53 and 39 sent to player 2 find `g_coop_events_own` set and pass through (21627, 21676-21680), so they run in player 2's words, which are already in.
  - coop_send_event (the 0x801F59B0 sends) passes them through (21000).
  - The bracket's restore order inside the turn is correct when keep[] is read live at entry.
- **The cross-room "T = A" bracket has no side effects before the gate refuses.** The code before the gate only reads:
  - 0x8006255C up to 0x800625B8;
  - 0x80061B9C up to 0x80061BE0;
  - 0x800637B8, whose only write is to its own stack (0x80063800).
- **Lookups.**
  - Node by id: 0x801F5F88 (disasm 509675-509705) returns r13-21492 for id == r13-21496, 0 for id 0, otherwise walks the r13-21468 list by the +4 links, comparing +16. The host's coop_next (6745-6749) walks the same list. Imitate 24516-24518 and add the r13-21496 → r13-21492 case.
  - 0x801F5F88 is also intercepted (coop_find_node 22365 → hle_find_by_id 24895), but only for bar ids and for return addresses inside its own handling, so a victim lookup (lr 0x80062590) is unaffected.
  - During player 2's Tome, r13-21496 is player 2's id while r13-21492 stays player 1's node (24412-24418). The game then finds player 1's node for player 2's id, so a hit on player 2 there is refused. That is harmless, and the host must make the same special case so its V matches the gate's.
  - coop_object_room (13799-13802) matches 0x801F662C → 0x80118CAC (`lwz +592`).
- **D5:** imitate 20982-20984 (r3 = r4 = 0). 0x801F59B0 returns r4 too (0x80063768 reads it).
- **D2:** put the test in coop_events after the `again` read and reset (21623-21624) and before both branches, for player 1's node too. Answer with `gpr[3]=0` and passthrough false; the game's own "not handled" path is `li r3,0` at 0x801F52CC and writes no answer.
- **Cost:** low. These run once per hit message (11/39/53), not every tick, and are chain stops only then. Do the early-outs first (mod_coop, coop_stay(), g_stay_bracket, sda_ready, coop_rooms_apart) and walk the list only when the players are apart. For the 120-frame log, imitate room_slots_frame's g_stay_log at 13622.
