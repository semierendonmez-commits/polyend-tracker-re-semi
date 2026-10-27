// OT fork reverb models from the open-source list (Master Reverb "Model"):
//
//  * PlateReverb: Jon Dattorro's plate ("Effect Design, Part 1", JAES 1997),
//    the network Valley's Plateau is built on: bandwidth filter, four input
//    diffusers, a figure-eight tank of two modulated all-passes, damped
//    delays and decay diffusers, output taps from both halves. The tank runs
//    at half rate (22.05 kHz) with the paper's timing (its 29.76 kHz sample
//    counts x 0.741), which halves its cost and fits the reverb memory.
//  * TextureReverb: a granular cloud in the manner of Mutable Instruments
//    Clouds / Beads: four grains read the recent send input at random
//    positions (some an octave up or down), panned, fed back into the buffer
//    and smeared by two diffusers.
//
// Both run block-wise (every delay is longer than a block, so each element is
// read and written as one span per block) in the stock reverb's int16 memory,
// mono in, stereo out; process() returns false when idle.
#pragma once

#include <cstdint>

#include "audio/audio_tables.h"

namespace ot {
namespace audio {

class PlateReverb {
public:
    static constexpr uint32_t kMemorySamples = 26624;
    // mem: the SDRAM rings; sram: four int16[512] all-pass buffers borrowed
    // from the (then idle) FdnReverb for the input diffusers (per sample).
    void init(int16_t* mem, int16_t* const sram[4]);
    void activate() { clearPending_ = true; }
    void setSize(float v) { size_ = v; }
    void setDamping(float v) { damp_ = v; }
    void setDiffusion(float v) { diff_ = v; }
    void setPredelay(float v) { pre_ = uint32_t((v < 0 ? 0 : (v > 1 ? 1 : v)) * 3900.0f); }
    // Page 2 character: A modulation depth, B input bandwidth (0.5 = Dattorro's).
    void setChar(float a, float b) {
        modScale_ = 2.0f * a;
        bwC_ = b <= 0.5f ? 0.15f + 1.4f * b : 0.85f + 0.3f * (b - 0.5f);
    }
    bool process(const float* in, float* outL, float* outR);

private:
    float modScale_ = 1.0f, bwC_ = 0.85f;
    enum { kPre, kMod0, kDel0, kAp0, kDel1, kMod1, kDel2, kAp1, kDel3, kRings };
    int16_t* ring_[kRings] = {};
    uint32_t mask_[kRings] = {};
    int16_t* in_[4] = {};
    uint16_t inPos_[4] = {};
    bool clearPending_ = true;
    float size_ = 0.6f, damp_ = 0.3f, diff_ = 0.7f;
    uint32_t pre_ = 0, w_ = 0, wh_ = 0, lfo_ = 0, quiet_ = 0;
    float bw_ = 0.0f, dampZ_[2] = {}, tankOut_[2] = {};
};

class TextureReverb {
public:
    static constexpr uint32_t kRingLen = 16384, kApLen = 1024;
    static constexpr uint32_t kMemorySamples = kRingLen + 2 * kApLen;
    static constexpr int kGrains = 4;
    void init(int16_t* mem);
    void activate() { clearPending_ = true; }
    void setSize(float v) { size_ = v; }
    void setDamping(float v) { damp_ = v; }
    void setDiffusion(float v) { diff_ = v; }
    void setPredelay(float v) { pre_ = uint32_t((v < 0 ? 0 : (v > 1 ? 1 : v)) * 4000.0f); }
    // Page 2 character: A grain length (x0.25..x4), B octave grains (0..48 %).
    void setChar(float a, float b);
    bool process(const float* in, float* outL, float* outR);

private:
    float grainScale_ = 1.0f, octProb_ = 0.24f;
    struct Grain {
        uint32_t len = 0, at = 0;   // length, samples played
        uint32_t src = 0;           // ring position of the grain's first sample
        int8_t pitch = 1;           // -1 octave down (half speed), 1 unity, 2 octave up
        float panL = 0.7f, panR = 0.7f;
    };
    void startGrain(Grain& g, int k);
    int16_t* ring_ = nullptr;
    int16_t* ap_[2] = {};
    bool clearPending_ = true;
    float size_ = 0.6f, damp_ = 0.3f, diff_ = 0.6f;
    uint32_t pre_ = 0, w_ = 0, quiet_ = 0, rng_ = 0x1234567u;
    float lpZ_[2] = {}, fbPrev_[kBlock] = {};
    Grain g_[kGrains];
};

}  // namespace audio
}  // namespace ot
