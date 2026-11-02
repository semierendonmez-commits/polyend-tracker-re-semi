// PTFX_NAME: Add Silence
// PTFX_AUTHOR: Polyend
// PTFX_FLAGS: EFFECT, STEREO
// PTFX_SCRATCH: 0

#include "tracker_plugin.h"

// Parameters:
// 0: Where  - "Beginning" (0) or "End" (1)
// 1: Length - Silence duration in ms (10 ms .. 5000 ms, default 2500 ms)
const PluginParam PLUGIN_PARAMS[2] = {
    PTFX_PARAM_LIST("Where", 0, "Beginning|End"),
    PTFX_PARAM_INT("Length", 10.0f, 5000.0f, 2500.0f, 10.0f, " ms")
};

static const PtfxHostApi* s_host = 0;
static uint8_t s_where = 0;
static float   s_length_ms = 2500.0f;

static void update_extra_samples(void) {
    if (!s_host || !s_host->set_extra_samples) {
        return;
    }
    uint32_t rate = (s_host->sample_rate > 0) ? s_host->sample_rate : 44100;
    uint32_t silence_samples = (uint32_t)((s_length_ms * (float)rate) / 1000.0f + 0.5f);
    if (s_where == 0) {
        s_host->set_extra_samples(silence_samples, 0);
    } else {
        s_host->set_extra_samples(0, silence_samples);
    }
}

void plugin_init(void* scratch_mem, uint32_t scratch_size, const PtfxHostApi* host) {
    (void)scratch_mem;
    (void)scratch_size;
    s_host = host;
    s_where = 0;
    s_length_ms = 2500.0f;
    update_extra_samples();
}

void plugin_set_param(uint8_t index, float value) {
    if (index == 0) {
        s_where = (value >= 0.5f) ? 1 : 0;
    } else if (index == 1) {
        if (value < 10.0f) value = 10.0f;
        if (value > 5000.0f) value = 5000.0f;
        s_length_ms = value;
    }
    update_extra_samples();
}

int32_t plugin_process_block(const int16_t** in, int16_t** out, uint32_t samples, uint8_t channels) {
    if (!out || !out[0] || samples == 0) {
        return 0;
    }

    for (uint8_t c = 0; c < channels; c++) {
        int16_t* dst = out[c];
        if (!dst) {
            continue;
        }
        const int16_t* src = (in != 0) ? in[c] : 0;
        if (src != 0) {
            if (s_host && s_host->memcpy) {
                s_host->memcpy(dst, src, samples * sizeof(int16_t));
            } else {
                for (uint32_t i = 0; i < samples; i++) {
                    dst[i] = src[i];
                }
            }
        } else {
            if (s_host && s_host->memset) {
                s_host->memset(dst, 0, samples * sizeof(int16_t));
            } else {
                for (uint32_t i = 0; i < samples; i++) {
                    dst[i] = 0;
                }
            }
        }
    }

    return (int32_t)samples;
}
