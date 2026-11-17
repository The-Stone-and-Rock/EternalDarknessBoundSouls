# The sanity effect's white flash: why it looks blocky, and the fix

The user's words (2026-11-17): "The sanity effect by the way I noticed when for example you answered the phone and the
screen on the player that triggered the effect flashes white, which ends the sanity effect I think, the white flash is
blocky and not a smooth white flash, that needs reviewed and fixed."

Read only. Nothing was built, run or changed for this page. Evidence is a file:line, a guest address in
`build/ed-db/disasm.txt`, a log line or a picture. Anything else says "inferred".

## Short answer

- The flash is the game's own "ed85" flash, 0x80145760. It is a flat, untextured disc drawn over the screen. It does not
  copy the screen, so texture filtering and copy settings have nothing to do with it.
- The "blocks" are one hard-edged wedge: a triangle from the screen centre, straight down, and up to the top-left corner.
  It shows in every run picture of the flash, solo and co-op.
- Cause: the game draws a 64-point fan but only fills 21 points. It marks the other 43 with z = +1, just behind the near
  plane, and leaves their x,y at whatever is there (0,0 by the pictures). On the GameCube those get clipped away. The
  host's near-plane snap (`.deps/GXRuntime/graphics/gxcore/src/wgsl.cpp:346-347`) pulls them back onto the plane, so one
  extra triangle is drawn: centre, last rim point, guest (0,0). Its far corners are never coloured by the game, so it
  does not fade with the disc.
- Fix: a small host notify on the disc's submit 0x8017C9DC (it has one caller) that moves the unused points onto the
  last used point. The extra triangle then has no area. No renderer change is needed.

## 1. What the game does

### Who calls it

- The flash is 0x80145760(a, b, c). The script command ed85 (0x80014D10) calls it at 0x80014DB0.
- The phone call (script 1801, room 69) ends with ed85(0,0,0) (`ed script 1801`, from the investigators; not re-read).
- Native callers (bl sites in disasm.txt): 0x800692FC (sanity code), 0x8009DFEC (the general "effect ends" path),
  0x800C0540, 0x80106EFC (Tome), 0x801112D4, 0x80113CCC, 0x801142F4.
- It plays sound 536 at 100 itself: `li r3,536` / `bl 0x8019DB90` at 0x80145878-0x80145880. That is the sound the sanity
  log notes at each return: `logs/run-20261104-232036-sanity2-17/page-0001.log`, "frame 3059 chapter 2 room 166:   sound
  536 (from 0x8019DBDC)", with "17 Head of Shakespeare ended after 1025 frames" at frame 3072.

### How it is made

- 0x80145760 fills a block with the default template 0x80183818 (bl at 0x801457C0), sets two colours with 0x801838B4
  (0x80145804), and places it at floats (320, 240) from 0x80622C24 (read from `generated/main.dol`: `43A00000 43700000`).
- Both colours come from 0x80622C1C: `E6E6FFFE E6E6FFFE`, so RGBA 230,230,255,254. A soft lavender white, by design.
- Its maker is 0x80183910 (`addi r4, r3, 14608` with r3 = 0x80180000, at 0x80145814). It goes to the queue maker
  0x801413E8 at 0x80145850.
- 0x80183910 calls 0x80183950, which sets update +332 = 0x80183C20 and draw +328 = 0x80183D18 (0x801839B8-0x801839C8).
  Then it calls 0x801747D0 (`stw r4, 328(r3)`) with r4 = 0x80183E1C (0x8018392C-0x80183934). So the draw actually used
  is 0x80183E1C, the screen-space one. (Investigator 2 read 0x80183D18; that one is overwritten.)
- Two other makers also use 0x80183910, so they draw the same way: 0x8006B2E8 (sanity code, stores it at 0x8006B30C)
  and 0x801D68F4 (stores it at 0x801D6948). What those two look like on screen was not checked.

### How it is drawn: 0x80183E1C

- Vertex buffers come from 0x80181834 (bl at 0x80183E6C). Their size comes from the halfword at 0x805D7102 (0x80183E2C
  copies 16 bytes from 0x805D7100; `lhz r6, 22(r1)` at 0x80183E60). 0x805D7100 is not in the DOL, so it is set at run
  time. 128 vertices (64 pairs) is inferred from the loop below.
- 0x8018432C (bl at 0x80183E80) fills the used points: 21 pairs, 6 bytes a vertex (x, y, z as s16).
- The unused points (0x80183E84-0x80183F14):
  - `lbz r3, 1(r30)` is the used count. `cmpwi r3, 64` / `subfic r3, r3, 64` give the number left.
  - The loop does `sth r4, 4(r5)` and `sth r4, 10(r5)` with r4 = 1, 12 bytes a pair. So it writes only z = 1 to both
    vertices of each unused pair. Their x and y are never written.
- The submit 0x8017C9DC (bl at 0x80183F64, its only caller in disasm.txt):
  - 0x8019CFD4(3) (0x8017CA00): the screen-space setup. It builds an ortho with C_MTXOrtho (bl 0x80205EE8). Its floats,
    read from the DOL at 0x806233E8: near 1.0, top 0, bottom 480, left -80 / right 720 in the wide branch (r13-24660 ==
    1, 0x8019CFF8), 640 in the other, far 30360.
  - Arrays: POS = r3, CLR0 = r5, TEX0 = r4 (GXSetArray 9, 11, 13 at 0x8017CA18-0x8017CA40).
  - Z mode on, LEQUAL, no update (0x801E1648(1,3,0) at 0x8017CA50).
  - GXCallDisplayList(0x805D6AC0, r13-23084) at 0x8017CA60.
- The display list (built at 0x8017C4BC-0x8017C5E4): GX_QUADS (`li r3,128`), 2n-4 vertices, indices 1,0, then
  k, k+1, k+1, k for each next pair. So every quad joins pair i to pair i+1, and the list always covers all 64 pairs.

### Why the GameCube hides the unused points

- The used points sit at z = -1, on the near plane (near = 1).
- The unused points sit at z = +1. With this ortho their clip z is (-1 - 30360) / 30359, about 6.6e-5 beyond the near
  plane. Clipping removes them.
- The quad from the last used pair to the first unused pair has two corners on the plane and two behind it. Clipped,
  nothing of it is left. The other 42 unused quads have all four corners in one place, so no area either way.
- That GX hardware clips here as Dolphin does is inferred. There is no Dolphin reference in the repo.

## 2. What the host does

- The renderer turns clip z around for reversed depth: `clip.z = clip.w * pcc.w - clip.z * pcc.z`
  (`wgsl.cpp:332-333`), with pcc.z = 1, pcc.w = 0 by default (`graphics/gxcore/src/raster.cpp`, "Default: no VS depth
  remap — formula o.pos.z = w*0 - z*1 = -z").
- Then it snaps anything within 1e-3 of either plane onto it (`wgsl.cpp:341-347`):
  `if (clip.z > clip.w && clip.z < clip.w + 1e-3 * abs(clip.w)) { clip.z = clip.w; }`
- The comment says why: the title screens put an ortho quad exactly on the near plane, and rounding clipped it. It came
  in with the first boot (git log -S on the GXRuntime repo: f6c1c16; the line is in
  `patches/gxruntime/eternal-darkness.patch:25717`).
- The flash's unused points are 6.6e-5 out, well inside 1e-3. So they are snapped back and kept.
- The renderer splits each quad into {0,1,2} and {2,3,0} (`.deps/GXRuntime/graphics/frontend/src/render_sink.cpp:618-632`).
  For the bad quad (last rim, centre, unused, unused) the first triangle is real: last rim, centre, the unused point.
- No host code touches this flash today. Searching `runtime/host` for 0x80145760, 0x80183910, 0x80183E1C, 0x8017C9DC and
  0x805D6AC0 finds nothing. `docs/coop-failed-attempts.md` has no flash or near-plane entry for it.

## 3. What is wrong on screen, and why it matches

- `logs/run-20261104-232036-sanity2-17/shots/000068.jpg` (solo, Head of Shakespeare's return, 960x540): the whole screen
  lavender, with a darker triangle. Its tip is at the centre (480,270). One edge runs straight down. The other goes up
  to the top-left, near (120,25). Its bottom edge meets the screen bottom near x = 340.
- The game's last rim point is at angle 0, straight down: (320, 240 + 512). If the unused x,y are (0,0), the triangle is
  (320,240), (320,752), (0,0) in guest units. With the wide ortho (-80..720):
  - guest x = 0 is NDC -0.8, pixel 96 on a 960 picture, top row. The picture's corner is near (120,25). Close.
  - the edge from (0,0) to (320,752) crosses guest y = 480 at x = 204, which is pixel 341. The picture: about 340.
  - So the unused x,y are (0,0). Inferred from this match; step 1's log confirms it.
- `logs/run-20261117-103854-sub-before-p2-follow/shots/000064.jpg` (co-op, the phone in room 69): the same wedge in the
  left half, squeezed to the half's shape, while the flash fades.
- The far corners take colours the game never wrote for this flash (stale buffer data), so the wedge has its own
  gradient and stays after the disc fades (000069-000073.jpg in the solo run, per Investigator 1; 000065-000066.jpg in
  the co-op run, per Investigator 2).
- The flash jumps from nothing to full in one picture: `shots/shots.csv` rows 000062 (0.022) to 000063 (204.241) in the
  co-op run. That is the game's design: the radius grows 100 a tick to 512, so it covers the screen in about 5 ticks
  (Investigator 1, from 0x80183C20 / 0x801761A8; not re-read). For those few ticks it is a 20-sided shape, not a circle.

## 4. The fix, step by step

### Step 1. A one-shot log (same build as step 2)

- Where: `runtime/host/hle.c`, a notify on 0x8017C9DC (see step 2), behind `ED_FLASH_LOG=1`.
- What it prints, once per flash: the frame, the count (`lbz 1(r30)`, r30 = the effect, still set at the submit's entry),
  n (halfword 0x805D7102), the x,y,z and RGBA of pair count-1 and pair count, before and after step 2's change.
- Why: it proves the unused x,y are (0,0) or stale, and that count is 21. Nothing on screen changes from the log.

### Step 2. Fold the unused points onto the last used one (the fix)

- Where: `runtime/host/hle.c`, a new notify, installed next to `sanity_tool_install` (hle.c:27689) so it runs in solo and
  co-op. Not inside `fx_install` (hle.c:16133): that one is co-op only.
- Install checks, in the file's usual style:
  - `strcmp(ED_GAME_ID, "GEDP01") == 0`
  - `mem_read32(cpu, 0x8017C9DCu) == 0x9421FFE0u` (stwu r1,-32(r1), disasm.txt)
  - `stayer_bl_is(cpu, 0x80183F64u, 0x8017C9DCu)` (hle.c:28599)
  - `entry->notify == NULL`. Otherwise print one `[effects]` line saying the flash keeps its wedge.
- What it does at the entry of 0x8017C9DC:
  - Only when `cpu->lr == 0x80183F68u` (the one caller). The game's own code still does everything else.
  - pos = r3, the effect = r30, count = byte at effect+1. Give up if count is 0 or 64 or more, or if
    `guest_ram_range_valid(cpu, pos, 64 * 12)` fails.
  - For each pair p from count to 63: copy x,y of vertex 0 of pair count-1 into vertex 0 of pair p, and x,y of vertex 1
    of pair count-1 into vertex 1 of pair p (s16, big-endian, `mem_read16` / `mem_write16`). Leave the game's z = 1.
  - The bad quad becomes (last rim, centre, centre, last rim): no area. The other unused quads already have none.
- Sketch (plan, not written to source):

```c
#define FLASH_SUBMIT 0x8017C9DCu  // the screen disc's submit (0x80183E1C's, its one caller at 0x80183F64)
static void flash_submit_notify(CPUState* cpu) {
    if (cpu->lr != 0x80183F68u) return;
    const u32 pos = cpu->gpr[3], count = mem_read8(cpu, cpu->gpr[30] + 1u);
    if (count == 0u || count >= 64u || !guest_ram_range_valid(cpu, pos, 64u * 12u)) return;
    const u32 last = pos + (count - 1u) * 12u;
    for (u32 p = count; p < 64u; p++)
        for (u32 v = 0u; v < 2u; v++) {
            mem_write16(cpu, pos + p * 12u + v * 6u + 0u, mem_read16(cpu, last + v * 6u + 0u));
            mem_write16(cpu, pos + p * 12u + v * 6u + 2u, mem_read16(cpu, last + v * 6u + 2u));
        }
}
```

- What else it touches: every draw through 0x80183E1C. That is the ed85 flash (all script and native callers above,
  the Tome's a=20 one too) and the two other makers, 0x8006B2E8 and 0x801D68F4. All of them get the same fix. Nothing
  else calls 0x8017C9DC.
- Risks:
  - If the renderer read the vertex arrays before the display list call, the write would come too late. It is written
    before GXSetArray even runs, so this is safe (inferred).
  - If n were not 128, 64 pairs would be wrong. The game's own loop assumes 64 (`cmpwi r3,64`), so the buffer holds at
    least that. Step 1's log prints n.
  - Interpolated frames (120 FPS) replay the recorded draw, so they carry the fixed points (inferred).

### Step 3. Leave the renderer's snap alone (unless step 2 is not enough)

- Narrowing the 1e-3 in `wgsl.cpp:346-347` (for example to 1e-5) would also hide the wedge, and any other effect that
  uses the same "z = +1 is gone" trick.
- But it changes every draw in the game, and the title screens are the reason it is there. Their real offset was never
  measured. Do it only if another wedge shows up elsewhere, as its own change.
- If done: rebuild GXRuntime with the VS2022 tools, then `git -C .deps/GXRuntime diff > patches/gxruntime/eternal-darkness.patch`
  (CLAUDE.md). Check the title, the Options page, Choose Character and the prologue pictures for missing quads.

### Step 4. Not this bug, noted for the co-op work

- In `logs/run-20261117-103854-sub-before-p2-follow`, player 2 pressed B (`env.txt`: `ED_PAD2_SCRIPT=4502,4508,lup;4560,4566,b`)
  and `EVENTS.log` line 12 says "[coop] script 1801 runs for player 2 (frame 4560)". But the flash is in the left half,
  player 1's (`shots/000063.jpg`, `000064.jpg`), and the right half still shows "ANSWER PHONE".
- So the flash goes to the wrong player's half when player 2 answers. How the renderer chooses a half for this
  screen-space draw was not read (inferred: player 1's pass). The spell flash already has a per-half mark
  (`fx_flash_draw`, hle.c:26507-26535; `notify_GXLoadPosMtxImm`, hle.c:3431). The same idea would fit here: note the
  effect's player when 0x80145760 runs, and mark the draw out of the other half. That is co-op work: read
  `docs/coop-no-pause.md` and `docs/coop-failed-attempts.md` first.

### Not a fix for this

- Linear filtering, bigger copies or upscaling. The flash has no texture and no copy.
- Changing the colour or the fade. Lavender 230,230,255 and the linear fade are the game's own.
- More rim points to round the first 5 ticks. The records and the buffer pool were not read; it would change the
  game's look.

## 5. How to check each step

Rules for these runs: fresh Jump-to-Game or title runs only, `ED_RUN_AWAY=1`, never `ed test`.

- A way to get a flash in a fresh run (part of the step 2 build, inferred safe): `ED_FLASH_TEST=FRAME,A` calls the game's
  own 0x80145760(A,0,0) once at FRAME from a per-frame host step. `sanity_tool_step` (hle.c:27622) is the model, but it
  returns early in co-op, so the call must sit before that return, or in a co-op per-frame step. 0x80145760 queues the
  disc and plays sound 536, as a real return does.
- Step 1 and 2 together, co-op, chapter 1 (control at about frame 1870 per `tools/ed/jtg.sh`'s notes):
  `ED_RUN_AWAY=1 ED_FLASH_LOG=1 ED_FLASH_TEST=2100,0 ED_RUN_FRAMES=2400 ED_RUN_SHOTS_MS=50 sh tools/ed/jtg.sh 1 flash-fold`
  - Log: one line with count 21, n 128, pair 21's x,y before (expect 0,0) and after (equal to pair 20's).
  - Pictures (`logs/run-*-jtg-flash-fold/shots/`): the disc grows from the centre, covers the half, and fades evenly. No
    dark triangle from the centre to the top-left, no hard vertical line down the middle, nothing left after the fade.
    Compare with `logs/run-20261104-232036-sanity2-17/shots/000068.jpg` and
    `logs/run-20261117-103854-sub-before-p2-follow/shots/000064.jpg`.
  - The install line in the log says the notify was taken.
- Same run with `ED_FLASH_TEST=2100,20` (the Tome's a=20 flash, slower to start): same checks.
- A title run, to show nothing else moved: the title, Options and Choose Character pictures match the last good run.
  Step 2 does not touch them; step 3 would.
- Last, the user's own play: answer the phone in room 69, and play a sanity effect to its end in single player. These
  need a save, so they are the user's checks, not a workflow run.
- After it works: add a line to `docs/game/systems/sanity.md` (the flash, 0x80145760, and the notify), and add any try
  that fails to `docs/coop-failed-attempts.md` with its code.

## Limits

- The unused x,y being (0,0) is inferred from the picture match. Step 1 confirms it.
- n = 128 is inferred from the 64-pair loop.
- The blend mode set by 0x8018117C / 0x801E21FC was not read.
- The growth per tick and the fade rate are Investigator 1's reading of 0x80183C20; not re-read here.
- Whether the GameCube showed the wedge is inferred "no", from how GX clips; there is no Dolphin picture.
- What 0x8006B2E8's and 0x801D68F4's discs are, on screen, was not checked.
- Which half the renderer picks for a screen-space draw in co-op was not read.

## Key files

- `E:\Eternal Darkness Recomp Fork\build\ed-db\disasm.txt` (0x80145760, 0x80183910, 0x80183E1C, 0x8017C9DC, 0x8017C4BC)
- `E:\Eternal Darkness Recomp Fork\.deps\GXRuntime\graphics\gxcore\src\wgsl.cpp:341-347`
- `E:\Eternal Darkness Recomp Fork\.deps\GXRuntime\graphics\frontend\src\render_sink.cpp:618-632`
- `E:\Eternal Darkness Recomp Fork\runtime\host\hle.c` (sanity_tool_install 27689, sanity_tool_step 27622,
  stayer_bl_is 28599, fx_flash_draw 26507)
- `E:\Eternal Darkness Recomp Fork\logs\run-20261104-232036-sanity2-17\shots\000068.jpg`
- `E:\Eternal Darkness Recomp Fork\logs\run-20261117-103854-sub-before-p2-follow\shots\000064.jpg`

## Skeptic's check

Read only (2026-11-17). Re-read: disasm.txt, the DOL constants (generated/main.dol, r2 = 0x80628460, the one r2 that
gives both of the doc's addresses), hle.c, wgsl.cpp, render_sink.cpp, raster.cpp, the patch, the two runs' logs and
pictures 000068/000071 (solo) and 000063/000064/000067 (co-op). hle.c changed while I read it (another workflow is
working), so hle.c places are given by function name.

### Confirmed

- **Which draw.** 0x80183950 sets draw +328 = 0x80183D18 (0x801839C4-0x801839C8). Then 0x801747D0 (`stw r4,328(r3)`)
  puts in 0x80183E1C (0x8018392C-0x80183934). 0x80183950 has one caller (0x80183920), so 0x80183D18 never draws.
- **Count 21, now with its source.** The template 0x80183818 writes 21 to byte 0 (0x8018381C, 0x80183850). 0x801839F4
  copies it to effect+1 (`lbz r25,0(r30)` 0x80183A30, `stb r25,1(r27)` 0x80183A50).
- **Untextured.** The template's +4 is -1 (0x80183824, 0x80183860). It goes to effect+14 (0x80183A5C-0x80183A60).
  0x8018117C then takes the -1 branch (0x801811AC-0x801811CC: a material colour via 0x8019C7C8), not the texture-703
  branch. A TEX0 array is still set (GXSetArray 13 at 0x8017CA40), but no texture is bound.
- **The fill.** 12 bytes a pair. 0x80183F88 copies the centre first (0x80184040) and the rim second (+6). The last pair
  takes table index 0 (`li r0,0` at 0x80183FF8 when i >= count-1), the same as the first. So the disc is a closed
  20-sided shape, and pair 20's rim points straight down.
- **The z = 1 loop** (0x80183E84-0x80183F14) is as the doc says. The draw never writes the unused pairs' x,y or colours.
- **The submit.** 0x8017C9DC has one caller, 0x80183F64. 0x8019CFD4 does C_MTXOrtho, GXSetProjection (ortho), then
  PSMTXIdentity and GXLoadPosMtxImm(...,0) (0x8019D020-0x8019D064). The position matrix is the identity, so the
  6.6e-5 sum holds.
- **DOL floats:** near 1.0 (0x806233E8), top 0, bottom 480, left -80, right 720 (wide) or 640, far 30360. Colours
  E6E6FFFE twice (0x80622C1C, 0x80622C20). Place 320, 240, and z -1.0 (0x80622C24-0x80622C2C). So the used points do
  sit on the near plane.
- **The display list:** GX_QUADS (`li r3,128` at 0x8017C4D4), 2N-4 vertices, in the order 1,0 then k,k+1,k+1,k. The
  bad quad is (rim 20, centre 20, U0, U1).
- **Host:** the snap is at wgsl.cpp:346-347, reversed Z at 332-333, the default comment at raster.cpp:104, the patch
  line at eternal-darkness.patch:25717, and the quad split {0,1,2} {2,3,0} at render_sink.cpp:618-632.
  6.6e-5 < 1e-3, so the unused points are snapped back.
- **Pictures:** the wedge is in solo 000068.jpg and co-op 000063/000064.jpg. The co-op shape fits too. On a 480-pixel
  half, guest x 0 is pixel 48 and the bottom crossing (guest x 204) is pixel 170. Both are seen.
- **Logs:** shots.csv goes from 0.022 (000062) to 204.241 (000063). page-0001.log:47 (sound 536, frame 3059) and :70
  (ended, frame 3072). EVENTS.log line 12 is "script 1801 runs for player 2 (frame 4560)". env.txt has
  `ED_PAD2_SCRIPT=4502,4508,lup;4560,4566,b`.
- **The host hook works as planned.** hle_dispatch_entry covers any code address below 0x80240000. A notify runs
  before the recompiled body and may write guest memory (hle_dispatch, `entry->notify(cpu)`). mem_read8, mem_write16,
  guest_ram_range_valid and stayer_bl_is all exist. sanity_tool_install runs in every mode (after `find_coop(cpu);`),
  while fx_install runs only under `coop_stay()`.
- **Range and timing are safe.** The game itself writes z into all 64 pairs (up to pos+766), so writing x,y into the
  same pairs adds no new range. GXSetArray runs after the notify (0x8017CA18). gxcore reads arrays and lists from guest
  memory (the comment in initialize_hle_dispatch).

### Wrong, with corrections

1. **0x8006B2E8 and 0x801D68F4 are not makers.** They are `bl 0x80183818` sites (the template). 0x8006B2E8 sits inside
   0x8006B270, a second copy of the same lavender flash: the same colours (E6E6FFFE at 0x80620EF8/0x80620EFC), the same
   320, 240, -1, and a = 20 (`li r0,20` / `sth r0,8(r30)`, 0x8006B33C-0x8006B34C). It plays sound 536 too
   (0x8006B384). Its callers are 0x8006C970, 0x80113234, 0x8011344C, 0x801137EC and 0x80113ABC.
2. **The sound in the sanity log is not shown to be 0x80145760's.** "from 0x8019DBDC" is inside the sound call itself
   (0x8019DBD8 `bl 0x801AD078`). Sound 536 is played at 0x80145878, 0x8006B384, 0x800A8F18 and 0x800A8FCC. The solo
   sanity return is as likely to be 0x8006B270 (inferred). Both use the same draw, so the cause and the fix stand.
3. **The colour story is half right.** The wedge has one stray corner (U), not "far corners". Its other two corners are
   pair 20's centre and rim. The game sets their colours (0x80184068, for count pairs) and writes their alpha every
   frame (`stb r0,3(r26)` at 0x80184390). Only U's colour is stale. In 000071.jpg (the solo fade) the wedge is lighter
   by its vertical edge and fades toward the top-left. That looks like a second lavender layer over the disc (inferred).
   At full flash (000068.jpg) the wedge is darker than the disc by its vertical edge. Plain alpha blending of two
   lavender layers does not explain that, and the blend was not read. It does not change the fix: a triangle with no
   area draws nothing, whatever its colours.
4. **The list's N is not shown to be 0x805D7102.** The list builder 0x8017C430 takes its N from the record at word 16
   of 0x805D6314 (0x80021CB8-0x80021CC8; `lhz r28,2(r30)` at 0x8017C4C0), not from 0x805D7100. That the two are the
   same is inferred. The game's own 64-pair loop still says 64.
5. **hle.c line numbers are stale.** When I read: sanity_tool_step 27755, sanity_tool_install 27825, stayer_bl_is
   28735, fx_flash_draw 26640. fx_install (16133) and notify_GXLoadPosMtxImm (3431) were unchanged. Use the names.
6. **f6c1c16 is a commit of the main repo**, "Boot Eternal Darkness (PAL GEDP01) ...", which added the patch. It is
   not in .deps/GXRuntime. A minor point.
7. **Limits names the wrong blend code.** 0x801E21FC is the texture branch and does not run for this flash. Its
   material is set by 0x801E5840, 0x801E16D0(3) (0x801811A0-0x801811A8) and 0x8019C7C8 (0x801811CC).
8. **"Every run picture" means two runs.** Only two runs were looked at.

### Missed

- If the stale U0 and U1 differ (data that is not 0,0), the quad's second half {U0, U1, rim} is a second stray
  triangle. The fold covers that too, because both go to the last pair.
- The stale points and colours are the effect's own buffers (effect+80/+84/+88 via 0x80181834). These come from a
  shared pool (0x80174D40 on 0x805D6314). A reused slot can hold another effect's leftovers, so the wedge could change
  shape from one flash to the next (inferred). The fix does not depend on the stale values.
- **A solo test needs no new code.** Use `ED_SANITY_TEST=17,...` with co-op off through `ED_SETTINGS`
  (docs/game/systems/sanity.md:18). Head of Shakespeare ended with the flash (page-0001.log:47, :70). That makes the
  "single player sanity effect" a workflow run, not only the user's check. sanity_tool_step returns early in co-op
  (`if (g_settings.mod_coop || ...) return;`), so it is solo only.
- **ED_FLASH_TEST timing.** hle_guest_call exists, but sanity_door_run's comment says a script run from the player's
  turn "breaks the game's threads". Make the 0x80145760 call at a pad read, as sanity_door_run does (inferred safe).
- **0x801D68F4's flash looks different.** Its colour comes from 0x801C7AF4, with the alphas lowered by 80 and 50, and
  +6 = 10. The fix touches it too. Its look was not checked.
- **The 120 FPS in-between frames were not checked.** There is no "interpolat" in runtime/host/*.c. Still inferred.

### What it changes in the plan

- **The fix does not change.** It stays a notify on 0x8017C9DC: lr 0x80183F68, count = byte r30+1, and pairs count..63
  fold onto pair count-1. It covers 0x80145760, 0x8006B270 and the 0x801D68F4 user alike.
- **Step 1:** also log one line at the entries of 0x80145760 and 0x8006B270. That shows which flash the phone and the
  sanity return really use.
- **Step 5:** add the solo `ED_SANITY_TEST` run. Make the ED_FLASH_TEST call from a pad read. Test a = 20 too (the
  0x8006B270 copy).
- **Read the blend** (0x8019C7C8, 0x801E5840) only if a darker patch is still there after the fold.
- **Name hle.c places by function**, not by line.

### Verdict

The cause holds. It is one stray triangle from the unused points, kept by the host's near-plane snap. The fix holds, and
its range and timing are safe. The doc gets some details wrong: the makers, the sound's source, the colour story, the
list's N and the line numbers. None of these changes the fix. The co-op problem (the flash in player 1's half when
player 2 answers) stands as its own co-op bug.

## Built (step F1, 2026-11-17)

- `flash_submit_notify`, `flash_install`, `flash_ed85_notify`, `flash_copy_notify`, `test_flash` in `runtime/host/hle.c`.
- The log confirmed the plan: count 21, n 128, the unused pairs at x, y = 0, 0 and z = 1, colours 0. The disc's radius is
  512 from its first draw (no growth seen in the log; the 5-tick growth above was not seen).
- The phone's flash is at the call's end (state 5 of L00/1801, frame 4882 in the slot 3 runs), not at the B press.
- Co-op: the flash goes to the half of the player who set it off (r13-21496 at 0x80145760's entry, as the subtitle's
  owner), moved as the subtitle line is. Half the widescreen HUD shift is put back, because the disc spans every HUD
  band and the renderer's choice to shift it can't be seen from here; its centre is then within 40 units either way.
- Seen in pictures: solo a = 0 and a = 20 (ED_FLASH_TEST), player 2's phone (one room and apart), player 1's phone,
  and a forced player 2 flash. Kept in `Screenshots for Claude/f1-sanity-flash/`.
- Not run: the split off, 21:9, the 0x801D68F4 user, a natural sanity effect's end (slot 2 is now chapter 1, where
  effect 17 needs a door), player 2's copy Tome.

## Built: review fixes (step F1, 2026-11-17)

- **The owner lasted only 2 frames.** A disc kept its player while its block was drawn again within 2 guest frames.
  The guest frame follows the wall clock, so a host stall (a first pipeline compile on a fresh cache, a load) made the
  next draw look like a new disc, with no owner: the rest of player 2's flash went to player 1's half. Now a block keeps
  its owner while it is drawn again within `FLASH_KEEP` (240) frames. A flash begun since takes a matched block only if
  the block's centre got brighter than at its last draw (a fade only darkens): a new flash in the same block.
  A test switch, `ED_FLASH_STALL=MS`, makes the host wait once after a flash's first draw.
  Seen: `logs/run-20261117-122919-flash-p2-stall` (400 ms): the next draw came 31 frames later, the log says
  "drawn again after 31 frames: still player 2's", and the fade stayed in player 2's half.
- **21:9 corners.** Half the HUD shift put back leaves the centre up to 102 units off at 21:9, more than the 20-gon's
  505 could cover. A moved disc's rim is now pushed out by half again (758 units). The split's view clips it to the half.
  Seen with a scratch settings file at aspect 4 (`logs/run-20261117-123004-flash-p2-219`, "moved 3993", so the 21:9 value
  was read): the whole of player 2's half lit, corners covered. The window was the user's 16:9 screen, so a real 21:9
  screen was not seen.
- Solo is not moved or grown; seen unchanged (`logs/run-20261117-123048-flash-solo-a0-recheck`).
- Pictures: `Screenshots for Claude/f1-sanity-flash/review-*`.

## Changed by step F2 (2026-11-17): the disc is centred
- The renderer now always moves a draw wholly past the screen's right edge out by the widescreen spare width
  (`widescreen_hud_shift`, gxcore_draw.cpp; docs/research/coop-gaps/subtitles.md "Built: step F2"). So `flash_away`
  no longer puts half the shift back: a moved disc's centre is its half's middle at 16:9 and 21:9. The grown rim stays.
- Seen: player 2's phone, "moved 4016" (was 4056), and at aspect 4 "moved 3891": player 2's half lit whole both times
  (`Screenshots for Claude/F2-subtitle-leftovers/` 08, 09).
