# Co-op: things found that open the Tome

Traces of each thing the user asked for, first in solo, then with each player. Their rule: there's only one of each tablet and page, so both players get it. Whoever picks up the last object gets the Tome's page that plays, and the other gets a notice like `(Character name) found "Enchant Spell"`. Spells made by hand stay each player's own.

## Slot 1, the Xelotath rune, solo (`logs/run-20261103-121532-s1-solo`)

| Frame | What happens |
|---|---|
| 7205 | Script 1293 ends the look at the Blue Urn |
| 7244 | Script 1300, from a zone (lr `0x80134B38`), asks for the Tome: mode 3 asked at 7327, the Inventory at 7336 |
| 7413 | The Tome's Use of the urn: script 1429 with item 62 |
| 7419 | The Tome closes, and script 1429 goes on: the urn breaks, in words and a Broken Blue Urn given |
| 7646 | "found a Magickal Rune" (script 1429 again). Flag 266 on at 7647 |
| 7963 | That close-up over, script 1362 (lr `0x800D9BA8`) |
| 7964 | The rune: magick record `+0` goes from 0 to 4. Flag 989 on |
| 7965 | Mode 3 asked, with the Magick page (`0x8010F228`) up at 7974 |
| 7973 | The page's start (`0x8010AF94`) runs script 2020, the help words. Flags 847 and 990 on |
| 8207 on | Script 887 steps the help: Spell List, New Spell, Runes, Quick Spell. Flag 622 on |

Flag 667 is on whenever the Tome is open. The rune is written by script 1362 in the world, not by the Tome.

## Slot 1, player 2 in co-op

The urn's words, the Broken Urn, the rune and the page's help run the same script steps as solo (`logs/run-20261103-122547-s1-p2b`). After the page closed, both players were invisible, and player 1 couldn't move during it. In the state saved then (slot 9), `r13-23456` was 1. It's set by `0x8014ED04`: 1 at the Tome's start (`0x80111168`), 0 at its way out (`0x80110E50`). With a copy of that save patched to 0, the players were drawn again and walked (`Screenshots for Claude/look-p9w.png`). It's one of the switches the world keeps while a Tome shares the frames, and the world's copy had the 1. The world's copy now always has 0.

In the user's play on that fix (`logs/run-20261103-*-s1-p2d`) both were visible after the page, and the rune went in at frame 7747 with flags 266, 989, 847, 990 and 622 as in solo. Player 1 can't move from player 2's use of the urn through the page: the game's hold for words and close-ups. It's the same as the long-item hold in `coop-next-session.md`. Not fixed.

## What holds player 1 during player 2's words

The picture is `Screenshots for Claude/p1-held-1.png`. Opening words sets the busy mask (`r13-23344`, bits 8 and 2, by `0x801556CC`). The per-character update `0x80007490` asks it (`0x80155738`, called at `0x8000749C`) before it runs the player's controller, so player 1's controller doesn't run. Read in the code, not tried. To free player 1, these would all have to change while player 2's words are up: the answer that call gives for player 1 only (34 other places read the same mask), the game's pad (it's player 2's then, for the words, and player 1's controller reads the same one), and player 1's half (`coop_hold_player1` keeps a still picture of it).
