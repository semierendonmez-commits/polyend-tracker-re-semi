// PTFX_NAME: OT Granular
// PTFX_AUTHOR: OpenTracker
// PTFX_FLAGS: EFFECT, STEREO
// PTFX_SCRATCH: 1048576
/* OpenTracker semi fork: the Sample Editor's Granular (after Ourednik's
 * Argotlunar) as a .ptfx plugin: grains with size, density, position spray,
 * pitch spread and reverse probability, rendered over a stretched length.
 *
 * As a plugin it sees the input as the output passes it, so a grain only
 * reads input from before its own start (Length 1x and more; the spray is
 * in milliseconds). The input is kept in the scratch memory; the result is
 * asked for as appended samples and ends at Length x the selection. */
#include "ot_common.h"

__attribute__((unused)) static const PluginParam PLUGIN_PARAMS[6] = {
    PTFX_PARAM_FLOAT("Length", 1.0f, 8.0f, 2.0f, 0.25f, "x"),
    PTFX_PARAM_INT("Grain", 10.0f, 500.0f, 80.0f, 5.0f, "ms"),
    PTFX_PARAM_INT("Density", 2.0f, 200.0f, 30.0f, 1.0f, "/s"),
    PTFX_PARAM_INT("Spray", 0.0f, 2000.0f, 200.0f, 10.0f, "ms"),
    PTFX_PARAM_FLOAT("Pitch", 0.0f, 12.0f, 0.0f, 0.5f, "st"),
    PTFX_PARAM_INT("Reverse", 0.0f, 100.0f, 0.0f, 1.0f, "%"),
};

#define BLOCK 8192
static float s_p[6];
static uint8_t* s_mem;
static uint32_t s_memBytes;
static float* s_acc;          /* BLOCK */
static int16_t* s_buf[OT_MAX_CH];
static uint32_t s_cap, s_opos, s_recv, s_len;
static int s_ch, s_ended;

static float lengthX(void) { return ot_clampf(s_p[0], 1.0f, 8.0f); }

static int layout(int channels) {
    const uint32_t work = BLOCK * 4u;
    int ch;
    if (channels < 1) channels = 1;
    if (channels > OT_MAX_CH) channels = OT_MAX_CH;
    if (!s_mem || s_memBytes < work + 4096u) return 0;
    s_acc = (float*)s_mem;
    s_cap = (s_memBytes - work) / 2u / (uint32_t)channels & ~1u;
    for (ch = 0; ch < channels; ++ch) s_buf[ch] = (int16_t*)(s_mem + work) + (uint32_t)ch * s_cap;
    s_ch = channels;
    s_opos = s_recv = s_len = 0;
    s_ended = 0;
    return 1;
}

static void extra(void) {
    uint32_t cap = s_cap, app;
    if (!H || !H->set_extra_samples) return;
    if (cap > OT_EDIT_MAX / 2u) cap = OT_EDIT_MAX / 2u;
    app = (uint32_t)((float)cap * (lengthX() - 1.0f));
    if (app > OT_EDIT_MAX - cap) app = OT_EDIT_MAX - cap;
    H->set_extra_samples(0, app);
}

void plugin_init(void* scratch_mem, uint32_t scratch_size, const PtfxHostApi* host) {
    H = host;
    s_p[0] = 2.0f;
    s_p[1] = 80.0f;
    s_p[2] = 30.0f;
    s_p[3] = 200.0f;
    s_p[4] = 0.0f;
    s_p[5] = 0.0f;
    s_mem = (uint8_t*)scratch_mem;
    s_memBytes = scratch_size;
    s_ch = 0;
    s_cap = 0;
    layout(1);
    extra();
}

void plugin_set_param(uint8_t index, float value) {
    if (index < 6) s_p[index] = value;
    extra();
}

int32_t plugin_process_block(const int16_t** in, int16_t** out, uint32_t samples, uint8_t channels) {
    const float len = lengthX();
    uint32_t g = (uint32_t)(s_p[1] * 44.1f);
    const float period = 44100.0f / (s_p[2] < 1.0f ? 1.0f : s_p[2]);
    const float spray = s_p[3] * 44.1f, spread = s_p[4], rev = s_p[5] / 100.0f;
    float overlap, norm;
    const uint32_t B = s_opos;
    uint32_t n = samples, ch, i, limitAll;
    int32_t k;
    const int hasIn = in && in[0];
    if (!out || !out[0] || samples == 0) return 0;
    if (g < 64) g = 64;
    overlap = (float)g / period;
    norm = 1.0f / H->sqrtf(overlap < 1.0f ? 1.0f : overlap);
    if (channels > OT_MAX_CH) channels = OT_MAX_CH;
    if ((int)channels != s_ch && !layout(channels)) return 0;
    if (n > BLOCK) n = BLOCK;
    if (hasIn) {
        for (ch = 0; ch < channels; ++ch) {
            const int16_t* src = in[ch] ? in[ch] : in[0];
            for (i = 0; i < n && B + i < s_cap; ++i) s_buf[ch][B + i] = src[i];
        }
        if (B + n > s_recv) s_recv = B + n;
    } else if (!s_ended) {
        s_ended = 1;
        s_len = s_recv;
    }
    if (s_ended) {
        const uint32_t endOut = (uint32_t)((float)s_len * len);
        if (endOut <= B) return 0;
        if (n > endOut - B) n = endOut - B;
    }
    limitAll = s_ended ? (s_len < s_cap ? s_len : s_cap) : s_cap;
    for (ch = 0; ch < channels; ++ch) {
        const int16_t* src = s_buf[ch];
        for (i = 0; i < n; ++i) s_acc[i] = 0.0f;
        /* Grains k start near k * period (+ up to half a period of jitter). */
        k = (int32_t)(((float)B - (float)g - period) / period);
        if (k < 0) k = 0;
        for (;; ++k) {
            const uint32_t h = ot_hash((uint32_t)k * 0x9E3779B9u + 0x1234567u);
            const float jit = (float)(h & 0xFFFF) / 65536.0f * 0.5f * period;
            const int32_t start = (int32_t)((float)k * period + jit);
            uint32_t h2, h3, h4, limit, j;
            float pos, semis, ratio, srcLen;
            int back;
            if (start >= (int32_t)(B + n)) break;
            if (start + (int32_t)g <= (int32_t)B) continue;
            h2 = ot_hash(h ^ 0xA5A5A5A5u);
            h3 = ot_hash(h2);
            h4 = ot_hash(h3);
            /* Source: the input under the output time, sprayed; never past the
             * grain's own start (that input is here for every block it plays). */
            pos = (float)start / len + ((float)(h2 & 0xFFFF) / 65536.0f - 0.5f) * 2.0f * spray;
            semis = ((float)(h3 & 0xFFFF) / 65536.0f - 0.5f) * 2.0f * spread;
            ratio = H->powf(2.0f, semis / 12.0f);
            back = (float)(h4 & 0xFFFF) / 65536.0f < rev;
            srcLen = (float)g * ratio;
            limit = (uint32_t)start < limitAll ? (uint32_t)start : limitAll;
            if (pos + srcLen + 1.0f >= (float)limit) pos = (float)limit - srcLen - 2.0f;
            if (pos < 0.0f) continue; /* too early for a whole grain */
            for (j = 0; j < g; ++j) {
                const int32_t o = start + (int32_t)j;
                float w, sp, fr, a, b;
                int32_t i0;
                if (o < (int32_t)B) continue;
                if (o >= (int32_t)(B + n)) break;
                w = 0.5f - 0.5f * H->cosf(2.0f * OT_PI * (float)j / (float)g);
                sp = back ? pos + srcLen - (float)j * ratio : pos + (float)j * ratio;
                i0 = (int32_t)sp;
                fr = sp - (float)i0;
                a = (float)src[i0];
                b = (float)src[i0 + 1];
                s_acc[o - (int32_t)B] += (a + (b - a) * fr) * w * norm;
            }
        }
        if (out[ch])
            for (i = 0; i < n; ++i) out[ch][i] = ot_sat(s_acc[i]);
    }
    s_opos = B + n;
    return (int32_t)n;
}
