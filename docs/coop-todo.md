# Co-op: to do

## Independent sanity effects (requested)

Each player should have their own sanity effects, each tied to that player's own
sanity level. When the game would normally call for one (blood running down the
wall, for example), each player gets their own random pick, so the two can
experience different ones at the same moment.

Seen in co-op as it stands: the blood effect fires often around Brother Andrew's
room, and a capture showed a red streak in player 2's half and not player 1's (one
frame, not confirmed to be the effect).

## Open issues

- The world behind the Tome (in testing, not committed): each player's Tome and quick spells leave the world running.
  - Both the world and the Tome run at 60 while a Tome is open, measured in the save in slot 7. `TW_WORLD_FRAMES` in `runtime/host/hle.c` is the balance if a heavier room falls short.
  - The Tome's yellow cursor border pulses twice too fast in co-op (175 ms a cycle, 342 in solo). Its cause is not found yet.
  - Both players' Tomes open together: the game runs at about 80% speed. The state swapped is in `runtime/host/tome_state.inc`.
  - The other player's bars and prompts are hidden while a Tome is open.
  - Untested: a door, cutscene, death or area change while a Tome is open; quick saving with a Tome open; save or load from the Tome; a Tome page other than the inventory.
- Player 2 cannot cast in room `0x72` (needs a run with `ED_COOP_CAST_LOG=1`).
- Both players held for about 30 seconds at the bottom of the stairs (room `0x67`),
  seen once, just after "the game's collision test ran on".
- `OSCheckActiveThreads` crash seconds after loading a save: seen once, cause unknown.
- Barrels in room `0x72` set aside: the co-op run may just have missed the right one.
