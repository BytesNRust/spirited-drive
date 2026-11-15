#!/usr/bin/env python3
"""The tyre spray loop (data/sounds/rain_spray.wav) from a recording of cars passing on a wet road.

    rain_spray_from_passes.py RECORDING OUT.wav [--report]

RECORDING is any file ffmpeg reads (the owner's mp3 of cars passing a standing microphone). Needs ffmpeg on the PATH; the
rest is the standard library, and the run is the same every time (fixed dice).

1. Decoded to mono at 22050 Hz.
2. The passes: the 50 ms level, smoothed; every peak at least PASS_ABOVE_DB over the recording's median (its background)
   and PASS_APART_S from a louder one. Each pass's core is the stretch within CORE_DB of its peak, at most CORE_MAX_S.
3. Each core is straightened:
   - the Doppler: the fall from the first third of the pass to the last (the shift of their log-frequency spectra) is taken
     off as a ramp, the closest point's pitch kept; a pass whose fall is not a plausible Doppler fall is left as it is;
   - the pass's swell: divided by its own level (a 120 ms envelope), so it is flat;
   - its loudness: every core to the same RMS.
4. The cores are cut into grains and joined round a ring of LOOP_S with equal-power cross-fades, in an order the dice
   choose, so the loop has no seam.
5. A bed of noise with the cores' own spectrum (their average, by FFT, shaped by an FIR through ffmpeg's afir) is mixed
   under it at BED_SHARE of the power, with a slow random swell, so the grains' repetition is not heard.
6. Level: RMS TARGET_RMS_DB, peaks rounded softly (tanh) under PEAK_DB: the loop it replaces had them.
"""
import array
import cmath
import math
import random
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

RATE = 22050
HOP = RATE // 20                      # the level's step: 50 ms
PASS_ABOVE_DB, PASS_APART_S = 15.0, 2.0
CORE_DB, CORE_MAX_S = 6.0, 1.2
FFT_N, TRACK_HOP = 2048, RATE // 25   # the Doppler's spectra: 93 ms windows every 40 ms
LOG_BINS, LOG_LO, LOG_HI = 480, 250.0, 8000.0
DOPPLER_MIN, DOPPLER_MAX = 0.01, 0.12  # a fall the pass can have: 1 % (slow, far) to 12 % (some 23 m/s close by)
ENVELOPE_S = 0.12
GRAIN_S, FADE_S, LOOP_S = 0.45, 0.15, 12.0
BED_SHARE, BED_SWELL_DB = 0.5, 2.0
FIR_TAPS = 1025
TARGET_RMS_DB, PEAK_DB = -23.4, -6.9
SEED = 66


def decode(path):
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", str(path), "-ac", "1", "-ar", str(RATE), "-f", "s16le", "-"],
                         check=True, capture_output=True).stdout
    pcm = array.array("h", raw)
    return [s / 32768.0 for s in pcm]


def rms(x):
    return math.sqrt(sum(v * v for v in x) / len(x)) if x else 0.0


def db(v):
    return 20.0 * math.log10(max(v, 1e-9))


def fft(a):
    """In-place iterative radix-2 FFT of a list of complex numbers (len a power of two)."""
    n = len(a)
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j |= bit
        if i < j:
            a[i], a[j] = a[j], a[i]
    size = 2
    while size <= n:
        w = cmath.exp(-2j * math.pi / size)
        half = size // 2
        for start in range(0, n, size):
            wk = 1.0
            for k in range(half):
                u, t = a[start + k], wk * a[start + k + half]
                a[start + k], a[start + k + half] = u + t, u - t
                wk *= w
        size <<= 1
    return a


HANN = [0.5 - 0.5 * math.cos(2 * math.pi * i / FFT_N) for i in range(FFT_N)]


def power_spectrum(x, at):
    frame = [complex(x[at + i] * HANN[i]) if at + i < len(x) else 0j for i in range(FFT_N)]
    spec = fft(frame)
    return [abs(spec[k]) ** 2 for k in range(FFT_N // 2 + 1)]


def find_passes(x):
    levels = [db(rms(x[i:i + HOP])) for i in range(0, len(x) - HOP, HOP)]
    smooth = [sum(levels[max(0, i - 2):i + 3]) / len(levels[max(0, i - 2):i + 3]) for i in range(len(levels))]
    floor = sorted(smooth)[len(smooth) // 2]
    peaks = sorted((v, i) for i, v in enumerate(smooth) if v > floor + PASS_ABOVE_DB
                   and v == max(smooth[max(0, i - 4):i + 5]))
    chosen = []
    for v, i in reversed(peaks):
        if all(abs(i - j) * HOP / RATE >= PASS_APART_S for _, j in chosen):
            chosen.append((v, i))
    cores = []
    half_max = int(CORE_MAX_S / 2 * RATE / HOP)
    for v, i in sorted(chosen, key=lambda p: p[1]):
        lo = i
        while lo > 0 and i - lo < half_max and smooth[lo - 1] > v - CORE_DB:
            lo -= 1
        hi = i
        while hi < len(smooth) - 1 and hi - i < half_max and smooth[hi + 1] > v - CORE_DB:
            hi += 1
        cores.append((lo * HOP, (hi + 1) * HOP, v, floor))
    return cores


LOG_RATIO = (LOG_HI / LOG_LO) ** (1.0 / LOG_BINS)


def log_spectrum(x, at):
    p = power_spectrum(x, at)
    out = []
    for b in range(LOG_BINS):
        f = LOG_LO * LOG_RATIO ** b
        k = f * FFT_N / RATE
        k0 = int(k)
        frac = k - k0
        out.append(math.log10(p[k0] * (1 - frac) + p[k0 + 1] * frac + 1e-12))
    mean = sum(out) / len(out)
    return [v - mean for v in out]


def shift_between(a, b, reach=12):
    """The log-frequency shift (in bins, sub-bin) that best lays b on a."""
    scores = []
    for s in range(-reach, reach + 1):
        lo, hi = max(0, s), min(LOG_BINS, LOG_BINS + s)
        scores.append(sum(a[i] * b[i - s] for i in range(lo, hi)) / (hi - lo))
    best = max(range(len(scores)), key=lambda i: scores[i])
    if 0 < best < len(scores) - 1:
        l, c, r = scores[best - 1], scores[best], scores[best + 1]
        den = l - 2 * c + r
        best += 0.5 * (l - r) / den if den else 0.0
    return best - reach


def straighten_doppler(core):
    """The core resampled against its Doppler fall; returns it and the fall straightened (0 when none was).

    The spray is broadband: a window-by-window pitch track is noise that wanders either way (measured on the owner's
    recording: up and down by 10-20 % with no Doppler shape), so only the whole pass is measured, the first third's average
    log spectrum against the last third's. A fall of DOPPLER_MIN to DOPPLER_MAX (a car going away is heard lower) is taken
    off as a straight ramp through the pass, its middle (the closest point) left where it is; anything else is not a
    Doppler fall and the core is left as it is."""
    starts = list(range(0, len(core) - FFT_N, TRACK_HOP))
    third = len(starts) // 3
    if third < 2:
        return core, 0.0
    specs = [log_spectrum(core, s) for s in starts]
    mean = lambda group: [sum(v) / len(group) for v in zip(*group)]
    fall = -shift_between(mean(specs[:third]), mean(specs[-third:]), reach=30)   # log-bins, last third against the first
    ratio = LOG_RATIO ** fall
    if not DOPPLER_MIN <= 1.0 - ratio <= DOPPLER_MAX:
        return core, 0.0
    first, last = starts[third // 2] + FFT_N // 2, starts[-third + third // 2] + FFT_N // 2
    out, pos, n = [], 0.0, len(core)
    while pos < n - 1:
        t = (pos - (first + last) / 2) / (last - first)   # -0.5 .. 0.5 between the two thirds' middles
        r = LOG_RATIO ** (fall * t)
        k = int(pos)
        frac = pos - k
        out.append(core[k] * (1 - frac) + core[k + 1] * frac)
        pos += 1.0 / r                             # a stretch heard r times higher is read r times slower: lowered back
    return out, 1.0 - ratio


def flatten(core):
    half = int(ENVELOPE_S * RATE / 2)
    power = [v * v for v in core]
    cum = [0.0]
    for p in power:
        cum.append(cum[-1] + p)
    out = []
    for i, v in enumerate(core):
        lo, hi = max(0, i - half), min(len(core), i + half)
        level = math.sqrt((cum[hi] - cum[lo]) / (hi - lo))
        out.append(v / max(level, 1e-6))
    return out


def ring(grains, rng):
    """Grains joined round a ring of LOOP_S with equal-power cross-fades: the end fades into the start."""
    total, fade = int(LOOP_S * RATE), int(FADE_S * RATE)
    out = [0.0] * total
    pos = 0
    order = []
    while pos < total:
        if not order:
            order = list(range(len(grains)))
            rng.shuffle(order)
        g = grains[order.pop()]
        for i, v in enumerate(g):
            if i < fade:
                v *= math.sin(0.5 * math.pi * i / fade)
            elif i >= len(g) - fade:
                v *= math.cos(0.5 * math.pi * (i - (len(g) - fade)) / fade)
            out[(pos + i) % total] += v
        pos += len(g) - fade
    return out


def bed(cores, rng, tmp):
    """Noise with the cores' average spectrum: an FIR built from it, run over white noise by ffmpeg's afir."""
    avg = [0.0] * (FFT_N // 2 + 1)
    count = 0
    for c in cores:
        for s in range(0, len(c) - FFT_N, FFT_N // 2):
            for k, v in enumerate(power_spectrum(c, s)):
                avg[k] += v
            count += 1
    mag = [math.sqrt(v / max(count, 1)) for v in avg]
    full = [complex(m) for m in mag] + [complex(m) for m in reversed(mag[1:-1])]
    taps = fft([v.conjugate() for v in full])     # inverse by the conjugate trick (real, even input)
    taps = [t.real / FFT_N for t in taps]
    half = FIR_TAPS // 2
    ir = [taps[(i - half) % FFT_N] * (0.5 + 0.5 * math.cos(math.pi * (i - half) / (half + 1))) for i in range(FIR_TAPS)]
    total = int(LOOP_S * RATE) + FIR_TAPS
    noise = [rng.gauss(0.0, 0.25) for _ in range(total)]
    write_wav(tmp / "noise.wav", noise, clip=True)
    write_wav(tmp / "ir.wav", [v / max(abs(t) for t in ir) * 0.9 for v in ir], clip=True)
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(tmp / "noise.wav"), "-i", str(tmp / "ir.wav"),
                    "-filter_complex", "[0][1]afir=gtype=none", str(tmp / "bed.wav")], check=True)
    shaped = read_wav(tmp / "bed.wav")[FIR_TAPS:FIR_TAPS + int(LOOP_S * RATE)]
    n, fade = len(shaped), int(FADE_S * RATE)
    looped = shaped[:]
    for i in range(fade):                          # its end into its start
        a = math.cos(0.5 * math.pi * i / fade)
        looped[i] = shaped[i] * math.sin(0.5 * math.pi * i / fade) + shaped[n - fade + i] * a
    swell_points = [rng.uniform(-BED_SWELL_DB, BED_SWELL_DB) for _ in range(12)]
    out = []
    for i, v in enumerate(looped):
        t = i / n * len(swell_points)
        k = int(t)
        f = t - k
        d = swell_points[k % len(swell_points)] * (1 - f) + swell_points[(k + 1) % len(swell_points)] * f
        out.append(v * 10 ** (d / 20))
    return out


def write_wav(path, x, clip=False):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(array.array("h", (int(max(-1.0, min(1.0, v)) * 32767) for v in x)).tobytes())


def read_wav(path):
    with wave.open(str(path), "rb") as w:
        return [s / 32768.0 for s in array.array("h", w.readframes(w.getnframes()))]


def scaled(x, rms_db):
    g = 10 ** (rms_db / 20) / max(rms(x), 1e-9)
    return [v * g for v in x]


def soft_peaks(x):
    limit, knee = 10 ** (PEAK_DB / 20), 0.5 * 10 ** (PEAK_DB / 20)
    out = []
    for v in x:
        a = abs(v)
        if a > knee:
            a = knee + (limit - knee) * math.tanh((a - knee) / (limit - knee))
        out.append(math.copysign(a, v))
    return out


def main(argv):
    if len(argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2
    source, target, report = Path(argv[1]), Path(argv[2]), "--report" in argv
    rng = random.Random(SEED)
    x = decode(source)
    passes = find_passes(x)
    if len(passes) < 3:
        print(f"only {len(passes)} passes found: not enough for a loop", file=sys.stderr)
        return 1
    cores, grains = [], []
    for lo, hi, peak, floor in passes:
        straight, fall = straighten_doppler(x[lo:hi])
        flat = flatten(straight)
        core = scaled(flat, -20.0)
        cores.append(core)
        g = int(GRAIN_S * RATE)
        for s in range(0, len(core) - g + 1, g // 2):
            grains.append(core[s:s + g])
        if report:
            print(f"pass {lo / RATE:6.2f}-{hi / RATE:6.2f} s  peak {peak:5.1f} dB  floor {floor:5.1f} dB  " +
                  (f"Doppler fall taken off {100 * fall:4.1f} %" if fall else "no Doppler fall measured: left as it is"))
    looped = ring(grains, rng)
    with tempfile.TemporaryDirectory() as tmp:
        noise = bed(cores, rng, Path(tmp))
    rec = scaled(looped, 0.0)
    noise = scaled(noise, 0.0)
    mixed = [math.sqrt(1 - BED_SHARE) * a + math.sqrt(BED_SHARE) * b for a, b in zip(rec, noise)]
    out = soft_peaks(scaled(mixed, TARGET_RMS_DB))
    write_wav(target, out)
    if report:
        print(f"{len(passes)} passes, {len(grains)} grains, loop {LOOP_S} s, rms {db(rms(out)):.1f} dBFS, "
              f"peak {db(max(abs(v) for v in out)):.1f} dBFS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
