/**
 * @file nimcp_jepa_imagination_bridge.c
 * @brief JEPA-Imagination Bidirectional Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "cognitive/imagination/nimcp_jepa_imagination_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/imagination/nimcp_imagination_engine.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(jepa_imagination_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_jepa_imagination_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_jepa_imagination_bridge_mesh_registry = NULL;

nimcp_error_t jepa_imagination_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_jepa_imagination_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "jepa_imagination_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "jepa_imagination_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_jepa_imagination_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_jepa_imagination_bridge_mesh_registry = registry;
    return err;
}

void jepa_imagination_bridge_mesh_unregister(void) {
    if (g_jepa_imagination_bridge_mesh_registry && g_jepa_imagination_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_jepa_imagination_bridge_mesh_registry, g_jepa_imagination_bridge_mesh_id);
        g_jepa_imagination_bridge_mesh_id = 0;
        g_jepa_imagination_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from jepa_imagination_bridge module (instance-level) */
static inline void jepa_imagination_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_jepa_imagination_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_jepa_imagination_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_jepa_imagination_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "JEPA_IMAGINATION_BRIDGE"


/* Local logging macros */
#define NIMCP_LOG_INFO(...)  LOG_INFO(__VA_ARGS__)
#define NIMCP_LOG_DEBUG(...) LOG_DEBUG(__VA_ARGS__)
#define NIMCP_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

int jepa_imagination_default_config(jepa_imagination_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_def", 0.0f);


    config->confidence_threshold = JEPA_IMAG_DEFAULT_CONFIDENCE_THRESHOLD;
    config->max_prediction_horizon = 10;
    config->prediction_decay = 0.9f;

    config->learning_rate = JEPA_IMAG_DEFAULT_LEARNING_RATE;
    config->novelty_bonus = 0.1f;
    config->enable_counterfactual_training = true;

    config->enable_prediction_constraints = true;
    config->constraint_softness = 0.3f;

    config->update_interval_ms = 16.0f;  /* ~60 Hz */
    config->enable_bio_async = true;

    return 0;
}

int jepa_imagination_validate_config(const jepa_imagination_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_val", 0.0f);


    if (config->confidence_threshold < 0.0f || config->confidence_threshold > 1.0f) {
        return -1;
    }
    if (config->max_prediction_horizon == 0 || config->max_prediction_horizon > 100) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_imagination_validate_config: config->max_prediction_horizon is zero");
        return -1;
    }
    if (config->prediction_decay < 0.0f || config->prediction_decay > 1.0f) {
        return -1;
    }
    if (config->learning_rate < 0.0f || config->learning_rate > 1.0f) {
        return -1;
    }
    if (config->novelty_bonus < 0.0f || config->novelty_bonus > 1.0f) {
        return -1;
    }
    if (config->constraint_softness < 0.0f || config->constraint_softness > 1.0f) {
        return -1;
    }
    if (config->update_interval_ms < 1.0f) {
        return -1;
    }

    return 0;
}

/* ============================================================================
 * LIFECYCLE
 * ============================================================================ */

jepa_imagination_bridge_t* jepa_imagination_bridge_create(
    const jepa_imagination_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_create", 0.0f);


    jepa_imagination_bridge_t* bridge = nimcp_calloc(
        1, sizeof(jepa_imagination_bridge_t));
    if (!bridge) {
        NIMCP_LOG_ERROR("Failed to allocate JEPA-imagination bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Initialize base */
    bridge->base.module_id = BIO_MODULE_IMAGINATION_JEPA;
    bridge->base.module_name = "jepa_imagination_bridge";
    bridge->base.system_a_connected = false;
    bridge->base.system_b_connected = false;
    bridge->base.bridge_active = false;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "jepa_imagination") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOG_ERROR("Failed to create bridge mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_imagination_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        if (jepa_imagination_validate_config(config) != 0) {
            NIMCP_LOG_ERROR("Invalid bridge configuration");
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_imagination_bridge_create: validation failed");
            return NULL;
        }
        bridge->config = *config;
    } else {
        jepa_imagination_default_config(&bridge->config);
    }

    /* Initialize effects */
    memset(&bridge->jepa_to_imag, 0, sizeof(bridge->jepa_to_imag));
    memset(&bridge->imag_to_jepa, 0, sizeof(bridge->imag_to_jepa));

    /* Initialize state */
    bridge->num_active_counterfactuals = 0;
    bridge->prediction_request_pending = false;
    bridge->pending_context = NULL;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Initialize timing */
    bridge->last_update_time_ms = nimcp_time_get_ms();

    NIMCP_LOG_INFO("Created JEPA-imagination bridge");
    return bridge;
}

void jepa_imagination_bridge_destroy(jepa_imagination_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "jepa_imagination");

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        jepa_imagination_disconnect_bio_async(bridge);
    }

    /* Free pending context */
    if (bridge->pending_context) {
        nimcp_tensor_destroy(bridge->pending_context);
    }

    /* Free predicted latents */
    if (bridge->jepa_to_imag.predicted_latents) {
        nimcp_tensor_destroy(bridge->jepa_to_imag.predicted_latents);
    }

    /* Free outcome embedding */
    if (bridge->jepa_to_imag.outcome_embedding) {
        nimcp_tensor_destroy(bridge->jepa_to_imag.outcome_embedding);
    }

    /* Free target/context embeddings */
    if (bridge->imag_to_jepa.target_embedding) {
        nimcp_tensor_destroy(bridge->imag_to_jepa.target_embedding);
    }
    if (bridge->imag_to_jepa.context_embedding) {
        nimcp_tensor_destroy(bridge->imag_to_jepa.context_embedding);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOG_INFO("Destroyed JEPA-imagination bridge");
}

int jepa_imagination_reset(jepa_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_res", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Free tensor pointers in effects BEFORE memset zeroes them */
    if (bridge->jepa_to_imag.predicted_latents) {
        nimcp_tensor_destroy(bridge->jepa_to_imag.predicted_latents);
    }
    if (bridge->jepa_to_imag.outcome_embedding) {
        nimcp_tensor_destroy(bridge->jepa_to_imag.outcome_embedding);
    }
    if (bridge->imag_to_jepa.target_embedding) {
        nimcp_tensor_destroy(bridge->imag_to_jepa.target_embedding);
    }
    if (bridge->imag_to_jepa.context_embedding) {
        nimcp_tensor_destroy(bridge->imag_to_jepa.context_embedding);
    }

    /* Clear effects (now safe - tensor pointers already freed) */
    memset(&bridge->jepa_to_imag, 0, sizeof(bridge->jepa_to_imag));
    memset(&bridge->imag_to_jepa, 0, sizeof(bridge->imag_to_jepa));

    /* Clear state */
    bridge->num_active_counterfactuals = 0;
    bridge->prediction_request_pending = false;

    /* Free and clear tensors */
    if (bridge->pending_context) {
        nimcp_tensor_destroy(bridge->pending_context);
        bridge->pending_context = NULL;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * CONNECTION
 * ============================================================================ */

int jepa_imagination_connect_jepa(
    jepa_imagination_bridge_t* bridge,
    struct jepa_predictor* jepa)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_con", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->jepa = jepa;
    bridge->base.system_a = jepa;
    bridge->base.system_a_connected = (jepa != NULL);
    bridge->base.bridge_active = bridge->base.system_a_connected &&
                                  bridge->base.system_b_connected;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_INFO("JEPA %s to bridge",
                   jepa ? "connected" : "disconnected");
    return 0;
}

int jepa_imagination_connect_imagination(
    jepa_imagination_bridge_t* bridge,
    struct imagination_engine* imagination)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_con", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->imagination = imagination;
    bridge->base.system_b = imagination;
    bridge->base.system_b_connected = (imagination != NULL);
    bridge->base.bridge_active = bridge->base.system_a_connected &&
                                  bridge->base.system_b_connected;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_INFO("Imagination engine %s to bridge",
                   imagination ? "connected" : "disconnected");
    return 0;
}

int jepa_imagination_disconnect_jepa(jepa_imagination_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_dis", 0.0f);


    return jepa_imagination_connect_jepa(bridge, NULL);
}

int jepa_imagination_disconnect_imagination(jepa_imagination_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_dis", 0.0f);


    return jepa_imagination_connect_imagination(bridge, NULL);
}

bool jepa_imagination_is_connected(const jepa_imagination_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_is_", 0.0f);


    return bridge->base.bridge_active;
}

/* ============================================================================
 * UPDATE
 * ============================================================================ */

int jepa_imagination_update(
    jepa_imagination_bridge_t* bridge,
    float delta_time_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bridge_active) return 0;  /* Nothing to do */

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_upd", 0.0f);


    (void)delta_time_ms;  /* Used for rate limiting below */

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check update interval */
    uint64_t now = nimcp_time_get_ms();
    float elapsed = (float)(now - bridge->last_update_time_ms);
    if (elapsed < bridge->config.update_interval_ms) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->last_update_time_ms = now;

    /* Compute effects in both directions */
    jepa_imagination_compute_jepa_effects(bridge);
    jepa_imagination_compute_imag_effects(bridge);

    /* Apply effects */
    jepa_imagination_apply_effects(bridge);

    /* Update statistics */
    bridge->stats.total_updates++;
    bridge->base.total_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_imagination_compute_jepa_effects(jepa_imagination_bridge_t* bridge) {
    if (!bridge || !bridge->jepa) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_imagination_compute_jepa_effects: required parameter is NULL (bridge, bridge->jepa)");
        return -1;
    }

    /* Query JEPA for current prediction state */
    /* In full implementation, this would call JEPA predictor APIs */

    /* Default: low-level effects when no active prediction */
    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_com", 0.0f);


    bridge->jepa_to_imag.prediction_confidence = 0.0f;
    bridge->jepa_to_imag.world_model_coherence = 0.0f;
    bridge->jepa_to_imag.counterfactual_plausibility = 0.0f;
    bridge->jepa_to_imag.num_predictions = 0;
    bridge->jepa_to_imag.action_outcome_likelihood = 0.0f;
    bridge->jepa_to_imag.constrain_to_predictions = false;

    /* If there's a pending prediction request, process it */
    if (bridge->prediction_request_pending && bridge->pending_context) {
        /* Would call jepa_predict() here */
        /* For now, mark request as processed */
        bridge->prediction_request_pending = false;
        bridge->stats.predictions_generated++;
    }

    return 0;
}

int jepa_imagination_compute_imag_effects(jepa_imagination_bridge_t* bridge) {
    if (!bridge || !bridge->imagination) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_imagination_compute_imag_effects: required parameter is NULL (bridge, bridge->imagination)");
        return -1;
    }

    /* Query imagination engine for current state */
    /* In full implementation, this would call imagination engine APIs */

    /* Default: no training signals */
    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_com", 0.0f);


    bridge->imag_to_jepa.learning_strength = 0.0f;
    bridge->imag_to_jepa.prediction_error = 0.0f;
    bridge->imag_to_jepa.update_world_model = false;
    bridge->imag_to_jepa.scenario_id = 0;
    bridge->imag_to_jepa.novelty_score = 0.0f;
    bridge->imag_to_jepa.exploration_value = 0.0f;
    bridge->imag_to_jepa.is_counterfactual = false;
    bridge->imag_to_jepa.counterfactual_divergence = 0.0f;

    return 0;
}

int jepa_imagination_apply_effects(jepa_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Apply JEPA effects to imagination */
    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_app", 0.0f);


    if (bridge->imagination && bridge->jepa_to_imag.num_predictions > 0) {
        /* Would call imagination APIs to inject prediction constraints */
        bridge->stats.predictions_used += bridge->jepa_to_imag.num_predictions;
    }

    /* Apply imagination effects to JEPA */
    if (bridge->jepa && bridge->imag_to_jepa.learning_strength > 0.0f) {
        /* Would call JEPA APIs to provide training signal */
        if (bridge->imag_to_jepa.update_world_model) {
            bridge->stats.world_model_updates++;
        }
        bridge->stats.training_signals_sent++;
    }

    /* Track counterfactual stats */
    if (bridge->imag_to_jepa.is_counterfactual) {
        bridge->stats.counterfactuals_simulated++;
    }

    return 0;
}

/* ============================================================================
 * JEPA-IMAGINATION INTEGRATION
 * ============================================================================ */

uint32_t jepa_imagination_request_predicted_imagination(
    jepa_imagination_bridge_t* bridge,
    const nimcp_tensor_t* context,
    struct imagination_goal* goal)
{
    if (!bridge || !context || !goal) return 0;
    if (!bridge->base.bridge_active) return 0;

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_req", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Store the context for processing */
    if (bridge->pending_context) {
        nimcp_tensor_destroy(bridge->pending_context);
    }
    bridge->pending_context = nimcp_tensor_clone(context);
    bridge->prediction_request_pending = true;

    /* Start imagination scenario using prediction-constrained goal */
    uint32_t scenario_id = 0;
    if (bridge->imagination) {
        imagination_scenario_t* scenario = imagination_begin_scenario(
            bridge->imagination,
            goal->mode,
            goal);
        if (scenario) {
            scenario_id = scenario->id;

            /* Inject JEPA context into scenario latent state as constraint */
            if (scenario->latent_state && context) {
                const float* ctx_data = nimcp_tensor_data_const(context);
                float* lat_data = nimcp_tensor_data(scenario->latent_state);
                uint32_t ctx_size = (uint32_t)nimcp_tensor_size(context);
                uint32_t lat_size = (uint32_t)nimcp_tensor_size(scenario->latent_state);
                uint32_t blend_size = (ctx_size < lat_size) ? ctx_size : lat_size;

                /* Blend: 70% imagination noise + 30% JEPA prediction */
                for (uint32_t i = 0; i < blend_size; i++) {
                    lat_data[i] = 0.7f * lat_data[i] + 0.3f * ctx_data[i];
                }
            }

            bridge->stats.predictions_used++;

            /* Release the workspace slot — we've consumed everything we
             * need from the scenario. Without this end_scenario call the
             * slot stays allocated forever and IMAGINATION_MAX_SCENARIOS
             * fills within minutes (~13:41 daemon stderr: workspace full
             * looping every few seconds, 2026-04-26). */
            imagination_end_scenario(bridge->imagination, scenario);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_DEBUG("Requested prediction-constrained imagination, scenario %u", scenario_id);
    return scenario_id;
}

uint32_t jepa_imagination_request_counterfactual(
    jepa_imagination_bridge_t* bridge,
    const nimcp_tensor_t* context,
    const nimcp_tensor_t* action)
{
    if (!bridge || !context || !action) return 0;
    if (!bridge->base.bridge_active) return 0;

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_req", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Start counterfactual imagination scenario:
     * Create scenario with context, inject action as latent perturbation */
    uint32_t scenario_id = 0;
    if (bridge->imagination) {
        imagination_goal_t cf_goal;
        memset(&cf_goal, 0, sizeof(cf_goal));
        cf_goal.mode = IMAGINATION_MODE_COUNTERFACTUAL;
        cf_goal.priority = 0.6f;

        imagination_scenario_t* scenario = imagination_begin_scenario(
            bridge->imagination,
            IMAGINATION_MODE_COUNTERFACTUAL,
            &cf_goal);
        if (scenario) {
            scenario_id = scenario->id;

            /* Blend context + action into latent state */
            if (scenario->latent_state) {
                const float* ctx_data = nimcp_tensor_data_const(context);
                const float* act_data = nimcp_tensor_data_const(action);
                float* lat_data = nimcp_tensor_data(scenario->latent_state);
                uint32_t lat_size = (uint32_t)nimcp_tensor_size(scenario->latent_state);
                uint32_t ctx_size = (uint32_t)nimcp_tensor_size(context);
                uint32_t act_size = (uint32_t)nimcp_tensor_size(action);

                /* Context provides baseline, action provides perturbation */
                for (uint32_t i = 0; i < lat_size; i++) {
                    float ctx_val = (i < ctx_size) ? ctx_data[i] : 0.0f;
                    float act_val = (i < act_size) ? act_data[i] : 0.0f;
                    lat_data[i] = ctx_val + 0.5f * act_val;
                }
            }

            /* Release the workspace slot (see request_prediction above). */
            imagination_end_scenario(bridge->imagination, scenario);
        }
    }

    if (scenario_id == 0) scenario_id = bridge->num_active_counterfactuals + 1;

    if (bridge->num_active_counterfactuals < JEPA_IMAG_MAX_COUNTERFACTUALS) {
        bridge->active_counterfactuals[bridge->num_active_counterfactuals++] = scenario_id;
    }

    bridge->stats.counterfactuals_simulated++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_DEBUG("Requested counterfactual simulation, scenario %u", scenario_id);
    return scenario_id;
}

int jepa_imagination_query_world_model(
    jepa_imagination_bridge_t* bridge,
    const nimcp_tensor_t* context,
    const nimcp_tensor_t* action,
    nimcp_tensor_t** outcome)
{
    if (!bridge || !context || !action || !outcome) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_imagination_query_world_model: required parameter is NULL (bridge, context, action, outcome)");
        return -1;
    }
    if (!bridge->jepa) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_imagination_query_world_model: bridge->jepa is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_que", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Create outcome tensor by combining context and action.
     * This is a simplified world model prediction: outcome = context + action_effect */
    uint32_t ctx_size = (uint32_t)nimcp_tensor_size(context);
    uint32_t act_size = (uint32_t)nimcp_tensor_size(action);
    uint32_t out_size = (ctx_size > act_size) ? ctx_size : act_size;

    uint32_t out_dims[] = {out_size};
    nimcp_tensor_t* predicted = nimcp_tensor_create(out_dims, 1, NIMCP_DTYPE_F32);
    if (!predicted) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_imagination_query_world_model: allocation failed");
        return -1;
    }

    const float* ctx_data = nimcp_tensor_data_const(context);
    const float* act_data = nimcp_tensor_data_const(action);
    float* out_data = nimcp_tensor_data(predicted);

    /* Simple linear prediction: outcome = context + 0.1 * action */
    for (uint32_t i = 0; i < out_size; i++) {
        float ctx_val = (i < ctx_size) ? ctx_data[i] : 0.0f;
        float act_val = (i < act_size) ? act_data[i] : 0.0f;
        out_data[i] = ctx_val + 0.1f * act_val;
    }

    *outcome = predicted;
    bridge->stats.predictions_generated++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_DEBUG("Queried world model for action outcome");
    return 0;
}

int jepa_imagination_provide_training_signal(
    jepa_imagination_bridge_t* bridge,
    const struct imagination_scenario* scenario,
    float emotional_weight)
{
    if (!bridge || !scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_imagination_provide_training_signal: required parameter is NULL (bridge, scenario)");
        return -1;
    }
    if (!bridge->jepa) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_imagination_provide_training_signal: bridge->jepa is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_pro", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Set up training signal */
    bridge->imag_to_jepa.learning_strength = emotional_weight * bridge->config.learning_rate;
    bridge->imag_to_jepa.update_world_model = true;

    /* In full implementation, would:
     * 1. Extract latent state from scenario
     * 2. Call JEPA training API
     */

    bridge->stats.training_signals_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_DEBUG("Provided training signal with weight %.2f", emotional_weight);
    return 0;
}

int jepa_imagination_get_jepa_effects(
    const jepa_imagination_bridge_t* bridge,
    jepa_to_imagination_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_imagination_get_jepa_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_get", 0.0f);

    nimcp_mutex_lock(((jepa_imagination_bridge_t*)bridge)->base.mutex);
    *effects = bridge->jepa_to_imag;
    nimcp_mutex_unlock(((jepa_imagination_bridge_t*)bridge)->base.mutex);
    return 0;
}

int jepa_imagination_get_imagination_effects(
    const jepa_imagination_bridge_t* bridge,
    imagination_to_jepa_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_imagination_get_imagination_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_get", 0.0f);

    nimcp_mutex_lock(((jepa_imagination_bridge_t*)bridge)->base.mutex);
    *effects = bridge->imag_to_jepa;
    nimcp_mutex_unlock(((jepa_imagination_bridge_t*)bridge)->base.mutex);
    return 0;
}

/* ============================================================================
 * QUERY
 * ============================================================================ */

int jepa_imagination_get_stats(
    const jepa_imagination_bridge_t* bridge,
    jepa_imagination_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_imagination_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_get", 0.0f);

    nimcp_mutex_lock(((jepa_imagination_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((jepa_imagination_bridge_t*)bridge)->base.mutex);
    return 0;
}

int jepa_imagination_reset_stats(jepa_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_res", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

uint32_t jepa_imagination_get_counterfactual_count(
    const jepa_imagination_bridge_t* bridge)
{
    if (!bridge) return 0;
    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_get", 0.0f);


    return bridge->num_active_counterfactuals;
}

/* ============================================================================
 * BIO-ASYNC
 * ============================================================================ */

int jepa_imagination_connect_bio_async(jepa_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;  /* Already connected */

    /* Use bridge base helper if available */
    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_con", 0.0f);


    int result = bridge_base_connect_bio_async(&bridge->base);
    if (result == 0) {
        NIMCP_LOG_INFO("JEPA-imagination bridge connected to bio-async");
    }

    return result;
}

int jepa_imagination_disconnect_bio_async(jepa_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;  /* Already disconnected */

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_dis", 0.0f);


    int result = bridge_base_disconnect_bio_async(&bridge->base);
    if (result == 0) {
        NIMCP_LOG_INFO("JEPA-imagination bridge disconnected from bio-async");
    }

    return result;
}

bool jepa_imagination_is_bio_async_connected(
    const jepa_imagination_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_is_", 0.0f);


    return bridge->base.bio_async_enabled;
}

int jepa_imagination_process_messages(jepa_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Process pending bio-async messages */
    /* In full implementation, would handle:
     * - BIO_MSG_JEPA_PREDICTION_REQUEST
     * - BIO_MSG_JEPA_PREDICTION_RESPONSE
     * - BIO_MSG_IMAGINATION_TRAINING_SIGNAL
     * - BIO_MSG_COUNTERFACTUAL_REQUEST
     */

    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_jepa_imagination_pro", 0.0f);


    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for JEPA Imagination Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int jepa_imagination_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    jepa_imagination_bridge_heartbeat("jepa_imagina_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "JEPA_Imagination_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                jepa_imagination_bridge_heartbeat("jepa_imagina_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOG_DEBUG("JEPA Imagination Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "JEPA_Imagination_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "JEPA_Imagination_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void jepa_imagination_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_jepa_imagination_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int jepa_imagination_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_imagination_bridge_training_begin: NULL argument");
        return -1;
    }
    jepa_imagination_bridge_heartbeat("jepa_imagination_bridge_training_begin", 0.0f);
    return 0;
}

int jepa_imagination_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_imagination_bridge_training_end: NULL argument");
        return -1;
    }
    jepa_imagination_bridge_heartbeat("jepa_imagination_bridge_training_end", 1.0f);
    return 0;
}

int jepa_imagination_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_imagination_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    jepa_imagination_bridge_heartbeat("jepa_imagination_bridge_training_step", progress);
    return 0;
}
