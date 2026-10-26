// OT fork: Data screen (DATASCREEN-lite), File F7 "Data" in images without
// Games. An Ikeda / datamatics style view of what the sequencer plays:
//
//   F1 Barcode  a scrolling barcode: one column per step, one lane per track,
//               a bar where a note was played, the bar width = its level
//   F2 Matrix   the pattern around the playhead, 32 steps
//   F3 Numbers  the playing steps as raw data: hex note / instrument / FX
//   F4 Rain     each track's last steps as falling hex bytes
//   F5 Pulse    a circular sequencer: one ring per track, the playhead turning
//   F6          horizontal / vertical (tracks as rows or as columns)
//   F7          colour: Mono (black and white), Theme, Tracks (a hue per track)
//   F8          Play / Stop
//
// Only the tracks that have notes in the pattern are shown, and they share
// the whole screen: two tracks get two big lanes, sixteen get sixteen thin
// ones. Pads 1-16 mute / unmute tracks 1-16 (lit = playing a note now, dim =
// on, dark = muted); the bottom two rows show the next steps. The encoder
// sets the tempo. A module key leaves.
#include "ui/screen_data.h"

#if !OT_GAMES

#include <cmath>
#include <cstdio>
#include <cstring>

#include "app/app.h"

namespace ot {

namespace {
constexpr uint32_t kWhite = 0xFFFFFF, kGrey = 0x5A5A5A, kDim = 0x222222, kBlack = 0x000000;
constexpr int kTop = 30, kBottom = 422, kAreaH = kBottom - kTop;   // under the title, above the bar
constexpr int kMaxRects = 700;                                      // display-list headroom
// A hue per track (Tracks colour mode).
constexpr uint32_t kHues[16] = {0xFF4D4D, 0xFF9F1C, 0xFFE14D, 0x9BE15D, 0x2EC4B6, 0x3DA5FF, 0x8E7DFF, 0xFF5CD1,
                                0xFF7A7A, 0xFFC27A, 0xFFF07A, 0xC2F07A, 0x7AE8DE, 0x7AC4FF, 0xB8ACFF, 0xFF99E3};

uint32_t dimmed(uint32_t c, int num, int den) {
    const uint32_t r = ((c >> 16) & 0xFF) * uint32_t(num) / uint32_t(den);
    const uint32_t gg = ((c >> 8) & 0xFF) * uint32_t(num) / uint32_t(den);
    const uint32_t b = (c & 0xFF) * uint32_t(num) / uint32_t(den);
    return (r << 16) | (gg << 8) | b;
}

const char* const kModeNames[DataScreen::kModeCount] = {"Barcode", "Matrix", "Numbers", "Rain", "Pulse"};
const char* const kColourNames[DataScreen::kColourCount] = {"Mono", "Theme", "Tracks"};
}  // namespace

void openData(App& app) { app.ui.switchTo(kModGames, 0); }

void DataScreen::start(int opt) {
    (void)opt;
    lastStep_ = -1;
    std::memset(rain_, 0xFF, sizeof(rain_));
    refreshBar();
}

void DataScreen::refreshBar() {
    BottomBar& b = ui_.bar;
    b.clear();
    for (int i = 0; i < kModeCount; ++i) {
        b.set(i, kModeNames[i]);
        b.highlight(i, mode == i);
    }
    b.set(5, "View", vertical ? "Vertical" : "Horizontal");
    b.set(6, "Colour", kColourNames[colour]);
    b.set(7, app_.seq.isPlaying() ? "Stop" : "Play");
}

int DataScreen::visibleTracks(uint8_t* out) const {
    const Pattern& p = app_.pattern();
    int n = 0;
    for (int t = 0; t < kTracks; ++t) {
        const Track& tr = p.tracks[t];
        bool any = false;
        for (int s = 0; s < tr.length() && !any; ++s) any = tr.steps[s].note >= 0;
        if (any) out[n++] = uint8_t(t);
    }
    if (n == 0)
        for (int t = 0; t < kTracks; ++t) out[n++] = uint8_t(t);
    return n;
}

uint32_t DataScreen::trackColour(int t, int idx, int count, bool bright) const {
    (void)count;
    switch (colour) {
        case kTheme: {
            const Theme& th = ui_.th();
            return bright ? th.track[idx % 4 == 3 ? 0 : idx % 4] : th.dim;
        }
        case kHue: return bright ? kHues[t & 15] : dimmed(kHues[t & 15], 1, 3);
        default: return bright ? kWhite : kGrey;
    }
}

void DataScreen::update(uint32_t now) {
    App& a = app_;
    // Track levels decay, refreshed from the engine meters (read and reset).
    for (int t = 0; t < kTracks; ++t) {
        const float p = t < 12 ? a.engine.trackPeak(t) : 0.0f;
        level_[t] = p > level_[t] ? p : level_[t] * 0.9f;
    }
    const int st = a.seq.isPlaying() ? a.seq.step(0) : -1;
    if (st != lastStep_) {
        lastStep_ = st;
        if (st >= 0) {
            uint16_t bits = 0;
            uint8_t widths[kTracks];
            rainHead_ = (rainHead_ + 1) % kRainRows;
            for (int t = 0; t < kTracks; ++t) {
                const int s = a.seq.step(t);
                const Step& x = a.pattern().tracks[t].steps[s < 0 ? 0 : (s > 127 ? 127 : s)];
                const bool hit = x.note >= 0;
                bits = uint16_t(bits | (hit ? (1u << t) : 0u));
                int w = int(level_[t] * 9.0f) + (hit ? 2 : 0);
                widths[t] = uint8_t(w > 10 ? 10 : w);
                rain_[rainHead_][t] = hit ? uint8_t(x.note) : 0xFF;
            }
            head_ = (head_ + 1) % kHistory;
            hist_[head_] = bits;
            std::memcpy(width_[head_], widths, sizeof(widths));
        }
        ui_.dirty();
    }
    if (now - lastDraw_ >= 40) {   // the meters and numbers move between steps too
        lastDraw_ = now;
        ui_.dirty();
    }
}

void DataScreen::draw(Gfx& g) {
    App& a = app_;
    const Theme& th = ui_.th();
    g.fill(0, kTop, kScreenW, kAreaH + 1, colour == kTheme ? th.bg : kBlack);
    uint8_t vis[kTracks];
    const int nv = visibleTracks(vis);
    char t[48], r[24];
    std::snprintf(t, sizeof(t), "Data  %s  %d tr", kModeNames[mode], nv);
    std::snprintf(r, sizeof(r), "%s %.1f BPM", a.seq.isPlaying() ? "PLAY" : "STOP", double(a.seq.effectiveTempo()));
    ui_.drawTitle(g, t, r);
    switch (mode) {
        case kMatrix: drawMatrix(g, vis, nv); break;
        case kNumbers: drawNumbers(g, vis, nv); break;
        case kRain: drawRain(g, vis, nv); break;
        case kPulse: drawPulse(g, vis, nv); break;
        default: drawBarcode(g, vis, nv); break;
    }
}

// Newest column at the right (horizontal) or at the top (vertical); one lane
// per visible track over the whole area.
void DataScreen::drawBarcode(Gfx& g, const uint8_t* vis, int nv) {
    const int lanes = nv;
    int cols = kMaxRects / (lanes > 0 ? lanes : 1);
    if (cols > kHistory) cols = kHistory;
    const int along = vertical ? kAreaH : kScreenW - 8;     // time axis length
    const int across = vertical ? kScreenW - 8 : kAreaH;    // lanes axis length
    const int pitch = along / cols;
    const int laneW = across / lanes;
    const int bar = laneW > 8 ? laneW - laneW / 6 : laneW;
    for (int ln = 0; ln < lanes; ++ln) {
        const int tr = vis[ln];
        g.rectsBegin(trackColour(tr, ln, lanes, true));
        for (int c = 0; c < cols; ++c) {
            const int idx = (head_ - c + kHistory) % kHistory;
            if (!(hist_[idx] & (1u << tr))) continue;
            const int w = 1 + width_[idx][tr] * (pitch > 3 ? pitch - 2 : 1) / 10;
            if (vertical) g.rectsAdd(4 + ln * laneW, kTop + 2 + c * pitch, bar, w);
            else g.rectsAdd(kScreenW - 4 - (c + 1) * pitch, kTop + 2 + ln * laneW, w, bar);
        }
        g.rectsEnd();
    }
    // Level lines along the lanes' edge.
    g.rectsBegin(colour == kTheme ? ui_.th().dim : kGrey);
    for (int ln = 0; ln < lanes; ++ln) {
        const int tr = vis[ln];
        const int len = int(level_[tr] * float(laneW - 2)) + 1;
        if (vertical) g.rectsAdd(4 + ln * laneW, kBottom - 3, len, 2);
        else g.rectsAdd(1, kTop + 2 + ln * laneW, 2, len);
    }
    g.rectsEnd();
}

// 32 steps around the playhead, cells filling the area.
void DataScreen::drawMatrix(Gfx& g, const uint8_t* vis, int nv) {
    App& a = app_;
    const int lanes = nv;
    const int steps = 32;
    const int s0 = lastStep_ < 0 ? 0 : (lastStep_ / steps) * steps;
    const int along = vertical ? kAreaH - 4 : kScreenW - 8;
    const int across = vertical ? kScreenW - 8 : kAreaH - 8;
    const int cell = along / steps;
    const int laneW = across / lanes;
    const int ox = 4, oy = kTop + 6;
    // Lane guides.
    g.rectsBegin(colour == kTheme ? ui_.th().grid : kDim);
    for (int ln = 0; ln < lanes; ++ln) {
        if (vertical) g.rectsAdd(ox + ln * laneW + laneW / 2, oy, 1, steps * cell);
        else g.rectsAdd(ox, oy + ln * laneW + laneW / 2, steps * cell, 1);
    }
    g.rectsEnd();
    for (int ln = 0; ln < lanes; ++ln) {
        const int tr = vis[ln];
        const Track& trk = a.pattern().tracks[tr];
        const int cw = cell > 4 ? cell - 2 : cell, lw = laneW > 4 ? laneW - laneW / 5 : laneW;
        g.rectsBegin(trackColour(tr, ln, lanes, true));
        for (int s = 0; s < steps; ++s) {
            const Step& x = trk.steps[(s0 + s) & 127];
            if (x.note < 0) continue;
            if (vertical) g.rectsAdd(ox + ln * laneW + (laneW - lw) / 2, oy + s * cell, lw, cw);
            else g.rectsAdd(ox + s * cell, oy + ln * laneW + (laneW - lw) / 2, cw, lw);
        }
        g.rectsEnd();
        g.rectsBegin(trackColour(tr, ln, lanes, false));
        for (int s = 0; s < steps; ++s) {
            const Step& x = trk.steps[(s0 + s) & 127];
            if (x.note >= 0 || !(x.fx[0].type || x.fx[1].type)) continue;
            if (vertical) g.rectsAdd(ox + ln * laneW + laneW / 2 - 3, oy + s * cell + cell / 2 - 2, 6, 4);
            else g.rectsAdd(ox + s * cell + cell / 2 - 3, oy + ln * laneW + laneW / 2 - 2, 6, 4);
        }
        g.rectsEnd();
    }
    if (lastStep_ >= 0) {
        const int p = lastStep_ - s0;
        const uint32_t c = colour == kTheme ? ui_.th().accent : kWhite;
        if (vertical) g.fill(0, oy + p * cell, 3, cell, c);
        else g.fill(ox + p * cell, kTop + 1, cell, 3, c);
    }
}

void DataScreen::drawNumbers(Gfx& g, const uint8_t* vis, int nv) {
    App& a = app_;
    char t[48];
    const bool big = nv <= 8;
    const int perCol = big ? nv : (nv + 1) / 2;
    const int rowH = (kAreaH - 40) / (perCol > 0 ? perCol : 1);
    for (int ln = 0; ln < nv; ++ln) {
        const int tr = vis[ln];
        const int s = a.seq.isPlaying() ? a.seq.step(tr) : 0;
        const Step& x = a.pattern().tracks[tr].steps[s < 0 ? 0 : (s & 127)];
        std::snprintf(t, sizeof(t), "%02d  %02X %02X  %02X:%02X %02X:%02X  %03d", (tr + 1) & 31, uint8_t(x.note),
                      x.instrument, x.fx[0].type, x.fx[0].value, x.fx[1].type, x.fx[1].value, s & 255);
        const int col = big ? 0 : ln / perCol, row = big ? ln : ln % perCol;
        g.text(16 + col * 400, kTop + 10 + row * rowH + rowH / 2, big ? kFont4 : kFont3,
               trackColour(tr, ln, nv, x.note >= 0), t, IDisplay::kOptCenterY);
    }
    std::snprintf(t, sizeof(t), "%08lX  %06.2f BPM  pos %03d", static_cast<unsigned long>(++frame_),
                  double(a.seq.effectiveTempo()), a.seq.patternPosition() & 255);
    g.text(16, kBottom - 14, kFont2, colour == kTheme ? ui_.th().dim : kGrey, t, IDisplay::kOptCenterY);
}

// The last steps of every visible track as hex bytes, newest first, fading.
void DataScreen::drawRain(Gfx& g, const uint8_t* vis, int nv) {
    int rows = 160 / (nv > 0 ? nv : 1);
    if (rows > kRainRows) rows = kRainRows;
    if (rows < 3) rows = 3;
    const int laneW = (vertical ? kScreenW - 16 : kAreaH - 8) / nv;
    const int rowP = (vertical ? kAreaH - 8 : kScreenW - 16) / rows;
    char b[4];
    for (int ln = 0; ln < nv; ++ln) {
        const int tr = vis[ln];
        for (int k = 0; k < rows; ++k) {
            const uint8_t v = rain_[(rainHead_ - k + kRainRows) % kRainRows][tr];
            if (v == 0xFF) std::snprintf(b, sizeof(b), "--");
            else std::snprintf(b, sizeof(b), "%02X", v);
            const bool fresh = k < 2 && v != 0xFF;
            uint32_t c = trackColour(tr, ln, nv, v != 0xFF);
            if (!fresh && v != 0xFF) c = dimmed(c, rows - k + 2, rows + 2);
            const int x = vertical ? 8 + ln * laneW + laneW / 2 : kScreenW - 8 - k * rowP - rowP / 2;
            const int y = vertical ? kTop + 8 + k * rowP + rowP / 2 : kTop + 4 + ln * laneW + laneW / 2;
            g.text(x, y, fresh ? kFont4 : kFont2, c, b, IDisplay::kOptCenter);
        }
    }
}

// A ring per visible track (outer = first), its hits as squares, the step
// that plays as a big one; a spoke marks the first track's playhead.
void DataScreen::drawPulse(Gfx& g, const uint8_t* vis, int nv) {
    App& a = app_;
    const int cx = kScreenW / 2, cy = kTop + kAreaH / 2;
    const float rOut = float(kAreaH) / 2.0f - 8.0f, rIn = 26.0f;
    int budget = 520;   // squares (batched rects: two words each)
    const int dot = nv > 8 ? 4 : 7;
    for (int ln = 0; ln < nv && budget > 0; ++ln) {
        const int tr = vis[ln];
        const Track& trk = a.pattern().tracks[tr];
        const int len = trk.length() > 0 ? trk.length() : 16;
        const float rad = nv > 1 ? rOut - (rOut - rIn) * float(ln) / float(nv - 1) : rOut;
        for (int pass = 0; pass < 2; ++pass) {   // empty steps (small, dim), then hits
            if (pass == 0 && nv > 6) continue;    // many rings: hits only
            g.rectsBegin(trackColour(tr, ln, nv, pass == 1));
            for (int s = 0; s < len && budget > 0; ++s) {
                const bool hit = trk.steps[s].note >= 0;
                if (hit != (pass == 1)) continue;
                const float ang = float(s) / float(len) * 6.2831853f - 1.5707963f;
                const int d = pass == 1 ? dot : 2;
                g.rectsAdd(cx + int(std::cos(ang) * rad) - d / 2, cy + int(std::sin(ang) * rad) - d / 2, d, d);
                --budget;
            }
            g.rectsEnd();
        }
    }
    if (a.seq.isPlaying()) {
        g.rectsBegin(colour == kTheme ? ui_.th().accent : kWhite);
        for (int ln = 0; ln < nv; ++ln) {
            const int tr = vis[ln];
            const Track& trk = a.pattern().tracks[tr];
            const int len = trk.length() > 0 ? trk.length() : 16;
            const float rad = nv > 1 ? rOut - (rOut - rIn) * float(ln) / float(nv - 1) : rOut;
            const float ang = float(a.seq.step(tr)) / float(len) * 6.2831853f - 1.5707963f;
            const int d = dot * 2;
            g.rectsAdd(cx + int(std::cos(ang) * rad) - d / 2, cy + int(std::sin(ang) * rad) - d / 2, d, d);
        }
        g.rectsEnd();
        const Track& t0 = a.pattern().tracks[vis[0]];
        const int len = t0.length() > 0 ? t0.length() : 16;
        const float ang = float(a.seq.step(vis[0])) / float(len) * 6.2831853f - 1.5707963f;
        g.line(cx, cy, cx + int(std::cos(ang) * rOut), cy + int(std::sin(ang) * rOut),
               colour == kTheme ? ui_.th().dim : kGrey);
    }
}

void DataScreen::onButton(uint8_t id, uint8_t state) {
    if (state != kPress) return;
    if (id <= btn::F5) {
        mode = id;
    } else if (id == btn::F6) {
        vertical = !vertical;
    } else if (id == btn::F7) {
        colour = (colour + 1) % kColourCount;
    } else {
        Screen::onButton(id == btn::F8 ? uint8_t(btn::Play) : id, state);   // F8 = Play, as the label says
    }
    refreshBar();
    ui_.dirty();
}

void DataScreen::onEncoder(int d) {
    App& a = app_;
    if (a.cfg.clockIn) return;   // external clock: the tempo is not ours
    float t = float(int(a.project.tempo() + 0.5f)) + float(d);
    t = t < 10.0f ? 10.0f : (t > 400.0f ? 400.0f : t);
    a.project.setTempo(t);
    a.markProjectDirty();
    ui_.dirty();
}

void DataScreen::onPad(uint8_t pad, uint8_t state) {
    if (state != kPress || pad >= kTracks) return;
    app_.setTrackMute(pad, !app_.trackMuted(pad));
    ui_.dirty();
}

void DataScreen::padLeds(uint8_t* lv) {
    // Pads 1-16: the tracks (lit: a note now, dim: on, dark: muted). Rows
    // 3-4: the next 12 steps of the first and second half of the shown tracks.
    App& a = app_;
    const uint8_t full = a.cfg.padFull(), dim = a.cfg.padDim();
    for (int p = 0; p < 48; ++p) lv[p] = 0;
    for (int t = 0; t < kTracks; ++t) {
        if (a.trackMuted(t)) continue;
        const int s = a.seq.isPlaying() ? a.seq.step(t) : -1;
        const bool hit = s >= 0 && a.pattern().tracks[t].steps[s & 127].note >= 0;
        lv[t] = hit ? full : dim;
    }
    uint8_t vis[kTracks];
    const int nv = visibleTracks(vis);
    const int base = lastStep_ < 0 ? 0 : lastStep_;
    const int half = (nv + 1) / 2;
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 12; ++col) {
            bool hit = false;
            for (int k = row * half; k < (row + 1) * half && k < nv; ++k) {
                const Track& trk = a.pattern().tracks[vis[k]];
                hit = hit || trk.steps[(base + col) % trk.length()].note >= 0;
            }
            lv[24 + row * 12 + col] = hit ? (col == 0 ? full : uint8_t(full / 2)) : 0;
        }
}

}  // namespace ot

#endif  // !OT_GAMES
