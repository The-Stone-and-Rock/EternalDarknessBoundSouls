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

#define PL_MPEG_IMPLEMENTATION
#include "third_party/pl_mpeg.h"

#include <algorithm>
#include <atomic>
#include <chrono>
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
constexpr Box kText[] = {{505, 85, 745, 220, true}, {340, 40, 500, 70, false}};

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
        return hash == kTitleHash ? video_on() : hash == kLoadScreenHash || hash == kOptionsHash;
    if (width == kWordW && height == kWordH)
        return hash == kLoadWordHash;
    return hidden(width, height, hash) != nullptr;
}

uint64_t titlevideo_generation(uint32_t width, uint32_t height) {
    return is_picture(width, height) && video_on() ? g.serial.load() : 1u;
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
        return 0;  // kept as it is
    }
    return hash == kTitleHash ? title_picture(rgba, width, height, out_rgba, out_width, out_height) : 0;
}

namespace {

int title_picture(const uint8_t* rgba, uint32_t width, uint32_t height, uint8_t** out_rgba, uint32_t* out_width,
                  uint32_t* out_height) {
    g.asked_ms = now_ms();
    std::vector<uint8_t> out;
    uint32_t ow, oh;
    out = video_frame(&ow, &oh);
    if (out.empty())
        return 2;  // the first frame on its way: the picture meanwhile
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
