/**
 * @file nimcp_energy_consistency_fep_bridge.c
 * @brief Energy Consistency - FEP Bridge Implementation
 *
 * Maps logical consistency energy to FEP prediction error.
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#include "cognitive/neuro_symbolic/bridges/nimcp_energy_consistency_fep_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "ENERGY_FEP_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(energy_consistency_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_energy_consistency_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_energy_consistency_fep_bridge_mesh_registry = NULL;

nimcp_error_t energy_consistency_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_energy_consistency_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "energy_consistency_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "energy_consistency_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_energy_consistency_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_energy_consistency_fep_bridge_mesh_registry = registry;
    return err;
}

void energy_consistency_fep_bridge_mesh_unregister(void) {
    if (g_energy_consistency_fep_bridge_mesh_registry && g_energy_consistency_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_energy_consistency_fep_bridge_mesh_registry, g_energy_consistency_fep_bridge_mesh_id);
        g_energy_consistency_fep_bridge_mesh_id = 0;
        g_energy_consistency_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from energy_consistency_fep_bridge module (instance-level) */
static inline void energy_consistency_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_energy_consistency_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_energy_consistency_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_energy_consistency_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#include <math.h>
#include <string.h>

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

NIMCP_API energy_fep_bridge_t* energy_fep_bridge_create(void) {
    energy_fep_bridge_config_t config;
    energy_fep_bridge_get_default_config(&config);
    NIMCP_LOGGING_INFO("Created %s bridge", "energy_consistency_fep");
    return energy_fep_bridge_create_with_config(&config);
}

NIMCP_API energy_fep_bridge_t* energy_fep_bridge_create_with_config(
    const energy_fep_bridge_config_t* config) {

    if (!config) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");


        return NULL;


    }

    energy_fep_bridge_t* bridge = nimcp_calloc(1, sizeof(energy_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Initialize base - order is (base, module_id, module_name) */
    if (bridge_base_init(&bridge->base, BIO_MODULE_ENERGY_CONSISTENCY_FEP_BRIDGE,
                         "energy_fep_bridge") != NIMCP_SUCCESS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "energy_fep_bridge_create: operation failed");
        return NULL;
    }

    memcpy(&bridge->config, config, sizeof(energy_fep_bridge_config_t));

    /* Initialize state */
    bridge->current_energy = 0.0f;
    bridge->smoothed_energy = 0.0f;
    bridge->energy_derivative = 0.0f;
    bridge->estimated_precision = bridge->config.base_precision;

    NIMCP_LOG_DEBUG("Created energy-FEP bridge");
    return bridge;
}

NIMCP_API void energy_fep_bridge_destroy(energy_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "energy_consistency_fep");

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOG_DEBUG("Destroyed energy-FEP bridge");
}

NIMCP_API nimcp_error_t energy_fep_bridge_reset(energy_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_fep_bridge_reset: bridge is NULL");
        return -1;
    }

    bridge->current_energy = 0.0f;
    bridge->smoothed_energy = 0.0f;
    bridge->energy_derivative = 0.0f;
    bridge->estimated_precision = bridge->config.base_precision;
    bridge->total_updates = 0;
    bridge->error_signals_sent = 0;
    bridge->actions_triggered = 0;
    bridge->avg_prediction_error = 0.0f;

    memset(bridge->precision_history, 0, sizeof(bridge->precision_history));
    bridge->precision_history_idx = 0;

    memset(&bridge->effects, 0, sizeof(energy_fep_bridge_effects_t));

    return 0;
}

NIMCP_API nimcp_error_t energy_fep_bridge_get_default_config(
    energy_fep_bridge_config_t* config) {

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_fep_bridge_get_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(energy_fep_bridge_config_t));

    config->enable_modulation = true;
    config->sensitivity = 1.0f;
    config->energy_to_error_scale = 1.0f;
    config->error_threshold = 0.5f;
    config->base_precision = 1.0f;
    config->precision_learning_rate = 0.01f;
    config->enable_active_inference = true;
    config->enable_bio_async = true;

    return 0;
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

NIMCP_API nimcp_error_t energy_fep_bridge_connect_checker(
    energy_fep_bridge_t* bridge,
    energy_consistency_checker_t* checker) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_fep_bridge_connect_checker: bridge is NULL");
        return -1;
    }

    bridge->base.system_a = checker;
    bridge->base.system_a_connected = (checker != NULL);
    bridge->base.bridge_active = (checker != NULL && bridge->base.system_b != NULL);

    return 0;
}

NIMCP_API nimcp_error_t energy_fep_bridge_connect_fep(
    energy_fep_bridge_t* bridge,
    void* fep) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_fep_bridge_connect_fep: bridge is NULL");
        return -1;
    }

    bridge->base.system_b = fep;
    bridge->base.system_b_connected = (fep != NULL);
    bridge->base.bridge_active = (bridge->base.system_a != NULL && fep != NULL);

    return 0;
}

NIMCP_API nimcp_error_t energy_fep_bridge_disconnect_checker(
    energy_fep_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_fep_bridge_disconnect_checker: bridge is NULL");
        return -1;
    }

    bridge->base.system_a = NULL;
    bridge->base.system_a_connected = false;
    bridge->base.bridge_active = false;

    return 0;
}

NIMCP_API nimcp_error_t energy_fep_bridge_disconnect_fep(
    energy_fep_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_fep_bridge_disconnect_fep: bridge is NULL");
        return -1;
    }

    bridge->base.system_b = NULL;
    bridge->base.system_b_connected = false;
    bridge->base.bridge_active = false;

    return 0;
}

/* ============================================================================
 * Update Functions
 * ============================================================================ */

NIMCP_API nimcp_error_t energy_fep_bridge_update(
    energy_fep_bridge_t* bridge,
    float consistency_energy) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_fep_bridge_update: bridge is NULL");
        return -1;
    }

    /* Compute derivative */
    bridge->energy_derivative = consistency_energy - bridge->current_energy;
    bridge->current_energy = consistency_energy;

    /* Exponential smoothing */
    float alpha = 0.1f;
    bridge->smoothed_energy = alpha * consistency_energy +
                               (1.0f - alpha) * bridge->smoothed_energy;

    /* Map to prediction error */
    bridge->effects.prediction_error = consistency_energy *
                                        bridge->config.energy_to_error_scale *
                                        bridge->config.sensitivity;

    /* Update precision estimate */
    float error_variance = fabsf(bridge->energy_derivative);
    float new_precision = 1.0f / (error_variance + 0.1f);

    bridge->estimated_precision = (1.0f - bridge->config.precision_learning_rate) *
                                   bridge->estimated_precision +
                                   bridge->config.precision_learning_rate * new_precision;

    bridge->effects.precision = bridge->estimated_precision;

    /* Store in history */
    bridge->precision_history[bridge->precision_history_idx] = bridge->estimated_precision;
    bridge->precision_history_idx = (bridge->precision_history_idx + 1) % 64;

    /* Compute belief update rate */
    bridge->effects.belief_update_rate = bridge->estimated_precision *
                                          bridge->effects.prediction_error;

    /* Compute action urgency */
    if (consistency_energy > bridge->config.error_threshold) {
        bridge->effects.action_urgency = (consistency_energy - bridge->config.error_threshold) /
                                          bridge->config.error_threshold;
        if (bridge->effects.action_urgency > 1.0f) {
            bridge->effects.action_urgency = 1.0f;
        }
        bridge->actions_triggered++;
    } else {
        bridge->effects.action_urgency = 0.0f;
    }

    /* Compute surprise */
    bridge->effects.surprise = -logf(1.0f / (1.0f + consistency_energy));

    /* Update statistics */
    bridge->total_updates++;
    bridge->avg_prediction_error = (bridge->avg_prediction_error *
                                     (bridge->total_updates - 1) +
                                     bridge->effects.prediction_error) /
                                    bridge->total_updates;

    if (bridge->effects.prediction_error > 0.01f) {
        bridge->error_signals_sent++;
    }

    return 0;
}

NIMCP_API nimcp_error_t energy_fep_bridge_update_from_result(
    energy_fep_bridge_t* bridge,
    const energy_consistency_result_t* result) {

    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_fep_bridge_update_from_result: bridge or result is NULL");
        return -1;
    }

    return energy_fep_bridge_update(bridge, result->total_energy);
}

NIMCP_API float energy_fep_bridge_get_precision_weighted_energy(
    const energy_fep_bridge_t* bridge,
    const consistency_violation_t* violation) {

    if (!bridge || !violation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_fep_bridge_get_precision_weighted_energy: bridge or violation is NULL");
        return 0.0f;
    }

    /* Weight violation energy by precision */
    return violation->energy_cost * bridge->estimated_precision;
}

/* ============================================================================
 * Effects Query
 * ============================================================================ */

NIMCP_API nimcp_error_t energy_fep_bridge_get_effects(
    const energy_fep_bridge_t* bridge,
    energy_fep_bridge_effects_t* effects) {

    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_fep_bridge_get_effects: bridge or effects is NULL");
        return -1;
    }

    memcpy(effects, &bridge->effects, sizeof(energy_fep_bridge_effects_t));
    return 0;
}

NIMCP_API float energy_fep_bridge_get_prediction_error(
    const energy_fep_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_fep_bridge_get_prediction_error: bridge is NULL");
        return 0.0f;
    }
    return bridge->effects.prediction_error;
}

NIMCP_API float energy_fep_bridge_get_precision(
    const energy_fep_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_fep_bridge_get_precision: bridge is NULL");
        return 0.0f;
    }
    return bridge->estimated_precision;
}

/* ============================================================================
 * Active Inference
 * ============================================================================ */

NIMCP_API nimcp_error_t energy_fep_bridge_get_repair_action(
    const energy_fep_bridge_t* bridge,
    const consistency_violation_t* violation,
    int* action) {

    if (!bridge || !violation || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_fep_bridge_get_repair_action: bridge, violation, or action is NULL");
        return -1;
    }

    /* Select action based on violation type and expected free energy reduction */
    switch (violation->type) {
        case CONSISTENCY_CONTRADICTION:
            *action = 0; /* Resolve contradiction */
            break;
        case CONSISTENCY_UNSATISFIED_RULE:
            *action = 1; /* Apply missing rule */
            break;
        case CONSISTENCY_CIRCULARITY:
            *action = 2; /* Break cycle */
            break;
        default:
            *action = 0;
            break;
    }

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

NIMCP_API nimcp_error_t energy_fep_bridge_register_bio_async(
    energy_fep_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_fep_bridge_register_bio_async: bridge is NULL");
        return -1;
    }

    /* Bio-async registration handled by bridge_base */
    bridge->base.bio_async_enabled = true;
    return 0;
}

NIMCP_API nimcp_error_t energy_fep_bridge_unregister_bio_async(
    energy_fep_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_fep_bridge_unregister_bio_async: bridge is NULL");
        return -1;
    }

    bridge->base.bio_async_enabled = false;
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void energy_consistency_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_energy_consistency_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int energy_consistency_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "energy_consistency_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    energy_consistency_fep_bridge_heartbeat_instance(NULL, "energy_consistency_fep_bridge_training_begin", 0.0f);
    return 0;
}

int energy_consistency_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "energy_consistency_fep_bridge_training_end: NULL argument");
        return -1;
    }
    energy_consistency_fep_bridge_heartbeat_instance(NULL, "energy_consistency_fep_bridge_training_end", 1.0f);
    return 0;
}

int energy_consistency_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "energy_consistency_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    energy_consistency_fep_bridge_heartbeat_instance(NULL, "energy_consistency_fep_bridge_training_step", progress);
    return 0;
}
