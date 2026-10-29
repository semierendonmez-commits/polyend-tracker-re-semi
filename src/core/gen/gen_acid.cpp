// Acid generator: a 303 line after Logarhythm's TB-3PO (O_C Hemisphere),
// whose density works in both directions (density -7..+7):
//
//   * Gates: a step plays with probability 10 + 14 |d| % -- 0 is the
//     sparsest (10 %), both ends play every step.
//   * Pitch variety (pd = d + 7 clamped to 0..8): pd 0 (d -7) only the root,
//     pd 1 the root and the next degree (303 half steps), pd 2..7 a growing
//     set of the lowest scale degrees, d >= +1 the whole scale. A new pitch
//     repeats the previous step's with probability 50 - 6 pd %, so the
//     negative side gives repeating root-heavy lines and the positive side
//     melodic ones.
//   * Octaves: a new pitch jumps an octave up or down 20 % each (TB-3PO), or
//     not at all / wider (the Octaves parameter).
//
// Every step keeps its own random draws (gate, repeat, pitch, octave, accent,
// slide), so the density can change live (Perform FX "Acid Density") and the
// same draws give the line back at the same density: rising |d| only adds
// notes, the pitches move only on the negative side. A slide only happens
// into a step that plays. The live mutation re-rolls some steps' draws at
// each loop end (live_gen.cpp).
#include <cstdio>

#include "gen/gen_internal.h"

namespace ot {
namespace gen {

namespace {

struct AcidCfg {
    float accent, slide, gate;
};

AcidCfg cfg(const int16_t* v) {
    static const float kGate[3] = {0.5f, 0.3f, 0.8f};
    AcidCfg c;
    c.accent = v[1] / 100.0f;
    c.slide = v[2] / 100.0f;
    c.gate = kGate[v[7] < 0 || v[7] > 2 ? 0 : v[7]];
    return c;
}

uint8_t byte(Rng& rng) { return uint8_t(rng.below(256)); }
// A 16-bit draw from the same uniform as before, and "draw below amount".
uint16_t q16(float u) {
    const float v = u * 65536.0f;
    return uint16_t(v >= 65535.0f ? 65535.0f : (v < 0.0f ? 0.0f : v));
}
bool below(uint16_t q, float amount) { return float(q) < amount * 65536.0f; }

void rollPitch(Rng& rng, MonoStep& m) {
    m.rep = byte(rng);
    m.pitch = byte(rng);
    m.oct = byte(rng);
}

void rollState(const AcidCfg& c, Rng& rng, MonoStep& m) {
    m.thr = q16(rng.uni());
    m.accThr = q16(rng.uni());
    m.accent = below(m.accThr, c.accent);
    m.slideThr = q16(rng.uni());
    m.slideWant = below(m.slideThr, c.slide);
    m.gate = c.gate;
}

// TB-3PO's available_pitches: the highest scale index used (0 = root only).
int availablePitches(int pd, int nd) {
    if (nd <= 1) return 0;
    if (pd > 7) return nd - 1;
    if (pd < 2) return pd < nd - 1 ? pd : nd - 1;
    int range = nd - 3;
    if (range < 4) range = 4;
    int a = 3 + (pd - 3) * range / 4;
    return a < 1 ? 1 : (a > nd - 1 ? nd - 1 : a);
}

}  // namespace

float acidDensityProb(int d) {
    d = d < -7 ? -7 : (d > 7 ? 7 : d);
    const int a = d < 0 ? -d : d;
    return float(10 + 14 * a) / 100.0f;
}

void acidApplyDensity(MonoStep* ph, int n, int density, int root, uint16_t scaleMask, int octaves) {
    density = density < -7 ? -7 : (density > 7 ? 7 : density);
    uint8_t deg[12];
    const int nd = scaleDegrees(scaleMask, deg);
    int pd = density + 7;
    if (pd > 8) pd = 8;
    const int avail = availablePitches(pd, nd);
    const int repeatPct = 50 - 6 * pd;   // 50 % at -7, 2 % from +1
    const float p = acidDensityProb(density);
    bool any = false;
    int lowest = 0;
    for (int i = 0; i < n; ++i) {
        MonoStep& m = ph[i];
        // Pitch.
        if (i > 0 && int(m.rep) * 100 < repeatPct * 256) {
            m.note = ph[i - 1].note;
        } else {
            const int d = int(m.pitch) * (avail + 1) / 256;
            int oct = 0;
            if (octaves == 1) {
                oct = m.oct < 51 ? 1 : (m.oct < 102 ? -1 : 0);   // 20 % up, 20 % down
            } else if (octaves >= 2) {
                oct = m.oct < 38 ? 1 : (m.oct < 64 ? 2 : (m.oct < 102 ? -1 : 0));
            }
            m.note = int16_t(degreeNote(root, deg, nd, d + oct * nd));
        }
        // Gate.
        m.active = below(m.thr, p);
        any = any || m.active;
        if (m.thr < ph[lowest].thr) lowest = i;
    }
    if (!any && n > 0) ph[lowest].active = true;   // at least one note
    for (int i = 0; i < n; ++i) ph[i].slide = ph[i].active && ph[i].slideWant && ph[(i + 1) % n].active;
}

void acidApplyAccentSlide(MonoStep* ph, int n, float accent, float slide) {
    for (int i = 0; i < n; ++i) {
        ph[i].accent = below(ph[i].accThr, accent);
        ph[i].slideWant = below(ph[i].slideThr, slide);
    }
}

void acidPhrase(const int16_t* v, const Target& t, Rng& rng, MonoStep* ph, int n) {
    const AcidCfg c = cfg(v);
    for (int i = 0; i < n; ++i) {
        MonoStep& m = ph[i];
        m = MonoStep{};
        rollState(c, rng, m);
        rollPitch(rng, m);
    }
    acidApplyDensity(ph, n, v[0], t.rootNote, t.scaleMask, v[3]);
}

void acidMutate(const int16_t* v, const Target& t, Rng& rng, MonoStep* ph, int n, float amount) {
    const AcidCfg c = cfg(v);
    bool any = false;
    for (int i = 0; i < n; ++i) {
        if (!rng.coin(amount)) continue;
        any = true;
        if (rng.coin(0.5f)) rollState(c, rng, ph[i]);
        else rollPitch(rng, ph[i]);
    }
    if (any) acidApplyDensity(ph, n, v[0], t.rootNote, t.scaleMask, v[3]);
}

void genAcid(const int16_t* v, Target& t, Rng& rng, Result& r) {
    const int n = t.s1 - t.s0 + 1;
    MonoStep ph[kStepCount];
    acidPhrase(v, t, rng, ph, n);
    for (int tr = t.t0; tr <= t.t1; ++tr) writeMono(*t.pat, tr, t.s0, ph, n, t.instrument);
    int notes = 0;
    for (int i = 0; i < n; ++i) notes += ph[i].active;
    if (v[4] > 0) std::snprintf(r.msg, sizeof(r.msg), "Acid: %d notes, mutating %d%%", notes, v[4]);
    else std::snprintf(r.msg, sizeof(r.msg), "Acid: %d notes in %d steps", notes, n);
    r.ok = true;
}

}  // namespace gen
}  // namespace ot
