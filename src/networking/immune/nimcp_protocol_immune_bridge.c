/**
 * @file nimcp_protocol_immune_bridge.c
 * @brief Protocol-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "networking/immune/nimcp_protocol_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(protocol_immune_bridge)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * NIMCP_MS_PER_SEC + (uint64_t)ts.tv_nsec / NIMCP_NS_PER_MS;
}

static float get_inflammation_validation_strictness(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_NONE_VALIDATION_STRICTNESS;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_LOCAL_VALIDATION_STRICTNESS;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_REGIONAL_VALIDATION_STRICTNESS;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_SYSTEMIC_VALIDATION_STRICTNESS;
        case INFLAMMATION_STORM:    return INFLAMMATION_STORM_VALIDATION_STRICTNESS;
        default:                    return 0.5f;
    }
}

static brain_inflammation_level_t compute_inflammation_from_errors(
    const protocol_error_metrics_t* metrics
) {
    if (!metrics) {
        return INFLAMMATION_NONE;
    }

    if (metrics->error_rate >= ERROR_RATE_SYSTEMIC_THRESHOLD) {
        return INFLAMMATION_SYSTEMIC;
    }
    if (metrics->error_rate >= ERROR_RATE_REGIONAL_THRESHOLD) {
        return INFLAMMATION_REGIONAL;
    }
    if (metrics->error_rate >= ERROR_RATE_LOCAL_THRESHOLD) {
        return INFLAMMATION_LOCAL;
    }

    return INFLAMMATION_NONE;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int protocol_immune_default_config(protocol_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    config->enable_error_inflammation = true;
    config->enable_violation_immune_response = true;
    config->enable_cytokine_protocol_modulation = true;
    config->enable_antibody_filters = true;

    config->error_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->immune_protocol_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    config->error_rate_threshold = ERROR_RATE_LOCAL_THRESHOLD;
    config->max_filters = 256;

    return 0;
}

protocol_immune_bridge_t* protocol_immune_bridge_create(
    const protocol_immune_config_t* config,
    brain_immune_system_t* immune_system
) {
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("protocol_immune_bridge_create: immune_system required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "protocol_immune_bridge_create: immune_system is NULL");
        return NULL;
    }

    protocol_immune_bridge_t* bridge = nimcp_malloc(sizeof(protocol_immune_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(protocol_immune_bridge_t),
                          "Failed to allocate protocol-immune bridge");
        NIMCP_LOGGING_ERROR("protocol_immune_bridge_create: allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(protocol_immune_bridge_t));

    protocol_immune_config_t default_config;
    if (!config) {
        protocol_immune_default_config(&default_config);
        config = &default_config;
    }

    bridge->enable_error_inflammation = config->enable_error_inflammation;
    bridge->enable_violation_immune_response = config->enable_violation_immune_response;
    bridge->enable_cytokine_protocol_modulation = config->enable_cytokine_protocol_modulation;
    bridge->enable_antibody_filters = config->enable_antibody_filters;

    bridge->immune_system = immune_system;

    bridge->protocol_modulation.filter_capacity = config->max_filters;
    bridge->protocol_modulation.filters =
        nimcp_malloc(sizeof(antibody_message_filter_t) * config->max_filters);

    bridge->last_update_time = get_time_ms();
    bridge->last_error_check_time = get_time_ms();

    if (bridge_base_init(&bridge->base, 0, "protocol_immune") != 0) { nimcp_free(bridge); return NULL; }

    NIMCP_LOGGING_INFO("protocol_immune_bridge: created successfully");
    return bridge;
}

void protocol_immune_bridge_destroy(protocol_immune_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        protocol_immune_disconnect_bio_async(bridge);
    }

    if (bridge->protocol_modulation.filters) {
        nimcp_free(bridge->protocol_modulation.filters);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Immune → Protocol API
 * ============================================================================ */

int protocol_immune_apply_cytokine_effects(protocol_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_immune_apply_cytokine_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_cytokine_protocol_modulation) {
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

    bridge->cytokine_effects.timeout_multiplier =
        1.0f + (bridge->cytokine_effects.il1_level * (CYTOKINE_IL1_TIMEOUT_MULTIPLIER - 1.0f));

    bridge->cytokine_effects.sequence_tracking_enabled =
        bridge->cytokine_effects.il6_level > 0.5f;

    bridge->cytokine_effects.confidence_threshold =
        CYTOKINE_TNF_CONFIDENCE_THRESHOLD * bridge->cytokine_effects.tnf_level;

    bridge->cytokine_effects.quarantine_mode =
        bridge->cytokine_effects.ifn_gamma_level > 0.7f;

    bridge->cytokine_effects.validation_relaxation =
        bridge->cytokine_effects.il10_level * CYTOKINE_IL10_VALIDATION_RELAXATION;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int protocol_immune_apply_inflammation_effects(protocol_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    brain_immune_stats_t stats;
    brain_immune_get_stats(bridge->immune_system, &stats);

    /* Use inflammation level from immune stats */
    brain_inflammation_level_t level = stats.inflammation_level;
    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.validation_strictness = get_inflammation_validation_strictness(level);
    bridge->inflammation_state.checksum_required = (level >= INFLAMMATION_LOCAL);
    bridge->inflammation_state.strict_filtering = (level >= INFLAMMATION_REGIONAL);
    bridge->inflammation_state.reject_unknown_types = (level >= INFLAMMATION_SYSTEMIC);
    bridge->inflammation_state.authenticated_only = (level == INFLAMMATION_STORM);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int protocol_immune_create_antibody_filter(
    protocol_immune_bridge_t* bridge,
    uint32_t antibody_id
) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_immune_create_antibody_filter: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_antibody_filters) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (bridge->protocol_modulation.filter_count >= bridge->protocol_modulation.filter_capacity) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "protocol_immune_create_antibody_filter: capacity exceeded");
        return -1;
    }

    /* Would query antibody from immune system and extract pattern */
    antibody_message_filter_t* filter =
        &bridge->protocol_modulation.filters[bridge->protocol_modulation.filter_count];

    memset(filter, 0, sizeof(antibody_message_filter_t));
    filter->antibody_id = antibody_id;
    filter->ab_class = ANTIBODY_IGG;
    filter->permanent = true;

    bridge->protocol_modulation.filter_count++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool protocol_immune_message_filtered(
    const protocol_immune_bridge_t* bridge,
    const uint8_t* message,
    size_t message_len,
    uint32_t source_node
) {
    if (!bridge || !message) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_immune_message_filtered: required parameter is NULL (bridge, message)");
        return false;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    for (size_t i = 0; i < bridge->protocol_modulation.filter_count; i++) {
        const antibody_message_filter_t* filter = &bridge->protocol_modulation.filters[i];

        if (filter->source_node_filter != 0 && filter->source_node_filter != source_node) {
            continue;
        }

        /* Simple pattern match */
        if (message_len >= filter->pattern_len &&
            memcmp(message, filter->blocked_pattern, filter->pattern_len) == 0) {
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            return true;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Message not matching any filter is normal -- not an error */
    return false;
}

/* ============================================================================
 * Protocol → Immune API
 * ============================================================================ */

int protocol_immune_update_error_metrics(
    protocol_immune_bridge_t* bridge,
    const protocol_error_metrics_t* metrics
) {
    if (!bridge || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_immune_update_error_metrics: required parameter is NULL (bridge, metrics)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    memcpy(&bridge->protocol_modulation.errors, metrics, sizeof(protocol_error_metrics_t));
    bridge->last_error_check_time = get_time_ms();

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int protocol_immune_trigger_error_inflammation(protocol_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_immune_trigger_error_inflammation: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_error_inflammation) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    brain_inflammation_level_t level =
        compute_inflammation_from_errors(&bridge->protocol_modulation.errors);

    if (level > INFLAMMATION_NONE) {
        bridge->protocol_modulation.error_triggered_inflammation = true;
        bridge->error_inflammation_events++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int protocol_immune_present_violation(
    protocol_immune_bridge_t* bridge,
    uint8_t error_type,
    const uint8_t* message_data,
    size_t data_len,
    uint32_t source_node
) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_immune_present_violation: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_violation_immune_response) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, sizeof(epitope));
    epitope[0] = error_type;

    size_t copy_len = data_len < (sizeof(epitope) - 1) ? data_len : (sizeof(epitope) - 1);
    if (message_data) {
        memcpy(epitope + 1, message_data, copy_len);
    }

    uint32_t severity = PROTOCOL_ERROR_INVALID_MAGIC_SEVERITY;
    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope,
        copy_len + 1,
        severity,
        source_node,
        &antigen_id
    );

    if (result == 0) {
        bridge->protocol_modulation.violation_antigen_id = antigen_id;
        bridge->violation_antigens++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

int protocol_immune_release_il10_from_recovery(protocol_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_immune_release_il10_from_recovery: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (bridge->protocol_modulation.errors.error_rate < ERROR_RATE_LOCAL_THRESHOLD) {
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL10,
            0,
            0.3f,
            0,
            &cytokine_id
        );

        bridge->protocol_modulation.recovery_triggered_il10 = true;
        bridge->recovery_il10_releases++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int protocol_immune_bridge_update(
    protocol_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->last_update_time = get_time_ms();
    bridge->total_updates++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    protocol_immune_trigger_error_inflammation(bridge);
    protocol_immune_release_il10_from_recovery(bridge);
    protocol_immune_apply_cytokine_effects(bridge);
    protocol_immune_apply_inflammation_effects(bridge);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int protocol_immune_get_cytokine_effects(
    const protocol_immune_bridge_t* bridge,
    cytokine_protocol_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_protocol_effects_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int protocol_immune_get_inflammation_state(
    const protocol_immune_bridge_t* bridge,
    inflammation_protocol_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_protocol_state_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool protocol_immune_has_high_error_rate(const protocol_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->protocol_modulation.errors.error_rate >= ERROR_RATE_LOCAL_THRESHOLD;
}

float protocol_immune_get_validation_strictness(const protocol_immune_bridge_t* bridge) {
    if (!bridge) {
        return 0.5f;
    }
    return bridge->inflammation_state.validation_strictness;
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

#define PROTOCOL_IMMUNE_MODULE_NAME "protocol_immune_bridge"

int protocol_immune_connect_bio_async(protocol_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("protocol_immune_connect_bio_async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_WARN("protocol_immune_bridge: Already connected to bio-async");
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_NETWORKING_PROTOCOL,
        .module_name = PROTOCOL_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("protocol_immune_bridge: Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("protocol_immune_bridge: Bio-async router not available, skipping registration");
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int protocol_immune_disconnect_bio_async(protocol_immune_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("protocol_immune_bridge: Disconnected from bio-async router");

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool protocol_immune_is_bio_async_connected(const protocol_immune_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
