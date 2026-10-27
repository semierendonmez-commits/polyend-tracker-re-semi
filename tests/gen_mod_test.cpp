// OT fork modular-inspired generators and play modes: Grids drum map,
// Marbles deja vu, Genotype Drummer, the M / TM / R play modes and the live
// Grids Perform offsets.
#include "test.h"

#include <set>
#include <vector>

#include "app/live_gen.h"
#include "app/perf_presets.h"
#include "gen/gen.h"
#include "gen/gen_internal.h"
#include "model/fx.h"
#include "sequencer/seq_playmodes.h"
#include "sequencer/seq_tables.h"
#include "ui_fakes.h"

using namespace ot;

namespace {

Pattern g_pat;

gen::Target target(int s0, int s1, int t0 = 0, int t1 = 0) {
    g_pat.setDefault();
    for (auto& tr : g_pat.tracks) tr.lastStep = 63;
    gen::Target t;
    t.pat = &g_pat;
    t.s0 = s0;
    t.s1 = s1;
    t.t0 = t0;
    t.t1 = t1;
    t.instrument = 2;
    t.rootNote = 48;
    t.scaleMask = 0x0AD5;   // major
    t.entropy = 777;
    return t;
}

void defaults(int k, int16_t* v) {
    for (int i = 0; i < gen::kParams; ++i) v[i] = gen::param(k, i, v).def;
}

bool stepValid(const Step& s) {
    if (s.note < 0 && s.note != kNoteEmpty && s.note != kNoteOff) return false;
    for (const StepFx& f : s.fx) {
        if (f.type == 0) continue;
        if (!fxIdValid(f.type)) return false;
        if (f.value < seq::fxMin(f.type) || f.value > seq::fxMax(f.type)) return false;
    }
    return true;
}

int hitsOn(int t, int s0, int n) {
    int h = 0;
    for (int i = 0; i < n; ++i) h += g_pat.tracks[t].steps[s0 + i].note >= 0;
    return h;
}

}  // namespace

TEST(grids_map_matches_the_nodes) {
    // Corner (0, 0) is node 10; (255, 255) node 22: the levels read straight.
    CHECK_EQ(gen::gridsLevel(0, 0, 0, 0), (((145 * 255) >> 8) * 255) >> 8);   // node 10 BD step 0 = 145, through U8Mix
    // Every level is a mix of the four nodes around: inside 0..255.
    for (int x = 0; x < 256; x += 37)
        for (int y = 0; y < 256; y += 41)
            for (int s = 0; s < 32; ++s)
                for (int p = 0; p < 3; ++p) CHECK(gen::gridsLevel(s, p, x, y) <= 255);
}

TEST(grids_kit_densities_and_accents) {
    int16_t v[gen::kParams];
    defaults(gen::kGrids, v);
    v[2] = 100;   // kick full
    v[3] = 0;     // snare off
    v[4] = 50;
    v[gen::kSeedParam] = 7;
    gen::Target t = target(0, 31, 0, 0);
    // Existing voices: the kick track keeps its instrument / note.
    g_pat.tracks[0].steps[40].note = 50;
    g_pat.tracks[0].steps[40].instrument = 9;
    gen::Result r = gen::generate(gen::kGrids, v, t);
    CHECK(r.ok);
    CHECK(hitsOn(0, 0, 32) > 0);
    CHECK_EQ(hitsOn(1, 0, 32), 0);   // density 0: no snare
    for (int s = 0; s < 32; ++s) {
        const Step& st = g_pat.tracks[0].steps[s];
        if (st.note < 0) continue;
        CHECK_EQ(st.note, 50);
        CHECK_EQ(st.instrument, 9);
        CHECK(stepValid(st));
    }
    // Same seed, same result; Accent Off writes no Volume FX.
    const int k1 = hitsOn(0, 0, 32), h1 = hitsOn(2, 0, 32);
    t = target(0, 31);
    r = gen::generate(gen::kGrids, v, t);
    CHECK_EQ(hitsOn(0, 0, 32), k1);
    CHECK_EQ(hitsOn(2, 0, 32), h1);
    v[7] = 1;
    t = target(0, 31);
    gen::generate(gen::kGrids, v, t);
    for (int tr = 0; tr < 3; ++tr)
        for (int s = 0; s < 32; ++s) CHECK(g_pat.tracks[tr].steps[s].fx[0].type != gen::kFxVolume);
    // More density, more hits.
    v[4] = 20;
    t = target(0, 31);
    gen::generate(gen::kGrids, v, t);
    const int lo = hitsOn(2, 0, 32);
    v[4] = 90;
    t = target(0, 31);
    gen::generate(gen::kGrids, v, t);
    CHECK(hitsOn(2, 0, 32) > lo);
    // One part on the cursor track only.
    v[9] = 2;   // snare
    v[3] = 100;
    t = target(0, 15, 4, 4);
    gen::generate(gen::kGrids, v, t);
    CHECK(hitsOn(4, 0, 16) > 0);
    CHECK_EQ(hitsOn(5, 0, 16), 0);
}

TEST(grids_xy_moves_the_pattern) {
    int16_t v[gen::kParams];
    defaults(gen::kGrids, v);
    v[gen::kSeedParam] = 3;
    std::set<uint32_t> seen;
    for (int x = 0; x <= 100; x += 25) {
        v[0] = int16_t(x);
        v[1] = int16_t(100 - x);
        gen::Target t = target(0, 15);
        gen::generate(gen::kGrids, v, t);
        uint32_t h = 0;
        for (int tr = 0; tr < 3; ++tr)
            for (int s = 0; s < 16; ++s) h = h * 3 + (g_pat.tracks[tr].steps[s].note >= 0);
        seen.insert(h);
    }
    CHECK(seen.size() >= 3);
}

TEST(marbles_deja_vu_locks_the_loop) {
    int16_t v[gen::kParams];
    defaults(gen::kMarbles, v);
    v[0] = 70;
    v[1] = 100;   // locked
    v[2] = 4;
    v[gen::kSeedParam] = 11;
    gen::Target t = target(0, 31);
    gen::Result r = gen::generate(gen::kMarbles, v, t);
    CHECK(r.ok);
    // Period 4: step s and s + 4 the same.
    for (int s = 0; s + 4 < 32; ++s) CHECK_EQ(g_pat.tracks[0].steps[s].note, g_pat.tracks[0].steps[s + 4].note);
    // Deja vu 0: not periodic, and every note in the scale (Steps = Scale).
    v[1] = 0;
    t = target(0, 63);
    gen::generate(gen::kMarbles, v, t);
    int diff = 0;
    for (int s = 0; s + 4 < 64; ++s) diff += g_pat.tracks[0].steps[s].note != g_pat.tracks[0].steps[s + 4].note;
    CHECK(diff > 8);
    for (int s = 0; s < 64; ++s) {
        const Step& st = g_pat.tracks[0].steps[s];
        CHECK(stepValid(st));
        if (st.note < 0) continue;
        const int pc = ((st.note - 48) % 12 + 12) % 12;
        CHECK((0x0AD5 >> pc) & 1);
    }
    // Spread 0: one pitch only (the bias).
    v[3] = 0;
    t = target(0, 31);
    gen::generate(gen::kMarbles, v, t);
    std::set<int> notes;
    for (int s = 0; s < 32; ++s)
        if (g_pat.tracks[0].steps[s].note >= 0) notes.insert(g_pat.tracks[0].steps[s].note);
    CHECK_EQ(int(notes.size()), 1);
    // Two tracks: every step on one of them.
    v[3] = 50;
    v[8] = 3;
    t = target(0, 15, 0, 1);
    gen::generate(gen::kMarbles, v, t);
    for (int s = 0; s < 16; ++s)
        CHECK((g_pat.tracks[0].steps[s].note >= 0) != (g_pat.tracks[1].steps[s].note >= 0));
}

TEST(genotype_drummer_keeps_columns) {
    int16_t v[gen::kParams];
    defaults(gen::kGenotype, v);
    v[0] = 3;   // Drummer
    v[1] = 20;
    v[gen::kSeedParam] = 5;
    gen::Target t = target(0, 31, 0, 2);
    // A kit: kick on 0 / 8, snare 4 / 12, hats every 2.
    for (int s = 0; s < 32; ++s) {
        if (s % 8 == 0) g_pat.tracks[0].steps[s].note = 36;
        if (s % 8 == 4) g_pat.tracks[1].steps[s].note = 38;
        if (s % 2 == 0) g_pat.tracks[2].steps[s].note = 42;
    }
    Pattern before = g_pat;
    gen::Result r = gen::generate(gen::kGenotype, v, t);
    CHECK(r.ok);
    // Every output column is one of the input columns (the kit stays together).
    for (int s = 0; s < 32; ++s) {
        bool found = false;
        for (int j = 0; j < 32 && !found; ++j) {
            bool same = true;
            for (int k = 0; k < 3; ++k) same = same && g_pat.tracks[k].steps[s] == before.tracks[k].steps[j];
            found = same;
        }
        CHECK(found);
    }
}

TEST(play_modes_m_tm_r_tables) {
    Harness h;
    App& a = *h.app;
    Pattern& p = a.pattern();
    for (int s = 0; s < 16; ++s) {
        p.tracks[0].steps[s].note = int8_t(48 + (s % 4) * 2);
        p.tracks[0].steps[s].instrument = 0;
    }
    p.tracks[0].lastStep = 15;
    for (int t = 1; t < 3; ++t) p.tracks[t] = p.tracks[0];
    // Labels.
    CHECK(std::string(seq::playModeLabel(40)) == "M1");
    CHECK(std::string(seq::playModeLabel(43)) == "TM8");
    CHECK(std::string(seq::playModeLabel(46)) == "R2");
    CHECK_EQ(perf::fx(perf::kPlayMode).max, 46);
    a.seq.setPerformanceMode(true);
    a.seq.setPerfPlayMode(0, 42);   // M3
    a.seq.setPerfPlayMode(1, 44);   // TM16
    a.seq.setPerfPlayMode(2, 46);   // R2
    for (int t = 0; t < 3; ++t) live::setLoopLock(t, -50);   // move every loop (TM: random)
    a.togglePlay(false);
    std::set<uint32_t> m3, tm, r2;
    for (int loop = 0; loop < 6; ++loop) {
        // One pattern loop: until the position wraps, then a few passes.
        int last = a.seq.patternPosition();
        for (int k = 0; k < 20000; ++k) {
            h.audio.pump(1);
            h.run(2);
            const int pos = a.seq.patternPosition();
            if (pos < last) break;
            last = pos;
        }
        h.run(20);
        uint32_t hm = 0, ht = 0, hr = 0;
        for (int k = 0; k < 16; ++k) {
            hm = hm * 31 + seq::gGenoTables[0][k];
            ht = ht * 31 + seq::gGenoTables[1][k];
            hr = hr * 31 + uint32_t(seq::gModeXpose[2][k] + 64);
            CHECK(seq::gGenoTables[0][k] < 16 && seq::gGenoTables[1][k] < 16);
            CHECK_EQ(seq::gGenoTables[2][k], k);   // R: forward
        }
        m3.insert(hm);
        tm.insert(ht);
        r2.insert(hr);
    }
    CHECK(seq::gGenoReady[0] && seq::gGenoReady[1] && seq::gGenoReady[2]);
    CHECK(m3.size() >= 3);
    CHECK(tm.size() >= 3);
    CHECK(r2.size() >= 3);
    // R2 offsets stay in the scale (C major, root C): the transposed notes too.
    for (int k = 0; k < 16; ++k) {
        const int note = p.tracks[2].steps[k].note + seq::gModeXpose[2][k];
        CHECK((0x0AD5 >> (note % 12)) & 1 || a.scale() == 0);
    }
    a.togglePlay(false);
    // The Turing Machine knob: +100 locks the 16-step loop, -100 gives a
    // locked loop of twice the length (two tables taking turns).
    for (int lockV : {100, -100}) {
        live::setLoopLock(1, lockV);
        a.togglePlay(false);
        std::vector<uint32_t> seen;
        for (int loop = 0; loop < 7; ++loop) {
            int last = a.seq.patternPosition();
            for (int k = 0; k < 20000; ++k) {
                h.audio.pump(1);
                h.run(2);
                const int pos = a.seq.patternPosition();
                if (pos < last) break;
                last = pos;
            }
            h.run(20);
            uint32_t ht = 0;
            for (int k = 0; k < 16; ++k) ht = ht * 31 + seq::gGenoTables[1][k];
            seen.push_back(ht);
        }
        a.togglePlay(false);
        // Skip the first loops (the register fills), then check the period.
        if (lockV > 0) {
            CHECK(seen[3] == seen[4] && seen[4] == seen[5] && seen[5] == seen[6]);
        } else {
            CHECK(seen[3] == seen[5] && seen[4] == seen[6]);
            CHECK(seen[3] != seen[4]);
        }
    }
    for (int t = 0; t < 3; ++t) live::setLoopLock(t, 0);
}

TEST(grids_live_perform_offsets) {
    Harness h;
    App& a = *h.app;
    int16_t v[gen::kParams];
    defaults(gen::kGrids, v);
    v[gen::kSeedParam] = 9;
    gen::Target t;
    t.pat = &a.pattern();
    t.t0 = t.t1 = 0;
    t.s0 = 0;
    t.s1 = 15;
    t.instrument = 0;
    t.rootNote = 48;
    const gen::Result r = gen::generate(gen::kGrids, v, t);
    live::grids(a, v, t, r.rngSeed);
    CHECK(live::gridsActive(0) && live::gridsActive(2));
    auto count = [&]() {
        int n = 0;
        for (int tr = 0; tr < 3; ++tr)
            for (int s = 0; s < 16; ++s) n += a.pattern().tracks[tr].steps[s].note >= 0;
        return n;
    };
    const int base = count();
    live::setGridsPerf(0, 0, 60);   // denser
    h.run(30);
    const int dense = count();
    CHECK(dense > base);
    live::setGridsPerf(0, 0, -100);   // nothing
    h.run(30);
    CHECK_EQ(count(), 0);
    live::setGridsPerf(0, 0, 0);
    h.run(30);
    CHECK_EQ(count(), base);   // back to the generated state
    // A hand edit ends it.
    a.pattern().tracks[1].steps[3].note = 70;
    h.run(30);
    CHECK(!live::gridsActive(0));
}
