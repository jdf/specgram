"""Generate a test WAV: 0->8kHz chirp + 440/2000 Hz tones + a noise burst.

usage: python3 make_test_wav.py [output.wav]
"""
import math
import random
import struct
import sys
import wave

SR = 44100
DUR = 4.0
N = int(SR * DUR)

samples = []
for i in range(N):
    t = i / SR
    # linear chirp 0 -> 8 kHz over the full duration
    chirp = 0.4 * math.sin(2 * math.pi * (8000 / (2 * DUR)) * t * t)
    # steady tones
    tones = 0.2 * math.sin(2 * math.pi * 440 * t) + 0.15 * math.sin(2 * math.pi * 2000 * t)
    # broadband noise burst from 2.0s to 2.3s
    noise = 0.3 * (random.random() * 2 - 1) if 2.0 <= t < 2.3 else 0.0
    samples.append(chirp + tones + noise)

out_path = sys.argv[1] if len(sys.argv) > 1 else "test.wav"
with wave.open(out_path, "wb") as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(SR)
    w.writeframes(b"".join(
        struct.pack("<h", max(-32767, min(32767, int(s * 32767 * 0.7)))) for s in samples))
print(f"wrote {out_path}")
