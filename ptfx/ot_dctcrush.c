// PTFX_NAME: OT DCT Crush
// PTFX_AUTHOR: OpenTracker
// PTFX_FLAGS: EFFECT, STEREO
// PTFX_SCRATCH: 0
/* OpenTracker semi fork: the Sample Editor's DCT Crush as a .ptfx plugin.
 * Short-time spectrum crush: each 50 %-overlap frame keeps only its strongest
 * bins, quantised to a few bits, and can freeze (the previous frame again):
 * low-bitrate MP3-like artefacts.
 *
 * A frame needs a whole window of input past the samples it finishes, so a
 * block returns all but the last window (whole hops); the host gives that
 * input again in the next block. A block shorter than a window and a hop is
 * the selection's end: the rest is padded with silence. */
#define OT_NEED_FFT
#include "ot_common.h"

__attribute__((unused)) static const PluginParam PLUGIN_PARAMS[5] = {
    PTFX_PARAM_LIST("Block", 2, "64|128|256|512"),
    PTFX_PARAM_INT("Keep", 1.0f, 100.0f, 12.0f, 1.0f, "%"),
    PTFX_PARAM_INT("Bits", 1.0f, 12.0f, 4.0f, 1.0f, ""),
    PTFX_PARAM_INT("Freeze", 0.0f, 100.0f, 0.0f, 1.0f, "%"),
    PTFX_PARAM_INT("Mix", 0.0f, 100.0f, 100.0f, 1.0f, "%"),
};

#define NMAX 512
typedef struct {
    float prevRe[NMAX], prevIm[NMAX], acc[NMAX + NMAX / 2];
    uint32_t rng;
    int started;
} Chan;

static float s_p[5];
static Chan s_c[OT_MAX_CH];
static float s_win[NMAX], s_re[NMAX], s_im[NMAX];
static int s_winN;

static float uni(uint32_t* s) {
    *s = *s * 1664525u + 1013904223u;
    return (float)(*s >> 8) * (1.0f / 16777216.0f);
}

static void reset(void) {
    int ch, i;
    for (ch = 0; ch < OT_MAX_CH; ++ch) {
        Chan* c = &s_c[ch];
        for (i = 0; i < NMAX; ++i) c->prevRe[i] = c->prevIm[i] = 0.0f;
        for (i = 0; i < NMAX + NMAX / 2; ++i) c->acc[i] = 0.0f;
        c->rng = 0x1234567u;
        c->started = 0;
    }
}

void plugin_init(void* scratch_mem, uint32_t scratch_size, const PtfxHostApi* host) {
    (void)scratch_mem;
    (void)scratch_size;
    H = host;
    s_p[0] = 2.0f;
    s_p[1] = 12.0f;
    s_p[2] = 4.0f;
    s_p[3] = 0.0f;
    s_p[4] = 100.0f;
    s_winN = 0;
    reset();
}

void plugin_set_param(uint8_t index, float value) {
    if (index < 5) s_p[index] = value;
}

/* One frame starting at src[0] (samples beyond `have` are silence) into the
 * channel's overlap-add buffer; then acc[0 .. hop) is finished. */
static void frame(Chan* c, const int16_t* src, int have, int N, float keep, int bits, float hold) {
    int i;
    const float levels = (float)((1 << (bits < 1 ? 1 : bits)) - 1);
    for (i = 0; i < N; ++i) {
        s_re[i] = (src && i < have && i >= 0 ? (float)src[i] : 0.0f) * s_win[i];
        s_im[i] = 0.0f;
    }
    ot_fft(s_re, s_im, N, 0);
    if (hold > 0.0f && uni(&c->rng) < hold) {
        for (i = 0; i < N; ++i) {
            s_re[i] = c->prevRe[i];
            s_im[i] = c->prevIm[i];
        }
    } else {
        /* Keep the strongest bins: threshold at the (1 - keep) quantile of the
         * magnitudes, found by a few bisection passes. */
        float mx = 0.0f, lo = 0.0f, hi, amax;
        const int want = (int)(keep * (float)(N / 2 + 1) + 0.5f);
        int it;
        for (i = 0; i <= N / 2; ++i) {
            const float m = s_re[i] * s_re[i] + s_im[i] * s_im[i];
            if (m > mx) mx = m;
        }
        hi = mx;
        for (it = 0; it < 16; ++it) {
            const float mid = 0.5f * (lo + hi);
            int cnt = 0;
            for (i = 0; i <= N / 2; ++i) cnt += s_re[i] * s_re[i] + s_im[i] * s_im[i] > mid;
            if (cnt > want) lo = mid;
            else hi = mid;
        }
        amax = H->sqrtf(mx);
        for (i = 0; i <= N / 2; ++i) {
            const float m2 = s_re[i] * s_re[i] + s_im[i] * s_im[i];
            if (m2 <= hi || amax <= 0.0f) {
                s_re[i] = s_im[i] = 0.0f;
            } else if (bits < 16) {
                /* Quantise the magnitude (relative to the frame peak), keep the phase. */
                const float m = H->sqrtf(m2);
                const float q = (float)(int)(m / amax * levels + 0.5f) / levels * amax;
                s_re[i] *= q / m;
                s_im[i] *= q / m;
            }
        }
        for (i = 1; i < N / 2; ++i) { /* Hermitian: a real frame */
            s_re[N - i] = s_re[i];
            s_im[N - i] = -s_im[i];
        }
        for (i = 0; i < N; ++i) {
            c->prevRe[i] = s_re[i];
            c->prevIm[i] = s_im[i];
        }
    }
    ot_fft(s_re, s_im, N, 1);
    for (i = 0; i < N; ++i) c->acc[i] += s_re[i] / (float)N * s_win[i];
}

static void shift(Chan* c, int N, int hop) {
    int i;
    for (i = 0; i < N; ++i) c->acc[i] = c->acc[i + hop];
    for (i = N; i < N + hop; ++i) c->acc[i] = 0.0f;
}

int32_t plugin_process_block(const int16_t** in, int16_t** out, uint32_t samples, uint8_t channels) {
    static const int kN[4] = {64, 128, 256, 512};
    const int N = kN[ot_clampi((int)(s_p[0] + 0.5f), 0, 3)], hop = N / 2;
    const float keep = s_p[1] / 100.0f, hold = s_p[3] / 100.0f, mix = s_p[4] / 100.0f;
    const int bits = ot_clampi((int)(s_p[2] + 0.5f), 1, 16);
    uint32_t done;
    uint8_t ch;
    if (!out || samples == 0) return 0;
    if (s_winN != N) {
        int i;
        for (i = 0; i < N; ++i) s_win[i] = H->sinf(OT_PI * ((float)i + 0.5f) / (float)N); /* sqrt-Hann */
        s_winN = N;
    }
    /* Whole hops whose frames have all their input here; at the end, all. */
    done = samples >= (uint32_t)(N + hop) ? (samples - (uint32_t)N) / (uint32_t)hop * (uint32_t)hop : samples;
    for (ch = 0; ch < channels && ch < OT_MAX_CH; ++ch) {
        Chan* c = &s_c[ch];
        const int16_t* src = ot_in(in, ch);
        int16_t* dst = out[ch];
        uint32_t o, i;
        if (!dst) continue;
        if (!c->started) {
            /* The frame at -hop: its second half is the first hop of input. */
            int k;
            {
                int16_t first[NMAX];
                for (k = 0; k < N; ++k) first[k] = (k >= hop && src && (uint32_t)(k - hop) < samples) ? src[k - hop] : 0;
                frame(c, first, N, N, keep, bits, hold);
            }
            shift(c, N, hop);
            c->started = 1;
        }
        for (o = 0; o < done; o += (uint32_t)hop) {
            const int have = (int)(samples - o);
            frame(c, src ? src + o : 0, have, N, keep, bits, hold);
            for (i = 0; i < (uint32_t)hop && o + i < done; ++i) {
                const float x = src ? (float)src[o + i] : 0.0f;
                dst[o + i] = ot_sat(x * (1.0f - mix) + c->acc[i] * mix);
            }
            shift(c, N, hop);
        }
    }
    return (int32_t)done;
}
