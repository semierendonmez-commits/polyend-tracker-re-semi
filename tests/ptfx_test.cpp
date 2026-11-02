// OT fork .ptfx Sample Editor plugins (app/ptfx.h): the listing and headers,
// the block feed (prepend / selection / append, short returns, early end,
// generators), the Sample Editor's use, the fork's own plugins (ptfx/)
// against the built-in effects they port, and Polyend's reference effects
// (third_party/polyend-ptfx-sdk/effects). Plugins run here compiled for the
// host, behind the loader's platform hook.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "test.h"
#include "ui_fakes.h"

#include "app/ptfx.h"
#include "app/sample_fx.h"
#include "app/sample_fx_ot.h"
#include "formats/wav.h"
#include "tracker_plugin.h"
#include "ui/screens.h"

using namespace ot;

#define PLUGIN_DECL(n)                                                               \
    extern "C" void n##_init(void*, uint32_t, const PtfxHostApi*);                   \
    extern "C" void n##_set(uint8_t, float);                                         \
    extern "C" int32_t n##_process(const int16_t**, int16_t**, uint32_t, uint8_t);   \
    void n##_wi(void* s, uint32_t b, const void* h) { n##_init(s, b, static_cast<const PtfxHostApi*>(h)); }
PLUGIN_DECL(ot_radio)
PLUGIN_DECL(ot_clipper)
PLUGIN_DECL(ot_transcode)
PLUGIN_DECL(ot_dctcrush)
PLUGIN_DECL(ot_paulstretch)
PLUGIN_DECL(ot_granular)
PLUGIN_DECL(multimode_filter)
PLUGIN_DECL(stereo_delay)
PLUGIN_DECL(add_silence)
PLUGIN_DECL(analog_kick_gen)

namespace {

std::map<std::string, ptfx::Entry>& natives() {
    static std::map<std::string, ptfx::Entry> m;
    if (m.empty()) {
#define REG(n) m[#n] = ptfx::Entry{n##_wi, n##_set, n##_process};
        REG(ot_radio) REG(ot_clipper) REG(ot_transcode) REG(ot_dctcrush) REG(ot_paulstretch) REG(ot_granular)
        REG(multimode_filter) REG(stereo_delay) REG(add_silence) REG(analog_kick_gen)
#undef REG
    }
    return m;
}

// The test "code" starts with the plugin's name: the hook hands out the
// host build of it.
bool nativePrepare(uint8_t* code, uint32_t, uint32_t, uint32_t, uint32_t, ptfx::Entry* e) {
    auto it = natives().find(reinterpret_cast<const char*>(code));
    if (it == natives().end()) return false;
    *e = it->second;
    return true;
}

struct PDef {
    const char* label;
    float min, max, def, step;
    const char* unit;
    char type;
    std::vector<std::string> items;
};

void put32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[at + size_t(i)] = uint8_t(v >> (8 * i));
}
void putf(std::vector<uint8_t>& b, size_t at, float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    put32(b, at, u);
}

// A .ptfx as ptfx_builder.py lays it out; the code is the plugin's name and
// the list names.
std::vector<uint8_t> makePtfx(const char* display, const char* tag, const std::vector<PDef>& ps, uint32_t scratch = 0,
                              uint16_t flags = 2, uint32_t magic = 0x58465450u) {
    std::vector<uint8_t> code(tag, tag + std::strlen(tag) + 1);
    while (code.size() % 4) code.push_back(0);
    std::vector<uint32_t> listAt;
    for (const PDef& p : ps) {
        listAt.push_back(uint32_t(code.size()));
        for (const std::string& s : p.items) code.insert(code.end(), s.c_str(), s.c_str() + s.size() + 1);
    }
    while (code.size() < 64) code.push_back(0);
    std::vector<uint8_t> f(472, 0);
    put32(f, 0, magic);
    f[4] = 1;
    f[6] = uint8_t(flags);
    f[7] = uint8_t(flags >> 8);
    std::strncpy(reinterpret_cast<char*>(&f[8]), display, 31);
    f[0x38] = uint8_t(ps.size());
    put32(f, 0x3C, scratch);
    put32(f, 0x40, 472);
    put32(f, 0x44, uint32_t(code.size()));
    put32(f, 0x48, 128);
    put32(f, 0x4C, 0);
    put32(f, 0x50, 4);
    put32(f, 0x54, 8);
    for (size_t i = 0; i < ps.size(); ++i) {
        const PDef& p = ps[i];
        const size_t b = 0x58 + 32 * i;
        std::strncpy(reinterpret_cast<char*>(&f[b]), p.label, 7);
        putf(f, b + 8, p.min);
        putf(f, b + 12, p.max);
        putf(f, b + 16, p.def);
        putf(f, b + 20, p.step);
        if (p.type == 'l') {
            f[b + 24] = uint8_t(listAt[i]);
            f[b + 25] = uint8_t(listAt[i] >> 8);
            f[b + 26] = uint8_t(p.items.size());
        } else {
            std::strncpy(reinterpret_cast<char*>(&f[b + 24]), p.unit, 5);
        }
        f[b + 30] = uint8_t(p.type);
    }
    f.insert(f.end(), code.begin(), code.end());
    return f;
}

// Nine plain parameters: the test sets the plugin's own indices directly.
std::vector<PDef> rawParams(int n = 9) {
    std::vector<PDef> v;
    for (int i = 0; i < n; ++i) v.push_back({"p", -1e6f, 1e6f, 0, 0.01f, "", 'f', {}});
    return v;
}

std::vector<uint8_t> gStore(ptfx::storeBytes());

void attachTo(Harness& h) {
    ptfx::setPrepare(&nativePrepare);
    ptfx::attach(&h.fs, gStore.data(), gStore.size());
}

std::vector<int16_t> noise(uint32_t n, uint32_t seed = 12345) {
    std::vector<int16_t> s(n);
    for (uint32_t i = 0; i < n; ++i) {
        seed = seed * 1103515245u + 12345u;
        // A tone and noise: something for the spectral effects to keep.
        s[i] = int16_t(8000.0 * std::sin(double(i) * 0.031) + double(int((seed >> 16) % 8001) - 4000));
    }
    return s;
}

// Runs the selected plugin (memory as the editor lends it).
std::vector<int16_t> runPlugin(const float* p, const std::vector<int16_t>& in, uint32_t max = 0x1E0000u) {
    std::vector<uint8_t> mem(ptfx::memBytes() + 16);
    if (!ptfx::load(mem.data(), uint32_t(mem.size()))) return {};
    const uint32_t m = ptfx::frames(p, uint32_t(in.size()), max);
    std::vector<int16_t> out(m + 1, 0x7777);
    const uint32_t got = ptfx::run(p, in.data(), uint32_t(in.size()), out.data(), m);
    CHECK_EQ(int(out[m]), 0x7777);   // never past m
    out.resize(got);
    return out;
}

// One plugin on its own card: "Effects/<tag>.ptfx".
void only(Harness& h, const char* tag, const std::vector<PDef>& ps, uint32_t scratch = 0, uint16_t flags = 2) {
    h.fs.files.clear();
    h.fs.mkdir("Effects");
    h.fs.files[std::string("Effects/") + tag + ".ptfx"] = makePtfx(tag, tag, ps, scratch, flags);
    attachTo(h);
    CHECK_EQ(ptfx::scan(), 1);
    CHECK(ptfx::select(0));
}

double rms(const std::vector<int16_t>& v, size_t a = 0, size_t b = SIZE_MAX) {
    b = b < v.size() ? b : v.size();
    double s = 0;
    for (size_t i = a; i < b; ++i) s += double(v[i]) * double(v[i]);
    return b > a ? std::sqrt(s / double(b - a)) : 0.0;
}
double rmsDiff(const std::vector<int16_t>& v) {   // the level of the highs
    double s = 0;
    for (size_t i = 1; i < v.size(); ++i) s += double(v[i] - v[i - 1]) * double(v[i] - v[i - 1]);
    return v.size() > 1 ? std::sqrt(s / double(v.size() - 1)) : 0.0;
}
double corr(const std::vector<int16_t>& x, const std::vector<int16_t>& y) {
    const size_t n = x.size() < y.size() ? x.size() : y.size();
    double xy = 0, xx = 0, yy = 0;
    for (size_t i = 0; i < n; ++i) {
        xy += double(x[i]) * y[i];
        xx += double(x[i]) * x[i];
        yy += double(y[i]) * y[i];
    }
    return xx > 0 && yy > 0 ? xy / std::sqrt(xx * yy) : 0.0;
}
int maxDiff(const std::vector<int16_t>& x, const std::vector<int16_t>& y) {
    int d = x.size() == y.size() ? 0 : 99999;
    for (size_t i = 0; i < x.size() && i < y.size(); ++i) d = std::max(d, std::abs(int(x[i]) - int(y[i])));
    return d;
}
std::vector<int16_t> builtin(int fx, const float* p, const std::vector<int16_t>& in) {
    const uint32_t m = sfx::outputFrames(fx, p, uint32_t(in.size()));
    std::vector<int16_t> out(m);
    std::vector<float> scratch(sfx::scratchFloats(fx, p) + 16);
    sfx::setScratch(scratch.data(), uint32_t(scratch.size()));
    sfx::process(fx, p, in.data(), uint32_t(in.size()), out.data(), m);
    sfx::setScratch(nullptr, 0);
    return out;
}

}  // namespace

TEST(ptfx_lists_and_describes) {
    Harness h;
    CHECK(ptfx::storeBytes() <= kBrowserListBytes);   // lives in the browsers' shared list
    h.fs.mkdir("Effects");
    const std::vector<PDef> ps = {
        {"Mode", 0, 0, 1, 1, "", 'l', {"LPF", "BPF", "HPF"}},
        {"Cutoff", 40, 16000, 1200, 50, "Hz", 'd', {}},
        {"Reso", 0, 95, 30, 5, "%", 'd', {}},
        {"Drive", 0, 24, 6, 0.5f, "dB", 'f', {}},
        {"Gain", -1, 1, 0.25f, 0.01f, "", 'f', {}},
    };
    h.fs.files["Effects/b.ptfx"] = makePtfx("Beta", "x", ps);
    h.fs.files["Effects/a.PTFX"] = makePtfx("Alpha", "x", {});
    h.fs.files["Effects/._a.ptfx"] = {0, 5, 22, 7};
    h.fs.files["Effects/bad.ptfx"] = makePtfx("Bad", "x", {}, 0, 2, 0x12345678u);
    h.fs.files["Effects/imx.ptfx"] = makePtfx("Big", "x", {}, 0, 0x12);
    h.fs.files["Effects/noname.ptfx"] = makePtfx("", "x", {});
    h.fs.files["Effects/readme.txt"] = {'h', 'i'};
    attachTo(h);
    CHECK_EQ(ptfx::scan(), 3);
    CHECK_STR(ptfx::name(0), "Alpha");
    CHECK_STR(ptfx::name(1), "Beta");
    CHECK_STR(ptfx::name(2), "noname");
    CHECK(ptfx::select(1));
    const sfx::EffectDesc& d = ptfx::desc();
    CHECK_STR(d.name, "Beta");
    CHECK_EQ(d.params, 5);
    CHECK_STR(d.p[0].name, "Mode");
    CHECK_NEAR(d.p[0].max, 2, 1e-6);
    float p[sfx::kMaxParams];
    ptfx::defaults(p);
    CHECK_NEAR(p[0], 1, 1e-6);
    CHECK_NEAR(p[1], 1200, 1e-6);
    char v[24];
    ptfx::format(0, p[0], v, sizeof(v));
    CHECK_STR(v, "BPF");
    ptfx::format(0, 2, v, sizeof(v));
    CHECK_STR(v, "HPF");
    ptfx::format(1, 1200, v, sizeof(v));
    CHECK_STR(v, "1200 Hz");
    ptfx::format(2, 30, v, sizeof(v));
    CHECK_STR(v, "30%");
    ptfx::format(3, 6.5f, v, sizeof(v));
    CHECK_STR(v, "6.5 dB");
    ptfx::format(4, 0.25f, v, sizeof(v));
    CHECK_STR(v, "0.25");
    CHECK_NEAR(ptfx::clamp(0, 7.4f), 2, 1e-6);
    CHECK_NEAR(ptfx::clamp(1, 20), 40, 1e-6);
    CHECK_NEAR(ptfx::clamp(1, 1210.4f), 1210, 1e-6);
    // Through the Sample Editor's tables: numbers from kEffectCount on.
    CHECK_STR(sfx::effect(sfx::kEffectCount).name, "Beta");
    sfx::formatParam(sfx::kEffectCount, 1, 50, v, sizeof(v));
    CHECK_STR(v, "50 Hz");
    // Without the platform hook (the simulator): listed, not run.
    ptfx::setPrepare(nullptr);
    std::vector<uint8_t> mem(ptfx::memBytes() + 16);
    CHECK(!ptfx::load(mem.data(), uint32_t(mem.size())));
}

TEST(ptfx_reference_effects_run) {
    Harness h;
    const std::vector<int16_t> in = noise(20000);
    float p[sfx::kMaxParams] = {};
    // Multi-Mode Filter: same length, filtered, the same twice.
    only(h, "multimode_filter",
         {{"Mode", 0, 0, 0, 1, "", 'l', {"LPF 24dB", "LPF 12dB", "BPF 12dB", "HPF 12dB", "Notch", "Peak"}},
          {"Cutoff", 40, 16000, 1200, 50, "Hz", 'd', {}},
          {"Reso", 0, 95, 30, 5, "%", 'd', {}},
          {"Drive", 0, 0, 0, 1, "", 'l', {"Clean", "SoftSat", "WarmTube", "HardClip", "Foldback"}}});
    ptfx::defaults(p);
    std::vector<int16_t> a = runPlugin(p, in);
    CHECK_EQ(int(a.size()), int(in.size()));
    CHECK(rmsDiff(a) < rmsDiff(in) * 0.5);   // the 1.2 kHz low-pass takes the noise
    CHECK(maxDiff(a, runPlugin(p, in)) == 0);
    // Stereo Delay: the tail is appended (set_extra_samples).
    only(h, "stereo_delay",
         {{"Time", 10, 1000, 250, 10, "ms", 'd', {}},
          {"Fdbk", 0, 95, 40, 1, "%", 'd', {}},
          {"Mix", 0, 100, 50, 1, "%", 'd', {}},
          {"Tail", 0, 0, 0, 1, "", 'l', {"Ring Out", "Cut Off"}}},
         176404);
    ptfx::defaults(p);
    a = runPlugin(p, in);
    CHECK(a.size() > in.size() + 11025);
    CHECK(rms(a, in.size()) > 100);   // the echoes ring on
    // Add Silence at the beginning: prepended samples get no input.
    only(h, "add_silence", {{"Where", 0, 0, 0, 1, "", 'l', {"Beginning", "End"}}, {"Length", 10, 5000, 100, 10, " ms", 'd', {}}});
    ptfx::defaults(p);
    a = runPlugin(p, in);
    CHECK_EQ(int(a.size()), int(in.size()) + 4410);
    CHECK(rms(a, 0, 4410) == 0.0);
    CHECK(maxDiff(std::vector<int16_t>(a.begin() + 4410, a.end()), in) == 0);
    // A generator: no input, up to 2 s, ends when it is done.
    only(h, "analog_kick_gen",
         {{"TopFreq", 60, 250, 140, 1, "Hz", 'd', {}},
          {"SubFreq", 30, 80, 45, 1, "Hz", 'd', {}},
          {"Decay", 100, 900, 350, 10, "ms", 'd', {}},
          {"Click", 0, 1, 0.7f, 0.05f, "", 'f', {}}},
         0, 1);
    ptfx::defaults(p);
    CHECK(ptfx::generator());
    a = runPlugin(p, in);
    CHECK_EQ(int(a.size()), int(ptfx::kGenFrames));
    CHECK(rms(a) > 500);
}

TEST(ptfx_fork_plugins_follow_the_builtin_effects) {
    Harness h;
    const std::vector<int16_t> in = noise(30000);
    std::vector<int16_t> a, b;
    // Transcode: the same codec chain, sample for sample.
    {
        only(h, "ot_transcode", rawParams());
        const float settings[3][7] = {{3, 0, 1, 0, 0, 1, 100}, {0, 2, 4, 20, 10, 2, 80}, {5, 1, 2, 5, 20, 0, 100}};
        for (const auto& s : settings) {
            float p[sfx::kMaxParams] = {};
            std::memcpy(p, s, sizeof(s));
            a = runPlugin(p, in);
            b = builtin(sfx::kTranscode, p, in);
            CHECK_EQ(maxDiff(a, b), 0);
        }
    }
    // Clipper (Match off): the curves; Tape's arctan is approximated.
    {
        only(h, "ot_clipper", rawParams());
        for (int c = 0; c < 7; ++c) {
            const float pb[6] = {float(c), 9, 40, 0, 1.5f, 90};
            const float pp[sfx::kMaxParams] = {float(c), 9, 40, 1.5f, 90};
            a = runPlugin(pp, in);
            b = builtin(sfx::kClipper, pb, in);
            if (c == 6) CHECK(maxDiff(a, b) < 300);
            else CHECK(maxDiff(a, b) <= 1);
        }
    }
    // Radio: phase accumulators instead of sin(2 pi f t): the same signal.
    {
        only(h, "ot_radio", rawParams());
        const float p[sfx::kMaxParams] = {1, 60, 30, 15, 800, 25, 20, 100};
        a = runPlugin(p, in);
        b = builtin(sfx::kRadio, p, in);
        CHECK_EQ(int(a.size()), int(b.size()));
        std::printf("  radio: correlation %.4f\n", corr(a, b));
        CHECK(corr(a, b) > 0.98);
    }
    // DCT Crush: the frames look ahead through short block returns.
    {
        only(h, "ot_dctcrush", rawParams());
        for (int blk = 0; blk < 4; ++blk) {
            const float p[sfx::kMaxParams] = {float(blk), 20, 6, 0, 100};
            a = runPlugin(p, in);
            b = builtin(sfx::kDctCrush, p, in);
            CHECK_EQ(int(a.size()), int(in.size()));
            std::printf("  dct crush %d: correlation %.4f\n", blk, corr(a, b));
            CHECK(corr(a, b) > 0.97);
        }
    }
    // Paulstretch: Stretch x the selection, the level of the built-in one.
    {
        only(h, "ot_paulstretch", rawParams(), 1048576);
        for (int w = 0; w < 3; ++w) {
            const float p[sfx::kMaxParams] = {4, float(w)};
            a = runPlugin(p, in);
            const float pb[2] = {4, float(w)};
            b = builtin(sfx::kPaulstretch, pb, in);
            CHECK_EQ(int(a.size()), int(in.size()) * 4);
            std::printf("  paulstretch window %d: rms %.0f (built-in %.0f)\n", w, rms(a), rms(b));
            CHECK(rms(a) > rms(b) * 0.7 && rms(a) < rms(b) * 1.4);
            CHECK(rms(a, a.size() / 2, a.size() / 2 + 4000) > 300);   // no holes in the middle
            CHECK_EQ(maxDiff(a, runPlugin(p, in)), 0);                 // Preview == Apply
        }
        // Longer than a block, Stretch 2, the 200 ms window: the input it
        // waits for comes in later blocks; the result follows the built-in one.
        const std::vector<int16_t> lng = noise(100000, 99);
        const float p[sfx::kMaxParams] = {2, 2};
        a = runPlugin(p, lng);
        b = builtin(sfx::kPaulstretch, p, lng);
        CHECK_EQ(int(a.size()), 200000);
        std::printf("  paulstretch long: correlation %.4f\n", corr(a, b));
        CHECK(corr(a, b) > 0.95);
    }
    // Granular: Length x the selection; grains all through it.
    {
        only(h, "ot_granular", rawParams(), 1048576);
        const float p[sfx::kMaxParams] = {2.5f, 60, 40, 300, 5, 30};
        a = runPlugin(p, in);
        CHECK_EQ(int(a.size()), int(in.size() * 5 / 2));
        for (int q = 1; q < 5; ++q) CHECK(rms(a, a.size() * size_t(q) / 5, a.size() * size_t(q) / 5 + 3000) > 300);
    }
}

TEST(ptfx_scratch_granted_in_part) {
    // Less memory than asked (as CE grants at most 640 KB): the plugin keeps
    // less input, the result ends at Stretch x what it kept.
    Harness h;
    only(h, "ot_paulstretch", rawParams(), 1048576);
    const std::vector<int16_t> in = noise(90000);
    const float p[sfx::kMaxParams] = {3, 0};
    std::vector<uint8_t> mem(ptfx::minBytes() + 120000);
    CHECK(ptfx::load(mem.data(), uint32_t(mem.size())));
    const uint32_t m = ptfx::frames(p, uint32_t(in.size()), 0x1E0000u);
    std::vector<int16_t> out(m);
    const uint32_t got = ptfx::run(p, in.data(), uint32_t(in.size()), out.data(), m);
    std::printf("  %u frames asked, %u made\n", unsigned(m), unsigned(got));
    CHECK(got > in.size() && got < in.size() * 3);
    CHECK(ptfx::load(mem.data(), ptfx::minBytes()));
    CHECK(!ptfx::load(mem.data(), ptfx::minBytes() - 8));
}

TEST(ptfx_in_the_sample_editor) {
    Harness h;
    App& a = *h.app;
    const std::vector<int16_t> src = noise(30000);
    std::vector<uint8_t> wav(src.size() * 2 + 64);
    wav.resize(wavEncode(src.data(), uint32_t(src.size()), 1, wav.data(), wav.size()));
    h.fs.files["n.wav"] = wav;
    CHECK(a.importSample("n.wav", 0));
    a.setCurrentInstrument(0);
    h.fs.mkdir("Effects");
    h.fs.files["Effects/delay.ptfx"] =
        makePtfx("Stereo Delay", "stereo_delay",
                 {{"Time", 10, 1000, 250, 10, "ms", 'd', {}},
                  {"Fdbk", 0, 95, 40, 1, "%", 'd', {}},
                  {"Mix", 0, 100, 50, 1, "%", 'd', {}},
                  {"Tail", 0, 0, 0, 1, "", 'l', {"Ring Out", "Cut Off"}}},
                 176404);
    h.fs.files["Effects/radio.ptfx"] =
        makePtfx("OT Radio", "ot_radio", {{"Band", 0, 0, 1, 1, "", 'l', {"2.4 kHz", "3 kHz", "4 kHz", "6 kHz"}}});
    ptfx::setPrepare(&nativePrepare);
    h.tap(btn::SampleEditor);
    EditorScreen& e = *static_cast<EditorScreen*>(a.ui.screen(kModEditor));
    CHECK(!e.inert);
    CHECK_EQ(e.effectTotal(), sfx::kEffectCount + 2);
    // The list: the plugins after the built-in effects.
    h.tap(btn::F8);   // focus the list
    e.setEffect(sfx::kEffectCount);
    h.run(2);
    CHECK_STR(sfx::effect(e.effect).name, "OT Radio");
    e.setEffect(sfx::kEffectCount + 1);
    CHECK_STR(sfx::effect(e.effect).name, "Stereo Delay");
    CHECK_NEAR(e.prm()[0], 250, 1e-6);
    h.tap(btn::F6);   // parameters
    CHECK_EQ(e.page, 2);
    char v[24];
    sfx::formatParam(e.effect, 3, e.prm()[3], v, sizeof(v));
    CHECK_STR(v, "Ring Out");
    h.run(5);
    CHECK(e.preview());
    CHECK(e.previewPlaying);
    h.tap(btn::F8);   // Apply
    h.run(5);
    const uint32_t len = a.bank.frames(0);
    std::printf("  delay applied: %u -> %u frames\n", unsigned(src.size()), unsigned(len));
    CHECK(len > src.size() + 11025);
    CHECK(e.undoAvail);
    // Leaving and coming back: listed again, the same plugin selected.
    h.tap(btn::Pattern);
    h.tap(btn::SampleEditor);
    CHECK_STR(sfx::effect(e.effect).name, "Stereo Delay");
    // A plugin removed from the card: back to the first effect.
    h.fs.files.erase("Effects/delay.ptfx");
    h.tap(btn::Pattern);
    h.tap(btn::SampleEditor);
    CHECK_EQ(e.effect, 0);
    // No platform hook (simulator): a message, the sample unchanged.
    ptfx::setPrepare(nullptr);
    e.setEffect(sfx::kEffectCount);
    const uint32_t before = a.bank.frames(0);
    CHECK(!e.apply());
    CHECK(a.ui.askOpen());
    CHECK_EQ(int(a.bank.frames(0)), int(before));
    ptfx::setPrepare(&nativePrepare);
}

TEST(ptfx_built_plugin_files) {
    // `make ptfx` output, when present: real builder headers read back.
    const char* names[6] = {"ot_clipper", "ot_dctcrush", "ot_granular", "ot_paulstretch", "ot_radio", "ot_transcode"};
    Harness h;
    h.fs.mkdir("Effects");
    int found = 0;
    for (const char* n : names) {
        std::ifstream f(std::string("../build/ptfx/Effects/") + n + ".ptfx", std::ios::binary);
        if (!f) continue;
        h.fs.files[std::string("Effects/") + n + ".ptfx"] =
            std::vector<uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        ++found;
    }
    if (!found) {
        std::printf("  (no build/ptfx: `make ptfx` not run, skipped)\n");
        return;
    }
    attachTo(h);
    CHECK_EQ(ptfx::scan(), found);
    for (int k = 0; k < found; ++k) {
        CHECK(ptfx::select(k));
        const sfx::EffectDesc& d = ptfx::desc();
        CHECK(std::strncmp(d.name, "OT ", 3) == 0);
        CHECK(d.params >= 2);
        for (int i = 0; i < d.params; ++i) {
            char v[24];
            float p[sfx::kMaxParams];
            ptfx::defaults(p);
            ptfx::format(i, p[i], v, sizeof(v));
            CHECK(v[0] != 0 && v[0] != '-');
        }
        if (!std::strcmp(d.name, "OT Transcode")) {
            char v[24];
            ptfx::format(0, 3, v, sizeof(v));
            CHECK_STR(v, "G726 32k");
            ptfx::format(5, 2, v, sizeof(v));
            CHECK_STR(v, "Glitch");
        }
    }
}
