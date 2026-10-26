// OT fork Sample Editor tools (sample_fx_mod.cpp): Tuner, Paulstretch,
// Granular, Strand.
#include "test.h"

#include <cmath>
#include <vector>

#include "app/sample_fx.h"
#include "app/sample_fx_ot.h"

using namespace ot;

namespace {
std::vector<int16_t> tone(float hz, uint32_t n, float amp = 12000.0f, bool saw = false) {
    std::vector<int16_t> d(n);
    for (uint32_t i = 0; i < n; ++i) {
        const float ph = std::fmod(hz * float(i) / 44100.0f, 1.0f);
        const float v = saw ? (2.0f * ph - 1.0f) : std::sin(6.2831853f * ph);
        d[i] = int16_t(amp * v);
    }
    return d;
}
double rms(const int16_t* x, size_t n) {
    double s = 0;
    for (size_t i = 0; i < n; ++i) s += double(x[i]) * x[i];
    return n ? std::sqrt(s / double(n)) : 0.0;
}
}  // namespace

TEST(tuner_finds_pitch_and_tune) {
    const float freqs[] = {55.0f, 110.0f, 220.0f, 261.63f, 440.0f, 466.16f, 880.0f, 1500.0f};
    for (float f : freqs)
        for (int saw = 0; saw < 2; ++saw) {
            auto d = tone(f, 30000, 12000.0f, saw != 0);
            float hz = 0, cl = 0;
            CHECK(sfx::detectPitch(d.data(), uint32_t(d.size()), &hz, &cl));
            const float cents = 1200.0f * std::log2(hz / f);
            const float tol = f > 1000.0f ? 8.0f : 3.0f;   // ~30-sample periods: interpolation limits
            if (std::fabs(cents) > tol) std::printf("  %.2f Hz saw %d -> %.2f Hz (%.1f cents)\n", double(f), saw, double(hz), double(cents));
            CHECK(std::fabs(cents) < tol);
        }
    // Silence and noise: no pitch.
    std::vector<int16_t> z(30000, 0);
    float hz, cl;
    CHECK(!sfx::detectPitch(z.data(), uint32_t(z.size()), &hz, &cl));
    // Tune: A4 (440) -> the nearest C is 3 semitones up (C5).
    int t, fi;
    sfx::tuneFor(440.0f, &t, &fi);
    CHECK_EQ(t, 3);
    CHECK(std::abs(fi) <= 1);
    sfx::tuneFor(261.63f * std::exp2(0.2f / 12.0f), &t, &fi);   // C4 + 20 cents
    CHECK_EQ(t, 0);
    CHECK(std::abs(fi + 20) <= 1);
    sfx::tuneFor(196.0f, &t, &fi);   // G3 -> C4 is +5
    CHECK_EQ(t, 5);
}

TEST(paulstretch_stretches_and_keeps_level) {
    auto d = tone(330.0f, 22050, 10000.0f, true);
    std::vector<float> scratch(4 * 16384 + 8192);
    float p[sfx::kMaxParams];
    sfx::defaults(sfx::kPaulstretch, p);
    p[0] = 4;
    p[1] = 1;   // 100 ms
    const uint32_t m = sfx::outputFrames(sfx::kPaulstretch, p, uint32_t(d.size()));
    CHECK_EQ(m, uint32_t(d.size()) * 4);
    std::vector<int16_t> out(m);
    sfx::setScratch(scratch.data(), uint32_t(scratch.size()));
    CHECK_EQ(sfx::process(sfx::kPaulstretch, p, d.data(), uint32_t(d.size()), out.data(), m), m);
    sfx::setScratch(nullptr, 0);
    const double ri = rms(d.data(), d.size()), ro = rms(out.data() + m / 4, m / 2);
    std::printf("  rms in %.0f out %.0f\n", ri, ro);
    CHECK(ro > 0.3 * ri && ro < 2.0 * ri);
    // Without scratch: a plain copy, no crash.
    CHECK_EQ(sfx::process(sfx::kPaulstretch, p, d.data(), uint32_t(d.size()), out.data(), m), m);
}

TEST(granular_and_strand_render) {
    auto d = tone(220.0f, 44100, 9000.0f);
    float p[sfx::kMaxParams];
    sfx::defaults(sfx::kGranular, p);
    p[0] = 2;
    p[4] = 7;
    p[5] = 50;
    uint32_t m = sfx::outputFrames(sfx::kGranular, p, uint32_t(d.size()));
    CHECK_EQ(m, uint32_t(d.size()) * 2);
    std::vector<int16_t> out(m);
    CHECK_EQ(sfx::process(sfx::kGranular, p, d.data(), uint32_t(d.size()), out.data(), m), m);
    const double rg = rms(out.data(), m);
    std::printf("  granular rms %.0f\n", rg);
    CHECK(rg > 1000.0 && rg < 30000.0);
    // Strand: every output sample comes from the input (segments copied whole).
    sfx::defaults(sfx::kStrand, p);
    p[4] = 1.5f;
    auto s = tone(150.0f, 30000, 8000.0f, true);
    m = sfx::outputFrames(sfx::kStrand, p, uint32_t(s.size()));
    CHECK_EQ(m, 45000u);
    out.assign(m, 0);
    CHECK_EQ(sfx::process(sfx::kStrand, p, s.data(), uint32_t(s.size()), out.data(), m), m);
    // No jumps bigger than the saw's own reset at a cut (zero crossings).
    int big = 0;
    for (uint32_t i = 1; i < m; ++i) big += std::abs(out[i] - out[i - 1]) > 17000;
    int bigIn = 0;
    for (size_t i = 1; i < s.size(); ++i) bigIn += std::abs(s[i] - s[i - 1]) > 17000;
    CHECK(big <= bigIn * 2 + 4);
    CHECK(rms(out.data(), m) > 2000.0);
}
