// Generator table, dispatch and shared helpers (gen.h).
#include "gen/gen.h"

#include <cstdio>

#include "gen/gen_internal.h"
#include "model/fx.h"

namespace ot {
namespace gen {

namespace {

const char* const kOctLabels[3] = {"1 oct", "2 oct", "3 oct"};
const char* const kAcidOct[3] = {"Off", "TB-3PO", "Wide"};
const char* const kAcidEvery[4] = {"1 loop", "2 loops", "4 loops", "8 loops"};
const char* const kAcidGate[3] = {"Normal", "Short", "Long"};
const char* const kCrLayout[3] = {"Call+Answer", "Answer mine", "Two tracks"};
const char* const kCrForm[6] = {"C R", "C R C R", "C C R", "C R R2 C", "C C C R", "C R C R2"};
const char* const kCrSteps[5] = {"Auto", "4", "8", "16", "32"};
const char* const kCrEnding[5] = {"Style", "Resolve", "Open", "Rise", "Fall"};
const char* const kCrOct[3] = {"Same", "+1 oct", "-1 oct"};
const char* const kCrMotif[4] = {"Auto", "2", "3", "4"};
const char* const kBBSource[6] = {"Auto", "1 beat", "2 beats", "1 bar", "2 bars", "4 bars"};
const char* const kBBMutate[5] = {"Off", "1 loop", "2 loops", "4 loops", "8 loops"};
const char* const kBBRolls[7] = {"Clean", "Fade out", "Fade in", "Pitch up", "Pitch down", "Stutter", "Single"};
const char* const kBBPitch[4] = {"Off", "+-1", "+-2", "+-3"};
const char* const kOnOff[2] = {"On", "Off"};
const char* const kTaPhrase[4] = {"1 bar", "2 bars", "4 bars", "8 bars"};
const char* const kTaStyle[6] = {"Rolls", "Stutter", "Drop", "Reverse", "Build", "Mix"};
const char* const kTaFx[3] = {"Soft", "Medium", "Hard"};
const char* const kTaTiming[2] = {"Tidy", "Keep"};
const char* const kGtMode[4] = {"Markov", "Ball", "Ball+Markov", "Drummer"};
const char* const kGrAccent[2] = {"On", "Off"};
const char* const kGrRes[2] = {"1/16", "1/32"};
const char* const kGrParts[4] = {"Kit 3 trk", "Kick", "Snare", "Hats"};
const char* const kMbSteps[5] = {"Chromatic", "Scale", "Triad", "Root+5th", "Root"};
const char* const kMbModel[4] = {"Coin", "Clusters", "Drums", "2 tracks"};
const char* const kMbRate[2] = {"1/16", "1/8"};
const char* const kTmGate[3] = {"Full", "Half", "Short"};
const char* const kStSource[4] = {"RDS name", "PI code", "Frequency", "All"};
const char* const kStMode[2] = {"Notes", "Slices"};
const char* const kStRate[3] = {"1/16", "1/8", "1/4"};
const char* const kStGroups[2] = {"Off", "Fives"};

constexpr const char* kSeedHelp = "0: a new result every time. 1-999: the same result again";
#define NONE {nullptr, 0, 0, 0, nullptr, nullptr}
#define SEED {"Seed", 0, 999, 0, nullptr, kSeedHelp}

// Labels filled at the first use (names from the generators' own tables);
// the descriptor table itself is const (flash), it only points at them.
const char* gCrStyleLabels[kCrStyleCount];
const char* gCrStrategyLabels[kCrStrategyCount];
const char* gBBProcLabels[kBBProcCount];

const KindDesc gKinds[kKindCount] = {
    {"Acid", "303 line: density, accent, slide (after TB-3PO)",
     {{"Density", -7, 7, 5, nullptr, "TB-3PO: 0 sparse, +-7 every step; below 0 fewer pitches (-7 root only). Live: Perform Acid Density"},
      {"Accent", 0, 100, 40, nullptr, "Chance of an accent (Volume 100)"},
      {"Slide", 0, 100, 25, nullptr, "Chance a note glides into the next one (Glide FX, no gate)"},
      {"Octaves", 0, 2, 1, kAcidOct, "Octave jumps of new pitches: TB-3PO 20 % up / 20 % down, Wide up to +2"},
      {"Mutate", 0, 100, 0, nullptr, "Live: share of the steps re-rolled while it plays. 0 = locked"},
      SEED,
      {"Mut. every", 0, 3, 0, kAcidEvery, "How often Mutate changes the line"},
      {"Gate", 0, 2, 0, kAcidGate, "Length of the notes that do not slide"},
      NONE, NONE, NONE, NONE}},
    {"Call+Resp", "Call phrase, then the answer (Nordl3 C/R Generator)",
     {{"Style", 0, kCrStyleCount - 1, 0, gCrStyleLabels, "Genre profile: density, leaps, accents, slides, rests"},
      {"Answer", 0, kCrStrategyCount - 1, 35, gCrStrategyLabels, "How the answer is made from the call (default Q+closure)"},
      {"Density", 0, 100, 0, nullptr, "Notes per phrase. 0 = the style's own density"},
      {"Layout", 0, 2, 0, kCrLayout, "One track / answer the call you wrote / call and answer on two tracks"},
      {"Form", 0, 5, 0, kCrForm, "Order of the parts: C = call, R = answer (R2 = a second answer)"},
      SEED,
      {"Steps", 0, 4, 0, kCrSteps, "Steps per part. Auto = the longest 4/8/16/32 that fits; the form repeats to the end"},
      {"Ending", 0, 4, 0, kCrEnding, "How the answer ends: the style's way, on the root, open, rising, falling"},
      {"Ans. oct", 0, 2, 0, kCrOct, "Register of the answer against the call"},
      {"Motif", 0, 3, 0, kCrMotif, "Length of the repeated motif in steps. Auto = from the part length"},
      NONE, NONE}},
    {"BBCut", "Rearrange a sliced break (BBCut / BBenCut procs)",
     {{"Proc", 0, kBBProcCount - 1, 0, gBBProcLabels, "BBCut cut procedure"},
      {"A", 0, 100, 50, nullptr, nullptr},
      {"B", 0, 100, 50, nullptr, nullptr},
      {"Source", 0, 5, 0, kBBSource, "Length of the break: Auto from the sample length at the song tempo"},
      {"Mutate", 0, 4, 0, kBBMutate, "Live: new cuts (a new seed) every N loops while it plays"},
      SEED,
      {"Rolls", 0, 6, 0, kBBRolls, "Cuts shorter than a step: Roll FX styles, a pitch slide, a hit per step, one hit"},
      {"Pitch", 0, 3, 0, kBBPitch, "Random pitch per hit in semitones (Tune FX on Beat Slice)"},
      {"Timing", 0, 1, 0, kOnOff, "Micro-move for cuts between steps"},
      {"Dynamics", 0, 1, 0, kOnOff, "Volume FX for quieter / louder cuts"},
      NONE, NONE}},
    {"Turnaround", "Phrase-end rolls, stutters and drops on a budget",
     {{"Phrase", 0, 3, 1, kTaPhrase, "Phrase length: every phrase, half and quarter phrase end gets edits"},
      {"Budget", 1, 16, 4, nullptr, "Steps of edits at a phrase end"},
      {"Style", 0, 5, 5, kTaStyle, "Kind of edits"},
      {"Intensity", 0, 100, 60, nullptr, "How strong and how many the edits are"},
      {"FX", 0, 2, 0, kTaFx, "How hard the FX hit: roll speed up to 2 / 4 / 8 per step, build volume from 70 / 50 / 30"},
      SEED,
      {"Timing", 0, 1, 0, kTaTiming, "Tidy: no Micro-move inside the edited steps. Keep: leave it as it was"},
      NONE, NONE, NONE, NONE, NONE}},
    {"Genotype", "Learn the track, play a variation of it",
     {{"Mode", 0, 3, 2, kGtMode,
       "Markov walk, a bouncing TILT ball, both, or Drummer: all selected tracks learnt together as a kit"},
      {"Identity", 0, 100, 60, nullptr, "Chance a step stays as it was"},
      {"Order", 1, 2, 1, nullptr, "Markov memory: 1 or 2 steps"},
      {"Variety", 0, 100, 40, nullptr, "How far the walk may jump"},
      NONE,
      SEED,
      NONE, NONE, NONE, NONE, NONE, NONE}},
    {"Station", "Numbers station from the radio (RDS, PI, frequency)",
     {{"Source", 0, 3, 3, kStSource, "What is read from the radio: station name, PI code, frequency, all"},
      {"Mode", 0, 1, 0, kStMode, "Notes: digits as scale degrees. Slices: digit d plays slice d (a voice counting 0-9)"},
      {"Rate", 0, 2, 1, kStRate, "Length of one digit"},
      {"Groups", 0, 1, 1, kStGroups, "Read in groups of five with a pause between"},
      {"Repeat", 1, 4, 2, nullptr, "Times the message is read"},
      SEED,
      NONE, NONE, NONE, NONE, NONE, NONE}},
    {"Grids", "Mutable Grids drum map: X / Y between 25 patterns",
     {{"X", 0, 100, 50, nullptr, "Map position X. Live: Perform FX Grids X"},
      {"Y", 0, 100, 50, nullptr, "Map position Y. Live: Perform FX Grids Y"},
      {"Kick", 0, 100, 60, nullptr, "Kick density (first track). Live: Perform FX Grids Density moves all three"},
      {"Snare", 0, 100, 50, nullptr, "Snare density (second track)"},
      {"Hats", 0, 100, 60, nullptr, "Hi-hat density (third track)"},
      SEED,
      {"Chaos", 0, 100, 0, nullptr, "Random lift of the levels, new every bar (live: every loop)"},
      {"Accent", 0, 1, 0, kGrAccent, "On: hits below the accent level get Volume 70"},
      {"Res", 0, 1, 0, kGrRes, "1/16: one Grids bar per 16 steps. 1/32: per 32 steps (half time, the 32nd hits too)"},
      {"Parts", 0, 3, 0, kGrParts, "Kit on three tracks from the cursor, or one part on the cursor track"},
      NONE, NONE}},
    {"Marbles", "Random notes with deja vu (Mutable Marbles)",
     {{"Density", 0, 100, 60, nullptr, "T: chance a step plays"},
      {"Deja vu", 0, 100, 50, nullptr, "0: always new draws. 100: the last Length draws loop"},
      {"Length", 1, 16, 8, nullptr, "Deja vu loop length in draws"},
      {"Spread", 0, 100, 50, nullptr, "X: width of the pitch distribution (0 one note, top: the extremes)"},
      {"Bias", 0, 100, 50, nullptr, "X: centre of the distribution, low to high"},
      SEED,
      {"Steps", 0, 4, 1, kMbSteps, "Quantizer: chromatic, the project scale, its triad, root and fifth, root only"},
      {"Range", 0, 2, 1, kOctLabels, "Pitch range above the root"},
      {"T model", 0, 3, 0, kMbModel, "Coin: independent. Clusters: hits bunch up. Drums: on the beat. 2 tracks: one or the other"},
      {"Jitter", 0, 100, 0, nullptr, "Random Micro-move on the notes"},
      {"Rate", 0, 1, 0, kMbRate, "A draw every step, or every second step"},
      NONE}},
    {"Turing", "Turing Machine: a looping shift register (Music Thing)",
     {{"Length", 1, 16, 8, nullptr, "Loop length in steps (register bits). Live: Perform FX Turing Length"},
      {"Lock", -100, 100, 80, nullptr,
       "+100 a locked loop, 0 random, -100 locked at twice the length, inverted. Live: Perform FX Turing Machine"},
      {"Density", 0, 100, 70, nullptr, "Share of the steps that play (from the register, so it loops too)"},
      {"Range", 0, 2, 1, kOctLabels, "Pitch range above the root"},
      {"Steps", 0, 4, 1, kMbSteps, "Quantizer: chromatic, the project scale, its triad, root and fifth, root only"},
      SEED,
      {"Gate", 0, 2, 0, kTmGate, "Note length: the whole step, half (Gate 50), short (Gate 20)"},
      {"Velocity", 0, 100, 0, nullptr, "Volume from the register: 0 all full, 100 a wide spread"},
      {"Rate", 0, 1, 0, kMbRate, "A clock every step, or every second step"},
      {"Glide", 0, 100, 0, nullptr, "Chance of a Glide between two notes in a row"},
      NONE, NONE}},
};
#undef NONE
#undef SEED

bool labelsReady = false;
void initLabels() {
    if (labelsReady) return;
    for (int i = 0; i < kCrStyleCount; ++i) gCrStyleLabels[i] = crStyleName(i);
    for (int i = 0; i < kCrStrategyCount; ++i) gCrStrategyLabels[i] = crStrategyName(i);
    for (int i = 0; i < kBBProcCount; ++i) gBBProcLabels[i] = bbProcName(i);
    labelsReady = true;
}

}  // namespace

const KindDesc& kind(int k) {
    initLabels();
    return gKinds[k < 0 ? 0 : (k >= kKindCount ? kKindCount - 1 : k)];
}

const ParamDesc& param(int k, int i, const int16_t* values) {
    const KindDesc& d = kind(k);
    if (k == kBBCut && (i == 1 || i == 2) && values) {
        const ParamDesc* bp = bbProcParam(values[0], i - 1);
        static const ParamDesc kNone = {nullptr, 0, 0, 0, nullptr, nullptr};
        return bp ? *bp : kNone;
    }
    return d.p[i];
}

int scaleDegrees(uint16_t mask, uint8_t out[12]) {
    int n = 0;
    for (int pc = 0; pc < 12; ++pc)
        if (mask & (1u << pc)) out[n++] = uint8_t(pc);
    if (n == 0) {   // no scale: natural minor
        static const uint8_t kMinor[7] = {0, 2, 3, 5, 7, 8, 10};
        for (int i = 0; i < 7; ++i) out[i] = kMinor[i];
        n = 7;
    }
    return n;
}

int degreeNote(int root, const uint8_t* deg, int n, int d) {
    int oct = d >= 0 ? d / n : -((-d + n - 1) / n);
    int idx = d - oct * n;
    int note = root + deg[idx] + 12 * oct;
    return note < 0 ? 0 : (note > 127 ? 127 : note);
}

void setFx(Step& s, int slot, uint8_t type, uint8_t value) {
    s.fx[slot].type = type;
    s.fx[slot].value = value;
}

// Up to two FX on a step; extra ones are dropped (callers pass them by priority).
void putFx(Step& s, uint8_t type, uint8_t value) {
    for (int k = 0; k < kFxPerStep; ++k)
        if (s.fx[k].type == 0) {
            setFx(s, k, type, value);
            return;
        }
}

void writeMono(Pattern& p, int t, int s0, const MonoStep* steps, int n, uint8_t instr, uint8_t glide,
               uint8_t normalVol) {
    Track& tr = p.tracks[t];
    for (int i = 0; i < n; ++i) {
        const int s = s0 + i;
        if (s >= kStepCount) break;
        Step& st = tr.steps[s];
        st.clear();
        const MonoStep& m = steps[i];
        if (!m.active) continue;
        st.note = int8_t(m.note < 0 ? 0 : (m.note > 127 ? 127 : m.note));
        st.instrument = instr;
        const MonoStep* prev = i > 0 ? &steps[i - 1] : nullptr;
        const bool glideIn = prev && prev->active && prev->slide;
        // Priority: Glide (the slide itself), accent Volume, then Gate.
        if (glideIn) putFx(st, kFxGlide, glide);
        if (m.accent) putFx(st, kFxVolume, 100);
        else if (normalVol) putFx(st, kFxVolume, normalVol);
        // A note that slides out (or ties) sustains into the next step.
        if (!m.slide && m.gate < 0.95f) {
            int g = int(m.gate * 100.0f + 0.5f);
            putFx(st, kFxGate, uint8_t(g < 5 ? 5 : (g > 100 ? 100 : g)));
        }
    }
}

Result generate(int k, const int16_t* params, Target& t) {
    initLabels();
    Result r;
    if (!t.pat) return r;
    if (t.s1 < t.s0) {
        const int x = t.s0;
        t.s0 = t.s1;
        t.s1 = x;
    }
    if (t.t1 < t.t0) {
        const int x = t.t0;
        t.t0 = t.t1;
        t.t1 = x;
    }
    int16_t v[kParams];
    for (int i = 0; i < kParams; ++i) {
        const ParamDesc& d = param(k, i, params);
        int x = params[i];
        if (d.name) x = x < d.min ? d.min : (x > d.max ? d.max : x);
        v[i] = int16_t(x);
    }
    const uint32_t seed = v[kSeedParam] > 0 ? uint32_t(v[kSeedParam]) * 2654435761u : (t.entropy | 1u);
    r.seed = v[kSeedParam] > 0 ? uint32_t(v[kSeedParam]) : 0;
    r.rngSeed = seed;
    Rng rng(seed);
    switch (k) {
        case kAcid: genAcid(v, t, rng, r); break;
        case kCallResponse: genCallResponse(v, t, rng, r); break;
        case kBBCut: genBBCut(v, t, rng, r); break;
        case kTurnaround: genTurnaround(v, t, rng, r); break;
        case kGenotype: genGenotype(v, t, rng, r); break;
        case kStation: genStation(v, t, rng, r); break;
        case kGrids: genGrids(v, t, rng, r); break;
        case kMarbles: genMarbles(v, t, rng, r); break;
        case kTuring: genTuring(v, t, rng, r); break;
        default: std::snprintf(r.msg, sizeof(r.msg), "Unknown generator"); break;
    }
    return r;
}

}  // namespace gen
}  // namespace ot
