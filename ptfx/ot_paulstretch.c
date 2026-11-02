// PTFX_NAME: OT Paulstretch
// PTFX_AUTHOR: OpenTracker
// PTFX_FLAGS: EFFECT, STEREO
// PTFX_SCRATCH: 1048576
/* OpenTracker semi fork: the Sample Editor's Paulstretch (after Paul
 * Nasca's algorithm) as a .ptfx plugin: windows of the input, their
 * magnitudes kept and phases randomised, overlap-added at a slower pace.
 * Two windows per FFT (frame A real, frame B imaginary), Hann analysis and
 * sine synthesis windows, hop N / 2, as the built-in effect.
 *
 * As a plugin it sees the input as the output passes it: the input is kept
 * in the scratch memory as it comes (the output reads it at 1 / Stretch of
 * the pace, so it is always there, except for part of the very first
 * 200 ms window). The result is asked for as appended samples and ends at
 * Stretch x the selection (or what the scratch memory holds of it). */
#define OT_NEED_FFT
#include "ot_common.h"

__attribute__((unused)) static const PluginParam PLUGIN_PARAMS[2] = {
    PTFX_PARAM_INT("Stretch", 2.0f, 32.0f, 8.0f, 1.0f, "x"),
    PTFX_PARAM_LIST("Window", 2, "50 ms|100 ms|200 ms"),
};

#define NMAX 8192
typedef struct {
    float* acc;     /* 1.5 N: a pair's output, the second half carried over */
    int16_t* rdy;   /* N: the last pair's finished output */
    int16_t* buf;   /* the input so far (cap samples) */
    int32_t rdyStart;
    uint32_t rng;
    int frame;
} Chan;

static float s_p[2];
static uint8_t* s_mem;
static uint32_t s_memBytes;
static float *s_re, *s_im, *s_win;
static Chan s_c[OT_MAX_CH];
static int s_N, s_ch;          /* the layout made for N and channel count */
static uint32_t s_cap;         /* input samples kept per channel */
static uint32_t s_opos, s_recv; /* output position; input received (absolute) */
static int s_ended;
static uint32_t s_len;         /* the selection's length, once it ended */

static int windowN(void) {
    static const int kN[3] = {2048, 4096, 8192};
    return kN[ot_clampi((int)(s_p[1] + 0.5f), 0, 2)];
}
static float stretch(void) { return ot_clampf((float)(int)(s_p[0] + 0.5f), 2.0f, 32.0f); }

/* Scratch layout for N and the channels: re, im, window; per channel acc,
 * ready and the input. Returns false when even the work buffers do not fit. */
static int layout(int N, int channels) {
    uint32_t work, per, i;
    uint8_t* p = s_mem;
    int ch;
    if (channels < 1) channels = 1;
    if (channels > OT_MAX_CH) channels = OT_MAX_CH;
    work = (uint32_t)N * 3u * 4u;
    per = (uint32_t)N * 6u + (uint32_t)N * 2u; /* acc 1.5 N floats, ready N int16 */
    if (!s_mem || s_memBytes < work + per * (uint32_t)channels + 1024u) return 0;
    s_re = (float*)p;
    s_im = s_re + N;
    s_win = s_im + N;
    p += work;
    for (ch = 0; ch < channels; ++ch) {
        s_c[ch].acc = (float*)p;
        s_c[ch].rdy = (int16_t*)(p + (uint32_t)N * 6u);
        p += per;
    }
    s_cap = (s_memBytes - work - per * (uint32_t)channels) / 2u / (uint32_t)channels & ~1u;
    for (ch = 0; ch < channels; ++ch) s_c[ch].buf = (int16_t*)p + (uint32_t)ch * s_cap;
    for (i = 0; i < (uint32_t)N; ++i) s_win[i] = H->sinf(OT_PI * (float)i / (float)N);
    s_N = N;
    s_ch = channels;
    return 1;
}

static void restart(void) {
    int ch, i;
    for (ch = 0; ch < s_ch; ++ch) {
        Chan* c = &s_c[ch];
        for (i = 0; i < s_N + s_N / 2; ++i) c->acc[i] = 0.0f;
        c->rdyStart = -2 * s_N; /* nothing ready */
        c->rng = 0x5EED1234u;
        c->frame = 0;
    }
    s_opos = s_recv = 0;
    s_ended = 0;
    s_len = 0;
}

static void extra(void) {
    /* The result as appended samples: (Stretch - 1) x what the scratch holds,
     * within the editors' longest result; the run ends early by itself. */
    uint32_t cap = s_cap ? s_cap : 0u, app;
    const uint32_t s = (uint32_t)stretch();
    if (!H || !H->set_extra_samples) return;
    if (cap > OT_EDIT_MAX / 2u) cap = OT_EDIT_MAX / 2u;
    app = cap * (s - 1u);
    if (app > OT_EDIT_MAX - cap) app = OT_EDIT_MAX - cap;
    H->set_extra_samples(0, app);
}

void plugin_init(void* scratch_mem, uint32_t scratch_size, const PtfxHostApi* host) {
    H = host;
    s_p[0] = 8.0f;
    s_p[1] = 2.0f;
    s_mem = (uint8_t*)scratch_mem;
    s_memBytes = scratch_size;
    s_N = 0;
    s_ch = 0;
    s_cap = 0;
    if (layout(windowN(), 1)) restart();
    extra();
}

void plugin_set_param(uint8_t index, float value) {
    if (index < 2) s_p[index] = value;
    if (index == 1) {
        s_N = 0;
        if (layout(windowN(), 1)) restart();
    }
    extra();
}

static float uni(uint32_t* s) {
    *s ^= *s << 13;
    *s ^= *s >> 17;
    *s ^= *s << 5;
    return (float)(*s >> 8) * (1.0f / 16777216.0f);
}

/* The input at absolute position k (silence outside what is here). */
static float inAt(const Chan* c, int32_t k, uint32_t have) {
    return k >= 0 && (uint32_t)k < have ? (float)c->buf[k] : 0.0f;
}

static void pair(Chan* c, uint32_t have) {
    const int N = s_N, hop = N / 2;
    const float inStep = (float)hop / stretch();
    const float gain = 1.6329932f / (float)N; /* sqrt(8/3): Hann^2 averages 3/8; 1/N for the inverse FFT */
    const int32_t a0 = (int32_t)((float)c->frame * inStep) - N / 2;
    const int32_t b0 = (int32_t)((float)(c->frame + 1) * inStep) - N / 2;
    int i, k;
    for (i = 0; i < N; ++i) {
        const float w = s_win[i] * s_win[i];
        s_re[i] = inAt(c, a0 + i, have) * w;
        s_im[i] = inAt(c, b0 + i, have) * w;
    }
    ot_fft(s_re, s_im, N, 0);
    s_re[0] = s_im[0] = 0.0f; /* DC and Nyquist dropped */
    s_re[N / 2] = s_im[N / 2] = 0.0f;
    for (k = 1; k < N / 2; ++k) {
        const float zr = s_re[k], zi = s_im[k], wr = s_re[N - k], wi = s_im[N - k];
        /* A = (Z[k] + conj Z[N-k]) / 2, B = (Z[k] - conj Z[N-k]) / 2i */
        const float ar = 0.5f * (zr + wr), ai = 0.5f * (zi - wi);
        const float br = 0.5f * (zi + wi), bi = 0.5f * (wr - zr);
        float u = uni(&c->rng) * 2.0f - 1.0f, v = uni(&c->rng) * 2.0f - 1.0f, q = u * u + v * v;
        float u2, v2, q2, sa, sb, yar, yai, ybr, ybi;
        if (q < 1e-6f) u = 1.0f, v = 0.0f, q = 1.0f;
        sa = H->sqrtf((ar * ar + ai * ai) / q);
        yar = sa * u;
        yai = sa * v;
        u2 = uni(&c->rng) * 2.0f - 1.0f;
        v2 = uni(&c->rng) * 2.0f - 1.0f;
        q2 = u2 * u2 + v2 * v2;
        if (q2 < 1e-6f) u2 = 1.0f, v2 = 0.0f, q2 = 1.0f;
        sb = H->sqrtf((br * br + bi * bi) / q2);
        ybr = sb * u2;
        ybi = sb * v2;
        s_re[k] = yar - ybi; /* Z' = YA + i YB at k, conj(YA) + i conj(YB) at N - k */
        s_im[k] = yai + ybr;
        s_re[N - k] = yar + ybi;
        s_im[N - k] = ybr - yai;
    }
    ot_fft(s_re, s_im, N, 1);
    for (i = 0; i < N; ++i) {
        const float w = s_win[i] * gain;
        c->acc[i] += s_re[i] * w;       /* frame A at 0 */
        c->acc[hop + i] += s_im[i] * w; /* frame B half a window later */
    }
    for (i = 0; i < N; ++i) c->rdy[i] = ot_sat(c->acc[i]);
    for (i = 0; i < hop; ++i) c->acc[i] = c->acc[N + i];
    for (i = hop; i < N + hop; ++i) c->acc[i] = 0.0f;
    c->rdyStart = c->frame * hop - N / 2;
    c->frame += 2;
}

int32_t plugin_process_block(const int16_t** in, int16_t** out, uint32_t samples, uint8_t channels) {
    const int N = s_N, hop = N / 2;
    const uint32_t B = s_opos;
    uint32_t n = samples, endOut = 0xFFFFFFFFu, ch, i;
    const int hasIn = in && in[0];
    if (!out || !out[0] || samples == 0 || N == 0) return 0;
    if (channels > OT_MAX_CH) channels = OT_MAX_CH;
    if (s_ch != channels) {   /* first block: the layout for the real channel count */
        if (!layout(N, channels)) return 0;
        restart();
    }
    /* Keep the input as it comes (the host may give part of it again). */
    if (hasIn) {
        for (ch = 0; ch < channels; ++ch) {
            const int16_t* src = in[ch] ? in[ch] : in[0];
            for (i = 0; i < samples && B + i < s_cap; ++i) s_c[ch].buf[B + i] = src[i];
        }
        if (B + samples > s_recv) s_recv = B + samples;
    } else if (!s_ended) {
        s_ended = 1;
        s_len = s_recv < s_cap ? s_recv : s_cap;
    }
    if (s_ended) {
        endOut = (uint32_t)((float)s_len * stretch());
        if (endOut <= B) return 0;
        if (n > endOut - B) n = endOut - B;
    }
    {
        const uint32_t have = s_ended ? s_len : (s_recv < s_cap ? s_recv : s_cap);
        const float inStep = (float)hop / stretch();
        for (i = 0; i < n; ++i) {
            const int32_t o = (int32_t)(B + i);
            while (o >= s_c[0].rdyStart + N) {
                /* The next pair reads up to here: wait for the input unless
                 * this block made nothing yet (the first window only). */
                const int32_t need = (int32_t)((float)(s_c[0].frame + 1) * inStep) + N / 2;
                if (!s_ended && need > (int32_t)have && i > 0) goto done;
                for (ch = 0; ch < channels; ++ch) pair(&s_c[ch], have);
            }
            for (ch = 0; ch < channels; ++ch)
                if (out[ch]) out[ch][i] = s_c[ch].rdy[o - s_c[ch].rdyStart];
        }
    }
done:
    s_opos = B + i;
    return (int32_t)i;
}
