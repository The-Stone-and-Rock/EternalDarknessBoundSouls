// High-level emulation (HLE) dispatch.
//
// The recompiled game runs natively, but GameCube SDK functions that touch
// hardware (GX/VI/PAD/SI/AI/DSP/DI/EXI) or the OS kernel cannot. Those are
// intercepted at their guest addresses. `hle_install` resolves the generated
// SDK names once and builds an address-indexed table; `host_call` then performs
// one constant-time lookup per dispatched block.
//
// Functions in the table without a handler fall through to their recompiled
// implementation (true for pure-computation SDK routines that run fine
// natively); set ED_HLE_LOG=1 to trace which SDK functions the game
// calls and whether each is handled.
#include "hle.h"

#include "gxruntime/aram.h"
#include "gxruntime/memory_card.h"
#include "host/audio.h"
#include "gxruntime/dvd.h"
#include "gxruntime/platform.h"
#include "host/hle_abi.h"
#include "host/interrupt.h"
#include "host/mmio.h"
#include "host/quickstate.h"
#include "host/sdk_map.h"
#include "host/compat.h"
#include "host/settings.h"
#ifdef ED_AURORA
#include "gxruntime/aurora_backend.h"
#include "host/camera.h"
#include "host/controls.h"
#include "host/frontend.h"
#include "host/report.h"
// Aurora (aurora/gfx.h): a movie is playing (HDR shows it at video white).
// Declared here: the Mac app and the Linux package build this file on the
// player's machine with only the host and GXRuntime headers.
void aurora_hdr_set_movie(bool playing);
#endif
#include "game_data.h"
#include "host/dtk.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef void (*HleHandler)(CPUState* cpu);

typedef struct {
    const char* name;
    HleHandler  fn;
} HleEntry;

typedef struct {
    HleHandler intercept;
    HleHandler notify;
} HleDispatchEntry;

// Trace every intercepted SDK call (and DVD activity). Set by ED_HLE_LOG.
static bool g_hle_log = false;
static bool g_dvd_log = false;  // ED_DVD_LOG: file opens only
static u32 g_trace_fn[16];
static u32 g_trace_fn_count;
// ED_CALL_PROFILE=name: count calls of one function by caller (lr) and print
// the counts once per VIFlush, so per-frame differences show without the
// slowdown of logging every call.
#ifdef ED_AURORA
void VISetWindowTitle(const char* title);  // Aurora (dolphin/vi.h)
#endif
static u32 g_call_prof_fn;
static u32 g_call_prof_vi;
static struct { u32 lr, n; } g_call_prof[64];
static bool g_audio_log = false;
static bool g_graphics_log = false;
static bool g_movie_log = false;
static bool g_movie_cadence_log = false;
static bool g_card_log = false;
static bool g_auto_input = false;
static bool g_mash_to_gameplay = false;
static bool g_mash_side_assigned = false;
static bool g_mash_prematch = false;
static bool g_mash_route_complete = false;
static bool g_auto_skip_card_prompt = false;
static bool g_state_log = false;
static u64 g_auto_input_last_pulse = ~(u64)0;
static u64 g_auto_input_once_frame = 0;
static u64 g_auto_input_once_sent = ~(u64)0;
static u64 g_auto_skip_card_last_pulse = ~(u64)0;
typedef struct {
    u64 start_frame;
    u64 end_frame; // exclusive
    u16 buttons;
    s8 stick_x;
    s8 stick_y;
    u8 analog_a;
} InputScriptEvent;
#define INPUT_SCRIPT_MAX_EVENTS 256u
#define INPUT_SCRIPT_MAX_TEXT   4096u
static InputScriptEvent g_input_script[INPUT_SCRIPT_MAX_EVENTS];
static unsigned g_input_script_count = 0;
static u64 g_input_script_last_log_frame = ~(u64)0;
static u16 g_input_script_last_buttons = 0;
static s8 g_input_script_last_stick_x = 0;
static s8 g_input_script_last_stick_y = 0;
static u8 g_input_script_last_analog_a = 0;
static unsigned g_movie_texobj_log_count = 0;
// Address handlers normally return to the intercepted function's caller.
// Synchronous host operations with a guest completion callback can instead
// tail-call that callback and let its normal `blr` return to the same caller.
static bool g_hle_tail_call = false;
// Set by an intercept that declines the call: the recompiled function runs.
static bool g_hle_passthrough = false;

static char* input_script_trim(char* text) {
    while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r')
        ++text;
    char* end = text + strlen(text);
    while (end > text) {
        char c = end[-1];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
            break;
        *--end = '\0';
    }
    return text;
}

static void parse_input_script(const char* text) {
    g_input_script_count = 0;
    g_input_script_last_log_frame = ~(u64)0;
    g_input_script_last_buttons = 0;
    g_input_script_last_stick_x = 0;
    g_input_script_last_stick_y = 0;
    g_input_script_last_analog_a = 0;
    if (text == NULL || text[0] == '\0')
        return;
    char buffer[INPUT_SCRIPT_MAX_TEXT];
    strncpy(buffer, text, sizeof buffer - 1u);
    buffer[sizeof buffer - 1u] = '\0';
    unsigned skipped_events = 0;
    for (char* event = buffer; event != NULL && *event != '\0';) {
        char* next_event = strchr(event, ';');
        if (next_event != NULL) {
            *next_event = '\0';
            ++next_event;
        }
        event = input_script_trim(event);
        if (*event == '\0') {
            event = next_event;
            continue;
        }
        char* fields[6] = {0};
        unsigned field_count = 0;
        bool too_many_fields = false;
        for (char* field = event; field != NULL;) {
            char* next_field = strchr(field, ',');
            if (next_field != NULL) {
                *next_field = '\0';
                ++next_field;
            }
            if (field_count < 6u) {
                fields[field_count++] = input_script_trim(field);
            } else {
                too_many_fields = true;
            }
            field = next_field;
        }
        if (field_count < 3u || too_many_fields ||
            g_input_script_count >= INPUT_SCRIPT_MAX_EVENTS) {
            ++skipped_events;
            event = next_event;
            continue;
        }
        InputScriptEvent parsed;
        memset(&parsed, 0, sizeof parsed);
        parsed.start_frame = strtoull(fields[0], NULL, 0);
        parsed.end_frame = strtoull(fields[1], NULL, 0);
        parsed.buttons = (u16)strtoul(fields[2], NULL, 0);
        parsed.stick_x = field_count >= 4u ? (s8)strtol(fields[3], NULL, 0) : 0;
        parsed.stick_y = field_count >= 5u ? (s8)strtol(fields[4], NULL, 0) : 0;
        parsed.analog_a =
            field_count >= 6u ? (u8)strtoul(fields[5], NULL, 0)
                              : ((parsed.buttons & 0x0100u) ? 0xFFu : 0u);
        if (parsed.end_frame <= parsed.start_frame)
            parsed.end_frame = parsed.start_frame + 1u;
        if (parsed.buttons == 0u && parsed.stick_x == 0 && parsed.stick_y == 0 &&
            parsed.analog_a == 0u) {
            ++skipped_events;
            event = next_event;
            continue;
        }
        g_input_script[g_input_script_count++] = parsed;
        event = next_event;
    }
    fprintf(stderr, "[input] script events=%u skipped=%u\n",
            g_input_script_count, skipped_events);
}

// ED_SKIP_INTRO=1 (testing): tap A every 16 frames to skip the intro movies,
// and stop once they are over, leaving the pad to the player for the language
// and memory-card prompts and the title. Two ways the intro ends: fmv0001 plays
// and a non-movie file opens after it, or A on the Poe quote skips the movies
// and the title loads /UBookInv.tpl after the second /UMemcardText.bin open
// (boot opens both once before that).
static bool g_skip_intro;
static bool g_skip_intro_done;
static bool g_skip_intro_armed;
static unsigned g_skip_intro_memcard_text;
static u64 g_skip_intro_start_frame;  // first frame it ran (0: not yet)
// ED_TITLE_STATE=path (the launcher's Skip intro, first time): the machine
// saved there once the title menu is up, for the next Start game to boot
// straight into (frontend.cpp start_game).
static u64 g_title_save_at;

// The launcher's New game in a running process (frontend.cpp): skip the
// intro movies or not, as at boot (the boot state it loads resets the rest).
void hle_set_skip_intro(bool on) {
    g_skip_intro = on;
    g_skip_intro_done = false;
    g_skip_intro_armed = false;
    g_skip_intro_memcard_text = 0;
    g_skip_intro_start_frame = 0;
}

// The start-up hidden (frontend.cpp: a black screen, no sound) while it is
// passed over for the launcher's Skip intro, until the title menu is saved.
bool hle_start_up_hidden(void) {
    return getenv("ED_TITLE_STATE") != NULL &&
           ((g_skip_intro && !g_skip_intro_done) || g_title_save_at != 0u);
}

static void skip_intro_note_open(const CPUState* cpu, const char* path) {
    if (!g_skip_intro || g_skip_intro_done)
        return;
    if (strstr(path, "/UMemcardText.bin"))
        g_skip_intro_memcard_text++;
    if (strstr(path, "/fmv/fmv0001."))
        g_skip_intro_armed = true;
    else if ((g_skip_intro_armed && !strstr(path, "/fmv/")) ||
             (g_skip_intro_memcard_text >= 2u &&
              strstr(path, "/UBookInv.tpl"))) {
        g_skip_intro_done = true;
        fprintf(stderr,
                "[input] opening movie over (%s) at frame %llu; skip-intro off\n",
                path, (unsigned long long)(cpu->timebase / 675000ull));
        // Two seconds on, the menu drawn and still.
        if (getenv("ED_TITLE_STATE") != NULL)
            g_title_save_at = cpu->timebase / 675000ull + 120u;
    }
}

// The intro's end is spotted from the files it opens, and a New game from the
// launcher does not always open them in that order: A was then tapped on
// through play (the gun fired over and over, Steam Deck 2026-10-16). So the
// player taking over (a button, or the main stick pushed) also ends it, and
// so does a minute and a half of tapping.
static bool skip_intro_apply(CPUState* cpu, u16* buttons, s8 stick_x, s8 stick_y,
                             u8* analog_a) {
    if (g_title_save_at != 0u && cpu->timebase / 675000ull >= g_title_save_at) {
        g_title_save_at = 0u;
        quickstate_request_save(getenv("ED_TITLE_STATE"));
        fprintf(stderr, "[input] title menu saved to %s\n", getenv("ED_TITLE_STATE"));
    }
    if (!g_skip_intro || g_skip_intro_done)
        return false;
    const u64 frame = cpu->timebase / 675000ull;
    if (g_skip_intro_start_frame == 0u || frame < g_skip_intro_start_frame)
        g_skip_intro_start_frame = frame != 0u ? frame : 1u;
    const u64 elapsed = frame - g_skip_intro_start_frame;
    const char* why = NULL;
    // Ignore the first frames: the launcher's A may still be held.
    if (elapsed > 20u && (*buttons != 0u || abs(stick_x) > 48 || abs(stick_y) > 48))
        why = "the player took over";
    else if (elapsed > 90u * 60u)
        why = "90 s";
    if (why != NULL) {
        g_skip_intro_done = true;
        fprintf(stderr, "[input] skip-intro off at frame %llu (%s)\n",
                (unsigned long long)frame, why);
        return false;
    }
    // A held two frames in every four: each screen ends almost as it opens.
    if (frame < 30u || (frame % 4u) >= 2u)
        return false;
    *buttons |= 0x0100u;
    *analog_a = 0xFFu;
    return true;
}

static bool input_script_apply(CPUState* cpu,
                               u16* buttons,
                               s8* stick_x,
                               s8* stick_y,
                               u8* analog_a) {
    if (g_input_script_count == 0u)
        return false;
    const u64 frame = cpu->timebase / 675000ull;
    bool active = false;
    for (unsigned i = 0; i < g_input_script_count; ++i) {
        const InputScriptEvent* event = &g_input_script[i];
        if (frame < event->start_frame || frame >= event->end_frame)
            continue;
        *buttons |= event->buttons;
        if (event->stick_x != 0)
            *stick_x = event->stick_x;
        if (event->stick_y != 0)
            *stick_y = event->stick_y;
        if (event->analog_a != 0u)
            *analog_a = event->analog_a;
        active = true;
    }
    if (active) {
        const bool continuation =
            g_input_script_last_log_frame + 1u == frame &&
            g_input_script_last_buttons == *buttons &&
            g_input_script_last_stick_x == *stick_x &&
            g_input_script_last_stick_y == *stick_y &&
            g_input_script_last_analog_a == *analog_a;
        g_input_script_last_log_frame = frame;
        g_input_script_last_buttons = *buttons;
        g_input_script_last_stick_x = *stick_x;
        g_input_script_last_stick_y = *stick_y;
        g_input_script_last_analog_a = *analog_a;
        if (continuation)
            return true;
        fprintf(stderr,
                "[input] script frame=%llu buttons=0x%04X stick=%d,%d analog_a=%u\n",
                (unsigned long long)frame, *buttons, *stick_x, *stick_y,
                (unsigned)*analog_a);
    }
    return active;
}

// Repeated edges are intentional: each front-end screen may begin polling at
// a different time, and most of them consume JustPressed rather than Held.
static bool mash_to_gameplay_apply(CPUState* cpu,
                                   u16* buttons,
                                   s8* stick_x,
                                   u8* analog_a) {
    if (!g_mash_to_gameplay || g_mash_route_complete)
        return false;
    // TEMP DIAG (G013): once prematch is reached, go neutral instead of mashing
    // A so the in-engine prematch camera pan cutscene actually PLAYS (mashing A
    // skips it). Lets a live run observe the cutscene's gxcore gap counters.
    if (g_mash_prematch) {
        static int s_cutscene_diag = -1;
        if (s_cutscene_diag < 0)
            s_cutscene_diag = getenv("ED_CUTSCENE_DIAG") != NULL ? 1 : 0;
        if (s_cutscene_diag)
            return false;
    }
    const u64 frame = cpu->timebase / 675000ull;
    if ((frame % 16u) >= 8u)
        return false;
    *buttons |= 0x0100u; // PAD_BUTTON_A
    if (!g_mash_prematch)
        *buttons |= 0x1000u; // PAD_BUTTON_START
    if (!g_mash_side_assigned) {
        *buttons |= 0x0001u; // PAD_BUTTON_LEFT
        *stick_x = -127;
    }
    *analog_a = 0xFFu;
    return true;
}

#ifdef ED_AURORA
#define HLE_DISPLAY_LIST_RETURN 0x7FFF0010u
#define HLE_GX_BEGIN_RETURN     0x7FFF0020u
#define HLE_BOOK_RETURN         0x7FFF0030u
#define HLE_CHARACTER_RETURN    0x7FFF0040u
#define HLE_PACK_RETURN         0x7FFF0050u
#define HLE_SPAWN_RETURN        0x7FFF0060u
#define HLE_TRACKS_RETURN       0x7FFF0064u
#define HLE_USE_RETURN          0x7FFF0070u
static bool continue_character_load(CPUState* cpu);
static bool continue_pack_load(CPUState* cpu);
static bool continue_spawn_create(CPUState* cpu);
static bool continue_tracks_load(CPUState* cpu);
static bool continue_coop_use(CPUState* cpu);
static u32 g_gx_dirty_state_helper;  // __GXSetDirtyState
static u32 g_gx_flush_prim_helper;   // __GXSendFlushPrim
#define GX_DATA_SDA_OFFSET      ED_GX_DATA_SDA_OFFSET
#define GX_DIRTY_STATE_OFFSET   ED_GX_DIRTY_STATE_OFFSET

typedef struct {
    bool active;
    u8 stage;
    u32 caller;
    u32 address;
    u32 size;
} HleDisplayListCall;

static HleDisplayListCall g_display_list_call;

typedef struct {
    bool active;
    u8 stage;
    u8 primitive;
    u8 vtxfmt;
    u16 vertex_count;
    u32 caller;
} HleGxBegin;

static HleGxBegin g_gx_begin;
#endif

// CARD API completions are asynchronous on GameCube. Performing host storage
// synchronously is fine, but invoking a guest callback recursively from the
// intercepted API is not: Strikers checks the API's immediate return before
// the callback is allowed to advance its state machine. Queue completions and
// run them on the next dispatch with an interrupt-style saved register context.
#define HLE_CALLBACK_RETURN 0x7FFF0000u
#define HLE_CALLBACK_QUEUE_CAPACITY 32u

typedef struct {
    u32 address;
    s32 channel;
    s32 result;
} HlePendingCallback;

typedef struct {
    u32 gpr[32];
    f64 fpr[32];
    f64 ps1[32];
    u32 pc;
    u32 lr;
    u32 ctr;
    u32 cr;
    u32 xer;
    u32 fpscr;
    u32 msr;
    u32 srr0;
    u32 srr1;
    u32 dar;
    u32 dsisr;
    u32 ear;
    u32 hid2;
    u32 sr[16];
    u32 gqr[8];
    u32 exception;
    u32 program_exception;
    u32 reserve_addr;
    bool reserve_valid;
} HleSavedContext;

static HlePendingCallback g_callback_queue[HLE_CALLBACK_QUEUE_CAPACITY];
static u32 g_callback_read;
static u32 g_callback_count;
static bool g_callback_active;
static HleSavedContext g_callback_context;
static DolMemoryCard* g_memory_card;
static DolMemoryCard* g_memory_card_b;  // slot B, when the executable gives it a card

// A host DVD read is done the moment it is asked for, but its completion
// callback waits until the thread that asked has moved past the call, as it
// always has on a drive. Eternal Darkness's disc streams (voices, effects) call
// DVDReadAsyncPrio and only then mark the read in flight; the callback clears
// the mark. Run on the call's return, before that store, it left the mark set
// for good: the stream never read again (its buffer replayed, a door's sound
// repeating) and never closed (picking up a book waited for it forever).
// Due once that thread has run DVD_CALLBACK_VISITS dispatch-loop visits of its
// own, or sleeps (it got to its wait). A read asked for with interrupts off is
// due at once: nothing lands before they are back on. (A fixed 2 ms instead
// held the chained 4 KB reads of the game's loader to 8 a frame.) A thread
// switched out in the window (a retrace waking main, which outranks the stream
// feeder) is ready but not past its store, and on the Steam Deck heavy scenes
// keep it off the CPU for over a second; so the give-up only breaks a deadlock
// (a higher-priority thread spinning on that callback) and is long. Not in the
// quick state: after a load every queued callback waits for the thread running
// then (hle_state_loaded). Callbacks stay in order, as a drive completes them.
#define DVD_CALLBACK_VISITS 64u
#define DVD_CALLBACK_GIVE_UP_NS 1000000000ull
#define DVD_CALLBACK_GIVE_UP_VISITS (1ull << 27)  // block-paced: ~1 s of 2M-block frames
#define OS_CURRENT_THREAD 0x800000E4u
#define OS_THREAD_STATE 0x2C8u                     // u16: 1 ready, 2 running
typedef struct {
    u32 thread;  // OSThread* that asked, 0 = due at once
    u32 visits;  // its own dispatch-loop visits since
    u64 queued_ns;
    u64 queued_visit;
} HleCallbackWait;
static HleCallbackWait g_callback_wait[HLE_CALLBACK_QUEUE_CAPACITY];
static u64 g_callback_visits;

static u64 host_ns(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (u64)now.tv_sec * 1000000000ull + (u64)now.tv_nsec;
}

static void delay_callback(CPUState* cpu, u32 index) {
    HleCallbackWait* w = &g_callback_wait[index];
    w->thread = (cpu->msr & 0x8000u) ? mem_read32(cpu, OS_CURRENT_THREAD) : 0u;
    w->visits = 0;
    w->queued_ns = host_ns();
    w->queued_visit = g_callback_visits;
}

static bool callback_due(CPUState* cpu, const HleCallbackWait* w) {
    if (w->thread == 0u || w->visits >= DVD_CALLBACK_VISITS)
        return true;
    const u16 state = mem_read16(cpu, w->thread + OS_THREAD_STATE);
    if (state != 1u && state != 2u)
        return true;  // waiting (or gone): past the call
    return interrupt_wall_clock()
               ? host_ns() - w->queued_ns >= DVD_CALLBACK_GIVE_UP_NS
               : g_callback_visits - w->queued_visit >= DVD_CALLBACK_GIVE_UP_VISITS;
}

static void save_callback_context(HleSavedContext* saved,
                                  const CPUState* cpu) {
    memcpy(saved->gpr, cpu->gpr, sizeof saved->gpr);
    memcpy(saved->fpr, cpu->fpr, sizeof saved->fpr);
    memcpy(saved->ps1, cpu->ps1, sizeof saved->ps1);
    saved->pc = cpu->pc;
    saved->lr = cpu->lr;
    saved->ctr = cpu->ctr;
    saved->cr = cpu->cr;
    saved->xer = cpu->xer;
    saved->fpscr = cpu->fpscr;
    saved->msr = cpu->msr;
    saved->srr0 = cpu->srr0;
    saved->srr1 = cpu->srr1;
    saved->dar = cpu->dar;
    saved->dsisr = cpu->dsisr;
    saved->ear = cpu->ear;
    saved->hid2 = cpu->hid2;
    memcpy(saved->sr, cpu->sr, sizeof saved->sr);
    memcpy(saved->gqr, cpu->gqr, sizeof saved->gqr);
    saved->exception = cpu->exception;
    saved->program_exception = cpu->program_exception;
    saved->reserve_addr = cpu->reserve_addr;
    saved->reserve_valid = cpu->reserve_valid;
}

static void restore_callback_context(CPUState* cpu,
                                     const HleSavedContext* saved) {
    memcpy(cpu->gpr, saved->gpr, sizeof saved->gpr);
    memcpy(cpu->fpr, saved->fpr, sizeof saved->fpr);
    memcpy(cpu->ps1, saved->ps1, sizeof saved->ps1);
    cpu->pc = saved->pc;
    cpu->lr = saved->lr;
    cpu->ctr = saved->ctr;
    cpu->cr = saved->cr;
    cpu->xer = saved->xer;
    cpu->fpscr = saved->fpscr;
    cpu->msr = saved->msr;
    cpu->srr0 = saved->srr0;
    cpu->srr1 = saved->srr1;
    cpu->dar = saved->dar;
    cpu->dsisr = saved->dsisr;
    cpu->ear = saved->ear;
    cpu->hid2 = saved->hid2;
    memcpy(cpu->sr, saved->sr, sizeof saved->sr);
    memcpy(cpu->gqr, saved->gqr, sizeof saved->gqr);
    cpu->exception = saved->exception;
    cpu->program_exception = saved->program_exception;
    cpu->reserve_addr = saved->reserve_addr;
    cpu->reserve_valid = saved->reserve_valid;
}

// What's done for player 2's Tome in its copy of the memory finishes there, before its turn
// ends: run by the loop, later, its callback would land in the real memory.
static bool g_tome_copy_turn;
static HlePendingCallback g_tome_copy_callbacks[HLE_CALLBACK_QUEUE_CAPACITY];
static u32 g_tome_copy_callback_count;

static bool queue_guest_callback_after(CPUState* cpu, u32 address, s32 channel,
                                       s32 result) {
    if (address == 0)
        return true;
    if (g_tome_copy_turn) {
        if (g_tome_copy_callback_count == HLE_CALLBACK_QUEUE_CAPACITY) {
            fprintf(stderr, "[tome-copy] too many callbacks in its turn\n");
            return false;
        }
        const HlePendingCallback callback = {address, channel, result};
        g_tome_copy_callbacks[g_tome_copy_callback_count++] = callback;
        return true;
    }
    if (g_callback_count == HLE_CALLBACK_QUEUE_CAPACITY) {
        fprintf(stderr, "[card] completion callback queue overflow\n");
        return false;
    }
    u32 index =
        (g_callback_read + g_callback_count) % HLE_CALLBACK_QUEUE_CAPACITY;
    g_callback_queue[index].address = address;
    g_callback_queue[index].channel = channel;
    g_callback_queue[index].result = result;
    g_callback_wait[index].thread = 0u;
    if (cpu != NULL)
        delay_callback(cpu, index);
    g_callback_count++;
    if (g_card_log)
        fprintf(stderr,
                "[card] queued callback=0x%08X channel=%d result=%d depth=%u\n",
                address, channel, result, g_callback_count);
    return true;
}

static bool queue_guest_callback(u32 address, s32 channel, s32 result) {
    return queue_guest_callback_after(NULL, address, channel, result);
}

// A completion callback runs the way the SDK's interrupt dispatcher runs device
// handlers (__OSDispatchInterrupt): from an exception context with the scheduler
// held off, then OSEnableScheduler, __OSReschedule and OSLoadContext. A callback
// that wakes a thread (OSSendMessage from a DVD completion, say) therefore
// switches threads only once it has finished, never from inside the call.
// When __OSReschedule does switch, the frame is gone for good, as on the
// hardware: SelectThread does not save an exception context (its thread later
// resumes where the callback interrupted it), and from the idle loop there is
// no thread to save. So the callback is over once __OSReschedule starts; the
// stage alone recognises a return from it. Left waiting for that return, every
// later callback queued behind it (a DVD read finishing while the game idled
// woke the main thread, and the book's movie never got its next read).
static struct {
    u32 stage;
    u32 context;
    HlePendingCallback pending;
} g_irq_callback;

static void continue_irq_callback(CPUState* cpu) {
    switch (g_irq_callback.stage++) {
    case 1:
        cpu->gpr[3] = (u32)g_irq_callback.pending.channel;
        cpu->gpr[4] = (u32)g_irq_callback.pending.result;
        cpu->pc = g_irq_callback.pending.address;
        cpu->lr = HLE_CALLBACK_RETURN;
        if (g_card_log)
            fprintf(stderr, "[card] dispatch callback=0x%08X channel=%d result=%d\n",
                    g_irq_callback.pending.address, g_irq_callback.pending.channel,
                    g_irq_callback.pending.result);
        break;
    case 2:
        cpu->pc = sdk_symbol_address("OSEnableScheduler");
        cpu->lr = HLE_CALLBACK_RETURN;
        break;
    case 3:
        cpu->pc = sdk_symbol_address("__OSReschedule");
        cpu->lr = HLE_CALLBACK_RETURN;
        g_callback_active = false;  // EE stays off until it returns or switches
        break;
    default:
        cpu->gpr[3] = g_irq_callback.context;
        cpu->pc = sdk_symbol_address("OSLoadContext");
        g_irq_callback.stage = 0;
        g_callback_active = false;
        break;
    }
}

bool hle_poll_callback(CPUState* cpu) {
    g_callback_visits++;
    if (cpu == NULL || g_callback_count == 0 ||
        !(cpu->msr & 0x8000u))  // MSR[EE]: only where an interrupt could land
        return false;
    const u32 current = mem_read32(cpu, OS_CURRENT_THREAD);
    for (u32 i = 0; i < g_callback_count; i++) {
        HleCallbackWait* w = &g_callback_wait[(g_callback_read + i) % HLE_CALLBACK_QUEUE_CAPACITY];
        if (w->thread == current && w->visits < DVD_CALLBACK_VISITS)
            w->visits++;
    }
    if (g_callback_active || !callback_due(cpu, &g_callback_wait[g_callback_read]))
        return false;  // a DVD read its thread has not moved past (DVD_CALLBACK_VISITS)
    const u32 context = interrupt_enter_exception(cpu);
    if (context == 0)
        return false;

    g_irq_callback.pending = g_callback_queue[g_callback_read];
    g_callback_read = (g_callback_read + 1u) % HLE_CALLBACK_QUEUE_CAPACITY;
    g_callback_count--;
    g_irq_callback.context = context;
    g_irq_callback.stage = 1;
    g_callback_active = true;
    cpu->pc = sdk_symbol_address("OSDisableScheduler");
    cpu->lr = HLE_CALLBACK_RETURN;
    return true;
}

// ---------------------------------------------------------------------------
// Guest memory helpers
// ---------------------------------------------------------------------------
char* hle_read_cstr(CPUState* c, u32 gaddr, char* buf, size_t cap) {
    size_t i = 0;
    if (gaddr != 0) {
        for (; i + 1 < cap; i++) {
            u8 ch = mem_read8(c, gaddr + (u32)i);
            if (ch == 0)
                break;
            buf[i] = (char)ch;
        }
    }
    buf[i] = '\0';
    return buf;
}

static bool copy_guest_to_host(CPUState* cpu, u32 guest_address, void* output,
                               u32 length) {
    if (length == 0)
        return true;
    u32 available = 0;
    const void* direct =
        mmio_guest_pointer(cpu, guest_address, &available);
    if (direct != NULL && available >= length) {
        memcpy(output, direct, length);
        return true;
    }
    u8* bytes = (u8*)output;
    for (u32 i = 0; i < length; i++)
        bytes[i] = mem_read8(cpu, guest_address + i);
    return true;
}

static bool copy_host_to_guest(CPUState* cpu, u32 guest_address,
                               const void* input, u32 length) {
    if (length == 0)
        return true;
    u32 available = 0;
    void* direct = mmio_guest_pointer(cpu, guest_address, &available);
    if (direct != NULL && available >= length) {
        memcpy(direct, input, length);
        return true;
    }
    const u8* bytes = (const u8*)input;
    for (u32 i = 0; i < length; i++)
        mem_write8(cpu, guest_address + i, bytes[i]);
    return true;
}

// ---------------------------------------------------------------------------
// OS handlers
// ---------------------------------------------------------------------------

// OSReport(const char* fmt, ...) — the game's own diagnostic logging. Aurora
// does not provide it, so reformat against the EABI varargs here. This is the
// most valuable early intercept: it surfaces what the boot code is doing.
static void hle_OSReport(CPUState* cpu) {
    char fmt[1024];
    hle_read_cstr(cpu, cpu->gpr[3], fmt, sizeof fmt);

    char out[2048];
    size_t oi = 0;
    unsigned iarg = 1;  // next integer vararg -> r4
    unsigned farg = 1;  // next float vararg   -> f2 (f1 unused for varargs)

    for (size_t i = 0; fmt[i] && oi + 1 < sizeof out;) {
        if (fmt[i] != '%') {
            out[oi++] = fmt[i++];
            continue;
        }
        // Rebuild a single conversion spec, dropping length modifiers so the
        // host snprintf matches the value type we pass (always 32-bit / double).
        char spec[32];
        size_t si = 0;
        spec[si++] = fmt[i++];
        while (fmt[i] && si + 2 < sizeof spec && strchr("-+ #0123456789.", fmt[i]))
            spec[si++] = fmt[i++];
        while (fmt[i] && strchr("lhLzjt", fmt[i]))
            i++;  // skip length modifier
        char conv = fmt[i] ? fmt[i++] : '\0';
        spec[si++] = conv;
        spec[si] = '\0';

        char tmp[576];
        tmp[0] = '\0';
        switch (conv) {
        case '%':
            tmp[0] = '%';
            tmp[1] = '\0';
            break;
        case 'd': case 'i':
            snprintf(tmp, sizeof tmp, spec, (int)hle_arg_u32(cpu, iarg++));
            break;
        case 'u': case 'x': case 'X': case 'o': case 'c':
            snprintf(tmp, sizeof tmp, spec, (unsigned)hle_arg_u32(cpu, iarg++));
            break;
        case 'p':
            snprintf(tmp, sizeof tmp, "0x%08X", hle_arg_u32(cpu, iarg++));
            break;
        case 's': {
            char s[512];
            hle_read_cstr(cpu, hle_arg_u32(cpu, iarg++), s, sizeof s);
            snprintf(tmp, sizeof tmp, spec, s);
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
            snprintf(tmp, sizeof tmp, spec, hle_arg_f64(cpu, farg++));
            break;
        default:
            snprintf(tmp, sizeof tmp, "%s", spec);
            break;
        }
        for (size_t k = 0; tmp[k] && oi + 1 < sizeof out;)
            out[oi++] = tmp[k++];
    }
    out[oi] = '\0';
    fputs(out, stdout);
    fflush(stdout);
}

// A handler that does nothing and returns to the caller. Used to skip recompiled
// SDK routines whose only job is to drive hardware the host owns instead.
static void hle_noop(CPUState* cpu) { (void)cpu; }

// Skip a recompiled routine and report success (BOOL/int return of 1). For
// init functions whose hardware bring-up the host replaces but whose callers
// branch on a success return.
static void hle_return_true(CPUState* cpu) { hle_set_u32(cpu, 1); }
static void hle_return_zero(CPUState* cpu) { hle_set_u32(cpu, 0); }

static u32 lc_transaction_count(u32 bytes) {
    const u32 blocks = (bytes + 31u) / 32u;
    return (blocks + 127u) / 128u;
}

static void hle_LCStoreBlocks(CPUState* cpu) {
    const u32 dest = hle_arg_u32(cpu, 0);
    const u32 src = hle_arg_u32(cpu, 1);
    const u32 blocks = hle_arg_u32(cpu, 2) ? hle_arg_u32(cpu, 2) : 128u;
    mmio_guest_copy(cpu, dest, src, blocks * 32u);
}

// LCLoadBlocks(lcDest, ramSrc, blocks): RAM -> locked cache. The recompiled
// routine only writes DMA_U/DMA_L, which start no transfer here, so without
// this the cache kept stale bytes and LCStoreBlocks copied them out. The game
// moves its per-frame character display lists between buffers this way; the
// stale copies were zero or an old list, so characters vanished on some frames.
static void hle_LCLoadBlocks(CPUState* cpu) {
    const u32 dest = hle_arg_u32(cpu, 0);
    const u32 src = hle_arg_u32(cpu, 1);
    const u32 blocks = hle_arg_u32(cpu, 2) ? hle_arg_u32(cpu, 2) : 128u;
    mmio_guest_copy(cpu, dest, src, blocks * 32u);
}

static void hle_LCStoreData(CPUState* cpu) {
    const u32 dest = hle_arg_u32(cpu, 0);
    const u32 src = hle_arg_u32(cpu, 1);
    const u32 bytes = hle_arg_u32(cpu, 2);
    mmio_guest_copy(cpu, dest, src, bytes);
    hle_set_u32(cpu, lc_transaction_count(bytes));
}

// Desktop hosts have no GameCube chassis reset switch. Controller-based
// Start+L+R soft reset remains implemented by the game's own input path.
// ED_RESET_BUTTON=start,end (testing): held from frame start to end.
static void hle_OSGetResetButtonState(CPUState* cpu) {
    static long long start = -1, end = -1;
    if (start == -1) {
        const char* env = getenv("ED_RESET_BUTTON");
        start = -2;
        if (env != NULL && sscanf(env, "%lld,%lld", &start, &end) != 2)
            start = -2;
    }
    const long long frame = (long long)(cpu->timebase / 675000ull);
    hle_set_u32(cpu, start >= 0 && frame >= start && frame < end ? 1u : 0u);
}

// OSResetSystem(OS_RESET_RESTART) ends in __OSReboot, which reads the disc's
// apploader to 0x81300000 and jumps to it to load the game again. ED restarts
// itself this way (e.g. after a chapter), keeping its state in the range it
// gave OSSetSaveRegion (PAL 0x80700000..0x81200000) and checking
// OSGetResetCode at boot. Nothing at 0x813xxxxx is recompiled, so the guest
// spun there for ever (Mac) or left recompiled code (PS5). quickstate.c
// restarts the machine instead, from the boot state with that range put back;
// until it does (at its next safe point) the guest waits here.
static bool g_reboot_requested;

// OSSetSaveRegion(start, end) is `stw r3, d(r13)`, `stw r4, d(r13)`: read the
// two words it stores to.
static bool save_region(CPUState* cpu, u32* start, u32* end) {
    const u32 fn = sdk_symbol_address("OSSetSaveRegion");
    if (fn == 0u)
        return false;
    u32 word[2];
    for (u32 i = 0; i < 2u; i++) {
        const u32 op = mem_read32(cpu, fn + 4u * i);
        if ((op >> 26) != 36u || ((op >> 21) & 31u) != 3u + i ||
            ((op >> 16) & 31u) != 13u)
            return false;
        word[i] = cpu->gpr[13] + (u32)(s32)(s16)(op & 0xFFFFu);
    }
    *start = mem_read32(cpu, word[0]);
    *end = mem_read32(cpu, word[1]);
    return true;
}

static void hle___OSReboot(CPUState* cpu) {
    if (!g_reboot_requested) {
        u32 start = 0, end = 0;
        if (!save_region(cpu, &start, &end))
            fprintf(stderr, "[reset] OSSetSaveRegion not found; keeping nothing\n");
        if (!quickstate_request_reboot(cpu, start, end, true)) {
            fprintf(stderr, "[reset] no boot state to restart from\n");
            g_hle_passthrough = true;
            return;
        }
        fprintf(stderr, "[reset] game restarting itself (save region 0x%08X..0x%08X)\n",
                start, end);
        g_reboot_requested = true;
    }
    g_hle_tail_call = true;  // stay at the entry until the restart
}

// OSResetSystem(OS_RESET_HOTRESET) and __OSReboot's fallback end in the SDK's
// Reset(code << 3): it writes the PI reset register and spins, for the
// hardware to reset the console and the IPL to boot the disc afresh. ED asks
// for it from its reset-button and disc-error paths. Restart from the boot
// state, keeping nothing (a cold start, as from the IPL).
static void hle_Reset(CPUState* cpu) {
    if (!g_reboot_requested) {
        if (!quickstate_request_reboot(cpu, 0u, 0u, false)) {
            fprintf(stderr, "[reset] hot reset: no boot state to restart from\n");
            g_hle_passthrough = true;
            return;
        }
        fprintf(stderr, "[reset] hot reset (code 0x%08X): starting the game again\n",
                cpu->gpr[3] >> 3);
        g_reboot_requested = true;
    }
    g_hle_tail_call = true;
}

static void hle_salInitDsp(CPUState* cpu) {
    audio_skip_dsp_init(cpu);
    hle_set_u32(cpu, 1);
}

// ---------------------------------------------------------------------------
// ARAM (AR) handlers
//
// The real ARAM setup (__OSInitAudioSystem -> ARInit) does a DSP/ARAM hardware
// handshake we skip, so the AR* SDK functions are served here against the host
// ARAM buffer (aram.c). Without these, ARGetBaseAddress returns uninitialized
// garbage and the game's ARAM heap walks an invalid free list forever.
// ---------------------------------------------------------------------------
// ARAM addresses are offsets into the 16 MB of sound RAM. ARInit and
// ARGetBaseAddress return the start of the user area, 0x4000 on hardware (the
// first 16 KB are reserved for the OS). This game's MusyX lays its sample and
// stream buffers out from that base and checks them against ARGetSize, so a
// synthetic CPU-window base made every stream allocation fail. GXRuntime's ARAM
// accepts plain offsets.
#define ARAM_USER_BASE 0x4000u
// ED_WATCH=addr[:len] (debug): report every host transfer into guest RAM that
// overlaps the range (disc reads, ARAM to RAM), with the caller.
static void watch_host_write(CPUState* cpu, const char* what, u32 dest,
                             u32 length) {
    static int armed = -1;
    static u32 watch, watch_len;
    if (armed < 0) {
        const char* env = getenv("ED_WATCH");
        armed = env != NULL && env[0] != '\0';
        char* end = NULL;
        watch = armed ? (u32)strtoul(env, &end, 0) | 0x80000000u : 0u;
        watch_len = armed && end != NULL && *end == ':'
                        ? (u32)strtoul(end + 1, NULL, 0)
                        : 1u;
    }
    if (!armed)
        return;
    const u32 d = dest | 0x80000000u;
    if (d < watch + watch_len && watch < d + length)
        fprintf(stderr, "[watch] %s writes 0x%08X..0x%08X (len 0x%X) lr=0x%08X "
                        "pc=0x%08X frame=%llu\n",
                what, dest, dest + length, length, cpu->lr, cpu->pc,
                (unsigned long long)(cpu->timebase / 675000ull));
}

// ED_ARAM_LOG=1: every host ARAM transfer, to find who owns an ARAM range.
static void aram_log(CPUState* cpu, const char* path, u32 type, u32 mainmem,
                     u32 aram, u32 length) {
    static int enabled = -1;
    if (enabled < 0)
        enabled = getenv("ED_ARAM_LOG") != NULL;
    if (enabled)
        fprintf(stderr, "[aram] %s %s mram=0x%08X aram=0x%08X..0x%08X len=0x%X "
                        "lr=0x%08X\n",
                path, type == 0 ? "to-aram" : "to-mram", mainmem, aram,
                aram + length, length, cpu->lr);
}

static void hle_ARInit(CPUState* cpu) { hle_set_u32(cpu, ARAM_USER_BASE); }
static void hle_ARGetBaseAddress(CPUState* cpu) { hle_set_u32(cpu, ARAM_USER_BASE); }
static void hle_ARGetSize(CPUState* cpu) { hle_set_u32(cpu, ARAM_SIZE); }
static void hle_ARGetDMAStatus(CPUState* cpu) { hle_set_u32(cpu, 0); }  // idle

// ARStartDMA(u32 type, u32 mainmem, u32 aram, u32 length). type 0 = MRAM->ARAM,
// 1 = ARAM->MRAM. Performed synchronously against the host ARAM buffer.
static void hle_ARStartDMA(CPUState* cpu) {
    u32 type = hle_arg_u32(cpu, 0);
    u32 mainmem = hle_arg_u32(cpu, 1);
    u32 aram = hle_arg_u32(cpu, 2);
    u32 length = hle_arg_u32(cpu, 3);
    aram_log(cpu, "ARStartDMA", type, mainmem, aram, length);
    if (type == 0)
        aram_dma_to_aram(cpu->ram, mainmem, aram, length);
    else
        watch_host_write(cpu, "ARStartDMA", mainmem, length);
        aram_dma_to_ram(cpu->ram, mainmem, aram, length);
}

// MusyX's own aramUploadData runs (no HLE): it hands each request to
// aramUploadCallback when the game set one, else to ARQPostRequest below. This
// game sets it to stream its sample banks from the disc straight into ARAM; a
// host shortcut that copied from main RAM uploaded memory the game never filled
// and made every sound effect and menu tune crackle.

// Some MusyX builds inline aramUploadData into aramInit, leaving ARQPostRequest
// as the actual hardware boundary. Complete the transfer immediately and
// tail-call ARQ's completion routine with the request pointer. The caller
// increments its queue count after ARQPostRequest returns; MusyX's callback
// decrements it first, so the original bookkeeping still settles at zero.
// ARQPostRequest(ARQRequest*, owner, type, priority, source, dest, length,
// callback). The SDK fills the request in before queueing it, and callbacks
// read it back: the room streamer's completion callback finds its stream
// through request->owner, so leaving the struct unwritten hung Start Game.
#define ARQ_REQ_NEXT 0x00u
#define ARQ_REQ_OWNER 0x04u
#define ARQ_REQ_TYPE 0x08u
#define ARQ_REQ_PRIORITY 0x0Cu
#define ARQ_REQ_SOURCE 0x10u
#define ARQ_REQ_DEST 0x14u
#define ARQ_REQ_LENGTH 0x18u
#define ARQ_REQ_CALLBACK 0x1Cu
static void hle_ARQPostRequest(CPUState* cpu) {
    u32 request = hle_arg_u32(cpu, 0);
    u32 type = hle_arg_u32(cpu, 2);
    u32 source = hle_arg_u32(cpu, 4);
    u32 destination = hle_arg_u32(cpu, 5);
    u32 length = hle_arg_u32(cpu, 6);
    u32 callback = hle_arg_u32(cpu, 7);
    if (request != 0) {
        mem_write32(cpu, request + ARQ_REQ_NEXT, 0);
        mem_write32(cpu, request + ARQ_REQ_OWNER, hle_arg_u32(cpu, 1));
        mem_write32(cpu, request + ARQ_REQ_TYPE, type);
        mem_write32(cpu, request + ARQ_REQ_PRIORITY, hle_arg_u32(cpu, 3));
        mem_write32(cpu, request + ARQ_REQ_SOURCE, source);
        mem_write32(cpu, request + ARQ_REQ_DEST, destination);
        mem_write32(cpu, request + ARQ_REQ_LENGTH, length);
        mem_write32(cpu, request + ARQ_REQ_CALLBACK, callback);
    }
    if (type == 0) {
        aram_log(cpu, "ARQPostRequest", 0, source, destination, length);
        aram_dma_to_aram(cpu->ram, source, destination, length);
    } else {
        aram_log(cpu, "ARQPostRequest", 1, destination, source, length);
        watch_host_write(cpu, "ARQPostRequest", destination, length);
        aram_dma_to_ram(cpu->ram, destination, source, length);
    }
    if (callback) {
        cpu->gpr[3] = request;
        cpu->pc = callback;
        g_hle_tail_call = true;
    }
}

// ---------------------------------------------------------------------------
// DVD (disc) handlers
//
// The game's file system reads assets off the disc through the DVD SDK API. We
// never ran the apploader, so there is no in-memory FST and no DI hardware to
// stream sectors; the host owns the "drive" and serves these from the original
// ISO (dvd.c). Intercepting the high-level entry points keeps the SDK's async
// read state machine intact -- it polls the command block, which our reads
// complete synchronously by setting cb.state = DVD_STATE_END.
// ---------------------------------------------------------------------------
// DVDFileInfo / DVDCommandBlock field offsets (dolphin/dvd.h).
#define DVD_CB_STATE      0x0Cu
#define DVD_CB_CURRXFER   0x1Cu
#define DVD_CB_XFERRED    0x20u
#define DVD_FI_STARTADDR  0x30u
#define DVD_FI_LENGTH     0x34u
#define DVD_FI_CALLBACK   0x38u
#define DVD_STATE_END     0u

// DVDInit / DVDClose: the host owns the drive, so init is a no-op and close has
// nothing to cancel. Both report success.
static void hle_DVDInit(CPUState* cpu) { (void)cpu; }
// The DVDFileInfo of the open movie (/fmv/*.avi), 0 when none: a cutscene
// for fast-forward (cutscene_playing).
static u32 g_movie_file;

static void hle_DVDClose(CPUState* cpu) {
    if (hle_arg_u32(cpu, 0) == g_movie_file)
        g_movie_file = 0u;
    hle_set_u32(cpu, 1);  // TRUE
}

// The original status helpers consult the SDK DVD driver's `executing`
// command pointer and error flags. Host reads bypass that driver entirely, so
// those globals are stale and can leave nlServiceFileSystem spinning forever
// in DVD_STATE_BUSY after a synchronously completed read.
static void hle_DVDGetCommandBlockStatus(CPUState* cpu) {
    u32 block = hle_arg_u32(cpu, 0);
    s32 state = block ? (s32)mem_read32(cpu, block + DVD_CB_STATE) : DVD_STATE_END;
    hle_set_u32(cpu, state == 3 ? 1u : (u32)state);
}

static void hle_DVDGetDriveStatus(CPUState* cpu) {
    hle_set_u32(cpu, DVD_STATE_END);
}

// DVDConvertPathToEntrynum(const char* path) -> s32 entry (or -1).
// The SDK's current directory (DVDChangeDir). Relative paths resolve against
// it; the host keeps it as an absolute path into the disc image's FST.
static char g_dvd_cwd[256] = "/";

static void dvd_guest_path(CPUState* cpu, u32 address, char* out, size_t cap) {
    char path[256];
    hle_read_cstr(cpu, address, path, sizeof path);
    if (path[0] == '/')
        snprintf(out, cap, "%s", path);
    else
        snprintf(out, cap, "%s%s%s", g_dvd_cwd,
                 g_dvd_cwd[strlen(g_dvd_cwd) - 1] == '/' ? "" : "/", path);
}

static void hle_DVDConvertPathToEntrynum(CPUState* cpu) {
    char path[256];
    dvd_guest_path(cpu, hle_arg_u32(cpu, 0), path, sizeof path);
    s32 entry = dvd_path_to_entrynum(path);
    if (g_dvd_log)
        fprintf(stderr, "[dvd] entry '%s' -> %d\n", path, entry);
    hle_set_u32(cpu, (u32)entry);
}

// DVDChangeDir(dirName) -> BOOL
static void hle_DVDChangeDir(CPUState* cpu) {
    char path[256];
    dvd_guest_path(cpu, hle_arg_u32(cpu, 0), path, sizeof path);
    const bool ok = dvd_path_to_entrynum(path) >= 0;
    if (ok)
        snprintf(g_dvd_cwd, sizeof g_dvd_cwd, "%s", path);
    if (g_hle_log)
        fprintf(stderr, "[dvd] chdir '%s' -> %s\n", path, ok ? "ok" : "not found");
    hle_set_u32(cpu, ok ? 1u : 0u);
}

// DVDGetCurrentDir(path, maxlen) -> BOOL
static void hle_DVDGetCurrentDir(CPUState* cpu) {
    const u32 out = hle_arg_u32(cpu, 0);
    const u32 cap = hle_arg_u32(cpu, 1);
    u32 i = 0;
    for (; cap && i + 1u < cap && g_dvd_cwd[i] != '\0'; i++)
        mem_write8(cpu, out + i, (u8)g_dvd_cwd[i]);
    if (cap)
        mem_write8(cpu, out + i, 0);
    hle_set_u32(cpu, 1);
}

// DVDFastOpen(s32 entry, DVDFileInfo* fi) -> BOOL. Fills the file's disc offset
// and length, exactly as the SDK does from the FST.
static void hle_DVDFastOpen(CPUState* cpu) {
    s32 entry = (s32)hle_arg_u32(cpu, 0);
    u32 fi = hle_arg_u32(cpu, 1);
    u32 start = 0, length = 0;
    if (fi && dvd_entry_info(entry, &start, &length)) {
        mem_write32(cpu, fi + DVD_FI_STARTADDR, start);
        mem_write32(cpu, fi + DVD_FI_LENGTH, length);
        mem_write32(cpu, fi + DVD_FI_CALLBACK, 0);
        mem_write32(cpu, fi + DVD_CB_STATE, DVD_STATE_END);
        hle_set_u32(cpu, 1);  // TRUE
    } else {
        hle_set_u32(cpu, 0);  // FALSE
    }
}

// DVDOpen(path, fileInfo). The SDK resolves the entry through the FST in guest
// memory, which the host never loads (there is no apploader); resolve it from
// the disc image instead.
static bool guest_ram_range_valid(const CPUState* cpu, u32 address, u32 size);

static void hle_DVDOpen(CPUState* cpu) {
    char path[256];
    dvd_guest_path(cpu, hle_arg_u32(cpu, 0), path, sizeof path);
    cpu->gpr[3] = (u32)dvd_path_to_entrynum(path);
    if ((s32)cpu->gpr[3] < 0) {
        if (g_hle_log)
            fprintf(stderr, "[dvd] open '%s' -> not found\n", path);
        hle_set_u32(cpu, 0);
        return;
    }
    if (g_dvd_log) {
        fprintf(stderr, "[dvd] open '%s' (frame %llu) calls: 0x%08X", path,
                (unsigned long long)(cpu->timebase / 675000ull), cpu->lr);
        // Who asked: the return addresses up the stack.
        u32 sp = cpu->gpr[1];
        for (int f = 0; f < 10 && guest_ram_range_valid(cpu, sp, 8u); f++) {
            const u32 caller = mem_read32(cpu, sp);
            if (caller <= sp || !guest_ram_range_valid(cpu, caller, 8u))
                break;
            fprintf(stderr, " 0x%08X", mem_read32(cpu, caller + 4u));
            sp = caller;
        }
        fputc('\n', stderr);
    }
    skip_intro_note_open(cpu, path);
    const size_t length = strlen(path);
    if (strncmp(path, "/fmv/", 5) == 0 && length > 4u &&
        strcmp(path + length - 4u, ".avi") == 0)
        g_movie_file = hle_arg_u32(cpu, 1);
    hle_DVDFastOpen(cpu);
}

// DVDReadAsyncPrio(DVDFileInfo* fi, void* addr, s32 len, s32 offset, cb, prio).
// Copies the file bytes immediately and marks the command block complete; the
// caller's poll loop (DVDGetCommandBlockStatus) then sees DVD_STATE_END.
// Every host DVD operation finishes at once: mark the block done and run the
// completion callback, callback(s32 result, DVDCommandBlock* block), the way the
// driver's interrupt path would, once the caller is past the call
// (DVD_CALLBACK_VISITS).
static void dvd_complete(CPUState* cpu, u32 block, u32 callback, s32 result,
                         u32 transferred) {
    if (block) {
        mem_write32(cpu, block + DVD_CB_STATE, DVD_STATE_END);
        mem_write32(cpu, block + DVD_CB_CURRXFER, transferred);
        mem_write32(cpu, block + DVD_CB_XFERRED, transferred);
    }
    queue_guest_callback_after(cpu, callback, result, (s32)block);
}

static void hle_DVDReadAsyncPrio(CPUState* cpu) {
    u32 fi = hle_arg_u32(cpu, 0);
    u32 addr = hle_arg_u32(cpu, 1);
    u32 length = hle_arg_u32(cpu, 2);
    u32 offset = hle_arg_u32(cpu, 3);
    u32 callback = hle_arg_u32(cpu, 4);

    u32 start = mem_read32(cpu, fi + DVD_FI_STARTADDR);
    if (g_dvd_log)
        fprintf(stderr, "[dvd] read fi=0x%08X disc=0x%08X+0x%X len=0x%X (file 0x%X) addr=0x%08X "
                        "cb=0x%08X lr=0x%08X frame=%llu\n",
                fi, start, offset, length, mem_read32(cpu, fi + DVD_FI_STARTADDR + 4u), addr,
                callback, cpu->lr, (unsigned long long)(cpu->timebase / 675000ull));
    watch_host_write(cpu, "DVDReadAsyncPrio", addr, length);
    dvd_read_to_guest(cpu, addr, start + offset, length);
    mem_write32(cpu, fi + DVD_FI_CALLBACK, callback);
    dvd_complete(cpu, fi, callback, (s32)length, length);
    hle_set_u32(cpu, 1);  // TRUE (queued)
}

// DVDReadPrio(fileInfo, addr, length, offset, prio): synchronous, returns bytes read.
static void hle_DVDReadPrio(CPUState* cpu) {
    u32 fi = hle_arg_u32(cpu, 0);
    u32 addr = hle_arg_u32(cpu, 1);
    u32 length = hle_arg_u32(cpu, 2);
    u32 offset = hle_arg_u32(cpu, 3);
    if (g_hle_log)
        fprintf(stderr, "[dvd] readprio addr=0x%08X len=%u disc=0x%08X lr=0x%08X\n",
                addr, length, mem_read32(cpu, fi + DVD_FI_STARTADDR) + offset,
                cpu->lr);
    watch_host_write(cpu, "DVDReadPrio", addr, length);
    dvd_read_to_guest(cpu, addr, mem_read32(cpu, fi + DVD_FI_STARTADDR) + offset, length);
    dvd_complete(cpu, fi, 0, 0, length);
    hle_set_u32(cpu, length);
}

// DVDReadAbsAsyncPrio(block, addr, length, discOffset, callback, prio)
static void hle_DVDReadAbsAsyncPrio(CPUState* cpu) {
    u32 block = hle_arg_u32(cpu, 0);
    u32 addr = hle_arg_u32(cpu, 1);
    u32 length = hle_arg_u32(cpu, 2);
    if (g_hle_log)
        fprintf(stderr, "[dvd] readabs addr=0x%08X len=%u disc=0x%08X cb=0x%08X\n",
                addr, length, hle_arg_u32(cpu, 3), hle_arg_u32(cpu, 4));
    watch_host_write(cpu, "DVDReadAbsAsyncPrio", addr, length);
    dvd_read_to_guest(cpu, addr, hle_arg_u32(cpu, 3), length);
    dvd_complete(cpu, block, hle_arg_u32(cpu, 4), (s32)length, length);
    hle_set_u32(cpu, 1);
}

// DVDReadDiskID(block, diskID, callback): the 0x20-byte disc header.
static void hle_DVDReadDiskID(CPUState* cpu) {
    u32 block = hle_arg_u32(cpu, 0);
    dvd_read_to_guest(cpu, hle_arg_u32(cpu, 1), 0, 0x20u);
    dvd_complete(cpu, block, hle_arg_u32(cpu, 2), 0x20, 0x20u);
    hle_set_u32(cpu, 1);
}

// DVDInquiryAsync(block, DVDDriveInfo* info, callback): a retail drive's reply.
static void hle_DVDInquiryAsync(CPUState* cpu) {
    u32 block = hle_arg_u32(cpu, 0);
    u32 info = hle_arg_u32(cpu, 1);
    for (u32 i = 0; i < 0x20u; i += 4u)
        mem_write32(cpu, info + i, 0);
    mem_write16(cpu, info + 0u, 0x0002u);       // revisionLevel
    mem_write16(cpu, info + 2u, 0x0006u);       // deviceCode
    mem_write32(cpu, info + 4u, 0x20020402u);   // releaseDate
    dvd_complete(cpu, block, hle_arg_u32(cpu, 2), 0x20, 0x20u);
    hle_set_u32(cpu, 1);
}

// Commands with nothing for the host drive to do: cancel, seek and the audio
// streaming requests (DTK). They succeed at once; the streamed music itself is
// played by the host from the disc image (see the DTK notes in audio.c).
// DVD*Async(block, ..., callback) with the callback as the last argument.
static void dvd_async_ok(CPUState* cpu, u32 callback_arg) {
    dvd_complete(cpu, hle_arg_u32(cpu, 0), hle_arg_u32(cpu, callback_arg), 0, 0);
    hle_set_u32(cpu, 1);
}
static void hle_DVDCancelAsync(CPUState* cpu) { dvd_async_ok(cpu, 1); }
static void hle_DVDCancel(CPUState* cpu) {
    dvd_complete(cpu, hle_arg_u32(cpu, 0), 0, 0, 0);
    hle_set_u32(cpu, 0);
}
static void hle_DVDCancelAll(CPUState* cpu) { hle_set_u32(cpu, 0); }
static void hle_DVDCheckDisk(CPUState* cpu) { hle_set_u32(cpu, 1); }
static void hle_DVDGetCurrentDiskID(CPUState* cpu) { hle_set_u32(cpu, 0x80000000u); }
static void hle_DVDReset(CPUState* cpu) { (void)cpu; }

// Disc audio streaming (DTK): the drive plays a prepared byte range of an .adp
// file into the AI stream; dtk.c decodes it from the image (see audio.c for the
// AI side). Replies follow Dolphin's DVDInterface: the play address is in
// 4-byte units, the status is 1 while a stream plays.
// DVDPrepareStreamAsync(fileInfo, length, offset, callback); length 0 = to end.
static void hle_DVDPrepareStreamAsync(CPUState* cpu) {
    const u32 fi = hle_arg_u32(cpu, 0);
    const u32 length = hle_arg_u32(cpu, 1);
    const u32 offset = hle_arg_u32(cpu, 2);
    const u32 start = mem_read32(cpu, fi + DVD_FI_STARTADDR);
    const u32 file_length = mem_read32(cpu, fi + DVD_FI_LENGTH);
    dtk_prepare(start + offset,
                length != 0u ? length : file_length - offset);
    dvd_complete(cpu, fi, hle_arg_u32(cpu, 3), 0, 0);
    hle_set_u32(cpu, 1);
}
// DVDPrepareStreamAbsAsync(block, length, discOffset, callback)
static void hle_DVDPrepareStreamAbsAsync(CPUState* cpu) {
    dtk_prepare(hle_arg_u32(cpu, 2), hle_arg_u32(cpu, 1));
    dvd_complete(cpu, hle_arg_u32(cpu, 0), hle_arg_u32(cpu, 3), 0, 0);
    hle_set_u32(cpu, 1);
}
static void hle_DVDCancelStreamAsync(CPUState* cpu) {
    dtk_cancel();
    dvd_async_ok(cpu, 1);
}
static void hle_DVDStopStreamAtEndAsync(CPUState* cpu) {
    dtk_stop_at_end();
    dvd_async_ok(cpu, 1);
}
static void hle_DVDGetStreamErrorStatusAsync(CPUState* cpu) {
    dvd_complete(cpu, hle_arg_u32(cpu, 0), hle_arg_u32(cpu, 1),
                 dtk_active() ? 1 : 0, 0);
    hle_set_u32(cpu, 1);
}
static void hle_DVDGetStreamPlayAddrAsync(CPUState* cpu) {
    dvd_complete(cpu, hle_arg_u32(cpu, 0), hle_arg_u32(cpu, 1),
                 (s32)(dtk_play_address() >> 2), 0);
    hle_set_u32(cpu, 1);
}

// ---------------------------------------------------------------------------
// CARD handlers
//
// GXRuntime owns a persistent, game-independent virtual card. These handlers
// translate the G4QE01 SDK ABI and guest structs onto that service. The
// executable configures slot A, and slot B when it has a card for it.
// ---------------------------------------------------------------------------
#define CARD_FILE_INFO_CHAN 0x00u
#define CARD_FILE_INFO_FILENO 0x04u
#define CARD_FILE_INFO_OFFSET 0x08u
#define CARD_FILE_INFO_LENGTH 0x0Cu
#define CARD_FILE_INFO_IBLOCK 0x10u

#define CARD_STAT_FILENAME 0x00u
#define CARD_STAT_LENGTH 0x20u
#define CARD_STAT_TIME 0x24u
#define CARD_STAT_GAME 0x28u
#define CARD_STAT_COMPANY 0x2Cu
#define CARD_STAT_BANNER_FORMAT 0x2Eu
#define CARD_STAT_ICON_ADDRESS 0x30u
#define CARD_STAT_ICON_FORMAT 0x34u
#define CARD_STAT_ICON_SPEED 0x36u
#define CARD_STAT_COMMENT_ADDRESS 0x38u
#define CARD_STAT_OFFSET_BANNER 0x3Cu
#define CARD_STAT_OFFSET_BANNER_TLUT 0x40u
#define CARD_STAT_OFFSET_ICON 0x44u
#define CARD_STAT_OFFSET_ICON_TLUT 0x64u
#define CARD_STAT_OFFSET_DATA 0x68u

static DolMemoryCard* card_for_channel(s32 channel) {
    return channel == 0 ? g_memory_card : channel == 1 ? g_memory_card_b : NULL;
}

static s32 card_invalid_channel_result(s32 channel) {
    return (channel < 0 || channel >= 2) ? DOL_CARD_RESULT_FATAL
                                         : DOL_CARD_RESULT_NO_CARD;
}

static void card_set_async_result(CPUState* cpu, s32 channel, s32 result,
                                  u32 callback) {
    if (result >= 0 && callback != 0 &&
        !queue_guest_callback(callback, channel, result))
        result = DOL_CARD_RESULT_IO_ERROR;
    hle_set_u32(cpu, (u32)result);
}

static void card_write_file_info(CPUState* cpu, u32 address, s32 channel,
                                 s32 file_no) {
    mem_write32(cpu, address + CARD_FILE_INFO_CHAN, (u32)channel);
    mem_write32(cpu, address + CARD_FILE_INFO_FILENO, (u32)file_no);
    mem_write32(cpu, address + CARD_FILE_INFO_OFFSET, 0);
    mem_write32(cpu, address + CARD_FILE_INFO_LENGTH, 0);
    mem_write16(cpu, address + CARD_FILE_INFO_IBLOCK,
                (u16)(DOL_CARD_SYSTEM_BLOCKS + (u32)file_no));
}

static void card_update_icon_offsets(CPUState* cpu, u32 address,
                                     const DolMemoryCardStat* stat) {
    u32 offset = stat->icon_address;
    bool icon_tlut = false;
    u8 banner_format = stat->banner_format & 3u;
    if (stat->icon_address == 0xFFFFFFFFu) {
        banner_format = 0;
        offset = 0;
    }

    if (banner_format == 1u) {
        mem_write32(cpu, address + CARD_STAT_OFFSET_BANNER, offset);
        offset += 96u * 32u;
        mem_write32(cpu, address + CARD_STAT_OFFSET_BANNER_TLUT, offset);
        offset += 2u * 256u;
    } else if (banner_format == 2u) {
        mem_write32(cpu, address + CARD_STAT_OFFSET_BANNER, offset);
        offset += 2u * 96u * 32u;
        mem_write32(cpu, address + CARD_STAT_OFFSET_BANNER_TLUT, 0xFFFFFFFFu);
    } else {
        mem_write32(cpu, address + CARD_STAT_OFFSET_BANNER, 0xFFFFFFFFu);
        mem_write32(cpu, address + CARD_STAT_OFFSET_BANNER_TLUT, 0xFFFFFFFFu);
    }

    for (u32 i = 0; i < 8u; i++) {
        u32 format = (stat->icon_format >> (2u * i)) & 3u;
        if (format == 1u) {
            mem_write32(cpu, address + CARD_STAT_OFFSET_ICON + i * 4u,
                        offset);
            offset += 32u * 32u;
            icon_tlut = true;
        } else if (format == 2u) {
            mem_write32(cpu, address + CARD_STAT_OFFSET_ICON + i * 4u,
                        offset);
            offset += 2u * 32u * 32u;
        } else {
            mem_write32(cpu, address + CARD_STAT_OFFSET_ICON + i * 4u,
                        0xFFFFFFFFu);
        }
    }
    if (icon_tlut) {
        mem_write32(cpu, address + CARD_STAT_OFFSET_ICON_TLUT, offset);
        offset += 2u * 256u;
    } else {
        mem_write32(cpu, address + CARD_STAT_OFFSET_ICON_TLUT, 0xFFFFFFFFu);
    }
    mem_write32(cpu, address + CARD_STAT_OFFSET_DATA, offset);
}

static void card_write_status(CPUState* cpu, u32 address,
                              const DolMemoryCardStat* stat) {
    const bool has_icon_data = stat->icon_address != 0xFFFFFFFFu;
    for (u32 i = 0; i < DOL_CARD_FILENAME_MAX; i++)
        mem_write8(cpu, address + CARD_STAT_FILENAME + i,
                   (u8)stat->file_name[i]);
    mem_write32(cpu, address + CARD_STAT_LENGTH, stat->length);
    mem_write32(cpu, address + CARD_STAT_TIME, stat->time);
    for (u32 i = 0; i < 4u; i++)
        mem_write8(cpu, address + CARD_STAT_GAME + i, stat->game_code[i]);
    for (u32 i = 0; i < 2u; i++)
        mem_write8(cpu, address + CARD_STAT_COMPANY + i, stat->company[i]);
    mem_write8(cpu, address + CARD_STAT_BANNER_FORMAT,
               has_icon_data ? stat->banner_format : 0);
    mem_write32(cpu, address + CARD_STAT_ICON_ADDRESS, stat->icon_address);
    mem_write16(cpu, address + CARD_STAT_ICON_FORMAT,
                has_icon_data ? stat->icon_format : 0);
    mem_write16(cpu, address + CARD_STAT_ICON_SPEED,
                has_icon_data ? stat->icon_speed : 0);
    mem_write32(cpu, address + CARD_STAT_COMMENT_ADDRESS,
                stat->comment_address);
    card_update_icon_offsets(cpu, address, stat);
}

static void card_read_status(CPUState* cpu, u32 address,
                             DolMemoryCardStat* stat) {
    memset(stat, 0, sizeof(*stat));
    stat->banner_format =
        mem_read8(cpu, address + CARD_STAT_BANNER_FORMAT);
    stat->icon_address =
        mem_read32(cpu, address + CARD_STAT_ICON_ADDRESS);
    stat->icon_format =
        mem_read16(cpu, address + CARD_STAT_ICON_FORMAT);
    stat->icon_speed =
        mem_read16(cpu, address + CARD_STAT_ICON_SPEED);
    stat->comment_address =
        mem_read32(cpu, address + CARD_STAT_COMMENT_ADDRESS);
}

static void hle_CARDInit(CPUState* cpu) {
    (void)cpu;
}

static void hle_CARDProbe(CPUState* cpu) {
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    u16 size_mbits = 0;
    u32 sector_size = 0;
    hle_set_u32(cpu,
                dol_card_probe(card_for_channel(channel), &size_mbits,
                               &sector_size) == DOL_CARD_RESULT_READY ? 1u : 0u);
}

static void hle_CARDProbeEx(CPUState* cpu) {
    static unsigned probe_log_count;
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    u32 memory_size_address = hle_arg_u32(cpu, 1);
    u32 sector_size_address = hle_arg_u32(cpu, 2);
    DolMemoryCard* card = card_for_channel(channel);
    u16 size_mbits = 0;
    u32 sector_size = 0;
    s32 result = card != NULL
                     ? dol_card_probe(card, &size_mbits, &sector_size)
                     : card_invalid_channel_result(channel);
    if (result == DOL_CARD_RESULT_READY) {
        if (memory_size_address != 0)
            mem_write32(cpu, memory_size_address, size_mbits);
        if (sector_size_address != 0)
            mem_write32(cpu, sector_size_address, sector_size);
    }
    if (g_card_log &&
        (probe_log_count < 32u || (probe_log_count % 600u) == 0u))
        fprintf(stderr,
                "[card] CARDProbeEx channel=%d result=%d size=%u sector=%u\n",
                channel, result, size_mbits, sector_size);
    probe_log_count++;
    hle_set_u32(cpu, (u32)result);
}

static void hle_CARDGetResultCode(CPUState* cpu) {
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    DolMemoryCard* card = card_for_channel(channel);
    hle_set_u32(cpu,
                (u32)(card != NULL ? dol_card_last_result(card)
                                   : card_invalid_channel_result(channel)));
}

static void hle_CARDGetFastMode(CPUState* cpu) {
    hle_set_u32(cpu, 0u);
}

static void hle_CARDGetXferredBytes(CPUState* cpu) {
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    hle_set_u32(cpu, dol_card_transferred_bytes(card_for_channel(channel)));
}

static void hle_CARDUnmount(CPUState* cpu) {
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    DolMemoryCard* card = card_for_channel(channel);
    s32 result = card != NULL ? dol_card_unmount(card)
                              : card_invalid_channel_result(channel);
    if (g_card_log)
        fprintf(stderr, "[card] CARDUnmount channel=%d result=%d\n",
                channel, result);
    hle_set_u32(cpu, (u32)result);
}

static void hle_CARDClose(CPUState* cpu) {
    u32 file_info = hle_arg_u32(cpu, 0);
    s32 channel =
        file_info != 0 ? (s32)mem_read32(cpu, file_info + CARD_FILE_INFO_CHAN)
                       : -1;
    s32 file_no =
        file_info != 0
            ? (s32)mem_read32(cpu, file_info + CARD_FILE_INFO_FILENO)
            : -1;
    DolMemoryCard* card = card_for_channel(channel);
    s32 result = card != NULL ? dol_card_close_file(card, file_no)
                              : card_invalid_channel_result(channel);
    if (result == DOL_CARD_RESULT_READY)
        mem_write32(cpu, file_info + CARD_FILE_INFO_CHAN, 0xFFFFFFFFu);
    hle_set_u32(cpu, (u32)result);
}

static void hle_CARDMountAsync(CPUState* cpu) {
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    u32 callback = hle_arg_u32(cpu, 3);
    DolMemoryCard* card = card_for_channel(channel);
    s32 result = card != NULL ? dol_card_mount(card)
                              : card_invalid_channel_result(channel);
    if (g_card_log)
        fprintf(stderr,
                "[card] CARDMountAsync channel=%d result=%d callback=0x%08X\n",
                channel, result, callback);
    card_set_async_result(cpu, channel, result, callback);
}

static void hle_CARDCheckAsync(CPUState* cpu) {
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    u32 callback = hle_arg_u32(cpu, 1);
    DolMemoryCard* card = card_for_channel(channel);
    s32 result = card != NULL ? dol_card_check(card)
                              : card_invalid_channel_result(channel);
    if (g_card_log)
        fprintf(stderr,
                "[card] CARDCheckAsync channel=%d result=%d callback=0x%08X\n",
                channel, result, callback);
    card_set_async_result(cpu, channel, result, callback);
}

static void hle_CARDCheckExAsync(CPUState* cpu) {
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    u32 transferred_address = hle_arg_u32(cpu, 1);
    u32 callback = hle_arg_u32(cpu, 2);
    DolMemoryCard* card = card_for_channel(channel);
    s32 result = card != NULL ? dol_card_check(card)
                              : card_invalid_channel_result(channel);
    if (transferred_address != 0)
        mem_write32(cpu, transferred_address, 0);
    card_set_async_result(cpu, channel, result, callback);
}

static void hle_CARDFreeBlocks(CPUState* cpu) {
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    u32 bytes_address = hle_arg_u32(cpu, 1);
    u32 files_address = hle_arg_u32(cpu, 2);
    DolMemoryCard* card = card_for_channel(channel);
    u32 bytes_free = 0;
    u32 files_free = 0;
    s32 result =
        card != NULL
            ? dol_card_free_space(card, &bytes_free, &files_free)
            : card_invalid_channel_result(channel);
    if (result == DOL_CARD_RESULT_READY) {
        if (bytes_address != 0)
            mem_write32(cpu, bytes_address, bytes_free);
        if (files_address != 0)
            mem_write32(cpu, files_address, files_free);
    }
    if (g_card_log)
        fprintf(stderr,
                "[card] CARDFreeBlocks channel=%d result=%d bytes=%u files=%u\n",
                channel, result, bytes_free, files_free);
    hle_set_u32(cpu, (u32)result);
}

static void hle_CARDOpen(CPUState* cpu) {
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    u32 name_address = hle_arg_u32(cpu, 1);
    u32 file_info = hle_arg_u32(cpu, 2);
    char name[DOL_CARD_FILENAME_MAX + 2u];
    hle_read_cstr(cpu, name_address, name, sizeof name);
    DolMemoryCard* card = card_for_channel(channel);
    s32 file_no = -1;
    u32 length = 0;
    s32 result =
        card != NULL ? dol_card_open_file(card, name, &file_no, &length)
                     : card_invalid_channel_result(channel);
    (void)length;
    if (result == DOL_CARD_RESULT_READY && file_info != 0)
        card_write_file_info(cpu, file_info, channel, file_no);
    else if (file_info != 0)
        mem_write32(cpu, file_info + CARD_FILE_INFO_CHAN, 0xFFFFFFFFu);
    if (g_card_log)
        fprintf(stderr,
                "[card] CARDOpen channel=%d name='%s' result=%d file=%d "
                "length=%u\n",
                channel, name, result, file_no, length);
    hle_set_u32(cpu, (u32)result);
}

static void hle_CARDCreateAsync(CPUState* cpu) {
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    u32 name_address = hle_arg_u32(cpu, 1);
    u32 length = hle_arg_u32(cpu, 2);
    u32 file_info = hle_arg_u32(cpu, 3);
    u32 callback = hle_arg_u32(cpu, 4);
    char name[DOL_CARD_FILENAME_MAX + 2u];
    hle_read_cstr(cpu, name_address, name, sizeof name);
    DolMemoryCard* card = card_for_channel(channel);
    s32 file_no = -1;
    s32 result =
        card != NULL
            ? dol_card_create_file(card, name, length, &file_no)
            : card_invalid_channel_result(channel);
    if (result == DOL_CARD_RESULT_READY && file_info != 0)
        card_write_file_info(cpu, file_info, channel, file_no);
    if (g_card_log)
        fprintf(stderr,
                "[card] CARDCreateAsync channel=%d name='%s' length=%u "
                "result=%d file=%d callback=0x%08X\n",
                channel, name, length, result, file_no, callback);
    card_set_async_result(cpu, channel, result, callback);
}

static void hle_CARDDeleteAsync(CPUState* cpu) {
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    u32 name_address = hle_arg_u32(cpu, 1);
    u32 callback = hle_arg_u32(cpu, 2);
    char name[DOL_CARD_FILENAME_MAX + 2u];
    hle_read_cstr(cpu, name_address, name, sizeof name);
    DolMemoryCard* card = card_for_channel(channel);
    s32 result =
        card != NULL ? dol_card_delete_file(card, name)
                     : card_invalid_channel_result(channel);
    if (g_card_log)
        fprintf(stderr,
                "[card] CARDDeleteAsync channel=%d name='%s' result=%d "
                "callback=0x%08X\n",
                channel, name, result, callback);
    card_set_async_result(cpu, channel, result, callback);
}

static void hle_CARDReadAsync(CPUState* cpu) {
    u32 file_info = hle_arg_u32(cpu, 0);
    u32 buffer = hle_arg_u32(cpu, 1);
    u32 length = hle_arg_u32(cpu, 2);
    u32 offset = hle_arg_u32(cpu, 3);
    u32 callback = hle_arg_u32(cpu, 4);
    s32 channel =
        file_info != 0 ? (s32)mem_read32(cpu, file_info + CARD_FILE_INFO_CHAN)
                       : -1;
    s32 file_no =
        file_info != 0
            ? (s32)mem_read32(cpu, file_info + CARD_FILE_INFO_FILENO)
            : -1;
    DolMemoryCard* card = card_for_channel(channel);
    u8* bytes = length != 0 ? (u8*)malloc(length) : NULL;
    s32 result = length != 0 && bytes == NULL
                     ? DOL_CARD_RESULT_IO_ERROR
                     : (card != NULL
                            ? dol_card_read_file(card, file_no, offset, bytes,
                                                 length)
                            : card_invalid_channel_result(channel));
    if (result == DOL_CARD_RESULT_READY)
        copy_host_to_guest(cpu, buffer, bytes, length);
    free(bytes);
    if (g_card_log)
        fprintf(stderr,
                "[card] CARDReadAsync channel=%d file=%d offset=%u length=%u "
                "result=%d callback=0x%08X\n",
                channel, file_no, offset, length, result, callback);
    card_set_async_result(cpu, channel, result, callback);
}

static void hle_CARDWriteAsync(CPUState* cpu) {
    u32 file_info = hle_arg_u32(cpu, 0);
    u32 buffer = hle_arg_u32(cpu, 1);
    u32 length = hle_arg_u32(cpu, 2);
    u32 offset = hle_arg_u32(cpu, 3);
    u32 callback = hle_arg_u32(cpu, 4);
    s32 channel =
        file_info != 0 ? (s32)mem_read32(cpu, file_info + CARD_FILE_INFO_CHAN)
                       : -1;
    s32 file_no =
        file_info != 0
            ? (s32)mem_read32(cpu, file_info + CARD_FILE_INFO_FILENO)
            : -1;
    DolMemoryCard* card = card_for_channel(channel);
    u8* bytes = length != 0 ? (u8*)malloc(length) : NULL;
    s32 result = length != 0 && bytes == NULL
                     ? DOL_CARD_RESULT_IO_ERROR
                     : DOL_CARD_RESULT_READY;
    if (result == DOL_CARD_RESULT_READY)
        copy_guest_to_host(cpu, buffer, bytes, length);
    if (result == DOL_CARD_RESULT_READY)
        result = card != NULL
                     ? dol_card_write_file(card, file_no, offset, bytes, length)
                     : card_invalid_channel_result(channel);
    free(bytes);
    if (g_card_log)
        fprintf(stderr,
                "[card] CARDWriteAsync channel=%d file=%d offset=%u "
                "length=%u result=%d callback=0x%08X\n",
                channel, file_no, offset, length, result, callback);
    card_set_async_result(cpu, channel, result, callback);
}

static void hle_CARDGetStatus(CPUState* cpu) {
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    s32 file_no = (s32)hle_arg_u32(cpu, 1);
    u32 status_address = hle_arg_u32(cpu, 2);
    DolMemoryCard* card = card_for_channel(channel);
    DolMemoryCardStat status;
    s32 result =
        card != NULL ? dol_card_get_status(card, file_no, &status)
                     : card_invalid_channel_result(channel);
    if (result == DOL_CARD_RESULT_READY && status_address != 0)
        card_write_status(cpu, status_address, &status);
    if (g_card_log)
        fprintf(stderr,
                "[card] CARDGetStatus channel=%d file=%d result=%d\n",
                channel, file_no, result);
    hle_set_u32(cpu, (u32)result);
}

static void hle_CARDSetStatusAsync(CPUState* cpu) {
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    s32 file_no = (s32)hle_arg_u32(cpu, 1);
    u32 status_address = hle_arg_u32(cpu, 2);
    u32 callback = hle_arg_u32(cpu, 3);
    DolMemoryCard* card = card_for_channel(channel);
    DolMemoryCardStat status;
    card_read_status(cpu, status_address, &status);
    s32 result;
    if ((status.icon_address != 0xFFFFFFFFu &&
         status.icon_address >= 512u) ||
        (status.comment_address != 0xFFFFFFFFu &&
         status.comment_address % DOL_CARD_SECTOR_SIZE >
             DOL_CARD_SECTOR_SIZE - 64u)) {
        result = DOL_CARD_RESULT_FATAL;
    } else {
        result = card != NULL ? dol_card_set_status(card, file_no, &status)
                              : card_invalid_channel_result(channel);
    }
    if (g_card_log)
        fprintf(stderr,
                "[card] CARDSetStatusAsync channel=%d file=%d result=%d "
                "callback=0x%08X\n",
                channel, file_no, result, callback);
    card_set_async_result(cpu, channel, result, callback);
}

static void hle_CARDGetSerialNo(CPUState* cpu) {
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    u32 serial_address = hle_arg_u32(cpu, 1);
    DolMemoryCard* card = card_for_channel(channel);
    s32 result = card != NULL ? dol_card_check(card)
                              : card_invalid_channel_result(channel);
    if (result == DOL_CARD_RESULT_READY && serial_address != 0)
        mem_write64(cpu, serial_address, dol_card_serial(card));
    hle_set_u32(cpu, (u32)result);
}

static void hle_CARDFormatAsync(CPUState* cpu) {
    s32 channel = (s32)hle_arg_u32(cpu, 0);
    u32 callback = hle_arg_u32(cpu, 1);
    DolMemoryCard* card = card_for_channel(channel);
    s32 result = card != NULL ? dol_card_format(card)
                              : card_invalid_channel_result(channel);
    if (g_card_log)
        fprintf(stderr,
                "[card] CARDFormatAsync channel=%d result=%d callback=0x%08X\n",
                channel, result, callback);
    card_set_async_result(cpu, channel, result, callback);
}

static u32 guest_hash_bytes(const void* data, u32 bytes) {
    const u8* p = (const u8*)data;
    u32 hash = 2166136261u;
    for (u32 i = 0; i < bytes; i++)
        hash = (hash ^ p[i]) * 16777619u;
    return hash ? hash : 1u;
}

static u32 guest_hash(CPUState* cpu, u32 address, u32 bytes,
                      u32* hashed_bytes) {
    u32 available = 0;
    const void* data = mmio_guest_pointer(cpu, address, &available);
    if (data == NULL || bytes == 0 || available == 0) {
        if (hashed_bytes != NULL)
            *hashed_bytes = 0;
        return 0;
    }
    if (bytes > available)
        bytes = available;
    if (hashed_bytes != NULL)
        *hashed_bytes = bytes;
    return guest_hash_bytes(data, bytes);
}

#define THP_SIMPLE_CONTROL 0x8032F000u

static f32 guest_read_f32(CPUState* cpu, u32 address) {
    u32 bits = mem_read32(cpu, address);
    f32 value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

#ifdef ED_AURORA
// ---------------------------------------------------------------------------
// Aurora graphics metadata
//
// Raw writes to the GameCube write-gather pipe are forwarded by mmio.c into
// Aurora's GX FIFO. Most GX state therefore needs no per-function marshalling.
// Pointer-bearing commands are the exception: the guest encodes 32-bit
// GameCube physical addresses, while Aurora needs native host pointers. Track
// the guest objects selected by GX and emit Aurora metadata immediately before
// each draw, after the guest's raw texture/state commands are already queued.
// ---------------------------------------------------------------------------
#define GX_MAX_TEXTURES 8u
#define GX_MAX_TLUTS    20u

typedef struct {
    bool valid;
    u32 object;
    u32 guest_data;
    u32 width;
    u32 height;
    u32 format;
    u32 tlut;
    u32 version;
    u32 emitted_version;
    u32 data_bytes;
    u32 data_hash;
    u8 flags;
} GxTextureBinding;

typedef struct {
    bool valid;
    u32 object;
    u32 guest_data;
    u32 format;
    u32 version;
    u32 emitted_version;
    u32 data_bytes;
    u32 data_hash;
    u16 entries;
} GxTlutBinding;

static GxTextureBinding g_textures[GX_MAX_TEXTURES];
static GxTlutBinding g_tluts[GX_MAX_TLUTS];
static u32 g_array_address[32];
static u8 g_array_stride[32];
static unsigned g_gx_begin_count;
static unsigned g_gx_array_count;
// Per-frame accounting of indexed-array uploads. Aurora uploads each distinct
// indexed array's *whole advertised size* into a fixed, non-growable 8 MB
// per-frame storage buffer (aurora/lib/gfx StorageBufferSize); if the sum of
// all arrays bound in one frame exceeds that, the non-owned ByteBuffer aborts
// (ByteBuffer::resize, m_owned==false). We watch the running total so a future
// heavy scene warns loudly instead of crashing silently.
static u64 g_frame_array_bytes;
static unsigned g_frame_array_calls;
static u64 g_frame_array_bytes_peak;
static bool g_array_budget_warned;
// Aurora's per-frame storage buffer is 8 MB; warn once past 75% of it.
enum { AURORA_STORAGE_BUDGET = 8u * 1024u * 1024u };
enum { ARRAY_UPLOAD_WARN_BYTES = (AURORA_STORAGE_BUDGET * 3u) / 4u };
// GXSetArray carries no vertex count (smstrikers-decomp NL/glx/glxSend.cpp:1708
// passes only base+stride). With the local Aurora span patch live, push_gx_draw
// uploads only each draw's REAL indexed span min(array.size,(maxidx+1)*stride),
// so we advertise the FULL resolvable range and let the span patch bound the
// actual upload. No vertex cap is needed, and a cap can ONLY truncate geometry:
// the old flat 2048 cap truncated gameplay player/pitch meshes (which index
// >2048 verts per base) and collapsed the whole 3D scene to the clear color.
// g_max_array_verts is therefore an OPTIONAL DEBUG CEILING ONLY; 0 (the default)
// means uncapped. Set ED_MAX_ARRAY_VERTS only to deliberately truncate.
static u32 g_max_array_verts = 0u;
static unsigned g_vi_wait_count;
static unsigned g_movie_texture_load_count;
static u64 g_movie_cadence_present_count;
static u64 g_movie_cadence_texframe_changes;
static double g_movie_cadence_start_time;
static s32 g_movie_cadence_last_texframe;
static bool g_movie_cadence_started;

static void* graphics_guest_pointer(CPUState* cpu, u32 address, u32* available) {
    return mmio_guest_pointer(cpu, address, available);
}

static u32 gx_texture_base_bytes(u32 width, u32 height, u32 format) {
    u32 shift_x = 2;
    u32 shift_y = 2;
    switch (format) {
    case 0x00u: // GX_TF_I4
    case 0x08u: // GX_TF_C4
    case 0x0Eu: // GX_TF_CMPR
    case 0x20u: // GX_CTF_R4
    case 0x30u: // GX_CTF_Z4
        shift_x = 3;
        shift_y = 3;
        break;
    case 0x01u: // GX_TF_I8
    case 0x02u: // GX_TF_IA4
    case 0x09u: // GX_TF_C8
    case 0x11u: // GX_TF_Z8
    case 0x22u: // GX_CTF_RA4
    case 0x27u: // GX_CTF_A8
    case 0x28u: // GX_CTF_R8
    case 0x29u: // GX_CTF_G8
    case 0x2Au: // GX_CTF_B8
    case 0x39u: // GX_CTF_Z8M
    case 0x3Au: // GX_CTF_Z8L
        shift_x = 3;
        shift_y = 2;
        break;
    default:
        shift_x = 2;
        shift_y = 2;
        break;
    }

    const u32 tile_x = (width + (1u << shift_x) - 1u) >> shift_x;
    const u32 tile_y = (height + (1u << shift_y) - 1u) >> shift_y;
    const u32 block_bits = (format == 0x06u || format == 0x16u) ? 64u : 32u;
    return block_bits * tile_x * tile_y;
}

static u32 gx_hash_token_mix(u32 hash, u32 value) {
    for (u32 i = 0; i < 4u; i++) {
        hash = (hash ^ (u8)(value >> (i * 8u))) * 16777619u;
    }
    return hash ? hash : 1u;
}

static u32 gx_texture_resource_version(u32 width, u32 height, u32 format,
                                       u32 tlut, u8 flags,
                                       u32 data_bytes, u32 data_hash) {
    u32 hash = 2166136261u;
    hash = gx_hash_token_mix(hash, 0x54455831u);
    hash = gx_hash_token_mix(hash, width);
    hash = gx_hash_token_mix(hash, height);
    hash = gx_hash_token_mix(hash, format);
    hash = gx_hash_token_mix(hash, tlut);
    hash = gx_hash_token_mix(hash, flags);
    hash = gx_hash_token_mix(hash, data_bytes);
    return gx_hash_token_mix(hash, data_hash);
}

static u32 gx_tlut_resource_version(u32 format, u16 entries, u32 data_bytes,
                                    u32 data_hash) {
    u32 hash = 2166136261u;
    hash = gx_hash_token_mix(hash, 0x544C5431u);
    hash = gx_hash_token_mix(hash, format);
    hash = gx_hash_token_mix(hash, entries);
    hash = gx_hash_token_mix(hash, data_bytes);
    return gx_hash_token_mix(hash, data_hash);
}

static bool thp_movie_plane_generation(CPUState* cpu, u32 guest_data,
                                       u32 width, u32 height, u32 format,
                                       u32* data_bytes, u32* data_hash,
                                       u32* cache_object) {
    if (format != 1u)
        return false;

    const u32 base = THP_SIMPLE_CONTROL;
    const u32 y = mem_read32(cpu, base + 0x15Cu);
    const u32 u = mem_read32(cpu, base + 0x160u);
    const u32 v = mem_read32(cpu, base + 0x164u);
    bool matches_plane = false;
    if (guest_data == y && width == 512u && height == 416u)
        matches_plane = true;
    else if ((guest_data == u || guest_data == v) &&
             width == 256u && height == 208u)
        matches_plane = true;
    if (!matches_plane)
        return false;

    const s32 tex_frame = (s32)mem_read32(cpu, base + 0x168u);
    if (tex_frame < 0)
        return false;

    const u32 bytes = gx_texture_base_bytes(width, height, format);
    u32 hash = 2166136261u;
    hash = gx_hash_token_mix(hash, 0x54485031u);
    hash = gx_hash_token_mix(hash, guest_data);
    hash = gx_hash_token_mix(hash, (u32)tex_frame);
    hash = gx_hash_token_mix(hash, width);
    hash = gx_hash_token_mix(hash, height);
    hash = gx_hash_token_mix(hash, format);
    if (data_bytes != NULL)
        *data_bytes = bytes;
    if (data_hash != NULL)
        *data_hash = hash;
    if (cache_object != NULL)
        *cache_object = guest_data;
    return true;
}

static double host_time_seconds(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static u32 thp_audio_valid_samples(CPUState* cpu, u32* output_valid) {
    const u32 base = THP_SIMPLE_CONTROL;
    u32 total = 0;
    for (u32 i = 0; i < 6u; i++)
        total += mem_read32(cpu, base + 0x16Cu + i * 12u + 8u);

    if (output_valid != NULL) {
        const u32 output_index = mem_read32(cpu, base + 0x1B8u);
        *output_valid = output_index < 6u
            ? mem_read32(cpu, base + 0x16Cu + output_index * 12u + 8u)
            : 0u;
    }
    return total;
}

static void movie_cadence_present(CPUState* cpu) {
    if (!g_movie_cadence_log)
        return;

    const u32 base = THP_SIMPLE_CONTROL;
    if (mem_read8(cpu, base + 0x6Cu) == 0u) {
        g_movie_cadence_started = false;
        return;
    }

    const s32 tex_frame = (s32)mem_read32(cpu, base + 0x168u);
    const double now = host_time_seconds();
    if (!g_movie_cadence_started) {
        g_movie_cadence_started = true;
        g_movie_cadence_start_time = now;
        g_movie_cadence_present_count = 0;
        g_movie_cadence_texframe_changes = 0;
        g_movie_cadence_last_texframe = tex_frame;
    }

    g_movie_cadence_present_count++;
    if (tex_frame != g_movie_cadence_last_texframe) {
        g_movie_cadence_texframe_changes++;
        g_movie_cadence_last_texframe = tex_frame;
    }

    const double elapsed = now - g_movie_cadence_start_time;
    if (elapsed >= 1.0) {
        u32 audio_output_valid = 0;
        const u32 audio_valid = thp_audio_valid_samples(cpu, &audio_output_valid);
        fprintf(stderr,
                "[movie-cadence] seconds=%.3f presents=%llu "
                "texframe_changes=%llu last_texframe=%d audio_dec=%u "
                "audio_out=%u audio_valid=%u audio_out_valid=%u\n",
                elapsed,
                (unsigned long long)g_movie_cadence_present_count,
                (unsigned long long)g_movie_cadence_texframe_changes,
                g_movie_cadence_last_texframe,
                mem_read32(cpu, base + 0x1B4u),
                mem_read32(cpu, base + 0x1B8u),
                audio_valid,
                audio_output_valid);
        g_movie_cadence_start_time = now;
        g_movie_cadence_present_count = 0;
        g_movie_cadence_texframe_changes = 0;
    }
}

static void guest_write_f32(CPUState* cpu, u32 address, f32 value) {
    u32 bits;
    memcpy(&bits, &value, sizeof bits);
    mem_write32(cpu, address, bits);
}

// PSMTXConcat is pure SDK math, but its paired-single implementation is used
// on virtually every rendered packet. Keep a portable scalar implementation at
// the HLE boundary so host floating-point behavior cannot corrupt transforms.
static void hle_PSMTXConcat(CPUState* cpu) {
    const u32 a_address = hle_arg_u32(cpu, 0);
    const u32 b_address = hle_arg_u32(cpu, 1);
    const u32 out_address = hle_arg_u32(cpu, 2);
    f32 a[12], b[12], out[12];

    for (u32 i = 0; i < 12; i++) {
        a[i] = guest_read_f32(cpu, a_address + i * 4u);
        b[i] = guest_read_f32(cpu, b_address + i * 4u);
    }
    for (u32 row = 0; row < 3; row++) {
        for (u32 col = 0; col < 3; col++) {
            out[row * 4u + col] =
                (f32)((f32)(a[row * 4u] * b[col]) +
                      (f32)(a[row * 4u + 1u] * b[4u + col]) +
                      (f32)(a[row * 4u + 2u] * b[8u + col]));
        }
        out[row * 4u + 3u] =
            (f32)(a[row * 4u + 3u] +
                  (f32)(a[row * 4u] * b[3u]) +
                  (f32)(a[row * 4u + 1u] * b[7u]) +
                  (f32)(a[row * 4u + 2u] * b[11u]));
    }
    for (u32 i = 0; i < 12; i++)
        guest_write_f32(cpu, out_address + i * 4u, out[i]);
}

// GXSetArray(GXAttr attr, void* base, u8 stride). Replace the hardware CP-base
// write with Aurora's extended command carrying a real host pointer.
// While the game records a display list (the CPU FIFO points at its buffer,
// not the GPU), the ED_GX_HLE shortcuts below step aside so the real SDK
// functions write their real commands into the list: the shortcuts feed the
// renderer directly and would leave the recorded list short of its commands.
static bool gx_recording_display_list(void) {
    if (mmio_cpu_fifo_feeds_gpu())
        return false;
    g_hle_passthrough = true;
    return true;
}

static void hle_GXSetArray(CPUState* cpu) {
    if (gx_recording_display_list())
        return;
    u32 attr = hle_arg_u32(cpu, 0);
    u32 address = hle_arg_u32(cpu, 1);
    u8 stride = (u8)hle_arg_u32(cpu, 2);
    u32 available = 0;
    void* data = graphics_guest_pointer(cpu, address, &available);
    // GXSetArray carries no length on real hardware, but Aurora's FIFO path
    // uploads the whole advertised array into a fixed per-frame storage buffer
    // (8 MB) every time the array is used, once per distinct base. The game
    // re-bases the position/color/texcoord arrays per object and indexes locally
    // from each base, so a single frame can bind dozens of arrays; advertising
    // the 16-bit index ceiling, or even stride*8192, summed past 8 MB once enough
    // character models were on screen (e.g. both team captains on the VS screen)
    // and aborted Aurora's non-owned storage ByteBuffer. Bound the advertised
    // range to a realistic per-mesh vertex count: small enough to keep the total
    // far under 8 MB, large enough to cover every index a real draw references.
    // Advertise the full resolvable range; Aurora's span patch uploads only the
    // real per-draw indexed span, so this never inflates storage and never
    // truncates geometry. An explicit ED_MAX_ARRAY_VERTS (g_max_array_verts
    // != 0) applies a debug ceiling only.
    u32 size = available;
    if (g_max_array_verts != 0u) {
        u64 indexed_span = (u64)(stride ? stride : 1u) * g_max_array_verts;
        if (indexed_span < (u64)size)
            size = (u32)indexed_span;
    }
    if (attr < 32u) {
        g_array_address[attr] = address;
        g_array_stride[attr] = stride;
    }
    if (g_graphics_log && (++g_gx_array_count <= 24 || size > 500000u))
        fprintf(stderr, "[gfx] array attr=%u guest=0x%08X host=%p size=%u stride=%u available=%u\n",
                attr, address, data, size, stride, available);
    g_frame_array_bytes += size;
    g_frame_array_calls++;
    // The advertised-byte budget warn modelled the OLD whole-array upload; with
    // the span patch the real upload is per-draw span-bounded, so this only makes
    // sense (and only risks false positives) when an explicit cap is set.
    if (g_max_array_verts != 0u &&
        g_frame_array_bytes > ARRAY_UPLOAD_WARN_BYTES && !g_array_budget_warned) {
        g_array_budget_warned = true;
        fprintf(stderr,
                "[gfx] WARNING: indexed-array uploads this frame reached %llu bytes, "
                "nearing Aurora's %u-byte per-frame storage limit. Lower "
                "ED_MAX_ARRAY_VERTS (currently %u) if this scene aborts.\n",
                (unsigned long long)g_frame_array_bytes,
                (unsigned)AURORA_STORAGE_BUDGET, g_max_array_verts);
    }
    dol_platform_set_array_guest(attr, address, data, size, stride);
}

static void emit_gx_resource_metadata(CPUState* cpu);

static void finish_GXCallDisplayList(CPUState* cpu) {
    u32 available = 0;
    void* data = graphics_guest_pointer(
        cpu, g_display_list_call.address, &available);
    if (data == NULL || g_display_list_call.size > available) {
        fprintf(stderr,
                "[gx] GXCallDisplayList unresolved address=0x%08X size=%u "
                "available=%u\n",
                g_display_list_call.address, g_display_list_call.size,
                available);
    } else {
        emit_gx_resource_metadata(cpu);
        mmio_fifo_dump_note("call-display-list", g_display_list_call.address,
                            g_display_list_call.size);
        dol_platform_call_display_list(data, g_display_list_call.size);
    }
    cpu->lr = g_display_list_call.caller;
    cpu->pc = g_display_list_call.caller;
    g_display_list_call.active = false;
}

static void continue_GXCallDisplayList(CPUState* cpu) {
    const u32 gx_data =
        mem_read32(cpu, cpu->gpr[ED_GX_DATA_BASE_REG] + (u32)(s32)GX_DATA_SDA_OFFSET);
    if (g_display_list_call.stage == 0u &&
        mem_read32(cpu, gx_data + GX_DIRTY_STATE_OFFSET) != 0u) {
        g_display_list_call.stage = 1u;
        cpu->lr = HLE_DISPLAY_LIST_RETURN;
        cpu->pc = g_gx_dirty_state_helper;
        return;
    }
    if (g_display_list_call.stage <= 1u &&
        ((mem_read32(cpu, gx_data) == 0u) == (ED_GX_FLUSH_PRIM_WHEN_ZERO != 0u))) {
        g_display_list_call.stage = 2u;
        cpu->lr = HLE_DISPLAY_LIST_RETURN;
        cpu->pc = g_gx_flush_prim_helper;
        return;
    }
    finish_GXCallDisplayList(cpu);
}

static void hle_GXCallDisplayList(CPUState* cpu) {
    if (gx_recording_display_list())
        return;
    if (g_display_list_call.active) {
        fprintf(stderr, "[gx] nested GXCallDisplayList host continuation\n");
        return;
    }
    g_display_list_call.active = true;
    g_display_list_call.stage = 0u;
    g_display_list_call.caller = cpu->lr;
    g_display_list_call.address = hle_arg_u32(cpu, 0);
    g_display_list_call.size = hle_arg_u32(cpu, 1);
    g_hle_tail_call = true;
    continue_GXCallDisplayList(cpu);
}

static void notify_GXBegin(CPUState* cpu);

static void finish_GXBegin(CPUState* cpu) {
    emit_gx_resource_metadata(cpu);
    dol_platform_mark_gx_begin();
    mem_write8(cpu, 0xCC008000u,
               (u8)(g_gx_begin.primitive | g_gx_begin.vtxfmt));
    mem_write16(cpu, 0xCC008000u, g_gx_begin.vertex_count);
    cpu->lr = g_gx_begin.caller;
    cpu->pc = g_gx_begin.caller;
    g_gx_begin.active = false;
}

static void continue_GXBegin(CPUState* cpu) {
    const u32 gx_data =
        mem_read32(cpu, cpu->gpr[ED_GX_DATA_BASE_REG] + (u32)(s32)GX_DATA_SDA_OFFSET);
    if (g_gx_begin.stage == 0u &&
        mem_read32(cpu, gx_data + GX_DIRTY_STATE_OFFSET) != 0u) {
        g_gx_begin.stage = 1u;
        cpu->lr = HLE_GX_BEGIN_RETURN;
        cpu->pc = g_gx_dirty_state_helper;
        return;
    }
    if (g_gx_begin.stage <= 1u && ((mem_read32(cpu, gx_data) == 0u) == (ED_GX_FLUSH_PRIM_WHEN_ZERO != 0u))) {
        g_gx_begin.stage = 2u;
        cpu->lr = HLE_GX_BEGIN_RETURN;
        cpu->pc = g_gx_flush_prim_helper;
        return;
    }
    finish_GXBegin(cpu);
}

static void hle_GXBegin(CPUState* cpu) {
    if (gx_recording_display_list())
        return;
    if (g_gx_begin.active) {
        fprintf(stderr, "[gx] nested GXBegin host continuation\n");
        cpu->pc = cpu->lr;
        return;
    }
    notify_GXBegin(cpu);
    g_gx_begin.active = true;
    g_gx_begin.stage = 0u;
    g_gx_begin.primitive = (u8)hle_arg_u32(cpu, 0);
    g_gx_begin.vtxfmt = (u8)hle_arg_u32(cpu, 1);
    g_gx_begin.vertex_count = (u16)hle_arg_u32(cpu, 2);
    g_gx_begin.caller = cpu->lr;
    g_hle_tail_call = true;
    continue_GXBegin(cpu);
}

static void record_GXLoadTexObj(CPUState* cpu, u32 obj, u32 slot) {
    static unsigned texture_trace_count = 0;
    static bool texture_watch_initialized = false;
    static u32 texture_watch_address = 0;
    static unsigned texture_watch_begin = UINT_MAX;
    if (slot >= GX_MAX_TEXTURES)
        return;
    u32 image0 = mem_read32(cpu, obj + 8u);
    u32 image3 = mem_read32(cpu, obj + 12u);
    u32 guest_data = GC_RAM_BASE | ((image3 & 0x001FFFFFu) << 5);
    u32 width = (image0 & 0x3FFu) + 1u;
    u32 height = ((image0 >> 10) & 0x3FFu) + 1u;
    u32 format = mem_read32(cpu, obj + 20u);
    u32 tlut = mem_read32(cpu, obj + 24u);
    u8 flags = mem_read8(cpu, obj + 31u);
    u32 data_bytes = gx_texture_base_bytes(width, height, format);
    u32 data_hash = 0;
    u32 cache_object = obj;
    const bool movie_generation =
        thp_movie_plane_generation(cpu, guest_data, width, height, format,
                                   &data_bytes, &data_hash, &cache_object);
    if (!movie_generation)
        data_hash = guest_hash(cpu, guest_data, data_bytes, &data_bytes);
    const u32 version =
        gx_texture_resource_version(width, height, format, tlut, flags,
                                    data_bytes, data_hash);
    if (!movie_generation)
        cache_object = version;
    if (!texture_watch_initialized) {
        const char* watch = getenv("ED_TEXTURE_WATCH");
        texture_watch_address =
            watch != NULL ? (u32)strtoul(watch, NULL, 0) : 0u;
        texture_watch_initialized = true;
    }
    if (guest_data == texture_watch_address &&
        texture_watch_begin != g_gx_begin_count) {
        texture_watch_begin = g_gx_begin_count;
        fprintf(stderr,
                "[texture-watch] begin=%u slot=%u obj=%08X data=%08X "
                "%ux%u fmt=%u flags=%02X hash=%08X id=%08X version=%08X\n",
                g_gx_begin_count, slot, obj, guest_data, width, height, format,
                flags, data_hash, cache_object, version);
    }
    if (g_mash_route_complete && getenv("ED_TEXTURE_CACHE_LOG") != NULL &&
        texture_trace_count < 256u) {
        ++texture_trace_count;
        fprintf(stderr,
                "[texture-cache] load=%u begin=%u slot=%u obj=%08X data=%08X "
                "%ux%u fmt=%u tlut=%u flags=%02X bytes=%u hash=%08X "
                "id=%08X version=%08X movie=%u\n",
                texture_trace_count, g_gx_begin_count, slot, obj, guest_data,
                width, height, format, tlut, flags, data_bytes, data_hash,
                cache_object, version, movie_generation ? 1u : 0u);
    }
    GxTextureBinding* binding = &g_textures[slot];
    bool changed = !binding->valid ||
                   binding->object != cache_object ||
                   binding->version != version;
    binding->valid = true;
    binding->object = cache_object;
    binding->guest_data = guest_data;
    binding->width = width;
    binding->height = height;
    binding->format = format;
    binding->tlut = tlut;
    binding->flags = flags;
    binding->data_bytes = data_bytes;
    binding->data_hash = data_hash;
    binding->version = version;
    // The retail BP texture-load sequence clears Aurora's recomp metadata.
    // Re-emit the stable identity after every guest bind, even when the bytes
    // are unchanged, so the renderer can hit its converted-texture cache.
    binding->emitted_version = 0;
    if (g_movie_log && format == 1u) {
        unsigned seq = ++g_movie_texobj_log_count;
        bool should_log = changed || seq <= 96u || (seq % 60u) == 0u;
        if (should_log) {
        fprintf(stderr,
                "[movie] GXLoadTexObj slot=%u obj=0x%08X data=0x%08X "
                "%ux%u fmt=%u bytes=%u hash=%08X changed=%u ver=%u seq=%u "
                "gen=%u cache=0x%08X lr=0x%08X\n",
                slot, obj, guest_data, width, height, format, data_bytes,
                data_hash, changed ? 1u : 0u, binding->version, seq,
                movie_generation ? 1u : 0u, cache_object, cpu->lr);
        }
    }
}

#endif

static bool ball_state_log_enabled(void) {
    static int enabled = -1;
    if (enabled < 0)
        enabled = getenv("ED_BALL_STATE_LOG") != NULL ? 1 : 0;
    return enabled != 0;
}

typedef struct {
    u64 quick_step;
    unsigned position_sets;
    unsigned velocity_sets;
    bool quick_entry_finite;
    bool quick_entry_valid;
    bool first_failure_reported;
} BallPhysicsProbe;

static BallPhysicsProbe g_ball_physics_probe;

typedef struct {
    bool active;
    bool interesting;
    u64 quick_step;
    u32 m;
    u32 nb;
    u32 j;
    u32 jb;
    u32 bodies;
    u32 inv_i;
    u32 lambda;
    u32 cforce;
    u32 rhs;
    u32 lo;
    u32 hi;
    u32 cfm;
    u32 findex;
    s32 ball_body_index;
} BallSorProbe;

static BallSorProbe g_ball_sor_probe;

typedef struct {
    u32 body;
    bool finite_seen;
    bool failure_reported;
} PhysicsBodyProbeEntry;

#define PHYSICS_BODY_PROBE_CAPACITY 256u
static PhysicsBodyProbeEntry
    g_physics_body_probes[PHYSICS_BODY_PROBE_CAPACITY];
static unsigned g_physics_body_probe_count;
static bool g_physics_sor_failure_reported;
static bool g_physics_set_failure_reported;

static void report_ball_physics_failure(CPUState* cpu, const char* boundary);

static bool guest_ram_range_valid(const CPUState* cpu, u32 address, u32 size) {
    if (address < GC_RAM_BASE || size > cpu->ram_size)
        return false;
    return address - GC_RAM_BASE <= cpu->ram_size - size;
}

static bool guest_vec3_finite(CPUState* cpu, u32 address) {
    return isfinite(guest_read_f32(cpu, address)) &&
           isfinite(guest_read_f32(cpu, address + 4u)) &&
           isfinite(guest_read_f32(cpu, address + 8u));
}

static bool guest_vec4_finite(CPUState* cpu, u32 address) {
    return guest_vec3_finite(cpu, address) &&
           isfinite(guest_read_f32(cpu, address + 12u));
}

static bool physics_body_finite(CPUState* cpu, u32 body) {
    return guest_vec3_finite(cpu, body + 0x98u) &&
           guest_vec4_finite(cpu, body + 0xA8u) &&
           guest_vec3_finite(cpu, body + 0xB8u) &&
           guest_vec3_finite(cpu, body + 0xC8u) &&
           guest_vec3_finite(cpu, body + 0xD8u) &&
           guest_vec3_finite(cpu, body + 0xE8u) &&
           guest_vec3_finite(cpu, body + 0xF8u) &&
           guest_vec3_finite(cpu, body + 0x108u) &&
           guest_vec3_finite(cpu, body + 0x118u);
}

static void log_physics_body(CPUState* cpu, const char* phase, u32 body) {
    const u32 object = mem_read32(cpu, body + 0x0Cu);
    const u32 vtable = guest_ram_range_valid(cpu, object, 0x94u)
                           ? mem_read32(cpu, object)
                           : 0u;
    const u32 parent = guest_ram_range_valid(cpu, object, 0x94u)
                           ? mem_read32(cpu, object + 0x0Cu)
                           : 0u;
    const u32 ai_character =
        guest_ram_range_valid(cpu, object, 0x94u)
            ? mem_read32(cpu, object + 0x8Cu)
            : 0u;
    fprintf(stderr,
            "[physics-body-first-failure] phase=%s step=%llu "
            "guest-frame=%llu body=0x%08X object=0x%08X "
            "vtable=0x%08X parent=0x%08X ai=0x%08X flags=0x%08X "
            "joint=0x%08X pc=0x%08X lr=0x%08X\n"
            "  pos=% .7g,% .7g,% .7g q=% .7g,% .7g,% .7g,% .7g\n"
            "  vel=% .7g,% .7g,% .7g avel=% .7g,% .7g,% .7g "
            "force=% .7g,% .7g,% .7g torque=% .7g,% .7g,% .7g\n",
            phase, (unsigned long long)g_ball_physics_probe.quick_step,
            (unsigned long long)(cpu->timebase / 675000ull), body, object,
            vtable, parent, ai_character, mem_read32(cpu, body + 0x18u),
            mem_read32(cpu, body + 0x14u), cpu->pc, cpu->lr,
            (double)guest_read_f32(cpu, body + 0x98u),
            (double)guest_read_f32(cpu, body + 0x9Cu),
            (double)guest_read_f32(cpu, body + 0xA0u),
            (double)guest_read_f32(cpu, body + 0xA8u),
            (double)guest_read_f32(cpu, body + 0xACu),
            (double)guest_read_f32(cpu, body + 0xB0u),
            (double)guest_read_f32(cpu, body + 0xB4u),
            (double)guest_read_f32(cpu, body + 0xE8u),
            (double)guest_read_f32(cpu, body + 0xECu),
            (double)guest_read_f32(cpu, body + 0xF0u),
            (double)guest_read_f32(cpu, body + 0xF8u),
            (double)guest_read_f32(cpu, body + 0xFCu),
            (double)guest_read_f32(cpu, body + 0x100u),
            (double)guest_read_f32(cpu, body + 0x108u),
            (double)guest_read_f32(cpu, body + 0x10Cu),
            (double)guest_read_f32(cpu, body + 0x110u),
            (double)guest_read_f32(cpu, body + 0x118u),
            (double)guest_read_f32(cpu, body + 0x11Cu),
            (double)guest_read_f32(cpu, body + 0x120u));
}

static void observe_physics_body(CPUState* cpu, const char* phase, u32 body) {
    if (!ball_state_log_enabled() ||
        !guest_ram_range_valid(cpu, body, 0x128u))
        return;
    PhysicsBodyProbeEntry* entry = NULL;
    for (unsigned i = 0; i < g_physics_body_probe_count; ++i) {
        if (g_physics_body_probes[i].body == body) {
            entry = &g_physics_body_probes[i];
            break;
        }
    }
    if (entry == NULL &&
        g_physics_body_probe_count < PHYSICS_BODY_PROBE_CAPACITY) {
        entry = &g_physics_body_probes[g_physics_body_probe_count++];
        entry->body = body;
        entry->finite_seen = false;
        entry->failure_reported = false;
    }
    if (entry == NULL)
        return;
    const bool finite = physics_body_finite(cpu, body);
    if (finite) {
        entry->finite_seen = true;
        return;
    }
    if (!entry->failure_reported) {
        entry->failure_reported = true;
        log_physics_body(cpu, phase, body);
    }
}

static void observe_physics_world(CPUState* cpu, const char* phase, u32 world) {
    if (!ball_state_log_enabled() ||
        !guest_ram_range_valid(cpu, world, 4u))
        return;
    u32 body = mem_read32(cpu, world);
    for (unsigned count = 0; body != 0u && count < 1024u; ++count) {
        if (!guest_ram_range_valid(cpu, body, 0x128u))
            break;
        observe_physics_body(cpu, phase, body);
        body = mem_read32(cpu, body + 4u);
    }
}

static void log_nonfinite_body_set(CPUState* cpu, const char* kind, u32 body,
                                   f64 x, f64 y, f64 z) {
    if ((isfinite(x) && isfinite(y) && isfinite(z)) ||
        g_physics_set_failure_reported)
        return;
    g_physics_set_failure_reported = true;
    fprintf(stderr,
            "[physics-body-set-nonfinite] kind=%s step=%llu body=0x%08X "
            "object=0x%08X value=% .9g,% .9g,% .9g pc=0x%08X lr=0x%08X\n",
            kind, (unsigned long long)g_ball_physics_probe.quick_step, body,
            guest_ram_range_valid(cpu, body, 20u)
                ? mem_read32(cpu, body + 0x0Cu)
                : 0u,
            x, y, z, cpu->pc, cpu->lr);
}

static void log_character_force_write(CPUState* cpu, const char* kind) {
    if (!ball_state_log_enabled() ||
        g_ball_physics_probe.quick_step < 252u ||
        g_ball_physics_probe.quick_step > 256u ||
        !guest_ram_range_valid(cpu, cpu->gpr[3], 0x128u))
        return;
    const u32 body = cpu->gpr[3];
    const u32 object = mem_read32(cpu, body + 0x0Cu);
    if (!guest_ram_range_valid(cpu, object, 0x94u) ||
        mem_read32(cpu, object) != 0x802AFA18u)
        return;
    fprintf(stderr,
            "[physics-force-write] kind=%s step=%llu body=0x%08X "
            "object=0x%08X ai=0x%08X value=% .9g,% .9g,% .9g "
            "before=% .9g,% .9g,% .9g lr=0x%08X\n",
            kind, (unsigned long long)g_ball_physics_probe.quick_step, body,
            object, mem_read32(cpu, object + 0x8Cu), cpu->fpr[1],
            cpu->fpr[2], cpu->fpr[3],
            (double)guest_read_f32(cpu, body + 0x108u),
            (double)guest_read_f32(cpu, body + 0x10Cu),
            (double)guest_read_f32(cpu, body + 0x110u), cpu->lr);
}

static bool ball_physics_addresses(CPUState* cpu, u32* ball_out,
                                   u32* physics_out, u32* body_out) {
    const u32 ball = mem_read32(cpu, 0x80373664u); // g_pBall
    if (!guest_ram_range_valid(cpu, ball, 0xACu))
        return false;
    const u32 physics = mem_read32(cpu, ball + 0x38u);
    if (!guest_ram_range_valid(cpu, physics, 0x5Cu))
        return false;
    const u32 body = mem_read32(cpu, physics + 4u);
    if (!guest_ram_range_valid(cpu, body, 0x128u))
        return false;
    if (ball_out != NULL)
        *ball_out = ball;
    if (physics_out != NULL)
        *physics_out = physics;
    if (body_out != NULL)
        *body_out = body;
    return true;
}

static bool ball_physics_finite(CPUState* cpu, u32 ball, u32 physics,
                                u32 body) {
    return guest_vec3_finite(cpu, ball + 0x40u) &&
           guest_vec3_finite(cpu, physics + 0x14u) &&
           guest_vec3_finite(cpu, physics + 0x20u) &&
           guest_vec3_finite(cpu, body + 0x98u) &&
           guest_vec3_finite(cpu, body + 0xE8u) &&
           guest_vec3_finite(cpu, body + 0xF8u) &&
           guest_vec3_finite(cpu, body + 0x108u) &&
           guest_vec3_finite(cpu, body + 0x118u);
}

static bool log_ball_physics_probe(CPUState* cpu, const char* phase,
                                   bool periodic) {
    u32 ball;
    u32 physics;
    u32 body;
    if (!ball_state_log_enabled() ||
        !ball_physics_addresses(cpu, &ball, &physics, &body))
        return true;

    const bool finite = ball_physics_finite(cpu, ball, physics, body);
    if (!periodic && finite)
        return true;
    if (!periodic && g_ball_physics_probe.first_failure_reported)
        return false;

    fprintf(stderr,
            "[ball-physics] phase=%s step=%llu guest-frame=%llu pc=0x%08X "
            "lr=0x%08X finite=%u ball=0x%08X physics=0x%08X "
            "body=0x%08X owner=0x%08X damage=%u target=0x%08X "
            "body-flags=0x%08X joint=0x%08X\n"
            "  sim=% .7g,% .7g,% .7g cache-pos=% .7g,% .7g,% .7g "
            "cache-vel=% .7g,% .7g,% .7g\n"
            "  ode-pos=% .7g,% .7g,% .7g ode-vel=% .7g,% .7g,% .7g "
            "ode-avel=% .7g,% .7g,% .7g\n"
            "  force=% .7g,% .7g,% .7g torque=% .7g,% .7g,% .7g\n",
            phase, (unsigned long long)g_ball_physics_probe.quick_step,
            (unsigned long long)(cpu->timebase / 675000ull), cpu->pc, cpu->lr,
            finite ? 1u : 0u, ball, physics, body,
            mem_read32(cpu, ball + 0x24u),
            (unsigned)mem_read8(cpu, ball + 0xA6u),
            mem_read32(cpu, ball + 0xA8u), mem_read32(cpu, body + 0x18u),
            mem_read32(cpu, body + 0x14u),
            (double)guest_read_f32(cpu, ball + 0x40u),
            (double)guest_read_f32(cpu, ball + 0x44u),
            (double)guest_read_f32(cpu, ball + 0x48u),
            (double)guest_read_f32(cpu, physics + 0x14u),
            (double)guest_read_f32(cpu, physics + 0x18u),
            (double)guest_read_f32(cpu, physics + 0x1Cu),
            (double)guest_read_f32(cpu, physics + 0x20u),
            (double)guest_read_f32(cpu, physics + 0x24u),
            (double)guest_read_f32(cpu, physics + 0x28u),
            (double)guest_read_f32(cpu, body + 0x98u),
            (double)guest_read_f32(cpu, body + 0x9Cu),
            (double)guest_read_f32(cpu, body + 0xA0u),
            (double)guest_read_f32(cpu, body + 0xE8u),
            (double)guest_read_f32(cpu, body + 0xECu),
            (double)guest_read_f32(cpu, body + 0xF0u),
            (double)guest_read_f32(cpu, body + 0xF8u),
            (double)guest_read_f32(cpu, body + 0xFCu),
            (double)guest_read_f32(cpu, body + 0x100u),
            (double)guest_read_f32(cpu, body + 0x108u),
            (double)guest_read_f32(cpu, body + 0x10Cu),
            (double)guest_read_f32(cpu, body + 0x110u),
            (double)guest_read_f32(cpu, body + 0x118u),
            (double)guest_read_f32(cpu, body + 0x11Cu),
            (double)guest_read_f32(cpu, body + 0x120u));
    return finite;
}

static void log_ball_contact_joints(CPUState* cpu, const char* phase,
                                    u32 body) {
    u32 node = mem_read32(cpu, body + 0x14u);
    for (unsigned index = 0; index < 12u; ++index) {
        if (!guest_ram_range_valid(cpu, node, 12u))
            break;
        const u32 joint = mem_read32(cpu, node);
        const u32 other_body = mem_read32(cpu, node + 4u);
        const u32 next = mem_read32(cpu, node + 8u);
        if (!guest_ram_range_valid(cpu, joint, 0xBCu))
            break;
        const u32 vtable = mem_read32(cpu, joint + 0x14u);
        const u32 type = guest_ram_range_valid(cpu, vtable, 20u)
                             ? mem_read32(cpu, vtable + 0x10u)
                             : ~0u;
        const u32 other_object =
            guest_ram_range_valid(cpu, other_body, 20u)
                ? mem_read32(cpu, other_body + 0x0Cu)
                : 0u;
        fprintf(stderr,
                "[ball-contact] phase=%s step=%llu index=%u node=0x%08X "
                "joint=0x%08X type=%u flags=0x%08X other-body=0x%08X "
                "other-object=0x%08X next=0x%08X "
                "lambda=% .7g,% .7g,% .7g,% .7g,% .7g,% .7g\n",
                phase, (unsigned long long)g_ball_physics_probe.quick_step,
                index, node, joint, type, mem_read32(cpu, joint + 0x18u),
                other_body, other_object, next,
                (double)guest_read_f32(cpu, joint + 0x38u),
                (double)guest_read_f32(cpu, joint + 0x3Cu),
                (double)guest_read_f32(cpu, joint + 0x40u),
                (double)guest_read_f32(cpu, joint + 0x44u),
                (double)guest_read_f32(cpu, joint + 0x48u),
                (double)guest_read_f32(cpu, joint + 0x4Cu));
        if (type == 4u) {
            const u32 contact = joint + 0x54u;
            const u32 geom1 = mem_read32(cpu, contact + 0x50u);
            const u32 geom2 = mem_read32(cpu, contact + 0x54u);
            fprintf(stderr,
                    "  contact rows=%d mode=0x%08X mu=% .7g mu2=% .7g "
                    "bounce=% .7g bounce-vel=% .7g erp=% .7g cfm=% .7g "
                    "motion=% .7g,% .7g slip=% .7g,% .7g\n"
                    "  point=% .7g,% .7g,% .7g normal=% .7g,% .7g,% .7g "
                    "depth=% .7g geom=0x%08X(type=%u,data=0x%08X),"
                    "0x%08X(type=%u,data=0x%08X)\n",
                    (s32)mem_read32(cpu, joint + 0x50u),
                    mem_read32(cpu, contact),
                    (double)guest_read_f32(cpu, contact + 4u),
                    (double)guest_read_f32(cpu, contact + 8u),
                    (double)guest_read_f32(cpu, contact + 0x0Cu),
                    (double)guest_read_f32(cpu, contact + 0x10u),
                    (double)guest_read_f32(cpu, contact + 0x14u),
                    (double)guest_read_f32(cpu, contact + 0x18u),
                    (double)guest_read_f32(cpu, contact + 0x1Cu),
                    (double)guest_read_f32(cpu, contact + 0x20u),
                    (double)guest_read_f32(cpu, contact + 0x24u),
                    (double)guest_read_f32(cpu, contact + 0x28u),
                    (double)guest_read_f32(cpu, contact + 0x2Cu),
                    (double)guest_read_f32(cpu, contact + 0x30u),
                    (double)guest_read_f32(cpu, contact + 0x34u),
                    (double)guest_read_f32(cpu, contact + 0x3Cu),
                    (double)guest_read_f32(cpu, contact + 0x40u),
                    (double)guest_read_f32(cpu, contact + 0x44u),
                    (double)guest_read_f32(cpu, contact + 0x4Cu), geom1,
                    guest_ram_range_valid(cpu, geom1, 16u)
                        ? mem_read32(cpu, geom1)
                        : ~0u,
                    guest_ram_range_valid(cpu, geom1, 16u)
                        ? mem_read32(cpu, geom1 + 8u)
                        : 0u,
                    geom2,
                    guest_ram_range_valid(cpu, geom2, 16u)
                        ? mem_read32(cpu, geom2)
                        : ~0u,
                    guest_ram_range_valid(cpu, geom2, 16u)
                        ? mem_read32(cpu, geom2 + 8u)
                        : 0u);
        }
        node = next;
    }
}

static void log_ball_sor_rows(CPUState* cpu, const char* phase) {
    const BallSorProbe* probe = &g_ball_sor_probe;
    if (!probe->active || probe->ball_body_index < 0)
        return;
    fprintf(stderr,
            "[ball-sor] phase=%s step=%llu m=%u nb=%u ball-index=%d "
            "J=0x%08X jb=0x%08X lambda=0x%08X cforce=0x%08X\n",
            phase, (unsigned long long)probe->quick_step, probe->m, probe->nb,
            probe->ball_body_index, probe->j, probe->jb, probe->lambda,
            probe->cforce);
    for (u32 row = 0; row < probe->m; ++row) {
        const s32 b1 = (s32)mem_read32(cpu, probe->jb + row * 8u);
        const s32 b2 = (s32)mem_read32(cpu, probe->jb + row * 8u + 4u);
        if (b1 != probe->ball_body_index && b2 != probe->ball_body_index)
            continue;
        const u32 j = probe->j + row * 48u;
        fprintf(stderr,
                "  row=%u bodies=%d,%d rhs=% .7g lambda=% .7g "
                "lo=% .7g hi=% .7g cfm=% .7g findex=%d\n"
                "    J1=% .7g,% .7g,% .7g | % .7g,% .7g,% .7g\n"
                "    J2=% .7g,% .7g,% .7g | % .7g,% .7g,% .7g\n",
                row, b1, b2, (double)guest_read_f32(cpu, probe->rhs + row * 4u),
                (double)guest_read_f32(cpu, probe->lambda + row * 4u),
                (double)guest_read_f32(cpu, probe->lo + row * 4u),
                (double)guest_read_f32(cpu, probe->hi + row * 4u),
                (double)guest_read_f32(cpu, probe->cfm + row * 4u),
                (s32)mem_read32(cpu, probe->findex + row * 4u),
                (double)guest_read_f32(cpu, j),
                (double)guest_read_f32(cpu, j + 4u),
                (double)guest_read_f32(cpu, j + 8u),
                (double)guest_read_f32(cpu, j + 12u),
                (double)guest_read_f32(cpu, j + 16u),
                (double)guest_read_f32(cpu, j + 20u),
                (double)guest_read_f32(cpu, j + 24u),
                (double)guest_read_f32(cpu, j + 28u),
                (double)guest_read_f32(cpu, j + 32u),
                (double)guest_read_f32(cpu, j + 36u),
                (double)guest_read_f32(cpu, j + 40u),
                (double)guest_read_f32(cpu, j + 44u));
    }
    const u32 force =
        probe->cforce + (u32)probe->ball_body_index * 24u;
    fprintf(stderr,
            "  cforce=% .7g,% .7g,% .7g | % .7g,% .7g,% .7g\n",
            (double)guest_read_f32(cpu, force),
            (double)guest_read_f32(cpu, force + 4u),
            (double)guest_read_f32(cpu, force + 8u),
            (double)guest_read_f32(cpu, force + 12u),
            (double)guest_read_f32(cpu, force + 16u),
            (double)guest_read_f32(cpu, force + 20u));
}

static void notify_SorLcp(CPUState* cpu) {
    memset(&g_ball_sor_probe, 0, sizeof g_ball_sor_probe);
    if (!ball_state_log_enabled())
        return;
    u32 ball;
    u32 body;
    if (!ball_physics_addresses(cpu, &ball, NULL, &body))
        return;

    BallSorProbe* probe = &g_ball_sor_probe;
    probe->m = cpu->gpr[3];
    probe->nb = cpu->gpr[4];
    if (probe->m == 0u || probe->m > 4096u || probe->nb == 0u ||
        probe->nb > 1024u)
        return;
    probe->j = cpu->gpr[5];
    probe->jb = cpu->gpr[6];
    probe->bodies = cpu->gpr[7];
    probe->inv_i = cpu->gpr[8];
    probe->lambda = cpu->gpr[9];
    probe->cforce = cpu->gpr[10];
    probe->rhs = mem_read32(cpu, cpu->gpr[1] + 8u);
    probe->lo = mem_read32(cpu, cpu->gpr[1] + 12u);
    probe->hi = mem_read32(cpu, cpu->gpr[1] + 16u);
    probe->cfm = mem_read32(cpu, cpu->gpr[1] + 20u);
    probe->findex = mem_read32(cpu, cpu->gpr[1] + 24u);
    probe->ball_body_index = -1;
    for (u32 index = 0; index < probe->nb; ++index) {
        if (mem_read32(cpu, probe->bodies + index * 4u) == body) {
            probe->ball_body_index = (s32)index;
            break;
        }
    }
    probe->quick_step = g_ball_physics_probe.quick_step;
    probe->interesting =
        !g_ball_physics_probe.first_failure_reported &&
        probe->quick_step <= 260u &&
        isfinite(guest_read_f32(cpu, ball + 0x44u)) &&
        fabsf(guest_read_f32(cpu, ball + 0x44u)) >= 5.0f;
    probe->active = true;

    if (!g_physics_sor_failure_reported) {
        for (u32 row = 0; row < probe->m; ++row) {
            const u32 j = probe->j + row * 48u;
            bool finite = guest_vec4_finite(cpu, j) &&
                          guest_vec4_finite(cpu, j + 16u) &&
                          guest_vec4_finite(cpu, j + 32u) &&
                          isfinite(guest_read_f32(cpu,
                                                  probe->rhs + row * 4u)) &&
                          isfinite(guest_read_f32(cpu,
                                                  probe->cfm + row * 4u));
            if (finite)
                continue;
            g_physics_sor_failure_reported = true;
            const s32 b1 = (s32)mem_read32(cpu, probe->jb + row * 8u);
            const s32 b2 =
                (s32)mem_read32(cpu, probe->jb + row * 8u + 4u);
            fprintf(stderr,
                    "[physics-sor-invalid-input] step=%llu row=%u "
                    "bodies=%d,%d rhs=% .7g cfm=% .7g\n",
                    (unsigned long long)probe->quick_step, row, b1, b2,
                    (double)guest_read_f32(cpu, probe->rhs + row * 4u),
                    (double)guest_read_f32(cpu, probe->cfm + row * 4u));
            if (b1 >= 0 && (u32)b1 < probe->nb)
                log_physics_body(
                    cpu, "SOR-invalid-input-body-1",
                    mem_read32(cpu, probe->bodies + (u32)b1 * 4u));
            if (b2 >= 0 && (u32)b2 < probe->nb)
                log_physics_body(
                    cpu, "SOR-invalid-input-body-2",
                    mem_read32(cpu, probe->bodies + (u32)b2 * 4u));
            break;
        }
    }
    if (probe->active && probe->interesting)
        log_ball_sor_rows(cpu, "entry");
}

static void notify_SorLcpReturn(CPUState* cpu) {
    if (!g_ball_sor_probe.active)
        return;
    bool ball_force_finite = true;
    if (g_ball_sor_probe.ball_body_index >= 0) {
        const u32 force =
            g_ball_sor_probe.cforce +
            (u32)g_ball_sor_probe.ball_body_index * 24u;
        ball_force_finite = guest_vec3_finite(cpu, force) &&
                            guest_vec3_finite(cpu, force + 12u);
    }
    if (g_ball_sor_probe.interesting || !ball_force_finite)
        log_ball_sor_rows(cpu, "return");
    if (!ball_force_finite)
        report_ball_physics_failure(cpu, "SOR-LCP");
    if (!g_physics_sor_failure_reported) {
        for (u32 index = 0; index < g_ball_sor_probe.nb; ++index) {
            const u32 force = g_ball_sor_probe.cforce + index * 24u;
            if (guest_vec3_finite(cpu, force) &&
                guest_vec3_finite(cpu, force + 12u))
                continue;
            g_physics_sor_failure_reported = true;
            fprintf(stderr,
                    "[physics-sor-invalid-output] step=%llu body-index=%u\n",
                    (unsigned long long)g_ball_sor_probe.quick_step, index);
            log_physics_body(
                cpu, "SOR-invalid-output",
                mem_read32(cpu, g_ball_sor_probe.bodies + index * 4u));
            break;
        }
    }
    g_ball_sor_probe.active = false;
}

static void report_ball_physics_failure(CPUState* cpu, const char* boundary) {
    if (g_ball_physics_probe.first_failure_reported)
        return;
    g_ball_physics_probe.first_failure_reported = true;
    fprintf(stderr,
            "[ball-physics-first-failure] boundary=%s step=%llu "
            "guest-frame=%llu pc=0x%08X lr=0x%08X\n",
            boundary, (unsigned long long)g_ball_physics_probe.quick_step,
            (unsigned long long)(cpu->timebase / 675000ull), cpu->pc, cpu->lr);
}

static void notify_dWorldQuickStep(CPUState* cpu) {
    if (!ball_state_log_enabled())
        return;
    ++g_ball_physics_probe.quick_step;
    observe_physics_world(cpu, "quick-step-entry", cpu->gpr[3]);
    const bool periodic = g_ball_physics_probe.quick_step <= 8u ||
                          (g_ball_physics_probe.quick_step % 60u) == 0u;
    g_ball_physics_probe.quick_entry_finite =
        log_ball_physics_probe(cpu, "quick-step-entry", periodic);
    g_ball_physics_probe.quick_entry_valid = true;
    u32 ball;
    u32 body;
    if (ball_physics_addresses(cpu, &ball, NULL, &body) &&
        isfinite(guest_read_f32(cpu, ball + 0x44u)) &&
        fabsf(guest_read_f32(cpu, ball + 0x44u)) >= 5.0f)
        log_ball_contact_joints(cpu, "quick-step-entry", body);
    if (!g_ball_physics_probe.quick_entry_finite)
        report_ball_physics_failure(cpu, "before-quick-step");
}

static void notify_dxStepBody(CPUState* cpu) {
    observe_physics_body(cpu, "integrator-entry", cpu->gpr[3]);
    u32 body;
    if (!ball_state_log_enabled() ||
        !ball_physics_addresses(cpu, NULL, NULL, &body) ||
        cpu->gpr[3] != body)
        return;
    const bool finite = log_ball_physics_probe(cpu, "integrator-entry", false);
    if (!finite && g_ball_physics_probe.quick_entry_valid &&
        g_ball_physics_probe.quick_entry_finite) {
        log_ball_contact_joints(cpu, "integrator-entry", body);
        report_ball_physics_failure(cpu, "quick-step-solver");
    }
}

static void notify_PhysicsAIBallPostUpdate(CPUState* cpu) {
    if (!ball_state_log_enabled())
        return;
    const bool finite = log_ball_physics_probe(cpu, "ode-post-update", false);
    if (!finite && g_ball_physics_probe.quick_entry_valid &&
        g_ball_physics_probe.quick_entry_finite)
        report_ball_physics_failure(cpu, "quick-step-integration");
}

static void notify_cBallPostPhysicsUpdate(CPUState* cpu) {
    if (!ball_state_log_enabled())
        return;
    const bool finite =
        log_ball_physics_probe(cpu, "game-post-physics-entry", false);
    if (!finite)
        report_ball_physics_failure(cpu, "before-game-copy");
}

static void notify_cBallUpdateOrientation(CPUState* cpu) {
    if (!ball_state_log_enabled())
        return;
    const bool finite =
        log_ball_physics_probe(cpu, "game-update-orientation", false);
    if (!finite)
        report_ball_physics_failure(cpu, "inside-game-copy");
}

static void notify_dBodySetPosition(CPUState* cpu) {
    if (ball_state_log_enabled())
        log_nonfinite_body_set(cpu, "position", cpu->gpr[3], cpu->fpr[1],
                               cpu->fpr[2], cpu->fpr[3]);
    u32 body;
    if (!ball_state_log_enabled() ||
        !ball_physics_addresses(cpu, NULL, NULL, &body) ||
        cpu->gpr[3] != body)
        return;
    ++g_ball_physics_probe.position_sets;
    const bool finite = isfinite(cpu->fpr[1]) && isfinite(cpu->fpr[2]) &&
                        isfinite(cpu->fpr[3]);
    if (finite && g_ball_physics_probe.position_sets > 20u &&
        (g_ball_physics_probe.position_sets % 60u) != 0u)
        return;
    fprintf(stderr,
            "[ball-physics-set] kind=position count=%u step=%llu "
            "value=% .9g,% .9g,% .9g finite=%u lr=0x%08X\n",
            g_ball_physics_probe.position_sets,
            (unsigned long long)g_ball_physics_probe.quick_step,
            cpu->fpr[1], cpu->fpr[2], cpu->fpr[3], finite ? 1u : 0u, cpu->lr);
    if (!finite)
        report_ball_physics_failure(cpu, "dBodySetPosition");
}

static void notify_dBodySetLinearVel(CPUState* cpu) {
    if (ball_state_log_enabled())
        log_nonfinite_body_set(cpu, "linear-velocity", cpu->gpr[3],
                               cpu->fpr[1], cpu->fpr[2], cpu->fpr[3]);
    u32 body;
    if (!ball_state_log_enabled() ||
        !ball_physics_addresses(cpu, NULL, NULL, &body) ||
        cpu->gpr[3] != body)
        return;
    ++g_ball_physics_probe.velocity_sets;
    const bool finite = isfinite(cpu->fpr[1]) && isfinite(cpu->fpr[2]) &&
                        isfinite(cpu->fpr[3]);
    if (finite && g_ball_physics_probe.velocity_sets > 20u &&
        (g_ball_physics_probe.velocity_sets % 60u) != 0u)
        return;
    fprintf(stderr,
            "[ball-physics-set] kind=velocity count=%u step=%llu "
            "value=% .9g,% .9g,% .9g finite=%u lr=0x%08X\n",
            g_ball_physics_probe.velocity_sets,
            (unsigned long long)g_ball_physics_probe.quick_step,
            cpu->fpr[1], cpu->fpr[2], cpu->fpr[3], finite ? 1u : 0u, cpu->lr);
    if (!finite)
        report_ball_physics_failure(cpu, "dBodySetLinearVel");
}

static void notify_dBodySetAngularVel(CPUState* cpu) {
    if (ball_state_log_enabled())
        log_nonfinite_body_set(cpu, "angular-velocity", cpu->gpr[3],
                               cpu->fpr[1], cpu->fpr[2], cpu->fpr[3]);
}

static void notify_dBodySetRotation(CPUState* cpu) {
    if (!ball_state_log_enabled() ||
        !guest_ram_range_valid(cpu, cpu->gpr[4], 44u))
        return;
    const u32 matrix = cpu->gpr[4];
    bool finite = true;
    for (u32 row = 0; row < 3u; ++row)
        finite = finite && guest_vec3_finite(cpu, matrix + row * 16u);
    if (!finite)
        fprintf(stderr,
                "[physics-body-set-nonfinite] kind=rotation step=%llu "
                "body=0x%08X object=0x%08X matrix=0x%08X "
                "pc=0x%08X lr=0x%08X\n",
                (unsigned long long)g_ball_physics_probe.quick_step,
                cpu->gpr[3],
                guest_ram_range_valid(cpu, cpu->gpr[3], 20u)
                    ? mem_read32(cpu, cpu->gpr[3] + 0x0Cu)
                    : 0u,
                matrix, cpu->pc, cpu->lr);
}

static void notify_dBodySetForce(CPUState* cpu) {
    if (!ball_state_log_enabled())
        return;
    log_character_force_write(cpu, "set");
    log_nonfinite_body_set(cpu, "force", cpu->gpr[3], cpu->fpr[1],
                           cpu->fpr[2], cpu->fpr[3]);
}

static void notify_dBodyAddForce(CPUState* cpu) {
    if (!ball_state_log_enabled())
        return;
    log_character_force_write(cpu, "add");
    log_nonfinite_body_set(cpu, "add-force", cpu->gpr[3], cpu->fpr[1],
                           cpu->fpr[2], cpu->fpr[3]);
}

static void notify_PhysicsUpdate(CPUState* cpu) {
    if (guest_ram_range_valid(cpu, cpu->gpr[3], 4u))
        observe_physics_world(cpu, "physics-update-entry",
                              mem_read32(cpu, cpu->gpr[3]));
}

static void notify_PhysicsWorldPreUpdate(CPUState* cpu) {
    if (guest_ram_range_valid(cpu, cpu->gpr[3], 4u))
        observe_physics_world(cpu, "world-pre-update-entry",
                              mem_read32(cpu, cpu->gpr[3]));
}

static void notify_PhysicsWorldUpdate(CPUState* cpu) {
    if (guest_ram_range_valid(cpu, cpu->gpr[3], 4u))
        observe_physics_world(cpu, "world-update-entry",
                              mem_read32(cpu, cpu->gpr[3]));
}

#ifdef ED_AURORA

static bool guest_backchain_contains(CPUState* cpu, u32 start, u32 end) {
    if (cpu->lr >= start && cpu->lr < end)
        return true;
    u32 sp = cpu->gpr[1];
    const u32 ram_end = GC_RAM_BASE + cpu->ram_size;
    for (unsigned frame = 0; frame < 24u; ++frame) {
        if (sp < GC_RAM_BASE || sp + 8u > ram_end)
            break;
        const u32 caller = mem_read32(cpu, sp);
        if (caller <= sp || caller < GC_RAM_BASE || caller + 8u > ram_end)
            break;
        const u32 ret = mem_read32(cpu, caller + 4u);
        if (ret >= start && ret < end)
            return true;
        sp = caller;
    }
    return false;
}

static void notify_DrawableBallRender(CPUState* cpu) {
    if (!ball_state_log_enabled())
        return;
    static unsigned render_count = 0;
    ++render_count;
    if (render_count > 20u && (render_count % 60u) != 0u)
        return;

    const u32 snapshot_ball = cpu->gpr[3];
    const u32 ball = mem_read32(cpu, 0x80373664u); // g_pBall
    const u32 drawable = ball ? mem_read32(cpu, ball + 0x20u) : 0u;
    fprintf(stderr,
            "[ball-state] render=%u guest-frame=%llu snapshot=0x%08X "
            "visible=%u owner-index=%d pos=% .7g,% .7g,% .7g "
            "ball=0x%08X owner=0x%08X sim=% .7g,% .7g,% .7g "
            "drawable=0x%08X world=% .7g,% .7g,% .7g "
            "translation=% .7g,% .7g,% .7g\n",
            render_count,
            (unsigned long long)(cpu->timebase / 675000ull),
            snapshot_ball, (unsigned)mem_read8(cpu, snapshot_ball + 4u),
            (int)(s8)mem_read8(cpu, snapshot_ball + 6u),
            (double)guest_read_f32(cpu, snapshot_ball + 0x18u),
            (double)guest_read_f32(cpu, snapshot_ball + 0x1Cu),
            (double)guest_read_f32(cpu, snapshot_ball + 0x20u),
            ball, ball ? mem_read32(cpu, ball + 0x24u) : 0u,
            ball ? (double)guest_read_f32(cpu, ball + 0x40u) : 0.0,
            ball ? (double)guest_read_f32(cpu, ball + 0x44u) : 0.0,
            ball ? (double)guest_read_f32(cpu, ball + 0x48u) : 0.0,
            drawable,
            drawable ? (double)guest_read_f32(cpu, drawable + 0x34u) : 0.0,
            drawable ? (double)guest_read_f32(cpu, drawable + 0x38u) : 0.0,
            drawable ? (double)guest_read_f32(cpu, drawable + 0x3Cu) : 0.0,
            drawable ? (double)guest_read_f32(cpu, drawable + 0x58u) : 0.0,
            drawable ? (double)guest_read_f32(cpu, drawable + 0x5Cu) : 0.0,
            drawable ? (double)guest_read_f32(cpu, drawable + 0x60u) : 0.0);
}

// GXLoadPosMtxImm(MtxPtr mtx, u32 pnIdx): every 3D actor/camera modelview is
// loaded here. The frame-end gx_modelview snapshot only catches the LAST matrix
// (a 2D/overlay layer), so to tell whether the recomp ever applies the real 3D
// camera tilt we sample the matrices loaded mid-frame. To avoid flooding we log
// only matrices with a meaningful rotation (|row1.z| or |row2.y| > 0.1, the
// camera-tilt elements) plus a per-frame count of how many loads carried tilt.
static unsigned g_posmtx_loads_this_frame = 0;
static unsigned g_posmtx_tilted_this_frame = 0;
static unsigned g_posmtx_logged = 0;
// A thing one player found is tied to the game's camera, and lay across both halves: it is drawn
// for its finder. Its matrices are marked as they are loaded (the renderer's marked_out_of), for the
// other player's view to leave it out. Set at the find (hle_coop_found), gone as its close-up ends.
#define COOP_OBJECT_DRAW 0x8011A278u
#define COOP_OBJECT_DRAW_END 0x8011B400u
static u32 g_coop_found_objects[2];
static int g_coop_found_out_of = -1;  // the view that leaves it out
static unsigned long long g_coop_found_until;
static bool g_coop_found_drawing;
// Whoever a press's scene or look hides (coop_show_asked), with what they hold: out of their own view alone.
#define COOP_UNSEEN_MAX 12
static u32 g_coop_unseen[COOP_UNSEEN_MAX];
static int g_coop_unseen_out_of = -1;
static int g_coop_mark_view;

static void coop_found_object_draw(CPUState* cpu) {
    g_coop_found_drawing = g_coop_found_out_of >= 0 && cpu->gpr[3] != 0u &&
                           (cpu->gpr[3] == g_coop_found_objects[0] || cpu->gpr[3] == g_coop_found_objects[1]);
    g_coop_mark_view = g_coop_found_out_of;
    for (int i = 0; !g_coop_found_drawing && g_coop_unseen_out_of >= 0 && i < COOP_UNSEEN_MAX; i++)
        if (cpu->gpr[3] != 0u && cpu->gpr[3] == g_coop_unseen[i]) {
            g_coop_found_drawing = true;
            g_coop_mark_view = g_coop_unseen_out_of;
        }
    if (g_coop_found_until != 0ull && cpu->timebase / 675000ull >= g_coop_found_until) {
        g_coop_found_until = 0ull;
        g_coop_found_out_of = -1;
        g_coop_found_drawing = false;
    }
}

static void coop_found_mark(CPUState* cpu) {
    const u32 m = cpu->gpr[3];
    if (cpu->lr < COOP_OBJECT_DRAW || cpu->lr >= COOP_OBJECT_DRAW_END || !guest_ram_range_valid(cpu, m, 48u))
        return;
    mem_write8(cpu, m + 15u, 0xD2u);
    mem_write8(cpu, m + 31u, (u8)(0xA0 + g_coop_mark_view));
    mem_write8(cpu, m + 47u, 0xE5u);
}

static void notify_GXLoadPosMtxImm(CPUState* cpu) {
    if (g_coop_found_drawing)
        coop_found_mark(cpu);
    static int matrix_log_enabled = -1;
    if (matrix_log_enabled < 0)
        matrix_log_enabled = getenv("ED_MATRIX_LOG") != NULL ? 1 : 0;
    const bool ball_load =
        ball_state_log_enabled() &&
        guest_backchain_contains(cpu, 0x8011DF10u, 0x8011E38Cu);
    if (!matrix_log_enabled && !ball_load)
        return;
    const u32 mtx_addr = hle_arg_u32(cpu, 0);
    const u32 pn_idx = hle_arg_u32(cpu, 1);
    u8 raw[48];
    copy_guest_to_host(cpu, mtx_addr, raw, sizeof raw);
    float m[12];
    for (int i = 0; i < 12; i++) {
        u32 b = ((u32)raw[i * 4] << 24) | ((u32)raw[i * 4 + 1] << 16) |
                ((u32)raw[i * 4 + 2] << 8) | (u32)raw[i * 4 + 3];
        memcpy(&m[i], &b, sizeof(float));
    }
    if (ball_load) {
        static unsigned ball_matrix_count = 0;
        ++ball_matrix_count;
        if (ball_matrix_count <= 40u || (ball_matrix_count % 60u) == 0u) {
            fprintf(stderr,
                    "[ball-posmtx] load=%u guest-frame=%llu begin=%u "
                    "addr=0x%08X pn=%u lr=0x%08X\n"
                    "  % .7g % .7g % .7g % .7g\n"
                    "  % .7g % .7g % .7g % .7g\n"
                    "  % .7g % .7g % .7g % .7g\n",
                    ball_matrix_count,
                    (unsigned long long)(cpu->timebase / 675000ull),
                    g_gx_begin_count, mtx_addr, pn_idx, cpu->lr,
                    (double)m[0], (double)m[1], (double)m[2], (double)m[3],
                    (double)m[4], (double)m[5], (double)m[6], (double)m[7],
                    (double)m[8], (double)m[9], (double)m[10], (double)m[11]);
        }
    }
    if (!matrix_log_enabled)
        return;
    ++g_posmtx_loads_this_frame;
    // Row1.z = m[6], Row2.y = m[9] carry the camera pitch/tilt rotation.
    const float tilt = (m[6] < 0 ? -m[6] : m[6]) + (m[9] < 0 ? -m[9] : m[9]);
    if (tilt > 0.1f) {
        ++g_posmtx_tilted_this_frame;
        if (g_posmtx_logged < 60u) {
            ++g_posmtx_logged;
            fprintf(stderr,
                    "[posmtx] id=%u TILTED:\n  % .5g % .5g % .5g % .5g\n"
                    "  % .5g % .5g % .5g % .5g\n  % .5g % .5g % .5g % .5g\n",
                    pn_idx, (double)m[0], (double)m[1], (double)m[2], (double)m[3],
                    (double)m[4], (double)m[5], (double)m[6], (double)m[7],
                    (double)m[8], (double)m[9], (double)m[10], (double)m[11]);
        }
    }
}

static void notify_GXLoadTexObj(CPUState* cpu) {
    record_GXLoadTexObj(cpu, hle_arg_u32(cpu, 0), hle_arg_u32(cpu, 1));
}

static void notify_GXLoadTlut(CPUState* cpu) {
    u32 slot = hle_arg_u32(cpu, 1);
    if (slot >= GX_MAX_TLUTS)
        return;
    u32 obj = hle_arg_u32(cpu, 0);
    u32 load_tlut0 = mem_read32(cpu, obj + 4u);
    u32 guest_data = GC_RAM_BASE | ((load_tlut0 & 0x001FFFFFu) << 5);
    u32 format = (mem_read32(cpu, obj) >> 10) & 3u;
    u16 entries = mem_read16(cpu, obj + 8u);
    u32 data_bytes = (u32)entries * 2u;
    u32 data_hash = guest_hash(cpu, guest_data, data_bytes, &data_bytes);
    const u32 version =
        gx_tlut_resource_version(format, entries, data_bytes, data_hash);
    GxTlutBinding* binding = &g_tluts[slot];
    binding->valid = true;
    binding->object = version;
    binding->guest_data = guest_data;
    binding->format = format;
    binding->entries = entries;
    binding->data_bytes = data_bytes;
    binding->data_hash = data_hash;
    binding->version = version;
    binding->emitted_version = 0;
}

static void emit_gx_resource_metadata(CPUState* cpu) {
    for (u32 slot = 0; slot < GX_MAX_TLUTS; slot++) {
        GxTlutBinding* binding = &g_tluts[slot];
        if (!binding->valid)
            continue;
        if (binding->emitted_version == binding->version)
            continue;
        void* data = graphics_guest_pointer(cpu, binding->guest_data, NULL);
        dol_platform_load_tlut_guest((u8)slot, binding->guest_data, data,
                                     binding->format, binding->entries,
                                     binding->object, binding->version);
        binding->emitted_version = binding->version;
    }

    for (u32 slot = 0; slot < GX_MAX_TEXTURES; slot++) {
        GxTextureBinding* binding = &g_textures[slot];
        if (!binding->valid)
            continue;
        if (binding->emitted_version == binding->version)
            continue;
        void* data = graphics_guest_pointer(cpu, binding->guest_data, NULL);
        if (g_graphics_log && g_gx_begin_count == 1 && data != NULL) {
            u64 pixels = (u64)binding->width * binding->height;
            u32 bytes = (binding->format == 0u || binding->format == 8u ||
                         binding->format == 14u)
                            ? (u32)((pixels + 1u) / 2u)
                            : (binding->format == 1u || binding->format == 2u ||
                               binding->format == 9u)
                                  ? (u32)pixels
                                  : (binding->format == 6u) ? (u32)(pixels * 4u)
                                                            : (u32)(pixels * 2u);
            const u8* source = (const u8*)data;
            u32 nonzero = 0;
            u32 hash = 2166136261u;
            for (u32 i = 0; i < bytes; i++) {
                nonzero += source[i] != 0;
                hash = (hash ^ source[i]) * 16777619u;
            }
            fprintf(stderr, "[gfx] tex-data slot=%u bytes=%u nonzero=%u hash=%08X\n",
                    slot, bytes, nonzero, hash);
        }
        if (g_graphics_log && g_gx_begin_count < 12)
            fprintf(stderr,
                    "[gfx] tex slot=%u obj=0x%08X data=0x%08X host=%p "
                    "%ux%u fmt=%u tlut=%u flags=0x%02X ver=%u\n",
                    slot, binding->object, binding->guest_data, data,
                    binding->width, binding->height, binding->format,
                    binding->tlut, binding->flags, binding->version);
        if (g_movie_log && binding->format == 1u) {
            const unsigned movie_tex_seq = ++g_movie_texture_load_count;
            if (movie_tex_seq < 96u || (movie_tex_seq % 60u) == 0u) {
                fprintf(stderr,
                        "[movie] draw-tex slot=%u data=0x%08X %ux%u fmt=I8 "
                        "bytes=%u hash=%08X ver=%u begin=%u seq=%u\n",
                        slot, binding->guest_data, binding->width, binding->height,
                        binding->data_bytes, binding->data_hash, binding->version,
                        g_gx_begin_count, movie_tex_seq);
            }
        }
        dol_platform_load_texture_guest(
            (u8)slot, binding->guest_data, data, binding->width,
            binding->height, binding->format, binding->tlut,
            (binding->flags & 1u) != 0, binding->object, binding->version);
        binding->emitted_version = binding->version;
    }
}

static void notify_GXBegin(CPUState* cpu) {
    ++g_gx_begin_count;
    if (ball_state_log_enabled() &&
        guest_backchain_contains(cpu, 0x8011DF10u, 0x8011E38Cu)) {
        fprintf(stderr,
                "[ball-draw] guest-frame=%llu begin=%u prim=%u fmt=%u "
                "count=%u lr=0x%08X\n",
                (unsigned long long)(cpu->timebase / 675000ull),
                g_gx_begin_count, hle_arg_u32(cpu, 0), hle_arg_u32(cpu, 1),
                hle_arg_u32(cpu, 2), cpu->lr);
    }
    if (g_graphics_log && g_gx_begin_count <= 12)
        fprintf(stderr,
                "[gfx] begin #%u prim=%u fmt=%u count=%u lr=0x%08X\n",
                g_gx_begin_count, hle_arg_u32(cpu, 0), hle_arg_u32(cpu, 1),
                hle_arg_u32(cpu, 2), cpu->lr);
    if (g_graphics_log && g_gx_begin_count == 1) {
        u32 packet = cpu->gpr[30];
        fprintf(stderr,
                "[gfx] packet=0x%08X index=0x%08X vertices=%u prim=%u "
                "streams=%u stream-list=0x%08X\n",
                packet, mem_read32(cpu, packet + 4u), mem_read16(cpu, packet + 8u),
                mem_read8(cpu, packet + 0x0Au), mem_read8(cpu, packet + 0x0Bu),
                mem_read32(cpu, packet + 0x0Cu));
        fprintf(stderr,
                "[gfx] packet-state texture=%08X%08X material=0x%08X "
                "program=0x%08X raster=0x%08X matrix=0x%08X "
                "textures=%08X,%08X,%08X,%08X,%08X,%08X "
                "texconfig=%u user-key=0x%08X material-set=0x%08X\n",
                mem_read32(cpu, packet + 0x10u),
                mem_read32(cpu, packet + 0x14u),
                mem_read32(cpu, packet + 0x18u),
                mem_read32(cpu, packet + 0x1Cu),
                mem_read32(cpu, packet + 0x20u),
                mem_read32(cpu, packet + 0x24u),
                mem_read32(cpu, packet + 0x28u),
                mem_read32(cpu, packet + 0x2Cu),
                mem_read32(cpu, packet + 0x30u),
                mem_read32(cpu, packet + 0x34u),
                mem_read32(cpu, packet + 0x38u),
                mem_read32(cpu, packet + 0x3Cu),
                mem_read8(cpu, packet + 0x40u),
                mem_read32(cpu, packet + 0x42u),
                mem_read32(cpu, packet + 0x46u));
        fprintf(stderr,
                "[gfx] state view=%u program=0x%08X texconfig=0x%08X "
                "viewport=%08X,%08X,%08X,%08X\n",
                mem_read32(cpu, 0x80374010u), mem_read32(cpu, 0x80372FA4u),
                mem_read32(cpu, 0x80374024u), mem_read32(cpu, 0x8032C238u),
                mem_read32(cpu, 0x8032C23Cu), mem_read32(cpu, 0x8032C240u),
                mem_read32(cpu, 0x8032C244u));
    }
}

static void notify_GXCopyTex(CPUState* cpu) {
    u32 address = hle_arg_u32(cpu, 0);
    void* data = graphics_guest_pointer(cpu, address, NULL);
    dol_platform_set_copy_destination_guest(address, data);
}

// GXCopyDisp(void* dest, GXBool clear) copies the EFB to the external frame
// buffer -- this is the game's real frame boundary (one per rendered frame).
// Drive Aurora's present here so each game frame is drained, rendered, and
// reset on its own. Presenting on VIWaitForRetrace instead let the front-end's
// load/transition loop (which renders many frames between retraces) pile every
// frame's geometry into a single Aurora frame packet, overflowing its fixed
// per-frame vertex/index staging buffers (ByteBuffer::resize abort). Aurora
// presents the EFB directly, so the stubbed display copy is harmless; we only
// need the boundary signal.
// Dump the game's GL/glx transform globals (G4QE01 addresses) as big-endian
// floats so the recomp's per-frame matrices can be diffed against Dolphin's
// View->Memory read of the same addresses. Gated by ED_MATRIX_LOG; rate
// limited so a long run does not flood. The conclusion fork: matching guest
// matrices + missing geometry -> bug is downstream (XF emission/Aurora); a
// degenerate/diverging matrix -> upstream recomp matrix math.
static void matrix_log_floats(CPUState* cpu, const char* name, u32 addr,
                              u32 count) {
    u8 raw[64];
    if (count * 4u > sizeof(raw))
        count = (u32)(sizeof(raw) / 4u);
    copy_guest_to_host(cpu, addr, raw, count * 4u);
    fprintf(stderr, "[matrix] %s @0x%08X:", name, addr);
    for (u32 i = 0; i < count; i++) {
        u32 bits = ((u32)raw[i * 4 + 0] << 24) | ((u32)raw[i * 4 + 1] << 16) |
                   ((u32)raw[i * 4 + 2] << 8) | (u32)raw[i * 4 + 3];
        float f;
        memcpy(&f, &bits, sizeof(f));
        if ((i & 3u) == 0u)
            fprintf(stderr, "\n  ");
        fprintf(stderr, " % .6g", (double)f);
    }
    fprintf(stderr, "\n");
}

static void matrix_log_frame(CPUState* cpu) {
    static int s_enabled = -1;
    static unsigned s_frame = 0;
    if (s_enabled < 0)
        s_enabled = getenv("ED_MATRIX_LOG") != NULL ? 1 : 0;
    if (!s_enabled)
        return;
    ++s_frame;
    if (s_frame > 20u && (s_frame % 60u) != 0u)
        return;
    fprintf(stderr, "[matrix] ---- frame %u ----\n", s_frame);
    matrix_log_floats(cpu, "gx_proj     ", 0x8032C090u, 16u); // projection 4x4
    matrix_log_floats(cpu, "gx_modelview", 0x8032C0D0u, 12u); // camera x object
    matrix_log_floats(cpu, "gx_mview    ", 0x8032C060u, 12u); // view (camera)
    // How many of this frame's GXLoadPosMtxImm loads carried a real 3D rotation.
    // Resolves whether the recomp ever applies the camera tilt (H2) or never
    // does (H1) — independent of the last-set frame-end gx_modelview.
    fprintf(stderr, "[matrix] posmtx loads=%u tilted=%u\n",
            g_posmtx_loads_this_frame, g_posmtx_tilted_this_frame);
    g_posmtx_loads_this_frame = 0;
    g_posmtx_tilted_this_frame = 0;
}

static void notify_GXCopyDisp(CPUState* cpu) {
    // TEMP DIAG (G013 cutscene): observe the guest transition-state flags each
    // frame so a live goal celebration can discriminate "guest chose not to
    // render the world" from "host skipped the draws". Gated by env.
    {
        static int s_cutscene_diag = -1;
        if (s_cutscene_diag < 0)
            s_cutscene_diag = getenv("ED_CUTSCENE_DIAG") != NULL ? 1 : 0;
        if (s_cutscene_diag) {
            static u64 s_diag_frame = 0;
            u8 render_world = mem_read8(cpu, 0x8037273Cu); // g_bRenderWorld
            // Log only transitions / every 60th frame to cut spam; always log
            // when world is off (H3-adjacent guest skip).
            static u8 s_prev_rw = 0xFFu;
            u64 fr = ++s_diag_frame;
            if (render_world != s_prev_rw || render_world == 0u ||
                (fr % 60u) == 0u)
                fprintf(stderr, "[cutscene] frame=%llu g_bRenderWorld=%u\n",
                        (unsigned long long)fr, render_world);
            s_prev_rw = render_world;
        }
    }
    matrix_log_frame(cpu);
    if (g_frame_array_bytes > g_frame_array_bytes_peak)
        g_frame_array_bytes_peak = g_frame_array_bytes;
    if (g_graphics_log && g_frame_array_calls)
        fprintf(stderr,
                "[gfx] frame array uploads: calls=%u bytes=%llu (%.2f MB) peak=%.2f MB cap=8MB\n",
                g_frame_array_calls,
                (unsigned long long)g_frame_array_bytes,
                (double)g_frame_array_bytes / (1024.0 * 1024.0),
                (double)g_frame_array_bytes_peak / (1024.0 * 1024.0));
    g_frame_array_bytes = 0;
    g_frame_array_calls = 0;
    movie_cadence_present(cpu);
}

static void notify_VIConfigure(CPUState* cpu) {
    u32 mode = hle_arg_u32(cpu, 0);
    if (!mode)
        return;
    if (g_graphics_log)
        fprintf(stderr,
                "[gfx] VIConfigure mode=0x%08X tv=%u fb=%ux%u xfb=%u vi=%ux%u\n",
                mode, mem_read32(cpu, mode), mem_read16(cpu, mode + 4u),
                mem_read16(cpu, mode + 6u), mem_read16(cpu, mode + 8u),
                mem_read16(cpu, mode + 0x0Eu), mem_read16(cpu, mode + 0x10u));
    dol_platform_configure_vi(
        mem_read32(cpu, mode),
        mem_read16(cpu, mode + 4u),
        mem_read16(cpu, mode + 6u),
        mem_read16(cpu, mode + 8u),
        mem_read16(cpu, mode + 0x0Eu),
        mem_read16(cpu, mode + 0x10u));
}

// E7 Virtual XFB: latch guest XFB address for present-from-XFB.
static void notify_VISetNextFrameBuffer(CPUState* cpu) {
    u32 fb = hle_arg_u32(cpu, 0);
    dol_platform_set_next_xfb(fb & 0x1FFFFFFFu);
}

// The frame is presented at VIFlush, not at GXCopyDisp as the Strikers host
// did: this game copies the frame (GXCopyDisp), then points the VI at it
// (VISetNextFrameBuffer) and flushes. Presenting on entry to GXCopyDisp showed
// the VI's buffer before its display copy existed, which was a black window.
static void notify_VIFlush(CPUState* cpu) {
    (void)cpu;
    dol_platform_flush_xfb();
    mmio_fifo_dump_note("present", 0, 0);
    dol_platform_present();
}

// TEMP DIAG (G013 H3): guest glxSwapSetBlack / VISetBlack. Gated by
// ED_CUTSCENE_DIAG. Discriminates "guest left VI black latched" from
// host skip paths (H1/H2). Does not alter black state — observe only.
static void notify_glxSwapSetBlack(CPUState* cpu) {
    static int s_cutscene_diag = -1;
    if (s_cutscene_diag < 0)
        s_cutscene_diag = getenv("ED_CUTSCENE_DIAG") != NULL ? 1 : 0;
    if (!s_cutscene_diag)
        return;
    fprintf(stderr, "[cutscene] glxSwapSetBlack black=%u lr=0x%08X\n",
            cpu->gpr[3] & 0xFFu, cpu->lr);
}

static void notify_VISetBlack(CPUState* cpu) {
    // E7: always latch black into the platform (present-from-XFB respects it).
    dol_platform_set_xfb_black((cpu->gpr[3] & 0xFFu) != 0u);
    {
        static int s_cutscene_diag = -1;
        if (s_cutscene_diag < 0)
            s_cutscene_diag = getenv("ED_CUTSCENE_DIAG") != NULL ? 1 : 0;
        if (s_cutscene_diag)
            fprintf(stderr, "[cutscene] VISetBlack black=%u lr=0x%08X\n",
                    cpu->gpr[3] & 0xFFu, cpu->lr);
    }
}

static void notify_VIWaitForRetrace(CPUState* cpu) {
    ++g_vi_wait_count;
    if (g_graphics_log && (g_vi_wait_count <= 12u || g_vi_wait_count % 60u == 0u)) {
        u32 manager = mem_read32(cpu, 0x803742A4u);
        u32 active = manager ? mem_read32(cpu, manager + 0xA04u) : 0u;
        fprintf(stderr, "[gfx] retrace=%u async-manager=0x%08X active=0x%08X",
                g_vi_wait_count, manager, active);
        if (active) {
            u32 entry = mem_read32(cpu, active);
            fprintf(stderr,
                    " entry=0x%08X file=0x%08X callback=0x%08X buffer=0x%08X "
                    "size=%u position=%u phase=%u remaining=%u",
                    entry, mem_read32(cpu, entry + 8u), mem_read32(cpu, entry + 0x0Cu),
                    mem_read32(cpu, entry + 0x10u), mem_read32(cpu, entry + 0x14u),
                    mem_read32(cpu, entry + 0x18u), mem_read32(cpu, entry + 0x20u),
                    mem_read32(cpu, entry + 0x24u));
        }
        fputc('\n', stderr);
    }
    // The frame is presented from GXCopyDisp (the real frame boundary); the VI
    // retrace only paces the game (handled by the retrace interrupt).
}

static void hle_PADInit(CPUState* cpu) {
    hle_set_u32(cpu, dol_platform_pad_init() ? 1u : 0u);
}

// The game's pause menu (inventory, map, magick, options), where L and R page
// it: its button icons show bumpers there and triggers in play (controls.cpp).
// Entering sets a small-data flag and pushes a render state; leaving pops the
// state and clears the flag, except on the two exits to an area change or a
// cutscene, which leave the flag set until the next area loads. So the menu is
// open while both hold. The two r13 offsets come from the code
// (find_game_menu); 0 when it was not found.
static s32 g_menu_flag_offset, g_menu_depth_offset;

bool hle_coop_tome_shares(void);
void aurora_split_only_view(int player);
static bool tw_world_turn(void);
static bool tw_tome_next(void);
static bool game_menu_open(CPUState* cpu) {
    if (hle_coop_tome_shares())
        return !tw_world_turn();  // the Tome open, its frames turn about with the world's
    const u32 sda = cpu->gpr[13];
    if (g_menu_flag_offset == 0 || sda < GC_RAM_BASE + 0x8000u ||
        sda - GC_RAM_BASE > cpu->ram_size - 0x8000u)
        return false;  // not found, or before the game set r13
    return mem_read32(cpu, sda + (u32)g_menu_flag_offset) != 0u &&
           (s32)mem_read32(cpu, sda + (u32)g_menu_depth_offset) > 0;
}

// The game's pause menu (the Tome) up now, for the split (camera.c), which
// hears of it from the pad a read later.
bool hle_game_menu_open(CPUState* cpu) { return game_menu_open(cpu); }

// A cutscene, where RB or Tab fast-forwards (controls.cpp): one of the movies is
// open, or a scene plays in the letterbox bars. The game's scene flag is set
// while one plays, and the bars' height is a float it keeps at 60 while they
// show (also on the title screen, where the flag is clear) and 0 in play,
// 0xBC into an object whose address one function loads. Both from the code
// (find_letterbox); 0 when not found.
static u32 g_letterbox_height;
static s32 g_scene_flag_offset;
static bool sda_ready(const CPUState* cpu);

static bool cutscene_playing(CPUState* cpu) {
    if (g_movie_file != 0u)
        return true;
    return g_letterbox_height != 0u && g_scene_flag_offset != 0 && sda_ready(cpu) &&
           mem_read32(cpu, cpu->gpr[13] + (u32)g_scene_flag_offset) != 0u &&
           guest_read_f32(cpu, g_letterbox_height) > 0.0f;
}

// Load Game goes straight to the saves: the memory card screen (Slot A or B)
// is passed over. Its stage is 6 at 0x8061FE28 (5 on the title menu and the
// save list), its loop waiting at 0x80170118 (the menu thread's saved pc at
// 0x80607790); the save list's flag at 0x8061F234 is 1. Come from the title
// menu, A is pressed for the player (Slot A); come back from the save list,
// B (back to the title menu). Pressed every other few pad reads until the
// screen has gone, as the screen takes no press while it fades in.
#define CARD_STAGE 0x8061FE28u
#define CARD_THREAD_PC 0x80607790u
#define CARD_SELECT_PC 0x80170118u
#define SAVE_LIST_FLAG 0x8061F234u

// The memory card screen draws a frame round the chosen card (Slot A's, a
// blue outline), over the save list too and for the moment the screen passes:
// its draw (0x8019C9C8, called from the card screen's own, 0x8001D0E8) is
// passed over.
#define CARD_FRAME_DRAW 0x8019C9C8u
#define CARD_SCREEN_DRAW 0x8001D0E8u
#define CARD_SCREEN_DRAW_END 0x8001D4A4u

static void hle_card_frame(CPUState* cpu) {
    g_hle_passthrough = !(cpu->lr >= CARD_SCREEN_DRAW && cpu->lr < CARD_SCREEN_DRAW_END);
}

// A chapter picked in Jump to Game starts straight away. The game has just
// been reset, so the title's look for cards being put in (0x8001BF68) finds
// slot A's as new, checks it, and holds the chapter's start (0x8001A164) on
// the memory card screen ("Press A to Continue": its step 0x8001CCC8, which
// runs only while it is up) until A. While that screen is up on the title
// (mode 0) and it is not Load Game's (skip_card_select), A is pressed for
// the player. The screen is drawn see-through meanwhile (its step draws it at
// the fade level in r3), so it does not flash up as it passes.
#define CARD_SCREEN_LOOK 0x8001CCC8u
static unsigned long long g_card_screen_drawn;  // the frame the screen's step last ran

static void notify_card_screen(CPUState* cpu) {
    g_card_screen_drawn = cpu->timebase / 675000ull;
    if (sda_ready(cpu) && mem_read32(cpu, cpu->gpr[13] - 26240u) == 0u &&
        mem_read32(cpu, cpu->gpr[13] - 26188u) == 0u && mem_read32(cpu, SAVE_LIST_FLAG) != 1u)
        cpu->gpr[3] = 0u;
}

static void skip_card_inserted(CPUState* cpu, DolPadState* pad) {
    static unsigned reads;
    const unsigned long long frame = cpu->timebase / 675000ull;
    const bool up = g_card_screen_drawn != 0u && frame - g_card_screen_drawn <= 2u && sda_ready(cpu) &&
                    mem_read32(cpu, cpu->gpr[13] - 26188u) == 0u && mem_read32(cpu, SAVE_LIST_FLAG) != 1u;
    if (!up) {
        reads = 0u;
        return;
    }
    if (reads++ == 0u)
        fprintf(stderr, "[load] the memory card screen after Jump to Game passed over\n");
    if (reads % 6u >= 3u)
        return;  // released between presses
    pad->button |= 0x0100u;
    pad->analog_a = 0xFFu;
    pad->error = 0;
}

static void skip_card_select(CPUState* cpu, DolPadState* pad) {
    static bool came_from_list;
    static unsigned reads, since_list = ~0u;
    if (strcmp(ED_GAME_ID, "GEDP01") != 0)
        return;
    // The save list seen in the last two seconds or so (the screens pass
    // through a few reads of neither on the way back).
    if (mem_read32(cpu, SAVE_LIST_FLAG) == 1u)
        since_list = 0u;
    else if (since_list != ~0u)
        since_list++;
    const bool card = mem_read32(cpu, CARD_STAGE) == 6u && mem_read32(cpu, CARD_THREAD_PC) == CARD_SELECT_PC;
    if (!card) {
        reads = 0u;
        skip_card_inserted(cpu, pad);
        return;
    }
    if (reads++ == 0u) {
        came_from_list = since_list < 120u;
        fprintf(stderr, "[load] the memory card screen passed over (%s)\n",
                came_from_list ? "back to the title menu" : "Slot A");
    }
    if (reads % 6u >= 3u)
        return;  // released between presses
    pad->button |= came_from_list ? 0x0200u : 0x0100u;  // B, else A
    if (came_from_list)
        pad->analog_b = 0xFFu;
    else
        pad->analog_a = 0xFFu;
    pad->error = 0;
}

#ifdef ED_AURORA
static void coop_pad_read(CPUState* cpu, bool playing);
// Split-screen co-op: while player 2's turn reads the pad (coop_player_turn),
// the game's pad 1 is player 2's controller.
static bool g_coop_pad_p2;
static bool g_coop_pad_p1;  // player 1's turn in a close-up of player 2's: their own pad, not the close-up's
static u16 g_coop_p2_buttons;  // player 2's pad as last read (coop_quick_cast)
// What they held as a screen of theirs closed (the A that chose in their Tome):
// nothing to play until let go, or it was a press there (their attack).
static u16 g_coop_p2_hold;
// Player 1's B held for them (coop_door_proxy: player 2 using a door).
static bool g_coop_press_b;
// Who has the screens that stop play (coop_menu_input).
static void coop_menu_input(CPUState* cpu, bool menu, bool cutscene, DolPadState* pad);
static int g_coop_tome_owner = -1;  // who has the screens that stop play
static int g_tw_held_from = -1;  // which reads' the pad's held buttons are now, -1 starting
static u16 g_tw_taps;  // what the Tome's owner pressed between the Tome's reads: a quick press is not lost
// ED_COOP_TOME_COPY=2 (testing): player 2's Tome in a copy of the memory (tome_copy_step).
static bool g_tome_copy_ask, g_tome_copy_open;
// The copy is player 1's: asked for in a close-up of player 2's, which goes on in the real game, with the
// game's window, script and pad its own. No sharing of the frames: the real world runs its usual way,
// its picture in player 2's half (coop_menu_input, tome_copy_step).
static bool g_tome_copy_p1;
static DolPadState g_tome_copy_p1_pad;  // player 1's pad as last read
static u16 g_tome_copy_p1_hold;         // what they held as it closed: not a press in play
static u16 g_tome_copy_held;  // what they held as it opened: not a press in it
static u16 g_tome_copy_taps;  // what they pressed between its reads
static u8 g_tome_copy_spell[24];  // what their cast asked of a Tome (COOP_SPELL_ITEM), for their copy alone
static bool g_tome_copy_spell_set, g_tome_copy_spell_open;
static void magick_trace(CPUState* cpu);
static int tome_copy_mode(void) {
    static int mode = -1;
    if (mode < 0)
        mode = getenv("ED_COOP_TOME_COPY") != NULL ? atoi(getenv("ED_COOP_TOME_COPY")) : 2;  // two Tomes: 0 the old way, 1 the empty turns
    return mode;
}
// Player 2 has a close-up their action press opened (coop_menu_input): the
// split is off (camera.c) and the close-up shows in their half.
static bool g_coop_screen;
bool hle_coop_screen(void) { return g_coop_screen; }
// A close-up of a thing looked at or taken: the player is busy, a mask of
// reasons at r13 - 23344 (set and cleared by 0x801556CC, read by 0x80155738,
// which the per-character update 0x80007490 asks before it runs the player's
// controller). Found with ED_COOP_WATCH and ED_COOP_LOG, examining the clock.
static bool coop_busy(CPUState* cpu) { return mem_read32(cpu, cpu->gpr[13] - 23344u) != 0u; }

// ED_COOP_WATCH=1 (testing): what a close-up changes, in the game's globals
// (0x80500000 up to 0x80640000, the small data among them) and in player 1's
// node, object and actor. Player 1's B is the press. Kept: before (about 40
// and 1 pad reads before it), during (45, 90 and 135 after), after (once
// they walk again, the stick held a half second). Words alike in all of
// before and after, alike in all of during and different between, are
// logged: what holds while a close-up shows, not a timer that flips.
#define WATCH_FROM 0x80500000u
#define WATCH_GLOBALS 0x140000u
#define WATCH_SIZE (WATCH_GLOBALS + 0x200u + 0x800u + 0x400u)
static bool g_coop_found;
static s16 g_coop_actor_offset;
static u32 coop_player(CPUState* cpu);
enum { WATCH_OLD, WATCH_LAST, WATCH_B1, WATCH_B2, WATCH_D1, WATCH_D2, WATCH_D3, WATCH_COUNT };

static void coop_watch(CPUState* cpu, const DolPadState* pad) {
    static int on = -1;
    static u8* keep[WATCH_COUNT + 1];  // and the present
    static u32 from[4];
    static const u32 size[4] = {WATCH_GLOBALS, 0x200u, 0x800u, 0x400u};
    static const char* const kWhere[4] = {"global", "node", "object", "actor"};
    static unsigned step, walking, reads;
    static bool b_was;
    if (on < 0) {
        on = getenv("ED_COOP_WATCH") != NULL;
        for (int i = 0; on && i <= WATCH_COUNT; i++)
            on = (keep[i] = (u8*)malloc(WATCH_SIZE)) != NULL;
    }
    if (!on || !g_coop_found || !sda_ready(cpu))
        return;
    const u32 player = coop_player(cpu);
    if (player == 0u)
        return;
    // The regions as they are now (the player's, at the press).
    if (step == 0u) {
        from[0] = WATCH_FROM;
        from[1] = player;
        from[2] = mem_read32(cpu, player + 52u);
        from[3] = mem_read32(cpu, player + (u32)(s32)g_coop_actor_offset);
    }
    u8* now = keep[WATCH_COUNT];
    for (u32 r = 0, at = 0; r < 4u; at += size[r], r++) {
        if (guest_ram_range_valid(cpu, from[r], size[r]))
            memcpy(now + at, cpu->ram + (from[r] - GC_RAM_BASE), size[r]);
        else
            memset(now + at, 0, size[r]);
    }
    const bool b = (pad->button & 0x0200u) != 0u;
    const bool pressed = b && !b_was;
    b_was = b;
    if (step == 0u) {
        if (pressed) {
            memcpy(keep[WATCH_B1], keep[WATCH_OLD], WATCH_SIZE);
            memcpy(keep[WATCH_B2], keep[WATCH_LAST], WATCH_SIZE);
            step = 1u;
            fprintf(stderr, "[watch] B pressed\n");
            return;
        }
        if (++reads % 40u == 0u)
            memcpy(keep[WATCH_OLD], keep[WATCH_LAST], WATCH_SIZE);
        memcpy(keep[WATCH_LAST], now, WATCH_SIZE);
        return;
    }
    step++;
    if (step == 45u || step == 90u || step == 135u)
        memcpy(keep[step == 45u ? WATCH_D1 : step == 90u ? WATCH_D2 : WATCH_D3], now, WATCH_SIZE);
    if (step == 135u)
        fprintf(stderr, "[watch] during kept; walk to finish\n");
    if (step <= 135u)
        return;
    const bool moving = pad->stick_x * pad->stick_x + pad->stick_y * pad->stick_y > 40 * 40;
    walking = moving ? walking + 1u : 0u;
    if (walking < 30u)
        return;
    unsigned shown = 0u;
    for (u32 r = 0, at = 0; r < 4u; at += size[r], r++)
        for (u32 off = 0; off < size[r] && shown < 300u; off += 4u) {
            u32 v[WATCH_COUNT + 1];
            for (int i = 0; i <= WATCH_COUNT; i++)
                memcpy(&v[i], keep[i] + at + off, 4);
            const u32 before = v[WATCH_B1], during = v[WATCH_D1];
            if (v[WATCH_B2] == before && v[WATCH_COUNT] == before && v[WATCH_D2] == during &&
                v[WATCH_D3] == during && during != before) {
                fprintf(stderr, "[watch] %s +0x%X (0x%08X): %08X -> %08X -> back\n", kWhere[r], off, from[r] + off,
                        __builtin_bswap32(before), __builtin_bswap32(during));
                shown++;
            }
        }
    fprintf(stderr, "[watch] %u word(s); press B again for another\n", shown);
    step = 0u;
    reads = 0u;
    memcpy(keep[WATCH_OLD], now, WATCH_SIZE);
    memcpy(keep[WATCH_LAST], now, WATCH_SIZE);
}
#endif

#ifdef ED_AURORA
static bool coop_character_load(CPUState* cpu);
#endif

#ifdef ED_AURORA
// Player 2's pad, for their Tome's frame in the copy.
static void tome_copy_pad(CPUState* cpu, u32 out) {
    DolPadState state[4];
    memset(state, 0, sizeof state);
    if (g_tome_copy_p1) {
        state[0] = g_tome_copy_p1_pad;
    } else {
        controls_set_tome_read(1);
        controls_read_player2(&state[0]);
        controls_set_tome_read(-1);
    }
    state[0].button |= g_tome_copy_taps;
    g_tome_copy_taps = 0u;
    g_tome_copy_held &= state[0].button;
    state[0].button &= (u16)~g_tome_copy_held;
    if ((g_tome_copy_held & 0x0100u) != 0u)
        state[0].analog_a = 0u;
    if ((g_tome_copy_held & 0x0200u) != 0u)
        state[0].analog_b = 0u;
    state[0].error = 0;
    for (u32 i = 1; i < 4; i++)
        state[i].error = -1;  // PAD_ERR_NO_CONTROLLER
    for (u32 i = 0; i < 4; i++) {
        const u32 p = out + i * 12u;
        mem_write16(cpu, p, state[i].button);
        mem_write8(cpu, p + 2u, (u8)state[i].stick_x);
        mem_write8(cpu, p + 3u, (u8)state[i].stick_y);
        mem_write8(cpu, p + 4u, (u8)state[i].substick_x);
        mem_write8(cpu, p + 5u, (u8)state[i].substick_y);
        mem_write8(cpu, p + 6u, state[i].trigger_left);
        mem_write8(cpu, p + 7u, state[i].trigger_right);
        mem_write8(cpu, p + 8u, state[i].analog_a);
        mem_write8(cpu, p + 9u, state[i].analog_b);
        mem_write8(cpu, p + 10u, (u8)state[i].error);
        mem_write8(cpu, p + 11u, 0);
    }
    hle_set_u32(cpu, 0u);
}
#endif

#ifdef ED_AURORA
// ED_BOTH=path (tools/ed): one row a frame with both players side by side.
static void both_record(CPUState* cpu, const DolPadState* p1);
static void test_run_script(CPUState* cpu);
static struct {
    u16 button;
    s8 x, y;
} g_both_p2_pad;  // player 2's pad as read, before a Tome or a close-up takes it
static s32 g_both_script;  // the last script started, apart from zones
#endif

static void hle_PADRead(CPUState* cpu) {
#ifdef ED_AURORA
    if (g_tome_copy_turn) {
        tome_copy_pad(cpu, hle_arg_u32(cpu, 0));
        return;
    }
    if (coop_character_load(cpu))
        return;  // the game loads a character's file first; this read runs after
#endif
    u32 out = hle_arg_u32(cpu, 0);
    DolPadState state[4];
    memset(state, 0, sizeof state);
#ifdef ED_AURORA
    if (g_coop_pad_p2) {
        controls_set_tome_read(hle_coop_tome_shares() ? g_coop_tome_owner == 1 : -1);
        controls_read_player2(&state[0]);
        controls_set_tome_read(-1);
        g_both_p2_pad.button = state[0].button;
        g_both_p2_pad.x = state[0].stick_x;
        g_both_p2_pad.y = state[0].stick_y;
        static int pad_log = -1;  // ED_COOP_PAD_LOG=1: player 2's pad when anything is pressed
        if (pad_log < 0)
            pad_log = getenv("ED_COOP_PAD_LOG") != NULL;
        if (pad_log && (state[0].button != 0u || state[0].stick_x != 0 || state[0].stick_y != 0))
            fprintf(stderr, "[coop] player 2's pad: buttons 0x%04X stick %d %d (frame %llu)\n", state[0].button,
                    state[0].stick_x, state[0].stick_y, (unsigned long long)(cpu->timebase / 675000ull));
        // Their pad is their Tome's, to the frames it closes on too: the A that chose
        // an item to use was their attack, and the B that left a look at what they stood by.
        const bool tome_pad = (g_coop_tome_owner == 1 && !g_coop_screen) || (g_tome_copy_open && !g_tome_copy_p1);
        if (tome_pad) {
            if (g_tome_copy_open && !g_tome_copy_p1)
                g_tome_copy_taps |= state[0].button;
            else if (tw_world_turn())
                g_tw_taps |= state[0].button;
            const s32 error = state[0].error;
            memset(&state[0], 0, sizeof state[0]);
            state[0].error = error;
        }
        g_coop_p2_hold &= state[0].button;
        state[0].button &= (u16)~g_coop_p2_hold;
        if ((g_coop_p2_hold & 0x0100u) != 0u)
            state[0].analog_a = 0u;
        if ((g_coop_p2_hold & 0x0200u) != 0u)
            state[0].analog_b = 0u;
        // What's pressed in their Tome is not for the world (a B there ran a prompt's script).
        g_coop_p2_buttons = state[0].button;
        state[0].button &= (u16)~0x1000u;  // their Start opens their own Tome (coop_menu_input)
        if (g_coop_screen)
            memset(&state[0], 0, sizeof state[0]);  // their pad runs the close-up instead
        for (u32 i = 1; i < 4; i++)
            state[i].error = -1;  // PAD_ERR_NO_CONTROLLER
        for (u32 i = 0; i < 4; i++) {
            const u32 p = out + i * 12u;
            mem_write16(cpu, p, state[i].button);
            mem_write8(cpu, p + 2u, (u8)state[i].stick_x);
            mem_write8(cpu, p + 3u, (u8)state[i].stick_y);
            mem_write8(cpu, p + 4u, (u8)state[i].substick_x);
            mem_write8(cpu, p + 5u, (u8)state[i].substick_y);
            mem_write8(cpu, p + 6u, state[i].trigger_left);
            mem_write8(cpu, p + 7u, state[i].trigger_right);
            mem_write8(cpu, p + 8u, state[i].analog_a);
            mem_write8(cpu, p + 9u, state[i].analog_b);
            mem_write8(cpu, p + 10u, (u8)state[i].error);
            mem_write8(cpu, p + 11u, 0);
        }
        hle_set_u32(cpu, 0u);
        return;
    }
#endif
    const bool menu = game_menu_open(cpu), cutscene = cutscene_playing(cpu);
    const bool p1_copy_up = g_tome_copy_open && g_tome_copy_p1;
    controls_set_game_menu(hle_coop_tome_shares() || menu);  // not turn about, for its button pictures
    controls_set_cutscene(cutscene);
#ifdef ED_AURORA
    coop_pad_read(cpu, !menu && !cutscene);
    static bool told;
    if (!told && getenv("ED_COOP_LOG") != NULL) {
        told = true;
        fprintf(stderr, "[coop] PADRead fills 0x%08X (from 0x%08X)\n", out, cpu->lr);
    }
#endif
    controls_note_frame(cpu->timebase / 675000ull);
    // One read, as the Tome's, while their Tome's up. Read twice, world then Tome, each change of buttons
    // marked all they held as held over (controls.cpp, g_tome_was, g_held_over): no press got through,
    // and their Tome couldn't be closed. The camera isn't told of it (camera_set_menu).
    controls_set_tome_read(p1_copy_up ? 1 : hle_coop_tome_shares() ? g_coop_tome_owner == 0 : -1);
    u32 motor_mask = dol_platform_pad_read(state);
    controls_set_tome_read(-1);
    {
        report_pad_read();
        magick_trace(cpu);
        static int pad_log = -1;  // ED_COOP_PAD_LOG=1: player 1's pad as it changes, and the Tome's owner
        static u16 pad_was;
        if (pad_log < 0)
            pad_log = getenv("ED_COOP_PAD_LOG") != NULL;
        if (pad_log && state[0].button != pad_was)
            fprintf(stderr, "[coop] player 1's pad: buttons 0x%04X (Tome open %d, its owner %d, frame %llu)\n", state[0].button,
                    hle_coop_tome_shares(), g_coop_tome_owner, (unsigned long long)(cpu->timebase / 675000ull));
        pad_was = state[0].button;
    }
#ifdef ED_AURORA
    coop_watch(cpu, &state[0]);
    both_record(cpu, &state[0]);
    test_run_script(cpu);
#endif
    if (camera_pad_buttons(cpu, state[0].button))
        state[0].stick_x = state[0].stick_y = 0;
    interrupt_set_speed(controls_fast_forward() ? 4u : 1u);
    if (g_auto_input) {
        const u64 frame = cpu->timebase / 675000ull;
        if (frame >= 135u && ((frame - 135u) % 90u) < 12u) {
            state[0].button |= 0x0100u;
            state[0].analog_a = 0xFFu;
            if (g_auto_input_last_pulse != frame) {
                fprintf(stderr, "[input] scripted A at frame %llu\n",
                        (unsigned long long)frame);
                g_auto_input_last_pulse = frame;
            }
        }
        state[0].error = 0;
    }
    if (g_auto_input_once_frame != 0) {
        const u64 frame = cpu->timebase / 675000ull;
        if (frame >= g_auto_input_once_frame &&
            frame < g_auto_input_once_frame + 8u &&
            g_auto_input_once_sent != g_auto_input_once_frame) {
            state[0].button |= 0x0100u;
            state[0].analog_a = 0xFFu;
            state[0].error = 0;
            fprintf(stderr, "[input] one-shot A at frame %llu\n",
                    (unsigned long long)frame);
            g_auto_input_once_sent = g_auto_input_once_frame;
        }
    }
    if (g_auto_skip_card_prompt) {
        const u64 frame = cpu->timebase / 675000ull;
        u16 scripted = 0;
        const char* action = NULL;
        if (frame >= 360u && frame < 372u) {
            scripted = 0x0004u; // PAD_BUTTON_DOWN
            action = "Down";
        } else if (frame >= 420u && frame < 432u) {
            scripted = 0x0100u; // PAD_BUTTON_A
            action = "A";
        }
        if (scripted != 0) {
            state[0].button |= scripted;
            if (scripted == 0x0004u)
                state[0].stick_y = -80;
            if (scripted == 0x0100u)
                state[0].analog_a = 0xFFu;
            state[0].error = 0;
            if (g_auto_skip_card_last_pulse != frame) {
                fprintf(stderr, "[input] card-prompt %s at frame %llu\n",
                        action, (unsigned long long)frame);
                g_auto_skip_card_last_pulse = frame;
            }
        }
    }
    if (input_script_apply(cpu, &state[0].button, &state[0].stick_x,
                           &state[0].stick_y, &state[0].analog_a))
        state[0].error = 0;
    if (skip_intro_apply(cpu, &state[0].button, state[0].stick_x, state[0].stick_y,
                         &state[0].analog_a))
        state[0].error = 0;
    mash_to_gameplay_apply(cpu, &state[0].button, &state[0].stick_x,
                           &state[0].analog_a);
#ifdef ED_AURORA
    if (g_coop_press_b) {
        state[0].button |= 0x0200u;  // PAD_BUTTON_B
        state[0].analog_b = 0xFFu;
        state[0].error = 0;
    }
    const DolPadState p1_read = state[0];
    g_tome_copy_p1_pad = p1_read;
    if (!g_coop_pad_p1)
        coop_menu_input(cpu, menu, cutscene, &state[0]);
    else
        state[0].button &= (u16)~0x1000u;  // their Start in a close-up of player 2's opens their copy (coop_menu_input)
    // Theirs is in their Tome, not the world: they stand. Unless the game's pad is player 2's just now.
    if (g_tome_copy_open && g_tome_copy_p1) {
        g_tome_copy_taps |= p1_read.button;
        if (g_coop_pad_p1 || g_coop_tome_owner != 1) {
            const s32 error = state[0].error;
            memset(&state[0], 0, sizeof state[0]);
            state[0].error = error;
        }
    } else if (g_tome_copy_p1_hold != 0u) {
        // What they held as it closed, until they let it go: the A that chose was their attack.
        g_tome_copy_p1_hold &= p1_read.button;
        if (g_coop_pad_p1 || g_coop_tome_owner != 1) {
            state[0].button &= (u16)~g_tome_copy_p1_hold;
            if ((g_tome_copy_p1_hold & 0x0100u) != 0u)
                state[0].analog_a = 0u;
            if ((g_tome_copy_p1_hold & 0x0200u) != 0u)
                state[0].analog_b = 0u;
        }
    }
#endif
    skip_card_select(cpu, &state[0]);
    if (g_mash_to_gameplay)
        state[0].error = 0;
    for (u32 i = 0; i < 4; i++) {
        u32 p = out + i * 12u;
        mem_write16(cpu, p, state[i].button);
        mem_write8(cpu, p + 2u, (u8)state[i].stick_x);
        mem_write8(cpu, p + 3u, (u8)state[i].stick_y);
        mem_write8(cpu, p + 4u, (u8)state[i].substick_x);
        mem_write8(cpu, p + 5u, (u8)state[i].substick_y);
        mem_write8(cpu, p + 6u, state[i].trigger_left);
        mem_write8(cpu, p + 7u, state[i].trigger_right);
        mem_write8(cpu, p + 8u, state[i].analog_a);
        mem_write8(cpu, p + 9u, state[i].analog_b);
        mem_write8(cpu, p + 10u, (u8)state[i].error);
        mem_write8(cpu, p + 11u, 0);
    }
    hle_set_u32(cpu, motor_mask);
}

static void hle_PADReset(CPUState* cpu) {
    bool reset = dol_platform_pad_reset(hle_arg_u32(cpu, 0));
    hle_set_u32(cpu, (reset || g_mash_to_gameplay) ? 1u : 0u);
}

static void hle_PADRecalibrate(CPUState* cpu) {
    bool recalibrated = dol_platform_pad_recalibrate(hle_arg_u32(cpu, 0));
    hle_set_u32(cpu, (recalibrated || g_mash_to_gameplay) ? 1u : 0u);
}

static void hle_PADControlMotor(CPUState* cpu) {
    dol_platform_pad_control_motor(hle_arg_u32(cpu, 0), hle_arg_u32(cpu, 1));
}

static void hle_PADSetSpec(CPUState* cpu) {
    dol_platform_pad_set_spec(hle_arg_u32(cpu, 0));
}
#else
// Deterministic controller pulses for headless integration tests. Port 1 always
// reports a connected controller (with nothing pressed unless scripted): this
// game parks on its "controller unplugged" screen otherwise.
static bool headless_scripted_pad(CPUState* cpu,
                                  u16* buttons,
                                  s8* stick_x,
                                  s8* stick_y,
                                  u8* analog_a) {
    const bool connected = true;
    const u64 frame = cpu->timebase / 675000ull;
    if (g_auto_input) {
        const bool pressed = frame >= 135u && ((frame - 135u) % 90u) < 12u;
        if (pressed) {
            *buttons |= 0x0100u;
            *analog_a = 0xFFu;
            if (g_auto_input_last_pulse != frame) {
                fprintf(stderr, "[input] scripted A at frame %llu\n",
                        (unsigned long long)frame);
                g_auto_input_last_pulse = frame;
            }
        }
    }
    (void)input_script_apply(cpu, buttons, stick_x, stick_y, analog_a);
    (void)skip_intro_apply(cpu, buttons, *stick_x, *stick_y, analog_a);
    mash_to_gameplay_apply(cpu, buttons, stick_x, analog_a);
    return connected;
}

static void hle_PADInit(CPUState* cpu) {
    hle_set_u32(cpu, 1u);
}

static void hle_PADRead(CPUState* cpu) {
    const u32 out = hle_arg_u32(cpu, 0);
    u16 buttons = 0;
    s8 stick_x = 0;
    s8 stick_y = 0;
    u8 analog_a = 0;
    const bool connected =
        headless_scripted_pad(cpu, &buttons, &stick_x, &stick_y, &analog_a);
    for (u32 i = 0; i < 4u; i++) {
        const u32 p = out + i * 12u;
        mem_write16(cpu, p, i == 0u ? buttons : 0u);
        mem_write8(cpu, p + 2u, (u8)(i == 0u ? stick_x : 0));
        mem_write8(cpu, p + 3u, (u8)(i == 0u ? stick_y : 0));
        for (u32 byte = 4u; byte < 8u; byte++)
            mem_write8(cpu, p + byte, 0u);
        mem_write8(cpu, p + 8u, i == 0u ? analog_a : 0u);
        mem_write8(cpu, p + 9u, 0u);
        mem_write8(cpu, p + 10u, (u8)((i == 0u && connected) ? 0 : -1));
        mem_write8(cpu, p + 11u, 0u);
    }
    hle_set_u32(cpu, 0u);
}

static void hle_PADReset(CPUState* cpu) {
    hle_set_u32(cpu, 1u);
}

static void hle_PADRecalibrate(CPUState* cpu) {
    hle_set_u32(cpu, 1u);
}

static void hle_PADControlMotor(CPUState* cpu) {
    (void)cpu;
}

static void hle_PADSetSpec(CPUState* cpu) {
    (void)cpu;
}
#endif

// ---------------------------------------------------------------------------
// Handler registry
// ---------------------------------------------------------------------------
// Aurora-implemented SDK functions, matched by name against the generated table.
bool tome_copy_hook_off(HleHandler fn);

static const HleEntry kHandlers[] = {
    { "OSReport",          hle_OSReport },
    { "ARInit",            hle_ARInit },
    { "ARGetBaseAddress",  hle_ARGetBaseAddress },
    { "ARGetSize",         hle_ARGetSize },
    { "ARGetDMAStatus",    hle_ARGetDMAStatus },
    { "ARStartDMA",        hle_ARStartDMA },
    { "CARDInit",          hle_CARDInit },
    { "CARDProbe",         hle_CARDProbe },
    { "CARDProbeEx",       hle_CARDProbeEx },
    { "CARDGetResultCode", hle_CARDGetResultCode },
    { "CARDGetFastMode",   hle_CARDGetFastMode },
    { "CARDGetXferredBytes", hle_CARDGetXferredBytes },
    { "CARDMountAsync",    hle_CARDMountAsync },
    { "CARDCheckExAsync",  hle_CARDCheckExAsync },
    { "CARDCheckAsync",    hle_CARDCheckAsync },
    { "CARDFreeBlocks",    hle_CARDFreeBlocks },
    { "CARDOpen",          hle_CARDOpen },
    { "CARDCreateAsync",   hle_CARDCreateAsync },
    { "CARDDeleteAsync",   hle_CARDDeleteAsync },
    { "CARDGetStatus",     hle_CARDGetStatus },
    { "CARDSetStatusAsync", hle_CARDSetStatusAsync },
    { "CARDGetSerialNo",   hle_CARDGetSerialNo },
    { "CARDUnmount",       hle_CARDUnmount },
    { "CARDClose",         hle_CARDClose },
    { "CARDReadAsync",     hle_CARDReadAsync },
    { "CARDWriteAsync",    hle_CARDWriteAsync },
};

// Functions intercepted by address rather than by Aurora name. These are host-
// owned hardware/OS boundaries below (or beside) the public SDK API: either a
// helper that busy-waits on unmodeled hardware (skipped) or a device the host
// implements itself (DVD, served from the ISO). Addresses are from the decomp
// symbol map (G4QE01).
typedef struct {
    u32         address;
    HleHandler  fn;
    const char* name;
} HleAddrEntry;

static const HleAddrEntry kAddrHandlers[] = {
    // DSP boot handshake: polls a DSP mailbox bit that never changes without a
    // modeled DSP. Audio is routed through Aurora, so skip the hardware init.
    { 0u, hle_noop,                     "__OSInitAudioSystem" },
    // Its counterpart on OSResetSystem: resets the DSP and waits for the reset
    // bit to clear, which it never does here (the game over's restart hung).
    { 0u, hle_noop,                     "__OSStopAudioSystem" },
    // AI sample-rate-converter calibration: times transitions of the AI sample
    // counter (__AIRegs[2]), which never ticks without modeled AI hardware.
    // Aurora owns audio, so skip the calibration (its result feeds only AI regs).
    { 0u, hle_noop,                     "__AI_SRC_INIT" },
    // MusyX's CPU-side sequencer and voice engine run unchanged. Only skip the
    // physical DSP task boot; audio.c executes the resulting AX command lists.
    { 0u, hle_salInitDsp,                "salInitDsp" },
    { 0u, hle_ARQPostRequest,            "ARQPostRequest" },
    { 0u, hle_return_zero,                "DSPCheckMailToDSP" },
    // Cache maintenance: this host has no dcache/icache model, so the SDK range
    // helpers would only loop dcbf/dcbi/dcbst/icbi (each a no-op through the
    // instruction fallback and a return to run_loop) over every 32-byte line:
    // ~0.6 MB a frame, 4.5 M round trips in 7 s at the "rats" cutscene. Skip them.
    { 0u, hle_noop,                     "DCInvalidateRange" },
    { 0u, hle_noop,                     "DCFlushRange" },
    { 0u, hle_noop,                     "DCStoreRange" },
    { 0u, hle_noop,                     "DCFlushRangeNoSync" },
    { 0u, hle_noop,                     "DCStoreRangeNoSync" },
    { 0u, hle_noop,                     "ICInvalidateRange" },
    // DVD: host serves the disc file system from the ISO (see DVD handlers).
    { 0u, hle_DVDInit,                  "DVDInit" },
    { 0u, hle_DVDConvertPathToEntrynum, "DVDConvertPathToEntrynum" },
    { 0u, hle_DVDFastOpen,              "DVDFastOpen" },
    { 0u, hle_DVDOpen,                  "DVDOpen" },
    { 0u, hle_DVDChangeDir,             "DVDChangeDir" },
    { 0u, hle_DVDGetCurrentDir,         "DVDGetCurrentDir" },
    { 0u, hle_DVDClose,                 "DVDClose" },
    { 0u, hle_DVDReadAsyncPrio,         "DVDReadAsyncPrio" },
    { 0u, hle_DVDGetCommandBlockStatus, "DVDGetCommandBlockStatus" },
    { 0u, hle_DVDGetDriveStatus,        "DVDGetDriveStatus" },
    { 0u, hle_DVDReadPrio,              "DVDReadPrio" },
    { 0u, hle_DVDReadAbsAsyncPrio,      "DVDReadAbsAsyncPrio" },
    { 0u, hle_DVDReadDiskID,            "DVDReadDiskID" },
    { 0u, hle_DVDInquiryAsync,          "DVDInquiryAsync" },
    { 0u, hle_DVDCancelAsync,           "DVDCancelAsync" },
    { 0u, hle_DVDCancel,                "DVDCancel" },
    { 0u, hle_DVDCancelAll,             "DVDCancelAll" },
    { 0u, hle_DVDCheckDisk,             "DVDCheckDisk" },
    { 0u, hle_DVDGetCurrentDiskID,      "DVDGetCurrentDiskID" },
    { 0u, hle_DVDReset,                 "DVDReset" },
    { 0u, hle_DVDPrepareStreamAsync,    "DVDPrepareStreamAsync" },
    { 0u, hle_DVDPrepareStreamAbsAsync, "DVDPrepareStreamAbsAsync" },
    { 0u, hle_DVDCancelStreamAsync,     "DVDCancelStreamAsync" },
    { 0u, hle_DVDStopStreamAtEndAsync,  "DVDStopStreamAtEndAsync" },
    { 0u, hle_DVDGetStreamErrorStatusAsync, "DVDGetStreamErrorStatusAsync" },
    { 0u, hle_DVDGetStreamPlayAddrAsync, "DVDGetStreamPlayAddrAsync" },
    // CARDFormatAsync is present in G4QE01 but absent from Aurora's SDK header
    // intersection, so it is registered by its exact game address.
    { 0u, hle_CARDFormatAsync,           "CARDFormatAsync" },
    { 0u, hle_OSGetResetButtonState,    "OSGetResetButtonState" },
    { 0u, hle___OSReboot,               "__OSReboot" },
    { 0u, hle_Reset,                    "Reset" },
    // Locked-cache DMA helpers. THP video decode writes into the 0xE0000000
    // scratch area and uses these to move rows into MEM1 texture planes.
    { 0u, hle_LCStoreBlocks,             "LCStoreBlocks" },
    { 0u, hle_LCLoadBlocks,              "LCLoadBlocks" },
    { 0u, hle_LCStoreData,               "LCStoreData" },
    { 0u, hle_noop,                      "LCQueueWait" },
#ifdef ED_AURORA
    { 0u, hle_PSMTXConcat,                "PSMTXConcat" },
    { 0u, hle_GXSetArray,                "GXSetArray" },
    { 0u, hle_GXBegin,                   "GXBegin" },
    { 0u, hle_GXCallDisplayList,          "GXCallDisplayList" },
#endif
    { 0u, hle_PADReset,                  "PADReset" },
    { 0u, hle_PADRecalibrate,            "PADRecalibrate" },
    { 0u, hle_PADInit,                   "PADInit" },
    { 0u, hle_PADRead,                   "PADRead" },
    { 0u, hle_PADControlMotor,           "PADControlMotor" },
    { 0u, hle_PADSetSpec,                "PADSetSpec" },
};

// Functions to trace (log name + first args) without intercepting: the
// recompiled body still runs. A debugging aid for bringing up the OS scheduler,
// VI, and interrupt paths. Enabled with ED_HLE_LOG=1.
typedef struct {
    u32         address;
    const char* name;
} HleTraceEntry;

static HleTraceEntry kTrace[] = {
    { 0u, "GXGetGPStatus" },
    { 0u, "GXGetFifoStatus" },
    { 0u, "GXEnableBreakPt" },
    { 0u, "GXDisableBreakPt" },
    { 0u, "OSSendMessage" },
    { 0u, "OSReceiveMessage" },
    { 0u, "VISetPostRetraceCallback" },
    { 0u, "VISetPreRetraceCallback" },
    { 0u, "GXSetBreakPtCallback" },
    { 0u, "GXSetDrawDoneCallback" },
    { 0u, "GXSetDrawDone" },
    { 0u, "GXFinishInterruptHandler" },
    { 0u, "OSJamMessage" },
    { 0u, "OSDisableScheduler" },
    { 0u, "VISetBlack" },
    { 0u, "GXCopyDisp" },
    { 0u, "VISetNextFrameBuffer" },
    { 0u, "OSEnableScheduler" },
    { 0u, "ARInit" },
    { 0u, "ARGetBaseAddress" },
    { 0u, "ARGetSize" },
    { 0u, "VIInit" },
    { 0u, "VIWaitForRetrace" },
    { 0u, "VIConfigure" },
    { 0u, "VISetNextFrameBuffer" },
    { 0u, "OSLoadContext" },
    { 0u, "__OSReschedule" },
    { 0u, "OSResumeThread" },
    { 0u, "__OSInterruptInit" },
    { 0u, "OSSleepThread" },
    { 0u, "OSSuspendThread" },
    { 0u, "GXSetDrawDone" },
    { 0u, "GXFinishInterruptHandler" },
};

static const char* trace_name(u32 address) {
    for (size_t i = 0; i < sizeof kTrace / sizeof kTrace[0]; i++)
        if (kTrace[i].address == address)
            return kTrace[i].name;
    return NULL;
}

// Notify hooks run *alongside* the recompiled function (it still executes), to
// observe a call and drive host state -- unlike kAddrHandlers, which replace it.

// FinishQueue in GXMisc.c for G4QE01. A thread sleeps here (in GXWaitDrawDone)
// only while a draw-done token is in flight (DrawDone == 0), so the wait itself
// is the faithful trigger for the Pixel Engine FINISH interrupt. By the time
// execution reaches this OSSleepThread call external interrupts are disabled, so
// committing here enqueues the waiter before interrupt_poll can deliver FINISH,
// matching a GPU completing submitted work. This covers both the standalone
// GXSetDrawDone+GXWaitDrawDone pair and the inlined GXDrawDone() (which never
// calls the standalone GXSetDrawDone -- the loading-screen GXDrawDone() used to
// hang here forever because nothing armed the finish).
#define GX_FINISH_QUEUE ED_GX_FINISH_QUEUE

// This game sets a draw-done callback and calls GXSetDrawDone once a frame
// without ever waiting in GXWaitDrawDone; the callback, run from the PE finish
// interrupt, receives the message the main loop sends just after. Raise the
// interrupt once the GPU would have finished, after that send.
static void notify_GXSetDrawDone(CPUState* cpu) {
    camera_held_end(cpu);  // the frame is drawn: what the player holds as the game has it
    interrupt_defer_pe_finish();
}

static void notify_GXSetDrawSync(CPUState* cpu) {
    interrupt_commit_pe_token((u16)hle_arg_u32(cpu, 0));
}

static void notify_PPCMtwpar(CPUState* cpu) {
    (void)cpu;
    mmio_write_gather_reset();
}

// The breakpoint moves to the current write pointer: the GPU can now run up to
// there, through the tokens held behind the old breakpoint (interrupt.c).
static void notify_GXEnableBreakPt(CPUState* cpu) {
    (void)cpu;
    interrupt_gp_breakpoint_moved();
}

static void notify_OSSleepThread(CPUState* cpu) {
    if (g_tome_copy_turn)
        fprintf(stderr, "[tome-copy] a thread sleeps in the copy's turn (lr 0x%08X)\n", cpu->lr);
    if (hle_arg_u32(cpu, 0) == GX_FINISH_QUEUE)
        interrupt_commit_pe_finish();
}

static void notify_DSPSendMailToDSP(CPUState* cpu) {
    audio_dsp_mail(cpu, hle_arg_u32(cpu, 0));
}

// B2.1 measure: per-entry call counts for certificate-demanded MusyX API surface.
// Names are G4QE01 symbols; addresses from symbols.txt / certificate_G4QE01.json.
typedef struct {
    u32 address;
    const char* name;
    u64 calls;
} AudioApiCounter;

static AudioApiCounter g_audio_api[] = {
    { 0u, "sndInit", 0 },
    { 0u, "sndQuit", 0 },
    { 0u, "sndIsInstalled", 0 },
    { 0u, "sndSetHooks", 0 },
    { 0u, "sndFXStartParaInfo", 0 },
    { 0u, "sndFXKeyOff", 0 },
    { 0u, "sndFXCheck", 0 },
    { 0u, "sndFXCtrl", 0 },
    { 0u, "sndFXCtrl14", 0 },
    { 0u, "sndFXAssignVolGroup2FXId", 0 },
    { 0u, "sndVolume", 0 },
    { 0u, "sndSilence", 0 },
    { 0u, "sndOutputMode", 0 },
    { 0u, "sndSetAuxProcessingCallbacks", 0 },
    { 0u, "sndStreamActivate", 0 },
    { 0u, "sndStreamDeactivate", 0 },
    { 0u, "sndStreamAllocEx", 0 },
    { 0u, "sndStreamFree", 0 },
    { 0u, "sndStreamMixParameterEx", 0 },
    { 0u, "sndStreamARAMUpdate", 0 },
    { 0u, "sndStreamLPFParameter", 0 },
    { 0u, "sndStreamADPCMParameter", 0 },
    { 0u, "sndStreamFrq", 0 },
    { 0u, "sndPushGroup", 0 },
    { 0u, "sndPopGroup", 0 },
    { 0u, "sndSeqPlayEx", 0 },
    { 0u, "sndStackSetCurrent", 0 },
    { 0u, "sndStackGetSize", 0 },
    { 0u, "sndStackAdd", 0 },
    { 0u, "sndStackGetARAMAddressRange", 0 },
    { 0u, "sndStackGetAvailableSampleMemory", 0 },
    { 0u, "sndSetSampleDataUploadCallback", 0 },
    { 0u, "sndAddEmitter2StudioPara", 0 },
    { 0u, "sndUpdateEmitter", 0 },
    { 0u, "sndRemoveEmitter", 0 },
    { 0u, "sndCheckEmitter", 0 },
    { 0u, "sndEmitterVoiceID", 0 },
    { 0u, "sndAddListenerEx", 0 },
    { 0u, "sndUpdateListener", 0 },
    { 0u, "sndRemoveListener", 0 },
    { 0u, "sndPitchUpOne", 0 },
    { 0u, "sndGetPitch", 0 },
    { 0u, "sndSin", 0 },
    { 0u, "sndAuxCallbackPrepareReverbSTD", 0 },
    { 0u, "sndAuxCallbackUpdateSettingsReverbSTD", 0 },
    { 0u, "sndAuxCallbackShutdownReverbSTD", 0 },
    { 0u, "sndAuxCallbackPrepareReverbHI", 0 },
    { 0u, "sndAuxCallbackUpdateSettingsReverbHI", 0 },
    { 0u, "sndAuxCallbackShutdownReverbHI", 0 },
    { 0u, "sndAuxCallbackPrepareDelay", 0 },
    { 0u, "sndAuxCallbackUpdateSettingsDelay", 0 },
    { 0u, "sndAuxCallbackShutdownDelay", 0 },
    { 0u, "sndAuxCallbackPrepareChorus", 0 },
    { 0u, "sndAuxCallbackUpdateSettingsChorus", 0 },
    { 0u, "sndAuxCallbackShutdownChorus", 0 },
};

static void notify_audio_api(CPUState* cpu) {
    for (size_t i = 0; i < sizeof g_audio_api / sizeof g_audio_api[0]; i++) {
        if (g_audio_api[i].address == cpu->pc) {
            g_audio_api[i].calls++;
            if (g_audio_log && (g_audio_api[i].calls <= 8u ||
                                (g_audio_api[i].calls % 64u) == 0u))
                fprintf(stderr,
                        "[audio] API %s #%llu r3=0x%08X r4=0x%08X r5=0x%08X\n",
                        g_audio_api[i].name,
                        (unsigned long long)g_audio_api[i].calls, cpu->gpr[3],
                        cpu->gpr[4], cpu->gpr[5]);
            return;
        }
    }
    if (g_audio_log)
        fprintf(stderr, "[audio] API 0x%08X r3=0x%08X r4=0x%08X r5=0x%08X\n",
                cpu->pc, cpu->gpr[3], cpu->gpr[4], cpu->gpr[5]);
}

void hle_audio_api_report(void) {
    if (!g_audio_log)
        return;
    u64 total = 0;
    u32 fired = 0;
    for (size_t i = 0; i < sizeof g_audio_api / sizeof g_audio_api[0]; i++) {
        if (g_audio_api[i].calls) {
            fired++;
            total += g_audio_api[i].calls;
        }
    }
    fprintf(stderr,
            "[audio] API report: entries=%zu fired=%u total_calls=%llu\n",
            sizeof g_audio_api / sizeof g_audio_api[0], fired,
            (unsigned long long)total);
    for (size_t i = 0; i < sizeof g_audio_api / sizeof g_audio_api[0]; i++) {
        if (g_audio_api[i].calls)
            fprintf(stderr, "[audio]   %-40s %llu\n", g_audio_api[i].name,
                    (unsigned long long)g_audio_api[i].calls);
    }
    for (size_t i = 0; i < sizeof g_audio_api / sizeof g_audio_api[0]; i++) {
        if (!g_audio_api[i].calls)
            fprintf(stderr, "[audio]   %-40s 0 (demanded, never called)\n",
                    g_audio_api[i].name);
    }
}

static void notify_OSResetSystem(CPUState* cpu) {
    fprintf(stderr,
            "[reset] OSResetSystem mode=%u code=0x%08X force-menu=%u lr=0x%08X\n",
            cpu->gpr[3], cpu->gpr[4], cpu->gpr[5], cpu->lr);
}

static void notify_SetNextState(CPUState* cpu) {
    if (g_mash_to_gameplay && g_mash_side_assigned &&
        !g_mash_route_complete) {
        if (g_mash_prematch && cpu->gpr[3] == 0x00000002u) {
            g_mash_route_complete = true;
            fprintf(stderr,
                    "[input] gameplay reached; controller is now neutral\n");
        } else if (!g_mash_prematch && cpu->gpr[3] == 0x00000100u) {
            g_mash_prematch = true;
            fprintf(stderr,
                    "[input] prematch reached; mashing A only\n");
        }
    }
    if (!g_state_log)
        return;
    const u32 manager = mem_read32(cpu, 0x803742B8u);
    fprintf(stderr,
            "[state] SetNextState next=0x%08X manager=0x%08X "
            "current=0x%08X pending=0x%08X lr=0x%08X\n",
            cpu->gpr[3], manager,
            manager ? mem_read32(cpu, manager + 8u) : 0u,
            manager ? mem_read32(cpu, manager + 0x0Cu) : 0u, cpu->lr);
}

static void notify_TaskManagerStartup(CPUState* cpu) {
    if (g_state_log)
        fprintf(stderr,
                "[state] TaskManager::Startup initial=0x%08X old=0x%08X "
                "lr=0x%08X timebase=%llu\n",
                cpu->gpr[3], mem_read32(cpu, 0x803742B8u), cpu->lr,
                (unsigned long long)cpu->timebase);
}

static void fe_route_pad_state(CPUState* cpu,
                               u16* buttons,
                               s8* stick_x,
                               s8* stick_y,
                               s8* err,
                               u16* just_pressed) {
    *buttons = 0;
    *stick_x = 0;
    *stick_y = 0;
    *err = -1;
    *just_pressed = 0;
    const u32 current = mem_read32(cpu, 0x80372FF0u); // PadStatus::s_Current
    if (current >= GC_RAM_BASE && current + 12u <= GC_RAM_BASE + cpu->ram_size) {
        *buttons = mem_read16(cpu, current);
        *stick_x = (s8)mem_read8(cpu, current + 2u);
        *stick_y = (s8)mem_read8(cpu, current + 3u);
        *err = (s8)mem_read8(cpu, current + 10u);
    }
    const u32 categories = mem_read32(cpu, 0x80372FF8u); // platpad.cpp padStatus
    if (categories >= GC_RAM_BASE &&
        categories + 0x382u <= GC_RAM_BASE + cpu->ram_size) {
        *just_pressed = mem_read16(cpu, categories + 0x380u);
    }
}

static bool fe_route_should_log(CPUState* cpu,
                                u16 buttons,
                                u16 just_pressed,
                                u64* last_frame) {
    const u64 frame = cpu->timebase / 675000ull;
    if (*last_frame == frame)
        return false;
    if (buttons != 0u || just_pressed != 0u || (frame % 120u) == 0u) {
        *last_frame = frame;
        return true;
    }
    return false;
}

static void notify_SHMainMenuUpdate(CPUState* cpu) {
    if (!g_state_log)
        return;
    static u64 s_last_frame = ~(u64)0;
    static s32 s_last_index = -999;
    const u32 self = cpu->gpr[3];
    const s32 index = (s32)mem_read32(cpu, self + 0x228u);
    const s32 count = (s32)mem_read32(cpu, self + 0x22Cu);
    const u32 flags = mem_read32(cpu, self + 0x234u);
    u16 buttons = 0;
    u16 just_pressed = 0;
    s8 stick_x = 0;
    s8 stick_y = 0;
    s8 err = -1;
    fe_route_pad_state(cpu, &buttons, &stick_x, &stick_y, &err, &just_pressed);
    const bool changed = index != s_last_index;
    if (changed || fe_route_should_log(cpu, buttons, just_pressed, &s_last_frame)) {
        fprintf(stderr,
                "[fe-route] main-menu frame=%llu self=0x%08X index=%d/%d "
                "flags=0x%08X pad=%04X jp=%04X stick=%d,%d err=%d%s\n",
                (unsigned long long)(cpu->timebase / 675000ull), self, index,
                count, flags, buttons, just_pressed, stick_x, stick_y, err,
                changed ? " index-change" : "");
        s_last_index = index;
    }
}

static void notify_IChooseSideCheckControllers(CPUState* cpu) {
    const u32 self = cpu->gpr[3];
    if (g_mash_to_gameplay && (s32)mem_read32(cpu, self) >= 0)
        g_mash_side_assigned = true;
    if (!g_state_log)
        return;
    static u64 s_last_frame = ~(u64)0;
    static s32 s_last_sides[4] = { -999, -999, -999, -999 };
    static u8 s_last_ready = 0xFFu;
    s32 sides[4];
    u8 ready = 0;
    for (u32 i = 0; i < 4u; ++i) {
        sides[i] = (s32)mem_read32(cpu, self + i * 4u);
        ready |= (mem_read8(cpu, self + 0x10u + i) != 0u) ? (u8)(1u << i) : 0u;
    }
    const s32 disabled_side = (s32)cpu->gpr[4];
    const s32 context = (s32)mem_read32(cpu, self + 0x9Cu);
    u16 buttons = 0;
    u16 just_pressed = 0;
    s8 stick_x = 0;
    s8 stick_y = 0;
    s8 err = -1;
    fe_route_pad_state(cpu, &buttons, &stick_x, &stick_y, &err, &just_pressed);
    bool changed = ready != s_last_ready;
    for (u32 i = 0; i < 4u; ++i)
        changed |= sides[i] != s_last_sides[i];
    if (changed || fe_route_should_log(cpu, buttons, just_pressed, &s_last_frame)) {
        fprintf(stderr,
                "[fe-route] choose-side frame=%llu self=0x%08X disabled=%d "
                "context=%d sides=%d,%d,%d,%d ready=0x%X pad=%04X jp=%04X "
                "stick=%d,%d err=%d%s\n",
                (unsigned long long)(cpu->timebase / 675000ull), self,
                disabled_side, context, sides[0], sides[1], sides[2], sides[3],
                ready, buttons, just_pressed, stick_x, stick_y, err,
                changed ? " side-change" : "");
        for (u32 i = 0; i < 4u; ++i)
            s_last_sides[i] = sides[i];
        s_last_ready = ready;
    }
}

static void notify_IChooseSidePositionController(CPUState* cpu) {
    if (!g_state_log)
        return;
    const u32 self = cpu->gpr[3];
    const s32 pad = (s32)cpu->gpr[4];
    if (pad < 0 || pad >= 4)
        return;
    const s32 side = (s32)mem_read32(cpu, self + (u32)pad * 4u);
    fprintf(stderr,
            "[fe-route] position-controller frame=%llu self=0x%08X pad=%d "
            "side=%d tween=%u vis=%u\n",
            (unsigned long long)(cpu->timebase / 675000ull), self, pad, side,
            cpu->gpr[5] != 0u, cpu->gpr[6] != 0u);
}

static void notify_MovieStart(CPUState* cpu) {
    if (!g_movie_log)
        return;
    char filename[256];
    hle_read_cstr(cpu, hle_arg_u32(cpu, 0), filename, sizeof filename);
    fprintf(stderr, "[movie] MovieStart file='%s' sound=%u loop=%u lr=0x%08X\n",
            filename, hle_arg_u32(cpu, 1), hle_arg_u32(cpu, 2), cpu->lr);
}

static void notify_MovieStop(CPUState* cpu) {
    if (g_movie_log)
        fprintf(stderr, "[movie] MovieStop lr=0x%08X\n", cpu->lr);
}

static void notify_MoviePlay(CPUState* cpu) {
    static unsigned count = 0;
    if (!g_movie_log)
        return;
    ++count;
    if (count <= 120u || (count % 60u) == 0u)
        fprintf(stderr, "[movie] MoviePlay #%u lr=0x%08X\n", count, cpu->lr);
}

static void notify_THPSimpleOpen(CPUState* cpu) {
    if (!g_movie_log)
        return;
    char filename[256];
    hle_read_cstr(cpu, hle_arg_u32(cpu, 0), filename, sizeof filename);
    fprintf(stderr, "[movie] THPSimpleOpen file='%s' lr=0x%08X\n",
            filename, cpu->lr);
}

static void notify_THPSimpleSetBuffer(CPUState* cpu) {
    if (g_movie_log)
        fprintf(stderr, "[movie] THPSimpleSetBuffer buffer=0x%08X lr=0x%08X\n",
                hle_arg_u32(cpu, 0), cpu->lr);
}

static void notify_THPSimplePreLoad(CPUState* cpu) {
    if (g_movie_log)
        fprintf(stderr, "[movie] THPSimplePreLoad loop=%u lr=0x%08X\n",
                hle_arg_u32(cpu, 0), cpu->lr);
}

static void notify_THPSimpleDecode(CPUState* cpu) {
    static unsigned count = 0;
    if (!g_movie_log)
        return;
    ++count;
    if (count <= 120u || (count % 60u) == 0u)
        fprintf(stderr, "[movie] THPSimpleDecode #%u audio-track=%d lr=0x%08X\n",
                count, (s32)hle_arg_u32(cpu, 0), cpu->lr);
}

static void notify_THPVideoDecode(CPUState* cpu) {
    static unsigned count = 0;
    if (!g_movie_log)
        return;
    ++count;
    if (count <= 120u || (count % 60u) == 0u)
        fprintf(stderr,
                "[movie] THPVideoDecode #%u frame=0x%08X y=0x%08X "
                "u=0x%08X v=0x%08X work=0x%08X lr=0x%08X\n",
                count, hle_arg_u32(cpu, 0), hle_arg_u32(cpu, 1),
                hle_arg_u32(cpu, 2), hle_arg_u32(cpu, 3),
                hle_arg_u32(cpu, 4), cpu->lr);
}

static u32 movie_plane_hash(CPUState* cpu, u32 guest_data, u32 bytes) {
    u32 hash_bytes = bytes;
    return guest_hash(cpu, guest_data, hash_bytes, &hash_bytes);
}

static void log_thp_simple_control(CPUState* cpu, const char* tag,
                                   unsigned count) {
    const u32 base = THP_SIMPLE_CONTROL;
    const u32 y = mem_read32(cpu, base + 0x15Cu);
    const u32 u = mem_read32(cpu, base + 0x160u);
    const u32 v = mem_read32(cpu, base + 0x164u);
    const u32 next_decode = mem_read32(cpu, base + 0x7Cu);
    const u32 read_idx = mem_read32(cpu, base + 0x80u);
    const u32 rb_index = next_decode < 16u ? next_decode : 0u;
    const u32 rb = base + 0x9Cu + rb_index * 12u;
    const u32 rb_ptr = mem_read32(cpu, rb + 0u);
    const u32 rb_frame = mem_read32(cpu, rb + 4u);
    const u32 rb_valid = mem_read32(cpu, rb + 8u);
    fprintf(stderr,
            "[movie] %s #%u ret=%d hid2=0x%08X open=%u playing=%u "
            "audio=%u loop=%u audioExist=%u nextDecode=%u readIdx=%u "
            "nextSize=%u frameCount=%u texFrame=%d rb[%u]={ptr=0x%08X "
            "frame=%u valid=%u} y=0x%08X:%08X u=0x%08X:%08X "
            "v=0x%08X:%08X\n",
            tag, count, (s32)cpu->gpr[3], cpu->hid2,
            mem_read32(cpu, base + 0x68u),
            mem_read8(cpu, base + 0x6Cu), mem_read8(cpu, base + 0x6Du),
            mem_read8(cpu, base + 0x6Eu), mem_read8(cpu, base + 0x6Fu),
            next_decode, read_idx, mem_read32(cpu, base + 0x84u),
            mem_read32(cpu, base + 0x88u),
            (s32)mem_read32(cpu, base + 0x168u), rb_index, rb_ptr, rb_frame,
            rb_valid, y, movie_plane_hash(cpu, y, 512u * 416u),
            u, movie_plane_hash(cpu, u, 256u * 208u),
            v, movie_plane_hash(cpu, v, 256u * 208u));
}

static void notify_THPVideoDecodeReturn(CPUState* cpu) {
    static unsigned count = 0;
    if (!g_movie_log)
        return;
    ++count;
    if ((s32)cpu->gpr[3] != 0 || count <= 120u || (count % 60u) == 0u)
        log_thp_simple_control(cpu, "THPVideoDecodeReturn", count);
}

static void notify_THPSimpleDecodeReturn(CPUState* cpu) {
    static unsigned count = 0;
    if (!g_movie_log)
        return;
    ++count;
    const s32 ret = (s32)cpu->gpr[3];
    const bool unusual = ret != 0 && ret != 3;
    if (unusual || count <= 120u || (count % 60u) == 0u)
        log_thp_simple_control(cpu, "THPSimpleDecodeReturn", count);
}

static void notify_LCEnable(CPUState* cpu) {
    if (g_movie_log)
        fprintf(stderr, "[movie] LCEnable entry hid2=0x%08X lr=0x%08X\n",
                cpu->hid2, cpu->lr);
}

static void notify_LCDisable(CPUState* cpu) {
    if (g_movie_log)
        fprintf(stderr, "[movie] LCDisable entry hid2=0x%08X lr=0x%08X\n",
                cpu->hid2, cpu->lr);
}


static const HleAddrEntry kNotify[] = {
    { 0u, notify_OSSleepThread,  "OSSleepThread" },
    { 0u, notify_GXSetDrawSync,  "GXSetDrawSync" },
    { 0u, notify_GXSetDrawDone,  "GXSetDrawDone" },
    { 0u, notify_GXEnableBreakPt, "GXEnableBreakPt" },
    { 0u, notify_PPCMtwpar,       "PPCMtwpar" },
    { 0u, notify_DSPSendMailToDSP, "DSPSendMailToDSP" },
    // B2.1: full certificate-demanded MusyX API surface (notify-only counters).
    { 0u, notify_audio_api, "sndInit" },
    { 0u, notify_audio_api, "sndQuit" },
    { 0u, notify_audio_api, "sndIsInstalled" },
    { 0u, notify_audio_api, "sndSetHooks" },
    { 0u, notify_audio_api, "sndFXStartParaInfo" },
    { 0u, notify_audio_api, "sndFXKeyOff" },
    { 0u, notify_audio_api, "sndFXCheck" },
    { 0u, notify_audio_api, "sndFXCtrl" },
    { 0u, notify_audio_api, "sndFXCtrl14" },
    { 0u, notify_audio_api, "sndFXAssignVolGroup2FXId" },
    { 0u, notify_audio_api, "sndVolume" },
    { 0u, notify_audio_api, "sndSilence" },
    { 0u, notify_audio_api, "sndOutputMode" },
    { 0u, notify_audio_api, "sndSetAuxProcessingCallbacks" },
    { 0u, notify_audio_api, "sndStreamActivate" },
    { 0u, notify_audio_api, "sndStreamDeactivate" },
    { 0u, notify_audio_api, "sndStreamAllocEx" },
    { 0u, notify_audio_api, "sndStreamFree" },
    { 0u, notify_audio_api, "sndStreamMixParameterEx" },
    { 0u, notify_audio_api, "sndStreamARAMUpdate" },
    { 0u, notify_audio_api, "sndStreamLPFParameter" },
    { 0u, notify_audio_api, "sndStreamADPCMParameter" },
    { 0u, notify_audio_api, "sndStreamFrq" },
    { 0u, notify_audio_api, "sndPushGroup" },
    { 0u, notify_audio_api, "sndPopGroup" },
    { 0u, notify_audio_api, "sndSeqPlayEx" },
    { 0u, notify_audio_api, "sndStackSetCurrent" },
    { 0u, notify_audio_api, "sndStackGetSize" },
    { 0u, notify_audio_api, "sndStackAdd" },
    { 0u, notify_audio_api, "sndStackGetARAMAddressRange" },
    { 0u, notify_audio_api, "sndStackGetAvailableSampleMemory" },
    { 0u, notify_audio_api, "sndSetSampleDataUploadCallback" },
    { 0u, notify_audio_api, "sndAddEmitter2StudioPara" },
    { 0u, notify_audio_api, "sndUpdateEmitter" },
    { 0u, notify_audio_api, "sndRemoveEmitter" },
    { 0u, notify_audio_api, "sndCheckEmitter" },
    { 0u, notify_audio_api, "sndEmitterVoiceID" },
    { 0u, notify_audio_api, "sndAddListenerEx" },
    { 0u, notify_audio_api, "sndUpdateListener" },
    { 0u, notify_audio_api, "sndRemoveListener" },
    { 0u, notify_audio_api, "sndPitchUpOne" },
    { 0u, notify_audio_api, "sndGetPitch" },
    { 0u, notify_audio_api, "sndSin" },
    { 0u, notify_audio_api, "sndAuxCallbackPrepareReverbSTD" },
    { 0u, notify_audio_api, "sndAuxCallbackUpdateSettingsReverbSTD" },
    { 0u, notify_audio_api, "sndAuxCallbackShutdownReverbSTD" },
    { 0u, notify_audio_api, "sndAuxCallbackPrepareReverbHI" },
    { 0u, notify_audio_api, "sndAuxCallbackUpdateSettingsReverbHI" },
    { 0u, notify_audio_api, "sndAuxCallbackShutdownReverbHI" },
    { 0u, notify_audio_api, "sndAuxCallbackPrepareDelay" },
    { 0u, notify_audio_api, "sndAuxCallbackUpdateSettingsDelay" },
    { 0u, notify_audio_api, "sndAuxCallbackShutdownDelay" },
    { 0u, notify_audio_api, "sndAuxCallbackPrepareChorus" },
    { 0u, notify_audio_api, "sndAuxCallbackUpdateSettingsChorus" },
    { 0u, notify_audio_api, "sndAuxCallbackShutdownChorus" },
    { 0u, notify_OSResetSystem,    "OSResetSystem" },
    { 0u, notify_LCEnable,          "LCEnable" },
    { 0u, notify_LCDisable,         "LCDisable" },
#ifdef ED_AURORA
    { 0u, notify_GXCopyDisp,      "GXCopyDisp" },
    { 0u, notify_GXCopyTex,       "GXCopyTex" },
    { 0u, notify_GXLoadTexObj,    "GXLoadTexObj" },
    { 0u, notify_GXLoadTlut,      "GXLoadTlut" },
    { 0u, notify_GXLoadPosMtxImm, "GXLoadPosMtxImm" },
    { 0u, notify_VIWaitForRetrace,"VIWaitForRetrace" },
    { 0u, notify_VIConfigure,     "VIConfigure" },
    { 0u, notify_VIFlush,         "VIFlush" },
    { 0u, notify_VISetNextFrameBuffer, "VISetNextFrameBuffer" },
    { 0u, notify_VISetBlack,      "VISetBlack" },
#endif
};

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------
// Lowest address holding loaded code. A call below this is a branch through an
// uninitialized/optional function pointer (e.g. a NULL callback hook).
#define HLE_CODE_BASE 0x80003000u
#define HLE_CODE_LIMIT 0x80240000u  // past the end of text in GEDE01 and GEDP01
#define HLE_DISPATCH_COUNT ((HLE_CODE_LIMIT - HLE_CODE_BASE) / 4u)

static HleDispatchEntry g_hle_dispatch[HLE_DISPATCH_COUNT];
static unsigned g_script_watch;  // ED_ROOM_LOG: script calls left to list

// Table entries name their function; the address comes from the symbol table
// built from the user's DOL (tools/symbols.py), so one table serves every build.
static u32 hle_entry_address(u32 address, const char* name) {
    return address != 0u ? address : sdk_symbol_address(name);
}

static HleDispatchEntry* hle_dispatch_entry(u32 address) {
    if (address < HLE_CODE_BASE || address >= HLE_CODE_LIMIT ||
        (address & 3u) != 0u)
        return NULL;
    return &g_hle_dispatch[(address - HLE_CODE_BASE) / 4u];
}

static void initialize_hle_dispatch(void) {
    memset(g_hle_dispatch, 0, sizeof g_hle_dispatch);
    for (size_t i = 0; i < sizeof kTrace / sizeof kTrace[0]; i++)
        kTrace[i].address = sdk_symbol_address(kTrace[i].name);
    for (size_t i = 0; i < sizeof g_audio_api / sizeof g_audio_api[0]; i++)
        g_audio_api[i].address = sdk_symbol_address(g_audio_api[i].name);
#ifdef ED_AURORA
    g_gx_dirty_state_helper = sdk_symbol_address("__GXSetDirtyState");
    g_gx_flush_prim_helper = sdk_symbol_address("__GXSendFlushPrim");
#endif

    for (size_t i = 0; i < sizeof kHandlers / sizeof kHandlers[0]; i++) {
        HleDispatchEntry* entry =
            hle_dispatch_entry(sdk_symbol_address(kHandlers[i].name));
        if (entry != NULL)
            entry->intercept = kHandlers[i].fn;
    }
    // ED_GX_HLE=1 keeps the Strikers host's GX shortcuts (GXSetArray, GXBegin,
    // GXCallDisplayList), which feed the live Aurora gx layer. gxcore, the
    // default renderer, decodes the game's own FIFO stream and resolves arrays
    // and display lists from guest memory, so by default they are left out.
    const char* gx_hle = getenv("ED_GX_HLE");
    const bool use_gx_hle = gx_hle != NULL && gx_hle[0] == '1';
    for (size_t i = 0; i < sizeof kAddrHandlers / sizeof kAddrHandlers[0]; i++) {
        if (!use_gx_hle && (strcmp(kAddrHandlers[i].name, "GXSetArray") == 0 ||
                            strcmp(kAddrHandlers[i].name, "GXBegin") == 0 ||
                            strcmp(kAddrHandlers[i].name, "GXCallDisplayList") == 0))
            continue;
        HleDispatchEntry* entry =
            hle_dispatch_entry(hle_entry_address(kAddrHandlers[i].address, kAddrHandlers[i].name));
        if (entry != NULL)
            entry->intercept = kAddrHandlers[i].fn;
    }
    for (size_t i = 0; i < sizeof kNotify / sizeof kNotify[0]; i++) {
        // The texture and TLUT bindings feed only the live Aurora gx layer
        // (emit_gx_resource_metadata, from the GX shortcuts above). gxcore reads
        // textures from the FIFO stream itself, so without ED_GX_HLE these hooks
        // would only hash every loaded texture on the game thread (and run the
        // Strikers host's THP movie-plane check, which reads a G4QE01 address).
        if (!use_gx_hle && (strcmp(kNotify[i].name, "GXLoadTexObj") == 0 ||
                            strcmp(kNotify[i].name, "GXLoadTlut") == 0))
            continue;
        HleDispatchEntry* entry =
            hle_dispatch_entry(hle_entry_address(kNotify[i].address, kNotify[i].name));
        if (entry != NULL)
            entry->notify = kNotify[i].fn;
    }
}

#ifdef ED_AURORA
extern CPUState dolrecomp_cpu;  // main.c
static void sync_widescreen_option(CPUState* cpu);
static bool continue_book_reload(CPUState* cpu);

// Once a game frame (VIFlush): the frames per second in the window title,
// refreshed each second (ED_NO_FPS=1 leaves the title alone), the game's
// widescreen option, and the in-game menu.
void hle_frame_boundary(void) {
    report_frame();  // the hang watchdog's sign of life
    aurora_hdr_set_movie(g_movie_file != 0u);
    static int fps_env = -1;
    static unsigned frames;
    static struct timespec mark;
    static bool shown;
    if (fps_env < 0)
        fps_env = getenv("ED_NO_FPS") == NULL;
    const bool fps_on = fps_env && g_settings.fps_counter;
    if (!fps_on && shown) {
        VISetWindowTitle("Eternal Darkness");
        shown = false;
    }
    if (fps_on) {
        shown = true;
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        frames++;
        if (mark.tv_sec == 0) {
            mark = now;
            frames = 0;
        } else {
            const double dt = (double)(now.tv_sec - mark.tv_sec) +
                              (double)(now.tv_nsec - mark.tv_nsec) * 1e-9;
            if (dt >= 1.0) {
                char title[64];
                snprintf(title, sizeof title, "Eternal Darkness \u2014 %.0f fps",
                         (double)frames / dt);
                VISetWindowTitle(title);
                mark = now;
                frames = 0;
            }
        }
    }
    sync_widescreen_option(&dolrecomp_cpu);
    // Esc / the controller's Back button opens the in-game menu here, at
    // a frame boundary, with the frame just drawn behind it. Not in a turn of player 2's Tome:
    // memory then is its copy, which a save state would take for the game.
    if (!g_tome_copy_turn)
        frontend_frame();
}
#endif

static bool hle_dispatch(CPUState* cpu, u32 address) {
    if (address == HLE_CALLBACK_RETURN && g_irq_callback.stage != 0u) {
        continue_irq_callback(cpu);
        return true;
    }
#ifdef ED_AURORA
    if (address == HLE_DISPLAY_LIST_RETURN && g_display_list_call.active) {
        continue_GXCallDisplayList(cpu);
        return true;
    }
    if (address == HLE_GX_BEGIN_RETURN && g_gx_begin.active) {
        continue_GXBegin(cpu);
        return true;
    }
    if (address == HLE_BOOK_RETURN && continue_book_reload(cpu))
        return true;
    if (address == HLE_CHARACTER_RETURN && continue_character_load(cpu))
        return true;
    if (address == HLE_PACK_RETURN && continue_pack_load(cpu))
        return true;
    if (address == HLE_SPAWN_RETURN && continue_spawn_create(cpu))
        return true;
    if (address == HLE_TRACKS_RETURN && continue_tracks_load(cpu))
        return true;
    if (address == HLE_USE_RETURN && continue_coop_use(cpu))
        return true;
#endif

    // Call through a NULL (or near-NULL) function pointer. GameCube SDK code
    // calls optional callback hooks (thread-switch callbacks, etc.) that default
    // to NULL; the recompiled `blrl` already saved the return address in lr, so
    // model the absent callback as a no-op and return to the caller instead of
    // halting. Loud + counted so genuine wild branches stay visible.
    if (address < HLE_CODE_BASE) {
        // __OSInitMemoryProtection calls Config24MB (Config48MB on a 48 MB
        // devkit) through its physical address with translation off, to set
        // the BATs. There is no MMU here: skip it, and say so by name.
        if (address != 0u) {
            const u32 cached = address | GC_RAM_BASE;
            const char* config = cached == sdk_symbol_address("Config24MB")   ? "Config24MB"
                                 : cached == sdk_symbol_address("Config48MB") ? "Config48MB"
                                                                              : NULL;
            if (config != NULL) {
                fprintf(stderr, "[hle] %s (real-mode call) skipped\n", config);
                hle_return(cpu);
                return true;
            }
        }
        static unsigned long null_calls = 0;
        if (cpu->lr >= HLE_CODE_BASE) {
            if (g_hle_log || null_calls < 8)
                fprintf(stderr,
                        "[hle] null-pointer call to 0x%08X from lr=0x%08X "
                        "(treating as no-op callback, #%lu)\n",
                        address, cpu->lr, ++null_calls);
            else
                ++null_calls;
            hle_return(cpu);
            return true;
        }
        return false;  // lr also bad -> let the runtime stop and report
    }

    // ED_TRACE_FN=name[,name...]: log each call of those functions with its
    // caller and first arguments, without the full ED_HLE_LOG firehose.
    // ED_TRACE_CHAIN=1: instead, each distinct chain of callers once (the
    // return address, then each saved one up the stack's back chain).
    // ED_TRACE_FROM=frame: only from that game frame (timebase / 675000) on.
    static long long trace_from = -1;
    if (trace_from < 0)
        trace_from = getenv("ED_TRACE_FROM") != NULL ? atoll(getenv("ED_TRACE_FROM")) : 0;
    if (g_trace_fn_count != 0u && cpu->timebase / 675000ull >= (u64)trace_from) {
        static int chain = -1;
        if (chain < 0)
            chain = getenv("ED_TRACE_CHAIN") != NULL;
        for (u32 i = 0; chain && i < g_trace_fn_count; i++)
            if (g_trace_fn[i] == address) {
                u32 lrs[10] = {cpu->lr};
                int n = 1;
                for (u32 sp = cpu->gpr[1]; n < 10;) {
                    const u32 back = guest_ram_range_valid(cpu, sp, 4u) ? mem_read32(cpu, sp) : 0u;
                    if (!guest_ram_range_valid(cpu, back, 8u) || back <= sp)
                        break;
                    lrs[n++] = mem_read32(cpu, back + 4u);
                    sp = back;
                }
                u64 h = address;
                for (int k = 0; k < n; k++)
                    h = (h ^ lrs[k]) * 1099511628211ull;
                static u64 seen[1024];
                static unsigned count;
                bool known = false;
                for (unsigned k = 0; k < count; k++)
                    known = known || seen[k] == h;
                if (known || count >= 1024u)
                    break;
                seen[count++] = h;
                fprintf(stderr, "[chain] 0x%08X r3=0x%08X frame=%llu <-", address, cpu->gpr[3],
                        (unsigned long long)(cpu->timebase / 675000ull));
                for (int k = 0; k < n; k++)
                    fprintf(stderr, " 0x%08X", lrs[k]);
                fputc('\n', stderr);
            }
        for (u32 i = 0; !chain && i < g_trace_fn_count; i++)
            if (g_trace_fn[i] == address)
            {
                char name[16];
                const char* sym = sdk_symbol_name(address);
                if (sym == NULL) {
                    snprintf(name, sizeof name, "0x%08X", address);
                    sym = name;
                }
                fprintf(stderr, "[fn] %s lr=0x%08X r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X "
                        "f1=%g f2=%g f3=%g msr.fp=%u",
                        sym, cpu->lr, cpu->gpr[3],
                        cpu->gpr[4], cpu->gpr[5], cpu->gpr[6], cpu->fpr[1],
                        cpu->fpr[2], cpu->fpr[3], (cpu->msr >> 13) & 1u);
                // ED_TRACE_R3=off,off..: also print the words at r3+off (hex
                // and as float), for reading fields of the object passed in r3.
                const char* offs = getenv("ED_TRACE_R3");
                for (const char* o = offs; o != NULL && *o != '\0';) {
                    char* end;
                    u32 off = (u32)strtoul(o, &end, 0);
                    u32 ea = cpu->gpr[3] + off;
                    if (ea >= 0x80000000u && ea < 0x81800000u) {
                        u32 w = mem_read32(cpu, ea);
                        float f;
                        memcpy(&f, &w, 4);
                        fprintf(stderr, " [%u]=0x%08X(%g)", off, w, f);
                    }
                    o = *end == ',' ? end + 1 : end;
                    if (end == o && *end != ',') break;
                }
                fputc('\n', stderr);
            }
    }
    // ED_DL_CHECK=1: at each GXCallDisplayList, summarise the list's bytes
    // (non-zero count, draw-opcode count, FNV hash, first bytes).
    {
        static int dl_check = -1;
        static u32 call_dl;
        if (dl_check < 0) {
            dl_check = getenv("ED_DL_CHECK") != NULL;
            call_dl = sdk_symbol_address("GXCallDisplayList");
        }
        if (dl_check && address == call_dl) {
            const u32 base = cpu->gpr[3], size = cpu->gpr[4];
            u32 nonzero = 0, draws = 0;
            u64 h = 1469598103934665603ull;
            u8 head[12] = {0};
            for (u32 i = 0; i < size && i < 0x100000u; i++) {
                const u8 b = mem_read8(cpu, base + i);
                if (i < sizeof head) head[i] = b;
                nonzero += b != 0u;
                h = (h ^ b) * 1099511628211ull;
            }
            // Walk the command stream from the start while it parses simply.
            for (u32 i = 0; i < size;) {
                const u8 c = mem_read8(cpu, base + i);
                if (c == 0x00u) { i++; continue; }
                if (c == 0x08u) { i += 6; continue; }
                if (c == 0x10u) { u32 n = (mem_read16(cpu, base + i + 1) & 0xFu) + 1u; i += 5 + 4 * n; continue; }
                if (c == 0x61u) { i += 5; continue; }
                if ((c & 0xF8u) >= 0x20u && (c & 0xF8u) <= 0x38u) { i += 5; continue; }
                if ((c & 0xC0u) == 0x80u) { draws++; break; }
                break;
            }
            fprintf(stderr, "[dl] lr=0x%08X addr=0x%08X size=0x%X nonzero=%u first_draw=%u fnv=%016llx head=%02X%02X%02X%02X%02X%02X%02X%02X\n",
                    cpu->lr, base, size, nonzero, draws, (unsigned long long)h,
                    head[0], head[1], head[2], head[3], head[4], head[5], head[6], head[7]);
        }
    }
#ifdef ED_AURORA
    // With the GX thread the backend runs this on the main thread after the
    // frame is shown (dol_aurora_set_frame_hook).
    if (address == g_call_prof_vi && g_call_prof_vi != 0u &&
        !dol_aurora_gx_threaded())
        hle_frame_boundary();
#endif
    if (g_call_prof_fn != 0u) {
        if (address == g_call_prof_fn) {
            for (u32 i = 0; i < 64u; i++)
                if (g_call_prof[i].lr == cpu->lr || g_call_prof[i].n == 0u) {
                    g_call_prof[i].lr = cpu->lr;
                    g_call_prof[i].n++;
                    break;
                }
        } else if (address == g_call_prof_vi) {
            static u32 frame;
            fprintf(stderr, "[callprof] %u:", frame++);
            for (u32 i = 0; i < 64u && g_call_prof[i].n != 0u; i++)
                fprintf(stderr, " %08X=%u", g_call_prof[i].lr, g_call_prof[i].n);
            fputc('\n', stderr);
            memset(g_call_prof, 0, sizeof g_call_prof);
        }
    }
    HleDispatchEntry* entry = hle_dispatch_entry(address);
    if (entry == NULL)
        return false;
    if (g_tome_copy_turn && ((entry->intercept != NULL && tome_copy_hook_off(entry->intercept)) ||
                             (entry->notify != NULL && tome_copy_hook_off(entry->notify))))
        return false;

    if (entry->intercept != NULL) {
        if (g_hle_log) {
            const char* name = sdk_symbol_name(address);
            if (name == NULL) {
                for (size_t i = 0;
                     i < sizeof kAddrHandlers / sizeof kAddrHandlers[0]; i++)
                    if (kAddrHandlers[i].address == address) {
                        name = kAddrHandlers[i].name;
                        break;
                    }
            }
            fprintf(stderr, "[hle] %-28s 0x%08X -> host\n",
                    name != NULL ? name : "(unnamed)", address);
        }
        g_hle_tail_call = false;
        g_hle_passthrough = false;
        entry->intercept(cpu);
        if (!g_hle_passthrough) {
            if (!g_hle_tail_call)
                hle_return(cpu);
            return true;
        }
    }

    // Observe-only hooks: drive host state, then fall through so the recompiled
    // function still runs.
    if (entry->notify != NULL)
        entry->notify(cpu);

    if (g_hle_log) {
        const char* tn = trace_name(address);
        if (tn != NULL)
            fprintf(stderr, "[trace] %-22s lr=0x%08X r3=0x%08X r4=0x%08X r5=0x%08X\n",
                    tn, cpu->lr, cpu->gpr[3], cpu->gpr[4], cpu->gpr[5]);
        const char* name = sdk_symbol_name(address);
        if (name != NULL)
            fprintf(stderr, "[hle] %-28s 0x%08X (recompiled)\n", name,
                    address);
    }
    return false;
}

static DolMemoryCard* open_card(const char* path, const char* slot) {
    DolMemoryCardConfig config;
    memset(&config, 0, sizeof config);
    config.path = path;
    config.size_mbits = 4;
    config.encoding = 0;
    memcpy(config.game_code, ED_GAME_ID, 4);
    memcpy(config.company, ED_GAME_ID + 4, 2);
    DolMemoryCard* card = dol_card_open(&config);
    if (card == NULL)
        return NULL;

    u16 size_mbits = 0;
    u32 sector_size = 0;
    if (dol_card_probe(card, &size_mbits, &sector_size) != DOL_CARD_RESULT_READY) {
        dol_card_close(card);
        return NULL;
    }
    fprintf(stderr,
            "[card] slot %s: %s (%u Mbit, %u-byte sectors, serial %016llX)\n", slot,
            path != NULL && path[0] != '\0' ? path : "(memory)",
            size_mbits, sector_size,
            (unsigned long long)dol_card_serial(card));
    return card;
}

bool hle_card_open(const char* path) {
    hle_card_close();
    g_memory_card = open_card(path, "A");
    return g_memory_card != NULL;
}

bool hle_card_open_b(const char* path) {
    dol_card_close(g_memory_card_b);
    g_memory_card_b = open_card(path, "B");
    return g_memory_card_b != NULL;
}

void hle_card_close(void) {
    dol_card_close(g_memory_card);
    g_memory_card = NULL;
    dol_card_close(g_memory_card_b);
    g_memory_card_b = NULL;
    g_callback_read = 0;
    g_callback_count = 0;
    g_callback_active = false;
    memset(&g_callback_context, 0, sizeof g_callback_context);
}

// Block chaining (DolRecomp cpu.h): generated code stays inside its chunk across
// loops, calls and returns unless the target is in this table. Every address
// hle_dispatch acts on is a stop: intercepts, notify hooks, VIFlush (the frame
// boundary), and the debug hooks (ED_TRACE_FN, ED_CALL_PROFILE, ED_DL_CHECK).
// ED_NO_CHAIN=1, or the full ED_HLE_LOG trace, sends every branch through
// hle_dispatch as before.
static u8 g_chain_stop[HLE_DISPATCH_COUNT];
const u8* dolrecomp_chain_stop;  // read by the generated code (generated.h)
u32 dolrecomp_chain_stop_base;
u32 dolrecomp_chain_stop_count;
u32 dolrecomp_chain_budget;  // shared by nested chunk calls (generated.h)
u32 dolrecomp_call_depth;
// The same table as one byte per word of MEM1, nonzero where a branch may
// chain, for chunks built with DOLRECOMP_STATIC_CPU (a constant target is one
// byte test). Only the HLE code range is ever written; the rest stays zero.
#define CHAIN_TABLE_SIZE 0x600000u  // generated.h DOLRECOMP_CHAIN_TABLE_SIZE
u8 dolrecomp_chain_table[CHAIN_TABLE_SIZE];

// Chain stops are read once, by publish_codegen_options (which also fills
// dolrecomp_chain_table): an intercept, notify or debug hook installed after
// it must call it again, or chunks keep chaining past the new hook.
static void chain_stop_at(u32 address) {
    if (address >= HLE_CODE_BASE && address < HLE_CODE_LIMIT)
        g_chain_stop[(address - HLE_CODE_BASE) / 4u] = 1u;
}

// Fast RAM access in the generated code (generated.h): bit 0 loads, bit 1
// stores, bit 2 write-gather pipe stores (mmio.c dolrecomp_wgp_write).
// ED_NO_FASTMEM=1 sends every access through mem_read / mem_write;
// ED_STORE_WATCH keeps RAM stores there so the watch sees them.
u8 dolrecomp_fast_ram;

static void publish_codegen_options(void) {
    dolrecomp_fast_ram = getenv("ED_NO_FASTMEM") != NULL ? 0u
                         : getenv("ED_STORE_WATCH") != NULL ? 5u : 7u;
    dolrecomp_chain_stop = NULL;
    u8* const chain_ok = &dolrecomp_chain_table[(HLE_CODE_BASE - 0x80000000u) / 4u];
    memset(chain_ok, 0, HLE_DISPATCH_COUNT);
    if (getenv("ED_NO_CHAIN") != NULL || g_hle_log)
        return;
    for (u32 i = 0; i < HLE_DISPATCH_COUNT; i++)
        g_chain_stop[i] = g_hle_dispatch[i].intercept != NULL ||
                          g_hle_dispatch[i].notify != NULL;
    chain_stop_at(g_call_prof_vi);
    chain_stop_at(g_call_prof_fn);
    chain_stop_at(sdk_symbol_address("GXCallDisplayList"));
    for (u32 i = 0; i < g_trace_fn_count; i++)
        chain_stop_at(g_trace_fn[i]);
    dolrecomp_chain_stop_base = HLE_CODE_BASE;
    dolrecomp_chain_stop_count = HLE_DISPATCH_COUNT;
    dolrecomp_chain_stop = g_chain_stop;
    for (u32 i = 0; i < HLE_DISPATCH_COUNT; i++)
        chain_ok[i] = g_chain_stop[i] == 0u;
}

// MSL's memcpy, memset and a forward byte copy sit at the start of text in this
// game and copy one byte per loop pass (lbzu/stbu: four guest instructions a
// byte, ~4 M passes a second in the cutscene that follows the "rats" quote, where
// the main thread then starves the Vorbis speech decoder). Done natively when the
// whole range is in MEM1; anywhere else, byte by byte through the bus as before.
// Found by their instructions, so either disc's DOL works.
static bool mem1_span(const CPUState* cpu, u32 address, u32 size) {
    return guest_ram_range_valid(cpu, address, size);
}

// memcpy(dst, src, n): copies backwards when src < dst, else forwards (memmove).
static void hle_msl_memcpy(CPUState* cpu) {
    const u32 dst = hle_arg_u32(cpu, 0);
    const u32 src = hle_arg_u32(cpu, 1);
    const u32 n = hle_arg_u32(cpu, 2);
    if (n != 0u) {
        if (mem1_span(cpu, dst, n) && mem1_span(cpu, src, n))
            memmove(cpu->ram + (dst - GC_RAM_BASE), cpu->ram + (src - GC_RAM_BASE), n);
        else if (src < dst)
            for (u32 i = n; i-- > 0u;)
                mem_write8(cpu, dst + i, mem_read8(cpu, src + i));
        else
            for (u32 i = 0; i < n; i++)
                mem_write8(cpu, dst + i, mem_read8(cpu, src + i));
    }
    hle_set_u32(cpu, dst);
}

// The forward-only copy: a byte loop, so an overlapping dst inside the source
// repeats the leading bytes; keep that.
static void hle_msl_copy_forward(CPUState* cpu) {
    const u32 dst = hle_arg_u32(cpu, 0);
    const u32 src = hle_arg_u32(cpu, 1);
    const u32 n = hle_arg_u32(cpu, 2);
    if (n != 0u) {
        if (mem1_span(cpu, dst, n) && mem1_span(cpu, src, n) &&
            (dst <= src || dst - src >= n))
            memmove(cpu->ram + (dst - GC_RAM_BASE), cpu->ram + (src - GC_RAM_BASE), n);
        else
            for (u32 i = 0; i < n; i++)
                mem_write8(cpu, dst + i, mem_read8(cpu, src + i));
    }
}

// memset(dst, c, n) (the wrapper around __fill_mem): returns dst.
static void hle_msl_memset(CPUState* cpu) {
    const u32 dst = hle_arg_u32(cpu, 0);
    const u8 c = (u8)hle_arg_u32(cpu, 1);
    const u32 n = hle_arg_u32(cpu, 2);
    if (n != 0u) {
        if (mem1_span(cpu, dst, n))
            memset(cpu->ram + (dst - GC_RAM_BASE), c, n);
        else
            for (u32 i = 0; i < n; i++)
                mem_write8(cpu, dst + i, c);
    }
    hle_set_u32(cpu, dst);
}

static bool code_matches(CPUState* cpu, u32 address, const u32* words, size_t count) {
    for (size_t i = 0; i < count; i++)
        if (mem_read32(cpu, address + (u32)i * 4u) != words[i])
            return false;
    return true;
}

static void install_msl_memory_hle(CPUState* cpu) {
    if (getenv("ED_NO_MEM_HLE") != NULL)
        return;
    static const u32 kCopyForward[] = {0x3884FFFFu, 0x38C3FFFFu, 0x38A50001u, 0x4800000Cu,
                                       0x8C040001u, 0x9C060001u, 0x34A5FFFFu, 0x4082FFF4u,
                                       0x4E800020u};
    static const u32 kMemcpyHead[] = {0x7C041840u, 0x41800028u};
    static const u32 kMemcpyTail[] = {0x7C842A14u, 0x7CC32A14u, 0x38A50001u, 0x4800000Cu,
                                      0x8C04FFFFu, 0x9C06FFFFu, 0x34A5FFFFu, 0x4082FFF4u,
                                      0x4E800020u};
    static const u32 kMemsetWrapper[] = {0x9421FFF0u, 0x7C0802A6u, 0x90010014u, 0x93E1000Cu,
                                         0x7C7F1B78u, 0x4800001Du};
    static const u32 kFillMemHead[] = {0x28050020u, 0x5484063Eu, 0x38C3FFFFu};
    u32 found_memcpy = 0, found_memset = 0, found_forward = 0;
    for (u32 a = HLE_CODE_BASE; a < HLE_CODE_BASE + 0x2000u; a += 4u) {
        if (!found_memcpy && code_matches(cpu, a, kMemcpyHead, 2) &&
            code_matches(cpu, a + 8u, kCopyForward, 9) &&
            code_matches(cpu, a + 0x2Cu, kMemcpyTail, 9))
            found_memcpy = a;
        else if (!found_memset && code_matches(cpu, a, kMemsetWrapper, 6) &&
                 code_matches(cpu, a + 0x30u, kFillMemHead, 3))
            found_memset = a;
        else if (!found_forward && mem_read32(cpu, a - 4u) == 0x4E800020u &&
                 code_matches(cpu, a, kCopyForward, 9) &&
                 (found_memcpy == 0u || a != found_memcpy + 8u))
            found_forward = a;
    }
    struct { u32 address; HleHandler fn; } found[] = {
        {found_memcpy, hle_msl_memcpy},
        {found_memset, hle_msl_memset},
        {found_forward, hle_msl_copy_forward},
    };
    for (size_t i = 0; i < sizeof found / sizeof found[0]; i++) {
        HleDispatchEntry* entry = hle_dispatch_entry(found[i].address);
        if (found[i].address != 0u && entry != NULL && entry->intercept == NULL)
            entry->intercept = found[i].fn;
    }
    fprintf(stderr, "[hle] native memcpy 0x%08X memset 0x%08X forward copy 0x%08X\n",
            found_memcpy, found_memset, found_forward);
}

// The prologue's book shows a photo of Alex that the game makes by copying a
// frame to an XFB and reading it back on the CPU (fn_801F50EC in the USA
// decomp: YUYV to a texture, r3 = the XFB). Our copies reach RAM a frame or
// two late, so the photo was an older frame: the book page on the Steam Deck,
// a flash of the unfinished scene, eyes out of line with the 3D Alex. Before
// it reads, wait for that copy and write it (twice in the sequence).
static void hle_xfb_reader_notify(CPUState* cpu) {
#ifdef ED_AURORA
    dol_aurora_xfb_ram_sync(cpu->gpr[3]);
    // It converts the XFB (r3) to a whole-frame RGB565 texture (r4), 640x528
    // on PAL, which it then draws with point sampling over the screen, fading
    // it out: the end of the prologue's logo shot dropped to that blocky
    // low-res picture. Draw the copy itself, at render resolution, instead.
    dol_aurora_xfb_texture(cpu->gpr[3], cpu->gpr[4]);
#else
    (void)cpu;
#endif
}

static void install_xfb_reader_sync(CPUState* cpu) {
    if (getenv("ED_NO_XFB_SYNC") != NULL)
        return;
    static const u32 kHead[] = {0x9421FFC0u, 0x7C0802A6u, 0x90010044u, 0xBF010020u,
                                0x7C982378u, 0x7C7B1B78u, 0x3B800000u};
    const u32 address = strcmp(ED_GAME_ID, "GEDP01") == 0 ? 0x801E9920u : 0x801F50ECu;
    HleDispatchEntry* entry = hle_dispatch_entry(address);
    if (entry == NULL || entry->notify != NULL || !code_matches(cpu, address, kHead, 7)) {
        fprintf(stderr, "[hle] XFB reader not found at 0x%08X: the book's photo may lag\n",
                address);
        return;
    }
    entry->notify = hle_xfb_reader_notify;
}

#ifdef ED_AURORA
// The game's own widescreen option: its camera projects at 16:9 (anamorphic,
// as for a widescreen TV) while this r13 word is 1 and at 4:3 while it is 0.
// The PAL disc's options menu flips it, in state 3 of its menu function; the
// USA disc has it only in a debug menu. The launcher's Aspect ratio drives it,
// so 16:9 is the game's wider view and never a stretched 4:3 picture: once a
// frame a value the game wrote anywhere else (boot, a save's options) goes
// back to the launcher's, and one the player chose in that menu becomes the
// launcher's. The card is never written here; the game saves what it holds.
static s32 g_widescreen_offset;        // the word's r13 offset, 0 = not found
static s32 g_options_state_offset;     // the PAL options menu's state word
// Calls of that menu in its toggle state, counted on the game thread. The sync
// runs on the main thread at each present, while the game goes on, so a call
// counts for the next three presents: the flip may land after the one that saw it.
static volatile u32 g_options_toggle_calls;
static int g_widescreen_seen = -1;     // the word as last seen or written

static bool sda_ready(const CPUState* cpu) {
    const u32 sda = cpu->gpr[13];
    return sda >= GC_RAM_BASE + 0x8000u && sda - GC_RAM_BASE <= cpu->ram_size - 0x8000u;
}

static void notify_options_menu(CPUState* cpu) {
    if (sda_ready(cpu) &&
        mem_read32(cpu, cpu->gpr[13] + (u32)g_options_state_offset) == 3u)
        g_options_toggle_calls = g_options_toggle_calls + 1u;
}

// The inventory book behind the game's menus comes in two shapes, a file per
// language for each (UBookIn?.tpl at 4:3, UWideIn?.tpl at 16:9, the same
// size). The game loads the one the word asks for into ARAM (load, given the
// language) and copies it into the menu's heap as the menu opens (copy); its
// options toggle makes both calls straight after the flip. A flip from the
// launcher only changes the word, so the book kept the other shape. Now when
// the menu opens, or draws a frame, over a book of the other shape, the game
// thread first makes the same calls (the copy only while the menu holds the
// book), then enters the menu function it was calling.
static struct {
    u32 load, copy;            // the game's two calls, 0 = not found
    u32 menu_open, menu_frame;
    s32 language_offset;       // r13 words: the language load takes,
    s32 size_offset;           // the size it answers (copy reads it),
    s32 menu_kind_offset;      // and 3 while the menu holds the book
    int loaded;                // the word as the book was last loaded, -1 unknown
    bool active;
    u32 stage;                 // 1 in load, 2 in copy
    bool copy_after;
    u32 resume;                // the menu function, entered at the end
    HleSavedContext saved;     // its arguments and return address
} g_book = {.loaded = -1};

static int widescreen_word(CPUState* cpu) {
    return mem_read32(cpu, cpu->gpr[13] + (u32)g_widescreen_offset) != 0u;
}

static void notify_book_load(CPUState* cpu) {
    if (sda_ready(cpu))
        g_book.loaded = widescreen_word(cpu);
}

static void book_check(CPUState* cpu, u32 function, bool menu_up) {
    g_hle_passthrough = true;
    if (g_book.active || !sda_ready(cpu) || widescreen_word(cpu) == g_book.loaded)
        return;
    const u32 sda = cpu->gpr[13];
    save_callback_context(&g_book.saved, cpu);
    g_book.active = true;
    g_book.stage = 1u;
    g_book.copy_after =
        menu_up && mem_read32(cpu, sda + (u32)g_book.menu_kind_offset) == 3u;
    g_book.resume = function;
    fprintf(stderr, "[widescreen] the menu's book reloaded at %s%s\n",
            widescreen_word(cpu) ? "16:9" : "4:3", g_book.copy_after ? " (menu open)" : "");
    g_hle_passthrough = false;
    g_hle_tail_call = true;
    cpu->gpr[3] = mem_read32(cpu, sda + (u32)g_book.language_offset);
    cpu->lr = HLE_BOOK_RETURN;
    cpu->pc = g_book.load;
}

static void hle_book_menu_open(CPUState* cpu) {
    book_check(cpu, g_book.menu_open, false);
}

static void hle_book_menu_frame(CPUState* cpu) {
    book_check(cpu, g_book.menu_frame, true);
}

static bool continue_book_reload(CPUState* cpu) {
    if (!g_book.active)
        return false;
    if (g_book.stage == 1u) {
        mem_write32(cpu, cpu->gpr[13] + (u32)g_book.size_offset, cpu->gpr[3]);
        g_book.loaded = widescreen_word(cpu);
        if (g_book.copy_after) {
            g_book.stage = 2u;
            cpu->lr = HLE_BOOK_RETURN;
            cpu->pc = g_book.copy;
            return true;
        }
    }
    const HleSavedContext* saved = &g_book.saved;
    memcpy(cpu->gpr, saved->gpr, sizeof saved->gpr);
    memcpy(cpu->fpr, saved->fpr, sizeof saved->fpr);
    memcpy(cpu->ps1, saved->ps1, sizeof saved->ps1);
    cpu->lr = saved->lr;
    cpu->ctr = saved->ctr;
    cpu->cr = saved->cr;
    cpu->xer = saved->xer;
    cpu->fpscr = saved->fpscr;
    cpu->pc = g_book.resume;
    g_book.active = false;
    return true;
}

static u32 bl_target(CPUState* cpu, u32 at) {
    const u32 word = mem_read32(cpu, at);
    if ((word & 0xFC000003u) != 0x48000001u)
        return 0u;
    s32 offset = (s32)(word & 0x03FFFFFCu);
    if (offset & 0x02000000)
        offset -= 0x04000000;
    return at + (u32)offset;
}

// PAL: after the toggle's stw come bl load, stw r3, size(r13) and bl copy. The
// menu opens at 0x80111078 (lwz r0, kind(r13); cmpwi r0, 3; bl to the setup
// that calls copy) and draws each frame from 0x80111234.
static void find_book(CPUState* cpu, u32 toggle) {
    const u32 load = bl_target(cpu, toggle + 16u), copy = bl_target(cpu, toggle + 24u);
    const u32 keep = mem_read32(cpu, toggle + 20u);
    const u32 open = 0x80111078u, frame = 0x80111234u;
    const u32 kind_load = mem_read32(cpu, open + 0x114u);
    const u32 setup = bl_target(cpu, open + 0x140u);
    HleDispatchEntry* load_entry = hle_dispatch_entry(load);
    HleDispatchEntry* open_entry = hle_dispatch_entry(open);
    HleDispatchEntry* frame_entry = hle_dispatch_entry(frame);
    if (load == 0u || copy == 0u || (keep >> 16) != 0x906Du ||
        mem_read32(cpu, load + 0x18u) != (0x800D0000u | ((u32)g_widescreen_offset & 0xFFFFu)) ||
        mem_read32(cpu, open) != 0x9421FFF0u || mem_read32(cpu, open + 4u) != 0x7C0802A6u ||
        mem_read32(cpu, frame) != 0x9421FFE0u || mem_read32(cpu, frame + 4u) != 0x7C0802A6u ||
        (kind_load >> 16) != 0x800Du || mem_read32(cpu, open + 0x128u) != 0x2C000003u ||
        setup == 0u || bl_target(cpu, setup + 0x44u) != copy || load_entry == NULL ||
        load_entry->notify != NULL || open_entry == NULL || open_entry->intercept != NULL ||
        frame_entry == NULL || frame_entry->intercept != NULL) {
        fprintf(stderr, "[widescreen] the menu's book loader not found: it keeps its shape\n");
        return;
    }
    g_book.load = load;
    g_book.copy = copy;
    g_book.menu_open = open;
    g_book.menu_frame = frame;
    g_book.language_offset = (s16)(mem_read32(cpu, toggle + 4u) & 0xFFFFu);
    g_book.size_offset = (s16)(keep & 0xFFFFu);
    g_book.menu_kind_offset = (s16)(kind_load & 0xFFFFu);
    g_book.loaded = -1;
    g_book.active = false;
    load_entry->notify = notify_book_load;
    open_entry->intercept = hle_book_menu_open;
    frame_entry->intercept = hle_book_menu_frame;
}

static void find_widescreen_option(CPUState* cpu) {
    const bool pal = strcmp(ED_GAME_ID, "GEDP01") == 0;
    // The camera, before C_MTXPerspective: lwz r0, flag(r13) ... cmpwi r0, 1.
    const u32 camera = pal ? 0x801E4C1Cu : 0x801F0430u;
    const u32 load = mem_read32(cpu, camera);
    bool compared = false;
    for (u32 k = 1; k <= 4u && !compared; k++)
        compared = mem_read32(cpu, camera + k * 4u) == 0x2C000001u;
    g_widescreen_offset = 0;
    g_options_state_offset = 0;
    g_widescreen_seen = -1;
    if ((load >> 16) != 0x800Du || !compared) {
        fprintf(stderr, "[widescreen] the game's option not found: 16:9 stretches its picture\n");
        return;
    }
    g_widescreen_offset = (s16)(load & 0xFFFFu);
    if (!pal)
        return;
    // PAL: the toggle (lwz r0, flag; lwz r3, ...; xori r0, r0, 1; stw r0, flag),
    // its menu function and the state word the function reads first.
    const u32 toggle = 0x80116CE8u, menu = 0x80116470u;
    const u32 state_load = mem_read32(cpu, menu + 0x20u);
    HleDispatchEntry* entry = hle_dispatch_entry(menu);
    if (mem_read32(cpu, toggle) != load || (mem_read32(cpu, toggle + 4u) >> 16) != 0x806Du ||
        mem_read32(cpu, toggle + 8u) != 0x68000001u ||
        mem_read32(cpu, toggle + 12u) != (0x900D0000u | (load & 0xFFFFu)) ||
        mem_read32(cpu, menu) != 0x9421FF80u || mem_read32(cpu, menu + 4u) != 0x7C0802A6u ||
        (state_load >> 16) != 0x800Du || entry == NULL || entry->notify != NULL) {
        fprintf(stderr, "[widescreen] the options menu's toggle not found: the launcher sets it\n");
        return;
    }
    g_options_state_offset = (s16)(state_load & 0xFFFFu);
    entry->notify = notify_options_menu;
    find_book(cpu, toggle);
}

// The launcher's choice as the game's word: 16:9 and 21:9 on, 4:3 off.
static int launcher_widescreen(void) {
    return g_settings.aspect != 1;
}

static void sync_widescreen_option(CPUState* cpu) {
    static u32 calls_seen;
    static int menu_presents;
    const u32 calls = g_options_toggle_calls;
    if (calls != calls_seen) {
        calls_seen = calls;
        menu_presents = 3;
    } else if (menu_presents > 0) {
        menu_presents--;
    }
    const bool chose = menu_presents > 0;
    static int wide = 2;  // turned back on in the game: the wide shape it had (16:9 or 21:9)
    if (g_settings.aspect != 1)
        wide = g_settings.aspect;
    if (g_widescreen_offset == 0 || !sda_ready(cpu))
        return;
    const u32 address = cpu->gpr[13] + (u32)g_widescreen_offset;
    const int value = mem_read32(cpu, address) != 0u;
    if (g_widescreen_seen >= 0 && value != g_widescreen_seen && chose) {
        g_settings.aspect = value ? wide : 1;
        settings_apply();
        settings_save();
        fprintf(stderr, "[widescreen] chosen in the game's options: %s\n",
                !value ? "4:3" : wide == 4 ? "21:9" : "16:9");
    }
    const int want = launcher_widescreen();
    if (value != want) {
        mem_write32(cpu, address, (u32)want);
        fprintf(stderr, "[widescreen] the game's option set to %s\n", want ? "16:9" : "4:3");
    }
    g_widescreen_seen = want;
}
#endif

#ifdef ED_AURORA
// The camera options (camera.c): the game's own camera hands C_MTXLookAt its
// struct's eye (r4) and target (r6); camera.c can swap both for its own.
void camera_look_at(CPUState* cpu);
static u32 g_camera_look_at_return;

static s16 g_show_player_offset, g_show_followed_offset;

// The player's mode bits (r13 + offset), bit 2 while they look closely at
// something (B, until they return), from their setter: lwz r0,x(r13);
// andc r4,r0,r3; beq +8; or r4,r0,r3; cmplwi r4,0; stw r4,x(r13).
static s16 g_player_modes_offset;

static void find_player_modes(CPUState* cpu) {
    for (u32 at = HLE_CODE_BASE; at + 24u < HLE_CODE_LIMIT; at += 4u) {
        const u32 load = mem_read32(cpu, at);
        if ((load >> 16) != 0x800Du || mem_read32(cpu, at + 4u) != 0x7C041878u ||
            mem_read32(cpu, at + 8u) != 0x41820008u || mem_read32(cpu, at + 12u) != 0x7C041B78u ||
            mem_read32(cpu, at + 16u) != 0x28040000u || mem_read32(cpu, at + 20u) != (0x908D0000u | (load & 0xFFFFu)))
            continue;
        g_player_modes_offset = (s16)(load & 0xFFFFu);
        return;
    }
    fprintf(stderr, "[camera] the player's mode bits not found: looking closely is told by the game's zoom\n");
}

// The room's lights for the renderer's shadows and haze (gxcore_draw.cpp set_room_lights), from the game's own grid.
// 0x804F0780 is a list of the room's models, 16 bytes each, and +4 of the first points at the grid: its count at +4,
// the far corner at +8, the size of a cell as u16s at +20, cells across at +27, +28, +29 (x fastest), and the cells
// at +32. Those follow it: 20 bytes each, a lamp count at +8 and the lamps at +12. A lamp is 20 bytes: place,
// colour at +12, power at +16. Seen the same way in the start of all 12 chapters. Sent when the room's grid changes.
static s16 g_coop_list_offset, g_coop_actor_offset, g_coop_stats_offset, g_coop_health_offset, g_coop_room_offset;  // defined with the co-op code below
#define LIGHT_GRID_POINTER 0x804F0784u
#define LIGHT_GRID_LAMPS 96

static void room_lights_update(CPUState* cpu) {
#ifdef ED_AURORA
    static u32 sent_grid, sent_mark;
    // AURORA_SHADOW_LOG: the game's frame now and then, among the renderer's lines (`ed chapters`). Its count isn't the game's.
    static int sync = -1;
    if (sync < 0)
        sync = getenv("AURORA_SHADOW_LOG") != NULL;
    if (sync && cpu->timebase / 675000ull % 20ull == 0ull)
        fprintf(stderr, "[sync] game frame %llu\n", (unsigned long long)(cpu->timebase / 675000ull));
    const u32 grid = mem_read32(cpu, LIGHT_GRID_POINTER);
    if (grid < 0x80000000u || grid >= 0x81800000u || mem_read32(cpu, grid + 32u) != grid + 36u)
        return;
    const u32 cells = grid + 36u;
    const u32 first = mem_read32(cpu, cells + 12u);
    if (first < 0x80000000u || first >= 0x81800000u)
        return;
    const u32 mark = mem_read32(cpu, first) ^ mem_read32(cpu, first + 4u) ^ mem_read32(cpu, grid + 8u);
    if (grid == sent_grid && mark == sent_mark)
        return;
    // Near a door the game has the next room loaded too, and this pointer goes back and forth between the two, for as
    // long as 280 frames seen in room 1: 12 lamps, its own, and 34, the next room's. Rooms load at the same place
    // (r13-25984), so what's preloaded lies after it: while the room's own grid is good, one further on isn't this
    // room's. Checked against the disc for rooms 1, 242, 243 and 246.
    static s32 sent_room = -1;
    const s32 room = g_coop_room_offset != 0 ? (s32)mem_read32(cpu, cpu->gpr[13] + (u32)(s32)g_coop_room_offset) : -1;
    if (sent_grid != 0u && room == sent_room && grid > sent_grid && mem_read32(cpu, sent_grid + 32u) == sent_grid + 36u) {
        const u32 was = mem_read32(cpu, sent_grid + 48u);
        if (was >= 0x80000000u && was < 0x81800000u &&
            (mem_read32(cpu, was) ^ mem_read32(cpu, was + 4u) ^ mem_read32(cpu, sent_grid + 8u)) == sent_mark)
            return;
    }
    sent_room = room;
    const u32 count = mem_read32(cpu, grid + 4u);
    const u32 sizes = mem_read32(cpu, grid + 20u), last = mem_read32(cpu, grid + 24u), across = mem_read32(cpu, grid + 28u);
    const int dims[3] = {(int)(last & 0xFFu), (int)(across >> 24), (int)((across >> 16) & 0xFFu)};
    if (count == 0u || count > 20000u || count != (u32)dims[0] * (u32)dims[1] * (u32)dims[2])
        return;
    sent_grid = grid;
    sent_mark = mark;
    const float origin[3] = {guest_read_f32(cpu, grid + 8u), guest_read_f32(cpu, grid + 12u), guest_read_f32(cpu, grid + 16u)};
    const float size[3] = {(float)(sizes >> 16), (float)(sizes & 0xFFFFu), (float)(last >> 16)};
    static float lamps[LIGHT_GRID_LAMPS * 7];
    static u32 seen[LIGHT_GRID_LAMPS];
    int lamp_count = 0;
    u8* per_cell = (u8*)malloc((size_t)count * 3u);
    if (per_cell == NULL)
        return;
    for (u32 c = 0; c < count; c++) {
        const u32 cell = cells + c * 20u;
        const u32 here = mem_read32(cpu, cell + 8u), list = mem_read32(cpu, cell + 12u);
        for (u32 i = 0; i < 3u; i++) {
            per_cell[c * 3u + i] = 255u;
            if (i >= here)
                continue;
            const u32 lamp = list + i * 20u;
            int found = -1;
            for (int n = 0; n < lamp_count && found < 0; n++)
                if (seen[n] == lamp || (lamps[n * 7] == guest_read_f32(cpu, lamp) && lamps[n * 7 + 1] == guest_read_f32(cpu, lamp + 4u) &&
                                        lamps[n * 7 + 2] == guest_read_f32(cpu, lamp + 8u)))
                    found = n;
            if (found < 0 && lamp_count < LIGHT_GRID_LAMPS) {
                found = lamp_count++;
                seen[found] = lamp;
                float* out = lamps + found * 7;
                for (u32 k = 0; k < 3u; k++)
                    out[k] = guest_read_f32(cpu, lamp + k * 4u);
                const u32 colour = mem_read32(cpu, lamp + 12u);
                out[3] = (float)(colour >> 24) / 255.0f;
                out[4] = (float)((colour >> 16) & 0xFFu) / 255.0f;
                out[5] = (float)((colour >> 8) & 0xFFu) / 255.0f;
                out[6] = (float)mem_read32(cpu, lamp + 16u);
            }
            if (found >= 0)
                per_cell[c * 3u + i] = (u8)found;
        }
    }
    fprintf(stderr, "[room-lights] grid 0x%08X: %d lamps, %d x %d x %d cells\n", grid, lamp_count, dims[0], dims[1], dims[2]);
    dol_aurora_set_room_lights(lamps, lamp_count, origin, size, dims, per_cell);
    free(per_cell);
#else
    (void)cpu;
#endif
}

// The Health, Sanity and Magick help that opens the Tome by itself (scripts 480, 694, 821) never comes: its "done" flags
// are on, as the game itself has them from later chapters (0x800076xx). The Tome's opening (0x80102A8C) and a first hit
// (0x80061FFC) each look at only these. r13-26528: the count, then the bits, flag >> 5 words, bit flag & 31.
static void meter_help_off(CPUState* cpu) {
    static const u32 kDone[] = {864u, 717u, 600u};
    const u32 set = mem_read32(cpu, cpu->gpr[13] - 26528u);
    if (set < 0x80000000u || set >= 0x81800000u)
        return;
    for (size_t i = 0; i < sizeof kDone / sizeof kDone[0]; i++) {
        const u32 word = set + 4u + (kDone[i] >> 5) * 4u;
        mem_write32(cpu, word, mem_read32(cpu, word) | (1u << (kDone[i] & 31u)));
    }
}

static void notify_look_at(CPUState* cpu) {
    if (cpu->lr != g_camera_look_at_return)
        return;
    if (sda_ready(cpu)) {
        const bool hidden = g_show_player_offset != 0 &&
                            mem_read32(cpu, cpu->gpr[13] + (u32)(s32)g_show_player_offset) == 0u;
        const int looking = g_player_modes_offset == 0
                                ? -1
                                : (mem_read32(cpu, cpu->gpr[13] + (u32)(s32)g_player_modes_offset) & 2u) != 0u;
        camera_set_examining(hidden, looking);
    }
    camera_look_at(cpu);
    camera_split_update(cpu);
    room_lights_update(cpu);
    meter_help_off(cpu);
}

// The game's collision, for the third-person camera's line test: the world a
// character collides with (fn(object)), the offset of the coordinates it
// collides in (the vector at object + 612), the segment builder (fn(out, start,
// end; f1 radius)) and the static world test (fn(object, world, segment,
// out, flags) -> the surface hit, or 0; out = the 24-byte hit).
static s16 g_camera_player_offset;
static u32 g_collide_world, g_collide_segment, g_collide_test;
static int g_collide_log = -1;

// The test is a query that must not hold the game: a call that runs past
// COLLIDE_MAX_BLOCKS (one ran on for seconds as the player picked up a gun;
// a test takes a few) is abandoned, and the camera goes without the game's
// collision for COLLIDE_REST_FRAMES.
#define COLLIDE_MAX_BLOCKS 200000u
#define COLLIDE_REST_FRAMES 300ull
#define COLLIDE_MAX_REACH 20000.0f
static unsigned long long g_collide_rest_until;

static bool collide_call(CPUState* cpu, u32 fn, const u32* args, int count, double f1, u32* result) {
    if (hle_guest_call_within(cpu, fn, args, count, f1, result, COLLIDE_MAX_BLOCKS))
        return true;
    g_collide_rest_until = cpu->timebase / 675000ull + COLLIDE_REST_FRAMES;
    fprintf(stderr, "[camera] the game's collision test 0x%08X ran on: none for %llu frames\n", fn,
            COLLIDE_REST_FRAMES);
    return false;
}

static bool camera_collide(CPUState* cpu, const float start[3], const float end[3], float radius, float hit[3]) {
    const u32 object = mem_read32(cpu, cpu->gpr[13] + (u32)(s32)g_camera_player_offset);
    if (object < 0x80000000u || object - 0x80000000u >= cpu->ram_size)
        return false;
    if (cpu->timebase / 675000ull < g_collide_rest_until)
        return false;
    for (int i = 0; i < 3; i++)
        if (!isfinite(start[i]) || !isfinite(end[i]) || fabsf(start[i]) > 1e6f || fabsf(end[i]) > 1e6f ||
            fabsf(end[i] - start[i]) > COLLIDE_MAX_REACH)
            return false;
    if (!(isfinite(radius) && radius >= 0.0f && radius < 1000.0f))
        return false;
    // Scratch below the stack, above the frames hle_guest_call's callee makes.
    const u32 scratch = (cpu->gpr[1] - 0x200u) & ~15u;
    const u32 from = scratch, to = scratch + 16u, segment = scratch + 32u, out = scratch + 112u;
    const u32 offset_at = object + 612u;
    u32 world = 0, surface = 0;
    if (!collide_call(cpu, g_collide_world, &object, 1, 0.0, &world) || world == 0u)
        return false;
    float offset[3];
    for (u32 i = 0; i < 3u; i++) {
        offset[i] = guest_read_f32(cpu, offset_at + i * 4u);
        guest_write_f32(cpu, from + i * 4u, start[i] + offset[i]);
        guest_write_f32(cpu, to + i * 4u, end[i] + offset[i]);
    }
    const u32 segment_args[] = {segment, from, to};
    const u32 test_args[] = {object, world, segment, out, 0u};
    for (u32 i = 0; i < 3u; i++)
        if (!isfinite(offset[i]) || fabsf(offset[i]) > 1e6f)
            return false;
    if (!collide_call(cpu, g_collide_segment, segment_args, 3, radius, NULL) ||
        !collide_call(cpu, g_collide_test, test_args, 5, 0.0, &surface))
        return false;
    if (g_collide_log < 0)
        g_collide_log = getenv("ED_CAM_LOG") != NULL;
    if (g_collide_log && surface != 0u) {
        static unsigned shown;
        if (shown++ < 8u)
            fprintf(stderr, "[camera] hit 0x%08X: %.1f %.1f %.1f / %.1f %.1f %.1f (from %.1f %.1f %.1f to %.1f %.1f %.1f, offset %.1f %.1f %.1f)\n",
                    surface, guest_read_f32(cpu, out), guest_read_f32(cpu, out + 4u), guest_read_f32(cpu, out + 8u),
                    guest_read_f32(cpu, out + 12u), guest_read_f32(cpu, out + 16u), guest_read_f32(cpu, out + 20u),
                    start[0], start[1], start[2], end[0], end[1], end[2], offset[0], offset[1], offset[2]);
    }
    if (surface == 0u)
        return false;
    for (u32 i = 0; i < 3u; i++)
        hit[i] = guest_read_f32(cpu, out + i * 4u) - offset[i];
    return true;
}

// The game fades out whatever is at or behind its camera (Alex, when its
// camera looks from her head at something she examines), measured from its
// camera's eye and target controllers. With another camera that is the wrong
// place: while the fade runs, the controllers hold the camera in use, and get
// their own values back as soon as it returns.
static u32 g_fade_fn, g_fade_eye, g_fade_target;
static bool g_fade_running;

static void hle_camera_fade(CPUState* cpu) {
    float eye[3], target[3];
    if (g_fade_running || !camera_view(eye, target)) {
        g_hle_passthrough = true;
        return;
    }
    u32 saved[6];
    for (u32 i = 0; i < 3u; i++) {
        saved[i] = mem_read32(cpu, g_fade_eye + i * 4u);
        saved[3 + i] = mem_read32(cpu, g_fade_target + i * 4u);
        guest_write_f32(cpu, g_fade_eye + i * 4u, eye[i]);
        guest_write_f32(cpu, g_fade_target + i * 4u, target[i]);
    }
    const u32 args[] = {cpu->gpr[3], cpu->gpr[4], cpu->gpr[5]};
    g_fade_running = true;
    const bool ok = hle_guest_call(cpu, g_fade_fn, args, 3, cpu->fpr[1], NULL);
    g_fade_running = false;
    for (u32 i = 0; i < 3u; i++) {
        mem_write32(cpu, g_fade_eye + i * 4u, saved[i]);
        mem_write32(cpu, g_fade_target + i * 4u, saved[3 + i]);
    }
    if (!ok)
        fprintf(stderr, "[camera] the fade did not return\n");
    if (g_collide_log > 0) {
        static unsigned n;
        const u32 player = mem_read32(cpu, cpu->gpr[13] + (u32)(s32)g_camera_player_offset);
        if (n++ % 30u == 0u)
            fprintf(stderr, "[camera] fade: player fade %.2f flags 0x%08X eye %.0f %.0f %.0f\n",
                    guest_read_f32(cpu, player + 684u), mem_read32(cpu, player + 596u), eye[0], eye[1], eye[2]);
    }
    // The nested dispatch passed itself through; this call is done here.
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

// The fade function (PAL, USA) and its controllers, from its own code:
// lis r5,hi; lis r4,hi; ...; addi r0,r5,target; ...; addi r4,r4,eye.
static void find_camera_fade(CPUState* cpu) {
    const bool pal = strcmp(ED_GAME_ID, "GEDP01") == 0;
    const u32 fn = pal ? 0x8011CA04u : 0x801237F0u;
    const u32 lis5 = mem_read32(cpu, fn + 0x4Cu), lis4 = mem_read32(cpu, fn + 0x50u);
    const u32 add_target = mem_read32(cpu, fn + 0x58u), add_eye = mem_read32(cpu, fn + 0x64u);
    HleDispatchEntry* entry = hle_dispatch_entry(fn);
    if ((mem_read32(cpu, fn) >> 16) != 0x9421u || (lis5 >> 16) != 0x3CA0u || (lis4 >> 16) != 0x3C80u ||
        (add_target >> 16) != 0x3805u || (add_eye >> 16) != 0x3884u || entry == NULL ||
        entry->intercept != NULL || entry->notify != NULL) {
        fprintf(stderr, "[camera] the game's fade not found: it measures from its own camera\n");
        return;
    }
    g_fade_fn = fn;
    g_fade_target = ((lis5 & 0xFFFFu) << 16) + (u32)(s32)(s16)(add_target & 0xFFFFu);
    g_fade_eye = ((lis4 & 0xFFFFu) << 16) + (u32)(s32)(s16)(add_eye & 0xFFFFu);
    entry->intercept = hle_camera_fade;
    camera_set_controllers(g_fade_eye, g_fade_target);
}

// The game draws the player only while two flags are set; looking closely at
// something clears them, as its camera is then at the player's head. The
// third-person camera is not, so while it is in use the characters' draw loop
// runs with both set (and first person, at the player's head, with both
// clear), and they get the game's values back as soon as it returns. The
// setter (found by its code) gives the flags: fn(show) stores show at
// offset(r13), and at a second offset when the player is also the one the
// camera follows; the draw loop is the function that tests the first.
static u32 g_draw_characters;
static bool g_draw_characters_running;

static void hle_draw_characters(CPUState* cpu) {
    const int drawn = camera_player_drawn();
    if (g_draw_characters_running || drawn < 0 || !sda_ready(cpu)) {
        g_hle_passthrough = true;
        return;
    }
    const u32 show = cpu->gpr[13] + (u32)(s32)g_show_player_offset;
    const u32 followed = cpu->gpr[13] + (u32)(s32)g_show_followed_offset;
    const u32 saved_show = mem_read32(cpu, show), saved_followed = mem_read32(cpu, followed);
    mem_write32(cpu, show, (u32)drawn);
    mem_write32(cpu, followed, (u32)drawn);
    u32 args[8];
    for (int i = 0; i < 8; i++)
        args[i] = cpu->gpr[3 + i];
    g_draw_characters_running = true;
    const bool ok = hle_guest_call(cpu, g_draw_characters, args, 8, cpu->fpr[1], NULL);
    g_draw_characters_running = false;
    mem_write32(cpu, show, saved_show);
    mem_write32(cpu, followed, saved_followed);
    if (!ok)
        fprintf(stderr, "[camera] the characters' draw did not return\n");
    g_hle_passthrough = false;  // the nested dispatch passed itself through
    g_hle_tail_call = false;
}

static void find_show_player(CPUState* cpu) {
    static const u32 kHead[] = {0x9421FFF0u, 0x7C0802A6u, 0x90010014u, 0x93E1000Cu, 0x93C10008u, 0x7C7E1B78u};
    u32 setter = 0;
    for (u32 at = HLE_CODE_BASE; at + 64u < HLE_CODE_LIMIT && setter == 0; at += 4u) {
        if (mem_read32(cpu, at) != kHead[0] || !code_matches(cpu, at, kHead, 6))
            continue;
        const u32 store = mem_read32(cpu, at + 40u), store2 = mem_read32(cpu, at + 48u);
        if ((mem_read32(cpu, at + 24u) & 0xFC000003u) != 0x48000001u || mem_read32(cpu, at + 28u) != 0x7C7F1B78u ||
            (mem_read32(cpu, at + 32u) & 0xFC000003u) != 0x48000001u || mem_read32(cpu, at + 36u) != 0x7C1F1800u ||
            (store >> 16) != 0x93CDu || mem_read32(cpu, at + 44u) != 0x40820008u || (store2 >> 16) != 0x93CDu)
            continue;
        setter = at;
        g_show_player_offset = (s16)(store & 0xFFFFu);
        g_show_followed_offset = (s16)(store2 & 0xFFFFu);
    }
    // The draw loop: lwz r0,show(r13); cmpwi r0,0; bne +16, a little after
    // its stwu r1,-n(r1).
    const u32 test = 0x800D0000u | ((u32)g_show_player_offset & 0xFFFFu);
    for (u32 at = HLE_CODE_BASE; setter != 0 && at + 12u < HLE_CODE_LIMIT; at += 4u) {
        if (mem_read32(cpu, at) != test || mem_read32(cpu, at + 4u) != 0x2C000000u ||
            mem_read32(cpu, at + 8u) != 0x40820010u)
            continue;
        for (u32 back = 4u; back <= 0x100u; back += 4u)
            if ((mem_read32(cpu, at - back) >> 16) == 0x9421u) {
                HleDispatchEntry* entry = hle_dispatch_entry(at - back);
                if (entry == NULL || entry->notify != NULL || entry->intercept != NULL)
                    break;
                g_draw_characters = at - back;
                entry->intercept = hle_draw_characters;
                return;
            }
    }
    fprintf(stderr, "[camera] the player's draw flags not found: looking closely hides the player\n");
}

// A character's bone, worked out relative to its parent: fn(character, bone,
// out matrix, flag); camera.c follows the player's to their head.
static void notify_bone_matrix(CPUState* cpu) {
    camera_bone_matrix(cpu, cpu->gpr[3], cpu->gpr[4], cpu->gpr[5]);
}

/* A character's update works out its bones (fn A(character, 0)), then skins
 * its mesh from them (fn B(..)): li r4,0; bl A; or r3,r29,r29; bl B, A
 * calling the bone function. Aiming in first person the player's skinning
 * runs with only their arms' bones (camera_skin_begin). */
static u32 g_bone_fn, g_skin_fn, g_skin_character;
static bool g_skin_running;

static void notify_character_bones(CPUState* cpu) {
    g_skin_character = cpu->gpr[3];
    camera_skin_begin(cpu, g_skin_character);
}

static void hle_skin(CPUState* cpu) {
    if (g_skin_running || !sda_ready(cpu)) {
        g_hle_passthrough = true;
        return;
    }
    u32 args[8];
    for (int i = 0; i < 8; i++)
        args[i] = cpu->gpr[3 + i];
    // ED_COOP_SKIN_LOG=1 (testing): the skinning's arguments for player 2, and
    // the words at the first, twice a second.
    static int skin_log = -1;
    if (skin_log < 0)
        skin_log = getenv("ED_COOP_SKIN_LOG") != NULL;
    u32 hle_coop_player2_object(CPUState* cpu);
    if (skin_log && g_skin_character != 0u && g_skin_character == hle_coop_player2_object(cpu) &&
        (cpu->timebase / 675000ull) % 30u == 0u) {
        fprintf(stderr, "[skin] p2 args");
        for (int i = 0; i < 8; i++)
            fprintf(stderr, " %08X", args[i]);
        fprintf(stderr, " | r3:");
        for (u32 w = 0; w < 16u && guest_ram_range_valid(cpu, args[0] + w * 4u, 4u); w++)
            fprintf(stderr, " %08X", mem_read32(cpu, args[0] + w * 4u));
        fputc('\n', stderr);
    }
    g_skin_running = true;
    const bool ok = hle_guest_call(cpu, g_skin_fn, args, 8, cpu->fpr[1], NULL);
    camera_skin_end(cpu, g_skin_character);
    g_skin_running = false;
    if (!ok)
        fprintf(stderr, "[camera] the characters' skinning did not return\n");
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

static void find_skinning(CPUState* cpu) {
    for (u32 at = HLE_CODE_BASE; g_bone_fn != 0u && at + 16u < HLE_CODE_LIMIT; at += 4u) {
        if (mem_read32(cpu, at) != 0x38800000u || mem_read32(cpu, at + 8u) != 0x7FA3EB78u)
            continue;
        const u32 a = bl_target(cpu, at + 4u), b = bl_target(cpu, at + 12u);
        if (a == 0u || b == 0u)
            continue;
        bool calls_bones = false;
        for (u32 c = a; c < a + 0x200u && !calls_bones; c += 4u)
            calls_bones = bl_target(cpu, c) == g_bone_fn;
        if (!calls_bones)
            continue;
        HleDispatchEntry* ea = hle_dispatch_entry(a);
        HleDispatchEntry* eb = hle_dispatch_entry(b);
        if (ea == NULL || eb == NULL || ea->notify != NULL || eb->notify != NULL || eb->intercept != NULL)
            break;
        ea->notify = notify_character_bones;
        eb->intercept = hle_skin;
        g_skin_fn = b;
        return;
    }
    fprintf(stderr, "[camera] the characters' skinning not found: first person aiming draws the whole player\n");
}

static void find_bone_matrix(CPUState* cpu) {
    // stwu r1,-32(r1); mflr r0; stw r0,36(r1); stmw r27,12(r1); or r29,r3,r3;
    // or r30,r4,r4; or r31,r5,r5; or r27,r6,r6; bl ...
    static const u32 kCode[] = {0x9421FFE0u, 0x7C0802A6u, 0x90010024u, 0xBF61000Cu,
                                0x7C7D1B78u, 0x7C9E2378u, 0x7CBF2B78u, 0x7CDB3378u};
    HleDispatchEntry* entry = NULL;
    for (u32 at = HLE_CODE_BASE; entry == NULL && at + 36u < HLE_CODE_LIMIT; at += 4u)
        if (mem_read32(cpu, at) == kCode[0] && code_matches(cpu, at, kCode, 8) &&
            (mem_read32(cpu, at + 32u) & 0xFC000003u) == 0x48000001u)
            entry = hle_dispatch_entry(at);
    if (entry == NULL || entry->notify != NULL || entry->intercept != NULL) {
        fprintf(stderr, "[camera] the characters' bones not found: first person at a set height\n");
        return;
    }
    entry->notify = notify_bone_matrix;
    for (u32 at = HLE_CODE_BASE; at + 36u < HLE_CODE_LIMIT; at += 4u)
        if (hle_dispatch_entry(at) == entry) {
            g_bone_fn = at;
            break;
        }
    find_skinning(cpu);
}

// The player object (position at +0), for the third-person camera: a pointer
// at r13 + offset that a small function compares its argument with:
// or r29,r3,r3; li r3,2; bl ...; lwz r0,offset(r13); or r31,r3,r3; cmplw r29,r0.
static void find_player(CPUState* cpu) {
    const bool pal = strcmp(ED_GAME_ID, "GEDP01") == 0;
    const u32 at = (pal ? 0x80123F70u : 0x8012AB2Cu) + 0x18u;
    static const u32 kHead[] = {0x7C7D1B78u, 0x38600002u};
    const u32 load = mem_read32(cpu, at + 12u);
    if (!code_matches(cpu, at, kHead, 2) || (mem_read32(cpu, at + 8u) & 0xFC000003u) != 0x48000001u ||
        (load >> 16) != 0x800Du || mem_read32(cpu, at + 16u) != 0x7C7F1B78u ||
        mem_read32(cpu, at + 20u) != 0x7C1D0040u) {
        fprintf(stderr, "[camera] the player not found: no third-person camera\n");
        return;
    }
    g_camera_player_offset = (s16)(load & 0xFFFFu);
    camera_set_player_offset(g_camera_player_offset);
    // The collision functions (PAL, USA), each starting stwu r1,-n(r1), and
    // the offset's getter (addi r3,r3,612; blr).
    g_collide_world = pal ? 0x80123F70u : 0x8012AB2Cu;
    g_collide_segment = pal ? 0x801388CCu : 0x8013F3C0u;
    g_collide_test = pal ? 0x80130684u : 0x80137178u;
    const u32 offset_getter = pal ? 0x801188D0u : 0x8011F770u;
    const u32 fns[] = {g_collide_world, g_collide_segment, g_collide_test};
    for (int i = 0; i < 4; i++)
        if (i == 3 ? mem_read32(cpu, offset_getter) != 0x38630264u : (mem_read32(cpu, fns[i]) >> 16) != 0x9421u) {
            fprintf(stderr, "[camera] the game's collision not found: the camera goes through walls\n");
            return;
        }
    camera_set_collide(camera_collide);
}

// The same function's projection: C_MTXPerspective(mtx, fovy f1, aspect f2,
// near f3, far f4) from one of two calls (lr = call + 4), whose field of view
// the camera can replace with its own.
static u32 g_camera_perspective_return[2];

static void notify_perspective(CPUState* cpu) {
    if (cpu->lr == g_camera_perspective_return[0] || cpu->lr == g_camera_perspective_return[1]) {
        const double fov = camera_field_of_view((float)cpu->fpr[1]);
        cpu->fpr[1] = fov;
        cpu->ps1[1] = fov;
        const double near = camera_near_plane((float)cpu->fpr[3], cpu->timebase / 675000ull);
        cpu->fpr[3] = near;
        cpu->ps1[3] = near;
    }
}

static void find_camera_perspective(CPUState* cpu, u32 from) {
    const u32 perspective = sdk_symbol_address("C_MTXPerspective");
    HleDispatchEntry* entry = hle_dispatch_entry(perspective);
    int found = 0;
    for (u32 at = from; at < from + 0x100u && found < 2; at += 4u) {
        const u32 bl = mem_read32(cpu, at);
        if ((bl & 0xFC000003u) == 0x48000001u && at + (u32)(((s32)(bl << 6)) >> 6 & ~3) == perspective)
            g_camera_perspective_return[found++] = at + 4u;
    }
    if (found == 0 || entry == NULL || entry->notify != NULL) {
        fprintf(stderr, "[camera] the game's projection not found: third person zooms with it\n");
        return;
    }
    entry->notify = notify_perspective;
}

// The call, a little after the widescreen check in the same function:
// or r4,r28,r28; addi r3,r31,396; addi r5,r1,24; addi r6,r28,12; bl C_MTXLookAt.
/* r3 at a call, from the lis/addi/li before it (from `from`); 0: unknown. */
static u32 r3_at_call(CPUState* cpu, u32 from, u32 call) {
    u32 value[32] = {0};
    bool known[32] = {false};
    for (u32 at = from; at < call; at += 4u) {
        const u32 w = mem_read32(cpu, at);
        const u32 op = w >> 26, rt = (w >> 21) & 31u, ra = (w >> 16) & 31u;
        const s32 simm = (s16)(w & 0xFFFFu);
        if (op == 15u && ra == 0u)
            value[rt] = (w & 0xFFFFu) << 16, known[rt] = true;  // lis
        else if (op == 14u && ra == 0u)
            value[rt] = (u32)simm, known[rt] = true;  // li
        else if (op == 14u && known[ra])
            value[rt] = value[ra] + (u32)simm, known[rt] = true;  // addi
        else if ((w & 0xFC000001u) == 0x48000001u)
            known[3] = false;  // a call's result
        else if (op == 14u || op == 15u)
            known[rt] = false;
    }
    return known[3] ? value[3] : 0u;
}

/* The game's draw-done callback (set once at boot, so found in the code: the
 * GXSetDrawDoneCallback call's argument) receives a message the main loop
 * sends just after GXSetDrawDone, blocking, inside the finish interrupt. The
 * deferred finish waits for that message (interrupt.c). */
static void find_finish_queue(CPUState* cpu) {
    const u32 set_callback = sdk_symbol_address("GXSetDrawDoneCallback");
    const u32 receive = sdk_symbol_address("OSReceiveMessage");
    if (set_callback == 0u || receive == 0u)
        return;
    for (u32 at = 0x80003100u; at < 0x80400000u; at += 4u) {
        const u32 w = mem_read32(cpu, at);
        if ((w & 0xFC000003u) != 0x48000001u || bl_target(cpu, at) != set_callback)
            continue;
        const u32 callback = r3_at_call(cpu, at - 32u, at);
        if (callback < 0x80003100u || callback >= 0x80400000u)
            continue;
        for (u32 c = callback; c < callback + 0x200u; c += 4u) {
            const u32 cw = mem_read32(cpu, c);
            if (cw == 0x4E800020u)
                break;  // blr
            if ((cw & 0xFC000003u) == 0x48000001u && bl_target(cpu, c) == receive) {
                const u32 queue = r3_at_call(cpu, callback, c);
                if (queue >= 0x80000000u && queue < 0x81800000u) {
                    interrupt_set_finish_queue(queue);
                    fprintf(stderr, "[gx] draw-done callback 0x%08X waits on queue 0x%08X\n", callback, queue);
                    return;
                }
            }
        }
    }
    fprintf(stderr, "[gx] draw-done callback's queue not found\n");
}

static void find_camera(CPUState* cpu) {
    const bool pal = strcmp(ED_GAME_ID, "GEDP01") == 0;
    const u32 from = pal ? 0x801E4C1Cu : 0x801F0430u;
    const u32 look_at = sdk_symbol_address("C_MTXLookAt");
    static const u32 kArgs[] = {0x7F84E378u, 0x387F018Cu, 0x38A10018u, 0x38DC000Cu};
    HleDispatchEntry* entry = hle_dispatch_entry(look_at);
    for (u32 at = from; at < from + 0x200u && entry != NULL; at += 4u) {
        const u32 bl = mem_read32(cpu, at + 16u);
        const u32 target = at + 16u + (u32)(((s32)(bl << 6)) >> 6 & ~3);
        if (code_matches(cpu, at, kArgs, 4) && (bl & 0xFC000003u) == 0x48000001u && target == look_at &&
            entry->notify == NULL) {
            g_camera_look_at_return = at + 20u;
            entry->notify = notify_look_at;
            find_player(cpu);
            find_camera_perspective(cpu, from - 0x40u);
            find_camera_fade(cpu);
            find_show_player(cpu);
            find_bone_matrix(cpu);
            find_player_modes(cpu);
            return;
        }
    }
    fprintf(stderr, "[camera] the game's camera not found: only its own fixed cameras\n");
}

// ---------------------------------------------------------------------------
// Split-screen co-op (Mods): player 2's character
// ---------------------------------------------------------------------------
// The game keeps its characters in a list (r13 + list: a head whose +4 is the
// first, each node's +4 the next). A node's +52 is its object (position at
// +0, facing at +44), its +8 its flags; its actor (node + actor) holds its type
// (+152), its model in the area's model list (+154) and its class (+158: 2 a
// creature); its stats (node + stats) its health (stats + health). The player
// is the node whose object is the player object (r13 + g_camera_player_offset).
//
// Player 2 is made as the area loader makes a character (0x80041868), as the
// debug menu's spawn does upstream: a spawn record (init: position, facing,
// type at +16, model at +20, flags at +40), create, then the room, register
// and add; with the player's own type, model and flags.
#define COOP_SPAWN_INIT 0x80040E94u
#define COOP_SPAWN_CREATE 0x8003121Cu
#define COOP_SPAWN_SET_ROOM 0x801F64C8u
#define COOP_SPAWN_REGISTER 0x801F5D18u
#define COOP_SPAWN_ADD 0x801DCB58u
#define COOP_SPAWN_DISTANCE 150.0f

static bool g_coop_found;
static s16 g_coop_list_offset, g_coop_actor_offset, g_coop_stats_offset, g_coop_health_offset, g_coop_room_offset;
static u32 g_coop_p2;            // player 2's node, 0 when there is none
static struct {
    u32 id, node;  // the original's id, the copy (coop_restore_held)
} g_coop_restore;
static unsigned g_coop_attempts;  // spawns tried in this area

// The list's nodes, in order: 0 past the last.
static u32 coop_next(CPUState* cpu, u32 node) {
    node = node == 0u ? mem_read32(cpu, cpu->gpr[13] + (u32)(s32)g_coop_list_offset) : node;
    const u32 next = guest_ram_range_valid(cpu, node, 8u) ? mem_read32(cpu, node + 4u) : 0u;
    return guest_ram_range_valid(cpu, next, 56u) ? next : 0u;
}

static u32 coop_player(CPUState* cpu) {
    const u32 player = mem_read32(cpu, cpu->gpr[13] + (u32)(s32)g_camera_player_offset);
    if (player == 0u)
        return 0u;
    u32 node = 0u;
    for (int i = 0; i < 1024 && (node = coop_next(cpu, node)) != 0u; i++)
        if (mem_read32(cpu, node + 52u) == player)
            return node;
    return 0u;
}

static bool coop_listed(CPUState* cpu, u32 wanted) {
    u32 node = 0u;
    for (int i = 0; i < 1024 && (node = coop_next(cpu, node)) != 0u; i++)
        if (node == wanted)
            return true;
    return false;
}

static void coop_describe(CPUState* cpu, const char* what, u32 node) {
    const u32 actor = mem_read32(cpu, node + (u32)(s32)g_coop_actor_offset);
    const u32 stats = mem_read32(cpu, node + (u32)(s32)g_coop_stats_offset);
    const u32 object = mem_read32(cpu, node + 52u);
    float at[3] = {0.0f, 0.0f, 0.0f};
    if (guest_ram_range_valid(cpu, object, 16u))
        for (u32 i = 0; i < 3u; i++)
            at[i] = guest_read_f32(cpu, object + i * 4u);
    fprintf(stderr, "[coop] %s 0x%08X: object 0x%08X type %u model %d class %u health %d flags 0x%08X at %.0f %.0f %.0f\n",
            what, node, object, guest_ram_range_valid(cpu, actor, 160u) ? mem_read16(cpu, actor + 152u) : 0u,
            guest_ram_range_valid(cpu, actor, 160u) ? (s16)mem_read16(cpu, actor + 154u) : -1,
            guest_ram_range_valid(cpu, actor, 160u) ? mem_read8(cpu, actor + 158u) : 0u,
            guest_ram_range_valid(cpu, stats + (u32)(s32)g_coop_health_offset, 2u)
                ? (s16)mem_read16(cpu, stats + (u32)(s32)g_coop_health_offset)
                : -1,
            mem_read32(cpu, node + 8u), at[0], at[1], at[2]);
    // ED_COOP_ENEMY_LOG=1: their health, sanity and magick as the game reads
    // them (0x80034E20(node, kind, out)).
    static int sanity_log = -1;
    if (sanity_log < 0)
        sanity_log = getenv("ED_COOP_ENEMY_LOG") != NULL;
    if (sanity_log && sda_ready(cpu)) {
        s16 v[3];
        for (u32 k = 0; k < 3u; k++) {
            const u32 out = (cpu->gpr[1] - 0x300u) & ~15u;
            const u32 args[3] = {node, k, out};
            u32 ok = 0u;
            mem_write16(cpu, out, 0u);
            hle_guest_call(cpu, 0x80034E20u, args, 3, 0.0, &ok);
            v[k] = (s16)mem_read16(cpu, out);
        }
        fprintf(stderr, "[coop] %s: health %d sanity %d magick %d\n", what, v[0], v[1], v[2]);
    }
    // ED_COOP_STATS (testing): the stats round health, for magick and sanity.
    // The playing character's record (0x8030A940): their most health (+68),
    // sanity (+70) and magick (+72), copied into the player's stats at +224..
    // (0x80032098; 0x80034E20(character, kind) reads them: 0 health, 1
    // sanity, 2 magick).
    if (getenv("ED_COOP_STATS") != NULL && strcmp(what, "player 1") == 0)
        fprintf(stderr, "[coop] player 1 most: health %d sanity %d magick %d (+74 %d)\n",
                (s16)mem_read16(cpu, 0x8030A984u), (s16)mem_read16(cpu, 0x8030A986u),
                (s16)mem_read16(cpu, 0x8030A988u), (s16)mem_read16(cpu, 0x8030A98Au));
    // ED_RAM_DUMP=file (testing): all of memory, once, as player 1 is first seen.
    static bool dumped;
    // ED_RAM_DUMP_P2: once player 2 is seen instead (all of it, the scratch too).
    const char* dump_at = getenv("ED_RAM_DUMP_P2") != NULL ? "player 2" : "player 1";
    if (getenv("ED_RAM_DUMP") != NULL && !dumped && strcmp(what, dump_at) == 0) {
        dumped = true;
        FILE* f = fopen(getenv("ED_RAM_DUMP"), "wb");
        if (f != NULL) {
            fwrite(cpu->ram, 1, cpu->ram_size, f);
            fclose(f);
        }
    }
    if (getenv("ED_COOP_STATS") != NULL) {
        const u32 blocks[2] = {stats, actor};
        const char* names[2] = {"stats", "actor"};
        for (int b = 0; b < 2; b++) {
            if (!guest_ram_range_valid(cpu, blocks[b], 512u))
                continue;
            fprintf(stderr, "[coop] %s %s:", what, names[b]);
            for (u32 i = 0; i < 512u; i += 2u) {
                const s16 v = (s16)mem_read16(cpu, blocks[b] + i);
                if (v != 0)
                    fprintf(stderr, " +%u=%d", i, v);
            }
            fputc('\n', stderr);
        }
    }
}

// Player 2 beside player 1: the first of eight directions round them (ahead
// first) clear in the game's collision with floor under it at player 1's
// height, at that floor: then within a step of it, then at half the
// distance; else where player 1 stands. On the stairs a clear way over the
// drop let Paul fall to the floor below; in the Tome's room a rock a little
// above the floor was taken, and he was pushed off it into the room's gap.
#define COOP_PLACE_LEVEL 12.0f  // floor this near player 1's is level with them
#define COOP_PLACE_STEP 50.0f   // the most the floor there may be above or below player 1's
// The game's line test is in the characters' own frame (floors come back
// about 50 below where a character stands on them), so the floor beside
// player 1 is measured against the floor under them, the same way.
static bool coop_floor(CPUState* cpu, float x, float y, float z, float* floor) {
    const float from[3] = {x, y, z + 60.0f}, to[3] = {x, y, z - COOP_PLACE_STEP - 120.0f};
    float hit[3];
    if (!camera_collide(cpu, from, to, 5.0f, hit))
        return false;
    *floor = hit[2];
    return true;
}

static bool coop_place_try(CPUState* cpu, const float at[3], float under, float facing, float distance, float rise,
                           float out[3]) {
    static const float kTurns[8] = {1.571f, -1.571f, 0.785f, -0.785f, 0.0f, 2.356f, -2.356f, 3.142f};
    for (int i = 0; i < 8; i++) {
        const float a = facing + kTurns[i];
        const float from[3] = {at[0], at[1], at[2] + 60.0f};
        const float to[3] = {at[0] + cosf(a) * distance, at[1] + sinf(a) * distance, at[2] + 60.0f};
        float hit[3], floor = under;
        if (camera_collide(cpu, from, to, 30.0f, hit) || !coop_floor(cpu, to[0], to[1], at[2], &floor) ||
            fabsf(floor - under) > rise)
            continue;
        out[0] = to[0];
        out[1] = to[1];
        out[2] = at[2] + (floor - under);
        return true;
    }
    return false;
}

static void coop_place(CPUState* cpu, u32 object, float out[3]) {
    float at[3], under = 0.0f;
    for (u32 i = 0; i < 3u; i++)
        out[i] = at[i] = guest_read_f32(cpu, object + i * 4u);
    const float qz = guest_read_f32(cpu, object + 52u), qw = guest_read_f32(cpu, object + 56u);
    const float facing = isfinite(qz) && isfinite(qw) ? 2.0f * atan2f(qz, qw) - 1.5707963f : 0.0f;
    if (!coop_floor(cpu, at[0], at[1], at[2], &under))
        return;  // no floor known under player 1: where they stand
    if (!coop_place_try(cpu, at, under, facing, COOP_SPAWN_DISTANCE, COOP_PLACE_LEVEL, out) &&
        !coop_place_try(cpu, at, under, facing, COOP_SPAWN_DISTANCE, COOP_PLACE_STEP, out))
        coop_place_try(cpu, at, under, facing, COOP_SPAWN_DISTANCE * 0.5f, COOP_PLACE_STEP, out);
}

// A save made in co-op keeps player 2 as one of the area's characters: a
// second of the player's own type, with the player's model or player 2's
// character's (hle_spawn_create makes it so). That one is player 2 again,
// rather than a new one beside it: a second player 2, left where the save
// had them, was a stranger, and both players stood ready to fight.
static int coop_wanted_character(void);

// What player 2 held in a co-op save: a thing lying not held (its holder,
// object +372, itself) just where they were saved, with the model of another
// thing, the original it copies (coop_restore_held).
static void coop_find_saved_copy(CPUState* cpu, u32 player, u32 p2) {
    const u32 o2 = mem_read32(cpu, p2 + 52u);
    if (!guest_ram_range_valid(cpu, o2, 12u))
        return;
    u32 node = 0u;
    for (int i = 0; i < 1024 && (node = coop_next(cpu, node)) != 0u; i++) {
        const u32 object = mem_read32(cpu, node + 52u);
        if (node == player || node == p2 || !guest_ram_range_valid(cpu, object, 376u) ||
            mem_read32(cpu, object + 372u) != object)
            continue;
        float d = 0.0f;
        for (u32 k = 0; k < 3u; k++) {
            const float a = guest_read_f32(cpu, object + k * 4u) - guest_read_f32(cpu, o2 + k * 4u);
            d += a * a;
        }
        if (!(d < 1.0f))
            continue;
        const u32 model = mem_read32(cpu, object + 60u);
        u32 other = 0u;
        for (int j = 0; j < 1024 && (other = coop_next(cpu, other)) != 0u; j++) {
            const u32 o = mem_read32(cpu, other + 52u);
            if (other != node && guest_ram_range_valid(cpu, o, 64u) && mem_read32(cpu, o + 60u) == model) {
                g_coop_restore.id = mem_read32(cpu, other + 16u);
                g_coop_restore.node = node;
                fprintf(stderr, "[coop] player 2's copy of %u in the save: node 0x%08X (id %u)\n",
                        g_coop_restore.id, node, mem_read32(cpu, node + 16u));
                return;
            }
        }
    }
}

static void coop_adopt(CPUState* cpu, u32 player) {
    if (getenv("ED_COOP_P2_KIND") != NULL)
        return;  // testing another character: a new player 2
    const u32 actor = mem_read32(cpu, player + (u32)(s32)g_coop_actor_offset);
    if (!guest_ram_range_valid(cpu, actor, 160u))
        return;
    const u32 kind = mem_read32(cpu, actor + 152u);  // type and model
    const int p2 = coop_wanted_character();
    const u32 p2_kind = p2 >= 0 ? (kind & 0xFFFF0000u) | ((u32)p2 & 0xFFFFu) : kind;
    u32 node = 0u;
    for (int i = 0; i < 1024 && (node = coop_next(cpu, node)) != 0u; i++) {
        const u32 other = mem_read32(cpu, node + (u32)(s32)g_coop_actor_offset);
        if (node != player && guest_ram_range_valid(cpu, other, 160u) &&
            (mem_read32(cpu, other + 152u) == kind || mem_read32(cpu, other + 152u) == p2_kind)) {
            g_coop_p2 = node;
            coop_describe(cpu, "player 1", player);
            coop_describe(cpu, "player 2 (from the save)", node);
            coop_find_saved_copy(cpu, player, node);
            // Beside player 1, in their room (ED_COOP_P2_STAYS=1, testing:
            // where the save had them): the game runs player 1's room, and in
            // another one what player 2 held was not in their hand.
            const u32 o1 = mem_read32(cpu, player + 52u), o2 = mem_read32(cpu, node + 52u);
            if (getenv("ED_COOP_P2_STAYS") == NULL && guest_ram_range_valid(cpu, o1, 64u) &&
                guest_ram_range_valid(cpu, o2, 64u)) {
                float at[3];
                coop_place(cpu, o1, at);
                for (u32 i = 0; i < 3u; i++)
                    guest_write_f32(cpu, o2 + i * 4u, at[i]);
                const u32 room_args[] = {node, mem_read32(cpu, cpu->gpr[13] + (u32)(s32)g_coop_room_offset)};
                u32 ignore = 0u;
                hle_guest_call(cpu, COOP_SPAWN_SET_ROOM, room_args, 2, 0.0, &ignore);
            }
            return;
        }
    }
}

// A character's file (chars/char%04d.cpk), loaded by 0x801318C0(model, 0) as
// the area loader loads one: a table of 760-byte records from r13 - 23680, the
// file's data at +72 once loaded. The game thread runs the load (it waits on
// the disc): the pad read hands over to it and runs again after
// (HLE_CHARACTER_RETURN).
#define COOP_CHARACTER_LOAD 0x801318C0u
// A playable character's (the game's list, 0x80044378) has one slot of its
// own: 0x80132ED8(model) opens /Chars/char%04d.cpk, reads its first 1 KB and
// keeps the file to stream the rest from; the slot at 0x80582760 (+0 the
// file, +4 the model in it).
#define COOP_PLAYABLE_LOAD 0x80132ED8u
#define COOP_PLAYABLE_SLOT 0x80582760u

static bool coop_playable(int model) {
    static const int kPlayable[] = {0, 9, 18, 69, 70, 73, 74, 75, 76, 81, 85, 119, 120, 121, 122, 150};
    for (size_t i = 0; i < sizeof kPlayable / sizeof kPlayable[0]; i++)
        if (kPlayable[i] == model)
            return true;
    return false;
}
static struct {
    bool active;
    int tried;  // the model last asked for
    HleSavedContext saved;
} g_character_load = {.tried = -1};

// The character player 2 is made as, -1 player 1's: Mods' Player 2
// character (coop_p2_choice), or ED_COOP_P2_KIND=model (testing).
static int coop_p2_choice(void);
static int coop_wanted_character(void) {
    const char* kind = getenv("ED_COOP_P2_KIND");
    return kind != NULL ? atoi(kind) : coop_p2_choice();
}

// A body in the chapter's list of bodies (r13 - 26452), by character.
static bool chapter_has_body(CPUState* cpu, int model) {
    const u32 list = mem_read32(cpu, cpu->gpr[13] - 26452u);
    return model >= 0 && guest_ram_range_valid(cpu, list, 8u) && mem_read32(cpu, list + 4u) == 0x6B5u &&
           mem_read32(cpu, list) > (u32)model && mem_read32(cpu, list + 8u + (u32)model * 8u) != 0u;
}

static bool coop_character_loaded(CPUState* cpu, int model) {
    if (coop_playable(model))
        return (s16)mem_read16(cpu, COOP_PLAYABLE_SLOT + 4u) == model || chapter_has_body(cpu, model);
    const u32 record = mem_read32(cpu, cpu->gpr[13] - 23680u) + (u32)model * 760u;
    return guest_ram_range_valid(cpu, record, 760u) && mem_read32(cpu, record + 72u) != 0u;
}

static bool coop_character_load(CPUState* cpu) {
    int model = coop_wanted_character();
    // ED_EXTRA_CHAR=n (testing): an area character's file loaded once in play.
    static int extra = -2;
    if (extra == -2)
        extra = getenv("ED_EXTRA_CHAR") != NULL ? atoi(getenv("ED_EXTRA_CHAR")) : -1;
    if (extra >= 0 && g_coop_found && sda_ready(cpu) && coop_player(cpu) != 0u && !game_menu_open(cpu) &&
        !cutscene_playing(cpu) && !g_character_load.active && !coop_character_loaded(cpu, extra)) {
        model = extra;
        extra = -1;
    } else if (g_character_load.active || model < 0 || model > 159 || model == g_character_load.tried ||
        !g_coop_found || !g_settings.mod_coop || !sda_ready(cpu) || game_menu_open(cpu) ||
        cutscene_playing(cpu) || coop_character_loaded(cpu, model) || g_coop_p2 != 0u || coop_player(cpu) == 0u ||
        coop_playable(model))
        return false;
    g_character_load.tried = model;
    g_character_load.active = true;
    save_callback_context(&g_character_load.saved, cpu);
    fprintf(stderr, "[coop] loading character file %d for player 2\n", model);
    g_hle_tail_call = true;
    cpu->gpr[3] = (u32)model;
    cpu->gpr[4] = 0u;
    cpu->lr = HLE_CHARACTER_RETURN;
    cpu->pc = coop_playable(model) ? COOP_PLAYABLE_LOAD : COOP_CHARACTER_LOAD;
    return true;
}

static bool continue_character_load(CPUState* cpu) {
    if (!g_character_load.active)
        return false;
    const HleSavedContext* saved = &g_character_load.saved;
    memcpy(cpu->gpr, saved->gpr, sizeof saved->gpr);
    memcpy(cpu->fpr, saved->fpr, sizeof saved->fpr);
    memcpy(cpu->ps1, saved->ps1, sizeof saved->ps1);
    cpu->lr = saved->lr;
    cpu->ctr = saved->ctr;
    cpu->cr = saved->cr;
    cpu->xer = saved->xer;
    cpu->fpscr = saved->fpscr;
    cpu->pc = saved->pc;  // the pad read, again
    g_character_load.active = false;
    fprintf(stderr, "[coop] character file %d %s\n", g_character_load.tried,
            coop_character_loaded(cpu, g_character_load.tried) ? "loaded" : "did not load");
    return true;
}

// Player 2 out of the way while co-op is off: a save made in co-op keeps them
// as one of the area's characters, who would stand there squaring up to
// player 1. They are kept as player 2 (so not player 1's threat,
// coop_send_event) but put far off; back beside player 1 when co-op is on.
#define COOP_PARK_DISTANCE 50000.0f
static bool g_coop_parked;

static void coop_park(CPUState* cpu, u32 player, bool park) {
    if (g_coop_p2 == 0u || park == g_coop_parked)
        return;
    const u32 o1 = mem_read32(cpu, player + 52u), o2 = mem_read32(cpu, g_coop_p2 + 52u);
    if (!guest_ram_range_valid(cpu, o1, 64u) || !guest_ram_range_valid(cpu, o2, 64u))
        return;
    float at[3];
    coop_place(cpu, o1, at);
    if (park)
        at[0] += COOP_PARK_DISTANCE;
    for (u32 i = 0; i < 3u; i++)
        guest_write_f32(cpu, o2 + i * 4u, at[i]);
    g_coop_parked = park;
    fprintf(stderr, "[coop] player 2 %s\n", park ? "put out of the way (co-op is off)" : "back beside player 1");
}

// Player 1's character (Mods, Player 1 character; ED_PLAYER_MODEL=n for
// testing): the chapter's playable character is theirs. The chapter loads its
// one (0x80132ED8(model), as it starts) and makes the player from a spawn
// record (COOP_SPAWN_CREATE: type at +16, model at +20); both are theirs.
// The twelve, in Jump to Game's order: their character file (the model) and
// their kind (the type). Jump to Game's Peter starts chapter 10 and Edward
// chapter 9 (made as 69, kind 53).
static const s16 kPlayerModel[12] = {0, 70, 85, 150, 18, 73, 74, 76, 75, 69, 81, 9};
static const s16 kPlayerType[12] = {1, 49, 46, 27, 44, 55, 47, 54, 45, 53, 48, 26};
static int g_chapter_model = -1;  // the playable the chapter asked for

// Mods' Player 2 character (co-op), -1 for player 1's.
static int coop_p2_choice(void) {
    const int choice = g_settings.mod_p2_character;
    return g_settings.mod_coop && choice >= 1 && choice <= 12 ? kPlayerModel[choice - 1] : -1;
}

static int player_model(void) {
    static int test = -2;
    if (test == -2) {
        const char* m = getenv("ED_PLAYER_MODEL");
        test = m != NULL ? atoi(m) : -1;
    }
    if (test >= 0)
        return test;
    const int choice = g_settings.mod_p1_character;
    return choice >= 1 && choice <= 12 ? kPlayerModel[choice - 1] : -1;
}

// A character's other bodies, their forms. Only the body: their file's moves, name and stats stay,
// because they are still made as themselves, with this body filed under their number
// (costume_file). A body comes from a list of bodies by number: a level's U_LevelNN.bin
// (the ghosts in Alex's last chapter, and the Liche), or a cutscene's cinNNNN.bin, which is the same
// kind of pack (0x80007FA0 makes it the list at r13-26452 while the scene plays).
// mod_p1_costume and mod_p2_costume hold 2 bits for each of the twelve, by their place in
// kPlayerModel: the form, 0 their own. ED_COSTUME=p1,p2 (testing): that form for them.
// ED_COSTUME_TRY=character,form,level,cutscene or -1,body (testing): another body as that form.
// A body's +4 and the head of each part of a character's file hold a skeleton word: a body plays
// a file with its own word as it is, bones in the same places. Most ghosts have their character's,
// so they play that character's moves. Where a body hasn't (Edward's ghost, Ellia's rotten body, the
// Liche), their moves are fitted to it by bone (costume_tracks). Each plays its wearer's moves
// and holds what they equip, as a ghost's weapon built into its mesh is left out (costume_strip).
typedef struct {
    int wearer;       // their character number
    int place;        // their place in kPlayerModel
    int form;         // 1 and on: the frontend's kForms has the same, in the same order
    const char* name;
    int level;        // where the body is
    int cutscene;     // that level's cutscene pack, or -1 for its U_Level
    int body;         // its number there
    int filed;        // its number in the chapter's list: its own, or one no character has a file for
    int strip_group;  // the mesh group with a weapon built into it, to leave out, or -1
    int held_file;    // a character's file whose standing pose the bones the wearer hasn't hold, or -1
} Costume;
static Costume kCostumes[] = {
    {0, 0, 1, "Pius", 13, -1, 86, 86, -1, 86},
    {70, 1, 1, "Ellia", 13, -1, 142, 142, 5, -1},
    {70, 1, 2, "Ellia", 0, 25, 37, 114, -1, -1},       // rotten, in the last scene of Lindsey's chapter (script L06/906)
    {85, 2, 1, "Anthony", 13, -1, 143, 143, 6, -1},
    {150, 3, 1, "Karim", 13, -1, 144, 144, 14, -1},
    {18, 4, 1, "Max", 0, -1, 140, 140, -1, -1},        // his blue spirit: script 1438 makes it, at MaxGhostMkr
    {74, 6, 1, "Paul", 13, -1, 146, 146, 5, -1},
    {76, 7, 1, "Roberto", 13, -1, 147, 147, 14, -1},
    {69, 9, 1, "Edward", 17, 77, 39, 113, -1, -1},     // a scene's body: 62 bones to his 72
};
#define COSTUMES ((int)(sizeof kCostumes / sizeof kCostumes[0]))
#define COSTUME_PLACES 12
static u32 g_costume_plain[COSTUME_PLACES];  // each wearer's own body, as it was in the chapter's list (costume_file)

static Costume* costume_form(int model, int form) {
    static bool tried;
    if (!tried) {
        tried = true;
        const char* t = getenv("ED_COSTUME_TRY");
        int wearer, as, level, cutscene, body;
        if (t != NULL && sscanf(t, "%d,%d,%d,%d,%d", &wearer, &as, &level, &cutscene, &body) == 5)
            for (int i = 0; i < COSTUMES; i++)
                if (kCostumes[i].wearer == wearer && kCostumes[i].form == as) {
                    kCostumes[i].level = level;
                    kCostumes[i].cutscene = cutscene;
                    kCostumes[i].body = body;
                    kCostumes[i].strip_group = -1;
                }
    }
    for (int i = 0; i < COSTUMES; i++)
        if (kCostumes[i].wearer == model && kCostumes[i].form == form)
            return &kCostumes[i];
    return NULL;
}

// The other body a player wears, or NULL: their chosen character's chosen form.
static Costume* costume_on(int player) {
    static int test[2] = {-2, -2};
    if (test[0] == -2) {
        const char* t = getenv("ED_COSTUME");
        if (t == NULL || sscanf(t, "%d,%d", &test[0], &test[1]) != 2)
            test[0] = test[1] = -1;
    }
    if (player == 1 && !g_settings.mod_coop)
        return NULL;
    const int model = player == 0 ? player_model() : coop_wanted_character();
    for (int place = 0; place < COSTUME_PLACES; place++)
        if (kPlayerModel[place] == model) {
            const int bits = player == 0 ? g_settings.mod_p1_costume : g_settings.mod_p2_costume;
            return costume_form(model, test[player] >= 0 ? test[player] : (bits >> (place * 2)) & 3);
        }
    return NULL;
}

static int player_type(int model) {
    for (int i = 0; i < 12; i++)
        if (kPlayerModel[i] == model)
            return kPlayerType[i];
    return -1;
}

// The chapter's playable character: 0x80007C8C(chapter) answers from a table
// (Alex 9, Pius 0, ...); the chapter's load takes its file and body from it.
#define CHAPTER_CHARACTER 0x80007C8Cu
// The chapter's body: 0x801326E0(model, part) loads that part of the
// character's file, the part by the chapter's era (0x80007DC4(chapter) is the
// chapter itself, 0 to 11; the part for an era is the word at
// CHAPTER_PARTS + 4 * era). Another chapter's character is given the part of
// their own chapter.
#define CHAPTER_BODY_LOAD 0x801326E0u
#define CHAPTER_PARTS 0x80308DB8u
static const u8 kChapterModel[17] = {9, 0, 70, 85, 150, 18, 73, 74, 76, 69, 75, 81, 9, 9, 9, 75, 9};

static void hle_chapter_body(CPUState* cpu) {
    const int want = player_model();
    if (want >= 0 && (s16)(cpu->gpr[3] & 0xFFFFu) == want) {
        for (u32 chapter = 0; chapter < 12u; chapter++)
            if (kChapterModel[chapter] == want) {
                const u32 part = mem_read32(cpu, CHAPTER_PARTS + chapter * 4u);
                fprintf(stderr, "[player] body part %u (chapter %u's) for %d, not %u\n", part, chapter, want,
                        cpu->gpr[4]);
                cpu->gpr[4] = part;
                break;
            }
    }
    g_hle_passthrough = true;
}

// The chapter's level data, /levels/levelNN/U_LevelNN.bin (NN the chapter),
// loaded by 0x8013209C(name, 0, 1) as the chapter loads (the call returning to
// CHAPTER_PACK_CALL); its first list holds the bodies the chapter's characters
// are made from, by character (0x801DCED0(pack, 0): count, 0x6B5, then 8 bytes
// each). Another chapter's character has no body there: their own chapter's
// pack is loaded too, and their body copied across.
#define PACK_LOAD 0x8013209Cu
#define CHAPTER_PACK_CALL 0x800408FCu
// The chapter's level memory: a bump allocator, its base at r13 - 23736 and
// what is used at r13 - 23656, 0x3D3100 bytes. Their chapter's level data
// (3.6 MB unpacked for Roberto's) is loaded into the host's scratch past the
// game's 24 MB instead, and the body kept there too (SCRATCH_BODY).
#define LEVEL_HEAP_BASE 23736u
#define LEVEL_HEAP_USED 23656u
#define SCRATCH_RAM 0x81800000u
#define SCRATCH_BODY 0x81C00000u  // past the most level data unpacks to (0x3D3100 bytes)
#define SCRATCH_BODY2 0x81E00000u  // player 2's (co-op), when theirs is another chapter's too
#define SCRATCH_COSTUME 0x81F00000u  // player 1's other body (costume_file)
#define SCRATCH_COSTUME2 0x81F80000u  // player 2's
#define SCRATCH_HUD 0x81FFF000u  // player 2's bars (coop_bar_copy), COOP_BAR_SLOT each
#define SCRATCH_END 0x82000000u
static struct {
    int stage;  // 0 none, 1 the chapter's own loading, 2 a character's
    u32 caller, pack;
    u32 base, used;  // the level memory's, put back after the scratch load
    int model;       // the body's number in the list the load in stage 2 is for
    int filed;       // and its number in the chapter's list
    u32 kept;        // where it is kept
} g_pack;

// Player 2's moves, as another character. A playable's moves come in parts
// (the base moves, a weapon's, ...) streamed from their file: 0x80132970
// (object, part) asks for one, into one of two slots kept for the one
// player (0x80582000, +696 each: the id, +764 the part made ready, 80 bytes
// each), and once it is in, gives the object the table of the part's sets
// by id it plays from (0x80133310: +644 the part, +648 the table, +652 the
// sets, +726 how many). The file's first 1 KB lists the parts (+16 how
// many, +20 where: 16 bytes each, +0 offset, +4 size, +8 the id); a part
// read is made ready by 0x8011E89C(part, 0) and its sets listed after it by
// 0x8012484C(part, sets) (16 bytes each: the animation's id, its resource,
// the part, flags). Player 2 asking too took player 1's slots from them
// (player 1 twisted when player 2 took a sword) and got player 1's moves:
// their asks are answered here instead, from their own character's file,
// into two slots of their own in the scratch. Their kind stays the
// chapter's, as the story's checks want.
#define MOVES_WANT 0x80132970u
#define MOVES_READY 0x8011E89Cu
#define MOVES_SETS 0x8012484Cu
#define MOVES_ENTRY 0x8012E414u  // a resource's streamed animation (cache entry), or 0
#define MOVES_FREE 0x8012E9ACu
// Two slots are player 2's and two anyone else's: player 1, once a story
// scene has opened the chapter's own character's file again (Anthony's,
// after his coffin), plays from them too, and one slot for both gave player
// 2 player 1's moves (Paul twisted, kneeling, as Ellia's).
#define P2_MOVES_SLOT 0x100000u  // each: the table (170 sets), then the part from +0x400
#define P2_MOVES_SLOTS 4
#define P2_MOVES_OTHERS 2        // the first of the others'
static struct {
    int model, part;  // what is in the slot, -1 nothing
    u32 ready, sets;
} g_p2_moves[P2_MOVES_SLOTS + 2] = {{-1, -1, 0u, 0u}, {-1, -1, 0u, 0u}, {-1, -1, 0u, 0u}, {-1, -1, 0u, 0u}, {-1, -1, 0u, 0u}, {-1, -1, 0u, 0u}};
// A file's moves for a form's held pose, past the four and past the first body (slot 4 would be on SCRATCH_BODY).
#define COSTUME_MOVES_SLOT 5
// Track lists too long for where they streamed in (costume_tracks), past that file's part.
#define SCRATCH_TRACKS 0x81D90000u
#define SCRATCH_TRACKS_SIZE 0x70000u
#define COSTUME_TRACKS_LIST 1280u  // 80 bones
#define COSTUME_TRACKS_KEYS 1280u  // what held bones' keys take

// A part's place in a file's list (16 bytes each, the id at +8), or -1.
static int moves_listed(CPUState* cpu, u32 list, u32 count, int part) {
    for (u32 i = 0; i < count && i < 64u; i++)
        if (mem_read16(cpu, list + i * 16u + 8u) == (u32)part)
            return (int)i;
    return -1;
}

// Parts come in sevens, one seven to a kind of weapon. Each character's
// file has only the kinds they hold in their own chapter (Pius's and Alex's
// none of Anthony's sword's): another's weapon in their hands asked for a
// part their file has not, and the moves twisted. Theirs of the same place
// in the sword's seven (14), else the bare hands' (0), is played instead.
static int moves_part(CPUState* cpu, u32 list, u32 count, int part) {
    static const int kInstead[2] = {14, 0};
    for (int i = 0; i < 2 && moves_listed(cpu, list, count, part) < 0; i++)
        if (moves_listed(cpu, list, count, kInstead[i] + part % 7) >= 0)
            part = kInstead[i] + part % 7;
    return part;
}

static void p2_moves_clear(void) {
    for (int k = 0; k < P2_MOVES_SLOTS + 2; k++)
        g_p2_moves[k].model = g_p2_moves[k].part = -1;
}

static bool p2_moves_load(CPUState* cpu, int k, int model, int part) {
    const u32 table = SCRATCH_RAM + (u32)k * P2_MOVES_SLOT;
    const u32 at = table + 0x400u;
    const u32 end = table + P2_MOVES_SLOT;
    // What was in it lets go of its animations first.
    if (g_p2_moves[k].model >= 0) {
        const u32 n = mem_read16(cpu, g_p2_moves[k].ready + 20u);
        for (u32 i = 0; i < n; i++) {
            const u32 resource = mem_read32(cpu, g_p2_moves[k].sets + i * 16u + 4u);
            u32 entry = 0u, ignore = 0u;
            if (hle_guest_call(cpu, MOVES_ENTRY, &resource, 1, 0.0, &entry) && entry != 0u) {
                mem_write8(cpu, entry + 22u, 0u);
                hle_guest_call(cpu, MOVES_FREE, &entry, 1, 0.0, &ignore);
            }
        }
        g_p2_moves[k].model = g_p2_moves[k].part = -1;
    }
    char path[32];
    snprintf(path, sizeof path, "/Chars/char%04d.cpk", model);
    const s32 file = dvd_path_to_entrynum(path);
    u32 start = 0u, length = 0u;
    if (file < 0 || !dvd_entry_info(file, &start, &length) || length < 1024u) {
        fprintf(stderr, "[coop] player 2's moves: no %s\n", path);
        return false;
    }
    // The list, then the part over it.
    dvd_read_to_guest(cpu, at, start, 1024u);
    const u32 count = mem_read32(cpu, at + 16u);
    const u32 list = mem_read32(cpu, at + 20u);
    u32 offset = 0u, size = 0u;
    const u32 listed = count <= 64u && list + count * 16u <= 1024u ? count : 0u;
    const int i = moves_listed(cpu, at + list, listed, moves_part(cpu, at + list, listed, part));
    if (i >= 0) {
        offset = mem_read32(cpu, at + list + (u32)i * 16u);
        size = mem_read32(cpu, at + list + (u32)i * 16u + 4u);
    }
    const u32 sets = (at + size + 31u) & ~31u;
    if (size == 0u || offset + size > length || sets + 0x2000u > end) {
        fprintf(stderr, "[coop] player 2's moves: no part %d in %s\n", part, path);
        return false;
    }
    dvd_read_to_guest(cpu, at, start + offset, size);
    const u32 ready_args[2] = {at, 0u};
    u32 ready = 0u, ignore = 0u;
    if (!hle_guest_call(cpu, MOVES_READY, ready_args, 2, 0.0, &ready) || ready == 0u)
        return false;
    const u32 sets_args[2] = {ready, sets};
    hle_guest_call(cpu, MOVES_SETS, sets_args, 2, 0.0, &ignore);
    for (u32 i = 0; i < 170u; i++)
        mem_write32(cpu, table + i * 4u, 0u);
    const u32 n = mem_read16(cpu, ready + 20u);
    for (u32 i = 0; i < n && sets + i * 16u + 16u <= end; i++) {
        const u32 id = mem_read32(cpu, sets + i * 16u);
        if (id < 170u)
            mem_write32(cpu, table + id * 4u, sets + i * 16u);
    }
    g_p2_moves[k].model = model;
    g_p2_moves[k].part = part;
    g_p2_moves[k].ready = ready;
    g_p2_moves[k].sets = sets;
    fprintf(stderr, "[coop] player 2's moves: part %d of %s (%u bytes, %u sets)\n", part, path, size, n);
    return true;
}

static bool g_coop_turn;
static u32 g_coop_turn_p1;

static void hle_moves_want(CPUState* cpu) {
    g_hle_passthrough = true;
    const u32 object = cpu->gpr[3];
    const int part = (s32)cpu->gpr[4];
    if (part < 0 || !guest_ram_range_valid(cpu, object + 580u, 4u) || cpu->ram_size < SCRATCH_BODY - GC_RAM_BASE)
        return;
    // Player 2, or anyone else playing a playable's moves who is not the one
    // whose file they are from.
    // Player 2 is known by their character too: a load (a save, a reload for
    // changed characters) makes them and asks their moves before they are
    // found again as player 2, and theirs went into the others' slots, where
    // player 1's next took them over (Paul twisted).
    const int model = (s32)mem_read32(cpu, object + 580u);
    const u32 p1 = g_coop_turn ? g_coop_turn_p1 : coop_player(cpu);
    const bool p2 = (g_coop_p2 != 0u && mem_read32(cpu, g_coop_p2 + 52u) == object) ||
                    (g_settings.mod_coop && model == coop_wanted_character() && (p1 == 0u || mem_read32(cpu, p1 + 52u) != object));
    if (!coop_playable(model))
        return;
    if (!p2 && model == (s16)mem_read16(cpu, COOP_PLAYABLE_SLOT + 4u)) {
        // Player 1, from the playable's file (its list kept at 0x80582360).
        const u32 header = 0x80582360u;
        const u32 list = mem_read32(cpu, header + 20u);
        const u32 count = mem_read32(cpu, header + 16u);
        if (guest_ram_range_valid(cpu, list, 16u) && count != 0u && count <= 64u)
            cpu->gpr[4] = (u32)moves_part(cpu, list, count, part);
        if ((int)cpu->gpr[4] != part) {
            static int told = -1;
            if (told != part)
                fprintf(stderr, "[player] moves part %d not in %d's file: %d instead\n", part, model, (int)cpu->gpr[4]);
            told = part;
        }
        return;
    }
    const int first = p2 ? 0 : P2_MOVES_OTHERS;
    int k = -1;
    for (int i = first; i < first + 2 && k < 0; i++)
        if (g_p2_moves[i].model == model && g_p2_moves[i].part == part)
            k = i;
    if (k < 0) {
        // Into the slot of theirs they are not playing from.
        k = mem_read32(cpu, object + 648u) == SCRATCH_RAM + (u32)first * P2_MOVES_SLOT ? first + 1 : first;
        if (!p2_moves_load(cpu, k, model, part))
            return;
    }
    mem_write32(cpu, object + 644u, g_p2_moves[k].ready);
    mem_write32(cpu, object + 648u, SCRATCH_RAM + (u32)k * P2_MOVES_SLOT);
    mem_write32(cpu, object + 652u, g_p2_moves[k].sets);
    mem_write16(cpu, object + 726u, 170u);
    g_hle_passthrough = false;
    hle_set_u32(cpu, 1u);
}

static const Costume* costume_object(CPUState* cpu, u32 object);

// A body whose skeleton word isn't its wearer's (Edward's ghost: 62 bones to his 72, and no file
// of its own moves) still plays the wearer's moves, so the moves are fitted to it. A body's bones
// are 80 bytes each from its +36, each with a name's hash at +64. A move's tracks are a list by bone
// number, 16 bytes each, at the first word of its record (a set's +4), and the bone count +1 reads
// one more for the root when the part's +36 is within the bones (0x801226C0): so that's moved too.
// Each player's table and sets are their own slot's, so the other player's moves aren't touched.
#define COSTUME_MAX_BONES 128u
#define COSTUME_NEAR 130.0f
#define MOVES_STREAM 0x8012E588u

static u32 body_bones(CPUState* cpu, u32 body) {
    const u32 at = mem_read32(cpu, body + 36u);
    return at < 0x80000000u ? body + at : at;
}

static struct {
    u32 body, plain;
    u32 bones, plain_bones;
    int from[COSTUME_MAX_BONES];
} g_costume_maps[COSTUME_PLACES];
static __typeof__(&g_costume_maps[0]) g_costume_map;

// Which of the wearer's bones each of the body's is: by name, or where few names match, by place. The
// body's bones then take those names, as what a character holds finds its bone by name.
static void costume_bone_map(CPUState* cpu, const Costume* c, u32 body, u32 plain) {
    g_costume_map = &g_costume_maps[c->place];
    if (g_costume_map->body != body || g_costume_map->plain != plain) {
        g_costume_map->body = body;
        g_costume_map->plain = plain;
        g_costume_map->bones = mem_read16(cpu, body + 8u);
        g_costume_map->plain_bones = mem_read16(cpu, plain + 8u);
        if (g_costume_map->bones > COSTUME_MAX_BONES - 1u || g_costume_map->plain_bones > COSTUME_MAX_BONES - 1u)
            g_costume_map->bones = 0u;
        const u32 own = body_bones(cpu, body), theirs = body_bones(cpu, plain);
        u32 named = 0u, placed = 0u;
        bool taken[COSTUME_MAX_BONES] = {false};
        for (u32 i = 0u; i < g_costume_map->bones; i++) {
            g_costume_map->from[i] = -1;
            const u32 name = mem_read32(cpu, own + i * 80u + 64u);
            for (u32 k = 0u; k < g_costume_map->plain_bones && g_costume_map->from[i] < 0; k++)
                if (mem_read32(cpu, theirs + k * 80u + 64u) == name) {
                    g_costume_map->from[i] = (int)k;
                    taken[k] = true;
                    named++;
                }
        }
        // Some bodies name their bones otherwise, or have more (Ellia's rotten one, the Liche's cape).
        // A bone with no track stays where it was made, while the rest walk off: so each of those takes
        // the nearest free bone of theirs under the same parent, parents first, when few go by name
        // (a bone that's truly extra is better left as made than on another's track). A bone's parent is
        // the high half of its +76, its place as made the matrix at its +12.
        if (g_costume_map->from[0] < 0 && !taken[0]) {
            g_costume_map->from[0] = 0;
            taken[0] = true;
        }
        for (u32 i = 1u; i < g_costume_map->bones && named * 2u < g_costume_map->bones; i++) {
            if (g_costume_map->from[i] >= 0)
                continue;
            const u32 parent = mem_read16(cpu, own + i * 80u + 76u);
            if (parent >= g_costume_map->bones || g_costume_map->from[parent] < 0)
                continue;
            float best = COSTUME_NEAR * COSTUME_NEAR;
            for (u32 k = 1u; k < g_costume_map->plain_bones; k++) {
                if (taken[k] || (int)mem_read16(cpu, theirs + k * 80u + 76u) != g_costume_map->from[parent])
                    continue;
                float apart = 0.0f;
                for (u32 axis = 0u; axis < 3u; axis++) {
                    const u32 at = 24u + axis * 16u;
                    const float d = guest_read_f32(cpu, own + i * 80u + at) - guest_read_f32(cpu, theirs + k * 80u + at);
                    apart += d * d;
                }
                if (apart < best) {
                    best = apart;
                    g_costume_map->from[i] = (int)k;
                }
            }
            if (g_costume_map->from[i] >= 0) {
                taken[g_costume_map->from[i]] = true;
                placed++;
            }
        }
        for (u32 i = 0u; i < g_costume_map->bones; i++)
            if (g_costume_map->from[i] >= 0)
                mem_write32(cpu, own + i * 80u + 64u, mem_read32(cpu, theirs + (u32)g_costume_map->from[i] * 80u + 64u));
        fprintf(stderr, "[player] %s's moves fitted to their other body: of its %u bones, %u by name and %u by place in their %u\n", c->name, g_costume_map->bones, named, placed,
                g_costume_map->plain_bones);
    }
}

// The other body an object wears, when it isn't on the wearer's skeleton and the wearer's moves are
// to be fitted to it, or NULL.
static const Costume* costume_to_fit(CPUState* cpu, u32 object) {
    const Costume* c = costume_object(cpu, object);
    if (c == NULL)
        return NULL;
    const u32 body = mem_read32(cpu, object + 60u), plain = g_costume_plain[c->place];
    if (!guest_ram_range_valid(cpu, body, 16u) || !guest_ram_range_valid(cpu, plain, 16u) || mem_read32(cpu, body + 4u) == mem_read32(cpu, plain + 4u))
        return NULL;  // the same skeleton: their moves fit as they are
    costume_bone_map(cpu, c, body, plain);
    return g_costume_map->bones != 0u ? c : NULL;
}

// A move's tracks, just streamed in and made addresses (0x8012E588, whose result is the cache's
// entry): the other body's bones get theirs, and the root the place after its last.
// A bone the wearer hasn't stays as it was modelled, and the Liche's cape was modelled flying out. Its
// file has it hanging, and a move's data is packed on the disc, so the game streams one standing move
// from that file, once, after the wearer's own; each bone's first key of each list is kept from it.
// Those bones then hold that key for as long as each move lasts. A track's keys are 12 bytes, and
// the high 30 bits of a key's first word its time: all of a move's tracks end on the same key.
static struct {
    u32 caller, record;
    int place, held_file;
    u32 object, result, held_record;
    bool holding;
} g_tracks_load;

static struct {
    bool tried, have;
    struct {
        u32 n[2];
        u8 key[2][12];
    } bone[COSTUME_MAX_BONES];
} g_costume_held;

static void hle_tracks_load(CPUState* cpu) {
    g_hle_passthrough = true;
    const Costume* c = g_tracks_load.caller == 0u ? costume_to_fit(cpu, cpu->gpr[3]) : NULL;
    if (c != NULL && cpu->ram_size >= SCRATCH_END - GC_RAM_BASE) {
        g_tracks_load.caller = cpu->lr;
        g_tracks_load.place = c->place;
        g_tracks_load.held_file = c->held_file;
        g_tracks_load.object = cpu->gpr[3];
        g_tracks_load.record = cpu->gpr[4];
        g_tracks_load.holding = false;
        cpu->lr = HLE_TRACKS_RETURN;
    }
}

static void costume_held_keep(CPUState* cpu, u32 record) {
    const u32 tracks = mem_read32(cpu, record);
    const u32 count = mem_read16(cpu, record + 4u);
    if (count > COSTUME_MAX_BONES || !guest_ram_range_valid(cpu, tracks, count * 16u))
        return;
    for (u32 i = 0u; i < count; i++)
        for (u32 list = 0u; list < 2u; list++) {
            const u32 n = mem_read16(cpu, tracks + i * 16u + list * 8u);
            const u32 keys = mem_read32(cpu, tracks + i * 16u + list * 8u + 4u);
            g_costume_held.bone[i].n[list] = n != 0u && guest_ram_range_valid(cpu, keys, 12u) ? n : 0u;
            if (g_costume_held.bone[i].n[list] != 0u)
                for (u32 k = 0u; k < 12u; k++)
                    g_costume_held.bone[i].key[list][k] = (u8)mem_read8(cpu, keys + k);
        }
    g_costume_held.have = true;
    // It has kept what's wanted: the cache has its place back, as when a slot's moves let go.
    u32 entry = 0u, ignore = 0u;
    if (hle_guest_call(cpu, MOVES_ENTRY, &record, 1, 0.0, &entry) && entry != 0u) {
        mem_write8(cpu, entry + 22u, 0u);
        hle_guest_call(cpu, MOVES_FREE, &entry, 1, 0.0, &ignore);
    }
    fprintf(stderr, "[player] the held pose kept, from %u tracks\n", count);
}

static bool continue_tracks_load(CPUState* cpu) {
    if (g_tracks_load.caller == 0u)
        return false;
    if (g_tracks_load.holding) {
        costume_held_keep(cpu, g_tracks_load.held_record);
        cpu->gpr[3] = g_tracks_load.result;
    } else if (g_tracks_load.held_file >= 0 && !g_costume_held.tried) {
        g_costume_held.tried = true;
        g_tracks_load.result = cpu->gpr[3];
        u32 held = 0u;
        if (p2_moves_load(cpu, COSTUME_MOVES_SLOT, g_tracks_load.held_file, 0)) {
            const u32 sets = g_p2_moves[COSTUME_MOVES_SLOT].sets;
            const u32 n = mem_read16(cpu, g_p2_moves[COSTUME_MOVES_SLOT].ready + 20u);
            for (u32 i = 0u; i < n; i++)
                if (mem_read32(cpu, sets + i * 16u) == 15u || (held == 0u && mem_read32(cpu, sets + i * 16u) == 17u))
                    held = mem_read32(cpu, sets + i * 16u + 4u);
        }
        cpu->gpr[3] = g_tracks_load.result;
        if (held != 0u) {
            g_tracks_load.holding = true;
            g_tracks_load.held_record = held;
            cpu->gpr[3] = g_tracks_load.object;
            cpu->gpr[4] = held;
            cpu->lr = HLE_TRACKS_RETURN;
            cpu->pc = MOVES_STREAM;
            return true;
        }
    }
    const u32 record = g_tracks_load.record;
    const bool hold_them = g_tracks_load.held_file >= 0 && g_costume_held.have;
    cpu->lr = cpu->pc = g_tracks_load.caller;
    g_tracks_load.caller = 0u;
    const u32 tracks = guest_ram_range_valid(cpu, record, 8u) ? mem_read32(cpu, record) : 0u;
    const u32 count = guest_ram_range_valid(cpu, record, 8u) ? mem_read16(cpu, record + 4u) : 0u;
    g_costume_map = &g_costume_maps[g_tracks_load.place];
    const u32 bones = g_costume_map->bones, theirs = g_costume_map->plain_bones;
    if (count < theirs || count > COSTUME_MAX_BONES || !guest_ram_range_valid(cpu, tracks, count * 16u))
        return true;
    // More bones than they have (the Liche's 75 to Pius's 72), or keys to add: the list goes where
    // there's room. The record names it, and the next time it's streamed in the game names the new one.
    u32 fitted = tracks;
    const bool moved = bones + 1u > count || hold_them;
    if (moved) {
        if ((bones + 1u) * 16u > COSTUME_TRACKS_LIST)
            return true;
        static u32 kept[SCRATCH_TRACKS_SIZE / (COSTUME_TRACKS_LIST + COSTUME_TRACKS_KEYS)], used;
        const u32 slots = (u32)(sizeof kept / sizeof kept[0]);
        u32 k = 0u;
        while (k < used && kept[k] != record)
            k++;
        if (k == used) {
            if (used == slots)
                used = k = 0u;
            kept[k] = record;
            used++;
        }
        fitted = SCRATCH_TRACKS + k * (COSTUME_TRACKS_LIST + COSTUME_TRACKS_KEYS);
    }
    u32 old[COSTUME_MAX_BONES * 4u];
    for (u32 i = 0u; i < count * 4u; i++)
        old[i] = mem_read32(cpu, tracks + i * 4u);
    // How long this move lasts, from its longest track.
    u32 first = 0u, last = 0u, longest = 0u;
    for (u32 i = 0u; i < theirs; i++)
        for (u32 list = 0u; list < 2u; list++) {
            const u32 n = old[i * 4u + list * 2u] >> 16;
            const u32 keys = old[i * 4u + list * 2u + 1u];
            if (n > longest && guest_ram_range_valid(cpu, keys, n * 12u)) {
                longest = n;
                first = mem_read32(cpu, keys);
                last = mem_read32(cpu, keys + (n - 1u) * 12u);
            }
        }
    u32 keys_at = fitted + COSTUME_TRACKS_LIST;
    const u32 keys_end = keys_at + COSTUME_TRACKS_KEYS;
    const u32 places = bones + 1u > count ? bones + 1u : count;
    for (u32 i = 0u; i < places; i++) {
        for (u32 w = 0u; w < 4u; w++) {
            u32 value = 0u;
            if (i < bones && g_costume_map->from[i] >= 0)
                value = old[(u32)g_costume_map->from[i] * 4u + w];
            else if (i == bones && count > theirs)
                value = old[theirs * 4u + w];
            mem_write32(cpu, fitted + i * 16u + w * 4u, value);
        }
        if (!hold_them || longest == 0u || i >= bones || g_costume_map->from[i] >= 0)
            continue;
        for (u32 list = 0u; list < 2u; list++) {
            if (g_costume_held.bone[i].n[list] == 0u || keys_at + 24u > keys_end)
                continue;
            for (u32 key = 0u; key < 2u; key++) {
                for (u32 b = 0u; b < 12u; b++)
                    mem_write8(cpu, keys_at + key * 12u + b, g_costume_held.bone[i].key[list][b]);
                const u32 word = mem_read32(cpu, keys_at + key * 12u);
                mem_write32(cpu, keys_at + key * 12u, ((key == 0u ? first : last) & ~3u) | (word & 3u));
            }
            mem_write16(cpu, fitted + i * 16u + list * 8u, 2u);
            mem_write32(cpu, fitted + i * 16u + list * 8u + 4u, keys_at);
            keys_at += 24u;
        }
    }
    mem_write32(cpu, record, fitted);
    return true;
}

// The part says which track is the root (0x801226C0 asks, as each move starts), and a change of
// weapon brings another part: so this looks each frame, and before a move.
static void costume_tracks_on(CPUState* cpu, u32 object) {
    if (costume_to_fit(cpu, object) == NULL)
        return;
    const u32 table = mem_read32(cpu, object + 648u);
    if (!guest_ram_range_valid(cpu, table, 170u * 4u))
        return;
    u32 last = 0u;
    for (u32 i = 0u; i < 170u; i++) {
        const u32 set = mem_read32(cpu, table + i * 4u);
        const u32 part = guest_ram_range_valid(cpu, set, 16u) ? mem_read32(cpu, set + 8u) : 0u;
        if (part != last && guest_ram_range_valid(cpu, part, 40u) && mem_read16(cpu, part + 36u) == g_costume_map->plain_bones)
            mem_write16(cpu, part + 36u, (u16)g_costume_map->bones);
        last = part;
    }
}

// Each frame, for what remakes the table with no move played yet (a change of weapon).
static void costume_moves(CPUState* cpu, u32 node) {
    if (node != 0u && guest_ram_range_valid(cpu, node + 52u, 4u))
        costume_tracks_on(cpu, mem_read32(cpu, node + 52u));
}

// And before any move of theirs is looked up, so the first is their own: from the frame's
// look alone, the first picture after they were made showed Pius's standing, twisted.
#define PLAY_BY_NUMBER 0x80122920u

static void hle_play_by_number(CPUState* cpu) {
    g_hle_passthrough = true;
    costume_tracks_on(cpu, cpu->gpr[3]);
}

// A player playing from a slot not theirs (player 2 from the others', player
// 1 from player 2's), or player 2 holding another's moves (a state saved
// while they shared one): theirs asked for again, the same part.
static void moves_repair(CPUState* cpu, u32 node, bool p2) {
    static u32 tried[2];  // the table last asked again for, each player
    const u32 object = node != 0u ? mem_read32(cpu, node + 52u) : 0u;
    if (!guest_ram_range_valid(cpu, object + 648u, 4u))
        return;
    const u32 table = mem_read32(cpu, object + 648u);
    const int model = (s32)mem_read32(cpu, object + 580u);
    for (int k = 0; k < P2_MOVES_SLOTS; k++) {
        if (table != SCRATCH_RAM + (u32)k * P2_MOVES_SLOT)
            continue;
        const bool own = p2 ? k < P2_MOVES_OTHERS : k >= P2_MOVES_OTHERS;
        if ((own && g_p2_moves[k].model == model) || table == tried[p2])
            return;
        tried[p2] = table;
        const u32 args[2] = {object, (u32)(g_p2_moves[k].part >= 0 ? g_p2_moves[k].part : 0)};
        u32 ignore = 0u;
        fprintf(stderr, "[coop] player %d (%d) played slot %d's moves (%d's): theirs again\n", p2 ? 2 : 1, model, k,
                g_p2_moves[k].model);
        hle_guest_call(cpu, MOVES_WANT, args, 2, 0.0, &ignore);
        // Player 1's part was already in the game's slot, so the game gave them its table as it
        // was, and that was never made: they asked first for the slot of ours (the same character
        // as player 2). Every move then came back empty, and the next one waited for ever.
        const u32 table_now = mem_read32(cpu, object + 648u), ready = mem_read32(cpu, object + 644u), sets = mem_read32(cpu, object + 652u);
        if (!p2 && guest_ram_range_valid(cpu, table_now, 170u * 4u) && guest_ram_range_valid(cpu, ready + 20u, 2u) && guest_ram_range_valid(cpu, sets, 16u)) {
            bool empty = true;
            for (u32 i = 0u; i < 170u && empty; i++)
                empty = mem_read32(cpu, table_now + i * 4u) == 0u;
            const u32 n = mem_read16(cpu, ready + 20u);
            for (u32 i = 0u; empty && i < n && guest_ram_range_valid(cpu, sets + i * 16u, 16u); i++) {
                const u32 id = mem_read32(cpu, sets + i * 16u);
                if (id < 170u)
                    mem_write32(cpu, table_now + id * 4u, sets + i * 16u);
            }
            if (empty)
                fprintf(stderr, "[coop] player 1's moves table was empty: made from %u sets\n", n);
        }
        return;
    }
}

// A loaded pack holds offsets: 0x801DCDD8(pack) makes the top level's
// addresses (each entry's offset plus the pack's own, once), as the chapter
// does right after its load, then the same for the list inside.
#define PACK_RELOCATE 0x801DCDD8u

static u32 pack_list(CPUState* cpu, u32 pack, bool relocated) {
    if (!guest_ram_range_valid(cpu, pack, 16u) || mem_read32(cpu, pack + 4u) != 0x6B5u || mem_read32(cpu, pack) == 0u)
        return 0u;
    const u32 list = mem_read32(cpu, pack + 8u) + (relocated ? 0u : pack);
    return guest_ram_range_valid(cpu, list, 8u) && mem_read32(cpu, list + 4u) == 0x6B5u ? list : 0u;
}

// The bodies to borrow, as the chapter loads: player 1's character's, and in
// co-op player 2's when they are another again. -1 when there is none.
static int borrow_p1(void) {
    const int want = player_model();
    return want >= 0 && g_chapter_model >= 0 && want != g_chapter_model ? want : -1;
}

static int borrow_p2(void) {
    const int p2 = coop_wanted_character();
    const int p1 = player_model() >= 0 ? player_model() : g_chapter_model;
    return p2 >= 0 && g_chapter_model >= 0 && p2 != g_chapter_model && p2 != p1 ? p2 : -1;
}

// A player's other body, unless this chapter's own list has it (the Liche, in Alex's
// last) or the other player's load brought it already.
static Costume* borrow_costume(CPUState* cpu, int player) {
    Costume* c = costume_on(player);
    if (c == NULL)
        return NULL;
    const u32 own = pack_list(cpu, g_pack.pack, false);
    return own != 0u && mem_read32(cpu, own) > (u32)c->filed && mem_read32(cpu, own + 8u + (u32)c->filed * 8u) == 0u ? c : NULL;
}

static unsigned g_playables_made;  // records of the chapter's kind made since the chapter loaded

static void hle_pack_load(CPUState* cpu) {
    // The chapter, from the name being loaded (/levels/levelNN/...): a save's
    // load asks for its character only after this (0x80007C8C), so the
    // chapter's character is not known from that yet.
    if (g_pack.stage == 0 && cpu->lr == CHAPTER_PACK_CALL) {
        char name[32] = {0};
        for (u32 i = 0; i + 1u < sizeof name && guest_ram_range_valid(cpu, cpu->gpr[3] + i, 1u); i++)
            if ((name[i] = (char)mem_read8(cpu, cpu->gpr[3] + i)) == '\0')
                break;
        int chapter = -1;
        if (sscanf(name, "/levels/level%d", &chapter) == 1 && chapter >= 0 && chapter < 17)
            g_chapter_model = kChapterModel[chapter];
        g_playables_made = 0u;
    }
    if (g_pack.stage == 0 && cpu->lr == CHAPTER_PACK_CALL && (borrow_p1() >= 0 || borrow_p2() >= 0 || costume_on(0) != NULL || costume_on(1) != NULL)) {
        g_pack.caller = cpu->lr;
        g_pack.stage = 1;
        cpu->lr = HLE_PACK_RETURN;
    }
    g_hle_passthrough = true;
}

// Their own chapter's level data (the first chapter that is theirs, 0 to 11)
// loaded into the host's scratch, for `model`'s body, kept at `kept`.
// Anthony's is not in his chapter's: it loads his body as the curse takes
// him, from /U_Tony0.bin to 4 (0x8003F9A8, each a list of bodies itself);
// his whole one is the first's.
#define ANTHONY 85
static bool borrow_load(CPUState* cpu, const char* name, int model, int filed, u32 kept) {
    const u32 at = cpu->gpr[1] + 156u;  // the chapter's own name was built here
    for (size_t i = 0; i <= strlen(name); i++)
        mem_write8(cpu, at + (u32)i, (u8)name[i]);
    fprintf(stderr, "[player] loading %s for %d's body\n", name, filed);
    mem_write32(cpu, cpu->gpr[13] - LEVEL_HEAP_BASE, SCRATCH_RAM);
    mem_write32(cpu, cpu->gpr[13] - LEVEL_HEAP_USED, 0u);
    g_pack.stage = 2;
    p2_moves_clear();  // player 2's moves, in the same scratch
    g_pack.model = model;
    g_pack.filed = filed;
    g_pack.kept = kept;
    cpu->gpr[3] = at;
    cpu->gpr[4] = 0u;
    cpu->gpr[5] = 1u;
    cpu->lr = HLE_PACK_RETURN;
    cpu->pc = PACK_LOAD;
    return true;
}

static bool start_borrow(CPUState* cpu, int model, u32 kept) {
    int chapter = -1;
    for (int c = 0; c < 12 && chapter < 0; c++)
        if (kChapterModel[c] == model)
            chapter = c;
    if (chapter < 0)
        return false;
    if (cpu->ram_size < SCRATCH_END - GC_RAM_BASE) {
        fprintf(stderr, "[player] no scratch for their chapter's data: %d has no body\n", model);
        return false;
    }
    // ED_BORROW_CHAPTER=n (testing): another chapter's data, to list its bodies.
    if (getenv("ED_BORROW_CHAPTER") != NULL)
        chapter = atoi(getenv("ED_BORROW_CHAPTER"));
    char name[48];
    if (model == ANTHONY && getenv("ED_BORROW_CHAPTER") == NULL)
        snprintf(name, sizeof name, "/U_Tony0.bin");
    else
        snprintf(name, sizeof name, "/levels/level%02d/U_Level%02d.bin", chapter, chapter);
    return borrow_load(cpu, name, model, model, kept);
}

// A player's other body, from where it is, kept in that player's place.
static bool start_borrow_costume(CPUState* cpu, const Costume* c, u32 kept) {
    if (cpu->ram_size < SCRATCH_END - GC_RAM_BASE) {
        fprintf(stderr, "[player] no scratch for %s's other body\n", c->name);
        return false;
    }
    char name[48];
    if (c->cutscene >= 0)
        snprintf(name, sizeof name, "/levels/level%02d/cin%04d.bin", c->level, c->cutscene);
    else
        snprintf(name, sizeof name, "/levels/level%02d/U_Level%02d.bin", c->level, c->level);
    return borrow_load(cpu, name, c->body, c->filed, kept);
}

// What is left to borrow after what's kept at `kept` (0: nothing yet).
static bool next_borrow(CPUState* cpu, u32 kept) {
    if (kept == 0u && borrow_p1() >= 0 && start_borrow(cpu, borrow_p1(), SCRATCH_BODY))
        return true;
    if ((kept == 0u || kept == SCRATCH_BODY) && borrow_p2() >= 0 && start_borrow(cpu, borrow_p2(), SCRATCH_BODY2))
        return true;
    const Costume* c;
    if (kept != SCRATCH_COSTUME && kept != SCRATCH_COSTUME2 && (c = borrow_costume(cpu, 0)) != NULL && start_borrow_costume(cpu, c, SCRATCH_COSTUME))
        return true;
    if (kept != SCRATCH_COSTUME2 && (c = borrow_costume(cpu, 1)) != NULL && start_borrow_costume(cpu, c, SCRATCH_COSTUME2))
        return true;
    return false;
}

static bool continue_pack_load(CPUState* cpu) {
    if (g_pack.stage == 1) {
        g_pack.pack = cpu->gpr[3];
        g_pack.base = mem_read32(cpu, cpu->gpr[13] - LEVEL_HEAP_BASE);
        g_pack.used = mem_read32(cpu, cpu->gpr[13] - LEVEL_HEAP_USED);
        if (next_borrow(cpu, 0u))
            return true;
    } else if (g_pack.stage == 2) {
        // Theirs made addresses (pack, then its list); the body's address goes
        // into the chapter's list as an offset from it, which the chapter's own
        // pass makes the address again.
        const u32 other = cpu->gpr[3];
        u32 ignore = 0u;
        hle_guest_call(cpu, PACK_RELOCATE, &other, 1, 0.0, &ignore);
        u32 theirs = other;  // Anthony's: the list itself
        if (g_pack.model != ANTHONY) {
            theirs = pack_list(cpu, other, true);
            if (theirs != 0u)
                hle_guest_call(cpu, PACK_RELOCATE, &theirs, 1, 0.0, &ignore);
        }
        const u32 own = pack_list(cpu, g_pack.pack, false);
        const u32 want = (u32)g_pack.model, filed = (u32)g_pack.filed;
        const u32 body = theirs != 0u && mem_read32(cpu, theirs) > want ? mem_read32(cpu, theirs + 8u + want * 8u) : 0u;
        if (own != 0u && body != 0u && mem_read32(cpu, own) > filed) {
            // The body kept in the host's scratch too, above where their level
            // data unpacks: the chapter's level memory stays all its own
            // (Karim's chapter, with Michael's body in it, ran out as its rooms
            // loaded). A body needs nothing else of its chapter's data, so the
            // next one unpacks over it.
            const u32 size = mem_read32(cpu, theirs + 12u + want * 8u);
            const u32 kept = g_pack.kept;
            const u32 room = (g_pack.kept == SCRATCH_BODY ? SCRATCH_BODY2 : g_pack.kept == SCRATCH_BODY2 ? SCRATCH_COSTUME :
                              g_pack.kept == SCRATCH_COSTUME ? SCRATCH_COSTUME2 : SCRATCH_HUD) - kept;
            if (size != 0u && size <= room && guest_ram_range_valid(cpu, body, size) &&
                guest_ram_range_valid(cpu, kept, size)) {
                memmove(cpu->ram + (kept - GC_RAM_BASE), cpu->ram + (body - GC_RAM_BASE), size);
                mem_write32(cpu, own + 8u + filed * 8u, kept - own);
                mem_write32(cpu, own + 12u + filed * 8u, size);
            }
            fprintf(stderr, "[player] %u's body from their chapter's level data (%u bytes kept at 0x%08X)\n", filed,
                    size, kept);
        } else {
            fprintf(stderr, "[player] no body for %u in their chapter's pack\n", want);
            if (theirs != 0u) {
                fprintf(stderr, "[player] its bodies (%u):", mem_read32(cpu, theirs));
                for (u32 i = 0; i < mem_read32(cpu, theirs) && i < 200u; i++) {
                    const u32 them = mem_read32(cpu, theirs + 8u + i * 8u);
                    if (them == 0u)
                        continue;
                    // Each body's size and its bones (+8): moves fit only a body with as many.
                    fprintf(stderr, " %u", i);
                    if (guest_ram_range_valid(cpu, them, 16u))
                        fprintf(stderr, "(%uB,%ub)", mem_read32(cpu, theirs + 12u + i * 8u), mem_read16(cpu, them + 8u));
                }
                fprintf(stderr, "\n");
            }
        }
        // Then player 2's, when theirs is borrowed too, then each other body.
        if (next_borrow(cpu, g_pack.kept))
            return true;
    } else {
        return false;
    }
    mem_write32(cpu, cpu->gpr[13] - LEVEL_HEAP_BASE, g_pack.base);
    mem_write32(cpu, cpu->gpr[13] - LEVEL_HEAP_USED, g_pack.used);
    cpu->gpr[3] = g_pack.pack;
    cpu->lr = g_pack.caller;
    cpu->pc = g_pack.caller;
    g_pack.stage = 0;
    return true;
}

// A cutscene's skip (Start, A or B: the scene's input table at 0x802436E0,
// all calling 0x800507DC; a narrated scene's, Start: 0x800505F0) is allowed
// once the game is finished: one of the
// three endings seen (0x80042190(1..3): bits of the progress block's byte at
// +6424). Every save may skip: that check, from there, is told yes; the scenes
// still play unless skipped.
#define ENDING_SEEN 0x80042190u

static void hle_ending_seen(CPUState* cpu) {
    // From the cutscenes' skip and the narrated scenes' (the "CinSubtitles"
    // table at 0x80243770: Start calls 0x800505F0, the same check).
    if (cpu->lr == 0x80050800u || cpu->lr == 0x8005080Cu || cpu->lr == 0x80050818u || cpu->lr == 0x80050614u ||
        cpu->lr == 0x80050620u || cpu->lr == 0x8005062Cu) {
        if (getenv("ED_SKIP_LOG") != NULL)
            fprintf(stderr, "[scene] skip asked (lr 0x%08X)\n", cpu->lr);
        hle_set_u32(cpu, 1u);
        return;
    }
    g_hle_passthrough = true;
}

// ED_SKIP_LOG (testing): each cutscene button press (0x800507DC(button)).
static void notify_scene_button(CPUState* cpu) {
    fprintf(stderr, "[scene] button 0x%X, end state %d, scene %u\n", cpu->gpr[3],
            (s8)mem_read8(cpu, 0x803162C0u + 472u), mem_read8(cpu, 0x803162C0u + 474u));
}

// Chapter scripts ask which character an object is (script function
// 0x8000C724 reads object +580, the character's file number, via 0x80117C64)
// and take another route when the player is not the chapter's own: Anthony's
// stairs go to the desk room instead of the tomb. While another character is
// played, scripts are told the player is the chapter's own. Player 2 too, while
// they are the player: to a clock in this chapter they were no one it knew, and it asked for
// no prompt.
#define OBJECT_MODEL 0x80117C64u
#define SCRIPT_MODEL_CALL 0x8000C7ACu

static void hle_object_model(CPUState* cpu) {
    const u32 object = cpu->gpr[3];
    const bool their_turn = g_settings.mod_coop && g_coop_p2 != 0u && guest_ram_range_valid(cpu, g_coop_p2 + 52u, 4u) &&
                            object == mem_read32(cpu, g_coop_p2 + 52u);
    if (cpu->lr == SCRIPT_MODEL_CALL && g_chapter_model >= 0 && object == mem_read32(cpu, cpu->gpr[13] - 26524u) &&
        guest_ram_range_valid(cpu, object + 580u, 4u) &&
        (their_turn || (player_model() >= 0 && (s32)mem_read32(cpu, object + 580u) == player_model()))) {
        hle_set_u32(cpu, (u32)g_chapter_model);
        return;
    }
    hle_set_u32(cpu, guest_ram_range_valid(cpu, object + 580u, 4u) ? mem_read32(cpu, object + 580u) : 0u);
}

// An animation another chapter's character does not have. 0x80123224(object,
// animation, ...) plays one: its resource (animation +184, then +4) names the
// character file (+280) and offset (+284) to stream it from, as needed
// (0x8012E504). Karim's chapter starts by playing one of Karim's own; for
// Michael its resource names nothing, and the stream asked for nothing at all
// was waited on for ever. While another character is played, such an
// animation is passed over: the character carries on as they were.
#define PLAY_ANIMATION 0x80123224u
static void room_log(CPUState* cpu, const char* what);

static void hle_play_animation(CPUState* cpu) {
    g_hle_passthrough = true;
    // ED_ANIM_TRACE=1 (testing, only reads): each animation played that the last few calls did not, with who asks.
    static int anim_trace = -1;
    if (anim_trace < 0)
        anim_trace = getenv("ED_ANIM_TRACE") != NULL;
    if (anim_trace && guest_ram_range_valid(cpu, cpu->gpr[4] + 184u, 4u)) {
        static u32 recent[64];
        static unsigned next;
        const u32 mark = cpu->gpr[3] ^ cpu->gpr[4];
        bool seen = false;
        for (int i = 0; i < 64 && !seen; i++)
            seen = recent[i] == mark;
        const u32 anim_set = mem_read32(cpu, cpu->gpr[4] + 184u);
        if (!seen || anim_set == 0u) {
            recent[next++ % 64u] = mark;
            fprintf(stderr, "[anim-trace] object 0x%08X set 0x%08X r5 0x%X r6 0x%X player 0x%08X |", cpu->gpr[3], anim_set, cpu->gpr[5], cpu->gpr[6], mem_read32(cpu, cpu->gpr[13] - 26524u));
            room_log(cpu, "play");
        }
    }
    // In co-op too, with the chapter's own character: Paul's waited for ever on 0x80516F70, which has no set.
    const bool chosen = player_model() >= 0;
    if (!chosen && (!g_settings.mod_coop || g_coop_p2 == 0u))
        return;
    const u32 animation = cpu->gpr[4];
    const u32 set = guest_ram_range_valid(cpu, animation + 184u, 4u) ? mem_read32(cpu, animation + 184u) : 0u;
    const u32 resource = guest_ram_range_valid(cpu, set + 4u, 4u) ? mem_read32(cpu, set + 4u) : 0u;
    static unsigned seen;
    if (getenv("ED_STREAM_LOG") != NULL && seen++ < 40u)
        fprintf(stderr, "[play] object 0x%08X animation 0x%08X set 0x%08X resource 0x%08X file %d offset 0x%X (lr 0x%08X)\n",
                cpu->gpr[3], animation, set, resource,
                guest_ram_range_valid(cpu, resource, 296u) ? (s32)mem_read32(cpu, resource + 280u) : -9,
                guest_ram_range_valid(cpu, resource, 296u) ? mem_read32(cpu, resource + 284u) : 0u, cpu->lr);
    // No set at all (Michael, Karim's chapter's first animation), or a
    // resource naming no file.
    if (set != 0u && (!chosen || !guest_ram_range_valid(cpu, resource, 296u) || mem_read32(cpu, resource + 280u) != 0u ||
                      mem_read32(cpu, resource + 284u) != 0u))
        return;
    static unsigned told;
    if (told++ < 8u)
        fprintf(stderr, "[player] an animation the character does not have, passed over (0x%08X; object 0x%08X, lr 0x%08X)\n", resource, cpu->gpr[3], cpu->lr);
    g_hle_passthrough = false;
}

// ED_ROOM_LOG=1 (testing): who asks the room loader (OSSendMessage to its
// queue, 0x8058AFDC) for a room, or calls the room change (0x80152298),
// with the return addresses up the stack.
static void room_log(CPUState* cpu, const char* what) {
    fprintf(stderr, "[room] %s 0x%08X 0x%08X (frame %llu) calls: 0x%08X", what, cpu->gpr[3], cpu->gpr[4],
            (unsigned long long)(cpu->timebase / 675000ull), cpu->lr);
    u32 sp = cpu->gpr[1];
    for (int f = 0; f < 14 && guest_ram_range_valid(cpu, sp, 8u); f++) {
        const u32 caller = mem_read32(cpu, sp);
        if (caller <= sp || !guest_ram_range_valid(cpu, caller, 8u))
            break;
        fprintf(stderr, " 0x%08X", mem_read32(cpu, caller + 4u));
        sp = caller;
    }
    fputc('\n', stderr);
}

static void notify_send_message(CPUState* cpu) {
    if (cpu->gpr[3] == 0x8058AFDCu)
        room_log(cpu, "request");
}

static void notify_room_change(CPUState* cpu) {
    room_log(cpu, "change");
}

// A new room asked for since the cutscene began (coop_cutscene_over).
static bool g_coop_room_asked;
static bool g_coop_room_asked_now;  // and for coop_menu_input
static bool g_coop_entered_p2;  // player 2 asked for the room: the first zone's scene there is theirs
static s32 g_coop_arrival_scene;
static unsigned long long g_trace_ticket_frame;  // ED_COOP_TRACE: both players' places, frame by frame, after this

static void notify_room_ticket(CPUState* cpu) {
    g_coop_room_asked = true;
    g_coop_room_asked_now = true;
    g_coop_entered_p2 = g_coop_p2 != 0u && mem_read32(cpu, cpu->gpr[13] - 21496u) == mem_read32(cpu, g_coop_p2 + 16u);
    g_coop_arrival_scene = 0;
    g_trace_ticket_frame = cpu->timebase / 675000ull;
    if (getenv("ED_ROOM_LOG") != NULL)
        room_log(cpu, "ticket");
}


// ED_COOP_TRACE=1 (play.sh): how each player's actions run, for building on them. Reads
// only. [trace] lines: scripts and who they run as, prompts, the look's stages, windows,
// the Tome's item page and the words it lays out.
static bool coop_trace(void) {
    static int on = -1;
    if (on < 0)
        on = getenv("ED_COOP_TRACE") != NULL;
    return on != 0;
}

static void trace_look(CPUState* cpu, const char* what) {
    const u32 look = 0x80307108u, entry = look + (mem_read32(cpu, look + 0x40u) & 1u) * 32u;
    fprintf(stderr, "[trace] %s r3 %d r4 %d lr 0x%08X who %u ctl %u | look stage %d script %d count %d words %d,%d | busy %d frame %llu\n", what,
            (s32)cpu->gpr[3], (s32)cpu->gpr[4], cpu->lr, mem_read32(cpu, cpu->gpr[13] - 21496u), mem_read32(cpu, cpu->gpr[13] - 21488u),
            (s32)mem_read32(cpu, entry), (s32)mem_read32(cpu, entry + 4u), (s32)mem_read32(cpu, entry + 8u), (s32)mem_read32(cpu, entry + 12u),
            (s32)mem_read32(cpu, entry + 16u), (s32)mem_read32(cpu, cpu->gpr[13] - 23344u), (unsigned long long)(cpu->timebase / 675000ull));
}
static void notify_trace_prompt_use(CPUState* cpu) { trace_look(cpu, "prompt use"); }
static void notify_trace_look_step(CPUState* cpu) { trace_look(cpu, "look step"); }
static void notify_trace_busy(CPUState* cpu) { trace_look(cpu, "busy"); }
static void notify_trace_prompt_set(CPUState* cpu) { trace_look(cpu, "prompt set"); }

static void notify_script_later(CPUState* cpu) {
    room_log(cpu, "later");
}

// While the stairs script runs: each game function it calls (0x8015974C's
// second argument names it; arguments follow on the script's stack).
static void notify_script_call(CPUState* cpu) {
    if (g_script_watch == 0u || !guest_ram_range_valid(cpu, cpu->gpr[4], 16u))
        return;
    g_script_watch--;
    fprintf(stderr, "[script] calls 0x%08X (%d args)\n", mem_read32(cpu, cpu->gpr[4]),
            (int)(s16)mem_read16(cpu, cpu->gpr[4] + 14u));
}

static void notify_script_run(CPUState* cpu) {
    if (cpu->lr == 0x80134B38u) {
        room_log(cpu, "trigger");
        g_script_watch = 200u;
    }
}

// ED_STREAM_LOG=1 (testing): each streamed read the game asks its reader for
// (0x80154118(offset, file entry, dest, size, ...)).
static void notify_stream_read(CPUState* cpu) {
    static unsigned shown;
    if (shown++ < 200u)
        fprintf(stderr, "[stream] offset 0x%08X file %d dest 0x%08X size 0x%X (lr 0x%08X)\n", cpu->gpr[3],
                (s32)cpu->gpr[4], cpu->gpr[5], cpu->gpr[6], cpu->lr);
}

// ED_ANIM_LOG=1 (testing): the loads that build characters' animation
// tables: a character's file (0x801318C0(model, flag)), a playable's
// (0x80132ED8(model)), each table of a file (0x80133028), and an object's
// holder set (0x80118F9C(object, holder)).
static void notify_anim_load(CPUState* cpu) {
    static unsigned shown;
    if (shown++ < 4000u)
        fprintf(stderr, "[anim] 0x%08X r3 0x%08X r4 0x%08X r5 0x%08X r6 0x%08X r7 0x%08X r8 0x%08X (lr 0x%08X)\n", cpu->pc,
                cpu->gpr[3], cpu->gpr[4], cpu->gpr[5], cpu->gpr[6], cpu->gpr[7], cpu->gpr[8], cpu->lr);
}

static void hle_chapter_character(CPUState* cpu) {
    static const u8* const kModel = kChapterModel;
    const u32 chapter = cpu->gpr[3];
    const int own = chapter < 17u ? kModel[chapter] : 9;
    const int want = player_model();
    g_chapter_model = own;
    if (want >= 0 && want != own) {
        static int told = -1;
        if (told != (int)chapter)
            fprintf(stderr, "[player] chapter %u's character %d played as %d\n", chapter, own, want);
        told = (int)chapter;
    }
    hle_set_u32(cpu, (u32)(want >= 0 ? want : own));
}

static struct {
    u32 caller;
    int keep, type;
} g_spawn_return;

// The player's file (moves, streamed as needed) is opened from the spawn
// record (0x80007844 → 0x80132ED8(model)) just before the record is made into
// the player. From Jump to Game the record already names the chosen character;
// a save's names the one it was saved with, and the chosen body then played
// the chapter character's moves (contorted). The same choice is made here.
#define PLAYER_FILE_CALL 0x80007848u

static void hle_player_file(CPUState* cpu) {
    g_hle_passthrough = true;
    if (cpu->lr != PLAYER_FILE_CALL || g_chapter_model < 0)
        return;
    const s32 model = (s16)cpu->gpr[3];
    const int want = player_model();
    const s32 now = want >= 0 && (model == g_chapter_model || player_type(model) >= 0) ? want
                    : want < 0 && player_type(model) >= 0                            ? g_chapter_model
                                                                                     : model;
    if (now != model) {
        fprintf(stderr, "[player] file %d opened for %d\n", now, model);
        cpu->gpr[3] = (u32)now;
    }
}

// The story's own change of the player's character (a script's command,
// 0x8000CA90: Anthony's rotting, 119 and on): the body from the chapter's
// list (0x80118A98, object +60), the file's character (0x80117C54, +580),
// the player's character (0x80131CF0, r13 - 23664), and the file opened
// again. Another chapter's chosen character keeps their own: Pius became
// rotting Anthony on the church's stairs. Each step left out when called
// from there (their return addresses).
#define STORY_BODY_SET 0x80118A98u
#define STORY_MODEL_SET 0x80117C54u
#define STORY_PLAYER_SET 0x80131CF0u

static void story_player_model(CPUState* cpu) {
    g_hle_passthrough = true;
    if (cpu->lr != 0x8000CB2Cu && cpu->lr != 0x8000CB38u && cpu->lr != 0x8000CB40u)
        return;
    const int want = player_model();
    if (want < 0 || g_chapter_model < 0 || want == g_chapter_model)
        return;
    if (cpu->pc == STORY_BODY_SET) {
        static u32 told;
        if (told != cpu->gpr[4])
            fprintf(stderr, "[player] the story's change of the player's body left out (%d is theirs)\n", want);
        told = cpu->gpr[4];
        cpu->gpr[3] = mem_read32(cpu, cpu->gpr[3] + 60u);  // as it answers: the body before
    }
    g_hle_passthrough = false;
}

static bool g_coop_spawning;  // coop_spawn is making player 2

// What a form's body has that the wearer's hasn't, in one mesh group, left out: all of its points in
// one place, so its triangles draw nothing. A body's points are 6 bytes each from its +80, its groups
// 276 bytes each from its +40 (+8 how many points, +10 the first). Everything else is as it was: what's
// equipped is another object, held by a bone.
static u32 body_part(CPUState* cpu, u32 body, u32 field) {
    const u32 at = mem_read32(cpu, body + field);
    return at < 0x80000000u ? body + at : at;
}

static void costume_strip(CPUState* cpu, const Costume* c, u32 body, u32 plain) {
    if (c->strip_group < 0 || !guest_ram_range_valid(cpu, body, 96u) || !guest_ram_range_valid(cpu, plain, 96u))
        return;
    const u32 groups = body_part(cpu, body, 40u);
    const u32 points = body_part(cpu, body, 80u), theirs = body_part(cpu, plain, 80u);
    const u32 total = (body_part(cpu, body, 52u) - points) / 6u, their_count = (body_part(cpu, plain, 52u) - theirs) / 6u;
    if (total > 0x10000u || !guest_ram_range_valid(cpu, points, total * 6u) || !guest_ram_range_valid(cpu, theirs, their_count * 6u))
        return;
    u8* own = cpu->ram + (points - GC_RAM_BASE);
    const u8* plain_points = cpu->ram + (theirs - GC_RAM_BASE);
    static u8 extra[0x10000];
    u32 left_out = 0u;
    for (u32 g = 0u; g < 32u; g++) {
        const u32 count = mem_read16(cpu, groups + g * 276u + 8u), first = mem_read16(cpu, groups + g * 276u + 10u);
        if (count == 0u || first + count > total)
            continue;
        for (u32 i = first; i < first + count; i++) {
            bool found = false;
            for (u32 k = 0u; k < their_count && !found; k++)
                found = memcmp(own + i * 6u, plain_points + k * 6u, 6u) == 0;
            extra[i] = !found;
        }
        // The weapon's group: all in one place. Another group has a stray point of it at most, and
        // that goes to the nearest of the group's own, so the triangles it joins draw nothing either.
        int all_to = -1;
        for (u32 i = first; i < first + count && all_to < 0; i++)
            if (!extra[i])
                all_to = (int)i;
        if (all_to < 0)
            continue;
        for (u32 i = first; i < first + count; i++) {
            if (!extra[i])
                continue;
            int to = all_to;
            if ((int)g != c->strip_group) {
                s64 best = -1;
                for (u32 k = first; k < first + count; k++) {
                    if (extra[k])
                        continue;
                    s64 apart = 0;
                    for (u32 axis = 0u; axis < 3u; axis++) {
                        const s64 d = (s64)(s16)((own[i * 6u + axis * 2u] << 8) | own[i * 6u + axis * 2u + 1u]) -
                                      (s64)(s16)((own[k * 6u + axis * 2u] << 8) | own[k * 6u + axis * 2u + 1u]);
                        apart += d * d;
                    }
                    if (best < 0 || apart < best) {
                        best = apart;
                        to = (int)k;
                    }
                }
            }
            memmove(own + i * 6u, own + (u32)to * 6u, 6u);
            left_out++;
        }
    }
    if (left_out != 0u)
        fprintf(stderr, "[player] %s's other body: %u points of a weapon left out\n", c->name, left_out);
}

// A character's body is taken from the chapter's list by their number as they're
// made, so what's filed under their number as a player is made is what they wear.
// Their own entry is kept, to put back for the other player or with their own form.
// `player` is who is being made as `model`; nothing for a character with no other body.
static void costume_file(CPUState* cpu, int model, int player) {
    static struct {
        u32 list, plain[2];
    } kept[COSTUME_PLACES];
    int place = -1;
    for (int i = 0; i < COSTUMES && place < 0; i++)
        if (kCostumes[i].wearer == model)
            place = kCostumes[i].place;
    if (place < 0)
        return;
    const Costume* worn = costume_on(player);
    if (worn != NULL && worn->wearer != model)
        worn = NULL;
    const u32 list = mem_read32(cpu, cpu->gpr[13] - 26452u);
    if (!guest_ram_range_valid(cpu, list, 8u) || mem_read32(cpu, list + 4u) != 0x6B5u || mem_read32(cpu, list) <= (u32)model)
        return;
    const u32 entry = list + 8u + (u32)model * 8u;
    // Their own: what's filed under them that is none of their forms.
    bool theirs = true;
    for (int i = 0; i < COSTUMES; i++)
        if (kCostumes[i].wearer == model && mem_read32(cpu, list) > (u32)kCostumes[i].filed &&
            mem_read32(cpu, list + 8u + (u32)kCostumes[i].filed * 8u) == mem_read32(cpu, entry))
            theirs = false;
    if (theirs && (kept[place].list != list || kept[place].plain[0] != mem_read32(cpu, entry))) {
        kept[place].list = list;
        kept[place].plain[0] = mem_read32(cpu, entry);
        kept[place].plain[1] = mem_read32(cpu, entry + 4u);
        g_costume_plain[place] = kept[place].plain[0];
    }
    if (worn != NULL && mem_read32(cpu, list) > (u32)worn->filed && mem_read32(cpu, list + 8u + (u32)worn->filed * 8u) != 0u) {
        const u32 costume = list + 8u + (u32)worn->filed * 8u;
        costume_strip(cpu, worn, mem_read32(cpu, costume), kept[place].plain[0]);
        if (mem_read32(cpu, mem_read32(cpu, costume) + 4u) != mem_read32(cpu, kept[place].plain[0] + 4u))
            costume_bone_map(cpu, worn, mem_read32(cpu, costume), kept[place].plain[0]);
        mem_write32(cpu, entry, mem_read32(cpu, costume));
        mem_write32(cpu, entry + 4u, mem_read32(cpu, costume + 4u));
    } else if (kept[place].list == list) {
        mem_write32(cpu, entry, kept[place].plain[0]);
        mem_write32(cpu, entry + 4u, kept[place].plain[1]);
    }
}

// The form an object wears: its body (+60) is that form's, as a player. The game's own Liche,
// in Alex's last chapter, keeps its moves: its number is 86, a player's Pius's 0.
static const Costume* costume_object(CPUState* cpu, u32 object) {
    const u32 list = mem_read32(cpu, cpu->gpr[13] - 26452u);
    if (!guest_ram_range_valid(cpu, list, 8u) || mem_read32(cpu, list + 4u) != 0x6B5u || !guest_ram_range_valid(cpu, object + 580u, 4u))
        return NULL;
    const int model = (s32)mem_read32(cpu, object + 580u);
    for (int i = 0; i < COSTUMES; i++)
        if (kCostumes[i].wearer == model && mem_read32(cpu, list) > (u32)kCostumes[i].filed) {
            const u32 body = mem_read32(cpu, list + 8u + (u32)kCostumes[i].filed * 8u);
            if (body != 0u && mem_read32(cpu, object + 60u) == body)
                return &kCostumes[i];
        }
    return NULL;
}

static const Costume* costume_worn(CPUState* cpu, u32 node) {
    const u32 object = node != 0u && guest_ram_range_valid(cpu, node + 52u, 4u) ? mem_read32(cpu, node + 52u) : 0u;
    return costume_object(cpu, object);
}

static void hle_spawn_create(CPUState* cpu) {
    const u32 rec = cpu->gpr[3];
    const int want = player_model();
    if (getenv("ED_BODY_LIST") != NULL && guest_ram_range_valid(cpu, rec, 48u)) {  // testing
        const u32 list = mem_read32(cpu, cpu->gpr[13] - 26452u);
        fprintf(stderr, "[player] making type %d model %d; bodies:", (s32)mem_read32(cpu, rec + 16u),
                (s32)mem_read32(cpu, rec + 20u));
        for (u32 i = 0; guest_ram_range_valid(cpu, list, 8u) && i < mem_read32(cpu, list) && i < 200u; i++)
            if (mem_read32(cpu, list + 8u + i * 8u) != 0u)
                fprintf(stderr, " %u(%08X,%u)", i, mem_read32(cpu, list + 8u + i * 8u), mem_read32(cpu, list + 12u + i * 8u));
        fprintf(stderr, "\n");
    }
    if (g_coop_spawning) {
        g_hle_passthrough = true;
        return;
    }
    // A co-op save has player 2 in it too: the second record of the
    // chapter's kind is theirs, and is made as player 2's character (it was
    // made as player 1's: Pius twice).
    if (g_chapter_model >= 0 && guest_ram_range_valid(cpu, rec, 48u) &&
        (s32)mem_read32(cpu, rec + 16u) == player_type(g_chapter_model) && g_playables_made++ > 0 &&
        getenv("ED_COOP_SAVE_P2_OFF") == NULL) {  // testing: without
        const int p2 = coop_wanted_character();
        const int model = p2 >= 0 ? p2 : want >= 0 ? want : g_chapter_model;
        fprintf(stderr, "[player] player 2 from the save (saved as %d) made as %d\n", (s32)mem_read32(cpu, rec + 20u),
                model);
        mem_write32(cpu, rec + 20u, (u32)model);
        costume_file(cpu, model, 1);
        g_hle_passthrough = true;
        return;
    }
    // A save made while another character was played remembers them: the
    // player's record has the chapter's kind but that character's number.
    // The chapter's own is put back (and the chosen one, if any, below).
    if (g_chapter_model >= 0 && guest_ram_range_valid(cpu, rec, 48u)) {
        const s32 saved = (s32)mem_read32(cpu, rec + 20u);
        if (saved != g_chapter_model && player_type(saved) >= 0 &&
            (s32)mem_read32(cpu, rec + 16u) == player_type(g_chapter_model)) {
            mem_write32(cpu, rec + 20u, (u32)g_chapter_model);
            fprintf(stderr, "[player] saved as %d, back to the chapter's %d\n", saved, g_chapter_model);
        }
    }
    if (want >= 0 && g_chapter_model >= 0 && guest_ram_range_valid(cpu, rec, 48u) &&
        ((s32)mem_read32(cpu, rec + 20u) == g_chapter_model || (s32)mem_read32(cpu, rec + 20u) == want)) {
        mem_write32(cpu, rec + 20u, (u32)want);
        // The bodies the chapter loaded, by character (a list at r13 - 26452:
        // count, 0x6B5, then 8 bytes each; 0x801DCED0 reads it): the body
        // loaded for the chapter's character is filed under theirs, so the
        // player's object (0x80007070) found none for another one.
        const u32 list = mem_read32(cpu, cpu->gpr[13] - 26452u);
        if (guest_ram_range_valid(cpu, list, 8u) && mem_read32(cpu, list + 4u) == 0x6B5u &&
            mem_read32(cpu, list) > (u32)want && mem_read32(cpu, list) > (u32)g_chapter_model &&
            mem_read32(cpu, list + 8u + (u32)want * 8u) == 0u) {
            for (u32 i = 0; i < 8u; i += 4u)
                mem_write32(cpu, list + 8u + (u32)want * 8u + i,
                            mem_read32(cpu, list + 8u + (u32)g_chapter_model * 8u + i));
            fprintf(stderr, "[player] body of %d filed under %d (0x%08X)\n", g_chapter_model, want,
                    mem_read32(cpu, list + 8u + (u32)want * 8u));
        }
        const char* type = getenv("ED_PLAYER_TYPE");  // testing: another kind
        const int kind = type != NULL ? atoi(type) : -1;  // the chapter's own kind: its scripts ask for it
        if (kind >= 0)
            mem_write32(cpu, rec + 16u, (u32)kind);
        fprintf(stderr, "[player] made as %d (the chapter's %d)\n", want, g_chapter_model);
        costume_file(cpu, want, 0);
        if (costume_on(0) != NULL)
            fprintf(stderr, "[player] player 1 wears %s's other body\n", costume_on(0)->name);
        // TEST ED_PLAYER_KEEP=1 kind, 2 model, 3 both: the made character
        // given back the chapter's character's.
        if (getenv("ED_PLAYER_KEEP") != NULL && g_spawn_return.caller == 0u) {
            g_spawn_return.caller = cpu->lr;
            g_spawn_return.keep = atoi(getenv("ED_PLAYER_KEEP"));
            g_spawn_return.type = player_type(g_chapter_model);
            cpu->lr = HLE_SPAWN_RETURN;
        }
    }
    g_hle_passthrough = true;
}

static bool continue_spawn_create(CPUState* cpu) {
    if (g_spawn_return.caller == 0u)
        return false;
    const u32 made = cpu->gpr[3];
    const u32 actor = guest_ram_range_valid(cpu, made + (u32)(s32)g_coop_actor_offset, 4u)
                          ? mem_read32(cpu, made + (u32)(s32)g_coop_actor_offset) : 0u;
    if (g_coop_actor_offset != 0 && guest_ram_range_valid(cpu, actor, 160u)) {
        fprintf(stderr, "[player] TEST made 0x%08X actor 0x%08X kind %d model %d -> keep %d\n", made, actor,
                (s16)mem_read16(cpu, actor + 152u), (s16)mem_read16(cpu, actor + 154u), g_spawn_return.keep);
        if (g_spawn_return.keep & 1)
            mem_write16(cpu, actor + 152u, (u16)g_spawn_return.type);
        if (g_spawn_return.keep & 2)
            mem_write16(cpu, actor + 154u, (u16)g_chapter_model);
    }
    cpu->lr = cpu->pc = g_spawn_return.caller;
    g_spawn_return.caller = 0u;
    return true;
}

static void coop_spawn(CPUState* cpu, u32 player) {
    const u32 actor = mem_read32(cpu, player + (u32)(s32)g_coop_actor_offset);
    const u32 object = mem_read32(cpu, player + 52u);
    if (!guest_ram_range_valid(cpu, actor, 160u) || !guest_ram_range_valid(cpu, object, 64u))
        return;
    const u32 rec = (cpu->gpr[1] - 0x200u) & ~15u;
    u32 made = 0u, ignore = 0u;
    float at[3];
    coop_place(cpu, object, at);
    if (!hle_guest_call(cpu, COOP_SPAWN_INIT, &rec, 1, 0.0, &ignore))
        return;
    for (u32 i = 0; i < 3u; i++)
        guest_write_f32(cpu, rec + i * 4u, at[i]);
    mem_write32(cpu, rec + 16u, mem_read16(cpu, actor + 152u));
    mem_write32(cpu, rec + 20u, (u32)(s32)(s16)mem_read16(cpu, actor + 154u));
    // Player 2 as another character (coop_character_load has loaded them).
    const int model = coop_wanted_character();
    if (model >= 0)
        mem_write32(cpu, rec + 20u, (u32)model);
    mem_write32(cpu, rec + 40u, mem_read32(cpu, player + 8u));
    const int made_as = (s32)mem_read32(cpu, rec + 20u);
    costume_file(cpu, made_as, 1);
    if (costume_on(1) != NULL && costume_on(1)->wearer == made_as)
        fprintf(stderr, "[player] player 2 wears %s's other body\n", costume_on(1)->name);
    g_coop_spawning = true;
    const bool created = hle_guest_call(cpu, COOP_SPAWN_CREATE, &rec, 1, 0.0, &made);
    g_coop_spawning = false;
    costume_file(cpu, made_as, 0);
    if (!created || made == 0u) {
        fprintf(stderr, "[coop] the game made no player 2\n");
        return;
    }
    const u32 room_args[] = {made, mem_read32(cpu, cpu->gpr[13] + (u32)(s32)g_coop_room_offset)};
    const u32 add_args[] = {1u, made};
    hle_guest_call(cpu, COOP_SPAWN_SET_ROOM, room_args, 2, 0.0, &ignore);
    hle_guest_call(cpu, COOP_SPAWN_REGISTER, &made, 1, 0.0, &ignore);
    hle_guest_call(cpu, COOP_SPAWN_ADD, add_args, 2, 0.0, &ignore);
    g_coop_p2 = made;
    coop_describe(cpu, "player 1", player);
    coop_describe(cpu, coop_listed(cpu, made) ? "player 2 (listed)" : "player 2 (not in the list)", made);
}

// Player 2's turn. The game controls the player each frame from the
// per-character update (0x80007490): the player controller (0x80045BC4,
// fn(node)) pumps the game's input channels, which turn the pad into the
// player's control block (its intent: heading, speed, walk or run; 0x805D7BA0,
// the data at r13 - 25660), and sends the player's character events with that
// block (0x801F59B0(event, from, to, data)), which its behaviour carries out.
// The pad itself is read and processed once a frame after the game's frame
// (0x8013E518, fn(): PADRead into 0x805880A0, then the input channels).
//
// Each time the game is about to control player 1, player 2 has a turn first:
// player 1's input buffers, control blocks and the controller's globals are
// put aside and player 2's put in their place (the first time, player 1's as
// they are); the pad is read and processed with PADRead answering with player
// 2's controller; the controller runs for player 2's character; then player
// 2's state is kept for their next turn and player 1's put back.
#define COOP_CONTROLLER 0x80045BC4u
#define COOP_PAD_UPDATE 0x8013E518u
#define COOP_INPUT 0x805880A0u       // raw pads, processed pads, the input channels
#define COOP_INPUT_SIZE 816u
#define COOP_INPUT_MASKS 0xE0u
#define COOP_CONTROL 0x805D7900u     // the control blocks (0x805D7BA0 among them)
#define COOP_CONTROL_SIZE 0x300u
#define COOP_AIM_SIZE 208u           // the block channel 3 drives (its +12)
// The game camera the controller reads at its start (0x801EAFA8 copies it):
// its eye, then what it looks at. Forward is from one to the other, so in
// player 2's turn it is player 2's camera.
#define COOP_CAMERA 0x803012E0u

// What the player is next to (a door, a thing to look at or pick up): the
// scan (0x80134A44, from each character's step, 0x80133434, for the player
// only) records who and what at r13 - 23196 and - 23192, with the prompt's
// and its state's words beside; acting on it reads them. Each player has
// their own.
static const s32 kCoopInteract[5] = {-29016, -24032, -23196, -23192, -21996};

typedef struct {
    u8 input[COOP_INPUT_SIZE];
    u8 control[COOP_CONTROL_SIZE];
    u8 aim[COOP_AIM_SIZE];
    u8 globals[48];       // r13 - 25680 .. - 25632: the controller's state
    u8 pad_globals[56];   // r13 - 23604 .. - 23548: the pad routine's
    u32 interact[5];      // kCoopInteract
} CoopPlayerState;

static void coop_interact(CPUState* cpu, u32 out[5], bool save) {
    for (int i = 0; i < 5; i++) {
        const u32 at = cpu->gpr[13] + (u32)kCoopInteract[i];
        if (save)
            out[i] = mem_read32(cpu, at);
        else
            mem_write32(cpu, at, out[i]);
    }
}

static CoopPlayerState g_coop_state[2];
// Player 2 is the game's player during their turn (ED_COOP_P2_NOT_PLAYER=1,
// testing: not).
static bool g_coop_p2_is_player = true;
// The game's look round the player for doors and things (0x80134A44) and the
// character info it takes its arguments from (0x80123F70), for player 2's
// turn; found with the characters' step (find_coop).
#define COOP_SCAN 0x80134A44u
#define COOP_CHARACTER_INFO 0x80123F70u
static bool g_coop_scan;
static bool g_coop_p2_state;  // player 2's has been kept since they were made
static bool g_coop_turn;      // player 2's turn is running

static void coop_bytes(CPUState* cpu, u32 address, u8* buffer, u32 size, bool save) {
    if (!guest_ram_range_valid(cpu, address, size))
        return;
    u8* guest = cpu->ram + (address - GC_RAM_BASE);
    if (save)
        memcpy(buffer, guest, size);
    else
        memcpy(guest, buffer, size);
}

// The words round the prompt's record (r13 - 24040 .. - 21980), put back
// after player 2's step and look, all but the sound's among them (MusyX's,
// r13 - 22920 .. - 22396: its voices' allocation). Put back, each sound
// player 2's step started (their footsteps) kept its voice for good, until
// none was left and the next two-part sound (the Tome's cutscene) had the
// same voice twice and hung.
#define COOP_WORDS 24040u
#define COOP_WORDS_SIZE 2060u
#define COOP_SOUND_WORDS (COOP_WORDS - 22920u)
#define COOP_SOUND_WORDS_END (COOP_WORDS - 22396u)

static void coop_words_back(CPUState* cpu, u8* words) {
    const u32 at = cpu->gpr[13] - COOP_WORDS;
    coop_bytes(cpu, at, words, COOP_SOUND_WORDS, false);
    coop_bytes(cpu, at + COOP_SOUND_WORDS_END, words + COOP_SOUND_WORDS_END, COOP_WORDS_SIZE - COOP_SOUND_WORDS_END,
               false);
}

// Each player has their own count of each kind of thing (the Chakrams
// left to throw, the Talisman's uses): a kind's record (a thing's record,
// node +44, names it at +4; 88 bytes) keeps it at +0x44, one for every
// thing of that kind, and player 1's throws took from player 2's. Player
// 2's copies (coop_weapon_copy) each have a kind of their own, a copy of
// the original's in the scratch (SCRATCH_KINDS, the original's address
// after them; coop_copy_own_kind): a throw is taken from the thrower's.
// Other things' counts (the Talisman's) are kept here by the kind's mark
// (+0x4C, a word of its own to each kind). While player 2's Tome is open,
// it shows and uses theirs; a kind new to them starts with player 1's
// count.
#define THING_KIND_RECORD 4u
#define THING_KIND_SIZE 88u
#define THING_KIND_COUNT 0x44u
#define THING_KIND_MARK 0x4Cu
#define SCRATCH_KINDS 0x81FF0000u  // past player 2's body (SCRATCH_BODY2), THING_KIND_SIZE each
#define SCRATCH_KINDS_FROM (SCRATCH_KINDS + 32u * THING_KIND_SIZE)  // each one's original, a word each
#define COOP_KINDS 96
typedef struct {
    u32 mark;
    u16 count, used;
} CoopKind;
static CoopKind g_coop_kinds[COOP_KINDS];
static struct {
    u32 kind;
    u16 p1;
    u32 own;  // player 2's copy's kind, or 0 (theirs is kept in g_coop_kinds)
    bool bare;  // no copy yet, and player 1's enchantment put by
    u32 enchant[3];  // player 1's enchantment, while their copy's is in
} g_coop_kinds_in[COOP_KINDS];  // the game's kinds while player 2's are in, and player 1's counts
static int g_coop_kinds_in_count, g_coop_kinds_depth;
// A kind's enchantment: its flags, the spell's effect and the alignment's
// bytes. Each player's sword is enchanted on its own: while player 2's Tome
// is open, the original (which it shows) has that of their copy.
static const u32 kCoopEnchant[3] = {0x04u, 0x1Cu, 0x50u};

static bool coop_own_kind(u32 kind) {
    return kind >= SCRATCH_KINDS && kind < SCRATCH_KINDS + 32u * THING_KIND_SIZE;
}

static CoopKind* coop_kind(u32 mark, bool make) {
    for (int i = 0; i < COOP_KINDS; i++)
        if (g_coop_kinds[i].used && g_coop_kinds[i].mark == mark)
            return &g_coop_kinds[i];
    for (int i = 0; make && i < COOP_KINDS; i++)
        if (!g_coop_kinds[i].used)
            return &g_coop_kinds[i];
    return NULL;
}

static u32 coop_next(CPUState* cpu, u32 node);
static bool coop_listed(CPUState* cpu, u32 wanted);
static void coop_kinds(CPUState* cpu, bool p2) {
    if (p2 ? g_coop_kinds_depth++ > 0 : g_coop_kinds_depth == 0 || --g_coop_kinds_depth > 0)
        return;
    if (!p2) {
        for (int i = 0; i < g_coop_kinds_in_count; i++) {
            const u32 kind = g_coop_kinds_in[i].kind, count = mem_read16(cpu, kind + THING_KIND_COUNT);
            CoopKind* own = coop_kind(mem_read32(cpu, kind + THING_KIND_MARK), false);
            if (g_coop_kinds_in[i].own != 0u)
                mem_write16(cpu, g_coop_kinds_in[i].own + THING_KIND_COUNT, (u16)count);
            else if (own != NULL)
                own->count = (u16)count;
            mem_write16(cpu, kind + THING_KIND_COUNT, g_coop_kinds_in[i].p1);
            for (int w = 0; g_coop_kinds_in[i].own != 0u && w < 3; w++) {
                mem_write32(cpu, g_coop_kinds_in[i].own + kCoopEnchant[w], mem_read32(cpu, kind + kCoopEnchant[w]));
                mem_write32(cpu, kind + kCoopEnchant[w], g_coop_kinds_in[i].enchant[w]);
            }
            for (int w = 1; g_coop_kinds_in[i].bare && w < 3; w++)
                mem_write32(cpu, kind + kCoopEnchant[w], g_coop_kinds_in[i].enchant[w]);
        }
        g_coop_kinds_in_count = 0;
        return;
    }
    int n = 0;
    for (u32 node = 0u, k = 0u; k < 1024u && n < COOP_KINDS && (node = coop_next(cpu, node)) != 0u; k++) {
        const u32 record = mem_read32(cpu, node + 44u);
        const u32 kind = guest_ram_range_valid(cpu, record, 8u) ? mem_read32(cpu, record + THING_KIND_RECORD) : 0u;
        if (coop_own_kind(kind) || !guest_ram_range_valid(cpu, kind, THING_KIND_SIZE) ||
            mem_read32(cpu, kind + THING_KIND_MARK) == 0u)
            continue;
        bool seen = false;
        for (int i = 0; i < n && !seen; i++)
            seen = g_coop_kinds_in[i].kind == kind;
        if (seen)
            continue;
        u32 copy_kind = 0u;
        for (u32 c = 0u; c < 32u && copy_kind == 0u; c++)
            if (mem_read32(cpu, SCRATCH_KINDS_FROM + c * 4u) == kind)
                copy_kind = SCRATCH_KINDS + c * THING_KIND_SIZE;
        CoopKind* own = copy_kind != 0u ? NULL : coop_kind(mem_read32(cpu, kind + THING_KIND_MARK), true);
        if (copy_kind == 0u && own == NULL)
            continue;
        const u16 count = mem_read16(cpu, kind + THING_KIND_COUNT);
        if (own != NULL && !own->used) {
            own->used = 1u;
            own->mark = mem_read32(cpu, kind + THING_KIND_MARK);
            own->count = count;
        }
        g_coop_kinds_in[n].kind = kind;
        g_coop_kinds_in[n].p1 = count;
        g_coop_kinds_in[n].own = copy_kind;
        for (int w = 0; copy_kind != 0u && w < 3; w++) {
            g_coop_kinds_in[n].enchant[w] = mem_read32(cpu, kind + kCoopEnchant[w]);
            mem_write32(cpu, kind + kCoopEnchant[w], mem_read32(cpu, copy_kind + kCoopEnchant[w]));
        }
        // No copy of theirs yet: what they would have is not enchanted (as coop_copy_own_kind).
        g_coop_kinds_in[n].bare = copy_kind == 0u && mem_read32(cpu, kind + kCoopEnchant[1]) != 0u;
        if (g_coop_kinds_in[n].bare) {
            g_coop_kinds_in[n].enchant[1] = mem_read32(cpu, kind + kCoopEnchant[1]);
            g_coop_kinds_in[n].enchant[2] = mem_read32(cpu, kind + kCoopEnchant[2]);
            mem_write32(cpu, kind + kCoopEnchant[1], 0u);
            mem_write32(cpu, kind + kCoopEnchant[2], g_coop_kinds_in[n].enchant[2] & 0xFF0000FFu);
        }
        n++;
        mem_write16(cpu, kind + THING_KIND_COUNT, copy_kind != 0u ? mem_read16(cpu, copy_kind + THING_KIND_COUNT) : own->count);
    }
    g_coop_kinds_in_count = n;
}

// Player 2's copy in slot i (g_coop_copies) given a kind of its own, a copy
// of its original's as it is (player 1's count, to start).
static void coop_copy_own_kind(CPUState* cpu, int i, u32 copy) {
    const u32 record = mem_read32(cpu, copy + 44u);
    const u32 kind = guest_ram_range_valid(cpu, record, 8u) ? mem_read32(cpu, record + THING_KIND_RECORD) : 0u;
    if (coop_own_kind(kind) || !guest_ram_range_valid(cpu, kind, THING_KIND_SIZE) || g_coop_kinds_depth > 0)
        return;
    const u32 own = SCRATCH_KINDS + (u32)i * THING_KIND_SIZE;
    for (u32 at = 0u; at < THING_KIND_SIZE; at += 4u)
        mem_write32(cpu, own + at, mem_read32(cpu, kind + at));
    // Player 1's enchantment stays theirs (a sword's, as enchanting it changed).
    if (mem_read32(cpu, own + kCoopEnchant[1]) != 0u) {
        mem_write32(cpu, own + kCoopEnchant[0], mem_read32(cpu, own + kCoopEnchant[0]) | 4u);
        mem_write32(cpu, own + kCoopEnchant[1], 0u);
        mem_write32(cpu, own + kCoopEnchant[2], mem_read32(cpu, own + kCoopEnchant[2]) & 0xFF0000FFu);
    }
    mem_write32(cpu, SCRATCH_KINDS_FROM + (u32)i * 4u, kind);
    mem_write32(cpu, record + THING_KIND_RECORD, own);
    fprintf(stderr, "[coop] player 2's copy (node 0x%08X) counts on its own (kind 0x%08X from 0x%08X, %u)\n", copy, own,
            kind, mem_read16(cpu, own + THING_KIND_COUNT));
}

// Quick state: player 2's counts (states saved before them start them
// with player 1's).
size_t hle_coop_kinds_size(void) {
    return sizeof g_coop_kinds;
}

void hle_coop_kinds_save(void* out) {
    memcpy(out, g_coop_kinds, sizeof g_coop_kinds);
}

void hle_coop_kinds_load(const void* in) {
    memset(g_coop_kinds, 0, sizeof g_coop_kinds);
    if (in != NULL)
        memcpy(g_coop_kinds, in, sizeof g_coop_kinds);
    g_coop_kinds_in_count = g_coop_kinds_depth = 0;
}

static void coop_state(CPUState* cpu, CoopPlayerState* s, bool save) {
    const u32 aim = mem_read32(cpu, COOP_INPUT + 176u + 3u * 32u + 12u);  // channel 3's context
    coop_bytes(cpu, COOP_INPUT, s->input, COOP_INPUT_SIZE, save);
    coop_bytes(cpu, COOP_CONTROL, s->control, COOP_CONTROL_SIZE, save);
    if (aim != 0u)
        coop_bytes(cpu, aim, s->aim, COOP_AIM_SIZE, save);
    coop_bytes(cpu, cpu->gpr[13] - 25680u, s->globals, sizeof s->globals, save);
    coop_bytes(cpu, cpu->gpr[13] - 23604u, s->pad_globals, sizeof s->pad_globals, save);
    coop_interact(cpu, s->interact, save);
}

static void coop_copies_follow(CPUState* cpu);
static void coop_restore_held(CPUState* cpu);
static void coop_rehold(CPUState* cpu);

// The player's look for threats round them (0x800CBE7C(node): the threat
// search, 0x80032174, every 16th frame), and where it keeps what it found
// (r13 - 25008, - 25004); each player has their own.
#define COOP_THREAT_LOOK 0x800CBE7Cu
static const s32 COOP_THREAT[2] = {-25008, -25004};
static u32 g_coop_threat[2];
static u32 g_coop_p1_id = ~0u;  // player 1's id, as it was before player 2's turn (coop_send_event)

static u32 g_coop_own_carry;  // player 2's own list, while their quick spell has player 1's

// Player 2's quick spells. The game casts the player's from its input:
// Y and the D-pad's bindings call 0x800069DC with the address of one of the
// five quick spells (the magick record's 0x80334DE8..), which, when the player
// may cast (event 185), casts it for the player (0x801C4250): the player's
// magick pays for it and their bar shows it. Player 2's presses cast theirs in
// their turn, with them the player, from five quick spells of their own (none
// at first; their Tome's Quick Spell page sets them: they are the game's
// while their Tome is open, coop_quick_swap). ED_COOP_CAST_LOG says each
// cast.
#define COOP_NODE_STATE 24u
#define COOP_QUICK 0x80334DE8u
#define COOP_MAGICK_RECORD 0x80334D50u
#define COOP_QUICKS 5u
#define COOP_QUICK_CAST 0x800069DCu
#define COOP_QUICK_SLOT (SCRATCH_HUD + 0xC00u)  // the one cast, for the handler to read
static const u16 kCoopQuickButton[COOP_QUICKS] = {0x0800u, 0x0008u, 0x0004u, 0x0001u, 0x0002u};  // Y, up, down, left, right
static u32 g_coop_quick[COOP_QUICKS];  // player 2's
static u32 g_coop_quick_p1[COOP_QUICKS];  // player 1's, while player 2's Tome has the game's
// A spell cast on an item (Enchant): the record at 0x80335748 has the item's
// id (+0) and, with the Tome asked to open to choose one, +16's bit 0. A cast
// asks how many things the caster carries first (0x801F98A4, their list's
// +8); none is "no items". Both players have the same things, and player 2's
// own list is empty, so theirs is player 1's for the cast. The Tome it opens
// is theirs (coop_menu_input).
#define COOP_SPELL_ITEM 0x80335748u
#define COOP_SPELL_ITEM_TOME 16u
#define CARRY_LIST 48u
static bool g_coop_quick_tome;
static u32 g_coop_turn_p1;                // player 1's node, while player 2 is the player in their turn

// What the controlled character is casting: the spell (r13 - 22336, which
// the cast sets and asks first: one being cast, no other is begun) and the
// words its spells keep with it (- 22320, - 22316, - 22312). Each player has
// their own, player 2's kept here while they are not casting, so the players
// cast at the same time.
static const u32 kCoopCasting[4] = {22336u, 22320u, 22316u, 22312u};
static u32 g_coop_casting[4];

// Player 2 the player, the controlled character and the one casting (on),
// or back (off).
static void coop_be_player2(CPUState* cpu, bool on, u32 was[5]) {
    const u32 at[5] = {cpu->gpr[13] - 26524u, cpu->gpr[13] - 21492u, cpu->gpr[13] - 21496u, cpu->gpr[13] - 21488u,
                       cpu->gpr[13] - 21484u};
    const u32 id = mem_read32(cpu, g_coop_p2 + 16u);
    const u32 now[5] = {mem_read32(cpu, g_coop_p2 + 52u), g_coop_p2, id, id, g_coop_p2};
    for (int i = 0; i < 5; i++) {
        if (on) {
            was[i] = mem_read32(cpu, at[i]);
            mem_write32(cpu, at[i], now[i]);
        } else {
            mem_write32(cpu, at[i], was[i]);
        }
    }
    for (int i = 0; i < 4; i++) {
        const u32 casting = cpu->gpr[13] - kCoopCasting[i], theirs = g_coop_casting[i];
        g_coop_casting[i] = mem_read32(cpu, casting);
        mem_write32(cpu, casting, theirs);
    }
}

// Player 2's quick spells the game's (their Tome opens) or player 1's back
// (it closes), player 2's kept as their Tome left them.
// With ED_COOP_TRACE (only reads): each word of the magick record that changes, for tracing the
// Magick page: what a spell made writes, and what's lost with player 2's copy.
#define COOP_MAGICK_SIZE 0xD0u
static void magick_trace(CPUState* cpu) {
    static u32 was[COOP_MAGICK_SIZE / 4u];
    static bool have;
    if (!coop_trace())
        return;
    for (u32 i = 0u; i < COOP_MAGICK_SIZE / 4u; i++) {
        const u32 now = mem_read32(cpu, COOP_MAGICK_RECORD + i * 4u);
        if (have && now != was[i])
            fprintf(stderr, "[magick] +0x%02X 0x%08X -> 0x%08X (frame %llu, Tome owner %d)\n", i * 4u, was[i], now, (unsigned long long)(cpu->timebase / 675000ull), g_coop_tome_owner);
        was[i] = now;
    }
    have = true;
    // And the story's flags (r13 - 26528: the count, then the bits), for what a thing found sets.
    static u32 flags_was[256];
    static bool flags_have;
    const u32 set = mem_read32(cpu, cpu->gpr[13] - 26528u);
    if (!guest_ram_range_valid(cpu, set, 4u))
        return;
    const u32 count = mem_read32(cpu, set);
    const u32 words = (count + 31u) / 32u;
    if (words > 256u || !guest_ram_range_valid(cpu, set + 4u, words * 4u))
        return;
    for (u32 i = 0u; i < words; i++) {
        const u32 now = mem_read32(cpu, set + 4u + i * 4u);
        for (u32 bit = 0u; flags_have && now != flags_was[i] && bit < 32u; bit++)
            if (((now ^ flags_was[i]) >> bit & 1u) != 0u)
                fprintf(stderr, "[flag] %u %s (frame %llu)\n", i * 32u + bit, (now >> bit & 1u) != 0u ? "on" : "off", (unsigned long long)(cpu->timebase / 675000ull));
        flags_was[i] = now;
    }
    flags_have = true;
}

// Each player's spell list is their own: a spell one makes on the Magick page isn't the other's.
// The record's runes and circles (+0, +4) stay one for both. Its spells (+8) and chosen circles
// (+0x38) are kept here for player 2 and put in their Tome's copy as it opens. A spell with its name
// known (0x08000000: not one made by hand, 0x04000000) goes to them from the game's record, for one
// found. Runes the game no longer has: another game, so their list starts again.
#define COOP_SPELL_WORDS 24u
static u32 g_coop_spells[COOP_SPELL_WORDS], g_coop_spells_runes;
static bool g_coop_spells_have;
static void coop_spells_in(CPUState* cpu) {
    const u32 runes = mem_read32(cpu, COOP_MAGICK_RECORD);
    if (g_coop_spells_have && (runes & g_coop_spells_runes) != g_coop_spells_runes)
        g_coop_spells_have = false;
    if (!g_coop_spells_have)
        memset(g_coop_spells, 0, sizeof g_coop_spells);
    for (u32 i = 0u; i < 12u; i++) {
        const u32 spell = COOP_MAGICK_RECORD + 8u + i * 4u, circle = spell + 0x30u;
        if ((mem_read32(cpu, spell) & 0x08000000u) != 0u) {
            g_coop_spells[i] = mem_read32(cpu, spell);
            if (g_coop_spells[12u + i] == 0u)
                g_coop_spells[12u + i] = mem_read32(cpu, circle);
        }
        mem_write32(cpu, spell, g_coop_spells[i]);
        mem_write32(cpu, circle, g_coop_spells[12u + i]);
    }
    g_coop_spells_have = true;
    g_coop_spells_runes = runes;
    if (coop_trace())
        fprintf(stderr, "[magick] player 2's spells: %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X\n", g_coop_spells[0], g_coop_spells[1], g_coop_spells[2], g_coop_spells[3], g_coop_spells[4], g_coop_spells[5],
                g_coop_spells[6], g_coop_spells[7], g_coop_spells[8], g_coop_spells[9], g_coop_spells[10], g_coop_spells[11]);
}

static void coop_quick_swap(CPUState* cpu, bool p2) {
    for (u32 i = 0; i < COOP_QUICKS; i++) {
        const u32 at = COOP_QUICK + i * 4u;
        if (p2) {
            g_coop_quick_p1[i] = mem_read32(cpu, at);
            mem_write32(cpu, at, g_coop_quick[i]);
        } else {
            g_coop_quick[i] = mem_read32(cpu, at);
            mem_write32(cpu, at, g_coop_quick_p1[i]);
        }
    }
    if (getenv("ED_COOP_CAST_LOG") != NULL && !hle_coop_tome_shares())
        fprintf(stderr, "[coop] quick spells: player 2's %08X %08X %08X %08X %08X, player 1's %08X %08X %08X %08X %08X (%s)\n",
                g_coop_quick[0], g_coop_quick[1], g_coop_quick[2], g_coop_quick[3], g_coop_quick[4], g_coop_quick_p1[0],
                g_coop_quick_p1[1], g_coop_quick_p1[2], g_coop_quick_p1[3], g_coop_quick_p1[4],
                p2 ? "player 2's Tome opens" : "it closes");
}

// In player 2's turn, with them the player: a quick spell's button pressed
// casts theirs. The cast pays from the character the game controls (r13 -
// 21488 its id, - 21484 its node; 0x801F6258, 0x801F6250), which is player 2
// for their cast too, with what they are casting (coop_be_player2).
static void coop_quick_cast(CPUState* cpu) {
    static u16 held;
    const u16 pressed = (u16)(g_coop_p2_buttons & ~held);
    held = g_coop_p2_buttons;
    if (pressed == 0u || cpu->ram_size < SCRATCH_END - GC_RAM_BASE)
        return;
    // ED_COOP_P2_QUICK=spell (testing): player 2's D-pad up casts that spell.
    const char* test = getenv("ED_COOP_P2_QUICK");
    if (test != NULL)
        g_coop_quick[1] = (u32)strtoul(test, NULL, 16);
    for (u32 i = 0; i < COOP_QUICKS; i++) {
        if ((pressed & kCoopQuickButton[i]) == 0u)
            continue;
        mem_write32(cpu, COOP_QUICK_SLOT, g_coop_quick[i]);
        const u32 args[2] = {0u, COOP_QUICK_SLOT};
        const u32 first = mem_read32(cpu, cpu->gpr[13] - 22328u);
        u32 was[5], ignore = 0u;
        const u32 carried = mem_read32(cpu, g_coop_p2 + CARRY_LIST);
        g_coop_own_carry = carried;
        const u32 asked = mem_read32(cpu, COOP_SPELL_ITEM + COOP_SPELL_ITEM_TOME) & 1u;
        u8 spell_was[24];
        coop_bytes(cpu, COOP_SPELL_ITEM, spell_was, sizeof spell_was, true);
        const u32 mode_asked_was = mem_read32(cpu, cpu->gpr[13] - 26188u), mode_now_was = mem_read32(cpu, cpu->gpr[13] - 26184u);
        // Player 1's cast has the record, asking of their Tome: theirs is asked apart from it.
        const bool beside = tome_copy_mode() == 2 && asked != 0u;
        if (beside)
            mem_write32(cpu, COOP_SPELL_ITEM + COOP_SPELL_ITEM_TOME, mem_read32(cpu, COOP_SPELL_ITEM + COOP_SPELL_ITEM_TOME) & ~1u);
        if (g_coop_turn_p1 != 0u)
            mem_write32(cpu, g_coop_p2 + CARRY_LIST, mem_read32(cpu, g_coop_turn_p1 + CARRY_LIST));
        coop_be_player2(cpu, true, was);
        hle_guest_call(cpu, COOP_QUICK_CAST, args, 2, 0.0, &ignore);
        coop_be_player2(cpu, false, was);
        mem_write32(cpu, g_coop_p2 + CARRY_LIST, carried);
        g_coop_own_carry = 0u;
        if ((!asked || beside) && (mem_read32(cpu, COOP_SPELL_ITEM + COOP_SPELL_ITEM_TOME) & 1u) != 0u) {
            if (tome_copy_mode() == 2) {
                // The Tome it asks for is their copy's: the real record and mode are as they were,
                // whatever player 1's Tome or cast is doing with them.
                coop_bytes(cpu, COOP_SPELL_ITEM, g_tome_copy_spell, sizeof g_tome_copy_spell, true);
                coop_bytes(cpu, COOP_SPELL_ITEM, spell_was, sizeof spell_was, false);
                mem_write32(cpu, cpu->gpr[13] - 26188u, mode_asked_was);
                mem_write32(cpu, cpu->gpr[13] - 26184u, mode_now_was);
                g_tome_copy_spell_set = true;
                g_tome_copy_ask = true;
                fprintf(stderr, "[tome-copy] player 2's cast asks for their Tome\n");
            } else {
                g_coop_quick_tome = true;
            }
        } else if (beside) {
            coop_bytes(cpu, COOP_SPELL_ITEM, spell_was, sizeof spell_was, false);
        }
        if (getenv("ED_COOP_CAST_LOG") != NULL) {
            const u32 spell = mem_read32(cpu, cpu->gpr[13] - 22328u);
            fprintf(stderr, "[coop] player 2's state %u, casting 0x%08X, record +16 0x%X, asked %u\n", mem_read32(cpu, g_coop_p2 + COOP_NODE_STATE), g_coop_casting[0],
                    mem_read32(cpu, COOP_SPELL_ITEM + COOP_SPELL_ITEM_TOME), mem_read32(cpu, cpu->gpr[13] - 26188u));
            fprintf(stderr, "[coop] player 2 casts their quick spell %u (0x%08X): %s (frame %llu)\n", i, g_coop_quick[i],
                    spell != first ? "cast" : "not cast", (unsigned long long)(cpu->timebase / 675000ull));
        }
    }
}

// What player 2 casts from their Tome is theirs. The Tome's Cast puts the
// spell in the magick record's next cast (0x80334DFC), which the frame after
// the Tome closes casts (0x801C4250(1, &next), from 0x80008468) for the
// controlled character, player 1. One put there while player 2 had the Tome
// is cast with them the player, the controlled character and the one casting.
#define COOP_CAST 0x801C4250u
#define COOP_NEXT_CAST 0x80334DFCu
#define COOP_NEXT_CAST_CALL 0x8000846Cu  // the return address in the frame's call
#define COOP_CAST_WORDS_WAIT 300u
static bool g_coop_next_cast_p2;
static bool g_coop_tome_cast;  // casting it
static unsigned g_coop_cast_words;  // pad reads the item their cast shows is yet to come
static unsigned g_coop_p1_cast_words;  // and player 1's
static bool g_coop_p1_cast_shown;      // it is showing: player 2's half keeps its view
bool hle_coop_player2_held(void) { return g_coop_p1_cast_shown; }
static u32 coop_weapon_copy(CPUState* cpu, u32 id, bool make);

static void coop_cast(CPUState* cpu) {
    // Player 1's, on an item (Enchant): what it shows is theirs (coop_menu_input).
    if (!g_coop_next_cast_p2 && cpu->gpr[4] == COOP_NEXT_CAST && cpu->lr == COOP_NEXT_CAST_CALL && sda_ready(cpu) &&
        mem_read32(cpu, COOP_SPELL_ITEM) != 0u)
        g_coop_p1_cast_words = COOP_CAST_WORDS_WAIT;
    if (!g_coop_next_cast_p2 || cpu->gpr[4] != COOP_NEXT_CAST || cpu->lr != COOP_NEXT_CAST_CALL || g_coop_p2 == 0u ||
        !sda_ready(cpu) || !coop_listed(cpu, g_coop_p2)) {
        g_hle_passthrough = true;
        return;
    }
    g_coop_next_cast_p2 = false;
    // An item chosen in their Tome is the original, as it shows; the one they
    // hold is their copy (the cast asks that their equipment lists it, and
    // what it enchants is theirs alone).
    const u32 item = mem_read32(cpu, COOP_SPELL_ITEM);
    const u32 copy = coop_weapon_copy(cpu, item, false);
    if (copy != 0u) {
        mem_write32(cpu, COOP_SPELL_ITEM, mem_read32(cpu, copy + 16u));
        g_coop_cast_words = COOP_CAST_WORDS_WAIT;
    }
    const u32 args[2] = {cpu->gpr[3], cpu->gpr[4]};
    u32 was[5], result = 0u;
    coop_be_player2(cpu, true, was);
    g_coop_turn_p1 = was[1];  // the Tome is player 1's (coop_has_tome)
    g_coop_tome_cast = true;
    hle_guest_call(cpu, COOP_CAST, args, 2, 0.0, &result);
    g_coop_tome_cast = false;
    coop_be_player2(cpu, false, was);
    mem_write32(cpu, COOP_SPELL_ITEM, item);
    if (getenv("ED_COOP_CAST_LOG") != NULL)
        fprintf(stderr, "[coop] player 2 casts from their Tome\n");
    cpu->gpr[3] = result;
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

// A lasting spell's sign over its owner's head (Recover's rune): the sign's
// thing (kind 34) is tied, when it is made, to the controlled character
// (0x80026958, outside anyone's turn: player 1), and each frame the tie
// (0x8002C0D4(sign, tied), each one a holder whose +56 is the node) puts it
// over the one tied. Its record (node +40, then +60) says whom the spell is
// for (+20); the sign of one for player 2 is put over them.
#define COOP_SIGN_FOLLOW 0x8002C0D4u
#define COOP_HOLDER_OF 0x8014ED1Cu

static void coop_sign_follow(CPUState* cpu) {
    g_hle_passthrough = true;
    const u32 sign = cpu->gpr[3], tied = cpu->gpr[4];
    if (g_coop_p2 == 0u || !g_settings.mod_coop || !sda_ready(cpu) || !guest_ram_range_valid(cpu, sign, 60u) ||
        !guest_ram_range_valid(cpu, tied, 60u))
        return;
    const u32 node = mem_read32(cpu, sign + 56u);
    const u32 object = guest_ram_range_valid(cpu, node, 44u) ? mem_read32(cpu, node + 40u) : 0u;
    const u32 record = guest_ram_range_valid(cpu, object, 64u) ? mem_read32(cpu, object + 60u) : 0u;
    if (!guest_ram_range_valid(cpu, record, 24u) || mem_read32(cpu, record + 20u) != mem_read32(cpu, g_coop_p2 + 16u) ||
        mem_read32(cpu, tied + 56u) == g_coop_p2)
        return;
    u32 theirs = 0u;
    if (hle_guest_call(cpu, COOP_HOLDER_OF, &g_coop_p2, 1, 0.0, &theirs) && theirs != 0u) {
        cpu->gpr[3] = sign;
        cpu->gpr[4] = theirs;
    }
}

// Each player their own lasting spell. A lasting spell's thing (kind 34, one
// made per cast: 0x800CBD34) is found, wherever the game asks for the
// controlled character's (a new one ending the old, its sign tied, its
// effect told), as the first of its kind in the world (0x801F8FD8(first,
// 34)): there was one, player 2's Recover ending player 1's. The one found is
// the controlled character's own (its record's +20), player 2's while their
// spells play out (coop_be_player2), never player 2's for anyone else.
#define COOP_FIND_KIND 0x801F8FD8u
#define COOP_KIND_LASTING 34u

static void coop_lasting_find(CPUState* cpu) {
    g_hle_passthrough = true;
    if (cpu->gpr[4] != COOP_KIND_LASTING || g_coop_p2 == 0u || !g_settings.mod_coop || !sda_ready(cpu) ||
        !coop_listed(cpu, g_coop_p2))
        return;
    const u32 p2_id = mem_read32(cpu, g_coop_p2 + 16u);
    const bool p2 = mem_read32(cpu, cpu->gpr[13] - 21488u) == p2_id;
    u32 found = 0u;
    for (u32 thing = cpu->gpr[3], n = 0u; thing != 0u && n < 65536u && guest_ram_range_valid(cpu, thing, 44u);
         thing = mem_read32(cpu, thing + 4u), n++) {
        if (mem_read32(cpu, thing + 8u) != COOP_KIND_LASTING)
            continue;
        const u32 object = mem_read32(cpu, thing + 40u);
        const u32 record = guest_ram_range_valid(cpu, object, 64u) ? mem_read32(cpu, object + 60u) : 0u;
        const bool theirs = guest_ram_range_valid(cpu, record, 24u) && mem_read32(cpu, record + 20u) == p2_id;
        if (theirs == p2) {
            found = thing;
            break;
        }
    }
    cpu->gpr[3] = found;
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

// Player 2's spells play out as theirs. A spell cast is a record in the
// game's list of them (r13 - 22324 the first, +20 the next; its caster's id
// at +12), each stepped every frame by the spells' update (0x801C5994),
// which calls the record's step (+28) as often as its clock (+0 >> 16, +4084
// the steps done) says. A step asks round the player and the controlled
// character (its aura, where it goes, who it acts on), which were player 1's:
// Roberto had the aura of Alex's spell. The update is the game's, run here,
// with a record player 2 cast stepped with them the player, the controlled
// character and the one casting (coop_be_player2): its end clears what they
// are casting, not what player 1 is.
#define COOP_SPELLS_UPDATE 0x801C5994u
#define COOP_SPELLS_PAUSED 0x80157364u
#define COOP_SPELL_RUNS 0x80155450u

static void coop_spells_update(CPUState* cpu) {
    if (g_coop_p2 == 0u || !g_settings.mod_coop || !sda_ready(cpu) || !coop_listed(cpu, g_coop_p2)) {
        g_hle_passthrough = true;
        return;
    }
    const u32 p2_id = mem_read32(cpu, g_coop_p2 + 16u);
    u32 paused = 0u, runs = 0u, ignore = 0u;
    // ED_COOP_SPELL_LIST=1 (testing): the spells, every 30th frame.
    if (getenv("ED_COOP_SPELL_LIST") != NULL && (cpu->timebase / 675000ull) % 30u == 0u) {
        fprintf(stderr, "[coop] spells at frame %llu:", (unsigned long long)(cpu->timebase / 675000ull));
        for (u32 s = mem_read32(cpu, cpu->gpr[13] - 22324u), n = 0u; s != 0u && n < 64u; s = mem_read32(cpu, s + 20u), n++)
            fprintf(stderr, " %08X(%08X by %X fl %02X%02X%02X step %08X)", s, mem_read32(cpu, s + 4u),
                    mem_read32(cpu, s + 12u), mem_read8(cpu, s + 4080u), mem_read8(cpu, s + 4081u),
                    mem_read8(cpu, s + 4082u), mem_read32(cpu, s + 28u));
        fprintf(stderr, "\n");
    }
    hle_guest_call(cpu, COOP_SPELLS_PAUSED, NULL, 0, 0.0, &paused);
    for (u32 spell = mem_read32(cpu, cpu->gpr[13] - 22324u), n = 0u; spell != 0u && n < 4096u;
         spell = mem_read32(cpu, spell + 20u), n++) {
        const u8 flags = mem_read8(cpu, spell + 4082u);
        if (paused != 0u) {
            if ((flags & 0x80u) == 0u)
                continue;
        } else if ((flags & 0x05u) == 0u) {
            const u32 room = mem_read32(cpu, spell + 8u);
            if (!hle_guest_call(cpu, COOP_SPELL_RUNS, &room, 1, 0.0, &runs) || runs == 0u)
                continue;
        }
        if (mem_read32(cpu, spell) == 0u && mem_read16(cpu, spell + 4084u) != 0u) {
            mem_write16(cpu, spell + 4084u, 0u);
            mem_write32(cpu, spell, 0x10000u);
        }
        const u32 due = mem_read32(cpu, spell) >> 16, done = mem_read16(cpu, spell + 4084u);
        const bool theirs = mem_read32(cpu, spell + 12u) == p2_id;
        if (done == 0u && due > 0u && getenv("ED_COOP_CAST_LOG") != NULL)
            fprintf(stderr, "[coop] spell 0x%08X (0x%08X) cast by 0x%X: %s (frame %llu)\n", spell,
                    mem_read32(cpu, spell + 4u), mem_read32(cpu, spell + 12u), theirs ? "player 2's" : "not player 2's",
                    (unsigned long long)(cpu->timebase / 675000ull));
        u32 was[5];
        if (theirs)
            coop_be_player2(cpu, true, was);
        for (u32 k = 0; due > done && k < due - done; k++) {
            hle_guest_call(cpu, mem_read32(cpu, spell + 28u), &spell, 1, 0.0, &ignore);
            mem_write16(cpu, spell + 4084u, (u16)(mem_read16(cpu, spell + 4084u) + 1u));
        }
        if (theirs)
            coop_be_player2(cpu, false, was);
        mem_write32(cpu, spell, mem_read32(cpu, spell) + mem_read32(cpu, cpu->gpr[13] - 27032u));
    }
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

// An enchanted thing's look (0x800D9304(node), from its update) ends the
// controlled character's cast, the spell having left the enchanting to it. A
// thing of player 2's ends theirs: player 1's would be ended, and theirs would
// stay, no other cast begun.
#define COOP_ENCHANTED_LOOK 0x800D9304u

static void coop_enchanted_look(CPUState* cpu) {
    static bool running;
    const u32 node = cpu->gpr[3];
    const u32 record = guest_ram_range_valid(cpu, node, 48u) ? mem_read32(cpu, node + 44u) : 0u;
    const u32 kind = guest_ram_range_valid(cpu, record, 8u) ? mem_read32(cpu, record + THING_KIND_RECORD) : 0u;
    if (running || g_coop_p2 == 0u || !g_settings.mod_coop || !sda_ready(cpu) || !coop_own_kind(kind) ||
        !coop_listed(cpu, g_coop_p2)) {
        g_hle_passthrough = true;
        return;
    }
    u32 was[5], ignore = 0u;
    coop_be_player2(cpu, true, was);
    running = true;
    hle_guest_call(cpu, COOP_ENCHANTED_LOOK, &node, 1, 0.0, &ignore);
    running = false;
    coop_be_player2(cpu, false, was);
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

// An enchant's close-up (0x80024528, as the thing is shown) stops play for
// both players. The spell shows it only if its caster is the controlled
// character (0x801F6258, asked at 0x801C6DC8, and at 0x801C6990 to tie the
// thing to the caster); for any other it enchants the thing where it is and
// gives it back, and play goes on. In co-op that is what it does for either
// player. With no close-up to end it, the cast ends here, for whoever's it is
// (their spell is stepped with them the player, coop_spells_update). The spell
// (r31; the thing's id at +188, the caster's at +196) has taken the thing from
// the caster's list and left the close-up to give it back, and what it is tied
// to (0x80024528's first steps): that is done here too.
#define COOP_CONTROLLED_ID 0x801F6258u
#define COOP_ENCHANT_ASKS 0x801C6DCCu  // the return addresses in the spell's asking
#define COOP_ENCHANT_TIES 0x801C6994u
#define COOP_CAST_SET 0x801C54C0u
#define COOP_NODE_LIST 0x801F63A0u
#define COOP_NODE_RECORD 0x801F6398u
#define COOP_RECORD_TIED 0x8015095Cu
#define COOP_LIST_MOVE 0x801F95A0u
#define COOP_THING_CARRIED 0x801F9DE8u

static u32 coop_enchant_node(CPUState* cpu, u32 id) {
    u32 node = 0u;
    hle_guest_call(cpu, 0x801F5F88u, &id, 1, 0.0, &node);
    return node;
}

// The thing and what it is tied to back on the caster's list.
static void coop_enchant_give_back(CPUState* cpu, u32 spell) {
    const u32 id = mem_read32(cpu, spell + 188u), caster = mem_read32(cpu, spell + 196u);
    const u32 node = coop_enchant_node(cpu, id), by = coop_enchant_node(cpu, caster);
    if (node == 0u || by == 0u)
        return;
    u32 list = 0u, record = 0u, tied = 0u, ignore = 0u;
    hle_guest_call(cpu, COOP_NODE_LIST, &by, 1, 0.0, &list);
    hle_guest_call(cpu, COOP_NODE_RECORD, &node, 1, 0.0, &record);
    hle_guest_call(cpu, COOP_RECORD_TIED, &record, 1, 0.0, &tied);
    const u32 move[2] = {node, list};
    hle_guest_call(cpu, COOP_LIST_MOVE, move, 2, 0.0, &ignore);
    if ((s32)tied > 0) {
        const u32 other = coop_enchant_node(cpu, tied), move2[2] = {other, list};
        hle_guest_call(cpu, COOP_LIST_MOVE, move2, 2, 0.0, &ignore);
    }
    const u32 carried[3] = {node, caster, 30u};
    hle_guest_call(cpu, COOP_THING_CARRIED, carried, 3, 0.0, &ignore);
}

static void coop_controlled_id(CPUState* cpu) {
    if ((cpu->lr != COOP_ENCHANT_ASKS && cpu->lr != COOP_ENCHANT_TIES) || g_coop_p2 == 0u || !g_settings.mod_coop ||
        !sda_ready(cpu)) {
        g_hle_passthrough = true;
        return;
    }
    if (cpu->lr == COOP_ENCHANT_ASKS) {
        const u32 none = 0u;
        u32 ignore = 0u;
        const u32 lr = cpu->lr;
        g_coop_cast_words = 0u;
        g_coop_p1_cast_words = 0u;
        coop_enchant_give_back(cpu, cpu->gpr[31]);
        hle_guest_call(cpu, COOP_CAST_SET, &none, 1, 0.0, &ignore);
        cpu->lr = lr;
        if (getenv("ED_COOP_CAST_LOG") != NULL)
            fprintf(stderr, "[coop] an enchant goes on without its close-up\n");
    }
    cpu->gpr[3] = ~0u;
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

// An enchant's start holds play for its close-up (0x80199EA0(0), at 0x801C36B0;
// the close-up's end lets go): there is none, so nothing to hold.
#define COOP_PLAY_HOLD 0x80199EA0u
#define COOP_ENCHANT_HOLDS 0x801C36B4u  // the return address

// A script's own step holding play (0x8016820C), not a look opening: what a way out does
// for its transition. Run as player 2's, it isn't a close-up of theirs: taken for one, the
// transition showed in their half at their cameras, with player 1's view held.
#define SCRIPT_HOLD_FROM 0x8016820Cu
#define SCRIPT_HOLD_TO 0x801682A8u
static bool g_coop_script_hold;
// Player 1 looking closely at a thing they pressed on, held with no words or find up yet (the circle:
// its scene, then DETAIL). The hold stays for them; player 2 plays on (coop_p1_close_up).
static bool g_coop_p1_look;
static s32 g_coop_p1_used_script, g_coop_p1_ran_script;

static void coop_play_hold(CPUState* cpu) {
    if (cpu->lr >= SCRIPT_HOLD_FROM && cpu->lr < SCRIPT_HOLD_TO && g_coop_p2 != 0u && sda_ready(cpu)) {
        g_coop_script_hold = cpu->gpr[3] == 0u && mem_read32(cpu, cpu->gpr[13] - 21496u) == mem_read32(cpu, g_coop_p2 + 16u);
        if (cpu->gpr[3] == 0u && !g_coop_script_hold && g_coop_p1_used_script > 0 && g_coop_p1_ran_script == g_coop_p1_used_script) {
            g_coop_p1_look = true;
            if (getenv("ED_COOP_LOG") != NULL)
                fprintf(stderr, "[coop] player 1's look at script %d holds them alone\n", g_coop_p1_used_script);
        }
    }
    if (cpu->lr != COOP_ENCHANT_HOLDS || g_coop_p2 == 0u || !g_settings.mod_coop || !sda_ready(cpu)) {
        g_hle_passthrough = true;
        return;
    }
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

// The Tome in co-op: the world goes on behind it. The game's loop runs one
// mode's frame (r13 - 26192; the mode - 26240, the one asked for - 26188; 2 the
// world, 3 the Tome), and the Tome's start stops the world's for good reason:
// the Tome is given the room's data buffer (0x801538A4), the collision among it,
// and the objects' motion is let go. Here it has memory of its own past the
// scratch (SCRATCH_TOME), the objects keep theirs, and the loop runs the two
// modes' frames in turn, TW_WORLD_FRAMES of the world's to one of the Tome's, with
// what each sets for its drawing (the settings' block, TW_BLOCK; the switches) as
// that one had it. The world is drawn once, from the other player's camera, and its
// frames are the ones shown, with the Tome's last picture in its owner's half (aurora).
// The Tome's are only kept, its frames in between shown with the world's. None waits on
// the presents' pacing (the world's retrace wait keeps time), so both go at full speed
// and both halves at the screen's rate, up to the frame rate set.
#define TW_MODE_CHANGE 0x80018728u
#define TW_TOME_START 0x80111078u
#define TW_WORLD_FRAME 0x800083C8u
#define TW_TOME_FRAME 0x80111234u
#define TW_ROOM_BUFFER 0x801538A4u
#define TW_TOME_ASKS 0x80110A30u
#define TW_TOME_MEMORY 0x2D8020u
#define SCRATCH_TOME 0x82000000u
#define TW_BLOCK 0x80602B18u
#define TW_BLOCK_SIZE 0x1400u
#define TW_WORLD_FRAMES 1u
static const u32 kTwSwitches[] = {22216u, 23760u, 23456u, 24920u, 24048u, 21936u, 21996u, 21896u, 21892u, 21800u, 21796u, 21700u};
#define TW_SWITCHES (sizeof kTwSwitches / sizeof kTwSwitches[0])
#define TW_OBJECTS_OFF 2u  // r13 - 23456 in kTwSwitches
static struct {
    bool sharing, world_tick, have_world;
    unsigned worlds_left;  // the world's frames to come before the Tome's, this one among them
    u32 world[TW_SWITCHES + 2], tome[TW_SWITCHES + 2];
    u8 block[2][TW_BLOCK_SIZE];  // 0 the world's, 1 the Tome's
    unsigned ticks;
} g_tw;

bool hle_coop_tome_shares(void) { return g_tw.sharing; }

static bool tw_world_turn(void) { return g_tw.sharing && g_tw.world_tick; }
// A pad read is for the frame after it.
static bool tw_tome_next(void) { return g_tw.sharing && g_tw.world_tick && g_tw.worlds_left <= 1u; }

static bool tw_on(CPUState* cpu) {
    return sda_ready(cpu) && g_settings.mod_coop && g_coop_p2 != 0u && cpu->ram_size >= SCRATCH_TOME + TW_TOME_MEMORY - GC_RAM_BASE;
}

static void tw_switches(CPUState* cpu, u32* save, const u32* load) {
    const u32 sda = cpu->gpr[13];
    for (u32 i = 0; i < TW_SWITCHES + 2u; i++) {
        const u32 at = i < TW_SWITCHES ? sda - kTwSwitches[i] : sda + (u32)(i == TW_SWITCHES ? g_menu_flag_offset : g_menu_depth_offset);
        if (save != NULL)
            save[i] = mem_read32(cpu, at);
        if (load != NULL)
            mem_write32(cpu, at, load[i]);
    }
}

static void tw_block(CPUState* cpu, int save, int load) {
    for (u32 i = 0u; save >= 0 && i < TW_BLOCK_SIZE; i += 4u) {
        const u32 v = mem_read32(cpu, TW_BLOCK + i);
        memcpy(g_tw.block[save] + i, &v, 4u);
    }
    for (u32 i = 0u; load >= 0 && i < TW_BLOCK_SIZE; i += 4u) {
        u32 v;
        memcpy(&v, g_tw.block[load] + i, 4u);
        mem_write32(cpu, TW_BLOCK + i, v);
    }
}

#define TOME_COPY_GX_POINTER 26688u  // r13 -: the GX library's record, what it thinks the GPU holds
#define TOME_COPY_GX_SIZE 0x4F8u     // the highest it's used at is +0x4F4 (its dirty state)
#define TOME_COPY_LAST_PAGE 29060u   // r13 -: the page a Tome opens on: 0 Options, 1 Inventory, 2 Map, 3 Magick, 4 Journal
#define TOME_COPY_JOURNAL 24448u     // r13 -: open on the Journal, for what's new in it
#define TOME_COPY_PAGE_STATE 24244u  // r13 -: 3 while the Tome's pages run
#define TOME_HAS_MAGICK 0x80109A68u
static struct {
    u8* copy;
    u8* real;
    bool started, has_magick;
    bool stands_in;   // player 1 has no Tome: the world behind the Tome (g_tw) is for this one alone
    u8* clean;        // the memory as player 1's Tome was asked for, before it was in it
    bool clean_set;
    unsigned turns;
    int kinds_depth, kinds_count;
    u8 kinds[sizeof g_coop_kinds_in];
    double ms_most;
} g_tome_copy;
static bool g_coop_tome_p2;
static bool g_coop_b_release;
void aurora_split_menu(int player, int layout);

// The in-game menu over a Tome: the renderer took the menu's frames for the Tome's and the world's,
// only keeping them, so nothing of the menu was shown and the game looked frozen. While it's up,
// the frames are plain ones, and as the menu closes the views go back as the Tome had them.
void hle_coop_ui_over(bool on) {
    static bool over;
    if (on && (g_tw.sharing || g_tome_copy_open || g_coop_tome_owner >= 0)) {
        over = true;
        aurora_split_menu(-1, 0);
        aurora_split_only_view(-1);
        dol_aurora_split_skip_views(0);
    } else if (!on && over) {
        over = false;
        if (g_tome_copy_open && g_tome_copy_p1) {
            aurora_split_menu(0, g_settings.mod_split);
            aurora_split_only_view(1);
        } else if (g_tome_copy.stands_in) {
            aurora_split_menu(1, g_settings.mod_split);
            aurora_split_only_view(0);
        } else {
            if (g_coop_tome_owner >= 0)
                aurora_split_menu(g_coop_tome_owner, g_settings.mod_split);
            aurora_split_only_view(g_tome_copy_open ? 2 : g_coop_tome_owner >= 0 ? 1 - g_coop_tome_owner : -1);
        }
    }
}


// A quick state loaded: the frames are the game's own again.
static void tw_state_loaded(void) {
    g_tome_copy_ask = g_tome_copy_open = false;
    g_tome_copy_p1 = false;
    g_tome_copy_p1_hold = 0u;
    g_tw.sharing = g_tw.world_tick = g_tw.have_world = false;
    g_tome_copy.stands_in = g_tome_copy.clean_set = false;
    dol_aurora_split_skip_views(0);
    dol_aurora_frame_unpaced(false);
    aurora_split_only_view(-1);
}

static void tw_tome_start(CPUState* cpu) {
    if (sda_ready(cpu)) {
        tw_block(cpu, 0, -1);
        tw_switches(cpu, g_tw.world, NULL);
        // The world's frames know of no menu (its asking may have set these already).
        g_tw.world[TW_SWITCHES] = g_tw.world[TW_SWITCHES + 1u] = 0u;
        g_tw.world[TW_OBJECTS_OFF] = 0u;  // the Tome's own (0x8014ED04): kept, nobody moved or was drawn, then or after it
        g_tw.have_world = true;
    }
    g_hle_passthrough = true;
}

// What the Tome's start lets go of, and its end takes back: the objects' motion
// (0x8011F2D4, 0x80125190), what they hold (0x80043AB8), the room packed into its
// own data (0x801104D4, 0x80110C24) and read back (0x80152298).
static void tw_keep(CPUState* cpu) {
    const bool ours = cpu->lr == 0x80111160u || cpu->lr == 0x80111164u || cpu->lr == 0x801110FCu || cpu->lr == 0x801110C4u ||
                      cpu->lr == 0x80110EC4u || cpu->lr == 0x80110E58u || cpu->lr == 0x80110E8Cu || cpu->lr == 0x80110E70u ||
                      cpu->lr == 0x80111138u || cpu->lr == 0x80111148u || cpu->lr == 0x8011114Cu || cpu->lr == 0x8011115Cu;  // its sound's hush
    if (!ours || !tw_on(cpu)) {
        g_hle_passthrough = true;
        return;
    }
    if (cpu->lr == 0x80110E8Cu)
        mem_write32(cpu, cpu->gpr[13] - 26508u, 0u);  // the room read back asked for: none
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

static void tw_room_buffer(CPUState* cpu) {
    if (cpu->lr != TW_TOME_ASKS || !tw_on(cpu)) {
        g_hle_passthrough = true;
        return;
    }
    cpu->gpr[3] = SCRATCH_TOME;
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

// A frame's end waits for the retrace's message (0x801DFA4C, OSReceiveMessage) and
// the retrace itself (0x801E4764, VIWaitForRetrace). The world's frames keep their waits
// and are the ones shown, at their own speed; the Tome's, between them, wait for neither.
#define TW_FRAME_WAIT 0x8020190Cu
#define TW_FRAME_WAITS 0x801DFA50u
#define TW_RETRACE_WAITS 0x801E4768u
static void tw_retrace_wait(CPUState* cpu) {
    if (cpu->lr == TW_RETRACE_WAITS && ((g_tw.sharing && !g_tw.world_tick) || g_tome_copy_turn)) {
        g_hle_passthrough = false;
        g_hle_tail_call = false;
    } else {
        g_hle_passthrough = true;
    }
}

static void tw_frame_wait(CPUState* cpu) {
    if (cpu->lr == TW_FRAME_WAITS && ((g_tw.sharing && !g_tw.world_tick) || g_tome_copy_turn)) {
        cpu->gpr[3] = 1u;
        g_hle_passthrough = false;
        g_hle_tail_call = false;
    } else {
        g_hle_passthrough = true;
    }
}

static void coop_tome_player(CPUState* cpu, bool p2);

// ED_COOP_TOME_COPY=1 (testing, off otherwise): test 1 of docs/coop-two-tomes-design.md. Once
// a loop, with no Tome in it, the machine runs one game call on a copy of its memory, and it's
// checked that nothing of that reached the real memory. The copy is made again every
// TOME_COPY_EVERY turns, for what that costs. A guest call polls no interrupts. One asked with
// the FPU not this thread's saves its registers as it takes it: that is done first, in the real memory.
#define TOME_COPY_EVERY 600u
#define TOME_COPY_CANARY 0x7E57C0DEu
#define TOME_COPY_STACK 0x1000u

static u64 tome_copy_ns(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (u64)now.tv_sec * 1000000000ull + (u64)now.tv_nsec;
}

static void tome_copy_test(CPUState* cpu) {
    static int on = -1;
    static u8* copy;
    static u8* stack_was;
    static unsigned turns, fpu_taken, bad;
    static u64 copy_ns, turn_ns_most;
    if (on < 0) {
        on = tome_copy_mode() == 1;
        if (on) {
            copy = (u8*)malloc(cpu->ram_size);
            stack_was = (u8*)malloc(TOME_COPY_STACK);
            on = copy != NULL && stack_was != NULL;
            fprintf(stderr, "[tome-copy] %s, %u MB of memory\n", on ? "on" : "no memory for it", cpu->ram_size / (1024u * 1024u));
        }
    }
    if (!on)
        return;
    if ((cpu->msr & 0x00002000u) == 0u) {  // MSR[FP], as hle_guest_call
        cpu->srr0 = cpu->pc;
        cpu->srr1 = cpu->msr;
        interrupt_switch_fpu_context(cpu);
        fpu_taken++;
    }
    u64 t0 = tome_copy_ns();
    if (turns % TOME_COPY_EVERY == 0u) {
        memcpy(copy, cpu->ram, cpu->ram_size);
        copy_ns = tome_copy_ns() - t0;
    }
    t0 = tome_copy_ns();
    u8* const real = cpu->ram;
    const u32 canary_at = GC_RAM_BASE + cpu->ram_size - 4u;
    const u32 stack_at = (cpu->gpr[1] - TOME_COPY_STACK) & ~15u;
    const u32 canary_was = mem_read32(cpu, canary_at);
    memcpy(stack_was, real + (stack_at - GC_RAM_BASE), TOME_COPY_STACK);
    const u32 id_real = mem_read32(cpu, cpu->gpr[13] - 21488u);
    cpu->ram = copy;
    mem_write32(cpu, canary_at, TOME_COPY_CANARY);
    u32 id_copy = 0u;
    const bool called = hle_guest_call(cpu, COOP_CONTROLLED_ID, NULL, 0, 0.0, &id_copy);
    const bool in_copy = mem_read32(cpu, canary_at) == TOME_COPY_CANARY;
    mem_write32(cpu, canary_at, canary_was);
    cpu->ram = real;
    const u64 took = tome_copy_ns() - t0;
    if (took > turn_ns_most)
        turn_ns_most = took;
    const bool same = mem_read32(cpu, canary_at) == canary_was && memcmp(stack_was, real + (stack_at - GC_RAM_BASE), TOME_COPY_STACK) == 0;
    const bool fresh = turns % TOME_COPY_EVERY != 0u || id_copy == id_real;
    if (!called || !in_copy || !same || !fresh) {
        if (bad++ < 20u)
            fprintf(stderr, "[tome-copy] FAILED turn %u: called %d, in the copy %d, real memory the same %d, same id %d (0x%X 0x%X)\n", turns,
                    called, in_copy, same, fresh, id_copy, id_real);
    }
    if (++turns % TOME_COPY_EVERY == 0u) {
        fprintf(stderr, "[tome-copy] %u turns, %u failed, FPU taken %u times; copy %.2f ms, longest turn %.1f us\n", turns, bad, fpu_taken,
                (double)copy_ns / 1e6, (double)turn_ns_most / 1e3);
        turn_ns_most = 0u;
    }
}

// ED_COOP_TOME_COPY=2 (testing, off otherwise): test 2 of docs/coop-two-tomes-design.md. Player
// 2's Tome runs in a whole copy of the memory, where the game is alone and stopped, as the Tome
// expects: its frames run here, as guest calls (no interrupts), before each of the world's.
// To the rest of the host it's the world behind the Tome (g_tw) with them its owner. What it does
// isn't sent to the real game yet. Nothing in the real memory knows of it; closing throws it away.

// Into the copy or back. There's one GPU, so what the GX library knows of it goes along.
static void tome_copy_swap(CPUState* cpu, bool in) {
    const u32 gx = mem_read32(cpu, cpu->gpr[13] - TOME_COPY_GX_POINTER);
    u8* const to = in ? g_tome_copy.copy : g_tome_copy.real;
    if (guest_ram_range_valid(cpu, gx, TOME_COPY_GX_SIZE))
        memcpy(to + (gx - GC_RAM_BASE), cpu->ram + (gx - GC_RAM_BASE), TOME_COPY_GX_SIZE);
    cpu->ram = to;
    dol_aurora_mem1_turn(to);
}

static void tome_copy_equips(CPUState* cpu);
static void tome_copy_left(CPUState* cpu, s32 script, u32 next_cast, const u8* spell_item);

// Player 1's words or find, up as the copy is made, are theirs and stay in the real game. In the
// copy they showed in player 2's Tome, and player 2's B closed them there: the script they lead to
// ran on in the copy, every find of it passed by, and came back to the world as an item's. Gone from
// the copy, which is all this writes: the words freed, the window let go, nothing left to run.
#define TOME_COPY_FIND_STEP 0x80024434u
#define TOME_COPY_FIND_RECORD 0x8030A6E8u
#define TOME_COPY_WORDS_RECORD 0x8023E458u
static void tome_copy_no_window(CPUState* cpu) {
    const u32 sda = cpu->gpr[13], window = mem_read32(cpu, sda - 25960u);
    u32 ignore = 0u;
    if (window != 0u && guest_ram_range_valid(cpu, window, 32u)) {
        u32 words[2] = {mem_read32(cpu, TOME_COPY_WORDS_RECORD + 12u), 0u};
        if (mem_read32(cpu, window) == TOME_COPY_FIND_STEP) {
            words[0] = mem_read32(cpu, TOME_COPY_FIND_RECORD);
            words[1] = mem_read32(cpu, TOME_COPY_FIND_RECORD + 4u);
        }
        for (int i = 0; i < 2; i++)
            if (words[i] != 0u)
                hle_guest_call(cpu, 0x801DA4D8u, &words[i], 1, 0.0, &ignore);
        hle_guest_call(cpu, 0x8013DB94u, &window, 1, 0.0, &ignore);
        fprintf(stderr, "[tome-copy] the window up as it opened is left out of the copy\n");
    }
    mem_write32(cpu, sda - 25960u, 0u);  // the window
    mem_write32(cpu, sda - 25944u, 0u);  // the script its closing runs
    mem_write32(cpu, sda - 25948u, 0u);
    mem_write32(cpu, sda - 23552u, 0u);  // the input's focus
    mem_write32(cpu, sda - 23556u, 0u);
    mem_write32(cpu, sda - 23344u, 0u);  // busy
    mem_write32(cpu, sda - 24048u, mem_read32(cpu, sda - 24048u) & ~(2u | 4u | 32u));  // what the holds set
    mem_write8(cpu, sda - 25932u, 0u);
    mem_write32(cpu, sda - 25696u, 0u);
    mem_write32(cpu, sda - 23040u, 1u);
    mem_write32(cpu, sda - 26324u, 0u);  // a script left for the world
    // The Magick page's help is for whoever the game opened it for (0x8010AEF0: 499 and not 500, 989
    // and not 990, 991 and not 992 play script 2020; not 622, script 887 goes on with it): done, for
    // this copy alone. Played here, it ended by closing their Tome. The game's own stay as they are.
    const u32 set = mem_read32(cpu, sda - 26528u);
    if (guest_ram_range_valid(cpu, set, 4u + 992u / 8u + 4u)) {
        static const u16 kDone[4][2] = {{499u, 500u}, {989u, 990u}, {991u, 992u}, {0u, 622u}};
        for (int i = 0; i < 4; i++) {
            const u32 from = set + 4u + (u32)kDone[i][0] / 32u * 4u, to = set + 4u + (u32)kDone[i][1] / 32u * 4u;
            if (i == 3 || ((mem_read32(cpu, from) >> (kDone[i][0] % 32u)) & 1u) != 0u)
                mem_write32(cpu, to, mem_read32(cpu, to) | 1u << (kDone[i][1] % 32u));
        }
    }
}

bool hle_coop_player1_tome_up(void) { return g_tw.sharing && !g_tome_copy.stands_in; }
bool hle_coop_player1_copy_up(void) { return g_tome_copy_open && g_tome_copy_p1; }

static void tome_copy_p1_left(CPUState* cpu);
static s32 g_tome_copy_use_script;
static u32 g_tome_copy_use_item;
// What player 1's Tome left for the world: it waits for player 2's close-up to end, as the game has one
// window and one script's steps (tome_copy_p1_left).
static struct {
    bool pending;
    s32 script, use_script;
    u32 next_cast, use_item;
    u8 spell_item[24];
} g_tome_copy_p1_left;
static u32 g_tome_copy_p1_record[COOP_MAGICK_SIZE / 4u];  // the magick record as their copy opened

static void tome_copy_p1_close(CPUState* cpu, const char* why) {
    fprintf(stderr, "[tome-copy] player 1's Tome closes (%s) after %u turns, longest %.2f ms\n", why, g_tome_copy.turns, g_tome_copy.ms_most);
    cpu->ram = g_tome_copy.copy;
    const s32 script_left = (s32)mem_read32(cpu, cpu->gpr[13] - 26324u);
    const u32 next_cast = mem_read32(cpu, COOP_NEXT_CAST);
    u8 spell_item[24];
    coop_bytes(cpu, COOP_SPELL_ITEM, spell_item, sizeof spell_item, true);
    u32 record[COOP_MAGICK_SIZE / 4u];
    for (u32 i = 0u; i < COOP_MAGICK_SIZE / 4u; i++)
        record[i] = mem_read32(cpu, COOP_MAGICK_RECORD + i * 4u);
    cpu->ram = g_tome_copy.real;
    // The record's theirs: what they changed (a quick spell, a spell made), and nothing else. Player 2's
    // find may have added to the real one since it opened.
    for (u32 i = 0u; i < COOP_MAGICK_SIZE / 4u; i++)
        if (record[i] != g_tome_copy_p1_record[i]) {
            fprintf(stderr, "[magick] player 1's copy +0x%02X 0x%08X goes to the real game\n", i * 4u, record[i]);
            mem_write32(cpu, COOP_MAGICK_RECORD + i * 4u, record[i]);
        }
    g_tome_copy_open = false;
    g_tome_copy_p1 = false;
    g_tome_copy_p1_hold = g_tome_copy_p1_pad.button;
    g_tome_copy_taps = 0u;
    g_tome_copy.stands_in = false;
    g_tw.sharing = g_tw.world_tick = false;
    dol_aurora_split_skip_views(0);
    dol_aurora_frame_unpaced(false);
    aurora_split_only_view(-1);
    aurora_split_menu(-1, 0);
    if (script_left > 0 || next_cast != 0u) {
        g_tome_copy_p1_left.pending = true;
        g_tome_copy_p1_left.script = script_left;
        g_tome_copy_p1_left.next_cast = next_cast;
        g_tome_copy_p1_left.use_script = g_tome_copy_use_script;
        g_tome_copy_p1_left.use_item = g_tome_copy_use_item;
        memcpy(g_tome_copy_p1_left.spell_item, spell_item, sizeof spell_item);
    }
}

static void tome_copy_close(CPUState* cpu, const char* why) {
    if (g_tome_copy_p1) {
        tome_copy_p1_close(cpu, why);
        return;
    }
    fprintf(stderr, "[tome-copy] player 2's Tome closes (%s) after %u turns, longest %.2f ms\n", why, g_tome_copy.turns, g_tome_copy.ms_most);
    // Back as coop_tome_player leaves them, in the copy: their quick spells and counts come out of it.
    cpu->ram = g_tome_copy.copy;
    // What the Tome left for the world as it closes: the script an item left, the spell to cast.
    const s32 script_left = (s32)mem_read32(cpu, cpu->gpr[13] - 26324u);
    const u32 next_cast = mem_read32(cpu, COOP_NEXT_CAST);
    u8 spell_item[24];
    coop_bytes(cpu, COOP_SPELL_ITEM, spell_item, sizeof spell_item, true);
    // The spells they made stay theirs, for their next Tome. The game's record is player 1's.
    for (u32 i = 0u; i < COOP_SPELL_WORDS; i++)
        g_coop_spells[i] = mem_read32(cpu, COOP_MAGICK_RECORD + 8u + i * 4u);
    // What their Tome changed in the magick record: it goes with the copy.
    for (u32 i = 0u; coop_trace() && i < COOP_MAGICK_SIZE / 4u; i++) {
        const u32 at = COOP_MAGICK_RECORD + i * 4u, copy_word = mem_read32(cpu, at);
        cpu->ram = g_tome_copy.real;
        const u32 real_word = mem_read32(cpu, at);
        cpu->ram = g_tome_copy.copy;
        if (copy_word != real_word)
            fprintf(stderr, "[magick] player 2's copy +0x%02X 0x%08X, real 0x%08X\n", i * 4u, copy_word, real_word);
    }
    g_coop_kinds_depth = g_tome_copy.kinds_depth;
    g_coop_kinds_in_count = g_tome_copy.kinds_count;
    memcpy(g_coop_kinds_in, g_tome_copy.kinds, sizeof g_coop_kinds_in);
    g_coop_tome_p2 = true;
    coop_tome_player(cpu, false);
    cpu->ram = g_tome_copy.real;
    g_coop_tome_p2 = false;
    g_tome_copy_open = false;
    // What they held as it closed is not a press in play (the A that chose was their attack).
    DolPadState pad;
    controls_read_player2(&pad);
    g_coop_p2_hold = pad.button;
    g_coop_b_release = true;
    if (g_tome_copy.stands_in) {
        g_tome_copy.stands_in = false;
        g_tw.sharing = g_tw.world_tick = false;
        dol_aurora_split_skip_views(0);
        dol_aurora_frame_unpaced(false);
        aurora_split_only_view(-1);
        aurora_split_menu(-1, 0);
    } else if (g_tw.sharing) {
        // Player 1's stays: theirs is the one menu, the world in player 2's half again.
        aurora_split_menu(0, g_settings.mod_split);
        aurora_split_only_view(1);
    }
    tome_copy_left(cpu, script_left, next_cast, spell_item);
}

// The callbacks its turn has asked for, in order, and those they ask for in turn (its loader's
// chained reads). All before the frame that waits on them.
static void tome_copy_callbacks(CPUState* cpu) {
    for (u32 done = 0u; g_tome_copy_callback_count > 0u && done < 4096u; done++) {
        const HlePendingCallback next = g_tome_copy_callbacks[0];
        g_tome_copy_callback_count--;
        memmove(g_tome_copy_callbacks, g_tome_copy_callbacks + 1, g_tome_copy_callback_count * sizeof g_tome_copy_callbacks[0]);
        const u32 args[2] = {(u32)next.channel, (u32)next.result};
        u32 ignore = 0u;
        if (!hle_guest_call(cpu, next.address, args, 2, 0.0, &ignore))
            fprintf(stderr, "[tome-copy] callback 0x%08X did not return\n", next.address);
    }
}

// Once a loop while it's asked for or open. True: it ran, and the world's frame is next.
static bool tome_copy_step(CPUState* cpu) {
    if (!g_tome_copy_ask && !g_tome_copy_open)
        return false;
    const u32 sda = cpu->gpr[13];
    const u32 mode_now = mem_read32(cpu, sda - 26240u), asked_now = mem_read32(cpu, sda - 26188u);
    const bool world_stays = (mode_now == 2u || mode_now == 3u) && (asked_now == 2u || asked_now == 3u) && !cutscene_playing(cpu) &&
                             g_coop_p2 != 0u && coop_listed(cpu, g_coop_p2);
    if (!world_stays) {
        g_tome_copy_ask = false;
        if (g_tome_copy_spell_set) {
            g_tome_copy_spell_set = false;
            tome_copy_left(cpu, 0, 0u, NULL);
        }
        if (g_tome_copy_open)
            tome_copy_close(cpu, "the world asks for something else");
        return false;
    }
    // Player 1 asks for their Tome: the game's own way in, and it's the real world behind a Tome.
    if (g_tome_copy.stands_in && asked_now == 3u) {
        g_tome_copy.stands_in = false;
        g_tw.sharing = g_tw.world_tick = false;
    }
    // Theirs has closed, with this one still open: it stands in again.
    if (g_tome_copy_open && !g_tome_copy_p1 && !g_tome_copy.stands_in && !g_tw.sharing && mode_now == 2u && asked_now == 2u) {
        g_tome_copy.stands_in = true;
        g_tw.sharing = true;
        g_tw.world_tick = true;
        aurora_split_menu(1, g_settings.mod_split);
        aurora_split_only_view(0);
    }
    // One of these to each of the world's frames: with player 1's Tome, after its frame.
    if (!g_tome_copy_ask && g_tw.sharing && !g_tome_copy.stands_in && g_tw.world_tick)
        return false;
    if ((cpu->msr & 0x00002000u) == 0u) {  // MSR[FP]: taken in the real memory, as hle_guest_call
        cpu->srr0 = cpu->pc;
        cpu->srr1 = cpu->msr;
        interrupt_switch_fpu_context(cpu);
    }
    const bool opening = g_tome_copy_ask;
    if (opening) {
        g_tome_copy_ask = false;
        if (g_tome_copy.copy == NULL)
            g_tome_copy.copy = (u8*)malloc(cpu->ram_size);
        if (g_tome_copy.copy == NULL || (g_tw.sharing && !g_tome_copy.clean_set))
            return false;
        g_tome_copy.real = cpu->ram;
        memcpy(g_tome_copy.copy, g_tw.sharing ? g_tome_copy.clean : cpu->ram, cpu->ram_size);
        // The story's flags as they are now: the snapshot is from before player 1's Tome, and a page's
        // first visit is told by them (r13 - 26528: the count, then the bits).
        const u32 flags = mem_read32(cpu, sda - 26528u);
        if (guest_ram_range_valid(cpu, flags, 4u)) {
            const u32 flags_size = 4u + (mem_read32(cpu, flags) + 31u) / 32u * 4u;
            if (flags_size <= 1028u && guest_ram_range_valid(cpu, flags, flags_size))
                memcpy(g_tome_copy.copy + (flags - GC_RAM_BASE), cpu->ram + (flags - GC_RAM_BASE), flags_size);
        }
        g_tome_copy.turns = 0u;
        g_tome_copy.ms_most = 0.0;
        g_tome_copy.started = false;
    }
    const u64 t0 = tome_copy_ns();
    const bool sharing_was = g_tw.sharing, tick_was = g_tw.world_tick;
    // Which half keeps its frame: player 1's Tome the first, as the real one when it's theirs. A real Tome
    // of player 2's (their page) beside it keeps the second (tw_mode_change).
    dol_aurora_split_skip_views(g_tome_copy_p1 ? 1 : 2);
    dol_aurora_frame_unpaced(true);
    tome_copy_swap(cpu, true);
    u32 ignore = 0u;
    if (opening) {
        tome_copy_no_window(cpu);
        if (!g_tome_copy_p1) {
            coop_tome_player(cpu, true);
            g_tome_copy.kinds_depth = g_coop_kinds_depth;
            g_tome_copy.kinds_count = g_coop_kinds_in_count;
            coop_spells_in(cpu);
            memcpy(g_tome_copy.kinds, g_coop_kinds_in, sizeof g_coop_kinds_in);
            g_coop_kinds_depth = g_coop_kinds_in_count = 0;
        } else {
            for (u32 i = 0u; i < COOP_MAGICK_SIZE / 4u; i++)
                g_tome_copy_p1_record[i] = mem_read32(cpu, COOP_MAGICK_RECORD + i * 4u);
        }
        mem_write32(cpu, sda - TOME_COPY_LAST_PAGE, 1u);
        mem_write32(cpu, sda - TOME_COPY_JOURNAL, 0u);
        mem_write32(cpu, sda - 26240u, 2u);
        mem_write32(cpu, sda - 26188u, 3u);
        for (u32 at = 0u; at < 24u; at += 4u)
            mem_write32(cpu, COOP_SPELL_ITEM + at, 0u);  // a cast of player 1's that asked for their Tome
        g_tome_copy_spell_open = g_tome_copy_spell_set;
        if (g_tome_copy_spell_set)
            coop_bytes(cpu, COOP_SPELL_ITEM, g_tome_copy_spell, sizeof g_tome_copy_spell, false);
        g_tome_copy_spell_set = false;
        g_tome_copy.has_magick = hle_guest_call(cpu, TOME_HAS_MAGICK, NULL, 0, 0.0, &ignore) && ignore != 0u;
        DolPadState pad;
        if (g_tome_copy_p1)
            pad = g_tome_copy_p1_pad;
        else
            controls_read_player2(&pad);
        g_tome_copy_held = pad.button;
        g_tome_copy_taps = 0u;
        g_tome_copy_open = true;
        if (g_tome_copy_p1) {
            fprintf(stderr, "[tome-copy] player 1's Tome opens in the copy, over player 2's close-up\n");
        } else if (!sharing_was) {
            g_tome_copy.stands_in = true;
            aurora_split_menu(1, g_settings.mod_split);
            aurora_split_only_view(0);
        } else {
            aurora_split_only_view(2);
        }
        if (!g_tome_copy_p1)
            fprintf(stderr, "[tome-copy] player 2's Tome opens in the copy%s\n", sharing_was ? ", beside player 1's" : "");
    }
    // Their half for the Tome's frame, the world in player 2's: the close-up's end sets these again.
    if (g_tome_copy_p1) {
        aurora_split_menu(0, g_settings.mod_split);
        aurora_split_only_view(sharing_was ? 2 : 1);
    }
    g_tw.sharing = true;
    g_tw.world_tick = false;
    g_coop_tome_p2 = !g_tome_copy_p1;
    g_tome_copy_turn = true;
    bool ok = true, leaving = false;
    if (!g_tome_copy.started) {
        ok = hle_guest_call(cpu, TW_MODE_CHANGE, NULL, 0, 0.0, &ignore);  // the game's own way in
        tome_copy_callbacks(cpu);
        g_tome_copy.started = true;
    } else {
        // Its way out runs from its frame (0x80111298, any state but 3) and waits for the
        // room to come back, which it never does here: seen first, the copy is just left.
        leaving = mem_read32(cpu, sda - 26188u) != 3u || mem_read32(cpu, sda - TOME_COPY_PAGE_STATE) != 3u;
    }
    if (ok && !leaving) {
        const u32 none[3] = {0u, 0u, 0u};
        hle_guest_call(cpu, 0x801E58C8u, none, 3, 0.0, &ignore);  // the Tome's fog, as its start set
        static const u32 kAfterFrame[5] = {0x80113DF8u, 0x8013E518u, 0x801718C0u, 0x801DC9F8u, 0x8012EDDCu};  // as the loop, 0x800186E8
        ok = hle_guest_call(cpu, mem_read32(cpu, sda - 26192u), NULL, 0, 0.0, &ignore);
        for (u32 i = 0u; ok && i < 5u; i++)
            ok = hle_guest_call(cpu, kAfterFrame[i], NULL, 0, 0.0, &ignore);
        tome_copy_callbacks(cpu);
    }
    g_tome_copy_turn = false;
    g_coop_tome_p2 = false;
    tome_copy_swap(cpu, false);
    dol_aurora_split_skip_views(0);
    tome_copy_equips(cpu);
    if (g_tome_copy.stands_in) {
        g_tw.sharing = true;
        g_tw.world_tick = true;
        g_tw.worlds_left = 2u;  // tw_tome_next: no read is the Tome's, in the real game
    } else {
        g_tw.sharing = sharing_was;
        g_tw.world_tick = tick_was;
    }
    g_tome_copy.turns++;
    const double ms = (double)(tome_copy_ns() - t0) / 1e6;
    if (ms > g_tome_copy.ms_most)
        g_tome_copy.ms_most = ms;
    if (!ok)
        fprintf(stderr, "[tome-copy] a call in its turn did not return\n");
    if (!ok || leaving)
        tome_copy_close(cpu, ok ? "its own way out" : "a call did not return");
    return true;
}

// The tabs player 2 has no use for: what leads to Journal or Options goes on past them.
#define TOME_PAGE_INVENTORY 0x80102810u
#define TOME_PAGE_MAP 0x80111B40u
#define TOME_PAGE_MAGICK 0x8010AAF0u
static void tome_copy_tab(CPUState* cpu) {
    g_hle_passthrough = true;
    if (!g_tome_copy_turn)
        return;
    u32 to = TOME_PAGE_INVENTORY;
    if (cpu->lr == 0x80103C44u) {  // back from the Inventory
        u32 has = 0u;
        hle_guest_call(cpu, TOME_HAS_MAGICK, NULL, 0, 0.0, &has);
        to = has != 0u ? TOME_PAGE_MAGICK : TOME_PAGE_MAP;
    }
    g_hle_passthrough = false;
    g_hle_tail_call = true;
    cpu->pc = to;
}

static void tw_mode_change(CPUState* cpu) {
    g_hle_passthrough = true;
    if (!tw_on(cpu))
        return;
    tome_copy_test(cpu);
    // ED_COOP_P1_QUICK=spell (testing): player 1's D-pad up casts that spell, as ED_COOP_P2_QUICK.
    if (!g_tome_copy_turn && getenv("ED_COOP_P1_QUICK") != NULL)
        mem_write32(cpu, COOP_QUICK + 4u, (u32)strtoul(getenv("ED_COOP_P1_QUICK"), NULL, 16));
    // Player 1 is about to have their Tome: the world as it is, for a Tome of player 2's after.
    if (tome_copy_mode() == 2 && (!g_tw.sharing || g_tome_copy.stands_in) && mem_read32(cpu, cpu->gpr[13] - 26240u) == 2u && mem_read32(cpu, cpu->gpr[13] - 26188u) == 3u) {
        // The FPU taken first, as for any copy: a copy of memory that says another thread has
        // it clears MSR[FP] at the Tome's start, and its next float is an exception.
        if ((cpu->msr & 0x00002000u) == 0u) {
            cpu->srr0 = cpu->pc;
            cpu->srr1 = cpu->msr;
            interrupt_switch_fpu_context(cpu);
        }
        if (g_tome_copy.clean == NULL)
            g_tome_copy.clean = (u8*)malloc(cpu->ram_size);
        g_tome_copy.clean_set = g_tome_copy.clean != NULL;
        if (g_tome_copy.clean_set)
            memcpy(g_tome_copy.clean, cpu->ram, cpu->ram_size);
    }
    const bool copy_ran = tome_copy_step(cpu);
    g_hle_passthrough = true;  // a nested dispatch passes itself through
    if (!copy_ran && !g_tome_copy_turn) {
        tome_copy_p1_left(cpu);
        g_hle_passthrough = true;
    }
    if (copy_ran && g_tome_copy.stands_in)
        return;
    const u32 sda = cpu->gpr[13];
    const u32 mode = mem_read32(cpu, sda - 26240u), asked = mem_read32(cpu, sda - 26188u);
    if (g_tw.sharing && asked != mode) {
        // Asked to leave: the Tome's own way out from its frame; the world's as it was.
        if (g_tw.world_tick && asked == 3u) {
            mem_write32(cpu, sda - 26188u, mode);  // it is open
        } else if (g_tw.world_tick) {
            fprintf(stderr, "[coop] the world asks for mode %u with the Tome open\n", asked);
            tw_switches(cpu, g_tw.world, g_tw.tome);
            tw_block(cpu, 0, 1);
        } else {
            // The Tome's way out has run: the menu's words as it left them.
            const u32 flag = mem_read32(cpu, sda + (u32)g_menu_flag_offset), depth = mem_read32(cpu, sda + (u32)g_menu_depth_offset);
            tw_switches(cpu, NULL, g_tw.world);
            mem_write32(cpu, sda + (u32)g_menu_flag_offset, flag);
            mem_write32(cpu, sda + (u32)g_menu_depth_offset, depth);
            tw_block(cpu, 1, 0);
        }
        if (!g_tw.world_tick || asked != 3u) {
            mem_write32(cpu, sda - 26240u, 3u);
            mem_write32(cpu, sda - 26192u, TW_TOME_FRAME);
            g_tw.sharing = false;
            g_tw.world_tick = false;
            dol_aurora_split_skip_views(0);
            dol_aurora_frame_unpaced(false);
            aurora_split_only_view(g_tome_copy_open ? 0 : -1);
            return;
        }
    }
    if (!g_tw.sharing) {
        if (mode != 3u || asked != 3u || !g_tw.have_world || mem_read32(cpu, sda - 26192u) != TW_TOME_FRAME)
            return;
        g_tw.sharing = true;
        g_tw.world_tick = false;
        g_tw.ticks = 0u;
        g_tw_held_from = -1;
        g_tw_taps = 0u;
        fprintf(stderr, "[coop] the world goes on behind the Tome\n");
    }
    g_tw.ticks++;
    if (g_tw.world_tick && g_tw.worlds_left > 1u) {
        g_tw.worlds_left--;  // another of the world's
        return;
    }
    tw_block(cpu, g_tw.world_tick ? 0 : 1, g_tw.world_tick ? 1 : 0);
    if (g_tw.world_tick) {
        const u32 lr = cpu->lr;
        u32 ignore = 0u;
        const u32 none[3] = {0u, 0u, 0u};
        hle_guest_call(cpu, 0x801E58C8u, none, 3, 0.0, &ignore);  // its fog, as its start set
        cpu->lr = lr;
        tw_switches(cpu, g_tw.world, g_tw.tome);
        g_tw.world[TW_SWITCHES] = g_tw.world[TW_SWITCHES + 1u] = 0u;
        g_tw.world[TW_OBJECTS_OFF] = 0u;  // the Tome's own (0x8014ED04): kept, nobody moved or was drawn, then or after it
        mem_write32(cpu, sda - 26240u, 3u);
        mem_write32(cpu, sda - 26188u, 3u);
        mem_write32(cpu, sda - 26192u, TW_TOME_FRAME);
        if (g_coop_tome_owner == 1)
            coop_tome_player(cpu, true);
    } else {
        if (g_coop_tome_owner == 1)
            coop_tome_player(cpu, false);
        tw_switches(cpu, g_tw.tome, g_tw.world);
        mem_write32(cpu, sda + (u32)g_menu_flag_offset, 0u);
        mem_write32(cpu, sda + (u32)g_menu_depth_offset, 0u);
        mem_write32(cpu, sda - 26240u, 2u);
        mem_write32(cpu, sda - 26188u, 2u);
        mem_write32(cpu, sda - 26192u, TW_WORLD_FRAME);
    }
    g_tw.world_tick = !g_tw.world_tick;
    // ED_TW_WORLD_FRAMES=N (testing): the world's frames to one of the Tome's.
    const unsigned s_world_frames = getenv("ED_TW_WORLD_FRAMES") != NULL ? (unsigned)atoi(getenv("ED_TW_WORLD_FRAMES")) : TW_WORLD_FRAMES;
    g_tw.worlds_left = s_world_frames;
    dol_aurora_split_skip_views(g_tw.world_tick ? 0 : (g_coop_tome_owner == 1 ? 2 : 1));
    dol_aurora_frame_unpaced(true);
    aurora_split_only_view(g_tome_copy_open ? 2 : g_coop_tome_owner >= 0 ? 1 - g_coop_tome_owner : -1);
}

static void cheat_apply(CPUState* cpu);
static void coop_own_stats(CPUState* cpu, u32 who);

// Player 2's own prompt ("B Examine"). A zone coming into the player's look
// (the scan) asks for its prompt (0x80117404(button, id, script, script,
// priority, slot, ..): into one of six slots, 0x80335A38, 28 bytes each:
// +0 the priority, +8 the id, +12 the script, -1 ids empty), which stays
// until the zone is left; each frame the first slot in use is the game's
// one prompt, a "ButtonText" element (0x801DB374; +8, +10 where it is, at
// the top right) in player 1's view. The look is made for player 2 too, in
// their turn, with six slots of their own in the game's place (and the
// look's words kept as they were); the first of theirs in use is shown in
// their own element at the top left of their half (coop_prompt_show).
#define PROMPT_ASK 0x80117404u
#define PROMPT_CLEAR 0x80117360u
#define PROMPT_LINE 0x801389DCu
#define PROMPT_MAKE 0x801DB374u
#define PROMPT_FREE 0x801DA4D8u
#define PROMPT_STYLE 0x80252D68u   // "ButtonText"
#define PROMPT_SLOTS 0x80335A38u
#define PROMPT_SLOTS_SIZE (7u * 28u)  // the six and the game's prompt
#define PROMPT_SCREEN 24036u          // r13 -: the screen the prompt's element is on
#define COOP_PROMPT_AT (4096 - 120)   // HUD units: COOP_BAR_AWAY (their half) and as far in from its left edge as player 1's from the right
// The prompts that take the player to another room (the ButtonText list:
// 1 OPEN_DOOR, 2 TELEPORT, 3 CLIMB_UP, 4 CLIMB_DOWN, 10 ENTER_SECRET_PASSAGE).
static bool coop_prompt_is_exit(s32 id) {
    return id == 1 || id == 2 || id == 3 || id == 4 || id == 10;
}
static bool g_coop_prompt_look;       // player 2's look is running
static s32 g_coop_prompt = -1;        // player 2's prompt, -1 none
static s32 g_coop_prompt_script;      // the script that asked for it, run on use
static u8 g_coop_prompt_slots[PROMPT_SLOTS_SIZE];  // player 2's six (and record), out of their look
static bool g_coop_prompt_slots_set;
static struct {
    u32 element, screen, p2;
    s32 id;
} g_coop_prompt_shown = {0u, 0u, 0u, -1};

// Player 2's six slots, all empty (as 0x80116E70 empties one), the game's
// prompt none.
static void coop_prompt_slots_clear(void) {
    memset(g_coop_prompt_slots, 0, sizeof g_coop_prompt_slots);
    for (u32 i = 0; i < 7u; i++) {
        u8* slot = g_coop_prompt_slots + i * 28u;
        for (u32 at = 4u; at <= 20u; at += (at == 8u ? 12u : 4u))
            memset(slot + at, 0xFF, 4u);  // +4, +8, +20
    }
    g_coop_prompt_slots_set = true;
}

// Their prompt to talk (coop_talk_check): it goes unless asked for again.
#define COOP_TALK_PROMPT 22
#define COOP_TALK_SCRIPT 1902
static unsigned long long g_coop_talk_frame;

static void coop_prompt_slot_clear(s32 prompt, s32 script_of) {
    for (u32 i = 0u; i < 6u; i++) {
        u8* slot = g_coop_prompt_slots + i * 28u;
        const s32 id = (s32)((u32)slot[8] << 24 | (u32)slot[9] << 16 | (u32)slot[10] << 8 | slot[11]);
        const s32 script = (s32)((u32)slot[12] << 24 | (u32)slot[13] << 16 | (u32)slot[14] << 8 | slot[15]);
        if (id != prompt || script != script_of)
            continue;
        memset(slot, 0, 28u);
        for (u32 at = 4u; at <= 20u; at += (at == 8u ? 12u : 4u))
            memset(slot + at, 0xFF, 4u);
    }
}

static void coop_talk_slot_clear(void) {
    coop_prompt_slot_clear(COOP_TALK_PROMPT, COOP_TALK_SCRIPT);
}

// Things lying about are looked for in each player's frame, not by a zone
// (0x800C030C(node), which asks for the prompt: 19, script 2246). Player 2's frame
// runs outside their turn, with player 1's prompts in: player 1 was offered what
// player 2 stood by, and player 2 nothing. It is looked for in their look, with
// their prompts in, and goes unless asked for again.
#define COOP_THING_LOOK 0x800C030Cu
#define COOP_THING_PROMPT 19
#define COOP_THING_SCRIPT 2246

static void coop_thing_look(CPUState* cpu) {
    g_hle_passthrough = g_coop_p2 == 0u || !g_settings.mod_coop || cpu->gpr[3] != g_coop_p2 || g_coop_prompt_look;
    if (g_hle_passthrough)
        return;
    cpu->gpr[3] = 0u;
    g_hle_tail_call = false;
}

// Player 2's look, as the player's step makes it (info = 0x80123F70(object);
// scan(object, info[180], info[176], out)), in their turn.
// What player 2's look looks along, as the character step makes it: where
// they were to where they are, each less (c, c, their height: object +628),
// c the step's (r2 - 23120), made into the step's line (0x801389DC(line,
// from, to)) below the stack. Their last place kept from their last look.
static bool g_coop_arrived;
static u32 coop_look_line(CPUState* cpu, u32 object) {
    static float was[3];
    static u32 was_for;
    // Just brought to a new room (coop_follow): where they were is the old room's.
    if (was_for != object || g_coop_arrived)
        for (u32 i = 0; i < 3u; i++)
            was[i] = guest_read_f32(cpu, object + i * 4u);
    was_for = object;
    const u32 from = (cpu->gpr[1] - 0x600u) & ~15u, to = from + 16u, line = from + 32u;
    // The step's line runs at their waist, height above them: the zones it
    // crosses (the cameras' among them, as a wire across the floor) test that.
    // Below the floor, only what they stand in was found: the Original camera
    // needs the rest.
    const float c = guest_read_f32(cpu, cpu->gpr[2] - 23120u), height = guest_read_f32(cpu, object + 628u);
    const float less[3] = {c, c, g_settings.mod_p2_camera == 2 ? -height : height};
    for (u32 i = 0; i < 3u; i++) {
        const float now = guest_read_f32(cpu, object + i * 4u);
        guest_write_f32(cpu, from + i * 4u, was[i] - less[i]);
        guest_write_f32(cpu, to + i * 4u, now - less[i]);
        was[i] = now;
    }
    const u32 make_line[3] = {line, from, to};
    u32 ignore = 0u;
    hle_guest_call(cpu, PROMPT_LINE, make_line, 3, 0.0, &ignore);
    return line;
}

// Each zone keeps who stands in it (+104, bit 0 the player's), from which
// the scan runs its script for coming in, staying or leaving (0x80134684;
// leaving clears the scripts' states, 0x801170D4), and the prompt's asking
// reads it (0x80117404). Player 2's look and use, with them the player, have
// their own: one shared, each player's look undid the other's every frame (a
// thing by player 2 gave player 1 its prompt, and what player 2 began there
// was forgotten). Theirs start clear in a new room.
#define COOP_ZONE_SIZE 116u   // a room's zones (a character's info +180, +176 how many)
#define COOP_ZONE_INSIDE 104u
#define COOP_ZONES_MAX 256u
// What a zone's coming in sets the camera to (the scripts' 0x8016839C and
// 0x80168450 write the two at 0x80602B18 and 0x80603178, 0x660 bytes each,
// with two small ones after). Each player has their own: player 2's are in
// for their look, and for a close-up of theirs (the thing they take seen as
// their zone sees it), player 1's otherwise.
#define COOP_ZONE_CAMERAS 0x80602B18u
#define COOP_ZONE_CAMERAS_SIZE 0xDC8u
static struct {
    u8 p2[COOP_ZONE_CAMERAS_SIZE], p1[COOP_ZONE_CAMERAS_SIZE];
    u8 p1_prompts[PROMPT_SLOTS_SIZE];
    bool p2_set, lent;
} g_coop_cameras;

// Player 2's cameras in for their close-up (on) or player 1's back, and
// their prompts: what a close-up of theirs asks for is theirs, and took player
// 1's place (Examine for player 1, at their door).
bool hle_coop_cameras_lent(void) { return g_coop_cameras.lent; }

static void coop_cameras_lend(CPUState* cpu, bool on) {
    if (on == g_coop_cameras.lent || (on && !g_coop_cameras.p2_set))
        return;
    g_coop_cameras.lent = on;
    coop_bytes(cpu, COOP_ZONE_CAMERAS, on ? g_coop_cameras.p1 : g_coop_cameras.p2, COOP_ZONE_CAMERAS_SIZE, true);
    coop_bytes(cpu, COOP_ZONE_CAMERAS, on ? g_coop_cameras.p2 : g_coop_cameras.p1, COOP_ZONE_CAMERAS_SIZE, false);
    if (on && !g_coop_prompt_slots_set)
        coop_prompt_slots_clear();
    coop_bytes(cpu, PROMPT_SLOTS, on ? g_coop_cameras.p1_prompts : g_coop_prompt_slots, PROMPT_SLOTS_SIZE, true);
    coop_bytes(cpu, PROMPT_SLOTS, on ? g_coop_prompt_slots : g_coop_cameras.p1_prompts, PROMPT_SLOTS_SIZE, false);
    static u8 said[PROMPT_SLOTS_SIZE];
    if (coop_trace() && memcmp(said, g_coop_cameras.p1_prompts, sizeof said) != 0) {
        memcpy(said, g_coop_cameras.p1_prompts, sizeof said);
        const u8* p = g_coop_cameras.p1_prompts;
        fprintf(stderr, "[trace] cameras %s; player 1's prompts:", on ? "lent" : "back");
        for (u32 i = 0u; i < 6u; i++, p += 28u)
            fprintf(stderr, " %d/%d", (s32)((u32)p[8] << 24 | (u32)p[9] << 16 | (u32)p[10] << 8 | p[11]), (s32)((u32)p[12] << 24 | (u32)p[13] << 16 | (u32)p[14] << 8 | p[15]));
        fprintf(stderr, " frame %llu\n", (unsigned long long)(cpu->timebase / 675000ull));
    }
}
static struct {
    u32 list, p1[COOP_ZONES_MAX], p2[COOP_ZONES_MAX];
} g_coop_zones;

// What each zone's script is at: its word in the table at 0x805CAA40 (ten
// of 8 bytes: script, value, a timer, a flag), set by the script
// (0x80168B64 -> 0x80163C6C) and read when it runs again (0x80168C24); a
// timer counting down runs it again by itself (0x80163E00, each frame).
// The scripts take the value they left as what has happened since: the
// ladder's 1 is "its prompt shown", so run again at 1 it takes the press as
// made (the climb). Shared, player 2 coming into the ladder's zone ran it at
// player 1's 1 and climbed, with no press; each player has their own.
// Player 2's timers count down here and run their script as theirs
// (hle_coop_controller).
#define SCRIPT_STATES 0x805CAA40u
#define SCRIPT_STATES_SIZE 80u
static struct {
    u8 p1[SCRIPT_STATES_SIZE], p2[SCRIPT_STATES_SIZE];
} g_coop_states;

// Player 2's word for a script into the game's table (in place of the
// game's own for it, or the first free), out of theirs.
static void coop_state_to_game(CPUState* cpu, s32 script) {
    u8* theirs = NULL;
    for (u32 i = 0; i < 10u && theirs == NULL; i++)
        if ((s16)(g_coop_states.p2[i * 8u] << 8 | g_coop_states.p2[i * 8u + 1u]) == script)
            theirs = g_coop_states.p2 + i * 8u;
    if (theirs == NULL)
        return;
    u32 at = 0u;
    for (u32 i = 0; i < 10u && at == 0u; i++)
        if ((s16)mem_read16(cpu, SCRIPT_STATES + i * 8u) == script)
            at = SCRIPT_STATES + i * 8u;
    for (u32 i = 0; i < 10u && at == 0u; i++)
        if (mem_read16(cpu, SCRIPT_STATES + i * 8u) == 0u)
            at = SCRIPT_STATES + i * 8u;
    if (at == 0u)
        return;
    coop_bytes(cpu, at, theirs, 8u, false);
    memset(theirs, 0, 8u);
}

static void coop_states_swap(CPUState* cpu, bool to_p2) {
    coop_bytes(cpu, SCRIPT_STATES, to_p2 ? g_coop_states.p1 : g_coop_states.p2, SCRIPT_STATES_SIZE, true);
    coop_bytes(cpu, SCRIPT_STATES, to_p2 ? g_coop_states.p2 : g_coop_states.p1, SCRIPT_STATES_SIZE, false);
}

static unsigned long long g_coop_own_view_frame;  // when their Original camera last stepped

// A new room's zones: what player 2 had of the old room's goes. Asked before
// their cameras come in: the old room's stayed, their rails with them, until
// a zone set others (a look at nothing, or worse).
static void coop_zones_new(CPUState* cpu, u32 info) {
    const u32 zones = mem_read32(cpu, info + 180u);
    if (zones != g_coop_zones.list) {
        g_coop_zones.list = zones;
        memset(g_coop_zones.p2, 0, sizeof g_coop_zones.p2);
        coop_prompt_slots_clear();
        g_coop_cameras.p2_set = false;
        g_coop_own_view_frame = 0ull;
        memset(g_coop_states.p2, 0, sizeof g_coop_states.p2);
    }
}

// Player 1 is in a new room and player 2 isn't brought yet (coop_follow, after their turn): their
// place is the old room's numbers, which mean somewhere else in this one. Looked at there, they
// "walked into" whatever zones those numbers fall in: Anthony's stairs room gave them Camera07 from
// where they stood in the room before, and on the stairs nothing changed it.
#define COOP_OBJECT_ROOM 592u
static bool coop_left_behind(CPUState* cpu) {
    const u32 o1 = g_coop_turn_p1 != 0u ? mem_read32(cpu, g_coop_turn_p1 + 52u) : 0u, o2 = mem_read32(cpu, g_coop_p2 + 52u);
    if (!guest_ram_range_valid(cpu, o1, COOP_OBJECT_ROOM + 4u) || !guest_ram_range_valid(cpu, o2, COOP_OBJECT_ROOM + 4u))
        return false;
    const u32 room = mem_read32(cpu, o1 + COOP_OBJECT_ROOM);
    return room != 0u && mem_read32(cpu, o2 + COOP_OBJECT_ROOM) != room;
}

// Player 2's zone words in (to_p2) or player 1's back.
static void coop_zones_swap(CPUState* cpu, u32 info, bool to_p2) {
    const u32 zones = mem_read32(cpu, info + 180u), n = mem_read16(cpu, info + 176u);
    const u32 count = n < COOP_ZONES_MAX ? n : COOP_ZONES_MAX;
    if (!guest_ram_range_valid(cpu, zones, count * COOP_ZONE_SIZE))
        return;
    coop_zones_new(cpu, info);
    for (u32 i = 0; i < count; i++) {
        const u32 at = zones + i * COOP_ZONE_SIZE + COOP_ZONE_INSIDE;
        u32* keep = to_p2 ? g_coop_zones.p1 : g_coop_zones.p2;
        keep[i] = mem_read32(cpu, at);
        mem_write32(cpu, at, to_p2 ? g_coop_zones.p2[i] : g_coop_zones.p1[i]);
    }
}

// Player 2's Original camera: the game's camera step (0x801EB9A0(3): the
// cameras' records move, the result to the view at 0x803012E0: eye, target,
// up) run again in their turn, with them the player and their records in. What
// it leaves there is their view, for the split (camera.c); the game's stays
// player 1's.
#define CAMERA_STEP 0x801EB9A0u
#define CAMERA_VIEW 0x803012E0u
#define CAMERA_VIEW_SIZE 52u
#define CAMERA_VIEW_FOV 48u
static float g_coop_own_view[9];
static float g_coop_own_zoom = 1.0f;  // their field of view, in the one the frame is drawn with

static bool g_coop_own_step;

// Player 1's records name player 1: set in player 2's turn (who is the player then,
// with player 1's records in), the game's camera followed player 2 until a new room.
static void coop_cameras_own(CPUState* cpu) {
    const u32 p1 = g_coop_turn_p1 != 0u ? mem_read32(cpu, g_coop_turn_p1 + 52u) : 0u,
              them = mem_read32(cpu, g_coop_p2 + 52u);
    if (p1 == 0u || them == 0u || p1 == them)
        return;
    for (u32 at = 0u; at < COOP_ZONE_CAMERAS_SIZE; at += 4u)
        if (mem_read32(cpu, COOP_ZONE_CAMERAS + at) == them) {
            mem_write32(cpu, COOP_ZONE_CAMERAS + at, p1);
            fprintf(stderr, "[coop] player 1's camera record 0x%08X named player 2: theirs again (frame %llu, Tome %d)\n",
                    COOP_ZONE_CAMERAS + at, (unsigned long long)(cpu->timebase / 675000ull), hle_coop_tome_shares());
        }
}

static void coop_camera_step(CPUState* cpu) {
    if (g_coop_cameras.lent)
        return;
    coop_cameras_own(cpu);
    if (g_settings.mod_p2_camera != 2 || coop_left_behind(cpu))
        return;
    static u8 cameras[COOP_ZONE_CAMERAS_SIZE];
    u8 view[CAMERA_VIEW_SIZE];
    coop_bytes(cpu, COOP_ZONE_CAMERAS, cameras, sizeof cameras, true);
    coop_bytes(cpu, CAMERA_VIEW, view, sizeof view, true);
    if (g_coop_cameras.p2_set)
        coop_bytes(cpu, COOP_ZONE_CAMERAS, g_coop_cameras.p2, sizeof g_coop_cameras.p2, false);
    // The records name whom they follow, each as it was set for the player:
    // their copies follow them.
    const u32 p1 = g_coop_turn_p1 != 0u ? mem_read32(cpu, g_coop_turn_p1 + 52u) : 0u,
              them = mem_read32(cpu, g_coop_p2 + 52u);
    for (u32 at = 0u; p1 != 0u && at < COOP_ZONE_CAMERAS_SIZE; at += 4u)
        if (mem_read32(cpu, COOP_ZONE_CAMERAS + at) == p1)
            mem_write32(cpu, COOP_ZONE_CAMERAS + at, them);
    const u32 mask = 3u;
    u32 ignore = 0u;
    const float game_fov = guest_read_f32(cpu, CAMERA_VIEW + CAMERA_VIEW_FOV);
    g_coop_own_step = true;
    const bool stepped = hle_guest_call(cpu, CAMERA_STEP, &mask, 1, 0.0, &ignore);
    g_coop_own_step = false;
    if (stepped) {
        static float eye_was[3];
        const float jump[3] = {guest_read_f32(cpu, CAMERA_VIEW) - eye_was[0], guest_read_f32(cpu, CAMERA_VIEW + 4u) - eye_was[1], guest_read_f32(cpu, CAMERA_VIEW + 8u) - eye_was[2]};
        const float moved = sqrtf(jump[0] * jump[0] + jump[1] * jump[1] + jump[2] * jump[2]);
        if (coop_trace() && moved > 60.0f)
            fprintf(stderr, "[trace] player 2's camera jumps %.0f in a frame (frame %llu)\n", moved, (unsigned long long)(cpu->timebase / 675000ull));
        for (u32 i = 0; i < 3u; i++)
            eye_was[i] = guest_read_f32(cpu, CAMERA_VIEW + i * 4u);
        for (u32 i = 0; i < 9u; i++)
            g_coop_own_view[i] = guest_read_f32(cpu, CAMERA_VIEW + i * 4u);
        g_coop_own_view_frame = cpu->timebase / 675000ull;
        // The game's zones set the field of view too (a close camera's narrow
        // one); one is drawn with, player 1's. Theirs, in that.
        const float own_fov = guest_read_f32(cpu, CAMERA_VIEW + CAMERA_VIEW_FOV);
        const float half = 3.14159265f / 360.0f;
        g_coop_own_zoom = game_fov > 1.0f && game_fov < 170.0f && own_fov > 1.0f && own_fov < 170.0f
                              ? tanf(game_fov * half) / tanf(own_fov * half)
                              : 1.0f;
    }
    coop_bytes(cpu, COOP_ZONE_CAMERAS, g_coop_cameras.p2, sizeof g_coop_cameras.p2, true);
    g_coop_cameras.p2_set = true;
    coop_bytes(cpu, COOP_ZONE_CAMERAS, cameras, sizeof cameras, false);
    coop_bytes(cpu, CAMERA_VIEW, view, sizeof view, false);
}

// In a close-up of player 2's the game's camera is theirs, and player 1's is stepped as player 2's is
// at other times (coop_camera_step): the same step in player 1's turn, with their records in and them
// the player. What it leaves is player 1's view, for their half (camera.c): their own camera, as
// before the close-up, its zones and turning with it.
static float g_coop_p1_view[9];
static float g_coop_p1_zoom = 1.0f;
static unsigned long long g_coop_p1_view_frame;

static void coop_player1_camera(CPUState* cpu) {
    if (!g_coop_cameras.lent)
        return;
    static u8 theirs[COOP_ZONE_CAMERAS_SIZE];
    u8 view[CAMERA_VIEW_SIZE];
    coop_bytes(cpu, COOP_ZONE_CAMERAS, theirs, sizeof theirs, true);
    coop_bytes(cpu, CAMERA_VIEW, view, sizeof view, true);
    coop_bytes(cpu, COOP_ZONE_CAMERAS, g_coop_cameras.p1, sizeof g_coop_cameras.p1, false);
    // Their records follow them: set with player 2 the player, one named player 2 (coop_cameras_own),
    // and their half looked at player 2 from wherever they were.
    const u32 p1_object = mem_read32(cpu, cpu->gpr[13] - 26524u), them = mem_read32(cpu, g_coop_p2 + 52u);
    for (u32 at = 0u; p1_object != them && at < COOP_ZONE_CAMERAS_SIZE; at += 4u)
        if (mem_read32(cpu, COOP_ZONE_CAMERAS + at) == them)
            mem_write32(cpu, COOP_ZONE_CAMERAS + at, p1_object);
    const u32 mask = 3u;
    u32 ignore = 0u;
    const float game_fov = guest_read_f32(cpu, CAMERA_VIEW + CAMERA_VIEW_FOV);
    g_coop_own_step = true;
    const bool stepped = hle_guest_call(cpu, CAMERA_STEP, &mask, 1, 0.0, &ignore);
    g_coop_own_step = false;
    if (stepped) {
        for (u32 i = 0; i < 9u; i++)
            g_coop_p1_view[i] = guest_read_f32(cpu, CAMERA_VIEW + i * 4u);
        g_coop_p1_view_frame = cpu->timebase / 675000ull;
        const float own_fov = guest_read_f32(cpu, CAMERA_VIEW + CAMERA_VIEW_FOV);
        const float half = 3.14159265f / 360.0f;
        g_coop_p1_zoom = game_fov > 1.0f && game_fov < 170.0f && own_fov > 1.0f && own_fov < 170.0f
                             ? tanf(game_fov * half) / tanf(own_fov * half)
                             : 1.0f;
    }
    coop_bytes(cpu, COOP_ZONE_CAMERAS, g_coop_cameras.p1, sizeof g_coop_cameras.p1, true);
    coop_bytes(cpu, COOP_ZONE_CAMERAS, theirs, sizeof theirs, false);
    coop_bytes(cpu, CAMERA_VIEW, view, sizeof view, false);
}

bool hle_coop_player1_camera(float eye[3], float target[3], float* zoom) {
    if (g_coop_p1_view_frame == 0ull)
        return false;
    *zoom = g_coop_p1_zoom;
    for (int i = 0; i < 3; i++) {
        eye[i] = g_coop_p1_view[i];
        target[i] = g_coop_p1_view[3 + i];
    }
    return isfinite(eye[0]) && isfinite(target[0]);
}

// Their view from a recent turn (none while their turn is not run: the game's
// close-ups keep the last).
bool hle_coop_player2_camera(float eye[3], float target[3], float up[3], float* zoom) {
    if (g_coop_own_view_frame == 0ull)
        return false;
    *zoom = g_coop_own_zoom;
    for (int i = 0; i < 3; i++) {
        eye[i] = g_coop_own_view[i];
        target[i] = g_coop_own_view[3 + i];
        up[i] = g_coop_own_view[6 + i];
    }
    return isfinite(eye[0]) && isfinite(target[0]);
}

// Brought to player 1's room, player 2 may stand in a way back (the stairs just
// come up: player 1 arrives clear of its zone, they beside them in it). Their
// first look in the room, all it finds being new to them, took that as walking
// in and sent both back, for ever. That look's asking for a room (0x801666A4,
// the script's) goes unheard; the zone counts as come into, and walked into
// again it takes them.
#define COOP_SCRIPT_GO_TO_ROOM 0x801666A4u
static bool g_coop_arrived;
// Player 2 is going through a door (until they arrive, or this frame): their turn
// runs through its fade, or its motion waited for the room after.
static unsigned long long g_coop_door_turns_until;

static bool coop_door_turns(const CPUState* cpu) {
    return g_coop_door_turns_until != 0ull && cpu->timebase / 675000ull <= g_coop_door_turns_until;
}

static void coop_script_go_to_room(CPUState* cpu) {
    g_hle_passthrough = !g_coop_prompt_look || (!g_coop_arrived && !coop_door_turns(cpu));
    if (g_hle_passthrough)
        return;
    if (getenv("ED_COOP_LOG") != NULL)
        fprintf(stderr, "[coop] player 2 arrived in a way out: its room change passed over\n");
    cpu->gpr[3] = 0u;
    g_hle_tail_call = false;
}

static void coop_prompt_look(CPUState* cpu, u32 object) {
    if (!g_coop_scan)
        return;
    u32 info = 0u, ignore = 0u;
    if (!hle_guest_call(cpu, COOP_CHARACTER_INFO, &object, 1, 0.0, &info) || !guest_ram_range_valid(cpu, info, 184u))
        return;
    static u8 words[2060], slots[PROMPT_SLOTS_SIZE], cameras[COOP_ZONE_CAMERAS_SIZE];
    coop_zones_new(cpu, info);
    if (coop_left_behind(cpu)) {
        g_coop_prompt = -1;
        g_coop_prompt_script = 0;
        return;
    }
    const u32 at = cpu->gpr[13] - 24040u;
    coop_bytes(cpu, at, words, sizeof words, true);
    coop_bytes(cpu, PROMPT_SLOTS, slots, sizeof slots, true);
    coop_bytes(cpu, COOP_ZONE_CAMERAS, cameras, sizeof cameras, true);
    if (g_coop_cameras.p2_set)
        coop_bytes(cpu, COOP_ZONE_CAMERAS, g_coop_cameras.p2, sizeof g_coop_cameras.p2, false);
    const u32 line = coop_look_line(cpu, object);
    const u32 args[4] = {object, mem_read32(cpu, info + 180u), mem_read16(cpu, info + 176u), line};
    coop_zones_swap(cpu, info, true);
    coop_states_swap(cpu, true);
    if (!g_coop_prompt_slots_set)
        coop_prompt_slots_clear();
    if (cpu->timebase / 675000ull > g_coop_talk_frame + 2ull)
        coop_talk_slot_clear();
    coop_bytes(cpu, PROMPT_SLOTS, g_coop_prompt_slots, sizeof g_coop_prompt_slots, false);
    g_coop_prompt_look = true;
    hle_guest_call(cpu, COOP_SCAN, args, 4, 0.0, &ignore);
    if (mem_read32(cpu, cpu->gpr[13] - 21492u) == g_coop_p2) {
        coop_bytes(cpu, PROMPT_SLOTS, g_coop_prompt_slots, sizeof g_coop_prompt_slots, true);
        coop_prompt_slot_clear(COOP_THING_PROMPT, COOP_THING_SCRIPT);
        coop_bytes(cpu, PROMPT_SLOTS, g_coop_prompt_slots, sizeof g_coop_prompt_slots, false);
        hle_guest_call(cpu, COOP_THING_LOOK, &g_coop_p2, 1, 0.0, &ignore);
    }
    g_coop_prompt_look = false;
    g_coop_arrived = false;
    coop_zones_swap(cpu, info, false);
    coop_states_swap(cpu, false);
    coop_bytes(cpu, PROMPT_SLOTS, g_coop_prompt_slots, sizeof g_coop_prompt_slots, true);
    coop_words_back(cpu, words);
    coop_bytes(cpu, PROMPT_SLOTS, slots, sizeof slots, false);
    coop_bytes(cpu, COOP_ZONE_CAMERAS, g_coop_cameras.p2, sizeof g_coop_cameras.p2, true);
    g_coop_cameras.p2_set = true;
    coop_bytes(cpu, COOP_ZONE_CAMERAS, cameras, sizeof cameras, false);
    g_coop_prompt = -1;
    g_coop_prompt_script = 0;
    for (u32 i = 0; i < 6u && g_coop_prompt < 0; i++) {
        const u8* slot = g_coop_prompt_slots + i * 28u;
        const s32 id = (s32)((u32)slot[8] << 24 | (u32)slot[9] << 16 | (u32)slot[10] << 8 | slot[11]);
        const u32 timer = (u32)slot[16] << 24 | (u32)slot[17] << 16 | (u32)slot[18] << 8 | slot[19];
        if (id != -1 && timer == 0u) {
            g_coop_prompt = id;
            g_coop_prompt_script = (s32)((u32)slot[12] << 24 | (u32)slot[13] << 16 | (u32)slot[14] << 8 | slot[15]);
        }
    }
}

// Player 2 uses what they stand by as themselves. Their look (the scan, in
// their turn) has the zone come into asks for its prompt, the asking
// script's index with it (their slot's +12); the player's own use of a
// prompt (0x80006EBC, from the controller's button handlers) runs that
// script again (0x801642D4(script, 0, 0)), which goes on from the state it
// keeps (0x80168C24 / 0x80168B64: the script's word in the table at
// 0x805CAA40): the close-up, the thing taken. Player 2's press does the
// same with their own prompt's script. Run from their turn (a host call) a
// script broke the game's threads, so it runs at player 1's controller
// instead (hle_coop_controller): the game's own thread, as the book reload
// does, runs the script with them the player (coop_use_start), then player
// 1 and the controller go on (continue_coop_use). The input stays the
// game's: what the use opens (a thing taken) hears the game's pad, which is
// player 2's while the close-up is theirs (g_coop_assist_at,
// coop_menu_input).
#define COOP_RUN_SCRIPT 0x801642D4u
static bool g_coop_use_own;      // the hooks are in: player 2 uses things as themselves
static s32 g_coop_use_script;    // their press: the script to run at player 1's controller (0: none)
static s32 g_coop_use_prompt;    // and the prompt it was pressed at
static unsigned g_coop_assist_at;
static struct {
    bool active, resumed, p2, found, at_press, lent_before;
    u32 resume, was[5], info;
    s32 script;  // what it last ran
    HleSavedContext saved;
} g_coop_use;

// What is found is gone for both: the script that gives it (0x80024528, the
// script running at r13 - 23228) left its word in the table of whoever took
// it; the same word goes into the other player's, or they were offered it
// again (the Tome taken twice).
#define RUNNING_SCRIPT 23228u

static struct {
    bool pending, p2;
    s32 script;
} g_coop_found_word;

// What player 2 opens is theirs from this frame: the split's look at it had gone by
// for this one, and player 1's half showed its words for a frame.
#ifdef ED_AURORA
void aurora_split_hold(int player);
static void coop_hold_player1(void) {}  // their half follows them (camera_split_update)
#else
static void coop_hold_player1(void) {}
#endif

static s32 g_coop_scene_script;     // the script of player 2's whose cutscene has just played, or is playing
static unsigned g_coop_scene_after; // pad reads since it ended
static bool g_coop_cutscene_pending;  // g_coop_cutscene.pending, as told here

static void coop_found_notice(CPUState* cpu, bool p2);
// The thing's object and the second it may have with it, as the find's opening finds them
// (0x80024584 on), for the finder alone. r6 the thing's id, r5 the finder's.
static void coop_found_for_finder(CPUState* cpu, bool p2) {
    u32 keep[13];
    for (int i = 0; i < 13; i++)
        keep[i] = cpu->gpr[i];
    const u32 lr = cpu->lr, id = cpu->gpr[6];
    u32 node = 0u, info = 0u, second = 0u, node2 = 0u;
    g_coop_found_objects[0] = g_coop_found_objects[1] = 0u;
    if (hle_guest_call(cpu, 0x801F5F88u, &id, 1, 0.0, &node) && guest_ram_range_valid(cpu, node, 64u)) {
        g_coop_found_objects[0] = mem_read32(cpu, node + 52u);
        if (hle_guest_call(cpu, 0x801F6398u, &node, 1, 0.0, &info) && hle_guest_call(cpu, 0x8015095Cu, &info, 1, 0.0, &second) &&
            (s32)second > 0 && hle_guest_call(cpu, 0x801F5F88u, &second, 1, 0.0, &node2) && guest_ram_range_valid(cpu, node2, 64u))
            g_coop_found_objects[1] = mem_read32(cpu, node2 + 52u);
    }
    for (int i = 0; i < 13; i++)
        cpu->gpr[i] = keep[i];
    cpu->lr = lr;
    g_coop_found_out_of = g_coop_found_objects[0] != 0u ? (p2 ? 0 : 1) : -1;
    g_coop_found_until = 0ull;
    if (getenv("ED_COOP_LOG") != NULL)
        fprintf(stderr, "[coop] the find's thing 0x%08X 0x%08X is drawn for player %d alone\n", g_coop_found_objects[0], g_coop_found_objects[1], p2 ? 2 : 1);
}

// ED_BOTH=path: one row a frame, player 1 and player 2 side by side, for tools/ed (ed check, ed see).
// It only reads: a log that calls the game's code changes the game (docs/coop-failed-attempts.md).
// The columns are named in the file's first line, and tools/ed/recording.py reads them by name.
// The motion a player's object plays: 0x801222F0, by reading.
static int both_motion(CPUState* cpu, u32 node) {
    if (node == 0u || !guest_ram_range_valid(cpu, node + 52u, 4u))
        return -1;
    const u32 object = mem_read32(cpu, node + 52u);
    if (!guest_ram_range_valid(cpu, object + 64u, 4u))
        return -1;
    const u32 block = mem_read32(cpu, object + 64u);
    if (!guest_ram_range_valid(cpu, block + 2176u, 2u))
        return -1;
    const u32 channel = block + mem_read16(cpu, block + 2176u) * 272u;
    if (!guest_ram_range_valid(cpu, channel + 184u, 4u))
        return -1;
    const u32 motion = mem_read32(cpu, channel + 184u);
    return motion != 0u && guest_ram_range_valid(cpu, motion, 4u) ? (s32)mem_read32(cpu, motion) : -1;
}

static void both_record(CPUState* cpu, const DolPadState* p1) {
    static FILE* f;
    static int on = -1;
    static unsigned long long last = ~0ull;
    if (on < 0) {
        const char* path = getenv("ED_BOTH");
        on = path != NULL && (f = fopen(path, "w")) != NULL;
        if (on)
            fprintf(f, "frame\tms\troom\tentry\twho\tctl\tp1x\tp1y\tp1z\tp1st\tp1hp\tp1m\tp2x\tp2y\tp2z\tp2st\tp2hp\tp2m\t"
                       "p1b\tp1sx\tp1sy\tp2b\tp2sx\tp2sy\tex\tey\tez\ttx\tty\ttz\tfov\t"
                       "p1ex\tp1ey\tp1ez\tp1tx\tp1ty\tp1tz\tp2ex\tp2ey\tp2ez\tp2tx\tp2ty\tp2tz\t"
                       "busy\thold\tfocus\tfocus2\twindow\tscript\tmode\tmode2\t"
                       "screen\towner\tshares\tcopy\tcopyp1\tlent\tcutscene\tmenu\t"
                       "ticket\tfade\tp1anim\tp2anim\tcamidx\tcamhash\tp1carry\tp2carry\n");
    }
    if (!on || !sda_ready(cpu))
        return;
    const unsigned long long frame = cpu->timebase / 675000ull;
    if (frame == last)
        return;
    last = frame;
    const u32 sda = cpu->gpr[13];
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    fprintf(f, "%llu\t%lld\t%d\t0x%08X\t0x%X\t0x%X", frame, (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000,
            (s32)mem_read32(cpu, sda + (u32)(s32)g_coop_room_offset), mem_read32(cpu, 0x8058B018u), mem_read32(cpu, sda - 21492u), mem_read32(cpu, sda - 21488u));
    u32 them = coop_player(cpu);
    if (g_coop_p2 != 0u && them == g_coop_p2)  // player 2 is "the player" in a use of theirs
        them = g_coop_use.active && g_coop_use.p2 ? g_coop_use.was[1] : g_coop_turn_p1;
    const u32 nodes_of_both[2] = {them, g_coop_p2};
    for (int i = 0; i < 2; i++) {
        const u32 node = nodes_of_both[i];
        const u32 object = node != 0u && guest_ram_range_valid(cpu, node + 52u, 4u) ? mem_read32(cpu, node + 52u) : 0u;
        float at[3] = {0.0f, 0.0f, 0.0f};
        if (guest_ram_range_valid(cpu, object, 16u))
            for (u32 k = 0u; k < 3u; k++)
                at[k] = guest_read_f32(cpu, object + k * 4u);
        int state = -1, health = -1, model = -1;
        if (object != 0u) {
            const u32 actor = mem_read32(cpu, node + (u32)(s32)g_coop_actor_offset);
            const u32 stats = mem_read32(cpu, node + (u32)(s32)g_coop_stats_offset);
            state = (int)mem_read32(cpu, node + COOP_NODE_STATE);
            if (guest_ram_range_valid(cpu, stats + (u32)(s32)g_coop_health_offset, 2u))
                health = (s16)mem_read16(cpu, stats + (u32)(s32)g_coop_health_offset);
            if (guest_ram_range_valid(cpu, actor, 160u))
                model = (s16)mem_read16(cpu, actor + 154u);
        }
        fprintf(f, "\t%.1f\t%.1f\t%.1f\t%d\t%d\t%d", at[0], at[1], at[2], state, health, model);
    }
    fprintf(f, "\t0x%X\t%d\t%d\t0x%X\t%d\t%d", p1->button, p1->stick_x, p1->stick_y, g_both_p2_pad.button, g_both_p2_pad.x, g_both_p2_pad.y);
    for (u32 k = 0u; k < 6u; k++)
        fprintf(f, "\t%.1f", guest_read_f32(cpu, CAMERA_VIEW + k * 4u));
    fprintf(f, "\t%.2f", guest_read_f32(cpu, CAMERA_VIEW + CAMERA_VIEW_FOV));
    for (u32 k = 0u; k < 6u; k++)
        fprintf(f, "\t%.1f", g_coop_p1_view_frame != 0ull ? g_coop_p1_view[k] : 0.0f);
    for (u32 k = 0u; k < 6u; k++)
        fprintf(f, "\t%.1f", g_coop_own_view_frame != 0ull ? g_coop_own_view[k] : 0.0f);
    fprintf(f, "\t0x%X\t0x%X\t0x%X\t0x%X\t0x%X\t%d\t%d\t%d", mem_read32(cpu, sda - 23344u), mem_read32(cpu, sda - 25696u),
            mem_read32(cpu, sda - 23552u), mem_read32(cpu, sda - 23556u), mem_read32(cpu, sda - 25960u),
            g_both_script, (s32)mem_read32(cpu, sda - 26188u), (s32)mem_read32(cpu, sda - 24244u));
    fprintf(f, "\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d", g_coop_screen, g_coop_tome_owner, hle_coop_tome_shares(), g_tome_copy_open,
            g_tome_copy_p1, g_coop_cameras.lent, cutscene_playing(cpu), game_menu_open(cpu));
    // The room change's countdown (ticket +0), the fade's ticks, and the motion each player is playing (0x801222F0, by reading).
    fprintf(f, "\t%d\t%d\t%d\t%d", (s32)mem_read32(cpu, 0x8058AFFCu), (s32)mem_read32(cpu, sda - 21784u),
            both_motion(cpu, nodes_of_both[0]), both_motion(cpu, nodes_of_both[1]));
    // Which camera the game's own records are on: the current index's eye record (0x80603178 + n*136), +124 the room's entry, its +52 hash.
    const s32 camera_index = (s32)mem_read32(cpu, sda - 26864u);
    u32 camera_hash = 0u;
    if (camera_index >= 0 && camera_index < 12) {
        const u32 entry = mem_read32(cpu, 0x80603178u + (u32)camera_index * 136u + 124u);
        if (guest_ram_range_valid(cpu, entry + 52u, 4u))
            camera_hash = mem_read32(cpu, entry + 52u);
    }
    fprintf(f, "\t%d\t0x%X", camera_index, camera_hash);
    // How much each carries (node +48, its count at +8), to see an item leave.
    for (int i = 0; i < 2; i++) {
        const u32 node = nodes_of_both[i];
        const u32 list = node != 0u && guest_ram_range_valid(cpu, node + 48u, 4u) ? mem_read32(cpu, node + 48u) : 0u;
        fprintf(f, "\t%d", guest_ram_range_valid(cpu, list + 8u, 4u) ? (s32)mem_read32(cpu, list + 8u) : -1);
    }
    fprintf(f, "\n");
    fflush(f);  // a freeze keeps every row up to it
}

// ED_RUN_SCRIPT=DELAY:SCRIPT[,DELAY:SCRIPT...]: for tests. Runs a level's script as a zone would, DELAY frames after the run starts
// (the same script twice is walking into its zone, then pressing on its prompt). Nothing happens without the variable.
static void test_run_script(CPUState* cpu) {
    static int on = -1;
    static char list[8192];
    static unsigned long long first;
    if (on < 0) {
        const char* text = getenv("ED_RUN_SCRIPT");
        on = text != NULL;
        if (on)
            snprintf(list, sizeof list, "%s", text);
    }
    if (!on || !sda_ready(cpu))
        return;
    const unsigned long long frame = cpu->timebase / 675000ull;
    if (first == 0ull)
        first = frame;
    int delay = 0, script = 0, used = 0;
    if (sscanf(list, "%d:%d%n", &delay, &script, &used) != 2) {
        on = 0;
        return;
    }
    if (frame - first < (unsigned long long)delay || mem_read32(cpu, cpu->gpr[13] - RUNNING_SCRIPT) != 0u)
        return;
    u32 args[3] = {(u32)script, 0u, 0u};
    u32 ignore = 0u;
    fprintf(stderr, "[test] frame %llu: running script %d\n", frame, script);
    hle_guest_call(cpu, COOP_RUN_SCRIPT, args, 3, 0.0, &ignore);
    const char* rest = list + used;
    memmove(list, *rest == ',' ? rest + 1 : rest, strlen(rest) + 1);
}

static void hle_coop_found(CPUState* cpu) {
    if (g_coop_p2 == 0u || !g_settings.mod_coop || !sda_ready(cpu))
        return;
    const bool finder_p2 = (g_coop_use.active && g_coop_use.p2) || cpu->gpr[5] == mem_read32(cpu, g_coop_p2 + 16u);
    coop_found_notice(cpu, finder_p2);
    coop_found_for_finder(cpu, finder_p2);
    // What player 2 finds is the players' together: both Tomes list player 1's
    // things (their own copies of weapons apart), so in player 2's list it
    // showed in neither.
    const u32 first = g_coop_use.active && g_coop_use.p2 ? g_coop_use.was[1] : g_coop_turn_p1;
    if (cpu->gpr[5] == mem_read32(cpu, g_coop_p2 + 16u) && guest_ram_range_valid(cpu, first, 20u))
        cpu->gpr[5] = mem_read32(cpu, first + 16u);
    const s16 script = (s16)mem_read32(cpu, cpu->gpr[13] - RUNNING_SCRIPT);
    u8 word[8];
    bool found = false;
    for (u32 i = 0; i < 10u && !found; i++)
        if ((s16)mem_read16(cpu, SCRIPT_STATES + i * 8u) == script) {
            coop_bytes(cpu, SCRIPT_STATES + i * 8u, word, 8u, true);
            found = true;
        }
    if (!found || script <= 0)
        return;
    u8* other = g_coop_use.active && g_coop_use.p2 ? g_coop_states.p1 : g_coop_states.p2;
    u8* at = NULL;
    for (u32 i = 0; i < 10u && at == NULL; i++)
        if ((s16)(other[i * 8u] << 8 | other[i * 8u + 1u]) == script)
            at = other + i * 8u;
    for (u32 i = 0; i < 10u && at == NULL; i++)
        if (other[i * 8u] == 0u && other[i * 8u + 1u] == 0u)
            at = other + i * 8u;
    if (at == NULL)
        return;
    memcpy(at, word, 8u);
    // The word it goes on to, as it leaves this frame (the monk's key is given at 3
    // and leaves 0: their copy of the 3, or the game's, gave it again).
    g_coop_found_word.pending = true;
    g_coop_found_word.script = script;
    g_coop_found_word.p2 = g_coop_use.active && g_coop_use.p2;
    // Player 2's find shows only in their half from its first frame: the
    // split held player 1's view with the words in it a frame later.
    if (g_coop_use.active && g_coop_use.p2)
        g_coop_use.found = true;
    // A thing given as a scene of theirs ends (the monk's key: the script goes on
    // as the game's, after the cutscene, with player 1 the player): theirs too, in
    // their half with their pad. It showed in both.
    if (!g_coop_use.active && script != 0 && script == g_coop_scene_script) {
        g_coop_scene_script = 0;
        g_coop_assist_at = 1u;
        g_coop_screen = true;
        coop_hold_player1();
        // Their cameras in, unless the cutscene's end still has them to sort out
        // (coop_cutscene_over): player 1's kept for it went for the cutscene's,
        // which follow player 2, and player 1's half showed them.
        if (!g_coop_cutscene_pending)
            coop_cameras_lend(cpu, true);
    }
    if (getenv("ED_COOP_LOG") != NULL)
        fprintf(stderr, "[coop] script %d found something: done for player %d too\n", script,
                other == g_coop_states.p1 ? 1 : 2);
}

// The script run, with player 2 the player when it is theirs (a use of
// theirs, or what a close-up of theirs left to run).
static void coop_use_start(CPUState* cpu, s32 script, bool p2) {
    if (p2 && !guest_ram_range_valid(cpu, mem_read32(cpu, g_coop_p2 + 52u), 640u))
        return;
    save_callback_context(&g_coop_use.saved, cpu);
    g_coop_use.resume = cpu->pc;
    g_coop_use.p2 = p2;
    g_coop_use.script = script;
    g_coop_use.found = false;
    if (p2) {
        coop_be_player2(cpu, true, g_coop_use.was);
        coop_kinds(cpu, true);  // what their script gives or takes is from their own counts
        coop_states_swap(cpu, true);
        // Their zone words too: what a script of theirs leaves for a zone (the
        // Tome's hand open: its zone looks again, and offers it) was player
        // 1's, who had the prompt when they came by, and player 2 none.
        const u32 object = mem_read32(cpu, g_coop_p2 + 52u);
        u32 ignore = 0u, info = 0u;
        g_coop_use.info = hle_guest_call(cpu, COOP_CHARACTER_INFO, &object, 1, 0.0, &info) &&
                                  guest_ram_range_valid(cpu, info, 184u)
                              ? info
                              : 0u;
        (void)ignore;
        if (g_coop_use.info != 0u)
            coop_zones_swap(cpu, g_coop_use.info, true);
        g_coop_assist_at = 1u;  // the close-up it opens is theirs
        // Their cameras in first: what it sets is theirs. Put in after, player
        // 1's kept the close-up's, and stayed at the thing when it was over.
        g_coop_use.lent_before = g_coop_cameras.lent;
        coop_cameras_lend(cpu, true);
    }
    g_coop_use.active = true;
    g_hle_passthrough = false;
    g_hle_tail_call = true;
    cpu->gpr[3] = (u32)script;
    cpu->gpr[4] = 0u;
    cpu->gpr[5] = 0u;
    cpu->lr = HLE_USE_RETURN;
    cpu->pc = COOP_RUN_SCRIPT;
    if (getenv("ED_COOP_LOG") != NULL)
        fprintf(stderr, "[coop] script %d runs for player %d (frame %llu)\n", script, p2 ? 2 : 1,
                (unsigned long long)(cpu->timebase / 675000ull));
}

static bool continue_coop_use(CPUState* cpu) {
    if (!g_coop_use.active)
        return false;
    g_coop_use.active = false;
    g_coop_use.resumed = !g_coop_use.at_press;
    g_coop_use.at_press = false;
    if (g_coop_use.p2) {
        coop_states_swap(cpu, false);
        if (g_coop_use.info != 0u)
            coop_zones_swap(cpu, g_coop_use.info, false);
        coop_kinds(cpu, false);
        coop_be_player2(cpu, false, g_coop_use.was);
        // What it opened is theirs from this frame on: player 1's half keeps
        // their last view from before it (not a frame with its words).
        if ((coop_busy(cpu) && !g_coop_script_hold) || g_coop_use.found) {
            g_coop_screen = true;
            coop_hold_player1();
        }
        else if (!g_coop_use.lent_before)
            coop_cameras_lend(cpu, false);
    }
    const HleSavedContext* saved = &g_coop_use.saved;
    memcpy(cpu->gpr, saved->gpr, sizeof saved->gpr);
    memcpy(cpu->fpr, saved->fpr, sizeof saved->fpr);
    memcpy(cpu->ps1, saved->ps1, sizeof saved->ps1);
    cpu->lr = saved->lr;
    cpu->ctr = saved->ctr;
    cpu->cr = saved->cr;
    cpu->xer = saved->xer;
    cpu->fpscr = saved->fpscr;
    cpu->pc = g_coop_use.resume;
    return true;
}

// A press in player 2's close-up comes through the game's pad to the game's
// use of a prompt (0x80006EBC -> the script), player 1 the player and their
// table in: the script took the press from where player 1's word had it (none:
// from its start, so what player 2 looked at never went on, and never let
// them go). It runs as theirs, then the game's use goes on.
// As for the script a window closed in their close-up runs (0x80024220; a
// thing found's, 0x80024434).
#define PROMPT_USE_RAN 0x8000704Cu
#define WINDOW_CLOSE_RAN 0x800242E4u
#define FOUND_CLOSE_RAN 0x800244C0u
// As for the script a close-up's end runs (0x80025194), which makes its look ready again
// for whoever is in its zone: run as player 1, away from it, the next press on Details
// found the look at its end, and the close-up never let them go.
#define CLOSE_UP_END_RAN 0x80025198u
#define COOP_SCRIPT_WAIT_RAN 0x80168388u

#define TOME_SCRIPT_RAN 0x800082ACu  // the world's call, as a Tome closes, of the script an item left
#define ARRIVAL_ZONE_RAN 0x80134B38u
#define ARRIVAL_TIMER_RAN 0x80163E5Cu
#define ARRIVAL_END_RAN 0x801EC1C8u
#define COOP_ARRIVAL_FRAMES 600ull
#define TOME_ITEM_CODE 0x80102000u  // the Inventory page's item actions
#define TOME_ITEM_CODE_END 0x80104800u

static s32 g_tome_copy_use_script;  // the item last used in the copy: what its script was asked
static u32 g_tome_copy_use_item;

// fn80(thing, event, w0, w1), a script sending a thing an event with two words: a thing told a script
// this way calls it when it's done (event 6 in the character's step, 0x801F7744). Who it goes back to is
// whoever was the player when it was sent.
// The game's show flags (fn111, 0x80043D04; a look's flag 4, 0x80043C88) hide "the player" so its scene's
// camera sees past them. There's one word, and it asks about whoever the frame's step has as the player:
// r13 - 26276 left player 1's body undrawn whoever pressed (0x8002794C), and r13 - 26272 stopped main's
// controller (0x80007460), and so player 2's turn too: nobody walked. Asked for in a press of a player's,
// neither word is touched, and the one who pressed goes from their own view alone (coop_found_mark).
#define COOP_SHOW_LATER 0x80043C34u
#define COOP_SHOW_NOW 0x80043D04u
#define COOP_SHOW_MAIN 0x80043C88u
#define COOP_LOOK_HIDES 0x80024F68u  // a look's opening, 0x80043C88(0)
#define COOP_LOOK_SHOWS 0x80025160u  // and its end
static bool g_coop_show_kept;
static void coop_show_asked(CPUState* cpu) {
    g_hle_passthrough = true;
    if (!g_settings.mod_coop || g_coop_p2 == 0u || !sda_ready(cpu) || cutscene_playing(cpu))
        return;
    if (cpu->gpr[3] != 0u) {
        if (!g_coop_show_kept)
            return;
        g_coop_show_kept = false;
        g_coop_unseen_out_of = -1;
    } else {
        const bool theirs = (g_coop_use.active && g_coop_use.p2) || mem_read32(cpu, cpu->gpr[13] - 21492u) == g_coop_p2;
        if (!theirs && !g_coop_p1_look && cpu->lr != COOP_LOOK_HIDES)
            return;
        if (g_coop_show_kept) {  // asked twice, as the look in a scene
            g_hle_passthrough = false;
            g_hle_tail_call = false;
            return;
        }
        u32 keep[13];
        for (int i = 0; i < 13; i++)
            keep[i] = cpu->gpr[i];
        const u32 lr = cpu->lr, node = theirs ? g_coop_p2 : mem_read32(cpu, cpu->gpr[13] - 21492u);
        const u32 id = guest_ram_range_valid(cpu, node, 64u) ? mem_read32(cpu, node + 16u) : 0u;
        memset(g_coop_unseen, 0, sizeof g_coop_unseen);
        g_coop_unseen[0] = guest_ram_range_valid(cpu, node, 64u) ? mem_read32(cpu, node + 52u) : 0u;
        // What they hold, as 0x80043DE8 lists it.
        const u32 args[2] = {id, 0u};
        u32 list = 0u, count = 0u;
        if (id != 0u && hle_guest_call(cpu, 0x80151500u, args, 2, 0.0, &list) && list != 0u && hle_guest_call(cpu, 0x80150D84u, &list, 1, 0.0, &count)) {
            int at = 1;
            for (u32 i = 0u; i < count && at < COOP_UNSEEN_MAX; i++) {
                const u32 entry[2] = {list, i};
                u32 thing = 0u, held = 0u, object = 0u;
                if (hle_guest_call(cpu, 0x80150D8Cu, entry, 2, 0.0, &thing) && hle_guest_call(cpu, 0x801F5F88u, &thing, 1, 0.0, &held) && held != 0u &&
                    hle_guest_call(cpu, 0x801F633Cu, &held, 1, 0.0, &object) && object != 0u)
                    g_coop_unseen[at++] = object;
            }
        }
        for (int i = 0; i < 13; i++)
            cpu->gpr[i] = keep[i];
        cpu->lr = lr;
        g_coop_show_kept = true;
        g_coop_unseen_out_of = theirs ? 1 : 0;
        if (getenv("ED_COOP_LOG") != NULL)
            fprintf(stderr, "[coop] player %d is hidden from their own view alone, the other stays (lr 0x%08X frame %llu)\n", theirs ? 2 : 1, lr, (unsigned long long)(cpu->timebase / 675000ull));
    }
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

// 0x80043C88 from anywhere but a look's opening and end (fn207, a script's own) stays the game's.
static void coop_show_main_asked(CPUState* cpu) {
    if (cpu->lr == COOP_LOOK_HIDES || cpu->lr == COOP_LOOK_SHOWS) {
        coop_show_asked(cpu);
        return;
    }
    g_hle_passthrough = true;
}

#define COOP_SCRIPT_SENDS 0x8016607Cu
#define COOP_THING_CALLS_BACK 0x801F7748u
static s32 g_coop_sent_script;  // the script of player 2's waiting on a thing

static void coop_script_sends(CPUState* cpu) {
    if (g_coop_p2 == 0u || !sda_ready(cpu))
        return;
    const s32 running = (s32)mem_read32(cpu, cpu->gpr[13] - RUNNING_SCRIPT);
    if (mem_read32(cpu, cpu->gpr[13] - 21496u) == mem_read32(cpu, g_coop_p2 + 16u))
        g_coop_sent_script = running;
    else if (running == g_coop_sent_script)
        g_coop_sent_script = 0;
}

static bool g_coop_p1_scan;  // their zones' look is running, with their prompts in (coop_scan_player1)
#define COOP_PROMPT_RUNS 0x8000704Cu  // a prompt's press running its script
#define COOP_ZONE_RUNS 0x80134B38u    // a zone's look running its script
static void coop_run_script(CPUState* cpu) {
    // What's being found, for either finder: the other player's word was copied as it was found, at the
    // find, and goes on when the find ends (coop_script_timers). Walking into its zone, or pressing on it,
    // in between took them to the find again, drawn over the finder's.
    if (g_coop_found_word.pending && (s32)cpu->gpr[3] == g_coop_found_word.script &&
        (cpu->lr == COOP_ZONE_RUNS || (g_coop_use.active ? g_coop_use.p2 != g_coop_found_word.p2 : cpu->lr == COOP_PROMPT_RUNS))) {
        if (getenv("ED_COOP_LOG") != NULL)
            fprintf(stderr, "[coop] script %d is being found by player %d: left to them (lr 0x%08X)\n", (s32)cpu->gpr[3], g_coop_found_word.p2 ? 2 : 1, cpu->lr);
        cpu->gpr[3] = 0u;
        g_hle_passthrough = false;
        g_hle_tail_call = false;
        return;
    }
    if (g_coop_p2 != 0u && sda_ready(cpu) && !(g_coop_use.active && g_coop_use.p2) &&
        mem_read32(cpu, cpu->gpr[13] - 21496u) != mem_read32(cpu, g_coop_p2 + 16u)) {
        g_coop_p1_ran_script = (s32)cpu->gpr[3];
        if (cpu->lr == COOP_PROMPT_RUNS)
            g_coop_p1_used_script = (s32)cpu->gpr[3];
    }
    // Player 1 walking into the zone of what player 2 is at, in their close-up: its word, copied from
    // theirs as they found the thing, took them to the find again, drawn over player 2's.
    // Only with such a word: at their own word's start it's only their EXAMINE, and left out, they walked
    // back to the bookshelf during player 2's scene and had no icon till they'd left its zone again.
    bool p1_word_going = false;
    for (u32 i = 0u; g_coop_p1_scan && i < 10u; i++)
        p1_word_going = p1_word_going || ((s16)mem_read16(cpu, SCRIPT_STATES + i * 8u) == (s16)cpu->gpr[3] && mem_read16(cpu, SCRIPT_STATES + i * 8u + 2u) != 0u);
    if (g_coop_p1_scan && g_coop_screen && (s32)cpu->gpr[3] == g_coop_use.script && p1_word_going) {
        if (getenv("ED_COOP_LOG") != NULL)
            fprintf(stderr, "[coop] player 1's zone leaves script %d to player 2's close-up\n", (s32)cpu->gpr[3]);
        cpu->gpr[3] = 0u;
        g_hle_passthrough = false;
        g_hle_tail_call = false;
        return;
    }
    if (g_tome_copy_turn && cpu->lr >= TOME_ITEM_CODE && cpu->lr < TOME_ITEM_CODE_END) {
        g_tome_copy_use_script = (s32)cpu->gpr[3];
        g_tome_copy_use_item = cpu->gpr[4];
    }
    if (cpu->lr != 0x800CA940u && cpu->lr != 0x80153B84u && cpu->lr != ARRIVAL_ZONE_RAN)
        g_both_script = (s32)cpu->gpr[3];  // not the zones', which run every frame
    if (coop_trace() && sda_ready(cpu) && cpu->lr != 0x800CA940u && cpu->lr != 0x80153B84u) {
        trace_look(cpu, "script run");
        g_script_watch = 40u;  // the game functions it calls
    }
    // An item player 2 used from their Tome. Its script asks if the player is in a zone
    // ("Paul cannot use that now.": they stood there, player 1 did not, and the words were
    // player 1's) and what it casts is cast by the controlled character. They are the
    // player for both, in the Tome's call and as it goes on when the Tome closes.
    const bool tome_item = (cpu->lr == TOME_SCRIPT_RAN || (cpu->lr >= TOME_ITEM_CODE && cpu->lr < TOME_ITEM_CODE_END)) && (g_coop_tome_owner == 1 || g_tome_copy_turn);
    // The first zone's script in a room player 2 asked for (the first time in a Tome's room,
    // a scene of whoever arrives): the zone fires for player 1, standing where the game put
    // them, and the scene showed them. It goes on from the timers and its end (0x801EC1C8).
    if (g_coop_entered_p2 && cpu->timebase / 675000ull > g_trace_ticket_frame + COOP_ARRIVAL_FRAMES)
        g_coop_entered_p2 = false;
    if (g_coop_entered_p2 && g_coop_arrival_scene == 0 && cpu->lr == ARRIVAL_ZONE_RAN && !g_coop_prompt_look && !g_coop_turn && (s32)cpu->gpr[3] > 0)
        g_coop_arrival_scene = (s32)cpu->gpr[3];
    const bool arrival_scene = g_coop_entered_p2 && (s32)cpu->gpr[3] == g_coop_arrival_scene &&
                               (cpu->lr == ARRIVAL_ZONE_RAN || cpu->lr == ARRIVAL_TIMER_RAN || cpu->lr == ARRIVAL_END_RAN);
    if ((tome_item || arrival_scene) && g_coop_p2 != 0u && !g_coop_use.active && sda_ready(cpu) &&
        coop_listed(cpu, g_coop_p2)) {
        const u32 args[3] = {cpu->gpr[3], cpu->gpr[4], cpu->gpr[5]};
        const u32 ran = cpu->lr;
        u32 was[5], ignore = 0u;
        const u32 object = mem_read32(cpu, g_coop_p2 + 52u);
        u32 info = 0u;
        const bool zones = hle_guest_call(cpu, COOP_CHARACTER_INFO, &object, 1, 0.0, &info) && guest_ram_range_valid(cpu, info, 184u);
        coop_be_player2(cpu, true, was);
        g_coop_turn_p1 = was[1];  // the Tome is player 1's (coop_has_tome)
        g_coop_tome_cast = true;
        if (zones)
            coop_zones_swap(cpu, info, true);  // "is the player in this zone" (0x8016A7DC) is theirs
        hle_guest_call(cpu, COOP_RUN_SCRIPT, args, 3, 0.0, &ignore);
        if (zones)
            coop_zones_swap(cpu, info, false);
        g_coop_tome_cast = false;
        coop_be_player2(cpu, false, was);
        cpu->lr = ran;
        g_hle_passthrough = false;
        g_hle_tail_call = false;
        return;
    }
    // A thing a script of player 2's sent on its way (coop_script_sends) calling that script back: the
    // bookshelf's lever, which ends the scene from there. Run as player 1's, from their word, it began
    // again for them, and player 2's word, the one to let go, never ran: both held for ever.
    if (cpu->lr == COOP_THING_CALLS_BACK && (s32)cpu->gpr[3] > 0 && (s32)cpu->gpr[3] == g_coop_sent_script && !g_coop_use.active && g_coop_p2 != 0u && sda_ready(cpu)) {
        const u32 ran = cpu->lr;
        g_coop_sent_script = 0;
        coop_use_start(cpu, (s32)cpu->gpr[3], true);
        if (g_coop_use.active) {
            g_coop_use.resume = ran;
            g_coop_use.at_press = true;
            return;
        }
    }
    g_hle_passthrough = true;
    // The game itself going on with a script of player 2's when what it waited for
    // is done (0x8016839C: the monk they rang for has come up the stairs): it goes
    // on from the game's table, their word still in theirs, and waited for ever.
    // The word is the game's now.
    if (!g_coop_use.active && !g_coop_turn && !g_coop_prompt_look && (s32)cpu->gpr[3] > 0 && sda_ready(cpu) &&
        cpu->lr == COOP_SCRIPT_WAIT_RAN) {
        if (getenv("ED_COOP_LOG") != NULL)
            for (u32 i = 0u; i < 10u; i++)
                if ((s16)(g_coop_states.p2[i * 8u] << 8 | g_coop_states.p2[i * 8u + 1u]) == (s32)cpu->gpr[3])
                    fprintf(stderr, "[coop] script %d goes on from the game (lr 0x%08X): its word is the game's%c",
                            (s32)cpu->gpr[3], cpu->lr, 10);
        coop_state_to_game(cpu, (s32)cpu->gpr[3]);
    }
    // A camera's arriving is one too (ARRIVAL_END_RAN): run as player 1's, from their word, the close-up's
    // find came again, for player 1, with the notice on player 2's half.
    if (!g_coop_screen || g_coop_use.active || (cpu->lr != PROMPT_USE_RAN && cpu->lr != WINDOW_CLOSE_RAN && cpu->lr != FOUND_CLOSE_RAN && cpu->lr != CLOSE_UP_END_RAN && cpu->lr != ARRIVAL_END_RAN) ||
        (s32)cpu->gpr[3] <= 0)
        return;
    const u32 ran = cpu->lr;
    coop_use_start(cpu, (s32)cpu->gpr[3], true);
    if (g_coop_use.active) {
        g_coop_use.resume = ran;
        g_coop_use.at_press = true;
    }
}

// A way to another room player 2 opened: it runs as player 1's, and the motion
// that goes with it (coop_door_motion) is theirs, for as long as its script may
// wait before it asks for the room.
#define COOP_DOOR_WAIT 300u
static unsigned long long g_coop_door_p2_until;

// A prompt with no script of its own (its script -2): a look its zone made ready
// (0x80307108), which the game's use of a prompt opens (0x80006EBC(its +4, 0): a
// press of the action button, the prompt the game's own, +28 * 6). Player 2's
// press has it opened with them the player and their prompt the game's: the
// press went unheard, a script being what it was looked for.
#define COOP_PROMPT_CLOSE_UP (-2)
#define COOP_PROMPT_USE 0x80006EBCu

static void coop_close_up_start(CPUState* cpu, s32 prompt) {
    coop_use_start(cpu, COOP_PROMPT_CLOSE_UP, true);
    if (!g_coop_use.active)
        return;
    g_coop_use.script = 0;
    const u32 own = PROMPT_SLOTS + 6u * 28u;
    for (u32 i = 0u; i < 6u; i++) {
        const u32 slot = PROMPT_SLOTS + i * 28u;
        if ((s32)mem_read32(cpu, slot + 8u) != prompt || (s32)mem_read32(cpu, slot + 12u) != COOP_PROMPT_CLOSE_UP)
            continue;
        for (u32 at = 0u; at < 28u; at += 4u)
            mem_write32(cpu, own + at, mem_read32(cpu, slot + at));
        break;
    }
    g_coop_use.found = true;  // what it opens is theirs, their prompts and cameras in
    cpu->gpr[3] = mem_read32(cpu, own + 4u);
    cpu->gpr[4] = 0u;
    cpu->pc = COOP_PROMPT_USE;
}

// No examine close-ups in co-op (ED_COOP_EXAMINE=1 keeps them). An examine
// (0x8001000C) makes its words and opens them in the message window
// (0x80024310(words, script, 0), from 0x80010134), which freezes the player
// until it is closed; closing runs the script it was given
// (0x80024220: 0x801642D4(script, 0, 0), r13 - 25940 the button: 0 A, 1 B), as
// often what the examine leads to (the Tome taken). In co-op the window is
// not opened: the words go and the script runs at the next controller, for
// the player whose it was (player 2 while their use runs). In a close-up it
// opens: the player is held, no controller runs (a thing to pick up, seen
// close, waited for ever), and the close-up's half and pad are its owner's.
#define MESSAGE_OPEN 0x80024310u
#define EXAMINE_OPENS 0x80010138u  // the return from the examine's opening
#define MESSAGE_CLOSED_WITH 25940u // r13 -
static struct {
    s32 script;
    bool p2;
} g_coop_close;

// The game's words name the chapter's character ("Paul cannot use that now."),
// whoever plays them: the player's own name goes in before they are laid out.
// Whatever makes words comes through the layout (0x801DB088(element): the words
// whole at its +122). A name followed by another ("Paul Luther") is the person's,
// and stays. Only the name: "he" and "his" stay as written.
#define WORDS_LAYOUT 0x801DB088u
#define WORDS_AT 122u
#define WORDS_MOST 1500u
static bool g_coop_tome_p2;

static const char* const kCharacterWords[12][2] = {
    {"Pius", "Pious"},  {"Ellia", NULL},  {"Anthony", NULL}, {"Karim", NULL},
    {"Max", "Maximillian"}, {"Lindsey", NULL}, {"Paul", NULL}, {"Roberto", NULL},
    {"Peter", NULL}, {"Edward", NULL}, {"Michael", NULL}, {"Alex", "Alexandra"}};

static bool words_letter(u8 c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

// Each whole word of from in the words becomes to, while there is room after them.
static void words_rename(u8* words, const char* from, const char* to) {
    const u32 from_n = (u32)strlen(from), to_n = (u32)strlen(to);
    u32 end = 0u;
    while (end < WORDS_MOST && words[end] != 0u)
        end++;
    if (end >= WORDS_MOST)
        return;
    for (u32 at = 0u; at + from_n <= end; at++) {
        if (memcmp(words + at, from, from_n) != 0 || (at > 0u && words_letter(words[at - 1u])) ||
            words_letter(words[at + from_n]) ||
            (words[at + from_n] == ' ' && words[at + from_n + 1u] >= 'A' && words[at + from_n + 1u] <= 'Z'))
            continue;
        if (to_n > from_n) {
            const u32 more = to_n - from_n;
            if (end + more >= WORDS_MOST)
                return;
            for (u32 i = 1u; i <= more; i++)
                if (words[end + i] != 0u)
                    return;
        }
        memmove(words + at + to_n, words + at + from_n, end + 1u - (at + from_n));
        if (to_n < from_n)
            memset(words + end + 1u - (from_n - to_n), 0, from_n - to_n);
        end = end + to_n - from_n;
        memcpy(words + at, to, to_n);
        at += to_n - 1u;
    }
}

// What one player finds, told in the other's half: "Pius has found a Magickal Rune". From the
// words the find shows (r3, its element), which name the chapter's character whoever found it.
static void coop_found_notice(CPUState* cpu, bool p2) {
    if (!guest_ram_range_valid(cpu, cpu->gpr[3] + WORDS_AT, 200u))
        return;
    char words[200];
    u32 n = 0u;
    for (u32 at = 0u; at < 199u && n + 1u < sizeof words; at++) {
        const u8 c = mem_read8(cpu, cpu->gpr[3] + WORDS_AT + at);
        if (c == 0u)
            break;
        if (c == '\\') {  // a colour or such: \a and its letter
            at++;
            if (mem_read8(cpu, cpu->gpr[3] + WORDS_AT + at) == 'a')
                at++;
            continue;
        }
        words[n++] = (char)(c >= 32u && c < 127u ? c : ' ');
    }
    words[n] = 0;
    // "has picked up a", "has found a", "has acquired a" as they come; anything else as it's worded.
    static const char* const kFound[3] = {" has picked up ", " has found ", " has acquired "};
    const char* item = NULL;
    for (int i = 0; i < 3 && item == NULL; i++) {
        item = strstr(words, kFound[i]);
        if (item != NULL)
            item += strlen(kFound[i]);
    }
    const char* const as_worded = item == NULL ? strstr(words, " has ") : NULL;
    int who = (p2 ? g_settings.mod_p2_character : g_settings.mod_p1_character) - 1;
    for (int i = 0; who < 0 && i < 12; i++)
        if (kPlayerModel[i] == g_chapter_model)
            who = i;
    if ((item == NULL && as_worded == NULL) || who < 0 || who >= 12) {
        fprintf(stderr, "[coop] no notice for the find's words: %s\n", words);
        return;
    }
    char notice[256];
    if (item != NULL)
        snprintf(notice, sizeof notice, "%s has found %s", kCharacterWords[who][0], item);
    else
        snprintf(notice, sizeof notice, "%s%s", kCharacterWords[who][0], as_worded);
    n = (u32)strlen(notice);
    while (n > 0u && (notice[n - 1u] == '!' || notice[n - 1u] == ' '))
        notice[--n] = 0;
    frontend_toast_player(p2 ? 0 : 1, notice);
    fprintf(stderr, "[coop] notice for player %d: %s\n", p2 ? 1 : 2, notice);
}

static void hle_words_layout(CPUState* cpu) {
    if (coop_trace() && guest_ram_range_valid(cpu, cpu->gpr[3] + WORDS_AT, 120u)) {
        static char seen[8][100];
        static int next;
        char now[100];
        u32 n = 0u;
        for (; n + 1u < sizeof now; n++) {
            const u8 c = mem_read8(cpu, cpu->gpr[3] + WORDS_AT + n);
            if (c == 0u)
                break;
            now[n] = (char)(c >= 32u && c < 127u ? c : 46);
        }
        now[n] = 0;
        bool known = false;
        for (int i = 0; i < 8; i++)
            known = known || strcmp(seen[i], now) == 0;
        if (!known) {
            snprintf(seen[next], sizeof seen[next], "%s", now);
            next = (next + 1) % 8;
            fprintf(stderr, "[trace] words: %s\n", now);
        }
    }
    g_hle_passthrough = true;
    // The labels by a page's L and R, in player 2's Tome, name the tab they go to: a label is
    // yellow (\ay), with nothing after its one word.
    if (g_tome_copy_turn && guest_ram_range_valid(cpu, cpu->gpr[3] + WORDS_AT, WORDS_MOST)) {
        u8* const label = cpu->ram + (cpu->gpr[3] + WORDS_AT - GC_RAM_BASE);
        const u8* const label_end = (const u8*)memchr(label, 0, 64u);
        if (label_end != NULL && strstr((const char*)label, "\\ay") != NULL) {
            const u32 n = (u32)(label_end - label);
            if (n > 7u && memcmp(label + n - 7u, "Options", 7u) == 0 && !words_letter(label[n - 8u]))
                words_rename(label, "Options", g_tome_copy.has_magick ? "Magick" : "Map");
            else if (n > 7u && memcmp(label + n - 7u, "Journal", 7u) == 0 && !words_letter(label[n - 8u]))
                words_rename(label, "Journal", "Inventory");
        }
    }
    if (g_chapter_model < 0 || !sda_ready(cpu) || !guest_ram_range_valid(cpu, cpu->gpr[3] + WORDS_AT, WORDS_MOST))
        return;
    int chapter = -1;
    for (int i = 0; i < 12; i++)
        if (kPlayerModel[i] == g_chapter_model)
            chapter = i;
    const bool p2 = g_settings.mod_coop && g_coop_p2 != 0u &&
                    (g_coop_tome_p2 || mem_read32(cpu, cpu->gpr[13] - 21492u) == g_coop_p2);
    const int choice = (p2 ? g_settings.mod_p2_character : g_settings.mod_p1_character) - 1;
    if (chapter < 0 || choice < 0 || choice >= 12 || choice == chapter)
        return;
    u8* const words = cpu->ram + (cpu->gpr[3] + WORDS_AT - GC_RAM_BASE);
    for (int k = 0; k < 2; k++)
        if (kCharacterWords[chapter][k] != NULL)
            words_rename(words, kCharacterWords[chapter][k], kCharacterWords[choice][0]);
}

// A script going on as a window closes (the clock's words, then what A took from it): the
// player is still held for it, so no controller runs to take up one passed over, and the
// game waited for ever. Its next words open as the game opens them.
static bool coop_from_window_close(CPUState* cpu) {
    // Player 2's goes on as a use of theirs, from the window's close (coop_run_script).
    if (g_coop_use.active && (g_coop_use.resume == WINDOW_CLOSE_RAN || g_coop_use.resume == FOUND_CLOSE_RAN))
        return true;
    u32 sp = cpu->gpr[1];
    for (int f = 0; f < 16 && guest_ram_range_valid(cpu, sp, 8u); f++) {
        const u32 caller = mem_read32(cpu, sp);
        if (caller <= sp || !guest_ram_range_valid(cpu, caller, 8u))
            break;
        const u32 ret = mem_read32(cpu, caller + 4u);
        if (ret == WINDOW_CLOSE_RAN || ret == FOUND_CLOSE_RAN)
            return true;
        sp = caller;
    }
    return false;
}

static void hle_message_open(CPUState* cpu) {
    if (coop_trace())
        trace_look(cpu, "window");
    static int keep = -1;
    if (keep < 0)
        keep = getenv("ED_COOP_EXAMINE") != NULL;
    // Player 1's words show (coop_words_hold keeps them from stopping player 2). Passed over too,
    // the podium's and the drawer's never showed in Paul's chapter.
    g_hle_passthrough = keep || !g_settings.mod_coop || g_coop_p2 == 0u || cpu->lr != EXAMINE_OPENS || coop_busy(cpu) ||
                        coop_from_window_close(cpu) || !(g_coop_use.active && g_coop_use.p2);
    if (g_hle_passthrough)
        return;
    const u32 words = cpu->gpr[3];
    const s32 script = (s32)cpu->gpr[4];
    u32 ignore = 0u;
    if (words != 0u)
        hle_guest_call(cpu, PROMPT_FREE, &words, 1, 0.0, &ignore);
    g_coop_close.script = script;
    g_coop_close.p2 = g_coop_use.active && g_coop_use.p2;
    if (getenv("ED_COOP_LOG") != NULL)
        fprintf(stderr, "[coop] an examine's close-up passed over for player %d (then script %d)\n",
                g_coop_close.p2 ? 2 : 1, script);
}

// A script's hold that asks for a prompt (0x8000A424: a thing looked at closely, B to go on) is a
// close-up of theirs, not a way out's transition: nobody's press reached it, and player 2 stood looking
// at the desk with the circle on it until player 1 pressed for them.
#define COOP_LOOK_PROMPT 0x8000A424u
static void coop_look_prompt(CPUState* cpu) {
    if (coop_trace())
        fprintf(stderr, "[trace] look prompt: controlled %u player 2's %u use %d of 2 %d turn %d assist %u lr 0x%08X\n", mem_read32(cpu, cpu->gpr[13] - 21496u),
                g_coop_p2 != 0u ? mem_read32(cpu, g_coop_p2 + 16u) : 0u, g_coop_use.active, g_coop_use.p2, g_coop_turn, g_coop_assist_at, cpu->lr);
    // With no use up too, as a page of their Tome ends (its RETURN): they're the player then. Not in their
    // turn: that's them walking up to a thing, its EXAMINE asked for. Taken for a close-up opening, player
    // 1's look in the 2 seconds after it went to player 2, both halves player 1's close-up.
    if ((g_coop_use.active && g_coop_use.p2) || (!g_coop_turn && g_coop_p2 != 0u && sda_ready(cpu) && mem_read32(cpu, cpu->gpr[13] - 21496u) == mem_read32(cpu, g_coop_p2 + 16u))) {
        g_coop_script_hold = false;
        g_coop_assist_at = 1u;  // the close-up it opens is theirs
    }
}

// Player 1's look under a hold of its own, with nothing but the hold up. Its channels stay on for the
// game's input, player 1's, and they stand. Everyone's update is told nobody's busy, as in a close-up of
// player 2's (coop_busy_ask): a character's step asks, outside their turn. Player 2's turn runs with the
// hold let go, as at player 1's words (coop_script_hold_go), and it's back after: its channels count
// down each frame, so each keeps its count (hle_coop_controller).
// The game runs one script's steps and one Tome: player 2 pressing on a thing of their own while player 1
// is in a look, a find, or their Tome is on its way, has it do nothing. They press again after.
static bool g_coop_find_free;
static bool coop_player1_mid_use(CPUState* cpu) {
    return g_coop_p1_look || (g_coop_found_word.pending && !g_coop_found_word.p2) || g_coop_find_free ||
           mem_read32(cpu, cpu->gpr[13] - 26188u) != mem_read32(cpu, cpu->gpr[13] - 26240u);
}
static bool g_coop_p1_look_open;  // player 2's turn, with the hold let go for it: the look isn't over
static bool coop_p1_close_up(CPUState* cpu) {
    if (!g_coop_p1_look)
        return false;
    if (g_coop_p1_look_open)
        return true;
    // Asked as the hold is still going on, before its bit and channels are: not over yet.
    static bool on;
    const u32 channel = sda_ready(cpu) ? mem_read32(cpu, cpu->gpr[13] - 23060u) : 0u;
    const bool held = (mem_read32(cpu, cpu->gpr[13] - 23344u) & ~32u) == 2u;  // 32: its scene's, as the lever's
    if (g_coop_screen || cutscene_playing(cpu) || (on && !held)) {
        if (coop_trace())
            fprintf(stderr, "[trace] player 1's look over: screen %d cutscene %d busy %u channel %08X mask %08X lr 0x%08X frame %llu\n", g_coop_screen, cutscene_playing(cpu),
                    mem_read32(cpu, cpu->gpr[13] - 23344u), channel, guest_ram_range_valid(cpu, channel, 20u) ? mem_read32(cpu, channel + 16u) : 0u, cpu->lr, (unsigned long long)(cpu->timebase / 675000ull));
        g_coop_p1_look = on = false;
        return false;
    }
    on = held;
    return held && !game_menu_open(cpu);
}

// The words' own hold (0x80042A8C: only the window's opening and closing call it): the player
// marked, busy bit 8, play held (bit 2) and the sound's mode. The game opens words without it over
// its Tome. Player 1's words go without it too, and player 2 plays on: the window's input lock
// keeps player 1 still, and player 2's turn opens that lock for them (coop_player_turn).
// Not a cutscene's words, and not player 2's own (their half and pad, coop_hold_player1).
// A script's own step may have held play just before (the urn, 0x8016820C). Let go as the script's
// end would: its input channels stay on with only the bit cleared, and player 2, standing, couldn't start.
static bool coop_script_hold_go(CPUState* cpu) {
    const u32 busy = mem_read32(cpu, cpu->gpr[13] - 23344u);
    if ((busy & ~2u) != 0u)
        return false;
    if (busy != 0u) {
        // What its own letting go does (0x80042B30(0), the step's 0x8016A6DC): 0x80199EA0 alone left
        // r13 - 25696 on, which the Tome's opening asks (0x80006D0C), and the page after a circle never
        // came. Not the call: its end (0x80134D48) takes "the player", player 2 here, and player 1's
        // camera stayed on them.
        const u32 lr = cpu->lr, r3 = cpu->gpr[3], go = 1u;
        u32 ignore = 0u;
        hle_guest_call(cpu, COOP_PLAY_HOLD, &go, 1, 0.0, &ignore);
        mem_write32(cpu, cpu->gpr[13] - 25696u, 0u);
        mem_write32(cpu, cpu->gpr[13] - 23040u, 1u);
        mem_write32(cpu, cpu->gpr[13] - 24048u, mem_read32(cpu, cpu->gpr[13] - 24048u) & ~4u);
        cpu->lr = lr;
        cpu->gpr[3] = r3;
    }
    return true;
}

// Player 2's words or find: the hold's bit stays for this host's asking (coop_busy: the close-up
// is theirs, with their pad), with no hold on play. The game is told nobody's busy (coop_busy_ask).
static bool coop_their_close_up(CPUState* cpu) {
    if (!g_settings.mod_coop || g_coop_p2 == 0u || !sda_ready(cpu) || cutscene_playing(cpu) ||
        !((g_coop_use.active && g_coop_use.p2) || mem_read32(cpu, cpu->gpr[13] - 21492u) == g_coop_p2))
        return false;
    return coop_script_hold_go(cpu);
}

static bool coop_no_pause(CPUState* cpu) {
    // Player 2's own too with no use of theirs up: their Tome's item (tome_copy_left), with them the player.
    if (!g_settings.mod_coop || g_coop_p2 == 0u || !sda_ready(cpu) || cutscene_playing(cpu) || (g_coop_use.active && g_coop_use.p2) ||
        mem_read32(cpu, cpu->gpr[13] - 21492u) == g_coop_p2)
        return false;
    return coop_script_hold_go(cpu);
}

#define WORDS_HOLD 0x80042A8Cu
#define WORDS_HOLD_OPENS 0x8002436Cu   // the return, in the window's opening
#define WORDS_HOLD_CLOSES 0x800242C4u  // and its closing
static bool g_coop_words_free;

static bool g_coop_words_theirs;

static void coop_words_hold(CPUState* cpu) {
    g_hle_passthrough = true;
    if (cpu->lr == WORDS_HOLD_OPENS && coop_their_close_up(cpu)) {
        g_coop_words_theirs = true;
        g_coop_script_hold = false;
        mem_write32(cpu, cpu->gpr[13] - 23344u, mem_read32(cpu, cpu->gpr[13] - 23344u) | 8u);
        g_hle_passthrough = false;
        g_hle_tail_call = false;
        return;
    }
    if (cpu->lr == WORDS_HOLD_CLOSES && g_coop_words_theirs) {
        g_coop_words_theirs = false;
        mem_write32(cpu, cpu->gpr[13] - 23344u, mem_read32(cpu, cpu->gpr[13] - 23344u) & ~8u);
        g_hle_passthrough = false;
        g_hle_tail_call = false;
        return;
    }
    if (cpu->lr == WORDS_HOLD_OPENS) {
        g_coop_words_free = coop_no_pause(cpu);
        if (!g_coop_words_free)
            return;
        if (getenv("ED_COOP_LOG") != NULL)
            fprintf(stderr, "[coop] player 1's words open with no hold\n");
    } else if (cpu->lr == WORDS_HOLD_CLOSES && g_coop_words_free) {
        g_coop_words_free = false;
    } else {
        return;
    }
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

// A find's own hold (0x80042BB8: only finds call it): play held, busy bit 16, and for the script
// timers the bit (r13 - 24048, bit 2) and the byte (r13 - 25932) that say a find is up. Those two
// stay: the rune inside the urn waits on them. Player 1 stays still by their controller not running.
#define FIND_HOLD 0x80042BB8u
static bool g_coop_find_free;

static void coop_find_hold(CPUState* cpu) {
    g_hle_passthrough = true;
    const bool on = cpu->gpr[3] != 0u;
    if (!on && g_coop_found_out_of >= 0)  // a few frames on: it is still there as its close-up ends
        g_coop_found_until = cpu->timebase / 675000ull + 4ull;
    static bool theirs;
    if ((on && coop_their_close_up(cpu)) || (!on && theirs)) {
        theirs = on;
        if (on)
            g_coop_script_hold = false;
        const u32 busy = mem_read32(cpu, cpu->gpr[13] - 23344u);
        mem_write32(cpu, cpu->gpr[13] - 23344u, on ? busy | 16u : busy & ~16u);
        const u32 kept = mem_read32(cpu, cpu->gpr[13] - 24048u);
        mem_write32(cpu, cpu->gpr[13] - 24048u, on ? kept | 2u : kept & ~2u);
        mem_write8(cpu, cpu->gpr[13] - 25932u, on ? 1u : 0u);
        g_hle_passthrough = false;
        g_hle_tail_call = false;
        return;
    }
    if (on) {
        g_coop_find_free = coop_no_pause(cpu);
        if (!g_coop_find_free)
            return;
        if (getenv("ED_COOP_LOG") != NULL)
            fprintf(stderr, "[coop] player 1's find opens with no hold\n");
    } else if (g_coop_find_free) {
        g_coop_find_free = false;
    } else {
        return;
    }
    const u32 flags = mem_read32(cpu, cpu->gpr[13] - 24048u);
    mem_write32(cpu, cpu->gpr[13] - 24048u, on ? flags | 2u : flags & ~2u);
    mem_write8(cpu, cpu->gpr[13] - 25932u, on ? 1u : 0u);
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

// A close-up of player 2's is theirs, with the game's pad: player 1 plays on. The game is told
// nobody's busy (everyone's update asks, 0x80155738), and at their controller player 1 has a turn as
// player 2 has theirs: input of their own, with their pad, past the window's lock.
#define COOP_BUSY_ASK 0x80155738u
static void coop_busy_ask(CPUState* cpu) {
    g_hle_passthrough = true;
    if ((g_coop_screen && (g_coop_words_theirs || coop_busy(cpu)) && !cutscene_playing(cpu) && !game_menu_open(cpu)) || coop_p1_close_up(cpu)) {
        hle_set_u32(cpu, 0u);
        g_hle_passthrough = false;
    }
}

// The hold let go for the other player's turn, and back after it: its channels count down each frame,
// so each keeps its count. The one holding stands, the other walks. For player 2 in a look of player 1's
// (hle_coop_controller), and player 1 in a scene of player 2's (coop_player1_turn).
typedef struct {
    u32 counts[3], words[4];
} CoopHoldKept;
static const u16 kCoopHoldChannels[3] = {23060u, 23052u, 23056u};
static const u16 kCoopHoldWords[4] = {23344u, 25696u, 23040u, 24048u};
static void coop_hold_open(CPUState* cpu, CoopHoldKept* kept) {
    for (int i = 0; i < 3; i++) {
        const u32 channel = mem_read32(cpu, cpu->gpr[13] - kCoopHoldChannels[i]);
        kept->counts[i] = 0u;
        if (guest_ram_range_valid(cpu, channel, 20u)) {
            kept->counts[i] = mem_read32(cpu, channel + 16u);
            mem_write32(cpu, channel + 16u, 0u);
        }
    }
    for (int i = 0; i < 4; i++)
        kept->words[i] = mem_read32(cpu, cpu->gpr[13] - kCoopHoldWords[i]);
    mem_write32(cpu, cpu->gpr[13] - 23344u, 0u);
    mem_write32(cpu, cpu->gpr[13] - 25696u, 0u);
    mem_write32(cpu, cpu->gpr[13] - 23040u, 1u);
    mem_write32(cpu, cpu->gpr[13] - 24048u, kept->words[3] & ~4u);
}
static void coop_hold_shut(CPUState* cpu, const CoopHoldKept* kept) {
    for (int i = 0; i < 3; i++) {
        const u32 channel = mem_read32(cpu, cpu->gpr[13] - kCoopHoldChannels[i]);
        if (guest_ram_range_valid(cpu, channel, 20u))
            mem_write32(cpu, channel + 16u, kept->counts[i]);
    }
    for (int i = 0; i < 4; i++)
        mem_write32(cpu, cpu->gpr[13] - kCoopHoldWords[i], kept->words[i]);
}

static bool g_coop_p1_turn;
static void coop_player1_turn(CPUState* cpu) {
    static CoopPlayerState game, own;
    static bool own_set;
    // A script's hold under a look of player 2's, asked for again as it goes on: let go as at their
    // words, its channel on (r13 - 23060, +16) kept player 1 from moving. Its bit stays for this host's
    // asking (coop_busy): the close-up is theirs.
    const u32 hold_channel = mem_read32(cpu, cpu->gpr[13] - 23060u), busy_was = mem_read32(cpu, cpu->gpr[13] - 23344u);
    // Their scene (bit 32, the lever's): all of it stays theirs, and is back after this turn.
    CoopHoldKept scene_hold;
    const bool scene = (busy_was & 32u) != 0u;
    if (scene) {
        coop_hold_open(cpu, &scene_hold);
        // Its bit stays for this host's asking, as below: the pad's read in this turn showed player 2's icon
        // over their own scene and took it away again, each frame. The game's still told nobody's busy.
        mem_write32(cpu, cpu->gpr[13] - 23344u, busy_was);
    } else if (busy_was == 2u && guest_ram_range_valid(cpu, hold_channel, 20u) && mem_read32(cpu, hold_channel + 16u) != 0u) {
        coop_script_hold_go(cpu);
        mem_write32(cpu, cpu->gpr[13] - 23344u, busy_was);
    }
    const u32 node = cpu->gpr[3];
    // Their words leave them the player, and the controller moves the controlled character.
    const u32 who[3] = {cpu->gpr[13] - 26524u, cpu->gpr[13] - 21492u, cpu->gpr[13] - 21496u};
    const u32 now[3] = {mem_read32(cpu, node + 52u), node, mem_read32(cpu, node + 16u)};
    const u32 who4[2] = {cpu->gpr[13] - 21488u, cpu->gpr[13] - 21484u};
    u32 was[3], was4[2];
    for (int i = 0; i < 3; i++) {
        was[i] = mem_read32(cpu, who[i]);
        mem_write32(cpu, who[i], now[i]);
    }
    for (int i = 0; i < 2; i++)
        was4[i] = mem_read32(cpu, who4[i]);
    mem_write32(cpu, who4[0], now[2]);
    mem_write32(cpu, who4[1], now[1]);
    coop_state(cpu, &game, true);
    if (own_set)
        coop_state(cpu, &own, false);
    // Their input has the focus, as player 2's own copy has in their turn: the window's took it,
    // and the controller's channel (r13 - 25648) was passed over: "standing" whatever the stick.
    // Back with the game's (coop_state).
    mem_write32(cpu, cpu->gpr[13] - 23552u, 0u);
    mem_write32(cpu, cpu->gpr[13] - 23556u, 0u);
    for (u32 at = COOP_INPUT_MASKS; at + 4u <= COOP_INPUT_SIZE; at += 32u)
        if (mem_read32(cpu, COOP_INPUT + at) == 0x0FFFFFFEu)
            mem_write32(cpu, COOP_INPUT + at, 0x0FFFFFFFu);
    u32 ignore = 0u;
    g_coop_pad_p1 = true;
    hle_guest_call(cpu, COOP_PAD_UPDATE, NULL, 0, 0.0, &ignore);
    g_coop_pad_p1 = false;
    // Their stick goes by the camera (COOP_CAMERA: eye, target): theirs, as in player 2's turn. By the
    // game's, player 2's close-up, up on the stick was anywhere.
    u8 game_camera[24];
    coop_bytes(cpu, COOP_CAMERA, game_camera, sizeof game_camera, true);
    for (u32 i = 0u; g_coop_p1_view_frame != 0ull && i < 6u; i++)
        guest_write_f32(cpu, COOP_CAMERA + i * 4u, g_coop_p1_view[i]);
    g_coop_p1_turn = true;
    hle_guest_call(cpu, COOP_CONTROLLER, &node, 1, 0.0, &ignore);
    g_coop_p1_turn = false;
    coop_bytes(cpu, COOP_CAMERA, game_camera, sizeof game_camera, false);
    coop_player1_camera(cpu);
    if (coop_trace() && cpu->timebase / 675000ull % 30ull == 0ull) {
        const u32 o1 = mem_read32(cpu, node + 52u);
        fprintf(stderr, "[trace] player 1's turn: at %.0f %.0f pad %d %d busy %u prompts off %X words %u frame %llu\n", guest_read_f32(cpu, o1), guest_read_f32(cpu, o1 + 4u),
                (s8)mem_read8(cpu, COOP_INPUT + 2u), (s8)mem_read8(cpu, COOP_INPUT + 3u), mem_read32(cpu, cpu->gpr[13] - 23344u), mem_read32(cpu, cpu->gpr[13] - 24048u), mem_read32(cpu, cpu->gpr[13] - 25960u), (unsigned long long)(cpu->timebase / 675000ull));
    }
    coop_state(cpu, &own, true);
    own_set = true;
    coop_state(cpu, &game, false);
    for (int i = 0; i < 3; i++)
        mem_write32(cpu, who[i], was[i]);
    for (int i = 0; i < 2; i++)
        mem_write32(cpu, who4[i], was4[i]);
    if (scene)
        coop_hold_shut(cpu, &scene_hold);
}

// Player 1's controller: player 2's turn first, then their use if they
// pressed for one.
static void coop_player_turn(CPUState* cpu);
// In a close-up of player 2's the game's prompts are theirs (coop_cameras_lend). Player 1 walking into
// a zone, or out, ran its script as theirs on those: player 2's Details gave way to player 1's Examine,
// and their B ran nothing. Player 1's own go where theirs are kept, and come back with them.
// The game's choosing of a prompt (0x80117628) first asks whether the player may have one (r13 - 24040,
// 0x800460EC). With play held, as it is in a close-up, that leaves out the player's own moves and asks
// only what a scene needs. Player 1's turn lets the hold go: then them running took player 2's Details
// away. Left out altogether, the pick up showed before the circle's scene and B skipped it. So: as
// the game has it in a close-up, with the hold's channel on and player 2 the player.
#define COOP_PROMPT_CHOOSE 0x80117628u
static void coop_prompt_choose(CPUState* cpu) {
    static bool running;
    g_hle_passthrough = true;
    // Player 1's look: told nobody's busy, the pick up showed before its scene ended, and B skipped it.
    // As in player 2's close-up below, with them the player already. Not in player 2's turn.
    if (!running && g_coop_p1_look && !g_coop_p1_look_open && !g_coop_turn && !g_coop_use.active && sda_ready(cpu)) {
        const u32 look_channel = mem_read32(cpu, cpu->gpr[13] - 23060u);
        if (guest_ram_range_valid(cpu, look_channel, 20u)) {
            const u32 mask_was = mem_read32(cpu, look_channel + 16u), busy_was = mem_read32(cpu, cpu->gpr[13] - 23344u), lr = cpu->lr;
            u32 ignore = 0u;
            mem_write32(cpu, look_channel + 16u, 0x0FFFFFFFu);
            running = true;
            hle_guest_call(cpu, COOP_PROMPT_CHOOSE, NULL, 0, 0.0, &ignore);
            running = false;
            mem_write32(cpu, look_channel + 16u, mask_was);
            mem_write32(cpu, cpu->gpr[13] - 23344u, busy_was);
            cpu->lr = lr;
            g_hle_passthrough = false;
            g_hle_tail_call = false;
            return;
        }
    }
    if (running || !g_coop_screen || g_coop_use.active || !sda_ready(cpu))
        return;
    const u32 channel = mem_read32(cpu, cpu->gpr[13] - 23060u);
    if (!guest_ram_range_valid(cpu, channel, 20u))
        return;
    const u32 mask_was = mem_read32(cpu, channel + 16u), lr = cpu->lr;
    u32 was[5], ignore = 0u;
    mem_write32(cpu, channel + 16u, 0x0FFFFFFFu);
    coop_be_player2(cpu, true, was);
    running = true;
    hle_guest_call(cpu, COOP_PROMPT_CHOOSE, NULL, 0, 0.0, &ignore);
    running = false;
    coop_be_player2(cpu, false, was);
    mem_write32(cpu, channel + 16u, mask_was);
    cpu->lr = lr;
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

#define COOP_PROMPT_SET 0x80117404u
static bool g_coop_p1_scan;  // their zones' look is running, with their prompts in (coop_scan_player1)
static bool coop_prompt_player1s(CPUState* cpu, u32 fn, int count) {
    static bool running;
    if (running || g_coop_p1_scan || !g_coop_screen || !g_coop_cameras.lent || g_coop_use.active || !sda_ready(cpu) ||
        mem_read32(cpu, cpu->gpr[13] - 21492u) == g_coop_p2)
        return false;
    u32 args[7], answer = 0u;
    for (int i = 0; i < count; i++)
        args[i] = cpu->gpr[3 + i];
    u8 theirs[PROMPT_SLOTS_SIZE];
    coop_bytes(cpu, PROMPT_SLOTS, theirs, PROMPT_SLOTS_SIZE, true);
    coop_bytes(cpu, PROMPT_SLOTS, g_coop_cameras.p1_prompts, PROMPT_SLOTS_SIZE, false);
    running = true;
    hle_guest_call(cpu, fn, args, count, 0.0, &answer);
    running = false;
    coop_bytes(cpu, PROMPT_SLOTS, g_coop_cameras.p1_prompts, PROMPT_SLOTS_SIZE, true);
    coop_bytes(cpu, PROMPT_SLOTS, theirs, PROMPT_SLOTS_SIZE, false);
    cpu->gpr[3] = answer;
    return true;
}

static void coop_prompt_set(CPUState* cpu) {
    g_hle_passthrough = !coop_prompt_player1s(cpu, COOP_PROMPT_SET, 7);
    g_hle_tail_call = false;
}

// Their look at the zones round them, whole, in a close-up of player 2's: walking out of one clears
// what its script asked for (0x80134758, 0x801170D4), and that was player 2's Details, with their B
// left running nothing.
// A scene of player 2's turns the zones off (0x80042974 -> 0x8019C088(0)), and a look with them off
// clears the looker's inside words (0x80134BE8). Alone the player's controller doesn't run in a scene;
// player 1's does, in player 2's close-up. Every zone then fired for them as the scene ended: a look
// at the bookshelf with no press, and held.
#define COOP_ZONES_ON 23040u
static void coop_scan_player1(CPUState* cpu) {
    g_hle_passthrough = true;
    if (g_coop_p1_scan || g_coop_prompt_look || !g_coop_screen || !g_coop_cameras.lent || g_coop_use.active || !sda_ready(cpu) ||
        cpu->gpr[3] == mem_read32(cpu, g_coop_p2 + 52u))
        return;
    u32 args[4], ignore = 0u;
    for (int i = 0; i < 4; i++)
        args[i] = cpu->gpr[3 + i];
    // Their cameras too: a zone's script sets them, and player 2's close-up went to the room's view.
    // Their zones are on, whatever player 2's scene has them as: they walk through it.
    const u32 zones_were = mem_read32(cpu, cpu->gpr[13] - COOP_ZONES_ON);
    mem_write32(cpu, cpu->gpr[13] - COOP_ZONES_ON, 1u);
    coop_cameras_lend(cpu, false);
    g_coop_p1_scan = true;
    hle_guest_call(cpu, COOP_SCAN, args, 4, 0.0, &ignore);
    g_coop_p1_scan = false;
    coop_cameras_lend(cpu, true);
    mem_write32(cpu, cpu->gpr[13] - COOP_ZONES_ON, zones_were);
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

static void coop_prompt_clear(CPUState* cpu) {
    // Only in their own turn. The game's use of a press clears what's on show (0x80006FD8), and in player
    // 2's close-up that press is player 2's, with player 1 still the player: it took player 1's own prompt,
    // the EXAMINE they stood by, and it never came back (zones ask once, on the way in).
    g_hle_passthrough = !g_coop_p1_turn || !coop_prompt_player1s(cpu, PROMPT_CLEAR, 1);
    g_hle_tail_call = false;
}

static void hle_coop_controller(CPUState* cpu) {
    g_hle_passthrough = true;
    if (g_coop_p2 != 0u && cpu->gpr[3] == g_coop_p2)
        return;  // player 2's own, in their turn
    if (g_coop_p1_turn)
        return;
    // And words over a Tome the game opened for player 2 (the page after a rune): the world goes on
    // behind it, and the words have the focus.
    if (!g_coop_use.active && !cutscene_playing(cpu) &&
        ((g_coop_screen && !game_menu_open(cpu)) ||
         (g_coop_tome_owner == 1 && hle_coop_tome_shares() && sda_ready(cpu) && mem_read32(cpu, cpu->gpr[13] - 25960u) != 0u))) {
        coop_player1_turn(cpu);
        g_hle_passthrough = false;
        g_hle_tail_call = false;
        return;
    }
    if (g_coop_use.resumed) {  // back from a run: the controller goes on
        g_coop_use.resumed = false;
        return;
    }
    const bool p1_look = coop_p1_close_up(cpu);
    CoopHoldKept look_hold;
    if (p1_look) {
        coop_hold_open(cpu, &look_hold);
        g_coop_p1_look_open = true;
    }
    coop_player_turn(cpu);
    if (p1_look) {
        g_coop_p1_look_open = false;
        coop_hold_shut(cpu, &look_hold);
    }
    // Player 2's turn calls into the game, whose hooks leave these as they
    // wanted: player 1's controller runs after it.
    g_hle_passthrough = true;
    g_hle_tail_call = false;
    if ((g_coop_find_free || g_coop_words_free) && coop_trace() && g_coop_p2 != 0u && cpu->timebase / 675000ull % 30ull == 0ull) {
        const u32 o2 = mem_read32(cpu, g_coop_p2 + 52u);
        if (guest_ram_range_valid(cpu, o2, 12u))
            fprintf(stderr, "[trace] no pause: player 2 at %.0f %.0f busy %u frame %llu\n", guest_read_f32(cpu, o2), guest_read_f32(cpu, o2 + 4u),
                    mem_read32(cpu, cpu->gpr[13] - 23344u), (unsigned long long)(cpu->timebase / 675000ull));
    }
    if (g_coop_find_free) {  // player 1 is looking at what they found
        g_hle_passthrough = false;
        return;
    }
    if (g_coop_use.active || g_coop_p2 == 0u || !sda_ready(cpu))
        return;
    if (g_coop_close.script != 0) {  // what a passed-over examine leaves
        const s32 script = g_coop_close.script;
        g_coop_close.script = 0;
        mem_write32(cpu, cpu->gpr[13] - MESSAGE_CLOSED_WITH, 0u);  // closed with A (B, 1, goes back)
        if (script > 0)
            coop_use_start(cpu, script, g_coop_close.p2);
        return;
    }
    // Pressed just before player 1's look or find, it would start with player 1's next step: their Tome's
    // page went to player 2's half, and player 2's close-up to player 1's (run-20261103-181505-both, 10703).
    if (g_coop_use_script != 0 && coop_player1_mid_use(cpu)) {
        if (getenv("ED_COOP_LOG") != NULL)
            fprintf(stderr, "[coop] player 2's press for script %d let go: player 1's look or find is up\n", g_coop_use_script);
        g_coop_use_script = 0;
    }
    // The game has one window: what player 2 pressed for waits for player 1's words to close.
    if (g_coop_use_script != 0 && !g_coop_words_free) {
        const s32 script = g_coop_use_script;
        g_coop_use_script = 0;
        if (script == COOP_PROMPT_CLOSE_UP) {
            coop_close_up_start(cpu, g_coop_use_prompt);
        } else if (coop_prompt_is_exit(g_coop_use_prompt)) {
            // A way to another room: the game loads rooms round player 1,
            // so it is taken as player 1 takes it, from what player 2's
            // prompt left (their script's word moved to the game's table):
            // its later steps run as the game's, and both players arrive
            // in the room (player 2 brought along, coop_follow).
            coop_state_to_game(cpu, script);
            coop_use_start(cpu, script, false);
            g_coop_door_p2_until = cpu->timebase / 675000ull + COOP_DOOR_WAIT;
        } else {
            coop_use_start(cpu, script, true);
        }
        return;
    }
}

// Player 2's script timers, as 0x80163E00 counts player 1's (+4, a halfword), at
// the same time in the same place: one run out runs its script for them. At
// player 1's controller, they stopped while it did not run (held in a scene of
// player 2's: the bell rung for the monk waited for ever at its timer).
#define COOP_SCRIPT_TIMERS 0x80163E00u

static void coop_script_timers(CPUState* cpu) {
    g_hle_passthrough = true;
    g_hle_tail_call = false;
    if (coop_trace() && g_coop_zones.list != 0u) {  // the game's zone words, as they change: who stands in what
        static u32 inside_was;
        u32 inside = 0u;
        for (u32 i = 0u; i < 32u && guest_ram_range_valid(cpu, g_coop_zones.list + i * COOP_ZONE_SIZE, COOP_ZONE_SIZE); i++)
            inside |= (mem_read32(cpu, g_coop_zones.list + i * COOP_ZONE_SIZE + COOP_ZONE_INSIDE) & 1u) << i;
        if (inside != inside_was)
            fprintf(stderr, "[trace] player 1's zones 0x%08X -> 0x%08X frame %llu\n", inside_was, inside, (unsigned long long)(cpu->timebase / 675000ull));
        inside_was = inside;
    }
    if (g_coop_use.resumed) {  // back from a run: the game's timers go on
        g_coop_use.resumed = false;
        return;
    }
    if (g_coop_use.active || g_coop_p2 == 0u || !g_settings.mod_coop || !sda_ready(cpu))
        return;
    if (g_coop_found_word.pending) {
        g_coop_found_word.pending = false;
        u8 game[SCRIPT_STATES_SIZE];
        coop_bytes(cpu, SCRIPT_STATES, game, SCRIPT_STATES_SIZE, true);
        const u8* from = g_coop_found_word.p2 ? g_coop_states.p2 : game;
        u8* to = g_coop_found_word.p2 ? game : g_coop_states.p2;
        const s32 script = g_coop_found_word.script;
        const u8* word = NULL;
        u8* at = NULL;
        for (u32 i = 0u; i < 10u; i++) {
            if ((s16)(from[i * 8u] << 8 | from[i * 8u + 1u]) == script)
                word = from + i * 8u;
            if ((s16)(to[i * 8u] << 8 | to[i * 8u + 1u]) == script)
                at = to + i * 8u;
        }
        if (at != NULL) {
            if (word != NULL) {
                memcpy(at, word, 8u);
                // Not its timer: a script going on by one (the urn broken, then the rune inside)
                // ran for the other player too, and what came next was theirs.
                at[4] = at[5] = 0u;
            } else {
                memset(at, 0, 8u);
            }
            coop_bytes(cpu, SCRIPT_STATES, game, SCRIPT_STATES_SIZE, false);
        }
    }
    for (u32 i = 0; i < 10u; i++) {
        u8* entry = g_coop_states.p2 + i * 8u;
        const s32 script = (s16)(entry[0] << 8 | entry[1]);
        const u32 timer = (u32)entry[4] << 8 | entry[5];
        if (script == 0 || timer == 0u)
            continue;
        entry[4] = (u8)((timer - 1u) >> 8);
        entry[5] = (u8)(timer - 1u);
        if (timer == 1u) {
            coop_use_start(cpu, script, true);
            return;
        }
    }
}

static void tw_state_loaded(void);
// Quick state: which element is player 2's (a state loaded without it
// leaves the one it had drawn, unknown here).
typedef struct {
    u32 element, screen, p2;
    s32 id;
} HleCoopPrompt;

size_t hle_coop_prompt_size(void) {
    return sizeof(HleCoopPrompt);
}

void hle_coop_prompt_save(void* out) {
    const HleCoopPrompt s = {g_coop_prompt_shown.element, g_coop_prompt_shown.screen, g_coop_prompt_shown.p2,
                             g_coop_prompt_shown.id};
    memcpy(out, &s, sizeof s);
}

void hle_coop_prompt_load(const void* in) {
    HleCoopPrompt s = {0u, 0u, 0u, -1};
    if (in != NULL)
        memcpy(&s, in, sizeof s);
    g_coop_prompt_shown.element = s.element;
    g_coop_prompt_shown.screen = s.screen;
    g_coop_prompt_shown.p2 = s.p2;
    g_coop_prompt_shown.id = s.id;
    g_coop_prompt = -1;
    tw_state_loaded();
}

// Player 2's element: made when their prompt changes, let go of when it goes
// (one on a screen since gone, with the area, is left alone).
static void coop_prompt_show(CPUState* cpu, bool shown) {
    const u32 screen = mem_read32(cpu, cpu->gpr[13] - PROMPT_SCREEN);
    const s32 want = shown && screen != 0u ? g_coop_prompt : -1;
    if (g_coop_prompt_shown.element != 0u &&
        (g_coop_prompt_shown.screen != screen || g_coop_prompt_shown.p2 != g_coop_p2))
        g_coop_prompt_shown.element = 0u, g_coop_prompt_shown.id = -1;
    if (want == g_coop_prompt_shown.id)
        return;
    u32 ignore = 0u;
    if (g_coop_prompt_shown.element != 0u)
        hle_guest_call(cpu, PROMPT_FREE, &g_coop_prompt_shown.element, 1, 0.0, &ignore);
    g_coop_prompt_shown.element = 0u;
    g_coop_prompt_shown.id = -1;
    if (want < 0)
        return;
    // Made as the game makes its own (0x8011708C), its word and picture
    // with it.
    u32 element = 0u;
    const u32 make[6] = {screen, 0u, (u32)want, 0u, 1u, PROMPT_STYLE};
    if (!hle_guest_call(cpu, PROMPT_MAKE, make, 6, 0.0, &element) || !guest_ram_range_valid(cpu, element, 0x100u))
        return;
    mem_write16(cpu, element + 8u, (u16)(s16)COOP_PROMPT_AT);
    g_coop_prompt_shown.element = element;
    g_coop_prompt_shown.screen = screen;
    g_coop_prompt_shown.p2 = g_coop_p2;
    g_coop_prompt_shown.id = want;
    if (getenv("ED_COOP_LOG") != NULL)
        fprintf(stderr, "[coop] player 2's prompt %d (element 0x%08X)\n", want, element);
}

// Player 1's own icon in a close-up or scene of player 2's. The game's prompt is player 2's then, and with
// the split's owner set only their view draws the game's HUD. Theirs is made as player 2's is, from the
// prompts kept for them (coop_prompt_player1s), far past player 2's place: only player 1's view brings it
// back, whatever it hides (build_view in gfx/interp.cpp).
#define COOP_PROMPT_P1_AWAY 8192
static struct {
    u32 element, screen;
    s32 id;
} g_coop_p1_icon = {0u, 0u, -1};

static void coop_p1_icon_show(CPUState* cpu) {
    const u32 screen = mem_read32(cpu, cpu->gpr[13] - PROMPT_SCREEN);
    s32 want = -1;
    if (g_coop_screen && g_coop_cameras.lent && screen != 0u && !game_menu_open(cpu) && !cutscene_playing(cpu) && !(g_tome_copy_open && g_tome_copy_p1)) {
        const u8* p = g_coop_cameras.p1_prompts;
        for (u32 i = 0u; i < 6u && want < 0; i++, p += 28u) {
            const s32 id = (s32)((u32)p[8] << 24 | (u32)p[9] << 16 | (u32)p[10] << 8 | p[11]);
            if (id >= 0)
                want = id;
        }
    }
    if (g_coop_p1_icon.element != 0u && g_coop_p1_icon.screen != screen)
        g_coop_p1_icon.element = 0u, g_coop_p1_icon.id = -1;
    if (want == g_coop_p1_icon.id)
        return;
    u32 ignore = 0u;
    if (g_coop_p1_icon.element != 0u)
        hle_guest_call(cpu, PROMPT_FREE, &g_coop_p1_icon.element, 1, 0.0, &ignore);
    g_coop_p1_icon.element = 0u;
    g_coop_p1_icon.id = -1;
    if (want < 0)
        return;
    u32 element = 0u;
    const u32 make[6] = {screen, 0u, (u32)want, 0u, 1u, PROMPT_STYLE};
    if (!hle_guest_call(cpu, PROMPT_MAKE, make, 6, 0.0, &element) || !guest_ram_range_valid(cpu, element, 0x100u))
        return;
    mem_write16(cpu, element + 8u, (u16)(mem_read16(cpu, element + 8u) + COOP_PROMPT_P1_AWAY));
    g_coop_p1_icon.element = element;
    g_coop_p1_icon.screen = screen;
    g_coop_p1_icon.id = want;
    if (getenv("ED_COOP_LOG") != NULL)
        fprintf(stderr, "[coop] player 1's prompt %d in player 2's close-up (element 0x%08X)\n", want, element);
}

static void coop_player_turn(CPUState* cpu) {
    cheat_apply(cpu);  // Mods' Cheats, at the player's frame
    if (sda_ready(cpu)) {  // each player their own character's stats
        coop_own_stats(cpu, mem_read32(cpu, cpu->gpr[13] - 21492u));
        if (g_coop_p2 != 0u && g_settings.mod_coop && coop_listed(cpu, g_coop_p2)) {
            coop_own_stats(cpu, g_coop_p2);
            // ED_COOP_P2_MAGICK=n (testing): player 2's magick set to n once,
            // as a spell's cost sets it, with them the player as in their turn
            // (the setter tells only the player's bars).
            static int test_magick = -2;
            if (test_magick == -2)
                test_magick = getenv("ED_COOP_P2_MAGICK") != NULL ? atoi(getenv("ED_COOP_P2_MAGICK")) : -1;
            if (test_magick >= 0) {
                const u32 args[4] = {g_coop_p2, 2u, (u32)test_magick, 1u};  // 1: tell their bar
                const u32 who[3] = {cpu->gpr[13] - 26524u, cpu->gpr[13] - 21492u, cpu->gpr[13] - 21496u};
                const u32 now[3] = {mem_read32(cpu, g_coop_p2 + 52u), g_coop_p2, mem_read32(cpu, g_coop_p2 + 16u)};
                u32 was[3], ignore = 0u;
                for (int i = 0; i < 3; i++) {
                    was[i] = mem_read32(cpu, who[i]);
                    mem_write32(cpu, who[i], now[i]);
                }
                hle_guest_call(cpu, 0x800354F8u, args, 4, 0.0, &ignore);
                for (int i = 0; i < 3; i++)
                    mem_write32(cpu, who[i], was[i]);
                fprintf(stderr, "[coop] player 2's magick set to %d\n", test_magick);
                test_magick = -1;
            }
        }
    }
    if (g_coop_turn || g_coop_p2 == 0u || !g_settings.mod_coop || !coop_listed(cpu, g_coop_p2))
        return;
    // While the player is busy (a close-up, a thing taken) the game runs it
    // for player 1 alone: player 2's turn, which pumps the game's input with
    // their pad and is "the player" for a while, kept it from going on.
    if (coop_busy(cpu) && !coop_door_turns(cpu))
        return;
    g_coop_turn = true;
    coop_kinds(cpu, true);  // their own counts, for what they fire and use
    const u32 node = cpu->gpr[3];
    g_coop_p1_id = mem_read32(cpu, node + 16u);
    coop_state(cpu, &g_coop_state[0], true);
    if (g_coop_p2_state)
        coop_state(cpu, &g_coop_state[1], false);
    // What the game's input lets through (each input channel's +16, 32 bytes
    // apart): set for a window or a lock and cleared in the game's, which is
    // player 1's between turns. Player 2's own copy kept a lock for good, and
    // their stick did nothing. Theirs is the game's as it stands.
    for (u32 at = COOP_INPUT_MASKS; at + 4u <= COOP_INPUT_SIZE; at += 32u) {
        const u8* b = g_coop_state[0].input + at;
        const u32 mask = (u32)b[0] << 24 | (u32)b[1] << 16 | (u32)b[2] << 8 | b[3];
        mem_write32(cpu, COOP_INPUT + at, mask == 0x0FFFFFFEu ? 0x0FFFFFFFu : mask);
    }
    u32 ignore = 0u;
    g_coop_pad_p2 = true;
    hle_guest_call(cpu, COOP_PAD_UPDATE, NULL, 0, 0.0, &ignore);
    g_coop_pad_p2 = false;
    const u32 p2 = g_coop_p2;
    u8 camera[24];
    float eye[3], target[3];
    const bool own_view = camera_player2_view(eye, target);
    coop_bytes(cpu, COOP_CAMERA, camera, sizeof camera, true);
    for (u32 i = 0; own_view && i < 3u; i++) {
        guest_write_f32(cpu, COOP_CAMERA + i * 4u, eye[i]);
        guest_write_f32(cpu, COOP_CAMERA + 12u + i * 4u, target[i]);
    }
    // Player 2 is "the player" for their turn: the game's checks for doors,
    // things to look at and what the player stands by ask round them.
    const u32 who[3] = {cpu->gpr[13] - 26524u, cpu->gpr[13] - 21492u, cpu->gpr[13] - 21496u};
    u32 was[3];
    const u32 now[3] = {mem_read32(cpu, p2 + 52u), p2, mem_read32(cpu, p2 + 16u)};
    for (int i = 0; i < 3; i++) {
        was[i] = mem_read32(cpu, who[i]);
        if (g_coop_p2_is_player)
            mem_write32(cpu, who[i], now[i]);
    }
    g_coop_turn_p1 = was[1];
    // What player 2 is next to, by the game's own scan for the player (as
    // each character's step makes it for player 1: info = 0x80123F70(object);
    // scan(object, info[180], info[176], out)); the controller then acts on
    // it as it does for player 1 (opening the door they stand by). Player 1's
    // record and prompt state come back with their state after the turn.
    // Off unless ED_COOP_P2_SCAN=1 (testing): a scan for player 2 kept player 1
    // from opening doors (something of the door's own); player 2 opens doors
    // through player 1's own instead (coop_door_proxy). The words round the
    // record are only put back with the scan: they hold player 2's walking
    // too, which putting them back undid.
    static int scan = -1;
    if (scan < 0)
        scan = getenv("ED_COOP_P2_SCAN") != NULL;
    static u8 p1_prompt[2060];
    const u32 prompt = cpu->gpr[13] - 24040u;
    if (scan)
        coop_bytes(cpu, prompt, p1_prompt, sizeof p1_prompt, true);
    if (!scan && g_coop_p2_is_player) {
        coop_prompt_look(cpu, now[0]);
        // Their B with a prompt: its script runs at player 1's controller
        // (coop_use_start).
        static bool b_was;
        const bool b = (g_coop_p2_buttons & 0x0200u) != 0u;
        if (g_coop_use_own && b && !b_was && getenv("ED_COOP_LOG") != NULL)
            fprintf(stderr, "[coop] player 2's B: prompt %d script %d\n", g_coop_prompt, g_coop_prompt_script);
        if (g_coop_use_own && b && !b_was && g_coop_prompt >= 0 && !coop_door_turns(cpu) &&
            (g_coop_prompt_script > 0 || g_coop_prompt_script == COOP_PROMPT_CLOSE_UP)) {
            if (coop_player1_mid_use(cpu)) {
                if (getenv("ED_COOP_LOG") != NULL)
                    fprintf(stderr, "[coop] player 2's press waits for nothing: player 1's look or find is up\n");
            } else {
                g_coop_use_script = g_coop_prompt_script;
                g_coop_use_prompt = g_coop_prompt;
            }
        }
        b_was = b;
    }
    if (scan && g_coop_p2_is_player && g_coop_scan) {
        u32 info = 0u;
        if (hle_guest_call(cpu, COOP_CHARACTER_INFO, &now[0], 1, 0.0, &info) &&
            guest_ram_range_valid(cpu, info, 184u)) {
            const u32 out = (cpu->gpr[1] - 0x400u) & ~15u;
            const u32 args[4] = {now[0], mem_read32(cpu, info + 180u), mem_read16(cpu, info + 176u), out};
            hle_guest_call(cpu, COOP_SCAN, args, 4, 0.0, &ignore);
        }
    }
    // Their own look for threats round them (as the player's own step does it,
    // every 16th frame), what it found theirs: they square up to enemies.
    const u32 threat_at[2] = {cpu->gpr[13] + (u32)COOP_THREAT[0], cpu->gpr[13] + (u32)COOP_THREAT[1]};
    u32 p1_threat[2];
    for (int i = 0; i < 2; i++) {
        p1_threat[i] = mem_read32(cpu, threat_at[i]);
        mem_write32(cpu, threat_at[i], g_coop_threat[i]);
    }
    hle_guest_call(cpu, COOP_THREAT_LOOK, &p2, 1, 0.0, &ignore);
    if (g_coop_p2_is_player)
        coop_quick_cast(cpu);  // their quick spells, as the player
    const bool ran = hle_guest_call(cpu, COOP_CONTROLLER, &p2, 1, 0.0, &ignore);
    if (g_coop_p2_is_player)
        coop_camera_step(cpu);
    for (int i = 0; i < 2; i++) {
        const u32 found = mem_read32(cpu, threat_at[i]);
        if (found != g_coop_threat[i] && getenv("ED_COOP_ENEMY_LOG") != NULL)
            fprintf(stderr, "[coop] player 2's threat %d: 0x%08X -> 0x%08X (player 1's 0x%08X)\n", i, g_coop_threat[i],
                    found, p1_threat[i]);
        g_coop_threat[i] = found;
        mem_write32(cpu, threat_at[i], p1_threat[i]);
    }
    coop_state(cpu, &g_coop_state[1], true);  // with what they found, for the next turn
    g_coop_p2_state = true;
    if (scan)
        coop_words_back(cpu, p1_prompt);
    for (int i = 0; i < 3; i++)
        mem_write32(cpu, who[i], was[i]);
    coop_bytes(cpu, COOP_CAMERA, camera, sizeof camera, false);
    coop_state(cpu, &g_coop_state[0], false);
    coop_restore_held(cpu);
    coop_rehold(cpu);
    coop_kinds(cpu, false);  // out first: a copy is given a kind of its own only with them out
    coop_copies_follow(cpu);
    g_coop_turn = false;
    static bool told;
    if (!told) {
        told = true;
        fprintf(stderr, "[coop] player 2's first turn %s (player 1's node 0x%08X, player 2's 0x%08X)\n",
                ran ? "ran" : "did not return", node, p2);
    }
}

// The players are not each other's threat. A character's threat search
// (0x80032174: who in its room, within reach, should it square up to) asks
// each other character with events 59, then 239, sent through the game's
// event call (0x801F59B0(event, from, to, data), its answer in r4: 1 a
// threat); another Alex answers yes, and both stood with their fists up. Asked
// between the two players, the answer is no; everything else goes through.
// Player 1's id is kept from before player 2 is the player (their turn, their
// look for threats): asked then, "the player" is player 2, and player 2 took
// player 1 for a threat (on guard with no enemy near).
#define COOP_SEND_EVENT 0x801F59B0u

static void coop_send_event(CPUState* cpu) {
    const u32 event = cpu->gpr[3], from = cpu->gpr[4], to = cpu->gpr[5];
    if ((event == 59u || event == 239u) && g_coop_p2 != 0u && sda_ready(cpu)) {
        const u32 player = coop_player(cpu);
        if (player != 0u && player != g_coop_p2)
            g_coop_p1_id = mem_read32(cpu, player + 16u);
        const u32 ids[2] = {g_coop_p1_id, mem_read32(cpu, g_coop_p2 + 16u)};
        if ((from == ids[0] && to == ids[1]) || (from == ids[1] && to == ids[0])) {
            cpu->gpr[3] = 0u;
            cpu->gpr[4] = 0u;
            return;
        }
    }
    g_hle_passthrough = true;
}

// Through doors together. A character's room is its object's +592 (what the
// game's set-room call, 0x801F64C8(node, room), stores). The players stay in
// one room: when player 2's is not player 1's (player 1 went through a door,
// or player 2 did alone), player 2 is put in player 1's room, beside them.

#define COOP_FALLEN 2000.0f  // below where they last stood still by more, player 2 has fallen out
#define COOP_INSIDE 30.0f    // nearer player 1 than this (across the floor), player 2 is inside them

#define COOP_STATE_DOOR 121u
#define COOP_STATE_ARRIVING 80u
#define COOP_NODE_HELD_SET 0x801F6828u
static bool g_coop_door_held;  // held while the game is busy, before a door of theirs let them go on

static void coop_follow(CPUState* cpu, u32 player) {
    const u32 o1 = mem_read32(cpu, player + 52u), o2 = mem_read32(cpu, g_coop_p2 + 52u);
    if (!guest_ram_range_valid(cpu, o1, 64u) || !guest_ram_range_valid(cpu, o2, 64u) ||
        !guest_ram_range_valid(cpu, o1 + COOP_OBJECT_ROOM, 4u) || !guest_ram_range_valid(cpu, o2 + COOP_OBJECT_ROOM, 4u))
        return;
    const u32 room = mem_read32(cpu, o1 + COOP_OBJECT_ROOM);
    if (room == 0u)
        return;
    // ED_COOP_P2_PUT=frame,x,y,z (testing): player 2 put there at that frame, to reach a rope or a desk
    // from a save without a walk. A place in the same room.
    {
        const char* put = getenv("ED_COOP_P2_PUT");
        static bool done;
        unsigned long long at_frame = 0ull;
        float place[3];
        if (put != NULL && !done && sscanf(put, "%llu,%f,%f,%f", &at_frame, &place[0], &place[1], &place[2]) == 4 && cpu->timebase / 675000ull >= at_frame) {
            done = true;
            for (u32 i = 0u; i < 3u; i++)
                guest_write_f32(cpu, o2 + i * 4u, place[i]);
            fprintf(stderr, "[coop] test: player 2 put at %.0f %.0f %.0f\n", place[0], place[1], place[2]);
        }
    }
    // Fallen out of the room (no floor under them: Paul, put beside Pius in
    // the Tome's room, was pushed off a rock as its scene ended and fell for
    // ever), or standing still inside player 1 for a second (the game does
    // not part them): beside player 1 again. Fallen is far below where they
    // last stood still: player 1 may be a floor above them (the church's
    // stairs) and they have not fallen. Not in a scene, which may hold
    // either player anywhere.
    static float was[3], stood_z;
    static unsigned inside;
    const float z2 = guest_read_f32(cpu, o2 + 8u);
    if (mem_read32(cpu, o2 + COOP_OBJECT_ROOM) == room) {
        float now[3];
        for (u32 i = 0; i < 3u; i++)
            now[i] = guest_read_f32(cpu, o2 + i * 4u);
        const bool still = fabsf(now[0] - was[0]) < 1.0f && fabsf(now[1] - was[1]) < 1.0f && fabsf(now[2] - was[2]) < 1.0f;
        memcpy(was, now, sizeof was);
        if (!isfinite(stood_z) || still)
            stood_z = z2;
        const float dx = now[0] - guest_read_f32(cpu, o1), dy = now[1] - guest_read_f32(cpu, o1 + 4u);
        const float dz = z2 - guest_read_f32(cpu, o1 + 8u);
        inside = dx * dx + dy * dy < COOP_INSIDE * COOP_INSIDE && fabsf(dz) < COOP_PLACE_STEP && still ? inside + 1u : 0u;
        const bool fallen = !(z2 >= stood_z - COOP_FALLEN);
        static unsigned long long retry;  // no place beside player 1 found: not again before this frame
        const unsigned long long frame = cpu->timebase / 675000ull;
        if ((fallen || (inside >= 60u && frame >= retry)) && !cutscene_playing(cpu)) {
            float at[3];
            coop_place(cpu, o1, at);
            const float ax = at[0] - guest_read_f32(cpu, o1), ay = at[1] - guest_read_f32(cpu, o1 + 4u);
            if (!fallen && ax * ax + ay * ay < COOP_INSIDE * COOP_INSIDE) {
                retry = frame + 600u;
                inside = 0u;
                return;
            }
            for (u32 i = 0; i < 3u; i++)
                guest_write_f32(cpu, o2 + i * 4u, at[i]);
            fprintf(stderr, "[coop] player 2 %s (at %.0f, stood at %.0f): beside player 1, at %.0f %.0f %.0f\n",
                    fallen ? "fell out of the room" : "stood inside player 1", z2, stood_z, at[0], at[1], at[2]);
            stood_z = at[2];
            inside = 0u;
        }
        return;
    }
    float at[3];
    coop_place(cpu, o1, at);
    for (u32 i = 0; i < 3u; i++) {
        // They asked for the room: its arrival spot is theirs, what plays there framed on it.
        const float arrival = guest_read_f32(cpu, o1 + i * 4u);
        guest_write_f32(cpu, o2 + i * 4u, g_coop_entered_p2 ? arrival : at[i]);
        if (g_coop_entered_p2)
            guest_write_f32(cpu, o1 + i * 4u, at[i]);
    }
    for (u32 i = 0; i < 4u; i++)  // facing as player 1
        mem_write32(cpu, o2 + 44u + i * 4u, mem_read32(cpu, o1 + 44u + i * 4u));
    stood_z = at[2];
    const u32 args[] = {g_coop_p2, room};
    u32 ignore = 0u;
    hle_guest_call(cpu, COOP_SPAWN_SET_ROOM, args, 2, 0.0, &ignore);
    g_coop_arrived = true;
    g_coop_show_kept = false;
    g_coop_unseen_out_of = -1;
    g_coop_door_turns_until = 0ull;
    // Through a door of theirs (its motion's state still theirs): the room change
    // tells the player they have arrived, which has them see the motion out and
    // stand as they should (state 80, then 81): player 1 alone. They go the same
    // way. Told the event, they stopped where they were, twisted, in mid motion.
    if (mem_read32(cpu, g_coop_p2 + COOP_NODE_STATE) == COOP_STATE_DOOR) {
        const u32 state[2] = {g_coop_p2, COOP_STATE_ARRIVING};
        const u32 fresh[2] = {g_coop_p2, 1u};
        hle_guest_call(cpu, 0x801F64A0u, state, 2, 0.0, &ignore);
        hle_guest_call(cpu, 0x801F6488u, fresh, 2, 0.0, &ignore);
    }
    if (g_coop_door_held) {
        const u32 hold[2] = {g_coop_p2, 1u};
        g_coop_door_held = false;
        hle_guest_call(cpu, COOP_NODE_HELD_SET, hold, 2, 0.0, &ignore);
    }
    fprintf(stderr, "[coop] player 2 brought to player 1's room 0x%08X, at %.0f %.0f %.0f\n", room, at[0], at[1],
            at[2]);
}

// Player 2's detail. Each frame the game shares out detail between the
// characters by where they are from its camera (0x8011CA04): one in front of
// the camera and near it may have full detail, the rest less, and their models
// are made ready for that detail before they are drawn. The game camera is
// player 1's, so player 2 close in their own view but behind player 1's
// camera was the low-detail model. Where that share reads a character's
// position (0x80118274, a copy of object +0..11, called from 0x8011CB70),
// each player is given a place just in front of the game camera, so both
// have full detail in both views: player 1's place was not enough when the
// camera looks away from player 1 too (Anthony's chapter opens on a pan to
// the windows; both players were low detail in player 2's view).
// ED_COOP_LOD_LOG=1 says so once a second.
#define COOP_POSITION_COPY 0x80118274u
#define COOP_DETAIL_CALL 0x8011CB74u  // the return address in the share

static void coop_position_copy(CPUState* cpu) {
    const u32 from = cpu->gpr[4];
    u32 at[3];
    for (u32 i = 0; i < 3u; i++)
        at[i] = mem_read32(cpu, from + i * 4u);
    // What a player holds (its holder, object +372, is them) too: out of
    // player 1's view, player 2's sword was left where they had been.
    const u32 p2 = g_coop_p2 != 0u ? mem_read32(cpu, g_coop_p2 + 52u) : 0u;
    if (cpu->lr == COOP_DETAIL_CALL && g_settings.mod_coop && p2 != 0u && sda_ready(cpu)) {
        const u32 player = coop_player(cpu);
        const u32 p1 = player != 0u ? mem_read32(cpu, player + 52u) : 0u;
        const u32 holder = guest_ram_range_valid(cpu, from + 372u, 4u) ? mem_read32(cpu, from + 372u) : 0u;
        if (from == p2 || from == p1 || (holder != 0u && (holder == p2 || holder == p1))) {
            for (u32 i = 0; i < 3u; i++) {  // a tenth of the way from the eye to what it looks at
                const float eye = guest_read_f32(cpu, COOP_CAMERA + i * 4u);
                const float look = guest_read_f32(cpu, COOP_CAMERA + 12u + i * 4u);
                const float v = eye + (look - eye) * 0.1f;
                memcpy(&at[i], &v, 4u);
            }
            static unsigned n;
            if (getenv("ED_COOP_LOD_LOG") != NULL && n++ % 60u == 0u)
                fprintf(stderr, "[coop] %s's detail by the camera\n", from == p1 || holder == p1 ? "player 1" : "player 2");
        }
    }
    for (u32 i = 0; i < 3u; i++)
        mem_write32(cpu, cpu->gpr[3] + i * 4u, at[i]);
    cpu->gpr[5] = at[0];
    cpu->gpr[0] = at[2];
}

// Player 2's object, for the skinning log (ED_COOP_SKIN_LOG).
u32 hle_coop_player2_object(CPUState* cpu) {
    return g_coop_p2 != 0u ? mem_read32(cpu, g_coop_p2 + 52u) : 0u;
}

// Player 2's own look round for doors and things. Each character's step
// (0x80133434, fn(object, ...)) scans round it only when it is the player;
// for player 2's it runs with them as the player, their own last scan in the
// shared record, which keeps what it finds for their turn, and player 1's put
// back after.
#define COOP_CHARACTER_STEP 0x80133434u
static bool g_coop_step_running;

// Seen by an enemy, a player loses sanity. A character's look round for the
// one it is after (0x8003B1C8(node, ...)) marks it as having seen them, and
// sends them its sanity hit (event 58, from it; their own answer is the hit,
// event 39), only when that one is the player (0x800324B8(node, 0), its
// quarry, against the player's id); for one after player 2, player 2 is the
// player while it looks. When it sees one player, the other gets the hit too
// if they are in its view as well.
#define COOP_TARGET_A 12u
#define COOP_TARGET_B 20u
#define COOP_SIGHTING 0x8003B1C8u
#define COOP_EVENTS 0x80058DD8u
#define COOP_SEEN_EVENT 58u
#define COOP_HIT_EVENT 39u
#define COOP_VIEW_HALF 1.0471976f  // 60 degrees
#define COOP_VIEW_REACH 1500.0f
#define COOP_EYE_HEIGHT 60.0f
static bool g_coop_sighting;

// fn(r3..r10) in place of the call being made, with player 2 as the player
// (their object, node and id where the game keeps the player's), its answer
// the caller's.
static void coop_as_player2(CPUState* cpu, u32 fn) {
    u32 args[8];
    for (int i = 0; i < 8; i++)
        args[i] = cpu->gpr[3 + i];
    const u32 who[3] = {cpu->gpr[13] - 26524u, cpu->gpr[13] - 21492u, cpu->gpr[13] - 21496u};
    const u32 now[3] = {mem_read32(cpu, g_coop_p2 + 52u), g_coop_p2, mem_read32(cpu, g_coop_p2 + 16u)};
    u32 was[3], result = 0u;
    for (int i = 0; i < 3; i++) {
        was[i] = mem_read32(cpu, who[i]);
        mem_write32(cpu, who[i], now[i]);
    }
    hle_guest_call(cpu, fn, args, 8, cpu->fpr[1], &result);
    for (int i = 0; i < 3; i++)
        mem_write32(cpu, who[i], was[i]);
    cpu->gpr[3] = result;
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

// A script's asking for a room plays its motion on whoever is the player
// (0x801669AC(motion): event 253 to them; 25 a door, from 0x801666A4): player 1,
// the script being run as theirs so the rooms load round them, whoever opened
// it. The one who opened it goes through it.
#define COOP_DOOR_MOTION 0x801669ACu
#define COOP_DOOR_FADE 120u  // frames at most: a door's fade is 60

static void coop_door_motion(CPUState* cpu) {
    const unsigned long long frame = cpu->timebase / 675000ull;
    const bool theirs = g_coop_door_p2_until != 0ull && frame <= g_coop_door_p2_until && g_settings.mod_coop &&
                        g_coop_p2 != 0u && sda_ready(cpu);
    g_coop_door_p2_until = 0ull;  // once: not for this call again, nor player 1's next door
    if (!theirs) {
        g_hle_passthrough = true;
        return;
    }
    if (getenv("ED_COOP_LOG") != NULL)
        fprintf(stderr, "[coop] player 2 opened it: motion %d is theirs\n", (s32)cpu->gpr[3]);
    g_coop_door_turns_until = frame + COOP_DOOR_FADE;
    // The room's asking lets the player go on while the game is busy (0x801F6828
    // (player, 0): their +28's 8, which holds a character then): only player 1, so
    // the motion stood still through the fade. Theirs too, until they arrive.
    const u32 go_on[2] = {g_coop_p2, 0u};
    u32 ignore = 0u;
    g_coop_door_held = (mem_read32(cpu, g_coop_p2 + 28u) & 8u) != 0u;
    hle_guest_call(cpu, COOP_NODE_HELD_SET, go_on, 2, 0.0, &ignore);
    coop_as_player2(cpu, COOP_DOOR_MOTION);
}

// Someone to talk to, or the like, looks for the player each frame
// (0x80091F0C(them, their object): whoever is the player, who if near enough has
// event 147, whose answer is the prompt: 22, script 1902): only ever player 1.
// It looks for player 2 too, with them the player and their prompts in.
#define COOP_TALK_CHECK 0x80091F0Cu

static void coop_talk_check(CPUState* cpu) {
    static bool running;
    g_hle_passthrough = true;
    g_hle_tail_call = false;
    if (running || g_coop_turn || g_coop_p2 == 0u || !g_coop_p2_state || !g_settings.mod_coop || !g_coop_use_own ||
        !sda_ready(cpu) || coop_busy(cpu))
        return;
    const u32 o1 = mem_read32(cpu, cpu->gpr[13] - 26524u), o2 = mem_read32(cpu, g_coop_p2 + 52u);
    if (!guest_ram_range_valid(cpu, o1, COOP_OBJECT_ROOM + 4u) || !guest_ram_range_valid(cpu, o2, COOP_OBJECT_ROOM + 4u) ||
        mem_read32(cpu, o1 + COOP_OBJECT_ROOM) != mem_read32(cpu, o2 + COOP_OBJECT_ROOM))
        return;
    const unsigned long long frame = cpu->timebase / 675000ull;
    if (!g_coop_prompt_slots_set)
        coop_prompt_slots_clear();
    if (frame != g_coop_talk_frame)
        coop_talk_slot_clear();  // asked again each frame it stands
    g_coop_talk_frame = frame;
    u32 args[8];
    for (int i = 0; i < 8; i++)
        args[i] = cpu->gpr[3 + i];
    static u8 slots[PROMPT_SLOTS_SIZE];
    coop_bytes(cpu, PROMPT_SLOTS, slots, sizeof slots, true);
    coop_bytes(cpu, PROMPT_SLOTS, g_coop_prompt_slots, PROMPT_SLOTS_SIZE, false);
    const u32 who[3] = {cpu->gpr[13] - 26524u, cpu->gpr[13] - 21492u, cpu->gpr[13] - 21496u};
    const u32 now[3] = {o2, g_coop_p2, mem_read32(cpu, g_coop_p2 + 16u)};
    u32 was[3], ignore = 0u;
    for (int i = 0; i < 3; i++) {
        was[i] = mem_read32(cpu, who[i]);
        mem_write32(cpu, who[i], now[i]);
    }
    running = true;
    hle_guest_call(cpu, COOP_TALK_CHECK, args, 8, cpu->fpr[1], &ignore);
    running = false;
    for (int i = 0; i < 3; i++)
        mem_write32(cpu, who[i], was[i]);
    coop_bytes(cpu, PROMPT_SLOTS, g_coop_prompt_slots, PROMPT_SLOTS_SIZE, true);
    coop_bytes(cpu, PROMPT_SLOTS, slots, sizeof slots, false);
    for (int i = 0; i < 8; i++)
        cpu->gpr[3 + i] = args[i];
    g_hle_passthrough = true;
    g_hle_tail_call = false;
}

static void coop_sighting(CPUState* cpu) {
    const u32 node = cpu->gpr[3];
    const u32 stats = guest_ram_range_valid(cpu, node, 56u) ? mem_read32(cpu, node + (u32)(s32)g_coop_stats_offset) : 0u;
    if (g_coop_sighting || g_coop_turn || !g_settings.mod_coop || g_coop_p2 == 0u || !g_coop_p2_state ||
        !sda_ready(cpu) || !guest_ram_range_valid(cpu, stats, COOP_TARGET_B + 4u) ||
        mem_read32(cpu, stats + COOP_TARGET_B) != mem_read32(cpu, g_coop_p2 + 16u)) {
        g_hle_passthrough = true;
        return;
    }
    g_coop_sighting = true;
    coop_as_player2(cpu, COOP_SIGHTING);
    g_coop_sighting = false;
}

static void coop_seen_shared(CPUState* cpu);
static bool g_coop_seen_p2;

static void coop_events(CPUState* cpu) {
    const u32 node = cpu->gpr[3], message = cpu->gpr[5];
    if (!g_coop_seen_p2)  // not again for the call made here
        coop_seen_shared(cpu);
    if (g_coop_seen_p2 || !g_settings.mod_coop || g_coop_p2 == 0u || node != g_coop_p2 || !sda_ready(cpu) ||
        !guest_ram_range_valid(cpu, message, 4u) || mem_read32(cpu, message) != COOP_SEEN_EVENT) {
        g_hle_passthrough = true;
        return;
    }
    g_coop_seen_p2 = true;
    coop_as_player2(cpu, COOP_EVENTS);
    g_coop_seen_p2 = false;
}

// Whether a character is looking a player's way: within its view (60 degrees
// either side of its facing), in reach, nothing between.
static bool coop_in_view(CPUState* cpu, u32 seer, u32 them, float* reach, float* off) {
    float to[3], from[3], at[3];
    for (u32 i = 0; i < 3u; i++) {
        from[i] = guest_read_f32(cpu, seer + i * 4u);
        at[i] = guest_read_f32(cpu, them + i * 4u);
        to[i] = at[i] - from[i];
    }
    const float qz = guest_read_f32(cpu, seer + 52u), qw = guest_read_f32(cpu, seer + 56u);
    const float facing = 2.0f * atan2f(qz, qw) - 1.5707963f;
    *reach = sqrtf(to[0] * to[0] + to[1] * to[1]);
    *off = atan2f(to[1], to[0]) - facing;
    while (*off > 3.14159265f)
        *off -= 6.2831853f;
    while (*off < -3.14159265f)
        *off += 6.2831853f;
    from[2] += COOP_EYE_HEIGHT;
    at[2] += COOP_EYE_HEIGHT;
    float hit[3];
    return isfinite(*off) && fabsf(*off) <= COOP_VIEW_HALF && *reach <= COOP_VIEW_REACH &&
           !camera_collide(cpu, from, at, 0.0f, hit);
}

// What each enemy that has seen a player left: whom it has hit (0 player 1, 1
// player 2) and what its hit carries.
#define COOP_SEEN_MAX 32u
static struct {
    u32 node, id, data;
    bool hit[2];
} g_coop_seen[COOP_SEEN_MAX];

static void coop_seen_note(u32 node, u32 id, u32 data, bool hit_p1, bool hit_p2) {
    static u32 next;
    u32 s = COOP_SEEN_MAX;
    for (u32 i = 0u; i < COOP_SEEN_MAX && s == COOP_SEEN_MAX; i++)
        if (g_coop_seen[i].node == node)
            s = i;
    for (u32 i = 0u; i < COOP_SEEN_MAX && s == COOP_SEEN_MAX; i++)
        if (g_coop_seen[i].node == 0u)
            s = i;
    if (s == COOP_SEEN_MAX)
        s = next++ % COOP_SEEN_MAX;
    g_coop_seen[s].node = node;
    g_coop_seen[s].id = id;
    g_coop_seen[s].data = data;
    g_coop_seen[s].hit[0] = hit_p1;
    g_coop_seen[s].hit[1] = hit_p2;
}

// An enemy's sanity hit (event 39, from it) to a player.
static void coop_seen_hit(CPUState* cpu, u32 player, u32 seer_id, u32 data) {
    const u32 args[4] = {COOP_HIT_EVENT, seer_id, mem_read32(cpu, player + 16u), data};
    // Taken by player 2 as the player, as their own (the glow over them).
    const u32 who[3] = {cpu->gpr[13] - 26524u, cpu->gpr[13] - 21492u, cpu->gpr[13] - 21496u};
    const u32 now[3] = {mem_read32(cpu, g_coop_p2 + 52u), g_coop_p2, mem_read32(cpu, g_coop_p2 + 16u)};
    u32 was[3], ignore = 0u;
    for (int i = 0; i < 3 && player == g_coop_p2; i++) {
        was[i] = mem_read32(cpu, who[i]);
        mem_write32(cpu, who[i], now[i]);
    }
    hle_guest_call(cpu, COOP_SEND_EVENT, args, 4, 0.0, &ignore);
    for (int i = 0; i < 3 && player == g_coop_p2; i++)
        mem_write32(cpu, who[i], was[i]);
}

static void coop_seen_shared(CPUState* cpu) {
    const u32 node = cpu->gpr[3], message = cpu->gpr[5];
    // Each event comes twice, asked (r4 1) then taken (r4 0): the second.
    if (!g_settings.mod_coop || g_coop_p2 == 0u || cpu->gpr[4] != 0u || !sda_ready(cpu) ||
        !guest_ram_range_valid(cpu, message, 32u) || mem_read32(cpu, message) != COOP_SEEN_EVENT)
        return;
    const u32 p1 = mem_read32(cpu, cpu->gpr[13] - 21492u);
    if (node != p1 && node != g_coop_p2)
        return;
    const u32 other = node == p1 ? g_coop_p2 : p1;
    if (!guest_ram_range_valid(cpu, other, 56u) || mem_read32(cpu, message + 8u) != mem_read32(cpu, node + 16u))
        return;
    // The other only when the one who saw is looking their way too: within
    // its view (60 degrees either side of its facing), in reach, nothing
    // between.
    const u32 seer_id = mem_read32(cpu, message + 4u);
    u32 seer = 0u, seer_node = 0u;
    for (u32 n = 0u, i = 0u; i < 1024u && (n = coop_next(cpu, n)) != 0u; i++)
        if (mem_read32(cpu, n + 16u) == seer_id) {
            seer_node = n;
            seer = mem_read32(cpu, n + 52u);
            break;
        }
    const u32 them = mem_read32(cpu, other + 52u);
    if (!guest_ram_range_valid(cpu, seer, 60u) || !guest_ram_range_valid(cpu, them, 16u))
        return;
    float reach, off;
    const bool in_view = coop_in_view(cpu, seer, them, &reach, &off);
    coop_seen_note(seer_node, seer_id, mem_read32(cpu, message + 28u), node == p1 || in_view, node != p1 || in_view);
    static int log = -1;
    if (log < 0)
        log = getenv("ED_COOP_ENEMY_LOG") != NULL;
    if (log)
        fprintf(stderr, "[coop] %u saw player %d; player %d %s (%.0f away, %.0f degrees off)\n", seer_id,
                node == p1 ? 1 : 2, node == p1 ? 2 : 1, in_view ? "in its view too" : "not in its view", reach,
                off * 57.29578f);
    if (!in_view)
        return;
    coop_seen_hit(cpu, other, seer_id, mem_read32(cpu, message + 28u));
}

// An enemy sees once: the game's mark on it (0x80033874, its 8) stops it telling
// anyone again, so a player who came into its view after that lost no sanity.
// Each step of an enemy that has seen one player, the other takes the same hit,
// once, as they come into its view.
static void coop_seen_later(CPUState* cpu, u32 node, u32 object, const u32 nodes[2], const u32 objects[2], u32 stats) {
    for (u32 s = 0u; s < COOP_SEEN_MAX; s++) {
        if (g_coop_seen[s].node != node || (g_coop_seen[s].hit[0] && g_coop_seen[s].hit[1]))
            continue;
        u32 flags = 0u;
        hle_guest_call(cpu, 0x80033874u, &node, 1, 0.0, &flags);
        if ((flags & 8u) == 0u || mem_read32(cpu, node + 16u) != g_coop_seen[s].id) {  // not the one that saw
            g_coop_seen[s].node = 0u;
            return;
        }
        if ((s16)mem_read16(cpu, stats + (u32)(s32)g_coop_health_offset) <= 0 || coop_busy(cpu) || cutscene_playing(cpu))
            return;
        for (int k = 0; k < 2; k++) {
            float reach, off;
            if (g_coop_seen[s].hit[k] || !coop_in_view(cpu, object, objects[k], &reach, &off))
                continue;
            g_coop_seen[s].hit[k] = true;
            if (getenv("ED_COOP_ENEMY_LOG") != NULL)
                fprintf(stderr, "[coop] %u seen already; player %d comes into its view (%.0f away, %.0f degrees off)\n",
                        g_coop_seen[s].id, k + 1, reach, off * 57.29578f);
            coop_seen_hit(cpu, nodes[k], g_coop_seen[s].id, g_coop_seen[s].data);
        }
        return;
    }
}

// Player 2's health, sanity and magick bars. Each bar is a node of its own (class 11,
// handler 0x80029454): its info's +20 is its record (44 bytes: +0 the draw,
// 0x80023348; +4, +8 its fill; +28 the kind, 0 health, 1 sanity, 2 magick;
// +32, +34 where it is on the screen; +42 how much it shows), which its
// frame's events (0x801F5D48(node): events 3 and 4 to itself) put in the
// HUD's list (0x80022C50) for the HUD's draw (0x80022E60). A player's stats
// are told to their bars: 0x80033758(node, kind, value) sends event 16 to the
// id at their info's +140 list, +4 + kind * 4; player 1's has their bars'
// ids, player 2's none. Player 2 has their own: copies of player 1's nodes,
// info and records (in the scratch, SCRATCH_HUD), with ids of their own in
// their list, which the game's look-up of a node by id (0x801F5F88) answers
// with; each copy has its frame when player 1's bar has theirs. Their records
// sit COOP_BAR_AWAY to the right, off the game's screen: the split screen's
// renderer shows them in player 2's half, at the far right (mirroring where
// player 1's are from the left), and player 1's not there.
// ED_COOP_BAR_LOG=1 says when the copies are made.
#define COOP_BAR_FRAME 0x801F5D48u
#define COOP_FIND_NODE 0x801F5F88u
#define COOP_BAR_HANDLER 0x80029454u
#define COOP_BAR_CLASS 11u
#define COOP_BARS 3              // health, sanity and magick
#define COOP_AMMO COOP_BARS      // the slot after them: what a throw has left (coop_bar_copy)
#define COOP_AMMO_CLASS 70u
#define COOP_AMMO_LEFT (-120)    // HUD units: player 2's count, as far in from their half's left edge as player 1's
#define COOP_BAR_ID 0x00FFFFF0u  // + kind: past the ids the game gives out
#define COOP_BAR_AWAY 4096       // HUD units; the renderer's split-screen view brings them back
#define COOP_BAR_SCREEN 640      // the HUD's width
#define COOP_BAR_WIDTH 32        // a bar's, end cap to end cap
#define COOP_NODE_SIZE 60u
#define COOP_INFO_SIZE 160u
#define COOP_RECORD_SIZE 44u
#define COOP_BAR_SLOT 0x200u      // a copy's node (+0), info (+64) and record (+224)
#define STAT_MOST 0x80034F7Cu     // (node, kind, out): a character's most health, sanity or magick
static struct {
    u32 from, p2;  // player 1's bar it was copied from, and for whom
} g_coop_bar[COOP_BARS + 1];
static bool g_coop_bar_frame;

static u32 coop_bar_list(CPUState* cpu, u32 node) {
    const u32 info = guest_ram_range_valid(cpu, node, 44u) ? mem_read32(cpu, node + 40u) : 0u;
    const u32 list = guest_ram_range_valid(cpu, info, 144u) ? mem_read32(cpu, info + 140u) : 0u;
    return guest_ram_range_valid(cpu, list, 4u + COOP_BARS * 4u) ? list : 0u;
}

// Player 1's bar of this kind, or 0.
static u32 coop_bar_source(CPUState* cpu, int kind) {
    const u32 list = coop_bar_list(cpu, mem_read32(cpu, cpu->gpr[13] - 21492u));
    const u32 id = list != 0u ? mem_read32(cpu, list + 4u + (u32)kind * 4u) : 0u;
    for (u32 n = 0u, i = 0u; id != 0u && i < 1024u && (n = coop_next(cpu, n)) != 0u; i++)
        if (mem_read32(cpu, n + 16u) == id)
            return mem_read32(cpu, n + 8u) == COOP_BAR_CLASS ? n : 0u;
    return 0u;
}

// Player 2's copy of a kind's bar, made from player 1's when there is none
// for them yet, and its id in their list.
// Player 2's copy of player 1's bar (kind 0..2) or of the count a throw
// shows (COOP_AMMO: player 1's at the bottom right, its id at their info's
// +0, +112, where a throw tells it, 0x800BCF78; player 2's had none, and
// their throws showed nothing), made once.
static u32 coop_bar_copy(CPUState* cpu, int kind, u32 from) {
    const u32 node = SCRATCH_HUD + (u32)kind * COOP_BAR_SLOT;
    const u32 info = node + 64u, record = info + COOP_INFO_SIZE;
    const u32 id = COOP_BAR_ID + (u32)kind;
    const u32 list = coop_bar_list(cpu, g_coop_p2);
    const u32 p2_info = mem_read32(cpu, g_coop_p2 + 40u);
    const u32 hud = guest_ram_range_valid(cpu, p2_info, 4u) ? mem_read32(cpu, p2_info) : 0u;
    if ((kind == COOP_AMMO ? !guest_ram_range_valid(cpu, hud, 116u) : list == 0u) ||
        cpu->ram_size < SCRATCH_END - GC_RAM_BASE)
        return 0u;
    const u32 at = kind == COOP_AMMO ? hud + 112u : list + 4u + (u32)kind * 4u;
    if (mem_read32(cpu, at) != 0u && mem_read32(cpu, at) != id)
        return 0u;  // theirs already (not the game's way, but kept)
    mem_write32(cpu, at, id);
    if (g_coop_bar[kind].from == from && g_coop_bar[kind].p2 == g_coop_p2 && mem_read32(cpu, node + 16u) == id)
        return node;
    const u32 from_info = mem_read32(cpu, from + 40u);
    const u32 from_record = guest_ram_range_valid(cpu, from_info, 24u) ? mem_read32(cpu, from_info + 20u) : 0u;
    if (!guest_ram_range_valid(cpu, from_info, COOP_INFO_SIZE) || !guest_ram_range_valid(cpu, from_record, COOP_RECORD_SIZE))
        return 0u;
    memmove(cpu->ram + (node - GC_RAM_BASE), cpu->ram + (from - GC_RAM_BASE), COOP_NODE_SIZE);
    memmove(cpu->ram + (info - GC_RAM_BASE), cpu->ram + (from_info - GC_RAM_BASE), COOP_INFO_SIZE);
    memmove(cpu->ram + (record - GC_RAM_BASE), cpu->ram + (from_record - GC_RAM_BASE), COOP_RECORD_SIZE);
    mem_write32(cpu, node + 0u, 0u);  // in no list: found by coop_find_node
    mem_write32(cpu, node + 4u, 0u);
    mem_write32(cpu, node + 16u, id);
    mem_write32(cpu, node + 40u, info);
    mem_write32(cpu, info + 20u, record);
    // Mirrored in player 2's half: the bars at the far right, the count at
    // the bottom left (at the bottom right it was over their bars), as near
    // the edge as player 1's: the widescreen HUD moves player 1's out to the
    // screen's edge, and not player 2's copies.
    const s16 x = (s16)mem_read16(cpu, from_record + 32u);
    mem_write16(cpu, record + 32u,
                (u16)(s16)(kind == COOP_AMMO ? COOP_BAR_AWAY + COOP_AMMO_LEFT
                                             : COOP_BAR_AWAY + COOP_BAR_SCREEN - x - COOP_BAR_WIDTH));
    g_coop_bar[kind].from = from;
    g_coop_bar[kind].p2 = g_coop_p2;
    if (getenv("ED_COOP_BAR_LOG") != NULL)
        fprintf(stderr, "[coop] player 2's bar %d (id %u) from player 1's 0x%08X, at %d %d\n", kind, id, from,
                COOP_BAR_SCREEN - x - COOP_BAR_WIDTH, (s16)mem_read16(cpu, record + 34u));
    return node;
}

static int coop_most(CPUState* cpu, u32 node, int kind) {
    const u32 out = (cpu->gpr[1] - 0x300u) & ~15u;
    const u32 args[3] = {node, (u32)kind, out};
    u32 ignore = 0u;
    mem_write32(cpu, out, 0u);
    hle_guest_call(cpu, STAT_MOST, args, 3, 0.0, &ignore);
    return (s16)mem_read16(cpu, out);
}

// A kind's most health, sanity or magick (kind 0..2), from the game's table
// of the kinds of character (0x80319EA0: 175 of 224 bytes, +4 the kind,
// +68.. the most of each), as a character of that kind is made with; 0 when
// not found.
#define KIND_TABLE 0x80319EA0u
#define KIND_SIZE 224u
#define KINDS 175u

static int coop_kind_most(CPUState* cpu, int type, int kind) {
    for (u32 i = 0; i < KINDS; i++) {
        const u32 entry = KIND_TABLE + i * KIND_SIZE;
        if (guest_ram_range_valid(cpu, entry, KIND_SIZE) && mem_read32(cpu, entry + 4u) == (u32)type)
            return (s16)mem_read16(cpu, entry + 68u + (u32)kind * 2u);
    }
    return 0;
}

// The kind of the playable a player is, by their model (Mods' characters:
// the chapter's kind stays theirs, as the story's checks want), when their
// Stats are their chosen character's (mod_p1_stats, mod_p2_stats); -1 when
// they are the chapter character's (Vanilla).
static int coop_player_kind(CPUState* cpu, u32 who) {
    const bool p2 = g_coop_p2 != 0u && who == g_coop_p2;
    if ((p2 ? g_settings.mod_p2_stats : g_settings.mod_p1_stats) != 1)
        return -1;
    const u32 actor = guest_ram_range_valid(cpu, who, 56u) ? mem_read32(cpu, who + (u32)(s32)g_coop_actor_offset) : 0u;
    const s16 model = guest_ram_range_valid(cpu, actor, 156u) ? (s16)mem_read16(cpu, actor + 154u) : -1;
    for (int i = 0; i < 12; i++)
        if (kPlayerModel[i] == model)
            return kPlayerType[i];
    return -1;
}

// A player is their own character, whatever the chapter: the game's most of a
// character's health, sanity or magick (0x80034F7C(node, kind, out): health
// their stats' +224, set from their kind as they are made; sanity and magick
// their kind's own, +70, +72), which caps them and sizes their bars, answers
// for each player with their character's (coop_kind_most). Others (the
// chapter's Pius, say) keep theirs.
static void coop_stat_most(CPUState* cpu) {
    const u32 node = cpu->gpr[3], kind = cpu->gpr[4], out = cpu->gpr[5];
    const bool player = sda_ready(cpu) && node != 0u &&
                        (node == mem_read32(cpu, cpu->gpr[13] - 21492u) || node == coop_player(cpu) ||
                         (g_coop_p2 != 0u && node == g_coop_p2));
    const int type = player && kind < 3u ? coop_player_kind(cpu, node) : -1;
    const int most = type >= 0 ? coop_kind_most(cpu, type, (int)kind) : 0;
    if (most <= 0 || !guest_ram_range_valid(cpu, out, 2u)) {
        g_hle_passthrough = true;
        return;
    }
    mem_write16(cpu, out, (u16)(s16)most);
    cpu->gpr[3] = 1u;
}

// At each player's frame: their stored most health their character's too, and
// none of the three above its most (one made as the chapter's character keeps
// Karim's 150 health until then).
static void coop_own_stats(CPUState* cpu, u32 who) {
    const int type = coop_player_kind(cpu, who);
    const u32 info = type >= 0 ? mem_read32(cpu, who + 40u) : 0u;
    const u32 stats = guest_ram_range_valid(cpu, info, 144u) ? mem_read32(cpu, info + 140u) : 0u;
    if (!guest_ram_range_valid(cpu, stats, 232u))
        return;
    const int health = coop_kind_most(cpu, type, 0);
    if (health > 0)
        mem_write16(cpu, stats + 224u, (u16)(s16)health);
    for (u32 kind = 0; kind < 3u; kind++) {
        const u32 out = (cpu->gpr[1] - 0x300u) & ~15u;
        const u32 args[3] = {who, kind, out};
        u32 ignore = 0u;
        mem_write32(cpu, out, 0u);
        hle_guest_call(cpu, 0x80034E20u, args, 3, 0.0, &ignore);
        const s16 has = (s16)mem_read16(cpu, out);
        const int most = coop_kind_most(cpu, type, (int)kind);
        const u32 set_args[4] = {who, kind, (u32)(s32)most, 0u};
        if (most > 0 && has > most)
            hle_guest_call(cpu, 0x800354F8u, set_args, 4, 0.0, &ignore);
    }
}

// Each character's bar is as long as their most: a record's +16 (and +12,
// which the bar eases to it) is the most / 100 (Ellia's health 0.75, Karim's
// 1.5). A player's is the game's most for them: their chosen character's
// with their Stats so (coop_stat_most), else the chapter character's.
static void coop_bar_length(CPUState* cpu, u32 record, u32 who, int kind) {
    const int most = coop_most(cpu, who, kind);
    const float length = (float)most / 100.0f;
    if (most > 0 && guest_ram_range_valid(cpu, record, COOP_RECORD_SIZE) && guest_read_f32(cpu, record + 16u) != length) {
        guest_write_f32(cpu, record + 12u, length);
        guest_write_f32(cpu, record + 16u, length);
    }
}

// Notify at a node's frame: when it is one of player 1's bars, its length;
// player 2's copy has its frame too.
static void coop_bar_frame(CPUState* cpu) {
    const u32 node = cpu->gpr[3];
    if (g_coop_bar_frame || !sda_ready(cpu) || !guest_ram_range_valid(cpu, node, COOP_NODE_SIZE))
        return;
    if (mem_read32(cpu, node + 8u) == COOP_AMMO_CLASS) {
        // Player 1's count: player 2's has its frame with it.
        const u32 p1_info = mem_read32(cpu, mem_read32(cpu, cpu->gpr[13] - 21492u) + 40u);
        const u32 hud = guest_ram_range_valid(cpu, p1_info, 4u) ? mem_read32(cpu, p1_info) : 0u;
        if (!g_settings.mod_coop || g_coop_p2 == 0u || !coop_listed(cpu, g_coop_p2) ||
            !guest_ram_range_valid(cpu, hud, 116u) || mem_read32(cpu, hud + 112u) != mem_read32(cpu, node + 16u))
            return;
        u32 copy = coop_bar_copy(cpu, COOP_AMMO, node), ignore = 0u;
        if (copy == 0u)
            return;
        g_coop_bar_frame = true;
        hle_guest_call(cpu, COOP_BAR_FRAME, &copy, 1, 0.0, &ignore);
        g_coop_bar_frame = false;
        return;
    }
    if (mem_read32(cpu, node + 8u) != COOP_BAR_CLASS)
        return;
    // Player 1's own bars, played or not with player 2.
    const u32 p1 = mem_read32(cpu, cpu->gpr[13] - 21492u);
    const u32 p1_list = coop_bar_list(cpu, p1);
    for (int kind = 0; kind < 3 && p1_list != 0u; kind++)
        if (mem_read32(cpu, p1_list + 4u + (u32)kind * 4u) == mem_read32(cpu, node + 16u) &&
            guest_ram_range_valid(cpu, mem_read32(cpu, node + 40u), 24u))
            coop_bar_length(cpu, mem_read32(cpu, mem_read32(cpu, node + 40u) + 20u), p1, kind);
    if (!g_settings.mod_coop || g_coop_p2 == 0u || !coop_listed(cpu, g_coop_p2))
        return;
    for (int kind = 0; kind < COOP_BARS; kind++) {
        if (coop_bar_source(cpu, kind) != node)
            continue;
        const u32 copy = coop_bar_copy(cpu, kind, node);
        if (copy == 0u)
            return;
        const u32 record = mem_read32(cpu, mem_read32(cpu, copy + 40u) + 20u);
        coop_bar_length(cpu, record, g_coop_p2, kind);
        u32 ignore = 0u;
        g_coop_bar_frame = true;
        hle_guest_call(cpu, COOP_BAR_FRAME, &copy, 1, 0.0, &ignore);
        g_coop_bar_frame = false;
        static unsigned frames[COOP_BARS];
        if (getenv("ED_COOP_BAR_LOG") != NULL && frames[kind]++ % 60u == 0u) {
            const u32 mine = mem_read32(cpu, mem_read32(cpu, node + 40u) + 20u);
            fprintf(stderr,
                    "[coop] bar %d: player 1's %.2f shown %u (most %d, length %.2f), player 2's %.2f shown %u "
                    "(most %d, length %.2f, state %u)\n",
                    kind, guest_read_f32(cpu, mine + 8u), mem_read8(cpu, mine + 42u),
                    coop_most(cpu, mem_read32(cpu, cpu->gpr[13] - 21492u), kind), guest_read_f32(cpu, mine + 16u),
                    guest_read_f32(cpu, record + 8u), mem_read8(cpu, record + 42u), coop_most(cpu, g_coop_p2, kind),
                    guest_read_f32(cpu, record + 16u), mem_read32(cpu, copy + 20u));
        }
        return;
    }
}

// The game's look-up of a node by id: player 2's bars by theirs, and in
// their Tome their hand and copies (hle_find_by_id, which this entry's one
// intercept hands the rest to: installed after it, hle_find_by_id never
// was, and player 2's equipping took player 1's weapon).
static void hle_find_by_id(CPUState* cpu);
static void coop_find_node(CPUState* cpu) {
    const u32 id = cpu->gpr[3];
    if (id >= COOP_BAR_ID && id <= COOP_BAR_ID + COOP_AMMO && g_coop_p2 != 0u) {
        const int kind = (int)(id - COOP_BAR_ID);
        const u32 node = SCRATCH_HUD + (u32)kind * COOP_BAR_SLOT;
        if (g_coop_bar[kind].p2 == g_coop_p2 && mem_read32(cpu, node + 16u) == id) {
            cpu->gpr[3] = node;
            return;
        }
    }
    hle_find_by_id(cpu);
}

// Mods' Cheats, each on or off (hle_cheat_set, from the in-game menu), kept
// at the player controller's frame (coop_player_turn's notify), in play.
//
// All magick: the game's magick record (0x80334D50, the Tome's own; it reads
// it as it opens) holds the runes found (+0: 0x3FFF3FFF all fourteen), the
// circles of power (+4: 0x70000 the 3, 5 and 7 point), the twelve spells
// written in the Tome (+8, a word each: its runes and the circles it takes)
// and each spell's chosen circle (+0x38); the story's flags 599..602 are set
// once a player has magick (599 the magick bar, 601 the Tome's magick page).
// On, all of them is kept as a finished game has it (a spell new to the Tome
// takes its smallest circle); off, the record and flags go back to what they
// were when it came on.
//
// Full stats: each player's health, sanity and magick at their most whenever
// they are below it (0x80034E20 (node, kind, out) one, 0x80034F7C its most,
// 0x800354F8(node, kind, value, 0) sets one).
#define MAGICK_RECORD 0x80334D50u
#define MAGICK_RECORD_SIZE 0x68u
#define MAGICK_SPELLS 12u
#define MAGICK_FLAG_FIRST 599u
#define MAGICK_FLAGS 4u
#define STAT_GET 0x80034E20u
#define STAT_SET 0x800354F8u
static const u32 kAllSpells[MAGICK_SPELLS] = {0x0C070309u, 0x0C071049u, 0x0C070449u, 0x0C070419u,
                                              0x0C070489u, 0x0C010824u, 0x0C07101Cu, 0x0C020828u,
                                              0x0C07050Cu, 0x0C040828u, 0x0C07042Cu, 0x0C07081Cu};
static volatile bool g_cheat_on[HLE_CHEATS];  // asked for by the menu
static bool g_cheat_made[HLE_CHEATS];         // as made in the game
static u8 g_magick_kept[MAGICK_RECORD_SIZE];   // the record and flags before All magick
static bool g_magick_flags_kept[MAGICK_FLAGS];

void hle_cheat_set(int which, bool on) {
    if (which > 0 && which < HLE_CHEATS)
        g_cheat_on[which] = on;
}

bool hle_cheat_on(int which) { return which > 0 && which < HLE_CHEATS && g_cheat_on[which]; }

static void cheat_flag(CPUState* cpu, u32 flag, bool on) {
    const u32 set = mem_read32(cpu, cpu->gpr[13] - 26528u);
    const u32 at = set + 4u + (flag >> 5) * 4u, bit = 1u << (flag & 31u);
    if (guest_ram_range_valid(cpu, set, 4u) && flag < mem_read32(cpu, set) && guest_ram_range_valid(cpu, at, 4u))
        mem_write32(cpu, at, on ? (mem_read32(cpu, at) | bit) : (mem_read32(cpu, at) & ~bit));
}

static bool cheat_has_flag(CPUState* cpu, u32 flag) {
    const u32 set = mem_read32(cpu, cpu->gpr[13] - 26528u);
    const u32 at = set + 4u + (flag >> 5) * 4u;
    return guest_ram_range_valid(cpu, at, 4u) && (mem_read32(cpu, at) >> (flag & 31u) & 1u) != 0u;
}

static void cheat_all_magick(CPUState* cpu) {
    mem_write32(cpu, MAGICK_RECORD, 0x3FFF3FFFu);
    mem_write32(cpu, MAGICK_RECORD + 4u, 0x00070000u);
    for (u32 i = 0; i < MAGICK_SPELLS; i++) {
        const u32 circle = MAGICK_RECORD + 0x38u + i * 4u;
        mem_write32(cpu, MAGICK_RECORD + 8u + i * 4u, kAllSpells[i]);
        if ((mem_read32(cpu, circle) & 0x00070000u & kAllSpells[i] << 16) == 0u) {
            const u32 circles = kAllSpells[i] >> 16 & 7u;
            mem_write32(cpu, circle, (circles & -circles) << 16);
        }
    }
    for (u32 i = 0; i < MAGICK_FLAGS; i++)
        cheat_flag(cpu, MAGICK_FLAG_FIRST + i, true);
}

// The Tome first: the magick page is only the Tome's. What a character
// carries is a list (their node's +48: +4 the first entry, each +4 the next,
// +8 the thing's id), each thing a node whose stats' +24 is its kind (733
// the Tome). Without it, the chapter's Tome is given as the game gives what is
// found (0x80024528(text, script, to, thing, 0, 0, 0), as the script's
// acquire does: its words from 0x801DB374(r13 - 26492, the thing's two text
// numbers 0x80150B2C / 0x80150B5C of its info, 0, 1, 0x8023C1B4)): the
// message, then it is theirs. Once for each Tome node (a chapter with none
// says so).
#define TOME_KIND 733u
#define ACQUIRE 0x80024528u
#define ACQUIRE_TEXT 0x801DB374u
#define ACQUIRE_TEXT_ARG 0x8023C1B4u
#define ITEM_TEXT_A 0x80150B2Cu
#define ITEM_TEXT_B 0x80150B5Cu

static u32 cheat_kind(CPUState* cpu, u32 node) {
    const u32 stats = guest_ram_range_valid(cpu, node, 40u) ? mem_read32(cpu, node + 36u) : 0u;
    return guest_ram_range_valid(cpu, stats, 28u) ? mem_read32(cpu, stats + 24u) : 0u;
}

static bool cheat_carries(CPUState* cpu, u32 who, u32 kind) {
    const u32 list = guest_ram_range_valid(cpu, who, CARRY_LIST + 4u) ? mem_read32(cpu, who + CARRY_LIST) : 0u;
    u32 entry = guest_ram_range_valid(cpu, list, 8u) ? mem_read32(cpu, list + 4u) : 0u;
    for (int i = 0; i < 64 && guest_ram_range_valid(cpu, entry, 12u); i++, entry = mem_read32(cpu, entry + 4u))
        for (u32 n = 0u, k = 0u; k < 1024u && (n = coop_next(cpu, n)) != 0u; k++)
            if (mem_read32(cpu, n + 16u) == mem_read32(cpu, entry + 8u)) {
                if (cheat_kind(cpu, n) == kind)
                    return true;
                break;
            }
    return false;
}

static void cheat_tome(CPUState* cpu) {
    static u32 tried;  // the Tome node given (or looked for) last
    const u32 player = mem_read32(cpu, cpu->gpr[13] - 21492u);
    if (!guest_ram_range_valid(cpu, player, 56u) || cheat_carries(cpu, player, TOME_KIND))
        return;
    u32 tome = 0u;
    for (u32 n = 0u, k = 0u; k < 1024u && (n = coop_next(cpu, n)) != 0u; k++)
        if (cheat_kind(cpu, n) == TOME_KIND) {
            tome = n;
            break;
        }
    if (tome == tried)
        return;
    tried = tome;
    if (tome == 0u) {
        frontend_toast("No Tome in this chapter");
        fprintf(stderr, "[cheat] no Tome to give in this chapter\n");
        return;
    }
    u32 info = 0u, text_a = 0u, text_b = 0u, text = 0u, ignore = 0u;
    hle_guest_call(cpu, 0x801F6398u, &tome, 1, 0.0, &info);
    hle_guest_call(cpu, ITEM_TEXT_A, &info, 1, 0.0, &text_a);
    hle_guest_call(cpu, ITEM_TEXT_B, &info, 1, 0.0, &text_b);
    const u32 text_args[6] = {mem_read32(cpu, cpu->gpr[13] - 26492u), text_a, text_b, 0u, 1u, ACQUIRE_TEXT_ARG};
    hle_guest_call(cpu, ACQUIRE_TEXT, text_args, 6, 0.0, &text);
    const u32 args[7] = {text, 0u, mem_read32(cpu, player + 16u), mem_read32(cpu, tome + 16u), 0u, 0u, 0u};
    hle_guest_call(cpu, ACQUIRE, args, 7, 0.0, &ignore);
    fprintf(stderr, "[cheat] the Tome (node %u) given to player 1\n", mem_read32(cpu, tome + 16u));
}

// The Tome is the players' together. Casting asks whether the player has it
// (0x80109A68(), "<name> cannot cast spells without the Tome" when not):
// either player has it when either carries it (the same ask, with the other
// one the player: player 1 while player 2 is the player in their turn or
// casting from their Tome, coop_cast; else player 2), whoever picked it up.
#define HAS_TOME 0x80109A68u

static u32 coop_ask_has_tome(CPUState* cpu, u32 node) {
    const u32 who[3] = {cpu->gpr[13] - 26524u, cpu->gpr[13] - 21492u, cpu->gpr[13] - 21496u};
    const u32 now[3] = {mem_read32(cpu, node + 52u), node, mem_read32(cpu, node + 16u)};
    u32 was[3], answer = 0u;
    for (int i = 0; i < 3; i++) {
        was[i] = mem_read32(cpu, who[i]);
        mem_write32(cpu, who[i], now[i]);
    }
    hle_guest_call(cpu, HAS_TOME, NULL, 0, 0.0, &answer);
    for (int i = 0; i < 3; i++)
        mem_write32(cpu, who[i], was[i]);
    return answer;
}

static void coop_has_tome(CPUState* cpu) {
    static bool asking;
    g_hle_passthrough = true;
    if (asking || g_coop_p2 == 0u || !g_settings.mod_coop || !sda_ready(cpu))
        return;
    const u32 self = mem_read32(cpu, cpu->gpr[13] - 21492u);
    // In a use of theirs too: what they found asks for the Tome's page (0x80109900), and it wasn't asked.
    const u32 other = self == g_coop_p2 ? ((g_coop_turn || g_coop_tome_cast)  ? g_coop_turn_p1
                                           : g_coop_use.active && g_coop_use.p2 ? g_coop_use.was[1]
                                                                                : 0u)
                                        : g_coop_p2;
    if (!guest_ram_range_valid(cpu, self, 56u) || !guest_ram_range_valid(cpu, other, 56u) ||
        !coop_listed(cpu, g_coop_p2))
        return;
    asking = true;
    // Their own things: with player 1's lent for their cast, the Tome they
    // carry was in neither list, and they could not cast.
    const u32 lent = g_coop_own_carry != 0u ? mem_read32(cpu, g_coop_p2 + CARRY_LIST) : 0u;
    if (lent != 0u)
        mem_write32(cpu, g_coop_p2 + CARRY_LIST, g_coop_own_carry);
    u32 answer = coop_ask_has_tome(cpu, self);
    if (lent != 0u)
        mem_write32(cpu, g_coop_p2 + CARRY_LIST, lent);
    if (answer == 0u)
        answer = coop_ask_has_tome(cpu, other);
    asking = false;
    cpu->gpr[3] = answer;
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

// What the players carry is theirs together for a script too. A script asks
// whether the player carries a thing (0x80165C94(5, id) -> 0x801F60BC(id, their
// list)) of whoever is the player as it runs: at a way out player 2's look asked
// their list, the Tome being in player 1's, and offered the way to fetch it;
// the press, run as player 1, found it and left with the input held, in the
// next room for good. Either list answers, whoever picked it up.
#define COOP_ITEM_IN_LIST 0x801F60BCu
#define COOP_SCRIPT_ITEM_ASKED 0x80165DB8u

static void coop_script_item(CPUState* cpu) {
    static bool asking;
    g_hle_passthrough = true;
    if (asking || cpu->lr != COOP_SCRIPT_ITEM_ASKED || g_coop_p2 == 0u || !g_settings.mod_coop || !sda_ready(cpu) ||
        !coop_listed(cpu, g_coop_p2))
        return;
    const u32 self = mem_read32(cpu, cpu->gpr[13] - 21492u);
    const u32 first = g_coop_use.active && g_coop_use.p2 ? g_coop_use.was[1] : g_coop_turn || g_coop_tome_cast ? g_coop_turn_p1 : 0u;
    const u32 other = self == g_coop_p2 ? first : g_coop_p2;
    if (!guest_ram_range_valid(cpu, other, CARRY_LIST + 4u))
        return;
    u32 args[2] = {cpu->gpr[3], cpu->gpr[4]};
    u32 answer = 0u;
    asking = true;
    hle_guest_call(cpu, COOP_ITEM_IN_LIST, args, 2, 0.0, &answer);
    if ((s32)answer < 0) {
        args[1] = mem_read32(cpu, other + CARRY_LIST);
        hle_guest_call(cpu, COOP_ITEM_IN_LIST, args, 2, 0.0, &answer);
        if ((s32)answer >= 0 && getenv("ED_COOP_LOG") != NULL)
            fprintf(stderr, "[coop] a script asks for item %d: the other player carries it (%d)\n", (s32)args[0], (s32)answer);
    }
    asking = false;
    cpu->gpr[3] = answer;
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

static void cheat_full(CPUState* cpu, u32 node) {
    if (!guest_ram_range_valid(cpu, node, 56u))
        return;
    for (u32 kind = 0; kind < 3u; kind++) {
        const u32 out = (cpu->gpr[1] - 0x300u) & ~15u;
        const u32 args[3] = {node, kind, out};
        u32 ignore = 0u;
        mem_write32(cpu, out, 0u);
        hle_guest_call(cpu, STAT_GET, args, 3, 0.0, &ignore);
        const s16 has = (s16)mem_read16(cpu, out);
        hle_guest_call(cpu, STAT_MOST, args, 3, 0.0, &ignore);
        const s16 most = (s16)mem_read16(cpu, out);
        const u32 set_args[4] = {node, kind, (u32)(s32)most, 0u};
        if (most > 0 && has < most)
            hle_guest_call(cpu, STAT_SET, set_args, 4, 0.0, &ignore);
    }
}

static void cheat_apply(CPUState* cpu) {
    static int asked = -1;  // ED_CHEAT=n (testing): HleCheat n on from the start
    if (asked < 0) {
        asked = getenv("ED_CHEAT") != NULL ? atoi(getenv("ED_CHEAT")) : 0;
        hle_cheat_set(asked, true);
    }
    if (!sda_ready(cpu))
        return;
    const bool magick = g_cheat_on[HLE_CHEAT_MAGICK];
    if (magick && !g_cheat_made[HLE_CHEAT_MAGICK]) {
        memcpy(g_magick_kept, cpu->ram + (MAGICK_RECORD - GC_RAM_BASE), MAGICK_RECORD_SIZE);
        for (u32 i = 0; i < MAGICK_FLAGS; i++)
            g_magick_flags_kept[i] = cheat_has_flag(cpu, MAGICK_FLAG_FIRST + i);
        frontend_toast("All magick on");
        fprintf(stderr, "[cheat] all magick on\n");
    } else if (!magick && g_cheat_made[HLE_CHEAT_MAGICK]) {
        memcpy(cpu->ram + (MAGICK_RECORD - GC_RAM_BASE), g_magick_kept, MAGICK_RECORD_SIZE);
        for (u32 i = 0; i < MAGICK_FLAGS; i++)
            cheat_flag(cpu, MAGICK_FLAG_FIRST + i, g_magick_flags_kept[i]);
        frontend_toast("All magick off");
        fprintf(stderr, "[cheat] all magick off\n");
    }
    g_cheat_made[HLE_CHEAT_MAGICK] = magick;
    if (magick) {
        cheat_tome(cpu);
        cheat_all_magick(cpu);
    }
    const bool full = g_cheat_on[HLE_CHEAT_FULL];
    if (full != g_cheat_made[HLE_CHEAT_FULL]) {
        frontend_toast(full ? "Full stats on" : "Full stats off");
        fprintf(stderr, "[cheat] full stats %s\n", full ? "on" : "off");
    }
    g_cheat_made[HLE_CHEAT_FULL] = full;
    if (full) {
        cheat_full(cpu, mem_read32(cpu, cpu->gpr[13] - 21492u));
        if (g_coop_p2 != 0u && coop_listed(cpu, g_coop_p2))
            cheat_full(cpu, g_coop_p2);
    }
}

// Enemies go after either player. Who a character is after is an id in its
// stats (node +36): +20 (+12 beside it, with the same); the game only ever
// puts player 1's there. In co-op, a character after one player takes the
// nearer of the two (by 60 units, so it does not turn back and forth between
// them), when both are in its room. ED_COOP_ENEMY_LOG=1 tells each change.
#define COOP_TARGET_MARGIN 60.0f

static float coop_distance(CPUState* cpu, u32 a, u32 b) {
    float d2 = 0.0f;
    for (u32 i = 0; i < 3u; i++) {
        const float d = guest_read_f32(cpu, a + i * 4u) - guest_read_f32(cpu, b + i * 4u);
        d2 += d * d;
    }
    return sqrtf(d2);
}

static void coop_enemy_target(CPUState* cpu, u32 object) {
    if (!g_settings.mod_coop || g_coop_turn || g_coop_step_running || g_coop_p2 == 0u || !g_coop_p2_state ||
        !sda_ready(cpu))
        return;
    const u32 p1_node = mem_read32(cpu, cpu->gpr[13] - 21492u);
    const u32 p1 = mem_read32(cpu, cpu->gpr[13] - 26524u), p2 = mem_read32(cpu, g_coop_p2 + 52u);
    if (object == p1 || object == p2 || !guest_ram_range_valid(cpu, p1_node, 20u) ||
        !guest_ram_range_valid(cpu, object, COOP_OBJECT_ROOM + 4u) ||
        !guest_ram_range_valid(cpu, p1, COOP_OBJECT_ROOM + 4u) || !guest_ram_range_valid(cpu, p2, COOP_OBJECT_ROOM + 4u))
        return;
    const u32 room = mem_read32(cpu, object + COOP_OBJECT_ROOM);
    if (mem_read32(cpu, p1 + COOP_OBJECT_ROOM) != room || mem_read32(cpu, p2 + COOP_OBJECT_ROOM) != room)
        return;
    u32 node = 0u;
    for (int i = 0; i < 1024 && (node = coop_next(cpu, node)) != 0u; i++)
        if (mem_read32(cpu, node + 52u) == object)
            break;
    const u32 stats = node != 0u ? mem_read32(cpu, node + (u32)(s32)g_coop_stats_offset) : 0u;
    if (!guest_ram_range_valid(cpu, stats, COOP_TARGET_B + 4u))
        return;
    static int log = -1;
    if (log < 0)
        log = getenv("ED_COOP_ENEMY_LOG") != NULL;
    if (log) {  // and each change of its health
        static struct {
            u32 node;
            s16 health;
        } seen[32];
        const s16 health = (s16)mem_read16(cpu, stats + (u32)(s32)g_coop_health_offset);
        for (int i = 0; i < 32; i++)
            if (seen[i].node == node || seen[i].node == 0u) {
                if (seen[i].node == node && seen[i].health != health)
                    fprintf(stderr, "[coop] %u's health %d -> %d\n", mem_read32(cpu, node + 16u), seen[i].health, health);
                seen[i].node = node;
                seen[i].health = health;
                break;
            }
    }
    const u32 ids[2] = {mem_read32(cpu, p1_node + 16u), mem_read32(cpu, g_coop_p2 + 16u)};
    const u32 after = mem_read32(cpu, stats + COOP_TARGET_B);
    const u32 player_nodes[2] = {p1_node, g_coop_p2}, player_objects[2] = {p1, p2};
    coop_seen_later(cpu, node, object, player_nodes, player_objects, stats);
    if (after != ids[0] && after != ids[1])
        return;
    const float d1 = coop_distance(cpu, object, p1), d2 = coop_distance(cpu, object, p2);
    const u32 want = after == ids[0] ? (d2 + COOP_TARGET_MARGIN < d1 ? ids[1] : ids[0])
                                     : (d1 + COOP_TARGET_MARGIN < d2 ? ids[0] : ids[1]);
    if (want == after)
        return;
    mem_write32(cpu, stats + COOP_TARGET_B, want);
    if (mem_read32(cpu, stats + COOP_TARGET_A) == after)
        mem_write32(cpu, stats + COOP_TARGET_A, want);
    if (log)
        fprintf(stderr, "[coop] %u goes after player %d (%.0f from player 1, %.0f from player 2)\n",
                mem_read32(cpu, node + 16u), want == ids[0] ? 1 : 2, d1, d2);
}

static void coop_character_step(CPUState* cpu) {
    const u32 object = cpu->gpr[3];
    static bool seen;
    if (!seen && g_coop_p2 != 0u && object == mem_read32(cpu, g_coop_p2 + 52u) && getenv("ED_COOP_LOG") != NULL) {
        seen = true;
        fprintf(stderr, "[coop] player 2's step runs (their turn %s yet)\n", g_coop_p2_state ? "has run" : "has not run");
    }
    coop_enemy_target(cpu, object);
    // Off unless ED_COOP_P2_STEP=1 (testing): with it, player 2 finds doors
    // (the record above is theirs) but neither player can open one yet:
    // something of the door's own, beyond these globals, is upset by the
    // second scan.
    static int off = -1;
    if (off < 0)
        off = getenv("ED_COOP_P2_STEP") == NULL;
    if (off || g_coop_step_running || g_coop_p2 == 0u || !g_coop_p2_state || !sda_ready(cpu) ||
        object != mem_read32(cpu, g_coop_p2 + 52u)) {
        g_hle_passthrough = true;
        return;
    }
    u32 args[8];
    for (int i = 0; i < 8; i++)
        args[i] = cpu->gpr[3 + i];
    // Player 2's whole state (their input too: using what they stand by is
    // their button, read here), as in their turn.
    const u32 who[3] = {cpu->gpr[13] - 26524u, cpu->gpr[13] - 21492u, cpu->gpr[13] - 21496u};
    const u32 now[3] = {object, g_coop_p2, mem_read32(cpu, g_coop_p2 + 16u)};
    u32 was[3];
    static CoopPlayerState p1;
    for (int i = 0; i < 3; i++) {
        was[i] = mem_read32(cpu, who[i]);
        mem_write32(cpu, who[i], now[i]);
    }
    // The scan's prompt and its state lie round the record (r13 - 24040 ..
    // - 21980): player 1's, as they are, come back after player 2's scan,
    // which finding nothing would clear them (player 1 could not open a door).
    static u8 p1_prompt[2060];
    const u32 prompt = cpu->gpr[13] - 24040u;
    coop_bytes(cpu, prompt, p1_prompt, sizeof p1_prompt, true);
    coop_state(cpu, &p1, true);
    coop_state(cpu, &g_coop_state[1], false);
    g_coop_step_running = true;
    const bool ok = hle_guest_call(cpu, COOP_CHARACTER_STEP, args, 8, cpu->fpr[1], NULL);
    g_coop_step_running = false;
    coop_state(cpu, &g_coop_state[1], true);
    coop_words_back(cpu, p1_prompt);
    coop_state(cpu, &p1, false);
    for (int i = 0; i < 3; i++)
        mem_write32(cpu, who[i], was[i]);
    static u32 told;
    if (getenv("ED_COOP_LOG") != NULL && g_coop_state[1].interact[3] != told) {
        told = g_coop_state[1].interact[3];
        fprintf(stderr, "[coop] player 2 is next to 0x%08X\n", told);
    }
    if (!ok)
        fprintf(stderr, "[coop] player 2's step did not return\n");
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

// Player 2's feet, for their camera (camera.c); false while they have none.
bool hle_coop_player2_position(CPUState* cpu, float out[3]) {
    if (g_coop_p2 == 0u)
        return false;
    const u32 object = mem_read32(cpu, g_coop_p2 + 52u);
    if (!guest_ram_range_valid(cpu, object, 16u))
        return false;
    for (u32 i = 0; i < 3u; i++)
        out[i] = guest_read_f32(cpu, object + i * 4u);
    return isfinite(out[0]) && isfinite(out[1]) && isfinite(out[2]);
}

// Each pad read: in co-op and in play, player 2 is there (made again in each
// new area: the game's area change takes every character with it).
// ED_COOP_DUMP=path (testing): player 1's node, actor and object (0x600
// bytes each, as words) written to path once, 600 pad reads into play, to
// compare a run with player 2 and one without.
static void coop_dump(CPUState* cpu, bool playing) {
    static int reads = -1;
    const char* path = getenv("ED_COOP_DUMP");
    if (path == NULL || !playing || reads >= 600)
        return;
    const u32 player = coop_player(cpu);
    if (player == 0u || ++reads != 600)
        return;
    FILE* f = fopen(path, "w");
    if (f == NULL)
        return;
    const u32 object = mem_read32(cpu, player + 52u);
    const u32 bases[4] = {player, mem_read32(cpu, player + (u32)(s32)g_coop_actor_offset), object,
                          guest_ram_range_valid(cpu, object, 64u) ? mem_read32(cpu, object + 60u) : 0u};
    static const char* const kWhat[4] = {"node", "actor", "object", "model"};
    for (int b = 0; b < 4; b++)
        for (u32 off = 0; off < 0x600u && guest_ram_range_valid(cpu, bases[b] + off, 4u); off += 4u)
            fprintf(f, "%s+%03X %08X\n", kWhat[b], off, mem_read32(cpu, bases[b] + off));
    fclose(f);
    fprintf(stderr, "[coop] dumped player 1 to %s\n", path);
}

// ED_COOP_SNAP=dir (testing): player 2's object (0x800 bytes) once a second
// in play, to dir/snapN.txt, with their distance from the game camera's eye.
static void coop_snap(CPUState* cpu) {
    const char* dir = getenv("ED_COOP_SNAP");
    static unsigned reads, n;
    if (dir == NULL || g_coop_p2 == 0u || ++reads % 60u != 0u || n >= 40u)
        return;
    const u32 object = mem_read32(cpu, g_coop_p2 + 52u);
    if (!guest_ram_range_valid(cpu, object, 0x800u))
        return;
    char path[512];
    snprintf(path, sizeof path, "%s/snap%02u.txt", dir, n++);
    FILE* f = fopen(path, "w");
    if (f == NULL)
        return;
    float d2 = 0.0f;
    for (u32 i = 0; i < 3u; i++) {
        const float d = guest_read_f32(cpu, object + i * 4u) - guest_read_f32(cpu, COOP_CAMERA + i * 4u);
        d2 += d * d;
    }
    fprintf(f, "distance %.0f\n", sqrtf(d2));
    for (u32 off = 0; off < 0x800u; off += 4u)
        fprintf(f, "%03X %08X\n", off, mem_read32(cpu, object + off));
    // ... and its model instance (object +60), 0x800 bytes.
    const u32 model = mem_read32(cpu, object + 60u);
    for (u32 off = 0; guest_ram_range_valid(cpu, model, 0x800u) && off < 0x800u; off += 4u)
        fprintf(f, "m%03X %08X\n", off, mem_read32(cpu, model + off));
    fclose(f);
}

// Testing, counted in pad reads in play since player 2 was made:
// ED_COOP_SWAP_AT=n: the players trade places (position and facing) at n.
// ED_COOP_SDA_DUMP=dir: the game's small-data area (r13 +-32 KB) written to
// dir/sdaK.bin at n-100, n-40, n+60 and n+120 (K 0..3), to compare.
// The players trade places: position (object +0..11) and facing (+44..59).
static bool coop_trade_places(CPUState* cpu) {
    const u32 player = coop_player(cpu);
    const u32 o1 = player != 0u ? mem_read32(cpu, player + 52u) : 0u;
    const u32 o2 = g_coop_p2 != 0u ? mem_read32(cpu, g_coop_p2 + 52u) : 0u;
    if (!guest_ram_range_valid(cpu, o1, 64u) || !guest_ram_range_valid(cpu, o2, 64u))
        return false;
    for (u32 off = 0; off < 60u; off += 4u) {
        if (off >= 12u && off < 44u)
            continue;
        const u32 a = mem_read32(cpu, o1 + off), b = mem_read32(cpu, o2 + off);
        mem_write32(cpu, o1 + off, b);
        mem_write32(cpu, o2 + off, a);
    }
    return true;
}

// Player 2 uses doors through player 1's own. Only the player looks round
// for doors and things, and a second look (for player 2) kept player 1 from
// opening any; so when player 2 presses B (a press, not a hold), the players
// trade places and player 1 presses B there for a few frames: the game's own
// look finds what player 2 stood by and its own use opens it, player 1 going
// through (player 2 follows them, coop_follow). Nothing to use there (player
// 1 still where they were put), they trade back.
// In pad reads after the trade: the game's look settles on what is there
// (its prompt comes up), then B, then time for the door to take them.
#define COOP_PROXY_PRESS_FROM 20u
#define COOP_PROXY_PRESS_UNTIL 26u
#define COOP_PROXY_DONE 60u

// Pad reads in play since the assist pressed B for player 2 (0: none lately),
// and player 2's B still held from a screen they had (not a new press).
static bool g_coop_b_release;

static void coop_door_proxy(CPUState* cpu) {
    if (g_coop_use_own)
        return;  // player 2 uses things as themselves (coop_use_start)
    static unsigned step;
    static bool b_was;
    static float at[3];
    DolPadState p2;
    controls_read_player2(&p2);
    const bool b = (p2.button & 0x0200u) != 0u;
    if (!b)
        g_coop_b_release = false;
    const bool pressed = b && !b_was && !g_coop_b_release;
    b_was = b;
    const u32 player = coop_player(cpu);
    const u32 o1 = player != 0u ? mem_read32(cpu, player + 52u) : 0u;
    if (step == 0u) {
        if (!pressed || !guest_ram_range_valid(cpu, o1, 64u) || !coop_trade_places(cpu))
            return;
        for (u32 i = 0; i < 3u; i++)
            at[i] = guest_read_f32(cpu, o1 + i * 4u);
        step = 1u;
        fprintf(stderr, "[coop] player 2 uses what they stand by (through player 1)\n");
        return;
    }
    step++;
    g_coop_press_b = step >= COOP_PROXY_PRESS_FROM && step < COOP_PROXY_PRESS_UNTIL;
    if (step == COOP_PROXY_PRESS_FROM)
        g_coop_assist_at = 1u;
    if (step == COOP_PROXY_PRESS_FROM && getenv("ED_COOP_LOG") != NULL)
        fprintf(stderr, "[coop] assist presses B: player 1 at %.0f %.0f, next to 0x%08X (who 0x%08X)\n",
                guest_read_f32(cpu, o1), guest_read_f32(cpu, o1 + 4u), mem_read32(cpu, cpu->gpr[13] - 23192u),
                mem_read32(cpu, cpu->gpr[13] - 23196u));
    if (step < COOP_PROXY_DONE)
        return;
    g_coop_press_b = false;
    if (g_coop_screen) {
        step = COOP_PROXY_DONE - 1u;  // a close-up of what was there: once it is over
        return;
    }
    step = 0u;
    if (!guest_ram_range_valid(cpu, o1, 64u))
        return;
    float moved = 0.0f;
    for (u32 i = 0; i < 3u; i++) {
        const float d = guest_read_f32(cpu, o1 + i * 4u) - at[i];
        moved += d * d;
    }
    if (moved < 4.0f) {
        coop_trade_places(cpu);
        // Put back where they were, player 1 never walked out of what was
        // there, so its prompt stayed theirs: it goes (0x80117360(1)).
        const u32 clear = 1u;
        u32 ignore = 0u;
        hle_guest_call(cpu, PROMPT_CLEAR, &clear, 1, 0.0, &ignore);
        fprintf(stderr, "[coop] nothing to use there: the players trade back\n");
    } else {
        fprintf(stderr, "[coop] player 1 went on (%.0f units)\n", sqrtf(moved));
    }
}

// The screens that stop play (the Tome; what an action press opens: a thing
// looked at, a thing taken) are run by the game's one pad. Player 2 has them
// when they opened them (their Start, or their B through the assist): their
// pad is the game's until play goes on, and the screen shows in their half,
// the other half keeping player 1's view as it was (aurora_split_menu).
// Player 1's Tome shows in player 1's half the same way.
#define COOP_ASSIST_WINDOW 120u  // pad reads after the assist's B that a screen is player 2's
#define COOP_START_WAIT 30u      // pad reads for the Tome to open after player 2's Start
void aurora_split_menu(int player, int layout);
void aurora_split_only_view(int player);

// ED_COOP_SNAP_DIR=dir (testing): memory as player 2's Tome opens and as
// play goes on again (snapN.ram, the first 24 MB).
static void coop_snapshot(CPUState* cpu) {
    static int n;
    const char* dir = getenv("ED_COOP_SNAP_DIR");
    if (dir == NULL || n >= 8)
        return;
    char path[512];
    snprintf(path, sizeof path, "%s/snap%d.ram", dir, n++);
    FILE* f = fopen(path, "wb");
    if (f != NULL) {
        fwrite(cpu->ram, 1, 24u * 1024u * 1024u, f);
        fclose(f);
        fprintf(stderr, "[coop] memory written to %s\n", path);
    }
}

// What each character holds is kept by their id (node +16) in a table of
// 20 (0x80588DF0, 408 bytes each: +0 the id, +4 what is held, 12 bytes each,
// +388 how many, +392/+396 the hands, +400/+404 what holding and letting go
// call: 0x800C994C puts the weapon in the hand of the one whose id it is).
// 0x80151500(id, make) finds one; the player's is made with play, and player
// 2 had none: what the Tome equipped for them went nowhere. Theirs is made
// as player 1's is.
#define EQUIP_FIND 0x80151500u
#define EQUIP_SET_UP 0x80151634u

static void coop_equip_record(CPUState* cpu) {
    const u32 player = coop_player(cpu);
    if (player == 0u || g_coop_p2 == 0u)
        return;
    u32 p1 = 0u, own = 0u, ignore = 0u;
    const u32 find1[2] = {mem_read32(cpu, player + 16u), 0u};
    const u32 find2[2] = {mem_read32(cpu, g_coop_p2 + 16u), 0u};
    if (!hle_guest_call(cpu, EQUIP_FIND, find1, 2, 0.0, &p1) || p1 == 0u ||
        !hle_guest_call(cpu, EQUIP_FIND, find2, 2, 0.0, &own) || own != 0u)
        return;
    const u32 make[2] = {find2[0], 1u};
    if (!hle_guest_call(cpu, EQUIP_FIND, make, 2, 0.0, &own) || own == 0u)
        return;
    const u32 set_up[4] = {own, mem_read32(cpu, p1 + 392u), mem_read32(cpu, p1 + 400u), mem_read32(cpu, p1 + 404u)};
    hle_guest_call(cpu, EQUIP_SET_UP, set_up, 4, 0.0, &ignore);
    fprintf(stderr, "[coop] player 2's own equipment (id %u) at 0x%08X\n", find2[0], own);
}

// While player 2 has the Tome it works for them: the player's object and id
// (r13 - 26524, r13 - 21496) are theirs, so what they equip goes in their
// own equipment (coop_equip_record) and their hand. The player's node (r13 -
// 21492) stays player 1's: the Tome lists what that node carries, and both
// players have the same things. The game finds the one an id names by
// 0x801F5F88(id), which answers the player's node for the player's id: in
// the equipping and letting go (0x800C994C, 0x800C9DD4), whose hand the
// weapon is in, player 2's id is answered with their node meanwhile
// (hle_find_by_id). (Everywhere, the Tome found nothing to equip: it asks
// which things the player's id carries the same way.) Player 1 is the
// player again as play goes on.
#define FIND_BY_ID 0x801F5F88u
static bool g_coop_tome_p2;

// Each player has a copy of a weapon: there is one of each, and player 1
// equipping what player 2 held took it out of their hand. Player 2's is
// made (as the story makes one) the first time they equip it, and their
// equipping and letting go of
// that weapon (the item's id: 0x800C994C, 0x800C9DD4 ask for its node) are
// answered with their copy's node.
static struct {
    u32 id, node;
} g_coop_copies[32];
static u32 g_coop_copies_for;  // the player 2 they are for

static void coop_copies_reset(void) {
    if (g_coop_copies_for != g_coop_p2) {  // another player 2 (another area): new copies
        memset(g_coop_copies, 0, sizeof g_coop_copies);
        g_coop_copies_for = g_coop_p2;
    }
}

static u32 coop_weapon_copy(CPUState* cpu, u32 id, bool make) {
    coop_copies_reset();
    // A copy asked for by its own id (equipment saved while it was so
    // listed) is itself, not the original to copy again.
    for (int i = 0; i < 32; i++)
        if (g_coop_copies[i].node != 0u && mem_read32(cpu, g_coop_copies[i].node + 16u) == id &&
            coop_listed(cpu, g_coop_copies[i].node))
            return g_coop_copies[i].node;
    int free_at = -1;
    for (int i = 0; i < 32; i++) {
        if (g_coop_copies[i].node != 0u && g_coop_copies[i].id == id) {
            if (coop_listed(cpu, g_coop_copies[i].node))
                return g_coop_copies[i].node;
            g_coop_copies[i].node = 0u;
        }
        if (g_coop_copies[i].node == 0u && free_at < 0)
            free_at = i;
    }
    if (!make || free_at < 0)
        return 0u;
    u32 original = 0u;
    for (u32 node = 0u, i = 0u; i < 1024u && (node = coop_next(cpu, node)) != 0u; i++)
        if (mem_read32(cpu, node + 16u) == id)
            original = node;
    const u32 actor = original != 0u ? mem_read32(cpu, original + (u32)(s32)g_coop_actor_offset) : 0u;
    const u32 object = original != 0u ? mem_read32(cpu, original + 52u) : 0u;
    if (!guest_ram_range_valid(cpu, actor, 160u) || !guest_ram_range_valid(cpu, object, 64u))
        return 0u;
    const u32 rec = (cpu->gpr[1] - 0x400u) & ~15u;
    u32 made = 0u, ignore = 0u;
    if (!hle_guest_call(cpu, COOP_SPAWN_INIT, &rec, 1, 0.0, &ignore))
        return 0u;
    // At player 2, in their room (an object's +592): a copy made in player
    // 1's room, with player 2 in another, stayed there.
    const u32 holder = mem_read32(cpu, g_coop_p2 + 52u);
    for (u32 i = 0; i < 3u; i++)
        guest_write_f32(cpu, rec + i * 4u, guest_read_f32(cpu, holder + i * 4u));
    mem_write32(cpu, rec + 16u, mem_read16(cpu, actor + 152u));
    mem_write32(cpu, rec + 20u, (u32)(s32)(s16)mem_read16(cpu, actor + 154u));
    mem_write32(cpu, rec + 40u, mem_read32(cpu, original + 8u));
    g_coop_spawning = true;
    const bool created = hle_guest_call(cpu, COOP_SPAWN_CREATE, &rec, 1, 0.0, &made);
    g_coop_spawning = false;
    if (!created || made == 0u) {
        fprintf(stderr, "[coop] no copy of %u made for player 2\n", id);
        return 0u;
    }
    const u32 room_args[] = {made, mem_read32(cpu, holder + 592u)};
    const u32 add_args[] = {1u, made};
    // As the story's making of a thing does (the Scramasax from the casket,
    // 0x8000E6CC on): its size, room, register, 0x800CA8D8(node), add, and its
    // model readied (0x8011F460(object)); without the last two it was held
    // unseen.
    const u32 made_object = mem_read32(cpu, made + 52u);
    u32 sized = 0u;
    if (hle_guest_call(cpu, 0x80118AB0u, &made_object, 1, 0.0, &sized) && sized != 0u)
        hle_guest_call(cpu, 0x80124BE4u, &made_object, 1, guest_read_f32(cpu, rec + 12u), &ignore);
    hle_guest_call(cpu, COOP_SPAWN_SET_ROOM, room_args, 2, 0.0, &ignore);
    hle_guest_call(cpu, COOP_SPAWN_REGISTER, &made, 1, 0.0, &ignore);
    hle_guest_call(cpu, 0x800CA8D8u, &made, 1, 0.0, &ignore);
    hle_guest_call(cpu, 0x8011F460u, &made_object, 1, 0.0, &ignore);
    hle_guest_call(cpu, COOP_SPAWN_ADD, add_args, 2, 0.0, &ignore);
    // Its moves (part 0), as the area's making of things sets them
    // (0x80118D98(object, part, 1)).
    const u32 moves_args[3] = {made_object, 0u, 1u};
    hle_guest_call(cpu, 0x80118D98u, moves_args, 3, 0.0, &ignore);
    g_coop_copies[free_at].id = id;
    g_coop_copies[free_at].node = made;
    fprintf(stderr, "[coop] player 2's copy of %u: node 0x%08X (id %u)\n", id, made, mem_read32(cpu, made + 16u));
    return made;
}

// What a character holds is drawn at the holder's place and facing (object
// +0..11, +44..59), its hand's bone from there (0x8011D394). The game puts
// them there each step for player 1's alone: player 2's copy stayed where it
// was made, floating. After player 2's turn, theirs go with them, into their
// room.
// A thing's record (node +44) names what kind of thing it is at +4 (the
// sword's: its moves for each stance, part = that +4's +54 + 2 * stance); a
// copy's was left empty, and its holder's guard, asked of it, was none: player
// 2 stood at ease among enemies. The copy is given its original's.
#define COOP_THING_RECORD 44u
#define COOP_THING_KIND 4u

static void coop_copy_kind(CPUState* cpu, int i) {
    const u32 record = mem_read32(cpu, g_coop_copies[i].node + COOP_THING_RECORD);
    if (!guest_ram_range_valid(cpu, record, COOP_THING_KIND + 4u) || mem_read32(cpu, record + COOP_THING_KIND) != 0u)
        return;
    for (u32 node = 0u, n = 0u; n < 1024u && (node = coop_next(cpu, node)) != 0u; n++) {
        if (node == g_coop_copies[i].node || mem_read32(cpu, node + 16u) != g_coop_copies[i].id)
            continue;
        const u32 theirs = mem_read32(cpu, node + COOP_THING_RECORD);
        if (guest_ram_range_valid(cpu, theirs, COOP_THING_KIND + 4u) && mem_read32(cpu, theirs + COOP_THING_KIND) != 0u) {
            mem_write32(cpu, record + COOP_THING_KIND, mem_read32(cpu, theirs + COOP_THING_KIND));
            fprintf(stderr, "[coop] player 2's copy of %u: its kind from the original (0x%08X)\n", g_coop_copies[i].id,
                    mem_read32(cpu, theirs + COOP_THING_KIND));
        }
        return;
    }
}

static void coop_copies_follow(CPUState* cpu) {
    const u32 p2 = mem_read32(cpu, g_coop_p2 + 52u);
    if (!guest_ram_range_valid(cpu, p2, COOP_OBJECT_ROOM + 4u))
        return;
    for (int i = 0; i < 32; i++) {
        const u32 node = g_coop_copies[i].node;
        const u32 held = node != 0u ? mem_read32(cpu, node + 52u) : 0u;
        if (!guest_ram_range_valid(cpu, held, COOP_OBJECT_ROOM + 4u) || mem_read32(cpu, held + 372u) != p2)
            continue;
        coop_copy_kind(cpu, i);
        coop_copy_own_kind(cpu, i, node);
        for (u32 at = 0u; at < 60u; at += 4u)
            if (at < 12u || at >= 44u)
                mem_write32(cpu, held + at, mem_read32(cpu, p2 + at));
        const u32 room = mem_read32(cpu, p2 + COOP_OBJECT_ROOM);
        if (mem_read32(cpu, held + COOP_OBJECT_ROOM) != room) {
            const u32 args[] = {node, room};
            u32 ignore = 0u;
            hle_guest_call(cpu, COOP_SPAWN_SET_ROOM, args, 2, 0.0, &ignore);
        }
    }
}

// A save made in co-op keeps player 2's copy, but not that they held it: the
// game keeps what each character holds by their id (coop_equip_record), and
// player 2 is made anew, with another id, on every load. The copy comes back
// as a thing lying where player 2 was saved, under them. coop_adopt finds it
// there (a thing of the same model as another, the original); on player 2's
// first turn it is theirs again and goes into their hand as the Tome puts it
// there (0x80150D9C(equipment, id, ...): listed, then held), the original's id
// answered with the copy (hle_find_by_id).
#define EQUIP_ADD 0x80150D9Cu
#define EQUIP_MOVES 0x8007A788u
static void coop_tome_player(CPUState* cpu, bool p2);

static u32 coop_equip_names_find(CPUState* cpu, u32 id);
static void coop_restore_held(CPUState* cpu) {
    const u32 id = g_coop_restore.id, node = g_coop_restore.node;
    if (node == 0u)
        return;
    g_coop_restore.id = g_coop_restore.node = 0u;
    if (!coop_listed(cpu, node))
        return;
    coop_copies_reset();
    int free_at = -1;
    for (int i = 0; i < 32 && free_at < 0; i++)
        if (g_coop_copies[i].node == 0u || g_coop_copies[i].id == id)
            free_at = i;
    if (free_at < 0)
        return;
    g_coop_copies[free_at].id = id;
    g_coop_copies[free_at].node = node;
    // How player 1's equipment lists the same thing (the hands it takes),
    // else as a sword.
    u32 hands[2] = {2u, 0u};
    const u32 entry = coop_equip_names_find(cpu, id);
    if (entry != 0u)
        hands[0] = mem_read32(cpu, entry + 4u), hands[1] = mem_read32(cpu, entry + 8u);
    // As in their Tome: player 2 the player while it is put in their hand and
    // their moves made for it (0x8007A788(node), what the Tome does after).
    coop_tome_player(cpu, true);
    u32 own = 0u, ignore = 0u;
    const u32 find[2] = {mem_read32(cpu, g_coop_p2 + 16u), 0u};
    if (hle_guest_call(cpu, EQUIP_FIND, find, 2, 0.0, &own) && own != 0u) {
        const u32 add[4] = {own, id, hands[0], hands[1]};
        hle_guest_call(cpu, EQUIP_ADD, add, 4, 0.0, &ignore);
        hle_guest_call(cpu, EQUIP_MOVES, &g_coop_p2, 1, 0.0, &ignore);
    }
    coop_tome_player(cpu, false);
    fprintf(stderr, "[coop] player 2's copy of %u from the save (node 0x%08X) back in their hand\n", id, node);
}

// Player 2's equipment can name a thing held (an entry: its id, then two
// words, as 9, 2, 1 the Tulwar, 11, 2, 0 the Chakrams) with no copy of it in
// their hand: the copies are
// known here only (g_coop_copies), and a quick save loaded, or one gone with
// a load, left it lost. Theirs is put in their hand as the Tome puts a
// thing there (0x800C994C(equipment, id), their copy made for it by
// hle_find_by_id). Looked at once a second.
#define EQUIP_HOLD 0x800C994Cu
#define EQUIP_ENTRIES 388u
static void coop_rehold(CPUState* cpu) {
    static unsigned turns, tries;
    static u32 tries_for;
    if (++turns % 60u != 0u || g_coop_tome_p2 || g_coop_p2 == 0u)
        return;
    if (tries_for != g_coop_p2)
        tries_for = g_coop_p2, tries = 0u;
    const u32 p2 = mem_read32(cpu, g_coop_p2 + 52u);
    u32 own = 0u;
    const u32 find[2] = {mem_read32(cpu, g_coop_p2 + 16u), 0u};
    if (tries >= 3u || !guest_ram_range_valid(cpu, p2, 376u) ||
        !hle_guest_call(cpu, EQUIP_FIND, find, 2, 0.0, &own) || own == 0u)
        return;
    const u32 id = mem_read32(cpu, own + EQUIP_ENTRIES) != 0u ? mem_read32(cpu, own + 4u) : 0u;
    if (id == 0u) {
        // Nothing held, so none of their copies is in their hand (one was
        // left there by letting go while their Tome took player 1's): let
        // go of as letting go leaves it (holder 0).
        for (int i = 0; i < 32; i++) {
            const u32 copy = g_coop_copies[i].node;
            const u32 object = copy != 0u ? mem_read32(cpu, copy + 52u) : 0u;
            if (guest_ram_range_valid(cpu, object, 376u) && mem_read32(cpu, object + 372u) == p2) {
                mem_write32(cpu, object + 372u, 0u);
                fprintf(stderr, "[coop] player 2 held nothing: their copy of %u out of their hand\n",
                        g_coop_copies[i].id);
            }
        }
        return;
    }
    for (u32 node = 0u, n = 0u; n < 1024u && (node = coop_next(cpu, node)) != 0u; n++) {
        const u32 object = mem_read32(cpu, node + 52u);
        if (node != g_coop_p2 && guest_ram_range_valid(cpu, object, 376u) && mem_read32(cpu, object + 372u) == p2) {
            tries = 0u;
            return;  // something is in their hand
        }
    }
    tries++;
    // Their equipment kept as it was (holding lists the thing again).
    u8 record[408];
    coop_bytes(cpu, own, record, sizeof record, true);
    coop_tome_player(cpu, true);
    u32 ignore = 0u;
    const u32 hold[2] = {own, id};
    hle_guest_call(cpu, EQUIP_HOLD, hold, 2, 0.0, &ignore);
    coop_tome_player(cpu, false);
    coop_bytes(cpu, own, record, sizeof record, false);
    fprintf(stderr, "[coop] player 2 held %u with nothing in their hand: their copy put there again\n", id);
}

// ED_COPY_WATCH=1 (testing): the readying and letting go of a copy's
// animation buffers (0x8011F460, 0x8011FC8C), and who called.
static void notify_copy_watch(CPUState* cpu) {
    static unsigned shown;
    // A held thing's bones made for drawing (0x8011D394(object, ...)): how
    // often, for player 2's held things, a second, with where they are.
    if (cpu->pc == 0x8011D394u) {
        static unsigned drawn, frames;
        static u64 last;
        const u32 o = cpu->gpr[3];
        const u32 p2 = g_coop_p2 != 0u ? mem_read32(cpu, g_coop_p2 + 52u) : 0u;
        if (p2 != 0u && guest_ram_range_valid(cpu, o + 372u, 4u) && o != p2 && mem_read32(cpu, o + 372u) == p2)
            drawn++;
        const u64 now = cpu->timebase / 40500000ull;
        if (now != last && p2 != 0u) {
            last = now;
            if (frames++ < 60u)
                fprintf(stderr, "[held] drawn %u a second; player 2 at %.0f %.0f\n", drawn, guest_read_f32(cpu, p2),
                        guest_read_f32(cpu, p2 + 4u));
            drawn = 0u;
        }
        return;
    }
    // The hand link: a held thing's bone found in its holder (0x8011D33C(
    // holder, tag)) and a bone's matrix (0x8012123C(object, bone, out)), for
    // player 2 and the held things.
    if (cpu->pc == 0x8011D33Cu || cpu->pc == 0x8012123Cu) {
        static unsigned linked;
        const u32 o = cpu->gpr[3];
        const bool p2 = g_coop_p2 != 0u && o == mem_read32(cpu, g_coop_p2 + 52u);
        const bool held = guest_ram_range_valid(cpu, o + 372u, 4u) && mem_read32(cpu, o + 372u) != o &&
                          g_coop_p2 != 0u && mem_read32(cpu, o + 372u) == mem_read32(cpu, g_coop_p2 + 52u);
        if ((held || (p2 && cpu->lr == 0x80121334u)) && linked++ < 300u)
            fprintf(stderr, "[link] 0x%08X on 0x%08X%s r4 0x%08X (lr 0x%08X)\n", cpu->pc, o, p2 ? " (p2)" : " (held)",
                    cpu->gpr[4], cpu->lr);
        return;
    }
    for (int i = 0; i < 32 && shown < 200u; i++)
        if (g_coop_copies[i].node != 0u && mem_read32(cpu, g_coop_copies[i].node + 52u) == cpu->gpr[3]) {
            shown++;
            fprintf(stderr, "[copy] 0x%08X on 0x%08X +672 0x%08X (lr 0x%08X)\n", cpu->pc, cpu->gpr[3],
                    mem_read32(cpu, cpu->gpr[3] + 672u), cpu->lr);
        }
}

// In their Tome and in their turn (a throw readies the next Chakram).
static void hle_find_by_id(CPUState* cpu) {
    g_hle_passthrough = true;
    if (!(g_coop_tome_p2 || g_coop_turn) || g_coop_p2 == 0u)
        return;
    const u32 id = cpu->gpr[3];
    u32 node = 0u;
    if ((cpu->lr == 0x800C99ACu || cpu->lr == 0x800C9E34u) && id == mem_read32(cpu, g_coop_p2 + 16u))
        node = g_coop_p2;  // whose hand
    else if ((cpu->lr == 0x800C996Cu || cpu->lr == 0x800C9DF4u) && getenv("ED_COOP_NO_COPIES") == NULL)
        node = coop_weapon_copy(cpu, id, cpu->lr == 0x800C996Cu);  // what goes in it (ED_COOP_NO_COPIES=1: the one)
    if (node != 0u) {
        g_hle_passthrough = false;
        hle_set_u32(cpu, node);
    }
}

// A weapon taken up has the player's moves made again (0x80131CF8): every
// moves slot let go (0x80132484(0)), then the player's asked for anew. In
// player 2's Tome they are the player, and their moves are kept in slots of
// their own (hle_moves_want); letting go of the game's slots took player 1's
// from them, and with no moves player 1 stood still, then twisted. While
// player 2's Tome is open, the game's slots are left as they are.
#define MOVES_LET_GO 0x80132484u
#define MOVES_REMAKE_CALL 0x80131D14u  // the return address in 0x80131CF8

static void hle_moves_let_go(CPUState* cpu) {
    g_hle_passthrough = !(g_coop_tome_p2 && g_coop_p2 != 0u && cpu->lr == MOVES_REMAKE_CALL);
}

// What player 2 holds names their copies (the drawing of what is held asks
// for each by its id), but the Tome, which shows the originals, finds them
// by the originals' ids: while their Tome is open their equipment names the
// originals, then the copies again.
static void coop_equip_names(CPUState* cpu, bool copies) {
    u32 rec = 0u;
    const u32 find[2] = {mem_read32(cpu, g_coop_p2 + 16u), 0u};
    if (!hle_guest_call(cpu, EQUIP_FIND, find, 2, 0.0, &rec) || rec == 0u)
        return;
    const u32 n = mem_read32(cpu, rec + 388u);
    for (u32 i = 0; i < n && i < 32u; i++) {
        const u32 at = rec + 4u + i * 12u, id = mem_read32(cpu, at);
        for (int k = 0; k < 32; k++) {
            const u32 copy = g_coop_copies[k].node;
            if (copy == 0u)
                continue;
            if (copies && id == g_coop_copies[k].id)
                mem_write32(cpu, at, mem_read32(cpu, copy + 16u));
            else if (!copies && id == mem_read32(cpu, copy + 16u))
                mem_write32(cpu, at, g_coop_copies[k].id);
        }
    }
}

// Player 1's equipment's entry for a thing (+0 the id, then 12 bytes), or 0.
static u32 coop_equip_names_find(CPUState* cpu, u32 id) {
    const u32 player = coop_player(cpu);
    u32 rec = 0u;
    const u32 find[2] = {player != 0u ? mem_read32(cpu, player + 16u) : 0u, 0u};
    if (player == 0u || !hle_guest_call(cpu, EQUIP_FIND, find, 2, 0.0, &rec) || rec == 0u)
        return 0u;
    const u32 n = mem_read32(cpu, rec + 388u);
    for (u32 i = 0; i < n && i < 32u; i++)
        if (mem_read32(cpu, rec + 4u + i * 12u) == id)
            return rec + 4u + i * 12u;
    return 0u;
}

// A spell done with an item it took from the hand gives it back
// (0x800C9FD4(holder, item, 0, hands, 0)) to the controlled character, by then
// player 1, as what is theirs fits their hands. A copy is player 2's: it goes
// to them, with the hands the original takes.
#define EQUIP_TAKE 0x800C9FD4u
#define TOME_COPY_EQUIPS 16u
static u32 g_tome_copy_equip[TOME_COPY_EQUIPS][5];
static u32 g_tome_copy_equip_count;

static void coop_equip_take(CPUState* cpu) {
    g_hle_passthrough = true;
    if (g_tome_copy_turn) {
        // In their Tome, in the copy: it shows there, and the real one is done after the turn.
        if (cpu->lr >= TOME_ITEM_CODE && cpu->lr < TOME_ITEM_CODE_END && g_tome_copy_equip_count < TOME_COPY_EQUIPS) {
            for (int i = 0; i < 5; i++)
                g_tome_copy_equip[g_tome_copy_equip_count][i] = cpu->gpr[3 + i];
            g_tome_copy_equip_count++;
        }
        return;
    }
    if (g_coop_p2 == 0u || !g_settings.mod_coop || !sda_ready(cpu))
        return;
    const u32 item = cpu->gpr[4], p2 = mem_read32(cpu, g_coop_p2 + 16u);
    if (cpu->gpr[3] == p2)
        return;
    for (int i = 0; i < 32; i++) {
        const u32 copy = g_coop_copies[i].node;
        if (copy == 0u || mem_read32(cpu, copy + 16u) != item || !coop_listed(cpu, copy))
            continue;
        const u32 entry = coop_equip_names_find(cpu, g_coop_copies[i].id);
        cpu->gpr[3] = p2;
        cpu->gpr[6] = entry != 0u ? mem_read32(cpu, entry + 4u) : 2u;
        return;
    }
}

static u32 g_coop_tome_was[2];  // player 1, while player 2's Tome has them the player

static void coop_tome_player(CPUState* cpu, bool p2) {
    u32* was = g_coop_tome_was;
    const u32 who[2] = {cpu->gpr[13] - 26524u, cpu->gpr[13] - 21496u};
    if (p2 && !g_coop_tome_p2 && g_coop_p2 != 0u) {
        coop_equip_record(cpu);
        coop_equip_names(cpu, false);
        const u32 now[2] = {mem_read32(cpu, g_coop_p2 + 52u), mem_read32(cpu, g_coop_p2 + 16u)};
        for (int i = 0; i < 2; i++) {
            was[i] = mem_read32(cpu, who[i]);
            mem_write32(cpu, who[i], now[i]);
        }
        g_coop_tome_p2 = true;
        coop_quick_swap(cpu, true);  // their Quick Spell page sets theirs
        coop_kinds(cpu, true);       // their own counts
    } else if (!p2 && g_coop_tome_p2) {
        for (int i = 0; i < 2; i++)
            mem_write32(cpu, who[i], was[i]);
        g_coop_tome_p2 = false;
        coop_quick_swap(cpu, false);
        coop_kinds(cpu, false);
        g_coop_next_cast_p2 = mem_read32(cpu, COOP_NEXT_CAST) != 0u;  // what they cast from it is theirs
        coop_equip_names(cpu, true);
    }
}

// The game's camera stays player 1's while player 2's Tome has them the
// player: stepped for them as it opens and closes, it went where player 2
// stands and came back slowly, in player 1's half. A close-up from their Tome
// is theirs.
static void coop_game_camera(CPUState* cpu) {
    static bool running;
    if (running || g_coop_own_step || !g_coop_tome_p2 || g_coop_screen || g_coop_cameras.lent) {
        g_hle_passthrough = true;
        return;
    }
    const u32 who[2] = {cpu->gpr[13] - 26524u, cpu->gpr[13] - 21496u};
    u32 theirs[2], ignore = 0u;
    const u32 mask = cpu->gpr[3];
    for (int i = 0; i < 2; i++) {
        theirs[i] = mem_read32(cpu, who[i]);
        mem_write32(cpu, who[i], g_coop_tome_was[i]);
    }
    running = true;
    if (!hle_guest_call(cpu, CAMERA_STEP, &mask, 1, 0.0, &ignore))
        fprintf(stderr, "[coop] the game's camera step did not return\n");
    running = false;
    for (int i = 0; i < 2; i++)
        mem_write32(cpu, who[i], theirs[i]);
    g_hle_passthrough = false;  // the nested dispatch passed itself through
    g_hle_tail_call = false;
}

// A cutscene ends with the player where it left them, by what set it off:
// player 2's, it put player 1 there, who came into its zone at the word it
// left ("its prompt shown"), and the close-up opened for them. Player 1 and
// their cameras go back, in the same room, and the word is player 2's. The
// game moves them a frame or so after it is over: they are held there a moment.
#define COOP_OBJECT_PLACE 60u
#define COOP_CUTSCENE_HOLD 30u
#define COOP_SCENE_AFTER 240u  // pad reads after a cutscene of theirs that what it gives is theirs
static struct {
    u8 place[COOP_OBJECT_PLACE];
    u32 object, zones;
    bool pending, cameras;
    unsigned hold;
} g_coop_cutscene;

static void coop_cutscene_place(CPUState* cpu, u32 object) {
    for (u32 off = 0u; off < COOP_OBJECT_PLACE; off += 4u) {
        if (off >= 12u && off < 44u)
            continue;
        const u8* b = g_coop_cutscene.place + off;
        mem_write32(cpu, object + off, (u32)b[0] << 24 | (u32)b[1] << 16 | (u32)b[2] << 8 | b[3]);
    }
}

// The script's word out of the game's table, to player 2's.
static void coop_cutscene_word(CPUState* cpu) {
    // Something found this frame: the word is as it was found until it has gone on
    // (coop_script_timers). Taken now, that went in their copy over the one it left,
    // and the monk gave his key again.
    if (g_coop_found_word.pending)
        return;
    for (u32 i = 0u; i < 10u; i++) {
        const u32 at = SCRIPT_STATES + i * 8u;
        if ((s16)mem_read16(cpu, at) != g_coop_use.script)
            continue;
        for (u32 j = 0u; j < 10u; j++) {
            u8* theirs = g_coop_states.p2 + j * 8u;
            if ((s16)(theirs[0] << 8 | theirs[1]) == g_coop_use.script || (theirs[0] == 0u && theirs[1] == 0u)) {
                coop_bytes(cpu, at, theirs, 8u, true);
                break;
            }
        }
        for (u32 off = 0u; off < 8u; off += 4u)
            mem_write32(cpu, at + off, 0u);
        break;
    }
}

static void coop_cutscene_held(CPUState* cpu) {
    const u32 object = mem_read32(cpu, cpu->gpr[13] - 26524u);
    if (object != g_coop_cutscene.object) {
        g_coop_cutscene.hold = 0u;
        return;
    }
    coop_cutscene_place(cpu, object);
    if (--g_coop_cutscene.hold == 0u) {
        if (g_coop_cutscene.cameras && !g_coop_cameras.lent) {
            coop_bytes(cpu, COOP_ZONE_CAMERAS, g_coop_cameras.p1, COOP_ZONE_CAMERAS_SIZE, false);
            coop_bytes(cpu, PROMPT_SLOTS, g_coop_cameras.p1_prompts, PROMPT_SLOTS_SIZE, false);
        }
        coop_cutscene_word(cpu);
    }
}

// Talking leaves the player in its state (event 147: 71), which the end of the
// talk takes them out of: whoever is the player then, player 1, so player 2, who
// had talked, stood in it for good. Out of it as what they said is over.
#define COOP_STATE_TALK 71u

static void coop_talk_over(CPUState* cpu) {
    if (g_coop_p2 == 0u || mem_read32(cpu, g_coop_p2 + COOP_NODE_STATE) != COOP_STATE_TALK)
        return;
    const u32 state[2] = {g_coop_p2, 1u};
    u32 ignore = 0u;
    hle_guest_call(cpu, 0x801F64A0u, state, 2, 0.0, &ignore);
    hle_guest_call(cpu, 0x801F6488u, state, 2, 0.0, &ignore);
}

static void coop_cutscene_over(CPUState* cpu) {
    g_coop_cutscene.pending = false;
    coop_talk_over(cpu);
    const u32 object = mem_read32(cpu, cpu->gpr[13] - 26524u);
    u32 info = 0u;
    // A cutscene that ends in another room (the Tome taken): the new room is not in
    // yet, its zones still the old one's, and the old room's place is out of its bounds.
    if (g_coop_room_asked || object != g_coop_cutscene.object || !hle_guest_call(cpu, COOP_CHARACTER_INFO, &object, 1, 0.0, &info) ||
        !guest_ram_range_valid(cpu, info, 184u) || mem_read32(cpu, info + 180u) != g_coop_cutscene.zones)
        return;
    coop_cutscene_place(cpu, object);
    g_coop_cutscene.hold = COOP_CUTSCENE_HOLD;
    if (g_coop_cutscene.cameras) {
        coop_bytes(cpu, COOP_ZONE_CAMERAS, g_coop_cameras.p2, COOP_ZONE_CAMERAS_SIZE, true);
        g_coop_cameras.p2_set = true;
        coop_bytes(cpu, COOP_ZONE_CAMERAS, g_coop_cameras.p1, COOP_ZONE_CAMERAS_SIZE, false);
        coop_bytes(cpu, PROMPT_SLOTS, g_coop_prompt_slots, PROMPT_SLOTS_SIZE, true);
        coop_bytes(cpu, PROMPT_SLOTS, g_coop_cameras.p1_prompts, PROMPT_SLOTS_SIZE, false);
    }
    coop_cutscene_word(cpu);
    fprintf(stderr, "[coop] the cutscene over: player 1 back where they were\n");
}


// The game's prompt (r13 - 24032, made at 0x801170BC) in a close-up of player 2's is theirs, in their
// half: where their own goes (COOP_PROMPT_AT, which only their view brings back). At the game's own
// place it was in player 1's corner. Put back as the close-up ends.
#define COOP_GAME_PROMPT 24032u
static void coop_close_up_prompt(CPUState* cpu) {
    static u32 moved;
    static u16 own_x;
    const u32 element = mem_read32(cpu, cpu->gpr[13] - COOP_GAME_PROMPT);
    const bool valid = guest_ram_range_valid(cpu, element, 0x100u);
    if (g_coop_screen && valid) {
        if (element != moved) {
            moved = element;
            own_x = mem_read16(cpu, element + 8u);
        }
        mem_write16(cpu, element + 8u, (u16)(s16)COOP_PROMPT_AT);
    } else if (!g_coop_screen && moved != 0u) {
        if (valid && element == moved)
            mem_write16(cpu, element + 8u, own_x);
        moved = 0u;
    }
    // Their close-up over, the hold's bit player 1's turn kept is gone with it: with nothing holding
    // (its channel off), it stopped both players' controllers for good.
    static bool was;
    const u32 hold_channel = mem_read32(cpu, cpu->gpr[13] - 23060u);
    if (was && !g_coop_screen && mem_read32(cpu, cpu->gpr[13] - 23344u) == 2u && guest_ram_range_valid(cpu, hold_channel, 20u) &&
        mem_read32(cpu, hold_channel + 16u) == 0u)
        mem_write32(cpu, cpu->gpr[13] - 23344u, 0u);
    // What they found: the word it ended at goes across (coop_script_timers), or player 1 walking up to
    // it later went on from the one copied as it was found, and found it again.
    if (was && !g_coop_screen && g_coop_found_word.p2 && g_coop_found_word.script > 0 && g_coop_found_word.script == g_coop_use.script)
        g_coop_found_word.pending = true;
    was = g_coop_screen;
}

static void coop_menu_input(CPUState* cpu, bool menu, bool cutscene, DolPadState* pad) {
    static int owner = -1;  // -1 the game's own, 0 player 1, 1 player 2
    static unsigned start_wait;
    static bool start_was;
    static unsigned quick_wait;  // pad reads for the Tome their quick spell asked for
    if (coop_trace() && sda_ready(cpu) && g_coop_p2 != 0u && g_trace_ticket_frame != 0ull) {  // a room change: where they are
        const unsigned long long frame = cpu->timebase / 675000ull;
        static unsigned long long last;
        if (frame != last && frame < g_trace_ticket_frame + 300ull) {
            const u32 o1 = mem_read32(cpu, cpu->gpr[13] - 26524u), o2 = mem_read32(cpu, g_coop_p2 + 52u);
            if (guest_ram_range_valid(cpu, o1, 12u) && guest_ram_range_valid(cpu, o2, 12u))
                fprintf(stderr, "[trace] room move +%llu game player %.0f %.0f %.0f | player 2 %.0f %.0f %.0f | who %u owner %d screen %d busy %d\n", frame - g_trace_ticket_frame,
                        guest_read_f32(cpu, o1), guest_read_f32(cpu, o1 + 4u), guest_read_f32(cpu, o1 + 8u), guest_read_f32(cpu, o2), guest_read_f32(cpu, o2 + 4u), guest_read_f32(cpu, o2 + 8u),
                        mem_read32(cpu, cpu->gpr[13] - 21496u), owner, g_coop_screen, mem_read32(cpu, cpu->gpr[13] - 23344u));
        }
        last = frame;
    }
    if (coop_trace() && sda_ready(cpu)) {  // the Tome's item page, the spell it waits on
        static const u32 kPage[3] = {24492u, 24508u, 24304u};
        static u32 page[3], spell[3];
        const u32 now[3] = {mem_read32(cpu, COOP_SPELL_ITEM), mem_read32(cpu, COOP_SPELL_ITEM + 12u), mem_read32(cpu, COOP_SPELL_ITEM + 16u)};
        for (int i = 0; i < 3; i++) {
            const u32 v = mem_read32(cpu, cpu->gpr[13] - kPage[i]);
            if (v != page[i])
                fprintf(stderr, "[trace] Tome r13-%u 0x%X -> 0x%X (owner %d sharing %d frame %llu)\n", kPage[i], page[i], v, owner, g_tw.sharing,
                        (unsigned long long)(cpu->timebase / 675000ull));
            page[i] = v;
        }
        if (memcmp(spell, now, sizeof now) != 0)
            fprintf(stderr, "[trace] spell item %d spell 0x%08X flags 0x%X (owner %d frame %llu)\n", (s32)now[0], now[1], now[2], owner,
                    (unsigned long long)(cpu->timebase / 675000ull));
        memcpy(spell, now, sizeof now);
    }
    g_coop_tome_owner = owner;
    // The world behind the Tome: a read is for the frame after it. For the world's,
    // the Tome's owner stands; for the Tome's, it is their pad as ever.
    if (hle_coop_tome_shares()) {
        // Player 1's Tome has closed with player 2's still open: their pad is theirs again.
        if (g_tome_copy.stands_in && owner == 0) {
            owner = -1;
            g_coop_tome_owner = -1;
            fprintf(stderr, "[tome-copy] player 1's Tome has closed; player 2's stays\n");
        }
        if (tome_copy_mode() == 2 && g_settings.mod_coop && g_coop_p2 != 0u && !g_coop_screen) {
            DolPadState theirs;
            controls_read_player2(&theirs);
            const bool down = (theirs.button & 0x1000u) != 0u;
            if (down && !start_was && !g_tome_copy_open && !g_tome_copy_ask)
                g_tome_copy_ask = true;
            start_was = down;
        }
        // The buttons held at the last read, which make a press new: the world's and
        // the Tome's reads are different pads to it, each with its own (held by turns,
        // a button was a new press each time, and a Tome opened with it shut again).
        static u32 held[2];
        const int turn = tw_tome_next() ? 1 : 0;
        const u32 now = mem_read32(cpu, COOP_INPUT + 48u);
        if (g_tw_held_from < 0)
            held[0] = held[1] = now;
        else
            held[g_tw_held_from] = now;
        g_tw_held_from = turn;
        mem_write32(cpu, COOP_INPUT + 48u, held[turn]);
        if (!tw_tome_next()) {
            if (owner == 0) {
                const s32 error = pad->error;
                g_tw_taps |= pad->button;
                memset(pad, 0, sizeof *pad);
                pad->error = error;
            }
            // Words in a Tome the game opened for player 2 (a rune they found) go on in the world's
            // frames, and took player 1's B while their half showed them: player 2's pad for those.
            if (owner == 1 && mem_read32(cpu, cpu->gpr[13] - 25960u) != 0u) {
                controls_set_tome_read(1);
                controls_read_player2(pad);
                controls_set_tome_read(-1);
                pad->error = 0;
            }
            return;
        }
        if (owner == 0) {
            pad->button |= g_tw_taps;
            g_tw_taps = 0u;
        }
        menu = true;
    }
    g_coop_screen = false;
    g_coop_cutscene_pending = g_coop_cutscene.pending;
    if (g_coop_scene_script != 0 && !cutscene && ++g_coop_scene_after > COOP_SCENE_AFTER)
        g_coop_scene_script = 0;
    if (g_coop_cutscene.pending && !cutscene && !menu && g_settings.mod_coop && g_coop_p2 != 0u)
        coop_cutscene_over(cpu);
    else if (g_coop_cutscene.hold > 0u && !cutscene && !menu)
        coop_cutscene_held(cpu);
    if (!g_settings.mod_coop || g_coop_p2 == 0u) {
        g_coop_quick_tome = false;
        quick_wait = 0u;
        coop_tome_player(cpu, false);
        if (owner >= 0)
            aurora_split_menu(-1, 0);
        owner = -1;
        start_wait = 0u;
        return;
    }
    static u16 pad_held;  // what they held as their close-up opened: not a press in it
    static bool quick_press;  // their Tome opened by a quick spell's button, still held
    if (g_coop_quick_tome) {
        g_coop_quick_tome = false;
        if (owner < 0) {
            owner = 1;
            quick_press = true;
            quick_wait = COOP_START_WAIT;
            coop_tome_player(cpu, true);
            fprintf(stderr, "[coop] player 2's quick spell opens the Tome\n");
        }
    }
    // The item their cast shows, tied to the game's camera, and its words stop
    // play: theirs, as a close-up is, in their half with their pad. Player 1's
    // half keeps its view.
    if (g_coop_cast_words > 0u && owner < 0) {
        g_coop_cast_words--;
        if (coop_busy(cpu)) {
            g_coop_cast_words = 0u;
            g_coop_assist_at = 1u;
            coop_cameras_lend(cpu, true);
        }
    }
    // Player 1's is tied to the game's camera too: seen from player 2's, it lay
    // across their half.
    if (g_coop_p1_cast_words > 0u && owner < 0) {
        g_coop_p1_cast_words--;
        if (coop_busy(cpu)) {
            g_coop_p1_cast_words = 0u;
            g_coop_p1_cast_shown = true;
        }
    }
    if (g_coop_p1_cast_shown && !coop_busy(cpu))
        g_coop_p1_cast_shown = false;
    // A close-up player 2's press opened stops play for them; a moment
    // without (between its pages) does not end it.
    static unsigned closeup_grace;
    if (!coop_busy(cpu))
        g_coop_script_hold = false;
    // Player 1's own look is never player 2's, whatever came just before it.
    // A script's own hold doesn't begin one (walking into a way out), but one begun stays theirs through
    // it (the bookshelf's lever, its scene after their Yes): it went back to player 1 as it began, and
    // the scene's camera never showed in their half.
    bool closeup = !menu && !cutscene && (owner == 1 || (g_coop_assist_at != 0u && !g_coop_script_hold)) && coop_busy(cpu) && !g_coop_p1_look;
    if (closeup && owner != 1 && coop_trace())
        fprintf(stderr, "[trace] close-up for player 2: assist %u busy %u player 1's look %d frame %llu\n", g_coop_assist_at, mem_read32(cpu, cpu->gpr[13] - 23344u), g_coop_p1_look,
                (unsigned long long)(cpu->timebase / 675000ull));
    if (closeup && owner == 1)
        closeup_grace = 20u;
    else if (!closeup && owner == 1 && closeup_grace > 0u && !menu && !cutscene) {
        closeup_grace--;
        closeup = true;
    }
    // A close-up of theirs that asks for another room (walking into a way out, no door's
    // prompt) ends with it: player 2 kept the screen into the new room, its scene in their
    // half and player 1's held. Play goes on as when player 1 asked for it.
    if (g_coop_room_asked_now) {
        g_coop_room_asked_now = false;
        if (!menu && !cutscene) {
            closeup = false;
            closeup_grace = 0u;
            g_coop_assist_at = 0u;  // or the new room's first scene is taken for theirs
        }
    }
    DolPadState p2;
    controls_read_player2(&p2);
    if (hle_coop_tome_shares() && owner == 1) {
        p2.button |= g_tw_taps;
        g_tw_taps = 0u;
    }
    // The button that opened it, a press on the first world frame behind the Tome,
    // cast player 1's spell from that slot, over the one their Tome was asking for.
    if (quick_press) {
        quick_press = false;
        pad_held = p2.button;
    }
    const bool start = (p2.button & 0x1000u) != 0u;
    const bool start_pressed = start && !start_was;
    start_was = start;
    if (!menu && !cutscene && !closeup) {
        // Where player 1 is in play, for a cutscene of player 2's.
        const u32 p1_object = mem_read32(cpu, cpu->gpr[13] - 26524u);
        if (owner < 0 && !coop_busy(cpu) && !g_coop_tome_p2 && guest_ram_range_valid(cpu, p1_object, COOP_OBJECT_PLACE)) {
            g_coop_cutscene.object = p1_object;
            coop_bytes(cpu, p1_object, g_coop_cutscene.place, COOP_OBJECT_PLACE, true);
        }
        if (g_coop_assist_at != 0u && ++g_coop_assist_at > COOP_ASSIST_WINDOW)
            g_coop_assist_at = 0u;
        // Player 1's own Start: the Tome it opens is theirs. Just after player 2's page or close-up, it went
        // to player 2, who stood in a Tome they hadn't asked for (run-20261103-182310-s1-room, frame 10278).
        if ((pad->button & 0x1000u) != 0u && g_coop_assist_at != 0u) {
            if (getenv("ED_COOP_LOG") != NULL)
                fprintf(stderr, "[coop] player 1's Start: what opens next is theirs\n");
            g_coop_assist_at = 0u;
        }
        if (g_tome_copy_ask)
            return;
        if (start_wait > 0u) {
            start_wait--;
            pad->button |= 0x1000u;  // player 2's Start, as the game's
            pad->error = 0;
            return;
        }
        if (quick_wait > 0u) {
            quick_wait--;
            return;
        }
        if (owner >= 0) {
            if (owner == 1) {
                g_coop_b_release = true;
                g_coop_p2_hold = p2.button;
            }
            // The game's prompt goes back to player 1's corner: left in player 2's half, under their own,
            // player 1 had no icon after any close-up of player 2's where the prompt carried on.
            coop_close_up_prompt(cpu);
            fprintf(stderr, "[coop] play goes on: the pad is player 1's again\n");
            coop_talk_over(cpu);
            coop_cameras_lend(cpu, false);
            coop_snapshot(cpu);
            coop_tome_player(cpu, false);
            owner = -1;
            aurora_split_menu(-1, 0);
        }
        if (start_pressed && (pad->button & 0x1000u) == 0u && tome_copy_mode() == 2) {
            if (!(g_tome_copy_open && g_tome_copy_p1))
                g_tome_copy_ask = true;
            return;
        }
        if (start_pressed && (pad->button & 0x1000u) == 0u) {
            start_wait = COOP_START_WAIT;
            owner = 1;
            coop_tome_player(cpu, true);
            fprintf(stderr, "[coop] player 2 opens the Tome\n");
            coop_snapshot(cpu);
        }
        return;
    }
    start_wait = 0u;
    quick_wait = 0u;
    // A cutscene player 2's close-up led to (the Tome taken) is the game's:
    // play is handed back as it starts. The cameras lent stay the game's
    // (put back, the cutscene's own went and it ended; put back after it,
    // player 1's camera was the old room's in the new).
    if (cutscene && owner == 1) {
        fprintf(stderr, "[coop] a cutscene: the screen and the pad are player 1's again\n");
        // Its end runs the script that began it again, in the game's table:
        // with its word still theirs, that began it again from the start.
        if (g_coop_use.script != 0)
            coop_state_to_game(cpu, g_coop_use.script);
        g_coop_scene_script = g_coop_use.script;
        g_coop_scene_after = 0u;
        g_coop_cutscene.pending = g_coop_cutscene.object != 0u;
        g_coop_room_asked = false;
        g_coop_cutscene.cameras = g_coop_cameras.lent;
        g_coop_cutscene.zones = g_coop_zones.list;
        g_coop_b_release = true;
        g_coop_p2_hold = p2.button;
        g_coop_cameras.lent = false;
        coop_snapshot(cpu);
        coop_tome_player(cpu, false);
        owner = -1;
        closeup_grace = 0u;
        g_coop_assist_at = 0u;
        aurora_split_menu(-1, 0);
        return;
    }
    if (owner < 0) {
        owner = g_coop_assist_at != 0u && !g_coop_p1_look ? 1 : menu ? 0 : -1;
        g_coop_assist_at = 0u;
        pad_held = owner == 1 ? p2.button : 0u;
        if (owner >= 0)
            fprintf(stderr, "[coop] %s has the %s\n", owner == 1 ? "player 2" : "player 1",
                    closeup ? "close-up" : "screen");
    }
    g_coop_screen = closeup && owner == 1;
    coop_close_up_prompt(cpu);
    // Player 1's Start in player 2's close-up: their Tome, in a copy of its own. The close-up goes on.
    {
        static bool p1_start_was;
        const bool p1_start = (g_tome_copy_p1_pad.button & 0x1000u) != 0u;
        if (p1_start && !p1_start_was && g_coop_screen && tome_copy_mode() == 2 && !g_tome_copy_open && !g_tome_copy_ask) {
            g_tome_copy_p1 = true;
            g_tome_copy_ask = true;
            fprintf(stderr, "[coop] player 1 opens their Tome in player 2's close-up\n");
        }
        p1_start_was = p1_start;
    }
    if (owner == 1) {
        pad_held &= p2.button;
        static u16 trace_pad;
        if (coop_trace() && p2.button != trace_pad)
            fprintf(stderr, "[trace] player 2's screen pad 0x%04X held 0x%04X screen %d busy %d frame %llu\n", p2.button, pad_held, g_coop_screen,
                    coop_busy(cpu), (unsigned long long)(cpu->timebase / 675000ull));
        trace_pad = p2.button;
        p2.button &= (u16)~pad_held;
        // The game's pad, and their Start would open the game's Tome over their close-up, and neither went on.
        if (g_coop_screen)
            p2.button &= (u16)~0x1000u;
        *pad = p2;
        pad->error = 0;
    }
    // A cutscene shows whole, once, as the game alone shows it (the Tome
    // taken), whoever's press led to it.
    if (owner >= 0 && !(g_tome_copy_open && g_tome_copy_p1))
        aurora_split_menu(cutscene ? -1 : owner, g_settings.mod_split);
    g_coop_tome_owner = owner;
}

// What player 2 equipped in the copy, in the real game: the same call with them the player, as
// in their Tome (coop_tome_player: their copies, their moves), and what the Tome's way out does
// after one (0x80110D1C), which never runs for the copy.
static void tome_copy_equips(CPUState* cpu) {
    if (g_tome_copy_equip_count == 0u)
        return;
    u32 ignore = 0u;
    if (g_tome_copy_p1) {
        const u32 node = mem_read32(cpu, cpu->gpr[13] - 21492u);
        for (u32 i = 0u; i < g_tome_copy_equip_count; i++) {
            fprintf(stderr, "[tome-copy] player 1 equips %u (hands %u) in the real game\n", g_tome_copy_equip[i][1], g_tome_copy_equip[i][3]);
            hle_guest_call(cpu, EQUIP_TAKE, g_tome_copy_equip[i], 5, 0.0, &ignore);
        }
        hle_guest_call(cpu, EQUIP_MOVES, &node, 1, 0.0, &ignore);
        g_tome_copy_equip_count = 0u;
        return;
    }
    coop_tome_player(cpu, true);
    for (u32 i = 0u; i < g_tome_copy_equip_count; i++) {
        fprintf(stderr, "[tome-copy] player 2 equips %u (hands %u) in the real game\n", g_tome_copy_equip[i][1], g_tome_copy_equip[i][3]);
        hle_guest_call(cpu, EQUIP_TAKE, g_tome_copy_equip[i], 5, 0.0, &ignore);
    }
    hle_guest_call(cpu, EQUIP_MOVES, &g_coop_p2, 1, 0.0, &ignore);
    coop_tome_player(cpu, false);
    g_coop_next_cast_p2 = false;  // coop_tome_player's reading of the real memory: nothing's asked yet
    g_tome_copy_equip_count = 0u;
}

// What player 1's Tome left for the world, with player 2's close-up over: as the world does when a Tome
// closes, and they're the player already.
static void tome_copy_p1_left(CPUState* cpu) {
    if (!g_tome_copy_p1_left.pending || g_coop_screen || coop_busy(cpu) || g_coop_use.active || g_tome_copy_open ||
        mem_read32(cpu, cpu->gpr[13] - 25960u) != 0u)
        return;
    g_tome_copy_p1_left.pending = false;
    u32 ignore = 0u;
    if (g_tome_copy_p1_left.next_cast != 0u) {
        fprintf(stderr, "[tome-copy] player 1's cast 0x%08X goes to the real game\n", g_tome_copy_p1_left.next_cast);
        mem_write32(cpu, COOP_NEXT_CAST, g_tome_copy_p1_left.next_cast);
        coop_bytes(cpu, COOP_SPELL_ITEM, g_tome_copy_p1_left.spell_item, 24u, false);
        const u32 args[2] = {1u, COOP_NEXT_CAST};
        hle_guest_call(cpu, COOP_CAST, args, 2, 0.0, &ignore);
        mem_write32(cpu, COOP_NEXT_CAST, 0u);
    }
    const s32 script = g_tome_copy_p1_left.script;
    if (script <= 0)
        return;
    fprintf(stderr, "[tome-copy] player 1's item leaves script %d for the real game\n", script);
    if (script == 2243) {
        u32 who = 0u;
        hle_guest_call(cpu, 0x801F62B8u, NULL, 0, 0.0, &who);
        hle_guest_call(cpu, 0x801C4F48u, &who, 1, 0.0, &ignore);
    }
    const u32 item_was = mem_read32(cpu, COOP_SPELL_ITEM);
    u32 item_chosen = 0u;
    memcpy(&item_chosen, g_tome_copy_p1_left.spell_item, sizeof item_chosen);
    mem_write32(cpu, COOP_SPELL_ITEM, __builtin_bswap32(item_chosen));
    if (g_tome_copy_p1_left.use_script == script) {
        const u32 asked[3] = {(u32)script, g_tome_copy_p1_left.use_item, 0u};
        hle_guest_call(cpu, COOP_RUN_SCRIPT, asked, 3, 0.0, &ignore);
    }
    if (mem_read32(cpu, cpu->gpr[13] - 25960u) == 0u) {
        const u32 args[3] = {(u32)script, 0u, 0u};
        hle_guest_call(cpu, COOP_RUN_SCRIPT, args, 3, 0.0, &ignore);
    }
    mem_write32(cpu, COOP_SPELL_ITEM, item_was);
}

// What the Tome left for the world, in the real game, as the world does when a Tome closes
// (0x80008280: the script, and 2243's call first; 0x80008468 casts on its next frame). They're
// the player, with their counts and zone words, as coop_run_script has it for a Tome's item.
static void tome_copy_left(CPUState* cpu, s32 script, u32 next_cast, const u8* spell_item) {
    g_coop_next_cast_p2 = false;
    u32 was[5];
    if (next_cast != 0u && coop_listed(cpu, g_coop_p2)) {
        fprintf(stderr, "[tome-copy] player 2's cast 0x%08X goes to the real game\n", next_cast);
        const u32 next_was = mem_read32(cpu, COOP_NEXT_CAST);
        u8 record_was[24];
        coop_bytes(cpu, COOP_SPELL_ITEM, record_was, sizeof record_was, true);
        mem_write32(cpu, COOP_NEXT_CAST, next_cast);
        coop_bytes(cpu, COOP_SPELL_ITEM, (u8*)spell_item, 24u, false);
        // As coop_cast: an item chosen is the original, as it shows; the one they hold is their copy.
        const u32 item = mem_read32(cpu, COOP_SPELL_ITEM);
        const u32 copy = coop_weapon_copy(cpu, item, false);
        if (copy != 0u) {
            mem_write32(cpu, COOP_SPELL_ITEM, mem_read32(cpu, copy + 16u));
            g_coop_cast_words = COOP_CAST_WORDS_WAIT;
        }
        const u32 args[2] = {1u, COOP_NEXT_CAST};
        u32 result = 0u;
        coop_be_player2(cpu, true, was);
        g_coop_turn_p1 = was[1];
        g_coop_tome_cast = true;
        hle_guest_call(cpu, COOP_CAST, args, 2, 0.0, &result);
        g_coop_tome_cast = false;
        coop_be_player2(cpu, false, was);
        mem_write32(cpu, COOP_NEXT_CAST, next_was);
        coop_bytes(cpu, COOP_SPELL_ITEM, record_was, sizeof record_was, false);
    } else if (g_tome_copy_spell_open) {
        // Backed out of what their cast asked: it's over, or no other could be begun.
        const u32 none = 0u;
        u32 ignored = 0u;
        coop_be_player2(cpu, true, was);
        hle_guest_call(cpu, COOP_CAST_SET, &none, 1, 0.0, &ignored);
        coop_be_player2(cpu, false, was);
        // Their cast left them in its state (64), where every later one is refused: out of it, as coop_talk_over.
        if (mem_read32(cpu, g_coop_p2 + COOP_NODE_STATE) == 64u) {
            const u32 state[2] = {g_coop_p2, 1u};
            hle_guest_call(cpu, 0x801F64A0u, state, 2, 0.0, &ignored);
            hle_guest_call(cpu, 0x801F6488u, state, 2, 0.0, &ignored);
        }
        fprintf(stderr, "[tome-copy] player 2 backs out of their cast\n");
    }
    g_tome_copy_spell_open = false;
    if (script <= 0)
        return;
    fprintf(stderr, "[tome-copy] player 2's item leaves script %d for the real game\n", script);
    coop_tome_player(cpu, true);
    const u32 object = mem_read32(cpu, g_coop_p2 + 52u);
    u32 info = 0u, ignore = 0u;
    const bool zones = hle_guest_call(cpu, COOP_CHARACTER_INFO, &object, 1, 0.0, &info) && guest_ram_range_valid(cpu, info, 184u);
    coop_be_player2(cpu, true, was);
    g_coop_turn_p1 = was[1];
    g_coop_tome_cast = true;
    if (zones)
        coop_zones_swap(cpu, info, true);
    if (script == 2243) {
        u32 who = 0u;
        hle_guest_call(cpu, 0x801F62B8u, NULL, 0, 0.0, &who);
        hle_guest_call(cpu, 0x801C4F48u, &who, 1, 0.0, &ignore);
    }
    // An item's script takes its item from the spell record (0x80015C30, by 0x80110478): the one
    // their Tome chose, as the real record has the last cast's. The Tulwar there, count 0, was "drained".
    const u32 item_was = mem_read32(cpu, COOP_SPELL_ITEM);
    u32 item_chosen = 0u;
    memcpy(&item_chosen, spell_item, sizeof item_chosen);
    item_chosen = __builtin_bswap32(item_chosen);
    fprintf(stderr, "[tome-copy] its item 0x%08X, the real record's 0x%08X\n", item_chosen, item_was);
    mem_write32(cpu, COOP_SPELL_ITEM, item_chosen);
    // The Tome's call asked it first, and it went on from where that left its word: both are
    // the real game's, or the first use only did what the Tome's call does.
    if (g_tome_copy_use_script == script) {
        const u32 asked[3] = {(u32)script, g_tome_copy_use_item, 0u};
        hle_guest_call(cpu, COOP_RUN_SCRIPT, asked, 3, 0.0, &ignore);
    }
    // If that opened words, going on opens them again: the first keep the game's one focus
    // (0x8013DB1C), the close only knows the second (0x80024220), and they never close.
    if (mem_read32(cpu, cpu->gpr[13] - 25960u) == 0u) {
        const u32 args[3] = {(u32)script, 0u, 0u};
        hle_guest_call(cpu, COOP_RUN_SCRIPT, args, 3, 0.0, &ignore);
    } else {
        fprintf(stderr, "[tome-copy] its script opened words: no second step\n");
    }
    mem_write32(cpu, COOP_SPELL_ITEM, item_was);
    // Words their item opened are theirs, in their half with their pad, as a close-up of theirs:
    // with no Tome of the game's up, the words showed in player 1's half and took player 1's B.
    // The script's word goes with them: closing the words runs it as theirs (coop_use_start), from
    // their table, and from its start it asked for the Tome again and waited for ever.
    if (mem_read32(cpu, cpu->gpr[13] - 25960u) != 0u) {
        g_coop_assist_at = 1u;
        // From this frame: player 1's half showed the words for a moment first.
        g_coop_screen = true;
        coop_cameras_lend(cpu, true);  // the game's camera is theirs, as in a close-up of theirs
        coop_hold_player1();
        for (u32 i = 0u; i < 10u; i++) {
            const u32 word = SCRIPT_STATES + i * 8u;
            if ((s16)mem_read16(cpu, word) != script)
                continue;
            u8* to = NULL;
            for (u32 k = 0u; k < 10u && to == NULL; k++)
                if ((s16)(g_coop_states.p2[k * 8u] << 8 | g_coop_states.p2[k * 8u + 1u]) == script)
                    to = g_coop_states.p2 + k * 8u;
            for (u32 k = 0u; k < 10u && to == NULL; k++)
                if (g_coop_states.p2[k * 8u] == 0u && g_coop_states.p2[k * 8u + 1u] == 0u)
                    to = g_coop_states.p2 + k * 8u;
            if (to == NULL)
                break;
            coop_bytes(cpu, word, to, 8u, true);
            for (u32 b = 0u; b < 8u; b += 4u)
                mem_write32(cpu, word + b, 0u);
            break;
        }
    }
    mem_write32(cpu, cpu->gpr[13] - 26324u, 0u);  // as the world's call, 0x800082B0
    g_tome_copy_use_script = 0;
    if (zones)
        coop_zones_swap(cpu, info, false);
    g_coop_tome_cast = false;
    coop_be_player2(cpu, false, was);
    coop_tome_player(cpu, false);
    g_coop_next_cast_p2 = false;
}

// Moving while casting ends the cast: 0x801C4F48(character) sets the word its spell looks at
// (r13 - 22312), if that character is the controlled one. In player 2's turn that's still player
// 1, so theirs was passed over and their spell went on. Their word is kept apart (g_coop_casting).
#define COOP_CAST_CANCEL 0x801C4F48u
static void coop_cast_cancel(CPUState* cpu) {
    g_hle_passthrough = true;
    if (g_tome_copy_turn || g_coop_p2 == 0u || !sda_ready(cpu) || cpu->gpr[3] != mem_read32(cpu, g_coop_p2 + 16u) ||
        mem_read32(cpu, cpu->gpr[13] - 21488u) == cpu->gpr[3] || g_coop_casting[0] == 0u)
        return;
    g_coop_casting[3] = 1u;
    fprintf(stderr, "[coop] player 2 moves: their cast ends\n");
    g_hle_passthrough = false;
    g_hle_tail_call = false;
}

// ED_COOP_CAST_TRACE=1 (testing, only reads): who ends a cast, and from where.
static void notify_cast_trace(CPUState* cpu) {
    const u32 frame = (u32)(cpu->timebase / 675000ull);
    static u32 last_pc, last_lr, last_frame;
    if (cpu->pc == last_pc && cpu->lr == last_lr && frame - last_frame < 30u)
        return;
    last_pc = cpu->pc;
    last_lr = cpu->lr;
    last_frame = frame;
    fprintf(stderr, "[cast-trace] 0x%08X r3 0x%X r4 0x%X lr 0x%08X ctl %u who %u casting 0x%X turn %d state1 %u state2 %u frame %u\n", cpu->pc, cpu->gpr[3], cpu->gpr[4], cpu->lr,
            mem_read32(cpu, cpu->gpr[13] - 21488u), mem_read32(cpu, cpu->gpr[13] - 21496u), mem_read32(cpu, cpu->gpr[13] - 22336u), g_coop_turn,
            coop_player(cpu) != 0u ? mem_read32(cpu, coop_player(cpu) + COOP_NODE_STATE) : 0u, g_coop_p2 != 0u ? mem_read32(cpu, g_coop_p2 + COOP_NODE_STATE) : 0u, frame);
}

// In the copy's turn the game is alone: co-op's hooks that step the world or keep host state
// stand aside, and what would have called them is said once. The rest (the SDK's, the drawing's) run.
bool tome_copy_hook_off(HleHandler fn) {
    static const struct {
        HleHandler fn;
        const char* name;
    } kOff[] = {
#define OFF(f) {f, #f}
        OFF(hle_coop_controller), OFF(coop_sighting), OFF(coop_events), OFF(coop_character_step), OFF(hle_coop_found), OFF(coop_bar_frame),
        OFF(coop_find_node), OFF(coop_stat_most), OFF(coop_thing_look), OFF(coop_talk_check), OFF(coop_script_timers), OFF(coop_door_motion),
        OFF(coop_script_go_to_room), OFF(coop_cast), OFF(tw_mode_change), OFF(tw_tome_start), OFF(coop_controlled_id), OFF(coop_play_hold),
        OFF(coop_enchanted_look), OFF(coop_sign_follow), OFF(coop_lasting_find), OFF(coop_spells_update),
        OFF(coop_game_camera), OFF(hle_moves_let_go), OFF(hle_moves_want), OFF(hle_find_by_id), OFF(coop_position_copy),
        OFF(coop_send_event), OFF(notify_GXSetDrawDone), OFF(coop_prompt_choose), OFF(coop_prompt_set), OFF(coop_prompt_clear), OFF(coop_scan_player1), OFF(coop_words_hold), OFF(coop_find_hold), OFF(coop_look_prompt), OFF(coop_script_sends), OFF(coop_show_asked), OFF(coop_show_main_asked), OFF(coop_found_object_draw), OFF(coop_busy_ask),
#undef OFF
    };
    static bool said[sizeof kOff / sizeof kOff[0]];
    for (size_t i = 0; i < sizeof kOff / sizeof kOff[0]; i++)
        if (kOff[i].fn == fn) {
            if (!said[i]) {
                said[i] = true;
                fprintf(stderr, "[tome-copy] stood aside in its turn: %s\n", kOff[i].name);
            }
            return true;
        }
    return false;
}

static void coop_test_swap(CPUState* cpu) {
    const char* at = getenv("ED_COOP_SWAP_AT");
    if (at == NULL || g_coop_p2 == 0u)
        return;
    static unsigned reads;
    const unsigned n = (unsigned)atoi(at), r = ++reads;
    const char* dir = getenv("ED_COOP_SDA_DUMP");
    const unsigned marks[4] = {n - 100u, n - 40u, n + 60u, n + 120u};
    for (int k = 0; dir != NULL && k < 4; k++)
        if (r == marks[k]) {
            char path[512];
            snprintf(path, sizeof path, "%s/sda%d.bin", dir, k);
            FILE* f = fopen(path, "wb");
            const u32 lo = cpu->gpr[13] - 0x8000u;
            if (f != NULL && guest_ram_range_valid(cpu, lo, 0x10000u)) {
                fwrite(cpu->ram + (lo - GC_RAM_BASE), 1, 0x10000u, f);
                fclose(f);
                fprintf(stderr, "[coop] small data %d written (read %u)\n", k, r);
            }
        }
    if (r != n)
        return;
    const u32 player = coop_player(cpu);
    const u32 o1 = player != 0u ? mem_read32(cpu, player + 52u) : 0u, o2 = mem_read32(cpu, g_coop_p2 + 52u);
    if (!guest_ram_range_valid(cpu, o1, 64u) || !guest_ram_range_valid(cpu, o2, 64u))
        return;
    for (u32 off = 0; off < 60u; off += 4u) {
        if (off >= 12u && off < 44u)
            continue;  // position (0..11) and facing (44..59) only
        const u32 a = mem_read32(cpu, o1 + off), b = mem_read32(cpu, o2 + off);
        mem_write32(cpu, o1 + off, b);
        mem_write32(cpu, o2 + off, a);
    }
    fprintf(stderr, "[coop] the players traded places (read %u)\n", r);
}

// The characters changed in play (Mods, Player 1 / Player 2 character) or a
// quick state made with others: the game is saved where it stands and loaded
// again, so the chapter's load makes each player the chosen one (their body
// borrowed, player 2 made again from the save). The save is the game's own:
// its scratch for the card (0x801703D8), the picture's (0x80170190), the save
// written there (0x800AE098(save, picture, flag), its size back). Then the
// game loads it as the Load Game screen does (0x80022328(2)): the game ends
// (mode 5, its sound and streams stopped), is reset for a load (the chapter
// and room none, so both are loaded whole and the bodies borrowed anew) and
// loads the save from that scratch (mode 9). From play straight to mode 9 the
// chapter came back unseen; the reset from the host waited on the game's
// other threads. Once for each choice, so a load that does not make them does
// not loop.
#define GAME_MODE_PLAY 2u
#define GAME_END 0x80022328u
#define GAME_END_LOAD 2u
#define SAVE_SCRATCH 0x801703D8u
#define SAVE_PICTURE 0x80170190u
#define SAVE_WRITE 0x800AE098u
#define SAVE_MAX 0x4000u

static u32 g_reload_tried = ~0u;  // the choice last reloaded for (a quick load forgets it)

static s32 actor_model(CPUState* cpu, u32 node) {
    const u32 actor = node != 0u ? mem_read32(cpu, node + (u32)(s32)g_coop_actor_offset) : 0u;
    return guest_ram_range_valid(cpu, actor, 160u) ? (s16)mem_read16(cpu, actor + 154u) : -1;
}

static void characters_reload(CPUState* cpu, u32 player) {
    static unsigned settled;  // frames in play since the last reload or load
    if (g_coop_actor_offset == 0 || g_chapter_model < 0 || mem_read32(cpu, cpu->gpr[13] - 26188u) != GAME_MODE_PLAY ||
        coop_busy(cpu) || hle_save_held() || g_character_load.active || g_pack.stage != 0) {
        settled = 0u;
        return;
    }
    // ED_TEST_P1_CHARACTER=frame,choice (testing): Player 1 character changed
    // to choice at that frame, as the in-game menu would.
    static int test_frame = -2, test_choice;
    if (test_frame == -2) {
        const char* t = getenv("ED_TEST_P1_CHARACTER");
        test_frame = t != NULL && sscanf(t, "%d,%d", &test_frame, &test_choice) == 2 ? test_frame : -1;
    }
    if (test_frame >= 0 && cpu->timebase / 675000ull >= (u64)test_frame) {
        g_settings.mod_p1_character = test_choice;
        test_frame = -1;
        fprintf(stderr, "[player] TEST Player 1 character now %d\n", test_choice);
    }
    if (++settled < 60u)
        return;
    const s32 p1 = actor_model(cpu, player);
    const s32 want1 = player_model() >= 0 ? player_model() : g_chapter_model;
    const int want2 = coop_wanted_character();
    bool p2_wrong = false;
    if (g_settings.mod_coop && g_coop_found) {
        if (g_coop_p2 != 0u && coop_listed(cpu, g_coop_p2))
            p2_wrong = actor_model(cpu, g_coop_p2) != (want2 >= 0 ? want2 : p1);
        else
            p2_wrong = want2 >= 0 && !coop_character_loaded(cpu, want2);
    }
    const Costume* const on1 = costume_on(0);
    const Costume* const on2 = costume_on(1);
    const bool wearing1 = on1 != NULL && on1->wearer == want1;
    const bool wearing2 = on2 != NULL && on2->wearer == (want2 >= 0 ? want2 : p1);
    // A cutscene's own list is up for it (its pack has its own few bodies): nobody's other body is to be found there.
    const u32 bodies = mem_read32(cpu, cpu->gpr[13] - 26452u);
    const bool level_list = guest_ram_range_valid(cpu, bodies, 8u) && mem_read32(cpu, bodies + 4u) == 0x6B5u && mem_read32(cpu, bodies) > 112u;
    if (level_list && (costume_worn(cpu, player) != (wearing1 ? on1 : NULL) ||
                       (g_coop_found && g_coop_p2 != 0u && coop_listed(cpu, g_coop_p2) && costume_worn(cpu, g_coop_p2) != (wearing2 ? on2 : NULL))))
        p2_wrong = true;
    u32 choice = ((u32)(want1 & 0x3FFF) << 16) | (u32)(want2 & 0xFFFF) | (wearing1 ? 0x80000000u : 0u) | (wearing2 ? 0x40000000u : 0u);
    choice ^= (wearing1 ? (u32)on1->form << 26 : 0u) ^ (wearing2 ? (u32)on2->form << 12 : 0u);
    if ((p1 == want1 && !p2_wrong) || choice == g_reload_tried)
        return;
    g_reload_tried = choice;
    settled = 0u;
    u32 save = 0u, picture = 0u, size = 0u, ignore = 0u;
    if (!hle_guest_call(cpu, SAVE_SCRATCH, NULL, 0, 0.0, &save) || save == 0u ||
        !hle_guest_call(cpu, SAVE_PICTURE, NULL, 0, 0.0, &picture)) {
        fprintf(stderr, "[player] characters changed, but there is no save scratch to reload through\n");
        return;
    }
    const u32 args[3] = {save, picture, mem_read32(cpu, cpu->gpr[13] - 24244u) == 3u ? 1u : 0u};
    if (!hle_guest_call(cpu, SAVE_WRITE, args, 3, 0.0, &size) || (size & 0xFFFFu) == 0u || (size & 0xFFFFu) > SAVE_MAX) {
        fprintf(stderr, "[player] characters changed, but the save to reload through was not written (%u)\n", size);
        return;
    }
    const u32 load = GAME_END_LOAD;
    hle_guest_call(cpu, GAME_END, &load, 1, 0.0, &ignore);
    fprintf(stderr, "[player] characters changed (player 1 %d, now %d; player 2 %d%s): reloading where they stand\n",
            want1, p1, want2, p2_wrong ? ", not them" : "");
}

// Where player 1 stands and the game's frame, as of the last pad read (photo mode's portraits).
static float g_player_place[3];
static unsigned g_player_place_frame;

bool hle_player_place(float place[3], unsigned* frame) {
    if (g_player_place_frame == 0u)
        return false;
    for (int i = 0; i < 3; i++)
        place[i] = g_player_place[i];
    *frame = g_player_place_frame;
    return true;
}

static void coop_pad_read(CPUState* cpu, bool playing) {
    // ED_MODE_LOG=1 (testing): the game's mode (r13 - 26240) and the one
    // asked for (- 26188), as they change.
    if (getenv("ED_MODE_LOG") != NULL && sda_ready(cpu)) {
        static u32 was = ~0u;
        const u32 now = mem_read32(cpu, cpu->gpr[13] - 26240u) << 8 | mem_read32(cpu, cpu->gpr[13] - 26188u);
        if (now != was)
            fprintf(stderr, "[mode] %d (asked %u) at frame %llu\n", (s32)now >> 8, now & 0xFFu,
                    (unsigned long long)(cpu->timebase / 675000ull));
        was = now;
    }
    if (g_coop_found && sda_ready(cpu) && playing)
        coop_test_swap(cpu);
    if (g_coop_found && sda_ready(cpu)) {
        coop_dump(cpu, playing);
        if (playing)
            coop_snap(cpu);
    }
    if (!g_coop_found || !sda_ready(cpu))
        return;
    if (g_coop_p2 != 0u && !coop_listed(cpu, g_coop_p2)) {
        fprintf(stderr, "[coop] player 2 is gone (an area change?)\n");
        g_coop_p2 = 0u;
        g_coop_attempts = 0u;
        g_coop_p2_state = false;
        g_coop_parked = false;
    }
    if (!playing)
        return;
    const u32 player = coop_player(cpu);
    if (player == 0u)
        return;
    characters_reload(cpu, player);
    costume_moves(cpu, player);
    const u32 standing = mem_read32(cpu, player + 52u);
    if (guest_ram_range_valid(cpu, standing, 12u)) {
        for (u32 i = 0u; i < 3u; i++)
            g_player_place[i] = guest_read_f32(cpu, standing + i * 4u);
        g_player_place_frame = (unsigned)(cpu->timebase / 675000ull);
    }
    if (g_coop_p2 == 0u)
        coop_adopt(cpu, player);
    if (!g_settings.mod_coop) {
        coop_park(cpu, player, true);  // a save made in co-op, played alone
        return;
    }
    coop_park(cpu, player, false);
    if (g_coop_p2 == 0u && g_coop_attempts < 3u &&
        (coop_wanted_character() < 0 || coop_character_loaded(cpu, coop_wanted_character()))) {
        g_coop_attempts++;
        coop_spawn(cpu, player);
    }
    if (g_coop_p2 != 0u) {
        moves_repair(cpu, g_coop_p2, true);
        moves_repair(cpu, player, false);
        costume_moves(cpu, g_coop_p2);
        coop_door_proxy(cpu);
        coop_follow(cpu, player);
        // Up through player 1's look at something and its scene, as theirs is through player 2's.
        coop_prompt_show(cpu, (!coop_busy(cpu) || g_coop_p1_look) && !game_menu_open(cpu) && !cutscene_playing(cpu));
        coop_p1_icon_show(cpu);
    }
    static int log = -1;
    if (log < 0)
        log = getenv("ED_COOP_LOG") != NULL;
    // ED_COOP_LOG=1: both players, once a second.
    const unsigned long long frame = cpu->timebase / 675000ull;
    if (log && g_coop_p2 != 0u && frame % 60u == 0u) {
        coop_describe(cpu, "player 1", player);
        coop_describe(cpu, "player 2", g_coop_p2);
    }
    // ED_COOP_LOG=2: the game's input channels (32-byte records from
    // 0x80588150), once, in play.
    static bool channels;
    if (log && !channels && getenv("ED_COOP_LOG")[0] == '2' && frame % 60u == 30u) {
        channels = true;
        // The characters' commands: a list from the root at r13 - 21524
        // (+36 the first, each +36 the next), each targeting a character by
        // its id (+8; a character's id is its node's +16).
        fprintf(stderr, "[coop] ids: player 1 %d, player 2 %d\n", (s32)mem_read32(cpu, player + 16u),
                g_coop_p2 != 0u ? (s32)mem_read32(cpu, g_coop_p2 + 16u) : -1);
        // Anything of player 1's that points at player 2 (a target, a threat).
        if (g_coop_p2 != 0u) {
            const u32 p2[4] = {g_coop_p2, mem_read32(cpu, g_coop_p2 + 52u),
                               mem_read32(cpu, g_coop_p2 + (u32)(s32)g_coop_actor_offset), 132u};
            const u32 p1[3] = {player, mem_read32(cpu, player + 52u),
                               mem_read32(cpu, player + (u32)(s32)g_coop_actor_offset)};
            static const char* const kWhat[4] = {"node", "object", "actor", "id"};
            for (int b = 0; b < 3; b++)
                for (u32 off = 0; off < 0x2000u; off += 4u) {
                    if (!guest_ram_range_valid(cpu, p1[b] + off, 4u))
                        break;
                    const u32 v = mem_read32(cpu, p1[b] + off);
                    for (int k = 0; k < 3; k++)
                        if (v == p2[k])
                            fprintf(stderr, "[coop] player 1's %s +0x%X holds player 2's %s\n", kWhat[b], off,
                                    kWhat[k]);
                }
        }
        const u32 root = mem_read32(cpu, cpu->gpr[13] - 21524u);
        u32 cmd = guest_ram_range_valid(cpu, root, 40u) ? mem_read32(cpu, root + 36u) : 0u;
        for (int i = 0; i < 64 && guest_ram_range_valid(cpu, cmd, 48u); i++, cmd = mem_read32(cpu, cmd + 36u)) {
            fprintf(stderr, "[coop] command 0x%08X:", cmd);
            for (u32 w = 0; w < 12u; w++)
                fprintf(stderr, " %08X", mem_read32(cpu, cmd + w * 4u));
            fputc('\n', stderr);
        }
        // Where the player's control block (channel 3's context) is pointed
        // to from their node and object, and the same words in player 2's.
        const u32 control = mem_read32(cpu, 0x805881B0u + 12u);
        const u32 bases[2][2] = {{player, mem_read32(cpu, player + 52u)},
                                 {g_coop_p2, g_coop_p2 != 0u ? mem_read32(cpu, g_coop_p2 + 52u) : 0u}};
        for (int b = 0; b < 2; b++)
            for (u32 off = 0; off < 0x1000u; off += 4u)
                if (guest_ram_range_valid(cpu, bases[0][b] + off, 4u) &&
                    mem_read32(cpu, bases[0][b] + off) == control)
                    fprintf(stderr, "[coop] control block 0x%08X at player 1's %s +0x%X; player 2's has 0x%08X\n",
                            control, b == 0 ? "node" : "object", off,
                            bases[1][b] != 0u ? mem_read32(cpu, bases[1][b] + off) : 0u);
        for (u32 c = 0; c < 20u; c++) {
            fprintf(stderr, "[coop] channel %2u at 0x%08X:", c, 0x80588150u + c * 32u);
            for (u32 w = 0; w < 8u; w++)
                fprintf(stderr, " %08X", mem_read32(cpu, 0x80588150u + c * 32u + w * 4u));
            fputc('\n', stderr);
        }
    }
}

// The list and actor (the game's search for the player's character beside
// the health setter, and the stat setter's actor getter), the health (the
// health setter's), and the spawn's calls, as upstream's debug menu finds
// them (PAL only).
static void find_coop(CPUState* cpu) {
    if (strcmp(ED_GAME_ID, "GEDP01") != 0 || g_camera_player_offset == 0) {
        fprintf(stderr, "[coop] not on this disc: no player 2\n");
        return;
    }
    const u32 setter = 0x801F64FCu, stat_fn = 0x800354F8u, search = 0x801F6344u;
    const u32 load = mem_read32(cpu, setter + 0x18u);
    const u32 bl = mem_read32(cpu, setter + 0x24u);
    const u32 set = setter + 0x24u + (u32)(((s32)(bl << 6)) >> 6 & ~3);
    const u32 read = mem_read32(cpu, set + 0x30u);
    const u32 actor_bl = mem_read32(cpu, stat_fn + 0x30u);
    const u32 actor = stat_fn + 0x30u + (u32)(((s32)(actor_bl << 6)) >> 6 & ~3);
    const u32 list_bl = mem_read32(cpu, search + 0x14u);
    const u32 list = search + 0x14u + (u32)(((s32)(list_bl << 6)) >> 6 & ~3);
    const u32 head = mem_read32(cpu, list + 0x0Cu);
    const u32 room = mem_read32(cpu, COOP_SPAWN_INIT + 0x44u);
    if (mem_read32(cpu, setter) != 0x9421FFF0u || (load >> 16) != 0x8063u || (bl & 0xFC000003u) != 0x48000001u ||
        (read >> 16) != 0xA87Eu || (actor_bl & 0xFC000003u) != 0x48000001u ||
        (mem_read32(cpu, actor) >> 16) != 0x8063u || mem_read32(cpu, actor + 4u) != 0x4E800020u ||
        (list_bl & 0xFC000003u) != 0x48000001u || mem_read32(cpu, search + 0x20u) != 0x80030034u ||
        (head >> 16) != 0x806Du || mem_read32(cpu, list + 0x10u) != 0x4E800020u ||
        mem_read32(cpu, COOP_SPAWN_INIT) != 0x9421FFF0u || mem_read32(cpu, COOP_SPAWN_INIT + 0x28u) != 0x907F0010u ||
        (room >> 16) != 0x806Du || mem_read32(cpu, COOP_SPAWN_CREATE) != 0x9421FFE0u ||
        mem_read32(cpu, COOP_SPAWN_SET_ROOM) != 0x9421FFF0u || mem_read32(cpu, COOP_SPAWN_REGISTER) != 0x9421FFF0u) {
        fprintf(stderr, "[coop] the game's characters not found: no player 2\n");
        return;
    }
    g_coop_stats_offset = (s16)(load & 0xFFFFu);
    g_coop_health_offset = (s16)(read & 0xFFFFu);
    g_coop_actor_offset = (s16)(mem_read32(cpu, actor) & 0xFFFFu);
    g_coop_list_offset = (s16)(head & 0xFFFFu);
    g_coop_room_offset = (s16)(room & 0xFFFFu);
    g_coop_p2_is_player = getenv("ED_COOP_P2_NOT_PLAYER") == NULL;
    // Player 2's turn, as the game controls player 1: the controller's start
    // (stwu r1,-160(r1); ... bl id (0x801F62C8)) and the pad routine's
    // (stwu r1,-32(r1); ... bl PADRead).
    HleDispatchEntry* controller = hle_dispatch_entry(COOP_CONTROLLER);
    // The camera's copy (lis r4, 0x8030; addi r5, r4, 0x12E0) checked too.
    if (mem_read32(cpu, COOP_CONTROLLER) == 0x9421FF60u && mem_read32(cpu, COOP_CONTROLLER + 0xA8u) == 0x481B065Du &&
        mem_read32(cpu, 0x801EAFA8u) == 0x3C808030u && mem_read32(cpu, 0x801EAFACu) == 0x38A412E0u &&
        mem_read32(cpu, COOP_PAD_UPDATE) == 0x9421FFE0u && controller != NULL && controller->notify == NULL &&
        controller->intercept == NULL)
        controller->intercept = hle_coop_controller;
    else
        fprintf(stderr, "[coop] the player controller not found: player 2 cannot move\n");
    // An enemy's look for the one it is after (stwu r1,-64(r1); ... bl
    // 0x800324B8 at +0x90) and the characters' answer to events (stwu
    // r1,-256(r1)), for both players being seen.
    HleDispatchEntry* sighting = hle_dispatch_entry(COOP_SIGHTING);
    HleDispatchEntry* events = hle_dispatch_entry(COOP_EVENTS);
    if (mem_read32(cpu, COOP_SIGHTING) == 0x9421FFC0u && mem_read32(cpu, COOP_SIGHTING + 0x90u) == 0x4BFF7261u &&
        mem_read32(cpu, COOP_EVENTS) == 0x9421FF00u && sighting != NULL && sighting->intercept == NULL &&
        sighting->notify == NULL && events != NULL && events->intercept == NULL && events->notify == NULL) {
        sighting->intercept = coop_sighting;
        events->intercept = coop_events;
    } else {
        fprintf(stderr, "[coop] the enemies' look not found: only player 1 is seen\n");
    }
    // Each character's step (stwu r1,-240(r1) ... or r31,r3,r3), and its
    // scan for the player (bl at 0x801335C4), for player 2's own.
    HleDispatchEntry* step = hle_dispatch_entry(COOP_CHARACTER_STEP);
    if (mem_read32(cpu, COOP_CHARACTER_STEP) == 0x9421FF10u && mem_read32(cpu, COOP_CHARACTER_STEP + 16u) == 0x7C7F1B78u &&
        mem_read32(cpu, 0x801335C4u) == 0x48001481u && mem_read32(cpu, 0x80133468u) == 0x4BFF0B09u &&
        mem_read32(cpu, COOP_SCAN) == 0x9421FFB0u && step != NULL && step->intercept == NULL && step->notify == NULL) {
        step->intercept = coop_character_step;
        g_coop_scan = true;
    } else {
        fprintf(stderr, "[coop] the characters' step not found: player 2 finds no doors\n");
    }
    // The prompt's asking (stwu r1,-48(r1)) and its element's making
    // (0x8011708C: lwz r3,-24036(r13) at +0x1C), for player 2's own prompt.
    // The message window's opening (stwu r1,-16(r1); ... stw r31,-25944(r13)
    // at +0x5C) and the examine's call of it (bl at 0x80010134).
    HleDispatchEntry* open = hle_dispatch_entry(MESSAGE_OPEN);
    if (mem_read32(cpu, MESSAGE_OPEN) != 0x9421FFF0u || mem_read32(cpu, MESSAGE_OPEN + 0x5Cu) != 0x93ED9AA8u ||
        mem_read32(cpu, EXAMINE_OPENS - 4u) != 0x480141DDu || open == NULL || open->intercept != NULL ||
        open->notify != NULL)
        fprintf(stderr, "[coop] the message window not found: examine close-ups stay in co-op\n");
    else
        open->intercept = hle_message_open;
    // What is found (stwu r1,-48(r1); mflr r0), gone for both players.
    HleDispatchEntry* found = hle_dispatch_entry(ACQUIRE);
    if (mem_read32(cpu, ACQUIRE) != 0x9421FFD0u || mem_read32(cpu, ACQUIRE + 4u) != 0x7C0802A6u ||
        found == NULL || found->intercept != NULL || found->notify != NULL)
        fprintf(stderr, "[coop] the finding of things not found: what one player takes the other may take again\n");
    else
        found->notify = hle_coop_found;
    HleDispatchEntry* ask = hle_dispatch_entry(PROMPT_ASK);
    if (mem_read32(cpu, PROMPT_ASK) != 0x9421FFD0u || mem_read32(cpu, 0x801170A8u) != 0x806DA21Cu || ask == NULL)
        fprintf(stderr, "[coop] the prompt's asking not found: player 2 has no prompt\n");
    else
        // ED_COOP_OWN_USE=0 (testing): player 2 uses things through player 1
        // again (coop_door_proxy).
        g_coop_use_own = g_coop_scan && (getenv("ED_COOP_OWN_USE") == NULL || atoi(getenv("ED_COOP_OWN_USE")) != 0);
    // A node's frame (stwu r1,-16(r1); ... li r3,3 at +0x18), the look-up by
    // id (... bl 0x801F5C20 at +0x18) and the bars' handler (stwu
    // r1,-32(r1); ... bl 0x801F6300 at +0x34), for player 2's bars.
    HleDispatchEntry* bar_frame = hle_dispatch_entry(COOP_BAR_FRAME);
    HleDispatchEntry* find_node = hle_dispatch_entry(COOP_FIND_NODE);
    if (mem_read32(cpu, COOP_BAR_FRAME) == 0x9421FFF0u && mem_read32(cpu, COOP_BAR_FRAME + 0x18u) == 0x38600003u &&
        mem_read32(cpu, COOP_FIND_NODE) == 0x9421FFF0u && mem_read32(cpu, COOP_FIND_NODE + 0x18u) == 0x4BFFFC81u &&
        mem_read32(cpu, COOP_BAR_HANDLER) == 0x9421FFE0u && mem_read32(cpu, COOP_BAR_HANDLER + 0x34u) == 0x481CCE79u &&
        bar_frame != NULL && bar_frame->notify == NULL && bar_frame->intercept == NULL && find_node != NULL &&
        find_node->notify == NULL && find_node->intercept == NULL) {
        bar_frame->notify = coop_bar_frame;
        find_node->intercept = coop_find_node;
    } else {
        fprintf(stderr, "[coop] the bars not found: player 2 has none\n");
    }
    // A character's most (stwu r1,-32(r1); ... lwz r3,144(r31) at +0x34), for
    // each player their own character's.
    HleDispatchEntry* most = hle_dispatch_entry(STAT_MOST);
    if (mem_read32(cpu, STAT_MOST) == 0x9421FFE0u && mem_read32(cpu, STAT_MOST + 0x34u) == 0x807F0090u &&
        most != NULL && most->intercept == NULL && most->notify == NULL) {
        most->intercept = coop_stat_most;
    } else {
        fprintf(stderr, "[coop] the characters' most not found: chosen characters keep the chapter's stats\n");
    }
    // Whether the player has the Tome (stwu r1,-16(r1); ... bl 0x801F62B8 at
    // +0x10), for either player casting with the other's Tome.
    HleDispatchEntry* tome = hle_dispatch_entry(HAS_TOME);
    if (mem_read32(cpu, HAS_TOME) == 0x9421FFF0u && mem_read32(cpu, HAS_TOME + 0x10u) == 0x480EC841u &&
        tome != NULL && tome->intercept == NULL && tome->notify == NULL) {
        tome->intercept = coop_has_tome;
    } else {
        fprintf(stderr, "[coop] the Tome's check not found: player 2 cannot cast\n");
    }
    // A script's asking for what the player carries (stwu r1,-16(r1) and the
    // script function's bl to it), of both players.
    HleDispatchEntry* item_asked = hle_dispatch_entry(COOP_ITEM_IN_LIST);
    if (mem_read32(cpu, COOP_ITEM_IN_LIST) == 0x9421FFF0u && mem_read32(cpu, COOP_SCRIPT_ITEM_ASKED - 4u) == 0x48090309u &&
        item_asked != NULL && item_asked->intercept == NULL && item_asked->notify == NULL) {
        item_asked->intercept = coop_script_item;
    } else {
        fprintf(stderr, "[coop] a script's asking for items not found: each player's own list answers\n");
    }
    // The layout of the game's words, for the player's own name in them.
    HleDispatchEntry* words_layout = hle_dispatch_entry(WORDS_LAYOUT);
    if ((mem_read32(cpu, WORDS_LAYOUT) >> 16) == 0x9421u && mem_read32(cpu, WORDS_LAYOUT + 4u) == 0x7C0802A6u &&
        words_layout != NULL && words_layout->intercept == NULL && words_layout->notify == NULL) {
        words_layout->intercept = hle_words_layout;
    } else {
        fprintf(stderr, "[coop] the layout of the game's words not found: they name the chapter's character\n");
    }
    // The look for things lying about (stwu r1,-32(r1); ... bl 0x800DA688 at +0x14),
    // player 2's in their own look.
    HleDispatchEntry* thing_look = hle_dispatch_entry(COOP_THING_LOOK);
    if (mem_read32(cpu, COOP_THING_LOOK) == 0x9421FFE0u && mem_read32(cpu, COOP_THING_LOOK + 0x14u) == 0x4801A369u &&
        thing_look != NULL && thing_look->intercept == NULL && thing_look->notify == NULL) {
        thing_look->intercept = coop_thing_look;
    } else {
        fprintf(stderr, "[coop] the look for things lying about not found: player 1 is offered what player 2 stands by\n");
    }
    // The look for the player to talk to them (stwu r1,-64(r1); ... bl 0x801F62B8
    // at +0x18), for player 2 too.
    HleDispatchEntry* talk_check = hle_dispatch_entry(COOP_TALK_CHECK);
    if (mem_read32(cpu, COOP_TALK_CHECK) == 0x9421FFC0u && mem_read32(cpu, COOP_TALK_CHECK + 0x18u) == 0x48164395u &&
        talk_check != NULL && talk_check->intercept == NULL && talk_check->notify == NULL) {
        talk_check->intercept = coop_talk_check;
    } else {
        fprintf(stderr, "[coop] the look for someone to talk to not found: player 2 cannot talk\n");
    }
    // The game's script timers (stwu r1,-16(r1); ... lha r0,0(r31) at +0x20), for
    // player 2's in step with them.
    HleDispatchEntry* script_timers = hle_dispatch_entry(COOP_SCRIPT_TIMERS);
    if (mem_read32(cpu, COOP_SCRIPT_TIMERS) == 0x9421FFF0u && mem_read32(cpu, COOP_SCRIPT_TIMERS + 0x20u) == 0xA81F0000u &&
        script_timers != NULL && script_timers->intercept == NULL && script_timers->notify == NULL) {
        script_timers->intercept = coop_script_timers;
    } else {
        fprintf(stderr, "[coop] the script timers not found: player 2's scripts wait for ever\n");
    }
    // A room's motion (stwu r1,-16(r1); ... bl 0x801F62B8 at +0x14), on whoever
    // opened the way.
    HleDispatchEntry* door_motion = hle_dispatch_entry(COOP_DOOR_MOTION);
    if (mem_read32(cpu, COOP_DOOR_MOTION) == 0x9421FFF0u && mem_read32(cpu, COOP_DOOR_MOTION + 0x14u) == 0x4808F8F9u &&
        door_motion != NULL && door_motion->intercept == NULL && door_motion->notify == NULL) {
        door_motion->intercept = coop_door_motion;
    } else {
        fprintf(stderr, "[coop] a room's motion not found: player 1 goes through every door\n");
    }
    // A script's asking for a room (stwu r1,-112(r1); ... cmpwi r3,7 at +0x24),
    // not heard from player 2's first look in the room they were brought to.
    HleDispatchEntry* go_to_room = hle_dispatch_entry(COOP_SCRIPT_GO_TO_ROOM);
    if (mem_read32(cpu, COOP_SCRIPT_GO_TO_ROOM) == 0x9421FF90u && mem_read32(cpu, COOP_SCRIPT_GO_TO_ROOM + 0x24u) == 0x2C030007u &&
        go_to_room != NULL && go_to_room->intercept == NULL && go_to_room->notify == NULL) {
        go_to_room->intercept = coop_script_go_to_room;
    } else {
        fprintf(stderr, "[coop] a script's asking for a room not found: player 2 may be sent back as they arrive\n");
    }
    // The cast (stwu r1,-160(r1); ... lwz r29,0(r4) at +0x28), for what
    // player 2 casts from their Tome.
    HleDispatchEntry* cast = hle_dispatch_entry(COOP_CAST);
    if (mem_read32(cpu, COOP_CAST) == 0x9421FF60u && mem_read32(cpu, COOP_CAST + 0x28u) == 0x83A40000u && cast != NULL &&
        cast->intercept == NULL && cast->notify == NULL) {
        cast->intercept = coop_cast;
    } else {
        fprintf(stderr, "[coop] the cast not found: what player 2 casts from their Tome is player 1's\n");
    }
    // The world behind the Tome.
    {
        HleDispatchEntry* wait = hle_dispatch_entry(TW_FRAME_WAIT);
        if (wait != NULL && wait->intercept == NULL)
            wait->intercept = tw_frame_wait;
        const u32 retrace = sdk_symbol_address("VIWaitForRetrace");
        HleDispatchEntry* wait2 = retrace != 0u ? hle_dispatch_entry(retrace) : NULL;
        if (wait2 != NULL && wait2->intercept == NULL)
            wait2->intercept = tw_retrace_wait;
        const u32 kept[11] = {0x80125190u, 0x8011F2D4u, 0x80043AB8u, 0x801104D4u, 0x80110C24u, 0x80152298u, 0x801556A8u,
                             0x801A0F30u, 0x801A05ECu, 0x801A4FCCu, 0x8019E068u};
        HleDispatchEntry* change = hle_dispatch_entry(TW_MODE_CHANGE);
        HleDispatchEntry* start = hle_dispatch_entry(TW_TOME_START);
        HleDispatchEntry* buffer = hle_dispatch_entry(TW_ROOM_BUFFER);
        if (change != NULL && start != NULL && buffer != NULL) {
            change->intercept = tw_mode_change;
            start->intercept = tw_tome_start;
            if (tome_copy_mode() == 2) {
                HleDispatchEntry* cancel = hle_dispatch_entry(COOP_CAST_CANCEL);
                if (cancel != NULL && cancel->intercept == NULL)
                    cancel->intercept = coop_cast_cancel;
            }
            if (getenv("ED_COOP_CAST_TRACE") != NULL) {
                static const u32 kCastEnds[] = {0x801C54C0u, 0x801C4F08u, 0x801C5118u, 0x801C51FCu, 0x801C588Cu, 0x801C5268u, 0x801C72ACu, 0x801C4F48u};
                for (u32 i = 0u; i < sizeof kCastEnds / sizeof kCastEnds[0]; i++) {
                    HleDispatchEntry* e = hle_dispatch_entry(kCastEnds[i]);
                    if (e != NULL && e->notify == NULL)
                        e->notify = notify_cast_trace;
                }
            }
            if (tome_copy_mode() == 2) {
                HleDispatchEntry* to_options = hle_dispatch_entry(0x80101600u);
                HleDispatchEntry* to_journal = hle_dispatch_entry(0x80106FD4u);
                if (to_options != NULL && to_options->intercept == NULL)
                    to_options->intercept = tome_copy_tab;
                if (to_journal != NULL && to_journal->intercept == NULL)
                    to_journal->intercept = tome_copy_tab;
            }
            buffer->intercept = tw_room_buffer;
            for (int i = 0; i < 11; i++) {
                HleDispatchEntry* e = hle_dispatch_entry(kept[i]);
                if (e != NULL && e->intercept == NULL)
                    e->intercept = tw_keep;
            }
        }
    }
    // Who the controlled character is, as an enchant asks (bl at 0x801C6DC8).
    HleDispatchEntry* controlled = hle_dispatch_entry(COOP_CONTROLLED_ID);
    if (mem_read32(cpu, COOP_ENCHANT_ASKS - 4u) == 0x48000000u + ((COOP_CONTROLLED_ID - (COOP_ENCHANT_ASKS - 4u)) & 0x03FFFFFCu) + 1u &&
        controlled != NULL && controlled->intercept == NULL && controlled->notify == NULL) {
        controlled->intercept = coop_controlled_id;
    } else {
        fprintf(stderr, "[coop] an enchant's asking not found: its close-up stops play\n");
    }
    // Holding play, as an enchant starts (bl at 0x801C36B0).
    HleDispatchEntry* hold = hle_dispatch_entry(COOP_PLAY_HOLD);
    if (mem_read32(cpu, COOP_ENCHANT_HOLDS - 4u) == 0x48000000u + ((COOP_PLAY_HOLD - (COOP_ENCHANT_HOLDS - 4u)) & 0x03FFFFFCu) + 1u &&
        hold != NULL && hold->intercept == NULL && hold->notify == NULL) {
        hold->intercept = coop_play_hold;
    } else {
        fprintf(stderr, "[coop] an enchant's holding play not found: it stops play\n");
    }
    // An object's drawing (stwu r1,-736(r1)).
    HleDispatchEntry* object_draw = hle_dispatch_entry(COOP_OBJECT_DRAW);
    if (mem_read32(cpu, COOP_OBJECT_DRAW) == 0x9421FD20u && object_draw != NULL && object_draw->intercept == NULL && object_draw->notify == NULL)
        object_draw->notify = coop_found_object_draw;
    else
        fprintf(stderr, "[coop] an object's drawing not found: what is found shows in both halves\n");
    // A find's hold (stwu r1,-32(r1)).
    HleDispatchEntry* find_hold = hle_dispatch_entry(FIND_HOLD);
    if (mem_read32(cpu, FIND_HOLD) == 0x9421FFE0u && find_hold != NULL && find_hold->intercept == NULL && find_hold->notify == NULL)
        find_hold->intercept = coop_find_hold;
    else
        fprintf(stderr, "[coop] a find's hold not found: finds stop both players\n");
    // Player 1's look at the zones.
    HleDispatchEntry* scan_p1 = hle_dispatch_entry(COOP_SCAN);
    if (scan_p1 != NULL && scan_p1->intercept == NULL && scan_p1->notify == NULL && mem_read32(cpu, COOP_SCAN) == 0x9421FFB0u)
        scan_p1->intercept = coop_scan_player1;
    else
        fprintf(stderr, "[coop] the zones' look not found: player 1 walking can take player 2's prompts\n");
    // A prompt's choosing.
    HleDispatchEntry* prompt_choose = hle_dispatch_entry(COOP_PROMPT_CHOOSE);
    if (prompt_choose != NULL && prompt_choose->intercept == NULL && prompt_choose->notify == NULL && mem_read32(cpu, COOP_PROMPT_CHOOSE) == 0x9421FFD0u)
        prompt_choose->intercept = coop_prompt_choose;
    else
        fprintf(stderr, "[coop] a prompt's choosing not found: player 1 running hides player 2's\n");
    // A prompt's setting and clearing.
    HleDispatchEntry* prompt_set = hle_dispatch_entry(COOP_PROMPT_SET);
    if (prompt_set != NULL && prompt_set->intercept == NULL)
        prompt_set->intercept = coop_prompt_set;
    HleDispatchEntry* prompt_clear = hle_dispatch_entry(PROMPT_CLEAR);
    if (prompt_clear != NULL && prompt_clear->intercept == NULL)
        prompt_clear->intercept = coop_prompt_clear;
    // A script's asking for a look's prompt.
    HleDispatchEntry* script_sends = hle_dispatch_entry(COOP_SCRIPT_SENDS);
    if (script_sends != NULL && script_sends->intercept == NULL && script_sends->notify == NULL && (mem_read32(cpu, COOP_SCRIPT_SENDS) >> 16) == 0x9421u)
        script_sends->notify = coop_script_sends;
    // What a press asks hidden: the game's show flags, kept out of it (coop_show_asked).
    HleDispatchEntry* show_later = hle_dispatch_entry(COOP_SHOW_LATER);
    HleDispatchEntry* show_now = hle_dispatch_entry(COOP_SHOW_NOW);
    HleDispatchEntry* show_main = hle_dispatch_entry(COOP_SHOW_MAIN);
    if (show_later != NULL && show_later->intercept == NULL && show_later->notify == NULL && show_now != NULL && show_now->intercept == NULL && show_now->notify == NULL &&
        show_main != NULL && show_main->intercept == NULL && show_main->notify == NULL && (mem_read32(cpu, COOP_SHOW_LATER) >> 16) == 0x9421u && (mem_read32(cpu, COOP_SHOW_NOW) >> 16) == 0x9421u) {
        show_later->intercept = coop_show_asked;
        show_now->intercept = coop_show_asked;
        show_main->intercept = coop_show_main_asked;
    } else {
        fprintf(stderr, "[coop] the show flags not found: a scene hides player 1 and stops both\n");
    }
    HleDispatchEntry* look_prompt = hle_dispatch_entry(COOP_LOOK_PROMPT);
    if (look_prompt != NULL && look_prompt->intercept == NULL && look_prompt->notify == NULL && (mem_read32(cpu, COOP_LOOK_PROMPT) >> 16) == 0x9421u)
        look_prompt->notify = coop_look_prompt;
    else
        fprintf(stderr, "[coop] a look's prompt not found: player 2 can't go on from a look\n");
    // The busy asking (lwz r3,-23344(r13)).
    HleDispatchEntry* busy_ask = hle_dispatch_entry(COOP_BUSY_ASK);
    if (mem_read32(cpu, COOP_BUSY_ASK) == 0x806DA4D0u && busy_ask != NULL && busy_ask->intercept == NULL && busy_ask->notify == NULL)
        busy_ask->intercept = coop_busy_ask;
    else
        fprintf(stderr, "[coop] the busy asking not found: player 2's close-ups stop player 1\n");
    // The words' hold (stwu r1,-16(r1), and the window's call of it at 0x80024368).
    HleDispatchEntry* words_hold = hle_dispatch_entry(WORDS_HOLD);
    if (mem_read32(cpu, WORDS_HOLD) == 0x9421FFF0u && mem_read32(cpu, WORDS_HOLD_OPENS - 4u) == 0x4801E725u && words_hold != NULL &&
        words_hold->intercept == NULL && words_hold->notify == NULL) {
        words_hold->intercept = coop_words_hold;
    } else {
        fprintf(stderr, "[coop] the words' hold not found: words stop both players\n");
    }
    // An enchanted thing's look (stwu r1,-16(r1); ... or r30,r3,r3 at +0x14).
    HleDispatchEntry* enchanted = hle_dispatch_entry(COOP_ENCHANTED_LOOK);
    if (mem_read32(cpu, COOP_ENCHANTED_LOOK) == 0x9421FFF0u && mem_read32(cpu, COOP_ENCHANTED_LOOK + 0x14u) == 0x7C7E1B78u &&
        enchanted != NULL && enchanted->intercept == NULL && enchanted->notify == NULL) {
        enchanted->intercept = coop_enchanted_look;
    } else {
        fprintf(stderr, "[coop] an enchanted thing's look not found: player 2 may enchant only once\n");
    }
    // A lasting spell's sign following the one tied to it (stwu r1,-96(r1);
    // ... stmw r27,76(r1) at +0x10), player 2's over them.
    HleDispatchEntry* sign = hle_dispatch_entry(COOP_SIGN_FOLLOW);
    if (mem_read32(cpu, COOP_SIGN_FOLLOW) == 0x9421FFA0u && mem_read32(cpu, COOP_SIGN_FOLLOW + 0x10u) == 0xBF61004Cu &&
        sign != NULL && sign->intercept == NULL && sign->notify == NULL) {
        sign->intercept = coop_sign_follow;
    } else {
        fprintf(stderr, "[coop] the spells' signs not found: player 2's show over player 1\n");
    }
    // The world's first thing of a kind (stwu r1,-16(r1); ... or r31,r4,r4 at
    // +0x10), each player's own lasting spell.
    HleDispatchEntry* lasting = hle_dispatch_entry(COOP_FIND_KIND);
    if (mem_read32(cpu, COOP_FIND_KIND) == 0x9421FFF0u && mem_read32(cpu, COOP_FIND_KIND + 0x10u) == 0x7C9F2378u &&
        lasting != NULL && lasting->intercept == NULL && lasting->notify == NULL) {
        lasting->intercept = coop_lasting_find;
    } else {
        fprintf(stderr, "[coop] the things' find not found: one lasting spell for both players\n");
    }
    // The spells' update (stwu r1,-32(r1); ... lwz r30,-22324(r13) at +0x1C),
    // player 2's spells stepped as theirs.
    HleDispatchEntry* spells = hle_dispatch_entry(COOP_SPELLS_UPDATE);
    if (mem_read32(cpu, COOP_SPELLS_UPDATE) == 0x9421FFE0u && mem_read32(cpu, COOP_SPELLS_UPDATE + 0x1Cu) == 0x83CDA8CCu &&
        spells != NULL && spells->intercept == NULL && spells->notify == NULL) {
        spells->intercept = coop_spells_update;
    } else {
        fprintf(stderr, "[coop] the spells' update not found: player 2's spells act on player 1\n");
    }
    // The copy of a position (lwz r5,0(r4); lwz r0,4(r4); stw r5,0(r3)), and
    // the share's call of it (bl at 0x8011CB70).
    {
        HleDispatchEntry* chapter = hle_dispatch_entry(CHAPTER_CHARACTER);
        HleDispatchEntry* create = hle_dispatch_entry(COOP_SPAWN_CREATE);
        if (mem_read32(cpu, CHAPTER_CHARACTER) == 0x28030010u && chapter != NULL && chapter->intercept == NULL &&
            create != NULL && create->intercept == NULL) {
            chapter->intercept = hle_chapter_character;
            create->intercept = hle_spawn_create;
            HleDispatchEntry* body = hle_dispatch_entry(CHAPTER_BODY_LOAD);
            if (body != NULL && body->intercept == NULL)
                body->intercept = hle_chapter_body;
            HleDispatchEntry* play = hle_dispatch_entry(PLAY_ANIMATION);
            if (mem_read32(cpu, PLAY_ANIMATION) == 0x9421FFD0u && play != NULL && play->intercept == NULL)
                play->intercept = hle_play_animation;
            HleDispatchEntry* file = hle_dispatch_entry(COOP_PLAYABLE_LOAD);
            if (mem_read32(cpu, PLAYER_FILE_CALL - 4u) == 0x4812B695u && file != NULL && file->intercept == NULL &&
                file->notify == NULL)
                file->intercept = hle_player_file;
            // The story's change of the player's character (lwz r0,60(r3);
            // stw r4,580(r3); stw r3,-23664(r13)).
            static const u32 kStory[3][2] = {
                {STORY_BODY_SET, 0x8003003Cu}, {STORY_MODEL_SET, 0x90830244u}, {STORY_PLAYER_SET, 0x906DA390u}};
            for (int i = 0; i < 3; i++) {
                HleDispatchEntry* e = hle_dispatch_entry(kStory[i][0]);
                if (mem_read32(cpu, kStory[i][0]) == kStory[i][1] && e != NULL && e->intercept == NULL && e->notify == NULL)
                    e->intercept = story_player_model;
                else
                    fprintf(stderr, "[player] the story's change of character (0x%08X) not found\n", kStory[i][0]);
            }
            HleDispatchEntry* ending = hle_dispatch_entry(ENDING_SEEN);
            if (mem_read32(cpu, ENDING_SEEN) == 0x3C808030u && mem_read32(cpu, 0x800507FCu) == 0x4BFF1995u &&
                ending != NULL && ending->intercept == NULL && ending->notify == NULL)
                ending->intercept = hle_ending_seen;
            HleDispatchEntry* scene_button = hle_dispatch_entry(0x800507DCu);
            if (getenv("ED_SKIP_LOG") != NULL && scene_button != NULL && scene_button->notify == NULL)
                scene_button->notify = notify_scene_button;
            HleDispatchEntry* model = hle_dispatch_entry(OBJECT_MODEL);
            if (mem_read32(cpu, OBJECT_MODEL) == 0x80630244u && model != NULL && model->intercept == NULL)
                model->intercept = hle_object_model;
            HleDispatchEntry* send = hle_dispatch_entry(sdk_symbol_address("OSSendMessage"));
            if (getenv("ED_ROOM_LOG") != NULL && send != NULL && send->notify == NULL)
                send->notify = notify_send_message;
            HleDispatchEntry* room = hle_dispatch_entry(0x80152298u);
            if (getenv("ED_ROOM_LOG") != NULL && room != NULL && room->notify == NULL && room->intercept == NULL)
                room->notify = notify_room_change;
            HleDispatchEntry* ticket = hle_dispatch_entry(0x80155770u);
            if (ticket != NULL && ticket->notify == NULL && ticket->intercept == NULL)
                ticket->notify = notify_room_ticket;
            if (coop_trace()) {
                static const struct {
                    u32 at;
                    void (*fn)(CPUState*);
                } kTrace[] = {{0x80006EBCu, notify_trace_prompt_use}, {0x80024D78u, notify_trace_look_step},
                              {0x801556CCu, notify_trace_busy}, {0x80117404u, notify_trace_prompt_set}};
                for (u32 i = 0u; i < sizeof kTrace / sizeof kTrace[0]; i++) {
                    HleDispatchEntry* entry = hle_dispatch_entry(kTrace[i].at);
                    if (entry != NULL && entry->notify == NULL && entry->intercept == NULL)
                        entry->notify = kTrace[i].fn;
                }
            }
            HleDispatchEntry* later = hle_dispatch_entry(0x80163C6Cu);
            if (getenv("ED_ROOM_LOG") != NULL && later != NULL && later->notify == NULL && later->intercept == NULL)
                later->notify = notify_script_later;
            HleDispatchEntry* script_run = hle_dispatch_entry(COOP_RUN_SCRIPT);
            if (script_run != NULL && script_run->intercept == NULL && script_run->notify == NULL)
                script_run->intercept = coop_run_script;
            HleDispatchEntry* run = hle_dispatch_entry(0x801642D4u);
            if (getenv("ED_ROOM_LOG") != NULL && run != NULL && run->notify == NULL && run->intercept == NULL)
                run->notify = notify_script_run;
            HleDispatchEntry* call = hle_dispatch_entry(0x8015974Cu);
            if ((getenv("ED_ROOM_LOG") != NULL || coop_trace()) && call != NULL && call->notify == NULL && call->intercept == NULL)
                call->notify = notify_script_call;
            HleDispatchEntry* stream = hle_dispatch_entry(0x80154118u);
            if (getenv("ED_STREAM_LOG") != NULL && stream != NULL && stream->notify == NULL)
                stream->notify = notify_stream_read;
            static const u32 kAnimLoads[] = {0x801318C0u, 0x80132ED8u, 0x80133028u, 0x80118F9Cu, 0x800C994Cu, 0x80118D98u, 0x801258BCu};
            for (size_t i = 0; getenv("ED_ANIM_LOG") != NULL && i < 7; i++) {
                HleDispatchEntry* e = hle_dispatch_entry(kAnimLoads[i]);
                if (e != NULL && e->notify == NULL)
                    e->notify = notify_anim_load;
            }
            if (strcmp(ED_GAME_ID, "GEDP01") == 0) {
                HleDispatchEntry* frame = hle_dispatch_entry(CARD_FRAME_DRAW);
                if (frame != NULL && frame->intercept == NULL && frame->notify == NULL)
                    frame->intercept = hle_card_frame;
                // The card screen's step (stwu r1,-96(r1)), seen to pass it
                // over after Jump to Game.
                HleDispatchEntry* screen = hle_dispatch_entry(CARD_SCREEN_LOOK);
                if (mem_read32(cpu, CARD_SCREEN_LOOK) == 0x9421FFA0u && screen != NULL && screen->intercept == NULL &&
                    screen->notify == NULL)
                    screen->notify = notify_card_screen;
            }
            static const u32 kWatch[] = {0x8011F460u, 0x8011FC8Cu, 0x8011D33Cu, 0x8012123Cu, 0x8011D394u};
            for (int i = 0; getenv("ED_COPY_WATCH") != NULL && i < 5; i++) {
                HleDispatchEntry* e = hle_dispatch_entry(kWatch[i]);
                if (e != NULL && e->notify == NULL)
                    e->notify = notify_copy_watch;
            }
            HleDispatchEntry* find = hle_dispatch_entry(FIND_BY_ID);
            if (find != NULL && find->intercept == NULL && find->notify == NULL)
                find->intercept = hle_find_by_id;
            HleDispatchEntry* take = hle_dispatch_entry(EQUIP_TAKE);
            if (take != NULL && take->intercept == NULL && take->notify == NULL)
                take->intercept = coop_equip_take;
            HleDispatchEntry* step = hle_dispatch_entry(CAMERA_STEP);
            if (step != NULL && step->intercept == NULL && step->notify == NULL)
                step->intercept = coop_game_camera;
            HleDispatchEntry* let_go = hle_dispatch_entry(MOVES_LET_GO);
            if (let_go != NULL && let_go->intercept == NULL && let_go->notify == NULL)
                let_go->intercept = hle_moves_let_go;
            HleDispatchEntry* moves = hle_dispatch_entry(MOVES_WANT);
            if (moves != NULL && moves->intercept == NULL && moves->notify == NULL)
                moves->intercept = hle_moves_want;
            HleDispatchEntry* tracks_in = hle_dispatch_entry(MOVES_STREAM);
            if (tracks_in != NULL && tracks_in->intercept == NULL && tracks_in->notify == NULL)
                tracks_in->intercept = hle_tracks_load;
            HleDispatchEntry* by_number = hle_dispatch_entry(PLAY_BY_NUMBER);
            if (by_number != NULL && by_number->intercept == NULL && by_number->notify == NULL)
                by_number->intercept = hle_play_by_number;
            HleDispatchEntry* pack = hle_dispatch_entry(PACK_LOAD);
            if (pack != NULL && pack->intercept == NULL && pack->notify == NULL)
                pack->intercept = hle_pack_load;
        }
    }
    HleDispatchEntry* copy = hle_dispatch_entry(COOP_POSITION_COPY);
    if (mem_read32(cpu, COOP_POSITION_COPY) == 0x80A40000u && mem_read32(cpu, COOP_POSITION_COPY + 4u) == 0x80040004u &&
        mem_read32(cpu, COOP_POSITION_COPY + 8u) == 0x90A30000u && mem_read32(cpu, COOP_DETAIL_CALL - 4u) == 0x4BFFB705u &&
        copy != NULL && copy->intercept == NULL && copy->notify == NULL)
        copy->intercept = coop_position_copy;
    else
        fprintf(stderr, "[coop] the detail share not found: player 2 can be the low-detail model\n");
    // The event call (stwu r1,-48(r1); mflr; stw; li r0,-1) for the threat
    // search, and the search's call of it (bl at 0x8003230C).
    HleDispatchEntry* send = hle_dispatch_entry(COOP_SEND_EVENT);
    if (mem_read32(cpu, COOP_SEND_EVENT) == 0x9421FFD0u && mem_read32(cpu, COOP_SEND_EVENT + 12u) == 0x3800FFFFu &&
        mem_read32(cpu, 0x8003230Cu) == 0x481C36A5u && send != NULL && send->intercept == NULL && send->notify == NULL)
        send->intercept = coop_send_event;
    else
        fprintf(stderr, "[coop] the threat search's question not found: the players square up to each other\n");
    g_coop_found = true;
    fprintf(stderr, "[coop] the player object's pointer is at 0x%08X; the characters' list at 0x%08X\n",
            ED_GUEST_R13 + (u32)(s32)g_camera_player_offset, ED_GUEST_R13 + (u32)(s32)g_coop_list_offset);
}

// The pause menu's flag and render-state depth (game_menu_open), from the one
// setter of the flag (flag = r3 != 0) and the reset of the depth to 0.
static void find_game_menu(CPUState* cpu) {
    const bool pal = strcmp(ED_GAME_ID, "GEDP01") == 0;
    const u32 setter = pal ? 0x800E5A14u : 0x800E7950u;
    const u32 reset = pal ? 0x801ECCE0u : 0x801F8598u;
    static const u32 kSetter[] = {0x7C0300D0u, 0x7C001B78u, 0x54000FFEu};  // neg, or, srwi
    const u32 flag_store = mem_read32(cpu, setter + 12u);  // stw r0, flag(r13)
    const u32 depth_store = mem_read32(cpu, reset + 4u);   // stw r0, depth(r13)
    if (!code_matches(cpu, setter, kSetter, 3) || (flag_store >> 16) != 0x900Du ||
        mem_read32(cpu, setter + 16u) != 0x4E800020u || mem_read32(cpu, reset) != 0x38000000u ||
        (depth_store >> 16) != 0x900Du || mem_read32(cpu, reset + 8u) != 0x4E800020u) {
        fprintf(stderr, "[hle] pause menu flag not found: L/R icons show triggers there too\n");
        return;
    }
    g_menu_flag_offset = (s16)(flag_store & 0xFFFFu);
    g_menu_depth_offset = (s16)(depth_store & 0xFFFFu);
}

// The scene flag and the letterbox bars' height (cutscene_playing): the
// flag's getter (lwz r3, flag(r13); blr), and a function that loads the bars'
// object's address (lis r4; lis r3; addi r4, r4) and sets one of its flags
// (li r0, 1).
static void find_letterbox(CPUState* cpu) {
    const bool pal = strcmp(ED_GAME_ID, "GEDP01") == 0;
    const u32 getter = pal ? 0x8012A89Cu : 0x80131458u;
    const u32 at = pal ? 0x801D41D8u : 0x801DFFD0u;
    const u32 load = mem_read32(cpu, getter);
    const u32 high = mem_read32(cpu, at);
    const u32 low = mem_read32(cpu, at + 8u);
    if ((load >> 16) != 0x806Du || mem_read32(cpu, getter + 4u) != 0x4E800020u ||
        (high >> 16) != 0x3C80u || (mem_read32(cpu, at + 4u) >> 16) != 0x3C60u ||
        (low >> 16) != 0x3884u || mem_read32(cpu, at + 12u) != 0x38000001u) {
        fprintf(stderr, "[hle] letterbox not found: only the movies fast-forward\n");
        return;
    }
    g_scene_flag_offset = (s16)(load & 0xFFFFu);
    g_letterbox_height = (high << 16) + (u32)(s32)(s16)(low & 0xFFFFu) + 0xBCu;
}

// Quick state: co-op and the chosen characters, kept by the host and not in
// the game's memory. Without them a load found player 2 again as a co-op
// save does (moved beside player 1), their copies were no longer theirs (the
// sword floated, equipping made another), and the story was told the chosen
// character, not the chapter's.
typedef struct {
    u32 p2, copies_for;
    struct {
        u32 id, node;
    } copies[32];
    CoopPlayerState state[2];
    struct {
        s32 model, part;
        u32 ready, sets;
    } moves[2];
    s32 chapter_model;
    u8 p2_state, parked, p2_is_player, pad;
} HleCoopState;

size_t hle_coop_state_size(void) {
    return sizeof(HleCoopState);
}

void hle_coop_state_save(void* out) {
    HleCoopState s;
    memset(&s, 0, sizeof s);
    s.p2 = g_coop_p2;
    s.copies_for = g_coop_copies_for;
    memcpy(s.copies, g_coop_copies, sizeof s.copies);
    memcpy(s.state, g_coop_state, sizeof s.state);
    for (int k = 0; k < 2; k++) {
        s.moves[k].model = g_p2_moves[k].model;
        s.moves[k].part = g_p2_moves[k].part;
        s.moves[k].ready = g_p2_moves[k].ready;
        s.moves[k].sets = g_p2_moves[k].sets;
    }
    s.chapter_model = g_chapter_model;
    s.p2_state = g_coop_p2_state;
    s.parked = g_coop_parked;
    s.p2_is_player = g_coop_p2_is_player;
    memcpy(out, &s, sizeof s);
}

// Player 2's quick spells and what they are casting, a state of their own
// (states saved before them keep loading).
typedef struct {
    u32 quick[COOP_QUICKS];
    u32 casting[4];
    u8 own, pad[3];  // 2: quick spells of their own (1: a copy of player 1's, from before they began with none)
} HleCoopMagick;

// Player 2's spell list, a state of its own too.
typedef struct {
    u32 spells[COOP_SPELL_WORDS], runes, have;
} HleCoopSpells;

size_t hle_coop_spells_size(void) {
    return sizeof(HleCoopSpells);
}

void hle_coop_spells_save(void* out) {
    HleCoopSpells s;
    memcpy(s.spells, g_coop_spells, sizeof s.spells);
    s.runes = g_coop_spells_runes;
    s.have = g_coop_spells_have;
    memcpy(out, &s, sizeof s);
}

void hle_coop_spells_load(const void* in) {
    g_coop_spells_have = false;
    if (in == NULL)
        return;
    HleCoopSpells s;
    memcpy(&s, in, sizeof s);
    memcpy(g_coop_spells, s.spells, sizeof g_coop_spells);
    g_coop_spells_runes = s.runes;
    g_coop_spells_have = s.have != 0u;
}

size_t hle_coop_magick_size(void) {
    return sizeof(HleCoopMagick);
}

void hle_coop_magick_save(void* out) {
    HleCoopMagick s;
    memset(&s, 0, sizeof s);
    memcpy(s.quick, g_coop_quick, sizeof s.quick);
    memcpy(s.casting, g_coop_casting, sizeof s.casting);
    s.own = 2u;
    memcpy(out, &s, sizeof s);
}

void hle_coop_magick_load(const void* in) {
    HleCoopMagick s;
    memset(&s, 0, sizeof s);
    if (in != NULL)
        memcpy(&s, in, sizeof s);
    if (s.own != 2u)
        memset(s.quick, 0, sizeof s.quick);
    memcpy(g_coop_quick, s.quick, sizeof g_coop_quick);
    memcpy(g_coop_casting, s.casting, sizeof g_coop_casting);
}

// While player 2's Tome is open the player's object and id are theirs
// (coop_tome_player): a save waits for it to close.
bool hle_save_held(void) {
    return g_coop_tome_p2;
}

// NULL: a state saved before co-op was kept; player 2 is found again as
// after a co-op save.
void hle_coop_state_load(const void* in) {
    g_coop_p1_icon.element = 0u;
    g_coop_p1_icon.id = -1;
    g_coop_show_kept = false;
    g_coop_unseen_out_of = -1;
    g_coop_turn = false;
    g_coop_zones.list = 0u;  // their zones and cameras start again, as in a new room
    g_coop_cameras.lent = false;
    g_reload_tried = ~0u;
    g_coop_tome_p2 = false;
    g_coop_next_cast_p2 = false;
    g_coop_attempts = 0u;
    g_coop_restore.id = g_coop_restore.node = 0u;
    if (in == NULL) {
        g_coop_p2 = 0u;
        g_coop_p2_state = false;
        g_coop_parked = false;
        memset(g_coop_copies, 0, sizeof g_coop_copies);
        g_coop_copies_for = 0u;
        p2_moves_clear();
        return;
    }
    HleCoopState s;
    memcpy(&s, in, sizeof s);
    g_coop_p2 = s.p2;
    g_coop_copies_for = s.copies_for;
    memcpy(g_coop_copies, s.copies, sizeof g_coop_copies);
    memcpy(g_coop_state, s.state, sizeof g_coop_state);
    for (int k = 0; k < 2; k++) {
        g_p2_moves[k].model = s.moves[k].model;
        g_p2_moves[k].part = s.moves[k].part;
        g_p2_moves[k].ready = s.moves[k].ready;
        g_p2_moves[k].sets = s.moves[k].sets;
    }
    g_chapter_model = s.chapter_model;
    g_coop_p2_state = s.p2_state != 0u;
    g_coop_parked = s.parked != 0u;
    g_coop_p2_is_player = s.p2_is_player != 0u;
}
#endif

// PAL: 60 Hz from the start, without the game's 50/60 Hz question. Booting in
// 50 Hz, the game asks at the quote screen when the console's TV takes 60 Hz
// (OSGetEuRgb60Mode, mislabelled OSGetProgressiveMode in the symbol map; the
// SRAM mmio.c serves always says so) and no answer is kept (a flag the game
// carries across its own resets). At that check (the call returning to
// 0x8001C038) this answers 60 as the game does: it keeps the answer and runs
// the game's switch to the 60 Hz render mode in the getter's place. The
// switch waits for the VI, so it runs in the game's thread (a tail call), not
// a host call.
#define PAL_EURGB60_GET 0x8020325Cu
#define PAL_SWITCH_TO_60HZ 0x801E3D6Cu
#define PAL_REFRESH_CHECK 0x8001C034u
static s16 g_refresh_chosen_offset;  // the kept answer, at r13 + this

// At boot, the game's graphics set-up (0x801E41F4, called from its start-up
// with the render mode) gets the 60 Hz mode and the values the start-up
// gives it when the game comes back from its own reset in 60 Hz (0x800185DC:
// the mode, two words and a float from the small data), and the answer is
// kept: the game starts in 60 Hz, with no switch (a flash) at the quote screen.
#define PAL_GRAPHICS_INIT 0x801E41F4u
#define PAL_MODE_50HZ 0x80301248u
#define PAL_MODE_60HZ 0x80301140u
static s16 g_refresh_word_offsets[3];  // -> 0, -> 0x10000, -> the float (r13)
static s16 g_refresh_float_source;     // the float, at r2 + this

static void hle_graphics_init(CPUState* cpu) {
    g_hle_passthrough = true;
    if ((cpu->lr != 0x800186C0u && cpu->lr != 0x800186D8u) || cpu->gpr[3] != PAL_MODE_50HZ)
        return;
    const u32 r13 = cpu->gpr[13];
    cpu->gpr[3] = PAL_MODE_60HZ;
    mem_write32(cpu, r13 + (u32)(s32)g_refresh_word_offsets[0], 0u);
    mem_write32(cpu, r13 + (u32)(s32)g_refresh_word_offsets[1], 0x10000u);
    mem_write32(cpu, r13 + (u32)(s32)g_refresh_word_offsets[2],
                mem_read32(cpu, cpu->gpr[2] + (u32)(s32)g_refresh_float_source));
    mem_write32(cpu, r13 + (u32)(s32)g_refresh_chosen_offset, 1u);
    fprintf(stderr, "[hle] starting in 60 Hz\n");
}

static void hle_eurgb60_get(CPUState* cpu) {
    const u32 chosen = cpu->gpr[13] + (u32)(s32)g_refresh_chosen_offset;
    if (cpu->lr != PAL_REFRESH_CHECK + 4u || mem_read32(cpu, chosen) != 0u) {
        g_hle_passthrough = true;
        return;
    }
    mem_write32(cpu, chosen, 1u);
    fprintf(stderr, "[hle] 60 Hz chosen (the 50/60 Hz question skipped)\n");
    cpu->pc = PAL_SWITCH_TO_60HZ;
    g_hle_tail_call = true;
}

static void find_refresh_prompt(CPUState* cpu) {
    if (strcmp(ED_GAME_ID, "GEDP01") != 0)
        return;
    // bl OSGetEuRgb60Mode at the check, the getter's lbz r0, 17(r31) (the
    // SRAM byte), and the check's lwz r0, chosen(r13) three calls later.
    const u32 call = 0x48000001u | ((PAL_EURGB60_GET - PAL_REFRESH_CHECK) & 0x03FFFFFCu);
    const u32 load = mem_read32(cpu, 0x8001C074u);
    HleDispatchEntry* entry = hle_dispatch_entry(PAL_EURGB60_GET);
    if (mem_read32(cpu, PAL_REFRESH_CHECK) != call || mem_read32(cpu, PAL_EURGB60_GET + 0x44u) != 0x881F0011u ||
        (load >> 16) != 0x800Du || mem_read32(cpu, PAL_SWITCH_TO_60HZ) != 0x9421FFD0u || entry == NULL ||
        entry->intercept != NULL) {
        fprintf(stderr, "[hle] the 50/60 Hz question not found: the game asks it\n");
        return;
    }
    g_refresh_chosen_offset = (s16)(load & 0xFFFFu);
    entry->intercept = hle_eurgb60_get;
    // The return from a reset in 60 Hz: lfs f0, float(r2) ... addi r0, r3,
    // mode ... stw r5, a(r13); stw r4, b(r13); stfs f0, c(r13).
    const u32 lfs = mem_read32(cpu, 0x800185DCu);
    const u32 a = mem_read32(cpu, 0x800185F0u), b = mem_read32(cpu, 0x800185F8u),
              c = mem_read32(cpu, 0x800185FCu);
    HleDispatchEntry* init = hle_dispatch_entry(PAL_GRAPHICS_INIT);
    if ((lfs >> 16) != 0xC002u || mem_read32(cpu, 0x800185ECu) != 0x38031140u ||
        (a >> 16) != 0x90ADu || (b >> 16) != 0x908Du || (c >> 16) != 0xD00Du ||
        mem_read32(cpu, 0x800186BCu) != (0x48000001u | ((PAL_GRAPHICS_INIT - 0x800186BCu) & 0x03FFFFFCu)) ||
        mem_read32(cpu, 0x80018440u) != 0x38A51248u || init == NULL || init->intercept != NULL) {
        fprintf(stderr, "[hle] the game's start-up in 60 Hz not found: it switches at the quote screen\n");
        return;
    }
    g_refresh_float_source = (s16)(lfs & 0xFFFFu);
    g_refresh_word_offsets[0] = (s16)(a & 0xFFFFu);
    g_refresh_word_offsets[1] = (s16)(b & 0xFFFFu);
    g_refresh_word_offsets[2] = (s16)(c & 0xFFFFu);
    init->intercept = hle_graphics_init;
}

static bool callback_pending(u32 address, s32 result) {
    if (g_callback_active && g_irq_callback.pending.address == address &&
        g_irq_callback.pending.result == result)
        return true;
    for (u32 i = 0; i < g_callback_count; i++) {
        const HlePendingCallback* c =
            &g_callback_queue[(g_callback_read + i) % HLE_CALLBACK_QUEUE_CAPACITY];
        if (c->address == address && c->result == result)
            return true;
    }
    return false;
}

// The disc stream slots of DVD_CALLBACK_VISITS: three of 132 bytes, each with
// its DVDFileInfo at +20, "read in flight" at +116 and "close once the read is
// in" at +120; the read's callback finds its slot by the file info. States
// saved before reads waited for their callbacks hold slots marked in flight
// with no read anywhere. Clear the mark: the slot reads again, or closes.
static void unstick_disc_streams(CPUState* cpu) {
    const bool pal = strcmp(ED_GAME_ID, "GEDP01") == 0;
    const u32 callback = pal ? 0x801A294Cu : 0x801AE6B0u;
    const u32 slots = pal ? 0x805DB410u : 0x8060B430u;
    // The callback's start, ending in lis/addi of the slot table.
    const u32 head[] = {0x9421FFF0u, 0x7C0802A6u, 0x90010014u, 0x93E1000Cu, 0x38A00000u,
                        0x3C600000u | ((slots + 0x8000u) >> 16),
                        0x38630000u | (slots & 0xFFFFu)};
    if (!code_matches(cpu, callback, head, 7))
        return;
    for (u32 i = 0; i < 3u; i++) {
        const u32 slot = slots + i * 132u;
        if (mem_read8(cpu, slot) == 0u || mem_read32(cpu, slot + 116u) == 0u ||
            callback_pending(callback, (s32)(slot + 20u)))
            continue;
        mem_write32(cpu, slot + 116u, 0u);
        fprintf(stderr, "[hle] disc stream %u: cleared a stuck read (state from an older build)\n",
                i);
    }
}

void hle_state_loaded(CPUState* cpu) {
    g_reboot_requested = false;
    for (u32 i = 0; i < g_callback_count; i++)
        delay_callback(cpu, (g_callback_read + i) % HLE_CALLBACK_QUEUE_CAPACITY);
    unstick_disc_streams(cpu);
    g_movie_file = 0u;  // a state saved during a movie: the next one sets it
#ifdef ED_AURORA
    g_book.loaded = -1;  // the state's ARAM holds a book of either shape
    g_book.active = false;
#endif
}

void hle_install(CPUState* cpu) {
    g_hle_log = getenv("ED_HLE_LOG") != NULL;
    g_dvd_log = g_hle_log || getenv("ED_DVD_LOG") != NULL;
    g_skip_intro = getenv("ED_SKIP_INTRO") != NULL;
    g_skip_intro_done = false;
    g_skip_intro_armed = false;
    g_skip_intro_memcard_text = 0;
    g_trace_fn_count = 0;
    if (getenv("ED_TRACE_FN") != NULL) {
        char names[512];
        snprintf(names, sizeof names, "%s", getenv("ED_TRACE_FN"));
        for (char* n = strtok(names, ","); n != NULL && g_trace_fn_count < 16u;
             n = strtok(NULL, ","))
            if (sdk_symbol_address(n) != 0u)
                g_trace_fn[g_trace_fn_count++] = sdk_symbol_address(n);
            else if (n[0] == '0' && n[1] == 'x')  // raw guest address
                g_trace_fn[g_trace_fn_count++] = (u32)strtoul(n, NULL, 16);
    }
    g_call_prof_fn = getenv("ED_CALL_PROFILE") != NULL
                         ? sdk_symbol_address(getenv("ED_CALL_PROFILE")) : 0u;
    g_call_prof_vi = sdk_symbol_address("VIFlush");
    g_audio_log = getenv("ED_AUDIO_LOG") != NULL;
    g_graphics_log = getenv("ED_GFX_LOG") != NULL;
    g_movie_log = getenv("ED_MOVIE_LOG") != NULL;
    g_movie_cadence_log = getenv("ED_MOVIE_CADENCE_LOG") != NULL;
    g_card_log = getenv("ED_CARD_LOG") != NULL;
    g_auto_input = getenv("ED_AUTO_INPUT") != NULL;
    g_mash_to_gameplay = getenv("ED_MASH_TO_GAMEPLAY") != NULL;
    g_mash_side_assigned = false;
    g_mash_prematch = false;
    g_mash_route_complete = false;
    memset(&g_ball_physics_probe, 0, sizeof g_ball_physics_probe);
    memset(&g_ball_sor_probe, 0, sizeof g_ball_sor_probe);
    memset(g_physics_body_probes, 0, sizeof g_physics_body_probes);
    g_physics_body_probe_count = 0;
    g_physics_sor_failure_reported = false;
    g_physics_set_failure_reported = false;
    g_auto_skip_card_prompt = getenv("ED_AUTO_SKIP_CARD_PROMPT") != NULL;
    g_state_log = getenv("ED_STATE_LOG") != NULL;
    if (g_mash_to_gameplay) {
        fprintf(stderr,
                "[input] mashing A + Left + Start (8 frames held, 8 released)\n");
    }
#ifdef ED_AURORA
    {
        const char* mav = getenv("ED_MAX_ARRAY_VERTS");
        if (mav) {
            unsigned long v = strtoul(mav, NULL, 0);
            if (v >= 16u && v <= 65536u)
                g_max_array_verts = (u32)v;
        }
    }
#endif
    g_auto_input_last_pulse = ~(u64)0;
    g_auto_skip_card_last_pulse = ~(u64)0;
    g_auto_input_once_sent = ~(u64)0;
    g_auto_input_once_frame = 0;
    g_callback_read = 0;
    g_callback_count = 0;
    g_callback_active = false;
    memset(&g_callback_context, 0, sizeof g_callback_context);
#ifdef ED_AURORA
    memset(&g_display_list_call, 0, sizeof g_display_list_call);
    memset(&g_gx_begin, 0, sizeof g_gx_begin);
#endif
    initialize_hle_dispatch();
    const char* once_frame = getenv("ED_AUTO_INPUT_ONCE");
    if (once_frame != NULL && once_frame[0] != '\0')
        g_auto_input_once_frame = strtoull(once_frame, NULL, 0);
    parse_input_script(getenv("ED_INPUT_SCRIPT"));
#ifdef ED_AURORA
    memset(g_textures, 0, sizeof g_textures);
    memset(g_tluts, 0, sizeof g_tluts);
    memset(g_array_address, 0, sizeof g_array_address);
    memset(g_array_stride, 0, sizeof g_array_stride);
    g_gx_begin_count = 0;
    g_gx_array_count = 0;
    g_frame_array_bytes = 0;
    g_frame_array_calls = 0;
    g_frame_array_bytes_peak = 0;
    g_array_budget_warned = false;
    g_vi_wait_count = 0;
    g_movie_texture_load_count = 0;
    g_movie_cadence_present_count = 0;
    g_movie_cadence_texframe_changes = 0;
    g_movie_cadence_start_time = 0.0;
    g_movie_cadence_last_texframe = -1;
    g_movie_cadence_started = false;
#endif
    cpu->host_call = hle_dispatch;
    install_msl_memory_hle(cpu);
    install_xfb_reader_sync(cpu);
#ifdef ED_AURORA
    find_finish_queue(cpu);
    find_widescreen_option(cpu);
#endif
#ifdef ED_AURORA
    find_game_menu(cpu);
    find_letterbox(cpu);
    find_camera(cpu);
    find_coop(cpu);
#endif
    find_refresh_prompt(cpu);
    publish_codegen_options();
}

// ---------------------------------------------------------------------------
// Quick state (host/quickstate.c)
// ---------------------------------------------------------------------------
// The host halves of guest operations in flight: queued and running completion
// callbacks (with the context a running one returns to), the DVD current
// directory, skip-intro progress, and whether each card is mounted. The cards'
// contents are not part of a state: loading one never rewrites saves.
typedef struct {
    HlePendingCallback callback_queue[HLE_CALLBACK_QUEUE_CAPACITY];
    u32 callback_read;
    u32 callback_count;
    bool callback_active;
    HleSavedContext callback_context;
    u32 irq_stage;
    u32 irq_context;
    HlePendingCallback irq_pending;
    char dvd_cwd[sizeof g_dvd_cwd];
    bool skip_intro_done;
    bool skip_intro_armed;
    bool card_mounted;
    bool card_b_mounted;  // in what was padding, so older states read it as false
} HleQuickState;

size_t hle_state_size(void) {
    return sizeof(HleQuickState);
}

void hle_state_save(void* out) {
    HleQuickState s;
    memset(&s, 0, sizeof s);
    memcpy(s.callback_queue, g_callback_queue, sizeof s.callback_queue);
    s.callback_read = g_callback_read;
    s.callback_count = g_callback_count;
    s.callback_active = g_callback_active;
    memcpy(&s.callback_context, &g_callback_context, sizeof s.callback_context);
    s.irq_stage = g_irq_callback.stage;
    s.irq_context = g_irq_callback.context;
    s.irq_pending = g_irq_callback.pending;
    snprintf(s.dvd_cwd, sizeof s.dvd_cwd, "%s", g_dvd_cwd);
    s.skip_intro_done = g_skip_intro_done;
    s.skip_intro_armed = g_skip_intro_armed;
    s.card_mounted = dol_card_mounted(g_memory_card);
    s.card_b_mounted = dol_card_mounted(g_memory_card_b);
    memcpy(out, &s, sizeof s);
}

static void match_card_mount(DolMemoryCard* card, bool mounted) {
    if (card == NULL || dol_card_mounted(card) == mounted)
        return;
    if (mounted)
        dol_card_mount(card);
    else
        dol_card_unmount(card);
}

void hle_state_load(const void* in) {
    HleQuickState s;
    memcpy(&s, in, sizeof s);
    memcpy(g_callback_queue, s.callback_queue, sizeof g_callback_queue);
    g_callback_read = s.callback_read % HLE_CALLBACK_QUEUE_CAPACITY;
    g_callback_count = s.callback_count <= HLE_CALLBACK_QUEUE_CAPACITY
                           ? s.callback_count
                           : 0u;
    memset(g_callback_wait, 0, sizeof g_callback_wait);
    // Stage 4 is inside __OSReschedule: over (see g_irq_callback), though
    // older builds saved it as still running.
    g_callback_active = s.callback_active && s.irq_stage < 4u;
    memcpy(&g_callback_context, &s.callback_context, sizeof g_callback_context);
    g_irq_callback.stage = s.irq_stage;
    g_irq_callback.context = s.irq_context;
    g_irq_callback.pending = s.irq_pending;
    s.dvd_cwd[sizeof s.dvd_cwd - 1u] = '\0';
    snprintf(g_dvd_cwd, sizeof g_dvd_cwd, "%s",
             s.dvd_cwd[0] == '/' ? s.dvd_cwd : "/");
    g_skip_intro_done = s.skip_intro_done;
    g_skip_intro_armed = s.skip_intro_armed;
    // The guest's CARD library remembers mounting; match the host cards to it.
    match_card_mount(g_memory_card, s.card_mounted);
    match_card_mount(g_memory_card_b, s.card_b_mounted);
}

// The multi-stage GX shortcuts (ED_GX_HLE=1) exist only in the windowed build,
// so their stage state is a section of its own. NULL: the state has none.
#ifdef ED_AURORA
typedef struct {
    HleDisplayListCall display_list_call;
    HleGxBegin gx_begin;
} HleGxQuickState;

size_t hle_gx_state_size(void) {
    return sizeof(HleGxQuickState);
}

void hle_gx_state_save(void* out) {
    HleGxQuickState s;
    memset(&s, 0, sizeof s);
    memcpy(&s.display_list_call, &g_display_list_call, sizeof s.display_list_call);
    memcpy(&s.gx_begin, &g_gx_begin, sizeof s.gx_begin);
    memcpy(out, &s, sizeof s);
}

void hle_gx_state_load(const void* in) {
    HleGxQuickState s;
    if (in != NULL)
        memcpy(&s, in, sizeof s);
    else
        memset(&s, 0, sizeof s);
    g_display_list_call = s.display_list_call;
    g_gx_begin = s.gx_begin;
}
#else
size_t hle_gx_state_size(void) {
    return 0;
}

void hle_gx_state_save(void* out) {
    (void)out;
}

void hle_gx_state_load(const void* in) {
    (void)in;
}
#endif
