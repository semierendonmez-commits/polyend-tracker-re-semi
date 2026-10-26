// OT fork sidechain ducking (Master 5/5 "Sidechain"): two sources, each with
// its own envelope, each ducking only the targets chosen for it.
//
//  * Trigger mode: a note on the source track (sample or synth note, tracks
//    1..16) starts the envelope: attack up to full depth, hold, release.
//  * Follow mode: the source's audio (tracks 1..8, line in L / R) drives it:
//    the level above the threshold, smoothed by attack and release, held
//    for the hold time after it falls.
//
// Targets: the sample voices of tracks 1..8, the three synth slots, the
// delay and reverb returns and the line input (dry). The gain is
// 1 - depth x envelope per sample; a target of both sources gets both.
#pragma once

#include <cstdint>

#include "audio/audio_tables.h"

namespace ot {
namespace audio {

struct SidechainSettings {
    uint8_t source = 0;    // 0 off, 1..16 track, 17 line in L, 18 line in R
    uint8_t mode = 0;      // 0 trigger (notes), 1 follow (audio)
    uint8_t attack = 2;    // ms, 0..100
    uint8_t hold = 10;     // x 5 ms, 0..100 (0..500 ms)
    uint8_t release = 15;  // x 10 ms, 1..100 (10..1000 ms)
    uint8_t depth = 80;    // %, 0..100
    uint8_t thresh = 60;   // follow: 0..100 = -60..0 dBFS
    uint8_t reserved = 0;
    uint16_t targets = 0;  // kTarget* bits
    static constexpr uint8_t kSourceCount = 19;
    static constexpr uint8_t kLineL = 17, kLineR = 18;
};
// Target bits.
constexpr uint16_t kScTrack0 = 1u << 0;     // .. bit 7: tracks 1..8 (sample voices)
constexpr uint16_t kScSynth0 = 1u << 8;     // .. bit 10: synth slots 1..3
constexpr uint16_t kScDelay = 1u << 11;
constexpr uint16_t kScReverb = 1u << 12;
constexpr uint16_t kScLineIn = 1u << 13;
constexpr int kScTargetCount = 14;

class SidechainEnv {
public:
    void reset() {
        env_ = 0.0f;
        hold_ = 0;
        stage_ = 0;
        follow_ = 0.0f;
        coefValid_ = false;
    }
    // One block: gain[i] = 1 - depth * env (1 = no ducking). `trigger` starts
    // the trigger envelope at the block start; `src` (may be null) feeds the
    // follower. Returns false when the whole block is at gain 1 (nothing to
    // multiply).
    bool process(const SidechainSettings& s, bool trigger, const float* src, float* gain);
    float envelope() const { return env_; }

private:
    float env_ = 0.0f, follow_ = 0.0f;
    uint32_t hold_ = 0;   // groups of 8 samples
    uint8_t stage_ = 0;   // trigger: 0 idle, 1 attack, 2 hold, 3 release
    // Coefficients of the last settings (key_).
    bool coefValid_ = false;
    uint32_t key_ = 0, holdG_ = 0;
    float attInc_ = 0.0f, attC_ = 0.0f, relC_ = 0.0f, invThr_ = 0.0f;
};

}  // namespace audio
}  // namespace ot
