# The effects player 2 does not have yet, and player 1's whole-game screens in co-op (read 2026-11-18, nothing run)

Read with `plan.md`, `return-spec.md`, `read-screens.md` and `read-shared-room.md` beside it. Every claim is marked
**READ** (seen in `ed dis`, the DOL, the recompiled chunks' comments or the host's code) or **GUESS**. hle.c is cited by
function name. `r13 = 0x80625420`, `r2 = 0x80628460`.

What is already built and reused below (READ, hle.c): the set swap `sanity_set_io`; player 2's update `sanity_p2_step`
(their set, controller state `coop_state`, `coop_be_player2`, their room `coop_stay_room_in`, their link
`sanity_p2_link`, busy bits kept in `g_sanity_p2_busy` by comparing the word before and after); their door
`sanity_p2_door`; their return (`g_sanity_p2_return`, `sanity_p2_return_open`, closed in `sanity_p2_door(false)`); the
"is it on" answers `sanity_is_on` / `sanity_is_on_n` (from player 2's kept set when `r13-21492 == g_coop_p2`); their
fake death `sanity_fake_death` / `sanity_death_ends`; the pickup bracket `sanity_pickup`; the owner list `g_phantoms`
(`sanity_phantoms_note`, kinds 64..66 only) with `coop_found_object_draw` (hide) and `coop_interface_player` at lr
0x8003256C (quarry); the screen block and draw (`g_sanity_p2_screen`, `sanity_overlay`, `sanity_screen_bank`,
`sanity_black`).

## New facts first

- **The start table is indexed by the effect's number** (0x80243D8C[n], READ from the DOL): 9 0x8009C5D4, 13 0x8009B0BC,
  15 0x8009C98C, 16 0x8009D0A8, 18 0x8009AC40, 19 0x8009AE28, 23 0x8009BAB4, 29 0x8009C250; 20, 25-28, 34, 36, 38 share
  0x8006B010, whose switch (0x8024411C, index n-20) gives 20 0x8006B1C0, 34 0x8006B0D4, 36 0x8006B118.
- **The end switch** (0x80244294, index n+1, at 0x8006C8D8) and the returning effects' extras (0x80244238, index n-13):
  13 0x8006CC68 (0x801EF254(0,0), data+32 |= 0x80000); 15 0x8006CC2C (event 233 for later to the tracker); 16 and 18
  0x8006CC50 (0x8009ADD8, the input handlers back); 19 0x8006CC5C (0x8009AF94); 9, 23, 29 nothing extra. Then all of
  them: 0x80024384, event 229 to holder+56, 0x80199EA0(0), 0x80117260(256,1), the flash and the transition-17 return
  (`return-spec.md`). 34 ends at 0x8006C96C (flash 0x8006B270, 0x80199EA0(1), 0x8009EB34(0)), 36 at 0x8006C9CC
  (0x8009DF9C(0)), neither returns. READ.
- **Spawn flags in the record's +24** (0x8006C008, READ): 4 = spawn at the door, 0x10 = the room's own kind-1 nodes moved
  to room 10000 (for both players in a shared room), 0x2000 = fewer if the room has some, 0x4000 / 0x8000 kept in r26 /
  r19 for the spawn loop. 9 sets 0x14, 16 and 18 0x4014, 19 0x8014, 29 0x14, 13 0x4; 15 and 23 set no door bit (23
  clears 0x30100). Count: 16 for 9 (radius 500 in rooms 27, 69, 123, 245, 255), 5 for the rest (0x8006C228..0x8006C29C).
- **Types** (0x8006E6B8, READ): 9 the chapter character's ammo (types 110..121 / 169..185 by 0x80307170+8, a switch at
  0x802443E4); 19 by 0x800C8AF0 (a kind 7, 5 or 4 creature by its own table); 16, 18, 29 the chapter's creature from the
  alignment table at 0x80238FB0+124 (r13-26320). Every spawn but 9's gets info+140 -> +0 |= 0x1000, "the current
  player's quarry only" (`read-shared-room.md` A1): **so 16, 18, 19 and 29's creatures go for player 1, as the phantoms
  did** (GUESS in play, READ of the rule).
- **0x80199EA0(0) is the play hold: the control channels r13-23060 and -23052 off, AND busy bit 2 by 0x801556CC(2,1)**;
  0x80199EA0(1) the reverse (READ). **29 calls 0x801556CC(2, 0) right after it** (0x8009C3E0): the player's controls off
  with the world running, so its creatures can attack. This is the way to hold one player without stopping the other.
- **16, 18, 19 take the player's A and Z away**: 0x80199DE0/E10/E40/E70 write the four handler slots of the binding list
  at 0x805D7980 (32-byte entries, +8 the handler, 0x8013DC10) with the empty 0x80024AEC and keep the old at data+172..+184
  (19: slot 1 only). READ. That 0x805D7980's entries 0..3 are A pressed, A held, Z pressed, Z held as the template at
  0x8023AF40 (whose +8 words are 0x8019A134, A144, A190, A1A0, READ) is a GUESS. 0x805D7980 is inside `COOP_CONTROL`
  (0x805D7900..+0x300), swapped per player by `coop_state` (READ): already each player's own.
- **Where each effect is asked "is it on"** (masks at the 17 callers of 0x8006A3A8 and 7 of 0x8006A32C, READ): 16 at
  0x80061C74 (in the hit taker 0x80061B9C); 18 (with 11) at 0x800CBF48 (the threat look 0x800CBE7C); 19 at 0x8003BA58,
  0x8005E9B4, 0x800627CC, 0x80072594 (hit code) and 0x8004A8BC; 13 at 0x800436F4 and 0x8009B494; 9 at 0x800DA700 (with
  17) and 0x800C3774. Asked outside player 2's turn they read player 1's set (`sanity_ask_is_p2s` needs r13-21492 ==
  player 2).

## The effects, one table each

### 9 Ammo Pickup
| | |
|---|---|
| Start | 0x8009C5D4: runner 0x8006BBC4, step 0x8009E778 every 50 ticks, flags \|= 0x14. Door-started (4). READ |
| Door | 0x8006C008 spawns up to 16 ammo items of the chapter's character (object flag 0x4000, 0x80118BEC), moves the room's enemies to room 10000 (0x10). READ |
| Steps | 0x8009E778: count+1; at 10 (500 ticks) and 0x8011727C(2) == 0 (r13-24048 & 2): steps cleared, 0x801F6828(holder+56's link, 0), 0x80199EA0(0), **event 81 sent at once** (0x801F59B0 at 0x8009E83C) to holder+56's info+140 -> +68. READ. What bit 2 of r13-24048 is: GUESS, a window up |
| Pickup | 0x800C362C: thing flag 0x4000 and 0x8006A32C(holder, 9): words (0x801DB374) and the window 0x80024A14 (record 0x8030A704, the head's), then event 71 to the thing; without 9 on, event 71 alone (the item vanishes, no words). Offered only while 9 or 17 is on (0x800DA700, mask 0x20200). READ |
| Holds / screen / return | play hold at the end only (busy 2 then the return). No overlay. Returns by transition 17. READ |
| Player 1's alone in player 2's update | Nothing in the update. Outside it: the fake items are real room things both see; a press of player 1's on one is never OFFERED (0x800DA700 asks player 1's set: READ) but a pickup that reaches 0x800C362C sends 71 and the item is gone (READ); the room's enemies are moved away for both (0x10, READ) |
| Build | 9 in `kSanityP2`. List the items in `g_phantoms` (widen `sanity_phantoms_note`: every node of data+188 / id +252, not only kinds 64..66) and hide them from player 1's half (`coop_found_object_draw`, as the phantoms). `sanity_pickup`: refuse player 1 on any id in player 2's data+252 list while their 9 is on (generalise the head test). The window's words are already placed by `sanity_pickup` (any window, READ); `sanity_p2_step`'s per-update placement is for 17 only: widen to 9. The window's close 0x80024818 asks only 17 (0x80024884, READ): nothing to bracket for 9 (GUESS) |
| Sees | Triggering player: ammo about them in their half, the words in their corner, the flash and the return to their door. Other: nothing (items hidden), plays on; their room's enemies gone if they share the room (`[DECIDE]`, as 32/33/35 in `read-shared-room.md` step 15) |

### 13 Timeline Visitation (keep off)
| | |
|---|---|
| Start | 0x8009B0BC: steps 700 ticks in chapters 7 and 10, 300 in the others (0x8009B168); flags \|= 4, clears 0x100. READ |
| Door | only into 27, 69, 123, 245, 255 (0x800689D0(room,2)). After the load 0x800436F4 asks 13 and 0x8006C708 **loads another chapter's script and word pack for the whole game** (table 0x802441F4), saves two storm flags r13-24924, sets data 0x4000 (examine gate); 0x8006CDF8 loads the chapter's own back. READ (sanity.md, and the 0x8006A32C call at 0x800436F4) |
| End | 0x8009B2B8: flag 1123 (Alex) or 1124 (others) set (shared, once a game), 81 for later, hold; extra 0x801EF254(0,0) (camera) and data+32 \|= 0x80000. Returns. READ |
| Player 1's alone | The script pack: one for the game, so player 1's room would run with the other era's scripts and words. The flag. READ of the load, GUESS of what breaks |
| Build | Off. Only possible as "both players go": a follow-mode door of both, or `[DECIDE]` by the user |

### 15 To Be Continued
| | |
|---|---|
| Start | 0x8009C98C: one step 0x8009CA84 (0 ticks) plus three empty. No door bit: it starts itself when picked (GUESS: the base flags 0x80068684 were not read for it) |
| Step | 0x8009CA84: the current player (0x801F62B8), 0x801F6828(link, 0), 0x80199EA0(0), flags \|= 0x401, **0x8006BE04(current player, tracker)** (its own return place), flag 837 set, **0x80018808(11)** at lr 0x8009CB20: the whole game into mode 11. READ |
| Mode 11 | 0x801138D0: picture 7 alpha 255; at tick 200 the timer is set to 0 and picture 8 fades in over it; at tick 200 again the end: VISetBlack, 0x80110DA8, event 81, 0x8006B270, the music back. 400 ticks. READ. Pictures 7 and 8 differ by language (`read-screens.md`, READ) |
| Return | 15 returns (transition 17 to the place kept by its own step) and sends event 233 for later. READ |
| Player 1's alone | The mode (both players' screen); r13-26524 / the current player for the kept place (in player 2's update it is them: READ `coop_be_player2`) |
| Build | Intercept 0x80018808 at lr 0x8009CB20 in co-op with the split on: do not ask the mode; open a host screen 15 in the triggering player's `g_sanity_screen[who]` (below, Q4), 400 ticks, two pictures from the bank (add pictures 7 and 8 of the language's file: 4 x 153,600 bytes fits below SCRATCH_FINISH 0x82400000, GUESS that `/UInsane.tpl` is the English set). End as the game: 81 to the tracker then the return (player 2's is the built return bracket). 15 in `kSanityP2` |
| Sees | Their half: the fake ending, then the sequel picture, then the flash and the return to where they stood. Other: plays on. Flag 837 is the story's, so the effect comes once per game for both (READ: it is one flag) |

### 16 Enemy One Hit Kill
| | |
|---|---|
| Start | 0x8009D0A8: step A 0x8009AD68 (A and Z handlers off, kept at data+172..+184), three steps B 0x8009C160 of 100 ticks; flags \|= 0x4014. READ |
| Door | 5 of the chapter's creatures near holder+56, flag 0x1000, room's own enemies moved away (0x10). READ |
| Steps | 0x8009C160: count+1; when 0x80043740(r13-23276, 0) == 0 (no kind-1 node left in the room in play) or count 10: steps cleared, 81 for later, 0x801F6828, 0x80199EA0(0). READ |
| The kill | 0x80061B9C (a hit taken by node r27, victim id r25): at 0x80061C38 0x801F62B8 (current player, **lr 0x80061C3C**) == r25 and 0x8006A3A8(holder, 0x10000) at **lr 0x80061C78**: event 8 (death) to the victim, which the fake-death test then plays as "not real". READ |
| End | 0x8009ADD8 (handlers back), the return. READ |
| Player 1's alone | The kill test: "victim == the current player" and the set asked are player 1's unless the hit is taken inside player 2's turn (GUESS: creature hits are taken in the world's update; `r6_hit_room` changes only the room, READ). The creatures' quarry (0x1000). |
| Build | 16 in `kSanityP2`; `sanity_phantoms_note` widened (owner, hide, quarry: as 9); `coop_interface_player` at lr 0x80061C3C: r25 == player 2's id -> answer it; `sanity_ask_is_p2s` also true at lr 0x80061C78 with r27 == `g_coop_p2`. The death is then `sanity_fake_death`'s (built). Cross-owner hits (`read-shared-room.md` step 4) before a shared-room test |
| Sees | Their half: creatures only they see; one hit kills them, "not real", the return. Other: nothing, plays on |

### 18 Helpless Battle
| | |
|---|---|
| Start | 0x8009AC40: the same steps as 16 (0x8009AD68, 0x8009C160 x3), flags \|= 0x4014. READ |
| Difference from 16 | Not asked at 0x80061C74; asked (with 11) in the threat look 0x800CBE7C at 0x800CBF48: on, r29 := 1 (the look's answer forced). READ of the branch; what 1 means (afraid, no fight stance): GUESS |
| Player 1's alone | The creatures' quarry. The threat look is run per player (`COOP_THREAT_LOOK`, `g_coop_threat`, READ) with them the player: GUESS that the ask is then theirs |
| Build | As 16 without the kill hook. 18 in `kSanityP2` |
| Sees | As 16: without A and Z against creatures only they see, until the room is clear or 1000 ticks |

### 19 Illusions of Grandeur
| | |
|---|---|
| Start | 0x8009AE28: step A 0x8009AF4C (A held off, kept at data+172), three steps B 0x8009D4DC of 100 ticks; flags \|= 0x8014. READ |
| Door | 5 creatures by 0x800C8AF0's own table (kinds 7, 5, 4), flag 0x1000, room's enemies away. READ |
| Steps | 0x8009D4DC: for each spawn id (data+252+4i) the distance to holder+56; event 86 to it (lr 0x8009D640); then 0x8009C160 (the end as 16). Also room 77 and 0x801F6160(2120) (a tagged thing), 0x8006A240(37). READ of the calls, GUESS of what they look like |
| Hit code | 0x8003BA58, 0x8005E9B4, 0x800627CC, 0x80072594 ask mask 0x80000 and 0x800641E4(..) (GUESS: "a sanity spawn") before 0x8006255C / event 86. READ |
| End | 0x8009AF94 (slot 1 back), the return. READ |
| Player 1's alone | The hit code's ask (current player at the time of the hit: GUESS player 1); the quarry |
| Build | As 16; `sanity_ask_is_p2s` true at those four lrs (+4) when the hit's attacker or victim is player 2 (which register holds them at each: NOT read). 19 in `kSanityP2` |
| Sees | As 16, with the creatures' own illusion (size): GUESS |

### 20 Empty Inventory
| | |
|---|---|
| Start | 0x8006B1C0: **0x80335758 \|= 2** (COOP_SPELL_ITEM 0x80335748 +16, READ), the name. Started when picked; waits for the Tome. READ |
| Tome | the inventory page: r13-24532 counts; at 1 event 84 (0x80106F4C); at 300 (0x80106E84) the bit cleared, the page rebuilt (0x80103A90, 0x80102810), event 81 to the tracker, the flash **0x80145760(20,0,0)**, 0x800432E8(60, 0x10000, 100, 100). Ends by steps (0x8006CB1C), no room change. READ |
| Player 1's alone | The bit is in real memory: player 2's pick would empty PLAYER 1's Tome. Player 2's Tome is a copy (`tome_copy_step`) that zeroes 0x80335748..+24 at its opening (READ, hle.c beside `g_tome_copy_spell`), and the copy's tracker is player 1's set (the copy is made from real memory: READ `tome_copy_swap`) |
| Build | Keep the bit and r13-24532 in `SanityOwn` (out of real memory with their set out). At their copy's opening, where COOP_SPELL_ITEM is written, put `g_sanity_p2` into the copy (`sanity_set_io` after the swap) and set the bit when their 20 is picked or on; at `tome_copy_close` take their set out of the copy into `g_sanity_p2`; replay 0x80145760(20,0,0) in real memory as them (their half: the flash goes to the half of who set it off, sanity.md). 20 in `kSanityP2` |
| Sees | Their Tome's inventory empty for 300 of its frames, then full, the flash in their half. Other: plays on (their Tome copy already holds only them, READ `coop_player_held`) |

### 23 Zombie (keep off)
| | |
|---|---|
| Start | 0x8009BAB4: step 0x8009BC10 (once), step 0x8009BC84 every 10 ticks; flags &= ~0x30100. READ |
| Steps | 0x8009BC10: **0x800A2254(the chapter's character)**: spawns a zombie body (0x80040E94) and makes it the current player (0x801F626C at 0x800A2400, READ via presses-effects-readers.md). 0x8009BC84: counts to 900 (or two counts of data+32 bit 0x8000, or enemies in the room) while nothing holds (0x8007AA64, 0x80157364, 0x8004303C), then event 8 to the current player. Returns. READ |
| Player 1's alone | Who the player IS: `g_coop_p2`, `coop_be_player2`, the cameras, the controller all follow one node. A body switch of player 2 changes that identity mid-effect |
| Build | Off. It needs the host's player 2 to follow a new body and come back (the Trapper summon's problem, presses-effects-readers.md 3.5). Last, and `[DECIDE]` |

### 29 Controller Not Found
| | |
|---|---|
| Start | 0x8009C250: step 0x8009C398 (70 ticks, once), step 0x8009C42C every update, flags \|= 0x14. READ |
| Door | 5 creatures (0x1000), the room's enemies away. READ |
| Steps | 0x8009C398: 0x80199EA0(0) then **0x801556CC(2, 0)** (controls off, world on), overlay r13-25492 := 0x8009C534, rumble-off mask r13-23608 \|= 4 (0x8013D93C(4,1), shared, stops the motor). 0x8009C42C: at count 400: 81 for later, overlay 0, 0x8013D93C(4,0). READ |
| Overlay | 0x8009C534: 0x8019CFD4(5), a band 0x8019C8FC(0, 210, 640, 60), image 15 of the HUD bank r13-26520 at 22..618 x 215..265 (0x8019D1A4). 60 high: HUD to the split, so `sanity_overlay`'s move across already puts it in their half. READ of the sizes, GUESS of the result |
| Player 1's alone | The creatures' quarry; r13-23608 (one word, both pads). The busy bit is cleared by the effect itself |
| Build | 29 in `kSanityP2`; `sanity_phantoms_note` widened (as 9). Nothing else (overlay, hold and return are built). |
| Sees | Their half: "controller not found" band, held, creatures only they see attack; after 400 the return. Other: plays on |

### 34 Shoots Self
| | |
|---|---|
| Start | 0x8006B0D4: step 0x8009EAF0 (count only, 1 tick). Not door-started. READ |
| Arm | **0x8009E5F0**, called from the reload behaviour at 0x80079FB4 (lr 0x80079FB8; r27 there the reloading character, GUESS): if 34 runs, answer 1 (the self-shot); else with nothing running and flag 1126 clear: event 83 (data+56 = 34) and **event 84 at once** (0x8009E6F4) to the tracker in memory, flag 1126 set. READ |
| Shot / end | 0x8009E4F8..: 0x801240B8, 0x80199EA0(0), the end 0x8006C96C: flash, 0x8009EB34 (holder+56 put at data+116, angle +128, 0x801C5580). No return. READ |
| Player 1's alone | The tracker asked and sent to at the reload is whichever set is in memory: player 2's reload reads and arms PLAYER 1's set (READ: no asker in 0x8009E5F0) |
| Build | One intercept on 0x8009E5F0 at lr 0x80079FB8: when the reloader is player 2, the call with their set and link in — `sanity_pickup`'s body made a helper (`sanity_as_p2(cpu, fn)`) and used by both. 34 in `kSanityP2`. Max's chapter only (flag 1126), with a gun |
| Sees | Their character shoots themself on reloading, the flash in their half. Other: plays on |

### 36 Spell Goes Wrong
| | |
|---|---|
| Start | 0x8006B118: step 0x8009DE78 (0 ticks), runner 0x8006BBC4, holder+106 = 1, +107 = 3. READ |
| Arm | **0x8009DDC4** from the cast code (0x801CD064, 0x801CD2A0): 36 waiting and flag 938 (chapter 4 on): event 84; answer 1 when taken. 0x8009DD20 (from 0x801C33DC) sends 82 when 36 waits. 0x80015D44 (a quick cast) asks 0x80068D18 == 36. All read the set in memory. READ |
| Step / end | 0x8009DE78: holder+56's model parts (0x80151500, 0x80150F48 x3), 0x80124B94, flag 938. End 0x8009DF9C: 0x80199EA0(0), **0x80145760(0,0,0)** (ed85's flash), parts back (0x801258BC x4), the weapon back (0x80150D9C, 0x800463DC). No return. READ |
| Player 1's alone | The arm, as 34 |
| Build | The same helper on 0x8009DDC4 and 0x8009DD20 when the current player (r13-21492) is player 2 (their quick casts run in their turn: READ `COOP_QUICK_CAST`; a Tome cast's later frames: GUESS). 36 in `kSanityP2` |
| Sees | Their body bursts on casting, their half's flash. Other: plays on |

### Stay off
- 12 Edward's Ghost: the story's scene order (flag 959), a cinematic for both. READ (sanity.md)
- 30 Alex's phone call: the story's (script L00/1801, flags 116, 885). READ (sanity.md)
- 27 Save/Erase: off by the user's decision.

## Q4. Player 1's own whole-game screens and Walk on Ceiling's black, in their half

How player 2's are drawn today (READ, hle.c):
- `sanity_p2_step`'s screen block: Start new with 25, 26 or 28 waiting (state 41) -> event 84 to the tracker, 0x80199EA0(0),
  the ticks (28's beep and presses, 26's stream 7 and music, 25's sounds), at the end 0x80199EA0(1), event 81,
  0x8006B270(holder). `g_sanity_p2_takes_start` keeps their Start from their Tome (`coop_p2_own_up`).
- `sanity_overlay` (on 0x800AC71C): player 1's overlay first with `g_sanity_overlay_out = 1`, then player 2's set in and
  `g_sanity_overlay_out = 0`, so the identity 0x8019CFD4 loads (lr 0x8019D068) is moved across by
  `coop_subtitle_away(cpu, 2)` in `notify_GXLoadPosMtxImm`; the black as two half-height 0x8019C8FC rectangles from -106,
  853 wide; the pictures from `sanity_screen_bank` (SANITY_BANK 0x82320000) with 0x801E1B74 bound and put back,
  0x8019C7C8, two 0x8019D084 quads; 25's word with 0x801D9AC8.
- `sanity_black` (on 0x801E4610, VISetBlack): in player 2's update at lr 0x8009B814 (black) / 0x8009B6B0 (shown):
  `g_sanity_p2_black`, VISetBlack not called, its flag word r13-21880 answered.
- Player 1 today: their Start reaches the game's 0x80006CDC, which asks modes 8, 7, 10 (0x80006DA8..0x80006E64): the whole
  game for both. 15 asks mode 11 itself. 22's VISetBlack at lr 0x8009B814 blacks both halves. READ.

The smallest change (each part names what it extends):
1. **State per player:** `g_sanity_p2_screen` -> `g_sanity_screen[2]` (0 player 1), `g_sanity_p2_black` ->
   `g_sanity_black[2]`; `g_sanity_p1_takes_start` beside `g_sanity_p2_takes_start`.
2. **The ticks:** the screen block of `sanity_p2_step` made `sanity_screen_step(cpu, who, node, holder, id, start_new,
   buttons)`; called for player 1 where the `ED_SANITY_P1_TEST` block runs (player 1's set in memory, before player 2's
   goes in), with player 1's pad (`g_tome_copy_p1_pad`, their pad as last read, READ). For player 1, after 0x80199EA0(0)
   call **0x801556CC(2, 0)** as 29 does (READ 0x8009C3E0): their channels off, the game's busy word clear, so player 2
   plays on. (Player 2's own keep bit 2 in `g_sanity_p2_busy`, as now.)
3. **Their Start:** while player 1's set has 25, 26 or 28 waiting (holder+4's number, state 41) or their screen is up,
   take Start out of player 1's pad in `coop_pad_read` (the `start_eat` way of `stayer_p1_presses`, READ) and pass the
   new press to step 2. 0x80006CDC is then never run for it: no mode is asked. (`read-screens.md`'s other way, an
   intercept on 0x8004303C at lr 0x80006D10 answering 1, needs a new hook on a two-instruction function: GUESS that it
   can be hooked.) The game's own gates for that press (fade, words up, a live channel, r13-22320, holder+196+32 bit 0)
   are skipped for player 2 today; skip the same for player 1, or test them in the host (READ the list, read-screens 5).
4. **The draw:** the three blocks of `sanity_overlay` (black, pictures, 25's word) made `sanity_screen_draw(cpu, who)`,
   called right after player 1's own 0x800AC71C call (`g_sanity_overlay_out == 1`: drawn where the game draws, which the
   split keeps in player 1's half: READ in plan.md's lessons) and in player 2's part as now.
5. **22's black:** `sanity_black` also in co-op with the split on and `!g_sanity_p2_in` at the same two lrs ->
   `g_sanity_black[0]`, drawn by step 4.
6. **15:** the intercept on 0x80018808 at lr 0x8009CB20 (above) fills `g_sanity_screen[who]` with who by
   `g_sanity_p2_in`; its draw is step 4 with pictures 7, 8 and the crossfade.
7. Not covered: the sounds stay for both (26's narration is the music stream: the room's music stops for both, as for
   player 2's today); a screen of player 1's pauses its ticks while `sanity_p2_step` returns early (a door of player 2's,
   a room ticket): GUESS a second, harmless.

What each sees: the triggering player's half holds the screen (blue screen, quote, VIDEO, fake ending, the ceiling's
black), they are held by their own controls; the other player's half and play go on; the flash after in the triggering
half; neither is ever moved to the other (no room change in 25, 26, 28; 15's and 22's returns go to their own kept place).

## Build list (easiest and safest first; each one run in Max's chapter, start `max-249`, board rule 8)

Every run: `ED_RUN_AWAY=1`, `ED_SETTINGS=saves/test-max-249.settings.ini ED_ROOM_SLOTS=2 ED_COOP_STAY=1`,
`sh tools/ed/run.sh saves/test-max-249.state NAME`, `ED_SANITY_P2_TEST=FRAME,N` (or `ED_SANITY_P1_TEST`), player 2 to
the door 249 -> 252 at -321, -887 (board rule 8) unless said. Check rule 9 (back at their own door) for every return.

1. **Player 1's screens (Q4 steps 1 to 4) with 28.** `ED_SANITY_P1_TEST=1800,28`, player 1 presses Start. Pass: the blue
   screen in player 1's half alone, player 2 walking, 6 presses or 300 ticks, flash in player 1's half, no mode change in
   the log. Then 25 and 26 the same run (`;` list is player 2's only: one effect a run, or two runs).
2. **Player 1's ceiling black (Q4 step 5).** Player 1 through 249 -> 252 -> 256 -> 257 (read-ceiling.md's way). Pass:
   only player 1's half black for the change, player 2 walking.
3. **29 for player 2, with the widened owner list.** `sanity_phantoms_note` lists every node of data+188 (not only kinds
   64..66): owner, hidden from the other, quarry the owner. 29 in `kSanityP2`. Pass: the band in player 2's half, held,
   five creatures in their half going for them, none in player 1's half, the return to the door at -321, -887.
4. **9 for player 2.** 9 in `kSanityP2`, items listed and hidden, player 1 refused on them in `sanity_pickup`, the window
   placed each update. Pass: ammo in player 2's half, PICK UP and the words in their corner, the return after 500 ticks.
5. **18, then 16.** 18 in `kSanityP2` (one run: no A, creatures theirs, room clear or 1000 ticks, the return). Then 16
   with the two answers at lr 0x80061C3C / 0x80061C78 (one run: one hit kills them "not real", the return; player 1
   untouched). Before any shared-room run: `read-shared-room.md` step 4 (no hits across owners).
6. **19.** Read first which registers hold the attacker and victim at 0x8003BA58, 0x8005E9B4, 0x800627CC, 0x80072594
   (not read here). Then one run as 18.
7. **34, then 36.** `sanity_as_p2` helper from `sanity_pickup`'s body; intercepts on 0x8009E5F0 (lr 0x80079FB8),
   0x8009DDC4, 0x8009DD20. 34: player 2 with a gun reloads in room 249 (flag 1126 clear: a fresh Max chapter). 36 needs
   flag 938 and a spell of player 2's: check Max's chapter has both before the run (GUESS that it does).
8. **20.** The bit and r13-24532 in `SanityOwn`; their set into and out of their Tome copy. Pass: their inventory empty
   for 300 Tome frames then full, the flash in their half, player 1's Tome full throughout (open it after).
9. **15** for either player (Q4 step 6 and the bank's pictures 7, 8). Pass: the fake ending in one half, 400 ticks, the
   return to where they stood. Flag 837 is set once a game: one try per save.
10. **13 and 23 stay off** (the shared script pack; the body switch): `[DECIDE]` by the user before any reading further.

## Not checked

- Which buttons 0x805D7980's four slots are (GUESS: A pressed, A held, Z pressed, Z held).
- 15's base flags (no door bit set in its start: READ; that it is never door-started: GUESS).
- r13-24048 bit 2 (9's wait), r13-23608 bit 4 (29's rumble mask), 0x800641E4, 19's illusion itself.
- Whether 0x80018808 and 0x8004303C can be hooked as intercepts; whether `/UInsane.tpl`'s pictures 7, 8 are English.
- Whether a creature's hit on player 2 is ever taken in their turn (decides how much of 16 and 19 is already theirs).
