/**
 * @file nimcp_mirror_neurons_fep_bridge.c
 * @brief Free Energy Principle - Mirror Neurons Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "cognitive/mirror_neurons/nimcp_mirror_neurons_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mirror_neurons_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mirror_neurons_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mirror_neurons_fep_bridge_mesh_registry = NULL;

nimcp_error_t mirror_neurons_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_mirror_neurons_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mirror_neurons_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mirror_neurons_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mirror_neurons_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mirror_neurons_fep_bridge_mesh_registry = registry;
    return err;
}

void mirror_neurons_fep_bridge_mesh_unregister(void) {
    if (g_mirror_neurons_fep_bridge_mesh_registry && g_mirror_neurons_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mirror_neurons_fep_bridge_mesh_registry, g_mirror_neurons_fep_bridge_mesh_id);
        g_mirror_neurons_fep_bridge_mesh_id = 0;
        g_mirror_neurons_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from mirror_neurons_fep_bridge module (instance-level) */
static inline void mirror_neurons_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mirror_neurons_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mirror_neurons_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mirror_neurons_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(mirror_neurons_fep_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int mirror_neurons_fep_bridge_default_config(mirror_neurons_fep_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat("mirror_neuro_default_config", 0.0f);


    config->precision_gain_factor = 1.0f;
    config->min_mirror_gain = FEP_PRECISION_MIRROR_GAIN_MIN;
    config->max_mirror_gain = FEP_PRECISION_MIRROR_GAIN_MAX;

    config->goal_belief_coupling_rate = 0.3f;
    config->belief_goal_coupling_rate = 0.3f;
    config->confidence_threshold = GOAL_BELIEF_CONFIDENCE_THRESHOLD;

    config->motor_evidence_weight = 0.5f;
    config->motor_evidence_threshold = MOTOR_EVIDENCE_THRESHOLD;

    config->motor_pe_sensitivity = 1.0f;
    config->goal_pe_sensitivity = 1.0f;

    config->enable_precision_modulation = true;
    config->enable_goal_belief_coupling = true;
    config->enable_motor_evidence = true;
    config->enable_pe_propagation = true;

    return 0;
}

mirror_neurons_fep_bridge_t* mirror_neurons_fep_bridge_create(
    const mirror_neurons_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat("mirror_neuro_create", 0.0f);

    NIMCP_LOGGING_DEBUG("Creating mirror_neurons_fep_bridge");

    mirror_neurons_fep_bridge_t* bridge = (mirror_neurons_fep_bridge_t*)nimcp_calloc(
        1, sizeof(mirror_neurons_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_WARN("Failed to allocate mirror_neurons_fep_bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    mirror_neurons_fep_config_t default_cfg;
    if (!config) {
        mirror_neurons_fep_bridge_default_config(&default_cfg);
        config = &default_cfg;
    }
    bridge->config = *config;

    /* Initialize state */
    memset(&bridge->state, 0, sizeof(mirror_neurons_fep_state_t));
    bridge->state.current_precision = FEP_PRECISION_DEFAULT;

    /* Initialize effects */
    memset(&bridge->effects, 0, sizeof(mirror_neurons_fep_effects_t));
    bridge->effects.mirror_gain_modulation = 1.0f;

    /* Initialize stats */
    memset(&bridge->stats, 0, sizeof(mirror_neurons_fep_stats_t));
    bridge->stats.avg_mirror_gain = 1.0f;
    bridge->stats.min_mirror_gain_applied = 1.0f;
    bridge->stats.max_mirror_gain_applied = 1.0f;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "mirror_neurons_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        mirror_neurons_fep_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_neurons_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Mirror neurons-FEP bridge created");
    return bridge;
}

void mirror_neurons_fep_bridge_destroy(mirror_neurons_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_destroy", 0.0f);

    NIMCP_LOGGING_DEBUG("Destroying mirror_neurons_fep_bridge");


    if (bridge->base.bio_async_enabled) {
        mirror_neurons_fep_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Mirror neurons-FEP bridge destroyed");
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

int mirror_neurons_fep_bridge_connect_fep(
    mirror_neurons_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Allow NULL fep to disconnect/reset FEP connection */

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_connect_fep", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Mirror neurons-FEP bridge connected to FEP system");
    return 0;
}

int mirror_neurons_fep_bridge_connect_mirror_neurons(
    mirror_neurons_fep_bridge_t* bridge,
    mirror_hierarchy_t mirror
) {
    if (!bridge || !mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_fep_bridge_connect_mirror_neurons: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_connect_mirror_neuro", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->mirror_system = mirror;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Mirror neurons-FEP bridge connected to mirror neuron system");
    return 0;
}

/* ============================================================================
 * FEP → Mirror Neurons Implementation
 * ============================================================================ */

int mirror_neurons_fep_apply_precision_modulation(
    mirror_neurons_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system || !bridge->mirror_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_fep_apply_precision_modulation: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enable_precision_modulation) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_mirror_neurons_fep_a", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get average precision across FEP hierarchy */
    fep_system_t* fep = bridge->fep_system;
    float total_precision = 0.0f;
    uint32_t precision_count = 0;

    for (uint32_t l = 0; l < fep->num_levels; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && fep->num_levels > 256) {
            mirror_neurons_fep_bridge_heartbeat("mirror_neuro_loop",
                             (float)(l + 1) / (float)fep->num_levels);
        }

        fep_hierarchy_level_t* level = &fep->levels[l];
        for (uint32_t i = 0; i < level->errors.dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && level->errors.dim > 256) {
                mirror_neurons_fep_bridge_heartbeat("mirror_neuro_loop",
                                 (float)(i + 1) / (float)level->errors.dim);
            }

            total_precision += level->errors.precision[i];
            precision_count++;
        }
    }

    float avg_precision = precision_count > 0 ? total_precision / precision_count : 1.0f;
    bridge->state.current_precision = avg_precision;

    /* Map precision to mirror gain */
    float gain = nimcp_myelin_clamp(
        avg_precision * bridge->config.precision_gain_factor,
        bridge->config.min_mirror_gain,
        bridge->config.max_mirror_gain
    );

    bridge->effects.mirror_gain_modulation = gain;

    /* Update statistics */
    bridge->stats.avg_mirror_gain =
        (bridge->stats.avg_mirror_gain * 0.99f) + (gain * 0.01f);
    if (gain < bridge->stats.min_mirror_gain_applied) {
        bridge->stats.min_mirror_gain_applied = gain;
    }
    if (gain > bridge->stats.max_mirror_gain_applied) {
        bridge->stats.max_mirror_gain_applied = gain;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_neurons_fep_update_goals_from_beliefs(
    mirror_neurons_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system || !bridge->mirror_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_fep_update_goals_from_beliefs: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enable_goal_belief_coupling) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_mirror_neurons_fep_u", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_system_t* fep = bridge->fep_system;

    /* Use top-level FEP beliefs as goal activations */
    if (fep->num_levels > 0) {
        fep_belief_t* beliefs = &fep->levels[fep->num_levels - 1].beliefs;

        /* Map belief means to goal activations (simplified) */
        uint32_t max_goals = beliefs->dim < 32 ? beliefs->dim : 32;

        for (uint32_t g = 0; g < max_goals; g++) {
            /* Phase 8: Loop progress heartbeat */
            if ((g & 0xFF) == 0 && max_goals > 256) {
                mirror_neurons_fep_bridge_heartbeat("mirror_neuro_loop",
                                 (float)(g + 1) / (float)max_goals);
            }

            float belief_activation = beliefs->mean[g];

            /* Only update if above confidence threshold */
            if (belief_activation >= bridge->config.confidence_threshold) {
                /* Goal activation boost from FEP beliefs */
                bridge->effects.goal_activation_boost =
                    belief_activation * bridge->config.belief_goal_coupling_rate;

                bridge->state.belief_goal_updates++;
                bridge->stats.total_belief_updates++;
            }
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_neurons_fep_propagate_prediction_errors(
    mirror_neurons_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system || !bridge->mirror_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_fep_propagate_prediction_errors: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enable_pe_propagation) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_mirror_neurons_fep_p", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_system_t* fep = bridge->fep_system;

    /* Compute average prediction errors */
    float total_pe = 0.0f;
    uint32_t pe_count = 0;

    for (uint32_t l = 0; l < fep->num_levels; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && fep->num_levels > 256) {
            mirror_neurons_fep_bridge_heartbeat("mirror_neuro_loop",
                             (float)(l + 1) / (float)fep->num_levels);
        }

        total_pe += fep->levels[l].errors.magnitude;
        pe_count++;
    }

    float avg_pe = pe_count > 0 ? total_pe / pe_count : 0.0f;

    /* Scale by sensitivity */
    bridge->effects.motor_prediction_error = avg_pe * bridge->config.motor_pe_sensitivity;
    bridge->state.avg_motor_pe =
        (bridge->state.avg_motor_pe * 0.9f) + (bridge->effects.motor_prediction_error * 0.1f);

    /* Update statistics */
    bridge->stats.avg_motor_pe = bridge->state.avg_motor_pe;
    if (avg_pe > MOTOR_PE_THRESHOLD_HIGH) {
        bridge->stats.high_motor_pe_count++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Mirror Neurons → FEP Implementation
 * ============================================================================ */

int mirror_neurons_fep_transfer_goals_to_beliefs(
    mirror_neurons_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system || !bridge->mirror_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_fep_transfer_goals_to_beliefs: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enable_goal_belief_coupling) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat("mirror_neuro_mirror_neurons_fep_t", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    mirror_hierarchy_t mirror = bridge->mirror_system;
    fep_system_t* fep = bridge->fep_system;

    /* Get mirror neuron statistics to extract goal info */
    mirror_hierarchy_stats_t mirror_stats;
    if (mirror_hierarchy_get_stats(mirror, &mirror_stats)) {
        bridge->state.active_goals = mirror_stats.active_goals;
        bridge->state.max_goal_activation = mirror_stats.mean_goal_activation;

        /* Transfer to FEP top-level beliefs (simplified) */
        if (fep->num_levels > 0 && mirror_stats.active_goals > 0) {
            fep_belief_t* beliefs = &fep->levels[fep->num_levels - 1].beliefs;

            /* Update belief means based on goal activations */
            float coupling_rate = bridge->config.goal_belief_coupling_rate;

            for (uint32_t i = 0; i < beliefs->dim && i < 32; i++) {
                /* Simplified: blend current belief with goal-derived evidence */
                beliefs->mean[i] = beliefs->mean[i] * (1.0f - coupling_rate) +
                                  (mirror_stats.mean_goal_activation * coupling_rate);
            }

            bridge->state.goal_belief_updates++;
            bridge->stats.total_goal_updates++;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_neurons_fep_provide_motor_evidence(
    mirror_neurons_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system || !bridge->mirror_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_fep_provide_motor_evidence: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enable_motor_evidence) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat("mirror_neuro_mirror_neurons_fep_p", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    mirror_hierarchy_t mirror = bridge->mirror_system;

    /* Get motor activation statistics */
    mirror_hierarchy_stats_t mirror_stats;
    if (mirror_hierarchy_get_stats(mirror, &mirror_stats)) {
        bridge->state.active_motors = mirror_stats.active_motors;
        bridge->state.max_motor_activation = mirror_stats.mean_motor_activation;

        /* Provide motor activations as sensory evidence if above threshold */
        if (mirror_stats.mean_motor_activation >= bridge->config.motor_evidence_threshold) {
            bridge->state.motor_evidence_samples++;
            bridge->stats.total_motor_evidence++;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_neurons_fep_set_precision_from_resonance(
    mirror_neurons_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system || !bridge->mirror_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_fep_set_precision_from_resonance: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat("mirror_neuro_mirror_neurons_fep_s", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get mirror neuron resonance strength */
    mirror_hierarchy_stats_t mirror_stats;
    if (mirror_hierarchy_get_stats(bridge->mirror_system, &mirror_stats)) {
        /* Use mean goal/motor activation as resonance strength proxy */
        float resonance = (mirror_stats.mean_goal_activation +
                          mirror_stats.mean_motor_activation) * 0.5f;

        /* Map resonance to FEP precision */
        fep_system_t* fep = bridge->fep_system;
        float precision_scale = nimcp_myelin_clamp(resonance, 0.5f, 1.5f);

        /* Apply to lowest FEP level (sensory) */
        if (fep->num_levels > 0) {
            fep_hierarchy_level_t* sensory = &fep->levels[0];
            for (uint32_t i = 0; i < sensory->errors.dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && sensory->errors.dim > 256) {
                    mirror_neurons_fep_bridge_heartbeat("mirror_neuro_loop",
                                     (float)(i + 1) / (float)sensory->errors.dim);
                }

                sensory->errors.precision[i] *= precision_scale;
            }
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Cycle Implementation
 * ============================================================================ */

int mirror_neurons_fep_bridge_update(
    mirror_neurons_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* FEP → Mirror Neurons */
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat("mirror_neuro_update", 0.0f);


    mirror_neurons_fep_apply_precision_modulation(bridge);
    mirror_neurons_fep_update_goals_from_beliefs(bridge);
    mirror_neurons_fep_propagate_prediction_errors(bridge);

    /* Mirror Neurons → FEP */
    mirror_neurons_fep_transfer_goals_to_beliefs(bridge);
    mirror_neurons_fep_provide_motor_evidence(bridge);
    mirror_neurons_fep_set_precision_from_resonance(bridge);

    /* Update current free energy for stats */
    if (bridge->fep_system) {
        bridge->state.current_free_energy = bridge->fep_system->free_energy.total;
    }

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * State/Stats Implementation
 * ============================================================================ */

int mirror_neurons_fep_bridge_get_state(
    const mirror_neurons_fep_bridge_t* bridge,
    mirror_neurons_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_fep_bridge_get_state: required parameter is NULL");
        return -1;
    }
    *state = bridge->state;
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat("mirror_neuro_get_state", 0.0f);


    return 0;
}

int mirror_neurons_fep_bridge_get_stats(
    const mirror_neurons_fep_bridge_t* bridge,
    mirror_neurons_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_fep_bridge_get_stats: required parameter is NULL");
        return -1;
    }
    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat("mirror_neuro_get_stats", 0.0f);


    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int mirror_neurons_fep_bridge_connect_bio_async(
    mirror_neurons_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat("mirror_neuro_connect_bio_async", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_MIRROR_NEURONS_BRIDGE,
        .module_name = "mirror_neurons_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Mirror neurons-FEP bridge connected to bio-async");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }
    return 0;
}

int mirror_neurons_fep_bridge_disconnect_bio_async(
    mirror_neurons_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat("mirror_neuro_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool mirror_neurons_fep_bridge_is_bio_async_connected(
    const mirror_neurons_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat("mirror_neuro_is_bio_async_connect", 0.0f);


    return bridge && bridge->base.bio_async_enabled;
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

int mirror_neurons_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_fep_bridge_heartbeat("mirror_neuro_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Mirror_Neurons_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                mirror_neurons_fep_bridge_heartbeat("mirror_neuro_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Mirror neurons FEP bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Mirror_Neurons_FEP_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Mirror_Neurons_FEP_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void mirror_neurons_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mirror_neurons_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int mirror_neurons_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_neurons_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_neurons_fep_bridge_heartbeat_instance(NULL, "mirror_neurons_fep_bridge_training_begin", 0.0f);
    return 0;
}

int mirror_neurons_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_neurons_fep_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_neurons_fep_bridge_heartbeat_instance(NULL, "mirror_neurons_fep_bridge_training_end", 1.0f);
    return 0;
}

int mirror_neurons_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_neurons_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mirror_neurons_fep_bridge_heartbeat_instance(NULL, "mirror_neurons_fep_bridge_training_step", progress);
    return 0;
}
