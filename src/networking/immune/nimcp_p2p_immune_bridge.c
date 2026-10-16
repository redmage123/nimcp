/**
 * @file nimcp_p2p_immune_bridge.c
 * @brief P2P Node-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "networking/immune/nimcp_p2p_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(p2p_immune_bridge)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * NIMCP_MS_PER_SEC + (uint64_t)ts.tv_nsec / NIMCP_NS_PER_MS;
}

static uint32_t get_inflammation_max_peers(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_NONE_MAX_PEERS;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_LOCAL_MAX_PEERS;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_REGIONAL_MAX_PEERS;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_SYSTEMIC_MAX_PEERS;
        case INFLAMMATION_STORM:    return INFLAMMATION_STORM_MAX_PEERS;
        default:                    return INFLAMMATION_NONE_MAX_PEERS;
    }
}

static brain_inflammation_level_t compute_inflammation_from_unhealthy_peers(uint32_t unhealthy_count) {
    if (unhealthy_count >= UNHEALTHY_PEERS_SYSTEMIC_THRESHOLD) {
        return INFLAMMATION_SYSTEMIC;
    }
    if (unhealthy_count >= UNHEALTHY_PEERS_REGIONAL_THRESHOLD) {
        return INFLAMMATION_REGIONAL;
    }
    if (unhealthy_count >= UNHEALTHY_PEERS_LOCAL_THRESHOLD) {
        return INFLAMMATION_LOCAL;
    }
    return INFLAMMATION_NONE;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int p2p_immune_default_config(p2p_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    config->enable_peer_failure_immune_response = true;
    config->enable_unhealthy_inflammation = true;
    config->enable_cytokine_p2p_modulation = true;
    config->enable_antibody_peer_filters = true;

    config->health_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->immune_p2p_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    config->unhealthy_threshold = UNHEALTHY_PEERS_LOCAL_THRESHOLD;
    config->max_peer_filters = 128;

    return 0;
}

p2p_immune_bridge_t* p2p_immune_bridge_create(
    const p2p_immune_config_t* config,
    brain_immune_system_t* immune_system,
    p2p_node_t p2p_node
) {
    if (!immune_system || !p2p_node) {
        NIMCP_LOGGING_ERROR("p2p_immune_bridge_create: immune_system and p2p_node required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "p2p_immune_bridge_create: required parameter is NULL (immune_system, p2p_node)");
        return NULL;
    }

    p2p_immune_bridge_t* bridge = nimcp_malloc(sizeof(p2p_immune_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(p2p_immune_bridge_t),
                          "Failed to allocate P2P-immune bridge");
        NIMCP_LOGGING_ERROR("p2p_immune_bridge_create: allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(p2p_immune_bridge_t));

    p2p_immune_config_t default_config;
    if (!config) {
        p2p_immune_default_config(&default_config);
        config = &default_config;
    }

    bridge->enable_peer_failure_immune_response = config->enable_peer_failure_immune_response;
    bridge->enable_unhealthy_inflammation = config->enable_unhealthy_inflammation;
    bridge->enable_cytokine_p2p_modulation = config->enable_cytokine_p2p_modulation;
    bridge->enable_antibody_peer_filters = config->enable_antibody_peer_filters;

    bridge->immune_system = immune_system;
    bridge->p2p_node = p2p_node;

    bridge->p2p_modulation.filter_capacity = config->max_peer_filters;
    bridge->p2p_modulation.filters = nimcp_malloc(sizeof(antibody_peer_filter_t) * config->max_peer_filters);

    bridge->last_update_time = get_time_ms();
    bridge->last_health_check_time = get_time_ms();

    if (bridge_base_init(&bridge->base, 0, "p2p_immune") != 0) { nimcp_free(bridge); return NULL; }

    NIMCP_LOGGING_INFO("p2p_immune_bridge: created successfully");
    return bridge;
}

void p2p_immune_bridge_destroy(p2p_immune_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        p2p_immune_disconnect_bio_async(bridge);
    }

    if (bridge->p2p_modulation.filters) {
        nimcp_free(bridge->p2p_modulation.filters);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Immune → P2P API
 * ============================================================================ */

int p2p_immune_apply_cytokine_effects(p2p_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_immune_apply_cytokine_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_cytokine_p2p_modulation) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    brain_immune_stats_t stats;
    brain_immune_get_stats(bridge->immune_system, &stats);

    /* Use actual cytokine levels from immune stats */
    bridge->cytokine_effects.il1_level = stats.cytokine_il1;
    bridge->cytokine_effects.il6_level = stats.cytokine_il6;
    bridge->cytokine_effects.tnf_level = stats.cytokine_tnf;
    bridge->cytokine_effects.ifn_gamma_level = stats.cytokine_ifn_gamma;
    bridge->cytokine_effects.il10_level = stats.cytokine_il10;

    bridge->cytokine_effects.heartbeat_rate_multiplier =
        1.0f + (bridge->cytokine_effects.il1_level * (CYTOKINE_IL1_HEARTBEAT_MULTIPLIER - 1.0f));

    bridge->cytokine_effects.reconnect_priority_multiplier =
        1.0f + (bridge->cytokine_effects.il6_level * (CYTOKINE_IL6_RECONNECT_PRIORITY - 1.0f));

    bridge->cytokine_effects.timeout_reduction_factor =
        1.0f - (bridge->cytokine_effects.tnf_level * (1.0f - CYTOKINE_TNF_TIMEOUT_REDUCTION));

    bridge->cytokine_effects.quarantine_enabled =
        bridge->cytokine_effects.ifn_gamma_level > 0.7f;

    bridge->cytokine_effects.restore_normal_policy =
        bridge->cytokine_effects.il10_level > 0.5f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int p2p_immune_apply_inflammation_effects(p2p_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    brain_immune_stats_t stats;
    brain_immune_get_stats(bridge->immune_system, &stats);

    /* Use inflammation level from immune stats */
    brain_inflammation_level_t level = stats.inflammation_level;
    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.max_peers = get_inflammation_max_peers(level);
    bridge->inflammation_state.prefer_healthy_peers = (level >= INFLAMMATION_LOCAL);
    bridge->inflammation_state.disconnect_low_trust = (level >= INFLAMMATION_SYSTEMIC);
    bridge->inflammation_state.emergency_isolation = (level == INFLAMMATION_STORM);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int p2p_immune_create_antibody_peer_filter(p2p_immune_bridge_t* bridge, uint32_t antibody_id) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_immune_create_antibody_peer_filter: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_antibody_peer_filters) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (bridge->p2p_modulation.filter_count >= bridge->p2p_modulation.filter_capacity) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "p2p_immune_create_antibody_peer_filter: capacity exceeded");
        return -1;
    }

    antibody_peer_filter_t* filter = &bridge->p2p_modulation.filters[bridge->p2p_modulation.filter_count];

    memset(filter, 0, sizeof(antibody_peer_filter_t));
    filter->antibody_id = antibody_id;
    filter->ab_class = ANTIBODY_IGG;
    filter->permanent = true;

    bridge->p2p_modulation.filter_count++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool p2p_immune_peer_filtered(const p2p_immune_bridge_t* bridge, uint32_t peer_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_immune_peer_filtered: bridge is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    for (size_t i = 0; i < bridge->p2p_modulation.filter_count; i++) {
        if (bridge->p2p_modulation.filters[i].blocked_peer_id == peer_id) {
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            return true;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Peer not in filter list is normal -- not an error */
    return false;
}

/* ============================================================================
 * P2P → Immune API
 * ============================================================================ */

int p2p_immune_update_health_metrics(p2p_immune_bridge_t* bridge) {
    if (!bridge || !bridge->p2p_node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_immune_update_health_metrics: required parameter is NULL (bridge, bridge->p2p_node)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Would query P2P node for actual peer health */
    bridge->p2p_modulation.health.total_peers = 0;
    bridge->p2p_modulation.health.healthy_peers = 0;
    bridge->p2p_modulation.health.unhealthy_peers = 0;
    bridge->p2p_modulation.health.overall_health_ratio = 1.0f;

    bridge->last_health_check_time = get_time_ms();

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int p2p_immune_trigger_unhealthy_inflammation(p2p_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_immune_trigger_unhealthy_inflammation: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_unhealthy_inflammation) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    brain_inflammation_level_t level =
        compute_inflammation_from_unhealthy_peers(bridge->p2p_modulation.health.unhealthy_peers);

    if (level > INFLAMMATION_NONE) {
        bridge->p2p_modulation.unhealthy_triggered_inflammation = true;
        bridge->unhealthy_inflammation_events++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int p2p_immune_present_peer_failure(p2p_immune_bridge_t* bridge, uint32_t peer_id, uint8_t failure_type) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_immune_present_peer_failure: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_peer_failure_immune_response) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, sizeof(epitope));
    memcpy(epitope, &peer_id, sizeof(peer_id));
    epitope[sizeof(peer_id)] = failure_type;

    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope,
        sizeof(peer_id) + 1,
        PEER_TIMEOUT_SEVERITY,
        peer_id,
        &antigen_id
    );

    if (result == 0) {
        bridge->p2p_modulation.peer_failure_antigen_id = antigen_id;
        bridge->peer_failure_antigens++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

int p2p_immune_release_il10_from_recovery(p2p_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_immune_release_il10_from_recovery: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (bridge->p2p_modulation.health.overall_health_ratio > 0.9f) {
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL10,
            0,
            0.3f,
            0,
            &cytokine_id
        );

        bridge->p2p_modulation.recovery_triggered_il10 = true;
        bridge->recovery_il10_releases++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int p2p_immune_bridge_update(p2p_immune_bridge_t* bridge, uint64_t delta_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->last_update_time = get_time_ms();
    bridge->total_updates++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    p2p_immune_update_health_metrics(bridge);
    p2p_immune_trigger_unhealthy_inflammation(bridge);
    p2p_immune_release_il10_from_recovery(bridge);
    p2p_immune_apply_cytokine_effects(bridge);
    p2p_immune_apply_inflammation_effects(bridge);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int p2p_immune_get_cytokine_effects(const p2p_immune_bridge_t* bridge, cytokine_p2p_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_p2p_effects_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int p2p_immune_get_inflammation_state(const p2p_immune_bridge_t* bridge, inflammation_p2p_state_t* state) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_p2p_state_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool p2p_immune_has_unhealthy_peers(const p2p_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->p2p_modulation.health.unhealthy_peers >= UNHEALTHY_PEERS_LOCAL_THRESHOLD;
}

uint32_t p2p_immune_get_max_peers(const p2p_immune_bridge_t* bridge) {
    if (!bridge) {
        return INFLAMMATION_NONE_MAX_PEERS;
    }
    return bridge->inflammation_state.max_peers;
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

#define P2P_IMMUNE_MODULE_NAME "p2p_immune_bridge"

int p2p_immune_connect_bio_async(p2p_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("p2p_immune_connect_bio_async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_WARN("p2p_immune_bridge: Already connected to bio-async");
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_NETWORKING_P2P,
        .module_name = P2P_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("p2p_immune_bridge: Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("p2p_immune_bridge: Bio-async router not available, skipping registration");
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int p2p_immune_disconnect_bio_async(p2p_immune_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("p2p_immune_bridge: Disconnected from bio-async router");

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool p2p_immune_is_bio_async_connected(const p2p_immune_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
