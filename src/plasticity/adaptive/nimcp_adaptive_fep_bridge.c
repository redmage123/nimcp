/**
 * @file nimcp_adaptive_fep_bridge.c
 * @brief Free Energy Principle - Adaptive Plasticity Integration Bridge Implementation
 */

#include "plasticity/adaptive/nimcp_adaptive_fep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE_ADAPTIVE_FEP "ADAPTIVE_FEP_BRIDGE"

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(adaptive_fep_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(adaptive_fep_bridge)


/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int adaptive_fep_bridge_default_config(adaptive_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_bridge_default_config: config is NULL");
        return -1;
    }

    config->pe_min_threshold = ADAPTIVE_FEP_PE_MIN_THRESHOLD;
    config->pe_max_threshold = ADAPTIVE_FEP_PE_MAX_THRESHOLD;
    config->precision_sensitivity = ADAPTIVE_FEP_PRECISION_SENSITIVITY;
    config->sparsity_min = ADAPTIVE_FEP_SPARSITY_MIN;
    config->sparsity_max = ADAPTIVE_FEP_SPARSITY_MAX;

    config->enable_pe_scaling = true;
    config->enable_precision_sparsity = true;
    config->enable_complexity_regularization = true;
    config->enable_sparsity_feedback = true;

    config->pe_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->precision_gain = 1.0f;
    config->complexity_gain = 0.5f;
    config->sparsity_feedback_gain = 1.0f;

    return 0;
}

adaptive_fep_bridge_t* adaptive_fep_bridge_create(const adaptive_fep_config_t* config) {
    adaptive_fep_bridge_t* bridge = (adaptive_fep_bridge_t*)nimcp_malloc(sizeof(adaptive_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "adaptive_fep_bridge_create: bridge allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(adaptive_fep_bridge_t));

    if (config) {
        bridge->config = *config;
    } else {
        adaptive_fep_bridge_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "adaptive_fep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "adaptive_fep_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "adaptive_fep_bridge_create: mutex creation failed");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->effects.effective_sparsity_target = ADAPTIVE_FEP_SPARSITY_BASELINE;
    bridge->effects.effective_adaptation_rate = 0.01f;
    bridge->effects.effective_threshold_scaling = 1.0f;

    NIMCP_LOGGING_INFO("Adaptive-FEP bridge created");
    return bridge;
}

void adaptive_fep_bridge_destroy(adaptive_fep_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        adaptive_fep_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Adaptive-FEP bridge destroyed");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int adaptive_fep_bridge_connect_fep(adaptive_fep_bridge_t* bridge, fep_system_t* fep) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_bridge_connect_fep: bridge is NULL");
        return -1;
    }
    if (!fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_bridge_connect_fep: fep is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected to FEP system");
    return 0;
}

int adaptive_fep_bridge_connect_adaptive(adaptive_fep_bridge_t* bridge,
                                          adaptive_network_t network) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_bridge_connect_adaptive: bridge is NULL");
        return -1;
    }
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_bridge_connect_adaptive: network is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->adaptive_network = network;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected to adaptive network");
    return 0;
}

int adaptive_fep_bridge_disconnect(adaptive_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_bridge_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->adaptive_network = NULL;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected systems");
    return 0;
}

/* ============================================================================
 * FEP → Adaptive Direction
 * ============================================================================ */

float adaptive_fep_apply_pe_scaling(adaptive_fep_bridge_t* bridge, float pe) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_apply_pe_scaling: bridge is NULL");
        return 1.0f;
    }

    if (!bridge->config.enable_pe_scaling) {
        return 1.0f;
    }

    float pe_abs = fabsf(pe);
    if (pe_abs < bridge->config.pe_min_threshold) {
        return ADAPTIVE_FEP_ADAPTATION_RATE_MIN;
    }

    if (pe_abs > bridge->config.pe_max_threshold) {
        pe_abs = bridge->config.pe_max_threshold;
    }

    float normalized_pe = (pe_abs - bridge->config.pe_min_threshold) /
                          (bridge->config.pe_max_threshold - bridge->config.pe_min_threshold);

    float scaling = ADAPTIVE_FEP_ADAPTATION_RATE_MIN +
                    normalized_pe * (ADAPTIVE_FEP_ADAPTATION_RATE_MAX - ADAPTIVE_FEP_ADAPTATION_RATE_MIN);

    return scaling * bridge->config.pe_sensitivity;
}

float adaptive_fep_apply_precision_sparsity(adaptive_fep_bridge_t* bridge, float precision) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_apply_precision_sparsity: bridge is NULL");
        return 1.0f;
    }

    if (!bridge->config.enable_precision_sparsity) {
        return 1.0f;
    }

    float scaling = powf(precision, bridge->config.precision_sensitivity);
    scaling *= bridge->config.precision_gain;

    float sparsity_range = bridge->config.sparsity_max - bridge->config.sparsity_min;
    float sparsity = bridge->config.sparsity_min + scaling * sparsity_range;

    if (sparsity < bridge->config.sparsity_min) {
        sparsity = bridge->config.sparsity_min;
    }
    if (sparsity > bridge->config.sparsity_max) {
        sparsity = bridge->config.sparsity_max;
    }

    return sparsity / ADAPTIVE_FEP_SPARSITY_BASELINE;
}

float adaptive_fep_apply_complexity_regularization(adaptive_fep_bridge_t* bridge,
                                                    float complexity) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_apply_complexity_regularization: bridge is NULL");
        return 1.0f;
    }
    if (!bridge->config.enable_complexity_regularization) {
        return 1.0f;
    }

    float clamped = fminf(fmaxf(complexity, 0.0f), 10.0f);
    return 1.0f + clamped * bridge->config.complexity_gain;
}

float adaptive_fep_get_effective_sparsity(const adaptive_fep_bridge_t* bridge,
                                           float base_sparsity) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_get_effective_sparsity: bridge is NULL");
        return base_sparsity;
    }
    return base_sparsity * bridge->effects.effective_sparsity_target / ADAPTIVE_FEP_SPARSITY_BASELINE;
}

float adaptive_fep_get_effective_adaptation_rate(const adaptive_fep_bridge_t* bridge,
                                                  float base_rate) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_get_effective_adaptation_rate: bridge is NULL");
        return base_rate;
    }
    return base_rate * bridge->effects.effective_adaptation_rate;
}

/* ============================================================================
 * Adaptive → FEP Direction
 * ============================================================================ */

int adaptive_fep_report_sparsity(adaptive_fep_bridge_t* bridge, float sparsity) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_report_sparsity: bridge is NULL");
        return -1;
    }
    if (sparsity < 0.0f || sparsity > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_fep_report_sparsity: sparsity out of range [0,1]");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->feedback.measured_sparsity = sparsity;

    if (bridge->config.enable_sparsity_feedback) {
        bridge->feedback.sparsity_precision_estimate =
            sparsity * bridge->config.sparsity_feedback_gain;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int adaptive_fep_report_threshold_changes(adaptive_fep_bridge_t* bridge,
                                           float threshold_delta) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_report_threshold_changes: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stats.threshold_updates++;
    bridge->stats.total_threshold_delta += fabsf(threshold_delta);
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

float adaptive_fep_compute_sparsity_precision(const adaptive_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_compute_sparsity_precision: bridge is NULL");
        return 1.0f;
    }

    float sparsity = bridge->feedback.measured_sparsity;
    if (sparsity < 0.1f) return 0.1f;
    if (sparsity > 0.95f) return 2.0f;

    return sparsity * bridge->config.sparsity_feedback_gain;
}

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

int adaptive_fep_bridge_update(adaptive_fep_bridge_t* bridge, uint64_t delta_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_bridge_update: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (bridge->fep_system && bridge->adaptive_network) {
        float pe_scaling = adaptive_fep_apply_pe_scaling(bridge, bridge->effects.pe_magnitude);
        float precision_scaling = adaptive_fep_apply_precision_sparsity(bridge,
                                                                         bridge->effects.precision_value);
        float complexity_scaling = adaptive_fep_apply_complexity_regularization(bridge,
                                                                                bridge->effects.complexity_value);

        bridge->effects.pe_adaptation_scaling = pe_scaling;
        bridge->effects.precision_sparsity_scaling = precision_scaling;
        bridge->effects.complexity_threshold_scaling = complexity_scaling;

        bridge->effects.effective_adaptation_rate = pe_scaling;
        bridge->effects.effective_sparsity_target = ADAPTIVE_FEP_SPARSITY_BASELINE * precision_scaling;
        bridge->effects.effective_threshold_scaling = complexity_scaling;

        float sparsity = adaptive_network_get_sparsity(bridge->adaptive_network);
        adaptive_fep_report_sparsity(bridge, sparsity);

        bridge->state.current_sparsity = sparsity;

        bridge->stats.avg_pe_scaling =
            (bridge->stats.avg_pe_scaling * bridge->stats.total_updates + pe_scaling) /
            (bridge->stats.total_updates + 1);

        bridge->stats.avg_precision_scaling =
            (bridge->stats.avg_precision_scaling * bridge->stats.total_updates + precision_scaling) /
            (bridge->stats.total_updates + 1);
    }

    bridge->stats.total_updates++;
    bridge->state.last_update_time = delta_ms;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

int adaptive_fep_bridge_get_state(const adaptive_fep_bridge_t* bridge,
                                   adaptive_fep_state_t* state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_bridge_get_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_bridge_get_state: state is NULL");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

int adaptive_fep_bridge_get_stats(const adaptive_fep_bridge_t* bridge,
                                   adaptive_fep_stats_t* stats) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_bridge_get_stats: stats is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int adaptive_fep_bridge_connect_bio_async(adaptive_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_ADAPTIVE_BRIDGE,
        .module_name = "adaptive_fep_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    }

    return 0;
}

int adaptive_fep_bridge_disconnect_bio_async(adaptive_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_fep_bridge_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "adaptive_fep_bridge_disconnect_bio_async: not connected to bio-async");
        return -1;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    return 0;
}

bool adaptive_fep_bridge_is_bio_async_connected(const adaptive_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
