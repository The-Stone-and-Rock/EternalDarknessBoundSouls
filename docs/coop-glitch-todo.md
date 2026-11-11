# Co-op glitch to-do, checked 2026-11-10

The user's playtest list of 2026-11-07 (and what they reported in play on 2026-11-08 and 2026-11-09), each item
checked against HEAD 61c05c0, the docs, the logs and the user's own words. Nothing here was run in the game. The
codes are the 2026-11-07 triage's. "Confirmed" means the user said in play that it works; "unplayed" means a fix is in
the code and was checked by script or log only.

The kit's test environment is outdated: every `saves/*.state` except `slot1.state` (2026-11-09) and every
`tools/ed/scenarios/*.json` predates the COFM save section and earlier fixes. Don't run `ed test` or load those states.
Make a fresh state from Jump to Game for each check and throw it away when the code changes.

## Open: nothing in the code yet

| Code | Glitch | What's known | First step | Size |
|---|---|---|---|---|
| T4 | Max: player 2's d-pad press with no Tome put "cannot use magic" words in player 1's half and hung the game | Words opened outside a recognised player 2 context go to player 1's half and pad. Same root as T3 | Repro by script from Jump to Game ch5 (`ED_COOP_P2_QUICK`), route the refusal words to the opener | hours |
| T5 | Karim: player 2 gets no EXAMINE at the Tome book unless they walk away and back | Their zone "inside" bit stays set when the enter script re-runs | Script repro `jtg.sh 4`, clear the bit on the re-run | hours |
| I5 | Karim: player 2's sword vanished from the Tome list until closed and reopened | Player 2's equip in their Tome copy reaches the real list only after the turn, so the list shown is stale | Refresh the copy's list after an equip | hours |
| E6 | Karim: player 2 doesn't trigger the Trapper dimension | Not known whether the Trapper even approaches player 2 | Fresh state near Trappers, `ED_COOP_ENEMY_LOG=1 ED_COOP_PLACE_LOG=1` | hours |
| A1 | Paul: a loud pop every so often in fights or spells | Reading only: volume worked out from whichever player is active, so it jumps. Not measured | `ED_PCM_OUT ED_AX_VOICES=1` run with player 2 far from player 1 | hours |
| M4 | Hard hangs (hang-20261106-224956, -225327) | `characters_reload` starts a game save from the host whenever the body on screen differs from the chosen character; it waited for good between rooms. Both reports stop there | Reload only when the settings changed, never because the body differs | hours |
| M2 | After a hang, Main menu leaves player 1's half on the broken spot, player 2 sees the launcher | Follows from M4: the menu queues a load nothing services | Fix M4; then make Main menu take the Quit path when the game thread has stopped | hours |
| G2 | Graphics options that don't turn off | Temple energy and spell streaks glow only through the HDR path (`settings.c:358-367`, `glow.cpp` `present_hdr`); Glow Off with HDR on still glows; Occlusion quality live with Occlusion off; Contact shadows/Haze build the atlas with shadows on "The game's"; per-pixel Off not applied to spell meshes. All still as `docs/graphics-review.md` lists | Show the emit chain in SDR, make Off mean off, then the rest of the review | hours |
| G1 | Co-op frame rate drops to 40 | 13 causes in `docs/graphics-review.md`, only the mip chains done. Co-op is capped at 60 (`settings.c:349`); a frame that misses 16.7 ms on a 120 Hz screen shows at 25 ms = 40 fps. The split draws the hidden third frame with the whole post chain and shadow atlas | Measure first (`AURORA_GPU_TIME_LOG=1`, 60 s walk), then stop drawing effects on the hidden frame | hours |
| G4 | Graphics options reviewed for cost and correctness without breaking what works (the user, 2026-11-10) | `docs/graphics-review.md` is the read-only review; nothing measured | Same measuring run as G1, then one option at a time | hours |
| G5 | Remastered spell graphics: scrap and redo with real lighting on runes and beams (the user, 2026-11-10) | The 2026-11-09 version is paused and off (`8ed2d6f`): runes floated, faced backwards, black blocks, beams thin, no fade. `docs/runes-3d.md` has the trace of the game's own spell effects | Design from the game's own effects first (trace in `docs/runes-3d.md`), then the renderer's spell tag so the glow shows in SDR | large |
| M1 | Title menu fires a press by itself after about a minute | Not traced. Suspects: `skip_card_inserted` (hle.c:3785, presses A when the card screen's step ran) and the game's own idle | Cold boot with logging, sit on the title 2 minutes, note the second and the row | trace first |
| C3 | Anthony Tome room: statue examine by one while the other examines the Tome, cameras mismatch | Never traced; which half is wrong isn't known | Fresh state in the Tome room after the Tome, `ED_COOP_LOG=1`, both roles | trace first |
| C4 | Anthony urn/monk room: cameras not aligned | Never observed in a log or picture; room probably 155 | Fresh state before the room's door, a screenshot of what "not aligned" looks like | trace first |
| C5 | Anthony urn/monk room: both held a few seconds after the entry scene | Host hold or the game's own timing: unknown | Same state as C4, plus one solo run to compare | trace first |
| C7 | Karim Trapper dimension: rune pads move player 1 but not the camera | Never traced | Solo trace of a pad first, then co-op | trace first |
| C8 | Anthony Tome room swirl missing in co-op (both halves fade and cut) | Renderer: the same-frame EFB copy in co-op. One try reverted (`docs/coop-failed-attempts.md:638`) | Compare pictures with logs/run-20261108-130144-solo-anthony-ref 2040-2110 | trace first |
| E2 | Anthony: enemies disappear when a player examines | Not known whether in the examiner's half, the other's, or both | Fresh state with a zombie in view and an examine nearby, `ED_DETAIL_LOG=1 ED_COOP_LOG=1` | trace first |
| E5 | Karim: player 2 can't hit the sanity-effect fake zombies | Not known whether the hit connects at all | Start effect 21 by script, log player 2's hit events | trace first |
| E7 | Karim Trapper dimension: player 2 missing, camera stuck | Depends on E6 | After E6 | trace first |
| E3 | Low-health (injured) moves for all characters and forms | Never checked anywhere | Set health by script as the finisher tests did, each body with a borrowed weapon | trace first |
| E4 | Anthony: all models stretched after the stairwell rune | Never reproduced. May have been the empty-moves or forms-after-load cause since fixed (61c05c0), or the detail pools | Fresh state in the stairwell, `ED_MOVES_LOG=1 ED_WPN_LOG=1`, play to the rune | trace first |
| C6 | Anthony: models bounce in the second rot scene at the fountain | Never traced | Fresh state before the scene, both forms named, a solo run to compare | trace first |
| I6 | Lindsey: items vanish and reappear in the inventories | Not traced; could be I5's stale list, the counts, or the copies | Fresh state in ch6, which items and where (list, quick slot, hand) | trace first |
| U1 | Using a long item (Bronze Necklace, script 0x5E6) holds player 2 | Seen 2026-11-02; nobody has re-run a long item since | Fresh state in Pius's chapter, one Use by each player, `ED_COOP_LOG=1` | trace first |
| U4 | Torch flames and ghost sparkles edge-on in player 2's half | Particles face player 1's camera. Paused by the user | Un-pause; nearest-player facing first | hours |
| U5 | Third and first person cameras change models (headless body seen by the other player) | Co-op camera fixes apply only to Original; headless needs per-half drawing in the renderer | Hold until Original is solid; then camera mode changes only the eye | large |
| T2 | Independent examine and pickup at the same moment | One look record, one words window, one input focus, one close-up stepper: the second press waits | A redesign; agree it first | large |
| S1 | Independent sanity effects per player | One sanity tracker; effects are rooms, cameras and sounds for "the player" | Design first (which effects count) | large |
| C2 | Ellia: in-world scenes must play only for the triggering player, everywhere | The general rule is not implemented: a scene player 2 triggers (other than a room's arrival scene) still moves player 1's camera. Only player 1 scenes with player 2 walking were script-checked. The necklace scene by player 2 is unplayed | The scene-camera rule below | large |

## Partial: some of the report is fixed

| Code | Fixed and confirmed | Still open |
|---|---|---|
| C1 | Pius room 183: player 2 walking into the block-holes scene no longer holds them for good (61c05c0); player 1's camera and controls through player 2's scene, player 2 walking through player 1's block scene and player 1 visible in their half (2026-11-10, the user: "That fixed it"). Five causes in `docs/coop-todo.md`, the last the hide rule: a script's hide of the player passed to the game zeroed the show word main's controller reads, so player 2's turn never ran | A block by player 2 from their Tome with player 1 walking (`tome_copy_left` as a use of theirs): script-checked only. Each camera after both blocks: not reported since |
| W1 | Any character plays a borrowed weapon's own moves (8615fdc, aa1b306, 58ef373): Anthony with a gun, Ellia and her forms' reload, Pius's own sword and torch, confirmed in play | Peter with the Chakrams (the report's own example) script-checked only; Dual Flintlocks on anyone but Max keep Max's moves (Peter has no part 119); Elephant Gun on Peter, a form put on while holding a weapon, player 2 picking a weapon up in play: not tried |
| T3 | A zone's room request during the other's words waits (scenewords, 2026-11-06) | Player 2's examine words in Paul's chapter landing in player 1's half: never re-traced, no fix. A door pressed by B with the other's words up: unverified. Same root as T4 |
| I8 | Cheats All magick without the teaching loop (2f3c348, the user: "That worked") | Anthony drawn flat after the Tome film: did not reproduce, the `wpn_check` repair has never been seen to fire; leftover transition sound and the casket starting with the Tome transition: not re-checked; the swirl is C8 |
| E8 | The "switch player" load screen from Summon Trapper: `characters_reload` no longer reloads a non-playable body (unplayed) | Both held after the cast, both transported, player 2's camera at the exit with no model, player 1's large distorted body: all open. Which player cast isn't known |
| U2 | `OSCheckActiveThreads` crash after a load (6981f80, 40 scripted reloads) | Player 2 can't cast in room 0x72; both held 30 s in room 0x67: each seen once, undiagnosed |
| U3 | Tome open leaves the world running (TW_WORLD_FRAMES) | Cursor border pulses twice too fast (counter not found); 80% speed with both Tomes open; door, death, cutscene or quick save with a Tome open untested |
| U6 | Forms' portraits ship (464711b) | Launcher music made on first play and the "Building... Please wait" screen: no code. The user hasn't said the portraits look right since 464711b |

## Fixed in the code, not yet confirmed by the user in play

| Code | Glitch | Fix | To confirm |
|---|---|---|---|
| C9 | Pius room 182: the statue's words (crossed into by player 2) in player 1's half, nobody held, player 2's stick scrolls them and no B closes them (the user, 2026-11-10) | Words a zone's script opens inside player 2's own look registered their input channel and focus in player 2's input copy, which the turn puts away. `coop_prompt_look` now takes such a window as a close-up of theirs (`coop_window_to_game`), the same state a use of theirs leaves. Then the user: the words right, player 2 running on under them; the game's control-channel hold is now done in their copy (statue-look7) | Player 2 walks up to the statue: the words in their half, player 1 free, player 2 still, scroll with their stick, B on the last page closes. Then player 1 the same. Any other zone whose script opens words, by player 2 (chapter 2's statue words, a barred door's words walked into) |
| C10 | Pius room 183: a block from player 1's Tome during player 2's block scene refused, "Ellia cannot use that now" in player 1's half (the user, 2026-11-10) | Player 1's Tome in the copy (over player 2's close-up): `coop_run_script` ran its item as player 2's. Now only the copy that is player 2's does | Player 1 uses a block while player 2's scene plays: accepted, named for Pius. Note: it goes in after player 2's scene ends (`tome_copy_p1_left` waits for their close-up); scenes at once not tried |
| C11 | Pius room 183: after player 2's block scene, player 1's examine (a filled hole) showed in player 2's half, then both frozen till game over (the user, 2026-11-10) | The cameras stayed lent to player 2 after their Tome item's scene (no route gave them back). Now one rule at the pad read gives them back whenever nothing of player 2's is up | Player 2 inserts a block from their Tome; after its scene player 1 examines a hole: in player 1's half. The freeze after the examine isn't explained: if it comes again, close the game right after and say what each player pressed |
| C12 | Pius room 183: as player 2's block scene ended during player 1's walk-in scene, player 1's half took player 2's camera and hung briefly (the user, 2026-11-10) | The camera lend swapped the records but not the live view, so the game eased from the other player's shot. `coop_cameras_lend` now carries each player's own live view both ways | Player 2 inserts a block from their Tome while player 1 is in a scene or walking: neither half should jump as the scene ends |
| C13 | Two scenes at once: one scene's end let go of both players (blocks-user3 8675, statue-user2 10770) | Per-player scene records in `coop_scene_set`; the other's hold is kept at an end; neither gets the other's hold opened while both are on | Player 2's block during player 1's walk-in scene, and the reverse; a different block by each, either order |
| W2 | FINISH prompt: text only, no icon | `coop_prompt_style` (hle.c:17552) gives player 2's prompt the game's own `\i6` icon-above-words style; player 1's icon cell un-dimmed (buttons.cpp). Never seen in a run | Finish a downed enemy with each player. The disc's words are "FINISH HIM" (prompt 23); if "Finish" is wanted, player 2's is one line, player 1's needs a text hook |
| T1 | Pickup notices with the wrong name; "Michael" in Anthony's subtitle | `coop_name_choice` (8615fdc); player 2's find words name the finder (8ed2d6f) | Each player picks up a ground item; player 1 as Michael at Anthony's end scene. Open question: a form that is another person (Maid, Legrasse, Chandra, Bishop, Custodian) is still named by the screen's character |
| I4 | Ellia: broken sword repaired by Enchant Item | Same fix as the urns (`coop_controlled_id`, 58ef373) | Ellia's chapter, both players repair the Broken Short Sword; the mended thing is then a weapon player 2 holds as a copy, which the urn didn't exercise |
| I7 | Max: player 2's ammo count shown in player 2's half | Icon and reload confirmed by the user; the count's placement checked by script only (logs/run-20261107-022030-reload-hud3) | After player 2 reloads, what each half's corner shows |
| E9 | Stamina never tires | `coop_breath` per player (hle.c:12025). Only player 2 sprinting while player 1 stood was logged. `g_coop_breath` is host memory, so stale after a save-state load | Sprint with each player while the other stands; each should slow after about 3 s |
| G3 | MSAA and anti-aliasing as two rows | One Anti-aliasing row: Off / SMAA / 4x MSAA / 4x MSAA + SMAA (8615fdc, frontend.cpp:1999) | Open Options > Graphics; MSAA applies from the next start |

## Fixed and confirmed by the user

| Code | Glitch | Commit | The user's words |
|---|---|---|---|
| E1 | Enemy sanity cone missed player 2 | 8615fdc | "enemies sighting player 2 is fixed" (2026-11-08) |
| I1, I2, I3 | Enchant Item: urn got runes instead of repair; player 2's enchant floated the item over player 1 | 58ef373 | "The urn was repaired", "That worked but...", then "Commit." (2026-11-08) |
| M3 | Max fireplace: player 1 invisible, player 2 held, hang until B | 52e2685, 2029149, 0cc5e57, 5dad4fe | "Players vanishing using the fireplace was fixed" (2026-11-08) |
| I7 | Max: player 2 couldn't reload | 8615fdc, aa1b306 | "Player 2 reload I saw worked" (2026-11-07) |
| | Player 2 kept the dream's Shotgun | f8f1bd8 | the dream "played and confirmed" (coop-next-session.md, 2026-11-08) |
| | Anthony's casket by player 2; player 2 into the Tome room first | 39e1eef, 98660d1 | "Yes, that part is fixed. Commit this." (2026-11-08) |
| | Each player's own torch | 13bb558 | both lit at once in the user's play (2026-11-08) |
| | Forms lose their shape after a save-state load | 61c05c0 (COFM) | "That fixed it. Commit to that." (2026-11-09) |

## The path to full co-op, by code (2026-11-10 night)

Every open or partial code above, against the piece of work that resolves it. The levels are the plan in
`docs/coop-todo.md` ("Both players acting at once"); their design and record are `docs/coop-level3-design.md`.

| Work | Codes it resolves or unblocks | State |
|---|---|---|
| Level 1, two scenes at once (per-player scene records, `coop_scene_set`) | C2 (a scene plays only for its player, everywhere), C13, the scene halves of C1, C5 | Built and played 2026-11-10 (C13, C1); C2's general rule is this one, to be played across chapters |
| Level 3 step A, a words window per player (`ED_COOP_OWN_WORDS=1`) | T2's words half, T3 (the second player's words no longer wait), T4 (words of player 2's in their own window), the words half of C3 | Built, script-verified both ways, unplayed |
| Level 3 step B, a close-up per player (DETAIL, RETURN, PICK UP in player 2's own set and copy) | T2's examine half, C3 (the statue examine beside the Tome), E2 (enemies hidden only from the examiner's view: the hide-others rule), the rest of C9 to C12's route | Design pass running (readers, design, refuters) |
| Level 3 step C, a find per player | T2's pickup half, U1 (a long item's words held player 2: an item's words are a window of the user's), the pickup half of T1 | After B |
| Level 3 steps D and E (the Tome copy's interplay, save states, room changes, cutscenes while a window or close-up is up) | M4 and M2 (hangs and the menu after them, where a load or a reload meets an open window), U2 (the crash after a load), U3's open items | After C |
| Level 3 step F (cleanup: one owner rule, the lend and the screen flag gone) | the "camera on the wrong player" family: C4, C7, C11, C12, U5's co-op part | After E |
| Scene rule across chapters (level 1 played in chapters 2 to 12) | C2, C5, C6 | Play, with `ed chapters` |
| Sanity per player (design first: which effects count) | S1, E5, C7's rune pads, E6 and E7 (the Trapper dimension is a sanity effect) | A design pass of its own |
| Enemies and bodies | E3 (injured moves), E4 (stretched models), W1's leftovers, E8 (Summon Trapper's switch) | Each a reading pass on its function |
| Items and Tomes | I5, I6 (the lists and counts), I8's leftovers, T5 (the Tome book's zone bit), U6 | Each a reading pass on its function |
| Graphics | G1, G2, G4, G5 (the spells redo), U4 (particles facing each camera), C8 (the swirl) | The graphics review page first (`docs/graphics-review.md`), then one change at a time judged on the whole game in pictures |
| Sound and menu | A1 (the pop), M1 (the title's press) | Each a reading pass on its function |

## The rules behind most of the open items

Most open rows above are one of four rules not yet written once for the whole game:

1. **Act as player 2 is one complete swap.** Today it is about six partial ones (`coop_as_player2`, `coop_be_player2`,
   `coop_events_words`, `coop_zones_swap`, `coop_states_swap`, the pad globals), so whether a call works depends on
   which route it took. Behind T3, T4, T5, I5, E5, E6.
2. **Words belong to whoever opened them**, in their half, closed by their pad, and a door waits while any words are up.
   Behind T3, T4, part of T1.
3. **A scene's camera writes go only to the triggering player's camera set**, kept across a room change inside the
   scene, and each camera comes back to its own player when the scene ends. Behind C1, C2, C3, C4, C7, and the Nov 9
   controls-backwards report.
4. **A camera mode changes only where the eye is**, never models, detail or enemies. Behind U5.

Each should be one helper at the game's own function (found with `ed verb`, `ed needs`, `ed uses`), with every route
listed once from the kit, then one cold Jump to Game pass over the 12 chapters for both players before any commit.
