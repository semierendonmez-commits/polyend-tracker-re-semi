// OT fork live generators (src/core/app/live_gen.h): BBCut Mutate re-cuts
// while the pattern plays, and every re-cut differs.
#include "test.h"

#include <set>

#include "app/live_gen.h"
#include "gen/gen.h"
#include "ui_fakes.h"

using namespace ot;

namespace {
uint32_t trackHash(const Pattern& p, int t, int n) {
    uint32_t h = 2166136261u;
    for (int s = 0; s < n; ++s) {
        const Step& st = p.tracks[t].steps[s];
        const uint8_t b[5] = {uint8_t(st.note), st.instrument, st.fx[0].type, st.fx[0].value, st.fx[1].value};
        for (uint8_t x : b) h = (h ^ x) * 16777619u;
    }
    return h;
}
}  // namespace

TEST(bbcut_mutate_recuts_every_loop) {
    Harness h;
    App& a = *h.app;
    Pattern& pat = a.pattern();
    const int len = pat.tracks[0].length();
    int16_t v[gen::kParams];
    for (int i = 0; i < gen::kParams; ++i) v[i] = gen::param(gen::kBBCut, i, v).name ? gen::kind(gen::kBBCut).p[i].def : 0;
    v[4] = 1;   // Mutate every loop
    uint16_t starts[16];
    for (int i = 0; i < 16; ++i) starts[i] = uint16_t(i * 4000);
    gen::Target t;
    t.pat = &pat;
    t.t0 = t.t1 = 0;
    t.s0 = 0;
    t.s1 = len - 1;
    t.slices.count = 16;
    t.slices.start = starts;
    t.slices.beatSlice = true;
    t.slices.sampleBeats = 4.0f;
    t.entropy = 777;
    const gen::Result r = gen::generate(gen::kBBCut, v, t);
    CHECK(r.ok);
    live::bbcut(a, v, t, r.rngSeed);
    std::set<uint32_t> seen;
    seen.insert(trackHash(pat, 0, len));
    a.togglePlay(false);
    CHECK(a.seq.isPlaying());
    int changes = 0;
    uint32_t last = trackHash(pat, 0, len);
    for (int loop = 0; loop < 6; ++loop) {
        // One pattern loop (~len steps) in small slices of audio + main loop.
        for (int k = 0; k < 400; ++k) {
            h.audio.pump(2);
            h.run(10);
        }
        const uint32_t now = trackHash(pat, 0, len);
        if (now != last) ++changes;
        last = now;
        seen.insert(now);
    }
    std::printf("  bbcut mutate: %d changes, %d distinct cuts\n", changes, int(seen.size()));
    CHECK(changes >= 2);
    CHECK(seen.size() >= 3);
}

TEST(bbcut_mutate_from_the_popup) {
    Harness h;
    App& a = *h.app;
    Instrument& in = a.instruments[0];
    a.instrPresent[0] = true;
    in.playMode = uint8_t(PlayMode::BeatSlice);
    in.sliceCount = 16;
    for (int i = 0; i < 16; ++i) in.slices[i] = uint16_t(i * 4000);
    in.lengthFrames = 88200;
    a.setCurrentInstrument(0);
    h.tap(btn::Pattern);
    h.tap(btn::Rec);
    h.tap(btn::F8);
    h.tap(btn::F8);
    h.tap(btn::F3);   // Generate
    h.tap(btn::F1);
    h.pad(2);         // BBCut
    h.tap(btn::F6);
    h.pad(1);         // Mutate: 1 loop
    h.tap(btn::F8);   // generate
    Pattern& pat = a.pattern();
    const int len = pat.tracks[0].length();
    const uint32_t first = trackHash(pat, 0, len);
    CHECK(first != trackHash(Pattern{}, 0, 0));
    h.tap(btn::Rec);   // edit off
    a.togglePlay(false);
    std::set<uint32_t> seen;
    seen.insert(first);
    for (int k = 0; k < 6; ++k) {
        for (int j = 0; j < 800; ++j) {
            h.audio.pump(2);
            h.run(10);
        }
        seen.insert(trackHash(pat, 0, len));
    }
    std::printf("  popup bbcut mutate: %d distinct cuts\n", int(seen.size()));
    CHECK(seen.size() >= 4);
}

TEST(generate_popup_draws_differ_after_boot) {
    // Two fresh boots at different times: the first Random draw differs.
    uint32_t h1 = 0, h2 = 0;
    for (int run = 0; run < 2; ++run) {
        Harness h;
        App& a = *h.app;
        h.clock.ms += 1234 + 977 * run;
        h.tap(btn::Pattern);
        h.tap(btn::Rec);
        h.tap(btn::F8);
        h.tap(btn::F8);
        h.tap(btn::F3);   // Generate (Acid)
        h.tap(btn::F8);
        (run ? h2 : h1) = trackHash(a.pattern(), 0, a.pattern().tracks[0].length());
    }
    CHECK(h1 != h2);
}
