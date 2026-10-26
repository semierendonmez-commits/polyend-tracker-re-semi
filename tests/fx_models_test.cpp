// OT fork send-effect models (src/core/audio/fx_models.h): stable, decaying
// tails, idle detection, shimmer bounded, gate closing, tape repeats.
#include "test.h"

#include <cmath>
#include <vector>

#include "audio/effects.h"
#include "audio/fx_models.h"
#include "audio/fx_models2.h"

using namespace ot;
using namespace ot::audio;

namespace {
uint32_t lcg = 12345;
float noise() {
    lcg = lcg * 1664525u + 1013904223u;
    return float(int32_t(lcg >> 8) - (1 << 23)) / float(1 << 23);
}
struct Run {
    double rms = 0;
    float peak = 0;
    bool finite = true;
};
// `on` blocks of noise at `level`, then `off` blocks of silence; RMS / peak
// of the outputs per segment.
template <class Fx>
void run(Fx& fx, int blocks, float level, Run& r, int& idleBlocks) {
    float in[kBlock], l[kBlock], rr[kBlock];
    double sum = 0;
    for (int b = 0; b < blocks; ++b) {
        for (uint32_t i = 0; i < kBlock; ++i) in[i] = level * noise();
        const bool active = fx.process(in, l, rr);
        if (!active) ++idleBlocks;
        for (uint32_t i = 0; i < kBlock; ++i) {
            if (!std::isfinite(l[i]) || !std::isfinite(rr[i])) r.finite = false;
            sum += double(l[i]) * l[i] + double(rr[i]) * rr[i];
            r.peak = std::fmax(r.peak, std::fmax(std::fabs(l[i]), std::fabs(rr[i])));
        }
    }
    r.rms = std::sqrt(sum / (2.0 * blocks * kBlock));
}
}  // namespace

TEST(fdn_reverb_models_decay_and_idle) {
    std::vector<int16_t> mem(Reverb::kMemorySamples, 0x1234);   // garbage: the model clears it
    for (int m = 1; m <= 3; ++m) {
        FdnReverb fx;
        fx.init(mem.data());
        fx.setModel(m);
        fx.setSize(0.8f);
        fx.setDamping(0.3f);
        fx.setDiffusion(0.7f);
        fx.setPredelay(0.1f);
        Run on, tail1, tail2, late;
        int idle = 0;
        run(fx, 600, 0.5f, on, idle);      // 1.7 s of noise
        CHECK_EQ(idle, 0);
        run(fx, 100, 0.0f, tail1, idle);   // first 0.3 s of the tail
        run(fx, 400, 0.0f, tail2, idle);   // next 1.2 s
        int idleLate = 0;
        run(fx, 4000, 0.0f, late, idleLate);   // 11.6 s more
        std::printf("  model %d: on %.4f tail %.4f / %.4f late %.6f, idle after %d blocks\n", m, on.rms, tail1.rms,
                    tail2.rms, late.rms, 4000 - idleLate);
        CHECK(on.finite && tail1.finite && tail2.finite && late.finite);
        CHECK(on.rms > 0.02 && on.peak <= 1.0f);
        CHECK(tail2.rms < tail1.rms);          // decays
        CHECK(idleLate > 0);                   // goes idle at the end
        if (m == 3) CHECK(tail2.rms < 0.2 * on.rms);   // the gate closed
    }
}

TEST(shimmer_tail_dies_at_max_size) {
    // Size, diffusion at max, no damping: the octave feedback must not keep
    // the tail alive (no self-oscillation) after the input stops.
    std::vector<int16_t> mem(Reverb::kMemorySamples, 0);
    FdnReverb fx;
    fx.init(mem.data());
    fx.setModel(2);
    fx.setSize(1.0f);
    fx.setDamping(0.0f);
    fx.setDiffusion(1.0f);
    Run a, t1, t2, t3;
    int idle = 0;
    run(fx, 300, 0.3f, a, idle);
    run(fx, 1000, 0.0f, t1, idle);   // 0..2.9 s of tail
    run(fx, 1000, 0.0f, t2, idle);   // 2.9..5.8 s
    run(fx, 3000, 0.0f, t3, idle);   // 5.8..14.5 s
    std::printf("  shimmer max: on %.4f tail %.4f %.4f %.5f\n", a.rms, t1.rms, t2.rms, t3.rms);
    CHECK(t1.finite && t2.finite && t3.finite);
    CHECK(t2.rms < 0.7 * t1.rms);
    CHECK(t3.rms < 0.7 * t2.rms);
}

TEST(tape_delay_repeats_decay) {
    std::vector<int16_t> mem(2 * kDelayLineLen, 0);
    for (int m = 1; m <= 2; ++m)
        for (int pp = 0; pp < 2; ++pp) {
            TapeDelay fx;
            fx.init(mem.data(), mem.data() + kDelayLineLen);
            fx.setModel(m);
            fx.setTimeSamples(11025);
            fx.setFeedback(0.7f);
            fx.setFilter(8000.0f);
            fx.setPingPong(pp != 0);
            Run on, t1, t2, late;
            int idle = 0;
            run(fx, 40, 0.5f, on, idle);
            run(fx, 200, 0.0f, t1, idle);
            run(fx, 600, 0.0f, t2, idle);
            int idleLate = 0;
            run(fx, 3000, 0.0f, late, idleLate);
            std::printf("  delay %d pp %d: %.4f %.4f %.4f late %.6f idle %d\n", m, pp, on.rms, t1.rms, t2.rms, late.rms,
                        idleLate);
            CHECK(on.finite && t1.finite && t2.finite && late.finite);
            CHECK(t1.rms > 0.01);      // repeats
            CHECK(t2.rms < t1.rms);    // that fade
            CHECK(idleLate > 0);
            for (auto& v : mem) v = 0;
        }
}

TEST(texture_plate_room_models) {
    std::vector<int16_t> mem(Reverb::kMemorySamples, 0x2222);   // garbage: each model clears it
    {
        FdnReverb fx;   // Room = model 4
        fx.init(mem.data());
        fx.setModel(4);
        fx.setSize(0.5f);
        fx.setPredelay(0.2f);
        Run on, t1, t2, late;
        int idle = 0;
        run(fx, 300, 0.5f, on, idle);
        run(fx, 60, 0.0f, t1, idle);
        run(fx, 300, 0.0f, t2, idle);
        int idleLate = 0;
        run(fx, 3000, 0.0f, late, idleLate);
        std::printf("  room: on %.4f tail %.4f / %.5f idle %d\n", on.rms, t1.rms, t2.rms, idleLate);
        CHECK(on.finite && t1.finite && t2.finite && late.finite);
        CHECK(on.rms > 0.02 && t2.rms < t1.rms && idleLate > 0);
    }
    for (int k = 0; k < 2; ++k) {
        PlateReverb plate;
        TextureReverb tex;
        Run on, t1, t2, late;
        int idle = 0, idleLate = 0;
        if (k == 0) {
            plate.init(mem.data());
            plate.activate();
            plate.setSize(0.8f);
            plate.setDamping(0.3f);
            run(plate, 600, 0.5f, on, idle);
            run(plate, 100, 0.0f, t1, idle);
            run(plate, 600, 0.0f, t2, idle);
            run(plate, 6000, 0.0f, late, idleLate);
        } else {
            tex.init(mem.data());
            tex.activate();
            tex.setSize(1.0f);
            tex.setDiffusion(1.0f);
            run(tex, 600, 0.5f, on, idle);
            run(tex, 100, 0.0f, t1, idle);
            run(tex, 600, 0.0f, t2, idle);
            run(tex, 6000, 0.0f, late, idleLate);
        }
        std::printf("  %s: on %.4f tail %.4f / %.5f late %.6f idle %d\n", k ? "texture" : "plate", on.rms, t1.rms,
                    t2.rms, late.rms, idleLate);
        CHECK(on.finite && t1.finite && t2.finite && late.finite);
        CHECK(on.rms > 0.01 && on.peak <= 1.0f);
        CHECK(t2.rms < t1.rms);
        CHECK(idleLate > 0);
    }
}

#include "audio/master_color.h"

TEST(master_colour_curves_bounded) {
    for (int model = 1; model <= 4; ++model) {
        ColorState st;
        float x[kBlock];
        float mx = 0.0f;
        bool finite = true;
        for (int b = 0; b < 200; ++b) {
            for (uint32_t i = 0; i < kBlock; ++i) x[i] = 1.6f * noise();
            colorBlock(model, x, kBlock, st);
            for (uint32_t i = 0; i < kBlock; ++i) {
                finite = finite && std::isfinite(x[i]);
                mx = std::fmax(mx, std::fabs(x[i]));
            }
        }
        // Small signals pass nearly unchanged (gentle curves).
        ColorState s2;
        for (uint32_t i = 0; i < kBlock; ++i) x[i] = 0.01f * float(i % 7) - 0.03f;
        float y[kBlock];
        for (uint32_t i = 0; i < kBlock; ++i) y[i] = x[i];
        colorBlock(model, y, kBlock, s2);
        float err = 0.0f;
        for (uint32_t i = 64; i < kBlock; ++i) err = std::fmax(err, std::fabs(y[i] - x[i]));
        std::printf("  colour %d: max %.3f small-signal err %.4f\n", model, double(mx), double(err));
        CHECK(finite);
        CHECK(mx < 1.6f);
        if (model != 1) CHECK(err < 0.01f);   // ToTape filters (head bump / roll-off)
    }
}
