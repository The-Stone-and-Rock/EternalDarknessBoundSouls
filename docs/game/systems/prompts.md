# Prompts, and how any verb starts

`ed prompts [TEXT]`: all 63, who asks each and what pressing runs. `ed verb [NAME]`: everything a player can do, including what has no prompt. Evidence: `docs/game/research/verbs.md`.

**Asked:** `0x80117404(button, id, script, zone script, priority, slot, flag)`. 29 sites ask, and 2 more are it moving slot 2 to slot 4. Scripts reach it through `ed301`, `ed219`, `ed220` and `ed221` only.

**Six slots at `0x80335A38`,** 28 bytes each: `+0` priority, `+4` button, `+8` id, `+12` script, `+16` skip, `+20` slot, `+24` flag. A seventh, `0x80335AE0`, is on show. Slots 0, 1, 3, 5 are cleared each frame, so what asks there asks each frame. Slots 2 and 4 are zones', kept until a press, leaving or an interface.

**Pressed:** A, Z, B and the stick call `0x80006EBC` (table `0x8023B9B8`), with `r3` 0, 1, 2. It runs only if `r3` is the shown prompt's button.

- **Every ask uses button 2 or 6. Button 6 has no binding:** BREAK FREE, PUSH OFF and some interface prompts are only shown.
- The stick acts only on ids 6, 7, 11, 13, 19, 26, 34, 58, and sets a script's word to 100.
- Then B calls `0x801C4F48`, the prompt clears, and by script: -2 the close-up, above 0 `0x801642D4(script, 0, 0)`.

**Almost every prompt means "run the script again":** the script asks at word 0, sets word 1, and word 1 does the thing. Scripts see their word through `fn0`, `fn1`. `fn34()` runs itself again at once. `fn36(frames)` runs it after a wait. `fn73(n)` runs n next.

**Words exist for prompts nothing asks for:** TELEPORT, READ_TOME, PUSH, LOCKED, NEXT and 23 others. Doors that won't open use words, and pushing uses EXAMINE and a yes/no.

## Regions start everything

A region is 116 bytes: `+108` shape, 0 sphere, 1 box, 2 plane. `ed room N` shows all of them.

| Event | Sphere | Box | When |
|---|---|---|---|
| Enter | `+58` | `+60` | Once |
| Leave | `+60` | `+62` | Once |
| Stay | `+62` | `+64` | Each scan. One on the disc |
| Enter, leave, stay for any other model | `+64`, `+66`, `+68` | `+66`, `+68`, `+70` | |

A plane runs `+100` or `+102` by which way it's crossed, and only `r13-26524` crosses them. **So in the game's own code, a second player reads the "other" fields, not the enter script, and crosses no plane.** The host has its own use of this scan.

## Buttons

Read by `0x8013E518`, into `0x805880A0`. Channel `r13-26224`: A, B, Z, stick to prompts. Start `0x80006CDC(1, 0)` the Tome. Y and D-pad `0x800069DC(1, slot)` quick spells.

Player's control, block `r13-25660`, flags `+8`, read by `0x80045BC4` each frame:

| Pad | Event | Goes to |
|---|---|---|
| A | 40 | `0x8005A2AC`, attack |
| B | 46 | Per state |
| Z | 207 | `0x800597B4`, reload |
| Stick | 44, else 45, unless R held | `0x8005A28C`, `0x8005A254` |
| R pressed, at 150, held | 41, 42, 43 | `0x8005A138`, `0x8005A17C`, `0x8005A1D0` |
| X, L | Flags `0x20`, `0x10` | Move mode, block `+16` |
