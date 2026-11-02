// PTFX_NAME: Stereo to Mono
// PTFX_AUTHOR: Polyend
// PTFX_FLAGS: STEREO, CONVERT_TO_MONO
// PTFX_SCRATCH: 0

#include "tracker_plugin.h"

// Knobs: Source Channel (String List) & Gain Trim
const PluginParam PLUGIN_PARAMS[2] = {
    PTFX_PARAM_LIST("Source", 0, "Merge L+R|Left Only|Right Only|Side L-R"),
    PTFX_PARAM_FLOAT("Gain", -12.0f, 6.0f, 0.0f, 0.5f, "dB")
};

static const PtfxHostApi* s_host = 0;
static uint8_t s_mode = 0;
static float   s_gain = 1.0f;

void plugin_init(void* scratch_mem, uint32_t scratch_size, const PtfxHostApi* host) {
    (void)scratch_mem;
    (void)scratch_size;
    s_host = host;
    s_mode = 0;
    s_gain = 1.0f;
}

void plugin_set_param(uint8_t index, float value) {
    if (index == 0) {
        s_mode = (uint8_t)value;
    } else if (index == 1) {
        if (value == 0.0f) {
            s_gain = 1.0f;
        } else if (s_host && s_host->powf) {
            s_gain = s_host->powf(10.0f, value / 20.0f);
        }
    }
}

// Processes 1 channel on MK66, or converts 2 channels on Tracker Mini / Tracker+
int32_t plugin_process_block(const int16_t** in, int16_t** out, uint32_t samples, uint8_t channels) {
    if (!in || !out || !in[0] || !out[0]) {
        return 0;
    }

    // Single-channel source or identical input channels (mono)
    if (channels < 2 || !in[1] || in[0] == in[1]) {
        for (uint32_t i = 0; i < samples; i++) {
            int16_t s = (s_gain == 1.0f) ? in[0][i] : (int16_t)SATURATE_16BIT((int32_t)(in[0][i] * s_gain));
            out[0][i] = s;
            if (channels > 1 && out[1]) {
                out[1][i] = s;
            }
        }
        return samples;
    }

    const int16_t* left  = in[0];
    const int16_t* right = in[1];
    int16_t* out_l = out[0];
    int16_t* out_r = out[1] ? out[1] : out[0];

    for (uint32_t i = 0; i < samples; i++) {
        int32_t mono_val = left[i];

        switch (s_mode) {
            case 0: // Merge (L + R) / 2
                mono_val = ((int32_t)left[i] + (int32_t)right[i]) / 2;
                break;
            case 1: // Keep Left Only (discard Right channel)
                mono_val = left[i];
                break;
            case 2: // Keep Right Only (discard Left channel)
                mono_val = right[i];
                break;
            case 3: // Side / Difference (L - R) / 2
                mono_val = ((int32_t)left[i] - (int32_t)right[i]) / 2;
                break;
            default:
                mono_val = left[i];
                break;
        }

        int16_t pcm = (s_gain == 1.0f) ? (int16_t)mono_val : (int16_t)SATURATE_16BIT((int32_t)(mono_val * s_gain));
        out_l[i] = pcm;
        out_r[i] = pcm;
    }

    return samples;
}
