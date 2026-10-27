// OT fork Master Reverb page 2 (Shift): low cut and width for every model,
// two character values per model (fx_models.h / fx_models2.h), the project
// file record and the page.
#include "test.h"

#include <cmath>
#include <memory>
#include <vector>

#include "app/fork_project.h"
#include "audio/engine.h"
#include "audio/fx_models.h"
#include "audio/fx_models2.h"
#include "mem_storage.h"
#include "ui/screens.h"
#include "ui_fakes.h"

using namespace ot;
using namespace ot::audio;

namespace {
// An impulse, then `blocks` blocks of silence: the left output.
std::vector<float> fdnImpulse(int model, float a, float b, bool setChar, int blocks = 200) {
    std::vector<int16_t> mem(FdnReverb::kMemorySamples);
    std::unique_ptr<FdnReverb> r(new FdnReverb());
    r->init(mem.data());
    r->setModel(model);
    r->setSize(0.6f);
    if (setChar) r->setChar(a, b);
    std::vector<float> out;
    float in[kBlock] = {}, L[kBlock], R[kBlock];
    for (int k = 0; k < blocks; ++k) {
        for (uint32_t i = 0; i < kBlock; ++i) in[i] = (k < 4) ? ((i & 7) == 0 ? 0.8f : 0.0f) : 0.0f;
        r->process(in, L, R);
        out.insert(out.end(), L, L + kBlock);
    }
    return out;
}
double energy(const std::vector<float>& x, size_t from, size_t to) {
    double e = 0;
    for (size_t i = from; i < to && i < x.size(); ++i) e += double(x[i]) * x[i];
    return e;
}
bool same(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) return false;
    return true;
}
}  // namespace

TEST(reverb_char_defaults_keep_the_sound) {
    // 0.5 / 0.5 is the model as designed; other values change it.
    for (int m = 1; m <= 4; ++m) {
        const auto base = fdnImpulse(m, 0.5f, 0.5f, false);
        CHECK(same(base, fdnImpulse(m, 0.5f, 0.5f, true)));
        const bool aMoves = !same(base, fdnImpulse(m, 1.0f, 0.5f, true));
        const bool bMoves = !same(base, fdnImpulse(m, 0.5f, 1.0f, true));
        std::printf("  fdn model %d: A %d B %d\n", m, int(aMoves), int(bMoves));
        CHECK(aMoves || m == 3);   // Gated: a longer hold may still end inside the window
        CHECK(bMoves);
    }
    // Gated: a short hold closes the tail sooner.
    const auto shortHold = fdnImpulse(3, 0.0f, 0.5f, true), longHold = fdnImpulse(3, 1.0f, 0.5f, true);
    const double es = energy(shortHold, 44100 / 4, 44100), el = energy(longHold, 44100 / 4, 44100);
    std::printf("  gated tail: hold 0 %.4f, hold 100 %.4f\n", es, el);
    CHECK(el > 2.0 * es + 1e-6);
}

TEST(reverb_char_plate_texture) {
    std::vector<int16_t> mem(PlateReverb::kMemorySamples), sramBuf(4 * 512);
    int16_t* const sram[4] = {sramBuf.data(), sramBuf.data() + 512, sramBuf.data() + 1024, sramBuf.data() + 1536};
    auto plate = [&](float a, float b, bool set) {
        std::unique_ptr<PlateReverb> p(new PlateReverb());
        std::fill(mem.begin(), mem.end(), 0);
        std::fill(sramBuf.begin(), sramBuf.end(), 0);
        p->init(mem.data(), sram);
        p->setSize(0.6f);
        if (set) p->setChar(a, b);
        std::vector<float> out;
        float in[kBlock] = {}, L[kBlock], R[kBlock];
        for (int k = 0; k < 120; ++k) {
            for (uint32_t i = 0; i < kBlock; ++i) in[i] = k < 4 ? ((i & 7) == 0 ? 0.8f : 0.0f) : 0.0f;
            p->process(in, L, R);
            out.insert(out.end(), L, L + kBlock);
        }
        return out;
    };
    const auto base = plate(0.5f, 0.5f, false);
    CHECK(same(base, plate(0.5f, 0.5f, true)));
    CHECK(!same(base, plate(1.0f, 0.5f, true)));
    // Low bandwidth: a darker plate (less energy).
    const double eBright = energy(plate(0.5f, 1.0f, true), 0, 120 * kBlock);
    const double eDark = energy(plate(0.5f, 0.0f, true), 0, 120 * kBlock);
    std::printf("  plate bandwidth: dark %.3f bright %.3f\n", eDark, eBright);
    CHECK(eDark < eBright);
    // Texture: the grain length and octave share change the cloud, stay finite.
    std::vector<int16_t> tm(TextureReverb::kMemorySamples);
    for (float a : {0.0f, 1.0f}) {
        std::unique_ptr<TextureReverb> t(new TextureReverb());
        std::fill(tm.begin(), tm.end(), 0);
        t->init(tm.data());
        t->setChar(a, a);
        float in[kBlock], L[kBlock], R[kBlock];
        double e = 0;
        bool finite = true;
        for (int k = 0; k < 200; ++k) {
            for (uint32_t i = 0; i < kBlock; ++i) in[i] = k < 60 ? 0.3f * std::sin(0.05f * float(k * kBlock + i)) : 0.0f;
            t->process(in, L, R);
            for (uint32_t i = 0; i < kBlock; ++i) {
                finite = finite && std::isfinite(L[i]) && std::isfinite(R[i]);
                e += double(L[i]) * L[i];
            }
        }
        std::printf("  texture char %.0f: energy %.2f\n", double(a), e);
        CHECK(finite && e > 0.1);
    }
}

namespace {
struct Rig {
    std::vector<int16_t> fx = std::vector<int16_t>(AudioEngine::kFxMemorySamples);
    std::vector<int16_t> pool = std::vector<int16_t>(4u << 20);
    std::unique_ptr<Instrument[]> ins = std::unique_ptr<Instrument[]>(new Instrument[kSampleInstrumentCount]);
    std::unique_ptr<AudioEngine> eng = std::unique_ptr<AudioEngine>(new AudioEngine());
    Rig() { CHECK(eng->init(fx.data(), fx.size(), pool.data(), pool.size(), ins.get())); }
    // RMS of left and right and of their difference over `blocks` blocks.
    void rms(int blocks, double* l, double* side) {
        int16_t out[kBlock * 2];
        double s = 0, d = 0;
        for (int b = 0; b < blocks; ++b) {
            eng->renderLive(out);
            for (uint32_t i = 0; i < kBlock; ++i) {
                s += double(out[2 * i]) * out[2 * i];
                const double df = double(out[2 * i]) - out[2 * i + 1];
                d += df * df;
            }
        }
        *l = std::sqrt(s / (blocks * kBlock));
        *side = std::sqrt(d / (blocks * kBlock));
    }
};
}  // namespace

TEST(reverb_low_cut_and_width_in_the_engine) {
    // A 60 Hz loop into the reverb only (dry muted), Hall: the low cut at
    // 1 kHz removes most of it; width 0 makes the return mono.
    double ref = 0, refSide = 0;
    for (int run = 0; run < 3; ++run) {
        Rig rig;
        std::vector<int16_t> d(44100);
        for (uint32_t i = 0; i < d.size(); ++i) d[i] = int16_t(0.5f * 32767.0f * std::sin(6.2831853f * 60.0f * float(i) / 44100.0f));
        rig.ins[0].lengthFrames = uint32_t(d.size());
        rig.ins[0].playMode = uint8_t(PlayMode::ForwardLoop);
        rig.ins[0].reverbSend = 100;
        CHECK(rig.eng->bank().store(0, d.data(), uint32_t(d.size()), false));
        MixerSettings& m = rig.eng->settings();
        m.dryMute = true;
        m.reverbModel = 1;
        m.revLowCut = run == 1 ? 100 : 0;
        m.revWidth = run == 2 ? 0 : 100;
        VoiceNoteOn n;
        n.instrument = 0;
        n.note = 60;
        rig.eng->voiceNoteOn(0, n);
        double l, side;
        rig.rms(150, &l, &side);
        std::printf("  run %d: rms %.0f side %.0f\n", run, l, side);
        if (run == 0) {
            ref = l;
            refSide = side;
            CHECK(ref > 300.0 && refSide > 30.0);
        } else if (run == 1) {
            CHECK(l < 0.2 * ref);
        } else {
            CHECK(side < 0.02 * refSide + 1.0);
        }
    }
}

TEST(reverb_extra_file_roundtrip) {
    MemStorage fs;
    ForkProject f;
    f.setDefault();
    CHECK(f.rev.isDefault());
    f.rev.lowCut = 40;
    f.rev.width = 130;
    f.rev.a[5] = 80;
    f.rev.b[2] = 10;
    CHECK(f.save(fs, "Workspace"));
    ForkProject g;
    CHECK(g.load(fs, "Workspace"));
    CHECK_EQ(g.rev.lowCut, 40);
    CHECK_EQ(g.rev.width, 130);
    CHECK_EQ(g.rev.a[5], 80);
    CHECK_EQ(g.rev.b[2], 10);
    CHECK_EQ(g.rev.a[0], 50);
    f.setDefault();
    CHECK(f.save(fs, "Workspace"));
    CHECK(!fs.exists("Workspace/otfork.bin"));
}

TEST(reverb_page2_on_the_master_reverb_page) {
    Harness h;
    App& a = *h.app;
    MixerScreen& m = *static_cast<MixerScreen*>(a.ui.screen(kModMaster));
    h.tap(btn::Master);
    h.tap(btn::F2);   // Master Reverb
    CHECK_EQ(m.sub, 1);
    // Model: Plate (F6, encoder).
    h.tap(btn::F6);
    h.enc(5);
    CHECK_EQ(int(a.cfg.reverbModel), 5);
    // Shift: page 2.
    h.tap(btn::Shift);
    CHECK_EQ(m.revPage, 1);
    h.tap(btn::F1);
    h.enc(30);
    CHECK_EQ(a.fork.rev.lowCut, 30);
    h.tap(btn::F2);
    h.enc(-40);
    CHECK_EQ(a.fork.rev.width, 60);
    h.tap(btn::F3);
    h.enc(20);
    CHECK_EQ(a.fork.rev.a[5], 70);
    h.tap(btn::F4);
    h.enc(-10);
    CHECK_EQ(a.fork.rev.b[5], 40);
    // Applied to the engine.
    h.audio.pump(2);
    h.run(20);
    CHECK_EQ(a.engine.settings().revLowCut, 30);
    CHECK_EQ(a.engine.settings().revCharA, 70);
    // Shift again: page 1; F8 Back leaves it on page 1 next time.
    h.tap(btn::Shift);
    CHECK_EQ(m.revPage, 0);
    h.tap(btn::Shift);
    h.tap(btn::F8);
    CHECK_EQ(m.sub, 0);
    CHECK_EQ(m.revPage, 0);
    a.autosave();
    CHECK(h.fs.exists("Workspace/otfork.bin"));
}
