/**
 * @file nimcp_distributed_immune_bridge.c
 * @brief Distributed Cognition-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "networking/immune/nimcp_distributed_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(distributed_immune_bridge)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * NIMCP_MS_PER_SEC + (uint64_t)ts.tv_nsec / NIMCP_NS_PER_MS;
}

/**
 * @brief Get inflammation broadcast factor
 */
static float get_inflammation_broadcast_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_NONE_BROADCAST_FACTOR;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_LOCAL_BROADCAST_FACTOR;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_REGIONAL_BROADCAST_FACTOR;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_SYSTEMIC_BROADCAST_FACTOR;
        case INFLAMMATION_STORM:    return INFLAMMATION_STORM_BROADCAST_FACTOR;
        default:                    return 1.0f;
    }
}

/**
 * @brief Map congestion metrics to inflammation level
 */
static brain_inflammation_level_t compute_inflammation_from_congestion(
    const network_congestion_metrics_t* metrics
) {
    /* Guard: null check */
    if (!metrics) {
        return INFLAMMATION_NONE;
    }

    /* Check for partition (most severe) */
    if (metrics->partition_detected) {
        return INFLAMMATION_STORM;
    }

    /* Check packet loss and latency thresholds */
    if (metrics->packet_loss_rate >= CONGESTION_PACKET_LOSS_SYSTEMIC ||
        metrics->avg_latency_ms >= CONGESTION_LATENCY_SYSTEMIC_MS) {
        return INFLAMMATION_SYSTEMIC;
    }

    if (metrics->packet_loss_rate >= CONGESTION_PACKET_LOSS_REGIONAL ||
        metrics->avg_latency_ms >= CONGESTION_LATENCY_REGIONAL_MS) {
        return INFLAMMATION_REGIONAL;
    }

    if (metrics->packet_loss_rate >= CONGESTION_PACKET_LOSS_LOCAL ||
        metrics->avg_latency_ms >= CONGESTION_LATENCY_LOCAL_MS) {
        return INFLAMMATION_LOCAL;
    }

    return INFLAMMATION_NONE;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int distributed_immune_default_config(distributed_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Enable all features by default */
    config->enable_congestion_inflammation = true;
    config->enable_peer_failure_immune_response = true;
    config->enable_cytokine_network_modulation = true;
    config->enable_recovery_memory = true;

    /* Default sensitivity */
    config->congestion_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->immune_network_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Default thresholds */
    config->packet_loss_threshold = CONGESTION_PACKET_LOSS_LOCAL;
    config->latency_threshold_ms = CONGESTION_LATENCY_LOCAL_MS;
    config->peer_failure_threshold = 3;

    return 0;
}

distributed_immune_bridge_t* distributed_immune_bridge_create(
    const distributed_immune_config_t* config,
    brain_immune_system_t* immune_system,
    distrib_cognition_t distributed_cognition
) {
    /* Guard: require systems */
    if (!immune_system || !distributed_cognition) {
        NIMCP_LOGGING_ERROR("distributed_immune_bridge_create: immune_system and distributed_cognition required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "distributed_immune_bridge_create: required parameter is NULL (immune_system, distributed_cognition)");
        return NULL;
    }

    /* Allocate bridge */
    distributed_immune_bridge_t* bridge = nimcp_malloc(sizeof(distributed_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("distributed_immune_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "distributed_immune_bridge_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(distributed_immune_bridge_t));

    /* Apply configuration */
    distributed_immune_config_t default_config;
    if (!config) {
        distributed_immune_default_config(&default_config);
        config = &default_config;
    }

    bridge->enable_congestion_inflammation = config->enable_congestion_inflammation;
    bridge->enable_peer_failure_immune_response = config->enable_peer_failure_immune_response;
    bridge->enable_cytokine_network_modulation = config->enable_cytokine_network_modulation;
    bridge->enable_recovery_memory = config->enable_recovery_memory;

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->distributed_cognition = distributed_cognition;

    /* Initialize timing */
    bridge->last_update_time = get_time_ms();
    bridge->last_congestion_check_time = get_time_ms();

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "distributed_immune") != 0) { nimcp_free(bridge); return NULL; }

    NIMCP_LOGGING_INFO("distributed_immune_bridge: created successfully");
    return bridge;
}

void distributed_immune_bridge_destroy(distributed_immune_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        distributed_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Immune → Network API
 * ============================================================================ */

int distributed_immune_apply_cytokine_effects(distributed_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distributed_immune_apply_cytokine_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_cytokine_network_modulation) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get immune stats (simplified - would query actual cytokine levels) */
    brain_immune_stats_t stats;
    brain_immune_get_stats(bridge->immune_system, &stats);

    /* Use actual cytokine levels from immune stats */
    bridge->cytokine_effects.il1_level = stats.cytokine_il1;
    bridge->cytokine_effects.il6_level = stats.cytokine_il6;
    bridge->cytokine_effects.tnf_level = stats.cytokine_tnf;
    bridge->cytokine_effects.ifn_gamma_level = stats.cytokine_ifn_gamma;
    bridge->cytokine_effects.il10_level = stats.cytokine_il10;

    /* Compute network effects */
    bridge->cytokine_effects.heartbeat_rate_multiplier =
        1.0f + (bridge->cytokine_effects.il1_level * (CYTOKINE_IL1_HEARTBEAT_MULTIPLIER - 1.0f));

    bridge->cytokine_effects.coordinator_escalation =
        bridge->cytokine_effects.il6_level > CYTOKINE_IL6_COORDINATOR_THRESHOLD;

    bridge->cytokine_effects.health_check_rate_multiplier =
        1.0f + (bridge->cytokine_effects.tnf_level * (CYTOKINE_TNF_HEALTH_CHECK_RATE - 1.0f));

    bridge->cytokine_effects.validation_strictness =
        CYTOKINE_IFN_VALIDATION_STRICTNESS * bridge->cytokine_effects.ifn_gamma_level;

    bridge->cytokine_effects.recovery_rate_boost =
        bridge->cytokine_effects.il10_level * CYTOKINE_IL10_RECOVERY_BOOST;

    bridge->network_modulations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int distributed_immune_apply_inflammation_effects(distributed_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distributed_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get immune stats */
    brain_immune_stats_t stats;
    brain_immune_get_stats(bridge->immune_system, &stats);

    /* Use inflammation level from immune stats */
    brain_inflammation_level_t level = stats.inflammation_level;
    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.broadcast_rate_factor = get_inflammation_broadcast_factor(level);
    bridge->inflammation_state.peer_filtering_enabled = (level >= INFLAMMATION_REGIONAL);
    bridge->inflammation_state.emergency_mode = (level >= INFLAMMATION_SYSTEMIC);
    bridge->inflammation_state.partition_mode = (level == INFLAMMATION_STORM);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float distributed_immune_compute_broadcast_rate(const distributed_immune_bridge_t* bridge) {
    if (!bridge) {
        return 1.0f;
    }
    return bridge->inflammation_state.broadcast_rate_factor;
}

/* ============================================================================
 * Network → Immune API
 * ============================================================================ */

int distributed_immune_update_congestion_metrics(distributed_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge || !bridge->distributed_cognition) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distributed_immune_update_congestion_metrics: required parameter is NULL (bridge, bridge->distributed_cognition)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get distributed cognition stats */
    distrib_cognition_stats_t dc_stats;
    if (!distrib_cognition_get_stats(bridge->distributed_cognition, &dc_stats)) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "distributed_immune_update_congestion_metrics: distrib_cognition_get_stats is NULL");
        return -1;
    }

    /* Compute congestion metrics */
    uint32_t total_messages = dc_stats.messages_sent + dc_stats.messages_received;
    bridge->network_modulation.congestion.packet_loss_rate =
        total_messages > 0 ? (float)dc_stats.messages_dropped / total_messages : 0.0f;

    bridge->network_modulation.congestion.avg_latency_ms = dc_stats.avg_neuromod_latency_ms;
    bridge->network_modulation.congestion.unhealthy_peers = 0;  /* Would query P2P health */
    bridge->network_modulation.congestion.partition_detected = false;  /* Would detect partition */

    bridge->last_congestion_check_time = get_time_ms();

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int distributed_immune_trigger_congestion_inflammation(distributed_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distributed_immune_trigger_congestion_inflammation: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_congestion_inflammation) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute inflammation level from congestion */
    brain_inflammation_level_t level =
        compute_inflammation_from_congestion(&bridge->network_modulation.congestion);

    /* If inflammation warranted, trigger it */
    if (level > INFLAMMATION_NONE) {
        /* Would call brain_immune_initiate_inflammation here */
        bridge->network_modulation.congestion_triggered_inflammation = true;
        bridge->congestion_inflammation_events++;
    } else {
        bridge->network_modulation.congestion_triggered_inflammation = false;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int distributed_immune_present_peer_failure(
    distributed_immune_bridge_t* bridge,
    uint32_t peer_id,
    uint8_t failure_type
) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distributed_immune_present_peer_failure: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_peer_failure_immune_response) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Create epitope from peer failure */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, sizeof(epitope));
    memcpy(epitope, &peer_id, sizeof(peer_id));
    epitope[sizeof(peer_id)] = failure_type;

    /* Present as antigen */
    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope,
        sizeof(peer_id) + 1,
        PEER_UNHEALTHY_SEVERITY,
        peer_id,
        &antigen_id
    );

    if (result == 0) {
        bridge->network_modulation.peer_failure_antigen_id = antigen_id;
        bridge->peer_failure_antigens++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

int distributed_immune_release_il10_from_recovery(distributed_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distributed_immune_release_il10_from_recovery: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Check if congestion has normalized */
    if (bridge->network_modulation.congestion.packet_loss_rate < CONGESTION_PACKET_LOSS_LOCAL &&
        bridge->network_modulation.congestion.avg_latency_ms < CONGESTION_LATENCY_LOCAL_MS) {

        /* Release IL-10 */
        uint32_t cytokine_id;
        int result = brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL10,
            0,
            CYTOKINE_IL10_RECOVERY_BOOST,
            0,
            &cytokine_id
        );

        if (result == 0) {
            bridge->network_modulation.recovery_triggered_il10 = true;
            bridge->recovery_il10_releases++;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int distributed_immune_bridge_update(
    distributed_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update timing */
    bridge->last_update_time = get_time_ms();
    bridge->total_updates++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* NETWORK → IMMUNE pathways */
    distributed_immune_update_congestion_metrics(bridge);
    distributed_immune_trigger_congestion_inflammation(bridge);
    distributed_immune_release_il10_from_recovery(bridge);

    /* IMMUNE → NETWORK pathways */
    distributed_immune_apply_cytokine_effects(bridge);
    distributed_immune_apply_inflammation_effects(bridge);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int distributed_immune_get_cytokine_effects(
    const distributed_immune_bridge_t* bridge,
    cytokine_network_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distributed_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_network_effects_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int distributed_immune_get_inflammation_state(
    const distributed_immune_bridge_t* bridge,
    inflammation_network_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distributed_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_network_state_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool distributed_immune_is_congested(const distributed_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    return bridge->network_modulation.congestion.packet_loss_rate >= CONGESTION_PACKET_LOSS_LOCAL ||
           bridge->network_modulation.congestion.avg_latency_ms >= CONGESTION_LATENCY_LOCAL_MS;
}

float distributed_immune_get_broadcast_rate_factor(const distributed_immune_bridge_t* bridge) {
    return distributed_immune_compute_broadcast_rate(bridge);
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

#define DISTRIBUTED_IMMUNE_MODULE_NAME "distributed_immune_bridge"

int distributed_immune_connect_bio_async(distributed_immune_bridge_t* bridge) {
    /* Guard: null check */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("distributed_immune_connect_bio_async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distributed_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Guard: already connected */
    if (bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_WARN("distributed_immune_bridge: Already connected to bio-async");
        return 0;
    }

    /* Build module info */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_NETWORKING_DISTRIBUTED,
        .module_name = DISTRIBUTED_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Register with router */
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("distributed_immune_bridge: Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("distributed_immune_bridge: Bio-async router not available, skipping registration");
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int distributed_immune_disconnect_bio_async(distributed_immune_bridge_t* bridge) {
    /* Guard: null check */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Guard: not connected */
    if (!bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Unregister */
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("distributed_immune_bridge: Disconnected from bio-async router");

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool distributed_immune_is_bio_async_connected(const distributed_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
