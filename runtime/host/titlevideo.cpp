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

}  // namespace

bool titlevideo_claims(uint32_t format, uint32_t width, uint32_t height) {
    if (!g_settings.mod_title_video || format != kCmpr || width != kTitleW || height != kTitleH)
        return false;
    start();
    return !g.missing;
}

uint64_t titlevideo_generation() {
    return g.serial.load();
}

int titlevideo_apply(const uint8_t* rgba, uint32_t width, uint32_t height, uint8_t** out_rgba,
                     uint32_t* out_width, uint32_t* out_height) {
    g.asked_ms = now_ms();
    if (width != kTitleW || height != kTitleH)
        return 0;
    std::vector<uint8_t> out;
    uint32_t ow, oh;
    {
        std::lock_guard<std::mutex> lock(g.mutex);
        if (g.frame.empty())
            return 2;  // the first frame on its way: the picture meanwhile
        out = g.frame;
        ow = g.width;
        oh = g.height;
    }
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
    *out_rgba = static_cast<uint8_t*>(std::malloc(out.size()));
    if (*out_rgba == nullptr)
        return 0;
    std::memcpy(*out_rgba, out.data(), out.size());
    *out_width = ow;
    *out_height = oh;
    return 1;
}
