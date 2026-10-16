/**
 * @file nimcp_rcog_immune_bridge.c
 * @brief Brain Immune System Integration Bridge Implementation for Recursive Cognition
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "cognitive/recursive/nimcp_rcog_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(rcog_immune_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from rcog_immune_bridge module (instance-level) */
static inline void rcog_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_rcog_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_rcog_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_rcog_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

static nimcp_health_agent_t* g_rcog_immune_bridge_instance_health_agent = NULL;

void rcog_immune_bridge_set_instance_health_agent(
    rcog_immune_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    (void)bridge;
    g_rcog_immune_bridge_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int rcog_immune_bridge_training_begin(rcog_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    rcog_immune_bridge_heartbeat_instance(
        g_rcog_immune_bridge_instance_health_agent, "rcog_imm_training_begin", 0.0f);
    return 0;
}

int rcog_immune_bridge_training_step(rcog_immune_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_immune_bridge_training_step: NULL argument");
        return -1;
    }
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    rcog_immune_bridge_heartbeat_instance(
        g_rcog_immune_bridge_instance_health_agent, "rcog_imm_training_step", clamped);
    return 0;
}

int rcog_immune_bridge_training_end(rcog_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_immune_bridge_training_end: NULL argument");
        return -1;
    }
    rcog_immune_bridge_heartbeat_instance(
        g_rcog_immune_bridge_instance_health_agent, "rcog_imm_training_end", 1.0f);
    return 0;
}

#define LOG_MODULE "RCOG_IMMUNE_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Immune bridge internal structure
 */
struct rcog_immune_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    rcog_immune_bridge_config_t config;

    /* Connections */
    struct brain_immune_system* immune;
    struct rcog_engine* engine;
    bool connected;

    /* Effects state */
    rcog_to_immune_effects_t outgoing_effects;
    immune_to_rcog_effects_t incoming_effects;

    /* Quarantine list */
    rcog_quarantine_entry_t quarantine[RCOG_IMMUNE_MAX_QUARANTINED_PATTERNS];
    size_t num_quarantined;

    /* Current modulation */
    rcog_immune_modulation_t current_modulation;
    rcog_inflammation_level_t inflammation_level;
    rcog_cytokine_levels_t cytokines;

    /* Statistics */
    rcog_immune_bridge_stats_t stats;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

rcog_immune_bridge_config_t rcog_immune_bridge_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__default_config", 0.0f);


    rcog_immune_bridge_config_t config = {0};

    config.il1_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.il6_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.tnf_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.il10_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    config.min_capacity = 0.2f;
    config.min_depth = 2.0f;
    config.min_parallelism = 1.0f;

    config.quarantine_threshold = RCOG_IMMUNE_DEFAULT_QUARANTINE_THRESHOLD;
    config.quarantine_decay_rate = 0.01f;
    config.max_quarantine_entries = RCOG_IMMUNE_MAX_QUARANTINED_PATTERNS;

    config.recovery_rate = RCOG_IMMUNE_DEFAULT_RECOVERY_RATE;
    config.enable_auto_recovery = true;

    return config;
}

rcog_immune_bridge_t* rcog_immune_bridge_create(
    const rcog_immune_bridge_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__create", 0.0f);


    rcog_immune_bridge_t* bridge = nimcp_calloc(1, sizeof(rcog_immune_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = rcog_immune_bridge_default_config();
    }

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "rcog_immune") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "rcog_immune_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize healthy state */
    bridge->current_modulation.capacity_multiplier = 1.0f;
    bridge->current_modulation.max_depth_multiplier = 1.0f;
    bridge->current_modulation.parallelism_multiplier = 1.0f;
    bridge->current_modulation.timeout_multiplier = 1.0f;
    bridge->current_modulation.enable_degraded_mode = false;

    bridge->inflammation_level = RCOG_INFLAMMATION_NONE;

    return bridge;
}

rcog_immune_bridge_t* rcog_immune_bridge_create_default(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__create_default", 0.0f);


    return rcog_immune_bridge_create(NULL);
}

void rcog_immune_bridge_destroy(rcog_immune_bridge_t* bridge) {
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "rcog_immune");
    }

    /* Cleanup base bridge infrastructure */
    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__destroy", 0.0f);


    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

/*=============================================================================
 * CONNECTION
 *===========================================================================*/

int rcog_immune_bridge_connect(
    rcog_immune_bridge_t* bridge,
    struct brain_immune_system* immune
) {
    if (!bridge || !immune) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__connect", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->immune = immune;
    bridge->connected = (bridge->immune != NULL && bridge->engine != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return RCOG_OK;
}

int rcog_immune_bridge_connect_engine(
    rcog_immune_bridge_t* bridge,
    struct rcog_engine* engine
) {
    if (!bridge || !engine) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__connect_engine", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->engine = engine;
    bridge->connected = (bridge->immune != NULL && bridge->engine != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return RCOG_OK;
}

bool rcog_immune_bridge_is_connected(const rcog_immune_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__is_connected", 0.0f);


    return bridge && bridge->connected;
}

/*=============================================================================
 * UPDATE
 *===========================================================================*/

int rcog_immune_bridge_update(
    rcog_immune_bridge_t* bridge,
    float delta_time_ms
) {
    if (!bridge) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Decay quarantine strength over time */
    float decay = delta_time_ms / 1000.0f * bridge->config.quarantine_decay_rate;
    for (size_t i = 0; i < bridge->num_quarantined; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_quarantined > 256) {
            rcog_immune_bridge_heartbeat("rcog_immune__loop",
                             (float)(i + 1) / (float)bridge->num_quarantined);
        }

        bridge->quarantine[i].quarantine_strength -= decay;
        if (bridge->quarantine[i].quarantine_strength < 0) {
            bridge->quarantine[i].quarantine_strength = 0;
        }
    }

    /* Auto-recovery if enabled */
    if (bridge->config.enable_auto_recovery && bridge->inflammation_level > RCOG_INFLAMMATION_NONE) {
        float recovery = delta_time_ms / 1000.0f * bridge->config.recovery_rate;
        bridge->current_modulation.capacity_multiplier += recovery;
        if (bridge->current_modulation.capacity_multiplier > 1.0f) {
            bridge->current_modulation.capacity_multiplier = 1.0f;
        }
    }

    /* Update incoming effects */
    bridge->incoming_effects.capacity_multiplier = bridge->current_modulation.capacity_multiplier;
    bridge->incoming_effects.max_depth_multiplier = bridge->current_modulation.max_depth_multiplier;
    bridge->incoming_effects.parallelism_multiplier = bridge->current_modulation.parallelism_multiplier;
    bridge->incoming_effects.timeout_multiplier = bridge->current_modulation.timeout_multiplier;
    bridge->incoming_effects.enable_degraded_mode = bridge->current_modulation.enable_degraded_mode;
    bridge->incoming_effects.inflammation_level = bridge->inflammation_level;
    bridge->incoming_effects.cytokines = bridge->cytokines;
    bridge->incoming_effects.num_quarantined = (uint32_t)bridge->num_quarantined;

    nimcp_mutex_unlock(bridge->base.mutex);

    return RCOG_OK;
}

/*=============================================================================
 * MODULATION
 *===========================================================================*/

int rcog_immune_bridge_get_modulation(
    const rcog_immune_bridge_t* bridge,
    rcog_immune_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__get_modulation", 0.0f);


    nimcp_mutex_lock(((rcog_immune_bridge_t*)bridge)->base.mutex);
    *modulation = bridge->current_modulation;
    nimcp_mutex_unlock(((rcog_immune_bridge_t*)bridge)->base.mutex);

    return RCOG_OK;
}

int rcog_immune_bridge_apply_modulation(
    rcog_immune_bridge_t* bridge,
    struct rcog_orchestrator* orchestrator
) {
    if (!bridge || !orchestrator) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__apply_modulation", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* In full implementation, would apply modulation to orchestrator config */
    bridge->stats.modulations_applied++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return RCOG_OK;
}

rcog_inflammation_level_t rcog_immune_bridge_get_inflammation_level(
    const rcog_immune_bridge_t* bridge
) {
    if (!bridge) {
        return RCOG_INFLAMMATION_NONE;
    }
    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__get_inflammation_lev", 0.0f);


    return bridge->inflammation_level;
}

int rcog_immune_bridge_get_cytokines(
    const rcog_immune_bridge_t* bridge,
    rcog_cytokine_levels_t* cytokines
) {
    if (!bridge || !cytokines) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__get_cytokines", 0.0f);


    nimcp_mutex_lock(((rcog_immune_bridge_t*)bridge)->base.mutex);
    *cytokines = bridge->cytokines;
    nimcp_mutex_unlock(((rcog_immune_bridge_t*)bridge)->base.mutex);

    return RCOG_OK;
}

/*=============================================================================
 * FAILURE REPORTING
 *===========================================================================*/

/**
 * @brief Simple hash function for pattern identification
 */
static uint64_t hash_pattern(const void* data, size_t len) {
    uint64_t hash = 14695981039346656037ULL;
    const uint8_t* bytes = (const uint8_t*)data;
    for (size_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            rcog_immune_bridge_heartbeat("rcog_immune__loop",
                             (float)(i + 1) / (float)len);
        }

        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

int rcog_immune_bridge_report_failure(
    rcog_immune_bridge_t* bridge,
    const struct rcog_subtask* subtask,
    rcog_error_t error
) {
    if (!bridge) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__report_failure", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Hash the subtask to identify the pattern */
    uint64_t pattern_hash = hash_pattern(&subtask, sizeof(void*));

    /* Find or create quarantine entry */
    rcog_quarantine_entry_t* entry = NULL;
    for (size_t i = 0; i < bridge->num_quarantined; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_quarantined > 256) {
            rcog_immune_bridge_heartbeat("rcog_immune__loop",
                             (float)(i + 1) / (float)bridge->num_quarantined);
        }

        if (bridge->quarantine[i].pattern_hash == pattern_hash) {
            entry = &bridge->quarantine[i];
            break;
        }
    }

    if (!entry && bridge->num_quarantined < RCOG_IMMUNE_MAX_QUARANTINED_PATTERNS) {
        entry = &bridge->quarantine[bridge->num_quarantined++];
        entry->pattern_hash = pattern_hash;
        entry->failure_count = 0;
        entry->first_failure_ms = nimcp_platform_time_monotonic_ms();
        entry->quarantine_strength = 0;
    }

    if (entry) {
        entry->failure_count++;
        entry->last_failure_ms = nimcp_platform_time_monotonic_ms();
        entry->last_error = error;

        /* Increase quarantine strength based on failure count */
        if (entry->failure_count >= bridge->config.quarantine_threshold) {
            entry->quarantine_strength = 0.5f + 0.1f * (float)(entry->failure_count - bridge->config.quarantine_threshold);
            if (entry->quarantine_strength > 1.0f) {
                entry->quarantine_strength = 1.0f;
            }
            bridge->stats.patterns_quarantined++;
        }
    }

    bridge->stats.failures_reported++;
    bridge->outgoing_effects.total_failures++;
    bridge->outgoing_effects.consecutive_failures++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return RCOG_OK;
}

int rcog_immune_bridge_report_pattern_failure(
    rcog_immune_bridge_t* bridge,
    const struct rcog_decomposition* decomposition,
    rcog_error_t error
) {
    if (!bridge) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Use the decomposition pointer as pattern identifier */
    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__report_pattern_failu", 0.0f);


    uint64_t pattern_hash = hash_pattern(&decomposition, sizeof(void*));

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->outgoing_effects.failure_pattern_hash = pattern_hash;
    bridge->outgoing_effects.failure_error = error;
    bridge->outgoing_effects.report_subtask_failure = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    return rcog_immune_bridge_report_failure(bridge, NULL, error);
}

/*=============================================================================
 * QUARANTINE
 *===========================================================================*/

bool rcog_immune_bridge_is_quarantined(
    const rcog_immune_bridge_t* bridge,
    const struct rcog_decomposition* decomposition
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__is_quarantined", 0.0f);


    uint64_t pattern_hash = hash_pattern(&decomposition, sizeof(void*));

    nimcp_mutex_lock(((rcog_immune_bridge_t*)bridge)->base.mutex);

    for (size_t i = 0; i < bridge->num_quarantined; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_quarantined > 256) {
            rcog_immune_bridge_heartbeat("rcog_immune__loop",
                             (float)(i + 1) / (float)bridge->num_quarantined);
        }

        if (bridge->quarantine[i].pattern_hash == pattern_hash &&
            bridge->quarantine[i].quarantine_strength > 0.5f) {
            ((rcog_immune_bridge_t*)bridge)->stats.patterns_blocked++;
            nimcp_mutex_unlock(((rcog_immune_bridge_t*)bridge)->base.mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(((rcog_immune_bridge_t*)bridge)->base.mutex);
    return false;
}

float rcog_immune_bridge_get_quarantine_strength(
    const rcog_immune_bridge_t* bridge,
    const struct rcog_decomposition* decomposition
) {
    if (!bridge) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__get_quarantine_stren", 0.0f);


    uint64_t pattern_hash = hash_pattern(&decomposition, sizeof(void*));

    nimcp_mutex_lock(((rcog_immune_bridge_t*)bridge)->base.mutex);

    for (size_t i = 0; i < bridge->num_quarantined; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_quarantined > 256) {
            rcog_immune_bridge_heartbeat("rcog_immune__loop",
                             (float)(i + 1) / (float)bridge->num_quarantined);
        }

        if (bridge->quarantine[i].pattern_hash == pattern_hash) {
            float strength = bridge->quarantine[i].quarantine_strength;
            nimcp_mutex_unlock(((rcog_immune_bridge_t*)bridge)->base.mutex);
            return strength;
        }
    }

    nimcp_mutex_unlock(((rcog_immune_bridge_t*)bridge)->base.mutex);
    return 0.0f;
}

int rcog_immune_bridge_get_quarantine_list(
    const rcog_immune_bridge_t* bridge,
    rcog_quarantine_entry_t* entries,
    size_t max_entries,
    size_t* num_entries
) {
    if (!bridge || !entries || !num_entries) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__get_quarantine_list", 0.0f);


    nimcp_mutex_lock(((rcog_immune_bridge_t*)bridge)->base.mutex);

    size_t to_copy = bridge->num_quarantined;
    if (to_copy > max_entries) {
        to_copy = max_entries;
    }

    memcpy(entries, bridge->quarantine, to_copy * sizeof(rcog_quarantine_entry_t));
    *num_entries = to_copy;

    nimcp_mutex_unlock(((rcog_immune_bridge_t*)bridge)->base.mutex);

    return RCOG_OK;
}

int rcog_immune_bridge_clear_quarantine(
    rcog_immune_bridge_t* bridge,
    uint64_t pattern_hash
) {
    if (!bridge) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__clear_quarantine", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    for (size_t i = 0; i < bridge->num_quarantined; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_quarantined > 256) {
            rcog_immune_bridge_heartbeat("rcog_immune__loop",
                             (float)(i + 1) / (float)bridge->num_quarantined);
        }

        if (bridge->quarantine[i].pattern_hash == pattern_hash) {
            /* Shift remaining entries */
            memmove(&bridge->quarantine[i], &bridge->quarantine[i + 1],
                    (bridge->num_quarantined - i - 1) * sizeof(rcog_quarantine_entry_t));
            bridge->num_quarantined--;
            nimcp_mutex_unlock(bridge->base.mutex);
            return RCOG_OK;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return RCOG_ERROR_CONTEXT_NOT_FOUND;
}

/*=============================================================================
 * EFFECTS ACCESS
 *===========================================================================*/

int rcog_immune_bridge_get_outgoing_effects(
    const rcog_immune_bridge_t* bridge,
    rcog_to_immune_effects_t* effects
) {
    if (!bridge || !effects) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__get_outgoing_effects", 0.0f);


    nimcp_mutex_lock(((rcog_immune_bridge_t*)bridge)->base.mutex);
    *effects = bridge->outgoing_effects;
    nimcp_mutex_unlock(((rcog_immune_bridge_t*)bridge)->base.mutex);

    return RCOG_OK;
}

int rcog_immune_bridge_get_incoming_effects(
    const rcog_immune_bridge_t* bridge,
    immune_to_rcog_effects_t* effects
) {
    if (!bridge || !effects) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__get_incoming_effects", 0.0f);


    nimcp_mutex_lock(((rcog_immune_bridge_t*)bridge)->base.mutex);
    *effects = bridge->incoming_effects;
    nimcp_mutex_unlock(((rcog_immune_bridge_t*)bridge)->base.mutex);

    return RCOG_OK;
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

int rcog_immune_bridge_get_stats(
    const rcog_immune_bridge_t* bridge,
    rcog_immune_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__get_stats", 0.0f);


    nimcp_mutex_lock(((rcog_immune_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((rcog_immune_bridge_t*)bridge)->base.mutex);

    return RCOG_OK;
}

void rcog_immune_bridge_reset_stats(rcog_immune_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(rcog_immune_bridge_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int rcog_immune_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    rcog_immune_bridge_heartbeat("rcog_immune__query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Recursive_Cognition_Immune_Bridge_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                rcog_immune_bridge_heartbeat("rcog_immune__loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Log self-knowledge observations */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Recursive_Cognition_Immune_Bridge_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Recursive_Cognition_Immune_Bridge_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}
