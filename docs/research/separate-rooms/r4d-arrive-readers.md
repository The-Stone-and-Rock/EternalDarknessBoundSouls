# R4d reader: player 1 arriving in player 2's room while player 2's scene runs (and the R4c coverage check)

Read only (2026-11-12). Nothing was edited, built or launched. I read the disassembly with `ed dis` / `ed fn`, the host
(runtime/host/hle.c at the working tree, HEAD 5eb040d plus uncommitted controls.cpp, which I did not read), and these
logs: logs/run-20261111-205757-r6c-walk3 (player 2's scene L01/419 apart), -222138-rooms-play and -221141-rooms-play (the
user's R4b/R4c plays), and arrival-readers.md / doors-readers.md / stay-readers.md for earlier facts.
Each fact is marked [read] (with the address or file:line I saw) or [inferred]. "Case A": player 1's door to a room Y that
is not player 2's room R2 (they stay apart). "Case B": player 1's door into R2, the room where player 2's scene runs
(they are together from the arrival on, mid-scene).

## Short answers

1. **Can it happen today?** Mostly not by a press or a walk-in exit. The host makes player 1's way out WAIT whenever player
   2's scene, close-up, words or hold is up, with no test of rooms: a zone exit waits (coop_way_out_p2_up, hle.c:19905 and
   19950, used by coop_script_go_to_room's `waits`), and a B on a prompt is kept until player 2's thing is over
   (coop_p1_press_kept, `up = coop_p2_scene_or_hold`, hle.c:25797). On top of that, player 2's fn22 scene sets the ONE
   prompts word r13-24048's scene bit, which hides every prompt but slot 0 for both players, in any room [read: the comment
   and the clear at hle.c:17936-17944; coop_hold_open keeps every bit but 4, hle.c:19521]. So today player 1 apart stands at
   the door until player 2's scene ends in the other room. What still reaches a change of player 1's during player 2's scene:
   a script of player 1's already under way that asks for its room later (a "door that asks first": hold, words, then fn66),
   game-code tickets (transition 17, a cinematic's end), and the kept press/way out going in on the frame the host thinks
   player 2's thing is over [inferred from the gates above]. Opening these waits apart is a design step of its own and is
   the precondition for everything below.
2. **What player 1's change does to player 2's running scene today, when it happens** (both cases):
   - At player 1's ticket the host forgets player 2's scene: notify_room_ticket bumps g_coop_tickets (hle.c:11300) and
     coop_p2_scene_on / coop_scene_p2s_on compare against it (hle.c:16927-16944). Only player 1's scene got its own counter
     (g_coop_p1_tickets, hle.c:11311-11312, R4b). [read]
   - From that frame player 2's half freezes: their scene's camera path is stepped only in a refused turn under
     `coop_p2_scene_on()` (hle.c:20588), now false; their turn stays refused (busy 32|2|4; coop_room_change_on is false with
     bit 32, hle.c:11285-11286). [read; the freeze itself inferred, not in a log]
   - At the arrival 0x80151FDC (no bracket for a change of player 1's: notify_ticket_run returns, hle.c:13859) the game
     clears the ONE busy word (0x801556CC(-1,0) at 0x80152184), so player 2's scene's hold bits 32 and 2 are gone; R4b's
     stay_busy_clear acts only inside player 2's bracket (hle.c:24429). [read]
   - The script-wait clear 0x80163F04(1) at 0x8015213C hits the GAME's table 0x805CAA40, which outside player 2's use is
     player 1's; player 2's waits normally live in the host copy g_coop_states.p2 (counted by coop_script_timers,
     hle.c:20259-20270) and survive. Only a wait of player 2's that was moved into the game's table (coop_state_to_game at a
     fn17 walk's end, hle.c:18592; g_coop_found_word) would be lost. [read; which kind a given scene uses: inferred]
   - After the arrival: busy 0, so player 2's turn runs mid-scene (they walk, their presses start things again), and their
     camera path is stepped inside their turn. When the path arrives, its script call (ARRIVAL_END_RAN 0x801EC1C8) no longer
     takes the "their own scene's end" route, which needs coop_p2_scene_on() and !g_coop_turn (hle.c:18483-18485): it runs
     as whoever is the player with the game's (player 1's) script table, where the scene's word isn't, so it finds state 0,
     not the state its end needs (the comment at hle.c:18480-18482 names exactly this: L01/419 "would have found state 0
     there, not its 1"). The scene's last step (camera back, fn22(0)) does not come for player 2. [read code; effect inferred]
3. **On arrival does player 1 see player 2's scene on their screen?** Not its cameras, in the normal case: the arrival camera
   0x80152614 -> 0x801EE998 / 0x801EE764 writes the live eye record 2 of whichever set is in (doors-readers.md:62), and at the
   world tick that is player 1's own (g_coop_set_in 1); player 2's scene cameras live in player 2's set (coop_script_cameras
   puts the owner's set in). Two ways it can still show on player 1's half [inferred]: (a) the scene's camera-arrival script
   run natively as above, as player 1 with the game's table, may start the scene again from state 0 in the room in play
   (Case A: player 1's room Y; Case B: R2) with player 1 the player; (b) on the lend route (player 2's use or close-up with
   g_coop_cameras.lent) the set in IS player 2's: the arrival camera lands in their set, 0x801EB550 clears their records'
   +108/+124 (0x8015211C), coop_camera_chosen returns at `lent` (hle.c:17212-17213), and player 1's half draws their kept
   g_coop_p1_view. In Case B player 1 sees R2's actors in the world from their own arrival camera; that is the room, not the
   scene's camera.
4. **Is player 1 held?** Case A: no (busy 0 after the arrival; player 2's scene's event 102 never reached player 1's room,
   see fact G1). Case B: no hold after the arrival today either (busy 0). If the scene's bits are kept (approach A2 below), the
   arrival's own re-hold of player 1 (0x801F6828(player 1, 1) at 0x80152190) plus busy 34 would pose player 1's door motion
   at the motion gate 0x800271F0 exactly as R4c found for player 2, unless the busy ask answers "not busy" for them
   (coop_p2_holds, which needs player 2's scene still known). [read gates; effect inferred]
5. **Does player 2's camera come back after?** Today: not reliably (point 2, last item): the scene's end step is lost for
   player 2 and their set stays on the scene's last camera until a zone of theirs chooses. [inferred]
6. **R4c's coverage:** R4c (stay_scene_look_again, hle.c:17165-17180; g_stay_arrival_camera, hle.c:15951-15968) covers ONE
   kind: player 1's fn22 in-world scene (g_coop_scene_of[0].function == 0x80042974, busy has 34, not cutscene_playing). Not
   covered (player 2's door during them): player 1's fn139 hold-scene, player 1's close-up (look kind 1), a script hold of
   theirs (kind 2), their words or find with no hold (g_coop_words_free / g_coop_find_free), and letterboxed cutscenes. For
   kinds 1, 2 and fn139, player 1's look is dropped at player 2's ticket (coop_p1_close_up's held test: busy 2|4 with no room
   bit for a change of player 2's, hle.c:19024-19028) and nothing re-arms it, so after player 2's arrival their turns stay
   refused until player 1's thing ends; the arrival camera kept (g_stay_arrival_camera) is set for any scene of player 1's,
   but only when coop_scene_on_of(0). [read code; not seen in a log]

## The facts (game side), with the player each one reads

G1. **Event 102 goes only to the sender's room.** 0x801556CC(bits, on) writes r13-23344; when the result is non-zero it sends
    event 102 from 0x801F62B8() (the current player id) to 0x801F5620(102, id, 0, room) with room = 0x801F662C(0x801F5F88(id))
    (0x801556FC-0x80155720) [read]. 0x801F5620 delivers to every node whose 0x801F662C(node) equals that room or is -1, skipping
    the sender (0x801F5654-0x801F568C) [read]. So:
    - player 2's scene fn22(1), run as player 2 (their use or look: walk3 log line 1788, "who 59"), holds only R2's nodes;
      player 1 in another room is not node-held by it, only by the shared busy word and input channels [read + inferred];
    - player 1's ticket's busy_set(4,1) (0x80155770, run with player 1 the player) holds only player 1's room's nodes; player
      2 and R2's actors are not held by it apart [inferred from the same code]. notify_room_slots_tail's let-go of the
      stayer's hold (hle.c:14315-14322) matters only when they were together at the ticket.
G2. **The busy word is one** (r13-23344): bits 2 (hold, 0x80199EA0), 32 (fn22 scene), 4 (ticket), 8 (words), 16 (find),
    1 (fn91). The busy getter 0x80155738 is asked by the main update (0x8000749C), the motion steps (0x800271F0-0x8002721C,
    0x800272A8, 0x80027774, 0x80027804) and the node tick (0x801F5D48); each pairs it with node+28 bit 8 (0x801F68D4). The host
    answers it through coop_busy_ask (hle.c:19435) for every caller. [read: arrival-readers.md 1, hle.c]
G3. **fn139 (0x80042B30)**: 0x80199EA0(!on) (bit 2), r13-25696 := on, 0x8019C088, the prompts word bit 4 (0x80117260(4,on)),
    and on 0, 0x80134D48 (the "inside" re-mark). It sets no busy 4 and no 32 (0x80042B30-0x80042BB4). [read]
G4. **The change, in order, for a change of player 1's (no bracket, r13-23276 = player 1's room X, slot table +0 = X's slot):**
    0x80155830 (frame tick) -> 0x80152614(room, entry, camera, script, flags) [read 0x80152614-0x80152810]:
    1. 0x80152640 0x8004305C(room): the current id's "comes along" thing (player 1's).
    2. 0x80152298(room, flags): r13-26508 := 1 only if wanted == r13-23276 (doors-readers.md:54). For Case B wanted = R2 and
       r13-23276 = X, so r13-26508 = 0: a full arrival, not the "same room" short path [inferred from that test].
       The load 0x80154C94: Case A unpacks Y into the former other slot after room_slots_return swapped the indices so player
       1's old slot is evicted, not R2's (hle.c:14348-14356); Case B finds R2 "already in the other slot" (0x80154D64) and only
       swaps (its record kept: "a player is in it" at every tail, hle.c:14306-14310) [read host; the engine path from
       readers.md fact 1]. The load stamps r13-26524's object +592 (player 1's) with the new room (0x80154D00-0x80154D5C).
    3. 0x80151FDC(flags) [read 0x80151FDC-0x80152200]: current record r29 (table +0, now the new room's); r31 = 0x801F62B0()
       (player 1's node); 0x801E1CFC(1); 0x801E1B74(record+0x813C); 0x80117260(0x10000,0); 0x80138D84; unless flags 0x20
       0x8019F3F0(record) (sound); 0x8019E068(0/1,100,0) (the host's music replay hook sits at lr 0x8015206C,
       hle.c:14106-14111); 0x800BA530(0); 0x80043A5C(record) (the strip walk: player 2 spared while apart,
       coop_stay_spared hle.c:13846-13853; in Case B they are in the new room, the in-room branch, a no-op); unless flags 0x10
       0x80153AD4; if r13-26508 == 0: event 61 to player 1 (unless 0x80157364) and 0x801F5620(61, 0, 0, r13-23288) to the
       OLD room X's nodes, 0x80044428, 0x80157604 (flag), 0x80044748, 0x801EB550 (camera records of the set in); unless
       flags 0x4: 0x801172B0 (the game's six prompt slots reset, with 0x80024CDC -> 0x80024B24(0), the close-up cancel on the
       game's one record) and 0x80163F04(1) (the game's wait table, entries not marked kept, 0x80163F38-0x80163F6C);
       unless flags 0x8: 0x80153998 -> 0x801E0170/0x801E01D8/0x801E055C; r13-21876 := 0; **0x801556CC(-1, 0)** (busy := 0, no
       102 since the result is 0); **0x801F6828(player 1's node, 1)** (the arriver held); 0x80174B84; the fade in 0x801E7C88;
       the visited list; 0x801E4A80(0).
    4. Back in 0x80152614: the placing of r13-26524's object on the entry marker (0x80155348, 0x80151914, 0x80130EF0),
       event 238 to 0x801F62B8() for the arrival motion and 191 if mapped (0x801526D8-0x80152724); 0x800BA530(0);
       0x800428D0; 0x800428F0; **event 62 to the NEW room's nodes** (0x80152770-0x80152780, r6 = r13-23276) unless
       r13-26508; the arrival camera 0x801EE998(hash, 2, 0) + 0x801EE764 on record 2 + 0x801ECCE0 + 0x801EEB58(2) +
       0x801EEA9C; the script after arrival 0x801642D4(script, 0, 0) at 0x801527F4; r13-26280 := 0.
    Who each part is: everything keyed on the identity is player 1's (no swap); everything keyed on the current record is
    the new room's; the camera parts are the set in; the busy word, the prompt word r13-24048, the hold channels and the
    game's wait table are single. [read; per-player split inferred from the host's copies listed in H2]
G5. **Event 62 in Case B reaches R2's nodes that have been running for a while** (player 2's scene actors, R2's creatures,
    player 2's own node). The game never sends 62 to a room someone is already in. The zombie (type 4) behaviour's 62 branch
    (0x8003B724-0x8003B848) re-poses it by its flags (0x80033874's flags; 0x80125A70 / 0x801289D0 motions 15, 0x80118BEC),
    then 0x80066044, 0x800BAF78 (route), 0x800C7CDC and 0x801C5674 [read the calls]; what this does to a creature mid-scene
    or mid-fight is [inferred: a reset of its pose and route]. There are 68 `cmpwi rX, 62` sites in the disassembly
    (build/ed-db/disasm.txt) across many kinds [read count]; their effects are not read.
G6. **What the game's scene state is for player 2's scene apart** (from walk3, L01/419 begun by player 2 crossing SKPlane01):
    fn22(1) as player 2 (who 59, turn 1), busy 2 then 32 (lr 0x80199F38, 0x800429E4), flag 15 on, "player 2's own scene holds
    them: busy 34"; player 1 plays on by coop_p2_holds (log lines 1788-1809). Its end came from the camera path's arrival
    (lr 0x801EC1C8) at frame 4547, run first as who 1 and then re-run as player 2's by the p2_scene_end route (lines
    3138-3151). So this scene kind waits on player 2's camera path, which only the host steps. [read log]
    cutscene_playing() is the movie file or the letterbox (r13 scene flag + bars' height, hle.c:3784-3790): an in-world fn22
    scene is not a cutscene for any host test [read].

## The host today (what each part does for player 1's change while player 2's scene is on)

H1. Ownership: notify_room_ticket (hle.c:11297-11320): g_coop_tickets++, g_coop_room_p2 = false (player 1 the player),
    g_coop_p1_tickets++ (not a change of player 2's), g_fade_mover = 1, g_coop_arrival_scene = 0.
    - coop_p2_scene_on / coop_scene_p2s_on go false from here (hle.c:16927-16944). g_coop_scene_of[1] keeps the stale script.
    - coop_room_change_on (hle.c:11281-11288) is false for the whole window because bit 32 is set: no "the other goes on
      through it" exemptions (coop_node_held_ask hle.c:19458, room_turn, room_other in the turn hle.c:20577).
    - coop_p2_holds (hle.c:19378-19391) goes false (scene flag gone, and busy & ~(1|32) is 6 with the ticket's 4), so the
      p2_hold branch of hle_coop_controller (hle.c:20033) stops; player 1 still moves as the asker (fn66 cleared their bit 8,
      transitions.md "What fn66 does" 3) by the game's own controller [inferred].
    - player 2: turn refused (coop_busy), the refused turn's scene camera step skipped (hle.c:20588). Half frozen [inferred].
H2. The arrival: no bracket (notify_ticket_run hle.c:13858-13860), so stay_waits_clear and stay_busy_clear pass through
    (hle.c:24404, 24429). Per-player copies the arrival does NOT reach: player 2's prompt slots (the host's copy saved round
    their look), their camera set g_coop_cameras.p2 (unless lent), their script words g_coop_states.p2, their zone words
    g_coop_zones.p2, their own words/close-up/find copies (g_coop_p2_words / _look / _find) [read the structs: hle.c:15529-15532,
    15386-15396; docs/coop-no-pause.md]. What it does reach: the busy word, the hold channels' counts are NOT cleared by
    0x801556CC (it writes the word only) but the hold words r13-25696/-23040 are the scene's and stay [read 0x801556CC], the
    prompt word r13-24048, and on the lend route the game's camera set and close-up record, which are player 2's then.
H3. Cameras: coop_camera_chosen (hle.c:17206-17237) for the arrival's choice: Case A returns at coop_stay_apart (hle.c:17232;
    r13-23276 = Y, player 2's room R2 resident and different) so player 2's set is not remade; Case B is together, so
    room_choices++ (hle.c:17234) and coop_camera_p2_in would remake player 2's set from player 1's arrival camera only while
    p2_bare and !g_coop_p1_look and !g_coop_screen and their set at rest (hle.c:15405-15406). During their scene p2_bare is
    normally false (their scene's own camera choices ran as their use or look: hle.c:17221-17226) [inferred].
H4. coop_follow (hle.c:21204): Case B is the same room, the first branch: no bring; but its "stood inside player 1 for a
    second" re-place (hle.c:21262-21285) is gated only by cutscene_playing, not by an in-world scene, so player 2 standing
    still in their scene on the spot player 1 arrives at would be put beside player 1 after 60 frames [read; inferred to
    happen only if player 2's scene holds them where the entry marker is]. Case A: coop_stay_apart, nothing (hle.c:21271).
    "Whoever triggers a room or scene gets it": with ED_COOP_STAY the rule is already per player: the mover owns the marker,
    arrival camera and arrival script; the scene's owner should keep the scene. Nothing in the host carries the second half
    across a change of the OTHER player's (H1).
H5. Music: player 2's music acts apart are deferred and replayed when player 1 arrives in that room (coop_music_replay at lr
    0x8015206C, hle.c:13998-14016, 14106-14111): Case B plays the scene's music on player 1's arrival. Nothing for Case A.
H6. The waits that keep player 1 from leaving (short answer 1): coop_way_out_p2_up (hle.c:16435-16438), coop_p2_scene_or_hold
    (hle.c:25764-25767), coop_p1_press_kept (hle.c:25788-25850). None test rooms. Their history (docs/coop-failed-attempts.md
    p1scene / p1scene2, lines 510-557: "A room change mid-scene froze both players") is the together case; apart that freeze
    is exactly what R4b/R4c/R4d bracket.

## Case A vs Case B (what differs)

| | Case A (player 1 to Y, stays apart) | Case B (player 1 into R2, together mid-scene) |
|---|---|---|
| Load | fresh unpack into player 1's old slot (indices pre-swapped) | R2 found in the other slot, swap only |
| Event 61 | X's nodes | X's nodes |
| Event 62 | Y's nodes (normal) | R2's nodes, including player 2's scene actors: a repeat the game never does (G5) |
| Arrival camera | player 1's set; player 2's untouched (coop_stay_apart) | player 1's set; room_choices++ may reach a bare player 2 set (H3) |
| After the arrival | apart: busy 0, player 2 free mid-scene, scene end lost (short answer 2) | together: the same, plus player 1 in the room where the scene's actors are |
| What the rule wants | player 2's scene goes on in their half, their camera back at its end; player 1 plays on in Y | the same, with player 1 let go by player 2's scene (the together route coop_p2_holds), player 1 on their own camera |
| Music | nothing | player 2's deferred music replayed (H5) |

## Candidate approaches (all host-only, each gated on coop_stay() and a change of player 1's with player 2 staying in a room
## they are in; nothing for solo, nothing without ED_COOP_STAY; each one engine-level: no room, script or chapter named)

A0. **Open the waits apart** (precondition). coop_way_out_p2_up's use in player 1's look (hle.c:19905, 19950) and
    coop_p1_press_kept's `up` (hle.c:25797): with coop_stay() and coop_rooms_apart (hle.c:13926), player 2's thing does not
    make player 1's way out or press wait. Touches only the "waits" booleans. The prompt word r13-24048's scene bit is shared:
    while player 2's scene is on, player 1 apart sees no prompt but slot 0's, so a B door needs either that bit cleared for
    player 1's look/turn while apart (coop_hold_open already writes r13-24048 for the turn, hle.c:19521: it keeps the bit), or
    the kept press to be pressed for them by the game's pad as it already is (coop_p1_press_kept) with `up` false apart.
    Refuter question: coop-failed-attempts p1scene2's way-out lessons were for the together case; apart, is anything of the
    old "both froze" left (the play-hold word r13-25696 stayed 1 when the room changed before fn22(0))? A1+A2 answer it for
    the apart case only.
A1. **A ticket counter for player 2's scenes** (the mirror of g_coop_p1_tickets): a g_coop_p2_tickets bumped by every ticket
    except a change of player 1's under coop_stay() (hle.c:11311's mirror: `if (!(coop_stay() && !g_coop_room_p2))`), and
    coop_p2_scene_on / coop_scene_p2s_on / their start stamps (hle.c:16927-16944, 17051, 17069) read it. Touches 5 lines.
    Effect: through player 1's door window and after it, the refused turn keeps stepping player 2's scene camera (no freeze),
    the p2_scene_end route stays live (their scene's end runs as theirs: camera back, fn22(0)), coop_p2_holds' scene branch
    stays live, coop_play_hold's other_scene for player 1's script let-go stays right (hle.c:13075). With ED_COOP_STAY a
    change of player 1's never moves player 2 (coop_follow returns at coop_stay_apart; room_slots_return keeps player 2's
    slot), so the cut was only ever right for the together build. Leave g_coop_tickets itself alone: way_look, way_out,
    the kept presses, g_coop_use.tickets, the found words read it (hle.c:16468, 16560, 17909, 19004, 20755, 25811, 26446).
A2. **Keep player 2's scene's busy bits and waits through player 1's arrival** (the mirror of R4b): in the same two intercepts
    (stay_waits_clear at lr 0x80152140, stay_busy_clear at lr 0x80152188) a second branch for g_stay_bracket == 0,
    coop_stay(), !g_coop_room_p2 and player 2's scene/hold on (coop_scene_on_of(1) with A1, or g_coop_script_hold of theirs):
    keep busy & ~4 (written, not set: no 102 from player 1, as stay_busy_clear does), and put back the waits of scripts that
    ARE player 2's (coop_p2_script_is) in the game's table. Then player 1 after the arrival is let go by coop_p2_holds
    (busy 34 & ~(1|32) == 2) and the busy ask answers "not busy", so their door motion advances (the R4c gap mirrored: their
    re-hold at 0x80152190 plus busy). Needs A1. Refuter questions: (i) Case A with an arrival scene of player 1's own in Y
    (fn22(1) from the script after arrival): both scenes on, p2_hold false (hle.c:19385 `g_coop_p1_look`), both_scenes route
    (hle.c:20018-20026); (ii) the hold channels' counts r13-23060/-23052/-23056 are untouched by the arrival anyway; (iii) what
    the prompt reset 0x801172B0 at 0x80152134 does to player 2's scene is nothing in the normal route (the game's slots are
    player 1's), but on the lend route see A4.
A3. **Case B only: no second event 62 to a room a player already stands in.** A notify/intercept on 0x801F5620 at lr
    0x80152784 (event 62, r6 = r13-23276) that passes it over when coop_stay() and the new room is player 2's room as it was
    before the change (they were in it, apart). Engine-level (any room); it mirrors what the game does for the "same room"
    flag r13-26508 (0x8015275C). Refuter questions: does any kind need 62 to notice the second player arriving (a creature
    retargeting, a sanity effect)? The 68 handlers are not read; read at least the zombie's (0x8003B724) and the scene-actor
    kinds of chapter 1 before choosing. And the mirror case already in play: player 2 arriving in player 1's room (R4c) sends
    62 to player 1's room's nodes inside the bracket today; it has not been seen to break (R4b/R4c runs), which is weak
    evidence that 62 is mild for the zombie kind at rest.
A4. **The lend route across player 1's change.** When g_coop_cameras.lent (player 2's use/close-up on the screen route) and
    player 1 changes room, the arrival's camera writes, 0x801EB550's clear and 0x801172B0's close-up cancel hit player 2's
    set and close-up. The R4 pattern applied to the mover: player 1's set in (coop_cameras_in(cpu, 1) plus the p1_prompts the
    lend keeps) across notify_ticket_run's span for a change of player 1's, back after (coop_stay_change_end's mirror).
    Needs a reading of coop_cameras_lend (what else the lend swaps: prompts, looks, g_coop_p1_view) before it is designed;
    whether the lend route is ever up apart is itself open (Q3).
A5. **coop_follow's re-place in an in-world scene** (H4): add the scene test it describes ("Not in a scene") for the stay build
    (coop_scene_on_of(either)), Case B only. Small; together behaviour unchanged if gated on coop_stay().
A6. **R4c's missing kinds** (short answer 6): re-arm player 1's look at the end of player 2's change for kinds 1 and 2 and
    fn139 (a test like stay_scene_look_again's on the look kind and the busy bits the kind rests on), or don't drop it at
    player 2's ticket (coop_p1_close_up's room_bit for a change of player 2's under coop_stay()). The second is the smaller
    and touches the one test at hle.c:19028; the refuter must check it against the together build (coop_stay() gate) and the
    "a way out of theirs waits" rule player 2's side has.

## Open questions (with the probe that settles each; none needs a new build except where said)

Q1. Which scene kinds of player 2's wait on their camera path (fn11, like L01/419) and which on a timer (g_coop_states.p2) or a
    walk (fn17, coop_state_to_game into the GAME's table)? The last kind's wait is cleared by player 1's arrival. Probe: ED_COOP_LOG
    "[coop] script N goes on from the game" lines in existing runs; `ed fn` on the scene scripts of chapters 1-3.
Q2. Does player 2's turn swap SCRIPT_STATES when their camera path arrives inside a running turn (after the arrival made busy 0)?
    The callers of coop_states_swap are coop_prompt_look, coop_use_start, the scene-end route and the item script (hle.c:16570,
    17915, 18507, 26442); the turn's own camera step is none of them [read], so the arrival's script would run with player 1's
    table and player 2's identity. Confirm in a run with ED_COOP_TRACE ("script run r3 N ... who 59 ctl 59" at lr 0x801EC1C8
    with turn 1).
Q3. Can the lend route (g_coop_cameras.lent, g_coop_screen) be up while apart (player 2's Tome item use, a use's close-up)?
    continue_coop_use sets g_coop_screen when the use left the game busy (hle.c:17991-17996). If yes, A4 is needed.
Q4. What event 62 does to each kind mid-action (G5), and whether any kind relies on it to see the second player arrive.
Q5. A script of player 2's asking for its own room (fn66 as theirs) while player 1's ticket is on its way: coop_script_go_to_room's
    "one change at a time" refusal is gated on coop_room_change_on, which is false with player 2's bit 32, so a second ticket
    could overwrite the first (the ticket is one record, 0x8058AFFC). [read gate; inferred risk]
Q6. Game-code tickets (transition 17, kinds 39/50/82, the cinematic end) during player 2's scene: run as player 1's by default
    (g_coop_room_p2 false); with A1 they would not cut player 2's scene. Which player such a ticket belongs to apart is not read.
Q7. The prompt word r13-24048 is one: player 2's scene hides player 1's prompts in another room today. Is that the cause of
    any "player 1 can't examine while player 2 is in a scene apart" the user has seen? Not in the two play logs (neither has a
    scene of player 2's: `grep "own scene begins"` finds none).
Q8. The load's level-wide strip 0x80043AB8 spares only the stayer's node; R2's scene actors (kind 1/2 characters other than
    the players) are not spared. R6's runs show R2's zombies live after player 1's changes, so the strip is mild for them, but
    a scene NPC with a body set mid-scene is not checked.

## What I did not read
- The 68 event-62 handlers beyond the zombie's branch; 0x801755B8 (before event 62); 0x80044428 / 0x80044748 / 0x80174B84 in
  detail (stay-readers.md:131 lists them as current-record or identity readers).
- coop_cameras_lend's full swap list (for A4) and coop_prompt_look's full scene path (for A0's prompt bit).
- Any log where player 1's change happened during player 2's scene with ED_COOP_STAY: I found none (the three hits of a
  scan over logs/run-202611*/page-0001.log, run-20261106-152729-jtg-rega5-c2q-m5, -20261110-152519-scenes-user7 and
  -153136-scenes-user8, are together-build runs, and the user7 one is player 1's own scene). A scripted run of Case A and
  Case B on the build that has A0 is the first thing to record.
