// OT fork Sample Editor tools from the open-source list (sample_fx_ot.h):
//
//  * Tuner: YIN pitch detection (de Cheveigne & Kawahara 2002) on the
//    selection, the median of a few frames; the editor writes Tune and
//    Finetune so the unity note plays in tune.
//  * Paulstretch (after Paul Nasca's algorithm): windows of the input, their
//    magnitudes kept and phases randomised, overlap-added at a slower pace.
//  * Granular (after Ourednik's Argotlunar): grains with size, density,
//    position spray, pitch spread and reverse probability, rendered over a
//    stretched length.
//  * Strand (after Leviathan's Strand): the selection cut at rising zero
//    crossings into segments, rebuilt with shuffles, repeats and reversals
//    (no clicks: every cut is at a zero crossing).
// The heavy ones work in a scratch area the editor lends (setScratch), part of
// the sample memory's free tail.
#include <cmath>
#include <cstring>

#include "app/sample_fx.h"
#include "app/sample_fx_ot.h"

namespace ot {
namespace sfx {

namespace {

constexpr float kPi = 3.14159265358979f;
float* gScratch = nullptr;
uint32_t gScratchFloats = 0;
void (*gProgressFn)(void*, int) = nullptr;
void* gProgressCtx = nullptr;
void reportProgress(uint64_t done, uint64_t total) {
    if (gProgressFn && total) gProgressFn(gProgressCtx, int(done * 100u / total));
}

inline int16_t sat16f(float v) {
    if (!(v == v)) return 0;
    return int16_t(v > 32767.0f ? 32767 : (v < -32768.0f ? -32768 : int32_t(v)));
}

// Deterministic hash RNG.
inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1u) {}
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float uni() { return float(next() >> 8) * (1.0f / 16777216.0f); }
};

// YIN on frame x[0..2W): the period in samples (0 = none) and its clarity.
float yinPeriod(const int16_t* x, int W, int tauMin, int tauMax, float* clarity) {
    float d[1100];
    if (tauMax > 1099) tauMax = 1099;
    d[0] = 1.0f;
    float run = 0.0f;
    int best = 0;
    for (int tau = 1; tau <= tauMax; ++tau) {
        float s = 0.0f;
        for (int j = 0; j < W; ++j) {
            const float e = float(x[j]) - float(x[j + tau]);
            s += e * e;
        }
        run += s;
        d[tau] = run > 0.0f ? s * float(tau) / run : 1.0f;   // cumulative mean normalised difference
    }
    for (int tau = tauMin; tau <= tauMax; ++tau)
        if (d[tau] < 0.15f) {
            while (tau + 1 <= tauMax && d[tau + 1] < d[tau]) ++tau;
            best = tau;
            break;
        }
    if (!best) {   // no dip under the threshold: the global minimum
        best = tauMin;
        for (int tau = tauMin; tau <= tauMax; ++tau)
            if (d[tau] < d[best]) best = tau;
        if (d[best] > 0.4f) {
            *clarity = 0.0f;
            return 0.0f;
        }
    }
    *clarity = 1.0f - d[best];
    // Parabolic interpolation.
    float p = float(best);
    if (best > 1 && best < tauMax) {
        const float a = d[best - 1], b = d[best], c = d[best + 1];
        const float den = a - 2.0f * b + c;
        if (den != 0.0f) p += 0.5f * (a - c) / den;
    }
    return p;
}

}  // namespace

const char* const kPsWindows[4] = {"50 ms", "100 ms", "200 ms", "400 ms"};

void setScratch(float* p, uint32_t floats) {
    gScratch = p;
    gScratchFloats = floats;
}

void setProgress(void (*fn)(void* ctx, int percent), void* ctx) {
    gProgressFn = fn;
    gProgressCtx = ctx;
}

uint32_t scratchFloats(int fx, const float* p) {
    (void)p;
    if (fx == kPaulstretch) return 4u * 16384u + 8192u;   // re, im, window, overlap (largest window)
    return 0;
}

bool detectPitch(const int16_t* in, uint32_t n, float* hz, float* clarity) {
    *hz = 0.0f;
    *clarity = 0.0f;
    constexpr int W = 1024;                       // integration window
    constexpr int tauMin = 20, tauMax = 1050;     // ~2.2 kHz .. 42 Hz
    if (n < uint32_t(W + tauMax + 1)) return false;
    // Frames from 10 % into the selection (past the attack), up to five.
    float per[5];
    int np = 0;
    const uint32_t span = n - uint32_t(W + tauMax + 1);
    for (int k = 0; k < 5; ++k) {
        const uint32_t at = span / 10 + uint32_t(uint64_t(span) * 8 / 10 * uint32_t(k) / 4);
        // Skip near-silent frames.
        int32_t pk = 0;
        for (int j = 0; j < W; j += 4) pk = std::abs(in[at + j]) > pk ? std::abs(in[at + j]) : pk;
        if (pk < 200) continue;
        float c = 0.0f;
        const float p = yinPeriod(in + at, W, tauMin, tauMax, &c);
        if (p > 0.0f && c > 0.5f) {
            per[np++] = p;
            *clarity += c;
        }
    }
    if (!np) return false;
    // Median period.
    for (int i = 1; i < np; ++i)
        for (int j = i; j > 0 && per[j] < per[j - 1]; --j) {
            const float t = per[j];
            per[j] = per[j - 1];
            per[j - 1] = t;
        }
    *hz = 44100.0f / per[np / 2];
    *clarity /= float(np);
    return true;
}

void tuneFor(float hz, int* tune, int* fine) {
    // The unity note (60, a C) plays the sample at its own pitch: move the
    // detected pitch to the nearest C pitch class (within +-6 semitones).
    const float midi = 69.0f + 12.0f * std::log2(hz / 440.0f);
    float d = 60.0f - midi;
    d -= 12.0f * std::floor((d + 6.0f) / 12.0f);
    int t = int(std::floor(d + 0.5f));
    int f = int(std::floor((d - float(t)) * 100.0f + 0.5f));
    if (f > 100) f = 100;
    if (f < -100) f = -100;
    *tune = t;
    *fine = f;
}

uint32_t paulstretchFrames(const float* p, uint32_t n) {
    const uint64_t m = uint64_t(n) * uint64_t(p[0] < 1.0f ? 1.0f : p[0]);
    return m > 0x1E0000u ? 0x1E0000u : uint32_t(m);
}

// Two windows per FFT (OT fork, after a user report of minutes on the
// device): frame A in the real part, frame B in the imaginary part; their
// spectra separate by symmetry, get random phases, and go back in one
// inverse FFT as A + iB (both real). Hop N / 2 as in Paul Nasca's original,
// Hann analysis and sine (sqrt-Hann) synthesis windows: the synthesis
// windows' squares sum to 1, so the decorrelated frames add up to an even
// level. A random phase is a random point of the square, normalised: one
// square root per bin and no trigonometry.
uint32_t paulstretch(const float* p, const int16_t* in, uint32_t n, int16_t* out, uint32_t m) {
    const float stretch = p[0] < 1.0f ? 1.0f : p[0];
    static const int kN[4] = {2048, 4096, 8192, 16384};
    int N = kN[int(p[1]) & 3];
    auto need = [](int w) { return uint32_t(4 * w + w / 2); };   // re, im, window, overlap (1.5 N)
    while (N > 1024 && (uint32_t(N) > n * 2 || need(N) > gScratchFloats)) N /= 2;
    if (!gScratch || gScratchFloats < need(N)) {
        for (uint32_t i = 0; i < m; ++i) out[i] = i < n ? in[i] : 0;
        return m;
    }
    float* re = gScratch;
    float* im = re + N;
    float* win = im + N;   // sin(pi i / N): synthesis; its square (Hann) for the analysis
    float* acc = win + N;  // 1.5 N: the pair's output, the second half carried over
    const int hop = N / 2;
    for (int i = 0; i < N; ++i) win[i] = std::sin(kPi * float(i) / float(N));
    std::memset(acc, 0, sizeof(float) * size_t(N + hop));
    // Random-phase frames keep the windowed input's energy spread over the
    // frame: Hann^2 averages 3/8, so sqrt(8/3), and 1/N for the inverse FFT.
    const float gain = 1.6329932f / float(N);
    Rng rng(0x5EED1234u);
    const double inStep = double(hop) / double(stretch);
    uint32_t written = 0;
    for (uint32_t frame = 0; written < m; frame += 2) {
        // Frames `frame` (A) and `frame + 1` (B), centred on their input point.
        const int64_t a0 = int64_t(double(frame) * inStep) - N / 2;
        const int64_t b0 = int64_t(double(frame + 1) * inStep) - N / 2;
        for (int i = 0; i < N; ++i) {
            const float w = win[i] * win[i];
            const int64_t ka = a0 + i, kb = b0 + i;
            re[i] = (ka >= 0 && ka < int64_t(n) ? float(in[ka]) : 0.0f) * w;
            im[i] = (kb >= 0 && kb < int64_t(n) ? float(in[kb]) : 0.0f) * w;
        }
        fftInPlace(re, im, N, false);
        // DC and Nyquist: real for both frames; dropped (no DC offset).
        re[0] = im[0] = 0.0f;
        re[N / 2] = im[N / 2] = 0.0f;
        for (int k = 1; k < N / 2; ++k) {
            const float zr = re[k], zi = im[k], wr = re[N - k], wi = im[N - k];
            // A = (Z[k] + conj Z[N-k]) / 2, B = (Z[k] - conj Z[N-k]) / 2i.
            const float ar = 0.5f * (zr + wr), ai = 0.5f * (zi - wi);
            const float br = 0.5f * (zi + wi), bi = 0.5f * (wr - zr);
            // New phases: random unit vectors, magnitudes kept.
            float u = rng.uni() * 2.0f - 1.0f, v = rng.uni() * 2.0f - 1.0f;
            float q = u * u + v * v;
            if (q < 1e-6f) u = 1.0f, v = 0.0f, q = 1.0f;
            const float sa = std::sqrt((ar * ar + ai * ai) / q);
            const float yar = sa * u, yai = sa * v;
            float u2 = rng.uni() * 2.0f - 1.0f, v2 = rng.uni() * 2.0f - 1.0f;
            float q2 = u2 * u2 + v2 * v2;
            if (q2 < 1e-6f) u2 = 1.0f, v2 = 0.0f, q2 = 1.0f;
            const float sb = std::sqrt((br * br + bi * bi) / q2);
            const float ybr = sb * u2, ybi = sb * v2;
            // Z' = YA + i YB at k, conj(YA) + i conj(YB) at N - k.
            re[k] = yar - ybi;
            im[k] = yai + ybr;
            re[N - k] = yar + ybi;
            im[N - k] = ybr - yai;
        }
        fftInPlace(re, im, N, true);
        for (int i = 0; i < N; ++i) {
            const float w = win[i] * gain;
            acc[i] += re[i] * w;          // frame A at 0
            acc[hop + i] += im[i] * w;    // frame B half a window later
        }
        // acc[0 .. N) is complete (the next pair starts N later): out. The
        // output starts half a window in (the first frame is centred on 0).
        const int64_t outStart = int64_t(frame) * hop - N / 2;
        for (int i = 0; i < N; ++i) {
            const int64_t o = outStart + i;
            if (o >= 0 && o < int64_t(m)) {
                out[o] = sat16f(acc[i]);
                written = uint32_t(o + 1);
            }
        }
        std::memmove(acc, acc + N, sizeof(float) * size_t(hop));
        std::memset(acc + hop, 0, sizeof(float) * size_t(N));
        reportProgress(written, m);
        if (outStart > int64_t(m) + N) break;
    }
    for (uint32_t i = written; i < m; ++i) out[i] = 0;
    return m;
}

uint32_t granularFrames(const float* p, uint32_t n) {
    const uint64_t m = uint64_t(double(n) * double(p[0]));
    return m == 0 ? 1 : (m > 0x1E0000u ? 0x1E0000u : uint32_t(m));
}

// p: Length (x), Grain ms, Density /s, Spray %, Pitch spread semitones, Reverse %.
uint32_t granular(const float* p, const int16_t* in, uint32_t n, int16_t* out, uint32_t m) {
    const uint32_t g = uint32_t(p[1] * 44.1f) < 64 ? 64 : uint32_t(p[1] * 44.1f);
    const float period = 44100.0f / (p[2] < 1.0f ? 1.0f : p[2]);
    const float spray = p[3] / 100.0f, spread = p[4], rev = p[5] / 100.0f;
    const float overlap = float(g) / period;
    const float norm = 1.0f / std::sqrt(overlap < 1.0f ? 1.0f : overlap);
    constexpr uint32_t kChunk = 2048;
    float acc[kChunk];
    for (uint32_t c0 = 0; c0 < m; c0 += kChunk) {
        const uint32_t c1 = c0 + kChunk < m ? c0 + kChunk : m;
        for (uint32_t i = 0; i < c1 - c0; ++i) acc[i] = 0.0f;
        // Grains k start near k * period (+ up to half a period of jitter).
        int64_t k0 = int64_t((float(c0) - float(g) - period) / period);
        if (k0 < 0) k0 = 0;
        for (int64_t k = k0;; ++k) {
            const uint32_t h = hash32(uint32_t(k) * 0x9E3779B9u + 0x1234567u);
            const float jit = float(h & 0xFFFF) / 65536.0f * 0.5f * period;
            const int64_t start = int64_t(float(k) * period + jit);
            if (start >= int64_t(c1)) break;
            if (start + int64_t(g) <= int64_t(c0)) continue;
            // Source position: follows the output time, sprayed.
            const uint32_t h2 = hash32(h ^ 0xA5A5A5A5u), h3 = hash32(h2), h4 = hash32(h3);
            const float t = float(start) / float(m);
            float pos = t * float(n) + (float(h2 & 0xFFFF) / 65536.0f - 0.5f) * 2.0f * spray * float(n) * 0.5f;
            const float semis = (float(h3 & 0xFFFF) / 65536.0f - 0.5f) * 2.0f * spread;
            const float ratio = std::exp2(semis / 12.0f);
            const bool back = float(h4 & 0xFFFF) / 65536.0f < rev;
            const float srcLen = float(g) * ratio;
            if (pos < 0.0f) pos = 0.0f;
            if (pos + srcLen >= float(n)) pos = float(n) - srcLen - 1.0f;
            if (pos < 0.0f) continue;   // the selection is shorter than a grain
            for (uint32_t j = 0; j < g; ++j) {
                const int64_t o = start + int64_t(j);
                if (o < int64_t(c0) || o >= int64_t(c1)) continue;
                const float w = 0.5f - 0.5f * std::cos(2.0f * kPi * float(j) / float(g));
                const float sp = back ? pos + srcLen - float(j) * ratio : pos + float(j) * ratio;
                const int32_t i0 = int32_t(sp);
                const float fr = sp - float(i0);
                const float a = float(in[i0]), b = i0 + 1 < int32_t(n) ? float(in[i0 + 1]) : a;
                acc[o - int64_t(c0)] += (a + (b - a) * fr) * w * norm;
            }
        }
        for (uint32_t i = c0; i < c1; ++i) out[i] = sat16f(acc[i - c0]);
    }
    return m;
}

uint32_t strandFrames(const float* p, uint32_t n) {
    const uint64_t m = uint64_t(double(n) * double(p[4]));
    return m == 0 ? 1 : (m > 0x1E0000u ? 0x1E0000u : uint32_t(m));
}

// p: Segment ms, Shuffle %, Repeat %, Reverse %, Length (x).
uint32_t strand(const float* p, const int16_t* in, uint32_t n, int16_t* out, uint32_t m) {
    constexpr int kMaxSeg = 2048;
    uint32_t bound[kMaxSeg + 1];
    uint32_t minLen = uint32_t(p[0] * 44.1f);
    if (minLen < 32) minLen = 32;
    if (n / minLen > kMaxSeg - 1) minLen = n / (kMaxSeg - 1) + 1;
    int ns = 0;
    bound[ns++] = 0;
    for (uint32_t i = 1; i < n && ns < kMaxSeg; ++i)
        if (i - bound[ns - 1] >= minLen && in[i - 1] < 0 && in[i] >= 0) bound[ns++] = i;
    bound[ns] = n;   // segments 0..ns-1: [bound[k], bound[k + 1])
    const float shuffle = p[1] / 100.0f, repeat = p[2] / 100.0f, rev = p[3] / 100.0f;
    Rng rng(0x51A4D001u);
    uint32_t o = 0;
    int seg = 0;
    while (o < m) {
        int reps = 1;
        if (rng.uni() < repeat) reps = 2 + int(rng.next() % 3u);
        const bool back = rng.uni() < rev;
        const uint32_t a = bound[seg], b = bound[seg + 1];
        for (int r = 0; r < reps && o < m; ++r)
            for (uint32_t j = 0; j < b - a && o < m; ++j) out[o++] = back ? in[b - 1 - j] : in[a + j];
        seg = rng.uni() < shuffle ? int(rng.next() % uint32_t(ns)) : seg + 1;
        if (seg >= ns) seg = 0;
    }
    return m;
}

}  // namespace sfx
}  // namespace ot
