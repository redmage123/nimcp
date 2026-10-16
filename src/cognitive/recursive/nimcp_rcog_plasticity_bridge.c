/**
 * @file nimcp_rcog_plasticity_bridge.c
 * @brief Recursive Cognition - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/recursive/nimcp_rcog_plasticity_bridge.h"
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

BRIDGE_BOILERPLATE(rcog_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int rcog_plasticity_bridge_training_begin(rcog_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    rcog_plasticity_bridge_heartbeat_instance(
        g_rcog_plasticity_bridge_instance_health_agent, "rcog_plast_training_begin", 0.0f);
    return 0;
}

int rcog_plasticity_bridge_training_step(rcog_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    rcog_plasticity_bridge_heartbeat_instance(
        g_rcog_plasticity_bridge_instance_health_agent, "rcog_plast_training_step", clamped);
    return 0;
}

int rcog_plasticity_bridge_training_end(rcog_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    rcog_plasticity_bridge_heartbeat_instance(
        g_rcog_plasticity_bridge_instance_health_agent, "rcog_plast_training_end", 1.0f);
    return 0;
}

#define LOG_MODULE "RCOG_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

typedef struct synapse_entry {
    rcog_plasticity_synapse_t synapse;
    bool in_use;
} synapse_entry_t;

struct rcog_plasticity_bridge {
    bridge_base_t base;
    rcog_plasticity_config_t config;

    /* State */
    rcog_plasticity_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Synapse storage */
    synapse_entry_t* synapses;
    uint32_t synapse_count;
    uint32_t max_synapses;

    /* Strategy state */
    rcog_strategy_state_t strategy_state;

    /* Global learning state */
    float current_reward;
    float learning_rate_effective;
    float bcm_global_threshold;

    /* Callbacks */
    rcog_plasticity_learn_callback_t learn_callback;
    void* learn_callback_data;
    rcog_plasticity_strategy_callback_t strategy_callback;
    void* strategy_callback_data;

    /* Statistics */
    rcog_plasticity_stats_t stats;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

static synapse_entry_t* find_synapse(rcog_plasticity_bridge_t* bridge, uint32_t synapse_id) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            rcog_plasticity_bridge_heartbeat("rcog_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static synapse_entry_t* find_free_slot(rcog_plasticity_bridge_t* bridge) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            rcog_plasticity_bridge_heartbeat("rcog_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (!bridge->synapses[i].in_use) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}

static bool is_protected_type(rcog_synapse_type_t type) {
    return type == RCOG_SYNAPSE_DEPTH_CONTROL ||
           type == RCOG_SYNAPSE_META_COGNITIVE;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

rcog_plasticity_config_t rcog_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_conf", 0.0f);


    rcog_plasticity_config_t config = {
        .base_learning_rate = RCOG_PLASTICITY_DEFAULT_LR,
        .stdp_tau_plus_ms = 20.0f,
        .stdp_tau_minus_ms = 20.0f,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,

        .bcm_tau_ms = 1000.0f,
        .bcm_target_rate = 5.0f,

        .homeostatic_tau_ms = 10000.0f,
        .target_depth = 0.5f,

        .depth_learning_boost = 1.5f,
        .decomp_learning_boost = 1.3f,
        .aggregation_modulation = 1.2f,

        .weight_min = 0.0f,
        .weight_max = 1.0f,

        .protect_depth_control = true,
        .protect_meta_cognitive = true,
        .protection_strength = 1.0f,

        .max_synapses = RCOG_PLASTICITY_MAX_SYNAPSES,

        .enable_bio_async = false
    };
    return config;
}

rcog_plasticity_bridge_t* rcog_plasticity_create(
    const rcog_plasticity_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_crea", 0.0f);


    rcog_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(rcog_plasticity_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = rcog_plasticity_config_default();
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "rcog_plasticity") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "rcog_plasticity_create: validation failed");
        return NULL;
    }

    /* Allocate synapse storage */
    bridge->max_synapses = bridge->config.max_synapses;
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(synapse_entry_t));
    if (!bridge->synapses) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_plasticity_create: bridge->synapses is NULL");
        return NULL;
    }

    /* Initialize strategy state */
    bridge->strategy_state.depth_preference = 0.5f;
    bridge->strategy_state.decomposition_calibration = 0.5f;
    bridge->strategy_state.aggregation_strength = 0.5f;
    bridge->strategy_state.meta_cognitive_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->strategy_state.self_reference_handling = 0.5f;
    bridge->strategy_state.learning_rate_mod = 1.0f;
    bridge->strategy_state.last_learning_us = 0;

    bridge->state = RCOG_PLASTICITY_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->synapse_count = 0;

    bridge->current_reward = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;
    bridge->bcm_global_threshold = bridge->config.bcm_target_rate;

    return bridge;
}

void rcog_plasticity_destroy(rcog_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "rcog_plasticity");

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_dest", 0.0f);


    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->synapses);
    nimcp_free(bridge);
    bridge = NULL;
}

int rcog_plasticity_reset(rcog_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_rese", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all synapses to initial weights */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            rcog_plasticity_bridge_heartbeat("rcog_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.weight = bridge->synapses[i].synapse.initial_weight;
            bridge->synapses[i].synapse.eligibility_trace = 0.0f;
            bridge->synapses[i].synapse.bcm_threshold = bridge->config.bcm_target_rate;
            bridge->synapses[i].synapse.avg_activity = 0.0f;
            bridge->synapses[i].synapse.update_count = 0;
        }
    }

    /* Reset strategy state */
    bridge->strategy_state.depth_preference = 0.5f;
    bridge->strategy_state.decomposition_calibration = 0.5f;
    bridge->strategy_state.aggregation_strength = 0.5f;
    bridge->strategy_state.meta_cognitive_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->strategy_state.learning_rate_mod = 1.0f;

    bridge->state = RCOG_PLASTICITY_STATE_IDLE;
    bridge->current_reward = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int rcog_plasticity_register_synapse(
    rcog_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    rcog_synapse_type_t type,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_register_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_regi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check for duplicate */
    if (find_synapse(bridge, synapse_id)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Find free slot */
    synapse_entry_t* slot = find_free_slot(bridge);
    if (!slot) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_register_synapse: slot is NULL");
        return -1;
    }

    /* Initialize synapse */
    slot->in_use = true;
    slot->synapse.synapse_id = synapse_id;
    slot->synapse.type = type;
    slot->synapse.weight = nimcp_clampf(initial_weight, bridge->config.weight_min, bridge->config.weight_max);
    slot->synapse.initial_weight = slot->synapse.weight;
    slot->synapse.eligibility_trace = 0.0f;
    slot->synapse.bcm_threshold = bridge->config.bcm_target_rate;
    slot->synapse.avg_activity = 0.0f;
    slot->synapse.last_update_us = bridge->current_time_us;
    slot->synapse.update_count = 0;

    /* Auto-protect depth control and meta-cognitive synapses */
    slot->synapse.is_protected = is_protected_type(type) &&
        (bridge->config.protect_depth_control || bridge->config.protect_meta_cognitive);

    bridge->synapse_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int rcog_plasticity_unregister_synapse(
    rcog_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_unre", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_unregister_synapse: entry is NULL");
        return -1;
    }

    entry->in_use = false;
    bridge->synapse_count--;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int rcog_plasticity_get_synapse(
    rcog_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    rcog_plasticity_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_get_synapse: entry is NULL");
        return -1;
    }

    *synapse = entry->synapse;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int rcog_plasticity_protect_synapse(
    rcog_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_protect_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_prot", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_protect_synapse: entry is NULL");
        return -1;
    }

    entry->synapse.is_protected = protect;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Learning Functions
//=============================================================================

int rcog_plasticity_learn(
    rcog_plasticity_bridge_t* bridge,
    rcog_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_learn: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_lear", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = RCOG_PLASTICITY_STATE_LEARNING;

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        bridge->state = RCOG_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_learn: entry is NULL");
        return -1;
    }

    /* Check protection */
    if (entry->synapse.is_protected) {
        bridge->stats.protected_updates_blocked++;
        bridge->state = RCOG_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0; /* Not an error, just blocked */
    }

    float lr = bridge->learning_rate_effective * magnitude;
    float weight_change = 0.0f;

    switch (event) {
        case RCOG_LEARN_DEPTH_OPTIMAL:
            weight_change = lr * bridge->config.depth_learning_boost;
            bridge->stats.depth_optimal_events++;
            break;

        case RCOG_LEARN_DEPTH_TOO_DEEP:
            weight_change = -lr * 0.5f;
            bridge->stats.depth_correction_events++;
            break;

        case RCOG_LEARN_DEPTH_TOO_SHALLOW:
            weight_change = -lr * 0.3f;
            bridge->stats.depth_correction_events++;
            break;

        case RCOG_LEARN_DECOMP_SUCCESS:
            weight_change = lr * bridge->config.decomp_learning_boost;
            bridge->stats.decomp_success_events++;
            break;

        case RCOG_LEARN_DECOMP_FAILURE:
            weight_change = -lr * 0.4f;
            break;

        case RCOG_LEARN_AGGREGATION_GOOD:
            weight_change = lr * bridge->config.aggregation_modulation;
            bridge->stats.aggregation_learning_events++;
            break;

        case RCOG_LEARN_AGGREGATION_POOR:
            weight_change = -lr * 0.3f;
            bridge->stats.aggregation_learning_events++;
            break;

        case RCOG_LEARN_SELF_REF_RESOLVED:
            weight_change = lr * 0.8f;
            break;

        case RCOG_LEARN_SELF_REF_STUCK:
            weight_change = -lr * 0.5f;
            break;

        case RCOG_LEARN_META_INSIGHT:
            weight_change = lr * 1.0f;
            break;
    }

    /* Context modulation */
    weight_change *= (0.5f + 0.5f * context);

    /* Apply weight change */
    float old_weight = entry->synapse.weight;
    entry->synapse.weight = nimcp_clampf(
        entry->synapse.weight + weight_change,
        bridge->config.weight_min,
        bridge->config.weight_max
    );

    /* Update statistics */
    float actual_change = entry->synapse.weight - old_weight;
    if (actual_change > 0) {
        bridge->stats.total_potentiation += actual_change;
    } else {
        bridge->stats.total_depression += -actual_change;
    }

    entry->synapse.update_count++;
    entry->synapse.last_update_us = bridge->current_time_us;

    bridge->stats.total_learning_events++;
    bridge->stats.weight_updates++;
    bridge->stats.mean_weight_change =
        (bridge->stats.mean_weight_change * (bridge->stats.weight_updates - 1) +
         fabsf(actual_change)) / bridge->stats.weight_updates;

    /* Invoke callback */
    if (bridge->learn_callback) {
        bridge->learn_callback(bridge, event, magnitude, bridge->learn_callback_data);
    }

    bridge->state = RCOG_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float rcog_plasticity_apply_stdp(
    rcog_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
) {
    if (!bridge) return NAN;

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_appl", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NAN;
    }

    /* Protected synapses don't get STDP */
    if (entry->synapse.is_protected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0.0f;
    }

    float dt = post_time - pre_time;
    float delta_w = 0.0f;

    if (dt > 0) {
        /* Potentiation: post after pre */
        delta_w = bridge->config.stdp_a_plus * expf(-dt / bridge->config.stdp_tau_plus_ms);
    } else {
        /* Depression: pre after post */
        delta_w = -bridge->config.stdp_a_minus * expf(dt / bridge->config.stdp_tau_minus_ms);
    }

    /* Apply weight change */
    float old_weight = entry->synapse.weight;
    entry->synapse.weight = nimcp_clampf(
        entry->synapse.weight + delta_w,
        bridge->config.weight_min,
        bridge->config.weight_max
    );

    float actual_change = entry->synapse.weight - old_weight;
    if (actual_change > 0) {
        bridge->stats.total_potentiation += actual_change;
    } else {
        bridge->stats.total_depression += -actual_change;
    }

    entry->synapse.update_count++;
    bridge->stats.weight_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return delta_w;
}

int rcog_plasticity_apply_reward(
    rcog_plasticity_bridge_t* bridge,
    float reward
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_apply_reward: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_appl", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    reward = nimcp_clampf(reward, -1.0f, 1.0f);
    bridge->current_reward = reward;

    /* Apply reward modulation to all eligible synapses */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            rcog_plasticity_bridge_heartbeat("rcog_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use && !bridge->synapses[i].synapse.is_protected) {
            float trace = bridge->synapses[i].synapse.eligibility_trace;
            if (fabsf(trace) > 0.001f) {
                float delta = bridge->config.base_learning_rate * reward * trace;
                bridge->synapses[i].synapse.weight = nimcp_clampf(
                    bridge->synapses[i].synapse.weight + delta,
                    bridge->config.weight_min,
                    bridge->config.weight_max
                );
                bridge->stats.weight_updates++;
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int rcog_plasticity_update_bcm(
    rcog_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_update_bcm: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_plasticity_update_bcm: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_upda", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = RCOG_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.bcm_tau_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            rcog_plasticity_bridge_heartbeat("rcog_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            /* Update sliding threshold towards average activity */
            float target = bridge->synapses[i].synapse.avg_activity;
            bridge->synapses[i].synapse.bcm_threshold =
                bridge->synapses[i].synapse.bcm_threshold * decay +
                target * (1.0f - decay);
        }
    }

    bridge->state = RCOG_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int rcog_plasticity_homeostatic_update(
    rcog_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_homeostatic_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_home", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = RCOG_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.homeostatic_tau_ms);

    /* Calculate mean depth preference from depth control synapses */
    float mean_depth = 0.0f;
    uint32_t depth_count = 0;
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            rcog_plasticity_bridge_heartbeat("rcog_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.type == RCOG_SYNAPSE_DEPTH_CONTROL) {
            mean_depth += bridge->synapses[i].synapse.weight;
            depth_count++;
        }
    }
    if (depth_count > 0) {
        mean_depth /= depth_count;
    }

    /* Scale non-protected synapses toward target depth */
    float target = bridge->config.target_depth;
    float scale_factor = 1.0f;
    if (mean_depth > 0.0f) {
        scale_factor = target / mean_depth;
        scale_factor = nimcp_clampf(scale_factor, 0.9f, 1.1f);
    }

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            rcog_plasticity_bridge_heartbeat("rcog_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use && !bridge->synapses[i].synapse.is_protected) {
            float scaled = bridge->synapses[i].synapse.weight * (1.0f + (scale_factor - 1.0f) * (1.0f - decay));
            bridge->synapses[i].synapse.weight = nimcp_clampf(
                scaled,
                bridge->config.weight_min,
                bridge->config.weight_max
            );
        }
    }

    bridge->state = RCOG_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int rcog_plasticity_update_traces(
    rcog_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_update_traces: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_upda", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float decay = expf(-dt_ms / bridge->config.stdp_tau_plus_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            rcog_plasticity_bridge_heartbeat("rcog_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace *= decay;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int rcog_plasticity_consolidate(rcog_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_cons", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = RCOG_PLASTICITY_STATE_CONSOLIDATING;

    /* Clear eligibility traces */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            rcog_plasticity_bridge_heartbeat("rcog_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace = 0.0f;
        }
    }

    /* Update strategy state based on synapse weights */
    float depth_sum = 0.0f, depth_count = 0;
    float decomp_sum = 0.0f, decomp_count = 0;
    float agg_sum = 0.0f, agg_count = 0;
    float meta_sum = 0.0f, meta_count = 0;
    float self_ref_sum = 0.0f, self_ref_count = 0;

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            rcog_plasticity_bridge_heartbeat("rcog_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            switch (bridge->synapses[i].synapse.type) {
                case RCOG_SYNAPSE_DEPTH_CONTROL:
                    depth_sum += bridge->synapses[i].synapse.weight;
                    depth_count++;
                    break;
                case RCOG_SYNAPSE_DECOMPOSITION:
                    decomp_sum += bridge->synapses[i].synapse.weight;
                    decomp_count++;
                    break;
                case RCOG_SYNAPSE_AGGREGATION:
                    agg_sum += bridge->synapses[i].synapse.weight;
                    agg_count++;
                    break;
                case RCOG_SYNAPSE_META_COGNITIVE:
                    meta_sum += bridge->synapses[i].synapse.weight;
                    meta_count++;
                    break;
                case RCOG_SYNAPSE_SELF_REFERENCE:
                    self_ref_sum += bridge->synapses[i].synapse.weight;
                    self_ref_count++;
                    break;
                default:
                    break;
            }
        }
    }

    float old_depth_pref = bridge->strategy_state.depth_preference;

    if (depth_count > 0) {
        bridge->strategy_state.depth_preference = depth_sum / depth_count;
    }
    if (decomp_count > 0) {
        bridge->strategy_state.decomposition_calibration = decomp_sum / decomp_count;
    }
    if (agg_count > 0) {
        bridge->strategy_state.aggregation_strength = agg_sum / agg_count;
    }
    if (meta_count > 0) {
        bridge->strategy_state.meta_cognitive_sensitivity = meta_sum / meta_count * 2.0f;
    }
    if (self_ref_count > 0) {
        bridge->strategy_state.self_reference_handling = self_ref_sum / self_ref_count;
    }

    bridge->strategy_state.last_learning_us = bridge->current_time_us;
    bridge->state = RCOG_PLASTICITY_STATE_IDLE;

    /* Invoke strategy callback if depth preference changed significantly */
    if (bridge->strategy_callback &&
        fabsf(bridge->strategy_state.depth_preference - old_depth_pref) > 0.05f) {
        bridge->strategy_callback(bridge, old_depth_pref,
                                  bridge->strategy_state.depth_preference,
                                  bridge->strategy_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// State Query Functions
//=============================================================================

int rcog_plasticity_get_strategy_state(
    rcog_plasticity_bridge_t* bridge,
    rcog_strategy_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_get_strategy_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->strategy_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_plasticity_get_state(
    rcog_plasticity_bridge_t* bridge,
    rcog_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->active_synapses = bridge->synapse_count;
    state->learning_rate_effective = bridge->learning_rate_effective;

    /* Calculate mean weight and variance */
    float sum = 0.0f;
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            rcog_plasticity_bridge_heartbeat("rcog_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            float w = bridge->synapses[i].synapse.weight;
            sum += w;
            sum_sq += w * w;
        }
    }

    if (bridge->synapse_count > 0) {
        state->mean_weight = sum / bridge->synapse_count;
        state->weight_variance = (sum_sq / bridge->synapse_count) -
                                (state->mean_weight * state->mean_weight);
    } else {
        state->mean_weight = 0.0f;
        state->weight_variance = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int rcog_plasticity_get_stats(
    rcog_plasticity_bridge_t* bridge,
    rcog_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_plasticity_reset_stats(rcog_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_rese", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(rcog_plasticity_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int rcog_plasticity_register_learn_callback(
    rcog_plasticity_bridge_t* bridge,
    rcog_plasticity_learn_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_register_learn_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_regi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->learn_callback = callback;
    bridge->learn_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_plasticity_register_strategy_callback(
    rcog_plasticity_bridge_t* bridge,
    rcog_plasticity_strategy_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_register_strategy_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_regi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->strategy_callback = callback;
    bridge->strategy_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int rcog_plasticity_bio_async_connect(rcog_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_bio_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_plasticity_bio_async_disconnect(rcog_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_plasticity_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_bio_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool rcog_plasticity_is_bio_async_connected(rcog_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_plasticity_bridge_heartbeat("rcog_plastic_rcog_plasticity_is_b", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}
