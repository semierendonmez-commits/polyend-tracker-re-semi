// OT fork send-effect models (fx_models.h).
#include "audio/fx_models.h"
#include "audio/fx_models2.h"

#include <cmath>

#include "audio/dsp.h"
#include "audio/ring_io.h"
#include "fastrun.h"

namespace ot {
namespace audio {

namespace {
constexpr float kFs = 44100.0f;
constexpr float kStore = 0.5f;                 // int16 storage at half scale: headroom in the loops
constexpr float kFromS16 = 1.0f / (32768.0f * kStore);
inline int16_t store(float x) { return toS16(x * kStore); }
inline float load(int16_t v) { return float(v) * kFromS16; }
// Sine for the slow modulators (block rate): good enough at 1 % error.
inline float fastSin(float x) {
    // x in radians, wrapped to -pi..pi, parabola with a correction.
    constexpr float kPi = 3.14159265f, kTwoPi = 6.2831853f;
    while (x > kPi) x -= kTwoPi;
    while (x < -kPi) x += kTwoPi;
    const float y = (4.0f / kPi) * x - (4.0f / (kPi * kPi)) * x * (x < 0 ? -x : x);
    return 0.225f * (y * (y < 0 ? -y : y) - y) + y;
}
// n samples from ring position pos (any alignment, wraps at mask + 1) as
// floats, through 32-bit word reads.
void readSpan(const int16_t* ring, uint32_t mask, uint32_t pos, uint32_t n, float* out) {
    uint32_t p = pos & mask, i = 0;
    if ((p & 1u) && n) {
        out[i++] = load(ring[p]);
        p = (p + 1) & mask;
    }
    while (i + 1 < n) {
        const uint32_t v = *reinterpret_cast<const u32_alias*>(ring + p);
        out[i] = load(int16_t(uint16_t(v)));
        out[i + 1] = load(int16_t(uint16_t(v >> 16)));
        i += 2;
        p = (p + 2) & mask;
    }
    if (i < n) out[i] = load(ring[p]);
    OT_SDRAM_ACCESS(n / 2 + 2);
}
// kBlock samples to an even, non-wrapping ring position, as 32-bit words.
void writeSpan(int16_t* ring, uint32_t p, const int16_t* src) {
    u32_alias* d = reinterpret_cast<u32_alias*>(ring + p);
    for (uint32_t i = 0; i < kBlock / 2; ++i) {
        OT_NO_LIBCALL();
        d[i] = uint32_t(uint16_t(src[2 * i])) | (uint32_t(uint16_t(src[2 * i + 1])) << 16);
    }
    OT_SDRAM_ACCESS(kBlock / 2);
}
void zeroRing(int16_t* p, uint32_t n) {
    // 32-bit stores (n even, p 4-byte aligned in the effect memory).
    u32_alias* q = reinterpret_cast<u32_alias*>(p);
    for (uint32_t i = 0; i < n / 2; ++i) {
        OT_NO_LIBCALL();
        q[i] = 0;
    }
    OT_SDRAM_ACCESS(n / 2);
}
}  // namespace

// --- FdnReverb ----------------------------------------------------------------

void FdnReverb::init(int16_t* mem) {
    pre_mem_ = mem;
    for (int k = 0; k < kLines; ++k) line_[k] = mem + kPreCap + uint32_t(k) * kLineCap;
    shim_ = mem + kPreCap + kLines * kLineCap;
    clearPending_ = true;
    dirty_ = true;
}

void FdnReverb::setModel(int m) {
    m = m < 1 ? 1 : (m > 4 ? 4 : m);
    if (m == model_) return;
    model_ = m;
    clearPending_ = true;
    dirty_ = true;
}

void FdnReverb::clearAll() {
    clearPending_ = false;
    if (!pre_mem_) return;
    zeroRing(pre_mem_, kMemorySamples);
    for (auto& d : ap_)
        for (auto& v : d) v = 0;
    for (auto& p : apPos_) p = 0;
    for (float& z : z_) z = 0.0f;
    gateEnv_ = gateGain_ = 0.0f;
    gateHold_ = 0;
    quiet_ = 0;
}

void FdnReverb::configure() {
    dirty_ = false;
    static const uint16_t kLen[5][kLines] = {{1931, 2333, 2767, 3191}, {1931, 2333, 2767, 3191},
                                             {1543, 1913, 2297, 2689}, {557, 683, 797, 911}, {389, 467, 541, 613}};
    static const uint16_t kAp[5][kDiff] = {{142, 107, 379, 277}, {142, 107, 379, 277}, {131, 97, 353, 263},
                                           {89, 131, 197, 61},   {71, 113, 167, 59}};
    const int m = model_ < 1 ? 1 : (model_ > 4 ? 4 : model_);
    const float s = size_ < 0 ? 0 : (size_ > 1 ? 1 : size_);
    float rt;   // RT60 in seconds
    if (m == 4) rt = 0.2f + 1.3f * s;   // Room (after Dragonfly Room: early reflections + a short dense tail)
    else if (m == 3) rt = 0.25f + 0.9f * s;
    else if (m == 2) rt = 1.0f + 12.0f * s * s;
    else rt = 0.5f + 9.5f * s * s;
    float gmax = 0.0f;
    for (int k = 0; k < kLines; ++k) {
        len_[k] = kLen[m][k];
        g_[k] = std::pow(10.0f, -3.0f * float(len_[k]) / (rt * kFs));
        if (g_[k] > gmax) gmax = g_[k];
        apLen_[k] = kAp[m][k];
        if (apPos_[k] >= apLen_[k]) apPos_[k] = 0;
    }
    const float d = damp_ < 0 ? 0 : (damp_ > 1 ? 1 : damp_);
    lp_ = 0.06f + 0.94f * (1.0f - d) * (1.0f - d);
    const float df = diff_ < 0 ? 0 : (diff_ > 1 ? 1 : diff_);
    apG_ = 0.4f + 0.35f * df;
    // Shimmer feed: below the headroom the decay leaves, so the loop stays
    // under unity gain; the octave builds up over the passes.
    shimAmt_ = m == 2 ? 0.45f * (1.0f - gmax) * (0.5f + 0.5f * df) : 0.0f;
    holdLen_ = uint32_t((0.12f + 0.45f * s) * kFs);
    outGain_ = m == 4 ? 0.38f : (m == 3 ? 0.55f : (m == 2 ? 0.4f : 0.42f));
}

OT_FASTRUN_FN bool FdnReverb::process(const float* in, float* outL, float* outR) {
    if (!pre_mem_) {
        zeroFloats(outL, kBlock);
        zeroFloats(outR, kBlock);
        return false;
    }
    if (clearPending_) clearAll();
    if (dirty_) configure();
    bool anyIn = false;
    for (uint32_t i = 0; i < kBlock; ++i) anyIn |= in[i] != 0.0f;
    const uint32_t idleAfter = 3 * kLineCap;
    if (!anyIn && quiet_ >= idleAfter) {   // idle: the tail has died away
        zeroFloats(outL, kBlock);
        zeroFloats(outR, kBlock);
        return false;
    }
    const int m = model_;
    const uint32_t w = w_;   // a multiple of kBlock: block writes are word aligned and never wrap
    constexpr uint32_t kPreMask = kPreCap - 1, kLineMask = kLineCap - 1, kShimMask = kShimCap - 1;
    // 1. Pre-delay (written first, so 0 reads this block's input). The rings
    //    are moved as 32-bit words (one SDRAM access per two samples).
    int16_t wb[kBlock];
    float x[kBlock];
    for (uint32_t i = 0; i < kBlock; ++i) wb[i] = store(in[i]);
    writeSpan(pre_mem_, w & kPreMask, wb);
    readSpan(pre_mem_, kPreMask, w - pre_, kBlock, x);
    // 2. Input diffusion: Schroeder all-passes in SRAM.
    const float g = apG_;
    // Room: early reflections, six taps behind the pre-delay, alternating sides.
    float erL[kBlock], erR[kBlock];
    if (m == 4) {
        static const uint16_t kTap[6] = {331, 563, 797, 1031, 1373, 1709};
        static const float kG[6] = {0.62f, -0.55f, 0.48f, -0.41f, 0.34f, -0.27f};
        const float sc = 0.5f + 0.5f * (size_ < 0 ? 0 : (size_ > 1 ? 1 : size_));
        for (uint32_t i = 0; i < kBlock; ++i) erL[i] = erR[i] = 0.0f;
        float t[kBlock];
        for (int k = 0; k < 6; ++k) {
            readSpan(pre_mem_, kPreMask, w - pre_ - uint32_t(float(kTap[k]) * sc), kBlock, t);
            float* dst = (k & 1) ? erR : erL;
            for (uint32_t i = 0; i < kBlock; ++i) dst[i] += kG[k] * t[i];
        }
    }
    const int nDiff = m >= 3 ? kDiff : 2;   // the FDN diffuses the rest
    for (int dI = 0; dI < nDiff; ++dI) {
        int16_t* buf = ap_[dI];
        uint32_t p = apPos_[dI];
        const uint32_t L = apLen_[dI];
        for (uint32_t i = 0; i < kBlock; ++i) {
            const float dl = load(buf[p]);
            const float y = dl - g * x[i];
            buf[p] = store(x[i] + g * y);
            p = p + 1 == L ? 0 : p + 1;
            x[i] = y;
        }
        apPos_[dI] = uint16_t(p);
    }
    // 3. Line reads: one span of kBlock + 1 per line at a fractional,
    //    slowly modulated delay (constant within the block).
    static const float kDepth[5] = {0, 6.0f, 3.5f, 0.0f, 1.5f};
    static const float kPhase[kLines] = {0.0f, 1.7f, 3.1f, 4.6f};
    lfo_ += 1;
    const float ph = float(lfo_) * (6.2831853f * 0.45f * float(kBlock) / kFs);
    float rd[kLines][kBlock + 1];
    float frac[kLines];
    for (int k = 0; k < kLines; ++k) {
        const float D = float(len_[k]) + kDepth[m] * (1.0f + fastSin(ph + kPhase[k]));
        const uint32_t Di = uint32_t(D);
        frac[k] = 1.0f - (D - float(Di));
        readSpan(line_[k], kLineMask, w - Di - 1, kBlock + 1, rd[k]);
    }
    // Shimmer heads (octave up): each moves 2 samples per output sample, so
    // a block reads a span of 2 * kBlock per head (two where its window wraps).
    const float shim = shimAmt_;
    constexpr uint32_t kShimW = 2048, kShimMin = 256;
    float hp[2][kBlock];
    float hw[2][kBlock];
    if (shim > 0.0f) {
        for (int h = 0; h < 2; ++h) {
            uint32_t ph0 = (shimD_ + (h ? kShimW / 2 : 0)) & (kShimW - 1);
            uint32_t i = 0;
            while (i < kBlock) {
                uint32_t n = kShimW - ph0;   // samples until this head jumps
                if (n > kBlock - i) n = kBlock - i;
                const uint32_t d = kShimMin + kShimW - 1 - ph0;
                float span[2 * kBlock];
                readSpan(shim_, kShimMask, w + i - d, 2 * n - 1, span);
                for (uint32_t k = 0; k < n; ++k) {
                    hp[h][i + k] = span[2 * k];
                    const float t = float(ph0 + k) * (2.0f / kShimW);
                    hw[h][i + k] = t < 1.0f ? t : 2.0f - t;
                }
                i += n;
                ph0 = (ph0 + n) & (kShimW - 1);
            }
        }
        shimD_ = (shimD_ + kBlock) & (kShimW - 1);
    }
    // 4. The network, per sample; line writes collected for word stores.
    const float lp = lp_, g0 = g_[0], g1 = g_[1], g2 = g_[2], g3 = g_[3], og = outGain_;
    float z0 = z_[0], z1 = z_[1], z2 = z_[2], z3 = z_[3];
    float gEnv = gateEnv_, gGain = gateGain_;
    uint32_t hold = gateHold_;
    float peak = 0.0f;
    int16_t wl[kLines][kBlock];
    int16_t ws[kBlock];
    for (uint32_t i = 0; i < kBlock; ++i) {
        const float a = rd[0][i] + frac[0] * (rd[0][i + 1] - rd[0][i]);
        const float b = rd[1][i] + frac[1] * (rd[1][i + 1] - rd[1][i]);
        const float c = rd[2][i] + frac[2] * (rd[2][i + 1] - rd[2][i]);
        const float d = rd[3][i] + frac[3] * (rd[3][i + 1] - rd[3][i]);
        const float h0 = 0.5f * (a + b + c + d), h1 = 0.5f * (a - b + c - d);
        const float h2 = 0.5f * (a + b - c - d), h3 = 0.5f * (a - b - c + d);
        const float u = x[i];
        const float p = shim > 0.0f ? shim * (hw[0][i] * hp[0][i] + hw[1][i] * hp[1][i]) : 0.0f;
        z0 += lp * (u + g0 * h0 + p - z0);
        z1 += lp * (-u + g1 * h1 + p - z1);
        z2 += lp * (u + g2 * h2 - p - z2);
        z3 += lp * (-u + g3 * h3 - p - z3);
        // store() saturates (VCVT): the loop cannot run away past full scale.
        wl[0][i] = store(z0);
        wl[1][i] = store(z1);
        wl[2][i] = store(z2);
        wl[3][i] = store(z3);
        ws[i] = store(0.25f * (a + b + c + d));
        float l = og * (a + 0.7f * c - 0.4f * d), r = og * (b + 0.7f * d - 0.4f * c);
        if (m == 4) {
            l += erL[i];
            r += erR[i];
        }
        if (m == 3) {
            // Gate: open while the input is up, held, then closed fast.
            const float ai = in[i] < 0 ? -in[i] : in[i];
            gEnv = ai > gEnv ? ai : gEnv * 0.9995f;
            float target;
            if (gEnv > 0.02f) {
                hold = holdLen_;
                target = 1.0f;
            } else if (hold) {
                --hold;
                target = 1.0f;
            } else {
                target = 0.0f;
            }
            gGain += (target - gGain) * (target > gGain ? 0.05f : 0.004f);
            l *= gGain;
            r *= gGain;
        }
        outL[i] = clampUnitFast(l);
        outR[i] = clampUnitFast(r);
        const float al = l < 0 ? -l : l;
        peak = al > peak ? al : peak;
    }
    for (int k = 0; k < kLines; ++k) writeSpan(line_[k], w & kLineMask, wl[k]);
    if (shim > 0.0f) writeSpan(shim_, w & kShimMask, ws);
    z_[0] = z0;
    z_[1] = z1;
    z_[2] = z2;
    z_[3] = z3;
    gateEnv_ = gEnv;
    gateGain_ = gGain;
    gateHold_ = hold;
    w_ = w + kBlock;
    if (!anyIn && peak < 2e-5f) quiet_ += kBlock;
    else quiet_ = 0;
    return true;
}

// --- TapeDelay ----------------------------------------------------------------

void TapeDelay::init(int16_t* lineA, int16_t* lineB) {
    a_ = lineA;
    b_ = lineB;
}

void TapeDelay::setFilter(float hz) {
    const float cap = model_ == 2 ? 3500.0f : 7000.0f;   // tape darkens, lo-fi is band-limited
    const float f = hz < cap ? hz : cap;
    lpA_ = 1.0f - std::exp(-6.2831853f * f / kFs);
}

OT_FASTRUN_FN bool TapeDelay::process(const float* in, float* outL, float* outR) {
    if (!a_ || !b_) {
        zeroFloats(outL, kBlock);
        zeroFloats(outR, kBlock);
        return false;
    }
    bool anyIn = false;
    for (uint32_t i = 0; i < kBlock; ++i) anyIn |= in[i] != 0.0f;
    if (!anyIn && quiet_ > time_ + 4096) {   // idle (the line is frozen)
        zeroFloats(outL, kBlock);
        zeroFloats(outR, kBlock);
        return false;
    }
    constexpr uint32_t kMask = kDelayLineLen - 1;
    const bool lofi = model_ == 2;
    // Wow (0.55 Hz) and flutter (6.5 Hz) on the read head, interpolated
    // over the block; lo-fi wobbles less but steps its samples.
    const float wowD = lofi ? 4.0f : 14.0f, flutD = lofi ? 0.5f : 1.6f;
    const float modA = wowD * fastSin(wowPh_) + flutD * fastSin(flutPh_);
    wowPh_ += 6.2831853f * 0.55f * float(kBlock) / kFs;
    flutPh_ += 6.2831853f * 6.5f * float(kBlock) / kFs;
    if (wowPh_ > 6.2831853f) wowPh_ -= 6.2831853f;
    if (flutPh_ > 6.2831853f) flutPh_ -= 6.2831853f;
    const float modB = wowD * fastSin(wowPh_) + flutD * fastSin(flutPh_);
    const uint32_t w = w_;   // a multiple of kBlock (word-aligned block writes)
    const float fb = fb_, lpA = lpA_, hpA = lofi ? 0.042f : 0.011f;
    float lz0 = lpZ_[0], lz1 = lpZ_[1], hz0 = hpZ_[0], hz1 = hpZ_[1];
    float peak = 0.0f;
    // The read head moves between D(0) and D(kBlock - 1): one span covers it.
    const float D0 = float(time_) + 24.0f + modA, D1 = float(time_) + 24.0f + modB;
    const uint32_t dHi = uint32_t(D0 > D1 ? D0 : D1) + 1, dLo = uint32_t(D0 < D1 ? D0 : D1);
    const uint32_t spanN = kBlock + (dHi - dLo) + 2;   // <= kBlock + 40
    const uint32_t spanStart = w - dHi - 1;
    float ra[kBlock + 48], rb[kBlock + 48];
    readSpan(a_, kMask, spanStart, spanN, ra);
    if (pingpong_) readSpan(b_, kMask, spanStart, spanN, rb);
    int16_t wa[kBlock], wbb[kBlock];
    for (uint32_t i = 0; i < kBlock; ++i) {
        const float mod = modA + (modB - modA) * (float(i) * (1.0f / kBlock));
        const float D = float(time_) + 24.0f + mod;
        const uint32_t Di = uint32_t(D);
        const float fr = D - float(Di);
        // Sample (w + i) - D: between (w + i - Di - 1) and (w + i - Di).
        const uint32_t j1 = (w + i - Di) - spanStart, j0 = j1 - 1;
        float ya = ra[j1] + fr * (ra[j0] - ra[j1]);
        float yb = pingpong_ ? rb[j1] + fr * (rb[j0] - rb[j1]) : ya;
        if (lofi) {
            // 11 kHz sample-and-hold and 6-bit steps.
            if ((holdN_++ & 3u) == 0) {
                hold_[0] = float(int(ya * 32.0f)) * (1.0f / 32.0f);
                hold_[1] = float(int(yb * 32.0f)) * (1.0f / 32.0f);
            }
            ya = hold_[0];
            yb = hold_[1];
        }
        // Feedback: saturation, low-pass, DC / rumble high-pass.
        auto shape = [&](float v, float& lz, float& hz) {
            v *= fb;
            v = v > 1.5f ? 1.5f : (v < -1.5f ? -1.5f : v);
            v = v - (v * v * v) * (1.0f / 6.75f);   // soft knee, unity at small levels, max ~1
            lz += lpA * (v - lz);
            hz += hpA * (lz - hz);
            return lz - hz;
        };
        const float fa = shape(ya, lz0, hz0);
        if (pingpong_) {
            const float fbb = shape(yb, lz1, hz1);
            wa[i] = store(in[i] + fbb);   // cross-fed: L -> R -> L
            wbb[i] = store(fa);
            outL[i] = ya;
            outR[i] = yb;
        } else {
            wa[i] = store(in[i] + fa);
            outL[i] = ya;
            outR[i] = ya;
        }
        const float ay = ya < 0 ? -ya : ya, by = yb < 0 ? -yb : yb;
        peak = ay > peak ? ay : peak;
        peak = by > peak ? by : peak;
    }
    writeSpan(a_, w & kMask, wa);
    if (pingpong_) writeSpan(b_, w & kMask, wbb);
    lpZ_[0] = lz0;
    lpZ_[1] = lz1;
    hpZ_[0] = hz0;
    hpZ_[1] = hz1;
    w_ = w + kBlock;
    if (!anyIn && peak < 2e-5f) quiet_ += kBlock;
    else quiet_ = 0;
    return true;
}


// --- block helpers for the span-based models ---------------------------------

namespace {
// All-pass (lattice): v = x + g v[-D], y = v[-D] - g v. D > kBlock (the
// delayed values of a whole block are in the past), fractional D linearly
// interpolated; x is replaced by y.
void apBlock(int16_t* ring, uint32_t mask, uint32_t w, float D, float g, float* x) {
    const uint32_t Di = uint32_t(D);
    const float fr = D - float(Di);
    float rd[kBlock + 1];
    readSpan(ring, mask, w - Di - 1, kBlock + 1, rd);   // rd[i] = v(w + i - Di - 1)
    int16_t wb[kBlock];
    for (uint32_t i = 0; i < kBlock; ++i) {
        const float vd = rd[i + 1] + fr * (rd[i] - rd[i + 1]);
        const float v = x[i] + g * vd;
        wb[i] = store(v);
        x[i] = vd - g * v;
    }
    writeSpan(ring, w & mask, wb);
}
// Delay: writes x, returns the samples D behind in out (D > kBlock).
void delayBlock(int16_t* ring, uint32_t mask, uint32_t w, uint32_t D, const float* x, float* out) {
    readSpan(ring, mask, w - D, kBlock, out);
    int16_t wb[kBlock];
    for (uint32_t i = 0; i < kBlock; ++i) wb[i] = store(x[i]);
    writeSpan(ring, w & mask, wb);
}
}  // namespace

// --- PlateReverb (Dattorro) ------------------------------------------------------

void PlateReverb::init(int16_t* mem) {
    static const uint32_t kSize[kRings] = {4096, 512, 512, 512, 512, 1024, 4096, 2048, 4096, 1024, 4096, 4096, 4096};
    uint32_t at = 0;
    for (int k = 0; k < kRings; ++k) {
        ring_[k] = mem + at;
        mask_[k] = kSize[k] - 1;
        at += kSize[k];
    }
    clearPending_ = true;
}

OT_FASTRUN_FN bool PlateReverb::process(const float* in, float* outL, float* outR) {
    if (!ring_[0]) {
        zeroFloats(outL, kBlock);
        zeroFloats(outR, kBlock);
        return false;
    }
    if (clearPending_) {
        clearPending_ = false;
        zeroRing(ring_[0], kMemorySamples);
        bw_ = dampZ_[0] = dampZ_[1] = 0.0f;
        quiet_ = 0;
    }
    bool anyIn = false;
    for (uint32_t i = 0; i < kBlock; ++i) anyIn |= in[i] != 0.0f;
    if (!anyIn && quiet_ >= 3u * 4096u) {
        zeroFloats(outL, kBlock);
        zeroFloats(outR, kBlock);
        return false;
    }
    const uint32_t w = w_;
    const float sz = size_ < 0 ? 0 : (size_ > 1 ? 1 : size_);
    const float dm = damp_ < 0 ? 0 : (damp_ > 1 ? 1 : damp_);
    const float df = diff_ < 0 ? 0 : (diff_ > 1 ? 1 : diff_);
    const float decay = 0.3f + 0.67f * sz;
    const float dd2 = decay + 0.15f < 0.25f ? 0.25f : (decay + 0.15f > 0.5f ? 0.5f : decay + 0.15f);
    const float g1 = 0.75f * (0.4f + 0.6f * df), g2 = 0.625f * (0.4f + 0.6f * df);
    const float dampA = 1.0f - 0.85f * dm;   // tank low-pass: 1 = open
    // 1. Pre-delay and the bandwidth low-pass.
    float x[kBlock];
    {
        int16_t wb[kBlock];
        for (uint32_t i = 0; i < kBlock; ++i) wb[i] = store(in[i]);
        writeSpan(ring_[kPre], w & mask_[kPre], wb);
        readSpan(ring_[kPre], mask_[kPre], w - pre_, kBlock, x);
        float b = bw_;
        for (uint32_t i = 0; i < kBlock; ++i) {
            b += 0.7f * (x[i] - b);
            x[i] = b;
        }
        bw_ = b;
    }
    // 2. Input diffusers (lengths kept above a block).
    apBlock(ring_[kIn0], mask_[kIn0], w, 131.0f, g1, x);
    apBlock(ring_[kIn1], mask_[kIn1], w, 149.0f, g1, x);
    apBlock(ring_[kIn2], mask_[kIn2], w, 341.0f, g2, x);
    apBlock(ring_[kIn3], mask_[kIn3], w, 249.0f, g2, x);
    // 3. The tank: each half takes the other half's output (read before this block's writes).
    float lOut[kBlock], rOut[kBlock];
    readSpan(ring_[kDel1], mask_[kDel1], w - 3348, kBlock, lOut);
    readSpan(ring_[kDel3], mask_[kDel3], w - 2847, kBlock, rOut);
    ++lfo_;
    const float ph = float(lfo_) * (6.2831853f * 0.9f * float(kBlock) / kFs);
    const float exc0 = 14.0f * (1.0f + fastSin(ph)), exc1 = 14.0f * (1.0f + fastSin(ph + 1.7f));
    float a[kBlock], b[kBlock];
    for (uint32_t i = 0; i < kBlock; ++i) {
        a[i] = x[i] + decay * rOut[i];
        b[i] = x[i] + decay * lOut[i];
    }
    // Left half.
    apBlock(ring_[kMod0], mask_[kMod0], w, 605.0f + exc0, -0.7f, a);
    float t[kBlock];
    delayBlock(ring_[kDel0], mask_[kDel0], w, 4008, a, t);
    {
        float z = dampZ_[0];
        for (uint32_t i = 0; i < kBlock; ++i) {
            z += dampA * (t[i] - z);
            t[i] = z * decay;
        }
        dampZ_[0] = z;
    }
    apBlock(ring_[kAp0], mask_[kAp0], w, 1620.0f, dd2, t);
    {
        int16_t wb[kBlock];
        for (uint32_t i = 0; i < kBlock; ++i) wb[i] = store(t[i]);
        writeSpan(ring_[kDel1], w & mask_[kDel1], wb);
    }
    // Right half.
    apBlock(ring_[kMod1], mask_[kMod1], w, 817.0f + exc1, -0.7f, b);
    delayBlock(ring_[kDel2], mask_[kDel2], w, 3795, b, t);
    {
        float z = dampZ_[1];
        for (uint32_t i = 0; i < kBlock; ++i) {
            z += dampA * (t[i] - z);
            t[i] = z * decay;
        }
        dampZ_[1] = z;
    }
    apBlock(ring_[kAp1], mask_[kAp1], w, 2390.0f, dd2, t);
    {
        int16_t wb[kBlock];
        for (uint32_t i = 0; i < kBlock; ++i) wb[i] = store(t[i]);
        writeSpan(ring_[kDel3], w & mask_[kDel3], wb);
    }
    // 4. Output taps (the paper's, x 0.9), after this block's writes.
    struct Tap {
        uint8_t ring;
        uint16_t d;
        int8_t sign;
    };
    static const Tap kL[7] = {{kDel2, 239, 1},  {kDel2, 2677, 1}, {kAp1, 1722, -1}, {kDel3, 1796, 1},
                              {kDel0, 1791, -1}, {kAp0, 168, -1},  {kDel1, 959, -1}};
    static const Tap kR[7] = {{kDel0, 318, 1},  {kDel0, 3264, 1}, {kAp0, 1105, -1}, {kDel1, 2406, 1},
                              {kDel2, 1900, -1}, {kAp1, 302, -1},  {kDel3, 109, -1}};
    float accL[kBlock] = {}, accR[kBlock] = {};
    for (int k = 0; k < 7; ++k) {
        readSpan(ring_[kL[k].ring], mask_[kL[k].ring], w - kL[k].d, kBlock, t);
        const float sg = float(kL[k].sign);
        for (uint32_t i = 0; i < kBlock; ++i) accL[i] += sg * t[i];
        readSpan(ring_[kR[k].ring], mask_[kR[k].ring], w - kR[k].d, kBlock, t);
        const float sr = float(kR[k].sign);
        for (uint32_t i = 0; i < kBlock; ++i) accR[i] += sr * t[i];
    }
    float peak = 0.0f;
    for (uint32_t i = 0; i < kBlock; ++i) {
        const float l = 0.6f * accL[i], r = 0.6f * accR[i];
        outL[i] = clampUnitFast(l);
        outR[i] = clampUnitFast(r);
        const float al = l < 0 ? -l : l;
        peak = al > peak ? al : peak;
    }
    w_ = w + kBlock;
    if (!anyIn && peak < 2e-5f) quiet_ += kBlock;
    else quiet_ = 0;
    return true;
}

// --- TextureReverb (granular cloud) ------------------------------------------

void TextureReverb::init(int16_t* mem) {
    ring_ = mem;
    ap_[0] = mem + kRingLen;
    ap_[1] = mem + kRingLen + kApLen;
    clearPending_ = true;
}

void TextureReverb::startGrain(Grain& g, int k) {
    auto rnd = [this]() {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return float(rng_ >> 8) * (1.0f / 16777216.0f);
    };
    const float sz = size_ < 0 ? 0 : (size_ > 1 ? 1 : size_);
    const float df = diff_ < 0 ? 0 : (diff_ > 1 ? 1 : diff_);
    uint32_t len = uint32_t((1500.0f + 5500.0f * sz) * (0.8f + 0.4f * rnd()));
    len = len < 512 ? 512 : (len > 7000 ? 7000 : len);
    const float r = rnd();
    g.pitch = r < 0.12f ? int8_t(2) : (r < 0.24f ? int8_t(-1) : int8_t(1));
    // How far back it starts: past what an octave-up head would overtake.
    const uint32_t minLag = (g.pitch == 2 ? len : 0) + 2 * kBlock + 16 + pre_;
    const uint32_t maxLag = kRingLen - len - 2 * kBlock - 16;
    const uint32_t span = maxLag > minLag ? maxLag - minLag : 0;
    const uint32_t lag = minLag + uint32_t(float(span) * df * rnd());
    g.len = len;
    g.at = 0;
    g.src = w_ - lag;
    const bool left = ((k + int(rnd() * 2.0f)) & 1) != 0;
    g.panL = left ? 0.85f : 0.45f;
    g.panR = left ? 0.45f : 0.85f;
}

OT_FASTRUN_FN bool TextureReverb::process(const float* in, float* outL, float* outR) {
    if (!ring_) {
        zeroFloats(outL, kBlock);
        zeroFloats(outR, kBlock);
        return false;
    }
    if (clearPending_) {
        clearPending_ = false;
        zeroRing(ring_, kMemorySamples);
        lpZ_[0] = lpZ_[1] = 0.0f;
        for (float& v : fbPrev_) v = 0.0f;
        quiet_ = 0;
        for (int k = 0; k < kGrains; ++k) {
            startGrain(g_[k], k);
            g_[k].at = g_[k].len * uint32_t(k) / kGrains;   // staggered
        }
    }
    bool anyIn = false;
    for (uint32_t i = 0; i < kBlock; ++i) anyIn |= in[i] != 0.0f;
    if (!anyIn && quiet_ >= 2u * kRingLen) {
        zeroFloats(outL, kBlock);
        zeroFloats(outR, kBlock);
        return false;
    }
    const uint32_t w = w_;
    constexpr uint32_t kMask = kRingLen - 1;
    const float sz = size_ < 0 ? 0 : (size_ > 1 ? 1 : size_);
    const float dm = damp_ < 0 ? 0 : (damp_ > 1 ? 1 : damp_);
    const float df = diff_ < 0 ? 0 : (diff_ > 1 ? 1 : diff_);
    const float fb = 0.2f + 0.45f * sz;
    // 1. The cloud buffer: the input plus the last block's grains (feedback).
    {
        int16_t wb[kBlock];
        for (uint32_t i = 0; i < kBlock; ++i) wb[i] = store(in[i] + fb * fbPrev_[i]);
        writeSpan(ring_, w & kMask, wb);
    }
    // 2. Grains.
    float L[kBlock] = {}, R[kBlock] = {};
    for (int k = 0; k < kGrains; ++k) {
        Grain& g = g_[k];
        uint32_t i = 0;
        while (i < kBlock) {
            if (g.at >= g.len) startGrain(g, k);
            uint32_t n = g.len - g.at;
            if (n > kBlock - i) n = kBlock - i;
            float sp[2 * kBlock + 2];
            const float inv = 1.0f / float(g.len);
            if (g.pitch == 2) {
                readSpan(ring_, kMask, g.src + 2 * g.at, 2 * n, sp);
                for (uint32_t j = 0; j < n; ++j) {
                    const float t = float(g.at + j) * inv, win = 4.0f * t * (1.0f - t);
                    const float v = sp[2 * j] * win;
                    L[i + j] += v * g.panL;
                    R[i + j] += v * g.panR;
                }
            } else if (g.pitch == -1) {
                const uint32_t base = g.at / 2;
                readSpan(ring_, kMask, g.src + base, n / 2 + 2, sp);
                for (uint32_t j = 0; j < n; ++j) {
                    const uint32_t q = (g.at + j) / 2 - base;
                    const float s = ((g.at + j) & 1u) ? 0.5f * (sp[q] + sp[q + 1]) : sp[q];
                    const float t = float(g.at + j) * inv, win = 4.0f * t * (1.0f - t);
                    L[i + j] += s * win * g.panL;
                    R[i + j] += s * win * g.panR;
                }
            } else {
                readSpan(ring_, kMask, g.src + g.at, n, sp);
                for (uint32_t j = 0; j < n; ++j) {
                    const float t = float(g.at + j) * inv, win = 4.0f * t * (1.0f - t);
                    const float v = sp[j] * win;
                    L[i + j] += v * g.panL;
                    R[i + j] += v * g.panR;
                }
            }
            g.at += n;
            i += n;
        }
    }
    // 3. Feedback source, diffusers, tone.
    for (uint32_t i = 0; i < kBlock; ++i) fbPrev_[i] = 0.25f * (L[i] + R[i]);   // fb < 1: the cloud thickens, never runs away
    const float ag = 0.3f + 0.4f * df;
    apBlock(ap_[0], kApLen - 1, w, 331.0f, ag, L);
    apBlock(ap_[1], kApLen - 1, w, 557.0f, ag, R);
    const float a = 0.12f + 0.88f * (1.0f - dm) * (1.0f - dm);
    float zl = lpZ_[0], zr = lpZ_[1], peak = 0.0f;
    for (uint32_t i = 0; i < kBlock; ++i) {
        zl += a * (0.6f * L[i] - zl);
        zr += a * (0.6f * R[i] - zr);
        outL[i] = clampUnitFast(zl);
        outR[i] = clampUnitFast(zr);
        const float al = zl < 0 ? -zl : zl;
        peak = al > peak ? al : peak;
    }
    lpZ_[0] = zl;
    lpZ_[1] = zr;
    w_ = w + kBlock;
    if (!anyIn && peak < 2e-5f) quiet_ += kBlock;
    else quiet_ = 0;
    return true;
}

}  // namespace audio
}  // namespace ot
