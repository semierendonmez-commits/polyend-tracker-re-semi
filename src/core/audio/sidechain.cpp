// OT fork sidechain ducking (sidechain.h).
#include "audio/sidechain.h"

#include <cmath>

#include "fastrun.h"

namespace ot {
namespace audio {

// The envelope runs per group of 8 samples (5.5 kHz control rate); the gain
// is a linear ramp between the group ends, so steps never click. The
// coefficients are recomputed only when the settings change.
OT_FASTRUN_FN bool SidechainEnv::process(const SidechainSettings& s, bool trigger, const float* src, float* gain) {
    constexpr uint32_t kG = 8;
    constexpr float kMsToSamples = 44.1f;
    const uint32_t key = uint32_t(s.attack) | uint32_t(s.release) << 8 | uint32_t(s.thresh) << 16 |
                         uint32_t(s.hold) << 24;
    if (key != key_ || !coefValid_) {
        key_ = key;
        coefValid_ = true;
        const float attS = float(s.attack) * kMsToSamples + 1.0f;
        const float relS = float(s.release < 1 ? 1 : s.release) * 10.0f * kMsToSamples;
        attInc_ = float(kG) / attS;                       // trigger: linear rise per group
        attC_ = std::exp(-6.9f * float(kG) / attS);       // follower: rise
        relC_ = std::exp(-6.9f * float(kG) / relS);       // -60 dB over the release time
        holdG_ = uint32_t(float(s.hold) * 5.0f * kMsToSamples) / kG;
        const float thrDb = -60.0f + 0.6f * float(s.thresh > 100 ? 100 : s.thresh);
        invThr_ = std::pow(10.0f, -thrDb / 20.0f) * (1.0f / 15.0f);   // up to 1 at 24 dB over
    }
    const float depth = float(s.depth > 100 ? 100 : s.depth) * 0.01f;
    float e = env_;
    const float e0 = e;
    const bool follow = s.mode != 0 && src;
    if (!follow && trigger) {
        stage_ = 1;
        hold_ = holdG_;
    }
    float f = follow_;
    float prev = 1.0f - depth * e;
    for (uint32_t g0 = 0; g0 < kBlock; g0 += kG) {
        if (follow) {
            float a = 0.0f;
            for (uint32_t i = 0; i < kG; ++i) {
                const float v = std::fabs(src[g0 + i]);
                a = v > a ? v : a;
            }
            f = a > f ? a + (f - a) * attC_ : f * relC_;
            float t = f * invThr_ - (1.0f / 15.0f);
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            if (t >= e) {
                e = t;
                hold_ = holdG_;
            } else if (hold_) {
                --hold_;
            } else {
                e = t + (e - t) * relC_;
                if (e < 1e-4f) e = 0.0f;
            }
        } else {
            switch (stage_) {
                case 1:
                    e += attInc_;
                    if (e >= 1.0f) {
                        e = 1.0f;
                        stage_ = 2;
                    }
                    break;
                case 2:
                    if (hold_) --hold_;
                    else stage_ = 3;
                    break;
                case 3:
                    e *= relC_;
                    if (e < 1e-4f) {
                        e = 0.0f;
                        stage_ = 0;
                    }
                    break;
                default: break;
            }
        }
        const float next = 1.0f - depth * e;
        const float step = (next - prev) * (1.0f / float(kG));
        float v = prev;
        for (uint32_t i = 0; i < kG; ++i) {
            v += step;
            gain[g0 + i] = v;
        }
        prev = next;
    }
    follow_ = f;
    env_ = e;
    return depth > 0.0f && (e0 > 0.0f || e > 0.0f);
}

}  // namespace audio
}  // namespace ot
