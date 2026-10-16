/**
 * @file nimcp_parietal_plasticity_bridge.c
 * @brief Parietal - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/parietal/nimcp_parietal_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

#include "glial/myelin_sheath/nimcp_myelin_math.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(parietal_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from parietal_plasticity_bridge module (instance-level) */
static inline void parietal_plasticity_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_parietal_plasticity_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_parietal_plasticity_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_parietal_plasticity_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PARIETAL_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

typedef struct synapse_entry {
    parietal_plasticity_synapse_t synapse;
    bool in_use;
} synapse_entry_t;

struct parietal_plasticity_bridge {
    bridge_base_t base;
    parietal_plasticity_config_t config;

    /* State */
    parietal_plasticity_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Synapse storage */
    synapse_entry_t* synapses;
    uint32_t synapse_count;
    uint32_t max_synapses;

    /* Spatial learning state */
    parietal_spatial_learning_state_t spatial_state;

    /* Global learning state */
    float current_reward;
    float learning_rate_effective;
    float bcm_global_threshold;

    /* Callbacks */
    parietal_plasticity_learn_callback_t learn_callback;
    void* learn_callback_data;
    parietal_plasticity_spatial_callback_t spatial_callback;
    void* spatial_callback_data;

    /* Statistics */
    parietal_plasticity_stats_t stats;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(parietal_plasticity_bridge)

//=============================================================================
// Helper Functions
//=============================================================================

static synapse_entry_t* find_synapse(parietal_plasticity_bridge_t* bridge, uint32_t synapse_id) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            parietal_plasticity_bridge_heartbeat("parietal_pla_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static synapse_entry_t* find_free_slot(parietal_plasticity_bridge_t* bridge) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            parietal_plasticity_bridge_heartbeat("parietal_pla_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (!bridge->synapses[i].in_use) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}

static bool is_protected_type(parietal_synapse_type_t type) {
    return type == PARIETAL_SYNAPSE_SPATIAL_ATTENTION ||
           type == PARIETAL_SYNAPSE_BODY_SCHEMA;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

parietal_plasticity_config_t parietal_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    parietal_plasticity_config_t config = {
        .base_learning_rate = PARIETAL_PLASTICITY_DEFAULT_LR,
        .stdp_tau_plus_ms = 20.0f,
        .stdp_tau_minus_ms = 20.0f,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,

        .bcm_tau_ms = 1000.0f,
        .bcm_target_rate = 5.0f,

        .homeostatic_tau_ms = 10000.0f,
        .target_spatial_activity = 0.5f,

        .spatial_learning_boost = 1.5f,
        .numerical_learning_boost = 1.3f,
        .attention_modulation = 1.2f,

        .weight_min = 0.0f,
        .weight_max = 1.0f,

        .protect_spatial_attention = true,
        .protect_body_schema = true,
        .protection_strength = 1.0f,

        .max_synapses = PARIETAL_PLASTICITY_MAX_SYNAPSES,

        .enable_bio_async = false
    };
    return config;
}

parietal_plasticity_bridge_t* parietal_plasticity_create(
    const parietal_plasticity_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    parietal_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(parietal_plasticity_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = parietal_plasticity_config_default();
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "parietal_plasticity") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "parietal_plasticity_create: validation failed");
        return NULL;
    }

    /* Allocate synapse storage */
    bridge->max_synapses = bridge->config.max_synapses;
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(synapse_entry_t));
    if (!bridge->synapses) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_plasticity_create: bridge->synapses is NULL");
        return NULL;
    }

    /* Initialize spatial learning state */
    bridge->spatial_state.attention_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->spatial_state.spatial_calibration = 0.5f;
    bridge->spatial_state.numerical_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->spatial_state.integration_strength = 0.5f;
    bridge->spatial_state.rotation_strength = 0.5f;
    bridge->spatial_state.learning_rate_mod = 1.0f;
    bridge->spatial_state.last_learning_us = 0;

    bridge->state = PARIETAL_PLASTICITY_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->synapse_count = 0;

    bridge->current_reward = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;
    bridge->bcm_global_threshold = bridge->config.bcm_target_rate;

    return bridge;
}

void parietal_plasticity_destroy(parietal_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "parietal_plasticity");

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->synapses);
    nimcp_free(bridge);
    bridge = NULL;
}

int parietal_plasticity_reset(parietal_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all synapses to initial weights */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            parietal_plasticity_bridge_heartbeat("parietal_pla_loop",
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

    /* Reset spatial learning state */
    bridge->spatial_state.attention_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->spatial_state.spatial_calibration = 0.5f;
    bridge->spatial_state.numerical_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->spatial_state.learning_rate_mod = 1.0f;

    bridge->state = PARIETAL_PLASTICITY_STATE_IDLE;
    bridge->current_reward = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int parietal_plasticity_register_synapse(
    parietal_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    parietal_synapse_type_t type,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_register_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check for duplicate */
    if (find_synapse(bridge, synapse_id)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Find free slot */
    synapse_entry_t* slot = find_free_slot(bridge);
    if (!slot) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_register_synapse: slot is NULL");
        return -1;
    }

    /* Initialize synapse */
    slot->in_use = true;
    slot->synapse.synapse_id = synapse_id;
    slot->synapse.type = type;
    slot->synapse.weight = nimcp_myelin_clamp(initial_weight, bridge->config.weight_min, bridge->config.weight_max);
    slot->synapse.initial_weight = slot->synapse.weight;
    slot->synapse.eligibility_trace = 0.0f;
    slot->synapse.bcm_threshold = bridge->config.bcm_target_rate;
    slot->synapse.avg_activity = 0.0f;
    slot->synapse.last_update_us = bridge->current_time_us;
    slot->synapse.update_count = 0;

    /* Auto-protect spatial attention and body schema synapses */
    slot->synapse.is_protected = is_protected_type(type) &&
        (bridge->config.protect_spatial_attention || bridge->config.protect_body_schema);

    bridge->synapse_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int parietal_plasticity_unregister_synapse(
    parietal_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  /* Synapse not found — normal condition */
    }

    entry->in_use = false;
    bridge->synapse_count--;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int parietal_plasticity_get_synapse(
    parietal_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    parietal_plasticity_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, synapse, sizeof(*synapse));

    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  /* Synapse not found — normal condition */
    }

    *synapse = entry->synapse;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int parietal_plasticity_protect_synapse(
    parietal_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_protect_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  /* Synapse not found — normal condition */
    }

    entry->synapse.is_protected = protect;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Learning Functions
//=============================================================================

int parietal_plasticity_learn(
    parietal_plasticity_bridge_t* bridge,
    parietal_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_learn: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = PARIETAL_PLASTICITY_STATE_LEARNING;

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        bridge->state = PARIETAL_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  /* Synapse not found — normal condition */
    }

    /* Check protection */
    if (entry->synapse.is_protected) {
        bridge->stats.protected_updates_blocked++;
        bridge->state = PARIETAL_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0; /* Not an error, just blocked */
    }

    float lr = bridge->learning_rate_effective * magnitude;
    float weight_change = 0.0f;

    switch (event) {
        case PARIETAL_LEARN_ATTENTION_SHIFT:
            weight_change = lr * bridge->config.attention_modulation;
            bridge->stats.attention_shift_events++;
            break;

        case PARIETAL_LEARN_ATTENTION_ERROR:
            weight_change = -lr * 0.5f;
            bridge->stats.attention_error_events++;
            break;

        case PARIETAL_LEARN_MAGNITUDE_ACCURATE:
            weight_change = lr * bridge->config.numerical_learning_boost;
            bridge->stats.magnitude_accurate_events++;
            break;

        case PARIETAL_LEARN_MAGNITUDE_ERROR:
            weight_change = -lr * 0.3f;
            break;

        case PARIETAL_LEARN_SPATIAL_SUCCESS:
            weight_change = lr * bridge->config.spatial_learning_boost;
            bridge->stats.spatial_success_events++;
            break;

        case PARIETAL_LEARN_SPATIAL_FAILURE:
            weight_change = -lr * 0.2f;
            break;

        case PARIETAL_LEARN_INTEGRATION_MATCHED:
            weight_change = lr * 0.8f;
            break;

        case PARIETAL_LEARN_ROTATION_SUCCESS:
            weight_change = lr * 1.0f;
            break;

        case PARIETAL_LEARN_ROTATION_FAILURE:
            weight_change = -lr * 0.3f;
            break;

        case PARIETAL_LEARN_TRANSFORM_COMPLETE:
            weight_change = lr * 0.5f;
            break;
    }

    /* Context modulation */
    weight_change *= (0.5f + 0.5f * context);

    /* Apply weight change */
    float old_weight = entry->synapse.weight;
    entry->synapse.weight = nimcp_myelin_clamp(
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

    /* Invoke callback outside mutex to prevent deadlock */
    parietal_plasticity_learn_callback_t cb = bridge->learn_callback;
    void* cb_data = bridge->learn_callback_data;

    bridge->state = PARIETAL_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);

    if (cb) {
        cb(bridge, event, magnitude, cb_data);
    }

    return 0;
}

float parietal_plasticity_apply_stdp(
    parietal_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
) {
    if (!bridge) return NAN;

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


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
    entry->synapse.weight = nimcp_myelin_clamp(
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

int parietal_plasticity_apply_reward(
    parietal_plasticity_bridge_t* bridge,
    float reward
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_apply_reward: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    reward = nimcp_myelin_clamp(reward, -1.0f, 1.0f);
    bridge->current_reward = reward;

    /* Apply reward modulation to all eligible synapses */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            parietal_plasticity_bridge_heartbeat("parietal_pla_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use && !bridge->synapses[i].synapse.is_protected) {
            float trace = bridge->synapses[i].synapse.eligibility_trace;
            if (fabsf(trace) > 0.001f) {
                float delta = bridge->config.base_learning_rate * reward * trace;
                bridge->synapses[i].synapse.weight = nimcp_myelin_clamp(
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

int parietal_plasticity_update_bcm(
    parietal_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_update_bcm: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_plasticity_update_bcm: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = PARIETAL_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.bcm_tau_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            parietal_plasticity_bridge_heartbeat("parietal_pla_loop",
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

    bridge->state = PARIETAL_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int parietal_plasticity_homeostatic_update(
    parietal_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_homeostatic_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "parietal_plasticity_homeostatic_update");
    BRIDGE_LGSS_GATE(bridge, "parietal_plasticity_homeostatic_update");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = PARIETAL_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.homeostatic_tau_ms);

    /* Calculate mean spatial activity */
    float mean_spatial = 0.0f;
    uint32_t spatial_count = 0;
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            parietal_plasticity_bridge_heartbeat("parietal_pla_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            (bridge->synapses[i].synapse.type == PARIETAL_SYNAPSE_SPATIAL_ATTENTION ||
             bridge->synapses[i].synapse.type == PARIETAL_SYNAPSE_VISUOSPATIAL)) {
            mean_spatial += bridge->synapses[i].synapse.weight;
            spatial_count++;
        }
    }
    if (spatial_count > 0) {
        mean_spatial /= spatial_count;
    }

    /* Scale non-protected synapses toward target */
    float target = bridge->config.target_spatial_activity;
    float scale_factor = 1.0f;
    if (mean_spatial > 0.0f) {
        scale_factor = target / mean_spatial;
        scale_factor = nimcp_myelin_clamp(scale_factor, 0.9f, 1.1f);
    }

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            parietal_plasticity_bridge_heartbeat("parietal_pla_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use && !bridge->synapses[i].synapse.is_protected) {
            float scaled = bridge->synapses[i].synapse.weight * (1.0f + (scale_factor - 1.0f) * (1.0f - decay));
            bridge->synapses[i].synapse.weight = nimcp_myelin_clamp(
                scaled,
                bridge->config.weight_min,
                bridge->config.weight_max
            );
        }
    }

    bridge->state = PARIETAL_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int parietal_plasticity_update_traces(
    parietal_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_update_traces: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float decay = expf(-dt_ms / bridge->config.stdp_tau_plus_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            parietal_plasticity_bridge_heartbeat("parietal_pla_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace *= decay;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int parietal_plasticity_consolidate(parietal_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = PARIETAL_PLASTICITY_STATE_CONSOLIDATING;

    /* Clear eligibility traces */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            parietal_plasticity_bridge_heartbeat("parietal_pla_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace = 0.0f;
        }
    }

    /* Update spatial learning state based on synapse weights */
    float attention_sum = 0.0f, attention_count = 0;
    float numerical_sum = 0.0f, numerical_count = 0;
    float integration_sum = 0.0f, integration_count = 0;
    float visuospatial_sum = 0.0f, visuospatial_count = 0;

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            parietal_plasticity_bridge_heartbeat("parietal_pla_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            switch (bridge->synapses[i].synapse.type) {
                case PARIETAL_SYNAPSE_SPATIAL_ATTENTION:
                    attention_sum += bridge->synapses[i].synapse.weight;
                    attention_count++;
                    break;
                case PARIETAL_SYNAPSE_NUMERICAL:
                    numerical_sum += bridge->synapses[i].synapse.weight;
                    numerical_count++;
                    break;
                case PARIETAL_SYNAPSE_MULTISENSORY:
                    integration_sum += bridge->synapses[i].synapse.weight;
                    integration_count++;
                    break;
                case PARIETAL_SYNAPSE_VISUOSPATIAL:
                    visuospatial_sum += bridge->synapses[i].synapse.weight;
                    visuospatial_count++;
                    break;
                default:
                    break;
            }
        }
    }

    if (attention_count > 0) {
        bridge->spatial_state.attention_sensitivity = attention_sum / attention_count * 2.0f;
    }
    if (numerical_count > 0) {
        bridge->spatial_state.numerical_sensitivity = numerical_sum / numerical_count * 2.0f;
    }
    if (integration_count > 0) {
        bridge->spatial_state.integration_strength = integration_sum / integration_count;
    }
    if (visuospatial_count > 0) {
        bridge->spatial_state.rotation_strength = visuospatial_sum / visuospatial_count;
    }

    bridge->spatial_state.last_learning_us = bridge->current_time_us;
    bridge->state = PARIETAL_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// State Query Functions
//=============================================================================

int parietal_plasticity_get_spatial_state(
    parietal_plasticity_bridge_t* bridge,
    parietal_spatial_learning_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_get_spatial_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->spatial_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int parietal_plasticity_get_state(
    parietal_plasticity_bridge_t* bridge,
    parietal_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

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
            parietal_plasticity_bridge_heartbeat("parietal_pla_loop",
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

int parietal_plasticity_get_stats(
    parietal_plasticity_bridge_t* bridge,
    parietal_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, stats, sizeof(*stats));

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int parietal_plasticity_reset_stats(parietal_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(parietal_plasticity_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int parietal_plasticity_register_learn_callback(
    parietal_plasticity_bridge_t* bridge,
    parietal_plasticity_learn_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_register_learn_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->learn_callback = callback;
    bridge->learn_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int parietal_plasticity_register_spatial_callback(
    parietal_plasticity_bridge_t* bridge,
    parietal_plasticity_spatial_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_register_spatial_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->spatial_callback = callback;
    bridge->spatial_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int parietal_plasticity_bio_async_connect(parietal_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int parietal_plasticity_bio_async_disconnect(parietal_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_plasticity_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool parietal_plasticity_is_bio_async_connected(parietal_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_plasticity_bridge_heartbeat("parietal_pla_parietal_plasticity_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

//=============================================================================
// Instance Health Agent Setter (B23 Upgrade)
//=============================================================================

void parietal_plasticity_bridge_set_instance_health_agent(
    parietal_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B23 Upgrade)
//=============================================================================

int parietal_plasticity_bridge_training_begin(parietal_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "parietal_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    parietal_plasticity_bridge_heartbeat_instance(bridge->health_agent, "parietal_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int parietal_plasticity_bridge_training_end(parietal_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "parietal_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    parietal_plasticity_bridge_heartbeat_instance(bridge->health_agent, "parietal_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int parietal_plasticity_bridge_training_step(parietal_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "parietal_plasticity_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "parietal_plasticity_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "parietal_plasticity_bridge_training_step");
    parietal_plasticity_bridge_heartbeat_instance(bridge->health_agent, "parietal_plasticity_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}
