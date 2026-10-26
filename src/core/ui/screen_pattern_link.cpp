// Pattern screen: Link popup (OT fork track links, sequencer/track_link.h).
//
// Opened from Pattern page 3 (More > More) "Link" (F4). The cursor track
// becomes a live layer of another track: whenever the source track triggers
// a note (its steps, rolls, arps, play modes, generators' live changes, live
// pads), this track plays its own instrument at the same moment. Laid out
// like the Generate popup: F1 Source, F2 Instrument, F3 Note (Follow or a
// fixed note), F4 Transpose, F5 Level, F6 Chance, F7 FX; the right column
// lists every link of the project. Changes apply at once; F8, Pattern or Rec
// close it. Pads set the selected field, the encoder / Up / Down change it.
#include <cstdio>
#include <cstring>

#include "app/app.h"
#include "app/pattern_ops.h"
#include "ui/notes.h"
#include "ui/screens.h"

namespace ot {

namespace {
int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
constexpr int kFields = 7;
const char* const kLinkNames[kFields] = {"Source", "Instrument", "Note", "Transpose", "Level", "Chance", "FX"};
const char* const kLinkHelp[kFields] = {
    "The track this one follows: it plays whenever the source plays a note",
    "The instrument this track plays with the source's notes",
    "Follow: the source's note; or a fixed note (drum layers)",
    "Semitones added to the note",
    "Volume: % of the source note's volume",
    "% of the source's notes this track plays",
    "Copy: the source step's FX come along (slices, volume, ...)",
};
const char* const kFxNames[2] = {"None", "Copy"};

// Instrument list of a track: tracks 1..8 all 67, tracks 9..16 synths and MIDI.
int instrFirst(int track) { return track < 8 ? 0 : kFirstMidiInstrument; }
int instrCount(int track) { return 67 - instrFirst(track); }
}  // namespace

void PatternScreen::linkOpenPopup() {
    linkOpen = true;
    linkField = 0;
}

int PatternScreen::linkGet(int f) const {
    const TrackLink& l = app_.fork.link[track];
    switch (f) {
        case 0: return l.source;
        case 1: return l.instr - instrFirst(track);
        case 2: return l.note + 1;   // 0 Follow
        case 3: return l.transpose;
        case 4: return l.level;
        case 5: return l.chance;
        default: return l.fx;
    }
}

void PatternScreen::linkRange(int f, int* lo, int* hi) const {
    *lo = 0;
    switch (f) {
        case 0: *hi = kTrackCount; break;
        case 1: *hi = instrCount(track) - 1; break;
        case 2: *hi = 128; break;
        case 3: *lo = -24; *hi = 24; break;
        case 4: case 5: *hi = 100; break;
        default: *hi = 1; break;
    }
}

void PatternScreen::linkSet(int f, int v, bool wrap) {
    App& a = app_;
    TrackLink& l = a.fork.link[track];
    int lo, hi;
    linkRange(f, &lo, &hi);
    v = wrap && v > hi ? lo : clampi(v, lo, hi);
    switch (f) {
        case 0: {
            if (v == track + 1) {   // not itself: skip over it
                const int d = v > l.source ? 1 : -1;
                v += d;
                if (v > hi) v = wrap ? 0 : l.source;
                if (v < 0) v = 0;
            }
            if (v && !l.source) {
                // A new link: the track's own instrument (its first note in
                // the pattern), else the current instrument.
                int ins = -1;
                const Track& tr = a.pattern().tracks[track];
                for (int s = 0; s <= tr.lastStep && ins < 0; ++s)
                    if (tr.steps[s].note >= 0) ins = tr.steps[s].instrument;
                if (ins < 0) ins = a.currentInstrument();
                ins = pops::mapInstrument(track, ins);
                l.instr = uint8_t(clampi(ins, instrFirst(track), 66));
            }
            l.source = uint8_t(v);
            break;
        }
        case 1: l.instr = uint8_t(v + instrFirst(track)); break;
        case 2: l.note = int8_t(v - 1); break;
        case 3: l.transpose = int8_t(v); break;
        case 4: l.level = uint8_t(v); break;
        case 5: l.chance = uint8_t(v); break;
        default: l.fx = uint8_t(v); break;
    }
    a.markForkDirty();
    ui_.dirty();
}

void PatternScreen::linkDelta(int d, bool wrap) {
    if (d > 5) d = 5;
    if (d < -5) d = -5;
    linkSet(linkField, linkGet(linkField) + d, wrap);
}

void PatternScreen::linkValueText(int f, char* out, int cap) const {
    const App& a = app_;
    const TrackLink& l = a.fork.link[track];
    switch (f) {
        case 0:
            if (l.source) std::snprintf(out, size_t(cap), "Track %d", l.source);
            else std::snprintf(out, size_t(cap), "Off");
            break;
        case 1: linkItemText(1, linkGet(1), out, cap); break;
        case 2: linkItemText(2, linkGet(2), out, cap); break;
        case 3: std::snprintf(out, size_t(cap), "%+d", l.transpose); break;
        case 4: std::snprintf(out, size_t(cap), "%d%%", l.level); break;
        case 5: std::snprintf(out, size_t(cap), "%d%%", l.chance); break;
        default: std::snprintf(out, size_t(cap), "%s", kFxNames[l.fx & 1]); break;
    }
}

// List item i of field f (Source, Instrument, Note, FX).
void PatternScreen::linkItemText(int f, int i, char* out, int cap) const {
    const App& a = app_;
    if (f == 0) {
        if (i == 0) std::snprintf(out, size_t(cap), "Off");
        else if (i == track + 1) std::snprintf(out, size_t(cap), "(this)");
        else std::snprintf(out, size_t(cap), "Track %d", i);
    } else if (f == 1) {
        const int ins = i + instrFirst(track);
        if (ins < kSampleInstrumentCount)
            std::snprintf(out, size_t(cap), "%02d %s", ins + 1, a.instrPresent[ins] ? a.instruments[ins].name : "");
        else if (isSynthInstrument(uint8_t(ins))) std::snprintf(out, size_t(cap), "Synth %d", synthSlotOf(uint8_t(ins)) + 1);
        else std::snprintf(out, size_t(cap), "MIDI %d", ins - kFirstMidiInstrument + 1);
    } else if (f == 2) {
        if (i == 0) {
            std::snprintf(out, size_t(cap), "Follow");
        } else {
            char n[4];
            noteName(int8_t(i - 1), n);
            std::snprintf(out, size_t(cap), "%s", n);
        }
    } else {
        std::snprintf(out, size_t(cap), "%s", kFxNames[i & 1]);
    }
}

void PatternScreen::linkButton(uint8_t id) {
    if (id <= btn::F7) {
        const int f = int(id);
        if (linkField == f) linkDelta(1, true);   // pressing the selected field again steps it
        linkField = f;
    } else if (id == btn::Pattern || id == btn::Rec || id == btn::F8) {
        linkOpen = false;
    } else if (id == btn::Right || id == btn::Left) {
        linkField = clampi(linkField + (id == btn::Right ? 1 : -1), 0, kFields - 1);
    } else if (id == btn::Up || id == btn::Down) {
        // Lists step down with Down; numbers count up with Up.
        const bool list = linkField <= 2 || linkField == 6;
        linkDelta((id == btn::Down) == list ? 1 : -1);
    } else if (id == btn::Delete) {
        if (linkField == 0) linkSet(0, 0);
    }
}

void PatternScreen::linkPad(uint8_t pad) {
    int lo, hi;
    linkRange(linkField, &lo, &hi);
    const int span = hi - lo;
    const int lp = pad;
    // Lists: pad n = item n. Numbers: the 48 pads spread over the range.
    const bool list = linkField <= 2 || linkField == 6;
    const int v = list || span <= 47 ? lo + lp : lo + (lp * span + 23) / 47;
    if (linkField == 0 && v == track + 1) return;
    linkSet(linkField, v);
}

void PatternScreen::linkBar(BottomBar& b) {
    char v[24];
    for (int f = 0; f < kFields; ++f) {
        linkValueText(f, v, sizeof(v));
        b.set(f, kLinkNames[f], v);
    }
    b.set(7, "Close");
}

void PatternScreen::drawLink(Gfx& g) {
    const Theme& th = ui_.th();
    const App& a = app_;
    constexpr int kTop = 58, kListH = 343, kBarH = 342, kHelpY = kTop + kListH + 2, kHelpH = 20;
    const TrackLink& l = a.fork.link[track];
    char title[48];
    std::snprintf(title, sizeof(title), "Link Track %d", track + 1);
    g.fill(0, wg::kMainTop, kScreenW, kTop - wg::kMainTop, th.slot(slot::kTitle + 1));
    g.text(10, wg::kMainTop + 12, kFont4, th.slot(slot::kTitle), title, IDisplay::kOptCenterY);
    g.text(790, wg::kMainTop + 12, kFont3, th.slot(slot::kTitle),
           l.source ? "Plays live with the source track's notes" : "Pick a source track (F1)",
           IDisplay::kOptCenterY | IDisplay::kOptRightX);
    g.fill(0, kHelpY, kScreenW, kHelpH, th.panel2);
    g.text(10, kHelpY + kHelpH / 2, kFont3, th.text, kLinkHelp[clampi(linkField, 0, kFields - 1)], IDisplay::kOptCenterY);
    struct Ctx {
        const PatternScreen* s;
        int field;
    };
    auto item = [](void* ctx, int i, char* out, int cap) {
        const Ctx* c = static_cast<const Ctx*>(ctx);
        c->s->linkItemText(c->field, i, out, cap);
    };
    for (int f = 0; f < kFields; ++f) {
        const int x = wg::columnX(f);
        const bool off = f > 0 && !l.source;
        int lo, hi;
        linkRange(f, &lo, &hi);
        const int v = linkGet(f);
        if (f <= 2 || f == 6) {
            Ctx ctx{this, f};
            wg::ListView lv;
            lv.x = x;
            lv.y = kTop;
            lv.w = wg::columnW();
            lv.h = kListH;
            lv.count = hi - lo + 1;
            lv.sel = v - lo;
            lv.top = wg::listFollow(lv.sel, lv.count, wg::listVisibleRows(lv.h), 0);
            lv.selStyle = wg::ListSel::kWhite;
            lv.item = item;
            lv.ctx = &ctx;
            wg::list(g, th, lv);
        } else {
            const float span = float(hi - lo);
            wg::ValueBarStyle st;
            st.bipolar = lo < 0;
            float fr = span > 0 ? float(v - lo) / span : 0;
            if (st.bipolar) fr = fr * 2.0f - 1.0f;
            wg::valueBar(g, th, x, kTop, wg::columnW(), kBarH, fr, st);
        }
        if (off) {   // no source yet: the settings are dimmed
            g.alpha(150);
            g.fill(x, kTop, wg::columnW(), kListH, th.panelDark);
            g.alpha(255);
        }
    }
    // Right column: every link of the project ("5 < 1": track 5 follows track 1).
    const int cx = wg::columnX(7), cw = wg::columnW();
    g.fill(cx, kTop, cw, kListH, th.panelDark);
    g.text(cx + 6, kTop + 12, kFont2, th.dim, "Links", IDisplay::kOptCenterY);
    int yy = kTop + 34, n = 0;
    for (int t = 0; t < kTrackCount; ++t) {
        const TrackLink& k = a.fork.link[t];
        if (!k.source) continue;
        char line[24];
        std::snprintf(line, sizeof(line), "%2d < %d", t + 1, k.source);
        g.text(cx + 8, yy, kFont3, t == track ? th.accent : th.text, line, IDisplay::kOptCenterY);
        yy += 20;
        if (++n >= 15) break;
    }
    if (!n) g.text(cx + 8, yy, kFont2, th.dim, "none", IDisplay::kOptCenterY);
    wg::focusFrame(g, th, wg::columnX(linkField), kTop, wg::columnX(linkField) + wg::columnW(), kTop + kListH);
}

}  // namespace ot
