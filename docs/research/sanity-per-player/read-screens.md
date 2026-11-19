# Blue Screen (28) and Game Reset (26) for one player: the spec (read from the disassembly, the disc and hle.c 2026-11-18, nothing run)

For the two whole-game screens still to do the way TV Video (25) is done: the host does the mode's work, the mode is
never entered. Read with `plan.md` and `return-spec.md` beside it.

Tags: **READ** from `ed dis`, `generated/main.dol`, the disc (`tools/ed/disc.py`) or the named host file. **GUESS** not
verified. `r13 = 0x80625420`, `r2 = 0x80628460`. hle.c is being edited while this was read: functions are named, line
numbers are left out.

Corrections to plan.md:
- **UInsane*.cmp is not 1.39 MB.** `UInsane.cmp` is 504,973 bytes packed (`*SK_ASC*`) and unpacks to a 4-byte size word
  plus a 1,440,160-byte TPL (French the largest: 1,446,016). The 1.39 MB (1,388,320) is **`/UInsane.tpl`, an unpacked
  file that is also on the disc and that no code names** (the DOL's only names are the five `.cmp`, 0x802529F8..). READ
- **Pictures 2 and 6 are the same bytes in every language and in `/UInsane.tpl`** (md5 of the CMPR data, all five `.cmp`
  unpacked with the game's own unpacker, `tools/ed/ppcemu.py`). Pictures 0, 3, 4, 7, 8 differ. READ
- **Stream 7 is `/audio/music/poe.adp`**, the narration of the boot's quote screen: sound only, through the one music
  stream. Picture 2 is that quote screen ("Deep into that darkness peering..." Edgar Allan Poe). No logo, no movie. READ
- **Mode 10 reads HELD buttons**, not new presses (0x8013DC24(mask, 1, 0) reads the pad's word +0, the same word the
  B+X+Start reset test reads at 0x8013E868). READ
- A wide screen (r13-24660 == 1) makes the 2D space -80..720, where one 640-wide quad is 1.6 clip units: only the game's
  own wide picture (0x8019D4EC, -106..745) or a 4:3 run makes "one whole-screen quad" there. Two half-height quads are
  right in both. READ (`interp.cpp` `screenWide`: both spans >= 1.8)

## 1. The enter, 0x80112D98 (modes 7, 8, 10, 11, 12, 13)

| Step | Address | What | Tag |
|---|---|---|---|
| 1 | 0x80112DB4 | r31 = 0x801538A4() = [r13-23304], the room's data buffer | READ |
| 2 | 0x80112DBC | 0x80111078(), the Tome's enter | READ |
| 3 | 0x80112DC8, 0x80112DD0 | r13-24244 := 2 (the mode to go back to); r13-24316 := the buffer | READ |
| 4 | 0x80112DC0..0x80112E8C | by language r13-26284: 0 and 5 `UInsane.cmp` 0x802529F8, 1 `UInsaneF.cmp` 0x80252A14, 2 `UInsaneG.cmp` 0x80252A04, 3 `UInsaneS.cmp` 0x80252A24, 4 `UInsaneI.cmp` 0x80252A34, other: no load. Call 0x801562B8(name, buffer, 0x801538AC() = 0x2C8020) | READ |
| 5 | 0x80112E94 | 0x80156924(buffer): the TPL unpack (section 3) | READ |
| 6 | 0x80112EAC..0x80112EC0 | r13-24156 (byte) := 150 (written only, no reader found); r13-24154 := 7; state r13-24148 := 0; picture r13-24136 := 2; crossfade alpha r13-24144 := 255 (none); r13-24168 := 0 | READ |
| 7 | 0x80112EC4 | 0x80111B28(): r13-24272, -24276, -24280, -24284 := 1 | READ |
| 8 | 0x80112EC8..0x80112F08 | the music: 0x801A1028() (empties the stream queue); if 0x801A1898(1, 0) != 0 (a stream is playing: kept in keep slot 1 of 0x805DB1E4, id and place): r13-24168 := 1, 0x801A16A0(0, 0, 16) (ramp to silence), 0x801A172C() (stop), 0x801A16A0(100, 100, 1) | READ |

## 2. The frames

Both frames begin with 0x80020B08, 0x801E3CB0, 0x801E3D00([r13-21804]), 0x80020B54 and end with 0x8014FA84(0),
0x8015012C, 0x80020B88, 0x801DF914(1), 0x8013D778, 0x8019DC50, 0x8014ED04, 0x801E4610(0) and the three 0x802035D4 /
0x80203508 / 0x80203644 calls on 0x805FF650: the mode's own begin, end and present. The host needs none of them. READ

The timer is r13-22216: 0x801DC9F8 adds 1 a frame, 0x801DC9F0(n) sets it. READ

### Mode 10, 0x80113590: Blue Screen (28)

| When | Address | What | Tag |
|---|---|---|---|
| every frame | 0x801135C0..0x801135D0 | 0x801E1164() (the mode's render reset); 0x801E1B74([r13-24316]) binds UInsane; 0x8019CFD4(5): the 2D setup | READ |
| every frame | 0x801135D4..0x80113600 | the picture: wide (r13-24660 == 1) 0x8019D4EC(6, 255, 0), else 0x8019D470(6, 255, 0). Picture 6, alpha 255. **No text, no rectangle** | READ |
| every frame | 0x80113608 | 0x801A0F30(1): r13-22920 := 0 | READ |
| tick == 1, or "pressed" r13-24164 == 1 | 0x80113624..0x80113664 | if the countdown r13-29028 == 10 (its start value, DOL): handle r13-29032 := 0x801A4884(583, 100, 6, 0, 0, 5, 0, 1). Countdown - 1. Above 0: nothing more this frame | READ |
| the countdown reaches 0 | 0x80113668..0x801136A0 | count r13-24160 + 1; if it is 6: count := 0 and **the end, this frame** (r30 := 300). Then 0x801A4F40(-1, 583) (stop sound 583), countdown := 10, pressed := 0 | READ |
| otherwise | 0x801136A8..0x8011375C | 0x8013DC24(mask, 1, 0) for 1024 (X), 2048 (Y), 256 (A), 512 (B), 64 (L), 32 (R), 4096 (Start), pad 0, **held**: any one sets pressed := 1, countdown := 10. Not Z, not the D-pad, not the sticks | READ |
| tick >= 300, or the 6th | 0x80113768..0x8011383C | the end (below), with 0x801A4B58(-1, -1, 0) first: every stream-backed sound stopped | READ |

- The tick-1 start plays the sound but counts nothing: its countdown stops at 9 and the first press sets it to 10 again.
  So it takes **6 presses**, each at least 11 ticks (1 to be seen, 10 of sound), or tick 300. A button held all the way
  ends it at about tick 68. READ
- 0x801A4884(id, volume, kind, r6, position or 0, r8, r9, now): one of the 4 stream-backed sound records at 0x805DB5C0
  (388 bytes each: +348 id, +328 serial, +375 volume, +378 state). No position: centred. Returns the serial
  (r13-22884). READ. Which file 583 is: not read.
- 0x801A4F40(-1, id): the record of that id asked to stop (+381 := 1). 0x801A4B58(serial or -1, id, x): stops the records
  whose serial is r3, or whose id is r4, or all when r4 is -1. READ

### Mode 7, 0x80112F24: Game Reset (26)

| When | Address | What | Tag |
|---|---|---|---|
| tick == 1 | 0x80112F54..0x80112F88 | 0x801A1028(); if 0x801A1898(1, 0) != 0: 0x801A16A0(0, 0, 32), 0x801A172C() (the music, once more) | READ |
| state 0, tick == 3 | 0x80113010..0x8011303C | 0x801A172C(); 0x801A16A0(100, 100, 1); **0x801A1750(7, 0, 0, 0)**: stream 7, `/audio/music/poe.adp` (table 0x8024C8E0, 32 bytes a name; 688,128 bytes; the boot plays the same at 0x80019990) | READ |
| every frame | 0x801130D0..0x80113118 | 0x801E1164(); bind; 0x8019CFD4(5); picture [r13-24136] = 2, alpha 255 (0x8019D470, or 0x8019D4EC wide). The crossfade to r13-24152 runs only while r13-24144 < 255: never here | READ |
| every frame | 0x80113190..0x801131A0 | 0x8019D358(0x80254D10, 0x8023C780): sets two vertex arrays, draws nothing | READ |
| state 0, tick >= 150 | 0x80112FC0..0x80113008 | state := 4; timer := 2; 0x801A16A0(0, 0, 10); 0x801A172C(); 0x801A16A0(100, 100, 1) | READ |
| state 4, timer >= 1 (the same frame) | 0x801131B4..0x80113268 | the end (below) | READ |

States 1, 2, 3 (pictures 1 and 10, a crossfade) are in the code but nothing sets state 1 (every writer of r13-24148
read). READ. So: the quote picture for 150 ticks, the narration from tick 3, cut at 150.

### The common end

| Step | What | Tag |
|---|---|---|
| 1 | node = 0x801F8FD8(0x801F6310(), 32), the tracker; holder = 0x8006A4A8(node) | READ |
| 2 | mode 10 only: 0x801A4B58(-1, -1, 0) | READ |
| 3 | 0x801E4610(1) (VISetBlack), 0x80007368(2), 0x801DF914(1), 0x801E3CB0, 0x801E3D00 | READ |
| 4 | 0x80110DA8: the Tome's exit (mode [r13-24244] = 2 asked by 0x80018808, the timer back from r13-24240, the room read again by 0x80152298([r13-23276])) | READ |
| 5 | if holder: 0x801F59B0(81, 0, 0x801F62C8(node), 0); **0x8006B270(holder)**: the flash, sound 536, the character's line | READ |
| 6 | if r13-24168 (music was kept): 0x801A16A0(0, 0, 16); 0x801A172C(); 0x801A17C4(1, 32) (keep slot 1 played again from its place, ramped up over 32); r13-24168 := 0 | READ |
| 7 | mode 10 only: handle := -1, pressed := 0, countdown := 10, count := 0 | READ |

After Game Reset the game is simply back in play with the flash and the line: no room change, no second screen. READ

## 3. The pictures in memory

| Address | What | Arguments, needs | Tag |
|---|---|---|---|
| 0x801562B8 | loads a packed file through the loader thread | (name, destination, r5 unused). Opens by 0x80156150, reads 4 bytes (the size), then that many bytes into the destination, each read a message to the loader thread and a blocking wait (0x8020190C send, 0x802019D4 receive). Returns the size | READ |
| 0x80156924 | TPL unpack, in place | (bank). Adds the bank's address to +8 and to every descriptor's pointers, always; the data pointers once (flag header+35). **A second run corrupts the bank** (`06-texture-bank.md` 2.2) | READ |
| 0x801E1B74 | bind | (bank) -> **the bank bound before**. The word is r13-22132; a change calls 0x801E1898, which only forgets the material code's "last image" notes. Nothing of the room is lost by binding: put the old bank back with the same call | READ |
| 0x8019C7C8 | material with image n | (&rgba, 0, n, 0x80000000): template 0x8023B060, then 0x801E21FC -> 0x801E2700, which finds n with 0x801569E8([r13-22132], n): **a plain index into the bound bank's descriptor array**, no table besides, no bounds check | READ |
| 0x8019D470 | the picture | (n, alpha, variant): colour [r2-20564] = FFFFFF00 with the alpha byte put in; 0x8019C7C8; then 0x8019D284(texcoords 0x80254D10, positions 0x80254D20 + 24 x variant, 0, 0, 5): one quad of 4 indexed vertices | READ |
| 0x8019D4EC | the wide picture | the same with positions 0x80254D20 + 24 x (variant + 2) | READ |

`/UInsane.tpl` and every unpacked `.cmp`: 11 pictures. 0, 1, 2, 6, 7, 8, 9, 10 are 640x480 CMPR (153,600 bytes each);
3 is 640x480 (850x480 in the `.cmp`), 4 a small label, 5 is 2x2. In `/UInsane.tpl` picture 2's header is at +172, data
+307,712; picture 6's header +316, data +620,320. Clamp, linear, no mipmaps. Stored bottom row first. READ (disc)

Positions (s16 x, y, z; 640x480 units): variant 0 (0,0) (639,0) (639,479) (0,479) z -30360 (the back); variant 1 the
same at z -1; variants 2 and 3 from -106 to 745. Texcoords 0x80254D10: (0,512) (512,512) (512,0) (0,0). READ (DOL)

**Memory for it.** The host has no guest allocator. It has fixed scratch past the game's 24 MB (48 MB of guest RAM
always: `cpu.c`), named by `#define` in hle.c: `SCRATCH_RAM` 0x81800000 .. `RETARGET_KEYS` 0x82F00000. The Tome copy is
not in guest memory at all: it is a whole second RAM (`malloc(cpu->ram_size)`, `tome_copy_swap` points `cpu->ram` at it).
The plaques already do what is wanted here: a host-made bank in scratch (`PLAQUE_BANK`, `tome_plaques_bank`), bound
with 0x801E1B74 and drawn with the game's quads (`tome_plaques_draw`). READ.
0x82310000..0x823FFFFF has no user by grep of `runtime/host` (a comment names a `SPELL_COLOUR` at 0x82300000; no define
of it was found). GUESS that it is free.

**Cheapest reliable design: a two-picture bank of the host's own, read straight from `/UInsane.tpl`.** 307,456 bytes.
No loader thread, no unpacker, no language. The host reads disc files this way already (`dvd_path_to_entrynum`,
`dvd_entry_info`, `dvd_read_to_guest`: `p2_moves_load`). READ for the parts; the whole is not run.

**Alternatives.**
- The game's own load into scratch: 0x801562B8(name by language, scratch, 0) then 0x80156924 once. 1.45 MB, and the
  load blocks on the loader thread inside a `hle_guest_call` (not known to work there: GUESS it can hang). Only needed
  if a disc has no `/UInsane.tpl`.
- A host picture over one half, outside the game's frame: `frontend.cpp` has `window_half(half)` (the Tome skin uses
  it) and the renderer decodes CMPR. It needs a decoder call and a new layer, and sits over the game's fade and flash.
  More code than the bank; keep it as the fallback if the quads misbehave.

## 4. Drawing the picture in one half

- 0x8019D470 loads **no matrix and no projection**. Its quad is placed by whatever 0x8019CFD4(5) set last: the ortho
  (top 0, bottom 480, left 0 right 640, or -80 and 720 wide, near 1, far 30360) and an identity loaded into matrix 0 by
  0x8021FBAC at 0x8019D064, **lr 0x8019D068**. That is the load `notify_GXLoadPosMtxImm` already moves across while
  `g_sanity_overlay_out >= 0`. READ
- 0x8019D470 takes no rectangle. Its siblings do: **0x8019D084(texcoords, x0, y0, x1, y1, z, first, 5)** draws one quad
  at direct positions (x0,y0) (x1,y0) (x1,y1) (x0,y1) with texcoord indices first..first+3 from its own array (stride
  4); 0x8019D1A4 is the same without the array. READ
- The split's rule (`interp.cpp`): a flat draw whose first vertices span >= 1.8 clip units both ways is "whole screen",
  not HUD, and is never brought back from +4096. A half-height quad spans 1.0 down: it is HUD, lies wholly past the
  right edge once moved, and is brought back in player 2's view. One uniform block shared with a whole-screen draw
  stops being HUD for all its draws. READ
- The matrix mark (0xD2, 0xA0 + view, 0xE5) only leaves a draw out of a view. It cannot bring a whole-screen quad into
  player 2's half, and the comment at `notify_GXLoadPosMtxImm` says it lost player 2's overlay from both. Use two quads.
  READ (the comment), not run here.
- z: the game's -30360 is the far plane; the world is already drawn when `sanity_overlay` runs (0x80008534, after the
  words 0x801DA574, before the fade 0x801E7DA4 and the present). Use -1, as the TV Video rectangles. GUESS that the 2D
  material tests depth; -1 is right either way.
- Do not call 0x801E1164 (the mode's whole render reset) inside the play frame. GUESS on what it would undo; the
  plaques draw without it.

## 5. Start, 0x80006CDC (the Tome's opener; Start is `0x80006CDC(1, 0)`)

| Address | What | Tag |
|---|---|---|
| 0x80006CF4..0x80006D08 | leaves if a fade is on (0x801E7C98) or the Start lock r13-26232 != 0 | READ |
| 0x80006D0C | leaves if 0x8004303C() = r13-25696 != 0 (words up); returns to **0x80006D10** | READ |
| 0x80006D18..0x80006D2C | leaves unless a control channel is live (0x80199F50 or 0x80199F74) | READ |
| 0x80006D30..0x80006D54 | r30 = the player's id (r13-21496); node, holder; **r31 = 0x80068D18(holder)**, returning to **0x80006D54**: holder+4's record, if its +10 (u16) == 5 its +0, else -1 | READ |
| 0x80006D58..0x80006D78 | event 135 to the player; leaves unless answered 1 | READ |
| 0x80006D7C..0x80006D94 | leaves if 0x801C551C() = r13-22320 != 0, or (holder+196)+32 bit 0 set (0x8006DB04(1)) | READ |
| 0x80006D98 | r13-26256 != 0 (a script's "open the menu"): mode 3, no sanity test | READ |
| 0x80006DA8..0x80006E64 | **25 -> event 84, answered 1 -> mode 8; 26 -> 84 -> mode 7; 28 -> 84 -> mode 10**; 27 and all others mode 3 (the Tome) | READ |
| 0x80006E68..0x80006E9C | 0x801F5620(28, 0, 1, room, f1) (event 28 to the list); 0x80018808(mode); 0x80175508; 0x80007368(2); 0x801E4610(1); 0x8013E154 | READ |

Confirmed: all three start here, and nothing but event 84 is sent before the mode is asked. No sound, no flag of the
effect's. The rest (event 28, the black, the mode word) is the mode switch itself, which the host does not make. What
the host skips today and the game asks: the fade, words up, a live channel, event 135's answer, r13-22320, the
holder's flag, and that 84 was answered 1. `g_sanity_p2_takes_start` (set at the end of the TV block in
`sanity_p2_step`, read by `coop_p2_own_up`) keeps their Start from opening their Tome copy. READ

## What to build

1. **The bank** (new `sanity_screen_bank(cpu)`, modelled on `tome_plaques_bank`). `SANITY_BANK` = 0x82310000 (check it is
   free first). If [B] != 0x0020AF30 or the canary [B+0xF0] differs, write:
   - B+0 0x0020AF30, B+4 2, B+8 B+12; B+12 B+28, B+16 0; B+20 B+64, B+24 0.
   - Headers at B+28 (picture 2) and B+64 (picture 6): u16 480, u16 640, u32 14, u32 data, u32 0, u32 0, u32 1, u32 1,
     u32 0, bytes 0, 0, 0, **1** (unpacked). Never pass the bank to 0x80156924.
   - Data at B+0x100 and B+0x25900 (both 32-aligned), 153,600 bytes each, with `dvd_read_to_guest(cpu, data, start +
     offset, 153600)`: `start` from `dvd_entry_info(dvd_path_to_entrynum("/UInsane.tpl"), ...)` (path form: GUESS), the
     offset from the file's own header (read its first 512 bytes; descriptor n at 12 + 8n; header +8 the data offset).
     Refuse unless both headers say 640x480, format 14. On this disc: +307,712 and +620,320.
   - B+0x68 the colour word 0xFFFFFFFF. B+0x70 eight s16 pairs: top quad (0,512) (512,512) (512,256) (0,256), bottom
     quad (0,256) (512,256) (512,0) (0,0).
   - Fill it when 26 or 28 is picked (state 41), not at the press: a 307 KB read.
   - No bank (no file): answer 26 and 28 "not allowed" for a split player, as today.
2. **The press** (the TV block of `sanity_p2_step`, made one for 25, 26, 28): `waiting` in {25, 26, 28}, Start new:
   `g_sanity_p2_screen` on with the effect, tick 0, press state cleared; event 84 to the tracker
   (`COOP_SEND_EVENT`{84, 0, id, 0}); 0x80199EA0(0). `g_sanity_p2_takes_start` true for all three. Add 26 and 28 to
   `kSanityP2`. (The game goes on only when 84 is answered 1; the host can check the tracker's state left 41.)
3. **28's ticks** (t = the tick after `++tick`; state: countdown 10, pressed false, count 0):
   - t == 1 or pressed: if countdown == 10: 0x801A4884(583, 100, 6, 0, 0, 5, 0, 1). countdown--. If it is 0: count++;
     if count == 6: end this tick; 0x801A4F40(-1, 583); countdown = 10; pressed = false.
   - else if `g_coop_p2_buttons & 0x1F60` (held): pressed = true, countdown = 10. (The opening Start still held counts
     as a press, as in the game; waiting for a release first is a choice.)
   - t >= 300 or the 6th: **0x801A4B58(-1, 583, 0)** (the game's -1, -1 would stop the other player's room sounds
     too), then the end.
   - The music: the game silences it for the screen (the enter). TV Video today leaves it. Same choice here.
4. **26's ticks:**
   - at the press: 0x801A1028(); kept = 0x801A1898(1, 0); if kept: 0x801A16A0(0, 0, 16); 0x801A172C();
     0x801A16A0(100, 100, 1).
   - t == 3: 0x801A172C(); 0x801A16A0(100, 100, 1); 0x801A1750(7, 0, 0, 0). (`coop_music_act` passes it: it only acts
     on the scripts' call sites.)
   - t >= 150: 0x801A16A0(0, 0, 10); 0x801A172C(); 0x801A16A0(100, 100, 1); the end; then if kept: 0x801A16A0(0, 0,
     16); 0x801A172C(); 0x801A17C4(1, 32).
5. **The end** (as TV Video's): screen off; 0x80199EA0(1); event 81 to the tracker; 0x8006B270(holder). Not
   0x80110DA8, not the black, not the mode word.
6. **The draw** (`sanity_overlay`, player 2's part, beside the TV Video block, their set in, `g_sanity_overlay_out`
   0), for effect 26 (image 0) or 28 (image 1):
   - `sanity_screen_bank(cpu)`; 0x8019CFD4(5);
   - old = 0x801E1B74(SANITY_BANK);
   - 0x8019C7C8(B+0x68, 0, image, 0x80000000);
   - x0, x1 = -106, 745 when r13-24660 == 1, else 0, 639 (the game's own two tables);
   - 0x8019D084(B+0x70, x0, 0, x1, 240, -1, 0, 5); 0x8019D084(B+0x70, x0, 240, x1, 479, -1, 4, 5);
   - 0x801E1B74(old).
   Nothing else: neither screen has words or a rectangle.
7. **Player 1 in co-op** (to be decided): the smallest change is three parts.
   - An intercept on 0x8004303C picked by **lr 0x80006D10**: co-op, the split on, r13-26256 == 0, and player 1's set
     (the one in memory) has 25, 26 or 28 waiting (0x80068D18's own test): start the host screen for player 1 and
     return 1. The opener then leaves at 0x80006D14 with nothing sent and nothing asked. Otherwise pass. The gates
     after that point are then the host's to ask: a live channel (0x80199F50 or 0x80199F74), r13-22320 == 0,
     (holder+196)+32 bit 0 clear. (0x8004303C is two instructions; if it cannot be hooked, 0x801E7C98 at lr
     0x80006CF8 returning 1 leaves the same way. GUESS on which the dispatch takes.)
   - Answering "none" (r3 = -1) from 0x80068D18 at **lr 0x80006D54**, as plan.md has it, is not enough alone: the
     opener then asks mode 3 and their Tome opens on that press. It fits only as a guard while their screen is up.
   - The same ticks in a step of player 1's beside `sanity_p2_step`'s call (no set swap: theirs is in memory), and the
     same draw in `sanity_overlay`'s first part (`g_sanity_overlay_out` 1, after player 1's 0x800AC71C).
   - Not checked: 0x80199EA0(0) for player 1 puts its busy bit in the game's one word (player 2's is taken out again by
     `sanity_p2_step`); whether that holds player 2 too.

## What one player gets, honestly

- **Blue Screen:** the picture in their half, held, the beep at each press, 6 presses or 300 ticks, the flash and the
  line. The beep and the flash's sound come from the one set of speakers: both hear them.
- **Game Reset:** the quote screen in their half for 150 ticks. The narration is the music stream: **both players hear
  it and the room's music stops for those 2.5 seconds**, then comes back from where it was. The fallback is the picture
  with no narration and the music left alone (drop step 4's music calls).

## Not checked

- Nothing here was run. Free scratch at 0x82310000 is by grep only.
- `colours = EFFECT_MTX_SCRATCH + 64` in the TV Video draw is the same address as `FOG_TEXMTX_SCRATCH` (both
  `SCRATCH_HUD + 0xE80`). Each writes before it reads; a clash was not looked for. READ (the defines)
- Which file sound 583 is. What event 135 does to the player besides answering.
- Whether 0x801562B8 can run inside a guest call (only the alternative needs it).
- The texcoord fixed point (512 = 1.0 is from the whole-picture table; 256 as the half holds either way).
- An HD texture pack keyed by Dolphin's hash would also match the bank's copies (same bytes): expected, not checked.
