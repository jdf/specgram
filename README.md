# specgram

A small command-line tool that renders a spectrogram of an audio file to a
PNG. C++20, built with CMake and vcpkg; kfr for the FFT, libsndfile for
decoding, cairo for rendering.

![Spectrogram of a test signal: a rising chirp, two steady tones, and a
noise burst, with a timescale along the bottom](docs/example.png)

The same signal with `--mel`, which allocates vertical space by perceived
pitch instead of Hz:

![The same spectrogram with a mel-scale frequency axis, bending the chirp
into a curve](docs/example-mel.png)

## Usage

```
specgram [-p|--palette <name>] [-s|--size <width>x<height>]
         [-t|--timescale] [-m|--mel] input.wav [output.png]
```

- `-p, --palette` — `magma` (default), `dark`, or `light`. Magma is
  matplotlib's perceptually uniform colormap, sampled from the canonical
  table; `dark` and `light` are a single-hue blue ramp anchored on a dark
  or light chart surface. In every palette, silence recedes exactly into
  the background.
- `-s, --size` — overall image size (default `880x296`). The spectrogram
  is drawn into a plot rectangle inset by fixed margins.
- `-t, --timescale` — draw a time axis along the bottom. Tick spacing
  escalates 1 s → 10 s → 1 min, whichever first keeps ticks ≥ 20 px apart.
- `-m, --mel` — mel-scale frequency axis.

Input can be anything libsndfile reads (WAV, AIFF, FLAC, Ogg, ...);
multichannel audio is mixed down to mono.

## How it renders

Analysis is a Hann-windowed 1024-point STFT with a hop of 256, mapped to dB
with an 80 dB floor relative to the file's peak. Cells are rasterized at no
more than one column per plot pixel; when a long file has more STFT frames
than pixels, each column takes the per-bin **max** of its frames, so brief
transients stay visible instead of averaging away. The mel axis pools FFT
bins per row the same way.

## Building

Requires CMake ≥ 3.25, Ninja, and a [vcpkg](https://vcpkg.io) checkout —
either set `VCPKG_ROOT` or keep it at `~/vcpkg`. On macOS the first build
also needs autotools for a transitive dependency of fontconfig:

```
brew install autoconf autoconf-archive automake libtool
```

Then:

```
cmake --preset default
cmake --build --preset default
./build/specgram -t input.wav out.png
```

The first configure builds kfr, libsndfile, cairo, and their dependencies
through vcpkg, which takes a while; afterwards builds are incremental.

## Test signals

Two stdlib-only Python generators reproduce the images above and exercise
the renderer:

```
python3 tools/make_test_wav.py test.wav [duration_seconds]
python3 tools/make_levels_wav.py levels.wav
```

`make_test_wav.py` writes a chirp, two steady tones, and a noise burst —
features with known positions in time and frequency. `make_levels_wav.py`
writes eight tones stepped 10 dB apart to exercise the color ramp, plus a
fading tone for the continuous gradient.

## Provenance

Curious how this was built? The whole conversation is in
[TRANSCRIPT.md](TRANSCRIPT.md).

## License

MIT — see [LICENSE](LICENSE).
