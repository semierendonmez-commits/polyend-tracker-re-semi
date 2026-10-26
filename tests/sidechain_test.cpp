// OT fork sidechain (src/core/audio/sidechain.h, Master 5/5): the two
// envelopes, per-source targets in the engine, the per-project file and the
// Master page.
#include "test.h"

#include <cmath>
#include <memory>
#include <vector>

#include "app/fork_project.h"
#include "audio/engine.h"
#include "audio/sidechain.h"
#include "mem_storage.h"
#include "ui/screens.h"
#include "ui_fakes.h"

using namespace ot;
using namespace ot::audio;

namespace {
float minOf(const float* g) {
    float m = 1.0f;
    for (uint32_t i = 0; i < kBlock; ++i) m = std::fmin(m, g[i]);
    return m;
}
}  // namespace

TEST(sidechain_trigger_envelope) {
    SidechainSettings s;
    s.source = 1;
    s.depth = 75;
    s.attack = 1;
    s.hold = 4;       // 20 ms
    s.release = 10;   // 100 ms
    SidechainEnv e;
    float g[kBlock];
    CHECK(!e.process(s, false, nullptr, g));
    CHECK_NEAR(minOf(g), 1.0f, 1e-6);
    CHECK(e.process(s, true, nullptr, g));
    CHECK_NEAR(minOf(g), 0.25f, 1e-3);   // full depth after the 1-ms attack
    // Held, then released: back to (nearly) 1 well after hold + release.
    int blocks = 0;
    float last = 0.0f;
    for (; blocks < 200; ++blocks) {
        e.process(s, false, nullptr, g);
        last = g[kBlock - 1];
        if (last > 0.999f) break;
    }
    const float ms = float(blocks + 1) * kBlock / 44.1f;
    std::printf("  trigger: back to 1 after %.0f ms\n", double(ms));
    CHECK(ms > 60.0f && ms < 200.0f);
    CHECK(!e.process(s, false, nullptr, g) || minOf(g) > 0.999f);
}

TEST(sidechain_follower_ducks_above_threshold) {
    SidechainSettings s;
    s.source = SidechainSettings::kLineL;
    s.mode = 1;
    s.depth = 100;
    s.thresh = 50;   // -30 dBFS
    s.hold = 0;
    s.release = 5;
    SidechainEnv e;
    float src[kBlock], g[kBlock];
    for (float& v : src) v = 0.01f;   // -40 dBFS: below
    for (int b = 0; b < 20; ++b) e.process(s, false, src, g);
    CHECK(minOf(g) > 0.99f);
    for (uint32_t i = 0; i < kBlock; ++i) src[i] = (i & 1) ? 0.5f : -0.5f;   // -6 dBFS
    for (int b = 0; b < 20; ++b) e.process(s, false, src, g);
    CHECK(minOf(g) < 0.05f);
    for (float& v : src) v = 0.0f;
    for (int b = 0; b < 100; ++b) e.process(s, false, src, g);
    CHECK(minOf(g) > 0.99f);
}

namespace {
struct Rig {
    std::vector<int16_t> fx = std::vector<int16_t>(AudioEngine::kFxMemorySamples);
    std::vector<int16_t> pool = std::vector<int16_t>(4u << 20);
    std::unique_ptr<Instrument[]> ins = std::unique_ptr<Instrument[]>(new Instrument[kSampleInstrumentCount]);
    std::unique_ptr<AudioEngine> eng = std::unique_ptr<AudioEngine>(new AudioEngine());
    Rig() { CHECK(eng->init(fx.data(), fx.size(), pool.data(), pool.size(), ins.get())); }
    void load(int i, const std::vector<int16_t>& d, PlayMode m) {
        ins[i].lengthFrames = uint32_t(d.size());
        ins[i].playMode = uint8_t(m);
        CHECK(eng->bank().store(i, d.data(), uint32_t(d.size()), false));
    }
    void note(uint8_t track, uint8_t instr) {
        VoiceNoteOn n;
        n.instrument = instr;
        n.note = 60;
        eng->voiceNoteOn(track, n);
    }
    // RMS of the left channel over `blocks` blocks.
    double rms(int blocks) {
        int16_t out[kBlock * 2];
        double s = 0;
        for (int b = 0; b < blocks; ++b) {
            eng->renderLive(out);
            for (uint32_t i = 0; i < kBlock; ++i) s += double(out[2 * i]) * out[2 * i];
        }
        return std::sqrt(s / (blocks * kBlock));
    }
};
std::vector<int16_t> sine(uint32_t len, float hz) {
    std::vector<int16_t> d(len);
    for (uint32_t i = 0; i < len; ++i) d[i] = int16_t(0.4f * 32767.0f * std::sin(6.2831853f * hz * float(i) / 44100.0f));
    return d;
}
}  // namespace

TEST(sidechain_ducks_only_its_targets) {
    // Track 1 plays a looping sine; a (silent) note on track 2 is the
    // trigger source. Only a source whose targets include track 1 ducks it.
    for (int run = 0; run < 3; ++run) {
        Rig rig;
        rig.load(0, sine(44100, 441.0f), PlayMode::ForwardLoop);
        rig.load(1, std::vector<int16_t>(4410, 0), PlayMode::OneShot);
        MixerSettings& m = rig.eng->settings();
        SidechainSettings& s = m.sc[run == 2 ? 1 : 0];
        s.source = 2;
        s.depth = 90;
        s.attack = 0;
        s.hold = 20;   // 100 ms
        s.targets = run == 0 ? uint16_t(kScTrack0 << 2) : uint16_t(kScTrack0);   // run 0: track 3 only
        rig.note(0, 0);
        const double before = rig.rms(40);
        rig.note(1, 1);
        const double during = rig.rms(10);
        std::printf("  run %d: before %.0f during %.0f env %.2f\n", run, before, during,
                    double(rig.eng->sidechainLevel(run == 2 ? 1 : 0)));
        CHECK(before > 1000.0);
        if (run == 0) CHECK(during > 0.9 * before);
        else CHECK(during < 0.25 * before);
    }
}

TEST(fork_project_file_roundtrip) {
    MemStorage fs;
    ForkProject f;
    f.setDefault();
    CHECK(f.isDefault());
    CHECK(f.save(fs, "Workspace"));
    CHECK(!fs.exists("Workspace/otfork.bin"));   // defaults: no file
    f.sc[0].source = 5;
    f.sc[0].targets = kScTrack0 | kScReverb;
    f.sc[1].source = SidechainSettings::kLineR;
    f.sc[1].mode = 1;
    f.sc[1].thresh = 33;
    f.sc[1].release = 70;
    CHECK(f.save(fs, "Workspace"));
    CHECK(fs.exists("Workspace/otfork.bin"));
    ForkProject g;
    CHECK(g.load(fs, "Workspace"));
    CHECK_EQ(g.sc[0].source, 5);
    CHECK_EQ(g.sc[0].targets, uint16_t(kScTrack0 | kScReverb));
    CHECK_EQ(g.sc[1].source, SidechainSettings::kLineR);
    CHECK_EQ(g.sc[1].mode, 1);
    CHECK_EQ(g.sc[1].thresh, 33);
    CHECK_EQ(g.sc[1].release, 70);
    // Garbage values are clamped.
    fs.files["Workspace/otfork.bin"][8] = 99;   // sc[0].source
    CHECK(g.load(fs, "Workspace"));
    CHECK_EQ(g.sc[0].source, 0);
    ForkProject h;
    CHECK(!h.load(fs, "Nowhere"));
    CHECK(h.isDefault());
}

TEST(sidechain_master_page) {
    Harness h;
    App& a = *h.app;
    MixerScreen& m = *static_cast<MixerScreen*>(a.ui.screen(kModMaster));
    for (int i = 0; i < 5; ++i) h.tap(btn::Master);
    CHECK_EQ(m.page, 4);
    // Pads toggle targets of SC 1; Shift + pad picks the source.
    h.pad(0);
    h.pad(12);   // delay
    CHECK_EQ(a.fork.sc[0].targets, uint16_t(kScTrack0 | kScDelay));
    h.down(btn::Shift);
    h.pad(3);
    h.up(btn::Shift);
    CHECK_EQ(a.fork.sc[0].source, 4);
    // F1: SC 2, its own targets.
    h.tap(btn::F1);
    CHECK_EQ(m.scSel, 1);
    h.pad(9);   // synth 2
    CHECK_EQ(a.fork.sc[1].targets, uint16_t(kScSynth0 << 1));
    CHECK_EQ(a.fork.sc[0].targets, uint16_t(kScTrack0 | kScDelay));
    // F7 Depth, encoder.
    h.tap(btn::F7);
    const int d0 = a.fork.sc[1].depth;
    h.enc(-10);
    CHECK_EQ(a.fork.sc[1].depth, d0 - 10);
    // Applied to the engine and saved with the autosave.
    h.audio.pump(2);
    h.run(20);
    CHECK_EQ(a.engine.settings().sc[0].source, 4);
    a.autosave();
    CHECK(h.fs.exists("Workspace/otfork.bin"));
    // The cycle wraps to Master 1/5.
    h.tap(btn::Master);
    CHECK_EQ(m.page, 0);
}
