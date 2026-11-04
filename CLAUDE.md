# Eternal Darkness Recomp Fork

- **Before writing any co-op code, use the `ed` skill: `python -I tools/ed/ed.py keep`, then `how THING`, `verb NAME`, `system NAME`, `needs ADDR` and `host WORD`.** The game already has the function: find it, don't rebuild it. `docs/game/systems/` has a page per system. After any change, run `ed test`.
- Then `docs/coop-next-session.md`: where things stand, how the user wants this worked on, and what's next.

- Co-op Tome work: read `docs/coop-two-tomes.md` and `docs/coop-todo.md` before changing anything.
- Run a test session with every log on, both players recorded and pictures: `sh tools/ed/run.sh saves/slot7.state NAME`. Logs go
  to `logs/run-*-NAME/`; read `ed timeline` and `EVENTS.log` first. Freezes write `saves/reports/hang-*.txt`.
- Rebuild after `.deps/GXRuntime` changes with the VS2022 tools, then regenerate the patch:
  `git -C .deps/GXRuntime diff > patches/gxruntime/eternal-darkness.patch`.
- `runtime/host/controls.cpp` may hold uncommitted test edits. Don't commit them.
- Commit only when told, and don't push unless asked.
- Add items seen in logs or code to `docs/items-log.md`, in any session, with only what was actually seen.
- Player 2 menu work: read `docs/coop-player2-menu.md`, and add what each trace finds. `play.sh` turns on the `[trace]` log lines.
- A player freezing during the other's pickup or words, or an item on both screens: start with `docs/coop-no-pause.md`.
- Before writing a fix, check `docs/coop-failed-attempts.md`, and add any attempt that fails, with its code.
- All screenshot folders go inside `E:\Eternal Darkness Recomp Fork\Screenshots for Claude`, never the root of `E:\`.
