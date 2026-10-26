// Marbles-style random sequence (after Mutable Instruments Marbles, Emilie
// Gillet, MIT): a T generator decides which steps play, an X generator draws
// their pitch from a distribution (Spread = its width, Bias = its centre)
// quantized by Steps; Deja vu replays the last Length draws instead of new
// ones (0 = always new, 100 = a locked loop).
#include <cmath>
#include <cstdio>

#include "gen/gen_internal.h"

namespace ot {
namespace gen {

namespace {

float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

// One X value 0..1. Spread 0: the bias itself; up to ~0.6 a bell around it
// getting wider; then towards uniform; at the top the two extremes (the
// higher one more likely with a high bias).
float drawX(Rng& rng, float spread, float bias) {
    const float z = (rng.uni() + rng.uni() + rng.uni() + rng.uni() - 2.0f) * 1.7320508f;   // ~N(0, 1)
    if (spread <= 0.6f) return clamp01(bias + z * spread * 0.35f);
    const float bell = clamp01(bias + z * 0.21f);
    if (spread <= 0.8f) {
        const float m = (spread - 0.6f) / 0.2f;
        return rng.coin(m) ? rng.uni() : bell;
    }
    const float m = (spread - 0.8f) / 0.2f;
    if (!rng.coin(m)) return rng.uni();
    const float edge = rng.uni() * 0.08f;
    return rng.coin(bias) ? 1.0f - edge : edge;
}

}  // namespace

int marblesNote(float x, int mode, const Target& t, int rangeOct) {
    uint8_t deg[12];
    const int nd = scaleDegrees(t.scaleMask, deg);
    const int span = 12 * rangeOct;
    const float semis = x * float(span);
    if (mode == 0) return t.rootNote + int(semis + 0.5f);   // chromatic
    // Allowed degree indices per octave for each mode.
    uint8_t allow[12];
    int na = 0;
    if (mode == 1) {
        for (int i = 0; i < nd; ++i) allow[na++] = uint8_t(i);
    } else if (mode == 2) {   // triad: 1, 3, 5 of the scale
        allow[na++] = 0;
        if (nd > 2) allow[na++] = 2;
        if (nd > 4) allow[na++] = 4;
    } else if (mode == 3) {   // root and fifth
        allow[na++] = 0;
        if (nd > 4) allow[na++] = 4;
    } else {
        allow[na++] = 0;
    }
    int best = t.rootNote, bestD = 1 << 20;
    for (int o = 0; o <= rangeOct; ++o)
        for (int k = 0; k < na; ++k) {
            const int note = t.rootNote + 12 * o + deg[allow[k]];
            const int d = std::abs(int(float(note - t.rootNote) * 16.0f - semis * 16.0f));
            if (d < bestD) {
                bestD = d;
                best = note;
            }
        }
    return best;
}

void genMarbles(const int16_t* v, Target& t, Rng& rng, Result& r) {
    const float density = v[0] / 100.0f;
    const float dv = v[1] / 100.0f;
    const int len = v[2] < 1 ? 1 : (v[2] > 16 ? 16 : v[2]);
    const float spread = v[3] / 100.0f, bias = v[4] / 100.0f;
    const int steps = v[6];
    const int range = v[7] + 1;
    const int model = v[8];
    const float jitter = v[9] / 100.0f;
    const int every = v[10] == 1 ? 2 : 1;   // 1/8: a draw every second step
    const int n = t.s1 - t.s0 + 1;
    const bool two = model == 3 && t.t1 > t.t0;
    float tMem[16], xMem[16];
    bool tSet[16] = {}, xSet[16] = {};
    int notes = 0;
    bool prevHit = false;
    for (int tr = t.t0; tr <= (two ? t.t0 + 1 : t.t0); ++tr)
        for (int i = 0; i < n && t.s0 + i < kStepCount; ++i) t.pat->tracks[tr].steps[t.s0 + i].clear();
    for (int i = 0, tick = 0; i < n; i += every, ++tick) {
        const int s = t.s0 + i;
        if (s >= kStepCount) break;
        const int lp = tick % len;
        // T: replayed or new.
        float u;
        if (tSet[lp] && rng.coin(dv)) {
            u = tMem[lp];
        } else {
            u = rng.uni();
            tMem[lp] = u;
            tSet[lp] = true;
        }
        bool hit;
        int track = t.t0;
        switch (model) {
            case 1: {   // clusters: a hit makes the next one likelier
                const float p = prevHit ? density + (1.0f - density) * 0.55f : density * 0.55f;
                hit = u < p;
                break;
            }
            case 2: {   // drums: stronger on the beat
                const float w = (i % 4 == 0) ? 1.35f : ((i % 2 == 0) ? 0.85f : 0.55f);
                hit = u < clamp01(density * w);
                break;
            }
            case 3:   // complementary: always a hit, on the first track with p = density
                hit = true;
                if (two && u >= density) track = t.t0 + 1;
                break;
            default: hit = u < density; break;
        }
        prevHit = hit;
        // X: replayed or new (drawn every tick so the loop stays aligned).
        float x;
        if (xSet[lp] && rng.coin(dv)) {
            x = xMem[lp];
        } else {
            x = drawX(rng, spread, bias);
            xMem[lp] = x;
            xSet[lp] = true;
        }
        const uint8_t jit = jitter > 0.0f ? uint8_t(rng.uni() * jitter * 60.0f) : 0;
        if (!hit) continue;
        Step& st = t.pat->tracks[track].steps[s];
        int note = marblesNote(x, steps, t, range);
        st.note = int8_t(note < 0 ? 0 : (note > 127 ? 127 : note));
        st.instrument = t.instrument;
        if (jit >= 3) putFx(st, kFxMicroMove, jit);
        ++notes;
    }
    std::snprintf(r.msg, sizeof(r.msg), "Marbles: %d notes", notes);
    r.ok = true;
}

}  // namespace gen
}  // namespace ot
