// The game's pad (port 0) built from the controller and the keyboard through
// a layout, so any action can sit on any button, trigger or key. Aurora still
// reads the controller for rumble; dol_aurora_set_pad0_override replaces only
// the state the game sees.
#include "host/controls.h"

#include "gxruntime/aurora_backend.h"
#include "host/camera.h"
#include "host/frontend.h"
#include "host/settings.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace controls {
namespace {

// Controller inputs (SDL's positional names) plus the analog triggers.
enum Native {
    kNone = -1,
    kSouth, kEast, kWest, kNorth,
    kLB, kRB, kLT, kRT, kL3, kR3,
    kStart,
    kDUp, kDDown, kDLeft, kDRight,
    kNativeCount,
};

const char* const kNativeIds[kNativeCount] = {
    "south", "east", "west", "north", "lb", "rb", "lt", "rt",
    "l3", "r3", "start", "dup", "ddown", "dleft", "dright",
};

struct Binding {
    int pad[2];  // up to two controller inputs (kNone when unused)
    int key;     // SDL scancode (0 when none)
};

// GameCube output of each action.
const unsigned short kGcButton[kActionCount] = {
    0x0100 /* A */, 0x0200 /* B */, 0x0400 /* X */, 0x0800 /* Y */,
    0x0010 /* Z */, 0x0040 /* L */, 0x0020 /* R */, 0x1000 /* Start */,
    0x0008 /* up */, 0x0004 /* down */, 0x0001 /* left */, 0x0002 /* right */,
};

const char* const kActionNames[kActionCount] = {
    "Attack / Confirm", "Action / Cancel", "Creep", "Quickcast", "Reload",
    "Sprint", "Target", "Pause", "Spell up", "Spell down", "Spell left",
    "Spell right",
};

const char* const kActionIds[kActionCount] = {
    "attack", "action", "creep", "quickcast", "reload", "sprint", "target",
    "pause", "spell_up", "spell_down", "spell_left", "spell_right",
};

// The keyboard is the same in every layout until it is rebound.
constexpr int kKeys[kActionCount] = {
    SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_U, SDL_SCANCODE_I,
    SDL_SCANCODE_Q, SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_RETURN,
    SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
};

// Movement (the main stick) and camera (the C-stick) on the keyboard: WASD and
// TFGH until rebound.
constexpr int kMoveKeys[kDirectionCount] = {
    SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D,
    SDL_SCANCODE_T, SDL_SCANCODE_G, SDL_SCANCODE_F, SDL_SCANCODE_H,
};

const char* const kDirectionNames[kDirectionCount] = {
    "Move forward", "Move back", "Move left", "Move right",
    "Camera up", "Camera down", "Camera left", "Camera right",
};

const char* const kDirectionIds[kDirectionCount] = {
    "move_up", "move_down", "move_left", "move_right",
    "camera_up", "camera_down", "camera_left", "camera_right",
};

// GameCube positions, as Aurora maps a controller by default.
const int kOriginalPad[kActionCount][2] = {
    {kSouth, kNone}, {kEast, kNone}, {kWest, kNone}, {kNorth, kNone},
    {kRB, kNone},    {kLT, kNone},   {kRT, kNone},   {kStart, kNone},
    {kDUp, kNone},   {kDDown, kNone}, {kDLeft, kNone}, {kDRight, kNone},
};

// Modern: attack and confirm on RT and A, action and cancel on B, aim on LT,
// sprint and creep on the stick clicks (toggles by default), reload X,
// quickcast Y. The GameCube L and R also sit on LB and RB, which page the
// game's own menus both ways (in play LB sprints, RB targets, while held).
const int kModernPad[kActionCount][2] = {
    {kRT, kSouth}, {kEast, kNone}, {kR3, kNone},    {kNorth, kNone},
    {kWest, kNone}, {kL3, kLB},    {kLT, kRB},      {kStart, kNone},
    {kDUp, kNone},  {kDDown, kNone}, {kDLeft, kNone}, {kDRight, kNone},
};

// The Tome (the game's pause menu) in Modern and Custom: the GameCube face
// buttons where they sit in Original, so its pages' A, B, X and Y are the
// controller's A, B, X and Y; the bumpers page it (L and R) and either
// trigger is Z (the map's current floor). Original needs none: it is these
// positions already. The keyboard keeps its keys.
const int kTomePad[kActionCount][2] = {
    {kSouth, kNone}, {kEast, kNone}, {kWest, kNone},  {kNorth, kNone},
    {kRT, kLT},      {kLB, kNone},   {kRB, kNone},    {kStart, kNone},
    {kDUp, kNone},   {kDDown, kNone}, {kDLeft, kNone}, {kDRight, kNone},
};

struct Player {
    Binding bindings[kActionCount];
    int moves[kDirectionCount];  // movement and camera keys (SDL scancodes, 0 when none)
    int layout = 0;
};
Player g_players[2];
// Player 1's: the game's pad.
Binding (&g_bindings)[kActionCount] = g_players[0].bindings;
int& g_layout = g_players[0].layout;
// The player the Controls pages show and change.
int g_menu_player = 0;
Player& menu() { return g_players[g_menu_player]; }
// The game's pause menu is open (set on the game thread, read by the icons).
std::atomic<bool> g_game_menu{false};
// A cutscene is playing (set on the game thread): the Skip prompt shows.
std::atomic<bool> g_cutscene{false};
// Cutscenes are no longer sped up (Start skips them, hle.c): always false.
std::atomic<bool> g_fast_forward{false};
SDL_Gamepad* g_pads[2] = {};  // each player's controller: the first and the second connected

int g_capture = -1;         // action (or kActionCount + direction) waiting for an input, or -1
Uint64 g_capture_deadline = 0;
bool g_capture_armed = false;  // every input released since capture began

// The controller inputs an action reads now: the Tome's while it is open
// (in Modern and Custom), else the action's binding.
bool tome_buttons() {
    return g_layout != kOriginal && g_game_menu.load(std::memory_order_relaxed);
}

const int* pad_inputs(int action) {
    return tome_buttons() ? kTomePad[action] : g_bindings[action].pad;
}

// The connected controller a player's pad_slot picks: the Nth connected, or
// automatic: player 1 the first, player 2 the next one player 1 is not using.
int pad_index(int player) {
    const int slot = g_settings.pad_slot[player];
    if (slot > 0)
        return slot - 1;
    if (player == 0 || !player_has_controller(0))
        return 0;
    return g_settings.pad_slot[0] == 1 || g_settings.pad_slot[0] == 0 ? 1 : 0;
}

int g_pad_index[2] = {-1, -1};  // the index each open controller was opened at

// A player's controller, or none (a keyboard player, or nothing connected
// at their place).
SDL_Gamepad* gamepad(int player = 0) {
    if (!player_has_controller(player))
        return nullptr;
    SDL_Gamepad*& pad = g_pads[player];
    const int index = pad_index(player);
    if (pad != nullptr && SDL_GamepadConnected(pad) && g_pad_index[player] == index)
        return pad;
    pad = nullptr;
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids != nullptr && index < count) {
        pad = SDL_OpenGamepad(ids[index]);
        g_pad_index[player] = index;
        // Which controller each player has, of all those connected.
        for (int i = 0; i < count; i++)
            fprintf(stderr, "[controls] controller %d of %d: %s%s\n", i + 1, count,
                    SDL_GetGamepadNameForID(ids[i]), i == index ? (player == 0 ? "  <- player 1" : "  <- player 2") : "");
    }
    SDL_free(ids);
    return pad;
}

// 0-255 for a native input (buttons are 0 or 255).
int native_value(SDL_Gamepad* pad, int n) {
    if (pad == nullptr || n == kNone)
        return 0;
    auto button = [&](SDL_GamepadButton b) { return SDL_GetGamepadButton(pad, b) ? 255 : 0; };
    auto trigger = [&](SDL_GamepadAxis a) {
        return std::clamp(SDL_GetGamepadAxis(pad, a) * 255 / 32767, 0, 255);
    };
    switch (n) {
    case kSouth: return button(SDL_GAMEPAD_BUTTON_SOUTH);
    case kEast: return button(SDL_GAMEPAD_BUTTON_EAST);
    case kWest: return button(SDL_GAMEPAD_BUTTON_WEST);
    case kNorth: return button(SDL_GAMEPAD_BUTTON_NORTH);
    case kLB: return button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    case kRB: return button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    case kLT: return trigger(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    case kRT: return trigger(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    case kL3: return button(SDL_GAMEPAD_BUTTON_LEFT_STICK);
    case kR3: return button(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    case kStart: return button(SDL_GAMEPAD_BUTTON_START);
    case kDUp: return button(SDL_GAMEPAD_BUTTON_DPAD_UP);
    case kDDown: return button(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    case kDLeft: return button(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    case kDRight: return button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    default: return 0;
    }
}

int native_from_id(const std::string& id);

// Test input without a controller: ED_PAD_SCRIPT=start,end,input;... holds
// each input (south, rt, lb, start...) from guest frame start to end
// (timebase / 675000, the frames ED_INPUT_SCRIPT counts) on a virtual Xbox
// controller, or a DualSense with ED_PAD_BRAND=ps.
// The sticks too: lup, ldown, lleft, lright, rup, rdown, rleft, rright push
// one all the way.
struct ScriptedInput {
    unsigned long long start, end;
    int input;  // a native input, or kNativeCount + one of kStickIds
};
const char* const kStickIds[8] = {"lup", "ldown", "lleft", "lright", "rup", "rdown", "rleft", "rright"};

int script_input(const char* id) {
    for (int i = 0; i < 8; i++)
        if (std::strcmp(id, kStickIds[i]) == 0)
            return kNativeCount + i;
    return native_from_id(id);
}
std::vector<ScriptedInput> g_script;
SDL_Joystick* g_virtual = nullptr;
std::atomic<unsigned long long> g_frame{0};

void attach_virtual_pad() {
    const char* text = std::getenv("ED_PAD_SCRIPT");
    if (text == nullptr || text[0] == '\0')
        return;
    const std::string all = text;
    for (size_t pos = 0; pos <= all.size();) {
        size_t end = all.find(';', pos);
        if (end == std::string::npos)
            end = all.size();
        const std::string item = all.substr(pos, end - pos);
        pos = end + 1;
        unsigned long long start = 0, stop = 0;
        char id[16] = {};
        if (std::sscanf(item.c_str(), "%llu,%llu,%15s", &start, &stop, id) == 3 &&
            script_input(id) != kNone)
            g_script.push_back({start, stop, script_input(id)});
    }
    const char* brand = std::getenv("ED_PAD_BRAND");
    const bool ps = brand != nullptr && std::strcmp(brand, "ps") == 0;
    // Presses count while the window is in the background (a test run).
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.vendor_id = ps ? 0x054c : 0x045e;
    desc.product_id = ps ? 0x0ce6 : 0x0b12;
    desc.name = ps ? "Scripted DualSense" : "Scripted Xbox controller";
    const SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
    g_virtual = id != 0 ? SDL_OpenJoystick(id) : nullptr;
    // The game reads the scripted pad, even with a real one connected.
    if (g_virtual != nullptr)
        g_pads[0] = SDL_OpenGamepad(id);
    // A trigger at rest is the axis minimum (0 is half pressed).
    if (g_virtual != nullptr) {
        SDL_SetJoystickVirtualAxis(g_virtual, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
        SDL_SetJoystickVirtualAxis(g_virtual, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
    }
    std::fprintf(stderr, "[controls] %s, %zu scripted presses%s\n", desc.name, g_script.size(),
                 g_virtual != nullptr ? "" : " (could not attach)");
}

void drive_virtual_pad() {
    if (g_virtual == nullptr)
        return;
    const unsigned long long frame = g_frame.load(std::memory_order_relaxed);
    bool down[kNativeCount + 8] = {};
    for (const ScriptedInput& press : g_script)
        if (frame >= press.start && frame < press.end)
            down[press.input] = true;
    static const SDL_GamepadAxis kAxes[4] = {SDL_GAMEPAD_AXIS_LEFTY, SDL_GAMEPAD_AXIS_LEFTX,
                                             SDL_GAMEPAD_AXIS_RIGHTY, SDL_GAMEPAD_AXIS_RIGHTX};
    for (int a = 0; a < 4; a++) {
        const bool* pair = &down[kNativeCount + a * 2];  // up/left, then down/right
        const int v = pair[0] ? SDL_JOYSTICK_AXIS_MIN : pair[1] ? SDL_JOYSTICK_AXIS_MAX : 0;
        SDL_SetJoystickVirtualAxis(g_virtual, kAxes[a], static_cast<Sint16>(v));
    }
    static const SDL_GamepadButton kButtons[kNativeCount] = {
        SDL_GAMEPAD_BUTTON_SOUTH,         SDL_GAMEPAD_BUTTON_EAST,
        SDL_GAMEPAD_BUTTON_WEST,          SDL_GAMEPAD_BUTTON_NORTH,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
        SDL_GAMEPAD_BUTTON_INVALID,       SDL_GAMEPAD_BUTTON_INVALID,
        SDL_GAMEPAD_BUTTON_LEFT_STICK,    SDL_GAMEPAD_BUTTON_RIGHT_STICK,
        SDL_GAMEPAD_BUTTON_START,         SDL_GAMEPAD_BUTTON_DPAD_UP,
        SDL_GAMEPAD_BUTTON_DPAD_DOWN,     SDL_GAMEPAD_BUTTON_DPAD_LEFT,
        SDL_GAMEPAD_BUTTON_DPAD_RIGHT};
    for (int n = 0; n < kNativeCount; n++) {
        if (n == kLT || n == kRT)
            SDL_SetJoystickVirtualAxis(g_virtual,
                                       n == kLT ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER
                                                : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
                                       down[n] ? SDL_JOYSTICK_AXIS_MAX : SDL_JOYSTICK_AXIS_MIN);
        else
            SDL_SetJoystickVirtualButton(g_virtual, kButtons[n], down[n]);
    }
}

bool key_down(int scancode) {
    int count = 0;
    const bool* keys = SDL_GetKeyboardState(&count);
    return keys != nullptr && scancode > 0 && scancode < count && keys[scancode];
}

// SDL stick axis (-32768..32767) to the GameCube's -127..127, with a deadzone.
int stick(SDL_Gamepad* pad, SDL_GamepadAxis axis, bool invert) {
    if (pad == nullptr)
        return 0;
    int v = SDL_GetGamepadAxis(pad, axis);
    if (invert)
        v = -v - 1;
    if (std::abs(v) < g_settings.stick_deadzone[g_layout] * 32767 / 100)
        return 0;
    return std::clamp(v / 256, -127, 127);
}

enum Brand { kXbox, kPlayStation, kNintendo, kXInput };

// The family a connected controller reports (Xbox for one SDL cannot tell).
Brand detected_brand(SDL_Gamepad* pad) {
    if (pad == nullptr)
        return kXbox;
    switch (SDL_GetGamepadType(pad)) {
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
        return kPlayStation;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        return kNintendo;
    case SDL_GAMEPAD_TYPE_STANDARD:
        return kXInput;  // a generic pad: Xbox positions, plain names
    default:
        return kXbox;
    }
}

// A player's family: the type set by hand, else the connected controller's.
Brand brand(int player = 0) {
    switch (g_settings.pad_type[player]) {
    case kPadXbox: return kXbox;
    case kPadXInput: return kXInput;
    case kPadPlayStation: return kPlayStation;
    case kPadSwitch: return kNintendo;
    default: return detected_brand(gamepad(player));
    }
}

const char* native_name(int n, int player = 0) {
    static const char* const xbox[kNativeCount] = {
        "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "LS", "RS", "Menu",
        "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right"};
    static const char* const ps[kNativeCount] = {
        "Cross", "Circle", "Square", "Triangle", "L1", "R1", "L2", "R2", "L3", "R3",
        "Options", "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right"};
    static const char* const nintendo[kNativeCount] = {
        "B", "A", "Y", "X", "L", "R", "ZL", "ZR", "L-stick", "R-stick", "+",
        "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right"};
    static const char* const xinput[kNativeCount] = {
        "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "LS", "RS", "Start",
        "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right"};
    if (n < 0 || n >= kNativeCount)
        return "-";
    switch (brand(player)) {
    case kPlayStation: return ps[n];
    case kNintendo: return nintendo[n];
    case kXInput: return xinput[n];
    default: return xbox[n];
    }
}

// A layout's controller buttons, and the keyboard's own keys (the same in
// every layout, so a keyboard player starts from them whatever the layout).
void load_preset(Player& p, int layout) {
    const int (*table)[2] = layout == kModern ? kModernPad : kOriginalPad;
    for (int a = 0; a < kActionCount; a++)
        p.bindings[a] = {{table[a][0], table[a][1]}, kKeys[a]};
    for (int d = 0; d < kDirectionCount; d++)
        p.moves[d] = kMoveKeys[d];
}

int native_from_id(const std::string& id) {
    for (int n = 0; n < kNativeCount; n++)
        if (id == kNativeIds[n])
            return n;
    return kNone;
}

// settings.ini: controls_custom = attack=rt+south:13;action=east:14;...;
// move_up=:26;... (the movement and camera keys: no controller input).
std::string serialize(const Player& p = g_players[0]) {
    std::string out;
    for (int a = 0; a < kActionCount; a++) {
        if (!out.empty())
            out += ';';
        out += kActionIds[a];
        out += '=';
        bool first = true;
        for (int n : p.bindings[a].pad)
            if (n != kNone) {
                if (!first)
                    out += '+';
                out += kNativeIds[n];
                first = false;
            }
        out += ':' + std::to_string(p.bindings[a].key);
    }
    for (int d = 0; d < kDirectionCount; d++)
        out += std::string(";") + kDirectionIds[d] + "=:" + std::to_string(p.moves[d]);
    return out;
}

void parse(Player& p, const char* text) {
    load_preset(p, kOriginal);
    std::string s(text);
    size_t pos = 0;
    while (pos < s.size()) {
        size_t end = s.find(';', pos);
        if (end == std::string::npos)
            end = s.size();
        const std::string item = s.substr(pos, end - pos);
        pos = end + 1;
        const size_t eq = item.find('=');
        const size_t colon = item.find(':');
        if (eq == std::string::npos || colon == std::string::npos || colon < eq)
            continue;
        const std::string id = item.substr(0, eq);
        const int key = std::atoi(item.c_str() + colon + 1);
        for (int d = 0; d < kDirectionCount; d++)
            if (id == kDirectionIds[d])
                p.moves[d] = key;
        int action = -1;
        for (int a = 0; a < kActionCount; a++)
            if (id == kActionIds[a])
                action = a;
        if (action < 0)
            continue;
        Binding b{{kNone, kNone}, key};
        const std::string pads = item.substr(eq + 1, colon - eq - 1);
        const size_t plus = pads.find('+');
        b.pad[0] = native_from_id(pads.substr(0, plus));
        if (plus != std::string::npos)
            b.pad[1] = native_from_id(pads.substr(plus + 1));
        p.bindings[action] = b;
    }
}

// Each player's layout, and their bindings while Custom.
void save() {
    g_settings.controls_layout = g_players[0].layout;
    if (g_players[0].layout == kCustom)
        snprintf(g_settings.controls_custom, sizeof g_settings.controls_custom, "%s",
                 serialize(g_players[0]).c_str());
    g_settings.mod_p2_layout = g_players[1].layout;
    if (g_players[1].layout == kCustom)
        snprintf(g_settings.mod_p2_custom, sizeof g_settings.mod_p2_custom, "%s",
                 serialize(g_players[1]).c_str());
    settings_save();
}

// A player's saved custom bindings ("" when they have none).
const char* saved_custom(int player) {
    return player == 0 ? g_settings.controls_custom : g_settings.mod_p2_custom;
}

// Toggle modes (sprint, creep, target): a press flips the action on or off.
bool g_toggled[kActionCount];
bool g_was_down[kActionCount];
Uint64 g_still_since = 0;  // when the main stick last came to rest

bool toggle_mode(int action) {
    return (action == kSprint && g_settings.sprint_toggle[g_layout]) ||
           (action == kCreep && g_settings.creep_toggle[g_layout]) ||
           (action == kTarget && g_settings.target_toggle[g_layout]);
}

// Custom starts from the layout in use: its buttons and hold/toggle choices.
void custom_from(int layout) {
    g_settings.sprint_toggle[kCustom] = g_settings.sprint_toggle[layout];
    g_settings.creep_toggle[kCustom] = g_settings.creep_toggle[layout];
    g_settings.target_toggle[kCustom] = g_settings.target_toggle[layout];
    g_settings.stick_deadzone[kCustom] = g_settings.stick_deadzone[layout];
}

// The device used last, keyboard or controller, so the game's prompts show
// that one's buttons (a controller left plugged in does not hide the keys).
bool g_keyboard_last = false;

void note_last_device(SDL_Gamepad* pad) {
    int count = 0;
    // Only with the window's focus, and not the keys that take it away: Alt-Tab to
    // another window left the keyboard's prompts behind.
    const bool* keys = player_has_keyboard(0) && SDL_GetKeyboardFocus() != nullptr ? SDL_GetKeyboardState(&count) : nullptr;
    if (keys != nullptr && (keys[SDL_SCANCODE_LALT] || keys[SDL_SCANCODE_RALT] || keys[SDL_SCANCODE_LGUI] || keys[SDL_SCANCODE_RGUI]))
        keys = nullptr;
    bool keyboard = false;
    for (int k = 1; keys != nullptr && k < count && !keyboard; k++)
        keyboard = keys[k];
    // Either player's controller: player 2's brought the prompts back no more than
    // their stick moved player 1.
    bool controller = false;
    SDL_Gamepad* const pads[2] = {pad, g_settings.mod_coop && player_has_controller(1) ? gamepad(1) : nullptr};
    for (SDL_Gamepad* const one : pads) {
        if (one == nullptr)
            continue;
        for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT && !controller; b++)
            controller = SDL_GetGamepadButton(one, static_cast<SDL_GamepadButton>(b));
        for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT && !controller; a++) {
            const int v = SDL_GetGamepadAxis(one, static_cast<SDL_GamepadAxis>(a));
            controller = std::abs(v) > g_settings.stick_deadzone[g_layout] * 32767 / 100 + 4000;
        }
    }
    if (controller)
        g_keyboard_last = false;
    else if (keyboard)
        g_keyboard_last = true;
}

// M, or a controller's Share / touchpad button: mark the moment for the log
// (dol_aurora_mark), to time a glitch as it is seen or heard. Taken from the
// events, so a tap shorter than a frame still counts.
bool SDLCALL mark_watch(void*, SDL_Event* event) {
    if ((event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_M &&
         !event->key.repeat) ||
        (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
         (event->gbutton.button == SDL_GAMEPAD_BUTTON_MISC1 ||
          event->gbutton.button == SDL_GAMEPAD_BUTTON_TOUCHPAD)))
        dol_aurora_mark();
    return true;
}

// Inputs held when the Tome's buttons come or go read as let go until they
// are released, so the button that was attacking does not press its new
// GameCube button.
bool g_tome_was = false;
bool g_held_over[kNativeCount];

int live_value(SDL_Gamepad* pad, int n) {
    const int v = native_value(pad, n);
    if (n == kNone || !g_held_over[n])
        return v;
    if (v < 64)
        g_held_over[n] = false;
    return 0;
}

// Input recording and replay (controls_record_start, ED_PAD_REPLAY): one
// line a pad read, "frame button stick_x stick_y substick_x substick_y
// trigger_l trigger_r analog_a analog_b cam_x cam_y aiming play", after a
// "camera" line with the camera's state when it began.
FILE* g_record = nullptr;

struct ReplayRead {
    unsigned long long frame;
    DolPadState pad;
    float cam_x, cam_y;
    bool aiming, play;
};
std::vector<ReplayRead> g_replay;
size_t g_replay_at = 0;
bool g_replay_loaded = false;

void load_replay() {
    g_replay_loaded = true;
    const char* path = std::getenv("ED_PAD_REPLAY");
    if (path == nullptr || path[0] == '\0')
        return;
    FILE* f = std::fopen(path, "r");
    if (f == nullptr) {
        std::fprintf(stderr, "[replay] cannot read %s\n", path);
        return;
    }
    char line[512];
    while (std::fgets(line, sizeof line, f) != nullptr) {
        float cam[CAMERA_STATE_SIZE];
        if (std::sscanf(line, "camera %f %f %f %f %f %f %f %f", &cam[0], &cam[1], &cam[2], &cam[3], &cam[4],
                        &cam[5], &cam[6], &cam[7]) == 8) {
            camera_set_state(cam);
            continue;
        }
        ReplayRead r{};
        unsigned v[10];
        int aiming = 0, play = 0;
        if (std::sscanf(line, "%llu %u %u %u %u %u %u %u %u %u %u %f %f %d %d", &r.frame, &v[0], &v[1], &v[2],
                        &v[3], &v[4], &v[5], &v[6], &v[7], &v[8], &v[9], &r.cam_x, &r.cam_y, &aiming,
                        &play) != 15)
            continue;
        r.pad.button = static_cast<u16>(v[0]);
        r.pad.stick_x = static_cast<s8>(v[1]);
        r.pad.stick_y = static_cast<s8>(v[2]);
        r.pad.substick_x = static_cast<s8>(v[3]);
        r.pad.substick_y = static_cast<s8>(v[4]);
        r.pad.trigger_left = static_cast<u8>(v[5]);
        r.pad.trigger_right = static_cast<u8>(v[6]);
        r.pad.analog_a = static_cast<u8>(v[7]);
        r.pad.analog_b = static_cast<u8>(v[8]);
        r.pad.error = static_cast<s8>(v[9]);
        r.aiming = aiming != 0;
        r.play = play != 0;
        g_replay.push_back(r);
    }
    std::fclose(f);
    std::fprintf(stderr, "[replay] %zu pad reads from %s (frames %llu to %llu)\n", g_replay.size(), path,
                 g_replay.empty() ? 0ull : g_replay.front().frame, g_replay.empty() ? 0ull : g_replay.back().frame);
}

void record_read(const DolPadState& s, float cam_x, float cam_y, bool aiming, bool play) {
    if (g_record == nullptr)
        return;
    std::fprintf(g_record, "%llu %u %u %u %u %u %u %u %u %u %u %.4f %.4f %d %d\n", g_frame.load(),
                 static_cast<unsigned>(s.button), static_cast<unsigned>(static_cast<u8>(s.stick_x)),
                 static_cast<unsigned>(static_cast<u8>(s.stick_y)), static_cast<unsigned>(static_cast<u8>(s.substick_x)),
                 static_cast<unsigned>(static_cast<u8>(s.substick_y)), static_cast<unsigned>(s.trigger_left),
                 static_cast<unsigned>(s.trigger_right), static_cast<unsigned>(s.analog_a),
                 static_cast<unsigned>(s.analog_b), static_cast<unsigned>(static_cast<u8>(s.error)), cam_x, cam_y,
                 aiming ? 1 : 0, play ? 1 : 0);
}

bool replay_pad(DolPadState* state) {
    if (!g_replay_loaded)
        load_replay();
    if (g_replay.empty())
        return false;
    const unsigned long long frame = g_frame.load(std::memory_order_relaxed);
    if (frame < g_replay.front().frame)
        return false;  // before the recording began: as it was
    while (g_replay_at + 1 < g_replay.size() && g_replay[g_replay_at + 1].frame <= frame)
        g_replay_at++;
    if (g_replay_at + 1 >= g_replay.size() && frame > g_replay.back().frame + 1) {
        static bool said;
        if (!said)
            std::fprintf(stderr, "[replay] finished at frame %llu\n", frame);
        said = true;
        *state = DolPadState{};
        camera_set_stick(0.0f, 0.0f, false, true);
        return true;
    }
    const ReplayRead& r = g_replay[g_replay_at];
    *state = r.pad;
    camera_set_stick(r.cam_x, r.cam_y, r.aiming, r.play);
    return true;
}

bool read_pad(DolPadState* state) {
    if (replay_pad(state))
        return true;
    // Photo mode: the game runs a frame now and then (for a new resolution,
    // say) and must not move; what is held then is held over afterwards.
    if (frontend_photo_active()) {
        SDL_Gamepad* pad = gamepad();
        for (int n = 0; n < kNativeCount; n++)
            g_held_over[n] = g_held_over[n] || native_value(pad, n) >= 64;
        *state = DolPadState{};
        camera_set_stick(0.f, 0.f, false, true);
        camera_set_move(0.f, 0.f);
        return true;
    }
    static bool scripted = (attach_virtual_pad(), true);
    (void)scripted;
    drive_virtual_pad();
    SDL_Gamepad* pad = gamepad();
    note_last_device(pad);
    const bool tome = tome_buttons();
    if (tome != g_tome_was) {
        for (int n = 0; n < kNativeCount; n++)
            g_held_over[n] = native_value(pad, n) >= 64;
        g_tome_was = tome;
    }
    // Player 1's keys, unless player 2 has the keyboard.
    const bool keys = player_has_keyboard(0);
    const bool cutscene = g_cutscene.load(std::memory_order_relaxed);
    DolPadState s{};
    // The main stick first: toggled sprint ends when you stop moving.
    int x = stick(pad, SDL_GAMEPAD_AXIS_LEFTX, false);
    int y = stick(pad, SDL_GAMEPAD_AXIS_LEFTY, true);
    const int* moves = g_players[0].moves;
    if (keys && key_down(moves[kMoveLeft])) x = -127;
    if (keys && key_down(moves[kMoveRight])) x = 127;
    if (keys && key_down(moves[kMoveUp])) y = 127;
    if (keys && key_down(moves[kMoveDown])) y = -127;
    const Uint64 now = SDL_GetTicks();
    if (x != 0 || y != 0)
        g_still_since = 0;
    else if (g_still_since == 0)
        g_still_since = now;
    if (g_toggled[kSprint] && g_still_since != 0 && now - g_still_since > 300)
        g_toggled[kSprint] = false;
    for (int a = 0; a < kActionCount; a++) {
        // In toggle mode only the first button and the key toggle; a second
        // button (Modern's bumpers) is a plain hold, so it pages menus once.
        // The Tome's buttons are all plain holds (a toggle stays as it was
        // for when play resumes).
        const int* inputs = pad_inputs(a);
        int v = live_value(pad, inputs[0]);
        const int held = live_value(pad, inputs[1]);
        if (keys && key_down(g_bindings[a].key))
            v = 255;
        const bool toggles = !tome && toggle_mode(a);
        if (!toggles)
            v = std::max(v, held);
        if (toggles) {
            const bool down = v >= 128;
            if (down && !g_was_down[a]) {
                g_toggled[a] = !g_toggled[a];
                // Sprinting and creeping cancel each other.
                if (g_toggled[a] && a == kSprint)
                    g_toggled[kCreep] = false;
                if (g_toggled[a] && a == kCreep)
                    g_toggled[kSprint] = false;
            }
            g_was_down[a] = down;
            v = std::max(g_toggled[a] ? 255 : 0, held);
        } else if (!tome) {
            g_toggled[a] = false;
        }
        if (a == kSprint || a == kTarget) {
            // Analog L/R: the trigger value; the digital click at full press.
            (a == kSprint ? s.trigger_left : s.trigger_right) = static_cast<u8>(v);
            if (v >= 230)
                s.button |= kGcButton[a];
        } else if (v >= 128) {
            s.button |= kGcButton[a];
        }
    }
    s.analog_a = (s.button & 0x0100) ? 255 : 0;
    s.analog_b = (s.button & 0x0200) ? 255 : 0;
    // C-stick: right stick or the camera keys (TFGH).
    int cx = stick(pad, SDL_GAMEPAD_AXIS_RIGHTX, false);
    int cy = stick(pad, SDL_GAMEPAD_AXIS_RIGHTY, true);
    if (keys && key_down(moves[kCameraLeft])) cx = -127;
    if (keys && key_down(moves[kCameraRight])) cx = 127;
    if (keys && key_down(moves[kCameraUp])) cy = 127;
    if (keys && key_down(moves[kCameraDown])) cy = -127;
    // The camera options (camera.c): in play the right stick turns the camera
    // instead of being the C-stick, and the left stick turns with the view, so
    // up walks away from it. Aiming (R) holds the camera and leaves the left
    // stick as it is, for picking a target.
    const bool aiming = (s.button & 0x0020) != 0 || s.trigger_right >= 128;
    const bool camera = camera_takes_stick() && !tome && !cutscene;
    camera_set_menu(tome);
    const float cam_x = camera ? cx / 127.0f : 0.0f, cam_y = camera ? cy / 127.0f : 0.0f;
    camera_set_stick(cam_x, cam_y, aiming, !cutscene);
    camera_set_move(camera ? x / 127.0f : 0.0f, camera ? y / 127.0f : 0.0f);
    if (camera) {
        cx = cy = 0;
        static const bool turn_stick = std::getenv("ED_CAM_STICK") == nullptr ||
                                       std::atoi(std::getenv("ED_CAM_STICK")) != 0;
        const float turn = turn_stick ? camera_stick_turn() : 0.0f;
        if (turn != 0.0f && (x != 0 || y != 0)) {
            const float c = std::cos(turn), sn = std::sin(turn);
            const float tx = x * c - y * sn, ty = x * sn + y * c;
            x = std::clamp(static_cast<int>(std::lround(tx)), -127, 127);
            y = std::clamp(static_cast<int>(std::lround(ty)), -127, 127);
        }
    }
    s.stick_x = static_cast<s8>(x);
    s.stick_y = static_cast<s8>(y);
    s.substick_x = static_cast<s8>(cx);
    s.substick_y = static_cast<s8>(cy);
    // ED_PAD_LOG=1: the GameCube buttons the game gets, at each change.
    static const bool log = std::getenv("ED_PAD_LOG") != nullptr;
    static unsigned last = 0;
    if (log && s.button != last)
        std::fprintf(stderr, "[pad] frame %llu: 0x%04x%s\n", g_frame.load(), s.button,
                     tome ? " (Tome buttons)" : "");
    last = s.button;
    *state = s;
    record_read(s, cam_x, cam_y, aiming, !cutscene);
    return true;
}

}  // namespace

void set_menu_player(int player) { g_menu_player = std::clamp(player, 0, 1); }
int menu_player() { return g_menu_player; }

bool player_uses_keyboard(int player) {
    return player == 0 ? g_settings.controls_device != 1 : g_settings.mod_p2_input == 1;
}

bool player_has_keyboard(int player) {
    if (player != 0)
        return player_uses_keyboard(1);
    // "Controller and keyboard" lends the keyboard to a keyboard player 2.
    const bool lent = g_settings.controls_device == 0 && g_settings.mod_coop && player_uses_keyboard(1);
    return player_uses_keyboard(0) && !lent;
}

bool player_has_controller(int player) {
    return player == 0 ? g_settings.controls_device != 2 : g_settings.mod_p2_input == 0;
}

const char* pad_type_name(int type) {
    static const char* const names[kPadTypeCount] = {"Automatic", "Xbox", "XInput", "PlayStation",
                                                     "Switch"};
    return type >= 0 && type < kPadTypeCount ? names[type] : "";
}

std::string detected_pad_type(int player) {
    SDL_Gamepad* pad = gamepad(player);
    if (pad == nullptr)
        return "";
    static const int kTypes[] = {kPadXbox, kPadPlayStation, kPadSwitch, kPadXInput};
    return pad_type_name(kTypes[detected_brand(pad)]);
}

std::vector<std::string> connected_controllers() {
    std::vector<std::string> names;
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    for (int i = 0; ids != nullptr && i < count; i++) {
        const char* name = SDL_GetGamepadNameForID(ids[i]);
        names.push_back(name != nullptr && name[0] != '\0' ? name : "Controller");
    }
    SDL_free(ids);
    return names;
}

const char* action_name(int action) {
    return action >= 0 && action < kActionCount ? kActionNames[action] : "";
}

const char* direction_name(int direction) {
    return direction >= 0 && direction < kDirectionCount ? kDirectionNames[direction] : "";
}

const char* layout_name(int layout) {
    static const char* const names[kLayoutCount] = {"Original", "Modern", "Custom"};
    return layout >= 0 && layout < kLayoutCount ? names[layout] : "";
}

std::string pad_label(int action) {
    std::string out;
    for (int n : menu().bindings[action].pad)
        if (n != kNone) {
            if (!out.empty())
                out += " / ";
            out += native_name(n, g_menu_player);
        }
    return out.empty() ? "-" : out;
}

namespace {
std::string scancode_label(int key) {
    if (key <= 0)
        return "-";
    const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(key));
    return name != nullptr && name[0] != '\0' ? name : "?";
}
}  // namespace

std::string key_label(int action) { return scancode_label(menu().bindings[action].key); }

namespace {
std::string glyph_img(const std::string& file, const std::string& alt) {
    // Paths are relative to res/ui/menu.rml: the ink versions, drawn for the
    // parchment (tools/ui/make_ink_glyphs.py).
    return "<img class=\"glyph\" src=\"glyphs/ink/" + file + ".png\" alt=\"" + alt + "\"/>";
}

std::string scancode_glyph_file(int key);

// A key's icon in the menu, or its name in a keycap.
std::string scancode_glyph(int key) {
    if (key <= 0)
        return "<span class=\"unbound\">-</span>";
    const std::string file = scancode_glyph_file(key);
    if (file.empty())
        return "<span class=\"keycap\">" + scancode_label(key) + "</span>";
    return glyph_img(file, scancode_label(key));
}
}  // namespace

std::string pad_glyphs(int action) {
    const Brand b = brand(g_menu_player);
    const char* set = b == kPlayStation ? "ps" : b == kNintendo ? "switch" : "xbox";
    std::string out;
    for (int n : menu().bindings[action].pad)
        if (n != kNone)
            out += glyph_img(std::string(set) + "/" + kNativeIds[n], native_name(n, g_menu_player));
    return out.empty() ? "<span class=\"unbound\">-</span>" : out;
}

// The bound key's icon under res/ui/glyphs ("kb/j"), or "" when it has none:
// player 1's, for the game's prompts.
std::string key_glyph_file(int action) { return scancode_glyph_file(g_bindings[action].key); }

namespace {
std::string scancode_glyph_file(int key) {
    if (key <= 0)
        return "";
    // SDL's key name to the icon file: "Return" -> return, "Up" -> arrow_up,
    // "Left Shift" -> shift; keys without an icon show their name in a keycap.
    std::string name = scancode_label(key);
    std::string file;
    for (char c : name)
        file += c == ' ' ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (file == "up" || file == "down" || file == "left" || file == "right")
        file = "arrow_" + file;
    else if (file == "left_shift" || file == "right_shift")
        file = "shift";
    else if (file == "left_ctrl" || file == "right_ctrl")
        file = "ctrl";
    else if (file == "left_alt" || file == "right_alt")
        file = "alt";
    else if (file == "left_gui" || file == "right_gui")
        file = "command";
    else if (file == "pageup")
        file = "page_up";
    else if (file == "pagedown")
        file = "page_down";
    else if (file == "keypad_enter")
        file = "numpad_enter";
    static const char* const kIcons[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c", "d", "e", "f", "g",
        "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x",
        "y", "z", "alt", "arrow_down", "arrow_left", "arrow_right", "arrow_up", "backspace",
        "capslock", "command", "ctrl", "delete", "end", "enter", "escape", "f1", "f2", "f3",
        "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12", "home", "insert",
        "numpad_enter", "page_down", "page_up", "return", "shift", "space", "tab"};
    for (const char* icon : kIcons)
        if (file == icon)
            return "kb/" + file;
    return "";
}
}  // namespace

std::string key_glyph(int action) { return scancode_glyph(menu().bindings[action].key); }

std::string direction_glyph(int direction) { return scancode_glyph(menu().moves[direction]); }

std::string glyph_set() {
    // ED_GLYPHS=xbox, ps, switch or kb: that set whatever is connected (to see
    // the game's prompts for a controller that is not there).
    static const char* const forced = std::getenv("ED_GLYPHS");
    if (forced != nullptr && forced[0] != '\0')
        return forced;
    // Player 1's prompts: the keyboard's when they play on it alone, their
    // controller type's when on a controller alone (connected or not), else
    // the one used last.
    if (!player_has_controller(0))
        return "kb";
    if (player_has_keyboard(0) && (gamepad() == nullptr || g_keyboard_last))
        return "kb";
    return brand() == kPlayStation ? "ps" : brand() == kNintendo ? "switch" : "xbox";
}

std::string skip_glyph() {
    if (!g_cutscene.load(std::memory_order_relaxed))
        return "";
    return action_glyph(kPause, kAnyButton);
}

std::string glyph_for(int action, int preference, bool tome) {
    const std::string set = glyph_set();
    if (set == "kb") {
        // The key's icon file, as the menu shows it.
        const std::string file = key_glyph_file(action);
        return file.empty() ? "" : file + ".png";
    }
    // Face buttons for the face cells. Shoulder cells: triggers, then
    // bumpers, in play (Modern's Target is LT, with RB as well); bumpers, then
    // triggers, in the game's pause menu, so its paging pairs LB with RB.
    // Else the first.
    auto rank = [&](int n) {
        if (n == kNone)
            return 0;
        if (preference == kFaceButton)
            return n >= kSouth && n <= kNorth ? 3 : 1;
        if (preference == kShoulderButton) {
            const bool bumper = n == kLB || n == kRB, trigger = n == kLT || n == kRT;
            return bumper ? (tome ? 3 : 2) : trigger ? (tome ? 2 : 3) : 1;
        }
        return 1;
    };
    const int* inputs = tome && g_layout != kOriginal ? kTomePad[action] : g_bindings[action].pad;
    int pick = kNone;
    for (int i = 0; i < 2; i++)
        if (rank(inputs[i]) > rank(pick))
            pick = inputs[i];
    return pick == kNone ? "" : set + "/" + kNativeIds[pick] + ".png";
}

// The buttons the action reads now: the Tome's while it is open.
std::string action_glyph(int action, int preference) {
    return glyph_for(action, preference, g_game_menu.load(std::memory_order_relaxed));
}

uint64_t glyph_signature() {
    // Asked on every draw of a redrawn sheet: worked out again only when the
    // controller family, the layout, the bindings or the pause menu change.
    const bool menu = g_game_menu.load(std::memory_order_relaxed);
    const std::string key = glyph_set() + "|" + std::to_string(g_layout) + "|" + serialize() +
                            (menu ? "|menu" : "");
    static std::mutex mutex;
    static std::string last_key;
    static uint64_t last = 0;
    std::lock_guard<std::mutex> lock(mutex);
    if (key == last_key)
        return last;
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](const std::string& text) {
        for (unsigned char c : text)
            h = (h ^ c) * 1099511628211ull;
    };
    mix(glyph_set());
    mix(serialize());
    // The icons that change with the pause menu (its shoulder icons, and the
    // Tome's buttons), so opening it redraws them only when one differs.
    for (int a = 0; a < kActionCount; a++)
        for (int preference : {kAnyButton, kFaceButton, kShoulderButton})
            mix(glyph_for(a, preference, menu));
    last_key = key;
    last = h;
    return h;
}

std::vector<std::string> pad_input_ids(int action) {
    std::vector<std::string> ids;
    for (int n : g_bindings[action].pad)
        if (n != kNone)
            ids.push_back(kNativeIds[n]);
    return ids;
}

uint64_t bindings_signature() {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : glyph_set() + "|" + serialize())
        h = (h ^ c) * 1099511628211ull;
    return h;
}

std::string controller_name() {
    if (!player_has_controller(g_menu_player))
        return "Keyboard";
    SDL_Gamepad* pad = gamepad(g_menu_player);
    if (pad == nullptr)
        return g_menu_player == 0 ? "No controller (keyboard)" : "No second controller";
    const char* name = SDL_GetGamepadName(pad);
    return name != nullptr ? name : "Controller";
}

void set_layout(int layout) {
    Player& p = menu();
    if (layout == kCustom) {
        if (p.layout != kCustom && saved_custom(g_menu_player)[0] != '\0')
            parse(p, saved_custom(g_menu_player));  // the saved custom layout
        else if (p.layout != kCustom && g_menu_player == 0)
            custom_from(p.layout);  // a first Custom starts from the layout in use
    } else {
        load_preset(p, layout);
    }
    p.layout = layout;
    save();
}

int layout() { return menu().layout; }

namespace {
// A rebinding starts Custom from what is in use.
void make_custom() {
    Player& p = menu();
    if (p.layout != kCustom) {
        if (g_menu_player == 0)
            custom_from(p.layout);
        p.layout = kCustom;
    }
}

// The keys offered by hand, in order: letters, digits, the arrows, then the
// space bar, Enter, Tab, Backspace and the modifiers.
const int kStepKeys[] = {
    SDL_SCANCODE_A, SDL_SCANCODE_B, SDL_SCANCODE_C, SDL_SCANCODE_D, SDL_SCANCODE_E,
    SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_I, SDL_SCANCODE_J,
    SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_O,
    SDL_SCANCODE_P, SDL_SCANCODE_Q, SDL_SCANCODE_R, SDL_SCANCODE_S, SDL_SCANCODE_T,
    SDL_SCANCODE_U, SDL_SCANCODE_V, SDL_SCANCODE_W, SDL_SCANCODE_X, SDL_SCANCODE_Y,
    SDL_SCANCODE_Z, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
    SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9,
    SDL_SCANCODE_0, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_SPACE, SDL_SCANCODE_RETURN, SDL_SCANCODE_TAB, SDL_SCANCODE_BACKSPACE,
    SDL_SCANCODE_LSHIFT, SDL_SCANCODE_LCTRL, SDL_SCANCODE_LALT,
    SDL_SCANCODE_RSHIFT, SDL_SCANCODE_RCTRL, SDL_SCANCODE_RALT,
};
}  // namespace

void step_pad(int action, int dir) {
    if (action < 0 || action >= kActionCount)
        return;
    make_custom();
    // kNone, then every controller input: kNativeCount + 1 places.
    Binding& b = menu().bindings[action];
    const int places = kNativeCount + 1;
    b.pad[0] = (b.pad[0] + 1 + dir + places) % places - 1;
    b.pad[1] = kNone;
    save();
}

void step_key(int target, int dir) {
    if (target < 0 || target >= kActionCount + kDirectionCount)
        return;
    make_custom();
    int& key = target < kActionCount ? menu().bindings[target].key : menu().moves[target - kActionCount];
    const int count = static_cast<int>(sizeof kStepKeys / sizeof kStepKeys[0]);
    int at = -1;
    for (int i = 0; i < count; i++)
        if (kStepKeys[i] == key)
            at = i;
    // A key not in the list steps in from its start (or its end, going back).
    at = at < 0 ? (dir > 0 ? 0 : count - 1) : (at + dir + count) % count;
    key = kStepKeys[at];
    save();
}

void begin_capture(int action) {
    make_custom();
    g_capture = action;
    g_capture_armed = false;
    g_capture_deadline = SDL_GetTicks() + 6000;
}

bool capturing() { return g_capture >= 0; }
int capture_action() { return g_capture; }

bool poll_capture() {
    if (g_capture < 0)
        return true;
    // The menu player's controller; a movement or camera key takes a key only.
    SDL_Gamepad* pad = g_capture < kActionCount ? gamepad(g_menu_player) : nullptr;
    bool any = false;
    int pressed = kNone;
    for (int n = 0; n < kNativeCount; n++) {
        const int v = native_value(pad, n);
        if (v >= 200) {
            any = true;
            if (pressed == kNone)
                pressed = n;
        }
    }
    // The keyboard as it is now, for a player who plays on it: the first key
    // held, read by its scancode.
    const bool keys = player_uses_keyboard(g_menu_player);
    int count = 0;
    const bool* state = keys ? SDL_GetKeyboardState(&count) : nullptr;
    int key = 0;
    for (int k = SDL_SCANCODE_A; state != nullptr && k < count; k++)
        if (state[k]) {
            any = true;
            if (key == 0)
                key = k;
        }
    if (!g_capture_armed) {
        // Wait for the button or key that started the capture to be let go.
        if (!any)
            g_capture_armed = true;
    } else if (key == SDL_SCANCODE_ESCAPE) {
        g_capture = -1;
        return true;
    } else if (key != 0) {
        capture_key(key);
        return true;
    } else if (pressed != kNone && g_capture < kActionCount) {
        menu().bindings[g_capture].pad[0] = pressed;
        menu().bindings[g_capture].pad[1] = kNone;
        g_capture = -1;
        save();
        return true;
    }
    if (SDL_GetTicks() > g_capture_deadline) {
        g_capture = -1;
        save();
        return true;
    }
    return false;
}

void capture_key(int scancode) {
    if (g_capture < 0 || scancode <= 0)
        return;
    if (scancode == SDL_SCANCODE_ESCAPE) {  // cancel
        g_capture = -1;
        return;
    }
    if (g_capture < kActionCount)
        menu().bindings[g_capture].key = scancode;
    else
        menu().moves[g_capture - kActionCount] = scancode;
    g_capture = -1;
    save();
}

}  // namespace controls

extern "C" void controls_init(void) {
    using namespace controls;
    const int layouts[2] = {g_settings.controls_layout, g_settings.mod_p2_layout};
    for (int i = 0; i < 2; i++) {
        Player& p = g_players[i];
        p.layout = std::clamp(layouts[i], 0, static_cast<int>(kLayoutCount) - 1);
        if (p.layout == kCustom && saved_custom(i)[0] != '\0')
            parse(p, saved_custom(i));
        else
            load_preset(p, p.layout == kCustom ? kOriginal : p.layout);
    }
    dol_aurora_set_pad0_override(read_pad);
    SDL_AddEventWatch(mark_watch, nullptr);
}

extern "C" void controls_player2_look(float* turn_x, float* turn_y, float* in_out) {
    using namespace controls;
    float tx = 0.f, ty = 0.f, io = 0.f;
    if (SDL_Gamepad* pad = gamepad(1)) {
        tx = stick(pad, SDL_GAMEPAD_AXIS_RIGHTX, false) / 127.f;
        ty = stick(pad, SDL_GAMEPAD_AXIS_RIGHTY, true) / 127.f;
        io = stick(pad, SDL_GAMEPAD_AXIS_LEFTY, true) / 127.f;
    }
    if (player_has_keyboard(1)) {
        const int* moves = g_players[1].moves;
        if (key_down(moves[kCameraLeft])) tx = -1.f;
        if (key_down(moves[kCameraRight])) tx = 1.f;
        if (key_down(moves[kCameraUp])) ty = 1.f;
        if (key_down(moves[kCameraDown])) ty = -1.f;
        if (key_down(moves[kMoveUp])) io = 1.f;
        if (key_down(moves[kMoveDown])) io = -1.f;
    }
    *turn_x = tx;
    *turn_y = ty;
    *in_out = io;
}

// Test input for player 2: ED_PAD2_SCRIPT=start,end,input;... as
// ED_PAD_SCRIPT, in the GameCube's own buttons (a, b, x, y, z, start, up,
// down, left, right) and lup, ldown, lleft, lright.
static void script_player2(DolPadState& s, int& x, int& y) {
    using namespace controls;
    struct Item {
        unsigned long long start, end;
        std::string id;
    };
    static std::vector<Item> items;
    static bool read;
    if (!read) {
        read = true;
        const char* text = std::getenv("ED_PAD2_SCRIPT");
        const std::string all = text != nullptr ? text : "";
        for (size_t pos = 0; pos < all.size();) {
            size_t end = all.find(';', pos);
            if (end == std::string::npos)
                end = all.size();
            unsigned long long a = 0, b = 0;
            char id[16] = {};
            if (std::sscanf(all.substr(pos, end - pos).c_str(), "%llu,%llu,%15s", &a, &b, id) == 3)
                items.push_back({a, b, id});
            pos = end + 1;
        }
    }
    static const struct {
        const char* id;
        u16 button;
    } kButtons[] = {{"a", 0x0100}, {"b", 0x0200}, {"x", 0x0400}, {"y", 0x0800}, {"z", 0x0010},
                    {"start", 0x1000}, {"up", 0x0008}, {"down", 0x0004}, {"left", 0x0001}, {"right", 0x0002}};
    const unsigned long long frame = g_frame.load(std::memory_order_relaxed);
    for (const Item& item : items) {
        if (frame < item.start || frame >= item.end)
            continue;
        for (const auto& b : kButtons)
            if (item.id == b.id)
                s.button |= b.button;
        if (item.id == "lup") y = 127;
        if (item.id == "ldown") y = -127;
        if (item.id == "lleft") x = -127;
        if (item.id == "lright") x = 127;
    }
}

extern "C" void controls_read_player2(DolPadState* state) {
    using namespace controls;
    const Player& p = g_players[1];
    SDL_Gamepad* pad = gamepad(1);
    const bool keys = player_has_keyboard(1);
    DolPadState s{};
    int x = stick(pad, SDL_GAMEPAD_AXIS_LEFTX, false);
    int y = stick(pad, SDL_GAMEPAD_AXIS_LEFTY, true);
    if (keys && key_down(p.moves[kMoveLeft])) x = -127;
    if (keys && key_down(p.moves[kMoveRight])) x = 127;
    if (keys && key_down(p.moves[kMoveUp])) y = 127;
    if (keys && key_down(p.moves[kMoveDown])) y = -127;
    // Their sprint, creep and target toggle as player 1's do in their
    // layout (the first button and the key; toggled sprint ends when they
    // stop moving).
    static bool toggled[kActionCount], was_down[kActionCount];
    static Uint64 still_since;
    const Uint64 now = SDL_GetTicks();
    if (x != 0 || y != 0)
        still_since = 0;
    else if (still_since == 0)
        still_since = now;
    if (toggled[kSprint] && still_since != 0 && now - still_since > 300)
        toggled[kSprint] = false;
    for (int a = 0; a < kActionCount; a++) {
        int v = native_value(pad, p.bindings[a].pad[0]);
        const int held = native_value(pad, p.bindings[a].pad[1]);
        if (keys && key_down(p.bindings[a].key))
            v = 255;
        const bool toggles = (a == kSprint && g_settings.sprint_toggle[p.layout]) ||
                             (a == kCreep && g_settings.creep_toggle[p.layout]) ||
                             (a == kTarget && g_settings.target_toggle[p.layout]);
        if (toggles) {
            const bool down = v >= 128;
            if (down && !was_down[a]) {
                toggled[a] = !toggled[a];
                if (toggled[a] && a == kSprint)
                    toggled[kCreep] = false;
                if (toggled[a] && a == kCreep)
                    toggled[kSprint] = false;
            }
            was_down[a] = down;
            v = std::max(toggled[a] ? 255 : 0, held);
        } else {
            toggled[a] = false;
            v = std::max(v, held);
        }
        if (a == kSprint || a == kTarget) {
            (a == kSprint ? s.trigger_left : s.trigger_right) = static_cast<u8>(v);
            if (v >= 230)
                s.button |= kGcButton[a];
        } else if (v >= 128) {
            s.button |= kGcButton[a];
        }
    }
    script_player2(s, x, y);
    s.analog_a = (s.button & 0x0100) ? 255 : 0;
    s.analog_b = (s.button & 0x0200) ? 255 : 0;
    s.stick_x = static_cast<s8>(x);
    s.stick_y = static_cast<s8>(y);
    // Recording (F12): player 2's pad too, a "p2" line at each change (a
    // replay reads only player 1's lines).
    static DolPadState recorded{};
    if (g_record != nullptr && (s.button != recorded.button || s.stick_x != recorded.stick_x ||
                                s.stick_y != recorded.stick_y || s.substick_x != recorded.substick_x ||
                                s.substick_y != recorded.substick_y)) {
        std::fprintf(g_record, "p2 %llu %u %d %d %d %d\n", g_frame.load(), static_cast<unsigned>(s.button), s.stick_x,
                     s.stick_y, s.substick_x, s.substick_y);
        recorded = s;
    }
    *state = s;
}

extern "C" void controls_note_frame(unsigned long long frame) {
    controls::g_frame.store(frame, std::memory_order_relaxed);
}

extern "C" void controls_record_start(const char* path) {
    controls_record_stop();
    controls::g_record = std::fopen(path, "w");
    if (controls::g_record == nullptr) {
        std::fprintf(stderr, "[record] cannot write %s\n", path);
        return;
    }
    float cam[CAMERA_STATE_SIZE];
    camera_get_state(cam);
    std::fprintf(controls::g_record, "camera %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n", cam[0], cam[1], cam[2], cam[3], cam[4],
                 cam[5], cam[6], cam[7]);
    std::fprintf(stderr, "[record] recording the controller to %s from frame %llu\n", path, controls::g_frame.load());
}

extern "C" void controls_record_stop(void) {
    if (controls::g_record == nullptr)
        return;
    std::fclose(controls::g_record);
    controls::g_record = nullptr;
    std::fprintf(stderr, "[record] stopped at frame %llu\n", controls::g_frame.load());
}

extern "C" bool controls_recording(void) { return controls::g_record != nullptr; }

extern "C" void controls_set_cutscene(bool playing) {
    if (controls::g_cutscene.exchange(playing, std::memory_order_relaxed) != playing)
        std::fprintf(stderr, "[controls] cutscene %s at frame %llu\n",
                     playing ? "started" : "over", controls::g_frame.load());
}

extern "C" bool controls_fast_forward(void) {
    return controls::g_fast_forward.load(std::memory_order_relaxed);
}

extern "C" void controls_set_game_menu(bool open) {
    if (controls::g_game_menu.exchange(open, std::memory_order_relaxed) != open)
        std::fprintf(stderr, "[controls] game's pause menu %s\n", open ? "open" : "closed");
}
