// Performance pattern play modes (spec 12 §D, spec 03 §2.6): step-order
// tables for modes 3..22 ("C1".."C20") and the roll-injection rule.
#pragma once

#include <cstdint>

#include "model/pattern.h"
#include "sequencer/seq_rng.h"

namespace ot {
namespace seq {

// Play-mode ids (FX 18 value, per-track).
constexpr uint8_t kPlayFwd = 0;
constexpr uint8_t kPlayBack = 1;
constexpr uint8_t kPlayRnd = 2;
constexpr uint8_t kPlayFirstTable = 3;   // "C1"
// OT fork: modes 23..32 ("B1".."B10") play BBCut cut tables (one per BBCut
// procedure, src/core/gen), built in RAM by the app while the sequencer is
// stopped (a new cut set on every play start).
constexpr uint8_t kPlayFirstBB = 23;
constexpr int kBBModeCount = 10;
// OT fork: modes 33..36 ("T1".."T4") play the track with a TILT pinball ball
// (Bounce, Flipper, Gravity, Multiball), a new path per seed (gTiltSeed, set
// by the app like the BBCut cut sets).
constexpr uint8_t kPlayFirstTilt = 33;
constexpr int kTiltModeCount = 4;
// OT fork: modes 37..39 ("G1".."G3") play a Genotype variation of the
// track's own steps (Markov over its notes, low / medium / high variety), a
// new walk every loop: per-track tables built by the app (live_gen).
constexpr uint8_t kPlayFirstGeno = 37;
constexpr int kGenoModeCount = 3;
constexpr uint8_t kPlayModeCount = 40;   // 0..39

constexpr int kPlayModeTableCount = 20;
constexpr int kPlayModeTableLen = 128;

// BBCut tables: entry k = {offset (1/128 of the track at the cut start),
// steps since the cut start, roll selector 0..15}. Written by the main loop
// only while the sequencer is stopped; read by the sequencer.
extern uint8_t gBBPlayTables[kBBModeCount][kPlayModeTableLen][3];
extern bool gBBPlayTablesReady;   // false: the B modes play forward
extern volatile uint32_t gTiltSeed;
// OT fork Perform "BBCut Rolls": 0 as built, 1 none, 2 dense, 3 fade-out.
extern volatile uint8_t gBBRollMode;
// Genotype tables: entry k = the step to play (mod the track length), one
// table per track; gGenoReady[t] false plays forward.
extern uint8_t gGenoTables[16][kPlayModeTableLen];
extern volatile bool gGenoReady[16];

// Stock behaviour: a roll-selector-2 miss leaves the previous injected roll
// in place (spec 12 §D quirk). OpenTracker fixes it; set false for stock.
constexpr bool kFixStockRollCarry = true;

// Table for mode m (3..22) at kPlayModeTables[m - 3]; entry k =
// {stepByte (1-based, 0 = step 1), rollSel 0..15}. Const data (flash).
extern const uint8_t kPlayModeTables[kPlayModeTableCount][kPlayModeTableLen][2];

// Roll value per selector group g = (rollSel - 1) / 3 (spec 12 §D).
extern const uint8_t kRollInjectValue[5];

inline bool isTableMode(uint8_t mode) { return mode >= kPlayFirstTable && mode < kPlayModeCount; }
inline bool isBBMode(uint8_t mode) { return mode >= kPlayFirstBB && mode < kPlayFirstTilt; }
inline bool isTiltMode(uint8_t mode) { return mode >= kPlayFirstTilt && mode < kPlayFirstGeno; }
inline bool isGenoMode(uint8_t mode) { return mode >= kPlayFirstGeno && mode < kPlayModeCount; }
// TILT ball position after k moves (k mod 128) on a track of `len` steps.
int tiltStep(int tiltMode, int k, int len, uint32_t seed);

// Step for table mode `mode` at counter k with last step index L (spec 12 §D):
// 0 if stepByte = 0, else (stepByte - 1) mod (L + 1). Writes the roll selector.
inline int tableStep(uint8_t mode, int k, int lastIndex, uint8_t& rollSel, int track = 0) {
    if (isGenoMode(mode)) {
        rollSel = 0;
        const int t = track & 15;
        if (!gGenoReady[t]) return k % (lastIndex + 1);
        return gGenoTables[t][k & (kPlayModeTableLen - 1)] % (lastIndex + 1);
    }
    if (isTiltMode(mode)) {
        rollSel = 0;
        return tiltStep(mode - kPlayFirstTilt, k, lastIndex + 1, gTiltSeed);
    }
    if (isBBMode(mode)) {
        if (!gBBPlayTablesReady) {
            rollSel = 0;
            return k % (lastIndex + 1);
        }
        const uint8_t* b = gBBPlayTables[mode - kPlayFirstBB][k & (kPlayModeTableLen - 1)];
        rollSel = b[2];
        switch (gBBRollMode) {   // OT fork Perform "BBCut Rolls"
            case 1: rollSel = 0; break;
            case 2: rollSel = rollSel ? 3 : (b[1] == 0 && (k & 1) ? 2 : 0); break;   // more and fuller
            case 3: rollSel = rollSel ? 6 : 0; break;                                // fade-out rolls
            default: break;
        }
        const int len = lastIndex + 1;
        return ((int(b[0]) * len) / kPlayModeTableLen + int(b[1])) % len;
    }
    const uint8_t* e = kPlayModeTables[mode - kPlayFirstTable][k & (kPlayModeTableLen - 1)];
    rollSel = e[1];
    return e[0] == 0 ? 0 : (e[0] - 1) % (lastIndex + 1);
}

// Roll injection for one step (spec 12 §D, [LG:0xc8da8]). `inject` is the
// track's injected FX (REC(t)+0x24). Draws from rng only for rollSel 1..15.
void applyRollSelector(StepFx& inject, uint8_t rollSel, ParkMiller& rng, bool fixCarry = kFixStockRollCarry);

// UI label for a play mode: "Fwd", "Back", "Rnd", "C1".."C20" (spec 12 §D).
const char* playModeLabel(uint8_t mode);

}  // namespace seq
}  // namespace ot
