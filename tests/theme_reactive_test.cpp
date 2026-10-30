// OT fork music-reactive themes (ui/theme.h): Beat follows the sequencer's
// beat, Reactor the master level, Pump the sidechain (else the beat).
#include "test.h"

#include <set>

#include "ui/theme.h"
#include "ui_fakes.h"

using namespace ot;

namespace {
int indexOf(const char* name) {
    for (int i = 0; i < kThemeTotal; ++i)
        if (std::string(themeName(uint8_t(i))) == name) return i;
    return -1;
}
uint32_t accentAt(int idx, uint8_t beat, uint8_t bar, uint8_t level, uint8_t duck) {
    themeInputs(beat, bar, level, duck);
    themeTick(uint8_t(idx), 0);
    return theme(uint8_t(idx)).accent;
}
}  // namespace

TEST(reactive_themes_follow_their_inputs) {
    const int beat = indexOf("Beat"), reactor = indexOf("Reactor"), pump = indexOf("Pump");
    CHECK(beat >= kThemeCount && reactor > beat && pump > reactor);
    for (int i : {beat, reactor, pump}) {
        CHECK(themeAnimated(uint8_t(i)));
        CHECK(themeReactive(uint8_t(i)));
        CHECK_EQ(themeTickMs(uint8_t(i)), 40u);
    }
    CHECK(!themeReactive(uint8_t(indexOf("Glitch"))));
    // Beat: the accent flashes towards white on the beat, the page on the bar.
    const uint32_t still = accentAt(beat, 0, 0, 0, 0);
    const uint32_t hit = accentAt(beat, 255, 0, 0, 0);
    CHECK(still != hit);
    CHECK_EQ(hit, 0xFFFFFFu);
    themeInputs(0, 0, 0, 0);
    themeTick(uint8_t(beat), 0);
    const uint32_t bg0 = theme(uint8_t(beat)).bg;
    themeInputs(0, 255, 0, 0);
    themeTick(uint8_t(beat), 0);
    CHECK(theme(uint8_t(beat)).bg != bg0);
    // Reactor: louder is hotter (more red in the accent and the page).
    const uint32_t quiet = accentAt(reactor, 0, 0, 0, 0), loud = accentAt(reactor, 0, 0, 200, 0);
    CHECK(((loud >> 16) & 0xFF) > ((quiet >> 16) & 0xFF));
    themeInputs(0, 0, 255, 0);
    themeTick(uint8_t(reactor), 0);
    CHECK(((theme(uint8_t(reactor)).bg >> 16) & 0xFF) > 0x20);
    // Pump: the text sinks into the page while ducked.
    themeInputs(0, 0, 0, 0);
    themeTick(uint8_t(pump), 0);
    const uint32_t textUp = theme(uint8_t(pump)).text;
    themeInputs(0, 0, 0, 255);
    themeTick(uint8_t(pump), 0);
    const uint32_t textDown = theme(uint8_t(pump)).text;
    CHECK(((textDown >> 8) & 0xFF) < ((textUp >> 8) & 0xFF));
}

TEST(beat_theme_moves_while_playing) {
    Harness h;
    App& a = *h.app;
    a.cfg.uiTheme = uint8_t(indexOf("Beat"));
    a.cfg.theme = themeBase(a.cfg.uiTheme);
    std::set<uint32_t> stopped, playing;
    for (int i = 0; i < 30; ++i) {
        h.run(5);
        stopped.insert(a.ui.th().accent);
    }
    a.togglePlay(false);
    for (int i = 0; i < 60; ++i) {
        h.audio.pump(4);
        h.run(5);
        playing.insert(a.ui.th().accent);
    }
    a.togglePlay(false);
    std::printf("  accent colours: stopped %zu, playing %zu\n", stopped.size(), playing.size());
    CHECK_EQ(int(stopped.size()), 1);   // still while nothing plays
    CHECK(playing.size() >= 3);         // flashes on the beats
}

TEST(trigger_theme_lights_the_playing_columns) {
    Harness h;
    App& a = *h.app;
    const int trig = indexOf("Trigger");
    CHECK(trig > indexOf("Pump"));
    CHECK(themeTriggerFlash(uint8_t(trig)));
    CHECK(!themeTriggerFlash(uint8_t(indexOf("Beat"))));
    a.cfg.uiTheme = uint8_t(trig);
    a.cfg.theme = themeBase(a.cfg.uiTheme);
    // Notes on every step of track 3 only.
    Pattern& p = *a.seq.currentPattern();
    for (int s = 0; s < 16; ++s) p.tracks[2].steps[s].note = 48;
    h.run(100);
    const uint32_t accent0 = a.ui.th().accent;
    CHECK_EQ(int(a.ui.trigFlash(2)), 0);
    const uint16_t n0 = a.seq.triggerCount(2);
    a.togglePlay(false);
    int lit2 = 0, lit5 = 0;
    for (int i = 0; i < 80; ++i) {
        h.audio.pump(4);
        h.run(5);
        lit2 += a.ui.trigFlash(2) > 0;
        lit5 += a.ui.trigFlash(5) > 0;
    }
    a.togglePlay(false);
    std::printf("  frames lit: track 3 %d, track 6 %d, notes %d\n", lit2, lit5, int(uint16_t(a.seq.triggerCount(2) - n0)));
    CHECK(uint16_t(a.seq.triggerCount(2) - n0) >= 2);
    CHECK(lit2 > 0);
    CHECK_EQ(lit5, 0);
    CHECK_EQ(a.ui.th().accent, accent0);   // the palette stays still
    // The light fades out once nothing plays.
    h.run(600);
    CHECK_EQ(int(a.ui.trigFlash(2)), 0);
    // A pad preview lights its track too.
    auto total = [&] {
        uint32_t n = 0;
        for (int t = 0; t < 16; ++t) n += a.seq.triggerCount(t);
        return n;
    };
    const uint32_t c = total();
    h.pad(30);
    h.run(45);
    CHECK(total() != c);
}
