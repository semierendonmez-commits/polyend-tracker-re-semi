// OT fork master colour (Master 4/5 Dynamics, after the saturation): small
// processors in the manner of Chris Johnson's Airwindows plugins (MIT),
// rewritten for the block engine with a polynomial sine (no libm calls in
// the audio block):
//   1 ToTape       sine saturation, a head bump (~90 Hz band boost) and a
//                  gentle high-frequency roll-off
//   2 Console      Airwindows Spiral (Console7's channel curve):
//                  sin(x |x|) / |x|
//   3 Density      Airwindows Density at full: the sine of |x| up to pi / 2
//   4 PurestDrive  Airwindows PurestDrive: the sine blended in by how hard
//                  the last and the current sample drive it
#pragma once

#include <cstdint>

namespace ot {
namespace audio {

// sin(x) for |x| <= pi / 2 (Taylor to x^7, error < 2e-4); clamped outside.
inline float colorSin(float x) {
    if (x > 1.5707963f) x = 1.5707963f;
    if (x < -1.5707963f) x = -1.5707963f;
    const float x2 = x * x;
    return x * (1.0f - x2 * (1.0f / 6.0f - x2 * (1.0f / 120.0f - x2 * (1.0f / 5040.0f))));
}

struct ColorState {
    float prev = 0.0f;           // PurestDrive
    float b1 = 0.0f, b2 = 0.0f;  // ToTape head bump (biquad band-pass state)
    float lp = 0.0f;             // ToTape roll-off
};

// One block of channel x through colour model `model` (1..4).
inline void colorBlock(int model, float* x, uint32_t n, ColorState& s) {
    switch (model) {
        case 1: {
            // Head bump: RBJ band-pass at 90 Hz, Q 0.9, 44.1 kHz (constant-0-dB peak).
            constexpr float b0 = 0.0070732f, a1 = -1.9856903f, a2 = 0.9858536f;
            constexpr float kRoll = 0.76f;   // one-pole ~10 kHz corner, a soft tape top
            float z1 = s.b1, z2 = s.b2, lp = s.lp;
            for (uint32_t i = 0; i < n; ++i) {
                const float u = colorSin(x[i]);
                // Direct form II transposed band-pass (b1 = 0, b2 = -b0).
                const float y = b0 * u + z1;
                z1 = -a1 * y + z2;
                z2 = -b0 * u - a2 * y;
                const float v = u + 0.25f * y;
                lp += kRoll * (v - lp);
                x[i] = lp;
            }
            s.b1 = z1;
            s.b2 = z2;
            s.lp = lp;
            break;
        }
        case 2:
            for (uint32_t i = 0; i < n; ++i) {
                float u = x[i];
                const float a = u < 0.0f ? -u : u;
                if (a > 1.2533141f) u = u < 0.0f ? -1.2533141f : 1.2533141f;   // sqrt(pi / 2)
                const float aa = u < 0.0f ? -u : u;
                x[i] = aa > 1e-6f ? colorSin(u * aa) / aa : u;
            }
            break;
        case 3:
            for (uint32_t i = 0; i < n; ++i) {
                const float u = x[i];
                const float a = colorSin(u < 0.0f ? -u : u);
                x[i] = u < 0.0f ? -a : a;
            }
            break;
        case 4: {
            float prev = s.prev;
            for (uint32_t i = 0; i < n; ++i) {
                const float u = x[i];
                const float sn = colorSin(u);
                float apply = prev + sn;
                apply = (apply < 0.0f ? -apply : apply) * 0.5f;
                apply = apply > 1.0f ? 1.0f : apply;
                x[i] = u * (1.0f - apply) + sn * apply;
                prev = sn;
            }
            s.prev = prev;
            break;
        }
        default: break;
    }
}

}  // namespace audio
}  // namespace ot
