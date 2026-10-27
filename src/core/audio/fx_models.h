// OT fork send-effect models (Master Reverb / Master Delay "Model").
//
// They run instead of the stock reverb / delay, in the same caller-provided
// int16 memory (SDRAM on the device), so they cost no RAM of their own beyond
// a few small SRAM diffusers. Our own designs from the textbook building
// blocks (feedback delay network with a Hadamard matrix, Schroeder all-pass
// diffusers, a two-head octave shifter, a modulated tape line); no code is
// taken from the reverbs in the plug-in family's source list.
//
//  * FdnReverb: 1 Hall (4-line FDN, slow modulation, damping in the loop),
//    2 Shimmer (Hall with an octave-up shifter fed back into the network),
//    3 Gated (a short dense FDN through a hold-and-close gate, 80s drums).
//  * TapeDelay: 1 Tape (wow and flutter on the read head, soft saturation
//    and a darkening low-pass in the feedback), 2 Lo-fi (sample-and-hold and
//    6-bit steps in the loop, band-limited repeats).
//
// Both process kBlock samples: mono in, stereo out, and return false when
// they are idle (silent input and a quiet tail), with zeros written out.
#pragma once

#include <cstdint>

#include "audio/audio_tables.h"

namespace ot {
namespace audio {

class FdnReverb {
public:
    static constexpr uint32_t kPreCap = 8192, kLineCap = 4096, kShimCap = 4096;
    static constexpr int kLines = 4, kDiff = 4, kDiffCap = 512;
    static constexpr uint32_t kMemorySamples = kPreCap + kLines * kLineCap + kShimCap;
    void init(int16_t* mem);
    // 1 Hall, 2 Shimmer, 3 Gated, 4 Room; a change clears the memory on the next block.
    void setModel(int m);
    void activate() { clearPending_ = true; dirty_ = true; }   // the memory was used by another model
    int16_t* sramAp(int k) { return ap_[k]; }   // lent to PlateReverb while the FDN is idle
    int model() const { return model_; }
    void setSize(float v) { if (v != size_) { size_ = v; dirty_ = true; } }
    void setDamping(float v) { if (v != damp_) { damp_ = v; dirty_ = true; } }
    void setDiffusion(float v) { if (v != diff_) { diff_ = v; dirty_ = true; } }
    void setPredelay(float v) { pre_ = uint32_t((v < 0 ? 0 : (v > 1 ? 1 : v)) * 5000.0f); }
    // Master Reverb page 2 character values A / B (0..1, 0.5 = the model's
    // own sound): Hall mod depth / rate, Shimmer amount / mod depth, Gated
    // hold / release, Room early reflections / mod depth.
    void setChar(float a, float b) { if (a != chA_ || b != chB_) { chA_ = a; chB_ = b; dirty_ = true; } }
    bool process(const float* in, float* outL, float* outR);

private:
    void configure();
    float chA_ = 0.5f, chB_ = 0.5f;
    float modScale_ = 1.0f, rateHz_ = 0.45f, erScale_ = 1.0f, relC_ = 0.004f;
    void clearAll();
    int16_t* pre_mem_ = nullptr;
    int16_t* line_[kLines] = {};
    int16_t* shim_ = nullptr;
    int model_ = 1;
    bool clearPending_ = true, dirty_ = true;
    float size_ = 0.7f, damp_ = 0.2f, diff_ = 0.625f;
    uint32_t pre_ = 0;
    uint32_t w_ = 0;                 // shared write index (all rings are powers of two)
    uint32_t len_[kLines] = {};
    float g_[kLines] = {};           // per-line decay gain
    float lp_ = 0.5f;                // damping low-pass coefficient
    float z_[kLines] = {};
    float apG_ = 0.6f;
    int16_t ap_[kDiff][kDiffCap] = {};
    uint16_t apLen_[kDiff] = {};
    uint16_t apPos_[kDiff] = {};
    uint32_t lfo_ = 0;               // modulation phase (block steps)
    float outGain_ = 0.35f;
    // Shimmer heads.
    uint32_t shimD_ = 0;
    float shimAmt_ = 0.0f;
    // Gate.
    float gateEnv_ = 0.0f, gateGain_ = 0.0f;
    uint32_t gateHold_ = 0, holdLen_ = 0;
    uint32_t quiet_ = 0;
};

class TapeDelay {
public:
    void init(int16_t* lineA, int16_t* lineB);   // two kDelayLineLen rings (the stock delay's A and B)
    void setModel(int m) { model_ = m; }         // 1 Tape, 2 Lo-fi
    void setTimeSamples(uint32_t t) { time_ = t < 256 ? 256 : (t > kDelayLineLen - 512 ? kDelayLineLen - 512 : t); }
    void setFeedback(float f) { fb_ = f < 0 ? 0 : (f > 0.97f ? 0.97f : f); }
    void setFilter(float hz);
    void setPingPong(bool p) { pingpong_ = p; }
    bool process(const float* in, float* outL, float* outR);

private:
    int16_t* a_ = nullptr;
    int16_t* b_ = nullptr;
    int model_ = 1;
    uint32_t w_ = 0, time_ = 22050;
    float fb_ = 0.5f, lpA_ = 0.5f;
    bool pingpong_ = false;
    float lpZ_[2] = {}, hpZ_[2] = {}, hold_[2] = {};
    uint32_t holdN_ = 0;
    float wowPh_ = 0.0f, flutPh_ = 0.0f;
    uint32_t quiet_ = 0;
};

}  // namespace audio
}  // namespace ot
