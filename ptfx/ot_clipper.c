// PTFX_NAME: OT Clipper
// PTFX_AUTHOR: OpenTracker
// PTFX_FLAGS: EFFECT, STEREO
// PTFX_SCRATCH: 0
/* OpenTracker semi fork: the Sample Editor's Clipper (NEXUSCLIP's seven
 * curves) as a .ptfx plugin: drive, a curve bounded at 0 dBFS, then the
 * ceiling. (The built-in version can match the peak first; a streaming
 * plugin cannot see the whole selection before it starts.) */
#include "ot_common.h"

__attribute__((unused)) static const PluginParam PLUGIN_PARAMS[5] = {
    PTFX_PARAM_LIST("Curve", 0, "Hard|Smooth|Crisp|Classic|Pro|Tube|Tape"),
    PTFX_PARAM_FLOAT("Drive", 0.0f, 24.0f, 6.0f, 0.5f, "dB"),
    PTFX_PARAM_INT("Knee", 0.0f, 100.0f, 30.0f, 1.0f, "%"),
    PTFX_PARAM_FLOAT("Ceiling", 0.0f, 12.0f, 0.0f, 0.5f, "-dB"),
    PTFX_PARAM_INT("Mix", 0.0f, 100.0f, 100.0f, 1.0f, "%"),
};

static float s_p[5];
static float s_dcx[OT_MAX_CH], s_dcy[OT_MAX_CH];

static float atanApprox(float x) {   // within ~0.005 rad
    const float a = x < 0.0f ? -x : x;
    float r;
    if (a <= 1.0f) r = a * (0.785398f + 0.273f * (1.0f - a));
    else {
        const float b = 1.0f / a;
        r = 1.5707963f - b * (0.785398f + 0.273f * (1.0f - b));
    }
    return x < 0.0f ? -r : r;
}

static float curve(int c, float x, float knee) {
    const float a = x < 0.0f ? -x : x, sg = x < 0.0f ? -1.0f : 1.0f;
    switch (c) {
        case 1: {   // Smooth: quadratic knee from (1 - s) to (1 + s)
            const float s = knee;
            float d;
            if (s <= 0.0001f || a <= 1.0f - s) return a >= 1.0f ? sg : x;
            if (a >= 1.0f + s) return sg;
            d = a - (1.0f - s);
            return sg * (a - d * d / (4.0f * s));
        }
        case 2: {   // Crisp: a short fixed knee from -2.5 dB, then a hard cut
            const float k = 0.75f;
            float y;
            if (a <= k) return x;
            if (a - k >= 2.0f * (1.0f - k)) return sg;
            y = k + (a - k) * (1.0f - (a - k) / (4.0f * (1.0f - k)));
            return sg * (y > 1.0f ? 1.0f : y);
        }
        case 3: return H->tanhf(x);   // Classic
        case 4: {                      // Pro: linear up to the knee, 2:1 in dB above it, bounded
            const float kk = knee < 0.25f ? 1.0f : (knee < 0.5f ? 0.75f : (knee < 0.75f ? 0.5f : 0.25f));
            float y;
            if (a <= kk) return x;
            y = kk * H->sqrtf(a / kk);
            return sg * (y > 1.0f ? 1.0f : y);
        }
        case 5: {   // Tube: asymmetric tanh (even harmonics); DC blocked after
            const float y = H->tanhf(1.2f * x + 0.3f) - H->tanhf(0.3f);
            return ot_clampf(y, -1.0f, 1.0f);
        }
        case 6: return 0.63661977f * atanApprox(1.5707963f * x);   // Tape: arctan shoulder
        default: return a >= 1.0f ? sg : x;                         // Hard
    }
}

void plugin_init(void* scratch_mem, uint32_t scratch_size, const PtfxHostApi* host) {
    int ch;
    (void)scratch_mem;
    (void)scratch_size;
    H = host;
    s_p[0] = 0.0f;
    s_p[1] = 6.0f;
    s_p[2] = 30.0f;
    s_p[3] = 0.0f;
    s_p[4] = 100.0f;
    for (ch = 0; ch < OT_MAX_CH; ++ch) s_dcx[ch] = s_dcy[ch] = 0.0f;
}

void plugin_set_param(uint8_t index, float value) {
    if (index < 5) s_p[index] = value;
}

int32_t plugin_process_block(const int16_t** in, int16_t** out, uint32_t samples, uint8_t channels) {
    const int c = ot_clampi((int)(s_p[0] + 0.5f), 0, 6);
    const float drive = H->powf(10.0f, s_p[1] / 20.0f);
    const float knee = s_p[2] / 100.0f;
    const float ceil = H->powf(10.0f, -s_p[3] / 20.0f);
    const float mix = s_p[4] / 100.0f;
    uint8_t ch;
    if (!out) return 0;
    for (ch = 0; ch < channels && ch < OT_MAX_CH; ++ch) {
        const int16_t* src = ot_in(in, ch);
        int16_t* dst = out[ch];
        uint32_t i;
        if (!dst) continue;
        for (i = 0; i < samples; ++i) {
            const float x = src ? (float)src[i] / 32768.0f : 0.0f;
            float y = curve(c, x * drive, knee);
            if (c == 5) {
                const float o = y - s_dcx[ch] + 0.995f * s_dcy[ch];
                s_dcx[ch] = y;
                s_dcy[ch] = o;
                y = o;
            }
            y = ot_clampf(y, -ceil, ceil);
            dst[i] = ot_sat((mix * y + (1.0f - mix) * x) * 32767.0f);
        }
    }
    return (int32_t)samples;
}
