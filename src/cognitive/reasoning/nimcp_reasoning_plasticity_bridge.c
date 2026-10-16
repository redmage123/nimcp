/**
 * @file nimcp_reasoning_plasticity_bridge.c
 * @brief Reasoning - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/reasoning/nimcp_reasoning_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(reasoning_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "REASONING_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct reasoning_plasticity_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    reasoning_plasticity_config_t config;

    /* State */
    reasoning_plasticity_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Synapses */
    reasoning_plasticity_synapse_t* synapses;
    uint32_t num_synapses;
    uint32_t max_synapses;

    /* Calibration state */
    reasoning_calibration_state_t calibration;

    /* Reward eligibility */
    float global_eligibility;
    float last_reward;

    /* Callbacks */
    reasoning_plasticity_learn_callback_t learn_callback;
    void* learn_callback_data;
    reasoning_plasticity_calibration_callback_t calibration_callback;
    void* calibration_callback_data;

    /* Statistics */
    reasoning_plasticity_stats_t stats;
};

BRIDGE_DEFINE_SECURITY_SETTERS(reasoning_plasticity_bridge)

static reasoning_plasticity_synapse_t* find_synapse(
    reasoning_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            reasoning_plasticity_bridge_heartbeat("reasoning_pl_loop",
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

reasoning_plasticity_config_t reasoning_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    reasoning_plasticity_config_t config = {
        .base_learning_rate = REASONING_PLASTICITY_DEFAULT_LR,
        .stdp_tau_plus_ms = 20.0f,
        .stdp_tau_minus_ms = 25.0f,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,

        .bcm_tau_ms = 1000.0f,
        .bcm_target_rate = 10.0f,

        .homeostatic_tau_ms = 10000.0f,
        .target_inference_accuracy = 0.8f,

        .accuracy_learning_boost = 2.0f,
        .error_learning_boost = 1.5f,
        .causal_modulation = 1.2f,

        .weight_min = 0.0f,
        .weight_max = 2.0f,

        .protect_deduction = true,
        .protect_causal = true,
        .protection_strength = 0.9f,

        .max_synapses = REASONING_PLASTICITY_MAX_SYNAPSES,

        .enable_bio_async = false
    };
    return config;
}

reasoning_plasticity_bridge_t* reasoning_plasticity_create(
    const reasoning_plasticity_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    reasoning_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(reasoning_plasticity_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = reasoning_plasticity_config_default();
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "reasoning_plasticity") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "reasoning_plasticity_create: validation failed");
        return NULL;
    }

    /* Allocate synapse storage */
    bridge->max_synapses = bridge->config.max_synapses;
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(reasoning_plasticity_synapse_t));
    if (!bridge->synapses) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reasoning_plasticity_create: bridge->synapses is NULL");
        return NULL;
    }

    bridge->num_synapses = 0;
    bridge->state = REASONING_PLASTICITY_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->global_eligibility = 0.0f;
    bridge->last_reward = 0.0f;

    /* Initialize calibration state */
    bridge->calibration.deduction_strength = 0.5f;
    bridge->calibration.induction_accuracy = 0.5f;
    bridge->calibration.causal_sensitivity = 0.5f;
    bridge->calibration.analogy_matching = 0.5f;
    bridge->calibration.evidence_weighting = 0.5f;
    bridge->calibration.learning_rate_mod = 1.0f;
    bridge->calibration.last_learning_us = 0;

    return bridge;
}

void reasoning_plasticity_destroy(reasoning_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "reasoning_plasticity");

    /* Cleanup base bridge infrastructure */
    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->synapses);
    nimcp_free(bridge);
    bridge = NULL;
}

int reasoning_plasticity_reset(reasoning_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all synapses to initial weights */
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            reasoning_plasticity_bridge_heartbeat("reasoning_pl_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        bridge->synapses[i].weight = bridge->synapses[i].initial_weight;
        bridge->synapses[i].eligibility_trace = 0.0f;
        bridge->synapses[i].bcm_threshold = bridge->config.bcm_target_rate;
        bridge->synapses[i].avg_activity = 0.0f;
        bridge->synapses[i].update_count = 0;
    }

    /* Reset calibration state */
    bridge->calibration.deduction_strength = 0.5f;
    bridge->calibration.induction_accuracy = 0.5f;
    bridge->calibration.causal_sensitivity = 0.5f;
    bridge->calibration.analogy_matching = 0.5f;
    bridge->calibration.evidence_weighting = 0.5f;
    bridge->calibration.learning_rate_mod = 1.0f;

    bridge->global_eligibility = 0.0f;
    bridge->last_reward = 0.0f;
    bridge->state = REASONING_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int reasoning_plasticity_register_synapse(
    reasoning_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    reasoning_synapse_type_t type,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_register_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already exists */
    if (find_synapse(bridge, synapse_id)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Check capacity */
    if (bridge->num_synapses >= bridge->max_synapses) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "reasoning_plasticity_register_synapse: capacity exceeded");
        return -1;
    }

    /* Add synapse */
    reasoning_plasticity_synapse_t* syn = &bridge->synapses[bridge->num_synapses];
    syn->synapse_id = synapse_id;
    syn->type = type;
    syn->weight = nimcp_clampf(initial_weight, bridge->config.weight_min, bridge->config.weight_max);
    syn->initial_weight = syn->weight;
    syn->eligibility_trace = 0.0f;
    syn->bcm_threshold = bridge->config.bcm_target_rate;
    syn->avg_activity = 0.0f;
    syn->last_update_us = bridge->current_time_us;
    syn->update_count = 0;

    /* Auto-protect deduction and causal synapses */
    syn->is_protected = false;
    if (bridge->config.protect_deduction && type == REASON_SYNAPSE_DEDUCTION) {
        syn->is_protected = true;
    }
    if (bridge->config.protect_causal && type == REASON_SYNAPSE_CAUSAL) {
        syn->is_protected = true;
    }

    bridge->num_synapses++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_plasticity_unregister_synapse(
    reasoning_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            reasoning_plasticity_bridge_heartbeat("reasoning_pl_loop",
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
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_plasticity_unregister_synapse: operation failed");
    return -1;
}

int reasoning_plasticity_get_synapse(
    reasoning_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    reasoning_plasticity_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    reasoning_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (!syn) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_get_synapse: syn is NULL");
        return -1;
    }

    *synapse = *syn;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_plasticity_protect_synapse(
    reasoning_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_protect_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    reasoning_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (!syn) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_protect_synapse: syn is NULL");
        return -1;
    }

    syn->is_protected = protect;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Learning Functions
//=============================================================================

int reasoning_plasticity_learn(
    reasoning_plasticity_bridge_t* bridge,
    reasoning_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_learn: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = REASONING_PLASTICITY_STATE_LEARNING;

    reasoning_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (!syn) {
        bridge->state = REASONING_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_learn: syn is NULL");
        return -1;
    }

    /* Check protection */
    if (syn->is_protected) {
        bridge->stats.protected_updates_blocked++;
        bridge->state = REASONING_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Calculate learning rate with modulation */
    float lr = bridge->config.base_learning_rate * bridge->calibration.learning_rate_mod;

    /* Apply event-specific learning */
    float delta_weight = 0.0f;
    switch (event) {
        case REASON_LEARN_VALID_CONCLUSION:
            delta_weight = lr * magnitude * bridge->config.accuracy_learning_boost;
            bridge->stats.valid_conclusion_events++;
            break;

        case REASON_LEARN_INVALID_CONCLUSION:
            delta_weight = -lr * magnitude * bridge->config.error_learning_boost;
            bridge->stats.invalid_conclusion_events++;
            break;

        case REASON_LEARN_CAUSAL_CONFIRMED:
            delta_weight = lr * magnitude * bridge->config.causal_modulation;
            bridge->stats.causal_learning_events++;
            break;

        case REASON_LEARN_CAUSAL_REFUTED:
            delta_weight = -lr * magnitude * bridge->config.causal_modulation;
            bridge->stats.causal_learning_events++;
            break;

        case REASON_LEARN_ANALOGY_MATCHED:
            delta_weight = lr * magnitude * context;
            bridge->stats.analogy_learning_events++;
            break;

        case REASON_LEARN_ANALOGY_FAILED:
            delta_weight = -lr * magnitude * 0.5f;
            bridge->stats.analogy_learning_events++;
            break;

        case REASON_LEARN_EVIDENCE_INTEGRATED:
            delta_weight = lr * magnitude * context;
            break;

        case REASON_LEARN_CONFLICT_RESOLVED:
            delta_weight = lr * magnitude * bridge->config.error_learning_boost;
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
    if (n > 0.0f) {
        bridge->stats.mean_weight_change = bridge->stats.mean_weight_change * ((n - 1) / n) +
                                           fabsf(actual_delta) / n;
    }

    syn->last_update_us = bridge->current_time_us;
    syn->update_count++;
    bridge->calibration.last_learning_us = bridge->current_time_us;

    /* Invoke callback */
    if (bridge->learn_callback) {
        bridge->learn_callback(bridge, event, magnitude, bridge->learn_callback_data);
    }

    bridge->state = REASONING_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float reasoning_plasticity_apply_stdp(
    reasoning_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
) {
    if (!bridge) return NAN;

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    reasoning_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
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

int reasoning_plasticity_apply_reward(
    reasoning_plasticity_bridge_t* bridge,
    float reward
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_apply_reward: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    reward = nimcp_clampf(reward, -1.0f, 1.0f);
    bridge->last_reward = reward;

    /* Apply reward-modulated learning to all synapses with eligibility */
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            reasoning_plasticity_bridge_heartbeat("reasoning_pl_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        reasoning_plasticity_synapse_t* syn = &bridge->synapses[i];

        if (syn->is_protected || syn->eligibility_trace < 0.001f) {
            continue;
        }

        float delta = bridge->config.base_learning_rate * reward *
                     syn->eligibility_trace * bridge->config.causal_modulation;

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

int reasoning_plasticity_update_bcm(
    reasoning_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_update_bcm: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_plasticity_update_bcm: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float decay = expf(-dt_ms / bridge->config.bcm_tau_ms);

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            reasoning_plasticity_bridge_heartbeat("reasoning_pl_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        reasoning_plasticity_synapse_t* syn = &bridge->synapses[i];

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

int reasoning_plasticity_homeostatic_update(
    reasoning_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_homeostatic_update: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_plasticity_homeostatic_update: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "reasoning_plasticity_homeostatic_update");
    BRIDGE_LGSS_GATE(bridge, "reasoning_plasticity_homeostatic_update");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Calculate mean weight */
    float mean_weight = 0.0f;
    uint32_t active_count = 0;

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            reasoning_plasticity_bridge_heartbeat("reasoning_pl_loop",
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
            reasoning_plasticity_bridge_heartbeat("reasoning_pl_loop",
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

    /* Update calibration accuracy towards target */
    float acc_decay = expf(-dt_ms / bridge->config.homeostatic_tau_ms);
    float old_calib = bridge->calibration.deduction_strength;
    bridge->calibration.deduction_strength =
        old_calib * acc_decay + bridge->config.target_inference_accuracy * (1.0f - acc_decay);

    /* Invoke calibration callback if significant change */
    if (bridge->calibration_callback &&
        fabsf(bridge->calibration.deduction_strength - old_calib) > 0.01f) {
        bridge->calibration_callback(bridge, old_calib,
                                     bridge->calibration.deduction_strength,
                                     bridge->calibration_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int reasoning_plasticity_update_traces(
    reasoning_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_update_traces: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_plasticity_update_traces: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float decay = expf(-dt_ms / bridge->config.stdp_tau_plus_ms);

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            reasoning_plasticity_bridge_heartbeat("reasoning_pl_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        bridge->synapses[i].eligibility_trace *= decay;
    }

    bridge->global_eligibility *= decay;
    bridge->current_time_us += (uint64_t)(dt_ms * 1000.0f);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_plasticity_consolidate(reasoning_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = REASONING_PLASTICITY_STATE_CONSOLIDATING;

    /* Consolidate learning by resetting eligibility traces */
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            reasoning_plasticity_bridge_heartbeat("reasoning_pl_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        bridge->synapses[i].eligibility_trace = 0.0f;
    }

    bridge->global_eligibility = 0.0f;
    bridge->state = REASONING_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// State Query Functions
//=============================================================================

int reasoning_plasticity_get_calibration_state(
    reasoning_plasticity_bridge_t* bridge,
    reasoning_calibration_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_get_calibration_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->calibration;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int reasoning_plasticity_get_state(
    reasoning_plasticity_bridge_t* bridge,
    reasoning_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->active_synapses = bridge->num_synapses;

    /* Calculate mean weight and variance */
    float sum = 0.0f;
    float sum_sq = 0.0f;

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            reasoning_plasticity_bridge_heartbeat("reasoning_pl_loop",
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

int reasoning_plasticity_get_stats(
    reasoning_plasticity_bridge_t* bridge,
    reasoning_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int reasoning_plasticity_reset_stats(reasoning_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(reasoning_plasticity_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int reasoning_plasticity_register_learn_callback(
    reasoning_plasticity_bridge_t* bridge,
    reasoning_plasticity_learn_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_register_learn_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->learn_callback = callback;
    bridge->learn_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int reasoning_plasticity_register_calibration_callback(
    reasoning_plasticity_bridge_t* bridge,
    reasoning_plasticity_calibration_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_register_calibration_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->calibration_callback = callback;
    bridge->calibration_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int reasoning_plasticity_bio_async_connect(reasoning_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int reasoning_plasticity_bio_async_disconnect(reasoning_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_plasticity_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool reasoning_plasticity_is_bio_async_connected(reasoning_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_plasticity_bridge_heartbeat("reasoning_pl_reasoning_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void reasoning_plasticity_bridge_set_instance_health_agent(reasoning_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "reasoning_plasticity_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int reasoning_plasticity_bridge_training_begin(reasoning_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    reasoning_plasticity_bridge_heartbeat_instance(bridge->health_agent, "reasoning_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int reasoning_plasticity_bridge_training_end(reasoning_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    reasoning_plasticity_bridge_heartbeat_instance(bridge->health_agent, "reasoning_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int reasoning_plasticity_bridge_training_step(reasoning_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    reasoning_plasticity_bridge_heartbeat_instance(bridge->health_agent, "reasoning_plasticity_bridge_training_step", progress);
    return 0;
}
