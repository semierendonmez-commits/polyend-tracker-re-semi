// Grids: Mutable Instruments' topographic drum sequencer (Emilie Gillet,
// grids/pattern_generator.cc, GPL-3.0). A 5 x 5 map of drum patterns; X / Y
// pick a point between four of them, a density per part sets which levels
// fire, chaos perturbs the levels once per bar, levels above 192 are accents.
#include <cstdio>

#include "gen/gen_internal.h"

namespace ot {
namespace gen {

namespace {

#include "gen/grids_nodes.inc"

// grids drum_map[5][5] (node numbers).
const uint8_t kDrumMap[5][5] = {
    {10, 8, 0, 9, 11}, {15, 7, 13, 12, 6}, {18, 14, 4, 5, 3}, {23, 16, 21, 1, 2}, {24, 19, 17, 20, 22},
};

inline uint8_t mix(uint8_t a, uint8_t b, uint8_t balance) {
    return uint8_t((a * (255 - balance) + b * balance) >> 8);   // avrlib U8Mix
}

int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

int mostUsedVoice(const Track& tr, int s0, int n, GridsVoice& out) {
    // The track's most used (instrument, note) in the target steps, else on the whole track.
    struct C {
        uint8_t ins;
        int8_t note;
        int n;
    } c[8] = {};
    int nc = 0;
    for (int pass = 0; pass < 2 && nc == 0; ++pass) {
        const int a = pass ? 0 : s0, b = pass ? tr.length() : s0 + n;
        for (int s = a; s < b && s < kStepCount; ++s) {
            const Step& st = tr.steps[s];
            if (st.note < 0) continue;
            int k = 0;
            while (k < nc && !(c[k].ins == st.instrument && c[k].note == st.note)) ++k;
            if (k == nc) {
                if (nc == 8) continue;
                c[nc++] = C{st.instrument, st.note, 0};
            }
            ++c[k].n;
        }
    }
    if (!nc) return 0;
    int best = 0;
    for (int k = 1; k < nc; ++k)
        if (c[k].n > c[best].n) best = k;
    out.instrument = c[best].ins;
    out.note = c[best].note;
    return 1;
}

}  // namespace

uint8_t gridsLevel(int step32, int part, int x, int y) {
    x = clamp(x, 0, 255);
    y = clamp(y, 0, 255);
    const int i = x >> 6, j = y >> 6;
    const int off = part * 32 + (step32 & 31);
    const uint8_t a = kGridsNodes[kDrumMap[i][j]][off];
    const uint8_t b = kGridsNodes[kDrumMap[i + 1][j]][off];
    const uint8_t c = kGridsNodes[kDrumMap[i][j + 1]][off];
    const uint8_t d = kGridsNodes[kDrumMap[i + 1][j + 1]][off];
    const uint8_t xf = uint8_t(x << 2), yf = uint8_t(y << 2);
    return mix(mix(a, b, xf), mix(c, d, xf), yf);
}

int gridsRender(const int16_t* v, const GridsOffsets& o, uint32_t seed, Pattern& p, int t0, int s0, int n,
                const GridsVoice* voice) {
    const int x = clamp(v[0] + o.x, 0, 100) * 255 / 100;
    const int y = clamp(v[1] + o.y, 0, 100) * 255 / 100;
    int dens[3];
    for (int k = 0; k < 3; ++k) dens[k] = clamp(v[2 + k] + o.density, 0, 100) * 255 / 100;
    const int chaos = clamp(v[6], 0, 100) * 255 / 100;
    const bool accent = v[7] == 0;
    const bool half = v[8] == 1;       // 1/32: one step = a 32nd of the Grids bar
    const int parts = v[9];            // 0 kit on three tracks, 1..3 one part
    const int barSteps = half ? 32 : 16;
    Rng rng(seed | 1u);
    int hits = 0;
    uint8_t pert[3] = {};
    for (int i = 0; i < n; ++i) {
        const int s = s0 + i;
        if (s >= kStepCount) break;
        if (i % barSteps == 0)   // a new perturbation per bar (Grids: at step 0)
            for (int k = 0; k < 3; ++k) pert[k] = uint8_t(((rng.next() >> 24) * uint32_t(chaos >> 2)) >> 8);
        const int g = half ? (i & 31) : ((2 * i) & 31);
        for (int k = 0; k < 3; ++k) {
            if (parts != 0 && k != parts - 1) continue;
            const int t = parts == 0 ? t0 + k : t0;
            if (t >= kTrackCount) continue;
            Step& st = p.tracks[t].steps[s];
            st.clear();
            int level = gridsLevel(g, k, x, y);
            level = level < 255 - pert[k] ? level + pert[k] : 255;
            if (dens[k] == 0 || level <= 255 - dens[k]) continue;   // Grids: level > ~density
            const GridsVoice& vc = voice[parts == 0 ? k : 0];
            st.note = vc.note;
            st.instrument = vc.instrument;
            if (accent && level <= 192) putFx(st, kFxVolume, 70);   // accents (level > 192) play full
            ++hits;
        }
    }
    return hits;
}

void gridsVoices(const Target& t, int parts, GridsVoice* out) {
    const int base = t.rootNote - (t.rootNote % 12) + 12;   // the unity note (60 for samples)
    const int nt = parts == 0 ? 3 : 1;
    for (int k = 0; k < nt; ++k) {
        GridsVoice& v = out[k];
        v.instrument = t.instrument;
        v.note = int8_t(base);
        const int tr = t.t0 + k;
        if (tr < kTrackCount) mostUsedVoice(t.pat->tracks[tr], t.s0, t.s1 - t.s0 + 1, v);
    }
}

void genGrids(const int16_t* v, Target& t, Rng& rng, Result& r) {
    GridsVoice voice[3];
    gridsVoices(t, v[9], voice);
    const int hits = gridsRender(v, GridsOffsets{}, rng.next(), *t.pat, t.t0, t.s0, t.s1 - t.s0 + 1, voice);
    std::snprintf(r.msg, sizeof(r.msg), "Grids: %d hits", hits);
    r.ok = true;
}

}  // namespace gen
}  // namespace ot
