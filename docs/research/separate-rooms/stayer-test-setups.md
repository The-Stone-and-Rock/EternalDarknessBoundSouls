# Stayer-presses test setups: a chapter that casts, and player 2 into a zone exit (2026-11-13)

Read-only research for the stayer-presses runs (design-stayer-presses.md). Nothing here was run. Tags: [read] at an
address or line, [read there] in a named doc or log, [inferred].

## 1. Why chapter 1 never casts, and the earliest chapter that does

**The quick cast refuses chapters 1 and 2 outright, with no words.**
- 0x800069DC reads the word at 0x80307170 +8 = 0x80307178, the chapter word (`ed level`). At 1 or 2 it jumps
  straight to the return at 0x80006A80. At 0 it needs flag 307 (0x80006A28-0x80006A38). Only at 3 or more does it go
  on to the cooldown (0x80006980), event 185 and 0x801C4250 [read 0x80006A04-0x80006A78].
- Bit 0x4000 of the node flags (0x801F68E0) skips this gate [read 0x800069F8-0x80006A00].
- So run S2's `[coop] player 2 casts their quick spell 1 (0x00010504): not cast` was this gate (level 1, Pius). No
  words opened because the gate shows none [read there: logs/run-20261113-162946-stayer-s2-p2-quick/page-0001.log:418-420].
- The same is true of every D-pad or Y cast in chapters 1 and 2, for both players: ED_COOP_P1_QUICK, ED_QUICK_SET and
  ED_COOP_P2_QUICK all go through 0x800069DC. The host's coop_quick_cast_one calls COOP_QUICK_CAST = 0x800069DC
  [read hle.c coop_quick_cast_one].
- So the chapter-1 casts in design-lights.md C-1 and in S2/S2b/S7/S11/S12 cannot cast [inferred].
- A Tome cast (the next cast at 0x80334DFC, cast from 0x80008468 by 0x801C4250) does not pass through 0x800069DC.
  But Pius has no Tome, and neither does Ellia at her chapter's start: script 1201 gives it to her later in level 02
  [read `ed script 1201`].

**Jump to Game starts chapters 3 to 11 with no Tome.**
- Chapter 3 (Anthony): `Anthony cannot cast spells without the Tome of Eternal Darkness!` before the test's spawned
  Tome was picked up [read there: logs/run-20261109-012852-jtg-runes-uly].
- Chapter 4 (Karim): the Tome was spawned for the cast tests [read there: logs/run-20261109-024126-jtg-rec-chat].
- Chapter 5 (Max): coop-glitch-todo T4, "no Tome" [read there].
- Chapter 7 (Paul): `Paul cannot cast spells without the Tome` [read there: logs/run-20261105-221906-jtg-vp2cam-v3-ch7-exam2].
- Chapter 12 (Alex's end, room 100) has the Tome and the spells from the start. Player 2's quick spell 0x00010504
  cast there with no cheat [read there: logs/run-20261106-092849-jtg-dc3-c12p1-p2cast:486-488, no `all magick on` line].

**Earliest that casts: chapter 3 (level 3, Anthony), with two test helpers.**
- ED_CHEAT=1 gives All magick: every rune, circle and spell, and the taught flags (hle.c cheat_all_magick, kMagickTaught).
- ED_ITEM_SPAWN puts the Tome (type 30) at player 1's feet, to pick up with B.
- The Tome is the two players' together. Either player carrying it answers 0x80109A68 for both [read hle.c coop_has_tome].
- This exact pair cast in chapter 3: 0x00010482, `cast by 0x1` at 2782, 2912 and 2935
  [read there: logs/run-20261109-022842-jtg-fx-dispel:487-529, 1196].
- With no cheat, use chapter 12 (section 1d).

### The spell word (a quick slot's)

`(circle << 16) | verb | noun | alignment`:
- circle: 0x1 is 3 points, 0x2 is 5, 0x4 is 7 [inferred from kAllSpells' circle masks 0x07/0x01/0x02/0x04 at +16].
- verb is `& 0x1E00`, noun is `& 0x1F0` [read hle.c rune glyph order].
- alignment bit: 1 Chattur'gha, 2 Ulyaoth, 4 Xel'lotath, 8 Mantorok [read hle.c `word & 1u ? 1u : word & 2u ...`].
- The cast picks its class by `spell & 0x1FF0` (0x801C43D8) [read there: design-stayer-presses.md:809].
- Words seen cast: 0x00010504, 0x00010508, 0x00010481, 0x00010482, 0x00010302, 0x00010304, 0x00011041, 0x00011012
  (players), and 0x00020821 (`cast by 0x5`, a chapter 11 creature) [read there: page logs].
- The spell list's low nibble (kAllSpells 0x...9 / 0x...C) is not the allowed alignments. 0x00010482 cast from
  0x0C070489 [inferred].
- The format: no 0x prefix needed (strtoul base 16). ED_COOP_P2_QUICK goes in player 2's slot 1, D-pad `up` in
  ED_PAD2_SCRIPT (not `dup`). ED_COOP_P1_QUICK goes in player 1's slot 1, D-pad up 0x0008 in ED_INPUT_SCRIPT.
  ED_QUICK_SET=FRAME,0,WORD goes in player 1's slot 0, Y 0x0800 [read hle.c coop_quick_cast, kCoopQuickButton,
  quick_test_set; read there: coop-failed-attempts.md:247].
- ED_COOP_P2_MAGICK=60 sets player 2's magick once [read hle.c].

| Class | Word to try | Where from | Gate |
|---|---|---|---|
| 0x500 (the usual test) | `00010504` | seen cast in chapters 3 and 12 [read there] | none beyond the Tome and magick |
| 0x810 | `00010812` (Ulyaoth, 3 points; 11/14/18 for the others) | kAllSpells 0x0C07081C [read hle.c] and the observed nibble rule [inferred] | a creature in the room in play (0x801F662C against r13-23276), else words 0,2 [read there: presses-effects-readers.md] |
| 0x820 summon | `00020821` first | the game itself cast it (chapter 11) [read there] | refused in rooms 71, 104, 236 and 58 (words 0,11). Kept to the tail by the stayer build. |
| 0x820 summon | `00010824` (3 points), `00040821` (7 points) | kAllSpells 0x0C010824, 0x0C040828 [read hle.c] | which word is Summon Trapper (E8's switch-player load) is unread. Try 00010824 last [inferred]. |

No spell name in the table is checked against the Tome's page. Read `[coop] spell 0x... (WORD) cast by` and look for
words 0,2 / 0,7 / 0,11.

### 1a. The room: 153 in level 3

`ed room 153 --level 3`, `ed door`, `ed examine`, placed_things.json level 3 [read]:
- **Door Region02**, box (-147,-518,-119)-(44,-239,200), centre (-51,-378). Script **1139**: OPEN DOOR (prompt 2,1),
  then go_to_room(151, EntryMkr02, Camera01, **transition 9**, then 1161). Motion 25, **62 ticks**. It has no flag gate.
  It works as 183's script 119 does [read `ed script 1139 --level 3`].
- **EXAMINE FOUNTAIN**, box CameraRgn03 (982,-322,-19)-(1430,75,337). Script 1274: `examine[ed219](139, 0, Camera03, 1)`,
  a close-up on Camera03 plus words, as 183's EXAMINE [read].
- More examines: TAPESTRY, box (255,-1563)-(514,-1224), script 795, an ed219 close-up on TapestryCam01 while flag
  1128 is off. RED URN, CameraRgn04, ed221, offers the Filled Red Urn. CODEX_PEDESTAL, Regions 03/04, ed221.
- **Creatures for 0x810:** two section-0 model 8 / chr 24 placings at (1184,-1001,0) and (133,-1188,0), and an
  alignment creature (chr 13/22/20) at (1079,-302,0), beside the fountain. Model 8 / chr 24 is what ch1's 183
  zombies are: nodes 38, 39 and 33 sit at those placings' places
  [read there: logs/run-20261113-163251-stayer-s11b-apart-p1 change-hold rows].
- A prior check saw "others casting 0" in 153 on the user's slot-3 save, so whether they are alive on a fresh Jump
  to Game is not known [read there: Screenshots for Claude/chapters/camfix/03-Anthony.txt].
- **Entering 153 by script 1106** (154's door): go_to_room(153, EntryMkr02 (384,-1434,0), Camera02, transition 9,
  then 1162). Script 1162 only sets Camera02 [read].
- Do not use 675 (151 to 153). With flag 56 off it plays the TransformCam01 scene, held 120 frames in camfix
  [read `ed script 675`; read there: camfix report].
- Do not use 892 (150 to 147). Its Tome-and-model tests sent camfix to room 5 [read there].
- A ladder door for a script-132-style run in level 3 is in room 147: ExitRgn01, sphere (926,678,0) r 181, script
  1097, CLIMB DOWN to 21, transition 15, 62 ticks.

### 1b. Making the fresh state (one Jump to Game, as S0 did)

The menu presses and base 2300 are jtg.sh's. Chapter 3 control came at 2123 and 2172 [read there: the two runs'
jtg-report.txt].

```sh
cd "/e/Eternal Darkness Recomp Fork"
rm -f saves/rooms-ch3-153.state
JTG_SLOT=1 JTG_P1=1 JTG_P2=2 env -u ED_COOP_STAY ED_CHEAT=1 ED_ITEM_SPAWN=2400,30,30,300,1 \
  JTG_P1_SCRIPT="130,138,0x0200;190,198,0x0200;230,238,0x0200;270,278,0x0200" \
  ED_RUN_SCRIPT=2750:1106,2810:1106 ED_SAVE_STATE=3050:saves/rooms-ch3-153.state ED_RUN_FRAMES=3150 \
  sh tools/ed/jtg.sh 3 stayer-c3-153 </dev/null
SET=$(ls -d logs/run-*-jtg-stayer-c3-153 | tail -1)/settings.ini; export ED_SETTINGS="$SET"; ST=saves/rooms-ch3-153.state
```

- JTG_P1=1 / JTG_P2=2 are the stayer runs' own characters (S0's settings.ini: P1 Pius model 0, P2 Ellia model 70).
  This also keeps the Tome's after-find quiet. Script 1599 branches on player 1's model: 85 plays cinematic 112 and
  moves the player to room 150's EntryMkr_TomeReturn, 9 goes to room 64, and model 0 has no branch [read
  `ed script 1599 --level 3`]. With Anthony (JTG_P1=0) the cinematic runs: 1599 at 2541, 868 at 2649 in fx-dispel.
- B at 2430, 2490, 2530 and 2570 picks up the Tome and closes its words, as fx-dispel did. Look for
  `has acquired the Tome` and `[coop] notice for player 2`.
- ED_RUN_SCRIPT=2750:1106,2810:1106: the first run shows the prompt and queues 153, the second goes. Follow mode
  (`-u ED_COOP_STAY`) takes both players.
- Check before using the state: `[room] ticket` near 2812, both players in room 153 by ~2875 (`ed timeline`), and
  the first frame the state loads at (both.tsv's first row, as chapters.py first_frame reads it). Call it T0. The
  frames below take T0 = 3050.
- Pass ED_CHEAT=1 again on the runs from this state: cheat_apply only puts back what it changed itself [read
  hle.c cheat_apply].

### 1c. The runs from it (the ch1 lines, moved to 153)

```sh
export ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1 ED_CHANGE_TIME=1 ED_RUN_SHOTS_MS=250
L="ED_COOP_LOG=1 ED_COOP_ENEMY_LOG=1 ED_CHANGE_HOLD=1 ED_STAYER_LOG=1 ED_COOP_TRACE=1 ED_COOP_CAST_TRACE=1 ED_COOP_CAST_LOG=1 ED_CHEAT=1"
P1GO="ED_RUN_SCRIPT=110:1139,170:1139"   # player 1's door 153 -> 151: prompt T0+110, ticket ~T0+172 = ~3222, window to ~3284
DOOR="-60,-400,-100"                       # inside Region02 (ray from z 200 down: the floor by EntryMkr01, z ~-110)
FOUNT="1150,-100,0"                        # inside CameraRgn03 (FOUNTAIN)
D2="3160,3170,b"                           # player 2's own OPEN DOOR: ticket ~3160 (S0's 3760 = T0+110)
# player 2 stays: quick cast in player 1's window (S2), the 0x810 (S7b), a summon (kept to the tail)
env $L $P1GO ED_COOP_P2_QUICK=00010504 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=3600 ED_PUT="3080,1,$DOOR;3080,2,$FOUNT" ED_PAD2_SCRIPT="3240,3246,up" sh tools/ed/run.sh $ST stayer-c3-p2-quick </dev/null
env $L $P1GO ED_COOP_P2_QUICK=00010812 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=3600 ED_PUT="3080,1,$DOOR;3080,2,$FOUNT" ED_PAD2_SCRIPT="3240,3246,up" sh tools/ed/run.sh $ST stayer-c3-p2-810 </dev/null
env $L $P1GO ED_COOP_P2_QUICK=00020821 ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=3700 ED_PUT="3080,1,$DOOR;3080,2,$FOUNT" ED_PAD2_SCRIPT="3240,3246,up" sh tools/ed/run.sh $ST stayer-c3-p2-summon </dev/null
# F3 in chapter 3: player 2's copy Tome in player 1's window (S3's presses, +30 from the ticket)
env $L $P1GO ED_COOP_P2_MAGICK=60 ED_RUN_FRAMES=3700 ED_PUT="3080,1,$DOOR;3080,2,$FOUNT" ED_PAD2_SCRIPT="3230,3236,start;3260,3266,right;3280,3286,b;3300,3306,b;3340,3346,b" sh tools/ed/run.sh $ST stayer-c3-p2-start </dev/null
# player 1 stays: quick (S7), then F2's two orders: the close-up begun BEFORE player 2's ticket, and inside the window
env $L ED_COOP_P1_QUICK=00010504 ED_RUN_FRAMES=3500 ED_PUT="3080,1,$FOUNT;3080,2,$DOOR" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3175,3181,0x0008" sh tools/ed/run.sh $ST stayer-c3-p1-quick </dev/null
env $L ED_RUN_FRAMES=3600 ED_PUT="3080,1,$FOUNT;3080,2,$DOOR" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3140,3146,0x0200" sh tools/ed/run.sh $ST stayer-c3-p1-exam-before </dev/null
env $L ED_RUN_FRAMES=3600 ED_PUT="3080,1,$FOUNT;3080,2,$DOOR" ED_PAD2_SCRIPT="$D2" ED_INPUT_SCRIPT="3180,3186,0x0200" sh tools/ed/run.sh $ST stayer-c3-p1-exam-in </dev/null
```

- The frames assume T0 = 3050. Shift them all by (T0 - 3050).
- The exam-before run answers F2's open question in a room that casts. In chapter 1 the same thing is S6 with
  `ED_INPUT_SCRIPT="3740,3746,0x0200"` (B before the 3760 ticket), with no magick needed:
  `env $L ED_RUN_FRAMES=4300 ED_PUT="3680,1,1305,1508,185;3680,2,360,3090,185" ED_PAD2_SCRIPT="3760,3770,b" ED_INPUT_SCRIPT="3740,3746,0x0200" sh tools/ed/run.sh saves/rooms-ch1-183.state stayer-s6c-p1-exam-before`.
- Read `[cast-trace]`, `[coop] player N casts ... : cast` and `[coop] spell 0x... cast by`. A creature in 153 may
  walk up during a run. `ed timeline` shows it.

### 1d. With no cheat: chapter 12 (Alex's end)

- Alex has the Tome and spells [read there: the c12 runs above].
- Room 100 has OPEN DOOR 567 to room 93 (transition 9, 62 ticks) and walk-in boxes 633-638 (transition 3, 62 ticks).
  It has no examine [read `ed door 100`, `ed examine 100`].
- The c12 runs used `JTG_SLOW=1` (grid presses 60 apart) with JTG_BASE=4300. Control came at 3358
  [read there: logs/run-20261106-092849-jtg-dc3-c12p1-p2cast/jtg.txt, jtg-report.txt].
- Item spawns make nothing in levels 12 to 15 [read hle.c ED_ITEM_SPAWN comment].
- Use it for a cast with no helper. Pick an examine room from `ed examine --level 12` first.

## 2. Player 2 into room 180's Region03: why S11b made no ticket, and a working recipe

**It is not a walk-in. It is a prompted ladder.**
- Script 28 at state 0: room_queue(344), prompt(2, 3) CLIMB UP, state 1. At state 1, on the press:
  go_to_room(344, EntryMkr02, Camera05, transition 12) [read `ed script 28 --level 1`; `ed door 180`: "asks CLIMB UP"].

**In S11b the zone did fire for player 2.**
- The put landed them at z 310: the floor ray starts 300 above the given z, and at (-1075,78) it hit something at
  ~308 [read hle.c sweep_put].
- They fell to z 0 at x ~-1022 by 4125 [read there: both.tsv 4101-4125]. The 4102-4110 `lup` came while they fell.
- Then `script run r3 28 ... who 59 ... frame 4122`, `words: CLIMB UP`, and `[coop] player 2's prompt 3`
  [read there: logs/run-20261113-163251-stayer-s11b-apart-p1/page-0001.log:1613-1623].
- Nothing pressed B after it, so no ticket. S11's player-1 ladder used ED_RUN_SCRIPT=420:28,480:28, which is the
  two runs, prompt then press [read there: build-run-stayer-presses.sh].

**How earlier runs walked a player into a zone.**
- coop-failed-attempts.md: "Walking into a zone is reliable as put outside, step, put inside, step" [read there].
- The stick is camera-relative, and a teleported player keeps their old camera until a camera zone changes, so a
  long stick press drifts them out of a 100-unit zone in ~30 frames [read there].
- S4 does exactly that for 183's CameraRgn05.
- Door regions take a single put inside. Player 2 was put in 183's Region01 at 3680, and their B at 3760 gave
  `player 2's B: prompt 1 script 119` and the 3760 ticket [read there: s11b page-0001.log:79-91].

**Recipe (S11c).** This is S11b with the put below the ladder's top, the outside-then-inside steps, and player 2's B
on CLIMB UP:

```sh
env $L ED_COOP_P1_QUICK=00010504 ED_RUN_FRAMES=4800 \
  ED_PUT="3680,1,1110,1170,185;3680,2,360,3090,185;4090,1,1305,1508,185;4100,2,-900,78,-150;4130,2,-1023,76,-150" \
  ED_PAD2_SCRIPT="3760,3770,b;4104,4110,lup;4134,4138,lup;4170,4176,b" \
  ED_INPUT_SCRIPT="4185,4191,0x0200;4205,4211,0x0200;4230,4236,0x0008;4260,4266,0x0200;4290,4296,0x0200" \
  sh tools/ed/run.sh saves/rooms-ch1-183.state stayer-s11c-apart-p1 </dev/null
```

- (-900,78) is 175 from Region03's centre, so outside its r 109. (-1023,76) is EntryMkr03, 52 from the centre, so
  inside. z -150 sends the ray from z 150, under whatever sits at ~308, onto the floor at ~0 [inferred from sweep_put
  and the S11b fall].
- The short steps (4-6 frames) stay inside the sphere.
- Check for `script run r3 28 ... who 59`, then `player 2's prompt 3`, then `[coop] player 2's B: prompt 3 script 28`,
  then `[room] ticket 0x00000158` (344) at ~4170. The window runs ~4170-4232.
- Player 1's presses in the window, timed off that ticket: B on 183's EXAMINE (4185), B (4205), the quick spell
  (4230), B (4260, 4290). Player 1's quick is refused in chapter 1 by the level gate (section 1); it only shows that
  the press is taken.
- If the ticket comes at another frame, move player 1's presses by the same amount.
- If the B is not taken (no `player 2's B` line), the prompt may have gone with player 2's put: press B again 30
  frames later (`4200,4206,b`).
- Room 344 is the climb's other end: `ed room 344` before testing the return (script 111, CameraRgn07, CLIMB DOWN).

## Facts index

| Fact | Where | Tag |
|---|---|---|
| quick cast gate: chapter word 1/2 refused, 0 needs flag 307 | 0x80006A04-0x80006A38 | [read] |
| node flag 0x4000 skips the gate | 0x800069F8-0x80006A00 | [read] |
| Tome shared between players for casting | hle.c coop_has_tome | [read] |
| All magick: kAllSpells, taught flags | hle.c cheat_all_magick, kMagickTaught | [read] |
| alignment bits 1/2/4/8 | hle.c rune code (`word & 1u ? 1u : ...`) | [read] |
| ED_PUT ray from z+300 to z-1500 | hle.c sweep_put | [read] |
| script 28 is a prompted ladder | `ed script 28 --level 1` | [read] |
| S11b: zone fired at 4122, CLIMB UP shown, no B | s11b page-0001.log:1613-1623 | [read there] |
| script 1599's model branches | `ed script 1599 --level 3` | [read] |
| room 153 door 1139, FOUNTAIN 1274, placings | `ed room/door/examine 153`, placed_things.json | [read] |
| ch3 cast with ED_CHEAT=1 + Tome spawn | logs/run-20261109-022842-jtg-fx-dispel | [read there] |
| ch12 cast with no cheat | logs/run-20261106-092849-jtg-dc3-c12p1-p2cast | [read there] |
| 0x00020821 cast in the game | logs/run-20261105-224706-jtg-vfin-ch11c-f0 | [read there] |
| 0x810 / 0x820 words for players | kAllSpells + observed words | [inferred] |
