/**
 * @file nimcp_rate_limiter_fep_bridge.c
 * @brief Implementation of rate limiter-FEP bridge
 */

#include "security/nimcp_rate_limiter_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(rate_limiter_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int rate_fep_default_config(rate_fep_config_t* config) {
    if (!config) {
        return -1;
    }

    config->violation_fe_threshold = RATE_FEP_VIOLATION_THRESHOLD;
    config->burst_fe_threshold = RATE_FEP_BURST_THRESHOLD;
    config->precision_learning_rate = 0.05f;

    config->enable_adaptive_limits = true;
    config->enable_precision_modulation = true;
    config->min_rate_multiplier = 0.5f;
    config->max_rate_multiplier = 2.0f;

    config->enable_online_learning = true;
    config->learning_rate = 0.01f;
    config->learn_from_violations = true;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

rate_fep_bridge_t* rate_fep_create(
    const rate_fep_config_t* config,
    nimcp_rate_limiter_t limiter,
    fep_system_t* fep_system
) {
    if (!limiter || !fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rate_fep_create: limiter or fep_system is NULL");
        return NULL;
    }

    rate_fep_bridge_t* bridge = (rate_fep_bridge_t*)nimcp_malloc(sizeof(rate_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rate_fep_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(rate_fep_bridge_t));

    if (config) {
        bridge->config = *config;
    } else {
        rate_fep_default_config(&bridge->config);
    }

    bridge->fep_system = fep_system;
    bridge->limiter = limiter;

    if (bridge_base_init(&bridge->base, 0, "rate_limiter_fep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "rate_fep_create: failed to init bridge base");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rate_fep_create: mutex creation failed");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->state.active = true;
    bridge->state.current_precision = RATE_FEP_DEFAULT_PRECISION;
    bridge->state.current_rate_multiplier = 1.0f;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Rate limiter FEP bridge created");
    return bridge;
}

void rate_fep_destroy(rate_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        rate_fep_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Rate limiter FEP bridge destroyed");
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int rate_fep_update(rate_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    if (!bridge->state.active) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Get current FEP state
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    // Compute violation score from free energy
    bridge->fep_effects.violation_score = current_fe / bridge->config.violation_fe_threshold;
    if (bridge->fep_effects.violation_score > 1.0f) {
        bridge->fep_effects.violation_score = 1.0f;
    }

    // Compute burst score from prediction error
    bridge->fep_effects.burst_score = pred_error / bridge->config.burst_fe_threshold;
    if (bridge->fep_effects.burst_score > 1.0f) {
        bridge->fep_effects.burst_score = 1.0f;
    }

    // Precision-based strictness
    bridge->fep_effects.rate_strictness = bridge->state.current_precision;

    // Adaptive rate multiplier based on FEP state
    if (bridge->config.enable_adaptive_limits) {
        if (current_fe > bridge->config.burst_fe_threshold) {
            // High FE → reduce rate (more strict)
            bridge->fep_effects.adaptive_rate_multiplier = bridge->config.min_rate_multiplier;
        } else if (current_fe < RATE_FEP_NORMAL_THRESHOLD) {
            // Low FE → increase rate (less strict)
            bridge->fep_effects.adaptive_rate_multiplier = bridge->config.max_rate_multiplier;
        } else {
            // Normal FE → maintain baseline
            bridge->fep_effects.adaptive_rate_multiplier = 1.0f;
        }
    } else {
        bridge->fep_effects.adaptive_rate_multiplier = 1.0f;
    }

    bridge->state.update_count++;
    bridge->stats.avg_free_energy = current_fe;
    bridge->stats.avg_prediction_error = pred_error;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int rate_fep_check_request(
    rate_fep_bridge_t* bridge,
    const char* client_id,
    bool* allowed
) {
    if (!bridge || !allowed) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Check standard rate limiter first
    bool limiter_allowed = nimcp_rate_limiter_allow(bridge->limiter, client_id);

    // Combine with FEP-based decision
    bool fep_allowed = (bridge->fep_effects.violation_score < 0.8f);

    *allowed = limiter_allowed && fep_allowed;

    bridge->state.request_count++;
    bridge->stats.total_requests_processed++;
    bridge->rate_effects.total_requests++;

    if (*allowed) {
        bridge->rate_effects.avg_request_rate =
            0.9f * bridge->rate_effects.avg_request_rate + 0.1f;
    }

    if (!limiter_allowed || !fep_allowed) {
        bridge->stats.violations_detected++;
        bridge->stats.fep_based_decisions++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int rate_fep_apply_modulation(rate_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    if (!bridge->config.enable_precision_modulation) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Adapt precision based on violation rate
    float violation_rate = (float)bridge->rate_effects.violations /
                          (float)(bridge->state.request_count + 1);

    float target_precision = RATE_FEP_DEFAULT_PRECISION;
    if (violation_rate > 0.2f) {
        // High violation rate → increase precision
        target_precision = RATE_FEP_MAX_PRECISION;
    } else if (violation_rate < 0.05f) {
        // Low violation rate → decrease precision
        target_precision = RATE_FEP_MIN_PRECISION;
    }

    // Smooth adaptation
    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * target_precision;

    // Update rate multiplier
    if (bridge->config.enable_adaptive_limits) {
        bridge->state.current_rate_multiplier = bridge->fep_effects.adaptive_rate_multiplier;
    }

    bridge->stats.precision_adaptations++;
    bridge->stats.current_precision = bridge->state.current_precision;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int rate_fep_report_violation(
    rate_fep_bridge_t* bridge,
    const char* client_id,
    uint32_t violation_count
) {
    if (!bridge) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->rate_effects.violations += violation_count;
    bridge->rate_effects.penalties_applied++;

    // Create high-surprise observation for FEP
    if (bridge->config.learn_from_violations) {
        float violation_features[16] = {0};
        violation_features[0] = (float)violation_count / 10.0f;  // Normalized count
        violation_features[1] = 1.0f;  // Violation indicator

        fep_process_observation(bridge->fep_system, violation_features, 16);
        fep_update_precision(bridge->fep_system);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int rate_fep_get_effects(
    const rate_fep_bridge_t* bridge,
    rate_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

int rate_fep_get_rate_effects(
    const rate_fep_bridge_t* bridge,
    fep_rate_effects_t* effects
) {
    if (!bridge || !effects) {
        return -1;
    }

    *effects = bridge->rate_effects;
    return 0;
}

int rate_fep_get_stats(
    const rate_fep_bridge_t* bridge,
    rate_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

float rate_fep_get_violation_score(const rate_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }

    return bridge->fep_effects.violation_score;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int rate_fep_connect_bio_async(rate_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_RATE_LIMITER_FEP,
        .module_name = "rate_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Rate limiter FEP bridge connected to bio-async");
    }

    return 0;
}

int rate_fep_disconnect_bio_async(rate_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Rate limiter FEP bridge disconnected from bio-async");
    return 0;
}

bool rate_fep_is_bio_async_connected(const rate_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
