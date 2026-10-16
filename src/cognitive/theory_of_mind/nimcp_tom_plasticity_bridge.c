/**
 * @file nimcp_tom_plasticity_bridge.c
 * @brief Theory of Mind - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/theory_of_mind/nimcp_tom_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(tom_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "TOM_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct tom_plasticity_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    tom_plasticity_config_t config;

    /* State */
    tom_plasticity_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Synapses */
    tom_plasticity_synapse_t* synapses;
    uint32_t num_synapses;
    uint32_t max_synapses;

    /* Calibration state */
    tom_calibration_state_t calibration;

    /* Reward eligibility */
    float global_eligibility;
    float last_reward;

    /* Callbacks */
    tom_plasticity_learn_callback_t learn_callback;
    void* learn_callback_data;
    tom_plasticity_calibration_callback_t calibration_callback;
    void* calibration_callback_data;

    /* Statistics */
    tom_plasticity_stats_t stats;
};

static tom_plasticity_synapse_t* find_synapse(
    tom_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            tom_plasticity_bridge_heartbeat("tom_plastici_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

tom_plasticity_config_t tom_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_confi", 0.0f);


    tom_plasticity_config_t config = {
        .base_learning_rate = TOM_PLASTICITY_DEFAULT_LR,
        .stdp_tau_plus_ms = 20.0f,
        .stdp_tau_minus_ms = 25.0f,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,

        .bcm_tau_ms = 1000.0f,
        .bcm_target_rate = 10.0f,

        .homeostatic_tau_ms = 10000.0f,
        .target_empathy = 0.7f,

        .belief_learning_boost = 2.0f,
        .deception_learning_boost = 1.5f,
        .empathy_modulation = 1.2f,
        .perspective_modulation = 1.0f,

        .weight_min = 0.0f,
        .weight_max = 2.0f,

        .protect_belief_patterns = true,
        .protect_perspective_patterns = true,
        .protection_strength = 0.9f,

        .max_synapses = TOM_PLASTICITY_MAX_SYNAPSES,

        .enable_bio_async = false
    };
    return config;
}

tom_plasticity_bridge_t* tom_plasticity_create(
    const tom_plasticity_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_creat", 0.0f);


    tom_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(tom_plasticity_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = tom_plasticity_config_default();
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, 0, "tom_plasticity") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to initialize bridge base in tom_plasticity_create");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Allocate synapse storage */
    bridge->max_synapses = bridge->config.max_synapses;
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(tom_plasticity_synapse_t));
    if (!bridge->synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate synapses in tom_plasticity_create");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    bridge->num_synapses = 0;
    bridge->state = TOM_PLASTICITY_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->global_eligibility = 0.0f;
    bridge->last_reward = 0.0f;

    /* Initialize calibration state */
    bridge->calibration.belief_sensitivity = 0.5f;
    bridge->calibration.intention_calibration = 0.7f;
    bridge->calibration.perspective_sensitivity = 0.5f;
    bridge->calibration.empathy_strength = 0.5f;
    bridge->calibration.deception_sensitivity = 0.5f;
    bridge->calibration.social_calibration = 0.5f;
    bridge->calibration.learning_rate_mod = 1.0f;
    bridge->calibration.last_learning_us = 0;

    return bridge;
}

void tom_plasticity_destroy(tom_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "tom_plasticity");

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_destr", 0.0f);


    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->synapses);
    nimcp_free(bridge);
    bridge = NULL;
}

int tom_plasticity_reset(tom_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all synapses to initial weights */
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            tom_plasticity_bridge_heartbeat("tom_plastici_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        bridge->synapses[i].weight = bridge->synapses[i].initial_weight;
        bridge->synapses[i].eligibility_trace = 0.0f;
        bridge->synapses[i].bcm_threshold = bridge->config.bcm_target_rate;
        bridge->synapses[i].avg_activity = 0.0f;
        bridge->synapses[i].update_count = 0;
    }

    /* Reset calibration state */
    bridge->calibration.belief_sensitivity = 0.5f;
    bridge->calibration.intention_calibration = 0.7f;
    bridge->calibration.perspective_sensitivity = 0.5f;
    bridge->calibration.empathy_strength = 0.5f;
    bridge->calibration.deception_sensitivity = 0.5f;
    bridge->calibration.social_calibration = 0.5f;
    bridge->calibration.learning_rate_mod = 1.0f;

    bridge->global_eligibility = 0.0f;
    bridge->last_reward = 0.0f;
    bridge->state = TOM_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int tom_plasticity_register_synapse(
    tom_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    tom_synapse_type_t type,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_register_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_regis", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already exists */
    if (find_synapse(bridge, synapse_id)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Check capacity */
    if (bridge->num_synapses >= bridge->max_synapses) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "tom_plasticity_register_synapse: capacity exceeded");
        return -1;
    }

    /* Add synapse */
    tom_plasticity_synapse_t* syn = &bridge->synapses[bridge->num_synapses];
    syn->synapse_id = synapse_id;
    syn->type = type;
    syn->weight = nimcp_clampf(initial_weight, bridge->config.weight_min, bridge->config.weight_max);
    syn->initial_weight = syn->weight;
    syn->eligibility_trace = 0.0f;
    syn->bcm_threshold = bridge->config.bcm_target_rate;
    syn->avg_activity = 0.0f;
    syn->last_update_us = bridge->current_time_us;
    syn->update_count = 0;

    /* Auto-protect belief and perspective patterns (core ToM capabilities) */
    syn->is_protected = false;
    if (bridge->config.protect_belief_patterns && type == TOM_SYNAPSE_BELIEF) {
        syn->is_protected = true;
    }
    if (bridge->config.protect_perspective_patterns && type == TOM_SYNAPSE_PERSPECTIVE) {
        syn->is_protected = true;
    }

    bridge->num_synapses++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int tom_plasticity_unregister_synapse(
    tom_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_unreg", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            tom_plasticity_bridge_heartbeat("tom_plastici_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            /* Move last to current position */
            if (i < bridge->num_synapses - 1) {
                bridge->synapses[i] = bridge->synapses[bridge->num_synapses - 1];
            }
            bridge->num_synapses--;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_plasticity_unregister_synapse: operation failed");
    return -1;
}

int tom_plasticity_get_synapse(
    tom_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    tom_plasticity_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_get_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    tom_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (!syn) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_get_synapse: syn is NULL");
        return -1;
    }

    *synapse = *syn;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int tom_plasticity_protect_synapse(
    tom_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_protect_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_prote", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    tom_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (!syn) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_protect_synapse: syn is NULL");
        return -1;
    }

    syn->is_protected = protect;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Learning Functions
//=============================================================================

int tom_plasticity_learn(
    tom_plasticity_bridge_t* bridge,
    tom_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_learn: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_learn", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = TOM_PLASTICITY_STATE_LEARNING;

    tom_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (!syn) {
        bridge->state = TOM_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_learn: syn is NULL");
        return -1;
    }

    /* Check protection */
    if (syn->is_protected) {
        bridge->stats.protected_updates_blocked++;
        bridge->state = TOM_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Calculate learning rate with modulation */
    float lr = bridge->config.base_learning_rate * bridge->calibration.learning_rate_mod;

    /* Apply event-specific learning */
    float delta_weight = 0.0f;
    switch (event) {
        case TOM_LEARN_CORRECT_BELIEF:
            delta_weight = lr * magnitude * bridge->config.belief_learning_boost;
            bridge->stats.correct_belief_events++;
            break;

        case TOM_LEARN_FALSE_BELIEF_DETECTED:
            delta_weight = lr * magnitude * bridge->config.deception_learning_boost;
            bridge->stats.false_belief_detections++;
            break;

        case TOM_LEARN_FALSE_BELIEF_MISSED:
            delta_weight = -lr * magnitude * bridge->config.deception_learning_boost;
            bridge->stats.false_belief_misses++;
            break;

        case TOM_LEARN_INTENTION_CORRECT:
            delta_weight = lr * magnitude * context;
            break;

        case TOM_LEARN_INTENTION_WRONG:
            delta_weight = -lr * magnitude * 0.5f;
            break;

        case TOM_LEARN_PERSPECTIVE_ALIGNED:
            delta_weight = lr * magnitude * bridge->config.perspective_modulation;
            bridge->stats.perspective_alignments++;
            break;

        case TOM_LEARN_PERSPECTIVE_ERROR:
            delta_weight = -lr * magnitude * bridge->config.perspective_modulation * 0.5f;
            break;

        case TOM_LEARN_EMPATHY_ACCURATE:
            delta_weight = lr * magnitude * bridge->config.empathy_modulation;
            bridge->stats.empathy_accurate_events++;
            break;

        case TOM_LEARN_EMPATHY_ERROR:
            delta_weight = -lr * magnitude * bridge->config.empathy_modulation * 0.5f;
            break;

        case TOM_LEARN_DECEPTION_CORRECT:
            delta_weight = lr * magnitude * bridge->config.deception_learning_boost;
            bridge->stats.deception_detections++;
            break;

        case TOM_LEARN_DECEPTION_MISSED:
            delta_weight = -lr * magnitude * bridge->config.deception_learning_boost;
            break;

        case TOM_LEARN_SOCIAL_CONTEXT_CORRECT:
            delta_weight = lr * magnitude * context;
            break;

        default:
            delta_weight = lr * magnitude;
            break;
    }

    /* Apply eligibility trace modulation */
    delta_weight *= (1.0f + syn->eligibility_trace);

    /* Update weight with bounds */
    float old_weight = syn->weight;
    syn->weight = nimcp_clampf(syn->weight + delta_weight, bridge->config.weight_min, bridge->config.weight_max);
    float actual_delta = syn->weight - old_weight;

    /* Update statistics */
    if (actual_delta > 0) {
        bridge->stats.total_potentiation += actual_delta;
    } else {
        bridge->stats.total_depression += fabsf(actual_delta);
    }
    bridge->stats.weight_updates++;
    bridge->stats.total_learning_events++;

    /* Update running mean */
    float n = (float)bridge->stats.weight_updates;
    bridge->stats.mean_weight_change = bridge->stats.mean_weight_change * ((n - 1) / (fabsf(n) > 1e-7f ? n : 1e-7f)) +
                                       fabsf(actual_delta) / (fabsf(n) > 1e-7f ? n : 1e-7f);

    syn->last_update_us = bridge->current_time_us;
    syn->update_count++;
    bridge->calibration.last_learning_us = bridge->current_time_us;

    /* Invoke callback */
    if (bridge->learn_callback) {
        bridge->learn_callback(bridge, event, magnitude, bridge->learn_callback_data);
    }

    bridge->state = TOM_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float tom_plasticity_apply_stdp(
    tom_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
) {
    if (!bridge) return NAN;

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_apply", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    tom_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (!syn) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NAN;
    }

    /* Check protection */
    if (syn->is_protected) {
        bridge->stats.protected_updates_blocked++;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0.0f;
    }

    float dt = post_time - pre_time;
    float delta_weight = 0.0f;

    if (dt > 0) {
        /* LTP: post after pre */
        delta_weight = bridge->config.stdp_a_plus *
                      expf(-dt / bridge->config.stdp_tau_plus_ms);
    } else if (dt < 0) {
        /* LTD: pre after post */
        delta_weight = -bridge->config.stdp_a_minus *
                      expf(dt / bridge->config.stdp_tau_minus_ms);
    }

    /* Apply with learning rate */
    delta_weight *= bridge->config.base_learning_rate * bridge->calibration.learning_rate_mod;

    /* Update weight */
    float old_weight = syn->weight;
    syn->weight = nimcp_clampf(syn->weight + delta_weight, bridge->config.weight_min, bridge->config.weight_max);
    float actual_delta = syn->weight - old_weight;

    /* Update statistics */
    if (actual_delta > 0) {
        bridge->stats.total_potentiation += actual_delta;
    } else {
        bridge->stats.total_depression += fabsf(actual_delta);
    }
    bridge->stats.weight_updates++;

    syn->last_update_us = bridge->current_time_us;
    syn->update_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return actual_delta;
}

int tom_plasticity_apply_reward(
    tom_plasticity_bridge_t* bridge,
    float reward
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_apply_reward: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_apply", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    reward = nimcp_clampf(reward, -1.0f, 1.0f);
    bridge->last_reward = reward;

    /* Apply reward-modulated learning to all synapses with eligibility */
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            tom_plasticity_bridge_heartbeat("tom_plastici_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        tom_plasticity_synapse_t* syn = &bridge->synapses[i];

        if (syn->is_protected || syn->eligibility_trace < 0.001f) {
            continue;
        }

        float delta = bridge->config.base_learning_rate * reward *
                     syn->eligibility_trace * bridge->config.empathy_modulation;

        syn->weight = nimcp_clampf(syn->weight + delta, bridge->config.weight_min, bridge->config.weight_max);

        if (delta > 0) {
            bridge->stats.total_potentiation += delta;
        } else {
            bridge->stats.total_depression += fabsf(delta);
        }
        bridge->stats.weight_updates++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int tom_plasticity_update_bcm(
    tom_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_update_bcm: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_plasticity_update_bcm: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_updat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float decay = expf(-dt_ms / bridge->config.bcm_tau_ms);

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            tom_plasticity_bridge_heartbeat("tom_plastici_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        tom_plasticity_synapse_t* syn = &bridge->synapses[i];

        /* Update sliding threshold towards average activity */
        syn->bcm_threshold = syn->bcm_threshold * decay +
                            syn->avg_activity * (1.0f - decay);

        /* BCM learning rule: weight change depends on activity relative to threshold */
        if (!syn->is_protected && syn->avg_activity > 0.001f) {
            float bcm_factor = (syn->avg_activity - syn->bcm_threshold) * syn->avg_activity;
            float delta = bridge->config.base_learning_rate * bcm_factor * 0.001f;

            syn->weight = nimcp_clampf(syn->weight + delta, bridge->config.weight_min, bridge->config.weight_max);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int tom_plasticity_homeostatic_update(
    tom_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_homeostatic_update: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_plasticity_homeostatic_update: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_homeo", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Calculate mean weight */
    float mean_weight = 0.0f;
    uint32_t active_count = 0;

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            tom_plasticity_bridge_heartbeat("tom_plastici_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (!bridge->synapses[i].is_protected) {
            mean_weight += bridge->synapses[i].weight;
            active_count++;
        }
    }

    if (active_count > 0) {
        mean_weight /= active_count;
    }

    /* Target weight is mid-range */
    float target = (bridge->config.weight_max + bridge->config.weight_min) / 2.0f;
    float error = target - mean_weight;

    /* Homeostatic scaling */
    float scale_factor = 1.0f + error * dt_ms / bridge->config.homeostatic_tau_ms;
    scale_factor = nimcp_clampf(scale_factor, 0.99f, 1.01f);

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            tom_plasticity_bridge_heartbeat("tom_plastici_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (!bridge->synapses[i].is_protected) {
            bridge->synapses[i].weight = nimcp_clampf(
                bridge->synapses[i].weight * scale_factor,
                bridge->config.weight_min,
                bridge->config.weight_max
            );
        }
    }

    /* Update calibration empathy towards target */
    float emp_decay = expf(-dt_ms / bridge->config.homeostatic_tau_ms);
    float old_calib = bridge->calibration.empathy_strength;
    bridge->calibration.empathy_strength =
        old_calib * emp_decay + bridge->config.target_empathy * (1.0f - emp_decay);

    /* Invoke calibration callback if significant change */
    if (bridge->calibration_callback &&
        fabsf(bridge->calibration.empathy_strength - old_calib) > 0.01f) {
        bridge->calibration_callback(bridge, old_calib,
                                     bridge->calibration.empathy_strength,
                                     bridge->calibration_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int tom_plasticity_update_traces(
    tom_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_update_traces: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_plasticity_update_traces: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_updat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float decay = expf(-dt_ms / bridge->config.stdp_tau_plus_ms);

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            tom_plasticity_bridge_heartbeat("tom_plastici_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        bridge->synapses[i].eligibility_trace *= decay;
    }

    bridge->global_eligibility *= decay;
    bridge->current_time_us += (uint64_t)(dt_ms * 1000.0f);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int tom_plasticity_consolidate(tom_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_conso", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = TOM_PLASTICITY_STATE_CONSOLIDATING;

    /* Consolidate learning by resetting eligibility traces */
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            tom_plasticity_bridge_heartbeat("tom_plastici_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        bridge->synapses[i].eligibility_trace = 0.0f;
    }

    bridge->global_eligibility = 0.0f;
    bridge->state = TOM_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// State Query Functions
//=============================================================================

int tom_plasticity_get_calibration_state(
    tom_plasticity_bridge_t* bridge,
    tom_calibration_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_get_calibration_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_get_c", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->calibration;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int tom_plasticity_get_state(
    tom_plasticity_bridge_t* bridge,
    tom_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_get_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->active_synapses = bridge->num_synapses;

    /* Calculate mean weight and variance */
    float sum = 0.0f;
    float sum_sq = 0.0f;

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            tom_plasticity_bridge_heartbeat("tom_plastici_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        sum += bridge->synapses[i].weight;
        sum_sq += bridge->synapses[i].weight * bridge->synapses[i].weight;
    }

    if (bridge->num_synapses > 0) {
        state->mean_weight = sum / bridge->num_synapses;
        state->weight_variance = (sum_sq / bridge->num_synapses) -
                                (state->mean_weight * state->mean_weight);
    } else {
        state->mean_weight = 0.0f;
        state->weight_variance = 0.0f;
    }

    state->learning_rate_effective = bridge->config.base_learning_rate *
                                     bridge->calibration.learning_rate_mod;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int tom_plasticity_get_stats(
    tom_plasticity_bridge_t* bridge,
    tom_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_get_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int tom_plasticity_reset_stats(tom_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(tom_plasticity_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int tom_plasticity_register_learn_callback(
    tom_plasticity_bridge_t* bridge,
    tom_plasticity_learn_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_register_learn_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_regis", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->learn_callback = callback;
    bridge->learn_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int tom_plasticity_register_calibration_callback(
    tom_plasticity_bridge_t* bridge,
    tom_plasticity_calibration_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_plasticity_register_calibration_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_regis", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->calibration_callback = callback;
    bridge->calibration_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int tom_plasticity_bio_async_connect(tom_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->config.enable_bio_async) return -1;

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_bio_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int tom_plasticity_bio_async_disconnect(tom_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_bio_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool tom_plasticity_is_bio_async_connected(tom_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_plasticity_bridge_heartbeat("tom_plastici_tom_plasticity_is_bi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void tom_plasticity_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_tom_plasticity_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 *
 * Stub: training integration planned — these are intentional no-ops that
 * provide heartbeat signaling only. Full training hooks will wire into the
 * training-immune bridge when per-module gradient propagation is implemented.
 * ============================================================================ */
int tom_plasticity_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    tom_plasticity_bridge_heartbeat("tom_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int tom_plasticity_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    tom_plasticity_bridge_heartbeat("tom_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int tom_plasticity_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    tom_plasticity_bridge_heartbeat("tom_plasticity_bridge_training_step", progress);
    return 0;
}
