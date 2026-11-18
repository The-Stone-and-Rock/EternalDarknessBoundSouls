// The Tome's Options plaques (hle.c tome_stops_*): the recomp's entries under the game's cross of four, drawn as the
// game draws its own art. hle.c puts a small picture bank in guest memory (a TEXPalette of transparent CMPR stubs, one
// per plaque) and draws each stub with the game's own quad calls, so the plaques sit in each player's half, in the
// book's layout, under the game's ring. When the renderer decodes a stub, the texture override (buttons.cpp) asks here
// and gets the plaque's picture: the game's own painted Load/Save plaque (its frame and parchment, from the Options
// page picture the game has in memory, image 27 of UBookPak.bin: nothing of the disc ships), its words rubbed out,
// and the entry's word written in the Tome's hand (La Belle Aurore, as tome.cpp's controls page), at 4x.
// docs/research/in-game-menu/06-texture-bank.md has the bank's bytes and the draw calls.
#include "host/plaques.h"

#include "host/tome.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include <xxhash.h>

namespace {

constexpr uint32_t kCmpr = 14;
constexpr int kCount = PLAQUE_COUNT;
const char* const kWords[kCount] = {"Characters", "Save States", "Settings", "Mods", "Exit Game"};
constexpr uint16_t kVersion = 2;  // in each stub's second block: a new look gets new bytes (new hashes)
// The picture: a 140x40 rectangle of the 640x480 page (the ring, 122x32, inside it as on the painted ones), 4 pixels
// to a unit.
constexpr int kUnitsW = 140, kUnitsH = 40, kScale = 4;
constexpr int kW = kUnitsW * kScale, kH = kUnitsH * kScale;
// The source, in the Options page picture (432x480, the page's x 144..575): the Load/Save plaque, its frame and
// lashings at x 140..279 and y 84..123 (its ring is 292..413, 87..119 on the page), the words in the band y 93..111
// between its end caps (x 156..261).
constexpr int kPageW = 432, kPageH = 480;
constexpr int kSrcX = 140, kSrcY = 84, kSrcW = 140, kSrcH = 40;
constexpr int kWordsY0 = 92, kWordsY1 = 112, kWordsX0 = 157, kWordsX1 = 262;
const float kInk[3] = {38.f, 27.f, 17.f};
constexpr float kInkAlpha = 0.88f;

std::mutex g_mutex;
std::vector<uint8_t> g_page;  // the Options page picture, upright RGBA
struct Pic {
    std::vector<uint8_t> rgba;  // stored upside down, as the game's textures are
    bool ready = false, running = false;
};
Pic g_pics[kCount];
uint64_t g_hashes[kCount];
bool g_hashed = false;

void hash_stubs() {
    if (g_hashed)
        return;
    std::vector<uint8_t> stub(PLAQUE_STUB_BYTES);
    for (int i = 0; i < kCount; i++) {
        plaques_stub(i, stub.data());
        g_hashes[i] = XXH64(stub.data(), stub.size(), 0);
    }
    g_hashed = true;
}

int which(uint64_t hash) {
    hash_stubs();
    for (int i = 0; i < kCount; i++)
        if (g_hashes[i] == hash)
            return i;
    return -1;
}

void rgb565(uint16_t c, float out[3]) {
    out[0] = ((c >> 11) & 31) * 255.f / 31.f;
    out[1] = ((c >> 5) & 63) * 255.f / 63.f;
    out[2] = (c & 31) * 255.f / 31.f;
}

// CMPR (big-endian, 8x8 tiles of four DXT1 blocks) to RGBA, upright (the stored rows are bottom first).
std::vector<uint8_t> decode(const uint8_t* src, uint32_t w, uint32_t h) {
    std::vector<uint8_t> stored(static_cast<size_t>(w) * h * 4, 0);
    const uint8_t* p = src;
    for (uint32_t ty = 0; ty < h; ty += 8)
        for (uint32_t tx = 0; tx < w; tx += 8)
            for (uint32_t sy = 0; sy < 8; sy += 4)
                for (uint32_t sx = 0; sx < 8; sx += 4, p += 8) {
                    const uint16_t c0 = static_cast<uint16_t>(p[0] << 8 | p[1]), c1 = static_cast<uint16_t>(p[2] << 8 | p[3]);
                    float pal[4][4];
                    rgb565(c0, pal[0]);
                    rgb565(c1, pal[1]);
                    pal[0][3] = pal[1][3] = 255.f;
                    for (int c = 0; c < 3; c++) {
                        if (c0 > c1) {
                            pal[2][c] = (2.f * pal[0][c] + pal[1][c]) / 3.f;
                            pal[3][c] = (pal[0][c] + 2.f * pal[1][c]) / 3.f;
                        } else {
                            pal[2][c] = (pal[0][c] + pal[1][c]) / 2.f;
                            pal[3][c] = 0.f;
                        }
                    }
                    pal[2][3] = 255.f;
                    pal[3][3] = c0 > c1 ? 255.f : 0.f;
                    const uint32_t bits = static_cast<uint32_t>(p[4]) << 24 | p[5] << 16 | p[6] << 8 | p[7];
                    for (int k = 0; k < 16; k++) {
                        const uint32_t x = tx + sx + (k & 3), y = ty + sy + (k >> 2);
                        if (x >= w || y >= h)
                            continue;
                        const float* c = pal[(bits >> (30 - 2 * k)) & 3];
                        uint8_t* o = &stored[(static_cast<size_t>(y) * w + x) * 4];
                        for (int i = 0; i < 4; i++)
                            o[i] = static_cast<uint8_t>(c[i] + 0.5f);
                    }
                }
    std::vector<uint8_t> upright(stored.size());
    for (uint32_t y = 0; y < h; y++)
        std::memcpy(&upright[static_cast<size_t>(y) * w * 4], &stored[static_cast<size_t>(h - 1 - y) * w * 4], w * 4);
    return upright;
}

float luma(const uint8_t* p) { return 0.3f * p[0] + 0.59f * p[1] + 0.11f * p[2]; }

// The plaque without its words: in the words' band each row's ink is put back to that row's parchment (the median of
// its lighter half), softly by how much darker than the parchment it is; the frame and lashings stay.
std::vector<uint8_t> blank_plaque(const std::vector<uint8_t>& page) {
    std::vector<uint8_t> out(static_cast<size_t>(kSrcW) * kSrcH * 4);
    for (int y = 0; y < kSrcH; y++)
        std::memcpy(&out[static_cast<size_t>(y) * kSrcW * 4], &page[(static_cast<size_t>(kSrcY + y) * kPageW + kSrcX) * 4],
                    kSrcW * 4);
    for (int py = kWordsY0; py < kWordsY1; py++) {
        std::vector<std::array<float, 4>> row;
        for (int px = kWordsX0; px < kWordsX1; px++) {
            const uint8_t* p = &page[(static_cast<size_t>(py) * kPageW + px) * 4];
            row.push_back({luma(p), static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2])});
        }
        std::sort(row.begin(), row.end(), [](const auto& a, const auto& b) { return a[0] < b[0]; });
        // The lighter half's median: the parchment under the words.
        const auto& paper = row[row.size() * 3 / 4];
        for (int px = kWordsX0; px < kWordsX1; px++) {
            uint8_t* o = &out[(static_cast<size_t>(py - kSrcY) * kSrcW + (px - kSrcX)) * 4];
            const float dark = std::clamp((paper[0] - luma(o)) / 22.f, 0.f, 1.f);
            for (int c = 0; c < 3; c++)
                o[c] = static_cast<uint8_t>(o[c] + (paper[1 + c] - o[c]) * dark);
        }
    }
    return out;
}

// Bilinear, from the source plaque (kSrcW x kSrcH) to the picture (kW x kH), upright.
std::vector<uint8_t> enlarge(const std::vector<uint8_t>& src) {
    std::vector<uint8_t> out(static_cast<size_t>(kW) * kH * 4);
    for (int y = 0; y < kH; y++)
        for (int x = 0; x < kW; x++) {
            const float fx = (x + 0.5f) * kSrcW / kW - 0.5f, fy = (y + 0.5f) * kSrcH / kH - 0.5f;
            const int x0 = std::clamp(static_cast<int>(std::floor(fx)), 0, kSrcW - 1), y0 = std::clamp(static_cast<int>(std::floor(fy)), 0, kSrcH - 1);
            const int x1 = std::min(x0 + 1, kSrcW - 1), y1 = std::min(y0 + 1, kSrcH - 1);
            const float ax = std::clamp(fx - x0, 0.f, 1.f), ay = std::clamp(fy - y0, 0.f, 1.f);
            for (int c = 0; c < 4; c++) {
                auto at = [&](int xx, int yy) { return static_cast<float>(src[(static_cast<size_t>(yy) * kSrcW + xx) * 4 + c]); };
                const float v = (at(x0, y0) * (1 - ax) + at(x1, y0) * ax) * (1 - ay) + (at(x0, y1) * (1 - ax) + at(x1, y1) * ax) * ay;
                out[(static_cast<size_t>(y) * kW + x) * 4 + c] = static_cast<uint8_t>(std::clamp(v + 0.5f, 0.f, 255.f));
            }
        }
    return out;
}

void build(int i, std::vector<uint8_t> page) {
    std::vector<uint8_t> pic = enlarge(blank_plaque(page));
    // The word, centred on the words' band, as large as the painted labels': their small letters about 7 of the page's
    // pixels tall. The hand's small letters are a small part of its size (tall ascenders and descenders), so the size
    // is found from an "x" measured at a trial size.
    const float page_to_pic = static_cast<float>(kH) / kSrcH;
    float xheight = 0.f;
    {
        std::vector<uint8_t> x_cover;
        int xw = 0, xh = 0, xb = 0;
        if (tome_hand_cover("x", 100.f, 0.f, &x_cover, &xw, &xh, &xb)) {
            int top = xh, bottom = -1;
            for (int y = 0; y < xh; y++)
                for (int x = 0; x < xw; x++)
                    if (x_cover[static_cast<size_t>(y) * xw + x] > 64) {
                        top = std::min(top, y);
                        bottom = std::max(bottom, y);
                    }
            xheight = bottom >= top ? (bottom - top + 1) / 100.f : 0.25f;  // per unit of size
        }
    }
    const float want_x = 7.5f * page_to_pic;
    const float px = xheight > 0.f ? want_x / xheight : 120.f;
    std::vector<uint8_t> cover;
    int cw = 0, ch = 0, base = 0;
    if (tome_hand_cover(kWords[i], px, 2.8f, &cover, &cw, &ch, &base)) {
        // The cover's ink spans from its first to its last non-empty column: centre that.
        int first = cw, last = -1;
        for (int x = 0; x < cw; x++)
            for (int y = 0; y < ch; y++)
                if (cover[static_cast<size_t>(y) * cw + x] > 24) {
                    first = std::min(first, x);
                    last = std::max(last, x);
                }
        const float band_mid = ((kWordsY0 + kWordsY1) / 2.f - kSrcY) * kH / kSrcH;
        const int left = (kW - (last - first + 1)) / 2 - first;
        const int baseline = static_cast<int>(band_mid + want_x * 0.5f);  // the small letters centred on the band
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw; x++) {
                const uint8_t v = cover[static_cast<size_t>(y) * cw + x];
                const int ox = left + x, oy = baseline - base + y;
                if (v == 0 || ox < 0 || oy < 0 || ox >= kW || oy >= kH)
                    continue;
                uint8_t* o = &pic[(static_cast<size_t>(oy) * kW + ox) * 4];
                const float a = v / 255.f * kInkAlpha;
                for (int c = 0; c < 3; c++)
                    o[c] = static_cast<uint8_t>(o[c] + (kInk[c] - o[c]) * a);
            }
    }
    std::vector<uint8_t> stored(pic.size());
    for (int y = 0; y < kH; y++)
        std::memcpy(&stored[static_cast<size_t>(y) * kW * 4], &pic[static_cast<size_t>(kH - 1 - y) * kW * 4], kW * 4);
    std::fprintf(stderr, "[plaques] \"%s\" drawn (%dx%d)\n", kWords[i], kW, kH);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_pics[i].rgba = std::move(stored);
    g_pics[i].running = false;
    g_pics[i].ready = true;
}

}  // namespace

extern "C" void plaques_stub(int id, uint8_t* out) {
    // Every block transparent (c0 0 <= c1, every index 3); c1 tells the plaques and the look apart, so each has its
    // own bytes and hash.
    for (uint32_t b = 0; b < PLAQUE_STUB_BYTES / 8u; b++) {
        const uint16_t kk = b == 0u ? static_cast<uint16_t>(0x5100 + id) : b == 1u ? kVersion : 0;
        uint8_t* o = out + b * 8u;
        o[0] = o[1] = 0;
        o[2] = static_cast<uint8_t>(kk >> 8);
        o[3] = static_cast<uint8_t>(kk & 0xFF);
        o[4] = o[5] = o[6] = o[7] = 0xFF;
    }
}

extern "C" void plaques_page(const uint8_t* cmpr, uint32_t width, uint32_t height) {
    if (width != static_cast<uint32_t>(kPageW) || height != static_cast<uint32_t>(kPageH))
        return;
    std::vector<uint8_t> page = decode(cmpr, width, height);
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_page.empty())
        return;
    g_page = std::move(page);
    std::fprintf(stderr, "[plaques] the Options page picture kept (%ux%u)\n", width, height);
}

bool plaques_claims(uint32_t format, uint32_t width, uint32_t height) {
    return format == kCmpr && width == PLAQUE_STUB_W && height == PLAQUE_STUB_H;
}

bool plaques_claims_hash(uint64_t hash) { return which(hash) >= 0; }

int plaques_apply(const uint8_t* rgba, uint32_t width, uint32_t height, uint64_t hash, uint8_t** out_rgba,
                  uint32_t* out_width, uint32_t* out_height) {
    (void)rgba;
    const int i = which(hash);
    if (i < 0 || width != PLAQUE_STUB_W || height != PLAQUE_STUB_H)
        return 0;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_pics[i].ready) {
        *out_rgba = static_cast<uint8_t*>(std::malloc(g_pics[i].rgba.size()));
        if (*out_rgba == nullptr)
            return 0;
        std::memcpy(*out_rgba, g_pics[i].rgba.data(), g_pics[i].rgba.size());
        *out_width = kW;
        *out_height = kH;
        return 1;
    }
    if (!g_pics[i].running && !g_page.empty()) {
        g_pics[i].running = true;
        std::thread(build, i, g_page).detach();
    }
    return 2;  // the stub (transparent) meanwhile
}

extern "C" uint64_t plaques_generation(void) { return kVersion; }
