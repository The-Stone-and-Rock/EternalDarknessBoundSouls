# The new main launcher: what the user chose (2026-11-18)

Only the MAIN launcher (the screen before the game starts). The menu over a running game, the title look and the Tome
look do not change. No file or exe renames yet (the user: "rename later to avoid hiccups").

**The look to build is mock-up 7:** `Screenshots for Claude/launcher-redesign/mock-7a-check-updates.png` and
`mock-7b-update-available.png` (drawn by `mock-up.ps1` here: its numbers are the layout at 1920x1080).

- **Layout** as the Metroid Prime 4 Beyond recomp launcher the user showed: a narrow side bar of tabs at the left, the
  main panel beside it, a thin help line at the foot.
- **Colours and hands: the title screen's Choose Character page** (menu.rcss `body.skin-title`): panel #0a1518, lines
  #3b5359 / #486168, the lit thing #a0e3f1 with a glow, gold #c9ab68, blue #8fc8f0, words #8db2ba / #6d9098, the red
  diamond mark. La Belle Aurore for the tabs and the big buttons, IM Fell English for the words, Alegreya SC (shipped,
  not loaded yet) for the title and the section heads, Fira Sans for the small spaced labels.
- **Backdrop: the user's own picture** (`owner-backdrop-source.jpg`, made with Gemini, not from the disc, so it ships
  in res/ui and shows from the very first start). It sits BEHIND the panels, dimmed with the blue-black (about 77%).
  It is 1200x896: ask for a larger one if they have it. The autopsy drawings from the disc are NOT needed now
  (prep.json's steps 14 and 15 are dropped unless the user asks again).
- **Side bar:** "Eternal Darkness" / "Bound Souls" small at the top, then Play, Choose Character, Display, Graphics,
  Sound, Controls, Mods, Credits, and Quit at the foot. NO "Load Game" tab (the Load button replaces it).
- **Title block:** "ETERNAL DARKNESS: SANITY'S REQUIEM" on ONE line at ONE size, a faint icy glow drawn by the menu
  (no picture; the user asked for less glow than mock-up 3), "BOUND SOULS" smaller under it in gold with spaced
  letters, then "FOR ONE OR TWO PLAYERS" and a thin rule. The title's line sets the column's width: everything under
  it ends at the same right edge.
- **Buttons:** Play (lit), Load (opens the save states), Continue (the newest save state), three in a row, each as wide
  as the tile under it. Above them: READY at the left; at the right "Version X" and a **Check Updates** button. When a
  newer release exists the words become "Version X - Y is out" in gold and the button becomes a lit **Update**, which
  for now OPENS THE GITHUB RELEASES PAGE in the browser (the user: "open the repo for now until we have a stable
  package release"). GitHub's "latest" API skips pre-releases and every release is one: list the releases. No
  self-install yet.
- **Three tiles:** HD textures, Resolution, Frame rate, each with "Change".
- **Recommended settings:** a placeholder row, "Recommended settings for this PC: coming soon." with a greyed Auto-set.
- **Controllers:** Player I and Player II with what each plays with, and Configure. **Saves:** save states used.
- **Help line** at the foot.

Not decided (ask with a picture when reached): whether Continue stays as the third button (it was my addition), what
Credits shows, the small-window and 4:3 / 21:9 fits.

The groundwork is `prep.json` (maps of the launcher code, the panel data, a 17-step plan and a skeptic's 21 problems
with fixes: apply the fixes). The user's style for this work: quick, plain words, pictures first, checkpoint commits.
