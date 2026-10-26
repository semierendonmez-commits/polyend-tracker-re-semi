// PLT synth engine (plt.h).
#include "audio/engines/plt.h"

#include <cmath>

#include "fastrun.h"

namespace ot {
namespace audio {
namespace plt {

namespace {

const float kSine[257] = {
    0.0000000f, 0.0245412f, 0.0490677f, 0.0735646f, 0.0980171f, 0.1224107f, 0.1467305f, 0.1709619f,
    0.1950903f, 0.2191012f, 0.2429802f, 0.2667128f, 0.2902847f, 0.3136817f, 0.3368899f, 0.3598950f,
    0.3826834f, 0.4052413f, 0.4275551f, 0.4496113f, 0.4713967f, 0.4928982f, 0.5141027f, 0.5349976f,
    0.5555702f, 0.5758082f, 0.5956993f, 0.6152316f, 0.6343933f, 0.6531728f, 0.6715590f, 0.6895405f,
    0.7071068f, 0.7242471f, 0.7409511f, 0.7572088f, 0.7730105f, 0.7883464f, 0.8032075f, 0.8175848f,
    0.8314696f, 0.8448536f, 0.8577286f, 0.8700870f, 0.8819213f, 0.8932243f, 0.9039893f, 0.9142098f,
    0.9238795f, 0.9329928f, 0.9415441f, 0.9495282f, 0.9569403f, 0.9637761f, 0.9700313f, 0.9757021f,
    0.9807853f, 0.9852776f, 0.9891765f, 0.9924795f, 0.9951847f, 0.9972905f, 0.9987955f, 0.9996988f,
    1.0000000f, 0.9996988f, 0.9987955f, 0.9972905f, 0.9951847f, 0.9924795f, 0.9891765f, 0.9852776f,
    0.9807853f, 0.9757021f, 0.9700313f, 0.9637761f, 0.9569403f, 0.9495282f, 0.9415441f, 0.9329928f,
    0.9238795f, 0.9142098f, 0.9039893f, 0.8932243f, 0.8819213f, 0.8700870f, 0.8577286f, 0.8448536f,
    0.8314696f, 0.8175848f, 0.8032075f, 0.7883464f, 0.7730105f, 0.7572088f, 0.7409511f, 0.7242471f,
    0.7071068f, 0.6895405f, 0.6715590f, 0.6531728f, 0.6343933f, 0.6152316f, 0.5956993f, 0.5758082f,
    0.5555702f, 0.5349976f, 0.5141027f, 0.4928982f, 0.4713967f, 0.4496113f, 0.4275551f, 0.4052413f,
    0.3826834f, 0.3598950f, 0.3368899f, 0.3136817f, 0.2902847f, 0.2667128f, 0.2429802f, 0.2191012f,
    0.1950903f, 0.1709619f, 0.1467305f, 0.1224107f, 0.0980171f, 0.0735646f, 0.0490677f, 0.0245412f,
    0.0000000f, -0.0245412f, -0.0490677f, -0.0735646f, -0.0980171f, -0.1224107f, -0.1467305f, -0.1709619f,
    -0.1950903f, -0.2191012f, -0.2429802f, -0.2667128f, -0.2902847f, -0.3136817f, -0.3368899f, -0.3598950f,
    -0.3826834f, -0.4052413f, -0.4275551f, -0.4496113f, -0.4713967f, -0.4928982f, -0.5141027f, -0.5349976f,
    -0.5555702f, -0.5758082f, -0.5956993f, -0.6152316f, -0.6343933f, -0.6531728f, -0.6715590f, -0.6895405f,
    -0.7071068f, -0.7242471f, -0.7409511f, -0.7572088f, -0.7730105f, -0.7883464f, -0.8032075f, -0.8175848f,
    -0.8314696f, -0.8448536f, -0.8577286f, -0.8700870f, -0.8819213f, -0.8932243f, -0.9039893f, -0.9142098f,
    -0.9238795f, -0.9329928f, -0.9415441f, -0.9495282f, -0.9569403f, -0.9637761f, -0.9700313f, -0.9757021f,
    -0.9807853f, -0.9852776f, -0.9891765f, -0.9924795f, -0.9951847f, -0.9972905f, -0.9987955f, -0.9996988f,
    -1.0000000f, -0.9996988f, -0.9987955f, -0.9972905f, -0.9951847f, -0.9924795f, -0.9891765f, -0.9852776f,
    -0.9807853f, -0.9757021f, -0.9700313f, -0.9637761f, -0.9569403f, -0.9495282f, -0.9415441f, -0.9329928f,
    -0.9238795f, -0.9142098f, -0.9039893f, -0.8932243f, -0.8819213f, -0.8700870f, -0.8577286f, -0.8448536f,
    -0.8314696f, -0.8175848f, -0.8032075f, -0.7883464f, -0.7730105f, -0.7572088f, -0.7409511f, -0.7242471f,
    -0.7071068f, -0.6895405f, -0.6715590f, -0.6531728f, -0.6343933f, -0.6152316f, -0.5956993f, -0.5758082f,
    -0.5555702f, -0.5349976f, -0.5141027f, -0.4928982f, -0.4713967f, -0.4496113f, -0.4275551f, -0.4052413f,
    -0.3826834f, -0.3598950f, -0.3368899f, -0.3136817f, -0.2902847f, -0.2667128f, -0.2429802f, -0.2191012f,
    -0.1950903f, -0.1709619f, -0.1467305f, -0.1224107f, -0.0980171f, -0.0735646f, -0.0490677f, -0.0245412f,
    -0.0000000f,
};

// sin(2 pi x), x any real (wrapped).
inline float sin2pi(float x) {
    x -= float(int32_t(x));
    if (x < 0.0f) x += 1.0f;
    const float f = x * 256.0f;
    const int i = int(f);
    const float fr = f - float(i);
    return kSine[i] + fr * (kSine[i + 1] - kSine[i]);
}

// 2-Op FM ratios (Harmonics quantized), Plaits-like spread.
const float kRatios[13] = {0.5f, 0.70710678f, 1.0f, 1.41421356f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
constexpr int kPartials = 16;

struct Voice {
    float car, mod;          // FM phases 0..1
    float fbMod, fbCar;      // last outputs (feedback)
    float hc, hs;            // Harmonic: fundamental phasor (cos, sin)
    float env;               // amp envelope 0..1
    float tenv;              // timbre envelope 0..1 (decay to sustain like the amp env, no release)
    float pitch, glideFrom;  // note pitch, glide start
    float glideT;            // 0..1 glide progress
    float lfo;               // LFO phase 0..1
    float amp[kPartials];    // Harmonic: partial levels at the last block end
    uint8_t stage;           // 0 idle, 1 attack, 2 decay / sustain, 3 release
};
static_assert(sizeof(Voice) <= kSynthVoiceStateBytes, "PLT voice state");

inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float expCoef(float t) {
    if (t < 0.0005f) return 1.0f;
    return 1.0f - std::exp(-5.0f / (t * kSampleRate));
}

void partials(float harm, float timbre, float morph, float f0, float* out) {
    const float centre = 1.0f + harm * float(kPartials - 1);
    const float width = 0.35f + 7.0f * timbre * timbre;
    const float odd = morph > 0.5f ? 1.0f : 2.0f * morph;          // morph 0: even only (plus the fundamental)
    const float even = morph < 0.5f ? 1.0f : 2.0f * (1.0f - morph);  // morph 1: odd only
    float sum = 0.0f;
    for (int k = 0; k < kPartials; ++k) {
        const float n = float(k + 1);
        float a = 0.0f;
        if (n * f0 < 18000.0f) {
            const float d = (n - centre) / width;
            a = std::exp(-d * d);
            if (k > 0) a *= ((k + 1) & 1) ? odd : even;
        }
        out[k] = a;
        sum += a * a;
    }
    const float norm = sum > 1e-9f ? 0.7f / std::sqrt(sum) : 0.0f;
    for (int k = 0; k < kPartials; ++k) out[k] *= norm;
}

class Engine final : public ISynthEngine {
public:
    int32_t id() const override { return kSynthEnginePlt; }
    const char* name() const override { return "PLT"; }

    void noteOn(SynthVoice& v, const SynthParams& p, const SynthNote& n) override {
        Voice& s = v.as<Voice>();
        const bool fresh = !n.reused && !n.overlap;
        const float glide = p.end[kGlide];
        if (n.overlap && glide > 0.0f && !n.fxGlide) {
            s.glideFrom = s.pitch + (s.glideT < 1.0f ? (s.glideFrom - s.pitch) * (1.0f - s.glideT) : 0.0f);
            s.glideT = 0.0f;
        } else {
            s.glideFrom = n.pitch;
            s.glideT = 1.0f;
        }
        s.pitch = n.pitch;
        if (fresh) {
            s.car = s.mod = 0.0f;
            s.hc = 1.0f;
            s.hs = 0.0f;
            s.fbMod = s.fbCar = 0.0f;
            s.lfo = 0.0f;
            for (float& a : s.amp) a = 0.0f;
        }
        if (!n.overlap) {
            s.stage = 1;
            s.tenv = 1.0f;
        }
        v.level = s.env;
    }
    void noteOff(SynthVoice& v, const SynthParams& p) override {
        (void)p;
        Voice& s = v.as<Voice>();
        if (s.stage) s.stage = 3;
    }
    bool render(SynthVoice& v, const SynthParams& p, float g0, float gInc, float* L, float* R) override;
};

OT_FASTRUN_FN bool Engine::render(SynthVoice& v, const SynthParams& p, float g0, float gInc, float* L, float* R) {
    Voice& s = v.as<Voice>();
    if (s.stage == 0) return false;
    const float* E = p.end;
    const int model = E[kModel] >= 0.5f ? 1 : 0;
    // Glide (exponential-ish over Glide Time), then the block's pitch ramp.
    float pitchA = s.pitch, pitchB = s.pitch;
    if (s.glideT < 1.0f) {
        const float gt = E[kGlide] > 0.001f ? E[kGlide] : 0.001f;
        const float step = float(kBlock) / (gt * kSampleRate);
        const float t0 = s.glideT, t1 = t0 + step > 1.0f ? 1.0f : t0 + step;
        pitchA = s.glideFrom + (s.pitch - s.glideFrom) * t0;
        pitchB = s.glideFrom + (s.pitch - s.glideFrom) * t1;
        s.glideT = t1;
    }
    const float tune = E[kTune];
    const float hzA = synthPitchHz(pitchA + tune + v.bendStart), hzB = synthPitchHz(pitchB + tune + v.bend);
    float inc = hzA / kSampleRate;
    const float incStep = (hzB - hzA) / kSampleRate * (1.0f / float(kBlock));
    // Envelopes (block rate for the timbre one), LFO.
    const float attack = E[kAttack] < 0.0005f ? 1.0f : 1.0f / (E[kAttack] * kSampleRate);
    const float kd = expCoef(E[kDecay]), kr = expCoef(E[kRelease]);
    const float sus = clamp01(E[kSustain]);
    const float kdB = 1.0f - std::pow(1.0f - kd, float(kBlock));
    s.tenv += (sus - s.tenv) * kdB;
    s.lfo += E[kLfoFreq] * float(kBlock) / kSampleRate;
    s.lfo -= float(int32_t(s.lfo));
    const float lfo = sin2pi(s.lfo);
    const float timbre = clamp01(E[kTimbre] + E[kTimbreEnv] * s.tenv + E[kLfoTimbre] * lfo * 0.5f);
    const float harm = clamp01(E[kHarmonics]), morph = clamp01(E[kMorph]);
    float env = s.env, g = g0;
    uint8_t st = s.stage;
    if (model == 0) {
        // 2-Op FM: ratio from Harmonics (snapped near the table entries).
        const float rf = harm * 12.0f;
        const int ri = int(rf);
        const float rfr = rf - float(ri);
        const float snap = rfr < 0.2f ? 0.0f : (rfr > 0.8f ? 1.0f : (rfr - 0.2f) / 0.6f);
        const float ratio = ri >= 12 ? kRatios[12] : kRatios[ri] + (kRatios[ri + 1] - kRatios[ri]) * snap;
        const float index = 2.4f * timbre * timbre * timbre + 0.6f * timbre;   // in cycles
        const float selfFb = morph < 0.5f ? (0.5f - morph) * 0.5f : 0.0f;
        const float crossFb = morph > 0.5f ? (morph - 0.5f) * 0.8f : 0.0f;
        float car = s.car, mod = s.mod, fbM = s.fbMod, fbC = s.fbCar;
        for (uint32_t i = 0; i < kBlock; ++i) {
            mod += inc * ratio;
            car += inc;
            mod -= float(int32_t(mod));
            car -= float(int32_t(car));
            const float m = sin2pi(mod + selfFb * fbM + crossFb * fbC);
            fbM = m;
            const float c = sin2pi(car + index * m);
            fbC = c;
            inc += incStep;
            if (st == 1) {
                env += attack;
                if (env >= 1.0f) {
                    env = 1.0f;
                    st = 2;
                }
            } else if (st == 2) {
                env += (sus - env) * kd;
            } else {
                env -= env * kr;
            }
            const float y = 0.7f * c * env * g;
            g += gInc;
            L[i] += y;
            R[i] += y;
        }
        s.car = car;
        s.mod = mod;
        s.fbMod = fbM;
        s.fbCar = fbC;
    } else {
        // Harmonic: partial levels for this block (two block-rate halves
        // blend the change); the fundamental is a rotating phasor and the
        // partial sum sum a_k sin(k x) comes from Clenshaw's recurrence
        // (one multiply-add pair per partial).
        float a1[kPartials], a[kPartials];
        partials(harm, timbre, morph, hzB, a1);
        for (int k = 0; k < kPartials; ++k) a[k] = 0.5f * (a1[k] + s.amp[k]);
        int top = 0;
        for (int k = 0; k < kPartials; ++k)
            if (a[k] > 1e-4f) top = k + 1;
        const float hzM = 0.5f * (hzA + hzB);
        const float wc = sin2pi(hzM / kSampleRate + 0.25f), ws = sin2pi(hzM / kSampleRate);
        float hc = s.hc, hsn = s.hs;
        {   // keep the phasor on the unit circle
            const float m = hc * hc + hsn * hsn;
            const float nrm = 1.5f - 0.5f * m;   // one Newton step towards 1 / sqrt(m)
            hc *= nrm;
            hsn *= nrm;
        }
        for (uint32_t i = 0; i < kBlock; ++i) {
            const float c = hc * wc - hsn * ws;
            hsn = hsn * wc + hc * ws;
            hc = c;
            const float twoC = 2.0f * hc;
            float b1 = 0.0f, b2 = 0.0f;
            for (int k = top - 1; k >= 0; --k) {
                const float b = a[k] + twoC * b1 - b2;
                b2 = b1;
                b1 = b;
            }
            const float out = b1 * hsn;
            if (st == 1) {
                env += attack;
                if (env >= 1.0f) {
                    env = 1.0f;
                    st = 2;
                }
            } else if (st == 2) {
                env += (sus - env) * kd;
            } else {
                env -= env * kr;
            }
            const float y = out * env * g;
            g += gInc;
            L[i] += y;
            R[i] += y;
        }
        s.hc = hc;
        s.hs = hsn;
        for (int k = 0; k < kPartials; ++k) s.amp[k] = a1[k];
    }
    s.env = env;
    s.stage = st;
    v.level = env;
    if ((st == 3 && env < 1.0e-4f) || (st == 2 && sus == 0.0f && env < 1.0e-4f)) {
        s.stage = 0;
        return false;
    }
    return true;
}

Engine gEngine;

}  // namespace

ISynthEngine* engine() { return &gEngine; }

}  // namespace plt
}  // namespace audio
}  // namespace ot
