# Eternal Darkness: Bound Souls

[![licence](https://img.shields.io/badge/licence-GPL--3.0-blue)](LICENSE)
![players](https://img.shields.io/badge/players-1%20or%202-8e1b12)
![status](https://img.shields.io/badge/co--op-experimental-orange)

**Eternal Darkness: Sanity's Requiem** (GameCube, 2002), rebuilt as a native PC game, and played **together**.

*Bound Souls* is a split-screen co-op fork of
**[Eternal Darkness Recomp](https://github.com/griches/eternaldarknessrecomp) by griches**, the native
recompilation of the game this project is built on. Everything that makes the game run on a modern PC is his work;
Bound Souls adds a second player, a cast of 34 playable bodies and a set of mods on top.

> **You need your own European disc (`GEDP01`)** as an `.iso` or `.gcm`. Nothing from the game is in this
> repository. The USA disc (`GEDE01`) is not supported.

---

## The dream

Eternal Darkness was always a story about many souls bound to one fate: twelve people across two thousand years,
each pulled into the same war against the Ancients. **Bound Souls asks: what if two of them faced it side by side?**

The goal is a real co-op game, not two players sharing one character's camera. Two players in the same world, each
with their **own half of the screen, their own Tome, their own spells, weapons, sanity and health**, walking
through the same rooms, fighting the same horrors, and losing their minds in their own ways. Any of the game's
characters can stand beside any other, in any chapter: Pius and Alex in the Roivas mansion, Karim and Chandra in the
Forbidden City, Edward's spirit guiding his granddaughter in person.

Nothing is rebuilt from scratch. Co-op works by letting the game's own code run for each player in turn, so every
examine, pickup, puzzle, door and scene is the game's own, just played by two.

**Legend:** 🧪 = *Experimental, expect glitches.* These are the same rows the menus tag
"(Experimental, expect glitches)". Co-op is new, and the game was only ever meant for one player: please report
what breaks.

## At a glance

| | |
| --- | --- |
| **Players** | 1, or 2 on one screen (side by side, or above and below) |
| **Playable characters** | **12**: Pius, Ellia, Anthony, Karim, Max, Lindsey, Paul, Roberto, Peter, Edward, Michael and Alex |
| **Other forms** | **22** more: spirits, corpses, the Liche, Chandra and her forms, the Bishop, the Custodian and more (**34 in all**) |
| **Tomes** | One for each player, open at the same time |
| **Save slots** | 100 save states, besides the game's own memory-card saves |
| **Sanity effects** | All 35 of the game's, on demand or at doors |

---

## Features

### Two-player co-op 🧪

*Launcher > Co-op, or Mods > Co-op.*

- 🧪 **Split-screen co-op**: a second player joins on the same screen, side by side or above and below, each
  with their own view of the world. Co-op plays at 60 frames a second.
- 🧪 **Player 2's own controls**: their own controller or keyboard, controller type, layout and button mapping.
- 🧪 **Each player's own camera**: the game's Original fixed cameras, Third person or First person, chosen for
  each player.
- 🧪 **Two Tomes at once**: each player opens their own Tome in their own half while the world keeps going for
  the other.
- 🧪 **Each player's own magick**: both cast at the same time, each with their own quick spells, spells they've
  made, magick bar and enchanting.
- 🧪 **Player 2's own health, sanity, weapons and ammo**, with their bars on their side of the screen, and their
  own reloads.
- 🧪 **Player 2 can do everything**: examine, pick up and use items, ring bells, talk, pull levers and solve
  puzzles, with their own prompts and close-ups in their half.
- 🧪 **Nobody waits**: when one player picks something up, reads or examines, the other keeps playing. A find
  shows only to whoever made it; the other gets a notice.
- 🧪 **Doors and scenes belong to whoever triggers them**, and the other player follows along.
- 🧪 **Creatures hunt both players**, and each player loses sanity as a horror comes into their own view.
- 🧪 **The opening nightmare in co-op**: the same opening shots for each player, a shotgun for both, and no Tome
  for either until the nightmare ends.
- 🧪 **Co-op saves**: player 2 is kept as their own character with what they carried.

### Characters 🧪

*Launcher > Choose Character, or in a game, Mods > Characters.*

- 🧪 **Any character for either player, in any chapter.** The game's own words use the name of the character
  you chose, and you can keep their stats or take the chapter's. A change mid-game reloads right where you stand.
- 🧪 **22 other forms from the story**, each with its own frame and portrait: Pius the **Liche**; Ellia's
  **spirit** and **corpse**; Anthony's **spirit**, **cursed** and **corpse** forms; Karim's **spirit**;
  **Chandra**, her **spirit** and **Mutilated Chandra**; Max's, Paul's, Roberto's and Edward's **spirits**; **the
  Bishop**, **the Custodian** and the **Black Robes**; and **the Maid**, her **spirit**, **the Servant**, **the
  Footman** and **Inspector Legrasse**. Ghost forms trail blue sparkles.
- 🧪 **Any weapon, its own moves**: every character uses any weapon with its original owner's attacks, reload and
  finishing move, and each player gets their own finisher close-up.
- 🧪 **A character select screen**: every character and form in a scrolling gallery with portraits, names, their
  story and stats, and a card for each player.

### Mods

*Launcher > Mods, or Mods in the pause menu.*

- **Sanity Effects**: pick any of the game's 35 sanity effects and play it on your save, or have random effects
  strike at doors (single player).
- 🧪 **Challenge: Enhanced sanity effects**: once sanity drops below 95%, any door can bring an effect, more often
  as it falls. *Off* turns the game's random effects off.
- 🧪 **Cheats**: *All magick* (every rune, circle and spell) and *Full stats* (health, sanity and magick kept full,
  for each player).
- **Skip intro**: Main menu goes straight to the title menu, Jump to Game straight into the chapter.
- **Title video**: your own video behind the title menu, with the game's music and words on top.

### Graphics 🧪

*Options > Graphics. All new in Bound Souls, on top of what the original recomp already offers.*

- 🧪 **Character shadows from each room's own lamps**, easing between lights, *Light* or *Dark*, *Soft* or *Hard*,
  plus contact shadows where characters meet the floor and walls (**Shadows**).
- 🧪 **Per-pixel lighting, skin scattering and full-size ambient occlusion** (**Lighting**).
- 🧪 **Haze around lamps and spells, light shafts, and spells that glow** (**Effects**).
- 🧪 **SMAA, 4x MSAA or both, and sharpening** (**Image**).
- 🧪 **Scenes fill the whole screen**, with no black bars, solo and co-op.

### Menus and comfort

- **A redesigned launcher and menus** on the Tome's parchment, with switches, sliders and arrows, Graphics split
  into short pages, and every menu remembering where you were.
- 🧪 **100 save state slots**, ten to a page, with Load, Duplicate and Delete. F1 to F10 save, F11 loads the newest.
  The cursor starts on the save you made last.
- **Skip any cutscene with Start**, even the first time you see it.
- **No forced Tome help pages**: the Health, Sanity and Magick help no longer pops the Tome open and pauses play.
- **Switch button prompts in Nintendo's colours** with a Switch controller.

---

## What's next

The road to a game that feels like it was always made for two:

- **Each player's own sanity effects**: when the game calls for one, each player gets their own, tied to their own
  sanity, so the two can see different horrors at the same moment.
- **Shared discoveries**: whoever finds the last piece of a spell gets the Tome's discovery page, and the other gets
  the spell with a notice like *"Pius found Enchant Spell"*. Runes and pages reach both players.
- **Player 2's own menu** for items and spells, alongside their Tome.
- **Using an item without holding the other player** (the Bronze Necklace still pauses player 2).
- **Chapters that cross into each other**: rooms that lead into another chapter, for sanity effects and a
  **randomizer**.
- **Spawn an enemy** from the Cheats page.
- **The character select, alive**: a turning model of each character on hover, and **Charlemagne** as a playable
  form.
- **Effects that face each player's own camera** (torch flames and sparkles face player 1's for now).
- **True 120 frames a second in co-op** (60 for now; 120 already works solo).
- **Every chapter checked for both players**: every examine, pickup and scene, chapter by chapter.

Ideas and bug reports are welcome in the Issues.

---

## Getting started

Packaged downloads for Bound Souls aren't up yet. Until they are, build it from source (below). The original
recomp's [releases](https://github.com/griches/eternaldarknessrecomp/releases) are single player only.

### HD textures (recommended)

| Download | What it is | Size |
| --- | --- | --- |
| **[Eternal Darkness HD Texture Pack](https://drive.google.com/file/d/1KGEr7dF2onTdzUFNp7Lia3p0D0I2-TaZ/view)** by Zombie | The game's textures redrawn at four times the detail | 2.9&nbsp;GB&nbsp;`.7z` |
| **[PAL supplement](https://drive.google.com/file/d/1BuoE0uL4K3xn5KfH8KYYtC6fk-6QiiiA/view)** by Eternal Darkness Recomp | What the pack misses on the European disc | 13.2&nbsp;MB&nbsp;`.zip` |

Unpack both and put both folders in a folder called `textures` next to `res` in the game's folder:

```
textures/
├── GED DDS 1.0/
└── Eternal Darkness HD Texture Pack PAL supplement/
```

Leave both as they are: the supplement replaces some of the pack's textures on purpose. **Options > Graphics >
Image > HD textures** says how many it found. The pack is the work of **Zombie**, shared on the
[Dolphin forums](https://forums.dolphin-emu.org/Thread-eternal-darkness-hd).

### Playing

- The launcher opens on **Main menu**. **Continue** loads your newest save state, **Load game** any of them, and
  **Choose Character** and **Co-op** set up the players.
- For co-op, plug in two controllers (or one and the keyboard), turn on **Split-screen co-op**, and pick each
  player's character.
- In the game, the View/Back button or **Esc** opens the pause menu: save states, Options, Mods, and the way back
  to the title or the launcher.
- If something goes wrong, the game says so on screen and writes a report to `saves/reports/`. Please attach it
  to an issue with a few words on what you were doing.

## Controls

A controller works as it is, and the keyboard too. *Options > Controls* switches layout (Modern, Original or
Custom), sets hold or toggle, deadzone and vibration, and remaps any action. Player 2 has their own under
*Co-op > Player 2 controls*.

| Action | Modern (default) | Original (GameCube positions) | Keyboard |
| --- | --- | --- | --- |
| Move | <img src="res/ui/glyphs/ink/xbox/stick_l.png" width="30" alt="Left stick"> | <img src="res/ui/glyphs/ink/xbox/stick_l.png" width="30" alt="Left stick"> | <img src="res/ui/glyphs/ink/kb/w.png" width="30" alt="W"><img src="res/ui/glyphs/ink/kb/a.png" width="30" alt="A"><img src="res/ui/glyphs/ink/kb/s.png" width="30" alt="S"><img src="res/ui/glyphs/ink/kb/d.png" width="30" alt="D"> |
| Attack / Confirm | <img src="res/ui/glyphs/ink/xbox/rt.png" width="30" alt="RT"> or <img src="res/ui/glyphs/ink/xbox/south.png" width="30" alt="A"> | <img src="res/ui/glyphs/ink/xbox/south.png" width="30" alt="A"> | <img src="res/ui/glyphs/ink/kb/j.png" width="30" alt="J"> |
| Action / Cancel | <img src="res/ui/glyphs/ink/xbox/east.png" width="30" alt="B"> | <img src="res/ui/glyphs/ink/xbox/east.png" width="30" alt="B"> | <img src="res/ui/glyphs/ink/kb/k.png" width="30" alt="K"> |
| Target / Aim | <img src="res/ui/glyphs/ink/xbox/lt.png" width="30" alt="LT"> or <img src="res/ui/glyphs/ink/xbox/rb.png" width="30" alt="RB"> | <img src="res/ui/glyphs/ink/xbox/rt.png" width="30" alt="RT"> | <img src="res/ui/glyphs/ink/kb/r.png" width="30" alt="R"> |
| Sprint | <img src="res/ui/glyphs/ink/xbox/l3.png" width="30" alt="Left stick click"> toggle, or hold <img src="res/ui/glyphs/ink/xbox/lb.png" width="30" alt="LB"> | <img src="res/ui/glyphs/ink/xbox/lt.png" width="30" alt="LT"> | <img src="res/ui/glyphs/ink/kb/e.png" width="30" alt="E"> |
| Creep | <img src="res/ui/glyphs/ink/xbox/r3.png" width="30" alt="Right stick click"> toggle | <img src="res/ui/glyphs/ink/xbox/west.png" width="30" alt="X"> | <img src="res/ui/glyphs/ink/kb/u.png" width="30" alt="U"> |
| Reload | <img src="res/ui/glyphs/ink/xbox/west.png" width="30" alt="X"> | <img src="res/ui/glyphs/ink/xbox/rb.png" width="30" alt="RB"> | <img src="res/ui/glyphs/ink/kb/q.png" width="30" alt="Q"> |
| Quickcast | <img src="res/ui/glyphs/ink/xbox/north.png" width="30" alt="Y"> | <img src="res/ui/glyphs/ink/xbox/north.png" width="30" alt="Y"> | <img src="res/ui/glyphs/ink/kb/i.png" width="30" alt="I"> |
| Spells | <img src="docs/glyphs/dpad.png" width="30" alt="D-pad"> | <img src="docs/glyphs/dpad.png" width="30" alt="D-pad"> | <img src="res/ui/glyphs/ink/kb/arrows.png" width="30" alt="Arrow keys"> |
| Tome / skip a cutscene | <img src="res/ui/glyphs/ink/xbox/start.png" width="30" alt="Menu"> | <img src="res/ui/glyphs/ink/xbox/start.png" width="30" alt="Menu"> | <img src="res/ui/glyphs/ink/kb/enter.png" width="30" alt="Enter"> |
| Pause menu (saves, options) | <img src="res/ui/glyphs/ink/xbox/back.png" width="30" alt="View"> | <img src="res/ui/glyphs/ink/xbox/back.png" width="30" alt="View"> | <img src="res/ui/glyphs/ink/kb/escape.png" width="30" alt="Esc"> |

PlayStation and Switch controllers use the same buttons in the same places, with their own icons.

## Building from source

```sh
tools/setup-deps.sh                           # pinned DolRecomp + GXRuntime, plus this project's patches
python3 tools/generate.py /path/to/disc.iso   # translates your disc's code into generated/
cp /path/to/disc.iso dump/GEDP01.iso          # the European disc
```

- **Windows**: configure with clang-cl and Ninja (`build-win`) from a Visual Studio 2022 tools prompt
  (`vcvars64.bat`), build `EternalDarkness`, copy the DLLs from the build tree next to the exe, then run
  `play.bat`. After a change in `.deps/GXRuntime`, save it with
  `git -C .deps/GXRuntime diff > patches/gxruntime/eternal-darkness.patch` (`.deps` isn't in git).
- **Mac**: `cmake -S . -B build-mac -G Ninja && ninja -C build-mac EternalDarkness`, then `./play.sh`.
- **Linux / Steam Deck**: clang and Ninja (`build-linux`).

`generated/` (code translated from your disc) and `dump/` (your disc image) stay on your machine.

### How it works

The game's PowerPC code is translated to C by [DolRecomp](https://github.com/ExpansionPak/DolRecomp) and compiled
for your machine. It runs on [GXRuntime](https://github.com/aharonahdoot/GXRuntime), which supplies the
GameCube's devices and SDK, and draws through its gxcore renderer on Aurora and Dawn (Direct3D 12, Vulkan or
Metal). Co-op lives in the host layer, `runtime/host/`: it runs the game's own functions for each player in turn.
`docs/` holds the notes behind it, and `tools/ed/` the tools that map the game's systems.

---

## Credits

- **[griches](https://github.com/griches)**, for **[Eternal Darkness Recomp](https://github.com/griches/eternaldarknessrecomp)**,
  the recompilation that made Bound Souls possible: the native port, launcher, widescreen, controls, baked
  shadows, save slots and everything else under the co-op. Thank you.
- **Zombie**, for the HD texture pack.
- [DolRecomp](https://github.com/ExpansionPak/DolRecomp), [GXRuntime](https://github.com/aharonahdoot/GXRuntime)
  and [StrikersRecomp](https://github.com/aharonahdoot/StrikersRecomp), the tools and host this is built on.
- The [unending-occlusion](https://github.com/PattyTrish/unending-occlusion) decomp, for function names (CC0).
- Alegreya SC and Fira Sans (SIL Open Font License), and Kenney's Input Prompts (CC0).
- **Silicon Knights and Nintendo**, for the game itself.

Bound Souls is a fan project, not affiliated with or endorsed by Nintendo or Silicon Knights. Please own the game.

## Licence

GPL-3.0-or-later (see `LICENSE`), as the projects it builds on. `THIRD_PARTY.md` lists everything else a build
holds and its licence.
