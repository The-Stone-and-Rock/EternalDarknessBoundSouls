// Launcher and in-game menu. One RmlUi document (res/ui/menu.rml) whose rows
// are rebuilt from C++ for each screen; selection, value changes and
// navigation are handled here rather than by RmlUi's focus, so the keyboard,
// a controller and the mouse all drive the same rows.
#include "host/frontend.h"

#include "gxruntime/aurora_backend.h"
#include "host/buttons.h"
#include "host/camera.h"
#include "host/controls.h"
#include "host/report.h"
#include "host/settings.h"
#include "host/texpack.h"
#include "host/titlevideo.h"

#include <aurora/gfx.h>
#include <aurora/imgui.h>
#include <aurora/rmlui.hpp>
#include <imgui.h>

#include <RmlUi/Core.h>
#include <xxhash.h>
#include <png.h>
#include <RmlUi_Platform_SDL.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include "host/compat.h"

#ifdef _WIN32
#include <process.h>
#endif

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <map>
#include <atomic>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <vector>

extern "C" {
void quickstate_request_save(const char* path);
void quickstate_request_load(const char* path);
const char* quickstate_boot_path(void);
unsigned int quickstate_file_mem1_size(const char* path);
void hle_set_skip_intro(bool on);
bool hle_start_up_hidden(void);
unsigned hle_pad_reads(void);
bool hle_player_place(float place[3], unsigned* frame);
bool camera_last_view(float eye[3], float target[3]);
void audio_set_muted(bool muted);
void hle_cheat_set(int which, bool on);  // hle.h: HLE_CHEAT_*
bool hle_cheat_on(int which);
int hle_sanity_count(void);
int hle_sanity_number(int i);
const char* hle_sanity_name(int i);
const char* hle_sanity_what(int i);
const char* hle_sanity_result(void);
bool hle_sanity_on(void);
void hle_sanity_play(int number, bool ignore_limits);
void hle_sanity_stop(void);
bool hle_sanity_doors_on(void);
void hle_sanity_doors_set(bool on);
int hle_sanity_doors_level(void);
void hle_sanity_doors_next(void);
}
enum { kCheatMagick = 1, kCheatFull = 2 };  // hle.h's HLE_CHEAT_MAGICK, HLE_CHEAT_FULL

namespace {

// --- Input ------------------------------------------------------------------

// kScrollUp/kScrollDown: the right stick (Page Up/Down on the keyboard), which scrolls a list that scrolls.
enum Button { kUp, kDown, kLeft, kRight, kAccept, kBack, kMenu, kCostume, kPageLeft, kPageRight, kScrollUp, kScrollDown, kButtonCount };

SDL_Gamepad* g_pad = nullptr;

SDL_Gamepad* gamepad() {
    if (g_pad != nullptr && SDL_GamepadConnected(g_pad))
        return g_pad;
    g_pad = nullptr;
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids != nullptr && count > 0)
        g_pad = SDL_OpenGamepad(ids[0]);
    SDL_free(ids);
    return g_pad;
}

bool key(int scancode) { return dol_aurora_key_down(scancode); }

// Whose page the menu is (frontend_open_page): 1 when player 2's own Tome asked for it, so their controller drives it
// (settings pad_slot: the Nth connected, or automatic: the next one player 1 is not using, as controls.cpp picks).
int g_menu_player = 0;
SDL_Gamepad* g_pad2 = nullptr;
SDL_Gamepad* menu_gamepad() {
    if (g_menu_player != 1 || !controls::player_has_controller(1))
        return gamepad();
    const int slot = g_settings.pad_slot[1];
    const int index = slot > 0 ? slot - 1
                               : (!controls::player_has_controller(0) ? 0 : (g_settings.pad_slot[0] <= 1 ? 1 : 0));
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    const SDL_JoystickID want = ids != nullptr && index < count ? ids[index] : 0;
    SDL_free(ids);
    if (g_pad2 != nullptr && (!SDL_GamepadConnected(g_pad2) || SDL_GetGamepadID(g_pad2) != want)) {
        SDL_CloseGamepad(g_pad2);
        g_pad2 = nullptr;
    }
    if (g_pad2 == nullptr && want != 0)
        g_pad2 = SDL_OpenGamepad(want);
    return g_pad2;
}

void read_buttons(bool held[kButtonCount]) {
    SDL_Gamepad* pad = menu_gamepad();
    auto btn = [&](SDL_GamepadButton b) {
        return pad != nullptr && SDL_GetGamepadButton(pad, b);
    };
    const float lx = pad != nullptr ? SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f : 0.f;
    const float ly = pad != nullptr ? SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.f : 0.f;
    // Controller only: the keyboard arrives as key events (below), which also
    // carry the system's key repeat.
    held[kUp] = btn(SDL_GAMEPAD_BUTTON_DPAD_UP) || ly < -0.5f;
    held[kDown] = btn(SDL_GAMEPAD_BUTTON_DPAD_DOWN) || ly > 0.5f;
    held[kLeft] = btn(SDL_GAMEPAD_BUTTON_DPAD_LEFT) || lx < -0.5f;
    held[kRight] = btn(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || lx > 0.5f;
    held[kAccept] = btn(SDL_GAMEPAD_BUTTON_SOUTH);
    held[kBack] = btn(SDL_GAMEPAD_BUTTON_EAST);
    // The GameCube pad has no Back/Select, so the game never sees it.
    held[kMenu] = btn(SDL_GAMEPAD_BUTTON_BACK);
    held[kCostume] = btn(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    held[kPageLeft] = btn(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    held[kPageRight] = btn(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    const float ry = pad != nullptr ? SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.f : 0.f;
    held[kScrollUp] = ry < -0.5f;
    held[kScrollDown] = ry > 0.5f;
}

// Keyboard presses since the last poll, from RmlUi key events.
bool g_key_pressed[kButtonCount] = {};

class KeyListener final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        using namespace Rml::Input;
        if (controls::capturing()) {
            // Rebinding: find the SDL scancode for this key. A key RmlUi does
            // not know (0) is left to the capture's own read of the keyboard,
            // which would otherwise match the first unknown scancode.
            const int id = event.GetParameter<int>("key_identifier", 0);
            for (int sc = SDL_SCANCODE_A; id != 0 && sc < SDL_SCANCODE_COUNT; sc++) {
                const SDL_Keycode code = SDL_GetKeyFromScancode(
                    static_cast<SDL_Scancode>(sc), SDL_KMOD_NONE, false);
                if (code != SDLK_UNKNOWN && RmlSDL::ConvertKey(static_cast<int>(code)) == id) {
                    controls::capture_key(sc);
                    break;
                }
            }
            return;
        }
        static const bool log = std::getenv("ED_FRONTEND_LOG") != nullptr;
        if (log)
            std::fprintf(stderr, "[frontend] key %d phase=%d target=%s current=%s t=%llu\n",
                         event.GetParameter<int>("key_identifier", 0),
                         static_cast<int>(event.GetPhase()),
                         event.GetTargetElement() ? event.GetTargetElement()->GetTagName().c_str() : "-",
                         event.GetCurrentElement() ? event.GetCurrentElement()->GetTagName().c_str() : "-",
                         static_cast<unsigned long long>(SDL_GetTicks()));
        switch (event.GetParameter<int>("key_identifier", 0)) {
        case KI_UP: case KI_W: g_key_pressed[kUp] = true; break;
        case KI_DOWN: case KI_S: g_key_pressed[kDown] = true; break;
        case KI_LEFT: case KI_A: g_key_pressed[kLeft] = true; break;
        case KI_RIGHT: case KI_D: g_key_pressed[kRight] = true; break;
        case KI_RETURN: case KI_NUMPADENTER: case KI_SPACE: case KI_J:
            g_key_pressed[kAccept] = true;
            break;
        case KI_ESCAPE:
            g_key_pressed[kBack] = true;
            g_key_pressed[kMenu] = true;
            break;
        case KI_BACK: case KI_K: g_key_pressed[kBack] = true; break;
        case KI_C: g_key_pressed[kCostume] = true; break;
        case KI_Q: g_key_pressed[kPageLeft] = true; break;
        case KI_E: g_key_pressed[kPageRight] = true; break;
        case KI_PRIOR: g_key_pressed[kScrollUp] = true; break;
        case KI_NEXT: g_key_pressed[kScrollDown] = true; break;
        default: break;
        }
    }
};
KeyListener g_key_listener;

// Presses this frame, with auto-repeat for the direction buttons.
struct InputState {
    bool held[kButtonCount] = {};
    Uint64 since[kButtonCount] = {};
    Uint64 repeat[kButtonCount] = {};

    void poll(bool pressed[kButtonCount]) {
        bool now[kButtonCount];
        read_buttons(now);
        const Uint64 t = SDL_GetTicks();
        for (int b = 0; b < kButtonCount; b++) {
            pressed[b] = false;
            if (now[b] && !held[b]) {
                pressed[b] = true;
                since[b] = t;
                repeat[b] = t + 380;
            } else if (now[b] && b <= kRight && t >= repeat[b]) {
                pressed[b] = true;
                repeat[b] = t + 90;
            } else if (now[b] && (b == kScrollUp || b == kScrollDown) && t >= repeat[b]) {
                pressed[b] = true;  // held, the list keeps going a line at a time
                repeat[b] = t + 160;
            }
            held[b] = now[b];
            if (g_key_pressed[b])
                pressed[b] = true;
            g_key_pressed[b] = false;
        }
    }

    bool any_held() const {
        for (bool h : held)
            if (h)
                return true;
        // Keys the game also reads (Enter is Start, J/K are A/B).
        for (int k : {SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE, SDL_SCANCODE_J,
                      SDL_SCANCODE_K, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_UP,
                      SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT})
            if (key(k))
                return true;
        return false;
    }
};

// --- Discs ------------------------------------------------------------------

struct Disc {
    std::string path;
    std::string id;    // game code + maker, e.g. GEDP01
    std::string name;  // title from the disc header
};

bool read_disc(const std::string& path, Disc* out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr)
        return false;
    unsigned char header[0x400];
    const size_t got = std::fread(header, 1, sizeof header, f);
    std::fclose(f);
    // GameCube discs carry this magic word at 0x1C.
    if (got != sizeof header || header[0x1C] != 0xC2 || header[0x1D] != 0x33 ||
        header[0x1E] != 0x9F || header[0x1F] != 0x3D)
        return false;
    out->path = path;
    out->id.assign(reinterpret_cast<const char*>(header), 6);
    out->name.assign(reinterpret_cast<const char*>(header + 0x20),
                     strnlen(reinterpret_cast<const char*>(header + 0x20), 0x3E0));
    return true;
}

std::vector<Disc> scan_discs(const char* default_iso) {
    std::vector<Disc> discs;
    auto add = [&](const std::string& path) {
        for (const Disc& d : discs)
            if (d.path == path)
                return;
        // Only the disc this build runs (the European disc is the one
        // supported): another image in the folder is not offered at all.
        Disc d;
        if (read_disc(path, &d) && d.id == ed_game_id())
            discs.push_back(d);
    };
    if (default_iso != nullptr && default_iso[0] != '\0')
        add(default_iso);
    for (const char* dir : {"dump", "."}) {
        std::error_code ec;
        std::filesystem::directory_iterator it(dir, ec);
        if (ec)
            continue;
        std::vector<std::string> names;
        for (const auto& e : it) {
            std::string name = e.path().filename().string();
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.size() > 4 && (lower.ends_with(".iso") || lower.ends_with(".gcm")))
                names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        for (const std::string& name : names)
            add(std::string(dir) == "." ? name : std::string(dir) + "/" + name);
    }
    return discs;
}

std::string region_name(const std::string& id) {
    if (id.size() < 4)
        return "?";
    switch (id[3]) {
    case 'E': return "USA";
    case 'P': return "Europe";
    case 'J': return "Japan";
    default: return std::string("region ") + id[3];
    }
}

// --- Menu model -------------------------------------------------------------

enum class Screen { Launcher, Play, States, Slot, SlotDelete, SlotOverwrite, Options, OptionsPage, Graphics, Controls, Mapping, InGame,
                    Problem, Mods, ModsPage, Characters, QuitAsk, Pick };

struct Row {
    std::string label;
    std::string value;           // empty: an action row
    std::string value_rml;       // markup instead of value (button icons)
    bool enabled = true;
    bool browse = false;         // left/right still works while disabled (a slot picker on an empty slot)
    std::string note;            // shown under the menu while selected
    std::function<void(int)> change;  // left/right (-1/+1); null: no value
    std::function<void()> activate;   // A / click; null: same as change(+1)
    std::string section;         // starts a section with this heading
    bool gap = false;            // an ornament before it: a new group
    bool tile = false;           // a character's portrait frame (Character select)
    int card = 0;                // 1 or 2: that player's card on Choose Character
    bool big = false;            // larger than the rows round it (the main menu's Start game)
    bool chosen = false;         // the choice in force (Character select)
    int page = 0;                // 1: the right-hand column of a spread
    bool toggle = false;         // On/Off: drawn as a switch
    int slider = -1;             // 0-100: drawn as a bar, with value beside it
    bool experimental = false;   // "(Experimental, expect glitches)" after its name
};

Rml::ElementDocument* g_doc = nullptr;
ImFont* g_fps_font = nullptr;  // the FPS counter's (frontend_imgui_init)
ImFont* g_tome_font = nullptr;  // the Tome-looking buttons' (frontend_tome_button)
bool g_ui_ready = false;
Screen g_screen = Screen::Launcher;
Screen g_options_return = Screen::Launcher;
std::vector<Row> g_rows;
int g_selected = 0;
bool g_close = false;      // leave the current menu loop
bool g_play = false;       // launcher result
std::vector<Disc> g_discs;
int g_disc = 0;
bool g_disc_on_main = true;  // first run, or the chosen disc is missing
int g_load_slot = 0;         // launcher: boot into this save slot (0: none)
// The anti-aliasing and texture filtering the renderer started with (main.c).
int g_running_anti_aliasing = 0, g_running_anisotropy = 4;
bool g_in_game = false;      // the game is running (menus open over it)
std::string g_running_disc;  // the disc this run plays (the launcher's choice)
int g_argc = 0;
char** g_argv = nullptr;
InputState g_input;

std::string escape(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else out += c;
    }
    return out;
}

std::string on_off(bool v) { return v ? "On" : "Off"; }

// Save state slots: saves/slot1.state to saves/slot100.state. F1-F10 save to the first ten
// (quickstate.c); the menus reach them all, kSlotsShown at a time.
constexpr int kSlotCount = 100;
constexpr int kSlotsShown = 10;
int g_slots_page = 0;

std::string slot_path(int slot) {
    return "saves/slot" + std::to_string(slot) + ".state";
}

bool slot_used(int slot) {
    struct stat st;
    return stat(slot_path(slot).c_str(), &st) == 0;
}

bool disc_playable() {
    return !g_discs.empty() && g_discs[g_disc].id == ed_game_id();
}

std::string g_run_disc;  // the disc this run plays (frontend_set_disc), when it is one this build runs

// The disc to read the menus' pictures from: the launcher's choice, else this run's (a run with no launcher).
std::string picture_disc() {
    return disc_playable() ? g_discs[g_disc].path : g_run_disc;
}

void build_rows();
std::string res_dir();

void cycle(int& value, int count, int dir) {
    value = (value + dir + count) % count;
}

void changed() {
    settings_apply();
}

Row disc_row() {
    Row disc{"Disc"};
    if (g_discs.empty()) {
        disc.value = "None found";
        disc.enabled = false;
        disc.note = std::string("Put your ") + region_name(ed_game_id()) + " disc image (" +
                    ed_game_id() + ", .iso) in the dump folder.";
        return disc;
    }
    const Disc& d = g_discs[g_disc];
    disc.value = d.name + " (" + region_name(d.id) + ")";
    disc.note = d.path + "  |  " + d.id;
    if (d.id != ed_game_id())
        disc.note = d.id + ": this build runs " + std::string(ed_game_id()) + " only.";
    if (g_discs.size() > 1)
        disc.change = [](int dir) {
            cycle(g_disc, static_cast<int>(g_discs.size()), dir);
            if (disc_playable()) {
                snprintf(g_settings.disc, sizeof g_settings.disc, "%s",
                         g_discs[g_disc].path.c_str());
                settings_save();
            }
        };
    return disc;
}

// "Today 16:55" or "15 Oct 16:55", for a save slot's file time.
std::string slot_time(int slot) {
    struct stat st;
    if (stat(slot_path(slot).c_str(), &st) != 0)
        return "";
    char text[64];
    const time_t now = time(nullptr);
    struct tm when, today;
    localtime_r(&st.st_mtime, &when);
    localtime_r(&now, &today);
    if (when.tm_year == today.tm_year && when.tm_yday == today.tm_yday)
        strftime(text, sizeof text, "Today %H:%M", &when);
    else
        strftime(text, sizeof text, "%d %b %H:%M", &when);
    return text;
}

int newest_slot() {
    int best = 0;
    time_t best_time = 0;
    for (int slot = 1; slot <= kSlotCount; slot++) {
        struct stat st;
        if (stat(slot_path(slot).c_str(), &st) == 0 && st.st_mtime >= best_time) {
            best = slot;
            best_time = st.st_mtime;
        }
    }
    return best;
}

int play_row(int slot);

// The launcher row a page was opened from (-1: none): back on the launcher, the cursor is on it again, not on
// Continue (the user's choice, 2026-11-07). Set in activate_selected, put back in build_rows.
int g_launcher_row = -1;

// The launcher's row the cursor starts on: Main menu (the user's choice, 2026-11-07), after the disc row and Continue
// when they are there; the disc row while no playable disc is chosen.
int launcher_main_row() {
    if (g_disc_on_main && !disc_playable())
        return 0;
    return (g_disc_on_main ? 1 : 0) + (newest_slot() != 0 ? 1 : 0);
}

// Start this program again from the launcher (another disc, or no boot state
// to go back to): the window closes and reopens. With `resume` ("start" or "load", the Solo / Co-op page's pick of the
// other mode: docs/research/solo-coop-picker/plan.md, step 4) it starts again on the title instead, past the launcher, and
// goes on into Start Game or Load Game by itself (main.c, hle.c title_resume_step), on the disc this run plays. Every
// boot choice of this run is dropped first, so the new one makes its own (frontend_title_resume_boot: its title machine
// follows its own rooms). Returns only when the restart failed.
void restart_process(const char* resume = nullptr) {
    settings_save();
    unsetenv("ED_QUICKLOAD");
    unsetenv("ED_SKIP_INTRO");
    unsetenv("ED_TITLE_STATE");
    unsetenv("ED_TITLE_RESUME");
    if (resume != nullptr) {
        setenv("ED_TITLE_RESUME", resume, 1);
        if (!g_run_disc.empty())
            setenv("ED_ISO", g_run_disc.c_str(), 1);  // (main.c takes it over argv's --iso for a resume)
        std::fprintf(stderr, "[frontend] starting again for %s (the same log goes on; disc %s)\n",
                     std::strcmp(resume, "load") == 0 ? "Load Game" : "Start Game",
                     g_run_disc.empty() ? "as given" : g_run_disc.c_str());
    } else {
        std::fprintf(stderr, "[frontend] starting again (the same log goes on)\n");
    }
    report_before_restart();  // the new process's output to the log, not to this one's tee pipe
#ifdef _WIN32
    // The CRT joins _execv's strings with spaces and quotes none of them, so a path with a space ("E:\Eternal Darkness
    // Recomp Fork\...", an exe started from Explorer or a shortcut, a disc path with a space) came back split in two.
    // Each one is quoted as the new process's CRT reads it back (CommandLineToArgvW's rules).
    std::vector<std::string> quoted;
    for (int i = 0; g_argv[i] != nullptr; i++) {
        const std::string a = g_argv[i];
        if (!a.empty() && a.find_first_of(" \t\n\v\"") == std::string::npos) {
            quoted.push_back(a);
            continue;
        }
        std::string q = "\"";
        size_t slashes = 0;
        for (char c : a) {
            if (c == '\\') {
                slashes++;
                continue;
            }
            q.append(c == '"' ? slashes * 2 + 1 : slashes, '\\');
            q += c;
            slashes = 0;
        }
        q.append(slashes * 2, '\\');
        q += '"';
        quoted.push_back(q);
    }
    std::vector<char*> args;
    for (std::string& q : quoted)
        args.push_back(q.data());
    args.push_back(nullptr);
    _execv(g_argv[0], args.data());
#else
    execv(g_argv[0], g_argv);
#endif
    unsetenv("ED_TITLE_RESUME");
    std::fprintf(stderr, "[frontend] restart failed\n");
}

// The machine at the game's title menu (Mods' Skip intro).
constexpr const char* kTitleState = "saves/title.state";
// With the two room slots (ED_ROOM_SLOTS, 56 MB) the title machine is another file: one made without them has no slot 1
// (the layout words are the state's), and the first room then unpacks into a slot at 0 and the game halts on its data check.
static const char* title_state_path() { return getenv("ED_ROOM_SLOTS") != nullptr ? "saves/title-rooms.state" : kTitleState; }

// Co-op separate rooms are co-op's alone (the user, 2026-11-12: "Co op fixes should not apply to solo plays"). play.bat
// asks for them for every run (ED_ROOM_SLOTS=2, ED_COOP_STAY=1); the ask is kept (ED_ROOM_SLOTS_ASKED, ED_COOP_STAY_ASKED,
// carried through restart_process with the environment) and the switches themselves are set only while co-op is chosen,
// and cleared otherwise: solo plays the game's own one room, its memory (48 MB) and title machine, with none of the two-room
// hooks. A solo play with them on froze returning into a room (saves/reports/hang-20261112-025508).
// A launch that names neither switch (the exe started on its own, the Mac's play.sh, a shortcut, a release) is asked for
// both as play.bat asks (the user, 2026-11-15: no build may leave the fixes off): separate rooms are co-op's normal play.
// A launch that names either one keeps what it names (the test scripts' follow mode is ED_ROOM_SLOTS=2 with ED_COOP_STAY
// unset), and a switch given as 0 is off.
extern "C" bool rooms_follow_coop(void) {
    static const char* const kSwitch[2] = {"ED_ROOM_SLOTS", "ED_COOP_STAY"};
    static const char* const kAsked[2] = {"ED_ROOM_SLOTS_ASKED", "ED_COOP_STAY_ASKED"};
    static const char* const kDefault[2] = {"2", "1"};
    const bool was = getenv(kSwitch[0]) != nullptr;
    bool named = false;
    for (int i = 0; i < 2; i++)
        named = named || getenv(kSwitch[i]) != nullptr || getenv(kAsked[i]) != nullptr;
    for (int i = 0; i < 2; i++) {
        if (!named)
            setenv(kAsked[i], kDefault[i], 1);
        if (getenv(kSwitch[i]) != nullptr && getenv(kAsked[i]) == nullptr) {
            const std::string value = getenv(kSwitch[i]);
            setenv(kAsked[i], value.c_str(), 1);
        }
        const char* asked = getenv(kAsked[i]);
        if (g_settings.mod_coop && asked != nullptr && std::strcmp(asked, "0") != 0) {
            const std::string value = asked;
            setenv(kSwitch[i], value.c_str(), 1);
        } else {
            unsetenv(kSwitch[i]);
        }
    }
    const bool now = getenv(kSwitch[0]) != nullptr;
    if (was != now)
        std::fprintf(stderr, "[rooms] separate rooms %s for this run: co-op is %s\n", now ? "on" : "off", g_settings.mod_coop ? "on" : "off");
    return was != now;
}

// Whether co-op would have the separate rooms, as rooms_follow_coop decides it (asked, or the default ask when nothing
// names either switch). Reads only.
static bool rooms_asked_for_coop() {
    const bool named = getenv("ED_ROOM_SLOTS") != nullptr || getenv("ED_COOP_STAY") != nullptr ||
                       getenv("ED_ROOM_SLOTS_ASKED") != nullptr || getenv("ED_COOP_STAY_ASKED") != nullptr;
    if (!named)
        return true;
    const char* asked = getenv("ED_ROOM_SLOTS_ASKED") != nullptr ? getenv("ED_ROOM_SLOTS_ASKED") : getenv("ED_ROOM_SLOTS");
    return asked != nullptr && std::strcmp(asked, "0") != 0;
}

extern "C" bool rooms_would_change(bool coop) {
    return (coop && rooms_asked_for_coop()) != (getenv("ED_ROOM_SLOTS") != nullptr);
}

extern "C" void coop_follow_boot_state(void) {
    const char* state = getenv("ED_QUICKLOAD");
    if (state == nullptr || state[0] == '\0')
        return;
    const unsigned mem1 = quickstate_file_mem1_size(state);
    if (mem1 == 0u) {
        std::fprintf(stderr, "[rooms] %s: no memory size read, co-op as settings.ini says (%s)\n", state,
                     g_settings.mod_coop ? "on" : "off");
        return;
    }
    const bool coop = mem1 == 56u * 1024u * 1024u;
    // (A smaller state plays co-op too when co-op has no separate rooms: ED_ROOM_SLOTS=0.)
    if (coop == g_settings.mod_coop || (!coop && !rooms_asked_for_coop()))
        return;
    settings_run_coop(coop);
    settings_apply();  // (co-op's frame rate)
    std::fprintf(stderr, "[rooms] %s is a %s state (%u MB): this run plays %s (settings.ini keeps mod_coop = %d)\n", state,
                 coop ? "co-op" : "solo", mem1 >> 20, coop ? "co-op" : "solo", coop ? 0 : 1);
}

// main.c, after rooms_follow_coop, in a program started again by a Solo / Co-op pick (ED_TITLE_RESUME, restart_process):
// straight to the title's main menu from this mode's title machine (title_state_path: it follows this run's rooms), when
// it is this run's memory size (56 MB with the separate rooms, 48 without). Else it is made fresh, as Mods' Skip intro
// does the first time: the start-up passed over behind black and the menu saved as it comes up (an older or smaller
// machine is not loaded: docs/research/solo-coop-picker/plan.md, skeptic's wrong 3). hle.c then presses Start Game or
// Load Game. The switches stay set: hle.c and quickstate read them later (skeptic's wrong 5); restart_process drops them.
extern "C" void frontend_title_resume_boot(void) {
    const char* resume = getenv("ED_TITLE_RESUME");
    if (resume == nullptr || resume[0] == '\0')
        return;
    if (getenv("ED_QUICKLOAD") != nullptr) {
        std::fprintf(stderr, "[title] started again for %s, from the given state %s\n", resume, getenv("ED_QUICKLOAD"));
        return;
    }
    const char* path = title_state_path();
    const unsigned want = (getenv("ED_ROOM_SLOTS") != nullptr ? 56u : 48u) * 1024u * 1024u;  // (cpu.c's memory)
    struct stat st;
    const unsigned mem1 = stat(path, &st) == 0 ? quickstate_file_mem1_size(path) : 0u;
    if (mem1 == want) {
        setenv("ED_QUICKLOAD", path, 1);
        std::fprintf(stderr, "[title] started again for %s: the title from %s\n", resume, path);
    } else {
        setenv("ED_SKIP_INTRO", "1", 1);
        setenv("ED_TITLE_STATE", path, 1);
        std::fprintf(stderr, "[title] started again for %s: %s %s, so the title is made fresh (the start-up passed over)\n",
                     resume, path, mem1 == 0u ? "is not there" : mem1 < want ? "is from a smaller memory" : "is from a larger memory");
    }
}

// The title's Solo / Co-op answer for hle.c (frontend_title_pick_take): -1, or row * 2 + co-op.
static std::atomic<int> g_title_pick_result{-1};
extern "C" int frontend_title_pick_take(void) { return g_title_pick_result.exchange(-1); }

void start_game(int slot) {
    snprintf(g_settings.disc, sizeof g_settings.disc, "%s", g_discs[g_disc].path.c_str());
    settings_save();
    const bool rooms_changed = rooms_follow_coop();  // before the title machine's path below and the game's memory
    if (g_in_game) {
        // The launcher over a running game: load the save, or the machine as
        // it booted for a new game, in place (the same disc only).
        // Co-op turned on or off changes the separate rooms' memory and hooks: the game starts again, as for a disc.
        if ((!g_running_disc.empty() && g_discs[g_disc].path != g_running_disc) || rooms_changed) {
            restart_process();
            return;
        }
        if (slot != 0) {
            quickstate_request_load(slot_path(slot).c_str());
        } else {
            struct stat st;
            const bool title = g_settings.mod_skip_intro && stat(title_state_path(), &st) == 0;
            hle_set_skip_intro(g_settings.mod_skip_intro != 0 && !title);
            quickstate_request_load(title ? title_state_path() : quickstate_boot_path());
        }
        g_close = true;
        return;
    }
    // Mods' Skip intro: straight into the title menu, from the machine saved
    // there (saves/title.state) the first time, when the start-up was passed
    // over and the menu saved as it came up (hle.c, ED_TITLE_STATE).
    if (slot == 0 && g_settings.mod_skip_intro) {
        struct stat st;
        if (stat(title_state_path(), &st) == 0) {
            setenv("ED_QUICKLOAD", title_state_path(), 1);
        } else {
            setenv("ED_SKIP_INTRO", "1", 1);
            setenv("ED_TITLE_STATE", title_state_path(), 1);
        }
    }
    g_load_slot = slot;
    g_play = true;
    g_close = true;
}

// The Mods page: from the launcher, the in-game menu and Options, back to the
// one it came from. Options keeps its own way back (g_options_return).
Screen g_mods_return = Screen::Launcher;
int g_mods_section = 2;  // the Mods page open: 2 Challenge, 3 Characters, 4 Cheats, 5 Sanity Effects (1 was Co-op)
int g_mods_row = 0;      // the Mods row to come back to
// A Mods section's page goes back to the Mods list, or to the launcher when opened from it (Choose
// Character is there too), at that row.
Screen g_mods_page_return = Screen::Mods;
int g_mods_page_return_row = 0;
void mods_page_back();
int section_number(const std::string& heading);
int g_options_row = 0;  // the Options row to come back to (its pages, Controls, Mods)
// Controls: player 1's or player 2's from the Player I / Player II page (Screen::Pick), and back.
Screen g_controls_return = Screen::Options;
int g_controls_return_row = 0;
// The two-card page (docs/research/solo-coop-picker/plan.md): Solo / Co-op before a game (the title's Start Game or
// Load Game, g_pick_title_row 0 or 1), or Player I / Player II before Controls. It goes back to its own screen and row,
// apart from Controls' (which comes back to it, on the card that opened it).
enum class PickKind { Mode, Controls };
PickKind g_pick_kind = PickKind::Controls;
int g_pick_title_row = 0;
// The Solo / Co-op card whose A needs the game started again (the other mode than this run's rooms): its line says so
// while the program starts again in it (g_pick_restart_row: the title row it goes on into, -1 none; live_page_frame).
// -1 none. g_pick_note_failed: the restart did not happen (the line says so, the mode as it was).
int g_pick_note = -1;
bool g_pick_note_failed = false;
int g_pick_restart_row = -1;
Uint64 g_pick_restart_at = 0;
Screen g_pick_return = Screen::Options;
int g_pick_return_row = 0;
void open_pick(PickKind kind, Screen back, int back_row);

// A page opened by the game's own menus (the Tome's Options, the title's Options: frontend_open_page): Back from it,
// or any way up past it, closes the menu and the game's menu is there again as it was.
bool g_root_closes = false;
Screen g_root = Screen::InGame;
// Opened from the title screen (FRONTEND_PAGE_TITLE_*): no game going yet, so no cheats or sanity effects, and a
// character chosen is the one a game starts with.
bool g_on_title = false;
// In a game (not the launcher, not over the title screen).
bool in_play() { return g_in_game && !g_on_title; }

// How the page is drawn (frontend_open_page_skin): the caller's choice, back to the launcher's after the menu closes.
// Each look is a class on <body> (skin-title, skin-tome) and its block in menu.rcss.
enum class Skin { Launcher, Title, Tome };
Skin g_skin = Skin::Launcher;
int g_skin_half = -1;  // the Tome's: its co-op half (0 or 1), -1 the whole picture
// The co-op split while that page is up (Display's "Co-op screen split", 1 one above the other): taken once as the page
// opens, as hle.c gives it to the renderer (aurora_split_page), so a change on the Display page moves nothing until the
// page closes and the renderer and the page keep the same halves.
int g_skin_split = 0;

// A list longer than its page scrolls by hand (RmlUi's own overflow scroll cut the last line): only rows
// g_list_top .. g_list_top + g_list_lines - 1 are drawn, each keeping its row<i> id (the mouse). g_list_lines 0: no
// scroll, every row drawn (the launcher's look). Not the roster (its own lines), the cards or the slot lists (pages).
int g_list_top = 0;
int g_list_lines = 0;

Row mods_row(Screen from) {
    Row mods{"Mods"};
    mods.activate = [from] {
        if (from != Screen::Options)
            g_options_return = from;
        g_mods_return = from;
        g_screen = Screen::Mods;
        g_selected = 0;
    };
    return mods;
}

// The characters, as Jump to Game lists them (settings mod_p1_character 1-12;
// 0 is each chapter's own).
const char* const kCharacters[] = {"Default", "Pius", "Ellia", "Anthony", "Karim",
                                   "Max", "Lindsey", "Paul", "Roberto", "Peter",
                                   "Edward", "Michael", "Alex"};
// Portrait frames four to a line, the details panel beside them; on a Tome page three to a line, the details in a strip
// under them (the page is narrow).
int roster_columns() {
    return g_skin == Skin::Tome ? 3 : 4;
}
constexpr int kRosterLines = 3;    // lines of frames in view; the rest scroll, a line at a time
int g_roster_top = 0;              // the first line in view
int g_characters_return_row = 0;    // the Mods row to come back to
int g_characters_player = 0;        // whose character the page chooses: 0 player 1, 1 player 2

int& chosen_character() {
    return g_characters_player == 1 ? g_settings.mod_p2_character : g_settings.mod_p1_character;
}

// The other bodies, for whoever's page this is: 4 bits for each character in the setting, the
// form, 0 their own. In the order of hle.c's kCostumes for each character.
// On Character select each form has its own frame, right after its character's; the others from the story
// (other = true) come together at the end. A form plays with its character's bones, moves and stats, which the
// details panel shows. The order of each character's forms is hle.c's kCostumes': it's the 4 bits' number.
struct CharacterForm {
    int character;      // kCharacters' place
    const char* name;   // under the frame's name and on the details panel ("" none)
    const char* word;   // after their name on the Mods row
    const char* title;  // the frame's name: nullptr, the character's
    const char* about;  // the details panel's words
    const char* year;   // nullptr: the character's
    bool other;         // one of the others from the story, at the end
};
const CharacterForm kForms[] = {
    {1, "Liche", "liche", nullptr,
     "Bound to an Ancient for two thousand years, Pius endures as an undying liche: the darkness's own champion, set "
     "against every soul who opens the Tome.", nullptr, false},
    {2, "Spirit", "spirit", nullptr,
     "Ellia gave her life fighting the darkness beneath Angkor Thom. Her spirit cannot rest while the Ancient she "
     "defied still stirs.", nullptr, false},
    {2, "Corpse", "corpse", nullptr,
     "Unable to pass on, her spirit stayed in her decaying body for eight centuries, guarding an ancient artifact and "
     "waiting for the one who could carry her knowledge onward.", nullptr, false},
    {2, "", "the Maid", "The Maid",
     "A maid of the Roivas household in Maximillian's day, who saw more of her master's madness than she ever dared "
     "to tell.", "1760 A.D.", true},
    {2, "Spirit", "the Maid's spirit", "The Maid",
     "Long after her service ended, the maid's spirit still drifts through the Roivas halls, murmuring of what she "
     "saw in Maximillian's house.", "2000 A.D.", true},
    {3, "Spirit", "spirit", nullptr,
     "Anthony carried the cursed scroll to his last breath. His spirit still holds the warning he never lived to "
     "deliver.", nullptr, false},
    {3, "Cursed", "cursed", nullptr,
     "The scroll's curse takes hold: Anthony's flesh greys and splits as he presses on toward Charlemagne's court, "
     "racing the rot to deliver his warning.", nullptr, false},
    {3, "Corpse", "corpse", nullptr,
     "The scroll's curse rotted Anthony from within but would not let him die. His ruined body still walks the "
     "cathedral's halls, a servant of what destroyed him.", nullptr, false},
    {4, "Spirit", "spirit", nullptr,
     "Karim fell in the forbidden city, but his vow outlived him. His spirit stands guard there still, waiting for a "
     "worthy hand.", nullptr, false},
    {4, "", "Chandra", "Chandra",
     "Chandra, the woman Karim braved the forbidden city to win. For her love he walked into the dark, and did not "
     "come back.", nullptr, false},
    {4, "Spirit", "Chandra's spirit", "Chandra",
     "Chandra's spirit lingers in the forbidden city, still waiting for the warrior who swore he would return to "
     "her.", nullptr, false},
    {4, "Mutilated", "mutilated Chandra", "Chandra",
     "What the darkness made of Chandra: torn and twisted into a cruel mockery of the woman Karim loved.", nullptr, false},
    {5, "Spirit", "spirit", nullptr,
     "Driven mad by what lay beneath the mansion, Max's restless blue spirit still walks the Roivas halls, keeping "
     "watch over the family he tried to save.", nullptr, false},
    {7, "Spirit", "spirit", nullptr,
     "Paul's faith carried him through the cathedral's horrors. His spirit keeps its vigil there, long after the "
     "last bell rang.", nullptr, false},
    {7, "", "the Bishop", "The Bishop",
     "The Bishop in his red robe, from Anthony's chapter. Behind his blessings lies a darker devotion.", "814 A.D.", false},
    {7, "", "the Custodian", "The Custodian",
     "The grey old Custodian of the bell tower, who has tended the cathedral's bells longer than anyone remembers, and "
     "knows what sleeps beneath them.", nullptr, false},
    {7, "", "black robes", "Black Robes",
     "One of the robed men who gather in the cathedral's shadows, keeping the rites of a god that is not the "
     "church's.", nullptr, false},
    {7, "", "Inspector Legrasse", "Legrasse",
     "Inspector Legrasse of the Rhode Island police, sent to the Roivas mansion to look into Edward's murder, who finds "
     "the case stranger than any before it.", "2000 A.D.", true},
    {8, "Spirit", "spirit", nullptr,
     "Roberto never left the Forbidden City. His spirit wanders the dark halls where he was held, still searching "
     "for a way out.", nullptr, false},
    {10, "Spirit", "spirit", nullptr,
     "Murdered in his own mansion, Edward lingers to guide his granddaughter through the Tome he spent his life "
     "studying.", nullptr, false},
    {10, "", "the Servant", "The Servant",
     "A servant of the Roivas mansion in Maximillian's day, loyal to his master even as the house filled with "
     "whispers.", "1760 A.D.", true},
    {10, "", "the Footman", "The Footman",
     "A footman of the Roivas mansion in Maximillian's day, who kept the doors and the silence of a house with too "
     "many secrets.", "1760 A.D.", true},
};

int forms_of(int character) {
    int n = 0;
    for (const CharacterForm& f : kForms)
        n += f.character == character;
    return n;
}

const CharacterForm* form_row(int character, int form) {
    for (const CharacterForm& f : kForms)
        if (f.character == character && --form == 0)
            return &f;
    return nullptr;
}

// A character's chosen form on a player's page: 0 their own body.
int form_on(int player, int character) {
    if (character < 1 || character > 12)
        return 0;
    const long long bits = player == 1 ? g_settings.mod_p2_costume : g_settings.mod_p1_costume;
    const int form = static_cast<int>((bits >> ((character - 1) * 4)) & 15);
    return form <= forms_of(character) ? form : 0;
}

// A character's form, on this page's player: the same 4 bits as ever, so the game reads it as before.
void form_set(int character, int form) {
    long long& bits = g_characters_player == 1 ? g_settings.mod_p2_costume : g_settings.mod_p1_costume;
    const int shift = (character - 1) * 4;
    bits = (bits & ~(15LL << shift)) | (static_cast<long long>(form) << shift);
}

// Character select's frames: Default (each chapter's own character) first, then each character followed by
// each of their forms, so a form sits beside whose it is.
struct RosterTile {
    int character;  // kCharacters' place
    int form;       // 0 their own body
};
const std::vector<RosterTile>& roster_tiles() {
    static std::vector<RosterTile> tiles;
    if (tiles.empty()) {
        tiles.push_back({0, 0});
        tiles.push_back({SETTINGS_RANDOM, 0});  // Random, beside Default: someone rolled once, kept, never shown
        for (int i = 1; i <= 12; i++)
            for (int form = 0; form <= forms_of(i); form++)
                if (form == 0 || !form_row(i, form)->other)
                    tiles.push_back({i, form});
        for (int i = 1; i <= 12; i++)
            for (int form = 1; form <= forms_of(i); form++)
                if (form_row(i, form)->other)
                    tiles.push_back({i, form});
    }
    return tiles;
}
int roster_count() {
    return static_cast<int>(roster_tiles().size());
}
// The frame of a character in a form (the first frame when there's none).
int roster_index(int character, int form) {
    for (int i = 0; i < roster_count(); i++)
        if (roster_tiles()[i].character == character && roster_tiles()[i].form == form)
            return i;
    return 0;
}

// A frame's name: the form's own (Chandra, The Bishop) or the character's; and the line under it.
const char* tile_title(const RosterTile& t) {
    if (t.character == 0)
        return "Default";
    if (t.character == SETTINGS_RANDOM)
        return "Random";
    const CharacterForm* f = t.form != 0 ? form_row(t.character, t.form) : nullptr;
    return f != nullptr && f->title != nullptr ? f->title : kCharacters[t.character];
}
const char* tile_form_name(const RosterTile& t) {
    if (t.character == 0 || t.character == SETTINGS_RANDOM)
        return "";
    return t.form != 0 ? form_row(t.character, t.form)->name : "";
}

int& chosen_stats() {
    return g_characters_player == 1 ? g_settings.mod_p2_stats : g_settings.mod_p1_stats;
}

// Each character's details (Character select's i): when they lived, who
// they are, and their most health, magick and sanity as their own chapter
// starts (measured in the game: the playing character's record, 0x8030A940
// +68 health, +70 sanity, +72 magick).
struct CharacterInfo {
    const char* year;
    const char* about;
    int health, magick, sanity;
};
const CharacterInfo kCharacterInfo[13] = {
    {"", "", -1, -1, -1},
    {"26 B.C.", "Pius Augustus, a Roman centurion whose search of a forgotten city in Persia sets the darkness in "
                "motion.", 125, 100, 125},
    {"1150 A.D.", "Ellia, a Khmer temple dancer drawn into the hidden chambers beneath Angkor Thom.", 75, 100, 75},
    {"814 A.D.", "Anthony, a messenger to Charlemagne's court, carrying a cursed scroll through a cathedral in France.",
     125, 100, 125},
    {"565 A.D.", "Karim, a Persian swordsman who braves a forbidden city hoping to win his beloved's hand.", 150, 100, 100},
    {"1760 A.D.", "Maximillian Roivas, who uncovers a horror beneath the family mansion in Rhode Island.", 125, 100, 125},
    {"1983 A.D.", "Dr. Edwin Lindsey, an archaeologist exploring the ruins of Angkor Thom.", 125, 100, 125},
    {"1485 A.D.", "Paul Luther, a Franciscan monk on pilgrimage to a cathedral in France.", 75, 125, 100},
    {"1460 A.D.", "Roberto Bianchi, a Venetian architect held prisoner in the Forbidden City of Persia.", 125, 75, 100},
    {"1916 A.D.", "Peter Jacob, a journalist on the Western Front of the First World War.", 100, 125, 75},
    {"1952 A.D.", "Edward Roivas, a clinical psychologist and Alex's grandfather, who studies the Tome in the family "
                  "mansion.", 75, 125, 125},
    {"1991 A.D.", "Michael Edwards, a firefighter serving with the Canadian forces in the Gulf War.", 125, 75, 75},
    {"2000 A.D.", "Alexandra Roivas, who returns to her grandfather's Rhode Island mansion to solve his murder.", 100, 125, 125},
};

// Random's words: who was rolled is never shown, on any page.
#define kRandomAbout "Someone of the twelve, chosen at random when you choose this, and kept from chapter to chapter until you " \
                     "choose again. Choosing Random again chooses someone new. Who it is stays a surprise."
#define kRandomShort "Someone chosen at random, kept until you choose again. Who it is stays a surprise."
#define kRandomStrip "Chosen at random and kept until you choose again: a surprise."  // one line on a Tome page

// Character select's panel beside the frames, the same for every frame: the name, the form in red ink under
// the Tome's ornament, when they lived, who they are, then the meters. A form's are its character's.
std::string character_details_rml(const RosterTile& t) {
    if (t.character == 0)
        return "<div class=\"bio\"><div class=\"bio-name\">Default</div><div class=\"bio-body\"><div class=\"bio-form\">"
               "<div class=\"bio-form-rule\"></div></div><div class=\"bio-about\">Play as the game intended: every chapter "
               "with its own character.</div></div></div>";
    if (t.character == SETTINGS_RANDOM)
        return "<div class=\"bio\"><div class=\"bio-name\">Random</div><div class=\"bio-body\"><div class=\"bio-form\">"
               "<div class=\"bio-form-rule\"></div></div><div class=\"bio-about\">" kRandomAbout "</div></div></div>";
    const CharacterInfo& c = kCharacterInfo[t.character];
    const CharacterForm* f = t.form != 0 ? form_row(t.character, t.form) : nullptr;
    const std::string about = f != nullptr ? f->about : c.about;
    const std::string year = f != nullptr && f->year != nullptr ? f->year : c.year;
    const std::string form = tile_form_name(t);
    // The form in red between the Tome's ornaments; a character's own body has the ornament alone.
    std::string out = "<div class=\"bio\"><div class=\"bio-name\">" + escape(tile_title(t)) +
                      "</div><div class=\"bio-body\"><div class=\"bio-form\"><div class=\"bio-form-rule\"></div>" +
                      (form.empty() ? std::string() : "<div class=\"bio-form-name\">" + escape(form) + "</div><div class=\"bio-form-rule\"></div>") +
                      "</div><div class=\"bio-year\">" + escape(year) + "</div><div class=\"bio-about\">" + escape(about) +
                      "</div><div class=\"meters\">";
    // Bars on one scale (150, the most any has), in the game's own colours.
    const struct {
        const char* name;
        const char* kind;
        int value;
    } meters[] = {{"Health", "health", c.health}, {"Magick", "magick", c.magick}, {"Sanity", "sanity", c.sanity}};
    for (const auto& m : meters) {
        const int width = m.value < 0 ? 0 : std::min(100, m.value * 100 / 150);
        out += std::string("<div class=\"meter\"><span class=\"meter-name\">") + m.name +
               "</span><div class=\"meter-bar\"><div class=\"meter-fill " + m.kind + "\" style=\"width: " +
               std::to_string(width) + "%;\"></div></div><span class=\"meter-value\">" +
               (m.value < 0 ? std::string("?") : std::to_string(m.value)) + "</span></div>";
    }
    return out + "</div></div></div>";
}

// The same on a Tome page, as one strip under the frames: the Tome's rule, the name, the form in red and the year on
// one line, then the three meters side by side (who they are is left out: the page has no room for it).
std::string character_strip_rml(const RosterTile& t) {
    std::string out = "<div class=\"strip\"><div class=\"strip-rule\"></div><div class=\"strip-line\"><span class=\"strip-name\">" +
                      escape(t.character == 0 ? std::string("Default") : tile_title(t)) + "</span>";
    if (t.character == 0)
        return out + "</div><div class=\"strip-about\">Every chapter with its own character, as the game intended.</div></div>";
    if (t.character == SETTINGS_RANDOM)
        return out + "</div><div class=\"strip-about\">" kRandomStrip "</div></div>";
    const CharacterInfo& c = kCharacterInfo[t.character];
    const CharacterForm* f = t.form != 0 ? form_row(t.character, t.form) : nullptr;
    const std::string form = tile_form_name(t);
    const std::string year = f != nullptr && f->year != nullptr ? f->year : c.year;
    if (!form.empty())
        out += "<span class=\"strip-form\">" + escape(form) + "</span>";
    out += "<span class=\"strip-year\">" + escape(year) + "</span></div><div class=\"strip-meters\">";
    const struct {
        const char* name;
        const char* kind;
        int value;
    } meters[] = {{"Health", "health", c.health}, {"Magick", "magick", c.magick}, {"Sanity", "sanity", c.sanity}};
    for (const auto& m : meters) {
        const int width = m.value < 0 ? 0 : std::min(100, m.value * 100 / 150);
        out += std::string("<div class=\"smeter\"><span class=\"smeter-name\">") + m.name +
               "</span><div class=\"meter-bar\"><div class=\"meter-fill " + m.kind + "\" style=\"width: " +
               std::to_string(width) + "%;\"></div></div><span class=\"smeter-value\">" +
               (m.value < 0 ? std::string("?") : std::to_string(m.value)) + "</span></div>";
    }
    return out + "</div></div>";
}

// The page's rows under the frames: Stats, Back.

// The characters' portraits, from the player's own disc: the main menu's
// texture file (UMnMenu.tpl) holds the chapter portraits the save list shows,
// 126x89 CMPR, stored upside down: Alex's (image 50), then the others in
// Jump to Game order (51-61). They are written once as PNGs into
// res/ui/portraits (git ignores it), so no game art is in the repository.
constexpr const char* kPortraitDir = "/ui/portraits/";

std::string portrait_file(int character) {
    return std::string("portraits/") + kCharacters[character] + ".png";
}

// One image's RGBA as a PNG (stored, not compressed: the files are small).
bool write_png(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba) {
    auto be32 = [](std::string& out, uint32_t v) {
        for (int s = 24; s >= 0; s -= 8)
            out += static_cast<char>((v >> s) & 0xFF);
    };
    auto crc = [](const std::string& data) {
        uint32_t c = 0xFFFFFFFFu;
        for (unsigned char b : data) {
            c ^= b;
            for (int k = 0; k < 8; k++)
                c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
        return ~c;
    };
    std::string raw;
    for (int y = 0; y < h; y++) {
        raw += '\0';
        raw.append(reinterpret_cast<const char*>(&rgba[static_cast<size_t>(y) * w * 4]), static_cast<size_t>(w) * 4);
    }
    std::string z = "\x78\x01";
    uint32_t a = 1, b = 0;
    for (unsigned char c : raw) {
        a = (a + c) % 65521u;
        b = (b + a) % 65521u;
    }
    for (size_t at = 0; at < raw.size(); at += 65535) {
        const size_t n = std::min<size_t>(65535, raw.size() - at);
        z += static_cast<char>(at + n == raw.size() ? 1 : 0);
        z += static_cast<char>(n & 0xFF);
        z += static_cast<char>(n >> 8);
        z += static_cast<char>(~n & 0xFF);
        z += static_cast<char>((~n >> 8) & 0xFF);
        z.append(raw, at, n);
    }
    be32(z, (b << 16) | a);
    std::string png = "\x89PNG\r\n\x1a\n";
    auto chunk = [&](const char* type, const std::string& body) {
        be32(png, static_cast<uint32_t>(body.size()));
        const std::string typed = std::string(type) + body;
        png += typed;
        be32(png, crc(typed));
    };
    std::string ihdr;
    be32(ihdr, static_cast<uint32_t>(w));
    be32(ihdr, static_cast<uint32_t>(h));
    ihdr += std::string("\x08\x06\x00\x00\x00", 5);
    chunk("IHDR", ihdr);
    chunk("IDAT", z);
    chunk("IEND", "");
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr)
        return false;
    const bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
    return std::fclose(f) == 0 && ok;
}

// A file of a GameCube disc image, by its path from the root (FST lookup).
std::vector<uint8_t> read_disc_file(const std::string& iso, const char* want) {
    std::vector<uint8_t> out;
    FILE* f = std::fopen(iso.c_str(), "rb");
    if (f == nullptr)
        return out;
    auto u32_at = [](const std::vector<uint8_t>& d, size_t at) {
        return at + 4 <= d.size() ? (uint32_t(d[at]) << 24) | (uint32_t(d[at + 1]) << 16) | (uint32_t(d[at + 2]) << 8) |
                                        d[at + 3]
                                  : 0u;
    };
    std::vector<uint8_t> head(8);
    std::fseek(f, 0x424, SEEK_SET);
    if (std::fread(head.data(), 1, 8, f) == 8) {
        std::vector<uint8_t> fst(u32_at(head, 4));
        std::fseek(f, static_cast<long>(u32_at(head, 0)), SEEK_SET);
        if (!fst.empty() && std::fread(fst.data(), 1, fst.size(), f) == fst.size()) {
            // Only root files are wanted: a directory's entries are skipped.
            const uint32_t count = u32_at(fst, 8);
            const size_t names = static_cast<size_t>(count) * 12;
            for (uint32_t i = 1; i < count && names < fst.size(); i++) {
                const size_t name = names + (u32_at(fst, i * 12) & 0xFFFFFF);
                if (fst[i * 12] != 0) {
                    i = u32_at(fst, i * 12 + 8) - 1;
                    continue;
                }
                if (name < fst.size() && SDL_strcasecmp(reinterpret_cast<const char*>(&fst[name]), want) == 0) {
                    out.resize(u32_at(fst, i * 12 + 8));
                    std::fseek(f, static_cast<long>(u32_at(fst, i * 12 + 4)), SEEK_SET);
                    if (std::fread(out.data(), 1, out.size(), f) != out.size())
                        out.clear();
                    break;
                }
            }
        }
    }
    std::fclose(f);
    return out;
}

// CMPR (S3TC blocks in 8x8 tiles) to RGBA, rows turned the right way up.
std::vector<uint8_t> decode_cmpr(const std::vector<uint8_t>& d, size_t at, int w, int h) {
    std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
    auto rgb = [](uint16_t v, uint8_t* c) {
        c[0] = uint8_t(((v >> 11) & 31) * 255 / 31);
        c[1] = uint8_t(((v >> 5) & 63) * 255 / 63);
        c[2] = uint8_t((v & 31) * 255 / 31);
        c[3] = 255;
    };
    for (int ty = 0; ty < h; ty += 8)
        for (int tx = 0; tx < w; tx += 8)
            for (int sb = 0; sb < 4; sb++, at += 8) {
                if (at + 8 > d.size())
                    return rgba;
                const uint16_t c0 = uint16_t(d[at] << 8 | d[at + 1]), c1 = uint16_t(d[at + 2] << 8 | d[at + 3]);
                uint8_t pal[4][4];
                rgb(c0, pal[0]);
                rgb(c1, pal[1]);
                for (int k = 0; k < 4; k++) {
                    pal[2][k] = uint8_t(c0 > c1 ? (2 * pal[0][k] + pal[1][k]) / 3 : (pal[0][k] + pal[1][k]) / 2);
                    pal[3][k] = uint8_t(c0 > c1 ? (pal[0][k] + 2 * pal[1][k]) / 3 : 0);
                }
                for (int i = 0; i < 16; i++) {
                    const int x = tx + (sb & 1) * 4 + i % 4, y = ty + (sb >> 1) * 4 + i / 4;
                    if (x >= w || y >= h)
                        continue;
                    const int index = (d[at + 4 + i / 4] >> (6 - 2 * (i % 4))) & 3;
                    std::memcpy(&rgba[(static_cast<size_t>(h - 1 - y) * w + x) * 4], pal[index], 4);
                }
            }
    return rgba;
}

// Default's picture: the game's own Tome and skull (UMnMenu.tpl image 26, 640x480 CMPR, the game over scene's picture
// without its logo; XXH64 90abb11e0abc338a), cropped to the portraits' 126x89 shape: x 25-640, y 46-480 upright. Made
// from the disc with the portraits (portraits/Default.png), and from the HD pack when HD textures are on and the pack
// has it (portraits/DefaultHD.png, from its 2560x1920 copy, stamped by the pack's file). emblem.png only when neither
// can be made. Both are made from the player's own disc or pack, so neither is in the repository or the package.
constexpr int kDefaultImage = 26;
constexpr uint64_t kDefaultHash = 0x90abb11e0abc338aull;
constexpr int kDefaultX = 25, kDefaultY = 46, kDefaultW = 615, kDefaultH = 434;
constexpr const char* kDefaultFile = "portraits/Default.png";
constexpr const char* kDefaultHdFile = "portraits/DefaultHD.png";
constexpr const char* kDefaultHdStamp = "portraits/DefaultHD.txt";
constexpr int kDefaultHdVersion = 1;  // raised when the HD picture is made another way: it is made again

// RGBA as a compressed PNG (the HD picture is too large to store plainly).
bool write_png_z(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba) {
    png_image png{};
    png.version = PNG_IMAGE_VERSION;
    png.width = static_cast<png_uint_32>(w);
    png.height = static_cast<png_uint_32>(h);
    png.format = PNG_FORMAT_RGBA;
    return png_image_write_to_file(&png, path.c_str(), 0, rgba.data(), 0, nullptr) != 0;
}

// Default's crop from a k-times picture of the 640x480 one (k 1 the disc's, 4 the pack's), made `by` times smaller.
std::vector<uint8_t> default_crop(const std::vector<uint8_t>& rgba, int k, bool flip, int by, int* out_w, int* out_h) {
    const int w = 640 * k, h = 480 * k, cw = kDefaultW * k / by, ch = kDefaultH * k / by;
    std::vector<uint8_t> out(static_cast<size_t>(cw) * ch * 4);
    for (int y = 0; y < ch; y++)
        for (int x = 0; x < cw; x++)
            for (int c = 0; c < 4; c++) {
                unsigned sum = 0;
                for (int dy = 0; dy < by; dy++)
                    for (int dx = 0; dx < by; dx++) {
                        const int sx = kDefaultX * k + x * by + dx, uy = kDefaultY * k + y * by + dy;
                        const int sy = flip ? h - 1 - uy : uy;
                        sum += rgba[(static_cast<size_t>(sy) * w + sx) * 4 + c];
                    }
                out[(static_cast<size_t>(y) * cw + x) * 4 + c] = static_cast<uint8_t>((sum + by * by / 2) / (by * by));
            }
    *out_w = cw;
    *out_h = ch;
    return out;
}

std::string read_text(const std::string& path);
bool write_text(const std::string& path, const std::string& text);

// Over a live title page (the title and its music run under it) the HD pictures are not made: writing them holds the
// game thread about 300 ms, a gap in the title's music. The disc's pictures show there instead, and the HD ones are made
// at the next page that stops the game (the Tome's, the launcher's) or by the next start's preparing page.
bool g_hd_pictures_waited = false;  // (set when a make was put off: the answer is not kept, so it is asked again)
bool hd_pictures_wait() {
    // (ED_TITLE_PAGE_PAUSE=1: title pages block the game, as title_page_pause reads it, so they are made there.)
    static const bool pause = std::getenv("ED_TITLE_PAGE_PAUSE") != nullptr && std::atoi(std::getenv("ED_TITLE_PAGE_PAUSE")) != 0;
    if (g_skin != Skin::Title || pause)
        return false;
    static bool said = false;
    if (!said) {
        said = true;
        std::fprintf(stderr, "[frontend] HD portraits not made over a live title page (the disc's are shown): made at "
                             "the next page that stops the game, or the next start\n");
    }
    g_hd_pictures_waited = true;
    return true;
}

// The pack's Default picture's stamp: this way of making it, and the pack file's size and time ("" with HD textures
// off or the pack without it).
std::string default_hd_id() {
    if (!g_settings.hd_textures || texpack_count() == 0)
        return "";
    const std::string pack = texpack_file(14, 640, 480, kDefaultHash);
    if (pack.empty())
        return "";
    std::error_code ec;
    const auto size = std::filesystem::file_size(pack, ec);
    const auto time = std::filesystem::last_write_time(pack, ec);
    return "default-hd " + std::to_string(kDefaultHdVersion) + " " + std::to_string(ec ? 0 : size) + " " +
           std::to_string(ec ? 0 : static_cast<long long>(time.time_since_epoch().count())) + " " + pack;
}

// True when the pack's Default picture is there and up to date with the pack (made now when it is not: once a run for
// a pack). False with HD textures off or the pack without it.
bool make_default_hd() {
    const std::string id = default_hd_id();
    if (id.empty())
        return false;
    const std::string png = res_dir() + "/ui/" + kDefaultHdFile, stamp = res_dir() + "/ui/" + kDefaultHdStamp;
    struct stat st;
    const std::string had = read_text(stamp);
    if (stat(png.c_str(), &st) == 0 && had == id + "\n")
        return true;
    // A pack whose picture can't be used is tried once, not at every start: the stamp then says "failed" for this file.
    if (had == "failed " + id + "\n")
        return false;
    static std::string tried;
    if (tried == id || hd_pictures_wait())
        return false;
    tried = id;
    const Uint64 t0 = SDL_GetTicks();
    std::vector<uint8_t> rgba;
    uint32_t w = 0, h = 0;
    const int k = texpack_read_rgba(14, 640, 480, kDefaultHash, &rgba, &w, &h) ? static_cast<int>(w / 640) : 0;
    if (k < 2 || w != 640u * k || h != 480u * k || rgba.size() < static_cast<size_t>(w) * h * 4) {
        std::fprintf(stderr, "[frontend] Default's HD picture NOT made: the pack's is %ux%u, not 2 or more times 640x480\n",
                     w, h);
        std::error_code ec;
        std::filesystem::create_directories(res_dir() + kPortraitDir, ec);
        write_text(stamp, "failed " + id + "\n");
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(res_dir() + kPortraitDir, ec);
    int cw = 0, ch = 0;
    // The pack's copy is stored as the disc's is, bottom row first: turned the right way up. Made half as large (the
    // card is at most about 750 points wide at 4K).
    const std::vector<uint8_t> crop = default_crop(rgba, k, true, k / 2 > 1 ? k / 2 : 1, &cw, &ch);
    if (!write_png_z(png, cw, ch, crop) || !write_text(stamp, id + "\n")) {
        std::fprintf(stderr, "[frontend] Default's HD picture NOT written to %s\n", png.c_str());
        write_text(stamp, "failed " + id + "\n");  // (when the folder can't be written at all, this can't be kept either)
        return false;
    }
    std::fprintf(stderr, "[frontend] Default's HD picture made from the pack (%ux%u, card %dx%d, %llu ms)\n", w, h, cw, ch,
                 static_cast<unsigned long long>(SDL_GetTicks() - t0));
    return true;
}

// The same, answered from the last look while HD textures stay on or off (a page asks again each time it is built).
// The preparing page sets the last look back to -1 when it has removed a stamp, so the picture is looked at again.
int g_default_hd_for = -1;
bool ensure_default_hd() {
    static bool ok = false;
    if (g_default_hd_for != (g_settings.hd_textures ? 1 : 0)) {
        g_hd_pictures_waited = false;
        ok = make_default_hd();
        if (!g_hd_pictures_waited)
            g_default_hd_for = g_settings.hd_textures ? 1 : 0;
    }
    return ok;
}

// The twelve portraits from the HD pack (its 504x356 copies of UMnMenu.tpl images 50-61), when HD textures are on and
// the pack has all twelve: portraits/hd/NAME.png, stamped by the pack's files (portraits/hd/stamp.txt). Each is found
// by its picture's XXH64 on the player's disc (the pack's name for it: 126x89 CMPR, 128x96 of blocks, 6144 bytes).
std::string portrait_hd_file(int character) {
    return std::string("portraits/hd/") + kCharacters[character] + ".png";
}
constexpr int kPortraitsHdVersion = 1;

// The pack's files for the twelve, or empty when it lacks one (or HD textures are off, or there is no disc).
std::vector<std::string> portraits_hd_pack(std::vector<uint64_t>* hashes_out) {
    if (!g_settings.hd_textures || texpack_count() == 0)
        return {};
    static std::vector<uint64_t> hashes;
    static bool hashed = false;
    if (!hashed && !picture_disc().empty()) {
        hashed = true;
        const std::vector<uint8_t> tpl = read_disc_file(picture_disc(), "UMnMenu.tpl");
        auto u32_at = [&](size_t at) {
            return at + 4 <= tpl.size()
                       ? (uint32_t(tpl[at]) << 24) | (uint32_t(tpl[at + 1]) << 16) | (uint32_t(tpl[at + 2]) << 8) | tpl[at + 3]
                       : 0u;
        };
        if (u32_at(0) == 0x0020AF30u && u32_at(4) >= 62u)
            for (int i = 1; i <= 12; i++) {
                const size_t image = u32_at(u32_at(8) + static_cast<size_t>(i == 12 ? 50 : 50 + i) * 8);
                const size_t data = u32_at(image + 8);
                if (u32_at(image) != ((89u << 16) | 126u) || u32_at(image + 4) != 14u || data + 6144 > tpl.size()) {
                    hashes.clear();
                    break;
                }
                hashes.push_back(XXH64(&tpl[data], 6144, 0));
            }
    }
    if (hashes.size() != 12)
        return {};
    std::vector<std::string> files;
    for (uint64_t hash : hashes) {
        files.push_back(texpack_file(14, 126, 89, hash));
        if (files.back().empty())
            return {};
    }
    if (hashes_out != nullptr)
        *hashes_out = hashes;
    return files;
}

// Their stamp: this way of making them and each pack file's size and time ("" when the pack can't give all twelve).
std::string portraits_hd_id() {
    const std::vector<std::string> files = portraits_hd_pack(nullptr);
    if (files.empty())
        return "";
    std::string id = "portraits-hd " + std::to_string(kPortraitsHdVersion);
    for (const std::string& file : files) {
        std::error_code ec;
        const auto size = std::filesystem::file_size(file, ec);
        const auto time = std::filesystem::last_write_time(file, ec);
        id += " " + std::to_string(ec ? 0 : size) + ":" + std::to_string(ec ? 0 : static_cast<long long>(time.time_since_epoch().count()));
    }
    return id;
}

// True when the twelve HD portraits are there and up to date with the pack (made now when not: once a run for a pack).
bool make_portraits_hd() {
    const std::string id = portraits_hd_id();
    if (id.empty())
        return false;
    const std::string stamp = res_dir() + "/ui/portraits/hd/stamp.txt";
    auto have_all = [] {
        for (int i = 1; i <= 12; i++) {
            struct stat st;
            if (stat((res_dir() + "/ui/" + portrait_hd_file(i)).c_str(), &st) != 0)
                return false;
        }
        return true;
    };
    const std::string had = read_text(stamp);
    if (have_all() && had == id + "\n")
        return true;
    if (had == "failed " + id + "\n")
        return false;  // (tried for these pack files already: see make_default_hd)
    static std::string tried;
    if (tried == id || hd_pictures_wait())
        return false;
    tried = id;
    const Uint64 t0 = SDL_GetTicks();
    std::vector<uint64_t> hashes;
    portraits_hd_pack(&hashes);
    std::error_code ec;
    std::filesystem::create_directories(res_dir() + "/ui/portraits/hd", ec);
    std::filesystem::remove(stamp, ec);
    for (int i = 1; i <= 12; i++) {
        std::vector<uint8_t> rgba;
        uint32_t w = 0, h = 0;
        if (!texpack_read_rgba(14, 126, 89, hashes[i - 1], &rgba, &w, &h) || w % 126 != 0 || h != w / 126 * 89 ||
            rgba.size() < static_cast<size_t>(w) * h * 4) {
            std::fprintf(stderr, "[frontend] HD portraits NOT made: the pack's %s is %ux%u, not a whole multiple of 126x89\n",
                         kCharacters[i], w, h);
            write_text(stamp, "failed " + id + "\n");
            return false;
        }
        // Stored as the disc's are, bottom row first: turned the right way up.
        std::vector<uint8_t> upright(rgba.size());
        const size_t row = static_cast<size_t>(w) * 4;
        for (uint32_t y = 0; y < h; y++)
            std::memcpy(&upright[y * row], &rgba[(h - 1 - y) * row], row);
        if (!write_png_z(res_dir() + "/ui/" + portrait_hd_file(i), static_cast<int>(w), static_cast<int>(h), upright)) {
            std::fprintf(stderr, "[frontend] HD portraits NOT written (%s)\n", portrait_hd_file(i).c_str());
            write_text(stamp, "failed " + id + "\n");
            return false;
        }
    }
    write_text(stamp, id + "\n");
    std::fprintf(stderr, "[frontend] HD portraits made from the pack (12, %llu ms)\n",
                 static_cast<unsigned long long>(SDL_GetTicks() - t0));
    return true;
}

// The same, answered from the last look while HD textures stay on or off (a page asks again each time it is built).
// With no disc known yet the answer isn't kept: the pack's portraits are found by the disc's pictures, so it is asked
// again once a disc is chosen.
int g_portraits_hd_for = -1;
bool ensure_portraits_hd() {
    static bool ok = false;
    if (g_portraits_hd_for != (g_settings.hd_textures ? 1 : 0)) {
        g_hd_pictures_waited = false;
        ok = make_portraits_hd();
        if (!picture_disc().empty() && !g_hd_pictures_waited)
            g_portraits_hd_for = g_settings.hd_textures ? 1 : 0;
    }
    return ok;
}

bool have_default_picture() {
    struct stat st;
    return stat((res_dir() + "/ui/" + kDefaultFile).c_str(), &st) == 0;
}

// Writes the portraits that are missing (and Default's picture); true when all twelve are there.
bool ensure_portraits() {
    const std::string dir = res_dir() + kPortraitDir;
    auto have_all = [&] {
        for (int i = 1; i <= 12; i++) {
            struct stat st;
            if (stat((res_dir() + "/ui/" + portrait_file(i)).c_str(), &st) != 0)
                return false;
        }
        return true;
    };
    static bool tried = false;
    if ((have_all() && have_default_picture()) || tried || picture_disc().empty())
        return have_all();
    tried = true;
    const std::vector<uint8_t> tpl = read_disc_file(picture_disc(), "UMnMenu.tpl");
    auto u32_at = [&](size_t at) {
        return at + 4 <= tpl.size()
                   ? (uint32_t(tpl[at]) << 24) | (uint32_t(tpl[at + 1]) << 16) | (uint32_t(tpl[at + 2]) << 8) | tpl[at + 3]
                   : 0u;
    };
    if (u32_at(0) != 0x0020AF30u || u32_at(4) < 62u) {
        std::fprintf(stderr, "[frontend] no portraits: UMnMenu.tpl not found or not as expected\n");
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (!have_default_picture()) {
        const size_t image = u32_at(u32_at(8) + static_cast<size_t>(kDefaultImage) * 8);
        const int h = image + 4 <= tpl.size() ? int(tpl[image] << 8 | tpl[image + 1]) : 0;
        const int w = image + 4 <= tpl.size() ? int(tpl[image + 2] << 8 | tpl[image + 3]) : 0;
        const size_t data = u32_at(image + 8);
        const bool ok = w == 640 && h == 480 && u32_at(image + 4) == 14u && data + 640 * 480 / 2 <= tpl.size() &&
                        XXH64(&tpl[data], 640 * 480 / 2, 0) == kDefaultHash;
        if (ok) {
            int cw = 0, ch = 0;
            const std::vector<uint8_t> crop = default_crop(decode_cmpr(tpl, data, w, h), 1, false, 1, &cw, &ch);
            if (write_png_z(res_dir() + "/ui/" + kDefaultFile, cw, ch, crop))
                std::fprintf(stderr, "[frontend] Default's picture (the Tome and skull, %dx%d) written from the disc\n", cw, ch);
        } else {
            std::fprintf(stderr, "[frontend] Default's picture NOT made: UMnMenu.tpl's image %d is not the Tome and skull "
                                 "(%dx%d format %u): the emblem\n", kDefaultImage, w, h, u32_at(image + 4));
        }
    }
    if (!have_all()) {
        for (int i = 1; i <= 12; i++) {
            const size_t image = u32_at(u32_at(8) + static_cast<size_t>(i == 12 ? 50 : 50 + i) * 8);
            const int h = int(tpl[image] << 8 | tpl[image + 1]), w = int(tpl[image + 2] << 8 | tpl[image + 3]);
            if (u32_at(image + 4) != 14u || w != 126 || h != 89)
                return false;
            write_png(res_dir() + "/ui/" + portrait_file(i), w, h, decode_cmpr(tpl, u32_at(image + 8), w, h));
        }
        std::fprintf(stderr, "[frontend] character portraits written to %s\n", dir.c_str());
    }
    return have_all();
}

// A frame's picture: Default's the game's Tome and skull (else its emblem), Random's the silhouettes (random.png, never
// who was rolled), a form's its own (portraits/Karim-1.png, made by tools/ed/form_portraits.py) when there is one, else
// the character's.
std::string portrait_rml(const RosterTile& t) {
    if (t.character == SETTINGS_RANDOM)
        return "<img src=\"random.png\"/>";
    if (t.character == 0) {
        if (ensure_default_hd())
            return std::string("<img src=\"") + kDefaultHdFile + "\"/>";
        if (have_default_picture() || (ensure_portraits(), have_default_picture()))
            return std::string("<img src=\"") + kDefaultFile + "\"/>";
        return "<img class=\"emblem\" src=\"emblem.png\"/>";
    }
    struct stat st;
    const std::string form_picture = std::string("portraits/") + kCharacters[t.character] + "-" + std::to_string(t.form) + ".png";
    if (t.form != 0 && stat((res_dir() + "/ui/" + form_picture).c_str(), &st) == 0)
        return "<img src=\"" + form_picture + "\"/>";
    // HD textures on and the pack has the twelve: theirs (made once for a pack).
    if (ensure_portraits_hd())
        return "<img src=\"" + portrait_hd_file(t.character) + "\"/>";
    // (Made here when missing, wherever a portrait is shown first: a card, the roster; once a run.)
    if (stat((res_dir() + "/ui/" + portrait_file(t.character)).c_str(), &st) == 0 ||
        (ensure_portraits() && stat((res_dir() + "/ui/" + portrait_file(t.character)).c_str(), &st) == 0))
        return "<img src=\"" + portrait_file(t.character) + "\"/>";
    return "";
}

// Choose Character: a player's card. Their label between the Tome's rules, their chosen frame's picture in
// a leather mount that glows in the player's colour (gold for the first, blue for the second), the name and
// form, the meters, and which stats they play with. Player 2's is dimmed in a solo game only: before a game (the title,
// the launcher) it can be set up for a co-op game, and its foot says where co-op is chosen (Start Game's Co-op).
std::string player_card_rml(int player, bool selected, size_t index) {
    const int chosen = player == 1 ? g_settings.mod_p2_character : g_settings.mod_p1_character;
    const RosterTile t{chosen, form_on(player, chosen)};
    const int stats = player == 1 ? g_settings.mod_p2_stats : g_settings.mod_p1_stats;
    const bool solo = player == 1 && !g_settings.mod_coop;
    const bool off = solo && in_play();
    std::string out = std::string("<div class=\"pcard p") + (player == 1 ? "2" : "1") + (selected ? " selected" : "") +
                      (off ? " off" : "") + "\" id=\"row" + std::to_string(index) + "\"><div class=\"pcard-head\"><div class=\"pcard-rule\"></div>" +
                      "<div class=\"pcard-who\">" + (player == 1 ? "Player II" : "Player I") + "</div><div class=\"pcard-rule\"></div></div>" +
                      "<div class=\"pcard-frame" + (chosen == 0 ? " default" : chosen == SETTINGS_RANDOM ? " random" : "") +
                      "\"><div class=\"pcard-portrait\">" + portrait_rml(t) +
                      "</div></div><div class=\"pcard-name\">" + escape(tile_title(t)) + "</div><div class=\"pcard-form\">" +
                      (tile_form_name(t)[0] != 0 ? escape(tile_form_name(t)) : std::string("&nbsp;")) + "</div>";
    if (chosen == 0) {
        out += "<div class=\"pcard-default\">Each chapter with its own character, as the game intended.</div>";
    } else if (chosen == SETTINGS_RANDOM) {
        out += "<div class=\"pcard-default\">" kRandomShort "</div>";  // (never who was rolled)
    } else {
        const CharacterInfo& c = kCharacterInfo[chosen];
        const struct {
            const char* name;
            const char* kind;
            int value;
        } meters[] = {{"Health", "health", c.health}, {"Magick", "magick", c.magick}, {"Sanity", "sanity", c.sanity}};
        out += "<div class=\"meters pcard-meters\">";
        for (const auto& m : meters)
            out += std::string("<div class=\"meter\"><span class=\"meter-name\">") + m.name +
                   "</span><div class=\"meter-bar\"><div class=\"meter-fill " + m.kind + "\" style=\"width: " +
                   std::to_string(std::min(100, std::max(0, m.value) * 100 / 150)) + "%;\"></div></div><span class=\"meter-value\">" +
                   std::to_string(m.value) + "</span></div>";
        out += "</div>";
    }
    out += std::string("<div class=\"pcard-foot\">") +
           (off ? "Solo game: pick Co-op at Start Game" : solo ? "For co-op: pick it at Start Game"
            : chosen == 0 ? "The chapter's own stats" : stats == 1 ? "Their own stats" : "The chapter's stats") +
           "</div><div class=\"pcard-press\">" + (selected ? "A &#8212; choose character" : "&nbsp;") + "</div></div>";
    return out;
}

// The two-card page's picture of a player: their Choose Character frame (Default's Tome and skull, Random's silhouettes
// and never who was rolled, a form's own). Player 2 on Default plays player 1's character in its own body (hle.c
// coop_p2_choice gives -1, costume_on no form), so theirs is player 1's frame with no form.
RosterTile pick_tile(int player) {
    const int p1 = g_settings.mod_p1_character;
    if (player == 1 && g_settings.mod_p2_character != 0)
        return {g_settings.mod_p2_character, form_on(1, g_settings.mod_p2_character)};
    return {p1, player == 1 ? 0 : form_on(0, p1)};
}
// The line under a picture's name: the form, or what Default and Random mean (never the rolled character).
std::string pick_line(int player) {
    const RosterTile t = pick_tile(player);
    if (player == 1 && g_settings.mod_p2_character == 0 && t.character != 0)
        return "The same as player 1";
    if (t.character == 0)
        return "Each chapter's own character";
    if (t.character == SETTINGS_RANDOM)
        return "Who it is stays a surprise";
    return tile_form_name(t);
}
std::string pick_frame_rml(int player, const char* where) {
    const RosterTile t = pick_tile(player);
    return std::string("<div class=\"pcard-frame") + where + (player == 1 ? " p2-frame" : "") +
           (t.character == 0 ? " default" : t.character == SETTINGS_RANDOM ? " random" : "") +
           "\"><div class=\"pcard-portrait\">" + portrait_rml(t) + "</div></div>";
}

// The two-card page: one card. Its label between the Tome's rules (Solo / Co-op, or Player I / Player II), one picture
// (Solo, a player) or two (Co-op: player 1's in front in gold, player 2's behind and up to the right in blue), the name
// line, the line under it and what A does. No meters: the players' details are on Choose Character.
std::string pick_card_rml(int which, bool selected, size_t index) {
    const bool controls = g_pick_kind == PickKind::Controls;
    const bool pair = !controls && which == 1;
    const int player = controls ? which : 0;  // whose picture is in front
    std::string name = tile_title(pick_tile(player)), line = pick_line(player);
    if (pair) {
        // Co-op: both names, or one when player 2 is on Default (they play player 1's).
        if (g_settings.mod_p2_character != 0) {
            name += std::string(" and ") + tile_title(pick_tile(1));
            line = "Two players, the screen split";
        } else {
            line = pick_tile(0).character == 0 ? "Each chapter's own character, for both" : "For both players";
        }
    }
    const char* label = controls ? (which == 1 ? "Player II" : "Player I") : (which == 1 ? "Co-op" : "Solo");
    const char* press = controls ? "A &#8212; their controls" : which == 1 ? "A &#8212; play together" : "A &#8212; play alone";
    // The card's own glow: Player II's blue, the rest gold (Co-op's two mounts keep each player's colour).
    std::string out = std::string("<div class=\"pcard pickcard ") + (controls && which == 1 ? "p2" : "p1") +
                      (selected ? " selected" : "") + "\" id=\"row" + std::to_string(index) +
                      "\"><div class=\"pcard-head\"><div class=\"pcard-rule\"></div><div class=\"pcard-who\">" + label +
                      "</div><div class=\"pcard-rule\"></div></div>";
    if (pair)
        out += "<div class=\"pcard-pair\">" + pick_frame_rml(1, " behind") + pick_frame_rml(0, " front") + "</div>";
    else
        out += pick_frame_rml(controls ? which : 0, "");
    if (controls && which == 1) {
        static const char* const kInput[] = {"Plays with a controller", "Plays with the keyboard"};
        line = line.empty() ? kInput[g_settings.mod_p2_input & 1] : line;
    }
    // (A mode that needs the game started again, once its A was pressed: the program starts again in it.)
    const bool note = !controls && g_pick_note == which;
    if (note)
        press = g_pick_note_failed ? "The game could not start again" : which == 1 ? "Starting the game again in co-op" : "Starting the game again in solo";
    out += "<div class=\"pcard-name\">" + escape(name) + "</div><div class=\"pcard-form\">" +
           (line.empty() ? std::string("&nbsp;") : escape(line)) + "</div><div class=\"pcard-press\">" +
           (selected || note ? press : "&nbsp;") + "</div></div>";
    return out;
}

// Open the two-card page, its card lit: Solo / Co-op on the mode played last (A A goes on as before), Player I / II on
// the player whose menu it is (player 2's own Tome page lights Player II).
void open_pick(PickKind kind, Screen back, int back_row) {
    g_pick_kind = kind;
    g_pick_return = back;
    g_pick_return_row = back_row;
    g_pick_note = -1;
    g_pick_note_failed = false;
    g_pick_restart_row = -1;
    g_screen = Screen::Pick;
    g_selected = kind == PickKind::Mode ? (g_settings.mod_coop ? 1 : 0) : (g_menu_player == 1 ? 1 : 0);
}

void pick_back() {
    controls::set_menu_player(0);
    g_screen = g_pick_return;
    g_selected = g_pick_return_row;
}

// The two cards (rows with card 1 and 2) and Back under them, as Choose Character's page.
void build_pick() {
    for (int which = 0; which < 2; which++) {
        Row r{g_pick_kind == PickKind::Controls ? (which == 1 ? "Player II" : "Player I") : (which == 1 ? "Co-op" : "Solo")};
        r.card = which + 1;
        if (g_pick_kind == PickKind::Controls) {
            r.activate = [which] {
                std::fprintf(stderr, "[frontend] pick: player %d's controls\n", which + 1);
                controls::set_menu_player(which);
                g_controls_return = Screen::Pick;
                g_controls_return_row = which;
                g_screen = Screen::Controls;
                g_selected = 0;
            };
        } else {
            // Over the title (its Start Game or Load Game, hle.c title_main_start_load): a mode this run's rooms fit is
            // set and saved, and the page closes with the answer; hle.c then presses A on the same row, so the game's
            // own Start or Load goes on. The other mode needs the game started again (plan step 4): it is saved, the
            // card says so, and the program starts again in it from the live page's frame (live_page_frame, once the
            // game is held), into the same row. (The launcher's test door only logs.)
            r.activate = [which] {
                const bool coop = which == 1;
                const char* const name = coop ? "co-op" : "solo";
                if (!g_on_title || g_root != Screen::Pick) {
                    std::fprintf(stderr, "[frontend] pick: %s\n", name);
                    return;
                }
                if (g_pick_restart_row >= 0)
                    return;  // (already starting again)
                if (rooms_would_change(coop)) {
                    g_settings.mod_coop = coop;
                    settings_coop_chosen();
                    settings_save();
                    g_pick_note = which;
                    g_pick_note_failed = false;
                    g_pick_restart_row = g_pick_title_row;
                    g_pick_restart_at = SDL_GetTicks();
                    std::fprintf(stderr, "[frontend] pick: %s for %s, saved: the game starts again in it\n", name,
                                 g_pick_title_row == 1 ? "Load Game" : "Start Game");
                    return;
                }
                g_settings.mod_coop = coop;
                settings_coop_chosen();
                changed();  // (co-op's frame rate)
                settings_save();
                g_title_pick_result = g_pick_title_row * 2 + (coop ? 1 : 0);
                std::fprintf(stderr, "[frontend] pick: %s for %s, saved\n", name,
                             g_pick_title_row == 1 ? "Load Game" : "Start Game");
                g_close = true;
            };
        }
        g_rows.push_back(r);
    }
    Row back{"Back"};
    back.gap = true;
    back.activate = [] { pick_back(); };
    g_rows.push_back(back);
}

// Character select: every character and form, their portrait (from the disc, ensure_portraits; a form's
// from tools/ed/form_portraits.py) with their name and form under it, in a list that scrolls.
void build_characters() {
    ensure_portraits();
    auto leave = [] {
        settings_save();
        g_screen = Screen::ModsPage;
        g_selected = g_characters_return_row;
    };
    for (const RosterTile& t : roster_tiles()) {
        Row r{tile_title(t)};
        r.tile = true;
        r.chosen = chosen_character() == t.character &&
                   (t.character == 0 || t.character == SETTINGS_RANDOM || form_on(g_characters_player, t.character) == t.form);
        r.value_rml = portrait_rml(t);
        r.activate = [t] {
            chosen_character() = t.character;
            if (t.character == SETTINGS_RANDOM) {
                // Rolled now and kept (settings.ini) until the player chooses again; A on it again rolls someone new.
                settings_roll_random(g_characters_player);
                chosen_stats() = 1;  // their own stats, as any chosen character
            } else if (t.character != 0) {
                form_set(t.character, t.form);
                chosen_stats() = 1;  // a chosen character plays with their own stats
            }
            settings_save();  // chosen: the page stays, with the gold frame on it now
        };
        g_rows.push_back(r);
    }
    static const char* const kStatsChoice[] = {"Vanilla", "Chosen character's"};
    Row stats{"Stats", kStatsChoice[chosen_stats()]};
    stats.gap = true;
    stats.note = "Vanilla: the chapter character's health, sanity and magick. Chosen character's: their own, bars too.";
    stats.change = [](int) { chosen_stats() = 1 - chosen_stats(); };
    g_rows.push_back(stats);
    Row back{"Back"};
    back.activate = leave;
    g_rows.push_back(back);
}

// Arrows on Character select: left/right along the frames, up/down between
// their lines and the rows under them.
void move_in_roster(int dx, int dy) {
    const int n = static_cast<int>(g_rows.size());
    const int count = roster_count();
    int& at = g_selected;
    if (dx != 0) {
        if (at < count)
            at = (at + dx + count) % count;
        else if (g_rows[at].change)
            g_rows[at].change(dx);
        return;
    }
    if (at < count) {
        at += dy * roster_columns();
        if (at >= count)
            at = count;
        else if (at < 0)
            at = n - 1;
    } else {
        at += dy;
        if (at < count)
            at = count - 1 - (count - 1) % roster_columns();  // the last line's first frame
        else if (at >= n)
            at = 0;
    }
}

void build_mods_rows() {
    // Mods > Co-op is gone (docs/research/solo-coop-picker/plan.md step 5): Start Game and Load Game ask Solo or Co-op
    // (mod_coop, kept in settings.ini), player 2's controls are Controls > Player II, and the screen split and player
    // 2's camera are on Display, under Camera. The sections keep their numbers (II Challenge ... V Sanity Effects).
    static const char* const kEnemies[] = {"Vanilla", "Enhanced"};
    Row enemies{"Creatures", kEnemies[g_settings.mod_enemies]};
    enemies.section = "II \u00b7 Challenge";
    enemies.note = "In development: no effect yet.";
    enemies.change = [](int dir) { cycle(g_settings.mod_enemies, 2, dir); };
    g_rows.push_back(enemies);

    static const char* const kInsanity[] = {"Normal", "Enhanced", "Off"};
    Row insanity{"Sanity effects", kInsanity[g_settings.mod_insanity]};
    insanity.note = g_settings.mod_insanity == 1
                        ? "Single player. Below 95% sanity, every door can bring an effect, more often as it falls."
                    : g_settings.mod_insanity == 2 ? "The game's random effects stop. What the story calls for stays."
                                                   : "As the game has them.";
    insanity.change = [](int dir) { cycle(g_settings.mod_insanity, 3, dir); };
    g_rows.push_back(insanity);

    // Each player's character, chosen on its own page (build_characters).
    // Player 1's (hle.c player_model): any of the twelve, in any chapter, in
    // their own body. Player 2's is kept for when player 2 can be another.
    auto character_row = [](int player) {
        const int chosen = player == 1 ? g_settings.mod_p2_character : g_settings.mod_p1_character;
        const RosterTile t{chosen, form_on(player, chosen)};
        Row r{player == 1 ? "Player 2 character" : "Player 1 character", tile_title(t)};
        if (tile_form_name(t)[0] != 0)
            r.value += std::string(" \u00b7 ") + tile_form_name(t);
        if ((player == 1 ? g_settings.mod_p2_stats : g_settings.mod_p1_stats) == 1)
            r.value += " (own stats)";
        r.page = 1;
        r.card = player + 1;
        r.note = in_play() ? "A change reloads the game where you stand once the menu closes."
                           : "Also applies to a save state you load: it reloads with them.";
        r.activate = [player] {
            g_characters_player = player;
            g_characters_return_row = g_selected;
            g_screen = Screen::Characters;
            g_selected = roster_index(chosen_character(), form_on(player, chosen_character()));
        };
        return r;
    };
    Row p1_character = character_row(0);
    p1_character.section = "III \u00b7 Characters";
    g_rows.push_back(p1_character);
    g_rows.push_back(character_row(1));

    // Cheats (hle.c cheat_apply), in a game: on or off, kept as play goes on.
    auto cheat_row = [](const char* label, const char* note, int which) {
        Row r{label, on_off(in_play() && hle_cheat_on(which))};
        r.page = 1;
        r.toggle = true;
        r.enabled = in_play();
        r.note = in_play() ? note : "In a game, from its menu.";
        r.change = [which](int) { hle_cheat_set(which, !hle_cheat_on(which)); };
        return r;
    };
    Row magick = cheat_row("All magick",
                           "Every rune, circle of power and spell, and the Tome's magick page. Off puts back what you had.",
                           kCheatMagick);
    magick.section = "IV \u00b7 Cheats";
    g_rows.push_back(magick);
    g_rows.push_back(cheat_row("Full stats", "Health, sanity and magick kept at their most, for each player.", kCheatFull));

    // Sanity Effects: any of the game's effects on a save (hle.c sanity_ask), to
    // see each one, with a log of how it went.
    static int sanity_effect = 0;
    static bool sanity_ignore = false;
    const bool sanity_can = in_play() && !g_settings.mod_coop;
    const char* const sanity_not = g_settings.mod_coop ? "Single player for now: choose Solo from Start Game or Load Game."
                                                       : "In a game, from its menu.";
    Row effect{"Effect", std::to_string(hle_sanity_number(sanity_effect)) + ": " + hle_sanity_name(sanity_effect)};
    effect.section = "V \u00b7 Sanity Effects";
    effect.note = hle_sanity_what(sanity_effect);
    effect.change = [](int dir) { cycle(sanity_effect, hle_sanity_count(), dir); };
    g_rows.push_back(effect);
    Row limits{"The game's limits", sanity_ignore ? "Ignore" : "Keep"};
    limits.toggle = true;
    limits.note = "Keep: sanity counts as lowest, the game's other rules stay, so a door may not start it. Ignore: starts "
                  "it at any door. It may break: use a save you don't mind losing, and don't save after.";
    limits.change = [](int) { sanity_ignore = !sanity_ignore; };
    g_rows.push_back(limits);
    Row play{"Play this effect"};
    play.enabled = sanity_can;
    play.note = sanity_can ? "Closes the menu and starts it the game's way: most take you through a nearby door, "
                             "and a few wait for you (a press of Start, the Tome)."
                           : sanity_not;
    play.activate = [] {
        hle_sanity_play(hle_sanity_number(sanity_effect), sanity_ignore);
        g_close = true;
    };
    g_rows.push_back(play);
    Row stop{"End the effect on now"};
    stop.enabled = sanity_can && hle_sanity_on();
    stop.note = "If one is stuck or waiting.";
    stop.activate = [] {
        hle_sanity_stop();
        g_close = true;
    };
    g_rows.push_back(stop);
    // Every door a test: one effect at a time, sanity going down as each level's are seen.
    Row doors{"Random effects at doors", on_off(hle_sanity_doors_on())};
    doors.gap = true;
    doors.toggle = true;
    doors.enabled = sanity_can;
    doors.note = sanity_can ? "Each door starts one at random, in any room. Your sanity drops below 75%, then 50%, then 25% as each "
                              "level's have played, then starts over. No story, Timeline, saving or full-screen effects."
                            : sanity_not;
    doors.change = [](int) { hle_sanity_doors_set(!hle_sanity_doors_on()); };
    g_rows.push_back(doors);
    static const char* const kLevels[] = {"Full", "Below 75%", "Below 50%", "Below 25%"};
    Row drain{"Sanity for the doors", kLevels[std::clamp(hle_sanity_doors_level(), 0, 3)]};
    drain.enabled = sanity_can && hle_sanity_doors_on();
    drain.note = "Press to go to the next level now. It holds there, so turn Full stats off.";
    drain.activate = [] { hle_sanity_doors_next(); };
    g_rows.push_back(drain);
    Row last{"Last result", hle_sanity_result()};
    last.enabled = false;
    last.note = "Every effect is written to logs/sanity-effects.log, the game's own too.";
    g_rows.push_back(last);

    Row back{"Back"};
    back.gap = true;
    back.page = 1;
    back.activate = [] {
        settings_save();
        g_screen = g_mods_return;
        g_selected = g_mods_return == Screen::Options ? g_options_row : 0;
    };
    g_rows.push_back(back);
}

// Mods: its sections, each a page of its own, then Back.
void build_mods() {
    auto page = [](const char* label, int section) {
        Row r{label};
        r.activate = [section] {
            g_mods_section = section;
            g_mods_row = g_selected;
            g_mods_page_return = Screen::Mods;
            g_screen = Screen::ModsPage;
            g_selected = 0;
        };
        return r;
    };
    auto experimental = [](Row r) {
        r.experimental = true;
        return r;
    };
    // (No Co-op: Start Game and Load Game ask Solo or Co-op.)
    g_rows.push_back(page("Challenge", 2));
    // In a game only: on the launcher and the title it's Choose Character, their own row.
    if (in_play())
        g_rows.push_back(experimental(page("Characters", 3)));
    // Not on the title: cheats and sanity effects are for a game going (greyed there, they did nothing).
    if (!g_on_title) {
        g_rows.push_back(experimental(page("Cheats", 4)));
        g_rows.push_back(page("Sanity Effects", 5));
    }
    // Only on the launcher: it is how Start game starts.
    if (!g_in_game) {
        Row skip{"Skip intro", on_off(g_settings.mod_skip_intro)};
        skip.gap = true;
        skip.toggle = true;
        skip.note = "Main menu skips the opening movies.";
        skip.change = [](int) { g_settings.mod_skip_intro = !g_settings.mod_skip_intro; };
        g_rows.push_back(skip);
    }
    // The title menu's moving background (titlevideo.cpp), once made. Not in a game (it is the title's).
    if (!in_play()) {
        std::error_code ec;
        const bool have_video =
            std::filesystem::is_regular_file(std::filesystem::path(frontend_res_dir()) / "video" / "title.mpg", ec);
        Row video{"Title video", have_video ? on_off(g_settings.mod_title_video) : "none"};
        video.gap = g_in_game;
        video.toggle = true;
        video.enabled = have_video;
        if (!have_video)
            video.note = "No video yet: tools/title_video.py makes one.";
        video.change = [](int) { g_settings.mod_title_video = !g_settings.mod_title_video; };
        g_rows.push_back(video);
    }
    Row back{"Back"};
    back.gap = true;
    back.activate = [] {
        settings_save();
        g_screen = g_mods_return;
        g_selected = g_mods_return == Screen::Options ? g_options_row : 0;
    };
    g_rows.push_back(back);
}

// One Mods section's rows, from the full list (build_mods_rows), on one page.
void build_mods_page() {
    std::vector<Row> all;
    std::swap(all, g_rows);
    build_mods_rows();
    std::swap(all, g_rows);
    int section = 0;
    for (Row& r : all) {
        if (!r.section.empty())
            section = section_number(r.section);
        if (section != g_mods_section || r.label == "Back")
            continue;
        r.section.clear();
        r.page = 0;
        g_rows.push_back(r);
    }
    Row back{"Back"};
    back.gap = true;
    back.activate = [] { mods_page_back(); };
    g_rows.push_back(back);
}

void mods_page_back() {
    settings_save();
    g_screen = g_mods_page_return;
    g_selected = g_mods_page_return == Screen::Mods ? g_mods_row : g_mods_page_return_row;
}

const char* mods_page_name() {
    switch (g_mods_section) {
    case 2: return "Challenge";
    case 3: return "Choose Character";
    case 4: return "Cheats";
    default: return "Sanity Effects";
    }
}

void build_launcher() {
    if (g_disc_on_main)
        g_rows.push_back(disc_row());

    const int newest = newest_slot();
    if (newest != 0) {
        Row cont{"Continue"};
        cont.enabled = disc_playable();
        cont.note = "Back where you saved last.";
        cont.activate = [newest] { start_game(newest); };
        g_rows.push_back(cont);
    }

    Row fresh{"Main menu"};
    fresh.big = true;
    fresh.enabled = disc_playable();
    fresh.note = "Start from the title screen (your memory card saves are there).";
    fresh.activate = [] { start_game(0); };
    g_rows.push_back(fresh);

    if (newest != 0) {
        Row load{"Load game"};
        load.experimental = true;
        load.enabled = disc_playable();
        load.note = "Your save slots: load, duplicate or delete one.";
        load.activate = [] {
            g_screen = Screen::Play;
            g_selected = play_row(newest_slot());  // the save made last
        };
        g_rows.push_back(load);
    }

    // Each player's character: the Mods page of the same name, opened from here. (No Co-op row: the title's Start Game
    // and Load Game ask Solo or Co-op, and Continue and Load game play a save state in the mode it was made in.)
    auto mods_page = [](const char* label, int section, const char* note) {
        Row r{label};
        r.note = note;
        r.experimental = true;
        r.activate = [section] {
            g_options_return = Screen::Launcher;
            g_mods_return = Screen::Launcher;
            g_mods_section = section;
            g_mods_page_return = Screen::Launcher;
            g_mods_page_return_row = g_selected;
            g_screen = Screen::ModsPage;
            g_selected = 0;
        };
        return r;
    };
    g_rows.push_back(mods_page("Choose Character", 3, "Player 1's and player 2's characters and forms."));

    Row options{"Options"};
    options.gap = true;
    options.activate = [] {
        dol_aurora_measure_screen();
        g_options_return = Screen::Launcher;
        g_screen = Screen::Options;
        g_selected = 0;
    };
    g_rows.push_back(options);
    g_rows.push_back(mods_row(Screen::Launcher));

    Row quit{"Quit"};
    quit.gap = true;
    quit.activate = [] {
        if (g_in_game) {
            settings_save();
            dol_aurora_request_quit();
        }
        g_play = false;
        g_close = true;
    };
    g_rows.push_back(quit);
}

// Load game: the used save slots; one chosen opens what can be done with it
// (build_slot). Continue loads the newest. In the game, Save states lists all
// of them (build_states), and a slot's page can save there too.
int g_slot_chosen = 0;
bool g_slot_from_states = false;  // the slot page was opened from Save states

int play_row(int slot);

// The page a slot's page goes back to, at that slot's row.
void slots_page(int slot) {
    g_screen = g_slot_from_states ? Screen::States : Screen::Play;
    g_selected = g_slot_from_states ? slot - 1 : play_row(slot);
}

// The slot page's row for Delete (after Save here, in the game).
int delete_row() {
    return g_slot_from_states ? 3 : 2;
}

void save_slot(int slot) {
    quickstate_request_save(slot_path(slot).c_str());
    std::fprintf(stderr, "[frontend] saved to slot %d\n", slot);
    g_close = true;
}

// LB and RB on a list of slots (and left and right on a slot): the page before or the next, round and round,
// at the same place on the page. From Back, the page's first slot.
void slots_jump(int dir) {
    const int slots = static_cast<int>(g_rows.size()) - 1;
    if (slots <= 0)
        return;
    const int pages = (slots - 1) / kSlotsShown + 1;
    const bool on_slot = g_selected < slots;
    const int page = ((on_slot ? g_selected / kSlotsShown : g_slots_page) + dir + pages) % pages;
    g_selected = std::min(page * kSlotsShown + (on_slot ? g_selected % kSlotsShown : 0), slots - 1);
}

std::string slots_note(int row, int slots) {
    const int page = row / kSlotsShown;
    const int from = page * kSlotsShown;
    return "Page " + std::to_string(page + 1) + " of " + std::to_string((slots - 1) / kSlotsShown + 1) + ": " + std::to_string(from + 1) + " to " +
           std::to_string(std::min(from + kSlotsShown, slots)) + " of " + std::to_string(slots) + ". LB and RB change page.";
}

void build_states() {
    for (int slot = 1; slot <= kSlotCount; slot++) {
        const bool used = slot_used(slot);
        Row r{"Slot " + std::to_string(slot), used ? slot_time(slot) : "empty"};
        r.note = slots_note(slot - 1, kSlotCount);
        r.change = slots_jump;
        r.activate = [slot, used] {
            if (!used) {
                save_slot(slot);
                return;
            }
            g_slot_chosen = slot;
            g_slot_from_states = true;
            g_screen = Screen::Slot;
            g_selected = 0;
        };
        g_rows.push_back(r);
    }

    Row back{"Back"};
    back.gap = true;
    back.activate = [] {
        g_screen = Screen::InGame;
        g_selected = 1;
    };
    g_rows.push_back(back);
}

void build_play() {
    int used_slots = 0;
    for (int slot = 1; slot <= kSlotCount; slot++)
        used_slots += slot_used(slot);
    for (int slot = 1; slot <= kSlotCount; slot++) {
        if (!slot_used(slot))
            continue;
        Row r{"Slot " + std::to_string(slot), slot_time(slot)};
        r.note = slots_note(static_cast<int>(g_rows.size()), used_slots);
        r.change = slots_jump;
        r.activate = [slot] {
            g_slot_chosen = slot;
            g_slot_from_states = false;
            g_screen = Screen::Slot;
            g_selected = 0;
        };
        g_rows.push_back(r);
    }

    Row back{"Back"};
    back.gap = true;
    back.activate = [] {
        g_screen = Screen::Launcher;
        g_selected = 0;
    };
    g_rows.push_back(back);
}

// The first unused slot, 0 when all are used.
int free_slot() {
    for (int slot = 1; slot <= kSlotCount; slot++)
        if (!slot_used(slot))
            return slot;
    return 0;
}

// The Load game page's row for a slot, to come back to.
int play_row(int slot) {
    int row = 0;
    for (int s = 1; s < slot; s++)
        row += slot_used(s);
    return row;
}

// One save slot: (in the game) save over it, load it, copy it into a free
// slot, or delete it.
void build_slot() {
    const int slot = g_slot_chosen;
    if (g_slot_from_states) {
        Row save{"Save here"};
        save.note = "Replaces this save. You are asked first.";
        save.activate = [] {
            g_screen = Screen::SlotOverwrite;
            g_selected = 0;
        };
        g_rows.push_back(save);
    }

    Row load{"Load"};
    load.enabled = disc_playable();
    load.activate = [slot] { start_game(slot); };
    g_rows.push_back(load);

    const int to = free_slot();
    Row dup{"Duplicate", to != 0 ? "to slot " + std::to_string(to) : "no free slot"};
    dup.enabled = to != 0;
    dup.note = to != 0 ? "Copy this save into slot " + std::to_string(to) + "." : "All slots are used.";
    dup.activate = [slot, to] {
        std::error_code ec;
        std::filesystem::copy_file(slot_path(slot), slot_path(to), std::filesystem::copy_options::overwrite_existing,
                                   ec);
        std::fprintf(stderr, "[frontend] slot %d duplicated to slot %d%s\n", slot, to, ec ? " (failed)" : "");
        slots_page(to);
    };
    g_rows.push_back(dup);

    Row del{"Delete"};
    del.note = "You are asked first.";
    del.activate = [] {
        g_screen = Screen::SlotDelete;
        g_selected = 0;
    };
    g_rows.push_back(del);

    Row back{"Back"};
    back.gap = true;
    back.activate = [slot] { slots_page(slot); };
    g_rows.push_back(back);
}

// Deleting a slot, asked first: keeping it is the first choice.
void build_slot_delete() {
    const int slot = g_slot_chosen;
    Row keep{"Keep it"};
    keep.note = "Slot " + std::to_string(slot) + " \u00b7 " + slot_time(slot) + " stays.";
    keep.activate = [] {
        g_screen = Screen::Slot;
        g_selected = delete_row();
    };
    g_rows.push_back(keep);

    Row del{"Delete slot " + std::to_string(slot)};
    del.note = "This save is gone for good.";
    del.activate = [slot] {
        std::error_code ec;
        std::filesystem::remove(slot_path(slot), ec);
        std::fprintf(stderr, "[frontend] slot %d deleted%s\n", slot, ec ? " (failed)" : "");
        // Back to the slots (the launcher's main menu when none is left).
        if (g_slot_from_states || newest_slot() != 0) {
            slots_page(slot);
            if (!g_slot_from_states)
                g_selected = 0;
        } else {
            g_screen = Screen::Launcher;
            g_selected = 0;
        }
    };
    g_rows.push_back(del);
}

// Saving over a used slot, asked first: keeping it is the first choice.
void build_slot_overwrite() {
    const int slot = g_slot_chosen;
    Row keep{"No, keep it"};
    keep.note = "Slot " + std::to_string(slot) + " · " + slot_time(slot) + " stays.";
    keep.activate = [] {
        g_screen = Screen::Slot;
        g_selected = 0;
    };
    g_rows.push_back(keep);

    Row save{"Yes, overwrite slot " + std::to_string(slot)};
    save.note = "The save in this slot is replaced by the game as it is now.";
    save.activate = [slot] { save_slot(slot); };
    g_rows.push_back(save);
}

// Quit game from the title's Options list, asked first: Up from the row Options opens on wraps to it.
void build_quit_ask() {
    Row no{"No, go back"};
    no.note = "Back to Options.";
    no.activate = [] {
        g_screen = Screen::InGame;  // past the page: the menu closes
        g_selected = 0;
    };
    g_rows.push_back(no);

    Row yes{"Yes, quit"};
    yes.note = "These settings are saved. The game's own options reach the memory card only when you leave Options with B.";
    yes.activate = [] {
        std::fprintf(stderr, "[frontend] Quit game from the title\n");
        settings_save();
        dol_aurora_request_quit();
        g_close = true;
    };
    g_rows.push_back(yes);
}

Row layout_row() {
    // On the keyboard alone every layout has the same keys: Default or Custom.
    if (!controls::player_has_controller(controls::menu_player())) {
        const bool custom = controls::layout() == controls::kCustom;
        Row keys{"Keys", custom ? "Custom" : "Default"};
        keys.note = custom ? "Your own keys (Button mapping)." : "WASD moves, TFGH turns the camera, J attacks, K acts.";
        keys.change = [custom](int) {
            controls::set_layout(custom ? controls::kOriginal : controls::kCustom);
        };
        return keys;
    }
    Row layout{"Layout", controls::layout_name(controls::layout())};
    static const char* const kLayoutNotes[] = {
        "The GameCube's button positions.",
        "Attack RT/A, action B, aim LT, sprint L3, creep R3.",
        "Your own buttons (Button mapping)."};
    layout.note = kLayoutNotes[controls::layout()];
    layout.change = [](int dir) {
        int l = controls::layout();
        cycle(l, controls::kLayoutCount, dir);
        controls::set_layout(l);
    };
    return layout;
}

// What the menu player plays with: player 1 a controller, the keyboard or
// both; player 2 a controller or the keyboard.
Row device_row() {
    const int player = controls::menu_player();
    if (player == 0) {
        static const char* const kDevice[] = {"Controller and keyboard", "Controller", "Keyboard"};
        Row r{"Plays with", kDevice[g_settings.controls_device]};
        r.change = [](int dir) {
            cycle(g_settings.controls_device, 3, dir);
            settings_save();
        };
        return r;
    }
    static const char* const kInput[] = {"Controller", "Keyboard"};
    Row r{"Plays with", kInput[g_settings.mod_p2_input]};
    if (g_settings.mod_p2_input == 1 && g_settings.controls_device == 2)
        r.note = "Player 1 uses the keyboard too: give them different keys (Button mapping).";
    r.change = [](int dir) {
        cycle(g_settings.mod_p2_input, 2, dir);
        settings_save();
    };
    return r;
}

// Which connected controller is the menu player's: automatic, or one by its
// place in the list (kept for the next time it is connected).
Row controller_row() {
    const int player = controls::menu_player();
    const std::vector<std::string> pads = controls::connected_controllers();
    int& slot = g_settings.pad_slot[player];
    std::string value;
    if (slot == 0) {
        const std::string name = controls::controller_name();
        value = "Automatic" + (name.rfind("No ", 0) == 0 ? std::string(" (none connected)") : " (" + name + ")");
    } else {
        value = std::to_string(slot) + ": " +
                (slot <= static_cast<int>(pads.size()) ? pads[slot - 1] : std::string("not connected"));
    }
    Row r{"Controller", value};
    if (pads.empty())
        r.note = "No controller is connected.";
    // Automatic, then a place for each connected controller (at least the
    // two players' own, so either can be chosen before it is plugged in).
    const int places = std::max(static_cast<int>(pads.size()), 2) + 1;
    r.change = [&slot, places](int dir) {
        slot = (std::min(slot, places - 1) + dir + places) % places;
        settings_save();
    };
    return r;
}

// The menu player's controller type: the names and icons shown for it.
Row pad_type_row() {
    const int player = controls::menu_player();
    int& type = g_settings.pad_type[player];
    std::string value = controls::pad_type_name(type);
    if (type == controls::kPadAuto) {
        const std::string found = controls::detected_pad_type(player);
        value += found.empty() ? "" : " (" + found + ")";
    }
    Row r{"Controller type", value};
    r.change = [&type](int dir) {
        cycle(type, controls::kPadTypeCount, dir);
        settings_save();
    };
    return r;
}

Row vibration_row() {
    Row vibration{"Vibration", on_off(g_settings.vibration)};
    vibration.toggle = true;
    vibration.change = [](int) {
        g_settings.vibration = !g_settings.vibration;
        changed();
        settings_save();
    };
    return vibration;
}

// Graphics enhancements: the options' rows and photo mode change the same
// values, applied at once.
namespace enhancement {
enum { kGlow, kOcclusion, kHdr, kShadows, kHdTextures, kOcclusionQuality, kPerPixel, kSmaa, kSharpen, kShafts, kCharShadows, kShadowEdges, kContact, kVolumetric, kSkin, kSpellRim, kSpellFx, kCount };
const char* const kNames[kCount] = {"Glow", "Ambient occlusion", "HDR", "Baked shadows", "HD textures", "Occlusion quality",
                                    "Per-pixel lighting", "SMAA", "Sharpening", "Light shafts", "Character shadows", "Shadow edges", "Contact shadows", "Haze", "Skin scattering", "Spell glow", "Remastered spells"};
const char* const kLevels[] = {"Off", "Soft", "Strong"};

bool available(int which) {
    if (which == kHdr)
        return dol_aurora_hdr_available();
    if (which == kHdTextures)
        return texpack_count() != 0;
    if (which == kShadowEdges)
        return g_settings.char_shadows != 0;  // the game's own have none to choose
    if (which == kSkin)
        return g_settings.per_pixel_lighting;  // it's worked out with it
    return true;
}

std::string value(int which) {
    switch (which) {
    case kGlow: return kLevels[g_settings.glow];
    case kOcclusion: return kLevels[g_settings.ssao];
    case kHdr: return available(which) ? on_off(g_settings.hdr) : "Not on this display";
    case kShadows: return g_settings.sharp_shadows ? "Sharp" : "Original";
    case kHdTextures: return available(which) ? on_off(g_settings.hd_textures) : "No pack";
    case kOcclusionQuality: return g_settings.ssao_high ? "High" : "Standard";
    case kPerPixel: return on_off(g_settings.per_pixel_lighting);
    case kSmaa: return on_off(g_settings.smaa);
    case kSharpen: {
        static const char* const kAmounts[] = {"Off", "Low", "Medium", "High"};
        return kAmounts[g_settings.sharpen];
    }
    case kShafts: return kLevels[g_settings.shafts];
    case kCharShadows: {
        static const char* const kKinds[] = {"The game's", "Light", "Dark"};
        return kKinds[g_settings.char_shadows];
    }
    case kShadowEdges: return g_settings.shadow_hard ? "Hard" : "Soft";
    case kContact: return on_off(g_settings.contact_shadows);
    case kVolumetric: return g_settings.volumetric == 0 ? "Off" : std::to_string(g_settings.volumetric) + "%";
    case kSkin: return on_off(g_settings.skin_scatter);
    case kSpellRim: return on_off(g_settings.spell_rim);
    case kSpellFx: return on_off(g_settings.spell_fx);
    default: return "";
    }
}

void change(int which, int dir) {
    switch (which) {
    case kGlow: cycle(g_settings.glow, 3, dir); break;
    case kOcclusion: cycle(g_settings.ssao, 3, dir); break;
    case kHdr: g_settings.hdr = !g_settings.hdr; break;
    case kShadows: g_settings.sharp_shadows = !g_settings.sharp_shadows; break;
    case kHdTextures: g_settings.hd_textures = !g_settings.hd_textures; break;
    case kOcclusionQuality: g_settings.ssao_high = !g_settings.ssao_high; break;
    case kPerPixel: g_settings.per_pixel_lighting = !g_settings.per_pixel_lighting; break;
    case kSmaa: g_settings.smaa = !g_settings.smaa; break;
    case kSharpen: cycle(g_settings.sharpen, 4, dir); break;
    case kShafts: cycle(g_settings.shafts, 3, dir); break;
    case kCharShadows: cycle(g_settings.char_shadows, 3, dir); break;
    case kShadowEdges: g_settings.shadow_hard = !g_settings.shadow_hard; break;
    case kContact: g_settings.contact_shadows = !g_settings.contact_shadows; break;
    case kVolumetric: g_settings.volumetric = std::clamp(g_settings.volumetric + dir * 5, 0, 100); break;
    case kSkin: g_settings.skin_scatter = !g_settings.skin_scatter; break;
    case kSpellRim: g_settings.spell_rim = !g_settings.spell_rim; break;
    case kSpellFx: g_settings.spell_fx = !g_settings.spell_fx; break;
    default: return;
    }
    settings_apply_graphics();
}

const char* const kNotes[kCount] = {
    "A soft haze round lamps, flames and magick.",
    "Darker corners and creases.",
    "Light brighter than white on an HDR display.",
    "Sharp: the rooms' light maps upscaled. Original: as on the disc.",
    "",
    "High: fuller, cleaner shade, at full size. It takes more of the graphics card.",
    "Smoother light across characters and things, from the game's own lights.",
    "Smooths edges MSAA can't reach, such as cut-out textures.",
    "Crisper textures. The HUD and text aren't touched.",
    "Soft beams from lamps and flames, cut by what stands in their way.",
    "Cast from the game's own lights, how dark. The game's: flat on the floor.",
    "Soft: blurs further from the character, as real shadows do. Hard: a crisp edge all along.",
    "Short, dark shadows where characters meet the floor and walls.",
    "Hazy air round the room's lamps and spells, with the characters' shadows through it.",
    "A warm glow where light turns to shade on skin.",
    "Spells shine, and the edges of their shapes glow.",
    "A spell's runes as 3D stones glowing in its colour, lighting what's round them, its sparks in its colour.",
};

}  // namespace enhancement

void run_hdr_calibration(int first_step);

void build_options_rows() {
    static const char* const kAspect[] = {"16:9", "4:3", "16:9", "16:9", "21:9"};
    // Each resolution as the pixels it draws at, for the display shape in use:
    // the window's own, or a standard height at 4:3 or 16:9. The shape comes
    // from the choice itself: the renderer takes a new aspect at its next
    // present, after these rows are built, so its own value lags one change.
    const float display_aspect = g_settings.aspect == 1   ? 4.f / 3.f
                                 : g_settings.aspect == 4 ? 21.f / 9.f
                                                          : 16.f / 9.f;
    uint32_t win_w = 0, win_h = 0;
    dol_aurora_get_display_info(nullptr, &win_w, &win_h);
    auto size_label = [&](int scale) {
        uint32_t w, h;
        if (scale == 0) {
            w = win_w;
            h = win_h;
            if (h != 0 && static_cast<float>(w) / h > display_aspect)
                w = static_cast<uint32_t>(h * display_aspect + 0.5f);
            else if (display_aspect > 0.f)
                h = static_cast<uint32_t>(w / display_aspect + 0.5f);
        } else {
            static const uint32_t kLines[] = {0, 720, 1080, 1440, 2160};
            h = kLines[scale];
            w = static_cast<uint32_t>(h * display_aspect + 0.5f);
        }
        return std::to_string(w) + "\u00d7" + std::to_string(h);
    };
#ifdef __PROSPERO__
    const char* const kNative = "Screen (";  // no window on a console
#else
    const char* const kNative = "Window (";
#endif
    const std::string kScale[] = {
        kNative + size_label(0) + ")", "720p (" + size_label(1) + ")",
        "1080p (" + size_label(2) + ")", "1440p (" + size_label(3) + ")",
        "4K (" + size_label(4) + ")"};

#ifndef __PROSPERO__
    Row full{"Fullscreen", on_off(g_settings.fullscreen)};
    full.section = "I \u00b7 Display";
    full.toggle = true;
    full.change = [](int) { g_settings.fullscreen = !g_settings.fullscreen; changed(); };
    g_rows.push_back(full);
#endif

    Row scale{"Resolution", kScale[g_settings.render_scale]};
#ifdef __PROSPERO__
    // The PS5 has no window: the picture always fills the screen, so no
    // Fullscreen row, and the section starts here.
    scale.section = "I \u00b7 Display";
#endif
    scale.change = [](int dir) { cycle(g_settings.render_scale, 5, dir); changed(); };
    g_rows.push_back(scale);

    // Frames in between the game's own 60 a second (the renderer blends the
    // last two) on a faster screen. The choices are the screen's: its rate,
    // measured as the options open (so a refresh rate changed meanwhile shows
    // here), and in fullscreen on a PC every faster rate the display has,
    // which it is switched to.
    int choices[8];
    const int count = dol_aurora_frame_rate_choices(choices, 8);
    const int screen_hz = dol_aurora_screen_refresh();
    const bool faster = count > 1;
    int shown = 60;
    if (g_settings.frame_rate != 60 && faster) {
        shown = screen_hz >= 75 ? screen_hz : choices[count - 1];
        for (int i = 0; i < count; i++)
            if (choices[i] == g_settings.frame_rate)
                shown = g_settings.frame_rate;
    }
    // Co-op plays at 60 (settings_apply): greyed in a co-op game only. Before a game (the title, the launcher) it can be
    // set for solo, whichever mode was picked last. The co-op note shows only when co-op was the last mode picked; a
    // solo player on a fast screen gets no note, as before.
    const bool coop_game = g_settings.mod_coop && in_play();
    Row rate{"Frame rate", std::to_string(coop_game ? 60 : shown)};
    rate.enabled = faster && !coop_game;
    if (coop_game || (faster && g_settings.mod_coop)) {
        rate.note = "Co-op plays at 60 frames a second.";
    } else if (!faster) {
#ifdef __PROSPERO__
        rate.note = "Your TV is running at 60 Hz. 120 needs a TV that takes 120 Hz, with Enable 120 Hz "
                    "Output on in the PS5's Screen and Video settings.";
#else
        rate.note = std::getenv("SteamDeck") != nullptr
                        ? std::string("Your screen is running at 60 Hz. On a Steam Deck OLED, set Refresh "
                                      "Rate to 90 in Steam's Performance menu, then open Options again.")
                        : "Your screen is running at " + std::to_string(screen_hz) +
                              " Hz: smoother motion needs a faster one.";
#endif
    }
    rate.change = [choices, count, shown](int dir) {
        int at = count - 1;
        for (int i = 0; i < count; i++)
            if (choices[i] == shown)
                at = i;
        cycle(at, count, dir);
        g_settings.frame_rate = choices[at];
        changed();
    };
    g_rows.push_back(rate);

    Row aspect{"Aspect ratio", kAspect[g_settings.aspect]};
    aspect.note = "Wider shows more at the sides, not stretched.";
    aspect.change = [](int dir) {
        static const int kChoices[] = {1, 2, 4};  // 4:3, 16:9, 21:9
        int at = 1;
        for (int i = 0; i < 3; i++)
            if (kChoices[i] == g_settings.aspect)
                at = i;
        cycle(at, 3, dir);
        g_settings.aspect = kChoices[at];
        changed();
    };
    // On a Tome page only the wide shapes: going to or from 4:3 makes hle.c sync_widescreen_option flip the game's
    // widescreen word with the Tome open, with no reload of the Tome's art. The game's own Widescreen in the Tome fades
    // out and reloads it first: that is the Tome's way in and out of 4:3. 16:9 and 21:9 are both "wide" to the game
    // (launcher_widescreen), so moving between them leaves its word alone and only the picture's width changes.
    if (g_skin == Skin::Tome) {
        if (g_settings.aspect == 1) {
            aspect.enabled = false;
            aspect.note = "Turn Widescreen on in the Tome's own Options first, then choose 16:9 or 21:9 here.";
            aspect.change = [](int) {};
        } else {
            aspect.change = [](int) {
                g_settings.aspect = g_settings.aspect == 4 ? 2 : 4;
                changed();
            };
        }
    }
    g_rows.push_back(aspect);


    {
        using namespace enhancement;
        Row hdr{kNames[kHdr], value(kHdr)};
        hdr.toggle = true;
        hdr.enabled = available(kHdr);
        hdr.note = available(kHdr) ? kNotes[kHdr]
                                   : "Needs an HDR display (on a Mac, its XDR screen or an HDR monitor).";
        hdr.change = [](int dir) { change(kHdr, dir); };
        g_rows.push_back(hdr);
        // Only where HDR can be on: elsewhere it would be one more dead row.
        if (available(kHdr)) {
            Row calibrate{"Calibrate HDR"};
            calibrate.enabled = g_settings.hdr;
            calibrate.value = std::to_string(g_settings.hdr_peak) + " nits peak";
            if (!g_settings.hdr)
                calibrate.note = "Turn HDR on to calibrate it.";
            calibrate.activate = [] { run_hdr_calibration(0); };
            g_rows.push_back(calibrate);
        }
    }

    Row music{"Music", std::to_string(g_settings.music_volume) + "%"};
    music.slider = g_settings.music_volume;
    music.section = "II \u00b7 Sound";
    music.change = [](int dir) {
        g_settings.music_volume = std::clamp(g_settings.music_volume + dir * 10, 0, 100);
    };
    g_rows.push_back(music);

    Row effects{"Effects & voices", std::to_string(g_settings.effects_volume) + "%"};
    effects.slider = g_settings.effects_volume;
    effects.change = [](int dir) {
        g_settings.effects_volume = std::clamp(g_settings.effects_volume + dir * 10, 0, 100);
    };
    g_rows.push_back(effects);

    // Graphics, on four pages of its own (build_graphics); HDR is with Display.
    auto graphics_rows = [](const char* section, std::initializer_list<int> list) {
        using namespace enhancement;
        bool first = true;
        for (int which : list) {
            Row r{kNames[which], value(which)};
            if (first)
                r.section = section;
            first = false;
            r.toggle = which == kHdTextures || which == kPerPixel || which == kSmaa || which == kContact ||
                       which == kSkin || which == kSpellRim || which == kSpellFx;
            if (which == kVolumetric)
                r.slider = g_settings.volumetric;
            r.enabled = available(which);
            r.note = kNotes[which];
            if (which == kHdTextures && !available(which))
                r.note = "No pack in the textures folder.";
            r.change = [which](int dir) { change(which, dir); };
            g_rows.push_back(r);
        }
    };

    graphics_rows("III \u00b7 Image", {enhancement::kHdTextures, enhancement::kSharpen});

    // Anti-aliasing, one choice: SMAA (a pass over the picture, at once) and 4x MSAA (the renderer's from its start,
    // main.c, so it waits for the next one) were two rows that read as the same thing. The same two settings as
    // before (smaa, anti_aliasing).
    static const char* const kAntiAliasing[] = {"Off", "SMAA", "4x MSAA", "4x MSAA + SMAA"};
    const int aa_mode = (g_settings.smaa ? 1 : 0) | (g_settings.anti_aliasing ? 2 : 0);
    Row aa{"Anti-aliasing", kAntiAliasing[aa_mode]};
    aa.note = (g_settings.anti_aliasing != 0) != (g_running_anti_aliasing != 0)
                  ? "MSAA changes from the next start."
                  : "SMAA smooths the whole picture at once. MSAA smooths edges more cleanly, from the next start.";
    aa.change = [](int dir) {
        int mode = (g_settings.smaa ? 1 : 0) | (g_settings.anti_aliasing ? 2 : 0);
        cycle(mode, 4, dir);
        g_settings.smaa = (mode & 1) != 0;
        g_settings.anti_aliasing = (mode & 2) != 0 ? 1 : 0;
        settings_apply_graphics();
        settings_save();
    };
    g_rows.push_back(aa);

    static const char* const kAnisotropy[] = {"1x", "2x", "4x", "8x", "16x"};
    Row af{"Texture filtering", kAnisotropy[g_settings.anisotropy]};
    if (g_settings.anisotropy != g_running_anisotropy)
        af.note = "From the next start.";
    af.change = [](int dir) {
        cycle(g_settings.anisotropy, 5, dir);
        settings_save();
    };
    g_rows.push_back(af);

    graphics_rows("VII \u00b7 Shadows", {enhancement::kShadows, enhancement::kCharShadows, enhancement::kShadowEdges,
                                         enhancement::kContact});
    graphics_rows("VIII \u00b7 Lighting", {enhancement::kPerPixel, enhancement::kSkin, enhancement::kOcclusion,
                                           enhancement::kOcclusionQuality});
    graphics_rows("IX \u00b7 Effects", {enhancement::kGlow, enhancement::kSpellRim, enhancement::kSpellFx, enhancement::kVolumetric,
                                        enhancement::kShafts});

    // (Controls and Mods are rows of Options itself, build_options: their sections here, IV and VI with its Disc row,
    // were never shown, and are gone.)
    static const char* const kCameraNotes[] = {"", "", "The right stick turns it.", "The right stick looks around."};
    Row camera{"Camera", camera_mode_name(g_settings.camera)};
    camera.experimental = true;
    camera.section = "I \u00b7 Display";  // Display's page (it was Game's, V)
    camera.page = 1;
    camera.note = kCameraNotes[g_settings.camera];
    camera.change = [](int dir) {
        static const int kModes[] = {CAMERA_ORIGINAL, CAMERA_THIRD_PERSON, CAMERA_FIRST_PERSON};
        int at = 0;
        for (int i = 0; i < 3; i++)
            if (kModes[i] == g_settings.camera)
                at = i;
        cycle(at, 3, dir);
        g_settings.camera = kModes[at];
    };
    g_rows.push_back(camera);

    // From Mods > Co-op (gone: Start Game asks Solo or Co-op). Shown always, so they can be set before a co-op game. In
    // a co-op game the split changes as the menu closes (g_skin_split: the page keeps its halves while it is up).
    static const char* const kSplit[] = {"Side by side", "Above and below"};
    Row split{"Co-op screen split", kSplit[g_settings.mod_split & 1]};
    split.page = 1;
    split.note = g_settings.mod_coop && in_play() ? "Changes when the menu closes." : "For co-op games.";
    split.change = [](int dir) { cycle(g_settings.mod_split, 2, dir); };
    g_rows.push_back(split);

    static const char* const kP2Camera[] = {"Third person", "First person", "Original"};
    Row p2_camera{"Player 2 camera", kP2Camera[std::clamp(g_settings.mod_p2_camera, 0, 2)]};
    p2_camera.page = 1;
    p2_camera.note = "Player 2's half in co-op. Original: the game's own cameras.";
    p2_camera.change = [](int dir) { cycle(g_settings.mod_p2_camera, 3, dir); };
    g_rows.push_back(p2_camera);

    static const char* const kGameFont[] = {"Sharp", "Tome", "Original"};
    Row font{"Game text", kGameFont[g_settings.game_font]};
    font.page = 1;
    font.note = "Sharp: redrawn crisply. Tome: old typefaces. Original: as on the disc.";
    font.change = [](int dir) { cycle(g_settings.game_font, 3, dir); };
    g_rows.push_back(font);

    Row fps{"FPS counter", on_off(g_settings.fps_counter)};
    fps.toggle = true;
    fps.page = 1;
    fps.change = [](int) { g_settings.fps_counter = !g_settings.fps_counter; };
    g_rows.push_back(fps);

    // The launcher's menu over the game is for testing: in play, the game's own menus hold everything (the Tome's
    // Options for player 1, the title screen's Options before a game).
    Row debug{"Debug menu in game", on_off(g_settings.debug_menu)};
    debug.toggle = true;
    debug.page = 1;
    debug.note = "Esc or Back opens this menu over the game. Off: settings are in the Tome's Options and the title's Options.";
    debug.change = [](int) { g_settings.debug_menu = !g_settings.debug_menu; };
    g_rows.push_back(debug);

    Row back{"Back"};
    back.gap = true;
    back.page = 1;
    back.activate = [] {
        settings_save();
        g_screen = g_options_return;
        g_selected = 0;
    };
    g_rows.push_back(back);
}

// Options: its sections, each a page of its own (Controls and Mods their
// own pages as before), then the disc and Back.
// The section page open: 1 Display, 2 Sound, 5 Game; Graphics' 3 Image, 7 Shadows, 8 Lighting, 9 Effects.
int g_options_section = 1;
int g_graphics_row = 0;  // the Graphics row to come back to

bool options_page_in_graphics() {
    return g_options_section == 3 || g_options_section >= 7;
}

void leave_options_page() {
    settings_save();
    g_screen = options_page_in_graphics() ? Screen::Graphics : Screen::Options;
    g_selected = options_page_in_graphics() ? g_graphics_row : g_options_row;
}

int section_number(const std::string& heading) {
    static const char* const kNumerals[] = {"I ", "II ", "III ", "IV ", "V ", "VI ", "VII ", "VIII ", "IX "};
    for (int n = 9; n >= 1; n--)
        if (heading.rfind(kNumerals[n - 1], 0) == 0)
            return n;
    return 0;
}

// One section's rows, from the full list (build_options_rows), on one page.
void build_options_page() {
    std::vector<Row> all;
    std::swap(all, g_rows);
    build_options_rows();
    std::swap(all, g_rows);
    int section = 0;
    for (Row& r : all) {
        if (!r.section.empty())
            section = section_number(r.section);
        if (section != g_options_section || r.label == "Back")
            continue;
        r.section.clear();
        r.page = 0;
        g_rows.push_back(r);
    }
    Row back{"Back"};
    back.gap = true;
    back.activate = leave_options_page;
    g_rows.push_back(back);
}

const char* options_page_name() {
    switch (g_options_section) {
    case 1: return "Display";
    case 2: return "Sound";
    case 3: return "Image";
    case 7: return "Shadows";
    case 8: return "Lighting";
    case 9: return "Effects";
    default: return "Game";
    }
}

// Graphics: its groups, each a short page of its own, then Back.
void build_graphics() {
    auto page = [](const char* label, int section) {
        Row r{label};
        r.activate = [section] {
            g_options_section = section;
            g_graphics_row = g_selected;
            g_screen = Screen::OptionsPage;
            g_selected = 0;
        };
        return r;
    };
    g_rows.push_back(page("Image", 3));
    g_rows.push_back(page("Shadows", 7));
    g_rows.push_back(page("Lighting", 8));
    g_rows.push_back(page("Effects", 9));
    Row back{"Back"};
    back.gap = true;
    back.activate = [] {
        settings_save();
        g_screen = Screen::Options;
        g_selected = g_options_row;
    };
    g_rows.push_back(back);
}

void build_options() {
    auto page = [](const char* label, int section) {
        Row r{label};
        r.activate = [section] {
            g_options_section = section;
            g_options_row = g_selected;
            g_screen = Screen::OptionsPage;
            g_selected = 0;
        };
        return r;
    };
    g_rows.push_back(page("Display", 1));
    g_rows.push_back(page("Sound", 2));
    Row graphics{"Graphics"};
    graphics.experimental = true;
    graphics.activate = [] {
        g_options_row = g_selected;
        g_screen = Screen::Graphics;
        g_selected = 0;
    };
    g_rows.push_back(graphics);
    Row controls{"Controls"};
    controls.activate = [] {
        g_options_row = g_selected;
        controls::set_menu_player(0);
        open_pick(PickKind::Controls, Screen::Options, g_selected);  // Player I / Player II first
    };
    g_rows.push_back(controls);
    // Camera, Game text and FPS counter are on Display (their rows come under its heading in build_options_rows), and
    // Mods is on the launcher and the in-game menu: neither is here (the user's choice, 2026-11-07).

    if (g_options_return == Screen::Launcher && !g_disc_on_main) {
        Row disc = disc_row();
        disc.gap = true;
        g_rows.push_back(disc);
    }

    // Opened from the game's own menus (the debug menu off, these are the launcher's rows' only home in play): from the
    // title's More Options, Mods too (Challenge and the title video, before a game); from the Tome's Settings, the way back
    // to the title screen. Quit from either.
    if (g_root_closes && g_root == Screen::Options) {
        if (g_on_title) {
            Row mods = mods_row(Screen::Options);
            mods.gap = true;
            g_rows.push_back(mods);
        } else {
            Row title{"Return to title screen"};
            title.gap = true;
            title.note = "Unsaved progress is lost.";
            title.enabled = quickstate_boot_path() != nullptr;
            title.activate = [] {
                settings_save();
                hle_set_skip_intro(false);
                quickstate_request_load(quickstate_boot_path());
                g_close = true;
            };
            g_rows.push_back(title);
        }
        Row quit{"Quit game"};
        quit.gap = !g_on_title;
        quit.note = g_on_title ? "" : "Unsaved progress is lost.";
        quit.activate = [] {
            settings_save();
            dol_aurora_request_quit();
            g_close = true;
        };
        g_rows.push_back(quit);
    }

    Row back{"Back"};
    back.gap = true;
    back.activate = [] {
        settings_save();
        g_screen = g_options_return;
        g_selected = 0;
    };
    g_rows.push_back(back);
}

void build_controls_mapping_and_back();

void build_controls() {
    const bool p2 = controls::menu_player() == 1;
    Row device = device_row();
    if (!p2)
        device.section = "Device";
    g_rows.push_back(device);
    if (controls::player_has_controller(controls::menu_player())) {
        g_rows.push_back(controller_row());
        g_rows.push_back(pad_type_row());
    }

    g_rows.push_back(layout_row());

    // Hold or toggle, deadzone and vibration: player 1's for now.
    if (p2) {
        build_controls_mapping_and_back();
        return;
    }

    auto hold_toggle = [](const char* label, bool* value, const char* note) {
        Row r{label, *value ? "Toggle" : "Hold"};
        r.note = note;
        r.change = [value](int) {
            *value = !*value;
            settings_save();
        };
        return r;
    };
    Row sprint = hold_toggle("Sprint", &g_settings.sprint_toggle[controls::layout()],
                             "Toggle: press to run, press again or stop moving to walk.");
    sprint.section = "Handling";
    g_rows.push_back(sprint);
    g_rows.push_back(hold_toggle("Creep", &g_settings.creep_toggle[controls::layout()],
                                 "Toggle: press to creep, press again (or sprint) to stop."));
    g_rows.push_back(hold_toggle("Target", &g_settings.target_toggle[controls::layout()],
                                 "Toggle: press to lock on, press again to release."));

    Row deadzone{"Stick deadzone",
                 std::to_string(g_settings.stick_deadzone[controls::layout()]) + "%"};
    deadzone.note = "Raise it if a stick drifts.";
    deadzone.change = [](int dir) {
        int& dz = g_settings.stick_deadzone[controls::layout()];
        dz = std::clamp(dz + dir * 5, 5, 25);
        settings_save();
    };
    g_rows.push_back(deadzone);

    g_rows.push_back(vibration_row());
    build_controls_mapping_and_back();
}

void build_controls_mapping_and_back() {
    Row mapping{"Button mapping"};
    mapping.gap = true;  // its own group, so it does not trail the settings
    mapping.activate = [] {
        g_screen = Screen::Mapping;
        g_selected = 0;
    };
    g_rows.push_back(mapping);

    Row back{"Back"};
    back.gap = true;
    back.activate = [] {
        controls::set_menu_player(0);
        g_screen = g_controls_return;
        g_selected = g_controls_return_row;
    };
    g_rows.push_back(back);
}

// The menu player's bindings: the controller column and the key column for
// the devices they play with, then (with a keyboard) the movement and camera
// keys on the right-hand page.
void build_mapping() {
    const int player = controls::menu_player();
    const bool pad = controls::player_has_controller(player);
    // Keys for a player whose device takes them (saved, though in co-op
    // player 2 may have the keyboard).
    const bool keys = controls::player_uses_keyboard(player);
    const std::string waiting_for = pad && keys ? "a controller button, trigger or key"
                                    : pad       ? "a controller button or trigger"
                                                : "a key";
    const std::string layout = pad ? controls::layout_name(controls::layout())
                               : controls::layout() == controls::kCustom ? "Custom"
                                                                         : "Default";
    // Left/right picks by hand (no controller needed): the controller button
    // where the player has one, else the key. A waits for a press.
    const std::string how = pad ? (keys ? "Left/Right: choose the controller button. A: press a button or key."
                                        : "Left/Right: choose the button. A: press it on the controller.")
                                : "Left/Right: choose the key. A: press it.";
    for (int a = 0; a < controls::kActionCount; a++) {
        Row r{controls::action_name(a)};
        if (a == 0 && keys)
            r.section = "Actions";
        if (controls::capturing() && controls::capture_action() == a) {
            r.value = "Press " + waiting_for + "...";
            r.note = "Waiting for " + waiting_for + " (Esc cancels).";
        } else {
            r.value_rml = pad && keys ? controls::pad_glyphs(a) + "<span class=\"sep\"></span>" +
                                            controls::key_glyph(a)
                          : pad ? controls::pad_glyphs(a)
                                : controls::key_glyph(a);
            r.note = how + " Layout: " + layout + ".";
        }
        r.activate = [a] { controls::begin_capture(a); };
        r.change = pad ? std::function<void(int)>([a](int dir) { controls::step_pad(a, dir); })
                       : std::function<void(int)>([a](int dir) { controls::step_key(a, dir); });
        g_rows.push_back(r);
    }
    for (int d = 0; keys && d < controls::kDirectionCount; d++) {
        const int target = controls::kActionCount + d;
        Row r{controls::direction_name(d)};
        if (d == 0)
            r.section = "Movement and camera";
        r.page = 1;
        if (controls::capturing() && controls::capture_action() == target) {
            r.value = "Press a key...";
            r.note = "Waiting for a key (Esc cancels).";
        } else {
            r.value_rml = controls::direction_glyph(d);
            r.note = std::string("Left/Right: choose the key. A: press it.") +
                     (pad ? " A controller moves and turns the camera with its sticks." : "");
        }
        r.activate = [target] { controls::begin_capture(target); };
        r.change = [target](int dir) { controls::step_key(target, dir); };
        g_rows.push_back(r);
    }

    Row back{"Back"};
    back.gap = true;
    back.page = keys ? 1 : 0;
    back.activate = [] {
        g_screen = Screen::Controls;
        g_selected = 0;
    };
    g_rows.push_back(back);
}

void build_in_game() {
    Row resume{"Resume"};
    resume.activate = [] { g_close = true; };
    g_rows.push_back(resume);

    Row states{"Save states"};
    states.gap = true;
    states.activate = [] {
        g_screen = Screen::States;
        // At the save made last (the user's choice, 2026-11-07); none yet, the first.
        g_selected = std::max(newest_slot() - 1, 0);
    };
    g_rows.push_back(states);

    Row options{"Options"};
    options.gap = true;
    options.activate = [] {
        dol_aurora_measure_screen();
        g_options_return = Screen::InGame;
        g_screen = Screen::Options;
        g_selected = 0;
    };
    g_rows.push_back(options);
    g_rows.push_back(mods_row(Screen::InGame));

    // A reset: the machine as it booted, so the game starts again through its
    // logos to the title screen (the launcher's New game, in place).
    Row title{"Return to title screen"};
    title.gap = true;
    title.note = "Unsaved progress is lost.";
    title.enabled = quickstate_boot_path() != nullptr;
    title.activate = [] {
        settings_save();
        hle_set_skip_intro(false);
        quickstate_request_load(quickstate_boot_path());
        g_close = true;
    };
    g_rows.push_back(title);

    Row launcher{"Return to launcher"};
    launcher.note = "Unsaved progress is lost.";
    launcher.activate = [] {
        settings_save();
        // The launcher in this window, over the paused game; without a boot
        // state for New game, start the program again instead.
        if (quickstate_boot_path() == nullptr) {
            restart_process();
            return;
        }
        g_screen = Screen::Launcher;
        g_options_return = Screen::Launcher;
        g_selected = launcher_main_row();
        g_launcher_row = -1;  // from a game: Main menu, as at start
    };
    g_rows.push_back(launcher);

    Row quit{"Quit"};
    quit.note = "Unsaved progress is lost.";
    quit.activate = [] {
        settings_save();
        dol_aurora_request_quit();
        g_close = true;
    };
    g_rows.push_back(quit);
}

// The problem page (frontend_problem).
std::string g_problem_heading;
std::string g_problem_text;
bool g_problem_can_wait = false;
bool g_problem_wait = false;

void build_problem() {
    if (g_problem_can_wait) {
        Row wait{"Keep waiting"};
        wait.note = g_problem_text;
        wait.activate = [] {
            g_problem_wait = true;
            g_close = true;
        };
        g_rows.push_back(wait);
    }
    Row quit{"Quit"};
    quit.note = g_problem_text;
    quit.activate = [] {
        g_problem_wait = false;
        g_close = true;
    };
    g_rows.push_back(quit);
}

// The frames' end, then the panel: the highlighted character's details (the
// chosen one's while a row under the frames is).
// The lines in view: the highlighted frame's line among them (when a frame is), and no blank line at the end.
int roster_lines() {
    return (roster_count() + roster_columns() - 1) / roster_columns();
}
void roster_keep_in_view() {
    if (g_selected < roster_count()) {
        const int line = g_selected / roster_columns();
        if (line < g_roster_top)
            g_roster_top = line;
        else if (line >= g_roster_top + kRosterLines)
            g_roster_top = line - kRosterLines + 1;
    }
    g_roster_top = std::clamp(g_roster_top, 0, std::max(roster_lines() - kRosterLines, 0));
}

// The mouse wheel over the page: the frames move a line, and the highlight with them if it's left behind.
void roster_wheel(int dy) {
    g_roster_top = std::clamp(g_roster_top + dy, 0, std::max(roster_lines() - kRosterLines, 0));
    if (g_selected < roster_count()) {
        const int line = g_selected / roster_columns();
        const int to = std::clamp(line, g_roster_top, g_roster_top + kRosterLines - 1);
        g_selected = std::min(g_selected + (to - line) * roster_columns(), roster_count() - 1);
    }
}

// A page of two cards (rows with card 1 and 2): Choose Character's, and the two-card page (Solo / Co-op, Player I / II).
bool card_screen() {
    return (g_screen == Screen::ModsPage && g_mods_section == 3) || g_screen == Screen::Pick;
}

// The hand-made list scroll (g_list_top, g_list_lines): on this screen, now?
// Not on a two-column spread (a row with page 1: Button Mapping's keys, the Mods rows): the window is one range of
// rows across both columns and the marks go in the left one, so the right column would empty with no mark.
bool list_scrolls() {
    if (g_list_lines <= 0 || g_screen == Screen::Characters || g_screen == Screen::States || g_screen == Screen::Play ||
        card_screen())
        return false;
    for (const Row& r : g_rows)
        if (r.page == 1)
            return false;
    return true;
}
// The rows in view: the highlighted one among them, and no blank lines at the end.
void list_keep_in_view() {
    if (!list_scrolls())
        return;
    const int n = static_cast<int>(g_rows.size());
    if (g_selected < g_list_top)
        g_list_top = g_selected;
    else if (g_selected >= g_list_top + g_list_lines)
        g_list_top = g_selected - g_list_lines + 1;
    g_list_top = std::clamp(g_list_top, 0, std::max(n - g_list_lines, 0));
}
// The wheel or the right stick: the rows move one, and the highlight with them if it's left behind (to the nearest
// row in view that can be chosen).
void list_wheel(int dy) {
    const int n = static_cast<int>(g_rows.size());
    g_list_top = std::clamp(g_list_top + dy, 0, std::max(n - g_list_lines, 0));
    const int last = std::min(g_list_top + g_list_lines, n) - 1;
    if (g_selected >= g_list_top && g_selected <= last)
        return;
    const int from = g_selected < g_list_top ? g_list_top : last, step = g_selected < g_list_top ? 1 : -1;
    for (int i = from; i >= g_list_top && i <= last; i += step)
        if (g_rows[i].enabled || g_rows[i].change) {
            g_selected = i;
            return;
        }
}

// The title look (Skin::Title): the title's Options picture with its words erased (titlevideo_title_art writes it as
// res/video/options-clean.png) laid where the game lays it, and a list page in the picture's word column.
struct WinRect {
    float x, y, w, h;
};
// The game's picture in the window (as gpu.cpp calculate_present_viewport), and whether the game's 2D is wide (its
// projection -80..720 across, 0..640 in 4:3: hle.c sync_widescreen_option follows the launcher's aspect).
WinRect game_viewport(bool* wide) {
    float aspect = 16.f / 9.f;
    uint32_t W = 0, H = 0;
    dol_aurora_get_display_info(&aspect, &W, &H);
    if (W == 0 || H == 0) {
        W = 1280;
        H = 720;
    }
    if (!(aspect > 0.f))
        aspect = 16.f / 9.f;
    float vw = static_cast<float>(W), vh = std::round(W / aspect);
    if (vh > H) {
        vh = static_cast<float>(H);
        vw = std::round(H * aspect);
    }
    *wide = aspect > 1.5f;
    return {(W - vw) / 2.f, (H - vh) / 2.f, vw, vh};
}
// The title's 850x480 pictures in the window: the game's quad for them is x -106..745, y 0..480 of its 2D units.
// At 21:9 (aspect 4) the renderer keeps the game's 2D at its 16:9 shape, narrowed about the middle with black at the
// sides (aurora_backend.cpp set_wide_scale, gxcore_draw.cpp wide_scale): its 800 units span a 16:9 box in the middle.
WinRect title_picture_rect() {
    bool wide = true;
    const WinRect v = game_viewport(&wide);
    const float left = wide ? -80.f : 0.f, units = wide ? 800.f : 640.f;
    const float span = wide && g_settings.aspect == 4 ? std::min(v.w, std::round(v.h * 16.f / 9.f)) : v.w;
    const float k = span / units;
    return {v.x + (v.w - span) / 2.f + (-106.f - left) * k, v.y, 851.f * k, v.h};
}
// A box of that picture (850x480, upright) in the window.
WinRect title_box(float x0, float y0, float x1, float y1) {
    const WinRect p = title_picture_rect();
    return {p.x + x0 / 850.f * p.w, p.y + y0 / 480.f * p.h, (x1 - x0) / 850.f * p.w, (y1 - y0) / 480.f * p.h};
}
// The Tome look (Skin::Tome): the page laid on the Tome's own page. The Tome draws in 640x480 units, its page picture
// at x 144..576, y 0..480 in both layouts; its 2D is -80..720 across in the wide layout (r13-24660 1, which follows
// the launcher's aspect), 0..640 in 4:3 (docs/research/in-game-menu/11-tome-pages.md 2.2, 12 1). At 21:9 the 800 units
// span a 16:9 box in the middle, as the title's. half -1: the whole picture; 0 or 1: that player's co-op half, the
// picture fitted in it (aurora.cpp half_of and the menu's fit; co-op is not placed yet, see step 6).
WinRect tome_units_box(int half, float u0, float v0, float u1, float v1) {
    bool wide = true;
    WinRect v = game_viewport(&wide);
    if (half >= 0) {
        float aspect = 16.f / 9.f;
        uint32_t W = 0, H = 0;
        dol_aurora_get_display_info(&aspect, &W, &H);
        if (W == 0 || H == 0) {
            W = 1280;
            H = 720;
        }
        const bool side = g_skin_split != 1;
        const float hx = side ? W * 0.5f * half : 0.f, hy = side ? 0.f : H * 0.5f * half;
        const float hw = side ? W * 0.5f : static_cast<float>(W), hh = side ? static_cast<float>(H) : H * 0.5f;
        const float s = std::min(hw / v.w, hh / v.h);
        v.w *= s;
        v.h *= s;
        v.x = hx + (hw - v.w) / 2.f;
        v.y = hy + (hh - v.h) / 2.f;
    }
    const float left = wide ? -80.f : 0.f, units = wide ? 800.f : 640.f;
    const float span = wide && g_settings.aspect == 4 ? std::min(v.w, std::round(v.h * 16.f / 9.f)) : v.w;
    const float k = span / units, x0 = v.x + (v.w - span) / 2.f;
    return {x0 + (u0 - left) * k, v.y + v0 / 480.f * v.h, (u1 - u0) * k, (v1 - v0) / 480.f * v.h};
}
// The Tome's page in the window.
WinRect tome_page_rect(int half) {
    return tome_units_box(half, 144.f, 0.f, 576.f, 480.f);
}
// Our leaf on that page, in the Tome's units: clear of the medallion on the left (it reaches about x 160) and of the
// "RB Inventory" hint at the foot of the right side (from about y 390), with the page's art showing round it, as the
// game lays its own Load/Save panel on the page. Its letters a part of the page's height: every size in the Tome
// look's rcss is in em of it. kTomeLines rows show at a time; the rest scroll.
constexpr float kTomeLeaf[4] = {166.f, 10.f, 554.f, 384.f};
constexpr float kTomeFont = 0.036f;
constexpr int kTomeLines = 9;
// Whether a co-op Tome page goes in its owner's half (g_skin_half): the renderer keeps the halves under it (hle.c
// hle_coop_ui_over, aurora.cpp aurora_split_page), their Tome in theirs. g_skin_half is -1 when they are not kept.
constexpr bool kTomeInHalves = true;
// Whether the Tome is the picture under a Tome page, so the page lies on it with no veil (body.tome-under): solo the
// whole picture is the Tome; in co-op the owner's half is their Tome (kTomeInHalves). Without the halves kept, a co-op
// page is over one whole picture, which may be the world (s5, run -094534): there it keeps the dim veil and its leaf in
// the middle.
bool tome_under_page() {
    return g_skin == Skin::Tome && (kTomeInHalves || g_skin_half < 0 || g_skin_half == 1);
}
// A co-op half of the window (as aurora.cpp half_of): 0 or 1, side by side or one above the other (the page's split,
// g_skin_split).
WinRect window_half(int half) {
    float aspect = 16.f / 9.f;
    uint32_t W = 0, H = 0;
    dol_aurora_get_display_info(&aspect, &W, &H);
    if (W == 0 || H == 0) {
        W = 1280;
        H = 720;
    }
    const bool side = g_skin_split != 1;
    return {side ? W * 0.5f * half : 0.f, side ? 0.f : H * 0.5f * half, side ? W * 0.5f : static_cast<float>(W),
            side ? static_cast<float>(H) : H * 0.5f};
}
// The leaf's shift with no Tome under it: to the middle of the -80..720 picture (the leaf's middle is at 360).
constexpr float kTomeLeafCentre = -40.f;

// The list page's box: the column where the game paints its Options rows (x 170..440, y 85..470), a little wider to
// the left for the scroll marks and to the right for the values (the skull's face starts near x 490), and its letters'
// size, a part of the picture's height (every size in the title look's list rcss is in em of it). kTitleLines rows show
// at a time; the rest scroll.
constexpr float kTitleBox[4] = {150.f, 56.f, 480.f, 476.f};
constexpr float kTitleFont = 0.05f;
constexpr int kTitleLines = 9;

// Writes the title look's pictures (once a run, tried again at each title page until the picture is kept): the clean
// Options picture and the scroll marks.
void ensure_title_art() {
    static bool done = false;
    if (done)
        return;
    const bool ok = titlevideo_title_art(res_dir() + "/video");
    done = ok;
    static bool told = false;
    if (ok || !told)
        std::fprintf(stderr, "[frontend] the title look's picture %s\n",
                     ok ? "written (res/video/options-clean.png)" : "NOT written (no Options picture kept yet): black behind");
    told = true;
}
// The title look's wide pages keep their own places on a dark panel: the roster, the two cards, Button Mapping (with or
// without its right-hand keys) and any page with a right-hand column. The rest are lists in the picture's word column.
bool title_wide_page() {
    if (g_screen == Screen::Characters || card_screen() || g_screen == Screen::Mapping)
        return true;
    for (const Row& r : g_rows)
        if (r.page == 1 && r.card == 0 && !r.tile)
            return true;
    return false;
}
// Whether res/video/options-clean.png is there to show. The page asks RmlUi for it (body class title-art) only then:
// RmlUi never tries a picture again once it failed to load, so asking before it is written would leave it black all run.
// Whether the page as last built shows the HD backdrop (render): the menu loop builds it again once the backdrop is
// written after it was built (HD textures turned on on the page itself: titlevideo_title_art_hd starts the read).
bool g_art_hd_shown = false;
bool title_art_ready() {
    static bool ready = false;
    if (!ready) {
        std::error_code ec;
        ready = std::filesystem::is_regular_file(std::filesystem::path(res_dir()) / "video" / "options-clean.png", ec);
    }
    return ready;
}

// The title look made ready ahead, a piece a frame, on the title once its Options picture is kept: a title page is live
// (the title's music goes on under it), and its first open did all this in one frame, a gap of about 100 ms in the music
// ("[audio] ran dry"). Writing the clean picture (about 25 ms) and RmlUi's load of the backdrop it will show (the HD one
// about 30 ms) each fit in a frame's sound reserve. A picture is only asked for once it is there: RmlUi never tries one
// again after a failed load (title_art_ready). The key is the stylesheet's own path, so the decorator finds it loaded.
void warm_title_look() {
    static int stage = -1;
    if (stage > 2 || g_doc == nullptr || (stage >= 0 && !titlevideo_options_kept()))
        return;  // (the layout at once, at the start; the pictures once the title's Options picture is kept)
    const Uint64 t0 = SDL_GetTicksNS();
    auto load = [](const char* file) {
        Rml::Context* context = aurora::rmlui::get_context();
        if (context == nullptr)
            return;
        const Rml::Texture texture = context->GetRenderManager().LoadTexture(std::string("../video/") + file, res_dir() + "/ui/menu.rcss");
        (void)texture.GetDimensions();  // (loads it)
    };
    const char* did = nullptr;
    if (stage == -1) {
        // The menu's first Show lays it all out (about 40 ms, when no launcher has shown it): shown and hidden at once,
        // in this one call, so nothing is drawn.
        if (!g_doc->IsVisible()) {
            g_doc->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
            g_doc->Hide();
            did = "the menu laid out";
        }
        stage = 0;
    } else if (stage == 0) {
        ensure_title_art();
        did = "the clean picture written";
        stage = 1;
    } else if (stage == 1) {
        if (title_art_ready()) {
            load("options-clean.png");
            did = "the clean picture loaded";
        }
        stage = 2;
    } else if (!g_settings.hd_textures) {
        stage = 3;  // (turned on later: the page loads it itself, as before)
    } else if (titlevideo_title_art_hd()) {
        load("options-clean-hd.png");
        did = "the HD backdrop loaded";
        stage = 3;
    }
    if (did != nullptr)
        std::fprintf(stderr, "[frontend] the title look ahead of its first page: %s (%.1f ms)\n", did, (SDL_GetTicksNS() - t0) / 1e6);
}

std::string roster_panel_end() {
    const RosterTile shown = g_selected < roster_count() ? roster_tiles()[g_selected]
                                                          : RosterTile{chosen_character(), form_on(g_characters_player, chosen_character())};
    // The scroll bar: the lines in view, of all of them.
    const int lines = roster_lines();
    const std::string bar = "<div class=\"roster-bar\"><img class=\"roster-cap\" src=\"tome/mark.png\"/><div class=\"roster-rail\">"
                            "<div class=\"roster-track\"></div><div class=\"roster-thumb\" style=\"top: " +
                            std::to_string(g_roster_top * 100 / lines) + "%; height: " + std::to_string(std::min(kRosterLines, lines) * 100 / lines) +
                            "%;\"></div></div><img class=\"roster-cap bottom\" src=\"tome/mark.png\"/></div>";
    return "</div>" + bar + "</div><div class=\"roster-details\">" +
           (g_skin == Skin::Tome ? character_strip_rml(shown) : character_details_rml(shown)) + "</div></div>";
}

// A row's control, right of its name: a switch, a bar or a value with arrows
// round it where left and right change it, and an arrow where it opens a page.
std::string row_control(const Row& r, bool plain, bool opens) {
    if (!r.value_rml.empty())
        return r.value_rml;
    if (r.toggle && (r.value == "On" || r.value == "Off"))
        return std::string("<span class=\"switch") + (r.value == "On" ? " on" : "") +
               "\"><span class=\"knob\"></span></span>";
    const std::string prev = "<span class=\"chev prev\">\u2039</span>";
    const std::string next = "<span class=\"chev next\">\u203a</span>";
    if (r.slider >= 0)
        return prev + "<span class=\"slider\"><span class=\"slider-fill\" style=\"width: " +
               std::to_string(r.slider) + "%;\"></span></span><span class=\"slider-value\">" + escape(r.value) +
               "</span>" + next;
    if (!plain && r.change && (r.enabled || r.browse))
        return prev + "<span class=\"pick\">" + escape(r.value) + "</span>" + next;
    std::string out;
    if (!r.value.empty())
        out = "<span>" + escape(r.value) + "</span>";
    if (opens)
        out += "<span class=\"chev\">\u203a</span>";
    return out;
}

void render() {
    if (g_doc == nullptr)
        return;
    const std::string slot_heading = "Slot " + std::to_string(g_slot_chosen);
    const char* heading = g_screen == Screen::Launcher   ? ""
                          : g_screen == Screen::Play     ? "Load Game"
                          : g_screen == Screen::States   ? "Save States"
                          : g_screen == Screen::Slot     ? slot_heading.c_str()
                          : g_screen == Screen::SlotDelete ? "Delete Save?"
                          : g_screen == Screen::SlotOverwrite ? "Do you want to overwrite this save?"
                          : g_screen == Screen::Options  ? (g_root_closes ? (g_on_title ? "More Options" : "Settings") : "Options")
                          : g_screen == Screen::OptionsPage ? options_page_name()
                          : g_screen == Screen::Graphics ? "Graphics"
                          : g_screen == Screen::Controls ? (controls::menu_player() == 1
                                                                ? "Player 2 Controls"
                                                                : "Controls")
                          : g_screen == Screen::Mapping  ? (controls::menu_player() == 1
                                                                ? "Player 2 Mapping"
                                                                : "Button Mapping")
                          : g_screen == Screen::Mods     ? "Mods"
                          : g_screen == Screen::ModsPage ? mods_page_name()
                          : g_screen == Screen::Characters ? (g_characters_player == 1 ? "Player 2 Character"
                                                                                       : "Player 1 Character")
                          : g_screen == Screen::Problem  ? g_problem_heading.c_str()
                          : g_screen == Screen::QuitAsk  ? "Quit the game?"
                          : g_screen == Screen::Pick     ? (g_pick_kind == PickKind::Controls ? "Controls"
                                                            : g_pick_title_row == 1           ? "Load Game"
                                                                                              : "Start Game")
                                                         : "Paused";
    g_doc->GetElementById("heading")->SetInnerRML(heading);
    std::string rml[2];
    bool spread = false;
    bool in_roster = false;
    // A list of slots shows ten at a time, the chosen one's, with Back under them.
    const bool slot_list = g_screen == Screen::States || g_screen == Screen::Play;
    // The plain lists of actions: their rows centred, no controls.
    const bool centered = g_screen == Screen::Slot || g_screen == Screen::SlotDelete ||
                          g_screen == Screen::SlotOverwrite || g_screen == Screen::Problem ||
                          g_screen == Screen::QuitAsk;
    const bool settings_page = !centered && !slot_list && g_screen != Screen::Launcher && g_screen != Screen::InGame &&
                               g_screen != Screen::Characters;
    const int slot_rows = static_cast<int>(g_rows.size()) - 1;
    if (slot_list && g_selected < slot_rows)
        g_slots_page = g_selected / kSlotsShown;
    if (slot_list)
        g_slots_page = std::clamp(g_slots_page, 0, std::max(slot_rows - 1, 0) / kSlotsShown);
    // A list that scrolls: the rows in view, a mark above and below where rows are hidden. In the title look the marks
    // are the painted list's chevrons, beside the first and the last row in view (as the game's list has them).
    const bool scrolls = list_scrolls();
    // (A wide page on the title's dark panel keeps the launcher's marks: the chevrons are sized for the word column.)
    const bool title_marks = scrolls && g_skin == Skin::Title && !title_wide_page();
    if (scrolls) {
        list_keep_in_view();
        if (g_list_top > 0 && !title_marks)
            rml[0] += "<div class=\"list-more up\"><img src=\"tome/mark.png\"/></div>";
    }
    const int list_end = std::min(g_list_top + g_list_lines, static_cast<int>(g_rows.size()));
    for (size_t i = 0; i < g_rows.size(); i++) {
        const Row& r = g_rows[i];
        if (slot_list && static_cast<int>(i) < slot_rows && static_cast<int>(i) / kSlotsShown != g_slots_page)
            continue;
        if (scrolls && r.card == 0 && !r.tile &&
            (static_cast<int>(i) < g_list_top || static_cast<int>(i) >= g_list_top + g_list_lines))
            continue;
        if (r.card != 0) {
            if (r.card == 1)
                rml[0] += "<div class=\"pcards\">";
            rml[0] += g_screen == Screen::Pick ? pick_card_rml(r.card - 1, static_cast<int>(i) == g_selected, i)
                                               : player_card_rml(r.card - 1, static_cast<int>(i) == g_selected, i);
            rml[0] += r.card == 1 ? "<div class=\"pcards-mid\"><div class=\"pcards-line\"></div><img class=\"pcards-mark\" "
                                    "src=\"tome/mark.png\"/><div class=\"pcards-line\"></div></div>"
                                  : "</div>";
            continue;
        }
        if (r.tile) {
            // A portrait frame and the name and form under it, four to a line, kRosterLines of them in view.
            if (!in_roster)
                roster_keep_in_view();
            const int line = static_cast<int>(i) / roster_columns();
            if (line < g_roster_top || line >= g_roster_top + kRosterLines) {
                if (!in_roster)
                    rml[0] += "<div class=\"roster-wrap\"><div class=\"roster-scroll\"><div class=\"roster\">";
                in_roster = true;
                continue;
            }
            if (!in_roster)
                rml[0] += "<div class=\"roster-wrap\"><div class=\"roster-scroll\"><div class=\"roster\">";
            in_roster = true;
            rml[0] += "<div class=\"tile" + std::string(roster_tiles()[i].character == 0 ? " default" : "") +
                      std::string(static_cast<int>(i) == g_selected ? " selected" : "") +
                      (r.chosen ? " chosen" : "") + "\" id=\"row" + std::to_string(i) +
                      "\"><div class=\"portrait\">" + r.value_rml + "</div><div class=\"name\">" + escape(r.label) +
                      "</div><div class=\"tile-form\">" +
                      escape(tile_form_name(roster_tiles()[i])) +
                      "</div></div>";
            continue;
        }
        if (in_roster)
            rml[0] += roster_panel_end();
        in_roster = false;
        const int page = r.page == 1 ? 1 : 0;
        spread = spread || page == 1;
        std::string& out = rml[page];
        if (!r.section.empty())
            out += "<div class=\"section\">" + escape(r.section) + "</div>";
        else if (r.gap)
            out += "<div class=\"gap\"></div>";
        out += "<div class=\"row";
        if (static_cast<int>(i) == g_selected)
            out += " selected";
        if (!r.enabled)
            out += " disabled";
        if (r.chosen)
            out += " chosen";
        if (r.big)
            out += " big";
        const bool is_back = r.label == "Back";
        if (is_back)
            out += " back";
        if (r.experimental)
            out += " tagged";
        // On the settings pages, a row that opens another page says so.
        const bool opens = settings_page && !is_back && r.enabled && r.activate && !r.change;
        const std::string control = row_control(r, slot_list, opens);
        // What the value is, so a settings page lines its column up (menu.rcss, body.settings).
        const char* kind = r.toggle && (r.value == "On" || r.value == "Off")       ? " toggle"
                           : r.slider >= 0                                          ? " level"
                           : !slot_list && r.change && (r.enabled || r.browse)      ? " choice"
                           : opens && r.value.empty() && r.value_rml.empty()        ? " opens"
                                                                                    : " plain";
        if (std::strcmp(kind, " opens") == 0)
            out += " opener";  // (the title look: its arrow right after the name)
        out += "\" id=\"row" + std::to_string(i) + "\">";
        if (title_marks && static_cast<int>(i) == g_list_top && g_list_top > 0)
            out += "<img class=\"row-mark\" src=\"../video/mark-up.png\"/>";
        else if (title_marks && static_cast<int>(i) == list_end - 1 && list_end < static_cast<int>(g_rows.size()))
            out += "<img class=\"row-mark\" src=\"../video/mark-down.png\"/>";
        out += "<span class=\"label\">" + escape(r.label) + "</span>";
        if (r.experimental)
            out += "<span class=\"tag\">(Experimental, expect glitches)</span>";
        if (!control.empty())
            out += "<span class=\"leader\"></span><span class=\"value" + std::string(kind) + "\">" + control + "</span>";
        out += "</div>";
    }
    if (in_roster)
        rml[0] += roster_panel_end();
    if (scrolls && !title_marks && g_list_top + g_list_lines < static_cast<int>(g_rows.size()))
        rml[0] += "<div class=\"list-more down\"><img src=\"tome/mark.png\"/></div>";
    g_doc->GetElementById("menu0")->SetInnerRML(rml[0]);
    g_doc->GetElementById("menu1")->SetInnerRML(rml[1]);
    const bool have = g_selected < static_cast<int>(g_rows.size());
    // (The title look's list has no room for a row's experimental tag: its help says it instead.)
    std::string note = have ? g_rows[g_selected].note : "";
    if (have && g_skin != Skin::Launcher && g_rows[g_selected].experimental)
        note = "Experimental, expect glitches. " + note;
    g_doc->GetElementById("note")->SetInnerRML(escape(note));
    g_doc->GetElementById("hint")->SetInnerRML(
        g_screen == Screen::InGame    ? "Up/Down choose &#183; Left/Right change &#183; A select &#183; B resume"
        : g_screen == Screen::Problem ? "Up/Down choose &#183; A select"
        : g_screen == Screen::QuitAsk ? "Up/Down choose &#183; A select &#183; B back"
        : g_screen == Screen::Characters ? "Arrows choose &#183; Right stick scrolls &#183; A select &#183; B back"
        : g_screen == Screen::ModsPage && g_mods_section == 3 ? "Left/Right choose the player &#183; A choose their character &#183; B back"
        : g_screen == Screen::Pick && g_pick_kind == PickKind::Controls ? "Left/Right choose the player &#183; A their controls &#183; B back"
        : g_screen == Screen::Pick    ? "Left/Right choose &#183; A start &#183; B back"
        : slot_list                   ? "Up/Down choose &#183; LB/RB page &#183; A select &#183; B back"
                                      : "Up/Down choose &#183; Left/Right change &#183; A select &#183; B back");
    g_doc->SetClass("spread", spread);
    // The launcher: its picture behind it (res/ui/launcher.png), the menu on
    // the picture's blank parchment, no frame, notes or directions.
    g_doc->SetClass("launcher", g_screen == Screen::Launcher);
    // The pause menu: the launcher's parchment over the dimmed game.
    g_doc->SetClass("pause", g_screen == Screen::InGame);
    g_doc->SetClass("centered", centered);
    g_doc->SetClass("slots", slot_list);  // (the Tome look sets its ten slots a little closer)
    g_doc->SetClass("dense", g_screen == Screen::Controls || g_screen == Screen::Mapping);
    // The settings pages (not Button Mapping's, which shows buttons): one column for every row's value.
    g_doc->SetClass("settings", settings_page && g_screen != Screen::Mapping && !card_screen());
    // Character select: one wide page, every frame at once.
    g_doc->SetClass("roster", g_screen == Screen::Characters);
    // Choose Character: the two players' cards on one wide page.
    g_doc->SetClass("cards", card_screen());
    g_doc->SetClass("pickpage", g_screen == Screen::Pick);
    // Over the game the page lies on the dimmed, paused frame.
    g_doc->SetClass("overlay", g_screen == Screen::InGame || g_screen == Screen::Problem || g_screen == Screen::QuitAsk ||
                                   g_screen == Screen::States ||
                                   ((g_screen == Screen::Slot || g_screen == Screen::SlotDelete ||
                                     g_screen == Screen::SlotOverwrite) && g_slot_from_states) ||
                                   ((g_screen == Screen::Options || g_screen == Screen::OptionsPage ||
                                     g_screen == Screen::Graphics || g_screen == Screen::Mods || g_screen == Screen::ModsPage ||
                                     g_screen == Screen::Characters ||
                                     g_screen == Screen::Controls || g_screen == Screen::Pick ||
                                     g_screen == Screen::Mapping) &&
                                    g_options_return == Screen::InGame));
    // The look the caller chose (frontend_open_page_skin); the launcher's has no class of its own.
    g_doc->SetClass("skin-title", g_skin == Skin::Title);
    g_doc->SetClass("skin-tome", g_skin == Skin::Tome);
    g_doc->SetClass("tome-under", tome_under_page());
    // The title look: the Options picture behind, where the game lays it; a list page in its word column, its letters
    // sized by the picture; the wide pages (the roster, the two cards, Button Mapping's two columns) keep their own
    // places, on a dark panel over the picture.
    const bool title = g_skin == Skin::Title;
    if (title)
        ensure_title_art();  // (tried again at each render until the Options picture has been kept)
    g_doc->SetClass("title-art", title && title_art_ready());
    // With HD textures on and the pack's Options picture, its clean copy (titlevideo_preload writes it at boot, from the
    // pack alone: there before Options is ever seen); its own path, so a failed try at one never blocks the other.
    const bool art_hd = title && g_settings.hd_textures && titlevideo_title_art_hd();
    g_doc->SetClass("title-art-hd", art_hd);
    g_art_hd_shown = art_hd;
    if (title) {
        static int told = -1;
        const int now = art_hd ? 2 : title_art_ready() ? 1 : 0;
        if (now != told)
            std::fprintf(stderr, "[frontend] the title look's backdrop: %s\n",
                         now == 2 ? "HD (res/video/options-clean-hd.png)" : now == 1 ? "the disc's (res/video/options-clean.png)"
                                                                         : "none yet (black)");
        told = now;
    }
    // (Button Mapping always: a controller-only player's has no right-hand rows, and keeps the same panel.)
    const bool wide_page = title_wide_page();
    g_doc->SetClass("title-list", title && !wide_page);
    auto px = [](float v) { return std::to_string(static_cast<int>(std::lround(v))) + "px"; };
    auto place = [&](Rml::Element* e, const WinRect& r) {
        e->SetProperty("left", px(r.x));
        e->SetProperty("top", px(r.y));
        e->SetProperty("width", px(r.w));
        e->SetProperty("height", px(r.h));
    };
    auto unplace = [](Rml::Element* e) {
        for (const char* name : {"left", "top", "width", "height", "font-size"})
            e->RemoveProperty(name);
    };
    Rml::Element* book = g_doc->GetElementById("book");
    Rml::Element* backdrop = g_doc->GetElementById("backdrop");
    Rml::Element* picture = g_doc->GetElementById("backdrop-picture");
    if (backdrop != nullptr && picture != nullptr) {
        if (title) {
            bool wide = true;
            const WinRect v = game_viewport(&wide);
            WinRect p = title_picture_rect();
            p.x -= v.x;  // (inside the viewport's box)
            p.y -= v.y;
            place(backdrop, v);
            place(picture, p);
        } else {
            unplace(backdrop);
            unplace(picture);
        }
    }
    if (title && !wide_page) {
        place(book, title_box(kTitleBox[0], kTitleBox[1], kTitleBox[2], kTitleBox[3]));
        book->SetProperty("font-size", px(title_picture_rect().h * kTitleFont));
    } else if (g_skin == Skin::Tome) {
        // The Tome look: every page (the roster and the cards too) on the leaf laid on the Tome's page.
        // In co-op, on their Tome in their half (the halves kept under the page by the renderer); the partner's half,
        // paused while the page is up, under a veil (#veil).
        // With no Tome under it (tome_under_page) the leaf goes in the middle of the dimmed picture.
        const int half = kTomeInHalves ? g_skin_half : -1;
        const float shift = tome_under_page() ? 0.f : kTomeLeafCentre;
        place(book, tome_units_box(half, kTomeLeaf[0] + shift, kTomeLeaf[1], kTomeLeaf[2] + shift, kTomeLeaf[3]));
        book->SetProperty("font-size", px(tome_page_rect(half).h * kTomeFont));
    } else {
        unplace(book);
    }
    if (Rml::Element* veil = g_doc->GetElementById("veil"); veil != nullptr) {
        const bool halves = g_skin == Skin::Tome && kTomeInHalves && g_skin_half >= 0;
        g_doc->SetClass("tome-halves", halves);
        if (halves)
            place(veil, window_half(1 - g_skin_half));
        else
            unplace(veil);
    }
}

// A screen above the page that was opened: Back (or a Back row) going there leaves the opened page, so the menu closes.
// The screens each is opened from: a section page from Options or Graphics, Graphics, Controls and Mods from Options,
// a Mods section from Mods, the roster and player 2's Controls from a Mods section, Mapping from Controls.
bool past_root(Screen s) {
    if (s == Screen::InGame || s == Screen::Launcher)
        return true;
    switch (g_root) {
    case Screen::OptionsPage: return s == Screen::Options || s == Screen::Graphics;
    case Screen::Graphics:
    case Screen::Mods: return s == Screen::Options;
    case Screen::Controls: return s == Screen::Options || s == Screen::ModsPage || s == Screen::Mods;
    case Screen::ModsPage: return s == Screen::Mods || s == Screen::Options;
    case Screen::Characters: return s == Screen::ModsPage || s == Screen::Mods || s == Screen::Options;
    case Screen::Mapping: return s == Screen::Controls || s == Screen::Options || s == Screen::ModsPage;
    case Screen::Pick: return s == Screen::Options;  // (Controls and Mapping under it come back to it)
    default: return false;
    }
}

void build_rows() {
    if (g_root_closes && past_root(g_screen)) {
        settings_save();
        g_screen = g_root;  // (drawn no more: the menu closes)
        g_close = true;
    }
    g_rows.clear();
    switch (g_screen) {
    case Screen::Launcher: build_launcher(); break;
    case Screen::Play: build_play(); break;
    case Screen::States: build_states(); break;
    case Screen::Slot: build_slot(); break;
    case Screen::SlotDelete: build_slot_delete(); break;
    case Screen::SlotOverwrite: build_slot_overwrite(); break;
    case Screen::Options: build_options(); break;
    case Screen::OptionsPage: build_options_page(); break;
    case Screen::Graphics: build_graphics(); break;
    case Screen::Controls: build_controls(); break;
    case Screen::Mapping: build_mapping(); break;
    case Screen::InGame: build_in_game(); break;
    case Screen::Problem: build_problem(); break;
    case Screen::Mods: build_mods(); break;
    case Screen::ModsPage: build_mods_page(); break;
    case Screen::Characters: build_characters(); break;
    case Screen::QuitAsk: build_quit_ask(); break;
    case Screen::Pick: build_pick(); break;
    }
    // A Tome page is one narrow leaf: one column (Button Mapping's keys, the Mods rows), so it scrolls as one list.
    if (g_skin == Skin::Tome)
        for (Row& r : g_rows)
            r.page = 0;
    // Back on the launcher from a page it opened: at the row that opened it.
    static Screen built = Screen::Launcher;
    if (g_screen == Screen::Launcher && built != Screen::Launcher && g_launcher_row >= 0 &&
        g_launcher_row < static_cast<int>(g_rows.size()))
        g_selected = g_launcher_row;
    if (g_screen != built)
        g_list_top = 0;  // a new list starts at its top (render keeps the highlight in view)
    built = g_screen;
    if (g_selected >= static_cast<int>(g_rows.size()))
        g_selected = 0;
    render();
}

void move_selection(int dir) {
    const int n = static_cast<int>(g_rows.size());
    for (int step = 0; step < n; step++) {
        g_selected = (g_selected + dir + n) % n;
        if (g_rows[g_selected].enabled || g_rows[g_selected].change)
            return;
    }
}

void activate_selected() {
    if (g_selected >= static_cast<int>(g_rows.size()))
        return;
    Row& r = g_rows[g_selected];
    if (!r.enabled)
        return;
    if (g_screen == Screen::Launcher)
        g_launcher_row = g_selected;
    if (r.activate)
        r.activate();
    else if (r.change)
        r.change(+1);
}

void go_back() {
    switch (g_screen) {
    case Screen::Play:
        g_screen = Screen::Launcher;
        g_selected = 0;
        break;
    case Screen::States:
        g_screen = Screen::InGame;
        g_selected = 1;
        break;
    case Screen::Slot:
        slots_page(g_slot_chosen);
        break;
    case Screen::SlotDelete:
        g_screen = Screen::Slot;
        g_selected = delete_row();
        break;
    case Screen::SlotOverwrite:
        g_screen = Screen::Slot;
        g_selected = 0;
        break;
    case Screen::Controls:
        controls::set_menu_player(0);
        g_screen = g_controls_return;
        g_selected = g_controls_return_row;
        break;
    case Screen::OptionsPage:
        leave_options_page();
        break;
    case Screen::Graphics:
        settings_save();
        g_screen = Screen::Options;
        g_selected = g_options_row;
        break;
    case Screen::Mapping:
        g_screen = Screen::Controls;
        g_selected = 0;
        break;
    case Screen::Options:
        settings_save();
        g_screen = g_options_return;
        g_selected = 0;
        break;
    case Screen::Mods:
        settings_save();
        g_screen = g_mods_return;
        g_selected = g_mods_return == Screen::Options ? g_options_row : 0;
        break;
    case Screen::Characters:
        settings_save();
        g_screen = Screen::ModsPage;
        g_selected = g_characters_return_row;
        break;
    case Screen::ModsPage:
        mods_page_back();
        break;
    case Screen::Pick:
        pick_back();
        break;
    case Screen::InGame:
        g_close = true;
        break;
    case Screen::QuitAsk:
        g_screen = Screen::InGame;  // past the page: the menu closes
        g_selected = 0;
        break;
    case Screen::Launcher:
    case Screen::Problem:  // only its rows close it (no quitting by accident)
        break;
    }
}

// Mouse: hovering selects a row, clicking activates it.
class MenuListener final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        if (event.GetId() == Rml::EventId::Mousescroll) {
            const float wheel = event.GetParameter<float>("wheel_delta_y", 0.f);
            if (g_screen == Screen::Characters && wheel != 0.f) {
                roster_wheel(wheel > 0.f ? 1 : -1);
                render();
                event.StopPropagation();
            } else if (list_scrolls() && wheel != 0.f) {
                list_wheel(wheel > 0.f ? 1 : -1);
                render();
                event.StopPropagation();
            }
            return;
        }
        Rml::Element* e = event.GetTargetElement();
        bool on_prev = false;
        while (e != nullptr && e->GetId().rfind("row", 0) != 0) {
            on_prev = on_prev || e->IsClassSet("prev");
            e = e->GetParentNode();
        }
        if (e == nullptr)
            return;
        const int index = std::atoi(e->GetId().c_str() + 3);
        if (index < 0 || index >= static_cast<int>(g_rows.size()))
            return;
        if (event.GetId() == Rml::EventId::Mousemove) {
            // Only a real pointer movement selects: focus changes and rebuilt
            // rows also report the pointer, and must not steal the selection
            // from a controller or the keyboard.
            static int last_x = -1, last_y = -1;
            const int mx = event.GetParameter<int>("mouse_x", 0);
            const int my = event.GetParameter<int>("mouse_y", 0);
            const bool moved = last_x >= 0 && (mx != last_x || my != last_y);
            last_x = mx;
            last_y = my;
            if (moved && g_selected != index) {
                g_selected = index;
                render();
            }
        } else if (event.GetId() == Rml::EventId::Click) {
            g_selected = index;
            if (on_prev && g_rows[index].change)
                g_rows[index].change(-1);
            else
                activate_selected();
            build_rows();
        }
    }
};
MenuListener g_listener;

// The res folder (fonts, menu documents): ED_RES_DIR, else res/ in the working
// directory (play.sh runs from the project), else next to the executable.
std::string res_dir() {
    auto has_menu = [](const std::string& dir) {
        struct stat st;
        return stat((dir + "/ui/menu.rml").c_str(), &st) == 0;
    };
    if (const char* env = std::getenv("ED_RES_DIR"); env != nullptr && has_menu(env))
        return env;
    std::error_code ec;
    const std::string cwd = std::filesystem::current_path(ec).string();
    if (!ec && has_menu(cwd + "/res"))
        return cwd + "/res";
    if (g_argv != nullptr && g_argv[0] != nullptr) {
        const std::filesystem::path exe = std::filesystem::absolute(g_argv[0], ec);
        if (!ec) {
            const std::string dir = exe.parent_path().string();
            for (const std::string& candidate : {dir + "/res", dir + "/../res"})
                if (has_menu(candidate))
                    return candidate;
        }
    }
    return "res";
}

bool ensure_ui() {
    if (g_ui_ready)
        return g_doc != nullptr;
    g_ui_ready = true;
    Rml::Context* context = aurora::rmlui::get_context();
    if (context == nullptr) {
        std::fprintf(stderr, "[frontend] no RmlUi context; menus disabled\n");
        return false;
    }
    const std::string res = res_dir();
    for (const char* font : {"FiraSans-Regular.ttf", "FiraSans-Bold.ttf",
                             "IMFellEnglish-Regular.ttf", "IMFellEnglish-Italic.ttf",
                             "IMFellEnglishSC-Regular.ttf",
                             "LaBelleAurore.ttf"})  // the title's and the Tome's painted hand (tome.cpp's too)
        if (!Rml::LoadFontFace(res + "/fonts/" + font))
            std::fprintf(stderr, "[frontend] font missing: %s/fonts/%s\n", res.c_str(), font);
    g_doc = context->LoadDocument(res + "/ui/menu.rml");
    if (g_doc == nullptr) {
        std::fprintf(stderr, "[frontend] %s/ui/menu.rml did not load; menus disabled\n",
                     res.c_str());
        return false;
    }
    context->GetRootElement()->AddEventListener(Rml::EventId::Keydown, &g_key_listener);
    Rml::Element* menu = g_doc->GetElementById("book");
    menu->AddEventListener(Rml::EventId::Click, &g_listener);
    // Mousemove, not mouseover: rebuilt rows would otherwise grab the
    // selection from the keyboard whenever the pointer rests over the menu.
    menu->AddEventListener(Rml::EventId::Mousemove, &g_listener);
    menu->AddEventListener(Rml::EventId::Mousescroll, &g_listener);
    return true;
}

// The mouse pointer hides after two seconds without moving or clicking, in the
// game and the menus, and shows again as soon as the mouse moves.
void update_cursor() {
    static float last_x = -1.f, last_y = -1.f;
    static Uint64 moved_at = 0;
    static bool shown = true;
    float x = 0.f, y = 0.f;
    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&x, &y);
    const Uint64 now = SDL_GetTicks();
    if (x != last_x || y != last_y || buttons != 0) {
        last_x = x;
        last_y = y;
        moved_at = now;
        if (!shown) {
            SDL_ShowCursor();
            shown = true;
        }
    } else if (shown && now - moved_at > 2000) {
        SDL_HideCursor();
        shown = false;
    }
}

// ED_MENU_TEST=after_s,hold_ms[,every_s]: open the in-game menu by itself
// after_s seconds in, close it hold_ms later, and again every every_s seconds
// (test runs without a controller).
static Uint64 g_test_close_at = 0;
// ED_PAGE_TEST_KEYS="d,d,a,b|a" (testing): in a page the game opened, a press every ED_PAGE_TEST_STEP_MS (default
// 700) from 1 s after it opens: u d l r (the arrows), a, b (Back), w s (the right stick: the list scroll up, down).
// "|" separates the pages opened in turn (the first list for the first page, and so on).
static std::string g_test_keys;
static size_t g_test_key_at = 0;
static Uint64 g_test_key_next = 0;

// The first launcher's music: the song of the game's own Options screen
// (MusyX music, taken from the player's own game: one loop, cut by
// tools/launcher_music.py into res/music/launcher.wav, which git ignores),
// played in a loop at the Music volume. Only the launcher the program opens
// with: not the menus over a running game.
struct LauncherMusic {
    SDL_AudioStream* stream = nullptr;
    Uint8* data = nullptr;
    Uint32 length = 0;

    void start() {
        SDL_AudioSpec spec;
        const std::string path = res_dir() + "/music/launcher.wav";
        if (!SDL_LoadWAV(path.c_str(), &spec, &data, &length))
            return;
        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (stream == nullptr) {
            stop();
            return;
        }
        tick();
        SDL_ResumeAudioStreamDevice(stream);
    }

    // Keeps a whole loop queued ahead, at the Music volume as set now.
    void tick() {
        if (stream == nullptr)
            return;
        SDL_SetAudioStreamGain(stream, g_settings.music_volume / 100.f);
        if (SDL_GetAudioStreamQueued(stream) < static_cast<int>(length))
            SDL_PutAudioStreamData(stream, data, static_cast<int>(length));
    }

    void stop() {
        if (stream != nullptr)
            SDL_DestroyAudioStream(stream);
        stream = nullptr;
        SDL_free(data);
        data = nullptr;
        length = 0;
    }
};
LauncherMusic g_launcher_music;

// A title-look page (Choose Character, the title Options rows) is live: it takes one step per game frame from the frame
// hook, and the title runs under it, so the title's own music goes on as on the game's own menus
// (docs/research/firstrun-hd-flash/title-music.md). Open: the page is up. Release: it closed, and the menu waits for every
// button to be let go before the game has its pad again (hle.c holds it). Every other menu blocks the game (run_menu).
enum class Live { Off, Open, Release };
Live g_live = Live::Off;
std::atomic<bool> g_live_flag{false};  // (any thread: frontend_page_live)

// One turn of the menu (run_menu's loop, and a live page's frame), without its present.
enum class MenuStep { Open, Closed };
bool g_menu_pressed[kButtonCount];

void menu_begin() {
    g_close = false;
    build_rows();
    g_doc->Show(Rml::ModalFlag::None, Rml::FocusFlag::Document);
    // Ignore whatever is still held from before the menu opened.
    g_input.poll(g_menu_pressed);
}

// The Solo / Co-op page's pick of the other mode (build_pick): the program starts again in it, into the same title row.
// A live title page steps inside the game's Present item (the frame hook) while the game thread can be a frame ahead
// and running; so here the page is presented by itself, as a blocking page is, until the game is held at its next
// present (its pad reads stand still for 100 ms; at most 2 s), and only then is the process replaced (plan, skeptic's
// wrong 7). The environment is changed only then too (wrong 4). Failed: the mode as it was, and the card says so.
void pick_restart_now() {
    const int row = g_pick_restart_row;
    report_pause(true);
    const Uint64 start = SDL_GetTicks();
    unsigned reads = hle_pad_reads();
    Uint64 still_since = start;
    bool held = false;
    while (SDL_GetTicks() - start < 2000) {
        if (!dol_aurora_ui_frame()) {
            report_pause(false);
            g_pick_restart_row = -1;
            dol_aurora_request_quit();  // (the window was closed)
            return;
        }
        const unsigned now = hle_pad_reads();
        if (now != reads) {
            reads = now;
            still_since = SDL_GetTicks();
        } else if (SDL_GetTicks() - still_since >= 100) {
            held = true;
            break;
        }
    }
    std::fprintf(stderr, "[frontend] the Solo / Co-op pick: the game %s after %llu ms\n",
                 held ? "is held (no pad read for 100 ms)" : "still reads its pad", static_cast<unsigned long long>(SDL_GetTicks() - start));
    restart_process(row == 1 ? "load" : "start");
    g_settings.mod_coop = !g_settings.mod_coop;  // (this run's mode again)
    settings_save();
    g_pick_note_failed = true;
    g_pick_restart_row = -1;
    report_pause(false);
    build_rows();
}

MenuStep menu_step() {
    bool* const pressed = g_menu_pressed;
    if (g_close)
        return MenuStep::Closed;
    if (g_pick_restart_row >= 0) {
        // (The card's line up for a moment first, drawn by the game's own presents under a live page.)
        if (SDL_GetTicks() - g_pick_restart_at >= 300)
            pick_restart_now();
        return MenuStep::Open;
    }
    {
        g_launcher_music.tick();
        update_cursor();
        // The title and Tome looks are placed in window pixels (render). The renderer takes a new aspect at its next
        // present, after the row that changed it rebuilt the page, and a window takes its new size later still
        // (Fullscreen, a resize): when the window's size, the shown aspect or the aspect setting changes, it is placed again.
        if (g_skin != Skin::Launcher) {
            static float placed_aspect = 0.f;
            static uint32_t placed_w = 0, placed_h = 0;
            static int placed_setting = -1;
            float aspect = 0.f;
            uint32_t w = 0, h = 0;
            dol_aurora_get_display_info(&aspect, &w, &h);
            if (aspect != placed_aspect || w != placed_w || h != placed_h || g_settings.aspect != placed_setting) {
                placed_aspect = aspect;
                placed_w = w;
                placed_h = h;
                placed_setting = g_settings.aspect;
                render();
            }
        }
        if (controls::capturing()) {
            const bool done = controls::poll_capture();
            if (done || !controls::capturing()) {
                // Swallow the press that bound the action.
                g_input.poll(pressed);
                std::fill(std::begin(g_key_pressed), std::end(g_key_pressed), false);
                build_rows();
            }
            return MenuStep::Open;
        }
        g_input.poll(pressed);
        if (g_test_key_next != 0 && SDL_GetTicks() >= g_test_key_next) {
            while (g_test_key_at < g_test_keys.size() && (g_test_keys[g_test_key_at] == ',' || g_test_keys[g_test_key_at] == ' '))
                g_test_key_at++;
            if (g_test_key_at < g_test_keys.size()) {
                const char c = g_test_keys[g_test_key_at++];
                const int b = c == 'u' ? kUp : c == 'd' ? kDown : c == 'l' ? kLeft : c == 'r' ? kRight : c == 'a' ? kAccept
                              : c == 'b' ? kBack : c == 'w' ? kScrollUp : c == 's' ? kScrollDown : -1;
                if (b >= 0)
                    pressed[b] = true;
                std::fprintf(stderr, "[frontend] test key %c on screen %d row %d\n", c, static_cast<int>(g_screen), g_selected);
            }
            static const int step = std::getenv("ED_PAGE_TEST_STEP_MS") != nullptr ? std::atoi(std::getenv("ED_PAGE_TEST_STEP_MS")) : 700;
            g_test_key_next = g_test_key_at < g_test_keys.size() ? SDL_GetTicks() + std::max(step, 50) : 0;
        }
        bool dirty = false;
        if (g_screen == Screen::Characters) {
            // A grid: every arrow moves (and left/right change nothing).
            if (pressed[kUp] || pressed[kDown] || pressed[kLeft] || pressed[kRight]) {
                move_in_roster(pressed[kLeft] ? -1 : pressed[kRight] ? 1 : 0,
                               pressed[kUp] ? -1 : pressed[kDown] ? 1 : 0);
                dirty = true;
            }
            // The right stick scrolls the frames, a line at a time, the highlight carried along.
            if (pressed[kScrollUp] != pressed[kScrollDown]) {
                roster_wheel(pressed[kScrollDown] ? 1 : -1);
                dirty = true;
            }
        } else {
            if (pressed[kUp]) {
                move_selection(-1);
                dirty = true;
            }
            if (pressed[kDown]) {
                move_selection(+1);
                dirty = true;
            }
            // A list that scrolls: the right stick (Page Up/Down) moves it a row, the highlight carried along.
            if (list_scrolls() && pressed[kScrollUp] != pressed[kScrollDown]) {
                list_wheel(pressed[kScrollDown] ? 1 : -1);
                dirty = true;
            }
        }
        // Choose Character: left and right go between the two players' cards.
        if (card_screen() && (pressed[kLeft] || pressed[kRight]) && g_selected < static_cast<int>(g_rows.size()) &&
            g_rows[g_selected].card != 0) {
            for (size_t i = 0; i < g_rows.size(); i++)
                if (g_rows[i].card != 0 && g_rows[i].card != g_rows[g_selected].card) {
                    g_selected = static_cast<int>(i);
                    break;
                }
            dirty = true;
        } else if (g_screen != Screen::Characters && (pressed[kLeft] || pressed[kRight]) &&
            g_selected < static_cast<int>(g_rows.size())) {
            Row& r = g_rows[g_selected];
            if (r.change && (r.enabled || r.browse)) {
                r.change(pressed[kLeft] ? -1 : +1);
                dirty = true;
            }
        }
        if ((g_screen == Screen::States || g_screen == Screen::Play) && (pressed[kPageLeft] || pressed[kPageRight])) {
            slots_jump(pressed[kPageLeft] ? -1 : +1);
            dirty = true;
        }
        if (pressed[kAccept]) {
            activate_selected();
            dirty = true;
        } else if (pressed[kBack] || (pressed[kMenu] && g_screen == Screen::InGame)) {
            go_back();
            dirty = true;
        }
        if (g_test_close_at != 0 && SDL_GetTicks() >= g_test_close_at) {
            g_test_close_at = 0;
            // ED_PAGE_TEST_BACK=1 (testing): Back pressed once instead, so the page's own way out is what closes it.
            static const bool by_back = std::getenv("ED_PAGE_TEST_BACK") != nullptr;
            if (by_back && g_root_closes) {
                std::fprintf(stderr, "[frontend] test: Back on screen %d\n", static_cast<int>(g_screen));
                go_back();
                dirty = true;
            } else {
                g_close = true;
            }
        }
        static const bool log = std::getenv("ED_FRONTEND_LOG") != nullptr;
        if (log && dirty)
            std::fprintf(stderr,
                         "[frontend] up=%d down=%d left=%d right=%d accept=%d back=%d "
                         "-> screen=%d row=%d close=%d\n",
                         pressed[kUp], pressed[kDown], pressed[kLeft], pressed[kRight],
                         pressed[kAccept], pressed[kBack], static_cast<int>(g_screen),
                         g_selected, g_close);
        if (g_close)
            return MenuStep::Closed;
        static Uint64 refresh = 0;
        if ((g_screen == Screen::Controls || g_screen == Screen::Mapping) &&
            SDL_GetTicks() > refresh) {
            refresh = SDL_GetTicks() + 500;  // controller plugged in / out
            dirty = true;
        }
        if (g_skin == Skin::Title && g_settings.hd_textures && !g_art_hd_shown && titlevideo_title_art_hd())
            dirty = true;  // (the HD backdrop just written: shown without waiting for a key)
        if (dirty)
            build_rows();
    }
    return MenuStep::Open;
}

// Run the current screen until g_close, the game held behind it; false when the window was closed.
bool run_menu() {
    menu_begin();
    bool open = true;
    while (menu_step() == MenuStep::Open) {
        if (!dol_aurora_ui_frame()) {
            open = false;
            break;
        }
    }
    g_doc->Hide();
    // Wait for every button to be let go so the game does not see the press
    // that closed the menu.
    while (open && g_input.any_held()) {
        g_input.poll(g_menu_pressed);
        if (!dol_aurora_ui_frame())
            open = false;
    }
    return open;
}

// --- The first-run step --------------------------------------------------------------------------------------------
// "Preparing the game from your disc": on the first start (or after an update or a pack change) the launcher makes what
// the menus take from the player's own disc and builds the renderer's pipelines from the shipped list before the game
// starts, so nothing is made in front of the player (docs/research/firstrun-hd-flash/firstrun.md, step 4). Each piece
// is stamped when it is done: res/firstrun.txt (the portraits, the HD backdrop's source) and prepared.txt in the
// renderer's cache folder (the shipped list it was built from: a cleared cache shows the step again). A piece not done
// (skipped with B, or timed out) is offered again at the next start. The title Options picture is not here: it is in
// the game's memory only, and is taken at the title's first frame (hle.c title_options_from_bank).
// 2: Default's picture (the Tome and skull) made with them.
constexpr int kPortraitsVersion = 2;  // raised when ensure_portraits makes other pictures: they are made again

std::string firstrun_path() { return res_dir() + "/firstrun.txt"; }

// The renderer's cache folder (aurora.cpp: AURORA_CACHE_DIR, else the app's own folder).
std::string cache_dir() {
    if (const char* dir = std::getenv("AURORA_CACHE_DIR"); dir != nullptr && dir[0] != '\0') {
        std::string path{dir};
        if (path.back() != '/' && path.back() != '\\')
            path += '/';
        return path;
    }
    char* pref = SDL_GetPrefPath(nullptr, "Eternal Darkness");
    std::string path = pref != nullptr ? pref : "";
    SDL_free(pref);
    return path;
}

// The shipped pipeline list the renderer merged at start (pipeline_cache.cpp pipeline_cache_seed_path: res/ beside the
// executable, one level up, or in the working folder), as "SIZE XXH64", or "none".
std::string seed_id() {
    std::string base;
    if (const char* b = SDL_GetBasePath(); b != nullptr)
        base = b;
    for (const std::string& dir : {base + "res/", base + "../res/", std::string("res/")}) {
        FILE* f = std::fopen((dir + "initial_pipeline_cache.db").c_str(), "rb");
        if (f == nullptr)
            continue;
        std::vector<uint8_t> data;
        std::vector<uint8_t> buf(1 << 16);
        size_t n;
        while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0)
            data.insert(data.end(), buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(n));
        std::fclose(f);
        char id[64];
        std::snprintf(id, sizeof id, "%zu %016llx", data.size(),
                      static_cast<unsigned long long>(XXH64(data.data(), data.size(), 0)));
        return id;
    }
    return "none";
}

std::string read_text(const std::string& path) {
    std::string out;
    if (FILE* f = std::fopen(path.c_str(), "rb")) {
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
            out.append(buf, n);
        std::fclose(f);
    }
    return out;
}

// The line of a stamp that starts with KEY and a space ("" when none).
std::string stamp_line(const std::string& text, const std::string& key) {
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(at, end - at);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.rfind(key + " ", 0) == 0)
            return line;
        at = end + 1;
    }
    return "";
}

bool write_text(const std::string& path, const std::string& text) {
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (f == nullptr)
        return false;
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    std::fclose(f);
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    return ok && !ec;
}

std::string with_commas(uint32_t v) {
    const std::string digits = std::to_string(v);
    std::string out;
    for (size_t i = 0; i < digits.size(); i++) {
        if (i > 0 && (digits.size() - i) % 3 == 0)
            out += ',';
        out += digits[i];
    }
    return out;
}

bool have_portraits() {
    for (int i = 1; i <= 12; i++) {
        struct stat st;
        if (stat((res_dir() + "/ui/" + portrait_file(i)).c_str(), &st) != 0)
            return false;
    }
    return true;
}

// The page: the launcher's picture, and on its parchment the heading, what is being made, a bar and how far. False
// when the window was closed.
bool run_preparing() {
    const char* force = std::getenv("ED_PREPARE");  // 0: never (tests), 1: every piece again
    if (force != nullptr && force[0] == '0')
        return true;
    const bool again = force != nullptr && force[0] == '1';
    if (!again && std::getenv("ED_MENU_OPEN") != nullptr)
        return true;  // (a test that opens a page: as before)
    const std::string disc_id = ed_game_id();
    const std::string res_stamp = read_text(firstrun_path());
    const std::string cache_stamp_path = cache_dir() + "prepared.txt";
    const std::string cache_stamp = read_text(cache_stamp_path);
    // The portraits: all twelve there, made by this version (a stamp of an older one makes them again).
    const std::string portraits_want = "portraits " + std::to_string(kPortraitsVersion) + " " + disc_id;
    const std::string portraits_had = stamp_line(res_stamp, "portraits");
    const bool portraits_old = !portraits_had.empty() && portraits_had != portraits_want;
    // Default's HD picture (HD textures on, the pack has it) is made with them: from the pack, stamped by its file.
    // A stamp of "failed" for this very pack file was tried once already: not again at every start. The portraits made
    // again (portraits_old) remove its stamp, so it is made again with them.
    const std::string default_hd = default_hd_id(), default_hd_had = read_text(res_dir() + "/ui/" + kDefaultHdStamp);
    const bool need_default_hd =
        !default_hd.empty() && (again || portraits_old ||
                                (default_hd_had != default_hd + "\n" && default_hd_had != "failed " + default_hd + "\n"));
    // And the twelve HD portraits the same way.
    const std::string portraits_hd = portraits_hd_id();
    const std::string portraits_hd_had = read_text(res_dir() + "/ui/portraits/hd/stamp.txt");
    const bool need_portraits_hd =
        !portraits_hd.empty() &&
        (again || (portraits_hd_had != portraits_hd + "\n" && portraits_hd_had != "failed " + portraits_hd + "\n"));
    bool need_portraits = again || portraits_old || !have_portraits() || need_default_hd || need_portraits_hd;
    // The pipelines: built from this shipped list into this cache folder, for this run's anti-aliasing (the renderer
    // builds only the rows of its own sample count, pipeline_cache.cpp: another setting is a build of its own).
    const std::string pipelines_want =
        "pipelines " + seed_id() + " msaa " + std::to_string(g_running_anti_aliasing ? 4 : 1);
    const bool need_pipelines = again || stamp_line(cache_stamp, "pipelines") != pipelines_want;
    // The HD backdrop: made from this pack's picture (the boot's workers make it: titlevideo_preload).
    const std::string hd_id = titlevideo_hd_id(), hd_want = "hd " + hd_id;
    const bool hd_none = hd_id == "off" || hd_id == "none";
    const bool need_hd = (again || stamp_line(res_stamp, "hd") != hd_want) && !hd_none;
    if (!need_portraits && !need_pipelines && !need_hd) {
        std::fprintf(stderr, "[prepare] everything prepared already (%s, %s)\n", firstrun_path().c_str(),
                     cache_stamp_path.c_str());
        return true;
    }
    std::fprintf(stderr, "[prepare] the first-run step: portraits %s, pipelines %s, HD backdrop %s\n",
                 need_portraits ? "to make" : "ready", need_pipelines ? "to build" : "built",
                 need_hd ? "to make" : hd_none ? "off" : "ready");
    const Uint64 t0 = SDL_GetTicks();
    bool portraits_done = !need_portraits, hd_done = !need_hd, pipelines_done = !need_pipelines;
    bool skipped = false, open = true;
    Uint64 portraits_ms = 0, hd_ms = 0, pipelines_ms = 0;
    uint32_t pipelines_total = 0;
    int frames = 0;
    for (const char* c : {"spread", "pause", "centered", "slots", "dense", "settings", "roster", "cards", "overlay",
                          "skin-title", "skin-tome", "tome-under", "title-art", "title-art-hd", "title-list", "tome-halves"})
        g_doc->SetClass(c, false);
    g_doc->SetClass("launcher", true);
    g_doc->SetClass("preparing", true);
    g_doc->GetElementById("heading")->SetInnerRML("");
    g_doc->GetElementById("menu1")->SetInnerRML("");
    g_doc->GetElementById("note")->SetInnerRML("");
    g_doc->GetElementById("hint")->SetInnerRML("");
    // The page is shown only when the work takes longer than a moment (kGraceMs): a stamp changed by a quick piece alone
    // (the HD pictures, about 0.3 s) doesn't flash it on and off.
    constexpr Uint64 kGraceMs = 700;
    bool page_up = false;
    bool pressed[kButtonCount];
    g_input.poll(pressed);
    std::string shown;
    while (true) {
        g_launcher_music.tick();  // (the launcher's music goes on under the page, as under the menu)
        const Uint64 now = SDL_GetTicks();
        // The pieces: the portraits in one go once the page is up, the pipelines and the HD pictures in the background
        // (the renderer's worker, titlevideo's), the bar shared out between the pieces needed.
        if (!portraits_done && frames >= 2) {
            const Uint64 p0 = SDL_GetTicks();
            if (again || portraits_old) {
                for (int i = 1; i <= 12; i++) {
                    std::error_code ec;
                    std::filesystem::remove(res_dir() + "/ui/" + portrait_file(i), ec);
                }
                std::error_code ec;
                std::filesystem::remove(res_dir() + "/ui/" + kDefaultFile, ec);
                std::filesystem::remove(res_dir() + "/ui/" + kDefaultHdStamp, ec);
            }
            const bool ok = ensure_portraits();
            // (each looked at afresh: a page shown before this one may have kept an older answer)
            if (need_default_hd) {
                g_default_hd_for = -1;
                ensure_default_hd();
            }
            if (need_portraits_hd) {
                std::error_code ec;
                std::filesystem::remove(res_dir() + "/ui/portraits/hd/stamp.txt", ec);
                g_portraits_hd_for = -1;
                ensure_portraits_hd();
            }
            portraits_ms = SDL_GetTicks() - p0;
            portraits_done = true;
            std::fprintf(stderr, "[prepare] the character portraits %s (%llu ms)\n", ok ? "made" : "NOT made",
                         static_cast<unsigned long long>(portraits_ms));
            if (!ok)
                need_portraits = false;  // (not stamped: offered again next start)
        }
        const AuroraStats* stats = aurora_get_stats();
        const uint32_t queued = stats != nullptr ? stats->queuedPipelines : 0;
        const uint32_t created = stats != nullptr ? stats->createdPipelines : 0;
        pipelines_total = std::max(pipelines_total, queued + created);
        if (!pipelines_done && queued == 0 && frames >= 2) {
            pipelines_done = true;
            pipelines_ms = now - t0;
            std::fprintf(stderr, "[prepare] the pipelines built (%u, %llu ms on this page)\n", created,
                         static_cast<unsigned long long>(pipelines_ms));
        }
        if (!hd_done && !titlevideo_hd_busy()) {
            hd_done = true;
            hd_ms = now - t0;
            std::fprintf(stderr, "[prepare] the HD title pictures %s (%llu ms on this page)\n",
                         titlevideo_title_art_hd() ? "made" : "NOT made", static_cast<unsigned long long>(hd_ms));
        }
        const bool timed_out = now - t0 > 300000;  // (5 minutes: the rest is offered again next start)
        if ((portraits_done && pipelines_done && hd_done) || skipped || timed_out) {
            if (timed_out && !skipped)
                std::fprintf(stderr, "[prepare] stopped after 5 minutes: the rest is offered again next start\n");
            break;
        }
        // The bar: the portraits a twentieth, the HD pictures a tenth, the pipelines the rest (of the pieces needed).
        const float wp = need_portraits ? 0.05f : 0.f, wh = need_hd ? 0.10f : 0.f, wl = need_pipelines ? 0.85f : 0.f;
        const float pf = pipelines_done ? 1.f : pipelines_total == 0 ? 0.f : static_cast<float>(created) / pipelines_total;
        const float total = wp + wh + wl;
        const float frac =
            total <= 0.f ? 1.f : (wp * (portraits_done ? 1.f : 0.f) + wh * (hd_done ? 1.f : 0.f) + wl * pf) / total;
        std::string doing, count;
        if (!portraits_done) {
            doing = "Painting the character portraits";
        } else if (!pipelines_done) {
            doing = "Building the shaders for your graphics card";
            count = with_commas(std::min(created, pipelines_total)) + " of " + with_commas(pipelines_total);
        } else {
            doing = "Making the title pictures in high definition";
        }
        const int pct = static_cast<int>(std::lround(std::clamp(frac, 0.f, 1.f) * 100.f));
        const std::string rml =
            "<div class=\"prep\"><div class=\"prep-title\">Preparing the game</div>"
            "<div class=\"prep-sub\">from your disc</div><div class=\"gap\"></div>"
            "<div class=\"prep-doing\">" + escape(doing) + "</div>"
            "<div class=\"prep-bar\"><div class=\"prep-fill\" style=\"width: " + std::to_string(pct) + "%;\"></div></div>"
            "<div class=\"prep-count\">" + (count.empty() ? std::to_string(pct) + "%" : count) + "</div>"
            "<div class=\"prep-note\">Only on the first start, and after an update.</div>"
            "<div class=\"prep-skip\">B &#8212; skip for now</div></div>";
        if (rml != shown) {
            g_doc->GetElementById("menu0")->SetInnerRML(rml);
            shown = rml;
        }
        if (!page_up && now - t0 >= kGraceMs) {
            page_up = true;
            g_doc->Show(Rml::ModalFlag::None, Rml::FocusFlag::Document);
            std::fprintf(stderr, "[prepare] the page is up (the work takes longer than %llu ms)\n",
                         static_cast<unsigned long long>(kGraceMs));
        }
        g_input.poll(pressed);
        if (pressed[kBack]) {
            skipped = true;
            std::fprintf(stderr, "[prepare] skipped with B: what is not done is offered again next start\n");
        }
        if (!dol_aurora_ui_frame()) {
            open = false;
            break;
        }
        frames++;
        // (A slow page leaves the renderer's pipeline worker the device: ED_PREPARE_FRAME_MS, default 33.)
        static const int frame_ms = std::getenv("ED_PREPARE_FRAME_MS") != nullptr ? std::atoi(std::getenv("ED_PREPARE_FRAME_MS")) : 33;
        if (frame_ms > 0)
            SDL_Delay(static_cast<Uint32>(frame_ms));
    }
    // The stamps of what was done (a piece not done keeps its old line, or none).
    std::string res_text = "# The first-run step's stamps (frontend.cpp run_preparing). Delete this file to see it again.\n";
    const std::string had_hd = stamp_line(res_stamp, "hd");
    if (need_portraits ? portraits_done : have_portraits())
        res_text += (need_portraits || portraits_had.empty() ? portraits_want : portraits_had) + "\n";
    if ((need_hd && hd_done) || hd_none)
        res_text += hd_want + "\n";
    else if (!had_hd.empty())
        res_text += had_hd + "\n";
    if (res_text != res_stamp && !write_text(firstrun_path(), res_text))
        std::fprintf(stderr, "[prepare] could not write %s\n", firstrun_path().c_str());
    if (pipelines_done && need_pipelines && !write_text(cache_stamp_path, pipelines_want + "\n"))
        std::fprintf(stderr, "[prepare] could not write %s\n", cache_stamp_path.c_str());
    std::fprintf(stderr, "[prepare] done in %llu ms (portraits %llu ms, pipelines %llu ms, HD %llu ms)%s\n",
                 static_cast<unsigned long long>(SDL_GetTicks() - t0), static_cast<unsigned long long>(portraits_ms),
                 static_cast<unsigned long long>(pipelines_ms), static_cast<unsigned long long>(hd_ms),
                 skipped ? ", skipped" : "");
    g_doc->SetClass("preparing", false);
    g_doc->GetElementById("menu0")->SetInnerRML("");
    // Wait for B to be let go, so the launcher does not take it.
    while (open && g_input.any_held()) {
        g_input.poll(pressed);
        if (!dol_aurora_ui_frame())
            open = false;
    }
    return open;
}

}  // namespace

std::string frontend_res_dir() { return res_dir(); }

// Before ImGui's first frame (its font atlas is locked during one): Fira Sans
// Bold, the menus' face, baked large so the counter stays sharp at any scale.
extern "C" void frontend_imgui_init(void) {
    const std::string path = res_dir() + "/fonts/FiraSans-Bold.ttf";
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        std::fprintf(stderr, "[frontend] font missing: %s (FPS counter in ImGui's own)\n", path.c_str());
        return;
    }
    g_fps_font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), 64.f);
    // The Tome's own-looking buttons (frontend_tome_button): IM Fell English, an old book's face.
    const std::string tome = res_dir() + "/fonts/IMFellEnglish-Regular.ttf";
    if (stat(tome.c_str(), &st) == 0)
        g_tome_font = ImGui::GetIO().Fonts->AddFontFromFileTTF(tome.c_str(), 64.f);
}

extern "C" void frontend_init(int argc, char** argv) {
    g_argc = argc;
    g_argv = argv;
    g_running_anti_aliasing = g_settings.anti_aliasing;
    g_running_anisotropy = g_settings.anisotropy;
    // Load the menus now so their key listener hears Esc even when the
    // launcher is skipped (quick-load and test runs).
    ensure_ui();
    // The window and Dock icon: the Recomp emblem.
    std::vector<uint8_t> icon;
    uint32_t w = 0, h = 0;
    if (load_png_rgba(res_dir() + "/icon/window.png", &icon, &w, &h))
        dol_aurora_set_window_icon(icon.data(), w, h);
}

extern "C" void frontend_set_disc(const char* path) {
    Disc d;
    if (path != nullptr && read_disc(path, &d) && d.id == ed_game_id())
        g_run_disc = path;
}

extern "C" void frontend_resume_discs(const char* iso_path) {
    // As frontend_run_launcher: the list from the chosen disc and the folders, then the disc this run plays chosen in it
    // (else the launcher's own choice), and the disc row on the launcher's main page only while none was chosen yet.
    g_discs = scan_discs(g_settings.disc[0] != '\0' ? g_settings.disc : iso_path);
    g_disc = 0;
    bool found = false;
    for (int pass = 0; pass < 3 && !found; pass++)
        for (size_t i = 0; i < g_discs.size() && !found; i++)
            if (g_discs[i].id == ed_game_id() && (pass == 2 || g_discs[i].path == (pass == 0 ? iso_path : g_settings.disc))) {
                g_disc = static_cast<int>(i);
                found = true;
            }
    g_disc_on_main = !(g_settings.disc[0] != '\0' && disc_playable() && g_discs[g_disc].path == g_settings.disc);
    g_running_disc = found ? g_discs[g_disc].path : std::string(iso_path);
    std::fprintf(stderr, "[frontend] started again past the launcher: %zu disc(s), %s chosen%s\n", g_discs.size(),
                 found ? g_discs[g_disc].path.c_str() : "none", g_disc_on_main ? " (the disc row on the launcher's page)" : "");
}

extern "C" bool frontend_run_launcher(const char* default_iso, char* iso_out,
                                      size_t iso_cap) {
    if (!ensure_ui()) {
        snprintf(iso_out, iso_cap, "%s", default_iso != nullptr ? default_iso : "");
        return true;
    }
    g_discs = scan_discs(g_settings.disc[0] != '\0' ? g_settings.disc : default_iso);
    // Start on the chosen disc, else the first one this build can run.
    g_disc = 0;
    bool found = false;
    for (size_t i = 0; i < g_discs.size() && !found; i++)
        if (g_discs[i].path == g_settings.disc && g_discs[i].id == ed_game_id()) {
            g_disc = static_cast<int>(i);
            found = true;
        }
    for (size_t i = 0; i < g_discs.size() && !found; i++)
        if (g_discs[i].id == ed_game_id()) {
            g_disc = static_cast<int>(i);
            found = true;
        }
    // Once a playable disc has been chosen, Play leads the main menu and the
    // disc choice lives in Options.
    g_disc_on_main = !(g_settings.disc[0] != '\0' && disc_playable() &&
                       g_discs[g_disc].path == g_settings.disc);
    g_screen = Screen::Launcher;
    g_selected = launcher_main_row();
    // ED_MENU_OPEN=options, graphics, display, sound, image, shadows, lighting, effects, game,
    // controls, mapping, mods, challenge, characters-page, cheats, characters, load, pick-mode, pick-load, pick-controls,
    // states, slotN, deleteN or overwriteN[,row] (testing): the launcher opens on that page, at that
    // row; controls2 and mapping2 show player 2's, slotN and deleteN slot N's.
    if (const char* page = std::getenv("ED_MENU_OPEN"); page != nullptr) {
        g_options_return = Screen::Launcher;
        g_mods_return = Screen::Launcher;
        g_screen = std::strncmp(page, "controls", 8) == 0  ? Screen::Controls
                   : std::strncmp(page, "mapping", 7) == 0 ? Screen::Mapping
                   : std::strncmp(page, "mods", 4) == 0    ? Screen::Mods
                   : std::strncmp(page, "characters", 10) == 0 ? Screen::Characters
                   : std::strncmp(page, "load", 4) == 0    ? Screen::Play
                   : std::strncmp(page, "states", 6) == 0  ? Screen::States
                   : std::strncmp(page, "slot", 4) == 0    ? Screen::Slot
                   : std::strncmp(page, "delete", 6) == 0  ? Screen::SlotDelete
                   : std::strncmp(page, "overwrite", 9) == 0 ? Screen::SlotOverwrite
                                                           : Screen::Options;
        static const struct {
            const char* name;
            Screen screen;
            int section;
        } kPages[] = {{"graphics", Screen::Graphics, 0},        {"display", Screen::OptionsPage, 1},
                      {"sound", Screen::OptionsPage, 2},        {"image", Screen::OptionsPage, 3},
                      {"game", Screen::OptionsPage, 5},         {"shadows", Screen::OptionsPage, 7},
                      {"lighting", Screen::OptionsPage, 8},     {"effects", Screen::OptionsPage, 9},
                      {"challenge", Screen::ModsPage, 2},       {"characters-page", Screen::ModsPage, 3},
                      {"cheats", Screen::ModsPage, 4}};
        for (const auto& p : kPages)
            if (std::strncmp(page, p.name, std::strlen(p.name)) == 0) {
                g_screen = p.screen;
                (p.screen == Screen::ModsPage ? g_mods_section : g_options_section) = p.section;
            }
        // pick-mode / pick-load (Solo / Co-op under Start Game or Load Game's heading) and pick-controls (Player I / II).
        if (std::strncmp(page, "pick-", 5) == 0) {
            g_pick_title_row = std::strncmp(page, "pick-load", 9) == 0 ? 1 : 0;
            open_pick(std::strncmp(page, "pick-controls", 13) == 0 ? PickKind::Controls : PickKind::Mode, Screen::Launcher, 0);
        }
        if (g_screen == Screen::SlotOverwrite)
            g_slot_from_states = true;
        if (g_screen == Screen::Slot || g_screen == Screen::SlotDelete || g_screen == Screen::SlotOverwrite)
            g_slot_chosen = std::atoi(page + (g_screen == Screen::Slot ? 4 : g_screen == Screen::SlotDelete ? 6 : 9));
        const char* digit = page + std::strcspn(page, "2,");
        controls::set_menu_player(*digit == '2' ? 1 : 0);
        const char* comma = std::strchr(page, ',');
        g_selected = comma != nullptr ? std::atoi(comma + 1) : 0;
    }
    g_play = false;
    g_load_slot = 0;
    g_launcher_music.start();
    // The first start (or after an update): the disc's pictures and the shaders made before the menu (run_preparing).
    if (disc_playable() && !run_preparing()) {
        g_launcher_music.stop();
        return false;
    }
    // ED_LAUNCHER_TEST_KEYS="a" (testing): presses on the launcher as ED_PAGE_TEST_KEYS's, the first 1.5 s after it opens.
    if (const char* t = std::getenv("ED_LAUNCHER_TEST_KEYS"); t != nullptr && t[0] != '\0') {
        g_test_keys = t;
        g_test_key_at = 0;
        g_test_key_next = SDL_GetTicks() + 1500;
    }
    const bool played = run_menu();
    g_launcher_music.stop();
    if (!played || !g_play)
        return false;
    snprintf(iso_out, iso_cap, "%s", g_discs[g_disc].path.c_str());
    g_running_disc = g_discs[g_disc].path;
    if (g_load_slot != 0) {
        // Boot straight into the save (the quick-state load at boot).
        setenv("ED_QUICKLOAD", slot_path(g_load_slot).c_str(), 1);
    }
    return true;
}

// dp in ImGui's units (window points) for the overlays drawn through ImGui:
// the display's content scale, as the menus use (2 on a 4K PS5, 1 on a Retina
// Mac whose points are 2 pixels).
float overlay_dp() {
    float dp = 1.f;
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    if (windows != nullptr && count > 0) {
        const float density = SDL_GetWindowPixelDensity(windows[0]);
        const float display = SDL_GetWindowDisplayScale(windows[0]);
        if (density > 0.f && display > 0.f)
            dp = display / density;
    }
    SDL_free(windows);
    return dp;
}

// On-screen frames per second (game frames, i.e. calls here), refreshed each
// second while the FPS counter option is on: every picture presented, which falls below the screen's
// rate only when frames in between are late and skipped. Drawn through Aurora's ImGui,
// which covers the whole screen: the RmlUi context covers only the game
// picture, so in 4:3 (and in the game's letterboxed scenes) the counter sat
// inside the bars, not at the screen's corner.
// A short message in the screen's top right corner ("Saved to slot 3"), for
// two and a half seconds, fading over the last half. Set from any thread.
std::mutex g_toast_mutex;
std::string g_toast;
Uint64 g_toast_at = 0;

std::string g_toast_player[2];
Uint64 g_toast_player_at[2] = {0, 0};

// right and top are of the area it goes in, 0 to 1 of the window.
void draw_one_toast(const std::string& text, Uint64 at_ms, Uint64 lasts_ms, float right_of, float top_of) {
    const Uint64 age = SDL_GetTicks() - at_ms;
    if (text.empty() || age >= lasts_ms || ImGui::GetCurrentContext() == nullptr)
        return;
    const float fade = age < lasts_ms - 500 ? 1.0f : 1.0f - (age - (lasts_ms - 500)) / 500.0f;
    const float dp = overlay_dp();
    ImFont* font = g_fps_font != nullptr ? g_fps_font : ImGui::GetFont();
    const float size = 18.f * dp;
    const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.f, text.c_str());
    const ImVec2 pad{10.f * dp, 4.f * dp};
    const ImVec2 window = ImGui::GetMainViewport()->Size;
    const float right = window.x * right_of - 14.f * dp;
    const ImVec2 at{right - extent.x - 2.f * pad.x, window.y * top_of + 10.f * dp};
    ImDrawList* draw = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
    draw->AddRectFilled(at, ImVec2{right, at.y + extent.y + 2.f * pad.y},
                        IM_COL32(0, 0, 0, static_cast<int>(0x99 * fade)), 4.f * dp);
    draw->AddText(font, size, ImVec2{at.x + pad.x, at.y + pad.y},
                  IM_COL32(0xf0, 0xe6, 0xc8, static_cast<int>(0xff * fade)), text.c_str());
}

// A button drawn as part of a Tome page (frontend_tome_button): asked each frame by the game thread while that page is up,
// drawn while the ask is fresh. The Options tab's "Characters", under its cross of four (Controller, Audio, Display,
// Load/Save), in the Tome's own plaque look: dark wood, a gold rule, the old book's face, its button picture beside.
std::string g_tome_button_text;
int g_tome_button_half = -1;  // -1 the whole screen, 0 or 1 a co-op half
float g_tome_button_x = 0.54f, g_tome_button_y = 0.415f;  // its centre, as parts of the half's width and the screen's height
Uint64 g_tome_button_at = 0;
void draw_tome_button() {
    std::string text;
    int half;
    float fx, fy;
    Uint64 at;
    {
        std::lock_guard<std::mutex> lock(g_toast_mutex);
        text = g_tome_button_text;
        half = g_tome_button_half;
        fx = g_tome_button_x;
        fy = g_tome_button_y;
        at = g_tome_button_at;
    }
    const Uint64 now = SDL_GetTicks();  // (the game thread's ask may be a tick newer than this frame's clock)
    static unsigned drawn, skipped;
    static Uint64 said_at;
    const bool fresh = !text.empty() && at != 0 && !(now > at && now - at > 400) && ImGui::GetCurrentContext() != nullptr;
    if (!text.empty())
        (fresh ? drawn : skipped)++;
    if (!text.empty() && now - said_at > 2000) {
        said_at = now;
        std::fprintf(stderr, "[frontend] tome button: drawn %u, skipped %u (at %llu now %llu)\n", drawn, skipped, (unsigned long long)at,
                     (unsigned long long)now);
    }
    if (!fresh)
        return;
    const float dp = overlay_dp();
    const ImVec2 window = ImGui::GetMainViewport()->Size;
    const bool above = g_settings.mod_split == 1;  // (the halves one above the other)
    float left = 0.f, width = window.x, top = 0.f, height = window.y;
    if (half >= 0) {
        if (above) {
            height = window.y * 0.5f;
            top = half == 0 ? 0.f : height;
        } else {
            width = window.x * 0.5f;
            left = half == 0 ? 0.f : width;
        }
    }
    ImFont* font = g_tome_font != nullptr ? g_tome_font : (g_fps_font != nullptr ? g_fps_font : ImGui::GetFont());
    const float size = height * 0.026f;
    const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.f, text.c_str());
    const ImVec2 pad{size * 0.55f, size * 0.22f};
    const ImVec2 centre{left + width * fx, top + height * fy};
    const ImVec2 a{centre.x - extent.x * 0.5f - pad.x, centre.y - extent.y * 0.5f - pad.y};
    const ImVec2 b{centre.x + extent.x * 0.5f + pad.x, centre.y + extent.y * 0.5f + pad.y};
    ImDrawList* draw = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
    // (As the Tome's own buttons: a parchment plaque, a dark outline doubled, dark lettering.)
    draw->AddRectFilled(a, b, IM_COL32(0xc9, 0xb3, 0x86, 0xf0), size * 0.12f);
    draw->AddRect(a, b, IM_COL32(0x2e, 0x22, 0x16, 0xff), size * 0.12f, 0, 1.6f * dp);
    const float inset = 2.5f * dp;
    draw->AddRect(ImVec2{a.x + inset, a.y + inset}, ImVec2{b.x - inset, b.y - inset}, IM_COL32(0x5a, 0x44, 0x2c, 0xc0), size * 0.1f, 0,
                  1.0f * dp);
    draw->AddText(font, size, ImVec2{a.x + pad.x, a.y + pad.y}, IM_COL32(0x26, 0x1a, 0x10, 0xff), text.c_str());
}

void draw_toast() {
    draw_tome_button();
    std::string text, player[2];
    Uint64 at_ms, player_at[2];
    {
        std::lock_guard<std::mutex> lock(g_toast_mutex);
        text = g_toast;
        at_ms = g_toast_at;
        for (int i = 0; i < 2; i++) {
            player[i] = g_toast_player[i];
            player_at[i] = g_toast_player_at[i];
        }
    }
    draw_one_toast(text, at_ms, 2500, 1.f, 0.f);
    const bool above = g_settings.mod_split == 1;
    draw_one_toast(player[0], player_at[0], 5000, above ? 1.f : 0.5f, 0.f);
    draw_one_toast(player[1], player_at[1], 5000, 1.f, above ? 0.5f : 0.f);
}

void update_fps_counter() {
    draw_toast();
    static Uint64 mark = 0;
    static unsigned frames = 0;
    static int shown_fps = 0;
    if (!g_settings.fps_counter) {
        mark = 0;
        shown_fps = 0;
        return;
    }
    const Uint64 now = SDL_GetTicksNS();
    frames++;
    if (mark == 0) {
        mark = now;
        frames = 0;
    } else if (now - mark >= 1000000000ull) {
        shown_fps = static_cast<int>(frames * 1e9 / static_cast<double>(now - mark) + 0.5);
        mark = now;
        frames = 0;
    }
    if (shown_fps <= 0 || ImGui::GetCurrentContext() == nullptr)
        return;
    const float dp = overlay_dp();
    ImFont* font = g_fps_font != nullptr ? g_fps_font : ImGui::GetFont();
    char text[32];
    // Pictures shown, not this function's calls: with a Tome up in co-op the game loop takes turns between the
    // world and the Tome, two or three passes to a picture, and it said 120 at a steady 60.
    const int presented = static_cast<int>(aurora_get_fps() + 0.5f);
    std::snprintf(text, sizeof text, "%d FPS", presented > 0 ? presented : shown_fps);
    const float size = 18.f * dp;
    const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.f, text);
    const ImVec2 at{14.f * dp, 10.f * dp};
    const ImVec2 pad{8.f * dp, 3.f * dp};
    // The main viewport's list: the plain call goes through ImGui's current
    // window, which is null outside an ImGui frame (on quit, Aurora has
    // stopped starting them while the game's frames still end here).
    ImDrawList* draw = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
    draw->AddRectFilled(at, ImVec2{at.x + extent.x + 2.f * pad.x, at.y + extent.y + 2.f * pad.y},
                        IM_COL32(0, 0, 0, 0x99), 4.f * dp);
    draw->AddText(font, size, ImVec2{at.x + pad.x, at.y + pad.y}, IM_COL32(0xf0, 0xe6, 0xc8, 0xff), text);
}

// While a cutscene plays, the Skip prompt at the screen's bottom right (in the
// letterbox bar): Start's icon (its key's on the keyboard) and "Skip", as Start
// ends the scene for every save (hle.c). Through ImGui, as the FPS counter.
void update_skip_prompt() {
    const std::string file = controls::skip_glyph();
    if (file.empty() || ImGui::GetCurrentContext() == nullptr)
        return;
    static std::map<std::string, ImTextureID> icons;  // 0: the file did not load
    auto it = icons.find(file);
    if (it == icons.end()) {
        std::vector<uint8_t> rgba;
        uint32_t w = 0, h = 0;
        ImTextureID id = 0;
        if (load_png_rgba(res_dir() + "/ui/glyphs/" + file, &rgba, &w, &h))
            id = aurora_imgui_add_texture(w, h, rgba.data());
        it = icons.emplace(file, id).first;
    }
    if (it->second == 0)
        return;
    const float dp = overlay_dp();
    const ImVec2 screen = ImGui::GetMainViewport()->Size;
    const float icon = 44.f * dp;
    const float gap = 8.f * dp;
    ImFont* font = g_fps_font != nullptr ? g_fps_font : ImGui::GetFont();
    const float size = 30.f * dp;
    const char* label = "Skip";
    const ImVec2 text = font->CalcTextSizeA(size, FLT_MAX, 0.f, label);
    const float right = screen.x - 24.f * dp;
    const float top = screen.y - 18.f * dp - icon;
    const float text_left = right - text.x;
    const float icon_left = text_left - gap - icon;
    ImDrawList* draw = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
    draw->AddImage(it->second, ImVec2{icon_left, top}, ImVec2{icon_left + icon, top + icon});
    draw->AddText(font, size, ImVec2{text_left, top + (icon - text.y) / 2.f}, IM_COL32(0xf0, 0xe6, 0xc8, 0xee),
                  label);
}

namespace {

// A button's picture for the controller in hand (or the key's), as an ImGui
// texture (0 when the file did not load).
ImTextureID glyph(const std::string& set, const std::string& name) {
    static std::map<std::string, ImTextureID> icons;
    const std::string file = set + "/" + name + ".png";
    auto it = icons.find(file);
    if (it == icons.end()) {
        std::vector<uint8_t> rgba;
        uint32_t w = 0, h = 0;
        ImTextureID id = 0;
        if (load_png_rgba(res_dir() + "/ui/glyphs/" + file, &rgba, &w, &h))
            id = aurora_imgui_add_texture(w, h, rgba.data());
        it = icons.emplace(file, id).first;
    }
    return it->second;
}

// HDR calibration: the menu steps aside and the renderer draws each step's
// test pattern in place of the picture (dol_aurora_set_hdr_pattern), with
// this caption under it. 1 peak: the ring (the most a signal asks for) in a
// box at the chosen peak disappears once the box reaches what the display
// shows. 2 brightness: the game picture, SDR white to taste. 3 black: the
// game picture again, its blacks as deep as looks right (a ring a shade above
// black stayed visible well past too dark). Before the game starts there is
// no picture: grey steps instead, the darkest ones for the black level.
void draw_hdr_calibration_caption(int step) {
    if (ImGui::GetCurrentContext() == nullptr)
        return;
    static const char* const kTitles[3] = {"1 / 3  Peak brightness", "2 / 3  Brightness", "3 / 3  Black level"};
    static const char* const kHelp[3] = {
        "Raise it until the ring in the box disappears, then go back one step.",
        "How bright the picture is: set it so the game looks right to you.",
        "How deep the blacks are: lower it until black looks black, before the shadows lose their detail.",
    };
    // Before the game starts there is no picture: grey steps instead.
    static const char* const kHelpSteps[3] = {
        kHelp[0],
        "How bright the picture is: set the grey steps' white to a comfortable brightness.",
        "How deep the blacks are: lower it until black looks black, before the darkest steps disappear.",
    };
    const char* const help = g_in_game ? kHelp[step] : kHelpSteps[step];
    char value[48];
    if (step == 0)
        std::snprintf(value, sizeof value, "%d nits", g_settings.hdr_peak);
    else if (step == 1)
        std::snprintf(value, sizeof value, "%d nits", g_settings.hdr_paper);
    else
        std::snprintf(value, sizeof value, "%+d", g_settings.hdr_black);
    const std::string head = std::string(kTitles[step]) + ":  " + value;
    // The buttons, as pictures for the controller in hand (or the keys).
    struct Hint {
        std::vector<std::string> names;
        const char* text;
    };
    const std::string set = controls::glyph_set();
    const bool kb = set == "kb";
    const std::vector<Hint> hints = {
        {kb ? std::vector<std::string>{"arrow_up", "arrow_down"} : std::vector<std::string>{"dup", "ddown"}, "Adjust"},
        {{kb ? "enter" : "south"}, "Next"},
        {{kb ? "escape" : "east"}, "Back"},
    };
    const float dp = overlay_dp();
    ImFont* font = g_fps_font != nullptr ? g_fps_font : ImGui::GetFont();
    const float big = 28.f * dp, small = 17.f * dp, icon = 28.f * dp, hint_gap = 22.f * dp;
    const ImVec2 a = font->CalcTextSizeA(big, FLT_MAX, 0.f, head.c_str());
    const ImVec2 b = font->CalcTextSizeA(small, FLT_MAX, 0.f, help);
    float hints_w = 0.f;
    for (const Hint& h : hints)
        hints_w += static_cast<float>(h.names.size()) * (icon + 2.f * dp) + 6.f * dp +
                   font->CalcTextSizeA(small, FLT_MAX, 0.f, h.text).x;
    hints_w += hint_gap * static_cast<float>(hints.size() - 1);
    const ImVec2 c{hints_w, icon};
    const float pad = 16.f * dp, gap = 8.f * dp;
    const float width = std::max({a.x, b.x, c.x}) + 2.f * pad;
    const float height = a.y + b.y + c.y + 2.f * gap + 2.f * pad;
    const ImVec2 screen = ImGui::GetMainViewport()->Size;
    const ImVec2 at{(screen.x - width) / 2.f, screen.y - height - 40.f * dp};
    ImDrawList* draw = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
    draw->AddRectFilled(at, ImVec2{at.x + width, at.y + height}, IM_COL32(0, 0, 0, 0xff), 6.f * dp);
    float y = at.y + pad;
    draw->AddText(font, big, ImVec2{at.x + (width - a.x) / 2.f, y}, IM_COL32(0xf0, 0xe6, 0xc8, 0xff), head.c_str());
    y += a.y + gap;
    draw->AddText(font, small, ImVec2{at.x + (width - b.x) / 2.f, y}, IM_COL32(0xd8, 0xd0, 0xb8, 0xff), help);
    y += b.y + gap;
    float x = at.x + (width - c.x) / 2.f;
    for (const Hint& h : hints) {
        for (const std::string& name : h.names) {
            if (ImTextureID id = glyph(set, name); id != 0)
                draw->AddImage(id, ImVec2{x, y}, ImVec2{x + icon, y + icon});
            x += icon + 2.f * dp;
        }
        const ImVec2 t = font->CalcTextSizeA(small, FLT_MAX, 0.f, h.text);
        draw->AddText(font, small, ImVec2{x + 6.f * dp, y + (icon - t.y) / 2.f}, IM_COL32(0xa0, 0x98, 0x88, 0xff), h.text);
        x += 6.f * dp + t.x + hint_gap;
    }
}

// ED_HDR_CALIBRATION_TEST=seconds,step (testing): calibration opens on its
// own at that step after that many seconds and closes 4 s later.
Uint64 g_calibration_test_until = 0;

void run_hdr_calibration(int first_step = 0) {
    // The renderer's test patterns: 1 peak box, 2 the game picture, 3 dark
    // grey steps, 4 grey steps black to white.
    static const int kPatterns[3] = {1, 2, 2};
    static const int kPatternsBeforeGame[3] = {1, 4, 3};
    // From a live title page (Graphics): this loop holds the game, which the watchdog must be told of (a blocking page
    // already has).
    const bool from_live = g_live == Live::Open;
    if (from_live)
        report_pause(true);
    g_doc->Hide();
    bool pressed[kButtonCount];
    g_input.poll(pressed);
    int step = first_step;
    bool open = true;
    while (open) {
        if (g_calibration_test_until != 0 && SDL_GetTicks() >= g_calibration_test_until) {
            g_calibration_test_until = 0;
            break;
        }
        dol_aurora_set_hdr_pattern(g_in_game ? kPatterns[step] : kPatternsBeforeGame[step]);
        g_input.poll(pressed);
        const int dir = pressed[kUp] ? 1 : pressed[kDown] ? -1 : 0;
        if (dir != 0) {
            if (step == 0)
                g_settings.hdr_peak = std::clamp(g_settings.hdr_peak + dir * 50, 100, 4000);
            else if (step == 1)
                g_settings.hdr_paper = std::clamp(g_settings.hdr_paper + dir * 10, 60, 400);
            else
                g_settings.hdr_black = std::clamp(g_settings.hdr_black + dir, -10, 10);
            settings_apply_graphics();
        }
        if (pressed[kAccept] && ++step == 3)
            break;
        if (pressed[kBack] && step-- == 0)
            break;
        draw_hdr_calibration_caption(step);
        open = dol_aurora_ui_frame();
    }
    dol_aurora_set_hdr_pattern(0);
    settings_save();
    if (open)
        g_doc->Show(Rml::ModalFlag::None, Rml::FocusFlag::Document);
    if (from_live)
        report_pause(false);
}

}  // namespace

// --- Photo mode ---------------------------------------------------------------
// Hold Back (P on the keyboard): the game pauses on its frame and a free camera
// looks round it (aurora_photo_*): the left stick moves, the right stick
// looks, LB/RB go down and up, LT/RT zoom, R3 puts the camera back. A HDR,
// B glow, X HD textures, Y shadows, L3 ambient occlusion; the D-pad's up and
// down the resolution, left and right the aspect. Start takes a photo (a PNG
// in Pictures/Eternal Darkness), a tap of Back hides the legend, holding it
// again leaves. Resolution, aspect, HD textures and shadows are drawn by the
// game: it runs one frame for them, with no input, and is held again.
extern "C" void audio_set_muted(bool muted);

namespace photo {
enum class State { Off, Waiting, On };
State g_state = State::Off;
// The free camera, level with the world: its heading (left positive) and
// elevation from the game camera's, a roll (R3 held), its position and zoom.
// g_up: the world's up in the game camera's view space (camera_view_up).
float g_heading = 0.f, g_elevation = 0.f, g_roll = 0.f, g_pos[3]{0.f, 0.f, 0.f}, g_zoom = 1.f;
float g_up[3]{0.f, 1.f, 0.f};
bool g_legend = true;
int g_resolution = 0;  // into kLines
int g_aspect = 0;      // into kAspects
std::string g_notice;
Uint64 g_notice_at = 0;
unsigned g_saved = 0;
bool g_saving = false;
std::string g_last_saved;
// The graphics settings as they were: photo mode's changes are its own, put
// back when it ends and never saved.
struct Saved {
    bool hdr, hd_textures, sharp_shadows;
    int glow, ssao;
};
Saved g_before{};
// HD textures were turned on or off: the pack reads its images on threads,
// the originals showing until they are ready, so the game draws until it is
// done (up to g_textures_until).
bool g_wait_textures = false;
Uint64 g_textures_until = 0;

constexpr int kLines[] = {720, 1080, 1440, 2160};
constexpr const char* kLineNames[] = {"720p", "1080p", "1440p", "4K"};
struct Aspect {
    const char* name;
    float value;  // 0: the game's own
};
constexpr Aspect kAspects[] = {{"Game", 0.f},        {"4:3", 4.f / 3.f}, {"16:9", 16.f / 9.f},
                               {"21:9", 21.f / 9.f}, {"1:1", 1.f},       {"4:5", 4.f / 5.f}};
constexpr int kAspectCount = sizeof kAspects / sizeof kAspects[0];

void notice(std::string text) {
    g_notice = std::move(text);
    g_notice_at = SDL_GetTicks();
}

// Pictures/Eternal Darkness (made if missing), else a folder beside the game.
std::string folder() {
    std::string dir;
    if (const char* pictures = SDL_GetUserFolder(SDL_FOLDER_PICTURES); pictures != nullptr && pictures[0] != '\0') {
        dir = std::string(pictures);
        if (dir.back() != '/' && dir.back() != '\\')
            dir += '/';
        dir += "Eternal Darkness";
    } else {
        dir = "Eternal Darkness Photos";
    }
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    return dir;
}

float g_elevation0 = 0.f;  // the game camera's own elevation

void reset_view() {
    camera_view_up(g_up);
    const float n = std::sqrt(g_up[0] * g_up[0] + g_up[1] * g_up[1] + g_up[2] * g_up[2]);
    if (n > 1e-4f)
        for (float& c : g_up)
            c /= n;
    else
        g_up[0] = 0.f, g_up[1] = 1.f, g_up[2] = 0.f;
    // Ahead is -z: its elevation is asin(ahead . up).
    g_elevation0 = std::asin(std::clamp(-g_up[2], -1.f, 1.f));
    g_heading = g_roll = 0.f;
    g_elevation = g_elevation0;
    g_pos[0] = g_pos[1] = g_pos[2] = 0.f;
    g_zoom = 1.f;
}

using V3 = std::array<float, 3>;
V3 add3(V3 a, V3 b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
V3 mul3(V3 a, float k) { return {a[0] * k, a[1] * k, a[2] * k}; }
float dot3(V3 a, V3 b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
V3 cross3(V3 a, V3 b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
V3 unit3(V3 a) {
    const float n = std::sqrt(dot3(a, a));
    return n > 1e-6f ? mul3(a, 1.f / n) : a;
}

// The level frame: the game camera's ahead and right flattened onto the
// floor (h0, r0), and the world's up.
struct Frame {
    V3 up, h0, r0;
};
Frame level_frame() {
    const V3 up{g_up[0], g_up[1], g_up[2]};
    V3 h = add3(V3{0.f, 0.f, -1.f}, mul3(up, -dot3(V3{0.f, 0.f, -1.f}, up)));
    if (dot3(h, h) < 1e-6f)  // looking straight down or up: the camera's up flattened
        h = add3(V3{0.f, 1.f, 0.f}, mul3(up, -dot3(V3{0.f, 1.f, 0.f}, up)));
    h = unit3(h);
    return {up, h, unit3(cross3(h, up))};
}

int current_lines() {
    static const int kRenderLines[] = {0, 720, 1080, 1440, 2160};
    return kRenderLines[std::clamp(g_settings.render_scale, 0, 4)];
}

struct Row {
    std::vector<std::string> pad, keys;  // glyph files: the controller's, the keyboard's
    std::string text;
};

// See-through grey (with HDR on, the brightest lamps show through a little).
constexpr ImU32 kPanel = IM_COL32(0x22, 0x22, 0x24, 0xb8);

// The legend in the bottom-left corner (two columns of button pictures and
// what they do) and a notice over the top.
void draw() {
    if (ImGui::GetCurrentContext() == nullptr)
        return;
    const float dp = overlay_dp();
    ImFont* font = g_fps_font != nullptr ? g_fps_font : ImGui::GetFont();
    ImDrawList* list = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
    const ImVec2 screen = ImGui::GetMainViewport()->Size;
    const ImU32 ink = IM_COL32(0xf0, 0xe6, 0xc8, 0xff), dim = IM_COL32(0xc8, 0xbe, 0xa4, 0xff);
    const Uint64 now = SDL_GetTicks();
    if (!g_notice.empty() && now - g_notice_at < 2500) {
        const float size = 22.f * dp;
        const ImVec2 t = font->CalcTextSizeA(size, FLT_MAX, 0.f, g_notice.c_str());
        const ImVec2 pad{14.f * dp, 8.f * dp};
        const ImVec2 at{(screen.x - t.x) / 2.f - pad.x, 24.f * dp};
        list->AddRectFilled(at, ImVec2{at.x + t.x + 2.f * pad.x, at.y + t.y + 2.f * pad.y}, kPanel, 6.f * dp);
        list->AddText(font, size, ImVec2{at.x + pad.x, at.y + pad.y}, ink, g_notice.c_str());
    }
    if (!g_legend)
        return;
    using namespace enhancement;
    const std::string set = controls::glyph_set();
    const bool kb = set == "kb";
    const auto on = [](bool b) { return std::string(b ? "On" : "Off"); };
    const std::vector<Row> settings = {
        {{"south"}, {"1"}, "HDR: " + (available(kHdr) ? on(g_settings.hdr) : std::string("Not on this display"))},
        {{"east"}, {"2"}, "Glow: " + value(kGlow)},
        {{"west"}, {"3"}, "HD textures: " + (available(kHdTextures) ? on(g_settings.hd_textures) : std::string("No pack"))},
        {{"north"}, {"4"}, "Shadows: " + value(kShadows)},
        {{"l3"}, {"5"}, "Ambient occlusion: " + value(kOcclusion)},
        {{"dup", "ddown"}, {"page_up", "page_down"}, std::string("Resolution: ") + kLineNames[g_resolution]},
        {{"dleft", "dright"}, {"home", "end"}, std::string("Ratio: ") + kAspects[g_aspect].name},
    };
    const std::vector<Row> camera = {
        {{"stick_l"}, {"w", "a", "s", "d"}, "Move"},
        {{"stick_r"}, {"arrows"}, "Look"},
        {{"lb", "rb"}, {"q", "e"}, "Down / up"},
        {{"lt", "rt"}, {"z", "c"}, "Zoom out / in"},
        {{"r3"}, {"r"}, "Reset \u00b7 hold + look to tilt"},
        {{"start"}, {"enter"}, "Take photo"},
        {{"back"}, {"h"}, kb ? "Hide this" : "Hide this \u00b7 hold to leave"},
    };
    std::vector<Row> right = camera;
    if (kb)
        right.push_back({{}, {"p"}, "Leave photo mode"});

    const float size = 18.f * dp, title = 22.f * dp, icon = 30.f * dp, gap = 4.f * dp, row_h = icon + gap;
    const ImVec2 pad{16.f * dp, 12.f * dp};
    const auto column_width = [&](const std::vector<Row>& rows) {
        float w = 0.f;
        for (const Row& r : rows) {
            const size_t n = kb ? r.keys.size() : r.pad.size();
            const float text = font->CalcTextSizeA(size, FLT_MAX, 0.f, r.text.c_str()).x;
            w = std::max(w, static_cast<float>(n) * (icon + 2.f * dp) + 8.f * dp + text);
        }
        return w;
    };
    const float left_w = column_width(settings), right_w = column_width(right), col_gap = 28.f * dp;
    const float head_h = font->CalcTextSizeA(title, FLT_MAX, 0.f, "Photo mode").y + 6.f * dp;
    const float body_h = static_cast<float>(std::max(settings.size(), right.size())) * row_h;
    const float width = left_w + col_gap + right_w + 2.f * pad.x, height = head_h + body_h + 2.f * pad.y;
    const ImVec2 at{24.f * dp, screen.y - 24.f * dp - height};
    list->AddRectFilled(at, ImVec2{at.x + width, at.y + height}, kPanel, 8.f * dp);
    list->AddText(font, title, ImVec2{at.x + pad.x, at.y + pad.y}, ink, "Photo mode");
    const auto column = [&](const std::vector<Row>& rows, float x) {
        float y = at.y + pad.y + head_h;
        for (const Row& r : rows) {
            float cx = x;
            for (const std::string& name : kb ? r.keys : r.pad) {
                if (ImTextureID id = glyph(set, name); id != 0)
                    list->AddImage(id, ImVec2{cx, y}, ImVec2{cx + icon, y + icon});
                cx += icon + 2.f * dp;
            }
            const float th = font->CalcTextSizeA(size, FLT_MAX, 0.f, r.text.c_str()).y;
            list->AddText(font, size, ImVec2{cx + 6.f * dp, y + (icon - th) / 2.f}, dim, r.text.c_str());
            y += row_h;
        }
    };
    column(settings, at.x + pad.x);
    column(right, at.x + pad.x + left_w + col_gap);
}

void begin() {
    if (g_state != State::Off)
        return;
    reset_view();
    g_legend = true;
    g_aspect = 0;
    int lines = current_lines();
    if (lines == 0) {
        // The screen's own resolution (Screen/Window): its height, as the
        // nearest of photo mode's choices (a 4K TV is 4K, not the 1080p
        // it fell back to).
        uint32_t w = 0, h = 0;
        dol_aurora_get_display_info(nullptr, &w, &h);
        lines = static_cast<int>(h);
    }
    g_resolution = 1;
    for (int i = 0; i < 4; i++)
        if (std::abs(kLines[i] - lines) < std::abs(kLines[g_resolution] - lines))
            g_resolution = i;
    g_saved = aurora_photo_saved();
    g_saving = false;
    g_before = {g_settings.hdr, g_settings.hd_textures, g_settings.sharp_shadows, g_settings.glow, g_settings.ssao};
    g_state = State::Waiting;
    audio_set_muted(true);
    aurora_photo_begin();
    std::fprintf(stderr, "[photo] on\n");
}

void end() {
    aurora_photo_end();
    dol_aurora_set_photo_aspect(0.f);
    dol_aurora_set_render_height(current_lines());
    const bool changed = g_settings.hdr != g_before.hdr || g_settings.hd_textures != g_before.hd_textures ||
                         g_settings.sharp_shadows != g_before.sharp_shadows || g_settings.glow != g_before.glow ||
                         g_settings.ssao != g_before.ssao;
    g_settings.hdr = g_before.hdr;
    g_settings.hd_textures = g_before.hd_textures;
    g_settings.sharp_shadows = g_before.sharp_shadows;
    g_settings.glow = g_before.glow;
    g_settings.ssao = g_before.ssao;
    if (changed)
        settings_apply_graphics();
    audio_set_muted(false);
    g_state = State::Off;
    std::fprintf(stderr, "[photo] off\n");
}

// ED_PHOTO_TEST=<seconds>: photo mode by itself after that long (glow, move,
// look, a photo, 21:9, HD textures off and on, a photo, leave), for tests
// with no input.
struct Test {
    bool on = false;
    Uint64 enter_at = 0, began = 0;
    int step = 0;
};
Test g_test;

// ED_PHOTO_PORTRAIT=frame,height,distance,file (tools/ed/form_portraits.py): photo mode at that
// frame of the game, the camera on the line from the game's camera to player 1's head (that high
// over where they stand), that far from it, one photo to that file, and the game closes.
struct Portrait {
    bool on = false;
    unsigned frame = 0;
    float height = 0.f, distance = 0.f;
    char file[512] = {0};
    int step = 0;
    Uint64 at = 0;
};
Portrait g_portrait;

enum class Exit { Leave, Step, Quit };

void portrait_aim();

constexpr int kKeys[] = {SDL_SCANCODE_RETURN, SDL_SCANCODE_H,      SDL_SCANCODE_P,        SDL_SCANCODE_1,
                         SDL_SCANCODE_2,      SDL_SCANCODE_3,      SDL_SCANCODE_4,        SDL_SCANCODE_5,
                         SDL_SCANCODE_R,      SDL_SCANCODE_PAGEUP, SDL_SCANCODE_PAGEDOWN, SDL_SCANCODE_HOME,
                         SDL_SCANCODE_END};

// Paused at the game's frame: the camera, the settings, the legend.
Exit run() {
    SDL_Gamepad* pad = gamepad();
    bool was[SDL_GAMEPAD_BUTTON_COUNT]{};
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++)
        was[b] = pad != nullptr && SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(b));
    static bool key_was[SDL_SCANCODE_COUNT];
    for (int k : kKeys)
        key_was[k] = key(k);
    // Back held from before (the hold that opened it): wait for its release.
    bool back_armed = !(pad != nullptr && SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK));
    Uint64 back_since = 0;
    Uint64 last = SDL_GetTicksNS();
    while (true) {
        pad = gamepad();
        const Uint64 now_ns = SDL_GetTicksNS();
        const float dt = std::min(static_cast<float>(now_ns - last) * 1e-9f, 0.1f);
        last = now_ns;
        const auto button = [&](SDL_GamepadButton b) { return pad != nullptr && SDL_GetGamepadButton(pad, b); };
        bool pressed[SDL_GAMEPAD_BUTTON_COUNT]{};
        for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
            const bool down = button(static_cast<SDL_GamepadButton>(b));
            pressed[b] = down && !was[b];
            was[b] = down;
        }
        bool key_pressed[SDL_SCANCODE_COUNT]{};
        for (int k : kKeys) {
            const bool down = key(k);
            key_pressed[k] = down && !key_was[k];
            key_was[k] = down;
        }
        const auto axis = [&](SDL_GamepadAxis a) {
            if (pad == nullptr)
                return 0.f;
            const float v = SDL_GetGamepadAxis(pad, a) / 32767.f;
            return std::fabs(v) < 0.15f ? 0.f : v;
        };
        float mx = axis(SDL_GAMEPAD_AXIS_LEFTX), mz = -axis(SDL_GAMEPAD_AXIS_LEFTY);
        float lx = axis(SDL_GAMEPAD_AXIS_RIGHTX), ly = -axis(SDL_GAMEPAD_AXIS_RIGHTY);
        float my = (button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) ? 1.f : 0.f) -
                   (button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) ? 1.f : 0.f);
        float zoom = std::max(axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER), 0.f) -
                     std::max(axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER), 0.f);
        // The keyboard: WASD move, arrows look, Q/E down/up, Z/C zoom.
        mx += (key(SDL_SCANCODE_D) ? 1.f : 0.f) - (key(SDL_SCANCODE_A) ? 1.f : 0.f);
        mz += (key(SDL_SCANCODE_W) ? 1.f : 0.f) - (key(SDL_SCANCODE_S) ? 1.f : 0.f);
        my += (key(SDL_SCANCODE_E) ? 1.f : 0.f) - (key(SDL_SCANCODE_Q) ? 1.f : 0.f);
        lx += (key(SDL_SCANCODE_RIGHT) ? 1.f : 0.f) - (key(SDL_SCANCODE_LEFT) ? 1.f : 0.f);
        ly += (key(SDL_SCANCODE_UP) ? 1.f : 0.f) - (key(SDL_SCANCODE_DOWN) ? 1.f : 0.f);
        zoom += (key(SDL_SCANCODE_C) ? 1.f : 0.f) - (key(SDL_SCANCODE_Z) ? 1.f : 0.f);

        bool take = pressed[SDL_GAMEPAD_BUTTON_START] || key_pressed[SDL_SCANCODE_RETURN];
        bool hdr = pressed[SDL_GAMEPAD_BUTTON_SOUTH] || key_pressed[SDL_SCANCODE_1];
        bool glow = pressed[SDL_GAMEPAD_BUTTON_EAST] || key_pressed[SDL_SCANCODE_2];
        bool textures = pressed[SDL_GAMEPAD_BUTTON_WEST] || key_pressed[SDL_SCANCODE_3];
        bool shadows = pressed[SDL_GAMEPAD_BUTTON_NORTH] || key_pressed[SDL_SCANCODE_4];
        bool occlusion = pressed[SDL_GAMEPAD_BUTTON_LEFT_STICK] || key_pressed[SDL_SCANCODE_5];
        // R3 (or R): a tap resets the camera; held, the look turns into a tilt.
        bool reset = false;
        const bool tilt_held = button(SDL_GAMEPAD_BUTTON_RIGHT_STICK) || key(SDL_SCANCODE_R);
        static bool tilt_was = false, tilted = false;
        if (tilt_held && !tilt_was)
            tilted = false;
        if (!tilt_held && tilt_was && !tilted)
            reset = true;
        tilt_was = tilt_held;
        int res = (pressed[SDL_GAMEPAD_BUTTON_DPAD_UP] || key_pressed[SDL_SCANCODE_PAGEUP] ? 1 : 0) -
                  (pressed[SDL_GAMEPAD_BUTTON_DPAD_DOWN] || key_pressed[SDL_SCANCODE_PAGEDOWN] ? 1 : 0);
        int ratio = (pressed[SDL_GAMEPAD_BUTTON_DPAD_RIGHT] || key_pressed[SDL_SCANCODE_END] ? 1 : 0) -
                    (pressed[SDL_GAMEPAD_BUTTON_DPAD_LEFT] || key_pressed[SDL_SCANCODE_HOME] ? 1 : 0);
        int ratio_to = -1;
        bool hide = key_pressed[SDL_SCANCODE_H];
        bool leave = key_pressed[SDL_SCANCODE_P];
        // Back: a tap hides the legend, a hold leaves.
        const bool back = button(SDL_GAMEPAD_BUTTON_BACK);
        if (!back_armed) {
            back_armed = !back;
        } else if (back) {
            if (back_since == 0)
                back_since = SDL_GetTicks();
            if (SDL_GetTicks() - back_since >= 500)
                leave = true;
        } else if (back_since != 0) {
            hide = true;
            back_since = 0;
        }

        if (g_portrait.on) {
            mx = mz = my = lx = ly = zoom = 0.f;
            const Uint64 ticks = SDL_GetTicks();
            if (g_portrait.step == 0) {
                portrait_aim();
                g_legend = false;
                g_portrait.at = ticks;
                g_portrait.step = 1;
            } else if (g_portrait.step == 1 && ticks - g_portrait.at > 500) {
                g_last_saved = g_portrait.file;
                aurora_photo_save(g_last_saved.c_str());
                g_saving = true;
                g_portrait.step = 2;
            } else if (g_portrait.step == 2 && aurora_photo_saved() != g_saved) {
                std::fprintf(stderr, "[photo] portrait %s\n", g_portrait.file);
                return Exit::Quit;
            }
        }

        // The test script.
        if (g_test.on) {
            const Uint64 t = SDL_GetTicks() - g_test.began;
            const int step = g_test.step;
            if (step == 0 && t > 1000) {
                glow = true;  // for the photo only: put back at the end
                g_test.step++;
            } else if (step == 1 && t > 1500) {
                mz = 1.f;
                if (t > 2500)
                    g_test.step++;
            } else if (step == 2) {
                lx = 1.f;
                if (t > 3200)
                    g_test.step++;
            } else if (step == 3 && t > 3600) {
                take = true;
                g_test.step++;
            } else if (step == 4 && t > 5000) {
                ratio_to = 3;  // 21:9
                g_test.step++;
            } else if (step == 5 && t > 6500) {
                textures = true;  // off
                shadows = true;
                g_test.step++;
            } else if (step == 6 && t > 8000) {
                textures = true;  // on again: read back from the pack
                shadows = true;   // the sharp light maps made again
                g_test.step++;
            } else if (step == 7 && t > 9500) {
                take = true;
                g_test.step++;
            } else if (step == 8 && t > 10500) {
                leave = true;
                g_test.step++;
            }
        }

        using namespace enhancement;
        bool step = false;
        // For the photo only: put back as it was when photo mode ends (end),
        // never saved.
        const auto toggle = [](int which) {
            if (!available(which))
                return;
            change(which, +1);
            notice(std::string(kNames[which]) + ": " + value(which));
        };
        if (hdr)
            toggle(kHdr);
        if (glow)
            toggle(kGlow);
        if (occlusion)
            toggle(kOcclusion);
        if (textures) {
            toggle(kHdTextures);
            g_wait_textures = true;
            g_textures_until = 0;
            step = true;
        }
        if (shadows) {
            toggle(kShadows);
            g_wait_textures = true;  // the sharp light maps are made on a worker
            g_textures_until = 0;
            step = true;
        }
        if (reset) {
            reset_view();
            notice("Camera back to the game's");
        }
        if (hide)
            g_legend = !g_legend;
        if (res != 0) {
            g_resolution = std::clamp(g_resolution + res, 0, 3);
            dol_aurora_set_render_height(kLines[g_resolution]);
            notice(std::string("Resolution: ") + kLineNames[g_resolution]);
            step = true;
        }
        if (ratio != 0 || ratio_to >= 0) {
            g_aspect = ratio_to >= 0 ? ratio_to : (g_aspect + ratio + kAspectCount) % kAspectCount;
            dol_aurora_set_photo_aspect(kAspects[g_aspect].value);
            notice(std::string("Ratio: ") + kAspects[g_aspect].name);
            step = true;
        }
        if (take) {
            char stamp[64];
            const std::time_t now = std::time(nullptr);
            std::strftime(stamp, sizeof stamp, "%Y-%m-%d %H.%M.%S", std::localtime(&now));
            g_last_saved = folder() + "/Eternal Darkness " + stamp + ".png";
            aurora_photo_save(g_last_saved.c_str());
            g_saving = true;
            notice("Taking photo...");
        }
        if (g_saving && aurora_photo_saved() != g_saved) {
            g_saved = aurora_photo_saved();
            g_saving = false;
            notice("Saved to Pictures/Eternal Darkness");
        }

        // The camera, level with the world: look turns about the world's up
        // and tips up and down (R3 held: tilts instead); the left stick moves
        // along the floor, LB/RB straight up and down (game units: about 100
        // a metre); zoom 0.3x to 4x.
        if (tilt_held) {
            if (lx != 0.f)
                tilted = true;
            g_roll -= lx * 1.2f * dt;
        } else {
            g_heading -= lx * 1.8f * dt;
            g_elevation = std::clamp(g_elevation + ly * 1.4f * dt, -1.5f, 1.5f);
        }
        g_zoom = std::clamp(g_zoom * std::exp(zoom * 1.2f * dt), 0.3f, 4.f);
        const Frame fr = level_frame();
        const float ch = std::cos(g_heading), sh = std::sin(g_heading);
        const V3 level_ahead = add3(mul3(fr.h0, ch), mul3(fr.r0, -sh));
        const V3 level_right = add3(mul3(fr.r0, ch), mul3(fr.h0, sh));
        const V3 ahead = add3(mul3(level_ahead, std::cos(g_elevation)), mul3(fr.up, std::sin(g_elevation)));
        const V3 up0 = cross3(level_right, ahead);
        const float cr = std::cos(g_roll), sr = std::sin(g_roll);
        const V3 right = add3(mul3(level_right, cr), mul3(up0, sr));
        const V3 up = add3(mul3(up0, cr), mul3(level_right, -sr));
        const float speed = 180.f / g_zoom;
        const V3 move = add3(add3(mul3(level_right, mx), mul3(level_ahead, mz)), mul3(fr.up, my));
        for (int i = 0; i < 3; i++)
            g_pos[i] += move[i] * speed * dt;
        // Row-major, columns right, up, back.
        const float rot[9]{right[0], up[0], -ahead[0], right[1], up[1], -ahead[1], right[2], up[2], -ahead[2]};
        aurora_photo_set_view(rot, g_pos[0], g_pos[1], g_pos[2], g_zoom, dol_aurora_display_aspect(), true);

        if (leave)
            return Exit::Leave;
        if (step) {
            // Two frames: a new size is only drawn from the frame after.
            aurora_photo_rehold(1);
            return Exit::Step;
        }
        draw();
        if (!dol_aurora_ui_frame())
            return Exit::Quit;
    }
}

void portrait_aim() {
    float place[3], eye[3], target[3];
    unsigned at = 0;
    if (!hle_player_place(place, &at) || !camera_last_view(eye, target))
        return;
    const V3 ahead = unit3(V3{target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]});
    const V3 right = unit3(cross3(ahead, V3{0.f, 0.f, 1.f}));
    const V3 up = cross3(right, ahead);
    const V3 to{place[0] - eye[0], place[1] - eye[1], place[2] + g_portrait.height - eye[2]};
    const V3 head{dot3(to, right), dot3(to, up), -dot3(to, ahead)};
    const V3 along = unit3(head);
    const V3 from = add3(head, mul3(along, -g_portrait.distance));
    for (int i = 0; i < 3; i++)
        g_pos[i] = from[i];
    const Frame fr = level_frame();
    g_elevation = std::asin(std::clamp(dot3(along, fr.up), -1.f, 1.f));
    const V3 level = unit3(add3(along, mul3(fr.up, -dot3(along, fr.up))));
    g_heading = std::atan2(-dot3(level, fr.r0), dot3(level, fr.h0));
    g_roll = 0.f;
    g_zoom = 1.f;
}

// At each frame boundary: enter (once held), run, step or leave.
void frame(bool start) {
    static bool checked = false;
    if (!checked) {
        checked = true;
        if (const char* t = std::getenv("ED_PHOTO_PORTRAIT");
            t != nullptr && std::sscanf(t, "%u,%f,%f,%511[^\n]", &g_portrait.frame, &g_portrait.height, &g_portrait.distance, g_portrait.file) == 4)
            g_portrait.on = true;
        if (const char* t = std::getenv("ED_PHOTO_TEST"); t != nullptr && std::atoi(t) > 0) {
            g_test.on = true;
            g_test.enter_at = SDL_GetTicks() + static_cast<Uint64>(std::atoi(t)) * 1000u;
        }
    }
    if (g_test.on && g_test.enter_at != 0 && SDL_GetTicks() >= g_test.enter_at) {
        g_test.enter_at = 0;
        start = true;
    }
    if (g_portrait.on && g_portrait.step == 0 && g_state == State::Off) {
        float place[3];
        unsigned at = 0;
        if (hle_player_place(place, &at) && at >= g_portrait.frame)
            start = true;
    }
    if (start && g_state == State::Off) {
        begin();
        return;  // the game draws the frame to hold
    }
    if (g_state != State::Waiting)
        return;
    // The held frame is finished on the render thread a moment after the
    // game hands it over.
    for (int i = 0; i < 50 && !aurora_photo_ready(); i++)
        SDL_Delay(2);
    if (!aurora_photo_ready())
        return;
    if (g_wait_textures) {
        static int frames = 0;
        if (g_textures_until == 0) {
            g_textures_until = SDL_GetTicks() + 3000;
            frames = 0;
            std::fprintf(stderr, "[photo] textures: %u waiting to upload, %u light maps being made%s\n",
                         aurora_texture_pack_pending(), aurora_texture_override_pending(),
                         texpack_busy() ? ", the pack still reading" : "");
        }
        const bool busy =
            texpack_busy() || aurora_texture_pack_pending() != 0 || aurora_texture_override_pending() != 0;
        if (busy && SDL_GetTicks() < g_textures_until) {
            frames++;
            aurora_photo_rehold(0);  // another frame, still black and silent
            return;
        }
        std::fprintf(stderr, "[photo] textures %s after %d more frame(s) (%u to upload, %u light maps%s)\n",
                     busy ? "still loading" : "loaded", frames, aurora_texture_pack_pending(),
                     aurora_texture_override_pending(), texpack_busy() ? ", the pack reading" : "");
        g_wait_textures = false;
    }
    if (g_test.on && g_test.began == 0)
        g_test.began = SDL_GetTicks();
    g_state = State::On;
    report_pause(true);
    const Exit exit = run();
    report_pause(false);
    if (exit == Exit::Step) {
        g_state = State::Waiting;
        return;
    }
    end();
    if (exit == Exit::Quit)
        dol_aurora_request_quit();
}
}  // namespace photo

extern "C" bool frontend_photo_active(void) { return photo::g_state != photo::State::Off; }

// A page asked for by the game's own menus (any thread): the Tome's Options stops (hle.c tome_stops) and the title's
// Options row. frontend_frame opens it over the paused game, driven by that player's controller; Back closes it.
static std::atomic<int> g_open_page{-1};
static std::atomic<int> g_open_page_player{0};
static std::atomic<int> g_open_page_skin{FRONTEND_SKIN_LAUNCHER};
static std::atomic<int> g_open_page_half{-1};
static std::atomic<unsigned> g_menu_closed{0};
extern "C" void frontend_open_page_skin(int page, int player, int skin, int half) {
    g_open_page_player = player == 1 ? 1 : 0;
    g_open_page_skin = skin;
    g_open_page_half = half == 0 || half == 1 ? half : -1;
    g_open_page = page;  // last: frontend_frame takes the page, then the rest
}
extern "C" void frontend_open_page(int page, int player) {
    frontend_open_page_skin(page, player, FRONTEND_SKIN_LAUNCHER, -1);
}
extern "C" unsigned frontend_menu_closed_count(void) { return g_menu_closed.load(); }
extern "C" void frontend_open_characters(void) { frontend_open_page(FRONTEND_PAGE_CHARACTERS, 0); }
// A Tome-looking button for this frame (any thread): text, the half it's in (-1 the whole screen) and its centre.
extern "C" void frontend_tome_button(const char* text, int half, float x, float y) {
    std::lock_guard<std::mutex> lock(g_toast_mutex);
    g_tome_button_text = text != nullptr ? text : "";
    g_tome_button_half = half;
    g_tome_button_x = x;
    g_tome_button_y = y;
    g_tome_button_at = SDL_GetTicks();
}

extern "C" void frontend_toast_player(int player, const char* text) {
    if (player < 0 || player > 1)
        return;
    std::lock_guard<std::mutex> lock(g_toast_mutex);
    g_toast_player[player] = text;
    g_toast_player_at[player] = SDL_GetTicks();
}

extern "C" void frontend_toast(const char* text) {
    std::lock_guard<std::mutex> lock(g_toast_mutex);
    g_toast = text;
    g_toast_at = SDL_GetTicks();
}

// Mods' Skip intro, the first time: the start-up passed over behind a black
// screen with no sound, until the title menu is up and saved (hle.c).
void update_start_up_cover() {
    static bool covering = false;
    const bool hide = hle_start_up_hidden();
    if (hide != covering) {
        covering = hide;
        audio_set_muted(hide);
    }
    if (!hide)
        return;
    const ImGuiViewport* view = ImGui::GetMainViewport();
    ImGui::GetForegroundDrawList(const_cast<ImGuiViewport*>(view))
        ->AddRectFilled(view->Pos, ImVec2(view->Pos.x + view->Size.x, view->Pos.y + view->Size.y),
                        IM_COL32(0, 0, 0, 255));
}

extern "C" bool hle_coop_ui_over(bool on, int half);

extern "C" bool frontend_page_live(void) { return g_live_flag.load(); }

static bool g_menu_was_held = false;  // (Escape held at the last frame: frontend_frame)

// ED_TITLE_PAGE_PAUSE=1 (testing, A/B): title pages block the game as every other page does, as before 2026-11-17.
static bool title_page_pause() {
    static const bool on = std::getenv("ED_TITLE_PAGE_PAUSE") != nullptr && std::atoi(std::getenv("ED_TITLE_PAGE_PAUSE")) != 0;
    return on;
}

// After any game-asked page or debug menu closes, blocking or live: the menu's state as before it opened, and the count
// a caller waits on moved on. (The watchdog's report_pause stays with the blocking way only: a live page never calls it.)
static void page_closed() {
    hle_coop_ui_over(false, -1);
    g_test_close_at = 0;  // (ED_PAGE_TEST_MS: a page closed sooner leaves no deadline for the next menu)
    g_test_key_next = 0;
    g_root_closes = false;
    g_on_title = false;
    g_skin = Skin::Launcher;  // the Esc debug menu and the launcher keep the launcher's look
    g_skin_half = -1;
    g_list_lines = 0;
    g_list_top = 0;
    g_menu_player = 0;
    g_menu_was_held = false;
    g_menu_closed++;
}

// A live title page's frame (frontend_frame, before it reads any button): one step of the page, drawn by the game's own
// present; after the close, the wait for every button to be let go.
// (The page's first frames: its slowest step and the longest wait between two of the game's frames, logged once.)
static unsigned g_live_frames;
static Uint64 g_live_slow_step_ns, g_live_slow_gap_ns, g_live_last_ns;
static void live_page_frame() {
    if (g_live == Live::Open) {
        const Uint64 t0 = SDL_GetTicksNS();
        const MenuStep step = menu_step();
        if (g_live_frames < 60) {
            g_live_slow_step_ns = std::max(g_live_slow_step_ns, SDL_GetTicksNS() - t0);
            g_live_slow_gap_ns = std::max(g_live_slow_gap_ns, t0 - g_live_last_ns);
            g_live_last_ns = t0;
            if (++g_live_frames == 60)
                std::fprintf(stderr, "[frontend] the live page's first 60 frames: slowest step %.1f ms, longest frame %.1f ms\n",
                             g_live_slow_step_ns / 1e6, g_live_slow_gap_ns / 1e6);
        }
        if (step == MenuStep::Open)
            return;
        g_doc->Hide();
        g_live = Live::Release;
    }
    if (g_input.any_held()) {
        g_input.poll(g_menu_pressed);
        return;
    }
    page_closed();
    g_live = Live::Off;
    g_live_flag = false;
    std::fprintf(stderr, "[frontend] the live page closed (%llu ms)\n", static_cast<unsigned long long>(SDL_GetTicks()));
}

extern "C" void frontend_frame(void) {
    const Uint64 entry_ns = SDL_GetTicksNS();
    update_start_up_cover();
    static Uint64 calibration_test_at = 0;
    static int calibration_test_step = 0;
    static bool calibration_test_checked = false;
    if (!calibration_test_checked) {
        calibration_test_checked = true;
        unsigned long after = 0;
        int step = 0;
        if (const char* t = std::getenv("ED_HDR_CALIBRATION_TEST"); t && std::sscanf(t, "%lu,%d", &after, &step) >= 1) {
            calibration_test_at = SDL_GetTicks() + after * 1000u;
            calibration_test_step = std::clamp(step, 0, 2);
        }
    }
    if (calibration_test_at != 0 && SDL_GetTicks() >= calibration_test_at && ensure_ui()) {
        calibration_test_at = 0;
        g_calibration_test_until = SDL_GetTicks() + 4000;
        run_hdr_calibration(calibration_test_step);
    }
    update_fps_counter();
    update_skip_prompt();
    update_cursor();
    if (g_live != Live::Off) {
        live_page_frame();  // (before the Back / Esc / photo reads: a Back on the page is the page's)
        return;
    }
    warm_title_look();
    bool& menu_was_held = g_menu_was_held;
    bool held[kButtonCount];
    read_buttons(held);
    // Back: a tap opens the menu (on its release), held half a second photo
    // mode; Escape opens the menu at once, P photo mode.
    static Uint64 back_since = 0;
    static bool back_photo = false;
    const bool back = held[kMenu];
    bool photo_start = false, back_tap = false;
    if (back && back_since == 0) {
        back_since = SDL_GetTicks();
        back_photo = false;
    }
    if (back && !back_photo && SDL_GetTicks() - back_since >= 500) {
        back_photo = true;
        photo_start = true;
    }
    if (!back && back_since != 0) {
        back_tap = !back_photo;
        back_since = 0;
    }
    static bool p_was = false;
    const bool p = key(SDL_SCANCODE_P);
    if (p && !p_was)
        photo_start = true;
    p_was = p;
    photo::frame(photo_start && photo::g_state == photo::State::Off);
    if (photo::g_state != photo::State::Off) {
        menu_was_held = true;
        std::fill(std::begin(g_key_pressed), std::end(g_key_pressed), false);
        return;
    }
    const bool menu_held = key(SDL_SCANCODE_ESCAPE);
    const bool press = (menu_held && !menu_was_held) || g_key_pressed[kMenu] || back_tap;
    menu_was_held = menu_held;
    std::fill(std::begin(g_key_pressed), std::end(g_key_pressed), false);
    static Uint64 test_open_at = 0, test_hold = 0, test_every = 0;
    static bool test_checked = false;
    if (!test_checked) {
        test_checked = true;
        unsigned long after = 0, hold = 0, every = 0;
        if (const char* t = std::getenv("ED_MENU_TEST"); t && std::sscanf(t, "%lu,%lu,%lu", &after, &hold, &every) >= 2) {
            test_open_at = SDL_GetTicks() + after * 1000u;
            test_hold = hold;
            test_every = every * 1000u;
        }
    }
    bool test_press = false;
    if (test_open_at != 0 && SDL_GetTicks() >= test_open_at) {
        test_open_at = test_every != 0 ? SDL_GetTicks() + test_hold + test_every : 0;
        test_press = true;
        g_test_close_at = SDL_GetTicks() + test_hold;
        std::fprintf(stderr, "[frontend] test menu open for %llu ms\n", (unsigned long long)test_hold);
    }
    // A page the game's own menus asked for (the Tome's Options, the title's Options): the menu opens straight on it.
    // Esc / Back open the launcher's menu over the game only as a debug menu (Options > Debug menu in game,
    // or ED_DEBUG_MENU=1): in play everything is in the game's own menus.
    const int page = g_open_page.exchange(-1);
    static int debug_env = -1;
    if (debug_env < 0)
        debug_env = std::getenv("ED_DEBUG_MENU") != nullptr && std::atoi(std::getenv("ED_DEBUG_MENU")) != 0;
    const bool debug = g_settings.debug_menu || debug_env == 1;
    if (!(press && debug) && !test_press && page < 0)
        return;
    if (!ensure_ui()) {
        // (No Solo / Co-op page can show: the title's Start Game or Load Game goes on in this run's mode.)
        if (page == FRONTEND_PAGE_TITLE_PICK)
            g_title_pick_result = (g_open_page_player.load() == 1 ? 2 : 0) + (g_settings.mod_coop ? 1 : 0);
        if (page >= 0)
            g_menu_closed++;  // the asked-for page is used up and can't open: a caller waiting on the count goes on
        return;
    }
    g_in_game = true;
    g_screen = Screen::InGame;
    g_options_return = Screen::InGame;
    g_mods_return = Screen::InGame;
    g_selected = 0;
    g_root_closes = page >= 0;
    g_on_title = page >= FRONTEND_PAGE_TITLE_SETTINGS && page <= FRONTEND_PAGE_TITLE_PICK;
    g_menu_player = 0;
    g_skin = Skin::Launcher;
    g_skin_half = -1;
    g_list_top = 0;
    g_list_lines = 0;
    if (const char* t = std::getenv("ED_LIST_LINES"); t != nullptr && std::atoi(t) > 0)
        g_list_lines = std::atoi(t);  // (testing: the list scroll in any look; the looks will set their own)
    if (page >= 0) {
        const int player = g_open_page_player.load();
        const int skin = g_open_page_skin.load();
        g_skin = skin == FRONTEND_SKIN_TITLE ? Skin::Title : skin == FRONTEND_SKIN_TOME ? Skin::Tome : Skin::Launcher;
        g_skin_half = g_skin == Skin::Tome ? g_open_page_half.load() : -1;
        if (g_skin == Skin::Title) {
            ensure_title_art();
            if (g_list_lines == 0)
                g_list_lines = kTitleLines;
        }
        if (g_skin == Skin::Tome && g_list_lines == 0)
            g_list_lines = kTomeLines;
        std::fprintf(stderr, "[frontend] page %d opened from the game's menu for player %d (skin %d, half %d)\n", page,
                     player + 1, skin, g_skin_half);
        // ED_PAGE_TEST_KEYS (testing): presses in the page, "|" between the pages opened in turn (see run_menu).
        if (const char* t = std::getenv("ED_PAGE_TEST_KEYS"); t != nullptr) {
            static int opened = 0;
            std::string all = t;
            g_test_keys.clear();
            for (int i = 0; i <= opened; i++) {
                const size_t bar = all.find('|');
                if (i == opened)
                    g_test_keys = all.substr(0, bar);
                else if (bar == std::string::npos)
                    break;  // no list for this page
                else
                    all = all.substr(bar + 1);
            }
            g_test_key_at = 0;
            g_test_key_next = g_test_keys.empty() ? 0 : SDL_GetTicks() + 1000;
            opened++;
        }
        // ED_PAGE_TEST_MS=N (testing): the page closes by itself after N ms, as Back would (scripted runs can't press in it).
        if (const char* t = std::getenv("ED_PAGE_TEST_MS"); t != nullptr && std::atoi(t) > 0)
            g_test_close_at = SDL_GetTicks() + static_cast<Uint64>(std::atoi(t));
        g_menu_player = player;
        // The screen's rate is measured again for the frame rate's choices (the launcher's Options do it too), which holds
        // one frame for a dozen refreshes (about 190 ms): over a live title page that is a gap in the title's music, so
        // there the measure from the start (taken again when the window moves to another screen) is kept.
        const bool live_title = g_skin == Skin::Title && !title_page_pause();
        auto measure_screen = [live_title] {
            if (!live_title)
                dol_aurora_measure_screen();
        };
        // Each page opens alone, as the root: Back from it (to a screen it is opened from) closes the menu (past_root).
        auto options_section = [&measure_screen](int section) {
            measure_screen();
            g_options_section = section;
            g_options_row = 0;
            g_screen = g_root = Screen::OptionsPage;
        };
        // Nothing opens (Quit, or an id this build doesn't know): the menu's state as after a close.
        auto none = [] {
            g_root_closes = false;
            g_on_title = false;
            g_skin = Skin::Launcher;
            g_skin_half = -1;
            g_list_lines = 0;
            g_test_close_at = 0;
            g_menu_player = 0;
        };
        switch (page) {
        case FRONTEND_PAGE_CHARACTERS:
            g_characters_player = player;
            g_characters_return_row = 0;
            g_screen = g_root = Screen::Characters;
            g_selected = roster_index(chosen_character(), form_on(player, chosen_character()));
            break;
        case FRONTEND_PAGE_SETTINGS:
        case FRONTEND_PAGE_TITLE_SETTINGS:
            measure_screen();
            g_screen = g_root = Screen::Options;
            break;
        case FRONTEND_PAGE_MODS:
        case FRONTEND_PAGE_TITLE_MODS:
            g_screen = g_root = Screen::Mods;
            break;
        case FRONTEND_PAGE_STATES:
            g_screen = g_root = Screen::States;
            g_selected = std::max(newest_slot() - 1, 0);  // at the save made last
            break;
        case FRONTEND_PAGE_TITLE_CHARACTERS:  // the launcher's Choose Character: the two players' cards
            g_mods_section = 3;
            g_mods_page_return = Screen::InGame;
            g_mods_page_return_row = 0;
            g_screen = g_root = Screen::ModsPage;
            break;
        case FRONTEND_PAGE_TITLE_DISPLAY:
            options_section(1);
            break;
        case FRONTEND_PAGE_TITLE_SOUND:
            options_section(2);
            break;
        case FRONTEND_PAGE_TITLE_GRAPHICS:
            measure_screen();
            g_options_row = 0;
            g_graphics_row = 0;
            g_screen = g_root = Screen::Graphics;
            break;
        case FRONTEND_PAGE_TITLE_CONTROLS:  // Player I / Player II first, then that player's Controls
            controls::set_menu_player(0);
            open_pick(PickKind::Controls, Screen::InGame, 0);
            g_root = Screen::Pick;
            break;
        case FRONTEND_PAGE_TITLE_PICK:  // Solo / Co-op, for the title's Start Game (player 0) or Load Game (1)
            g_pick_title_row = player == 1 ? 1 : 0;
            g_menu_player = 0;
            open_pick(PickKind::Mode, Screen::InGame, 0);
            g_root = Screen::Pick;
            break;
        case FRONTEND_PAGE_TITLE_QUIT:  // asked first (Yes/No), "No" the first choice
            g_screen = g_root = Screen::QuitAsk;
            g_selected = 0;
            break;
        default:
            std::fprintf(stderr, "[frontend] page %d: no such page\n", page);
            none();
            g_menu_closed++;  // nothing opened, but the page is used up: count it as closed
            return;
        }
    }
    // A title-look page is live: the title runs under it (its music goes on), the page steps once a game frame and the
    // game's own presents draw it (opaque: the moving title is hidden). hle.c holds the guest's pad till it closes.
    // ED_TITLE_PAGE_PAUSE=1 (testing, A/B): title pages block the game as every other page does.
    if (page >= 0 && g_skin == Skin::Title && !title_page_pause()) {
        g_skin_split = g_settings.mod_split;
        hle_coop_ui_over(true, -1);  // (on the title it does nothing: it needs a co-op Tome)
        menu_begin();
        g_live = Live::Open;
        g_live_flag = true;
        g_live_frames = 0;
        g_live_slow_step_ns = g_live_slow_gap_ns = 0;
        g_live_last_ns = SDL_GetTicksNS();
        std::fprintf(stderr, "[frontend] page %d live: the title runs under it (%llu ms; set up in %.1f ms)\n", page,
                     static_cast<unsigned long long>(SDL_GetTicks()), (g_live_last_ns - entry_ns) / 1e6);
        return;  // no report_pause: the game's frames keep coming
    }
    report_pause(true);
    // A Tome page in co-op: in its owner's half, with the halves kept under it (hle.c), else on the whole window. The
    // split is read here, just before hle.c gives the same value to the renderer, and held till the page closes.
    g_skin_split = g_settings.mod_split;
    if (!hle_coop_ui_over(true, g_skin == Skin::Tome ? g_skin_half : -1))
        g_skin_half = -1;
    if (!run_menu())
        dol_aurora_request_quit();  // window closed while paused
    report_pause(false);
    page_closed();
}

extern "C" bool frontend_problem(const char* heading, const char* text, bool can_wait) {
    if (!ensure_ui())
        return false;
    const Screen screen = g_screen;
    const int selected = g_selected;
    g_problem_heading = heading;
    g_problem_text = text;
    g_problem_can_wait = can_wait;
    g_problem_wait = false;
    g_screen = Screen::Problem;
    g_selected = 0;
    const bool open = run_menu();
    g_screen = screen;
    g_selected = selected;
    if (open && g_live == Live::Open) {
        // Over a live title page (a stall of the game under it): the page as it was, its close undone.
        g_close = false;
        build_rows();
        g_doc->Show(Rml::ModalFlag::None, Rml::FocusFlag::Document);
    }
    return open && g_problem_wait;
}
