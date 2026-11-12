# R6c readers (2026-11-11 night): enemy behaviours of player 2's room, and the other enemy kinds

Read only. Facts carry EVIDENCE; [inf] marks inferences.

######## read:bracket
READER REPORT: bracketing a whole enemy behaviour call. Read only, HEAD 2886af7. Nothing was changed, run or built. hle.c = runtime/host/hle.c. [inf] marks an inference. I wrote one scratch scanner, not a project file: C:\Users\stevi\AppData\Local\Temp\claude\9282620f7b4851ea\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\clos4.py (bl closure; flags room-word, player, slot and sound reads).

## (1) Signatures and how often each handler is called
- **Type table.** Entries are registered by 0x801F5A9C(fn, type): it stores fn at 0x806068A8 + type*16.
  - type 4 = 0x8003B470 (0x80030C84 li r4,4; 0x80030C88 addi r3,r3,-19344)
  - type 37 = 0x8003E468 (0x80030C98 / 0x80030C9C)
  - **the spawner 0x8002FC94 is type 78** (0x80031148 li r4,78; 0x8003114C = 0x3863FC94)
  - type 18 = 0x80060370 (0x80030E00 / 0x80030E04)
  - type 19 = 0x80060EC8 (0x80030E14 / 0x80030E18)
- **Signature: handler(node r3, state r4, msg r5, answer* r6) -> r3.** The low byte of r3 is "handled".
  - 0x801F5294 passes r3-r6 through unchanged to the bctrl at 0x801F52F4.
  - The prologues confirm it: 0x8003B470 does r31=r3, r16=r4, r26=r5, r29=r6 and event = 0x801F5384(msg) into r23. 0x8003E468 (r28/r23/r24, event r25), 0x8002FC94 (r29/r26, event r27) and 0x80060370 (r28/r23/r29/r24, event r26) do the same.
  - 0x801F50E4 tests the result with `rlwinm. r0,r3,0,24,31`.
  - Unhandled returns 0: 0x8003E448 `li r3,0`.
- **How each delivery calls it.** Each 0x801F5000(msg) delivery calls 0x801F5294:
  - once with state = node+20 (0x801F50D0-E0; skipped when msg+12 == 0);
  - again with state 0, when the event is not handled and is not 3 (0x801F50EC-0x801F510C);
  - for the flush: event 2 to the old state, event 1 to the new one (0x801F519C, 0x801F51C4), and a type change at 0x801F5230, plus event 1 by 0x801F59B0 (0x801F525C).
  - So a handler-entry intercept sees each of these as a separate, non-nested call.
- **Per frame and per node.** The holder step 0x800276C4 calls 0x801F5D48 (0x80027784), which sends event 3, then event 4, both synchronously (0x801F5D60-0x801F5D80). It is skipped while busy (0x80155738) for held nodes (0x801F68D4).
  - That gives 1 call for event 3 and 2 calls for event 4 (state N, then state 0, both unhandled).
  - **No event 4 case exists in 4, 37, 78 or 18.** There is no `cmpwi rEvent,4` in 0x8003B470-0x8003E464, 0x8003E468-0x8003E934, 0x8002FC94-0x800302D4 or 0x80060370-0x80060EC4.
  - So the intercept can pass event 4 through and pay 1 guest call per node per frame.
- **The sight runs on every event 3, whatever the state.** 0x8003B528 `cmpwi r23,3` leads to bl 0x8003B1C8 at 0x8003B554, before the state chain (state 0 at 0x8003B558).

## (2) Event-3 ticks and the states, with room B current
- **State chain of 0x8003B470** (`cmpwi r16,N`): 0 B558, 1 BEAC, 21 C060, 3 C550, 6 C5E0, 7 C7C8, 47 C844, 95 CA74, 33 CB1C, 32 D08C, 52 D1DC, 53 D588, 55 D6F8, 9 D7EC, 31 DBB0, 8 DFA8. The function ends at 0x8003E464.
- **What the 29 sites do once B is current** (numbers as in enemy-readers-2 section 2):
  - **WAITs end:**
    - 1: the spawner resumes and spawns. It needs its own key; see "Spawner key" below.
    - 24 and 28: the rise happens.
  - **DESTROYs become the in-play path:**
    - 7 and 22: the death plays (move 24, health 0, event 17 posted).
    - 15: a placed-dead node (bit 0x2) gets 8 instead of 57.
  - **Become right:** the room constants (6, 8, 10, 11, 17-20; e.g. 19 gives room 183's 0.02 speed), the sound tags (9, 13, 14, 16, 21, 26: tagged B), the counts (23, 27: B's characters), 12 (event 30's effects run) and 25 (start move 79 plays).
  - **Not in these handlers, so unchanged:** 2 (r6_hit_room already covers it), 3, 4 and 5.
- **Room-word reads outside 0x8003xxxx that the bracket also fixes:**
  - **State 6, 0x8006F69C(pos,out,400,4) at 0x8003C630/0x8003C680.** It uses the current slot's points (0x80155234(2) at 0x8006F6CC), a ray in the current slot (0x801551A0(2) at 0x8006F7C8, then 0x801390F0) and a node scan by the room word (0x8006F9B0). Its result is the target of state 44 (0x8003C63C-0x8003C668).
    - Today a B zombie picks this point from room A's slot. With B current it uses B's [inf: a wander/flee point].
    - Its ray's blocker filter 0x8013C93C (word at 0x8013C958) becomes B's too.
  - **State 0, event 14, 0x800659F8 (0x8003BBC4).** It makes a hit block only if msg data & 0x7FFFFFFF == the room word (0x80065A40-0x80065A50).
  - **State 0, event 234, 0x80064548 (0x8003BE10):** a room-41 constant (0x80064634).
  - **State 53, event 116, 0x80045658 (0x8003D640):** a scan of the in-play room's objects (0x80045710) [inf: separation].
  - **0x800648BC** (from the sight, state 0 event 1, and 0x8003E500): registers a node with actor+159 == 3 and +148 == 2 in the 3-entry table 0x80316978+248, only if its room == the word (0x8006490C). B's nodes of that kind would then register too. The table is shared [inf: low risk].
  - **Sound tags:** 0x800CD4B0 (state 8 event 17, 0x800CD590), 0x8019DA18 (0x8003C25C, 0x8003DC08), 0x801A0C90 (0x8003D718).
- **The route post 0x800BBCC8** tests 0x80155130(2), the current slot's record (0x800BBCF0). With B current it is B's record and the job is posted. The stamp at 0x800BAF08 has no reader. The route's graph is R6b's, on the job thread. Fine.
- **The sight with coop_sighting nested inside: still works, and better.**
  - None of coop_sighting's conditions read the room word. r6_apart uses +592 and g_room_slot_room (hle.c 24119-24124, 13810-13816).
  - coop_as_player2 swaps only the who words (21392-21409), so B stays current inside it.
  - Better: 0x80123F70 (by 0x800324B8 -> 0x800323A8 at 0x800323D4) takes the current slot for r13-26524's object (0x80123F94-0x80123FA0, leading to 0x80124010). That object is player 2 inside coop_sighting. Today that is A's slot; with the bracket it is B's.
- **The attack start 0x80039840** stamps hit block +198 := the word (0x8019B6AC). It becomes B. Only the debug dump 0x8019B2B0 reads it: no effect.
  - The swing itself lands from the step's animation keys, outside this call (enemy-readers-2, player-hits section 2). The bracket does not cover the swing's blocker filter 0x8013CBEC.
- **Walk and collision: the same either way.** For a non-player object, 0x80123F70 loops over indices 2..3 (0x80123FA4-0x80123FFC) and takes the slot whose record room == +592. coop_room_in swaps the indices but keeps both slots, so it finds the same one.
- **Sounds, with R5:**
  - B's enemy tracks get +132 = B. coop_sound_fade_rooms keeps or fades by +132 (hle.c ~14042), so they now count as player 2's room.
  - Snag: coop_sound_gain tests coop_stay_apart, which reads r13-23276 (13818-13823). Inside a B bracket it is false, so the gain at a track's start (lr 0x8019F230, inside the handler) is measured from player 1's eye. The per-tick update (lr 0x801A0310) runs in the frame and uses player 2's [inf].
  - Fix: use r6_apart there, or a host flag set during the bracket.
- **Reads of "the player" that stay player 1 (A) inside the bracket:**
  - the quarry 0x800324B8: P1ID at 0x80032568, the player-only candidate. coop_sighting covers it for B's nodes after player 2 or nobody.
  - the sight: 0x8003B260, 0x8003B3C0, 0x800CA3A8, 0x800CA504. coop_sighting covers these the same way.
  - **event 203, 0x8003A8AC (0x8003B8A0):** event 39 with sanity 50 sent to r13-21496 (0x8003A9A8-0x8003A9F4). With the bracket, r6_hit_room refuses it: the attacker is in B, the victim in A. So no one is hit; it should go to player 2 or the striker.
  - state 0, event 59 (0x8003BCAC): the answer is 1 when the asker is r13-21496.
  - event 17, 0x80038B80: in chapter 5, if the player's state is 30, the 17 is re-posted (0x80038BA4-0x80038C1C).
  - 0x800C90E8: the "main" r13-21484 special at 0x800C9114/0x800C9260.
  - 0x800CA6F4 (state 9/8 event 1): P1ID at 0x800CA710. Not read further.
  - **Recommendation: no who swap in the bracket.** The sight is the only per-tick reader, and coop_sighting covers it.

## (3) The delayed events go through the same bctrl: confirmed
- 0x801F58AC (event 24 at 0x8003DA54, event 17 at 0x800391BC) builds the message and calls 0x801F5000 (0x801F591C).
- With a future time, 0x801F5000 queues it (0x801F509C-0x801F50B0, bl 0x801F5310).
- The pump 0x801F53E8 (main loop, 0x800084D4) delivers it by bl 0x801F5000 at 0x801F54EC. That leads to 0x801F5294 and the bctrl.
- 0x801F57C0 does the same (0x801F581C). So a handler-entry intercept covers them, in the frame with room A current.
- The pump has its own room test, for event 75 only (0x801F54B8).

## (4) Hazards
- **DESTROY sites: correct with B current.**
  - 0x8003B580: a placed-dead B node gets 8 and dies, as it would in play. Its event 1 arrives synchronously at creation, inside its creator's bracket (a spawner bracket included).
  - 0x80038ED4 and 0x8003CBE4: the death plays, then event 17 (state 8, 0x8003E0A4-0x8003E174) changes the type to 0. No room test there; only the 0x800CD4B0 sound tag.
- **Recursion with r6_hit_room: fine.**
  - Inside the bracket, 0x8006255C runs for the node's own 11/48/237 cases (6 bl sites in 0x8003B470). It computes t; when t == the word it passes through.
  - Across rooms it switches to the attacker's room and the game's gate refuses the hit. Conversely, the synchronous 8 inside r6_hit_room reaches the handler with R == the word and passes through.
  - Give the handler bracket its own depth guard, like g_r6_depth's 4. Each hle_guest_call moves the guest r1 down 0x800 and keeps a CPUState copy on the host stack (runtime/host/main.c 61-110). The main thread's stack size was not read.
- **Player 2's events (coop_events, B current):** an event they send to a B node finds R == the word and passes through.
  - The other way round: inside a B bracket, coop_events' coop_stay_room_in returns false (coop_stay_apart reads the word), and they run with B anyway. Correct.
  - An A node reached from inside a B bracket gets A by the same rule; keep[] restores B.
- **The R4 bracket:** g_stay_bracket != 0 makes r6_apart false (24120), so nothing is bracketed during player 2's change. Correct.
- **Events 61/62 at doors.**
  - 0x80152298 calls 0x80154C94 at 0x80152388: r13-23288 := old room, r13-23276 := NEW room (0x80154CD4-0x80154CE0). Then 0x80151FDC at 0x801523A4 broadcasts 61 to r13-23288 (0x801520E4-F4).
  - So when player 1 leaves B, B's enemies get 61 with room != word, in the middle of the change. Bracketing that would swap the slot table mid-change. It must be excluded.
  - It also changes nothing: 61's work reads no room word. State 0 (0x8003B850-84): 0x800BB0C0 / 0x8003A2E8 have no reads, 0x80064A7C only looks up an id, 0x800E8464 sends. State 33: 0x8003A1D0 == 4 leads to 57.
  - 62 to the new room (0x80152780): R == the word, so it passes.
  - **Exclusion test:** coop_camera_at_room_change(cpu) (hle.c 17129-17140) walks the lr and stack for 0x80152298-0x80152814.
  - The strip of the stayer's room by 61 is a separate fix: refuse the 0x801F5620 broadcast at lr 0x801520F8 when r6 is player 2's room.
- **Spawner key.** Its room is info(node+40)+124 -> +28 (0x8002FCCC, 0x8002FD6C), not the node's +592. So key it as mem[mem[node+40]+124]+28.
  - Its spawn-point test 0x8002F5D0 takes the eye from 0x803012E0 (0x801EB01C at 0x8002F620) and ray-tests in 0x801551A0(2) (0x8002F654). With B current: player 1's camera in B's geometry [inf: 0x803012E0 is the camera eye]. B's spawns may ignore player 2's view.
- **Host tests that read the word and flip inside a B bracket:**
  - coop_stay_apart and its users: coop_sound_gain (14142), the draw marks (3301), coop_stay_spared (13836). Use r6_apart, which does not read the word.
- **Interrupts.** R6b masks MSR[EE] for its job thread. The per-frame guest calls already made on the main thread (coop_events for player 2's ticks, coop_sighting) show this works [inf].

## (5) Performance
- Per B node per frame: 1 guest call (event 3), if event 4 passes through; otherwise 3.
- Rare events add calls. The spawner adds 1 per frame.
- Room 183: 4 zombies, so about 4 guest calls per frame plus coop_sighting's existing nested ones.
- Each call costs 3 reads and 3 writes (coop_room_in), 2 CPUState memcpys and the run [inf: tens of µs each].
- Put the stack-walk test (24 frames) after the cheap tests: R != the word, resident, loaded.

## Recommendation
- **Intercept the type entries, not 0x801F5294.** 0x801F5294 is reached for every node of every type, up to 3 times per frame. It would also wrap player 2's type 2 and 19 calls, on top of coop_events and coop_grab_events, and every door, item and script node.
- **Intercept these four, with these install checks:**
  - **type 4, 0x8003B470:** +0 = 0x9421FEA0; +0xE4 = 0x4BFFFC75 (bl 0x8003B1C8 at 0x8003B554); +0x110 = 0x800DA514 (0x8003B580); registration 0x80030C88 = 0x3863B470.
  - **type 37, 0x8003E468:** +0 = 0x9421FF90; +0xB4 = 0x800DA514 (0x8003E51C); +0x2E4 = 0x800DA514 (0x8003E74C); registration 0x80030C9C = 0x3863E468.
  - **type 78 (the spawner), 0x8002FC94:** +0 = 0x9421FF90; +0xD4 = 0x806DA514 (0x8002FD68); +0x4AC = 0x806DA514 (0x80030140); registration 0x8003114C = 0x3863FC94. Key by info+124 -> +28.
  - **type 18 (the grabber), 0x80060370: include it.** +0 = 0x9421FFB0; +0x240 = 0x800DA514 (0x800605B0, sound 497 tag); registration 0x80030E04 = 0x38630370.
    - A grabbing zombie's node type is 18 (0x80039AD4-DC), so its ticks leave 0x8003B470.
    - Its room-word reads would otherwise stay A's: the sound tag, 0x80064548 (0x8006054C), the event-14 gate 0x800659F8 (0x800607D4) and the scan 0x80045658 (0x8006098C, 0x80060C2C). The bites need no room (enemy-readers-2 section 3).
    - Its event 1 back to type 4 goes through type 4's intercept.
- **The rule:**
  - pass event 4;
  - R = coop_object_room(node) (the spawner: its own key);
  - act only if r6_apart, R != -1, R != r13-23276, r6_room_loaded(R), not coop_camera_at_room_change, and depth < 4;
  - then coop_room_in(R), hle_guest_call(entry, r3-r10, f1), coop_stay_room_out, r3 = the result, passthrough off (r6_hit_room's pattern, hle.c 24158-24207).
- **Type 19 needs no new intercept.** It is the player's own held behaviour; coop_grab_events already owns that entry (26816-26820).
  - It has WAITs: events 138/139/140 re-post themselves when the node's room != the word (0x800613C0, 0x800614FC, 0x80061624), plus a sound tag at 0x8006104C.
  - So player 2 held apart waits forever. Add coop_stay_room_in/out round coop_grab_events' guest call (21985) for player 2's node.

## Open
- Whether the spawner node has node+52 (for coop_object_room).
- What 0x800CA6F4 (P1ID at 0x800CA710) does.
- Who sends event 14 and what its data's room means.
- The main thread's stack size for nested guest calls.
- Whether interrupts can be raised inside hle_guest_call (main.c 93-99 breaks the call on a new exception).

######## read:kinds
READER REPORT: enemy kinds and their looks. Read only. Nothing was changed, run or built. hle.c = runtime/host/hle.c. Every fact has evidence. [inf] marks an inference.

## (1) The node-type table built at 0x80030C28
- Each entry is `lis r3,hi; li r4,TYPE; addi r3,r3,lo; addi r5,r13,-30728; bl 0x801F5A9C` (decoded from `ed dis 0x80030C28 400`).
- There are 75 entries, written as type:function, with the bl site of the first entry:
  - 0:0 (0x80030C40, li r3,0, no handler)
  - 2:80058DD8, 5:8005E4E0, 8:80077324, 4:8003B470, 37:8003E468, 31:8006757C, 7:800721C0, 30:800D8634, 91:800DAD74, 29:801F759C
  - 10:80029168, 11:80029454, 20:80029D40, 22:8002A05C, 23:8002A188, 24:8002A78C, 87:8002AEF4, 25:8002A348, 26:8002B09C
  - 32:8006CEAC, 34:8002B714, 36:8002C794, 18:80060370, 19:80060EC8, 21:80029E28, 28:800661EC, 35:80028928, 66:8002872C, 51:80028E0C
  - 6:800881C8, 40:8002EC90, 41:8003E938, 43:8002EFD8, 9:8008E414, 44:80094530, 45:800913B4, 56:800A5B88, 58:800A8000, 42:800978DC
  - 90:80099DA0, 53:8009F4C4, 62:8002F36C, 39:800E860C, 49:8002B428, 46:800DBC84, 47:800DB7DC, 48:800DCAE8, 50:800A2B94, 82:800A3F44
  - 59:80092070, 61:800841DC, 63:800C6588, 89:800E44E0, 64:80093B94, 65:800DC160, 68:800BC1EC, 69:8009723C, 70:800298A4, 72:800C4E5C
  - 73:8008D938, 74:800E0408, 75:80097554, 76:8002F50C, 77:800283F4, 78:8002FC94, 83:800E7318, 79:800DE86C, 80:800DF634, 81:800DFEC0
  - 84:8006D70C, 85:800E1CE0, 86:80030484, 88:800E3208, 57:800E5AA8 (the last bl is at 0x80031208)
- **The creature class of each chr.** The table is at 0x80319EA0: 0x8006F3D8 computes index*224 + 0x80319EA0, and 0x8006F3EC finds an entry by +4 == chr over 175 entries. It is in .bss, so I read it from saves/*.state; every save holds the same table. +64 is the class and +66 the sub-kind. A node's info +158/+159 repeats them: rooms-r6-base.state shows zombies 2/3 and players 1/1.
  - Class 2 (creatures) by sub-kind:
    - 3: chr 13, 20, 22, 24, 30, 64-67
    - 4: chr 14, 37, 38
    - 5: chr 21, 59, 60
    - 6: chr 50, 51, 52
    - 7: chr 23, 56, 57
    - 10: chr 34, 68, 123
    - 12: chr 69, 70, 71
    - 13: chr 79, 80, 81
    - 16: chr 12
    - 22: chr 28, 82
    - 30: chr 89
    - 38: chr 136
    - 41: chr 132
    - 31/32/33/35/36/37: chr 94/95/96/92/108/109
  - Class 1 is the player characters (chr 1, 26, 27, 33, 44-49, 53-55). Classes 3 and 4 are objects and NPCs (for example chr 83, 84, 122 are 3/15; chr 110-121 are 4/20).

## (2) How each enemy behaviour acquires its target
**What each look function is.**
- **0x800CA370 is the sanity hit on sight, for "the player" only.**
  - It reads r27 = 0x801F62B8 = r13-21496 at 0x800CA3A8.
  - It requires the node's quarry to be the player or nobody (0x800CA3D4-0x800CA40C).
  - It then calls 0x800CA4E4(node, 8, 23, rec+144) at 0x800CA4C4.
  - 0x800CA4E4 reads the player again (0x800CA504), does a ray test (0x801388CC, 0x801390F0), builds a hit block whose victim is the player (0x8019B744 at 0x800CA660), posts event 58 (0x801F57C0 at 0x800CA6A8), then sets a flag (0x800338BC at 0x800CA6BC).
- **0x8003B1C8 (type 4) does more.** It calls 0x800CA370 (0x8003B1F8) and 0x800324B8(node,0) (0x8003B258). Only when that pick equals 0x801F62B8 (0x8003B260-0x8003B274) does it set flag 0x8.
- **0x800324B8 (the quarry pick) does not test for the player by itself.**
  - A candidate must be in the looker's room by +592 (0x800328C8).
  - Its +158 must be 1 or 2 (0x800328E0-F0).
  - If its alignment (+148) equals the looker's, only the player with 158 == 1 and 159 == 1 passes (0x800328F4-0x80032910).
  - If the looker's flag 0x1000 is set, only the player passes (0x8003283C, then 0x8003293C-0x80032950).
  - Then come reach, view, the answer to event 59 == 1, and a ray.
- **Who has flag 0x1000.** No type-4 or type-37 node has it in any save: their flags are 0x401, 0x409 or 0x481, and +148 is 4 against the players' 0. Its only setters are:
  - 0x80031568 (spawn, creature sub-kind 37 = chr 109)
  - 0x8006C534
  - 0x800E0514 (type 74 itself)
  - [inf] So the claim in coop_sighting's comment (hle.c:21499-21501) that a zombie takes only the player because of 0x1000 does not hold for these nodes. What player 2 lost in a zombie's look was the sanity hit and flag 0x8.

**Per behaviour.** Each line gives the look path, then what reads the player in the kind's own code: 0x801F62B8 / 0x801F62B0 / r13-21496 / -21492 / -26524. Paths come from a call graph with bl, b and lis+addi pointer edges, depth 2 (scratchpad words.py and looks2.py).
- **Type 4 (0x8003B470):** 0x8003B1C8 at 0x8003B554. coop_sighting runs this look as player 2.
- **Type 37 (0x8003E468):** 0x800324B8(node,0) at 0x8003E65C and 0x8003E87C, no player words. Rising makes it type 4 (0x8003E7EC/898/8F0).
- **Type 5 (0x8005E4E0):** 0x800CA370 at 0x8005E5D4; the result is not used (r3 is overwritten at 0x8005E5D8). 0x800324B8(node,0) at 0x8005DE24, 0x8005DEB4 and 0x8005E070. There are no player words [inf: so its quarry can be player 2 in its own room today].
- **Type 6 (0x800881C8):** 0x800CA370 at 0x80088270. Then, if 0x80089A78 answers yes, the target is the player: 0x801F62B0 at 0x800882C4, then 0x801F62C8, then 0x80089D24(node, id) at 0x800882F0. Also 0x800324B8(…,0) at 0x8008911C. Player words: 0x800886E8, 0x80088700, 0x80089910, 0x80089AB8, 0x80089B24, 0x80089B78.
- **Type 7 (0x800721C0):** 0x800CA370 at 0x8007226C. 0x800324B8(…,0) at 0x80071A9C, 0x80071DD4, 0x80072910 and 0x80072CAC. Player-id test at 0x80073210.
- **Type 8 (0x80077324):** its look is the separate function 0x80076600, whose one caller is 0x800773DC. It reads the player node (0x80076624), calls 0x800CA370 (0x8007664C), and makes a hit block with victim = 0x801F62B8 (0x8007676C-0x8007678C). Also 0x800324B8(…,0) at 0x800761BC and 0x80076E94.
- **Type 41 (0x8003E938, not placed):** 0x800CA370 at 0x8003E9D8. Player tests at 0x8003EB44 and 0x8003EC48.
- **Type 42 (0x800978DC):** 0x800CA370 at 0x80097980. Player words at 0x80097C80, 0x8009803C, 0x80098300 and 0x80098504.
- **Type 44 (0x80094530):** 0x800CA4E4 called directly at 0x800968D4. Player words at 0x800957E0 (event 6 to the player), 0x80096468, 0x800966B4 and 0x800968A0. 0x800324B8 with 'other' at 0x80094028. It becomes type 45 (0x80093F18, 0x800966CC).
- **Type 53 (0x8009F4C4):** dispatches by vtable (info+100 → +12, bctrl at 0x8009F524). The methods reach 0x800CA370 at 0x800CECA0 (in 0x800CEBA8), 0x800D247C (in 0x800D2390) and 0x800D4C24 (in 0x800D4B38), via 0x800A104C (0x8009F500).
- **Type 63 (0x800C6588, not placed):** player position at 0x800C65C0 and 0x800C6618, then 0x800CA370 at 0x800C664C.
- **Type 89 (0x800E44E0, not placed):** player at 0x800E4518, 0x800E4570 and 0x800E4738; 0x800CA370 at 0x800E45B0.
- **Type 74 (0x800E0408):** 0x800324B8(…,0) at 0x800E0D70, 0x800E1580 and 0x800E1AC8. It sets 0x1000 on itself at 0x800E0514, so only the player passes. Player words at 0x800E0560-0x800E0944 and 0x800E1684.
- **Type 56 (0x800A5B88):** 0x800324B8 with 'other' at 0x800A5814. Player words: 9 sites, 0x800A50EC-0x800A6A1C.
- **Type 59 (0x80092070):** no look. The 'from' of an event is compared with the player id (0x800921A0-0x800921C4).
- **Types 31, 64, 69, 76 and 62:** no look call found within depth 2. Type 31 reads the player at 0x80068250 and type 76 at 0x8002F580.

## (3) Where the kinds are placed (placed_things.json; level = chapter per levels.json)
L1 is Pious, L2 Ellia, L3 Anthony, L4 Karim, L5 Max, L6 Lindsey, L7 Paul, L8 Roberto, L9 Edward, L10 Peter, L11 MAJOR. L0, L13 and L16 are Alex.

| Type | chr | Levels | Rooms where few |
|---|---|---|---|
| 4 (zombies) | 13/20/22 | L2, 3, 4, 6, 7, 8, 10, 11 | |
| 4 (zombies) | 24 | L0, 1, 3, 5, 6, 7, 11 | L0 room 96, L5 room 293 |
| 37 (zombies) | 13/20/22/24 | L1-L11 | L9 only room 276 |
| 5 | 14/37/38 | L4, 6, 7, 8, 9, 10, 11 | |
| 6 | 50/51/52 | L4, 6, 7, 8, 9, 10, 11 | |
| 7 | 21/59/60 | L7, L10 | L7 rooms 110/117; L10 rooms 14/19/345/346 |
| 8 | 23/56/57 | L6, L8, L11 | L6 rooms 144/303, L8 room 203, L11 rooms 34/44/47/48 |
| 42 | 69/70/71 | L9 | 276-282 |
| 44 | 68 | L2, 3, 5, 7, 8, 9, 10 | |
| 44 | 123 | L3 | 152 |
| 69 | 68 | L4, 8, 10 | |
| 64 | 68 | L10 | 27 |
| 53 | 79/80/81 | L10, L16 | 39 |
| 59 | 89 | L10 | 3, 11, 12, 19, 20, 75 |
| 74 | 132 | L7 | 104 |
| 56 | 82 | L13 | 64 |
| 31 | 12 | L1 | 182 |
| 76 | 14/37/38 | L6 | 172 |
| 62 | 136 | L6 | 139, 297, 298, 313 |

- Class-2 chrs 92/94/95/96/108 are placed only as type 29 (0x801F759C, no look) in L2 and L6.
- Chapters with kinds outside 4/37:
  - L4: 5, 6, 69
  - L6: 5, 6, 8, 62, 76
  - L7: 5, 6, 7, 44, 74
  - L8: 5, 6, 8, 44, 69
  - L9: 5, 6, 42, 44
  - L10: 5, 6, 7, 44, 53, 59, 64, 69
  - L11: 5, 6, 8
  - L2, L3, L5: 44
  - L13: 56; L16: 53
  - L0 and L1 have only zombies, plus type 31 in L1 room 182.

## Which would NOT find player 2 apart today
- **The sanity hit on sight (event 58) and its flag are never given to player 2** by types 5, 6, 7, 8, 41, 42, 44, 53, 63 and 89. Their 0x800CA370/0x800CA4E4 runs with r13-21496 = player 1. Only type 4 has that call inside coop_sighting's swap.
- **Their acquisition is player 1 only**, because it is taken from the player words: types 6 (0x800882C4), 8 (0x80076600), 42, 44, 53 [inf], 63, 89, and 74 (0x1000).
- **These pick by 0x800324B8(node,0) with no player words**, so they can find player 2 in their own room today [inf]: types 5 and 37, and mostly 7 (0x80073210 is one player test).

## (4) The smallest host change, following coop_sighting
**Rule.** r6_apart(cpu), coop_object_room(cpu, node) == coop_object_room(cpu, g_coop_p2), node != g_coop_p2, not g_coop_turn. Then `coop_as_player2(cpu, FN)`; otherwise `g_hle_passthrough = true`. No hle.c hook exists today on any address below (grep).

- **A. The sanity hit, all kinds at once.**
  - Intercept 0x800CA370. Pass through when g_coop_sighting is set, so type 4 is not swapped twice.
  - Install check: +0 0x9421FFE0; 0x800CA3A8 == 0x4812BF11 (bl 0x801F62B8); 0x800CA4C4 == 0x48000021 (bl 0x800CA4E4).
  - For type 44's direct call, also intercept 0x800CA4E4: +0 0x9421FF60; 0x800CA504 == 0x4812BDB5; caller 0x800968D4 == 0x48033C11. Pass through when r13-21496 already equals player 2's id, which is the nested call from A.
  - Callers to check: 0x8005E5D4 = 0x4806BD9D, 0x80088270 = 0x48042101, 0x8007226C = 0x48058105, 0x8003E9D8 = 0x4808B999, 0x80097980 = 0x480329F1, 0x800C664C = 0x48003D25, 0x800E45B0 = 0x4BFE5DC1, 0x800CECA0 = 0x4BFFB6D1, 0x800D247C = 0x4BFF7EF5, 0x800D4C24 = 0x4BFF574D.
- **B. Acquisition from the player words.**
  - Only type 8 has a separate look function: intercept 0x80076600 (+0 0x9421FF60; 0x8007664C == 0x48053D25; its caller 0x800773DC == 0x4BFFF225).
  - For types 6, 7, 41, 42, 44, 53, 56, 63, 74 and 89 the words are read inside the handler, so the intercept is the table function itself. Check its +0 word and its table addi word, as coop_grab_events checks 0x80030E18 == 0x38630EC8:

| Type | Function | +0 word | Table addi |
|---|---|---|---|
| 5 | 8005E4E0 | 9421FED0 | 80030C60 = 3863E4E0 |
| 6 | 800881C8 | 9421FF20 | 80030E90 = 386381C8 |
| 7 | 800721C0 | 9421FEF0 | 80030CC4 = 386321C0 |
| 8 | 80077324 | 9421FF80 | 80030C74 = 38637324 |
| 41 | 8003E938 | 9421FFA0 | 80030EB8 = 3863E938 |
| 42 | 800978DC | 9421FEF0 | 80030F44 = 386378DC |
| 44 | 80094530 | 9421FF40 | 80030EF4 = 38634530 |
| 45 | 800913B4 | 9421FFB0 | 80030F08 = 386313B4 |
| 53 | 8009F4C4 | 9421FFE0 | 80030F6C = 3863F4C4 |
| 56 | 800A5B88 | 9421FE90 | 80030F1C = 38635B88 |
| 59 | 80092070 | 9421FF90 | 80031020 = 38632070 |
| 63 | 800C6588 | 9421FF20 | 80031048 = 38636588 |
| 64 | 80093B94 | 9421FFD0 | 80031070 = 38633B94 |
| 69 | 8009723C | 9421FFE0 | 800310AC = 3863723C |
| 74 | 800E0408 | 9421FF90 | 800310FC = 38630408 |
| 76 | 8002F50C | 9421FFE0 | 80031124 = 3863F50C |
| 89 | 800E44E0 | 9421FEF0 | 8003105C = 386344E0 |

  - Wrapping a handler also covers A for that kind.
- **C. One hook for every kind instead of B.** Intercept the dispatcher 0x801F5294: +0 0x9421FFF0, +0xC 0x8005000C, +0x60 0x4E800421. That is enemy-readers-2.md §4. It needs the reverse swap for a node of A delivered nested inside [inf].
- **Hazards.**
  - Inside the swap, a kind's hits aimed at "the player" become player 2's id (for example type 8's block at 0x8007678C). R6's victim-room brackets then take them.
  - Type 74's 0x1000 rule passes player 2 only under the swap.

Files: runtime/host/hle.c (21363-21515, 26801-26813, 24119-24124, 24370-24430); build/ed-db/disasm.txt; build/ed-db/facts/placed_things.json; build/ed-db/facts/levels.json; generated/main.dol (the words); saves/rooms-r6-base.state and the other saves/*.state (creature table, node flags); docs/research/separate-rooms/enemy-readers-2.md.

Scratch scripts: C:\Users\stevi\AppData\Local\Temp\claude\9282620f7b4851ea\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\looks.py, C:\Users\stevi\AppData\Local\Temp\claude\9282620f7b4851ea\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\looks2.py, C:\Users\stevi\AppData\Local\Temp\claude\9282620f7b4851ea\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\words.py.
