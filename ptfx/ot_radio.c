// PTFX_NAME: OT Radio
// PTFX_AUTHOR: OpenTracker
// PTFX_FLAGS: EFFECT, STEREO
// PTFX_SCRATCH: 0
/* OpenTracker semi fork: the Sample Editor's Radio (CIPHER) as a .ptfx
 * plugin. A shortwave receiver: the band filter, QSB fading with the noise
 * rising in the dips (AGC), a heterodyne whistle, band noise and static
 * crashes. Both channels get the same fades and crashes. */
#include "ot_common.h"

__attribute__((unused)) static const PluginParam PLUGIN_PARAMS[8] = {
    PTFX_PARAM_LIST("Band", 1, "2.4 kHz|3 kHz|4 kHz|6 kHz"),
    PTFX_PARAM_INT("Fading", 0.0f, 100.0f, 60.0f, 1.0f, "%"),
    PTFX_PARAM_INT("Rate", 0.0f, 100.0f, 30.0f, 1.0f, "%"),
    PTFX_PARAM_INT("Whistle", 0.0f, 100.0f, 15.0f, 1.0f, "%"),
    PTFX_PARAM_INT("Pitch", 100.0f, 3000.0f, 800.0f, 10.0f, "Hz"),
    PTFX_PARAM_INT("Noise", 0.0f, 100.0f, 25.0f, 1.0f, "%"),
    PTFX_PARAM_INT("Crashes", 0.0f, 100.0f, 20.0f, 1.0f, "%"),
    PTFX_PARAM_INT("Mix", 0.0f, 100.0f, 100.0f, 1.0f, "%"),
};

typedef struct {
    float b0, b1, b2, a1, a2, x1, x2, y1, y2;
} Biquad;
typedef struct {
    Biquad hp, lp1, lp2;
} Band;
typedef struct {
    Band sig, nb, cb;
    uint32_t rng;
    float crash, ph1, ph2, ph3, wph;
} Chan;

static float s_p[8];
static Chan s_c[OT_MAX_CH];

static void bqSet(Biquad* q, int high, float fc, float qq) {
    const float w = 2.0f * OT_PI * fc / 44100.0f;
    const float c = H->cosf(w), al = H->sinf(w) / (2.0f * qq);
    const float a0 = 1.0f + al;
    if (high) {
        q->b0 = (1.0f + c) / 2.0f / a0;
        q->b1 = -(1.0f + c) / a0;
    } else {
        q->b0 = (1.0f - c) / 2.0f / a0;
        q->b1 = (1.0f - c) / a0;
    }
    q->b2 = q->b0;
    q->a1 = -2.0f * c / a0;
    q->a2 = (1.0f - al) / a0;
    q->x1 = q->x2 = q->y1 = q->y2 = 0.0f;
}
static float bqRun(Biquad* q, float x) {
    const float y = q->b0 * x + q->b1 * q->x1 + q->b2 * q->x2 - q->a1 * q->y1 - q->a2 * q->y2;
    q->x2 = q->x1;
    q->x1 = x;
    q->y2 = q->y1;
    q->y1 = y;
    return y;
}
static void bandSet(Band* b, float bw) {
    bqSet(&b->hp, 1, 300.0f, 0.707f);
    bqSet(&b->lp1, 0, bw, 0.6f);
    bqSet(&b->lp2, 0, bw, 0.9f);
}
static float bandRun(Band* b, float x) { return bqRun(&b->lp2, bqRun(&b->lp1, bqRun(&b->hp, x))); }
static float uni(uint32_t* s) {   // [-1, 1)
    *s = *s * 1664525u + 1013904223u;
    return (float)(int32_t)*s * (1.0f / 2147483648.0f);
}

static void setup(void) {
    static const float kBw[4] = {2400.0f, 3000.0f, 4000.0f, 6000.0f};
    const float bw = kBw[ot_clampi((int)(s_p[0] + 0.5f), 0, 3)];
    int ch;
    for (ch = 0; ch < OT_MAX_CH; ++ch) {
        Chan* c = &s_c[ch];
        bandSet(&c->sig, bw);
        bandSet(&c->nb, bw);
        bandSet(&c->cb, bw);
        c->rng = 0x5EED1234u;
        c->crash = 0.0f;
        c->ph1 = 1.3f / (2.0f * OT_PI);
        c->ph2 = 4.1f / (2.0f * OT_PI);
        c->ph3 = 0.0f;
        c->wph = 0.0f;
    }
}

void plugin_init(void* scratch_mem, uint32_t scratch_size, const PtfxHostApi* host) {
    (void)scratch_mem;
    (void)scratch_size;
    H = host;
    s_p[0] = 1.0f;
    s_p[1] = 60.0f;
    s_p[2] = 30.0f;
    s_p[3] = 15.0f;
    s_p[4] = 800.0f;
    s_p[5] = 25.0f;
    s_p[6] = 20.0f;
    s_p[7] = 100.0f;
    setup();
}

void plugin_set_param(uint8_t index, float value) {
    if (index < 8) s_p[index] = value;
    if (index == 0) setup();
}

int32_t plugin_process_block(const int16_t** in, int16_t** out, uint32_t samples, uint8_t channels) {
    const float depth = s_p[1] / 100.0f, rate = s_p[2] / 100.0f, whistle = s_p[3] / 100.0f, pitch = s_p[4];
    const float noise = s_p[5] / 100.0f, crashes = s_p[6] / 100.0f, mix = s_p[7] / 100.0f;
    const float f1 = (0.05f + rate * 1.2f) / 44100.0f, f2 = f1 * 0.37f;   // QSB: two slow LFOs
    const float crashDecay = H->expf(-1.0f / (0.03f * 44100.0f));
    const float crashChance = crashes * 3.0f / 44100.0f;   // up to ~3 per second
    uint8_t ch;
    if (!out) return 0;
    for (ch = 0; ch < channels && ch < OT_MAX_CH; ++ch) {
        Chan* c = &s_c[ch];
        const int16_t* src = ot_in(in, ch);
        int16_t* dst = out[ch];
        uint32_t i;
        if (!dst) continue;
        for (i = 0; i < samples; ++i) {
            const float x = src ? (float)src[i] / 32768.0f : 0.0f;
            float lfo = 0.5f + 0.35f * H->sinf(2.0f * OT_PI * c->ph1) + 0.25f * H->sinf(2.0f * OT_PI * c->ph2);
            float fade, agc, nz, cr, drift, wh, y;
            lfo = ot_clampf(lfo, 0.0f, 1.0f);
            fade = 1.0f - depth * lfo;
            agc = 1.0f + 1.5f * (1.0f - fade);   // the receiver turns up as the signal fades
            if (0.5f * (uni(&c->rng) + 1.0f) < crashChance) c->crash = 0.4f + 0.5f * 0.5f * (uni(&c->rng) + 1.0f);
            c->crash *= crashDecay;
            nz = bandRun(&c->nb, uni(&c->rng)) * noise * 0.35f * agc;
            cr = bandRun(&c->cb, uni(&c->rng)) * c->crash * 1.2f;
            drift = pitch + 2.0f * H->sinf(2.0f * OT_PI * c->ph3);
            c->wph += drift / 44100.0f;
            if (c->wph >= 1.0f) c->wph -= 1.0f;
            wh = whistle * 0.2f * H->sinf(2.0f * OT_PI * c->wph);
            y = bandRun(&c->sig, x) * fade * (1.0f + 0.5f * (1.0f - fade)) + nz + cr + wh;
            y = H->tanhf(y * 1.2f);
            dst[i] = ot_sat((mix * y + (1.0f - mix) * x) * 32767.0f);
            c->ph1 += f1;
            if (c->ph1 >= 1.0f) c->ph1 -= 1.0f;
            c->ph2 += f2;
            if (c->ph2 >= 1.0f) c->ph2 -= 1.0f;
            c->ph3 += 0.13f / 44100.0f;
            if (c->ph3 >= 1.0f) c->ph3 -= 1.0f;
        }
    }
    return (int32_t)samples;
}
