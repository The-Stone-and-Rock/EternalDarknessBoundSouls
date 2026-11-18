# Player 2's sanity RETURN in stay mode: the spec (read from the disassembly 2026-11-18, nothing run)

For the effects the game starts at a door and ends with "it wasn't real". Read with `plan.md` beside it.

Corrections to plan.md:
- **The body falling apart is 11 "Blood Bath"** (start table 0x80243D8C[11] = 0x80086A4C, step 0x80086BA0), not 17.
  17 "Head of Shakespeare" is the head that falls, is picked up and recites.
- **0x801E4610 is VISetBlack** (0x8020C648, flag r13-21880). Walk on Ceiling is a SECOND ROOM, its number + 1000.
- 0x8006B45C calls 0x8006BF00, which calls 0x8006B7B8.

## 1. The return, step by step

| Step | Address | Runs in | Reads / writes |
|---|---|---|---|
| 81 sent | a step's 0x801F57C0 (for later) to info+140 -> +68 | their update; kept by `sanity_later` | their link |
| 81 handled | 0x8006C850 -> 0x8006CC08 / 0x8006CC98 | their update | event 229 to holder+56; 0x80199EA0(0) at 0x8006CCC8 (play hold, busy 2); 0x80117260(256,1); registers the flash 0x8006B270 and 0x8006B438 (10 ticks on) |
| Flash, then the ticket | 0x8006B438 -> 0x8006BFAC -> 0x80155770(data+72, countdown 1, transition 17, callback 0x8006B45C, arg holder, +24 = 1) | their update (stepper 0x8006BAB0) | busy 4 (0x801557EC), r13-23392, input lock 0x800443E4; `notify_room_ticket` sets `g_coop_room_p2` |
| The tick | 0x80155830, called at 0x800083EC (lr 0x800083F0) and 0x80050A90 | the frame, outside any turn: player 1 current, player 1's set | the ticket 0x8058AFFC |
| The load | 0x80152298 at 0x801558A8 (lr 0x801558AC) | the frame | 0x80154D00-5C moves **r13-26524's object** to the room (ticket +24 != 0); r13-23276; the slots |
| Event 62 | 0x801558D0 | the frame, queued | reaches whichever set is in |
| The callback | bctrl 0x80155914 -> 0x8006B45C -> 0x8006BF00 | the frame | 0x801EF494(0x803196C8) cameras; 0x8006B7B8(holder,0): place data+116, angle +128, parts 0x801258BC x4, items +156/+160/+164, four stats 0x800354F8; 0x8006C5A8 (0x80319578's nodes back to room data+76, event 57 to spawns); 0x80199EA0(1); 0x801F57C0(188 -> tracker) |

Without a bracket the load and the callback act on player 1 with player 1's set (place and stats zero).

## 2. The bracket

- **Mark the ticket:** in `notify_room_ticket`, `lr == 0x8006BFF8 && g_sanity_p2_in` sets a new `g_sanity_p2_return`.
  `g_coop_room_p2` is already true then (it routes the fade's mover, the split's mover, no slot swap, the held ask) but
  opens no bracket: `notify_ticket_run` hangs only on the runner 0x80152614.
- **Open:** at 0x80152298's entry (`notify_room_change`, its first line) when `lr == 0x801558AC && g_coop_room_p2 &&
  g_sanity_p2_return`. Reuse `notify_ticket_run`'s body (from its `g_stay_bracket = 2` on) as a helper: it reads only
  gpr[3], the room, the same register here. Do NOT call `tome_trip_runner` (it reads the runner's arguments).
- **Close:** `coop_stay_change_end` as it is: the behaviour pass (lr 0x80008400) follows the callback in the same frame.
- **Also inside the bracket:** player 2's controller state (`coop_state`), or the callback's 0x80199EA0(1) frees player 1
  and player 2 stays held; `g_sanity_p2_busy` put into r13-23344 at the open, and at the close only the bits still set
  kept and taken out of the word.
- **Guards in `sanity_p2_step`:** bit 4 never goes into `g_sanity_p2_busy`; no update while a ticket is pending (or
  `split_ticket_ask` refuses their return ticket); skip the "came into another room: send 82" once after a return.

## 3. Missing from `SanityOwn`

| Address | Size | What | Decision |
|---|---|---|---|
| 0x80319578 | 128 | up to 32 node pointers a door effect moved to room 10000 | add (the nodes themselves are shared room state) |
| 0x803196C8 | at most 2008 (next reference 0x80319EA0); exact size not checked | the camera stack kept by 0x801EF2EC, restored only if the kept room == r13-23276 | add |
| r13-25380, -25384 | 4 each (floats) | 32 / 33's scale and step | add to `kSanityWords` |
| 0x80319608, r13-25508, -25512 | | read only, no writer found | skip |
| 0x803195F8 | 16 | save staging | skip |

## 4. Per effect

| # | Start / step | Acts on | Notes |
|---|---|---|---|
| 32, 33 | 0x8009D1D0 / 0x8009D2F8 | holder+56 only (0x800BF934(id,15)) | no spawn |
| 35 | 0x8009E308 / 0x8009E430 | holder+56's position (0x801240B8) | no spawn |
| 29 | 0x8009C250 / 0x8009C398, 0x8009C42C | overlay r13-25492 := 0x8009C534; the play hold only | its overlay may be a whole-screen quad (not checked) |
| 9 | 0x8009C5D4 / 0x8009E778 | up to 16 fake items (0x8006C008 -> 0x8006D9B4, flag 0x4000), stamped with the new room | 81 sent at once (0x8009E83C) |
| 16, 18 | 0x8009D0A8, 0x8009AC40 / 0x8009AD68, 0x8009C160 | input channels; 5 enemies spawned in the room | both players meet the enemies |
| 19 | 0x8009AE28 / 0x8009AF4C, 0x8009D4DC | as 18, the enemies scaled | |
| 11 | 0x80086A4C / 0x80086BA0 | body parts made by 0x801F9FD0(obj, part, vel, 0x2000), stamped r13-23276; held items dropped to data+156/160/164; ends with **event 8 (death)** to holder+56 | the body falling apart |
| 23 | 0x8009BAB4 / 0x8009BC10, 0x8009BC84 | body swap 0x800A2254 on the current player; event 8 after 900 ticks | keep off |
| 17 | 0x8009A7AC / 0x8009A8D4 (500 ticks), 0x8009AAC4 | the head made by 0x801F9FD0(obj, 0, vel, 0x24000) -> its id at data+148, pickable (flag 0x4000); event 53 (animation 11) to the character | ends at record+8 == 10 (81 for later, 0x8009AC18) |
| 22 | 0x8009B520 / 0x8009B760, 0x8009B640 | below | |

- **A death's 81 (11, 23):** 0x800C04EC sends 81 at once at 0x800C05D4, from the death code (called at 0x8005876C),
  probably in their turn where their link is 0 (not traced): intercept and queue it in `g_sanity_p2_later`.
- **Picking up the head:** 0x800C362C(the picker's node). If the thing has flag 0x4000 and 0x8006A32C(holder, 17) is
  true: data+52 = 2, words (0x801DB374 text 12, 0x80024A14), event 91 to the picker's link, then event 71 to the head.
  Event 91 (0x8006D078) plays the recital: the stream 0x801A1750(2,0,0,0) with subtitles 0x80008D0C (one set of
  speakers). As read, the pickup runs with player 1's set in memory, so the head would be taken with no recital: bracket
  0x800C362C with their set and link when r3 == `g_coop_p2`, and refuse player 1 on the id at their data+148. The head
  is not in the list 0x8006C5A8 clears: delete it at the return if player 1 keeps that room resident (not checked).
- **22 Walk on Ceiling:** 0x8009B760 writes 0x80155770(data+76 + 1000, entry data+88, camera data+92, countdown 3,
  transition 0) at 0x8009B808: this goes through the RUNNER, so the existing bracket takes it. Then VISetBlack(1) (both
  halves; how the host shows that per half is not checked) and the play hold. 0x8009B640 un-blacks at its first call;
  after 50 calls of 30 ticks it sends 81 and the transition-17 return goes to data+72. Outside its ten rooms it ends
  itself (0x8009B844). Rooms >= 1000 in the two-slot engine: not checked. None of the ten rooms is in level 12 (doors:
  level 7 109 -> 105, level 3 152 -> 151, levels 5, 6, 9, 11): test it in another chapter.

## 5. Order and tests

Room 51 and every other room out of 100 has no effect list, so 0x80043380 refuses all of these there: use the forced
path (`sanity_door_rules`, `ED_SANITY_P2_TEST="F,N"`, then `ED_COOP_P2_PUT` into the box region of room 100). The
chapter rules allow all of them in chapter 12 (9 is refused only in chapters 1, 3, 4).

1. 32, then 33, 35: needs section 2 whole, plus 0x80319578, 0x803196C8 and the two floats in the set. Pass: player 2
   back in room 100 at the kept place, player 1 untouched, no stuck hold.
2. 29: needs its overlay seen in their half.
3. 9: needs the pickup bracket.
4. 17: needs the pickup bracket and the head's cleanup.
5. 11: needs the death's 81 queued, and player 2's fake death kept from the co-op death code (not checked).
6. 16, 18, 19: shared enemies: the user's decision.
7. 22: another chapter (above).
8. 23: off.
