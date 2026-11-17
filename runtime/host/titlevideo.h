#pragma once
// The title menu's pictures (titlevideo.cpp): Mods' Title video behind the
// title menu, and the Load screen (its memory card screen passed over, its
// card and labels hidden, headed "Load"). Part of the renderer's one texture
// override (buttons.cpp).
#ifdef __cplusplus
extern "C" {
#endif
// At boot, after texpack_init (main.c): with HD textures on, reads the pack's title and Options pictures on a worker
// (their BC7 decoded once), and writes the title look's HD backdrop (res/video/options-clean-hd.png) from the pack.
void titlevideo_preload(void);
// Waits (at most 3 s) for the HD workers still running, before main returns: they use statics the exit destroys.
// main.c calls it when the launcher is closed; titlevideo_preload also registers it with atexit.
void titlevideo_shutdown(void);
#ifdef __cplusplus
}

#include <cstdint>
#include <string>

// By format and size, then by name hash.
bool titlevideo_claims(uint32_t format, uint32_t width, uint32_t height);
bool titlevideo_claims_hash(uint32_t width, uint32_t height, uint64_t hash);
int titlevideo_apply(const uint8_t* rgba, uint32_t width, uint32_t height, uint64_t hash, uint8_t** out_rgba,
                     uint32_t* out_width, uint32_t* out_height);
// The pictures' changes with each video frame while it plays.
uint64_t titlevideo_generation(uint32_t width, uint32_t height);
// The menu's title look (frontend.cpp): DIR/options-clean.png (the Options picture, its words erased, upright) and the
// list's scroll marks DIR/mark-up.png, DIR/mark-down.png. False when the picture couldn't be written.
bool titlevideo_title_art(const std::string& dir);
// The title look's HD backdrop (DIR/options-clean-hd.png, made from the pack's Options picture at boot): whether it is
// written and up to date for this run (false without the pack or with HD textures off). With HD textures on and the
// pack's picture not read yet (turned on after boot), it starts the read, so the backdrop follows a few frames later.
bool titlevideo_title_art_hd();
#endif
