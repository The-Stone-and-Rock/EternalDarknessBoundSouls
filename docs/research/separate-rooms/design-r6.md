# R6 design (2026-11-11 night): enemies and hits by room

From the readers (`enemy-readers.md`, `enemy-readers-2.md`) and the baseline runs on today's build (HEAD 2886af7, no code
change, `saves/rooms-ch1-183.state`, ED_HURT_LOG on):

- r6-base2-c2 (player 1 stays with room 183's zombies, player 2 to 180): player 1 hit 125 -> 76; player 2 untouched. Right today.
- r6-base-c1 (the broken state, player 2 standing in 183 with player 1 in 180): zombie 40 sends `event 11 from 40 to 59`
  (player 2's id) 15 times from 4129; no 39, no 53, no damage: the hit is thrown away in the victim's hit taker.
- r6-base2-c3 (player 2 swings apart): their swings (state 6) send no hit on a zombie.
- r6-base2-c4 (together, both among the zombies): 40 goes after the nearer, player 1 (699 vs 823), and only player 1 is hit
  (7 each). The reference for the together check after the build.

## The rule

**Nothing changes while the players are together.** Every new path below starts with "the players are apart"
(`coop_stay()` and `coop_rooms_apart`, both players' object +592 differ) and, for the hit, "the rooms involved differ from
the room in play". Together, both players are in the room in play, so each new path is skipped at its first test.

Apart, an enemy sees, goes after and hurts only a player in its own room (object +592), and a player's swing, shot or spell
hurts only what is in their own room.

## D1. The hit's room (the core)

The game takes every hit at three victim-side functions, each asking "is the victim's room the room in play" (r13-23276):

- `0x8006255C(block)`: the hit taker of event 11 (swings, shots, spells' 237, the finisher's 48, area hits): victim id at
  block +152 (`0x8019B72C`), attacker id at +156 (`0x8019B734`); gate at `0x800625A4`. It sends 53 (`0x8006367C`) and 39
  (`0x80063764`) to the victim synchronously (`0x801F59B0`), inside the call.
- `0x80061B9C(node, message, answer)`: event 39's damage taker; gate at `0x80061BD0`. Sends 8 (death) synchronously.
- `0x800637B8(node, message, answer)`: event 53's death; gate at `0x8006383C`.

Three intercepts, one host function. On entry:
1. Pass through unless mod_coop, `coop_stay()`, the players apart, no R4 bracket (`g_stay_bracket == 0`), and the sda.
2. Victim node: r3 (39, 53) or the node of block +152 (11). Victim room V = its object +592; attacker node from block +156
   (11) or message +4 (39, 53); attacker room A (-1 when no node or no object).
3. The room the call should see: T = V when A == V or A == -1; otherwise T = A (a cross-room hit: the game's own gate then
   refuses it, because V != T).
4. T == r13-23276: pass through (the game's own answer; this is every hit of the room in play with both sides in it).
5. T must be resident (`room_slot_of`) with its record loaded (+0x8140 == T, +0x8142 and +0x8143 non-zero, as `0x80123F70`
   tests); otherwise pass through.
6. Bracket: r13-23276 := T and the slot table's +0/+4 := T's slot (the turn's `coop_stay_room_in`, made for a given room),
   `hle_guest_call` the same function, put the three words back, return its r3. The nested call sees T == the word and
   passes through at step 4, so no re-entry flag is needed; nested hits on another room's node bracket by the same rule.

What this gives, apart:
- An enemy of player 2's room hits player 2: 11 -> player 2's handler (coop_events, their words) -> 0x8006255C bracketed to
  their room -> 53 and 39 nested -> 0x80061B9C passes -> health; at 0 the death 0x800637B8 passes.
- Player 2 hits an enemy of their room: the same, with the enemy as victim; its death (event 8, synchronous, 0x80038ED4's
  own room test) runs inside the bracket, so the zombie dies and falls (not made inert: the readers' hazard).
- A stale cross-room hit (an enemy of B whose quarry +152 is player 1 in A by coordinates, up to 63 frames after they part):
  V = A (player 1), attacker room B, so T = B and the game's own gate refuses it (V != T). Same for player 2's swing at a
  stale target of the other room.
- Together: V == the word, A == V: step 4, pass through. Unchanged.

Why the slot table too: the turn's bracket swaps it and is proven; nested effects (blood, decals: 0x80172DC0 family on the
current section) then land on the victim's room. The readers found no current-slot read inside the three functions
themselves, so swapping it cannot make them worse than the turn does.

## D2. Sight's sanity hit across rooms (event 58): DROPPED after the refuters (refute-r6.md)

The 58 carries a hit block the receiver frees (0x80059C30-38); answering 0 would leak it, and D1 already refuses the 39
the player's 58 case forwards (attacker room B, victim room A: T = B). Left: a GRABBED player's 58 is taken with no gate
(type 19, 0x80060188). The original text, for the record:

The game's 58 (`0x800CA4E4`, posted with a delay by `0x801F57C0` at `0x800CA6A8`, to r13-21496) has no room test: an idle
enemy of room B can "see" player 1 at room A's coordinates through B's collision. Both players' nodes run the player
behaviour `0x80058DD8`, intercepted by `coop_events`. At its entry: event 58, the players apart, the sender's node found
with a room != -1 and != this node's room -> answer 0, the handler not run (`[enemy] N (room R) can't see player K (room
S)`). Together: never (same room).

## D3. coop_sighting (widened after the first checks)

Together, unchanged: the enemy's look (0x8003B1C8) runs as player 2 when stats+20 is player 2's id. Apart (built after the
first R6 checks, r6-c1/r6-c3: room 183's zombies stood idle, never after player 2): a zombie takes only "the player" as a
candidate (its flag 0x1000, `rlwinm r0,r17,0,19,19` at 0x8003283C, then `cmpw r18,r28` at 0x8003294C), so with player 1 in
another room it found nobody. Apart, the look runs as player 2 for an enemy of player 2's room that is after player 2 or
after nobody: it then finds them by its own cone, distance and line of sight, as it finds player 1. An enemy of any other
room looks as the game's.

## D1-D5 as built: the refuters' fixes

Every new path opens with `r6_apart` (mod_coop, ED_COOP_STAY, no R4 bracket, both players' rooms known, different and
resident); the hit takers are hooked only under ED_COOP_STAY and stand aside in the Tome copy's turn; the hook ends as
coop_as_player2 does (r3 from the guest call, passthrough off: else the game's function ran a second time and wrote 0);
the room word and slot table go through `coop_room_in` (coop_stay_room_in's body for any room; coop_stay_room_in itself
unchanged); +0x8140 read as a halfword; a victim with no room passes; 4 nested hits at most. 0x800637B8 is event 53's hurt
reaction (not the death: that is event 8, inside 0x80061B9C). D4 and the roster act on the two resident rooms only (the
first roster showed live nodes of rooms 342-344 left from the chapter's start).

## D4. coop_enemy_target apart

Today it returns unless all three rooms are equal: kept exactly for the together case (distance by 60 units, the held rule,
coop_seen_later, the health log, and the +12 write: see "Left as is"). New, only when apart: the enemy's +20 is a player
whose room != the enemy's room -> +20 := 0 (`[enemy] N (room R) dropped player K (room S)`); the enemy's +20 is the player in
its room and that player is held (coop_player_held: their words, close-up, Tome) -> +20 := 0 (`... passed over player K:
held`). The game then re-chooses at its next quarry (every 8-64 frames), from its own room only, and a held player answers
59 'no' (coop_send_event) so isn't chosen again while held. 0 is the game's own "nobody" (the quarry's step 3 writes it).
The health log (`[coop] N's health a -> b`) runs apart too.

## D5. Event 59 across rooms

The quarry's `other != 0` path (event 101 and 14 other callers) has no room test; all its paths ask event 59 first. In
coop_send_event: event 59 to a player, the players apart, the sender's node found with a room != -1 and != that player's
room -> answer 0. Senders with no node or no room (hazards' boxes, damage fields) pass as today.

## D6. Logs (ED_COOP_ENEMY_LOG, frame-stamped)

- `[enemy] hit on V (room r) by A (room r): the call sees room T` / `... refused: rooms differ` at each bracketed hit.
- `[enemy] N (room R) dropped player K (room S)`, `... passed over player K: held`, `... can't see player K (room S)`.
- Every 120 frames while apart: `[enemy] frame F: N room R at x y z state S after T health H` for each character with
  stats that isn't a player.

## Left as is (each a later step, written in the todo)

- The +12 write in coop_enemy_target (the readers: +12 is the quarry's flags word, bit 0 locked, not a second id). It is
  part of the together behaviour the user confirmed; changing it changes that, so not in R6.
- Player 2's FINISH HIM apart (the finisher's look 0x800330E0 keeps bodies of the room in play only; it runs in coop_events
  without their room word).
- A downed zombie of player 2's room doesn't get up while apart (state 9's event 24 waits on the room in play, 0x8003D97C);
  it gets up when the players are together again.
- The spawner (0x8002FC94) of player 2's room waits while apart.
- Enemy sounds of player 2's room are tagged with player 1's room (R5's per-room gain treats them as player 1's).
- Event 61 at a door reaches the enemies of the room left even when the other player stays (state 33 with 0x8003A1D0 == 4
  goes inert). Not seen in the runs: room 183's zombies kept swinging at player 2 after player 1 left.
- The character-vs-character collision of other-room nodes (0x801308F8 filters by the room in play).
- The grab (kind 6 -> type 18/19): its bites have no room test; with D4 a grab can only start on a player of the enemy's room.

## Checks (one build, then the user plays)

On `saves/rooms-ch1-183.state` (host-only change; a fresh JTG state only when the machine is idle, its `hold` 0x0 checked):
- c1: player 1 to 180; player 2 stands still at 1305,1508 in 183: the zombies hit player 2 (p2hp drops, `[enemy] hit on 59
  ... sees room 183`); player 1's health never moves.
- c2: as base2-c2: player 1 hit in 183, player 2 untouched in 180 (unchanged).
- c3: player 1 to 180; player 2 waits for the zombies, then R + A: a zombie's health drops (`[coop] N's health`), a killed
  one falls (state 8, not inert).
- c4: together, as base2-c4: the same "goes after" by distance and hits on player 1 only.
- A same-room swing of player 2's (together) still lands (unchanged path).
