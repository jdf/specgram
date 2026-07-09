// specgram: render a spectrogram of a WAV file to a PNG.
//
// Pipeline: libsndfile (decode) -> kfr (Hann-windowed STFT) -> cairo (render).

#include <cstdio>
#include <string>
#include <vector>

#include "dsp.h"
#include "render.h"

namespace {

using namespace specgram;

void print_usage(const char* prog) {
    std::fprintf(stderr,
                 "usage: %s [-p|--palette <name>] [-s|--size <width>x<height>] "
                 "[-t|--timescale] [-m|--mel] input.wav [output.png]\n",
                 prog);
    std::fprintf(stderr, "palettes:");
    for (const Palette& p : kPalettes)
        std::fprintf(stderr, " %s%s", p.name, &p == &kPalettes.front() ? " (default)" : "");
    std::fprintf(stderr, "\n");
}

}  // namespace

int main(int argc, char** argv) {
    RenderOptions opts;
    std::vector<const char*> paths;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p" || arg == "--palette") {
            if (++i == argc || !(opts.palette = find_palette(argv[i]))) {
                std::fprintf(stderr, "error: %s\n",
                             i == argc ? "missing palette name" : "unknown palette");
                print_usage(argv[0]);
                return 2;
            }
        } else if (arg == "-s" || arg == "--size") {
            int w = 0, h = 0;
            if (++i == argc || std::sscanf(argv[i], "%dx%d", &w, &h) != 2 ||
                opts.layout.plot_width_for(w) < 1 || opts.layout.plot_height_for(h) < 1) {
                std::fprintf(stderr, "error: size must be <width>x<height> with room for the\n"
                                     "plot beyond the %dx%d margins, e.g. 880x296\n",
                             opts.layout.margin_left + opts.layout.margin_right,
                             opts.layout.margin_top + opts.layout.margin_bottom);
                print_usage(argv[0]);
                return 2;
            }
            opts.layout.width = w;
            opts.layout.height = h;
        } else if (arg == "-t" || arg == "--timescale") {
            opts.timescale = true;
        } else if (arg == "-m" || arg == "--mel") {
            opts.freq_scale = FreqScale::kMel;
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

    if (!render_png(db, num_frames, num_bins, meta, opts, out_path.c_str())) return 1;

    std::printf("%s: %.2fs @ %d Hz -> %s (%zu frames x %zu bins)\n", in_path,
                meta.duration_seconds, meta.sample_rate, out_path.c_str(), num_frames, num_bins);
    return 0;
}
