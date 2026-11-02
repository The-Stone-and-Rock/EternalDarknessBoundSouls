# Co-op: enemy sight and sanity

**The rule, for every enemy that sees and attacks this way:** each player takes
an enemy's sanity hit once, when they are first in its view, whichever player it
saw first. The enemy still goes after the closer player, and its attacks still
do their health damage to either player. Neither is part of this rule.

## How it works

The game's sight check (`0x8003B1C8`) tells only one player, then marks the enemy
as having seen (`0x80033874`, bit 8), so it never tells anyone again. That mark
belongs to the enemy, not to a player.

In `runtime/host/hle.c`:

- `coop_seen_shared` handles the sight event (58). It hits the other player if
  they are in view too, and notes in `g_coop_seen` who has been hit.
- `coop_seen_later` runs on each enemy's step and hits a player not yet hit, once,
  when they come into view.
- `coop_in_view` is the view test: 60 degrees either side of its facing, within
  1500 units, nothing between.
- `coop_seen_hit` sends the hit (event 39).

## If a player loses no sanity again

Run with `ED_COOP_ENEMY_LOG=1` and look for these lines:

| In the log | Means | Look at |
|---|---|---|
| `N saw player X` never appears | The enemy never told anyone, so it may not use this sight check (a scripted scare, a boss) | Find what that enemy does instead |
| `N saw player X` but never `seen already; player Y comes into its view` | The other player never passed the view test, or the save was loaded after the sighting | `coop_in_view`, `COOP_VIEW_HALF`, `COOP_VIEW_REACH` |
| `seen already` appears and sanity does not change | The hit was sent but not taken | `coop_seen_hit` and event 39's handling |

Health, sanity and magick are logged by the same setting where players are listed.

## Limits

- Who has been hit is not kept in save states. Load a save made after an enemy saw
  someone, and that enemy gives no late hit.
- The test can flicker for a frame or so on a player in plain sight, which at most
  delays the hit a moment.
- Event 39 is also what attacks arrive as: an event 39 with no sanity change is
  not a failure in itself.
