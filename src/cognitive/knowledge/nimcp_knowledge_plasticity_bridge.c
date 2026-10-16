/**
 * @file nimcp_knowledge_plasticity_bridge.c
 * @brief Knowledge - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/knowledge/nimcp_knowledge_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/statistics/nimcp_statistics.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(knowledge_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "KNOWLEDGE_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

typedef struct synapse_entry {
    knowledge_plasticity_synapse_t synapse;
    bool in_use;
} synapse_entry_t;

struct knowledge_plasticity_bridge {
    bridge_base_t base;
    knowledge_plasticity_config_t config;

    /* State */
    knowledge_plasticity_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Synapse storage */
    synapse_entry_t* synapses;
    uint32_t synapse_count;
    uint32_t max_synapses;

    /* Consolidation state */
    knowledge_consolidation_state_t consolidation_state;

    /* Global learning state */
    float current_reward;
    float learning_rate_effective;
    float bcm_global_threshold;

    /* Callbacks */
    knowledge_plasticity_learn_callback_t learn_callback;
    void* learn_callback_data;
    knowledge_plasticity_consolidation_callback_t consolidation_callback;
    void* consolidation_callback_data;

    /* Statistics */
    knowledge_plasticity_stats_t stats;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

static synapse_entry_t* find_synapse(knowledge_plasticity_bridge_t* bridge, uint32_t synapse_id) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            knowledge_plasticity_bridge_heartbeat("knowledge_pl_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static synapse_entry_t* find_free_slot(knowledge_plasticity_bridge_t* bridge) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            knowledge_plasticity_bridge_heartbeat("knowledge_pl_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (!bridge->synapses[i].in_use) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}

static bool is_protected_type(knowledge_synapse_type_t type) {
    return type == KNOWLEDGE_SYNAPSE_RETRIEVAL ||
           type == KNOWLEDGE_SYNAPSE_CONFIDENCE;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

knowledge_plasticity_config_t knowledge_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    knowledge_plasticity_config_t config = {
        .base_learning_rate = KNOWLEDGE_PLASTICITY_DEFAULT_LR,
        .stdp_tau_plus_ms = 20.0f,
        .stdp_tau_minus_ms = 20.0f,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,

        .bcm_tau_ms = 1000.0f,
        .bcm_target_rate = 5.0f,

        .homeostatic_tau_ms = 10000.0f,
        .target_activation = 0.5f,

        .retrieval_success_boost = 1.5f,
        .association_learning_boost = 1.3f,
        .semantic_modulation = 1.2f,

        .weight_min = 0.0f,
        .weight_max = 1.0f,

        .protect_retrieval_pathway = true,
        .protect_confidence_pathway = true,
        .protection_strength = 1.0f,

        .max_synapses = KNOWLEDGE_PLASTICITY_MAX_SYNAPSES,

        .enable_bio_async = false
    };
    return config;
}

knowledge_plasticity_bridge_t* knowledge_plasticity_create(
    const knowledge_plasticity_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    knowledge_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(knowledge_plasticity_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_plasticity_create: failed to allocate bridge");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = knowledge_plasticity_config_default();
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "knowledge_plasticity") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_plasticity_create: bridge_base_init failed");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Allocate synapse storage */
    bridge->max_synapses = bridge->config.max_synapses;
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(synapse_entry_t));
    if (!bridge->synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_plasticity_create: failed to allocate synapses");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize consolidation state */
    bridge->consolidation_state.semantic_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->consolidation_state.retrieval_calibration = 0.5f;
    bridge->consolidation_state.association_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->consolidation_state.categorical_strength = 0.5f;
    bridge->consolidation_state.hierarchical_strength = 0.5f;
    bridge->consolidation_state.learning_rate_mod = 1.0f;
    bridge->consolidation_state.last_learning_us = 0;

    bridge->state = KNOWLEDGE_PLASTICITY_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->synapse_count = 0;

    bridge->current_reward = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;
    bridge->bcm_global_threshold = bridge->config.bcm_target_rate;

    return bridge;
}

void knowledge_plasticity_destroy(knowledge_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "knowledge_plasticity");

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge->synapses);
    nimcp_free(bridge);
    bridge = NULL;
}

int knowledge_plasticity_reset(knowledge_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all synapses to initial weights */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            knowledge_plasticity_bridge_heartbeat("knowledge_pl_loop",
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

    /* Reset consolidation state */
    bridge->consolidation_state.semantic_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->consolidation_state.retrieval_calibration = 0.5f;
    bridge->consolidation_state.association_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->consolidation_state.learning_rate_mod = 1.0f;

    bridge->state = KNOWLEDGE_PLASTICITY_STATE_IDLE;
    bridge->current_reward = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int knowledge_plasticity_register_synapse(
    knowledge_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    knowledge_synapse_type_t type,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_register_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check for duplicate */
    if (find_synapse(bridge, synapse_id)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Find free slot */
    synapse_entry_t* slot = find_free_slot(bridge);
    if (!slot) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_register_synapse: slot is NULL");
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

    /* Auto-protect retrieval and confidence synapses */
    slot->synapse.is_protected = is_protected_type(type) &&
        (bridge->config.protect_retrieval_pathway || bridge->config.protect_confidence_pathway);

    bridge->synapse_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_plasticity_unregister_synapse(
    knowledge_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_unregister_synapse: entry is NULL");
        return -1;
    }

    entry->in_use = false;
    bridge->synapse_count--;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_plasticity_get_synapse(
    knowledge_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    knowledge_plasticity_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_get_synapse: entry is NULL");
        return -1;
    }

    *synapse = entry->synapse;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_plasticity_protect_synapse(
    knowledge_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_protect_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_protect_synapse: entry is NULL");
        return -1;
    }

    entry->synapse.is_protected = protect;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Learning Functions
//=============================================================================

int knowledge_plasticity_learn(
    knowledge_plasticity_bridge_t* bridge,
    knowledge_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_learn: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = KNOWLEDGE_PLASTICITY_STATE_LEARNING;

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        bridge->state = KNOWLEDGE_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_learn: entry is NULL");
        return -1;
    }

    /* Check protection */
    if (entry->synapse.is_protected) {
        bridge->stats.protected_updates_blocked++;
        bridge->state = KNOWLEDGE_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0; /* Not an error, just blocked */
    }

    float lr = bridge->learning_rate_effective * magnitude;
    float weight_change = 0.0f;

    switch (event) {
        case KNOWLEDGE_LEARN_RETRIEVAL_SUCCESS:
            weight_change = lr * bridge->config.retrieval_success_boost;
            bridge->stats.retrieval_success_events++;
            break;

        case KNOWLEDGE_LEARN_RETRIEVAL_FAILURE:
            weight_change = -lr * 0.5f;
            bridge->stats.retrieval_failure_events++;
            break;

        case KNOWLEDGE_LEARN_ASSOCIATION_FORMED:
            weight_change = lr * bridge->config.association_learning_boost;
            bridge->stats.association_formed_events++;
            break;

        case KNOWLEDGE_LEARN_ASSOCIATION_WEAK:
            weight_change = -lr * 0.3f;
            break;

        case KNOWLEDGE_LEARN_ENCODING_STRONG:
            weight_change = lr * bridge->config.semantic_modulation;
            break;

        case KNOWLEDGE_LEARN_ENCODING_WEAK:
            weight_change = -lr * 0.2f;
            break;

        case KNOWLEDGE_LEARN_CATEGORY_MATCHED:
            weight_change = lr * 0.8f;
            break;

        case KNOWLEDGE_LEARN_CONSOLIDATION:
            weight_change = lr * 1.0f;
            bridge->stats.consolidation_events++;
            break;

        case KNOWLEDGE_LEARN_INTERFERENCE:
            weight_change = -lr * 0.3f;
            break;

        case KNOWLEDGE_LEARN_REINFORCEMENT:
            weight_change = lr * 0.5f;
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

    bridge->state = KNOWLEDGE_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float knowledge_plasticity_apply_stdp(
    knowledge_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
) {
    if (!bridge) return NAN;

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


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

int knowledge_plasticity_apply_reward(
    knowledge_plasticity_bridge_t* bridge,
    float reward
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_apply_reward: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    reward = nimcp_clampf(reward, -1.0f, 1.0f);
    bridge->current_reward = reward;

    /* Apply reward modulation to all eligible synapses */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            knowledge_plasticity_bridge_heartbeat("knowledge_pl_loop",
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

int knowledge_plasticity_update_bcm(
    knowledge_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_update_bcm: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_plasticity_update_bcm: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = KNOWLEDGE_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.bcm_tau_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            knowledge_plasticity_bridge_heartbeat("knowledge_pl_loop",
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

    bridge->state = KNOWLEDGE_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_plasticity_homeostatic_update(
    knowledge_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_homeostatic_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = KNOWLEDGE_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.homeostatic_tau_ms);

    /* Calculate mean activation */
    float mean_activation = 0.0f;
    uint32_t activation_count = 0;
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            knowledge_plasticity_bridge_heartbeat("knowledge_pl_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.type == KNOWLEDGE_SYNAPSE_SEMANTIC) {
            mean_activation += bridge->synapses[i].synapse.weight;
            activation_count++;
        }
    }
    if (activation_count > 0) {
        mean_activation /= activation_count;
    }

    /* Scale non-protected synapses toward target */
    float target = bridge->config.target_activation;
    float scale_factor = 1.0f;
    if (mean_activation > 0.0f) {
        scale_factor = target / mean_activation;
        scale_factor = nimcp_clampf(scale_factor, 0.9f, 1.1f);
    }

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            knowledge_plasticity_bridge_heartbeat("knowledge_pl_loop",
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

    bridge->state = KNOWLEDGE_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_plasticity_update_traces(
    knowledge_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_update_traces: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float decay = expf(-dt_ms / bridge->config.stdp_tau_plus_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            knowledge_plasticity_bridge_heartbeat("knowledge_pl_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace *= decay;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_plasticity_consolidate(knowledge_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = KNOWLEDGE_PLASTICITY_STATE_CONSOLIDATING;

    /* Clear eligibility traces */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            knowledge_plasticity_bridge_heartbeat("knowledge_pl_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace = 0.0f;
        }
    }

    /* Update consolidation state based on synapse weights */
    float semantic_sum = 0.0f, semantic_count = 0;
    float association_sum = 0.0f, association_count = 0;
    float categorical_sum = 0.0f, categorical_count = 0;
    float hierarchical_sum = 0.0f, hierarchical_count = 0;

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            knowledge_plasticity_bridge_heartbeat("knowledge_pl_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            switch (bridge->synapses[i].synapse.type) {
                case KNOWLEDGE_SYNAPSE_SEMANTIC:
                    semantic_sum += bridge->synapses[i].synapse.weight;
                    semantic_count++;
                    break;
                case KNOWLEDGE_SYNAPSE_ASSOCIATION:
                    association_sum += bridge->synapses[i].synapse.weight;
                    association_count++;
                    break;
                case KNOWLEDGE_SYNAPSE_CATEGORICAL:
                    categorical_sum += bridge->synapses[i].synapse.weight;
                    categorical_count++;
                    break;
                case KNOWLEDGE_SYNAPSE_HIERARCHICAL:
                    hierarchical_sum += bridge->synapses[i].synapse.weight;
                    hierarchical_count++;
                    break;
                default:
                    break;
            }
        }
    }

    if (semantic_count > 0) {
        bridge->consolidation_state.semantic_sensitivity = semantic_sum / semantic_count * 2.0f;
    }
    if (association_count > 0) {
        bridge->consolidation_state.association_sensitivity = association_sum / association_count * 2.0f;
    }
    if (categorical_count > 0) {
        bridge->consolidation_state.categorical_strength = categorical_sum / categorical_count;
    }
    if (hierarchical_count > 0) {
        bridge->consolidation_state.hierarchical_strength = hierarchical_sum / hierarchical_count;
    }

    bridge->consolidation_state.last_learning_us = bridge->current_time_us;
    bridge->state = KNOWLEDGE_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// State Query Functions
//=============================================================================

int knowledge_plasticity_get_consolidation_state(
    knowledge_plasticity_bridge_t* bridge,
    knowledge_consolidation_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_get_consolidation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->consolidation_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int knowledge_plasticity_get_state(
    knowledge_plasticity_bridge_t* bridge,
    knowledge_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->active_synapses = bridge->synapse_count;
    state->learning_rate_effective = bridge->learning_rate_effective;

    /* Calculate mean weight and variance using statistics module */
    if (bridge->synapse_count > 0) {
        /* Collect active synapse weights to temporary array */
        float* weights = (float*)nimcp_calloc(bridge->synapse_count, sizeof(float));
        if (weights) {
            uint32_t weight_idx = 0;
            for (uint32_t i = 0; i < bridge->max_synapses && weight_idx < bridge->synapse_count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
                    knowledge_plasticity_bridge_heartbeat("knowledge_pl_loop",
                                     (float)(i + 1) / (float)bridge->max_synapses);
                }

                if (bridge->synapses[i].in_use) {
                    weights[weight_idx++] = bridge->synapses[i].synapse.weight;
                }
            }
            state->mean_weight = nimcp_stats_mean(weights, bridge->synapse_count);
            state->weight_variance = nimcp_stats_variance_population(weights, bridge->synapse_count);
            nimcp_free(weights);
            weights = NULL;
        } else {
            state->mean_weight = 0.0f;
            state->weight_variance = 0.0f;
        }
    } else {
        state->mean_weight = 0.0f;
        state->weight_variance = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_plasticity_get_stats(
    knowledge_plasticity_bridge_t* bridge,
    knowledge_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int knowledge_plasticity_reset_stats(knowledge_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(knowledge_plasticity_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int knowledge_plasticity_register_learn_callback(
    knowledge_plasticity_bridge_t* bridge,
    knowledge_plasticity_learn_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_register_learn_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->learn_callback = callback;
    bridge->learn_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int knowledge_plasticity_register_consolidation_callback(
    knowledge_plasticity_bridge_t* bridge,
    knowledge_plasticity_consolidation_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_register_consolidation_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->consolidation_callback = callback;
    bridge->consolidation_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int knowledge_plasticity_bio_async_connect(knowledge_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int knowledge_plasticity_bio_async_disconnect(knowledge_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_plasticity_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool knowledge_plasticity_is_bio_async_connected(knowledge_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_plasticity_bridge_heartbeat("knowledge_pl_knowledge_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void knowledge_plasticity_bridge_set_instance_health_agent(knowledge_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "knowledge_plasticity_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int knowledge_plasticity_bridge_training_begin(knowledge_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    knowledge_plasticity_bridge_heartbeat_instance(bridge->health_agent, "knowledge_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int knowledge_plasticity_bridge_training_end(knowledge_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    knowledge_plasticity_bridge_heartbeat_instance(bridge->health_agent, "knowledge_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int knowledge_plasticity_bridge_training_step(knowledge_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    knowledge_plasticity_bridge_heartbeat_instance(bridge->health_agent, "knowledge_plasticity_bridge_training_step", progress);
    return 0;
}
