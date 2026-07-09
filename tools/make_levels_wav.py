"""Generate a WAV exercising the level->color mapping of the spectrogram.

Eight steady tones spread across the spectrum, each 10 dB quieter than the
last (0 dB at 1 kHz down to -70 dB at 18.5 kHz), so each renders as a
horizontal line in a distinct ramp color, the quietest fading into the
background. A 500 Hz tone fades in linearly to show the continuous gradient.

usage: python3 make_levels_wav.py [output.wav]
"""
import math
import struct
import sys
import wave

SR = 44100
DUR = 4.0
N = int(SR * DUR)

samples = []
for i in range(N):
    t = i / SR
    s = 0.0
    for k in range(8):
        freq = 1000 + k * 2500
        amp = 10 ** (-10 * k / 20)
        s += amp * math.sin(2 * math.pi * freq * t)
    # continuous-gradient demo: 500 Hz fading in over the full duration
    s += (t / DUR) * math.sin(2 * math.pi * 500 * t)
    samples.append(s)

peak = max(abs(s) for s in samples)
out_path = sys.argv[1] if len(sys.argv) > 1 else "levels.wav"
with wave.open(out_path, "wb") as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(SR)
    w.writeframes(b"".join(
        struct.pack("<h", int(s / peak * 0.9 * 32767)) for s in samples))
print(f"wrote {out_path}")
