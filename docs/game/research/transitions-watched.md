# Door transitions, watched in the running game

The static report is `transitions.md`. This records four real runs, which settle what it left open. **Each is a normal player 1 with player 2 following.** The game's own door scripts ran through `ED_RUN_SCRIPT`, twice each: once as the zone's enter, once as B's press. `python -I tools/ed/facts/checks/watch_door.py NAME` reprints each from its `both.tsv`.

| Run | Save, room | Script | Transition | Goes to |
|---|---|---|---|---|
| `run-20261103-222005-w15` | slot 1, 147 | `L03/1097` | 15 | 21 |
| `run-20261103-222115-w12` | slot 3, 21 | `L03/642` | 12 | 147 |
| `run-20261103-222218-w9b` | slot 7, 155 | `L03/656` | 9 | 154 |
| `run-20261103-222028-w2` | slot 1, 147 | `L03/898` | 2 | 150 |
| `run-20261103-222052-w9` | slot 3, 21 | `L03/420` | 9 | 152, into a cutscene |

Logs are not committed, so these runs stay on this machine only.

## Answered

**A tick is one frame.** The ticket falls by 1 a frame. From ticket 61 to the room change took 63 or 64 frames, 1.05 s. At ticket 62 that is about 1.05 s for 9, 12 and 15. Transition 2 took 14 frames from ticket 11, 0.23 s.

**The door's motion is still playing at the room change, on all three,** including motion 104, which the report couldn't call:

| Transition | Leaving, state 121 | State at the room change | Arriving, state 80 | Then state 81 | State 1 after |
|---|---|---|---|---|---|
| 15 | 104 | 104 | **103 for 80 frames** | Motion 2, 23 frames | 103 frames, 1.7 s |
| 12 | 103 | 103 | **104 for 60 frames** | Motion 2, 23 frames | 83 frames, 1.4 s |
| 9 | 25 | 25 | **25 for 106 frames** | Motion 2, 23 frames | 129 frames, 2.1 s |
| 2 | None, state 1 throughout | | None | None | At once |

So event 238's map (25 to 25, 103 to 104, 104 to 103) is proved, and state 80 and 81 run as read. The frame counts equal the motion lengths in `transitions.md` (slots 1 to 10: 25 is 106, 103 is 80, 104 is 60). Motion 2 is cut short: 23 frames of a 52 update motion.

## Seen in the pictures

Sheets of 12 frames, both halves, each run's `sheet-*.jpg`:

- **9, a door:** the player steps forward as the picture fades, to black by about the 60th frame. Then the new room fades in over about 10 frames. The arriving player moves about 50 units in from the door end of the corridor (position 1311,480 to 1310,428) while motion 25 plays. Whether it reads as a backward step can't be told from these pictures, as player 2 stands in front of them.
- **15, climbing down:** the character moves as it fades, then comes up on the camera's far side and turns off the ladder, with CLIMB UP on show from arrival, since they're in the ladder's zone.
- **2:** the picture dips briefly and they're just there.

Motion names are from what scripts ask for. "Climb" here means prompt and transition, as the pictures are dark.

## For two players

**In all four, player 2 stands still and plays no motion through leaving or arriving** (state 1 motion 15, or state 7 motion 11 where they'd been in a state of their own). `coop_follow` places them 150 units from player 1 and sends no event 238.

**In slot 7's door, player 2 lands in line with player 1 and hides them from the camera.** Both cameras were the same. Player 2 opening a door was not run.

## Limits

- The scripts ran from the host at pad time, not from a real zone entry and a real B, and nowhere near the door. Motions turn toward the nearest flag 4 marker, so positions don't match a walk-up.
- Not watched: 0, 1, 3, 6, and the numbers no script uses. Their fades and motions are read from the code and the jump table.
- Pictures are every 100 ms, not every frame.
- `L03/420` with flag 340 off ends in a cutscene: state 80 lasted 2 frames, so the motion's end was cut by the scene.
