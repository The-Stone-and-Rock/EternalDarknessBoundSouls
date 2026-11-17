#pragma once
// The Tome's controls page redrawn for the controller and layout in use
// (tome.cpp). Hooked into the texture override beside the button prompts
// (buttons.cpp).
#include <cstdint>

// Asked by format and size (432x480 CMPR: every Tome page), then by Dolphin's
// name hash: only the English controls page is claimed, so the other pages
// keep their HD pack versions.
bool tome_claims(uint32_t format, uint32_t width, uint32_t height);
bool tome_claims_hash(uint64_t hash);
// A texture override answer: 1 with the page, 2 while a worker draws it (the
// game's page shows meanwhile), 0 otherwise.
int tome_apply(const uint8_t* rgba, uint32_t width, uint32_t height, uint8_t** out_rgba,
               uint32_t* out_width, uint32_t* out_height);
// Changes with the layout, the bindings and the controller family.
uint64_t tome_generation();
#include <vector>
// The Tome's hand (La Belle Aurore) as an upright coverage mask: text px tall, strokes thickened by bold pixels; the
// first letter at column px / 4, the baseline at row *baseline. False without the font.
bool tome_hand_cover(const char* text, float px, float bold, std::vector<uint8_t>* out, int* w, int* h, int* baseline);
