# Mods: feasibility report (split-screen co-op)

> **Superseded in part (2026-10-29):** the "two copies of the game" design below was
> turned down: co-op is to be one shared world (shared enemies, both players through a
> door together), with split-screen drawn by replaying each game frame from each
> player's camera. The code findings below still hold.

Base: upstream v0.9.6 (branch `mods`). Written 2026-10-29 from reading the code; nothing here is built yet.

## The one big finding

**The game can only run once per process.** The build compiles the game code against a single
built-in CPU (`dolrecomp_cpu`, `CMakeLists.txt` `DOLRECOMP_STATIC_CPU`), and the graphics, audio,
disc, interrupts and hardware models in `runtime/host/` are all global variables. Two games in one
program would mean rewriting most of the runtime.

**So co-op = two copies of the game running side by side**, one per player, each in its own
borderless window filling half the screen, plus a small link between them so each can see the
other. This gives several of the wanted features for free, but it also sets hard limits:

| Comes free | Hard limit |
|---|---|
| Independent pause: pausing one game never touches the other | **Enemies are not shared.** Each player fights their own copy of each room's enemies. Player 1 can't help kill player 2's zombie. |
| Independent health, magick, sanity, stamina | **Each player's world is separate.** Doors, items, puzzles and story progress happen in each game separately. |
| Each player walks through doors and loads rooms on their own | **Both players must be in the same chapter** to see each other, since the other player is a stand-in model. |
| Sanity effects stay private: they happen inside one game only | **Sound comes out of the same speakers**, so player 2 hears player 1's sanity sounds unless one of you wears headphones on a separate audio device. |

## Feature go / no-go

| Feature | Verdict | Notes |
|---|---|---|
| Mods menu (launcher and pause menu) | **Go, easy** | Menus are rows built in `runtime/host/frontend.cpp` (`build_launcher`, `build_in_game`, `build_options`); settings are saved through `settings.c` to `settings.ini`. Adding a `Screen::Mods` page with toggles follows the existing pattern. |
| Split-screen, vertical or horizontal | **Go, moderate** | A co-op launcher starts the second copy and gives each a half-screen borderless window (SDL already supports positioning, `aurora/lib/window.cpp`). Orientation just changes the window rectangles. |
| Player 1 / player 2 controllers | **Go, moderate** | Today each copy opens the first controller (`controls.cpp:139`, `frontend.cpp:72`). Add a "use controller N" setting, and let input reach a window without focus (SDL's background-input hint). |
| Separate saves per player | **Go, easy** | Each copy gets its own `saves/` folder and memory card file. |
| Independent pause, health, magick, sanity | **Go, free** | Each comes from running two separate games. |
| Music without clashing | **Go, easy** | Music is the disc's streamed track, which already has its own volume (`audio.c:208`, separate from effects). Mute music in player 2's copy; low-sanity music changes follow player 1. |
| Private sanity *sound* effects | **Partial** | Possible only with a separate audio output per player (for example, player 2 on headphones). Needs an "audio device" option per copy. |
| 2x damage to players (Enhanced) | **Go, easy** | Every health change goes through one game function. Upstream's newer debug menu already intercepts it for Invincible (on `main`, `hle.c` `hle_set_health`). Port that and double each loss. |
| Extra random enemies (Enhanced) | **Go, with a limit** | Upstream's newer Spawn enemy (on `main`, `hle.c` `spawn_requested`) uses the game's own area loader. It can only spawn **creature types the current area already has loaded**, so we can't put Mantorok zombies in a room that has none. Random picks from the area's own creature list work. |
| Seeing the other player | **Hard** | Each copy would place a stand-in character at the other player's position, using the spawn route above, with its movement driven over a local link. It only works if that character's model is loaded in this area, which mostly means both players play the same character. Animations are the hardest part. |
| Seeing each other's spells | **Hard, after stand-ins** | Visual only (the effect appears), because enemies aren't shared. |
| Ceiling and other insanity poses not shown to the other player | **Go, once stand-ins exist** | The stand-in only copies normal position and animation and holds still while an effect is running. The game's current insanity effect can already be read (on `main`, `hle.c` `player_insanity_effect`). |
| Character swapping, any character, with animations | **No-go for now** | Each chapter loads only its own character's model, skeleton and animations. Loading another chapter's character mid-game means reverse-engineering the asset loader. Possibly later, for characters whose assets are already loaded. |
| Per-character base health, magick, sanity | **Go, moderate** | The game's stat setter and maximums are already found (on `main`, `hle.c` Sanity / Infinite magick). Overriding the maximums per player is a small step from there. |

## Recommended order

1. **Mods menu + settings plumbing.** Toggles stored in `settings.ini`.
2. **Port upstream's gameplay hooks** (health, stats, spawn, insanity state) from `main` into the `mods` branch, without its broken title-menu change.
3. **Enhanced enemies:** 2x damage, then random extra spawns from the area's own creature list.
4. **Co-op launcher:** a second copy, half-screen windows, orientation, controller per player, separate saves, music muted in player 2's copy.
5. **Link between the copies:** share position and state, then stand-ins, then spells.
6. **Character swapping:** revisit last.

Steps 1-4 give you a playable "two games side by side" co-op. Step 5 is where it starts to feel like a shared world, and where most of the risk is.
