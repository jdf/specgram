#include "render.h"

#include <cairo.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace specgram {
namespace {

constexpr float kDynamicRangeDb = 80.0f;

// Map t in [0,1] (0 = silence, 1 = peak energy) onto the palette's ramp.
uint32_t colormap(const Palette& palette, float t) {
    int n = static_cast<int>(palette.ramp.size());
    float pos = std::clamp(t, 0.0f, 1.0f) * (n - 1);
    int i = std::min(static_cast<int>(pos), n - 2);
    float frac = pos - i;
    auto lerp = [frac](uint8_t a, uint8_t b) {
        return static_cast<uint32_t>(std::lround(a + (b - a) * frac));
    };
    return lerp(palette.ramp[i].r, palette.ramp[i + 1].r) << 16 |
           lerp(palette.ramp[i].g, palette.ramp[i + 1].g) << 8 |
           lerp(palette.ramp[i].b, palette.ramp[i + 1].b);
}

// Renders the dB grid at native resolution (one pixel per STFT cell),
// low frequencies at the bottom.
cairo_surface_t* render_cells(const std::vector<float>& db, size_t num_frames, size_t num_bins,
                              const Palette& palette) {
    float peak = *std::max_element(db.begin(), db.end());

    cairo_surface_t* surface = cairo_image_surface_create(
        CAIRO_FORMAT_RGB24, static_cast<int>(num_frames), static_cast<int>(num_bins));
    cairo_surface_flush(surface);
    unsigned char* data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);

    for (size_t y = 0; y < num_bins; ++y) {
        auto* row = reinterpret_cast<uint32_t*>(data + y * stride);
        size_t bin = num_bins - 1 - y;
        for (size_t x = 0; x < num_frames; ++x) {
            float t = 1.0f + (db[x * num_bins + bin] - peak) / kDynamicRangeDb;
            row[x] = colormap(palette, t);
        }
    }
    cairo_surface_mark_dirty(surface);
    return surface;
}

// Draws a baseline along the bottom plot edge with ticks and mm:ss labels.
// Tick interval escalates 1s -> 10s -> 1min, taking the first that keeps
// ticks at least 20px apart; labels thin out further to stay readable.
void draw_timescale(cairo_t* cr, const Layout& layout, const AudioMeta& meta,
                    const Palette& palette) {
    constexpr double kMinTickSpacing = 20.0;
    constexpr double kMinLabelSpacing = 50.0;
    constexpr int kIntervals[] = {1, 10, 60};

    double px_per_second = layout.plot_width() / meta.duration_seconds;
    int interval = kIntervals[std::size(kIntervals) - 1];
    for (int candidate : kIntervals) {
        if (candidate * px_per_second >= kMinTickSpacing) {
            interval = candidate;
            break;
        }
    }
    double spacing = interval * px_per_second;
    int label_every = std::max(1, static_cast<int>(std::ceil(kMinLabelSpacing / spacing)));

    double baseline_y = layout.plot_y() + layout.plot_height() + 0.5;
    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgb(cr, palette.axis.r / 255.0, palette.axis.g / 255.0,
                         palette.axis.b / 255.0);
    cairo_move_to(cr, layout.plot_x(), baseline_y);
    cairo_line_to(cr, layout.plot_x() + layout.plot_width(), baseline_y);
    cairo_stroke(cr);
    for (int t = 0; t <= meta.duration_seconds; t += interval) {
        double x = std::floor(layout.plot_x() + t * px_per_second) + 0.5;
        cairo_move_to(cr, x, baseline_y);
        cairo_line_to(cr, x, baseline_y + 4.0);
    }
    cairo_stroke(cr);

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, palette.label.r / 255.0, palette.label.g / 255.0,
                         palette.label.b / 255.0);
    for (int t = 0, tick = 0; t <= meta.duration_seconds; t += interval, ++tick) {
        if (tick % label_every != 0) continue;
        char text[16];
        std::snprintf(text, sizeof(text), "%d:%02d", t / 60, t % 60);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, text, &ext);
        double x = layout.plot_x() + t * px_per_second;
        cairo_move_to(cr, x - ext.width / 2 - ext.x_bearing, baseline_y + 16.0);
        cairo_show_text(cr, text);
    }
}

}  // namespace

const std::vector<Palette> kPalettes = {
    {"dark",
     {0x1a, 0x1a, 0x19},
     {0x38, 0x38, 0x35},
     {0x89, 0x87, 0x81},
     {{0x1a, 0x1a, 0x19}, {0x0d, 0x36, 0x6b}, {0x10, 0x42, 0x81},
      {0x18, 0x4f, 0x95}, {0x1c, 0x5c, 0xab}, {0x25, 0x6a, 0xbf},
      {0x2a, 0x78, 0xd6}, {0x39, 0x87, 0xe5}, {0x55, 0x98, 0xe7},
      {0x6d, 0xa7, 0xec}, {0x86, 0xb6, 0xef}, {0x9e, 0xc5, 0xf4},
      {0xb7, 0xd3, 0xf6}, {0xcd, 0xe2, 0xfb}}},
    {"light",
     {0xfc, 0xfc, 0xfb},
     {0xc3, 0xc2, 0xb7},
     {0x89, 0x87, 0x81},
     {{0xfc, 0xfc, 0xfb}, {0xcd, 0xe2, 0xfb}, {0xb7, 0xd3, 0xf6},
      {0x9e, 0xc5, 0xf4}, {0x86, 0xb6, 0xef}, {0x6d, 0xa7, 0xec},
      {0x55, 0x98, 0xe7}, {0x39, 0x87, 0xe5}, {0x2a, 0x78, 0xd6},
      {0x25, 0x6a, 0xbf}, {0x1c, 0x5c, 0xab}, {0x18, 0x4f, 0x95},
      {0x10, 0x42, 0x81}, {0x0d, 0x36, 0x6b}}},
};

const Palette* find_palette(const char* name) {
    for (const Palette& p : kPalettes)
        if (std::string_view(name) == p.name) return &p;
    return nullptr;
}

bool render_png(const std::vector<float>& db, size_t num_frames, size_t num_bins,
                const Layout& layout, const AudioMeta& meta, const Palette& palette,
                bool timescale, const char* out_path) {
    cairo_surface_t* cells = render_cells(db, num_frames, num_bins, palette);

    cairo_surface_t* surface =
        cairo_image_surface_create(CAIRO_FORMAT_RGB24, layout.width, layout.height);
    cairo_t* cr = cairo_create(surface);

    cairo_set_source_rgb(cr, palette.background.r / 255.0, palette.background.g / 255.0,
                         palette.background.b / 255.0);
    cairo_paint(cr);

    double sx = static_cast<double>(layout.plot_width()) / static_cast<double>(num_frames);
    double sy = static_cast<double>(layout.plot_height()) / static_cast<double>(num_bins);
    cairo_save(cr);
    cairo_translate(cr, layout.plot_x(), layout.plot_y());
    cairo_scale(cr, sx, sy);
    cairo_set_source_surface(cr, cells, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    cairo_restore(cr);

    if (timescale) draw_timescale(cr, layout, meta, palette);

    cairo_status_t status = cairo_surface_write_to_png(surface, out_path);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    cairo_surface_destroy(cells);

    if (status != CAIRO_STATUS_SUCCESS) {
        std::fprintf(stderr, "error: writing '%s': %s\n", out_path, cairo_status_to_string(status));
        return false;
    }
    return true;
}

}  // namespace specgram
