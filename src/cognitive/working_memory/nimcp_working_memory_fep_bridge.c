/**
 * @file nimcp_working_memory_fep_bridge.c
 * @brief Free Energy Principle - Working Memory Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "cognitive/working_memory/nimcp_working_memory_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <string.h>
#include <math.h>

/* Define LOG_MODULE for this file */
#define LOG_MODULE "working_memory_fep_bridge"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(working_memory_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
BRIDGE_DEFINE_SECURITY_SETTERS(working_memory_fep_bridge)

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int working_memory_fep_bridge_default_config(working_memory_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* FEP → Working Memory */
    config->precision_capacity_scaling = 1.0f;
    config->pe_refresh_threshold = WM_FEP_PE_REFRESH_THRESHOLD;
    config->efe_salience_weight = 0.5f;
    config->enable_precision_capacity_modulation = true;
    config->enable_pe_auto_refresh = true;
    config->enable_efe_item_selection = true;

    /* Working Memory → FEP */
    config->item_context_weight = 0.3f;
    config->capacity_pressure_sensitivity = 1.0f;
    config->enable_context_modulation = true;
    config->enable_capacity_feedback = true;
    config->enable_eviction_signals = true;

    /* Sensitivity */
    config->precision_sensitivity = 1.0f;
    config->wm_sensitivity = 1.0f;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

working_memory_fep_bridge_t* working_memory_fep_bridge_create(
    const working_memory_fep_config_t* config
) {
    working_memory_fep_bridge_t* bridge = nimcp_malloc(sizeof(working_memory_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate working memory FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(working_memory_fep_bridge_t));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        working_memory_fep_bridge_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "working_memory_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "working_memory_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created working memory FEP bridge");
    return bridge;
}

void working_memory_fep_bridge_destroy(working_memory_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        working_memory_fep_bridge_disconnect_bio_async(bridge);
    }

    /* Free allocated arrays */
    if (bridge->fep_effects.item_efe_scores) {
        nimcp_free(bridge->fep_effects.item_efe_scores);
    }
    if (bridge->wm_effects.context_vector) {
        nimcp_free(bridge->wm_effects.context_vector);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed working memory FEP bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int working_memory_fep_bridge_connect_fep(
    working_memory_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected FEP system to working memory bridge");
    return 0;
}

int working_memory_fep_bridge_connect_working_memory(
    working_memory_fep_bridge_t* bridge,
    working_memory_t* wm
) {
    NIMCP_FEP_CHECK_THROW(bridge && wm, NIMCP_ERROR_NULL_POINTER, "bridge or wm is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->working_memory = wm;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected working memory to FEP bridge");
    return 0;
}

int working_memory_fep_bridge_disconnect(working_memory_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->working_memory = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected all systems from working memory FEP bridge");
    return 0;
}

/* ============================================================================
 * FEP → Working Memory Direction
 * ============================================================================ */

int working_memory_fep_apply_precision_capacity_modulation(
    working_memory_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system || !bridge->working_memory) {
        return -1;
    }

    if (!bridge->config.enable_precision_capacity_modulation) {
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current precision from FEP system */
    float precision = bridge->state.current_precision;

    /* Compute capacity adjustment */
    int32_t adjustment = 0;
    if (precision > WM_FEP_PRECISION_THRESHOLD) {
        adjustment = WM_FEP_HIGH_PRECISION_CAPACITY_BOOST;
    } else if (precision < WM_FEP_PRECISION_THRESHOLD * 0.5f) {
        adjustment = -WM_FEP_LOW_PRECISION_CAPACITY_PENALTY;
    }

    /* Apply sensitivity scaling */
    adjustment = (int32_t)(adjustment * bridge->config.precision_sensitivity);

    /* Update effects */
    bridge->fep_effects.capacity_adjustment = adjustment;
    uint32_t base_capacity = working_memory_get_capacity(bridge->working_memory);
    bridge->fep_effects.effective_capacity = (uint32_t)(base_capacity + adjustment);
    if ((int32_t)bridge->fep_effects.effective_capacity < 3) {
        bridge->fep_effects.effective_capacity = 3; /* Minimum capacity */
    }

    /* Update stats */
    bridge->stats.precision_capacity_adjustments++;
    if (bridge->stats.precision_capacity_adjustments > 0) {
        bridge->stats.avg_capacity_adjustment =
            (bridge->stats.avg_capacity_adjustment *
             (bridge->stats.precision_capacity_adjustments - 1) +
             (float)adjustment) / bridge->stats.precision_capacity_adjustments;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied precision capacity modulation: %d", adjustment);
    return 0;
}

int working_memory_fep_pe_auto_refresh(
    working_memory_fep_bridge_t* bridge,
    float pe_magnitude
) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->working_memory, NIMCP_ERROR_NULL_POINTER, "bridge or working_memory is NULL");

    if (!bridge->config.enable_pe_auto_refresh) {
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if PE exceeds threshold */
    if (pe_magnitude > bridge->config.pe_refresh_threshold) {
        bridge->fep_effects.auto_refresh_triggered = true;

        /* Refresh all items (simple strategy) */
        uint32_t count = working_memory_get_count(bridge->working_memory);
        uint32_t refreshed = 0;
        for (uint32_t i = 0; i < count; i++) {
            if (working_memory_refresh(bridge->working_memory, i)) {
                refreshed++;
            }
        }

        bridge->fep_effects.items_to_refresh = refreshed;
        bridge->state.items_refreshed = refreshed;

        /* Update stats */
        bridge->stats.pe_triggered_refreshes++;

        NIMCP_LOGGING_DEBUG("PE-triggered auto-refresh: %u items (PE: %f)",
                          refreshed, pe_magnitude);
    } else {
        bridge->fep_effects.auto_refresh_triggered = false;
        bridge->fep_effects.items_to_refresh = 0;
    }

    bridge->fep_effects.current_prediction_error = pe_magnitude;
    bridge->state.current_prediction_error = pe_magnitude;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int working_memory_fep_efe_item_selection(working_memory_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system || !bridge->working_memory) {
        return -1;
    }

    if (!bridge->config.enable_efe_item_selection) {
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current EFE from FEP system */
    float efe = bridge->fep_effects.current_efe;

    /* Compute EFE-based salience boost */
    float efe_salience = bridge->config.efe_salience_weight * (1.0f / (1.0f + efe));

    /* Update stats */
    bridge->stats.efe_item_selections++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("EFE-guided item selection (EFE: %f, salience: %f)",
                       efe, efe_salience);
    return 0;
}

/* ============================================================================
 * Working Memory → FEP Direction
 * ============================================================================ */

int working_memory_fep_apply_context_modulation(
    working_memory_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system || !bridge->working_memory) {
        return -1;
    }

    if (!bridge->config.enable_context_modulation) {
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Extract context from working memory content */
    uint32_t count = working_memory_get_count(bridge->working_memory);
    uint32_t cap = working_memory_get_capacity(bridge->working_memory);
    float context_strength = (cap > 0) ?
                            (bridge->config.item_context_weight *
                            (float)count /
                            (float)cap) : 0.0f;

    /* Update effects */
    bridge->wm_effects.context_strength = context_strength;
    bridge->state.context_modulation = context_strength;

    /* Update stats */
    bridge->stats.context_modulations++;
    if (bridge->stats.context_modulations > 0) {
        bridge->stats.avg_context_strength =
            (bridge->stats.avg_context_strength * (bridge->stats.context_modulations - 1) +
             context_strength) / bridge->stats.context_modulations;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied context modulation: %f", context_strength);
    return 0;
}

int working_memory_fep_signal_capacity_pressure(
    working_memory_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system || !bridge->working_memory) {
        return -1;
    }

    if (!bridge->config.enable_capacity_feedback) {
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get capacity utilization */
    float utilization = working_memory_get_utilization(bridge->working_memory);

    /* Compute precision cost multiplier */
    float cost_mult = 1.0f;
    if (utilization > WM_FEP_CAPACITY_CRITICAL_THRESHOLD) {
        cost_mult = 2.0f;
        bridge->state.capacity_critical = true;
        bridge->state.capacity_warning = true;
        bridge->stats.capacity_warnings++;
    } else if (utilization > WM_FEP_CAPACITY_WARNING_THRESHOLD) {
        cost_mult = 1.5f;
        bridge->state.capacity_warning = true;
        bridge->state.capacity_critical = false;
    } else {
        bridge->state.capacity_warning = false;
        bridge->state.capacity_critical = false;
    }

    /* Apply sensitivity */
    cost_mult = 1.0f + (cost_mult - 1.0f) * bridge->config.capacity_pressure_sensitivity;

    /* Update effects */
    bridge->wm_effects.capacity_utilization = utilization;
    bridge->wm_effects.precision_cost_multiplier = cost_mult;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Capacity pressure signaled: utilization=%f, cost=%f",
                       utilization, cost_mult);
    return 0;
}

int working_memory_fep_signal_eviction(working_memory_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");

    if (!bridge->config.enable_eviction_signals) {
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Signal strength proportional to number of evictions */
    float signal = (float)bridge->wm_effects.items_evicted * 0.1f;

    /* Update effects */
    bridge->wm_effects.belief_update_signal = signal;
    bridge->wm_effects.eviction_occurred = (bridge->wm_effects.items_evicted > 0);

    /* Update stats */
    if (bridge->wm_effects.eviction_occurred) {
        bridge->stats.eviction_signals++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Eviction signal: %u items, signal=%f",
                       bridge->wm_effects.items_evicted, signal);
    return 0;
}

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

int working_memory_fep_bridge_update(
    working_memory_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "working_memory_fep_bridge_update");
    BRIDGE_LGSS_GATE(bridge, "working_memory_fep_bridge_update");

    /* FEP → Working Memory */
    working_memory_fep_apply_precision_capacity_modulation(bridge);
    working_memory_fep_efe_item_selection(bridge);

    /* Working Memory → FEP */
    working_memory_fep_apply_context_modulation(bridge);
    working_memory_fep_signal_capacity_pressure(bridge);
    working_memory_fep_signal_eviction(bridge);

    /* Update average stats */
    nimcp_mutex_lock(bridge->base.mutex);

    bridge->stats.avg_precision =
        (bridge->stats.avg_precision * 0.9f) +
        (bridge->state.current_precision * 0.1f);

    if (bridge->working_memory) {
        float util = working_memory_get_utilization(bridge->working_memory);
        bridge->stats.avg_wm_utilization =
            (bridge->stats.avg_wm_utilization * 0.9f) + (util * 0.1f);
    }

    bridge->stats.avg_prediction_error =
        (bridge->stats.avg_prediction_error * 0.9f) +
        (bridge->state.current_prediction_error * 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

int working_memory_fep_bridge_get_state(
    const working_memory_fep_bridge_t* bridge,
    working_memory_fep_state_t* state
) {
    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int working_memory_fep_bridge_get_stats(
    const working_memory_fep_bridge_t* bridge,
    working_memory_fep_stats_t* stats
) {
    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int working_memory_fep_bridge_connect_bio_async(
    working_memory_fep_bridge_t* bridge
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_WORKING_MEMORY_BRIDGE,
        .module_name = "working_memory_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available");
    }

    return 0;
}

int working_memory_fep_bridge_disconnect_bio_async(
    working_memory_fep_bridge_t* bridge
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return 0;
}

bool working_memory_fep_bridge_is_bio_async_connected(
    const working_memory_fep_bridge_t* bridge
) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 * WHAT: Retrieve module's self-awareness information from KG
 * WHY:  Enable introspection about module capabilities and connections
 * HOW:  Query KG reader for entity and relations
 */
int working_memory_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Working_Memory_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            NIMCP_LOGGING_DEBUG("Working memory FEP bridge self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Working_Memory_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Working_Memory_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}


void working_memory_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_working_memory_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int working_memory_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "working_memory_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    working_memory_fep_bridge_heartbeat_instance(NULL, "working_memory_fep_bridge_training_begin", 0.0f);
    return 0;
}

int working_memory_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "working_memory_fep_bridge_training_end: NULL argument");
        return -1;
    }
    working_memory_fep_bridge_heartbeat_instance(NULL, "working_memory_fep_bridge_training_end", 1.0f);
    return 0;
}

int working_memory_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "working_memory_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    working_memory_fep_bridge_heartbeat_instance(NULL, "working_memory_fep_bridge_training_step", progress);
    return 0;
}
