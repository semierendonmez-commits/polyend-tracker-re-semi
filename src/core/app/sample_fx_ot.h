// OT fork Sample Editor effects: Transcode (G.711 / G.726 generation loss,
// bit errors, packet loss) and DCT Crush (spectral crush). See the .cpp.
#pragma once

#include <cstdint>

namespace ot {
namespace sfx {

extern const char* const kTcCodecs[6];
extern const char* const kTcRates[4];
extern const char* const kTcConceal[3];
extern const char* const kDcBlocks[4];
extern const char* const kRdBands[4];
extern const char* const kClCurves[7];
extern const char* const kOffOn[2];

// p: Codec, Rate, Generations, Bit errors (per 10k bits), Loss %, Conceal, Mix %.
uint32_t transcode(const float* p, const int16_t* in, uint32_t n, int16_t* out);
// p: Block, Keep %, Bits, Freeze %, Mix %.
uint32_t dctCrush(const float* p, const int16_t* in, uint32_t n, int16_t* out);

// p: Bandwidth, Fading %, Fade rate %, Whistle %, Pitch Hz, Noise %, Crashes %, Mix %.
// A shortwave receiver: the band filter, QSB fading with the noise rising in
// the dips (AGC), a heterodyne whistle, band noise and static crashes. The
// random parts are seeded the same for every call, so both channels of a
// stereo sample fade together.
uint32_t radio(const float* p, const int16_t* in, uint32_t n, int16_t* out);
// p: Curve, Drive dB, Knee %, Match (normalise to 0 dBFS first), Ceiling -dB, Mix %.
// NEXUSCLIP's seven curves bounded at 0 dBFS, then the ceiling.
uint32_t clipper(const float* p, const int16_t* in, uint32_t n, int16_t* out);
float clipCurve(int curve, float x, float knee);   // tests

// OT fork tools from the open-source list (sample_fx_mod.cpp).
// Scratch memory for the heavy offline effects (the editor lends free sample
// memory before processing; nullptr = none: they fall back to a copy).
void setScratch(float* p, uint32_t floats);
// Progress of the long offline effects (Paulstretch): fn(ctx, 0..100) as they
// go, so the editor can draw it (the work is synchronous). nullptr = none.
void setProgress(void (*fn)(void* ctx, int percent), void* ctx);
uint32_t scratchFloats(int fx, const float* p);   // floats an effect wants (0 = none)
// YIN pitch of the selection: Hz and clarity 0..1; false when none found.
bool detectPitch(const int16_t* in, uint32_t n, float* hz, float* clarity);
// Tune (semitones) and finetune (cents) that put `hz` on the unity note's C.
void tuneFor(float hz, int* tune, int* fine);
// p: Stretch x, Window (0..3 = 50 / 100 / 200 / 400 ms).
uint32_t paulstretchFrames(const float* p, uint32_t n);
uint32_t paulstretch(const float* p, const int16_t* in, uint32_t n, int16_t* out, uint32_t m);
// p: Length x, Grain ms, Density per second, Spray %, Pitch spread semitones, Reverse %.
uint32_t granularFrames(const float* p, uint32_t n);
uint32_t granular(const float* p, const int16_t* in, uint32_t n, int16_t* out, uint32_t m);
// p: Segment ms, Shuffle %, Repeat %, Reverse %, Length x.
uint32_t strandFrames(const float* p, uint32_t n);
uint32_t strand(const float* p, const int16_t* in, uint32_t n, int16_t* out, uint32_t m);
extern const char* const kPsWindows[4];

// Radix-2 complex FFT in place (n a power of two; inverse unscaled).
void fftInPlace(float* re, float* im, int n, bool inverse);

// The codec chain alone, at the input rate, no errors (tests).
void codecDirect(int codec, int gens, const int16_t* in, uint32_t n, int16_t* out);
// G.711 round trips (tests).
int16_t ulawRoundTrip(int16_t x);
int16_t alawRoundTrip(int16_t x);

}  // namespace sfx
}  // namespace ot
