// Mods' Title video. The title menu's picture is one texture (850x480, CMPR,
// stored upside down) with the menu's words (Start Game, Load Game, Options)
// and the copyright line painted in; the chosen word's glow, Jump to Game and
// Credits are drawn over it. With the toggle on and res/video/title.mpg made
// (tools/title_video.py), that texture is each new frame of the video, looping
// silently (the game's music goes on), with the painted words and copyright
// line lifted from the original and laid over it.
#include "host/titlevideo.h"

#include "host/frontend.h"
#include "host/settings.h"
#include "host/tome.h"

#include <png.h>

#define PL_MPEG_IMPLEMENTATION
#include "third_party/pl_mpeg.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr uint32_t kCmpr = 0x0E;
constexpr uint32_t kTitleW = 850, kTitleH = 480;

// Where the original's painted text lies, in its own (stored, upside down)
// coordinates: the menu's words (over the dark of the hair, the face to their
// left) and the copyright line (on black).
struct Box {
    uint32_t x0, y0, x1, y1;
    bool words;
};
// The words' box reaches down to moved Options and left to "Choose Character" (title_main_*: upright y 254..410).
constexpr Box kText[] = {{490, 69, 745, 225, true}, {340, 40, 500, 70, false}};

struct Video {
    std::mutex mutex;
    std::vector<uint8_t> frame;  // RGBA, upside down as the texture is
    uint32_t width = 0, height = 0;
    std::atomic<uint64_t> serial{0};  // frames made
    std::atomic<int64_t> asked_ms{0};  // when the title last drew it
    bool started = false, missing = false;
} g;

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string video_path() {
    return (std::filesystem::path(frontend_res_dir()) / "video" / "title.mpg").string();
}

void on_frame(plm_t*, plm_frame_t* frame, void*) {
    const uint32_t w = frame->width, h = frame->height;
    std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
    plm_frame_to_rgba(frame, rgba.data(), static_cast<int>(w * 4));
    for (size_t i = 3; i < rgba.size(); i += 4)
        rgba[i] = 255;  // pl_mpeg leaves alpha alone
    std::lock_guard<std::mutex> lock(g.mutex);
    g.frame.resize(rgba.size());
    for (uint32_t y = 0; y < h; y++)
        std::memcpy(&g.frame[static_cast<size_t>(h - 1 - y) * w * 4], &rgba[static_cast<size_t>(y) * w * 4],
                    static_cast<size_t>(w) * 4);
    g.width = w;
    g.height = h;
    g.serial++;
}

// Plays the video in real time while the title draws it; idle otherwise.
void player(std::string path) {
    plm_t* plm = plm_create_with_filename(path.c_str());
    if (plm == nullptr) {
        std::fprintf(stderr, "[titlevideo] %s could not be read\n", path.c_str());
        return;
    }
    plm_set_audio_enabled(plm, 0);
    plm_set_loop(plm, 1);
    plm_set_video_decode_callback(plm, on_frame, nullptr);
    std::fprintf(stderr, "[titlevideo] %s: %dx%d at %.0f a second\n", path.c_str(), plm_get_width(plm),
                 plm_get_height(plm), plm_get_framerate(plm));
    auto last = std::chrono::steady_clock::now();
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - last).count();
        last = now;
        if (now_ms() - g.asked_ms.load() < 1000)
            plm_decode(plm, std::min(dt, 0.1));
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }
}

void start() {
    if (g.started)
        return;
    g.started = true;
    const std::string path = video_path();
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        g.missing = true;
        std::fprintf(stderr, "[titlevideo] no %s (tools/title_video.py makes it)\n", path.c_str());
        return;
    }
    std::thread(player, path).detach();
}

float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

float smooth(float edge0, float edge1, float v) {
    const float t = clamp01((v - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

// The original's colour at a point of the larger frame, filtered as the game
// draws it scaled (bilinear between its texels' centres).
void sample(const uint8_t* rgba, uint32_t width, uint32_t height, float fx, float fy, float out[3]) {
    fx = std::clamp(fx, 0.0f, static_cast<float>(width - 1));
    fy = std::clamp(fy, 0.0f, static_cast<float>(height - 1));
    const uint32_t x0 = static_cast<uint32_t>(fx), y0 = static_cast<uint32_t>(fy);
    const uint32_t x1 = std::min(x0 + 1, width - 1), y1 = std::min(y0 + 1, height - 1);
    const float tx = fx - x0, ty = fy - y0;
    for (int c = 0; c < 3; c++) {
        const float top = rgba[(y0 * width + x0) * 4 + c] * (1 - tx) + rgba[(y0 * width + x1) * 4 + c] * tx;
        const float bottom = rgba[(y1 * width + x0) * 4 + c] * (1 - tx) + rgba[(y1 * width + x1) * 4 + c] * tx;
        out[c] = top * (1 - ty) + bottom * ty;
    }
}

// A box blur of a w x h field, radius r each way.
void blur(std::vector<float>& field, uint32_t w, uint32_t h, int r) {
    std::vector<float> tmp(field.size());
    for (uint32_t y = 0; y < h; y++)
        for (uint32_t x = 0; x < w; x++) {
            float sum = 0.0f;
            for (int k = -r; k <= r; k++)
                sum += field[y * w + std::clamp<int>(static_cast<int>(x) + k, 0, static_cast<int>(w) - 1)];
            tmp[y * w + x] = sum / (2 * r + 1);
        }
    for (uint32_t y = 0; y < h; y++)
        for (uint32_t x = 0; x < w; x++) {
            float sum = 0.0f;
            for (int k = -r; k <= r; k++)
                sum += tmp[std::clamp<int>(static_cast<int>(y) + k, 0, static_cast<int>(h) - 1) * w + x];
            field[y * w + x] = sum / (2 * r + 1);
        }
}

// The Load screen. Load Game goes straight to the saves (hle.c
// skip_card_select), but the save list is drawn over the memory card screen:
// its picture (another 850x480) with "Memory Card", "Slot A", "Slot B" and two
// card frames painted in its top left, the GameCube card and its highlight
// (96x111), and a "Slot B" label (79x25). The card and label are hidden; the
// picture is the video (or itself, its top left faded out), headed "Load" in
// the game's own lit lettering (the title menu's "Load Game", 160x62, kept as
// it is drawn there).
constexpr uint64_t kTitleHash = 0x2a6eea64eb8fbbbbull;
constexpr uint64_t kLoadScreenHash = 0x14eea9ccc025ff46ull;
constexpr uint32_t kWordW = 160, kWordH = 62;
constexpr uint64_t kLoadWordHash = 0xa46c75052d75745aull;
constexpr uint32_t kLoadWordEnd = 81;  // "Load" (then "Game" from 83)

struct Hidden {
    uint32_t width, height;
    uint64_t hash;
};
constexpr Hidden kHidden[] = {{96, 111, 0xb3e143ae44ce4be9ull},  // the card
                              {96, 111, 0x36369b836df93775ull},  // its highlight
                              {79, 25, 0xeef276c2f51d5608ull}};  // "Slot B"

std::mutex g_word_mutex;
std::vector<uint8_t> g_word;   // the lit "Load Game", as stored
// The Options screen's picture (the X-ray skull, the logo, its words painted
// on the left), kept as stored and in res/video/options.rgba, so the Load
// screen has it before Options is ever opened.
constexpr uint64_t kOptionsHash = 0xbefc5ef6c1629595ull;
std::vector<uint8_t> g_skull;
bool g_skull_read = false;
constexpr float kSkullGlow = 0.6f;  // how strongly it lights the Load screen's video
// Where the skull's eye socket is, and her eye in the video (as seen in
// 850x480), and how much larger her face is (eye to chin).
constexpr float kSkullEye[2] = {580.0f, 210.0f};
constexpr float kHerEye[2] = {620.0f, 210.0f};
constexpr float kSkullScale = 1.12f;

std::string skull_path() {
    return (std::filesystem::path(frontend_res_dir()) / "video" / "options.rgba").string();
}

// The kept picture (under g_word_mutex), read from disk the first time.
const std::vector<uint8_t>& skull() {
    if (g_skull.empty() && !g_skull_read) {
        g_skull_read = true;
        if (FILE* f = std::fopen(skull_path().c_str(), "rb")) {
            std::vector<uint8_t> data(static_cast<size_t>(kTitleW) * kTitleH * 4);
            if (std::fread(data.data(), 1, data.size(), f) == data.size())
                g_skull = std::move(data);
            std::fclose(f);
        }
    }
    return g_skull;
}

void keep_skull(const uint8_t* rgba) {
    std::lock_guard<std::mutex> lock(g_word_mutex);
    if (!skull().empty())
        return;
    g_skull.assign(rgba, rgba + static_cast<size_t>(kTitleW) * kTitleH * 4);
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(skull_path()).parent_path(), ec);
    if (FILE* f = std::fopen(skull_path().c_str(), "wb")) {
        std::fwrite(g_skull.data(), 1, g_skull.size(), f);
        std::fclose(f);
        std::fprintf(stderr, "[titlevideo] the Options picture kept in %s\n", skull_path().c_str());
    }
}

bool video_on() {
    if (!g_settings.mod_title_video)
        return false;
    start();
    return !g.missing;
}

bool is_picture(uint32_t width, uint32_t height) { return width == kTitleW && height == kTitleH; }

const Hidden* hidden(uint32_t width, uint32_t height, uint64_t hash) {
    for (const Hidden& h : kHidden)
        if (h.width == width && h.height == height && (hash == 0u || h.hash == hash))
            return &h;
    return nullptr;
}

// The newest video frame, else empty.
std::vector<uint8_t> video_frame(uint32_t* w, uint32_t* h) {
    std::lock_guard<std::mutex> lock(g.mutex);
    *w = g.width;
    *h = g.height;
    return g.frame;
}

// The still of the video behind the Load screen: the moment the skull was
// fitted to her face.
constexpr double kStillAt = 8.0;
std::vector<uint8_t> g_still;
uint32_t g_still_w = 0, g_still_h = 0;
bool g_still_read = false;

std::vector<uint8_t> still(uint32_t* w, uint32_t* h) {
    if (!g_still_read) {
        g_still_read = true;
        if (plm_t* plm = plm_create_with_filename(video_path().c_str())) {
            plm_set_audio_enabled(plm, 0);
            if (plm_frame_t* frame = plm_seek_frame(plm, kStillAt, 1)) {
                const uint32_t fw = frame->width, fh = frame->height;
                std::vector<uint8_t> rgba(static_cast<size_t>(fw) * fh * 4);
                plm_frame_to_rgba(frame, rgba.data(), static_cast<int>(fw * 4));
                g_still.resize(rgba.size());
                for (uint32_t y = 0; y < fh; y++)  // upside down, as the texture is
                    std::memcpy(&g_still[static_cast<size_t>(fh - 1 - y) * fw * 4],
                                &rgba[static_cast<size_t>(y) * fw * 4], static_cast<size_t>(fw) * 4);
                for (size_t i = 3; i < g_still.size(); i += 4)
                    g_still[i] = 255;
                g_still_w = fw;
                g_still_h = fh;
            }
            plm_destroy(plm);
        }
    }
    *w = g_still_w;
    *h = g_still_h;
    return g_still;
}

// The Load screen as last drawn over the still, kept while what it was drawn
// with (the word, the skull) stays the same.
std::mutex g_load_mutex;
struct {
    std::vector<uint8_t> rgba;
    uint32_t width = 0, height = 0;
    bool word = false, skull = false;
} g_load;

int give(std::vector<uint8_t>& out, uint32_t w, uint32_t h, uint8_t** out_rgba, uint32_t* out_width,
         uint32_t* out_height) {
    *out_rgba = static_cast<uint8_t*>(std::malloc(out.size()));
    if (*out_rgba == nullptr)
        return 0;
    std::memcpy(*out_rgba, out.data(), out.size());
    *out_width = w;
    *out_height = h;
    return 1;
}

// The title Options' ninth row (hle.c title_more_*: the game's own cursor goes there, A opens the recomp's settings):
// "More Options" painted under "Language Select" in the Tome's hand, in the painted rows' teal; lit pale cyan with a
// soft glow while it is the chosen row, as the game lights its own. Measured on the kept picture (upright 850x480):
// the rows' tops step 41-47 (Language Select's at 384), their first letters at x 174, their ink about (67, 90, 96)
// and at most (90, 117, 123).
std::atomic<int> g_options_row{-1};  // the title Options' cursor, as its frame last drew (hle.c)
constexpr uint64_t kOptionsMoreSerial = 0x4D4F52450000ull;  // the Options picture's generations ("MORE")
struct Painted {
    std::vector<uint8_t> rgba;
    bool lit = false, ready = false;
};
std::mutex g_more_mutex;
Painted g_more[2];

std::vector<uint8_t> paint_more(const uint8_t* rgba, uint32_t w, uint32_t h, bool lit) {
    std::vector<uint8_t> out(rgba, rgba + static_cast<size_t>(w) * h * 4);
    std::vector<uint8_t> cover;
    int cw = 0, ch = 0, base = 0;
    constexpr float kPx = 44.f;
    if (!tome_hand_cover("More Options", kPx, 0.7f, &cover, &cw, &ch, &base))
        return out;
    const int left = 174 - static_cast<int>(kPx / 4.f), baseline = 452;
    const float ink[3] = {72.f, 97.f, 104.f}, light[3] = {0xA0, 0xE3, 0xF1};
    auto put = [&](int x, int y, const float rgb[3], float a) {
        if (x < 0 || y < 0 || x >= static_cast<int>(w) || y >= static_cast<int>(h) || a <= 0.f)
            return;
        uint8_t* p = &out[(static_cast<size_t>(h - 1 - y) * w + x) * 4];  // stored upside down
        for (int c = 0; c < 3; c++)
            p[c] = static_cast<uint8_t>(std::clamp(p[c] + (rgb[c] - p[c]) * std::min(a, 1.f), 0.f, 255.f));
    };
    if (lit) {  // the glow: the letters spread 6 pixels, faint
        constexpr int kR = 6;
        for (int y = -kR; y < ch + kR; y++)
            for (int x = -kR; x < cw + kR; x++) {
                float m = 0.f;
                for (int dy = -kR; dy <= kR; dy += 2)
                    for (int dx = -kR; dx <= kR; dx += 2) {
                        const int sx = x + dx, sy = y + dy;
                        if (sx < 0 || sy < 0 || sx >= cw || sy >= ch)
                            continue;
                        const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                        if (d <= kR)
                            m = std::max(m, cover[static_cast<size_t>(sy) * cw + sx] / 255.f * (1.f - d / (kR + 1)));
                    }
                put(left + x, baseline - base + y, light, m * 0.35f);
            }
    }
    for (int y = 0; y < ch; y++)
        for (int x = 0; x < cw; x++)
            if (const uint8_t v = cover[static_cast<size_t>(y) * cw + x])
                put(left + x, baseline - base + y, lit ? light : ink, v / 255.f * (lit ? 1.f : 0.9f));
    return out;
}

// The title's main menu (hle.c title_main_*): "Choose Character" between Load Game and Options, the game's spare row 5.
// The disc's picture (texture 1) has Start Game, Load Game and Options painted in; Options is lifted, its place filled
// from the hair round it, and put back 34 lower (its glow quad moved the same in RAM, as Jump's and Credits'), and the
// new words painted where it was, in the Tome's hand and the painted rows' teal, lit pale cyan with a soft glow while
// the game's cursor is on row 5 (as "More Options"). Measured on the disc's picture (upright 850x480,
// docs/research/in-game-menu/10-title-choose-character.md section 2): Options' ink x 562..652, y 326..367; the rows
// centred about x 618; Load Game's ink ends at y 324.
std::atomic<int> g_menu_row{-1};  // the main menu's cursor as its frame last drew; -1 not English (hle.c)
std::atomic<bool> g_menu_painted{false};
constexpr int kMenuShift = 34;
Painted g_menu[2];
std::mutex g_menu_mutex;

// The words' ink as the disc's picture paints them (the same test as the video's lift, below).
float menu_ink(const uint8_t* p) {
    const float lum = 0.3f * p[0] + 0.59f * p[1] + 0.11f * p[2];
    return smooth(45.0f, 80.0f, lum) * smooth(12.0f, 24.0f, static_cast<float>(p[2]) - p[0]);
}

std::vector<uint8_t> paint_menu(const uint8_t* rgba, uint32_t w, uint32_t h, bool lit, bool* ok) {
    std::vector<uint8_t> out(rgba, rgba + static_cast<size_t>(w) * h * 4);
    *ok = false;
    auto at = [&](std::vector<uint8_t>& v, int x, int y) { return &v[(static_cast<size_t>(h - 1 - y) * w + x) * 4]; };
    auto src = [&](int x, int y) { return &rgba[(static_cast<size_t>(h - 1 - y) * w + x) * 4]; };
    // 1. Options' ink, in a box round it (soft, smoothed once). Its faint edges (darker than the ink test takes, but
    // still teal over the near-black hair) are found by a lower test, spread, for the hole.
    constexpr int bx0 = 554, bx1 = 668, by0 = 325, by1 = 374;  // (Load Game's ink ends at 324: left alone)
    constexpr int bw = bx1 - bx0, bh = by1 - by0;
    std::vector<float> mask(static_cast<size_t>(bw) * bh), wide(mask.size());
    for (int y = 0; y < bh; y++)
        for (int x = 0; x < bw; x++) {
            const uint8_t* p = src(bx0 + x, by0 + y);
            const float lum = 0.3f * p[0] + 0.59f * p[1] + 0.11f * p[2];
            mask[static_cast<size_t>(y) * bw + x] = menu_ink(p);
            wide[static_cast<size_t>(y) * bw + x] = smooth(16.0f, 36.0f, lum) * smooth(3.0f, 10.0f, static_cast<float>(p[2]) - p[0]);
        }
    std::vector<float> lift = mask;
    blur(lift, bw, bh, 1);
    for (size_t i = 0; i < lift.size(); i++)
        lift[i] = clamp01(std::max(lift[i] * 1.4f, wide[i]));
    blur(wide, bw, bh, 2);
    float ink = 0.f;
    for (float m : mask)
        ink += m;
    if (ink < 200.f)
        return out;  // not the picture measured: left as it is
    // 2. The hole filled: each point under the words takes the hair round it (a blur of the points not under them).
    std::vector<float> keep(static_cast<size_t>(bw) * bh), sum[3];
    for (int c = 0; c < 3; c++)
        sum[c].assign(keep.size(), 0.f);
    for (size_t i = 0; i < keep.size(); i++) {
        keep[i] = 1.f - clamp01(wide[i] * 3.f);
        const int x = static_cast<int>(i % bw), y = static_cast<int>(i / bw);
        for (int c = 0; c < 3; c++)
            sum[c][i] = src(bx0 + x, by0 + y)[c] * keep[i];
    }
    for (int c = 0; c < 3; c++)
        blur(sum[c], bw, bh, 7);
    blur(keep, bw, bh, 7);
    for (int y = 0; y < bh; y++)
        for (int x = 0; x < bw; x++) {
            const size_t i = static_cast<size_t>(y) * bw + x;
            const float a = clamp01(std::max(lift[i], clamp01(wide[i] * 3.f)));
            if (a <= 0.f || keep[i] <= 0.001f)
                continue;
            uint8_t* d = at(out, bx0 + x, by0 + y);
            for (int c = 0; c < 3; c++)
                d[c] = static_cast<uint8_t>(std::clamp(d[c] + (sum[c][i] / keep[i] - d[c]) * a, 0.f, 255.f));
        }
    // 3. Options put back 34 lower, its colour mixed in by its own mask (the ground there is nearly black).
    for (int y = 0; y < bh; y++)
        for (int x = 0; x < bw; x++) {
            const float a = lift[static_cast<size_t>(y) * bw + x];
            const int ty = by0 + y + kMenuShift;
            if (a <= 0.f || ty >= static_cast<int>(h))
                continue;
            const uint8_t* s = src(bx0 + x, by0 + y);
            uint8_t* d = at(out, bx0 + x, ty);
            for (int c = 0; c < 3; c++)
                d[c] = static_cast<uint8_t>(std::clamp(d[c] + (s[c] - d[c]) * a, 0.f, 255.f));
        }
    // 4. "Choose Character" where Options was: at most 232 wide, centred on the rows, its baseline at 356.
    std::vector<uint8_t> cover;
    int cw = 0, ch = 0, base = 0, ix0 = 0, ix1 = 0;
    // As large as the game's own words (Load Game's ink is 33 tall), but no wider than 232 and its top under Load Game.
    for (float px = 64.f; px >= 30.f; px -= 1.f) {
        if (!tome_hand_cover("Choose Character", px, 0.8f, &cover, &cw, &ch, &base))
            return out;
        ix0 = cw;
        ix1 = -1;
        int iy0 = ch;
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw; x++)
                if (cover[static_cast<size_t>(y) * cw + x] > 24) {
                    ix0 = std::min(ix0, x);
                    ix1 = std::max(ix1, x);
                    iy0 = std::min(iy0, y);
                }
        if (ix1 - ix0 <= 232 && base - iy0 <= 30)
            break;
    }
    if (ix1 < ix0)
        return out;
    const int left = 618 - (ix0 + ix1) / 2, top = 356 - base;
    const float paint[3] = {72.f, 97.f, 104.f}, light[3] = {0xA0, 0xE3, 0xF1}, dark[3] = {0.f, 0.f, 0.f};
    auto put = [&](int x, int y, const float rgb[3], float a) {
        if (x < 0 || y < 0 || x >= static_cast<int>(w) || y >= static_cast<int>(h) || a <= 0.f)
            return;
        uint8_t* p = at(out, x, y);
        for (int c = 0; c < 3; c++)
            p[c] = static_cast<uint8_t>(std::clamp(p[c] + (rgb[c] - p[c]) * std::min(a, 1.f), 0.f, 255.f));
    };
    // A soft shadow round the letters, so they read where they cross her chin (as the video's lift does). Not over
    // the other words' ink (Load Game's foot, moved Options' top): darkened, the video's lift would lose them.
    constexpr int kR = 6;
    std::vector<float> halo(static_cast<size_t>(cw + 2 * kR) * (ch + 2 * kR), 0.f);
    const int hw = cw + 2 * kR, hh = ch + 2 * kR;
    for (int y = 0; y < ch; y++)
        for (int x = 0; x < cw; x++)
            halo[static_cast<size_t>(y + kR) * hw + x + kR] = cover[static_cast<size_t>(y) * cw + x] / 255.f;
    blur(halo, hw, hh, 4);
    for (int y = 0; y < hh; y++)
        for (int x = 0; x < hw; x++) {
            const float m = halo[static_cast<size_t>(y) * hw + x];
            const int px = left + x - kR, py = top + y - kR;
            if (px < 0 || py < 0 || px >= static_cast<int>(w) || py >= static_cast<int>(h) || m <= 0.f)
                continue;
            put(px, py, dark, clamp01(m * 2.2f) * 0.45f * (1.f - menu_ink(at(out, px, py))));
            if (lit)  // the glow: faint pale cyan spread round the letters
                put(px, py, light, clamp01(m * 1.6f) * 0.4f);
        }
    for (int y = 0; y < ch; y++)
        for (int x = 0; x < cw; x++)
            if (const uint8_t v = cover[static_cast<size_t>(y) * cw + x])
                put(left + x, top + y, lit ? light : paint, v / 255.f * (lit ? 1.f : 0.9f));
    *ok = true;
    return out;
}

// The disc's main menu picture, repainted for the cursor (lit or not), kept; false when it isn't repainted.
bool menu_picture(const uint8_t* rgba, uint32_t w, uint32_t h, std::vector<uint8_t>* out) {
    if (g_menu_row.load() < 0 || !is_picture(w, h))
        return false;
    const bool lit = g_menu_row.load() == 5;
    std::lock_guard<std::mutex> lock(g_menu_mutex);
    Painted& p = g_menu[lit ? 1 : 0];
    if (!p.ready || p.rgba.size() != static_cast<size_t>(w) * h * 4) {
        bool ok = false;
        p.rgba = paint_menu(rgba, w, h, lit, &ok);
        p.lit = lit;
        p.ready = ok;
        if (!ok) {
            static bool told = false;
            if (!told)
                std::fprintf(stderr, "[titlevideo] the main menu's picture NOT repainted (not as measured, or no hand font)\n");
            told = true;
            return false;
        }
        g_menu_painted = true;
        if (const char* dump = std::getenv("ED_TITLE_PAINT_DUMP"); dump != nullptr && *dump != 0) {
            // (testing) the repainted picture, upright, as dump-lit.png / dump-unlit.png
            std::vector<uint8_t> up(p.rgba.size());
            for (uint32_t y = 0; y < h; y++)
                std::memcpy(&up[static_cast<size_t>(y) * w * 4], &p.rgba[static_cast<size_t>(h - 1 - y) * w * 4], w * 4);
            png_image png{};
            png.version = PNG_IMAGE_VERSION;
            png.width = w;
            png.height = h;
            png.format = PNG_FORMAT_RGBA;
            const std::string path = std::string(dump) + (lit ? "-lit.png" : "-unlit.png");
            png_image_write_to_file(&png, path.c_str(), 0, up.data(), 0, nullptr);
        }
    }
    *out = p.rgba;
    return true;
}

int options_picture(const uint8_t* rgba, uint32_t w, uint32_t h, uint8_t** out_rgba, uint32_t* out_width,
                    uint32_t* out_height) {
    const bool lit = g_options_row.load() == 8;
    std::lock_guard<std::mutex> lock(g_more_mutex);
    Painted& p = g_more[lit ? 1 : 0];
    if (!p.ready || p.rgba.size() != static_cast<size_t>(w) * h * 4) {
        p.rgba = paint_more(rgba, w, h, lit);
        p.lit = lit;
        p.ready = true;
    }
    return give(p.rgba, w, h, out_rgba, out_width, out_height);
}

int load_screen(const uint8_t* rgba, uint32_t width, uint32_t height, uint8_t** out_rgba, uint32_t* out_width,
                uint32_t* out_height) {
    uint32_t ow = width, oh = height;
    std::vector<uint8_t> out;
    if (video_on()) {
        // One still of the video (the skull is laid on her face as it is
        // there), drawn once.
        bool word, skull_kept;
        {
            std::lock_guard<std::mutex> lock(g_word_mutex);
            word = !g_word.empty();
            skull_kept = !skull().empty();
        }
        {
            std::lock_guard<std::mutex> lock(g_load_mutex);
            if (!g_load.rgba.empty() && g_load.word == word && g_load.skull == skull_kept)
                return give(g_load.rgba, g_load.width, g_load.height, out_rgba, out_width, out_height);
        }
        out = still(&ow, &oh);
        if (out.empty())
            return 0;
    } else {
        // The original with its top left (the card screen's words and frames,
        // 140-410 across, 25-245 down as seen) faded into the dark.
        out.assign(rgba, rgba + static_cast<size_t>(width) * height * 4);
        for (uint32_t y = 0; y < height; y++)
            for (uint32_t x = 0; x < width; x++) {
                const float vy = static_cast<float>(height - 1 - y);
                const float fx = std::min(smooth(120.0f, 160.0f, x), 1.0f - smooth(390.0f, 430.0f, x));
                const float fy = std::min(smooth(5.0f, 45.0f, vy), 1.0f - smooth(225.0f, 265.0f, vy));
                const float k = 1.0f - fx * fy;
                uint8_t* d = &out[(static_cast<size_t>(y) * width + x) * 4];
                for (int c = 0; c < 3; c++)
                    d[c] = static_cast<uint8_t>(d[c] * k);
            }
    }
    // Over the still, the Options screen's X-ray skull and logo, see-through
    // (screen blended: its black leaves the video be, the skull glows over
    // it). Its painted words on the left (to 430 across as seen) are left out.
    std::vector<uint8_t> overlay;
    if (video_on()) {
        std::lock_guard<std::mutex> lock(g_word_mutex);
        overlay = skull();
    }
    if (!overlay.empty()) {
        const float sx = static_cast<float>(kTitleW) / ow, sy = static_cast<float>(kTitleH) / oh;
        for (uint32_t y = 0; y < oh; y++)
            for (uint32_t x = 0; x < ow; x++) {
                // The skull laid on her face: its eye socket on her eye, a
                // little larger (as seen in 850x480).
                const float dx = (x + 0.5f) * sx - 0.5f;
                const float dvy = static_cast<float>(kTitleH - 1) - ((y + 0.5f) * sy - 0.5f);
                const float px = (dx - kHerEye[0]) / kSkullScale + kSkullEye[0];
                const float vy = (dvy - kHerEye[1]) / kSkullScale + kSkullEye[1];
                if (px < 0.0f || px > kTitleW - 1.0f || vy < 0.0f || vy > kTitleH - 1.0f)
                    continue;
                const float py = static_cast<float>(kTitleH - 1) - vy;
                float k = kSkullGlow * smooth(420.0f, 470.0f, px);
                if (k <= 0.0f)
                    continue;
                float c[3];
                sample(overlay.data(), kTitleW, kTitleH, px, py, c);
                // The logo (400-720 across, 20-190 down as seen; the video has
                // its own) goes too, by its dim teal: the skull behind it is
                // far brighter.
                const float logo = std::min(1.0f - smooth(700.0f, 730.0f, px), 1.0f - smooth(185.0f, 200.0f, vy));
                // Where its thin lettering crosses the crown, the crown's
                // brightest just above or below is taken instead.
                if (logo > 0.0f) {
                    float m[3] = {c[0], c[1], c[2]};
                    for (int oy = -10; oy <= 10; oy += 2) {
                        float n[3];
                        sample(overlay.data(), kTitleW, kTitleH, px, py + oy, n);
                        if (n[0] + n[1] + n[2] > m[0] + m[1] + m[2])
                            std::copy(n, n + 3, m);
                    }
                    for (int i = 0; i < 3; i++)
                        c[i] += (m[i] - c[i]) * logo;
                    k *= 1.0f - logo * (1.0f - smooth(120.0f, 170.0f, (c[0] + c[1] + c[2]) / 3.0f));
                }
                uint8_t* d = &out[(static_cast<size_t>(y) * ow + x) * 4];
                for (int i = 0; i < 3; i++) {
                    const float a = d[i] / 255.0f, b = c[i] / 255.0f * k;
                    d[i] = static_cast<uint8_t>(255.0f * (1.0f - (1.0f - a) * (1.0f - b)));
                }
            }
    } else {
        // Without the video, a see-through black panel behind the saves (their
        // pictures, chapter and time: 195-655 across, 255-420 down as seen).
        const float sx = static_cast<float>(ow) / width, sy = static_cast<float>(oh) / height;
        for (uint32_t y = 0; y < oh; y++) {
            const float vy = (oh - 1 - y) / sy;
            const float fy = std::min(smooth(245.0f, 265.0f, vy), 1.0f - smooth(410.0f, 430.0f, vy));
            if (fy <= 0.0f)
                continue;
            for (uint32_t x = 0; x < ow; x++) {
                const float vx = x / sx;
                const float f = fy * std::min(smooth(185.0f, 205.0f, vx), 1.0f - smooth(645.0f, 665.0f, vx));
                uint8_t* d = &out[(static_cast<size_t>(y) * ow + x) * 4];
                for (int c = 0; c < 3; c++)
                    d[c] = static_cast<uint8_t>(d[c] * (1.0f - 0.55f * f));
            }
        }
    }
    // "Load" where "Memory Card" was: 1.6 times its size, its top left at
    // (215, 28) as seen in the 850x480 picture; lit (the brighter wins).
    std::vector<uint8_t> word;
    {
        std::lock_guard<std::mutex> lock(g_word_mutex);
        word = g_word;
    }
    if (!word.empty()) {
        const float sx = static_cast<float>(ow) / width, sy = static_cast<float>(oh) / height;
        const float scale = 1.6f, left = 215.0f, top = 28.0f;
        const uint32_t x0 = static_cast<uint32_t>(left * sx), x1 = static_cast<uint32_t>((left + kLoadWordEnd * scale) * sx);
        const uint32_t y0 = static_cast<uint32_t>(top * sy), y1 = static_cast<uint32_t>((top + kWordH * scale) * sy);
        for (uint32_t vy = y0; vy < y1 && vy < oh; vy++)
            for (uint32_t x = x0; x < x1 && x < ow; x++) {
                // The word's own point (as seen), then as stored.
                const float wx = (x / sx - left) / scale, wvy = (vy / sy - top) / scale;
                float c[3];
                sample(word.data(), kWordW, kWordH, wx, static_cast<float>(kWordH - 1) - wvy, c);
                uint8_t* d = &out[(static_cast<size_t>(oh - 1 - vy) * ow + x) * 4];
                for (int k = 0; k < 3; k++)
                    d[k] = static_cast<uint8_t>(std::max(static_cast<float>(d[k]), std::min(255.0f, c[k])));
            }
    }
    if (video_on()) {
        std::lock_guard<std::mutex> lock(g_load_mutex);
        g_load.rgba = out;
        g_load.width = ow;
        g_load.height = oh;
        g_load.word = !word.empty();
        g_load.skull = !overlay.empty();
    }
    return give(out, ow, oh, out_rgba, out_width, out_height);
}

int title_picture(const uint8_t* rgba, uint32_t width, uint32_t height, uint8_t** out_rgba, uint32_t* out_width,
                  uint32_t* out_height);

}  // namespace

bool titlevideo_claims(uint32_t format, uint32_t width, uint32_t height) {
    return format == kCmpr && (is_picture(width, height) || (width == kWordW && height == kWordH) ||
                               hidden(width, height, 0u) != nullptr);
}

bool titlevideo_claims_hash(uint32_t width, uint32_t height, uint64_t hash) {
    if (is_picture(width, height))
        // The main menu's picture: always while its Choose Character row is in use (English), else with the video.
        return hash == kTitleHash ? video_on() || g_menu_row.load() >= 0 : hash == kLoadScreenHash || hash == kOptionsHash;
    if (width == kWordW && height == kWordH)
        return hash == kLoadWordHash;
    return hidden(width, height, hash) != nullptr;
}

uint64_t titlevideo_generation(uint32_t width, uint32_t height) {
    if (!is_picture(width, height))
        return 1u;
    // One word for every 850x480 picture: the Options picture is redrawn when its ninth row is lit or not, the main
    // menu's when its Choose Character row comes into use and when it is lit or not (and the video's frames change the
    // title's).
    const int row = g_menu_row.load();
    const uint64_t key = (g_options_row.load() == 8 ? 2u : 0u) + (row >= 0 ? 4u : 0u) + (row == 5 ? 1u : 0u);
    return video_on() ? g.serial.load() * 256u + key : kOptionsMoreSerial | key;
}

int titlevideo_apply(const uint8_t* rgba, uint32_t width, uint32_t height, uint64_t hash, uint8_t** out_rgba,
                     uint32_t* out_width, uint32_t* out_height) {
    if (hidden(width, height, hash) != nullptr) {
        std::vector<uint8_t> clear(static_cast<size_t>(width) * height * 4, 0u);
        return give(clear, width, height, out_rgba, out_width, out_height);
    }
    if (width == kWordW && height == kWordH) {
        if (hash == kLoadWordHash) {
            std::lock_guard<std::mutex> lock(g_word_mutex);
            g_word.assign(rgba, rgba + static_cast<size_t>(width) * height * 4);
        }
        return 0;  // kept as it is
    }
    if (hash == kLoadScreenHash)
        return load_screen(rgba, width, height, out_rgba, out_width, out_height);
    if (hash == kOptionsHash) {
        keep_skull(rgba);
        return options_picture(rgba, width, height, out_rgba, out_width, out_height);
    }
    return hash == kTitleHash ? title_picture(rgba, width, height, out_rgba, out_width, out_height) : 0;
}

namespace {

int title_picture(const uint8_t* rgba, uint32_t width, uint32_t height, uint8_t** out_rgba, uint32_t* out_width,
                  uint32_t* out_height) {
    // The main menu's own rows first (Choose Character): the video lifts its words from that.
    std::vector<uint8_t> menu;
    if (menu_picture(rgba, width, height, &menu))
        rgba = menu.data();
    if (!video_on())
        return menu.empty() ? 0 : give(menu, width, height, out_rgba, out_width, out_height);
    g.asked_ms = now_ms();
    std::vector<uint8_t> out;
    uint32_t ow, oh;
    out = video_frame(&ow, &oh);
    if (out.empty())  // the first frame on its way: the picture meanwhile
        return menu.empty() ? 2 : give(menu, width, height, out_rgba, out_width, out_height);
    // The painted text over the video: the menu's words by their pale teal
    // (brightness 60-110, blue over red by 20-40; the grey face beside them
    // 40-60 and 10-17), the copyright line by its brightness. The video's
    // face lies under the words, where the original has dark hair: a soft
    // shadow round them keeps them as legible.
    for (const Box& box : kText) {
        const uint32_t x0 = box.x0 * ow / width, x1 = std::min(box.x1 * ow / width, ow);
        const uint32_t y0 = box.y0 * oh / height, y1 = std::min(box.y1 * oh / height, oh);
        const uint32_t bw = x1 - x0, bh = y1 - y0;
        // The original filtered up to the frame's size, and how much of each
        // point is text: soft edges (smoothstep), then smoothed once more so
        // the compression's blocks do not show as grit.
        std::vector<float> colour(static_cast<size_t>(bw) * bh * 3), mask(static_cast<size_t>(bw) * bh);
        const float sx = static_cast<float>(width) / ow, sy = static_cast<float>(height) / oh;
        for (uint32_t y = 0; y < bh; y++)
            for (uint32_t x = 0; x < bw; x++) {
                float* c = &colour[(y * bw + x) * 3];
                sample(rgba, width, height, (x0 + x + 0.5f) * sx - 0.5f, (y0 + y + 0.5f) * sy - 0.5f, c);
                const float lum = 0.3f * c[0] + 0.59f * c[1] + 0.11f * c[2];
                mask[y * bw + x] = box.words ? smooth(45.0f, 80.0f, lum) * smooth(12.0f, 24.0f, c[2] - c[0])
                                             : smooth(70.0f, 140.0f, lum);
            }
        blur(mask, bw, bh, 1);
        // The shadow: the mask spread wide.
        std::vector<float> spread = mask;
        blur(spread, bw, bh, 6);
        for (uint32_t y = 0; y < bh; y++)
            for (uint32_t x = 0; x < bw; x++) {
                const float* c = &colour[(y * bw + x) * 3];
                // The smoothing thins the strokes: as solid and bright as the
                // words the game draws beside them.
                const float a = clamp01(mask[y * bw + x] * 1.35f);
                const float shade = 1.0f - clamp01(spread[y * bw + x] * 2.5f) * 0.7f;
                uint8_t* d = &out[(static_cast<size_t>(y0 + y) * ow + x0 + x) * 4];
                for (int k = 0; k < 3; k++)
                    d[k] = static_cast<uint8_t>(std::min(255.0f, c[k] * 1.15f * a + d[k] * shade * (1.0f - a)));
            }
    }
    return give(out, ow, oh, out_rgba, out_width, out_height);
}

}  // namespace

// The title Options' cursor row as its frame draws (hle.c, the game thread): row 8 lights "More Options".
extern "C" void titlevideo_options_row(int row) { g_options_row = row; }
// The main menu's cursor as its frame draws (hle.c, the game thread), -1 not English: row 5 lights "Choose Character".
extern "C" void titlevideo_menu_row(int row) { g_menu_row = row; }
// The main menu's picture has been repainted with Choose Character (hle.c adds the row only then).
extern "C" int titlevideo_menu_painted(void) { return g_menu_painted.load() ? 1 : 0; }
