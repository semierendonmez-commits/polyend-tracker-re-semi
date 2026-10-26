// Live generators (live_gen.h).
#include "app/live_gen.h"

#include <cstring>

#include "app/app.h"
#include "sdram_bss.h"
#include "sequencer/seq_playmodes.h"

namespace ot {
namespace live {

namespace {

struct AcidSlot {
    bool used = false;
    uint8_t track = 0;
    uint8_t pattern = 0;
    int16_t s0 = 0, n = 0;
    int16_t lastStep = -1;
    int16_t v[gen::kParams] = {};
    uint8_t instrument = 0;
    int16_t rootNote = 36;
    uint16_t scaleMask = 0;
    uint32_t rng = 1;
    int8_t applied = 0;       // density the steps were written with
    int16_t appliedAcc = -1, appliedSlide = -1;   // accent / slide amounts (%) applied
    uint16_t loops = 0;
    uint32_t hash = 0;        // of the steps as written (a hand edit ends the slot)
    gen::MonoStep ph[kStepCount];
};

struct BBSlot {
    bool used = false;
    uint8_t t0 = 0, t1 = 0, pattern = 0;
    int16_t s0 = 0, n = 0, lastStep = -1;
    int16_t v[gen::kParams] = {};
    uint8_t instrument = 0;
    int16_t rootNote = 36;
    uint16_t scaleMask = 0;
    int16_t sliceCount = 0;
    uint16_t slices[kSliceCount] = {};
    bool beatSlice = false;
    float sampleBeats = 0;
    uint32_t rng = 1;
    uint16_t loops = 0;
    uint32_t hash = 0;
};

struct State {
    AcidSlot acid[kAcidSlots];
    BBSlot bb[kBBSlots];
    int lastPos = -1;          // pattern position (B mode re-cuts per loop)
    uint16_t loops = 0;
    uint8_t tableTmp[seq::kPlayModeTableLen][3];
    int16_t bbA[10] = {}, bbB[10] = {};
    bool bbSet[10] = {};
    int8_t densOff[kTrackCount] = {};
    int8_t accOff[kTrackCount] = {}, slideOff[kTrackCount] = {}, mutOff[kTrackCount] = {};
    int8_t bbAOff = 0, bbBOff = 0;
    uint8_t bbRecut = 0;
    bool recutNow = false, tablesDirty = false;
    uint32_t genoSeed = 0x9E3779B9u;
    uint16_t genoLoops[kTrackCount] = {};
    bool wasPlaying = true;   // builds the tables on the first stopped pass
    bool firstStart = true;   // the boot-time set is rebuilt at the first play (user timing in the seed)
    uint32_t tableSeed = 0x2545F491u;
};

// Main-loop data only: SDRAM on the device.
OT_SDRAM_BSS SdramLazy<State> gState;

gen::Target targetOf(App& a, const AcidSlot& s) {
    gen::Target t;
    t.pat = &a.pattern();
    t.t0 = t.t1 = s.track;
    t.s0 = s.s0;
    t.s1 = s.s0 + s.n - 1;
    t.instrument = s.instrument;
    t.rootNote = s.rootNote;
    t.scaleMask = s.scaleMask;
    return t;
}

uint32_t hashRange(const Pattern& p, int t0, int t1, int s0, int n) {
    uint32_t h = 2166136261u;
    for (int t = t0; t <= t1; ++t) {
        const Track& tr = p.tracks[t];
        for (int i = 0; i < n && s0 + i < kStepCount; ++i) {
            const Step& st = tr.steps[s0 + i];
            const uint8_t b[6] = {uint8_t(st.note), st.instrument, st.fx[0].type, st.fx[0].value, st.fx[1].type,
                                  st.fx[1].value};
            for (uint8_t x : b) h = (h ^ x) * 16777619u;
        }
    }
    return h;
}
uint32_t hashSteps(const Pattern& p, const AcidSlot& s) { return hashRange(p, s.track, s.track, s.s0, s.n); }
uint32_t hashSteps(const Pattern& p, const BBSlot& s) { return hashRange(p, s.t0, s.t1, s.s0, s.n); }

// New cut tables for the B play modes (time mixed into the seed: a new set
// every power-on too). Built aside and copied in one go per mode.
void rebuildTables(App& a, State& st) {
    for (int m = 0; m < seq::kBBModeCount; ++m) {
        st.tableSeed = st.tableSeed * 1103515245u + 12345u + a.micros();
        // A / B: the Generate popup's (else the default), plus the Perform
        // offsets, kept inside the procedure's range.
        int ab[2] = {-1, -1};
        for (int w = 0; w < 2; ++w) {
            int16_t lo, hi, def;
            if (!gen::bbProcRange(m, w, lo, hi, def)) continue;
            const int base = st.bbSet[m] ? (w ? st.bbB[m] : st.bbA[m]) : def;
            const int off = w ? st.bbBOff : st.bbAOff;
            int v = base + off;
            ab[w] = v < lo ? lo : (v > hi ? hi : v);
        }
        gen::bbPlayTable(m, ab[0], ab[1], st.tableSeed | 1u, st.tableTmp);
        std::memcpy(seq::gBBPlayTables[m], st.tableTmp, sizeof(st.tableTmp));
    }
    seq::gBBPlayTablesReady = true;
    seq::gTiltSeed = st.tableSeed * 2654435761u + 1u;   // a new TILT game too
}

// BBCut Mutate: the same settings, a new random draw over the slot's steps.
void recut(App& a, BBSlot& s, int aOff, int bOff) {
    gen::Target t;
    t.pat = &a.pattern();
    t.t0 = s.t0;
    t.t1 = s.t1;
    t.s0 = s.s0;
    t.s1 = s.s0 + s.n - 1;
    t.instrument = s.instrument;
    t.rootNote = s.rootNote;
    t.scaleMask = s.scaleMask;
    t.slices.count = s.sliceCount;
    t.slices.start = s.slices;
    t.slices.beatSlice = s.beatSlice;
    t.slices.sampleBeats = s.sampleBeats;
    s.rng = s.rng * 1664525u + 1013904223u + a.micros();   // a new draw every time
    t.entropy = s.rng;
    int16_t v[gen::kParams];
    for (int i = 0; i < gen::kParams; ++i) v[i] = s.v[i];
    // Perform "BBCut A" / "BBCut B" move the slot's A / B too.
    for (int w = 0; w < 2; ++w) {
        int16_t lo, hi, def;
        if (!gen::bbProcRange(v[0], w, lo, hi, def)) continue;
        const int x = v[1 + w] + (w ? bOff : aOff);
        v[1 + w] = int16_t(x < lo ? lo : (x > hi ? hi : x));
    }
    v[gen::kSeedParam] = 0;   // random: the entropy above
    gen::generate(gen::kBBCut, v, t);
    s.hash = hashSteps(a.pattern(), s);
}

// Genotype walk over track t's own steps (play modes G1..G3): step to step
// in order, but at a step with a note it may jump to another step holding
// the same note on the same beat phase (else any phase), and go on from
// there: the track's material, a new order every loop.
void buildGeno(App& a, State& st, int t, int level) {
    static const float kVar[3] = {0.2f, 0.45f, 0.75f};
    const Track& tr = a.pattern().tracks[t];
    const int len = tr.length();
    st.genoSeed = st.genoSeed * 1664525u + 1013904223u + a.micros();
    gen::Rng rng(st.genoSeed | 1u);
    const float var = kVar[level < 0 ? 0 : (level > 2 ? 2 : level)];
    int pos = 0;
    uint8_t cand[kStepCount];
    for (int k = 0; k < seq::kPlayModeTableLen; ++k) {
        seq::gGenoTables[t][k] = uint8_t(pos);
        int next = (pos + 1) % len;
        const int8_t want = tr.steps[next].note;
        if (want >= 0 && rng.coin(var)) {
            int n = 0;
            for (int pass = 0; pass < 2 && n == 0; ++pass)
                for (int j = 0; j < len; ++j)
                    if (j != next && tr.steps[j].note == want && (pass == 1 || (j & 3) == (next & 3)))
                        cand[n++] = uint8_t(j);
            if (n) next = cand[rng.below(n)];
        }
        pos = next;
    }
    seq::gGenoReady[t] = true;
}

int effectiveDensity(const State& st, const AcidSlot& s) {
    const int d = s.v[0] + st.densOff[s.track];
    return d < -7 ? -7 : (d > 7 ? 7 : d);
}

void write(App& a, AcidSlot& s) {
    gen::writeMono(a.pattern(), s.track, s.s0, s.ph, s.n, s.instrument);
    s.hash = hashSteps(a.pattern(), s);
}

}  // namespace

void service(App& a) {
    State& st = gState.get();
    const bool playing = a.seq.isPlaying();
    // B play modes (Config > BB play modes): new cuts at every stop, every N
    // loops while playing, or locked (built once).
    uint8_t reseed = a.cfg.bbReseed;
    if (st.bbRecut >= 2 && st.bbRecut <= 6) reseed = uint8_t(st.bbRecut - 1);   // Perform "BBCut Recut"
    if (st.tablesDirty || st.recutNow) {
        st.tablesDirty = false;
        rebuildTables(a, st);
    }
    // Genotype play modes: a walk for a track entering G1..G3, a new one at
    // each of its loop ends (pattern position wrap).
    for (int t = 0; t < kTrackCount; ++t) {
        const uint8_t m = a.seq.perfPlayMode(t);
        if (!seq::isGenoMode(m)) {
            seq::gGenoReady[t] = false;
            continue;
        }
        if (!seq::gGenoReady[t] || st.genoLoops[t] != st.loops) {
            st.genoLoops[t] = st.loops;
            buildGeno(a, st, t, m - seq::kPlayFirstGeno);
        }
    }
    if (!playing && st.wasPlaying) {
        if (reseed != Config::kBBLocked || !seq::gBBPlayTablesReady) rebuildTables(a, st);
        for (AcidSlot& s : st.acid) s.lastStep = -1;
        for (BBSlot& s : st.bb) s.lastStep = -1;
        st.lastPos = -1;
        st.loops = 0;
    }
    if (playing && !st.wasPlaying && st.firstStart) {
        st.firstStart = false;
        if (reseed != Config::kBBLocked) rebuildTables(a, st);
    }
    st.wasPlaying = playing;
    if (playing) {
        const int pos = a.seq.patternPosition();
        if (st.lastPos >= 0 && pos < st.lastPos) {
            ++st.loops;
            static const uint16_t kN[5] = {0, 1, 2, 4, 8};
            if (reseed >= 1 && reseed <= 4 && st.loops % kN[reseed] == 0) rebuildTables(a, st);
        }
        st.lastPos = pos;
    }
    bool edited = false;
    static const uint16_t kEvery[4] = {1, 2, 4, 8};
    for (AcidSlot& s : st.acid) {
        if (!s.used) continue;
        if (a.patternNumber() != s.pattern) continue;
        if (hashSteps(a.pattern(), s) != s.hash) {   // edited by hand: hands off
            s.used = false;
            continue;
        }
        bool dirty = false;
        if (playing) {
            const int now = a.seq.step(s.track);
            // The loop end: the track's step went back (forward play).
            if (s.lastStep >= 0 && now < s.lastStep) {
                ++s.loops;
                const int every = kEvery[s.v[6] < 0 || s.v[6] > 3 ? 0 : s.v[6]];
                int mut = s.v[4] + st.mutOff[s.track];   // Perform "Acid Mutate"
                mut = mut < 0 ? 0 : (mut > 100 ? 100 : mut);
                if (mut > 0 && s.loops % every == 0) {
                    gen::Rng rng(s.rng);
                    gen::Target t = targetOf(a, s);
                    gen::acidMutate(s.v, t, rng, s.ph, s.n, mut / 200.0f);
                    s.appliedAcc = s.appliedSlide = -1;   // re-applies the Perform amounts
                    s.rng = rng.s;
                    s.applied = int8_t(100);   // force the rewrite below
                }
            }
            s.lastStep = int16_t(now);
        }
        // Perform "Acid Accent" / "Acid Slide".
        const int acc = s.v[1] + st.accOff[s.track], sl = s.v[2] + st.slideOff[s.track];
        const int16_t accC = int16_t(acc < 0 ? 0 : (acc > 100 ? 100 : acc));
        const int16_t slC = int16_t(sl < 0 ? 0 : (sl > 100 ? 100 : sl));
        if (accC != s.appliedAcc || slC != s.appliedSlide) {
            gen::acidApplyAccentSlide(s.ph, s.n, accC / 100.0f, slC / 100.0f);
            s.appliedAcc = accC;
            s.appliedSlide = slC;
            s.applied = int8_t(100);   // re-applies density (slides) and rewrites
        }
        const int d = effectiveDensity(st, s);
        if (d != s.applied) {
            gen::acidApplyDensity(s.ph, s.n, d, s.rootNote);
            s.applied = int8_t(d);
            dirty = true;
        }
        if (dirty) {
            write(a, s);
            edited = true;
        }
    }
    static const uint16_t kBBEvery[5] = {0, 1, 2, 4, 8};
    for (BBSlot& s : st.bb) {
        if (!s.used || a.patternNumber() != s.pattern) continue;
        if (hashSteps(a.pattern(), s) != s.hash) {   // edited by hand: hands off
            s.used = false;
            continue;
        }
        if (st.recutNow) {
            recut(a, s, st.bbAOff, st.bbBOff);
            edited = true;
        }
        if (!playing) continue;
        const int now = a.seq.step(s.t0);
        if (s.lastStep >= 0 && now < s.lastStep) {
            ++s.loops;
            const int m = s.v[4] < 1 || s.v[4] > 4 ? 1 : s.v[4];
            if (s.loops % kBBEvery[m] == 0) {
                recut(a, s, st.bbAOff, st.bbBOff);
                edited = true;
            }
        }
        s.lastStep = int16_t(now);
    }
    st.recutNow = false;
    if (edited) a.patternEdited();
}

void acid(App& a, const int16_t* v, const gen::Target& t, uint32_t rngSeed) {
    State& st = gState.get();
    // An existing slot for the track, else a free one, else the oldest.
    AcidSlot* slot = nullptr;
    for (AcidSlot& s : st.acid)
        if (s.used && s.track == t.t0) slot = &s;
    if (!slot)
        for (AcidSlot& s : st.acid)
            if (!s.used) {
                slot = &s;
                break;
            }
    if (!slot) {
        for (int i = 0; i + 1 < kAcidSlots; ++i) st.acid[i] = st.acid[i + 1];
        slot = &st.acid[kAcidSlots - 1];
    }
    slot->used = true;
    slot->track = uint8_t(t.t0);
    slot->pattern = uint8_t(a.patternNumber());
    slot->s0 = int16_t(t.s0);
    slot->n = int16_t(t.s1 - t.s0 + 1);
    for (int i = 0; i < gen::kParams; ++i) slot->v[i] = v[i];
    slot->instrument = t.instrument;
    slot->rootNote = int16_t(t.rootNote);
    slot->scaleMask = t.scaleMask;
    slot->lastStep = -1;
    slot->loops = 0;
    // The same phrase the generate wrote (same RNG seed, same order).
    gen::Rng rng(rngSeed);
    gen::acidPhrase(v, t, rng, slot->ph, slot->n);
    slot->rng = rng.s;
    slot->applied = int8_t(v[0] < -7 ? -7 : (v[0] > 7 ? 7 : v[0]));
    slot->hash = hashSteps(a.pattern(), *slot);
    // A density offset already on: the next service pass applies it.
}

void release(int track) {
    for (AcidSlot& s : gState.get().acid)
        if (s.used && s.track == track) s.used = false;
    for (BBSlot& s : gState.get().bb)
        if (s.used && track >= s.t0 && track <= s.t1) s.used = false;
}

bool acidActive(int track) {
    for (const AcidSlot& s : gState.get().acid)
        if (s.used && s.track == track) return true;
    return false;
}

void setAcidDensityOffset(int track, int offset) {
    if (track < 0 || track >= kTrackCount) return;
    gState.get().densOff[track] = int8_t(offset < -14 ? -14 : (offset > 14 ? 14 : offset));
}

void setAcidPerf(int track, int accent, int slide, int mutate) {
    if (track < 0 || track >= kTrackCount) return;
    State& st = gState.get();
    auto c = [](int v) { return int8_t(v < -100 ? -100 : (v > 100 ? 100 : v)); };
    st.accOff[track] = c(accent);
    st.slideOff[track] = c(slide);
    st.mutOff[track] = c(mutate);
}

void setBBPerf(int aOff, int bOff, int rolls, int recut) {
    State& st = gState.get();
    if (aOff != st.bbAOff || bOff != st.bbBOff) st.recutNow = true;   // B modes and Mutate slots cut again
    st.bbAOff = int8_t(aOff);
    st.bbBOff = int8_t(bOff);
    seq::gBBRollMode = uint8_t(rolls < 0 || rolls > 3 ? 0 : rolls);
    st.bbRecut = uint8_t(recut < 0 || recut > 6 ? 0 : recut);
}

void recutNow() { gState.get().recutNow = true; }

void setBBParams(int proc, int a, int b) {
    if (proc < 0 || proc >= 10) return;
    State& st = gState.get();
    st.bbA[proc] = int16_t(a);
    st.bbB[proc] = int16_t(b);
    st.bbSet[proc] = true;
}

void bbcut(App& a, const int16_t* v, const gen::Target& t, uint32_t rngSeed) {
    State& st = gState.get();
    if (v[4] <= 0) return;   // Mutate Off: nothing live (release() already ran)
    BBSlot* slot = nullptr;
    for (BBSlot& s : st.bb)
        if (!s.used) {
            slot = &s;
            break;
        }
    if (!slot) {
        st.bb[0] = st.bb[1];
        slot = &st.bb[1];
    }
    *slot = BBSlot{};
    slot->used = true;
    slot->t0 = uint8_t(t.t0);
    slot->t1 = uint8_t(t.t1);
    slot->pattern = uint8_t(a.patternNumber());
    slot->s0 = int16_t(t.s0);
    slot->n = int16_t(t.s1 - t.s0 + 1);
    for (int i = 0; i < gen::kParams; ++i) slot->v[i] = v[i];
    slot->instrument = t.instrument;
    slot->rootNote = int16_t(t.rootNote);
    slot->scaleMask = t.scaleMask;
    slot->sliceCount = int16_t(t.slices.count > kSliceCount ? kSliceCount : t.slices.count);
    for (int i = 0; i < slot->sliceCount; ++i) slot->slices[i] = t.slices.start[i];
    slot->beatSlice = t.slices.beatSlice;
    slot->sampleBeats = t.slices.sampleBeats;
    slot->rng = rngSeed ^ a.micros();
    slot->hash = hashSteps(a.pattern(), *slot);
}

bool bbcutActive(int track) {
    for (const BBSlot& s : gState.get().bb)
        if (s.used && track >= s.t0 && track <= s.t1) return true;
    return false;
}

void stopAll() {
    for (AcidSlot& s : gState.get().acid) s.used = false;
    for (BBSlot& s : gState.get().bb) s.used = false;
}

}  // namespace live
}  // namespace ot
