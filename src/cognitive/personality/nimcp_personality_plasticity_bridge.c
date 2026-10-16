/**
 * @file nimcp_personality_plasticity_bridge.c
 * @brief Personality - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/personality/nimcp_personality_plasticity_bridge.h"
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
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(personality_plasticity_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_personality_plasticity_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_personality_plasticity_bridge_mesh_registry = NULL;

nimcp_error_t personality_plasticity_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_personality_plasticity_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "personality_plasticity_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "personality_plasticity_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_personality_plasticity_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_personality_plasticity_bridge_mesh_registry = registry;
    return err;
}

void personality_plasticity_bridge_mesh_unregister(void) {
    if (g_personality_plasticity_bridge_mesh_registry && g_personality_plasticity_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_personality_plasticity_bridge_mesh_registry, g_personality_plasticity_bridge_mesh_id);
        g_personality_plasticity_bridge_mesh_id = 0;
        g_personality_plasticity_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from personality_plasticity_bridge module (instance-level) */
static inline void personality_plasticity_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_personality_plasticity_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_personality_plasticity_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_personality_plasticity_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PERSONALITY_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

typedef struct synapse_entry {
    personality_plasticity_synapse_t synapse;
    bool in_use;
} synapse_entry_t;

struct personality_plasticity_bridge {
    bridge_base_t base;
    personality_plasticity_config_t config;

    /* State */
    personality_plasticity_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Synapse storage */
    synapse_entry_t* synapses;
    uint32_t synapse_count;
    uint32_t max_synapses;

    /* Trait adaptation state */
    personality_adaptation_state_t adaptation_state;

    /* Global learning state */
    float current_reward;
    float learning_rate_effective;
    float bcm_global_threshold;

    /* Callbacks */
    personality_plasticity_learn_callback_t learn_callback;
    void* learn_callback_data;
    personality_plasticity_adaptation_callback_t adaptation_callback;
    void* adaptation_callback_data;

    /* Statistics */
    personality_plasticity_stats_t stats;

    /* Phase 8: Instance health agent (B24 upgrade) */
    nimcp_health_agent_t* health_agent;
};

//=============================================================================
// Helper Functions
//=============================================================================

static synapse_entry_t* find_synapse(personality_plasticity_bridge_t* bridge, uint32_t synapse_id) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            personality_plasticity_bridge_heartbeat("personality__loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static synapse_entry_t* find_free_slot(personality_plasticity_bridge_t* bridge) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            personality_plasticity_bridge_heartbeat("personality__loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (!bridge->synapses[i].in_use) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}

static bool is_protected_type(personality_synapse_type_t type) {
    return type == PERSONALITY_SYNAPSE_CONSCIENTIOUSNESS ||
           type == PERSONALITY_SYNAPSE_STABILITY;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

personality_plasticity_config_t personality_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    personality_plasticity_config_t config = {
        .base_learning_rate = PERSONALITY_PLASTICITY_DEFAULT_LR,
        .stdp_tau_plus_ms = 20.0f,
        .stdp_tau_minus_ms = 20.0f,
        .stdp_a_plus = 0.005f,
        .stdp_a_minus = 0.006f,

        .bcm_tau_ms = 2000.0f,
        .bcm_target_rate = 5.0f,

        .homeostatic_tau_ms = 20000.0f,
        .target_trait_level = 0.5f,

        .social_learning_boost = 1.3f,
        .emotional_learning_boost = 1.2f,
        .trait_modulation = 1.0f,

        .weight_min = 0.0f,
        .weight_max = 1.0f,

        .protect_conscientiousness = true,
        .protect_stability = true,
        .protection_strength = 1.0f,

        .max_synapses = PERSONALITY_PLASTICITY_MAX_SYNAPSES,

        .enable_bio_async = false
    };
    return config;
}

personality_plasticity_bridge_t* personality_plasticity_create(
    const personality_plasticity_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    personality_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(personality_plasticity_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = personality_plasticity_config_default();
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "personality_plasticity") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "personality_plasticity_create: validation failed");
        return NULL;
    }

    /* Allocate synapse storage */
    bridge->max_synapses = bridge->config.max_synapses;
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(synapse_entry_t));
    if (!bridge->synapses) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "personality_plasticity_create: bridge->synapses is NULL");
        return NULL;
    }

    /* Initialize trait adaptation state */
    bridge->adaptation_state.openness_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->adaptation_state.conscientiousness_calibration = 0.5f;
    bridge->adaptation_state.extraversion_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->adaptation_state.agreeableness_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->adaptation_state.neuroticism_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->adaptation_state.learning_rate_mod = 1.0f;
    bridge->adaptation_state.last_learning_us = 0;

    bridge->state = PERSONALITY_PLASTICITY_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->synapse_count = 0;

    bridge->current_reward = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;
    bridge->bcm_global_threshold = bridge->config.bcm_target_rate;

    return bridge;
}

void personality_plasticity_destroy(personality_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "personality_plasticity");

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->synapses);
    nimcp_free(bridge);
    bridge = NULL;
}

int personality_plasticity_reset(personality_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all synapses to initial weights */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            personality_plasticity_bridge_heartbeat("personality__loop",
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

    /* Reset trait adaptation state */
    bridge->adaptation_state.openness_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->adaptation_state.conscientiousness_calibration = 0.5f;
    bridge->adaptation_state.extraversion_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->adaptation_state.agreeableness_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->adaptation_state.neuroticism_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->adaptation_state.learning_rate_mod = 1.0f;

    bridge->state = PERSONALITY_PLASTICITY_STATE_IDLE;
    bridge->current_reward = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int personality_plasticity_register_synapse(
    personality_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    personality_synapse_type_t type,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_register_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check for duplicate */
    if (find_synapse(bridge, synapse_id)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "personality_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Find free slot */
    synapse_entry_t* slot = find_free_slot(bridge);
    if (!slot) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_LOGGING_WARN("personality_plasticity_register_synapse: synapse capacity full");
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

    /* Auto-protect conscientiousness and stability synapses */
    slot->synapse.is_protected = is_protected_type(type) &&
        (bridge->config.protect_conscientiousness || bridge->config.protect_stability);

    bridge->synapse_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int personality_plasticity_unregister_synapse(
    personality_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


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

int personality_plasticity_get_synapse(
    personality_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    personality_plasticity_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


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

int personality_plasticity_protect_synapse(
    personality_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_protect_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


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

int personality_plasticity_learn(
    personality_plasticity_bridge_t* bridge,
    personality_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_learn: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = PERSONALITY_PLASTICITY_STATE_LEARNING;

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        bridge->state = PERSONALITY_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  /* Synapse not found — normal condition */
    }

    /* Check protection */
    if (entry->synapse.is_protected) {
        bridge->stats.protected_updates_blocked++;
        bridge->state = PERSONALITY_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0; /* Not an error, just blocked */
    }

    float lr = bridge->learning_rate_effective * magnitude;
    float weight_change = 0.0f;

    switch (event) {
        case PERSONALITY_LEARN_TRAIT_CONFIRMED:
            weight_change = lr * bridge->config.trait_modulation;
            bridge->stats.trait_confirmed_events++;
            break;

        case PERSONALITY_LEARN_TRAIT_MISMATCH:
            weight_change = -lr * 0.3f;
            bridge->stats.trait_mismatch_events++;
            break;

        case PERSONALITY_LEARN_SOCIAL_SUCCESS:
            weight_change = lr * bridge->config.social_learning_boost;
            bridge->stats.social_success_events++;
            break;

        case PERSONALITY_LEARN_SOCIAL_FAILURE:
            weight_change = -lr * 0.2f;
            break;

        case PERSONALITY_LEARN_APPROACH_REWARDED:
            weight_change = lr * 1.0f;
            break;

        case PERSONALITY_LEARN_AVOIDANCE_REWARDED:
            weight_change = lr * 0.8f;
            break;

        case PERSONALITY_LEARN_EMOTIONAL_REGULATION:
            weight_change = lr * bridge->config.emotional_learning_boost;
            bridge->stats.emotional_regulation_events++;
            break;

        case PERSONALITY_LEARN_STRESS_ADAPTATION:
            weight_change = lr * 0.5f;
            break;

        case PERSONALITY_LEARN_OPENNESS_REWARDED:
            weight_change = lr * 0.7f;
            break;

        case PERSONALITY_LEARN_STABILITY_EVENT:
            weight_change = lr * 0.3f;
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
    personality_plasticity_learn_callback_t cb = bridge->learn_callback;
    void* cb_data = bridge->learn_callback_data;

    bridge->state = PERSONALITY_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);

    if (cb) {
        cb(bridge, event, magnitude, cb_data);
    }

    return 0;
}

float personality_plasticity_apply_stdp(
    personality_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
) {
    if (!bridge) return NAN;

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


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

int personality_plasticity_apply_reward(
    personality_plasticity_bridge_t* bridge,
    float reward
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_apply_reward: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    reward = nimcp_myelin_clamp(reward, -1.0f, 1.0f);
    bridge->current_reward = reward;

    /* Apply reward modulation to all eligible synapses */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            personality_plasticity_bridge_heartbeat("personality__loop",
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

int personality_plasticity_update_bcm(
    personality_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_update_bcm: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "personality_plasticity_update_bcm: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = PERSONALITY_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.bcm_tau_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            personality_plasticity_bridge_heartbeat("personality__loop",
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

    bridge->state = PERSONALITY_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int personality_plasticity_homeostatic_update(
    personality_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_homeostatic_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = PERSONALITY_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.homeostatic_tau_ms);

    /* Calculate mean trait level */
    float mean_trait = 0.0f;
    uint32_t trait_count = 0;
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            personality_plasticity_bridge_heartbeat("personality__loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.type <= PERSONALITY_SYNAPSE_NEUROTICISM) {
            mean_trait += bridge->synapses[i].synapse.weight;
            trait_count++;
        }
    }
    if (trait_count > 0) {
        mean_trait /= trait_count;
    }

    /* Scale non-protected synapses toward target */
    float target = bridge->config.target_trait_level;
    float scale_factor = 1.0f;
    if (mean_trait > 0.0f) {
        scale_factor = target / mean_trait;
        scale_factor = nimcp_myelin_clamp(scale_factor, 0.95f, 1.05f);
    }

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            personality_plasticity_bridge_heartbeat("personality__loop",
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

    bridge->state = PERSONALITY_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int personality_plasticity_update_traces(
    personality_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_update_traces: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float decay = expf(-dt_ms / bridge->config.stdp_tau_plus_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            personality_plasticity_bridge_heartbeat("personality__loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace *= decay;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int personality_plasticity_consolidate(personality_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = PERSONALITY_PLASTICITY_STATE_CONSOLIDATING;

    /* Clear eligibility traces */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            personality_plasticity_bridge_heartbeat("personality__loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace = 0.0f;
        }
    }

    /* Update trait adaptation state based on synapse weights */
    float openness_sum = 0.0f, openness_count = 0;
    float conscient_sum = 0.0f, conscient_count = 0;
    float extra_sum = 0.0f, extra_count = 0;
    float agree_sum = 0.0f, agree_count = 0;
    float neuro_sum = 0.0f, neuro_count = 0;

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            personality_plasticity_bridge_heartbeat("personality__loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            switch (bridge->synapses[i].synapse.type) {
                case PERSONALITY_SYNAPSE_OPENNESS:
                    openness_sum += bridge->synapses[i].synapse.weight;
                    openness_count++;
                    break;
                case PERSONALITY_SYNAPSE_CONSCIENTIOUSNESS:
                    conscient_sum += bridge->synapses[i].synapse.weight;
                    conscient_count++;
                    break;
                case PERSONALITY_SYNAPSE_EXTRAVERSION:
                    extra_sum += bridge->synapses[i].synapse.weight;
                    extra_count++;
                    break;
                case PERSONALITY_SYNAPSE_AGREEABLENESS:
                    agree_sum += bridge->synapses[i].synapse.weight;
                    agree_count++;
                    break;
                case PERSONALITY_SYNAPSE_NEUROTICISM:
                    neuro_sum += bridge->synapses[i].synapse.weight;
                    neuro_count++;
                    break;
                default:
                    break;
            }
        }
    }

    if (openness_count > 0) {
        bridge->adaptation_state.openness_sensitivity = openness_sum / openness_count * 2.0f;
    }
    if (conscient_count > 0) {
        bridge->adaptation_state.conscientiousness_calibration = conscient_sum / conscient_count;
    }
    if (extra_count > 0) {
        bridge->adaptation_state.extraversion_sensitivity = extra_sum / extra_count * 2.0f;
    }
    if (agree_count > 0) {
        bridge->adaptation_state.agreeableness_sensitivity = agree_sum / agree_count * 2.0f;
    }
    if (neuro_count > 0) {
        bridge->adaptation_state.neuroticism_sensitivity = neuro_sum / neuro_count * 2.0f;
    }

    bridge->adaptation_state.last_learning_us = bridge->current_time_us;
    bridge->state = PERSONALITY_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// State Query Functions
//=============================================================================

int personality_plasticity_get_adaptation_state(
    personality_plasticity_bridge_t* bridge,
    personality_adaptation_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_get_adaptation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->adaptation_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int personality_plasticity_get_state(
    personality_plasticity_bridge_t* bridge,
    personality_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


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
            personality_plasticity_bridge_heartbeat("personality__loop",
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

int personality_plasticity_get_stats(
    personality_plasticity_bridge_t* bridge,
    personality_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int personality_plasticity_reset_stats(personality_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(personality_plasticity_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int personality_plasticity_register_learn_callback(
    personality_plasticity_bridge_t* bridge,
    personality_plasticity_learn_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_register_learn_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->learn_callback = callback;
    bridge->learn_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int personality_plasticity_register_adaptation_callback(
    personality_plasticity_bridge_t* bridge,
    personality_plasticity_adaptation_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_register_adaptation_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->adaptation_callback = callback;
    bridge->adaptation_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int personality_plasticity_bio_async_connect(personality_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_LOGGING_WARN("personality_plasticity_bio_async_connect: bio_async not enabled in config");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int personality_plasticity_bio_async_disconnect(personality_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_plasticity_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool personality_plasticity_is_bio_async_connected(personality_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_plasticity_bridge_heartbeat("personality__personality_plastici", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

//=============================================================================
// Instance Health Agent Setter (B24 Upgrade)
//=============================================================================

void personality_plasticity_bridge_set_instance_health_agent(
    personality_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B24 Upgrade)
//=============================================================================

int personality_plasticity_bridge_training_begin(personality_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "personality_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    personality_plasticity_bridge_heartbeat_instance(bridge->health_agent, "personality_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int personality_plasticity_bridge_training_end(personality_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "personality_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    personality_plasticity_bridge_heartbeat_instance(bridge->health_agent, "personality_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int personality_plasticity_bridge_training_step(personality_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "personality_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    personality_plasticity_bridge_heartbeat_instance(bridge->health_agent, "personality_plasticity_bridge_training_step", progress);
    return 0;
}
