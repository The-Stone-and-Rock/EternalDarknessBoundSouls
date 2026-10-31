#pragma once
// The title menu's pictures (titlevideo.cpp): Mods' Title video behind the
// title menu, and the Load screen (its memory card screen passed over, its
// card and labels hidden, headed "Load"). Part of the renderer's one texture
// override (buttons.cpp).
#include <cstdint>

// By format and size, then by name hash.
bool titlevideo_claims(uint32_t format, uint32_t width, uint32_t height);
bool titlevideo_claims_hash(uint32_t width, uint32_t height, uint64_t hash);
int titlevideo_apply(const uint8_t* rgba, uint32_t width, uint32_t height, uint64_t hash, uint8_t** out_rgba,
                     uint32_t* out_width, uint32_t* out_height);
// The pictures' changes with each video frame while it plays.
uint64_t titlevideo_generation(uint32_t width, uint32_t height);
