// OT fork: Data screen (DATASCREEN-lite) in the Games slot of images built
// without Games (OT_GAMES=0): File F7 "Data". See screen_data.cpp.
#pragma once

#include "ui/screen_games.h"   // OT_GAMES
#include "ui/ui.h"

#if !OT_GAMES
#include <cstdint>

namespace ot {

void openData(App& app);

class DataScreen final : public Screen {
public:
    using Screen::Screen;
    const char* name() const override { return "Data"; }
    void start(int opt) override;
    void update(uint32_t now) override;
    void draw(Gfx& g) override;
    void onButton(uint8_t id, uint8_t state) override;
    void onEncoder(int d) override;
    void onPad(uint8_t pad, uint8_t state) override;
    void padLeds(uint8_t* levels) override;
    enum Mode { kBarcode, kMatrix, kNumbers, kRain, kPulse, kModeCount };
    enum Colour { kMono, kTheme, kHue, kColourCount };
    int mode = kBarcode;
    bool vertical = false;   // tracks as columns, time running down
    int colour = kMono;

private:
    static constexpr int kTracks = 16, kHistory = 64, kRainRows = 14;
    void refreshBar();
    int visibleTracks(uint8_t* out) const;   // tracks with notes (all when none)
    uint32_t trackColour(int t, int idx, int count, bool bright) const;
    void drawBarcode(Gfx& g, const uint8_t* vis, int nv);
    void drawMatrix(Gfx& g, const uint8_t* vis, int nv);
    void drawNumbers(Gfx& g, const uint8_t* vis, int nv);
    void drawRain(Gfx& g, const uint8_t* vis, int nv);
    void drawPulse(Gfx& g, const uint8_t* vis, int nv);
    uint16_t hist_[kHistory] = {};
    uint8_t width_[kHistory][kTracks] = {};
    uint8_t rain_[kRainRows][kTracks] = {};   // last steps' note bytes per track (0xFF = none)
    int head_ = 0;
    int rainHead_ = 0;
    int lastStep_ = -1;
    float level_[kTracks] = {};
    uint32_t lastDraw_ = 0;
    uint32_t frame_ = 0;
};

}  // namespace ot
#endif
