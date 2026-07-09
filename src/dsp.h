// Audio decoding and STFT analysis.
#pragma once

#include <cstddef>
#include <vector>

namespace specgram {

constexpr size_t kFftSize = 1024;
constexpr size_t kHopSize = 256;

struct AudioMeta {
    int sample_rate = 0;
    double duration_seconds = 0.0;
};

// Reads a sound file and mixes it down to mono.
bool read_audio_mono(const char* path, std::vector<float>& mono, AudioMeta& meta);

// Computes the STFT and returns per-cell magnitudes in dB, frame-major
// (num_frames columns of num_bins values each).
std::vector<float> stft_db(const std::vector<float>& mono, size_t& num_frames, size_t& num_bins);

}  // namespace specgram
