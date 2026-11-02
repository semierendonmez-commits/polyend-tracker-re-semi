/* OpenTracker semi fork: shared helpers for the fork's .ptfx Sample Editor
 * plugins (built with third_party/polyend-ptfx-sdk/ptfx_builder.py).
 *
 * Plugin rules (the builder links position-independent code at 0 and never
 * relocates it): everything `static`, no pointers in initialised data, no
 * libc (the host table has the maths), no double, no 64-bit division, big
 * buffers in the scratch memory, not in .bss.
 *
 * The host feeds blocks of up to 8192 samples, the input pointer at the
 * output position: [selection][append, no input]. A block may return fewer
 * samples than it got; the host then gives the rest of the input again in
 * the next block (this is how the frame effects look ahead). 0 ends the run.
 */
#ifndef OT_PTFX_COMMON_H
#define OT_PTFX_COMMON_H

#include "tracker_plugin.h"

#define OT_MAX_CH 2
#define OT_PI 3.14159265f
#define OT_EDIT_MAX 1966080u /* the editors' longest result (both firmwares) */

static const PtfxHostApi* H;

static inline float ot_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline int ot_clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline int16_t ot_sat(float v) {
    if (!(v == v)) return 0;
    if (v >= 32767.0f) return 32767;
    if (v <= -32768.0f) return -32768;
    return (int16_t)(int32_t)v;
}
static inline const int16_t* ot_in(const int16_t** in, uint8_t ch) { return in ? in[ch] : 0; }

static inline uint32_t ot_hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

#ifdef OT_NEED_FFT
/* Radix-2 complex FFT in place (inverse unscaled). Stage twiddles from the
 * host's sin / cos, worked out once per size. */
static float s_fftCos[16], s_fftSin[16];
static int s_fftN;
static void ot_fft(float* re, float* im, int n, int inverse) {
    int i, j = 0, len, st;
    for (i = 1; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float t = re[i];
            re[i] = re[j];
            re[j] = t;
            t = im[i];
            im[i] = im[j];
            im[j] = t;
        }
    }
    if (s_fftN != n) {
        for (len = 2, st = 0; len <= n && st < 16; len <<= 1, ++st) {
            const float ang = -2.0f * OT_PI / (float)len;
            s_fftCos[st] = H->cosf(ang);
            s_fftSin[st] = H->sinf(ang);
        }
        s_fftN = n;
    }
    for (len = 2, st = 0; len <= n; len <<= 1, ++st) {
        const float wr = s_fftCos[st], wi = inverse ? -s_fftSin[st] : s_fftSin[st];
        for (i = 0; i < n; i += len) {
            float cr = 1.0f, ci = 0.0f;
            int k;
            for (k = 0; k < len / 2; ++k) {
                const int a = i + k, b = a + len / 2;
                const float xr = re[b] * cr - im[b] * ci, xi = re[b] * ci + im[b] * cr;
                float nr;
                re[b] = re[a] - xr;
                im[b] = im[a] - xi;
                re[a] += xr;
                im[a] += xi;
                nr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = nr;
            }
        }
    }
}
#endif

#endif
