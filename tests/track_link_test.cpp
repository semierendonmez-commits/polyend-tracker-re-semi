// OT fork track links (sequencer/track_link.h): a track plays live with
// another track's notes, its own instrument; muted sources, note-offs, no
// chains, the per-project file and the Pattern page 3 Link popup.
#include "test.h"

#include <cstring>
#include <vector>

#include "app/fork_project.h"
#include "mem_storage.h"
#include "model/pattern.h"
#include "model/project.h"
#include "sequencer/sequencer.h"
#include "ui/screens.h"
#include "ui_fakes.h"

using namespace ot;

namespace {

struct Ev {
    enum Kind { NoteOn, NoteOff, ApplyFx, ResetFx, Click, Midi } kind;
    int track;
    int a, b, c;          // NoteOn: note, instr, volume; NoteOff: mode; ApplyFx: slot,type,value; Midi: bytes
    int len;
    uint64_t tick;        // seq.tickCount() at the event
    double time;          // seconds of audio time elapsed when the event happened
    int fxAType, fxAVal, fxBType, fxBVal;
    int ports;
};

struct FakeSink : ISequencerSink {
    Sequencer* seq = nullptr;
    double time = 0;
    std::vector<Ev> ev;
    int blockUpdates = 0;
    float lastTempo = 0;

    Ev base(Ev::Kind k, int track) {
        Ev e{};
        e.kind = k;
        e.track = track;
        e.tick = seq ? seq->tickCount() : 0;
        e.time = time;
        return e;
    }
    void voiceNoteOn(uint8_t track, const VoiceNoteOn& n) override {
        Ev e = base(Ev::NoteOn, track);
        e.a = n.note; e.b = n.instrument; e.c = n.volume;
        e.fxAType = n.fxA.type; e.fxAVal = n.fxA.value; e.fxBType = n.fxB.type; e.fxBVal = n.fxB.value;
        ev.push_back(e);
    }
    void voiceNoteOff(uint8_t track, NoteOffMode mode) override {
        Ev e = base(Ev::NoteOff, track);
        e.a = int(mode);
        ev.push_back(e);
    }
    void voiceApplyFx(uint8_t track, uint8_t slot, uint8_t type, uint8_t value) override {
        Ev e = base(Ev::ApplyFx, track);
        e.a = slot; e.b = type; e.c = value;
        ev.push_back(e);
    }
    void resetAllVoiceFx() override { ev.push_back(base(Ev::ResetFx, -1)); }
    void engineBlockUpdate(float bpm) override { ++blockUpdates; lastTempo = bpm; }
    void metronomeClick(bool accent, uint8_t vel) override {
        Ev e = base(Ev::Click, -1);
        e.a = accent; e.b = vel;
        ev.push_back(e);
    }
    void midiSend(uint8_t ports, const uint8_t* b, uint8_t len) override {
        Ev e = base(Ev::Midi, -1);
        e.a = b[0]; e.b = len > 1 ? b[1] : -1; e.c = len > 2 ? b[2] : -1;
        e.len = len;
        e.ports = ports;
        ev.push_back(e);
    }

    std::vector<Ev> of(Ev::Kind k, int track = -2) const {
        std::vector<Ev> r;
        for (const auto& e : ev)
            if (e.kind == k && (track == -2 || e.track == track)) r.push_back(e);
        return r;
    }
    std::vector<Ev> midiStatus(int status) const {
        std::vector<Ev> r;
        for (const auto& e : ev)
            if (e.kind == Ev::Midi && e.a == status) r.push_back(e);
        return r;
    }
    std::vector<Ev> midiNoteOns() const {
        std::vector<Ev> r;
        for (const auto& e : ev)
            if (e.kind == Ev::Midi && (e.a & 0xF0) == 0x90) r.push_back(e);
        return r;
    }
};

struct Load { int num; bool sync; uint64_t tick; };

struct FakeHost : ISequencerHost {
    Sequencer* seq = nullptr;
    std::vector<Load> loads;
    int patternEnds = 0, patternAdvances = 0, stepChanges = 0, dirty = 0;
    bool songScreen = false;
    void loadPattern(uint8_t number, Pattern& dst, bool sync) override {
        loads.push_back({number, sync, seq ? seq->tickCount() : 0});
        dst.setDefault();
        for (auto& t : dst.tracks) t.lastStep = 3;   // 4-step patterns
        dst.tracks[0].steps[0].note = int8_t(number * 10 % 128);
        dst.tracks[0].steps[0].instrument = 0;
    }
    void onStepChanged() override { ++stepChanges; }
    std::vector<uint64_t> endTicks;
    void onPatternEnd() override {
        ++patternEnds;
        endTicks.push_back(seq ? seq->tickCount() : 0);
    }
    void onPatternAdvanced() override { ++patternAdvances; }
    void markPatternDirty() override { ++dirty; }
    bool songScreenActive() override { return songScreen; }
};

struct Rig {
    Project project;
    Pattern a, b;
    FakeSink sink;
    FakeHost host;
    Sequencer seq;
    Rig(float bpm = 120.0f) {
        a.setDefault();
        b.setDefault();
        project.setTempo(bpm);
        sink.seq = &seq;
        host.seq = &seq;
        seq.attach(&sink, &host, &project, &a, &b);
        seq.settings().midiOut = 3;
    }
    Step& step(int t, int s) { return seq.currentPattern()->tracks[t].steps[s]; }
    void setNote(int t, int s, int note, int instr = 0) {
        step(t, s).note = int8_t(note);
        step(t, s).instrument = uint8_t(instr);
    }
    // FX-A = model fx[1], FX-B = model fx[0].
    void fxA(int t, int s, int type, int v) { step(t, s).fx[1] = StepFx{uint8_t(type), uint8_t(v)}; }
    void fxB(int t, int s, int type, int v) { step(t, s).fx[0] = StepFx{uint8_t(type), uint8_t(v)}; }
    void block() {
        seq.processBlock(128);
        sink.time += 128.0 / 44122.6484;
    }
    void blocks(int n) { for (int i = 0; i < n; ++i) block(); }
    // Run blocks until `pred` holds (max n blocks).
    template <class F> bool runUntil(F pred, int n = 100000) {
        for (int i = 0; i < n; ++i) {
            if (pred()) return true;
            block();
        }
        return pred();
    }
};

}  // namespace

namespace {
TrackLink mk(int src, int instr, int note = -1, int xp = 0, int level = 100, int chance = 100, int fx = 0) {
    TrackLink l;
    l.source = uint8_t(src);
    l.instr = uint8_t(instr);
    l.note = int8_t(note);
    l.transpose = int8_t(xp);
    l.level = uint8_t(level);
    l.chance = uint8_t(chance);
    l.fx = uint8_t(fx);
    return l;
}
// One pass of the 16-step pattern.
void onePass(Rig& r) {
    r.seq.play(0);
    r.runUntil([&] { return r.host.patternEnds >= 1; });
    r.seq.stop();
}
}  // namespace

TEST(link_layers_the_source_notes) {
    Rig r;
    for (auto& t : r.seq.currentPattern()->tracks) t.lastStep = 15;
    r.setNote(0, 0, 48, 0);
    r.setNote(0, 4, 50, 0);
    r.step(0, 4).fx[0] = StepFx{18, 80};   // volume 80
    r.setNote(0, 8, 53, 0);
    TrackLink links[16];
    links[2] = mk(1, 5, -1, 12, 50);
    r.seq.setLinks(links);
    onePass(r);
    const auto src = r.sink.of(Ev::NoteOn, 0), lay = r.sink.of(Ev::NoteOn, 2);
    CHECK_EQ(int(src.size()), 3);
    CHECK_EQ(int(lay.size()), 3);
    for (size_t i = 0; i < lay.size() && i < src.size(); ++i) {
        CHECK_EQ(lay[i].tick, src[i].tick);    // the same moment
        CHECK_EQ(lay[i].b, 5);                 // its own instrument
        CHECK_EQ(lay[i].a, src[i].a + 12);     // the source's note, transposed
    }
    CHECK_EQ(lay[0].c, 50);   // instrument volume (100) x 50 %
    CHECK_EQ(lay[1].c, 40);   // the step's volume 80 x 50 %
    // Other tracks get nothing.
    CHECK(r.sink.of(Ev::NoteOn, 1).empty());
}

TEST(link_fixed_note_chance_and_fx) {
    Rig r;
    for (auto& t : r.seq.currentPattern()->tracks) t.lastStep = 15;
    for (int s = 0; s < 16; ++s) {
        r.setNote(0, s, 40 + s, 0);
        r.step(0, s).fx[0] = StepFx{25, uint8_t(s)};   // Slice
    }
    TrackLink links[16];
    links[1] = mk(1, 3, 60, 0, 100, 100, 1);   // fixed C, FX copied
    links[3] = mk(1, 4, 60, 0, 100, 0, 0);     // chance 0: never
    links[4] = mk(1, 6, -1, 0, 100, 50, 0);    // about half
    r.seq.setLinks(links);
    onePass(r);
    const auto a = r.sink.of(Ev::NoteOn, 1);
    CHECK_EQ(int(a.size()), 16);
    bool fixed = true, fx = true;
    for (size_t i = 0; i < a.size(); ++i) {
        fixed = fixed && a[i].a == 60;
        fx = fx && a[i].fxBType == 25 && a[i].fxBVal == int(i);
    }
    CHECK(fixed && fx);
    CHECK(r.sink.of(Ev::NoteOn, 3).empty());
    const int half = int(r.sink.of(Ev::NoteOn, 4).size());
    std::printf("  chance 50: %d of 16\n", half);
    CHECK(half > 1 && half < 15);
}

TEST(link_muted_source_plays_only_the_layer) {
    Rig r;
    for (auto& t : r.seq.currentPattern()->tracks) t.lastStep = 15;
    r.setNote(0, 0, 48, 0);
    r.setNote(0, 8, 48, 0);
    r.step(0, 8).fx[0] = StepFx{15, 100};   // Tempo 200: a muted source must not change it
    TrackLink links[16];
    links[1] = mk(1, 2);
    r.seq.setLinks(links);
    r.project.setTrackMuted(0, true);
    onePass(r);
    CHECK(r.sink.of(Ev::NoteOn, 0).empty());
    CHECK_EQ(int(r.sink.of(Ev::NoteOn, 1).size()), 2);
    CHECK(!r.seq.tempoOverrideActive());
    // A muted layer stays silent.
    r.sink.ev.clear();
    r.host.patternEnds = 0;
    r.step(0, 8).fx[0] = StepFx{};
    r.project.setTrackMuted(0, false);
    r.project.setTrackMuted(1, true);
    onePass(r);
    CHECK_EQ(int(r.sink.of(Ev::NoteOn, 0).size()), 2);
    CHECK(r.sink.of(Ev::NoteOn, 1).empty());
}

TEST(link_note_off_and_no_chains) {
    Rig r;
    for (auto& t : r.seq.currentPattern()->tracks) t.lastStep = 15;
    r.setNote(0, 0, 48, 0);
    r.setNote(0, 2, kNoteOff, 0);
    r.setNote(1, 6, 55, 1);   // track 2's own note
    TrackLink links[16];
    links[1] = mk(1, 2);   // 2 follows 1
    links[2] = mk(2, 3);   // 3 follows 2: only 2's own notes
    links[5] = mk(6, 3);   // itself: ignored
    r.seq.setLinks(links);
    onePass(r);
    const auto on1 = r.sink.of(Ev::NoteOn, 1), on2 = r.sink.of(Ev::NoteOn, 2);
    CHECK_EQ(int(on1.size()), 2);             // linked 48, own 55
    CHECK_EQ(int(on2.size()), 1);             // only the own 55, no chain
    if (!on2.empty()) CHECK_EQ(on2[0].a, 55);
    // The source's OFF ends the linked note (before the own note at step 6).
    const auto off1 = r.sink.of(Ev::NoteOff, 1);
    CHECK(!off1.empty());
    if (!off1.empty() && on1.size() == 2) CHECK(off1[0].tick < on1[1].tick && off1[0].tick > on1[0].tick);
    CHECK(r.sink.of(Ev::NoteOn, 5).empty());
}

TEST(link_live_pads_layer_too) {
    Rig r;
    TrackLink links[16];
    links[3] = mk(1, 7, -1, -12);
    r.seq.setLinks(links);
    r.seq.liveNoteOn(60, 100, 0, 0);
    const auto lay = r.sink.of(Ev::NoteOn, 3);
    CHECK_EQ(int(lay.size()), 1);
    if (!lay.empty()) {
        CHECK_EQ(lay[0].a, 48);
        CHECK_EQ(lay[0].b, 7);
    }
    r.seq.liveNoteOff(60, false);
    CHECK_EQ(int(r.sink.of(Ev::NoteOff, 3).size()), 1);
}

TEST(link_project_file_roundtrip) {
    MemStorage fs;
    ForkProject f;
    f.setDefault();
    f.link[4] = mk(2, 9, 60, -5, 70, 80, 1);
    CHECK(!f.isDefault());
    CHECK(f.save(fs, "Workspace"));
    ForkProject g;
    CHECK(g.load(fs, "Workspace"));
    CHECK_EQ(g.link[4].source, 2);
    CHECK_EQ(g.link[4].instr, 9);
    CHECK_EQ(g.link[4].note, 60);
    CHECK_EQ(g.link[4].transpose, -5);
    CHECK_EQ(g.link[4].level, 70);
    CHECK_EQ(g.link[4].chance, 80);
    CHECK_EQ(g.link[4].fx, 1);
    CHECK_EQ(g.link[0].source, 0);
    // Sanitised: no self links, tracks 9..16 play synths / MIDI.
    f.link[4].source = 5;
    f.link[10] = mk(1, 3);
    CHECK(f.save(fs, "Workspace"));
    CHECK(g.load(fs, "Workspace"));
    CHECK_EQ(g.link[4].source, 0);
    CHECK(g.link[10].instr >= 48);
    // No links, no sidechain: no file.
    f.setDefault();
    CHECK(f.save(fs, "Workspace"));
    CHECK(!fs.exists("Workspace/otfork.bin"));
}

TEST(link_popup_sets_the_cursor_track) {
    Harness h;
    App& a = *h.app;
    h.tap(btn::Pattern);
    PatternScreen& p = *static_cast<PatternScreen*>(a.ui.screen(kModPattern));
    h.tap(btn::F8);
    h.tap(btn::F8);
    CHECK_EQ(p.page, 2);
    h.tap(btn::Right);   // cursor on track 2
    const int tr = p.track;
    h.tap(btn::F4);
    CHECK(p.linkOpen);
    // F1 Source: pad 1 = track 1.
    h.pad(1);
    CHECK_EQ(a.fork.link[tr].source, 1);
    // F3 Note: Down steps from Follow to the first fixed note.
    h.tap(btn::F3);
    CHECK_EQ(a.fork.link[tr].note, -1);
    h.tap(btn::Down);
    CHECK_EQ(a.fork.link[tr].note, 0);
    // F6 Chance with the encoder.
    h.tap(btn::F6);
    h.enc(-3);
    CHECK_EQ(a.fork.link[tr].chance, 97);
    // Applied to the sequencer and saved with the autosave.
    h.audio.pump(2);
    h.run(20);
    CHECK_EQ(a.seq.link(tr).source, 1);
    a.autosave();
    CHECK(h.fs.exists("Workspace/otfork.bin"));
    h.tap(btn::F8);
    CHECK(!p.linkOpen);
}
