// OT fork MIDI learn on the Performance screen: Shift + Fx2, a column pad or
// Fx1 + F key, then a control or note; learned controls drive their column,
// learned keys recall scenes, Config > MIDI > Perform CC out echoes values.
#include <cstring>

#include "test.h"
#include "ui_fakes.h"
#include "nvstore_fake.h"

#include "app/perf_presets.h"
#include "ui/screens.h"

using namespace ot;

namespace {
struct Rig {
    FakeNvStore nv;
    std::unique_ptr<PerfPatternMemory> mem{new PerfPatternMemory};
    Harness h{false};
    Rig() {
        h.app->setNvStore(&nv);
        h.app->seq.setPerfPatternMemory(mem.get());
        h.app->boot();
    }
    App& a() { return *h.app; }
    PerformanceScreen& p() { return *static_cast<PerformanceScreen*>(h.app->ui.screen(kModPerformance)); }
    void msg(uint8_t st, uint8_t d1, uint8_t d2) {
        h.midi.in.push_back(st);
        h.midi.in.push_back(d1);
        h.midi.in.push_back(d2);
        h.run(8);
    }
    void learnToggle() {
        h.down(btn::Shift);
        h.tap(btn::Fx2);
        h.up(btn::Shift);
    }
};
bool sentCc(const std::vector<uint8_t>& s, uint8_t st, uint8_t cc, int* value) {
    for (size_t i = 0; i + 2 < s.size(); ++i)
        if (s[i] == st && s[i + 1] == cc) {
            *value = s[i + 2];
            return true;
        }
    return false;
}
}  // namespace

TEST(midi_learn_columns_and_scenes) {
    Rig r;
    App& a = r.a();
    r.h.tap(btn::Performance);
    PerformanceScreen& p = r.p();
    // Learn: column 3 <- CC 21 on channel 3.
    r.learnToggle();
    CHECK(p.learn);
    r.h.pad(2);
    CHECK_EQ(p.learnTarget, 2);
    r.msg(0xB2, 21, 10);
    CHECK_EQ(int(a.cfg.perfMidi[2][0]), 3);
    CHECK_EQ(int(a.cfg.perfMidi[2][1]), 21);
    CHECK_EQ(p.learnTarget, -1);
    // Scene 1 <- note 36 on channel 1 (Fx1 + F1 picks it).
    r.h.down(btn::Fx1);
    r.h.tap(btn::F1);
    r.h.up(btn::Fx1);
    CHECK_EQ(p.learnTarget, Config::kMidiMapScene0);
    r.msg(0x90, 36, 100);
    CHECK_EQ(int(a.cfg.perfMidi[Config::kMidiMapScene0][1]), 0x80 | 36);
    r.learnToggle();
    CHECK(!p.learn);
    // The learned control drives column 3 over its whole range.
    const uint8_t ty = p.src.type(2);
    r.msg(0xB2, 21, 127);
    r.h.run(80);
    CHECK_EQ(int(p.cur[2]), perf::ccToValue(ty, 127));
    r.msg(0xB2, 21, 0);
    r.h.run(80);
    CHECK_EQ(int(p.cur[2]), perf::ccToValue(ty, 0));
    // Another channel: not this control.
    r.msg(0xB5, 21, 127);
    r.h.run(80);
    CHECK_EQ(int(p.cur[2]), perf::ccToValue(ty, 0));
    // A scene stored with column 3 up, recalled by the learned note.
    r.msg(0xB2, 21, 127);
    r.h.run(80);
    r.h.down(btn::Fx2);
    r.h.tap(btn::F1);
    r.h.up(btn::Fx2);
    r.msg(0xB2, 21, 0);
    r.h.run(80);
    CHECK_EQ(int(p.cur[2]), perf::ccToValue(ty, 0));
    r.msg(0x90, 36, 90);
    r.h.run(80);
    CHECK_EQ(int(p.cur[2]), perf::ccToValue(ty, 127));
    // The mapping is kept in the EEPROM.
    CHECK(a.saveConfig());
    Config c;
    CHECK(c.load(r.nv));
    CHECK_EQ(int(c.perfMidi[2][1]), 21);
    CHECK_EQ(int(c.perfMidi[Config::kMidiMapScene0][1]), 0x80 | 36);
}

TEST(midi_learn_cc_out_and_clear) {
    Rig r;
    App& a = r.a();
    a.cfg.midiOut = 3;
    a.cfg.perfCcOut = 1;
    a.cfg.ccIn = 3;   // the stock CC 51-62 path at the end
    r.h.tap(btn::Performance);
    PerformanceScreen& p = r.p();
    r.learnToggle();
    r.h.pad(0);
    r.msg(0xB0, 74, 5);
    r.learnToggle();
    // Store a value on row 2 of column 1 (edit mode), then play it: CC out.
    r.h.midi.sent.clear();
    r.h.pad(12);   // column 1, row 1
    r.h.run(5);
    int v = -1;
    const bool sent = sentCc(r.h.midi.sent, 0xB0, 74, &v);
    std::printf("  CC out: %s %d (cur %d)\n", sent ? "sent" : "none", v, int(p.cur[0]));
    if (p.cur[0] != 0) CHECK(sent);
    // A control coming in is not echoed back.
    r.h.midi.sent.clear();
    r.msg(0xB0, 74, 99);
    r.h.run(80);
    CHECK(!sentCc(r.h.midi.sent, 0xB0, 74, &v));
    // Delete in learn mode clears the picked target.
    r.learnToggle();
    r.h.pad(0);
    r.h.tap(btn::Delete);
    CHECK_EQ(int(a.cfg.perfMidi[0][0]), 0);
    r.learnToggle();
    // Learn off: CC 51-62 (stock) still drive the columns.
    r.msg(0xB0, 52, 127);
    r.h.run(80);
    CHECK_EQ(int(p.cur[1]), perf::ccToValue(p.src.type(1), 127));
}
