#pragma once
// Launcher and in-game menu (RmlUi over the game, windowed build): choose the
// disc and options before boot; Esc or the controller's Back/Select button
// pauses the game into a menu with save slots and the same options.
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Remember the command line, for "return to launcher" (restarts the process).
void frontend_init(int argc, char** argv);

// Before boot: run the launcher until the player picks Play (true, the chosen
// disc image in iso_out) or quits (false).
bool frontend_run_launcher(const char* default_iso, char* iso_out,
                           size_t iso_cap);

// The disc this run plays (main.c, once it is known): the portraits are made from it when they are first shown, also in
// a run with no launcher (whose disc list is empty).
void frontend_set_disc(const char* path);

// Once a game frame: open the in-game menu when asked for, and run it with
// the game paused until the player resumes.
void frontend_frame(void);

// Co-op separate rooms only with co-op on (ED_ROOM_SLOTS / ED_COOP_STAY, asked by play.bat): sets or clears the two
// switches for this run from the ask and Mods > Co-op, before the game's memory, title machine and hooks read them.
// True when that changed them.
bool rooms_follow_coop(void);

// A problem page over the game (report.c): heading, the text, and Keep
// waiting (when can_wait) or Quit. True when the player chose to wait.
bool frontend_problem(const char* heading, const char* text, bool can_wait);

// The disc id this build runs ("GEDE01" or "GEDP01"; main.c).
const char* ed_game_id(void);

// For AuroraBackendConfig.imgui_init: the FPS counter's font into ImGui.
void frontend_imgui_init(void);

// Photo mode is on (holding Back): the game sees no input.
bool frontend_photo_active(void);

// A short message in the screen's top right corner for a moment ("Saved to
// slot 3"). Any thread.
void frontend_toast(const char* text);
// The same in one co-op player's half (0 or 1), for longer.
void frontend_toast_player(int player, const char* text);
// A page asked for by the game's own menus (the Tome's Options, the title's Options), for player 0 or 1: the menu opens
// on it over the paused game, driven by that player's controller, and Back from it closes the menu.
enum {
    FRONTEND_PAGE_CHARACTERS = 0,  // that player's character roster
    FRONTEND_PAGE_SETTINGS = 1,    // Options: display, sound, graphics, controls
    FRONTEND_PAGE_MODS = 2,        // co-op, challenge, cheats
    FRONTEND_PAGE_STATES = 3,      // save states (player 1's)
    FRONTEND_PAGE_TITLE_SETTINGS = 4,  // Options from the title screen: with Mods (co-op, characters) and Quit
    // The title screen's own rows, each opening its page alone (Back from it closes the menu):
    FRONTEND_PAGE_TITLE_CHARACTERS = 5,  // Choose Character: the two players' cards (as the launcher's row)
    FRONTEND_PAGE_TITLE_DISPLAY = 6,
    FRONTEND_PAGE_TITLE_SOUND = 7,
    FRONTEND_PAGE_TITLE_GRAPHICS = 8,
    FRONTEND_PAGE_TITLE_CONTROLS = 9,    // Player I / Player II first, then that player's Controls
    FRONTEND_PAGE_TITLE_MODS = 10,
    FRONTEND_PAGE_TITLE_QUIT = 11,       // "Quit the game?" (No first); Yes saves the settings and quits
    // Solo or Co-op: two cards (docs/research/solo-coop-picker/plan.md). `player` is the title row it asks for
    // (0 Start Game, 1 Load Game: the page's heading). Nothing opens it from the game yet (plan step 3).
    FRONTEND_PAGE_TITLE_PICK = 12,
};
// How the page is drawn, chosen by whoever opens it: the launcher's parchment, the title's look (over the title
// screen), or a Tome page (over the Tome: in `half` 0 or 1, that co-op half, -1 the whole picture).
enum {
    FRONTEND_SKIN_LAUNCHER = 0,
    FRONTEND_SKIN_TITLE = 1,
    FRONTEND_SKIN_TOME = 2,
};
// frontend_open_page is frontend_open_page_skin(page, player, FRONTEND_SKIN_LAUNCHER, -1).
void frontend_open_page(int page, int player);
void frontend_open_page_skin(int page, int player, int skin, int half);
// Counts each time the menu closes (any thread): a caller sees a page it opened has closed when this moves on.
unsigned frontend_menu_closed_count(void);
// A title-look page is up with the title running under it (any thread), to the moment the game has its pad again
// (docs/research/firstrun-hd-flash/title-music.md). The game's frames go on: quick-state keys wait (quickstate.c).
bool frontend_page_live(void);
// Player 1's roster (frontend_open_page(FRONTEND_PAGE_CHARACTERS, 0)).
void frontend_open_characters(void);
// A button drawn as part of a Tome page, for this frame (asked each frame while the page is up): its text, the co-op half
// it's in (-1 the whole screen) and its centre as parts of that half's width and height.
void frontend_tome_button(const char* text, int half, float x, float y);

#ifdef __cplusplus
}
#include <string>
// The res folder (fonts, menus, icons).
std::string frontend_res_dir();
#endif
