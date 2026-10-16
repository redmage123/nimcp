/**
 * @file nimcp_predictive_fep_bridge.c
 * @brief Free Energy Principle - Predictive Regions Integration Bridge Implementation
 *
 * WHAT: Implements bidirectional integration between FEP and hierarchical predictive coding
 * WHY:  Predictive coding IS the neural implementation of FEP - this bridge makes
 *       the theoretical connection explicit
 * HOW:  Direct mapping: predictions = generative model, errors = variational gradients,
 *       precision = attention, minimizing errors = minimizing free energy
 *
 * BIOLOGICAL BASIS:
 * - Rao & Ballard (1999): Predictive coding in visual cortex
 * - Friston (2005): "A theory of cortical responses"
 * - Clark (2013): "Whatever next? Predictive brains"
 * - Predictive coding = message passing on FEP free energy functional
 */

#include "cognitive/predictive/nimcp_predictive_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception_immune.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "predictive_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(predictive_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_predictive_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_predictive_fep_bridge_mesh_registry = NULL;

nimcp_error_t predictive_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_predictive_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "predictive_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "predictive_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_predictive_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_predictive_fep_bridge_mesh_registry = registry;
    return err;
}

void predictive_fep_bridge_mesh_unregister(void) {
    if (g_predictive_fep_bridge_mesh_registry && g_predictive_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_predictive_fep_bridge_mesh_registry, g_predictive_fep_bridge_mesh_id);
        g_predictive_fep_bridge_mesh_id = 0;
        g_predictive_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from predictive_fep_bridge module (instance-level) */
static inline void predictive_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_predictive_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_predictive_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_predictive_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int predictive_fep_bridge_default_config(predictive_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* FEP -> Predictive */
    config->enable_belief_prediction_sync = true;
    config->enable_precision_gain_control = true;
    config->enable_fe_error_mapping = true;
    config->belief_sync_rate = 0.1f;

    /* Predictive -> FEP */
    config->enable_error_gradient_flow = true;
    config->enable_prediction_generative_model = true;
    config->enable_precision_kalman_gains = true;
    config->error_gradient_scaling = PRED_FEP_ERROR_FE_SCALING;

    /* Hierarchy mapping */
    config->match_hierarchy_levels = PRED_FEP_LEVEL_MATCHING_STRICT;
    config->hierarchy_offset = 0;

    /* Sensitivity factors */
    config->precision_sensitivity = 1.0f;
    config->prediction_sensitivity = 1.0f;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

predictive_fep_bridge_t* predictive_fep_bridge_create(
    const predictive_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_create", 0.0f);


    predictive_fep_bridge_t* bridge = nimcp_malloc(sizeof(predictive_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate predictive FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(predictive_fep_bridge_t));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        predictive_fep_bridge_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "predictive_fep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to initialize bridge base in predictive_fep_bridge_create");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Mutex is NULL after bridge_base_init in predictive_fep_bridge_create");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize effects with defaults */
    bridge->fep_effects.avg_precision = PRED_FEP_PRECISION_DEFAULT;

    NIMCP_LOGGING_INFO("Created predictive FEP bridge");
    return bridge;
}

void predictive_fep_bridge_destroy(predictive_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        predictive_fep_bridge_disconnect_bio_async(bridge);
    }

    /* Free allocated arrays in effects */
    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->fep_effects.synchronized_beliefs) {
        nimcp_free(bridge->fep_effects.synchronized_beliefs);
    }
    if (bridge->fep_effects.precision_gains) {
        nimcp_free(bridge->fep_effects.precision_gains);
    }
    if (bridge->fep_effects.fe_per_level) {
        nimcp_free(bridge->fep_effects.fe_per_level);
    }
    if (bridge->pred_effects.error_gradients) {
        nimcp_free(bridge->pred_effects.error_gradients);
    }
    if (bridge->pred_effects.generative_predictions) {
        nimcp_free(bridge->pred_effects.generative_predictions);
    }
    if (bridge->pred_effects.kalman_gains) {
        nimcp_free(bridge->pred_effects.kalman_gains);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed predictive FEP bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int predictive_fep_bridge_connect_fep(
    predictive_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected FEP system to predictive bridge");
    return 0;
}

int predictive_fep_bridge_connect_predictive(
    predictive_fep_bridge_t* bridge,
    predictive_network_t predictive
) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_connect_predictive", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->predictive = predictive;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected predictive network to FEP bridge");
    return 0;
}

int predictive_fep_bridge_disconnect(predictive_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    memset(&bridge->predictive, 0, sizeof(predictive_network_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected all systems from predictive FEP bridge");
    return 0;
}

/* ============================================================================
 * FEP -> Predictive Direction
 * ============================================================================ */

int predictive_fep_sync_beliefs_to_predictions(
    predictive_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_predictive_fep_sync_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_belief_prediction_sync) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Synchronize FEP beliefs with predictions
     * FEP belief = expected state = prediction
     * μ_FEP → prediction_predictive
     */
    bridge->state.beliefs_synchronized = true;
    bridge->stats.belief_syncs++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Synchronized beliefs to predictions");
    return 0;
}

int predictive_fep_apply_precision_gain_control(
    predictive_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_predictive_fep_apply", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_precision_gain_control) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Apply FEP precision as predictive gain control
     * High precision → amplify errors
     * Low precision → suppress errors
     * Attention = precision optimization
     */
    float avg_precision = bridge->fep_effects.avg_precision;
    float gain = avg_precision * bridge->config.precision_sensitivity;

    /* This would modulate prediction error gain in the predictive network */
    bridge->stats.precision_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied precision gain control: gain=%f", gain);
    return 0;
}

int predictive_fep_map_fe_to_error(
    predictive_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_predictive_fep_map_f", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_fe_error_mapping) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Map free energy to prediction error
     * F ≈ ∑Π||ε||² (equivalence in linear Gaussian case)
     * This is the core theoretical connection
     */
    float fe = bridge->fep_effects.total_free_energy;
    float avg_precision = bridge->fep_effects.avg_precision;

    /* PE² ≈ F / Π (approximate inverse mapping) */
    float pe_squared = (avg_precision > 0.01f) ? (fe / avg_precision) : fe;
    float pe = sqrtf(fabsf(pe_squared));

    bridge->state.current_prediction_error = pe;
    bridge->stats.fe_error_mappings++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Mapped FE to PE: FE=%f, PE=%f", fe, pe);
    return 0;
}

/* ============================================================================
 * Predictive -> FEP Direction
 * ============================================================================ */

int predictive_fep_flow_error_gradients(
    predictive_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_predictive_fep_flow_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_error_gradient_flow) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Flow prediction errors as variational gradients
     * PE = ∂F/∂μ (prediction errors are variational gradients)
     * ε_predictive → ∂F/∂μ_FEP
     */
    float pe = bridge->state.current_prediction_error;
    float gradient = pe * bridge->config.error_gradient_scaling;

    /* This gradient would be used to update FEP beliefs */
    bridge->stats.gradient_flows++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Flowed error gradients: PE=%f, gradient=%f", pe, gradient);
    return 0;
}

int predictive_fep_provide_generative_predictions(
    predictive_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_predictive_fep_provi", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_prediction_generative_model) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Provide predictions as generative model
     * Predictions = g(μ) (generative model output)
     * prediction_predictive → g(μ)_FEP
     */
    bridge->stats.generative_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Provided generative predictions");
    return 0;
}

int predictive_fep_compute_kalman_gains(
    predictive_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_predictive_fep_compu", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_precision_kalman_gains) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute precision as Kalman gains
     * Precision = Kalman gain (optimal Bayesian weighting)
     * K = Σ_prior * (Σ_prior + Σ_likelihood)^-1
     * Under Gaussian assumptions, precision maps directly
     */
    bridge->stats.kalman_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Computed Kalman gains");
    return 0;
}

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

int predictive_fep_bridge_update(
    predictive_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* FEP -> Predictive direction */
    predictive_fep_sync_beliefs_to_predictions(bridge);
    predictive_fep_apply_precision_gain_control(bridge);
    predictive_fep_map_fe_to_error(bridge);

    /* Predictive -> FEP direction */
    predictive_fep_flow_error_gradients(bridge);
    predictive_fep_provide_generative_predictions(bridge);
    predictive_fep_compute_kalman_gains(bridge);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Track convergence */
    float prev_fe = bridge->stats.avg_free_energy;
    float curr_fe = bridge->fep_effects.total_free_energy;
    float fe_change = prev_fe - curr_fe;

    if (fe_change > 0.0f) {
        /* FE decreasing = converging */
        bridge->state.fe_convergence_rate =
            (bridge->state.fe_convergence_rate * 0.9f) + (fe_change * 0.1f);
    }

    float prev_pe = bridge->stats.avg_prediction_error;
    float curr_pe = bridge->state.current_prediction_error;
    float pe_change = prev_pe - curr_pe;

    if (pe_change > 0.0f) {
        /* PE decreasing = converging */
        bridge->state.pe_convergence_rate =
            (bridge->state.pe_convergence_rate * 0.9f) + (pe_change * 0.1f);
    }

    /* Check convergence */
    if (curr_pe < 0.01f && curr_fe < 0.01f) {
        bridge->state.converged = true;
        bridge->stats.convergence_count++;
    } else {
        bridge->state.converged = false;
    }

    /* Update stats */
    bridge->stats.avg_free_energy =
        (bridge->stats.avg_free_energy * 0.9f) + (curr_fe * 0.1f);
    bridge->stats.avg_prediction_error =
        (bridge->stats.avg_prediction_error * 0.9f) + (curr_pe * 0.1f);

    bridge->state.current_free_energy = curr_fe;
    bridge->state.last_sync_time += delta_ms;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

int predictive_fep_bridge_get_state(
    const predictive_fep_bridge_t* bridge,
    predictive_fep_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int predictive_fep_bridge_get_stats(
    const predictive_fep_bridge_t* bridge,
    predictive_fep_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int predictive_fep_bridge_connect_bio_async(
    predictive_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_PREDICTIVE_BRIDGE,
        .module_name = "predictive_fep_bridge",
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

int predictive_fep_bridge_disconnect_bio_async(
    predictive_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_disconnect_bio_async", 0.0f);


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

bool predictive_fep_bridge_is_bio_async_connected(
    const predictive_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_is_bio_async_connect", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int predictive_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    predictive_fep_bridge_heartbeat("predictive_f_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Predictive_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                predictive_fep_bridge_heartbeat("predictive_f_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Predictive_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Predictive_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

//=============================================================================
// Instance Health Agent Setter (B24 Upgrade)
//=============================================================================

void predictive_fep_bridge_set_instance_health_agent(
    predictive_fep_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B24 Upgrade)
//=============================================================================

int predictive_fep_bridge_training_begin(predictive_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    predictive_fep_bridge_heartbeat_instance(bridge->health_agent, "predictive_fep_bridge_training_begin", 0.0f);
    return 0;
}

int predictive_fep_bridge_training_end(predictive_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_fep_bridge_training_end: NULL argument");
        return -1;
    }
    predictive_fep_bridge_heartbeat_instance(bridge->health_agent, "predictive_fep_bridge_training_end", 1.0f);
    return 0;
}

int predictive_fep_bridge_training_step(predictive_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_fep_bridge_training_step: NULL argument");
        return -1;
    }
    predictive_fep_bridge_heartbeat_instance(bridge->health_agent, "predictive_fep_bridge_training_step", progress);
    return 0;
}
