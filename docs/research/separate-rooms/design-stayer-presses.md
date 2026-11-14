# Design (final): the stayer keeps every action through the other's door countdown

The item (the user, 2026-11-13): when one player takes a door and the other stays (ED_ROOM_SLOTS=2 + ED_COOP_STAY=1, co-op
on), the stayer keeps every action through the ~62-frame countdown, and through a 12-frame zone exit's. That means quick
spells, spells from the Tome, B on prompts (examine, pick up), zone looks, and Start (the Tome). Both shapes: split (both
in one room) and apart. Both directions. "That needs to happen for co op to function like a true co op."

The split hold (design-split-hold.md, split-hold-impl-notes.md) already lets the stayer's room play on. This design opens
the stayer's presses on top of it. This is the final version. It folds in refute-stayer-presses-impl.md and
refute-stayer-presses-goal.md. Each of their points was checked again in the code; §13 lists what each one came to.

Read only: no code was changed, nothing was built, the game was not run. Sources: runtime/host/hle.c (working tree,
uncommitted, 30,141 lines), runtime/host/main.c, build/ed-db/disasm.txt (`ed dis`, `ed fn`, `ed script`), the readers.
[read] = seen at the address or line given. [read there] = seen in the named doc. [inferred] = reasoned, not seen.

Words: the **mover** takes the door. The **stayer** stays. **Player 1 staying** = player 2's door (mover 2).
**Player 2 staying** = player 1's door (mover 1). **The window** = from the ticket (0x80155770) to the change's tail
(split_counting, hle.c:24607). The firing and the tail are in one frame, so "+0 > 0" is the whole window [read there:
refute-goal §3, split-k1-p1-leaves page-0001.log:556, 1377].

## 0. In short

The plan has three build steps. Each one is behind one switch, `ED_STAYER_PRESSES`, and one predicate.

**Step A: the presses go in.**
1. One predicate, `stayer_window(cpu, who)`: split_counting, the switch, and "who is the stayer" (§1).
2. **L1, the root of player 1's refusals.** Player 2's prompt door runs as a use, which swaps no input copy. So the
   ticket's input lock lands in player 1's input copy, the stayer's, for ~65 frames. At that one call, set only the
   Start lock. Put the Start lock back to its solo count at the window's end (§2.1).
3. **P1, the door's prompt bit.** 0x10000 of r13-24048 hides player 1's prompts. The two readers answer without it,
   through one wrapper round today's chooser code. Bit 4 is not masked (§2.2).
4. **The host's follow-mode refusals, lifted one line each,** for zone looks, B, quick casts and player 2's Start (§2.3,
   §2.6, §2.7). The way-out wait gets a new first test in coop_script_go_to_room, under the window only (§5.2).
5. **Start opens a Tome copy for either player** (player 2 as always; player 1 by the route that already exists in player
   2's close-up). Only with ED_COOP_TOME_COPY=2, the default. The copy's input lock is cleared at its opening (§2.7).

**Step B: the room plays on under the stayer's own action, with no "not busy for everyone".**
6. **B1.** In the window, coop_busy_ask's four blanket cases give way to the split hold's allow list (§3.1).
7. **B2.** split_on stays on under the busy bits the stayer set, recorded by who set them. It calls nothing that writes
   (§3.2).
8. **B3.** Player 1's look is not read "over" for the other's room bit, so R4b carries it. Player 2's coop_p2_holds is left
   as it is (§3.3).
9. **B4.** Player 1's words and finds open with no hold in the window, and never let go of a hold that is the door's
   (§3.4). **B5.** Player 1 stands in their own look, words and find (§3.5).
10. **B6.** Player 2's uses in the window are not taken for a close-up because of the window's busy 4 (§3.6).

**Step C: the firing.**
11. Player 1 staying: everything crosses the firing. R4b carries their close-up and scene. Their words and finds set no
    bit (§4.1).
12. Player 2 staying: their own-set actions, quick-spell records and copy Tome cross the firing. Their lend-route uses,
    lend-route words and scenes still make the mover's change wait (G2, G2b), because R4d is not built. **C2 removes that
    wait**: R4d's per-player waits and bits, widened to the split shape (§4.2). Until C2, the mover waits in the black for
    those, and that is the one freeze left.

**Room requests in the window** (one ticket record, §5): a B on an exit prompt is kept and goes in after the change (M11);
a way out walked into waits (M12); a room asked later by the stayer's own look or use (the ladder's Yes) is kept and asked
again after the tail (M13, needs read R1 first).

**Still refused or deferred** (§6): the real Tome (mode 3) for player 1 in player 2's countdown (their copy opens
instead); Enchant's "choose an item" from player 1's quick spell; a summon pressed in the window (kept to the tail); a
summon charging at the ticket (G3, as today); a cinematic begun in the window (G2, risk 12).

## 1. The window, the switch, and who set a bit

```
// ED_STAYER_PRESSES: unset or 1 both directions, p1 player 1 staying only, p2 player 2 staying only, 0 off.
static bool stayer_window(CPUState* cpu, int who /* 0 player 1, 1 player 2 */) {
    if (!split_counting(cpu) || !stayer_switch_allows(who))
        return false;
    return g_split.mover == (who == 0 ? 2 : 1);
}
```

- split_counting (hle.c:24607-24612) is: g_split.on, not the copy's turn, mod_coop, g_coop_p2, ticket +0 > 0, and the
  stayer still in the room they stayed in [read]. g_split.on is set only in split_ticket_open, which returns at once
  without g_split_ready, mod_coop, g_coop_p2 or coop_stay() [read 24781-24782].
- So with ED_SPLIT_HOLD=0, a cutscene at the ticket, the stayer casting at the ticket (G3), or the copy's turn, there is no
  window. Every press is refused as today.
- The mover is g_split.mover, set from g_coop_room_p2 at the ticket [read 24784]. The stayer's node is split_stayer_node
  (r6_player1_node for player 1, g_coop_p2 for player 2: hle.c:24603-24604) [read].
- Both players are read for everything. Player 1's words in memory are the game's. Player 2's are swapped in by
  coop_be_player2 (r13-26524, -21492, -21496, -21488, -21484 and the casting words) or by their turn (three words,
  hle.c:20970-20975), with their input copy g_coop_state[1] [read].

**g_stayer** (new host record, zeroed at each split_ticket_open and at split_state_load):
- `prompts0`: r13-24048 at the ticket. split_ticket_open runs at the writer's entry, before 0x80117260(0x10000, 1) at
  0x801557E0 [read 11389; hle.c:5578-5589, the notify runs only on the intercept's passthrough].
- `bits`: the busy bits the stayer set. Recorded in stay_busy_clear, the intercept already on 0x801556CC (install at
  hle.c:29450-29455) [read], before its own test: when split_counting, r4 == 1, and the current id (r13-21496) is the
  stayer's, `bits |= r3 & (8|16|32)`. A clear (r4 == 0) by the stayer takes its bits off. At the ticket, word0's bits 8,
  16 and 32 go into `bits` only when the stayer alone has a thing up (player 1: coop_scene_on_of(0), g_coop_p1_look,
  g_coop_words_free, g_coop_find_free; player 2: coop_scene_on_of(1), g_coop_screen, g_coop_words_theirs,
  g_coop_find_theirs) and the mover has none [inferred; logged at the window's open].
- `lock_n`, `lock_frame`: what L1 set and when (§2.1).
- the counts for the over line (§7), and the deferred records of §2.6 (summons) and §2.7 (player 2's Tome item).

## 2. Step A: each action, which gate opens, and how

### 2.1 L1: the input lock at player 2's door lands on player 1

**The fact.**
- The ticket writer calls 0x800443E4(r30 + 3) at 0x801557F4; it returns to 0x801557F8 [read].
- 0x800443E4 raises two counts to at least n: r13-26232, the Start lock (by 0x800068E0), and r13-23592, the binding lock
  (by 0x8013E99C) [read there: b3 §1].
- r13-23592 is in pad_globals (hle.c:12015, swapped by coop_state at 12298-12299), so it is per input copy [read].
  r13-26232 is in no copy, so it is shared [read there: b3 §4].
- At player 2's prompt door the writer runs inside coop_use_start, which swaps the player words but not coop_state
  [read 18182-18232]. The log shows the ticket in player 1's controller, outside g_coop_turn, with player 2's words in
  [read there: refute-impl §4]. So the binding lock goes into the game's copy, which is player 1's, the stayer's.
- While it is up, the play channel r13-26224 runs no row: no B, no Start, no quick spell, no R [read there: b3 §2; the
  skip at 0x8013E210]. That is why "an intercept on player 1's Start 0x80006CDC in player 2's room change was never
  reached" (coop-failed-attempts.md:560) [read].
- The same holds for a ticket of player 2's way out that the game writes with player 1 current (notify_room_ticket's
  exit_p2, hle.c:11362-11366) [read]: lr 0x801557F8, outside g_coop_turn, mover 2.

**The change: a new intercept `stayer_lock` on 0x800443E4.**
- Only when cpu->lr == 0x801557F8, stayer_window(cpu, 0), and !g_coop_turn.
- Then guest-call 0x800068E0(r3) (the Start lock alone, the same count), keep `lock_n = r3` and the frame, and return.
  0x8013E99C is not called.
- split_counting is already true at that call: the notify ran at the writer's entry, and the record's +0 is written at
  0x801557BC, before 0x801557F4 [read].
- G2's own call of 0x800443E4 for mover 1 (hle.c:24937-24941) has lr 0x0BADF010, the guest-call sentinel (main.c:52, 80)
  [read]. It passes through.
- Player 2's walk-in door in their own turn (g_coop_turn): the lock goes into their own copy, the mover's, as in solo
  [inferred there: b3 §4]. It passes through.
- No hook sits on 0x800443E4, 0x800068E0 or 0x8013E99C today [read there: refute-impl §4]. G2 already checks 0x800443E4's
  first words (g_split_lock_ok, hle.c:29484) [read].

**Why the Start lock stays.** 0x80006CDC opens the real Tome or menu and returns while r13-26232 is not 0 (0x80006D00)
[read there: b3 §3]. A real Tome with the change firing behind it was never run, and game_menu_open turns every room
rule off (coop_room_change_on, hle.c:11346-11348) [read]. Solo never opens a Tome in a door. So player 1's Start goes to
their copy (§2.7).

**The Start lock after the window.** r13-26232 counts down only while the play channel runs with r4 == 1, not while words
have the focus (0x80006900-0x80006964) [read there: b3 §3]. Player 1's words in the window would keep up to 65 frames of it
past the window, and their Start would do nothing after [inferred]. So at split_over, when L1 ran this window: if
r13-26232 is not above lock_n, lower it to `max(0, lock_n - frames since lock_frame)`, the count solo would have reached.
A larger count was set by something else (a script's deny_menu, ed217) and is left.

### 2.2 P1: the door's prompt bit hides player 1's prompts

**The fact.**
- The world frame asks 0x8011727C(-1) at 0x800084DC (lr 0x800084E0). If any bit of r13-24048 is set, it clears the shown
  prompt with 0x80117360(0) at 0x800084EC [read].
- The chooser 0x80117628, called at 0x800084F8, returns at 0x801176B4 while the word is not 0. It reads the word
  directly, not through 0x8011727C [read there: refute-impl §4].
- The ticket sets 0x10000 at 0x801557E0 [read]. The arrival clears only 0x10000 (0x8015203C-0x80152044) [read there].
- The prompt use 0x80006EBC presses only the shown record (0x80006EE4 -> 0x8011799C) [read]. So player 1's B finds nothing.
- r13-24048 has three readers: the set 0x80117260, the ask 0x8011727C (from 0x80008490, 0x800084DC and 0x8009E7BC) and the
  chooser at 0x801176B4 [read there: refute-goal §3]. P1 covers player 1's prompts fully.
- Bit 4 is fn139's (0x80042B30 sets r13-25696, the zones word r13-23040 and bit 4: 0x80042B7C-0x80042B90) [read].
  g_coop_script_hold is fn21's, the stepper's 0x80199EA0 (bl at 0x8016829C; hle.c:13043-13044, 13185-13186), which
  touches only the walking channels and busy 2 [read there: refute-goal 1.11; read 0x80199EA0-0x80199F34]. Door script 119
  calls neither [read there: `ed script 119`].
- Player 2's B does not use the chooser: coop_prompt_look copies the slot to the shown record itself and calls 0x80006EBC
  (hle.c:16912-16917) [read]. Whether their shown icon goes through the chooser is read R2.

**The change.**
- `door_bits = 0x10000 & ~g_stayer.prompts0`. Bit 4 is never masked: it may be player 1's own fn139 hold.
- coop_flags_ask (intercept on 0x8011727C, hle.c:19628): a second case. When lr == 0x800084E0 and stayer_window(cpu, 0):
  answer `word & r3 & ~door_bits`. Today's case (lr 0x80008494, the script timers) is unchanged [read 19630].
- coop_prompt_choose (intercept on 0x80117628, hle.c:19923): **one wrapper, first.** When stayer_window(cpu, 0),
  !g_coop_turn, !running and door_bits is in the word: clear door_bits in r13-24048; run whatever today's code would run
  (the player 1 look branch at 19928-19944 when its test holds, with its look channel opened; else a plain guest call of the
  chooser with running = true); then put door_bits back and return handled. The look branch's own failure ("the pick up
  showed before its scene ended, and B skipped it", 19926) stays fixed [read].
- The bits stay in the word for everything else (0x8009E7BC, the arrival's clear).
- If a door of player 2's sets fn139's bit 4 before its fn66, player 1's prompts stay hidden by it. Read R5 says whether
  any door does.

### 2.3 Zone looks (walking in)

**Player 1 staying.** coop_scan_player1 (intercept on 0x80134A44) answers "nothing come into" at 20183-20188 while
coop_room_change_other(cpu, false) [read].
- Lift that refusal under stayer_window(cpu, 0).
- Add `|| stayer_window(cpu, 0)` to coop_way_out_p2_up (16691). Its only readers are coop_scan_player1's two branches
  (20196, 20241) [read there: refute-goal §3]. Then player 1's own look takes the branch at 20197, which guest-calls the
  scan and marks a way out as waiting (coop_way_look_begin(..., true)) [read 20194-20232].
- The zones-on line at 20210 gains the window: `zones_were == 0u && (coop_p2_scene_on() || stayer_window(cpu, 0)) &&
  !cutscene_playing(cpu)`. Their zones come into as at any time, and the word is put back after the scan (20226-20227)
  [read].

**Player 2 staying.** coop_player_turn skips coop_prompt_look while room_other (hle.c:21000-21003) [read].
- Lift it: `room_other && !stayer_window(cpu, 1)`.
- Add `|| stayer_window(cpu, 1)` to coop_way_out_p1_up (16686). Its only reader is coop_prompt_look (16815) [read]. Then
  their scan waits a way out (coop_way_look_begin(cpu, 1, ..., p1_up), 16856) [read].
- Zones on for their scan: in coop_prompt_look, under stayer_window(cpu, 1), with r13-23040 at 0, their words not up and
  no scene of theirs, write it 1 for the scan and put it back after, as coop_scan_player1 does. The scan asks the zones
  word at 0x80134A80 and on 0 only clears what they stand in (0x80134BE8) [read there: refute-goal 1.12]. Today no door
  writes it 0 before its fn66 as far as read (R5).
- Their turn already runs in the window (room_other, 20869-20870), with their room in [read].

### 2.4 B: examine and pick up

**Player 2 staying.** Once their look runs (§2.3), their B is their own: EXAMINE, DETAIL, words, PICK UP and find (steps
A-C, F) open in their own set, with nothing in the game's busy word [read there: hle.c:13107-13110; coop_find_hold's own
branch 19541-19575]. coop_prompt_look's own gate is their words, the lend screen, their scene, their door turns and
coop_left_behind (16881) [read]. coop_left_behind is false in a resident room under the stay (16244-16253) [read there].
- A script prompt (a use) goes to g_coop_use_script (21035) and runs at player 1's controller (coop_use_start) [read].
- An exit prompt is kept (M11, §5.1). Any other use runs at once, with B6 (§3.6).

**Player 1 staying.** With L1 and P1, the play channel's B row reaches 0x80006EBC(2, 0) in the world frame (0x800084F4)
with the game's copy (player 1's) in.
- Its B path asks no busy word and no walking hold: the 0x80199F50/74 test is only for r4 == 3 (0x80006EE8-0x80006F08)
  [read].
- A door's fn21 hold holds only the three walking channels (0x80199EA0 touches -23060, -23056, -23052) [read], not the
  play channel. So the press goes in [inferred; the frame line prints the play channel's +16 to confirm].
- Their close-up then runs in the game's memory, as player 1's always does. Step B keeps the room going under it, and R4b
  carries it over the arrival (§4.1).
- coop_p1_press_kept (27310-27329) keeps a B only when the shown record is not player 1's B prompt (`!shown_b`) [read].
  With P1 the prompt is on show, so the press route takes the B and the keep does not. To be sure of one route only:
  under stayer_window(cpu, 0), coop_p1_press_kept takes no new B (M11's exit keep is its own, §5.1).

### 2.5 B on an exit prompt

Kept and pressed after the change: M11, §5.1.

### 2.6 Quick spells, the ticket's cast end, and G3

**Player 2 staying.**
- coop_quick_cast's wait at 21090 drops the press under room_other [read]. It becomes
  `(room_other && !(stayer_window(cpu, 1) && tome_copy_mode() == 2)) || coop_left_behind(cpu)`. Step D's refusal stays
  (12482-12486) [read].
- Only with the copy: a cast that asks for the Tome (Enchant) opens their copy in mode 2 (12516-12525). In mode 0 or 1 it
  sets g_coop_quick_tome, which leads to owner 1 and coop_tome_player(true) (27533-27539) [read there: refute-impl 2.6]:
  player 2 the game's current player during player 1's countdown.
- Their cast is a guest call of 0x800069DC with coop_be_player2 (12511-12513) [read]. coop_quick_cast_ask refuses it there
  (19774-19790) [read]. It returns at once under stayer_window(cpu, p2 ? 1 : 0).

**Player 1 staying.**
- With L1 the play channel's quick row reaches 0x800069DC. coop_quick_cast_ask returns at once under stayer_window(cpu, 0).
- 0x800069DC's own gates are none of the ticket's: the node flag 0x4000, the record 0x80307170 +8, flag 307, the
  cooldown r13-26228, and event 185 to the current id [read 0x800069F0-0x80006A78]. Event 185 meets the player's filter,
  which A3 answers "not busy" for the stayer's node [read split_busy_allows 24714-24733].

**Summons (class 0x820) wait for the tail.** The cast picks its class by `(spell & 0x1FF0)` (0x801C43D8) and 0x820 is
the summon (0x801C4438-0x801C443C) [read]. A controlled summon's body returns by a ticket, and a body switch comes from
kind 2's event 250, whose sender is unread [read there: presses-effects §3.5]. So in the window a summon is kept, not
dropped: player 2's by its quick slot index, player 1's by its spell word (coop_quick_cast_ask reads it from r4's word).
Each is cast after the tail by the route it came from (player 2's coop_quick_cast with coop_be_player2; player 1's a guest
call of 0x800069DC(0, COOP_QUICK_SLOT) with the word in the slot, as coop_quick_cast does) [inferred]. It is dropped by a
new ticket or a state load. Read R3 can remove the wait.

**The ticket's cast end.** The writer calls 0x801C5118(0) at 0x801557FC (lr 0x80155800) [read]. It ends the charging
cast of r13-21488 and r13-22336 (0x801F6258; 0x801C5138) [read there: refute-impl §4].
- At player 1's door, r13-21488 is player 1's, the mover's: as solo.
- At player 2's use door, coop_be_player2 put player 2's in: the mover's, as solo.
- At player 2's walk-in door in their turn, the turn swaps only three words (20970-20975), so r13-21488 is player 1's,
  the stayer's [read]. The same at an exit_p2 ticket written with player 1 current [inferred].
- **New intercept `stayer_cast_end` on 0x801C5118:** when lr == 0x80155800, split_counting, and r13-21488 is the
  stayer's id: return 0 and log it (the cast trace's notify on that entry runs only on passthrough, hle.c:5578-5589, so
  the skip is logged by this intercept). Otherwise pass.

**G3 narrowed.** split_stayer_casting (24736) opens no window while the stayer has any spell word. It becomes "a
summon-class spell word" ((spell & 0x1FF0) == 0x820). A record-only cast under way goes on through the window and the
firing: the change calls nothing in the spell code [read there: effects §1], and 0x801C5994 steps it every world frame
[read there].

**0x810 spells hold play.** A spell step calls 0x80199EA0(0) when 0x801D7024 says so (0x801C3754-0x801C376C) [read]: busy
2 and the game copy's walking channels. A stayer's such cast in the window puts bit 2 in the shared word. Its 102 is kept
from the room by S2 while split_on holds (§3.2). At player 1's door the channels held are the mover's, who is locked
anyway [inferred]. Run S7b checks one.

### 2.7 Start: the Tome

**Player 2 staying.**
- Two refusals: the copy ask at 27424 (`!coop_room_change_other(cpu, true)`) and start_room at 27647 [read]. Each becomes
  `coop_room_change_other(cpu, true) && !(stayer_window(cpu, 1) && tome_copy_mode() == 2)`. In mode 0 or 1 the press is
  refused and logged: lifted there, start_pressed reaches the path without the copy (owner 1, coop_tome_player(true),
  27737-27742) [read]. Step D's refusals stay.
- The comment's reason (27642-27646) is a copy "opened in the fade ... showed black in their half once the new room was
  in". Under the stay their half never changes room [inferred, as stayer-readers §2.9]. A copy opened before a ticket
  "stays whole through it" [read the comment].

**Player 1 staying.** The game's Start is refused by the Start lock (L1 keeps it) and by the fade (0x801E7C98) [read there:
gates G-c, G-e]. Their copy opens instead:
- In coop_menu_input's play branch, before `if (g_tome_copy_ask) return;` (27676): a Start edge on `pad` (a static of
  player 1's own) with stayer_window(cpu, 0), tome_copy_mode() == 2, no copy open or asked, and none of player 1's own
  look, words or find up: clear Start from `pad`, set `g_tome_copy_p1 = g_tome_copy_ask = true`.
- That is the route player 1 already has in player 2's close-up (27800-27809) [read]. The copy opens by mode words and
  TW_MODE_CHANGE, not 0x80006CDC, so the Start lock does not refuse it [read there: refute-goal §3].
- With tome_copy_mode() != 2 the Start is refused as today, and logged.
- This route has only run over player 2's close-up. tome_copy_step's player 1 path sets aurora_split_menu(0) and
  aurora_split_only_view(sharing_was ? 2 : 1) (15060-15063) [read]. In the window player 2's half is their door's fade
  (g_fade_mover 2). Run S8 takes shots of both halves.

**The copy carries the game copy's binding lock.** tome_copy_step copies all of cpu->ram at the opening (14977) [read]. Its
after-frame list runs 0x8013E518 (15067-15071), which counts r13-23592 down [read]. At player 1's door the game's copy is
player 1's, locked for ~65 frames, so player 2's copy Tome would take no button for its first ~65 turns [inferred]. **The
change:** right after the memcpy, when split_counting (the copy's turn has not begun), write 0 to the copy's r13-23592
(`g_tome_copy.copy + (sda - 23592 - GC_RAM_BASE)`) and log what it held. A copy's Tome is never the mover's.

**What the copy leaves when it closes in the window.**
- **Casts go at once** (not summons): the window's cast end has already run, and a record crosses the firing.
  - Player 2's: tome_copy_close calls tome_copy_left at once (14887) [read]. A summon-class next_cast is kept to the tail.
  - Player 1's: tome_copy_p1_left waits on coop_busy (27881) [read]. In the window, its cast goes when the busy word holds
    only 4, 2 and 1 and the cast is not a summon. Its script stays pending (the one `pending` flag is split: the cast
    goes, the script waits).
- **Item scripts wait** for the tail. An item's script can ask for a room (a key on a door) or a scene. A room asked in
  the window would be passed over by R4 and the item could be used up for nothing [inferred].
  - Player 1's already waits on coop_busy (27881).
  - Player 2's is kept as `g_stayer.p2_left {pending, script, spell_item}` and run by tome_copy_left(cpu, script, 0, item)
    at the first player 2 turn with !split_counting and !coop_busy. Read R6: where the copy's Use takes the item off their
    list. If the copy takes it, the count goes with the kept script, so a dropped script (a state load) gives it back.
- **Write-back.** The copy's close writes back the magick record, the equips and a script and cast [read there:
  refute-goal 1.10, hle.c:14787-14830, 27846-27857]. A Mix or the item order made in the copy is not known to come back
  [inferred]. This is not new to the window: the copy route has it at any time. Run S8 does a Mix.

### 2.8 A spell from the Tome

A Tome spell is chosen in the copy and cast in the real game when the copy closes (§2.7). For player 2 it is cast with
coop_be_player2 and COOP_CAST (0x801C4250) [read 27938-27943]. coop_quick_cast_ask is only on 0x800069DC, so nothing else
refuses it [read the install, 28959]. Runs S3 and S8 cast one.

## 3. Step B: the room plays on under the stayer's own action

Without these, the stayer's own action would stop their room, and for player 1 the mover's change too. Each was a trap
found by the readers or the refuters.

### 3.1 B1: no "not busy for everyone" in the window

**The fact.** coop_busy_ask (19717-19740) asks, in order [read]:
1. split_busy_kept: K1 (the mover's own filter) and the K7 sites keep the game's word;
2. four cases that answer 0 to **every** caller: (a) g_coop_screen with words or busy, (b) coop_p1_close_up, p1_free
   (busy == 1 exactly), (d) coop_p2_holds;
3. split_busy_allows (A1-A3, only under split_on);
4. the game's word.

Today (b) and (d) never fire in the window: each look's room bit is accepted only for its own player's door (19308,
19670) [read]. So the allow list is all that answers. B3 below lets player 1's look keep the room bit, and (a) can be up for
player 2's lend-route close-up. Then (b) or (a) would answer 0 to the mover's node update and motion (split shape), the
message pump (A2), kind 19 and the K7 kinds inside the room, and every unread asker [read there: refute-impl 2.1;
split_node_freed 24634-24638, split_kind_kept 24626-24629].

**The change.** In coop_busy_ask, when split_counting(cpu) and the switch is on: skip (a), (b), p1_free and (d). Answer
only through split_busy_allows (with B2's split_on), else the game's word.
- The stayer's own node passes A1 (it is in g_split.room, not the mover, not kind 19 or K7), so their controller and
  motion run [read 24634-24637].
- The stayer's look scripts get the game's word, as in solo, where a look's scripts run with busy 2 up [inferred].
- The mover runs by the game's own rule: fn66 cleared their held bit, and coop_node_held_ask frees only the non-asker
  [read 11335-11340, 19747-19766].
- With the switch off, coop_busy_ask is today's.

### 3.2 B2: split_on stays on under the stayer's own bits, statelessly

**The fact.** split_on (24614) is split_counting && coop_room_change_on, which is false with bits 8, 16 or 32 [read]. So a
scene of the stayer's (32) would turn the allow list, S2, S3 and S4 off: their room stops, and the 102 of the scene's busy
set reaches the room [read split_send 24961-24974].

**The change.** `split_on = split_counting && !cutscene_playing && !game_menu_open && (busy & 4) &&
(busy & ~(4|2|1) & ~g_stayer.bits) == 0`, where g_stayer.bits (§1) holds only the bits the stayer set.
- It calls nothing that writes. (The first draft called coop_p1_close_up here, which clears g_coop_p1_look and its static
  `on` when it reads "over" (19311-19318) [read]; asked inside a bracket that zeroed the word, it would end player 1's look
  for good.)
- With the switch off, g_stayer.bits is 0 and split_on is today's.
- coop_room_change_on itself is not changed. Its other readers (room_other, room_turn, coop_node_held_ask, the quick ask)
  see "off" under 8/16/32 as today. That is right: a player in their own scene or held words stands, as in solo.

### 3.3 B3: player 1's look is not read "over" for player 2's room bit

**The fact.** coop_p1_close_up's room_bit accepts bit 4 only for player 1's own door (19308) [read]. At player 2's door,
busy 4|2 reads "over", and g_coop_p1_look is cleared (19313-19318) [read]. R4b keys on it: stay_busy_clear and
split_r4b_case use coop_p1_in_close_up, which is g_coop_p1_look (19247) [read 24898, 25590-25593].

**The change.** `room_bit = coop_room_change_other(cpu, true) || stayer_window(cpu, 0) ? COOP_BUSY_ROOM : 0u`.

**Its guards.**
- B1 above: case (b) no longer answers for everyone in the window.
- hle_coop_controller: p1_look (20310) opens the hold for player 2's turn (coop_hold_open: busy 0, the three channels'
  +16 to 0, r13-25696 0, zones on, 20318-20321) [read]. In the window player 2 is the mover in state 121. With busy 0 in
  their turn, K1 would pass 0 to the mover's filter [inferred]. So: no hold-open for player 2's turn under
  stayer_window(cpu, 0). Their turn runs through coop_door_turns as today.
- p1_look is then true, so room_turn (20342) is off and the game's controller runs for player 1 in their look, as at any
  time [read].

**Player 2: coop_p2_holds is not widened.** The first draft widened it too. Then hle_coop_controller's p2_hold branch
(20324-20333) would run coop_hold_open and coop_player1_turn for player 1, the mover, and replace the game's controller
for them (g_hle_passthrough = !p2_hold && !room_turn, 20379) [read]. coop_p2_holds' room bit stays player 2's own door
only (19670). What player 2's look needed from it is only "enemies leave a held player alone": coop_player_held(cpu, 1)
(19677) gains `|| (stayer_window(cpu, 1) && coop_p2_scene_on())`. g_coop_script_hold is left out of it: it is one bool
that any player's hold step rewrites [read there: design-r4d D0].

### 3.4 B4: player 1's words and finds open with no hold, and keep the door's hold

**The fact.**
- coop_no_pause -> coop_script_hold_go (19339) refuses while `(busy & ~2u) != 0u && busy != (2u | 1)` (19349-19350)
  [read]. Busy 4 is the window's. So today player 1's words take the game's hold (busy 8) and their find takes busy 16.
  Then coop_room_change_on goes off, the allow list goes off, and G2 holds the mover for the new bit [read 24914].
- When it passes with bit 2 up, it lets go: COOP_PLAY_HOLD(1) (0x80199EA0(1): busy 2 cleared, the walking channels'
  +16 rewritten), r13-25696 := 0, r13-23040 := 1, r13-24048 &= ~4 (19352-19363) [read].
- At a door whose hold spans the ticket (script 132: hold[fn21](0), deny_menu(200), wait 120, then fn66, with no let-go
  before fn66 [read `ed script 132 --level 4`]), bit 2 is the door's. g_split.word0 then has bit 2 [read 24818].
- coop_script_hold_go has three callers: coop_no_pause (19377), coop_their_close_up (19385) and coop_player1_turn (19849)
  [read].

**The change.** `coop_script_hold_go(cpu, bool room_ok)` tests `busy & ~(room_ok ? 4u : 0u)`.
- coop_no_pause passes `stayer_window(cpu, 0)`.
- coop_their_close_up passes `false` until C2 (§4.2). Player 2's lend-route words with no hold would set no bit, so G2
  would not wait, and player 1's arrival would cut them (R4d is not built) [inferred from §4.2's facts]. With the hold
  (busy 8) G2 waits for them as today.
- coop_player1_turn passes `false`.
- With room_ok and `(g_split.word0 & 2u) != 0u`: return true with no let-go. The door's hold stays, and player 1 stands in
  their words anyway (the words have the focus) [inferred].
- A let-go that does run sends 102 from player 1 (bit 2 cleared, 4 still up). S2 keeps it from the room's nodes while
  split_on holds, the mover's included in the split shape (split_send keeps any node of g_split.room that is not kind 19
  or K7: 24961-24974) [read]. So no zeroing round it is needed.
- A per-player hold bit is the real fix for a look's end clearing a hold that is not its own (coop-todo.md "Half", open).

### 3.5 B5: player 1 stands in their own look, words and find

**The fact.** room_turn = `!p1_look && !p2_hold && coop_room_change_other(cpu, false)` (20342) [read]. coop_player1_turn
clears the focus for their turn (r13-23552, -23556 := 0: 19862-19865) [read]. So under no-hold words they would walk.

**The change.**
- room_turn gains `&& !(g_coop_words_free || g_coop_find_free || g_coop_p1_look || (r13-25960 != 0 && !g_coop_words_theirs))`.
  Then the game's own controller runs for them that frame (passthrough at 20379), and they stand as at any time.
- When room_turn comes back after such a pause, it starts from the game's copy: set g_coop_p1_turn_fresh with
  g_coop_p1_from_free false. g_coop_p1_free is saved only while the busy word is 0 (20301-20304) [read], so in the window
  it is from before the ticket, and room_turn would bring back the stick and buttons of before the door (20362-20363,
  20375) [read].

### 3.6 B6: player 2's uses in the window are not "a close-up of theirs" by busy 4

**The fact.**
- coop_use_start sets g_coop_assist_at = 1 for any use that is not a close use (18213) [read].
- coop_menu_input: `closeup = ... (owner == 1 || (g_coop_assist_at != 0u && !g_coop_script_hold)) && coop_busy(cpu) &&
  !coop_p1_in_close_up(cpu)` (27579). coop_busy is `busy != 0` (3974) [read]. Busy 4 alone makes it true. Then owner 1,
  and the game's pad becomes player 2's [read there: 27775-27780, 27831].
- continue_coop_use: with no new ticket, `busy_new = close_use ? (busy & ~busy_before) != 0 : coop_busy(cpu)` (18274), and
  then g_coop_screen and coop_hold_player1() (18275-18277) [read].
- Also g_coop_script_hold is cleared only once busy is 0 (27573) [read], so one set before the window stays through it.

**The change.** Under split_counting, a use is "up" only by the bits it set itself:
- continue_coop_use: `busy_new = (busy & ~g_coop_use.busy_before) != 0u` (the close-use form). busy_before was read at
  coop_use_start (18190) and holds the window's 4 [read].
- coop_menu_input's closeup tests `(busy & ~COOP_BUSY_ROOM) != 0u` in place of coop_busy(cpu) under split_counting.
- The `!coop_busy` clear of g_coop_script_hold (27573) tests `(busy & ~COOP_BUSY_ROOM) == 0u` under split_counting.
- Then a use that opens nothing (a switch, a flag) runs and is over at once. A use that opens a close-up (busy 2) or a
  scene (32) is player 2's as at any time, and G2/G2b wait for it (§4.2).

## 4. The firing: what crosses it, what waits for it

The tick is the world frame's third call (0x800083EC, after 0x80020B08 and 0x801E3CB0) [read `ed dis 0x800083D8`]. It is
before the behaviour pass and the play channel, so no press is cut in half by the firing.

### 4.1 Player 1 staying (player 2's door), split or apart

- The change runs in R4's bracket: player 2's identity, casting words, cameras and prompt slots in, player 1's slots kept
  aside and put back [read there: effects §3.1].
- At the arrival: stay_waits_clear keeps player 1's waits (25565-25588); stay_busy_clear keeps their bits (all but 4)
  under their close-up or scene (25590-25612); coop_room_change_words leaves their words for player 2's door [read].
- B3 keeps their look up through the window, so R4b carries it. Their no-hold words and finds set no busy bit (B4).
- G2 does not wait for player 1's close-up or scene (split_r4b_case, 24898) [read]. So player 2 never waits in the black
  for player 1, and their 120 door turns are not used up.
- Whether the arrival closes a words window of player 1's that is up with no hold is not read (read R7). Run S6w shows it.
- Split: the stayer's room is the mover's old room. The arrival's event 61 goes to the room left [read there]. That is the
  split hold's known next stop (design-split-hold §8).

### 4.2 Player 2 staying (player 1's door), split or apart

- The change runs with player 1 in. Player 2's own set (words, close-up, find, looks, script states, cameras, input) is
  host memory then, and the change does not reach it [read there: effects §1].
- Their quick-spell record goes on. Their copy Tome ticks no world frame, so it crosses [read 15060-15067].
- **What waits for the firing (G2, G2b), until C2:** a scene of theirs (32, in the game's word), and a lend-route use,
  close-up or words of theirs. Player 1's arrival clears every script wait (0x80163F04(1)) and every busy bit
  (0x801556CC(-1, 0)), with no bracket for player 2, and cancels the current close-up (0x801172B0) [read there: effects
  §1, §3.3].
  - G2 waits today for 8, 16 and 32 begun in the window (24914) [read].
  - **G2b (new):** for mover 1 it also waits while g_coop_screen was not up at the ticket and is up now. A lend-route
    close-up sets only bit 2, which G2 masks. After B6, g_coop_screen is up only while their use really has something up.
  - The mover's lock is renewed through the wait (24937-24941) [read]. Player 1 waits in the black. After
    SPLIT_WAIT_LONGEST (900 frames) the countdown goes on under it (24921-24928) [read], and the action is then cut.
- **C2 (the next build, needs read R9):** R4d's D0 (a per-player script hold), D2 (the arrival's wait clear keeps player
  2's waits), D3 (its busy clear keeps player 2's bits) and D5 (player 1's own set in for the change if the cameras are
  lent), widened from "apart at the ticket" to any window with player 1 the mover. Then G2 and G2b stop waiting for player
  2's things, coop_their_close_up passes room_ok, and nothing of either stayer holds the other's door.
- Split: after the firing they are apart. Player 2's turns run with their room in [read 20980].

### 4.3 12-frame zone exits (transition 2)

The same rules. G2 waits only while +0 > 1 (24914) [read]. A press 2-8 frames after the ticket meets the same gates
[inferred]. Run S18 checks one.

## 5. Room requests in the window

The game holds one ticket record (0x8058AFFC). A second fn66 in the window is passed over by R4 (16761-16769) or its
ticket refused by G1 (24827-24850) [read]. So the stayer's room requests are kept and made after the tail. Each waits at
most the window, ~62 frames.

### 5.1 M11: B on an exit prompt is kept

- **Player 2:** at hle.c:21033-21036, when stayer_window(cpu, 1) and coop_prompt_is_exit(g_coop_prompt) (ids 1, 2, 3, 4,
  10: 15412-15414) [read]: `coop_press_keep(&g_coop_p2_kept, 2, g_coop_prompt, g_coop_prompt_script, "player 1's door is on
  its way")` instead of g_coop_use_script. Its release (21054-21055) gains `|| split_counting(cpu)` in the wait test.
- **Player 1:** in coop_menu_input's play branch, after coop_p1_press_kept (27668): when stayer_window(cpu, 0), B is new on
  `pad`, and the shown record (PROMPT_SLOTS + 6*28: +4 button 2, +8 id, +12 script) is an exit: clear B from `pad` and
  `coop_press_keep(&g_coop_p1_kept, 1, id, script, "player 2's door is on its way")`. Its release (27352) waits on
  coop_busy, which holds through the window [read].
- The drop rule `kept->tickets != g_coop_tickets` keeps it: the door's ticket was counted before the press (11362) [read].
- In a split, the stayer's kept OPEN DOOR goes in after the mover is through, and the stayer follows by their own ticket.
  Apart, it opens their own door.

### 5.2 M12: a way out walked into waits

- coop_script_go_to_room (16760) tests R4, then g_follow_refusing, then the way-out wait [read 16761-16786]. Today a way
  out in the window is passed over by R4, and its zone stays marked inside until the player steps out and back
  (coop-no-pause.md row 66) [read].
- **The change:** a new first test: compute `waits` as today, and `if (stayer_window(cpu, g_coop_way_look.who) && waits)
  { coop_way_out_wait(cpu); r3 = 0; return; }`. The rest keeps today's order. Follow mode and a stay with no window are
  unchanged. (The first draft moved the wait test above R4, and so above follow mode's way-back refusal too.)
- The wait is let go after the window: player 1's at coop_way_out_p1_go (20160-20163), player 2's in coop_prompt_look
  (16816-16817), once p1_up / p2_up are false and (for player 2) the busy word is 0 [read]. The release clears the zone's
  inside bit, so the next look comes into it by the game's own path (16739-16758) [read].

### 5.3 M13: a room asked later by the stayer's own look or use

- Many ways out begin at EXAMINE. Script 103 (room 183's ladder, L01/103): EXAMINE, hold, busy_set(1), DETAIL, words with
  Yes/No; on Yes `busy_set(0)`, `hold(1)`, `state_set(0)`, then fn66 [read `ed script 103 --level 1`]. A lever's script
  can end in fn66 too.
- A Yes inside the window meets R4 at fn66. The script has already reset its state, so the answer is lost. No script reads
  fn66's return [read there: transitions.md "What fn66 does"].
- **The change (after read R1):** in coop_script_go_to_room, under stayer_window(cpu, who) with the current id the
  stayer's: keep fn66's arguments and the asker, answer 0, and make the request again after the tail as that player.
  fn66 reads its arguments only when it has 6 or 7; otherwise it carries on with whatever r21-r28 hold (0x801666C8-
  0x801666DC, 0x80166788) [read there: `ed fn 0x801666A4`]. So the game's own fn66 can be guest-called with r21-r28 set
  from the kept arguments [inferred: R1 reads that path, and what r3 must hold]. Until M13 is built, the loss is printed
  every time (§7).

## 6. What stays refused or deferred, and why

| Action | What happens | Why (from the code) |
|---|---|---|
| A way out the stayer walks into | waits; fires at their next look after the change (M12) | one ticket record 0x8058AFFC; a second fn66 is passed over (16761) or refused (G1, 24827) [read] |
| B on the stayer's exit prompt | kept; goes in after the change (M11) | the same |
| A room asked later by the stayer's look or use (the ladder's Yes) | until M13: passed over and printed; with M13: asked again after the tail | the same; the script reset its state before fn66 [read script 103] |
| Player 1's real Tome (mode 3) in player 2's countdown | refused by the game's Start lock; their copy opens instead | solo's own rule (0x80006D00) [read there]; game_menu_open turns every room rule off (11348) [read] |
| Any Start or Tome-asking cast in the window with ED_COOP_TOME_COPY 0 or 1 | refused and logged | the path without the copy makes player 2 the game's player (27737-27742, 27533-27539) [read] |
| Enchant Item by player 1's quick spell with no item chosen | the cast asks 0x80006CDC(0, 1); the Start lock refuses it; the spell word is cleared and event 176 sent | 0x801C4560-0x801C45B4 [read there]; a copy route for player 1's "choose an item" is read R8 |
| A summon (0x820) pressed by the stayer | kept; cast after the tail | its body and return paths are unread (R3) |
| A summon charging at the ticket | no window (G3): the stayer's room holds as today | the same |
| Player 2's scene, lend-route use, close-up or words at player 1's door | runs at once; player 1's change waits for it (G2, G2b) | R4d is not built: player 1's arrival would clear its waits and bits (C2 removes the wait) |
| The stayer's Tome item script | runs after the window | an item's script can ask for a room (one ticket) |
| A cinematic begun in the window | G2 waits; its own return ticket is lost (risk 12) | as design-split-hold §19.6 |

## 7. Logging

Under split_log (ED_COOP_LOG or ED_COOP_ENEMY_LOG), prefix `[stayer]`, once per (kind, player) per window:
- at the window's open: `[stayer] player N stays: their presses go on (ED_STAYER_PRESSES=...); bits at the ticket 0x.. (the stayer's 0x..)`;
- L1: `[stayer] player 2's door: the input lock 65 kept out of player 1's copy (the Start lock set) (frame F)`; at the
  over: `[stayer] the Start lock put back to N (was M)`;
- `[stayer] player N's zone look ...`; `... B on prompt P (script S) ...`; `... quick spell 0x... ...`; `... Start: their Tome in a copy ...`;
  `... the copy's input lock cleared (was N)`;
- `[stayer] the ticket's cast end passed over: the main id is player N's, the stayer's`;
- `[stayer] player N's way out waits for player M's door`;
- `[stayer] player N set busy 0x..` (g_stayer.bits) and `[stayer] player 2's use is up by its own bits only: 0x..` (B6);
- the over line (split_over) gains `stayer: looks n, B n, casts n, Starts n, kept n, ways n, prompts unmasked n, bits 0x..`.

Always printed:
- `[stayer] A ROOM ASKED FOR BY PLAYER N'S SCRIPT S IN PLAYER M'S DOOR: PASSED OVER (one ticket record)` (until M13);
- `[stayer] player N's summon waits for player M's door`;
- `[stayer] PLAYER N'S START REFUSED: no Tome copy (ED_COOP_TOME_COPY != 2)`;
- `[stayer] THE COUNTDOWN WAITS FOR PLAYER 2'S <scene|use|words> (G2/G2b): PLAYER 1 IN THE BLACK` (until C2).

A frame line, `ED_STAYER_LOG=1`, once per window frame. It settles the [inferred] points: the game copy's r13-23592,
r13-26232, r13-24048, r13-23344, r13-25696, r13-23040; the play channel's +16 (from r13-26224) and the focus r13-23552; the
shown record's id and button; g_coop_use.active / .p2, g_coop_screen, g_coop_p1_look, g_coop_words_free, g_coop_find_free;
player 2's g_coop_prompt; whether room_turn ran; g_stayer.bits; both players' positions.

## 8. The switch, state load, saves

- `ED_STAYER_PRESSES`: unset or `1` both directions, `p1` player 1 staying only, `p2` player 2 staying only, `0` off.
  Off, every refusal of today stands, coop_busy_ask and split_on are today's, and the runs can compare. It needs the
  split hold's window (ED_SPLIT_HOLD unset, 1 or `split`).
- **Quick state load:** split_state_load (25007) zeroes g_split [read]. It also zeroes g_stayer, the kept summons and
  room requests, and player 2's kept Tome item. A kept press made in a window and a way out waiting from one are dropped
  (both carry the tickets count, which a load does not reset).
- tw_state_loaded (13480-13490) already closes both copies [read].
- Nothing of this straddles a save: hle_save_held is unchanged.
- **In-game saves:** not designed here. The user has not decided how saving works (together, apart, or either). A save
  prompt pressed by the stayer in a window opens a game mode, and a mode change turns coop_room_change_on off (11348)
  [read]. For now a save prompt in the window is kept as an exit is (M11) [inferred: it is not an exit id; read with the
  save design].

## 9. Solo and follow mode

- Every change starts from stayer_window -> split_counting -> g_split.on, or from split_counting with the switch on.
  g_split.on needs mod_coop, g_coop_p2, coop_stay and g_split_ready (24781-24782) [read].
- The two new intercepts (0x800443E4, 0x801C5118), the coop_flags_ask and coop_prompt_choose branches and the
  g_stayer.bits record in stay_busy_clear pass through without it.
- coop_script_hold_go's new argument is false outside a window. M12's new test needs the window; today's order is kept.
- Solo, follow mode and co-op without ED_COOP_STAY run today's code. Runs S12-S14 check it.

## 10. Runs (fresh Jump-to-Game states, as build-run-split-hold.sh)

Frames follow the split-hold runs: player 1's door by ED_RUN_SCRIPT=110:119 ticketed at ~3822 (window to ~3884); player
2's OPEN DOOR B at 3760, ticketed the same frame (window 3760 to ~3822) [read there: refute-impl §3, page-0001.log]. Each
run checks its own `[split] player N's door` frame first, and the presses move with it.

Pads: ED_INPUT_SCRIPT=start,end,buttons[,x,y,analog] for player 1 (B 0x0200, Start 0x1000, D-pad up 0x0008, right
0x0002); ED_PAD2_SCRIPT=start,end,id for player 2 (b, start, up, right, lup); ED_RUN_SCRIPT=DELAY:SCRIPT;
ED_PUT=frame,who,x,y,z (floor-collided).

Places (`ed room 183`) [read there: coop-level3-design.md:834, 871]: CameraRgn05, the EXAMINE close-up (Camera06), at
Puzzle_Mkr04 (1305, 1508, 185); Region01, the 183 -> 180 OPEN DOOR, box (183..541, 2994..3194); BlockRegion01, the PICK
UP block, box x 215..504, y 195..485. The ladder L01/103 and a door that holds through its ticket (script 132, level 04)
are placed with `ed room` / `ed script 132` before their runs.

```sh
#!/bin/sh
# The stayer keeps every action through the other's door countdown (design-stayer-presses.md). Fresh states first.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_CHANGE_TIME=1 ED_RUN_SHOTS_MS=250
L="ED_COOP_LOG=1 ED_COOP_ENEMY_LOG=1 ED_CHANGE_HOLD=1 ED_STAYER_LOG=1 ED_COOP_TRACE=1 ED_COOP_CAST_TRACE=1"
rm -f saves/rooms-ch1-183.state saves/rooms-ch1-344.state
JTG_SLOT=1 env -u ED_COOP_STAY ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29 ED_SAVE_STATE=3650:saves/rooms-ch1-183.state ED_RUN_FRAMES=3750 sh tools/ed/jtg.sh 1 stayer-s0-183 </dev/null
SET=$(ls -d logs/run-*-jtg-stayer-s0-183 | tail -1)/settings.ini; export ED_SETTINGS="$SET"; ST=saves/rooms-ch1-183.state
P1GO="ED_RUN_SCRIPT=110:119,170:119"                       # player 1's door (183 -> 180), ticket ~3822
D2="3760,3770,b"                                            # player 2's OPEN DOOR, ticket 3760
# --- player 2 stays (player 1's door), split ---
# S1 examine and words across the firing: EXAMINE ~3830, DETAIL ~3850 (words open in the window), RETURN ~3950.
env $L $P1GO ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1508,185" ED_PAD2_SCRIPT="3828,3834,b;3850,3856,b;3900,3906,b;3950,3956,b" sh tools/ed/run.sh $ST stayer-s1-p2-examine </dev/null
# S2 quick spell; S2b the same spell begun before the ticket (L3: a record already under way).
env $L $P1GO ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4200 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1000,185" ED_PAD2_SCRIPT="3840,3846,up" sh tools/ed/run.sh $ST stayer-s2-p2-quick </dev/null
env $L $P1GO ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4200 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1000,185" ED_PAD2_SCRIPT="3810,3816,up" sh tools/ed/run.sh $ST stayer-s2b-p2-cast-before </dev/null
# S3 Start: their copy Tome, a page, a spell cast from it, then out.
env $L $P1GO ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1000,185" ED_PAD2_SCRIPT="3830,3836,start;3860,3866,right;3880,3886,b;3900,3906,b;3940,3946,b" sh tools/ed/run.sh $ST stayer-s3-p2-start </dev/null
# S4 zone look: put outside CameraRgn05, step, put inside in the window, step (failed-attempts 566), then B.
env $L $P1GO ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1000,185;3845,2,1305,1508,185" ED_PAD2_SCRIPT="3700,3714,lup;3850,3864,lup;3870,3876,b" sh tools/ed/run.sh $ST stayer-s4-p2-zone </dev/null
# S5 exit kept: both at the 183 -> 180 door; player 1 leaves by script, player 2's B on OPEN DOOR in the window.
env $L $P1GO ED_RUN_FRAMES=4400 ED_PUT="3680,1,1110,1170,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3840,3846,b" sh tools/ed/run.sh $ST stayer-s5-p2-exit </dev/null
# S5b pick up in the window: PICK UP, the found words, close.
env $L $P1GO ED_RUN_FRAMES=4400 ED_PUT="3680,1,1110,1170,185;3680,2,360,340,185" ED_PAD2_SCRIPT="3830,3836,b;3860,3866,b;3990,3996,b" sh tools/ed/run.sh $ST stayer-s5b-p2-pickup </dev/null
# S5c a use by player 2 in the window (B6): a script prompt that is not an exit (the lever of `ed test`'s lever-* room).
# S7b a 0x810 spell by player 2 in the window (pick the spell word from docs/game/systems/magick).
# --- player 1 stays (player 2's door), split ---
# S6 examine; S6w words across player 2's firing (EXAMINE 3770, DETAIL 3790, RETURN 3880).
env $L ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3770,3776,0x0200;3790,3796,0x0200;3830,3836,0x0200;3880,3886,0x0200" sh tools/ed/run.sh $ST stayer-s6-p1-examine </dev/null
# S6b player 1's pickup across the firing (P4's no-hold find).
env $L ED_RUN_FRAMES=4400 ED_PUT="3680,1,360,340,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3770,3776,0x0200;3800,3806,0x0200;3900,3906,0x0200" sh tools/ed/run.sh $ST stayer-s6b-p1-pickup </dev/null
env $L ED_COOP_P1_QUICK=00010504 ED_RUN_FRAMES=4100 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3775,3781,0x0008" sh tools/ed/run.sh $ST stayer-s7-p1-quick </dev/null
# S8 Start: their copy, a page, a cast from it, a Mix if the chapter has one, close; shots of both halves.
env $L ED_RUN_FRAMES=4200 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3770,3776,0x1000;3790,3796,0x0002;3805,3811,0x0200;3830,3836,0x0200;3870,3876,0x0200" sh tools/ed/run.sh $ST stayer-s8-p1-start </dev/null
env $L ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1000,185;3680,2,360,3090,185;3772,1,1305,1508,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3700,3714,0,0,127;3775,3789,0,0,127;3800,3806,0x0200" sh tools/ed/run.sh $ST stayer-s9-p1-zone </dev/null
env $L ED_RUN_FRAMES=4400 ED_PUT="3680,1,250,3050,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3775,3781,0x0200" sh tools/ed/run.sh $ST stayer-s10-p1-exit </dev/null
# --- apart ---
# S11 player 2 stays: player 1 to 180, then 180's ladder 28 (ticket ~4132) while player 2 in 183 examines and casts.
env $L ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4700 ED_PUT="3680,1,1110,1170,185;4100,2,1305,1508,185" ED_RUN_SCRIPT=110:119,170:119,420:28,480:28 ED_PAD2_SCRIPT="4140,4146,b;4160,4166,up;4240,4246,b" sh tools/ed/run.sh $ST stayer-s11-apart-p2 </dev/null
# S11b player 1 stays: player 2 to 180 by the door, then 180's ladder by B; player 1 in 183 examines, casts, Start, picks up
#   (180's ladder place from `ed room 180` first).
# --- doors that hold play through their ticket, one per direction (refute-impl 2.3, 2.8) ---
# S15/S16 script 132's ladder (level 04): the mover's hold spans the ticket (g_split.word0 & 2); the stayer examines and
#   opens words in the window. S17 L01/103: the stayer's own ladder Yes in the other's window (M13, or today's printed loss).
# --- the 12-frame exit and player 2's walk-in door ---
# S18 player 1 walks into a zone exit (transition 2) while player 2 presses B 4 frames after its ticket.
# S19 player 2 walks into a way out in their turn (L1's g_coop_turn case; stayer_cast_end with player 1 charging a cast).
# --- off, follow, solo ---
env $L ED_STAYER_PRESSES=0 $P1GO ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=4200 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1508,185" ED_PAD2_SCRIPT="3835,3841,b;3840,3846,up" sh tools/ed/run.sh $ST stayer-s12-off </dev/null
env -u ED_COOP_STAY $L $P1GO ED_RUN_FRAMES=4300 ED_PUT="3680,1,1110,1170,185;3680,2,1305,1508,185" ED_PAD2_SCRIPT="3835,3841,b" sh tools/ed/run.sh $ST stayer-s13-follow </dev/null
unset ED_SETTINGS
JTG_SLOT=1 JTG_SET="mod_coop=0" env $L ED_RUN_SCRIPT=1950:101,2010:101,2350:105,2410:105,2750:111,2810:111,3150:29,3210:29,3820:119,3880:119 ED_RUN_FRAMES=4300 sh tools/ed/jtg.sh 1 stayer-s14-solo </dev/null
echo RUNS DONE
```

The script is saved as docs/research/separate-rooms/build-run-stayer-presses.sh once the build exists, and copied before
a queued run uses it (coop-failed-attempts.md:551). Chapter 1's Pious may have no Tome magick: then a cast shows only as
its words 0,13 (the gate is open), and S2/S3/S7/S8 are run again from a slot whose character has spells.

What each run must show (`ed timeline`, EVENTS.log, page-*.log, both.tsv, shots/):

| Run | Must show | Must not show |
|---|---|---|
| S1 | `[stayer] player 2's zone look`; ~3828 `[coop] player 2's own close-up: press id ...`; ~3850 `player 2's own words open in their copy`; the words still up after ~3884; RETURN ~3950; the right half on Camera06 (shots); the mover's position moving from 3822 to ~3850 (both.tsv) | `player 2's turn refused: busy`; `own close-up cancelled` at ~3884; a G2 wait |
| S2 / S2b | `[stayer] player 2's quick spell 0x00010504`; `[cast-trace]` for player 2's record before and after ~3884; S2b: a window opens (no G3 line) | `player 2's quick spell waits`; `[stayer] the ticket's cast end passed over` (not player 2's id here) |
| S3 | `[tome-copy] player 2's Tome opens in the copy`; `[stayer] the copy's input lock cleared (was ~6x)`; a page change at ~3860; a cast from the Tome; `player 2's Tome closes`; the right half shows room 183 after (shots) | `player 2's Start in player 1's room change: no Tome`; a black right half |
| S4 | player 2's EXAMINE prompt between 3845 and 3884; Camera06 in their half; their close-up at 3870 | `[stay] a room asked for ... passed over` |
| S5 | `[coop] player 2's B on prompt N script 119 kept: player 1's door is on its way`; after ~3894 `player 2's kept B goes in`; a second `[split] player 2's door from room 183 (to room 180)`; both in 180 | G1's `second ticket refused`; R4's `passed over` |
| S5b | `player 2's own find opens`; the item on the list once (`hle_coop_found`); the find's close and its script after the firing | a lost item, a second copy, words left open |
| S5c | `[stayer] player 2's use is up by its own bits only`; no `player 2 has the close-up` for a use that opens nothing; a use that opens one: `[stayer] THE COUNTDOWN WAITS ... (G2b)` | a 900-frame wait; the game's pad given to player 2 with nothing of theirs up |
| S6 / S6w | `[stayer] player 2's door: the input lock 65 kept out of player 1's copy`; the frame line: game copy r13-23592 0, r13-24048 0x10000 kept, the shown record player 1's EXAMINE; ~3770 player 1's close-up (Camera06 in the left half); ~3790 `player 1's words open with no hold`, still open after ~3822; `[stay] player 2's change keeps player 1's busy 2` if the close-up is up at the firing; RETURN ~3880; room 183's zombies `step` > 0 through 3760-3822 | `[trace] player 1's look over` between 3770 and 3822; a G2 wait; room_turn moving player 1 in their close-up or words (both.tsv); the Start lock above 0 at ~3830 |
| S6b | player 1's PICK UP, find words and the item once, across ~3822 | busy 16 in the frame line; a G2 wait |
| S7 / S7b | `[stayer] player 1's quick spell`; `[cast-trace]` for player 1's record across ~3822; S7b: the 0x810 spell's bit 2 in the frame line, the room still playing | `player 1's quick spell waits` |
| S8 | `[stayer] player 1's Start: their Tome in a copy`; `[tome-copy] player 1's Tome opens in the copy`; the frame line's game menu 0 throughout; a cast from it; the close; player 1 standing in the world while it is open (both.tsv); both halves right (shots) | the real Tome (mode 3) |
| S9 | player 1's EXAMINE prompt shown inside the window (frame line's shown id); its close-up at ~3800 | `nothing come into` for player 1 |
| S10 | `[coop] player 1's B on prompt N script 119 kept: player 2's door is on its way`; `player 1's kept B goes in` after ~3832; player 1's own ticket; both in 180 | the kept B dropped as `a room was asked for` before it went in |
| S11 / S11b | apart: `[split] player N's door ... player M stays in room 183 (apart)`; the stayer's close-up, quick spell, Start and pickup | as S1-S8 |
| S15 / S16 | `busy before 0x2` at the ticket; the stayer's words open; no let-go line for the door's hold (`[stayer]` frame line keeps bit 2 to the firing) | the mover's zones word turned on mid-door; the mover hit or moved |
| S17 | the ladder's Yes in the window: with M13 the kept request and the stayer's own ticket after the tail; without it the always-printed loss | the stayer standing with no line |
| S18 / S19 | the press goes in inside a 12-frame window; S19: `[stayer] the ticket's cast end passed over` and the lock in player 2's copy only | player 1 locked at player 2's walk-in door |
| S12 | today's lines: `player 2's quick spell waits`, no look in the window | any `[stayer]` line |
| S13 | follow mode as today: no `[split]`, no `[stayer]` lines | |
| S14 | solo: none of the co-op lines; the door as solo | |

Then `ed test` (the core loop: urn-p1/urn-p2, find-p2-desk, lever-*, both-walk), `ed check` with no new findings, and no
`[guest-call] ... stopped`.

## 11. What only the user's play can check (and the exact presses)

Scripted presses cannot test these: controls.cpp (ED_INPUT_SCRIPT goes round it, coop-failed-attempts.md:151); a readied
attack (ED_INPUT_SCRIPT's R sets only the digital bit, 551); the feel of no hitch; a chapter with the spells and items the
runs lack.

In any chapter, in a room with a door and something to examine, co-op on, separate rooms on:

1. **Player 2 takes a door, player 1 stays (split).** Player 2 presses the action button on OPEN DOOR. In the second
   before the room goes black for player 2, player 1:
   a. presses the action button on an EXAMINE beside them, then DETAIL, and RETURN after the door is done;
   b. presses a quick spell (the D-pad direction that holds one);
   c. presses Start, turns a page, casts a spell from the Tome, and closes it;
   d. holds R and attacks a creature.
   Each must work at once. Player 1's half must not stop, and player 2 must not wait in the black.
2. **Player 1 takes a door, player 2 stays (split).** The same four, with the roles swapped. One known wait is left here
   until C2: if player 2 pulls a lever or starts a scene during the door, player 1 waits in the black until it ends. Say
   how long it felt.
3. **Apart.** One player goes through a door first. Then that player takes a second door while the other, in the first
   room, does a to d.
4. **Both at one door.** One player opens a door. During its second, the other presses the action button on the same
   OPEN DOOR. They must follow by their own door about a second later, not stand.
5. **A walk-in exit.** One player walks into a zone exit (no prompt). The other walks into a zone (a camera change or an
   EXAMINE) at the same moment.
6. **A pickup.** During the other's door, the stayer picks up a thing (PICK UP, the found words, close). The item must be
   on the list once.
7. **A ladder that asks Yes/No.** During the other's door, the stayer answers Yes at a ladder. Say whether they go down
   after the other's door, or stand.

Tell me the moment if any of them does nothing. The logs carry a `[stayer]` line for each press that went through, and
an always-printed line for each one refused.

## 12. Reads still needed before building

Step A and Step B can be built after R5. C2 needs R9; M13 needs R1.

1. **R1 (M13):** fn66's entry 0x801666A4-0x80166788: how it counts its arguments, what r3 must hold, and whether a guest
   call with r21-r28 set makes the same request (the gate 0x800440D0, the let-go 0x801F6828, the motion 0x801669AC, the
   ticket, the fade, the sound fade).
2. **R2:** whether player 2's shown prompt icon goes through 0x80117628 with the shared word (run S4 settles it). If it
   does, their chooser gets the P1 wrapper under stayer_window(cpu, 1).
3. **R3:** which spell sends kind 2's event 250 with data 2 (0x800C46BC). Then the summon wait can narrow or go, and G3 with
   it.
4. **R4:** the copy Tome's frame with busy 4 and the ticket record in its memory: whether any of the 35 busy asks refuses a
   page (S3 settles it).
5. **R5:** which door scripts call fn139 (bit 4 of r13-24048, the zones word 0, r13-25696) or fn22 before their fn66
   (`ed uses fn139`, `ed uses fn22`, crossed with fn66 in the same state). If any do, P1 must mask the mover's bit 4 by a
   per-player record of who called fn139, and §2.3's zones-on covers both players.
6. **R6:** where the copy Tome's Use takes an item off player 2's list (in the copy, or in the item's script).
7. **R7:** whether player 2's arrival (0x801172B0, 0x80151FDC) closes a words window of player 1's that is up with no hold
   (S6w settles it).
8. **R8:** a copy route for Enchant's "choose an item" from player 1's quick spell: 0x801C4560-0x801C45B4 and what the copy
   writes back for an enchanted item.
9. **R9 (C2):** whether design-r4d.md's D0, D2, D3 and D5 hold when both players were in one room at the ticket, and what
   the arrival does to player 2's lend-route close-up camera in the split shape.

Settled since the first draft: g_coop_use.active ends when the door script returns (the use is a tail call into
COOP_RUN_SCRIPT with lr HLE_USE_RETURN; continue_coop_use clears it, 18233-18245) [read]. The play channel's +16 and focus
in player 2's countdown are printed by the frame line (S6).

## 13. Refuted or corrected

| # | Point | What the code showed | Verdict | What changed |
|---|---|---|---|---|
| I-2.1 / G-1.3 | P3 brings back coop_busy_ask's "not busy for everyone" in the window | cases (a), (b), (d) answer 0 to every caller before the allow list (19728-19734); A1 leaves out the mover, K7 and kind 19 (24634-24638) [read] | real | B1: under split_counting those cases give way to the allow list |
| I-2.2 / G-1.2 | P3's mirror: p2_hold gives the mover a turn with the hold let go | p2_hold runs coop_hold_open and coop_player1_turn for player 1 and replaces their controller (20324-20333, 20379) [read] | real | coop_p2_holds is not widened (§3.3); only coop_player_held(1) gains the window |
| I-2.3 | P4's let-go and a look's end clear the door's bit 2 | the let-go clears busy 2, the walking channels, 25696, sets zones 1, clears 24048 bit 4 (19352-19363); script 132 holds through its fn66 [read] | real for doors that hold through the ticket | B4: no let-go when g_split.word0 has bit 2; the per-player hold bit stays open; runs S15/S16 |
| I-2.4 | P1's chooser branch pre-empts player 1's look branch; prompts0 masks out bit 4 | the look branch opens the look channel round the call (19928-19944) [read] | real | one wrapper round today's code (§2.2) |
| G-1.11 | door_bits' bit 4 has the wrong source | bit 4 is fn139's (0x80042B7C-0x80042B90); g_coop_script_hold is fn21's (0x80199EA0) [read] | real | bit 4 dropped from door_bits; R5 reads whether a door sets it |
| I-2.5 / G-1.1 | player 2's use in the window becomes "their close-up" by busy 4 | assist_at = 1 (18213); closeup and busy_new by coop_busy (27579, 18274) [read] | real | B6: under split_counting a use is up only by its own bits |
| I-2.6 | player 2's Start or Tome-asking cast without the copy | the path without the copy: owner 1, coop_tome_player(true) (27737-27742, 27533-27539) [read] | real, ED_COOP_TOME_COPY 0 or 1 only | lifted only with tome_copy_mode() == 2; refused and logged otherwise |
| I-2.7 | the kept-B route doubles player 1's B | the keep needs `!shown_b` (27323); with P1 the prompt is on show [read]; doubling needs lent cameras or a shown record not theirs | corrected: a maybe, not shown real | under the window coop_p1_press_kept takes no new B; M11 keeps exits |
| I-2.8 | no run meets a door that holds play | 183 -> 180 has busy 0x4 alone [read there: log] | real | runs S15-S17 |
| I-3 / G-1.4 | moving the way-out wait above R4 also moves it above the follow refusal, unswitched | order R4, follow, waits (16761-16786) [read] | real | M12: a new first test under the window only |
| I-3 | 0x810 spells hold play | 0x801C3754-0x801C376C calls 0x80199EA0(0) [read] | real call; harm a maybe | S2 keeps its 102 under split_on; run S7b |
| I-3 | L3 has no run for a cast under way at the ticket | — | real (test plan) | run S2b |
| I-3 | player 1's copy outside player 2's close-up never run | its views are set "over player 2's close-up" (15060-15063) [read] | maybe | S8 takes both halves |
| I-3 / G-1.8 | P5 calls a stateful test from split_on | coop_p1_close_up clears g_coop_p1_look and `on` (19311-19318) [read] | real | B2: split_on by g_stayer.bits, writes nothing |
| I-3 | the Start lock outlives the window under words | it counts only with r4 == 1 (0x80006900-0x80006964) [read there: b3 §3] | real by reading | put back at split_over (§2.1) |
| I-3 | coop_script_hold_go has a third caller | coop_player1_turn, 19849 [read] | real, small | room_ok false there |
| I-3 | the tick is not the world frame's first call | 0x800083E4 and 0x800083E8 come first [read] | real, harmless | §4 says third |
| I-3 | player 2's OPEN DOOR ticket at 3760, not ~3763 | press, script and ticket in frame 3760 [read there: log] | real | §10 frames from 3760 |
| I-3 | the cast-end skip is not traced | the notify runs only on passthrough (hle.c:5578-5589) [read] | real, small | stayer_cast_end logs its own skip |
| G-1.5 | a look that asks for a room later loses the stayer's answer | script 103 resets its state before fn66; R4 passes fn66 over [read] | real | M13 (after R1); until then always printed |
| G-1.6 | the mover waits in black for the stayer, then the stayer is cut | G2's 900 cap, then "under it" (24921-24928) [read] | real for player 2's lend-route things and scenes | C2 (R4d widened); until then logged loudly and named in the user's check 2 |
| G-1.7 | the stayer's busy sets send 102 to the mover | split_send keeps 102 from every node of g_split.room that is not kind 19/K7 while split_on holds, the mover included (24961-24974) [read] | refuted while split_on holds | B2 keeps split_on on under the stayer's bits; S1/S6 check the mover's motion |
| G-1.9 | a charging summon keeps the old freeze; a pressed one is lost | G3 (24801-24806) [read] | real | a pressed summon is kept to the tail; G3 stays for a charging one (R3) |
| G-1.10 | player 1's Tome in the window is not the whole Tome | the copy's close writes back the magick record, equips, a script and a cast [read there] | maybe, and not new to the window | S8 does a Mix; Enchant for player 1 is R8 |
| G-1.12 | player 2's zone look under a door that turns zones off | the scan asks the zones word (0x80134A80) [read there] | maybe | zones on for their scan in the window (§2.3); R5 |
| G-1.13 | room_turn comes back from a stale copy | g_coop_p1_free saved only at busy 0 (20301-20304); fresh from free (20362-20363) [read] | real | after a B5 pause, from the game's copy |
| G-1.14 | player 2's deferred Tome item | not read where Use takes the item | maybe | R6; the count goes with the kept script |
| G-2 | the runs do not exercise the goal | the gaps listed (Tome casts, words in the window, player 1's pickup, 12-frame exit, player 2's walk-in door, apart with player 1 staying, a use, the ladder) | real | runs S1 (words in window), S3/S8 (casts), S5c, S6w, S6b, S11b, S15-S19 |
| new | coop_their_close_up with room_ok | player 2's lend-route words with no hold set no bit, so G2 would not wait and player 1's arrival would cut them (R4d not built) [inferred from §4.2's reads] | correction to the first draft | room_ok false for player 2 until C2 |
| new | bits up at the ticket | split_on is off under word0's 8/16/32 today [read 24614, 11343-11348] | addition | word0's bits go to g_stayer.bits when only the stayer has a thing up [inferred] |
| new | L1 at an exit_p2 ticket | that ticket is written with player 1 current (11362-11366) [read] | addition | L1 and stayer_cast_end cover it by their tests |

## 14. Facts read for this design

| Fact | Where | Mark |
|---|---|---|
| ticket writer order: record +0 (0x801557BC), r13-23392, 0x80117260(0x10000,1) at 0x801557E0, busy 4, 0x800443E4 at 0x801557F4, 0x801C5118(0) at 0x801557FC | disasm 0x80155770-0x80155800 | [read] |
| the notify and split_ticket_open run at the writer's entry, on the intercept's passthrough | hle.c:11359-11389, 5578-5589 | [read] |
| guest calls return to the sentinel 0x0BADF010 | main.c:52, 80 | [read] |
| coop_room_change_on / _other | hle.c:11343-11351 | [read] |
| coop_use_start swaps no input copy; assist_at; busy_before | hle.c:18182-18232 | [read] |
| continue_coop_use: busy_new by coop_busy | hle.c:18274-18277 | [read] |
| the world frame's order: 0x80020B08, 0x801E3CB0, the tick | 0x800083E4-0x800083EC | [read] |
| the world frame's prompt clear asks 0x8011727C(-1) at 0x800084DC | 0x800084D8-0x800084EC | [read] |
| player 2's B writes the shown record and calls 0x80006EBC | hle.c:16912-16917 | [read] |
| 0x80006EBC's B path asks no walking hold (r4 == 3 only) | 0x80006EE8-0x80006F08 | [read] |
| 0x80199EA0 holds the three walking channels and busy 2 | 0x80199EA0-0x80199F34 | [read] |
| fn139 sets r13-25696, the zones word and r13-24048 bit 4 | 0x80042B7C-0x80042B90 | [read] |
| a spell step calls 0x80199EA0(0) | 0x801C3754-0x801C376C | [read] |
| quick spell: class mask 0x1FF0; 0x820 summon | 0x801C43D8, 0x801C4438 | [read] |
| coop_quick_cast and its Tome ask | hle.c:12475-12540 | [read] |
| coop_quick_cast_ask | hle.c:19774-19790 | [read] |
| coop_hold_open / shut | hle.c:19796-19820 | [read] |
| coop_player1_turn: own copy, focus cleared, coop_script_hold_go | hle.c:19823-19909 | [read] |
| coop_p1_close_up's room bit and its writes | hle.c:19301-19334 | [read] |
| coop_script_hold_go: refusal under busy 4, the let-go | hle.c:19339-19364 | [read] |
| coop_p2_holds' room bit; coop_player_held | hle.c:19661-19688 | [read] |
| coop_busy_ask's order | hle.c:19717-19740 | [read] |
| coop_flags_ask; coop_prompt_choose and its look branch | hle.c:19628-19640; 19922-19960 | [read] |
| coop_scan_player1's refusal and own-look branch | hle.c:20165-20258 | [read] |
| hle_coop_controller: p1_free save, p1_look, p2_hold, room_turn, passthrough | hle.c:20267-20380 | [read] |
| player 2's look and B off under room_other; kept B; quick cast wait | hle.c:20998-21090 | [read] |
| coop_way_out_p1_up / p2_up | hle.c:16686-16694 | [read] |
| go_to_room order: R4, follow, waits | hle.c:16760-16798 | [read] |
| coop_prompt_look: zones word, their press | hle.c:16800-16960 | [read] |
| kept presses: keep, drop, player 1's keep needs !shown_b | hle.c:19268-19296, 27288-27370 | [read] |
| coop_menu_input: closeup; Start refusals; the path without the copy; player 1's copy route | hle.c:27573-27579, 27424, 27647, 27725-27742, 27800-27809 | [read] |
| the copy is a memcpy of cpu->ram; its player 1 views; its after-frame runs 0x8013E518 | hle.c:14977, 15060-15071 | [read] |
| tome_copy_p1_left waits on busy | hle.c:27873-27882 | [read] |
| split_counting, split_on, K7, the allow list, G3 | hle.c:24603-24742 | [read] |
| split_ticket_open, word0, G1, split_requests, G2 | hle.c:24778-24950 | [read] |
| split_send keeps 102 from every node of the room under split_on | hle.c:24957-24974 | [read] |
| stay_waits_clear / stay_busy_clear; the intercept on 0x801556CC | hle.c:25563-25612, 29450-29455 | [read] |
| split_state_load | hle.c:25007-25011 | [read] |
| script 103's state reset before fn66; script 132's hold through fn66 | `ed script 103 --level 1`, `ed script 132 --level 4` | [read] |
| fn66 reads its arguments only with 6 or 7 | `ed fn 0x801666A4`, transitions.md | [read there] |

## 15. Read R5, settled (2026-11-13, before the build)

`ed uses fn139` (134 scripts) crossed with `ed uses fn66` (200): five meet, L04/138 and 1599 in levels 00, 03, 04, 05 [read].
- L04/138 (a scene: fn139(1), fn22(1,1) ... fn22(0,1), fn139(0)) lets go of fn139 at its line 40, before its fn66 at line 53.
- 1599 is the whole game's chapter-end and Tome-return script: every fn66 in it follows play_cinematic (ed55) or sits
  under fn22, and its fn139(1) at line 135 is let go at line 203 before the fn66 at 205. No window opens under a
  cutscene (split_ticket_open), so none of these reaches the stayer's presses.

So no door holds fn139's bit 4 across its fn66: P1 masks 0x10000 only, as §2.2 says, and §2.3's zones-on needs no mover
record. fn22 (418 scripts, 34 with fn66) is a cutscene, and no window opens under one.
