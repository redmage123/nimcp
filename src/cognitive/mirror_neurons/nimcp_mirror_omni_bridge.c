/**
 * @file nimcp_mirror_omni_bridge.c
 * @brief Mirror Neurons - Omnidirectional Cognitive Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "cognitive/mirror_neurons/nimcp_mirror_omni_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include <math.h>
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mirror_omni_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mirror_omni_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mirror_omni_bridge_mesh_registry = NULL;

nimcp_error_t mirror_omni_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_mirror_omni_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mirror_omni_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mirror_omni_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mirror_omni_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mirror_omni_bridge_mesh_registry = registry;
    return err;
}

void mirror_omni_bridge_mesh_unregister(void) {
    if (g_mirror_omni_bridge_mesh_registry && g_mirror_omni_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mirror_omni_bridge_mesh_registry, g_mirror_omni_bridge_mesh_id);
        g_mirror_omni_bridge_mesh_id = 0;
        g_mirror_omni_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from mirror_omni_bridge module (instance-level) */
static inline void mirror_omni_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mirror_omni_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mirror_omni_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mirror_omni_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(mirror_omni_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/* clamp_f replaced by nimcp_myelin_clamp (B22 Upgrade) */

static inline float normalize_weight(float w1, float w2, float w3, float target) {
    float sum = w1 + w2 + w3;
    if (sum <= 0.0f) return 0.333f;
    return target / sum;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int mirror_omni_bridge_default_config(mirror_omni_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Mirror -> World Model coupling */
    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__default_config", 0.0f);


    config->state_coupling_rate = MIRROR_OMNI_STATE_COUPLING_RATE;
    config->state_confidence_threshold = MIRROR_OMNI_STATE_CONFIDENCE_MIN;
    config->enable_hierarchical_states = true;

    /* Mirror -> Active Inference coupling */
    config->prior_coupling_rate = MIRROR_OMNI_PRIOR_COUPLING_RATE;
    config->prior_decay_rate = MIRROR_OMNI_PRIOR_DECAY_RATE;
    config->precision_gain_min = MIRROR_OMNI_PRECISION_GAIN_MIN;
    config->precision_gain_max = MIRROR_OMNI_PRECISION_GAIN_MAX;
    config->enable_confidence_precision = true;

    /* Active Inference -> Mirror coupling */
    config->imitation_threshold = MIRROR_OMNI_IMITATION_THRESHOLD;
    config->policy_weight_min = MIRROR_OMNI_POLICY_WEIGHT_MIN;
    config->enable_imitation_queries = true;

    /* World Model -> Mirror coupling */
    config->counterfactual_horizon = MIRROR_OMNI_CF_HORIZON_DEFAULT;
    config->divergence_threshold = MIRROR_OMNI_CF_DIVERGENCE_MAX;
    config->enable_counterfactual = true;

    /* General settings */
    config->enable_omnidirectional = true;
    config->enable_bio_async = true;

    return 0;
}

mirror_omni_bridge_t* mirror_omni_bridge_create(
    const mirror_omni_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__create", 0.0f);


    mirror_omni_bridge_t* bridge = (mirror_omni_bridge_t*)nimcp_calloc(
        1, sizeof(mirror_omni_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate mirror-omni bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    mirror_omni_config_t default_cfg;
    if (!config) {
        mirror_omni_bridge_default_config(&default_cfg);
        config = &default_cfg;
    }
    bridge->config = *config;

    /* Initialize base */
    bridge->base.module_id = BIO_MODULE_MIRROR_OMNI_BRIDGE;
    bridge->base.module_name = "mirror_omni_bridge";
    if (bridge_base_init(&bridge->base, 0, "mirror_omni") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for mirror-omni bridge");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_omni_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize agent states cache */
    bridge->agent_states_capacity = 16;
    bridge->agent_states = (mirror_omni_agent_state_t*)nimcp_calloc(
        bridge->agent_states_capacity, sizeof(mirror_omni_agent_state_t));
    if (!bridge->agent_states) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_omni_bridge_create: bridge->agent_states is NULL");
        return NULL;
    }
    bridge->num_agent_states = 0;

    /* Initialize action priors cache */
    bridge->action_priors_capacity = 64;
    bridge->action_priors = (mirror_omni_action_prior_t*)nimcp_calloc(
        bridge->action_priors_capacity, sizeof(mirror_omni_action_prior_t));
    if (!bridge->action_priors) {
        nimcp_free(bridge->agent_states);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_omni_bridge_create: bridge->action_priors allocation failed");
        return NULL;
    }
    bridge->num_action_priors = 0;

    /* Initialize effects */
    memset(&bridge->effects, 0, sizeof(mirror_omni_effects_t));
    bridge->effects.precision_modulation = 1.0f;

    /* Initialize state */
    memset(&bridge->state, 0, sizeof(mirror_omni_state_t));
    bridge->state.forward_inference_weight = 0.5f;
    bridge->state.backward_inference_weight = 0.25f;
    bridge->state.lateral_inference_weight = 0.25f;

    /* Initialize stats */
    memset(&bridge->stats, 0, sizeof(mirror_omni_stats_t));

    NIMCP_LOGGING_INFO("Mirror-omni bridge created");
    return bridge;
}

void mirror_omni_bridge_destroy(mirror_omni_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async first */
    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        mirror_omni_bridge_disconnect_bio_async(bridge);
    }

    /* Free agent states */
    if (bridge->agent_states) {
        for (uint32_t i = 0; i < bridge->num_agent_states; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_agent_states > 256) {
                mirror_omni_bridge_heartbeat("mirror_omni__loop",
                                 (float)(i + 1) / (float)bridge->num_agent_states);
            }

            if (bridge->agent_states[i].predicted_state) {
                nimcp_free(bridge->agent_states[i].predicted_state);
            }
        }
        nimcp_free(bridge->agent_states);
    }

    /* Free action priors */
    if (bridge->action_priors) {
        nimcp_free(bridge->action_priors);
    }

    /* Cleanup base */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Mirror-omni bridge destroyed");
}

int mirror_omni_bridge_reset(mirror_omni_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Clear agent states (keep capacity) */
    for (uint32_t i = 0; i < bridge->num_agent_states; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_agent_states > 256) {
            mirror_omni_bridge_heartbeat("mirror_omni__loop",
                             (float)(i + 1) / (float)bridge->num_agent_states);
        }

        if (bridge->agent_states[i].predicted_state) {
            nimcp_free(bridge->agent_states[i].predicted_state);
            bridge->agent_states[i].predicted_state = NULL;
        }
    }
    bridge->num_agent_states = 0;

    /* Clear action priors (keep capacity) */
    bridge->num_action_priors = 0;

    /* Reset effects */
    memset(&bridge->effects, 0, sizeof(mirror_omni_effects_t));
    bridge->effects.precision_modulation = 1.0f;

    /* Reset state (keep connections) */
    bool mirror_conn = bridge->state.mirror_connected;
    bool wm_conn = bridge->state.world_model_connected;
    bool ai_conn = bridge->state.active_inference_connected;
    memset(&bridge->state, 0, sizeof(mirror_omni_state_t));
    bridge->state.mirror_connected = mirror_conn;
    bridge->state.world_model_connected = wm_conn;
    bridge->state.active_inference_connected = ai_conn;
    bridge->state.forward_inference_weight = 0.5f;
    bridge->state.backward_inference_weight = 0.25f;
    bridge->state.lateral_inference_weight = 0.25f;

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(mirror_omni_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

int mirror_omni_bridge_connect_mirror(
    mirror_omni_bridge_t* bridge,
    mirror_neurons_t mirror
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Allow NULL to disconnect */

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__connect_mirror", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->mirror_system = mirror;
    bridge->state.mirror_connected = (mirror != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    if (mirror) {
        NIMCP_LOGGING_INFO("Mirror-omni bridge connected to mirror neuron system");
    } else {
        NIMCP_LOGGING_INFO("Mirror-omni bridge disconnected from mirror neuron system");
    }
    return 0;
}

int mirror_omni_bridge_connect_world_model(
    mirror_omni_bridge_t* bridge,
    omni_world_model_t* world_model
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Allow NULL to disconnect */

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__connect_world_model", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->world_model = world_model;
    bridge->state.world_model_connected = (world_model != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    if (world_model) {
        NIMCP_LOGGING_INFO("Mirror-omni bridge connected to world model");
    } else {
        NIMCP_LOGGING_INFO("Mirror-omni bridge disconnected from world model");
    }
    return 0;
}

int mirror_omni_bridge_connect_active_inference(
    mirror_omni_bridge_t* bridge,
    omni_active_inference_t* active_inference
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Allow NULL to disconnect */

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__connect_active_infer", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->active_inference = active_inference;
    bridge->state.active_inference_connected = (active_inference != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    if (active_inference) {
        NIMCP_LOGGING_INFO("Mirror-omni bridge connected to active inference");
    } else {
        NIMCP_LOGGING_INFO("Mirror-omni bridge disconnected from active inference");
    }
    return 0;
}

bool mirror_omni_bridge_is_fully_connected(
    const mirror_omni_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__is_fully_connected", 0.0f);


    return bridge->state.mirror_connected &&
           bridge->state.world_model_connected &&
           bridge->state.active_inference_connected;
}

/* ============================================================================
 * Mirror -> World Model Implementation
 * ============================================================================ */

int mirror_omni_feed_agent_state(
    mirror_omni_bridge_t* bridge,
    uint32_t agent_id
) {
    if (!bridge || !bridge->mirror_system || !bridge->world_model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_feed_agent_state: bridge, mirror_system, or world_model is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_feed_age", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get mirror neuron activation for this agent's actions */
    mirror_neuron_stats_t mirror_stats;
    if (!mirror_neurons_get_stats(bridge->mirror_system, &mirror_stats)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_feed_agent_state: mirror_neurons_get_stats is NULL");
        return -1;
    }

    /* Check if we have recent observations */
    if (!mirror_neurons_has_recent_observations(bridge->mirror_system)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* No observations to process */
    }

    /* Find or create agent state entry */
    mirror_omni_agent_state_t* agent_state = NULL;
    for (uint32_t i = 0; i < bridge->num_agent_states; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_agent_states > 256) {
            mirror_omni_bridge_heartbeat("mirror_omni__loop",
                             (float)(i + 1) / (float)bridge->num_agent_states);
        }

        if (bridge->agent_states[i].agent_id == agent_id) {
            agent_state = &bridge->agent_states[i];
            break;
        }
    }

    if (!agent_state && bridge->num_agent_states < bridge->agent_states_capacity) {
        agent_state = &bridge->agent_states[bridge->num_agent_states++];
        memset(agent_state, 0, sizeof(mirror_omni_agent_state_t));
        agent_state->agent_id = agent_id;
    }

    if (agent_state) {
        /* Use mirror neuron activations to estimate agent state */
        float activations[32];
        uint32_t num_activations = 0;
        mirror_neurons_get_all_activations(bridge->mirror_system,
                                            activations, 32, &num_activations);

        /* Compute confidence based on activation strength */
        float max_activation = 0.0f;
        for (uint32_t i = 0; i < num_activations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_activations > 256) {
                mirror_omni_bridge_heartbeat("mirror_omni__loop",
                                 (float)(i + 1) / (float)num_activations);
            }

            if (activations[i] > max_activation) {
                max_activation = activations[i];
            }
        }

        agent_state->confidence = max_activation;

        /* Only update world model if confidence above threshold */
        if (agent_state->confidence >= bridge->config.state_confidence_threshold) {
            /* Create state representation for world model */
            omni_wm_state_t* wm_state = omni_wm_state_create(num_activations);
            if (wm_state) {
                for (uint32_t i = 0; i < num_activations && i < wm_state->dim; i++) {
                    wm_state->values[i] = activations[i];
                }
                wm_state->uncertainty = 1.0f - agent_state->confidence;

                /* Update world model with agent state */
                omni_wm_set_state(bridge->world_model, wm_state);

                omni_wm_state_destroy(wm_state);

                bridge->state.state_predictions_made++;
                bridge->stats.total_state_predictions++;
            }
        }

        bridge->effects.agent_state_prediction_strength = agent_state->confidence;
        bridge->effects.agents_tracked = bridge->num_agent_states;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_omni_update_state_transitions(
    mirror_omni_bridge_t* bridge
) {
    if (!bridge || !bridge->mirror_system || !bridge->world_model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_update_state_transitions: bridge, mirror_system, or world_model is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_update_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get recent action observations from mirror neurons */
    mirror_neuron_stats_t stats;
    if (!mirror_neurons_get_stats(bridge->mirror_system, &stats)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_update_state_transitions: mirror_neurons_get_stats is NULL");
        return -1;
    }

    /* Update world model with observation-derived transition priors */
    float coupling_rate = bridge->config.state_coupling_rate;
    bridge->effects.state_update_magnitude = stats.avg_match_quality * coupling_rate;

    /* Track observed agents from mirror neuron activations */
    if (stats.num_observed_agents > 0 || stats.num_active_neurons > 0) {
        bridge->state.observed_agents = stats.num_observed_agents > 0 ?
            stats.num_observed_agents : stats.num_active_neurons;
        bridge->state.active_mirror_neurons = stats.num_active_neurons;
        bridge->state.max_observation_activation = stats.avg_match_quality;
    }

    /* Make state predictions using world model for each observed agent */
    if (bridge->state.observed_agents > 0 && bridge->world_model) {
        /* Predict state transitions for tracked agents */
        bridge->state.state_predictions_made++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_omni_get_agent_state(
    const mirror_omni_bridge_t* bridge,
    uint32_t agent_id,
    mirror_omni_agent_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_get_agent_state: bridge or state is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_get_agen", 0.0f);


    for (uint32_t i = 0; i < bridge->num_agent_states; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_agent_states > 256) {
            mirror_omni_bridge_heartbeat("mirror_omni__loop",
                             (float)(i + 1) / (float)bridge->num_agent_states);
        }

        if (bridge->agent_states[i].agent_id == agent_id) {
            *state = bridge->agent_states[i];
            return 0;
        }
    }

    return -1;  /* Agent not found */
}

/* ============================================================================
 * Mirror -> Active Inference Implementation
 * ============================================================================ */

int mirror_omni_provide_action_priors(
    mirror_omni_bridge_t* bridge
) {
    if (!bridge || !bridge->mirror_system || !bridge->active_inference) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_provide_action_priors: bridge, mirror_system, or active_inference is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_provide_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get mirror neuron statistics */
    mirror_neuron_stats_t stats;
    if (!mirror_neurons_get_stats(bridge->mirror_system, &stats)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_provide_action_priors: mirror_neurons_get_stats is NULL");
        return -1;
    }

    /* Decay existing priors */
    float decay = 1.0f - bridge->config.prior_decay_rate;
    for (uint32_t i = 0; i < bridge->num_action_priors; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_action_priors > 256) {
            mirror_omni_bridge_heartbeat("mirror_omni__loop",
                             (float)(i + 1) / (float)bridge->num_action_priors);
        }

        bridge->action_priors[i].prior_probability *= decay;
        bridge->action_priors[i].recency_weight *= decay;
    }

    /* Update priors based on observed actions */
    float total_prior = 0.0f;
    for (uint32_t i = 0; i < bridge->num_action_priors; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_action_priors > 256) {
            mirror_omni_bridge_heartbeat("mirror_omni__loop",
                             (float)(i + 1) / (float)bridge->num_action_priors);
        }

        /* Get activation for this action from mirror neurons */
        float activation = mirror_neurons_get_activation(
            bridge->mirror_system, bridge->action_priors[i].action_id);

        if (activation > 0.0f) {
            float coupling = bridge->config.prior_coupling_rate;
            bridge->action_priors[i].prior_probability =
                bridge->action_priors[i].prior_probability * (1.0f - coupling) +
                activation * coupling;
            bridge->action_priors[i].observation_count += activation;
            bridge->action_priors[i].recency_weight = 1.0f;
        }

        total_prior += bridge->action_priors[i].prior_probability;
    }

    /* Normalize priors */
    if (total_prior > 0.0f) {
        for (uint32_t i = 0; i < bridge->num_action_priors; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_action_priors > 256) {
                mirror_omni_bridge_heartbeat("mirror_omni__loop",
                                 (float)(i + 1) / (float)bridge->num_action_priors);
            }

            bridge->action_priors[i].prior_probability /= total_prior;
        }
    }

    bridge->state.action_priors_generated++;
    bridge->stats.total_prior_updates++;

    /* Update effects */
    bridge->effects.action_prior_strength = stats.avg_match_quality;
    bridge->effects.priors_active = bridge->num_action_priors;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_omni_modulate_precision(
    mirror_omni_bridge_t* bridge
) {
    if (!bridge || !bridge->mirror_system || !bridge->active_inference) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_modulate_precision: bridge, mirror_system, or active_inference is NULL");
        return -1;
    }
    if (!bridge->config.enable_confidence_precision) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_modulate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get average mirror neuron confidence */
    float avg_confidence = bridge->state.avg_prediction_confidence;

    /* Map confidence to precision gain */
    float precision_gain = nimcp_myelin_clamp(
        avg_confidence * (bridge->config.precision_gain_max - bridge->config.precision_gain_min)
            + bridge->config.precision_gain_min,
        bridge->config.precision_gain_min,
        bridge->config.precision_gain_max
    );

    bridge->effects.precision_modulation = precision_gain;

    /* Apply to active inference config */
    if (bridge->active_inference && bridge->active_inference->config.use_precision_context) {
        /* Modulate policy precision based on mirror confidence */
        bridge->active_inference->config.policy_precision *= precision_gain;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_omni_get_action_prior(
    const mirror_omni_bridge_t* bridge,
    uint32_t action_id,
    mirror_omni_action_prior_t* prior
) {
    if (!bridge || !prior) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_get_action_prior: bridge or prior is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_get_acti", 0.0f);


    for (uint32_t i = 0; i < bridge->num_action_priors; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_action_priors > 256) {
            mirror_omni_bridge_heartbeat("mirror_omni__loop",
                             (float)(i + 1) / (float)bridge->num_action_priors);
        }

        if (bridge->action_priors[i].action_id == action_id) {
            *prior = bridge->action_priors[i];
            return 0;
        }
    }

    /* Not found - return default prior */
    memset(prior, 0, sizeof(mirror_omni_action_prior_t));
    prior->action_id = action_id;
    prior->prior_probability = 1.0f / 64.0f;  /* Uniform prior */
    return 0;
}

/* ============================================================================
 * Active Inference -> Mirror Implementation
 * ============================================================================ */

int mirror_omni_evaluate_imitation_cost(
    mirror_omni_bridge_t* bridge,
    uint32_t action_id,
    mirror_omni_imitation_cost_t* cost
) {
    if (!bridge || !cost) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_evaluate_imitation_cost: bridge or cost is NULL");
        return -1;
    }
    if (!bridge->config.enable_imitation_queries) return -1;

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_evaluate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    memset(cost, 0, sizeof(mirror_omni_imitation_cost_t));
    cost->action_id = action_id;

    if (bridge->mirror_system) {
        /* Get activation record for this action */
        mirror_activation_t activation;
        if (mirror_neurons_get_activation_record(bridge->mirror_system,
                                                  action_id, &activation)) {
            /* Motor cost based on observation-execution gap */
            cost->motor_cost = 1.0f - activation.association_strength;

            /* Learning cost inversely related to observation count */
            float obs_factor = (float)activation.observation_count / 10.0f;
            cost->learning_cost = 1.0f / (1.0f + obs_factor);

            /* Expected reward based on observation success */
            cost->expected_reward = activation.association_strength * 0.8f;

            /* Risk based on execution variance */
            float exec_factor = (float)activation.execution_count / 10.0f;
            cost->risk = 1.0f / (1.0f + exec_factor);

        } else {
            /* Unknown action - high cost */
            cost->motor_cost = 1.0f;
            cost->learning_cost = 1.0f;
            cost->expected_reward = 0.0f;
            cost->risk = 1.0f;
        }
    } else {
        /* No mirror system - cannot evaluate */
        cost->result = MIRROR_OMNI_IMITATE_UNKNOWN;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Compute total cost */
    cost->total_cost = cost->motor_cost * 0.3f +
                       cost->learning_cost * 0.3f +
                       cost->risk * 0.2f -
                       cost->expected_reward * 0.2f;

    /* Determine result */
    if (cost->total_cost < bridge->config.imitation_threshold) {
        cost->result = MIRROR_OMNI_IMITATE_SUCCESS;
    } else if (cost->total_cost < 0.9f) {
        cost->result = MIRROR_OMNI_IMITATE_COSTLY;
    } else {
        cost->result = MIRROR_OMNI_IMITATE_INFEASIBLE;
    }

    bridge->state.imitation_queries_handled++;
    bridge->stats.total_imitation_queries++;
    bridge->effects.avg_imitation_cost =
        (bridge->effects.avg_imitation_cost * 0.95f) + (cost->total_cost * 0.05f);
    bridge->stats.avg_imitation_cost = bridge->effects.avg_imitation_cost;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_omni_query_goal_actions(
    mirror_omni_bridge_t* bridge,
    uint32_t goal_id,
    uint32_t* actions,
    uint32_t max_actions,
    uint32_t* num_actions
) {
    if (!bridge || !actions || !num_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_query_goal_actions: bridge, actions, or num_actions is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_query_go", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    *num_actions = 0;

    if (bridge->mirror_system) {
        /* Query mirror hierarchy for goal-motor bindings */
        /* Simplified: return actions with high association strength */
        for (uint32_t i = 0; i < bridge->num_action_priors && *num_actions < max_actions; i++) {
            mirror_activation_t activation;
            if (mirror_neurons_get_activation_record(bridge->mirror_system,
                                                      bridge->action_priors[i].action_id,
                                                      &activation)) {
                if (activation.association_strength > 0.5f) {
                    actions[*num_actions] = bridge->action_priors[i].action_id;
                    (*num_actions)++;
                }
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * World Model -> Mirror Implementation
 * ============================================================================ */

int mirror_omni_simulate_counterfactual(
    mirror_omni_bridge_t* bridge,
    uint32_t action_id,
    uint32_t horizon,
    mirror_omni_cf_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_simulate_counterfactual: bridge or result is NULL");
        return -1;
    }
    if (!bridge->config.enable_counterfactual) return -1;

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_simulate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    memset(result, 0, sizeof(mirror_omni_cf_result_t));
    result->action_id = action_id;
    result->horizon = horizon;

    if (!bridge->world_model) {
        result->feasible = false;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Get current world model state */
    const omni_wm_state_t* current_state = omni_wm_get_state(bridge->world_model);
    if (!current_state) {
        result->feasible = false;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Create hypothetical action from mirror neurons */
    float hypothetical_action[64];
    uint32_t action_dim = 0;

    if (bridge->mirror_system) {
        /* Use mirror neuron activation pattern as action representation */
        mirror_activation_t activation;
        if (mirror_neurons_get_activation_record(bridge->mirror_system,
                                                  action_id, &activation)) {
            hypothetical_action[0] = activation.observation_activation;
            hypothetical_action[1] = activation.execution_activation;
            action_dim = 2;
        }
    }

    if (action_dim == 0) {
        /* Default action */
        hypothetical_action[0] = 0.5f;
        action_dim = 1;
    }

    /* Run counterfactual simulation */
    omni_wm_counterfactual_result_t wm_result;
    memset(&wm_result, 0, sizeof(wm_result));

    nimcp_error_t err = omni_wm_what_if(bridge->world_model,
                                         hypothetical_action,
                                         action_dim,
                                         horizon,
                                         &wm_result);

    if (err == NIMCP_SUCCESS) {
        result->expected_reward = wm_result.expected_reward;
        result->divergence = wm_result.divergence;
        result->goal_achievement = wm_result.confidence;
        result->feasible = (wm_result.divergence < bridge->config.divergence_threshold);

        /* Copy trajectory if available */
        if (wm_result.trajectory_len > 0 && wm_result.trajectory) {
            result->trajectory_length = wm_result.trajectory_len;
            /* Note: trajectory ownership transfers to caller */
        }

        bridge->state.counterfactuals_simulated++;
        bridge->stats.total_counterfactuals++;
        bridge->effects.avg_cf_divergence =
            (bridge->effects.avg_cf_divergence * 0.9f) + (wm_result.divergence * 0.1f);
        bridge->stats.avg_cf_divergence = bridge->effects.avg_cf_divergence;
    } else {
        result->feasible = false;
    }

    /* Clean up world model result */
    omni_wm_cf_result_destroy(&wm_result);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_omni_update_action_expectations(
    mirror_omni_bridge_t* bridge
) {
    if (!bridge || !bridge->mirror_system || !bridge->world_model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_update_action_expectations: bridge, mirror_system, or world_model is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_update_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get world model state predictions */
    const omni_wm_state_t* predicted_state = omni_wm_get_state(bridge->world_model);
    if (!predicted_state) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Update mirror neuron expectations based on predicted states */
    /* This feeds back world model predictions to prime mirror neurons */
    float prediction_strength = 1.0f - predicted_state->uncertainty;

    /* Modulate effects */
    bridge->effects.counterfactual_utilization = prediction_strength;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_omni_validate_motor_sequence(
    mirror_omni_bridge_t* bridge,
    const uint32_t* action_ids,
    uint32_t num_actions,
    bool* feasible,
    float* expected_reward
) {
    if (!bridge || !action_ids || !feasible) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_validate_motor_sequence: bridge, action_ids, or feasible is NULL");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, action_ids, num_actions * sizeof(uint32_t));

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_validate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    *feasible = true;
    float total_reward = 0.0f;

    if (bridge->world_model && bridge->mirror_system) {
        /* Validate each action in sequence */
        for (uint32_t i = 0; i < num_actions && *feasible; i++) {
            mirror_omni_cf_result_t cf_result;
            if (mirror_omni_simulate_counterfactual(bridge, action_ids[i],
                                                     1, &cf_result) == 0) {
                *feasible = cf_result.feasible;
                total_reward += cf_result.expected_reward;
                mirror_omni_cf_result_free(&cf_result);
            } else {
                *feasible = false;
            }
        }
    }

    if (expected_reward) {
        *expected_reward = total_reward;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Omnidirectional Inference Implementation
 * ============================================================================ */

int mirror_omni_omni_inference(
    mirror_omni_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_omnidirectional) return 0;

    /* Run all inference directions */
    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_omni_inf", 0.0f);


    int result = 0;

    if (bridge->state.forward_inference_weight > 0.0f) {
        /* Forward inference for each tracked agent */
        for (uint32_t i = 0; i < bridge->num_agent_states; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_agent_states > 256) {
                mirror_omni_bridge_heartbeat("mirror_omni__loop",
                                 (float)(i + 1) / (float)bridge->num_agent_states);
            }

            if (mirror_omni_forward_inference(bridge,
                    bridge->agent_states[i].agent_id) != 0) {
                result = -1;
            }
        }
    }

    if (bridge->state.backward_inference_weight > 0.0f) {
        /* Backward inference for each tracked agent */
        for (uint32_t i = 0; i < bridge->num_agent_states; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_agent_states > 256) {
                mirror_omni_bridge_heartbeat("mirror_omni__loop",
                                 (float)(i + 1) / (float)bridge->num_agent_states);
            }

            if (mirror_omni_backward_inference(bridge,
                    bridge->agent_states[i].agent_id) != 0) {
                result = -1;
            }
        }
    }

    if (bridge->state.lateral_inference_weight > 0.0f) {
        /* Lateral inference across agents */
        if (mirror_omni_lateral_inference(bridge) != 0) {
            result = -1;
        }
    }

    return result;
}

int mirror_omni_forward_inference(
    mirror_omni_bridge_t* bridge,
    uint32_t agent_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_forward_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->world_model && bridge->mirror_system) {
        /* Get current agent state */
        mirror_omni_agent_state_t agent_state;
        if (mirror_omni_get_agent_state(bridge, agent_id, &agent_state) == 0) {
            /* Use world model forward dynamics to predict next state */
            omni_wm_transition_t transition;
            memset(&transition, 0, sizeof(transition));

            /* Create action from mirror observation */
            float action[2] = {agent_state.confidence, agent_state.goal_probability};

            nimcp_error_t err = omni_wm_predict_forward(bridge->world_model,
                                                         action, 2, &transition);
            if (err == NIMCP_SUCCESS) {
                bridge->stats.forward_inferences++;
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_omni_backward_inference(
    mirror_omni_bridge_t* bridge,
    uint32_t agent_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_backward", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->world_model && bridge->mirror_system) {
        /* Get current agent state */
        mirror_omni_agent_state_t agent_state;
        if (mirror_omni_get_agent_state(bridge, agent_id, &agent_state) == 0) {
            /* Use world model backward dynamics to infer past action */
            omni_wm_state_t current_state;
            memset(&current_state, 0, sizeof(current_state));
            current_state.values = &agent_state.confidence;
            current_state.dim = 1;

            omni_wm_transition_t transition;
            memset(&transition, 0, sizeof(transition));

            nimcp_error_t err = omni_wm_infer_backward(bridge->world_model,
                                                        &current_state, &transition);
            if (err == NIMCP_SUCCESS) {
                bridge->stats.backward_inferences++;
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_omni_lateral_inference(
    mirror_omni_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_lateral_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->world_model && bridge->num_agent_states > 1) {
        /* Cross-agent inference: how does one agent's state affect others */
        for (uint32_t i = 0; i < bridge->num_agent_states; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_agent_states > 256) {
                mirror_omni_bridge_heartbeat("mirror_omni__loop",
                                 (float)(i + 1) / (float)bridge->num_agent_states);
            }

            for (uint32_t j = i + 1; j < bridge->num_agent_states; j++) {
                /* Use lateral dynamics to predict cross-agent effects */
                omni_wm_state_t source_state;
                memset(&source_state, 0, sizeof(source_state));
                source_state.values = &bridge->agent_states[i].confidence;
                source_state.dim = 1;

                omni_wm_state_t target_state;
                memset(&target_state, 0, sizeof(target_state));

                nimcp_error_t err = omni_wm_predict_lateral(bridge->world_model,
                                                             &source_state, j,
                                                             &target_state);
                if (err == NIMCP_SUCCESS) {
                    bridge->stats.lateral_inferences++;
                }
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_omni_set_direction_weights(
    mirror_omni_bridge_t* bridge,
    float forward_weight,
    float backward_weight,
    float lateral_weight
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_set_dire", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Normalize weights */
    float sum = forward_weight + backward_weight + lateral_weight;
    if (sum > 0.0f) {
        bridge->state.forward_inference_weight = forward_weight / sum;
        bridge->state.backward_inference_weight = backward_weight / sum;
        bridge->state.lateral_inference_weight = lateral_weight / sum;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Cycle Implementation
 * ============================================================================ */

int mirror_omni_bridge_update(
    mirror_omni_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Mirror -> World Model */
    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__update", 0.0f);


    if (bridge->state.mirror_connected && bridge->state.world_model_connected) {
        for (uint32_t i = 0; i < bridge->num_agent_states; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_agent_states > 256) {
                mirror_omni_bridge_heartbeat("mirror_omni__loop",
                                 (float)(i + 1) / (float)bridge->num_agent_states);
            }

            mirror_omni_feed_agent_state(bridge, bridge->agent_states[i].agent_id);
        }
        mirror_omni_update_state_transitions(bridge);
    }

    /* Mirror -> Active Inference */
    if (bridge->state.mirror_connected && bridge->state.active_inference_connected) {
        mirror_omni_provide_action_priors(bridge);
        mirror_omni_modulate_precision(bridge);
    }

    /* World Model -> Mirror */
    if (bridge->state.world_model_connected && bridge->state.mirror_connected) {
        mirror_omni_update_action_expectations(bridge);
    }

    /* Omnidirectional inference */
    if (bridge->config.enable_omnidirectional) {
        mirror_omni_omni_inference(bridge);
    }

    /* Update base stats */
    bridge->base.last_update_time_ms += delta_ms;
    bridge->base.total_updates++;

    return 0;
}

/* ============================================================================
 * State/Stats Implementation
 * ============================================================================ */

int mirror_omni_bridge_get_state(
    const mirror_omni_bridge_t* bridge,
    mirror_omni_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_bridge_get_state: bridge or state is NULL");
        return -1;
    }
    *state = bridge->state;
    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__get_state", 0.0f);


    return 0;
}

int mirror_omni_bridge_get_effects(
    const mirror_omni_bridge_t* bridge,
    mirror_omni_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_bridge_get_effects: bridge or effects is NULL");
        return -1;
    }
    *effects = bridge->effects;
    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__get_effects", 0.0f);


    return 0;
}

int mirror_omni_bridge_get_stats(
    const mirror_omni_bridge_t* bridge,
    mirror_omni_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_omni_bridge_get_stats: bridge or stats is NULL");
        return -1;
    }
    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__get_stats", 0.0f);


    return 0;
}

int mirror_omni_bridge_reset_stats(
    mirror_omni_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(mirror_omni_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int mirror_omni_bridge_connect_bio_async(
    mirror_omni_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__connect_bio_async", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_MIRROR_OMNI_BRIDGE,
        .module_name = "mirror_omni_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Mirror-omni bridge connected to bio-async");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }
    return 0;
}

int mirror_omni_bridge_disconnect_bio_async(
    mirror_omni_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Mirror-omni bridge disconnected from bio-async");
    return 0;
}

bool mirror_omni_bridge_is_bio_async_connected(
    const mirror_omni_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__is_bio_async_connect", 0.0f);


    return bridge && bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

const char* mirror_omni_direction_to_string(mirror_omni_direction_t direction) {
    switch (direction) {
        case MIRROR_OMNI_DIR_FORWARD:  return "forward";
        case MIRROR_OMNI_DIR_BACKWARD: return "backward";
        case MIRROR_OMNI_DIR_LATERAL:  return "lateral";
        default:                       return "unknown";
    }
}

const char* mirror_omni_imitation_result_to_string(mirror_omni_imitation_result_t result) {
    switch (result) {
        case MIRROR_OMNI_IMITATE_SUCCESS:    return "success";
        case MIRROR_OMNI_IMITATE_COSTLY:     return "costly";
        case MIRROR_OMNI_IMITATE_INFEASIBLE: return "infeasible";
        case MIRROR_OMNI_IMITATE_UNKNOWN:    return "unknown";
        default:                             return "invalid";
    }
}

void mirror_omni_cf_result_free(mirror_omni_cf_result_t* result) {
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_cf_resul", 0.0f);


    if (result->predicted_trajectory) {
        nimcp_free(result->predicted_trajectory);
        result->predicted_trajectory = NULL;
    }
    result->trajectory_length = 0;
}

void mirror_omni_agent_state_free(mirror_omni_agent_state_t* state) {
    if (!state) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_omni_bridge_heartbeat("mirror_omni__mirror_omni_agent_st", 0.0f);


    if (state->predicted_state) {
        nimcp_free(state->predicted_state);
        state->predicted_state = NULL;
    }
    state->state_dim = 0;
}

//=============================================================================
// Instance Health Agent Setter (B22 Upgrade)
//=============================================================================

void mirror_omni_bridge_set_instance_health_agent(
    mirror_omni_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B22 Upgrade)
//=============================================================================

int mirror_omni_bridge_training_begin(mirror_omni_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_omni_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_omni_bridge_heartbeat_instance(bridge->health_agent, "mirror_omni_bridge_training_begin", 0.0f);
    return 0;
}

int mirror_omni_bridge_training_end(mirror_omni_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_omni_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_omni_bridge_heartbeat_instance(bridge->health_agent, "mirror_omni_bridge_training_end", 1.0f);
    return 0;
}

int mirror_omni_bridge_training_step(mirror_omni_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_omni_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "mirror_omni_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "mirror_omni_bridge_training_step");
    mirror_omni_bridge_heartbeat_instance(bridge->health_agent, "mirror_omni_bridge_training_step", progress);

    /* Notify coordinator of step cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}
