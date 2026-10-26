// OT fork PLT synth engine (audio/engines/plt.h): both models sound, follow
// pitch, respond to Harmonics / Timbre / Morph, release and stay finite.
#include "test.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "audio/engines/plt.h"
#include "formats/patch_pb.h"
#include "model/project.h"

using namespace ot;
using namespace ot::audio;

namespace {
SynthParams params(int model, float harm, float timbre, float morph) {
    SynthParams p;
    p.engine = kSynthEnginePlt;
    int n = 0;
    const PatchParamRange* r = patchEngineParams(kSynthEnginePlt, &n);
    p.count = n;
    for (int i = 0; i < n; ++i) p.start[i] = p.end[i] = r[i].def;
    p.end[plt::kModel] = p.start[plt::kModel] = float(model);
    p.end[plt::kHarmonics] = p.start[plt::kHarmonics] = harm;
    p.end[plt::kTimbre] = p.start[plt::kTimbre] = timbre;
    p.end[plt::kMorph] = p.start[plt::kMorph] = morph;
    p.end[plt::kTimbreEnv] = p.start[plt::kTimbreEnv] = 0.0f;
    p.end[plt::kSustain] = p.start[plt::kSustain] = 1.0f;
    return p;
}
// Renders `blocks` blocks of one note; returns the output.
std::vector<float> play(const SynthParams& p, float pitch, int blocks, int releaseAt = -1, bool* ended = nullptr) {
    SynthVoice v;
    std::memset(v.state, 0, sizeof(v.state));
    SynthNote n;
    n.pitch = pitch;
    ISynthEngine* e = plt::engine();
    e->noteOn(v, p, n);
    std::vector<float> out;
    bool alive = true;
    for (int b = 0; b < blocks && alive; ++b) {
        if (b == releaseAt) e->noteOff(v, p);
        float L[kBlock] = {}, R[kBlock] = {};
        alive = e->render(v, p, 1.0f, 0.0f, L, R);
        out.insert(out.end(), L, L + kBlock);
    }
    if (ended) *ended = !alive;
    return out;
}
// Zero crossings per second (a pitch estimate for a plain tone).
double zc(const std::vector<float>& x, size_t from) {
    int n = 0;
    for (size_t i = from + 1; i < x.size(); ++i) n += (x[i - 1] < 0.0f) != (x[i] < 0.0f);
    return double(n) / 2.0 / (double(x.size() - from) / double(ot::audio::kSampleRate));
}
double rms(const std::vector<float>& x, size_t from) {
    double s = 0;
    for (size_t i = from; i < x.size(); ++i) s += double(x[i]) * x[i];
    return std::sqrt(s / double(x.size() - from));
}
// Energy above ~3 kHz relative to the total (one-pole high-pass).
double brightness(const std::vector<float>& x, size_t from) {
    double e = 0, eh = 0;
    float prev = 0, hp = 0;
    const float a = std::exp(-6.2831853f * 3000.0f / ot::audio::kSampleRate);
    for (size_t i = from; i < x.size(); ++i) {
        hp = a * (hp + x[i] - prev);
        prev = x[i];
        e += double(x[i]) * x[i];
        eh += double(hp) * hp;
    }
    return e > 0 ? eh / e : 0;
}
}  // namespace

TEST(plt_registered_and_patch_init) {
    CHECK(synthEngineFor(kSynthEnginePlt) != nullptr);
    CHECK(std::strcmp(patchEngineName(kSynthEnginePlt), "PLT") == 0);
    CHECK_EQ(patchVolumeParam(kSynthEnginePlt), int(plt::kVolume));
    SynthPatch sp;
    CHECK(patchInit(kSynthEnginePlt, &sp));
    CHECK_EQ(int(sp.paramCount), int(plt::kParamCount));
    CHECK(std::strcmp(synthModelName(SynthModel::PLT), "PLT") == 0);
    CHECK_EQ(int(synthEngineId(SynthModel::PLT)), int(kSynthEnginePlt));
}

TEST(plt_models_sound_and_follow_pitch) {
    for (int model = 0; model < 2; ++model) {
        // A plain tone: FM with no index, Harmonic with the fundamental only.
        const SynthParams p = model == 0 ? params(0, 2.0f / 12.0f, 0.0f, 0.5f) : params(1, 0.0f, 0.0f, 0.5f);
        const std::vector<float> a = play(p, 57.0f, 200);   // A3 220 Hz
        const std::vector<float> b = play(p, 69.0f, 200);   // A4 440 Hz
        const double fa = zc(a, 2000), fb = zc(b, 2000);
        std::printf("  model %d: %.1f Hz / %.1f Hz, rms %.3f\n", model, fa, fb, rms(a, 2000));
        CHECK(std::fabs(fa - 220.0) < 4.0);
        CHECK(std::fabs(fb - 440.0) < 6.0);
        CHECK(rms(a, 2000) > 0.1 && rms(a, 2000) < 1.0);
    }
}

TEST(plt_timbre_and_harmonics_brighten) {
    const double fmDark = brightness(play(params(0, 2.0f / 12.0f, 0.0f, 0.5f), 48.0f, 150), 2000);
    const double fmBright = brightness(play(params(0, 2.0f / 12.0f, 0.9f, 0.5f), 48.0f, 150), 2000);
    const double hDark = brightness(play(params(1, 0.0f, 0.1f, 0.5f), 48.0f, 150), 2000);
    const double hBright = brightness(play(params(1, 1.0f, 0.1f, 0.5f), 48.0f, 150), 2000);
    std::printf("  FM %.4f -> %.4f, Harmonic %.4f -> %.4f\n", fmDark, fmBright, hDark, hBright);
    CHECK(fmBright > fmDark * 3.0 + 0.001);
    CHECK(hBright > hDark * 3.0 + 0.001);
    // Morph extremes stay finite (feedback both ways).
    for (float m : {0.0f, 1.0f}) {
        const std::vector<float> x = play(params(0, 0.5f, 1.0f, m), 40.0f, 200);
        bool finite = true;
        float pk = 0;
        for (float v : x) {
            finite = finite && std::isfinite(v);
            pk = std::fmax(pk, std::fabs(v));
        }
        CHECK(finite && pk < 1.5f);
    }
}

TEST(plt_release_ends_the_voice) {
    SynthParams p = params(1, 0.3f, 0.3f, 0.5f);
    p.end[plt::kRelease] = p.start[plt::kRelease] = 0.05f;
    bool ended = false;
    play(p, 60.0f, 400, 50, &ended);
    CHECK(ended);
}
