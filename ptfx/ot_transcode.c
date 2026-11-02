// PTFX_NAME: OT Transcode
// PTFX_AUTHOR: OpenTracker
// PTFX_FLAGS: EFFECT, STEREO
// PTFX_SCRATCH: 0
/* OpenTracker semi fork: the Sample Editor's Transcode (TRANSCODE /
 * FRAMELOSS) as a .ptfx plugin. Telephone / radio codec generation loss:
 * down to a codec rate, then 1..16 tandem passes of G.711 (u-law, A-law) or
 * G.726 ADPCM (40/32/24/16 kbit/s, the ITU algorithm in the classic Sun g72x
 * structure), bit errors on the codes and lost 10 ms packets (silence,
 * repeat or glitch concealment). */
#include "ot_common.h"

__attribute__((unused)) static const PluginParam PLUGIN_PARAMS[7] = {
    PTFX_PARAM_LIST("Codec", 3, "u-law|A-law|G726 40k|G726 32k|G726 24k|G726 16k"),
    PTFX_PARAM_LIST("Rate", 0, "8 kHz|11 kHz|16 kHz|22 kHz"),
    PTFX_PARAM_INT("Gens", 1.0f, 16.0f, 1.0f, 1.0f, ""),
    PTFX_PARAM_INT("BitErr", 0.0f, 100.0f, 0.0f, 1.0f, "/10k"),
    PTFX_PARAM_INT("Loss", 0.0f, 50.0f, 0.0f, 1.0f, "%"),
    PTFX_PARAM_LIST("Conceal", 1, "Silence|Repeat|Glitch"),
    PTFX_PARAM_INT("Mix", 0.0f, 100.0f, 100.0f, 1.0f, "%"),
};

/* --- G.711 --------------------------------------------------------------- */
static uint8_t linearToUlaw(int16_t pcm) {
    int s = pcm, exp = 7, mask, mant;
    const int sign = (s >> 8) & 0x80;
    if (sign) s = -s;
    if (s > 32635) s = 32635;
    s += 0x84;
    for (mask = 0x4000; (s & mask) == 0 && exp > 0; mask >>= 1) --exp;
    mant = (s >> (exp + 3)) & 0x0F;
    return (uint8_t)~(sign | (exp << 4) | mant);
}
static int16_t ulawToLinear(uint8_t u) {
    int t;
    u = (uint8_t)~u;
    t = (((u & 0x0F) << 3) + 0x84) << ((u & 0x70) >> 4);
    return (int16_t)((u & 0x80) ? (0x84 - t) : (t - 0x84));
}
static uint8_t linearToAlaw(int16_t pcm) {
    static const int16_t kSegEnd[8] = {0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF};
    int s = pcm >> 3, mask, seg = 0, a;
    if (s >= 0) {
        mask = 0xD5;
    } else {
        mask = 0x55;
        s = -s - 1;
    }
    while (seg < 8 && s > kSegEnd[seg]) ++seg;
    if (seg >= 8) return (uint8_t)(0x7F ^ mask);
    a = seg << 4;
    a |= seg < 2 ? (s >> 1) & 0x0F : (s >> seg) & 0x0F;
    return (uint8_t)(a ^ mask);
}
static int16_t alawToLinear(uint8_t a) {
    int t, seg;
    a ^= 0x55;
    t = (a & 0x0F) << 4;
    seg = (a & 0x70) >> 4;
    if (seg == 0) t += 8;
    else if (seg == 1) t += 0x108;
    else t = (t + 0x108) << (seg - 1);
    return (int16_t)((a & 0x80) ? t : -t);
}

/* --- G.726 (Sun g72x structure) ------------------------------------------ */
static const int16_t kPower2[15] = {1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000};

static int quan(int val, const int16_t* table, int size) {
    int i = 0;
    while (i < size && val >= table[i]) ++i;
    return i;
}

typedef struct {
    int32_t yl;
    int16_t yu, dms, dml, ap;
    int16_t a[2], b[6], pk[2], dq[6], sr[2];
    uint8_t td;
} G72x;

static void g72xInit(G72x* s) {
    int i;
    s->yl = 34816;
    s->yu = 544;
    s->dms = s->dml = s->ap = 0;
    s->a[0] = s->a[1] = 0;
    s->pk[0] = s->pk[1] = 0;
    for (i = 0; i < 6; ++i) {
        s->b[i] = 0;
        s->dq[i] = 32;
    }
    s->sr[0] = s->sr[1] = 32;
    s->td = 0;
}

static int fmult(int an, int srn) {
    const int anmag = an > 0 ? an : ((-an) & 0x1FFF);
    const int anexp = quan(anmag, kPower2, 15) - 6;
    const int anmant = anmag == 0 ? 32 : (anexp >= 0 ? anmag >> anexp : anmag << -anexp);
    const int wanexp = anexp + ((srn >> 6) & 0xF) - 13;
    const int wanmant = (anmant * (srn & 077) + 0x30) >> 4;
    const int ret = wanexp >= 0 ? ((wanmant << wanexp) & 0x7FFF) : (wanmant >> -wanexp);
    return (an ^ srn) < 0 ? -ret : ret;
}
static int predictorZero(const G72x* s) {
    int sezi = fmult(s->b[0] >> 2, s->dq[0]), i;
    for (i = 1; i < 6; ++i) sezi += fmult(s->b[i] >> 2, s->dq[i]);
    return sezi;
}
static int predictorPole(const G72x* s) { return fmult(s->a[1] >> 2, s->sr[1]) + fmult(s->a[0] >> 2, s->sr[0]); }
static int stepSize(const G72x* s) {
    int y, dif, al;
    if (s->ap >= 256) return s->yu;
    y = (int)(s->yl >> 6);
    dif = s->yu - y;
    al = s->ap >> 2;
    if (dif > 0) y += (dif * al) >> 6;
    else if (dif < 0) y += (dif * al + 0x3F) >> 6;
    return y;
}
static int quantize(int d, int y, const int16_t* table, int size) {
    const int dqm = d < 0 ? -d : d;
    const int exp = quan(dqm >> 1, kPower2, 15);
    const int mant = ((dqm << 7) >> exp) & 0x7F;
    const int dl = (exp << 7) + mant;
    const int dln = dl - (y >> 2);
    const int i = quan(dln, table, size);
    if (size == 1) return d < 0 ? 3 - i : i;   /* 16 kbit/s: no zero rule */
    if (d < 0) return (size << 1) + 1 - i;
    if (i == 0) return (size << 1) + 1;
    return i;
}
static int reconstruct(int sign, int dqln, int y) {
    const int dql = dqln + (y >> 2);
    int dex, dqt, dq;
    if (dql < 0) return sign ? -0x8000 : 0;
    dex = (dql >> 7) & 15;
    dqt = 128 + (dql & 127);
    dq = (dqt << 7) >> (14 - dex);
    return sign ? dq - 0x8000 : dq;
}
static void update(int codeSize, int y, int wi, int fi, int dq, int sr, int dqsez, G72x* s) {
    const int pk0 = dqsez < 0 ? 1 : 0;
    int mag = dq & 0x7FFF, i, exp;
    const int ylint = (int)(s->yl >> 15), ylfrac = (int)((s->yl >> 10) & 0x1F);
    const int thr1 = (32 + ylfrac) << ylint;
    const int thr2 = ylint > 9 ? 31 << 10 : thr1;
    const int dqthr = (thr2 + (thr2 >> 1)) >> 1;
    const int tr = (s->td != 0 && mag > dqthr) ? 1 : 0;
    int yu = y + ((wi - y) >> 5);
    int a2p;
    yu = yu < 544 ? 544 : (yu > 5120 ? 5120 : yu);
    s->yu = (int16_t)yu;
    s->yl += s->yu + ((-s->yl) >> 6);
    a2p = s->a[1];
    if (tr == 1) {
        s->a[0] = s->a[1] = 0;
        for (i = 0; i < 6; ++i) s->b[i] = 0;
        a2p = 0;
    } else {
        const int pks1 = pk0 ^ s->pk[0];
        int a1, a1ul;
        a2p = s->a[1] - (s->a[1] >> 7);
        if (dqsez != 0) {
            const int fa1 = pks1 ? s->a[0] : -s->a[0];
            if (fa1 < -8191) a2p -= 0x100;
            else if (fa1 > 8191) a2p += 0xFF;
            else a2p += fa1 >> 5;
            if (pk0 ^ s->pk[1]) {
                if (a2p <= -12160) a2p = -12288;
                else if (a2p >= 12416) a2p = 12288;
                else a2p -= 0x80;
            } else if (a2p <= -12416) {
                a2p = -12288;
            } else if (a2p >= 12160) {
                a2p = 12288;
            } else {
                a2p += 0x80;
            }
        }
        s->a[1] = (int16_t)a2p;
        a1 = s->a[0] - (s->a[0] >> 8);
        if (dqsez != 0) a1 += pks1 == 0 ? 192 : -192;
        a1ul = 15360 - a2p;
        a1 = a1 < -a1ul ? -a1ul : (a1 > a1ul ? a1ul : a1);
        s->a[0] = (int16_t)a1;
        for (i = 0; i < 6; ++i) {
            int bi = s->b[i] - (codeSize == 5 ? (s->b[i] >> 9) : (s->b[i] >> 8));
            if (dq & 0x7FFF) bi += ((dq ^ s->dq[i]) >= 0) ? 128 : -128;
            s->b[i] = (int16_t)bi;
        }
    }
    for (i = 5; i > 0; --i) s->dq[i] = s->dq[i - 1];
    if (mag == 0) {
        s->dq[0] = (int16_t)(dq >= 0 ? 0x20 : 0xFC20);
    } else {
        exp = quan(mag, kPower2, 15);
        s->dq[0] = (int16_t)(dq >= 0 ? (exp << 6) + ((mag << 6) >> exp) : (exp << 6) + ((mag << 6) >> exp) - 0x400);
    }
    s->sr[1] = s->sr[0];
    if (sr == 0) {
        s->sr[0] = 0x20;
    } else if (sr > 0) {
        exp = quan(sr, kPower2, 15);
        s->sr[0] = (int16_t)((exp << 6) + ((sr << 6) >> exp));
    } else if (sr > -32768) {
        mag = -sr;
        exp = quan(mag, kPower2, 15);
        s->sr[0] = (int16_t)((exp << 6) + ((mag << 6) >> exp) - 0x400);
    } else {
        s->sr[0] = (int16_t)0xFC20;
    }
    s->pk[1] = s->pk[0];
    s->pk[0] = (int16_t)pk0;
    s->td = (tr == 1) ? 0 : (a2p < -11776 ? 1 : 0);
    s->dms = (int16_t)(s->dms + ((fi - s->dms) >> 5));
    s->dml = (int16_t)(s->dml + (((fi << 2) - s->dml) >> 7));
    if (tr == 1) {
        s->ap = 256;
    } else if (y < 1536 || s->td == 1) {
        s->ap = (int16_t)(s->ap + ((0x200 - s->ap) >> 4));
    } else {
        int diff = (s->dms << 2) - s->dml;
        if (diff < 0) diff = -diff;
        s->ap = (int16_t)(s->ap + (diff >= (s->dml >> 3) ? ((0x200 - s->ap) >> 4) : ((-s->ap) >> 4)));
    }
}

static const int16_t kQ16[1] = {261};
static const int16_t kDq16[4] = {116, 365, 365, 116};
static const int16_t kWi16[4] = {-704, 14048, 14048, -704};
static const int16_t kFi16[4] = {0, 0xE00, 0xE00, 0};
static const int16_t kQ24[3] = {8, 218, 331};
static const int16_t kDq24[8] = {-2048, 135, 273, 373, 373, 273, 135, -2048};
static const int16_t kWi24[8] = {-128, 960, 4384, 18624, 18624, 4384, 960, -128};
static const int16_t kFi24[8] = {0, 0x200, 0x400, 0xE00, 0xE00, 0x400, 0x200, 0};
static const int16_t kQ32[7] = {-124, 80, 178, 246, 300, 349, 400};
static const int16_t kDq32[16] = {-2048, 4, 135, 213, 273, 323, 373, 425, 425, 373, 323, 273, 213, 135, 4, -2048};
static const int16_t kWi32[16] = {-12, 18, 41, 64, 112, 198, 355, 1122, 1122, 355, 198, 112, 64, 41, 18, -12};
static const int16_t kFi32[16] = {0, 0, 0, 0x200, 0x200, 0x200, 0x600, 0xE00, 0xE00, 0x600, 0x200, 0x200, 0x200, 0, 0, 0};
static const int16_t kQ40[15] = {-122, -16, 68, 139, 198, 250, 298, 339, 378, 413, 445, 475, 502, 528, 553};
static const int16_t kDq40[32] = {-2048, -66, 28,  104, 169, 224, 274, 318, 358, 395, 429, 459, 488, 514, 539, 566,
                                  566,   539, 514, 488, 459, 429, 395, 358, 318, 274, 224, 169, 104, 28,  -66, -2048};
static const int16_t kWi40[32] = {448,  448,  768,  1248, 1280, 1312, 1856, 3200, 4512, 5728, 7008, 8960, 11456, 14080, 16928, 22272,
                                  22272, 16928, 14080, 11456, 8960, 7008, 5728, 4512, 3200, 1856, 1312, 1280, 1248, 768, 448, 448};
static const int16_t kFi40[32] = {0,     0,     0,     0,     0,     0x200, 0x200, 0x200, 0x200, 0x200, 0x400,
                                  0x600, 0x800, 0xA00, 0xC00, 0xC00, 0xC00, 0xC00, 0xA00, 0x800, 0x600, 0x400,
                                  0x200, 0x200, 0x200, 0x200, 0x200, 0,     0,     0,     0,     0};

/* No pointers in initialised data (position-independent code): picked at run time. */
typedef struct {
    int bits, qsize, wiShift;
    const int16_t *qtab, *dqln, *wi, *fi;
} Rate;
static void rateFor(int codec, Rate* r) {
    switch (codec) {
        case 2: r->bits = 5, r->qsize = 15, r->wiShift = 0, r->qtab = kQ40, r->dqln = kDq40, r->wi = kWi40, r->fi = kFi40; break;
        case 3: r->bits = 4, r->qsize = 7, r->wiShift = 5, r->qtab = kQ32, r->dqln = kDq32, r->wi = kWi32, r->fi = kFi32; break;
        case 4: r->bits = 3, r->qsize = 3, r->wiShift = 0, r->qtab = kQ24, r->dqln = kDq24, r->wi = kWi24, r->fi = kFi24; break;
        default: r->bits = 2, r->qsize = 1, r->wiShift = 0, r->qtab = kQ16, r->dqln = kDq16, r->wi = kWi16, r->fi = kFi16; break;
    }
}

static int g726Encode(G72x* s, const Rate* r, int sl) {
    int sezi, sez, se, d, y, i, dq, sr, dqsez;
    sl >>= 2; /* 14-bit */
    sezi = predictorZero(s);
    sez = sezi >> 1;
    se = (sezi + predictorPole(s)) >> 1;
    d = sl - se;
    y = stepSize(s);
    i = quantize(d, y, r->qtab, r->qsize);
    dq = reconstruct((i & (1 << (r->bits - 1))) != 0, r->dqln[i], y);
    sr = dq < 0 ? se - (dq & 0x3FFF) : se + dq;
    dqsez = sr + sez - se;
    update(r->bits, y, r->wi[i] * (1 << r->wiShift), r->fi[i], dq, sr, dqsez, s);
    return i;
}
static int g726Decode(G72x* s, const Rate* r, int i) {
    int sezi, sez, se, y, dq, sr, dqsez, out;
    i &= (1 << r->bits) - 1;
    sezi = predictorZero(s);
    sez = sezi >> 1;
    se = (sezi + predictorPole(s)) >> 1;
    y = stepSize(s);
    dq = reconstruct((i & (1 << (r->bits - 1))) != 0, r->dqln[i], y);
    sr = dq < 0 ? se - (dq & 0x3FFF) : se + dq;
    dqsez = sr - se + sez;
    update(r->bits, y, r->wi[i] * (1 << r->wiShift), r->fi[i], dq, sr, dqsez, s);
    out = sr * 4;
    return out > 32767 ? 32767 : (out < -32768 ? -32768 : out);
}

/* --- The chain ------------------------------------------------------------ */
#define MAX_GENS 16
typedef struct {
    G72x enc, dec;
    int16_t prev;
} Gen;
typedef struct {
    Gen g[MAX_GENS];
    uint32_t rng;
    int16_t last[256], cur[256];
    int pk, lost;
    float phase, acc, accN, y0, y1;
} Chan;

static float s_p[7];
static Chan s_c[OT_MAX_CH];

static float uni(uint32_t* s) {
    *s = *s * 1664525u + 1013904223u;
    return (float)(*s >> 8) * (1.0f / 16777216.0f);
}

static int16_t chain(Chan* c, int codec, int gens, float ber, int16_t x) {
    Rate r;
    int v = x, k;
    rateFor(codec, &r);
    for (k = 0; k < gens; ++k) {
        Gen* g = &c->g[k];
        int code, bits, b;
        if (k > 0) {
            /* Between passes a D/A - A/D hop: a half-sample shift and 8-bit u-law. */
            const int16_t in = (int16_t)v;
            v = ulawToLinear(linearToUlaw((int16_t)(((int)in + (int)g->prev) / 2)));
            g->prev = in;
        }
        if (codec <= 1) {
            code = codec == 0 ? linearToUlaw((int16_t)v) : linearToAlaw((int16_t)v);
            bits = 8;
        } else {
            code = g726Encode(&g->enc, &r, v);
            bits = r.bits;
        }
        if (ber > 0.0f)
            for (b = 0; b < bits; ++b)
                if (uni(&c->rng) < ber) code ^= 1 << b;
        if (codec <= 1) v = codec == 0 ? ulawToLinear((uint8_t)code) : alawToLinear((uint8_t)code);
        else v = g726Decode(&g->dec, &r, code);
    }
    return (int16_t)v;
}

static void reset(void) {
    int ch, k;
    for (ch = 0; ch < OT_MAX_CH; ++ch) {
        Chan* c = &s_c[ch];
        for (k = 0; k < MAX_GENS; ++k) {
            g72xInit(&c->g[k].enc);
            g72xInit(&c->g[k].dec);
            c->g[k].prev = 0;
        }
        for (k = 0; k < 256; ++k) c->last[k] = c->cur[k] = 0;
        c->rng = 0x1234567u;
        c->pk = 0;
        c->lost = 0;
        c->phase = 1.0f;
        c->acc = c->accN = c->y0 = c->y1 = 0.0f;
    }
}

void plugin_init(void* scratch_mem, uint32_t scratch_size, const PtfxHostApi* host) {
    (void)scratch_mem;
    (void)scratch_size;
    H = host;
    s_p[0] = 3.0f;
    s_p[1] = 0.0f;
    s_p[2] = 1.0f;
    s_p[3] = 0.0f;
    s_p[4] = 0.0f;
    s_p[5] = 1.0f;
    s_p[6] = 100.0f;
    reset();
}

void plugin_set_param(uint8_t index, float value) {
    if (index < 7) s_p[index] = value;
}

int32_t plugin_process_block(const int16_t** in, int16_t** out, uint32_t samples, uint8_t channels) {
    static const float kRateHz[4] = {8000.0f, 11025.0f, 16000.0f, 22050.0f};
    const int codec = ot_clampi((int)(s_p[0] + 0.5f), 0, 5);
    const float rate = kRateHz[ot_clampi((int)(s_p[1] + 0.5f), 0, 3)];
    const int gens = ot_clampi((int)(s_p[2] + 0.5f), 1, MAX_GENS);
    const float ber = s_p[3] / 10000.0f, loss = s_p[4] / 100.0f;
    const int conceal = ot_clampi((int)(s_p[5] + 0.5f), 0, 2);
    const float mix = s_p[6] / 100.0f;
    const int packet = (int)(rate / 100.0f);   /* 10 ms at the codec rate */
    const float step = rate / 44100.0f;       /* codec samples per input sample */
    uint8_t ch;
    if (!out) return 0;
    for (ch = 0; ch < channels && ch < OT_MAX_CH; ++ch) {
        Chan* c = &s_c[ch];
        const int16_t* src = ot_in(in, ch);
        int16_t* dst = out[ch];
        uint32_t i;
        if (!dst) continue;
        for (i = 0; i < samples; ++i) {
            const float x = src ? (float)src[i] : 0.0f;
            float wet;
            c->acc += x;
            c->accN += 1.0f;
            c->phase += step;
            if (c->phase >= 1.0f) {
                int16_t y;
                c->phase -= 1.0f;
                y = chain(c, codec, gens, ber, ot_sat(c->acc / c->accN));
                c->acc = c->accN = 0.0f;
                if (c->pk == 0) c->lost = loss > 0.0f && uni(&c->rng) < loss;
                c->cur[c->pk] = y;
                if (c->lost) {
                    if (conceal == 0) y = 0;
                    else if (conceal == 1) y = c->last[c->pk];
                    else y = c->last[(c->pk * 2) % packet]; /* glitch: the last packet at double speed */
                }
                if (++c->pk >= packet) {
                    int k;
                    c->pk = 0;
                    if (!c->lost)
                        for (k = 0; k < packet; ++k) c->last[k] = c->cur[k];
                }
                c->y0 = c->y1;
                c->y1 = (float)y;
            }
            wet = c->y0 + (c->y1 - c->y0) * c->phase;
            dst[i] = ot_sat(x * (1.0f - mix) + wet * mix);
        }
    }
    return (int32_t)samples;
}
