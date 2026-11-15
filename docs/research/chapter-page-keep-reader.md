CHAPTER PAGE KEEP: WHY THE HEADER CHECK FAILS, AND THE SMALLEST FIX (nothing edited, built or run)

(1) What 0x8003F354 loads, and why the check fails only in play and film mode

- The keep 0x800B7208(4) goes to case 4, 0x800B7100, then 0x800ADCC0(0x80307184). That writes:
  - a 64-byte header: 0x80307170..78, the room 0x80117C64, the player's +152, 0x801DD65C;
  - then 0x80008778 at +64;
  - then 0x800B8018, the level's placed things [read].
- 0x800B8018 calls 0x8003F354(level = 0x80307178 & 0xFFFF, name = r13-29736, dst = *(r13-21988)) [read 0x800B8080-0x800B809C].
  - r13-29736 is the string "NPCS" (0x8061DFF8) [read dol].
  - r13-21988 is a 64KB scratch buffer (256x256), allocated once at boot (0x801E06D4, from 0x80021B58). In the hang it is 0x810C7080 [read].
  - r13-23488 is the size: 44 + 275*188 = 0xCA20, set by 0x8014EAC4(275) [read].
- So the keep re-reads the file NPCS<chapter>.NPC (the level's placed things), for chapter 0 [read the hang state].
- How 0x8003F354 reads it [read]:
  1. It zeroes dst (r13-23484 = dst).
  2. Fast path: if 0x801326B8() (r13-28976 and r13-28972 both != -1) and 0x8003F4E4() (r13-30472) == level, then 0x80157868 copies from ARAM 0xFA63D0. That is an ARQ ARAM-to-MRAM copy that waits for completion.
  3. Otherwise it reads "NPCS%d.NPC" from the disc through 0x80020BE4.
  4. Then 0x801DDF54 checks word 0 == 0x2C at 0x801DDFE8. A failure goes to the assert 0x8014EB18.
- Who keeps that ARAM cache [read]:
  - The level loader 0x80040750 runs on the loader thread, asked by 0x8003FC78. It fills ARAM:
    - 0x80157794 puts a file at 0xEA5E40, size r13-24332 = 0x14F580;
    - 0x801326E0(model, part) puts the body file at 0xEA5E40 and sets r13-28976 = part, r13-28972 = model;
    - NPCS<level>.NPC goes to 0xFA63D0 and /ai/ed_chrs.dat to 0xFB63D0;
    - last, r13-30472 = level (0x80040E7C).
  - The Tome's frame shares that ARAM:
    - When the body is cached, the Tome reads its pack from the disc into MRAM r13-24300.
    - At 0x80111664-0x8011172C it then writes 0x14F580 bytes of that pack, MRAM to ARAM, at 0xEA5E40. That runs to 0xFF53C0, over the body, NPCS and ed_chrs caches.
    - Then 0x801327A4 sets r13-28976 and r13-28972 to -1.
  - Mode 5 also clears them: 0x80022154 and 0x800221D0 call 0x801327A4 and 0x8003F4EC(-1).
- So in Tome mode (after that write) and in mode 5, the fast path is off, the file comes from the disc, and the check passes [read].
- The hang state, saves/reports/hang-20261115-112424.state [read]:
  - r13-28976=14, r13-28972=0, r13-30472=0, chapter 0: the fast path was taken.
  - ARAM 0xEA5E40 = 00000004 000006B5..., a pack header (the Tome's pack, given the 0x14F580 write above [inferred]).
  - ARAM 0xFA63D0 = 41A44183... (picture data), and dst holds the same bytes.
  - In the normal saves tome-ch5 and rooms-ch1-183, ARAM 0xFA63D0 = 0000002C... and 0xEA5E40 holds the body (D06537E9...).
- Cause [inferred, strong]: the copy Tome swaps only MEM1 (tome_copy_swap changes cpu->ram). ARAM is the host's one copy.
  - The copy's Tome wrote its pack over the real ARAM caches.
  - Its 0x801327A4 cleared the words only in the copy RAM, which is thrown away.
  - The real game still says "cached", so its fast path copies picture data.
  - Play mode and film mode fail for this same reason. The keep 0x80307184 in real RAM is all zero [read state].
- The 112957 state file is missing, so the play-mode hang is judged the same cause [inferred].

(2) The smallest correct way to make player 2's page match player 1's

Root fix (option c, for every copy-Tome close, not just pages):
- In tome_copy_close (and in tome_copy_p1_close), carry r13-28976 and r13-28972 from the copy into real RAM, next to script_left. Or call 0x801327A4 in the real game when the copy's pair is -1.
- The words that describe ARAM must follow the ARAM, which is shared. hle.c has no handling of r13-28976 today [read grep].
- This also covers later real fast paths that read stale caches:
  - the NPCS read in the save, 0x800ADF48;
  - the body cache gate at a real Tome's open: with the words at -1 it takes ARAM 0xEA5E40, which then really holds the Tome pack [read 0x80110AC8-0x80110BFC].

Then, in tome_copy_left, for a film page (copy r13-24244 != 2):
1. Run state 0 only, as player 2 (room in, for the room 83 test). It sets r13-25172=4, r13-26324, r13-24244=6 and byte 0x8031649A.
2. Put the swaps back, so player 1 is the game's player again.
3. Call 0x80018808(r13-24244), as 0x80110DE8 does.
4. Then call 0x800B7208(4), as the Tome does at 0x80111780-0x80111784, right after its 0x80110DA8. That is the same order as the real close [read].
   - The case-4 test of r13-24244 == 3 (0x800B711C, 0x800B715C) is not met in the real close either: ed9 has already written 6.
   - Writing the keep before mode 6's frame clears r13-25172 (0x800B72F0), so the film's 0x80050D4C call does nothing [read].
5. Let the film end run state 1 (ed87) natively.

On the other options:
- (a) Run 0x801327A4 in the real game before the keep. That is option (c) done only for pages; (c) is the general form.
- Do not let the film's frame write the keep: by then mode 6 has replaced the room and actors, so 0x800ADCC0 would record the film's room and player [inferred].
- Do not run the Tome's 0x80110DA8 room give-back. The real room was never taken from the real game [inferred].
- (b) Rejected:
  - The copy never runs its own close ("it runs no way out", hle.c:16371-16373 [read]), so the copy never writes 0x80307184.
  - If it did, the keep would hold player 2's room, type and position (0x801F62B0 is the current player in the copy). ed89 would then bring the hero back where player 2 stood.
  - Player 1 is the game's hero, and player 2 is remade beside them, so the keep should be written in real memory as player 1's page writes it [inferred].

Check run: ED_ARAM_LOG=1 (hle.c:982) prints every ARAM transfer. Expect a to-aram line, aram=0x00EA5E40, len=0x14F580, inside player 2's copy turns.

Files:
- E:\Eternal Darkness Recomp Fork\runtime\host\hle.c (tome_copy_swap 16216, tome_copy_close 16330, aram_log 982)
- E:\Eternal Darkness Recomp Fork\saves\reports\hang-20261115-112424.state
- E:\Eternal Darkness Recomp Fork\saves\tome-ch5.state
- C:\Users\stevi\AppData\Local\Temp\claude\6553937076d20482\52421c26-4209-49d0-ab20-99a22010a9f2\scratchpad\st.py (reads the cache words and ARAM out of a state)

### REFUTE
CHECK OF THE READER'S CLAIMS (nothing edited, built or run)

(1) Loader, buffer, fast path
- 0x800B8018 calls 0x8003F354(level = *(0x80307178) & 0xFFFF, r13-29736, *(r13-21988)) [read 0x800B8080-0x800B809C]. In the hang state r13-29736 is "NPCS", dst 0x810C7080, size r13-23488 = 0xCA20 [read state]. [confirmed]
- 0x8003F354 zeroes dst (0x80003100). If 0x801326B8() says yes (r13-28976 != -1 and r13-28972 != -1) and r13-30472 == level, it calls 0x80157868(ARAM 0xFA63D0, dst, size). 0x80157868 is ARQPostRequest type 1 (ARAM to MRAM) and then a wait. Otherwise it reads the file from disc (0x800F213C name, 0x80020BE4). Then 0x801DDF54 checks word0 == 0x2C at 0x801DDFE8; a failure goes to 0x8014EB18 [read]. [confirmed]
- The loader sets r13-30472 at 0x80040E7C [read]. [confirmed] I did not check the ARAM placement of NPCS and ed_chrs by the loader. [unverified, but it agrees with the clean saves]
- The Tome writes its pack to ARAM: ARQPostRequest(type 0, MRAM r13-24300, ARAM 0xEA5E40, length r13-24332 = 0x14F580) at 0x801116F8, then 0x801327A4 (both words set to -1) at 0x8011172C [read]. 0xEA5E40 + 0x14F580 = 0xFF53C0, which covers 0xFA63D0 and 0xFB63D0. [confirmed]
  - Correction: this is not in the close. It sits on the page-frame path. 0x80111298 branches to the close (0x8011173C) only when r13-24244 != 3. With 3 it runs on through 0x80111484 to 0x80111664. That code is gated by (r13-24324 != 0 or cached) and r13-24276 and r13-24284, so it runs once, as soon as the pack read is done [read].
  - So the copy, which runs only page frames, does run it. The copy's "no way out" (hle.c:16664-16667) does not prevent the write. The reader's cause still holds.
- Tome open (0x80110AC8): if cached, it reads the pack from disc; if not, it copies ARAM 0xEA5E40 into MRAM r13-24300 (0x80110BE0-0x80110BFC) [read]. [confirmed]
- Mode 5 clears both: 0x80022154/0x80022158 and 0x800221D0/0x800221D4 call 0x801327A4 and 0x8003F4EC(-1) [read]. [confirmed]
- ARAM is shared. hle_ARQPostRequest calls aram_dma_to_aram(cpu->ram, ...) into the one GXRuntime ARAM, and tome_copy_swap changes only cpu->ram [read hle.c:1038-1061, 16216-16224]. No hle.c line touches r13-28976 or r13-28972 [read grep]. [confirmed]
- Hang state 112424 [read with st.py]: words 14/0, r13-30472 = 0, level 0. ARAM 0xEA5E40 = 00000004 000006B5..., and ARAM 0xFA63D0 equals dst (41A44183...). r13-25172 = 4, r13-25176 = -1, r13-24244 = 6, modes r13-26240 and r13-26188 = 6, film byte 6, keep at 0x80307184 all zero. [confirmed]
- Keep path: 0x800B7208 runs only if (arg & r13-25172) is set and the countdown r13-25176-- drops to 0 or below. Case 4 goes 0x800B7260 -> 0x800B72D8 -> 0x800B7100 -> 0x800ADCC0(0x80307184), then r13-25172 = 0 at 0x800B72F4 [read]. Its r13-24244 == 3 tests are at 0x800B711C and 0x800B715C. [confirmed] The film frame calls 0x800B7208(127) at 0x80050D4C [read]. [confirmed]
- Real close order: 0x80111778 calls 0x80110DA8, then 0x8011177C calls 0x80025FF0 (the event-queue drain, which also runs every frame in play), then 0x80111780-0x80111784 call 0x800B7208(4) [read].
  - 0x80110DA8 waits on r13-24276, r13-24280, r13-24272 and r13-24284, then calls 0x80018808(r13-24244) at 0x80110DE8. 0x80018808 only stores r13-26188. Later it calls 0x80152298(r13-23276, ...) at 0x80110E88 [read]. [confirmed; the reader left out the drain]
- (b) is rejected because the copy never runs 0x8011173C [read hle.c:16664-16667]. [confirmed] Player 2's room and position would land in the keep. [inferred, agree]
- The 112957 state is missing; only the .txt exists. [confirmed]

FINAL CAUSE
Player 2's copy Tome opens with the copy's words 14/0, which mean "body cached", so it reads its pack from disc. On a page frame it writes 0x14F580 bytes of that pack into the shared ARAM at 0xEA5E40..0xFF53C0. That overwrites the body cache, the NPCS cache at 0xFA63D0 and the ed_chrs cache at 0xFB63D0. It sets the "cached" words to -1 only in the copy's RAM, which is then thrown away.

The real game still says the body is cached (r13-28976/-28972 = 14/0, r13-30472 = level). The keep's NPCS read therefore takes the ARAM fast path and copies Tome-pack picture data, and the 0x2C header check asserts. This happens in play mode and in film mode alike, because the mode does not matter, only the stale words. In solo, the real Tome clears the real words, and mode 5 clears them too, so both read from disc and pass. [read + state; that the copy did the write is inferred, strong]

RECOMMENDED FIX
A. In tome_copy_close and tome_copy_p1_close, while cpu->ram = g_tome_copy.copy, read r13-28976 and r13-28972. After switching back to real RAM, write both into the real game. The simplest form: if the copy's pair is -1, call 0x801327A4 in the real game. This puts the real game in the same state solo is in after any Tome close. Do it on every close, before tome_copy_left and before the stayer path keeps the script. It also stops later real fast paths reading the overwritten ARAM: the next room load's body gate, the save's NPCS read at 0x800ADF48, and a real Tome open [inferred].

B. In tome_copy_left, for the film page (real r13-24244 == 6 after state 0):
  1. Run state 0 only, as now, with player 2's swaps. It leaves r13-25172 = 4, r13-24244 = 6 and 0x8031649A.
  2. Put back the swaps (coop_state, coop_be_player2 false, room out) so player 1 is the game's player.
  3. Call 0x80018808(real r13-24244 = 6), as 0x80110DE8 does.
  4. Call 0x800B7208(4), as 0x80111784 does. It writes player 1's keep into 0x80307184 and reads NPCS from disc, because A cleared the words. It also clears r13-25172, so the film's 0x80050D4C call does nothing. Check that r13-25176 is 0 or below before the call; otherwise it only counts down [read 0x800B7228-0x800B7234].
  5. Let mode 6 play the film and its end run state 1 (ed87) natively. Do not run COOP_RUN_SCRIPT for state 1.

Do not call 0x80110DA8 or its 0x80152298 room give-back (the real room was never taken), and do not call the drain 0x80025FF0 from the host (it runs in the loop) [inferred]. (a) is A done for pages only. Prefer A.

Check run: ED_ARAM_LOG=1 (hle.c:982). Expect "[aram] ARQPostRequest to-aram ... aram=0x00EA5E40 ... len=0x14F580 lr=0x801116FC" during player 2's copy turns, and after the fix no to-mram read from 0x00FA63D0 at the page.

Files: E:\Eternal Darkness Recomp Fork\runtime\host\hle.c (tome_copy_swap 16216, tome_copy_p1_close 16291, tome_copy_close 16330, copy turn 16590-16704, tome_copy_left 31545, film branch 31651), E:\Eternal Darkness Recomp Fork\saves\reports\hang-20261115-112424.state