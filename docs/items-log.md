# Eternal Darkness: items seen

Items found while tracing, in any session. Only what was seen in a log or written in the code. A blank means not seen yet.

**The Item id column is a handle: it lasts one play, and the same number can be another item later.** The game's own table is `ed item`, by type id, always the same. Bronze Necklace is type 21, Tulwar 84, Chakrams 82, Talisman 150, Blue Urn 31.

| Item | Item id | Use script | Hands | Description (from the game) | Source |
|---|---|---|---|---|---|
| Bronze Necklace | 102 | 1510 (`0x5E6`), then `0x8C0` | | "A well-crafted Bronze Necklace, resplendent with glowing gems." | Runs on slot 1 and slot 10, 2026-11-02. A shared quest item. Refuses with "cannot use that now" outside its zone |
| Strange Necklace | | | | "A Strange Necklace, fashioned from a dark ancient stone and engraved with weird designs. It c..." (log cut off) | Slot 10, 2026-11-02 |
| Short Sword | | | | "A Short Sword with a wooden grip - light and durable. A small curve running from the handle m..." (log cut off) | Slot 10, 2026-11-02 |
| Tulwar | 9 | | 2, 1 | | Comment at `coop_rehold`, `runtime/host/hle.c` |
| Chakrams | 11 | | 2, 0 | | Comment at `coop_rehold`, `runtime/host/hle.c` |
| Talisman | 10 | 1874 | | Five uses in slot 3 for player 2. When used up: "The Magick in this item has been completely drained. It cannot be used anymore." Casts `0x40011041` twice | `logs/run-20261103-093522-hand9` |
| Circle of Power (3 Point) | | 2129 (the find, on the desk in save 2) | | Tome page: "Circles of Power describe the amount of power that a spell will have. The more points within that circle, the stronger the spell will be." | `logs/run-20261103-181505-both` |

| Strange Necklace (again) | | find from script 2240 (ch2 room 166, the bronze statue L02/2240, after Yes) | | | `logs/run-20261105-214619-jtg-p2door-f3-ch2-p1b` (player 1), `logs/run-20261105-222855-jtg-vp2door-ch2-p2statue` (player 2): carry count 2 to 3, then scene 2242 on MantorokCam |
| Historical Journal | | find from script 780 (ch10 room 237, Region01, ed221) | | | `logs/run-20261105-205257-jtg-np1-ch10-p1journal`: notice "Karim has found a Historical Journal" to player 2; player 2's in `logs/run-20261105-210124-jtg-np1-ch10-p2journal-b` |
| Soldier's Letter | | find from script 1817 (ch9 room 27, at 682,-1220, ed221) | | | `logs/run-20261105-215024-jtg-finds9-p1-f3b`: "Karim has found a Soldier's Letter!" |
| Torch | | close-up camera CamTorch01 (ch7 room 123) | | | `logs/run-20261105-222816-jtg-vp2cam-v3-ch7-r123`, picked up by player 2 |
| Strange Necklace (type) | | type id 24 (MANT_NECK, SPECIAL): USE script 2243, AFTER FIND script 2242 | | Last words of its find: "This Necklace can be USED..." | Round-3 p1scene agent, `logs/run-20261106-011325-jtg-p1scene-st1c`, `logs/run-20261106-015651-jtg-p1scene3-st1`: carry count 2 -> 3. Player 1 (Karim's model 9) got "Ellia has acquired a Strange Necklace!"; player 2's notice "Karim has found a Strange Necklace" |
| Granite Block | | pickup CHATT_BLOCK (ch1 room 342, ChatBlockCam01) | | | `logs/run-20261106-010646-jtg-rega-c1d-r2b` (player 1; notice "Karim has found a Granite Block" to player 2), `logs/run-20261106-011826-jtg-rega-c1f-r2` (player 2; "Ellia has found a Granite Block" to player 1) |
| Granite Block (room 183, player 2's own find) | 52 | BlockRegion01 (ch1 room 183, ed221 script 1181, MantBlockCam01), the block at 362,336,37 | | "A strange Granite Block rests in the middle of the dais." (DETAIL), "Ellia has picked up a Granite Block." (the find, in player 2's half), notice "Ellia has found a Granite Block." to player 1; the block floats spinning in front of their close-up camera, on the one carry list (3 -> 4) | `logs/run-20261110-235512-stepC-run2-p2find` (ED_COOP_OWN_FIND=1, 2026-11-10 night; player 1's the same by the game's own route in -235449-stepC-run1-p1find) |
| Historical Journal (type) | | item 160, tag 2056, FAMILY_JOURNAL_1, script 780, JournalCam (`ed examine`) | | | Round-3 overlap agent, `logs/run-20261106-033816-jtg-ov-ord10-f5`: "Edward has found a Historical Journal", notice "Ellia has found a Historical Journal" |
| Essence of Xel'lotath | | found with ch11's opening words (Michael, room 50, script 2319) | | | `logs/run-20261106-040357-jtg-vsf-c11-sf1`: "Karim has acquired the Essence of Xel'lotath", notice "Karim has found the Essence of Xel'lotath" to player 2; thing object 0x81242100, no id logged |
| Pius's sword (ch1) | 9 | | | | Jump to Game ch1: player 1 carries only handle 9, held. Player 2's copy is handle 60 (node 0x81116F50, "player 2 equips 9 (hands 2)") (`logs/run-20261106-022456-jtg-f3-c11-p2eq`). Type and name not read |
| Karim's things (ch4) | 10, 11, 9 | | | | Jump to Game ch4: player 1 carries 10, 11 and 9, with 9 held (frame 1471, round-3 finisher verifier) |
| TORCH_BRACKET (ch7) | | pickup script 2212 (room 123, CamTorch01) | | | After player 2's pickup, player 1's carried list is 68 then 9 (68 new, likely the torch, not confirmed) (frame 7361, `logs/run-20261106-031321-jtg-vf3-ch7-d9`); player 2's equipment still holds 0 |
| Gladius | | | | | Name shown as the selected item in player 1's Tome page, ch1 (`logs/run-20261106-044135-jtg-vov-c1-ov6/shots/000235.jpg`). Also shown on player 2's item page in ch1 (round-4 doorhold checker) |
| Flintlock Pistol Ammunition | | type id 87 ("Flintlock Pistol Ammo", MUSKET/AMMO); ch5 room 249 DESK BoxRegion01: script 2420 sets PICK UP, 2246 gives it, 2036 runs after the find | | Words: "<player> has found some Flintlock Pistol Ammunition!" | Round-4 reg-a, found by each player (`logs/run-20261106-*-jtg-rega3-c5*-r3`) |
| Meditation Rod | | | | | Selected item in the Tome at the ch7 start, Paul (`logs/run-20261106-064000-jtg-tw-ch7-t3/shots/000167.jpg`) |
| A Lucky Penny | | | | | Tome name shown in ch9 (round-4 reg-b) |
| Liquid Courage | | | | | Tome name shown in ch10 (round-4 reg-b) |
| Pius's sword, player 2's copy (ch1) | 60 | | 2 | | Node 0x81116F50, held from about frame 5052 after player 2's Tome equip; player 2's own equipment is id 59 at 0x80588F88; item 9 asks moves part 14, a sword (round-4 finisher4 and moves7) |
| Michael's item 10 (ch11) | 10 | | 4 | | "player 2 equips 10 (hands 4)"; their equipment id 133 at 0x80588F88; asks moves parts 0 and 4, so not a weapon. Name not read (round-4 moves7) |
| Mace | | placed item 62 in ch7 room 109 (Npcs7) | | | item_table, round-5 moves8 (not seen in play) |
| Crossbow | | placed item 93 in ch7 room 220 (Npcs7) and ch8 room 204 (Npcs8) | | | item_table, round-5 moves8 (not seen in play) |
| Double Shotgun | | placed item 185 in ch9 room 239 (Npcs9), also given by script 2283 | | | item_table, round-5 moves8 (not seen in play) |
| Pistol | | placed item 138 in ch11 room 50 (Npcs11) | | | item_table, round-5 moves8 (not seen in play) |
| Strange Necklace (words) | | statue L02/2240 Yes, scene 2242 | | "This Necklace can be USED to heal Ellia. Make sure that Ellia does not move when the sp..." (cut off) | Round-5 scenewords, found by each player (logs/run-20261106-104629-jtg-sw-m4-p2s-w8, -092524-jtg-sw-p2s-w3) |
| Flintlock Pistol Ammunition | | ch5 room 249 DESK (BoxRegion01, script 2420 -> find 2246), then script 2036 (ammo_merge ed131, take_item fn93) | | 'Ellia has found some Flintlock Pistol Ammunition!' (player 2, model 70); 'Karim has found some Flintlock Pistol Ammunition' (player 1, model 9). Carry 2 -> 3. The desk's examine: 'A desk cluttered with notes and writs. Mostly business to do with the Roivas Estate.' (which script gives it is unconfirmed; the trace showed 2784) | Round 6 (logs/run-20261106-134034-jtg-ev-c5bp-e2, shots/000129.jpg and 000142.jpg) |
| Strange Necklace (notices) | | statue L02/2240 Yes | | '[coop] notice for player 2: Karim has found a Strange Necklace' (player 1 found it); 'notice for player 1: Ellia has found a Strange Necklace' (player 2 found it) | Round 6 (logs/run-20261106-132649-jtg-sc-c2d-s1, -133308-jtg-sc-c2b-s1) |
| Granite Block (notice) | | ch1 room 342 CHATT_BLOCK | | 'Ellia has found a Granite Block.' (notice to player 1). Carry 1 -> 2 | Round 6 reg-a (logs/run-20261106-132405-jtg-rega5-c1cF-m5) |
| Pius's sword through player 2's Tome | 9 | ch1 room 341, equipped from player 2's Tome | | '[tome-copy] player 2 equips 9 (hands 2) in the real game'; 'player 2's own equipment (id 59)' (ch1), '(id 133)' (ch11 room 50). Lindsey (73) and Michael (81) play part 42 for it ('73's file has no part 14: their part 42'); Ellia (70) plays part 14 | Round 6 (logs/run-20261106-133702-jtg-t11-c1t-fix) |

Weapon moves parts, from the disc's InvU.bin (+54, round-4 moves7): 7 Torch, Flash Pan; 14 Sabre, Gladius, Two-Edged Sword, Ram Dao 81, Saif, Short Sword, Enchanted Gladius; 21 Ram Dao 92; 42 Tulwar, Kukri, Scramasax, Fire Ax; 56 Mace; 63 Chakrams, Automatic Pistol, Flintlock, Pistol, Revolvers, .38; 77 Shotgun, Assault Rifle, Rifle, Elephant Gun; 84 Double Shotgun; 105 Crossbow; 112 Blowgun; 119 Dual Tulwar, Dual Flintlocks, Pickax.

Finisher, slot13 (Pius's chapter, room 342, 2026-11-06): player 1 held item 96 (Gladius). The finisher's attack block (r25 +104 at `0x800C3254`) gives the item's info: +4 its InvU record, +12 its type. Player 2 (Ellia) held nothing to fight with. Weapon homes, from `tools/ed/facts/weapon_homes.py`: Gladius 64 is placed in Npcs0.npc room 71 (Alex); Gladius 96 is given by script 1200 (Pius); Kukri 97 by script 1205 (Lindsey); Sabre 101 is in Npcs5.npc (Max); Ram Dao 81 is in Npcs4.npc room 191 (Karim).

## Not yet matched to a name

- An item whose Use runs script 2243 (`0x8C3`) and casts spell `0x40011041`, in slot 1.
- Item ids a script asked for in Paul's chapter: 678 and 733. The save had them, but the log gives no names.
- Items 733 and 831: Karim's door script 132 (ch4 room 338) and Anthony's script 892 send the chapter's character to room 5 unless they carry one of them (round-1 finisher agent's reading, 2026-11-05).
- Dual Flintlocks in hand (the user's slot 1, ch5 room 258, 2026-11-07, `ed carried`): two carried things, both IN HAND, the Flintlock Pistol (type 149, mask 2) and the Dual Flintlocks (type 229, mask 1). Each gun has its own record and its own clip of 1 (player 2's copies 0x807098E0 left, 0x807098FC right). The RELOAD prompt looks at the right one; B loads the right one if it isn't full, then the left one (logs/run-20261107-111233-s1-p2fix).

## Player 2's Tome tabs

Player 2 doesn't need Journal or Cinematics in their Tome for now, so they aren't planned for a player 2 menu.
- Spawned with `ED_ITEM_SPAWN` (hle.c `item_test_spawn`), slot 7, chapter 7, room 103, player 2 (Pius, model 0) pressing B (2026-11-09, logs/run-20261109-001519-items-l7): Sheet of Music (type 113, model 65), Diary Page 1 (114) and Diary Page 2 (115) each picked up through the PICK UP prompt (script 2246); after-find scripts 1495, 1496 and 1811 ran; carried count 1 to 4. Their words read "Paul has found a Sheet of Music!" and "Paul has found a Diary Page!" while the host's notice to player 1 rightly said Pius (player 1 was Paul, the chapter's own). Types 9 and 10 (Bronze and Gold Bracelet, chapter 6 models 154 and 155) were made in this chapter but not seen and not picked up (logs/run-20261109-001433-items-ahead).
- Item sweep, all 244 types, both players, fresh slot 1 (chapter 1, room 341), 2026-11-09 (logs/item-sweep-20261109-002957-report.md): 453 of 488 finds went as a normal player's. Some rows' pickup words are only a name or the item's own text: type 97's words read "Blowgun", types 177 to 179's the whole letter from Aaron Roivas ("A letter, written with some precision, in longhand: Dear Friend, ..."). Type 127 (Sapphire Effigy, record flags 0x80) and type 188 (Piece of Green Urn) never asked PICK UP when spawned, in room 341 or chapter 7's room 103.
