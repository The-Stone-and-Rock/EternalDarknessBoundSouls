#pragma once
// Player settings shown in the launcher and the in-game menu, kept in
// settings.ini (ED_SETTINGS=path to use another file).
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int aspect;           // 1 4:3, 2 16:9 (the game's own widescreen view), 4 21:9 (that view
                          // widened); settings.ini's 0 (follow the game) and 3 (the old
                          // stretch) read as 16:9
    int render_scale;     // 0 window pixels, 1-4 = 720p, 1080p, 1440p, 4K
    int frame_rate;       // 60 the game's own; above it, frames in between at the screen's rate
    int game_font;        // the game's text: 0 sharp, 1 tome (default), 2 as on the disc
    bool hd_textures;     // use the texture pack in textures/ when there is one
    bool sharp_shadows;   // upscale the rooms' light maps as they load (lightmaps.cpp)
    int glow;             // light glows round lamps, flames, magick: 0 off, 1 soft, 2 strong
    bool hdr;             // HDR output where the display has it (light past SDR white)
    int ssao;             // ambient occlusion: 0 off, 1 soft, 2 strong
    bool ssao_high;       // ambient occlusion at full size, by its horizons (GTAO)
    bool smaa;            // SMAA on the finished scene, with or without MSAA
    int sharpen;          // sharpening (CAS): 0 off, 1 low, 2 medium, 3 high
    int shafts;           // light shafts: 0 off, 1 soft, 2 strong
    bool per_pixel_lighting;  // the game's lights worked out per pixel, not per vertex
    int char_shadows;     // characters' shadows: 0 the game's own, 1 light, 2 dark (from the game's lights)
    bool shadow_hard;     // their edges: crisp at any distance, not softening further from the character
    bool contact_shadows; // short shadows where characters meet what's round them
    int volumetric;       // the air lit by lamps and spells: 0 off, 1 soft, 2 strong
    bool skin_scatter;    // light through skin, with per-pixel lighting
    bool spell_rim;       // spells' meshes with glowing edges, and their sparks shining
    int anti_aliasing;    // 1: 4x multisampling (from the next start)
    int anisotropy;       // texture filtering: 1x, 2x, 4x, 8x, 16x (0-4, from the next start)
    // HDR calibration (Graphics enhancements > Calibrate HDR): the display's
    // peak and SDR white in nits, and the black level in steps (-5 sinks,
    // up to 10 lifts the darkest light).
    int hdr_peak;
    int hdr_paper;
    int hdr_black;
    bool fullscreen;
    bool fps_counter;     // frames per second in the window title
    int music_volume;     // disc-streamed music, 0-100
    int effects_volume;   // the game's own mix (effects, voices), 0-100
    int audio_buffer_ms;  // audio queued ahead of the output: 60 ms, not a
                          // setting (the renderer adds reserve if it runs dry)
    int controls_layout;  // 0 original, 1 modern, 2 custom
    // Hold or toggle, per controls layout (original, modern, custom).
    bool sprint_toggle[3];  // press to start/stop sprinting (stops when you stop)
    bool creep_toggle[3];
    bool target_toggle[3];
    int stick_deadzone[3];  // percent of the stick's travel ignored, per layout
    bool vibration;
    int camera;           // 0 the game's own, 2 third person, 3 first person (camera.h)
    // Each player's controller (player 1, player 2): which one is theirs (0
    // automatic, else the Nth connected) and its type (controls.h PadType: 0
    // automatic, 1 Xbox, 2 XInput, 3 PlayStation, 4 Switch).
    int controls_device;  // player 1: 0 controller and keyboard, 1 controller, 2 keyboard
    int pad_slot[2];
    int pad_type[2];
    // Mods (the Mods page; in development, nothing reads them yet).
    bool mod_coop;        // two-player split-screen co-op
    int mod_split;        // 0 side by side, 1 one above the other
    int mod_p2_input;     // player 2: 0 the second controller, 1 the keyboard
    int mod_p2_camera;    // player 2's camera: 0 third person, 1 first person, 2 the game's own
    int mod_p2_layout;    // player 2's controls_layout (0 original, 1 modern, 2 custom)
    char mod_p2_custom[512];  // player 2's custom bindings (controls.cpp format)
    int mod_stats;        // 0 each character's own health, magick and sanity, 1 custom
    int mod_health;       // custom stats: percent of the character's own, 50-300
    int mod_magick;
    int mod_sanity;
    int mod_enemies;      // 0 vanilla, 1 enhanced (harder hits, more creatures)
    int mod_insanity;     // sanity effects: 0 the game's own, 1 more often, 2 off
    int mod_p1_character; // player 1's character: 0 the chapter's own, 1-12 Pius .. Alex (Jump to Game order)
    int mod_p2_character; // player 2's (co-op), the same way: chosen, not yet played
    int mod_p1_stats;     // player 1's health, magick and sanity: 0 the chapter character's, 1 the chosen one's
    int mod_p2_stats;     // player 2's, the same way
    int mod_p1_costume;   // Pius's other body for player 1: 0 his own, 1 the skeletal one. Name, moves and stats stay his
    int mod_p2_costume;   // player 2's, the same way
    int mod_skip_intro;   // 1: the launcher's Main menu passes over the opening movie to the title
    int mod_title_video;  // 1: the title menu over res/video/title.mpg (titlevideo.cpp)
    char controls_custom[512];  // custom bindings (controls.cpp format)
    char disc[1024];      // last disc image chosen in the launcher
} Settings;

extern Settings g_settings;

void settings_load(void);
void settings_save(void);
// Push the current values to the renderer, window and audio (safe to call
// again after any change).
void settings_apply(void);
// Glow, ambient occlusion and HDR only: safe from the game thread (the
// in-game shortcuts); baked shadows and HD textures are read where used.
void settings_apply_graphics(void);

#ifdef __cplusplus
}
#endif
