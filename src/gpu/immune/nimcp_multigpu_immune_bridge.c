/**
 * @file nimcp_multigpu_immune_bridge.c
 * @brief Multi-GPU Coordination-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "gpu/immune/nimcp_multigpu_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(multigpu_immune_bridge)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 *
 * WHAT: Get system time for duration tracking
 * WHY:  Track multi-GPU coordination timing
 * HOW:  Platform-specific time retrieval
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Get inflammation GPU count recommendation
 */
static uint32_t get_inflammation_gpu_count(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_NONE_GPU_COUNT;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_LOCAL_GPU_COUNT;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_REGIONAL_GPU_COUNT;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_SYSTEMIC_GPU_COUNT;
        case INFLAMMATION_STORM:    return INFLAMMATION_STORM_GPU_COUNT;
        default:                    return 0;  /* Use all */
    }
}

/**
 * @brief Get inflammation partition strategy
 */
static multigpu_partition_strategy_t get_inflammation_partition(
    brain_inflammation_level_t level
) {
    if (level == INFLAMMATION_NONE || level == INFLAMMATION_LOCAL) {
        return INFLAMMATION_PARTITION_NONE_LOCAL;
    } else if (level == INFLAMMATION_REGIONAL) {
        return INFLAMMATION_PARTITION_REGIONAL;
    } else {
        return INFLAMMATION_PARTITION_SYSTEMIC;  /* SYSTEMIC/STORM */
    }
}

/**
 * @brief Get inflammation rebalance frequency factor
 */
static float get_inflammation_rebalance_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_REBALANCE_NONE_FACTOR;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_REBALANCE_LOCAL_FACTOR;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_REBALANCE_REGIONAL_FACTOR;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_REBALANCE_SYSTEMIC_FACTOR;
        case INFLAMMATION_STORM:    return INFLAMMATION_REBALANCE_STORM_FACTOR;
        default:                    return 1.0f;
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int multigpu_immune_default_config(multigpu_immune_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Enable all features by default */
    config->enable_cytokine_coordination_modulation = true;
    config->enable_multigpu_error_immune_response = true;
    config->enable_gpu_count_modulation = true;
    config->enable_partition_modulation = true;
    config->enable_rebalance_modulation = true;

    /* Default sensitivity */
    config->cytokine_sensitivity = 1.0f;
    config->error_sensitivity = 1.0f;
    config->imbalance_sensitivity = 1.0f;

    /* Baseline configuration */
    config->baseline_gpu_count = 0;  /* Use all */
    config->baseline_partition = MULTIGPU_PARTITION_DYNAMIC;
    config->baseline_rebalance_interval = 100;

    /* Thresholds */
    config->imbalance_threshold = MULTIGPU_IMBALANCE_NORMAL_THRESHOLD;
    config->rebalance_churn_threshold = 10;
    config->error_window_ms = 5000;

    return NIMCP_SUCCESS;
}

multigpu_immune_bridge_t* multigpu_immune_create(
    const multigpu_immune_config_t* config,
    brain_immune_system_t* immune_system,
    multigpu_context_t multigpu_context
) {
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("multigpu_immune_create: immune_system required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;
    }

    multigpu_immune_bridge_t* bridge = nimcp_malloc(sizeof(multigpu_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("multigpu_immune_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(multigpu_immune_bridge_t));

    multigpu_immune_config_t default_config;
    if (!config) {
        multigpu_immune_default_config(&default_config);
        config = &default_config;
    }

    bridge->enable_cytokine_coordination_modulation = config->enable_cytokine_coordination_modulation;
    bridge->enable_multigpu_error_immune_response = config->enable_multigpu_error_immune_response;
    bridge->enable_gpu_count_modulation = config->enable_gpu_count_modulation;
    bridge->enable_partition_modulation = config->enable_partition_modulation;
    bridge->enable_rebalance_modulation = config->enable_rebalance_modulation;

    bridge->immune_system = immune_system;
    bridge->multigpu_context = multigpu_context;

    bridge->baseline_gpu_count = config->baseline_gpu_count;
    bridge->baseline_partition = config->baseline_partition;
    bridge->baseline_rebalance_interval = config->baseline_rebalance_interval;

    bridge->last_update_time = get_time_ms();
    bridge->error_window_start = bridge->last_update_time;

    if (bridge_base_init(&bridge->base, 0, "multigpu_immune") != 0) { nimcp_free(bridge); return NULL; }

    /* Allocate per-GPU utilization array if multigpu context available */
    if (multigpu_context) {
        uint32_t gpu_count = multigpu_get_device_count(multigpu_context);
        if (gpu_count > 0) {
            bridge->error_state.per_gpu_utilization = nimcp_malloc(gpu_count * sizeof(float));
            if (bridge->error_state.per_gpu_utilization) {
                memset(bridge->error_state.per_gpu_utilization, 0, gpu_count * sizeof(float));
            }
            bridge->error_state.total_gpu_count = gpu_count;
            bridge->error_state.active_gpu_count = gpu_count;
        }
    }

    NIMCP_LOGGING_INFO("multigpu_immune_bridge: created successfully");
    return bridge;
}

void multigpu_immune_destroy(multigpu_immune_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        multigpu_immune_disconnect_bio_async(bridge);
    }

    if (bridge->error_state.per_gpu_utilization) {
        nimcp_free(bridge->error_state.per_gpu_utilization);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Immune → Multi-GPU API
 * ============================================================================ */

int multigpu_immune_apply_cytokine_effects(multigpu_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->immune_system, NIMCP_ERROR_NULL_POINTER, "bridge->immune_system is NULL");

    if (!bridge->enable_cytokine_coordination_modulation) {
        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    brain_immune_stats_t stats;
    brain_immune_get_stats(bridge->immune_system, &stats);

    memset(&bridge->cytokine_effects, 0, sizeof(multigpu_cytokine_effects_t));

    /* Use inflammation level from immune stats */
    brain_inflammation_level_t level = stats.inflammation_level;

    /* Set recommendations based on inflammation */
    bridge->cytokine_effects.recommended_gpu_count = get_inflammation_gpu_count(level);
    bridge->cytokine_effects.recommended_partition = get_inflammation_partition(level);
    bridge->cytokine_effects.rebalance_frequency_factor = get_inflammation_rebalance_factor(level);

    /* Increase imbalance tolerance during inflammation */
    if (level == INFLAMMATION_STORM) {
        bridge->cytokine_effects.imbalance_tolerance = MULTIGPU_IMBALANCE_STORM_THRESHOLD;
    } else if (level >= INFLAMMATION_REGIONAL) {
        bridge->cytokine_effects.imbalance_tolerance = MULTIGPU_IMBALANCE_INFLAMED_THRESHOLD;
    } else {
        bridge->cytokine_effects.imbalance_tolerance = MULTIGPU_IMBALANCE_NORMAL_THRESHOLD;
    }

    /* Disable work stealing during high inflammation */
    bridge->cytokine_effects.disable_work_stealing = (level >= INFLAMMATION_SYSTEMIC);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

uint32_t multigpu_immune_get_recommended_gpu_count(
    const multigpu_immune_bridge_t* bridge
) {
    if (!bridge) {
        return 0;  /* Use all */
    }

    return bridge->cytokine_effects.recommended_gpu_count;
}

multigpu_partition_strategy_t multigpu_immune_get_recommended_partition(
    const multigpu_immune_bridge_t* bridge
) {
    if (!bridge) {
        return MULTIGPU_PARTITION_DYNAMIC;
    }

    return bridge->cytokine_effects.recommended_partition;
}

float multigpu_immune_get_rebalance_frequency_factor(
    const multigpu_immune_bridge_t* bridge
) {
    if (!bridge) {
        return 1.0f;
    }

    return bridge->cytokine_effects.rebalance_frequency_factor;
}

/* ============================================================================
 * Multi-GPU → Immune API
 * ============================================================================ */

int multigpu_immune_trigger_error_response(
    multigpu_immune_bridge_t* bridge,
    int error_code,
    const char* error_message
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->immune_system, NIMCP_ERROR_NULL_POINTER, "bridge->immune_system is NULL");

    if (!bridge->enable_multigpu_error_immune_response) {
        return NIMCP_SUCCESS;
    }

    uint8_t severity = MULTIGPU_ERROR_SEVERITY_P2P_FAILURE;

    const char* msg = error_message ? error_message : "Multi-GPU error";
    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        (const uint8_t*)msg,
        strlen(msg),
        severity,
        0,
        &antigen_id
    );

    if (result == 0) {
        bridge->immune_triggers++;
        NIMCP_LOGGING_WARN("multigpu_immune: triggered immune response for error %d", error_code);
    }

    return result;
}

int multigpu_immune_monitor_load_balance(multigpu_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->multigpu_context, NIMCP_ERROR_NULL_POINTER, "bridge->multigpu_context is NULL");

    /* Get multi-GPU performance stats */
    uint64_t total_ops;
    double total_time_ms;
    float avg_utilization;
    float load_imbalance;

    if (!multigpu_get_performance_stats(bridge->multigpu_context, &total_ops,
                                       &total_time_ms, &avg_utilization, &load_imbalance)) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    bridge->error_state.current_load_imbalance = load_imbalance;
    bridge->error_state.avg_gpu_utilization = avg_utilization;

    /* Check if imbalance exceeds threshold */
    float threshold = bridge->cytokine_effects.imbalance_tolerance;
    if (threshold == 0.0f) {
        threshold = MULTIGPU_IMBALANCE_NORMAL_THRESHOLD;
    }

    if (load_imbalance > threshold) {
        bridge->error_state.load_imbalance_detected = true;

        /* Trigger immune response for chronic imbalance */
        const char* msg = "Multi-GPU load imbalance";
        uint32_t antigen_id;
        brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_MANUAL,
            (const uint8_t*)msg,
            strlen(msg),
            MULTIGPU_ERROR_SEVERITY_LOAD_IMBALANCE,
            0,
            &antigen_id
        );

        bridge->immune_triggers++;
    }

    return NIMCP_SUCCESS;
}

int multigpu_immune_update_error_state(multigpu_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->multigpu_context, NIMCP_ERROR_NULL_POINTER, "bridge->multigpu_context is NULL");

    bridge->error_state.active_gpu_count = multigpu_get_device_count(bridge->multigpu_context);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int multigpu_immune_update(multigpu_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    multigpu_immune_update_error_state(bridge);
    multigpu_immune_apply_cytokine_effects(bridge);
    multigpu_immune_monitor_load_balance(bridge);

    bridge->total_updates++;
    bridge->last_update_time = get_time_ms();

    return NIMCP_SUCCESS;
}

int multigpu_immune_apply_modulation(multigpu_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->multigpu_context, NIMCP_ERROR_NULL_POINTER, "bridge->multigpu_context is NULL");

    /* Check if GPU count should be reduced */
    uint32_t recommended_count = multigpu_immune_get_recommended_gpu_count(bridge);
    if (recommended_count > 0 && bridge->enable_gpu_count_modulation) {
        uint32_t current_count = bridge->error_state.active_gpu_count;
        if (recommended_count != current_count) {
            bridge->gpu_count_changes++;
            NIMCP_LOGGING_INFO("multigpu_immune: GPU count changed to %u (was %u)",
                              recommended_count, current_count);
            /* Would apply GPU count change here via multigpu API */
        }
    }

    /* Check if partition strategy should change */
    if (bridge->enable_partition_modulation) {
        multigpu_partition_strategy_t recommended =
            multigpu_immune_get_recommended_partition(bridge);
        if (recommended != bridge->baseline_partition) {
            bridge->partition_changes++;
            NIMCP_LOGGING_INFO("multigpu_immune: partition strategy changed to %d", recommended);
            /* Would apply partition change here via multigpu API */
        }
    }

    /* Check if rebalancing should be suppressed */
    if (bridge->enable_rebalance_modulation) {
        float factor = multigpu_immune_get_rebalance_frequency_factor(bridge);
        if (factor < 1.0f) {
            bridge->rebalance_suppressions++;
            NIMCP_LOGGING_DEBUG("multigpu_immune: rebalancing suppressed (factor %.2f)", factor);
        }
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int multigpu_immune_get_cytokine_effects(
    const multigpu_immune_bridge_t* bridge,
    multigpu_cytokine_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");

    memcpy(effects, &bridge->cytokine_effects, sizeof(multigpu_cytokine_effects_t));
    return NIMCP_SUCCESS;
}

int multigpu_immune_get_error_state(
    const multigpu_immune_bridge_t* bridge,
    multigpu_error_state_t* state
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");

    memcpy(state, &bridge->error_state, sizeof(multigpu_error_state_t));
    return NIMCP_SUCCESS;
}

bool multigpu_immune_is_gpu_count_reduced(const multigpu_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    uint32_t recommended = bridge->cytokine_effects.recommended_gpu_count;
    return (recommended > 0 && recommended < bridge->error_state.total_gpu_count);
}

uint32_t multigpu_immune_get_active_gpu_count(const multigpu_immune_bridge_t* bridge) {
    if (!bridge) {
        return 0;
    }

    return bridge->error_state.active_gpu_count;
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int multigpu_immune_connect_bio_async(multigpu_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_MULTIGPU,
        .module_name = "multigpu_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("multigpu_immune: connected to bio-async router");
        return NIMCP_SUCCESS;
    }

    NIMCP_LOGGING_WARN("multigpu_immune: bio-async router not available");
    return NIMCP_ERROR_OPERATION_FAILED;
}

int multigpu_immune_disconnect_bio_async(multigpu_immune_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("multigpu_immune: disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool multigpu_immune_is_bio_async_connected(const multigpu_immune_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
