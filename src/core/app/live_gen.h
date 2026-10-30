// Live generators (OT fork): what keeps running while the sequencer plays.
//
//  * BBCut play modes B1..B10: new cut tables at every stop, every N loops
//    while playing, or locked (Config > BB play modes); procedure A / B from
//    the Generate popup.
//  * BBCut Mutate (up to kBBSlots selections): new cuts every N loops.
//  * Genotype play modes G1..G3: a new Markov walk over each such track's
//    own steps every loop.
//  * Acid: a track written by Acid stays "live" (up to kAcidSlots tracks):
//      - Density follows the Perform FX "Acid Density" (-7..+7 on top of the
//        generated density, on the tracks the FX is on), changing the notes
//        right away without changing the melody;
//      - Mutate > 0 re-rolls part of the line every "Mut. every" loops
//        (TB-3PO's mutate).
//    A slot ends when its track is edited by hand, when it is generated
//    again by something else, or when the pattern is left.
//  * Grids: tracks written by Grids stay live (kGridsSlots): Perform "Grids
//    X / Y / Density" redraw them at once, Chaos > 0 draws new chaos every loop.
//  * Play modes M1..M3 / R1 / R2 (and the hidden TM8 / TM16 of older sets):
//    per-track tables every loop (Marbles deja vu, Turing register, randLoops
//    drift); Perform "Loop Lock".
//  * Turing: tracks written by the Turing generator (kTuringSlots) run their
//    register on every loop with Perform "Turing Machine" / "Turing Length";
//    the knob on other tracks plays their steps in Turing order.
#pragma once

#include <cstdint>

#include "gen/gen.h"

namespace ot {

class App;

namespace live {

constexpr int kAcidSlots = 4;
constexpr int kBBSlots = 2;
constexpr int kGridsSlots = 2;
constexpr int kTuringSlots = 4;

// Main loop, every pass (App::loop).
void service(App& a);
// Takes over the acid line Acid just wrote on target track t.t0.
// v = the Acid parameters, rngSeed = gen::Result::rngSeed of the generate.
void acid(App& a, const int16_t* v, const gen::Target& t, uint32_t rngSeed);
// Drops a live slot on track `track` (another generator wrote there).
void release(int track);
bool acidActive(int track);
// BBCut: A / B of procedure `proc` (Generate popup), also used by the B play
// modes; bbcut() takes over a track BBCut just wrote (live Mutate).
void setBBParams(int proc, int a, int b);
void bbcut(App& a, const int16_t* v, const gen::Target& t, uint32_t rngSeed);
bool bbcutActive(int track);
// Perform FX "Acid Density" offset per track (0 = none); applied by service().
void setAcidDensityOffset(int track, int offset);
// Perform FX "Acid Accent" / "Acid Slide" / "Acid Mutate": percent offsets
// on top of the generated amounts, per track (0 = none).
void setAcidPerf(int track, int accent, int slide, int mutate);
// Perform FX for the BBCut play modes (global): A / B offsets in list steps,
// rolls (0 as built, 1 none, 2 dense, 3 fade) and re-cut override (0 =
// Config, 2..5 every 1 / 2 / 4 / 8 loops, 6 locked; 1 "Now" via recutNow).
void setBBPerf(int aOff, int bOff, int rolls, int recut);
void recutNow();   // new BBCut play-mode cuts (and a new BBCut Mutate draw) at once
// Grids: takes over the tracks Grids just wrote; Perform offsets (global).
void grids(App& a, const int16_t* v, const gen::Target& t, uint32_t rngSeed);
bool gridsActive(int track);
void setGridsPerf(int x, int y, int density);
// Perform "Loop Lock" (-100..100) for the M / TM / R play modes.
void setLoopLock(int track, int v);   // Perform "Loop Lock" per track (-100..+100)
// Turing: takes over the tracks the Turing generator just wrote; Perform
// "Turing Machine" (knob, 0 = off) and "Turing Length" (0 = as generated) per
// track. On a live Turing track the register runs on at every loop end while
// either is on; on any other track the knob plays the steps in Turing order.
void turing(App& a, const int16_t* v, const gen::Target& t, uint32_t rngSeed);
bool turingActive(int track);
void setTuringPerf(int track, int knob, int len);
void stopAll();

}  // namespace live
}  // namespace ot
