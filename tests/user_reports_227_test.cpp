// OT fork, 2.2.7-semi: user reports (thedrexel) and the 2.2.7 merge.
//  - the Length under F2 follows a pattern switch made while playing;
//  - sidechain "ghost" notes: a muted Trigger source, and sample notes on
//    tracks 9-16 (no sample voice), still duck the targets;
//  - the export writes each WAV in whole chunks (a stems export kept 14
//    files open and wrote one sector of each in turn) with the same bytes;
//  - the config record v7 (upstream 2.2.7's v5 payload + the fork bytes)
//    reads a semi.1..semi.6 v6 record with the scenes and MIDI learn after it.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "test.h"
#include "ui_fakes.h"
#include "mem_storage.h"
#include "nvstore_fake.h"

#include "audio/engine.h"
#include "formats/wav.h"
#include "ui/screens.h"

using namespace ot;

namespace {

void addWav(Harness& h, const char* path, const std::vector<int16_t>& s, int ch = 1) {
    std::vector<uint8_t> w(s.size() * 2 + 64);
    w.resize(wavEncode(s.data(), uint32_t(s.size() / size_t(ch)), uint16_t(ch), w.data(), w.size()));
    h.fs.files[path] = w;
}

// Output level per window of 8 blocks after Play (min, max over the windows
// after the first few).
void levels(Harness& h, double& mn, double& mx) {
    App& a = *h.app;
    a.togglePlay(false);
    int16_t buf[256];
    mn = 1e9;
    mx = 0;
    for (int w = 0; w < 30; ++w) {
        double e = 0;
        for (int b = 0; b < 8; ++b) {
            h.audio.fn(buf, 128, h.audio.ctx);
            h.run(1);
            for (int i = 0; i < 256; ++i) e += double(buf[i]) * buf[i];
        }
        const double r = std::sqrt(e / 2048);
        if (w > 3) {
            mn = r < mn ? r : mn;
            mx = r > mx ? r : mx;
        }
    }
    a.togglePlay(false);
}

// Kick notes on `src`, a held tone on track 2 (the target).
void sidechainSong(Harness& h, int src, bool muteSrc) {
    App& a = *h.app;
    std::vector<int16_t> kick(4000), pad(44100 * 4);
    for (int i = 0; i < 4000; ++i) kick[i] = int16_t(20000 * std::sin(i * 0.01) * std::exp(-i / 800.0));
    for (size_t i = 0; i < pad.size(); ++i) pad[i] = int16_t(8000 * std::sin(double(i) * 0.03));
    addWav(h, "k.wav", kick);
    addWav(h, "p.wav", pad);
    CHECK(a.importSample("k.wav", 0));
    CHECK(a.importSample("p.wav", 1));
    for (int s = 0; s < 16; s += 4) {
        a.pattern().tracks[src].steps[s].note = 60;
        a.pattern().tracks[src].steps[s].instrument = 0;
    }
    a.pattern().tracks[1].steps[0].note = 60;
    a.pattern().tracks[1].steps[0].instrument = 1;
    for (auto& tr : a.pattern().tracks) tr.lastStep = 15;
    if (muteSrc) a.setTrackMute(src, true);
    a.patternEdited();
    a.fork.sc[0].source = uint8_t(src + 1);
    a.fork.sc[0].mode = 0;   // Trigger
    a.fork.sc[0].targets = audio::kScTrack0 << 1;
    a.fork.sc[0].depth = 100;
    a.markForkDirty();
    h.run(5);
}

// Counts the writes and their sizes on top of the in-memory card.
struct CountingStorage : MemStorage {
    int writes = 0;
    int32_t write(Handle h, const void* src, uint32_t len) override {
        ++writes;
        return MemStorage::write(h, src, len);
    }
};

}  // namespace

TEST(length_label_follows_a_pattern_switch_while_playing) {
    Harness h;
    App& a = *h.app;
    a.selectPattern(2);
    for (auto& tr : a.pattern().tracks) tr.lastStep = 63;
    a.pattern().tracks[0].steps[0].note = 62;
    a.patternEdited();
    a.selectPattern(1);
    for (auto& tr : a.pattern().tracks) tr.lastStep = 127;
    a.pattern().tracks[0].steps[0].note = 60;
    a.patternEdited();
    h.tap(btn::Pattern);
    h.run(30);
    CHECK_STR(a.ui.bar.label[1].top, "Length");
    CHECK_STR(a.ui.bar.label[1].bottom, "128");
    a.togglePlay(false);
    a.selectPattern(2);   // queued: the switch at the end of pattern 1
    for (int i = 0; i < 3000 && a.patternNumber() != 2; ++i) {
        h.audio.pump(2);
        h.run(2);
    }
    CHECK_EQ(a.patternNumber(), 2);
    h.run(30);
    CHECK_STR(a.ui.bar.label[1].bottom, "64");   // without a key press
    a.togglePlay(false);
}

TEST(sidechain_ghost_notes_duck_without_sound) {
    double mn, mx;
    {   // reference: no sidechain, the held tone alone after the kick track is muted
        Harness h;
        sidechainSong(h, 0, true);
        h.app->fork.sc[0].depth = 0;
        h.app->markForkDirty();
        h.run(5);
        levels(h, mn, mx);
        CHECK(mn > 3000);   // the tone, never ducked
    }
    {   // the muted kick track still triggers: the tone ducks, no kick heard
        Harness h;
        sidechainSong(h, 0, true);
        levels(h, mn, mx);
        std::printf("  muted source: %.0f .. %.0f\n", mn, mx);
        CHECK(mn < 500);
        CHECK(mx < 5000);   // no kick on top
    }
    {   // sample notes on track 10 (no sample voice there) trigger too
        Harness h;
        sidechainSong(h, 9, false);
        levels(h, mn, mx);
        std::printf("  track 10 source: %.0f .. %.0f\n", mn, mx);
        CHECK(mn < 500);
    }
    {   // a muted track that is no sidechain source stays silent and triggers nothing
        Harness h;
        sidechainSong(h, 0, true);
        h.app->fork.sc[0].source = 3;   // track 3: empty
        h.app->markForkDirty();
        h.run(5);
        levels(h, mn, mx);
        CHECK(mn > 3000);
    }
}

TEST(export_writes_whole_chunks_with_the_same_bytes) {
    std::map<std::string, std::vector<uint8_t>> files[2];
    int writes[2] = {};
    for (int buffered = 0; buffered < 2; ++buffered) {
        Harness h;
        App& a = *h.app;
        std::vector<int16_t> s(30000);
        for (size_t i = 0; i < s.size(); ++i) s[i] = int16_t(9000 * std::sin(double(i) * 0.02));
        addWav(h, "s.wav", s);
        CHECK(a.importSample("s.wav", 0));
        for (int t = 0; t < 4; ++t)
            for (int st = t; st < 32; st += 8) {
                a.pattern().tracks[t].steps[st].note = 60;
                a.pattern().tracks[t].steps[st].instrument = 0;
            }
        for (auto& tr : a.pattern().tracks) tr.lastStep = 31;
        a.patternEdited();
        CountingStorage fs;
        fs.mkdir("Export");
        std::vector<uint8_t> buf(64 * 1024);
        a.engine.setExportWriteBuffer(buffered ? buf.data() : nullptr, uint32_t(buf.size()));
        const int32_t n = a.engine.exportWav(fs, "Export/st", true, 2000, nullptr, nullptr, true);
        a.engine.setExportWriteBuffer(nullptr, 0);
        CHECK(n > 0);
        files[buffered] = fs.files;
        writes[buffered] = fs.writes;
    }
    std::printf("  writes: %d unbuffered, %d buffered\n", writes[0], writes[1]);
    CHECK(writes[1] * 20 < writes[0]);   // 4 KB chunks instead of 256 bytes
    CHECK_EQ(files[0].size(), files[1].size());
    for (auto& kv : files[0]) CHECK(files[1][kv.first] == kv.second);
    // The App lends the main-loop scratch around a song export.
    Harness h;
    h.fs.mkdir("Export");
    CHECK(h.app->exportAudio(App::ExportKind::Pattern, "Export/m.wav", nullptr, false));
    CHECK(h.fs.files.count("Export/m.wav") == 1);
}

TEST(config_v6_record_read_by_v7) {
    // A semi.1..semi.6 store: the v6 record (the 27 upstream bytes of 2.2.3,
    // then the fork bytes), the scenes and the MIDI learn right after it.
    FakeNvStore nv;
    Config c;
    c.save(nv);   // the image
    uint8_t rec[5 + 49 + 1] = {'O', 'T', 'X', 6, 49};
    uint8_t* p = rec + 5;
    p[0] = 1;            // cpuMeter
    p[1] = 6;            // synthVoices
    p[6] = 0;            // exportRate (device rate)
    uint8_t* f = p + 27;   // the fork bytes
    f[0] = 1;            // sliceDetector
    f[5] = 1;            // scaleLock
    f[8] = 2;            // reverbModel
    f[17] = 1;           // perfKeys
    uint8_t sum = 0;
    for (int i = 0; i < 49; ++i) sum = uint8_t(sum + p[i]);
    rec[5 + 49] = sum;
    for (size_t i = 0; i < sizeof(rec); ++i) nv.mem[Config::kExtOffset + i] = rec[i];
    Config sc;
    sc.scenes[1][0] = 1;
    sc.scenes[1][1] = 7;
    sc.perfMidi[3][0] = 2;
    sc.perfMidi[3][1] = 21;
    uint8_t scb[Config::kScenesSize], mmb[Config::kMidiMapSize];
    sc.toScenes(scb);
    sc.toMidiMap(mmb);
    const uint32_t at = Config::kExtOffset + sizeof(rec);
    for (uint32_t i = 0; i < Config::kScenesSize; ++i) nv.mem[at + i] = scb[i];
    for (uint32_t i = 0; i < Config::kMidiMapSize; ++i) nv.mem[at + Config::kScenesSize + i] = mmb[i];
    Config r;
    CHECK(r.load(nv));
    CHECK_EQ(r.cpuMeter, 1);
    CHECK_EQ(r.synthVoices, 6);
    CHECK_EQ(r.exportRate, 0);
    CHECK_EQ(r.sliceDetector, 1);
    CHECK_EQ(r.scaleLock, 1);
    CHECK_EQ(r.reverbModel, 2);
    CHECK_EQ(r.perfKeys, 1);
    CHECK_EQ(r.padLayout, 0);   // the v6 fork bytes are not read as 2.2.7's pad / note options
    CHECK_EQ(r.noteNames, 0);
    CHECK_EQ(int(r.scenes[1][1]), 7);
    CHECK_EQ(int(r.perfMidi[3][0]), 2);
    CHECK_EQ(int(r.perfMidi[3][1]), 21);
    // Saved again as v7, then read back the same.
    r.noteNames = 1;
    CHECK(r.save(nv));
    CHECK_EQ(int(nv.mem[Config::kExtOffset + 3]), 7);
    Config r2;
    CHECK(r2.load(nv));
    CHECK_EQ(r2.reverbModel, 2);
    CHECK_EQ(r2.perfKeys, 1);
    CHECK_EQ(r2.noteNames, 1);
    CHECK_EQ(int(r2.scenes[1][1]), 7);
    CHECK_EQ(int(r2.perfMidi[3][1]), 21);
}

// User report (2.2.7-semi.3): stems "not lining up", at 44.1 kHz and with a
// Tempo FX in the song. Clicks on four tracks, the tempo changed twice: every
// stem has the master's length and rate, each click sits where the master has
// it (to a few frames), at both export rates.
namespace {
std::vector<int16_t> wavLeft(const std::vector<uint8_t>& f, uint32_t* rate) {
    WavInfo in;
    std::vector<int16_t> out;
    if (!wavParseHeader(f.data(), f.size(), in) || in.channels != 2 || in.bitsPerSample != 16) return out;
    *rate = in.sampleRate;
    const uint32_t n = in.dataSize / 4;
    out.resize(n);
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t* p = f.data() + in.dataOffset + i * 4;
        out[i] = int16_t(uint16_t(p[0] | (p[1] << 8)));
    }
    return out;
}
// Frames where the level first rises over `thr` after 2000 quiet frames.
std::vector<uint32_t> onsets(const std::vector<int16_t>& s, int thr) {
    std::vector<uint32_t> o;
    uint32_t quiet = 2000;
    for (uint32_t i = 0; i < s.size(); ++i) {
        if (std::abs(int(s[i])) > thr) {
            if (quiet >= 2000) o.push_back(i);
            quiet = 0;
        } else {
            ++quiet;
        }
    }
    return o;
}
}  // namespace

TEST(stems_line_up_with_the_master_through_tempo_changes) {
    for (uint8_t rate : {uint8_t(1), uint8_t(0)}) {
        Harness h;
        App& a = *h.app;
        a.cfg.exportRate = rate;
        std::vector<int16_t> click(400);
        for (int i = 0; i < 400; ++i) click[i] = int16_t(i < 200 ? 20000 : 0);
        addWav(h, "c.wav", click);
        CHECK(a.importSample("c.wav", 0));
        Pattern& p = a.pattern();
        for (auto& tr : p.tracks) tr.lastStep = 63;
        for (int t = 0; t < 4; ++t)
            for (int s = t * 2; s < 64; s += 8) {
                p.tracks[t].steps[s].note = 60;
                p.tracks[t].steps[s].instrument = 0;
            }
        p.tracks[5].steps[16].fx[0].type = uint8_t(FxId::Tempo);
        p.tracks[5].steps[16].fx[0].value = 174;
        p.tracks[5].steps[40].fx[0].type = uint8_t(FxId::Tempo);
        p.tracks[5].steps[40].fx[0].value = 90;
        a.patternEdited();
        h.fs.mkdir("Export");
        CHECK(a.exportAudio(App::ExportKind::Pattern, "Export/st", nullptr, true));
        uint32_t mr = 0;
        const auto master = wavLeft(h.fs.files["Export/st/master.wav"], &mr);
        CHECK(!master.empty());
        CHECK_EQ(mr, rate ? 44100u : 44117u);
        const auto mo = onsets(master, 2000);
        CHECK(mo.size() >= 32);   // (+ the first step again in the tail)
        std::vector<uint32_t> all;
        for (int t = 0; t < 4; ++t) {
            char nm[40];
            std::snprintf(nm, sizeof(nm), "Export/st/track%d.wav", t + 1);
            uint32_t r = 0;
            const auto st = wavLeft(h.fs.files[nm], &r);
            CHECK_EQ(r, mr);
            CHECK_EQ(st.size(), master.size());
            const auto o = onsets(st, 2000);
            CHECK(o.size() >= 8);
            all.insert(all.end(), o.begin(), o.end());
        }
        std::sort(all.begin(), all.end());
        CHECK_EQ(all.size(), mo.size());
        int worst = 0;
        for (size_t i = 0; i < all.size() && i < mo.size(); ++i) {
            const int d = std::abs(int(all[i]) - int(mo[i]));
            worst = d > worst ? d : worst;
        }
        std::printf("  rate %s: %zu frames, worst stem/master onset gap %d frames\n", rate ? "44.1" : "device",
                    master.size(), worst);
        CHECK(worst <= 8);
        for (const char* other : {"reverb.wav", "delay.wav", "synth_1.wav", "synth_2.wav", "synth_3.wav", "track8.wav"}) {
            uint32_t r = 0;
            const auto st = wavLeft(h.fs.files[std::string("Export/st/") + other], &r);
            CHECK_EQ(st.size(), master.size());
        }
    }
}
