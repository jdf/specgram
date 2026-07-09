// Spectrogram rendering: palettes, layout, and PNG output. Cairo stays an
// implementation detail of render.cc.
#pragma once

#include <cstdint>
#include <vector>

#include "dsp.h"

namespace specgram {

struct Rgb {
    uint8_t r, g, b;
};

// A palette is a chart surface plus a sequential ramp running silence ->
// peak energy, anchored so silence recedes into the surface. Both are the
// same single-hue blue ramp (steps 100..700), re-anchored per surface:
// dark surfaces run toward light blue at peak, light surfaces toward dark.
struct Palette {
    const char* name;
    Rgb background;
    Rgb axis;   // baseline and ticks
    Rgb label;  // muted text
    std::vector<Rgb> ramp;
};

extern const std::vector<Palette> kPalettes;

// Returns the palette with the given name, or nullptr.
const Palette* find_palette(const char* name);

// Geometry of the output image. The spectrogram is drawn into the plot
// rectangle (inset by the margins); axes, tick labels, and a colorbar can
// claim margin space without touching the spectrogram code.
struct Layout {
    int width = 880;
    int height = 296;
    int margin_left = 40;
    int margin_top = 20;
    int margin_right = 40;
    int margin_bottom = 40;

    int plot_x() const { return margin_left; }
    int plot_y() const { return margin_top; }
    int plot_width_for(int w) const { return w - margin_left - margin_right; }
    int plot_height_for(int h) const { return h - margin_top - margin_bottom; }
    int plot_width() const { return plot_width_for(width); }
    int plot_height() const { return plot_height_for(height); }
};

// Vertical mapping of frequency to plot rows: equal Hz per row, or equal
// mel per row (compresses highs, expands the perceptually busy lows).
enum class FreqScale { kLinear, kMel };

struct RenderOptions {
    Layout layout;
    const Palette* palette = &kPalettes.front();
    bool timescale = false;
    FreqScale freq_scale = FreqScale::kLinear;
};

// Renders the dB grid (as produced by stft_db) to a PNG at out_path.
bool render_png(const std::vector<float>& db, size_t num_frames, size_t num_bins,
                const AudioMeta& meta, const RenderOptions& opts, const char* out_path);

}  // namespace specgram
