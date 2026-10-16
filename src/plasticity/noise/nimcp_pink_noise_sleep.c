#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_pink_noise_sleep.c - Sleep/Wake Pink Noise Integration
//=============================================================================

#include "plasticity/noise/nimcp_pink_noise_sleep.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_frequency_constants.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pink_noise_sleep)

pink_sleep_config_t pink_sleep_default_config(void) {
    pink_sleep_config_t config = {0};

    // Wake: Low noise, pure pink
    config.stages[PINK_SLEEP_WAKE].amplitude = 0.03f;
    config.stages[PINK_SLEEP_WAKE].alpha = 1.0f;
    config.stages[PINK_SLEEP_WAKE].spindle_probability = 0.0f;

    // Drowsy: Slightly increased
    config.stages[PINK_SLEEP_DROWSY].amplitude = 0.05f;
    config.stages[PINK_SLEEP_DROWSY].alpha = 0.95f;
    config.stages[PINK_SLEEP_DROWSY].spindle_probability = 0.0f;

    // N1: Light sleep
    config.stages[PINK_SLEEP_N1].amplitude = 0.08f;
    config.stages[PINK_SLEEP_N1].alpha = 0.9f;
    config.stages[PINK_SLEEP_N1].spindle_probability = 0.01f;

    // N2: Spindles
    config.stages[PINK_SLEEP_N2].amplitude = 0.1f;
    config.stages[PINK_SLEEP_N2].alpha = 1.1f;
    config.stages[PINK_SLEEP_N2].spindle_probability = 0.05f;
    config.stages[PINK_SLEEP_N2].spindle_frequency = 13.0f;

    // N3: Deep slow-wave
    config.stages[PINK_SLEEP_N3].amplitude = 0.15f;
    config.stages[PINK_SLEEP_N3].alpha = 1.5f;
    config.stages[PINK_SLEEP_N3].spindle_probability = 0.0f;

    // REM: Variable, whiter
    config.stages[PINK_SLEEP_REM].amplitude = 0.06f;
    config.stages[PINK_SLEEP_REM].alpha = 0.8f;
    config.stages[PINK_SLEEP_REM].spindle_probability = 0.0f;

    config.arousal_amplitude_gain = 0.5f;
    config.transition_rate = 0.01f;
    config.sample_rate = NIMCP_OSC_SAMPLE_RATE_HZ;
    config.seed = 0;

    return config;
}

pink_sleep_bridge_t* pink_sleep_create(const pink_sleep_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }

    pink_sleep_bridge_t* bridge = nimcp_calloc(1, sizeof(pink_sleep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    memcpy(&bridge->config, config, sizeof(pink_sleep_config_t));
    bridge->current_stage = PINK_SLEEP_WAKE;
    bridge->arousal_level = 1.0f;
    bridge->current_amplitude = config->stages[PINK_SLEEP_WAKE].amplitude;
    bridge->current_alpha = config->stages[PINK_SLEEP_WAKE].alpha;

    pink_noise_config_t noise_config = pink_noise_default_config();
    noise_config.amplitude = bridge->current_amplitude;
    noise_config.alpha = bridge->current_alpha;
    noise_config.seed = config->seed;
    bridge->noise_generator = pink_noise_create(&noise_config);

    NIMCP_LOGGING_INFO("Created sleep/wake pink noise bridge");
    return bridge;
}

void pink_sleep_destroy(pink_sleep_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->noise_generator) {
        pink_noise_destroy(bridge->noise_generator);
    }
    nimcp_free(bridge);
}

int pink_sleep_set_stage(pink_sleep_bridge_t* bridge, pink_sleep_stage_t stage) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_sleep_set_stage: bridge is NULL");
        return -1;
    }
    if (stage >= PINK_SLEEP_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "pink_sleep_set_stage: invalid stage value");
        return -1;
    }
    bridge->current_stage = stage;
    bridge->stage_duration = 0;
    return 0;
}

int pink_sleep_set_arousal(pink_sleep_bridge_t* bridge, float arousal) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_sleep_set_arousal: bridge is NULL");
        return -1;
    }
    bridge->arousal_level = fmaxf(0.0f, fminf(1.0f, arousal));
    return 0;
}

int pink_sleep_step(pink_sleep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_sleep_step: bridge is NULL");
        return -1;
    }

    pink_sleep_stage_params_t* target = &bridge->config.stages[bridge->current_stage];
    float rate = bridge->config.transition_rate;

    // Smooth transition of parameters
    bridge->current_amplitude += rate * (target->amplitude - bridge->current_amplitude);
    bridge->current_alpha += rate * (target->alpha - bridge->current_alpha);

    // Arousal modulates amplitude
    float arousal_factor = 1.0f - bridge->config.arousal_amplitude_gain *
                          (1.0f - bridge->arousal_level);
    bridge->current_amplitude *= arousal_factor;

    bridge->sample_count++;
    bridge->stage_duration++;

    return 0;
}

float pink_sleep_get_amplitude(const pink_sleep_bridge_t* bridge) {
    return bridge ? bridge->current_amplitude : 0.05f;
}

float pink_sleep_get_alpha(const pink_sleep_bridge_t* bridge) {
    return bridge ? bridge->current_alpha : 1.0f;
}

float pink_sleep_generate_sample(pink_sleep_bridge_t* bridge) {
    if (!bridge || !bridge->noise_generator) return 0.0f;

    pink_sleep_step(bridge);

    float sample;
    pink_noise_generate_sample(bridge->noise_generator, &sample);
    sample *= bridge->current_amplitude / 0.05f;  // Scale relative to default

    // Add spindle if in N2 and probability triggers
    pink_sleep_stage_params_t* params = &bridge->config.stages[bridge->current_stage];
    if (params->spindle_probability > 0.0f) {
        static uint32_t rng = NIMCP_LCG_INCREMENT;
        rng = rng * NIMCP_LCG_MULTIPLIER + NIMCP_LCG_INCREMENT;
        float r = (float)(rng % 10000) / 10000.0f;

        if (r < params->spindle_probability || bridge->in_spindle) {
            if (!bridge->in_spindle) {
                bridge->in_spindle = true;
                bridge->spindle_phase = 0.0f;
            }

            float spindle = 0.1f * sinf(NIMCP_TWO_PI_F * params->spindle_frequency *
                                        bridge->spindle_phase / bridge->config.sample_rate);
            sample += spindle;
            bridge->spindle_phase += 1.0f;

            if (bridge->spindle_phase > bridge->config.sample_rate * 0.5f) {
                bridge->in_spindle = false;
            }
        }
    }

    return sample;
}

const char* pink_sleep_stage_name(pink_sleep_stage_t stage) {
    switch (stage) {
        case PINK_SLEEP_WAKE: return "wake";
        case PINK_SLEEP_DROWSY: return "drowsy";
        case PINK_SLEEP_N1: return "N1";
        case PINK_SLEEP_N2: return "N2";
        case PINK_SLEEP_N3: return "N3";
        case PINK_SLEEP_REM: return "REM";
        default: return "unknown";
    }
}
