#pragma once
// Mods' Title video: a video of your own behind the title menu, in place of
// the game's still picture (titlevideo.cpp). Part of the renderer's one
// texture override (buttons.cpp).
#include <cstdint>

bool titlevideo_claims(uint32_t format, uint32_t width, uint32_t height);
int titlevideo_apply(const uint8_t* rgba, uint32_t width, uint32_t height, uint8_t** out_rgba,
                     uint32_t* out_width, uint32_t* out_height);
uint64_t titlevideo_generation();
