import io
root = "E:/Eternal Darkness Recomp Fork/"
def load(p): return io.open(p, encoding="utf-8", newline="").read()
def save(p, s): io.open(p, "w", encoding="utf-8", newline="").write(s)
def rep(s, a, b, n=1):
    assert s.count(a) == n, (s.count(a), n, a[:100])
    return s.replace(a, b)

s = load(root + "runtime/host/hle.c")

# ---- the DTK handlers log with the frame (the music is player 1's: every change of the disc stream is a line) ----
s = rep(s, "// DVDPrepareStreamAsync(fileInfo, length, offset, callback); length 0 = to end.\nstatic void hle_DVDPrepareStreamAsync(CPUState* cpu) {\n",
        "static int g_stay_bracket;  // (below, R4)\nstatic bool coop_sound_log(void);  // (below, R5)\n"
        "// DVDPrepareStreamAsync(fileInfo, length, offset, callback); length 0 = to end.\nstatic void hle_DVDPrepareStreamAsync(CPUState* cpu) {\n"
        "    if (coop_sound_log())  // R5: the music is player 1's; a line per change of the disc stream, with the frame\n"
        "        fprintf(stderr, \"[dtk] prepare from lr 0x%08X, bracket %d (frame %llu)\\n\", cpu->lr, g_stay_bracket, (unsigned long long)(cpu->timebase / 675000ull));\n")
s = rep(s, "static void hle_DVDCancelStreamAsync(CPUState* cpu) {\n    dtk_cancel();\n",
        "static void hle_DVDCancelStreamAsync(CPUState* cpu) {\n"
        "    if (coop_sound_log())\n"
        "        fprintf(stderr, \"[dtk] cancel from lr 0x%08X, bracket %d (frame %llu)\\n\", cpu->lr, g_stay_bracket, (unsigned long long)(cpu->timebase / 675000ull));\n"
        "    dtk_cancel();\n")

# ---- player 1's node remembered at every swap ----
s = rep(s, "static void coop_be_player2(CPUState* cpu, bool on, u32 was[5]) {\n",
        "static u32 g_coop_p1_node;  // player 1's node, as the last swap found it (R5: the other player's room at a door)\n"
        "static void coop_be_player2(CPUState* cpu, bool on, u32 was[5]) {\n")
s = rep(s, """        if (on) {
            was[i] = mem_read32(cpu, at[i]);
            mem_write32(cpu, at[i], now[i]);
        } else {
            mem_write32(cpu, at[i], was[i]);
        }
    }
    for (int i = 0; i < 4; i++) {
        const u32 casting = cpu->gpr[13] - kCoopCasting[i], theirs = g_coop_casting[i];""", """        if (on) {
            was[i] = mem_read32(cpu, at[i]);
            mem_write32(cpu, at[i], now[i]);
        } else {
            mem_write32(cpu, at[i], was[i]);
        }
    }
    if (on && was[1] != 0u && was[1] != g_coop_p2)
        g_coop_p1_node = was[1];
    for (int i = 0; i < 4; i++) {
        const u32 casting = cpu->gpr[13] - kCoopCasting[i], theirs = g_coop_casting[i];""")

# ---- tw_keep: the change's sound acts first ----
s = rep(s, "static void tw_keep(CPUState* cpu) {\n    const bool ours = cpu->lr == 0x80111160u",
        "static bool coop_sound_change_act(CPUState* cpu);  // R5 (below): a door's fades, the arrival's group reset and fn151's duck, with a stayer\n"
        "static void tw_keep(CPUState* cpu) {\n"
        "    if (coop_sound_change_act(cpu))\n"
        "        return;\n"
        "    const bool ours = cpu->lr == 0x80111160u")

# ---- the copy Tome's close: its live handles stopped, its close sound ----
s = rep(s, """    g_coop_tome_p2 = true;
    coop_tome_player(cpu, false);
    cpu->ram = g_tome_copy.real;
    g_coop_tome_p2 = false;
    g_tome_copy_open = false;
""", """    g_coop_tome_p2 = true;
    coop_tome_player(cpu, false);
    // R5: the sounds its turn started live (tome_copy_sound) and still holds: the looped item sound (r13-24480), 509 (r13-29056)
    // and 688 (r13-24108), stopped live when the copy's handle isn't the real memory's; then the Tome's close sound (569),
    // which the copy never reaches (it runs no way out).
    const u32 copy_handles[3] = {mem_read32(cpu, cpu->gpr[13] - 24480u), mem_read32(cpu, cpu->gpr[13] - 29056u), mem_read32(cpu, cpu->gpr[13] - 24108u)};
    cpu->ram = g_tome_copy.real;
    tome_copy_sound_close(cpu, copy_handles);
    g_coop_tome_p2 = false;
    g_tome_copy_open = false;
""")
s = rep(s, "static void tome_copy_close(CPUState* cpu, const char* why) {\n",
        "static void tome_copy_sound_close(CPUState* cpu, const u32 copy_handles[3]);  // (below, R5)\nstatic void tome_copy_close(CPUState* cpu, const char* why) {\n")

# ---- the stream starts remembered with their room (the SANITY_STREAM notify sees every 0x801A4884) ----
s = rep(s, """static void sanity_stream_notify(CPUState* cpu) {
    if (g_sanity.effect >= 0 && g_sanity.sounds < SANITY_SOUNDS_LOGGED) {""", """static void coop_stream_started(CPUState* cpu);  // (R5, below)
static void sanity_stream_notify(CPUState* cpu) {
    coop_stream_started(cpu);
    if (g_sanity.effect >= 0 && g_sanity.sounds < SANITY_SOUNDS_LOGGED) {""")

# ---- the R5 block after the bracket's end ----
R5 = r'''
// ---- R5 sound: player 1 holds the music; the stayer's room keeps its sounds; a track in player 2's room is heard as theirs;
// the copy Tome's sounds are live (rooms/design-r5.md, readers in rooms/sound-readers.md, refuters in refute-r5.md) ----
static bool coop_sound_log(void) {
    static int on = -1;
    if (on < 0)
        on = getenv("ED_COOP_LOG") != NULL;
    return on != 0;
}
#define SOUND_TRACKS 0x805D80C0u      // 80 tracks of 152 bytes: +132 room (s16), +143 mode (0 free), +146 stream-backed, +116 handle
#define SOUND_TRACK_BYTES 152u
#define SOUND_TRACK_FADE 0x8019F360u   // (track, fade, mode): the per-track act 0x801A05EC and fn52's walker use
#define SOUND_GAIN 0x801A01D0u         // (pos, f1 range, min, max, mode) -> u8: a positioned track's gain from the camera eye and the player
#define SOUND_PLAIN 0x8019DB90u        // (sound, volume): a centred effect (the Tome's ticks among them)
#define SOUND_PLAIN_STOP 0x8019DC00u   // (handle)
#define SOUND_STREAM_DROP 0x801A0EACu  // the change's: every playing stream-backed track's slot released, and with them all 4 stream records
#define SOUND_STREAM_STOP 0x801A4B58u  // (id, serial, 1): the stream records of a sound id stopped (reason 9)
#define SOUND_VIEW_BLOCK 0x803012E0u   // the view block the sound code reads the camera eye from (CAMERA_VIEW)
#define MUSIC_SET 0x801A5CB8u          // fn168's target (track, fade)
#define MUSIC_PLAY 0x801A1750u         // fn54's target (stream, 0, 0, 0)
#define MUSIC_STOP 0x801A172Cu         // fn56's target
#define MUSIC_RAMP 0x801A16A0u         // fn58's target (l, r, frames)
#define MUSIC_PAUSE 0x801A1990u        // fn57's target
#define MUSIC_WAIT 0x801A19D0u         // fn59's target (frames): a wait cell that holds the queue (fn62's door sounds use it too, from their own sites)
#define MUSIC_STEP_ON 0x801A661Cu      // fn147(1)'s target
#define MUSIC_STEP_OFF 0x801A65E4u     // fn147(0)'s target
static bool coop_p2_script_is(s32 script);  // (below: a script last run as player 2's)
// Player 1's node: the current one unless player 2 is swapped in, then the one the swap found.
static u32 coop_player1_node(CPUState* cpu) {
    const u32 now = mem_read32(cpu, cpu->gpr[13] - 21492u);
    return now != g_coop_p2 && now != 0u ? now : g_coop_p1_node;
}
// The players are in different rooms, by their own nodes (the room word may be either's at the time).
static bool coop_rooms_apart(CPUState* cpu) {
    const s32 r1 = coop_object_room(cpu, coop_player1_node(cpu)), r2 = coop_object_room(cpu, g_coop_p2);
    return r1 != -1 && r2 != -1 && r1 != r2;
}
// A music act of a script is player 2's when the script runs as theirs: their use or turn (player 2 the player), their change's
// bracket, or a script of theirs resumed after a wait (coop_p2_script_is).
static bool coop_music_theirs(CPUState* cpu) {
    if (!g_settings.mod_coop || g_coop_p2 == 0u || !sda_ready(cpu) || !coop_stay())
        return false;
    if (g_stay_bracket != 0)
        return true;
    if (mem_read32(cpu, cpu->gpr[13] - 21492u) == g_coop_p2 || mem_read32(cpu, cpu->gpr[13] - 21496u) == mem_read32(cpu, g_coop_p2 + 16u))
        return true;
    return coop_p2_script_is((s32)(s16)mem_read32(cpu, cpu->gpr[13] - 23228u));
}
// The music acts of player 2's scripts while the players are APART: not dropped (a music call behind a one-shot flag, L02/2325,
// would be lost for the chapter) but deferred: the last of each kind is kept for the room and replayed, through the game's own
// targets, when player 1 arrives in that room. Together, the music is the room's and player 1 is in it: their calls pass.
static struct {
    s32 room;
    bool set, play, stop;
    u32 track, fade, stream;
} g_music_deferred = {-1, false, false, false, 0u, 0u, 0u};
static void coop_music_act(CPUState* cpu, u32 target, int fn) {
    static bool running;
    static const u32 kSites[] = {0x8016A348u, 0x80169E94u, 0x80169F8Cu, 0x8016A280u, 0x8016A294u, 0x8016A03Cu, 0x8016A13Cu, 0x8016A1F0u};
    bool script = false;
    for (size_t i = 0u; i < sizeof kSites / sizeof kSites[0]; i++)
        script = script || cpu->lr == kSites[i];
    if (running || !script || !coop_music_theirs(cpu)) {
        g_hle_passthrough = true;
        return;
    }
    const s32 room = coop_object_room(cpu, g_coop_p2);
    if (!coop_rooms_apart(cpu)) {
        if (coop_sound_log())
            fprintf(stderr, "[sound] music fn%d (script %d) by player 2 in room %d with player 1: passes (frame %llu)\n", fn,
                    (s32)(s16)mem_read32(cpu, cpu->gpr[13] - 23228u), room, (unsigned long long)(cpu->timebase / 675000ull));
        g_hle_passthrough = true;
        return;
    }
    if (g_music_deferred.room != room)
        memset(&g_music_deferred, 0, sizeof g_music_deferred);
    g_music_deferred.room = room;
    if (target == MUSIC_SET) {
        g_music_deferred.set = true;
        g_music_deferred.track = cpu->gpr[3];
        g_music_deferred.fade = cpu->gpr[4];
    } else if (target == MUSIC_PLAY) {
        g_music_deferred.play = true;
        g_music_deferred.stream = cpu->gpr[3];
    } else if (target == MUSIC_STOP) {
        g_music_deferred.stop = true;
        g_music_deferred.play = false;
        g_music_deferred.set = false;
    }
    if (coop_sound_log())
        fprintf(stderr, "[sound] music fn%d (script %d) by player 2 apart in room %d: deferred, the music is player 1's (frame %llu)\n", fn,
                (s32)(s16)mem_read32(cpu, cpu->gpr[13] - 23228u), room, (unsigned long long)(cpu->timebase / 675000ull));
    cpu->gpr[3] = 0u;
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}
static void coop_music_set(CPUState* cpu) { coop_music_act(cpu, MUSIC_SET, 168); }
static void coop_music_play(CPUState* cpu) { coop_music_act(cpu, MUSIC_PLAY, 54); }
static void coop_music_stop(CPUState* cpu) { coop_music_act(cpu, MUSIC_STOP, 56); }
static void coop_music_ramp(CPUState* cpu) { coop_music_act(cpu, MUSIC_RAMP, 58); }
static void coop_music_pause(CPUState* cpu) { coop_music_act(cpu, MUSIC_PAUSE, 57); }
static void coop_music_wait(CPUState* cpu) { coop_music_act(cpu, MUSIC_WAIT, 59); }
static void coop_music_step_on(CPUState* cpu) { coop_music_act(cpu, MUSIC_STEP_ON, 147); }
static void coop_music_step_off(CPUState* cpu) { coop_music_act(cpu, MUSIC_STEP_OFF, 147); }
// Player 1 arrives in the room whose music player 2's scripts asked for: the game's own targets, as those scripts would have.
static void coop_music_replay(CPUState* cpu, s32 room) {
    if (g_music_deferred.room != room || room == -1)
        return;
    if (g_music_deferred.stop)
        hle_guest_call(cpu, MUSIC_STOP, NULL, 0, 0.0, NULL);
    if (g_music_deferred.play) {
        const u32 args[4] = {g_music_deferred.stream, 0u, 0u, 0u};
        hle_guest_call(cpu, MUSIC_PLAY, args, 4, 0.0, NULL);
    }
    if (g_music_deferred.set) {
        const u32 args[2] = {g_music_deferred.track, g_music_deferred.fade};
        hle_guest_call(cpu, MUSIC_SET, args, 2, 0.0, NULL);
    }
    if (coop_sound_log())
        fprintf(stderr, "[sound] music deferred for room %d replayed as player 1 arrives: stop %d, stream %u (%d), track %u fade %u (%d) (frame %llu)\n", room,
                g_music_deferred.stop, g_music_deferred.stream, g_music_deferred.play, g_music_deferred.track, g_music_deferred.fade, g_music_deferred.set,
                (unsigned long long)(cpu->timebase / 675000ull));
    memset(&g_music_deferred, 0, sizeof g_music_deferred);
    g_music_deferred.room = -1;
}
// The room a fade leaves alone: the other player's, when the stayer is on and both players exist; -1 none.
static s32 coop_sound_keep_room(CPUState* cpu, int mover) {
    if (!coop_stay() || g_coop_p2 == 0u || !sda_ready(cpu))
        return -1;
    return coop_object_room(cpu, mover == 2 ? coop_player1_node(cpu) : g_coop_p2);
}
// The direct stream records (fn126/fn148/fn149 ambience, voice lines) carry no room: each start's sound id is remembered with
// the room in play, so a change can stop the ones of a room nobody is in (what 0x801A0EAC did to all of them).
static struct { u32 id; s32 room; } g_stream_starts[16];
static u32 g_stream_starts_next;
static void coop_stream_started(CPUState* cpu) {
    if (!sda_ready(cpu))
        return;
    g_stream_starts[g_stream_starts_next % 16u].id = cpu->gpr[3];
    g_stream_starts[g_stream_starts_next % 16u].room = (s32)mem_read32(cpu, cpu->gpr[13] - 23276u);
    g_stream_starts_next++;
}
static int coop_streams_stop_except(CPUState* cpu, s32 keep_room) {
    int stopped = 0;
    for (u32 i = 0u; i < 16u; i++) {
        if (g_stream_starts[i].room == 0 && g_stream_starts[i].id == 0u)
            continue;
        if (g_stream_starts[i].room == keep_room)
            continue;
        const u32 args[3] = {g_stream_starts[i].id, 0xFFFFFFFFu, 1u};
        hle_guest_call(cpu, SOUND_STREAM_STOP, args, 3, 0.0, NULL);
        g_stream_starts[i].id = 0u;
        g_stream_starts[i].room = 0;
        stopped++;
    }
    return stopped;
}
// 0x801A05EC(ticks, 1, 0)'s walk, minus the kept room: a playing track of another room fades as the game would fade it (mode
// 4 over ticks; a stream-backed one with a live handle mode 6 over ticks + 10, without a handle stopped at once).
static void coop_sound_fade_rooms(CPUState* cpu, s32 keep_room, u32 ticks, int mover, const char* site) {
    int kept = 0, faded = 0;
    for (u32 i = 0u; i < 80u; i++) {
        const u32 t = SOUND_TRACKS + i * SOUND_TRACK_BYTES;
        if ((mem_read32(cpu, t + 140u) & 0xFFu) == 0u)  // +143: free
            continue;
        const s32 room = (s32)(s16)(mem_read32(cpu, t + 132u) >> 16);
        if (room == keep_room) {
            kept++;
            continue;
        }
        const bool stream = (mem_read32(cpu, t + 144u) & 0xFF00u) != 0u;  // +146
        u32 args[3] = {t, ticks, 4u};
        if (stream) {
            if (mem_read32(cpu, t + 116u) != 0xFFFFFFFFu) {
                args[1] = ticks + 10u < 127u ? ticks + 10u : 127u;
                args[2] = 6u;
            } else {
                args[1] = 0u;
            }
        }
        hle_guest_call(cpu, SOUND_TRACK_FADE, args, 3, 0.0, NULL);
        faded++;
    }
    if (coop_sound_log())
        fprintf(stderr, "[sound] fade at %s: mover %d, room %d kept (%d tracks), %d faded over %u (frame %llu)\n", site, mover, keep_room, kept, faded, ticks,
                (unsigned long long)(cpu->timebase / 675000ull));
}
// Inside tw_keep (the intercept on 0x801A05EC and 0x8019E068): fn66's departure fade (lr 0x801668FC the cut, 0x80166970 the
// fade; r3 = the ticks; the ticket is already written, so g_coop_room_p2 names the mover), the change's fade 0x801A05EC(10,
// 1, 0) (lr 0x80152354), the arrival's group reset 0x8019E068(0|1, 100, 0) (lr 0x8015206C / 0x8015207C) and fn151's duck
// (lr 0x80164DD4: r4 25 ducks, 100 lifts; the groups scale the music too). Answers true when it took the call.
static bool coop_sound_change_act(CPUState* cpu) {
    static bool running;
    if (running || !coop_stay() || g_coop_p2 == 0u || !sda_ready(cpu))
        return false;
    const bool press = cpu->lr == 0x801668FCu || cpu->lr == 0x80166970u, change = cpu->lr == 0x80152354u;
    if (press || change) {
        const int mover = g_coop_room_p2 ? 2 : 1;
        if (change && mover == 2 && g_stay_bracket == 0) {
            if (coop_sound_log())
                fprintf(stderr, "[sound] fade at the change: player 2's change with no bracket: the game's (frame %llu)\n", (unsigned long long)(cpu->timebase / 675000ull));
            return false;
        }
        const s32 keep = coop_sound_keep_room(cpu, mover);
        if (keep == -1)
            return false;
        running = true;
        coop_sound_fade_rooms(cpu, keep, change ? 10u : (cpu->gpr[3] & 0xFFu), mover, press ? "the press" : "the change");
        running = false;
        g_hle_passthrough = false;
        g_hle_tail_call = false;
        return true;
    }
    if (cpu->lr == 0x8015206Cu || cpu->lr == 0x8015207Cu) {
        if (g_stay_bracket == 0) {
            if (cpu->lr == 0x8015206Cu) {  // player 1's arrival: the music their room was asked for while they were away
                running = true;
                coop_music_replay(cpu, (s32)mem_read32(cpu, cpu->gpr[13] - 23276u));
                running = false;
            }
            return false;
        }
        if (coop_sound_log() && cpu->lr == 0x8015206Cu)
            fprintf(stderr, "[sound] the arrival's group reset is player 2's: passed over, player 1's duck stays (frame %llu)\n",
                    (unsigned long long)(cpu->timebase / 675000ull));
        g_hle_passthrough = false;
        g_hle_tail_call = false;
        return true;
    }
    if (cpu->lr == 0x80164DD4u && cpu->gpr[4] == 25u && coop_music_theirs(cpu) && coop_rooms_apart(cpu)) {
        if (coop_sound_log())
            fprintf(stderr, "[sound] a duck (fn151) by player 2 apart: passed over, player 1's music stays (frame %llu)\n",
                    (unsigned long long)(cpu->timebase / 675000ull));
        g_hle_passthrough = false;
        g_hle_tail_call = false;
        return true;
    }
    return false;
}
// 0x801A0EAC (the change's slot drop, one caller): with a stayer, only the direct streams of rooms nobody is in are stopped; the
// mover's old room's stream-backed tracks fade through the walk (mode 6) and their handles stop at the fade's end.
static void coop_sound_stream_drop(CPUState* cpu) {
    static bool running;
    const int mover = g_coop_room_p2 ? 2 : 1;
    if (running || !coop_stay() || g_coop_p2 == 0u || !sda_ready(cpu) || (mover == 2 && g_stay_bracket == 0) || coop_sound_keep_room(cpu, mover) == -1) {
        g_hle_passthrough = true;
        return;
    }
    const s32 keep = coop_sound_keep_room(cpu, mover);
    running = true;
    const int stopped = coop_streams_stop_except(cpu, keep);
    running = false;
    if (coop_sound_log())
        fprintf(stderr, "[sound] the change's stream drop: room %d's streams stay, %d of other rooms stopped (frame %llu)\n", keep, stopped,
                (unsigned long long)(cpu->timebase / 675000ull));
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}
// 0x801A01D0 from the track start (lr 0x8019F230) and the per-tick update (lr 0x801A0310): a track of player 2's room, with
// the players apart, is measured from player 2's eye and player 2's position (the block and r13-26524 theirs for the call).
bool camera_player2_view(float eye[3], float target[3]);
static void coop_sound_gain(CPUState* cpu) {
    static bool running;
    static u32 logged;
    float eye[3], target[3];
    if (running || g_tome_copy_turn || (cpu->lr != 0x8019F230u && cpu->lr != 0x801A0310u) || !coop_stay_apart(cpu) || !camera_player2_view(eye, target)) {
        g_hle_passthrough = true;
        return;
    }
    const u32 t = cpu->gpr[3] - 80u;
    const s32 room = (s32)(s16)(mem_read32(cpu, t + 132u) >> 16);
    if (room != coop_object_room(cpu, g_coop_p2)) {
        g_hle_passthrough = true;
        return;
    }
    u32 block[3], args[4] = {cpu->gpr[3], cpu->gpr[4], cpu->gpr[5], cpu->gpr[6]}, out = 0u;
    const u32 player = mem_read32(cpu, cpu->gpr[13] - 26524u);
    for (int i = 0; i < 3; i++) {
        block[i] = mem_read32(cpu, SOUND_VIEW_BLOCK + 4u * (u32)i);
        guest_write_f32(cpu, SOUND_VIEW_BLOCK + 4u * (u32)i, eye[i]);
    }
    mem_write32(cpu, cpu->gpr[13] - 26524u, mem_read32(cpu, g_coop_p2 + 52u));
    running = true;
    const bool ok = hle_guest_call(cpu, SOUND_GAIN, args, 4, cpu->fpr[1], &out);
    running = false;
    mem_write32(cpu, cpu->gpr[13] - 26524u, player);
    for (int i = 0; i < 3; i++)
        mem_write32(cpu, SOUND_VIEW_BLOCK + 4u * (u32)i, block[i]);
    if (!ok) {
        g_hle_passthrough = true;
        return;
    }
    if (coop_sound_log() && (logged < 8u || logged % 256u == 0u))
        fprintf(stderr, "[sound] gain of track %u (sound %u, room %d) as player 2: %u (frame %llu)\n", (t - SOUND_TRACKS) / SOUND_TRACK_BYTES,
                mem_read32(cpu, t + 132u) & 0xFFFFu, room, out & 0xFFu, (unsigned long long)(cpu->timebase / 675000ull));
    logged++;
    hle_set_u32(cpu, out & 0xFFu);
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}
// The copy Tome's plain sounds (0x8019DB90(sound, volume), 0x8019DC00(handle)) from the Tome's own code: started and stopped in
// the LIVE memory, so a voice exists in the one MusyX state the main loop ticks; the copy keeps the live handle for its stops.
// Not from the sound module's own calls (the music queue's cells, the music step: the copy's tick pops them too, and the real
// tick plays them).
static void tome_copy_sound_live(CPUState* cpu, u32 fn, int count) {
    static bool running;
    static u32 logged;
    if (!g_tome_copy_turn || running || cpu->ram != g_tome_copy.copy || g_tome_copy.real == NULL || cpu->lr < 0x80100000u || cpu->lr >= 0x80117000u) {
        g_hle_passthrough = true;
        return;
    }
    u32 args[2] = {cpu->gpr[3], cpu->gpr[4]}, out = 0u;
    cpu->ram = g_tome_copy.real;
    running = true;
    const bool ok = hle_guest_call(cpu, fn, args, count, 0.0, &out);
    running = false;
    cpu->ram = g_tome_copy.copy;
    if (!ok) {
        g_hle_passthrough = true;
        return;
    }
    if (coop_sound_log() && logged < 12u)
        fprintf(stderr, "[sound] tome copy: %s %u (at %u) live from lr 0x%08X (frame %llu)\n", fn == SOUND_PLAIN ? "sound" : "stop of", args[0], args[1], cpu->lr,
                (unsigned long long)(cpu->timebase / 675000ull));
    logged++;
    hle_set_u32(cpu, out);
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}
static void tome_copy_sound(CPUState* cpu) { tome_copy_sound_live(cpu, SOUND_PLAIN, 2); }
static void tome_copy_sound_stop(CPUState* cpu) { tome_copy_sound_live(cpu, SOUND_PLAIN_STOP, 1); }
// At the copy's close (cpu->ram is the real memory): the handles its turn left running, stopped; the close sound.
static void tome_copy_sound_close(CPUState* cpu, const u32 copy_handles[3]) {
    static const u32 kWords[3] = {24480u, 29056u, 24108u};
    if (hle_dispatch_entry(SOUND_PLAIN) == NULL || hle_dispatch_entry(SOUND_PLAIN)->intercept != tome_copy_sound)
        return;
    for (int i = 0; i < 3; i++) {
        const u32 real_handle = mem_read32(cpu, cpu->gpr[13] - kWords[i]);
        if (copy_handles[i] != 0u && copy_handles[i] != real_handle) {
            const u32 arg = copy_handles[i];
            hle_guest_call(cpu, SOUND_PLAIN_STOP, &arg, 1, 0.0, NULL);
            if (coop_sound_log())
                fprintf(stderr, "[sound] tome copy closes: its handle %u (r13-%u) stopped live (frame %llu)\n", arg, kWords[i], (unsigned long long)(cpu->timebase / 675000ull));
        }
    }
    const u32 close[2] = {569u, 100u};
    hle_guest_call(cpu, SOUND_PLAIN, close, 2, 0.0, NULL);
}
// Every 120 frames while apart: the playing tracks per room (the [stay] cadence), so a run shows the stayer's room kept its sounds.
static void coop_sound_rooms_log(CPUState* cpu) {
    int n[2] = {0, 0}, other = 0;
    const s32 r1 = (s32)mem_read32(cpu, cpu->gpr[13] - 23276u), r2 = coop_object_room(cpu, g_coop_p2);
    for (u32 i = 0u; i < 80u; i++) {
        const u32 t = SOUND_TRACKS + i * SOUND_TRACK_BYTES;
        if ((mem_read32(cpu, t + 140u) & 0xFFu) == 0u)
            continue;
        const s32 room = (s32)(s16)(mem_read32(cpu, t + 132u) >> 16);
        if (room == r1)
            n[0]++;
        else if (room == r2)
            n[1]++;
        else
            other++;
    }
    fprintf(stderr, "[sound] tracks playing: room %d (player 1's) %d, room %d (player 2's) %d, other rooms %d, free %d of 80 (frame %llu)\n", r1, n[0], r2, n[1],
            other, 80 - n[0] - n[1] - other, (unsigned long long)(cpu->timebase / 675000ull));
}
'''
s = rep(s, """static void notify_behaviour_pass(CPUState* cpu) {
    if (cpu->lr == 0x80008400u || cpu->lr == 0x80050A9Cu)
        coop_stay_change_end(cpu);
}
""", """static void notify_behaviour_pass(CPUState* cpu) {
    if (cpu->lr == 0x80008400u || cpu->lr == 0x80050A9Cu)
        coop_stay_change_end(cpu);
}
""" + R5)

# ---- the [stay] cadence also counts the tracks ----
s = rep(s, """    if (coop_stay_apart(cpu) && frame >= g_stay_log + 120ull) {
        const u32 o2 = mem_read32(cpu, g_coop_p2 + 52u);
        g_stay_log = frame;
""", """    if (coop_stay_apart(cpu) && frame >= g_stay_log + 120ull) {
        const u32 o2 = mem_read32(cpu, g_coop_p2 + 52u);
        g_stay_log = frame;
        coop_sound_rooms_log(cpu);
""")
s = rep(s, "static bool coop_stay_apart(CPUState* cpu);  // (ED_COOP_STAY: player 2 in another resident room)\n",
        "static bool coop_stay_apart(CPUState* cpu);  // (ED_COOP_STAY: player 2 in another resident room)\nstatic void coop_sound_rooms_log(CPUState* cpu);  // (R5)\n")

# ---- install: the stay's hooks ----
s = rep(s, """                            walk->intercept = coop_stay_strip_walk;
                            kind->intercept = coop_stay_kind_ask;
                            ticket_run->notify = notify_ticket_run;  // R4: a change of player 2's runs as theirs
                            pass->notify = notify_behaviour_pass;
                            fprintf(stderr, "[stay] the other player stays in their room (ED_COOP_STAY)\\n");
""", """                            walk->intercept = coop_stay_strip_walk;
                            kind->intercept = coop_stay_kind_ask;
                            ticket_run->notify = notify_ticket_run;  // R4: a change of player 2's runs as theirs
                            pass->notify = notify_behaviour_pass;
                            fprintf(stderr, "[stay] the other player stays in their room (ED_COOP_STAY)\\n");
                            // R5: the music is player 1's (the script primitives' targets, by their wrappers' call sites), the
                            // change's stream drop and the gain per track's room; the fades, the group reset and fn151's duck go
                            // through tw_keep (installed below, with the Tome).
                            {
                                const struct { u32 address; u32 first; HleHandler fn; const char* what; } kSound[] = {
                                    {MUSIC_SET, 0x9421FFF0u, coop_music_set, "fn168"},     {MUSIC_PLAY, 0x9421FFE0u, coop_music_play, "fn54"},
                                    {MUSIC_STOP, 0x9421FFF0u, coop_music_stop, "fn56"},    {MUSIC_RAMP, 0x9421FFE0u, coop_music_ramp, "fn58"},
                                    {MUSIC_PAUSE, 0x9421FFF0u, coop_music_pause, "fn57"},  {MUSIC_WAIT, 0x9421FFF0u, coop_music_wait, "fn59"},
                                    {MUSIC_STEP_ON, 0x9421FFF0u, coop_music_step_on, "fn147(1)"}, {MUSIC_STEP_OFF, 0x2C030001u, coop_music_step_off, "fn147(0)"},
                                    {SOUND_STREAM_DROP, 0x9421FFF0u, coop_sound_stream_drop, "the change's stream drop"},
                                    {SOUND_GAIN, 0x9421FFA0u, coop_sound_gain, "the gain"}};
                                for (size_t i = 0u; i < sizeof kSound / sizeof kSound[0]; i++) {
                                    HleDispatchEntry* e = hle_dispatch_entry(kSound[i].address);
                                    if (e != NULL && e->intercept == NULL && mem_read32(cpu, kSound[i].address) == kSound[i].first)
                                        e->intercept = kSound[i].fn;
                                    else
                                        fprintf(stderr, "[sound] NOT installed: %s at 0x%08X (first word 0x%08X)\\n", kSound[i].what, kSound[i].address,
                                                mem_read32(cpu, kSound[i].address));
                                }
                            }
""")
# ---- install: the copy Tome's sounds, beside tome_copy_tab ----
s = rep(s, """                if (to_journal != NULL && to_journal->intercept == NULL)
                    to_journal->intercept = tome_copy_tab;
            }
""", """                if (to_journal != NULL && to_journal->intercept == NULL)
                    to_journal->intercept = tome_copy_tab;
                // R5: the copy's plain sounds live (its navigation ticks were started in the copied MusyX state and thrown away).
                HleDispatchEntry* plain = hle_dispatch_entry(SOUND_PLAIN);
                HleDispatchEntry* plain_stop = hle_dispatch_entry(SOUND_PLAIN_STOP);
                if (plain != NULL && plain->intercept == NULL && plain_stop != NULL && plain_stop->intercept == NULL &&
                    mem_read32(cpu, SOUND_PLAIN) == 0x9421FFE0u && mem_read32(cpu, SOUND_PLAIN_STOP) == 0x9421FFF0u) {
                    plain->intercept = tome_copy_sound;
                    plain_stop->intercept = tome_copy_sound_stop;
                } else {
                    fprintf(stderr, "[sound] the copy Tome's sounds NOT forwarded: 0x%08X / 0x%08X taken or not as expected\\n", SOUND_PLAIN, SOUND_PLAIN_STOP);
                }
            }
""")
save(root + "runtime/host/hle.c", s)

# ---- the kit ----
p = root + "tools/ed/run.sh"
s = load(p)
s = rep(s, "ED_BOTH=\"$R/both.tsv\" ED_DETAIL_LOG=1 ED_ROOM_LOG=1 ED_COOP_LOG=1", "ED_BOTH=\"$R/both.tsv\" ED_DETAIL_LOG=1 ED_ROOM_LOG=1 ED_COOP_LOG=1 ED_DTK_LOG=1")
save(p, s)
p = root + "tools/coop-test/pagelog.py"
s = load(p)
s = rep(s, "KEY = re.compile(r'^\\[(room\\] (ticket|change)|", "KEY = re.compile(r'^\\[(sound\\]|dtk\\]|stay\\]|room\\] (ticket|change)|")
save(p, s)
p = root + "tools/ed/coopjudge.py"
s = load(p)
s = rep(s, """            if 'p2room' in r30:
                v.check('same room', r30['p1room'] == r30['p2room'], 'both players\\' own rooms %d and %d' % (r30['p1room'], r30['p2room']))
""", """            if 'p2room' in r30 and not plan.get('two_rooms'):  # (with the stayer on, the other player keeps their room)
                v.check('same room', r30['p1room'] == r30['p2room'], 'both players\\' own rooms %d and %d' % (r30['p1room'], r30['p2room']))
    if plan.get('two_rooms'):
        # R5 sound: a door never touches the disc music, and the stayer's room keeps its tracks (the host's [sound] lines).
        dtk = lines_in(log, s, tc + 120, r'\\[dtk\\] (prepare|cancel)')
        v.check('music untouched', not dtk, 'the disc music changed at a door (%s)' % (dtk[0][1].strip() if dtk else ''))
        fades = lines_in(log, s, tc + 120, r'\\[sound\\] fade at .*: mover (\\d), room (-?\\d+) kept \\((\\d+) tracks\\), (\\d+) faded')
        v.check('door fade by owner', bool(fades), 'the door\\'s fades were walked by the host (%d lines)' % len(fades))
        if fades:
            kept = max(int(m.group(3)) for f, l, m in fades)
            v.info('stayer room sounds', 'room %s kept %d playing tracks through the door' % (fades[0][2].group(2), kept))
""")
save(p, s)
print("patched r5")
