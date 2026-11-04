# The Tome menu, and the Tome room

## The menu: one for the game, a copy for co-op

| Step | Game function or data |
|---|---|
| Start | `r13-24244` asks for mode 2 |
| Start opens it | `0x80006CDC(1, 0)`, then mode 3 by `0x80018808(3)`. `ed verb tome` has the gates |
| A script leaves itself for the Tome's end | `ed9(script)` `0x8000F8D4`, which writes `r13-24244` and `r13-26324`. It doesn't open a Tome |
| Mode | `r13-26188`: 3 in the Tome |
| Pages | Inventory `0x80105F9C`, Map `0x80112594`, Magick `0x8010F228`, Journal `0x8010952C`, Options `0x80101980` (`r13-24304`) |
| First-visit help | Scripts 2020 and 887, flags 989 and 990 |

**Works, and the user likes it:** two Tomes. `tome_copy_*`, `coop_menu_input`. Read `docs/coop-two-tomes-design.md` before changing it. Fix what the copy takes inside the copy, never the real game's scripts or text.

## The Tome room: the same script everywhere

**Script 1351 is the same in 15 levels.** It sets the room's cameras by region, and on `ed143() == 3` plays the first-entry scene once (flag 111, `IntroLookMkr01`). `ed script 1351`.

**For two players:** whoever triggered the room gets the scene: `coop_follow`, `48b6c88`. Compare with a solo run of the same save.
