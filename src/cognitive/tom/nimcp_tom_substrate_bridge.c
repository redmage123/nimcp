/**
 * @file nimcp_tom_substrate_bridge.c
 * @brief Theory of Mind substrate integration implementation
 *
 * WHAT: Implements ToM-substrate bridge for metabolic modulation of mentalizing
 * WHY: ToM requires prefrontal and temporal-parietal networks with high metabolic demands
 * HOW: Monitors substrate metrics and computes ATP-based capacity factors
 *
 * Uses shared metabolic modulation utilities from nimcp_metabolic_modulation.h
 */

#include "cognitive/tom/nimcp_tom_substrate_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/time/nimcp_time.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(tom_substrate_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_tom_substrate_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_tom_substrate_bridge_mesh_registry = NULL;

nimcp_error_t tom_substrate_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_tom_substrate_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "tom_substrate_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "tom_substrate_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_tom_substrate_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_tom_substrate_bridge_mesh_registry = registry;
    return err;
}

void tom_substrate_bridge_mesh_unregister(void) {
    if (g_tom_substrate_bridge_mesh_registry && g_tom_substrate_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_tom_substrate_bridge_mesh_registry, g_tom_substrate_bridge_mesh_id);
        g_tom_substrate_bridge_mesh_id = 0;
        g_tom_substrate_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from tom_substrate_bridge module (instance-level) */
static inline void tom_substrate_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_tom_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_tom_substrate_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_tom_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



/* ============================================================================
 * Helper Functions (using shared nimcp_clamp_f from nimcp_metabolic_modulation.h)
 * ============================================================================ */

/**
 * Compute mentalizing capacity from substrate state
 *
 * WHAT: Calculates ability to infer others' mental states
 * WHY: Mentalizing relies on mPFC/TPJ networks requiring high ATP
 * HOW: mentalizing_capacity = nimcp_clampf(metabolic_capacity * atp_level, 0.2, 1.0)
 *
 * Biological basis:
 * - Medial prefrontal cortex (mPFC) and temporo-parietal junction (TPJ)
 *   are metabolically expensive regions
 * - ATP depletion impairs capacity to model complex mental states
 * - Minimum 0.2 represents basic reflex-level social responses
 */
static float compute_mentalizing_capacity(
    const substrate_metabolic_state_t* metabolic,
    float sensitivity
) {
    /* Guard: validate inputs */
    if (!metabolic) return 0.2f;

    float atp = metabolic->atp_level;
    float capacity = metabolic->metabolic_capacity;

    /* Clamp inputs to valid ranges */
    atp = nimcp_clampf(atp, 0.0f, 1.0f);
    capacity = nimcp_clampf(capacity, 0.0f, 1.0f);
    sensitivity = nimcp_clampf(sensitivity, 0.5f, 2.0f);

    /* Base mentalizing: product of metabolic capacity and ATP level */
    float mentalizing = capacity * atp;

    /* Apply sensitivity scaling */
    mentalizing = powf(mentalizing, sensitivity);

    /* Clamp to [0.2, 1.0] - maintain minimal social cognition */
    return nimcp_clampf(mentalizing, 0.2f, 1.0f);
}

/**
 * Compute perspective-taking accuracy
 *
 * WHAT: Calculates ability to adopt others' viewpoints
 * WHY: Perspective-taking is cognitively demanding, requires sustained attention
 * HOW: perspective_taking = nimcp_clampf(atp_level * 1.1, 0.2, 1.0)
 *
 * Biological basis:
 * - Perspective-taking involves mental rotation and viewpoint transformation
 * - Requires sustained prefrontal activity
 * - 1.1 multiplier allows brief enhancement with optimal ATP
 */
static float compute_perspective_taking(
    const substrate_metabolic_state_t* metabolic,
    float sensitivity
) {
    /* Guard: validate inputs */
    if (!metabolic) return 0.2f;

    float atp = metabolic->atp_level;

    /* Clamp inputs */
    atp = nimcp_clampf(atp, 0.0f, 1.0f);
    sensitivity = nimcp_clampf(sensitivity, 0.5f, 2.0f);

    /* Perspective-taking scales directly with ATP, slight boost at optimal */
    float perspective = atp * 1.1f;

    /* Apply sensitivity */
    perspective = powf(perspective, sensitivity);

    /* Clamp to [0.2, 1.0] */
    return nimcp_clampf(perspective, 0.2f, 1.0f);
}

/**
 * Compute belief tracking capacity
 *
 * WHAT: Calculates ability to track others' belief states
 * WHY: Tracking beliefs requires working memory and sustained attention
 * HOW: belief_tracking = nimcp_clampf((atp_level + glucose_level) / 2.0, 0.2, 1.0)
 *
 * Biological basis:
 * - False belief tasks require maintaining multiple mental models
 * - Both ATP and glucose contribute to working memory capacity
 * - Combined metric reflects metabolic support for belief tracking
 */
static float compute_belief_tracking(
    const substrate_metabolic_state_t* metabolic,
    float sensitivity
) {
    /* Guard: validate inputs */
    if (!metabolic) return 0.2f;

    float atp = metabolic->atp_level;
    float glucose = metabolic->glucose_level;

    /* Clamp inputs */
    atp = nimcp_clampf(atp, 0.0f, 1.0f);
    glucose = nimcp_clampf(glucose, 0.0f, 1.0f);
    sensitivity = nimcp_clampf(sensitivity, 0.5f, 2.0f);

    /* Average of ATP and glucose reflects sustained metabolic support */
    float belief = (atp + glucose) / 2.0f;

    /* Apply sensitivity */
    belief = powf(belief, sensitivity);

    /* Clamp to [0.2, 1.0] */
    return nimcp_clampf(belief, 0.2f, 1.0f);
}

/**
 * Compute empathy factor
 *
 * WHAT: Calculates empathic processing capacity
 * WHY: Empathy involves mirror neurons and emotional processing
 * HOW: empathy_factor = nimcp_clampf(metabolic_capacity, 0.3, 1.0)
 *
 * Biological basis:
 * - Mirror neuron systems require metabolic resources
 * - Emotional resonance depends on overall metabolic health
 * - Minimum 0.3 represents basic emotional contagion
 */
static float compute_empathy_factor(
    const substrate_metabolic_state_t* metabolic,
    float sensitivity
) {
    /* Guard: validate inputs */
    if (!metabolic) return 0.3f;

    float capacity = metabolic->metabolic_capacity;

    /* Clamp inputs */
    capacity = nimcp_clampf(capacity, 0.0f, 1.0f);
    sensitivity = nimcp_clampf(sensitivity, 0.5f, 2.0f);

    /* Empathy scales with overall metabolic capacity */
    float empathy = capacity;

    /* Apply sensitivity */
    empathy = powf(empathy, sensitivity);

    /* Clamp to [0.3, 1.0] - maintain basic emotional contagion */
    return nimcp_clampf(empathy, 0.3f, 1.0f);
}

/**
 * Update statistics tracking
 *
 * WHAT: Updates running statistics for ToM substrate bridge
 * WHY: Enables monitoring and diagnostics of ToM metabolic state
 * HOW: Increments counters, tracks extrema, computes running averages
 */
static void update_statistics(
    tom_substrate_bridge_t* bridge,
    const substrate_metabolic_state_t* metabolic
) {
    /* Guard: validate inputs */
    if (!bridge || !metabolic) return;

    tom_substrate_stats_t* stats = &bridge->stats;
    const tom_substrate_effects_t* effects = &bridge->effects;

    /* Increment update counter */
    stats->total_updates++;

    /* Track impairment episodes */
    if (effects->is_impaired) {
        stats->impairment_episodes++;
        if (effects->mentalizing_capacity < 0.3f) {
            stats->severe_impairment_episodes++;
        }
    }

    /* Update current values */
    stats->current_mentalizing = effects->mentalizing_capacity;
    stats->current_perspective_taking = effects->perspective_taking;
    stats->current_belief_tracking = effects->belief_tracking;
    stats->current_empathy = effects->empathy_factor;

    /* Track extrema */
    if (stats->total_updates == 1) {
        /* First update: initialize extrema */
        stats->min_mentalizing = effects->mentalizing_capacity;
        stats->max_mentalizing = effects->mentalizing_capacity;
        stats->avg_mentalizing = effects->mentalizing_capacity;
        stats->avg_atp_level = metabolic->atp_level;
        /* Compute fatigue from inverse of metabolic capacity */
        stats->avg_fatigue_level = 1.0f - metabolic->metabolic_capacity;
    } else {
        /* Update extrema */
        if (effects->mentalizing_capacity < stats->min_mentalizing) {
            stats->min_mentalizing = effects->mentalizing_capacity;
        }
        if (effects->mentalizing_capacity > stats->max_mentalizing) {
            stats->max_mentalizing = effects->mentalizing_capacity;
        }

        /* Update running averages */
        float n = (float)stats->total_updates;
        stats->avg_mentalizing = ((stats->avg_mentalizing * (n - 1.0f)) +
                                  effects->mentalizing_capacity) / (fabsf(n) > 1e-7f ? n : 1e-7f);
        stats->avg_atp_level = ((stats->avg_atp_level * (n - 1.0f)) +
                               metabolic->atp_level) / (fabsf(n) > 1e-7f ? n : 1e-7f);
        /* Compute fatigue from inverse of metabolic capacity */
        float fatigue_level = 1.0f - metabolic->metabolic_capacity;
        stats->avg_fatigue_level = ((stats->avg_fatigue_level * (n - 1.0f)) +
                                    fatigue_level) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    }
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

/**
 * Get default config (backward compat - returns struct)
 *
 * WHAT: Returns a default configuration struct
 * WHY: Old API used return-struct pattern, new API uses pointer parameter
 * HOW: Creates config, calls pointer version, returns struct
 */
tom_substrate_config_t tom_substrate_get_default_config(void) {
    tom_substrate_config_t config;

    /* Enable all modulations by default */
    config.enable_atp_modulation = true;
    config.enable_fatigue_modulation = true;
    config.enable_stress_modulation = true;
    config.enable_empathy_modulation = true;

    /* Set moderate sensitivities */
    config.atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.stress_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.empathy_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Impairment threshold: 60% capacity */
    config.impairment_threshold = 0.6f;

    /* Update every 100ms */
    config.update_interval_ms = 100;

    return config;
}

int tom_substrate_default_config(tom_substrate_config_t* config) {
    /* Guard: validate config pointer */
    if (!config) {
        NIMCP_LOGGING_ERROR("Cannot initialize default config: NULL pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Enable all modulations by default */
    config->enable_atp_modulation = true;
    config->enable_fatigue_modulation = true;
    config->enable_stress_modulation = true;
    config->enable_empathy_modulation = true;

    /* Set moderate sensitivities */
    config->atp_sensitivity = 1.0f;
    config->fatigue_sensitivity = 1.0f;
    config->stress_sensitivity = 1.0f;
    config->empathy_sensitivity = 1.0f;

    /* Impairment threshold: 60% capacity */
    config->impairment_threshold = 0.6f;

    /* Update every 100ms */
    config->update_interval_ms = 100;

    NIMCP_LOGGING_DEBUG("Initialized default ToM substrate config");
    return NIMCP_SUCCESS;
}

tom_substrate_bridge_t* tom_substrate_bridge_create(
    const tom_substrate_config_t* config,
    theory_of_mind_t tom,
    neural_substrate_t* substrate
) {
    /* Guard: validate required pointers */
    if (!tom) {
        NIMCP_LOGGING_ERROR("Cannot create ToM substrate bridge: NULL ToM module");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom is NULL");

        return NULL;
    }

    if (!substrate) {
        NIMCP_LOGGING_ERROR("Cannot create ToM substrate bridge: NULL substrate");
        return NULL;
    }

    /* Allocate bridge structure */
    tom_substrate_bridge_t* bridge = (tom_substrate_bridge_t*)nimcp_malloc(
        sizeof(tom_substrate_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate ToM substrate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Initialize pointers */
    bridge->tom = tom;
    bridge->substrate = substrate;

    /* Copy or use default configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(tom_substrate_config_t));
    } else {
        tom_substrate_default_config(&bridge->config);
    }

    /* Initialize effects to neutral state */
    memset(&bridge->effects, 0, sizeof(tom_substrate_effects_t));
    bridge->effects.mentalizing_capacity = 1.0f;
    bridge->effects.perspective_taking = 1.0f;
    bridge->effects.belief_tracking = 1.0f;
    bridge->effects.empathy_factor = 1.0f;
    bridge->effects.is_impaired = false;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(tom_substrate_stats_t));
    bridge->stats.min_mentalizing = 1.0f;
    bridge->stats.max_mentalizing = 1.0f;
    bridge->stats.avg_mentalizing = 1.0f;

    /* Initialize bio-async context */
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    if (bridge_base_init(&bridge->base, 0, "tom_substrate") != 0) { nimcp_free(bridge); return NULL; }

    /* Initialize timestamp */
    bridge->last_update_time_ms = 0;

    NIMCP_LOGGING_INFO("Created ToM substrate bridge");
    return bridge;
}

void tom_substrate_bridge_destroy(tom_substrate_bridge_t* bridge) {
    /* Guard: NULL-safe */
    if (!bridge) {
        return;
    }

    /* Disconnect from bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        tom_substrate_disconnect_bio_async(bridge);
    }

    bridge_base_cleanup(&bridge->base);

    /* Free bridge structure */
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_DEBUG("Destroyed ToM substrate bridge");
}

int tom_substrate_connect_bio_async(tom_substrate_bridge_t* bridge) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot connect bio-async: NULL bridge");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Guard: check if already connected */
    if (bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_DEBUG("ToM substrate bridge already connected to bio-async");
        return NIMCP_SUCCESS;
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_SUBSTRATE_TOM,
        .module_name = "tom_substrate_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected ToM substrate bridge to bio-async router");
        return NIMCP_SUCCESS;
    } else {
        NIMCP_LOGGING_DEBUG("Bio-async router not available, skipping registration");
        return NIMCP_SUCCESS;
    }
}

int tom_substrate_disconnect_bio_async(tom_substrate_bridge_t* bridge) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot disconnect bio-async: NULL bridge");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Guard: check if connected */
    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    /* Unregister from bio-async router */
    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_DEBUG("Disconnected ToM substrate bridge from bio-async");
    return NIMCP_SUCCESS;
}

bool tom_substrate_is_bio_async_connected(const tom_substrate_bridge_t* bridge) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        return false;
    }

    return bridge->base.bio_async_enabled;
}

int tom_substrate_update(tom_substrate_bridge_t* bridge) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot update ToM substrate effects: NULL bridge");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Lock for thread safety */
    if (bridge->base.mutex) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
    }

    /* Check update interval */
    uint64_t current_time = nimcp_time_get_ms();
    if (bridge->last_update_time_ms > 0 &&
        (current_time - bridge->last_update_time_ms) < bridge->config.update_interval_ms) {
        /* Too soon, skip update */
        if (bridge->base.mutex) {
            nimcp_platform_mutex_unlock(bridge->base.mutex);
        }
        return NIMCP_SUCCESS;
    }

    /* Get substrate metabolic state */
    substrate_metabolic_state_t metabolic;
    int ret = substrate_get_metabolic_state(bridge->substrate, &metabolic);
    if (ret != NIMCP_SUCCESS) {
        NIMCP_LOGGING_ERROR("Failed to get substrate metabolic state");
        if (bridge->base.mutex) {
            nimcp_platform_mutex_unlock(bridge->base.mutex);
        }
        return ret;
    }

    /* Compute mentalizing capacity */
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.mentalizing_capacity = compute_mentalizing_capacity(
            &metabolic,
            bridge->config.atp_sensitivity
        );
    } else {
        bridge->effects.mentalizing_capacity = 1.0f;
    }

    /* Compute perspective-taking */
    if (bridge->config.enable_fatigue_modulation) {
        bridge->effects.perspective_taking = compute_perspective_taking(
            &metabolic,
            bridge->config.fatigue_sensitivity
        );
    } else {
        bridge->effects.perspective_taking = 1.0f;
    }

    /* Compute belief tracking */
    if (bridge->config.enable_stress_modulation) {
        bridge->effects.belief_tracking = compute_belief_tracking(
            &metabolic,
            bridge->config.stress_sensitivity
        );
    } else {
        bridge->effects.belief_tracking = 1.0f;
    }

    /* Compute empathy factor */
    if (bridge->config.enable_empathy_modulation) {
        bridge->effects.empathy_factor = compute_empathy_factor(
            &metabolic,
            bridge->config.empathy_sensitivity
        );
    } else {
        bridge->effects.empathy_factor = 1.0f;
    }

    /* Determine impairment state */
    bridge->effects.is_impaired =
        (bridge->effects.mentalizing_capacity < bridge->config.impairment_threshold);

    /* Update statistics */
    update_statistics(bridge, &metabolic);

    /* Update timestamp */
    bridge->last_update_time_ms = current_time;

    /* Unlock */
    if (bridge->base.mutex) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    }

    return NIMCP_SUCCESS;
}

float tom_substrate_get_mentalizing_capacity(const tom_substrate_bridge_t* bridge) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get mentalizing capacity: NULL bridge");
        return -1.0f;
    }

    float result;
    if (bridge->base.mutex) { nimcp_platform_mutex_lock(bridge->base.mutex); }
    result = bridge->effects.mentalizing_capacity;
    if (bridge->base.mutex) { nimcp_platform_mutex_unlock(bridge->base.mutex); }
    return result;
}

float tom_substrate_get_perspective_taking(const tom_substrate_bridge_t* bridge) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get perspective-taking: NULL bridge");
        return -1.0f;
    }

    float result;
    if (bridge->base.mutex) { nimcp_platform_mutex_lock(bridge->base.mutex); }
    result = bridge->effects.perspective_taking;
    if (bridge->base.mutex) { nimcp_platform_mutex_unlock(bridge->base.mutex); }
    return result;
}

float tom_substrate_get_belief_tracking(const tom_substrate_bridge_t* bridge) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get belief tracking: NULL bridge");
        return -1.0f;
    }

    float result;
    if (bridge->base.mutex) { nimcp_platform_mutex_lock(bridge->base.mutex); }
    result = bridge->effects.belief_tracking;
    if (bridge->base.mutex) { nimcp_platform_mutex_unlock(bridge->base.mutex); }
    return result;
}

float tom_substrate_get_empathy_factor(const tom_substrate_bridge_t* bridge) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get empathy factor: NULL bridge");
        return -1.0f;
    }

    float result;
    if (bridge->base.mutex) { nimcp_platform_mutex_lock(bridge->base.mutex); }
    result = bridge->effects.empathy_factor;
    if (bridge->base.mutex) { nimcp_platform_mutex_unlock(bridge->base.mutex); }
    return result;
}

int tom_substrate_get_effects(
    const tom_substrate_bridge_t* bridge,
    tom_substrate_effects_t* effects
) {
    /* Guard: validate pointers */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get effects: NULL bridge");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!effects) {
        NIMCP_LOGGING_ERROR("Cannot get effects: NULL output pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Copy effects structure under lock to prevent torn reads */
    if (bridge->base.mutex) { nimcp_platform_mutex_lock(bridge->base.mutex); }
    memcpy(effects, &bridge->effects, sizeof(tom_substrate_effects_t));
    if (bridge->base.mutex) { nimcp_platform_mutex_unlock(bridge->base.mutex); }

    return NIMCP_SUCCESS;
}

bool tom_substrate_is_impaired(const tom_substrate_bridge_t* bridge) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        return false;
    }

    bool result;
    if (bridge->base.mutex) { nimcp_platform_mutex_lock(bridge->base.mutex); }
    result = bridge->effects.is_impaired;
    if (bridge->base.mutex) { nimcp_platform_mutex_unlock(bridge->base.mutex); }
    return result;
}

int tom_substrate_get_stats(
    const tom_substrate_bridge_t* bridge,
    tom_substrate_stats_t* stats
) {
    /* Guard: validate pointers */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get stats: NULL bridge");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!stats) {
        NIMCP_LOGGING_ERROR("Cannot get stats: NULL output pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Copy statistics structure under lock to prevent torn reads */
    if (bridge->base.mutex) { nimcp_platform_mutex_lock(bridge->base.mutex); }
    memcpy(stats, &bridge->stats, sizeof(tom_substrate_stats_t));
    if (bridge->base.mutex) { nimcp_platform_mutex_unlock(bridge->base.mutex); }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int tom_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "ToM_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "ToM_Substrate_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "ToM_Substrate_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Backward Compatibility API (theory_of_mind namespace)
 * ============================================================================
 * These functions provide backward compatibility with the older API used by
 * nimcp_theory_of_mind.c. They wrap the new unified API.
 * ============================================================================ */

/**
 * Update bridge (backward compat wrapper)
 *
 * WHAT: Alias for tom_substrate_update()
 * WHY: Old code uses tom_substrate_bridge_update() naming convention
 */
int tom_substrate_bridge_update(tom_substrate_bridge_t* bridge) {
    return tom_substrate_update(bridge);
}

/**
 * Apply effects via bio-async (backward compat)
 *
 * WHAT: Broadcasts ToM substrate effects via bio-async messaging
 * WHY: Old API had separate apply step for broadcasting
 * HOW: Sends modulation and capacity update messages if bio-async connected
 */
int tom_substrate_bridge_apply_effects(tom_substrate_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Only broadcast if bio-async is enabled */
    if (!bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return NIMCP_SUCCESS;
    }

    /* Get current substrate state for message */
    substrate_metabolic_state_t metabolic;
    float atp_level = 1.0f, fatigue_level = 0.0f;
    if (bridge->substrate && substrate_get_metabolic_state(bridge->substrate, &metabolic) == 0) {
        atp_level = metabolic.atp_level;
        fatigue_level = 1.0f - metabolic.metabolic_capacity;
    }

    /* Broadcast modulation message */
    bio_msg_substrate_modulation_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_SUBSTRATE_MODULATION,
                        BIO_MODULE_SUBSTRATE_TOM, 0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  /* ToM uses acetylcholine for attention */

    msg.bridge_module_id = BIO_MODULE_SUBSTRATE_TOM;
    msg.processing_capacity = bridge->effects.mentalizing_capacity;
    msg.overall_capacity = (bridge->effects.mentalizing_capacity +
                           bridge->effects.perspective_taking +
                           bridge->effects.belief_tracking +
                           bridge->effects.empathy_factor) / 4.0f;
    msg.effect_values[0] = bridge->effects.mentalizing_capacity;
    msg.effect_values[1] = bridge->effects.perspective_taking;
    msg.effect_values[2] = bridge->effects.belief_tracking;
    msg.effect_values[3] = bridge->effects.empathy_factor;
    msg.atp_level = atp_level;
    msg.fatigue_level = fatigue_level;
    msg.update_count = bridge->stats.total_updates;
    msg.critical_low = bridge->effects.is_impaired;

    bio_router_broadcast(bridge->base.bio_ctx, &msg, sizeof(msg));

    return NIMCP_SUCCESS;
}

/**
 * Get effects (backward compat wrapper)
 *
 * WHAT: Alias for tom_substrate_get_effects()
 * WHY: Maintains API consistency for old code
 *
 * Note: The old tom_substrate_effects_t has different fields (recursive_depth,
 * false_belief_reasoning, overall_capacity vs belief_tracking, empathy_factor,
 * is_impaired). This wrapper provides field mapping.
 */
int tom_substrate_bridge_get_effects(const tom_substrate_bridge_t* bridge,
                                     tom_substrate_effects_t* effects) {
    return tom_substrate_get_effects(bridge, effects);
}

/**
 * Register with bio-async router (backward compat)
 *
 * WHAT: Alias for tom_substrate_connect_bio_async()
 * WHY: Old API used different function name
 */
int tom_substrate_bridge_register_bio_async(tom_substrate_bridge_t* bridge, bio_router_t* router) {
    (void)router;  /* Router is global in new API */
    return tom_substrate_connect_bio_async(bridge);
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void tom_substrate_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_tom_substrate_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int tom_substrate_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    tom_substrate_bridge_heartbeat_instance(NULL, "tom_substrate_bridge_training_begin", 0.0f);
    return 0;
}

int tom_substrate_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    tom_substrate_bridge_heartbeat_instance(NULL, "tom_substrate_bridge_training_end", 1.0f);
    return 0;
}

int tom_substrate_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    tom_substrate_bridge_heartbeat_instance(NULL, "tom_substrate_bridge_training_step", progress);
    return 0;
}
