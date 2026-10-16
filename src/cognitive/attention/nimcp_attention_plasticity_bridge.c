/**
 * @file nimcp_attention_plasticity_bridge.c
 * @brief Attention System - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/attention/nimcp_attention_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(attention_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "ATTENTION_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct attention_plasticity_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    /* Configuration */
    attention_plasticity_config_t config;

    /* Synapses */
    attention_plasticity_synapse_t* synapses;
    uint32_t synapse_count;
    uint32_t synapse_capacity;

    /* Per-head state */
    attention_head_plasticity_t* head_states;
    uint32_t num_heads;

    /* State */
    attention_plasticity_state_t state;

    /* Current modulation values */
    float current_attention_mod;
    float current_salience_mod;
    float current_novelty_mod;
    float global_learning_rate;

    /* Pending reward */
    float pending_reward;
    uint64_t last_reward_time_us;

    /* Statistics */
    attention_plasticity_stats_t stats;

    /* Callbacks */
    attention_weight_change_cb weight_callback;
    void* weight_callback_data;
    attention_shift_cb shift_callback;
    void* shift_callback_data;

    /* Bio-async */
    bool bio_async_connected;

};

BRIDGE_DEFINE_SECURITY_SETTERS(attention_plasticity_bridge)

/**
 * @brief Find synapse by ID
 */
static attention_plasticity_synapse_t* find_synapse(
    attention_plasticity_bridge_t* bridge,
    uint32_t synapse_id)
{
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

/**
 * @brief Compute STDP weight change
 */
static float compute_stdp_weight_change(
    const attention_plasticity_config_t* config,
    float dt_ms,
    float attention_mod)
{
    float dw = 0.0f;

    if (dt_ms > 0) {
        /* Pre before post -> LTP */
        dw = config->stdp_a_plus * expf(-dt_ms / config->stdp_tau_plus);
        dw *= attention_mod * config->focus_learning_boost;
    } else if (dt_ms < 0) {
        /* Post before pre -> LTD */
        dw = -config->stdp_a_minus * expf(dt_ms / config->stdp_tau_minus);
        dw *= (attention_mod > 0.0f ? (1.0f / attention_mod) : 1.0f) * config->unfocused_ltd_boost;
    }

    return dw;
}

/**
 * @brief Apply eligibility trace modulation
 */
static float apply_eligibility(
    attention_plasticity_synapse_t* synapse,
    float reward,
    float gain)
{
    float dw = synapse->eligibility_trace * reward * gain;
    return dw;
}

/**
 * @brief Update BCM threshold
 */
static void update_bcm_threshold(
    attention_plasticity_synapse_t* synapse,
    float activity,
    float tau)
{
    /* Sliding threshold based on recent activity */
    float target = activity * activity;  /* Quadratic function of activity */
    synapse->bcm_threshold += (target - synapse->bcm_threshold) / tau;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

attention_plasticity_config_t attention_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    attention_plasticity_config_t config = {
        /* STDP parameters */
        .stdp_ltp_window_ms = ATTENTION_PLASTICITY_STDP_WINDOW,
        .stdp_ltd_window_ms = ATTENTION_PLASTICITY_STDP_WINDOW,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,
        .stdp_tau_plus = NIMCP_STDP_TAU_PLUS_MS,
        .stdp_tau_minus = NIMCP_STDP_TAU_MINUS_MS,

        /* Attention modulation */
        .enable_attention_modulation = true,
        .focus_learning_boost = 2.0f,
        .unfocused_ltd_boost = 1.5f,
        .attention_learning_gain = 1.0f,

        /* Salience modulation */
        .enable_salience_modulation = true,
        .salience_learning_gain = 1.5f,
        .salience_threshold = NIMCP_SALIENCE_THRESHOLD,

        /* BCM */
        .enable_bcm = true,
        .bcm_threshold_tau = 1000.0f,
        .bcm_activity_tau = 100.0f,

        /* Homeostatic */
        .enable_homeostatic = true,
        .target_attention_rate = 0.5f,
        .homeostatic_tau_ms = 10000.0f,

        /* Eligibility */
        .enable_eligibility = true,
        .eligibility_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT,
        .reward_modulation_gain = 1.0f,

        /* Weights */
        .weight_min = 0.0f,
        .weight_max = 1.0f,
        .initial_weight = 0.3f,

        /* Habituation */
        .enable_habituation = true,
        .habituation_rate = 0.01f,
        .spontaneous_recovery_tau = 60000.0f,

        /* Novelty */
        .enable_novelty_detection = true,
        .novelty_boost = 2.0f,
        .familiarity_threshold = 0.8f,

        /* Integration */
        .enable_bio_async = false,
        .enable_immune_modulation = false,
        .enable_sleep_consolidation = true
    };
    return config;
}

attention_plasticity_bridge_t* attention_plasticity_create(
    const attention_plasticity_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    attention_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(attention_plasticity_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_plasticity_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = attention_plasticity_config_default();
    }

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "attention_plasticity") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "attention_plasticity_create: failed to initialize bridge base");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Allocate synapse array */
    bridge->synapse_capacity = ATTENTION_PLASTICITY_MAX_SYNAPSES;
    bridge->synapses = nimcp_calloc(bridge->synapse_capacity, sizeof(attention_plasticity_synapse_t));
    if (!bridge->synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_plasticity_create: failed to allocate synapses");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }
    bridge->synapse_count = 0;

    /* Allocate per-head state */
    bridge->num_heads = ATTENTION_PLASTICITY_MAX_HEADS;
    bridge->head_states = nimcp_calloc(bridge->num_heads, sizeof(attention_head_plasticity_t));
    if (!bridge->head_states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_plasticity_create: failed to allocate head_states");
        nimcp_free(bridge->synapses);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize head states */
    for (uint32_t h = 0; h < bridge->num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && bridge->num_heads > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(h + 1) / (float)bridge->num_heads);
        }

        bridge->head_states[h].learning_rate = 1.0f;
        bridge->head_states[h].attention_bias = 0.0f;
        bridge->head_states[h].habituation_level = 0.0f;
        bridge->head_states[h].novelty_score = 1.0f;
        bridge->head_states[h].last_focus_time_us = 0;
    }

    /* Initialize state */
    bridge->state = ATTENTION_PLASTICITY_STATE_IDLE;
    bridge->current_attention_mod = 1.0f;
    bridge->current_salience_mod = 1.0f;
    bridge->current_novelty_mod = 1.0f;
    bridge->global_learning_rate = 1.0f;
    bridge->pending_reward = 0.0f;
    bridge->last_reward_time_us = 0;
    bridge->bio_async_connected = false;

    return bridge;
}

void attention_plasticity_destroy(attention_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "attention_plasticity");

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    if (bridge->bio_async_connected) {
        attention_plasticity_disconnect_bio_async(bridge);
    }

    if (bridge->synapses) nimcp_free(bridge->synapses);
    if (bridge->head_states) nimcp_free(bridge->head_states);

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int attention_plasticity_reset(attention_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all synapses to initial state */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        bridge->synapses[i].weight = bridge->synapses[i].initial_weight;
        bridge->synapses[i].eligibility_trace = 0.0f;
        bridge->synapses[i].bcm_threshold = 0.5f;
        bridge->synapses[i].avg_activity = 0.0f;
        bridge->synapses[i].habituation_level = 0.0f;
        bridge->synapses[i].familiarity = 0.0f;
        bridge->synapses[i].exposure_count = 0;
    }

    /* Reset head states */
    for (uint32_t h = 0; h < bridge->num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && bridge->num_heads > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(h + 1) / (float)bridge->num_heads);
        }

        bridge->head_states[h].learning_rate = 1.0f;
        bridge->head_states[h].attention_bias = 0.0f;
        bridge->head_states[h].habituation_level = 0.0f;
        bridge->head_states[h].novelty_score = 1.0f;
        bridge->head_states[h].last_focus_time_us = 0;
    }

    /* Reset state */
    bridge->state = ATTENTION_PLASTICITY_STATE_IDLE;
    bridge->current_attention_mod = 1.0f;
    bridge->current_salience_mod = 1.0f;
    bridge->current_novelty_mod = 1.0f;
    bridge->global_learning_rate = 1.0f;
    bridge->pending_reward = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int attention_plasticity_register_synapse(
    attention_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    attention_synapse_type_t type,
    uint32_t head_idx,
    float initial_weight)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_register_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already registered */
    if (find_synapse(bridge, synapse_id)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_plasticity_register_synapse: validation failed");
        return -1;  /* Already exists */
    }

    /* Check capacity */
    if (bridge->synapse_count >= bridge->synapse_capacity) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "attention_plasticity_register_synapse: capacity exceeded");
        return -1;  /* Full */
    }

    /* Bounds check head_idx */
    if (head_idx >= bridge->num_heads) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_plasticity_register_synapse: head_idx out of bounds");
        return -1;
    }

    /* Add synapse */
    attention_plasticity_synapse_t* synapse = &bridge->synapses[bridge->synapse_count];
    synapse->synapse_id = synapse_id;
    synapse->type = type;
    synapse->head_idx = head_idx;
    synapse->weight = nimcp_clampf(initial_weight, bridge->config.weight_min, bridge->config.weight_max);
    synapse->initial_weight = synapse->weight;
    synapse->original_strength = synapse->weight;
    synapse->last_pre_spike_us = 0;
    synapse->last_post_spike_us = 0;
    synapse->eligibility_trace = 0.0f;
    synapse->bcm_threshold = 0.5f;
    synapse->avg_activity = 0.0f;
    synapse->habituation_level = 0.0f;
    synapse->familiarity = 0.0f;
    synapse->exposure_count = 0;

    bridge->synapse_count++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int attention_plasticity_unregister_synapse(
    attention_plasticity_bridge_t* bridge,
    uint32_t synapse_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Find synapse */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            /* Move last synapse to this slot */
            if (i < bridge->synapse_count - 1) {
                bridge->synapses[i] = bridge->synapses[bridge->synapse_count - 1];
            }
            bridge->synapse_count--;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_plasticity_unregister_synapse: operation failed");
    return -1;  /* Not found */
}

int attention_plasticity_get_synapse(
    attention_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    attention_plasticity_synapse_t* synapse)
{
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_get_synapse: bridge or synapse is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    attention_plasticity_synapse_t* found = find_synapse(bridge, synapse_id);
    if (!found) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_get_synapse: found is NULL");
        return -1;
    }

    *synapse = *found;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Event Recording
//=============================================================================

int attention_plasticity_focus(
    attention_plasticity_bridge_t* bridge,
    uint32_t head_idx,
    float attention_weight,
    uint64_t timestamp_us)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_focus: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    if (head_idx >= bridge->num_heads) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_plasticity_focus: head_idx out of range");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = ATTENTION_PLASTICITY_STATE_OBSERVING;

    /* Update head state */
    attention_head_plasticity_t* head = &bridge->head_states[head_idx];
    head->last_focus_time_us = timestamp_us;

    /* Update attention bias based on repeated focus */
    float focus_delta = attention_weight * bridge->config.attention_learning_gain;
    head->attention_bias += focus_delta * 0.01f;
    head->attention_bias = nimcp_clampf(head->attention_bias, -1.0f, 1.0f);

    /* Record pre-spike for all synapses of this head */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].head_idx == head_idx) {
            bridge->synapses[i].last_pre_spike_us = timestamp_us;

            /* Update eligibility trace */
            if (bridge->config.enable_eligibility) {
                bridge->synapses[i].eligibility_trace += attention_weight;
            }

            /* Update exposure count for novelty */
            bridge->synapses[i].exposure_count++;

            bridge->stats.total_pre_spikes++;
        }
    }

    bridge->stats.total_focus_events++;
    bridge->state = ATTENTION_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int attention_plasticity_shift(
    attention_plasticity_bridge_t* bridge,
    uint32_t from_head,
    uint32_t to_head,
    float shift_strength,
    uint64_t timestamp_us)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_shift: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    if (from_head >= bridge->num_heads || to_head >= bridge->num_heads) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_plasticity_shift: head index out of range");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = ATTENTION_PLASTICITY_STATE_UPDATING;

    /* Update source head - LTD for losing focus */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].head_idx == from_head) {
            if (bridge->synapses[i].last_pre_spike_us > 0) {
                float dt = (float)(timestamp_us - bridge->synapses[i].last_pre_spike_us) / 1000.0f;
                if (dt < bridge->config.stdp_ltd_window_ms) {
                    float dw = -bridge->config.stdp_a_minus * shift_strength * 0.5f;
                    float old_weight = bridge->synapses[i].weight;
                    bridge->synapses[i].weight = nimcp_clampf(
                        bridge->synapses[i].weight + dw,
                        bridge->config.weight_min,
                        bridge->config.weight_max);

                    if (bridge->weight_callback) {
                        bridge->weight_callback(
                            bridge->synapses[i].synapse_id,
                            from_head,
                            old_weight,
                            bridge->synapses[i].weight,
                            ATTENTION_LEARN_SHIFT,
                            bridge->weight_callback_data);
                    }

                    bridge->stats.ltd_events++;
                }
            }
        }
    }

    /* Update destination head - LTP for gaining focus */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].head_idx == to_head) {
            bridge->synapses[i].last_pre_spike_us = timestamp_us;

            float dw = bridge->config.stdp_a_plus * shift_strength;
            float old_weight = bridge->synapses[i].weight;
            bridge->synapses[i].weight = nimcp_clampf(
                bridge->synapses[i].weight + dw,
                bridge->config.weight_min,
                bridge->config.weight_max);

            if (bridge->weight_callback) {
                bridge->weight_callback(
                    bridge->synapses[i].synapse_id,
                    to_head,
                    old_weight,
                    bridge->synapses[i].weight,
                    ATTENTION_LEARN_SHIFT,
                    bridge->weight_callback_data);
            }

            bridge->stats.ltp_events++;
        }
    }

    /* Fire shift callback */
    if (bridge->shift_callback) {
        bridge->shift_callback(from_head, to_head, shift_strength, bridge->shift_callback_data);
    }

    bridge->stats.total_shift_events++;
    bridge->state = ATTENTION_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int attention_plasticity_salience(
    attention_plasticity_bridge_t* bridge,
    const float* salience_map,
    uint32_t sequence_length,
    uint64_t timestamp_us)
{
    if (!bridge || !salience_map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_salience: bridge or salience_map is NULL");
        return -1;
    }

    BRIDGE_BBB_VALIDATE(bridge, salience_map, sequence_length * sizeof(float));

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (sequence_length == 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_plasticity_salience: sequence_length is 0");
        return -1;
    }

    /* Compute average salience */
    float avg_salience = 0.0f;
    for (uint32_t i = 0; i < sequence_length; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sequence_length > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(i + 1) / (float)sequence_length);
        }

        avg_salience += salience_map[i];
    }
    avg_salience /= (float)sequence_length;

    /* Update salience modulation */
    bridge->current_salience_mod = avg_salience * bridge->config.salience_learning_gain;

    /* Modulate learning rate based on salience */
    if (bridge->config.enable_salience_modulation) {
        bridge->global_learning_rate = 1.0f + (avg_salience - 0.5f) * bridge->config.salience_learning_gain;
        bridge->global_learning_rate = nimcp_clampf(bridge->global_learning_rate, 0.5f, 2.0f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int attention_plasticity_reward(
    attention_plasticity_bridge_t* bridge,
    float reward,
    uint64_t timestamp_us)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_reward: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->pending_reward = reward;
    bridge->last_reward_time_us = timestamp_us;

    /* Apply reward to all eligible synapses */
    if (bridge->config.enable_eligibility) {
        for (uint32_t i = 0; i < bridge->synapse_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
                attention_plasticity_bridge_heartbeat("attention_pl_loop",
                                 (float)(i + 1) / (float)bridge->synapse_count);
            }

            if (bridge->synapses[i].eligibility_trace > 0.01f) {
                float dw = apply_eligibility(
                    &bridge->synapses[i],
                    reward,
                    bridge->config.reward_modulation_gain);

                float old_weight = bridge->synapses[i].weight;
                bridge->synapses[i].weight = nimcp_clampf(
                    bridge->synapses[i].weight + dw,
                    bridge->config.weight_min,
                    bridge->config.weight_max);

                if (reward > 0) {
                    bridge->stats.ltp_events++;
                } else {
                    bridge->stats.ltd_events++;
                }

                bridge->stats.avg_weight_change += fabsf(bridge->synapses[i].weight - old_weight);
            }
        }
    }

    if (reward > 0) {
        bridge->stats.total_reward += reward;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int attention_plasticity_habituation_trial(
    attention_plasticity_bridge_t* bridge,
    uint32_t head_idx,
    uint64_t timestamp_us)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_habituation_trial: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    if (head_idx >= bridge->num_heads) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_plasticity_habituation_trial: head_idx out of range");
        return -1;
    }
    if (!bridge->config.enable_habituation) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    attention_head_plasticity_t* head = &bridge->head_states[head_idx];

    /* Increase habituation */
    head->habituation_level += bridge->config.habituation_rate;
    head->habituation_level = nimcp_clampf(head->habituation_level, 0.0f, 1.0f);

    /* Apply habituation to synapses of this head */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].head_idx == head_idx) {
            bridge->synapses[i].habituation_level = head->habituation_level;

            /* Reduce effective weight */
            float effective_reduction = bridge->synapses[i].habituation_level *
                                       bridge->config.habituation_rate;
            float old_weight = bridge->synapses[i].weight;
            bridge->synapses[i].weight *= (1.0f - effective_reduction);
            bridge->synapses[i].weight = nimcp_clampf(
                bridge->synapses[i].weight,
                bridge->config.weight_min,
                bridge->config.weight_max);

            bridge->stats.avg_weight_change += fabsf(bridge->synapses[i].weight - old_weight);
        }
    }

    bridge->stats.habituation_events++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int attention_plasticity_novelty(
    attention_plasticity_bridge_t* bridge,
    uint32_t head_idx,
    float novelty_score,
    uint64_t timestamp_us)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_novelty: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    if (head_idx >= bridge->num_heads) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_plasticity_novelty: head_idx out of range");
        return -1;
    }
    if (!bridge->config.enable_novelty_detection) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    attention_head_plasticity_t* head = &bridge->head_states[head_idx];

    /* Update novelty score */
    head->novelty_score = novelty_score;
    bridge->current_novelty_mod = novelty_score;

    /* Apply novelty boost to learning */
    if (novelty_score > (1.0f - bridge->config.familiarity_threshold)) {
        /* High novelty - boost learning */
        for (uint32_t i = 0; i < bridge->synapse_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
                attention_plasticity_bridge_heartbeat("attention_pl_loop",
                                 (float)(i + 1) / (float)bridge->synapse_count);
            }

            if (bridge->synapses[i].head_idx == head_idx) {
                float boost = bridge->config.novelty_boost * novelty_score;
                float dw = bridge->config.stdp_a_plus * boost * 0.1f;
                float old_weight = bridge->synapses[i].weight;
                bridge->synapses[i].weight = nimcp_clampf(
                    bridge->synapses[i].weight + dw,
                    bridge->config.weight_min,
                    bridge->config.weight_max);

                bridge->stats.avg_weight_change += fabsf(bridge->synapses[i].weight - old_weight);

                /* Decrease familiarity (it's novel) */
                bridge->synapses[i].familiarity *= 0.9f;
            }
        }

        /* Reset habituation for novel stimuli */
        head->habituation_level *= 0.5f;

        bridge->stats.novelty_events++;
    } else {
        /* Familiar - increase familiarity metric */
        for (uint32_t i = 0; i < bridge->synapse_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
                attention_plasticity_bridge_heartbeat("attention_pl_loop",
                                 (float)(i + 1) / (float)bridge->synapse_count);
            }

            if (bridge->synapses[i].head_idx == head_idx) {
                bridge->synapses[i].familiarity += 0.1f * (1.0f - novelty_score);
                bridge->synapses[i].familiarity = nimcp_clampf(
                    bridge->synapses[i].familiarity, 0.0f, 1.0f);
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int attention_plasticity_update(
    attention_plasticity_bridge_t* bridge,
    float dt_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = ATTENTION_PLASTICITY_STATE_UPDATING;

    uint64_t now_us = nimcp_time_get_us();

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        attention_plasticity_synapse_t* syn = &bridge->synapses[i];

        /* Decay eligibility traces */
        if (bridge->config.enable_eligibility) {
            syn->eligibility_trace *= bridge->config.eligibility_decay;
        }

        /* Update BCM threshold */
        if (bridge->config.enable_bcm) {
            update_bcm_threshold(syn, syn->avg_activity, bridge->config.bcm_threshold_tau);
        }

        /* Spontaneous recovery from habituation */
        if (bridge->config.enable_habituation && syn->habituation_level > 0.0f) {
            float recovery_rate = (fabsf(bridge->config.spontaneous_recovery_tau) > 1e-10f) ?
                (dt_ms / bridge->config.spontaneous_recovery_tau) : 0.0f;
            syn->habituation_level -= recovery_rate;
            syn->habituation_level = nimcp_clampf(syn->habituation_level, 0.0f, 1.0f);

            /* Recover weight towards original */
            if (syn->habituation_level < 0.1f && syn->weight < syn->original_strength) {
                syn->weight += recovery_rate * (syn->original_strength - syn->weight);
            }
        }

        /* Homeostatic scaling */
        if (bridge->config.enable_homeostatic && syn->head_idx < bridge->num_heads) {
            attention_head_plasticity_t* head = &bridge->head_states[syn->head_idx];
            float current_rate = syn->avg_activity;
            float diff = bridge->config.target_attention_rate - current_rate;
            float scale = 1.0f + diff * (dt_ms / bridge->config.homeostatic_tau_ms);
            syn->weight *= scale;
            syn->weight = nimcp_clampf(syn->weight, bridge->config.weight_min, bridge->config.weight_max);
        }

        /* Update average activity */
        float alpha = (fabsf(bridge->config.bcm_activity_tau) > 1e-10f) ?
            (dt_ms / bridge->config.bcm_activity_tau) : 0.0f;
        float recent_activity = (syn->last_pre_spike_us > now_us - 100000) ? 1.0f : 0.0f;
        syn->avg_activity = (1.0f - alpha) * syn->avg_activity + alpha * recent_activity;
    }

    /* Update per-head states */
    for (uint32_t h = 0; h < bridge->num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && bridge->num_heads > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(h + 1) / (float)bridge->num_heads);
        }

        attention_head_plasticity_t* head = &bridge->head_states[h];

        /* Decay habituation */
        if (bridge->config.enable_habituation) {
            float recovery_rate = dt_ms / bridge->config.spontaneous_recovery_tau;
            head->habituation_level -= recovery_rate;
            head->habituation_level = nimcp_clampf(head->habituation_level, 0.0f, 1.0f);
        }

        /* Compute head learning rate based on recent focus */
        float time_since_focus = (float)(now_us - head->last_focus_time_us) / 1000000.0f;
        head->learning_rate = expf(-time_since_focus * 0.1f);
        head->learning_rate = nimcp_clampf(head->learning_rate, 0.1f, 1.0f);
    }

    bridge->state = ATTENTION_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int attention_plasticity_consolidate(attention_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_consolidate: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_sleep_consolidation) return 0;

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = ATTENTION_PLASTICITY_STATE_CONSOLIDATING;

    /* Consolidation: strengthen high-weight synapses, weaken low-weight ones */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        attention_plasticity_synapse_t* syn = &bridge->synapses[i];

        float midpoint = (bridge->config.weight_max + bridge->config.weight_min) / 2.0f;

        if (syn->weight > midpoint) {
            /* Strengthen strong synapses */
            syn->weight += 0.01f * (bridge->config.weight_max - syn->weight);
        } else {
            /* Weaken weak synapses */
            syn->weight -= 0.01f * (syn->weight - bridge->config.weight_min);
        }

        /* Clamp */
        syn->weight = nimcp_clampf(syn->weight, bridge->config.weight_min, bridge->config.weight_max);

        /* Reset eligibility traces during consolidation */
        syn->eligibility_trace = 0.0f;

        /* Update original strength to consolidated value */
        syn->original_strength = syn->weight;
    }

    /* Reset habituation levels during sleep */
    for (uint32_t h = 0; h < bridge->num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && bridge->num_heads > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(h + 1) / (float)bridge->num_heads);
        }

        bridge->head_states[h].habituation_level *= 0.5f;
    }

    bridge->state = ATTENTION_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int attention_plasticity_get_bias(
    attention_plasticity_bridge_t* bridge,
    uint32_t head_idx,
    float* bias)
{
    if (!bridge || !bias) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_get_bias: bridge or bias is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    if (head_idx >= bridge->num_heads) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_plasticity_get_bias: head_idx out of range");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    *bias = bridge->head_states[head_idx].attention_bias;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int attention_plasticity_get_modulation(
    attention_plasticity_bridge_t* bridge,
    float* modulation,
    uint32_t num_heads)
{
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_get_modulation: bridge or modulation is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t n = (num_heads < bridge->num_heads) ? num_heads : bridge->num_heads;

    for (uint32_t h = 0; h < n; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && n > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(h + 1) / (float)n);
        }

        /* Compute modulation from learned bias, habituation, and novelty */
        float bias = bridge->head_states[h].attention_bias;
        float hab = bridge->head_states[h].habituation_level;
        float nov = bridge->head_states[h].novelty_score;

        modulation[h] = (1.0f + bias) * (1.0f - hab) * (0.5f + 0.5f * nov);
        modulation[h] = nimcp_clampf(modulation[h], 0.0f, 2.0f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float attention_plasticity_get_habituation(
    attention_plasticity_bridge_t* bridge,
    uint32_t head_idx)
{
    if (!bridge) return -1.0f;
    if (head_idx >= bridge->num_heads) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float hab = bridge->head_states[head_idx].habituation_level;

    nimcp_mutex_unlock(bridge->base.mutex);

    return hab;
}

float attention_plasticity_get_novelty_score(
    attention_plasticity_bridge_t* bridge,
    uint32_t head_idx)
{
    if (!bridge) return -1.0f;
    if (head_idx >= bridge->num_heads) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float nov = bridge->head_states[head_idx].novelty_score;

    nimcp_mutex_unlock(bridge->base.mutex);

    return nov;
}

float attention_plasticity_get_sensitivity(
    attention_plasticity_bridge_t* bridge,
    uint32_t head_idx)
{
    if (!bridge) return -1.0f;
    if (head_idx >= bridge->num_heads) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute sensitivity as average weight of head's synapses */
    float sum = 0.0f;
    uint32_t count = 0;

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            attention_plasticity_bridge_heartbeat("attention_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].head_idx == head_idx) {
            sum += bridge->synapses[i].weight;
            count++;
        }
    }

    float sensitivity = (count > 0) ? (sum / (float)count) : 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return sensitivity;
}

//=============================================================================
// State and Statistics
//=============================================================================

int attention_plasticity_get_state(
    const attention_plasticity_bridge_t* bridge,
    attention_plasticity_bridge_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_get_state: bridge or state is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    attention_plasticity_bridge_t* mutable_bridge = (attention_plasticity_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    state->state = bridge->state;
    state->registered_synapses = bridge->synapse_count;
    state->global_learning_rate = bridge->global_learning_rate;
    state->current_attention_mod = bridge->current_attention_mod;
    state->current_salience_mod = bridge->current_salience_mod;
    state->current_novelty_mod = bridge->current_novelty_mod;
    state->bio_async_connected = bridge->bio_async_connected;

    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

int attention_plasticity_get_stats(
    const attention_plasticity_bridge_t* bridge,
    attention_plasticity_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_get_stats: bridge or stats is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    attention_plasticity_bridge_t* mutable_bridge = (attention_plasticity_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    *stats = bridge->stats;

    /* Compute averages */
    uint64_t total_events = stats->ltp_events + stats->ltd_events;
    if (total_events > 0) {
        stats->avg_weight_change /= (float)total_events;
    }

    if (stats->total_focus_events > 0) {
        stats->avg_attention_modulation = bridge->current_attention_mod;
    }

    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

void attention_plasticity_reset_stats(attention_plasticity_bridge_t* bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    memset(&bridge->stats, 0, sizeof(attention_plasticity_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);
}

//=============================================================================
// Callbacks
//=============================================================================

int attention_plasticity_set_weight_callback(
    attention_plasticity_bridge_t* bridge,
    attention_weight_change_cb callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_set_weight_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->weight_callback = callback;
    bridge->weight_callback_data = user_data;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int attention_plasticity_set_shift_callback(
    attention_plasticity_bridge_t* bridge,
    attention_shift_cb callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_set_shift_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->shift_callback = callback;
    bridge->shift_callback_data = user_data;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Modulation Functions
//=============================================================================

int attention_plasticity_set_attention_modulation(
    attention_plasticity_bridge_t* bridge,
    float attention_level)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_set_attention_modulation: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_attention_mod = nimcp_clampf(attention_level, 0.1f, 2.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int attention_plasticity_set_salience_modulation(
    attention_plasticity_bridge_t* bridge,
    float salience_level)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_set_salience_modulation: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_salience_mod = nimcp_clampf(salience_level, 0.0f, 2.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int attention_plasticity_connect_bio_async(attention_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) return 0;

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Registration would happen here via bio-async API */
    bridge->bio_async_connected = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    return bridge->bio_async_connected ? 0 : -1;
}

int attention_plasticity_disconnect_bio_async(attention_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_plasticity_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->bio_async_connected) {
        bridge->bio_async_connected = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool attention_plasticity_is_bio_async_connected(const attention_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    attention_plasticity_bridge_heartbeat("attention_pl_attention_plasticity", 0.0f);


    return bridge->bio_async_connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void attention_plasticity_bridge_set_instance_health_agent(attention_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "attention_plasticity_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int attention_plasticity_bridge_training_begin(attention_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "attention_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    attention_plasticity_bridge_heartbeat_instance(bridge->health_agent, "attention_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int attention_plasticity_bridge_training_end(attention_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "attention_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    attention_plasticity_bridge_heartbeat_instance(bridge->health_agent, "attention_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int attention_plasticity_bridge_training_step(attention_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "attention_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    attention_plasticity_bridge_heartbeat_instance(bridge->health_agent, "attention_plasticity_bridge_training_step", progress);
    return 0;
}
