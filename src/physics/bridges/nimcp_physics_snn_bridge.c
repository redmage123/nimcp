#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_physics_snn_bridge.c - Physics Layer to SNN/Plasticity Bridge
//=============================================================================

#include "utils/bridge/nimcp_bridge_base.h"
#include "physics/bridges/nimcp_physics_snn_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(physics_snn_bridge)

#define LOG_MODULE "PHYSICS_SNN_BRIDGE"


//=============================================================================
// Internal Constants
//=============================================================================

#define SPIKE_BUFFER_SIZE       1024
#define ELIGIBILITY_TABLE_SIZE  512

//=============================================================================
// Internal Structure
//=============================================================================

struct physics_snn_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    physics_snn_config_t config;

    /** Spike buffer (circular) */
    physics_snn_spike_t spike_buffer[SPIKE_BUFFER_SIZE];
    uint32_t spike_head;
    uint32_t spike_count;

    /** Eligibility trace table */
    physics_snn_eligibility_t eligibility[ELIGIBILITY_TABLE_SIZE];
    uint32_t num_eligibility;

    /** Current physics state */
    float temperature_k;
    float atp_level;
    float coherence;

    /** Current STDP windows (temperature-adjusted) */
    float current_ltp_window;
    float current_ltd_window;

    /** Accumulated modulation */
    physics_snn_modulation_t accumulated_mod;

    /** Timing */
    float sim_time_ms;

    /** Statistics */
    physics_snn_stats_t stats;

    bool initialized;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Compute Q10 scaling factor for STDP window
 */
static float compute_q10_factor(float temperature_k, float q10) {
    float delta_t = (temperature_k - PHYSICS_SNN_TEMP_REF) / 10.0f;
    return powf(q10, delta_t);
}

/**
 * @brief Classical STDP function
 */
static float classical_stdp(float dt_ms, float ltp_window, float ltd_window,
                            float ltp_amp, float ltd_amp) {
    if (dt_ms > 0) {
        /* Post after pre → LTP */
        return ltp_amp * expf(-dt_ms / ltp_window);
    } else {
        /* Pre after post → LTD */
        return -ltd_amp * expf(dt_ms / ltd_window);
    }
}

/**
 * @brief Symmetric STDP function
 */
static float symmetric_stdp(float dt_ms, float window, float amplitude) {
    float abs_dt = fabsf(dt_ms);
    return amplitude * expf(-abs_dt / window);
}

/**
 * @brief Update STDP windows based on temperature
 */
static void update_stdp_windows(physics_snn_bridge_t* bridge) {
    if (bridge->config.enable_temp_scaling) {
        float q10_factor = compute_q10_factor(bridge->temperature_k,
                                               bridge->config.temp_q10);
        /* Higher temperature → narrower windows (faster kinetics) */
        bridge->current_ltp_window = bridge->config.ltp_window_ms / q10_factor;
        bridge->current_ltd_window = bridge->config.ltd_window_ms / q10_factor;
    } else {
        bridge->current_ltp_window = bridge->config.ltp_window_ms;
        bridge->current_ltd_window = bridge->config.ltd_window_ms;
    }
}

//=============================================================================
// Configuration API
//=============================================================================

int physics_snn_default_config(physics_snn_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->encoding = PHYSICS_SNN_ENCODE_PRECISE;
    config->spike_threshold_mv = 0.0f;
    config->refractory_ms = NIMCP_REFRACTORY_PERIOD_MS;

    config->stdp_rule = PHYSICS_SNN_STDP_CLASSICAL;
    config->ltp_window_ms = PHYSICS_SNN_LTP_WINDOW;
    config->ltd_window_ms = PHYSICS_SNN_LTD_WINDOW;
    config->ltp_amplitude = NIMCP_STDP_A_PLUS;
    config->ltd_amplitude = NIMCP_STDP_A_MINUS;  /* Slight LTD bias for stability */

    config->enable_temp_scaling = true;
    config->enable_atp_gating = true;
    config->atp_ltp_threshold = 0.3f;
    config->temp_q10 = PHYSICS_SNN_STDP_Q10;

    config->enable_eligibility = true;
    config->eligibility_decay_ms = 100.0f;
    config->coherence_gate_threshold = NIMCP_MODULATION_DEFAULT;

    config->enable_feedback = true;
    config->mod_target = PHYSICS_SNN_MOD_CONDUCTANCE;
    config->mod_strength = 0.1f;

    config->update_interval_ms = 1.0f;

    return 0;
}

//=============================================================================
// Lifecycle API
//=============================================================================

physics_snn_bridge_t* physics_snn_bridge_create(
    const physics_snn_config_t* config
) {
    physics_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate physics-SNN bridge");
    if (bridge_base_init(&bridge->base, 0, "physics_snn") != 0) { nimcp_free(bridge); return NULL; }

    if (config) {
        bridge->config = *config;
    } else {
        physics_snn_default_config(&bridge->config);
    }

    /* Initialize physics state to defaults */
    bridge->temperature_k = PHYSICS_SNN_TEMP_REF;
    bridge->atp_level = 1.0f;
    bridge->coherence = 0.5f;

    /* Initialize STDP windows */
    update_stdp_windows(bridge);

    /* Initialize modulation to neutral */
    bridge->accumulated_mod.g_na_factor = 1.0f;
    bridge->accumulated_mod.g_k_factor = 1.0f;
    bridge->accumulated_mod.tau_factor = 1.0f;
    bridge->accumulated_mod.threshold_shift = 0.0f;

    bridge->initialized = true;

    NIMCP_LOG_INFO(PHYSICS_SNN_MODULE_NAME,
        "Physics-SNN bridge created: encoding=%d, stdp_rule=%d, "
        "temp_scaling=%d, atp_gating=%d",
        bridge->config.encoding, bridge->config.stdp_rule,
        bridge->config.enable_temp_scaling, bridge->config.enable_atp_gating);

    return bridge;
}

void physics_snn_bridge_destroy(physics_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "physics_snn");

    NIMCP_LOG_INFO(PHYSICS_SNN_MODULE_NAME,
        "Bridge destroyed - spikes: %lu, stdp_updates: %lu, "
        "eligibility_conversions: %lu",
        (unsigned long)bridge->stats.spikes_encoded,
        (unsigned long)bridge->stats.stdp_updates,
        (unsigned long)bridge->stats.eligibility_conversions);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

//=============================================================================
// Spike Input API
//=============================================================================

int physics_snn_register_spike(
    physics_snn_bridge_t* bridge,
    const physics_snn_spike_t* spike
) {
    if (!bridge || !spike) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_snn_register_spike: required parameter is NULL (bridge, spike)");
        return -1;
    }

    /* Add to circular buffer */
    bridge->spike_buffer[bridge->spike_head] = *spike;
    bridge->spike_head = (bridge->spike_head + 1) % SPIKE_BUFFER_SIZE;
    if (bridge->spike_count < SPIKE_BUFFER_SIZE) {
        bridge->spike_count++;
    }

    bridge->stats.spikes_encoded++;

    NIMCP_LOG_DEBUG(PHYSICS_SNN_MODULE_NAME,
        "Spike registered: source=%u, time=%.2fms, V=%.1fmV",
        spike->source_id, spike->spike_time_ms, spike->membrane_voltage);

    return 0;
}

int physics_snn_encode_spikes(
    physics_snn_bridge_t* bridge,
    float* output_currents,
    uint32_t num_outputs,
    float window_ms
) {
    if (!bridge || !output_currents) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_snn_encode_spikes: required parameter is NULL (bridge, output_currents)");
        return -1;
    }

    /* Clear outputs */
    memset(output_currents, 0, num_outputs * sizeof(float));

    float window_start = bridge->sim_time_ms - window_ms;
    int active_count = 0;

    /* Iterate through spike buffer */
    for (uint32_t i = 0; i < bridge->spike_count; i++) {
        uint32_t idx = (bridge->spike_head - 1 - i + SPIKE_BUFFER_SIZE) % SPIKE_BUFFER_SIZE;
        physics_snn_spike_t* spike = &bridge->spike_buffer[idx];

        /* Check if spike is within window */
        if (spike->spike_time_ms < window_start) break;

        /* Map source to output */
        uint32_t out_idx = spike->source_id % num_outputs;

        switch (bridge->config.encoding) {
            case PHYSICS_SNN_ENCODE_PRECISE:
                /* Binary spike presence */
                output_currents[out_idx] = 1.0f;
                break;

            case PHYSICS_SNN_ENCODE_RATE:
                /* Accumulate for rate */
                output_currents[out_idx] += 1.0f / window_ms * 1000.0f;
                break;

            case PHYSICS_SNN_ENCODE_BURST:
                /* Burst detection - multiple spikes = stronger signal */
                output_currents[out_idx] += 0.5f;
                break;

            case PHYSICS_SNN_ENCODE_PHASE:
                /* Phase-relative encoding (would need LFP reference) */
                output_currents[out_idx] = spike->membrane_voltage / 100.0f;
                break;
        }

        active_count++;
    }

    return active_count;
}

//=============================================================================
// STDP API
//=============================================================================

float physics_snn_compute_stdp(
    physics_snn_bridge_t* bridge,
    const physics_snn_spike_t* pre_spike,
    const physics_snn_spike_t* post_spike,
    physics_snn_stdp_event_t* event
) {
    if (!bridge || !pre_spike || !post_spike) return 0.0f;

    float dt_ms = post_spike->spike_time_ms - pre_spike->spike_time_ms;
    float weight_change = 0.0f;

    /* Compute base STDP */
    switch (bridge->config.stdp_rule) {
        case PHYSICS_SNN_STDP_CLASSICAL:
            weight_change = classical_stdp(dt_ms,
                bridge->current_ltp_window,
                bridge->current_ltd_window,
                bridge->config.ltp_amplitude,
                bridge->config.ltd_amplitude);
            break;

        case PHYSICS_SNN_STDP_SYMMETRIC:
            weight_change = symmetric_stdp(dt_ms,
                bridge->current_ltp_window,
                bridge->config.ltp_amplitude);
            break;

        case PHYSICS_SNN_STDP_TRIPLET:
        case PHYSICS_SNN_STDP_VOLTAGE:
            /* Use classical as fallback */
            weight_change = classical_stdp(dt_ms,
                bridge->current_ltp_window,
                bridge->current_ltd_window,
                bridge->config.ltp_amplitude,
                bridge->config.ltd_amplitude);
            break;
    }

    /* Apply temperature factor */
    float temp_factor = 1.0f;
    if (bridge->config.enable_temp_scaling) {
        temp_factor = compute_q10_factor(
            (pre_spike->temperature + post_spike->temperature) / 2.0f,
            bridge->config.temp_q10);
    }

    /* Apply ATP gating (only affects LTP) */
    float atp_factor = 1.0f;
    if (bridge->config.enable_atp_gating && weight_change > 0) {
        float avg_atp = (pre_spike->atp_level + post_spike->atp_level) / 2.0f;
        if (avg_atp < bridge->config.atp_ltp_threshold) {
            /* ATP below threshold → no LTP */
            atp_factor = 0.0f;
        } else {
            /* Scale LTP by ATP level */
            atp_factor = avg_atp;
        }
    }

    weight_change *= temp_factor * atp_factor;

    /* Fill event if provided */
    if (event) {
        event->pre_id = pre_spike->source_id;
        event->post_id = post_spike->source_id;
        event->dt_ms = dt_ms;
        event->weight_change = weight_change;
        event->temperature_factor = temp_factor;
        event->atp_factor = atp_factor;
    }

    /* Update statistics */
    if (weight_change > 0) {
        bridge->stats.avg_ltp_magnitude =
            0.99f * bridge->stats.avg_ltp_magnitude + 0.01f * weight_change;
    } else if (weight_change < 0) {
        bridge->stats.avg_ltd_magnitude =
            0.99f * bridge->stats.avg_ltd_magnitude + 0.01f * (-weight_change);
    }
    bridge->stats.total_weight_change += weight_change;
    bridge->stats.stdp_updates++;

    return weight_change;
}

int physics_snn_process_stdp(
    physics_snn_bridge_t* bridge,
    physics_snn_stdp_event_t* events,
    uint32_t max_events
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    int update_count = 0;
    uint32_t event_idx = 0;

    /* Process all spike pairs within STDP window */
    for (uint32_t i = 0; i < bridge->spike_count && i < SPIKE_BUFFER_SIZE; i++) {
        uint32_t pre_idx = (bridge->spike_head - 1 - i + SPIKE_BUFFER_SIZE) % SPIKE_BUFFER_SIZE;
        physics_snn_spike_t* pre = &bridge->spike_buffer[pre_idx];

        for (uint32_t j = i + 1; j < bridge->spike_count && j < SPIKE_BUFFER_SIZE; j++) {
            uint32_t post_idx = (bridge->spike_head - 1 - j + SPIKE_BUFFER_SIZE) % SPIKE_BUFFER_SIZE;
            physics_snn_spike_t* post = &bridge->spike_buffer[post_idx];

            /* Check if within STDP window */
            float dt = fabsf(post->spike_time_ms - pre->spike_time_ms);
            if (dt > bridge->current_ltp_window + bridge->current_ltd_window) {
                continue;
            }

            /* Compute STDP */
            physics_snn_stdp_event_t event;
            physics_snn_compute_stdp(bridge, pre, post, &event);

            if (events && event_idx < max_events) {
                events[event_idx++] = event;
            }

            update_count++;

            /* Also compute reverse direction */
            physics_snn_compute_stdp(bridge, post, pre, &event);
            if (events && event_idx < max_events) {
                events[event_idx++] = event;
            }
            update_count++;
        }
    }

    return update_count;
}

int physics_snn_set_temperature(
    physics_snn_bridge_t* bridge,
    float temperature_k
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->temperature_k = temperature_k;
    update_stdp_windows(bridge);
    return 0;
}

int physics_snn_set_atp(
    physics_snn_bridge_t* bridge,
    float atp_level
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->atp_level = (atp_level < 0.0f) ? 0.0f :
                        (atp_level > 1.0f) ? 1.0f : atp_level;
    return 0;
}

//=============================================================================
// Eligibility Trace API
//=============================================================================

float physics_snn_update_eligibility(
    physics_snn_bridge_t* bridge,
    uint32_t synapse_id,
    float increment
) {
    if (!bridge) return 0.0f;

    /* Find or create eligibility entry */
    physics_snn_eligibility_t* entry = NULL;
    for (uint32_t i = 0; i < bridge->num_eligibility; i++) {
        if (bridge->eligibility[i].synapse_id == synapse_id) {
            entry = &bridge->eligibility[i];
            break;
        }
    }

    if (!entry && bridge->num_eligibility < ELIGIBILITY_TABLE_SIZE) {
        entry = &bridge->eligibility[bridge->num_eligibility++];
        entry->synapse_id = synapse_id;
        entry->trace_value = 0.0f;
        entry->converted = false;
    }

    if (entry) {
        entry->trace_value += increment;
        entry->last_update_ms = bridge->sim_time_ms;
        return entry->trace_value;
    }

    return 0.0f;
}

int physics_snn_convert_eligibility(
    physics_snn_bridge_t* bridge,
    float coherence
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    int converted = 0;

    /* Check if coherence meets threshold */
    if (bridge->config.coherence_gate_threshold > 0.0f &&
        coherence < bridge->config.coherence_gate_threshold) {
        return 0;  /* Gated - no conversion */
    }

    for (uint32_t i = 0; i < bridge->num_eligibility; i++) {
        physics_snn_eligibility_t* entry = &bridge->eligibility[i];
        if (entry->trace_value > 0.01f && !entry->converted) {
            /* Convert trace to weight change */
            /* In full implementation, would apply to actual synapse */
            bridge->stats.total_weight_change += entry->trace_value * coherence;
            entry->converted = true;
            converted++;
        }
    }

    bridge->stats.eligibility_conversions += converted;
    return converted;
}

int physics_snn_decay_eligibility(
    physics_snn_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    float decay = expf(-dt_ms / bridge->config.eligibility_decay_ms);

    for (uint32_t i = 0; i < bridge->num_eligibility; i++) {
        bridge->eligibility[i].trace_value *= decay;
    }

    return 0;
}

//=============================================================================
// Feedback API
//=============================================================================

int physics_snn_receive_modulation(
    physics_snn_bridge_t* bridge,
    const physics_snn_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_snn_receive_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }

    /* Accumulate modulation with smoothing */
    float alpha = 0.1f;  /* Smoothing factor */

    bridge->accumulated_mod.g_na_factor =
        (1.0f - alpha) * bridge->accumulated_mod.g_na_factor +
        alpha * modulation->g_na_factor;

    bridge->accumulated_mod.g_k_factor =
        (1.0f - alpha) * bridge->accumulated_mod.g_k_factor +
        alpha * modulation->g_k_factor;

    bridge->accumulated_mod.threshold_shift =
        (1.0f - alpha) * bridge->accumulated_mod.threshold_shift +
        alpha * modulation->threshold_shift;

    bridge->accumulated_mod.tau_factor =
        (1.0f - alpha) * bridge->accumulated_mod.tau_factor +
        alpha * modulation->tau_factor;

    bridge->stats.feedback_events++;

    return 0;
}

int physics_snn_get_modulation(
    physics_snn_bridge_t* bridge,
    physics_snn_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_snn_get_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }
    *modulation = bridge->accumulated_mod;
    return 0;
}

//=============================================================================
// Update API
//=============================================================================

int physics_snn_update(
    physics_snn_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->sim_time_ms += dt_ms;

    /* Decay eligibility traces */
    if (bridge->config.enable_eligibility) {
        physics_snn_decay_eligibility(bridge, dt_ms);
    }

    /* Update STDP windows based on current temperature */
    update_stdp_windows(bridge);

    bridge->stats.last_update_ms = bridge->sim_time_ms;

    return 0;
}

int physics_snn_reset(physics_snn_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->spike_head = 0;
    bridge->spike_count = 0;
    bridge->num_eligibility = 0;
    bridge->sim_time_ms = 0.0f;

    /* Reset modulation to neutral */
    bridge->accumulated_mod.g_na_factor = 1.0f;
    bridge->accumulated_mod.g_k_factor = 1.0f;
    bridge->accumulated_mod.tau_factor = 1.0f;
    bridge->accumulated_mod.threshold_shift = 0.0f;

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    NIMCP_LOG_DEBUG(PHYSICS_SNN_MODULE_NAME, "Bridge reset");

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

int physics_snn_get_stats(
    const physics_snn_bridge_t* bridge,
    physics_snn_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int physics_snn_get_stdp_windows(
    const physics_snn_bridge_t* bridge,
    float* ltp_window_ms,
    float* ltd_window_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    if (ltp_window_ms) *ltp_window_ms = bridge->current_ltp_window;
    if (ltd_window_ms) *ltd_window_ms = bridge->current_ltd_window;

    return 0;
}

bool physics_snn_is_learning_gated(const physics_snn_bridge_t* bridge) {
    if (!bridge) return true;

    if (!bridge->config.enable_atp_gating) {
        return false;
    }

    return bridge->atp_level < bridge->config.atp_ltp_threshold;
}
