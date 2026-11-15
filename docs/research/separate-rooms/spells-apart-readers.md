### end
ANSWER: The spell's own step 0x801C316C ends it. On its first step it fails a room check and frees itself. The test is at 0x801C31B8-0x801C31C4: `lwz r0,8(r29); lwz r6,-23276(r13); cmpw r0,r6; bne 0x801C31DC`. Spell +8 is player 2's room (153). r13-23276 is player 1's room (151) when coop_spells_update steps the spell. So the branch goes to the end path, and that path frees the record with 0x801C5090, which unlinks it from r13-22324. Neither 0x80155450 nor the update loop removes anything.

1. Where spell +8 comes from. The record maker 0x801C4F90 stores +8 := r13-23276 at creation, at 0x801C5060-0x801C5068 [read 0x801C5060]. The same maker:
- sets +4082 := 1 and +4080 := 8, then ORs in 4 [read 0x801C502C-0x801C5044];
- sets +4080 bit 0x10 when the caster id equals the main id (0x801F6258) [read 0x801C5048-0x801C505C]. Under coop_be_player2 the caster is player 2, so this bit is set. That matches the flags "1C" in the log.

Player 2's quick cast (hle.c:22943) runs inside their turn's coop_stay_room_in (hle.c:22798) / coop_stay_room_out (hle.c:22959-22960). So r13-23276 is 153 at creation, and spell +8 = 153 [read hle.c; inferred: the value].

2. The update loop. 0x801C5994 is the address hle.c:13017 names and hle.c:32338-32341 intercepts. It does not remove records. It reads r13-22324 and calls 0x80157364 (paused). If not paused and (+4082 & 5) == 0, it calls 0x80155450(spell +8); a 0 answer only skips the step (0x801C59E8-0x801C59F4). Then it steps +28 due-minus-done times and adds r13-27032 to +0 [read 0x801C5994-0x801C5AA0]. coop_spells_update copies this loop and adds no room swap: coop_be_player2 does not touch r13-23276 [read hle.c:13021-13084, 12548-12569].

3. The 0x80155450(room) test [read 0x80155450-0x80155524]:
- answers 1 if room == -1;
- otherwise loops over the slots. For each slot record (0x80155130) it needs byte -32446 and byte -32445 nonzero; if halfword -32448 == room, it answers 1;
- if the slot count (from 0x8004304C) == 2, it also scans the record's exit list (0x80155284 / 0x80155314, entries of 92 bytes, +40 == room) and answers 1 on a match.

Room 153 is loaded in slot 0 [read there: log line 381], so it answers 1 and the step runs. The list being empty, not frozen, confirms the step ran [inferred].

4. The step's opening check, 0x801C316C [read 0x801C3184-0x801C31D8]. Set-up: r31 = spell +644 (a second record); r28 = &local 0, or &r13-22312 when spell +4080 bit 0x10 is set (0x801C31A0-0x801C31A8). The normal path (0x801C33E4) needs all four of these:
- (a) *r28 == 0, which here means r13-22312 (the cast-cancel word) is 0;
- (b) spell +8 == r13-23276;
- (c) (+644 record)+4080 bit 0 is clear;
- (d) spell +4080 bit 0 is clear.

If any fails, it falls to 0x801C31DC, the end. Apart, (b) fails: 153 != 151. (a) is player 2's own word, swapped in by coop_be_player2, and nothing logged a cancel [read there: page-0001.log has no cast-end line; inferred that it was 0].

5. The end path [read 0x801C31DC-0x801C33E0]:
- If done (+4084) > 10, 20 or 30, it runs fades and sound calls (0x801F59B0, 0x801F2684..., 0x8018F3A4). If +8 == r13-23276 with bit 8 set, it calls 0x801C72EC. At the first step done == 0, so it skips to 0x801C3324.
- At 0x801C3324, kind 768 (enchant) clears r13-22320.
- At 0x801C3364, done <= 30, so with bit 0x10 set it calls 0x801C54C0(0). That sets r13-22336 := 0 and r13-22312 := 0 [read 0x801C54C0-0x801C54EC].
- Then, with no branch around it, 0x801C3390-0x801C33DC: *r28 := 0, 0x801F59B0(176, caster, caster, 0), 0x801A484C(+644's +16, 10), 0x8019E0DC(-1), 0x801C5090(+644 record), 0x801C5090(spell), 0x8009DD20(0), return.

So the spell is freed on its first step, frame 3702/3703. The log agrees: "cast by 0x4C" at 3702, then the list is empty at 3720 [read there: apart page-0001.log:1208, 1234].

6. The free, 0x801C5090 [read 0x801C5090-0x801C5114]. It unlinks the record through +20 (next) and +24 (prev). It moves r13-22324 (head) or r13-22328 (tail) forward if the record was either, then calls 0x80171828(r13-22308, rec). It has 24 bl sites, among them 0x801C33CC/33D4 (this step), 0x801C3DE8, 0x801C71BC, 0x801C73E4, 0x801CC4B0, 0x801CC874, 0x801CCA6C, 0x801CDFC4, 0x801CE1CC, 0x801CEFCC, 0x801CF404, 0x801D068C and 0x801D0A14 [read ed fn]. Each spell kind's step ends itself this way.

7. The second record's step, 0x801CDF1C, has the same opening check. It ends the record if +8 != r13-23276 or +4080 bit 0 is set (0x801CDF38-0x801CDF50). Then it calls 0x801C54C0(0) if bit 0x10, 0x8019E0DC(-1), 0x801A484C, and 0x801C5090 [read 0x801CDF1C-0x801CDFC8]. Together, this second record is made at ~3830 inside coop_spells_update, outside any room swap. Apart, it would get +8 = player 1's room [inferred].

8. Other ways a spell ends, none of which ran here [read]:
- 0x801C4F08 sets +4080 bit 0 and calls the +40 callback. It has 9 sites (0x80097D54...0x800A5F4C, and 0x801C51DC from the ticket's 0x801C5118);
- 0x801C4F48 (moving) sets r13-22312 := 1 if the character is the main id and 0x801C5260 agrees. A set r13-22312 makes check (a) fail.

Both mark the record; the step then frees it. The host lists the cast ends at hle.c:32078 (kCastEnds).

The exact check: 0x801C31B8-0x801C31C4 in 0x801C316C (and 0x801CDF38-0x801CDF44 in 0x801CDF1C) compares spell +8 with r13-23276. Spell +8 was stamped from r13-23276 at 0x801C5068. One fix would be to step player 2's spells with coop_stay_room_in(...) / coop_stay_room_out around the +28 call in coop_spells_update. Together the rooms match, so it is harmless there, but it is not tested. The second record would then also be made with room 153 [inferred].

Files: E:\Eternal Darkness Recomp Fork\runtime\host\hle.c (13017-13084, 12548-12569, 22798/22943/22959, 15002-15013, 32336-32344); E:\Eternal Darkness Recomp Fork\logs\run-20261115-041146-spell-list-apart\page-0001.log; E:\Eternal Darkness Recomp Fork\logs\run-20261115-041205-spell-list-together\page-0001.log

### room
ANSWER: the step's first instruction block compares the record's own room (+8) with the room in play. Apart, the record fails that test on its first step and ends itself. This is the game's own "a spell ends when you leave its room" rule, and it runs once per record step.

1. Why the list is empty apart
- The record maker 0x801C4F90 sets +8 := r13-23276 (0x801C5060-0x801C5068). It also sets +4080 := 8, adds |4 for a flag argument, and adds |0x10 when the caster is the main id (0x801C5048-0x801C505C). [read]
- Player 2's cast runs in their turn with their room in, so both records get +8 = 153. [read there: presses-effects-readers.md, hle.c:20980/21090 order]
- The steps then run in coop_spells_update. coop_be_player2 swaps r13-26524, -21492, -21496, -21488, -21484 and the casting words, but not r13-23276 or the slot table. [read hle.c coop_be_player2, coop_spells_update]
- So each step sees +8 = 153 and room in play = 151.
- Charge step 0x801C316C, at 0x801C31AC-0x801C31D8: it continues only if all of these hold [read]:
  - the cancel word is 0 (local, or r13-22312 when +4080 has 0x10);
  - +8 == r13-23276;
  - the 0x500 record (+644) has no +4080 bit 1;
  - its own +4080 has no bit 1.
- Otherwise it takes the end path 0x801C31DC-0x801C33E0. With 0 steps done (the first step) [read]:
  - it skips the finish 0x801C72EC, because that also needs +8 == room (0x801C32F8);
  - 0x801C54C0(0), the cast clear (+4080 0x10);
  - event 176 to the caster;
  - 0x801A484C(+644's +16, 10) fades the spell's sound handle;
  - 0x8019E0DC(-1);
  - frees the 0x500 record AND the charge record (0x801C5090 twice);
  - 0x8009DD20(0).
- So both records are gone at the first step (frame 3702). The log's 3720 listing is empty. [inferred from the log + code]
- Together, 153 == 153 and it runs normally: the charge counts to 30 steps and more. At 0x801C3DD4 it calls the 0x500 record's callback (+188+424), frees the charge record, and sends event 176 or 184. The 0x801CDF1C record then shows in the list. [read the code; that the callback lists it is inferred from the together log, 812E44E0 appearing at 3830]

2. Spell 0x00010504
- Class 0x500 (bits 4-12 = 1280). It is the only 0x500 entry in kAllSpells (0x0C07050C) [read hle.c:24669].
- No doc names it. From what its step does (bolts at every targetable creature), it is probably Magick Attack [inferred].
- Its maker is 0x801CDB84, called from the cast at 0x801C4CFC [read]. It makes:
  - the step-0x801CDF1C record via 0x801C4F90 (+8 = room in play);
  - the charge record (step 0x801C316C, set at 0x801C2858) via 0x801C27DC.

3. Every r13-23276 read in the spell code (0x801C0000-0x801CFFFF: 34 reads) [read]

Charge step 0x801C316C:
- 0x801C31BC: the stay-alive guard (above). Apart: the record ends.
- 0x801C32FC: the finish (0x801C72EC) needs the same room. Apart: no finish.
- 0x801C371C/0x801C3728 (class 0x810): 0x801F6160(2170/2171, room in play), a tag search in the room, compared with the target; then the 0x801D7024 hold. Apart: wrong room's tags.
- 0x801C37A8 (class 0x820, the summon): spawn init 0x80040E94 stamps the room in play, then 0x8003121C. Apart: the summon would be a node of player 1's room. [read; stamp read there: readers.md:462]
- 0x801C352C: rune things made by 0x800422FC(91, 104, room = +8, ...), so the room comes from the record, not the room in play [read]. hle.c:9735 says their object's +592 still gets the room in play from the spawn stamp [read there]. Mixed.

Cast 0x801C4250 (runs in player 2's turn, so it already sees their room):
- 0x801C42B8: room 83 refuses.
- 0x801C45F8: class 0x810 needs a creature in the room.
- 0x801C4660: class 0x820 refuses in rooms 71/104/236/58.
- 0x801C4920: 0x801C2968(spell, room in play, ...).

Finish 0x801C72EC (from the cast 0x801C4950 or the step 0x801C3320):
- makes a record (step 0x801C73FC) via 0x801C4F90, so +8 = room in play;
- plays sound 552 via 0x8019F104, with r10 = the room in play's low half (0x801C7360-0x801C7378: the sound's room tag, per enemy-readers-2.md:133).
- Apart: player 1's room on both.

Step 0x801C73FC, at 0x801C7430: the same guard; else 0x801F2974(+68) and the end 0x801C7390 (event 57 to the +48 node, cast clear, free).

Step 0x801CDF1C (the 0x500's), at 0x801CDF38: the same guard. On a mismatch:
- 0x801F2974(+68);
- event 57 to +48 if 98 steps or fewer are done;
- cast clear, 0x8019E0DC(-1);
- fades sound +16;
- frees the record.
When it runs:
- every 16 steps: 0x80147ED4(rec+364, rec+56 place, ...), the spirit streaks [read; hle.c:9930].
- at step 60, unless the caster is a kind-8 thing:
  - 0x801F307C(+68, 60);
  - the target walk 0x801CDC70;
  - an effect record at +188 (0x8017998C, place from rec +56..+64);
  - queued with 0x801DCB58(16, +188), so the queue makes it later, outside any turn, likely stamped with player 1's room [inferred; effects +56 are stamped when made, hle.c:15720];
- at step 98: event 57 to +48.
- at step 118: the +40 callback, cast clear, 0x8019E0DC(-1), free.

Target walk 0x801CDC70:
- It walks every node (0x801F6310/0x801F6334) and keeps only those where 0x801F662C(node) == r13-23276 (0x801CDD70, 0x801CDE90): creatures of "this room".
- It skips the caster. Event 59 (can be hit) must answer 1.
- For each target it calls 0x800CA250(1, caster, target, delay += 32, spell, 1).
- In room 97 it first looks up tags 2186/2187/2188 with 0x801F6160(tag, room in play) and sends three bolts (delays 0/120/240). It then shows words 952 (0x8000B738 by alignment, then 0x801642D4).
- The bolt maker 0x800CA250 makes a thing of kind 36 (0x8003460C), puts the room in play in its data +4 (0x800CA2A8), and adds it with op 21. [read]

Other class steps with the same "+8 == room in play, else end" guard:
- 0x801CC4E0, 0x801CD03C, 0x801CEFFC, 0x801CF158, 0x801CF77C, 0x801C65FC;
- likely also 0x801CBB78, 0x801CC0E0, 0x801CFC88/0x801CFF94.

Other reads:
- 0x801CB7D0: a sound tagged with the room in play.
- 0x801CF95C: stores the room in play at +192.
- 0x801CFCD8: room 139.
- 0x801CB1D8/0x801CB218: passed on as arguments.
- [read the instructions; which spell each step belongs to is unread]

4. What goes wrong for player 2 apart (player 1's room in play)
- (a) The killer: every step's guard ends the record at its first step. No charge, no runes, no effect, no finish. The cost is already paid at the cast (0x800354F8). [read + inferred]
- (b) If only the guard were passed, more would still be wrong:
  - the target walk would aim at player 1's room's creatures (event 59 and bolts across rooms);
  - the room-97 tags would be looked up in the wrong room;
  - the bolt things would carry room 151;
  - the queued +188 effect and the streaks would take player 1's stamp;
  - sound 552 and 0x801CB7D0's sound would be tagged room 151;
  - records made inside steps (0x801C72EC) would get +8 = 151;
  - the summon (0x820) would spawn as a node of room 151;
  - the 0x810 tag test would look in room 151.
- (c) Not room-bound:
  - events 57/176/184 go to ids;
  - 0x801C54C0 and the casting words are swapped already;
  - the sound fade +16 is a handle.
  [read]

5. Implication [inferred]
- The fix is the effects pattern (fx_ticks): around each step of a record whose +8 is a resident room other than r13-23276, call coop_room_in(cpu, +8, keep) and then coop_stay_room_out. Key it on the record's +8 (the game's own field, the same one 0x80155450 tests), not only on player 2's id.
- That alone fixes the guard, the target walk, the bolt and summon stamps, the sound tags, and the +8 of successor records.
- The queued op-16 effect may still be stamped later, outside the swap.
- Not built, not run.

Scratch disassembly: C:/Users/stevi/AppData/Local/Temp/claude/6553937076d20482/52421c26-4209-49d0-ab20-99a22010a9f2/scratchpad/spells.dis

### host
THE HOST TODAY: player 2's spells apart (room 153 theirs, 151 in play)

1. What the cast stores in spell +8
- The record maker 0x801C4F90, called by the cast through 0x801C72EC, stamps the room: `lwz r3,-23276(r13); stw r3,8(r31)` [read 0x801C5060-0x801C5068]. In the same function it sets +4082 := 1 [read 0x801C502C], +4080 := 8 (|4 if r30, |0x10 if caster == 0x801F6258's controlled id) [read 0x801C5030-0x801C505C], and +4081 := 0 [read 0x801C5074]. So +8 is whatever room is current at the cast.
- Quick cast: coop_quick_cast_one (hle.c:12696-12744) calls 0x800069DC under coop_be_player2. It runs from coop_player_turn at hle.c:22943, inside the turn's bracket: `stay_in = coop_stay_room_in(...)` at hle.c:22798, closed at hle.c:22959-22960 [read]. Apart, r13-23276 is 153 during the cast, so **spell +8 = 153** [inferred from the two reads]. The cast's own room tests (0x801C42B8, the 0x810 creature test at 0x801C45F8, the summon rooms at 0x801C4660) also see 153 [read the lwz sites; inferred value].
- Tome cast from the copy: tome_copy_left calls COOP_CAST at hle.c:31218-31223 under coop_be_player2 with **no** room bracket. Its coop_stay_room_in at hle.c:31241 opens only after, for the item's script [read]. coop_cast, the non-copy route at hle.c:12788-12794, has no bracket either [read]. Both run outside the turn, so +8 = 151, player 1's room [inferred].
- Kept summons: player 2's kept summon is cast in their turn through coop_quick_cast_one, so it is bracketed (hle.c:12669-12675) [read]. Player 1's goes through stayer_after_tail (hle.c:27185-27204); it is player 1's own and has no bracket [read].

2. Who drops the spell: the step itself, not the gate and not the game's loop
- The game's loop 0x801C5994 does not run in co-op. coop_spells_update replaces it: it ends with passthrough off at hle.c:13082 [read].
- The COOP_SPELL_RUNS gate (hle.c:13044-13047) is asked only when (+4082 & 5) == 0. The log's "fl 1C0601" shows +4082 = 01, so it is never asked for this spell [read there: together log 3720; byte order from hle.c:13032-13034].
- Even if asked, 0x80155450(153) answers 1 for a loaded slot holding that room [read 0x80155488-0x801554C4].
- **The drop is the step 0x801C316C:**
  - It keeps going only if all of these hold: *r28 == 0 (r28 = r13-22312 when +4080 bit 0x10 is set, else a zeroed local), **+8 == r13-23276**, (+644 record)+4080 bit 0 clear, and own +4080 bit 0 clear. Then it branches to 0x801C33E4 [read 0x801C3184-0x801C31D8].
  - Otherwise it takes the end path. With +4084 == 0 it goes straight to 0x801C3324 [read 0x801C31DC-0x801C31E4]. That path sends event 176 to the caster, calls 0x801A484C and 0x8019E0DC(-1), and frees both the +644 record and the spell (0x801C5090 twice) [read 0x801C3390-0x801C33D4].
  - coop_spells_update steps it under coop_be_player2 only (hle.c:13063-13079), so r13-23276 = 151 while +8 = 153.
  - So the very first step, at frame 3702 (the "cast by 0x4C" line is printed at done == 0, hle.c:13055), frees it. That fits the empty list by 3720 [read there: apart page-0001.log 1204-1208, 3720 empty].
- What is lost with it: the end path makes the cast's next record (0x801C72EC) only if +8 == r13-23276 and +4080 bit 0x08 is set [read 0x801C32F8-0x801C3320]. That is together's second record (step 0x801CDF1C, made at 3830) [inferred]. 0x801C72EC also plays sound 552 with the room word as its room [read 0x801C7358-0x801C7378]. The step reads +8 again at 0x801C3538 and 0x801C3A6C [read; uses not traced].
- A Tome cast apart would not be dropped (151 == 151). It would run as a room-151 spell: its creature test, sounds and next record all take 151 [inferred].

3. Where the brackets go
- **Steps.** In coop_spells_update, around the step loop: open after coop_be_player2 (hle.c:13063-13064), close before un-swapping (hle.c:13078-13079). Mirror fx_ticks (hle.c:15738-15748): if coop_stay_apart, +8 == coop_object_room(g_coop_p2), room_slot_of >= 0, and +8 != r13-23276, call coop_room_in(cpu, +8, keep) and later coop_stay_room_out [inferred design]. Two things to keep:
  - Key it on the spell's own +8 (as fx keys on effect +56). The game's own rule stays: a spell whose caster's room is no longer current ends, as solo.
  - Skip it inside a room change, as R6 does with `r6_apart && !coop_camera_at_room_change` (hle.c:23917, 24209).
  - The torch swap and coop_be_player2 touch other words, so the order does not matter [inferred].
- **Casts.** The quick cast is already in the turn's bracket, so nothing is needed there [read 22798/22943]. Bracket COOP_CAST in tome_copy_left (hle.c:31218-31223) with coop_stay_room_in/out. Do the same for coop_cast (hle.c:12789-12794), the copy-off route [inferred].
- **R6c, the pattern to follow.** r6c_creature (hle.c:28710-28770) wraps 0x801F5294 for a creature or spawner of a resident room. When that room isn't current it calls coop_room_in(room, keep) (the room word and slot-table +0/+4, hle.c:14990-15000). It also sets that room's player in r13-26524/-21492/-21496, limits depth to 4, and skips the players' own nodes and event 4. coop_events and coop_grab_events use `r6_apart && !coop_camera_at_room_change && coop_stay_room_in` around their calls [read].

4. What the docs already say
- docs/research/separate-rooms/presses-effects-readers.md:354-358:
  - player 2's cast has their room in;
  - their later steps run with player 1's room in play;
  - which steps read r13-23276 is still open: line 526 lists it as to read [read there].
- refute-lights-goal.md:22-37: a rune cast by player 2 apart is stamped room0 [read there]. The fix it gives: take the caster's room from the spell record, after first reading which field names the caster. rune_caster_room (hle.c:9731-9738) now uses +12, then +8 [read].
- docs/coop-todo.md:129: P7, spell lights by the caster's room, is "still to do". docs/coop-next-session.md:82: "a spell cast apart (P7 not done)". coop-todo.md:126 lists quick spells through the door window [read there]. coop-todo.md:20: a quick cast's Tome ask isn't backed out when the world stops staying (not room-specific) [read there].
- docs/coop-failed-attempts.md has no entry for spells or casts apart, r13-22324 or the step's room test [read: grep].


### REFUTE
CHECKS OF THE READERS' KEY CLAIMS

1. The step's room guard. The step 0x801C316C checks four things before going on to 0x801C33E4: the cancel word (*r28; r28 = r13-22312 when +4080 bit 0x10 is set) is 0, spell +8 == r13-23276, (+644 record)+4080 bit 0 is clear, and own +4080 bit 0 is clear. If any fails, it branches to 0x801C31DC. [confirmed, read 0x801C3184-0x801C31D8]

2. The end path. With done (+4084) == 0 it jumps to 0x801C3324. With bit 0x10 set and done <= 30 it calls 0x801C54C0(0). It then runs, with no branch around them: *r28 := 0, event 176 (0x801F59B0), 0x801A484C(+644's +16, 10), 0x8019E0DC(-1), 0x801C5090(+644 record), 0x801C5090(spell), 0x8009DD20(0). The finish 0x801C72EC is called only if +8 == r13-23276 and +4080 bit 0x08 is set. [confirmed, read 0x801C31DC-0x801C33E0, 0x801C32F8-0x801C3320]

3. The record maker 0x801C4F90 stamps the room: +8 := r13-23276. It sets bit 0x10 when the caster equals the 0x801F6258 id. [confirmed, read 0x801C5048-0x801C5068]

4. Player 2's quick cast runs inside the turn's room bracket. coop_quick_cast is at hle.c:22943, between coop_stay_room_in (hle.c:22798) and coop_stay_room_out (hle.c:22959-22960). So apart, the records get +8 = 153. [confirmed for the bracket, read hle.c; the value is inferred, but it fits the guard failing apart and passing together]

5. coop_be_player2 swaps r13-26524, -21492, -21496, -21488, -21484 and the four casting words (-22336, -22320, -22316, -22312). It does not swap r13-23276 or the slot table. coop_spells_update steps +28 with only coop_be_player2 and the torch swap around it. [confirmed, read hle.c:12548-12569, 13021-13084]

6. The "end" reader says 0x80155450 is asked and answers 1 for room 153. [wrong in part] The log shows "fl 1C0601". The print order is +4080, +4081, +4082, so +4082 = 01 and (flags & 5) != 0. The gate is skipped and the step runs without it, both in hle.c:13044 and in the game's loop at 0x801C59DC-0x801C59E4. [read hle.c:13026-13034; read the together log, line 228]

7. The game's loop 0x801C5994 removes nothing. It reads the next record (+20) after the step, and it writes +4084 and +0 after the step, even when the step has freed the record. The host copies this exactly. [confirmed, read 0x801C5A40-0x801C5A78]

8. The "host" reader says the second record (step 0x801CDF1C) is made by 0x801C72EC. The "end" reader says it is made at about frame 3830, outside any swap, so it gets player 1's room. [both wrong]
- 0x801C72EC makes a record with step 0x801C73FC (r7 = 0x801C0000+29692). [read 0x801C72F4-0x801C7338]
- The 0x801CDF1C record is made at the cast by 0x801CDB84: r7 = 0x801D0000-8420 = 0x801CDF1C, then bl 0x801C4F90 at 0x801CDBC4. So it is made inside the turn's bracket, with +8 = 153. [read]
- Together, it enters the list at about 3830, after the charge's callback at 0x801C3DD4 (+424 of +188). [read 0x801C3DD4-0x801C3DE8; the callback being what lists it is inferred]
- That record is very likely the +644 record that the apart end path frees. [inferred]

9. The second step 0x801CDF1C has the same guard: +8 vs r13-23276, then +4080 bit 0; on a failure it goes to its end path. [confirmed, read 0x801CDF38-0x801CDF50]

10. The logs. Apart: cast at 3700, "cast by 0x4C: player 2's" at 3702, an empty list from 3720 to 4020. Together: 812E54D8 is listed from 3720 to 3810 with step 801C316C; 812E44E0 is first stepped at 3830 and listed at 3840 with step 801CDF1C. The room change of player 1's is at 3462. Slot 0 holds 153 and slot 1 holds 151. [confirmed, read there: apart page-0001.log:197, 381-382, 1204-1342; together page-0001.log:219-254]

11. The room reader's list of other steps with the same guard (0x801CC4E0, 0x801CD03C, 0x801CEFFC and others), and the target walk 0x801CDC70, which keeps only nodes where 0x801F662C(node) == r13-23276. [unverified by me; it is consistent with every step I read]

MOST LIKELY CAUSE

Player 2's records are stamped +8 = 153 at the cast, inside their turn. coop_spells_update then steps them outside any room bracket, with r13-23276 = 151. The first step of 0x801C316C fails its guard (153 != 151) at 0x801C31C4, because the game ends a spell whose room is not the room in play. On that same first step (frame 3702) the end path frees both the charge record and the 0x500 record with 0x801C5090. [read the guard and the end path; inferred that it happened at 3702, which fits the empty list at 3720]

SMALLEST ENGINE-GENERIC FIX (fx_ticks / R6c style; not built)

The change goes in coop_spells_update's step block (hle.c:13063-13079):

    u32 room_keep[3]; bool room_in = false;
    if (theirs) coop_be_player2(cpu, true, was);
    if (theirs && due > done && !coop_camera_at_room_change && coop_stay_apart(cpu)) {
        const s32 r = (s32)mem_read32(cpu, spell + 8u);
        if (r == coop_object_room(cpu, g_coop_p2) && room_slot_of(cpu, r) >= 0) {
            coop_room_in(cpu, r, room_keep); room_in = true; }
    }
    ... torch swap, the +28 step loop, torch back ...
    if (room_in) coop_stay_room_out(cpu, room_keep);
    if (theirs) coop_be_player2(cpu, false, was);

Why it is shaped this way:
- It keys on the record's own +8, which is the field the guard and 0x80155450 test. It also requires player 2 as the caster (+12 == p2_id), and +8 must be player 2's current, resident room. [inferred design]
- So the game's own rule still holds. If player 2 leaves the spell's room, +8 no longer equals their room, there is no bracket, and the spell ends as in solo. [inferred]
- Inside the bracket, everything the step reads sees room 153: the target walk, room tags, bolt and summon stamps, sound room tags, and the +8 of records made inside steps (0x801C72EC calls 0x801C4F90). [read that these use r13-23276]
- coop_stay_room_out restores the room word and the slot table's two words from keep[], as fx_ticks does. [read hle.c:14990-15013, 15734-15748]

What must also hold:
- **Solo: untouched.** coop_spells_update passes through when g_coop_p2 == 0 or mod_coop is off (hle.c:13022-13025). coop_stay_apart needs coop_stay() and player 2 (hle.c:14469-14473). [read]
- **Together: untouched.** coop_stay_apart is false when player 2's room equals r13-23276, so there is no bracket. [read 14472-14473]
- **During player 2's room change: untouched.** coop_stay_apart is false while g_stay_bracket != 0. Skip on coop_camera_at_room_change too, as R6 does. [read; the R6 lines are from the host reader, unverified]
- **Player 1's spells: untouched**, because the bracket also requires theirs. [inferred]

Still open after the fix:
- Work the step queues through 0x801DCB58 (for example op 28 at 0x801C3DC0, op 16 in 0x801CDF1C) runs later, outside the bracket, and may be stamped with player 1's room. [inferred]
- A Tome cast through COOP_CAST in tome_copy_left (hle.c:31218-31223) or coop_cast (hle.c:12788-12794) has no bracket. Apart, it would be stamped 151 and play as a room-151 spell. [host reader; unverified by me]
- docs/coop-failed-attempts.md: per the host reader, it has no entry for spells apart.

Files:
- E:\Eternal Darkness Recomp Fork\runtime\host\hle.c (12548-12569, 13017-13084, 14457-14473, 14990-15013, 15734-15760, 22798, 22943, 22959, 32336-32344)
- E:\Eternal Darkness Recomp Fork\logs\run-20261115-041146-spell-list-apart\page-0001.log
- E:\Eternal Darkness Recomp Fork\logs\run-20261115-041205-spell-list-together\page-0001.log### makers
Short answer: all three kinds take their room from r13-23276 at the moment their kind function runs. Nothing tests the main player or the camera. What decides whether they are made is a room test on a game thing that runs later, outside coop_spells_update's room bracket.

STAMP (all three)
- Each kind function takes a 336-byte effect from 0x80174D40. That stamps +56 := r13-23276 at 0x80174C98/0x80174C9C [read there: particles-reader.md].
- No caster, main-player or camera test in 0x80187758 or 0x8018F300 [read]. 0x80177588 was read only for its +328/+332 stores.

1. RING (step 0x801899BC; draw 0x80187B14 / 0x80187A38)
- Kind function 0x80187758 [read 0x80187758-0x80187870]:
  - 0x80174D40(r13-23104 pool);
  - +332 := 0x801899BC at 0x80187828-0x80187830;
  - +328 := 0x80187B14 at 0x8018785C-0x80187864, only if effect +264 bits 1/2/4 and request +56 bit 0x80000 are clear;
  - +34 := 4.
- It is set as +144 by the request init 0x80143500 [read 0x80143514].
- 0x8002E9D0 makes three rings: 0x80143500 at 0x8002EAD8/EB74/EC14. Each is attached at once with 0x8014152C/0x80141824, and 0x80141824 calls +144 inline at 0x80141914 [read].
- 0x8002E9D0's only caller is 0x8002D154. That is in 0x8002C794, type 36's behaviour (behaviour-readers.md:147), inside its state-1 EVENT 99 block (0x8002CEAC-0x8002D1B8).
- The block opens with: data+4 == r13-23276, else event 57 (0x8002CEB4-0x8002CED4) [read].
- Event 99 is only ever sent delayed: 0x801F57C0(99) at 0x8002CDB4/0x8002CDBC, from the event 98 block [read].
- Delayed messages go out through 0x801F53E8. Its only caller is the world frame at 0x800084D4, outside any turn, and it hands each one to 0x801F5000 at 0x801F54EC [read].

2. BEAMS (0x8018FB84, then 0x8018FD70, then 0x8018FD18; draw 0x8017F474)
- Kind function 0x8018F300: 0x80174D40, +332 := 0x8018FB84, +328 := 0x8017F474 (0x8018F360-0x8018F378) [read].
- 0x8018FB84 sets 0x8018FD70 (0x8018FBB0), and 0x8018FD70 sets 0x8018FD18 (0x8018FDB8) [read].
- The request is built in 0x8002E458(caster node, 1) [read]:
  - holder 0x8014ED1C(caster); joint 19 looked up three times with 0x801186F8;
  - entry 0x80143328; +144 := 0x8018F300 (0x8002E590-0x8002E5A0);
  - attached with 0x80141824, so the effect is made right then.
- 0x8002E458's only caller is 0x8002C990, in type 36's state-0 EVENT 98 block [read]:
  - gate data+4 == r13-23276, else event 57 (0x8002C8A0-0x8002C8C4);
  - caster node (+8) and target node (+12) must be found;
  - target type must not be 6 or 57.
- Event 98 runs at once if the delay (data+16) is 0 or less; otherwise it is sent delayed through the same pump (0x8002C824-0x8002C874) [read].

THE TYPE-36 THING (the bolt)
- Made by 0x800CA250 [read]:
  - 0x8003460C(36), which links the node into the list at once (0x801F5E94);
  - data: +0 = 1, +4 := r13-23276 (0x800CA2A8), +8 caster, +12 target, +16 delay, +20;
  - 0x801F5D18 sends event 1 to it right away;
  - op 21 is queued for its holder.
- Called from the target walk 0x801CDC70 (0x801CDEEC), which runs from the 0x801CDF1C step at count 60 (0x801CE088) [read].
- The walk keeps nodes with [read]:
  - room == r13-23276 (0x801CDE8C-0x801CDE98);
  - +56 <= 1, or type 88/57;
  - not the caster;
  - an answer of 1 to event 59.
- Delays are 0, 32, 64… (r30 = 0, then +32 at 0x801CDEF0) [read]. The step-60 path also needs the caster's state != 8 (0x801CE05C-0x801CE064).
- The only main-player test: 0x8002CA20-0x8002CA38 compares 0x801F6258 with data+8, and only in room 97.

3. 0x8017FF6C (step 0x8017790C, then 0x80177990)
- Kind function 0x80177588: +332 := 0x8017790C (0x801775F0), +328 := 0x8017FF6C (0x801775F8). 0x8017790C later sets 0x80177990 (0x80177970) [read].
- The only reference is step 0x801D0980 (0x801D1120, made inline with 0x8014152C at 0x801D119C). That step ends itself unless +8 == r13-23276 (0x801D09A4-0x801D09B4) [read].
- Its record maker 0x801D04C0(room in r3, stored at +8 at 0x801D0518) has two callers [read]:
  - 0x801D07B8, in another spell class's step 0x801D06A4 (made by 0x801CF9B8, not the 0x500 one);
  - 0x800A5250, in 0x800A51F8, which passes room = r13-23276 (0x800A523C).
- 0x800A51F8 is an animation-key callback, registered by 0x800A5284 through 0x80121C08, inside type 56's behaviour 0x800A5B88 [read].
- These effects sit on the rune things [read there: a log]: run a has runes at 933 -234 and 1150 140 and effects at 949 -229 and 1143 132.
- So [inferred]: the runes are type-56 things made by the charge step (0x800422FC, room from +8). Their animation runs in the world's object update, outside any bracket.

WHY APART (run a)
- The bolt is made inside the bracket, so data+4 = 153.
- Every event that comes through the pump sees 151 in play, fails its data+4 test and sends event 57 (0x8002CE20: queued destroy, op 2) [read].
- RING: event 99 is always pumped, so 0x8002E9D0 is never reached. Never made [read].
- BEAMS: only a bolt whose event 98 runs at once (delay 0, the first target only) can reach 0x8002E458. Every later target's event 98 is pumped and ended [read].
- The first bolt's event 98 runs inside the bracket, so it should pass. That none appeared at all is NOT explained by the code read. Possibilities [unverified]:
  - the walk found no target in 153;
  - 0x8002E458 found nothing for player 2: the holder, or joint 19 on model 70.
  - Or a delayed 98 arrives in state 1, where only 99/100 are handled. A small read suggests this, but it is unconfirmed.
- In run b everything is in 153, the room in play, so every pumped event passes.
- 0x8017FF6C ROOM 151: the runes' animation callback runs with 151 in play. The record gets +8 = 151 and is stepped as not player 2's (no bracket; it passes its own guard with 151). The effect is stamped 151 [inferred, consistent with the log].

(3) THE GREEN GLOW (not traced)
- The kind draw 0x8017E938 / step 0x80182E90 sits on the non-caster in both runs [read there: a log]:
  - a: -244 119 145, room 151, frame 3857, at player 1;
  - b: -252 97 250, room 153, frame 3748, at player 2.
- Writers of 0x80182E90: 0x8017345C, 0x8017418C, 0x801D1D8C (at 0x801D2678) [read].

FIX DIRECTION [inferred]
- Run two things with the thing's own room current (coop_room_in): pumped messages whose receiver's room (0x801F662C) is player 2's resident room (around the 0x801F5000 call at 0x801F54EC), and the animation/update of player 2's room's things.
- Probe to settle the beams: log 0x800CA250 calls and events 1/98/99/57 into 0x8002C794, with data+4, r13-23276 and the node's state.

Files: E:\Eternal Darkness Recomp Fork\build\ed-db\disasm.txt; E:\Eternal Darkness Recomp Fork\runtime\host\hle.c (coop_spells_update 13037-13110; coop_room_in 15019); E:\Eternal Darkness Recomp Fork\logs\run-20261115-042728-spell-fx-a\page-0001.log; E:\Eternal Darkness Recomp Fork\logs\run-20261115-042746-spell-fx-b\page-0001.log; the lookup script C:\Users\stevi\AppData\Local\Temp\claude\6553937076d20482\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\xref.py (finds the lis/addi pairs that build an address).

### queue
THE DEFERRED WORK: the queue, when it drains, and player 2's spell apart.

1. The queue
- 0x801DCB58(op, payload) only links a 16-byte entry {+0 op, +4 payload, +8 next, +12 prev} onto the list at r13-22208. The entry comes from the pool r13-22204 (0x801DCD40) [read 0x801DCB58-0x801DCBD4].
- **No entry stores a caster or a room.** Its only fields are op and payload [read].
- The drain is 0x80025FF0. It walks the list (0x801DCCC4/CCCC/CCD4/CCDC), jumps on op through table 0x8023E660 (ops 0-32), and frees each entry (0x801DCC60) [read 0x80026038-0x80026F68, table dumped from main.dol].
- In play it runs once a frame from the world frame 0x800083C8, at 0x80008548. That is late in the frame:
  - after the spells' update 0x80008484 (coop_spells_update),
  - after the emitters 0x80008488,
  - after the event pump 0x801F53E8 at 0x800084D4,
  - after the timer table 0x801DCBD8 at 0x8000853C [read].
- Its other callers are the frames of other modes: 0x80020430 (level start), 0x80050ABC (cinematic), 0x800B8578, and 0x8011177C (the Tome).
- **The host does not intercept the drain** (no hle.c hit for 0x80025FF0, 0x801413E8 or r13-22208) [read: grep].
- The host already intercepts the push: COOP_THING_GONE = 0x801DCB58 is wrapped by coop_thing_gone. It passes everything through except player 2's box ops (hle.c:24969, 25107-25119, 31968-31970) [read].

2. The ops that matter here
- **Op 16** (0x80026C78) = 0x801413E8(desc):
  - It calls the maker at desc+144 (result stored at desc+148), then makes a behaviour record (0x8014FD08) [read 0x801413E8-0x80141524].
  - The spell code always puts maker 0x80179B94 there [read 0x801C2E5C, 0x801CE108, 0x801D103C].
  - That maker allocates the effect. The allocation stamps +56 := r13-23276 (0x80174C98) and sets draw 0x8017A718, step 0x8017A07C.
  - It also plays sound 264 (0x8019F104), tagged with r13-23276's low half (0x80179CC0-0x80179CC8) [read].
- **Op 21 / op 13** (0x80026880 / 0x800269FC): a type-7 behaviour record bound to a node (0x8014FD08, then 0x8014F880). No room is read [read].
- Ops 1 and 28 in 0x801C316C (0x801C3D5C, 0x801C3D6C, 0x801C3DC0) are the summon path only: the node comes from 0x8003121C at 0x801C3A00. Op 28 = 0x800339A4 + 0x801F5EC4, the delete. They are not used by 0x00010504 [read].

3. What player 2's spell pushes, and what runs outside both brackets with player 1's room in play

(a) Charge step 0x801C316C:
- It calls 0x801C2DBC at 0x801C34D0, 0x801C3EEC, 0x801C3F50 and 0x801C3FC4. Each call fills a description inside the record (r30+72 at 0x801C3ED0) and **queues op 16** (0x801C2E94) [read].
- At the drain this becomes the effect 0x8017A718/0x8017A07C, stamped 151.
- That matches run a, frame 3713: "draw 0x8017A718 step 0x8017A07C at 1150 -100 9, room 151". Run b has the same effect in room 153 [read there: a page-0001.log:1220; b :771]. It is in player 2's place but stamped with player 1's room, and its sound is tagged 151.
- At 0x801C3F14-0x801C3F2C the step also makes a kind-23 thing (0x800CC8BC, a screen-space thing: 640x480 fields at 0x800CB28C) at spell +48, and queues op 13. No room is involved [read].

(b) The 0x500 step 0x801CDF1C, step 60:
- The rec+188 effect is queued as **op 16** (0x801CE15C) and made the same way, stamped 151 at the drain [read; inferred: frame 3886 in a].

(c) **The bolts. This is fact (1).**
- The target walk 0x801CDC70 runs inside the bracket and calls 0x800CA250 once per target.
- 0x800CA250 makes a kind-36 thing. Its data record (node+40, then +64) holds:
  - +0 kind;
  - **+4 = r13-23276** (now 153, set at 0x800CA2A8);
  - **+8 caster id**;
  - +12 target id;
  - +16 delay;
  - +20 spell.
- It registers the thing (0x801F5D18, which sends event 1 at once, 0x801F5D2C) and queues op 21 [read 0x800CA250-0x800CA2D0].
- Event 1 reaches the kind-36 handler 0x8002C794 inside the bracket. The rooms match, so it posts **event 98 for later**, delayed by data+16 (0x801F57C0 at 0x8002C85C) [read 0x8002C7F4-0x8002C88C].
- The event pump 0x801F53E8 (0x800084D4) delivers event 98 a frame or more later, outside any bracket. Every case of the handler begins with `data+4 == r13-23276` (98 at 0x8002C8A0, 99 at 0x8002CEB8, 100 at 0x8002D1C0). Here 153 != 151, so the handler sends **event 57 to the bolt itself**; event 57 queues op 2 (0x8002CE30) [read].
- Lost with it:
  - the beams: 0x8002E458, then maker 0x8018F300, then draw 0x8017F474 / step 0x8018FB84, 0x8018FD70, 0x8018FD18;
  - the ring: 0x8002E9D0, then 0x80187758, then 0x80187B14 / 0x80187A38, step 0x801899BC;
  - event 100's hit: the op-16 effect at 0x8002D398, sounds, and **event 101 to the struck creature at 0x8002D488, followed by 0x80063D68 (damage, inferred)** [read: the maker sites by constant search].
- So apart, player 2's Magick Attack does no damage.

(d) Fact (2), draw 0x8017FF6C, stamped 151:
- Its maker 0x80177588 (+328 := 0x8017FF6C at 0x801775F8) has one reference, 0x801D1120 in the spell step 0x801D0980 (set up by 0x801D04C0). No such record shows in the 30-frame spell lists [read there: logs/run-20261115-041205-spell-list-together].
- How this cast reaches it is **unresolved**. The 151 stamp means it is made outside the bracket [inferred].

(e) Fact (3), player 1's green glow:
- **Not traced.** Run a has 0x8017E938/0x80182E90 stamped 151 at frame 3857 with local-looking offsets (-244 119 145). Its makers are 0x8017345C, 0x8017418C and 0x801D1D8C (not read).
- Most likely cause [inferred]: a glow attached to "the controlled character" (0x801F6258 / r13-21484), made at the drain or the pump, where player 1's identity is in.

4. A per-op record of the room and caster the host could use
- The queue entry has none. The payloads and things do:
  - **Op 16:** the description is inside the spell record (r30+72; rec+188). The owning record's +8 (room) and +12 (caster) can be found from the address [inferred]. Simpler: the existing push hook at 0x801DCB58 can note `payload -> room (r13-23276), player 2` whenever it runs inside coop_spells_update's room_in (or player 2's turn).
  - Then intercept 0x801413E8 only when lr is 0x80026C7C (op 16). Also 0x80026CEC / 0x80026D34 (op 22 / op 25 call it too). Wrap it in coop_room_in(room) + coop_be_player2.
  - The made effect then carries +56 = 153, and fx_ticks takes it from there.
- **The bolts (kind 36):** the game's own per-thing record holds the room (data+4) and the caster (data+8). The place to wrap is event delivery 0x801F5294, which R6c's r6c_creature already wraps. Today it skips them: it resolves rooms only for creatures (actor+158 == 2) and type 78 (hle.c:28739-28760). It is also installed only when ED_ROOM_SLOTS is set (hle.c:32720) [read].
  - Adding type 36 with room = data+4 and player = the owner of data+8 would run events 98/99/100 as player 2 in 153. That restores the beams, the ring, the hit and the damage [inferred, not built].
- The kind-23 thing at spell +48 and the op-13/op-21 behaviour records read no room [read].

Files: E:\Eternal Darkness Recomp Fork\runtime\host\hle.c (13037-13105, 24969, 25107, 28738-28790, 31968, 32720); E:\Eternal Darkness Recomp Fork\build\ed-db\disasm.txt; E:\Eternal Darkness Recomp Fork\logs\run-20261115-042728-spell-fx-a\page-0001.log; E:\Eternal Darkness Recomp Fork\logs\run-20261115-042746-spell-fx-b\page-0001.log

### glow
THE CASTER'S GLOW: findings. The exact call that makes the glow is NOT pinned. Below is what is ruled out, two routes that run outside the bracket, what the pictures show, and the probe that would settle it.

PICTURES
- Together (c), during the charge (shots 24-25, about frames 3560-3612; c's cast is at 3542 and state 64 runs until 3666): player 2 is the thin dark body (model 70) at the circle, arm raised in shot 24. In shot 25 they turn solid green, and so does the red creature by the fountain. Player 1, the Roman soldier, stays unlit: his teal helmet shows behind the creature. [read there: shots 000024/000025, cropped]
- Shot 31 (about frame 3750, during the second record) shows the red creature with the patterned glow. That fits the bolt-hit glow 0x80198268, which calls 0x80119C34 on the target id at +4 [read 0x80198228-0x80198268]. So together, the caster (player 2) glows, which is right. [inferred]
- Apart (a, run 042402), shots 32-34 (about frames 3770-3818; the cast is at 3702 and state 64 runs until 3826): player 1 shows the patterned green. The dark figure next to player 1 in 151 does not glow. Player 2 and the red creature, both at the circle in 153, are not lit at all. [read there: shots 000032/34/36; frames mapped through both.tsv ms]

WHAT THE GLOW LOOKS LIKE IN CODE
- The patterned look matches the object glow: 0x80119C34 writes +700/+704/+722/+724 [read 0x80119C34-0x80119CAC]. The actor draw draws it through 0x801E1D74 at 0x8011A684 and 0x8011BF48 [read].
- The same look is on the bolt-hit target in c shot 31. [read there]

RULED OUT [read]
- 0x80119C34 has 82 call sites (build/ed-db/disasm.txt). None are in the 0x500 charge step 0x801C316C, the record step 0x801CDF1C or the cast 0x801C4250.
  - The sites that use alignment colour are all hits, enchants, runes or other spells: 0x8002AC54, 0x8002D8D0/D97C (events 39/220), 0x80038CD4 (weapon hit), 0x800DA38C/4F0 (enchant), 0x8002BD24 (rune thing), 0x80198268 (bolt target), and 0x801D34D8/35B0/4C88/4D78 (other record steps 0x801D3268/0x801D4290).
  - The 0x800A1828 glow helper is used only in room 39.
- The charge's glow-bit and tint calls (0x80118BEC/0x80125A70 at 0x801C3608-0x801C3628 and 0x801C3CB8-0x801C3CC8) go on the rune things from 0x800422FC and the summon. They don't touch the caster.
- The room light: at done==20, 0x801C3F8C calls 0x801D6D30(spell+644 record +68). That stamps +52 := r13-23276 (0x801D6D58) and queues op 19 (0x801D6DE8). Op 19 runs 0x801458A0(0, desc) -> 0x801F1D84, whose room is desc+48 (0x801F2A20). With the new bracket that stamp is 153, so this light can't reach player 1 [inferred]. It also has no holder that could follow a body (op 19 passes 0) [read 0x80026ECC].

TWO ROUTES THAT RUN OUTSIDE THE BRACKET (these explain the room-151 stamps)
1. Queue op 16 [read]
   - The charge builds effect records with 0x801C2DBC: at done 0/10 (0x801C3EEC, 0x801C3F50) and for each rune (0x801C34D0).
   - Each record carries the maker 0x80179B94 at +144 (0x801C2E5C) and is queued as op 16 (0x801C2E94).
   - The queue worker's op 16 (0x80026C78) calls 0x801413E8, which runs the maker (0x80141440-0x8014145C) later in the frame, outside coop_spells_update.
   - So the effect draw 0x8017A718 / step 0x8017A07C (count 32) is stamped with the room in play: 151 in a (page-0001.log:1220), 153 in b (:771). [read there]
2. The cast animation's key-frame callback
   - Player 2's cast-state handler 0x800A5B88 (installed at 0x80030F1C) calls 0x800A5284. That starts cast animation 79/77 (0x80122920) and registers 0x800A51F8 as a frame callback (0x80121C08 at 0x800A5348/0x800A5380). [read]
   - 0x800A51F8 calls 0x801D04C0(room = r13-23276 at 0x800A523C, the position of thing r13-25292, ...). That makes a spell record with step 0x801D0980, which has the same rule: +8 == r13-23276, or it ends (0x801D09AC). [read]
   - Its emitter 0x8014152C (maker 0x80177588, at 0x801D1120/0x801D119C) makes draw 0x8017FF6C / step 0x8017790C. [read]
   - In both runs that effect sits at the same offset from the caster, (-201, -129). It is stamped 151 in a, so this record stepped with 151 in play. So the callback runs outside player 2's room bracket. [inferred from the log lines a:1286/1290 and b:848]

WHICH CHARACTER
- Everything the charge itself reads runs inside coop_be_player2, so the caster it uses is player 2. The caster at spell +12 is 0x4C [read there].
- The glow on player 1 therefore comes from a path outside the bracket: a queue op, the animation callback, or an effect step under fx_ticks/fx_emitters (room swapped, no identity swap) that reads the main player r13-21492/-21496/-26524. [inferred]
- The giveaway is that only the body of the half's hero is lit, not the creature beside them [read there: picture]. A path keyed on r13-26524 fits this, for example the actor draw's player-only branch, budget 4 with lights 5-7 (0x8011A3F8, 0x801E652C) [inferred, not proven].

WHY PLAYER 1 GLOWS APART: unproven. The best-supported idea is above. It does not come from the charge or record steps, which are now correct [read].

PROBE THAT SETTLES IT (no code was changed)
- Log every 0x80119C34 call whose object (r3) is player 1's object 0x8123D7A0, with lr, during frames 3702-3826 of the a script.
- In the same run, set ED_FX_LIGHTS=1 (fx_lights_probe, hle.c:15802) and read each of the 12 records' room (+42) and flags (+41) during the charge.

Files: E:\Eternal Darkness Recomp Fork\runtime\host\hle.c (coop_spells_update about 13040-13110, lights_room_in 27342, lights_actor_call 27394); E:\Eternal Darkness Recomp Fork\logs\run-20261115-042728-spell-fx-a\page-0001.log; E:\Eternal Darkness Recomp Fork\logs\run-20261115-042746-spell-fx-b\page-0001.log; the c shots 000024/25/31 and a shots 000032/34. Crops are in C:\Users\stevi\AppData\Local\Temp\claude\6553937076d20482\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\ (c000025.png, c000031.png, c24.png, a32.png).

### REFUTE
CHECKS

Makers
- The effect stamp +56 := r13-23276 at 0x80174C98/9C. [confirmed]
- The type-36 gates compare data+4 with r13-23276 and send event 57 when they differ: event 1 at 0x8002C7FC, event 98 at 0x8002C8A0, event 99 at 0x8002CEB4, event 100 at 0x8002D1C0. [confirmed]
- Event 99 is only sent delayed, through 0x801F57C0 at 0x8002CDBC. [confirmed]
- 0x800CA250 sets data+4 := r13-23276 at 0x800CA2A8 (data = 0x801F6300(node)+64). Event 1 is sent at once: 0x801F5D18 calls 0x801F59B0, which stamps the time as now and calls 0x801F5000 directly. [confirmed]
- The walk's room test (0x801CDE8C-98), its event 59 (sent at once), the delays growing by 32 (0x801CDEF0) and the caster state != 8 test (0x801CE05C). [confirmed]
- When the delay is 0, event 98 is sent at once (0x8002C874). [confirmed]
- A delayed 98 arriving in state 1 is lost. [confirmed] The handler picks its branch on r4 = node+20 (0x801F50D0, 0x8002C7F0, 0x8002CEA4), and state 1 handles only 99 and 100 (otherwise 0x8002E1D8). This is the same together, so it is not a cause of the apart failure.
- 0x800A51F8 passes room = r13-23276 (0x800A523C) and thing r13-25292 to 0x801D04C0, which stores it at +8 (0x801D0518). The step's guard is at 0x801D09A4-B4. [confirmed]
- 0x800A5B88 is type 56's handler, registered at 0x80030F14-24. [confirmed] The glow reader's "player 2's cast-state handler" is [wrong].
- Frame order: spells 0x80008484, emitters 0x80008488, pump 0x800084D4, drain 0x80008548. [confirmed]

Queue
- Op 16 is 0x80026C78, which calls 0x801413E8 (lr 0x80026C7C). The charge pushes it at 0x801C2E94 with maker 0x80179B94 set at 0x801C2E5C. The maker's sound tag reads r13-23276 at 0x80179CC0. [confirmed]
- r6c_creature (hle.c:28738-28790) works out a room only for type 78 and for actors whose +158 == 2. It is installed only when ED_ROOM_SLOTS is set (hle.c:32718-32722). [confirmed]
- The room light: sth +52 := r13-23276 at 0x801D6D58, then op 19 calls 0x801F1D84(desc+4), which copies that room. So it is 153 in the bracket. [confirmed]
- Event 101 and the damage. [unverified]

Glow
- 0x8017E938/0x80182E90 is a hit spark, not the spell's glow. Every reader who tied it to the glow is [wrong]. [read there: logs]
  - In run b it shows on player 1 at frame 3565, before any cast, and player 1's health goes 125 to 115.
  - In run a it shows on player 1 at frame 3857, then player 1's health goes 125 to 117 at -258 176. The creature in 151 hit him.
  - In run b it shows on player 2 in 151, whose health goes 75 to 67 to 59.
- The a32 crop shows player 1 lit strongly green from below and in front, and the creature beside him unlit. [read there: picture]

THE CAUSES

(1) Beams and ring missing
- In run b, the beam (0x8018FB84) and the ring (0x801899BC) both first appear at frame 3886. That is the second record's cast at 3826 plus 60, at the target, 1385 -109. [read there: b log]
- In run a, nothing appears at 3886.
- Ring, and every target after the first: event 99 and every delayed event 98 arrive through the pump, after the bracket. With 151 in play they fail the data+4 test, get event 57 and are destroyed through op 2 (0x8002CE30). [confirmed]
- The first target's beams: its events 1 and 98 run at once, inside the bracket, so they should pass. Yet nothing appeared. [unverified] Possible reasons:
  - the walk found no node in 153 (the room, +56 <= 1, or the answer to event 59);
  - the event-98 block bailed when the caster or target node was not found (0x8002C900/908);
  - or the target was type 6 or 57.

(2) 0x8017FF6C stamped room 151
- Type 56's animation callback 0x800A51F8 runs in the object update, with 151 in play. [inferred]
- So 0x801D04C0's record gets +8 = 151, and the effect maker stamps 151.
- coop_spells_update only brackets records where spell+8 == player 2's room, and only when they are theirs. So this record steps with 151 in play and passes its own guard.

The op-16 effect 0x8017A718 (stamped 151 at frame 3713) has the same cause: the drain runs after the bracket.

(3) Player 1 glowing [unverified]
- It is not the hit spark, not the charge step, and not the op-19 light, which is stamped 153.
- During the glow window (frames 3770-3818), the only spell work player 1's room can show is:
  - 0x8017A718, the drain's op-16 effect;
  - 0x8017FF6C, the runes' callback;
  - anything else that runs outside player 2's brackets (the pump, the drain, the type-56 updates).
- Most likely one of these lights or glows through room 151's lights or the hero light budget of r13-26524 (player 1 outside a bracket).
- Probe to settle it:
  - ED_FX_LIGHTS=1 (fx_lights_probe, hle.c:15802), to read the 12 dynamic light records' room and flags during the charge;
  - log every 0x80119C34 call on 0x8123D7A0 with its lr, frames 3702-3826.

SMALLEST HOST CHANGES (none touch solo, since there is no g_coop_p2; together, the room is already the one in play, so all pass through)

A. Fact (1): add type 36 to r6c_creature.
- room = mem(0x801F6300(node)+64)+4;
- player = the node of the id at data+8 (0x801F5F88);
- if that is g_coop_p2, use coop_be_player2 (the full 5-word swap, with the casting words) instead of r6c's 3 words, and coop_room_in(room) around 0x801F5294.
- Events 98, 99 and 100 then run in 153 as player 2: beams, ring and the hit.
- It needs the R6c install, which depends on ED_ROOM_SLOTS. Make sure that is on for separate rooms. [inferred]
- First run the probe for the first target: log 0x800CA250's calls, and events 1/98/57 into 0x8002C794 with data+4 and r13-23276.

B. Op-16 and op-19 work queued during player 2's spell bracket.
- coop_thing_gone already wraps the push 0x801DCB58 (hle.c:24969, 31968).
- While coop_spells_update's room_in is on, record payload -> (room, player 2) for ops 16, 19, 22 and 25.
- Intercept 0x801413E8 when lr is 0x80026C7C, 0x80026CEC or 0x80026D34 (or 0x801458A0 from op 19) and the payload has a note. Wrap it in coop_room_in(room) and coop_be_player2, then clear the note.

C. Fact (2): two parts.
- Intercept 0x801D04C0 when lr is 0x800A5254. Key on the room of thing r13-25292 (coop_object_room). If that is player 2's room, wrap it in coop_room_in, plus coop_be_player2 when it is player 2's rune.
- In coop_spells_update, key the room_in bracket on spell+8 == player 2's room alone; keep the identity swap only for player 2's records. Otherwise a record not cast by player 2 but in their room is still ended by its own guard.

What can share one change
- B and C both put the work back in the room it came from, using that record's own room.
- B and A can share one helper: "deferred work carries its originator's room and caster", applied at the drain and at the event delivery.
- The coop_spells_update change in C is useful on its own, for any spell record in player 2's room.
- Fact (3) will probably go away with B and C. Check it with the probe before writing a separate fix.

Files: E:\Eternal Darkness Recomp Fork\runtime\host\hle.c (13037-13110, 15019, 27342-27432, 28738-28790, 32718), E:\Eternal Darkness Recomp Fork\build\ed-db\disasm.txt, E:\Eternal Darkness Recomp Fork\logs\run-20261115-042728-spell-fx-a\page-0001.log (lines 1203-1340), E:\Eternal Darkness Recomp Fork\logs\run-20261115-042746-spell-fx-b\page-0001.log (lines 594-918).