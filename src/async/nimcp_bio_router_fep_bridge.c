/**
 * @file nimcp_bio_router_fep_bridge.c
 * @brief Implementation of FEP bridge for bio-router
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#include "async/nimcp_bio_router_fep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/validation/nimcp_common.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(bio_router_fep_bridge)

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int bio_router_fep_default_config(bio_router_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_router_fep_default_config: NULL config");
        return -1;
    }

    config->route_prediction_confidence = 0.7f;
    config->latency_tolerance_ms = 10.0f;
    config->surprise_threshold = 2.0f;
    config->learning_rate = NIMCP_LEARNING_RATE_COARSE;
    config->enable_route_learning = true;
    config->history_window = 100;
    config->enable_route_optimization = true;
    config->exploration_rate = NIMCP_EXPLORATION_RATE_DEFAULT;
    config->max_routes_evaluated = 8;
    config->enable_latency_prediction = true;
    config->enable_congestion_avoidance = true;

    return 0;
}

bio_router_fep_bridge_t* bio_router_fep_create(
    const bio_router_fep_config_t* config,
    fep_system_t* fep_system,
    bio_router_t router
) {
    if (!config || !fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_router_fep_create: NULL config or fep_system");
        NIMCP_LOGGING_ERROR("bio_router_fep_create: NULL config or fep_system");
        return NULL;
    }

    bio_router_fep_bridge_t* bridge = (bio_router_fep_bridge_t*)nimcp_malloc(
        sizeof(bio_router_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bio_router_fep_create: Failed to allocate bridge");
        NIMCP_LOGGING_ERROR("bio_router_fep_create: Failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(bio_router_fep_bridge_t));

    /* Copy configuration */
    memcpy(&bridge->config, config, sizeof(bio_router_fep_config_t));

    /* Connect modules */
    bridge->fep_system = fep_system;
    bridge->router = router;

    /* Initialize state */
    bridge->state.fep_active = true;
    bridge->state.router_connected = (router != NULL);

    /* Initialize effects */
    bridge->fep_effects.route_confidence = config->route_prediction_confidence;
    bridge->fep_effects.exploration_factor = config->exploration_rate;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "bio_router_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_MUTEX_INIT, "bio_router_fep_create: Failed to create mutex");
        NIMCP_LOGGING_ERROR("bio_router_fep_create: Failed to create mutex");
        nimcp_free(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created bio-router FEP bridge");

    return bridge;
}

void bio_router_fep_destroy(bio_router_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect from bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        bio_router_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Integration Implementation
 * ============================================================================ */

int bio_router_fep_update_effects(bio_router_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_router_fep_update_effects: NULL bridge or fep_system");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current free energy */
    float free_energy = fep_get_free_energy(bridge->fep_system);
    bridge->stats.avg_free_energy =
        NIMCP_EMA_WEIGHT_SLOW * bridge->stats.avg_free_energy + NIMCP_EMA_WEIGHT_FAST * free_energy;

    /* Low free energy = high confidence in routing */
    bridge->fep_effects.route_confidence = expf(-free_energy / 5.0f);

    /* Estimate congestion based on prediction errors */
    float avg_error = bridge->stats.avg_latency_error_ms;
    bridge->fep_effects.congestion_estimate = fminf(avg_error / 100.0f, 1.0f);

    /* Determine if should avoid congestion */
    bridge->fep_effects.avoid_congestion =
        bridge->config.enable_congestion_avoidance &&
        bridge->fep_effects.congestion_estimate > 0.5f;

    /* Adjust exploration based on prediction accuracy */
    if (bridge->router_effects.prediction_accuracy > 0.8f) {
        /* High accuracy → exploit more */
        bridge->fep_effects.exploration_factor =
            bridge->config.exploration_rate * 0.5f;
    } else {
        /* Low accuracy → explore more */
        bridge->fep_effects.exploration_factor =
            bridge->config.exploration_rate * 2.0f;
    }

    /* Priority boost for predicted routes */
    bridge->fep_effects.routing_priority_boost =
        bridge->fep_effects.route_confidence * 0.5f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int bio_router_fep_observe_routing(
    bio_router_fep_bridge_t* bridge,
    bio_module_id_t target,
    float latency_ms,
    bool success
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_router_fep_observe_routing: NULL bridge");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update routing observations */
    bridge->router_effects.actual_latency_ms = latency_ms;
    bridge->router_effects.messages_routed++;
    if (!success) {
        bridge->router_effects.routing_errors++;
    }

    /* Compute latency prediction error */
    float predicted_latency = bridge->fep_effects.predicted_latency_ms;
    bridge->router_effects.latency_prediction_error =
        latency_ms - predicted_latency;

    /* Compute routing surprise */
    float error_magnitude = fabsf(bridge->router_effects.latency_prediction_error);
    bridge->router_effects.routing_surprise = error_magnitude / 10.0f;

    /* Check for high latency event */
    bridge->router_effects.high_latency_event =
        latency_ms > (predicted_latency + bridge->config.latency_tolerance_ms);

    /* Update statistics */
    bridge->stats.total_messages_routed++;
    if (!success) {
        bridge->stats.total_routing_errors++;
    }
    bridge->stats.avg_routing_latency_ms =
        0.95f * bridge->stats.avg_routing_latency_ms + 0.05f * latency_ms;
    bridge->stats.avg_latency_error_ms =
        0.95f * bridge->stats.avg_latency_error_ms + 0.05f * error_magnitude;
    bridge->stats.avg_routing_surprise =
        0.95f * bridge->stats.avg_routing_surprise +
        0.05f * bridge->router_effects.routing_surprise;

    /* Update prediction accuracy */
    if (error_magnitude < bridge->config.latency_tolerance_ms) {
        bridge->router_effects.correct_predictions++;
    }
    if (bridge->state.total_route_predictions > 0) {
        bridge->router_effects.prediction_accuracy =
            (float)bridge->router_effects.correct_predictions /
            (float)bridge->state.total_route_predictions;
    }

    /* Store state */
    bridge->state.last_actual_route = target;
    bridge->state.last_latency_ms = latency_ms;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int bio_router_fep_predict_route(
    bio_router_fep_bridge_t* bridge,
    bio_module_id_t source,
    bio_module_id_t target,
    bio_module_id_t* predicted_route,
    float* confidence
) {
    if (!bridge || !predicted_route || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_router_fep_predict_route: NULL bridge or output parameters");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* For simplicity, predict direct routing */
    /* In a full implementation, would use FEP to evaluate multiple paths */
    *predicted_route = target;
    *confidence = bridge->fep_effects.route_confidence;

    /* Update state */
    bridge->state.active_route_predictions++;
    bridge->state.total_route_predictions++;
    bridge->state.last_predicted_route = target;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int bio_router_fep_predict_latency(
    bio_router_fep_bridge_t* bridge,
    bio_module_id_t target,
    float* predicted_latency_ms,
    float* uncertainty
) {
    if (!bridge || !predicted_latency_ms || !uncertainty) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_router_fep_predict_latency: NULL bridge or output parameters");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Use historical average as prediction */
    *predicted_latency_ms = bridge->stats.avg_routing_latency_ms;
    if (*predicted_latency_ms < 0.1f) {
        *predicted_latency_ms = 1.0f; /* Default estimate */
    }

    /* Uncertainty based on prediction error variance */
    *uncertainty = bridge->stats.avg_latency_error_ms;

    /* Store prediction for later comparison */
    bridge->fep_effects.predicted_latency_ms = *predicted_latency_ms;
    bridge->fep_effects.latency_uncertainty = *uncertainty;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

int bio_router_fep_connect_bio_async(bio_router_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_router_fep_connect_bio_async: NULL bridge");
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0; /* Already connected */
    }

    /* Note: Integration with bio-router is via direct API, not bio-async messaging */
    bridge->base.bio_async_enabled = true;

    NIMCP_LOGGING_INFO("Connected bio-router FEP bridge");

    return 0;
}

int bio_router_fep_disconnect_bio_async(bio_router_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_router_fep_disconnect_bio_async: NULL bridge");
        return -1;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected bio-router FEP bridge");

    return 0;
}

bool bio_router_fep_is_bio_async_connected(const bio_router_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int bio_router_fep_get_effects(
    const bio_router_fep_bridge_t* bridge,
    bio_router_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_router_fep_get_effects: NULL bridge or effects");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->fep_effects, sizeof(bio_router_fep_effects_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int bio_router_fep_get_router_effects(
    const bio_router_fep_bridge_t* bridge,
    fep_bio_router_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_router_fep_get_router_effects: NULL bridge or effects");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->router_effects, sizeof(fep_bio_router_effects_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int bio_router_fep_get_stats(
    const bio_router_fep_bridge_t* bridge,
    bio_router_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_router_fep_get_stats: NULL bridge or stats");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(bio_router_fep_stats_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int bio_router_fep_reset_stats(bio_router_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_router_fep_reset_stats: NULL bridge");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bio_router_fep_stats_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from the knowledge graph
 *
 * WHAT: Retrieves structural self-knowledge about the Bio_Router_FEP_Bridge module
 * WHY:  Enables runtime introspection and self-awareness capabilities
 * HOW:  Queries KG for Bio_Router_FEP_Bridge entity and logs observations/relations
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge was found, 0 otherwise
 */
int bio_router_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Bio_Router_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("Bio_Router_FEP_Bridge self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Bio_Router_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Bio_Router_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
