#pragma once
// The Tome's Options plaques (plaques.cpp): host pictures drawn through the game's own art path from a small bank of
// transparent stubs hle.c writes into guest memory. Part of the renderer's one texture override (buttons.cpp).
#include <stdint.h>

#define PLAQUE_COUNT 4                // Characters, Save States, Settings, Mods (ids 0-3)
#define PLAQUE_STUB_W 280u            // each stub's size (CMPR): a size no game picture has
#define PLAQUE_STUB_H 64u
#define PLAQUE_STUB_BYTES ((PLAQUE_STUB_W / 8u) * (PLAQUE_STUB_H / 8u) * 32u)

#ifdef __cplusplus
extern "C" {
#endif
// A plaque's stub bytes (PLAQUE_STUB_BYTES): what hle.c writes into the bank, and what the hashes are made from.
void plaques_stub(int id, uint8_t* out);
// The Options page picture (UBookPak image 27, 432x480 CMPR) as the game has it in memory: the plaques' frame.
void plaques_page(const uint8_t* cmpr, uint32_t width, uint32_t height);
#ifdef __cplusplus
}
bool plaques_claims(uint32_t format, uint32_t width, uint32_t height);
bool plaques_claims_hash(uint64_t hash);
// 1 with the plaque's picture, 2 while it is drawn (the transparent stub shows), 0 not a plaque.
int plaques_apply(const uint8_t* rgba, uint32_t width, uint32_t height, uint64_t hash, uint8_t** out_rgba,
                  uint32_t* out_width, uint32_t* out_height);
uint64_t plaques_generation(void);
#endif
