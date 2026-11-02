# Eternal Darkness Recomp Fork

- Co-op Tome work: read `docs/coop-two-tomes.md` and `docs/coop-todo.md` before changing anything.
- Run a test session with every log on: `sh tools/coop-test/play.sh saves/slot7.state NAME`. Logs go
  to `logs/run-*-NAME/`; read `EVENTS.log` first. Freezes write `saves/reports/hang-*.txt`.
- Rebuild after `.deps/GXRuntime` changes with the VS2022 tools, then regenerate the patch:
  `git -C .deps/GXRuntime diff > patches/gxruntime/eternal-darkness.patch`.
- `runtime/host/controls.cpp` may hold uncommitted test edits. Don't commit them.
- Commit only when told, and don't push unless asked.
