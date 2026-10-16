#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_physics_perception_bridge.c - Physics Layer to Perception Bridge
//=============================================================================

#include "utils/bridge/nimcp_bridge_base.h"
#include "physics/bridges/nimcp_physics_perception_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(physics_perception_bridge)

#define LOG_MODULE "PHYSICS_PERCEPTION_BRIDGE"


//=============================================================================
// Internal Constants
//=============================================================================

#define TEMP_REF_K          310.15f   /* 37°C reference temperature */
#define DEFAULT_Q10         2.5f      /* Q10 for phototransduction */

//=============================================================================
// Internal Structure
//=============================================================================

struct physics_percept_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    physics_percept_config_t config;

    /** Physics state */
    float temperature_k;
    float atp_level;

    /** Oscillation state */
    float gamma_power;
    float alpha_power;

    /** Computed binding state */
    physics_percept_binding_t binding;

    /** Attention state per modality */
    physics_percept_attention_t attention[4];

    /** Current gains (temperature/ATP adjusted) */
    float visual_gain;
    float auditory_gain;

    /** Timing */
    float sim_time_ms;

    /** Statistics */
    physics_percept_stats_t stats;

    bool initialized;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Compute Q10-based gain factor
 */
static float compute_temp_factor(float temperature_k, float q10) {
    float delta_t = (temperature_k - TEMP_REF_K) / 10.0f;
    return powf(q10, delta_t);
}

/**
 * @brief Compute ATP-based gain
 */
static float compute_atp_gain(float atp_level, float min_gain, float max_gain) {
    /* Linear interpolation between min and max */
    float clamped = (atp_level < 0.0f) ? 0.0f :
                    (atp_level > 1.0f) ? 1.0f : atp_level;
    return min_gain + clamped * (max_gain - min_gain);
}

/**
 * @brief Update internal gain values
 */
static void update_gains(physics_percept_bridge_t* bridge) {
    float temp_factor = compute_temp_factor(bridge->temperature_k,
                                             bridge->config.temp_q10);
    float atp_gain = compute_atp_gain(bridge->atp_level,
                                       bridge->config.atp_gain_min,
                                       bridge->config.atp_gain_max);

    /* Combined gain = temperature × ATP × attention */
    bridge->visual_gain = temp_factor * atp_gain *
                          bridge->attention[PHYSICS_PERCEPT_VISUAL].attention_gain;
    bridge->auditory_gain = temp_factor * atp_gain *
                            bridge->attention[PHYSICS_PERCEPT_AUDITORY].attention_gain;
}

/**
 * @brief Update binding state from oscillations
 */
static void update_binding(physics_percept_bridge_t* bridge) {
    /* Binding strength from gamma coherence */
    bridge->binding.gamma_coherence = bridge->gamma_power;
    bridge->binding.alpha_power = bridge->alpha_power;

    /* Active binding when gamma exceeds threshold */
    bridge->binding.binding_active =
        bridge->gamma_power >= bridge->config.gamma_binding_threshold;

    /* Binding strength modulated by alpha inhibition */
    if (bridge->binding.binding_active) {
        float alpha_inhibition = 1.0f - bridge->alpha_power *
                                 bridge->config.alpha_inhibition_gain;
        if (alpha_inhibition < 0.0f) alpha_inhibition = 0.0f;

        bridge->binding.binding_strength =
            bridge->gamma_power * alpha_inhibition;
    } else {
        bridge->binding.binding_strength = 0.0f;
    }
}

//=============================================================================
// Configuration API
//=============================================================================

int physics_percept_default_config(physics_percept_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_visual = true;
    config->enable_auditory = true;
    config->enable_binding = true;

    config->photo_tau_ms = PHYSICS_PERCEPT_PHOTO_TAU;
    config->audio_tau_ms = 10.0f;
    config->temp_q10 = DEFAULT_Q10;

    config->gamma_binding_threshold = 0.3f;
    config->alpha_inhibition_gain = 0.5f;

    config->atp_gain_min = 0.2f;
    config->atp_gain_max = 1.0f;
    config->atp_fatigue_threshold = 0.3f;

    config->update_interval_ms = 1.0f;

    return 0;
}

//=============================================================================
// Lifecycle API
//=============================================================================

physics_percept_bridge_t* physics_percept_bridge_create(
    const physics_percept_config_t* config
) {
    physics_percept_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate physics-perception bridge");
    if (bridge_base_init(&bridge->base, 0, "physics_perception") != 0) { nimcp_free(bridge); return NULL; }

    if (config) {
        bridge->config = *config;
    } else {
        physics_percept_default_config(&bridge->config);
    }

    /* Initialize physics state */
    bridge->temperature_k = TEMP_REF_K;
    bridge->atp_level = 1.0f;

    /* Initialize oscillations */
    bridge->gamma_power = 0.0f;
    bridge->alpha_power = 0.0f;

    /* Initialize attention to neutral */
    for (int i = 0; i < 4; i++) {
        bridge->attention[i].attention_gain = 1.0f;
        bridge->attention[i].target_modality = (physics_percept_modality_t)i;
        bridge->attention[i].spatial_focus_x = 0.5f;
        bridge->attention[i].spatial_focus_y = 0.5f;
        bridge->attention[i].focus_radius = 1.0f;
    }

    /* Compute initial gains */
    update_gains(bridge);

    bridge->initialized = true;

    NIMCP_LOG_INFO(PHYSICS_PERCEPT_MODULE_NAME,
        "Physics-perception bridge created: visual=%d, auditory=%d, binding=%d",
        bridge->config.enable_visual,
        bridge->config.enable_auditory,
        bridge->config.enable_binding);

    return bridge;
}

void physics_percept_bridge_destroy(physics_percept_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "physics_perception");

    NIMCP_LOG_INFO(PHYSICS_PERCEPT_MODULE_NAME,
        "Bridge destroyed - visual: %lu, auditory: %lu, binding_events: %lu",
        (unsigned long)bridge->stats.visual_inputs,
        (unsigned long)bridge->stats.auditory_inputs,
        (unsigned long)bridge->stats.binding_events);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

//=============================================================================
// Sensory Input API
//=============================================================================

float physics_percept_process_input(
    physics_percept_bridge_t* bridge,
    const physics_percept_input_t* input,
    physics_percept_output_t* output
) {
    if (!bridge || !input) return 0.0f;

    float gain = 1.0f;
    physics_percept_state_t state = PHYSICS_PERCEPT_STATE_NORMAL;

    /* Select modality-specific gain */
    switch (input->modality) {
        case PHYSICS_PERCEPT_VISUAL:
            if (!bridge->config.enable_visual) return 0.0f;
            gain = bridge->visual_gain;
            bridge->stats.visual_inputs++;
            bridge->stats.avg_visual_gain =
                0.99f * bridge->stats.avg_visual_gain + 0.01f * gain;
            break;

        case PHYSICS_PERCEPT_AUDITORY:
            if (!bridge->config.enable_auditory) return 0.0f;
            gain = bridge->auditory_gain;
            bridge->stats.auditory_inputs++;
            bridge->stats.avg_auditory_gain =
                0.99f * bridge->stats.avg_auditory_gain + 0.01f * gain;
            break;

        case PHYSICS_PERCEPT_SOMATOSENSORY:
        case PHYSICS_PERCEPT_MULTIMODAL:
            gain = (bridge->visual_gain + bridge->auditory_gain) / 2.0f;
            break;
    }

    /* Check for fatigue */
    if (bridge->atp_level < bridge->config.atp_fatigue_threshold) {
        state = PHYSICS_PERCEPT_STATE_DEGRADED;
        bridge->stats.fatigue_events++;
    }

    /* Check for attention enhancement */
    physics_percept_attention_t* att = &bridge->attention[input->modality];
    if (att->attention_gain > 1.2f) {
        state = PHYSICS_PERCEPT_STATE_ENHANCED;
    }

    /* Apply gain to input */
    float processed = input->spike_rate_hz * gain;

    /* Apply binding coherence for multimodal */
    float binding_coherence = 0.0f;
    if (bridge->config.enable_binding && bridge->binding.binding_active) {
        binding_coherence = bridge->binding.binding_strength;
        processed *= (1.0f + binding_coherence * 0.5f);
        bridge->stats.binding_events++;
    }

    /* Fill output if provided */
    if (output) {
        output->modality = input->modality;
        output->channel_id = input->channel_id;
        output->processed_value = processed;
        output->gain = gain;
        output->binding_coherence = binding_coherence;
        output->state = state;
    }

    return processed;
}

int physics_percept_process_batch(
    physics_percept_bridge_t* bridge,
    const physics_percept_input_t* inputs,
    physics_percept_output_t* outputs,
    uint32_t count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_percept_process_batch: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    int processed = 0;
    for (uint32_t i = 0; i < count; i++) {
        physics_percept_output_t* out = outputs ? &outputs[i] : NULL;
        physics_percept_process_input(bridge, &inputs[i], out);
        processed++;
    }

    return processed;
}

//=============================================================================
// Oscillation/Binding API
//=============================================================================

int physics_percept_set_oscillations(
    physics_percept_bridge_t* bridge,
    float gamma_power,
    float alpha_power
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->gamma_power = (gamma_power < 0.0f) ? 0.0f :
                          (gamma_power > 1.0f) ? 1.0f : gamma_power;
    bridge->alpha_power = (alpha_power < 0.0f) ? 0.0f :
                          (alpha_power > 1.0f) ? 1.0f : alpha_power;

    update_binding(bridge);

    bridge->stats.avg_binding_coherence =
        0.99f * bridge->stats.avg_binding_coherence +
        0.01f * bridge->binding.binding_strength;

    return 0;
}

int physics_percept_get_binding(
    const physics_percept_bridge_t* bridge,
    physics_percept_binding_t* binding
) {
    if (!bridge || !binding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_percept_get_binding: required parameter is NULL (bridge, binding)");
        return -1;
    }
    *binding = bridge->binding;
    return 0;
}

bool physics_percept_is_binding_active(const physics_percept_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->binding.binding_active;
}

//=============================================================================
// Attention Modulation API
//=============================================================================

int physics_percept_apply_attention(
    physics_percept_bridge_t* bridge,
    const physics_percept_attention_t* attention
) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_percept_apply_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    if (attention->target_modality >= 4) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_percept_apply_attention: capacity exceeded");
        return -1;
    }

    bridge->attention[attention->target_modality] = *attention;
    update_gains(bridge);

    NIMCP_LOG_DEBUG(PHYSICS_PERCEPT_MODULE_NAME,
        "Attention applied: modality=%d, gain=%.2f",
        attention->target_modality, attention->attention_gain);

    return 0;
}

int physics_percept_get_attention(
    const physics_percept_bridge_t* bridge,
    physics_percept_modality_t modality,
    physics_percept_attention_t* attention
) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_percept_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    if (modality >= 4) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_percept_get_attention: capacity exceeded");
        return -1;
    }

    *attention = bridge->attention[modality];
    return 0;
}

//=============================================================================
// Metabolic API
//=============================================================================

int physics_percept_set_temperature(
    physics_percept_bridge_t* bridge,
    float temperature_k
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->temperature_k = temperature_k;
    update_gains(bridge);
    return 0;
}

int physics_percept_set_atp(
    physics_percept_bridge_t* bridge,
    float atp_level
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->atp_level = (atp_level < 0.0f) ? 0.0f :
                        (atp_level > 1.0f) ? 1.0f : atp_level;
    update_gains(bridge);
    return 0;
}

bool physics_percept_is_fatigued(const physics_percept_bridge_t* bridge) {
    if (!bridge) return true;
    return bridge->atp_level < bridge->config.atp_fatigue_threshold;
}

//=============================================================================
// Update API
//=============================================================================

int physics_percept_update(
    physics_percept_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->sim_time_ms += dt_ms;
    bridge->stats.last_update_ms = bridge->sim_time_ms;

    /* Update gains in case state changed */
    update_gains(bridge);
    update_binding(bridge);

    return 0;
}

int physics_percept_reset(physics_percept_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->temperature_k = TEMP_REF_K;
    bridge->atp_level = 1.0f;
    bridge->gamma_power = 0.0f;
    bridge->alpha_power = 0.0f;
    bridge->sim_time_ms = 0.0f;

    /* Reset attention to neutral */
    for (int i = 0; i < 4; i++) {
        bridge->attention[i].attention_gain = 1.0f;
    }

    /* Reset binding */
    memset(&bridge->binding, 0, sizeof(bridge->binding));

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    update_gains(bridge);

    NIMCP_LOG_DEBUG(PHYSICS_PERCEPT_MODULE_NAME, "Bridge reset");

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

int physics_percept_get_stats(
    const physics_percept_bridge_t* bridge,
    physics_percept_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_percept_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

float physics_percept_get_gain(
    const physics_percept_bridge_t* bridge,
    physics_percept_modality_t modality
) {
    if (!bridge) return 1.0f;

    switch (modality) {
        case PHYSICS_PERCEPT_VISUAL:
            return bridge->visual_gain;
        case PHYSICS_PERCEPT_AUDITORY:
            return bridge->auditory_gain;
        case PHYSICS_PERCEPT_SOMATOSENSORY:
        case PHYSICS_PERCEPT_MULTIMODAL:
            return (bridge->visual_gain + bridge->auditory_gain) / 2.0f;
        default:
            return 1.0f;
    }
}
