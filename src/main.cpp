// specgram: render a spectrogram of a WAV file to a PNG.
//
// Pipeline: libsndfile (decode) -> kfr (Hann-windowed STFT) -> cairo (render).

#include <cairo.h>
#include <sndfile.h>

#include <kfr/dft.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr size_t kFftSize = 1024;
constexpr size_t kHopSize = 256;
constexpr float kDynamicRangeDb = 80.0f;

// Geometry of the output image. The spectrogram is drawn into the plot
// rectangle (inset by the margins); axes, tick labels, and a colorbar can
// later claim margin space without touching the spectrogram code.
struct Layout {
    int width = 880;
    int height = 296;
    int margin_left = 40;
    int margin_top = 20;
    int margin_right = 40;
    int margin_bottom = 40;

    int plot_x() const { return margin_left; }
    int plot_y() const { return margin_top; }
    int plot_width() const { return width - margin_left - margin_right; }
    int plot_height() const { return height - margin_top - margin_bottom; }
};

// Everything the decoration layer will eventually need to label the plot.
struct AudioMeta {
    int sample_rate = 0;
    double duration_seconds = 0.0;
};

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
    std::vector<Rgb> ramp;
};

const std::vector<Palette> kPalettes = {
    {"dark",
     {0x1a, 0x1a, 0x19},
     {{0x1a, 0x1a, 0x19}, {0x0d, 0x36, 0x6b}, {0x10, 0x42, 0x81},
      {0x18, 0x4f, 0x95}, {0x1c, 0x5c, 0xab}, {0x25, 0x6a, 0xbf},
      {0x2a, 0x78, 0xd6}, {0x39, 0x87, 0xe5}, {0x55, 0x98, 0xe7},
      {0x6d, 0xa7, 0xec}, {0x86, 0xb6, 0xef}, {0x9e, 0xc5, 0xf4},
      {0xb7, 0xd3, 0xf6}, {0xcd, 0xe2, 0xfb}}},
    {"light",
     {0xfc, 0xfc, 0xfb},
     {{0xfc, 0xfc, 0xfb}, {0xcd, 0xe2, 0xfb}, {0xb7, 0xd3, 0xf6},
      {0x9e, 0xc5, 0xf4}, {0x86, 0xb6, 0xef}, {0x6d, 0xa7, 0xec},
      {0x55, 0x98, 0xe7}, {0x39, 0x87, 0xe5}, {0x2a, 0x78, 0xd6},
      {0x25, 0x6a, 0xbf}, {0x1c, 0x5c, 0xab}, {0x18, 0x4f, 0x95},
      {0x10, 0x42, 0x81}, {0x0d, 0x36, 0x6b}}},
};

const Palette* find_palette(const char* name) {
    for (const Palette& p : kPalettes)
        if (std::string(name) == p.name) return &p;
    return nullptr;
}

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

// Reads a sound file and mixes it down to mono.
bool read_audio_mono(const char* path, std::vector<float>& mono, AudioMeta& meta) {
    SF_INFO info{};
    SNDFILE* snd = sf_open(path, SFM_READ, &info);
    if (!snd) {
        std::fprintf(stderr, "error: cannot open '%s': %s\n", path, sf_strerror(nullptr));
        return false;
    }

    std::vector<float> interleaved(static_cast<size_t>(info.frames) * info.channels);
    sf_count_t got = sf_readf_float(snd, interleaved.data(), info.frames);
    sf_close(snd);

    mono.resize(static_cast<size_t>(got));
    for (sf_count_t f = 0; f < got; ++f) {
        float sum = 0.0f;
        for (int c = 0; c < info.channels; ++c)
            sum += interleaved[static_cast<size_t>(f) * info.channels + c];
        mono[static_cast<size_t>(f)] = sum / static_cast<float>(info.channels);
    }

    meta.sample_rate = info.samplerate;
    meta.duration_seconds = static_cast<double>(got) / info.samplerate;
    return true;
}

// Computes the STFT and returns per-cell magnitudes in dB, frame-major
// (frames columns of num_bins values each).
std::vector<float> stft_db(const std::vector<float>& mono, size_t& num_frames, size_t& num_bins) {
    num_bins = kFftSize / 2 + 1;
    num_frames = mono.size() >= kFftSize ? (mono.size() - kFftSize) / kHopSize + 1 : 0;

    std::vector<float> window(kFftSize);
    for (size_t i = 0; i < kFftSize; ++i)
        window[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (kFftSize - 1)));

    kfr::dft_plan_real<float> plan(kFftSize);
    kfr::univector<float> in(kFftSize);
    kfr::univector<kfr::complex<float>> out(num_bins);
    kfr::univector<kfr::u8> temp(plan.temp_size);

    std::vector<float> db(num_frames * num_bins);
    for (size_t f = 0; f < num_frames; ++f) {
        for (size_t i = 0; i < kFftSize; ++i)
            in[i] = mono[f * kHopSize + i] * window[i];
        plan.execute(out, in, temp);
        for (size_t b = 0; b < num_bins; ++b)
            db[f * num_bins + b] = 20.0f * std::log10(kfr::cabs(out[b]) + 1e-10f);
    }
    return db;
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

// Composites the spectrogram into the plot rectangle. Decorations (axes,
// labels, colorbar) belong here, drawn against `layout` and `meta`.
bool render_image(cairo_surface_t* cells, const Layout& layout, const AudioMeta& meta,
                  const Palette& palette, const char* out_path) {
    (void)meta;  // reserved for axis labels

    cairo_surface_t* surface =
        cairo_image_surface_create(CAIRO_FORMAT_RGB24, layout.width, layout.height);
    cairo_t* cr = cairo_create(surface);

    cairo_set_source_rgb(cr, palette.background.r / 255.0, palette.background.g / 255.0,
                         palette.background.b / 255.0);
    cairo_paint(cr);

    double sx = static_cast<double>(layout.plot_width()) / cairo_image_surface_get_width(cells);
    double sy = static_cast<double>(layout.plot_height()) / cairo_image_surface_get_height(cells);
    cairo_save(cr);
    cairo_translate(cr, layout.plot_x(), layout.plot_y());
    cairo_scale(cr, sx, sy);
    cairo_set_source_surface(cr, cells, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    cairo_restore(cr);

    cairo_status_t status = cairo_surface_write_to_png(surface, out_path);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    if (status != CAIRO_STATUS_SUCCESS) {
        std::fprintf(stderr, "error: writing '%s': %s\n", out_path, cairo_status_to_string(status));
        return false;
    }
    return true;
}

}  // namespace

void print_usage(const char* prog) {
    std::fprintf(stderr, "usage: %s [-p|--palette <name>] input.wav [output.png]\n", prog);
    std::fprintf(stderr, "palettes:");
    for (const Palette& p : kPalettes)
        std::fprintf(stderr, " %s%s", p.name, &p == &kPalettes.front() ? " (default)" : "");
    std::fprintf(stderr, "\n");
}

int main(int argc, char** argv) {
    const Palette* palette = &kPalettes.front();
    std::vector<const char*> paths;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p" || arg == "--palette") {
            if (++i == argc || !(palette = find_palette(argv[i]))) {
                std::fprintf(stderr, "error: %s\n",
                             i == argc ? "missing palette name" : "unknown palette");
                print_usage(argv[0]);
                return 2;
            }
        } else {
            paths.push_back(argv[i]);
        }
    }
    if (paths.empty() || paths.size() > 2) {
        print_usage(argv[0]);
        return 2;
    }
    const char* in_path = paths[0];
    std::string out_path = paths.size() == 2 ? paths[1] : "spectrogram.png";

    std::vector<float> mono;
    AudioMeta meta;
    if (!read_audio_mono(in_path, mono, meta)) return 1;

    size_t num_frames = 0, num_bins = 0;
    std::vector<float> db = stft_db(mono, num_frames, num_bins);
    if (num_frames == 0) {
        std::fprintf(stderr, "error: '%s' is shorter than one FFT window (%zu samples)\n",
                     in_path, kFftSize);
        return 1;
    }

    cairo_surface_t* cells = render_cells(db, num_frames, num_bins, *palette);
    Layout layout;
    bool ok = render_image(cells, layout, meta, *palette, out_path.c_str());
    cairo_surface_destroy(cells);
    if (!ok) return 1;

    std::printf("%s: %.2fs @ %d Hz -> %s (%zu frames x %zu bins)\n", in_path,
                meta.duration_seconds, meta.sample_rate, out_path.c_str(), num_frames, num_bins);
    return 0;
}
