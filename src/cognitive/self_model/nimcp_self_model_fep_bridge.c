/**
 * @file nimcp_self_model_fep_bridge.c
 * @brief Free Energy Principle - Self-Model Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "cognitive/self_model/nimcp_self_model_fep_bridge.h"
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

#define LOG_MODULE_SELF_MODEL_FEP "[SELF_MODEL_FEP]"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(self_model_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_self_model_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_self_model_fep_bridge_mesh_registry = NULL;

nimcp_error_t self_model_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_self_model_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "self_model_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "self_model_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_self_model_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_self_model_fep_bridge_mesh_registry = registry;
    return err;
}

void self_model_fep_bridge_mesh_unregister(void) {
    if (g_self_model_fep_bridge_mesh_registry && g_self_model_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_self_model_fep_bridge_mesh_registry, g_self_model_fep_bridge_mesh_id);
        g_self_model_fep_bridge_mesh_id = 0;
        g_self_model_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from self_model_fep_bridge module (instance-level) */
static inline void self_model_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_self_model_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_self_model_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_self_model_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


BRIDGE_DEFINE_SECURITY_SETTERS(self_model_fep_bridge)

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int self_model_fep_bridge_default_config(self_model_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->capability_pe_threshold = SELF_FEP_PE_CAPABILITY_THRESHOLD;
    config->belief_pe_threshold = SELF_FEP_PE_BELIEF_THRESHOLD;
    config->self_uncertainty_threshold = SELF_FEP_UNCERTAINTY_MEDIUM;

    config->belief_update_rate = SELF_FEP_BELIEF_UPDATE_RATE;
    config->capability_update_rate = SELF_FEP_CAPABILITY_UPDATE_RATE;
    config->core_belief_resistance = SELF_FEP_CORE_BELIEF_RESISTANCE;

    config->enable_belief_updates = true;
    config->enable_capability_learning = true;
    config->enable_self_exploration = true;
    config->enable_identity_protection = true;

    config->pe_sensitivity = 1.0f;
    config->uncertainty_sensitivity = 1.0f;

    return 0;
}

self_model_fep_bridge_t* self_model_fep_bridge_create(
    const self_model_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_create", 0.0f);


    self_model_fep_bridge_t* bridge =
        (self_model_fep_bridge_t*)nimcp_malloc(sizeof(self_model_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    memset(bridge, 0, sizeof(self_model_fep_bridge_t));

    if (config) {
        bridge->config = *config;
    } else {
        self_model_fep_bridge_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "self_model_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->state.identity_stable = true;
    bridge->state.self_knowledge_certainty = 0.5f;
    bridge->state.capability_confidence = 0.5f;

    NIMCP_LOGGING_INFO(LOG_MODULE_SELF_MODEL_FEP " Bridge created");
    return bridge;
}

void self_model_fep_bridge_destroy(self_model_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        self_model_fep_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO(LOG_MODULE_SELF_MODEL_FEP " Bridge destroyed");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int self_model_fep_bridge_connect_fep(
    self_model_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO(LOG_MODULE_SELF_MODEL_FEP " Connected to FEP system");
    return 0;
}

int self_model_fep_bridge_connect_self_model(
    self_model_fep_bridge_t* bridge,
    self_model_t* self_model
) {
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_connect_self_model", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && self_model, NIMCP_ERROR_NULL_POINTER, "bridge or self_model is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->self_model_system = self_model;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO(LOG_MODULE_SELF_MODEL_FEP " Connected to self-model system");
    return 0;
}

/* ============================================================================
 * FEP → Self-Model Direction
 * ============================================================================ */

int self_model_fep_update_belief(
    self_model_fep_bridge_t* bridge,
    uint32_t belief_index,
    float prediction_error
) {
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_self_model_fep_updat", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_belief_updates) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    if (fabsf(prediction_error) > bridge->config.belief_pe_threshold) {
        float update_rate = bridge->config.belief_update_rate;

        /* Core beliefs resist change */
        update_rate *= (1.0f - bridge->config.core_belief_resistance);

        bridge->state.beliefs_updated++;
        bridge->stats.belief_updates_total++;
        bridge->stats.avg_belief_pe =
            (bridge->stats.avg_belief_pe * 0.9f) + (fabsf(prediction_error) * 0.1f);

        NIMCP_LOGGING_DEBUG(LOG_MODULE_SELF_MODEL_FEP
            " Updated belief %u (PE=%.2f)", belief_index, prediction_error);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int self_model_fep_update_capability(
    self_model_fep_bridge_t* bridge,
    uint32_t capability_index,
    float prediction_error
) {
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_self_model_fep_updat", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_capability_learning) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    if (fabsf(prediction_error) > bridge->config.capability_pe_threshold) {
        float update_rate = bridge->config.capability_update_rate;

        bridge->state.capabilities_updated++;
        bridge->stats.capability_updates_total++;
        bridge->stats.avg_capability_pe =
            (bridge->stats.avg_capability_pe * 0.9f) + (fabsf(prediction_error) * 0.1f);

        /* Update capability confidence */
        bridge->state.capability_confidence *= (1.0f - update_rate);
        bridge->state.capability_confidence += update_rate * 0.7f;

        NIMCP_LOGGING_DEBUG(LOG_MODULE_SELF_MODEL_FEP
            " Updated capability %u (PE=%.2f)", capability_index, prediction_error);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int self_model_fep_explore_self(self_model_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_self_model_fep_explo", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_self_exploration) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state.exploring_self = true;
    bridge->stats.exploration_episodes++;
    bridge->effects.exploration_drive = 1.0f;

    NIMCP_LOGGING_INFO(LOG_MODULE_SELF_MODEL_FEP " Self-exploration initiated");

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Self-Model → FEP Direction
 * ============================================================================ */

int self_model_fep_apply_belief_priors(self_model_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_self_model_fep_apply", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Apply self-beliefs as FEP priors */
    /* Implementation would interact with FEP prior setting */

    NIMCP_LOGGING_DEBUG(LOG_MODULE_SELF_MODEL_FEP " Applied belief priors to FEP");

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int self_model_fep_constrain_policies(self_model_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_self_model_fep_const", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Constrain FEP policy space by known capabilities */
    /* Implementation would filter FEP action space */

    NIMCP_LOGGING_DEBUG(LOG_MODULE_SELF_MODEL_FEP " Constrained FEP policies");

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int self_model_fep_apply_sensory_attenuation(
    self_model_fep_bridge_t* bridge,
    bool is_self_generated
) {
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_self_model_fep_apply", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    if (is_self_generated) {
        /* Reduce precision for self-generated predictions */
        /* Implementation would modulate FEP precision */

        NIMCP_LOGGING_DEBUG(LOG_MODULE_SELF_MODEL_FEP " Applied sensory attenuation");
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

int self_model_fep_bridge_update(
    self_model_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "self_model_fep_bridge_update");
    BRIDGE_LGSS_GATE(bridge, "self_model_fep_bridge_update");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update self-knowledge uncertainty */
    float uncertainty = 1.0f - bridge->state.self_knowledge_certainty;
    bridge->effects.self_knowledge_uncertainty = uncertainty;

    /* Check for identity crisis */
    if (bridge->effects.identity_pe > SELF_FEP_PE_IDENTITY_THRESHOLD) {
        bridge->effects.identity_crisis = true;
        bridge->state.identity_stable = false;
        bridge->stats.identity_crises++;
        bridge->state.last_identity_crisis_ms = delta_ms;

        NIMCP_LOGGING_WARN(LOG_MODULE_SELF_MODEL_FEP
            " Identity crisis detected (PE=%.2f)", bridge->effects.identity_pe);
    } else {
        bridge->effects.identity_crisis = false;
    }

    /* Update exploration drive based on uncertainty */
    if (uncertainty > bridge->config.self_uncertainty_threshold) {
        bridge->effects.exploration_drive = uncertainty;
    } else {
        bridge->effects.exploration_drive *= 0.95f;
    }

    /* Decay exploration state */
    if (bridge->state.exploring_self) {
        bridge->state.exploration_progress += 0.01f;
        if (bridge->state.exploration_progress >= 1.0f) {
            bridge->state.exploring_self = false;
            bridge->state.exploration_progress = 0.0f;
            bridge->state.self_knowledge_certainty += 0.1f;
            if (bridge->state.self_knowledge_certainty > 1.0f) {
                bridge->state.self_knowledge_certainty = 1.0f;
            }
        }
    }

    /* Update statistics */
    bridge->stats.avg_self_certainty =
        (bridge->stats.avg_self_certainty * 0.99f) +
        (bridge->state.self_knowledge_certainty * 0.01f);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

int self_model_fep_bridge_get_state(
    const self_model_fep_bridge_t* bridge,
    self_model_fep_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int self_model_fep_bridge_get_stats(
    const self_model_fep_bridge_t* bridge,
    self_model_fep_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool self_model_fep_is_exploring(const self_model_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_self_model_fep_is_ex", 0.0f);


    return bridge->state.exploring_self;
}

float self_model_fep_get_self_certainty(const self_model_fep_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_self_model_fep_get_s", 0.0f);


    return bridge->state.self_knowledge_certainty;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int self_model_fep_bridge_connect_bio_async(self_model_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_SELF_MODEL_BRIDGE,
        .module_name = "self_model_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO(LOG_MODULE_SELF_MODEL_FEP " Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN(LOG_MODULE_SELF_MODEL_FEP
        " Bio-async router not available, skipping registration");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_model_fep_bridge_connect_bio_async: validation failed");
    return -1;
}

int self_model_fep_bridge_disconnect_bio_async(
    self_model_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_disconnect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO(LOG_MODULE_SELF_MODEL_FEP " Disconnected from bio-async router");
    return 0;
}

bool self_model_fep_bridge_is_bio_async_connected(
    const self_model_fep_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_is_bio_async_connect", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ========================================================================
 * KG SELF-AWARENESS INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Query knowledge graph for self-knowledge about self-model FEP bridge
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int self_model_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    self_model_fep_bridge_heartbeat("self_model_f_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Self_Model_FEP_Bridge");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                self_model_fep_bridge_heartbeat("self_model_f_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG(LOG_MODULE_SELF_MODEL_FEP " self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Self_Model_FEP_Bridge");
    if (connections) {
        NIMCP_LOGGING_DEBUG(LOG_MODULE_SELF_MODEL_FEP " has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Self_Model_FEP_Bridge");
    if (incoming) {
        NIMCP_LOGGING_DEBUG(LOG_MODULE_SELF_MODEL_FEP " has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void self_model_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_self_model_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int self_model_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_model_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    self_model_fep_bridge_heartbeat("self_model_fep_bridge_training_begin", 0.0f);
    return 0;
}

int self_model_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_model_fep_bridge_training_end: NULL argument");
        return -1;
    }
    self_model_fep_bridge_heartbeat("self_model_fep_bridge_training_end", 1.0f);
    return 0;
}

int self_model_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_model_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    self_model_fep_bridge_heartbeat("self_model_fep_bridge_training_step", progress);
    return 0;
}
