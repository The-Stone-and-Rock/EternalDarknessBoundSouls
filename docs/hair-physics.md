# Hair and cloth physics: where to start (2026-11-09)

The user's wish: hair and flowing clothes that sway like Skyrim's HDT-SMP mods, at render time, without changing
the game's animations or skeletons, and kept apart for each player. Not started; read-only findings so far.

## The test character: Ellia (model 70)

Read from each playable's body in `build/ed-db/unpacked/*.raw` (not a live dump: `ED_BONE_DUMP`, hle.c ~8015, only
prints a worn form).

| Character | Bones | Hair chain | Notes |
|---|---|---|---|
| **Ellia** (70) | 77 | Two, 3 bones each, off the head (bone 14): right 29->31->45 (`FF12B320`, `12DB60EA`, `12DA20E8`), left 30->32->46 (`FF13F322`, `13AFE29E`, `13AEA29C`), from about 239 down to 220 high | Proven to move her hair: Alex's ponytail moves on them "stand out in spikes" (`docs/coop-failed-attempts.md` 614-619, hle.c ~9065). Bone 63, one off the root at the front, 76.7 high: maybe a front flap, unverified |
| Alex (9) | 73 | One, 29->30->44, the back of the head to the neck | Same names as Ellia's left chain (`coop-failed-attempts.md` 435) |
| Karim (150) | | Two bones, 40->42, behind the head | Same first two names |
| Pius, Max, Edward, Lindsey, Paul, Peter, Roberto, Michael, Anthony | | None | The hips carry only the spine and legs: robes, coats, skirts and sleeves are skinned to the legs and hips, so cloth needs a per-vertex step, not bones |

Chains found by those name hashes cover Alex and Karim too.

## Where it would go

`runtime/host/hle.c`, `hle_skin`: characters are skinned on the CPU.
- Bones: just before `hle_guest_call(cpu, g_skin_fn, ...)`. Every bone is posed by then: 48-byte 3x4 world matrices in
  the block `camera_bone_matrix` (camera.c ~1020) records as `g_bone_block`, for the player only today, so it needs one
  per character.
- Vertices (cloth): after the call returns, beside `camera_skin_end`.

## Care points

- The hair has hand-made motion already (a ponytail swings forward with a sword, camera.c ~241): sway adds to the
  pose, never replaces it.
- Collision: the chains end by the jaw (bone 55, 218.8 high): colliders on the head (14) and chest (5).
- First person: `camera_skin_begin` zeroes every non-arm bone's scale in the same bracket (camera.c ~1172). Skip or
  freeze sway then; more swing could also cross the near planes (FP_NEAR 45, FP_ARMS_NEAR 35).
- Co-op: one step per character object per game frame, not per view and not by model (both players can be Ellia).
- Forms remap bones and take the wearer's names (hle.c ~8030); the ghosts' hair follows the head, and `wpn_bind`
  (hle.c ~9065) holds Ellia's side braid. Look chains up by name after the remap.
- Use the real frame time, and reset sway on door teleports and cutscenes.
- Never change the character shadows' light direction (the user's rule).
