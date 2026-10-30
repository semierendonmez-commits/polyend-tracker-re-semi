// OT fork Turing Machine: the Generate kind (gen_turing.cpp), its live slot
// (Perform "Turing Machine" / "Turing Length" run the register on every
// loop), the knob on ordinary tracks (Turing step order over Play Mode) and
// TM8 / TM16 no longer offered under Pattern Play Mode.
#include <cstring>
#include <memory>

#include "test.h"
#include "ui_fakes.h"
#include "nvstore_fake.h"

#include "app/live_gen.h"
#include "app/perf_presets.h"
#include "gen/gen.h"
#include "sequencer/seq_playmodes.h"
#include "ui/screens.h"

using namespace ot;

namespace {
void defaults(int16_t* v) {
    for (int i = 0; i < gen::kParams; ++i) {
        const gen::ParamDesc& d = gen::param(gen::kTuring, i, nullptr);
        v[i] = d.name ? d.def : 0;
    }
}
gen::Target target(Pattern& p, int track, int n) {
    gen::Target t;
    t.pat = &p;
    t.t0 = t.t1 = track;
    t.s0 = 0;
    t.s1 = n - 1;
    t.instrument = 3;
    t.rootNote = 36;
    t.scaleMask = 0x0AD5;
    t.entropy = 77;
    return t;
}
int noteAt(const Pattern& p, int t, int s) { return p.tracks[t].steps[s].note; }
}  // namespace

TEST(turing_generator_loops_like_the_module) {
    CHECK_STR(gen::kind(gen::kTuring).name, "Turing");
    std::unique_ptr<Pattern> p(new Pattern);
    p->setDefault();
    int16_t v[gen::kParams];
    defaults(v);
    v[2] = 100;   // every step plays
    v[5] = 7;     // seed
    // Lock +100, Length 4: a loop of four steps.
    v[0] = 4;
    v[1] = 100;
    gen::Target t = target(*p, 0, 32);
    gen::Result r = gen::generate(gen::kTuring, v, t);
    CHECK(r.ok);
    for (int s = 0; s + 4 < 32; ++s) CHECK_EQ(noteAt(*p, 0, s), noteAt(*p, 0, s + 4));
    CHECK_EQ(int(p->tracks[0].steps[0].instrument), 3);
    // Lock -100: twice the length (8), the second half inverted (other notes).
    v[1] = -100;
    t = target(*p, 0, 32);
    gen::generate(gen::kTuring, v, t);
    int differ = 0;
    for (int s = 0; s + 8 < 32; ++s) CHECK_EQ(noteAt(*p, 0, s), noteAt(*p, 0, s + 8));
    for (int s = 0; s < 4; ++s) differ += noteAt(*p, 0, s) != noteAt(*p, 0, s + 4);
    CHECK(differ > 0);
    // Lock 0: random, no 4-step loop.
    v[1] = 0;
    v[0] = 4;
    t = target(*p, 0, 64);
    gen::generate(gen::kTuring, v, t);
    differ = 0;
    for (int s = 0; s + 4 < 64; ++s) differ += noteAt(*p, 0, s) != noteAt(*p, 0, s + 4);
    CHECK(differ > 8);
    // Density 0: nothing; the notes stay in the scale.
    v[1] = 100;
    v[2] = 0;
    t = target(*p, 0, 16);
    r = gen::generate(gen::kTuring, v, t);
    for (int s = 0; s < 16; ++s) CHECK_EQ(noteAt(*p, 0, s), int(kNoteEmpty));
    v[2] = 60;
    t = target(*p, 0, 16);
    gen::generate(gen::kTuring, v, t);
    int notes = 0;
    for (int s = 0; s < 16; ++s) {
        const int n = noteAt(*p, 0, s);
        if (n == kNoteEmpty) continue;
        ++notes;
        CHECK(0x0AD5 & (1u << ((n - 36) % 12)));
    }
    CHECK(notes > 0 && notes < 16);
}

TEST(turing_live_runs_with_the_perform_knob) {
    Harness h;
    App& a = *h.app;
    int16_t v[gen::kParams];
    defaults(v);
    v[0] = 8;
    v[1] = 100;
    v[2] = 100;
    v[5] = 3;
    for (auto& tr : a.pattern().tracks) tr.lastStep = 15;
    gen::Target t = target(a.pattern(), 1, 16);
    const gen::Result r = gen::generate(gen::kTuring, v, t);
    live::turing(a, v, t, r.rngSeed);
    CHECK(live::turingActive(1));
    CHECK(!live::turingActive(2));
    int before[16];
    for (int s = 0; s < 16; ++s) before[s] = noteAt(a.pattern(), 1, s);
    // The live slot reproduced the written line (same register).
    auto changed = [&] {
        int d = 0;
        for (int s = 0; s < 16; ++s) d += noteAt(a.pattern(), 1, s) != before[s];
        return d;
    };
    auto playLoops = [&](int loops) {
        a.togglePlay(false);
        for (int i = 0; i < loops * 70; ++i) {
            h.audio.pump(4);
            h.run(2);
        }
        a.togglePlay(false);
        h.run(5);
    };
    // No Perform FX: the line stays as written.
    playLoops(3);
    CHECK_EQ(changed(), 0);
    CHECK(live::turingActive(1));
    // Knob locked, Length 8 over 16 steps: still the same notes.
    live::setTuringPerf(1, 100, 0);
    playLoops(3);
    CHECK_EQ(changed(), 0);
    // Knob near random: the register runs on, the notes change.
    live::setTuringPerf(1, 1, 0);
    playLoops(3);
    std::printf("  notes changed with the knob at +1: %d of 16\n", changed());
    CHECK(changed() > 0);
    CHECK(live::turingActive(1));   // still live (its own rewrite is not a hand edit)
    // A hand edit ends the slot.
    a.pattern().tracks[1].steps[0].note = 70;
    h.run(5);
    CHECK(!live::turingActive(1));
    live::setTuringPerf(1, 0, 0);
}

TEST(turing_play_mode_values_hidden) {
    // TM8 / TM16 are skipped by the encoder and by CC; older sets still label them.
    CHECK(perf::valueHidden(perf::kPlayMode, seq::kPlayFirstTuring));
    CHECK(perf::valueHidden(perf::kPlayMode, seq::kPlayFirstTuring + 1));
    CHECK(!perf::valueHidden(perf::kPlayMode, seq::kPlayFirstRandLoop));
    CHECK(!perf::valueHidden(perf::kLoopLock, seq::kPlayFirstTuring));
    CHECK_EQ(perf::stepValue(perf::kPlayMode, seq::kPlayFirstTuring - 1, 1), int(seq::kPlayFirstRandLoop));
    CHECK_EQ(perf::stepValue(perf::kPlayMode, seq::kPlayFirstRandLoop, -1), seq::kPlayFirstTuring - 1);
    for (int cc = 0; cc < 128; ++cc) CHECK(!perf::valueHidden(perf::kPlayMode, perf::ccToValue(perf::kPlayMode, uint8_t(cc))));
    CHECK_STR(perf::fx(perf::kTuring).name, "Turing Machine");
    CHECK_STR(perf::fx(perf::kTuringLength).name, "Turing Length");
    char t[16];
    perf::valueText(perf::kTuringLength, 0, t, sizeof t);
    CHECK_STR(t, "Gen");
    perf::valueText(perf::kTuringLength, 12, t, sizeof t);
    CHECK_STR(t, "12");
}

TEST(turing_knob_orders_ordinary_tracks) {
    FakeNvStore nv;
    std::unique_ptr<PerfPatternMemory> mem(new PerfPatternMemory);
    Harness h(false);
    h.app->setNvStore(&nv);
    h.app->seq.setPerfPatternMemory(mem.get());
    h.app->boot();
    App& a = *h.app;
    // Track 2 is a Turing generator line, the others are not.
    int16_t v[gen::kParams];
    defaults(v);
    gen::Target t = target(a.pattern(), 1, 16);
    const gen::Result r = gen::generate(gen::kTuring, v, t);
    live::turing(a, v, t, r.rngSeed);
    h.tap(btn::Performance);
    PerformanceScreen& p = *static_cast<PerformanceScreen*>(a.ui.screen(kModPerformance));
    p.src.setType(0, perf::kTuring);
    p.src.setValue(0, 1, 60);
    p.src.setType(1, perf::kPlayMode);
    p.src.setValue(1, 1, 3);   // C1 on the same tracks
    h.pad(12);   // column 1, row 1
    h.pad(13);   // column 2, row 1
    CHECK_EQ(int(p.cur[0]), 60);
    CHECK_EQ(int(a.seq.perfPlayMode(0)), int(seq::kPlayFirstTuring));   // the knob wins over Play Mode
    CHECK_EQ(int(a.seq.perfPlayMode(5)), int(seq::kPlayFirstTuring));
    CHECK_EQ(int(a.seq.perfPlayMode(1)), 3);   // the generator line keeps its notes' order
    // The knob off: Play Mode again.
    h.pad(0);   // column 1, row 0 = off
    CHECK_EQ(int(a.seq.perfPlayMode(0)), 3);
}
