# Levels

`ed level [N]`. **The level is the chapter word `0x80307178`,** which equals the loaded level at `r13-30472` in every save in play. Most levels have their own scripts (`ScrLvlNN.gpk`), words (`URmTxtNN.cmp`) and placings (`NpcsN.npc`): level 12 has no Npcs, 14 and 15 only `U_Level`, and 17 no scripts. Evidence: `docs/game/research/arguments.md`.

**A script that isn't in the loaded level does nothing, silently.** Most scripts are in many levels: `ed script ID` says which, and whether they're the same.

- **Level 0** is Alex in the mansion. It becomes level 12's files once flag 787 is on (`0x80040650`, or script 1020).
- **Level 13** is reached by `ed87(12, 13, 1)`, in script 1010.
- **Level 16:** chapter 16 reads level 0's scripts and words. `ScrLvl16` is 299 copies of level 9's scripts, and loads only by way of argument 19, from `0x8006C708`. So `L16` rows in `ed item`, `ed door` and `ed mix` aren't a normal level's.
- **Levels 14, 15:** no way in was found. **17:** cinematics and audio only.

**`ed87(a, level, c)`** changes chapter. Scripts 2028 and 1572 to 1581, the Chapter Pages, call it.

**Who** comes from name counts in each level's words, so it's inferred, and the page label isn't always the person: level 4 is JOSEPH, and level 8 ABDUL. The model column is the game's table, and `player_model()` in `runtime/host/hle.c` can override it with a chosen character.

**Alignment:** `ed17()` is 1 Chattur'gha, 2 Ulyaoth or 3 Xel'lotath, set in level 1 by script 655. Table 5 placings and many scripts pick by it. No save holds 2 or 3.

**Saves don't stay what they were.** The user re-saves, so check a slot before relying on it: `ed carried saves/slotN.state`.
