# The room mist per room: safety review of the build (2026-11-13)

Scope: Part A (A1-A10) in `runtime/host/hle.c` (search `fog_`) and Part B in `.deps/GXRuntime` (interp.cpp `mist_rows`,
common.cpp `mist_for`/`split_set_mist`/`plan_outputs`, aurora_backend.cpp `split_set_mist_op`), against `design-fog.md`
and `fog-impl-notes.md`. Read only: nothing edited, built or run. Lens: solo, follow, together, the Tome copy turn,
cutscenes, whole frames, state loads; every scope word restored on every path; lists never cleared mid-draw or left
stale; frame time; Part B false positives; the patch file. [real] = shown from the code or disassembly; [maybe] = a path
that can happen but whose game-side trigger was not seen.

## Problems

### 1. [maybe] A recompile forced by A4 also resets a flag that both rooms' section draws read
- Where: hle.c:26604-26606 (`fog_list` clears `+0x8138`/`+304`), so the next `0x801E75B4` call recompiles. The compile
  path clears `r13-21780` at 0x801E77BC, and `0x801E5330` sets it to 1 at 0x801E53A4 when any piece of THAT record has
  `+14 != -1`. The cached path of both sections reads that one global at 0x801E7724 (opaque) and 0x801E73E8
  (translucent) to choose `0x801E5D34(1)` or `(0)`. 0x801E5D34 sets GX state for the whole section draw: through
  0x801E1BB4 and 0x8021C514, with texture `r13-21800`, and skipped for room 50.
- Failure: apart, the forced section-3 recompile (after a state load, F6, or any list baked outside a scope) leaves
  `r13-21780` holding room 1's value. Room 0's section 2, in player 1's own half, then draws with room 1's flag. A room 0
  piece that needs the state 0x801E5D34(1) sets would lose it, and a room 0 without such pieces would get it set. Two
  resident rooms already had this (the last compile wins). A4 adds compiles at new times (state loads, apart, both
  sections), so a picture of player 1's room can change because player 2's room was recompiled.
- Fix: in `room_section_draw`, keep the flag per slot. After a guest draw in which `fog_list` saw a compile
  (`+0x8138 == 0` before the call), store `g_fog_flag21780[slot] = r13-21780`. Before each section's guest draw, both
  passes, write that slot's stored value when it is known, and put the previous value back after the draw. A minimal
  version keeps today's result: save `r13-21780` before a call that recompiles because of A4, and restore it after.
  Check it in F6b/F7: log `r13-21780` per slot at each compile.

### 2. [real, low] Together and with no player 2, A4 still recompiles the current room's list once per state load or boot
- Where: hle.c:26845 (`fog_state_load` sets both `g_fog_list_ok` false), 26876 (also at install), and 26602 (the clear
  for section 2 needs only `!g_fog_list_ok[slot]`). It is reached from the "one room for both" path, hle.c:14036, which
  runs together. It is also reached with co-op on and no player 2, because `fog_list` does not test `g_coop_p2`.
- Failure: Part A is not quite inert together. On the first co-op frame after every state load, and once after boot,
  the room in play's list is cleared and compiled again. It is the game's own first-draw path, compiled in its own room,
  so the bytes should match. It still runs 0x801E00C8 (resets `r13-22104`, `r13-22044`, `r13-22048`), the GX
  display-list compile and finding 1's flag. The log also gets an extra `[fog] list: ... cleared` line (printed without
  ED_FOG_LOG), so F6b's pass rule "exactly one cleared line for room 180 (slot 0)" (design-fog.md:501) will see two:
  section 2's 183 and section 3's 180. F3/F4 "ROOMS on and off equal" may differ only by that one recompile.
- Fix: return from `fog_list` when `g_coop_p2 == 0`. Then do one of two things:
  (a) Accept the one recompile. Write it in the notes, and change F6b's rule to one line per slot, with no recompile
  together beyond that one.
  (b) Make section 2's distrust narrower. Mark a slot "bad" only when a section-3 compile outside a scope is seen, or at
  a state load only for the OTHER slot. Clear section 2 only when it is marked bad. A state saved while a list was baked
  wrong in the current slot then keeps that list until the room is reloaded. That is today's behaviour, and the log says
  so.
  (a) is closer to the design's intent (1d).

### 3. [maybe] `g_fog_list_ok` is kept per slot, not per room: a compile it never sees leaves the slot's old verdict
- Where: hle.c:26212, 26580-26611. A verdict is only written when `fog_list` sees `+0x8138 == 0`. `fog_list` returns
  early with `!g_settings.mod_coop` (26581). `room_section_draw` passes straight to the game at 13995 (`running`,
  `!sda_ready`, `g_gx_load_pos_mtx == 0`).
- Failure: a slot holds room A with a verdict of true. Co-op is turned off (if the settings allow it mid-session), or
  the room draw hook passes through. The slot is loaded with room B, which is compiled under the other room's mist and
  not seen. Later, with co-op on, B's bad list is trusted (`g_fog_list_ok[slot]` is still true for A) and never cleared.
  This applies to section 3 apart, and to section 2 together and apart. The disassembly rules out an unseen compile of
  a record with +0x8142 or +0x8143 clear: 0x801E76A4-0x801E76BC returns before the compile.
- Fix: store the room with the verdict, `g_fog_list_room[slot] = room`, at the mark and at the clear. Treat the verdict
  as unknown, so the clear rule applies, when the slot's `+0x8140` room differs from it.

### 4. [maybe] A scope's slot can differ from the record the section draws
- Where: hle.c:26341 (`fog_begin` takes `slot = room_slot_of(room1)`, read from the host's cache `g_room_slot_room`)
  against hle.c:14019/14046 (the section draws, and A4 marks, the record of `other`, read from the slot table).
  `room_slot_of` returns the first cache entry equal to room 1. The cache keeps a slot's old room while an unpack is on
  its way (13906).
- Failure: a slot that is being unpacked still shows room 1 in the cache, and slot 0 comes first. Then A2 points the
  section's asks at that slot: E = 0 (its loaded bytes are clear) and its bank. Meanwhile section 3 draws, and may
  compile, the other slot's record. A list compiled then holds no mist and is marked "in its own mist" (scoped and not
  degraded), so A4 never clears it.
- Fix: pass the section's slot into `fog_begin` for `FOG_SITE_SECTION`/`_ALPHA`. Return false, with a skipped line,
  when `room_slot_of(room1) != other` or when the record's `+0x8140` room is not room 1.

### 5. [maybe] A section-3 compile marked "its own" when room 1's ask failed rather than room 1 having no mist
- Where: hle.c:26351 and 26265 (`fog_ask` returns 0 on a failed guest call too), then 26591 (the compile is marked own
  because it was scoped and not degraded).
- Failure: the 0x80153998 guest call stops (budget, or an exception changes, main.c:96-103) on the frame room 1's list
  is compiled. E1 is then 0, the scope sets the gate to 0, and the list is baked without mist and kept as "own" until
  the room is reloaded. This is rare, but it never heals.
- Fix: have `fog_ask` report "failed" apart from "none", for example through an out flag. In `fog_begin`, on a failed
  ask, set `g_fog_degraded = true`, so the compile is not marked and is cleared once a later ask works.

### 6. [real, low] Frame time: one to three extra guest calls per player-2-room actor, in every object-draw pass
- Where: hle.c:26497-26524. For each object of room 1 that is drawn, in the frame draw and in the other callers of
  0x8011A278 (16 bl sites, including the flattened shadow pass from 0x80028348), the code does:
  - `fog_ask(slot)`;
  - `fog_ask(2)` and a back-chain walk of up to 24 frames when the game's gate is off (26377-26379), which is every
    actor outside the frame draw;
  - a re-entered guest call of the whole object draw.

  Each `hle_guest_call` copies the CPU state twice and calls `getenv("ED_GUEST_CALL_LOG")` (main.c:107), and
  `getenv("ED_COOP_TOME_COPY")` when it fails. Effects of room 1 add one ask each, plus two GXLoadTexMtxImm calls
  whenever the room changes along the effect list (26571-26572).
- Failure: none in correctness; the design asks for the cost to be measured. With about 30 to 60 room-1 objects across
  passes, this is a few hundred guest calls and getenvs a frame. That is small, but it is the only part of this build
  that grows with the scene. No recompile loop exists: each bad list is cleared once (`g_fog_list_ok := true` at
  26606), a degraded scope never clears (26602), and the alpha pass never clears (26581).
- Fix (optional): skip the scope for actors when the gate would be 0 and E1 is 0. Reuse one `fog_ask(slot)` result per
  frame: cache it per `g_fog_window` and slot, since the record does not change inside a frame draw. Check frame time in
  `ed timeline` against ROOMS=0 (design 9: within 2%).

## Checked and safe (no change needed)

- **Solo and follow:** `fog_install` runs only with the two-slot room draws installed and `coop_stay()` (hle.c:31366,
  26880-26883); follow mode (no ED_COOP_STAY) installs nothing. Installed, every handler is inert without
  `mod_coop`:
  - `fog_record_ask` changes r3 only while `g_fog_slot >= 0`, and only `fog_begin` sets that, behind `lights_apart`,
    which needs mod_coop;
  - `fog_material` likewise;
  - `fog_texmtx_load`, `fog_list`, `fog_frame`, A8 and A9 test `mod_coop`;
  - the door hooks need `g_fog_door.on`, set only with co-op.

  What remains in solo: one host call per call of 0x80153998 and 0x801E21FC, and a host counter.
- **Together:** `coop_stay_apart` is false, so no scope opens (`fog_apart` -> `lights_apart`), effects return at
  14093, and fog_frame writes guest memory only for A9's H1, which needs `r6_apart`. A8 can fire only at the door
  where the players separate (a stayer); at a reunion the stayer's room is the current slot (26771). The one exception
  is problem 2.
- **The Tome copy turn and whole frames:** `lights_apart` tests `g_tome_copy_turn` and `g_room_whole_owner`.
  `room_slots_frame` (and so fog_frame and fog_views_frame) is skipped in the copy turn (13993). Effects return at 14087.
  `fog_views_frame` needs `g_room_whole_owner == 0`. The copy turn's dispatch skip (5623-5625) checks both the
  intercept and the notify of an entry. `coop_found_object_draw` was already on the list (30057), so adding
  `fog_object_draw` changes nothing there. In whole frames A4 only marks a compile (14013); section 3 is never cleared
  outside a scope (26602).
- **Scope restore on every path:** `fog_begin` writes guest memory only after every early return (26395-26413). Every
  caller calls `fog_end` whether its guest call succeeded or failed (14051, 14116, 26517). `fog_end` restores the 96
  bytes of the buffer it saved (`buf_at`, kept, so a buffer index change cannot misplace it), the gate and the bank, and
  clears the host scope state. The bank A2 writes at each material setup (26475) is inside the scope and is restored.
  The game's own gate change for objects with +580 == 104 is saved and restored inside 0x8011A278 (0x8011A39C save to
  r21, 0x8011AC44 restore), so the scope's restore matches it. Nesting: an inner `fog_begin` returns false while a
  scope is open (26335, 26500), so only the outer scope saves and restores.
- **Re-entering the object draw:** a `running` guard (26498-26516). On the inner dispatch the intercept passes through
  and the notify `coop_found_object_draw` runs once; on the outer call, which does not pass through, it does not run
  (5641-5653). 0x8011A278 reads r3-r8 only (0x8011A298-0x8011A2AC), and nothing from the caller's stack. The notify does
  not use lr, and the matrix marks test lr inside 0x8011A278, which still holds in the inner call. The install comes
  after the notify's install (30766 before 31366) and does not need `notify == NULL` (26898).
- **Lists never cleared mid-draw:** the clear is in the opaque pass's own intercept, before the game's function runs.
  The translucent list `+292` is made again by the same opaque compile. GXCallDisplayList copies the guest bytes into
  the GX stream when called (aurora_backend.cpp:2420-2424, Op::CallDisplayList), so a later recompile cannot change a
  frame in flight. The compile writes into the record's fixed buffer: GXBeginDisplayList(record+288, 0x51CA0), then the
  translucent list into the remainder (0x801E77C4-0x801E7864). Nothing is allocated, so a recompile leaks nothing.
  `r13-26912` is saved and restored by 0x801E75B4 itself (0x801E75C8 / 0x801E7928).
- **State loads:** `fog_state_load` (31622) resets the scope state (`g_fog_slot`, E, bank, flags), the list verdicts,
  the noise flag, A7's matrices (window 0, which never matches a live counter), scroll, H1 and A8.
- **Part B false positives:** a texgen row is rewritten only if all of its x, y and z are within 0.1% of
  `-P[2][0..2]/2000`, where P is that draw's own position matrix. The draw is skipped when `max|P[2]| == 0`, so an
  all-zero or unused texgen row can never match. Its w must also fit one room's band within 0.1% + 0.01. The only code
  in the game that reads the 2000.0 constant is the mist build (`-19188(r2)`, used only at 0x801E4EE8). The only loads
  of texmtx 54 are the mist sites (design 1.1), and every non-mist draw path leaves `texmatrices` alone. Only row 0 is
  written; rows 1-2 (the band) are left. Views that are exactly the identity (player 1's own half, and view 1 while
  lent; camera.c:1630-1658 and 1804 send exact identity and zeros) are left byte for byte, also on frames in between,
  because blending identity with identity stays exact. A frame whose stream sent no `SplitSetMist` (solo, the copy
  turn, whole frames, the split off, ED_FOG_VIEWS unset) is not touched (`fresh`, common.cpp:1716-1720). Part A's room-1
  rows (A' from player 2's eye) only match if player 2's camera has player 1's row 2, and then the rewrite gives the
  same value.
- **The patch:** `patches/gxruntime/eternal-darkness.patch` is identical, byte for byte, to
  `git -C .deps/GXRuntime diff`, with 79 files, as in HEAD's copy and in the pre-Part-B copy. Every line the
  pre-Part-B patch (scratchpad `patch-before.patch`) added is still in the tree, except 11 lines in common.cpp,
  interp.cpp and interp.hpp. Those are Part B's own change: the `split_build`/`split_between`/`build_view` signatures
  gained `const Mist*`. The 64 gxcore_draw.cpp lines that are missing against HEAD's patch are the earlier lights work
  (11-12), not this build. The patch is LF only.
- **A8:** the noise fill 0x801E01D8 builds the noise from constants only (0x801E0A98 with fixed arguments) into a
  temporary allocation, so keeping the stayer's noise gives the mover the same kind of noise. The lr checks match the
  bl sites (0x80152160/68/70 -> lr 64/6C/74), and `g_fog_door.on` is cleared at every change's setup (26763).
