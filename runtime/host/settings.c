// settings.ini: one `key = value` per line, unknown keys ignored, missing
// keys keep their defaults.
#include "host/settings.h"

#ifdef ED_AURORA
#include <aurora/gfx.h>
#include "gxruntime/aurora_backend.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Settings g_settings = {
    .aspect = 2,  // 16:9
    .render_scale = 0,
    .frame_rate = 60,
    .game_font = 1,  // Tome
    .hd_textures = true,
    .sharp_shadows = true,
    .glow = 0,
    .hdr = true,  // only where the display has it
    .ssao = 1,
    .anti_aliasing = 0,
    .anisotropy = 4,
    .ssao_high = true,
    .smaa = true,
    .sharpen = 1,
    .shafts = 0,
    .per_pixel_lighting = true,
    .char_shadows = 1,
    .shadow_hard = false,
    .contact_shadows = true,
    .volumetric = 50,
    .skin_scatter = true,
    .spell_rim = true,
    .hdr_peak = 1000,
#if defined(__APPLE__)
    .hdr_paper = 100,  // macOS's own SDR white (EDR 1.0)
#else
    .hdr_paper = 140,  // the PS5's own SDR picture
#endif
    .hdr_black = 0,
    .fullscreen = true,
    .fps_counter = false,
    .music_volume = 100,
    .effects_volume = 100,
    .audio_buffer_ms = 60,
    .controls_layout = 1,  // Modern
    // Modern: sprint (left stick click) and creep (right stick click) toggle.
    .sprint_toggle = {false, true, false},
    .creep_toggle = {false, true, false},
    .target_toggle = {false, false, false},
    .stick_deadzone = {5, 5, 5},
    .vibration = true,
    .camera = 0,
    .controls_device = 0,
    .pad_slot = {0, 0},
    .pad_type = {0, 0},
    .mod_coop = false,
    .mod_split = 0,
    .mod_p2_input = 0,
    .mod_p2_camera = 0,
    .mod_p2_layout = 1,  // Modern, as player 1
    .mod_p2_custom = "",
    .mod_stats = 0,
    .mod_health = 100,
    .mod_magick = 100,
    .mod_sanity = 100,
    .mod_enemies = 0,
    .mod_p1_character = 0,
    .mod_p2_character = 0,
    .mod_p1_stats = 1,
    .mod_p2_stats = 1,
    .mod_p1_costume = 0,
    .mod_p2_costume = 0,
    .mod_skip_intro = 0,
    .mod_title_video = 0,
    .mod_insanity = 0,
    .controls_custom = "",
    .disc = "",
};

static const char* settings_path(void) {
    const char* path = getenv("ED_SETTINGS");
    return path != NULL && path[0] != '\0' ? path : "settings.ini";
}

static int clamp_int(int value, int lo, int hi) {
    return value < lo ? lo : value > hi ? hi : value;
}

static char* trim(char* s) {
    while (*s == ' ' || *s == '\t')
        s++;
    char* end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' ||
                       end[-1] == '\r'))
        *--end = '\0';
    return s;
}

static const char* const kLayoutKeys[3] = {"original", "modern", "custom"};

// "sprint_toggle_modern = 1" into sprint_toggle[1].
static bool parse_toggle(const char* key, const char* prefix, bool values[3], int n) {
    const size_t len = strlen(prefix);
    if (strncmp(key, prefix, len) != 0)
        return false;
    for (int i = 0; i < 3; i++)
        if (strcmp(key + len, kLayoutKeys[i]) == 0) {
            values[i] = n != 0;
            return true;
        }
    return false;
}

void settings_load(void) {
    FILE* f = fopen(settings_path(), "r");
    if (f == NULL)
        return;
    char line[1200];
    while (fgets(line, sizeof line, f) != NULL) {
        char* eq = strchr(line, '=');
        if (line[0] == '#' || eq == NULL)
            continue;
        *eq = '\0';
        const char* key = trim(line);
        const char* value = trim(eq + 1);
        const int n = atoi(value);
        if (strcmp(key, "aspect") == 0)
            g_settings.aspect = n == 1 || n == 4 ? n : 2;
        else if (strcmp(key, "render_scale") == 0)
            g_settings.render_scale = clamp_int(n, 0, 4);
        else if (strcmp(key, "frame_rate") == 0)
            g_settings.frame_rate = clamp_int(n, 60, 360);
        else if (strcmp(key, "game_font") == 0)
            g_settings.game_font = clamp_int(n, 0, 2);
        else if (strcmp(key, "hd_textures") == 0)
            g_settings.hd_textures = n != 0;
        else if (strcmp(key, "sharp_shadows") == 0)
            g_settings.sharp_shadows = n != 0;
        else if (strcmp(key, "glow") == 0)
            g_settings.glow = clamp_int(n, 0, 2);
        else if (strcmp(key, "hdr") == 0)
            g_settings.hdr = n != 0;
        else if (strcmp(key, "ssao") == 0)
            g_settings.ssao = clamp_int(n, 0, 2);
        else if (strcmp(key, "ssao_high") == 0)
            g_settings.ssao_high = n != 0;
        else if (strcmp(key, "smaa") == 0)
            g_settings.smaa = n != 0;
        else if (strcmp(key, "sharpen") == 0)
            g_settings.sharpen = clamp_int(n, 0, 3);
        else if (strcmp(key, "shafts") == 0)
            g_settings.shafts = clamp_int(n, 0, 2);
        else if (strcmp(key, "per_pixel_lighting") == 0)
            g_settings.per_pixel_lighting = n != 0;
        else if (strcmp(key, "char_shadows") == 0)
            g_settings.char_shadows = clamp_int(n, 0, 2);
        else if (strcmp(key, "shadow_hard") == 0)
            g_settings.shadow_hard = n != 0;
        else if (strcmp(key, "haze") == 0)
            g_settings.volumetric = clamp_int(n, 0, 100);
        else if (strcmp(key, "volumetric") == 0)  // as it was saved: off, soft, strong
            g_settings.volumetric = clamp_int(n, 0, 2) * 50;
        else if (strcmp(key, "skin_scatter") == 0)
            g_settings.skin_scatter = n != 0;
        else if (strcmp(key, "spell_rim") == 0)
            g_settings.spell_rim = n != 0;
        else if (strcmp(key, "contact_shadows") == 0)
            g_settings.contact_shadows = n != 0;
        else if (strcmp(key, "anti_aliasing") == 0)
            g_settings.anti_aliasing = clamp_int(n, 0, 1);
        else if (strcmp(key, "anisotropy") == 0)
            g_settings.anisotropy = clamp_int(n, 0, 4);
        else if (strcmp(key, "hdr_peak") == 0)
            g_settings.hdr_peak = clamp_int(n, 100, 4000);
        else if (strcmp(key, "hdr_paper") == 0)
            g_settings.hdr_paper = clamp_int(n, 60, 400);
        else if (strcmp(key, "hdr_black") == 0)
            g_settings.hdr_black = clamp_int(n, -10, 10);
        else if (strcmp(key, "fullscreen") == 0)
            g_settings.fullscreen = n != 0;
        else if (strcmp(key, "fps_counter") == 0)
            g_settings.fps_counter = n != 0;
        else if (strcmp(key, "music_volume") == 0)
            g_settings.music_volume = clamp_int(n, 0, 100);
        else if (strcmp(key, "effects_volume") == 0)
            g_settings.effects_volume = clamp_int(n, 0, 100);
        else if (strcmp(key, "controls_layout") == 0)
            g_settings.controls_layout = clamp_int(n, 0, 2);
        else if (parse_toggle(key, "sprint_toggle_", g_settings.sprint_toggle, n) ||
                 parse_toggle(key, "creep_toggle_", g_settings.creep_toggle, n) ||
                 parse_toggle(key, "target_toggle_", g_settings.target_toggle, n))
            ;
        else if (strncmp(key, "stick_deadzone_", 15) == 0) {
            for (int i = 0; i < 3; i++)
                if (strcmp(key + 15, kLayoutKeys[i]) == 0)
                    g_settings.stick_deadzone[i] = clamp_int(n, 5, 25);
        }
        else if (strcmp(key, "vibration") == 0)
            g_settings.vibration = n != 0;
        else if (strcmp(key, "camera") == 0)
            g_settings.camera = n == 1 ? 0 : clamp_int(n, 0, 3);  // 1 was a free look
        else if (strcmp(key, "controls_device") == 0)
            g_settings.controls_device = clamp_int(n, 0, 2);
        else if (strcmp(key, "pad_slot_p1") == 0 || strcmp(key, "pad_slot_p2") == 0)
            g_settings.pad_slot[key[10] - '1'] = clamp_int(n, 0, 8);
        else if (strcmp(key, "pad_type_p1") == 0 || strcmp(key, "pad_type_p2") == 0)
            g_settings.pad_type[key[10] - '1'] = clamp_int(n, 0, 4);
        else if (strcmp(key, "mod_coop") == 0)
            g_settings.mod_coop = n != 0;
        else if (strcmp(key, "mod_split") == 0)
            g_settings.mod_split = clamp_int(n, 0, 1);
        else if (strcmp(key, "mod_p2_input") == 0)
            g_settings.mod_p2_input = clamp_int(n, 0, 1);
        else if (strcmp(key, "mod_p2_camera") == 0)
            g_settings.mod_p2_camera = clamp_int(n, 0, 2);
        else if (strcmp(key, "mod_p2_layout") == 0)
            g_settings.mod_p2_layout = clamp_int(n, 0, 2);
        else if (strcmp(key, "mod_p2_custom") == 0)
            snprintf(g_settings.mod_p2_custom, sizeof g_settings.mod_p2_custom, "%s", value);
        else if (strcmp(key, "mod_stats") == 0)
            g_settings.mod_stats = clamp_int(n, 0, 1);
        else if (strcmp(key, "mod_health") == 0)
            g_settings.mod_health = clamp_int(n, 50, 300);
        else if (strcmp(key, "mod_magick") == 0)
            g_settings.mod_magick = clamp_int(n, 50, 300);
        else if (strcmp(key, "mod_sanity") == 0)
            g_settings.mod_sanity = clamp_int(n, 50, 300);
        else if (strcmp(key, "mod_enemies") == 0)
            g_settings.mod_enemies = clamp_int(n, 0, 1);
        else if (strcmp(key, "mod_insanity") == 0)
            g_settings.mod_insanity = clamp_int(n, 0, 2);
        else if (strcmp(key, "mod_p1_character") == 0)
            g_settings.mod_p1_character = clamp_int(n, 0, 12);
        else if (strcmp(key, "mod_p2_character") == 0)
            g_settings.mod_p2_character = clamp_int(n, 0, 12);
        else if (strcmp(key, "mod_p1_stats") == 0)
            g_settings.mod_p1_stats = clamp_int(n, 0, 1);
        else if (strcmp(key, "mod_p2_stats") == 0)
            g_settings.mod_p2_stats = clamp_int(n, 0, 1);
        else if (strcmp(key, "mod_p1_costume") == 0)
            g_settings.mod_p1_costume = clamp_int(n, 0, 1);
        else if (strcmp(key, "mod_p2_costume") == 0)
            g_settings.mod_p2_costume = clamp_int(n, 0, 1);
        else if (strcmp(key, "mod_skip_intro") == 0)
            g_settings.mod_skip_intro = clamp_int(n, 0, 1);
        else if (strcmp(key, "mod_title_video") == 0)
            g_settings.mod_title_video = clamp_int(n, 0, 1);
        else if (strcmp(key, "controls_custom") == 0)
            snprintf(g_settings.controls_custom, sizeof g_settings.controls_custom, "%s", value);
        else if (strcmp(key, "disc") == 0)
            snprintf(g_settings.disc, sizeof g_settings.disc, "%s", value);
    }
    fclose(f);
}

void settings_save(void) {
    FILE* f = fopen(settings_path(), "w");
    if (f == NULL) {
        fprintf(stderr, "[settings] cannot write %s\n", settings_path());
        return;
    }
    fprintf(f, "# Eternal Darkness settings (written by the launcher)\n");
    fprintf(f, "aspect = %d\n", g_settings.aspect);
    fprintf(f, "render_scale = %d\n", g_settings.render_scale);
    fprintf(f, "frame_rate = %d\n", g_settings.frame_rate);
    fprintf(f, "game_font = %d\n", g_settings.game_font);
    fprintf(f, "hd_textures = %d\n", g_settings.hd_textures ? 1 : 0);
    fprintf(f, "sharp_shadows = %d\n", g_settings.sharp_shadows ? 1 : 0);
    fprintf(f, "glow = %d\n", g_settings.glow);
    fprintf(f, "hdr = %d\n", g_settings.hdr ? 1 : 0);
    fprintf(f, "ssao = %d\n", g_settings.ssao);
    fprintf(f, "ssao_high = %d\n", g_settings.ssao_high ? 1 : 0);
    fprintf(f, "smaa = %d\n", g_settings.smaa ? 1 : 0);
    fprintf(f, "sharpen = %d\n", g_settings.sharpen);
    fprintf(f, "shafts = %d\n", g_settings.shafts);
    fprintf(f, "per_pixel_lighting = %d\n", g_settings.per_pixel_lighting ? 1 : 0);
    fprintf(f, "char_shadows = %d\n", g_settings.char_shadows);
    fprintf(f, "shadow_hard = %d\n", g_settings.shadow_hard ? 1 : 0);
    fprintf(f, "contact_shadows = %d\n", g_settings.contact_shadows ? 1 : 0);
    fprintf(f, "haze = %d\n", g_settings.volumetric);
    fprintf(f, "skin_scatter = %d\n", g_settings.skin_scatter ? 1 : 0);
    fprintf(f, "spell_rim = %d\n", g_settings.spell_rim ? 1 : 0);
    fprintf(f, "anti_aliasing = %d\n", g_settings.anti_aliasing);
    fprintf(f, "anisotropy = %d\n", g_settings.anisotropy);
    fprintf(f, "hdr_peak = %d\n", g_settings.hdr_peak);
    fprintf(f, "hdr_paper = %d\n", g_settings.hdr_paper);
    fprintf(f, "hdr_black = %d\n", g_settings.hdr_black);
    fprintf(f, "fullscreen = %d\n", g_settings.fullscreen ? 1 : 0);
    fprintf(f, "fps_counter = %d\n", g_settings.fps_counter ? 1 : 0);
    fprintf(f, "music_volume = %d\n", g_settings.music_volume);
    fprintf(f, "effects_volume = %d\n", g_settings.effects_volume);
    fprintf(f, "controls_layout = %d\n", g_settings.controls_layout);
    for (int i = 0; i < 3; i++) {
        fprintf(f, "sprint_toggle_%s = %d\n", kLayoutKeys[i], g_settings.sprint_toggle[i] ? 1 : 0);
        fprintf(f, "creep_toggle_%s = %d\n", kLayoutKeys[i], g_settings.creep_toggle[i] ? 1 : 0);
        fprintf(f, "target_toggle_%s = %d\n", kLayoutKeys[i], g_settings.target_toggle[i] ? 1 : 0);
    }
    for (int i = 0; i < 3; i++)
        fprintf(f, "stick_deadzone_%s = %d\n", kLayoutKeys[i], g_settings.stick_deadzone[i]);
    fprintf(f, "vibration = %d\n", g_settings.vibration ? 1 : 0);
    fprintf(f, "camera = %d\n", g_settings.camera);
    fprintf(f, "controls_device = %d\n", g_settings.controls_device);
    for (int i = 0; i < 2; i++) {
        fprintf(f, "pad_slot_p%d = %d\n", i + 1, g_settings.pad_slot[i]);
        fprintf(f, "pad_type_p%d = %d\n", i + 1, g_settings.pad_type[i]);
    }
    fprintf(f, "mod_coop = %d\n", g_settings.mod_coop ? 1 : 0);
    fprintf(f, "mod_split = %d\n", g_settings.mod_split);
    fprintf(f, "mod_p2_input = %d\n", g_settings.mod_p2_input);
    fprintf(f, "mod_p2_camera = %d\n", g_settings.mod_p2_camera);
    fprintf(f, "mod_p2_layout = %d\n", g_settings.mod_p2_layout);
    fprintf(f, "mod_p2_custom = %s\n", g_settings.mod_p2_custom);
    fprintf(f, "mod_stats = %d\n", g_settings.mod_stats);
    fprintf(f, "mod_health = %d\n", g_settings.mod_health);
    fprintf(f, "mod_magick = %d\n", g_settings.mod_magick);
    fprintf(f, "mod_sanity = %d\n", g_settings.mod_sanity);
    fprintf(f, "mod_enemies = %d\n", g_settings.mod_enemies);
    fprintf(f, "mod_insanity = %d\n", g_settings.mod_insanity);
    fprintf(f, "mod_p1_character = %d\n", g_settings.mod_p1_character);
    fprintf(f, "mod_p2_character = %d\n", g_settings.mod_p2_character);
    fprintf(f, "mod_p1_stats = %d\n", g_settings.mod_p1_stats);
    fprintf(f, "mod_p2_stats = %d\n", g_settings.mod_p2_stats);
    fprintf(f, "mod_p1_costume = %d\n", g_settings.mod_p1_costume);
    fprintf(f, "mod_p2_costume = %d\n", g_settings.mod_p2_costume);
    fprintf(f, "mod_skip_intro = %d\n", g_settings.mod_skip_intro);
    fprintf(f, "mod_title_video = %d\n", g_settings.mod_title_video);
    fprintf(f, "controls_custom = %s\n", g_settings.controls_custom);
    fprintf(f, "disc = %s\n", g_settings.disc);
    fclose(f);
}

void settings_apply(void) {
#ifdef ED_AURORA
    dol_aurora_set_aspect_mode(g_settings.aspect);
    dol_aurora_set_widescreen_hud(true);  // the HUD's side prompts at the screen's edges
    static const int kRenderLines[] = {0, 720, 1080, 1440, 2160};
    dol_aurora_set_render_height(kRenderLines[g_settings.render_scale]);
    dol_aurora_set_fullscreen(g_settings.fullscreen);
    // Split-screen co-op plays at the game's own 60: each moment between frames is drawn twice, once for each
    // player, and they don't fit. The setting is kept for solo. `ed system graphics`, Frame rate.
    dol_aurora_set_frame_rate(g_settings.mod_coop ? 60 : g_settings.frame_rate);
    dol_aurora_set_audio_buffer_ms(g_settings.audio_buffer_ms);
    dol_aurora_set_vibration(g_settings.vibration);
#endif
    settings_apply_graphics();
}

void settings_apply_graphics(void) {
#ifdef ED_AURORA
    static const float kGlow[] = {0.f, 0.6f, 1.0f};
    dol_aurora_set_glow(kGlow[g_settings.glow]);
    /* The living energy in the temples (the red doorways, the yellow floors):
     * a 256x256 C8 texture whose palette the game cycles every frame. With
     * HDR on it glows, at Soft when the glow setting is off. */
    aurora_glow_add_texture(9u, 256u, 256u, 0u, kGlow[1]);
    /* The streaks of a magick effect (green and white), round a struck
     * enemy and the player: by Dolphin's hash. */
    aurora_glow_add_texture(14u, 64u, 64u, 0x81aacb2ba734f59bull, kGlow[1]);
    aurora_glow_add_texture(14u, 64u, 64u, 0x7cb863247f69c913ull, kGlow[1]);
    dol_aurora_set_hdr(g_settings.hdr);
    static const float kSsao[] = {0.f, 1.3f, 2.4f};
    dol_aurora_set_ssao(kSsao[g_settings.ssao]);
    dol_aurora_set_ssao_high(g_settings.ssao_high);
    dol_aurora_set_smaa(g_settings.smaa);
    static const float kSharpen[] = {0.f, 0.35f, 0.65f, 1.f};
    dol_aurora_set_sharpen(kSharpen[g_settings.sharpen]);
    static const float kShafts[] = {0.f, 0.6f, 1.2f};
    dol_aurora_set_shafts(kShafts[g_settings.shafts]);
    dol_aurora_set_per_pixel_lighting(g_settings.per_pixel_lighting);
    static const float kCharShadows[] = {0.f, 0.5f, 0.8f};
    dol_aurora_set_char_shadows(kCharShadows[g_settings.char_shadows]);
    dol_aurora_set_shadow_hard(g_settings.shadow_hard);
    dol_aurora_set_volumetric(0.9f * (float)g_settings.volumetric / 100.f);
    dol_aurora_set_skin_scatter(g_settings.skin_scatter);
    dol_aurora_set_spell_rim(g_settings.spell_rim);
    dol_aurora_set_contact_shadows(g_settings.contact_shadows ? 0.45f : 0.f);
    // Black level in steps of 0.0002 linear: -10 sinks the darkest 0.002
    // (about 7/255 of SDR) to black, the most that looked right on an OLED.
    dol_aurora_set_hdr_levels((float)g_settings.hdr_paper, (float)g_settings.hdr_peak,
                              (float)g_settings.hdr_black * 0.0002f);
    // Highlights up to 3.5x SDR white, 4x with ambient occlusion (which
    // darkens the room round them). The PS5 (HDR10, SDR white at 140 nits to
    // match the console's own SDR picture) goes to 5x and 6x: about 700 and
    // 850 nits, a TV's peak, where 3.5x at 200 nits only looked lighter.
#if defined(__PROSPERO__)
    dol_aurora_set_hdr_highlight(g_settings.ssao != 0 ? 5.0f : 4.0f);
#else
    dol_aurora_set_hdr_highlight(g_settings.ssao != 0 ? 3.0f : 2.5f);
#endif
#endif
}
