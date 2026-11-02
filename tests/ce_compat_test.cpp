// OT fork compatibility with Polyend Tracker CE 1.0 (polyend/tracker-ce):
// structure-6 pattern files (8-byte steps with volume and micro-move) load,
// and the Chance conditions (values 101..174) play as CE plays them.
#include <cstring>
#include <vector>

#include "test.h"
#include "ui_fakes.h"

#include "app/project_io.h"
#include "formats/pattern_mtp.h"
#include "ui/fx_text.h"

using namespace ot;

namespace {
// A CE 1.0 file of `tracks` tracks: header (structure 6), every step empty
// (note -1, volume -1, micro-move -1), tracks 32 steps long.
std::vector<uint8_t> ceFile(int tracks) {
    const uint32_t size = mtp::v6FileSizeFor(tracks);
    std::vector<uint8_t> f(size, 0);
    f[0] = 'K';
    f[1] = 'S';
    f[2] = 2;
    f[4] = 1;   // fw 1.0.0
    for (int i = 8; i < 12; ++i) f[size_t(i)] = 6;
    f[0x0C] = uint8_t(size);
    f[0x0D] = uint8_t(size >> 8);
    for (int t = 0; t < tracks; ++t) {
        uint8_t* tr = f.data() + mtp::kTracksOff + uint32_t(t) * mtp::kV6TrackSize;
        tr[0] = 31;
        for (int s = 0; s < 128; ++s) {
            uint8_t* st = tr + 1 + s * 8;
            st[0] = 0xFF;   // empty note
            st[6] = 0xFF;   // volume -1
            st[7] = 0xFF;   // micro-move -1
        }
    }
    return f;
}
uint8_t* stepAt(std::vector<uint8_t>& f, int t, int s) {
    return f.data() + mtp::kTracksOff + uint32_t(t) * mtp::kV6TrackSize + 1 + uint32_t(s) * 8;
}
}  // namespace

TEST(ce_v6_pattern_loads) {
    for (int tracks : {16, 12}) {
        std::vector<uint8_t> f = ceFile(tracks);
        // Track 1 step 0: a note with Roll on disk FX slot 0 and volume 70.
        uint8_t* a = stepAt(f, 0, 0);
        a[0] = 48;
        a[1] = 3;
        a[2] = 3;   // Roll
        a[3] = 9;
        a[6] = 70;
        // Track 3 step 5: both FX slots used, a volume: no room for it.
        uint8_t* b = stepAt(f, 2, 5);
        b[0] = 50;
        b[2] = 18;
        b[3] = 40;
        b[4] = 19;
        b[5] = 30;
        b[6] = 99;
        // Last track step 127: a CE-only FX (z = 45) and a micro-move 25.
        uint8_t* c = stepAt(f, tracks - 1, 127);
        c[0] = 60;
        c[2] = 45;
        c[3] = 50;
        c[7] = 25;
        V6Info vi;
        const uint32_t len = normalizeV6Pattern(f.data(), uint32_t(f.size()), &vi);
        CHECK(vi.converted);
        CHECK_EQ(len, mtp::fileSizeFor(tracks));
        CHECK_EQ(int(vi.movedVm), 2);
        CHECK_EQ(int(vi.droppedVm), 1);
        CHECK_EQ(int(vi.droppedFx), 1);
        Track out[kTrackCount];
        PatternLoadInfo li;
        decodePattern16(f.data(), len, out, nullptr, &li);
        CHECK(!li.upgraded);
        CHECK_EQ(int(out[0].lastStep), 31);
        CHECK_EQ(int(out[0].steps[0].note), 48);
        CHECK_EQ(int(out[0].steps[0].instrument), 3);
        // Disk slot 0 = model fx[1]; the volume went to the free slot.
        CHECK_EQ(int(out[0].steps[0].fx[1].type), 3);
        CHECK_EQ(int(out[0].steps[0].fx[1].value), 9);
        CHECK_EQ(int(out[0].steps[0].fx[0].type), 18);
        CHECK_EQ(int(out[0].steps[0].fx[0].value), 70);
        CHECK_EQ(int(out[2].steps[5].fx[1].type), 18);
        CHECK_EQ(int(out[2].steps[5].fx[0].type), 19);
        const Step& z = out[tracks - 1].steps[127];
        CHECK_EQ(int(z.note), 60);
        CHECK_EQ(int(z.fx[1].type), 2);   // micro-move took the slot of the dropped z
        CHECK_EQ(int(z.fx[1].value), 25);
        CHECK_EQ(int(z.fx[0].type), 0);
        // Other steps stay empty, nothing reset.
        CHECK_EQ(int(out[1].steps[3].note), int(kNoteEmpty));
        CHECK_EQ(int(out[tracks - 1].lastStep), 31);
        if (tracks == 12) CHECK_EQ(int(li.firstResetTrack), 12);
    }
    // Any other file is left alone.
    std::vector<uint8_t> v5(mtp::kMiniFileSize, 0);
    v5[8] = 5;
    v5[0x0C] = uint8_t(mtp::kMiniFileSize);
    v5[0x0D] = uint8_t(mtp::kMiniFileSize >> 8);
    V6Info none;
    CHECK_EQ(normalizeV6Pattern(v5.data(), uint32_t(v5.size()), &none), mtp::kMiniFileSize);
    CHECK(!none.converted);
}

TEST(ce_v6_project_opens_from_the_card) {
    Harness h;
    App& a = *h.app;
    // A project saved here, then its pattern 1 replaced by a CE 1.0 file
    // (as if saved by the Community Edition on the same card).
    CHECK(saveProjectInto(a, "Projects", "ce"));
    std::vector<uint8_t> f = ceFile(16);
    uint8_t* st = stepAt(f, 0, 0);
    st[0] = 52;
    st[6] = 55;
    h.fs.files["Projects/ce/patterns/pattern_01.mtp"] = f;
    CHECK(a.openProject("Projects", "ce"));
    const Step& s = a.pattern().tracks[0].steps[0];
    CHECK_EQ(int(s.note), 52);
    CHECK((s.fx[0].type == 18 && s.fx[0].value == 55) || (s.fx[1].type == 18 && s.fx[1].value == 55));
    CHECK_EQ(int(a.pattern().tracks[0].lastStep), 31);
}

TEST(ce_chance_conditions) {
    char t[8];
    fxValueText(4, 101, t);
    CHECK_STR(t, " GL");
    fxValueText(4, 102, t);
    CHECK_STR(t, " !L");
    fxValueText(4, 103, t);
    CHECK_STR(t, "1:1");
    fxValueText(4, 138, t);
    CHECK_STR(t, "6:6");
    fxValueText(4, 139, t);
    CHECK_STR(t, "S11");
    fxValueText(4, 50, t);
    CHECK_STR(t, "50");
    // Play 2 / skip 1 (103 + 1 * 6 + 0 = 109) on a one-step-per-loop track:
    // passes 0, 1 play, 2 skips, 3, 4 play, 5 skips.
    Harness h;
    App& a = *h.app;
    Pattern& p = a.pattern();
    for (auto& tr : p.tracks) tr.lastStep = 3;
    Step& s = p.tracks[0].steps[0];
    s.note = 48;
    s.fx[0].type = 4;
    s.fx[0].value = 109;
    // Skip 1 / play 2 (139 + 0 * 6 + 1 = 140) on track 2.
    Step& s2 = p.tracks[1].steps[0];
    s2.note = 48;
    s2.fx[0].type = 4;
    s2.fx[0].value = 140;
    // GL on track 3 step 2 follows its own step 0 (Chance 0: never).
    Step& g0 = p.tracks[2].steps[0];
    g0.note = 48;
    g0.fx[0].type = 4;
    g0.fx[0].value = 0;
    Step& g1 = p.tracks[2].steps[2];
    g1.note = 50;
    g1.fx[0].type = 4;
    g1.fx[0].value = 101;
    const uint16_t c0 = a.seq.triggerCount(0), c1 = a.seq.triggerCount(1), c2 = a.seq.triggerCount(2);
    a.togglePlay(false);
    // Six passes of four steps (the master position counts the steps).
    const int start = a.seq.masterStep();
    for (int guard = 0; guard < 20000 && a.seq.masterStep() - start < 24; ++guard) {
        h.audio.pump(1);
        h.run(1);
    }
    a.togglePlay(false);
    const int n0 = int(uint16_t(a.seq.triggerCount(0) - c0));
    const int n1 = int(uint16_t(a.seq.triggerCount(1) - c1));
    const int n2 = int(uint16_t(a.seq.triggerCount(2) - c2));
    std::printf("  6 passes: 2:1 played %d, S12 played %d, GL after never played %d\n", n0, n1, n2);
    CHECK_EQ(n0, 4);
    CHECK_EQ(n1, 4);
    CHECK_EQ(n2, 0);
}

// The EEPROM image: stock header and block sizes (so stock / CE read every
// block), and a Community Edition image (interface block 2 bytes longer)
// read by its block sizes.
#include "app/config.h"
#include "nvstore_fake.h"

TEST(ce_config_image_layout) {
    FakeNvStore nv;
    Config c;
    c.clockIn = 2;
    c.metronome = 1;
    c.tsNum = 6;
    c.sliceCount = 16;
    CHECK(c.save(nv));
    uint8_t img[Config::kImageSize];
    CHECK(nv.read(0, img, sizeof img));
    CHECK_EQ(int(img[7]), 10);
    CHECK_EQ(int(img[8] | (img[9] << 8)), int(Config::kImageSize));
    const int at[9] = {0x0C, 0x1C, 0x24, 0x32, 0xC8, 0xDA, 0xE4, 0xEC, 0xF0};
    int off = 0x0C;
    for (int b = 0; b < 9; ++b) {   // walking the sizes lands on every block
        CHECK_EQ(off, at[b]);
        off += img[off] | (img[off + 1] << 8);
    }
    CHECK_EQ(off, int(Config::kImageSize));
    // A CE image: the same blocks, the interface block 12 bytes, so the
    // metronome / debug / common blocks sit 2 bytes later; total 0xF8.
    uint8_t ce[Config::kImageSize + 64] = {};
    std::memcpy(ce, img, 0xDA);
    ce[8] = 0xF8;
    ce[0xDA] = 12;
    ce[0xDA + 2] = img[0xDA + 2];   // fxPopupDescription
    std::memcpy(ce + 0xE6, img + 0xE4, 8);   // metronome block
    ce[0xE6] = 8;
    std::memcpy(ce + 0xEE, img + 0xEC, 4);
    std::memcpy(ce + 0xF2, img + 0xF0, 4);
    uint8_t norm[Config::kImageSize];
    CHECK_EQ(Config::normalizeImage(ce, sizeof ce, norm), 2u);
    Config r;
    CHECK(r.fromImage(norm));
    CHECK_EQ(int(r.clockIn), 2);
    CHECK_EQ(int(r.metronome), 1);
    CHECK_EQ(int(r.tsNum), 6);
    CHECK_EQ(int(r.sliceCount), 16);
    // Our own image is taken as it is; an older OpenTracker image (0xF4 in
    // the first block size) is read at the fixed offsets.
    CHECK_EQ(Config::normalizeImage(img, sizeof img, norm), 1u);
    img[0x0C] = 0xF4;
    CHECK_EQ(Config::normalizeImage(img, sizeof img, norm), 0u);
    Config o;
    CHECK(o.fromImage(norm));
    CHECK_EQ(int(o.tsNum), 6);
}

// Data-safety fixes from the tracker-ce comparison.
#include "formats/legacy_upgrade.h"
#include "model/instrument.h"

TEST(ce_save_cleans_every_stale_instrument) {
    Harness h;
    App& a = *h.app;
    CHECK(saveProjectInto(a, "Projects", "many"));
    // 90 stale instrument files and 30 other files in instruments/.
    for (int i = 0; i < 90; ++i) {
        char n[64];
        std::snprintf(n, sizeof n, "Projects/many/instruments/%02d old %d.pti", 1 + i % 48, i);
        h.fs.files[n] = {1, 2, 3};
    }
    for (int i = 0; i < 30; ++i) {
        char n[64];
        std::snprintf(n, sizeof n, "Projects/many/instruments/notes %d.txt", i);
        h.fs.files[n] = {1};
    }
    CHECK(saveProjectInto(a, "Projects", "many"));
    int stale = 0, other = 0;
    for (const auto& kv : h.fs.files) {
        if (kv.first.rfind("Projects/many/instruments/", 0) != 0) continue;
        if (kv.first.find(" old ") != std::string::npos) ++stale;
        if (kv.first.find("notes ") != std::string::npos) ++other;
    }
    CHECK_EQ(stale, 0);
    CHECK_EQ(other, 30);   // other files are kept (documented)
}

TEST(ce_wavetable_refresh_after_import) {
    Instrument in;
    in.playMode = 6;
    in.wavetableWindowSize = 2048;
    in.wavetableWindowCount = 16;
    in.wavetableCurrentWindow = 15;
    legacy::applyImportedLength(in, 3000, false);   // 3000 frames: window 2048 -> fits once
    CHECK_EQ(int(in.wavetableWindowSize), 2048);
    CHECK_EQ(int(in.wavetableWindowCount), 1);
    CHECK_EQ(int(in.wavetableCurrentWindow), 0);
    in.wavetableWindowSize = 2048;
    in.wavetableWindowCount = 4;
    in.wavetableCurrentWindow = 3;
    legacy::applyImportedLength(in, 1500, false);   // shorter than a window: 1024 (power of two)
    CHECK_EQ(int(in.wavetableWindowSize), 1024);
    CHECK_EQ(int(in.wavetableWindowCount), 1);
    in.wavetableWindowSize = 256;
    in.wavetableWindowCount = 8;
    in.wavetableCurrentWindow = 7;
    legacy::applyImportedLength(in, 256 * 32, false);
    CHECK_EQ(int(in.wavetableWindowCount), 32);
    CHECK_EQ(int(in.wavetableCurrentWindow), 31);   // the last window stays the last
}

// Community Edition themes (/Themes/*.json from Polyend's web theme editor).
#include "ui/theme.h"
#include "ui/theme_file.h"

TEST(ce_json_theme_loads) {
    Harness h;
    const char* js =
        "{\n  \"name\": \"Minimal Blue\",\n  \"background\": \"#0c1017\",\n"
        "  \"patternTracker\": {\n    \"background\": \"#0e131d\",\n    \"note\": \"#58a6ff\",\n"
        "    \"instrument\": \"#e3b341\"\n  },\n  \"accents\": { \"red\": \"#f85149\", \"green\": \"#2ea043\" },\n"
        "  \"base\": \"Monochrome\"\n}\n";
    h.fs.mkdir("Themes");
    h.fs.files["Themes/Minimal Blue.json"] = std::vector<uint8_t>(js, js + std::strlen(js));
    h.fs.files["Themes/._Minimal Blue.json"] = {0, 5, 22};
    char names[8][kThemeNameMax + 1];
    CHECK_EQ(listThemeFiles(h.fs, names, 8), 1);
    CHECK_STR(names[0], "Minimal Blue");
    uint32_t slots[kThemeSlotCount];
    ThemeParse pr;
    CHECK(loadThemeFile(h.fs, "Minimal Blue", slots, &pr));
    CHECK_EQ(int(pr.base), 1);   // the base, though it comes last in the file
    const Theme t = themeView("Minimal Blue", slots);
    CHECK_EQ(t.bg & 0xFFFFFFu, 0x0C1017u);
    CHECK_EQ(t.track[0] & 0xFFFFFFu, 0x58A6FFu);
    CHECK_EQ(t.track[1] & 0xFFFFFFu, 0xE3B341u);
    CHECK_EQ(t.accent & 0xFFFFFFu, 0xF85149u);
    CHECK_EQ(t.good & 0xFFFFFFu, 0x2EA043u);
    // A .txt of the same name wins and is listed once.
    const char* tx = "base = original\nbackground = #112233\n";
    h.fs.files["Themes/Minimal Blue.txt"] = std::vector<uint8_t>(tx, tx + std::strlen(tx));
    CHECK_EQ(listThemeFiles(h.fs, names, 8), 1);
    CHECK(loadThemeFile(h.fs, "Minimal Blue", slots, &pr));
    CHECK_EQ(themeView("x", slots).bg & 0xFFFFFFu, 0x112233u);
}
