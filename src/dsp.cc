#include "dsp.h"

#include <sndfile.h>

#include <kfr/dft.hpp>

#include <cmath>
#include <cstdio>

namespace specgram {

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

}  // namespace specgram
