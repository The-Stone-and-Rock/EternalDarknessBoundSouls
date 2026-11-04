# Doors, and every room change

**One call for nearly all of it:** `fn66(room, entry marker hash, camera hash, transition, script after, flags[, 7th])`, at `0x801666A4`. 1,083 calls in the scripts. The only other script route is `ed29`, script 1023, transition 17.

`ed door ROOM`: every way in and out, with region, prompt, entry, camera and transition. `ed transition`: what each number does. Evidence: `docs/game/research/transitions.md`, and `transitions-watched.md` for what was seen in runs.

**For a normal player**, as in script 1097:

```
ed301(2, 4)            -- on walking in: CLIMB DOWN. 1 OPEN DOOR, 3 CLIMB UP, 10 SECRET PASSAGE
fn71(21)               -- queue the room
fn1(1)                 -- next word
-- B runs the script again, at word 1:
fn66(21, EntryMkr01, Camera02, 15, 0, 0)
```

**A prompted door never changes room on walking in:** over every script asking 1, 3, 4 or 10, no `fn66` is in the same word as its ask. Zone exits do: 265 region calls ask nothing, 204 of them transition 2, as `L06/334`, just `fn71(59); fn66(59, ...)`. Not every door is this simple: 200 of 265 OPEN DOOR scripts are. Some run a scene first. Some have no `fn66`: L05/66 plays a sound and says locked.

## What each transition does

The number packs a fade to black and a motion on the player. **A tick is one frame, 1/60 s (measured).**

| n | Motion | Fade | Ticks to the room change (runs took 2 or 3 frames more) | Used | What it is |
|---|---|---|---|---|---|
| 0 | none | none | 1 | 143 | A cut. Tome returns, mirrors. Not watched |
| 1 | none | 5 | 7 | 2 | Short fade. L04's elevator. Not watched |
| 2 | none | 10 | 12 | 422 | **Short fade, watched:** dips briefly, then just there. Most zone exits |
| 3 | none | 60 | 62 | 86 | Long fade. Not watched |
| 6 | 2 | 60 | 62 | 2 | L04 jump-overs. Not watched |
| **9** | **25** | **60** | **62** | **333** | **A door, watched.** Steps forward as it fades. Arrives on 25 for 106 frames, then motion 2 for 23. In control after about 2.1 s |
| **12** | **103** | **60** | **62** | **46** | **Climb up, watched.** Arrives on 104 for 60 frames, then motion 2 for 23. About 1.4 s |
| **15** | **104** | **60** | **62** | **49** | **Climb down, watched.** Arrives on 103 for 80 frames, then motion 2 for 23. About 1.7 s |
| 4, 5, 7, 8, 10, 11, 13, 14 | 2, 2, 25, 25, 103, 103, 104, 104 | 5 or 10 | 7 or 12 | 0 | Never used |
| 16 | none | 62, after 128 | 190 | 0 | Game code, to room 83 |
| 17 | none | set by caller | 1 | 0 | Game code. Loads the room, doesn't place the player |

**The prompt doesn't fix the transition:** OPEN DOOR also goes with 2, 3 and 0.

**Motions need a marker.** The motion plays toward the nearest marker with flag 4 (`GravityMkr`, at no distance limit). 85 rooms have none, and there it plays nothing.

## Arrival

From `0x80155830` at 0, then `0x80152614(room, entry, camera, script, flags)`:

1. The room loads. `0x80151FDC` ends the wait. A 10 tick fade in starts (the script after can replace it: 868 does).
2. The player is placed at the entry marker: `0x80155348(entry, 2)`. **A marker that isn't in the room skips placing and event 238.**
3. **Event 238 picks the motion, from what is playing right then.** 25 gives 25, 99 gives 99, 103 gives 104, 104 gives 103, anything else none. Facing the marker + pi. `ed196(n)` overrides it, in 3 scripts.
4. Player state 80 plays it, then state 81 plays motion 2, then state 1. **Seen in runs.**
5. Event 62 to the new room's nodes, the camera from the third argument, then the script after arrival.

**Script after arrival, by calls:** 0 none (326). 1161 sets `Camera01` (242), and 1162 to 1170 and 1061 are camera scripts too. 2004 (126) is long, levels 09 and 16, unread. 868 (97) is a fade, then a script by player model. 1828 (39) branches on `fn127`.

**A door script can do more first:** `L03/420` with flag 340 off sends event 82 to the kind 32 thing before its `fn66`, and arrives in a scene with no script after.

**Flags:** bit 1 arrives at saved place slot 0 (`fn104`), bit 2 is `0x80043BE8(0)`.

**The ticket, `0x8058AFFC`:** `+0` countdown, `+4` delay, `+8` room, `+12` script, `+16` flags, `+20` transition, `+24` 7th, `+28` entry, `+32` camera. **It stays after use**, so a value there doesn't mean a change is coming: test `+0`.

## Other room changes

`ed29` (`0x800E8C9C`) and 7 game code sites write a ticket, and `ed door` lists none of them: `0x800E8E14`, `0x8006BFAC`, `0x8009B760`, `0x800A2548`, `0x800ADA24`, `0x800C04EC`, `0x8012D1A0`. Teleports use plain `fn66`: the TELEPORT prompt is asked by nothing.

## For two players

`coop_follow` moves player 2 to player 1's room. **Whoever is current as the ticket is written takes the marker** (`g_coop_entered_p2`, at the ticket's notice). Zones, Tome items and non-exit prompts of player 2 run as them, so they take it. Exit prompts 1, 2, 3, 4, 10 (`coop_prompt_is_exit`) run as player 1 through `coop_use_start(script, false)`, so player 1 takes it even when player 2 pressed. `coop_door_motion` hooks `0x801669AC`: when player 2 opened it, the motion is theirs, and if they're still in state 121 on arrival `coop_follow` forces state 80 with that motion running on.

**Seen in runs: the other player stands still through the leaving, and arrives standing, 150 units from the opener where the floor allows, then 75, else on their spot (`coop_place`).** It's the arrival event 238 that plays the motion, and `coop_follow` sends none, so a follower gets no arrival motion. That's from player 1 opening, in today's runs. Player 2 opening is in older logs with no recording: `logs/run-20261102-0141-coop-slot6-door80`, `logs/t28-doors-unheld.log`. In slot 7's door, player 2 lands between the camera and player 1, hiding them. Whether that wants changing is the user's call.

**Already tried: sending player 2 event 238 by `hle_guest_call`, as themselves** (`2fa5936`). They stopped where they were, twisted, in mid motion, so `862e14d` set state 80 instead: `docs/coop-failed-attempts.md`. That try never read the event's answer, or sent event 191 after, as the game does: `hle_guest_call` returns `r3` and restores registers, while event 238's answer is in `r4` (`0x801526F0`).

**A zone a follower lands in must not send them back:** comment at `COOP_SCRIPT_GO_TO_ROOM`.

**Not worked out:** what motion 2 and 99 look like. Whether `fn79(handle, 135, 0)` can also change room. What rooms 1000 and above are for.
