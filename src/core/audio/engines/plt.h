// PLT: two models in the manner of Mutable Instruments Plaits (Emilie
// Gillet, MIT) as an OpenTracker synth engine [OT fork]: our own code after
// the Plaits macro-oscillator idea (Harmonics / Timbre / Morph):
//   0 2-Op FM    Harmonics: the frequency ratio (quantized: 0.5 .. 8),
//                Timbre: the modulation index, Morph: feedback (below the
//                middle the modulator feeds itself, above it the carrier
//                feeds the modulator).
//   1 Harmonic   16 sine partials: Harmonics: the most prominent partial,
//                Timbre: the width of the bump around it, Morph: odd /
//                even balance (middle: both).
// Amp envelope (ADSR), an envelope and an LFO onto Timbre, Tune, Glide,
// Volume. Not a Tracker Mini model: a project with it opens on the Mini with
// the slot reset.
#pragma once

#include "audio/synth_engine.h"

namespace ot {
namespace audio {
namespace plt {

enum Param : int {
    kModel = 0, kHarmonics, kTimbre, kMorph, kTimbreEnv, kAttack, kDecay, kSustain, kRelease, kTune, kGlide,
    kVolume, kLfoFreq, kLfoTimbre, kParamCount = 14
};

ISynthEngine* engine();

}  // namespace plt
}  // namespace audio
}  // namespace ot
