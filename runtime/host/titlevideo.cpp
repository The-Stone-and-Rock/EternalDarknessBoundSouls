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
#include "host/texpack.h"
#include "host/tome.h"

#include <png.h>

#define PL_MPEG_IMPLEMENTATION
#include "third_party/pl_mpeg.h"

#include <gxruntime/gxcore/texture_decode.hpp>
#include <xxhash.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
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

// The title Options as one list of 14 rows that scrolls (hle.c title_more_*, docs/research/in-game-menu/
// 09-title-options-scroll.md): the game's 8 painted rows, then Display, Sound, Graphics, Controls, Mods and Quit game.
// Nine show at a time, in the nine slots (the painted rows' baselines, then one more under Language Select). For each
// (scroll s, lit row) the picture is painted again: the painted words erased (their ground is black), each of the
// game's rows still shown copied up to its slot (the game moves its glow and value words the same, by a shifted copy of
// its quads), ours painted in the Tome's hand at theirs, lit pale cyan with a soft glow while chosen (the game draws no
// glow past row 7), and a small mark beside the top or bottom slot while rows are hidden there. Measured on the kept
// picture (upright 850x480): the words' ink is the only thing brighter than 6 in x 140..440, y 60..470 (less the logo,
// x 400.. above y 200); it makes 52 pieces that each sit clearly on one row.
constexpr int kOptionRows = 14, kGameRows = 8, kSlots = 9, kScrollMax = kOptionRows - kSlots;
// The game's rows' baselines as painted (39-48 apart), and the slots' (upright): evenly spaced from the first row's to the
// last's, so a row moved up keeps the same room above and below (hle.c kTitleRowY, kTitleSlotY: the same).
constexpr int kRowY[kGameRows] = {111, 150, 188, 236, 276, 323, 370, 406};
constexpr int kSlotY[kSlots] = {111, 153, 195, 237, 280, 322, 364, 406, 448};
const char* const kOurRows[kOptionRows - kGameRows] = {"Display", "Sound", "Graphics", "Controls", "Mods", "Quit game"};
// The cursor (bits 0-7), scroll (8-15) and value words shown (16-20, hle.c title_more_draw) as the frame last drew,
// -1 not in use (hle.c).
std::atomic<int> g_options_key{-1};
std::atomic<int> g_options_asked{-1};  // the same, as the renderer last asked the generation (its paint uses this)
std::atomic<bool> g_options_ready{false};
constexpr uint64_t kOptionsMoreSerial = 0x4D4F52450000ull;  // the 850x480 pictures' generations without the video
struct Painted {
    std::vector<uint8_t> rgba;
    bool lit = false, ready = false;
};
std::mutex g_more_mutex;
// Which row each point of the words' box belongs to (its ink grown 3 pixels), -1 none: made once.
constexpr int kBoxX0 = 140, kBoxX1 = 441, kBoxY0 = 60, kBoxY1 = 471;
constexpr int kBoxW = kBoxX1 - kBoxX0, kBoxH = kBoxY1 - kBoxY0;
std::vector<int8_t> g_row_of;
bool g_rows_tried = false;
struct OptionsPaint {
    int key;
    std::vector<uint8_t> rgba;
};
std::vector<OptionsPaint> g_options_paints;  // the last few painted, newest last

float lum_of(const uint8_t* p) { return 0.3f * p[0] + 0.59f * p[1] + 0.11f * p[2]; }

// The rows' pieces of ink (under g_more_mutex); false if the picture isn't the one measured.
bool find_rows(const uint8_t* rgba, uint32_t w, uint32_t h) {
    if (g_rows_tried)
        return !g_row_of.empty();
    g_rows_tried = true;
    auto src = [&](int x, int y) { return &rgba[(static_cast<size_t>(h - 1 - y) * w + x) * 4]; };
    std::vector<uint8_t> ink(static_cast<size_t>(kBoxW) * kBoxH, 0);
    for (int y = 0; y < kBoxH; y++)
        for (int x = 0; x < kBoxW; x++) {
            const int px = kBoxX0 + x, py = kBoxY0 + y;
            if (px >= 400 && py < 200)
                continue;  // the logo
            ink[static_cast<size_t>(y) * kBoxW + x] = lum_of(src(px, py)) > 6.f;
        }
    // Pieces (8-connected). Each letter's piece goes to the row whose baseline - 8 is nearest its middle. A dot (of an i
    // or j) or a fleck goes to the row of the nearest letter: the j of "Volume Adjustment" has its dot nearer Rumble
    // Feature's baseline than its own.
    std::vector<int> piece(ink.size(), -1);
    std::vector<int8_t> row_of(ink.size(), -1);
    std::vector<int> stack;
    std::vector<std::vector<int>> all;
    int per_row[kGameRows] = {}, closest = 1 << 20;
    constexpr size_t kDot = 20;  // points: smaller is a dot
    for (size_t start = 0; start < ink.size(); start++) {
        if (!ink[start] || piece[start] >= 0)
            continue;
        std::vector<int> cells;
        stack.assign(1, static_cast<int>(start));
        piece[start] = static_cast<int>(all.size());
        int y0 = kBoxH, y1 = -1;
        while (!stack.empty()) {
            const int i = stack.back();
            stack.pop_back();
            cells.push_back(i);
            const int x = i % kBoxW, y = i / kBoxW;
            y0 = std::min(y0, y);
            y1 = std::max(y1, y);
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    const int nx = x + dx, ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= kBoxW || ny >= kBoxH)
                        continue;
                    const int n = ny * kBoxW + nx;
                    if (ink[n] && piece[n] < 0) {
                        piece[n] = static_cast<int>(all.size());
                        stack.push_back(n);
                    }
                }
        }
        if (cells.size() >= kDot) {
            const float mid = kBoxY0 + (y0 + y1) / 2.f;
            int best = 0;
            float d1 = 1e9f, d2 = 1e9f;
            for (int r = 0; r < kGameRows; r++) {
                const float d = std::fabs(mid - (kRowY[r] - 8));
                if (d < d1) {
                    d2 = d1;
                    d1 = d;
                    best = r;
                } else if (d < d2) {
                    d2 = d;
                }
            }
            closest = std::min(closest, static_cast<int>(d2 - d1));
            per_row[best] += static_cast<int>(cells.size());
            for (int i : cells)
                row_of[i] = static_cast<int8_t>(best);
        }
        all.push_back(std::move(cells));
    }
    const int pieces = static_cast<int>(all.size());
    int dots = 0, dots_alone = 0;
    for (const std::vector<int>& cells : all) {
        if (cells.size() >= kDot)
            continue;
        dots++;
        const int x = cells[0] % kBoxW, y = cells[0] / kBoxW;
        int best = -1, bd = 15 * 15 + 1;
        for (int dy = -15; dy <= 15; dy++)
            for (int dx = -15; dx <= 15; dx++) {
                const int nx = x + dx, ny = y + dy, d = dx * dx + dy * dy;
                if (nx < 0 || ny < 0 || nx >= kBoxW || ny >= kBoxH || d >= bd)
                    continue;
                const size_t n = static_cast<size_t>(ny) * kBoxW + nx;
                if (row_of[n] >= 0 && all[piece[n]].size() >= kDot) {
                    bd = d;
                    best = row_of[n];
                }
            }
        if (best < 0) {  // a fleck by itself: the row whose baseline - 8 is nearest
            dots_alone++;
            best = 0;
            for (int r = 1; r < kGameRows; r++)
                if (std::abs(kBoxY0 + y - (kRowY[r] - 8)) < std::abs(kBoxY0 + y - (kRowY[best] - 8)))
                    best = r;
        }
        for (int i : cells)
            row_of[i] = static_cast<int8_t>(best);
    }
    // Grown 3 pixels (the letters' soft edges), the nearer row winning.
    std::vector<int8_t> grown(ink.size(), -1);
    std::vector<int> dist(ink.size(), 1 << 20);
    for (int y = 0; y < kBoxH; y++)
        for (int x = 0; x < kBoxW; x++) {
            const int8_t r = row_of[static_cast<size_t>(y) * kBoxW + x];
            if (r < 0)
                continue;
            for (int dy = -3; dy <= 3; dy++)
                for (int dx = -3; dx <= 3; dx++) {
                    const int nx = x + dx, ny = y + dy, d = dx * dx + dy * dy;
                    if (nx < 0 || ny < 0 || nx >= kBoxW || ny >= kBoxH || d > 10)
                        continue;
                    const size_t n = static_cast<size_t>(ny) * kBoxW + nx;
                    if (d < dist[n]) {
                        dist[n] = d;
                        grown[n] = r;
                    }
                }
        }
    bool ok = pieces >= 30 && pieces <= 90 && closest >= 4;
    for (int r = 0; r < kGameRows; r++)
        ok = ok && per_row[r] >= 200;
    std::fprintf(stderr, "[titlevideo] the Options picture's words: %d pieces (%d dots, %d of them far from a letter), "
                 "rows of %d %d %d %d %d %d %d %d points, the closest second row %d px off: %s\n", pieces, dots, dots_alone,
                 per_row[0], per_row[1], per_row[2], per_row[3], per_row[4], per_row[5], per_row[6], per_row[7], closest,
                 ok ? "the list is painted" : "NOT as measured, left alone");
    if (ok)
        g_row_of = std::move(grown);
    return ok;
}

// 1b of paint_options. A row moved up into the logo (Rumble's On / Off, Audio Mode's Stereo Surround, Brightness
// Adjustment's end) would cross its letters: the logo is darkened softly round the row's words and the game's value words
// there. The shade over the logo's box (kShadeX0.., 1x upright), false when nothing is moved there.
constexpr int kShadeX0 = 360, kShadeX1 = 640, kShadeY0 = 30, kShadeY1 = 240;
bool options_shade(int s, int values, std::vector<float>* out) {
    {
        // The value words' quads, the one the game draws (menu x0, x1, y0, y1; the picture is menu x + 106): 09
        // section 1.2. values: bit 0 Subtitles on, 1 Widescreen 16:9, 2 Rumble off, 3-4 Audio Mode.
        struct Rect {
            int row, x0, x1, y0, y1;
        };
        const int audio = std::min((values >> 3) & 3, 2);
        constexpr Rect kAudio[3] = {{6, 221, 292, 342, 382}, {6, 217, 295, 338, 385}, {6, 217, 413, 335, 386}};
        const Rect kValues[] = {(values & 1) ? Rect{1, 187, 228, 124, 159} : Rect{1, 189, 245, 106, 164},
                                (values & 2) ? Rect{2, 208, 255, 159, 198} : Rect{2, 208, 328, 160, 197},
                                (values & 4) ? Rect{3, 274, 330, 190, 248} : Rect{3, 273, 314, 209, 244}, kAudio[audio]};
        constexpr int kX0 = kShadeX0, kX1 = kShadeX1, kY0 = kShadeY0, kY1 = kShadeY1, kW = kX1 - kX0, kH = kY1 - kY0;
        std::vector<float> shade(static_cast<size_t>(kW) * kH, 0.f);
        bool any = false;
        auto mark = [&](int x, int y) {
            if (x >= kX0 && x < kX1 && y >= kY0 && y < kY1) {
                shade[static_cast<size_t>(y - kY0) * kW + x - kX0] = 1.f;
                any = true;
            }
        };
        for (int y = 0; y < kBoxH; y++)
            for (int x = 0; x < kBoxW; x++) {
                const int r = g_row_of[static_cast<size_t>(y) * kBoxW + x];
                if (r >= s && s > 0)  // (at the top, as the game painted it: nothing moved into the logo)
                    mark(kBoxX0 + x, kBoxY0 + y + kSlotY[r - s] - kRowY[r]);
            }
        for (const Rect& v : kValues) {
            if (v.row < s || s == 0)
                continue;
            const int dy = kSlotY[v.row - s] - kRowY[v.row];
            for (int y = v.y0 + 4; y < v.y1 - 4; y++)
                for (int x = v.x0 + 106 + 2; x < v.x1 + 106 - 2; x++)
                    mark(x, y + dy);
        }
        if (!any)
            return false;
        blur(shade, kW, kH, 5);
        blur(shade, kW, kH, 5);
        *out = std::move(shade);
        return true;
    }
}

// The picture for one (lit row, scroll): from the stored original, upside down as it is.
std::vector<uint8_t> paint_options(const uint8_t* rgba, uint32_t w, uint32_t h, int lit, int s, int values) {
    std::vector<uint8_t> out(rgba, rgba + static_cast<size_t>(w) * h * 4);
    auto at = [&](int x, int y) { return &out[(static_cast<size_t>(h - 1 - y) * w + x) * 4]; };
    auto src = [&](int x, int y) { return &rgba[(static_cast<size_t>(h - 1 - y) * w + x) * 4]; };
    // 1. The painted words erased.
    for (int y = 0; y < kBoxH; y++)
        for (int x = 0; x < kBoxW; x++)
            if (g_row_of[static_cast<size_t>(y) * kBoxW + x] >= 0) {
                uint8_t* d = at(kBoxX0 + x, kBoxY0 + y);
                d[0] = d[1] = d[2] = 0;
            }
    // 1b. The logo shaded round a row moved up into it (options_shade).
    {
        constexpr int kX0 = kShadeX0, kY0 = kShadeY0, kW = kShadeX1 - kShadeX0, kH = kShadeY1 - kShadeY0;
        std::vector<float> shade;
        if (options_shade(s, values, &shade))
            for (int y = 0; y < kH; y++)
                for (int x = 0; x < kW; x++) {
                    const float k = 1.f - 0.85f * clamp01(shade[static_cast<size_t>(y) * kW + x] * 2.5f);
                    uint8_t* d = at(kX0 + x, kY0 + y);
                    for (int c = 0; c < 3; c++)
                        d[c] = static_cast<uint8_t>(d[c] * k);
                }
    }
    // 2. The game's rows still shown, each at its slot (the brighter wins: the ground is black).
    for (int y = 0; y < kBoxH; y++)
        for (int x = 0; x < kBoxW; x++) {
            const int r = g_row_of[static_cast<size_t>(y) * kBoxW + x];
            if (r < s)
                continue;
            const int ty = kBoxY0 + y + kSlotY[r - s] - kRowY[r];
            if (ty < 0 || ty >= static_cast<int>(h))
                continue;
            const uint8_t* p = src(kBoxX0 + x, kBoxY0 + y);
            uint8_t* d = at(kBoxX0 + x, ty);
            for (int c = 0; c < 3; c++)
                d[c] = std::max(d[c], p[c]);
        }
    auto put = [&](int x, int y, const float rgb[3], float a) {
        if (x < 0 || y < 0 || x >= static_cast<int>(w) || y >= static_cast<int>(h) || a <= 0.f)
            return;
        uint8_t* p = at(x, y);
        for (int c = 0; c < 3; c++)
            p[c] = static_cast<uint8_t>(std::clamp(p[c] + (rgb[c] - p[c]) * std::min(a, 1.f), 0.f, 255.f));
    };
    const float ink[3] = {68.f, 92.f, 98.f}, light[3] = {0xA0, 0xE3, 0xF1};
    // 3. Ours in their slots, as large as the game's (09 section 6: 52 px matches its width and x-height).
    constexpr float kPx = 52.f;
    for (int j = 0; j < kOptionRows - kGameRows; j++) {
        const int slot = kGameRows + j - s;
        if (slot < 0 || slot >= kSlots)
            continue;
        std::vector<uint8_t> cover;
        int cw = 0, ch = 0, base = 0;
        if (!tome_hand_cover(kOurRows[j], kPx, 0.8f, &cover, &cw, &ch, &base))
            continue;
        const bool on = lit == kGameRows + j;
        const int left = 174 - static_cast<int>(kPx / 4.f), top = kSlotY[slot] - base;
        if (on) {  // the glow: the letters spread 6 pixels, faint
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
                    put(left + x, top + y, light, m * 0.35f);
                }
        }
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw; x++)
                if (const uint8_t v = cover[static_cast<size_t>(y) * cw + x])
                    put(left + x, top + y, on ? light : ink, v / 255.f * (on ? 1.f : 0.9f));
    }
    // 4. The marks: a small pen stroke chevron left of the top slot while rows are hidden above, of the bottom slot
    // while rows are hidden below (soft edges, the rows' ink a little brighter).
    auto chevron = [&](int cx, int cy, int dir) {  // dir -1 points up, +1 down
        constexpr float kHalf = 7.f, kTall = 5.f, kWidth = 1.6f;
        const float tip[2] = {static_cast<float>(cx), cy + dir * kTall};
        const float ends[2][2] = {{cx - kHalf, cy - dir * kTall}, {cx + kHalf, cy - dir * kTall}};
        const float mark[3] = {88.f, 118.f, 126.f};
        for (int y = cy - 10; y <= cy + 10; y++)
            for (int x = cx - 11; x <= cx + 11; x++) {
                float best = 1e9f;
                for (const auto& e : ends) {  // distance to the stroke from the end to the tip
                    const float vx = tip[0] - e[0], vy = tip[1] - e[1];
                    const float t = std::clamp(((x - e[0]) * vx + (y - e[1]) * vy) / (vx * vx + vy * vy), 0.f, 1.f);
                    const float dx = x - (e[0] + vx * t), dy = y - (e[1] + vy * t);
                    best = std::min(best, std::sqrt(dx * dx + dy * dy));
                }
                put(x, y, mark, clamp01(kWidth + 0.5f - best));
            }
    };
    if (s > 0)
        chevron(156, kSlotY[0] - 12, -1);
    if (s < kScrollMax)
        chevron(156, kSlotY[kSlots - 1] - 10, 1);
    return out;
}

// The Options picture for the row and scroll the renderer asked the generation for, kept for a few; false if not painted.
bool options_list(const uint8_t* rgba, uint32_t w, uint32_t h, std::vector<uint8_t>* out) {
    std::lock_guard<std::mutex> lock(g_more_mutex);
    if (!is_picture(w, h) || !find_rows(rgba, w, h))
        return false;
    g_options_ready = true;
    const int asked = g_options_asked.load();
    const int row = asked < 0 ? 0 : asked & 0xFF, s = asked < 0 ? 0 : std::clamp((asked >> 8) & 0xFF, 0, kScrollMax);
    const int values = asked < 0 ? 0 : (asked >> 16) & 0x1F;
    const int key = values * 256 + row * 16 + s;
    for (size_t i = 0; i < g_options_paints.size(); i++)
        if (g_options_paints[i].key == key) {
            *out = g_options_paints[i].rgba;
            return true;
        }
    if (g_options_paints.size() >= 8)
        g_options_paints.erase(g_options_paints.begin());
    g_options_paints.push_back({key, paint_options(rgba, w, h, row, s, values)});
    *out = g_options_paints.back().rgba;
    if (const char* dump = std::getenv("ED_TITLE_PAINT_DUMP"); dump != nullptr && *dump != 0) {
        // (testing) each repainted list, upright, as PATH-options-ROW-SCROLL.png
        std::vector<uint8_t> up(out->size());
        for (uint32_t y = 0; y < h; y++)
            std::memcpy(&up[static_cast<size_t>(y) * w * 4], &(*out)[static_cast<size_t>(h - 1 - y) * w * 4], w * 4);
        png_image png{};
        png.version = PNG_IMAGE_VERSION;
        png.width = w;
        png.height = h;
        png.format = PNG_FORMAT_RGBA;
        const std::string path = std::string(dump) + "-options-" + std::to_string(row) + "-" + std::to_string(s) + ".png";
        png_image_write_to_file(&png, path.c_str(), 0, up.data(), 0, nullptr);
    }
    return true;
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

// words false: only steps 1-3 (Options moved), for the HD paint's measures.
std::vector<uint8_t> paint_menu(const uint8_t* rgba, uint32_t w, uint32_t h, bool lit, bool* ok, bool words = true) {
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
    if (!words) {
        *ok = true;
        return out;
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

// ---- The HD pack's pictures ----
// With HD textures on and the pack holding the title's or Options' 850x480 picture at a whole multiple (the pack's are
// 3400x1920, 4x), the claimed pictures are painted on the pack's instead (docs/research/firstrun-hd-flash/hdtitle.md):
// measured on the disc's picture at 1x, as the paints above, and painted at the HD scale k. Each pack picture is read
// and its BC7 decoded once, on a worker from boot (titlevideo_preload); kept as stored (upside down, as the disc's).
// Until a paint is ready the 1x paint is the stand-in; the generation's HD bits then make the renderer ask again.
struct HdPicture {
    std::vector<uint8_t> rgba;  // stored rows, w x h
    uint32_t w = 0, h = 0;
    int k = 0;
};
// The largest scale used: the title's whole pack size (painted once, given on each lit swap from one copy); Options at
// most 2x (1700x960), as it is painted again for each key press on the renderer's thread.
constexpr int kTitleHdMax = 4, kOptionsHdMax = 2;
// The hand's thickening at HD, per k: the 1x paints' 0.8 makes strokes about 10 HD pixels at 4x, where the HD picture's
// own pen words are about 6 (seen in run -130147's dump); the font's own stroke is about 4 there.
constexpr float kHdBold = 0.25f;
// A part of the HD picture as painted, where it differs from the pack's.
struct HdPatch {
    uint32_t x0 = 0, y0 = 0, w = 0, h = 0;  // stored rows
    std::vector<uint8_t> rgba;
};
struct OptionsCover {
    std::vector<uint8_t> hd, x1;  // the hand at k and at 1x
    int hw = 0, hh = 0, hbase = 0, w1 = 0, h1 = 0, base1 = 0;
};
struct OptionsHdPaint {
    int key;
    std::vector<uint8_t> rgba;
};
// Made once and never destroyed (as texpack.cpp's Shared): detached boot workers use them, and the launcher can be
// closed while one runs (titlevideo_shutdown waits for them too).
std::mutex& g_hd_mutex = *new std::mutex;  // all of g_hd
struct HdState {
    int title_state = 0, options_state = 0;  // 0 not read, 1 being read, 2 read (the picture set if the pack has it)
    std::shared_ptr<const HdPicture> title, options;
    std::vector<uint8_t> title_disc;  // the disc's title picture, from the renderer's first ask
    bool title_painting = false;
    HdPatch title_patch[2];  // unlit, lit (empty: the pack's picture as it is)
    bool options_prepping = false;
    std::shared_ptr<const HdPicture> options_clean;  // its words' box blacked
    OptionsCover covers[kOptionRows - kGameRows];
    float options_ink[3] = {68.f, 92.f, 98.f};
    std::vector<OptionsHdPaint> options_paints;  // the last few, newest last
};
HdState& g_hd = *new HdState;
std::atomic<bool> g_hd_title_ready{false}, g_hd_options_ready{false};
std::atomic<bool> g_hd_art_ready{false};  // res/video/options-clean-hd.png written or up to date this run
std::atomic<int> g_hd_workers{0};  // HD workers running (titlevideo_shutdown waits for them)

// A detached worker on the HD pictures, counted: it also uses this file's other statics and texpack's, so the exit
// waits for it before they are destroyed.
template <class F>
void hd_thread(F f) {
    g_hd_workers++;
    std::thread([f]() mutable {
        f();
        g_hd_workers--;
    }).detach();
}

int64_t ms_since(int64_t t0) { return now_ms() - t0; }

// by x by boxes averaged: a pack picture at a smaller whole scale.
HdPicture shrink(const std::vector<uint8_t>& rgba, uint32_t w, uint32_t h, int by) {
    HdPicture out;
    out.w = w / by;
    out.h = h / by;
    out.rgba.assign(static_cast<size_t>(out.w) * out.h * 4, 0);
    for (uint32_t y = 0; y < out.h; y++)
        for (uint32_t x = 0; x < out.w; x++)
            for (int c = 0; c < 4; c++) {
                uint32_t sum = 0;
                for (int dy = 0; dy < by; dy++)
                    for (int dx = 0; dx < by; dx++)
                        sum += rgba[((static_cast<size_t>(y) * by + dy) * w + static_cast<size_t>(x) * by + dx) * 4 + c];
                out.rgba[(static_cast<size_t>(y) * out.w + x) * 4 + c] = static_cast<uint8_t>((sum + by * by / 2) / (by * by));
            }
    return out;
}

// The pack's picture for hash at a whole scale k >= 2 (made smaller to at most kmax), else null.
std::shared_ptr<const HdPicture> hd_read(uint64_t hash, int kmax, const char* name) {
    const int64_t t0 = now_ms();
    std::vector<uint8_t> rgba;
    uint32_t w = 0, h = 0;
    if (!texpack_read_rgba(kCmpr, kTitleW, kTitleH, hash, &rgba, &w, &h)) {
        std::fprintf(stderr, "[titlevideo] HD %s: not in the pack (or HD textures off): the disc's picture\n", name);
        return nullptr;
    }
    const int k = static_cast<int>(w / kTitleW);
    if (k < 2 || w != kTitleW * k || h != kTitleH * k || rgba.size() != static_cast<size_t>(w) * h * 4) {
        std::fprintf(stderr, "[titlevideo] HD %s: the pack's is %ux%u, not a whole multiple of 850x480: the disc's\n", name, w,
                     h);
        return nullptr;
    }
    auto pic = std::make_shared<HdPicture>();
    int by = 1;
    while (k / by > kmax && (k / by) % 2 == 0)
        by *= 2;
    if (by > 1) {
        *pic = shrink(rgba, w, h, by);
    } else {
        pic->rgba = std::move(rgba);
        pic->w = w;
        pic->h = h;
    }
    pic->k = k / by;
    std::fprintf(stderr, "[titlevideo] HD %s: the pack's %ux%u read in %lld ms, used at %ux%u (x%d)\n", name, w, h,
                 static_cast<long long>(ms_since(t0)), pic->w, pic->h, pic->k);
    return pic;
}

// A field fw x fh at 1x (its first point at 1x upright (fx0, fy0)), sampled bilinearly at an HD point (X, Y).
float upsample(const std::vector<float>& f, int fw, int fh, int fx0, int fy0, int k, int X, int Y) {
    const float u = std::clamp((X + 0.5f) / k - 0.5f - fx0, 0.f, static_cast<float>(fw - 1));
    const float v = std::clamp((Y + 0.5f) / k - 0.5f - fy0, 0.f, static_cast<float>(fh - 1));
    const int x0 = static_cast<int>(u), y0 = static_cast<int>(v);
    const int x1 = std::min(x0 + 1, fw - 1), y1 = std::min(y0 + 1, fh - 1);
    const float tx = u - x0, ty = v - y0;
    const float a = f[static_cast<size_t>(y0) * fw + x0] * (1 - tx) + f[static_cast<size_t>(y0) * fw + x1] * tx;
    const float b = f[static_cast<size_t>(y1) * fw + x0] * (1 - tx) + f[static_cast<size_t>(y1) * fw + x1] * tx;
    return a * (1 - ty) + b * ty;
}

// Each point the most of it and its 8 neighbours (the field grown by one 1x pixel).
void grow1(std::vector<float>& f, int w, int h) {
    std::vector<float> g(f.size(), 0.f);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float m = 0.f;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    const int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && ny >= 0 && nx < w && ny < h)
                        m = std::max(m, f[static_cast<size_t>(ny) * w + nx]);
                }
            g[static_cast<size_t>(y) * w + x] = m;
        }
    f = std::move(g);
}

// The painted words' colour, to match ours to them on the HD picture (its words are greyer than the disc's,
// hdtitle.md 1.7): the median of the brighter half of the points the measure calls ink.
void median_bright(std::vector<std::array<uint8_t, 3>>& pts, float out[3]) {
    if (pts.empty())
        return;
    std::sort(pts.begin(), pts.end(), [](const std::array<uint8_t, 3>& a, const std::array<uint8_t, 3>& b) {
        return 0.3f * a[0] + 0.59f * a[1] + 0.11f * a[2] < 0.3f * b[0] + 0.59f * b[1] + 0.11f * b[2];
    });
    const size_t from = pts.size() / 2;
    for (int c = 0; c < 3; c++) {
        std::vector<uint8_t> v;
        for (size_t i = from; i < pts.size(); i++)
            v.push_back(pts[i][c]);
        std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
        out[c] = v[v.size() / 2];
    }
}
// A colour tuned on the disc moved by the HD words' colour over the disc words' (each channel, within 0.6..1.6).
void match_ink(const float disc[3], const float hd[3], const float tuned[3], float out[3]) {
    for (int c = 0; c < 3; c++)
        out[c] = std::clamp(tuned[c] * std::clamp(disc[c] > 1.f ? hd[c] / disc[c] : 1.f, 0.6f, 1.6f), 0.f, 255.f);
}

void write_png_upright(const std::string& path, const uint8_t* stored, uint32_t w, uint32_t h) {
    std::vector<uint8_t> up(static_cast<size_t>(w) * h * 4);
    for (uint32_t y = 0; y < h; y++)
        std::memcpy(&up[static_cast<size_t>(y) * w * 4], &stored[static_cast<size_t>(h - 1 - y) * w * 4], w * 4);
    png_image png{};
    png.version = PNG_IMAGE_VERSION;
    png.width = w;
    png.height = h;
    png.format = PNG_FORMAT_RGBA;
    png_image_write_to_file(&png, path.c_str(), 0, up.data(), 0, nullptr);
}

// The title's main menu painted on the HD picture as paint_menu paints the disc's: its measures (the ink masks, the
// words' size) on the disc picture at 1x, upsampled; the hole filled from the HD hair; Options moved 34 x k lower;
// Choose Character drawn in the hand at k (its strokes k times as thick), in the HD words' colour.
std::vector<uint8_t> paint_menu_hd(const uint8_t* disc, const HdPicture& hd, bool lit, bool* ok) {
    const int k = hd.k;
    const uint32_t W = hd.w, H = hd.h, w = kTitleW, h = kTitleH;
    std::vector<uint8_t> out = hd.rgba;
    *ok = false;
    auto at = [&](int x, int y) { return &out[(static_cast<size_t>(H - 1 - y) * W + x) * 4]; };
    auto src = [&](int x, int y) { return &hd.rgba[(static_cast<size_t>(H - 1 - y) * W + x) * 4]; };
    auto dsrc = [&](int x, int y) { return &disc[(static_cast<size_t>(h - 1 - y) * w + x) * 4]; };
    // 1. The masks at 1x, exactly as paint_menu's, then grown by one disc pixel (the HD letters sit inside the disc's
    // masks grown by 1: hdtitle.md 1.7).
    constexpr int bx0 = 554, bx1 = 668, by0 = 325, by1 = 374;
    constexpr int bw = bx1 - bx0, bh = by1 - by0;
    std::vector<float> mask(static_cast<size_t>(bw) * bh), wide(mask.size());
    for (int y = 0; y < bh; y++)
        for (int x = 0; x < bw; x++) {
            const uint8_t* p = dsrc(bx0 + x, by0 + y);
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
        return out;  // not the picture measured: the pack's as it is
    grow1(wide, bw, bh);
    grow1(lift, bw, bh);
    // The words' colour on each picture, where the disc's mask says ink (Options).
    float disc_ink[3] = {0, 0, 0}, hd_ink[3] = {0, 0, 0};
    {
        std::vector<std::array<uint8_t, 3>> dp, hp;
        for (int y = 0; y < bh; y++)
            for (int x = 0; x < bw; x++)
                if (mask[static_cast<size_t>(y) * bw + x] > 0.6f) {
                    const uint8_t* p = dsrc(bx0 + x, by0 + y);
                    dp.push_back({p[0], p[1], p[2]});
                }
        for (int Y = by0 * k; Y < by1 * k; Y++)
            for (int X = bx0 * k; X < bx1 * k; X++)
                if (upsample(mask, bw, bh, bx0, by0, k, X, Y) > 0.6f) {
                    const uint8_t* p = src(X, Y);
                    hp.push_back({p[0], p[1], p[2]});
                }
        median_bright(dp, disc_ink);
        median_bright(hp, hd_ink);
    }
    // 2. The hole filled from the HD hair round it. The disc's masks follow its compression's 4x4 blocks (its words'
    // teal bleeds into them), which on the sharp HD hair would show as dark squares: on HD the words are taken by their
    // own brightness (they are far brighter than the near-black hair round them), only inside the disc's grown mask.
    const int BX0 = bx0 * k, BY0 = by0 * k, BW = bw * k, BH = bh * k;
    // (Brighter than the hair round it, not than a fixed level: a lighter strand of hair crosses the box.)
    std::vector<float> up_lift(static_cast<size_t>(BW) * BH), edge, ground(up_lift.size()), near(up_lift.size());
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            ground[static_cast<size_t>(y) * BW + x] = lum_of(src(BX0 + x, BY0 + y));
    blur(ground, BW, BH, 3 * k);
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++) {
            const size_t i = static_cast<size_t>(y) * BW + x;
            near[i] = clamp01(std::max(upsample(wide, bw, bh, bx0, by0, k, BX0 + x, BY0 + y) * 3.f,
                                       upsample(lift, bw, bh, bx0, by0, k, BX0 + x, BY0 + y)));
            const float l = lum_of(src(BX0 + x, BY0 + y));
            up_lift[i] = smooth(4.f, 18.f, l - ground[i]) * smooth(14.f, 30.f, l) * near[i];
        }
    blur(up_lift, BW, BH, std::max(1, k / 4));
    for (size_t i = 0; i < up_lift.size(); i++)
        up_lift[i] = clamp01(up_lift[i] * 1.5f);
    // What the fill erases: the letters above (moved in 3) and their soft edge and dark-teal outline, which the
    // brightness test leaves (a ghost of "Opt" under Choose Character at 1:1, zoom-hd-ghost.png). Against the hair round
    // them with the letters taken out, a point a little brighter or a little more teal than that is the word's too;
    // only within 2k of the letters (the outline is next to them: further out it would take the hair's own grain);
    // then grown by k/2 and softened, so the fill reaches all of it at full strength.
    auto grow_by = [&](std::vector<float>& f, int r) {  // a max over a (2r+1) square, by rows then columns
        for (int pass = 0; pass < 2; pass++) {
            std::vector<float> g(f.size(), 0.f);
            for (int y = 0; y < BH; y++)
                for (int x = 0; x < BW; x++) {
                    float m = 0.f;
                    for (int d = -r; d <= r; d++) {
                        const int nx = pass == 0 ? x + d : x, ny = pass == 0 ? y : y + d;
                        if (nx >= 0 && ny >= 0 && nx < BW && ny < BH)
                            m = std::max(m, f[static_cast<size_t>(ny) * BW + nx]);
                    }
                    g[static_cast<size_t>(y) * BW + x] = m;
                }
            f = std::move(g);
        }
    };
    std::vector<float> erase(up_lift.size());
    {
        std::vector<float> prox = up_lift;
        grow_by(prox, 2 * k);
        std::vector<float> w8(up_lift.size()), gl(up_lift.size()), gt(up_lift.size());
        for (size_t i = 0; i < up_lift.size(); i++) {
            const uint8_t* p = src(BX0 + static_cast<int>(i % BW), BY0 + static_cast<int>(i / BW));
            w8[i] = 1.f - clamp01(up_lift[i] * 3.f);
            gl[i] = lum_of(p) * w8[i];
            gt[i] = (static_cast<float>(p[2]) - p[0]) * w8[i];
        }
        blur(w8, BW, BH, 3 * k);
        blur(gl, BW, BH, 3 * k);
        blur(gt, BW, BH, 3 * k);
        for (size_t i = 0; i < erase.size(); i++) {
            const uint8_t* p = src(BX0 + static_cast<int>(i % BW), BY0 + static_cast<int>(i / BW));
            const float wsum = std::max(w8[i], 0.05f);
            const float dl = lum_of(p) - gl[i] / wsum, dt = (static_cast<float>(p[2]) - p[0]) - gt[i] / wsum;
            const float soft = std::max(smooth(1.5f, 7.f, dl), smooth(1.5f, 6.f, dt));
            erase[i] = std::max(up_lift[i], soft * near[i] * clamp01(prox[i] * 2.f));
        }
        grow_by(erase, std::max(1, k / 2));
        blur(erase, BW, BH, std::max(1, k / 4));
        for (size_t i = 0; i < erase.size(); i++)
            erase[i] = clamp01(erase[i] * 1.5f);
    }
    edge = erase;  // the erased part's soft edge, for the fill's average to keep off
    blur(edge, BW, BH, k);
    {
        std::vector<float> keep(static_cast<size_t>(BW) * BH), sum[3];
        for (int c = 0; c < 3; c++)
            sum[c].assign(keep.size(), 0.f);
        for (size_t i = 0; i < keep.size(); i++) {
            keep[i] = 1.f - clamp01(std::max(erase[i], edge[i]) * 4.f);
            const int x = static_cast<int>(i % BW), y = static_cast<int>(i / BW);
            for (int c = 0; c < 3; c++)
                sum[c][i] = src(BX0 + x, BY0 + y)[c] * keep[i];
        }
        for (int c = 0; c < 3; c++)
            blur(sum[c], BW, BH, 7 * k);
        blur(keep, BW, BH, 7 * k);
        for (int y = 0; y < BH; y++)
            for (int x = 0; x < BW; x++) {
                const size_t i = static_cast<size_t>(y) * BW + x;
                const float a = clamp01(std::max(erase[i], edge[i] * 2.f));
                if (a <= 0.f || keep[i] <= 0.001f)
                    continue;
                uint8_t* d = at(BX0 + x, BY0 + y);
                for (int c = 0; c < 3; c++)
                    d[c] = static_cast<uint8_t>(std::clamp(d[c] + (sum[c][i] / keep[i] - d[c]) * a, 0.f, 255.f));
            }
    }
    // 3. Options put back kMenuShift x k lower, from the HD picture.
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++) {
            const float a = up_lift[static_cast<size_t>(y) * BW + x];
            const int ty = BY0 + y + kMenuShift * k;
            if (a <= 0.f || ty >= static_cast<int>(H))
                continue;
            const uint8_t* s = src(BX0 + x, BY0 + y);
            uint8_t* d = at(BX0 + x, ty);
            for (int c = 0; c < 3; c++)
                d[c] = static_cast<uint8_t>(std::clamp(d[c] + (s[c] - d[c]) * a, 0.f, 255.f));
        }
    // 4. "Choose Character": its size chosen at 1x by paint_menu's rule, then drawn at k.
    std::vector<uint8_t> cover;
    int cw = 0, ch = 0, base = 0, ix0 = 0, ix1 = 0;
    float px = 64.f;
    for (; px >= 30.f; px -= 1.f) {
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
    if (ix1 < ix0 || !tome_hand_cover("Choose Character", px * k, kHdBold * k, &cover, &cw, &ch, &base))
        return out;
    ix0 = cw;
    ix1 = -1;
    for (int y = 0; y < ch; y++)
        for (int x = 0; x < cw; x++)
            if (cover[static_cast<size_t>(y) * cw + x] > 24) {
                ix0 = std::min(ix0, x);
                ix1 = std::max(ix1, x);
            }
    if (ix1 < ix0)
        return out;
    // (1x pixel 618's middle and baseline row 356, as paint_menu's, placed from the HD cover's own ink and baseline.)
    const int left = 618 * k + (k - 1) / 2 - (ix0 + ix1) / 2, top = 356 * k - base;
    float paint[3];
    {
        const float tuned[3] = {72.f, 97.f, 104.f};
        match_ink(disc_ink, hd_ink, tuned, paint);
    }
    const float light[3] = {0xA0, 0xE3, 0xF1}, dark[3] = {0.f, 0.f, 0.f};
    auto put = [&](int x, int y, const float rgb[3], float a) {
        if (x < 0 || y < 0 || x >= static_cast<int>(W) || y >= static_cast<int>(H) || a <= 0.f)
            return;
        uint8_t* p = at(x, y);
        for (int c = 0; c < 3; c++)
            p[c] = static_cast<uint8_t>(std::clamp(p[c] + (rgb[c] - p[c]) * std::min(a, 1.f), 0.f, 255.f));
    };
    // The shadow keeps off the other words' ink as paint_menu's does: measured on its 1x picture (Options moved).
    bool ok1 = false;
    const std::vector<uint8_t> moved = paint_menu(disc, w, h, false, &ok1, false);
    std::vector<float> other(static_cast<size_t>(w) * h, 0.f);
    for (uint32_t y = 0; y < h; y++)
        for (uint32_t x = 0; x < w; x++)
            other[static_cast<size_t>(y) * w + x] = menu_ink(&moved[(static_cast<size_t>(h - 1 - y) * w + x) * 4]);
    const int kR = 6 * k;
    const int hw = cw + 2 * kR, hh = ch + 2 * kR;
    std::vector<float> halo(static_cast<size_t>(hw) * hh, 0.f);
    for (int y = 0; y < ch; y++)
        for (int x = 0; x < cw; x++)
            halo[static_cast<size_t>(y + kR) * hw + x + kR] = cover[static_cast<size_t>(y) * cw + x] / 255.f;
    blur(halo, hw, hh, 4 * k);
    for (int y = 0; y < hh; y++)
        for (int x = 0; x < hw; x++) {
            const float m = halo[static_cast<size_t>(y) * hw + x];
            const int X = left + x - kR, Y = top + y - kR;
            if (X < 0 || Y < 0 || X >= static_cast<int>(W) || Y >= static_cast<int>(H) || m <= 0.f)
                continue;
            put(X, Y, dark, clamp01(m * 2.2f) * 0.45f * (1.f - upsample(other, w, h, 0, 0, k, X, Y)));
            if (lit)
                put(X, Y, light, clamp01(m * 1.6f) * 0.4f);
        }
    for (int y = 0; y < ch; y++)
        for (int x = 0; x < cw; x++)
            if (const uint8_t v = cover[static_cast<size_t>(y) * cw + x])
                put(left + x, top + y, lit ? light : paint, v / 255.f * (lit ? 1.f : 0.9f));
    if (!lit)
        std::fprintf(stderr, "[titlevideo] HD title: the words' ink on the disc (%.0f,%.0f,%.0f), on HD (%.0f,%.0f,%.0f): "
                     "Choose Character in (%.0f,%.0f,%.0f), %.0f px at 1x, %dx%d at x%d\n", disc_ink[0], disc_ink[1],
                     disc_ink[2], hd_ink[0], hd_ink[1], hd_ink[2], paint[0], paint[1], paint[2], px, cw, ch, k);
    *ok = true;
    return out;
}

// Where painted differs from the base (stored rows), kept small.
HdPatch patch_of(const std::vector<uint8_t>& painted, const HdPicture& base) {
    HdPatch p;
    uint32_t x0 = base.w, x1 = 0, y0 = base.h, y1 = 0;
    for (uint32_t y = 0; y < base.h; y++) {
        const uint8_t* a = &painted[static_cast<size_t>(y) * base.w * 4];
        const uint8_t* b = &base.rgba[static_cast<size_t>(y) * base.w * 4];
        if (std::memcmp(a, b, static_cast<size_t>(base.w) * 4) == 0)
            continue;
        y0 = std::min(y0, y);
        y1 = y;
        for (uint32_t x = 0; x < base.w; x++)
            if (std::memcmp(a + x * 4, b + x * 4, 4) != 0) {
                x0 = std::min(x0, x);
                x1 = std::max(x1, x);
            }
    }
    if (x1 < x0 || y1 < y0)
        return p;
    p.x0 = x0;
    p.y0 = y0;
    p.w = x1 - x0 + 1;
    p.h = y1 - y0 + 1;
    p.rgba.resize(static_cast<size_t>(p.w) * p.h * 4);
    for (uint32_t y = 0; y < p.h; y++)
        std::memcpy(&p.rgba[static_cast<size_t>(y) * p.w * 4],
                    &painted[((static_cast<size_t>(p.y0) + y) * base.w + p.x0) * 4], static_cast<size_t>(p.w) * 4);
    return p;
}

// The title's two HD paints (unlit, lit), on a worker; on failure the pack's picture unpainted.
void hd_title_paint(std::shared_ptr<const HdPicture> base, std::vector<uint8_t> disc) {
    const int64_t t0 = now_ms();
    HdPatch patches[2];
    bool ok = true;
    for (int lit = 0; lit < 2 && ok; lit++) {
        std::vector<uint8_t> painted = paint_menu_hd(disc.data(), *base, lit != 0, &ok);
        if (!ok)
            break;
        patches[lit] = patch_of(painted, *base);
        if (const char* dump = std::getenv("ED_TITLE_PAINT_DUMP"); dump != nullptr && *dump != 0)
            write_png_upright(std::string(dump) + (lit ? "-hd-lit.png" : "-hd-unlit.png"), painted.data(), base->w, base->h);
    }
    if (!ok)
        patches[0] = patches[1] = HdPatch{};
    const uint32_t sizes[4] = {patches[0].w, patches[0].h, patches[1].w, patches[1].h};
    {
        std::lock_guard<std::mutex> lock(g_hd_mutex);
        g_hd.title_patch[0] = std::move(patches[0]);
        g_hd.title_patch[1] = std::move(patches[1]);
        g_hd.title_painting = false;
    }
    g_hd_title_ready = true;
    std::fprintf(stderr, "[titlevideo] HD title %s in %lld ms (%ux%u; unlit and lit patches %ux%u, %ux%u)\n",
                 ok ? "painted" : "NOT painted (not as measured, or no hand font): the pack's picture as it is",
                 static_cast<long long>(ms_since(t0)), base->w, base->h, sizes[0], sizes[1], sizes[2], sizes[3]);
}

// Under g_hd_mutex: the title's paint started once both the pack's picture and the disc's are in hand.
void hd_title_kick_locked() {
    if (g_hd.title_state != 2 || !g_hd.title || g_hd.title_disc.empty() || g_hd.title_painting || g_hd_title_ready)
        return;
    g_hd.title_painting = true;
    hd_thread([base = g_hd.title, disc = g_hd.title_disc] { hd_title_paint(base, disc); });
}

// The title's pack picture read (once), then painted if the disc's has been asked for.
void hd_want_title() {
    std::lock_guard<std::mutex> lock(g_hd_mutex);
    if (g_hd.title_state != 0 || !g_settings.hd_textures || texpack_count() == 0)
        return;
    g_hd.title_state = 1;
    hd_thread([] {
        std::shared_ptr<const HdPicture> pic = hd_read(kTitleHash, kTitleHdMax, "title");
        std::lock_guard<std::mutex> lock(g_hd_mutex);
        g_hd.title = std::move(pic);
        g_hd.title_state = 2;
        hd_title_kick_locked();
    });
}

// The Options words' box blacked out on an HD picture (x 140..441, y 60..471 upright at 1x, less the logo's corner x
// >= 400, y < 200): measured, only the words are brighter than 6 there and the ground is black (hdtitle.md 1.7).
std::shared_ptr<const HdPicture> options_clean_of(const HdPicture& hd) {
    auto clean = std::make_shared<HdPicture>(hd);
    const int k = hd.k;
    for (int Y = kBoxY0 * k; Y < kBoxY1 * k; Y++)
        for (int X = kBoxX0 * k; X < kBoxX1 * k; X++) {
            if (X >= 400 * k && Y < 200 * k)
                continue;
            uint8_t* d = &clean->rgba[(static_cast<size_t>(hd.h - 1 - Y) * hd.w + X) * 4];
            d[0] = d[1] = d[2] = 0;
        }
    return clean;
}

std::string art_hd_path() {
    return (std::filesystem::path(frontend_res_dir()) / "video" / "options-clean-hd.png").string();
}

// The HD backdrop's maker: raised when options_clean_of or kOptionsHdMax changes, so a kept file is made again.
constexpr int kArtHdVersion = 1;

// What the HD backdrop was made from: this code, the scale, and the pack file's path, size and time. Kept beside it
// (options-clean-hd.png.stamp): a pack swapped for one with older file times (an unzip keeps the zip's) is still seen.
std::string art_hd_stamp(const HdPicture& clean) {
    const std::string pack = texpack_file(kCmpr, kTitleW, kTitleH, kOptionsHash);
    std::error_code ec;
    const auto size = pack.empty() ? 0 : std::filesystem::file_size(pack, ec);
    const auto time = pack.empty() ? std::filesystem::file_time_type{} : std::filesystem::last_write_time(pack, ec);
    return "options-clean-hd " + std::to_string(kArtHdVersion) + "\nscale " + std::to_string(clean.k) + " " +
           std::to_string(clean.w) + "x" + std::to_string(clean.h) + "\npack " + pack + "\nsize " +
           std::to_string(ec ? 0 : size) + "\ntime " +
           std::to_string(ec ? 0 : static_cast<long long>(time.time_since_epoch().count())) + "\n";
}

// The title look's HD backdrop (the clean HD Options picture, upright), written when missing or when its stamp is not
// this pack's and this code's: to a .tmp first, then renamed, so the menu never reads half a file; the stamp after.
void write_art_hd(const HdPicture& clean) {
    const std::string path = art_hd_path(), stamp_path = path + ".stamp";
    const std::string stamp = art_hd_stamp(clean);
    std::error_code ec;
    if (std::filesystem::is_regular_file(path, ec)) {
        std::string kept;
        if (FILE* f = std::fopen(stamp_path.c_str(), "rb")) {
            char buf[2048];
            const size_t n = std::fread(buf, 1, sizeof buf, f);
            kept.assign(buf, n);
            std::fclose(f);
        }
        if (kept == stamp) {
            g_hd_art_ready = true;
            std::fprintf(stderr, "[titlevideo] the title look's HD backdrop up to date (%s)\n", path.c_str());
            return;
        }
        std::fprintf(stderr, "[titlevideo] the title look's HD backdrop is %s: made again\n",
                     kept.empty() ? "not stamped" : "another pack's or an older build's");
    }
    const int64_t t0 = now_ms();
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    const std::string tmp = path + ".tmp";
    std::filesystem::remove(stamp_path, ec);  // (no stamp until the picture is whole)
    write_png_upright(tmp, clean.rgba.data(), clean.w, clean.h);
    ec.clear();
    std::filesystem::rename(tmp, path, ec);
    g_hd_art_ready = !ec;
    if (!ec) {
        const std::string stmp = stamp_path + ".tmp";
        if (FILE* f = std::fopen(stmp.c_str(), "wb")) {
            std::fwrite(stamp.data(), 1, stamp.size(), f);
            std::fclose(f);
            std::error_code ec2;
            std::filesystem::rename(stmp, stamp_path, ec2);
        }
    }
    std::fprintf(stderr, "[titlevideo] the title look's HD backdrop %s %s (%ux%u, %lld ms)\n",
                 ec ? "NOT written" : "written", path.c_str(), clean.w, clean.h, static_cast<long long>(ms_since(t0)));
}

// The HD Options paint's parts (the hand's six rows at k and 1x, the words' colour), made on a worker once the pack's
// picture is read and the disc's has been measured (find_rows: res/video/options.rgba, or the first ask).
void hd_options_prep() {
    std::shared_ptr<const HdPicture> base;
    {
        std::lock_guard<std::mutex> lock(g_hd_mutex);
        if (g_hd.options_state != 2 || !g_hd.options || g_hd.options_prepping || g_hd_options_ready)
            return;
        g_hd.options_prepping = true;
        base = g_hd.options;
    }
    std::vector<uint8_t> pic;
    {
        std::lock_guard<std::mutex> lock(g_word_mutex);
        pic = skull();
    }
    bool rows = false;
    if (pic.size() == static_cast<size_t>(kTitleW) * kTitleH * 4) {
        std::lock_guard<std::mutex> lock(g_more_mutex);
        rows = find_rows(pic.data(), kTitleW, kTitleH);
    }
    if (!rows) {
        std::lock_guard<std::mutex> lock(g_hd_mutex);
        g_hd.options_prepping = false;  // (tried again when the disc's picture is asked for)
        return;
    }
    hd_thread([base, pic] {
        const int64_t t0 = now_ms();
        const int k = base->k;
        constexpr float kPx = 52.f;
        OptionsCover covers[kOptionRows - kGameRows];
        bool ok = true;
        for (int j = 0; j < kOptionRows - kGameRows && ok; j++) {
            OptionsCover& c = covers[j];
            ok = tome_hand_cover(kOurRows[j], kPx * k, kHdBold * k, &c.hd, &c.hw, &c.hh, &c.hbase) &&
                 tome_hand_cover(kOurRows[j], kPx, 0.8f, &c.x1, &c.w1, &c.h1, &c.base1);
        }
        // The words' colour: the game's rows' ink on each picture.
        float disc_ink[3] = {0, 0, 0}, hd_ink[3] = {0, 0, 0}, ink[3] = {68.f, 92.f, 98.f};
        {
            std::vector<std::array<uint8_t, 3>> dp, hp;
            std::lock_guard<std::mutex> lock(g_more_mutex);
            for (int y = 0; y < kBoxH; y++)
                for (int x = 0; x < kBoxW; x++) {
                    if (g_row_of[static_cast<size_t>(y) * kBoxW + x] < 0)
                        continue;
                    const uint8_t* p = &pic[(static_cast<size_t>(kTitleH - 1 - (kBoxY0 + y)) * kTitleW + kBoxX0 + x) * 4];
                    if (lum_of(p) > 6.f)
                        dp.push_back({p[0], p[1], p[2]});
                    for (int dy = 0; dy < k; dy++)
                        for (int dx = 0; dx < k; dx++) {
                            const int X = (kBoxX0 + x) * k + dx, Y = (kBoxY0 + y) * k + dy;
                            const uint8_t* q = &base->rgba[(static_cast<size_t>(base->h - 1 - Y) * base->w + X) * 4];
                            if (lum_of(q) > 6.f)
                                hp.push_back({q[0], q[1], q[2]});
                        }
                }
            median_bright(dp, disc_ink);
            median_bright(hp, hd_ink);
            const float tuned[3] = {68.f, 92.f, 98.f};
            match_ink(disc_ink, hd_ink, tuned, ink);
        }
        {
            std::lock_guard<std::mutex> lock(g_hd_mutex);
            for (int j = 0; j < kOptionRows - kGameRows; j++)
                g_hd.covers[j] = std::move(covers[j]);
            std::copy(ink, ink + 3, g_hd.options_ink);
            g_hd.options_prepping = false;
        }
        if (ok)
            g_hd_options_ready = true;
        std::fprintf(stderr, "[titlevideo] HD Options %s in %lld ms (x%d; the words' ink on the disc (%.0f,%.0f,%.0f), "
                     "on HD (%.0f,%.0f,%.0f): ours in (%.0f,%.0f,%.0f))\n", ok ? "ready" : "NOT ready (no hand font)",
                     static_cast<long long>(ms_since(t0)), k, disc_ink[0], disc_ink[1], disc_ink[2], hd_ink[0], hd_ink[1],
                     hd_ink[2], ink[0], ink[1], ink[2]);
    });
}

// The Options pack picture read (once): its clean copy, the title look's HD backdrop, then the paint's parts. True
// when this call started it.
bool hd_want_options() {
    {
        std::lock_guard<std::mutex> lock(g_hd_mutex);
        if (g_hd.options_state != 0 || !g_settings.hd_textures || texpack_count() == 0)
            return false;
        g_hd.options_state = 1;
    }
    hd_thread([] {
        std::shared_ptr<const HdPicture> pic = hd_read(kOptionsHash, kOptionsHdMax, "Options");
        std::shared_ptr<const HdPicture> clean = pic ? options_clean_of(*pic) : nullptr;
        {
            std::lock_guard<std::mutex> lock(g_hd_mutex);
            g_hd.options = pic;
            g_hd.options_clean = clean;
            g_hd.options_state = 2;
        }
        if (clean)
            write_art_hd(*clean);
        hd_options_prep();
    });
    return true;
}

// The title picture at HD for the cursor (the kept pack picture with the lit or unlit patch, one copy), or false: HD
// off, no pack picture, or its paint not ready yet (the 1x stand-in shows meanwhile; the generation asks again).
bool hd_title_give(const uint8_t* disc, uint32_t w, uint32_t h, uint8_t** out_rgba, uint32_t* out_width,
                   uint32_t* out_height) {
    if (!g_settings.hd_textures || !is_picture(w, h))
        return false;
    hd_want_title();
    std::shared_ptr<const HdPicture> base;
    {
        std::lock_guard<std::mutex> lock(g_hd_mutex);
        if (g_hd.title_disc.empty()) {
            g_hd.title_disc.assign(disc, disc + static_cast<size_t>(w) * h * 4);
            hd_title_kick_locked();
        }
        if (!g_hd_title_ready || !g_hd.title)
            return false;
        base = g_hd.title;
    }
    const int64_t t0 = now_ms();
    uint8_t* out = static_cast<uint8_t*>(std::malloc(base->rgba.size()));
    if (out == nullptr)
        return false;
    std::memcpy(out, base->rgba.data(), base->rgba.size());
    const bool lit = g_menu_row.load() == 5;
    {
        std::lock_guard<std::mutex> lock(g_hd_mutex);
        const HdPatch& p = g_hd.title_patch[lit ? 1 : 0];
        for (uint32_t y = 0; y < p.h; y++)
            std::memcpy(&out[((static_cast<size_t>(p.y0) + y) * base->w + p.x0) * 4], &p.rgba[static_cast<size_t>(y) * p.w * 4],
                        static_cast<size_t>(p.w) * 4);
    }
    *out_rgba = out;
    *out_width = base->w;
    *out_height = base->h;
    static int told = 0;
    if (told < 8) {
        told++;
        std::fprintf(stderr, "[titlevideo] HD title given (%s, %ux%u) in %lld ms\n", lit ? "lit" : "unlit", base->w, base->h,
                     static_cast<long long>(ms_since(t0)));
    }
    return true;
}

// The Options list painted on the HD picture as paint_options paints the disc's (under g_more_mutex: g_row_of).
std::vector<uint8_t> paint_options_hd(const HdPicture& hd, const HdPicture& clean, const OptionsCover* covers,
                                      const float ink[3], int lit, int s, int values) {
    const int k = hd.k;
    const uint32_t W = hd.w, H = hd.h;
    std::vector<uint8_t> out = clean.rgba;  // 1. the painted words erased
    auto at = [&](int x, int y) { return &out[(static_cast<size_t>(H - 1 - y) * W + x) * 4]; };
    auto src = [&](int x, int y) { return &hd.rgba[(static_cast<size_t>(H - 1 - y) * W + x) * 4]; };
    // 1b. The logo's shade, made at 1x and upsampled.
    {
        constexpr int kW = kShadeX1 - kShadeX0, kH = kShadeY1 - kShadeY0;
        std::vector<float> shade;
        if (options_shade(s, values, &shade))
            for (int Y = kShadeY0 * k; Y < kShadeY1 * k; Y++)
                for (int X = kShadeX0 * k; X < kShadeX1 * k; X++) {
                    const float f = 1.f - 0.85f * clamp01(upsample(shade, kW, kH, kShadeX0, kShadeY0, k, X, Y) * 2.5f);
                    uint8_t* d = at(X, Y);
                    for (int c = 0; c < 3; c++)
                        d[c] = static_cast<uint8_t>(d[c] * f);
                }
    }
    // 2. The game's rows still shown, each at its slot, from the HD picture (each HD point takes its 1x point's row).
    for (int Y = kBoxY0 * k; Y < kBoxY1 * k; Y++)
        for (int X = kBoxX0 * k; X < kBoxX1 * k; X++) {
            const int r = g_row_of[static_cast<size_t>(Y / k - kBoxY0) * kBoxW + (X / k - kBoxX0)];
            if (r < s)
                continue;
            const int ty = Y + (kSlotY[r - s] - kRowY[r]) * k;
            if (ty < 0 || ty >= static_cast<int>(H))
                continue;
            const uint8_t* p = src(X, Y);
            uint8_t* d = at(X, ty);
            for (int c = 0; c < 3; c++)
                d[c] = std::max(d[c], p[c]);
        }
    auto put = [&](int x, int y, const float rgb[3], float a) {
        if (x < 0 || y < 0 || x >= static_cast<int>(W) || y >= static_cast<int>(H) || a <= 0.f)
            return;
        uint8_t* p = at(x, y);
        for (int c = 0; c < 3; c++)
            p[c] = static_cast<uint8_t>(std::clamp(p[c] + (rgb[c] - p[c]) * std::min(a, 1.f), 0.f, 255.f));
    };
    const float light[3] = {0xA0, 0xE3, 0xF1};
    // 3. Ours in their slots: the glow made at 1x (as paint_options') and upsampled, the letters at k.
    constexpr float kPx = 52.f;
    for (int j = 0; j < kOptionRows - kGameRows; j++) {
        const int slot = kGameRows + j - s;
        if (slot < 0 || slot >= kSlots)
            continue;
        const OptionsCover& c = covers[j];
        const bool on = lit == kGameRows + j;
        const int left1 = 174 - static_cast<int>(kPx / 4.f), top1 = kSlotY[slot] - c.base1;
        const int left = 174 * k - static_cast<int>(kPx * k / 4.f), top = kSlotY[slot] * k - c.hbase;
        if (on) {
            constexpr int kR = 6;
            const int gw = c.w1 + 2 * kR, gh = c.h1 + 2 * kR;
            std::vector<float> glow(static_cast<size_t>(gw) * gh, 0.f);
            for (int y = -kR; y < c.h1 + kR; y++)
                for (int x = -kR; x < c.w1 + kR; x++) {
                    float m = 0.f;
                    for (int dy = -kR; dy <= kR; dy += 2)
                        for (int dx = -kR; dx <= kR; dx += 2) {
                            const int sx = x + dx, sy = y + dy;
                            if (sx < 0 || sy < 0 || sx >= c.w1 || sy >= c.h1)
                                continue;
                            const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                            if (d <= kR)
                                m = std::max(m, c.x1[static_cast<size_t>(sy) * c.w1 + sx] / 255.f * (1.f - d / (kR + 1)));
                        }
                    glow[static_cast<size_t>(y + kR) * gw + x + kR] = m;
                }
            const int gx0 = left1 - kR, gy0 = top1 - kR;
            for (int Y = gy0 * k; Y < (gy0 + gh) * k; Y++)
                for (int X = gx0 * k; X < (gx0 + gw) * k; X++)
                    put(X, Y, light, upsample(glow, gw, gh, gx0, gy0, k, X, Y) * 0.35f);
        }
        for (int y = 0; y < c.hh; y++)
            for (int x = 0; x < c.hw; x++)
                if (const uint8_t v = c.hd[static_cast<size_t>(y) * c.hw + x])
                    put(left + x, top + y, on ? light : ink, v / 255.f * (on ? 1.f : 0.9f));
    }
    // 4. The marks, as paint_options' chevrons at k.
    auto chevron = [&](int cx1, int cy1, int dir) {
        const float half = 7.f * k, tall = 5.f * k, width = 1.6f * k;
        const float cx = cx1 * k + (k - 1) / 2.f, cy = cy1 * k + (k - 1) / 2.f;
        const float tip[2] = {cx, cy + dir * tall};
        const float ends[2][2] = {{cx - half, cy - dir * tall}, {cx + half, cy - dir * tall}};
        const float mark[3] = {88.f, 118.f, 126.f};
        for (int y = static_cast<int>(cy) - 10 * k; y <= static_cast<int>(cy) + 10 * k; y++)
            for (int x = static_cast<int>(cx) - 11 * k; x <= static_cast<int>(cx) + 11 * k; x++) {
                float best = 1e9f;
                for (const auto& e : ends) {
                    const float vx = tip[0] - e[0], vy = tip[1] - e[1];
                    const float t = std::clamp(((x - e[0]) * vx + (y - e[1]) * vy) / (vx * vx + vy * vy), 0.f, 1.f);
                    const float dx = x - (e[0] + vx * t), dy = y - (e[1] + vy * t);
                    best = std::min(best, std::sqrt(dx * dx + dy * dy));
                }
                put(x, y, mark, clamp01(width + 0.5f - best));
            }
    };
    if (s > 0)
        chevron(156, kSlotY[0] - 12, -1);
    if (s < kScrollMax)
        chevron(156, kSlotY[kSlots - 1] - 10, 1);
    return out;
}

// The Options picture at HD for the asked (row, scroll, values), painted at once and kept for a few (one copy given),
// or 0: HD off, no pack picture, its parts not ready, or the disc's not as measured (then options_picture's 1x).
int options_picture_hd(const uint8_t* rgba, uint32_t w, uint32_t h, uint8_t** out_rgba, uint32_t* out_width,
                       uint32_t* out_height) {
    if (!g_settings.hd_textures || !is_picture(w, h))
        return 0;
    hd_want_options();
    if (!g_hd_options_ready) {
        hd_options_prep();  // (the disc's picture is kept by now: keep_skull)
        return 0;
    }
    std::lock_guard<std::mutex> rows(g_more_mutex);
    if (!find_rows(rgba, w, h))
        return 0;
    g_options_ready = true;
    const int asked = g_options_asked.load();
    const int row = asked < 0 ? 0 : asked & 0xFF, s = asked < 0 ? 0 : std::clamp((asked >> 8) & 0xFF, 0, kScrollMax);
    const int values = asked < 0 ? 0 : (asked >> 16) & 0x1F;
    const int key = values * 256 + row * 16 + s;
    std::lock_guard<std::mutex> lock(g_hd_mutex);
    if (!g_hd.options || !g_hd.options_clean)
        return 0;
    const HdPicture& hd = *g_hd.options;
    const OptionsHdPaint* found = nullptr;
    for (const OptionsHdPaint& p : g_hd.options_paints)
        if (p.key == key)
            found = &p;
    const int64_t t0 = now_ms();
    int64_t painted = 0;
    if (found == nullptr) {
        if (g_hd.options_paints.size() >= 3)
            g_hd.options_paints.erase(g_hd.options_paints.begin());
        g_hd.options_paints.push_back(
            {key, paint_options_hd(hd, *g_hd.options_clean, g_hd.covers, g_hd.options_ink, row, s, values)});
        found = &g_hd.options_paints.back();
        painted = ms_since(t0);
        if (const char* dump = std::getenv("ED_TITLE_PAINT_DUMP"); dump != nullptr && *dump != 0)
            write_png_upright(std::string(dump) + "-hd-options-" + std::to_string(row) + "-" + std::to_string(s) + ".png",
                              found->rgba.data(), hd.w, hd.h);
    }
    const int64_t t1 = now_ms();
    *out_rgba = static_cast<uint8_t*>(std::malloc(found->rgba.size()));
    if (*out_rgba == nullptr)
        return 0;
    std::memcpy(*out_rgba, found->rgba.data(), found->rgba.size());
    *out_width = hd.w;
    *out_height = hd.h;
    static int told = 0;
    if (told < 16) {
        told++;
        std::fprintf(stderr, "[titlevideo] HD Options given (row %d, scroll %d, values %d): painted in %lld ms (0: kept), "
                     "copied in %lld ms\n", row, s, values, static_cast<long long>(painted),
                     static_cast<long long>(ms_since(t1)));
    }
    return 1;
}

// The generation's HD bits: the title's paint and the Options parts ready, with HD on.
bool hd_title_on() { return g_hd_title_ready.load() && g_settings.hd_textures; }
bool hd_options_on() { return g_hd_options_ready.load() && g_settings.hd_textures; }

int options_picture(const uint8_t* rgba, uint32_t w, uint32_t h, uint8_t** out_rgba, uint32_t* out_width,
                    uint32_t* out_height) {
    if (options_picture_hd(rgba, w, h, out_rgba, out_width, out_height) == 1)
        return 1;  // on the pack's HD picture
    std::vector<uint8_t> list;
    if (!options_list(rgba, w, h, &list))
        return 0;  // the game's own 8 rows
    return give(list, w, h, out_rgba, out_width, out_height);
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
    // One word for every 850x480 picture: the Options picture is redrawn for each (scroll, lit row of ours), the main
    // menu's when its Choose Character row comes into use and when it is lit or not (and the video's frames change the
    // title's). The Options paint uses the very (row, scroll, values) read here, so the two can't disagree.
    const int row = g_menu_row.load();
    const int options = g_options_key.load();
    g_options_asked = options;
    const int lit = options >= 0 && (options & 0xFF) >= kGameRows ? (options & 0xFF) - kGameRows + 1 : 0;
    const uint64_t list = options < 0 ? 0u
                                      : 1u + static_cast<uint64_t>(lit) + 7u * static_cast<uint64_t>((options >> 8) & 0xFF) +
                                            42u * static_cast<uint64_t>((options >> 16) & 0x1F);
    const uint64_t key = list * 8u + (row >= 0 ? 4u : 0u) + (row == 5 ? 1u : 0u);
    // The HD bits (titlevideo's HD paints ready, with HD textures on): the title's +2 (bit 1 of key, free), Options'
    // 0x8000 (free under kOptionsMoreSerial) or bit 60 with the video.
    if (video_on())
        return (g.serial.load() * 16384u + key) ^ (hd_options_on() ? 1ull << 60 : 0u);
    return (kOptionsMoreSerial | key) + (hd_title_on() ? 2u : 0u) + (hd_options_on() ? 0x8000u : 0u);
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
    const uint8_t* disc = rgba;
    if (menu_picture(rgba, width, height, &menu))
        rgba = menu.data();
    if (!video_on()) {
        // On the pack's HD picture once painted (the 1x paint meanwhile; the pack's unpainted if the paint fails).
        if (hd_title_give(disc, width, height, out_rgba, out_width, out_height))
            return 1;
        return menu.empty() ? 0 : give(menu, width, height, out_rgba, out_width, out_height);
    }
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

// The title Options' cursor and scroll as its frame draws (hle.c, the game thread), row -1 not in use: rows 8-13 are ours.
extern "C" void titlevideo_options_row(int row, int scroll, int values) {
    // (At the top nothing is moved into the logo: the values change nothing there.)
    g_options_key = row < 0 ? -1 : (row & 0xFF) | ((scroll & 0xFF) << 8) | ((scroll > 0 ? values & 0x1F : 0) << 16);
}
// The Options picture has been found as measured and is painted as the list (hle.c turns the 14 rows on only then).
extern "C" int titlevideo_options_painted(void) { return g_options_ready.load() ? 1 : 0; }
// The main menu's cursor as its frame draws (hle.c, the game thread), -1 not English: row 5 lights "Choose Character".
extern "C" void titlevideo_menu_row(int row) { g_menu_row = row; }
// The main menu's picture has been repainted with Choose Character (hle.c adds the row only then).
extern "C" int titlevideo_menu_painted(void) { return g_menu_painted.load() ? 1 : 0; }

extern "C" int titlevideo_options_kept(void) {
    std::lock_guard<std::mutex> lock(g_word_mutex);
    return skull().empty() ? 0 : 1;
}

// The Options picture from the game's own bank at the title's first frame (hle.c title_options_from_bank): checked by
// size and by the renderer's own hash of the raw texels (XXH64 of the base level, as gxcore_draw.cpp hashes it), then
// decoded with the renderer's own decoder (gxc::decode_texture) on a worker and kept, as the renderer's decode would
// keep it when Options is first drawn: the same function on the same bytes, so the same file. Rows in stored order (the
// picture is stored upside down; titlevideo_title_art turns it over).
extern "C" int titlevideo_options_from_bank(const uint8_t* cmpr, uint32_t w, uint32_t h, size_t size) {
    if (w != kTitleW || h != kTitleH || cmpr == nullptr || XXH64(cmpr, size, 0) != kOptionsHash)
        return 0;
    std::vector<uint8_t> bytes(cmpr, cmpr + size);
    hd_thread([bytes = std::move(bytes)] {
        const int64_t t0 = now_ms();
        const std::vector<uint8_t> rgba = gxruntime::gxcore::decode_texture(kCmpr, kTitleW, kTitleH, bytes.data(), bytes.size());
        if (rgba.size() != static_cast<size_t>(kTitleW) * kTitleH * 4) {
            std::fprintf(stderr, "[titlevideo] the Options picture from the game's memory did not decode (%zu bytes)\n",
                         rgba.size());
            return;
        }
        keep_skull(rgba.data());
        std::fprintf(stderr, "[titlevideo] the Options picture taken from the game's memory at the title (%lld ms)\n",
                     static_cast<long long>(ms_since(t0)));
    });
    return 1;
}

extern "C" void titlevideo_shutdown(void) {
    // (At most 3 s: a pack read and the title's paint take well under one.)
    for (int i = 0; i < 300 && g_hd_workers.load() > 0; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

extern "C" void titlevideo_preload(void) {
    static bool waits = false;
    if (!waits) {
        waits = true;
        std::atexit(titlevideo_shutdown);  // (any other way out of main, too)
    }
    if (!g_settings.hd_textures || texpack_count() == 0) {
        std::fprintf(stderr, "[titlevideo] HD pictures: %s\n", g_settings.hd_textures ? "no pack" : "HD textures off");
        return;
    }
    hd_want_options();  // (Options and the title look's backdrop, video or not)
    if (!g_settings.mod_title_video)
        hd_want_title();  // (with the video it is the video; read later if the video's file is missing)
}

// The first-run step (frontend.cpp run_preparing): what the title look's HD backdrop is made from, "off" with HD textures
// off or no pack, "none" when the pack has no Options picture; and whether the boot's HD workers are still at work.
std::string titlevideo_hd_id() {
    if (!g_settings.hd_textures || texpack_count() == 0)
        return "off";
    const std::string pack = texpack_file(kCmpr, kTitleW, kTitleH, kOptionsHash);
    if (pack.empty())
        return "none";
    std::error_code ec;
    const auto size = std::filesystem::file_size(pack, ec);
    const auto time = std::filesystem::last_write_time(pack, ec);
    return std::to_string(kArtHdVersion) + " " + std::to_string(ec ? 0 : size) + " " +
           std::to_string(ec ? 0 : static_cast<long long>(time.time_since_epoch().count())) + " " + pack;
}
bool titlevideo_hd_busy() { return g_hd_workers.load() > 0; }

bool titlevideo_title_art_hd() {
    // HD textures turned on after boot (off at boot, so preload read nothing): the pack's Options picture is read now,
    // so the title look gets its HD backdrop without the game's Options screen being opened first. hd_want_options
    // reads once and returns at once after that.
    if (!g_hd_art_ready.load() && g_settings.hd_textures && hd_want_options())
        std::fprintf(stderr, "[titlevideo] HD textures on after boot: the pack's Options picture read for the title look\n");
    return g_hd_art_ready.load();
}

// The menu's title look (frontend.cpp): the Options picture with its painted words erased, upright, as
// DIR/options-clean.png, and the list's scroll marks (the painted list's pen-stroke chevrons, larger, in its ink) as
// DIR/mark-up.png and DIR/mark-down.png. False when the picture isn't kept yet or isn't the one measured (then no
// picture is written; the marks always are).
bool titlevideo_title_art(const std::string& dir) {
    auto write = [](const std::string& path, uint32_t w, uint32_t h, const std::vector<uint8_t>& rgba) {
        png_image png{};
        png.version = PNG_IMAGE_VERSION;
        png.width = w;
        png.height = h;
        png.format = PNG_FORMAT_RGBA;
        return png_image_write_to_file(&png, path.c_str(), 0, rgba.data(), 0, nullptr) != 0;
    };
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    // The marks: 48 x 32, a chevron of two soft strokes (as paint_options' chevron, three times its size).
    for (int dir_y : {-1, 1}) {
        constexpr int kW = 48, kH = 32;
        std::vector<uint8_t> mark(static_cast<size_t>(kW) * kH * 4, 0);
        const float cx = kW / 2.f, cy = kH / 2.f, half = 19.f, tall = 9.f, width = 3.2f;
        const float tip[2] = {cx, cy + dir_y * tall};
        const float ends[2][2] = {{cx - half, cy - dir_y * tall}, {cx + half, cy - dir_y * tall}};
        for (int y = 0; y < kH; y++)
            for (int x = 0; x < kW; x++) {
                const float px = x + 0.5f, py = y + 0.5f;
                float best = 1e9f;
                for (const auto& e : ends) {
                    const float vx = tip[0] - e[0], vy = tip[1] - e[1];
                    const float t = std::clamp(((px - e[0]) * vx + (py - e[1]) * vy) / (vx * vx + vy * vy), 0.f, 1.f);
                    const float dx = px - (e[0] + vx * t), dy = py - (e[1] + vy * t);
                    best = std::min(best, std::sqrt(dx * dx + dy * dy));
                }
                uint8_t* p = &mark[(static_cast<size_t>(y) * kW + x) * 4];
                p[0] = 88;
                p[1] = 118;
                p[2] = 126;
                p[3] = static_cast<uint8_t>(255.f * clamp01(width - best + 0.5f));
            }
        write(dir + (dir_y < 0 ? "/mark-up.png" : "/mark-down.png"), kW, kH, mark);
    }
    std::vector<uint8_t> pic;
    {
        std::lock_guard<std::mutex> lock(g_word_mutex);
        pic = skull();
    }
    if (pic.size() != static_cast<size_t>(kTitleW) * kTitleH * 4)
        return false;
    std::lock_guard<std::mutex> lock(g_more_mutex);
    if (!find_rows(pic.data(), kTitleW, kTitleH))
        return false;
    std::vector<uint8_t> up(pic.size());
    for (uint32_t y = 0; y < kTitleH; y++)
        std::memcpy(&up[static_cast<size_t>(y) * kTitleW * 4], &pic[static_cast<size_t>(kTitleH - 1 - y) * kTitleW * 4],
                    kTitleW * 4);
    for (int y = 0; y < kBoxH; y++)
        for (int x = 0; x < kBoxW; x++)
            if (g_row_of[static_cast<size_t>(y) * kBoxW + x] >= 0) {
                uint8_t* d = &up[(static_cast<size_t>(kBoxY0 + y) * kTitleW + kBoxX0 + x) * 4];
                d[0] = d[1] = d[2] = 0;
            }
    return write(dir + "/options-clean.png", kTitleW, kTitleH, up);
}
