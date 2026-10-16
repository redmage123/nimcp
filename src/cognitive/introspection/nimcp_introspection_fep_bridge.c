/**
 * @file nimcp_introspection_fep_bridge.c
 * @brief Introspection FEP Bridge Implementation
 */

#include "cognitive/introspection/nimcp_introspection_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE_INTROSPECTION_FEP "[INTROSPECTION_FEP]"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(introspection_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_introspection_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_introspection_fep_bridge_mesh_registry = NULL;

nimcp_error_t introspection_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_introspection_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "introspection_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "introspection_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_introspection_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_introspection_fep_bridge_mesh_registry = registry;
    return err;
}

void introspection_fep_bridge_mesh_unregister(void) {
    if (g_introspection_fep_bridge_mesh_registry && g_introspection_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_introspection_fep_bridge_mesh_registry, g_introspection_fep_bridge_mesh_id);
        g_introspection_fep_bridge_mesh_id = 0;
        g_introspection_fep_bridge_mesh_registry = NULL;
    }
}


static inline void introspection_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_introspection_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_introspection_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_introspection_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


int introspection_fep_bridge_default_config(introspection_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    introspection_fep_bridge_heartbeat("introspectio_default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->pe_threshold = INTROSPECTION_FEP_HIGH_PE_THRESHOLD;
    config->uncertainty_threshold = INTROSPECTION_FEP_HIGH_UNCERTAINTY;
    config->meta_learning_rate = INTROSPECTION_FEP_META_UPDATE_RATE;
    config->enable_precision_monitoring = true;
    config->enable_meta_learning = true;
    config->pe_sensitivity = 1.0f;
    return 0;
}

introspection_fep_bridge_t* introspection_fep_bridge_create(const introspection_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    introspection_fep_bridge_heartbeat("introspectio_create", 0.0f);


    introspection_fep_bridge_t* bridge = (introspection_fep_bridge_t*)nimcp_malloc(sizeof(introspection_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    memset(bridge, 0, sizeof(introspection_fep_bridge_t));
    if (config) bridge->config = *config;
    else introspection_fep_bridge_default_config(&bridge->config);
    if (bridge_base_init(&bridge->base, 0, "introspection_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    bridge->state.current_precision = 0.5f;
    NIMCP_LOGGING_INFO(LOG_MODULE_INTROSPECTION_FEP " Bridge created");
    return bridge;
}

void introspection_fep_bridge_destroy(introspection_fep_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    introspection_fep_bridge_heartbeat("introspectio_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) introspection_fep_bridge_disconnect_bio_async(bridge);
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int introspection_fep_bridge_connect_fep(introspection_fep_bridge_t* bridge, fep_system_t* fep) {
    /* Phase 8: Heartbeat at operation start */
    introspection_fep_bridge_heartbeat("introspectio_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_fep_bridge_connect_introspection(introspection_fep_bridge_t* bridge, introspection_context_t intro) {
    /* Phase 8: Heartbeat at operation start */
    introspection_fep_bridge_heartbeat("introspectio_connect_introspectio", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && intro, NIMCP_ERROR_NULL_POINTER, "bridge or introspection is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->introspection_system = intro;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_fep_estimate_precision(introspection_fep_bridge_t* bridge, float* precision) {
    /* Phase 8: Heartbeat at operation start */
    introspection_fep_bridge_heartbeat("introspectio_introspection_fep_es", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && precision, NIMCP_ERROR_NULL_POINTER, "bridge or precision is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *precision = bridge->state.current_precision;
    bridge->effects.precision_estimate = *precision;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_fep_monitor_uncertainty(introspection_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    introspection_fep_bridge_heartbeat("introspectio_introspection_fep_mo", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->effects.uncertainty_estimate = bridge->state.current_uncertainty;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_fep_meta_learn(introspection_fep_bridge_t* bridge, float prediction_error) {
    /* Phase 8: Heartbeat at operation start */
    introspection_fep_bridge_heartbeat("introspectio_introspection_fep_me", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_meta_learning) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    float pe_abs = fabsf(prediction_error);
    if (pe_abs > bridge->config.pe_threshold) {
        float lr = bridge->config.meta_learning_rate;
        bridge->state.current_precision *= (1.0f - lr);
        bridge->state.current_uncertainty += lr * 0.5f;
        if (bridge->state.current_uncertainty > 1.0f) bridge->state.current_uncertainty = 1.0f;
        bridge->state.pe_events_monitored++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_fep_bridge_update(introspection_fep_bridge_t* bridge, uint64_t delta_ms) {
    /* Phase 8: Heartbeat at operation start */
    introspection_fep_bridge_heartbeat("introspectio_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->effects.meta_confidence = 1.0f - bridge->state.current_uncertainty;
    if (isfinite(bridge->state.current_precision)) {
        bridge->stats.avg_precision = (bridge->stats.avg_precision * 0.99f) + (bridge->state.current_precision * 0.01f);
    }
    if (isfinite(bridge->state.current_uncertainty)) {
        bridge->stats.avg_uncertainty = (bridge->stats.avg_uncertainty * 0.99f) + (bridge->state.current_uncertainty * 0.01f);
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_fep_bridge_get_state(introspection_fep_bridge_t* bridge, introspection_fep_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    introspection_fep_bridge_heartbeat("introspectio_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_fep_bridge_get_stats(introspection_fep_bridge_t* bridge, introspection_fep_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    introspection_fep_bridge_heartbeat("introspectio_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_fep_bridge_connect_bio_async(introspection_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    introspection_fep_bridge_heartbeat("introspectio_connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_INTROSPECTION_BRIDGE,
        .module_name = "introspection_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        return 0;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_fep_bridge_connect_bio_async: validation failed");
    return -1;
}

int introspection_fep_bridge_disconnect_bio_async(introspection_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    /* Phase 8: Heartbeat at operation start */
    introspection_fep_bridge_heartbeat("introspectio_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool introspection_fep_bridge_is_bio_async_connected(const introspection_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    introspection_fep_bridge_heartbeat("introspectio_is_bio_async_connect", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ========================================================================
 * KG SELF-AWARENESS INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Query knowledge graph for self-knowledge about introspection FEP bridge
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int introspection_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    introspection_fep_bridge_heartbeat("introspectio_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Introspection_FEP_Bridge");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                introspection_fep_bridge_heartbeat("introspectio_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG(LOG_MODULE_INTROSPECTION_FEP " self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Introspection_FEP_Bridge");
    if (connections) {
        NIMCP_LOGGING_DEBUG(LOG_MODULE_INTROSPECTION_FEP " has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Introspection_FEP_Bridge");
    if (incoming) {
        NIMCP_LOGGING_DEBUG(LOG_MODULE_INTROSPECTION_FEP " has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Full Training
 * ============================================================================ */

/**
 * @brief Set instance-level health agent on bridge struct
 */
void introspection_fep_bridge_set_instance_health_agent(
    introspection_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/**
 * @brief Begin training - reset counters, set flags, log start
 */
int introspection_fep_bridge_training_begin(introspection_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "introspection_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    introspection_fep_bridge_heartbeat_instance(bridge->health_agent,
        "intro_fep_training_begin", 0.0f);
    bridge->stats.precision_estimates_total = 0;
    bridge->stats.avg_precision = 0.0f;
    bridge->state.current_precision = 0.5f; /* Reset to neutral baseline */
    NIMCP_LOGGING_INFO("[INTROSPECTION_FEP] Training begin: counters reset, baseline state initialized");
    return 0;
}

/**
 * @brief Training step - clamp progress [0,1], adapt thresholds/weights, increment counters
 */
int introspection_fep_bridge_training_step(introspection_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "introspection_fep_bridge_training_step: NULL argument");
        return -1;
    }
    /* Clamp progress to [0,1] */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    introspection_fep_bridge_heartbeat_instance(bridge->health_agent,
        "intro_fep_training_step", progress);
    /* Adapt meta learning rate based on training progress */
    float lr = bridge->config.meta_learning_rate;
    float adaptation = lr * (1.0f - progress) * 0.1f;
    bridge->config.meta_learning_rate = lr + adaptation;
    if (bridge->config.meta_learning_rate > 1.0f) bridge->config.meta_learning_rate = 1.0f;
    if (bridge->config.meta_learning_rate < 0.001f) bridge->config.meta_learning_rate = NIMCP_LEARNING_RATE_FINE;
    /* Blend state toward training target */
    bridge->state.current_precision = bridge->state.current_precision * 0.99f + progress * 0.01f;
    bridge->stats.precision_estimates_total++;
    return 0;
}

/**
 * @brief End training - compute averages, clear flags, log metrics
 */
int introspection_fep_bridge_training_end(introspection_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "introspection_fep_bridge_training_end: NULL argument");
        return -1;
    }
    introspection_fep_bridge_heartbeat_instance(bridge->health_agent,
        "intro_fep_training_end", 1.0f);
    /* Finalize state */
    if (bridge->state.current_precision < 0.0f) bridge->state.current_precision = 0.0f;
    if (bridge->state.current_precision > 1.0f) bridge->state.current_precision = 1.0f;
    NIMCP_LOGGING_INFO("[INTROSPECTION_FEP] Training end: precision=%.3f, steps=%u",
        bridge->state.current_precision, bridge->stats.precision_estimates_total);
    return 0;
}
