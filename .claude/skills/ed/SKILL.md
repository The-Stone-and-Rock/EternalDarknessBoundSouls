---
name: ed
description: The whole-game tools for the Eternal Darkness recomp. Use before writing or changing any co-op, examine, pickup, use, mix, words, door, transition, room, location, camera, Tome, item or player code, when the user reports a co-op bug, when asked how any of these work in the game, and to look at what both players did in a run. Finds the game's own functions so nothing gets rebuilt from scratch.
---

Everything runs from the repo root as `python -I tools/ed/ed.py COMMAND`. `build/ed-db` is made once by `ed build` (a few minutes; `ed build facts` for the tables alone); make it again only if the disc or `generated/` changes.

## Which command

| About | Page | Commands |
|---|---|---|
| Doors, room changes, transitions | `ed system doors` | `ed door ROOM`, `ed transition [N]`, `ed verb door` |
| Camera | `ed system camera` | `ed room N`, `ed camera NAME --room N`, `ed where ROOM X Y Z` |
| Examine | `ed system examine` | `ed examine ROOM`, `ed verb examine` |
| Pick up, finds | `ed system pickups` | `ed verb pick up`, `ed item ID` |
| Use, Equip, Check, Mix, Mode, reload | `ed system use-mix` | `ed verb NAME`, `ed mix`, `ed item ID` |
| Item ids and names | `ed system items` | `ed item --list`, `ed item TEXT\|ID`, `ed carried SAVE`, `ed tag N` |
| Tome | `ed system tome` | `ed verb tome` |
| Location | `ed system location` | `ed where ROOM X Y Z`, `ed room N` |
| Prompts, regions, buttons | `ed system prompts` | `ed prompts`, `ed verb` |
| Levels | `ed system levels` | `ed level` |
| Which player a function reads | `ed system players` | `ed who ADDR`, `ed needs ADDR` |
| Words these pages use | `ed system glossary` | |

## Before writing anything

1. `ed keep`: what the user already has as they like it. Extend it. A correction or a move starts from what's there.
2. `ed how THING`: what the game does for a normal player. That's the reference, not a guess at how it should work.
3. `ed system NAME`: the system's page, the same for every room and chapter.
4. `ed needs ADDR|fnN` (or `--script ID`): the one-player data a game function reaches, and the host helper for each. Calling game functions is how this project works; this is how to call them without a mismatch.
5. `ed host WORD`: what the host has for it. `ed fn ADDR` for all that's known of one function. Add what you learn to `tools/ed/primitives.tsv` and `tools/ed/player_data.tsv`, saying how sure.

Never name a camera, item, room, door or transition from memory: `ed room N`, `ed camera NAME`, `ed item TEXT|ID`, `ed text WORDS`, `ed door ROOM`, `ed transition`, `ed script ID`, `ed prompts`.

**What a player does is one function for the whole game:** `ed verb [NAME]` (use, equip, check, mix, mode, examine, pick up, door, reload, talk, attack, cast...): what to call, its arguments, gates, and which player it reads. `ed mix`, `ed examine ROOM|TEXT`, `ed level`. Of the 413 script functions, 397 are named and 16 are unknown_N: `ed fn NAME`, `ed uses NAME`. `ed where ROOM X Y Z`, `ed tag N`, `ed item --list`, `ed camera NAME --room N`.

**Items have four numbers:** type id (always the same, `ed item ID`), handle (one play, what logs and `ed20` show), tag (from the placing), chr. `ed carried SAVE [HANDLE]` shows a save's, with what's in hand. `ed system glossary` for words the reports use differently.

**The game has two player pairs, current and main. Move both:** `ed who`, `ed system players`.

**A body twists on another's moves: each plays its own file's.** `ed system players`, Bodies, before touching any model. `ED_BORROW_CHAPTER=N` lists a level's bodies, and `tools/ed/facts/checks/moves_of.py N` a file's moves.

## Seeing both players

`sh tools/ed/run.sh SAVE NAME` runs the game as the user plays it, recording both players each frame, with pictures. It ends with `ed check`. Use it when the user plays too.

- `ed check RUN`: who froze, camera jumps and bounces, falls, stalls, still pictures. `STUCK?` may be a wall.
- `ed timeline RUN [FROM TO]`: both side by side, a line each time anything changes. Read this first.
- `ed see FRAME RUN`: one frame in full, with its picture's path.
- `ed sheet RUN F1 F2...`: a captioned sheet of both halves. Read the file it prints to look at it.

A recording holds only what it reads, so believe what the user saw over it. The log's `[trace] words` lines have the words shown.

## After changing anything

`ed test` runs every scenario in `tools/ed/scenarios` (nobody should hold a controller). A fix isn't done until it passes. When the user finds something a scenario missed, add one: their presses come from the run's log (`player N's pad`). `ed diff RUN_A RUN_B` shows what's new between two runs.

`ED_RUN_SCRIPT=DELAY:SCRIPT,...` runs a level's script from the host, for reaching a door or an examine without walking there. It isn't a real zone or press: code that branches on those return addresses isn't exercised. `ED_RUN_FRAMES` closes every running copy of the game by name, so don't use it with one open. `tools/ed/facts/checks/watch_door.py RUN` reads a door frame by frame.

`ED_INPUT_SCRIPT` goes round `controls.cpp`, so player 1's button handling needs the user's play. Say what wasn't covered.
