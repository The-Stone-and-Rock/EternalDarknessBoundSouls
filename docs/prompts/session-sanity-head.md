# The next session's prompt (written 2026-11-18, the end of the second night session)

Paste everything in the block below into the new session.

```
Continue with Eternal Darkness: Bound Souls (E:\Eternal Darkness Recomp Fork, branch tools/game-kit, everything
committed at 1967947 or later, nothing pushed). Read CLAUDE.md, then the status board at the top of
docs/coop-next-session.md (the rules 1 to 15 and the items 12a to 12g), then docs/research/sanity-per-player/plan.md.

WHAT WE ARE WORKING ON: item 12, each player's own sanity effects in co-op. Start with 12g, the head.

12g, Head of Shakespeare for player 2 (everything else about the head is done and checked off: it lands, PICK UP,
the recital, REPLACE HEAD in their half, the return to their door):
- THE BUG: after player 2 picks their head up, the head is not in the middle of THEIR half. It is far too large, off
  to the left and cut by the edge, and their camera glides at the pickup. In single player the head floats to the
  middle of the screen, about a third of its height, turns slowly while it recites, and the camera does not move
  (pictures 21 and 22 in Screenshots for Claude/sanity-per-player/; runs logs/run-20261118-215123-jtg-head-solo-max2
  and -214708-max-head-centre).
- WHAT RIGHT LOOKS LIKE: the head centred in the triggering player's half and in proportion to that half, as single
  player has it on the full screen. THE GAME DOES NOT PAUSE. The player who triggered the head cannot move while it
  recites; the OTHER player must be able to move and play on the whole time.
- IDs: the head's own job 0x801FA520 (eye 0x801EB01C, view angle 0x801EB088, the screen point 320,165 unprojected by
  0x801EB15C at 0x801FA608); its window's record 0x8030A704, opened by 0x80024A14, closed by 0x80024818; the pickup
  0x800C362C; the part's handler 0x801FA07C; the effect's setup 0x8009A7AC and steps 0x8009A8D4, 0x8009AAC4. Host:
  sanity_pickup, sanity_head_close, sanity_p2_step, part_step_own_room, coop_found_place / coop_found_unproject (how a
  found thing is placed for player 2), g_coop_own_view, g_coop_cameras.lent.
- ALREADY TRIED, DID NOTHING, DO NOT REPEAT: the job with player 2's camera set in (coop_cameras_in); the job with
  their own view in the camera block as coop_found_place does. The cameras are lent to player 2 during the find and
  the unprojected point is already dead centre of their view, yet the head stands about 75 to the side and only 30 to
  50 ahead of the eye.
- FIRST STEP: one single-player run and one co-op run of the same thing in Max's chapter, logging the head's place
  and the camera frame by frame from the pickup on, read side by side. Find the first frame they differ. Then fix.
  Look first at why player 2's camera glides at the pickup when single player's stays.

After the head: 12d on the board (blood per player first: take its single-player reference in Max's bedroom before
building; then the phantoms' hits, the leftovers).

HOW TO WORK (the user's rules, all on the board):
- Use the ed skill and the kit before writing anything: python -I tools/ed/ed.py keep, how, verb, system, needs,
  host, fn, dis, text, prompts. The game already has the function: find it, do not rebuild it. Look a text or a
  prompt up in the kit FIRST (the REPLACE HEAD lesson). Check docs/coop-failed-attempts.md before any fix and add
  every try that fails.
- Test sanity effects in Max's chapter, not Alex's (the camera is close): the start max-249, the state
  saves/test-max-249.state, run with
  ED_SETTINGS=saves/test-max-249.settings.ini ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_RUN_AWAY=1 sh tools/ed/run.sh
  saves/test-max-249.state NAME (frames count on from 1762; the door 249 -> 252 is at -321,-887,3: ED_PUT, a tap of
  lup, then b). ED_COOP_P2_SANITY=10 and ED_SANITY_P2_TEST=FRAME,EFFECT ask for an effect as player 2.
- Player 2 first. Player 1 only when a bug appears or as the reference.
- Every run records BOTH players at once (both.tsv, shots/): read it with ed timeline RUN, ed see FRAME RUN, ed sheet
  RUN F1 F2 ..., ed zoom. Look at the pictures; do not judge from the log alone.
- Test only what is needed. One run that answers one question. Never the same measurement again and again. When
  something differs from single player, take the side-by-side log FIRST, not after several guesses.
- After an effect a player is never moved to the other player: the return puts them where they took the door.
- Do not break the prompts: they were hard to get right. Touch only the one thing being fixed.
- Solo play is not touched. Fixes are for the whole game and both players.
- Commit ONLY when you can report no bugs in what was just worked on (ed test passes, the pictures are right). Never
  push unless asked.
- Check the usage tracker (mcp__ccd_session_mgmt__get_usage) before starting and after each finding, and tell the
  user the number. After each finding report plainly and simply: what you saw, what it means, what you do next. At
  95% of the weekly limit stop at a commit and write the hand-off.
```
